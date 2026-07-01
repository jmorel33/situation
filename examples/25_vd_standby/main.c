/***************************************************************************************************
 *  Situation — 25: VD Standby / Test Pattern Explorer
 *
 *  Live exploration of SitVdStandbyConfig on a Virtual Display via the production compositor
 *  (PATTERN idle path) — snow, layers 0–8, stack order, and per-layer parameters.
 *
 *  Build:
 *    build\build_examples.bat static-opengl  25_vd_standby
 *    build\build_examples.bat static-vulkan 25_vd_standby
 *
 *  See README.md and doc/guide/test_patterns.md
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>
#include <stdio.h>

#define VD_W 1280.0f
#define VD_H 720.0f

/* VD create default — situation_impl_vd.h (SituationCreateVirtualDisplayFromDesc). */
#define SIT_VD_DEFAULT_SOLID_FALLBACK ((ColorRGBA){13, 38, 102, 255})

/* HUD bar height matches sit_example.h (reference 1280×1024). */
#define SIT_EX_REF_W 1280.0f
#define SIT_EX_REF_H 1024.0f
#define SIT_EX_RX(v) ((v) / SIT_EX_REF_W)
#define SIT_EX_RY(v) ((v) / SIT_EX_REF_H)
#define SIT_EX_HUD_BAR_H_RY SIT_EX_RY(22.0f)
#define SIT_EX_PANEL_LINE_H_RY SIT_EX_RY(18.0f)
#define SIT_EX_PANEL_PAD_RY SIT_EX_RY(8.0f)
#define SIT_EX_PANEL_MARGIN_RX SIT_EX_RX(8.0f)
#define SIT_EX_PANEL_TEXT_PAD_RX SIT_EX_RX(4.0f)
#define SIT_EX_PANEL_CENTER_RY 0.62f
#define SIT_EX_PANEL_BOT_GAP_RY SIT_EX_RY(12.0f)
#define SIT_EX_PANEL_TOP_GAP_RY SIT_EX_RY(8.0f)

static const char* const k_layer_names[] = {
    "SMPTE", "Checker", "Conv", "Grad", "Grid", "PLUGE", "Xhatch", "MBurst", "Cube"
};

static int              g_vd_id = -1;
static SitVdStandbyConfig g_cfg;
static int              g_sel_layer = 1;
static int              g_cfg_dirty = 1;
static SituationVDFallbackMode g_fallback = SITUATION_VD_FALLBACK_PATTERN;
static int              g_live_draw = 0;
static int              g_manual_snow_seed = 0;

static int shift_down(void) {
    return SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT);
}

static float nudge_step(int coarse) {
    return coarse ? 8.0f : 1.0f;
}

static void mark_dirty(void) {
    g_cfg_dirty = 1;
}

static void sync_vd_resolution(void) {
    g_cfg.width = VD_W;
    g_cfg.height = VD_H;
}

static void apply_pattern_config(void) {
    if (g_fallback != SITUATION_VD_FALLBACK_PATTERN) {
        return;
    }
    sync_vd_resolution();
    SituationSetVirtualDisplayPatternConfig(g_vd_id, &g_cfg);
    g_cfg_dirty = 0;
}

static void set_fallback(SituationVDFallbackMode mode) {
    g_fallback = mode;
    SituationSetVirtualDisplayFallbackMode(g_vd_id, mode);
    /* SOLID uses the VD create default deep blue — do not override fallback_color here. */
    if (mode == SITUATION_VD_FALLBACK_PATTERN) {
        mark_dirty();
        apply_pattern_config();
    }
}

static void cycle_fallback(void) {
    if (g_fallback == SITUATION_VD_FALLBACK_PATTERN) {
        set_fallback(SITUATION_VD_FALLBACK_COLORBURST);
    } else if (g_fallback == SITUATION_VD_FALLBACK_COLORBURST) {
        set_fallback(SITUATION_VD_FALLBACK_SOLID);
    } else {
        set_fallback(SITUATION_VD_FALLBACK_PATTERN);
    }
}

static void clear_calibration_layers(void) {
    g_cfg.pattern_layers &= ~(int32_t)SIT_VD_STANDBY_LAYER_CALIBRATION_MASK;
    g_manual_snow_seed = 0;
    mark_dirty();
}

static void toggle_chroma_snow(void) {
    bool on = (g_cfg.pattern_layers & (int32_t)SIT_VD_STANDBY_LAYER_CHROMA_SNOW) != 0;
    on = !on;
    g_cfg.snow.chroma = on ? 1 : 0;
    if (on) {
        g_cfg.pattern_layers |= (int32_t)SIT_VD_STANDBY_LAYER_CHROMA_SNOW;
    } else {
        g_cfg.pattern_layers &= ~(int32_t)SIT_VD_STANDBY_LAYER_CHROMA_SNOW;
    }
    mark_dirty();
}

static void stack_swap_selected(int dir) {
    uint8_t count = g_cfg.layer_stack_count;
    if (count < 2u) {
        return;
    }
    for (uint8_t i = 0; i < count; ++i) {
        if (g_cfg.layer_stack[i] != (uint8_t)g_sel_layer) {
            continue;
        }
        int j = (int)i + dir;
        if (j < 0 || j >= (int)count) {
            return;
        }
        uint8_t tmp = g_cfg.layer_stack[i];
        g_cfg.layer_stack[i] = g_cfg.layer_stack[j];
        g_cfg.layer_stack[j] = tmp;
        mark_dirty();
        return;
    }
}

static void load_preset(int preset) {
    SituationVdStandbyConfigInitDefaults(&g_cfg, -1, VD_W, VD_H);
    g_manual_snow_seed = 0;
    switch (preset) {
        case 1:
            break;
        case 2:
            g_cfg.pattern_layers = (int32_t)SIT_VD_STANDBY_LAYER_SMPTE;
            break;
        case 3:
            g_cfg.pattern_layers = (int32_t)(SIT_VD_STANDBY_LAYER_SMPTE | SIT_VD_STANDBY_LAYER_CHECKERBOARD);
            break;
        case 4:
            g_cfg.pattern_layers = (int32_t)SIT_VD_STANDBY_LAYER_CALIBRATION_MASK;
            break;
        default:
            break;
    }
    mark_dirty();
}

static void reset_defaults(void) {
    SituationVdStandbyConfigInitDefaults(&g_cfg, -1, VD_W, VD_H);
    g_manual_snow_seed = 0;
    g_sel_layer = 1;
    mark_dirty();
}

static void nudge_params(int coarse) {
    float step = nudge_step(coarse);
    SitVdStandbyLayerParams* lp = &g_cfg.layer;

    if ((g_cfg.pattern_layers & (int32_t)SIT_VD_STANDBY_LAYER_CALIBRATION_MASK) == 0) {
        if (SituationIsKeyPressed(SIT_KEY_N)) {
            g_cfg.snow.noise_frame_seed -= step * 100.0f;
            g_manual_snow_seed = 1;
            mark_dirty();
        }
        if (SituationIsKeyPressed(SIT_KEY_M)) {
            g_cfg.snow.noise_frame_seed += step * 100.0f;
            g_manual_snow_seed = 1;
            mark_dirty();
        }
        return;
    }

    switch (g_sel_layer) {
        case 0:
            if (SituationIsKeyPressed(SIT_KEY_Q)) { lp->smpte.content_margin_x -= step * 0.005f; mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_A)) { lp->smpte.content_margin_x += step * 0.005f; mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_W)) { lp->smpte.content_margin_y -= step * 0.005f; mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_S)) { lp->smpte.content_margin_y += step * 0.005f; mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_O)) {
                lp->smpte.show_overlay_circle = lp->smpte.show_overlay_circle ? 0 : 1;
                mark_dirty();
            }
            break;
        case 1:
            if (SituationIsKeyPressed(SIT_KEY_Q)) { lp->checker.tile_size_x = fmaxf(1.0f, lp->checker.tile_size_x - step); mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_A)) { lp->checker.tile_size_x += step; mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_W)) { lp->checker.tile_size_y = fmaxf(1.0f, lp->checker.tile_size_y - step); mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_S)) { lp->checker.tile_size_y += step; mark_dirty(); }
            break;
        case 2:
            if (SituationIsKeyPressed(SIT_KEY_Q)) { lp->convergence.stripe_width = fmaxf(1.0f, lp->convergence.stripe_width - step); mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_A)) { lp->convergence.stripe_width += step; mark_dirty(); }
            break;
        case 4:
            if (SituationIsKeyPressed(SIT_KEY_Q)) { lp->grid.spacing_px = fmaxf(0.0f, lp->grid.spacing_px - step); mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_A)) { lp->grid.spacing_px += step; mark_dirty(); }
            break;
        case 5:
            if (SituationIsKeyPressed(SIT_KEY_Q)) { lp->pluge.safe_margin = fmaxf(0.0f, lp->pluge.safe_margin - step * 0.01f); mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_A)) { lp->pluge.safe_margin += step * 0.01f; mark_dirty(); }
            break;
        case 6:
            if (SituationIsKeyPressed(SIT_KEY_Q)) { lp->crosshatch.grid_nx = (int32_t)fmaxf(2.0f, (float)lp->crosshatch.grid_nx - step); mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_A)) { lp->crosshatch.grid_nx += (int32_t)step; mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_W)) { lp->crosshatch.grid_ny = (int32_t)fmaxf(2.0f, (float)lp->crosshatch.grid_ny - step); mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_S)) { lp->crosshatch.grid_ny += (int32_t)step; mark_dirty(); }
            break;
        case 7:
            if (SituationIsKeyPressed(SIT_KEY_Q)) { lp->multiburst.num_frequencies -= (int32_t)step; if (lp->multiburst.num_frequencies < 0) lp->multiburst.num_frequencies = 0; mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_A)) { lp->multiburst.num_frequencies += (int32_t)step; if (lp->multiburst.num_frequencies > 6) lp->multiburst.num_frequencies = 6; mark_dirty(); }
            break;
        case 8:
            if (SituationIsKeyPressed(SIT_KEY_Q)) { lp->cube.size = fmaxf(0.25f, lp->cube.size - step * 0.1f); mark_dirty(); }
            if (SituationIsKeyPressed(SIT_KEY_A)) { lp->cube.size += step * 0.1f; mark_dirty(); }
            break;
        default:
            break;
    }
}

static void handle_input(void) {
    if (SituationIsKeyPressed(SIT_KEY_TAB)) {
        g_sel_layer = (g_sel_layer + 1) % 9;
    }
    if (SituationIsKeyPressed(SIT_KEY_MINUS)) {
        clear_calibration_layers();
    }
    if (SituationIsKeyPressed(SIT_KEY_C)) {
        toggle_chroma_snow();
    }
    if (SituationIsKeyPressed(SIT_KEY_R)) {
        reset_defaults();
    }
    if (SituationIsKeyPressed(SIT_KEY_F)) {
        cycle_fallback();
    }
    if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
        g_live_draw = !g_live_draw;
        SituationSetVirtualDisplayIdleThreshold(g_vd_id, g_live_draw ? 0.35 : 0.0);
    }
    if (SituationIsKeyPressed(SIT_KEY_LEFT_BRACKET)) {
        stack_swap_selected(-1);
    }
    if (SituationIsKeyPressed(SIT_KEY_RIGHT_BRACKET)) {
        stack_swap_selected(1);
    }

    if (shift_down()) {
        for (int p = 1; p <= 4; ++p) {
            if (SituationIsKeyPressed(SIT_KEY_0 + p)) {
                load_preset(p);
            }
        }
    } else {
        for (int layer = 0; layer <= 8; ++layer) {
            if (SituationIsKeyPressed(SIT_KEY_0 + layer)) {
                uint32_t bit = SituationVdStandbyLayerBit(layer);
                bool on = (g_cfg.pattern_layers & (int32_t)bit) != 0;
                SituationVdStandbyToggleLayer(&g_cfg, layer, !on);
                mark_dirty();
            }
        }
    }

    nudge_params(shift_down() ? 1 : 0);
}

static const char* fallback_name(SituationVDFallbackMode mode) {
    switch (mode) {
        case SITUATION_VD_FALLBACK_COLORBURST: return "COLORBURST";
        case SITUATION_VD_FALLBACK_SOLID:      return "SOLID";
        default:                               return "PATTERN";
    }
}

static void draw_text(SituationCommandBuffer cmd, const char* text, float x, float y, ColorRGBA col) {
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, text, (Vector2){{x, y}}, 16.0f, 1.0f, col);
}

static void draw_fill(SituationCommandBuffer cmd, float x, float y, float w, float h, Vector4 col) {
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
}

static float status_panel_y0(int line_count) {
    float sh = (float)SituationGetRenderHeight();
    float line_h = sh * SIT_EX_PANEL_LINE_H_RY;
    float pad = sh * SIT_EX_PANEL_PAD_RY;
    float bar_h = sh * SIT_EX_HUD_BAR_H_RY;
    float panel_h = pad * 2.0f + (float)line_count * line_h;
    float bot_limit = sh - bar_h - sh * SIT_EX_PANEL_BOT_GAP_RY;
    float center_y = sh * SIT_EX_PANEL_CENTER_RY;
    float y0 = center_y - panel_h * 0.5f;
    if (y0 + panel_h > bot_limit) {
        y0 = bot_limit - panel_h;
    }
    if (y0 < bar_h + sh * SIT_EX_PANEL_TOP_GAP_RY) {
        y0 = bar_h + sh * SIT_EX_PANEL_TOP_GAP_RY;
    }
    return y0;
}

static void draw_status_hud(SituationCommandBuffer cmd) {
    char line1[160];
    char line2[200];
    char line3[200];
    const char* hint1 = "0-8:layers  -:snow  C:chroma  Tab:sel  [/]:stack  F:mode  Space:live";
    const char* hint2 = "Shift+1-4:preset  Q/A/W/S params  R:reset";
    const int line_count = 5;
    float sw = (float)SituationGetRenderWidth();
    float sh = (float)SituationGetRenderHeight();
    float x = sw * SIT_EX_PANEL_MARGIN_RX;
    float line_h = sh * SIT_EX_PANEL_LINE_H_RY;
    float pad = sh * SIT_EX_PANEL_PAD_RY;
    float text_pad = sw * SIT_EX_PANEL_TEXT_PAD_RX;
    float y0 = status_panel_y0(line_count);
    float panel_w = sw - sw * SIT_EX_PANEL_MARGIN_RX * 2.0f;
    float panel_h = pad * 2.0f + (float)line_count * line_h;
    float text_y = y0 + pad;

    snprintf(line1, sizeof(line1),
        "mode:%s  layers:0x%03X  sel:%d %s  live:%s",
        fallback_name(g_fallback),
        (unsigned)(g_cfg.pattern_layers & (int32_t)SIT_VD_STANDBY_LAYER_CALIBRATION_MASK),
        g_sel_layer, k_layer_names[g_sel_layer],
        g_live_draw ? "ON" : "OFF");

    line2[0] = '\0';
    if (g_fallback == SITUATION_VD_FALLBACK_PATTERN) {
        int off = snprintf(line2, sizeof(line2), "stack:[");
        for (uint8_t i = 0; i < g_cfg.layer_stack_count && i < 9u; ++i) {
            if (i > 0) {
                off += snprintf(line2 + off, sizeof(line2) - (size_t)off, ",");
            }
            off += snprintf(line2 + off, sizeof(line2) - (size_t)off, "%u", (unsigned)g_cfg.layer_stack[i]);
        }
        snprintf(line2 + off, sizeof(line2) - (size_t)off, "]");
    } else if (g_fallback == SITUATION_VD_FALLBACK_SOLID) {
        snprintf(line2, sizeof(line2), "solid tint RGB(%u,%u,%u) — VD create default",
            (unsigned)SIT_VD_DEFAULT_SOLID_FALLBACK.r,
            (unsigned)SIT_VD_DEFAULT_SOLID_FALLBACK.g,
            (unsigned)SIT_VD_DEFAULT_SOLID_FALLBACK.b);
    }

    if (g_fallback == SITUATION_VD_FALLBACK_SOLID) {
        snprintf(line3, sizeof(line3), "Press F to return to PATTERN or COLORBURST");
    } else switch (g_sel_layer) {
        case 0:
            snprintf(line3, sizeof(line3), "SMPTE margin %.3f,%.3f  overlay:%d",
                g_cfg.layer.smpte.content_margin_x, g_cfg.layer.smpte.content_margin_y,
                g_cfg.layer.smpte.show_overlay_circle);
            break;
        case 1:
            snprintf(line3, sizeof(line3), "Checker tile %.0fx%.0f",
                g_cfg.layer.checker.tile_size_x, g_cfg.layer.checker.tile_size_y);
            break;
        case 2:
            snprintf(line3, sizeof(line3), "Convergence stripe %.0f px", g_cfg.layer.convergence.stripe_width);
            break;
        case 4:
            snprintf(line3, sizeof(line3), "Grid spacing %.1f px (0=auto)", g_cfg.layer.grid.spacing_px);
            break;
        case 5:
            snprintf(line3, sizeof(line3), "PLUGE margin %.2f", g_cfg.layer.pluge.safe_margin);
            break;
        case 6:
            snprintf(line3, sizeof(line3), "Crosshatch grid %dx%d",
                g_cfg.layer.crosshatch.grid_nx, g_cfg.layer.crosshatch.grid_ny);
            break;
        case 7:
            snprintf(line3, sizeof(line3), "Multiburst bands %d", g_cfg.layer.multiburst.num_frequencies);
            break;
        case 8:
            snprintf(line3, sizeof(line3), "Cube size %.2f", g_cfg.layer.cube.size);
            break;
        default:
            if ((g_cfg.pattern_layers & (int32_t)SIT_VD_STANDBY_LAYER_CALIBRATION_MASK) == 0) {
                snprintf(line3, sizeof(line3), "Snow chroma:%s  seed:%s",
                    (g_cfg.pattern_layers & (int32_t)SIT_VD_STANDBY_LAYER_CHROMA_SNOW) ? "ON" : "OFF",
                    g_manual_snow_seed ? "manual" : "auto");
            } else {
                snprintf(line3, sizeof(line3), "Layer 3 (Gradients): preset-only in v1");
            }
            break;
    }

    draw_fill(cmd, x, y0, panel_w, panel_h, (Vector4){{0.0f, 0.0f, 0.0f, 0.72f}});

    draw_text(cmd, line1, x + text_pad, text_y, (ColorRGBA){220, 240, 255, 255});
    text_y += line_h;
    if (line2[0]) {
        draw_text(cmd, line2, x + text_pad, text_y, (ColorRGBA){180, 200, 220, 255});
        text_y += line_h;
    } else {
        text_y += line_h;
    }
    draw_text(cmd, line3, x + text_pad, text_y, (ColorRGBA){160, 190, 170, 255});
    text_y += line_h;
    draw_text(cmd, hint1, x + text_pad, text_y, (ColorRGBA){200, 200, 255, 220});
    text_y += line_h;
    draw_text(cmd, hint2, x + text_pad, text_y, (ColorRGBA){130, 130, 170, 220});
}

static void draw_live_quad(SituationCommandBuffer cmd) {
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){VD_W * 0.5f - 80.0f, VD_H * 0.5f - 60.0f, 0.0f});
    glm_scale(m, (vec3){160.0f, 120.0f, 1.0f});
    SituationCmdDrawQuad(cmd, m, (Vector4){{0.15f, 0.55f, 0.95f, 1.0f}});
}

static bool init_vd(void) {
    SituationError err = SituationCreateVirtualDisplayEx(
        (Vector2){{VD_W, VD_H}},
        1.0, 0,
        SITUATION_SCALING_FIT,
        SITUATION_BLEND_NONE,
        SITUATION_VD_FLAG_NONE,
        &g_vd_id);
    if (err != SITUATION_SUCCESS || g_vd_id < 0) {
        fprintf(stderr, "[25] CreateVirtualDisplayEx failed\n");
        return false;
    }

    SituationSetVirtualDisplayIdleThreshold(g_vd_id, 0.0);
    SituationVdStandbyConfigInitDefaults(&g_cfg, -1, VD_W, VD_H);
    set_fallback(SITUATION_VD_FALLBACK_PATTERN);
    return true;
}

int main(int argc, char** argv) {
    if (SitExample_Init(argc, argv, "25 — VD Standby Pattern Explorer") != SITUATION_SUCCESS) {
        return -1;
    }

    if (!init_vd()) {
        SitExample_Shutdown();
        return -1;
    }

    printf("25_vd_standby — VD test pattern explorer (1280x720)\n");
    printf("  0-8 toggle layers  - snow  C chroma  Tab select  [/] stack  QASW nudge  Shift+1-4 presets\n");

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) {
            break;
        }
        if (SituationIsAppPaused()) {
            continue;
        }

        handle_input();
        if (g_cfg_dirty) {
            apply_pattern_config();
        }

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            continue;
        }

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

        if (g_live_draw) {
            SituationRenderPassInfo vd_rp = {0};
            vd_rp.display_id = g_vd_id;
            vd_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
            vd_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
            vd_rp.color_attachment.clear.color = (ColorRGBA){12, 12, 24, 255};
            vd_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
            vd_rp.depth_attachment.clear.depth = 1.0f;
            SituationCmdBeginRenderPass(cmd, &vd_rp);
            draw_live_quad(cmd);
            SituationCmdEndRenderPass(cmd);
        }

        SituationRenderPassInfo main_rp = {0};
        main_rp.display_id = -1;
        main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
        main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        main_rp.depth_attachment.clear.depth = 1.0f;

        SituationCmdBeginRenderPass(cmd, &main_rp);
        SituationRenderVirtualDisplays(cmd);
        draw_status_hud(cmd);
        SitExample_DrawHUD(cmd, "25 — VD Standby Pattern Explorer", NULL);
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    SituationDestroyVirtualDisplay(g_vd_id);
    SitExample_Shutdown();
    return 0;
}
