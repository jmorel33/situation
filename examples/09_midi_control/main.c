/***************************************************************************************************
 *  Situation — 09: MIDI Control
 *
 *  Lists MIDI input ports, plays notes (graph tone synth or SituationPlayToneEx fallback),
 *  draws a piano keyboard + CC bars, and supports MIDI Learn on synth waveform (L).
 *
 *  Keys:
 *    Z–M / Q-row   on-screen piano (updates key glow + virtual MIDI when connected)
 *    L             MIDI learn — wiggle a knob to map CC → reverb room size
 *    V             create virtual MIDI loopback (no hardware required for full demo)
 *                  NOTE: V is also piano key C4 (MIDI 53). After the loopback is built, the
 *                  key plays normally through the graph. On the first press it builds the graph
 *                  AND fires a single C4 — expected behaviour, not a bug.
 *
 *  Build:
 *    build\build_examples.bat static-opengl  09_midi_control
 *    build\build_examples.bat static-vulkan  09_midi_control
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>
#include <math.h>
#include <string.h>

/* Tone synth control indices (from sit/aud/tone_synth_graph.h — internal, not in public API) */
#define EX09_TONE_CTRL_FREQUENCY  0
#define EX09_TONE_CTRL_WAVEFORM   1  /* 0=sine 1=sq 2=tri 3=saw 4=noise */
#define MAX_DEVS  32
#define PIANO_LO 48  /* C3 */
#define PIANO_HI 71  /* B4 */

typedef struct { int key; int note; } KeyMap;

static const KeyMap k_keys[] = {
    { SIT_KEY_Z, 48 }, { SIT_KEY_S, 49 }, { SIT_KEY_X, 50 }, { SIT_KEY_D, 51 },
    { SIT_KEY_C, 52 }, { SIT_KEY_V, 53 }, { SIT_KEY_G, 54 }, { SIT_KEY_B, 55 },
    { SIT_KEY_H, 56 }, { SIT_KEY_N, 57 }, { SIT_KEY_J, 58 }, { SIT_KEY_M, 59 },
    { SIT_KEY_Q, 60 }, { SIT_KEY_2, 61 }, { SIT_KEY_W, 62 }, { SIT_KEY_3, 63 },
    { SIT_KEY_E, 64 }, { SIT_KEY_R, 65 }, { SIT_KEY_5, 66 }, { SIT_KEY_T, 67 },
    { SIT_KEY_6, 68 }, { SIT_KEY_Y, 69 }, { SIT_KEY_7, 70 }, { SIT_KEY_U, 71 },
    { 0, -1 }
};

static SituationAudioGraph*   g_graph  = NULL;
static SituationNodeHandle    g_tone   = SITUATION_INVALID_NODE_HANDLE;
static SituationNodeHandle    g_reverb = SITUATION_INVALID_NODE_HANDLE;
static SituationNodeHandle    g_mixer  = SITUATION_INVALID_NODE_HANDLE;
static int                    g_midi   = -1;
static int                    g_virt   = 0;
static float                  g_glow[128];
static int                    g_key_note[512];
static SituationMidiDeviceInfo g_devs[MAX_DEVS];
static int                    g_ndev;
static char                   g_msg[80] = "";

static void txt(SituationCommandBuffer cmd, const char* s, float x, float y,
                float fs, ColorRGBA c)
{
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, s, (Vector2){{x, y}}, fs, 0.0f, c);
}

static void quad(SituationCommandBuffer cmd, float x, float y, float w, float h, Vector4 c)
{
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, c);
}

static int first_input_id(void)
{
    for (int i = 0; i < g_ndev; ++i) {
        if (g_devs[i].is_input) return g_devs[i].device_id;
    }
    return -1;
}

static SituationError wire_midi(int device_id)
{
    /* One PortMidi input stream per device — opening the same hw port twice crashes
     * on Windows. Notes/CC for the tone synth use this stream; reverb CC/learn is
     * keyboard-driven in this demo until the graph shares a single MIDI reader. */
    return SituationEnableMidiControl(g_graph, g_tone, device_id);
}

static int build_graph(int device_id)
{
    g_graph = SituationCreateGraph();
    if (!g_graph) return 0;

    if (SituationCreateNode(g_graph, SITUATION_NODE_TONE_SYNTH, &g_tone) != SITUATION_SUCCESS ||
        SituationCreateNode(g_graph, SITUATION_NODE_REVERB, &g_reverb) != SITUATION_SUCCESS ||
        SituationCreateNode(g_graph, SITUATION_NODE_MIXER, &g_mixer) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g_graph);
        g_graph = NULL;
        return 0;
    }

    if (SituationCreatePatch(g_graph, g_tone, 0, g_reverb, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g_graph, g_reverb, 0, g_mixer, 0, false) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g_graph);
        g_graph = NULL;
        return 0;
    }

    SituationSetControl(g_graph, g_tone, EX09_TONE_CTRL_WAVEFORM, 0.0f); /* sine */
    SituationSetControl(g_graph, g_reverb, 0, 0.55f);
    SituationSetControl(g_graph, g_mixer, 0, 0.85f);

    if (SituationTopologicalSort(g_graph) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g_graph);
        g_graph = NULL;
        return 0;
    }

    if (wire_midi(device_id) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g_graph);
        g_graph = NULL;
        return 0;
    }

    g_midi = device_id;
    if (SitExample_ActivateAudioGraph(g_graph) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g_graph);
        g_graph = NULL;
        return 0;
    }
    snprintf(g_msg, sizeof g_msg, "Graph: Tone -> Reverb -> Mixer (MIDI id %d)", device_id);
    return 1;
}

static void play_tone(int note, uint8_t vel)
{
    float hz = SITUATION_MIDI_NOTE_FREQUENCY[note];
    SituationPlayToneEx(SIT_WAVE_SINE, hz, (float)vel / 127.0f * 0.45f, 0.0f,
                        0.001f, 0.04f, 0.35f, 0.07f, 0.05f);
}

static void note_on(int note, uint8_t vel)
{
    if (note < 0 || note > 127) return;
    g_glow[note] = fmaxf(g_glow[note], (float)vel / 127.0f);

    if (g_graph && g_virt) {
        SituationVirtualMidiNoteOn((uint8_t)note, vel);
    } else {
        play_tone(note, vel);
    }
}

static void note_off(int note)
{
    if (note < 0 || note > 127) return;
    if (g_graph && g_virt) {
        SituationVirtualMidiNoteOff((uint8_t)note);
    }
}

static int is_black_key(int note)
{
    int n = note % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

static void draw_piano(SituationCommandBuffer cmd, float ox, float oy, float kw, float kh)
{
    const int lo = PIANO_LO;
    const int hi = PIANO_HI;
    const int count = hi - lo + 1;

    for (int i = 0; i < count; ++i) {
        int note = lo + i;
        if (is_black_key(note)) continue;
        float x = ox + (float)i * kw * 0.65f;
        float g = g_glow[note];
        Vector4 col = {{0.92f, 0.93f, 0.96f, 1.0f}};
        if (g > 0.05f) {
            col.x = 0.3f + 0.7f * g;
            col.y = 0.85f * g;
            col.z = 0.35f + 0.2f * g;
        }
        quad(cmd, x, oy, kw * 0.6f, kh, col);
    }

    for (int i = 0; i < count; ++i) {
        int note = lo + i;
        if (!is_black_key(note)) continue;
        float x = ox + (float)i * kw * 0.65f - kw * 0.2f;
        float g = g_glow[note];
        Vector4 col = {{0.12f, 0.12f, 0.14f, 1.0f}};
        if (g > 0.05f) {
            col.x = col.y = col.z = 0.15f + 0.75f * g;
        }
        quad(cmd, x, oy, kw * 0.38f, kh * 0.62f, col);
    }
}

static void draw_cc(SituationCommandBuffer cmd, float ox, float oy, float bw, float bh)
{
    /* Labels use control names; CC numbers are the default mapping (can be rebound via Learn). */
    static const char* labels[] = { "room", "damp", "wet", "dry" };
    float vals[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (g_graph) {
        for (int i = 0; i < 4; ++i) {
            SituationGetControl(g_graph, g_reverb, (uint32_t)i, &vals[i]);
        }
    }

    for (int i = 0; i < 4; ++i) {
        float y = oy + (float)i * (bh + 10.0f);
        quad(cmd, ox, y, bw, bh, (Vector4){{0.15f, 0.16f, 0.22f, 1.0f}});
        quad(cmd, ox, y, bw * vals[i], bh, (Vector4){{0.35f, 0.75f, 1.0f, 0.95f}});
        char buf[32];
        snprintf(buf, sizeof buf, "%s  %.2f", labels[i], vals[i]);
        txt(cmd, buf, ox + bw + 12.0f, y + 2.0f, 11.0f, (ColorRGBA){170, 185, 220, 255});
    }
}

static void draw_devices(SituationCommandBuffer cmd, int sw, int sh)
{
    float y = 140.0f;
    txt(cmd, "No MIDI input found — plug in a keyboard or controller, then restart.",
        40.0f, y, 14.0f, (ColorRGBA){255, 200, 120, 255});
    y += 28.0f;
    txt(cmd, "Press V for virtual MIDI loopback (full graph + learn demo without hardware).",
        40.0f, y, 12.0f, (ColorRGBA){140, 200, 255, 230});
    y += 32.0f;
    txt(cmd, "SituationListMidiDevices:", 40.0f, y, 12.0f, (ColorRGBA){130, 140, 170, 220});
    y += 20.0f;

    if (g_ndev == 0) {
        txt(cmd, "  (empty — zero PortMidi devices)", 48.0f, y, 11.0f,
            (ColorRGBA){180, 120, 120, 255});
        return;
    }

    char line[160];
    for (int i = 0; i < g_ndev && y < (float)sh - 80.0f; ++i) {
        snprintf(line, sizeof line, "  [%d] %s  (%s%s)",
                 g_devs[i].device_id, g_devs[i].device_name,
                 g_devs[i].is_input ? "in" : "",
                 g_devs[i].is_output ? "out" : "");
        txt(cmd, line, 48.0f, y, 11.0f, (ColorRGBA){190, 195, 220, 240});
        y += 16.0f;
    }
}

static void poll_keys(void)
{
    for (const KeyMap* k = k_keys; k->key; ++k) {
        if (SituationIsKeyPressed(k->key)) {
            g_key_note[k->key] = k->note;
            note_on(k->note, 100);
        }
        if (SituationIsKeyReleased(k->key)) {
            int n = g_key_note[k->key];
            if (n >= 0) note_off(n);
            g_key_note[k->key] = -1;
        }
    }
}

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "09 — MIDI Control") != SITUATION_SUCCESS) {
        return -1;
    }

    memset(g_key_note, -1, sizeof g_key_note);
    SitExample_InitAudioRegistry();

    g_ndev = SituationListMidiDevices(g_devs, MAX_DEVS);
    printf("[09] SituationListMidiDevices: %d port(s)\n", g_ndev);
    for (int i = 0; i < g_ndev; ++i) {
        printf("  [%d] %s (%s%s)\n", g_devs[i].device_id, g_devs[i].device_name,
               g_devs[i].is_input ? "in " : "", g_devs[i].is_output ? "out" : "");
    }

    /* Always start with virtual MIDI + graph (same reliable path as examples 04/06/19).
     * Hardware ports are listed above; press H to attach the first input instead. */
    if (SituationSetupVirtualMidiLoopback(&g_midi) == SITUATION_SUCCESS) {
        g_virt = 1;
        if (build_graph(g_midi)) {
            printf("[09] Virtual MIDI graph active — keyboard plays through tone synth\n");
        } else {
            fprintf(stderr, "[09] Graph build failed — audio disabled\n");
            snprintf(g_msg, sizeof g_msg, "Graph build failed — see stderr");
        }
    } else {
        fprintf(stderr, "[09] Virtual MIDI loopback failed — press keys for legacy tone fallback\n");
        snprintf(g_msg, sizeof g_msg, "No virtual MIDI — keyboard uses legacy tones");
    }

    int was_learning = 0;

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) break;

        float dt = SituationGetFrameTime();

        if (SituationIsKeyPressed(SIT_KEY_H) && g_virt) {
            int hw = first_input_id();
            if (hw >= 0 && g_graph) {
                SituationSetActiveGraph(NULL);
                SituationDisableMidiControl(g_graph, g_tone);
                if (wire_midi(hw) == SITUATION_SUCCESS) {
                    g_midi = hw;
                    g_virt = 0;
                    SitExample_ActivateAudioGraph(g_graph);
                    snprintf(g_msg, sizeof g_msg, "Hardware MIDI input id=%d", hw);
                    printf("[09] Switched to hardware MIDI input id=%d\n", hw);
                }
            }
        }

        if (SituationIsKeyPressed(SIT_KEY_V) && !g_virt) {
            if (SituationSetupVirtualMidiLoopback(&g_midi) == SITUATION_SUCCESS) {
                g_virt = 1;
                if (g_graph) {
                    SituationSetActiveGraph(NULL);
                    SituationDisableMidiControl(g_graph, g_tone);
                    wire_midi(g_midi);
                    SitExample_ActivateAudioGraph(g_graph);
                    snprintf(g_msg, sizeof g_msg, "Virtual MIDI id=%d", g_midi);
                } else if (build_graph(g_midi)) {
                    printf("[09] Virtual MIDI loopback — device id=%d\n", g_midi);
                }
            }
        }

        if (SituationIsKeyPressed(SIT_KEY_L) && g_graph) {
            if (!SituationIsLearning(g_graph, g_tone)) {
                (void)SituationEnableMidiLearn(g_graph, g_tone);
            }
            SituationError err = SituationStartMidiLearn(
                g_graph, g_tone, EX09_TONE_CTRL_WAVEFORM, "waveform", 0.0f, 4.0f, 3);
            if (err == SITUATION_SUCCESS) {
                snprintf(g_msg, sizeof g_msg, "LEARN: wiggle a knob (CC -> waveform)");
                printf("[09] MIDI learn started — move a controller\n");
            }
        }

        if (g_graph) {
            int learning = SituationIsLearning(g_graph, g_tone);
            if (was_learning && !learning) {
                snprintf(g_msg, sizeof g_msg, "Learn complete (or timed out)");
            }
            was_learning = learning;
        }

        poll_keys();

        for (int i = 0; i < 128; ++i) {
            if (g_glow[i] > 0.0f) {
                g_glow[i] = fmaxf(0.0f, g_glow[i] - dt * 4.0f);
            }
        }

        if (SituationIsAppPaused()) continue;
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) continue;

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        int sw = SituationGetRenderWidth();
        int sh = SituationGetRenderHeight();
        SituationRenderPassInfo pass =
            SituationRenderPassInfoDefault(-1, (ColorRGBA){8, 10, 16, 255});
        SituationCmdBeginRenderPass(cmd, &pass);

        txt(cmd, "09 — MIDI Control", 24.0f, 72.0f, 18.0f, (ColorRGBA){255, 230, 140, 255});
        txt(cmd, g_msg, 24.0f, 96.0f, 11.0f, (ColorRGBA){150, 170, 210, 230});

        if (g_graph) {
            int learning = SituationIsLearning(g_graph, g_reverb);
            if (learning) {
                txt(cmd, ">>> MIDI LEARN ACTIVE — move a knob on your controller <<<",
                    24.0f, 116.0f, 12.0f, (ColorRGBA){255, 120, 80, 255});
            }
            draw_piano(cmd, 80.0f, 160.0f, 28.0f, 120.0f);
            txt(cmd, "Piano keys (Z-M, Q-U): glow + virtual MIDI when graph connected",
                80.0f, 292.0f, 10.0f, (ColorRGBA){120, 130, 160, 200});
            draw_cc(cmd, 80.0f, 330.0f, 320.0f, 14.0f);
            txt(cmd, "Reverb CC bars (default mapping CC16-19) — L remaps room size via MIDI Learn",
                80.0f, 420.0f, 10.0f, (ColorRGBA){120, 130, 160, 200});
        } else {
            draw_devices(cmd, sw, sh);
            draw_piano(cmd, 80.0f, (float)sh - 220.0f, 28.0f, 100.0f);
            txt(cmd, "Graph failed — keyboard uses legacy SituationPlayToneEx",
                80.0f, (float)sh - 100.0f, 10.0f, (ColorRGBA){120, 130, 160, 200});
        }

        SitExample_DrawHUD(cmd, "09 — MIDI Control",
            "Z-M/Q-U piano  L:learn  H:hw MIDI  V:virtual MIDI  — graph tone synth + reverb");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    SituationVirtualMidiControlChange(0, 123, 0);
    SitExample_DestroyAudioGraph(&g_graph, g_tone);
    if (g_virt) SitExample_TeardownVirtualMidi();
    SitExample_Shutdown();
    return 0;
}
