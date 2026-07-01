/**
 * @file test_output_color_depth.c
 * @brief 10-bit output color depth harness (Phase 4 — doc/plan/10BIT_COLOR_OUTPUT_PLAN.md)
 *
 * CI always runs 8-bit-default modules. This module adds:
 * - monitor hot-swap stress (skip when < 2 displays)
 * - opt-in 10-bit verification when SIT_TEST_10BIT=1
 * - opt-in HDR10 swapchain verification when SIT_TEST_HDR=1
 * - opt-in visual grading acceptance when SIT_TEST_10BIT_VISUAL=1
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_window.h"
#include "sit_test_output_color_depth.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static bool g_init_ok = false;
static SituationOutputColorDepth g_test_init_policy = SIT_OUTPUT_COLOR_8BIT;

static void sit_test_render_solid_clear(ColorRGBA color, int frames);

static void output_color_depth_setup(void) {
    SituationInitInfo config = {0};
    if (sit_test_hdr_enabled() || sit_test_hdr_visual_enabled()) {
        sit_test_window_init_info_hdr10(&config, "SIT_TEST_HDR");
        g_test_init_policy = SIT_OUTPUT_COLOR_HDR10;
    } else if (sit_test_10bit_enabled() || sit_test_10bit_visual_enabled()) {
        sit_test_window_init_info_10bit(&config, "SIT_TEST_10BIT");
        g_test_init_policy = SIT_OUTPUT_COLOR_AUTO;
    } else {
        sit_test_window_init_info(&config, "SIT_TEST_OUTPUT_COLOR_DEPTH");
        g_test_init_policy = SIT_OUTPUT_COLOR_8BIT;
    }
    if (sit_test_10bit_visual_enabled()) {
        config.initial_active_window_flags |= SITUATION_FLAG_WINDOW_RESIZABLE;
    }

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    if (sit_test_hdr_enabled() || sit_test_hdr_visual_enabled()) {
        sit_test_position_window_on_hdr_monitor();
        sit_test_render_solid_clear((ColorRGBA){0, 0, 0, 255}, 2);
    }
}

static void output_color_depth_teardown(void) {
    if (g_init_ok) {
        graphics_test_destroy_fullscreen_mesh();
        SituationShutdown();
        g_init_ok = false;
    }
}

static void sit_test_render_solid_clear(ColorRGBA color, int frames) {
    for (int i = 0; i < frames; ++i) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        SIT_ASSERT_EQ(sit_test_acquire_frame(), SITUATION_SUCCESS);
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SIT_ASSERT_NOT_NULL(cmd);

        SituationRenderPassInfo rp = {0};
        rp.display_id = -1;
        rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        rp.color_attachment.clear.color = color;
        rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.depth_attachment.clear.depth = 1.0f;
        SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);
        SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
        SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);
    }
}

static void test_opt_in_10bit_caps_and_hdr(void) {
    if (!sit_test_10bit_enabled()) {
        SIT_TEST_SKIP("set SIT_TEST_10BIT=1 for opt-in 10-bit swapchain/FB verification");
    }

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    sit_test_assert_output_color_depth_consistent();

    fprintf(stderr, "[output_color_depth] SIT_TEST_10BIT: bits=%u active=%u sdr10=%d hdr10=%d backend=%d\n",
        (unsigned)caps.output_bits_per_channel,
        (unsigned)caps.output_color_depth_active,
        SituationIsFeatureSupported(SIT_FEATURE_10BIT_SDR_OUTPUT) ? 1 : 0,
        SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT) ? 1 : 0,
        (int)caps.backend);

    if (caps.output_color_depth_active) {
        SIT_ASSERT(caps.output_bits_per_channel == 10u);
        SIT_ASSERT(!caps.output_hdr_active);
        SIT_ASSERT(SituationIsFeatureSupported(SIT_FEATURE_10BIT_SDR_OUTPUT));
        SIT_ASSERT(!SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT));
    } else {
        SIT_ASSERT(caps.output_bits_per_channel == 8u);
        SIT_ASSERT(!SituationIsFeatureSupported(SIT_FEATURE_10BIT_SDR_OUTPUT));
        SIT_ASSERT(!SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT));
    }
}

static void test_opt_in_10bit_screenshot_readback(void) {
    if (!sit_test_10bit_enabled()) {
        SIT_TEST_SKIP("set SIT_TEST_10BIT=1 for opt-in 10-bit screenshot readback");
    }

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    if (!caps.output_color_depth_active) {
        fprintf(stderr,
            "[output_color_depth] 10-bit not active on this system — screenshot subtest skipped (fail-soft OK)\n");
        SIT_ASSERT(true);
        return;
    }

    sit_test_render_solid_clear((ColorRGBA){32, 64, 200, 255}, 2);

    SituationImage screen = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen), SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));
    SIT_ASSERT(screen.width == SituationGetScreenWidth());
    SIT_ASSERT(screen.height == SituationGetScreenHeight());

    const int cx = screen.width / 2;
    const int cy = screen.height / 2;
    const size_t idx = ((size_t)cy * (size_t)screen.width + (size_t)cx) * 4u;
    const uint8_t* pixels = (const uint8_t*)screen.data;
    SIT_ASSERT_NOT_NULL(pixels);
    SIT_ASSERT(idx + 3u < (size_t)screen.width * (size_t)screen.height * 4u);
    SIT_ASSERT(pixels[idx + 0] >= 20 && pixels[idx + 0] <= 48);
    SIT_ASSERT(pixels[idx + 1] >= 48 && pixels[idx + 1] <= 80);
    SIT_ASSERT(pixels[idx + 2] >= 180);

    SituationUnloadImage(screen);
}

#define SIT_TEST_GRADING_BAND_SECONDS 5.0

typedef struct SitTestGradingPushData {
    float fb_width;
    float fb_height;
    float grade_max;
    float pad;
} SitTestGradingPushData;

#if defined(SITUATION_USE_VULKAN)
/** Four horizontal bands; one grading step per framebuffer column (top→bottom: B&W, R, G, B). */
static const char* g_fs_grading_bands =
    "#version 450\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    float uFbWidth;\n"
    "    float uFbHeight;\n"
    "    float uGradeMax;\n"
    "    float uPad;\n"
    "} pc;\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() {\n"
    "    int max_col = int(pc.uGradeMax);\n"
    "    int col = int(floor(gl_FragCoord.x));\n"
    "    col = clamp(col, 0, max_col);\n"
    "    float t = (pc.uGradeMax > 0.0) ? (float(col) / pc.uGradeMax) : 0.0;\n"
    "    float band_h = max(pc.uFbHeight * 0.25, 1.0);\n"
    "    float y_top = pc.uFbHeight - gl_FragCoord.y;\n"
    "    int band = int(floor(y_top / band_h));\n"
    "    band = clamp(band, 0, 3);\n"
    "    if (band == 0) fragColor = vec4(t, t, t, 1.0);\n"
    "    else if (band == 1) fragColor = vec4(t, 0.0, 0.0, 1.0);\n"
    "    else if (band == 2) fragColor = vec4(0.0, t, 0.0, 1.0);\n"
    "    else fragColor = vec4(0.0, 0.0, t, 1.0);\n"
    "}\n";
#else
static const char* g_fs_grading_bands =
    "#version 460 core\n"
    "uniform vec2 uFbSize;\n"
    "uniform float uGradeMax;\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() {\n"
    "    int max_col = int(uGradeMax);\n"
    "    int col = int(floor(gl_FragCoord.x));\n"
    "    col = clamp(col, 0, max_col);\n"
    "    float t = (uGradeMax > 0.0) ? (float(col) / uGradeMax) : 0.0;\n"
    "    float band_h = max(uFbSize.y * 0.25, 1.0);\n"
    "    float y_top = uFbSize.y - gl_FragCoord.y;\n"
    "    int band = int(floor(y_top / band_h));\n"
    "    band = clamp(band, 0, 3);\n"
    "    if (band == 0) fragColor = vec4(t, t, t, 1.0);\n"
    "    else if (band == 1) fragColor = vec4(t, 0.0, 0.0, 1.0);\n"
    "    else if (band == 2) fragColor = vec4(0.0, t, 0.0, 1.0);\n"
    "    else fragColor = vec4(0.0, 0.0, t, 1.0);\n"
    "}\n";
#endif

#if defined(SITUATION_USE_VULKAN)
/** Full-screen horizontal PQ signal ramp (fragment output = ST.2084 code, not linear). */
static const char* g_fs_pq_ramp =
    "#version 450\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    float uFbWidth;\n"
    "    float uFbHeight;\n"
    "    float uGradeMax;\n"
    "    float uPad;\n"
    "} pc;\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() {\n"
    "    int max_col = int(pc.uGradeMax);\n"
    "    int col = int(floor(gl_FragCoord.x));\n"
    "    col = clamp(col, 0, max_col);\n"
    "    float pq = (pc.uGradeMax > 0.0) ? (float(col) / pc.uGradeMax) : 0.0;\n"
    "    fragColor = vec4(pq, pq, pq, 1.0);\n"
    "}\n";

/** Four horizontal PQ ramps (top→bottom: gray, R, G, B). */
static const char* g_fs_pq_grading_bands =
    "#version 450\n"
    "layout(push_constant) uniform PushConstants {\n"
    "    float uFbWidth;\n"
    "    float uFbHeight;\n"
    "    float uGradeMax;\n"
    "    float uPad;\n"
    "} pc;\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() {\n"
    "    int max_col = int(pc.uGradeMax);\n"
    "    int col = int(floor(gl_FragCoord.x));\n"
    "    col = clamp(col, 0, max_col);\n"
    "    float pq = (pc.uGradeMax > 0.0) ? (float(col) / pc.uGradeMax) : 0.0;\n"
    "    float band_h = max(pc.uFbHeight * 0.25, 1.0);\n"
    "    float y_top = pc.uFbHeight - gl_FragCoord.y;\n"
    "    int band = int(floor(y_top / band_h));\n"
    "    band = clamp(band, 0, 3);\n"
    "    if (band == 0) fragColor = vec4(pq, pq, pq, 1.0);\n"
    "    else if (band == 1) fragColor = vec4(pq, 0.0, 0.0, 1.0);\n"
    "    else if (band == 2) fragColor = vec4(0.0, pq, 0.0, 1.0);\n"
    "    else fragColor = vec4(0.0, 0.0, pq, 1.0);\n"
    "}\n";
#endif

static SituationError sit_test_draw_grading_bands_frame(
    SituationShader shader,
    SituationMesh mesh,
    const SitTestGradingPushData* push)
{
    SituationPollInputEvents();
    SituationUpdateTimers();
    SituationError err = sit_test_acquire_frame();
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    err = SituationCmdBeginRenderPass(cmd, &rp);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

#if defined(SITUATION_USE_OPENGL)
    {
        const float fb_size[2] = {push->fb_width, push->fb_height};
        err = SituationSetShaderUniform(shader, "uFbSize", fb_size, SIT_UNIFORM_VEC2);
        if (err != SITUATION_SUCCESS) {
            SituationCmdEndRenderPass(cmd);
            return err;
        }
        err = SituationSetShaderUniform(shader, "uGradeMax", &push->grade_max, SIT_UNIFORM_FLOAT);
        if (err != SITUATION_SUCCESS) {
            SituationCmdEndRenderPass(cmd);
            return err;
        }
    }
#endif

    err = SituationCmdBindPipeline(cmd, shader);
    if (err != SITUATION_SUCCESS) {
        SituationCmdEndRenderPass(cmd);
        return err;
    }
#if defined(SITUATION_USE_VULKAN)
    err = SituationCmdSetPushConstant(cmd, 0, push, sizeof(*push));
    if (err != SITUATION_SUCCESS) {
        SituationCmdEndRenderPass(cmd);
        return err;
    }
#endif
    err = SituationCmdDrawMesh(cmd, mesh);
    if (err != SITUATION_SUCCESS) {
        SituationCmdEndRenderPass(cmd);
        return err;
    }
    err = SituationCmdEndRenderPass(cmd);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    return SituationEndFrame();
}

/**
 * Visual acceptance: force 1024×768 framebuffer, four 1024-step horizontal ramps (B&W, R, G, B) for 5s.
 * Set SIT_TEST_10BIT_VISUAL=1 (and SIT_TEST_10BIT=1 for AUTO 10-bit init). Skipped in headless CI.
 */
static void test_visual_10bit_grading_bands(void) {
    if (!sit_test_10bit_visual_enabled()) {
        SIT_TEST_SKIP("set SIT_TEST_10BIT_VISUAL=1 for 5s on-screen 1024-step grading bands");
    }
    if (sit_test_headless()) {
        SIT_TEST_SKIP("visible window required for visual grading acceptance");
    }

    if (!sit_test_force_render_resolution(SIT_TEST_WINDOW_WIDTH, SIT_TEST_WINDOW_HEIGHT, 24)) {
        fprintf(stderr,
            "[output_color_depth] could not lock render size to %dx%d (got %dx%d client %dx%d)\n",
            SIT_TEST_WINDOW_WIDTH,
            SIT_TEST_WINDOW_HEIGHT,
            SituationGetRenderWidth(),
            SituationGetRenderHeight(),
            SituationGetScreenWidth(),
            SituationGetScreenHeight());
        SIT_ASSERT(false);
    }

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    sit_test_assert_output_color_depth_consistent();
    fprintf(stderr,
        "[output_color_depth] visual grading bands: render %dx%d, %u bpp active=%u — hold %.0fs "
        "(top→bottom: B&W | R | G | B)\n",
        SituationGetRenderWidth(),
        SituationGetRenderHeight(),
        (unsigned)caps.output_bits_per_channel,
        (unsigned)caps.output_color_depth_active,
        SIT_TEST_GRADING_BAND_SECONDS);

    SituationSetWindowTitle("Situation: 10-bit grading — B&W | R | G | B (5s)");

    SituationShader shader = {0};
    SIT_ASSERT_EQ(
        SituationLoadShaderFromMemory(graphics_test_glsl_vs_passthrough(), g_fs_grading_bands, &shader),
        SITUATION_SUCCESS);
    SituationMesh mesh = graphics_test_fullscreen_mesh();

    const SitTestGradingPushData push = {
        (float)SituationGetRenderWidth(),
        (float)SituationGetRenderHeight(),
        (float)(SituationGetRenderWidth() - 1),
        0.0f,
    };

    const double end_time = sit_get_time_seconds() + SIT_TEST_GRADING_BAND_SECONDS;
    int frames = 0;
    while (sit_get_time_seconds() < end_time) {
        if (SituationWindowShouldClose()) {
            break;
        }
        SIT_ASSERT_EQ(sit_test_draw_grading_bands_frame(shader, mesh, &push), SITUATION_SUCCESS);
        frames++;
    }
    SIT_ASSERT(frames > 0);

    SituationUnloadShader(&shader);
    sit_test_window_restore_default_size_limits();
}

static void test_monitor_hot_swap_recreate(void) {
    const int monitor_count = SituationGetMonitorCount();
    if (monitor_count < 2) {
        SIT_TEST_SKIP("needs 2+ monitors for display hot-swap stress");
    }

    sit_test_assert_output_color_depth_consistent();

    const int primary = 0;
    const int secondary = 1;
    Vector2 sec_pos = SituationGetMonitorPosition(secondary);
    const int sec_w = SituationGetMonitorWidth(secondary);
    const int sec_h = SituationGetMonitorHeight(secondary);
    SIT_ASSERT(sec_w > 0 && sec_h > 0);

    SituationSetWindowSize(640, 480);
    SituationSetWindowPosition((int)sec_pos.x + sec_w / 4, (int)sec_pos.y + sec_h / 4);
    SituationPollInputEvents();
    sit_test_render_solid_clear((ColorRGBA){128, 0, 128, 255}, 3);

    SIT_ASSERT(SituationGetCurrentMonitor() >= 0);
    sit_test_assert_output_color_depth_consistent();

    SituationSetWindowMonitor(secondary);
    SituationPollInputEvents();
    sit_test_render_solid_clear((ColorRGBA){0, 128, 128, 255}, 3);
    sit_test_assert_output_color_depth_consistent();

    SituationToggleFullscreen();
    SituationPollInputEvents();
    sit_test_render_solid_clear((ColorRGBA){64, 64, 64, 255}, 2);
    SituationToggleFullscreen();
    SituationPollInputEvents();

    SituationSetWindowMonitor(primary);
    SituationPollInputEvents();
    sit_test_render_solid_clear((ColorRGBA){200, 200, 200, 255}, 2);
    sit_test_assert_output_color_depth_consistent();

    /* Exit fullscreen before module teardown. Without this, the next module's
     * SituationInit → glfwCreateWindow can crash or hang on NVIDIA Windows
     * because glfwDestroyWindow in exclusive fullscreen leaves the ICD dirty. */
    SituationToggleFullscreen();
    SituationPollInputEvents();
    sit_test_render_solid_clear((ColorRGBA){100, 100, 100, 255}, 3);

    sit_test_window_restore_default_size_limits();
}

static void test_report_hdr_10bit_display_capability(void) {
    SituationDisplayInfo* displays = NULL;
    int display_count = 0;
    SIT_ASSERT_EQ(SituationGetDisplays(&displays, &display_count), SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(displays);
    SIT_ASSERT(display_count > 0);

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    sit_test_assert_output_color_depth_consistent();

    int dxgi_valid = 0;
    int hdr_supported_count = 0;
    int hdr_enabled_count = 0;
    int bpc10_count = 0;
    const int window_monitor = SituationGetCurrentMonitor();

    fprintf(stderr, "[output_color_depth] === HDR / 10-bit capability report (Phase 5–6) ===\n");
    fprintf(stderr,
        "[output_color_depth] Init policy: %s\n",
        sit_test_output_color_policy_name(g_test_init_policy));
    fprintf(stderr,
        "[output_color_depth] Swapchain: bits=%u active_10bit=%u hdr_active=%u color_space=%s (%u)\n",
        (unsigned)caps.output_bits_per_channel,
        (unsigned)caps.output_color_depth_active,
        (unsigned)caps.output_hdr_active,
        sit_test_output_color_space_name(caps.output_color_space),
        (unsigned)caps.output_color_space);
    fprintf(stderr,
        "[output_color_depth] WSI: wsi_10bit_sdr=%u wsi_hdr10=%u | Features: sdr10=%d hdr10=%d | window_monitor=%d\n",
        (unsigned)caps.wsi_supports_10bit_sdr,
        (unsigned)caps.wsi_supports_hdr10,
        SituationIsFeatureSupported(SIT_FEATURE_10BIT_SDR_OUTPUT) ? 1 : 0,
        SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT) ? 1 : 0,
        window_monitor);

    for (int i = 0; i < display_count; ++i) {
        const SituationDisplayInfo* d = &displays[i];
        fprintf(stderr,
            "[output_color_depth] Monitor[%d] \"%s\" %s %dx%d@%d desktop_bpp=%d\n",
            i,
            d->name,
            d->is_primary ? "(primary)" : "",
            d->current_mode.width,
            d->current_mode.height,
            d->current_mode.refresh_rate,
            d->current_mode.color_depth);
#if defined(_WIN32)
        fprintf(stderr,
            "[output_color_depth]   DXGI: metadata_valid=%d bpc=%u max_nits=%.1f color_space=%u hdr_supported=%d hdr_enabled=%d\n",
            d->dxgi_metadata_valid ? 1 : 0,
            (unsigned)d->bits_per_color,
            (double)d->max_luminance_nits,
            (unsigned)d->dxgi_color_space,
            d->hdr_supported ? 1 : 0,
            d->hdr_enabled ? 1 : 0);
        if (d->dxgi_metadata_valid) {
            ++dxgi_valid;
            if (d->hdr_supported) {
                ++hdr_supported_count;
            }
            if (d->hdr_enabled) {
                ++hdr_enabled_count;
            }
            if (d->bits_per_color >= 10u) {
                ++bpc10_count;
            }
        }
#else
        fprintf(stderr, "[output_color_depth]   DXGI: N/A (Windows-only Phase 5 probe)\n");
#endif
    }

#if defined(_WIN32)
    fprintf(stderr,
        "[output_color_depth] Summary: monitors=%d dxgi_ok=%d hdr_supported=%d hdr_enabled=%d bpc>=10=%d\n",
        display_count,
        dxgi_valid,
        hdr_supported_count,
        hdr_enabled_count,
        bpc10_count);
    if (window_monitor >= 0 && window_monitor < display_count) {
        const SituationDisplayInfo* wm = &displays[window_monitor];
        fprintf(stderr,
            "[output_color_depth] Window monitor[%d]: dxgi_hdr_enabled=%d (needed for HDR10 swapchain + AUTO)\n",
            window_monitor,
            (wm->dxgi_metadata_valid && wm->hdr_enabled) ? 1 : 0);
    }
    SIT_ASSERT_EQ(dxgi_valid, display_count);
#endif

    SituationFreeDisplays(displays, display_count);
    SIT_ASSERT(true);
}

static bool sit_test_window_monitor_dxgi_hdr_enabled(const SituationDisplayInfo* displays, int count) {
    if (!displays || count <= 0) {
        return false;
    }
    const int mid = SituationGetCurrentMonitor();
    if (mid < 0 || mid >= count) {
        return false;
    }
    const SituationDisplayInfo* wm = &displays[mid];
    return wm->dxgi_metadata_valid && wm->hdr_enabled;
}

static void test_opt_in_hdr10_swapchain_capability(void) {
    if (!sit_test_hdr_enabled()) {
        SIT_TEST_SKIP("set SIT_TEST_HDR=1 for opt-in HDR10 swapchain verification");
    }

    SituationDisplayInfo* displays = NULL;
    int display_count = 0;
    SIT_ASSERT_EQ(SituationGetDisplays(&displays, &display_count), SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(displays);

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    sit_test_assert_output_color_depth_consistent();

    const bool os_hdr = sit_test_window_monitor_dxgi_hdr_enabled(displays, display_count);

    fprintf(stderr,
        "[output_color_depth] SIT_TEST_HDR: policy=%s os_hdr=%d wsi_hdr10=%u hdr_active=%u color_space=%s\n",
        sit_test_output_color_policy_name(g_test_init_policy),
        os_hdr ? 1 : 0,
        (unsigned)caps.wsi_supports_hdr10,
        (unsigned)caps.output_hdr_active,
        sit_test_output_color_space_name(caps.output_color_space));

    if (os_hdr && caps.wsi_supports_hdr10) {
        SIT_ASSERT(caps.output_hdr_active);
        SIT_ASSERT(caps.output_color_depth_active);
        SIT_ASSERT(caps.output_bits_per_channel == 10u);
        SIT_ASSERT(caps.output_color_space == (uint8_t)SIT_OUTPUT_COLOR_SPACE_HDR10_ST2084);
        SIT_ASSERT(SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT));
        SIT_ASSERT(!SituationIsFeatureSupported(SIT_FEATURE_10BIT_SDR_OUTPUT));
    } else {
        if (!os_hdr) {
            fprintf(stderr,
                "[output_color_depth] HDR10 not active — enable Windows \"Use HDR\" on the window monitor and re-run\n");
        }
        if (!caps.wsi_supports_hdr10) {
            fprintf(stderr,
                "[output_color_depth] HDR10 not active — WSI lacks A2R10G10B10+HDR10_ST2084 on this surface (fail-soft OK)\n");
        }
        SIT_ASSERT(!caps.output_hdr_active);
        SIT_ASSERT(!SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT));
    }

    SituationFreeDisplays(displays, display_count);
}

static void test_hdr_caps_and_feature(void) {
    sit_test_require_hdr10_active();

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    fprintf(stderr,
        "[output_color_depth] hdr_caps_and_feature: hdr_active=%u color_space=%s bits=%u sdr10_feat=%d hdr_feat=%d\n",
        (unsigned)caps.output_hdr_active,
        sit_test_output_color_space_name(caps.output_color_space),
        (unsigned)caps.output_bits_per_channel,
        SituationIsFeatureSupported(SIT_FEATURE_10BIT_SDR_OUTPUT) ? 1 : 0,
        SituationIsFeatureSupported(SIT_FEATURE_HDR_OUTPUT) ? 1 : 0);
    SIT_ASSERT(!SituationIsFeatureSupported(SIT_FEATURE_10BIT_SDR_OUTPUT));
}

static void test_hdr_swapchain_format_logged(void) {
    sit_test_require_hdr10_active();

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    fprintf(stderr,
        "[output_color_depth] hdr_swapchain_format_logged: color_space=%s (%u) wsi_hdr10=%u "
        "(init stderr should contain \"HDR10 swapchain active\")\n",
        sit_test_output_color_space_name(caps.output_color_space),
        (unsigned)caps.output_color_space,
        (unsigned)caps.wsi_supports_hdr10);
    SIT_ASSERT(caps.output_color_space == (uint8_t)SIT_OUTPUT_COLOR_SPACE_HDR10_ST2084);
}

#if defined(SITUATION_USE_VULKAN)
static int sit_test_count_distinct_packed_row(const uint32_t* packed, int width, int row, int pitch_pixels) {
    uint32_t uniq[1024];
    int nuniq = 0;
    const uint32_t* row_px = packed + (size_t)row * (size_t)pitch_pixels;
    for (int x = 0; x < width && x < 1024; ++x) {
        const uint32_t px = row_px[x];
        bool seen = false;
        for (int j = 0; j < nuniq; ++j) {
            if (uniq[j] == px) {
                seen = true;
                break;
            }
        }
        if (!seen && nuniq < 1024) {
            uniq[nuniq++] = px;
        }
    }
    return nuniq;
}

static void sit_test_render_pq_ramp(SituationShader shader, SituationMesh mesh, int frames) {
    const SitTestGradingPushData push = {
        (float)SituationGetRenderWidth(),
        (float)SituationGetRenderHeight(),
        (float)(SituationGetRenderWidth() - 1),
        0.0f,
    };
    for (int i = 0; i < frames; ++i) {
        SIT_ASSERT_EQ(sit_test_draw_grading_bands_frame(shader, mesh, &push), SITUATION_SUCCESS);
    }
}
#endif

static void test_hdr_raw_ramp_readback(void) {
    sit_test_require_hdr10_active();
#if !defined(SITUATION_USE_VULKAN)
    SIT_TEST_SKIP("HDR raw A2R10G10B10 readback requires Vulkan backend");
#else
    if (!sit_test_force_render_resolution(SIT_TEST_WINDOW_WIDTH, SIT_TEST_WINDOW_HEIGHT, 24)) {
        SIT_ASSERT(false);
    }

    SituationShader shader = {0};
    SIT_ASSERT_EQ(
        SituationLoadShaderFromMemory(graphics_test_glsl_vs_passthrough(), g_fs_pq_ramp, &shader),
        SITUATION_SUCCESS);
    SituationMesh mesh = graphics_test_fullscreen_mesh();
    sit_test_render_pq_ramp(shader, mesh, 2);

    const int w = SituationGetRenderWidth();
    const int h = SituationGetRenderHeight();
    const size_t row_pitch = (size_t)w * 4u;
    const size_t buf_bytes = row_pitch * (size_t)h;
    uint32_t* packed = (uint32_t*)malloc(buf_bytes);
    SIT_ASSERT_NOT_NULL(packed);

    SituationReadPixelsDesc desc = {0};
    desc.width = w;
    desc.height = h;
    desc.dst_row_pitch_bytes = row_pitch;
    SIT_ASSERT_EQ(SituationReadFramebufferHdr(&desc, packed, buf_bytes), SITUATION_SUCCESS);

    const int row = h / 2;
    const int distinct = sit_test_count_distinct_packed_row(packed, w, row, w);
    fprintf(stderr,
        "[output_color_depth] hdr_raw_ramp_readback: row=%d distinct_packed=%d (need >=512)\n",
        row,
        distinct);
    SIT_ASSERT(distinct >= 512);

    free(packed);
    SituationUnloadShader(&shader);
    sit_test_window_restore_default_size_limits();
#endif
}

static void test_visual_hdr_grading_bands(void) {
    if (!sit_test_hdr_enabled()) {
        SIT_TEST_SKIP("set SIT_TEST_HDR=1 for HDR visual grading bands");
    }
    if (!sit_test_hdr_visual_enabled()) {
        SIT_TEST_SKIP("set SIT_TEST_HDR_VISUAL=1 for 5s on-screen HDR PQ grading bands");
    }
    if (sit_test_headless()) {
        SIT_TEST_SKIP("visible window required for HDR visual grading acceptance");
    }
#if !defined(SITUATION_USE_VULKAN)
    SIT_TEST_SKIP("HDR visual grading requires Vulkan backend");
#else
    sit_test_require_hdr10_active();

    if (!sit_test_force_render_resolution(SIT_TEST_WINDOW_WIDTH, SIT_TEST_WINDOW_HEIGHT, 24)) {
        SIT_ASSERT(false);
    }

    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    fprintf(stderr,
        "[output_color_depth] visual HDR PQ grading: render %dx%d hdr_active=%u — hold %.0fs "
        "(top→bottom: PQ gray | PQ R | PQ G | PQ B)\n",
        SituationGetRenderWidth(),
        SituationGetRenderHeight(),
        (unsigned)caps.output_hdr_active,
        SIT_TEST_GRADING_BAND_SECONDS);

    SituationSetWindowTitle("Situation: HDR PQ grading — gray | R | G | B (5s)");

    SituationShader shader = {0};
    SIT_ASSERT_EQ(
        SituationLoadShaderFromMemory(graphics_test_glsl_vs_passthrough(), g_fs_pq_grading_bands, &shader),
        SITUATION_SUCCESS);
    SituationMesh mesh = graphics_test_fullscreen_mesh();

    const SitTestGradingPushData push = {
        (float)SituationGetRenderWidth(),
        (float)SituationGetRenderHeight(),
        (float)(SituationGetRenderWidth() - 1),
        0.0f,
    };

    const double end_time = sit_get_time_seconds() + SIT_TEST_GRADING_BAND_SECONDS;
    int frames = 0;
    while (sit_get_time_seconds() < end_time) {
        if (SituationWindowShouldClose()) {
            break;
        }
        SIT_ASSERT_EQ(sit_test_draw_grading_bands_frame(shader, mesh, &push), SITUATION_SUCCESS);
        frames++;
    }
    SIT_ASSERT(frames > 0);

    SituationUnloadShader(&shader);
    sit_test_window_restore_default_size_limits();
#endif
}

static SitTestCase output_color_depth_tests[] = {
    {"report_hdr_10bit_display_capability", test_report_hdr_10bit_display_capability, true, false},
    {"opt_in_hdr10_swapchain_capability", test_opt_in_hdr10_swapchain_capability, true, false},
    {"hdr_caps_and_feature",              test_hdr_caps_and_feature,              true, false},
    {"hdr_swapchain_format_logged",       test_hdr_swapchain_format_logged,       true, false},
    {"hdr_raw_ramp_readback",             test_hdr_raw_ramp_readback,             true, false},
    {"visual_hdr_grading_bands",          test_visual_hdr_grading_bands,          true, true},
    {"opt_in_10bit_caps_and_hdr",        test_opt_in_10bit_caps_and_hdr,        true, false},
    {"opt_in_10bit_screenshot_readback", test_opt_in_10bit_screenshot_readback, true, false},
    {"monitor_hot_swap_recreate",        test_monitor_hot_swap_recreate,        true, false},
    {"visual_10bit_grading_bands",      test_visual_10bit_grading_bands,       true, true},
};

const SitTestModule g_module_output_color_depth = {
    .name = "output_color_depth",
    .setup = output_color_depth_setup,
    .teardown = output_color_depth_teardown,
    .tests = output_color_depth_tests,
    .test_count = sizeof(output_color_depth_tests) / sizeof(output_color_depth_tests[0]),
    .requires_context = true,
};
