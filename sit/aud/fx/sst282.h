/***************************************************************************************************
*
*   sst282.h - Internal SST-282 Reverb/Delay Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Implementation Details
*
*   Signal Flow
*   Input: Applies gain (0–10) and updates the peak level indicator (-30 to 0 dB).
*   EQ: Placeholder biquad filters for LF (20 Hz) and HF (7 kHz) shelving (0 to -10 dB). Replace with actual coefficients for production use.
*   Delay Buffer: Circular buffer with 4080 samples (255 ms at 16 kHz).
*   Audition Taps: 8 taps with programmable delays (1–255 ms) and paired levels (0–12 dB), odd to left, even to right.
*   Reverb/Echo:
*   Reverb: 16 randomized taps (20–100 ms for MEDIUM, 50–200 ms for LONG).
*   Echo: Single tap (30–256 ms).
*   Feedback: Applied to reverb/echo signals (0–10).
*   Output: Mixes direct, audition taps, and reverb/echo signals (0–12 dB each).
*
*   Preset System
*   The sst282_set_preset function allows you to set predefined configurations by name. Examples include:
*
*   "ROOM1": Simulates a small room with short delays (10–80 ms) and medium feedback (Image 6).
*   "ROOM4": Larger room with longer delays (50–225 ms) and higher feedback (Images 9, 12).
*   "ECHO": Long echo with a 250 ms delay and no audition tap levels (Images 7, 11).
*   "D4_COMB_SCI_FI": Comb filter effect from Image 14, with evenly spaced delays (6–24 ms) for sci-fi voices.
*   "E9_SPACE_REPEAT_3": Space Repeat 3 from Image 14, with 3 repeats (0–170 ms) and reverb.
*   These presets are derived from the manual’s descriptions and the specific settings in Images 13 and 14. You can expand this function with additional presets (e.g., "FATTY", "CLOUD", "R1_LARGE_ROOM") based on further manual details.
*
*   Notes
*   EQ: The biquad filters are placeholders. For accurate EQ, implement shelving filters using DSP formulas (e.g., RBJ Audio Cookbook) with corner frequencies at 20 Hz and 7 kHz.
*   Preset Flexibility: Unlike the hardware’s fixed programs, the software allows dynamic preset creation and adjustment via function calls.
*   Manual Integration: The presets reflect the front panel controls (Image 0) and the illustrated effects (Images 13, 14), ensuring fidelity to the SST-282’s capabilities.
*   How to Use
*   Integrate: Add sst282_process to your audio callback to process samples in real-time.
*   Set Presets: Call sst282_set_preset(&state, "ROOM1") to apply a preset before processing.
*   Customize: Use setter functions (e.g., sst282_set_feedback) to tweak parameters after setting a preset.
*   Expand: Add more presets to sst282_set_preset based on additional manual examples or your own designs.
*
***************************************************************************************************/

#ifndef SIT_AUX_SST282_H
#define SIT_AUX_SST282_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// FMA detection
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define SST282_HAS_FMA 1
    #if defined(__GNUC__) || defined(__clang__)
        #define SST282_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
    #else
        #define SST282_FMA(a, b, c) fmaf((a), (b), (c))
    #endif
#else
    #define SST282_HAS_FMA 0
    #define SST282_FMA(a, b, c) ((a) * (b) + (c))
#endif

#define SST282_MAX_DELAY_MS 255
#define SST282_NUM_REVERB_TAPS 16
#define SST282_INTERNAL_SAMPLE_RATE 16000.0f

typedef enum {
    SST282_ROOM1, SST282_ROOM2, SST282_ROOM3, SST282_ROOM4,
    SST282_COMB6, SST282_COMB10,
    SST282_FATTY, SST282_CLOUD, SST282_SLAP1, SST282_SLAP2,
    SST282_ECHO, SST282_REPEAT2, SST282_REPEAT3, SST282_REPEAT4
} SST282AuditionProgram;

typedef struct {
    float input_gain;          // Range: 0 to 10
    float lf_cut_dB;          // Range: 0 to 10 dB (low-frequency cut)
    float hf_cut_dB;          // Range: 0 to 10 dB (high-frequency cut)
    SST282AuditionProgram audition_program; // Preset for audition delay taps
    float echo_delay_ms;      // Range: 30 to 256 ms (echo mode only)
    float feedback;           // Range: 0 to 10
    float dry_dB;             // Range: 0 to 12 dB (dry signal level)
    float echo_dB;            // Range: 0 to 12 dB (echo/reverb level)
    float tap_levels[4];      // Range: 0 to 12 dB for each of the 4 tap pairs
    bool reverb_program_long; // False = MEDIUM, True = LONG (reverb length)
    bool reverb_mode;         // True = reverb, False = echo (mode switch)
    float direct_dB;          // Range: 0 to 12 dB (direct output level)
} SST282Params;

typedef struct {
    int delay_samples;
    float level;
} SST282Tap;

typedef struct {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
} SST282Biquad;

typedef struct {
    float *delay_buffer;
    int max_delay_samples;
    int write_index;
    SST282Tap audition_taps[8];
    float tap_levels[4];
    SST282Tap reverb_taps[SST282_NUM_REVERB_TAPS];
    SST282Tap echo_tap;
    float feedback_gain;
    float input_gain;
    float lf_cut_dB;
    float hf_cut_dB;
    float dry_level;
    float echo_level;
    float direct_level;
    bool reverb_mode;
    bool long_reverb;
    SST282Biquad lf_biquad;
    SST282Biquad hf_biquad;
    float peak_level;
    float stream_sample_rate;  // Audio stream’s sample rate
    int downsample_factor;     // e.g., 6 for 96 kHz
    float *input_buffer;       // Buffer for accumulating input samples
    int input_count;           // Number of samples accumulated
	float *left_output_buffer;  // Buffer for upsampled left output
	float *right_output_buffer; // Buffer for upsampled right output
	int output_index;           // Index into output buffers

    // Internal state for parameter tracking
    SST282Params previous_params;
    bool params_initialized;
} SST282State;

// Forward declarations
static int sst282_init(SST282State *state, float stream_sample_rate);
static void sst282_destroy(SST282State *state);
static void sst282_process(SST282State *state, float input, float *left_out, float *right_out);
static void sst282_set_audition_program(SST282State *state, SST282AuditionProgram program);
static void sst282_set_preset(SST282State *state, const char *preset_name);
static void sst282_set_reverb_program(SST282State *state, bool long_reverb);
static void sst282_set_input_gain(SST282State *state, float gain_0_to_10);
static void sst282_set_eq(SST282State *state, float lf_cut_dB, float hf_cut_dB);
static void sst282_set_tap_level(SST282State *state, int pair, float level_dB);
static void sst282_set_feedback(SST282State *state, float feedback_0_to_10);
static void sst282_set_mix(SST282State *state, float dry_dB, float echo_dB, float direct_dB);
static void sst282_set_mode(SST282State *state, bool reverb_mode);
static void sst282_set_echo_delay(SST282State *state, float delay_ms);
static void sst282_apply_params(SST282State *state, SST282Params *params);
static int sst282_get_latency_samples(SST282State* state);

// Helper function to convert dB to linear gain
static float sst282_db_to_gain(float dB) {
    return powf(10.0f, dB / 20.0f);
}

// Helper function to hard limit the audio signal so it doesn't click pop
static inline float sst282_hard_limit(float sample) {
    if (sample > 1.0f) return 1.0f;
    if (sample < -1.0f) return -1.0f;
    return sample;
}

// Biquad initialization
static void sst282_init_biquad(SST282Biquad *biquad, float freq, float gain_dB, float Q, float sample_rate) {
    float A = powf(10.0f, gain_dB / 40.0f);
    float w0 = 2.0f * (float)M_PI * freq / sample_rate;
    float alpha = sinf(w0) / (2.0f * Q);
    float b0, b1, b2, a0, a1, a2;
    // Shelving filter coefficients (simplified RBJ)
    b0 = A * ((A + 1.0f) - (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha);
    b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosf(w0));
    b2 = A * ((A + 1.0f) - (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha);
    a0 = (A + 1.0f) + (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha;
    a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosf(w0));
    a2 = (A + 1.0f) + (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha;
    biquad->b0 = b0 / a0; biquad->b1 = b1 / a0; biquad->b2 = b2 / a0;
    biquad->a1 = a1 / a0; biquad->a2 = a2 / a0;
}

static float sst282_apply_biquad(SST282Biquad *biquad, float input) {
    // FMA-optimized biquad: y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2
    float output = SST282_FMA(biquad->b0, input, SST282_FMA(biquad->b1, biquad->x1,
                   SST282_FMA(biquad->b2, biquad->x2, -SST282_FMA(biquad->a1, biquad->y1, biquad->a2 * biquad->y2))));
    biquad->x2 = biquad->x1; biquad->x1 = input;
    biquad->y2 = biquad->y1; biquad->y1 = output;
    return output;
}

// Resets state variables to defaults without re-allocating memory
static void sst282_reset(SST282State *state) {
    if (state->delay_buffer) memset(state->delay_buffer, 0, sizeof(float) * state->max_delay_samples);
    if (state->input_buffer) memset(state->input_buffer, 0, sizeof(float) * state->downsample_factor);
    if (state->left_output_buffer) memset(state->left_output_buffer, 0, sizeof(float) * state->downsample_factor);
    if (state->right_output_buffer) memset(state->right_output_buffer, 0, sizeof(float) * state->downsample_factor);

    state->write_index = 0;
    state->input_count = 0;

    // Default params
    state->echo_tap.delay_samples = (int)SST282_INTERNAL_SAMPLE_RATE;
    state->echo_tap.level = 1.0f;
    state->feedback_gain = 0.0f;
    state->input_gain = 0.5f;
    state->lf_cut_dB = 0.0f;
    state->hf_cut_dB = 0.0f;
    state->dry_level = 0.5f;
    state->echo_level = 0.5f;
    state->direct_level = 0.5f;
    state->reverb_mode = true;
    state->long_reverb = false;

    sst282_init_biquad(&state->lf_biquad, 20.0f, -state->lf_cut_dB, 0.707f, SST282_INTERNAL_SAMPLE_RATE);
    sst282_init_biquad(&state->hf_biquad, 7000.0f, -state->hf_cut_dB, 0.707f, SST282_INTERNAL_SAMPLE_RATE);
    state->peak_level = -30.0f;
    state->output_index = 0;

    // Zero taps to avoid garbage
    memset(state->audition_taps, 0, sizeof(state->audition_taps));
    memset(state->reverb_taps, 0, sizeof(state->reverb_taps));

    state->params_initialized = false;
    memset(&state->previous_params, 0, sizeof(SST282Params));
}

// Initialize the SST-282 state
static int sst282_init(SST282State *state, float stream_sample_rate) {
    state->stream_sample_rate = stream_sample_rate;
    state->downsample_factor = (int)(stream_sample_rate / SST282_INTERNAL_SAMPLE_RATE + 0.5f);
    state->max_delay_samples = (int)(SST282_MAX_DELAY_MS * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);

    state->delay_buffer = (float*)SIT_MALLOC(sizeof(float) * state->max_delay_samples);
    state->input_buffer = (float*)SIT_MALLOC(sizeof(float) * state->downsample_factor);
    state->left_output_buffer = (float*)SIT_MALLOC(sizeof(float) * state->downsample_factor);
    state->right_output_buffer = (float*)SIT_MALLOC(sizeof(float) * state->downsample_factor);

    if (!state->delay_buffer || !state->input_buffer ||
        !state->left_output_buffer || !state->right_output_buffer) {
        if(state->delay_buffer) SIT_FREE(state->delay_buffer);
        if(state->input_buffer) SIT_FREE(state->input_buffer);
        if(state->left_output_buffer) SIT_FREE(state->left_output_buffer);
        if(state->right_output_buffer) SIT_FREE(state->right_output_buffer);
        state->delay_buffer = NULL;
        state->input_buffer = NULL;
        state->left_output_buffer = NULL;
        state->right_output_buffer = NULL;
        return -1; // Indicate failure
    }

    sst282_reset(state);
    return 0; // Success
}

static void sst282_destroy(SST282State *state) {
    if(state->delay_buffer) SIT_FREE(state->delay_buffer);
    if(state->input_buffer) SIT_FREE(state->input_buffer);
    if(state->left_output_buffer) SIT_FREE(state->left_output_buffer);
    if(state->right_output_buffer) SIT_FREE(state->right_output_buffer);
}

static void sst282_set_audition_program(SST282State *state, SST282AuditionProgram program) {
    float base_delay_ms;
    switch (program) {
        case SST282_ROOM1: base_delay_ms = 10.0f; break;   // Small room reverb
        case SST282_ROOM2: base_delay_ms = 20.0f; break;   // Medium room reverb
        case SST282_ROOM3: base_delay_ms = 30.0f; break;   // Larger room reverb
        case SST282_ROOM4: base_delay_ms = 40.0f; break;   // Big room reverb
        case SST282_COMB6: base_delay_ms = 6.0f; break;    // Short comb filter
        case SST282_COMB10: base_delay_ms = 10.0f; break;  // Longer comb filter
        case SST282_FATTY: base_delay_ms = 15.0f; break;   // Thick, short reverb
        case SST282_CLOUD: base_delay_ms = 25.0f; break;   // Diffuse reverb
        case SST282_SLAP1: base_delay_ms = 50.0f; break;   // Short slapback echo
        case SST282_SLAP2: base_delay_ms = 70.0f; break;   // Longer slapback echo
        case SST282_ECHO: base_delay_ms = 100.0f; break;   // Standard echo
        case SST282_REPEAT2: base_delay_ms = 200.0f; break;// Long repeating echo
        case SST282_REPEAT3: base_delay_ms = 300.0f; break;// Very long repeating echo
        case SST282_REPEAT4: base_delay_ms = 400.0f; break;// Extremely long echo
        default: base_delay_ms = 10.0f; break;      // Fallback to small delay
    }
    for (int i = 0; i < 8; i++) {
        float delay_ms = base_delay_ms + i * 10.0f; // Increment delay per tap
        state->audition_taps[i].delay_samples = (int)(delay_ms * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
    }
}

// Process audio sample
static void sst282_process(SST282State *state, float input, float *left_out, float *right_out) {
    state->input_buffer[state->input_count++] = input;
    if (state->input_count >= state->downsample_factor) {
        float downsampled_input = state->input_buffer[state->downsample_factor - 1];
        float signal = downsampled_input * state->input_gain;
		signal = sst282_apply_biquad(&state->lf_biquad, signal); // Low-frequency EQ
		signal = sst282_apply_biquad(&state->hf_biquad, signal); // High-frequency EQ
		float abs_signal = fabsf(signal);
		state->peak_level = fmaxf(-30.0f, 20.0f * log10f(abs_signal + 1e-6f));
        float reverb_echo = 0.0f; // Placeholder
        // Feedback calculation moved after echo generation

		if (state->reverb_mode) {
			// Reverb: Sum 16 taps
			for (int i = 0; i < SST282_NUM_REVERB_TAPS; i++) {
				int read_idx = (state->write_index - state->reverb_taps[i].delay_samples + state->max_delay_samples) % state->max_delay_samples;
				reverb_echo += state->delay_buffer[read_idx] * state->reverb_taps[i].level;
			}
			reverb_echo /= SST282_NUM_REVERB_TAPS; // Average the taps
		} else {
			// Echo: Single tap
			int read_idx = (state->write_index - state->echo_tap.delay_samples + state->max_delay_samples) % state->max_delay_samples;
			reverb_echo = state->delay_buffer[read_idx] * state->echo_tap.level;
		}

        float feedback_signal = reverb_echo * state->feedback_gain;
        state->delay_buffer[state->write_index] = signal + feedback_signal;

        float left_tap_sum = 0.0f, right_tap_sum = 0.0f;
        for (int i = 0; i < 4; i++) {
            int tap_odd = 2 * i;     // Right
            int tap_even = tap_odd + 1; // Left
            int read_odd = (state->write_index - state->audition_taps[tap_odd].delay_samples + state->max_delay_samples) % state->max_delay_samples;
            int read_even = (state->write_index - state->audition_taps[tap_even].delay_samples + state->max_delay_samples) % state->max_delay_samples;
            right_tap_sum += state->delay_buffer[read_odd] * state->tap_levels[i];
            left_tap_sum += state->delay_buffer[read_even] * state->tap_levels[i];
        }
		// Calculate the output at 16 kHz
		float left_16khz = state->direct_level * downsampled_input + left_tap_sum + state->echo_level * reverb_echo;
		float right_16khz = state->direct_level * downsampled_input + right_tap_sum + state->echo_level * reverb_echo;

		// Apply the hard limiter
		left_16khz = sst282_hard_limit(left_16khz);
		right_16khz = sst282_hard_limit(right_16khz);
        state->write_index = (state->write_index + 1) % state->max_delay_samples;

		// Fill the output buffers with the limited values
        for (int i = 0; i < state->downsample_factor; i++) {
            state->left_output_buffer[i] = left_16khz;
            state->right_output_buffer[i] = right_16khz;
        }
        state->output_index = 0;
        state->input_count = 0;
    }
    if (state->output_index < state->downsample_factor) {
        *left_out = state->left_output_buffer[state->output_index];
        *right_out = state->right_output_buffer[state->output_index];
        state->output_index++;
    } else {
        *left_out = 0.0f;
        *right_out = 0.0f;
    }
}

// Setter functions
static void sst282_set_input_gain(SST282State *state, float gain_0_to_10) {
    state->input_gain = gain_0_to_10 / 10.0f;
}

static void sst282_set_eq(SST282State *state, float lf_cut_dB, float hf_cut_dB) {
    state->lf_cut_dB = fmaxf(0.0f, fminf(lf_cut_dB, 10.0f));
    state->hf_cut_dB = fmaxf(0.0f, fminf(hf_cut_dB, 10.0f));
}

static void sst282_set_tap_level(SST282State *state, int pair, float level_dB) {
    if (pair >= 0 && pair < 4) {
        state->tap_levels[pair] = sst282_db_to_gain(fmaxf(0.0f, fminf(level_dB, 12.0f)));
    }
}

static void sst282_set_feedback(SST282State *state, float feedback_0_to_10) {
    state->feedback_gain = feedback_0_to_10 / 10.0f;
}

static void sst282_set_mix(SST282State *state, float dry_dB, float echo_dB, float direct_dB) {
    state->dry_level = sst282_db_to_gain(fmaxf(0.0f, fminf(dry_dB, 12.0f)));
    state->echo_level = sst282_db_to_gain(fmaxf(0.0f, fminf(echo_dB, 12.0f)));
    state->direct_level = sst282_db_to_gain(fmaxf(0.0f, fminf(direct_dB, 12.0f)));
}

static void sst282_set_mode(SST282State *state, bool reverb_mode) {
    state->reverb_mode = reverb_mode;
}

static void sst282_set_echo_delay(SST282State *state, float delay_ms) {
    state->echo_tap.delay_samples = (int)(fmaxf(30.0f, fminf(delay_ms, 256.0f)) * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
}

// Helper function for reverb programs
static void sst282_set_reverb_program(SST282State *state, bool long_reverb) {
    state->long_reverb = long_reverb;
    float min_delay_ms = long_reverb ? 50.0f : 20.0f;
    float max_delay_ms = long_reverb ? 200.0f : 100.0f;
    for (int i = 0; i < SST282_NUM_REVERB_TAPS; i++) {
        float delay_ms = min_delay_ms + (max_delay_ms - min_delay_ms) * ((float)rand() / RAND_MAX);
        state->reverb_taps[i].delay_samples = (int)(delay_ms * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
        state->reverb_taps[i].level = 0.5f;
    }
}

// Preset system
static void sst282_set_preset(SST282State *state, const char *preset_name) {
    sst282_reset(state); // Reset state without re-allocation

    if (strcmp(preset_name, "ROOM1") == 0) {
        for (int i = 0; i < 8; i++) {
            state->audition_taps[i].delay_samples = (int)((10.0f + i * 10.0f) * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
        }
        sst282_set_tap_level(state, 0, 6.0f); sst282_set_tap_level(state, 1, 4.0f);
        sst282_set_tap_level(state, 2, 2.0f); sst282_set_tap_level(state, 3, 0.0f);
        sst282_set_reverb_program(state, false);
        sst282_set_mode(state, true);
        sst282_set_feedback(state, 5.0f);
        sst282_set_mix(state, 6.0f, 8.0f, 6.0f);
        sst282_set_input_gain(state, 5.0f);

    } else if (strcmp(preset_name, "ROOM4") == 0) {
        for (int i = 0; i < 8; i++) {
            state->audition_taps[i].delay_samples = (int)((50.0f + i * 25.0f) * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
        }
        sst282_set_tap_level(state, 0, 3.0f); sst282_set_tap_level(state, 1, 6.0f);
        sst282_set_tap_level(state, 2, 9.0f); sst282_set_tap_level(state, 3, 12.0f);
        sst282_set_reverb_program(state, true);
        sst282_set_mode(state, true);
        sst282_set_feedback(state, 7.0f);
        sst282_set_mix(state, 6.0f, 10.0f, 6.0f);
        sst282_set_input_gain(state, 6.0f);

    } else if (strcmp(preset_name, "ECHO") == 0) {
        for (int i = 0; i < 8; i++) {
            state->audition_taps[i].delay_samples = (int)(250.0f * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
        }
        sst282_set_tap_level(state, 0, 0.0f); sst282_set_tap_level(state, 1, 0.0f);
        sst282_set_tap_level(state, 2, 0.0f); sst282_set_tap_level(state, 3, 0.0f);
        sst282_set_mode(state, false);
        sst282_set_echo_delay(state, 250.0f);
        sst282_set_feedback(state, 3.0f);
        sst282_set_mix(state, 0.0f, 12.0f, 0.0f);
        sst282_set_eq(state, 3.0f, 3.0f);
        sst282_set_input_gain(state, 7.0f);

    } else if (strcmp(preset_name, "D4_COMB_SCI_FI") == 0) { // From Image 14
        for (int i = 0; i < 4; i++) {
            state->audition_taps[2*i].delay_samples = (int)((6.0f + i * 6.0f) * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
            state->audition_taps[2*i+1].delay_samples = (int)((6.0f + i * 6.0f) * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
        }
        sst282_set_tap_level(state, 0, 12.0f); sst282_set_tap_level(state, 1, 12.0f);
        sst282_set_tap_level(state, 2, 12.0f); sst282_set_tap_level(state, 3, 12.0f);
        sst282_set_mode(state, false);
        sst282_set_feedback(state, 0.0f);
        sst282_set_mix(state, 0.0f, 12.0f, 0.0f);
        sst282_set_input_gain(state, 5.0f);

    } else if (strcmp(preset_name, "E9_SPACE_REPEAT_3") == 0) { // From Image 14
        for (int i = 0; i < 8; i++) {
            state->audition_taps[i].delay_samples = (int)((i % 3) * 85.0f * SST282_INTERNAL_SAMPLE_RATE / 1000.0f);
        }
        sst282_set_tap_level(state, 0, 12.0f); sst282_set_tap_level(state, 1, 10.0f);
        sst282_set_tap_level(state, 2, 8.0f); sst282_set_tap_level(state, 3, 6.0f);
        sst282_set_reverb_program(state, true);
        sst282_set_mode(state, true);
        sst282_set_feedback(state, 5.0f);
        sst282_set_mix(state, 6.0f, 10.0f, 6.0f);
        sst282_set_input_gain(state, 6.0f);
    }

    // Add more presets as needed
}

static void sst282_apply_params(SST282State *state, SST282Params *params) {
    bool force_update = !state->params_initialized;

    // Update only the parameters that have changed (or all if first run)
    if (force_update || params->input_gain != state->previous_params.input_gain) {
        sst282_set_input_gain(state, params->input_gain);
        state->previous_params.input_gain = params->input_gain;
    }
    if (force_update || params->lf_cut_dB != state->previous_params.lf_cut_dB || params->hf_cut_dB != state->previous_params.hf_cut_dB) {
        sst282_set_eq(state, params->lf_cut_dB, params->hf_cut_dB);
        state->previous_params.lf_cut_dB = params->lf_cut_dB;
        state->previous_params.hf_cut_dB = params->hf_cut_dB;
    }
    if (force_update || params->audition_program != state->previous_params.audition_program) {
        sst282_set_audition_program(state, params->audition_program);
        state->previous_params.audition_program = params->audition_program;
    }
    if (force_update || params->echo_delay_ms != state->previous_params.echo_delay_ms) {
        sst282_set_echo_delay(state, params->echo_delay_ms);
        state->previous_params.echo_delay_ms = params->echo_delay_ms;
    }
    if (force_update || params->feedback != state->previous_params.feedback) {
        sst282_set_feedback(state, params->feedback);
        state->previous_params.feedback = params->feedback;
    }
    if (force_update || params->dry_dB != state->previous_params.dry_dB || params->echo_dB != state->previous_params.echo_dB || params->direct_dB != state->previous_params.direct_dB) {
        sst282_set_mix(state, params->dry_dB, params->echo_dB, params->direct_dB);
        state->previous_params.dry_dB = params->dry_dB;
        state->previous_params.echo_dB = params->echo_dB;
        state->previous_params.direct_dB = params->direct_dB;
    }
    // Handle the tap_levels array
    for (int i = 0; i < 4; i++) {
        if (force_update || params->tap_levels[i] != state->previous_params.tap_levels[i]) {
            sst282_set_tap_level(state, i, params->tap_levels[i]);
            state->previous_params.tap_levels[i] = params->tap_levels[i];
        }
    }
    if (force_update || params->reverb_program_long != state->previous_params.reverb_program_long) {
        sst282_set_reverb_program(state, params->reverb_program_long);
        state->previous_params.reverb_program_long = params->reverb_program_long;
    }
    if (force_update || params->reverb_mode != state->previous_params.reverb_mode) {
        sst282_set_mode(state, params->reverb_mode);
        state->previous_params.reverb_mode = params->reverb_mode;
    }

    state->params_initialized = true;
}

static int sst282_get_latency_samples(SST282State* state) {
    return state->downsample_factor;  // Latency equals the downsample factor
}
#endif // SIT_AUX_SST282_H
