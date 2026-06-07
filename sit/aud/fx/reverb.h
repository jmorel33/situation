/***************************************************************************************************
*
*   reverb.h - Internal Reverb Implementation (v1.1 - 2026/04/19)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Standalone implementation of a modern Schroeder/Freeverb-derived reverb algorithm.
*   Features true stereo processing, early reflections, LFO modulation, and M/S width control.
*
***************************************************************************************************/

#ifndef SIT_AUX_REVERB_H
#define SIT_AUX_REVERB_H

#include <math.h>

// FMA detection
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define REVERB_HAS_FMA 1
    #if defined(__GNUC__) || defined(__clang__)
        #define REVERB_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
    #else
        #define REVERB_FMA(a, b, c) fmaf((a), (b), (c))
    #endif
#else
    #define REVERB_HAS_FMA 0
    #define REVERB_FMA(a, b, c) ((a) * (b) + (c))
#endif

// Constants for 44.1kHz (will scale for 48k+)
#define SIT_REVERB_COMB_COUNT 8
#define SIT_REVERB_ALLPASS_COUNT 6
#define SIT_REVERB_STEREO_SPREAD 23
#define SIT_REVERB_INPUT_GAIN 0.015f
#define SIT_REVERB_COMB_NORM (1.0f / (float)SIT_REVERB_COMB_COUNT)

typedef struct {
    float* buffer;
    int size;
    int cursor;
    float feedback;
    float filter_store;
    float damp;
} SituationReverbComb;

typedef struct {
    float* buffer;
    int size;
    int cursor;
    float feedback;
} SituationReverbAllPass;

typedef struct {
    // True stereo separate filters
    SituationReverbComb combs_l[SIT_REVERB_COMB_COUNT];
    SituationReverbComb combs_r[SIT_REVERB_COMB_COUNT];
    SituationReverbAllPass allpasses_l[SIT_REVERB_ALLPASS_COUNT];
    SituationReverbAllPass allpasses_r[SIT_REVERB_ALLPASS_COUNT];
    
    // Shared history buffer for Predelay and Early Reflections
    float* history_l;
    float* history_r;
    int history_size;
    int history_cursor;

    // Early Reflection Taps
    int er_taps_l[4];
    int er_taps_r[4];
    float er_gains[4];
    
    // Modulation LFO
    float lfo_phase;
    
    // Public Parameters
    float room_size;
    float damp;
    float wet;
    float dry;
    float width;
    float predelay_ms;
    float diffusion;
    float mod_rate;
    float mod_depth;
    
    uint32_t sample_rate;
} SituationReverbState;

/**
 * @brief [INTERNAL] Reads from a delay buffer using Hermite (Catmull-Rom) interpolation.
 * @details Preserves high frequencies much better than linear interpolation during modulation.
 */
static inline float _sit_reverb_hermite_read(const float* buffer, int size, float read_idx) {
    int idx_int = (int)read_idx;
    float frac = read_idx - idx_int;

    // Fast wrap-around without modulo for adjacent indices
    int i0 = idx_int - 1; if (i0 < 0) i0 += size;
    int i1 = idx_int;
    int i2 = idx_int + 1; if (i2 >= size) i2 -= size;
    int i3 = idx_int + 2; if (i3 >= size) i3 -= size;

    float y0 = buffer[i0];
    float y1 = buffer[i1];
    float y2 = buffer[i2];
    float y3 = buffer[i3];

    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return y1 + frac * (c1 + frac * (c2 + frac * c3));
}

/**
 * @brief [INTERNAL] Processes a single sample through a Schroeder comb filter with modulated delay.
 */
static float _sit_reverb_comb_process(SituationReverbComb* comb, float input, float mod_offset) {
    float read_idx = (float)comb->cursor + mod_offset;
    if (read_idx >= comb->size) read_idx -= comb->size;
    if (read_idx < 0) read_idx += comb->size;
    
    float output = _sit_reverb_hermite_read(comb->buffer, comb->size, read_idx);
    
    comb->filter_store = REVERB_FMA(comb->damp, comb->filter_store, (1.0f - comb->damp) * output);
    comb->buffer[comb->cursor] = REVERB_FMA(comb->feedback, comb->filter_store, input);
    
    if (++comb->cursor >= comb->size) comb->cursor = 0;
    return output;
}

/**
 * @brief [INTERNAL] Processes a single sample through an All-Pass filter with modulated delay.
 */
static float _sit_reverb_allpass_process(SituationReverbAllPass* ap, float input, float mod_offset) {
    float read_idx = (float)ap->cursor + mod_offset;
    if (read_idx >= ap->size) read_idx -= ap->size;
    if (read_idx < 0) read_idx += ap->size;

    float delayed = _sit_reverb_hermite_read(ap->buffer, ap->size, read_idx);
    
    float output = -input + delayed;
    ap->buffer[ap->cursor] = input + (delayed * ap->feedback);
    
    if (++ap->cursor >= ap->size) ap->cursor = 0;
    return output;
}

/**
 * @brief [INTERNAL] Frees all memory associated with the reverb state.
 */
static void _SituationUninitReverb(void* state_ptr) {
    if (!state_ptr) return;
    SituationReverbState* rev = (SituationReverbState*)state_ptr;
    
    for(int i=0; i<SIT_REVERB_COMB_COUNT; ++i) {
        SIT_FREE(rev->combs_l[i].buffer);
        SIT_FREE(rev->combs_r[i].buffer);
    }
    for(int i=0; i<SIT_REVERB_ALLPASS_COUNT; ++i) {
        SIT_FREE(rev->allpasses_l[i].buffer);
        SIT_FREE(rev->allpasses_r[i].buffer);
    }
    if (rev->history_l) SIT_FREE(rev->history_l);
    if (rev->history_r) SIT_FREE(rev->history_r);
    
    SIT_FREE(rev);
}

/**
 * @brief [INTERNAL] Allocates and initializes the true-stereo reverb engine state.
 */
static SituationError _SituationInitReverb(uint32_t sample_rate, void** out_state) {
    if (!out_state) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    *out_state = NULL;

    SituationReverbState* rev = (SituationReverbState*)SIT_CALLOC(1, sizeof(SituationReverbState));
    if (!rev) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_MEMORY_ALLOCATION, "Reverb state allocation failed.");
    }

    rev->sample_rate = sample_rate;
    float scale = (float)sample_rate / 44100.0f;

    const int comb_tunings[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    const int allpass_tunings[] = {225, 341, 441, 556, 161, 719};

    for (int i = 0; i < SIT_REVERB_COMB_COUNT; ++i) {
        rev->combs_l[i].size = (int)(comb_tunings[i] * scale);
        rev->combs_l[i].buffer = (float*)SIT_CALLOC(rev->combs_l[i].size, sizeof(float));
        rev->combs_r[i].size = (int)((comb_tunings[i] + SIT_REVERB_STEREO_SPREAD) * scale);
        rev->combs_r[i].buffer = (float*)SIT_CALLOC(rev->combs_r[i].size, sizeof(float));
        if (!rev->combs_l[i].buffer || !rev->combs_r[i].buffer) {
            _SituationUninitReverb(rev);
            return _SituationSetErrorFromCode(
                SITUATION_ERROR_MEMORY_ALLOCATION, "Reverb comb buffer allocation failed.");
        }
    }

    for (int i = 0; i < SIT_REVERB_ALLPASS_COUNT; ++i) {
        rev->allpasses_l[i].size = (int)(allpass_tunings[i] * scale);
        rev->allpasses_l[i].buffer = (float*)SIT_CALLOC(rev->allpasses_l[i].size, sizeof(float));
        rev->allpasses_r[i].size = (int)((allpass_tunings[i] + SIT_REVERB_STEREO_SPREAD) * scale);
        rev->allpasses_r[i].buffer = (float*)SIT_CALLOC(rev->allpasses_r[i].size, sizeof(float));
        if (!rev->allpasses_l[i].buffer || !rev->allpasses_r[i].buffer) {
            _SituationUninitReverb(rev);
            return _SituationSetErrorFromCode(
                SITUATION_ERROR_MEMORY_ALLOCATION, "Reverb allpass buffer allocation failed.");
        }
    }

    rev->history_size = (int)(sample_rate * 0.15f);
    if (rev->history_size < 1) {
        rev->history_size = 1;
    }
    rev->history_l = (float*)SIT_CALLOC(rev->history_size, sizeof(float));
    rev->history_r = (float*)SIT_CALLOC(rev->history_size, sizeof(float));
    if (!rev->history_l || !rev->history_r) {
        _SituationUninitReverb(rev);
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_MEMORY_ALLOCATION, "Reverb history buffer allocation failed.");
    }

    const float er_t_l[4] = { 7.4f,  17.2f, 26.5f, 41.1f };
    const float er_t_r[4] = { 9.3f,  14.8f, 31.2f, 45.7f };
    const float er_g[4]   = { 0.50f, 0.25f, 0.12f, 0.06f };

    for (int i = 0; i < 4; i++) {
        rev->er_taps_l[i] = (int)(er_t_l[i] * sample_rate / 1000.0f);
        rev->er_taps_r[i] = (int)(er_t_r[i] * sample_rate / 1000.0f);
        rev->er_gains[i]  = er_g[i];
    }

    rev->room_size = 0.6f;
    rev->damp = 0.4f;
    rev->wet = 0.3f;
    rev->dry = 0.7f;
    rev->width = 1.0f;
    rev->predelay_ms = 15.0f; 
    rev->diffusion = 0.7f;
    rev->mod_rate = 1.2f;
    rev->mod_depth = 0.5f;

    rev->lfo_phase = 0.0f;

    *out_state = rev;
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Processes a block of audio through the true-stereo reverb engine.
 * HARDENING: void by design — real-time audio path (Phase 7 / Phase 9).
 */
static void _SituationProcessReverb(void* state_ptr, float* pOutput, const float* pInput, uint32_t frameCount, int channels) {
    if (!state_ptr) return;
    SituationReverbState* rev = (SituationReverbState*)state_ptr;

    // Scale parameters to musical ranges
    float room_scale = rev->room_size * 0.48f + 0.5f; // 0.5 to 0.98
    float damp_scale = rev->damp * 0.8f;              // 0.0 to 0.8
    float diff_scale = rev->diffusion * 0.6f + 0.3f;  // 0.3 to 0.9 (lush)

    for(int i=0; i<SIT_REVERB_COMB_COUNT; ++i) {
        rev->combs_l[i].feedback = room_scale;
        rev->combs_l[i].damp = damp_scale;
        rev->combs_r[i].feedback = room_scale;
        rev->combs_r[i].damp = damp_scale;
    }

    for(int i=0; i<SIT_REVERB_ALLPASS_COUNT; ++i) {
        rev->allpasses_l[i].feedback = diff_scale;
        rev->allpasses_r[i].feedback = diff_scale;
    }

    // Modulation calculation
    float lfo_inc = 2.0f * 3.14159265359f * rev->mod_rate / (float)rev->sample_rate;
    float mod_depth_samples = rev->mod_depth * 12.0f * ((float)rev->sample_rate / 44100.0f);

    // Predelay calculation
    int pd_samples = (int)(rev->predelay_ms * rev->sample_rate / 1000.0f);
    if (pd_samples >= rev->history_size) pd_samples = rev->history_size - 1;
    if (pd_samples < 0) pd_samples = 0;

    for (uint32_t i = 0; i < frameCount; ++i) {
        // True Stereo Input with gentle crossfeed for realism
        float in_l = pInput[i * channels];
        float in_r = (channels > 1) ? pInput[i * channels + 1] : in_l;
        
        float in_l_scaled = in_l * SIT_REVERB_INPUT_GAIN;
        float in_r_scaled = in_r * SIT_REVERB_INPUT_GAIN;
        
        float mix_l = in_l_scaled + in_r_scaled * 0.2f;
        float mix_r = in_r_scaled + in_l_scaled * 0.2f;

        // Write to history buffer
        rev->history_l[rev->history_cursor] = mix_l;
        rev->history_r[rev->history_cursor] = mix_r;

        // Extract Early Reflections
        float er_out_l = 0.0f;
        float er_out_r = 0.0f;
        for(int t=0; t<4; t++) {
            int idx_l = rev->history_cursor - rev->er_taps_l[t];
            if (idx_l < 0) idx_l += rev->history_size;
            er_out_l += rev->history_l[idx_l] * rev->er_gains[t];

            int idx_r = rev->history_cursor - rev->er_taps_r[t];
            if (idx_r < 0) idx_r += rev->history_size;
            er_out_r += rev->history_r[idx_r] * rev->er_gains[t];
        }

        // Extract Late Predelay
        int read_idx = rev->history_cursor - pd_samples;
        if (read_idx < 0) read_idx += rev->history_size;
        float pd_out_l = rev->history_l[read_idx];
        float pd_out_r = rev->history_r[read_idx];

        if (++rev->history_cursor >= rev->history_size) rev->history_cursor = 0;

        // Advance LFO
        rev->lfo_phase += lfo_inc;
        if (rev->lfo_phase > 6.28318530718f) rev->lfo_phase -= 6.28318530718f;

        float late_l = 0.0f;
        float late_r = 0.0f;

        // Process True Stereo Combs with decorrelated LFO modulation
        for(int j=0; j<SIT_REVERB_COMB_COUNT; ++j) {
            float phase_offset = (float)j * 0.785398f;
            float lfo_l = (sinf(rev->lfo_phase + phase_offset) + 1.0f) * mod_depth_samples;
            // 180-degree phase offset for right channel decorrelation
            float lfo_r = (sinf(rev->lfo_phase + phase_offset + 3.14159265f) + 1.0f) * mod_depth_samples; 
            
            late_l += _sit_reverb_comb_process(&rev->combs_l[j], pd_out_l, lfo_l);
            late_r += _sit_reverb_comb_process(&rev->combs_r[j], pd_out_r, lfo_r);
        }
        late_l *= SIT_REVERB_COMB_NORM;
        late_r *= SIT_REVERB_COMB_NORM;

        // Process True Stereo All-Passes (also modulated for maximum smoothness)
        for(int j=0; j<SIT_REVERB_ALLPASS_COUNT; ++j) {
            float phase_offset = (float)j * 1.047197f;
            float lfo_l = (sinf(rev->lfo_phase + phase_offset) + 1.0f) * (mod_depth_samples * 0.5f);
            float lfo_r = (sinf(rev->lfo_phase + phase_offset + 3.14159265f) + 1.0f) * (mod_depth_samples * 0.5f);
            
            late_l = _sit_reverb_allpass_process(&rev->allpasses_l[j], late_l, lfo_l);
            late_r = _sit_reverb_allpass_process(&rev->allpasses_r[j], late_r, lfo_r);
        }

        // Combine ER and Late Reverb (ER gains already scaled in er_gains[])
        float out_l = er_out_l + late_l;
        float out_r = er_out_r + late_r;

        // M/S Width processing (prevents phase cancellation at extreme widths)
        float mid  = (out_l + out_r) * 0.5f;
        float side = (out_l - out_r) * 0.5f * rev->width;
        out_l = (mid + side) * rev->wet;
        out_r = (mid - side) * rev->wet;

        // Output Mix
        for(int c=0; c<channels; ++c) {
            float wet_sig = (c % 2 == 0) ? out_l : out_r;
            // If mono output, sum L and R reverb tails
            if (channels == 1) wet_sig = (out_l + out_r) * 0.5f; 
            
            pOutput[i*channels + c] = (pInput[i*channels + c] * rev->dry) + wet_sig;
        }
    }
}

#endif // SIT_AUX_REVERB_H