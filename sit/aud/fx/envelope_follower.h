/***************************************************************************************************
*
*   sit/aud/fx/envelope_follower.h - Envelope Follower (Modulator)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Classic envelope follower: rectifies input signal, smooths with attack/release
*   coefficients, outputs a control signal (0.0–1.0) representing the amplitude envelope.
*
*   Used for sidechain-style modulation — connect its control output to a Dynamics node's
*   control input, or to any parameter that should track input level.
*
*   Controls:
*     [0] attack    — attack time in seconds (0.001–0.5, default 0.01)
*     [1] release   — release time in seconds (0.01–2.0, default 0.1)
*     [2] sensitivity — input gain before detection (0.1–10.0, default 1.0)
*
***************************************************************************************************/

#ifndef SITUATION_ENVELOPE_FOLLOWER_H
#define SITUATION_ENVELOPE_FOLLOWER_H

#include <math.h>

// ================================================================================================
// STATE
// ================================================================================================

typedef struct {
    float envelope;         // Current envelope value (0.0–1.0)
    float attack_coeff;     // Smoothing coefficient for rising signal
    float release_coeff;    // Smoothing coefficient for falling signal
    float sample_rate;      // For recalculating coefficients
} SituationEnvelopeFollowerState;

// ================================================================================================
// FUNCTIONS
// ================================================================================================

static inline float _sit_envf_time_to_coeff(float time_sec, float sample_rate) {
    if (time_sec <= 0.0f) return 1.0f;
    return 1.0f - expf(-1.0f / (time_sec * sample_rate));
}

static inline void situation_envf_init(SituationEnvelopeFollowerState* state, float sample_rate) {
    state->envelope = 0.0f;
    state->sample_rate = sample_rate;
    state->attack_coeff = _sit_envf_time_to_coeff(0.01f, sample_rate);
    state->release_coeff = _sit_envf_time_to_coeff(0.1f, sample_rate);
}

static inline void situation_envf_set_attack(SituationEnvelopeFollowerState* state, float attack_sec) {
    state->attack_coeff = _sit_envf_time_to_coeff(attack_sec, state->sample_rate);
}

static inline void situation_envf_set_release(SituationEnvelopeFollowerState* state, float release_sec) {
    state->release_coeff = _sit_envf_time_to_coeff(release_sec, state->sample_rate);
}

/**
 * @brief Process envelope follower.
 * @param state Envelope follower state.
 * @param input Input audio buffer (stereo interleaved).
 * @param output Output control signal buffer (mono — one value per frame).
 * @param frames Number of frames.
 * @param channels Number of input channels.
 * @param sensitivity Input gain multiplier.
 */
static inline void situation_envf_process(
    SituationEnvelopeFollowerState* state,
    const float* input,
    float* output,
    int frames,
    int channels,
    float sensitivity
) {
    for (int i = 0; i < frames; i++) {
        // Rectify: take absolute value of input (sum channels for stereo)
        float rectified = 0.0f;
        for (int c = 0; c < channels; c++) {
            float sample = input[i * channels + c];
            float abs_sample = (sample < 0.0f) ? -sample : sample;
            if (abs_sample > rectified) rectified = abs_sample;
        }
        
        // Apply sensitivity
        rectified *= sensitivity;
        
        // Clamp to 1.0
        if (rectified > 1.0f) rectified = 1.0f;
        
        // Smooth with attack/release
        float coeff;
        if (rectified > state->envelope) {
            coeff = state->attack_coeff;
        } else {
            coeff = state->release_coeff;
        }
        
        state->envelope += coeff * (rectified - state->envelope);
        
        // Output one value per frame
        output[i] = state->envelope;
    }
}

#endif // SITUATION_ENVELOPE_FOLLOWER_H
