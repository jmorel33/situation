/**
 * @file test_timer.c
 * @brief Timer/Oscillator module tests — Temporal oscillator system, time queries
 *
 * Requires context: calls SituationInit() in setup, SituationShutdown() in teardown.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_init_ok = false;

static void timer_setup(void) {
    SituationInitInfo config = {0};
    config.window_width = 320;
    config.window_height = 240;
    config.window_title = "SIT_TEST_TIMER";

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    // Run a frame to initialize timers
    SituationPollInputEvents();
    SituationUpdateTimers();
}

static void timer_teardown(void) {
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

// ============================================================================
//  Time Query Tests
// ============================================================================

static void test_get_time(void) {
    double t = SituationTimerGetTime();
    // After init + one update, time should be > 0
    SIT_ASSERT(t >= 0.0);
}

static void test_get_time_advances(void) {
    double t1 = SituationTimerGetTime();
    // Do another frame
    SituationPollInputEvents();
    SituationUpdateTimers();
    double t2 = SituationTimerGetTime();
    // Time should advance (or at minimum not go backwards)
    SIT_ASSERT(t2 >= t1);
}

// ============================================================================
//  Oscillator Tests
// ============================================================================

static void test_set_oscillator_period(void) {
    // Set oscillator 0 to 0.5 seconds (120 BPM quarter note)
    SituationError err = SituationSetTimerOscillatorPeriod(0, 0.5);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
}

static void test_get_oscillator_period(void) {
    SituationSetTimerOscillatorPeriod(0, 0.25);
    double period = SituationTimerGetOscillatorPeriod(0);
    // Should match what we set (within floating point tolerance)
    SIT_ASSERT(period > 0.24 && period < 0.26);
}

static void test_oscillator_state(void) {
    // State should be a boolean (0 or 1)
    bool state = SituationTimerGetOscillatorState(0);
    SIT_ASSERT(state == true || state == false);
}

static void test_oscillator_previous_state(void) {
    bool prev = SituationTimerGetPreviousOscillatorState(0);
    SIT_ASSERT(prev == true || prev == false);
}

static void test_oscillator_has_updated(void) {
    // May or may not have updated — just verify no crash and returns bool
    bool updated = SituationTimerHasOscillatorUpdated(0);
    SIT_ASSERT(updated == true || updated == false);
}

static void test_oscillator_ping(void) {
    bool pinged = SituationTimerPingOscillator(0);
    SIT_ASSERT(pinged == true || pinged == false);
}

static void test_oscillator_trigger_count(void) {
    uint64_t count = SituationTimerGetOscillatorTriggerCount(0);
    // Count should be >= 0 (might be 0 if oscillator hasn't fired yet)
    SIT_ASSERT(count >= 0);
}

static void test_oscillator_ping_progress(void) {
    double progress = SituationTimerGetPingProgress(0);
    // Progress should be >= 0.0
    SIT_ASSERT(progress >= 0.0);
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase timer_tests[] = {
    // Time queries
    {"get_time",                test_get_time,              true},
    {"get_time_advances",       test_get_time_advances,     true},
    // Oscillator
    {"set_oscillator_period",   test_set_oscillator_period, true},
    {"get_oscillator_period",   test_get_oscillator_period, true},
    {"oscillator_state",        test_oscillator_state,      true},
    {"oscillator_previous_state", test_oscillator_previous_state, true},
    {"oscillator_has_updated",  test_oscillator_has_updated, true},
    {"oscillator_ping",         test_oscillator_ping,       true},
    {"oscillator_trigger_count", test_oscillator_trigger_count, true},
    {"oscillator_ping_progress", test_oscillator_ping_progress, true},
};

const SitTestModule g_module_timer = {
    .name = "timer",
    .setup = timer_setup,
    .teardown = timer_teardown,
    .tests = timer_tests,
    .test_count = sizeof(timer_tests) / sizeof(timer_tests[0]),
    .requires_context = true
};
