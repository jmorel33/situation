/***************************************************************************************************
 *  Situation — 02: Draw Shapes
 *
 *  Draws animated 2D geometry using SituationCmdDrawQuad.  Every shape is a scaled + translated
 *  quad (the library's universal 2D primitive).  Text labels are drawn with the IBM 8×8 bitmap font.
 *
 *  What you see:
 *    - Six named colour swatches, each pulsing at a slightly different phase
 *    - A "bouncing ball" that travels across the screen and wraps around the edges
 *    - A spinning square in the centre, driven by SituationGetFrameTime
 *    - All shapes labelled with SituationCmdDrawTextEx
 *
 *  What this example teaches:
 *    - SituationCmdDrawQuad with a 4×4 transform matrix (translate + scale)
 *    - SituationCmdDrawTextEx — bitmap font, size, spacing, colour
 *    - SituationGetFrameTime for frame-rate-independent animation
 *    - SituationTimerGetTime for continuous / sinusoidal animation
 *    - ColorRGBA and Vector4 colour types
 *    - Rectangle and Vector2 helper types
 *
 *  Universal hotkeys (from sit_example.h):
 *    ESC — quit   F11 — fullscreen   F9 — VSync   P — pause   F12 — screenshot
 *
 *  Build:
 *    build\build_examples.bat static-opengl  02_draw_shapes
 *    build\build_examples.bat static-vulkan  02_draw_shapes
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>

/* ── Helpers ──────────────────────────────────────────────────────────────────────────────── */

/* Draw a solid axis-aligned rectangle using a plain identity+T+S matrix. */
static void draw_rect(SituationCommandBuffer cmd,
                      float x, float y, float w, float h,
                      Vector4 colour)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m,    (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, colour);
}

/* Draw a rotated quad (angle in radians, rotated about its own centre). */
static void draw_rect_rot(SituationCommandBuffer cmd,
                          float cx, float cy, float w, float h,
                          float angle, Vector4 colour)
{
    mat4 m;
    glm_mat4_identity(m);
    /* Translate to centre, rotate, then offset back so (cx,cy) is the centre. */
    glm_translate(m, (vec3){cx, cy, 0.0f});
    glm_rotate_z(m, angle, m);
    glm_translate(m, (vec3){-w * 0.5f, -h * 0.5f, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, colour);
}

/* ── Main ─────────────────────────────────────────────────────────────────────────────────── */

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "02 — Draw Shapes") != SITUATION_SUCCESS) {
        return -1;
    }

    /* Animation state */
    const float BALL_R = 22.0f;
    float ball_x  = 100.0f;
    float ball_y  = 300.0f;
    float ball_vx = 220.0f;
    float ball_vy = 160.0f;

    /* Generate a circle texture at runtime (white filled disc with alpha). */
    SituationTexture circle_tex = {0};
    {
        const int TEX_SIZE = 64;
        SituationImage circle_img = {0};
        SituationCreateImage(TEX_SIZE, TEX_SIZE, 4, &circle_img);
        float center = (float)(TEX_SIZE - 1) * 0.5f;
        float radius_sq = center * center;
        for (int py = 0; py < TEX_SIZE; py++) {
            for (int px = 0; px < TEX_SIZE; px++) {
                float dx = (float)px - center;
                float dy = (float)py - center;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq <= radius_sq) {
                    /* Soft edge: anti-alias the last pixel ring */
                    float dist = sqrtf(dist_sq);
                    float alpha = 1.0f - fmaxf(0.0f, (dist - (center - 1.0f)));
                    uint8_t a = (uint8_t)(alpha * 255.0f);
                    SituationSetPixelColor(&circle_img, px, py, (ColorRGBA){255, 255, 255, a});
                } else {
                    SituationSetPixelColor(&circle_img, px, py, (ColorRGBA){0, 0, 0, 0});
                }
            }
        }
        SituationCreateTexture(circle_img, false, &circle_tex);
        SituationUnloadImage(circle_img);
    }

    while (!SituationWindowShouldClose())
    {
        if (SitExample_BeginFrame()) { break; }

        int sw = SituationGetRenderWidth();
        int sh = SituationGetRenderHeight();

        /* All animation is purely time-driven (no dt accumulation) for micro-stutter-free motion. */

        /* ── Render ────────────────────────────────────────────────────── */
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) { continue; }

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

        SituationRenderPassInfo pass = SituationRenderPassInfoDefault(-1, (ColorRGBA){18, 18, 28, 255});
        SituationCmdBeginRenderPass(cmd, &pass);

        /* ---- Colour swatches ---- */
        /* Six named colours laid out in a row near the top.
           Each swatch pulses in brightness with a different phase offset. */
        double t_now = SituationTimerGetTime();
        const struct { float r, g, b; const char* name; float phase; } swatches[6] = {
            { 0.85f, 0.20f, 0.20f, "Crimson",  0.00f },
            { 0.20f, 0.75f, 0.35f, "Emerald",  0.52f },
            { 0.20f, 0.45f, 0.90f, "Cobalt",   1.05f },
            { 0.90f, 0.80f, 0.15f, "Amber",    1.57f },
            { 0.70f, 0.25f, 0.90f, "Violet",   2.09f },
            { 0.20f, 0.80f, 0.90f, "Cyan",     2.62f },
        };
        const float SWATCH_W = 96.0f;
        const float SWATCH_H = 50.0f;
        const float SWATCH_Y = 40.0f;
        float total_w = SWATCH_W * 6.0f + 10.0f * 5.0f;
        float sx0 = ((float)sw - total_w) * 0.5f;

        for (int i = 0; i < 6; i++) {
            float pulse = 0.65f + 0.35f * (float)sin(t_now * 1.8 + (double)swatches[i].phase);
            Vector4 col = {{ swatches[i].r * pulse,
                             swatches[i].g * pulse,
                             swatches[i].b * pulse,
                             1.0f }};
            float sx = sx0 + (float)i * (SWATCH_W + 10.0f);
            draw_rect(cmd, sx, SWATCH_Y, SWATCH_W, SWATCH_H, col);

            /* White label under each swatch */
            SituationCmdDrawTextEx(cmd, _sit_ex_font, swatches[i].name,
                                   (Vector2){{sx + 4.0f, SWATCH_Y + SWATCH_H + 4.0f}},
                                   8.0f, 1.0f, (ColorRGBA){220, 220, 220, 255});
        }

        /* ---- Spinning square (centre screen) ---- */
        /* Use raw wall-clock time directly (not accumulated deltas) for perfectly smooth
         * rotation regardless of frame rate or dt jitter. At 2000fps the per-frame dt
         * is ~0.5ms — accumulating those introduces floating-point noise that causes
         * micro-stutter. A single glfwGetTime() call is monotonic and jitter-free. */
        float spin_angle = (float)(t_now * 1.2);
        float spin_size  = 80.0f + 20.0f * (float)sin(t_now * 0.7);
        float spin_cx    = (float)sw * 0.5f;
        float spin_cy    = (float)sh * 0.5f;
        {
            /* Soft drop-shadow: larger offset + lower opacity for a clean look */
            Vector4 shadow = {{0.0f, 0.0f, 0.0f, 0.35f}};
            draw_rect_rot(cmd,
                          spin_cx + 6.0f, spin_cy + 8.0f,
                          spin_size + 4.0f, spin_size + 4.0f,
                          spin_angle, shadow);

            float hue = (float)fmod(t_now * 0.3, 1.0);
            /* Cycle hue manually: red→green→blue→red */
            float r = 0.5f + 0.5f * (float)cos(hue * 6.283185f);
            float g = 0.5f + 0.5f * (float)cos(hue * 6.283185f - 2.094f);
            float b = 0.5f + 0.5f * (float)cos(hue * 6.283185f - 4.189f);
            Vector4 col = {{r, g, b, 1.0f}};
            draw_rect_rot(cmd, spin_cx, spin_cy, spin_size, spin_size, spin_angle, col);

            SituationCmdDrawTextEx(cmd, _sit_ex_font, "Spinning square",
                                   (Vector2){{spin_cx - 56.0f, spin_cy + spin_size * 0.5f + 10.0f}},
                                   8.0f, 1.0f, (ColorRGBA){200, 200, 200, 255});
        }

        /* ---- Bouncing ball ---- */
        {
            float dt = SituationGetFrameTime();
            if (dt > 0.05f) dt = 0.05f;

            if (!SituationIsAppPaused()) {
                ball_x += ball_vx * dt;
                ball_y += ball_vy * dt;
                if (ball_x < 0.0f)                              { ball_x = 0.0f;                              ball_vx = fabsf(ball_vx); }
                if (ball_x > (float)sw - BALL_R * 2.0f)         { ball_x = (float)sw - BALL_R * 2.0f;         ball_vx = -fabsf(ball_vx); }
                if (ball_y < 22.0f)                             { ball_y = 22.0f;                              ball_vy = fabsf(ball_vy); }
                if (ball_y > (float)sh - BALL_R * 2.0f - 22.0f) { ball_y = (float)sh - BALL_R * 2.0f - 22.0f; ball_vy = -fabsf(ball_vy); }
            }

            float t01 = (float)fmod(t_now * 0.5, 1.0);
            float r = 0.3f + 0.7f * (float)fabs(sin(t01 * 3.14159f));
            ColorRGBA ball_tint = {
                (uint8_t)(r * 255.0f),
                (uint8_t)(0.4f * 255.0f),
                (uint8_t)((1.0f - r) * 255.0f),
                255
            };
            SitRectangle src = {0.0f, 0.0f, 64.0f, 64.0f};
            SitRectangle dst = {ball_x, ball_y, BALL_R * 2.0f, BALL_R * 2.0f};
            SituationCmdDrawTexture(cmd, circle_tex, src, dst,
                                    (Vector2){{0.0f, 0.0f}}, 0.0f, ball_tint);
            SituationCmdDrawTextEx(cmd, _sit_ex_font, "Ball",
                                   (Vector2){{ball_x, ball_y - 12.0f}},
                                   8.0f, 1.0f, (ColorRGBA){200, 200, 200, 255});
        }

        /* ---- HUD overlay ---- */
        SitExample_DrawHUD(cmd,
            "02 — Draw Shapes",
            "SituationCmdDrawQuad (quads) + SituationCmdDrawTextEx (labels)");

        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    SituationDestroyTexture(&circle_tex);
    SitExample_Shutdown();
    return 0;
}
