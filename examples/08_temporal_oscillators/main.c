/***************************************************************************************************
 *  Situation — 08: Temporal Oscillators
 *
 *  Eight independent metronomes at polyrhythmic beat divisions (1/4 … 4 beats @ 120 BPM).
 *  Each trigger flashes its circle white and fires a short tone — a visual + audible
 *  pulse machine you can mute, tempo-shift, and watch breathe via ping progress rings.
 *
 *  Keys:
 *    +/- (or numpad)  BPM (60–200)     1–8  mute channel
 *    Universal hotkeys via sit_example.h (ESC, F11, F9, P, F12)
 *
 *  Build:
 *    build\build_examples.bat static-opengl  08_temporal_oscillators
 *    build\build_examples.bat static-vulkan  08_temporal_oscillators
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>
#include <math.h>
#include <string.h>

#define NUM_OSC 8

static const float k_beat_mult[NUM_OSC] = {
    0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 4.0f
};
static const char* k_labels[NUM_OSC] = {
    "1/4", "1/2", "3/4", "1", "5/4", "3/2", "2", "4"
};
static const float k_pitch[NUM_OSC] = {
    523.3f, 587.3f, 659.3f, 698.5f, 784.0f, 880.0f, 988.0f, 1046.5f
};
static const ColorRGBA k_colors[NUM_OSC] = {
    {255,  80,  80, 255}, {255, 160,  60, 255}, {255, 230,  70, 255}, { 90, 220, 120, 255},
    { 60, 200, 255, 255}, {120, 140, 255, 255}, {200, 100, 255, 255}, {255, 120, 200, 255},
};

static float   g_bpm = 120.0f;
static uint64_t g_last_trig[NUM_OSC];
static float   g_flash[NUM_OSC];
static int     g_muted[NUM_OSC];

static void draw_disc(SituationCommandBuffer cmd, float cx, float cy, float r, Vector4 col)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){cx - r, cy - r, 0.0f});
    glm_scale(m, (vec3){r * 2.0f, r * 2.0f, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
}

static void txt(SituationCommandBuffer cmd, const char* s, float x, float y, float fs, ColorRGBA c)
{
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, s, (Vector2){{x, y}}, fs, 0.0f, c);
}

static void apply_bpm(void)
{
    const double beat = 60.0 / (double)g_bpm;
    for (int i = 0; i < NUM_OSC; ++i) {
        /* period_seconds is a half-cycle — oscillator spends `period` in state 0
         * then `period` in state 1, so one full trigger interval = 2 × period.
         * Divide the desired beat interval by 2 so triggers fire at the correct rate. */
        SituationSetTimerOscillatorPeriod(i, (beat * (double)k_beat_mult[i]) * 0.5);
    }
}

static void play_tick(int i)
{
    if (g_muted[i]) return;
    SituationPlayToneEx(SIT_WAVE_SINE, k_pitch[i], 0.38f, 0.0f,
                        0.001f, 0.015f, 0.0f, 0.05f, 0.025f);
}

static void sync_triggers(void)
{
    for (int i = 0; i < NUM_OSC; ++i) {
        g_last_trig[i] = SituationTimerGetOscillatorTriggerCount(i);
    }
}

static void draw_scene(SituationCommandBuffer cmd, int sw, int sh)
{
    const float grid_w = 720.0f;
    const float grid_h = 280.0f;
    const float ox = ((float)sw - grid_w) * 0.5f;
    const float oy = 120.0f;
    const float cell_w = grid_w / 4.0f;
    const float cell_h = grid_h / 2.0f;
    const float base_r = 36.0f;

    char buf[64];
    snprintf(buf, sizeof buf, "BPM %.0f  (beat = %.3fs)", g_bpm, 60.0 / g_bpm);
    txt(cmd, buf, ox, oy - 28.0f, 14.0f, (ColorRGBA){255, 230, 120, 255});
    txt(cmd, "256 independent oscillators — this demo uses 8 at polyrhythmic divisions",
        ox, oy - 12.0f, 10.0f, (ColorRGBA){130, 135, 170, 210});

    for (int i = 0; i < NUM_OSC; ++i) {
        int col = i % 4;
        int row = i / 4;
        float cx = ox + (float)col * cell_w + cell_w * 0.5f;
        float cy = oy + (float)row * cell_h + cell_h * 0.5f;

        /* Drive the ping timer so PingProgress reflects phase within the current beat cycle.
         * Without calling PingOscillator, last_ping_time never resets and the value
         * accumulates past 1.0 indefinitely. */
        SituationTimerPingOscillator(i);
        float prog = (float)SituationTimerGetPingProgress(i);
        if (prog < 0.0f) prog = 0.0f;
        if (prog > 1.0f) prog = 1.0f;

        Vector4 ring = {{
            k_colors[i].r / 255.0f * 0.35f,
            k_colors[i].g / 255.0f * 0.35f,
            k_colors[i].b / 255.0f * 0.35f,
            0.55f + 0.35f * prog
        }};
        draw_disc(cmd, cx, cy, base_r + 10.0f + prog * 14.0f, ring);

        float flash = g_flash[i];
        Vector4 fill = {{
            (k_colors[i].r / 255.0f) * (1.0f - flash) + flash,
            (k_colors[i].g / 255.0f) * (1.0f - flash) + flash,
            (k_colors[i].b / 255.0f) * (1.0f - flash) + flash,
            g_muted[i] ? 0.35f : 0.95f
        }};
        draw_disc(cmd, cx, cy, base_r, fill);

        snprintf(buf, sizeof buf, "%s beat", k_labels[i]);
        txt(cmd, buf, cx - 28.0f, cy + base_r + 8.0f, 10.0f,
            g_muted[i] ? (ColorRGBA){100, 100, 120, 180} : (ColorRGBA){200, 205, 230, 230});

        if (g_muted[i]) {
            txt(cmd, "MUTE", cx - 14.0f, cy - 5.0f, 10.0f, (ColorRGBA){180, 80, 80, 200});
        }
    }

    /* Oscillator 3 ping is driven in the per-oscillator loop above, so PingProgress
     * here is already reset on each beat. No fmod needed. */
    float pulse = (float)SituationTimerGetPingProgress(3);
    pulse = 1.0f - (pulse > 1.0f ? 1.0f : pulse);
    draw_disc(cmd, (float)sw * 0.5f, oy + grid_h + 36.0f, 8.0f + pulse * 10.0f,
              (Vector4){{1.0f, 1.0f, 1.0f, 0.25f + 0.5f * pulse}});
    txt(cmd, "downbeat (1 beat)", (float)sw * 0.5f - 48.0f, oy + grid_h + 58.0f, 10.0f,
        (ColorRGBA){160, 165, 200, 200});
}

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "08 — Temporal Oscillators") != SITUATION_SUCCESS) {
        return -1;
    }

    /* Example 08 uses the legacy tone pool (short percussive clicks). Disable the
     * library default graph so PlayToneEx mixes directly to the device buffer. */
    SituationSetActiveGraph(NULL);

    apply_bpm();
    sync_triggers();

    printf("[08] Polyrhythm pulse — %d oscillators @ %.0f BPM\n", NUM_OSC, g_bpm);

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) break;

        float dt = SituationGetFrameTime();

        if (SituationIsKeyPressed(SIT_KEY_MINUS) || SituationIsKeyPressed(SIT_KEY_KP_SUBTRACT)) {
            g_bpm = fmaxf(60.0f, g_bpm - 5.0f);
            apply_bpm();
            sync_triggers();
        }
        if (SituationIsKeyPressed(SIT_KEY_EQUAL) || SituationIsKeyPressed(SIT_KEY_KP_ADD)) {
            g_bpm = fminf(200.0f, g_bpm + 5.0f);
            apply_bpm();
            sync_triggers();
        }

        for (int k = 0; k < NUM_OSC; ++k) {
            int key = SIT_KEY_1 + k;
            if (SituationIsKeyPressed(key)) {
                g_muted[k] = !g_muted[k];
            }
        }

        if (!SituationIsAppPaused()) {
            for (int i = 0; i < NUM_OSC; ++i) {
                uint64_t tr = SituationTimerGetOscillatorTriggerCount(i);
                if (tr != g_last_trig[i]) {
                    g_last_trig[i] = tr;
                    g_flash[i] = 1.0f;
                    play_tick(i);
                }
                if (g_flash[i] > 0.0f) {
                    g_flash[i] = fmaxf(0.0f, g_flash[i] - dt * 6.0f);
                }
            }
        }

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) continue;

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationRenderPassInfo pass =
            SituationRenderPassInfoDefault(-1, (ColorRGBA){8, 10, 18, 255});
        SituationCmdBeginRenderPass(cmd, &pass);
        draw_scene(cmd, SituationGetRenderWidth(), SituationGetRenderHeight());
        SitExample_DrawHUD(cmd, "08 — Temporal Oscillators",
            "+/- BPM  1-8 mute  — edge-detect SituationTimerGetOscillatorTriggerCount");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    SitExample_Shutdown();
    return 0;
}
