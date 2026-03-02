/***************************************************************************************************
*
*   spring_reverb.h - Internal Spring Reverb Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Implementation of a digital Spring Reverb model with gate and EQ.
*   This file is intended to be included within the audio subsystem implementation.
*
***************************************************************************************************/

#ifndef SIT_AUX_SPRING_REVERB_H
#define SIT_AUX_SPRING_REVERB_H

#include <math.h>

#ifndef PI
#define PI 3.141592653589793f
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef struct {
    // Main delay lines for left and right channels
    float *delay_line_l;
    float *delay_line_r;
    int delay_length;
    int main_write_pos;
    float fb;           // Feedback gain (controlled by decay_time)
    float a_lpf;        // LPF coefficient (influenced by EQ)
    float lpf_state_l;
    float lpf_state_r;
	float a_hpf;        // HPF coefficient
    float hpf_state_l;  // HPF state for left channel
    float hpf_state_r;  // HPF state for right channel

    // All-pass filters for dispersion (4 per channel)
    float *ap_delay_lines_l[4];
    float *ap_delay_lines_r[4];
    int ap_delay_lengths[4];
    int ap_write_pos_l[4];
    int ap_write_pos_r[4];
    float g;            // All-pass coefficient (dispersion)

    // Control parameters
    float input_level;  // Input gain (0 to 1)
    float threshold;    // Gate threshold (0 to 1)
    float decay_time;   // Reverb decay time in seconds
    int gate_enabled;   // Gate ON (1) or OFF (0)
	float bass;         // Bass EQ (-1 to 1)
    float middle;       // Midrange EQ (-1 to 1)
    float treble;       // Treble EQ (-1 to 1)
    float direct;       // Dry level (0 to 1)
    float reverb;       // Wet level (0 to 1)
    float gate_state;   // Current gate state (0 to 1)
    float delta;        // Gate release rate
	float cross_mix;  // Cross-feedback mix (0 to 1)

    int sample_rate;
} SpringReverb;

static void SpringReverb_init(SpringReverb *reverb, int sample_rate) {
    reverb->sample_rate = sample_rate;

    // Main delay: 50ms to simulate a long spring
    reverb->delay_length = (int)(0.05f * sample_rate + 0.5f);
    reverb->delay_line_l = (float*)SIT_CALLOC(reverb->delay_length, sizeof(float));
    reverb->delay_line_r = (float*)SIT_CALLOC(reverb->delay_length, sizeof(float));
    reverb->main_write_pos = 0;
    reverb->lpf_state_l = 0.0f;
    reverb->lpf_state_r = 0.0f;

    // All-pass filters: 10ms, 15ms, 20ms, 25ms
    float ap_delays_ms[4] = {10.0f, 15.0f, 20.0f, 25.0f};
    for (int k = 0; k < 4; k++) {
        reverb->ap_delay_lengths[k] = (int)(0.001f * ap_delays_ms[k] * sample_rate + 0.5f);
        reverb->ap_delay_lines_l[k] = (float*)SIT_CALLOC(reverb->ap_delay_lengths[k], sizeof(float));
        reverb->ap_delay_lines_r[k] = (float*)SIT_CALLOC(reverb->ap_delay_lengths[k], sizeof(float));
        reverb->ap_write_pos_l[k] = 0;
        reverb->ap_write_pos_r[k] = 0;
    }

    // Default parameters
    reverb->input_level = 0.5f;  // 5/10
    reverb->threshold = 0.1f;    // 1/10
    reverb->decay_time = 1.0f;   // 1 second
    reverb->gate_enabled = 1;    // ON
    reverb->middle = 0.0f;       // Neutral
    reverb->treble = 0.0f;       // Neutral
    reverb->direct = 0.5f;       // 5/10
    reverb->reverb = 0.5f;       // 5/10
    reverb->gate_state = 0.0f;
    reverb->fb = 0.5f;
    reverb->a_lpf = 0.5f;
	reverb->bass = 0.0f;       // Neutral setting
    reverb->a_hpf = 0.0f;      // Will be calculated in set_params
    reverb->hpf_state_l = 0.0f;
    reverb->hpf_state_r = 0.0f;
    reverb->g = 0.6f;
    reverb->delta = 1.0f / (0.1f * sample_rate);  // Default 100ms release
	reverb->cross_mix = 0.2f;  // Default to 20% cross-feedback
}

static void SpringReverb_cleanup(SpringReverb *reverb) {
    SIT_FREE(reverb->delay_line_l);
    SIT_FREE(reverb->delay_line_r);
    for (int k = 0; k < 4; k++) {
        SIT_FREE(reverb->ap_delay_lines_l[k]);
        SIT_FREE(reverb->ap_delay_lines_r[k]);
    }
}

static void SpringReverb_set_params(SpringReverb *reverb, float input_level, float threshold, float decay_time, int gate_enabled, float bass, float middle, float treble, float direct, float reverb_level, float cross_mix) {
    // Map ranges to internal values
	reverb->input_level = input_level / 10.0f;
    reverb->threshold = threshold / 10.0f;
    reverb->decay_time = decay_time * 0.5f;
    reverb->gate_enabled = gate_enabled ? 1 : 0;
    reverb->bass = bass / 10.0f;
    reverb->middle = middle / 10.0f;
    reverb->treble = treble / 10.0f;
    reverb->direct = direct / 10.0f;
    reverb->reverb = reverb_level / 10.0f;

    // Update feedback based on decay time
    if (reverb->decay_time > 0.0f) {
        reverb->fb = powf(0.001f, (float)reverb->delay_length / (reverb->decay_time * reverb->sample_rate));
    } else {
        reverb->fb = 0.0f;
    }

    // Simplified EQ: Adjust LPF cutoff based on treble (treble < 0 increases damping)
    float damping = MAX(0.0f, -reverb->treble);  // 0 to 1
    float fc = 10000.0f * (1.0f - damping);      // 10kHz down to 0
    reverb->a_lpf = expf(-2.0f * PI * fc / reverb->sample_rate);
    if (reverb->treble >= 0) reverb->a_lpf = 0.0f;  // No filtering if treble boost

	// Calculate HPF coefficient
    float bass_factor = (reverb->bass < 0) ? -reverb->bass : 0.0f; // 0 to 1 when bass is negative
    float fc_hpf = 20.0f + 480.0f * bass_factor;                  // 20 Hz to 500 Hz
    reverb->a_hpf = expf(-2.0f * PI * fc_hpf / reverb->sample_rate);

    // Dispersion based on middle (simplified influence)
    reverb->g = 0.6f * (1.0f - fabsf(reverb->middle) * 0.5f);  // Reduce dispersion with strong mid EQ

    // Gate release (100ms fixed for simplicity, could be made adjustable)
    reverb->delta = 1.0f / (0.1f * reverb->sample_rate);

	// Set cross_mix (map 0-10 to 0-1)
    reverb->cross_mix = cross_mix / 10.0f;
}

static inline float hard_limiter(float sample) {
    return fmaxf(-1.0f, fminf(1.0f, sample));
}

static void SpringReverb_process(SpringReverb *reverb, float *in_l, float *in_r, float *out_l, float *out_r, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        float input_l = in_l[i] * reverb->input_level;
        float input_r = in_r[i] * reverb->input_level;

        float input_level = MAX(fabsf(input_l), fabsf(input_r));
        if (reverb->gate_enabled && input_level > reverb->threshold) {
            reverb->gate_state = 1.0f;
        } else if (reverb->gate_enabled) {
            reverb->gate_state = MAX(0.0f, reverb->gate_state - reverb->delta);
        } else {
            reverb->gate_state = 1.0f;
        }

        // Compute mixed feedback states
        float mix_l = (1.0f - reverb->cross_mix) * reverb->lpf_state_l + reverb->cross_mix * reverb->lpf_state_r;
        float mix_r = (1.0f - reverb->cross_mix) * reverb->lpf_state_r + reverb->cross_mix * reverb->lpf_state_l;

        // Left channel
        float output_l = reverb->delay_line_l[reverb->main_write_pos];
        float feedback_l = reverb->fb * mix_l;
        reverb->delay_line_l[reverb->main_write_pos] = input_l + feedback_l;
        reverb->lpf_state_l = reverb->a_lpf * reverb->lpf_state_l + (1.0f - reverb->a_lpf) * output_l;
        float reverb_l = output_l;

        for (int k = 0; k < 4; k++) {
            float *ap_delay = reverb->ap_delay_lines_l[k];
            int ap_M = reverb->ap_delay_lengths[k];
            int wp = reverb->ap_write_pos_l[k];
            int rp = (wp - ap_M + reverb->ap_delay_lengths[k]) % reverb->ap_delay_lengths[k];
            float delayed = ap_delay[rp];
            float temp = reverb_l + reverb->g * delayed;
            reverb_l = -reverb->g * temp + delayed;
            ap_delay[wp] = temp;
            reverb->ap_write_pos_l[k] = (wp + 1) % reverb->ap_delay_lengths[k];
        }

        float hpf_out_l = reverb_l - reverb->hpf_state_l;
        reverb->hpf_state_l = reverb->hpf_state_l + reverb->a_hpf * hpf_out_l;
        reverb_l = hpf_out_l;

        // Right channel
        float output_r = reverb->delay_line_r[reverb->main_write_pos];
        float feedback_r = reverb->fb * mix_r;
        reverb->delay_line_r[reverb->main_write_pos] = input_r + feedback_r;
        reverb->lpf_state_r = reverb->a_lpf * reverb->lpf_state_r + (1.0f - reverb->a_lpf) * output_r;
        float reverb_r = output_r;

        for (int k = 0; k < 4; k++) {
            float *ap_delay = reverb->ap_delay_lines_r[k];
            int ap_M = reverb->ap_delay_lengths[k];
            int wp = reverb->ap_write_pos_r[k];
            int rp = (wp - ap_M + reverb->ap_delay_lengths[k]) % reverb->ap_delay_lengths[k];
            float delayed = ap_delay[rp];
            float temp = reverb_r + reverb->g * delayed;
            reverb_r = -reverb->g * temp + delayed;
            ap_delay[wp] = temp;
            reverb->ap_write_pos_r[k] = (wp + 1) % reverb->ap_delay_lengths[k];
        }

        float hpf_out_r = reverb_r - reverb->hpf_state_r;
        reverb->hpf_state_r = reverb->hpf_state_r + reverb->a_hpf * hpf_out_r;
        reverb_r = hpf_out_r;

        reverb->main_write_pos = (reverb->main_write_pos + 1) % reverb->delay_length;

        float mix_l_out = reverb->direct * input_l + reverb->reverb * reverb_l * reverb->gate_state;
        float mix_r_out = reverb->direct * input_r + reverb->reverb * reverb_r * reverb->gate_state;

        out_l[i] = hard_limiter(mix_l_out);
        out_r[i] = hard_limiter(mix_r_out);
    }
}

static int spring_reverb_get_latency_samples(SpringReverb *reverb) {
    (void)reverb;
    return 0; // Sample-by-sample processing with dry signal included
}

#endif // SIT_AUX_SPRING_REVERB_H
