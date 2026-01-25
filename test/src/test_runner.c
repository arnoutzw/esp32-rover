/**
 * @file test_runner.c
 * @brief Main test runner for ESP32 Rover unit tests
 *
 * Runs all unit tests and reports results.
 */

#include <stdio.h>

/* Test suite declarations */
extern int run_config_tests(void);
extern int run_diag_state_machine_tests(void);

int main(void)
{
    int total_failures = 0;

    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         ESP32 Rover Firmware - Unit Test Suite             ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    /* Run all test suites */
    total_failures += run_config_tests();
    total_failures += run_diag_state_machine_tests();

    /* Final summary */
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    if (total_failures == 0) {
        printf("\033[32m✓ All test suites passed!\033[0m\n");
    } else {
        printf("\033[31m✗ %d test(s) failed across all suites\033[0m\n", total_failures);
    }
    printf("════════════════════════════════════════════════════════════\n");

    return total_failures;
}
