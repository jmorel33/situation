/***************************************************************************************************
*
*   phaseshifter.h - Phase Shifter Effect
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Phase shifter effect with feedback delay and stereo widening.
*   This file is intended to be included within the audio subsystem implementation.
*
***************************************************************************************************/

#ifndef SIT_AUX_PHASESHIFTER_H
#define SIT_AUX_PHASESHIFTER_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// FMA detection
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define PHASER_HAS_FMA 1
    #if defined(__GNUC__) || defined(__clang__)
        #define PHASER_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
    #else
        #define PHASER_FMA(a, b, c) fmaf((a), (b), (c))
    #endif
#else
    #define PHASER_HAS_FMA 0
    #define PHASER_FMA(a, b, c) ((a) * (b) + (c))
#endif

#define MAX_DELAY_SAMPLES 5000  // For feedback delay enhancement

typedef struct PhaseShifter {
    float sample_rate;
    float lfo_freq;
    float lfo_phase;
    float lfo_phase_offset;
    float a_base;
    float a_depth;
    float feedback;
    float mix;
    float pan_depth;          // Pan depth (0.0 to 1.0)
    float stereo_width;       // Stereo width (0.0 to 2.0)
    float apf_state[2][4][2]; // Filter states: [channel][filter][x[n-1], y[n-1]]
    float previous_filtered[2]; // For feedback
    float delay_buffer[2][MAX_DELAY_SAMPLES]; // Delay buffers for enhancement
    int delay_index[2];
    float feedback_delay_ms;  // Feedback delay in milliseconds
    int delay_samples;        // Feedback delay in samples
} PhaseShifter;

// Initialization
static void initPhaseShifter(PhaseShifter *ps, float sample_rate, float initial_rate, float initial_feedback, float initial_mix, float initial_pan_depth, float initial_stereo_width, float initial_feedback_delay_ms);

// Setters with clamping
static void setFeedbackDelayMs(PhaseShifter *ps, float feedback_delay_ms);
static void setRate(PhaseShifter *ps, float rate);
static void setFeedback(PhaseShifter *ps, float feedback);
static void setMix(PhaseShifter *ps, float mix);
static void setPanDepth(PhaseShifter *ps, float pan_depth);
static void setStereoWidth(PhaseShifter *ps, float stereo_width);

// Processing
static void processBuffer(PhaseShifter *ps, float *buffer, int num_samples, int num_channels);

static int phaseshifter_get_latency_samples(PhaseShifter *ps);

// Initialize the phase shifter
static void initPhaseShifter(PhaseShifter *ps, float sample_rate, float initial_rate, float initial_feedback, float initial_mix, float initial_pan_depth, float initial_stereo_width, float initial_feedback_delay_ms) {
    ps->sample_rate = sample_rate;
    ps->lfo_freq = initial_rate;
    ps->lfo_phase = 0.0f;
    ps->lfo_phase_offset = (float)M_PI;
    ps->a_base = 0.5f;
    ps->a_depth = 0.4f;
    ps->feedback = initial_feedback;
    ps->mix = initial_mix;
    ps->pan_depth = initial_pan_depth;
    ps->stereo_width = initial_stereo_width;
    ps->feedback_delay_ms = initial_feedback_delay_ms;
    ps->delay_samples = (int)(initial_feedback_delay_ms * sample_rate / 1000.0f);
    if (ps->delay_samples < 1) ps->delay_samples = 1;
    if (ps->delay_samples > MAX_DELAY_SAMPLES) ps->delay_samples = MAX_DELAY_SAMPLES;
    for (int c = 0; c < 2; c++) {
        for (int f = 0; f < 4; f++) {
            ps->apf_state[c][f][0] = 0.0f;
            ps->apf_state[c][f][1] = 0.0f;
        }
        ps->previous_filtered[c] = 0.0f;
        for (int d = 0; d < MAX_DELAY_SAMPLES; d++) {
            ps->delay_buffer[c][d] = 0.0f;
        }
        ps->delay_index[c] = 0;
    }
}

// Setter functions with clamping
static void setRate(PhaseShifter *ps, float rate) {
    ps->lfo_freq = rate;
}

static void setFeedback(PhaseShifter *ps, float feedback) {
    ps->feedback = (feedback < 0.0f) ? 0.0f : (feedback > 0.99f) ? 0.99f : feedback;
}

static void setMix(PhaseShifter *ps, float mix) {
    ps->mix = (mix < 0.0f) ? 0.0f : (mix > 1.0f) ? 1.0f : mix;
}

static void setPanDepth(PhaseShifter *ps, float pan_depth) {
    ps->pan_depth = (pan_depth < 0.0f) ? 0.0f : (pan_depth > 1.0f) ? 1.0f : pan_depth;
}

static void setStereoWidth(PhaseShifter *ps, float stereo_width) {
    ps->stereo_width = (stereo_width < 0.0f) ? 0.0f : (stereo_width > 2.0f) ? 2.0f : stereo_width;
}

static void setFeedbackDelayMs(PhaseShifter *ps, float feedback_delay_ms) {
    if (feedback_delay_ms < 0.0f) feedback_delay_ms = 0.0f;    // No negative delays
    if (feedback_delay_ms > 100.0f) feedback_delay_ms = 100.0f; // Cap at 100 ms
    ps->feedback_delay_ms = feedback_delay_ms;
    ps->delay_samples = (int)(feedback_delay_ms * ps->sample_rate / 1000.0f);
    if (ps->delay_samples < 1) ps->delay_samples = 1;
    if (ps->delay_samples > MAX_DELAY_SAMPLES) ps->delay_samples = MAX_DELAY_SAMPLES;
}

/**
 * Process an interleaved stereo audio buffer with pan depth and stereo widening
 * @param ps Pointer to PhaseShifter instance
 * @param buffer Interleaved audio samples (L0, R0, L1, R1, ...)
 * @param num_samples Number of frames (one frame = L + R sample)
 * @param num_channels Number of channels (should be 2 for stereo)
 */
// Process the buffer
static void processBuffer(PhaseShifter *ps, float *buffer, int num_samples, int num_channels) {
    for (int i = 0; i < num_samples; i++) {
        float input_left = buffer[i * num_channels + 0];
        float input_right = (num_channels > 1) ? buffer[i * num_channels + 1] : input_left;  // Handle mono

        // Process phasing for both internal channels
        float x[2];
        for (int c = 0; c < 2; c++) {
            float input = (c == 0) ? input_left : input_right;

            // LFO with offset for phasing
            float lfo_value_c = sinf(ps->lfo_phase + c * ps->lfo_phase_offset);
            float a = ps->a_base + ps->a_depth * lfo_value_c;
            if (a < 0.1f) a = 0.1f;
            if (a > 0.9f) a = 0.9f;

            // Feedback with delay
            int read_index = (ps->delay_index[c] - ps->delay_samples + MAX_DELAY_SAMPLES) % MAX_DELAY_SAMPLES;
            float delayed_feedback = ps->delay_buffer[c][read_index];
            float filtered_input = input + ps->feedback * delayed_feedback;

            // Process 4 all-pass filters
            float temp_x = filtered_input;
            for (int f = 0; f < 4; f++) {
                float y = a * temp_x + ps->apf_state[c][f][0] - a * ps->apf_state[c][f][1];
                ps->apf_state[c][f][0] = temp_x;
                ps->apf_state[c][f][1] = y;
                temp_x = y;
            }
            x[c] = temp_x;

            // Update delay buffer
            ps->delay_buffer[c][ps->delay_index[c]] = x[c];
            ps->delay_index[c] = (ps->delay_index[c] + 1) % MAX_DELAY_SAMPLES;
        }

        // FMA-optimized dry mix base
        float dry_gain = 1.0f - ps->mix;
        float output_left = input_left * dry_gain;
        float output_right = input_right * dry_gain;

        // Per-channel panning with cross-mixing for circular effect
        for (int c = 0; c < 2; c++) {
            // Use channel-offset LFO for panning (creates opposing spin)
            float lfo_value_pan = sinf(ps->lfo_phase + c * ps->lfo_phase_offset);
            float balance = lfo_value_pan * ps->pan_depth;
            float pan_position = (balance + 1.0f) / 2.0f;

            // Constant-power panning law
            float gain_to_left = cosf(pan_position * (float)M_PI / 2.0f);
            float gain_to_right = sinf(pan_position * (float)M_PI / 2.0f);

            // FMA-optimized cross-mix
            output_left = PHASER_FMA(x[c] * gain_to_left, ps->mix, output_left);
            output_right = PHASER_FMA(x[c] * gain_to_right, ps->mix, output_right);
        }

        // Stereo widening
        float mid = (output_left + output_right) / 2.0f;
        float side = (output_left - output_right) / 2.0f;
        output_left = PHASER_FMA(ps->stereo_width, side, mid);
        output_right = mid - ps->stereo_width * side;

        // Hard limiter
        output_left = fmaxf(-1.0f, fminf(1.0f, output_left));
        output_right = fmaxf(-1.0f, fminf(1.0f, output_right));

        // Write back
        buffer[i * num_channels + 0] = output_left;
        if (num_channels > 1) buffer[i * num_channels + 1] = output_right;

        // Advance LFO
        ps->lfo_phase += 2.0f * (float)M_PI * ps->lfo_freq / ps->sample_rate;
        if (ps->lfo_phase > 2.0f * (float)M_PI) ps->lfo_phase -= 2.0f * (float)M_PI;
    }
}

static int phaseshifter_get_latency_samples(PhaseShifter *ps) {
    (void)ps;
    return 0; // Sample-by-sample processing with dry signal included
}
#endif // SIT_AUX_PHASESHIFTER_H
