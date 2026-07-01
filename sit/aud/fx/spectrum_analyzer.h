/***************************************************************************************************
*
*   sit/aud/fx/spectrum_analyzer.h - Spectrum Analyzer (Analyzer)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   FFT-based frequency spectrum analyzer for display. Accumulates samples into a window,
*   computes magnitude spectrum when full. Audio passes through unmodified.
*
*   Controls:
*     [0] fft_size — FFT window size (256, 512, or 1024; default 512)
*
*   State is polled from the main thread for UI display via SituationGetSpectrumData().
*
***************************************************************************************************/

#ifndef SITUATION_SPECTRUM_ANALYZER_H
#define SITUATION_SPECTRUM_ANALYZER_H

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef SIT_MALLOC
#define SIT_MALLOC(s) malloc(s)
#define SIT_FREE(p) free(p)
#endif

#define SIT_SPECTRUM_MAX_FFT_SIZE 1024

// ================================================================================================
// STATE
// ================================================================================================

typedef struct {
    float* time_buffer;       // Accumulated input samples (mono downmix)
    float* fft_real;          // FFT real part (working buffer)
    float* fft_imag;          // FFT imaginary part (working buffer)
    float* magnitude;         // Output magnitude bins (fft_size/2)
    int fft_size;             // Current FFT size (256, 512, or 1024)
    int write_pos;            // Circular write position
    bool ready;               // New frame available for reading
} SituationSpectrumAnalyzerState;

// ================================================================================================
// SIMPLE RADIX-2 FFT (in-place, Cooley-Tukey)
// ================================================================================================

static inline void _sit_spectrum_fft(float* real, float* imag, int n) {
    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            float tr = real[i]; real[i] = real[j]; real[j] = tr;
            float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }
    
    // Cooley-Tukey butterfly
    for (int step = 1; step < n; step <<= 1) {
        float angle = -3.14159265358979323846f / (float)step;
        float wr = cosf(angle);
        float wi = sinf(angle);
        
        for (int group = 0; group < n; group += step << 1) {
            float cur_r = 1.0f;
            float cur_i = 0.0f;
            
            for (int pair = 0; pair < step; pair++) {
                int a = group + pair;
                int b = a + step;
                
                float tr = cur_r * real[b] - cur_i * imag[b];
                float ti = cur_r * imag[b] + cur_i * real[b];
                
                real[b] = real[a] - tr;
                imag[b] = imag[a] - ti;
                real[a] += tr;
                imag[a] += ti;
                
                float new_r = cur_r * wr - cur_i * wi;
                float new_i = cur_r * wi + cur_i * wr;
                cur_r = new_r;
                cur_i = new_i;
            }
        }
    }
}

// ================================================================================================
// FUNCTIONS
// ================================================================================================

static inline void situation_spectrum_init(SituationSpectrumAnalyzerState* state, int fft_size) {
    if (fft_size != 256 && fft_size != 512 && fft_size != 1024) {
        fft_size = 512;  // Default
    }
    
    state->fft_size = fft_size;
    state->write_pos = 0;
    state->ready = false;
    
    state->time_buffer = (float*)SIT_MALLOC(fft_size * sizeof(float));
    state->fft_real = (float*)SIT_MALLOC(fft_size * sizeof(float));
    state->fft_imag = (float*)SIT_MALLOC(fft_size * sizeof(float));
    state->magnitude = (float*)SIT_MALLOC((fft_size / 2) * sizeof(float));
    
    if (state->time_buffer) memset(state->time_buffer, 0, fft_size * sizeof(float));
    if (state->fft_real) memset(state->fft_real, 0, fft_size * sizeof(float));
    if (state->fft_imag) memset(state->fft_imag, 0, fft_size * sizeof(float));
    if (state->magnitude) memset(state->magnitude, 0, (fft_size / 2) * sizeof(float));
}

static inline void situation_spectrum_cleanup(SituationSpectrumAnalyzerState* state) {
    if (state->time_buffer) { SIT_FREE(state->time_buffer); state->time_buffer = NULL; }
    if (state->fft_real) { SIT_FREE(state->fft_real); state->fft_real = NULL; }
    if (state->fft_imag) { SIT_FREE(state->fft_imag); state->fft_imag = NULL; }
    if (state->magnitude) { SIT_FREE(state->magnitude); state->magnitude = NULL; }
}

/**
 * @brief Process spectrum analyzer — accumulates samples, computes FFT when window is full.
 * @param state Spectrum analyzer state.
 * @param input Input audio buffer (stereo interleaved).
 * @param output Output audio buffer (stereo interleaved — passthrough copy).
 * @param frames Number of frames.
 */
static inline void situation_spectrum_process(
    SituationSpectrumAnalyzerState* state,
    const float* input,
    float* output,
    int frames
) {
    if (!state->time_buffer || !state->fft_real || !state->fft_imag || !state->magnitude) {
        // Buffers not allocated — just passthrough
        memcpy(output, input, frames * 2 * sizeof(float));
        return;
    }
    
    for (int i = 0; i < frames; i++) {
        float l = input[i * 2];
        float r = input[i * 2 + 1];
        
        // Passthrough
        output[i * 2] = l;
        output[i * 2 + 1] = r;
        
        // Mono downmix for analysis
        float mono = (l + r) * 0.5f;
        state->time_buffer[state->write_pos] = mono;
        state->write_pos++;
        
        // When window is full, compute FFT
        if (state->write_pos >= state->fft_size) {
            state->write_pos = 0;
            
            // Apply Hann window and copy to FFT buffers
            for (int k = 0; k < state->fft_size; k++) {
                float window = 0.5f * (1.0f - cosf(2.0f * 3.14159265358979323846f * k / (float)(state->fft_size - 1)));
                state->fft_real[k] = state->time_buffer[k] * window;
                state->fft_imag[k] = 0.0f;
            }
            
            // Compute FFT
            _sit_spectrum_fft(state->fft_real, state->fft_imag, state->fft_size);
            
            // Compute magnitude (first half only — symmetric)
            int half = state->fft_size / 2;
            for (int k = 0; k < half; k++) {
                float re = state->fft_real[k];
                float im = state->fft_imag[k];
                state->magnitude[k] = sqrtf(re * re + im * im) / (float)state->fft_size;
            }
            
            state->ready = true;
        }
    }
}

#endif // SITUATION_SPECTRUM_ANALYZER_H
