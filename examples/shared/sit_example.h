/***************************************************************************************************
 *  sit_example.h — Shared scaffolding for all Situation digestible examples
 *
 *  Include this header once at the top of every numbered example, after selecting a backend
 *  and before including situation.h:
 *
 *      #if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
 *          #define SITUATION_USE_OPENGL
 *      #endif
 *      #include "shared/sit_example.h"
 *      // situation.h is already included by this header — do not include it again.
 *
 *  What this provides:
 *    - Standard include block (situation.h, stdio, string, math)
 *    - IBM 8x8 VGA bitmap font (inline data — included once per translation unit)
 *    - SitExample_Init()          — window + font setup with standard 1280x1024 config
 *    - SitExample_BeginFrame()    — wraps SITUATION_BEGIN_FRAME() + common hotkeys
 *    - SitExample_DrawHUD()       — draws title + FPS + key hints at top of screen
 *    - SitExample_EndFrame()      — wraps SituationEndFrame()
 *    - SitExample_Shutdown()      — font cleanup + SituationShutdown()
 *
 *  Universal hotkeys (handled inside SitExample_BeginFrame):
 *    ESC       — quit
 *    F11       — toggle borderless fullscreen / windowed
 *    F9 / V    — toggle VSync on/off (also uncaps FPS target when off so the difference is visible)
 *    P         — toggle pause (SituationPauseApp / SituationResumeApp)
 *    F12       — take screenshot (saved as  screenshot_NNNN.bmp)
 *    M         — toggle debug metrics overlay (spikes + phase timing)
 *
 *  (c) 2025-2026 Jacques Morel — MIT Licensed
 ***************************************************************************************************/
#ifndef SIT_EXAMPLE_H
#define SIT_EXAMPLE_H

#include "situation.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* No external font data needed — the library's built-in default font is used automatically. */

/* ── Internal state ─────────────────────────────────────────────────────── */
static SituationFont  _sit_ex_font;
static int            _sit_ex_vsync        = 1;   /* on by default */
static int            _sit_ex_screenshot_n = 0;
static int            _sit_ex_wants_quit   = 0;
static bool           _sit_ex_show_metrics = false;  /* toggle for spike/phase monitoring */

/* Mouse button aliases (matching GLFW values; 0-based) */
#ifndef SIT_MOUSE_BUTTON_LEFT
    #define SIT_MOUSE_BUTTON_LEFT   0
    #define SIT_MOUSE_BUTTON_RIGHT  1
    #define SIT_MOUSE_BUTTON_MIDDLE 2
#endif

/* Forward declaration — _sit_ex_draw_hud is defined after the public API below */
static void _sit_ex_draw_hud(SituationCommandBuffer cmd,
                              const char* title, const char* hint_line);

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * @brief Initialise Situation with standard example configuration.
 *
 * @param argc         Forwarded from main().
 * @param argv         Forwarded from main().
 * @param window_title Short title shown in the window title bar and HUD.
 * @return SITUATION_SUCCESS on success, non-zero on failure.
 */
static inline SituationError SitExample_Init(int argc, char** argv,
                                              const char* window_title)
{
    SituationInitInfo cfg = {
        .window_title  = window_title,
        .window_width  = 1280,
        .window_height = 1024,
        /* Force 8-bit SDR so that the VSync toggle (F9/V) can actually select IMMEDIATE present mode
         * for uncapped FPS. AUTO/HDR would force FIFO regardless of the vsync flag (for compositor compatibility). */
        .output_color_depth = SIT_OUTPUT_COLOR_8BIT,
        /* VSync on by default; resizable so the user can go fullscreen */
        .initial_active_window_flags =
            SITUATION_FLAG_VSYNC_HINT | SITUATION_FLAG_WINDOW_RESIZABLE,
#if defined(SITUATION_ENABLE_RENDER_THREAD)
        /* Enable render thread by default for examples: offloads fence waits, GL execution,
         * and present to a dedicated thread. This reduces jitter/stutter on the main thread's
         * dt sampling and update loop (main only does light recording + logic). */
        .render_thread_count = 1,
        .backpressure_policy = SIT_RENDER_BACKPRESSURE_YIELD,  // YIELD allows high FPS when vsync off (main yields instead of blocking hard)
#endif
    };

    SituationError err = SituationInit(argc, argv, &cfg);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "[sit_example] SituationInit failed\n");
        return err;
    }

    /* Audio starts usable after init, but examples that pause via SituationPauseApp()
     * must call SituationResumeApp(). Resume the device here so graph/tone examples
     * hear output without each duplicating this call. */
    (void)SituationResumeAudioDevice();

    /* Leave _sit_ex_font zero-initialized.
     * SituationCmdDrawTextEx detects generation==0 and falls back to the library's
     * built-in 8×8 VGA default font automatically — no load call needed.
     * (Same pattern used by platformer_plumber.c and other shipping examples.) */

    // Sync our local VSync tracking var with what was actually requested at init.
    // (The HUD displays this; toggles will flip it and call into the library.)
    _sit_ex_vsync = (cfg.initial_active_window_flags & SITUATION_FLAG_VSYNC_HINT) != 0;

    // Software cap at 0; VSync (driver/present mode) handles pacing when enabled.
    // Avoids double-wait issues.
    SituationSetTargetFPS(0);
    return SITUATION_SUCCESS;
}

/** Register audio node types — required before SituationCreateNode in examples. */
static inline void SitExample_InitAudioRegistry(void)
{
    SituationInitDeviceRegistry();
}

/**
 * @brief Wire virtual MIDI loopback into a tone synth node (keyboard → graph path).
 * @return SITUATION_SUCCESS when notes can reach the synth via SituationVirtualMidiNoteOn/Off.
 */
static inline SituationError SitExample_WireToneSynthVirtualMidi(
    SituationAudioGraph* graph,
    SituationNodeHandle tone,
    const char* tag,
    int* out_midi_in_id)
{
    int midi_in = -1;
    SituationError vm = SituationSetupVirtualMidiLoopback(&midi_in);
    if (vm != SITUATION_SUCCESS) {
        fprintf(stderr, "[%s] Virtual MIDI loopback failed (%d) — piano keys will be silent\n",
                tag ? tag : "sit_example", (int)vm);
        return vm;
    }
    SituationError mc = SituationEnableMidiControl(graph, tone, midi_in);
    if (mc != SITUATION_SUCCESS) {
        fprintf(stderr, "[%s] EnableMidiControl failed (%d) — piano keys will be silent\n",
                tag ? tag : "sit_example", (int)mc);
        return mc;
    }
    SituationSetNodeMidiChannel(graph, tone, 0);
    if (out_midi_in_id) {
        *out_midi_in_id = midi_in;
    }
    return SITUATION_SUCCESS;
}

/** Topological sort + activate graph for audio output (Policy B default graph is replaced). */
static inline SituationError SitExample_ActivateAudioGraph(SituationAudioGraph* graph)
{
    if (!graph) return SITUATION_ERROR_INVALID_PARAM;
    SituationError err = SituationTopologicalSort(graph);
    if (err != SITUATION_SUCCESS) return err;
    return SituationSetActiveGraph(graph);
}

/**
 * @brief Safe graph teardown: deactivate, close node MIDI streams, destroy graph.
 *        Call before SituationTeardownVirtualMidiLoopback().
 */
static inline void SitExample_DestroyAudioGraph(
    SituationAudioGraph** graph,
    SituationNodeHandle tone_with_midi)
{
    if (!graph || !*graph) return;
    SituationSetActiveGraph(NULL);
    if (tone_with_midi != SITUATION_INVALID_NODE_HANDLE) {
        (void)SituationDisableMidiControl(*graph, tone_with_midi);
    }
    SituationDestroyGraph(*graph);
    *graph = NULL;
}

/** Close virtual MIDI after the graph (and its Pm_OpenInput streams) are gone. */
static inline void SitExample_TeardownVirtualMidi(void)
{
    SituationTeardownVirtualMidiLoopback();
}

/**
 * @brief Process input, handle universal hotkeys, and update timers.
 *        Call this at the very top of your main loop body.
 *
 * @return 0 to keep running, 1 if the user requested quit (ESC or window close).
 */
static inline int SitExample_BeginFrame(void)
{
    SITUATION_BEGIN_FRAME();

    /* ── ESC — quit ─────────────────────────────────────────── */
    if (SituationIsKeyPressed(SIT_KEY_ESCAPE) || SituationWindowShouldClose()) {
        _sit_ex_wants_quit = 1;
        SituationConsumeKeyPress(SIT_KEY_ESCAPE);
    }

    /* ── F11 — toggle borderless fullscreen ────────────────── */
    if (SituationIsKeyPressed(SIT_KEY_F11)) {
        SituationToggleBorderlessWindowed();
        SituationConsumeKeyPress(SIT_KEY_F11);
    }

    /* ── F9 — toggle VSync ──────────────────────────────────── */
    if (SituationIsKeyPressed(SIT_KEY_F9)) {
        _sit_ex_vsync = !_sit_ex_vsync;
        SituationSetVSync((bool)_sit_ex_vsync);
        // Keep software cap at 0; VSync (when on) will cap via the driver/present mode.
        // This prevents double-wait (software sleep after vsync-blocked present) that caused sub-60 FPS.
        SituationSetTargetFPS(0);
        SituationConsumeKeyPress(SIT_KEY_F9);
    }

    /* ── P — toggle pause ───────────────────────────────────── */
    if (SituationIsKeyPressed(SIT_KEY_P)) {
        if (SituationIsAppPaused()) {
            SituationResumeApp();
        } else {
            SituationPauseApp();
        }
        SituationConsumeKeyPress(SIT_KEY_P);
    }

    /* ── F12 — screenshot ───────────────────────────────────── */
    if (SituationIsKeyPressed(SIT_KEY_F12)) {
        char fname[64];
        snprintf(fname, sizeof(fname), "screenshot_%04d", _sit_ex_screenshot_n++);
        if (SituationTakeScreenshot(fname) == SITUATION_SUCCESS) {
            printf("[sit_example] Screenshot saved: %s.bmp\n", fname);
        }
        SituationConsumeKeyPress(SIT_KEY_F12);
    }

    /* ── M — toggle debug metrics / spike monitor ───────────── */
    if (SituationIsKeyPressed(SIT_KEY_M)) {
        _sit_ex_show_metrics = !_sit_ex_show_metrics;
        SituationConsumeKeyPress(SIT_KEY_M);
    }

    return _sit_ex_wants_quit;
}

/**
 * @brief Draw a slim HUD banner at the top of the screen.
 *        Shows: example title on the left, FPS + backend top-right,
 *        and a one-line key-hint on the bottom-left.
 *
 * @param cmd        Active command buffer (inside a render pass).
 * @param title      Example title string (e.g. "01 — Open a Window").
 * @param hint_line  Short hint displayed at the bottom-left (may be NULL).
 */
static inline void SitExample_DrawHUD(SituationCommandBuffer cmd,
                                       const char* title,
                                       const char* hint_line)
{
    _sit_ex_draw_hud(cmd, title, hint_line);
}

/**
 * @brief Submit the current frame. Call after SituationCmdEndRenderPass.
 */
static inline void SitExample_EndFrame(void)
{
    SituationEndFrame();
}

/**
 * @brief Release the font and shut down Situation. Call once after the main loop.
 */
static inline void SitExample_Shutdown(void)
{
    SituationShutdown();
}

/* ── Private helpers ────────────────────────────────────────────────────── */

#define _SIT_EX_FONT_SIZE    16.0f
#define _SIT_EX_FONT_SPACING 1.0f

/* Fill a screen-aligned rectangle. Alpha < 1 is used for the bar background. */
static void _sit_ex_fill(SituationCommandBuffer cmd,
                          float x, float y, float w, float h, Vector4 col)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m,    (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
}

/* Draw text at the confirmed working parameters used by existing examples:
   font_size=16, spacing=1.0  (matches platformer_plumber's draw_hud pattern). */
static void _sit_ex_text(SituationCommandBuffer cmd,
                          const char* s, float x, float y, ColorRGBA col)
{
    SituationCmdDrawTextEx(cmd, _sit_ex_font, s,
                           (Vector2){{x, y}}, _SIT_EX_FONT_SIZE, _SIT_EX_FONT_SPACING, col);
}

/* ── HUD ─────────────────────────────────────────────────────────────────── */
/*
 *  Layout — resolution-independent fractions of sw/sh (reference 1280×1024).
 *  Font size stays 16px; glyph advance for layout = 8×(16/8)+1 = 17px per char.
 *
 *  TOP BAR  y = 0 .. sh×(22/1024)
 *  BOTTOM BAR  y = sh×(1 − 22/1024) .. sh
 *
 *  Status tokens [FULLSCREEN] and [PAUSED] only appear when active.
 */
#define _SIT_EX_REF_W   1280.0f
#define _SIT_EX_REF_H   1024.0f
#define _SIT_EX_RX(v)   ((v) / _SIT_EX_REF_W)
#define _SIT_EX_RY(v)   ((v) / _SIT_EX_REF_H)

#define _SIT_EX_BAR_H_RY        _SIT_EX_RY(22.0f)
#define _SIT_EX_TEXT_INSET_RY   _SIT_EX_RY(3.0f)
#define _SIT_EX_MARGIN_RX       _SIT_EX_RX(4.0f)
#define _SIT_EX_GAP_RX          _SIT_EX_RX(12.0f)
#define _SIT_EX_VSYNC_ANCHOR_RX 0.75f
#define _SIT_EX_FPS_INSET_RX    _SIT_EX_RX(72.0f)
#define _SIT_EX_FPS_GAP_RX      _SIT_EX_RX(8.0f)
#define _SIT_EX_BACKEND_ANCHOR_RX 0.50f
#define _SIT_EX_BOTTOM_KEYS_ANCHOR_RX 0.50f

/* Default 8×8 grid font: advance = cell_w × (fontSize/8) + spacing */
#define _SIT_EX_FONT_CELL_W  8.0f

static float _sit_ex_char_advance(void)
{
    return _SIT_EX_FONT_CELL_W * (_SIT_EX_FONT_SIZE / _SIT_EX_FONT_CELL_W) + _SIT_EX_FONT_SPACING;
}

static float _sit_ex_text_w(const char* s)
{
    return (float)strlen(s) * _sit_ex_char_advance();
}

static float _sit_ex_center_x(float swf, const char* s)
{
    float w = _sit_ex_text_w(s);
    float mx = swf * _SIT_EX_MARGIN_RX;
    float x = swf * _SIT_EX_BOTTOM_KEYS_ANCHOR_RX - w * 0.5f;
    if (x < mx) {
        x = mx;
    }
    if (x + w > swf - mx) {
        x = swf - mx - w;
    }
    return x;
}

static const char* _sit_ex_pick_bottom_keys(float swf)
{
    static const char k_full[] =
        "F9/V:VSync  F11:Fullscreen  F12:Shot  P:Pause  M:Metrics  ESC:Quit";
    static const char k_compact[] =
        "F9:VSync  F11:Full  F12:Shot  P:Pause  M:Metrics  ESC:Quit";
    float mx = swf * _SIT_EX_MARGIN_RX;
    float budget = swf - mx * 2.0f;
    if (_sit_ex_text_w(k_full) <= budget) {
        return k_full;
    }
    if (_sit_ex_text_w(k_compact) <= budget) {
        return k_compact;
    }
    return k_compact;
}

static void _sit_ex_draw_hud(SituationCommandBuffer cmd,
                              const char* title, const char* hint_line)
{
    int sw = SituationGetRenderWidth();
    int sh = SituationGetRenderHeight();
    float swf = (float)sw;
    float shf = (float)sh;
    float bar_h = shf * _SIT_EX_BAR_H_RY;
    float ty = shf * _SIT_EX_TEXT_INSET_RY;
    float mx = swf * _SIT_EX_MARGIN_RX;
    float gap_x = swf * _SIT_EX_GAP_RX;
    int is_fullscreen = SituationIsWindowState(SITUATION_FLAG_BORDERLESS_WINDOWED_MODE) ? 1 : 0;
    int is_paused     = SituationIsAppPaused()                                          ? 1 : 0;

    /* ── TOP BAR background ── */
    _sit_ex_fill(cmd, 0.0f, 0.0f, swf, bar_h,
                 (Vector4){{0.0f, 0.0f, 0.0f, 0.40f}});

    /* Title — left */
    if (title && title[0]) {
        _sit_ex_text(cmd, title, mx, ty, (ColorRGBA){220, 220, 220, 255});
    }

    /* Backend — centered */
    {
        const char* backend = SituationGetGraphicsBackendName();
        float bw = _sit_ex_text_w(backend);
        float bx = swf * _SIT_EX_BACKEND_ANCHOR_RX - bw * 0.5f;
        _sit_ex_text(cmd, backend, bx, ty, (ColorRGBA){160, 160, 200, 255});
    }

    /* VSync — right-aligned at 75% of bar width */
    {
        const char* tok = _sit_ex_vsync ? "VSync:ON" : "VSync:OFF";
        float tw = _sit_ex_text_w(tok);
        float anchor = swf * _SIT_EX_VSYNC_ANCHOR_RX;
        _sit_ex_text(cmd, tok, anchor - tw, ty,
                     _sit_ex_vsync ? (ColorRGBA){100, 220, 100, 255}
                                   : (ColorRGBA){200,  80,  80, 255});
    }

    /* Right edge — FPS + optional status flags, packed right-to-left */
    float rx = swf - mx;

    /* [PAUSED] */
    if (is_paused) {
        const char* tok = "[PAUSED]";
        rx -= _sit_ex_text_w(tok);
        _sit_ex_text(cmd, tok, rx, ty, (ColorRGBA){255, 220, 50, 255});
        rx -= gap_x;
    }

    /* [FULLSCREEN] */
    if (is_fullscreen) {
        const char* tok = "[FULLSCREEN]";
        rx -= _sit_ex_text_w(tok);
        _sit_ex_text(cmd, tok, rx, ty, (ColorRGBA){80, 220, 220, 255});
        rx -= gap_x;
    }

    /* FPS — inset from the right so digit count stays visible */
    {
        char fps_buf[16];
        float fps_gap = swf * _SIT_EX_FPS_GAP_RX;
        snprintf(fps_buf, sizeof(fps_buf), "FPS:%d", SituationGetFPS());
        float fw = _sit_ex_text_w(fps_buf);
        float fps_right = swf - swf * _SIT_EX_FPS_INSET_RX;
        float fx = fps_right - fw;
        if (fx + fw > rx - fps_gap) {
            fx = rx - fps_gap - fw;
        }
        _sit_ex_text(cmd, fps_buf, fx, ty, (ColorRGBA){255, 240, 80, 255});
    }

    /* ── BOTTOM BAR ── */
    float bot_y = shf - bar_h;
    _sit_ex_fill(cmd, 0.0f, bot_y, swf, bar_h,
                 (Vector4){{0.0f, 0.0f, 0.0f, 0.40f}});

    /* Universal hotkeys — centered in the bottom bar */
    {
        const char* keys = _sit_ex_pick_bottom_keys(swf);
        float kx = _sit_ex_center_x(swf, keys);
        _sit_ex_text(cmd, keys, kx, bot_y + ty, (ColorRGBA){130, 130, 170, 220});

        /* Example hint — left, only when it fits beside centered keys */
        if (hint_line && hint_line[0]) {
            float hw = _sit_ex_text_w(hint_line);
            float hx = mx;
            if (hx + hw + gap_x <= kx) {
                _sit_ex_text(cmd, hint_line, hx, bot_y + ty,
                             (ColorRGBA){200, 200, 255, 220});
            }
        }
    }

    /* ── Debug metrics / spike monitor (toggle with M) ── */
    if (_sit_ex_show_metrics) {
        _sit_ex_fill(cmd,
            swf * _SIT_EX_RX(10.0f), shf * _SIT_EX_RY(60.0f),
            swf * _SIT_EX_RX(580.0f), shf * _SIT_EX_RY(150.0f),
            (Vector4){{0.0f, 0.0f, 0.1f, 0.85f}});
        SituationDrawMetricsOverlay(cmd,
            (Vector2){{ swf * _SIT_EX_RX(20.0f), shf * _SIT_EX_RY(65.0f) }},
            (ColorRGBA){220, 255, 220, 255});
    }
}

#endif /* SIT_EXAMPLE_H */
