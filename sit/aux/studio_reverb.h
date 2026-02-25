/***************************************************************************************************
*
*   studio_reverb.h - Internal Studio Reverb Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   High-quality algorithmic reverb with Early Reflections, FDN, and Diffusion.
*   This file is intended to be included within the audio subsystem implementation.
*
***************************************************************************************************/

#ifndef SIT_AUX_STUDIO_REVERB_H
#define SIT_AUX_STUDIO_REVERB_H

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef SIT_MALLOC
#define SIT_MALLOC(s) malloc(s)
#define SIT_CALLOC(n,s) calloc(n,s)
#define SIT_FREE(p) free(p)
#endif

// Studio Reverb parameters
typedef struct {
    int size;                  // 1-13 (mapped to volume in m³)
    float decay_time;          // 0.1-200 s
    float bass_coef;           // 0.25-4.00
    float treble_coef;         // 0.25-4.00
    float pre_delay_ms;        // 0-999 ms
    float reverb_atten_db;     // 0-99 dB
    float early_reflections[4][2]; // [delay_ms, atten_db] for 4 reflections
    float stereo_discorrelator; // ±30 units
    float diffusion_db;        // 30-100 dB
    float wet_mix;             // 0.0-1.0 (dry/wet balance)
} SituationStudioReverbParams;

// Constants
#define SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS 4
#define SIT_STUDIO_REVERB_NUM_FDN_DELAYS 8
#define SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS 4
#define SIT_STUDIO_REVERB_DISCORRELATOR_DELAY 30     // Fixed delay for discorrelator in samples

// Studio Reverb state
typedef struct {
    SituationStudioReverbParams params;
    int sample_rate;
    int max_delay_samples;     // Dynamic buffer size based on sample rate
    // Delay buffers
    float* pre_delay_left;
    float* pre_delay_right;
    float* early_reflections_buffers[SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS];
    float* fdn_delays[SIT_STUDIO_REVERB_NUM_FDN_DELAYS];
    float* allpass_buffers[SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS];
    float* discorrelator_left;
    float* discorrelator_right;
    // Buffer indices
    int pre_delay_idx;
    int early_reflections_idx[SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS];
    int fdn_idx[SIT_STUDIO_REVERB_NUM_FDN_DELAYS];
    int allpass_idx[SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS];
    int discorrelator_idx;
} SituationStudioReverb;

// Preset array
static SituationStudioReverbParams studio_reverb_presets[52] = {
    // Preset 0: Medium Room Example
    {
        .size = 9,              // 10,000 m³
        .decay_time = 2.1f,     // 2.1 seconds
        .bass_coef = 1.00f,
        .treble_coef = 0.45f,
        .pre_delay_ms = 21.0f,
        .reverb_atten_db = 14.0f,
        .early_reflections = {{32.0f, 0.0f}, {57.0f, -10.0f}, {57.0f, -11.0f}, {45.0f, -11.0f}},
        .stereo_discorrelator = 10.0f,
        .diffusion_db = 50.0f,
        .wet_mix = 0.5f
    },
    // Add remaining 51 presets here...
};

// Forward declarations
static void _SituationStudioReverbDestroy(SituationStudioReverb* reverb);

static SituationStudioReverb* _SituationStudioReverbCreate(int sample_rate) {
    SituationStudioReverb* reverb = (SituationStudioReverb*)SIT_MALLOC(sizeof(SituationStudioReverb));
    if (!reverb) return NULL;
    memset(reverb, 0, sizeof(SituationStudioReverb));

    reverb->sample_rate = sample_rate;
    reverb->max_delay_samples = (int)(1.0f * sample_rate + 0.5f); // 1 second buffer

    // Allocate delay buffers
    reverb->pre_delay_left = (float*)SIT_CALLOC(reverb->max_delay_samples, sizeof(float));
    reverb->pre_delay_right = (float*)SIT_CALLOC(reverb->max_delay_samples, sizeof(float));

    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS; i++) {
        reverb->early_reflections_buffers[i] = (float*)SIT_CALLOC(reverb->max_delay_samples, sizeof(float));
    }
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
        reverb->fdn_delays[i] = (float*)SIT_CALLOC(reverb->max_delay_samples, sizeof(float));
    }
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS; i++) {
        reverb->allpass_buffers[i] = (float*)SIT_CALLOC(reverb->max_delay_samples, sizeof(float));
    }
    reverb->discorrelator_left = (float*)SIT_CALLOC(SIT_STUDIO_REVERB_DISCORRELATOR_DELAY, sizeof(float));
    reverb->discorrelator_right = (float*)SIT_CALLOC(SIT_STUDIO_REVERB_DISCORRELATOR_DELAY, sizeof(float));

    // Check allocation success
    if (!reverb->pre_delay_left || !reverb->pre_delay_right ||
        !reverb->discorrelator_left || !reverb->discorrelator_right) {
        _SituationStudioReverbDestroy(reverb);
        return NULL;
    }

    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS; i++) {
        if (!reverb->early_reflections_buffers[i]) {
            _SituationStudioReverbDestroy(reverb);
            return NULL;
        }
    }
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
        if (!reverb->fdn_delays[i]) {
            _SituationStudioReverbDestroy(reverb);
            return NULL;
        }
    }
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS; i++) {
        if (!reverb->allpass_buffers[i]) {
            _SituationStudioReverbDestroy(reverb);
            return NULL;
        }
    }

    // Initialize indices - calloc zeroed them, but being explicit is fine.
    reverb->pre_delay_idx = 0;

    return reverb;
}

static void _SituationStudioReverbDestroy(SituationStudioReverb* reverb) {
    if (!reverb) return;
    if(reverb->pre_delay_left) SIT_FREE(reverb->pre_delay_left);
    if(reverb->pre_delay_right) SIT_FREE(reverb->pre_delay_right);
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS; i++) if(reverb->early_reflections_buffers[i]) SIT_FREE(reverb->early_reflections_buffers[i]);
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) if(reverb->fdn_delays[i]) SIT_FREE(reverb->fdn_delays[i]);
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS; i++) if(reverb->allpass_buffers[i]) SIT_FREE(reverb->allpass_buffers[i]);
    if(reverb->discorrelator_left) SIT_FREE(reverb->discorrelator_left);
    if(reverb->discorrelator_right) SIT_FREE(reverb->discorrelator_right);
    SIT_FREE(reverb);
}

static void _SituationStudioReverbSetParams(SituationStudioReverb* reverb, SituationStudioReverbParams params) {
    // Clamp parameters
    if (params.size < 1) params.size = 1;
    if (params.size > 13) params.size = 13;
    if (params.decay_time < 0.1f) params.decay_time = 0.1f;
    if (params.decay_time > 200.0f) params.decay_time = 200.0f;
    if (params.bass_coef < 0.25f) params.bass_coef = 0.25f;
    if (params.bass_coef > 4.0f) params.bass_coef = 4.0f;
    if (params.treble_coef < 0.25f) params.treble_coef = 0.25f;
    if (params.treble_coef > 4.0f) params.treble_coef = 4.0f;
    if (params.pre_delay_ms < 0.0f) params.pre_delay_ms = 0.0f;
    if (params.pre_delay_ms > 999.0f) params.pre_delay_ms = 999.0f;
    if (params.reverb_atten_db < 0.0f) params.reverb_atten_db = 0.0f;
    if (params.reverb_atten_db > 99.0f) params.reverb_atten_db = 99.0f;
    if (params.stereo_discorrelator < -30.0f) params.stereo_discorrelator = -30.0f;
    if (params.stereo_discorrelator > 30.0f) params.stereo_discorrelator = 30.0f;
    if (params.diffusion_db < 30.0f) params.diffusion_db = 30.0f;
    if (params.diffusion_db > 100.0f) params.diffusion_db = 100.0f;
    if (params.wet_mix < 0.0f) params.wet_mix = 0.0f;
    if (params.wet_mix > 1.0f) params.wet_mix = 1.0f;
    reverb->params = params;
}

static void _SituationStudioReverbSetPreset(SituationStudioReverb* reverb, int preset_index) {
    if (preset_index < 0 || preset_index >= 52) return;
    _SituationStudioReverbSetParams(reverb, studio_reverb_presets[preset_index]);
}

static void _SituationStudioReverbProcess(SituationStudioReverb* reverb, const float* in_left, const float* in_right,
                    float* out_left, float* out_right, int num_samples) {
    int pre_delay_samples = (int)(reverb->params.pre_delay_ms * reverb->sample_rate / 1000.0f);
    float early_gains[SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS];
    int early_delays[SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS];

    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS; i++) {
        early_delays[i] = (int)(reverb->params.early_reflections[i][0] * reverb->sample_rate / 1000.0f);
        early_gains[i] = powf(10.0f, reverb->params.early_reflections[i][1] / 20.0f);
    }

    for (int n = 0; n < num_samples; n++) {
        float input = (in_left[n] + in_right[n]) * 0.5f;

        // Stereo Discorrelator
        int discorrelator_delay_left = (int)(reverb->params.stereo_discorrelator);
        int discorrelator_delay_right = -discorrelator_delay_left;
        int read_idx_left = (reverb->discorrelator_idx - discorrelator_delay_left + SIT_STUDIO_REVERB_DISCORRELATOR_DELAY) % SIT_STUDIO_REVERB_DISCORRELATOR_DELAY;
        int read_idx_right = (reverb->discorrelator_idx - discorrelator_delay_right + SIT_STUDIO_REVERB_DISCORRELATOR_DELAY) % SIT_STUDIO_REVERB_DISCORRELATOR_DELAY;
        float discorrelator_out_left = reverb->discorrelator_left[read_idx_left];
        float discorrelator_out_right = reverb->discorrelator_right[read_idx_right];
        reverb->discorrelator_left[reverb->discorrelator_idx] = input;
        reverb->discorrelator_right[reverb->discorrelator_idx] = input;

        // Pre-delay
        int pre_delay_read_idx = (reverb->pre_delay_idx - pre_delay_samples + reverb->max_delay_samples) % reverb->max_delay_samples;
        reverb->pre_delay_left[reverb->pre_delay_idx] = discorrelator_out_left;
        reverb->pre_delay_right[reverb->pre_delay_idx] = discorrelator_out_right;
        float pre_delay_out_left = reverb->pre_delay_left[pre_delay_read_idx];
        float pre_delay_out_right = reverb->pre_delay_right[pre_delay_read_idx];

        // Early reflections
        float early_left = 0.0f, early_right = 0.0f;
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS; i++) {
            int read_idx = (reverb->early_reflections_idx[i] - early_delays[i] + reverb->max_delay_samples) % reverb->max_delay_samples;
            float sample = reverb->early_reflections_buffers[i][read_idx];
            if (i == 0 || i == 2) early_left += sample * early_gains[i];
            else early_right += sample * early_gains[i];
            reverb->early_reflections_buffers[i][reverb->early_reflections_idx[i]] = (i % 2 == 0) ? pre_delay_out_left : pre_delay_out_right;
        }

        // Diffusion Network (all-pass filters)
        float diffusion_gain = powf(10.0f, reverb->params.diffusion_db / 20.0f);
        float allpass_out = pre_delay_out_left;
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS; i++) {
            int delay_samples = 100; // Fixed delay (could be made time-based)
            int read_idx = (reverb->allpass_idx[i] - delay_samples + reverb->max_delay_samples) % reverb->max_delay_samples;
            float delayed_sample = reverb->allpass_buffers[i][read_idx];
            float temp = allpass_out;
            allpass_out = -diffusion_gain * allpass_out + delayed_sample;
            reverb->allpass_buffers[i][reverb->allpass_idx[i]] = temp + diffusion_gain * allpass_out;
        }

        // Main Reverberation Network (FDN)
        float fdn_out = 0.0f;
        float decay_gain = expf(-6.907755f / (reverb->params.decay_time * reverb->sample_rate));
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
            int delay_samples = 1000 + (reverb->params.size * 100); // Size-based scaling
            int read_idx = (reverb->fdn_idx[i] - delay_samples + reverb->max_delay_samples) % reverb->max_delay_samples;
            fdn_out += reverb->fdn_delays[i][read_idx];
            reverb->fdn_delays[i][reverb->fdn_idx[i]] = allpass_out * decay_gain * 0.5f;
        }
        fdn_out /= SIT_STUDIO_REVERB_NUM_FDN_DELAYS;

        // Mixing
        float wet_gain = powf(10.0f, -reverb->params.reverb_atten_db / 20.0f);
        float wet_left = (early_left + fdn_out) * wet_gain;
        float wet_right = (early_right + fdn_out) * wet_gain;
        out_left[n] = in_left[n] * (1.0f - reverb->params.wet_mix) + wet_left * reverb->params.wet_mix;
        out_right[n] = in_right[n] * (1.0f - reverb->params.wet_mix) + wet_right * reverb->params.wet_mix;

        // Update indices
        reverb->pre_delay_idx = (reverb->pre_delay_idx + 1) % reverb->max_delay_samples;
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS; i++) {
            reverb->early_reflections_idx[i] = (reverb->early_reflections_idx[i] + 1) % reverb->max_delay_samples;
        }
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
            reverb->fdn_idx[i] = (reverb->fdn_idx[i] + 1) % reverb->max_delay_samples;
        }
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS; i++) {
            reverb->allpass_idx[i] = (reverb->allpass_idx[i] + 1) % reverb->max_delay_samples;
        }
        reverb->discorrelator_idx = (reverb->discorrelator_idx + 1) % SIT_STUDIO_REVERB_DISCORRELATOR_DELAY;
    }
}

static int _SituationStudioReverbGetLatencySamples(SituationStudioReverb *reverb) {
    return 0; // Sample-by-sample processing with dry signal included
}

#endif // SIT_AUX_STUDIO_REVERB_H
