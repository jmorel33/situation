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
#include "sit_test_window.h"

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_init_ok = false;

static void core_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_CORE");
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    config.render_thread_count = 1;
#endif

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

static void test_init_double_init_reports_error(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_DOUBLE_INIT");

    SituationError err = SituationInit(0, NULL, &config);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_ALREADY_INITIALIZED);
    SIT_ASSERT_EQ(SituationGetLastErrorCode(), SITUATION_ERROR_ALREADY_INITIALIZED);
}

static void test_init_success_sets_ready_state(void) {
    SIT_ASSERT(SituationIsInitialized());
    SIT_ASSERT_EQ(SituationGetInitState(), SITUATION_STATE_READY);
}

static void test_errno_table_phase_2_1(void) {
    SIT_ASSERT_NOT_NULL(SituationErrorToString(SITUATION_ERROR_BACKEND_SPECIFIC));
    SIT_ASSERT_NOT_NULL(SituationErrorToString(SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED));
    SIT_ASSERT_NOT_NULL(SituationErrorToString(SITUATION_ERROR_MEMORY_ACCESS));
    SIT_ASSERT_NOT_NULL(SituationErrorToString(SITUATION_ERROR_FILE_MODIFIED));
    SIT_ASSERT(strcmp(SituationErrorToString(SITUATION_ERROR_GLAD_LOAD_FAILED),
                      SituationErrorToString(SITUATION_ERROR_OPENGL_LOADER_FAILED)) == 0);
    SIT_ASSERT(strcmp(SituationErrorToString(SITUATION_ERROR_ACCESS_DENIED),
                      SituationErrorToString(SITUATION_ERROR_FILE_ACCESS_DENIED)) == 0);
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

static void test_get_graphics_backend(void) {
#if defined(SITUATION_USE_VULKAN)
    SIT_ASSERT_EQ(SituationGetGraphicsBackend(), SIT_GRAPHICS_BACKEND_VULKAN);
    SIT_ASSERT(strcmp(SituationGetGraphicsBackendName(), "Vulkan") == 0);
#elif defined(SITUATION_USE_OPENGL)
    SIT_ASSERT_EQ(SituationGetGraphicsBackend(), SIT_GRAPHICS_BACKEND_OPENGL);
    SIT_ASSERT(strcmp(SituationGetGraphicsBackendName(), "OpenGL") == 0);
#endif
}

static void test_get_graphics_caps(void) {
    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
#if defined(SITUATION_USE_VULKAN)
    SIT_ASSERT_EQ(caps.backend, SIT_GRAPHICS_BACKEND_VULKAN);
    SIT_ASSERT_EQ(caps.api_version_packed, (1u << 16) | 4u);
    SIT_ASSERT(caps.device_api_version_packed >= (1u << 16));
#elif defined(SITUATION_USE_OPENGL)
    SIT_ASSERT_EQ(caps.backend, SIT_GRAPHICS_BACKEND_OPENGL);
    SIT_ASSERT_EQ(caps.api_version_packed, (4u << 16) | 6u);
    SIT_ASSERT((caps.device_api_version_packed >> 16) >= 4);
#endif
    SIT_ASSERT(caps.compute_supported == 1);
    SIT_ASSERT(caps.max_viewports >= 1);
}

static void test_viewport_index_zero_parity(void) {
    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    SIT_ASSERT(caps.max_viewports >= 1);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);

    const float win_w = (float)sit_test_window_width();
    const float win_h = (float)sit_test_window_height();
    const float half_w = win_w * 0.5f;
    const float half_h = win_h * 0.5f;

    SIT_ASSERT_EQ(SituationCmdSetViewport(cmd, 0.0f, 0.0f, win_w, win_h), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdSetScissor(cmd, 0, 0, (int)win_w, (int)win_h), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdSetViewportIndexed(cmd, 0, 0.0f, 0.0f, win_w, win_h), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdSetScissorIndexed(cmd, 0, 0, 0, (int)win_w, (int)win_h), SITUATION_SUCCESS);

    if (caps.max_viewports >= 2) {
        SIT_ASSERT_EQ(SituationCmdSetViewportIndexed(cmd, 1, 0.0f, 0.0f, half_w, half_h), SITUATION_SUCCESS);
        SIT_ASSERT_EQ(SituationCmdSetScissorIndexed(cmd, 1, 0, 0, (int)half_w, (int)half_h), SITUATION_SUCCESS);
    }

    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);
}

static void test_viewport_index_out_of_range(void) {
    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    SIT_ASSERT(caps.max_viewports >= 1);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);

    uint32_t bad_index = (uint32_t)caps.max_viewports;
    const float win_w = (float)sit_test_window_width();
    const float win_h = (float)sit_test_window_height();

    SIT_ASSERT_EQ(SituationCmdSetViewportIndexed(cmd, bad_index, 0.0f, 0.0f, win_w, win_h),
                  SITUATION_ERROR_INVALID_PARAM);
    SIT_ASSERT_EQ(SituationCmdSetScissorIndexed(cmd, bad_index, 0, 0, (int)win_w, (int)win_h),
        SITUATION_ERROR_INVALID_PARAM);

    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SituationEndFrame();
}

static void test_render_pass_info_default_helper(void) {
    ColorRGBA clear = {12, 34, 56, 255};
    SituationRenderPassInfo helper = SituationRenderPassInfoDefault(-1, clear);
    SituationRenderPassInfo manual = {0};
    manual.display_id = -1;
    manual.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    manual.color_attachment.storeOp = SIT_STORE_OP_STORE;
    manual.color_attachment.clear.color = clear;
    manual.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    manual.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    manual.depth_attachment.clear.depth = 1.0f;
    manual.stencil_attachment.loadOp = SIT_LOAD_OP_DONT_CARE;
    manual.stencil_attachment.storeOp = SIT_STORE_OP_DONT_CARE;

    SIT_ASSERT_EQ(helper.display_id, manual.display_id);
    SIT_ASSERT_EQ(helper.color_attachment.loadOp, manual.color_attachment.loadOp);
    SIT_ASSERT_EQ(helper.color_attachment.storeOp, manual.color_attachment.storeOp);
    SIT_ASSERT_EQ(helper.color_attachment.clear.color.r, manual.color_attachment.clear.color.r);
    SIT_ASSERT_EQ(helper.depth_attachment.loadOp, manual.depth_attachment.loadOp);
    SIT_ASSERT_EQ(helper.depth_attachment.storeOp, manual.depth_attachment.storeOp);
    SIT_ASSERT(helper.depth_attachment.clear.depth == manual.depth_attachment.clear.depth);
    SIT_ASSERT_EQ(helper.stencil_attachment.loadOp, manual.stencil_attachment.loadOp);
    SIT_ASSERT_EQ(helper.stencil_attachment.storeOp, manual.stencil_attachment.storeOp);
    SIT_ASSERT_EQ(SituationRenderPassConfigurationKey(&helper), SituationRenderPassConfigurationKey(&manual));
}

static void test_render_pass_info_load_helper(void) {
    SituationRenderPassInfo helper = SituationRenderPassInfoLoad(2);
    SIT_ASSERT_EQ(helper.display_id, 2);
    SIT_ASSERT_EQ(helper.color_attachment.loadOp, SIT_LOAD_OP_LOAD);
    SIT_ASSERT_EQ(helper.color_attachment.storeOp, SIT_STORE_OP_STORE);
    SIT_ASSERT_EQ(helper.depth_attachment.loadOp, SIT_LOAD_OP_LOAD);
    SIT_ASSERT_EQ(helper.depth_attachment.storeOp, SIT_STORE_OP_DONT_CARE);
    SIT_ASSERT_EQ(helper.stencil_attachment.loadOp, SIT_LOAD_OP_LOAD);
    SIT_ASSERT_EQ(helper.stencil_attachment.storeOp, SIT_STORE_OP_DONT_CARE);
}

static void test_render_pass_configuration_key_stencil_ops(void) {
    SituationRenderPassInfo base = SituationRenderPassInfoDefault(-1, (ColorRGBA){0, 0, 0, 255});
    SituationRenderPassInfo stencil_clear = base;
    stencil_clear.stencil_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    stencil_clear.stencil_attachment.storeOp = SIT_STORE_OP_STORE;

    SituationRenderPassInfo stencil_store_only = base;
    stencil_store_only.stencil_attachment.storeOp = SIT_STORE_OP_STORE;

    uint32_t key_base = SituationRenderPassConfigurationKey(&base);
    SIT_ASSERT(key_base != SituationRenderPassConfigurationKey(&stencil_clear));
    SIT_ASSERT(key_base != SituationRenderPassConfigurationKey(&stencil_store_only));
    SIT_ASSERT(SituationRenderPassConfigurationKey(&stencil_clear) !=
               SituationRenderPassConfigurationKey(&stencil_store_only));

    SituationRenderPassInfo vd = base;
    vd.display_id = 0;
    SIT_ASSERT(key_base != SituationRenderPassConfigurationKey(&vd));
}

static void test_render_pass_info_default_begin_pass(void) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp = SituationRenderPassInfoDefault(-1, (ColorRGBA){0, 0, 0, 255});
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);
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
//  Module-to-Core Assignment (real library threads)
// ============================================================================

#ifdef SITUATION_ENABLE_THREADING
static void test_module_core_assignment(void) {
    /*
     * Reports the actual Situation library thread layout after all subsystems
     * are warmed up: workers, I/O, audio, and render (if enabled).
     */
    SituationThreadPool* pool = SituationGetInternalThreadPool();
    SIT_ASSERT(pool != NULL);

    // Trigger audio thread by playing a silent tone
    SituationPlayTone(SIT_WAVE_SINE, 440.0f, 0.0f, 0.001f, 0.0f, 0.0f, 0.001f, 0.05f);

    // Pump a few frames so audio callback fires
    for (int i = 0; i < 10; ++i) {
        SituationPollInputEvents();
        SituationUpdateTimers();
#if defined(_WIN32)
        Sleep(20);
#else
        usleep(20000);
#endif
    }

    SituationStopAllTones();

    fprintf(stderr, "\n=== Situation Module-to-Core Assignment ===\n");
    fprintf(stderr, "Build flags:\n");
    fprintf(stderr, "  SITUATION_ENABLE_THREADING:           YES\n");
    fprintf(stderr, "  SITUATION_WORKER_NUMA_SPREAD_DEFAULT: %d\n", SITUATION_WORKER_NUMA_SPREAD_DEFAULT);
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    fprintf(stderr, "  SITUATION_ENABLE_RENDER_THREAD:       YES\n");
#else
    fprintf(stderr, "  SITUATION_ENABLE_RENDER_THREAD:       NO\n");
#endif
    fprintf(stderr, "\n");

    // Full threading report from the library
    SituationDumpThreadingReport(pool, stderr, false);

    // Snapshot for assertions
    SituationThreadPoolSnapshot snap;
    SIT_ASSERT(SituationGetThreadPoolSnapshot(pool, &snap));
    SIT_ASSERT(snap.pool_active);
    SIT_ASSERT(snap.worker_count >= 1);
    SIT_ASSERT(snap.io_thread_enabled);

    // Check that audio thread registered
    bool saw_audio = false;
    int distinct_cpus = 0;
    bool cpu_seen[256];
    memset(cpu_seen, 0, sizeof(cpu_seen));

    for (int s = 0; s < snap.slot_count; ++s) {
        if (snap.slots[s].role == SIT_THREAD_ROLE_AUDIO) saw_audio = true;
        int cpu = snap.slots[s].last_logical_cpu;
        if (cpu >= 0 && cpu < 256 && !cpu_seen[cpu]) {
            cpu_seen[cpu] = true;
            distinct_cpus++;
        }
    }

    fprintf(stderr, "  Distinct CPUs across all roles: %d\n", distinct_cpus);
    fprintf(stderr, "  Audio thread visible: %s\n", saw_audio ? "YES" : "NO");
    fprintf(stderr, "=== End ===\n\n");

    SIT_ASSERT(saw_audio);
    SIT_ASSERT(distinct_cpus >= 2);
}
#endif

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase core_tests[] = {
    // Version & state
    {"version_string",          test_version_string,        true},
    {"is_initialized",          test_is_initialized,        true},
    {"init_state_ready",        test_init_success_sets_ready_state, true},
    {"init_double_init_error",  test_init_double_init_reports_error, true},
    {"errno_table_phase_2_1",   test_errno_table_phase_2_1, true},
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
    {"get_graphics_backend",    test_get_graphics_backend,  true},
    {"get_graphics_caps",       test_get_graphics_caps,     true},
    {"viewport_index_zero_parity", test_viewport_index_zero_parity, true},
    {"viewport_index_out_of_range", test_viewport_index_out_of_range, true},
    {"render_pass_info_default_helper", test_render_pass_info_default_helper, true},
    {"render_pass_info_load_helper", test_render_pass_info_load_helper, true},
    {"render_pass_configuration_key_stencil_ops", test_render_pass_configuration_key_stencil_ops, true},
    {"render_pass_info_default_begin_pass", test_render_pass_info_default_begin_pass, true},
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
#ifdef SITUATION_ENABLE_THREADING
    {"module_core_assignment",      test_module_core_assignment,        true},
#endif
};

const SitTestModule g_module_core = {
    .name = "core",
    .setup = core_setup,
    .teardown = core_teardown,
    .tests = core_tests,
    .test_count = sizeof(core_tests) / sizeof(core_tests[0]),
    .requires_context = true
};
