/**
 * @file resource_guard.h
 * @brief ESP32 Resource Consumption Guards
 *
 * This component provides runtime checks to guard against resource exhaustion
 * that could cause crashes. Based on ESP-IDF documentation best practices.
 *
 * Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/heap_debug.html
 *
 * ## Resource Limits (ESP32 with PSRAM)
 * - Internal DRAM: ~200KB available after boot
 * - PSRAM: ~4MB (if available)
 * - Task stack: Minimum 2KB recommended
 * - Max tasks: Limited by heap
 *
 * ## Critical Thresholds
 * These thresholds are based on ESP-IDF recommendations and empirical testing:
 * - Minimum free heap: 32KB (below this, allocations may fail)
 * - Minimum free internal: 16KB (critical for DMA, WiFi buffers)
 * - Stack watermark warning: 512 bytes remaining
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Resource Thresholds (configurable via menuconfig or defines)
// =============================================================================

#ifndef RESOURCE_GUARD_MIN_FREE_HEAP
#define RESOURCE_GUARD_MIN_FREE_HEAP        (32 * 1024)  // 32KB minimum free heap
#endif

#ifndef RESOURCE_GUARD_MIN_FREE_INTERNAL
#define RESOURCE_GUARD_MIN_FREE_INTERNAL    (16 * 1024)  // 16KB minimum internal RAM
#endif

#ifndef RESOURCE_GUARD_MIN_STACK_WATERMARK
#define RESOURCE_GUARD_MIN_STACK_WATERMARK  512          // 512 bytes minimum stack remaining
#endif

#ifndef RESOURCE_GUARD_MAX_TASKS
#define RESOURCE_GUARD_MAX_TASKS            32           // Maximum number of tasks
#endif

#ifndef RESOURCE_GUARD_WARN_HEAP_PERCENT
#define RESOURCE_GUARD_WARN_HEAP_PERCENT    80           // Warn when heap usage exceeds 80%
#endif

// =============================================================================
// Resource Check Result Structure
// =============================================================================

typedef struct {
    // Heap statistics
    size_t total_heap;              // Total heap size
    size_t free_heap;               // Current free heap
    size_t min_free_heap;           // Minimum free heap since boot (watermark)
    size_t free_internal;           // Free internal (DRAM) heap
    size_t largest_free_block;      // Largest contiguous free block

    // Task statistics
    uint32_t task_count;            // Number of running tasks
    uint32_t tasks_core0;           // Tasks on Core 0
    uint32_t tasks_core1;           // Tasks on Core 1

    // Check results
    bool heap_ok;                   // Free heap above minimum threshold
    bool internal_ok;               // Internal RAM above minimum threshold
    bool fragmentation_ok;          // Not severely fragmented
    bool task_count_ok;             // Task count within limits
    bool all_ok;                    // All checks passed
} resource_check_result_t;

typedef struct {
    const char *task_name;          // Task name
    uint32_t stack_watermark;       // Minimum stack free (high water mark)
    uint32_t stack_size;            // Total stack size
    bool stack_ok;                  // Stack watermark above minimum
} task_stack_result_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize resource guard (optional, called automatically)
 * @return ESP_OK on success
 */
esp_err_t resource_guard_init(void);

/**
 * @brief Perform all resource checks
 * @param[out] result Pointer to store check results
 * @return ESP_OK if all checks pass, ESP_ERR_INVALID_STATE if any check fails
 *
 * This function checks:
 * - Total free heap against RESOURCE_GUARD_MIN_FREE_HEAP
 * - Free internal RAM against RESOURCE_GUARD_MIN_FREE_INTERNAL
 * - Heap fragmentation (largest block should be reasonable)
 * - Task count against RESOURCE_GUARD_MAX_TASKS
 */
esp_err_t resource_guard_check_all(resource_check_result_t *result);

/**
 * @brief Check heap resources only
 * @param[out] result Pointer to store check results (may be NULL)
 * @return true if heap resources are OK
 */
bool resource_guard_check_heap(resource_check_result_t *result);

/**
 * @brief Check if a memory allocation of given size is safe
 * @param size Size in bytes to check
 * @return true if allocation would leave sufficient free heap
 *
 * Use this before large allocations to prevent crashes.
 * Returns true if (current_free - size) >= RESOURCE_GUARD_MIN_FREE_HEAP
 */
bool resource_guard_can_alloc(size_t size);

/**
 * @brief Check task stack usage
 * @param task_handle Task handle (NULL for current task)
 * @param[out] result Pointer to store result (may be NULL)
 * @return true if stack usage is safe
 *
 * Uses uxTaskGetStackHighWaterMark() to check remaining stack.
 */
bool resource_guard_check_stack(void *task_handle, task_stack_result_t *result);

/**
 * @brief Check all task stacks and log warnings
 * @return Number of tasks with low stack watermark
 *
 * Iterates through all tasks and logs warnings for any with
 * stack watermark below RESOURCE_GUARD_MIN_STACK_WATERMARK.
 */
int resource_guard_check_all_stacks(void);

/**
 * @brief Get current heap usage percentage
 * @return Heap usage as percentage (0-100)
 */
uint8_t resource_guard_get_heap_usage_percent(void);

/**
 * @brief Log current resource status
 *
 * Logs heap stats, task count, and any warnings to the console.
 */
void resource_guard_log_status(void);

/**
 * @brief Assert that resources are OK (for use in tests)
 *
 * Calls resource_guard_check_all() and logs detailed info if any check fails.
 * Use in unit tests to verify resource consumption.
 */
void resource_guard_assert_ok(void);

// =============================================================================
// Test Macros for Unity Framework
// =============================================================================

/**
 * @brief Unity test assertion macros for resource guards
 *
 * Usage in unit tests:
 *   TEST_ASSERT_RESOURCE_OK()              - Assert all resources OK
 *   TEST_ASSERT_HEAP_OK()                  - Assert heap only
 *   TEST_ASSERT_CAN_ALLOC(size)            - Assert allocation is safe
 *   TEST_ASSERT_STACK_OK(handle)           - Assert task stack OK
 */
#ifdef UNITY_INCLUDE_CONFIG_H
#include "unity.h"

#define TEST_ASSERT_RESOURCE_OK() do { \
    resource_check_result_t __res; \
    esp_err_t __err = resource_guard_check_all(&__res); \
    if (__err != ESP_OK) { \
        resource_guard_log_status(); \
    } \
    TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, __err, "Resource check failed"); \
} while(0)

#define TEST_ASSERT_HEAP_OK() do { \
    resource_check_result_t __res; \
    bool __ok = resource_guard_check_heap(&__res); \
    if (!__ok) { \
        resource_guard_log_status(); \
    } \
    TEST_ASSERT_TRUE_MESSAGE(__ok, "Heap check failed"); \
} while(0)

#define TEST_ASSERT_CAN_ALLOC(size) do { \
    bool __ok = resource_guard_can_alloc(size); \
    TEST_ASSERT_TRUE_MESSAGE(__ok, "Allocation would exhaust heap"); \
} while(0)

#define TEST_ASSERT_STACK_OK(handle) do { \
    task_stack_result_t __res; \
    bool __ok = resource_guard_check_stack(handle, &__res); \
    TEST_ASSERT_TRUE_MESSAGE(__ok, "Stack watermark too low"); \
} while(0)

#endif // UNITY_INCLUDE_CONFIG_H

#ifdef __cplusplus
}
#endif
