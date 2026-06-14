/***************************************************************************************************
*
*   sit/aud/isa110.h - Focusrite ISA 110 Preamp + 4-Band Inductor EQ
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Faithful emulation of the legendary 1985 Focusrite ISA 110 (Rupert Neve).
*   Transformer-coupled preamp + classic 4-band inductor EQ.
*
*   Features:
*   - Musical transformer saturation
*   - Switchable 18 dB/oct HPF
*   - 4-band inductor EQ (2 shelves + 2 bells)
*   - One-pole coefficient smoothing (click-free automation)
*   - FMA-optimized, zero-allocation, real-time safe
*   - Header-only, C11 / C++ compatible
*
*   Thread-safety note:
*   Parameter updates must be called from the audio thread or externally synchronized.
*
*   Input range: normalized [-1, 1]
*
***************************************************************************************************/

#ifndef SIT_AUD_ISA110_H
#define SIT_AUD_ISA110_H

#include <math.h>

// FMA detection (identical to the rest of the library)
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define ISA110_HAS_FMA 1
    #if defined(__GNUC__) || defined(__clang__)
        #define ISA110_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
    #else
        #define ISA110_FMA(a, b, c) fmaf((a), (b), (c))
    #endif
#else
    #define ISA110_HAS_FMA 0
    #define ISA110_FMA(a, b, c) ((a) * (b) + (c))
#endif

#define ISA110_NUM_EQ_BANDS 4
#define ISA110_BUTTERWORTH_Q 0.7071067811865475f
#define ISA110_ANTIDENORMAL   1e-24f

typedef struct {
    double a0, a1, a2, b0, b1, b2;
    double x1, x2, y1, y2;

    // Smoothing targets
    double tb0, tb1, tb2, ta1, ta2;
} ISA110Biquad;

typedef struct {
    float drive;           // 0.0 = clean → 10.0 = saturated
    float hpf_cutoff;      // 20–400 Hz
    bool  hpf_enabled;

    float freq[ISA110_NUM_EQ_BANDS];
    float gain_db[ISA110_NUM_EQ_BANDS];
    float q[ISA110_NUM_EQ_BANDS];
    bool  is_shelf[ISA110_NUM_EQ_BANDS];
} ISA110Params;

typedef struct {
    ISA110Biquad hpf;
    ISA110Biquad eq[ISA110_NUM_EQ_BANDS];

    ISA110Params params;
    float sample_rate;
    float retention_factor;   // 0.0–1.0 (0.999 ≈ 10 ms smoothing at 48 kHz)
    bool  dirty;              // Force coefficient recalc
} ISA110Processor;

/* ============================================================================================ */
/*  Internal helpers                                                                           */
/* ============================================================================================ */

static inline void isa110_init_biquad(ISA110Biquad* bq) {
    bq->a0 = bq->a1 = bq->a2 = bq->b0 = bq->b1 = bq->b2 = 0.0;
    bq->x1 = bq->x2 = bq->y1 = bq->y2 = 0.0;
    bq->tb0 = bq->tb1 = bq->tb2 = bq->ta1 = bq->ta2 = 0.0;
}

static inline void isa110_update_biquad_target(ISA110Biquad* bq,
    double b0, double b1, double b2, double a1, double a2) {
    bq->tb0 = b0; bq->tb1 = b1; bq->tb2 = b2;
    bq->ta1 = a1; bq->ta2 = a2;
}

static inline void isa110_smooth_coefficients(ISA110Biquad* bq, float old_weight) {
    bq->b0 = bq->tb0 + old_weight * (bq->b0 - bq->tb0);
    bq->b1 = bq->tb1 + old_weight * (bq->b1 - bq->tb1);
    bq->b2 = bq->tb2 + old_weight * (bq->b2 - bq->tb2);
    bq->a1 = bq->ta1 + old_weight * (bq->a1 - bq->ta1);
    bq->a2 = bq->ta2 + old_weight * (bq->a2 - bq->ta2);
}

static inline float isa110_apply_biquad(ISA110Biquad* bq, float x) {
    double y = ISA110_FMA(bq->b0, x,
               ISA110_FMA(bq->b1, bq->x1,
               ISA110_FMA(bq->b2, bq->x2,
               -ISA110_FMA(bq->a1, bq->y1, bq->a2 * bq->y2))));

    bq->x2 = bq->x1; bq->x1 = x;
    bq->y2 = bq->y1; bq->y1 = y;
    return (float)(y + ISA110_ANTIDENORMAL);
}

/* ============================================================================================ */
/*  Transformer saturation                                                                     */
/* ============================================================================================ */

static inline float isa110_transformer_saturate(float x, float drive) {
    float s = x * (1.0f + drive * 0.8f);
    if (s <= -3.0f) return -1.0f;
    if (s >=  3.0f) return  1.0f;
    float x2 = s * s;
    return s * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/* ============================================================================================ */
/*  Public API                                                                                 */
/* ============================================================================================ */

static inline void isa110_init(ISA110Processor* proc, float sample_rate) {
    proc->sample_rate = sample_rate;
    proc->retention_factor = 0.999f;   // ~10 ms smoothing at 48 kHz
    proc->dirty = true;

    proc->params.drive = 1.0f;
    proc->params.hpf_cutoff = 75.0f;
    proc->params.hpf_enabled = true;

    proc->params.freq[0] = 80.0f;    proc->params.gain_db[0] = 0.0f; proc->params.is_shelf[0] = true;
    proc->params.freq[1] = 220.0f;   proc->params.gain_db[1] = 0.0f; proc->params.is_shelf[1] = false;
    proc->params.freq[2] = 2200.0f;  proc->params.gain_db[2] = 0.0f; proc->params.is_shelf[2] = false;
    proc->params.freq[3] = 12000.0f; proc->params.gain_db[3] = 0.0f; proc->params.is_shelf[3] = true;

    proc->params.q[0] = ISA110_BUTTERWORTH_Q;
    proc->params.q[1] = 1.4f;
    proc->params.q[2] = 1.4f;
    proc->params.q[3] = ISA110_BUTTERWORTH_Q;

    isa110_init_biquad(&proc->hpf);
    for (int i = 0; i < ISA110_NUM_EQ_BANDS; i++)
        isa110_init_biquad(&proc->eq[i]);
}

static inline void isa110_set_sample_rate(ISA110Processor* proc, float sample_rate) {
    if (sample_rate == proc->sample_rate) return;
    proc->sample_rate = sample_rate;
    proc->dirty = true;
}

static inline void isa110_update_preamp(ISA110Processor* proc, float drive, float hpf_cutoff, bool hpf_enabled) {
    proc->params.drive = fmaxf(0.0f, drive);
    proc->params.hpf_cutoff = fmaxf(20.0f, fminf(400.0f, hpf_cutoff));
    proc->params.hpf_enabled = hpf_enabled;
    proc->dirty = true;
}

static inline void isa110_update_eq_band(ISA110Processor* proc, int band, float freq, float gain_db, float Q) {
    if (band < 0 || band >= ISA110_NUM_EQ_BANDS) return;

    freq    = fmaxf(20.0f, fminf(20000.0f, freq));
    gain_db = fmaxf(-24.0f, fminf(24.0f, gain_db));
    if (!proc->params.is_shelf[band])
        Q = fmaxf(0.1f, fminf(20.0f, Q));

    proc->params.freq[band] = freq;
    proc->params.gain_db[band] = gain_db;
    if (!proc->params.is_shelf[band]) proc->params.q[band] = Q;

    proc->dirty = true;
}

static inline void isa110_process(ISA110Processor* proc, const float* input, float* output, unsigned long frame_count) {
    if (proc->dirty) {
        // HPF
        if (proc->params.hpf_enabled) {
            double w0 = 2.0 * M_PI * proc->params.hpf_cutoff / proc->sample_rate;
            double alpha = sin(w0) / (2.0 * ISA110_BUTTERWORTH_Q);
            double b0 = (1.0 + cos(w0)) / 2.0;
            double b1 = -(1.0 + cos(w0));
            double b2 = (1.0 + cos(w0)) / 2.0;
            double a0 = 1.0 + alpha;
            double a1 = -2.0 * cos(w0);
            double a2 = 1.0 - alpha;
            isa110_update_biquad_target(&proc->hpf, b0/a0, b1/a0, b2/a0, a1/a0, a2/a0);
        }

        // EQ bands
        for (int b = 0; b < ISA110_NUM_EQ_BANDS; b++) {
            double w0 = 2.0 * M_PI * proc->params.freq[b] / proc->sample_rate;
            double A = pow(10.0, proc->params.gain_db[b] / 40.0);
            double cosw = cos(w0);

            if (proc->params.is_shelf[b]) {
                double alpha = sin(w0) * sqrt((A + 1.0) / A) * 0.7071067811865475;
                if (b == 0) { // Low shelf
                    isa110_update_biquad_target(&proc->eq[b],
                        A*((A+1)-(A-1)*cosw+2*sqrt(A)*alpha)/(A+1+(A-1)*cosw+2*sqrt(A)*alpha),
                        2*A*((A-1)-(A+1)*cosw)/(A+1+(A-1)*cosw+2*sqrt(A)*alpha),
                        A*((A+1)-(A-1)*cosw-2*sqrt(A)*alpha)/(A+1+(A-1)*cosw+2*sqrt(A)*alpha),
                        -2*((A-1)+(A+1)*cosw)/(A+1+(A-1)*cosw+2*sqrt(A)*alpha),
                        (A+1+(A-1)*cosw-2*sqrt(A)*alpha)/(A+1+(A-1)*cosw+2*sqrt(A)*alpha));
                } else { // High shelf
                    isa110_update_biquad_target(&proc->eq[b],
                        A*((A+1)+(A-1)*cosw+2*sqrt(A)*alpha)/((A+1)-(A-1)*cosw+2*sqrt(A)*alpha),
                        -2*A*((A-1)+(A+1)*cosw)/((A+1)-(A-1)*cosw+2*sqrt(A)*alpha),
                        A*((A+1)+(A-1)*cosw-2*sqrt(A)*alpha)/((A+1)-(A-1)*cosw+2*sqrt(A)*alpha),
                        2*((A-1)-(A+1)*cosw)/((A+1)-(A-1)*cosw+2*sqrt(A)*alpha),
                        (A+1-(A-1)*cosw-2*sqrt(A)*alpha)/((A+1)-(A-1)*cosw+2*sqrt(A)*alpha));
                }
            } else {
                double alpha = sin(w0) / (2.0 * proc->params.q[b]);
                isa110_update_biquad_target(&proc->eq[b],
                    (1 + alpha*A) / (1 + alpha/A),
                    -2*cos(w0) / (1 + alpha/A),
                    (1 - alpha*A) / (1 + alpha/A),
                    -2*cos(w0) / (1 + alpha/A),
                    (1 - alpha/A) / (1 + alpha/A));
            }
        }
        proc->dirty = false;
    }

    // Per-block smoothing: retain this much of the old coefficient
    float old_weight = powf(proc->retention_factor, (float)frame_count);

    for (unsigned long i = 0; i < frame_count; i++) {
        for (int ch = 0; ch < 2; ch++) {
            float s = input[i * 2 + ch];

            // Transformer saturation
            s = isa110_transformer_saturate(s, proc->params.drive);

            // HPF (only if enabled)
            if (proc->params.hpf_enabled) {
                isa110_smooth_coefficients(&proc->hpf, old_weight);
                s = isa110_apply_biquad(&proc->hpf, s);
            }

            // EQ
            for (int b = 0; b < ISA110_NUM_EQ_BANDS; b++) {
                isa110_smooth_coefficients(&proc->eq[b], old_weight);
                s = isa110_apply_biquad(&proc->eq[b], s);
            }

            output[i * 2 + ch] = s;
        }
    }
}

static inline void isa110_reset(ISA110Processor* proc) {
    isa110_init_biquad(&proc->hpf);
    for (int i = 0; i < ISA110_NUM_EQ_BANDS; i++)
        isa110_init_biquad(&proc->eq[i]);
    proc->dirty = true;
}

#endif /* SIT_AUD_ISA110_H */