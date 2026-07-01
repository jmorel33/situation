/**
 * @file sit_test_hud.h
 * @brief Simple HUD overlay for visual harness tests.
 *
 * Drop one call into any test render loop:
 *
 *     // inside a render pass, before SituationCmdEndRenderPass:
 *     sit_test_hud_draw(cmd, "test_graphics", "cmd_draw_quad_color", "ESC:quit  P:pause");
 *
 * The HUD draws:
 *   TOP BAR    — module name (left)  |  FPS · backend (right)
 *   BOTTOM BAR — test/segment name (left)  |  hint keys (right)
 *
 * No assets, no font load — uses the library default 8×8 font via zeroed SituationFont.
 * No state beyond the static counters below (screenshot index, vsync flag).
 *
 * Universal hotkeys handled by sit_test_hud_poll():
 *   ESC  — returns 1 (caller should break the loop)
 *   F9   — toggle VSync
 *   F11  — toggle fullscreen
 *   F12  — screenshot (saved as sit_test_NNNN.bmp)
 *   P    — pause/resume
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#ifndef SIT_TEST_HUD_H
#define SIT_TEST_HUD_H

#include "sit_api_include.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ── internal state ─────────────────────────────────────────────────────── */
static int   _sit_hud_vsync        = 1;
static int   _sit_hud_screenshot_n = 0;

/* ── private helpers ────────────────────────────────────────────────────── */

static inline void _sit_hud_fill(SituationCommandBuffer cmd,
                                  float x, float y, float w, float h,
                                  Vector4 col)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m,    (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
}

static inline void _sit_hud_text(SituationCommandBuffer cmd,
                                  const char* s, float x, float y,
                                  ColorRGBA col)
{
    SituationFont null_font = {0};   /* zeroed → library falls back to built-in 8×8 VGA font */
    SituationCmdDrawTextEx(cmd, null_font, s,
                           (Vector2){{x, y}}, 16.0f, 1.0f, col);
}

/* approximate glyph width at size=16 */
#define _SIT_HUD_CHW  10.0f
#define _SIT_HUD_BAR  22.0f

/* ── public API ─────────────────────────────────────────────────────────── */

/**
 * @brief Poll input and handle universal test hotkeys.
 *
 * Call once per frame BEFORE acquiring the command buffer:
 *
 *     SITUATION_BEGIN_FRAME();
 *     if (sit_test_hud_poll()) break;
 *
 * @return 1 if the test loop should exit (ESC pressed or window close), 0 otherwise.
 */
static inline int sit_test_hud_poll(void)
{
    if (SituationWindowShouldClose()) return 1;
    if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) return 1;

    if (SituationIsKeyPressed(SIT_KEY_F11)) {
        SituationToggleFullscreen();
        SituationConsumeKeyPress(SIT_KEY_F11);
    }
    if (SituationIsKeyPressed(SIT_KEY_F9)) {
        _sit_hud_vsync = !_sit_hud_vsync;
        SituationSetVSync((bool)_sit_hud_vsync);
        SituationConsumeKeyPress(SIT_KEY_F9);
    }
    if (SituationIsKeyPressed(SIT_KEY_P)) {
        if (SituationIsAppPaused()) SituationResumeApp();
        else                        SituationPauseApp();
        SituationConsumeKeyPress(SIT_KEY_P);
    }
    if (SituationIsKeyPressed(SIT_KEY_F12)) {
        char fname[64];
        snprintf(fname, sizeof(fname), "sit_test_%04d", _sit_hud_screenshot_n++);
        if (SituationTakeScreenshot(fname) == SITUATION_SUCCESS) {
            printf("[hud] screenshot: %s.bmp\n", fname);
        }
        SituationConsumeKeyPress(SIT_KEY_F12);
    }
    return 0;
}

/**
 * @brief Draw the HUD. Call inside an open render pass, before EndRenderPass.
 *
 * @param cmd      Active command buffer.
 * @param module   Module name shown top-left  (e.g. "test_graphics").
 * @param test     Test/segment name shown bottom-left (may be NULL).
 * @param hint     Short key hint shown bottom-right (may be NULL).
 */
static inline void sit_test_hud_draw(SituationCommandBuffer cmd,
                                      const char* module,
                                      const char* test,
                                      const char* hint)
{
    const int sw = SituationGetRenderWidth();
    const int sh = SituationGetRenderHeight();
    const Vector4 bar_bg = {{0.0f, 0.0f, 0.0f, 0.80f}};

    /* ── TOP BAR ── */
    _sit_hud_fill(cmd, 0.0f, 0.0f, (float)sw, _SIT_HUD_BAR, bar_bg);

    /* module name — left */
    if (module && module[0]) {
        _sit_hud_text(cmd, module, 4.0f, 3.0f, (ColorRGBA){220, 220, 220, 255});
    }

    /* right side: [PAUSED]  VSync  Backend  FPS */
    float rx = (float)sw - 4.0f;

    if (SituationIsAppPaused()) {
        const char* tok = "[PAUSED]";
        rx -= (float)strlen(tok) * _SIT_HUD_CHW;
        _sit_hud_text(cmd, tok, rx, 3.0f, (ColorRGBA){255, 220, 50, 255});
        rx -= 12.0f;
    }
    {
        const char* tok = _sit_hud_vsync ? "VSync:ON" : "VSync:OFF";
        rx -= (float)strlen(tok) * _SIT_HUD_CHW;
        _sit_hud_text(cmd, tok, rx, 3.0f,
                      _sit_hud_vsync ? (ColorRGBA){100, 220, 100, 255}
                                     : (ColorRGBA){200,  80,  80, 255});
        rx -= 12.0f;
    }
    {
        const char* backend = SituationGetGraphicsBackendName();
        rx -= (float)strlen(backend) * _SIT_HUD_CHW;
        _sit_hud_text(cmd, backend, rx, 3.0f, (ColorRGBA){160, 160, 200, 255});
        rx -= 12.0f;
    }
    {
        char fps_buf[16];
        snprintf(fps_buf, sizeof(fps_buf), "FPS:%d", SituationGetFPS());
        rx -= (float)strlen(fps_buf) * _SIT_HUD_CHW;
        _sit_hud_text(cmd, fps_buf, rx, 3.0f, (ColorRGBA){255, 240, 80, 255});
    }

    /* ── BOTTOM BAR ── */
    const float bot_y = (float)sh - _SIT_HUD_BAR;
    _sit_hud_fill(cmd, 0.0f, bot_y, (float)sw, _SIT_HUD_BAR, bar_bg);

    /* test/segment — left */
    if (test && test[0]) {
        _sit_hud_text(cmd, test, 4.0f, bot_y + 3.0f,
                      (ColorRGBA){200, 220, 255, 220});
    }

    /* hint — right (caller keys) + universal keys */
    {
        char combined[192] = {0};
        const char* universal = "F9:VSync  F11:Full  F12:Shot  P:Pause  ESC:quit";
        if (hint && hint[0]) {
            snprintf(combined, sizeof(combined), "%s    %s", hint, universal);
        } else {
            snprintf(combined, sizeof(combined), "%s", universal);
        }
        float kx = (float)sw - (float)strlen(combined) * _SIT_HUD_CHW - 4.0f;
        if (kx < 0.0f) kx = 0.0f;
        _sit_hud_text(cmd, combined, kx, bot_y + 3.0f,
                      (ColorRGBA){130, 130, 170, 200});
    }
}

#undef _SIT_HUD_CHW
#undef _SIT_HUD_BAR

#endif /* SIT_TEST_HUD_H */
