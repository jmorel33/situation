/***************************************************************************************************
*
*   sit/aud/dynamics.h - Advanced Dynamics Processor (Compressor/Limiter/Gate)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Professional dynamics processor featuring:
*   - Compressor with soft knee
*   - Enhanced limiter with lookahead, peak hold, and smooth gain reduction
*   - Gate with adjustable threshold
*   - Expander for dynamic range expansion
*   
*   The limiter mode uses a sophisticated algorithm with:
*   - 1ms lookahead delay for transparent limiting
*   - Peak hold with decay for smoother response
*   - Dual-stage envelope follower (attack/release)
*   - Smooth gain interpolation to prevent clicks
*   - Hard clipping protection at ±0.999
*
***************************************************************************************************/

#ifndef SITUATION_DYNAMICS_H
#define SITUATION_DYNAMICS_H

#include <math.h>
#include <string.h>
#include <stdbool.h>

#ifndef DYNAMICS_MAX_LOOKAHEAD
#define DYNAMICS_MAX_LOOKAHEAD 256  // Max lookahead buffer size (samples)
#endif

// FMA detection and optimization
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define DYNAMICS_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
#else
    #define DYNAMICS_FMA(a, b, c) ((a) * (b) + (c))
#endif

// ================================================================================================
// DYNAMICS MODES
// ================================================================================================

typedef enum {
    DYNAMICS_COMPRESSOR,
    DYNAMICS_LIMITER,
    DYNAMICS_GATE,
    DYNAMICS_EXPANDER
} DynamicsMode;

// ================================================================================================
// DYNAMICS STATE
// ================================================================================================

typedef struct {
    DynamicsMode mode;
    float sample_rate;
    bool initialized;
    
    // Parameters
    float threshold_db;
    float ratio;
    float attack_ms;
    float release_ms;
    float knee_db;
    float makeup_gain_db;
    
    // Envelope follower state
    float envelope_l;
    float envelope_r;
    float attack_coeff;
    float release_coeff;
    
    // Enhanced limiter state
    float threshold;           // Linear threshold (not dB)
    float makeup_gain;         // Linear makeup gain
    int delay_samples;         // Lookahead delay in samples
    int buffer_capacity;       // Delay buffer capacity
    float* delay_line_l;       // Lookahead delay buffer (left)
    float* delay_line_r;       // Lookahead delay buffer (right)
    int delay_write_pos;       // Circular buffer write position
    float envelope;            // Limiter envelope
    float smooth_gain;         // Smoothed gain reduction
    float target_gain;         // Target gain reduction
    float peak_hold;           // Peak hold value
    int peak_hold_samples;     // Peak hold counter
    float release_ms_cache;    // Cached release time
} SituationDynamics;

// ================================================================================================
// INITIALIZATION
// ================================================================================================

/**
 * @brief Initialize dynamics processor.
 * @param dyn Pointer to dynamics structure.
 * @param sample_rate Sample rate in Hz.
 */
static void dynamics_init(SituationDynamics* dyn, float sample_rate) {
    memset(dyn, 0, sizeof(SituationDynamics));
    dyn->sample_rate = sample_rate;
    dyn->mode = DYNAMICS_COMPRESSOR;
    dyn->threshold_db = -20.0f;
    dyn->ratio = 4.0f;
    dyn->attack_ms = 10.0f;
    dyn->release_ms = 100.0f;
    dyn->knee_db = 0.0f;
    dyn->makeup_gain_db = 0.0f;
    
    // Calculate coefficients
    dyn->attack_coeff = expf(-1.0f / (dyn->attack_ms * 0.001f * sample_rate));
    dyn->release_coeff = expf(-1.0f / (dyn->release_ms * 0.001f * sample_rate));
    
    // Enhanced limiter initialization
    dyn->threshold = powf(10.0f, dyn->threshold_db / 20.0f);
    dyn->makeup_gain = 1.0f / dyn->threshold;
    dyn->buffer_capacity = DYNAMICS_MAX_LOOKAHEAD;
    dyn->delay_samples = (int)(sample_rate * 0.001f);  // 1ms lookahead
    if (dyn->delay_samples >= dyn->buffer_capacity) {
        dyn->delay_samples = dyn->buffer_capacity - 1;
    }
    if (dyn->delay_samples < 1) dyn->delay_samples = 1;
    
    // Allocate lookahead buffers
    dyn->delay_line_l = (float*)SIT_CALLOC(dyn->buffer_capacity, sizeof(float));
    dyn->delay_line_r = (float*)SIT_CALLOC(dyn->buffer_capacity, sizeof(float));
    
    dyn->delay_write_pos = 0;
    dyn->envelope = 0.0f;
    dyn->smooth_gain = 1.0f;
    dyn->target_gain = 1.0f;
    dyn->peak_hold = 0.0f;
    dyn->peak_hold_samples = 0;
    dyn->release_ms_cache = dyn->release_ms;
    dyn->initialized = (dyn->delay_line_l && dyn->delay_line_r);
}

/**
 * @brief Cleanup dynamics processor (free buffers).
 * @param dyn Pointer to dynamics structure.
 */
static void dynamics_cleanup(SituationDynamics* dyn) {
    if (dyn->delay_line_l) {
        SIT_FREE(dyn->delay_line_l);
        dyn->delay_line_l = NULL;
    }
    if (dyn->delay_line_r) {
        SIT_FREE(dyn->delay_line_r);
        dyn->delay_line_r = NULL;
    }
    dyn->initialized = false;
}

// ================================================================================================
// PARAMETER SETTERS
// ================================================================================================

static void dynamics_set_threshold(SituationDynamics* dyn, float threshold_db) {
    dyn->threshold_db = threshold_db;
    dyn->threshold = powf(10.0f, threshold_db / 20.0f);
    dyn->makeup_gain = 1.0f / dyn->threshold;
}

static void dynamics_set_ratio(SituationDynamics* dyn, float ratio) {
    dyn->ratio = ratio;
}

static void dynamics_set_attack(SituationDynamics* dyn, float attack_ms) {
    dyn->attack_ms = attack_ms;
    dyn->attack_coeff = expf(-1.0f / (attack_ms * 0.001f * dyn->sample_rate));
}

static void dynamics_set_release(SituationDynamics* dyn, float release_ms) {
    dyn->release_ms = release_ms;
    dyn->release_coeff = expf(-1.0f / (release_ms * 0.001f * dyn->sample_rate));
    dyn->release_ms_cache = release_ms;
}

static void dynamics_set_knee(SituationDynamics* dyn, float knee_db) {
    dyn->knee_db = knee_db;
}

static void dynamics_set_makeup_gain(SituationDynamics* dyn, float makeup_gain_db) {
    dyn->makeup_gain_db = makeup_gain_db;
}

static void dynamics_set_mode(SituationDynamics* dyn, DynamicsMode mode) {
    dyn->mode = mode;
    
    // Adjust parameters for limiter mode
    if (mode == DYNAMICS_LIMITER) {
        dyn->ratio = 20.0f;  // Very high ratio for limiting
        dyn->attack_ms = 0.1f;  // Very fast attack
        dyn->attack_coeff = expf(-1.0f / (dyn->attack_ms * 0.001f * dyn->sample_rate));
    }
}

// ================================================================================================
// ENHANCED LIMITER PROCESSING
// ================================================================================================

/**
 * @brief Process enhanced limiter with lookahead (per-sample).
 * @param dyn Pointer to dynamics structure.
 * @param input_l Left input sample.
 * @param input_r Right input sample.
 * @param output_l Pointer to left output sample.
 * @param output_r Pointer to right output sample.
 */
static void dynamics_process_limiter_sample(SituationDynamics* dyn, float input_l, float input_r, 
                                           float* output_l, float* output_r) {
    if (!dyn->initialized) {
        *output_l = input_l * 0.5f;
        *output_r = input_r * 0.5f;
        return;
    }
    
    // Store input in delay line
    dyn->delay_line_l[dyn->delay_write_pos] = input_l;
    dyn->delay_line_r[dyn->delay_write_pos] = input_r;
    
    // Calculate read position for lookahead
    int cap = dyn->buffer_capacity;
    int read_pos = (dyn->delay_write_pos - dyn->delay_samples + cap) % cap;
    float delayed_l = dyn->delay_line_l[read_pos];
    float delayed_r = dyn->delay_line_r[read_pos];
    
    // Advance write position
    dyn->delay_write_pos = (dyn->delay_write_pos + 1) % cap;
    
    // Peak detection on current (non-delayed) input
    float input_peak = fmaxf(fabsf(input_l), fabsf(input_r));
    
    // Peak hold with decay for smoother response
    if (input_peak > dyn->peak_hold) {
        dyn->peak_hold = input_peak;
        dyn->peak_hold_samples = (int)(dyn->sample_rate * 0.002f);  // 2ms hold
    } else if (dyn->peak_hold_samples > 0) {
        dyn->peak_hold_samples--;
    } else {
        dyn->peak_hold *= 0.999f;
    }
    
    // Use peak hold for envelope detection
    float detection_level = dyn->peak_hold;
    
    // Envelope follower with attack/release using FMA
    if (detection_level > dyn->envelope) {
        dyn->envelope = DYNAMICS_FMA(dyn->attack_coeff, dyn->envelope - detection_level, detection_level);
    } else {
        dyn->envelope = DYNAMICS_FMA(dyn->release_coeff, dyn->envelope - detection_level, detection_level);
    }
    
    // Calculate gain reduction
    float gain_reduction = 1.0f;
    if (dyn->envelope > dyn->threshold) {
        float over_threshold = dyn->envelope - dyn->threshold;
        float compressed_over = over_threshold / dyn->ratio;
        float target_level = dyn->threshold + compressed_over;
        gain_reduction = target_level / dyn->envelope;
        
        // Hard limit to threshold
        if (gain_reduction * dyn->envelope > dyn->threshold) {
            gain_reduction = dyn->threshold / dyn->envelope;
        }
    }
    
    // Smooth gain changes to prevent clicks using FMA
    dyn->target_gain = gain_reduction;
    float gain_smooth_coeff = 0.99f;
    dyn->smooth_gain = DYNAMICS_FMA(gain_smooth_coeff, dyn->smooth_gain - dyn->target_gain, dyn->target_gain);
    
    // Apply limiting with makeup gain
    float final_gain = dyn->smooth_gain * dyn->makeup_gain;
    *output_l = fmaxf(-0.999f, fminf(0.999f, delayed_l * final_gain));
    *output_r = fmaxf(-0.999f, fminf(0.999f, delayed_r * final_gain));
}

// ================================================================================================
// STANDARD COMPRESSOR/GATE PROCESSING
// ================================================================================================

/**
 * @brief Process standard compressor/gate (per-sample).
 * @param dyn Pointer to dynamics structure.
 * @param input_l Left input sample.
 * @param input_r Right input sample.
 * @param output_l Pointer to left output sample.
 * @param output_r Pointer to right output sample.
 */
static void dynamics_process_standard_sample(SituationDynamics* dyn, float input_l, float input_r,
                                            float* output_l, float* output_r) {
    // Calculate input level in dB
    float level_l = 20.0f * log10f(fabsf(input_l) + 1e-10f);
    float level_r = 20.0f * log10f(fabsf(input_r) + 1e-10f);
    float level = fmaxf(level_l, level_r);
    
    // Envelope follower using FMA
    float target_envelope = level;
    if (target_envelope > dyn->envelope_l) {
        dyn->envelope_l = DYNAMICS_FMA(dyn->attack_coeff, dyn->envelope_l, (1.0f - dyn->attack_coeff) * target_envelope);
    } else {
        dyn->envelope_l = DYNAMICS_FMA(dyn->release_coeff, dyn->envelope_l, (1.0f - dyn->release_coeff) * target_envelope);
    }
    
    // Calculate gain reduction
    float gain_reduction_db = 0.0f;
    float over_threshold = dyn->envelope_l - dyn->threshold_db;
    
    if (dyn->mode == DYNAMICS_COMPRESSOR) {
        if (over_threshold > 0.0f) {
            // Soft knee
            if (dyn->knee_db > 0.0f && over_threshold < dyn->knee_db) {
                float knee_factor = over_threshold / dyn->knee_db;
                gain_reduction_db = knee_factor * knee_factor * over_threshold * (1.0f - 1.0f / dyn->ratio);
            } else {
                gain_reduction_db = over_threshold * (1.0f - 1.0f / dyn->ratio);
            }
        }
    } else if (dyn->mode == DYNAMICS_GATE) {
        if (over_threshold < 0.0f) {
            gain_reduction_db = over_threshold * (dyn->ratio - 1.0f);
        }
    } else if (dyn->mode == DYNAMICS_EXPANDER) {
        if (over_threshold < 0.0f) {
            gain_reduction_db = over_threshold * (dyn->ratio - 1.0f);
        }
    }
    
    // Apply gain reduction with makeup gain
    float makeup_gain = powf(10.0f, dyn->makeup_gain_db / 20.0f);
    float gain = powf(10.0f, -gain_reduction_db / 20.0f) * makeup_gain;
    
    *output_l = input_l * gain;
    *output_r = input_r * gain;
}

// ================================================================================================
// PUBLIC PROCESSING FUNCTION
// ================================================================================================

/**
 * @brief Process audio through dynamics processor.
 * @param dyn Pointer to dynamics structure.
 * @param input Input audio buffer.
 * @param output Output audio buffer.
 * @param frames Number of frames to process.
 * @param channels Number of channels (1 or 2).
 */
static void dynamics_process(SituationDynamics* dyn, const float* input, float* output, 
                            int frames, int channels) {
    for (int i = 0; i < frames; i++) {
        float in_l = input[i * channels];
        float in_r = (channels == 2) ? input[i * channels + 1] : in_l;
        float out_l, out_r;
        
        // Route to appropriate processor
        if (dyn->mode == DYNAMICS_LIMITER) {
            dynamics_process_limiter_sample(dyn, in_l, in_r, &out_l, &out_r);
        } else {
            dynamics_process_standard_sample(dyn, in_l, in_r, &out_l, &out_r);
        }
        
        output[i * channels] = out_l;
        if (channels == 2) {
            output[i * channels + 1] = out_r;
        }
    }
}

#endif // SITUATION_DYNAMICS_H
