/***************************************************************************************************
*
*   sit/aud/tone_synth_graph.h - Graph tone synth node + MIDI plumbing (shared)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   16-voice polyphonic graph tone synth per node (legacy pool remains 64) — ADSR, pan,
 *   waveforms, velocity). Per-voice Polysonix-style SVF (sit/aud/fx/filter.h). Oscillators use
 *   polyBLEP (pulse/saw) and polyBLAMP (triangle) for band-limited edges; hard-sync resets apply
 *   a matching blep at the master wrap. MIDI global:
 *   bend, CC1 vibrato, CC7/CC11 volume, CC64 sustain, CC92 tremolo, CC70 waveform,
 *   CC72–77 ADSR, CC16/17/18/22/71/74/102 filter, CC106 pulse width, CC24–31 mod LFO,
 *   CC5 portamento time, CC20 portamento speed (st/s), CC107–110 sub-osc level/wave/octave/fine,
 *   CC111–113 sub coarse (±12 st) / sync / ring mod,
 *   CC114 patch slot 0–15 (recall on change), CC115 patch store (≥64 saves to slot),
 *   CC126 mono / CC127 poly, CC123 all-notes-off. Post-voice sum bus uses the same
 *   enhanced lookahead limiter as Polysonix (sit/aud/polysonix/polysonix.h).
 *
***************************************************************************************************/

#ifndef SITUATION_TONE_SYNTH_GRAPH_H
#define SITUATION_TONE_SYNTH_GRAPH_H

#include "../situation_base_etc.h"
#include "fx/filter.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef SIT_TONE_PI
#define SIT_TONE_PI  ((float)M_PI)
#endif
#ifndef SIT_TONE_TWO_PI
#define SIT_TONE_TWO_PI (2.0f * SIT_TONE_PI)
#endif

#ifndef SITUATION_TONE_SYNTH_VIBRATO_HZ
#define SITUATION_TONE_SYNTH_VIBRATO_HZ 5.0f
#endif

#ifndef SITUATION_TONE_SYNTH_TREMOLO_HZ
#define SITUATION_TONE_SYNTH_TREMOLO_HZ 5.0f
#endif

#ifndef SITUATION_TONE_SYNTH_MAX_VOICES
#define SITUATION_TONE_SYNTH_MAX_VOICES 16
#endif

/*
 * Tone synth control indices (registry + voice template).
 *
 * Why two layers: real MIDI gear exposes a "MIDI card" — note/CC/bend as integer
 * bytes (0..127, etc.). That card is what controllers, DAWs, and patch dumps speak.
 * The synth engine uses production parameters (seconds, Hz, levels) in control_values[].
 * Situation maps MIDI card -> ctrl; never treat a CC as a fractional production value.
 *
 *   MIDI  = card / wire: integers only (CC 0..127, note 0..127, bend 0..16383).
 *   ctrl  = production: float or int in engineering units (may be fractional).
 */
#define SIT_TONE_CTRL_FREQUENCY         0   /* ctrl float Hz 20-20000; manual only */
#define SIT_TONE_CTRL_WAVEFORM          1   /* ctrl int 0-4 sine..noise; MIDI CC70 int 0-127 */
#define SIT_TONE_CTRL_VOLUME            2   /* ctrl float 0-1 level; note vel+CC7/11 not this */
#define SIT_TONE_CTRL_PAN               3   /* ctrl float -1..+1; MIDI CC10 int 0-127 */
#define SIT_TONE_CTRL_ATTACK            4   /* ctrl float seconds 0-2; MIDI CC73 int 0-127 */
#define SIT_TONE_CTRL_DECAY             5   /* ctrl float seconds 0-2; MIDI CC75 int 0-127 */
#define SIT_TONE_CTRL_SUSTAIN           6   /* ctrl float 0-1; MIDI CC76 int 0-127 */
#define SIT_TONE_CTRL_RELEASE           7   /* ctrl float seconds 0-5; MIDI CC72 int 0-127 */
#define SIT_TONE_CTRL_HOLD              8   /* ctrl float seconds -1=inf 0-10; MIDI CC77 int 0-127 */
#define SIT_TONE_CTRL_FILTER_MODE       9   /* ctrl int 0-8 off..BP+HP; MIDI CC16 int 0-127 */
#define SIT_TONE_CTRL_FILTER_CUTOFF     10  /* ctrl float Hz 20-20000; MIDI CC74 int 0-127 */
#define SIT_TONE_CTRL_FILTER_RESONANCE  11  /* ctrl float Q; MIDI CC71 int 0-127 */
#define SIT_TONE_CTRL_FILTER_POLES      12  /* ctrl int 1-4; MIDI CC102 int 0-127 */
#define SIT_TONE_CTRL_FILTER_DRIVE      13  /* ctrl float 1-10; MIDI CC17 int 0-127 */
#define SIT_TONE_CTRL_FILTER_KEYTRACK   14  /* ctrl float 0-1; MIDI CC22 int 0-127 */
#define SIT_TONE_CTRL_FILTER_OVERSAMPLE 15  /* ctrl int 0-2 off/2x; MIDI CC18 int 0-127 */
#define SIT_TONE_CTRL_VOICE_MODE        16  /* ctrl int 0=poly 1=mono; MIDI CC126/127 */
#define SIT_TONE_CTRL_PULSE_WIDTH       17  /* ctrl float duty wf1; MIDI CC106 int 0-127 */
#define SIT_TONE_CTRL_LFO_RATE          18  /* ctrl float Hz 0=off; MIDI CC24 int 0-127 */
#define SIT_TONE_CTRL_LFO_WAVEFORM      19  /* ctrl int 0-2 tri/sq/rand; MIDI CC25 int 0-127 */
#define SIT_TONE_CTRL_LFO_PITCH_AMOUNT  20  /* ctrl float 0-1; MIDI CC26 int 0-127 */
#define SIT_TONE_CTRL_LFO_PITCH_RANGE   21  /* ctrl float semitones 0-12; MIDI CC27 int 0-127 */
#define SIT_TONE_CTRL_LFO_PWM_AMOUNT    22  /* ctrl float 0-1; MIDI CC28 int 0-127 */
#define SIT_TONE_CTRL_LFO_PWM_RANGE     23  /* ctrl float duty span; MIDI CC29 int 0-127 */
#define SIT_TONE_CTRL_LFO_FILTER_AMOUNT 24  /* ctrl float 0-1; MIDI CC30 int 0-127 */
#define SIT_TONE_CTRL_LFO_FILTER_RANGE  25  /* ctrl float Hz span; MIDI CC31 int 0-127 */
#define SIT_TONE_CTRL_FILTER_ENV_AMOUNT 26  /* ctrl float 0-1; MIDI CC32 int 0-127 */
#define SIT_TONE_CTRL_FILTER_ENV_RANGE  27  /* ctrl float Hz span; MIDI CC33 int 0-127 */
#define SIT_TONE_CTRL_PORTAMENTO_TIME   28  /* ctrl float seconds mono; MIDI CC5 int 0-127 */
#define SIT_TONE_CTRL_PORTAMENTO_SPEED  29  /* ctrl float st/s mono; MIDI CC20 int 0-127 */
#define SIT_TONE_CTRL_PORTAMENTO        SIT_TONE_CTRL_PORTAMENTO_TIME /* alias ctrl 28 */
#define SIT_TONE_CTRL_SUB_LEVEL         30  /* ctrl float 0-1 mix; MIDI CC107 int 0-127 */
#define SIT_TONE_CTRL_SUB_WAVEFORM      31  /* ctrl int 0-4; MIDI CC108 int 0-127 */
#define SIT_TONE_CTRL_SUB_OCTAVE        32  /* ctrl int 0-2 unison/oct-1/oct-2; MIDI CC109 int 0-127 */
#define SIT_TONE_CTRL_SUB_FINE          33  /* ctrl float semitones; MIDI CC110 int 0-127 */
#ifndef SIT_TONE_SUB_COARSE_SEMITONE_MAX
#define SIT_TONE_SUB_COARSE_SEMITONE_MAX 12.0f /* ±1 octave from main note */
#endif

#define SIT_TONE_CTRL_SUB_COARSE        34  /* ctrl float semitones ±12; MIDI CC111 — coarse offset from main */
#define SIT_TONE_CTRL_SUB_SYNC          35  /* ctrl bool; MIDI CC112 — sub hard-synced to main (main=master) */
#define SIT_TONE_CTRL_SUB_RING_MOD      36  /* ctrl bool; MIDI CC113 — ring: dry/wet crossfade main×(1-lvl) + (main×sub)×lvl */
#define SIT_TONE_CTRL_PATCH_SLOT        37  /* ctrl int 0-15; MIDI CC114 int 0-127 — recall slot on change */
#define SIT_TONE_CTRL_PATCH_STORE       38  /* ctrl bool; MIDI CC115 int ≥64 save — also SetControl ≥0.5 */
#define SIT_TONE_CTRL_LFO_RESET_ON_KEY  39  /* ctrl bool; MIDI CC116 int ≥64 — reset mod LFO phase on note-on */
#define SIT_TONE_CTRL_LFO_START_PHASE   40  /* ctrl float 0..1; MIDI CC117 — cycle position at reset (0=0°, 0.25=90°, …) */
#define SIT_TONE_CTRL_GLIDE_RESET_ON_KEY 41 /* ctrl bool; MIDI CC118 ≥64 — mono legato: retrigger ADSR on note-on (pitch glide unchanged) */

#define SITUATION_TONE_SYNTH_PATCH_SLOT_COUNT 16
#define SITUATION_TONE_SYNTH_PATCH_PARAM_FIRST SIT_TONE_CTRL_WAVEFORM
#define SITUATION_TONE_SYNTH_PATCH_PARAM_LAST  SIT_TONE_CTRL_SUB_RING_MOD
#define SITUATION_TONE_SYNTH_PATCH_PARAM_COUNT \
    (SITUATION_TONE_SYNTH_PATCH_PARAM_LAST - SITUATION_TONE_SYNTH_PATCH_PARAM_FIRST + 1)
#define SITUATION_TONE_SYNTH_PATCH_CC_SLOT 114
#define SITUATION_TONE_SYNTH_PATCH_CC_STORE 115

#ifndef SITUATION_TONE_SYNTH_SUSTAIN_FULL_THRESH
#define SITUATION_TONE_SYNTH_SUSTAIN_FULL_THRESH 0.999f
#endif

/* Release reaches ~-60 dB at progress=1 (classic RC/exponential tail, not linear). */
#ifndef SITUATION_TONE_SYNTH_ENV_RELEASE_FLOOR
#define SITUATION_TONE_SYNTH_ENV_RELEASE_FLOOR 0.001f
#endif
#define SIT_TONE_SYNTH_ENV_RELEASE_LN_FLOOR (-6.90775527898f) /* ln(0.001) */

/* Polysonix master bus limiter defaults (patch.limiter_threshold / limiter_release_ms). */
#ifndef SITUATION_TONE_SYNTH_SUM_LIMITER_THRESHOLD
#define SITUATION_TONE_SYNTH_SUM_LIMITER_THRESHOLD 0.95f
#endif
#ifndef SITUATION_TONE_SYNTH_SUM_LIMITER_RELEASE_MS
#define SITUATION_TONE_SYNTH_SUM_LIMITER_RELEASE_MS 50.0f
#endif

/** Same layout and algorithm as Polysonix `EnhancedLimiter` (post-voice sum). */
typedef struct SituationToneSynthSumLimiter {
    float threshold;
    float ratio;
    float attack_coeff;
    float release_coeff;
    float makeup_gain;
    float envelope;
    float* delay_line_l;
    float* delay_line_r;
    int delay_write_pos;
    int delay_samples;
    int buffer_capacity;
    float smooth_gain;
    float target_gain;
    float peak_hold;
    int peak_hold_samples;
    bool initialized;
    float release_ms_cache;
} SituationToneSynthSumLimiter;

static inline void _SituationToneSynthSumLimiterInit(SituationToneSynthSumLimiter* limiter,
                                                     float sample_rate, float threshold,
                                                     float release_ms) {
    if (sample_rate <= 0.0f) {
        limiter->initialized = false;
        return;
    }

    limiter->threshold = threshold;
    limiter->ratio = 20.0f;
    limiter->makeup_gain = 1.0f / limiter->threshold;
    const float attack_time_ms = 0.1f;
    limiter->attack_coeff = expf(-1.0f / (attack_time_ms * 0.001f * sample_rate));
    limiter->release_coeff = expf(-1.0f / (release_ms * 0.001f * sample_rate));
    limiter->delay_samples = (int)(sample_rate * 0.001f);

    if (limiter->buffer_capacity > 0) {
        if (limiter->delay_samples >= limiter->buffer_capacity) {
            limiter->delay_samples = limiter->buffer_capacity - 1;
        }
    }
    if (limiter->delay_samples < 1) {
        limiter->delay_samples = 1;
    }

    if (limiter->delay_line_l) {
        memset(limiter->delay_line_l, 0, (size_t)limiter->buffer_capacity * sizeof(float));
    }
    if (limiter->delay_line_r) {
        memset(limiter->delay_line_r, 0, (size_t)limiter->buffer_capacity * sizeof(float));
    }

    limiter->delay_write_pos = 0;
    limiter->envelope = 0.0f;
    limiter->smooth_gain = 1.0f;
    limiter->target_gain = 1.0f;
    limiter->peak_hold = 0.0f;
    limiter->peak_hold_samples = 0;
    limiter->release_ms_cache = release_ms;
    limiter->initialized = true;
}

static inline void _SituationToneSynthSumLimiterProcess(SituationToneSynthSumLimiter* limiter,
                                                        float* input_l, float* input_r,
                                                        float* output_l, float* output_r,
                                                        float sample_rate) {
    if (!limiter->initialized) {
        *output_l = *input_l * 0.5f;
        *output_r = *input_r * 0.5f;
        return;
    }

    limiter->delay_line_l[limiter->delay_write_pos] = *input_l;
    limiter->delay_line_r[limiter->delay_write_pos] = *input_r;

    const int cap = limiter->buffer_capacity;
    const int read_pos =
        (limiter->delay_write_pos - limiter->delay_samples + cap) % cap;
    const float delayed_l = limiter->delay_line_l[read_pos];
    const float delayed_r = limiter->delay_line_r[read_pos];

    limiter->delay_write_pos = (limiter->delay_write_pos + 1) % cap;

    const float input_peak = fmaxf(fabsf(*input_l), fabsf(*input_r));

    if (input_peak > limiter->peak_hold) {
        limiter->peak_hold = input_peak;
        limiter->peak_hold_samples = (int)(sample_rate * 0.002f);
    } else if (limiter->peak_hold_samples > 0) {
        limiter->peak_hold_samples--;
    } else {
        limiter->peak_hold *= 0.999f;
    }

    const float detection_level = limiter->peak_hold;

    if (detection_level > limiter->envelope) {
        limiter->envelope =
            fmaf(limiter->envelope - detection_level, limiter->attack_coeff, detection_level);
    } else {
        limiter->envelope =
            fmaf(limiter->envelope - detection_level, limiter->release_coeff, detection_level);
    }

    float gain_reduction = 1.0f;
    if (limiter->envelope > limiter->threshold) {
        const float over_threshold = limiter->envelope - limiter->threshold;
        const float compressed_over = over_threshold / limiter->ratio;
        const float target_level = limiter->threshold + compressed_over;
        gain_reduction = target_level / limiter->envelope;
        if (gain_reduction * limiter->envelope > limiter->threshold) {
            gain_reduction = limiter->threshold / limiter->envelope;
        }
    }

    limiter->target_gain = gain_reduction;
    const float gain_smooth_coeff = 0.99f;
    limiter->smooth_gain =
        fmaf(limiter->smooth_gain - limiter->target_gain, gain_smooth_coeff, limiter->target_gain);

    const float final_gain = limiter->smooth_gain * limiter->makeup_gain;

    *output_l = fmaxf(-0.999f, fminf(0.999f, delayed_l * final_gain));
    *output_r = fmaxf(-0.999f, fminf(0.999f, delayed_r * final_gain));
}

static inline void _SituationToneSynthSumLimiterFree(SituationToneSynthSumLimiter* limiter) {
    if (limiter->delay_line_l) {
        SIT_FREE(limiter->delay_line_l);
        limiter->delay_line_l = NULL;
    }
    if (limiter->delay_line_r) {
        SIT_FREE(limiter->delay_line_r);
        limiter->delay_line_r = NULL;
    }
    limiter->buffer_capacity = 0;
    limiter->initialized = false;
}

static inline bool _SituationToneSynthSumLimiterAlloc(SituationToneSynthSumLimiter* limiter,
                                                      float sample_rate) {
    _SituationToneSynthSumLimiterFree(limiter);
    int limiter_buf_size = (int)(sample_rate * 0.002f);
    if (limiter_buf_size < 16) {
        limiter_buf_size = 16;
    }
    limiter->buffer_capacity = limiter_buf_size;
    limiter->delay_line_l = (float*)SIT_CALLOC((size_t)limiter_buf_size, sizeof(float));
    limiter->delay_line_r = (float*)SIT_CALLOC((size_t)limiter_buf_size, sizeof(float));
    if (!limiter->delay_line_l || !limiter->delay_line_r) {
        _SituationToneSynthSumLimiterFree(limiter);
        return false;
    }
    _SituationToneSynthSumLimiterInit(limiter, sample_rate,
                                     SITUATION_TONE_SYNTH_SUM_LIMITER_THRESHOLD,
                                     SITUATION_TONE_SYNTH_SUM_LIMITER_RELEASE_MS);
    return limiter->initialized;
}

typedef enum SituationToneSynthLfoWaveform {
    SIT_TONE_LFO_TRIANGLE = 0,
    SIT_TONE_LFO_SQUARE = 1,
    SIT_TONE_LFO_RANDOM = 2
} SituationToneSynthLfoWaveform;

typedef struct SituationToneSynthVoiceFilter {
    SituationFilter filter;
    int mode_cache;
    int poles_cache;
    float cutoff_cache;
    float res_cache;
} SituationToneSynthVoiceFilter;

typedef enum SituationToneSynthEnvState {
    SIT_TONE_SYNTH_ENV_IDLE = 0,
    SIT_TONE_SYNTH_ENV_ATTACK,
    SIT_TONE_SYNTH_ENV_DECAY,
    SIT_TONE_SYNTH_ENV_SUSTAIN,
    SIT_TONE_SYNTH_ENV_RELEASE
} SituationToneSynthEnvState;

typedef struct SituationToneSynthVoice {
    uint8_t active;
    uint8_t note;
    uint8_t env_state;
    uint8_t release_pending; /* note-off while sustain pedal held */
    int waveform;
    int sub_waveform;
    float base_hz;       /* current pitch (glides toward target_hz in mono) */
    float target_hz;     /* MIDI note target frequency */
    float phase;
    float sub_phase;
    uint8_t main_cycle_pending; /* set when main phase wraps; sub hard-syncs if enabled */
    float env_level;     /* last envelope output (release start level) */
    float release_start_env;
    float volume_peak;
    float pan;
    uint64_t cursor_frames;
    uint64_t t_attack;
    uint64_t t_decay;
    uint64_t t_hold;
    uint64_t t_release;
    float level_sustain;
    float mod_lfo_phase;
    float mod_lfo_random;
    uint32_t mod_lfo_cycle;
    SituationToneSynthVoiceFilter vf;
} SituationToneSynthVoice;

/** Graph tone synth node — voice pool + MIDI globals + manual (non-MIDI) fallback. */
typedef struct SituationToneSynthNodeState {
    SituationToneSynthVoice voices[SITUATION_TONE_SYNTH_MAX_VOICES];

    /* MIDI globals (shared across voices) */
    float bend_semitones;
    float mod_depth_semitones;
    float ch_volume;
    float expression;
    uint8_t sustain_pedal;
    float lfo_phase;           /* CC1 vibrato (fixed 5 Hz) */
    float tremolo_depth;       /* CC92 amplitude LFO depth 0..1 */
    float tremolo_phase;

    /* Manual-path mod LFO + free-run template when lfo_reset_on_key is off */
    float mod_lfo_phase;
    float mod_lfo_random;
    uint32_t mod_lfo_cycle;

    /* Manual control path when no voices active (SituationSetControl / harness) */
    float frequency;
    float amplitude;
    float phase;
    float sub_phase;
    uint8_t main_cycle_pending;
    int waveform;

    /* Manual (non-MIDI voice) filter path */
    SituationToneSynthVoiceFilter manual_vf;
    uint8_t manual_filter_note;

    /* Post-sum master limiter (Polysonix EnhancedLimiter) */
    SituationToneSynthSumLimiter sum_limiter;
    float sum_limiter_sample_rate;

    /* Per-node patch memory: 16 slots × snapshot of controls PATCH_PARAM_FIRST..LAST */
    struct {
        float param[SITUATION_TONE_SYNTH_PATCH_PARAM_COUNT];
        uint8_t in_use;
    } patch_slots[SITUATION_TONE_SYNTH_PATCH_SLOT_COUNT];
    uint8_t patch_slot;
    uint8_t patch_last_cc_slot;
    uint8_t patch_last_cc_store;
} SituationToneSynthNodeState;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(
    sizeof(((SituationToneSynthNodeState *)0)->patch_slots[0].param) ==
        sizeof(float) * (size_t)SITUATION_TONE_SYNTH_PATCH_PARAM_COUNT,
    "Tone synth patch_slots[].param size must match SITUATION_TONE_SYNTH_PATCH_PARAM_COUNT");
#endif

static inline void _SituationToneSynthSumLimiterEnsure(SituationToneSynthNodeState* s,
                                                         float sample_rate) {
    if (sample_rate <= 0.0f) {
        sample_rate = 48000.0f;
    }
    if (s->sum_limiter_sample_rate != sample_rate || !s->sum_limiter.initialized) {
        _SituationToneSynthSumLimiterAlloc(&s->sum_limiter, sample_rate);
        s->sum_limiter_sample_rate = sample_rate;
    }
}

typedef struct SituationToneSynthMidiCtx {
    float* controls;
    SituationToneSynthNodeState* synth;
} SituationToneSynthMidiCtx;

static inline float _SituationToneSynthMidiCombinedVolume(const SituationToneSynthNodeState* s) {
    if (!s) return 0.0f;
    return s->ch_volume * s->expression;
}

static inline int _SituationToneSynthAnyVoiceActive(const SituationToneSynthNodeState* s) {
    if (!s) return 0;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        if (s->voices[i].active) return 1;
    }
    return 0;
}

static inline float _SituationToneSynthVoicePortamentoTauSec(const SituationToneSynthVoice* v,
                                                           float portamento_time_sec,
                                                           float portamento_speed_st_per_sec) {
    if (!v) return 0.0f;
    if (portamento_time_sec <= 0.0f && portamento_speed_st_per_sec <= 0.0f) return 0.0f;

    float tau = portamento_time_sec;
    if (portamento_speed_st_per_sec > 0.0f && v->base_hz > 0.0f && v->target_hz > 0.0f) {
        float ratio = v->target_hz / v->base_hz;
        if (ratio < 1.0f) ratio = 1.0f / ratio;
        float semis = 12.0f * log2f(ratio);
        float speed_tau = semis / portamento_speed_st_per_sec;
        if (tau <= 0.0f) {
            tau = speed_tau;
        } else {
            /* Slower glide wins: speed sets a minimum glide time for the interval. */
            if (speed_tau > tau) tau = speed_tau;
        }
    }
    return tau;
}

static inline void _SituationToneSynthVoiceGlidePitch(SituationToneSynthVoice* v,
                                                      float portamento_time_sec,
                                                      float portamento_speed_st_per_sec,
                                                      float sample_rate) {
    if (!v || sample_rate <= 0.0f) return;
    float tau = _SituationToneSynthVoicePortamentoTauSec(v, portamento_time_sec,
                                                       portamento_speed_st_per_sec);
    if (tau <= 0.0f) {
        v->base_hz = v->target_hz;
        return;
    }
    if (fabsf(v->base_hz - v->target_hz) < 0.01f) {
        v->base_hz = v->target_hz;
        return;
    }
    if (tau < 0.001f) tau = 0.001f;
    float coeff = 1.0f - expf(-1.0f / (tau * sample_rate));
    v->base_hz = fmaf(v->target_hz - v->base_hz, coeff, v->base_hz);
}

static inline float _SituationToneSynthVoiceFrequency(const SituationToneSynthNodeState* s,
                                                      const SituationToneSynthVoice* v,
                                                      float vibrato_phase,
                                                      float mod_lfo_pitch_semitones) {
    float semis = s->bend_semitones + mod_lfo_pitch_semitones;
    if (s->mod_depth_semitones > 0.0f) {
        semis = fmaf(sinf(vibrato_phase), s->mod_depth_semitones, semis);
    }
    return v->base_hz * powf(2.0f, semis / 12.0f);
}

static inline float _SituationToneSynthClampPulseWidth(float pulse_width) {
    if (pulse_width < 0.05f) return 0.05f;
    if (pulse_width > 0.95f) return 0.95f;
    return pulse_width;
}

static inline float _SituationToneSynthModLfoRandomSample(uint32_t* cycle) {
    if (!cycle) return 0.0f;
    (*cycle)++;
    float u = sinf(fmaf((float)(*cycle), 12.9898f, 78.233f)) * 43758.5453f;
    return fmaf(u - floorf(u), 2.0f, -1.0f);
}

static inline float _SituationToneSynthModLfoWaveform(float phase, int waveform, float random_val) {
    if (waveform == SIT_TONE_LFO_SQUARE) {
        return (phase < SIT_TONE_PI) ? 1.0f : -1.0f;
    }
    if (waveform == SIT_TONE_LFO_RANDOM) {
        return random_val;
    }
  /* triangle */
    if (phase < SIT_TONE_PI) {
        return fmaf(phase / SIT_TONE_PI, 2.0f, -1.0f);
    }
    return fmaf(-(phase / SIT_TONE_PI), 2.0f, 3.0f);
}

static inline void _SituationToneSynthModLfoAdvancePhase(float* phase, float* random_val,
                                                        uint32_t* cycle, const float* controls,
                                                        float sample_rate) {
    if (!phase || !random_val || !cycle || !controls || sample_rate <= 0.0f) return;
    float rate = controls[SIT_TONE_CTRL_LFO_RATE];
    if (rate <= 0.0f) return;

    float prev = *phase;
    *phase += SIT_TONE_TWO_PI * rate / sample_rate;
    if (*phase >= SIT_TONE_TWO_PI) {
        *phase -= SIT_TONE_TWO_PI;
        if ((int)(controls[SIT_TONE_CTRL_LFO_WAVEFORM] + 0.5f) == SIT_TONE_LFO_RANDOM) {
            *random_val = _SituationToneSynthModLfoRandomSample(cycle);
        }
    } else if (prev <= 0.0f && *phase > 0.0f &&
               (int)(controls[SIT_TONE_CTRL_LFO_WAVEFORM] + 0.5f) == SIT_TONE_LFO_RANDOM &&
               *cycle == 0) {
        *random_val = _SituationToneSynthModLfoRandomSample(cycle);
    }
}

static inline void _SituationToneSynthModLfoAdvance(SituationToneSynthNodeState* s,
                                                    const float* controls,
                                                    float sample_rate) {
    if (!s) return;
    _SituationToneSynthModLfoAdvancePhase(&s->mod_lfo_phase, &s->mod_lfo_random, &s->mod_lfo_cycle,
                                          controls, sample_rate);
}

static inline void _SituationToneSynthModLfoAdvanceVoice(SituationToneSynthVoice* v,
                                                         const float* controls,
                                                         float sample_rate) {
    if (!v) return;
    _SituationToneSynthModLfoAdvancePhase(&v->mod_lfo_phase, &v->mod_lfo_random, &v->mod_lfo_cycle,
                                          controls, sample_rate);
}

static inline float _SituationToneSynthModLfoValueFromPhase(const float* controls, float phase,
                                                            float random_val) {
    if (!controls || controls[SIT_TONE_CTRL_LFO_RATE] <= 0.0f) return 0.0f;
    int wf = (int)(controls[SIT_TONE_CTRL_LFO_WAVEFORM] + 0.5f);
    if (wf < SIT_TONE_LFO_TRIANGLE) wf = SIT_TONE_LFO_TRIANGLE;
    if (wf > SIT_TONE_LFO_RANDOM) wf = SIT_TONE_LFO_RANDOM;
    return _SituationToneSynthModLfoWaveform(phase, wf, random_val);
}

static inline float _SituationToneSynthModLfoValue(const SituationToneSynthNodeState* s,
                                                   const float* controls) {
    if (!s) return 0.0f;
    return _SituationToneSynthModLfoValueFromPhase(controls, s->mod_lfo_phase, s->mod_lfo_random);
}

static inline float _SituationToneSynthModLfoValueVoice(const SituationToneSynthVoice* v,
                                                        const float* controls) {
    if (!v) return 0.0f;
    return _SituationToneSynthModLfoValueFromPhase(controls, v->mod_lfo_phase, v->mod_lfo_random);
}

static inline float _SituationToneSynthModLfoStartPhaseRad(const float* controls) {
    float p = controls ? controls[SIT_TONE_CTRL_LFO_START_PHASE] : 0.0f;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return p * SIT_TONE_TWO_PI;
}

static inline void _SituationToneSynthModLfoPrimePhase(float* phase, float* random_val,
                                                       uint32_t* cycle, const float* controls) {
    if (!phase || !random_val || !cycle) return;
    *phase = _SituationToneSynthModLfoStartPhaseRad(controls);
    *cycle = 0;
    int wf = controls ? (int)(controls[SIT_TONE_CTRL_LFO_WAVEFORM] + 0.5f) : SIT_TONE_LFO_TRIANGLE;
    if (wf < SIT_TONE_LFO_TRIANGLE) wf = SIT_TONE_LFO_TRIANGLE;
    if (wf > SIT_TONE_LFO_RANDOM) wf = SIT_TONE_LFO_RANDOM;
    if (wf == SIT_TONE_LFO_RANDOM) {
        *random_val = _SituationToneSynthModLfoRandomSample(cycle);
    } else {
        *random_val = 0.0f;
    }
}

static inline void _SituationToneSynthVoiceModLfoOnNoteOn(SituationToneSynthVoice* v,
                                                          SituationToneSynthNodeState* s,
                                                          const float* controls,
                                                          int fresh_voice_init) {
    if (!v) return;
    const int reset_on_key =
        !controls || controls[SIT_TONE_CTRL_LFO_RESET_ON_KEY] >= 0.5f;
    if (reset_on_key) {
        _SituationToneSynthModLfoPrimePhase(&v->mod_lfo_phase, &v->mod_lfo_random,
                                              &v->mod_lfo_cycle, controls);
    } else if (fresh_voice_init && s) {
        v->mod_lfo_phase = s->mod_lfo_phase;
        v->mod_lfo_random = s->mod_lfo_random;
        v->mod_lfo_cycle = s->mod_lfo_cycle;
    }
}

static inline float _SituationToneSynthModLfoPitchSemis(const float* controls, float lfo_val) {
    if (!controls || lfo_val == 0.0f) return 0.0f;
    float amt = controls[SIT_TONE_CTRL_LFO_PITCH_AMOUNT];
    float range = controls[SIT_TONE_CTRL_LFO_PITCH_RANGE];
    if (amt <= 0.0f || range <= 0.0f) return 0.0f;
    return lfo_val * amt * range;
}

static inline float _SituationToneSynthModLfoPulseWidth(const float* controls, float lfo_val,
                                                      float base_pw) {
    if (!controls || lfo_val == 0.0f) return base_pw;
    float amt = controls[SIT_TONE_CTRL_LFO_PWM_AMOUNT];
    float range = controls[SIT_TONE_CTRL_LFO_PWM_RANGE];
    if (amt <= 0.0f || range <= 0.0f) return base_pw;
    return _SituationToneSynthClampPulseWidth(fmaf(lfo_val * amt, range, base_pw));
}

static inline float _SituationToneSynthModLfoFilterCutoffOffset(const float* controls, float lfo_val) {
    if (!controls || lfo_val == 0.0f) return 0.0f;
    float amt = controls[SIT_TONE_CTRL_LFO_FILTER_AMOUNT];
    float range = controls[SIT_TONE_CTRL_LFO_FILTER_RANGE];
    if (amt <= 0.0f || range <= 0.0f) return 0.0f;
    return lfo_val * amt * range;
}

static inline float _SituationToneSynthFilterEnvCutoffOffset(const float* controls, float envelope) {
    if (!controls || envelope <= 0.0f) return 0.0f;
    float amt = controls[SIT_TONE_CTRL_FILTER_ENV_AMOUNT];
    float range = controls[SIT_TONE_CTRL_FILTER_ENV_RANGE];
    if (amt <= 0.0f || range <= 0.0f) return 0.0f;
    return envelope * amt * range;
}

static inline int _SituationToneSynthClampWaveform(int wf) {
    if (wf < 0) return 0;
    if (wf > 4) return 4;
    return wf;
}

/** Normalized phase in [0, 1) for polyBLEP. */
static inline float _SituationToneSynthPhaseNorm01(float phase) {
    float p = phase * (1.0f / SIT_TONE_TWO_PI);
    p -= floorf(p);
    if (p < 0.0f) {
        p += 1.0f;
    }
    return p;
}

/** Cycles advanced per sample (fraction of one period). */
static inline float _SituationToneSynthPhaseIncNorm(float freq_hz, float sample_rate) {
    if (sample_rate <= 0.0f || freq_hz <= 0.0f) {
        return 0.0f;
    }
    float dt = freq_hz / sample_rate;
    if (dt >= 1.0f) {
        return 0.999f;
    }
    return dt;
}

/* Polynomial band-limited step (Finke / Tolerance). t, dt in [0, 1) cycles. */
static inline float _SituationToneSynthPolyBlep(float t, float dt) {
    if (dt <= 0.0f || dt >= 1.0f) {
        return 0.0f;
    }
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/* Integrated polyBLEP for triangle slope discontinuities. */
static inline float _SituationToneSynthPolyBlamp(float t, float dt) {
    if (dt <= 0.0f || dt >= 1.0f) {
        return 0.0f;
    }
    if (t < dt) {
        t /= dt;
        return (t * t * t) * (1.0f / 3.0f) - 0.5f * t * t;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return (t * t * t) * (1.0f / 3.0f) + 0.5f * t * t;
    }
    return 0.0f;
}

static inline float _SituationToneSynthOscSampleWave(int waveform,
                                                     float phase,
                                                     uint8_t note,
                                                     float freq_hz,
                                                     float sample_rate,
                                                     float pulse_width) {
    float sample = 0.0f;
    const float dt = _SituationToneSynthPhaseIncNorm(freq_hz, sample_rate);
    const float p = _SituationToneSynthPhaseNorm01(phase);

    switch (waveform) {
        case 0:
            sample = sinf(phase);
            break;
        case 1: {
            const float pw = _SituationToneSynthClampPulseWidth(pulse_width);
            sample = (p < pw) ? 1.0f : -1.0f;
            sample += _SituationToneSynthPolyBlep(p, dt);
            float t2 = p + 1.0f - pw;
            if (t2 >= 1.0f) {
                t2 -= 1.0f;
            }
            sample -= _SituationToneSynthPolyBlep(t2, dt);
        } break;
        case 2: {
            sample = 1.0f - 4.0f * fabsf(p - floorf(p + 0.5f));
            sample += 4.0f * dt * _SituationToneSynthPolyBlamp(p, dt);
        } break;
        case 3:
            sample = 2.0f * p - 1.0f;
            sample -= _SituationToneSynthPolyBlep(p, dt);
            break;
        case 4: {
            float u = (sinf(fmaf(phase, 12.9898f, freq_hz * 0.001f)) + (float)note * 0.17f) *
                      43758.5453f;
            sample = fmaf(u - floorf(u), 2.0f, -1.0f);
        } break;
        default:
            sample = sinf(phase);
            break;
    }
    return sample;
}

static inline float _SituationToneSynthOscSample(const SituationToneSynthVoice* v,
                                                 float freq_hz,
                                                 float sample_rate,
                                                 float pulse_width) {
    return _SituationToneSynthOscSampleWave(v->waveform, v->phase, v->note, freq_hz, sample_rate,
                                            pulse_width);
}

static inline int _SituationToneSynthCtrlOn(const float* controls, int idx) {
    return controls && controls[idx] > 0.5f;
}

static inline float _SituationToneSynthClampSubCoarse(float coarse) {
    if (coarse < -SIT_TONE_SUB_COARSE_SEMITONE_MAX) return -SIT_TONE_SUB_COARSE_SEMITONE_MAX;
    if (coarse > SIT_TONE_SUB_COARSE_SEMITONE_MAX) return SIT_TONE_SUB_COARSE_SEMITONE_MAX;
    return coarse;
}

static inline uint8_t _SituationToneSynthFreqToMidiNote(float hz) {
    if (hz <= 0.0f) {
        return 60;
    }
    float n = fmaf(12.0f, log2f(hz / 440.0f), 69.0f);
    if (n < 0.0f) {
        n = 0.0f;
    }
    if (n > 127.0f) {
        n = 127.0f;
    }
    return (uint8_t)(n + 0.5f);
}

/** Sub MIDI note for waveform noise seed: main note + coarse / octave / fine displacement. */
static inline uint8_t _SituationToneSynthSubMidiNote(uint8_t main_note, const float* controls) {
    if (!controls) return main_note;
    const float coarse = _SituationToneSynthClampSubCoarse(controls[SIT_TONE_CTRL_SUB_COARSE]);
    int oct = (int)(controls[SIT_TONE_CTRL_SUB_OCTAVE] + 0.5f);
    if (oct < 0) oct = 0;
    if (oct > 2) oct = 2;
    float fine = controls[SIT_TONE_CTRL_SUB_FINE];
    if (fine < -1.0f) fine = -1.0f;
    if (fine > 1.0f) fine = 1.0f;
    const float total_st = coarse - (float)oct * 12.0f + fine;
    int note = (int)main_note + (int)(total_st + (total_st >= 0.0f ? 0.5f : -0.5f));
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    return (uint8_t)note;
}

/** sub_coarse / sub_octave / sub_fine interval applied to main voice Hz. */
static inline float _SituationToneSynthApplySubInterval(float base_hz, const float* controls) {
    if (!controls || base_hz <= 0.0f) return 0.0f;
    int oct = (int)(controls[SIT_TONE_CTRL_SUB_OCTAVE] + 0.5f);
    if (oct < 0) oct = 0;
    if (oct > 2) oct = 2;
    float fine = controls[SIT_TONE_CTRL_SUB_FINE];
    if (fine < -1.0f) fine = -1.0f;
    if (fine > 1.0f) fine = 1.0f;
    const float coarse = _SituationToneSynthClampSubCoarse(controls[SIT_TONE_CTRL_SUB_COARSE]);
    return base_hz * powf(2.0f, (coarse - (float)oct * 12.0f + fine) / 12.0f);
}

/** Hard-sync slave ratio: coarse+fine semitones from main (octave ignored — classic 0.5×…2× sweep). */
static inline float _SituationToneSynthSyncSubFrequencyHz(float main_hz, const float* controls) {
    if (!controls || main_hz <= 0.0f) {
        return 0.0f;
    }
    const float coarse = _SituationToneSynthClampSubCoarse(controls[SIT_TONE_CTRL_SUB_COARSE]);
    float fine = controls[SIT_TONE_CTRL_SUB_FINE];
    if (fine < -1.0f) {
        fine = -1.0f;
    }
    if (fine > 1.0f) {
        fine = 1.0f;
    }
    return main_hz * powf(2.0f, (coarse + fine) / 12.0f);
}

/** Sub pitch: main_hz displaced by sub_coarse, sub_octave, and sub_fine (sync uses ratio-only path). */
static inline float _SituationToneSynthSubFrequencyHz(float main_hz, uint8_t main_note,
                                                      const float* controls) {
    (void)main_note;
    if (!controls || main_hz <= 0.0f) {
        return 0.0f;
    }
    const int sync_on =
        _SituationToneSynthCtrlOn(controls, SIT_TONE_CTRL_SUB_SYNC) &&
        !_SituationToneSynthCtrlOn(controls, SIT_TONE_CTRL_SUB_RING_MOD);
    if (sync_on) {
        return _SituationToneSynthSyncSubFrequencyHz(main_hz, controls);
    }
    return _SituationToneSynthApplySubInterval(main_hz, controls);
}

/**
 * Main + sub oscillators (pre-filter). Advances main/sub phase; optional sync / ring mod.
 * Sync: main = master; each main cycle resets sub phase. Slave Hz = main × 2^((coarse+fine)/12)
 *   (octave ignored — CC111 sweeps classic 0.5×…2× ratios). Mix favors the synced sub oscillator.
 * Ring mod (four-quadrant multiply, dry/wet crossfade):
 *   sample = main×(1−ring_level) + (main×sub)×ring_level.
 *   At ring_level=1 the carrier is fully suppressed — only sum/difference sidebands remain.
 *   CC113 enables ring; CC107 = ring depth / dry-wet (defaults to 1 when ring on and level is 0).
 *   Coarse/oct/fine retune the modulator only — not additive sub while ring is on.
 *   (SID-style ring uses triangle MSB XOR and is not emulated here.)
 * @param main_cycle_pending In/out: set when main completes a cycle.
 */
static inline float _SituationToneSynthMixMainSub(
    int main_waveform,
    int sub_waveform,
    float* main_phase,
    float* sub_phase,
    uint8_t* main_cycle_pending,
    uint8_t main_note,
    float main_hz,
    float sample_rate,
    float pulse_width,
    const float* controls) {
    const float sub_level = controls ? controls[SIT_TONE_CTRL_SUB_LEVEL] : 0.0f;
    const int sync_on =
        _SituationToneSynthCtrlOn(controls, SIT_TONE_CTRL_SUB_SYNC) &&
        !_SituationToneSynthCtrlOn(controls, SIT_TONE_CTRL_SUB_RING_MOD);
    const int ring_mod = _SituationToneSynthCtrlOn(controls, SIT_TONE_CTRL_SUB_RING_MOD);
    const int run_sub =
        controls && sub_phase && (sub_level > 0.0f || sync_on || ring_mod);
    const float main_inc = (SIT_TONE_TWO_PI * main_hz) / sample_rate;

    float sub_s = 0.0f;
    int sub_active = 0;
    float mod_hz = 0.0f;

    if (run_sub) {
        mod_hz = _SituationToneSynthSubFrequencyHz(main_hz, main_note, controls);
        if (mod_hz > 0.0f) {
            sub_active = 1;
            const float mod_inc = (SIT_TONE_TWO_PI * mod_hz) / sample_rate;
            const float dt_main = main_inc / SIT_TONE_TWO_PI;
            const float p_main = _SituationToneSynthPhaseNorm01(*main_phase);
            const int master_wrap = (*main_phase + main_inc >= SIT_TONE_TWO_PI);
            const uint8_t sub_midi_note = sync_on
                ? _SituationToneSynthFreqToMidiNote(mod_hz)
                : _SituationToneSynthSubMidiNote(main_note, controls);
            /* Ring mod: if sub waveform is default sine (0) and main is richer,
             * inherit main waveform so the product has dense sidebands. */
            const int effective_sub_wf = (ring_mod && sub_waveform == 0 && main_waveform != 0)
                ? main_waveform : sub_waveform;

            if (sync_on && master_wrap) {
                const float at_reset = _SituationToneSynthOscSampleWave(
                    effective_sub_wf, 0.0f, sub_midi_note, mod_hz, sample_rate, pulse_width);
                const float at_cur = _SituationToneSynthOscSampleWave(
                    effective_sub_wf, *sub_phase, sub_midi_note, mod_hz, sample_rate, pulse_width);
                const float sync_disc = at_reset - at_cur;
                *sub_phase = 0.0f;
                sub_s = at_reset + sync_disc * _SituationToneSynthPolyBlep(p_main, dt_main);
            } else {
                if (sync_on && main_cycle_pending && *main_cycle_pending) {
                    *sub_phase = 0.0f;
                    *main_cycle_pending = 0;
                }
                sub_s = _SituationToneSynthOscSampleWave(
                    effective_sub_wf, *sub_phase, sub_midi_note, mod_hz, sample_rate, pulse_width);
                *sub_phase += mod_inc;
                if (*sub_phase >= SIT_TONE_TWO_PI) {
                    *sub_phase -= SIT_TONE_TWO_PI;
                }
            }
        }
    }

    float main_s = _SituationToneSynthOscSampleWave(
        main_waveform, *main_phase, main_note, main_hz, sample_rate, pulse_width);

    float sample = main_s;
    if (ring_mod && sub_active) {
        float ring_level = sub_level;
        if (ring_level <= 0.0f) {
            ring_level = 1.0f;
        }
        /* True ring mod: dry/wet crossfade — at level=1 carrier suppressed, only sidebands.
         * The 2× on the product compensates for the sin×sin → 0.5×cos identity so that
         * ring output has the same peak amplitude as the dry carrier. */
        sample = fmaf(2.0f * main_s * sub_s, ring_level, main_s * (1.0f - ring_level));
    } else if (sync_on && sub_active) {
        float main_mix = 1.0f - sub_level;
        if (main_mix < 0.0f) {
            main_mix = 0.0f;
        }
        if (main_mix > 1.0f) {
            main_mix = 1.0f;
        }
        sample = fmaf(main_mix, main_s, sub_s);
    } else if (sub_level > 0.0f && sub_active && sub_s != 0.0f) {
        sample = fmaf(sub_level, sub_s, main_s);
    }

    *main_phase += main_inc;
    if (*main_phase >= SIT_TONE_TWO_PI) {
        *main_phase -= SIT_TONE_TWO_PI;
        if (main_cycle_pending) {
            *main_cycle_pending = 1;
        }
    }

    return sample;
}

static inline void _SituationToneSynthVoiceSubFromControls(SituationToneSynthVoice* v,
                                                         const float* controls) {
    if (!v) return;
    v->sub_waveform = _SituationToneSynthClampWaveform(
        controls ? (int)(controls[SIT_TONE_CTRL_SUB_WAVEFORM] + 0.5f) : 0);
}

static inline uint8_t _SituationToneSynthMidiCcClamp(uint8_t value) {
    return value > 127 ? (uint8_t)127 : value;
}

/** Map MIDI CC byte 0..127 → integer index 0..(steps−1) (integer math only). */
static inline int _SituationToneSynthMidiCcSteps(uint8_t value, int steps) {
    if (steps <= 1) return 0;
    return (int)((unsigned)value * (unsigned)(steps - 1) + 63u) / 127u;
}

static inline float _SituationToneSynthMidiNormLog(uint8_t value, float min_hz, float max_hz) {
    float norm = (float)_SituationToneSynthMidiCcClamp(value) / 127.0f;
    float log_min = logf(min_hz);
    float log_max = logf(max_hz);
    return expf(fmaf(norm, log_max - log_min, log_min));
}

static inline int _SituationToneSynthFilterEnabled(const float* controls) {
    if (!controls) return 0;
    int mode = (int)(controls[SIT_TONE_CTRL_FILTER_MODE] + 0.5f);
    return mode != PX_FILTER_MODE_OFF && mode >= PX_FILTER_MODE_LP && mode <= PX_FILTER_MODE_BP_HP;
}

static inline float _SituationToneSynthFilterKeytrackFactor(uint8_t note, float keytrack) {
    if (keytrack <= 0.0001f) return 1.0f;
    return exp2f(((float)note - 60.0f) / 12.0f * keytrack);
}

static inline void _SituationToneSynthVoiceFilterEnsure(SituationToneSynthVoiceFilter* vf, float sample_rate) {
    if (!vf || sample_rate <= 0.0f) return;
    if (vf->filter.sample_rate <= 0.0f) {
        filter_init(&vf->filter, sample_rate);
        vf->mode_cache = -1;
        vf->poles_cache = -1;
        vf->cutoff_cache = -1.0f;
        vf->res_cache = -1.0f;
    }
}

/** Force per-voice SVF to pick up live filter control changes (Q, cutoff, mode, …). */
static inline void _SituationToneSynthInvalidateVoiceFilterCaches(SituationToneSynthNodeState* s) {
    if (!s) return;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        s->voices[i].vf.mode_cache = -1;
        s->voices[i].vf.poles_cache = -1;
        s->voices[i].vf.cutoff_cache = -1.0f;
        s->voices[i].vf.res_cache = -1.0f;
    }
    s->manual_vf.mode_cache = -1;
    s->manual_vf.poles_cache = -1;
    s->manual_vf.cutoff_cache = -1.0f;
    s->manual_vf.res_cache = -1.0f;
}

static inline void _SituationToneSynthVoiceFilterUpdate(SituationToneSynthVoiceFilter* vf,
                                                        const float* controls,
                                                        uint8_t note,
                                                        float sample_rate,
                                                        float cutoff_offset_hz) {
    if (!vf || !controls || sample_rate <= 0.0f) return;
    _SituationToneSynthVoiceFilterEnsure(vf, sample_rate);

    int mode = (int)(controls[SIT_TONE_CTRL_FILTER_MODE] + 0.5f);
    if (mode < PX_FILTER_MODE_OFF) mode = PX_FILTER_MODE_OFF;
    if (mode > PX_FILTER_MODE_BP_HP) mode = PX_FILTER_MODE_BP_HP;
    if (mode == PX_FILTER_MODE_OFF) return;

    int poles = (int)(controls[SIT_TONE_CTRL_FILTER_POLES] + 0.5f);
    if (poles < 1) poles = 1;
    if (poles > 4) poles = 4;

    float cutoff = controls[SIT_TONE_CTRL_FILTER_CUTOFF] + cutoff_offset_hz;
    float q = controls[SIT_TONE_CTRL_FILTER_RESONANCE];
    if (q < 0.5f) q = 0.5f;
    if (q > 20.0f) q = 20.0f;
    cutoff *= _SituationToneSynthFilterKeytrackFactor(note, controls[SIT_TONE_CTRL_FILTER_KEYTRACK]);
    if (cutoff < 20.0f) cutoff = 20.0f;
    {
        float max_safe = sample_rate * 0.45f;
        if (cutoff > max_safe) cutoff = max_safe;
    }

    if (mode != vf->mode_cache || poles != vf->poles_cache ||
        cutoff != vf->cutoff_cache || q != vf->res_cache) {
        filter_set_coefficients(&vf->filter, cutoff, q, (PxFilterMode)mode, poles);
        vf->mode_cache = mode;
        vf->poles_cache = poles;
        vf->cutoff_cache = cutoff;
        vf->res_cache = q;
    }

    vf->filter.drive = controls[SIT_TONE_CTRL_FILTER_DRIVE];
    int os = (int)(controls[SIT_TONE_CTRL_FILTER_OVERSAMPLE] + 0.5f);
    vf->filter.use_oversampling = (os >= 1);
}

static inline float _SituationToneSynthVoiceFilterProcess(SituationToneSynthVoiceFilter* vf,
                                                          const float* controls,
                                                          uint8_t note,
                                                          float sample,
                                                          float sample_rate,
                                                          float cutoff_offset_hz,
                                                          float amp) {
    if (!_SituationToneSynthFilterEnabled(controls)) return sample;
    _SituationToneSynthVoiceFilterUpdate(vf, controls, note, sample_rate, cutoff_offset_hz);
    return filter_process_oversampled_amp(&vf->filter, sample, amp);
}

static inline void _SituationToneSynthVoicePrimeEnvelopeOnNoteOn(SituationToneSynthVoice* v) {
    if (!v) return;
    v->cursor_frames = 0;
    v->release_pending = 0;
    v->release_start_env = 0.0f;

    if (v->t_attack == 0) {
        if (v->t_decay == 0 || v->level_sustain >= SITUATION_TONE_SYNTH_SUSTAIN_FULL_THRESH) {
            v->env_state = SIT_TONE_SYNTH_ENV_SUSTAIN;
            v->env_level = v->level_sustain;
        } else {
            v->env_state = SIT_TONE_SYNTH_ENV_DECAY;
            v->env_level = 1.0f;
        }
    } else {
        v->env_state = SIT_TONE_SYNTH_ENV_ATTACK;
        v->env_level = 0.0f;
    }
}

static inline float _SituationToneSynthEnvStep(SituationToneSynthVoice* v) {
    float envelope = 0.0f;
    switch ((SituationToneSynthEnvState)v->env_state) {
        case SIT_TONE_SYNTH_ENV_ATTACK:
            envelope = (v->t_attack > 0) ? (float)v->cursor_frames / (float)v->t_attack : 1.0f;
            if (v->cursor_frames >= v->t_attack) {
                if (v->t_decay == 0 || v->level_sustain >= SITUATION_TONE_SYNTH_SUSTAIN_FULL_THRESH) {
                    v->env_state = SIT_TONE_SYNTH_ENV_SUSTAIN;
                    envelope = v->level_sustain;
                } else {
                    v->env_state = SIT_TONE_SYNTH_ENV_DECAY;
                    envelope = 1.0f;
                }
                v->cursor_frames = 0;
            }
            break;
        case SIT_TONE_SYNTH_ENV_DECAY: {
            float progress = (v->t_decay > 0) ? (float)v->cursor_frames / (float)v->t_decay : 1.0f;
            envelope = fmaf(-(1.0f - v->level_sustain), progress, 1.0f);
            if (v->cursor_frames >= v->t_decay) {
                v->env_state = SIT_TONE_SYNTH_ENV_SUSTAIN;
                v->cursor_frames = 0;
                envelope = v->level_sustain;
            }
        } break;
        case SIT_TONE_SYNTH_ENV_SUSTAIN:
            envelope = v->level_sustain;
            if (v->t_hold != UINT64_MAX && v->cursor_frames >= v->t_hold) {
                v->env_state = SIT_TONE_SYNTH_ENV_RELEASE;
                v->cursor_frames = 0;
                v->release_start_env = envelope;
            }
            break;
        case SIT_TONE_SYNTH_ENV_RELEASE: {
            if (v->t_release == 0) {
                envelope = 0.0f;
                v->active = 0;
                v->env_state = SIT_TONE_SYNTH_ENV_IDLE;
                break;
            }
            float progress = (float)v->cursor_frames / (float)v->t_release;
            if (progress >= 1.0f) {
                envelope = 0.0f;
                v->active = 0;
                v->env_state = SIT_TONE_SYNTH_ENV_IDLE;
            } else {
                /* Exponential tail: ~-60 dB at release time (classic synth RC curve). */
                envelope = v->release_start_env *
                    expf(SIT_TONE_SYNTH_ENV_RELEASE_LN_FLOOR * progress);
                if (envelope < SITUATION_TONE_SYNTH_ENV_RELEASE_FLOOR * v->release_start_env) {
                    envelope = 0.0f;
                    v->active = 0;
                    v->env_state = SIT_TONE_SYNTH_ENV_IDLE;
                }
            }
        } break;
        default:
            v->active = 0;
            break;
    }
    if (v->active) {
        v->cursor_frames++;
    }
    v->env_level = envelope;
    return envelope;
}

static inline void _SituationToneSynthVoiceInitFromControls(SituationToneSynthVoice* v,
                                                            const float* controls,
                                                            int sample_rate,
                                                            uint8_t note,
                                                            uint8_t velocity) {
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->note = note;
    float note_hz = SITUATION_MIDI_NOTE_FREQUENCY[note];
    v->base_hz = note_hz;
    v->target_hz = note_hz;
    v->volume_peak = (velocity > 0) ? ((float)velocity / 127.0f) : 0.0f;
    v->pan = controls ? controls[3] : 0.0f;
    v->waveform = _SituationToneSynthClampWaveform(controls ? (int)(controls[1] + 0.5f) : 0);
    _SituationToneSynthVoiceSubFromControls(v, controls);

    float attack_sec = controls ? controls[4] : 0.01f;
    float decay_sec = controls ? controls[5] : 0.1f;
    float sustain_lvl = controls ? controls[6] : 0.7f;
    float release_sec = controls ? controls[7] : 0.2f;
    float hold_sec = controls ? controls[8] : 1.0f;

    if (attack_sec < 0.0f) attack_sec = 0.0f;
    if (decay_sec < 0.0f) decay_sec = 0.0f;
    if (release_sec < 0.0f) release_sec = 0.0f;
    if (sustain_lvl < 0.0f) sustain_lvl = 0.0f;
    if (sustain_lvl > 1.0f) sustain_lvl = 1.0f;

    v->level_sustain = sustain_lvl;
    v->t_attack = (uint64_t)(attack_sec * (float)sample_rate);
    v->t_decay = (uint64_t)(decay_sec * (float)sample_rate);
    v->t_release = (uint64_t)(release_sec * (float)sample_rate);
    v->t_hold = (hold_sec < 0.0f) ? UINT64_MAX : (uint64_t)(hold_sec * (float)sample_rate);

    _SituationToneSynthVoiceFilterEnsure(&v->vf, (float)sample_rate);
    v->vf.filter.drive = controls ? controls[SIT_TONE_CTRL_FILTER_DRIVE] : 1.0f;
    v->vf.filter.use_oversampling =
        controls && ((int)(controls[SIT_TONE_CTRL_FILTER_OVERSAMPLE] + 0.5f) >= 1);

    _SituationToneSynthVoicePrimeEnvelopeOnNoteOn(v);
}

static inline void _SituationToneSynthVoiceApplyAdsrFromControls(SituationToneSynthVoice* v,
                                                                 const float* controls,
                                                                 int sample_rate) {
    if (!v || !controls || sample_rate <= 0) return;

    float attack_sec = controls[4];
    float decay_sec = controls[5];
    float sustain_lvl = controls[6];
    float release_sec = controls[7];
    float hold_sec = controls[8];

    if (attack_sec < 0.0f) attack_sec = 0.0f;
    if (decay_sec < 0.0f) decay_sec = 0.0f;
    if (release_sec < 0.0f) release_sec = 0.0f;
    if (sustain_lvl < 0.0f) sustain_lvl = 0.0f;
    if (sustain_lvl > 1.0f) sustain_lvl = 1.0f;

    v->level_sustain = sustain_lvl;
    v->t_attack = (uint64_t)(attack_sec * (float)sample_rate);
    v->t_decay = (uint64_t)(decay_sec * (float)sample_rate);
    v->t_release = (uint64_t)(release_sec * (float)sample_rate);
    v->t_hold = (hold_sec < 0.0f) ? UINT64_MAX : (uint64_t)(hold_sec * (float)sample_rate);
}

/** Mono legato: glide pitch, keep phase/filter; envelope stays at sustain when already there. */
static inline void _SituationToneSynthVoiceLegatoFromControls(SituationToneSynthVoice* v,
                                                              const float* controls,
                                                              int sample_rate,
                                                              uint8_t note,
                                                              uint8_t velocity) {
    if (!v || !controls || sample_rate <= 0) return;

    SituationToneSynthVoiceFilter vf_saved = v->vf;
    float phase_saved = v->phase;
    float sub_phase_saved = v->sub_phase;
    float base_hz_saved = v->base_hz;
    SituationToneSynthEnvState env_saved = (SituationToneSynthEnvState)v->env_state;
    float env_level_saved = v->env_level;
    uint64_t cursor_saved = v->cursor_frames;
    _SituationToneSynthVoiceApplyAdsrFromControls(v, controls, sample_rate);

    v->active = 1;
    v->note = note;
    v->target_hz = SITUATION_MIDI_NOTE_FREQUENCY[note];
    v->base_hz = base_hz_saved;
    v->phase = phase_saved;
    v->sub_phase = sub_phase_saved;
    v->vf = vf_saved;
    v->volume_peak = (velocity > 0) ? ((float)velocity / 127.0f) : 0.0f;
    v->pan = controls[3];
    v->waveform = _SituationToneSynthClampWaveform((int)(controls[1] + 0.5f));
    _SituationToneSynthVoiceSubFromControls(v, controls);
    v->release_pending = 0;

    if (env_saved == SIT_TONE_SYNTH_ENV_RELEASE) {
        _SituationToneSynthVoicePrimeEnvelopeOnNoteOn(v);
    } else if (env_saved == SIT_TONE_SYNTH_ENV_SUSTAIN) {
        v->env_state = SIT_TONE_SYNTH_ENV_SUSTAIN;
        v->env_level = v->level_sustain;
        v->cursor_frames = cursor_saved;
    } else {
        v->env_state = (uint8_t)env_saved;
        v->env_level = env_level_saved;
        v->cursor_frames = cursor_saved;
    }
}

static inline void _SituationToneSynthRefreshActiveVoiceAdsr(SituationToneSynthNodeState* s,
                                                             const float* controls,
                                                             int sample_rate) {
    if (!s || !controls || sample_rate <= 0) return;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        if (s->voices[i].active) {
            _SituationToneSynthVoiceApplyAdsrFromControls(&s->voices[i], controls, sample_rate);
        }
    }
}

static inline void _SituationToneSynthSetWaveform(SituationToneSynthMidiCtx* ctx, int wf) {
    if (!ctx || !ctx->controls || !ctx->synth) return;
    wf = _SituationToneSynthClampWaveform(wf);
    ctx->controls[1] = (float)wf;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        if (ctx->synth->voices[i].active) {
            ctx->synth->voices[i].waveform = wf;
        }
    }
}

static inline void _SituationToneSynthSetSubWaveform(SituationToneSynthMidiCtx* ctx, int wf) {
    if (!ctx || !ctx->controls || !ctx->synth) return;
    wf = _SituationToneSynthClampWaveform(wf);
    ctx->controls[SIT_TONE_CTRL_SUB_WAVEFORM] = (float)wf;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        if (ctx->synth->voices[i].active) {
            ctx->synth->voices[i].sub_waveform = wf;
        }
    }
}

static inline int _SituationToneSynthIsMono(const float* controls) {
    return controls && controls[SIT_TONE_CTRL_VOICE_MODE] >= 0.5f;
}

/** Mono pivot: keep the most recently triggered voice in slot 0, silence the rest. */
static inline void _SituationToneSynthEnforceMonoVoices(SituationToneSynthNodeState* s) {
    if (!s) return;

    int keep = -1;
    uint64_t newest = 0;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        SituationToneSynthVoice* v = &s->voices[i];
        if (!v->active) continue;
        if (keep < 0 || v->cursor_frames >= newest) {
            keep = i;
            newest = v->cursor_frames;
        }
    }

    SituationToneSynthVoice saved = {0};
    if (keep >= 0) {
        saved = s->voices[keep];
    }

    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        memset(&s->voices[i], 0, sizeof(s->voices[i]));
    }

    if (keep >= 0) {
        s->voices[0] = saved;
    }
}

static inline void _SituationToneSynthSetVoiceMode(SituationToneSynthMidiCtx* ctx, int mono) {
    if (!ctx || !ctx->controls || !ctx->synth) return;
    ctx->controls[SIT_TONE_CTRL_VOICE_MODE] = mono ? 1.0f : 0.0f;
    if (mono) {
        _SituationToneSynthEnforceMonoVoices(ctx->synth);
    }
}

static inline int _SituationToneSynthPatchSlotFromCc(uint8_t value) {
    return _SituationToneSynthMidiCcSteps(value, SITUATION_TONE_SYNTH_PATCH_SLOT_COUNT);
}

static inline void _SituationToneSynthPatchSyncVoicesFromControls(SituationToneSynthMidiCtx* ctx,
                                                                  int sample_rate) {
    if (!ctx || !ctx->controls || !ctx->synth || sample_rate <= 0) return;

    _SituationToneSynthSetWaveform(ctx, (int)(ctx->controls[SIT_TONE_CTRL_WAVEFORM] + 0.5f));
    _SituationToneSynthSetSubWaveform(ctx, (int)(ctx->controls[SIT_TONE_CTRL_SUB_WAVEFORM] + 0.5f));
    if (_SituationToneSynthIsMono(ctx->controls)) {
        _SituationToneSynthEnforceMonoVoices(ctx->synth);
    }
    _SituationToneSynthRefreshActiveVoiceAdsr(ctx->synth, ctx->controls, sample_rate);
    ctx->synth->waveform = (int)(ctx->controls[SIT_TONE_CTRL_WAVEFORM] + 0.5f);
}

static inline void _SituationToneSynthPatchSave(SituationToneSynthMidiCtx* ctx, int slot) {
    if (!ctx || !ctx->controls || !ctx->synth) return;
    if (slot < 0 || slot >= SITUATION_TONE_SYNTH_PATCH_SLOT_COUNT) return;

    memcpy(ctx->synth->patch_slots[slot].param,
           &ctx->controls[SITUATION_TONE_SYNTH_PATCH_PARAM_FIRST],
           sizeof(float) * (size_t)SITUATION_TONE_SYNTH_PATCH_PARAM_COUNT);
    ctx->synth->patch_slots[slot].in_use = 1;
    ctx->synth->patch_slot = (uint8_t)slot;
    if (ctx->controls) {
        ctx->controls[SIT_TONE_CTRL_PATCH_SLOT] = (float)slot;
    }
}

static inline void _SituationToneSynthPatchRecall(SituationToneSynthMidiCtx* ctx, int slot,
                                                  int sample_rate) {
    if (!ctx || !ctx->controls || !ctx->synth) return;
    if (slot < 0 || slot >= SITUATION_TONE_SYNTH_PATCH_SLOT_COUNT) return;
    if (!ctx->synth->patch_slots[slot].in_use) return;

    memcpy(&ctx->controls[SITUATION_TONE_SYNTH_PATCH_PARAM_FIRST],
           ctx->synth->patch_slots[slot].param,
           sizeof(float) * (size_t)SITUATION_TONE_SYNTH_PATCH_PARAM_COUNT);
    ctx->synth->patch_slot = (uint8_t)slot;
    ctx->controls[SIT_TONE_CTRL_PATCH_SLOT] = (float)slot;
    _SituationToneSynthPatchSyncVoicesFromControls(ctx, sample_rate);
}

static inline void _SituationToneSynthPatchHandleSlotCc(SituationToneSynthMidiCtx* ctx,
                                                        uint8_t value, int sample_rate) {
    if (!ctx || !ctx->synth) return;

    const int slot = _SituationToneSynthPatchSlotFromCc(value);
    ctx->controls[SIT_TONE_CTRL_PATCH_SLOT] = (float)slot;
    ctx->synth->patch_slot = (uint8_t)slot;

    if (value != ctx->synth->patch_last_cc_slot) {
        ctx->synth->patch_last_cc_slot = value;
        _SituationToneSynthPatchRecall(ctx, slot, sample_rate);
    }
}

static inline void _SituationToneSynthPatchHandleStoreCc(SituationToneSynthMidiCtx* ctx,
                                                         uint8_t value) {
    if (!ctx || !ctx->synth) return;

    const int save = (value >= 64) ? 1 : 0;
    const int rising = save && !ctx->synth->patch_last_cc_store;
    ctx->synth->patch_last_cc_store = (uint8_t)save;

    if (rising) {
        _SituationToneSynthPatchSave(ctx, (int)ctx->synth->patch_slot);
        ctx->controls[SIT_TONE_CTRL_PATCH_STORE] = 1.0f;
    } else if (!save) {
        ctx->controls[SIT_TONE_CTRL_PATCH_STORE] = 0.0f;
    }
}

static inline void _SituationToneSynthPatchHandleSetControl(SituationToneSynthMidiCtx* ctx,
                                                            uint32_t control_id, float value,
                                                            int sample_rate) {
    if (!ctx || !ctx->controls || !ctx->synth) return;

    if (control_id == (uint32_t)SIT_TONE_CTRL_PATCH_SLOT) {
        int slot = (int)(value + 0.5f);
        if (slot < 0) slot = 0;
        if (slot >= SITUATION_TONE_SYNTH_PATCH_SLOT_COUNT) {
            slot = SITUATION_TONE_SYNTH_PATCH_SLOT_COUNT - 1;
        }
        ctx->controls[SIT_TONE_CTRL_PATCH_SLOT] = (float)slot;
        ctx->synth->patch_slot = (uint8_t)slot;
        _SituationToneSynthPatchRecall(ctx, slot, sample_rate);
        return;
    }

    if (control_id == (uint32_t)SIT_TONE_CTRL_PATCH_STORE) {
        if (value >= 0.5f) {
            _SituationToneSynthPatchSave(ctx, (int)ctx->synth->patch_slot);
            ctx->controls[SIT_TONE_CTRL_PATCH_STORE] = 1.0f;
        } else {
            ctx->controls[SIT_TONE_CTRL_PATCH_STORE] = 0.0f;
        }
    }
}

static inline int _SituationToneSynthAllocVoice(SituationToneSynthNodeState* s, const float* controls) {
    if (!s) return -1;

    if (_SituationToneSynthIsMono(controls)) {
        for (int i = 1; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
            memset(&s->voices[i], 0, sizeof(s->voices[i]));
        }
        return 0;
    }

    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        if (!s->voices[i].active) return i;
    }

    int candidate = -1;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        if (s->voices[i].active && s->voices[i].env_state == SIT_TONE_SYNTH_ENV_RELEASE) {
            if (candidate < 0 || s->voices[i].cursor_frames > s->voices[candidate].cursor_frames) {
                candidate = i;
            }
        }
    }
    if (candidate >= 0) return candidate;

    uint64_t min_cursor = UINT64_MAX;
    candidate = -1;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        if (s->voices[i].active && s->voices[i].cursor_frames < min_cursor) {
            min_cursor = s->voices[i].cursor_frames;
            candidate = i;
        }
    }
    return candidate;
}

static inline void _SituationToneSynthVoiceRelease(SituationToneSynthVoice* v, uint8_t sustain_pedal) {
    if (!v || !v->active) return;
    if (sustain_pedal) {
        v->release_pending = 1;
        return;
    }
    v->release_pending = 0;
    v->release_start_env = v->env_level;
    v->env_state = SIT_TONE_SYNTH_ENV_RELEASE;
    v->cursor_frames = 0;
}

static inline void _SituationToneSynthReleaseNote(SituationToneSynthNodeState* s, uint8_t note) {
    if (!s) return;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        SituationToneSynthVoice* v = &s->voices[i];
        if (v->active && v->note == note) {
            _SituationToneSynthVoiceRelease(v, s->sustain_pedal);
        }
    }
}

static inline void _SituationToneSynthReleaseAll(SituationToneSynthNodeState* s) {
    if (!s) return;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        if (s->voices[i].active) {
            s->voices[i].release_pending = 0;
            s->voices[i].release_start_env = s->voices[i].env_level;
            s->voices[i].env_state = SIT_TONE_SYNTH_ENV_RELEASE;
            s->voices[i].cursor_frames = 0;
        }
    }
}

static inline void _SituationToneSynthOnSustainPedalUp(SituationToneSynthNodeState* s) {
    if (!s) return;
    for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
        SituationToneSynthVoice* v = &s->voices[i];
        if (v->active && v->release_pending) {
            _SituationToneSynthVoiceRelease(v, 0);
        }
    }
}

/**
 * MIDI CC → node controls. Wire values are always integers 0..127 (uint8_t).
 * Continuous parameters (time, Hz, level) map to float engineering units via norm;
 * discrete parameters (waveform, octaves, modes) use integer CC → integer index only.
 */
static inline void _SituationToneSynthApplyControlChange(SituationToneSynthMidiCtx* ctx,
                                                         uint8_t controller,
                                                         uint8_t value,
                                                         int sample_rate) {
    if (!ctx || !ctx->controls || !ctx->synth) return;
    value = _SituationToneSynthMidiCcClamp(value);
    float norm = (float)value / 127.0f;
    SituationToneSynthNodeState* s = ctx->synth;

    switch (controller) {
        case 1:
            s->mod_depth_semitones = norm * 1.0f;
            break;
        case 5:
            ctx->controls[SIT_TONE_CTRL_PORTAMENTO_TIME] = norm * 2.0f;
            break;
        case 20:
            ctx->controls[SIT_TONE_CTRL_PORTAMENTO_SPEED] = norm * 48.0f;
            break;
        case 7:
            s->ch_volume = norm;
            break;
        case 10:
            ctx->controls[3] = fmaf(norm, 2.0f, -1.0f);
            break;
        case 11:
            s->expression = norm;
            break;
        case 64:
            s->sustain_pedal = (value >= 64) ? 1 : 0;
            if (!s->sustain_pedal) {
                _SituationToneSynthOnSustainPedalUp(s);
            }
            break;
        case 70:
            _SituationToneSynthSetWaveform(ctx, (int)(value % 5));
            break;
        case 72:
            ctx->controls[7] = norm * 5.0f;
            _SituationToneSynthRefreshActiveVoiceAdsr(s, ctx->controls, sample_rate);
            break;
        case 73:
            ctx->controls[4] = norm * 2.0f;
            _SituationToneSynthRefreshActiveVoiceAdsr(s, ctx->controls, sample_rate);
            break;
        case 75:
            ctx->controls[5] = norm * 2.0f;
            _SituationToneSynthRefreshActiveVoiceAdsr(s, ctx->controls, sample_rate);
            break;
        case 76:
            ctx->controls[6] = norm;
            _SituationToneSynthRefreshActiveVoiceAdsr(s, ctx->controls, sample_rate);
            break;
        case 77:
            ctx->controls[8] = norm * 4.0f;
            _SituationToneSynthRefreshActiveVoiceAdsr(s, ctx->controls, sample_rate);
            break;
        case 92:
            s->tremolo_depth = norm;
            break;
        case 16:
            ctx->controls[SIT_TONE_CTRL_FILTER_MODE] =
                (float)_SituationToneSynthMidiCcSteps(value, 9);
            _SituationToneSynthInvalidateVoiceFilterCaches(s);
            break;
        case 17:
            ctx->controls[SIT_TONE_CTRL_FILTER_DRIVE] = fmaf(norm, 9.0f, 1.0f);
            _SituationToneSynthInvalidateVoiceFilterCaches(s);
            break;
        case 18:
            ctx->controls[SIT_TONE_CTRL_FILTER_OVERSAMPLE] =
                (float)_SituationToneSynthMidiCcSteps(value, 3);
            _SituationToneSynthInvalidateVoiceFilterCaches(s);
            break;
        case 22:
            ctx->controls[SIT_TONE_CTRL_FILTER_KEYTRACK] = norm;
            _SituationToneSynthInvalidateVoiceFilterCaches(s);
            break;
        case 71:
            ctx->controls[SIT_TONE_CTRL_FILTER_RESONANCE] = fmaf(norm, 19.5f, 0.5f);
            _SituationToneSynthInvalidateVoiceFilterCaches(s);
            break;
        case 74:
            ctx->controls[SIT_TONE_CTRL_FILTER_CUTOFF] =
                _SituationToneSynthMidiNormLog(value, 20.0f, 20000.0f);
            _SituationToneSynthInvalidateVoiceFilterCaches(s);
            break;
        case 102:
            ctx->controls[SIT_TONE_CTRL_FILTER_POLES] =
                (float)(_SituationToneSynthMidiCcSteps(value, 4) + 1);
            _SituationToneSynthInvalidateVoiceFilterCaches(s);
            break;
        case 126:
            _SituationToneSynthSetVoiceMode(ctx, 1);
            break;
        case 127:
            _SituationToneSynthSetVoiceMode(ctx, 0);
            break;
        case 106:
            ctx->controls[SIT_TONE_CTRL_PULSE_WIDTH] = fmaf(norm, 0.90f, 0.05f);
            break;
        case 24:
            if (value == 0) {
                ctx->controls[SIT_TONE_CTRL_LFO_RATE] = 0.0f;
            } else {
                ctx->controls[SIT_TONE_CTRL_LFO_RATE] =
                    _SituationToneSynthMidiNormLog(value, 0.05f, 20.0f);
            }
            break;
        case 25:
            ctx->controls[SIT_TONE_CTRL_LFO_WAVEFORM] =
                (float)_SituationToneSynthMidiCcSteps(value, 3);
            break;
        case 26:
            ctx->controls[SIT_TONE_CTRL_LFO_PITCH_AMOUNT] = norm;
            break;
        case 27:
            ctx->controls[SIT_TONE_CTRL_LFO_PITCH_RANGE] = norm * 12.0f;
            break;
        case 28:
            ctx->controls[SIT_TONE_CTRL_LFO_PWM_AMOUNT] = norm;
            break;
        case 29:
            ctx->controls[SIT_TONE_CTRL_LFO_PWM_RANGE] = norm * 0.45f;
            break;
        case 30:
            ctx->controls[SIT_TONE_CTRL_LFO_FILTER_AMOUNT] = norm;
            break;
        case 31:
            ctx->controls[SIT_TONE_CTRL_LFO_FILTER_RANGE] =
                _SituationToneSynthMidiNormLog(value, 20.0f, 8000.0f);
            break;
        case 32:
            ctx->controls[SIT_TONE_CTRL_FILTER_ENV_AMOUNT] = norm;
            break;
        case 33:
            ctx->controls[SIT_TONE_CTRL_FILTER_ENV_RANGE] =
                _SituationToneSynthMidiNormLog(value, 20.0f, 8000.0f);
            break;
        case 107:
            ctx->controls[SIT_TONE_CTRL_SUB_LEVEL] = norm;
            break;
        case 108:
            _SituationToneSynthSetSubWaveform(ctx, (int)(value % 5));
            break;
        case 109:
            ctx->controls[SIT_TONE_CTRL_SUB_OCTAVE] =
                (float)_SituationToneSynthMidiCcSteps(value, 3);
            break;
        case 110: {
            /* 128 discrete fine-tune steps: MIDI 64 = 0 st, 0/127 → −1 st, 127 → +1 st. */
            int fine_step = (int)value - 64;
            ctx->controls[SIT_TONE_CTRL_SUB_FINE] = (float)fine_step / 64.0f;
        } break;
        case 111: {
            /* 128 coarse steps: MIDI 64 = 0 st, 0/127 → −12 st, 127 → +12 st. */
            int coarse_step = (int)value - 64;
            ctx->controls[SIT_TONE_CTRL_SUB_COARSE] =
                (float)coarse_step / 64.0f * SIT_TONE_SUB_COARSE_SEMITONE_MAX;
        } break;
        case 112:
            ctx->controls[SIT_TONE_CTRL_SUB_SYNC] = (value >= 64) ? 1.0f : 0.0f;
            break;
        case 113:
            ctx->controls[SIT_TONE_CTRL_SUB_RING_MOD] = (value >= 64) ? 1.0f : 0.0f;
            break;
        case SITUATION_TONE_SYNTH_PATCH_CC_SLOT:
            _SituationToneSynthPatchHandleSlotCc(ctx, value, sample_rate);
            break;
        case SITUATION_TONE_SYNTH_PATCH_CC_STORE:
            _SituationToneSynthPatchHandleStoreCc(ctx, value);
            break;
        case 116:
            ctx->controls[SIT_TONE_CTRL_LFO_RESET_ON_KEY] = (value >= 64) ? 1.0f : 0.0f;
            break;
        case 117:
            ctx->controls[SIT_TONE_CTRL_LFO_START_PHASE] = norm;
            break;
        case 118:
            ctx->controls[SIT_TONE_CTRL_GLIDE_RESET_ON_KEY] = (value >= 64) ? 1.0f : 0.0f;
            break;
        case 123:
            _SituationToneSynthReleaseAll(s);
            break;
        default:
            break;
    }
}

static inline void _SituationToneSynthMidiInitState(SituationToneSynthNodeState* s) {
    if (!s) return;
    memset(s->voices, 0, sizeof(s->voices));
    s->bend_semitones = 0.0f;
    s->mod_depth_semitones = 0.0f;
    s->ch_volume = 1.0f;
    s->expression = 1.0f;
    s->sustain_pedal = 0;
    s->lfo_phase = 0.0f;
    s->tremolo_depth = 0.0f;
    s->tremolo_phase = 0.0f;
    s->frequency = 440.0f;
    s->amplitude = 0.0f;
    s->phase = 0.0f;
    s->waveform = 0;
    s->patch_slot = 0;
    s->patch_last_cc_slot = 0xFF;
    s->patch_last_cc_store = 0;
    memset(s->patch_slots, 0, sizeof(s->patch_slots));
}

static inline void _SituationToneSynthMidiSilence(SituationToneSynthMidiCtx* ctx) {
    if (!ctx || !ctx->synth) return;
    SituationToneSynthNodeState* s = ctx->synth;
    memset(s->voices, 0, sizeof(s->voices));
    s->bend_semitones = 0.0f;
    s->mod_depth_semitones = 0.0f;
    s->ch_volume = 1.0f;
    s->expression = 1.0f;
    s->sustain_pedal = 0;
    s->lfo_phase = 0.0f;
    s->tremolo_depth = 0.0f;
    s->tremolo_phase = 0.0f;
    s->mod_lfo_phase = 0.0f;
    s->mod_lfo_random = 0.0f;
    s->mod_lfo_cycle = 0;
    s->amplitude = 0.0f;
    if (ctx->controls) {
        ctx->controls[2] = 0.0f;
    }
}

static inline void _SituationToneSynthMidiSyncControls(SituationToneSynthMidiCtx* ctx) {
    if (!ctx || !ctx->controls || !ctx->synth) return;
    SituationToneSynthNodeState* s = ctx->synth;

    if (_SituationToneSynthAnyVoiceActive(s)) {
        ctx->controls[2] = _SituationToneSynthMidiCombinedVolume(s);
        for (int i = 0; i < SITUATION_TONE_SYNTH_MAX_VOICES; i++) {
            if (s->voices[i].active) {
                ctx->controls[0] = _SituationToneSynthVoiceFrequency(s, &s->voices[i], s->lfo_phase,
                                                                     0.0f);
                break;
            }
        }
    } else {
        ctx->controls[2] = 0.0f;
    }
}

static inline void _SituationToneSynthTriggerNoteOn(SituationToneSynthMidiCtx* ctx,
                                                  uint8_t note,
                                                  uint8_t velocity,
                                                  int sample_rate) {
    if (!ctx || !ctx->controls || !ctx->synth || sample_rate <= 0 || note > 127) return;

    int slot = _SituationToneSynthAllocVoice(ctx->synth, ctx->controls);
    if (slot < 0) return;

    SituationToneSynthVoice* v = &ctx->synth->voices[slot];
    int mono_legato = _SituationToneSynthIsMono(ctx->controls) && v->active &&
                      (v->env_state == SIT_TONE_SYNTH_ENV_ATTACK ||
                       v->env_state == SIT_TONE_SYNTH_ENV_DECAY ||
                       v->env_state == SIT_TONE_SYNTH_ENV_SUSTAIN);

    if (mono_legato) {
        _SituationToneSynthVoiceLegatoFromControls(v, ctx->controls, sample_rate, note, velocity);
        if (_SituationToneSynthCtrlOn(ctx->controls, SIT_TONE_CTRL_GLIDE_RESET_ON_KEY)) {
            _SituationToneSynthVoicePrimeEnvelopeOnNoteOn(v);
        }
        _SituationToneSynthVoiceModLfoOnNoteOn(v, ctx->synth, ctx->controls, 0);
    } else {
        _SituationToneSynthVoiceInitFromControls(v, ctx->controls, sample_rate, note, velocity);
        _SituationToneSynthVoiceModLfoOnNoteOn(v, ctx->synth, ctx->controls, 1);
        /* Detached mono retrigger: snap pitch immediately (no glide from prior phrase). */
        if (_SituationToneSynthIsMono(ctx->controls)) {
            v->base_hz = v->target_hz;
        }
    }
}

void _SituationToneSynthOnPitchBend(SituationToneSynthMidiCtx* ctx, int16_t bend);
void _SituationToneSynthOnProgramChange(SituationToneSynthMidiCtx* ctx, uint8_t program);

#endif /* SITUATION_TONE_SYNTH_GRAPH_H */
