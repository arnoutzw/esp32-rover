/**
 * @file resource_guard.c
 * @brief ESP32 Resource Consumption Guards Implementation
 *
 * Based on ESP-IDF documentation:
 * - Heap: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/heap_debug.html
 * - Tasks: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
 * - Memory: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html
 */

#include "resource_guard.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "RESOURCE_GUARD";

// =============================================================================
// Internal State
// =============================================================================

static bool s_initialized = false;
static size_t s_initial_free_heap = 0;

// =============================================================================
// Initialization
// =============================================================================

esp_err_t resource_guard_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    // Record initial heap state for reference
    s_initial_free_heap = esp_get_free_heap_size();
    s_initialized = true;

    ESP_LOGI(TAG, "Resource guard initialized, initial free heap: %zu bytes", s_initial_free_heap);
    return ESP_OK;
}

// =============================================================================
// Heap Checks
// =============================================================================

bool resource_guard_check_heap(resource_check_result_t *result)
{
    resource_check_result_t local_result = {0};
    resource_check_result_t *res = result ? result : &local_result;

    // Get heap statistics
    res->free_heap = esp_get_free_heap_size();
    res->min_free_heap = esp_get_minimum_free_heap_size();
    res->free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    res->largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    // Get total heap (internal + PSRAM if available)
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);
    res->total_heap = heap_info.total_free_bytes + heap_info.total_allocated_bytes;

    // Check thresholds
    res->heap_ok = (res->free_heap >= RESOURCE_GUARD_MIN_FREE_HEAP);
    res->internal_ok = (res->free_internal >= RESOURCE_GUARD_MIN_FREE_INTERNAL);

    // Fragmentation check: largest block should be at least 25% of free heap
    // Severe fragmentation means many small blocks but no large contiguous memory
    res->fragmentation_ok = (res->largest_free_block >= (res->free_heap / 4));

    return res->heap_ok && res->internal_ok;
}

bool resource_guard_can_alloc(size_t size)
{
    size_t free_heap = esp_get_free_heap_size();

    // Check if allocation would leave enough free heap
    if (size >= free_heap) {
        return false;  // Would exhaust heap
    }

    size_t remaining = free_heap - size;
    return (remaining >= RESOURCE_GUARD_MIN_FREE_HEAP);
}

uint8_t resource_guard_get_heap_usage_percent(void)
{
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);

    size_t total = heap_info.total_free_bytes + heap_info.total_allocated_bytes;
    if (total == 0) {
        return 0;
    }

    size_t used = heap_info.total_allocated_bytes;
    return (uint8_t)((used * 100) / total);
}

// =============================================================================
// Task/Stack Checks
// =============================================================================

/**
 * @brief Count tasks by core affinity
 */
static void count_tasks_by_core(uint32_t *total, uint32_t *core0, uint32_t *core1)
{
    *total = 0;
    *core0 = 0;
    *core1 = 0;

    // Get number of tasks
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    *total = task_count;

    // Allocate array for task status
    TaskStatus_t *task_status = malloc(task_count * sizeof(TaskStatus_t));
    if (!task_status) {
        return;
    }

    // Get task info
    UBaseType_t actual_count = uxTaskGetSystemState(task_status, task_count, NULL);

    for (UBaseType_t i = 0; i < actual_count; i++) {
#if configUSE_CORE_AFFINITY
        // ESP-IDF FreeRTOS uses xCoreID for core affinity
        BaseType_t core = task_status[i].xCoreID;
        if (core == 0) {
            (*core0)++;
        } else if (core == 1) {
            (*core1)++;
        }
        // tskNO_AFFINITY tasks can run on either core
#else
        // Single core - all tasks on core 0
        (*core0)++;
#endif
    }

    free(task_status);
}

bool resource_guard_check_stack(void *task_handle, task_stack_result_t *result)
{
    TaskHandle_t handle = (TaskHandle_t)task_handle;
    if (handle == NULL) {
        handle = xTaskGetCurrentTaskHandle();
    }

    task_stack_result_t local_result = {0};
    task_stack_result_t *res = result ? result : &local_result;

    res->task_name = pcTaskGetName(handle);
    res->stack_watermark = uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t);

    // Note: Getting total stack size requires storing it separately or using
    // ESP-IDF specific APIs. For now, we just check watermark.
    res->stack_size = 0;  // Not easily available

    res->stack_ok = (res->stack_watermark >= RESOURCE_GUARD_MIN_STACK_WATERMARK);

    return res->stack_ok;
}

int resource_guard_check_all_stacks(void)
{
    int low_stack_count = 0;

    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_status = malloc(task_count * sizeof(TaskStatus_t));
    if (!task_status) {
        ESP_LOGE(TAG, "Failed to allocate memory for task status");
        return -1;
    }

    UBaseType_t actual_count = uxTaskGetSystemState(task_status, task_count, NULL);

    for (UBaseType_t i = 0; i < actual_count; i++) {
        // Stack high water mark is in words, convert to bytes
        uint32_t watermark = task_status[i].usStackHighWaterMark * sizeof(StackType_t);

        if (watermark < RESOURCE_GUARD_MIN_STACK_WATERMARK) {
            ESP_LOGW(TAG, "Task '%s' low stack watermark: %lu bytes",
                     task_status[i].pcTaskName, (unsigned long)watermark);
            low_stack_count++;
        }
    }

    free(task_status);
    return low_stack_count;
}

// =============================================================================
// Combined Checks
// =============================================================================

esp_err_t resource_guard_check_all(resource_check_result_t *result)
{
    if (!s_initialized) {
        resource_guard_init();
    }

    resource_check_result_t local_result = {0};
    resource_check_result_t *res = result ? result : &local_result;

    // Heap checks
    resource_guard_check_heap(res);

    // Task count checks
    count_tasks_by_core(&res->task_count, &res->tasks_core0, &res->tasks_core1);
    res->task_count_ok = (res->task_count <= RESOURCE_GUARD_MAX_TASKS);

    // Combined result
    res->all_ok = res->heap_ok && res->internal_ok &&
                  res->fragmentation_ok && res->task_count_ok;

    return res->all_ok ? ESP_OK : ESP_ERR_INVALID_STATE;
}

// =============================================================================
// Logging and Debugging
// =============================================================================

void resource_guard_log_status(void)
{
    resource_check_result_t res;
    resource_guard_check_all(&res);

    ESP_LOGI(TAG, "=== Resource Status ===");
    ESP_LOGI(TAG, "Heap: %zu/%zu bytes free (%.1f%% used)",
             res.free_heap, res.total_heap,
             res.total_heap > 0 ? 100.0f * (1.0f - (float)res.free_heap / res.total_heap) : 0);
    ESP_LOGI(TAG, "  Internal DRAM: %zu bytes free", res.free_internal);
    ESP_LOGI(TAG, "  Largest block: %zu bytes", res.largest_free_block);
    ESP_LOGI(TAG, "  Watermark: %zu bytes (min free since boot)", res.min_free_heap);
    ESP_LOGI(TAG, "Tasks: %lu total (Core0: %lu, Core1: %lu)",
             (unsigned long)res.task_count,
             (unsigned long)res.tasks_core0,
             (unsigned long)res.tasks_core1);

    // Log check results
    if (!res.heap_ok) {
        ESP_LOGW(TAG, "WARNING: Free heap (%zu) below threshold (%d)",
                 res.free_heap, RESOURCE_GUARD_MIN_FREE_HEAP);
    }
    if (!res.internal_ok) {
        ESP_LOGW(TAG, "WARNING: Internal RAM (%zu) below threshold (%d)",
                 res.free_internal, RESOURCE_GUARD_MIN_FREE_INTERNAL);
    }
    if (!res.fragmentation_ok) {
        ESP_LOGW(TAG, "WARNING: Heap fragmentation detected (largest block: %zu)",
                 res.largest_free_block);
    }
    if (!res.task_count_ok) {
        ESP_LOGW(TAG, "WARNING: Task count (%lu) exceeds limit (%d)",
                 (unsigned long)res.task_count, RESOURCE_GUARD_MAX_TASKS);
    }

    if (res.all_ok) {
        ESP_LOGI(TAG, "All resource checks PASSED");
    } else {
        ESP_LOGE(TAG, "Some resource checks FAILED");
    }
}

void resource_guard_assert_ok(void)
{
    resource_check_result_t res;
    esp_err_t err = resource_guard_check_all(&res);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Resource assertion failed!");
        resource_guard_log_status();

        // Check individual stacks
        int low_stack_count = resource_guard_check_all_stacks();
        if (low_stack_count > 0) {
            ESP_LOGE(TAG, "%d task(s) with low stack watermark", low_stack_count);
        }

        // In a test environment, this would cause test failure
        // In production, we log but don't crash
    }
}
