/**
 * @file test_query_pool.c
 * @brief P10.4 user SituationQueryPool tests.
 *
 * Run: sit_test.exe --module query_pool [--filter substr]
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include <string.h>

static bool g_qp_init_ok = false;

static void qp_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_QUERY_POOL");
    config.render_thread_count = 0;
    SituationError err = SituationInit(0, NULL, &config);
    g_qp_init_ok = (err == SITUATION_SUCCESS);
    if (!g_qp_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void qp_teardown(void) {
    if (g_qp_init_ok) {
        SituationShutdown();
        g_qp_init_ok = false;
    }
}

static void qp_run_frames(int count) {
    for (int i = 0; i < count; ++i) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
        SIT_ASSERT(SituationEndFrame() == SITUATION_SUCCESS);
    }
}

static void test_create_destroy_query_pool(void) {
    SituationQueryPool ts_pool = SITUATION_NULL_QUERY_POOL;
    SituationQueryPool occ_pool = SITUATION_NULL_QUERY_POOL;

    SituationError ts_err = SituationCreateQueryPool(SITUATION_QUERY_TYPE_TIMESTAMP, 4, &ts_pool);
    if (ts_err == SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED) {
        SIT_ASSERT_EQ(ts_pool.generation, 0u);
    } else {
        SIT_ASSERT_EQ(ts_err, SITUATION_SUCCESS);
        SIT_ASSERT(ts_pool.generation != 0u);
    }

    SIT_ASSERT_EQ(SituationCreateQueryPool(SITUATION_QUERY_TYPE_OCCLUSION, 2, &occ_pool), SITUATION_SUCCESS);
    SIT_ASSERT(occ_pool.generation != 0u);

    SituationDestroyQueryPool(&ts_pool);
    SituationDestroyQueryPool(&occ_pool);
    SIT_ASSERT_EQ(ts_pool.generation, 0u);
    SIT_ASSERT_EQ(occ_pool.generation, 0u);
}

static void test_query_timestamp_monotonic(void) {
    SituationQueryPool pool = SITUATION_NULL_QUERY_POOL;
    SituationError cr = SituationCreateQueryPool(SITUATION_QUERY_TYPE_TIMESTAMP, 2, &pool);
    if (cr == SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED) {
        return;
    }
    SIT_ASSERT_EQ(cr, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT(cmd != NULL);

    SIT_ASSERT_EQ(SituationCmdResetQueryPool(cmd, pool, 0, 2), SITUATION_SUCCESS);

#if defined(SITUATION_USE_VULKAN)
    SIT_ASSERT_EQ(SituationCmdWriteTimestamp(cmd, SITUATION_PIPELINE_STAGE_TOP, pool, 0), SITUATION_SUCCESS);
#endif

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);

#if defined(SITUATION_USE_OPENGL)
    SIT_ASSERT_EQ(SituationCmdWriteTimestamp(cmd, SITUATION_PIPELINE_STAGE_TOP, pool, 0), SITUATION_SUCCESS);
#endif

    SIT_ASSERT_EQ(SituationCmdClearColor(cmd, (ColorRGBA){64, 64, 64, 255}), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdWriteTimestamp(cmd, SITUATION_PIPELINE_STAGE_BOTTOM, pool, 1), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT(SituationEndFrame() == SITUATION_SUCCESS);

    qp_run_frames(SITUATION_MAX_FRAMES_IN_FLIGHT + 2);

    uint64_t results[2] = {0, 0};
    SIT_ASSERT_EQ(SituationGetQueryPoolResults(pool, 0, 2, results, SITUATION_QUERY_RESULT_WAIT_BIT), SITUATION_SUCCESS);
    SIT_ASSERT(results[1] > results[0]);

    SituationDestroyQueryPool(&pool);
}

static void test_occlusion_query_visible_vs_clipped(void) {
    SituationQueryPool pool = SITUATION_NULL_QUERY_POOL;
    SIT_ASSERT_EQ(SituationCreateQueryPool(SITUATION_QUERY_TYPE_OCCLUSION, 2, &pool), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdResetQueryPool(cmd, pool, 0, 2), SITUATION_SUCCESS);

    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){(float)sit_test_window_width() * 0.5f, (float)sit_test_window_height() * 0.5f, 0.0f});
    glm_scale(model, (vec3){40.0f, 40.0f, 1.0f});
    Vector4 white = {{1.0f, 1.0f, 1.0f, 1.0f}};

    SIT_ASSERT_EQ(SituationCmdBeginOcclusionQuery(cmd, pool, 0), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdDrawQuad(cmd, model, white), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndOcclusionQuery(cmd), SITUATION_SUCCESS);

    SIT_ASSERT_EQ(SituationCmdBeginOcclusionQuery(cmd, pool, 1), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndOcclusionQuery(cmd), SITUATION_SUCCESS);

    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT(SituationEndFrame() == SITUATION_SUCCESS);

    qp_run_frames(SITUATION_MAX_FRAMES_IN_FLIGHT + 2);

    uint64_t results[2] = {0, 0};
    SIT_ASSERT_EQ(SituationGetQueryPoolResults(pool, 0, 2, results, SITUATION_QUERY_RESULT_WAIT_BIT), SITUATION_SUCCESS);
    SIT_ASSERT(results[0] > 0ull);
    SIT_ASSERT_EQ(results[1], 0ull);

    SituationDestroyQueryPool(&pool);
}

static SitTestCase g_query_pool_tests[] = {
    {"create_destroy_query_pool", test_create_destroy_query_pool, true},
    {"query_timestamp_monotonic", test_query_timestamp_monotonic, true},
    {"occlusion_query_visible_vs_clipped", test_occlusion_query_visible_vs_clipped, true},
};

const SitTestModule g_module_query_pool = {
    .name = "query_pool",
    .setup = qp_setup,
    .teardown = qp_teardown,
    .tests = g_query_pool_tests,
    .test_count = sizeof(g_query_pool_tests) / sizeof(g_query_pool_tests[0]),
    .requires_context = true
};
