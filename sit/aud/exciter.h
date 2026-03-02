/***************************************************************************************************
*
*   exciter.h - Harmonic Exciter Audio Effect Library
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Lightweight, real-time capable harmonic exciter audio effect.
*   This file is intended to be included within the audio subsystem implementation.
*
***************************************************************************************************/

/**
 * @file exciter.h
 * @brief Harmonic Exciter Audio Effect Library
 *
 * This library provides a lightweight, real-time capable harmonic exciter audio effect implemented in standard C11. It is designed to enhance audio signals by generating
 * melodic upper harmonics through asymmetric soft clipping, combined with transient detection for responsive "spark" on peaks. The effect adds brightness, warmth, and
 * excitement to sounds, making it suitable for music production, sound design, or embedded audio processing.
 *
 * Key Features:
 * - **Harmonic Generation**: Uses an asymmetric cubic soft clipper to produce a mix of even-order (warm, octave-like) and odd-order (bright) harmonics, shifting energy
 *   upward in the frequency spectrum for a more melodic, euphonic distortion compared to hard limiting.
 * - **High-Pass Filtering**: A 2nd-order Butterworth high-pass filter isolates high-frequency content for excitation, preventing muddiness in lower bands.
 * - **Transient Responsiveness**: Dual envelope followers detect peaks (transients) and dynamically boost the drive parameter, enhancing harmonics selectively on attacks
 *   (e.g., drums, plucks) for added liveliness without constant harshness.
 * - **Soft Limiting**: Integrated soft clipping acts as a gentle limiter, rounding off peaks to maintain headroom while generating harmonics.
 * - **Mono/Stereo Support**: Processes interleaved float buffers for 1 (mono) or 2 (stereo) channels; can be extended for more.
 * - **Parameter Control**: Runtime-adjustable settings via API functions for cutoff, drive, mix, clip softness, transient sensitivity, attack, and release.
 * - **Efficiency**: Low computational overhead, using basic math operations; suitable for real-time applications on desktop, mobile, or embedded systems.
 * - **Stateful Processing**: Maintains filter and envelope states across buffer calls to ensure seamless audio without artifacts.
 *
 * Usage Overview:
 * 1. Define parameters using ExciterParams (or use defaults).
 * 2. Initialize state with init_exciter().
 * 3. Adjust settings dynamically with set_* functions.
 * 4. Process audio buffers with process_exciter().
 * 5. Clean up with deinit_exciter() if needed.
 *
 * Signal Flow:
 * - Input -> Transient Detection (modulates drive on peaks)
 * - Wet Path: High-Pass Filter -> Pre-Gain (Drive) -> Asymmetric Soft Clip -> Post-Gain
 * - Mix Wet with Dry -> Output
 *
 * Dependencies: <math.h>, <stddef.h>, <string.h> (standard C libraries).
 * Compile with -std=c11 -lm.
 *
 * Limitations:
 * - No oversampling (may alias at high drive; add if needed for production).
 * - Fixed to max 2 channels; extend MAX_CHANNELS for multichannel.
 * - Assumes input/output in [-1, 1] normalized float range.
 *
 * Example Applications:
 * - Enhancing vocals or instruments in DAWs.
 * - Adding sparkle to electronic music elements.
 * - Real-time effects in games or live audio systems.
 *
 * For debugging/analysis, uncomment printf statements in process_exciter() to log intermediate values (e.g., transient amount, boosted, clipped).
 */

#ifndef SIT_AUX_EXCITER_H
#define SIT_AUX_EXCITER_H

#include <math.h>
#include <stddef.h>
#include <string.h>

#define MAX_CHANNELS 2
#define PI 3.141592653589793f

/**
 * @struct ExciterParams
 * @brief Configuration parameters for the exciter effect.
 *
 * All parameters are floats for fine-grained control. Use default_exciter_params()
 * to set sensible starting values.
 */
typedef struct {
    float fc;             // High-pass cutoff frequency in Hz (e.g., 5000.0f).
    float drive;          // Base pre-gain for harmonic generation (e.g., 10.0f).
    float post_attenuate; // Post-gain to normalize levels (typically 1/drive).
    float mix;            // Wet/dry mix ratio (0.0 dry, 1.0 wet; e.g., 0.5f).
    float clip_k;         // Clipper softness (0.01 hard to 1.0 soft; e.g., 0.5f).
    float trans_sens;     // Transient sensitivity (0.0 off, 1.0+ boost; e.g., 1.5f).
    float trans_attack;   // Transient envelope attack time in seconds (e.g., 0.005f).
    float trans_release;  // Transient envelope release time in seconds (e.g., 0.05f).
} ExciterParams;

/**
 * @struct ExciterState
 * @brief Internal state for the exciter, including params, coeffs, and per-channel history.
 *
 * Do not modify directly; use init_exciter() and setter functions.
 */
typedef struct {
    ExciterParams params; // Current parameters.
    float sample_rate;    // Sample rate in Hz (used for coefficient computation).

    // Precomputed filter coefficients for Butterworth high-pass.
    float a0, a1, a2;
    float b1, b2;

    // Filter state per channel.
    float x1[MAX_CHANNELS];
    float x2[MAX_CHANNELS];
    float y1[MAX_CHANNELS];
    float y2[MAX_CHANNELS];

    // Transient detection states per channel (envelope followers).
    float fast_env[MAX_CHANNELS];  // Fast envelope for peak detection.
    float slow_env[MAX_CHANNELS];  // Slow envelope for average level.
} ExciterState;

/**
 * @brief Set default values for ExciterParams.
 * @param params Pointer to the params struct to initialize.
 */
static void default_exciter_params(ExciterParams *params);

/**
 * @brief Initialize the exciter state.
 * @param state Pointer to the state struct.
 * @param params Optional pointer to custom params (NULL for defaults).
 * @param sample_rate Sample rate in Hz.
 */
static void init_exciter(ExciterState *state, const ExciterParams *params, float sample_rate);

/**
 * @brief Deinitialize the exciter state (clears memory).
 * @param state Pointer to the state struct.
 */
static void deinit_exciter(ExciterState *state);

/**
 * @brief Set the high-pass cutoff frequency.
 * @param state Pointer to the state.
 * @param fc New cutoff in Hz (must be >0 and < Nyquist).
 */
static void set_cutoff(ExciterState *state, float fc);

/**
 * @brief Set the base drive (pre-gain).
 * @param state Pointer to the state.
 * @param drive New drive value (>0; auto-updates post_attenuate).
 */
static void set_drive(ExciterState *state, float drive);

/**
 * @brief Set the wet/dry mix.
 * @param state Pointer to the state.
 * @param mix New mix value (clamped 0-1).
 */
static void set_mix(ExciterState *state, float mix);

/**
 * @brief Set the clipper softness.
 * @param state Pointer to the state.
 * @param clip_k New softness value (clamped 0.01-1.0).
 */
static void set_clip_softness(ExciterState *state, float clip_k);

/**
 * @brief Set the transient sensitivity.
 * @param state Pointer to the state.
 * @param sens New sensitivity (>=0; 0 disables transient boost).
 */
static void set_trans_sensitivity(ExciterState *state, float sens);

/**
 * @brief Set the transient attack time.
 * @param state Pointer to the state.
 * @param attack New attack in seconds (clamped 0.001-0.1).
 */
static void set_trans_attack(ExciterState *state, float attack);

/**
 * @brief Set the transient release time.
 * @param state Pointer to the state.
 * @param release New release in seconds (clamped 0.01-1.0).
 */
static void set_trans_release(ExciterState *state, float release);

/**
 * @brief Process an audio buffer through the exciter.
 * @param state Pointer to the state.
 * @param input Input buffer (interleaved floats, normalized [-1,1]).
 * @param output Output buffer (same format as input).
 * @param num_frames Number of frames to process.
 * @param num_channels Number of channels (1 or 2).
 */
static void process_exciter(ExciterState *state, const float *input, float *output, size_t num_frames, int num_channels);

// Default parameters (added transient defaults)
static void default_exciter_params(ExciterParams *params) {
    params->fc = 5000.0f;
    params->drive = 10.0f;
    params->post_attenuate = 1.0f / params->drive;
    params->mix = 0.5f;
    params->clip_k = 0.5f;
    params->trans_sens = 1.5f;      // Moderate boost on transients
    params->trans_attack = 0.005f;  // Fast attack (5ms)
    params->trans_release = 0.05f;  // Medium release (50ms)
}

// Compute 2nd-order Butterworth high-pass coefficients based on fc and sample_rate
static void compute_coeffs(ExciterState *state) {
    float w = tanf(PI * state->params.fc / state->sample_rate);
    float denom = w * w + sqrtf(2.0f) * w + 1.0f;
    state->a0 = w * w / denom;
    state->a1 = -2.0f * state->a0;
    state->a2 = state->a0;
    state->b1 = -2.0f * (w * w - 1.0f) / denom;
    state->b2 = -(w * w - sqrtf(2.0f) * w + 1.0f) / denom;
}

// Initialize the exciter state with params and sample_rate
static void init_exciter(ExciterState *state, const ExciterParams *params, float sample_rate) {
    memset(state, 0, sizeof(*state));
    if (params) {
        state->params = *params;
    } else {
        default_exciter_params(&state->params);
    }
    state->sample_rate = sample_rate > 0.0f ? sample_rate : 44100.0f;  // Default to 44.1kHz if invalid
    compute_coeffs(state);
    // Initialize envelopes to 0
    for (int c = 0; c < MAX_CHANNELS; ++c) {
        state->fast_env[c] = 0.0f;
        state->slow_env[c] = 0.0f;
    }
}

// Deinitialize (clear state)
static void deinit_exciter(ExciterState *state) {
    memset(state, 0, sizeof(*state));
}

// Settings API functions (updated for new params; recompute coeffs/post_attenuate as needed)
static void set_cutoff(ExciterState *state, float fc) {
    if (fc > 0.0f && fc < state->sample_rate / 2.0f) {
        state->params.fc = fc;
        compute_coeffs(state);
    }
}

static void set_drive(ExciterState *state, float drive) {
    if (drive > 0.0f) {
        state->params.drive = drive;
        state->params.post_attenuate = 1.0f / drive;
    }
}

static void set_mix(ExciterState *state, float mix) {
    state->params.mix = fmaxf(0.0f, fminf(1.0f, mix));
}

static void set_clip_softness(ExciterState *state, float clip_k) {
    state->params.clip_k = fmaxf(0.01f, fminf(1.0f, clip_k));
}

static void set_trans_sensitivity(ExciterState *state, float sens) {
    state->params.trans_sens = fmaxf(0.0f, sens);  // 0 disables transient boost
}

static void set_trans_attack(ExciterState *state, float attack) {
    state->params.trans_attack = fmaxf(0.001f, fminf(0.1f, attack));  // Constrain for stability
}

static void set_trans_release(ExciterState *state, float release) {
    state->params.trans_release = fmaxf(0.01f, fminf(1.0f, release));
}

// Asymmetric cubic soft clipper: introduces even and odd harmonics asymmetrically for richer, more melodic distortion.
// The asymmetry helps generate a broader spectrum of overtones that can align better with musical intervals (e.g., octaves from even harmonics).
// Input x should be normalized [-1,1]; output is softly clipped with 'k' controlling the knee smoothness.
static float asymmetric_soft_clip(float x, float k) {
    float z = 2.0f / 3.0f;  // Bias for asymmetry
    x -= z;
    float lm_val = -sqrtf(k * k * k) / (k * k * k);
    float max_val = fmaxf(x, lm_val);
    float kx = k * max_val;
    float cubic = (kx * kx * kx) / 3.0f;
    float kubic = max_val - cubic;
    float nl = (x > 0.0f) ? x : kubic;
    return nl + z;
}

// Process function: now with transient detection for responsive excitation on peaks.
// Signal flow updates:
// 0. Input analysis: Compute absolute value for envelope detection (full-wave rectification).
// 1. Transient detection (per channel):
//    - Use two envelope followers: fast (quick response to peaks) and slow (average level).
//    - Envelope formula: env = alpha * |input| + (1 - alpha) * prev_env
//      - Alpha = exp(-1 / (tau * sr)) where tau is time constant in seconds.
//    - Transient amount = max(0, fast_env - slow_env) * sens
//      - This detects peaks where fast > slow, indicating a transient.
//      - Normalized and scaled by sensitivity to boost drive/mix.
//    - Here, we modulate the drive: effective_drive = base_drive * (1 + transient_amount)
//      - This boosts harmonic generation on transients for "spark," then attenuates accordingly.
//      - Could alternatively modulate mix for similar effect.
// 2-6. Rest unchanged, but with modulated drive.
// This makes the exciter more dynamic: harmonics "spark" on attacks (drums, plucks), adding liveliness while remaining melodic. Deeper analysis: Transients shift more energy upward briefly, enhancing
// perceived brightness without constant harshness. Adjust sens/attack/release for genre-specific response (e.g., faster for EDM, slower for acoustics).
static void process_exciter(ExciterState *state, const float *input, float *output, size_t num_frames, int num_channels) {
    if (num_channels < 1 || num_channels > MAX_CHANNELS) return;

    // Precompute alpha for envelopes (time-invariant, but could recompute on param change)
    float alpha_fast = expf(-1.0f / (state->params.trans_attack * state->sample_rate));
    float alpha_slow = expf(-1.0f / (state->params.trans_release * state->sample_rate));

    for (size_t f = 0; f < num_frames; ++f) {
        for (int c = 0; c < num_channels; ++c) {
            size_t idx = f * num_channels + c;
            float dry = input[idx];  // Stage 1: Dry signal

            // Stage 0: Transient detection
            float abs_in = fabsf(dry);
            state->fast_env[c] = alpha_fast * abs_in + (1.0f - alpha_fast) * state->fast_env[c];
            state->slow_env[c] = alpha_slow * abs_in + (1.0f - alpha_slow) * state->slow_env[c];
            float trans_diff = state->fast_env[c] - state->slow_env[c];
            float trans_amount = fmaxf(0.0f, trans_diff) * state->params.trans_sens;
            // For analysis: // printf("Ch%d Frame%zu: Trans_amount = %f\n", c, f, trans_amount);

            // Modulate drive for transient boost
            float eff_drive = state->params.drive * (1.0f + trans_amount);
            float eff_post_att = 1.0f / eff_drive;  // Dynamic post-attenuate to compensate

            // Stage 2: High-pass filter on wet path
            float x = dry;
            float hp = state->a0 * x + state->a1 * state->x1[c] + state->a2 * state->x2[c]
                       + state->b1 * state->y1[c] + state->b2 * state->y2[c];

            // Update filter state
            state->x2[c] = state->x1[c];
            state->x1[c] = x;
            state->y2[c] = state->y1[c];
            state->y1[c] = hp;

            // Stage 3: Apply effective pre-gain
            float boosted = hp * eff_drive;
            // For analysis: // printf("Ch%d Frame%zu: Boosted = %f\n", c, f, boosted);

            // Stage 4: Soft clip
            float clipped = asymmetric_soft_clip(boosted, state->params.clip_k);
            // For analysis: // printf("Ch%d Frame%zu: Clipped = %f\n", c, f, clipped);

            // Stage 5: Effective post-attenuate
            float wet = clipped * eff_post_att;
            // For analysis: // printf("Ch%d Frame%zu: Wet = %f\n", c, f, wet);

            // Stage 6: Mix with dry
            output[idx] = dry * (1.0f - state->params.mix) + wet * state->params.mix;
        }
    }
}
#endif // SIT_AUX_EXCITER_H
