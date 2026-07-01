/***************************************************************************************************
 *  Situation — 05: Virtual Display Retro CRT
 *
 *  A 320×240 "game screen" rendered into a Virtual Display, integer-scaled and letterboxed on a
 *  black host window. Scanline bands, a bouncing ball, and chunky pixel text live inside the VD.
 *  A second 160×120 VD acts as a corner minimap (composited manually with alpha blending).
 *
 *  What to look for:
 *    - Crisp nearest-neighbor pixels on the main CRT (SITUATION_SCALING_INTEGER)
 *    - Pitch-black letterbox bars around the scaled framebuffer
 *    - Minimap PiP in the bottom-right with a different compositing path (alpha tint)
 *
 *  Keys:
 *    N  — toggle minimap          B  — toggle minimap glow (brighter alpha)
 *    ESC / F9 / F11 / P / F12     — universal hotkeys from sit_example.h
 *
 *  Build:
 *    build\build_examples.bat static-opengl  05_virtual_display_retro
 *    build\build_examples.bat static-vulkan  05_virtual_display_retro
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>

#define VD_MAIN_W 320
#define VD_MAIN_H 240
#define VD_MINI_W 160
#define VD_MINI_H 120
#define MINI_PIP_SCALE 2

static int      g_vd_main = -1;
static int      g_vd_mini = -1;
static float    g_ball_x  = 80.0f;
static float    g_ball_y  = 120.0f;
static float    g_ball_vx = 95.0f;
static float    g_ball_vy = 72.0f;
static int      g_show_mini = 1;
static int      g_mini_glow = 0;

static void draw_rect(SituationCommandBuffer cmd,
                      float x, float y, float w, float h, Vector4 col)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
}

static void draw_scanlines(SituationCommandBuffer cmd, int w, int h, double t)
{
    for (int y = 0; y < h; y += 4) {
        float phase = (float)sin(t * 2.4 + (double)y * 0.07);
        float r = 0.08f + 0.06f * phase;
        float g = 0.05f + 0.04f * sin(t * 1.7 + (double)y * 0.05);
        float b = 0.18f + 0.10f * sin(t * 3.1 + (double)y * 0.09);
        draw_rect(cmd, 0.0f, (float)y, (float)w, 2.0f, (Vector4){{r, g, b, 1.0f}});
    }
}

static void draw_ball(SituationCommandBuffer cmd, float cx, float cy, float radius, Vector4 col)
{
    draw_rect(cmd, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, col);
}

static void draw_vd_text(SituationCommandBuffer cmd,
                         const char* text, float x, float y, ColorRGBA col)
{
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, text,
                           (Vector2){{x, y}}, 8.0f, 0.0f, col);
}

static void draw_main_scene(SituationCommandBuffer cmd, double t)
{
    const int w = VD_MAIN_W;
    const int h = VD_MAIN_H;

    draw_rect(cmd, 0.0f, 0.0f, (float)w, (float)h,
              (Vector4){{0.04f, 0.03f, 0.10f, 1.0f}});
    draw_scanlines(cmd, w, h, t);

    /* Bouncing ball — coordinates stay inside the VD pixel grid */
    const float radius = 6.0f;
    float dt = SituationGetFrameTime();
    g_ball_x += g_ball_vx * dt;
    g_ball_y += g_ball_vy * dt;
    if (g_ball_x < radius) { g_ball_x = radius; g_ball_vx = fabsf(g_ball_vx); }
    if (g_ball_y < radius) { g_ball_y = radius; g_ball_vy = fabsf(g_ball_vy); }
    if (g_ball_x > (float)w - radius) { g_ball_x = (float)w - radius; g_ball_vx = -fabsf(g_ball_vx); }
    if (g_ball_y > (float)h - radius) { g_ball_y = (float)h - radius; g_ball_vy = -fabsf(g_ball_vy); }

    draw_ball(cmd, g_ball_x, g_ball_y, radius,
              (Vector4){{1.0f, 0.82f, 0.12f, 1.0f}});

    draw_vd_text(cmd, "SITUATION VD", 8.0f, 8.0f, (ColorRGBA){120, 255, 180, 255});
    draw_vd_text(cmd, "320x240 CRT", 8.0f, 20.0f, (ColorRGBA){180, 180, 255, 255});
}

static void draw_minimap_scene(SituationCommandBuffer cmd)
{
    const int w = VD_MINI_W;
    const int h = VD_MINI_H;

    draw_rect(cmd, 0.0f, 0.0f, (float)w, (float)h,
              (Vector4){{0.02f, 0.02f, 0.05f, 1.0f}});

    for (int y = 0; y < h; y += 8) {
        draw_rect(cmd, 0.0f, (float)y, (float)w, 1.0f,
                  (Vector4){{0.10f, 0.08f, 0.22f, 1.0f}});
    }

    float mx = g_ball_x * ((float)w / (float)VD_MAIN_W);
    float my = g_ball_y * ((float)h / (float)VD_MAIN_H);
    draw_ball(cmd, mx, my, 3.0f, (Vector4){{1.0f, 0.9f, 0.2f, 1.0f}});

    draw_rect(cmd, 0.0f, 0.0f, (float)w, 2.0f, (Vector4){{0.2f, 0.9f, 0.5f, 1.0f}});
    draw_rect(cmd, 0.0f, (float)h - 2.0f, (float)w, 2.0f, (Vector4){{0.2f, 0.9f, 0.5f, 1.0f}});
    draw_rect(cmd, 0.0f, 0.0f, 2.0f, (float)h, (Vector4){{0.2f, 0.9f, 0.5f, 1.0f}});
    draw_rect(cmd, (float)w - 2.0f, 0.0f, 2.0f, (float)h, (Vector4){{0.2f, 0.9f, 0.5f, 1.0f}});
}

static SituationRenderPassInfo vd_pass(int display_id, ColorRGBA clear)
{
    SituationRenderPassInfo rp = {0};
    rp.display_id = display_id;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = clear;
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;
    return rp;
}

static void draw_minimap_pip(SituationCommandBuffer cmd)
{
    if (!g_show_mini) {
        return;
    }

    SituationTexture mini_tex = {0};
    if (SituationGetVirtualDisplayTexture(g_vd_mini, &mini_tex) != SITUATION_SUCCESS) {
        return;
    }

    int sw = SituationGetRenderWidth();
    int sh = SituationGetRenderHeight();
    const float margin = 16.0f;
    const float bar_h  = 22.0f;
    const float pip_w  = (float)(VD_MINI_W * MINI_PIP_SCALE);
    const float pip_h  = (float)(VD_MINI_H * MINI_PIP_SCALE);
    const float pip_x  = (float)sw - pip_w - margin;
    const float pip_y  = (float)sh - pip_h - margin - bar_h;

    /* Frame behind the PiP */
    draw_rect(cmd, pip_x - 4.0f, pip_y - 4.0f, pip_w + 8.0f, pip_h + 8.0f,
              (Vector4){{0.08f, 0.12f, 0.10f, g_mini_glow ? 0.95f : 0.75f}});

    SitRectangle src = {0.0f, 0.0f, (float)VD_MINI_W, (float)VD_MINI_H};
    SitRectangle dst = {pip_x, pip_y, pip_w, pip_h};
    Vector2 origin = {{0.0f, 0.0f}};
    uint8_t alpha = g_mini_glow ? 255 : 210;
    ColorRGBA tint = {255, 255, 255, alpha};

    SituationCmdDrawTexture(cmd, mini_tex, src, dst, origin, 0.0f, tint);
}

static bool init_virtual_displays(void)
{
    SituationError err = SituationCreateVirtualDisplayEx(
        (Vector2){{(float)VD_MAIN_W, (float)VD_MAIN_H}},
        1.0, 0,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_NONE,
        SITUATION_VD_FLAG_NONE,
        &g_vd_main);
    if (err != SITUATION_SUCCESS || g_vd_main < 0) {
        fprintf(stderr, "[05] CreateVirtualDisplayEx (main) failed\n");
        return false;
    }

    err = SituationCreateVirtualDisplay(
        (Vector2){{(float)VD_MINI_W, (float)VD_MINI_H}},
        1.0, 1,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_ALPHA,
        &g_vd_mini);
    if (err != SITUATION_SUCCESS || g_vd_mini < 0) {
        fprintf(stderr, "[05] CreateVirtualDisplay (minimap) failed\n");
        return false;
    }

    /* Minimap is drawn manually in the corner — keep it out of auto-composite. */
    SituationConfigureVirtualDisplay(
        g_vd_mini,
        (Vector2){{0.0f, 0.0f}},
        1.0f, 1, false, 1.0, SITUATION_BLEND_ALPHA);

    return true;
}

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "05 — Virtual Display Retro") != SITUATION_SUCCESS) {
        return -1;
    }

    if (!init_virtual_displays()) {
        SitExample_Shutdown();
        return -1;
    }

    printf("Virtual Display Retro — main %dx%d (integer scale), minimap %dx%d PiP\n",
           VD_MAIN_W, VD_MAIN_H, VD_MINI_W, VD_MINI_H);

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) {
            break;
        }

        if (SituationIsKeyPressed(SIT_KEY_N)) {
            g_show_mini = !g_show_mini;
        }
        if (SituationIsKeyPressed(SIT_KEY_B)) {
            g_mini_glow = !g_mini_glow;
        }

        if (SituationIsAppPaused()) {
            continue;
        }

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            continue;
        }

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        double t = SituationTimerGetTime();

        /* Pass A — draw retro scene into the main CRT buffer */
        SituationRenderPassInfo rp_main = vd_pass(g_vd_main, (ColorRGBA){8, 6, 20, 255});
        SituationCmdBeginRenderPass(cmd, &rp_main);
        draw_main_scene(cmd, t);
        SituationCmdEndRenderPass(cmd);

        /* Pass B — draw minimap into the secondary VD */
        SituationRenderPassInfo rp_mini = vd_pass(g_vd_mini, (ColorRGBA){0, 0, 0, 255});
        SituationCmdBeginRenderPass(cmd, &rp_mini);
        draw_minimap_scene(cmd);
        SituationCmdEndRenderPass(cmd);

        /* Pass C — composite to the host window (black letterbox + HUD) */
        SituationRenderPassInfo main_rp = {0};
        main_rp.display_id = -1;
        main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
        main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        main_rp.depth_attachment.clear.depth = 1.0f;

        SituationCmdBeginRenderPass(cmd, &main_rp);
        SituationRenderVirtualDisplays(cmd);
        draw_minimap_pip(cmd);
        SitExample_DrawHUD(cmd,
            "05 — Virtual Display Retro",
            "N:minimap  B:glow  — integer-scaled 320x240 CRT + corner PiP");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    SituationDestroyVirtualDisplay(g_vd_mini);
    SituationDestroyVirtualDisplay(g_vd_main);
    SitExample_Shutdown();
    return 0;
}
