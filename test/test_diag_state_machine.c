/**
 * @file test_diag_state_machine.c
 * @brief Unit tests for diagnostic mode state machine (REQ-06)
 *
 * REQ-06: The diagnostic screen is entered by long pressing both buttons
 * together for 3s. Once the buttons are let loose you will stay there.
 * Exit by holding both buttons for 1s.
 */

#include "unity.h"
#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * Diagnostic Mode State Machine (extracted from main.c for testing)
 * ========================================================================== */

typedef enum {
    DIAG_MODE_OFF,
    DIAG_MODE_ENTERING,      /* Both buttons held, counting down to enter */
    DIAG_MODE_WAIT_RELEASE,  /* Entered, waiting for buttons to be released */
    DIAG_MODE_ON,            /* Diagnostic screen active, buttons released */
    DIAG_MODE_EXIT_PENDING,  /* Both buttons pressed to exit, waiting for hold */
    DIAG_MODE_EXITING        /* Confirmed exit, returning to normal */
} diag_mode_t;

/* Timing constants (in ticks, assuming 1ms per tick for testing) */
#define DIAG_ENTRY_HOLD_TIME 3000  /* 3 seconds to enter */
#define DIAG_EXIT_HOLD_TIME  1000  /* 1 second to exit */

/* State machine context */
typedef struct {
    diag_mode_t mode;
    uint32_t both_buttons_start;
    uint32_t current_tick;
} diag_context_t;

/* Initialize context */
static void diag_init(diag_context_t *ctx)
{
    ctx->mode = DIAG_MODE_OFF;
    ctx->both_buttons_start = 0;
    ctx->current_tick = 0;
}

/* Process one tick of the state machine */
static void diag_process(diag_context_t *ctx, bool btn_left, bool btn_right)
{
    bool both_pressed = btn_left && btn_right;

    switch (ctx->mode) {
        case DIAG_MODE_OFF:
            if (both_pressed) {
                ctx->both_buttons_start = ctx->current_tick;
                ctx->mode = DIAG_MODE_ENTERING;
            }
            break;

        case DIAG_MODE_ENTERING:
            if (!both_pressed) {
                ctx->mode = DIAG_MODE_OFF;
            } else if ((ctx->current_tick - ctx->both_buttons_start) >= DIAG_ENTRY_HOLD_TIME) {
                ctx->mode = DIAG_MODE_WAIT_RELEASE;
            }
            break;

        case DIAG_MODE_WAIT_RELEASE:
            if (!both_pressed) {
                ctx->mode = DIAG_MODE_ON;
            }
            break;

        case DIAG_MODE_ON:
            if (both_pressed) {
                ctx->both_buttons_start = ctx->current_tick;
                ctx->mode = DIAG_MODE_EXIT_PENDING;
            }
            break;

        case DIAG_MODE_EXIT_PENDING:
            if (!both_pressed) {
                ctx->mode = DIAG_MODE_ON;
            } else if ((ctx->current_tick - ctx->both_buttons_start) >= DIAG_EXIT_HOLD_TIME) {
                ctx->mode = DIAG_MODE_EXITING;
            }
            break;

        case DIAG_MODE_EXITING:
            ctx->mode = DIAG_MODE_OFF;
            break;
    }
}

/* Helper to check if currently showing diagnostics */
static bool diag_is_showing(diag_context_t *ctx)
{
    return ctx->mode == DIAG_MODE_ON ||
           ctx->mode == DIAG_MODE_WAIT_RELEASE ||
           ctx->mode == DIAG_MODE_EXIT_PENDING;
}

/* ==========================================================================
 * Test Cases for REQ-06
 * ========================================================================== */

void test_req06_initial_state_is_off(void)
{
    diag_context_t ctx;
    diag_init(&ctx);
    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);
}

void test_req06_no_buttons_stays_off(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Process 100 ticks with no buttons pressed */
    for (int i = 0; i < 100; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, false, false);
    }

    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);
}

void test_req06_single_button_stays_off(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Press only left button for 5 seconds */
    for (int i = 0; i < 5000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, false);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);

    /* Reset and try right button only */
    diag_init(&ctx);
    for (int i = 0; i < 5000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, false, true);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);
}

void test_req06_both_buttons_enters_entering_state(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Press both buttons */
    ctx.current_tick = 0;
    diag_process(&ctx, true, true);

    TEST_ASSERT_EQUAL(DIAG_MODE_ENTERING, ctx.mode);
}

void test_req06_release_before_3s_cancels_entry(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Press both buttons */
    ctx.current_tick = 0;
    diag_process(&ctx, true, true);
    TEST_ASSERT_EQUAL(DIAG_MODE_ENTERING, ctx.mode);

    /* Hold for 2 seconds */
    for (int i = 1; i < 2000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_ENTERING, ctx.mode);

    /* Release at 2 seconds - should cancel */
    ctx.current_tick = 2000;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);
}

void test_req06_hold_3s_enters_wait_release(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Press and hold both buttons for 3 seconds */
    for (int i = 0; i <= 3000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }

    TEST_ASSERT_EQUAL(DIAG_MODE_WAIT_RELEASE, ctx.mode);
}

void test_req06_release_after_entry_goes_to_on(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Enter diagnostic mode (hold 3s) */
    for (int i = 0; i <= 3000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_WAIT_RELEASE, ctx.mode);

    /* Release buttons */
    ctx.current_tick = 3001;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);
}

void test_req06_stays_on_after_release(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Enter diagnostic mode */
    for (int i = 0; i <= 3000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    ctx.current_tick = 3001;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);

    /* Continue for 10 more seconds with no buttons - should stay ON */
    for (int i = 3002; i < 13000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, false, false);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);
    TEST_ASSERT_TRUE(diag_is_showing(&ctx));
}

void test_req06_brief_press_in_on_mode_stays_on(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Enter diagnostic mode */
    for (int i = 0; i <= 3000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    ctx.current_tick = 3001;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);

    /* Brief press (500ms) - should not exit */
    for (int i = 3002; i < 3502; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_EXIT_PENDING, ctx.mode);

    /* Release before 1s - should go back to ON */
    ctx.current_tick = 3502;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);
}

void test_req06_hold_1s_in_on_mode_exits(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Enter diagnostic mode */
    for (int i = 0; i <= 3000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    ctx.current_tick = 3001;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);

    /* Hold both buttons for 1 second to exit */
    for (int i = 3002; i <= 4002; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_EXITING, ctx.mode);

    /* Process one more tick to complete exit */
    ctx.current_tick = 4003;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);
}

void test_req06_full_cycle(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* Start in OFF */
    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);
    TEST_ASSERT_FALSE(diag_is_showing(&ctx));

    /* Press both buttons - enters ENTERING */
    ctx.current_tick = 0;
    diag_process(&ctx, true, true);
    TEST_ASSERT_EQUAL(DIAG_MODE_ENTERING, ctx.mode);

    /* Hold for 3 seconds - enters WAIT_RELEASE */
    for (int i = 1; i <= 3000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_WAIT_RELEASE, ctx.mode);
    TEST_ASSERT_TRUE(diag_is_showing(&ctx));

    /* Release - enters ON */
    ctx.current_tick = 3001;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);
    TEST_ASSERT_TRUE(diag_is_showing(&ctx));

    /* Wait 5 seconds - still ON */
    for (int i = 3002; i < 8000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, false, false);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);

    /* Press both buttons - enters EXIT_PENDING */
    ctx.current_tick = 8000;
    diag_process(&ctx, true, true);
    TEST_ASSERT_EQUAL(DIAG_MODE_EXIT_PENDING, ctx.mode);

    /* Hold for 1 second - enters EXITING */
    for (int i = 8001; i <= 9000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    TEST_ASSERT_EQUAL(DIAG_MODE_EXITING, ctx.mode);

    /* Process to complete exit - back to OFF */
    ctx.current_tick = 9001;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);
    TEST_ASSERT_FALSE(diag_is_showing(&ctx));
}

void test_req06_can_reenter_after_exit(void)
{
    diag_context_t ctx;
    diag_init(&ctx);

    /* First cycle: enter and exit */
    for (int i = 0; i <= 3000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    ctx.current_tick = 3001;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);

    /* Exit */
    for (int i = 3002; i <= 4002; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    ctx.current_tick = 4003;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_OFF, ctx.mode);

    /* Wait a bit */
    for (int i = 4004; i < 5000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, false, false);
    }

    /* Second cycle: enter again */
    for (int i = 5000; i <= 8000; i++) {
        ctx.current_tick = i;
        diag_process(&ctx, true, true);
    }
    ctx.current_tick = 8001;
    diag_process(&ctx, false, false);
    TEST_ASSERT_EQUAL(DIAG_MODE_ON, ctx.mode);
}

/* ==========================================================================
 * Test Runner for Diagnostic State Machine Tests
 * ========================================================================== */

int run_diag_state_machine_tests(void)
{
    printf("\n=== Diagnostic State Machine Tests (REQ-06) ===\n");

    UNITY_BEGIN();

    printf("\nBasic State Tests\n");
    RUN_TEST(test_req06_initial_state_is_off);
    RUN_TEST(test_req06_no_buttons_stays_off);
    RUN_TEST(test_req06_single_button_stays_off);

    printf("\nEntry Tests\n");
    RUN_TEST(test_req06_both_buttons_enters_entering_state);
    RUN_TEST(test_req06_release_before_3s_cancels_entry);
    RUN_TEST(test_req06_hold_3s_enters_wait_release);
    RUN_TEST(test_req06_release_after_entry_goes_to_on);

    printf("\nStay in Diagnostic Mode Tests\n");
    RUN_TEST(test_req06_stays_on_after_release);
    RUN_TEST(test_req06_brief_press_in_on_mode_stays_on);

    printf("\nExit Tests\n");
    RUN_TEST(test_req06_hold_1s_in_on_mode_exits);

    printf("\nFull Cycle Tests\n");
    RUN_TEST(test_req06_full_cycle);
    RUN_TEST(test_req06_can_reenter_after_exit);

    UNITY_END();
}
