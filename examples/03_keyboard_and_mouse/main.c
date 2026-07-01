/***************************************************************************************************
 *  Situation — 03: Keyboard and Mouse
 *
 *  A moving rectangle controlled entirely by the keyboard and mouse.
 *  Demonstrates the full input API in a simple, hands-on way.
 *
 *  Controls:
 *    WASD / Arrow keys — move the square (hold for continuous, press for impulse)
 *    Left click        — teleport square to mouse cursor
 *    Right click (hold)— drag the square with the mouse
 *    Mouse wheel       — resize the square
 *    Tab               — cycle through five preset colours
 *    Space             — reset position to centre
 *
 *  What this example teaches:
 *    - SituationIsKeyDown      — continuous hold (smooth movement)
 *    - SituationIsKeyPressed   — single-frame press (discrete actions)
 *    - SituationGetMousePosition
 *    - SituationIsMouseButtonDown / SituationIsMouseButtonPressed
 *    - SituationGetMouseWheelMove
 *    - Frame-rate-independent movement via SituationGetFrameTime
 *    - SituationSetWindowTitle with snprintf (live dynamic title bar)
 *    - SIT_KEY_* and SIT_MOUSE_BUTTON_* constants
 *
 *  Universal hotkeys (from sit_example.h):
 *    ESC — quit   F11 — fullscreen   F9 — VSync   P — pause   F12 — screenshot
 *
 *  Build:
 *    build\build_examples.bat static-opengl  03_keyboard_and_mouse
 *    build\build_examples.bat static-vulkan  03_keyboard_and_mouse
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>

/* ── Helper ── */
static void draw_rect(SituationCommandBuffer cmd,
                      float x, float y, float w, float h, Vector4 col)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m,    (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
}

/* ── Main ── */
int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "03 — Keyboard and Mouse") != SITUATION_SUCCESS) {
        return -1;
    }

    /* Square state */
    float sq_x    = 450.0f;
    float sq_y    = 340.0f;
    float sq_size = 60.0f;
    int   colour_idx = 0;

    /* Mouse drag state */
    float drag_offset_x = 0.0f;
    float drag_offset_y = 0.0f;

    /* Five preset colours */
    static const Vector4 COLOURS[5] = {
        {{0.85f, 0.25f, 0.25f, 1.0f}},  /* red    */
        {{0.25f, 0.80f, 0.35f, 1.0f}},  /* green  */
        {{0.25f, 0.50f, 0.95f, 1.0f}},  /* blue   */
        {{0.90f, 0.80f, 0.15f, 1.0f}},  /* yellow */
        {{0.75f, 0.25f, 0.95f, 1.0f}},  /* violet */
    };

    /* Clamp square within screen bounds */
    #define CLAMP_SQUARE() \
        do { \
            int _sw = SituationGetRenderWidth(); \
            int _sh = SituationGetRenderHeight(); \
            if (sq_x < 0.0f) sq_x = 0.0f; \
            if (sq_y < 20.0f) sq_y = 20.0f; \
            if (sq_x + sq_size > (float)_sw) sq_x = (float)_sw - sq_size; \
            if (sq_y + sq_size > (float)_sh - 14.0f) sq_y = (float)_sh - sq_size - 14.0f; \
        } while(0)

    while (!SituationWindowShouldClose())
    {
        if (SitExample_BeginFrame()) { break; }

        float dt = SituationGetFrameTime();
        if (dt > 0.05f) { dt = 0.05f; }

        int sw = SituationGetRenderWidth();
        int sh = SituationGetRenderHeight();

        /* ── Input: movement (WASD / arrows) ────────────────────────── */
        const float SPEED = 280.0f;  /* pixels per second */
        if (!SituationIsAppPaused()) {
            if (SituationIsKeyDown(SIT_KEY_W) || SituationIsKeyDown(SIT_KEY_UP))    { sq_y -= SPEED * dt; }
            if (SituationIsKeyDown(SIT_KEY_S) || SituationIsKeyDown(SIT_KEY_DOWN))  { sq_y += SPEED * dt; }
            if (SituationIsKeyDown(SIT_KEY_A) || SituationIsKeyDown(SIT_KEY_LEFT))  { sq_x -= SPEED * dt; }
            if (SituationIsKeyDown(SIT_KEY_D) || SituationIsKeyDown(SIT_KEY_RIGHT)) { sq_x += SPEED * dt; }
        }

        /* ── Input: mouse wheel — resize ─────────────────────────────── */
        {
            float wheel = SituationGetMouseWheelMove();
            if (wheel != 0.0f && !SituationIsAppPaused()) {
                sq_size += wheel * 6.0f;
                if (sq_size < 12.0f)  { sq_size = 12.0f; }
                if (sq_size > 200.0f) { sq_size = 200.0f; }
            }
        }

        /* ── Input: left click — teleport ───────────────────────────── */
        if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT) && !SituationIsAppPaused()) {
            Vector2 mp = SituationGetMousePosition();
            sq_x = mp.x - sq_size * 0.5f;
            sq_y = mp.y - sq_size * 0.5f;
        }

        /* ── Input: right button — drag ─────────────────────────────── */
        if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_RIGHT) && !SituationIsAppPaused()) {
            /* Record offset from square's top-left when drag starts */
            Vector2 mp = SituationGetMousePosition();
            drag_offset_x = mp.x - sq_x;
            drag_offset_y = mp.y - sq_y;
        }
        if (SituationIsMouseButtonDown(SIT_MOUSE_BUTTON_RIGHT) && !SituationIsAppPaused()) {
            Vector2 mp = SituationGetMousePosition();
            sq_x = mp.x - drag_offset_x;
            sq_y = mp.y - drag_offset_y;
        }

        /* ── Input: Tab — cycle colour ──────────────────────────────── */
        if (SituationIsKeyPressed(SIT_KEY_TAB)) {
            colour_idx = (colour_idx + 1) % 5;
        }

        /* ── Input: Space — reset to centre ─────────────────────────── */
        if (SituationIsKeyPressed(SIT_KEY_SPACE) && !SituationIsAppPaused()) {
            sq_x = (float)sw * 0.5f - sq_size * 0.5f;
            sq_y = (float)sh * 0.5f - sq_size * 0.5f;
        }

        CLAMP_SQUARE();

        /* ── Live window title with cursor coordinates ─────────────── */
        {
            Vector2 mp = SituationGetMousePosition();
            char title[128];
            snprintf(title, sizeof(title),
                     "03 — Input | Square (%.0f, %.0f) | Mouse (%.0f, %.0f) | FPS %d",
                     sq_x, sq_y, mp.x, mp.y, SituationGetFPS());
            SituationSetWindowTitle(title);
        }

        /* ── Render ─────────────────────────────────────────────────── */
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) { continue; }

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationRenderPassInfo pass = {
            .display_id       = -1,
            .color_attachment = {
                .loadOp = SIT_LOAD_OP_CLEAR,
                .clear  = { .color = {20, 22, 32, 255} }
            }
        };
        SituationCmdBeginRenderPass(cmd, &pass);

        /* Background grid for spatial reference */
        {
            Vector4 grid_col = {{0.18f, 0.18f, 0.28f, 1.0f}};
            for (int gy = 40; gy < sh; gy += 48) {
                draw_rect(cmd, 0.0f, (float)gy, (float)sw, 1.0f, grid_col);
            }
            for (int gx = 0; gx < sw; gx += 64) {
                draw_rect(cmd, (float)gx, 20.0f, 1.0f, (float)(sh - 34), grid_col);
            }
        }

        /* Mouse cursor cross-hair */
        {
            Vector2 mp = SituationGetMousePosition();
            Vector4 cursor_col = {{1.0f, 1.0f, 1.0f, 0.6f}};
            draw_rect(cmd, mp.x - 12.0f, mp.y - 1.0f, 24.0f, 2.0f, cursor_col);
            draw_rect(cmd, mp.x - 1.0f,  mp.y - 12.0f, 2.0f, 24.0f, cursor_col);
        }

        /* Shadow */
        {
            Vector4 shadow = {{0.0f, 0.0f, 0.0f, 0.4f}};
            draw_rect(cmd, sq_x + 5.0f, sq_y + 5.0f, sq_size, sq_size, shadow);
        }

        /* The controllable square */
        draw_rect(cmd, sq_x, sq_y, sq_size, sq_size, COLOURS[colour_idx]);

        /* Label above the square */
        {
            static const char* const colour_names[5] = {
                "Red", "Green", "Blue", "Yellow", "Violet"
            };
            char label[48];
            snprintf(label, sizeof(label), "%.0f×%.0f  [%s]",
                     sq_size, sq_size, colour_names[colour_idx]);
            SituationCmdDrawTextEx(cmd, _sit_ex_font, label,
                                   (Vector2){{sq_x, sq_y - 12.0f}},
                                   8.0f, 1.0f, (ColorRGBA){220, 220, 220, 220});
        }

        /* On-screen control legend */
        {
            const char* lines[] = {
                "WASD/Arrows — move",
                "Mouse wheel — resize",
                "LMB click   — teleport",
                "RMB hold    — drag",
                "Tab         — colour",
                "Space       — reset",
            };
            int n = (int)(sizeof(lines) / sizeof(lines[0]));
            float lx = 8.0f;
            float ly = 32.0f;
            for (int i = 0; i < n; i++) {
                SituationCmdDrawTextEx(cmd, _sit_ex_font, lines[i],
                                       (Vector2){{lx, ly + (float)i * 12.0f}},
                                       8.0f, 1.0f, (ColorRGBA){160, 180, 160, 200});
            }
        }

        SitExample_DrawHUD(cmd, "03 — Keyboard and Mouse",
            "WASD/Arrows:move  Wheel:resize  LMB:teleport  RMB:drag  Tab:colour  Space:reset");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    SitExample_Shutdown();
    return 0;
}
