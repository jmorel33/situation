/**
 * @file test_frame_profile.c
 * @brief P10.1 SituationGetFrameProfile snapshot tests.
 *
 * Run: sit_test.exe --module frame_profile [--filter substr]
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include <math.h>
#include <string.h>

static bool g_fp_init_ok = false;

static void fp_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_FRAME_PROFILE");
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    config.render_thread_count = 1;
#endif
    SituationError err = SituationInit(0, NULL, &config);
    g_fp_init_ok = (err == SITUATION_SUCCESS);
    if (!g_fp_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void fp_teardown(void) {
    if (g_fp_init_ok) {
        SituationShutdown();
        g_fp_init_ok = false;
    }
}

static void fp_run_frames(int count) {
    for (int i = 0; i < count; ++i) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
        SIT_ASSERT(SituationEndFrame() == SITUATION_SUCCESS);
    }
}

static void fp_assert_profile_matches_getters(const SituationFrameProfile* profile) {
    SIT_ASSERT_EQ(profile->struct_version, (uint32_t)SITUATION_FRAME_PROFILE_VERSION);
    SIT_ASSERT_EQ(profile->struct_size, (uint32_t)sizeof(SituationFrameProfile));

    double frame_ms = (double)SituationGetFrameTime() * 1000.0;
    SIT_ASSERT(fabs(profile->frame_time_ms - frame_ms) < 0.001);

    double max_ms = SituationGetMaxFrameTime() * 1000.0;
    SIT_ASSERT(fabs(profile->max_frame_time_ms - max_ms) < 0.001);

    SIT_ASSERT_EQ(profile->spike_count, SituationGetFrameSpikeCount());

    uint64_t bp = 0, fence = 0, exec = 0, pres = 0;
    SituationGetLastFramePhases(&bp, &fence, &exec, &pres);
    SIT_ASSERT_EQ(profile->backpressure_ns, bp);
    SIT_ASSERT_EQ(profile->fence_wait_ns, fence);
    SIT_ASSERT_EQ(profile->execute_ns, exec);
    SIT_ASSERT_EQ(profile->present_ns, pres);

#if defined(SITUATION_ENABLE_RENDER_THREAD)
    uint64_t avg = 0, max_lat = 0;
    SituationGetRenderLatencyStats(&avg, &max_lat);
    SIT_ASSERT_EQ(profile->render_latency_avg_ns, avg);
    SIT_ASSERT_EQ(profile->render_latency_max_ns, max_lat);
    SIT_ASSERT_EQ(profile->queue_depth, SituationGetRenderQueueDepth());
#endif

    if (!SituationIsFeatureSupported(SIT_FEATURE_GPU_TIMESTAMPS)) {
        for (uint32_t z = 0; z < SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT; ++z) {
            SIT_ASSERT_EQ(profile->gpu_zone_ns[z], 0ull);
        }
    }
}

static void test_user_gpu_zone_reports_elapsed(void) {
    if (!SituationIsFeatureSupported(SIT_FEATURE_GPU_TIMESTAMPS)) {
        return;
    }
#if defined(SITUATION_USE_VULKAN)
    /* Vulkan: user GPU zones must be recorded outside an active render pass (P10.3). */
    return;
#endif

    for (int i = 0; i < 10; ++i) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SIT_ASSERT(cmd != NULL);

        SituationRenderPassInfo rp = {0};
        rp.display_id = -1;
        rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        rp.color_attachment.clear.color = (ColorRGBA){32, 32, 32, 255};
        rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.depth_attachment.clear.depth = 1.0f;

        SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);
        SIT_ASSERT_EQ(SituationCmdGPUZoneBegin(cmd, (uint32_t)SITUATION_GPU_ZONE_USER_0), SITUATION_SUCCESS);
        SIT_ASSERT_EQ(SituationCmdClearColor(cmd, (ColorRGBA){64, 64, 64, 255}), SITUATION_SUCCESS);
        SIT_ASSERT_EQ(SituationCmdGPUZoneEnd(cmd, (uint32_t)SITUATION_GPU_ZONE_USER_0), SITUATION_SUCCESS);
        SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
        SIT_ASSERT(SituationEndFrame() == SITUATION_SUCCESS);
    }

    /* GPU query readback is one frame slot late (after fence on reuse). */
    fp_run_frames(SITUATION_MAX_FRAMES_IN_FLIGHT + 2);

    SituationFrameProfile profile = {0};
    SituationGetFrameProfile(&profile);
    SIT_ASSERT(profile.gpu_zone_ns[SITUATION_GPU_ZONE_USER_0] > 0ull);
}

static void test_gpu_zone_overflow_rejected(void) {
    if (!SituationIsFeatureSupported(SIT_FEATURE_GPU_TIMESTAMPS)) {
        return;
    }
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT(cmd != NULL);
    SIT_ASSERT_EQ(SituationCmdGPUZoneBegin(cmd, SITUATION_FRAME_PROFILE_GPU_ZONE_COUNT),
                  SITUATION_ERROR_PROFILING_ZONE_OVERFLOW);
    SIT_ASSERT(SituationEndFrame() == SITUATION_SUCCESS);
}

static void test_get_frame_profile_matches_getters(void) {
    fp_run_frames(5);

    SituationFrameProfile profile = {0};
    SituationGetFrameProfile(&profile);
    fp_assert_profile_matches_getters(&profile);
}

static void test_reset_frame_profile_stats(void) {
    fp_run_frames(3);

    SituationResetFrameProfileStats();

    SIT_ASSERT_EQ(SituationGetFrameSpikeCount(), 0u);
    SIT_ASSERT(SituationGetMaxFrameTime() == 0.0);

    SituationFrameProfile profile = {0};
    SituationGetFrameProfile(&profile);
    SIT_ASSERT_EQ(profile.spike_count, 0u);
    SIT_ASSERT(profile.max_frame_time_ms == 0.0);
    fp_assert_profile_matches_getters(&profile);
}

static SitTestCase g_frame_profile_tests[] = {
    {"get_frame_profile_matches_getters", test_get_frame_profile_matches_getters, true},
    {"reset_frame_profile_stats", test_reset_frame_profile_stats, true},
    {"user_gpu_zone_reports_elapsed", test_user_gpu_zone_reports_elapsed, true},
    {"gpu_zone_overflow_rejected", test_gpu_zone_overflow_rejected, true},
};

const SitTestModule g_module_frame_profile = {
    .name = "frame_profile",
    .setup = fp_setup,
    .teardown = fp_teardown,
    .tests = g_frame_profile_tests,
    .test_count = sizeof(g_frame_profile_tests) / sizeof(g_frame_profile_tests[0]),
    .requires_context = true
};
