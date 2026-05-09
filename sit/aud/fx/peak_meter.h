/***************************************************************************************************
*
*   sit/aud/fx/peak_meter.h - Peak Meter (Analyzer)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Read-only analyzer: tracks peak and RMS levels per channel with ballistic decay.
*   Audio passes through unmodified. State is polled from the main thread for UI display.
*
*   No controls — this is a passive tap node.
*
***************************************************************************************************/

#ifndef SITUATION_PEAK_METER_H
#define SITUATION_PEAK_METER_H

#include <math.h>
#include <string.h>

// ================================================================================================
// STATE
// ================================================================================================

#define SIT_PEAK_METER_RMS_WINDOW 128

typedef struct {
    float peak_l, peak_r;           // Current peak (decays over time)
    float rms_l, rms_r;             // RMS level
    float decay_rate;               // Peak decay per sample (linear)
    float rms_sum_l, rms_sum_r;     // Running sum of squares
    float rms_window_l[SIT_PEAK_METER_RMS_WINDOW];
    float rms_window_r[SIT_PEAK_METER_RMS_WINDOW];
    int rms_pos;                    // Circular buffer write position
} SituationPeakMeterState;

// ================================================================================================
// FUNCTIONS
// ================================================================================================

static inline void situation_peak_meter_init(SituationPeakMeterState* state, float sample_rate) {
    state->peak_l = 0.0f;
    state->peak_r = 0.0f;
    state->rms_l = 0.0f;
    state->rms_r = 0.0f;
    state->rms_sum_l = 0.0f;
    state->rms_sum_r = 0.0f;
    state->rms_pos = 0;
    memset(state->rms_window_l, 0, sizeof(state->rms_window_l));
    memset(state->rms_window_r, 0, sizeof(state->rms_window_r));
    
    // Decay rate: ~300ms to fall from 1.0 to ~0.01 (ballistic meter standard)
    // decay per sample = 1.0 / (0.3 * sample_rate)
    state->decay_rate = 1.0f / (0.3f * sample_rate);
}

/**
 * @brief Process peak meter — reads audio, passes through, updates levels.
 * @param state Peak meter state.
 * @param input Input audio buffer (stereo interleaved).
 * @param output Output audio buffer (stereo interleaved — passthrough copy).
 * @param frames Number of frames.
 */
static inline void situation_peak_meter_process(
    SituationPeakMeterState* state,
    const float* input,
    float* output,
    int frames
) {
    for (int i = 0; i < frames; i++) {
        float l = input[i * 2];
        float r = input[i * 2 + 1];
        
        // Passthrough
        output[i * 2] = l;
        output[i * 2 + 1] = r;
        
        // Peak detection with decay
        float abs_l = (l < 0.0f) ? -l : l;
        float abs_r = (r < 0.0f) ? -r : r;
        
        if (abs_l > state->peak_l) {
            state->peak_l = abs_l;
        } else {
            state->peak_l -= state->decay_rate;
            if (state->peak_l < 0.0f) state->peak_l = 0.0f;
        }
        
        if (abs_r > state->peak_r) {
            state->peak_r = abs_r;
        } else {
            state->peak_r -= state->decay_rate;
            if (state->peak_r < 0.0f) state->peak_r = 0.0f;
        }
        
        // RMS (sliding window)
        float sq_l = l * l;
        float sq_r = r * r;
        
        // Remove oldest sample from sum, add new
        state->rms_sum_l -= state->rms_window_l[state->rms_pos];
        state->rms_sum_r -= state->rms_window_r[state->rms_pos];
        state->rms_window_l[state->rms_pos] = sq_l;
        state->rms_window_r[state->rms_pos] = sq_r;
        state->rms_sum_l += sq_l;
        state->rms_sum_r += sq_r;
        
        state->rms_pos = (state->rms_pos + 1) % SIT_PEAK_METER_RMS_WINDOW;
    }
    
    // Compute final RMS values
    state->rms_l = sqrtf(state->rms_sum_l / (float)SIT_PEAK_METER_RMS_WINDOW);
    state->rms_r = sqrtf(state->rms_sum_r / (float)SIT_PEAK_METER_RMS_WINDOW);
}

#endif // SITUATION_PEAK_METER_H
