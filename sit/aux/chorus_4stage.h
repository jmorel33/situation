/***************************************************************************************************
*
*   chorus_4stage.h - 4-Stage Chorus Effect
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Implementation of a 4-stage chorus effect with oversampling support.
*
***************************************************************************************************/

#ifndef SIT_AUX_CHORUS_4STAGE_H
#define SIT_AUX_CHORUS_4STAGE_H

#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef SIT_MALLOC
    #define SIT_MALLOC(sz) malloc(sz)
#endif
#ifndef SIT_FREE
    #define SIT_FREE(p) free(p)
#endif

// LFO waveform options
typedef enum {
    LFO_SINE,
    LFO_TRIANGLE,
    LFO_SAWTOOTH,
    LFO_SQUARE
} LFOShape;

// Chorus effect structure
typedef struct {
    float *delay_line_left;
    float *delay_line_right;
    int delay_line_size;
    int write_pos;
    float sample_rate;
    float base_delay[4];
    float lfo_freq[4];
    float lfo_depth[4];
    float lfo_phase_left[4];
    float lfo_phase_right[4];
    float pan[4];
    float width;           // LFO phase offset for stereo width
    float dry_gain;        // Dry signal gain
    float wet_gain;        // Wet signal gain
    float feedback;        // Feedback amount for delay lines
    LFOShape lfo_shape[4]; // LFO waveform per stage
    float stereo_enhance;  // Stereo width enhancement factor
} SituationChorus4Stage;

// Upsampling state for 4x oversampling
#define SIT_CHORUS_FILTER_LENGTH 16
typedef struct {
    float h[SIT_CHORUS_FILTER_LENGTH];   // Low-pass filter coefficients
    float dl[4];              // Delay line for input samples
} SituationChorusUpsampleState;

// Downsampling state for 4x oversampling
typedef struct {
    float h[SIT_CHORUS_FILTER_LENGTH];   // Low-pass filter coefficients
    float dl[SIT_CHORUS_FILTER_LENGTH];  // Delay line for oversampled samples
    int counter;              // Counts oversampled samples (0 to 3)
} SituationChorusDownsampleState;

// Compute LFO value based on waveform shape and phase
static float _sit_chorus_lfo_value(LFOShape shape, float phase) {
    float t = phase / (2.0f * (float)M_PI); // Normalize phase to [0,1)
    switch (shape) {
        case LFO_SINE:
            return sinf(phase);
        case LFO_TRIANGLE:
            if (t < 0.25f) return 4.0f * t;
            else if (t < 0.75f) return 2.0f - 4.0f * t;
            else return 4.0f * t - 4.0f;
        case LFO_SAWTOOTH:
            return 2.0f * t - 1.0f;
        case LFO_SQUARE:
            return (t < 0.5f) ? 1.0f : -1.0f;
        default:
            return 0.0f;
    }
}

// Compute windowed sinc filter coefficients for oversampling
static void _sit_chorus_compute_filter_coeffs(float h[SIT_CHORUS_FILTER_LENGTH]) {
    const int N = SIT_CHORUS_FILTER_LENGTH;
    const float omega_c = (float)M_PI / 4.0f; // Cutoff at pi/4 for 4x oversampling
    const float delay = (N - 1.0f) / 2.0f; // Center of filter
    float sum = 0.0f;
    for (int n = 0; n < N; n++) {
        float x = n - delay;
        if (fabsf(x) < 1e-6f) {
            h[n] = omega_c / (float)M_PI; // Sinc limit at zero
        } else {
            h[n] = (sinf(omega_c * x)) / ((float)M_PI * x);
        }
        // Apply Hamming window
        float w = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * n / (N - 1));
        h[n] *= w;
        sum += h[n];
    }
    // Normalize for 4x oversampling gain
    for (int n = 0; n < N; n++) {
        h[n] *= 4.0f / sum;
    }
}

// Initialize chorus effect
static void SituationChorus4Stage_Init(SituationChorus4Stage *effect, float sample_rate, int max_delay_samples) {
    effect->delay_line_size = max_delay_samples;
    effect->delay_line_left = (float *)SIT_MALLOC(max_delay_samples * sizeof(float));
    effect->delay_line_right = (float *)SIT_MALLOC(max_delay_samples * sizeof(float));
    effect->write_pos = 0;
    effect->sample_rate = sample_rate;

    for (int i = 0; i < max_delay_samples; i++) {
        effect->delay_line_left[i] = 0.0f;
        effect->delay_line_right[i] = 0.0f;
    }

    for (int stage = 0; stage < 4; stage++) {
        effect->base_delay[stage] = 0.0f;
        effect->lfo_freq[stage] = 0.0f;
        effect->lfo_depth[stage] = 0.0f;
        effect->lfo_phase_left[stage] = 0.0f;
        effect->lfo_phase_right[stage] = (stage + 1) * 0.25f;
        effect->pan[stage] = 0.0f;
        effect->lfo_shape[stage] = LFO_SINE;
    }

    effect->width = 0.0f;
    effect->dry_gain = 1.0f;
    effect->wet_gain = 0.5f;
    effect->feedback = 0.0f;
    effect->stereo_enhance = 1.0f;
}

// Set stage parameters
static void SituationChorus4Stage_SetStageParams(SituationChorus4Stage *effect, int stage, float base_delay_ms, float lfo_freq, float lfo_depth_ms, float pan) {
    if (stage < 0 || stage >= 4) return;

    effect->base_delay[stage] = base_delay_ms * effect->sample_rate / 1000.0f;
    effect->lfo_freq[stage] = lfo_freq;
    effect->lfo_depth[stage] = lfo_depth_ms * effect->sample_rate / 1000.0f;
    effect->pan[stage] = pan;
}

// Set stereo width (LFO phase offset)
static void SituationChorus4Stage_SetWidth(SituationChorus4Stage *effect, float width) {
    if (width < 0.0f) width = 0.0f;
    if (width > 1.0f) width = 1.0f;
    effect->width = width;
    float phase_offset = width * ((float)M_PI / 2.0f);
    for (int stage = 0; stage < 4; stage++) {
        effect->lfo_phase_right[stage] = fmodf(effect->lfo_phase_left[stage] + phase_offset, 2.0f * (float)M_PI);
    }
}

// Set dry gain
static void SituationChorus4Stage_SetDryGain(SituationChorus4Stage *effect, float dry_gain) {
    effect->dry_gain = dry_gain;
}

// Set wet gain
static void SituationChorus4Stage_SetWetGain(SituationChorus4Stage *effect, float wet_gain) {
    effect->wet_gain = wet_gain;
}

// Set feedback amount
static void SituationChorus4Stage_SetFeedback(SituationChorus4Stage *effect, float feedback) {
    if (feedback < 0.0f) feedback = 0.0f;
    if (feedback > 0.99f) feedback = 0.99f;
    effect->feedback = feedback;
}

// Set LFO waveform for a stage
static void SituationChorus4Stage_SetLfoShape(SituationChorus4Stage *effect, int stage, LFOShape shape) {
    if (stage >= 0 && stage < 4) {
        effect->lfo_shape[stage] = shape;
    }
}

// Set stereo enhancement factor
static void SituationChorus4Stage_SetStereoEnhance(SituationChorus4Stage *effect, float enhance) {
    if (enhance < 0.0f) enhance = 0.0f;
    effect->stereo_enhance = enhance;
}

// Process audio with all improvements
static void SituationChorus4Stage_Process(SituationChorus4Stage *effect, float *input_left, float *input_right, float *output_left, float *output_right, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        float sum_left = 0.0f;
        float sum_right = 0.0f;

        for (int stage = 0; stage < 4; stage++) {
            // Update LFO phases
            effect->lfo_phase_left[stage] += 2.0f * (float)M_PI * effect->lfo_freq[stage] / effect->sample_rate;
            effect->lfo_phase_right[stage] += 2.0f * (float)M_PI * effect->lfo_freq[stage] / effect->sample_rate;
            if (effect->lfo_phase_left[stage] > 2.0f * (float)M_PI) effect->lfo_phase_left[stage] -= 2.0f * (float)M_PI;
            if (effect->lfo_phase_right[stage] > 2.0f * (float)M_PI) effect->lfo_phase_right[stage] -= 2.0f * (float)M_PI;

            // Compute modulated delay times with variable LFO
            float mod_left = _sit_chorus_lfo_value(effect->lfo_shape[stage], effect->lfo_phase_left[stage]) * effect->lfo_depth[stage];
            float mod_right = _sit_chorus_lfo_value(effect->lfo_shape[stage], effect->lfo_phase_right[stage]) * effect->lfo_depth[stage];
            float delay_left = effect->base_delay[stage] + mod_left;
            float delay_right = effect->base_delay[stage] + mod_right;

            // Fractional delay
            float frac_left = delay_left - floorf(delay_left);
            float frac_right = delay_right - floorf(delay_right);
            int int_delay_left = (int)delay_left;
            int int_delay_right = (int)delay_right;

            // Cubic interpolation indices
            int pos_m1_left = (effect->write_pos - int_delay_left - 1 + effect->delay_line_size) % effect->delay_line_size;
            int pos_0_left = (effect->write_pos - int_delay_left + effect->delay_line_size) % effect->delay_line_size;
            int pos_1_left = (effect->write_pos - int_delay_left + 1 + effect->delay_line_size) % effect->delay_line_size;
            int pos_2_left = (effect->write_pos - int_delay_left + 2 + effect->delay_line_size) % effect->delay_line_size;

            int pos_m1_right = (effect->write_pos - int_delay_right - 1 + effect->delay_line_size) % effect->delay_line_size;
            int pos_0_right = (effect->write_pos - int_delay_right + effect->delay_line_size) % effect->delay_line_size;
            int pos_1_right = (effect->write_pos - int_delay_right + 1 + effect->delay_line_size) % effect->delay_line_size;
            int pos_2_right = (effect->write_pos - int_delay_right + 2 + effect->delay_line_size) % effect->delay_line_size;

            // Read delay line samples
            float ym1_left = effect->delay_line_left[pos_m1_left];
            float y0_left = effect->delay_line_left[pos_0_left];
            float y1_left = effect->delay_line_left[pos_1_left];
            float y2_left = effect->delay_line_left[pos_2_left];

            float ym1_right = effect->delay_line_right[pos_m1_right];
            float y0_right = effect->delay_line_right[pos_0_right];
            float y1_right = effect->delay_line_right[pos_1_right];
            float y2_right = effect->delay_line_right[pos_2_right];

            // Compute cubic interpolation coefficients
            float a0_left = y0_left;
            float a1_left = 0.5f * (y1_left - ym1_left);
            float a2_left = ym1_left - 2.5f * y0_left + 2.0f * y1_left - 0.5f * y2_left;
            float a3_left = 0.5f * (y2_left - ym1_left) + 1.5f * (y0_left - y1_left);

            float a0_right = y0_right;
            float a1_right = 0.5f * (y1_right - ym1_right);
            float a2_right = ym1_right - 2.5f * y0_right + 2.0f * y1_right - 0.5f * y2_right;
            float a3_right = 0.5f * (y2_right - ym1_right) + 1.5f * (y0_right - y1_right);

            // Interpolate samples
            float t = frac_left;
            float sample_left = a0_left + t * (a1_left + t * (a2_left + t * a3_left));
            t = frac_right;
            float sample_right = a0_right + t * (a1_right + t * (a2_right + t * a3_right));

            // Apply constant power panning
            float pan = effect->pan[stage];
            float theta = (pan + 1.0f) * (float)M_PI / 4.0f; // Map [-1,1] to [0,pi/2]
            float left_gain = cosf(theta);
            float right_gain = sinf(theta);
            sum_left += sample_left * left_gain;
            sum_right += sample_right * right_gain;
        }

        // Mix dry and wet signals
        float wet_left = sum_left;
        float wet_right = sum_right;
        output_left[i] = effect->dry_gain * input_left[i] + effect->wet_gain * wet_left;
        output_right[i] = effect->dry_gain * input_right[i] + effect->wet_gain * wet_right;

        // Apply stereo enhancement via mid-side processing
        float mid = (output_left[i] + output_right[i]) / 2.0f;
        float side = (output_left[i] - output_right[i]) / 2.0f;
        side *= effect->stereo_enhance;
        output_left[i] = mid + side;
        output_right[i] = mid - side;

        // Update delay lines with feedback
        effect->delay_line_left[effect->write_pos] = input_left[i] + effect->feedback * wet_left;
        effect->delay_line_right[effect->write_pos] = input_right[i] + effect->feedback * wet_right;

        // Advance write position
        effect->write_pos = (effect->write_pos + 1) % effect->delay_line_size;
    }
}

// Free memory
static void SituationChorus4Stage_Free(SituationChorus4Stage *effect) {
    if (effect->delay_line_left) SIT_FREE(effect->delay_line_left);
    if (effect->delay_line_right) SIT_FREE(effect->delay_line_right);
}

// Initialize upsampling state
static void SituationChorus4Stage_UpsampleInit(SituationChorusUpsampleState *state) {
    _sit_chorus_compute_filter_coeffs(state->h);
    memset(state->dl, 0, sizeof(state->dl));
}

// Upsample one input to four outputs
static void SituationChorus4Stage_Upsample(float input, float output[4], SituationChorusUpsampleState *state) {
    state->dl[3] = state->dl[2];
    state->dl[2] = state->dl[1];
    state->dl[1] = state->dl[0];
    state->dl[0] = input;
    output[0] = state->h[0] * state->dl[0] + state->h[4] * state->dl[1] + state->h[8] * state->dl[2] + state->h[12] * state->dl[3];
    output[1] = state->h[1] * state->dl[0] + state->h[5] * state->dl[1] + state->h[9] * state->dl[2] + state->h[13] * state->dl[3];
    output[2] = state->h[2] * state->dl[0] + state->h[6] * state->dl[1] + state->h[10] * state->dl[2] + state->h[14] * state->dl[3];
    output[3] = state->h[3] * state->dl[0] + state->h[7] * state->dl[1] + state->h[11] * state->dl[2] + state->h[15] * state->dl[3];
}

// Initialize downsampling state
static void SituationChorus4Stage_DownsampleInit(SituationChorusDownsampleState *state) {
    _sit_chorus_compute_filter_coeffs(state->h);
    memset(state->dl, 0, sizeof(state->dl));
    state->counter = 0;
}

// Downsample one oversampled input
static float SituationChorus4Stage_Downsample(float input, SituationChorusDownsampleState *state, int *ready) {
    for (int k = SIT_CHORUS_FILTER_LENGTH - 1; k > 0; k--) {
        state->dl[k] = state->dl[k - 1];
    }
    state->dl[0] = input;
    state->counter = (state->counter + 1) % 4;
    if (state->counter == 0) {
        *ready = 1;
        float sum = 0.0f;
        for (int k = 0; k < SIT_CHORUS_FILTER_LENGTH; k++) {
            sum += state->h[k] * state->dl[k];
        }
        return sum;
    }
    *ready = 0;
    return 0.0f;
}

static int SituationChorus4Stage_GetLatencySamples(SituationChorus4Stage *effect) {
    (void)effect;
    return 0; // Sample-by-sample processing with dry signal included
}

#endif // SIT_AUX_CHORUS_4STAGE_H
