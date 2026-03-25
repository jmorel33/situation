/***************************************************************************************************
*
*   reverb.h - Internal Reverb Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Standalone implementation of the Schroeder/Freeverb reverb algorithm.
*   This file is intended to be included within the audio subsystem implementation.
*
***************************************************************************************************/

#ifndef SIT_AUX_REVERB_H
#define SIT_AUX_REVERB_H

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

// --- Internal Reverb Implementation (Schroeder/Freeverb) ---
// Constants for 44.1kHz (will scale for 48k)
#define SIT_REVERB_COMB_COUNT 8
#define SIT_REVERB_ALLPASS_COUNT 4
#define SIT_REVERB_STEREO_SPREAD 23

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
    SituationReverbComb combs[SIT_REVERB_COMB_COUNT];
    SituationReverbAllPass allpasses[SIT_REVERB_ALLPASS_COUNT];
    float room_size;
    float damp;
    float wet;
    float dry;
    float width;
    uint32_t sample_rate;
} SituationReverbState;

/**
 * @brief [INTERNAL] Processes a single sample through a Schroeder comb filter (FMA-optimized).
 * @details The comb filter creates a series of decaying echoes by feeding the output back into the input
 *          through a delay buffer. It also includes a low-pass filter in the feedback loop to simulate
 *          the absorption of high frequencies by air and walls (damping).
 *
 * @param comb Pointer to the comb filter state.
 * @param input The input audio sample (normalized float).
 * @return The processed output sample.
 */
static float _sit_reverb_comb_process(SituationReverbComb* comb, float input) {
    float output = comb->buffer[comb->cursor];
    // FMA-optimized one-pole lowpass: y = a*y_prev + (1-a)*x
    comb->filter_store = REVERB_FMA(comb->damp, comb->filter_store, (1.0f - comb->damp) * output);
    // FMA-optimized feedback: input + feedback*delayed
    comb->buffer[comb->cursor] = REVERB_FMA(comb->feedback, comb->filter_store, input);
    if (++comb->cursor >= comb->size) comb->cursor = 0;
    return output;
}

/**
 * @brief [INTERNAL] Processes a single sample through an All-Pass filter.
 * @details All-pass filters change the phase relationship of frequencies without altering their amplitude response.
 *          In reverb algorithms, they are used to increase the "density" of the reflections, diffusing the
 *          distinct echoes from the comb filters into a smooth wash of sound.
 *
 * @param ap Pointer to the all-pass filter state.
 * @param input The input audio sample (normalized float).
 * @return The processed output sample.
 */
static float _sit_reverb_allpass_process(SituationReverbAllPass* ap, float input) {
    float buffered = ap->buffer[ap->cursor];
    float output = -input + buffered;
    ap->buffer[ap->cursor] = input + (buffered * ap->feedback);
    if (++ap->cursor >= ap->size) ap->cursor = 0;
    return output;
}

/**
 * @brief [INTERNAL] Frees all memory associated with the reverb state.
 * @details This function iterates through all comb and all-pass filters, freeing their internal delay buffers,
 *          and then frees the main state structure itself.
 *
 * @param state_ptr A void pointer to the `SituationReverbState` struct to destroy.
 */
static void _SituationUninitReverb(void* state_ptr) {
    if (!state_ptr) return;
    SituationReverbState* rev = (SituationReverbState*)state_ptr;
    for(int i=0; i<SIT_REVERB_COMB_COUNT; ++i) SIT_FREE(rev->combs[i].buffer);
    for(int i=0; i<SIT_REVERB_ALLPASS_COUNT; ++i) SIT_FREE(rev->allpasses[i].buffer);
    SIT_FREE(rev);
}

/**
 * @brief [INTERNAL] Allocates and initializes the Schroeder/Freeverb reverb engine state.
 * @details This function sets up the complex network of comb and all-pass filters required for the reverb effect.
 *          It scales the delay line lengths based on the provided sample rate to ensure consistent timing
 *          across different audio configurations (e.g., 44.1kHz vs 48kHz).
 *
 * @param sample_rate The sample rate of the audio context (e.g., 48000).
 * @return A void pointer to the opaque `SituationReverbState` struct, or NULL on allocation failure.
 */
static void* _SituationInitReverb(uint32_t sample_rate) {
    SituationReverbState* rev = (SituationReverbState*)SIT_CALLOC(1, sizeof(SituationReverbState));
    if (!rev) return NULL;

    rev->sample_rate = sample_rate;
    float scale = (float)sample_rate / 44100.0f;

    // Tuning values (Schroeder/Freeverb defaults scaled)
    const int comb_tunings[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    const int allpass_tunings[] = {225, 341, 441, 556};

    for(int i=0; i<SIT_REVERB_COMB_COUNT; ++i) {
        rev->combs[i].size = (int)(comb_tunings[i] * scale);
        rev->combs[i].buffer = (float*)SIT_CALLOC(rev->combs[i].size, sizeof(float));
        rev->combs[i].feedback = 0.5f; // Initial room size
        rev->combs[i].damp = 0.5f;     // Initial damp
    }

    for(int i=0; i<SIT_REVERB_ALLPASS_COUNT; ++i) {
        rev->allpasses[i].size = (int)(allpass_tunings[i] * scale);
        rev->allpasses[i].buffer = (float*)SIT_CALLOC(rev->allpasses[i].size, sizeof(float));
        rev->allpasses[i].feedback = 0.5f;
    }

    rev->room_size = 0.5f;
    rev->damp = 0.5f;
    rev->wet = 0.3f;
    rev->dry = 1.0f;
    rev->width = 1.0f;

    return rev;
}

/**
 * @brief [INTERNAL] Processes a block of audio through the reverb engine.
 * @details This is the main DSP loop for the reverb effect. It takes an input buffer (stereo or mono),
 *          downmixes it to mono for processing, runs it through the parallel comb filters and series all-pass filters,
 *          and then mixes the wet (reverberated) signal back into the output buffer with stereo spreading.
 *
 * @param state_ptr A void pointer to the `SituationReverbState` struct.
 * @param pOutput The output buffer to write mixed audio to (interleaved).
 * @param pInput The input buffer containing the dry signal (interleaved).
 * @param frameCount The number of frames to process.
 * @param channels The number of channels (must be uniform for input/output).
 */
static void _SituationProcessReverb(void* state_ptr, float* pOutput, const float* pInput, uint32_t frameCount, int channels) {
    if (!state_ptr) return;
    SituationReverbState* rev = (SituationReverbState*)state_ptr;

    // Apply parameters
    float room_scale = rev->room_size * 0.28f + 0.7f; // Scale to stable range
    for(int i=0; i<SIT_REVERB_COMB_COUNT; ++i) {
        rev->combs[i].feedback = room_scale;
        rev->combs[i].damp = rev->damp * 0.4f;
    }

    for (uint32_t i = 0; i < frameCount; ++i) {
        float in_sample = 0.0f;
        // Downmix input to mono for reverb engine
        for(int c=0; c<channels; ++c) in_sample += pInput[i*channels + c];
        in_sample *= (0.015f / channels); // Gain compensation

        float out = 0.0f;
        for(int j=0; j<SIT_REVERB_COMB_COUNT; ++j) {
            out += _sit_reverb_comb_process(&rev->combs[j], in_sample);
        }

        for(int j=0; j<SIT_REVERB_ALLPASS_COUNT; ++j) {
            out = _sit_reverb_allpass_process(&rev->allpasses[j], out);
        }

        // Apply Wet/Dry mix
        for(int c=0; c<channels; ++c) {
            float wet_sig = out * rev->wet;
            // Simple stereo spread
            if (c%2==1) wet_sig *= -1.0f; // Phase invert right channel for wideness

            pOutput[i*channels + c] = (pInput[i*channels + c] * rev->dry) + wet_sig;
        }
    }
}

#endif // SIT_AUX_REVERB_H
