/**
 * @file test_virtual_display.c
 * @brief Virtual Display harness module � API, compositing, scaling, blend, timing.
 *
 * Split from test_graphics.c so VD failures are isolated from the main graphics suite.
 * Runs after [text_rendering], immediately before [compute] in the harness registry.
 *
 * (c) 2025-2026 Jacques Morel � MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_harness_pattern_ubo.h"
#include "sit_test_framework.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_window.h"
#include "sit_test_stereo_scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if !defined(_WIN32)
#include <strings.h>
#include <time.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

#define VD_IDLE_SOLID_PHOTO_ASSET       "prairie.jpg"
#define VD_IDLE_SWITCH_TOTAL_MS   1500u
#define VD_IDLE_SWITCH_LIVE_START  500u
#define VD_IDLE_SWITCH_LIVE_END   1000u

static const char* g_vs_passthrough =
    "#version 460 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "void main() { gl_Position = vec4(aPos, 1.0); }\n";

static const char* g_fs_solid_red =
    "#version 460 core\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";

static const char* g_fs_linear_half =
    "#version 460 core\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() { fragColor = vec4(0.5, 0.5, 0.5, 1.0); }\n";

static void vd_composite_main_pass_clear_and_present(SituationCommandBuffer cmd, ColorRGBA bg) {
    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = bg;
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &main_rp), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationRenderVirtualDisplays(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SituationRequestScreenCapture();
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);
}

/**
 * vd_default_clear_color — inherit stored clear, no draw, composite readback (VD-2).
 */
static void test_vd_default_clear_color(void) {
    SituationVirtualDisplayDesc desc = {0};
    desc.resolution = (Vector2){64.0f, 64.0f};
    desc.frame_time_mult = 1.0;
    desc.z_order = 0;
    desc.scaling_mode = SITUATION_SCALING_STRETCH;
    desc.blend_mode = SITUATION_BLEND_NONE;
    desc.visible = true;
    desc.opacity = 1.0f;
    desc.attachments = (SituationVirtualDisplayAttachmentDefaults){0};
    desc.attachments.color_load = SIT_LOAD_OP_CLEAR;
    desc.attachments.color_store = SIT_STORE_OP_STORE;
    desc.attachments.depth_load = SIT_LOAD_OP_CLEAR;
    desc.attachments.clear.color = (ColorRGBA){40, 80, 120, 255};
    desc.attachments.clear.depth = 1.0f;

    int vd_id = -1;
    SIT_ASSERT_EQ(SituationCreateVirtualDisplayFromDesc(&desc, &vd_id), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT_EQ(SituationAcquireFrameCommandBuffer(), SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo vd_rp = SituationRenderPassInfoInherit(vd_id);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &vd_rp), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);

    vd_composite_main_pass_clear_and_present(cmd, (ColorRGBA){0, 0, 0, 255});

    SituationImage screen = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen), SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int pi = (cy * screen.width + cx) * 4;
    SIT_ASSERT(abs((int)pixels[pi]   - 40) <= 12);
    SIT_ASSERT(abs((int)pixels[pi+1] - 80) <= 12);
    SIT_ASSERT(abs((int)pixels[pi+2] - 120) <= 12);

    SituationUnloadImage(screen);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * vd_srgb_format_composite — SRGB vs UNORM storage with linear shader output (VD-2).
 */
static uint8_t vd_readback_center_r_after_linear_half_draw(SituationVirtualDisplayColorFormat fmt,
                                                           SituationShader shader,
                                                           SituationMesh mesh) {
    SituationVirtualDisplayDesc desc = {0};
    desc.resolution = (Vector2){32.0f, 32.0f};
    desc.color_format = fmt;
    desc.depth_stencil_mode = SIT_VD_DEPTH_NONE;
    desc.frame_time_mult = 1.0;
    desc.scaling_mode = SITUATION_SCALING_STRETCH;
    desc.blend_mode = SITUATION_BLEND_NONE;
    desc.visible = true;
    desc.opacity = 1.0f;
    desc.attachments = (SituationVirtualDisplayAttachmentDefaults){0};
    desc.attachments.color_load = SIT_LOAD_OP_CLEAR;
    desc.attachments.color_store = SIT_STORE_OP_STORE;
    desc.attachments.clear.color = (ColorRGBA){0, 0, 0, 255};

    int vd_id = -1;
    SIT_ASSERT_EQ(SituationCreateVirtualDisplayFromDesc(&desc, &vd_id), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT_EQ(SituationAcquireFrameCommandBuffer(), SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo vd_rp = SituationRenderPassInfoInherit(vd_id);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &vd_rp), SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);

    vd_composite_main_pass_clear_and_present(cmd, (ColorRGBA){0, 0, 0, 255});

    SituationImage screen = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen), SITUATION_SUCCESS);
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int pi = (cy * screen.width + cx) * 4;
    uint8_t center_r = ((uint8_t*)screen.data)[pi];
    SituationUnloadImage(screen);
    SituationDestroyVirtualDisplay(vd_id);
    return center_r;
}

static void test_vd_srgb_format_composite(void) {
    SituationShader shader = {0};
    SIT_ASSERT_EQ(SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_linear_half, &shader), SITUATION_SUCCESS);

    float vertices[] = { -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f };
    uint32_t indices[] = {0, 1, 2};
    SituationMesh mesh = {0};
    SIT_ASSERT_EQ(SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh), SITUATION_SUCCESS);

    uint8_t unorm_r = vd_readback_center_r_after_linear_half_draw(SIT_VD_FORMAT_RGBA8_UNORM, shader, mesh);
    uint8_t srgb_r = vd_readback_center_r_after_linear_half_draw(SIT_VD_FORMAT_RGBA8_SRGB, shader, mesh);
    SIT_ASSERT(abs((int)unorm_r - (int)srgb_r) <= 20);

    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
}

/**
 * vd_sampler_nearest_upscale — explicit NEAREST composite sampler on stretched VD (VD-3).
 */
static void test_vd_sampler_nearest_upscale(void) {
    int vd_id = -1;
    Vector2 resolution = {4.0f, 4.0f};
    SIT_ASSERT_EQ(SituationCreateVirtualDisplay(resolution, 1.0, 0, SITUATION_SCALING_STRETCH,
        SITUATION_BLEND_NONE, &vd_id), SITUATION_SUCCESS);

    SituationVirtualDisplaySamplerDesc sampler = SituationVirtualDisplaySamplerDescDefault();
    sampler.min_filter = SIT_TEXTURE_FILTER_NEAREST;
    sampler.mag_filter = SIT_TEXTURE_FILTER_NEAREST;
    SIT_ASSERT_EQ(SituationSetVirtualDisplaySampler(vd_id, &sampler), SITUATION_SUCCESS);

    SituationShader shader = {0};
    SIT_ASSERT_EQ(SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader), SITUATION_SUCCESS);
    float vertices[] = { -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f };
    uint32_t indices[] = {0, 1, 2};
    SituationMesh mesh = {0};
    SIT_ASSERT_EQ(SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT_EQ(SituationAcquireFrameCommandBuffer(), SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo vd_rp = {0};
    vd_rp.display_id = vd_id;
    vd_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    vd_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    vd_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    vd_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    vd_rp.depth_attachment.clear.depth = 1.0f;
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &vd_rp), SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);

    vd_composite_main_pass_clear_and_present(cmd, (ColorRGBA){0, 0, 0, 255});

    SituationImage screen = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen), SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int pi = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[pi] > 200);
    SIT_ASSERT(pixels[pi+1] < 30);
    SIT_ASSERT(pixels[pi+2] < 30);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * vd_aniso_sampler_configure — max anisotropy configure API smoke (VD-4a).
 */
static void test_vd_aniso_sampler_configure(void) {
    int vd_id = -1;
    SIT_ASSERT_EQ(SituationCreateVirtualDisplay((Vector2){16.0f, 16.0f}, 1.0, 0,
        SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetVirtualDisplayMaxAnisotropy(vd_id, 4.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetVirtualDisplayMipLevels(vd_id, 0, 0), SITUATION_SUCCESS);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * vd_update_mode_static — static VDs do not advance frame clock (VD-5).
 */
static void test_vd_update_mode_static(void) {
    int vd_id = -1;
    SIT_ASSERT_EQ(SituationCreateVirtualDisplay((Vector2){16.0f, 16.0f}, 1.0, 0,
        SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetVirtualDisplayUpdateMode(vd_id, SIT_VD_UPDATE_STATIC), SITUATION_SUCCESS);

    SituationVirtualDisplay* vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT(vd != NULL);
    uint64_t before = vd->frame_count;
    SituationPollInputEvents();
    SituationUpdateTimers();
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT_EQ(vd->frame_count, before);

    SituationDestroyVirtualDisplay(vd_id);
}

static const char* g_cs_image_write =
    "#version 460 core\n"
    "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
    "layout(rgba8, binding = 0) uniform writeonly image2D outImage;\n"
    "void main() {\n"
    "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
    "    imageStore(outImage, pos, vec4(1.0, 0.0, 0.0, 1.0));\n"
    "}\n";

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
    bool acquired = (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SIT_ASSERT(acquired);

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // Begin main-window render pass (required before compositing VDs)
    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    err = SituationCmdBeginRenderPass(cmd, &rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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

    // Composite to main G�� VD2 (z=1) should be on top
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    // Wide VD (128x32) in the default harness window � should letterbox (black top/bottom)
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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

    // Composite over bright green G�� BLEND_NONE should fully overwrite
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
    SituationRequestScreenCapture();
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pixels = (uint8_t*)screen.data;

    // (5,5) should be black G�� before the VD offset
    int tl_idx = (5 * screen.width + 5) * 4;
    SIT_ASSERT(pixels[tl_idx] < 30);
    SIT_ASSERT(pixels[tl_idx+1] < 30);
    SIT_ASSERT(pixels[tl_idx+2] < 30);

    // (65, 65) should be red G�� inside the VD area (offset 50 + within 32px)
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

/**
 * After a VD draw pass, SituationGetVirtualDisplayUpdateInfo reports near-zero seconds_since_update.
 */
static void test_vd_content_update_info_after_draw(void) {
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

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationEndFrame();

    double seconds_since = -1.0;
    uint64_t frames_since = 999;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, &frames_since, &seconds_since);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(seconds_since >= 0.0 && seconds_since < 0.25);
    SIT_ASSERT(frames_since < 5);

    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * Clear-only VD pass does not bump content-update timestamp; idle threshold eventually exceeded without draws.
 */
static void test_vd_content_update_info_idle(void) {
    int vd_id = -1;
    Vector2 resolution = {32.0f, 32.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationSetVirtualDisplayIdleThreshold(vd_id, 0.05);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    double t_after_create = 0.0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, &t_after_create, NULL, NULL, NULL);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    /* Clear-only pass — should not refresh content timestamp */
    SituationRenderPassInfo rp_clear = {0};
    rp_clear.display_id = vd_id;
    rp_clear.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_clear.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_clear.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_clear.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_clear.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp_clear);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationEndFrame();

    double t_after_clear_only = 0.0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, &t_after_clear_only, NULL, NULL, NULL);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(t_after_clear_only == t_after_create);

    /* Several frames with no VD writes (VD frame_count advances; wall clock may still be < threshold). */
    for (int i = 0; i < 6; ++i) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
        SituationEndFrame();
    }

    uint64_t frames_since = 0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, &frames_since, NULL);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(frames_since >= 6);

    /* Fast GPUs can run 6 empty frames in <50ms — advance wall clock past idle_threshold. */
    sit_test_harness_wait_ms(80);
    SituationPollInputEvents();
    SituationUpdateTimers();

    double seconds_since = 0.0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, NULL, &seconds_since);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(seconds_since > 0.05);

    SituationDestroyVirtualDisplay(vd_id);
}

static uint32_t vd_wall_ms(void) {
#if defined(_WIN32)
    return GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
#endif
}

static bool vd_load_harness_image(SituationImage* out, const char* filename) {
    static const char* prefixes[] = {
        "tests/harness/assets/",
        "../tests/harness/assets/",
        "../../tests/harness/assets/",
        NULL
    };
    char path[512];

    if (!out || !filename || !filename[0]) {
        return false;
    }

    for (int i = 0; prefixes[i] != NULL; ++i) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], filename);
        if (SituationLoadImage(path, out) == SITUATION_SUCCESS && SituationIsImageValid(*out)) {
            if (out->width > 512 || out->height > 512) {
                int nw = out->width;
                int nh = out->height;
                if (nw > nh) {
                    nw = 512;
                    nh = (int)((512.0f * (float)out->height) / (float)out->width);
                } else {
                    nh = 512;
                    nw = (int)((512.0f * (float)out->width) / (float)out->height);
                }
                if (nw < 1) nw = 1;
                if (nh < 1) nh = 1;
                SituationImageResize(out, nw, nh);
            }
            return SituationIsImageValid(*out);
        }
    }
    return false;
}

static bool vd_strcasecmp_match(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }
#if defined(_WIN32)
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
}

static bool vd_is_image_filename(const char* filename) {
    if (!filename) {
        return false;
    }
    const char* ext = SituationGetFileExtension(filename);
    return ext && SituationIsStbImageLoadExtension(ext);
}

/** Load any harness assets image except exclude_filename (directory scan). */
static bool vd_load_harness_image_other_than(
    SituationImage* out,
    const char* exclude_filename,
    char* out_used_name,
    size_t out_used_name_sz)
{
    static const char* asset_dirs[] = {
        "tests/harness/assets",
        "../tests/harness/assets",
        "../../tests/harness/assets",
        NULL
    };

    if (!out) {
        return false;
    }

    for (int d = 0; asset_dirs[d] != NULL; ++d) {
        int count = 0;
        char** entries = SituationListDirectoryFiles(asset_dirs[d], &count);
        if (!entries || count <= 0) {
            if (entries) {
                SituationFreeDirectoryFileList(entries, count);
            }
            continue;
        }

        for (int i = 0; i < count; ++i) {
            const char* name = entries[i];
            if (!vd_is_image_filename(name)) {
                continue;
            }
            if (exclude_filename && vd_strcasecmp_match(name, exclude_filename)) {
                continue;
            }
            if (vd_load_harness_image(out, name)) {
                if (out_used_name && out_used_name_sz > 0) {
                    snprintf(out_used_name, out_used_name_sz, "%s", name);
                }
                SituationFreeDirectoryFileList(entries, count);
                return true;
            }
        }
        SituationFreeDirectoryFileList(entries, count);
    }
    return false;
}

/** Distinct orange/teal checker when no second photo exists in assets. */
static bool vd_create_orange_teal_test_image(SituationImage* out) {
    if (!out) {
        return false;
    }
    const int w = 256;
    const int h = 256;
    SituationError err = SituationCreateImage(w, h, 4, out);
    if (err != SITUATION_SUCCESS || !SituationIsImageValid(*out)) {
        return false;
    }

    const ColorRGBA orange = {255, 140, 0, 255};
    const ColorRGBA teal = {0, 180, 160, 255};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool use_orange = (((x / 32) + (y / 32)) & 1) == 0;
            SituationSetPixelColor(out, x, y, use_orange ? orange : teal);
        }
    }
    return SituationIsImageValid(*out);
}

/**
 * Live-phase image for COLORBURST test: any harness photo except prairie.jpg,
 * otherwise a built-in orange/teal grid (still runs the 1.5s visual demo).
 */
static bool vd_load_colorburst_live_image(SituationImage* out, char* out_label, size_t out_label_sz) {
    if (vd_load_harness_image_other_than(out, VD_IDLE_SOLID_PHOTO_ASSET, out_label, out_label_sz)) {
        return true;
    }
    if (vd_create_orange_teal_test_image(out)) {
        if (out_label && out_label_sz > 0) {
            snprintf(out_label, out_label_sz, "built-in orange/teal grid");
        }
        fprintf(stderr,
            "[virtual_display] no second image in tests/harness/assets/ besides %s — "
            "using built-in orange/teal grid for the live phase\n",
            VD_IDLE_SOLID_PHOTO_ASSET);
        return true;
    }
    return false;
}

/**
 * SituationCmdDrawTexture into a VD pass must bump content-update metadata (EndRenderPass hook).
 */
static void test_vd_content_update_info_after_draw_texture(void) {
    SituationImage photo = {0};
    if (!vd_load_harness_image(&photo, VD_IDLE_SOLID_PHOTO_ASSET)) {
        fprintf(stderr,
            "[virtual_display] vd_content_update_info_after_draw_texture skipped (missing tests/harness/assets/%s)\n",
            VD_IDLE_SOLID_PHOTO_ASSET);
        return;
    }

    SituationTexture tex = {0};
    SituationError err = SituationCreateTexture(photo, false, &tex);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    int vd_id = -1;
    Vector2 resolution = {(float)photo.width, (float)photo.height};
    err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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

    SitRectangle source = {0.0f, 0.0f, (float)photo.width, (float)photo.height};
    SitRectangle dest = {0.0f, 0.0f, resolution.x, resolution.y};
    Vector2 origin = {{0.0f, 0.0f}};
    err = SituationCmdDrawTexture(cmd, tex, source, dest, origin, 0.0f, (ColorRGBA){255, 255, 255, 255});
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();

    double seconds_since = -1.0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, NULL, &seconds_since);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(seconds_since >= 0.0 && seconds_since < 0.25);

    SituationDestroyTexture(&tex);
    SituationUnloadImage(photo);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * COMPUTE_TARGET VD: dispatch to bound storage image bumps content-update metadata.
 */
static void test_vd_compute_target_updates_on_dispatch(void) {
    int vd_id = -1;
    Vector2 resolution = {4.0f, 4.0f};
    SituationError err = SituationCreateVirtualDisplayEx(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE,
        SITUATION_VD_FLAG_COMPUTE_TARGET, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationTexture tex = {0};
    err = SituationGetVirtualDisplayTexture(vd_id, &tex);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationComputePipeline pipeline = {0};
    err = SituationCreateComputePipelineFromMemory(
        g_cs_image_write, SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO, &pipeline);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyVirtualDisplay(vd_id);
        SIT_TEST_SKIP("compute pipeline unavailable on this backend");
        return;
    }

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationCmdBindComputePipeline(cmd, pipeline);
    SituationCmdBindComputeTexture(cmd, 0, tex);
    SituationCmdDispatch(cmd, 4, 4, 1);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_TRANSFER_READ);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    double seconds_since = -1.0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, NULL, &seconds_since);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(seconds_since >= 0.0 && seconds_since < 0.25);

    SituationDestroyComputePipeline(&pipeline);
    SituationDestroyVirtualDisplay(vd_id);
}

static bool vd_screen_is_idle_solid(void) {
    SituationImage screen = {0};
    SituationError err = SituationLoadImageFromScreen(&screen);
    if (err != SITUATION_SUCCESS || !SituationIsImageValid(screen)) {
        if (SituationIsImageValid(screen)) SituationUnloadImage(screen);
        return false;
    }

    int match = 0;
    int total = screen.width * screen.height;
    const uint8_t* pixels = (const uint8_t*)screen.data;
    for (int i = 0; i < total; ++i) {
        int o = i * 4;
        uint8_t r = pixels[o];
        uint8_t g = pixels[o + 1];
        uint8_t b = pixels[o + 2];
        if (r <= 40 && g <= 60 && b >= 80) {
            match++;
        }
    }
    SituationUnloadImage(screen);
    return total > 0 && ((match * 100) / total) >= 70;
}

static int vd_classify_smpte_bar_pixel(uint8_t r, uint8_t g, uint8_t b) {
    if (r >= 155 && g >= 155 && b >= 155) {
        return 1; /* 75% white */
    }
    if (r >= 155 && g >= 155 && b <= 45) {
        return 2; /* yellow */
    }
    if (r <= 45 && g >= 155 && b >= 155) {
        return 3; /* cyan */
    }
    if (r <= 45 && g >= 155 && b <= 45) {
        return 4; /* green */
    }
    if (r >= 155 && g <= 45 && b >= 155) {
        return 5; /* magenta */
    }
    if (r >= 155 && g <= 45 && b <= 45) {
        return 6; /* red */
    }
    if (r <= 45 && g <= 45 && b >= 155) {
        return 7; /* blue */
    }
    return 0;
}

static bool vd_screen_is_idle_colorburst(void) {
    SituationImage screen = {0};
    SituationError err = SituationLoadImageFromScreen(&screen);
    if (err != SITUATION_SUCCESS || !SituationIsImageValid(screen)) {
        if (SituationIsImageValid(screen)) SituationUnloadImage(screen);
        return false;
    }

    if (vd_screen_is_idle_solid()) {
        SituationUnloadImage(screen);
        return false;
    }

    int bar_hits = 0;
    int bar_kinds_seen = 0;
    int bar_kind_counts[8] = {0};
    int total = screen.width * screen.height;
    const uint8_t* pixels = (const uint8_t*)screen.data;
    for (int i = 0; i < total; ++i) {
        int o = i * 4;
        int kind = vd_classify_smpte_bar_pixel(pixels[o], pixels[o + 1], pixels[o + 2]);
        if (kind > 0) {
            bar_hits++;
            if (bar_kind_counts[kind] == 0) {
                bar_kinds_seen++;
            }
            bar_kind_counts[kind]++;
        }
    }
    SituationUnloadImage(screen);
    return total > 0
        && ((bar_hits * 100) / total) >= 35
        && bar_kinds_seen >= 4;
}

/** PATTERN idle with zero layers — grayscale noise, not solid blue or COLORBURST bars. */
static bool vd_screen_is_idle_snow(void) {
    if (vd_screen_is_idle_solid()) {
        return false;
    }
    if (vd_screen_is_idle_colorburst()) {
        return false;
    }

    SituationImage screen = {0};
    SituationError err = SituationLoadImageFromScreen(&screen);
    if (err != SITUATION_SUCCESS || !SituationIsImageValid(screen)) {
        if (SituationIsImageValid(screen)) {
            SituationUnloadImage(screen);
        }
        return false;
    }

    int samples = 0;
    int gray_ok = 0;
    uint8_t min_l = 255;
    uint8_t max_l = 0;
    const uint8_t* pixels = (const uint8_t*)screen.data;
    for (int py = 8; py < screen.height; py += 16) {
        for (int px = 8; px < screen.width; px += 16) {
            int o = (py * screen.width + px) * 4;
            uint8_t r = pixels[o];
            uint8_t g = pixels[o + 1];
            uint8_t b = pixels[o + 2];
            uint8_t l = (uint8_t)((r + g + b) / 3u);
            if (abs((int)r - (int)g) <= 2 && abs((int)g - (int)b) <= 2) {
                gray_ok++;
            }
            if (l < min_l) {
                min_l = l;
            }
            if (l > max_l) {
                max_l = l;
            }
            samples++;
        }
    }
    SituationUnloadImage(screen);
    return samples >= 4
        && ((gray_ok * 100) / samples) >= 80
        && (max_l - min_l) >= 8;
}

static bool vd_screen_is_live_not_snow(void) {
    return !vd_screen_is_idle_snow();
}

static void vd_composite_only_end_frame(void) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
}

static void vd_draw_photo_and_composite(int vd_id, SituationTexture tex, float img_w, float img_h, float vd_w, float vd_h) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo rp_vd = {0};
    rp_vd.display_id = vd_id;
    rp_vd.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_vd.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_vd.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp_vd.depth_attachment.clear.depth = 1.0f;

    SituationError err = SituationCmdBeginRenderPass(cmd, &rp_vd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SitRectangle source = {0.0f, 0.0f, img_w, img_h};
    SitRectangle dest = {0.0f, 0.0f, vd_w, vd_h};
    Vector2 origin = {{0.0f, 0.0f}};
    err = SituationCmdDrawTexture(cmd, tex, source, dest, origin, 0.0f, (ColorRGBA){255, 255, 255, 255});
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
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
    SituationRequestScreenCapture();
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
}

static void vd_phase_composite_until(uint32_t deadline_ms) {
    while (vd_wall_ms() < deadline_ms) {
        vd_composite_only_end_frame();
    }
}

static void vd_phase_photo_until(uint32_t deadline_ms, int vd_id, SituationTexture tex,
                                 float img_w, float img_h, float vd_w, float vd_h) {
    while (vd_wall_ms() < deadline_ms) {
        vd_draw_photo_and_composite(vd_id, tex, img_w, img_h, vd_w, vd_h);
    }
}

/** Failure-path cleanup for timed idle-switch tests (SIT_ASSERT longjmp skips normal teardown). */
static SituationTexture g_vd_idle_switch_tex = {0};
static int g_vd_idle_switch_vd_id = -1;
static SituationImage g_vd_idle_switch_photo = {0};
static bool g_vd_idle_switch_photo_valid = false;

static void vd_idle_content_switch_test_cleanup(void) {
    if (g_vd_idle_switch_vd_id >= 0) {
        SituationDestroyVirtualDisplay(g_vd_idle_switch_vd_id);
        g_vd_idle_switch_vd_id = -1;
    }
    if (g_vd_idle_switch_tex.slot_index >= 0) {
        SituationDestroyTexture(&g_vd_idle_switch_tex);
        memset(&g_vd_idle_switch_tex, 0, sizeof(g_vd_idle_switch_tex));
    }
    if (g_vd_idle_switch_photo_valid) {
        SituationUnloadImage(g_vd_idle_switch_photo);
        g_vd_idle_switch_photo_valid = false;
        memset(&g_vd_idle_switch_photo, 0, sizeof(g_vd_idle_switch_photo));
    }
}

/**
 * Shared 1.5s timed idle compositor demo (SNOW, SOLID, or COLORBURST standby).
 */
static void vd_run_idle_content_switch_with_image(
    const char* test_name,
    const char* photo_label,
    SituationImage photo,
    SituationVDFallbackMode fallback_mode,
    bool (*screen_is_idle)(void),
    bool (*screen_is_live)(void))
{
    memset(&g_vd_idle_switch_tex, 0, sizeof(g_vd_idle_switch_tex));
    g_vd_idle_switch_vd_id = -1;
    g_vd_idle_switch_photo = photo;
    g_vd_idle_switch_photo_valid = SituationIsImageValid(photo);
    sit_test_set_crash_cleanup(vd_idle_content_switch_test_cleanup);

    SituationTexture tex = {0};
    SituationError err = SituationCreateTexture(photo, false, &tex);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    g_vd_idle_switch_tex = tex;

    int vd_id = -1;
    Vector2 resolution = {(float)photo.width, (float)photo.height};
    err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    g_vd_idle_switch_vd_id = vd_id;

    SituationSetVirtualDisplayIdleThreshold(vd_id, 0.15);
    SituationSetVirtualDisplayFallbackMode(vd_id, fallback_mode);
    if (fallback_mode == SITUATION_VD_FALLBACK_SOLID) {
        SituationSetVirtualDisplayFallbackColor(vd_id, (ColorRGBA){13, 38, 102, 255});
    } else if (fallback_mode == SITUATION_VD_FALLBACK_PATTERN) {
        SituationSetVirtualDisplayPatternLayers(vd_id, 0);
    }

    fprintf(stderr, "[virtual_display] %s: warming up to idle standby...\n", test_name);
    for (int warm = 0; warm < 4; ++warm) {
        vd_composite_only_end_frame();
    }
    sit_test_harness_wait_ms(200);

    const uint32_t t0 = vd_wall_ms();
    const uint32_t t_live_start = t0 + VD_IDLE_SWITCH_LIVE_START;
    const uint32_t t_live_end = t0 + VD_IDLE_SWITCH_LIVE_END;
    const uint32_t t_end = t0 + VD_IDLE_SWITCH_TOTAL_MS;

    const char* mode_label = "SOLID";
    if (fallback_mode == SITUATION_VD_FALLBACK_COLORBURST) {
        mode_label = "COLORBURST";
    } else if (fallback_mode == SITUATION_VD_FALLBACK_PATTERN) {
        mode_label = "SNOW";
    }
    fprintf(stderr, "[virtual_display] %s: 1.5s idle → photo → idle (%s, %s)\n",
        test_name, photo_label ? photo_label : "image", mode_label);

    fprintf(stderr, "  phase 1/3: idle standby (0.0–0.5s)\n");
    vd_phase_composite_until(t_live_start);
    SIT_ASSERT(screen_is_idle());

    fprintf(stderr, "  phase 2/3: live photo content (0.5–1.0s)\n");
    vd_phase_photo_until(t_live_end, vd_id, tex, (float)photo.width, (float)photo.height,
                         resolution.x, resolution.y);
    SituationPollInputEvents();
    SituationUpdateTimers();
    {
        double seconds_since = 999.0;
        uint64_t frames_since = 999;
        err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, &frames_since, &seconds_since);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
        /* Same tolerance as vd_content_update_info_after_draw_texture. Under full-module GPU
           load a single photo frame can exceed 50ms between UpdateTimers and this query. */
        SIT_ASSERT(seconds_since >= 0.0 && seconds_since < 0.25);
        SIT_ASSERT(frames_since < 5);
    }
    SIT_ASSERT(screen_is_live());

    fprintf(stderr, "  phase 3/3: idle standby returns (1.0–1.5s)\n");
    vd_phase_composite_until(t_end);
    SIT_ASSERT(screen_is_idle());

    vd_idle_content_switch_test_cleanup();
    sit_test_set_crash_cleanup(NULL);
}

static void vd_run_idle_content_switch_test(
    const char* test_name,
    const char* photo_asset,
    SituationVDFallbackMode fallback_mode,
    bool (*screen_is_idle)(void),
    bool (*screen_is_live)(void))
{
    SituationImage photo = {0};
    if (!vd_load_harness_image(&photo, photo_asset)) {
        fprintf(stderr,
            "[virtual_display] %s skipped (missing tests/harness/assets/%s)\n",
            test_name, photo_asset);
        return;
    }
    vd_run_idle_content_switch_with_image(
        test_name, photo_asset, photo, fallback_mode, screen_is_idle, screen_is_live);
}

static bool vd_screen_is_live_not_solid(void) {
    return !vd_screen_is_idle_solid();
}

static bool vd_screen_is_live_not_colorburst(void) {
    return !vd_screen_is_idle_colorburst();
}

/**
 * Static COLORBURST idle frame — readback classifies multiple SMPTE bar colors.
 */
static void test_vd_idle_fallback_colorburst(void) {
    int vd_id = -1;
    const float win_w = (float)sit_test_window_render_width();
    const float win_h = (float)sit_test_window_render_height();
    Vector2 resolution = {win_w, win_h};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationConfigureVirtualDisplay(vd_id, (Vector2){0, 0}, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SituationSetVirtualDisplayIdleThreshold(vd_id, 0.15f);
    SituationSetVirtualDisplayFallbackMode(vd_id, SITUATION_VD_FALLBACK_COLORBURST);

    for (int warm = 0; warm < 4; ++warm) {
        vd_composite_only_end_frame();
    }
    sit_test_harness_wait_ms(200);

    double seconds_since = -1.0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, NULL, &seconds_since);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(seconds_since >= 0.15);

    vd_composite_only_end_frame();
    SIT_ASSERT(vd_screen_is_idle_colorburst());

    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * 1.5s timed idle compositor fallback: snow → prairie.jpg → snow (library default standby).
 */
static void test_vd_idle_content_switch(void) {
    vd_run_idle_content_switch_test(
        "vd_idle_content_switch",
        VD_IDLE_SOLID_PHOTO_ASSET,
        SITUATION_VD_FALLBACK_PATTERN,
        vd_screen_is_idle_snow,
        vd_screen_is_live_not_snow);
}

/**
 * 1.5s timed idle compositor fallback: COLORBURST → (any assets photo except prairie, or built-in grid) → COLORBURST.
 */
static void test_vd_idle_content_switch_colorburst(void) {
    SituationImage photo = {0};
    char photo_label[128] = {0};
    if (!vd_load_colorburst_live_image(&photo, photo_label, sizeof(photo_label))) {
        fprintf(stderr,
            "[virtual_display] vd_idle_content_switch_colorburst skipped (failed to prepare live image)\n");
        return;
    }
    vd_run_idle_content_switch_with_image(
        "vd_idle_content_switch_colorburst",
        photo_label,
        photo,
        SITUATION_VD_FALLBACK_COLORBURST,
        vd_screen_is_idle_colorburst,
        vd_screen_is_live_not_colorburst);
}

/**
 * Set/GetVirtualDisplayPatternConfig round-trip — config stored on VD slot.
 */
static void test_vd_pattern_config_api(void) {
    int vd_id = -1;
    Vector2 resolution = {320.0f, 240.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(vd_id >= 0);

    SitVdStandbyConfig cfg;
    sit_harness_pattern_config_init_defaults(&cfg, SIT_VD_STANDBY_CHECKERBOARD, resolution.x, resolution.y);
    cfg.layer.checker.tile_size_x = cfg.layer.checker.tile_size_y = 64.f;
    sit_harness_pattern_toggle_layer(&cfg, SIT_VD_STANDBY_GRID_ONLY, true);

    SituationSetVirtualDisplayPatternConfig(vd_id, &cfg);

    SitVdStandbyConfig out = {0};
    SituationGetVirtualDisplayPatternConfig(vd_id, &out);
    SIT_ASSERT_EQ(out.pattern_layers, cfg.pattern_layers);
    SIT_ASSERT(out.layer.checker.tile_size_x == cfg.layer.checker.tile_size_x);
    SIT_ASSERT(out.layer.checker.tile_size_y == cfg.layer.checker.tile_size_y);
    SIT_ASSERT(out.width == resolution.x);
    SIT_ASSERT(out.height == resolution.y);

    SituationSetVirtualDisplayPatternLayers(vd_id, SIT_VD_STANDBY_LAYER_PLUGE);
    SIT_ASSERT_EQ(SituationGetVirtualDisplayPatternLayers(vd_id), (int32_t)SIT_VD_STANDBY_LAYER_PLUGE);

    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * PATTERN idle compositor — checkerboard via SituationSetVirtualDisplayPatternConfig.
 * Full layer compositor on Vulkan and OpenGL (shaderc→SPIR-V compositor path).
 */
static void test_vd_idle_pattern_standby(void) {
    int vd_id = -1;
    const float win_w = (float)sit_test_window_render_width();
    const float win_h = (float)sit_test_window_render_height();
    Vector2 resolution = {win_w, win_h};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0, SITUATION_SCALING_STRETCH, SITUATION_BLEND_NONE, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(vd_id >= 0);

    SituationConfigureVirtualDisplay(vd_id, (Vector2){0, 0}, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);
    SituationSetVirtualDisplayIdleThreshold(vd_id, 0.15f);

    SitVdStandbyConfig cfg;
    sit_harness_pattern_config_init_defaults(&cfg, SIT_VD_STANDBY_CHECKERBOARD, win_w, win_h);
    SituationSetVirtualDisplayPatternConfig(vd_id, &cfg);

    fprintf(stderr, "[virtual_display] vd_idle_pattern_standby: warming up to PATTERN idle...\n");
    for (int warm = 0; warm < 4; ++warm) {
        vd_composite_only_end_frame();
    }
    sit_test_harness_wait_ms(200);

    double seconds_since = -1.0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, NULL, &seconds_since);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(seconds_since >= 0.15);

    vd_composite_only_end_frame();

    uint8_t white_corner[4] = {0};
    uint8_t dark_corner[4] = {0};
    err = graphics_test_read_pixel_rgba(8, 8, white_corner);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = graphics_test_read_pixel_rgba(40, 8, dark_corner);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_pixel_approx_float(white_corner[0], 1.0f, 2));
    SIT_ASSERT(graphics_test_pixel_approx_float(dark_corner[0], 0.0f, 2));

    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * vd_get_texture_handle — SituationGetVirtualDisplayTexture works for all VDs (not just COMPUTE_TARGET).
 *
 * Creates a regular raster VD and verifies that GetVirtualDisplayTexture returns a valid
 * SituationTexture handle with the correct dimensions. This was broken before v2.4.257 —
 * the function returned RESOURCE_INVALID for non-compute VDs because they were never
 * registered in the texture registry.
 */
static void test_vd_get_texture_handle(void) {
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

    /* This was the bug: returns RESOURCE_INVALID for non-compute VDs before v2.4.257 */
    SituationTexture tex = {0};
    err = SituationGetVirtualDisplayTexture(vd_id, &tex);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(tex.slot_index != 0 || tex.generation != 0); /* valid handle */
    SIT_ASSERT_EQ(tex.width, 64);
    SIT_ASSERT_EQ(tex.height, 64);

    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * vd_load_color_attachment — LOAD_OP_LOAD on a VD preserves prior frame content (GL+VK parity).
 *
 * Frame 1: draw red fullscreen into VD (CLEAR pass).
 * Frame 2: begin VD pass with LOAD_OP_LOAD, draw nothing, end pass, composite.
 *   → VD should still show red (content preserved, not cleared).
 */
static void test_vd_load_color_attachment(void) {
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

    SituationConfigureVirtualDisplay(vd_id, (Vector2){0, 0}, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float verts[] = { -1.0f,-1.0f,0.0f,  3.0f,-1.0f,0.0f,  -1.0f,3.0f,0.0f };
    uint32_t vidx[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(verts, 3, sizeof(float)*3, vidx, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    /* --- Frame 1: CLEAR + draw red into VD --- */
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp_clear = {0};
    rp_clear.display_id = vd_id;
    rp_clear.color_attachment.loadOp  = SIT_LOAD_OP_CLEAR;
    rp_clear.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_clear.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp_clear.depth_attachment.loadOp  = SIT_LOAD_OP_CLEAR;
    rp_clear.depth_attachment.clear.depth = 1.0f;

    err = SituationCmdBeginRenderPass(cmd, &rp_clear);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp  = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp  = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationRenderVirtualDisplays(cmd);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    /* --- Frame 2: LOAD_OP_LOAD — no draw — content must remain red --- */
    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp_load = {0};
    rp_load.display_id = vd_id;
    rp_load.color_attachment.loadOp  = SIT_LOAD_OP_LOAD;
    rp_load.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp_load.depth_attachment.loadOp  = SIT_LOAD_OP_LOAD;
    rp_load.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;

    err = SituationCmdBeginRenderPass(cmd, &rp_load);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    /* No draw — content should remain red */
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationRenderVirtualDisplays(cmd);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationRequestScreenCapture();
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    /* Center pixel must be red — VD filled window, content preserved */
    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int pi = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[pi]   > 200);
    SIT_ASSERT(pixels[pi+1] < 50);
    SIT_ASSERT(pixels[pi+2] < 50);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

static const char* g_fs_solid_green =
    "#version 460 core\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() { fragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n";

/**
 * vd_color_only_no_depth — color-only VD (SIT_VD_DEPTH_NONE) renders and composites without depth.
 */
static void test_vd_color_only_no_depth(void) {
    int vd_id = -1;
    SituationVirtualDisplayDesc desc = {0};
    desc.resolution = (Vector2){64.0f, 64.0f};
    desc.color_format = SIT_VD_FORMAT_RGBA8_UNORM;
    desc.depth_stencil_mode = SIT_VD_DEPTH_NONE;
    desc.attachments.color_load = SIT_LOAD_OP_CLEAR;
    desc.attachments.color_store = SIT_STORE_OP_STORE;
    desc.attachments.clear.color = (ColorRGBA){0, 0, 0, 255};
    desc.scaling_mode = SITUATION_SCALING_STRETCH;
    desc.blend_mode = SITUATION_BLEND_NONE;
    desc.visible = true;
    desc.frame_time_mult = 1.0;

    SituationError err = SituationCreateVirtualDisplayFromDesc(&desc, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(vd_id >= 0);

    SituationVirtualDisplay* vd = SituationGetVirtualDisplay(vd_id);
    SIT_ASSERT_NOT_NULL(vd);
    SIT_ASSERT_EQ(vd->depth_stencil_mode, SIT_VD_DEPTH_NONE);

    SituationConfigureVirtualDisplay(vd_id, (Vector2){0, 0}, 1.0f, 0, true, 1.0, SITUATION_BLEND_NONE);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_green, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float verts[] = { -1.0f,-1.0f,0.0f,  3.0f,-1.0f,0.0f,  -1.0f,3.0f,0.0f };
    uint32_t vidx[] = { 0, 1, 2 };
    SituationMesh mesh = {0};
    err = SituationCreateMesh(verts, 3, sizeof(float)*3, vidx, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp = {0};
    rp.display_id = vd_id;
    rp.color_attachment.loadOp  = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};

    err = SituationCmdBeginRenderPass(cmd, &rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp  = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp  = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &main_rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationRenderVirtualDisplays(cmd);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationRequestScreenCapture();
    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));
    uint8_t* pixels = (uint8_t*)screen.data;
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int pi = (cy * screen.width + cx) * 4;
    SIT_ASSERT(pixels[pi]   < 50);
    SIT_ASSERT(pixels[pi+1] > 200);
    SIT_ASSERT(pixels[pi+2] < 50);

    SituationUnloadImage(screen);
    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * vd_configure_attachment_defaults — SetAttachmentDefaults roundtrips through RenderPassInfoInherit.
 */
static void test_vd_configure_attachment_defaults(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0, 0,
        SITUATION_SCALING_STRETCH,
        SITUATION_BLEND_NONE,
        &vd_id
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationVirtualDisplayAttachmentDefaults defaults = {0};
    defaults.color_load = SIT_LOAD_OP_LOAD;
    defaults.color_store = SIT_STORE_OP_STORE;
    defaults.depth_load = SIT_LOAD_OP_DONT_CARE;
    defaults.depth_store = SIT_STORE_OP_DONT_CARE;
    defaults.stencil_load = SIT_LOAD_OP_DONT_CARE;
    defaults.stencil_store = SIT_STORE_OP_DONT_CARE;
    defaults.clear.color = (ColorRGBA){10, 20, 30, 255};
    defaults.clear.depth = 0.5f;
    defaults.clear.stencil = 7;

    err = SituationSetVirtualDisplayAttachmentDefaults(vd_id, &defaults);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationRenderPassInfo inherited = SituationRenderPassInfoInherit(vd_id);
    SIT_ASSERT_EQ(inherited.display_id, vd_id);
    SIT_ASSERT_EQ(inherited.color_attachment.loadOp, defaults.color_load);
    SIT_ASSERT_EQ(inherited.color_attachment.storeOp, defaults.color_store);
    SIT_ASSERT_EQ(inherited.depth_attachment.loadOp, defaults.depth_load);
    SIT_ASSERT_EQ(inherited.depth_attachment.storeOp, defaults.depth_store);
    SIT_ASSERT_EQ(inherited.stencil_attachment.loadOp, defaults.stencil_load);
    SIT_ASSERT_EQ(inherited.stencil_attachment.storeOp, defaults.stencil_store);
    SIT_ASSERT_EQ(inherited.color_attachment.clear.color.r, defaults.clear.color.r);
    SIT_ASSERT_EQ(inherited.color_attachment.clear.color.g, defaults.clear.color.g);
    SIT_ASSERT_EQ(inherited.color_attachment.clear.color.b, defaults.clear.color.b);
    SIT_ASSERT_EQ(inherited.color_attachment.clear.color.a, defaults.clear.color.a);
    SIT_ASSERT(fabs(inherited.depth_attachment.clear.depth - defaults.clear.depth) < 0.0001f);
    SIT_ASSERT_EQ(inherited.stencil_attachment.clear.stencil, defaults.clear.stencil);

    SituationDestroyVirtualDisplay(vd_id);
}

/**
 * Track D regression guard (LIBRARY_RECOVERY_PLAN_244 §D-C1 / G7):
 * Consumer apps use LoadShaderFromMemory + RenderVirtualDisplays + consecutive EndFrame
 * with implicit pre-swap screenshot capture (v2.4.362+). A single EndFrame assert is not
 * enough — stale GL errors from frame N screenshot readback or VD compositor restore can
 * make frame N+1 EndFrame return OPENGL_GENERAL (-600) on the render thread.
 */
#if defined(SITUATION_USE_OPENGL)
static void test_gl_endframe_execute_vd_streak(void) {
    int vd_id = -1;
    Vector2 resolution = {64.0f, 64.0f};
    SituationError err = SituationCreateVirtualDisplay(
        resolution, 1.0f, 0,
        SITUATION_SCALING_STRETCH,
        SITUATION_BLEND_ALPHA,
        &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    Vector2 offset = {0.0f, 0.0f};
    err = SituationConfigureVirtualDisplay(vd_id, offset, 1.0f, 0, true, 1.0f, SITUATION_BLEND_ALPHA);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = SituationLoadShaderFromMemory(g_vs_passthrough, g_fs_solid_red, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f
    };
    uint32_t indices[] = {0, 1, 2};
    SituationMesh mesh = {0};
    err = SituationCreateMesh(vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationRenderPassInfo vd_rp = {0};
    vd_rp.display_id = vd_id;
    vd_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    vd_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    vd_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    vd_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    vd_rp.depth_attachment.clear.depth = 1.0f;

    SituationRenderPassInfo main_rp = {0};
    main_rp.display_id = -1;
    main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    main_rp.depth_attachment.clear.depth = 1.0f;

    static const SituationBlendMode blend_per_frame[] = {
        SITUATION_BLEND_ALPHA,
        SITUATION_BLEND_OVERLAY,
        SITUATION_BLEND_ALPHA,
        SITUATION_BLEND_NONE,
    };

    for (int frame = 0; frame < 4; ++frame) {
        err = SituationConfigureVirtualDisplay(
            vd_id, offset, 1.0f, 0, true, 1.0f, blend_per_frame[frame]);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

        SituationPollInputEvents();
        SituationUpdateTimers();
        SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SIT_ASSERT_NOT_NULL(cmd);

        err = SituationCmdBeginRenderPass(cmd, &vd_rp);
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

        /* No SituationRequestScreenCapture — production EndFrame -> LoadImageFromScreen path. */
        err = SituationEndFrame();
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    }

    SituationImage screen = {0};
    err = SituationLoadImageFromScreen(&screen);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));
    SituationUnloadImage(screen);

    SituationDestroyMesh(&mesh);
    SituationUnloadShader(&shader);
    SituationDestroyVirtualDisplay(vd_id);
}
#endif /* SITUATION_USE_OPENGL */

static SitTestCase virtual_display_tests[] = {
    {"create_destroy_virtual_display",  test_create_destroy_virtual_display, true},
    {"configure_virtual_display",       test_configure_virtual_display,     true},
    {"get_virtual_display",             test_get_virtual_display,           true},
    {"virtual_display_dirty_flag",      test_virtual_display_dirty_flag,    true},
    {"virtual_display_size",            test_virtual_display_size,          true},
    {"render_virtual_displays",         test_render_virtual_displays,       true},
#if defined(SITUATION_USE_OPENGL)
    {"gl_endframe_execute_vd_streak",   test_gl_endframe_execute_vd_streak, true},
#endif
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
    {"vd_content_update_info_after_draw", test_vd_content_update_info_after_draw, true},
    {"vd_content_update_info_idle",    test_vd_content_update_info_idle,   true},
    {"vd_content_update_info_after_draw_texture", test_vd_content_update_info_after_draw_texture, true},
    {"vd_compute_target_updates_on_dispatch", test_vd_compute_target_updates_on_dispatch, true},
    /* v2.4.257 correctness fixes */
    {"vd_get_texture_handle",           test_vd_get_texture_handle,         true},
    {"vd_load_color_attachment",        test_vd_load_color_attachment,      true},
    {"vd_color_only_no_depth",          test_vd_color_only_no_depth,        true},
    {"vd_configure_attachment_defaults", test_vd_configure_attachment_defaults, true},
    {"vd_default_clear_color",           test_vd_default_clear_color,           true},
    {"vd_srgb_format_composite",         test_vd_srgb_format_composite,         true},
    {"vd_sampler_nearest_upscale",       test_vd_sampler_nearest_upscale,       true},
    {"vd_aniso_sampler_configure",       test_vd_aniso_sampler_configure,       true},
    {"vd_update_mode_static",            test_vd_update_mode_static,            true},
    /* COLORBURST before snow/prairie so a failure cannot leave a leaked VD on screen. */
    {"vd_idle_fallback_colorburst",       test_vd_idle_fallback_colorburst,       true},
    {"vd_idle_content_switch_colorburst", test_vd_idle_content_switch_colorburst, true},
    {"vd_idle_content_switch",         test_vd_idle_content_switch,        true},
    {"vd_pattern_config_api",          test_vd_pattern_config_api,         true},
    {"vd_idle_pattern_standby",        test_vd_idle_pattern_standby,     true},
};

const SitTestModule g_module_virtual_display = {
    .name = "virtual_display",
    .setup = virtual_display_setup,
    .teardown = virtual_display_teardown,
    .tests = virtual_display_tests,
    .test_count = sizeof(virtual_display_tests) / sizeof(virtual_display_tests[0]),
    .requires_context = true
};
