/***************************************************************************************************
 *  Situation — Node graph piano (keyboard → chromatic)
 *  ---------------------------------------------------
 *  Signal chain: Tone Synth → Echo → Reverb → Gain → device (unpatched tail sums to output).
 *
 *  Build (OpenGL, MinGW):
 *    build_examples.bat opengl node_graph_piano_demo
 *    (Links with -mwindows: no second console window.)
 *
 *  Piano: lower Z..M, upper Q..I (black keys 2 3 5 6 7 on number row). Hold = sound.
 *  F2 / F3     Octave down / up (±12, clamped).  ESC quit.
 *
 *  Waveform [ / ]     Cycle tone waveform (Sine Square Triangle Saw Noise).
 *  Master   - / =     Gain node trim (post-FX level, 0 … ~2).
 *  Echo     , .      Delay time   ; '      Feedback   9 0      Wet mix
 *  Reverb   K L      Room size    O P      Wet level
 ***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <math.h>
#include <stdio.h>

#include "font_data.h"

typedef struct {
    int situation_key;
    int midi_note;
} KeyNoteMapping;

static const KeyNoteMapping k_piano_keys[] = {
    {SIT_KEY_Z, 60},  {SIT_KEY_S, 61}, {SIT_KEY_X, 62}, {SIT_KEY_D, 63}, {SIT_KEY_C, 64}, {SIT_KEY_V, 65},
    {SIT_KEY_G, 66},  {SIT_KEY_B, 67}, {SIT_KEY_H, 68}, {SIT_KEY_N, 69}, {SIT_KEY_J, 70}, {SIT_KEY_M, 71},
    {SIT_KEY_Q, 72},  {SIT_KEY_2, 73}, {SIT_KEY_W, 74}, {SIT_KEY_3, 75}, {SIT_KEY_E, 76}, {SIT_KEY_R, 77},
    {SIT_KEY_5, 78},  {SIT_KEY_T, 79}, {SIT_KEY_6, 80}, {SIT_KEY_Y, 81}, {SIT_KEY_7, 82}, {SIT_KEY_U, 83},
    {SIT_KEY_I, 84},
    {0, -1},
};

static const char* k_waveform_label(int w) {
    static const char* names[] = {"Sine", "Square", "Triangle", "Saw", "Noise"};
    if (w < 0) {
        w = 0;
    }
    if (w > 4) {
        w = 4;
    }
    return names[w];
}

static float midi_to_hz(int note) {
    return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}

typedef struct {
    SituationNodeHandle tone;
    SituationNodeHandle echo;
    SituationNodeHandle verb;
    SituationNodeHandle gain;
} DemoNodes;

static SituationAudioGraph* build_demo_graph(DemoNodes* nodes) {
    SituationAudioGraph* g = SituationCreateGraph();
    if (!g) {
        return NULL;
    }

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle echo = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle verb = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle gain = SITUATION_INVALID_NODE_HANDLE;

    if (SituationCreateNode(g, SITUATION_NODE_TONE_SYNTH, &tone) != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_ECHO, &echo) != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_REVERB, &verb) != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_GAIN, &gain) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g);
        return NULL;
    }

    if (SituationCreatePatch(g, tone, 0, echo, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, echo, 0, verb, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, verb, 0, gain, 0, false) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g);
        return NULL;
    }

    SituationSetControl(g, tone, 0, 440.0f);
    SituationSetControl(g, tone, 1, 2.0f);
    SituationSetControl(g, tone, 2, 0.0f);
    SituationSetControl(g, tone, 3, 0.0f);

    SituationSetControl(g, echo, 0, 0.19f);
    SituationSetControl(g, echo, 1, 0.22f);
    SituationSetControl(g, echo, 2, 0.28f);

    SituationSetControl(g, verb, 0, 0.52f);
    SituationSetControl(g, verb, 1, 0.48f);
    SituationSetControl(g, verb, 2, 0.32f);
    SituationSetControl(g, verb, 3, 0.68f);
    SituationSetControl(g, verb, 4, 0.85f);

    SituationSetControl(g, gain, 0, 0.78f);

    nodes->tone = tone;
    nodes->echo = echo;
    nodes->verb = verb;
    nodes->gain = gain;
    return g;
}

static void ui_line(SituationCommandBuffer cmd, SituationFont* font, const char* text, float x, float y, float fs,
                    ColorRGBA col) {
    SituationCmdDrawTextEx(cmd, *font, text, (Vector2){{x, y}}, fs, 0.0f, col);
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void adjust_pressed(float* v, float step, float lo, float hi, int dec_key, int inc_key) {
    if (SituationIsKeyPressed(dec_key)) {
        *v = clampf(*v - step, lo, hi);
    }
    if (SituationIsKeyPressed(inc_key)) {
        *v = clampf(*v + step, lo, hi);
    }
}

int main(int argc, char** argv) {
    SituationInitInfo cfg = {
        .window_title = "Situation — Node graph piano",
        .window_width = 960,
        .window_height = 340,
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,
    };

    if (SituationInit(argc, argv, &cfg) != SITUATION_SUCCESS) {
        return -1;
    }

    SituationAudioGraph* default_graph = SituationGetActiveGraph();
    DemoNodes nodes = {SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE,
                      SITUATION_INVALID_NODE_HANDLE};

    SituationAudioGraph* demo_graph = build_demo_graph(&nodes);
    if (!demo_graph) {
        SituationShutdown();
        return -1;
    }

    if (SituationSetActiveGraph(demo_graph) != SITUATION_SUCCESS) {
        SituationDestroyGraph(demo_graph);
        SituationShutdown();
        return -1;
    }

    SituationFont font = {0};
    SituationLoadBitmapFontFromMemory(ibm_font_8x8, 8, 8, 256, &font);

    int octave = 0;
    int waveform = 2;
    float master_gain = 0.78f;
    float echo_delay = 0.19f;
    float echo_fb = 0.22f;
    float echo_wet = 0.28f;
    float rev_room = 0.52f;
    float rev_wet = 0.32f;

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }
        if (SituationIsKeyPressed(SIT_KEY_F2)) {
            octave--;
            if (octave < -2) {
                octave = -2;
            }
        }
        if (SituationIsKeyPressed(SIT_KEY_F3)) {
            octave++;
            if (octave > 2) {
                octave = 2;
            }
        }

        if (SituationIsKeyPressed(SIT_KEY_LEFT_BRACKET)) {
            waveform--;
            if (waveform < 0) {
                waveform = 4;
            }
        }
        if (SituationIsKeyPressed(SIT_KEY_RIGHT_BRACKET)) {
            waveform++;
            if (waveform > 4) {
                waveform = 0;
            }
        }

        adjust_pressed(&master_gain, 0.04f, 0.0f, 2.0f, SIT_KEY_MINUS, SIT_KEY_EQUAL);
        adjust_pressed(&echo_delay, 0.02f, 0.02f, 0.92f, SIT_KEY_COMMA, SIT_KEY_PERIOD);
        adjust_pressed(&echo_fb, 0.03f, 0.0f, 0.92f, SIT_KEY_SEMICOLON, SIT_KEY_APOSTROPHE);
        adjust_pressed(&echo_wet, 0.04f, 0.0f, 1.0f, SIT_KEY_9, SIT_KEY_0);
        adjust_pressed(&rev_room, 0.04f, 0.05f, 0.98f, SIT_KEY_K, SIT_KEY_L);
        adjust_pressed(&rev_wet, 0.04f, 0.0f, 1.0f, SIT_KEY_O, SIT_KEY_P);

        SituationSetControl(demo_graph, nodes.tone, 1, (float)waveform);
        SituationSetControl(demo_graph, nodes.gain, 0, master_gain);
        SituationSetControl(demo_graph, nodes.echo, 0, echo_delay);
        SituationSetControl(demo_graph, nodes.echo, 1, echo_fb);
        SituationSetControl(demo_graph, nodes.echo, 2, echo_wet);
        SituationSetControl(demo_graph, nodes.verb, 0, rev_room);
        SituationSetControl(demo_graph, nodes.verb, 2, rev_wet);

        int best_note = -1;
        for (const KeyNoteMapping* k = k_piano_keys; k->situation_key != 0; k++) {
            if (SituationIsKeyDown(k->situation_key)) {
                int n = k->midi_note + octave * 12;
                if (best_note < 0 || n < best_note) {
                    best_note = n;
                }
            }
        }

        if (best_note >= 0) {
            SituationSetControl(demo_graph, nodes.tone, 0, midi_to_hz(best_note));
            SituationSetControl(demo_graph, nodes.tone, 2, 0.34f);
        } else {
            SituationSetControl(demo_graph, nodes.tone, 2, 0.0f);
        }

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {.loadOp = SIT_LOAD_OP_CLEAR, .clear = {.color = {18, 16, 28, 255}}},
                .depth_attachment = {.loadOp = SIT_LOAD_OP_CLEAR, .clear = {.depth = 1.0f}},
            };
            SituationCmdBeginRenderPass(cmd, &pass);

            char line[192];
            float fs = 12.0f;
            float y = 14.0f;
            float x = 12.0f;
            ColorRGBA hi = {220, 225, 240, 255};
            ColorRGBA dim = {150, 160, 185, 255};

            snprintf(line, sizeof line, "Tone > Echo > Reverb > Gain   oct %+d (F2/F3)   wave %s  [ / ]",
                     octave, k_waveform_label(waveform));
            ui_line(cmd, &font, line, x, y, fs, hi);
            y += 18.0f;

            snprintf(line, sizeof line, "Master gain %.2f (-/=)   echo dly %.2f (,.) fb %.2f (;') wet %.2f (9/0)",
                     master_gain, echo_delay, echo_fb, echo_wet);
            ui_line(cmd, &font, line, x, y, fs, dim);
            y += 18.0f;

            snprintf(line, sizeof line, "Reverb room %.2f (K/L) wet %.2f (O/P)   Piano: Z..M  Q..I  ESC quit",
                     rev_room, rev_wet);
            ui_line(cmd, &font, line, x, y, fs, dim);

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    SituationUnloadFont(font);

    if (SituationSetActiveGraph(default_graph) != SITUATION_SUCCESS) {
        /* no console in -mwindows build */
    }
    SituationDestroyGraph(demo_graph);

    SituationShutdown();
    return 0;
}
