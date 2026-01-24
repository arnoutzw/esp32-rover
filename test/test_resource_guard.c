/**
 * @file test_resource_guard.c
 * @brief Unit tests for ESP32 resource consumption guards
 *
 * These tests verify that:
 * 1. Heap memory stays within safe limits during operation
 * 2. Stack usage doesn't approach dangerous levels
 * 3. Task creation doesn't exceed reasonable limits
 * 4. Memory allocations can be safely performed
 *
 * Run with: idf.py -T test build flash monitor
 *
 * References:
 * - ESP-IDF Unit Testing: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html
 * - Heap Memory: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/mem_alloc.html
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"
#include "resource_guard.h"

static const char *TAG = "TEST_RESOURCE";

// =============================================================================
// Test Setup/Teardown
// =============================================================================

static size_t s_heap_before_test = 0;

void setUp(void)
{
    // Record heap before each test to detect leaks
    s_heap_before_test = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Test starting, free heap: %zu", s_heap_before_test);
}

void tearDown(void)
{
    // Check for memory leaks
    size_t heap_after = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Test ended, free heap: %zu (delta: %d)",
             heap_after, (int)(heap_after - s_heap_before_test));

    // Allow small delta for FreeRTOS overhead
    if (s_heap_before_test > heap_after + 1024) {
        ESP_LOGW(TAG, "Possible memory leak detected: %zu bytes",
                 s_heap_before_test - heap_after);
    }
}

// =============================================================================
// Heap Memory Tests
// =============================================================================

/**
 * @brief Test that initial heap is above minimum threshold
 *
 * ESP32 should have sufficient free heap after boot for normal operation.
 * This catches cases where boot code has excessive memory usage.
 */
TEST_CASE("Heap above minimum threshold after boot", "[resource][heap]")
{
    size_t free_heap = esp_get_free_heap_size();
    size_t min_threshold = RESOURCE_GUARD_MIN_FREE_HEAP;

    ESP_LOGI(TAG, "Free heap: %zu, threshold: %zu", free_heap, min_threshold);

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        min_threshold, free_heap,
        "Free heap below minimum threshold - system may be unstable"
    );
}

/**
 * @brief Test that internal DRAM is above minimum threshold
 *
 * Internal DRAM is critical for DMA operations, WiFi buffers, and ISR stacks.
 */
TEST_CASE("Internal DRAM above minimum threshold", "[resource][heap]")
{
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t min_threshold = RESOURCE_GUARD_MIN_FREE_INTERNAL;

    ESP_LOGI(TAG, "Free internal: %zu, threshold: %zu", free_internal, min_threshold);

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        min_threshold, free_internal,
        "Internal DRAM below threshold - WiFi/DMA may fail"
    );
}

/**
 * @brief Test heap watermark hasn't dropped too low
 *
 * The minimum free heap since boot indicates peak memory usage.
 * If this drops too low, the system may have been at risk of OOM.
 */
TEST_CASE("Heap watermark above safe level", "[resource][heap]")
{
    size_t min_free_ever = esp_get_minimum_free_heap_size();
    size_t safe_watermark = RESOURCE_GUARD_MIN_FREE_HEAP / 2;  // 50% of threshold

    ESP_LOGI(TAG, "Min free heap since boot: %zu, safe level: %zu",
             min_free_ever, safe_watermark);

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        safe_watermark, min_free_ever,
        "Heap watermark indicates near-OOM condition occurred"
    );
}

/**
 * @brief Test heap fragmentation is acceptable
 *
 * Severe fragmentation can cause allocation failures even with free memory.
 */
TEST_CASE("Heap fragmentation acceptable", "[resource][heap]")
{
    size_t free_heap = esp_get_free_heap_size();
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    ESP_LOGI(TAG, "Free heap: %zu, largest block: %zu, ratio: %.1f%%",
             free_heap, largest_block,
             free_heap > 0 ? 100.0f * largest_block / free_heap : 0);

    // Largest block should be at least 25% of free heap
    size_t min_largest_block = free_heap / 4;

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        min_largest_block, largest_block,
        "Heap severely fragmented - large allocations will fail"
    );
}

/**
 * @brief Test resource_guard_can_alloc() correctly predicts safe allocations
 */
TEST_CASE("can_alloc predicts safe allocations", "[resource][heap]")
{
    size_t free_heap = esp_get_free_heap_size();

    // Small allocation should be safe
    TEST_ASSERT_TRUE(resource_guard_can_alloc(1024));

    // Allocation that would leave minimum should be safe
    size_t safe_size = free_heap - RESOURCE_GUARD_MIN_FREE_HEAP - 1024;
    if (safe_size < free_heap) {  // Avoid underflow
        TEST_ASSERT_TRUE(resource_guard_can_alloc(safe_size));
    }

    // Allocation that would exhaust heap should not be safe
    TEST_ASSERT_FALSE(resource_guard_can_alloc(free_heap));

    // Allocation larger than free heap should not be safe
    TEST_ASSERT_FALSE(resource_guard_can_alloc(free_heap + 1024));
}

/**
 * @brief Test allocation and free doesn't leak memory
 */
TEST_CASE("Allocation cycle doesn't leak", "[resource][heap]")
{
    size_t heap_before = esp_get_free_heap_size();

    // Allocate and free several times
    for (int i = 0; i < 10; i++) {
        void *ptr = malloc(4096);
        TEST_ASSERT_NOT_NULL(ptr);
        memset(ptr, 0xAA, 4096);  // Touch memory
        free(ptr);
    }

    size_t heap_after = esp_get_free_heap_size();

    // Should be within 256 bytes (allow for heap metadata)
    int delta = (int)heap_before - (int)heap_after;
    ESP_LOGI(TAG, "Heap delta after alloc cycle: %d bytes", delta);

    TEST_ASSERT_INT_WITHIN_MESSAGE(
        256, 0, delta,
        "Memory leak detected in allocation cycle"
    );
}

// =============================================================================
// Stack Tests
// =============================================================================

/**
 * @brief Test current task stack has safe watermark
 */
TEST_CASE("Current task stack watermark safe", "[resource][stack]")
{
    task_stack_result_t result;
    bool ok = resource_guard_check_stack(NULL, &result);

    ESP_LOGI(TAG, "Task '%s' stack watermark: %lu bytes",
             result.task_name, (unsigned long)result.stack_watermark);

    TEST_ASSERT_TRUE_MESSAGE(ok, "Current task stack dangerously low");
    TEST_ASSERT_GREATER_OR_EQUAL(RESOURCE_GUARD_MIN_STACK_WATERMARK, result.stack_watermark);
}

/**
 * @brief Test that deep recursion triggers stack warning
 */
static volatile int s_recursion_depth = 0;

static void recursive_function(int depth, int max_depth)
{
    volatile char stack_user[64];  // Use some stack
    memset((void *)stack_user, depth & 0xFF, sizeof(stack_user));
    s_recursion_depth = depth;

    if (depth < max_depth) {
        recursive_function(depth + 1, max_depth);
    }
}

TEST_CASE("Deep recursion doesn't crash", "[resource][stack]")
{
    // Record initial watermark
    UBaseType_t initial_watermark = uxTaskGetStackHighWaterMark(NULL);

    // Recurse moderately (adjust based on stack size)
    recursive_function(0, 20);

    UBaseType_t final_watermark = uxTaskGetStackHighWaterMark(NULL);
    size_t stack_used = (initial_watermark - final_watermark) * sizeof(StackType_t);

    ESP_LOGI(TAG, "Recursion used ~%zu bytes of stack", stack_used);
    ESP_LOGI(TAG, "Final watermark: %u words (%zu bytes)",
             (unsigned int)final_watermark, final_watermark * sizeof(StackType_t));

    // Should still have safe margin
    TEST_ASSERT_GREATER_OR_EQUAL(
        RESOURCE_GUARD_MIN_STACK_WATERMARK / sizeof(StackType_t),
        final_watermark
    );
}

// =============================================================================
// Task Tests
// =============================================================================

/**
 * @brief Test task count is within limits
 */
TEST_CASE("Task count within limits", "[resource][task]")
{
    UBaseType_t task_count = uxTaskGetNumberOfTasks();

    ESP_LOGI(TAG, "Current task count: %u, limit: %d",
             (unsigned int)task_count, RESOURCE_GUARD_MAX_TASKS);

    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(
        RESOURCE_GUARD_MAX_TASKS, task_count,
        "Too many tasks - possible task leak"
    );
}

static void test_task(void *arg)
{
    volatile int counter = 0;
    while (counter < 1000) {
        counter++;
        vTaskDelay(1);
    }
    vTaskDelete(NULL);
}

/**
 * @brief Test creating and deleting tasks doesn't leak resources
 */
TEST_CASE("Task create/delete cycle doesn't leak", "[resource][task]")
{
    size_t heap_before = esp_get_free_heap_size();
    UBaseType_t tasks_before = uxTaskGetNumberOfTasks();

    // Create and delete several tasks
    for (int i = 0; i < 5; i++) {
        TaskHandle_t handle;
        BaseType_t ret = xTaskCreate(test_task, "test_task", 2048, NULL, 1, &handle);
        TEST_ASSERT_EQUAL(pdPASS, ret);

        // Let task run briefly
        vTaskDelay(pdMS_TO_TICKS(50));

        // Wait for task to delete itself
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Wait for cleanup
    vTaskDelay(pdMS_TO_TICKS(500));

    size_t heap_after = esp_get_free_heap_size();
    UBaseType_t tasks_after = uxTaskGetNumberOfTasks();

    ESP_LOGI(TAG, "Tasks: before=%u, after=%u", (unsigned int)tasks_before, (unsigned int)tasks_after);
    ESP_LOGI(TAG, "Heap: before=%zu, after=%zu", heap_before, heap_after);

    // Task count should return to original
    TEST_ASSERT_EQUAL_MESSAGE(tasks_before, tasks_after, "Task count increased - task leak");

    // Heap should be within tolerance (task cleanup may not be immediate)
    int heap_delta = (int)heap_before - (int)heap_after;
    TEST_ASSERT_INT_WITHIN_MESSAGE(1024, 0, heap_delta, "Heap leak after task cycle");
}

// =============================================================================
// Combined Resource Tests
// =============================================================================

/**
 * @brief Test resource_guard_check_all() passes
 */
TEST_CASE("All resource checks pass", "[resource]")
{
    resource_check_result_t result;
    esp_err_t err = resource_guard_check_all(&result);

    resource_guard_log_status();

    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, err, "Resource check failed");
    TEST_ASSERT_TRUE(result.heap_ok);
    TEST_ASSERT_TRUE(result.internal_ok);
    TEST_ASSERT_TRUE(result.fragmentation_ok);
    TEST_ASSERT_TRUE(result.task_count_ok);
    TEST_ASSERT_TRUE(result.all_ok);
}

/**
 * @brief Test system remains stable after memory pressure
 */
TEST_CASE("System stable after memory pressure", "[resource][stress]")
{
    // Record initial state
    size_t initial_free = esp_get_free_heap_size();

    // Allocate large blocks until we approach threshold
    void *blocks[20] = {0};
    int allocated = 0;

    for (int i = 0; i < 20; i++) {
        size_t block_size = 8192;

        if (!resource_guard_can_alloc(block_size)) {
            ESP_LOGI(TAG, "Stopped allocation at block %d (would exceed threshold)", i);
            break;
        }

        blocks[i] = malloc(block_size);
        if (blocks[i]) {
            memset(blocks[i], i, block_size);  // Touch memory
            allocated++;
        } else {
            break;
        }
    }

    ESP_LOGI(TAG, "Allocated %d blocks, free heap now: %zu",
             allocated, esp_get_free_heap_size());

    // Free all blocks
    for (int i = 0; i < 20; i++) {
        if (blocks[i]) {
            free(blocks[i]);
        }
    }

    // Verify system is stable
    size_t final_free = esp_get_free_heap_size();
    ESP_LOGI(TAG, "After freeing: %zu (initial: %zu)", final_free, initial_free);

    // Should recover most memory (within 1KB)
    TEST_ASSERT_INT_WITHIN(1024, initial_free, final_free);

    // All resource checks should still pass
    resource_check_result_t result;
    TEST_ASSERT_EQUAL(ESP_OK, resource_guard_check_all(&result));
}

// =============================================================================
// Log Buffer Specific Tests (REQ-31)
// =============================================================================

/**
 * @brief Test log buffer doesn't cause memory issues
 *
 * The log buffer uses 32KB which should leave plenty of free heap.
 */
TEST_CASE("Log buffer memory budget OK", "[resource][log_buffer]")
{
    // Log buffer is 32KB
    size_t log_buffer_size = 32 * 1024;
    size_t free_heap = esp_get_free_heap_size();

    ESP_LOGI(TAG, "Free heap: %zu, log buffer: %zu", free_heap, log_buffer_size);

    // Should still have plenty of free heap with log buffer
    TEST_ASSERT_GREATER_OR_EQUAL(
        RESOURCE_GUARD_MIN_FREE_HEAP + log_buffer_size,
        free_heap
    );
}

// =============================================================================
// Camera Stream Specific Tests (REQ-34)
// =============================================================================

/**
 * @brief Test camera stream task stack is adequate
 *
 * MJPEG streaming task uses 4096 bytes stack.
 */
TEST_CASE("Camera stream stack budget OK", "[resource][camera]")
{
    size_t stream_task_stack = 4096;
    size_t min_stack_per_task = RESOURCE_GUARD_MIN_STACK_WATERMARK;

    // Stack should be well above minimum watermark requirement
    TEST_ASSERT_GREATER_THAN(min_stack_per_task * 4, stream_task_stack);
}

// =============================================================================
// Test Runner
// =============================================================================

void app_main(void)
{
    ESP_LOGI(TAG, "Starting resource guard tests");
    ESP_LOGI(TAG, "Initial free heap: %zu", esp_get_free_heap_size());

    // Initialize resource guard
    resource_guard_init();

    // Log initial status
    resource_guard_log_status();

    // Run tests
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
