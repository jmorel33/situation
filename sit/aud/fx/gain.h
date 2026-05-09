/***************************************************************************************************
*
*   sit/aud/fx/gain.h - Simple Gain Node
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Trivial utility node: multiplies input by a gain factor with smoothing to avoid clicks.
*   Used as pre-insert and post-insert gain staging in the signal path.
*
*   Controls:
*     [0] gain — linear gain factor (0.0 to 4.0, default 1.0)
*
***************************************************************************************************/

#ifndef SITUATION_GAIN_H
#define SITUATION_GAIN_H

#include <math.h>

// ================================================================================================
// STATE
// ================================================================================================

typedef struct {
    float current_gain;   // Smoothed gain (for click-free changes)
    float target_gain;    // Target gain from control_values
    float smooth_coeff;   // Smoothing coefficient (one-pole filter)
} SituationGainState;

// ================================================================================================
// FUNCTIONS
// ================================================================================================

static inline void situation_gain_init(SituationGainState* state, float sample_rate) {
    state->current_gain = 1.0f;
    state->target_gain = 1.0f;
    // ~5ms smoothing time constant
    float tau = 0.005f * sample_rate;
    state->smooth_coeff = (tau > 1.0f) ? (1.0f / tau) : 1.0f;
}

static inline void situation_gain_process(
    SituationGainState* state,
    const float* input,
    float* output,
    int frames,
    int channels,
    float target_gain
) {
    state->target_gain = target_gain;
    
    for (int i = 0; i < frames * channels; i++) {
        // One-pole smoothing toward target
        state->current_gain += state->smooth_coeff * (state->target_gain - state->current_gain);
        output[i] = input[i] * state->current_gain;
    }
}

#endif // SITUATION_GAIN_H
