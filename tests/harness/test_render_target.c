/**
 * @file test_render_target.c
 * @brief User SituationRenderTarget tests (Phase 3c).
 *
 * Run: sit_test.exe --module render_target [--filter substr]
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include <string.h>

static bool g_rt_init_ok = false;

static void rt_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_RENDER_TARGET");
    SituationError err = SituationInit(0, NULL, &config);
    g_rt_init_ok = (err == SITUATION_SUCCESS);
    if (!g_rt_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void rt_teardown(void) {
    if (g_rt_init_ok) {
        SituationShutdown();
        g_rt_init_ok = false;
    }
}

static SituationError rt_texture_barrier(SituationCommandBuffer cmd, SituationTexture texture,
        SituationTextureLayout old_layout, SituationTextureLayout new_layout) {
    SituationTextureBarrierDesc desc = {0};
    desc.old_layout = old_layout;
    desc.new_layout = new_layout;
    desc.base_mip_level = 0;
    desc.mip_level_count = 1;
    return SituationCmdTextureBarrier(cmd, texture, &desc);
}

static void test_create_destroy_render_target(void) {
    SituationRenderTargetDesc desc = {0};
    desc.width = 8;
    desc.height = 8;
    desc.msaa_samples = 1;
    desc.want_depth = true;

    SituationRenderTarget rt = SITUATION_NULL_RENDER_TARGET;
    SIT_ASSERT_EQ(SituationCreateRenderTarget(&desc, &rt), SITUATION_SUCCESS);
    SIT_ASSERT(rt.generation != 0u);

    SituationTexture tex = {0};
    SIT_ASSERT_EQ(SituationGetRenderTargetTexture(rt, &tex), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(tex.width, 8);
    SIT_ASSERT_EQ(tex.height, 8);

    SituationDestroyRenderTarget(&rt);
    SIT_ASSERT_EQ(rt.generation, 0u);
    SIT_ASSERT_EQ(SituationGetRenderTargetTexture(rt, &tex), SITUATION_ERROR_RENDER_TARGET_INVALID);
}

static void test_render_target_read_render_target(void) {
    SituationRenderTargetDesc desc = {0};
    desc.width = 4;
    desc.height = 4;
    desc.msaa_samples = 1;
    desc.want_depth = false;

    SituationRenderTarget rt = SITUATION_NULL_RENDER_TARGET;
    SIT_ASSERT_EQ(SituationCreateRenderTarget(&desc, &rt), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    ColorRGBA green = {0, 255, 0, 255};
    SituationRenderPassInfo rp = SituationRenderPassInfoForRenderTarget(rt, green);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    uint8_t pixels[64] = {0};
    SIT_ASSERT_EQ(SituationReadRenderTarget(rt, NULL, pixels, sizeof(pixels)), SITUATION_SUCCESS);
    for (int i = 0; i < 16; i += 4) {
        SIT_ASSERT_EQ(pixels[i + 0], 0);
        SIT_ASSERT_EQ(pixels[i + 1], 255);
        SIT_ASSERT_EQ(pixels[i + 2], 0);
        SIT_ASSERT_EQ(pixels[i + 3], 255);
    }

    SituationDestroyRenderTarget(&rt);
}

static void test_render_target_cmd_readback(void) {
    SituationRenderTargetDesc desc = {0};
    desc.width = 4;
    desc.height = 4;
    desc.msaa_samples = 1;
    desc.want_depth = false;

    SituationRenderTarget rt = SITUATION_NULL_RENDER_TARGET;
    SIT_ASSERT_EQ(SituationCreateRenderTarget(&desc, &rt), SITUATION_SUCCESS);

    SituationBuffer dst_buf = {0};
    SIT_ASSERT_EQ(SituationCreateReadbackBuffer(64, &dst_buf), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    ColorRGBA blue = {0, 0, 255, 255};
    SituationRenderPassInfo rp = SituationRenderPassInfoForRenderTarget(rt, blue);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);

    SituationTexture tex = {0};
    SIT_ASSERT_EQ(SituationGetRenderTargetTexture(rt, &tex), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(
        rt_texture_barrier(cmd, tex, SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC),
        SITUATION_SUCCESS);

    SituationTextureCopyRegion region = {0};
    region.src_rect = (SituationTextureRect){0, 0, 2, 2};
    SIT_ASSERT_EQ(SituationCmdCopyTextureToBuffer(cmd, tex, &region, dst_buf, 0, 0), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    uint8_t readback[64] = {0};
    SituationReadBuffer(dst_buf, readback, sizeof(readback));
    for (int i = 0; i < 16; i += 4) {
        SIT_ASSERT_EQ(readback[i + 0], 0);
        SIT_ASSERT_EQ(readback[i + 1], 0);
        SIT_ASSERT_EQ(readback[i + 2], 255);
        SIT_ASSERT_EQ(readback[i + 3], 255);
    }

    SituationDestroyBuffer(&dst_buf);
    SituationDestroyRenderTarget(&rt);
}

static SitTestCase rt_tests[] = {
    {"create_destroy_render_target", test_create_destroy_render_target, true},
    {"render_target_read_render_target", test_render_target_read_render_target, true},
    {"render_target_cmd_readback", test_render_target_cmd_readback, true},
};

const SitTestModule g_module_render_target = {
    .name = "render_target",
    .setup = rt_setup,
    .teardown = rt_teardown,
    .tests = rt_tests,
    .test_count = sizeof(rt_tests) / sizeof(rt_tests[0]),
    .requires_context = true
};
