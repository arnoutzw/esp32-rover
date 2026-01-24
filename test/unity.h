/**
 * Minimal Unity test framework header for host-based testing
 * Based on Unity - A unit test framework for C
 * https://github.com/ThrowTheSwitch/Unity
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Test counters */
static int unity_tests_run = 0;
static int unity_tests_failed = 0;
static const char* unity_current_test = NULL;

/* Color codes for terminal */
#define UNITY_COLOR_RED    "\033[31m"
#define UNITY_COLOR_GREEN  "\033[32m"
#define UNITY_COLOR_RESET  "\033[0m"

/* Test macros */
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("%s:%d: FAIL: %s\n", __FILE__, __LINE__, #condition); \
            unity_tests_failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("%s:%d: FAIL: Expected %d, got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
            unity_tests_failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) TEST_ASSERT_EQUAL(expected, actual)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("%s:%d: FAIL: Expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, (expected), (actual)); \
            unity_tests_failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(pointer) \
    do { \
        if ((pointer) == NULL) { \
            printf("%s:%d: FAIL: Expected non-NULL pointer\n", __FILE__, __LINE__); \
            unity_tests_failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(pointer) \
    do { \
        if ((pointer) != NULL) { \
            printf("%s:%d: FAIL: Expected NULL pointer\n", __FILE__, __LINE__); \
            unity_tests_failed++; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual) \
    do { \
        float _diff = (expected) - (actual); \
        if (_diff < 0) _diff = -_diff; \
        if (_diff > (delta)) { \
            printf("%s:%d: FAIL: Expected %f within %f, got %f\n", __FILE__, __LINE__, (double)(expected), (double)(delta), (double)(actual)); \
            unity_tests_failed++; \
            return; \
        } \
    } while(0)

/* Run a test function */
#define RUN_TEST(func) \
    do { \
        unity_current_test = #func; \
        unity_tests_run++; \
        int _failed_before = unity_tests_failed; \
        func(); \
        if (unity_tests_failed == _failed_before) { \
            printf(UNITY_COLOR_GREEN "." UNITY_COLOR_RESET); \
        } else { \
            printf(UNITY_COLOR_RED "F" UNITY_COLOR_RESET); \
        } \
    } while(0)

/* Initialize Unity */
#define UNITY_BEGIN() \
    do { \
        unity_tests_run = 0; \
        unity_tests_failed = 0; \
        printf("\n"); \
    } while(0)

/* Finalize Unity and return result */
#define UNITY_END() \
    do { \
        printf("\n\n"); \
        if (unity_tests_failed == 0) { \
            printf(UNITY_COLOR_GREEN "All %d tests passed!" UNITY_COLOR_RESET "\n", unity_tests_run); \
        } else { \
            printf(UNITY_COLOR_RED "%d of %d tests failed" UNITY_COLOR_RESET "\n", unity_tests_failed, unity_tests_run); \
        } \
        return unity_tests_failed; \
    } while(0)

#endif /* UNITY_H */
