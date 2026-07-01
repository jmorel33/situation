/***************************************************************************************************
 *  Situation -- 04: Play a Sound
 *
 *  Polyphonic keyboard synthesizer -- 100% procedural, no audio files.
 *  Demonstrates the audio node graph, tone synth node, virtual MIDI loopback,
 *  ADSR, sub-oscillator, LFO, PWM, SVF filter, and effects (echo, reverb,
 *  chorus, phaser, overdrive).
 *
 *  The SITUATION_NODE_TONE_SYNTH is a 16-voice polyphonic synth engine inside the
 *  audio node graph.  Notes are triggered via virtual MIDI NoteOn/NoteOff messages
 *  -- not the legacy SituationPlayToneEx tone pool.  This is the correct way to
 *  drive the synth so that every note passes through the full effects chain
 *  (tone -> overdrive -> chorus -> phaser -> echo -> reverb -> gain -> peak meter).
 *
 *  Piano keys -- hold for sustain, release for note-off (two octaves):
 *
 *    Lower octave (C3):  Z S X D C  V G B H N J M
 *    Upper octave (C4):  Q 2 W 3 E  R 5 T 6 Y 7 U  8 I 9 O 0 P [ ]
 *    Sustain pedal:      SPACE  (CC64 hold-pedal -- synth handles release)
 *    Octave shift:       F2 / F3  (down / up, ±1 octave)
 *
 *  ADSR / oscillator / filter / sub / LFO / effects — all via travel edit (F1):
 *    Toggle edit         F1
 *    Section             PgUp / PgDn     ADSR -> OSC -> Filter -> Sub -> LFO -> FX
 *    Parameter           Up / Down       Prev / next in section
 *    Adjust value        Left / Right    Hold repeats 10/s (Shift = fine)
 *    Jump param          Home / End
 *    Save patch          Enter
 *
 *  Piano, sustain, octave, FX preset, master gain stay on dedicated keys (see below).
 *
 *  What this example teaches:
 *    - SituationCreateGraph / SituationCreateNode / SituationCreatePatch
 *    - SITUATION_NODE_TONE_SYNTH with TONE_CTRL_* controls
 *    - Virtual MIDI loopback: SituationSetupVirtualMidiLoopback + SituationEnableMidiControl
 *    - Note-on/off via SituationVirtualMidiNoteOn/Off (correct polyphonic path)
 *    - CC64 sustain pedal via SituationVirtualMidiControlChange
 *    - SituationSetControl for live synth parameter changes
 *    - SituationSetActiveGraph / SituationInitDeviceRegistry
 *
 *  Universal hotkeys (sit_example.h):
 *    ESC -- quit   F11 -- borderless fullscreen   F9 -- VSync   P -- pause   F12 -- screenshot   (env on [/] to avoid conflict)
 *
 *  Build:
 *    build\build_examples.bat static-opengl  04_play_a_sound
 *    build\build_examples.bat static-vulkan  04_play_a_sound
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include "../../sit/aud/fx/peak_meter.h"
#include <cglm/cglm.h>
#include <string.h>
#include <math.h>

/* ── Piano layout (logical keys) → resolved to scancodes at init ─────────
 * Notes use physical scancode + logical key (see init_piano_scancode_maps).
 * Numpad is options-only; library strips OS main-row digit aliases when KP is down. */
typedef struct { int key; int midi_note; } KeyDef;
typedef struct { int scancode; int logical_key; int midi_note; } PianoMap;

#define MAX_PIANO_KEYS 48

static PianoMap s_piano_keys[MAX_PIANO_KEYS];
static int      s_piano_key_count = 0;
static bool     s_piano_held[MAX_PIANO_KEYS];
static int      s_piano_note[MAX_PIANO_KEYS]; /* MIDI note sent per slot */

/* Lower octave anchor: C3 = MIDI 48 */
static const KeyDef k_lower[] = {
    { SIT_KEY_Z, 48 }, /* C3  */
    { SIT_KEY_S, 49 }, /* C#3 */
    { SIT_KEY_X, 50 }, /* D3  */
    { SIT_KEY_D, 51 }, /* D#3 */
    { SIT_KEY_C, 52 }, /* E3  */
    { SIT_KEY_V, 53 }, /* F3  */
    { SIT_KEY_G, 54 }, /* F#3 */
    { SIT_KEY_B, 55 }, /* G3  */
    { SIT_KEY_H, 56 }, /* G#3 */
    { SIT_KEY_N, 57 }, /* A3  */
    { SIT_KEY_J, 58 }, /* A#3 */
    { SIT_KEY_M, 59 }, /* B3  */
    { 0, -1 }
};

/* Upper octave anchor: C4 = MIDI 60 */
static const KeyDef k_upper[] = {
    { SIT_KEY_Q,             60 }, /* C4  */
    { SIT_KEY_2,             61 }, /* C#4 */
    { SIT_KEY_W,             62 }, /* D4  */
    { SIT_KEY_3,             63 }, /* D#4 */
    { SIT_KEY_E,             64 }, /* E4  */
    { SIT_KEY_R,             65 }, /* F4  */
    { SIT_KEY_5,             66 }, /* F#4 */
    { SIT_KEY_T,             67 }, /* G4  */
    { SIT_KEY_6,             68 }, /* G#4 */
    { SIT_KEY_Y,             69 }, /* A4  */
    { SIT_KEY_7,             70 }, /* A#4 */
    { SIT_KEY_U,             71 }, /* B4  */
    { SIT_KEY_8,             72 }, /* C5  */
    { SIT_KEY_I,             73 }, /* C#5 */
    { SIT_KEY_9,             74 }, /* D5  */
    { SIT_KEY_O,             75 }, /* D#5 */
    { SIT_KEY_0,             76 }, /* E5  */
    { SIT_KEY_P,             77 }, /* F5  */
    { SIT_KEY_LEFT_BRACKET,  79 }, /* G5  */
    { SIT_KEY_RIGHT_BRACKET, 81 }, /* A5  */
    { 0, -1 }
};

static void init_piano_scancode_maps(void)
{
    s_piano_key_count = 0;
    const KeyDef* tables[] = { k_lower, k_upper };
    for (int t = 0; t < 2; t++) {
        for (const KeyDef* kd = tables[t]; kd->key != 0; kd++) {
            int sc = SituationGetKeyScancode(kd->key);
            if (sc < 0 || sc >= SITUATION_MAX_SCANCODES)
                continue;
            if (s_piano_key_count >= MAX_PIANO_KEYS)
                break;
            s_piano_keys[s_piano_key_count++] = (PianoMap){ sc, kd->key, kd->midi_note };
        }
    }
}

/* ── Graph nodes ──────────────────────────────────────────────────────── */
typedef struct {
    SituationNodeHandle tone;
    SituationNodeHandle overdrive;
    SituationNodeHandle chorus;
    SituationNodeHandle phaser;
    SituationNodeHandle echo;
    SituationNodeHandle reverb;
    SituationNodeHandle gain;
    SituationNodeHandle peak_meter;
    SituationNodeHandle mixer;
} Nodes;

/* Prefix of sit/aud/node_graph.h SituationNode — enough to read analyzer device_data. */
typedef struct SitMeterNodeView {
    SituationNodeType type;
    SituationNodeHandle handle;
    uint16_t generation;
    const SituationDeviceMetadata* metadata;
    void* device_data;
} SitMeterNodeView;

/* ── Small helpers ────────────────────────────────────────────────────── */
static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── Synth state ──────────────────────────────────────────────────────── */

/* Per piano-map slot hold tracking (indexed by s_piano_keys[], not SIT_KEY_*). */

static int   s_octave   = 0;
static float s_frequency = 440.0f;
static float s_volume   = 0.0f;   /* manual path; MIDI uses velocity + CC7/11 */
static int   s_waveform = 3;   /* 0=Sine 1=Pulse 2=Tri 3=Saw 4=Noise — saw shows filter Q */
static float s_attack   = 0.01f;
static float s_decay    = 0.08f;
static float s_sustain  = 0.75f;
static float s_release  = 0.22f;
static float s_hold     = -1.0f;
static float s_pwm      = 0.50f;
static float s_pan      = 0.0f;
static bool  s_mono     = false;
static bool  s_portamento = false;
static float s_portamento_time = 0.25f;
static float s_portamento_speed = 0.0f;
static bool  s_glide_reset_on_key = false;
static int   s_patch_slot = 0;

static int   s_filt_mode = 1;   /* 0=Off 1=LP 2=HP 3=BP 4=Notch 5=Allpass (LP default, Polysonix parity) */
static int   s_filt_poles = 2;  /* 1..4 → 6/12/18/24 dB/oct (TONE_CTRL_FILTER_POLES) */
static float s_filt_cut  = 800.f;
static float s_filt_res  = 4.0f;
static float s_filt_env  = 0.0f;
static float s_filt_drive = 1.0f;
static float s_filt_keytrack = 0.0f;
static int   s_filt_os   = 0;
static float s_filt_env_range = 4000.0f;

static float s_sub_level  = 0.0f;
static int   s_sub_wave   = 0;
static int   s_sub_oct    = 1;   /* 0=unison 1=oct-1 2=oct-2 */
static float s_sub_coarse = 0.0f;
static float s_sub_fine   = 0.0f;
static bool  s_sub_sync   = false;
static bool  s_sub_ring   = false;

static float s_lfo_rate  = 0.0f;
static int   s_lfo_wave  = 0;
static float s_lfo_pitch_amt  = 0.0f;
static float s_lfo_pitch_range = 2.0f;
static float s_lfo_pwm_amt    = 0.0f;
static float s_lfo_pwm_range  = 0.20f;
static float s_lfo_filt_amt   = 0.0f;
static float s_lfo_filt_range = 1200.0f;
static bool  s_lfo_reset_on_key = true;
static float s_lfo_start_phase = 0.0f;

static int   s_fx_preset = 0;
static float s_master    = 0.80f;
static bool  s_sus_ped   = false;

/* Unified parameter travel — F1 toggles; PgUp/Dn = section, arrows = param/value */
#define TRV_SEC_ADSR   0
#define TRV_SEC_OSC    1
#define TRV_SEC_FILTER 2
#define TRV_SEC_SUB    3
#define TRV_SEC_LFO    4
#define TRV_SEC_FX     5
#define TRV_SEC_COUNT  6

static bool  s_travel_edit   = false;
static int   s_travel_section = 0;
static int   s_travel_param   = 0;
static int   s_fx_group       = 0; /* FX section: effect block (Drive..Master) */

/* ── UI drawing helpers ───────────────────────────────────────────────── */

static void draw_rect(SituationCommandBuffer cmd,
                      float x, float y, float w, float h, Vector4 col)
{
    mat4 m; glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m,    (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
}

static void txt(SituationCommandBuffer cmd, const char* s,
                float x, float y, float fs, ColorRGBA col)
{
    SituationCmdDrawTextEx(cmd, _sit_ex_font, s,
                           (Vector2){{x, y}}, fs, 0.5f, col);
}

static void panel(SituationCommandBuffer cmd, float x, float y, float w, float h)
{
    draw_rect(cmd, x, y, w, h, (Vector4){{0.10f, 0.11f, 0.16f, 0.95f}});
    draw_rect(cmd, x, y, w, 2.0f, (Vector4){{0.35f, 0.40f, 0.65f, 1.0f}});
}

#define MAIN_METER_SEGMENTS 18

static float meter_display_level(float linear)
{
    if (linear < 0.0f) linear = 0.0f;
    if (linear > 1.0f) linear = 1.0f;
    return sqrtf(linear);
}

static void draw_segment_vu(SituationCommandBuffer cmd,
                            float x, float y, float bar_w, float bar_h,
                            float level, int segments)
{
    const float seg_gap = 1.0f;
    float seg_h = (bar_h - seg_gap * (float)(segments - 1)) / (float)segments;
    if (seg_h < 2.0f) seg_h = 2.0f;

    float display = meter_display_level(level);
    for (int i = 0; i < segments; i++) {
        float thresh = (float)(i + 1) / (float)segments;
        bool on = display + 0.0001f >= thresh;
        float sy = y + bar_h - (float)(i + 1) * (seg_h + seg_gap) + seg_gap;
        Vector4 col = on
            ? (Vector4){{0.22f, 0.82f, 0.34f, 1.0f}}
            : (Vector4){{0.10f, 0.12f, 0.16f, 1.0f}};
        draw_rect(cmd, x, sy, bar_w, seg_h, col);
    }
}

static void read_main_meter(SituationAudioGraph* graph, SituationNodeHandle meter,
                            float* peak_l, float* peak_r)
{
    if (peak_l) *peak_l = 0.0f;
    if (peak_r) *peak_r = 0.0f;
    if (!graph || meter == SITUATION_INVALID_NODE_HANDLE)
        return;

    SituationNode* raw = SituationGetNode(graph, meter);
    if (!raw)
        return;

    const SitMeterNodeView* node = (const SitMeterNodeView*)raw;
    if (!node->device_data)
        return;

    const SituationPeakMeterState* st = (const SituationPeakMeterState*)node->device_data;
    if (peak_l) *peak_l = st->peak_l;
    if (peak_r) *peak_r = st->peak_r;
}

static void draw_main_meters(SituationCommandBuffer cmd,
                             float x, float y, float w, float h,
                             float peak_l, float peak_r)
{
    panel(cmd, x, y, w, h);

    const float label_w = 58.0f;
    const float bar_w   = 14.0f;
    const float bar_gap = 10.0f;
    const float top_pad = 16.0f;
    const float bar_h   = h - top_pad - 6.0f;
    float bars_x = x + label_w + (w - label_w - bar_w * 2.0f - bar_gap) * 0.5f;
    float bar_y  = y + top_pad;

    txt(cmd, "MAIN", x + 8.0f, y + 5.0f, 11.0f, (ColorRGBA){255, 200, 50, 255});
    txt(cmd, "OUT", x + 8.0f, y + 17.0f, 10.0f, (ColorRGBA){120, 125, 155, 200});

    draw_segment_vu(cmd, bars_x, bar_y, bar_w, bar_h, peak_l, MAIN_METER_SEGMENTS);
    draw_segment_vu(cmd, bars_x + bar_w + bar_gap, bar_y, bar_w, bar_h, peak_r, MAIN_METER_SEGMENTS);

    txt(cmd, "L", bars_x + 4.0f, y + h - 12.0f, 9.0f, (ColorRGBA){140, 200, 150, 220});
    txt(cmd, "R", bars_x + bar_w + bar_gap + 4.0f, y + h - 12.0f, 9.0f, (ColorRGBA){140, 200, 150, 220});
}

static void param_bar(SituationCommandBuffer cmd,
                      const char* label, float val, float lo, float hi,
                      float x, float y, float w, ColorRGBA bar_col)
{
    const float LABEL_W = (w < 300.0f) ? 54.0f : (w < 420.0f) ? 64.0f : 76.0f;
    const float BAR_H   = 11.0f;
    const float VAL_W   = 44.0f;
    const float bar_w   = w - LABEL_W - VAL_W - 8.0f;
    float t = (hi > lo) ? (val - lo) / (hi - lo) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    txt(cmd, label, x, y, 11.0f, (ColorRGBA){160, 165, 200, 220});
    float bx = x + LABEL_W;
    draw_rect(cmd, bx, y + 1.0f, bar_w, BAR_H - 2.0f,
              (Vector4){{0.08f, 0.09f, 0.14f, 1.0f}});
    if (t > 0.001f)
        draw_rect(cmd, bx, y + 1.0f, bar_w * t, BAR_H - 2.0f,
                  (Vector4){{bar_col.r/255.0f, bar_col.g/255.0f,
                              bar_col.b/255.0f, 0.85f}});
    char vbuf[16];
    if (hi - lo > 5.0f)
        snprintf(vbuf, sizeof vbuf, "%.0f", val);
    else if (hi - lo > 1.5f)
        snprintf(vbuf, sizeof vbuf, "%.1f", val);
    else
        snprintf(vbuf, sizeof vbuf, "%.2f", val);
    txt(cmd, vbuf, bx + bar_w + 4.0f, y, 11.0f, (ColorRGBA){200, 210, 240, 230});
}

static void toggle_pill(SituationCommandBuffer cmd,
                        const char* label, bool on,
                        float x, float y, float w)
{
    Vector4 bg = on ? (Vector4){{0.15f, 0.55f, 0.25f, 1.0f}}
                    : (Vector4){{0.18f, 0.18f, 0.22f, 1.0f}};
    draw_rect(cmd, x, y, w, 14.0f, bg);
    ColorRGBA fc = on ? (ColorRGBA){180, 255, 190, 255}
                      : (ColorRGBA){120, 120, 140, 200};
    txt(cmd, label, x + 4.0f, y + 2.0f, 10.0f, fc);
}

/** Row of small selectable buttons; returns y after the row. */
static float draw_btn_row(SituationCommandBuffer cmd,
                          float x, float y, float total_w,
                          const char* const* labels, int count, int selected,
                          Vector4 sel_bg, Vector4 off_bg,
                          ColorRGBA sel_fg, ColorRGBA off_fg)
{
    if (count <= 0) return y;
    const float gap = 3.0f;
    const float btn_h = 13.0f;
    const float btn_w = (total_w - gap * (float)(count - 1)) / (float)count;
    for (int i = 0; i < count; i++) {
        float bx = x + (float)i * (btn_w + gap);
        draw_rect(cmd, bx, y, btn_w, btn_h, (i == selected) ? sel_bg : off_bg);
        txt(cmd, labels[i], bx + 3.0f, y + 1.0f, 9.0f, (i == selected) ? sel_fg : off_fg);
    }
    return y + btn_h;
}

static void panel_hints(SituationCommandBuffer cmd,
                        float x, float y, float w, float panel_bottom,
                        ColorRGBA col, const char* line1, const char* line2)
{
    const float line_h = 11.0f;
    float hy = panel_bottom - line_h * (line2 ? 2.0f : 1.0f) - 6.0f;
    if (hy < y) hy = y;
    (void)w;
    txt(cmd, line1, x, hy, 9.5f, col);
    if (line2) {
        txt(cmd, line2, x, hy + line_h, 9.5f, col);
    }
}

/* ── Piano keyboard visual ────────────────────────────────────────────── */
static void draw_piano(SituationCommandBuffer cmd, float kx, float ky,
                       float total_w, float white_h)
{
    static const int IS_BLACK[12] = {0,1,0,1,0,0,1,0,1,0,1,0};

    const int NUM_SEMITONES = 60; /* C2..B6 — five octaves */
    const int FIRST_NOTE    = 36;
    int total_whites = 0;
    for (int i = 0; i < NUM_SEMITONES; i++)
        if (!IS_BLACK[i % 12]) total_whites++;

    float white_w = total_w / (float)total_whites;
    float black_w = white_w * 0.60f;
    float black_h = white_h * 0.62f;
    float gap     = 1.5f;

    /* Build note-held lookup from piano slot state */
    bool note_held[128] = {0};
    for (int i = 0; i < s_piano_key_count; i++)
        if (s_piano_held[i] && s_piano_note[i] >= 0 && s_piano_note[i] < 128)
            note_held[s_piano_note[i]] = true;

    /* Pass 1: white keys */
    int wi = 0;
    for (int i = 0; i < NUM_SEMITONES; i++) {
        int note = FIRST_NOTE + i;
        if (IS_BLACK[i % 12]) continue;
        float x = kx + (float)wi * white_w;
        bool held = note_held[note];
        Vector4 col = held
            ? (Vector4){{0.30f, 0.70f, 1.00f, 1.0f}}
            : (Vector4){{0.88f, 0.90f, 0.92f, 1.0f}};
        draw_rect(cmd, x + gap, ky, white_w - gap*2.0f, white_h, col);
        draw_rect(cmd, x + gap, ky + white_h - 2.0f,
                  white_w - gap*2.0f, 2.0f,
                  (Vector4){{0.2f, 0.2f, 0.2f, 1.0f}});
        if (i % 12 == 0) {
            char nn[8];
            snprintf(nn, sizeof nn, "C%d", note/12 - 1);
            txt(cmd, nn, x + gap + 2.0f, ky + white_h - 14.0f,
                9.0f, (ColorRGBA){60, 60, 80, 200});
        }
        wi++;
    }

    /* Pass 2: black keys */
    wi = 0;
    for (int i = 0; i < NUM_SEMITONES; i++) {
        int note = FIRST_NOTE + i;
        if (IS_BLACK[i % 12]) {
            float x = kx + ((float)wi - 0.35f) * white_w;
            bool held = note_held[note];
            Vector4 col = held
                ? (Vector4){{0.10f, 0.45f, 0.90f, 1.0f}}
                : (Vector4){{0.15f, 0.15f, 0.18f, 1.0f}};
            draw_rect(cmd, x, ky, black_w, black_h, col);
        } else {
            wi++;
        }
    }
}

static void draw_travel_panel(SituationCommandBuffer cmd, SituationAudioGraph* graph,
                              const Nodes* nodes, int section,
                              float x, float y0, float quad_w, float quad_h,
                              float inner_w, float pad);

/* ── Main HUD — 3×2 control panels below the piano ────────────────────── */
static void draw_ui(SituationCommandBuffer cmd, int sw, int sh,
                    SituationAudioGraph* graph, const Nodes* nodes)
{
    const float PAD    = 8.0f;
    const float GAP    = 6.0f;
    const float FS     = 11.0f;
    const float FS_H   = 12.0f;
    const float ROW_H  = 15.0f;
    const float BOT_BAR = 24.0f; /* sit_example bottom HUD */

    ColorRGBA C_HEAD = {255, 200,  50, 255};
    ColorRGBA C_DIM  = {120, 125, 155, 200};
    ColorRGBA C_KEY  = {180, 185, 220, 200};
    ColorRGBA C_LBL  = {160, 165, 200, 220};

    Vector4 sel_blue  = {{0.25f, 0.50f, 0.90f, 1.0f}};
    Vector4 off_btn   = {{0.12f, 0.13f, 0.18f, 1.0f}};
    Vector4 sel_orange = {{0.55f, 0.25f, 0.10f, 1.0f}};
    Vector4 sel_green = {{0.20f, 0.45f, 0.20f, 1.0f}};
    Vector4 sel_purple = {{0.50f, 0.20f, 0.45f, 1.0f}};

    /* ── Piano ─────────────────────────────────────────────────────────── */
    float piano_top = 26.0f;
    float piano_h   = 128.0f;
    float piano_w   = (float)sw - PAD * 2.0f;

    draw_rect(cmd, PAD, piano_top, piano_w, piano_h,
              (Vector4){{0.06f, 0.07f, 0.10f, 1.0f}});
    draw_piano(cmd, PAD + 4.0f, piano_top + 8.0f, piano_w - 8.0f, piano_h - 22.0f);

    {
        char buf[64];
        snprintf(buf, sizeof buf, "oct %+d  F2/F3", s_octave);
        txt(cmd, buf, PAD + 6.0f, piano_top + piano_h - 13.0f, 10.0f, C_DIM);

        const char* sus_str = s_sus_ped ? "[ SUSTAIN ON ]" : "SPACE = sustain";
        ColorRGBA sc = s_sus_ped ? (ColorRGBA){80, 220, 120, 255} : C_DIM;
        txt(cmd, sus_str, (float)sw * 0.5f - 50.0f, piano_top + piano_h - 13.0f, 10.0f, sc);
    }

    /* ── Stereo main output meters (post-gain, pre-mixer) ─────────────── */
    float meter_h   = 44.0f;
    float meter_top = piano_top + piano_h + 4.0f;
    float meter_w   = 108.0f;
    float meter_x   = (float)sw - PAD - meter_w;
    float peak_l = 0.0f, peak_r = 0.0f;
    read_main_meter(graph, nodes->peak_meter, &peak_l, &peak_r);
    draw_main_meters(cmd, meter_x, meter_top, meter_w, meter_h, peak_l, peak_r);

    /* ── 3×2 panels ───────────────────────────────────────────────────── */
    float grid_top = meter_top + meter_h + GAP;
    float grid_bot = (float)sh - BOT_BAR - GAP;
    float grid_h   = grid_bot - grid_top;
    float quad_w   = ((float)sw - PAD * 2.0f - GAP * 2.0f) / 3.0f;
    float quad_h   = (grid_h - GAP) * 0.5f;
    float inner_w  = quad_w - PAD * 2.0f;

    float qx[3] = {
        PAD,
        PAD + quad_w + GAP,
        PAD + (quad_w + GAP) * 2.0f
    };
    float qy[2] = { grid_top, grid_top + quad_h + GAP };

    /* P1 — ADSR (top-left) */
    {
        float x = qx[0], y0 = qy[0];
        panel(cmd, x, y0, quad_w, quad_h);
        if (s_travel_edit) {
            draw_travel_panel(cmd, graph, nodes, TRV_SEC_ADSR, x, y0, quad_w, quad_h, inner_w, PAD);
        } else {
            txt(cmd, "ADSR", x + PAD, y0 + 5.0f, FS_H, C_HEAD);
            float y = y0 + 22.0f;
            param_bar(cmd, "Attack",  s_attack,  0.001f, 2.0f, x + PAD, y, inner_w, (ColorRGBA){80, 180, 255, 255}); y += ROW_H;
            param_bar(cmd, "Decay",   s_decay,   0.001f, 2.0f, x + PAD, y, inner_w, (ColorRGBA){80, 200, 200, 255}); y += ROW_H;
            param_bar(cmd, "Sustain", s_sustain, 0.0f,   1.0f, x + PAD, y, inner_w, (ColorRGBA){100, 220, 130, 255}); y += ROW_H;
            param_bar(cmd, "Release", s_release, 0.01f,  5.0f, x + PAD, y, inner_w, (ColorRGBA){180, 140, 255, 255});
            panel_hints(cmd, x + PAD, y, inner_w, y0 + quad_h, C_KEY, "F1  parameter edit", NULL);
        }
    }

    /* P2 — Oscillator (top-center) */
    {
        float x = qx[1], y0 = qy[0];
        panel(cmd, x, y0, quad_w, quad_h);
        if (s_travel_edit) {
            draw_travel_panel(cmd, graph, nodes, TRV_SEC_OSC, x, y0, quad_w, quad_h, inner_w, PAD);
        } else {
            txt(cmd, "OSCILLATOR", x + PAD, y0 + 5.0f, FS_H, C_HEAD);
            float y = y0 + 22.0f;

            txt(cmd, "Waveform", x + PAD, y, FS, C_LBL);
            y += 11.0f;
            {
                static const char* wfn[] = {"Sine", "Pulse", "Tri", "Saw", "Noise"};
                y = draw_btn_row(cmd, x + PAD, y, inner_w, wfn, 5, s_waveform,
                                 sel_blue, off_btn,
                                 (ColorRGBA){220, 240, 255, 255}, (ColorRGBA){120, 125, 155, 200});
                y += 6.0f;
            }
            {
                ColorRGBA pwm_c = (s_waveform == 1)
                    ? (ColorRGBA){255, 200, 100, 255}
                    : (ColorRGBA){80, 82, 100, 180};
                param_bar(cmd, "Pulse wd", s_pwm, 0.05f, 0.95f, x + PAD, y, inner_w, pwm_c);
                y += ROW_H;
            }
            param_bar(cmd, "Pan", s_pan, -1.0f, 1.0f, x + PAD, y, inner_w,
                      (ColorRGBA){140, 180, 255, 255});
            y += ROW_H;
            toggle_pill(cmd, s_mono ? "Mono" : "Poly", true, x + PAD, y, inner_w * 0.30f);
            toggle_pill(cmd, "Glide", s_portamento, x + PAD + inner_w * 0.34f, y, inner_w * 0.30f);
            {
                char pbuf[20];
                snprintf(pbuf, sizeof pbuf, "P%02d", s_patch_slot);
                txt(cmd, pbuf, x + PAD + inner_w * 0.70f, y + 2.0f, 10.0f,
                    (ColorRGBA){200, 210, 240, 230});
            }
            panel_hints(cmd, x + PAD, y, inner_w, y0 + quad_h, C_KEY, "F1  parameter edit", NULL);
        }
    }

    /* P3 — Filter (top-right) */
    {
        float x = qx[2], y0 = qy[0];
        panel(cmd, x, y0, quad_w, quad_h);
        if (s_travel_edit) {
            draw_travel_panel(cmd, graph, nodes, TRV_SEC_FILTER, x, y0, quad_w, quad_h, inner_w, PAD);
        } else {
            txt(cmd, "FILTER", x + PAD, y0 + 5.0f, FS_H, C_HEAD);
            float y = y0 + 22.0f;

            txt(cmd, "Mode", x + PAD, y, FS, C_LBL);
            y += 11.0f;
            {
                static const char* fm[] = {
                    "Off", "LP", "HP", "BP", "Notch", "AP", "LP+BP", "LP+HP", "BP+HP"
                };
                int mi = s_filt_mode;
                if (mi < 0) mi = 0;
                if (mi > 8) mi = 8;
                char mbuf[32];
                snprintf(mbuf, sizeof mbuf, "%s", fm[mi]);
                txt(cmd, mbuf, x + PAD, y, FS, (ColorRGBA){255, 210, 150, 255});
                y += 14.0f;
            }

            txt(cmd, "Slope", x + PAD, y, FS, C_LBL);
            y += 11.0f;
            {
                static const char* slopes[] = {"6dB", "12dB", "18dB", "24dB"};
                y = draw_btn_row(cmd, x + PAD, y, inner_w, slopes, 4, s_filt_poles - 1,
                                 sel_orange, off_btn,
                                 (ColorRGBA){255, 210, 150, 255}, (ColorRGBA){120, 125, 155, 200});
                y += 5.0f;
            }

            param_bar(cmd, "Cutoff",  s_filt_cut, 40.f, 19000.f, x + PAD, y, inner_w, (ColorRGBA){255, 140, 80, 255}); y += ROW_H;
            param_bar(cmd, "Filter Q", s_filt_res, 0.5f, 20.f,    x + PAD, y, inner_w, (ColorRGBA){255, 80, 120, 255}); y += ROW_H;
            param_bar(cmd, "Env amt", s_filt_env, 0.0f, 1.0f,    x + PAD, y, inner_w, (ColorRGBA){255, 180, 60, 255});
            panel_hints(cmd, x + PAD, y, inner_w, y0 + quad_h, C_KEY, "F1  parameter edit", NULL);
        }
    }

    /* P4 — Sub-oscillator (bottom-left) */
    {
        float x = qx[0], y0 = qy[1];
        panel(cmd, x, y0, quad_w, quad_h);
        if (s_travel_edit) {
            draw_travel_panel(cmd, graph, nodes, TRV_SEC_SUB, x, y0, quad_w, quad_h, inner_w, PAD);
        } else {
            txt(cmd, "SUB-OSCILLATOR", x + PAD, y0 + 5.0f, FS_H, C_HEAD);
            float y = y0 + 22.0f;

            param_bar(cmd, "Level",  s_sub_level,  0.0f, 1.0f,   x + PAD, y, inner_w, (ColorRGBA){140, 200, 255, 255}); y += ROW_H;
            param_bar(cmd, "Coarse", s_sub_coarse, -12.f, 12.0f, x + PAD, y, inner_w, (ColorRGBA){180, 160, 255, 255}); y += ROW_H;
            param_bar(cmd, "Fine",   s_sub_fine,   -1.0f,  1.0f,  x + PAD, y, inner_w, (ColorRGBA){160, 200, 255, 255}); y += ROW_H;

            txt(cmd, "Octave", x + PAD, y, FS, C_LBL);
            y += 11.0f;
            {
                static const char* on[] = {"Unison", "Oct-1", "Oct-2"};
                y = draw_btn_row(cmd, x + PAD, y, inner_w, on, 3, s_sub_oct,
                                 sel_green, off_btn,
                                 (ColorRGBA){180, 255, 180, 255}, (ColorRGBA){120, 125, 155, 200});
                y += 5.0f;
            }

            txt(cmd, "Waveform", x + PAD, y, FS, C_LBL);
            y += 11.0f;
            {
                static const char* wfn[] = {"Sine", "Pulse", "Tri", "Saw", "Noise"};
                y = draw_btn_row(cmd, x + PAD, y, inner_w, wfn, 5, s_sub_wave,
                                 (Vector4){{0.35f, 0.35f, 0.55f, 1.0f}}, off_btn,
                                 (ColorRGBA){220, 220, 255, 255}, (ColorRGBA){120, 125, 155, 200});
                y += 6.0f;
            }

            toggle_pill(cmd, "Sync", s_sub_sync, x + PAD,                  y, inner_w * 0.48f);
            toggle_pill(cmd, "Ring", s_sub_ring, x + PAD + inner_w * 0.52f, y, inner_w * 0.48f);
            panel_hints(cmd, x + PAD, y, inner_w, y0 + quad_h, C_KEY, "F1  parameter edit", NULL);
        }
    }

    /* P5 — LFO (bottom-center) */
    {
        float x = qx[1], y0 = qy[1];
        panel(cmd, x, y0, quad_w, quad_h);
        if (s_travel_edit) {
            draw_travel_panel(cmd, graph, nodes, TRV_SEC_LFO, x, y0, quad_w, quad_h, inner_w, PAD);
        } else {
            txt(cmd, "LFO", x + PAD, y0 + 5.0f, FS_H, C_HEAD);
            float y = y0 + 22.0f;

            param_bar(cmd, "Rate", s_lfo_rate, 0.0f, 20.0f, x + PAD, y, inner_w, (ColorRGBA){255, 200, 80, 255}); y += ROW_H;

            txt(cmd, "Waveform", x + PAD, y, FS, C_LBL);
            y += 11.0f;
            {
                static const char* lwfn[] = {"Tri", "Square", "Random"};
                y = draw_btn_row(cmd, x + PAD, y, inner_w, lwfn, 3, s_lfo_wave,
                                 (Vector4){{0.45f, 0.35f, 0.10f, 1.0f}}, off_btn,
                                 (ColorRGBA){255, 220, 120, 255}, (ColorRGBA){120, 125, 155, 200});
                y += 8.0f;
            }

            param_bar(cmd, "Pitch amt", s_lfo_pitch_amt, 0.0f, 1.0f, x + PAD, y, inner_w,
                      (ColorRGBA){255, 180, 100, 255}); y += ROW_H;
            param_bar(cmd, "PWM amt",   s_lfo_pwm_amt,   0.0f, 1.0f, x + PAD, y, inner_w,
                      (ColorRGBA){255, 200, 120, 255}); y += ROW_H;
            param_bar(cmd, "Filt amt",  s_lfo_filt_amt,  0.0f, 1.0f, x + PAD, y, inner_w,
                      (ColorRGBA){200, 160, 255, 255}); y += ROW_H;
            toggle_pill(cmd, "Rst key", s_lfo_reset_on_key, x + PAD, y, inner_w * 0.48f);
            panel_hints(cmd, x + PAD, y, inner_w, y0 + quad_h, C_KEY, "F1  parameter edit", NULL);
        }
    }

    /* P6 — Effects + master (bottom-right) */
    {
        float x = qx[2], y0 = qy[1];
        panel(cmd, x, y0, quad_w, quad_h);
        if (s_travel_edit) {
            draw_travel_panel(cmd, graph, nodes, TRV_SEC_FX, x, y0, quad_w, quad_h, inner_w, PAD);
        } else {
            txt(cmd, "EFFECTS", x + PAD, y0 + 5.0f, FS_H, C_HEAD);
            float y = y0 + 22.0f;

            txt(cmd, "Preset  (`)", x + PAD, y, FS, C_LBL);
            y += 11.0f;
            {
                static const char* fx_a[] = {"Dry", "Echo", "Verb", "Chrs"};
                static const char* fx_b[] = {"Phas", "Drv", "All"};
                y = draw_btn_row(cmd, x + PAD, y, inner_w, fx_a, 4,
                                 (s_fx_preset < 4) ? s_fx_preset : -1,
                                 sel_purple, off_btn,
                                 (ColorRGBA){255, 180, 240, 255}, (ColorRGBA){120, 125, 155, 200});
                y += 4.0f;
                y = draw_btn_row(cmd, x + PAD, y, inner_w, fx_b, 3,
                                 (s_fx_preset >= 4) ? (s_fx_preset - 4) : -1,
                                 sel_purple, off_btn,
                                 (ColorRGBA){255, 180, 240, 255}, (ColorRGBA){120, 125, 155, 200});
                y += 8.0f;
            }

            param_bar(cmd, "Master", s_master, 0.1f, 2.0f, x + PAD, y, inner_w,
                      (ColorRGBA){200, 200, 200, 255});

            panel_hints(cmd, x + PAD, y, inner_w, y0 + quad_h, C_KEY,
                        "F1  parameter edit   `  preset",
                        "\\ / BACKSPACE  master gain");
        }
    }
}

/* ── Tone synth control indices ───────────────────────────────────────── */
/*
 * These match TONE_CTRL_* from sit/aud/tone_synth_graph.h (internal).
 * Defined locally so the example doesn't need to include implementation headers.
 * See tone_synth_graph.h for the authoritative list and documentation.
 */
#define TONE_CTRL_FREQUENCY         0
#define TONE_CTRL_WAVEFORM          1
#define TONE_CTRL_VOLUME            2
#define TONE_CTRL_PAN               3
#define TONE_CTRL_ATTACK            4
#define TONE_CTRL_DECAY             5
#define TONE_CTRL_SUSTAIN           6
#define TONE_CTRL_RELEASE           7
#define TONE_CTRL_HOLD              8
#define TONE_CTRL_FILTER_MODE       9
#define TONE_CTRL_FILTER_CUTOFF     10
#define TONE_CTRL_FILTER_RESONANCE  11
#define TONE_CTRL_FILTER_POLES      12
#define TONE_CTRL_FILTER_DRIVE      13
#define TONE_CTRL_FILTER_KEYTRACK   14
#define TONE_CTRL_FILTER_OVERSAMPLE 15
#define TONE_CTRL_PULSE_WIDTH       17
#define TONE_CTRL_VOICE_MODE        16
#define TONE_CTRL_PORTAMENTO_TIME   28
#define TONE_CTRL_PORTAMENTO_SPEED  29
#define TONE_CTRL_LFO_RATE          18
#define TONE_CTRL_LFO_WAVEFORM      19
#define TONE_CTRL_LFO_PITCH_AMOUNT  20
#define TONE_CTRL_LFO_PITCH_RANGE   21
#define TONE_CTRL_LFO_PWM_AMOUNT    22
#define TONE_CTRL_LFO_PWM_RANGE     23
#define TONE_CTRL_LFO_FILTER_AMOUNT 24
#define TONE_CTRL_LFO_FILTER_RANGE  25
#define TONE_CTRL_LFO_RESET_ON_KEY  39
#define TONE_CTRL_LFO_START_PHASE   40
#define TONE_CTRL_GLIDE_RESET_ON_KEY 41
#define TONE_CTRL_FILTER_ENV_AMOUNT 26
#define TONE_CTRL_FILTER_ENV_RANGE  27
#define TONE_CTRL_PATCH_SLOT        37
#define TONE_CTRL_PATCH_STORE       38
#define TONE_CTRL_SUB_LEVEL         30
#define TONE_CTRL_SUB_WAVEFORM      31
#define TONE_CTRL_SUB_OCTAVE        32
#define TONE_CTRL_SUB_FINE          33
#define TONE_CTRL_SUB_COARSE        34
#define TONE_CTRL_SUB_SYNC          35
#define TONE_CTRL_SUB_RING_MOD      36

/* FX control indices (must match sit/aud/registry_init.h + device_wrappers.h) */
#define OVERDRIVE_CTRL_MODE         0
#define OVERDRIVE_CTRL_DRIVE        1
#define OVERDRIVE_CTRL_MIX          4
#define OVERDRIVE_CTRL_TONE         5
#define CHORUS_CTRL_LFO_RATE        1   /* stage0_lfo_freq */
#define CHORUS_CTRL_LFO_DEPTH       2   /* stage0_lfo_depth_ms */
#define CHORUS_CTRL_DRY_GAIN        17
#define CHORUS_CTRL_WET_GAIN        18
#define CHORUS_CTRL_FEEDBACK        19
#define PHASER_CTRL_LFO             0
#define PHASER_CTRL_FEEDBACK        1
#define PHASER_CTRL_MIX             2
#define PHASER_CTRL_DEPTH           3
#define ECHO_CTRL_TIME              0
#define ECHO_CTRL_FEEDBACK          1
#define ECHO_CTRL_WET               2
#define REVERB_CTRL_ROOM            0
#define REVERB_CTRL_DAMP            1
#define REVERB_CTRL_WET             2
#define REVERB_CTRL_WIDTH           4

#define FX_GRP_DRIVE   0
#define FX_GRP_CHORUS  1
#define FX_GRP_PHASER  2
#define FX_GRP_ECHO    3
#define FX_GRP_REVERB  4
#define FX_GRP_MASTER  5
#define FX_GRP_COUNT   6

typedef struct {
    uint32_t    ctrl_id;
    const char* label;
    float       min_val;
    float       max_val;
    float       step;
    float       fine_step;
    bool        is_enum;
    int         enum_max;
    bool        log_scale;
} FxParamDef;

typedef struct {
    const char*         name;
    const FxParamDef*   params;
    int                 param_count;
} FxGroupDef;

static const FxParamDef k_fx_drive_params[] = {
    { OVERDRIVE_CTRL_MODE,  "Mode",  0.0f,   3.0f,    1.0f,  1.0f,  true,  3, false },
    { OVERDRIVE_CTRL_DRIVE, "Drive", 0.0f, 200.0f,    4.0f,  1.0f, false,  0, false },
    { OVERDRIVE_CTRL_MIX,   "Mix",   0.0f,   1.0f,  0.05f, 0.01f, false,  0, false },
    { OVERDRIVE_CTRL_TONE,  "Tone", 20.0f, 22050.f,  0.0f,  0.0f, false,  0, true  },
};

static const FxParamDef k_fx_chorus_params[] = {
    { CHORUS_CTRL_WET_GAIN,  "Wet",  0.0f,  2.0f, 0.05f, 0.01f, false, 0, false },
    { CHORUS_CTRL_DRY_GAIN,  "Dry",  0.0f,  2.0f, 0.05f, 0.01f, false, 0, false },
    { CHORUS_CTRL_LFO_RATE,  "Rate", 0.1f, 10.0f, 0.10f, 0.02f, false, 0, true  },
    { CHORUS_CTRL_LFO_DEPTH, "Depth",0.1f,  9.0f, 0.20f, 0.05f, false, 0, false },
    { CHORUS_CTRL_FEEDBACK,  "Fdbk", 0.0f, 0.99f, 0.03f, 0.01f, false, 0, false },
};

static const FxParamDef k_fx_phaser_params[] = {
    { PHASER_CTRL_MIX,      "Mix",   0.0f, 1.0f, 0.05f, 0.01f, false, 0, false },
    { PHASER_CTRL_LFO,      "Rate",  0.1f, 10.0f, 0.10f, 0.02f, false, 0, true  },
    { PHASER_CTRL_DEPTH,    "Depth", 0.0f, 1.0f, 0.05f, 0.01f, false, 0, false },
    { PHASER_CTRL_FEEDBACK, "Fdbk",  0.0f, 0.99f, 0.03f, 0.01f, false, 0, false },
};

static const FxParamDef k_fx_echo_params[] = {
    { ECHO_CTRL_TIME,     "Time", 0.01f, 2.0f, 0.03f, 0.01f, false, 0, false },
    { ECHO_CTRL_FEEDBACK, "Fdbk", 0.0f,  1.0f, 0.03f, 0.01f, false, 0, false },
    { ECHO_CTRL_WET,      "Wet",  0.0f,  1.0f, 0.03f, 0.01f, false, 0, false },
};

static const FxParamDef k_fx_reverb_params[] = {
    { REVERB_CTRL_ROOM,  "Tail",  0.0f, 1.0f, 0.04f, 0.01f, false, 0, false },
    { REVERB_CTRL_DAMP,  "Damp",  0.0f, 1.0f, 0.04f, 0.01f, false, 0, false },
    { REVERB_CTRL_WET,   "Wet",   0.0f, 1.0f, 0.04f, 0.01f, false, 0, false },
    { REVERB_CTRL_WIDTH, "Width", 0.0f, 1.0f, 0.04f, 0.01f, false, 0, false },
};

static const FxParamDef k_fx_master_params[] = {
    { 0, "Gain", 0.1f, 2.0f, 0.05f, 0.01f, false, 0, false },
};

static const FxGroupDef k_fx_groups[FX_GRP_COUNT] = {
    { "Drive",  k_fx_drive_params,  (int)(sizeof k_fx_drive_params  / sizeof k_fx_drive_params[0])  },
    { "Chorus", k_fx_chorus_params, (int)(sizeof k_fx_chorus_params / sizeof k_fx_chorus_params[0]) },
    { "Phaser", k_fx_phaser_params, (int)(sizeof k_fx_phaser_params / sizeof k_fx_phaser_params[0]) },
    { "Echo",   k_fx_echo_params,   (int)(sizeof k_fx_echo_params   / sizeof k_fx_echo_params[0])   },
    { "Verb",   k_fx_reverb_params, (int)(sizeof k_fx_reverb_params / sizeof k_fx_reverb_params[0]) },
    { "Master", k_fx_master_params, (int)(sizeof k_fx_master_params / sizeof k_fx_master_params[0]) },
};

static const char* k_overdrive_mode_labels[] = { "Soft", "Hard", "Tube", "Fold" };
static const char* k_waveform_labels[] = { "Sine", "Pulse", "Tri", "Saw", "Noise" };
static const char* k_voice_mode_labels[] = { "Poly", "Mono" };

static void push_params(SituationAudioGraph* g, const Nodes* n);

static void pull_tone_params(SituationAudioGraph* g, SituationNodeHandle tone)
{
    float v = 0.0f;
    if (SituationGetControl(g, tone, TONE_CTRL_FREQUENCY, &v) == SITUATION_SUCCESS)
        s_frequency = v;
    if (SituationGetControl(g, tone, TONE_CTRL_VOLUME, &v) == SITUATION_SUCCESS)
        s_volume = v;
    if (SituationGetControl(g, tone, TONE_CTRL_WAVEFORM, &v) == SITUATION_SUCCESS)
        s_waveform = (int)(v + 0.5f);
    if (SituationGetControl(g, tone, TONE_CTRL_PAN, &v) == SITUATION_SUCCESS)
        s_pan = v;
    if (SituationGetControl(g, tone, TONE_CTRL_ATTACK, &v) == SITUATION_SUCCESS)
        s_attack = v;
    if (SituationGetControl(g, tone, TONE_CTRL_DECAY, &v) == SITUATION_SUCCESS)
        s_decay = v;
    if (SituationGetControl(g, tone, TONE_CTRL_SUSTAIN, &v) == SITUATION_SUCCESS)
        s_sustain = v;
    if (SituationGetControl(g, tone, TONE_CTRL_RELEASE, &v) == SITUATION_SUCCESS)
        s_release = v;
    if (SituationGetControl(g, tone, TONE_CTRL_HOLD, &v) == SITUATION_SUCCESS)
        s_hold = v;
    if (SituationGetControl(g, tone, TONE_CTRL_PULSE_WIDTH, &v) == SITUATION_SUCCESS)
        s_pwm = v;
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_MODE, &v) == SITUATION_SUCCESS)
        s_filt_mode = (int)(v + 0.5f);
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_CUTOFF, &v) == SITUATION_SUCCESS)
        s_filt_cut = v;
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_RESONANCE, &v) == SITUATION_SUCCESS)
        s_filt_res = v;
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_POLES, &v) == SITUATION_SUCCESS)
        s_filt_poles = (int)(v + 0.5f);
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_ENV_AMOUNT, &v) == SITUATION_SUCCESS)
        s_filt_env = v;
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_ENV_RANGE, &v) == SITUATION_SUCCESS)
        s_filt_env_range = v;
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_DRIVE, &v) == SITUATION_SUCCESS)
        s_filt_drive = v;
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_KEYTRACK, &v) == SITUATION_SUCCESS)
        s_filt_keytrack = v;
    if (SituationGetControl(g, tone, TONE_CTRL_FILTER_OVERSAMPLE, &v) == SITUATION_SUCCESS)
        s_filt_os = (v >= 0.5f) ? 1 : 0;
    if (SituationGetControl(g, tone, TONE_CTRL_VOICE_MODE, &v) == SITUATION_SUCCESS)
        s_mono = v >= 0.5f;
    if (SituationGetControl(g, tone, TONE_CTRL_PORTAMENTO_TIME, &v) == SITUATION_SUCCESS) {
        s_portamento_time = v;
        s_portamento = v > 0.001f;
    }
    if (SituationGetControl(g, tone, TONE_CTRL_PORTAMENTO_SPEED, &v) == SITUATION_SUCCESS)
        s_portamento_speed = v;
    if (SituationGetControl(g, tone, TONE_CTRL_GLIDE_RESET_ON_KEY, &v) == SITUATION_SUCCESS)
        s_glide_reset_on_key = v >= 0.5f;
    if (SituationGetControl(g, tone, TONE_CTRL_SUB_LEVEL, &v) == SITUATION_SUCCESS)
        s_sub_level = v;
    if (SituationGetControl(g, tone, TONE_CTRL_SUB_WAVEFORM, &v) == SITUATION_SUCCESS)
        s_sub_wave = (int)(v + 0.5f);
    if (SituationGetControl(g, tone, TONE_CTRL_SUB_OCTAVE, &v) == SITUATION_SUCCESS)
        s_sub_oct = (int)(v + 0.5f);
    if (SituationGetControl(g, tone, TONE_CTRL_SUB_COARSE, &v) == SITUATION_SUCCESS)
        s_sub_coarse = v;
    if (SituationGetControl(g, tone, TONE_CTRL_SUB_FINE, &v) == SITUATION_SUCCESS)
        s_sub_fine = v;
    if (SituationGetControl(g, tone, TONE_CTRL_SUB_SYNC, &v) == SITUATION_SUCCESS)
        s_sub_sync = v >= 0.5f;
    if (SituationGetControl(g, tone, TONE_CTRL_SUB_RING_MOD, &v) == SITUATION_SUCCESS)
        s_sub_ring = v >= 0.5f;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_RATE, &v) == SITUATION_SUCCESS)
        s_lfo_rate = v;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_WAVEFORM, &v) == SITUATION_SUCCESS)
        s_lfo_wave = (int)(v + 0.5f);
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_PITCH_AMOUNT, &v) == SITUATION_SUCCESS)
        s_lfo_pitch_amt = v;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_PITCH_RANGE, &v) == SITUATION_SUCCESS)
        s_lfo_pitch_range = v;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_PWM_AMOUNT, &v) == SITUATION_SUCCESS)
        s_lfo_pwm_amt = v;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_PWM_RANGE, &v) == SITUATION_SUCCESS)
        s_lfo_pwm_range = v;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_FILTER_AMOUNT, &v) == SITUATION_SUCCESS)
        s_lfo_filt_amt = v;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_FILTER_RANGE, &v) == SITUATION_SUCCESS)
        s_lfo_filt_range = v;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_RESET_ON_KEY, &v) == SITUATION_SUCCESS)
        s_lfo_reset_on_key = v >= 0.5f;
    if (SituationGetControl(g, tone, TONE_CTRL_LFO_START_PHASE, &v) == SITUATION_SUCCESS)
        s_lfo_start_phase = v;
    if (SituationGetControl(g, tone, TONE_CTRL_PATCH_SLOT, &v) == SITUATION_SUCCESS)
        s_patch_slot = (int)(v + 0.5f);
}

static SituationNodeHandle fx_group_node(const Nodes* n, int group)
{
    switch (group) {
        case FX_GRP_DRIVE:  return n->overdrive;
        case FX_GRP_CHORUS: return n->chorus;
        case FX_GRP_PHASER: return n->phaser;
        case FX_GRP_ECHO:   return n->echo;
        case FX_GRP_REVERB: return n->reverb;
        case FX_GRP_MASTER: return n->gain;
        default:            return SITUATION_INVALID_NODE_HANDLE;
    }
}

static bool fx_read_value(SituationAudioGraph* graph, const Nodes* nodes,
                          int group, const FxParamDef* p, float* out)
{
    if (group == FX_GRP_MASTER && p->ctrl_id == 0) {
        *out = s_master;
        return true;
    }
    SituationNodeHandle h = fx_group_node(nodes, group);
    if (h == SITUATION_INVALID_NODE_HANDLE)
        return false;
    return SituationGetControl(graph, h, p->ctrl_id, out) == SITUATION_SUCCESS;
}

static void fx_write_value(SituationAudioGraph* graph, const Nodes* nodes,
                           int group, const FxParamDef* p, float v)
{
    v = clampf(v, p->min_val, p->max_val);
    if (group == FX_GRP_MASTER && p->ctrl_id == 0) {
        s_master = v;
        SituationSetControl(graph, nodes->gain, 0, v);
        return;
    }
    SituationNodeHandle h = fx_group_node(nodes, group);
    if (h != SITUATION_INVALID_NODE_HANDLE)
        SituationSetControl(graph, h, p->ctrl_id, v);
}

#include "travel_edit.inc"

/* ── Graph construction ───────────────────────────────────────────────── */
static SituationAudioGraph* build_graph(Nodes* n)
{
    SituationAudioGraph* g = SituationCreateGraph();
    if (!g) return NULL;

    if (SituationCreateNode(g, SITUATION_NODE_TONE_SYNTH, &n->tone)      != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_OVERDRIVE,  &n->overdrive) != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_CHORUS,     &n->chorus)    != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_PHASER,     &n->phaser)    != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_ECHO,       &n->echo)      != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_REVERB,     &n->reverb)    != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_GAIN,       &n->gain)       != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_PEAK_METER, &n->peak_meter) != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_MIXER,      &n->mixer)      != SITUATION_SUCCESS) {
        SituationDestroyGraph(g); return NULL;
    }

    /* Signal chain: tone -> overdrive -> chorus -> phaser -> echo -> reverb -> gain -> meter -> mixer */
    if (SituationCreatePatch(g, n->tone,      0, n->overdrive, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->overdrive, 0, n->chorus,    0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->chorus,    0, n->phaser,    0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->phaser,    0, n->echo,      0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->echo,      0, n->reverb,    0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->reverb,    0, n->gain,      0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->gain,      0, n->peak_meter, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->peak_meter, 0, n->mixer,     0, false) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g); return NULL;
    }

    if (SituationTopologicalSort(g) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g); return NULL;
    }

    /* Manual-mode amplitude (ctrl 2); keep 0 — notes use virtual MIDI velocity only. */
    SituationSetControl(g, n->tone, TONE_CTRL_VOLUME,  0.0f);
    SituationSetControl(g, n->tone, TONE_CTRL_HOLD,   -1.0f); /* MIDI NoteOff drives release */
    SituationSetControl(g, n->tone, TONE_CTRL_SUB_OCTAVE, (float)s_sub_oct);

    /* Effects -- all bypassed initially (dry path must stay at 1.0) */
    SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_DRIVE, 40.0f);
    SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_MIX,   0.0f);
    SituationSetControl(g, n->chorus,    CHORUS_CTRL_DRY_GAIN, 1.0f);
    SituationSetControl(g, n->chorus,    CHORUS_CTRL_WET_GAIN, 0.0f);
    SituationSetControl(g, n->phaser,    PHASER_CTRL_MIX,      0.0f);
    SituationSetControl(g, n->echo,      0,  0.30f); /* time */
    SituationSetControl(g, n->echo,      1,  0.35f); /* feedback */
    SituationSetControl(g, n->echo,      2,  0.0f);  /* wet=0 */
    SituationSetControl(g, n->reverb,    0,  0.65f); /* room */
    SituationSetControl(g, n->reverb,    1,  0.45f); /* damp */
    SituationSetControl(g, n->reverb,    2,  0.0f);  /* wet=0 */
    SituationSetControl(g, n->reverb,    3,  1.0f);  /* dry */
    SituationSetControl(g, n->reverb,    4,  0.80f); /* width */
    SituationSetControl(g, n->gain,      0,  s_master);
    SituationSetControl(g, n->mixer,     0,  1.0f);

    return g;
}

/* ── Push live synth state to graph controls ──────────────────────────── */
static void push_params(SituationAudioGraph* g, const Nodes* n)
{
    SituationSetControl(g, n->tone, TONE_CTRL_FREQUENCY,       s_frequency);
    SituationSetControl(g, n->tone, TONE_CTRL_VOLUME,          s_volume);
    SituationSetControl(g, n->tone, TONE_CTRL_HOLD,            s_hold);
    SituationSetControl(g, n->tone, TONE_CTRL_WAVEFORM,         (float)s_waveform);
    SituationSetControl(g, n->tone, TONE_CTRL_ATTACK,           s_attack);
    SituationSetControl(g, n->tone, TONE_CTRL_DECAY,            s_decay);
    SituationSetControl(g, n->tone, TONE_CTRL_SUSTAIN,          s_sustain);
    SituationSetControl(g, n->tone, TONE_CTRL_RELEASE,          s_release);
    SituationSetControl(g, n->tone, TONE_CTRL_PULSE_WIDTH,      s_pwm);
    SituationSetControl(g, n->tone, TONE_CTRL_PAN,              s_pan);
    SituationSetControl(g, n->tone, TONE_CTRL_VOICE_MODE,       s_mono ? 1.0f : 0.0f);
    SituationSetControl(g, n->tone, TONE_CTRL_PORTAMENTO_TIME,
                        s_portamento ? s_portamento_time : 0.0f);
    SituationSetControl(g, n->tone, TONE_CTRL_PORTAMENTO_SPEED,
                        s_portamento ? s_portamento_speed : 0.0f);
    SituationSetControl(g, n->tone, TONE_CTRL_GLIDE_RESET_ON_KEY,
                        s_glide_reset_on_key ? 1.0f : 0.0f);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_MODE,      (float)s_filt_mode);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_CUTOFF,    s_filt_cut);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_RESONANCE, s_filt_res);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_POLES,     (float)s_filt_poles);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_DRIVE,     s_filt_drive);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_KEYTRACK,  s_filt_keytrack);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_OVERSAMPLE, (float)s_filt_os);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_ENV_AMOUNT,s_filt_env);
    SituationSetControl(g, n->tone, TONE_CTRL_FILTER_ENV_RANGE, s_filt_env_range);
    SituationSetControl(g, n->tone, TONE_CTRL_SUB_LEVEL,        s_sub_level);
    SituationSetControl(g, n->tone, TONE_CTRL_SUB_WAVEFORM,     (float)s_sub_wave);
    SituationSetControl(g, n->tone, TONE_CTRL_SUB_OCTAVE,       (float)s_sub_oct);
    SituationSetControl(g, n->tone, TONE_CTRL_SUB_COARSE,       s_sub_coarse);
    SituationSetControl(g, n->tone, TONE_CTRL_SUB_FINE,        s_sub_fine);
    SituationSetControl(g, n->tone, TONE_CTRL_SUB_SYNC,         s_sub_sync ? 1.0f : 0.0f);
    SituationSetControl(g, n->tone, TONE_CTRL_SUB_RING_MOD,     s_sub_ring ? 1.0f : 0.0f);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_RATE,         s_lfo_rate);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_WAVEFORM,     (float)s_lfo_wave);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_PITCH_AMOUNT, s_lfo_pitch_amt);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_PITCH_RANGE,  s_lfo_pitch_range);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_PWM_AMOUNT,   s_lfo_pwm_amt);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_PWM_RANGE,    s_lfo_pwm_range);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_FILTER_AMOUNT,s_lfo_filt_amt);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_FILTER_RANGE, s_lfo_filt_range);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_RESET_ON_KEY, s_lfo_reset_on_key ? 1.0f : 0.0f);
    SituationSetControl(g, n->tone, TONE_CTRL_LFO_START_PHASE, s_lfo_start_phase);
    SituationSetControl(g, n->gain, 0, s_master);
}

static void push_fx_defaults(SituationAudioGraph* g, const Nodes* n)
{
    SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_DRIVE, 40.0f);
    SituationSetControl(g, n->echo,      0, 0.30f);
    SituationSetControl(g, n->echo,      1, 0.35f);
    /* reverb tail/damp/width: leave user-tweakable — only set on build_graph */
    SituationSetControl(g, n->mixer,     0, 1.0f);
}

/* ── Set FX preset wet levels ─────────────────────────────────────────── */
static void push_fx(SituationAudioGraph* g, const Nodes* n, int preset)
{
    SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_MIX, 0.0f);
    SituationSetControl(g, n->chorus,    CHORUS_CTRL_DRY_GAIN, 1.0f);
    SituationSetControl(g, n->chorus,    CHORUS_CTRL_WET_GAIN, 0.0f);
    SituationSetControl(g, n->phaser,    PHASER_CTRL_MIX,      0.0f);
    SituationSetControl(g, n->echo,      2, 0.0f);
    SituationSetControl(g, n->reverb,    2, 0.0f);
    switch (preset) {
        case 1: SituationSetControl(g, n->echo,      2, 0.45f); break;
        case 2: SituationSetControl(g, n->reverb,    2, 0.50f); break;
        case 3:
            SituationSetControl(g, n->chorus, CHORUS_CTRL_WET_GAIN, 0.80f);
            break;
        case 4: SituationSetControl(g, n->phaser, PHASER_CTRL_MIX, 0.60f); break;
        case 5: SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_MIX, 0.85f); break;
        case 6:
            SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_MIX, 0.35f);
            SituationSetControl(g, n->chorus,    CHORUS_CTRL_WET_GAIN, 0.45f);
            SituationSetControl(g, n->phaser,    PHASER_CTRL_MIX,      0.30f);
            SituationSetControl(g, n->echo,      2, 0.28f);
            SituationSetControl(g, n->reverb,    2, 0.42f);
            break;
        default: break;
    }
}

static void apply_graph_state(SituationAudioGraph* g, const Nodes* n)
{
    push_params(g, n);
    push_fx_defaults(g, n);
    push_fx(g, n, s_fx_preset);
}

/* ── Note helpers: virtual MIDI -> tone synth node ────────────────────── */
/*
 * The SITUATION_NODE_TONE_SYNTH is a 16-voice polyphonic engine inside the graph.
 * Notes must be triggered via the virtual MIDI loopback -- not SituationPlayToneEx.
 * SituationPlayToneEx routes through the legacy tone pool, which is a separate path
 * that bypasses the graph entirely.  The virtual MIDI path is the correct one:
 *
 *   SituationSetupVirtualMidiLoopback()  -- creates a virtual out->in pair
 *   SituationEnableMidiControl()         -- connects the in device to the synth node
 *   SituationVirtualMidiNoteOn/Off()     -- injects note events
 *
 * The tone synth handles its own voice allocation, ADSR, filter, and LFO per
 * note.  SituationSetControl() changes parameters shared across all voices.
 */
static void note_on(int slot, int midi_note)
{
    if (slot < 0 || slot >= s_piano_key_count) return;
    if (s_piano_held[slot]) return;
    s_piano_held[slot] = true;
    s_piano_note[slot] = midi_note;
    SituationVirtualMidiNoteOn((uint8_t)midi_note, 100);
}

static void note_off(int slot)
{
    if (slot < 0 || slot >= s_piano_key_count) return;
    if (!s_piano_held[slot]) return;
    int note = s_piano_note[slot];
    s_piano_held[slot] = false;
    s_piano_note[slot] = -1;
    if (note >= 0 && note <= 127)
        SituationVirtualMidiNoteOff((uint8_t)note);
}

/* Keys held before the first frame must not trigger notes until released. */
static void arm_keyboard_input(void)
{
    for (int i = 0; i < s_piano_key_count; i++) {
        if (SituationIsScancodeDown(s_piano_keys[i].scancode)) {
            s_piano_held[i] = true;
            s_piano_note[i] = -1;
        }
    }
    if (SituationIsKeyDown(SIT_KEY_SPACE))
        s_sus_ped = true;
}

/* Same repeat rate as travel_edit.inc TRV_ADJ_RATE_HZ */
#define MASTER_ADJ_RATE_HZ 10.0f

static int   s_master_adj_dir = 0;
static float s_master_adj_phase = 0.0f;

static void scan_master_gain_paced(SituationAudioGraph* graph, const Nodes* nodes)
{
    int dir = 0;
    if (SituationIsKeyDown(SIT_KEY_BACKSLASH)) dir = +1;
    if (SituationIsKeyDown(SIT_KEY_BACKSPACE))  dir = -1;

    if (dir == 0) {
        s_master_adj_dir = 0;
        s_master_adj_phase = 0.0f;
        return;
    }

    if (dir != s_master_adj_dir) {
        s_master_adj_dir = dir;
        s_master_adj_phase = 0.0f;
        s_master = clampf(s_master + (float)dir * 0.05f, 0.1f, 2.0f);
        SituationSetControl(graph, nodes->gain, 0, s_master);
        return;
    }

    float dt = SituationGetFrameTime();
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.25f) dt = 0.25f;
    s_master_adj_phase += dt;

    const float period = 1.0f / MASTER_ADJ_RATE_HZ;
    int steps = 0;
    while (s_master_adj_phase >= period && steps < 4) {
        s_master = clampf(s_master + (float)dir * 0.05f, 0.1f, 2.0f);
        SituationSetControl(graph, nodes->gain, 0, s_master);
        s_master_adj_phase -= period;
        steps++;
    }
}

/* Synth/options input — travel edit + global hotkeys (not piano). */
static void scan_synth_options(SituationAudioGraph* graph, const Nodes* nodes)
{
    scan_travel_edit(graph, nodes);

    /* ── Sustain pedal (SPACE) -- CC64 ─────────────────────────── */
    {
        bool space = SituationIsKeyDown(SIT_KEY_SPACE);
        if (space && !s_sus_ped) {
            s_sus_ped = true;
            SituationVirtualMidiControlChange(0, 64, 127);
        } else if (!space && s_sus_ped) {
            s_sus_ped = false;
            SituationVirtualMidiControlChange(0, 64, 0);
        }
    }

    if (SituationIsKeyPressed(SIT_KEY_F2)) { if (s_octave > -4) s_octave -= 1; }
    if (SituationIsKeyPressed(SIT_KEY_F3)) { if (s_octave <  4) s_octave += 1; }

    if (SituationIsKeyPressed(SIT_KEY_GRAVE_ACCENT)) {
        s_fx_preset = (s_fx_preset + 1) % 7;
        push_fx(graph, nodes, s_fx_preset);
    }

    scan_master_gain_paced(graph, nodes);
}

/* Piano input — positional scancodes only (see init_piano_scancode_maps). */
static void scan_piano_keys(void)
{
    for (int i = 0; i < s_piano_key_count; i++) {
        int note = s_piano_keys[i].midi_note + s_octave * 12;
        note = (note < 0) ? 0 : (note > 127 ? 127 : note);
        bool down = SituationIsScancodeDown(s_piano_keys[i].scancode)
                 && SituationIsKeyDown(s_piano_keys[i].logical_key);
        if (down && !s_piano_held[i])
            note_on(i, note);
        if (!down && s_piano_held[i])
            note_off(i);
    }
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "04 -- Play a Sound") != SITUATION_SUCCESS)
        return -1;

    init_piano_scancode_maps();

    /* The audio device starts paused -- must be explicitly resumed before any
     * sound will be heard.  Do this before building the graph. */
    SituationResumeAudioDevice();

    SitExample_InitAudioRegistry();

    /* Build the audio node graph */
    Nodes nodes = {
        SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE,
        SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE,
        SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE,
        SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE,
        SITUATION_INVALID_NODE_HANDLE
    };
    SituationAudioGraph* graph = build_graph(&nodes);
    if (!graph) {
        fprintf(stderr, "[04] Failed to build audio graph\n");
        SitExample_Shutdown();
        return -1;
    }

    (void)SitExample_WireToneSynthVirtualMidi(graph, nodes.tone, "04", NULL);

    if (SitExample_ActivateAudioGraph(graph) != SITUATION_SUCCESS) {
        SituationDestroyGraph(graph);
        SitExample_Shutdown();
        return -1;
    }

    /* Push initial synth state (overrides node defaults after activation) */
    apply_graph_state(graph, &nodes);
    SituationVirtualMidiControlChange(0, 64, 0);  /* sustain off */
    SituationVirtualMidiControlChange(0, 123, 0); /* all notes off */

    memset(s_piano_held, 0,  sizeof s_piano_held);
    memset(s_piano_note, -1, sizeof s_piano_note);

    int input_armed = 0;

    /* ── Main loop ────────────────────────────────────────────────────── */
    while (!SituationWindowShouldClose())
    {
        if (SitExample_BeginFrame()) { break; }

        if (!input_armed) {
            arm_keyboard_input();
            input_armed = 1;
        }

        int sw = SituationGetRenderWidth();
        int sh = SituationGetRenderHeight();

        scan_synth_options(graph, &nodes);
        scan_piano_keys();

        /* ── Render ──────────────────────────────────────────────────── */
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) { continue; }

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationRenderPassInfo pass = SituationRenderPassInfoDefault(-1, (ColorRGBA){14, 15, 20, 255});
        SituationCmdBeginRenderPass(cmd, &pass);

        draw_ui(cmd, sw, sh, graph, &nodes);

        SitExample_DrawHUD(cmd, "04 -- Play a Sound",
            "Piano: Z/Q rows  SPACE:sus  F1:edit  PgUp/Dn:section  arrows:travel  `:preset");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    /* ── Cleanup ─────────────────────────────────────────────────────── */
    SituationVirtualMidiControlChange(0, 123, 0); /* all-notes-off */
    SitExample_DestroyAudioGraph(&graph, nodes.tone);
    SitExample_TeardownVirtualMidi();
    SitExample_Shutdown();
    return 0;
}

