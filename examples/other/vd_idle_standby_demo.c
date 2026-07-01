/*******************************************************************************
 * Situation — Virtual Display idle standby demo
 *
 * Renders a spinning cube into a Virtual Display. Stop drawing to the VD and,
 * after idle_threshold_seconds, the compositor shows SOLID or SMPTE COLORBURST
 * standby instead of a stale texture.
 *
 * Build:
 *   build_examples.bat opengl vd_idle_standby_demo
 *   build_examples.bat vulkan vd_idle_standby_demo
 *
 * Controls:
 *   SPACE     Toggle live cube render (off → idle standby after threshold)
 *   F         Toggle standby: SOLID / COLORBURST
 *   C         Cycle SOLID standby color
 *   F11       Toggle fullscreen (Alt+Enter also works)
 *   ESC       Exit
 ******************************************************************************/

#include "situation.h"
#include "font_data.h"
#include <cglm/cglm.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define VD_W 1280
#define VD_H 720
#define WIN_W 1280
#define WIN_H 720
#define HUD_FONT_PX 16.0f
#define IDLE_THRESHOLD_S 0.35

static SituationFont g_font;
static int g_vd_id = -1;
static float g_rotation = 0.0f;
static bool g_render_live = true;
static SituationVDFallbackMode g_fallback_mode = SITUATION_VD_FALLBACK_SOLID;
static int g_solid_color_index = 0;

static const ColorRGBA g_solid_colors[] = {
    {13, 38, 102, 255},
    {20, 20, 28, 255},
    {32, 96, 48, 255},
    {120, 24, 24, 255},
    {140, 90, 20, 255},
    {48, 48, 56, 255},
};

static void draw_spinning_cube(SituationCommandBuffer cmd) {
    /* Original demo: 80/120/4 px at 640x480. Scale by both axes so 720p height
     * doesn't clip corners before the VD border; leave ~8% rotation slack. */
    const float cx = (float)VD_W * 0.5f;
    const float cy = (float)VD_H * 0.5f;
    const float s = fminf((float)VD_W / 640.0f, (float)VD_H / 480.0f) * 0.92f;
    const float half = 80.0f * s;
    const float face = 120.0f * s;
    const float depth = 4.0f * s;

    static const Vector4 face_colors[6] = {
        {{1.0f, 0.2f, 0.2f, 1.0f}},
        {{0.2f, 1.0f, 0.3f, 1.0f}},
        {{0.2f, 0.4f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 0.2f, 1.0f}},
        {{1.0f, 0.2f, 1.0f, 1.0f}},
        {{0.2f, 1.0f, 1.0f, 1.0f}},
    };

    for (int fi = 0; fi < 6; ++fi) {
        mat4 model;
        glm_mat4_identity(model);
        glm_translate(model, (vec3){cx, cy, 0.0f});
        glm_rotate(model, g_rotation, (vec3){0.0f, 1.0f, 0.0f});
        glm_rotate(model, g_rotation * 0.65f, (vec3){1.0f, 0.0f, 0.0f});

        vec3 offset = {0.0f, 0.0f, 0.0f};
        vec3 rot_axis = {0.0f, 0.0f, 0.0f};
        float rot_angle = 0.0f;
        switch (fi) {
            case 0: offset[2] = half; break;
            case 1: offset[2] = -half; rot_axis[1] = 1.0f; rot_angle = (float)M_PI; break;
            case 2: offset[0] = half; rot_axis[1] = 1.0f; rot_angle = (float)M_PI * 0.5f; break;
            case 3: offset[0] = -half; rot_axis[1] = 1.0f; rot_angle = -(float)M_PI * 0.5f; break;
            case 4: offset[1] = half; rot_axis[0] = 1.0f; rot_angle = -(float)M_PI * 0.5f; break;
            case 5: offset[1] = -half; rot_axis[0] = 1.0f; rot_angle = (float)M_PI * 0.5f; break;
        }
        glm_translate(model, offset);
        if (rot_angle != 0.0f) {
            glm_rotate(model, rot_angle, rot_axis);
        }
        glm_scale(model, (vec3){face, face, depth});
        SituationCmdDrawQuad(cmd, model, face_colors[fi]);
    }
}

static float hud_font_size(void) {
    return HUD_FONT_PX;
}

static void hud_line(SituationCommandBuffer cmd, const char* text, float x, float y, ColorRGBA color) {
    SituationCmdDrawTextEx(cmd, g_font, text, (Vector2){{x, y}}, hud_font_size(), 0.0f, color);
}

static void apply_fallback_settings(void) {
    SituationSetVirtualDisplayFallbackMode(g_vd_id, g_fallback_mode);
    SituationSetVirtualDisplayFallbackColor(g_vd_id, g_solid_colors[g_solid_color_index]);
}

static bool fullscreen_toggle_pressed(void) {
    int alt_down = SituationIsKeyDown(SIT_KEY_LEFT_ALT) || SituationIsKeyDown(SIT_KEY_RIGHT_ALT);
    return SituationIsKeyPressed(SIT_KEY_F11) ||
           (alt_down && SituationIsKeyPressed(SIT_KEY_ENTER));
}

static void render_frame(void) {
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        return;
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    if (g_render_live) {
        SituationRenderPassInfo vd_rp = {0};
        vd_rp.display_id = g_vd_id;
        vd_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        vd_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        vd_rp.color_attachment.clear.color = (ColorRGBA){8, 8, 16, 255};
        vd_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        vd_rp.depth_attachment.clear.depth = 1.0f;

        SituationCmdBeginRenderPass(cmd, &vd_rp);
        draw_spinning_cube(cmd);
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

    double seconds_since = 0.0;
    SituationGetVirtualDisplayUpdateInfo(g_vd_id, NULL, NULL, NULL, &seconds_since);

    char line[160];
    snprintf(line, sizeof(line), "VD idle standby demo  |  live: %s  |  fullscreen: %s  |  since update: %.2fs",
        g_render_live ? "ON" : "OFF",
        SituationIsWindowFullscreen() ? "ON" : "OFF",
        seconds_since);
    hud_line(cmd, line, 12.0f, 12.0f, (ColorRGBA){255, 255, 255, 255});

    snprintf(line, sizeof(line), "standby: %s",
        g_fallback_mode == SITUATION_VD_FALLBACK_COLORBURST ? "COLORBURST (SMPTE)" : "SOLID");
    hud_line(cmd, line, 12.0f, 32.0f, (ColorRGBA){200, 220, 255, 255});

    if (g_fallback_mode == SITUATION_VD_FALLBACK_SOLID) {
        ColorRGBA c = g_solid_colors[g_solid_color_index];
        snprintf(line, sizeof(line), "solid color: RGB(%u,%u,%u)", c.r, c.g, c.b);
        hud_line(cmd, line, 12.0f, 52.0f, c);
    }

    hud_line(cmd, "[SPACE] live  [F] standby  [C] solid color  [F11] fullscreen  [ESC] quit",
        12.0f, (float)SituationGetRenderHeight() - 28.0f, (ColorRGBA){180, 180, 180, 255});

    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "VD Idle Standby Demo",
        .window_width = WIN_W,
        .window_height = WIN_H,
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        fprintf(stderr, "SituationInit failed\n");
        return 1;
    }

    SituationLoadBitmapFontFromMemory(ibm_font_8x8, 8, 8, 256, &g_font);

    SituationError err = SituationCreateVirtualDisplay(
        (Vector2){{(float)VD_W, (float)VD_H}},
        1.0, 0,
        SITUATION_SCALING_STRETCH,
        SITUATION_BLEND_NONE,
        &g_vd_id);
    if (err != SITUATION_SUCCESS || g_vd_id < 0) {
        fprintf(stderr, "CreateVirtualDisplay failed\n");
        SituationShutdown();
        return 1;
    }

    SituationConfigureVirtualDisplay(
        g_vd_id,
        (Vector2){{0.0f, 0.0f}},
        1.0f, 0, true, 1.0,
        SITUATION_BLEND_NONE);

    SituationSetVirtualDisplayIdleThreshold(g_vd_id, IDLE_THRESHOLD_S);
    apply_fallback_settings();

    printf("VD idle standby demo\n");
    printf("  SPACE  live render on/off (off -> standby after %.2fs)\n", IDLE_THRESHOLD_S);
    printf("  F      SOLID / COLORBURST\n");
    printf("  C      cycle SOLID color\n");
    printf("  F11    fullscreen (Alt+Enter)\n");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }
        if (fullscreen_toggle_pressed()) {
            SituationToggleFullscreen();
        }
        if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
            g_render_live = !g_render_live;
        }
        if (SituationIsKeyPressed(SIT_KEY_F)) {
            g_fallback_mode = (g_fallback_mode == SITUATION_VD_FALLBACK_SOLID)
                ? SITUATION_VD_FALLBACK_COLORBURST
                : SITUATION_VD_FALLBACK_SOLID;
            apply_fallback_settings();
        }
        if (SituationIsKeyPressed(SIT_KEY_C)) {
            g_solid_color_index = (g_solid_color_index + 1) % (int)(sizeof(g_solid_colors) / sizeof(g_solid_colors[0]));
            apply_fallback_settings();
        }

        g_rotation += SituationGetFrameTime() * 1.2f;
        render_frame();
    }

    SituationDestroyVirtualDisplay(g_vd_id);
    SituationUnloadFont(g_font);
    SituationShutdown();
    return 0;
}
