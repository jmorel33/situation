/**

 * @file test_graphics_patterns.c

 * @brief Harness readback tests for sit/gpu/test_patterns/ (§7).

 *

 * Config delivery: std140 UBO @ set=0 / binding=0 on both backends via

 * sit_harness_test_pattern_* helpers + SituationCmdBindDescriptorSet (§1.5).

 */



#include "test_graphics_patterns.h"

#include "sit_api_include.h"

#include "sit_harness_pattern_ubo.h"

#include "sit_test_framework.h"

#include "sit_graphics_test_helpers.h"

#include "sit_harness_spirv_embed.h"

#include "sit_harness_test_pattern_embed.h"
#include "sit_harness_test_pattern_helpers.h"

#include "sit_test_window.h"



#include <stdint.h>

#include <stdio.h>

#include <stdlib.h>

#include <string.h>



static SituationBuffer s_pattern_cfg_ubo = {0};
static SituationBuffer s_pattern_params_ssbo = {0};
static SituationBuffer s_pattern_smpte_vd_ubo = {0};



static void pattern_fail_missing_embed(void) {

    fprintf(stderr,

        "[test] Test-pattern harness SPIR-V embed is empty. Run: compile_harness_shaders.bat\n");

    SIT_ASSERT(false);

}



static bool pattern_embed_ready(void) {

    return sit_harness_passthrough_vs_spv_len > 0u

        && sit_harness_test_pattern_fs_spv_len > 0u

        && sit_harness_test_pattern_smpte_vd_fs_spv_len > 0u;

}



static SituationError pattern_load_shader(const unsigned char* fs, size_t fs_len, SituationShader* out) {

    return SituationLoadShaderFromSpirvMemory(

        sit_harness_passthrough_vs_spv,

        sit_harness_passthrough_vs_spv_len,

        fs,

        fs_len,

        out);

}



static SituationError pattern_load_test_pattern_shader(SituationShader* out) {
#if defined(SITUATION_USE_VULKAN)
    return SituationLoadShaderFromSpirvMemoryEx(
        sit_harness_passthrough_vs_spv,
        sit_harness_passthrough_vs_spv_len,
        sit_harness_test_pattern_fs_spv,
        sit_harness_test_pattern_fs_spv_len,
        SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO,
        out);
#else
    return pattern_load_shader(
        sit_harness_test_pattern_fs_spv,
        sit_harness_test_pattern_fs_spv_len,
        out);
#endif
}



static SituationError pattern_load_smpte_vd_shader(SituationShader* out) {

    return pattern_load_shader(

        sit_harness_test_pattern_smpte_vd_fs_spv,

        sit_harness_test_pattern_smpte_vd_fs_spv_len,

        out);

}



static SituationError pattern_ensure_cfg_ubo(void) {

    if (s_pattern_cfg_ubo.generation != 0) {

        return SITUATION_SUCCESS;

    }

    uint8_t zero[SIT_HARNESS_PATTERN_CONFIG_UBO_SIZE] = {0};

    return SituationCreateBuffer(

        SIT_HARNESS_PATTERN_CONFIG_UBO_SIZE,

        zero,

        SITUATION_BUFFER_USAGE_UNIFORM_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,

        &s_pattern_cfg_ubo);

}



static SituationError pattern_ensure_smpte_vd_ubo(void) {

    if (s_pattern_smpte_vd_ubo.generation != 0) {

        return SITUATION_SUCCESS;

    }

    uint8_t zero[SIT_HARNESS_PATTERN_SMPTE_VD_UBO_SIZE] = {0};

    return SituationCreateBuffer(

        SIT_HARNESS_PATTERN_SMPTE_VD_UBO_SIZE,

        zero,

        SITUATION_BUFFER_USAGE_UNIFORM_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,

        &s_pattern_smpte_vd_ubo);

}



static SituationError pattern_ensure_params_ssbo(void) {
    if (s_pattern_params_ssbo.generation != 0) {
        return SITUATION_SUCCESS;
    }
    uint8_t zero[SIT_HARNESS_PATTERN_PARAMS_SSBO_SIZE] = {0};
    return SituationCreateBuffer(
        SIT_HARNESS_PATTERN_PARAMS_SSBO_SIZE,
        zero,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,
        &s_pattern_params_ssbo);
}

static SituationError pattern_draw_cfg(SituationShader shader, SituationMesh mesh, const SitVdStandbyConfig* cfg) {
    SituationError err = pattern_ensure_cfg_ubo();
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    err = pattern_ensure_params_ssbo();
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    uint8_t header_bytes[SIT_HARNESS_PATTERN_CONFIG_UBO_SIZE];
    uint8_t params_bytes[SIT_HARNESS_PATTERN_PARAMS_SSBO_SIZE];
    sit_harness_pattern_pack_config_std140(header_bytes, cfg);
    sit_harness_pattern_pack_params_std430(params_bytes, cfg);

    SituationCommandBuffer cmd = graphics_test_begin_frame();
    if (!cmd) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    return sit_harness_test_pattern_draw_fullscreen_config(
        cmd, shader, mesh, s_pattern_cfg_ubo, s_pattern_params_ssbo,
        header_bytes, params_bytes,
        SIT_HARNESS_PATTERN_CONFIG_UBO_SIZE, SIT_HARNESS_PATTERN_PARAMS_SSBO_SIZE);
}



static SituationError pattern_draw_smpte_vd(

    SituationShader shader,

    SituationMesh mesh,

    const SitHarnessPatternSmpteVdConfig* cfg)

{

    SituationError err = pattern_ensure_smpte_vd_ubo();

    if (err != SITUATION_SUCCESS) {

        return err;

    }



    uint8_t ubo_bytes[SIT_HARNESS_PATTERN_SMPTE_VD_UBO_SIZE];

    sit_harness_pattern_pack_smpte_vd_std140(ubo_bytes, cfg);



    SituationCommandBuffer cmd = graphics_test_begin_frame();

    if (!cmd) {

        return SITUATION_ERROR_RENDER_COMMAND_FAILED;

    }



    return sit_harness_test_pattern_draw_fullscreen_ubo(

        cmd, shader, mesh, s_pattern_smpte_vd_ubo, ubo_bytes, SIT_HARNESS_PATTERN_SMPTE_VD_UBO_SIZE);

}



static void pattern_run_sample(int layer_index, int px, int py, float exp_r, float exp_g, float exp_b) {

    if (graphics_test_skip_if_no_spirv()) {

        return;

    }

    if (!pattern_embed_ready()) {

        pattern_fail_missing_embed();

        return;

    }



    const float w = (float)sit_test_window_render_width();

    const float h = (float)sit_test_window_render_height();

    SitVdStandbyConfig cfg;

    sit_harness_pattern_config_init_defaults(&cfg, layer_index, w, h);



    SituationShader shader = {0};

    SituationError err = pattern_load_test_pattern_shader(&shader);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = sit_harness_test_pattern_bind_config_resources(shader, false);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    SituationMesh mesh = graphics_test_fullscreen_mesh();

    err = pattern_draw_cfg(shader, mesh, &cfg);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    uint8_t rgba[4] = {0};

    err = graphics_test_read_pixel_rgba(px, py, rgba);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_ASSERT(graphics_test_pixel_approx_float(rgba[0], exp_r, 2));

    SIT_ASSERT(graphics_test_pixel_approx_float(rgba[1], exp_g, 2));

    SIT_ASSERT(graphics_test_pixel_approx_float(rgba[2], exp_b, 2));



    SituationUnloadShader(&shader);

}



void test_pattern_smpte_vd_bar_color(void) {

    if (graphics_test_skip_if_no_spirv()) {

        return;

    }

    if (!pattern_embed_ready()) {

        pattern_fail_missing_embed();

        return;

    }



    const float w = (float)sit_test_window_render_width();

    const float h = (float)sit_test_window_render_height();

    SitHarnessPatternSmpteVdConfig cfg = {w, h};



    SituationShader shader = {0};

    SituationError err = pattern_load_smpte_vd_shader(&shader);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = sit_harness_test_pattern_bind_config_resources(shader, true);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    SituationMesh mesh = graphics_test_fullscreen_mesh();

    err = pattern_draw_smpte_vd(shader, mesh, &cfg);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    int px = (int)(w * (1.5f / 7.0f));

    int py = (int)(h / 6.0f);



    uint8_t rgba[4] = {0};

    err = graphics_test_read_pixel_rgba(px, py, rgba);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_ASSERT(graphics_test_pixel_approx_float(rgba[0], 0.706f, 2));

    SIT_ASSERT(graphics_test_pixel_approx_float(rgba[1], 0.706f, 2));

    SIT_ASSERT(graphics_test_pixel_approx_float(rgba[2], 0.0f, 2));



    SituationUnloadShader(&shader);

}



void test_pattern_smpte_full_bar_color(void) {

    const float w = (float)sit_test_window_render_width();

    const float h = (float)sit_test_window_render_height();

    const float margin_x = 0.1f;
    const float margin_y = 0.1f;
    float content_x = w * margin_x;
    float content_y = h * margin_y;
    float content_w = w * (1.0f - 2.0f * margin_x);
    float content_h = h * (1.0f - 2.0f * margin_y);
    float bar_w = content_w / 7.0f;
    float top_h = content_h * 0.45f;

    int px = (int)(content_x + 1.5f * bar_w);

    int py = (int)(content_y + top_h * 0.5f);

    pattern_run_sample(0, px, py, 192.0f / 255.0f, 192.0f / 255.0f, 0.0f);

}



void test_pattern_checkerboard_corner(void) {

    pattern_run_sample(1, 8, 8, 1.0f, 1.0f, 1.0f);

    pattern_run_sample(1, 40, 8, 0.0f, 0.0f, 0.0f);

}



void test_pattern_grid_line(void) {

    if (graphics_test_skip_if_no_spirv()) {

        return;

    }

    if (!pattern_embed_ready()) {

        pattern_fail_missing_embed();

        return;

    }



    const float w = (float)sit_test_window_render_width();

    const float h = (float)sit_test_window_render_height();

    SitVdStandbyConfig cfg;

    sit_harness_pattern_config_init_defaults(&cfg, 4, w, h);



    SituationShader shader = {0};

    SituationError err = pattern_load_test_pattern_shader(&shader);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = sit_harness_test_pattern_bind_config_resources(shader, false);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    SituationMesh mesh = graphics_test_fullscreen_mesh();

    err = pattern_draw_cfg(shader, mesh, &cfg);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    uint8_t bg[4] = {0};

    uint8_t line[4] = {0};

    err = graphics_test_read_pixel_rgba(17, 17, bg);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = graphics_test_read_pixel_rgba(0, 0, line);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_ASSERT(line[0] + line[1] + line[2] > bg[0] + bg[1] + bg[2]);



    SituationUnloadShader(&shader);

}



void test_pattern_pluge_black_bar(void) {

    const float w = (float)sit_test_window_render_width();

    const float h = (float)sit_test_window_render_height();

    float content_x = w * 0.1f;

    float bar_w = (w * 0.8f) / 10.0f;

    float content_y = h * 0.1f;

    float content_h = h * 0.8f;

    float bar_h = content_h * 0.6f;

    float bar_y = content_y + (content_h - bar_h) * 0.5f;

    int px = (int)(content_x + bar_w * 1.5f + 5.0f);

    int py = (int)(bar_y + bar_h * 0.5f + 7.0f);

    pattern_run_sample(5, px, py, 0.0f, 0.0f, 0.0f);

}



void test_pattern_crosshatch_center(void) {

    const float w = (float)sit_test_window_render_width();

    const float h = (float)sit_test_window_render_height();

    int px = (int)(w * 0.5f);

    int py = (int)(h * 0.5f);

    pattern_run_sample(6, px, py, 1.0f, 1.0f, 1.0f);

}



void test_pattern_convergence_moire_zone(void) {

    if (graphics_test_skip_if_no_spirv()) {

        return;

    }

    if (!pattern_embed_ready()) {

        pattern_fail_missing_embed();

        return;

    }



    const float w = (float)sit_test_window_render_width();

    const float h = (float)sit_test_window_render_height();

    SitVdStandbyConfig cfg;

    sit_harness_pattern_config_init_defaults(&cfg, 2, w, h);



    SituationShader shader = {0};

    SituationError err = pattern_load_test_pattern_shader(&shader);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = sit_harness_test_pattern_bind_config_resources(shader, false);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    SituationMesh mesh = graphics_test_fullscreen_mesh();

    err = pattern_draw_cfg(shader, mesh, &cfg);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    uint8_t outer[4] = {0};

    uint8_t center[4] = {0};

    err = graphics_test_read_pixel_rgba(20, (int)(h * 0.52f), outer);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = graphics_test_read_pixel_rgba((int)(w * 0.5f), (int)(h * 0.52f), center);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_ASSERT(outer[0] != center[0] || outer[1] != center[1]);



    SituationUnloadShader(&shader);

}



void test_pattern_multiburst_bands(void) {

    if (graphics_test_skip_if_no_spirv()) {

        return;

    }

    if (!pattern_embed_ready()) {

        pattern_fail_missing_embed();

        return;

    }



    const float w = (float)sit_test_window_render_width();

    const float h = (float)sit_test_window_render_height();

    SitVdStandbyConfig cfg;

    sit_harness_pattern_config_init_defaults(&cfg, 7, w, h);



    SituationShader shader = {0};

    SituationError err = pattern_load_test_pattern_shader(&shader);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = sit_harness_test_pattern_bind_config_resources(shader, false);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    SituationMesh mesh = graphics_test_fullscreen_mesh();

    err = pattern_draw_cfg(shader, mesh, &cfg);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);



    float content_x = w * 0.1f;

    float content_w = w * 0.8f;

    float band_w = content_w / 6.0f;

    float bar_y = h * 0.1f + ((h * 0.8f) - (h * 0.8f * 0.7f)) * 0.5f + (h * 0.8f * 0.7f) * 0.5f;



    uint8_t b0[4] = {0};

    uint8_t b3[4] = {0};

    int py = (int)bar_y;

    err = graphics_test_read_pixel_rgba((int)(content_x + band_w * 0.5f), py, b0);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = graphics_test_read_pixel_rgba((int)(content_x + band_w * 3.5f), py, b3);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_ASSERT(b0[0] != b3[0] || b0[1] != b3[1]);



    SituationUnloadShader(&shader);
}

void test_pattern_cube_lit_faces(void) {
    if (graphics_test_skip_if_no_spirv()) {
        return;
    }
    if (!pattern_embed_ready()) {
        pattern_fail_missing_embed();
        return;
    }

    const float w = (float)sit_test_window_render_width();
    const float h = (float)sit_test_window_render_height();
    SitVdStandbyConfig cfg;
    sit_harness_pattern_config_init_defaults(&cfg, SIT_VD_STANDBY_3D_GRID, w, h);
    cfg.layer.cube.size = 1.0f;

    SituationShader shader = {0};
    SituationError err = pattern_load_test_pattern_shader(&shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = sit_harness_test_pattern_bind_config_resources(shader, false);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationMesh mesh = graphics_test_fullscreen_mesh();
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t sky[4] = {0};
    uint8_t cube[4] = {0};
    err = graphics_test_read_pixel_rgba(4, 4, sky);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    const int cx = (int)(w * 0.5f);
    const int cy = (int)(h * 0.55f);
    err = graphics_test_read_pixel_rgba(cx, cy, cube);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    /* Background: dark gray (~45). */
    SIT_ASSERT(sky[0] <= 55 && sky[1] <= 55 && sky[2] <= 55);
    /* Lit cube center: brighter than sky. */
    SIT_ASSERT(cube[0] > 80 || cube[1] > 80 || cube[2] > 80);

    int luma_min = 255;
    int luma_max = 0;
    err = graphics_test_begin_screen_probe();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    for (int py = cy - 24; py <= cy + 24; py += 8) {
        for (int px = cx - 32; px <= cx + 32; px += 8) {
            uint8_t rgba[4] = {0};
            err = graphics_test_read_pixel_rgba_probed(px, py, rgba);
            SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
            int luma = (int)rgba[0] + (int)rgba[1] + (int)rgba[2];
            if (luma < luma_min) {
                luma_min = luma;
            }
            if (luma > luma_max) {
                luma_max = luma;
            }
        }
    }
    graphics_test_end_screen_probe();
    /* Multiple lit face tones on the cube (not flat gray). */
    SIT_ASSERT(luma_max - luma_min >= 24);

    SituationUnloadShader(&shader);
}

void test_pattern_compose_checker_plus_smpte(void) {
    if (graphics_test_skip_if_no_spirv()) {
        return;
    }
    if (!pattern_embed_ready()) {
        pattern_fail_missing_embed();
        return;
    }

    const float w = (float)sit_test_window_render_width();
    const float h = (float)sit_test_window_render_height();
    SitVdStandbyConfig cfg;
    sit_harness_pattern_config_init_defaults(&cfg, SIT_VD_STANDBY_SMPTE_BARS, w, h);
    cfg.pattern_layers = (int32_t)(SIT_VD_STANDBY_LAYER_CHECKERBOARD | SIT_VD_STANDBY_LAYER_SMPTE);

    SituationShader shader = {0};
    SituationError err = pattern_load_test_pattern_shader(&shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = sit_harness_test_pattern_bind_config_resources(shader, false);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationMesh mesh = graphics_test_fullscreen_mesh();
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t checker[4] = {0};
    err = graphics_test_read_pixel_rgba(8, 8, checker);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_pixel_approx_float(checker[0], 1.0f, 2));
    SIT_ASSERT(graphics_test_pixel_approx_float(checker[1], 1.0f, 2));
    SIT_ASSERT(graphics_test_pixel_approx_float(checker[2], 1.0f, 2));

    const float margin_x = 0.1f;
    const float margin_y = 0.1f;
    float content_x = w * margin_x;
    float content_y = h * margin_y;
    float content_w = w * (1.0f - 2.0f * margin_x);
    float content_h = h * (1.0f - 2.0f * margin_y);
    float bar_w = content_w / 7.0f;
    float top_h = content_h * 0.45f;
    int px = (int)(content_x + 1.5f * bar_w);
    int py = (int)(content_y + top_h * 0.5f);

    uint8_t bar[4] = {0};
    err = graphics_test_read_pixel_rgba(px, py, bar);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_pixel_approx_float(bar[0], 192.0f / 255.0f, 2));
    SIT_ASSERT(graphics_test_pixel_approx_float(bar[1], 192.0f / 255.0f, 2));
    SIT_ASSERT(graphics_test_pixel_approx_float(bar[2], 0.0f, 2));

    SituationUnloadShader(&shader);
}

void test_pattern_zero_layers_noise(void) {
    if (graphics_test_skip_if_no_spirv()) {
        return;
    }
    if (!pattern_embed_ready()) {
        pattern_fail_missing_embed();
        return;
    }

    const float w = (float)sit_test_window_render_width();
    const float h = (float)sit_test_window_render_height();
    SitVdStandbyConfig cfg;
    SituationVdStandbyConfigInitDefaults(&cfg, -1, w, h);
    cfg.pattern_layers = 0;
    cfg.snow.noise_frame_seed = 1.0f;

    SituationShader shader = {0};
    SituationError err = pattern_load_test_pattern_shader(&shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = sit_harness_test_pattern_bind_config_resources(shader, false);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationMesh mesh = graphics_test_fullscreen_mesh();
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t a[4] = {0};
    err = graphics_test_read_pixel_rgba(17, 23, a);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    cfg.snow.noise_frame_seed = 987654.0f;
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t b[4] = {0};
    err = graphics_test_read_pixel_rgba(17, 23, b);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    /* Animated snow: same pixel, different frame seed -> different value. */
    SIT_ASSERT(a[0] != b[0] || a[1] != b[1] || a[2] != b[2]);
    /* Not the old flat bg_dark_gray (45, 45, 45). */
    SIT_ASSERT(a[0] != 45 || a[1] != 45 || a[2] != 45);
    /* Grayscale noise. */
    SIT_ASSERT(abs((int)a[0] - (int)a[1]) <= 2);
    SIT_ASSERT(abs((int)a[1] - (int)a[2]) <= 2);

    SituationUnloadShader(&shader);
}

void test_pattern_chroma_snow(void) {
    if (graphics_test_skip_if_no_spirv()) {
        return;
    }
    if (!pattern_embed_ready()) {
        pattern_fail_missing_embed();
        return;
    }

    const float w = (float)sit_test_window_render_width();
    const float h = (float)sit_test_window_render_height();
    SitVdStandbyConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.width = w;
    cfg.height = h;
    cfg.pattern_layers = (int32_t)SIT_VD_STANDBY_LAYER_CHROMA_SNOW;
    cfg.snow.noise_frame_seed = 1000.0f;

    SituationShader shader = {0};
    SituationError err = pattern_load_test_pattern_shader(&shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = sit_harness_test_pattern_bind_config_resources(shader, false);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationMesh mesh = graphics_test_fullscreen_mesh();
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t rgba[4] = {0};
    err = graphics_test_read_pixel_rgba(17, 23, rgba);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    /* Chroma: channels may differ (not forced grayscale). */
    SIT_ASSERT(rgba[0] != rgba[1] || rgba[1] != rgba[2] || rgba[0] != rgba[2]);

    SituationUnloadShader(&shader);
}

void test_pattern_config_defaults(void) {
    const float w = 640.0f;
    const float h = 480.0f;
    SitVdStandbyConfig cfg;
    sit_harness_pattern_config_init_defaults(&cfg, SIT_VD_STANDBY_CHECKERBOARD, w, h);

    SIT_ASSERT_EQ(cfg.pattern_layers, (int32_t)SIT_VD_STANDBY_LAYER_CHECKERBOARD);
    SIT_ASSERT_EQ(cfg.width, w);
    SIT_ASSERT_EQ(cfg.height, h);
    SIT_ASSERT_EQ(cfg.layer_stack_count, SIT_VD_STANDBY_DEFAULT_STACK_COUNT);
    SIT_ASSERT_EQ(cfg.layer_stack[0], 1);
    SIT_ASSERT_EQ(cfg.layer_stack[1], 2);
    SIT_ASSERT_EQ(cfg.layer_stack[2], 3);
    SIT_ASSERT_EQ(cfg.layer_stack[3], 5);
    SIT_ASSERT_EQ(cfg.layer_stack[4], 7);
    SIT_ASSERT_EQ(cfg.layer_stack[5], 6);
    SIT_ASSERT_EQ(cfg.layer_stack[6], 0);
    SIT_ASSERT_EQ(cfg.layer_stack[7], 4);
    SIT_ASSERT_EQ(cfg.layer_stack[8], SIT_VD_STANDBY_STACK_UNUSED);

    SIT_ASSERT(cfg.layer.checker.tile_size_x == 32.0f);
    SIT_ASSERT(cfg.layer.checker.tile_size_y == 32.0f);
    SIT_ASSERT(cfg.layer.cube.size == 1.0f);
    SIT_ASSERT(cfg.layer.convergence.stripe_width == 16.0f);
    SIT_ASSERT(cfg.layer.smpte.show_overlay_circle == 1);

    uint8_t header[SIT_HARNESS_PATTERN_CONFIG_UBO_SIZE];
    uint8_t ssbo[SIT_HARNESS_PATTERN_PARAMS_SSBO_SIZE];
    sit_harness_pattern_pack_config_std140(header, &cfg);
    sit_harness_pattern_pack_params_std430(ssbo, &cfg);
    int32_t packed_layers = 0;
    int32_t stack_count = 0;
    float packed_checker_x = 0.0f;
    float packed_cube_size = 0.0f;
    memcpy(&packed_layers, header + SIT_HARNESS_PATTERN_UBO_OFF_PATTERN_LAYERS, sizeof(packed_layers));
    memcpy(&stack_count, header + SIT_VD_STANDBY_HEADER_UBO_OFF_STACK_COUNT, sizeof(stack_count));
    memcpy(&packed_checker_x, ssbo + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CHECKER, sizeof(packed_checker_x));
    memcpy(&packed_cube_size, ssbo + SIT_VD_STANDBY_PARAMS_SSBO_OFF_CUBE, sizeof(packed_cube_size));
    SIT_ASSERT_EQ(packed_layers, cfg.pattern_layers);
    SIT_ASSERT_EQ(stack_count, (int32_t)cfg.layer_stack_count);
    SIT_ASSERT(packed_checker_x == 32.0f);
    SIT_ASSERT(packed_cube_size == 1.0f);
}

void test_pattern_layer_params_checker_tile(void) {
    if (graphics_test_skip_if_no_spirv()) {
        return;
    }
    const float w = (float)sit_test_window_render_width();
    const float h = (float)sit_test_window_render_height();
    if (!pattern_embed_ready()) {
        pattern_fail_missing_embed();
        return;
    }
    SituationMesh mesh = graphics_test_fullscreen_mesh();
    SIT_ASSERT(mesh.generation != 0);

    SitVdStandbyConfig cfg;
    sit_harness_pattern_config_init_defaults(&cfg, SIT_VD_STANDBY_CHECKERBOARD, w, h);
    cfg.layer.checker.tile_size_x = 8.0f;
    cfg.layer.checker.tile_size_y = 8.0f;

    SituationShader shader = {0};
    SituationError err = pattern_load_test_pattern_shader(&shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = sit_harness_test_pattern_bind_config_resources(shader, false);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t white[4] = {0};
    err = graphics_test_read_pixel_rgba(8, 8, white);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(white[0] > 200);

    cfg.layer.checker.tile_size_x = 32.0f;
    cfg.layer.checker.tile_size_y = 32.0f;
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t black[4] = {0};
    err = graphics_test_read_pixel_rgba(40, 8, black);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(black[0] < 32);

    SituationUnloadShader(&shader);
}

void test_pattern_layer_params_convergence_stripe(void) {
    if (graphics_test_skip_if_no_spirv()) {
        return;
    }
    const float w = (float)sit_test_window_render_width();
    const float h = (float)sit_test_window_render_height();
    if (!pattern_embed_ready()) {
        pattern_fail_missing_embed();
        return;
    }
    SituationMesh mesh = graphics_test_fullscreen_mesh();
    SIT_ASSERT(mesh.generation != 0);

    SitVdStandbyConfig cfg;
    sit_harness_pattern_config_init_defaults(&cfg, SIT_VD_STANDBY_CONVERGENCE, w, h);
    cfg.layer.convergence.stripe_width = 4.0f;

    SituationShader shader = {0};
    SituationError err = pattern_load_test_pattern_shader(&shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = sit_harness_test_pattern_bind_config_resources(shader, false);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t narrow[4] = {0};
    err = graphics_test_read_pixel_rgba(4, 64, narrow);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    cfg.layer.convergence.stripe_width = 32.0f;
    err = pattern_draw_cfg(shader, mesh, &cfg);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t wide[4] = {0};
    err = graphics_test_read_pixel_rgba(4, 64, wide);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(narrow[0] != wide[0]);

    SituationUnloadShader(&shader);
}

#if defined(SITUATION_USE_VULKAN) && defined(SITUATION_ENABLE_SHADER_COMPILER)

void test_pattern_runtime_include_compile(void) {

    static const char* vs =

        "#version 450\n"

        "layout(location=0) in vec3 aPos;\n"

        "void main() { gl_Position = vec4(aPos, 1.0); }\n";

    static const char* fs =

        "#version 450 core\n"

        "#include \"sit/gpu/test_patterns/sit_tp_config_ubo.glslh\"\n"

        "#include \"sit/gpu/test_patterns/sit_test_patterns.glslh\"\n"

        "layout(location = 0) out vec4 fragColor;\n"

        "void main() {\n"

        "  SitTpConfig cfg = sit_tp_config_from_ubo();\n"

        "  vec2 uv = gl_FragCoord.xy / vec2(max(cfg.width, 1.0), max(cfg.height, 1.0));\n"

        "  fragColor = vec4(sit_tp_sample(uv, cfg, sit_tp_default_palette()), 1.0);\n"

        "}\n";



    SituationShader shader = {0};

    SituationError err = SituationLoadShaderFromMemory(vs, fs, &shader);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadShader(&shader);



    /* V15 — second compile of same include-expanded GLSL must hit Layer A (< 50 ms). */

    double t0 = sit_get_time_seconds();

    err = SituationLoadShaderFromMemory(vs, fs, &shader);

    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    double reload_ms = (sit_get_time_seconds() - t0) * 1000.0;

    SituationUnloadShader(&shader);

    if (reload_ms >= 50.0) {

        fprintf(stderr,

            "[pattern_runtime_include_compile] FAIL: second load took %.2f ms — expected < 50 ms (Layer A hit)\n",

            reload_ms);

    }

    SIT_ASSERT(reload_ms < 50.0);

    fprintf(stderr, "[pattern_runtime_include_compile] second load: %.3f ms\n", reload_ms);

}

#endif


