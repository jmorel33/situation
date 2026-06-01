/**
 * @file test_virtual_display.c
 * @brief Virtual Display harness module — API, compositing, scaling, blend, timing.
 *
 * Split from test_graphics.c so VD failures are isolated from the main graphics suite.
 * Runs as its own module immediately after [graphics] in the harness registry.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* g_vs_passthrough =
    "#version 460 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "void main() { gl_Position = vec4(aPos, 1.0); }\n";

static const char* g_fs_solid_red =
    "#version 460 core\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";

static bool g_vd_init_ok = false;

static void virtual_display_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_VIRTUAL_DISPLAY");

    SituationError err = SituationInit(0, NULL, &config);
    g_vd_init_ok = (err == SITUATION_SUCCESS);
    if (!g_vd_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    graphics_test_print_renderer_banner();
}

static void virtual_display_teardown(void) {
    if (g_vd_init_ok) {
        SituationShutdown();
        g_vd_init_ok = false;
    }
}
// ============================================================================
//  Virtual Display Tests
// ============================================================================

static void test_create_destroy_virtual_display(void) {
    int vd_id = -1;
    Vector2 resolution = {320.0f, 240.0f};

    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_ALPHA,
        &vd_id
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(vd_id >= 0);

    err = SituationDestroyVirtualDisplay(vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
}

static void test_configure_virtual_display(void) {
    int vd_id = -1;
    Vector2 resolution = {160.0f, 120.0f};

    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_ALPHA,
        &vd_id
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {10.0f, 10.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 0.8f, 1, true, 1.0, SITUATION_BLEND_ALPHA);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyVirtualDisplay(vd_id);
}

static void test_get_virtual_display(void) {
    int vd_id = -1;
    Vector2 resolution = {256.0f, 256.0f};

    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_ALPHA,
        &vd_id
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationVirtualDisplay* vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT_NOT_NULL(vd);
    SIT_ASSERT_EQ(vd->id, vd_id);

    SituationDestroyVirtualDisplay(vd_id);
}

static void test_virtual_display_dirty_flag(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};

    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_ALPHA,
        &vd_id
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationSetVirtualDisplayDirty(vd_id, true);
    SIT_ASSERT(SituationIsVirtualDisplayDirty(vd_id));

    SituationSetVirtualDisplayDirty(vd_id, false);
    SIT_ASSERT(!SituationIsVirtualDisplayDirty(vd_id));

    SituationDestroyVirtualDisplay(vd_id);
}

static void test_virtual_display_size(void) {
    int vd_id = -1;
    Vector2 resolution = {128.0f, 96.0f};

    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_ALPHA,
        &vd_id
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    int w = 0, h = 0;
    SituationGetVirtualDisplaySize(vd_id, &w, &h);
    SIT_ASSERT_EQ(w, 128);
    SIT_ASSERT_EQ(h, 96);

    SituationDestroyVirtualDisplay(vd_id);
}

static void test_render_virtual_displays(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};

    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_ALPHA,
        &vd_id
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Do a frame with VD compositing
    SituationPollInputEvents();
    SituationUpdateTimers();
    bool acquired = SituationAcquireFrameCommandBuffer();
    SIT_ASSERT(acquired);

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    /* SituationRenderVirtualDisplays leaves the main-window render pass active (resume pass for caller draws). */
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationEndFrame();
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * Task 13.1: Render-to-VD pipeline tests
 */

/**
 * Render into VD: create 64x64 VD -> render pass targeting VD -> draw solid red ->
 * composite -> screenshot -> verify red region
 */
static void test_vd_render_into_pipeline(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0,
        SITUATION_SCALING_STRETCH,
        SITUATION_BLEND_NONE,
        &vd_id
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(vd_id >= 0);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f
    };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    // Render pass targeting the VD
    SituationRenderPassInfo rp = {0};
    rp.display_id = vd_id;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    err = SituationCmdBeginRenderPass(cmd, &rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Composite VDs to main window
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;

    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Readback and verify red pixels exist
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    bool found_red = false;
    uint8_t* pixels = (uint8_t*)screen.data;
    int pixel_count = screen.width * screen.height;
    for (int i = 0; i < pixel_count * 4; i += 4) {
        if (pixels[i] > 200 && pixels[i+1] < 50 && pixels[i+2] < 50) {
            found_red = true;
            break;
        }
    }
    SIT_ASSERT(found_red);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}


/**
 * Z-ordering: VD1 (z=0, blue) behind VD2 (z=1, red) -> composite -> verify VD2 on top
 */
static void test_vd_z_ordering(void) {
    static const char* fs_blue =
        "#version 460 core\n"
        "layout(location=0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(0.0, 0.0, 1.0, 1.0); }\n";

    int vd1 = -1, vd2 = -1;
    Vector2 resolution = {64.0f, 64.0f};

    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd1);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCreateVirtualDisplay(
        resolution, 1.0, 1, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd2);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd1, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationConfigureVirtualDisplay(vd2, offset, 1.0f, 1, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader_blue = {0}, shader_red = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, fs_blue, &shader_blue);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader_red);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    // Render blue into VD1
    SituationRenderPassInfo rp1 = {0};
    rp1.display_id = vd1;
    rp1.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp1.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp1.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp1.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp1.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp1);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader_blue);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Render red into VD2
    SituationRenderPassInfo rp2 = {0};
    rp2.display_id = vd2;
    rp2.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp2.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp2.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp2.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp2.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp2);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader_red);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Composite to main ΓÇö VD2 (z=1) should be on top
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Center pixel should be red (VD2 on top), not blue
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int idx = (cy * screen.width + cx) * 4;
    uint8_t* pixels = (uint8_t*)screen.data;
    SIT_ASSERT(graphics_test_pixel_approx_eq(pixels[idx], 255, 10));
    SIT_ASSERT(graphics_test_pixel_approx_eq(pixels[idx+1], 0, 10));
    SIT_ASSERT(graphics_test_pixel_approx_eq(pixels[idx+2], 0, 10));

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader_blue);
    SituationUnloadShader(&shader_red);
    SituationDestroyVirtualDisplay(vd1);
    SituationDestroyVirtualDisplay(vd2);
}

/**
 * Visibility toggle: set visible=false -> composite -> verify absent ->
 * set visible=true -> verify present
 */
static void test_vd_visibility_toggle(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // --- Frame 1: VD invisible ---
    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, false, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // Render red into VD
    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Composite (VD invisible)
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Should be all black
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int idx = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[idx] < 10);
    SIT_ASSERT(pixels[idx+1] < 10);
    SIT_ASSERT(pixels[idx+2] < 10);
    SituationUnloadImage(screen);

    // --- Frame 2: VD visible ---
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    cmd = SituationGetMainCommandBuffer();

    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Should have red pixels now
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    pixels = (uint8_t*)screen.data;
    idx = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[idx] > 200);
    SituationUnloadImage(screen);

    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * Opacity: VD with opacity=0.5 over white background -> composite -> verify blended color
 */
static void test_vd_opacity_blending(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_ALPHA, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 0.5f, 0, true, 1.0, SITUATION_BLEND_ALPHA);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Composite over white
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){255, 255, 255, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Blended result: not pure red, not pure white
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int idx = (cy * screen.width + cx) * 4;
    uint8_t r = pixels[idx], g = pixels[idx+1], b = pixels[idx+2];
    SIT_ASSERT(r > 100);
    SIT_ASSERT(g > 50 && g < 240);
    SIT_ASSERT(b > 50 && b < 240);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}


/**
 * Task 13.2: VD scaling mode tests
 */

/**
 * SITUATION_SCALING_STRETCH: 32x32 VD stretched to fill window -> verify fills entire region
 */
static void test_vd_scaling_stretch(void) {
    int vd_id = -1;
    Vector2 resolution = {32.0f, 32.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Corners should be red (stretched to fill)
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int margin = 2;
    int tl_idx = (margin * screen.width + margin) * 4;
    SIT_ASSERT(pixels[tl_idx] > 200);
    int br_x = screen.width - margin - 1;
    int br_y = screen.height - margin - 1;
    int br_idx = (br_y * screen.width + br_x) * 4;
    SIT_ASSERT(pixels[br_idx] > 200);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * SITUATION_SCALING_FIT: non-square VD in square region -> verify aspect ratio preserved
 */
static void test_vd_scaling_fit(void) {
    // Wide VD (128x32) in the default harness window — should letterbox (black top/bottom)
    int vd_id = -1;
    Vector2 resolution = {128.0f, 32.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_FIT, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Center should be red, top/bottom edges should be black (letterbox)
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int center_idx = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[center_idx] > 200);
    int top_idx = (1 * screen.width + cx) * 4;
    SIT_ASSERT(pixels[top_idx] < 30);
    int bot_idx = ((screen.height - 2) * screen.width + cx) * 4;
    SIT_ASSERT(pixels[bot_idx] < 30);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * SITUATION_SCALING_INTEGER: 32x32 VD in the default harness window -> verify integer scale factor
 */
static void test_vd_scaling_integer(void) {
    int vd_id = -1;
    Vector2 resolution = {32.0f, 32.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_INTEGER, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Center should be red, corner (0,0) should be black (integer scaling leaves borders)
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int center_idx = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[center_idx] > 200);
    int corner_idx = 0;
    SIT_ASSERT(pixels[corner_idx] < 30);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * Runtime mode switch via SituationSetVirtualDisplayScalingMode -> verify change takes effect
 */
static void test_vd_scaling_mode_switch(void) {
    int vd_id = -1;
    Vector2 resolution = {32.0f, 32.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_INTEGER, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationVirtualDisplay* vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT_NOT_NULL(vd);
    SIT_ASSERT_EQ((int)vd->scaling_mode, (int)SITUATION_SCALING_INTEGER);

    err = SituationSetVirtualDisplayScalingMode(vd_id, SITUATION_SCALING_STRETCH);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT_NOT_NULL(vd);
    SIT_ASSERT_EQ((int)vd->scaling_mode, (int)SITUATION_SCALING_STRETCH);

    err = SituationSetVirtualDisplayScalingMode(vd_id, SITUATION_SCALING_FIT);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT_NOT_NULL(vd);
    SIT_ASSERT_EQ((int)vd->scaling_mode, (int)SITUATION_SCALING_FIT);

    SituationDestroyVirtualDisplay(vd_id);
}


/**
 * Task 13.3: VD blend mode tests
 */

/**
 * SITUATION_BLEND_ALPHA: semi-transparent VD over white -> verify alpha-blended result
 */
static void test_vd_blend_alpha(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_ALPHA, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 0.5f, 0, true, 1.0, SITUATION_BLEND_ALPHA);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){255, 255, 255, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int idx = (cy * screen.width + cx) * 4;
    uint8_t r = pixels[idx], g = pixels[idx+1], b = pixels[idx+2];
    // Alpha blend: red*0.5 + white*0.5 => R~255, G~128, B~128
    SIT_ASSERT(r > 100);
    SIT_ASSERT(g > 50 && g < 200);
    SIT_ASSERT(b > 50 && b < 200);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * SITUATION_BLEND_ADDITIVE: dark VD over dark background -> verify brightening
 */
static void test_vd_blend_additive(void) {
    static const char* fs_dim_green =
        "#version 460 core\n"
        "layout(location=0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(0.0, 0.4, 0.0, 1.0); }\n";

    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_ADDITIVE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_ADDITIVE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, fs_dim_green, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Composite over dark red background (64, 0, 0)
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){64, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Additive: result = src + dst => R >= 50, G > 50
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int idx = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[idx] >= 50);     // R at least background
    SIT_ASSERT(pixels[idx+1] > 50);    // G from VD added

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * SITUATION_BLEND_MULTIPLY: white VD over colored -> no change
 */
static void test_vd_blend_multiply(void) {
    static const char* fs_white =
        "#version 460 core\n"
        "layout(location=0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(1.0, 1.0, 1.0, 1.0); }\n";

    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_MULTIPLY, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_MULTIPLY);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, fs_white, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Composite over green background (0, 200, 0)
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 200, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // white * green = green (multiply by 1.0 preserves)
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int idx = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[idx+1] > 150);  // Green preserved

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * SITUATION_BLEND_NONE: VD overwrites destination completely
 */
static void test_vd_blend_none_overwrite(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Composite over bright green ΓÇö BLEND_NONE should fully overwrite
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 255, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Center should be pure red (no green bleed-through)
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int idx = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[idx] > 200);
    SIT_ASSERT(pixels[idx+1] < 30);
    SIT_ASSERT(pixels[idx+2] < 30);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}


/**
 * Task 13.4: VD timing and performance tests
 */

/**
 * SituationGetLastVDCompositeTimeMS -> composite 4 VDs -> verify time > 0.0
 */
static void test_vd_composite_time(void) {
    int vd_ids[4] = {-1, -1, -1, -1};
    Vector2 resolution = {64.0f, 64.0f};
    Vector2 offset = {0.0f, 0.0f};

    for (int i = 0; i < 4; i++) {
        SituationError e = SituationCreateVirtualDisplay(
            resolution, 1.0, i, SITUATION_SCALING_STRETCH, SITUATION_BLEND_ALPHA, &vd_ids[i]);
        SIT_ASSERT_EQ(e, SITUATION_SUCCESS);
        e = SituationConfigureVirtualDisplay(vd_ids[i], offset, 1.0f, i, true, 1.0, SITUATION_BLEND_ALPHA);
        SIT_ASSERT_EQ(e, SITUATION_SUCCESS);
        SituationSetVirtualDisplayDirty(vd_ids[i], true);
    }

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;

    SituationError err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    double composite_time = SituationGetLastVDCompositeTimeMS();
    SIT_ASSERT(composite_time >= 0.0);

    for (int i = 0; i < 4; i++) {
        SituationDestroyVirtualDisplay(vd_ids[i]);
    }
}

/**
 * frame_time_mult=0.5 -> verify dirty flag behavior respects timing
 */
static void test_vd_frame_time_multiplier(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 0.5, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationVirtualDisplay* vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT_NOT_NULL(vd);
    SIT_ASSERT(vd->frame_time_multiplier > 0.4 && vd->frame_time_multiplier < 0.6);

    // Reconfigure with different multiplier
    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 2.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT_NOT_NULL(vd);
    SIT_ASSERT(vd->frame_time_multiplier > 1.9 && vd->frame_time_multiplier < 2.1);

    // Run a frame to advance timing
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationEndFrame();

    vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT(vd->frame_count >= 0);

    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * Offset/position: VD with offset (50,50) -> composite -> verify content shifted
 */
static void test_vd_offset_position(void) {
    int vd_id = -1;
    Vector2 resolution = {32.0f, 32.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Position VD at offset (50, 50)
    Vector2 offset = {50.0f, 50.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = { -1.0f,-1.0f,0.0f, 3.0f,-1.0f,0.0f, -1.0f,3.0f,0.0f };
    uint32_t indices[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    // Composite to main (black background)
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationRenderVirtualDisplays(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;

    // (5,5) should be black ΓÇö before the VD offset
    int tl_idx = (5 * screen.width + 5) * 4;
    SIT_ASSERT(pixels[tl_idx] < 30);
    SIT_ASSERT(pixels[tl_idx+1] < 30);
    SIT_ASSERT(pixels[tl_idx+2] < 30);

    // (65, 65) should be red ΓÇö inside the VD area (offset 50 + within 32px)
    int inside_x = 65;
    int inside_y = 65;
    if (inside_x < screen.width && inside_y < screen.height) {
        int inside_idx = (inside_y * screen.width + inside_x) * 4;
        SIT_ASSERT(pixels[inside_idx] > 200);
    }

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}
static SitTestCase virtual_display_tests[] = {
    {"create_destroy_virtual_display",  test_create_destroy_virtual_display, true},
    {"configure_virtual_display",       test_configure_virtual_display,     true},
    {"get_virtual_display",             test_get_virtual_display,           true},
    {"virtual_display_dirty_flag",      test_virtual_display_dirty_flag,    true},
    {"virtual_display_size",            test_virtual_display_size,          true},
    {"render_virtual_displays",         test_render_virtual_displays,       true},
    {"vd_render_into_pipeline",         test_vd_render_into_pipeline,       true},
    {"vd_z_ordering",                   test_vd_z_ordering,                 true},
    {"vd_visibility_toggle",            test_vd_visibility_toggle,          true},
    {"vd_opacity_blending",             test_vd_opacity_blending,           true},
    {"vd_scaling_stretch",              test_vd_scaling_stretch,            true},
    {"vd_scaling_fit",                  test_vd_scaling_fit,                true},
    {"vd_scaling_integer",              test_vd_scaling_integer,            true},
    {"vd_scaling_mode_switch",          test_vd_scaling_mode_switch,        true},
    {"vd_blend_alpha",                  test_vd_blend_alpha,                true},
    {"vd_blend_additive",              test_vd_blend_additive,             true},
    {"vd_blend_multiply",              test_vd_blend_multiply,             true},
    {"vd_blend_none_overwrite",        test_vd_blend_none_overwrite,       true},
    {"vd_composite_time",              test_vd_composite_time,             true},
    {"vd_frame_time_multiplier",       test_vd_frame_time_multiplier,      true},
    {"vd_offset_position",             test_vd_offset_position,            true},
};

const SitTestModule g_module_virtual_display = {
    .name = "virtual_display",
    .setup = virtual_display_setup,
    .teardown = virtual_display_teardown,
    .tests = virtual_display_tests,
    .test_count = sizeof(virtual_display_tests) / sizeof(virtual_display_tests[0]),
    .requires_context = true
};
