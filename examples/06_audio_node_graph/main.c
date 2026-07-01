/***************************************************************************************************
 *  Situation — 06: Audio Node Graph
 *
 *  Live ASCII diagram of an audio processing chain:
 *    Tone Synth → EQ 4-Band → Reverb → Mixer → output
 *
 *  What to look for:
 *    - The signal-flow diagram updates as you change controls
 *    - Press C to attempt a feedback patch (cycle detection rejects it)
 *    - S / L save and reload graph_session.json
 *
 *  Keys:
 *    Q/W/E     waveform (sine / square / saw)
 *    1/2/3     frequency presets (220 / 440 / 880 Hz)
 *    UP/DOWN   reverb room size
 *    S / L     save / load graph_session.json
 *    C         try illegal feedback patch (prints error)
 *    SPACE     gate tone on / off
 *
 *  Build:
 *    build\build_examples.bat static-opengl  06_audio_node_graph
 *    build\build_examples.bat static-vulkan  06_audio_node_graph
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>
#include <math.h>
#include <string.h>

#define GRAPH_FILE "graph_session.json"

/* Minimal node view — field layout must match sit/aud/node_graph.h SituationNode. */
typedef struct SitNodeView {
    SituationNodeType type;
    SituationNodeHandle handle;
    uint16_t generation;
    const SituationDeviceMetadata* metadata;
    void* device_data;
    SituationAudioPort* audio_inputs;
    SituationAudioPort* audio_outputs;
    SituationControlPort* ctrl_inputs;
    SituationControlPort* ctrl_outputs;
    float* control_values;
    SituationPatch* input_patches;
    int num_input_patches;
    SituationPatch* output_patches;
    int num_output_patches;
    bool is_active;
    bool needs_processing;
    struct SIT_MidiDevice* midi_device;
    void* midi_input;
    int midi_device_id;
    struct SIT_MidiLearnState* learn_state;
} SitNodeView;

#define TONE_CTRL_FREQ     0
#define TONE_CTRL_WAVE     1
#define TONE_CTRL_VOLUME   2
#define TONE_CTRL_HOLD     8

#define EQ_CTRL_PEAK_GAIN  6

#define REV_CTRL_ROOM      0
#define REV_CTRL_WET       2

typedef struct {
    SituationNodeHandle tone;
    SituationNodeHandle eq;
    SituationNodeHandle reverb;
    SituationNodeHandle mixer;
} GraphNodes;

static SituationAudioGraph* g_graph = NULL;
static GraphNodes           g_nodes;
static int                  g_midi_in = -1;
static int                  g_gate    = 1;
static float                g_freq    = 440.0f;
static int                  g_wave    = 0; /* 0 sine, 1 square, 3 saw */
static float                g_room    = 0.65f;
static char                 g_status[96] = "Ready";

static const char* wave_label(int w)
{
    switch (w) {
        case 0: return "Sine";
        case 1: return "Square";
        case 3: return "Saw";
        default: return "?";
    }
}

static int hz_to_midi(float hz)
{
    return (int)(69.0f + 12.0f * log2f(hz / 440.0f) + 0.5f);
}

static void txt(SituationCommandBuffer cmd, const char* s, float x, float y,
                float fs, ColorRGBA c)
{
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, s,
                           (Vector2){{x, y}}, fs, 0.0f, c);
}

static SituationNodeHandle find_node_by_type(SituationAudioGraph* g, SituationNodeType want)
{
    for (uint32_t gen = 1; gen <= 8u; ++gen) {
        for (uint32_t idx = 0; idx < (uint32_t)SITUATION_MAX_NODES; ++idx) {
            SituationNodeHandle h = (SituationNodeHandle)((gen << 16) | idx);
            SituationNode* raw = SituationGetNode(g, h);
            if (!raw) continue;
            SitNodeView* n = (SitNodeView*)raw;
            if (n->is_active && n->type == want) return h;
        }
    }
    return SITUATION_INVALID_NODE_HANDLE;
}

static void resolve_handles(void)
{
    g_nodes.tone   = find_node_by_type(g_graph, SITUATION_NODE_TONE_SYNTH);
    g_nodes.eq     = find_node_by_type(g_graph, SITUATION_NODE_EQ_4BAND);
    g_nodes.reverb = find_node_by_type(g_graph, SITUATION_NODE_REVERB);
    g_nodes.mixer  = find_node_by_type(g_graph, SITUATION_NODE_MIXER);
}

static void gate_tone(int on)
{
    int note = hz_to_midi(g_freq);
    if (on && !g_gate) {
        SituationVirtualMidiNoteOn((uint8_t)note, 100);
        g_gate = 1;
    } else if (!on && g_gate) {
        SituationVirtualMidiNoteOff((uint8_t)note);
        g_gate = 0;
    }
}

static void push_controls(void)
{
    if (!g_graph) return;

    SituationSetControl(g_graph, g_nodes.tone,   TONE_CTRL_FREQ,   g_freq);
    SituationSetControl(g_graph, g_nodes.tone,   TONE_CTRL_WAVE,   (float)g_wave);
    SituationSetControl(g_graph, g_nodes.tone,   TONE_CTRL_VOLUME, 0.40f);
    SituationSetControl(g_graph, g_nodes.tone,   TONE_CTRL_HOLD,  -1.0f);
    SituationSetControl(g_graph, g_nodes.reverb, REV_CTRL_ROOM,    g_room);
    SituationSetControl(g_graph, g_nodes.reverb, REV_CTRL_WET,     0.40f);
    SituationSetControl(g_graph, g_nodes.reverb, 3,                0.60f);
    SituationSetControl(g_graph, g_nodes.mixer,  0,                0.85f);

    float peak_gain = 0.0f;
    SituationGetControl(g_graph, g_nodes.eq, EQ_CTRL_PEAK_GAIN, &peak_gain);
    (void)peak_gain;
}

static SituationAudioGraph* build_graph(void)
{
    SituationAudioGraph* g = SituationCreateGraph();
    if (!g) return NULL;

    if (SituationCreateNode(g, SITUATION_NODE_TONE_SYNTH, &g_nodes.tone) != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_EQ_4BAND,   &g_nodes.eq)   != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_REVERB,     &g_nodes.reverb) != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_MIXER,      &g_nodes.mixer)  != SITUATION_SUCCESS) {
        SituationDestroyGraph(g);
        return NULL;
    }

    if (SituationCreatePatch(g, g_nodes.tone,   0, g_nodes.eq,     0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, g_nodes.eq,     0, g_nodes.reverb, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, g_nodes.reverb, 0, g_nodes.mixer,  0, false) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g);
        return NULL;
    }

    SituationSetControl(g, g_nodes.eq, 5, 1200.0f);
    SituationSetControl(g, g_nodes.eq, EQ_CTRL_PEAK_GAIN, 2.0f);

    if (SituationTopologicalSort(g) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g);
        return NULL;
    }

    push_controls();
    return g;
}

static void try_cycle_patch(void)
{
    SituationError err = SituationCreatePatch(
        g_graph, g_nodes.mixer, 0, g_nodes.tone, 0, false);
    if (err == SITUATION_ERROR_NODE_PATCH_CYCLE_DETECTED) {
        snprintf(g_status, sizeof g_status, "Cycle blocked (Mixer->Tone)");
        printf("[06] Cycle detected — patch rejected\n");
    } else if (err == SITUATION_SUCCESS) {
        snprintf(g_status, sizeof g_status, "WARNING: cycle patch accepted");
        printf("[06] WARNING: cycle patch was accepted\n");
    } else {
        snprintf(g_status, sizeof g_status, "Patch failed (%d)", (int)err);
    }
}

static void draw_diagram(SituationCommandBuffer cmd, int sw, int sh, double t)
{
    const float cx = (float)sw * 0.5f;
    float y = 70.0f;
    const float dy = 52.0f;
    const float box_w = 280.0f;
    const float box_h = 36.0f;
    const float pulse = 0.5f + 0.5f * sinf((float)t * 4.0f);

    ColorRGBA hi  = {255, 220, 80, 255};
    ColorRGBA dim = {140, 150, 190, 220};
    ColorRGBA ok  = {120, 255, 160, 255};

    txt(cmd, "AUDIO NODE GRAPH", cx - 80.0f, 36.0f, 16.0f, hi);

    float peak = 0.0f;
    SituationGetControl(g_graph, g_nodes.eq, EQ_CTRL_PEAK_GAIN, &peak);

    char line[96];
    const char* boxes[] = {"Tone Synth", "EQ 4-Band", "Reverb", "Mixer", "OUTPUT"};
    char detail[5][80];
    snprintf(detail[0], sizeof detail[0], "%.0f Hz  %s  %s",
             g_freq, wave_label(g_wave), g_gate ? "[ON]" : "[OFF]");
    snprintf(detail[1], sizeof detail[1], "peak +%.1f dB @ 1.2 kHz", peak);
    snprintf(detail[2], sizeof detail[2], "room %.2f  wet 0.40", g_room);
    snprintf(detail[3], sizeof detail[3], "master 0.85");
    snprintf(detail[4], sizeof detail[4], "device out");

    for (int i = 0; i < 5; ++i) {
        float bx = cx - box_w * 0.5f;
        Vector4 fill = {{0.10f, 0.12f, 0.18f, 0.95f}};
        if (i < 4) {
            fill.r += 0.06f * pulse;
            fill.g += 0.04f * pulse;
        }
        mat4 m;
        glm_mat4_identity(m);
        glm_translate(m, (vec3){bx, y, 0.0f});
        glm_scale(m, (vec3){box_w, box_h, 1.0f});
        SituationCmdDrawQuad(cmd, m, fill);

        txt(cmd, boxes[i], bx + 10.0f, y + 4.0f, 13.0f, ok);
        txt(cmd, detail[i], bx + 10.0f, y + 20.0f, 11.0f, dim);

        if (i < 4) {
            float ax = cx;
            float ay = y + box_h;
            mat4 arr;
            glm_mat4_identity(arr);
            glm_translate(arr, (vec3){ax - 3.0f, ay + 4.0f, 0.0f});
            glm_scale(arr, (vec3){6.0f, 10.0f + 6.0f * pulse, 1.0f});
            SituationCmdDrawQuad(cmd, arr, (Vector4){{0.4f, 0.7f, 1.0f, 0.85f}});
        }
        y += dy;
    }

    txt(cmd, g_status, 24.0f, (float)sh - 48.0f, 11.0f, hi);

    txt(cmd, "Topological order:", 24.0f, y + 8.0f, 12.0f, dim);
    y += 26.0f;
    static const char* order[] = {
        "Tone Synth", "EQ 4-Band", "Reverb", "Mixer"
    };
    for (int i = 0; i < 4; ++i) {
        snprintf(line, sizeof line, "  %d. %s", i + 1, order[i]);
        txt(cmd, line, 32.0f, y, 11.0f, (ColorRGBA){180, 190, 230, 240});
        y += 14.0f;
    }
}

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "06 — Audio Node Graph") != SITUATION_SUCCESS) {
        return -1;
    }

    SitExample_InitAudioRegistry();

    g_graph = build_graph();
    if (!g_graph) {
        fprintf(stderr, "[06] Failed to build graph\n");
        SitExample_Shutdown();
        return -1;
    }

    if (SitExample_WireToneSynthVirtualMidi(g_graph, g_nodes.tone, "06", &g_midi_in) != SITUATION_SUCCESS) {
        fprintf(stderr, "[06] MIDI wiring failed — SPACE gate will be silent\n");
    }

    if (SitExample_ActivateAudioGraph(g_graph) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g_graph);
        SitExample_Shutdown();
        return -1;
    }

    gate_tone(1);
    printf("[06] Graph ready — chain: Tone -> EQ -> Reverb -> Mixer\n");

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) break;

        bool dirty = false;

        if (SituationIsKeyPressed(SIT_KEY_Q)) { g_wave = 0; dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_W)) { g_wave = 1; dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_E)) { g_wave = 3; dirty = true; }

        if (SituationIsKeyPressed(SIT_KEY_1)) { g_freq = 220.0f; dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_2)) { g_freq = 440.0f; dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_3)) { g_freq = 880.0f; dirty = true; }

        if (SituationIsKeyDown(SIT_KEY_UP)) {
            g_room = fminf(g_room + SituationGetFrameTime() * 0.35f, 1.0f);
            dirty = true;
        }
        if (SituationIsKeyDown(SIT_KEY_DOWN)) {
            g_room = fmaxf(g_room - SituationGetFrameTime() * 0.35f, 0.05f);
            dirty = true;
        }

        if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
            gate_tone(!g_gate);
        }

        if (SituationIsKeyPressed(SIT_KEY_C)) {
            try_cycle_patch();
        }

        if (SituationIsKeyPressed(SIT_KEY_S)) {
            SituationError err = SituationSaveGraphToFile(g_graph, GRAPH_FILE);
            if (err == SITUATION_SUCCESS) {
                snprintf(g_status, sizeof g_status, "Saved %s", GRAPH_FILE);
                printf("[06] Saved %s\n", GRAPH_FILE);
            } else {
                snprintf(g_status, sizeof g_status, "Save failed");
            }
        }

        if (SituationIsKeyPressed(SIT_KEY_L)) {
            gate_tone(0);
            SituationSetActiveGraph(NULL);
            SituationDestroyGraph(g_graph);
            g_graph = SituationCreateGraph();
            SituationError err = SITUATION_ERROR_NOT_INITIALIZED;
            if (g_graph) {
                err = SituationLoadGraphFromFile(g_graph, GRAPH_FILE, NULL, 0);
            }
            if (g_graph && err == SITUATION_SUCCESS) {
                resolve_handles();
                SituationTopologicalSort(g_graph);
                SituationSetActiveGraph(g_graph);
                SituationEnableMidiControl(g_graph, g_nodes.tone, g_midi_in);
                SituationGetControl(g_graph, g_nodes.tone, TONE_CTRL_FREQ, &g_freq);
                {
                    float wf = 0.0f;
                    SituationGetControl(g_graph, g_nodes.tone, TONE_CTRL_WAVE, &wf);
                    g_wave = (int)wf;
                }
                SituationGetControl(g_graph, g_nodes.reverb, REV_CTRL_ROOM, &g_room);
                snprintf(g_status, sizeof g_status, "Loaded %s", GRAPH_FILE);
                printf("[06] Loaded %s\n", GRAPH_FILE);
                gate_tone(1);
            } else {
                if (g_graph) SituationDestroyGraph(g_graph);
                g_graph = build_graph();
                SituationSetActiveGraph(g_graph);
                SituationEnableMidiControl(g_graph, g_nodes.tone, g_midi_in);
                snprintf(g_status, sizeof g_status, "Load failed — rebuilt default graph");
                gate_tone(1);
            }
        }

        if (dirty) {
            if (g_gate) gate_tone(0);
            push_controls();
            if (g_gate) gate_tone(1);
        }

        if (SituationIsAppPaused()) continue;

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) continue;

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationRenderPassInfo pass =
            SituationRenderPassInfoDefault(-1, (ColorRGBA){10, 12, 18, 255});
        SituationCmdBeginRenderPass(cmd, &pass);
        draw_diagram(cmd, SituationGetRenderWidth(), SituationGetRenderHeight(),
                     SituationTimerGetTime());
        SitExample_DrawHUD(cmd, "06 — Audio Node Graph",
            "Q/W/E:wave  1/2/3:Hz  UP/DOWN:reverb  S/L:save/load  C:cycle  SPACE:gate");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    gate_tone(0);
    SitExample_DestroyAudioGraph(&g_graph, g_nodes.tone);
    SitExample_TeardownVirtualMidi();
    SitExample_Shutdown();
    return 0;
}
