/***************************************************************************************************
*
*   sit/aud/overdrive.h - Ultra-flexible Overdrive Module
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Ultra-flexible Overdrive - inspired by Korg Wavestation Overdrive-Filter-EQ.
*   Single-header, zero-allocation, real-time safe DSP effect.
*   Fast tanh-based soft clip + hard clip options.
*   Input-dependent behaviour (like analog drive).
*   Post-distortion biquad filter (the "Hot Spot" resonant filter).
*   Simple post-EQ shelves.
*   Dry/wet mix, gains, etc.
*
***************************************************************************************************/

#ifndef SIT_AUX_OVERDRIVE_H
#define SIT_AUX_OVERDRIVE_H

#include <math.h>   // tanhf, fmaxf, fminf, etc.
#include <stdint.h> // for fixed types if you use them elsewhere
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Types & enums
// -----------------------------------------------------------------------------

typedef enum {
    SIT_OVERDRIVE_MODE_SOFT,     // Smooth guitar-like overdrive (tanh)
    SIT_OVERDRIVE_MODE_HARD,     // Aggressive clipping
    SIT_OVERDRIVE_MODE_TUBE,     // Warmer, even harmonics + sag simulation
    SIT_OVERDRIVE_MODE_FOLD,     // Wavefolding for synth-y bite
    SIT_OVERDRIVE_MODE_COUNT
} sit_overdrive_mode;

typedef struct sit_overdrive {
    // Parameters (set via setters or directly if you prefer)
    sit_overdrive_mode mode;
    float drive;          // 0..200+   (Edge - how much distortion)
    float input_gain;     // 0..4      linear gain before drive
    float output_gain;    // 0..2      linear post-EQ
    float mix;            // 0..1      dry/wet

    // Post-distortion filter ("Hot Spot")
    float filter_cutoff;  // 20..22050 Hz
    float filter_res;     // 0..1      (mapped to Q ~0.5..12)
    float low_shelf_db;   // -24..+24 dB
    float high_shelf_db;  // -24..+24 dB
    float low_shelf_gain;
    float high_shelf_gain;

    // Extra flavour
    float asymmetry;      // -1..1     (0 = symmetric, positive = more even harmonics)

    // Internal state (biquad filter coefficients & z-regs)
    float a0, a1, a2, b1, b2;   // Direct form II coeffs
    float z1_l, z2_l;           // Left channel delay
    float z1_r, z2_r;           // Right channel delay

    // Pre-calc helpers
    float prev_drive;
    float sample_rate;
} sit_overdrive;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

void sit_overdrive_init(sit_overdrive* od, float sample_rate);
void sit_overdrive_reset(sit_overdrive* od);

// Main processing - processes interleaved stereo (or use L/R separately)
//   in:  interleaved float* [frames * 2]
//   out: interleaved float* [frames * 2]  (can be same buffer)
//   frames: number of frames (NOT samples!)
void sit_overdrive_process(sit_overdrive* od, const float* in, float* out, int frames);

// Optional: separate L/R buffers (non-interleaved)
void sit_overdrive_process_split(sit_overdrive* od,
                                 const float* in_l, const float* in_r,
                                 float* out_l, float* out_r, int frames);

// Parameter setters (for modulation / UI)
void sit_overdrive_set_mode(sit_overdrive* od, sit_overdrive_mode mode);
void sit_overdrive_set_drive(sit_overdrive* od, float value);       // 0..200+
void sit_overdrive_set_input_gain(sit_overdrive* od, float value);  // 0..4
void sit_overdrive_set_output_gain(sit_overdrive* od, float value); // 0..2
void sit_overdrive_set_mix(sit_overdrive* od, float value);         // 0..1
void sit_overdrive_set_filter_cutoff(sit_overdrive* od, float hz);
void sit_overdrive_set_filter_res(sit_overdrive* od, float norm);   // 0..1
void sit_overdrive_set_low_shelf(sit_overdrive* od, float db);
void sit_overdrive_set_high_shelf(sit_overdrive* od, float db);
void sit_overdrive_set_asymmetry(sit_overdrive* od, float norm);    // -1..1

// Quick "Wavestation legacy" preset
void sit_overdrive_set_wavestation_legacy(sit_overdrive* od);

#ifdef SIT_OVERDRIVE_IMPLEMENTATION

// -----------------------------------------------------------------------------
// Implementation
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Precomputed table - place this at file scope or in a static init function
// -----------------------------------------------------------------------------

#define DB_TABLE_MIN_DB   -120.0f
#define DB_TABLE_MAX_DB    +24.0f
#define DB_TABLE_STEPS     4096          // ~0.035 dB/step -> very smooth
#define DB_TABLE_RANGE    (DB_TABLE_MAX_DB - DB_TABLE_MIN_DB)
#define DB_TABLE_SCALE    (DB_TABLE_STEPS / DB_TABLE_RANGE)

static float db_to_gain_table[DB_TABLE_STEPS + 1];
static int db_table_initialized = 0;

static void init_db_table(void) {
    if (db_table_initialized) return;
    for (int i = 0; i <= DB_TABLE_STEPS; ++i) {
        float db = DB_TABLE_MIN_DB + (float)i * DB_TABLE_RANGE / (float)DB_TABLE_STEPS;
        db_to_gain_table[i] = expf(db * 0.11512925464970228f);  // or powf(10, db*0.05f)
    }
    db_table_initialized = 1;
}

// -----------------------------------------------------------------------------
// Fast lookup version
// -----------------------------------------------------------------------------
static inline float db_to_gain(float db) {
    // Clamp input range
    if (db <= DB_TABLE_MIN_DB) return db_to_gain_table[0];
    if (db >= DB_TABLE_MAX_DB) return db_to_gain_table[DB_TABLE_STEPS];

    float pos = (db - DB_TABLE_MIN_DB) * DB_TABLE_SCALE;
    int idx = (int)pos;
    float frac = pos - (float)idx;

    // Linear interpolation
    return db_to_gain_table[idx] + frac * (db_to_gain_table[idx + 1] - db_to_gain_table[idx]);
}




// Fast tanh approximation for real-time DSP
// Using a safe, bounded approximation:
static inline float fast_tanhf(float x) {
    if (x <= -3.0f) return -1.0f;
    if (x >= 3.0f) return 1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static inline float soft_clip(float x) {
    return fast_tanhf(x);
}

static inline float hard_clip(float x) {
    return fmaxf(-1.0f, fminf(1.0f, x));
}

static inline float tube_sag(float x, float asym) {
    float s = x * (1.0f + 0.3f * asym * (x > 0 ? 1.0f : -1.0f));
    return fast_tanhf(s * 1.4f) * 0.8f;  // rough tube warmth + compression feel
}

static inline float wavefold(float x, float amount) {
    float f = x * (1.0f + amount * 4.0f);
    float folded = f - 4.0f * floorf((f + 2.0f) * 0.25f) - 2.0f;
    return folded * (1.0f - amount * 0.4f);  // tame extremes
}

static void sit_overdrive_update_coeffs(sit_overdrive* od) {
    if (od->sample_rate <= 0) return;

    float omega = 2.0f * (float)M_PI * od->filter_cutoff / od->sample_rate;
    float alpha = sinf(omega) / (2.0f * (0.5f + od->filter_res * 11.5f)); // Q 0.5..12

    float cosw = cosf(omega);
    float sinw = sinf(omega);

    float b0 = (1.0f - cosw) * 0.5f;
    float b1 = 1.0f - cosw;
    float b2 = b0;

    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosw;
    float a2 = 1.0f - alpha;

    // Normalize (b coeffs / a0)
    od->b1 = b1 / a0;
    od->b2 = b2 / a0;
    od->a0 = b0 / a0;  // actually b0/a0 for direct form
    od->a1 = a1 / a0;
    od->a2 = a2 / a0;
}

void sit_overdrive_init(sit_overdrive* od, float sample_rate) {
    init_db_table();
    memset(od, 0, sizeof(*od));
    od->sample_rate = sample_rate;

    // Sensible defaults (close to Wavestation Overdrive vibe)
    od->mode          = SIT_OVERDRIVE_MODE_SOFT;
    od->drive         = 40.0f;
    od->input_gain    = 1.0f;
    od->output_gain   = 1.0f;
    od->mix           = 1.0f;
    od->filter_cutoff = 4500.0f;   // "Hot Spot" default-ish
    od->filter_res    = 0.4f;
    od->low_shelf_db  = 3.0f;
    od->high_shelf_db = -3.0f;
    od->asymmetry     = 0.0f;

    sit_overdrive_update_coeffs(od);
    od->low_shelf_gain = db_to_gain(od->low_shelf_db * 0.5f);
    od->high_shelf_gain = db_to_gain(od->high_shelf_db * 0.5f);
    sit_overdrive_reset(od);
}

void sit_overdrive_reset(sit_overdrive* od) {
    od->z1_l = od->z2_l = 0.0f;
    od->z1_r = od->z2_r = 0.0f;
    od->prev_drive = od->drive;
}

static inline float process_sample(sit_overdrive* od, float x, float* z1, float* z2) {
    float in = x * od->input_gain;

    // Drive scaling (input-dependent like analog)
    float effective_drive = od->drive * (1.0f + fabsf(in) * 0.3f);

    float distorted;
    switch (od->mode) {
        case SIT_OVERDRIVE_MODE_SOFT:
            distorted = soft_clip(in * (effective_drive * 0.05f));
            break;
        case SIT_OVERDRIVE_MODE_HARD:
            distorted = hard_clip(in * (effective_drive * 0.08f));
            break;
        case SIT_OVERDRIVE_MODE_TUBE:
            distorted = tube_sag(in, od->asymmetry) * (effective_drive * 0.06f);
            break;
        case SIT_OVERDRIVE_MODE_FOLD:
            distorted = wavefold(in, od->asymmetry) * (effective_drive * 0.07f);
            break;
        default:
            distorted = in;
    }

    // Post-distortion filter (biquad direct form II)
    float y = od->a0 * distorted
            + od->b1 * (*z1)
            + od->b2 * (*z2);
    (*z2) = (*z1);
    (*z1) = y - od->a1 * distorted - od->a2 * (*z2);

    // Quick & dirty shelves (1-pole approx for speed)
    float low  = y * od->low_shelf_gain;   // gentle
    float high = y * od->high_shelf_gain;
    float shelved = (low + high) * 0.5f + y * 0.5f;  // blend

    return shelved * od->output_gain;
}

void sit_overdrive_process(sit_overdrive* od, const float* in, float* out, int frames) {
    if (od->drive != od->prev_drive) {
        sit_overdrive_update_coeffs(od);
        od->prev_drive = od->drive;
    }

    for (int i = 0; i < frames; ++i) {
        float l = in[i*2 + 0];
        float r = in[i*2 + 1];

        float wet_l = process_sample(od, l, &od->z1_l, &od->z2_l);
        float wet_r = process_sample(od, r, &od->z1_r, &od->z2_r);

        out[i*2 + 0] = l * (1.0f - od->mix) + wet_l * od->mix;
        out[i*2 + 1] = r * (1.0f - od->mix) + wet_r * od->mix;
    }
}

void sit_overdrive_process_split(sit_overdrive* od,
                                 const float* in_l, const float* in_r,
                                 float* out_l, float* out_r, int frames) {
    if (od->drive != od->prev_drive) {
        sit_overdrive_update_coeffs(od);
        od->prev_drive = od->drive;
    }

    for (int i = 0; i < frames; ++i) {
        float wet_l = process_sample(od, in_l[i], &od->z1_l, &od->z2_l);
        float wet_r = process_sample(od, in_r[i], &od->z1_r, &od->z2_r);

        out_l[i] = in_l[i] * (1.0f - od->mix) + wet_l * od->mix;
        out_r[i] = in_r[i] * (1.0f - od->mix) + wet_r * od->mix;
    }
}

// Setters (clamp & update when needed)
void sit_overdrive_set_mode(sit_overdrive* od, sit_overdrive_mode mode) {
    if (mode < 0 || mode >= SIT_OVERDRIVE_MODE_COUNT) mode = SIT_OVERDRIVE_MODE_SOFT;
    od->mode = mode;
}

void sit_overdrive_set_drive(sit_overdrive* od, float value) {
    od->drive = fmaxf(0.0f, value);
}

void sit_overdrive_set_input_gain(sit_overdrive* od, float value) { od->input_gain = fmaxf(0.0f, value); }
void sit_overdrive_set_output_gain(sit_overdrive* od, float value) { od->output_gain = fmaxf(0.0f, value); }
void sit_overdrive_set_mix(sit_overdrive* od, float value) { od->mix = fminf(1.0f, fmaxf(0.0f, value)); }

void sit_overdrive_set_filter_cutoff(sit_overdrive* od, float hz) {
    od->filter_cutoff = fmaxf(20.0f, fminf(22050.0f, hz));
    sit_overdrive_update_coeffs(od);
    od->low_shelf_gain = db_to_gain(od->low_shelf_db * 0.5f);
    od->high_shelf_gain = db_to_gain(od->high_shelf_db * 0.5f);
}

void sit_overdrive_set_filter_res(sit_overdrive* od, float norm) {
    od->filter_res = fminf(1.0f, fmaxf(0.0f, norm));
    sit_overdrive_update_coeffs(od);
    od->low_shelf_gain = db_to_gain(od->low_shelf_db * 0.5f);
    od->high_shelf_gain = db_to_gain(od->high_shelf_db * 0.5f);
}

void sit_overdrive_set_low_shelf(sit_overdrive* od, float db) {
    od->low_shelf_db = fminf(24.0f, fmaxf(-24.0f, db));
    od->low_shelf_gain = db_to_gain(od->low_shelf_db * 0.5f);
}
void sit_overdrive_set_high_shelf(sit_overdrive* od, float db) {
    od->high_shelf_db = fminf(24.0f, fmaxf(-24.0f, db));
    od->high_shelf_gain = db_to_gain(od->high_shelf_db * 0.5f);
}
void sit_overdrive_set_asymmetry(sit_overdrive* od, float norm) { od->asymmetry = fminf(1.0f, fmaxf(-1.0f, norm)); }

// Wavestation-ish starting point
void sit_overdrive_set_wavestation_legacy(sit_overdrive* od) {
    od->mode          = SIT_OVERDRIVE_MODE_SOFT;
    od->drive         = 50.0f;      // Edge ~50
    od->input_gain    = 1.2f;
    od->output_gain   = 0.9f;
    od->mix           = 1.0f;
    od->filter_cutoff = 8000.0f;    // Hot Spot mid-high
    od->filter_res    = 0.6f;
    od->low_shelf_db  = 4.0f;
    od->high_shelf_db = -5.0f;
    od->asymmetry     = 0.2f;
    sit_overdrive_update_coeffs(od);
    od->low_shelf_gain = db_to_gain(od->low_shelf_db * 0.5f);
    od->high_shelf_gain = db_to_gain(od->high_shelf_db * 0.5f);
}

#endif // SIT_OVERDRIVE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // SIT_AUX_OVERDRIVE_H
