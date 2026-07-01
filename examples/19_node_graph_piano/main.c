/***************************************************************************************************
 *  Situation — 19: Node Graph Piano (Full Synth)
 *  -------------------------------------------------------
 *  Signal chain: Tone Synth → Overdrive → Chorus → Phaser → Echo → Reverb → Gain → Mixer
 *
 *  Promoted from examples/other/node_graph_piano_demo.c
 *
 *  Build:
 *    build\build_examples.bat static-opengl  19_node_graph_piano
 *    build\build_examples.bat static-vulkan  19_node_graph_piano
 *
 *  Piano keys (hold = sustain, release = note-off):
 *    White keys (C D E F G A B C D E F G A B C D E F G A B):
 *      Lower: Z X C V B N M , . /   (C3–E4 whites)
 *      Upper: Q W E R T Y U I O P   (C4–A5 whites, black-key rows interleaved)
 *      Top:   1 2 3 4 5 6 7 8 9 0   (C5-ish to B5 continuation)
 *    Black keys:
 *      A-row (sharps of lower): A S D F G H J K L ;
 *      Number-row (sharps of upper): — used for octave shift on the Q-row
 *    Black keys for lower octave: A(C#3) S(D#3) F(F#3) G(G#3) H(A#3)
 *    Black keys for upper octave: T(F#4) Y(G#4) U(A#4) I(C#5) O(D#5) P(F#5)
 *
 *  ── Controls ────────────────────────────────────────────────────────────
 *  Octave           F2/F3         Down/Up (clamped ±2)
 *  Sustain pedal    SPACE         Hold notes after key release
 *
 *  ADSR (hold to repeat-step):
 *    Attack          LEFT/RIGHT    ±0.01 s   (0.001–2 s)
 *    Decay           UP/DOWN       ±0.01 s   (0.001–2 s)
 *    Sustain level   ;/'           ±0.05     (0–1)
 *    Release         +/-           ±0.05 s   (0.01–5 s)
 *
 *  Waveform         TAB           Cycle: Sine→Pulse→Tri→Saw→Noise
 *  PWM (Pulse only) ,/.           ±0.02 duty (0.05–0.95)
 *  Filter mode      F4            Cycle: Off→LP→HP→BP→Notch→Allpass
 *  Filter cutoff    F5/F6         ×0.9 / ×1.1  (logarithmic feel)
 *  Filter resonance F7/F8         ±0.5 Q  (0.5–20)
 *  Filter env amt   [/]           ±0.05   (0–1)
 *
 *  Sub-oscillator:
 *    Sub level       KP_4/KP_6    ±0.05   (0–1)
 *    Sub waveform    KP_7         Cycle (Sine→Pulse→Tri→Saw→Noise)
 *    Sub octave      KP_8/KP_9   0=Unison / 1=Oct-1 / 2=Oct-2
 *    Sync on/off     KP_5        Toggle hard-sync (sub locked to main)
 *    Ring mod on/off KP_0        Toggle ring modulation
 *    Sub coarse      KP_+/KP_-   ±1 semitone  (±12 st)
 *
 *  LFO:
 *    LFO rate        KP_1/KP_2   ±0.2 Hz  (0–20 Hz)
 *    LFO waveform    KP_3        Cycle: Tri→Square→Random
 *    Pitch amount    (see screen) default 0 → set via LFO rate > 0
 *    PWM amount      (auto set)
 *    Filter amount   (auto set)
 *
 *  Effects presets  F11          Cycle: Dry→Echo→Reverb→Chorus→Phaser→Overdrive→All-in
 *
 *  LFO pitch mod    KP_*        Toggle: pitch vibrato on/off
 *  LFO PWM mod      KP_/        Toggle: PWM sweep on/off (pulse wave only)
 *  LFO filter mod   KP_ENTER    Toggle: filter wah on/off
 *
 *  Master gain      BACKSPACE/\       Master gain ±0.05  (0.1–2.0)
 *
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif
#include "shared/sit_example.h"
/* sit_example.h already includes situation.h */

#include <cglm/cglm.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

/* Tone synth control indices — mirror sit/aud/tone_synth_graph.h (internal header). */
#define SIT_TONE_CTRL_WAVEFORM          1
#define SIT_TONE_CTRL_VOLUME            2
#define SIT_TONE_CTRL_ATTACK            4
#define SIT_TONE_CTRL_DECAY             5
#define SIT_TONE_CTRL_SUSTAIN           6
#define SIT_TONE_CTRL_RELEASE           7
#define SIT_TONE_CTRL_HOLD              8
#define SIT_TONE_CTRL_FILTER_MODE       9
#define SIT_TONE_CTRL_FILTER_CUTOFF     10
#define SIT_TONE_CTRL_FILTER_RESONANCE  11
#define SIT_TONE_CTRL_PULSE_WIDTH       17
#define SIT_TONE_CTRL_LFO_RATE          18
#define SIT_TONE_CTRL_LFO_WAVEFORM      19
#define SIT_TONE_CTRL_LFO_PITCH_AMOUNT  20
#define SIT_TONE_CTRL_LFO_PITCH_RANGE   21
#define SIT_TONE_CTRL_LFO_PWM_AMOUNT    22
#define SIT_TONE_CTRL_LFO_PWM_RANGE     23
#define SIT_TONE_CTRL_LFO_FILTER_AMOUNT 24
#define SIT_TONE_CTRL_LFO_FILTER_RANGE  25
#define SIT_TONE_CTRL_FILTER_ENV_AMOUNT 26
#define SIT_TONE_CTRL_SUB_LEVEL         30
#define SIT_TONE_CTRL_SUB_WAVEFORM      31
#define SIT_TONE_CTRL_SUB_OCTAVE        32
#define SIT_TONE_CTRL_SUB_COARSE        34
#define SIT_TONE_CTRL_SUB_SYNC          35
#define SIT_TONE_CTRL_SUB_RING_MOD      36

#define OVERDRIVE_CTRL_DRIVE        1
#define OVERDRIVE_CTRL_MIX          4
#define CHORUS_CTRL_DRY_GAIN        17
#define CHORUS_CTRL_WET_GAIN        18
#define PHASER_CTRL_MIX             2

static void hud_txt(SituationCommandBuffer cmd, const char* s, float x, float y, ColorRGBA col)
{
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, s, (Vector2){{x, y}}, 14.0f, 0.5f, col);
}

/* ── Key → MIDI note mapping ──────────────────────────────────────────────
 * Three octaves: C3 (MIDI 48) through B5 (MIDI 83).
 * Layout mirrors a piano keyboard across four keyboard rows.
 * White keys on Z-row and Q-row; black keys on A-row and number-row.
 *
 * Lower octave (C3–B4):
 *   White: Z=C3  X=D3  C=E3  V=F3  B=G3  N=A3  M=B3  ,=C4  .=D4  /=E4
 *   Black: A=C#3 S=D#3 [F=F#3] D=... wait: standard layout:
 *          A=C#3 S=D#3  (no E#) D=F#3? No — let's use the classic tracker layout.
 *
 * Classic tracker piano layout (2 octaves across QWERTY + ZXCV):
 *   Z=C  S=C#  X=D  D=D#  C=E  V=F  G=F#  B=G  H=G#  N=A  J=A#  M=B  ,=C(+1)
 *   Q=C  2=C#  W=D  3=D#  E=E  R=F  5=F#  T=G  6=G#  Y=A  7=A#  U=B  I=C(+1)
 *
 * We use the classic 2-row tracker layout and add a third octave on the number
 * row above Q-row.
 */

typedef struct { int key; int midi_note; } KeyMap;

/* Lower octave (C3=48): Z-row whites + A-row blacks (classic tracker) */
static const KeyMap k_lower[] = {
    { SIT_KEY_Z,         48 }, /* C3  */
    { SIT_KEY_S,         49 }, /* C#3 */
    { SIT_KEY_X,         50 }, /* D3  */
    { SIT_KEY_D,         51 }, /* D#3 */
    { SIT_KEY_C,         52 }, /* E3  */
    { SIT_KEY_V,         53 }, /* F3  */
    { SIT_KEY_G,         54 }, /* F#3 */
    { SIT_KEY_B,         55 }, /* G3  */
    { SIT_KEY_H,         56 }, /* G#3 */
    { SIT_KEY_N,         57 }, /* A3  */
    { SIT_KEY_J,         58 }, /* A#3 */
    { SIT_KEY_M,         59 }, /* B3  */
    { 0, -1 }
};

/* Middle octave (C4=60): Q-row whites + number-row blacks */
static const KeyMap k_middle[] = {
    { SIT_KEY_Q,         60 }, /* C4  */
    { SIT_KEY_2,         61 }, /* C#4 */
    { SIT_KEY_W,         62 }, /* D4  */
    { SIT_KEY_3,         63 }, /* D#4 */
    { SIT_KEY_E,         64 }, /* E4  */
    { SIT_KEY_R,         65 }, /* F4  */
    { SIT_KEY_5,         66 }, /* F#4 */
    { SIT_KEY_T,         67 }, /* G4  */
    { SIT_KEY_6,         68 }, /* G#4 */
    { SIT_KEY_Y,         69 }, /* A4  */
    { SIT_KEY_7,         70 }, /* A#4 */
    { SIT_KEY_U,         71 }, /* B4  */
    { SIT_KEY_8,         72 }, /* C5  */
    { SIT_KEY_I,         73 }, /* C#5 */
    { SIT_KEY_9,         74 }, /* D5  */
    { SIT_KEY_O,         75 }, /* D#5 */
    { SIT_KEY_0,         76 }, /* E5  */
    { SIT_KEY_P,         77 }, /* F5  */
    { SIT_KEY_LEFT_BRACKET,  79 }, /* G5  */
    { SIT_KEY_RIGHT_BRACKET, 81 }, /* A5  */
    { 0, -1 }
};

#define TOTAL_KEYS 32  /* max keys we track */

/* ── Graph nodes ──────────────────────────────────────────────────────── */
typedef struct {
    SituationNodeHandle tone;
    SituationNodeHandle overdrive;
    SituationNodeHandle chorus;
    SituationNodeHandle phaser;
    SituationNodeHandle echo;
    SituationNodeHandle reverb;
    SituationNodeHandle gain;
    SituationNodeHandle mixer;  // final sink for proper output summing
} PianoNodes;

/* ── Synth state ──────────────────────────────────────────────────────── */
/* Per-key note tracking for NoteOn/Off */
static bool     s_key_held[400]; /* indexed by SIT_KEY_* value, true=currently held */
static int      s_key_note[400]; /* MIDI note for each key (with octave applied) */

static int      s_octave     = 0;    /* octave shift ±2 */
static int      s_waveform   = 0;    /* 0=Sine 1=Pulse 2=Tri 3=Saw 4=Noise */
static float    s_attack     = 0.01f;
static float    s_decay      = 0.08f;
static float    s_sustain    = 0.75f;
static float    s_release    = 0.22f;
static float    s_pwm        = 0.50f;

/* Filter */
static int      s_filt_mode  = 0;   /* 0=Off 1=LP 2=HP 3=BP 4=Notch 5=Allpass */
static float    s_filt_cut   = 4000.f;
static float    s_filt_res   = 0.707f;
static float    s_filt_env   = 0.0f;

/* Sub-osc */
static float    s_sub_level  = 0.0f;
static int      s_sub_wave   = 0;
static int      s_sub_oct    = 1;   /* 0=unison 1=oct-1 2=oct-2 */
static float    s_sub_coarse = 0.0f;/* ±12 st */
static bool     s_sub_sync   = false;
static bool     s_sub_ring   = false;

/* LFO */
static float    s_lfo_rate   = 0.0f;
static int      s_lfo_wave   = 0;   /* 0=Tri 1=Square 2=Random */
static bool     s_lfo_pitch  = false;
static bool     s_lfo_pwm    = false;
static bool     s_lfo_filt   = false;

/* Effects preset */
static int      s_fx_preset  = 0;   /* 0=Dry 1=Echo 2=Reverb 3=Chorus 4=Phaser 5=Drive 6=All */
static float    s_master     = 0.80f;

/* Sustain pedal (SPACE) */
static bool     s_sustain_ped = false;

/* ── Helpers ──────────────────────────────────────────────────────────── */
static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static const char* wf_name(int w) {
    static const char* n[] = { "Sine", "Pulse", "Tri", "Saw", "Noise" };
    return n[w < 0 ? 0 : (w > 4 ? 4 : w)];
}

static const char* filt_name(int m) {
    static const char* n[] = { "Off", "LP", "HP", "BP", "Notch", "Allpass" };
    return n[m < 0 ? 0 : (m > 5 ? 5 : m)];
}

static const char* sub_oct_name(int o) {
    static const char* n[] = { "Unison", "Oct-1", "Oct-2" };
    return n[o < 0 ? 0 : (o > 2 ? 2 : o)];
}

static const char* lfo_wave_name(int w) {
    static const char* n[] = { "Tri", "Square", "Rand" };
    return n[w < 0 ? 0 : (w > 2 ? 2 : w)];
}

static const char* fx_preset_name(int p) {
    static const char* n[] = { "Dry", "Echo", "Reverb", "Chorus", "Phaser", "Drive", "All-In" };
    return n[p < 0 ? 0 : (p > 6 ? 6 : p)];
}

/* ── Graph build ──────────────────────────────────────────────────────── */
static SituationAudioGraph* build_graph(PianoNodes* n) {
    SituationAudioGraph* g = SituationCreateGraph();
    if (!g) return NULL;

    if (SituationCreateNode(g, SITUATION_NODE_TONE_SYNTH, &n->tone)      != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_OVERDRIVE,  &n->overdrive) != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_CHORUS,     &n->chorus)    != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_PHASER,     &n->phaser)    != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_ECHO,       &n->echo)      != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_REVERB,     &n->reverb)    != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_GAIN,       &n->gain)      != SITUATION_SUCCESS ||
        SituationCreateNode(g, SITUATION_NODE_MIXER,      &n->mixer)     != SITUATION_SUCCESS) {
        SituationDestroyGraph(g);
        return NULL;
    }

    /* Chain: tone → overdrive → chorus → phaser → echo → reverb → gain */
    if (SituationCreatePatch(g, n->tone,      0, n->overdrive, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->overdrive, 0, n->chorus,    0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->chorus,    0, n->phaser,    0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->phaser,    0, n->echo,      0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->echo,      0, n->reverb,    0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->reverb,    0, n->gain,      0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g, n->gain,      0, n->mixer,     0, false) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g);
        return NULL;
    }

    if (SituationTopologicalSort(g) != SITUATION_SUCCESS) {
        SituationDestroyGraph(g);
        return NULL;
    }

    /* Tone synth defaults */
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_WAVEFORM,   (float)s_waveform);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_VOLUME,     0.75f);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_ATTACK,     s_attack);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_DECAY,      s_decay);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUSTAIN,    s_sustain);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_RELEASE,    s_release);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_HOLD,       -1.0f); /* infinite — MIDI NoteOff triggers release */
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_FILTER_MODE, 0.0f);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_PULSE_WIDTH, s_pwm);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUB_OCTAVE, (float)s_sub_oct);

    /* Overdrive: start bypassed (mix=0, dry via mix knob) */
    SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_DRIVE, 40.0f);
    SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_MIX,   0.0f);

    /* Chorus: dry=1 wet=0 (required — zero dry silences the chain) */
    SituationSetControl(g, n->chorus, CHORUS_CTRL_DRY_GAIN, 1.0f);
    SituationSetControl(g, n->chorus, CHORUS_CTRL_WET_GAIN, 0.0f);

    /* Phaser: start bypassed (mix=0) */
    SituationSetControl(g, n->phaser, PHASER_CTRL_MIX, 0.0f);

    /* Echo: start bypassed (wet_mix=0) */
    SituationSetControl(g, n->echo, 0, 0.30f);      /* time */
    SituationSetControl(g, n->echo, 1, 0.35f);      /* feedback */
    SituationSetControl(g, n->echo, 2, 0.0f);       /* wet_mix = 0 */

    /* Reverb: start bypassed (wet=0) */
    SituationSetControl(g, n->reverb, 0, 0.65f);    /* room_size */
    SituationSetControl(g, n->reverb, 1, 0.45f);    /* damp */
    SituationSetControl(g, n->reverb, 2, 0.0f);     /* wet = 0 */
    SituationSetControl(g, n->reverb, 3, 1.0f);     /* dry */
    SituationSetControl(g, n->reverb, 4, 0.80f);    /* width */

    /* Master gain */
    SituationSetControl(g, n->gain, 0, s_master);
    SituationSetControl(g, n->mixer, 0, 1.0f);  /* mixer output level */

    return g;
}

/* ── Apply current synth state to graph ──────────────────────────────── */
static void apply_synth_params(SituationAudioGraph* g, const PianoNodes* n) {
    /* ADSR */
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_ATTACK,  s_attack);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_DECAY,   s_decay);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUSTAIN, s_sustain);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_RELEASE, s_release);

    /* Waveform + PWM */
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_WAVEFORM,    (float)s_waveform);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_PULSE_WIDTH, s_pwm);

    /* Filter */
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_FILTER_MODE,       (float)s_filt_mode);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_FILTER_CUTOFF,     s_filt_cut);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_FILTER_RESONANCE,  s_filt_res);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_FILTER_ENV_AMOUNT, s_filt_env);

    /* Sub-oscillator */
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUB_LEVEL,    s_sub_level);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUB_WAVEFORM, (float)s_sub_wave);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUB_OCTAVE,   (float)s_sub_oct);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUB_COARSE,   s_sub_coarse);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUB_SYNC,     s_sub_sync   ? 1.0f : 0.0f);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_SUB_RING_MOD, s_sub_ring   ? 1.0f : 0.0f);

    /* LFO */
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_LFO_RATE,         s_lfo_rate);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_LFO_WAVEFORM,     (float)s_lfo_wave);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_LFO_PITCH_AMOUNT, s_lfo_pitch ? 0.8f : 0.0f);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_LFO_PITCH_RANGE,  2.0f);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_LFO_PWM_AMOUNT,   s_lfo_pwm   ? 0.7f : 0.0f);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_LFO_PWM_RANGE,    0.20f);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_LFO_FILTER_AMOUNT,s_lfo_filt  ? 0.7f : 0.0f);
    SituationSetControl(g, n->tone, SIT_TONE_CTRL_LFO_FILTER_RANGE, 1200.0f);

    /* Master gain */
    SituationSetControl(g, n->gain, 0, s_master);
}

/* Apply FX preset — wet levels only, don't touch node params */
static void apply_fx_preset(SituationAudioGraph* g, const PianoNodes* n, int preset) {
    /* Reset all effects wet to 0 first; keep chorus dry at 1.0 so the chain stays audible */
    SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_MIX, 0.0f);
    SituationSetControl(g, n->chorus,    CHORUS_CTRL_DRY_GAIN, 1.0f);
    SituationSetControl(g, n->chorus,    CHORUS_CTRL_WET_GAIN, 0.0f);
    SituationSetControl(g, n->phaser,    PHASER_CTRL_MIX,      0.0f);
    SituationSetControl(g, n->echo,      2, 0.0f);
    SituationSetControl(g, n->reverb,    2, 0.0f);

    switch (preset) {
        case 0: /* Dry — all off */ break;
        case 1: /* Echo only */
            SituationSetControl(g, n->echo, 2, 0.45f);
            break;
        case 2: /* Reverb only */
            SituationSetControl(g, n->reverb, 2, 0.50f);
            break;
        case 3: /* Chorus */
            SituationSetControl(g, n->chorus, CHORUS_CTRL_WET_GAIN, 0.80f);
            break;
        case 4: /* Phaser */
            SituationSetControl(g, n->phaser, PHASER_CTRL_MIX, 0.60f);
            break;
        case 5: /* Overdrive */
            SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_MIX, 0.85f);
            break;
        case 6: /* All-in */
            SituationSetControl(g, n->overdrive, OVERDRIVE_CTRL_MIX, 0.35f);
            SituationSetControl(g, n->chorus,    CHORUS_CTRL_WET_GAIN, 0.45f);
            SituationSetControl(g, n->phaser,    PHASER_CTRL_MIX,      0.30f);
            SituationSetControl(g, n->echo,      2, 0.28f);
            SituationSetControl(g, n->reverb,    2, 0.42f);
            break;
        default: break;
    }
}

/* ── Note helpers ─────────────────────────────────────────────────────── */
static void note_on(int midi_note, int raw_key) {
    if (midi_note < 0 || midi_note > 127) return;
    if (s_key_held[raw_key]) return; /* already held */
    s_key_held[raw_key] = true;
    s_key_note[raw_key] = midi_note;
    SituationVirtualMidiNoteOn((uint8_t)midi_note, 100);
}

static void note_off(int raw_key) {
    if (!s_key_held[raw_key]) return;
    s_key_held[raw_key] = false;
    int midi_note = s_key_note[raw_key];
    if (!s_sustain_ped) {
        SituationVirtualMidiNoteOff((uint8_t)midi_note);
    }
}

/* Release all held notes (called on sustain pedal release if pedal was lifting notes) */
static void release_all_held_notes(void) {
    for (int k = 0; k < 400; k++) {
        if (s_key_held[k]) continue; /* still physically held */
        /* key not physically held but note was sustained by pedal — send NoteOff now */
    }
    /* Actually pedal-off sends CC64=0; the synth handles this internally */
    SituationVirtualMidiControlChange(0, 64, 0);
}

/* ── HUD drawing ──────────────────────────────────────────────────────── */
static void draw_hud(SituationCommandBuffer cmd, int sw, int sh) {
    (void)sh;

    float x  = 8.0f;
    float y  = 28.0f;
    float dy = 17.0f;

    ColorRGBA hi   = {230, 235, 255, 255};
    ColorRGBA val  = {100, 220, 140, 255};
    ColorRGBA dim  = {140, 145, 170, 210};
    ColorRGBA head = {255, 200, 60,  255};
    ColorRGBA on   = {80,  230, 80,  255};
    ColorRGBA off  = {150, 80,  80,  220};

    char line[256];

    /* ── Title / octave / sustain ── */
    snprintf(line, sizeof line,
        "Node-Graph Piano  oct %+d (F2/F3)  Sus:%s (SPACE)",
        s_octave, s_sustain_ped ? "ON " : "off");
    hud_txt(cmd,line, x, y, head); y += dy;

    /* ── ADSR ── */
    snprintf(line, sizeof line,
        "ADSR  A:%.3fs (\x1B\x1A) D:%.3fs (\x18\x19) S:%.2f (;/') R:%.3fs (+/-)",
        s_attack, s_decay, s_sustain, s_release);
    hud_txt(cmd,line, x, y, hi); y += dy;

    /* ── Waveform + PWM ── */
    snprintf(line, sizeof line,
        "Wave: %s (TAB)   PWM:%.2f (,/.)  [active when Pulse]",
        wf_name(s_waveform), s_pwm);
    hud_txt(cmd,line, x, y, hi); y += dy;

    /* ── Filter ── */
    snprintf(line, sizeof line,
        "Filt: %s (F4)  Cut:%.0fHz (F5/F6)  Res:%.2f (F7/F8)  EnvAmt:%.2f ([/])",
        filt_name(s_filt_mode), s_filt_cut, s_filt_res, s_filt_env);
    hud_txt(cmd,line, x, y, hi); y += dy;

    /* ── Sub-oscillator ── */
    snprintf(line, sizeof line,
        "Sub: lvl:%.2f (KP4/6) wf:%s (KP7) oct:%s (KP8/9) crs:%+.0fst (KP+/-)",
        s_sub_level, wf_name(s_sub_wave), sub_oct_name(s_sub_oct), s_sub_coarse);
    hud_txt(cmd,line, x, y, hi); y += dy;

    snprintf(line, sizeof line,
        "     Sync:%s (KP5)  RingMod:%s (KP0)",
        s_sub_sync ? "ON " : "off",
        s_sub_ring ? "ON " : "off");
    hud_txt(cmd,line, x, y, hi); y += dy;

    /* ── LFO ── */
    snprintf(line, sizeof line,
        "LFO: %.2fHz (KP1/2) wf:%s (KP3)  Pitch:%s (KP*)  PWM:%s (KP/)  Filt:%s (KPEnter)",
        s_lfo_rate, lfo_wave_name(s_lfo_wave),
        s_lfo_pitch ? "ON " : "off",
        s_lfo_pwm   ? "ON " : "off",
        s_lfo_filt  ? "ON " : "off");
    hud_txt(cmd,line, x, y, hi); y += dy;

    /* ── FX Preset + Master ── */
    snprintf(line, sizeof line,
        "FX: %s (F11)   Master:%.2f (\\=/BACKSPACE)",
        fx_preset_name(s_fx_preset), s_master);
    hud_txt(cmd,line, x, y, hi); y += dy + 4.0f;

    /* ── Piano keyboard visual (two rows) ── */
    /* Lower row label */
    hud_txt(cmd,"Lower: Z S X D C V G B H N J M", x, y, dim); y += dy;

    /* Upper row */
    hud_txt(cmd,"Upper: Q 2 W 3 E R 5 T 6 Y 7 U 8 I 9 O 0 P [ ]", x, y, dim); y += dy;

    /* Currently held notes */
    int any_held = 0;
    char held_buf[128] = "Held: ";
    for (int k = 0; k < 400; k++) {
        if (s_key_held[k]) {
            int note = s_key_note[k];
            static const char* note_names[] = {
                "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
            };
            char tmp[12];
            snprintf(tmp, sizeof tmp, "%s%d ", note_names[note % 12], note / 12 - 1);
            strncat(held_buf, tmp, sizeof(held_buf) - strlen(held_buf) - 1);
            any_held++;
            if (any_held >= 8) { strncat(held_buf, "...", sizeof(held_buf)-strlen(held_buf)-1); break; }
        }
    }
    if (any_held) hud_txt(cmd, held_buf, x, y, val);
    else          hud_txt(cmd, "Held: (none)", x, y, dim);
    y += dy;

    /* FPS (right-aligned) */
    char fps[32];
    snprintf(fps, sizeof fps, "FPS:%d", SituationGetFPS());
    float fpsw = (float)strlen(fps) * 9.0f;
    hud_txt(cmd, fps, (float)sw - fpsw - 8.0f, 28.0f, (ColorRGBA){255, 240, 80, 255});
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(int argc, char** argv) {
    if (SitExample_Init(argc, argv, "Situation — Node Graph Piano") != SITUATION_SUCCESS)
        return -1;

    SitExample_InitAudioRegistry();

    PianoNodes nodes = {
        SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE,
        SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE,
        SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE,
        SITUATION_INVALID_NODE_HANDLE, SITUATION_INVALID_NODE_HANDLE
    };

    SituationAudioGraph* graph = build_graph(&nodes);
    if (!graph) { SitExample_Shutdown(); return -1; }

    (void)SitExample_WireToneSynthVirtualMidi(graph, nodes.tone, "19", NULL);

    if (SitExample_ActivateAudioGraph(graph) != SITUATION_SUCCESS) {
        SituationDestroyGraph(graph);
        SitExample_Shutdown();
        return -1;
    }

    /* Initial state push */
    apply_synth_params(graph, &nodes);
    apply_fx_preset(graph, &nodes, s_fx_preset);

    memset(s_key_held, 0,  sizeof s_key_held);
    memset(s_key_note, -1, sizeof s_key_note);

    /* ── Main loop ────────────────────────────────────────────────────── */
    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) break;

        bool dirty = false; /* set true when synth params changed */

        /* ── Octave ── */
        if (SituationIsKeyPressed(SIT_KEY_F2)) { s_octave = s_octave > -2 ? s_octave - 1 : -2; }
        if (SituationIsKeyPressed(SIT_KEY_F3)) { s_octave = s_octave <  2 ? s_octave + 1 :  2; }

        /* ── ADSR ── */
        if (SituationIsKeyPressed(SIT_KEY_LEFT)  || SituationIsKeyDown(SIT_KEY_LEFT))
            { s_attack  = clampf(s_attack  - 0.005f, 0.001f, 2.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_RIGHT) || SituationIsKeyDown(SIT_KEY_RIGHT))
            { s_attack  = clampf(s_attack  + 0.005f, 0.001f, 2.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_DOWN)  || SituationIsKeyDown(SIT_KEY_DOWN))
            { s_decay   = clampf(s_decay   - 0.005f, 0.001f, 2.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_UP)    || SituationIsKeyDown(SIT_KEY_UP))
            { s_decay   = clampf(s_decay   + 0.005f, 0.001f, 2.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_SEMICOLON))
            { s_sustain = clampf(s_sustain - 0.05f, 0.0f, 1.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_APOSTROPHE))
            { s_sustain = clampf(s_sustain + 0.05f, 0.0f, 1.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_MINUS))
            { s_release = clampf(s_release - 0.05f, 0.01f, 5.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_EQUAL))
            { s_release = clampf(s_release + 0.05f, 0.01f, 5.0f); dirty = true; }

        /* ── Waveform + PWM ── */
        if (SituationIsKeyPressed(SIT_KEY_TAB)) {
            s_waveform = (s_waveform + 1) % 5;
            dirty = true;
        }
        if (SituationIsKeyPressed(SIT_KEY_COMMA))
            { s_pwm = clampf(s_pwm - 0.02f, 0.05f, 0.95f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_PERIOD))
            { s_pwm = clampf(s_pwm + 0.02f, 0.05f, 0.95f); dirty = true; }

        /* ── Filter ── */
        if (SituationIsKeyPressed(SIT_KEY_F4)) {
            s_filt_mode = (s_filt_mode + 1) % 6;
            dirty = true;
        }
        if (SituationIsKeyPressed(SIT_KEY_F5))
            { s_filt_cut = clampf(s_filt_cut * 0.89f, 40.0f, 19000.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_F6))
            { s_filt_cut = clampf(s_filt_cut * 1.12f, 40.0f, 19000.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_F7))
            { s_filt_res = clampf(s_filt_res - 0.5f, 0.5f, 20.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_F8))
            { s_filt_res = clampf(s_filt_res + 0.5f, 0.5f, 20.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_LEFT_BRACKET))
            { s_filt_env = clampf(s_filt_env - 0.05f, 0.0f, 1.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_RIGHT_BRACKET))
            { s_filt_env = clampf(s_filt_env + 0.05f, 0.0f, 1.0f); dirty = true; }

        /* ── Sub oscillator (KP keys) ── */
        if (SituationIsKeyPressed(SIT_KEY_KP_4))
            { s_sub_level = clampf(s_sub_level - 0.05f, 0.0f, 1.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_6))
            { s_sub_level = clampf(s_sub_level + 0.05f, 0.0f, 1.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_7)) {
            s_sub_wave = (s_sub_wave + 1) % 5; dirty = true;
        }
        if (SituationIsKeyPressed(SIT_KEY_KP_8)) {
            s_sub_oct = s_sub_oct > 0 ? s_sub_oct - 1 : 0; dirty = true;
        }
        if (SituationIsKeyPressed(SIT_KEY_KP_9)) {
            s_sub_oct = s_sub_oct < 2 ? s_sub_oct + 1 : 2; dirty = true;
        }
        if (SituationIsKeyPressed(SIT_KEY_KP_5)) { s_sub_sync = !s_sub_sync; dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_0)) { s_sub_ring = !s_sub_ring; dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_ADD))
            { s_sub_coarse = clampf(s_sub_coarse + 1.0f, -12.0f, 12.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_SUBTRACT))
            { s_sub_coarse = clampf(s_sub_coarse - 1.0f, -12.0f, 12.0f); dirty = true; }

        /* ── LFO ── */
        if (SituationIsKeyPressed(SIT_KEY_KP_1))
            { s_lfo_rate = clampf(s_lfo_rate - 0.2f, 0.0f, 20.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_2))
            { s_lfo_rate = clampf(s_lfo_rate + 0.2f, 0.0f, 20.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_3)) {
            s_lfo_wave = (s_lfo_wave + 1) % 3; dirty = true;
        }
        if (SituationIsKeyPressed(SIT_KEY_KP_MULTIPLY)) { s_lfo_pitch = !s_lfo_pitch; dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_DIVIDE))   { s_lfo_pwm   = !s_lfo_pwm;   dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_KP_ENTER))    { s_lfo_filt  = !s_lfo_filt;  dirty = true; }

        /* ── FX preset ── */
        if (SituationIsKeyPressed(SIT_KEY_F11)) {
            s_fx_preset = (s_fx_preset + 1) % 7;
            apply_fx_preset(graph, &nodes, s_fx_preset);
        }

        /* ── Master gain ── */
        if (SituationIsKeyPressed(SIT_KEY_BACKSLASH))
            { s_master = clampf(s_master - 0.05f, 0.1f, 2.0f); dirty = true; }
        if (SituationIsKeyPressed(SIT_KEY_BACKSPACE))
            { s_master = clampf(s_master + 0.05f, 0.1f, 2.0f); dirty = true; }

        /* ── Sustain pedal ── */
        bool space_now = SituationIsKeyDown(SIT_KEY_SPACE);
        if (space_now && !s_sustain_ped) {
            s_sustain_ped = true;
            SituationVirtualMidiControlChange(0, 64, 127); /* CC64 sustain on */
        } else if (!space_now && s_sustain_ped) {
            s_sustain_ped = false;
            release_all_held_notes();
        }

        /* Push param changes if needed */
        if (dirty) apply_synth_params(graph, &nodes);

        /* ── Piano key scan — check each mapped key ── */
        /* Lower octave */
        for (const KeyMap* k = k_lower; k->key != 0; k++) {
            int note = k->midi_note + s_octave * 12;
            note = (note < 0) ? 0 : (note > 127 ? 127 : note);
            int key = k->key;
            bool down = SituationIsKeyDown(key);
            if (down  && !s_key_held[key]) note_on(note, key);
            if (!down &&  s_key_held[key]) note_off(key);
        }
        /* Middle / upper octave */
        for (const KeyMap* k = k_middle; k->key != 0; k++) {
            int note = k->midi_note + s_octave * 12;
            note = (note < 0) ? 0 : (note > 127 ? 127 : note);
            int key = k->key;
            bool down = SituationIsKeyDown(key);
            if (down  && !s_key_held[key]) note_on(note, key);
            if (!down &&  s_key_held[key]) note_off(key);
        }

        /* ── Render ── */
        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id       = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear  = { .color = {14, 12, 22, 255} }
                },
                .depth_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .depth = 1.0f } },
            };
            SituationCmdBeginRenderPass(cmd, &pass);

            int sw = SituationGetRenderWidth();
            int sh = SituationGetRenderHeight();

            /* Dark header bar */
            {
                mat4 m; glm_mat4_identity(m);
                glm_translate(m, (vec3){0.0f, 0.0f, 0.0f});
                glm_scale(m, (vec3){(float)sw, 22.0f, 1.0f});
                SituationCmdDrawQuad(cmd, m, (Vector4){{0.0f, 0.0f, 0.0f, 0.70f}});
            }

            /* Standard HUD (title, FPS, hotkeys bar) */
            SitExample_DrawHUD(cmd,
                "19 — Node Graph Piano",
                "Piano:Z/Q rows | ADSR:arrows | FX:F11 | TAB:wave | ESC");

            draw_hud(cmd, sw, sh);

            SituationCmdEndRenderPass(cmd);
            SitExample_EndFrame();
        }
    }

    /* Cleanup */
    SituationVirtualMidiControlChange(0, 123, 0); /* all-notes-off */
    SitExample_DestroyAudioGraph(&graph, nodes.tone);
    SitExample_TeardownVirtualMidi();
    SitExample_Shutdown();
    return 0;
}
