/***************************************************************************************************
*
*   studio_reverb.h - Internal Studio Reverb Implementation (v1.1 2026/04/19)
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

// FMA detection
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define STUDIO_REV_HAS_FMA 1
    #if defined(__GNUC__) || defined(__clang__)
        #define STUDIO_REV_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
    #else
        #define STUDIO_REV_FMA(a, b, c) fmaf((a), (b), (c))
    #endif
#else
    #define STUDIO_REV_HAS_FMA 0
    #define STUDIO_REV_FMA(a, b, c) ((a) * (b) + (c))
#endif

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
#define SIT_STUDIO_REVERB_DISCORRELATOR_DELAY 64

// Studio Reverb state
typedef struct {
    SituationStudioReverbParams params;
    int sample_rate;
    int max_delay_samples;
    
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
    
    // Internal state for FDN filters
    float fdn_lpf_state[SIT_STUDIO_REVERB_NUM_FDN_DELAYS];
    float fdn_hpf_state[SIT_STUDIO_REVERB_NUM_FDN_DELAYS];
    
    // Complex rotation LFO state
    float lfo_x;
    float lfo_y;
    float lfo_cos_theta;
    float lfo_sin_theta;

    // Pre-calculated DSP variables
    int pre_delay_samples;
    float early_gains[SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS];
    int early_delays_samples[SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS];
    float diffusion_gain;
    float fdn_delays_samples[SIT_STUDIO_REVERB_NUM_FDN_DELAYS];
    float fdn_gains[SIT_STUDIO_REVERB_NUM_FDN_DELAYS];
    int ap_delays_samples[SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS];
    float hf_damping;
    float lf_damping;
    float wet_gain;
    float dry_gain;

} SituationStudioReverb;

// Preset array
static SituationStudioReverbParams studio_reverb_presets[52] = {
    { 9,  2.1f, 1.00f, 0.45f, 21.0f, 14.0f, {{32.0f, 0.0f}, {57.0f, -10.0f}, {57.0f, -11.0f}, {45.0f, -11.0f}}, 10.0f, 50.0f, 0.5f }, // Medium Room
    {13,  4.5f, 1.20f, 0.30f, 45.0f, 12.0f, {{40.0f, -2.0f}, {65.0f, -8.0f}, {85.0f, -10.0f}, {110.0f, -12.0f}}, 20.0f, 70.0f, 0.6f }, // Large Hall
    { 4,  0.8f, 0.90f, 0.80f,  5.0f, 18.0f, {{12.0f, -4.0f}, {22.0f, -6.0f}, {30.0f, -10.0f}, {40.0f, -14.0f}},  5.0f, 40.0f, 0.3f }, // Small Studio
    {10,  2.5f, 0.50f, 1.50f, 10.0f, 15.0f, {{15.0f, -5.0f}, {25.0f, -5.0f}, {35.0f, -5.0f}, {45.0f, -5.0f}}, 15.0f, 85.0f, 0.4f }, // Bright Plate
    {13, 15.0f, 2.00f, 0.25f, 80.0f, 10.0f, {{50.0f, 0.0f}, {100.0f, -5.0f}, {150.0f, -10.0f}, {200.0f, -15.0f}}, 30.0f, 90.0f, 0.8f }, // Deep Space
    { 2,  1.2f, 0.80f, 0.90f,  3.0f, 20.0f, {{ 8.0f, -3.0f}, {15.0f, -8.0f}, {22.0f, -12.0f}, {28.0f, -18.0f}},  3.0f, 35.0f, 0.4f }, // 5: Spring Reverb
    { 8,  2.8f, 0.60f, 1.40f, 12.0f, 16.0f, {{18.0f, -4.0f}, {26.0f, -6.0f}, {35.0f, -7.0f}, {48.0f, -9.0f}}, 12.0f, 82.0f, 0.45f }, // 6: Classic Plate
    { 3,  0.6f, 1.10f, 0.70f,  2.0f, 22.0f, {{10.0f, -2.0f}, {18.0f, -9.0f}, {25.0f, -11.0f}, {32.0f, -15.0f}},  4.0f, 45.0f, 0.35f }, // 7: Bathroom
    { 5,  1.8f, 0.95f, 0.85f,  8.0f, 17.0f, {{14.0f, -5.0f}, {24.0f, -7.0f}, {33.0f, -10.0f}, {41.0f, -13.0f}},  7.0f, 55.0f, 0.5f }, // 8: Tile Room / Cask
    { 6,  2.4f, 1.30f, 0.55f, 15.0f, 13.0f, {{20.0f, -3.0f}, {35.0f, -8.0f}, {48.0f, -12.0f}, {62.0f, -14.0f}},  9.0f, 60.0f, 0.55f }, // 9: Cellar
    {11,  3.8f, 1.15f, 0.40f, 28.0f, 11.0f, {{35.0f, -1.0f}, {55.0f, -6.0f}, {72.0f, -9.0f}, {88.0f, -11.0f}}, 18.0f, 68.0f, 0.6f }, // 10: Concert Hall A
    {12,  4.2f, 1.25f, 0.35f, 38.0f, 10.0f, {{42.0f, -2.0f}, {68.0f, -7.0f}, {90.0f, -10.0f}, {115.0f, -13.0f}}, 22.0f, 72.0f, 0.65f }, // 11: Concert Hall B
    {10,  3.5f, 1.40f, 0.45f, 25.0f, 14.0f, {{30.0f, -4.0f}, {52.0f, -9.0f}, {70.0f, -11.0f}, {95.0f, -15.0f}}, 16.0f, 65.0f, 0.5f }, // 12: Church
    {13,  6.5f, 1.50f, 0.28f, 55.0f,  9.0f, {{48.0f, -1.0f}, {80.0f, -6.0f}, {120.0f, -10.0f}, {160.0f, -14.0f}}, 25.0f, 78.0f, 0.7f }, // 13: Cathedral
    { 7,  1.5f, 0.75f, 1.10f,  7.0f, 19.0f, {{16.0f, -6.0f}, {29.0f, -8.0f}, {37.0f, -11.0f}, {46.0f, -16.0f}},  8.0f, 75.0f, 0.4f }, // 14: Bright Tube
    { 9,  2.9f, 1.05f, 0.60f, 18.0f, 12.0f, {{28.0f, -3.0f}, {45.0f, -7.0f}, {58.0f, -9.0f}, {74.0f, -12.0f}}, 14.0f, 58.0f, 0.55f }, // 15: Medium Chamber
    {13,  8.0f, 1.80f, 0.22f, 65.0f,  8.0f, {{60.0f, 0.0f}, {95.0f, -4.0f}, {135.0f, -8.0f}, {180.0f, -13.0f}}, 28.0f, 85.0f, 0.75f }, // 16: Sound Hoarder (iconic long tail)
    { 1,  0.4f, 0.70f, 1.20f,  1.0f, 25.0f, {{ 5.0f, -2.0f}, {11.0f, -10.0f}, {17.0f, -14.0f}, {23.0f, -20.0f}},  2.0f, 30.0f, 0.25f }, // 17: Very Small Room
    { 5,  1.1f, 0.85f, 0.95f,  6.0f, 21.0f, {{13.0f, -5.0f}, {21.0f, -9.0f}, {29.0f, -12.0f}, {38.0f, -17.0f}},  6.0f, 48.0f, 0.4f }, // 18: Drum Booth
    {12,  5.5f, 1.35f, 0.32f, 50.0f, 10.0f, {{52.0f, -2.0f}, {78.0f, -7.0f}, {105.0f, -11.0f}, {140.0f, -15.0f}}, 24.0f, 80.0f, 0.68f }, // 19: Large Stone Hall
    // Presets 20-51 will be implicitly zero-initialized by the C compiler
};


// Forward declarations
static void _SituationStudioReverbDestroy(SituationStudioReverb* reverb);
static void _SituationStudioReverbSetPreset(SituationStudioReverb* reverb, int preset_index);

static SituationStudioReverb* _SituationStudioReverbCreate(int sample_rate) {
    SituationStudioReverb* reverb = (SituationStudioReverb*)SIT_MALLOC(sizeof(SituationStudioReverb));
    if (!reverb) return NULL;
    memset(reverb, 0, sizeof(SituationStudioReverb));

    reverb->sample_rate = sample_rate;
    reverb->max_delay_samples = (int)(1.5f * sample_rate + 0.5f); // 1.5 second buffer

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
        if (!reverb->early_reflections_buffers[i]) { _SituationStudioReverbDestroy(reverb); return NULL; }
    }
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
        if (!reverb->fdn_delays[i]) { _SituationStudioReverbDestroy(reverb); return NULL; }
    }
    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS; i++) {
        if (!reverb->allpass_buffers[i]) { _SituationStudioReverbDestroy(reverb); return NULL; }
    }

    // Initialize LFO state
    reverb->lfo_x = 1.0f;
    reverb->lfo_y = 0.0f;

    // Load default preset to populate pre-calculated DSP variables
    _SituationStudioReverbSetPreset(reverb, 0);

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

    // --- Pre-calculate all DSP variables to save CPU in the process loop ---

    reverb->pre_delay_samples = (int)(params.pre_delay_ms * reverb->sample_rate / 1000.0f);

    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS; i++) {
        reverb->early_delays_samples[i] = (int)(params.early_reflections[i][0] * reverb->sample_rate / 1000.0f);
        reverb->early_gains[i] = powf(10.0f, params.early_reflections[i][1] / 20.0f);
    }

    reverb->diffusion_gain = 0.5f + ((params.diffusion_db - 30.0f) / 70.0f) * 0.35f;
    if (reverb->diffusion_gain > 0.95f) reverb->diffusion_gain = 0.95f;

    float size_factor = params.size / 13.0f;
    static const float fdn_base_ms[SIT_STUDIO_REVERB_NUM_FDN_DELAYS] = { 37.1f, 41.3f, 47.9f, 53.1f, 59.3f, 67.7f, 73.3f, 83.9f };
    static const float ap_base_ms[SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS] = { 5.1f, 7.7f, 11.3f, 13.1f };

    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
        reverb->fdn_delays_samples[i] = fdn_base_ms[i] * (0.5f + 0.5f * size_factor) * reverb->sample_rate / 1000.0f;
        reverb->fdn_gains[i] = powf(10.0f, -3.0f * reverb->fdn_delays_samples[i] / (params.decay_time * reverb->sample_rate));
    }

    for (int i = 0; i < SIT_STUDIO_REVERB_NUM_ALLPASS_FILTERS; i++) {
        reverb->ap_delays_samples[i] = (int)(ap_base_ms[i] * (0.5f + 0.5f * size_factor) * reverb->sample_rate / 1000.0f);
    }

    // Treble damping: High treble_coef = less damping (brighter)
    reverb->hf_damping = 0.4f * (1.0f / params.treble_coef);
    if (reverb->hf_damping > 0.95f) reverb->hf_damping = 0.95f;

    // Bass damping: High bass_coef = less bass cut (heavier bass)
    reverb->lf_damping = 0.02f * (1.0f / params.bass_coef);
    if (reverb->lf_damping > 0.5f) reverb->lf_damping = 0.5f;

    reverb->wet_gain = powf(10.0f, -params.reverb_atten_db / 20.0f);
    reverb->dry_gain = 1.0f - params.wet_mix;

    // LFO Configuration (0.5 Hz)
    float theta = 6.283185307f * 0.5f / reverb->sample_rate;
    reverb->lfo_cos_theta = cosf(theta);
    reverb->lfo_sin_theta = sinf(theta);
}

static void _SituationStudioReverbSetPreset(SituationStudioReverb* reverb, int preset_index) {
    if (preset_index < 0 || preset_index >= 52) return;
    _SituationStudioReverbSetParams(reverb, studio_reverb_presets[preset_index]);
}

static void _SituationStudioReverbProcess(SituationStudioReverb* reverb, const float* in_left, const float* in_right,
                    float* out_left, float* out_right, int num_samples) {
    
    // Pre-calculated trigonometric offsets for LFO phasing
    static const float lfo_cos[8] = { 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f, 0.7071f };
    static const float lfo_sin[8] = { 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f };

    for (int n = 0; n < num_samples; n++) {
        float input = (in_left[n] + in_right[n]) * 0.5f;

        // 1. Stereo Discorrelator
        int discorrelator_delay_left = (int)(reverb->params.stereo_discorrelator);
        int discorrelator_delay_right = -discorrelator_delay_left;
        int read_idx_left = (reverb->discorrelator_idx - discorrelator_delay_left + SIT_STUDIO_REVERB_DISCORRELATOR_DELAY) % SIT_STUDIO_REVERB_DISCORRELATOR_DELAY;
        int read_idx_right = (reverb->discorrelator_idx - discorrelator_delay_right + SIT_STUDIO_REVERB_DISCORRELATOR_DELAY) % SIT_STUDIO_REVERB_DISCORRELATOR_DELAY;
        
        float discorrelator_out_left = reverb->discorrelator_left[read_idx_left];
        float discorrelator_out_right = reverb->discorrelator_right[read_idx_right];
        reverb->discorrelator_left[reverb->discorrelator_idx] = input;
        reverb->discorrelator_right[reverb->discorrelator_idx] = input;

        // 2. Pre-delay
        int pre_delay_read_idx = (reverb->pre_delay_idx - reverb->pre_delay_samples + reverb->max_delay_samples) % reverb->max_delay_samples;
        reverb->pre_delay_left[reverb->pre_delay_idx] = discorrelator_out_left;
        reverb->pre_delay_right[reverb->pre_delay_idx] = discorrelator_out_right;
        
        float pre_delay_out_left = reverb->pre_delay_left[pre_delay_read_idx];
        float pre_delay_out_right = reverb->pre_delay_right[pre_delay_read_idx];

        // 3. Early reflections
        float early_left = 0.0f, early_right = 0.0f;
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_EARLY_REFLECTIONS; i++) {
            int read_idx = (reverb->early_reflections_idx[i] - reverb->early_delays_samples[i] + reverb->max_delay_samples) % reverb->max_delay_samples;
            float sample = reverb->early_reflections_buffers[i][read_idx];
            if (i == 0 || i == 2) early_left += sample * reverb->early_gains[i];
            else early_right += sample * reverb->early_gains[i];
            reverb->early_reflections_buffers[i][reverb->early_reflections_idx[i]] = (i % 2 == 0) ? pre_delay_out_left : pre_delay_out_right;
        }

        // 4. Stereo Diffusion Network (All-pass filters)
        float ap_out_l = pre_delay_out_left;
        for (int i = 0; i < 2; i++) {
            int read_idx = (reverb->allpass_idx[i] - reverb->ap_delays_samples[i] + reverb->max_delay_samples) % reverb->max_delay_samples;
            float delayed_sample = reverb->allpass_buffers[i][read_idx];
            float temp = ap_out_l;
            ap_out_l = -reverb->diffusion_gain * ap_out_l + delayed_sample;
            reverb->allpass_buffers[i][reverb->allpass_idx[i]] = temp + reverb->diffusion_gain * ap_out_l;
        }
        
        float ap_out_r = pre_delay_out_right;
        for (int i = 2; i < 4; i++) {
            int read_idx = (reverb->allpass_idx[i] - reverb->ap_delays_samples[i] + reverb->max_delay_samples) % reverb->max_delay_samples;
            float delayed_sample = reverb->allpass_buffers[i][read_idx];
            float temp = ap_out_r;
            ap_out_r = -reverb->diffusion_gain * ap_out_r + delayed_sample;
            reverb->allpass_buffers[i][reverb->allpass_idx[i]] = temp + reverb->diffusion_gain * ap_out_r;
        }

        // 5. Complex Rotation LFO (Zero-trig oscillator)
        float new_x = reverb->lfo_x * reverb->lfo_cos_theta - reverb->lfo_y * reverb->lfo_sin_theta;
        float new_y = reverb->lfo_x * reverb->lfo_sin_theta + reverb->lfo_y * reverb->lfo_cos_theta;
        
        // Taylor series normalization to prevent drift
        float mag_sq = new_x * new_x + new_y * new_y;
        float norm = 1.5f - 0.5f * mag_sq;
        reverb->lfo_x = new_x * norm;
        reverb->lfo_y = new_y * norm;
        
        float base_cos = reverb->lfo_x;
        float base_sin = reverb->lfo_y;

        // 6. FDN Read & Interpolation
        float fdn_outs[SIT_STUDIO_REVERB_NUM_FDN_DELAYS];
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
            float mod = (base_sin * lfo_cos[i] + base_cos * lfo_sin[i]) * 8.0f; 
            float total_delay = reverb->fdn_delays_samples[i] + mod;
            
            int delay_int = (int)total_delay;
            float frac = total_delay - delay_int;
            
            int read_idx1 = (reverb->fdn_idx[i] - delay_int + reverb->max_delay_samples) % reverb->max_delay_samples;
            int read_idx2 = (read_idx1 - 1 + reverb->max_delay_samples) % reverb->max_delay_samples;
            
            float s1 = reverb->fdn_delays[i][read_idx1];
            float s2 = reverb->fdn_delays[i][read_idx2];
            fdn_outs[i] = s1 + frac * (s2 - s1); // Linear interpolation
        }

        // 7. Householder Feedback Matrix
        float fdn_sum = 0.0f;
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) fdn_sum += fdn_outs[i];
        fdn_sum *= 0.25f; // 2.0f / N for N=8

        float fdn_in[SIT_STUDIO_REVERB_NUM_FDN_DELAYS];
        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
            fdn_in[i] = fdn_outs[i] - fdn_sum;
        }

        // 8. Apply Filters, Gains, and Inject Input
        float fdn_out_mix_left = 0.0f;
        float fdn_out_mix_right = 0.0f;

        for (int i = 0; i < SIT_STUDIO_REVERB_NUM_FDN_DELAYS; i++) {
            // Low-pass filter (Treble damping)
            float lp_out = fdn_in[i] * (1.0f - reverb->hf_damping) + reverb->hf_damping * reverb->fdn_lpf_state[i];
            reverb->fdn_lpf_state[i] = lp_out;
            
            // High-pass filter (Bass damping via DC-blocker topology)
            reverb->fdn_hpf_state[i] += reverb->lf_damping * (lp_out - reverb->fdn_hpf_state[i]);
            float hp_out = lp_out - reverb->fdn_hpf_state[i];
            
            // Apply exact T60 decay gain
            float feedback_sig = hp_out * reverb->fdn_gains[i];
            
            // Safe energy injection alternating phase (+, -, -, +) to prevent DC buildup
            float injection = (i % 2 == 0) ? (ap_out_l * 0.15f) : (ap_out_r * 0.15f);
            if (i % 4 >= 2) injection = -injection;
            
            reverb->fdn_delays[i][reverb->fdn_idx[i]] = feedback_sig + injection;
            
            // Accumulate damped signals to L/R outputs to create the wide stereo tail
            if (i % 2 == 0) fdn_out_mix_left += hp_out;
            else fdn_out_mix_right += hp_out;
        }

        // Normalize FDN outputs (approx 1/sqrt(8) to maintain energy scale)
        fdn_out_mix_left *= 0.3535f;
        fdn_out_mix_right *= 0.3535f;

        // 9. Final Mixing
        float wet_left = (early_left + fdn_out_mix_left) * reverb->wet_gain;
        float wet_right = (early_right + fdn_out_mix_right) * reverb->wet_gain;
        
        out_left[n] = STUDIO_REV_FMA(wet_left, reverb->params.wet_mix, in_left[n] * reverb->dry_gain);
        out_right[n] = STUDIO_REV_FMA(wet_right, reverb->params.wet_mix, in_right[n] * reverb->dry_gain);

        // 10. Update pointers
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
    return 0; // Sample-by-sample processing with dry signal included in output
}

#endif // SIT_AUX_STUDIO_REVERB_H