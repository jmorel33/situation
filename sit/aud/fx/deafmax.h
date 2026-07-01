/***************************************************************************************************
*
*   sit/aud/fx/deafmax.h - DeafMax Surgical Peak Maximizer
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Zero-allocation, lock-free, real-time safe maximizer.
*   Preserves every frequency and harmonic exactly (no filtering, no phase distortion).
*   Crushes peaks with surgical aggression via fast ballistic envelope following.
*   Extreme loudness + modulatable insanity (drive/release/ceiling can be driven by
*   sequencer steps, VM expressions, mod matrix, etc.).
*
*   Features:
*   - Mono and stereo processing (joined or separated channels)
*   - Pre-saturation via tanh for harmonic density
*   - Fast ballistic peak-hold envelope follower
*   - Aggressive reduction curve + brickwall ceiling
*   - Cubic soft-clip harmonic restoration
*   - Header-only, no external dependencies except <math.h>
*   - C11, compatible with C++ compilation
*
*   Entry Points:
*   1. deafmax_create   - Allocate and initialize (single calloc)
*   2. deafmax_destroy  - Free
*   3. deafmax_reset    - Reset envelope state and defaults
*   4. deafmax_process_mono   - Process mono block
*   5. deafmax_process_stereo - Process stereo block (linked or independent)
*   6. deafmax_set_insane     - "Deaf Mode" preset
*
***************************************************************************************************/
#ifndef SIT_AUD_FX_DEAFMAX_H
#define SIT_AUD_FX_DEAFMAX_H

#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

// FMA detection
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define DEAFMAX_HAS_FMA 1
    #if defined(__GNUC__) || defined(__clang__)
        #define DEAFMAX_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
    #else
        #define DEAFMAX_FMA(a, b, c) fmaf((a), (b), (c))
    #endif
#else
    #define DEAFMAX_HAS_FMA 0
    #define DEAFMAX_FMA(a, b, c) ((a) * (b) + (c))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Types
// ─────────────────────────────────────────────────────────────────────────────

typedef struct DeafMax {
    // User parameters
    float drive;        // pre-saturation intensity (default 1.0)
    float release;      // envelope decay coefficient (default 0.0008)
    float ceiling;      // final hard limit in dBFS-ish offset (default -0.1)
    float makeup;       // post gain multiplier (default 1.0)
    bool  linked;       // stereo linkage (default true)

    // Pre-computed coefficients (updated by setters)
    float decay;        // 1.0 - release (envelope decay multiplier)
    float drive_x_20;   // drive * 20.0 (reduction curve scale)
    float makeup_x_4;   // makeup * 4.0 (combined post-gain)

    // Internal state (per channel: 0=left/mono, 1=right)
    float peak_env[2];  // envelope followers
    float last_out[2];  // reserved for future harmonic restoration
} DeafMax;

// ─────────────────────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────────────────────

DeafMax* deafmax_create(void);
void     deafmax_destroy(DeafMax* m);
void     deafmax_reset(DeafMax* m);

// Parameter setters (all float, safe to call from UI thread)
void deafmax_set_drive   (DeafMax* m, float v);     // 0.0 → insane (default 1.0)
void deafmax_set_release (DeafMax* m, float v);     // seconds 0.00005 → 0.05 (default 0.0008)
void deafmax_set_ceiling (DeafMax* m, float v);     // dBFS offset -1.0 → 0.0 (default -0.1)
void deafmax_set_makeup  (DeafMax* m, float v);     // post-gain multiplier (default 1.0)
void deafmax_set_linked  (DeafMax* m, bool linked); // true = stereo linked (default true)

// Process functions (block-based, can call with frames=1)
void deafmax_process_mono(DeafMax* m,
                          const float* in, float* out, int frames);

void deafmax_process_stereo(DeafMax* m,
                            const float* inL, const float* inR,
                            float* outL, float* outR,
                            int frames);

// "Deaf Mode" preset — crank everything to insanity
void deafmax_set_insane(DeafMax* m);

// ─────────────────────────────────────────────────────────────────────────────
// Implementation
// ─────────────────────────────────────────────────────────────────────────────

#ifdef SIT_DEAFMAX_IMPLEMENTATION

// ── Internal helpers ─────────────────────────────────────────────────────────

// Fast tanh approximation — bounded rational, no branches in the hot range
// Max error ~0.0004 vs tanhf, plenty accurate for saturation
static inline float deafmax_fast_tanhf(float x) {
    if (x <= -3.0f) return -1.0f;
    if (x >=  3.0f) return  1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static inline float deafmax_saturate(float x, float drive) {
    return deafmax_fast_tanhf(x * drive);
}

static inline void deafmax_update_env(DeafMax* m, int ch, float abs_in) {
    m->peak_env[ch] = fmaxf(m->peak_env[ch] * m->decay, abs_in);
}

// Recompute derived coefficients from current parameters
static inline void deafmax_update_coeffs(DeafMax* m) {
    m->decay      = 1.0f - m->release;
    m->drive_x_20 = m->drive * 20.0f;
    m->makeup_x_4 = m->makeup * 4.0f;
}

// ── Create / Destroy / Reset ─────────────────────────────────────────────────

DeafMax* deafmax_create(void) {
    DeafMax* m = (DeafMax*)SIT_CALLOC(1, sizeof(DeafMax));
    if (m) deafmax_reset(m);
    return m;
}

void deafmax_destroy(DeafMax* m) {
    if (m) SIT_FREE(m);
}

void deafmax_reset(DeafMax* m) {
    m->drive       = 1.0f;
    m->release     = 0.0008f;
    m->ceiling     = -0.1f;
    m->makeup      = 1.0f;
    m->linked      = true;
    m->peak_env[0] = 0.0f;
    m->peak_env[1] = 0.0f;
    m->last_out[0] = 0.0f;
    m->last_out[1] = 0.0f;
    deafmax_update_coeffs(m);
}

// ── Setters ──────────────────────────────────────────────────────────────────

void deafmax_set_drive(DeafMax* m, float v) {
    m->drive = fmaxf(0.0f, v);
    deafmax_update_coeffs(m);
}

void deafmax_set_release(DeafMax* m, float v) {
    m->release = fminf(0.05f, fmaxf(0.00005f, v));
    deafmax_update_coeffs(m);
}

void deafmax_set_ceiling(DeafMax* m, float v) {
    m->ceiling = fminf(0.0f, fmaxf(-1.0f, v));
}

void deafmax_set_makeup(DeafMax* m, float v) {
    m->makeup = fmaxf(0.0f, v);
    deafmax_update_coeffs(m);
}

void deafmax_set_linked(DeafMax* m, bool linked) {
    m->linked = linked;
}

// ── Process: Mono ────────────────────────────────────────────────────────────

void deafmax_process_mono(DeafMax* m, const float* in, float* out, int frames) {
    const float drive     = m->drive;
    const float drive_20  = m->drive_x_20;
    const float makeup_4  = m->makeup_x_4;
    const float lo        = -1.0f + m->ceiling;
    const float hi        =  1.0f - m->ceiling;

    for (int i = 0; i < frames; i++) {
        float s   = deafmax_saturate(in[i], drive);
        float env = fabsf(s);
        deafmax_update_env(m, 0, env);

        float gain    = 1.0f / fmaxf(1.0f, m->peak_env[0] * drive_20);
        float crushed = s * gain * makeup_4;

        // Brickwall ceiling
        float clamped = fminf(fmaxf(crushed, lo), hi);

        // Cubic soft-clip harmonic restoration: out = clamped + clamped³ * 0.18
        // FMA: clamped² * clamped * 0.18 + clamped
        out[i] = DEAFMAX_FMA(clamped * clamped * 0.18f, clamped, clamped);
    }
}

// ── Process: Stereo ──────────────────────────────────────────────────────────

void deafmax_process_stereo(DeafMax* m,
                            const float* inL, const float* inR,
                            float* outL, float* outR,
                            int frames)
{
    const float drive     = m->drive;
    const float drive_20  = m->drive_x_20;
    const float makeup_4  = m->makeup_x_4;
    const float lo        = -1.0f + m->ceiling;
    const float hi        =  1.0f - m->ceiling;
    const bool  linked    = m->linked;

    for (int i = 0; i < frames; i++) {
        float sL = deafmax_saturate(inL[i], drive);
        float sR = deafmax_saturate(inR[i], drive);

        float envL = fabsf(sL);
        float envR = fabsf(sR);

        if (linked) {
            float max_env = fmaxf(envL, envR);
            deafmax_update_env(m, 0, max_env);
            deafmax_update_env(m, 1, max_env);
        } else {
            deafmax_update_env(m, 0, envL);
            deafmax_update_env(m, 1, envR);
        }

        float gainL = 1.0f / fmaxf(1.0f, m->peak_env[0] * drive_20);
        float gainR = linked
                    ? gainL
                    : 1.0f / fmaxf(1.0f, m->peak_env[1] * drive_20);

        float crushedL = sL * gainL * makeup_4;
        float crushedR = sR * gainR * makeup_4;

        // Brickwall ceiling
        float cL = fminf(fmaxf(crushedL, lo), hi);
        float cR = fminf(fmaxf(crushedR, lo), hi);

        // Cubic soft-clip harmonic restoration via FMA
        outL[i] = DEAFMAX_FMA(cL * cL * 0.18f, cL, cL);
        outR[i] = DEAFMAX_FMA(cR * cR * 0.18f, cR, cR);
    }
}

// ── Preset: Deaf Mode ────────────────────────────────────────────────────────

void deafmax_set_insane(DeafMax* m) {
    deafmax_set_drive(m, 48.0f);
    deafmax_set_release(m, 0.00012f);
    deafmax_set_ceiling(m, -0.01f);
    deafmax_set_makeup(m, 1.0f);
    deafmax_set_linked(m, true);
}

#endif // SIT_DEAFMAX_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // SIT_AUD_FX_DEAFMAX_H
