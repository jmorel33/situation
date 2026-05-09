/**
 * @file test_core.c
 * @brief Core/Lifecycle module tests — Init, Shutdown, State, FPS, Callbacks, System Info
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

static void core_setup(void) {
    SituationInitInfo config = {0};
    config.window_width = 320;
    config.window_height = 240;
    config.window_title = "SIT_TEST_CORE";
    config.initial_active_window_flags = 0;

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        // Setup failed — framework will skip all tests
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void core_teardown(void) {
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

// ============================================================================
//  Version & Init State Tests
// ============================================================================

static void test_version_string(void) {
    const char* ver = SituationGetVersionString();
    SIT_ASSERT_NOT_NULL(ver);
    SIT_ASSERT(strlen(ver) > 0);
}

static void test_is_initialized(void) {
    SIT_ASSERT(SituationIsInitialized());
}

static void test_init_state_ready(void) {
    SituationInitState state = SituationGetInitState();
    SIT_ASSERT_EQ(state, SITUATION_STATE_READY);
}

// ============================================================================
//  Frame Timing Tests
// ============================================================================

static void test_poll_and_update(void) {
    // Should not crash — just exercises the frame begin path
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(true);
}

static void test_get_frame_time(void) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    float dt = SituationGetFrameTime();
    // dt should be >= 0 (first frame might be 0 or very small)
    SIT_ASSERT(dt >= 0.0f);
}

static void test_get_fps(void) {
    // FPS might be 0 on first frame, but should not be negative
    int fps = SituationGetFPS();
    SIT_ASSERT(fps >= 0);
}

static void test_set_target_fps(void) {
    // Should not crash — just sets internal state
    SituationSetTargetFPS(60);
    SIT_ASSERT(true);
    SituationSetTargetFPS(0); // Reset to uncapped
}

// ============================================================================
//  App State Tests
// ============================================================================

static void test_window_should_close(void) {
    // After init, window should NOT want to close
    SIT_ASSERT(!SituationWindowShouldClose());
}

static void test_pause_resume(void) {
    SIT_ASSERT(!SituationIsAppPaused());
    SituationPauseApp();
    SIT_ASSERT(SituationIsAppPaused());
    SituationResumeApp();
    SIT_ASSERT(!SituationIsAppPaused());
}

// ============================================================================
//  Callback Tests
// ============================================================================

static void test_set_exit_callback(void) {
    // Set a callback, then set NULL (unregister) — neither should crash
    SituationSetExitCallback(NULL, NULL);
    SIT_ASSERT(true);
}

static void test_set_resize_callback(void) {
    SituationSetResizeCallback(NULL, NULL);
    SIT_ASSERT(true);
}

static void test_set_focus_callback(void) {
    SituationSetFocusCallback(NULL, NULL);
    SIT_ASSERT(true);
}

static void test_set_file_drop_callback(void) {
    SituationSetFileDropCallback(NULL, NULL);
    SIT_ASSERT(true);
}

// ============================================================================
//  System Info Tests
// ============================================================================

static void test_get_cpu_thread_count(void) {
    uint32_t count = SituationGetCPUThreadCount();
    SIT_ASSERT(count > 0);
}

static void test_get_gpu_name(void) {
    const char* name = SituationGetGPUName();
    SIT_ASSERT_NOT_NULL(name);
    SIT_ASSERT(strlen(name) > 0);
}

static void test_get_user_directory(void) {
    char* dir = SituationGetUserDirectory();
    SIT_ASSERT_NOT_NULL(dir);
    SIT_ASSERT(strlen(dir) > 0);
    SituationFreeString(dir);
}

static void test_get_device_info(void) {
    SituationDeviceInfo info = SituationGetDeviceInfo();
    // CPU name should be populated
    SIT_ASSERT(strlen(info.cpu_name) > 0);
    // Should have at least 1 core
    SIT_ASSERT(info.cpu_cores > 0);
}

static void test_get_last_error_msg(void) {
    // After successful init, there should be no pending error
    // But the function should still be callable without crashing
    char* msg = NULL;
    SituationGetLastErrorMsg(&msg);
    // msg might be NULL (no error) or a valid string — either is fine
    if (msg) free(msg);
    SIT_ASSERT(true);
}

static void test_argument_queries(void) {
    // We didn't pass any args, so these should return false/NULL
    SIT_ASSERT(!SituationIsArgumentPresent("-nonexistent"));
    const char* val = SituationGetArgumentValue("-nonexistent");
    SIT_ASSERT_NULL((void*)val);
}

// ============================================================================
//  Phase 20 — System Utilities & Logging
// ============================================================================

static void test_log_info(void) {
    // Should not crash — just logs a message
    SituationLog(SIT_LOG_INFO, "sit_test: Phase 20 log info test");
    SIT_ASSERT(true);
}

static void test_log_warning(void) {
    // SituationLogWarning takes an error code + format string
    SituationLogWarning(SITUATION_SUCCESS, "sit_test: Phase 20 warning test");
    SIT_ASSERT(true);
}

static void test_set_trace_log_level_error(void) {
    SituationSetTraceLogLevel(SIT_LOG_ERROR);
    SIT_ASSERT(true);
}

static void test_set_trace_log_level_none(void) {
    SituationSetTraceLogLevel(SIT_LOG_NONE);
    SIT_ASSERT(true);
    // Restore to default
    SituationSetTraceLogLevel(SIT_LOG_INFO);
}

static void test_free_string_null(void) {
    // Should not crash on NULL
    SituationFreeString(NULL);
    SIT_ASSERT(true);
}

static void test_free_string_valid(void) {
    // Get a string from the API, then free it
    char* path = SituationGetBasePath();
    SIT_ASSERT_NOT_NULL(path);
    SituationFreeString(path);
    SIT_ASSERT(true);
}

#if defined(_WIN32)
static void test_get_current_drive_letter(void) {
    char letter = SituationGetCurrentDriveLetter();
    // Should be A-Z or 0 (if somehow fails)
    SIT_ASSERT((letter >= 'A' && letter <= 'Z') || letter == 0);
}

static void test_get_drive_info(void) {
    char letter = SituationGetCurrentDriveLetter();
    if (letter >= 'A' && letter <= 'Z') {
        uint64_t total = 0, free_space = 0;
        char vol_name[256] = {0};
        bool ok = SituationGetDriveInfo(letter, &total, &free_space, vol_name, sizeof(vol_name));
        // May or may not succeed depending on drive, but should not crash
        if (ok) {
            SIT_ASSERT(total > 0);
        }
    }
    SIT_ASSERT(true);
}
#endif

static void test_execute_command(void) {
    char* output = NULL;
    int ret = SituationExecuteCommand("echo sit_test_phase20", &output);
    // On Windows, echo should succeed (exit code 0)
    SIT_ASSERT(ret == 0);
    if (output) {
        SIT_ASSERT(strstr(output, "sit_test_phase20") != NULL);
        free(output);
    }
}

static void test_get_last_error_msg_after_init(void) {
    // After successful init, calling GetLastErrorMsg should be safe
    char* msg = NULL;
    SituationError err = SituationGetLastErrorMsg(&msg);
    // Either returns SUCCESS with a message, or no error pending
    (void)err;
    if (msg) free(msg);
    SIT_ASSERT(true);
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase core_tests[] = {
    // Version & state
    {"version_string",          test_version_string,        true},
    {"is_initialized",          test_is_initialized,        true},
    {"init_state_ready",        test_init_state_ready,      true},
    // Frame timing
    {"poll_and_update",         test_poll_and_update,       true},
    {"get_frame_time",          test_get_frame_time,        true},
    {"get_fps",                 test_get_fps,               true},
    {"set_target_fps",          test_set_target_fps,        true},
    // App state
    {"window_should_close",     test_window_should_close,   true},
    {"pause_resume",            test_pause_resume,          true},
    // Callbacks
    {"set_exit_callback",       test_set_exit_callback,     true},
    {"set_resize_callback",     test_set_resize_callback,   true},
    {"set_focus_callback",      test_set_focus_callback,    true},
    {"set_file_drop_callback",  test_set_file_drop_callback, true},
    // System info
    {"get_cpu_thread_count",    test_get_cpu_thread_count,  true},
    {"get_gpu_name",            test_get_gpu_name,          true},
    {"get_user_directory",      test_get_user_directory,    true},
    {"get_device_info",         test_get_device_info,       true},
    {"get_last_error_msg",      test_get_last_error_msg,    true},
    {"argument_queries",        test_argument_queries,      true},

    // --- Phase 20: System Utilities & Logging (9+) ---
    {"log_info",                    test_log_info,                  true},
    {"log_warning",                 test_log_warning,               true},
    {"set_trace_log_level_error",   test_set_trace_log_level_error, true},
    {"set_trace_log_level_none",    test_set_trace_log_level_none,  true},
    {"free_string_null",            test_free_string_null,          true},
    {"free_string_valid",           test_free_string_valid,         true},
#if defined(_WIN32)
    {"get_current_drive_letter",    test_get_current_drive_letter,  true},
    {"get_drive_info",              test_get_drive_info,            true},
#endif
    {"execute_command",             test_execute_command,            true},
    {"error_msg_after_init",        test_get_last_error_msg_after_init, true},
};

const SitTestModule g_module_core = {
    .name = "core",
    .setup = core_setup,
    .teardown = core_teardown,
    .tests = core_tests,
    .test_count = sizeof(core_tests) / sizeof(core_tests[0]),
    .requires_context = true
};
