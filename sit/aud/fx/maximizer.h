/***************************************************************************************************
*
*   maximizer.h - Spectral Multiband Maximizer Audio Effect Library
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   High-quality spectral maximizer and harmonic enhancer.
*   This file is intended to be included within the audio subsystem implementation.
*
***************************************************************************************************/

/**
 * @file maximizer.h
 * @brief Spectral Multiband Maximizer Audio Effect Library
 *
 * This library provides a spectral maximizer audio effect implemented in standard C11. It uses FFT/STFT processing
 * to enhance specific frequency bands and overtones, adding richness and clarity to the audio signal.
 * It is designed for mastering or detailed sound shaping.
 *
 * Key Features:
 * - **Spectral Enhancement**: Boosts specific frequency bands and their harmonics in the frequency domain.
 * - **Multiband Processing**: Allows defining multiple bands with custom Q, center frequency, and enhancement factors.
 * - **Overtone Generation**: Enhances upper harmonics of dominant frequencies within bands.
 * - **Optimized**: Uses SIMD intrinsics (SSE) for efficient processing of spectral data.
 * - **Built-in FFT**: Uses a custom highly optimized Radix-2 FFT (Zero external dependencies).
 *
 * Usage Overview:
 * 1. Initialize `MaximizerState` with `init_maximizer`.
 * 2. Configure bands using `set_band_params`.
 * 3. Process audio blocks using `maximizer_processor`.
 * 4. Free resources with `free_maximizer`.
 *
 * Dependencies: <math.h>, <stdlib.h>, <string.h>, <immintrin.h>.
 * Compile with -std=c11 -lm.
 *
 * Limitations:
 * - Assumes mono processing per instance (state is single-channel). For stereo, use two instances.
 * - Input/output buffers must match hop size (H), which will be enforced to a power of 2.
 */

#ifndef SIT_AUX_MAXIMIZER_H
#define SIT_AUX_MAXIMIZER_H

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <immintrin.h> // For SSE intrinsics

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Complex number definition (matches float[2] for SIMD compatibility)
typedef float sit_complex[2];

typedef struct {
    int N;
    int *bitrev;
    float *twiddle_re;
    float *twiddle_im;
} sit_fft_plan;

typedef struct {
    float center_freq;       // Center frequency of the band (Hz)
    float Q;                 // Quality factor (bandwidth control)
    float enhancement_factor;// Factor to amplify overtones
    int D;                   // Number of overtones to enhance
} BandParams;

typedef struct {
    int sample_rate;           // Audio sample rate (e.g., 44100 Hz)
    int W;                     // Window size for STFT
    int H;                     // Hop size (buffer size)
    int N;                     // FFT size (4x W for oversampling)
    int N_dominant;            // Number of dominant frequencies to enhance
    float cutoff_frequency;    // High-frequency cutoff in Hz
    float hpf_cutoff;          // High-pass filter cutoff frequency in Hz
    float lpf_cutoff;          // Low-pass filter cutoff frequency in Hz
    float *input_history;      // Buffer for past input samples
    float *output_accumulator; // Buffer for overlap-add output
    sit_complex *fft_buffer;   // Buffer for FFT output (complex floats)
    float *windowed_signal;    // Current window of samples (size N)
    float *window;             // Hann window coefficients (size W)
    sit_fft_plan fft_plan;     // Custom FFT Plan
    int num_bands;             // Number of bands (dynamic)
    BandParams *bands;         // Pointer to dynamically allocated band parameters
    float *max_enhancement;    // Maximum enhancement factor per bin (size N/2 + 1)
    float *total_multiplier;   // Total multiplier per bin (size N/2 + 1)
    float *magnitude;          // Magnitude spectrum (size N/2 + 1)
    float inv_N;               // 1.0 / N for normalization
    float fft_bin_scale;       // N / sample_rate for bin calculations
    float freq_scale;          // sample_rate / N for frequency calculations
    int half_W;                // W / 2 for convenience
} MaximizerState;

// Function prototypes
static void init_maximizer(MaximizerState *state, int sample_rate, int H, int N_dominant, float cutoff_frequency, int num_bands);
static void set_band_params(MaximizerState *state, int band_index, float center_freq, float Q, float enhancement_factor, int D);
static void set_hpf_cutoff(MaximizerState *state, float cutoff);
static void set_lpf_cutoff(MaximizerState *state, float cutoff);
static void free_maximizer(MaximizerState *state);
static void maximizer_processor(MaximizerState *state, float *in_buffer, float *out_buffer, unsigned int frames);
static int maximizer_get_latency_samples(MaximizerState *state);

// Helper to align size to 16 bytes
static inline size_t align_size_16(size_t size) {
    return (size + 15) & ~15;
}

// -------------------------------------------------------------------------------------------------
// Custom FFT Implementation
// -------------------------------------------------------------------------------------------------

static void sit_fft_init(sit_fft_plan *plan, int N) {
    plan->N = N;
    plan->bitrev = (int *)malloc(N * sizeof(int));
    plan->twiddle_re = (float *)malloc((N / 2) * sizeof(float));
    plan->twiddle_im = (float *)malloc((N / 2) * sizeof(float));

    int log2N = 0;
    while ((1 << log2N) < N) log2N++;

    // Precompute bit-reversal table
    for (int i = 0; i < N; i++) {
        int rev = 0;
        for (int j = 0; j < log2N; j++) {
            if ((i >> j) & 1) rev |= (1 << (log2N - 1 - j));
        }
        plan->bitrev[i] = rev;
    }

    // Precompute twiddle factors
    for (int i = 0; i < N / 2; i++) {
        plan->twiddle_re[i] = cosf(-2.0f * M_PI * i / N);
        plan->twiddle_im[i] = sinf(-2.0f * M_PI * i / N);
    }
}

static void sit_fft_free(sit_fft_plan *plan) {
    free(plan->bitrev);
    free(plan->twiddle_re);
    free(plan->twiddle_im);
}

static void sit_fft_execute(sit_fft_plan *plan, sit_complex *data, int is_inverse) {
    int N = plan->N;

    // Bit-reversal permutation
    for (int i = 0; i < N; i++) {
        int rev = plan->bitrev[i];
        if (i < rev) {
            float temp_re = data[i][0];
            float temp_im = data[i][1];
            data[i][0] = data[rev][0];
            data[i][1] = data[rev][1];
            data[rev][0] = temp_re;
            data[rev][1] = temp_im;
        }
    }

    // Cooley-Tukey decimation-in-time Radix-2 FFT
    for (int len = 2; len <= N; len <<= 1) {
        int half_len = len >> 1;
        int step = N / len;
        for (int i = 0; i < N; i += len) {
            for (int j = 0; j < half_len; j++) {
                int twiddle_idx = j * step;
                float w_re = plan->twiddle_re[twiddle_idx];
                float w_im = plan->twiddle_im[twiddle_idx];
                
                if (is_inverse) w_im = -w_im; // Conjugate twiddle for IFFT

                int u_idx = i + j;
                int v_idx = i + j + half_len;

                float u_re = data[u_idx][0];
                float u_im = data[u_idx][1];
                float v_re = data[v_idx][0];
                float v_im = data[v_idx][1];

                float t_re = v_re * w_re - v_im * w_im;
                float t_im = v_re * w_im + v_im * w_re;

                data[u_idx][0] = u_re + t_re;
                data[u_idx][1] = u_im + t_im;
                data[v_idx][0] = u_re - t_re;
                data[v_idx][1] = u_im - t_im;
            }
        }
    }
}

// -------------------------------------------------------------------------------------------------
// Maximizer Implementation
// -------------------------------------------------------------------------------------------------

static void init_maximizer(MaximizerState *state, int sample_rate, int H, int N_dominant, float cutoff_frequency, int num_bands) {
    state->sample_rate = sample_rate;
    
    // Ensure Hop size is a power of 2 for the Radix-2 FFT
    int h_pow2 = 16;
    while (h_pow2 < H && h_pow2 < 512) h_pow2 *= 2;
    state->H = h_pow2;
    
    state->W = 2 * state->H;   // 50% overlap
    state->N = 4 * state->W;   // 4x oversampling via FFT size
    state->N_dominant = N_dominant;
    state->cutoff_frequency = cutoff_frequency;
    state->hpf_cutoff = 30.0f;     // Default HPF cutoff: 30 Hz
    state->lpf_cutoff = 16000.0f;  // Default LPF cutoff: 16 kHz
    state->inv_N = 1.0f / state->N;
    state->fft_bin_scale = (float)state->N / state->sample_rate;
    state->freq_scale = (float)state->sample_rate / state->N;
    state->half_W = state->W / 2;
    state->num_bands = num_bands;

    // Allocate memory for dynamic bands
    state->bands = (BandParams *)malloc(num_bands * sizeof(BandParams));
    if (!state->bands) return;

    // Initialize bands to zero
    memset(state->bands, 0, num_bands * sizeof(BandParams));

    // Allocate buffers with 16-byte alignment
#ifdef _WIN32
    state->input_history = (float *)_aligned_malloc(align_size_16(state->W * sizeof(float)), 16);
    state->output_accumulator = (float *)_aligned_malloc(align_size_16(state->W * sizeof(float)), 16);
    state->windowed_signal = (float *)_aligned_malloc(align_size_16(state->N * sizeof(float)), 16);
    state->window = (float *)_aligned_malloc(align_size_16(state->W * sizeof(float)), 16);
    state->fft_buffer = (sit_complex *)_aligned_malloc(align_size_16(state->N * sizeof(sit_complex)), 16);

    size_t half_complex_size = (state->N / 2 + 1) * sizeof(float);
    state->max_enhancement = (float *)_aligned_malloc(align_size_16(half_complex_size), 16);
    state->total_multiplier = (float *)_aligned_malloc(align_size_16(half_complex_size), 16);
    state->magnitude = (float *)_aligned_malloc(align_size_16(half_complex_size), 16);
#else
    state->input_history = (float *)aligned_alloc(16, align_size_16(state->W * sizeof(float)));
    state->output_accumulator = (float *)aligned_alloc(16, align_size_16(state->W * sizeof(float)));
    state->windowed_signal = (float *)aligned_alloc(16, align_size_16(state->N * sizeof(float)));
    state->window = (float *)aligned_alloc(16, align_size_16(state->W * sizeof(float)));
    state->fft_buffer = (sit_complex *)aligned_alloc(16, align_size_16(state->N * sizeof(sit_complex)));

    size_t half_complex_size = (state->N / 2 + 1) * sizeof(float);
    state->max_enhancement = (float *)aligned_alloc(16, align_size_16(half_complex_size));
    state->total_multiplier = (float *)aligned_alloc(16, align_size_16(half_complex_size));
    state->magnitude = (float *)aligned_alloc(16, align_size_16(half_complex_size));
#endif

    // Initialize the internal FFT plan
    sit_fft_init(&state->fft_plan, state->N);

    // Compute Hann window coefficients
    float omega = 2.0f * M_PI / state->W;
    for (int j = 0; j < state->W; j++) {
        state->window[j] = 0.5f * (1.0f - cosf(omega * j));
    }

    memset(state->input_history, 0, state->W * sizeof(float));
    memset(state->output_accumulator, 0, state->W * sizeof(float));
}

static void set_band_params(MaximizerState *state, int band_index, float center_freq, float Q, float enhancement_factor, int D) {
    if (band_index < 0 || band_index >= state->num_bands) return;
    state->bands[band_index].center_freq = center_freq;
    state->bands[band_index].Q = Q;
    state->bands[band_index].enhancement_factor = enhancement_factor;
    state->bands[band_index].D = D;
}

static void set_hpf_cutoff(MaximizerState *state, float cutoff) {
    state->hpf_cutoff = cutoff;
}

static void set_lpf_cutoff(MaximizerState *state, float cutoff) {
    state->lpf_cutoff = cutoff;
}

static void free_maximizer(MaximizerState *state) {
    free(state->bands);
    
#ifdef _WIN32
    _aligned_free(state->input_history);
    _aligned_free(state->output_accumulator);
    _aligned_free(state->windowed_signal);
    _aligned_free(state->window);
    _aligned_free(state->fft_buffer);
    _aligned_free(state->max_enhancement);
    _aligned_free(state->total_multiplier);
    _aligned_free(state->magnitude);
#else
    free(state->input_history);
    free(state->output_accumulator);
    free(state->windowed_signal);
    free(state->window);
    free(state->fft_buffer);
    free(state->max_enhancement);
    free(state->total_multiplier);
    free(state->magnitude);
#endif
    
    sit_fft_free(&state->fft_plan);
}

static void maximizer_processor(MaximizerState *state, float *in_buffer, float *out_buffer, unsigned int frames) {
    if (frames != (unsigned int)state->H) {
        return; // Ensure buffer matches hop size
    }

    // Shift input history and append new samples
    memmove(state->input_history, state->input_history + state->H, (state->W - state->H) * sizeof(float));
    memcpy(state->input_history + (state->W - state->H), in_buffer, state->H * sizeof(float));

    // Apply Hann window and copy to windowed_signal[0 to W-1]
    int j;
    for (j = 0; j < state->W - 3; j += 4) {
        __m128 hist = _mm_load_ps(&state->input_history[j]);
        __m128 win = _mm_load_ps(&state->window[j]);
        __m128 result = _mm_mul_ps(hist, win);
        _mm_store_ps(&state->windowed_signal[j], result);
    }
    for (; j < state->W; j++) {
        state->windowed_signal[j] = state->input_history[j] * state->window[j];
    }

    // Zero-pad windowed_signal from W to N-1
    for (j = state->W; j < state->N - 3; j += 4) {
        _mm_store_ps(&state->windowed_signal[j], _mm_setzero_ps());
    }
    for (; j < state->N; j++) {
        state->windowed_signal[j] = 0.0f;
    }

    // Prepare real signal for Forward FFT
    for (int i = 0; i < state->N; i++) {
        state->fft_buffer[i][0] = state->windowed_signal[i];
        state->fft_buffer[i][1] = 0.0f;
    }

    // Forward FFT
    sit_fft_execute(&state->fft_plan, state->fft_buffer, 0);

    // Compute magnitudes with SIMD (process up to N/2)
    int k;
    for (k = 0; k <= state->N / 2 - 1; k += 2) {  // Process two complex bins (four floats) at a time
        __m128 fft_parts = _mm_load_ps(&state->fft_buffer[k][0]);  // real0, imag0, real1, imag1
        __m128 reals = _mm_shuffle_ps(fft_parts, fft_parts, _MM_SHUFFLE(2, 0, 2, 0));  // real0, real1, real0, real1
        __m128 imags = _mm_shuffle_ps(fft_parts, fft_parts, _MM_SHUFFLE(3, 1, 3, 1));  // imag0, imag1, imag0, imag1
        __m128 real_sq = _mm_mul_ps(reals, reals);
        __m128 imag_sq = _mm_mul_ps(imags, imags);
        __m128 mag_sq = _mm_add_ps(real_sq, imag_sq);
        __m128 mag = _mm_sqrt_ps(mag_sq);  // mag0, mag1, mag0, mag1
        
        state->magnitude[k] = ((float*)&mag)[0];
        state->magnitude[k + 1] = ((float*)&mag)[1];
    }
    // Handle remaining bins (like Nyquist)
    for (; k <= state->N / 2; k++) {
        state->magnitude[k] = sqrtf(state->fft_buffer[k][0] * state->fft_buffer[k][0] +
                                    state->fft_buffer[k][1] * state->fft_buffer[k][1]);
    }

    // Initialize max_enhancement array to 1.0f
    for (k = 0; k <= state->N / 2 - 3; k += 4) {
        _mm_store_ps(&state->max_enhancement[k], _mm_set1_ps(1.0f));
    }
    for (; k <= state->N / 2; k++) {
        state->max_enhancement[k] = 1.0f;
    }

    // Process each band dynamically
    for (int band_idx = 0; band_idx < state->num_bands; band_idx++) {
        BandParams *band = &state->bands[band_idx];
        if (band->enhancement_factor <= 0.0f) continue;

        float BW = band->center_freq / band->Q;
        float low_freq = band->center_freq - BW * 0.5f;
        float high_freq = band->center_freq + BW * 0.5f;
        if (low_freq < 0.0f) low_freq = 0.0f;

        int low_bin = (int)ceilf(low_freq * state->fft_bin_scale);
        int high_bin = (int)floorf(high_freq * state->fft_bin_scale);
        if (low_bin < 0) low_bin = 0;
        if (high_bin > state->N / 2) high_bin = state->N / 2;
        if (low_bin >= high_bin) continue;

        // Initialize top N peaks
        float top_mags[state->N_dominant];
        float top_bins[state->N_dominant];
        for (int p = 0; p < state->N_dominant; p++) {
            top_mags[p] = 0.0f;
            top_bins[p] = -1.0f;
        }

        // Find peaks with parabolic interpolation
        for (int p = low_bin + 1; p < high_bin; p++) {
            float y0 = state->magnitude[p - 1];
            float y1 = state->magnitude[p];
            float y2 = state->magnitude[p + 1];
            if (y1 > y0 && y1 > y2) { // Local maximum
                float denom = 2.0f * (y0 - 2.0f * y1 + y2);
                if (denom < 0.0f && fabsf(denom) > 1e-6f) {
                    float delta = y2 - y0;
                    float interp_p = delta / denom;
                    float a = (y0 + y2 - 2.0f * y1) / 2.0f;
                    float b = delta / 2.0f;
                    float y_max = y1 - (b * b) / (4.0f * a);
                    float estimated_bin = (float)p + interp_p;
                    if (y_max > top_mags[0]) {
                        top_mags[0] = y_max;
                        top_bins[0] = estimated_bin;
                        // Bubble up to maintain sorted order
                        for (int q = 0; q < state->N_dominant - 1; q++) {
                            if (top_mags[q] > top_mags[q + 1]) {
                                float temp_mag = top_mags[q + 1];
                                float temp_bin = top_bins[q + 1];
                                top_mags[q + 1] = top_mags[q];
                                top_bins[q + 1] = top_bins[q];
                                top_mags[q] = temp_mag;
                                top_bins[q] = temp_bin;
                            } else {
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Enhance overtones with cubic interpolation
        for (int p = 0; p < state->N_dominant; p++) {
            float dominant_bin_est = top_bins[p];
            if (dominant_bin_est < 0.0f) continue;
            for (int d = 1; d <= band->D; d++) {
                float bin_overtone = dominant_bin_est * d;
                if (bin_overtone > state->N / 2) continue;
                int b = (int)floorf(bin_overtone);
                float f = bin_overtone - b;
                
                __m128 s_vec = _mm_set_ps(f + 2.0f, f + 1.0f, f + 0.0f, f - 1.0f);
                __m128 abs_s = _mm_and_ps(s_vec, _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)));
                
                __m128 one = _mm_set1_ps(1.0f);
                __m128 two = _mm_set1_ps(2.0f);
                __m128 zero = _mm_setzero_ps();
                
                __m128 lt_one = _mm_cmplt_ps(abs_s, one);
                __m128 lt_two = _mm_cmplt_ps(abs_s, two);
                
                __m128 abs_s_sq = _mm_mul_ps(abs_s, abs_s);
                __m128 abs_s_cu = _mm_mul_ps(abs_s_sq, abs_s);
                __m128 kernel_lt_one = _mm_sub_ps(
                    _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1.5f), abs_s_cu), one),
                    _mm_mul_ps(_mm_set1_ps(2.5f), abs_s_sq)
                );
                
                __m128 kernel_lt_two = _mm_add_ps(
                    _mm_add_ps(_mm_mul_ps(_mm_set1_ps(-0.5f), abs_s_cu), _mm_mul_ps(_mm_set1_ps(2.5f), abs_s_sq)),
                    _mm_add_ps(_mm_mul_ps(_mm_set1_ps(-4.0f), abs_s), two)
                );
                
                __m128 kernel = _mm_blendv_ps(
                    _mm_blendv_ps(zero, kernel_lt_two, lt_two),
                    kernel_lt_one,
                    lt_one
                );
                
                float weights[4];
                _mm_store_ps(weights, kernel);
                for (int offset = -1; offset <= 2; offset++) {
                    int m = b + offset;
                    if (m >= 0 && m <= state->N / 2) {
                        float weight = weights[offset + 1];
                        float enhancement_m = 1.0f + weight * band->enhancement_factor;
                        state->max_enhancement[m] = fmaxf(state->max_enhancement[m], enhancement_m);
                    }
                }
            }
        }
    }

    // Compute total multiplier with SIMD
    __m128 freq_scale_vec = _mm_set1_ps(state->freq_scale);
    __m128 lpf_cutoff_vec = _mm_set1_ps(state->lpf_cutoff);
    __m128 hpf_cutoff_vec = _mm_set1_ps(state->hpf_cutoff);
    __m128 one_vec = _mm_set1_ps(1.0f);

    for (k = 0; k <= state->N / 2 - 3; k += 4) {
        __m128 k_vec = _mm_set_ps((float)(k + 3), (float)(k + 2), (float)(k + 1), (float)k);
        __m128 f_k = _mm_mul_ps(k_vec, freq_scale_vec);
        __m128 ratio_lp = _mm_div_ps(f_k, lpf_cutoff_vec);
        __m128 ratio_lp2 = _mm_mul_ps(ratio_lp, ratio_lp);
        __m128 ratio_lp4 = _mm_mul_ps(ratio_lp2, ratio_lp2);
        __m128 pow8_lp = _mm_mul_ps(ratio_lp4, ratio_lp4);
        __m128 denom_lp = _mm_add_ps(one_vec, pow8_lp);
        __m128 H_LP = _mm_div_ps(one_vec, _mm_sqrt_ps(denom_lp));
        __m128 ratio_hp = _mm_div_ps(hpf_cutoff_vec, f_k);
        __m128 ratio_hp2 = _mm_mul_ps(ratio_hp, ratio_hp);
        __m128 ratio_hp4 = _mm_mul_ps(ratio_hp2, ratio_hp2);
        __m128 pow8_hp = _mm_mul_ps(ratio_hp4, ratio_hp4);
        __m128 denom_hp = _mm_add_ps(one_vec, pow8_hp);
        __m128 H_HP = _mm_div_ps(one_vec, _mm_sqrt_ps(denom_hp));

        if (k == 0) {
             __m128 mask = _mm_castsi128_ps(_mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0));
             H_HP = _mm_and_ps(H_HP, mask);
        }

        __m128 filter_response = _mm_mul_ps(H_HP, H_LP);
        __m128 enhancement = _mm_load_ps(&state->max_enhancement[k]);
        __m128 total = _mm_mul_ps(enhancement, filter_response);
        _mm_store_ps(&state->total_multiplier[k], total);
    }
    for (; k <= state->N / 2; k++) {
        float f_k = (float)k * state->freq_scale;
        float ratio_lp = f_k / state->lpf_cutoff;
        float pow8_lp = ratio_lp * ratio_lp;
        pow8_lp *= pow8_lp;
        pow8_lp *= pow8_lp;
        float H_LP = 1.0f / sqrtf(1.0f + pow8_lp);
        float ratio_hp = state->hpf_cutoff / f_k;
        float pow8_hp = ratio_hp * ratio_hp;
        pow8_hp *= pow8_hp;
        pow8_hp *= pow8_hp;
        float H_HP = (k == 0) ? 0.0f : 1.0f / sqrtf(1.0f + pow8_hp);
        state->total_multiplier[k] = state->max_enhancement[k] * H_HP * H_LP;
    }

    // Apply multiplier to FFT buffer with SIMD
    for (k = 0; k <= state->N / 2 - 1; k += 2) {
        float mul0 = state->total_multiplier[k];
        float mul1 = state->total_multiplier[k + 1];
        __m128 multiplier = _mm_set_ps(mul1, mul1, mul0, mul0);
        __m128 fft_parts = _mm_load_ps(&state->fft_buffer[k][0]);
        __m128 result = _mm_mul_ps(fft_parts, multiplier);
        _mm_store_ps(&state->fft_buffer[k][0], result);
    }
    for (; k <= state->N / 2; k++) {
        state->fft_buffer[k][0] *= state->total_multiplier[k];
        state->fft_buffer[k][1] *= state->total_multiplier[k];
    }

    // Prepare for Inverse FFT: Enforce conjugate symmetry for real output
    state->fft_buffer[0][1] = 0.0f; // DC is strictly real
    state->fft_buffer[state->N / 2][1] = 0.0f; // Nyquist is strictly real
    for (k = 1; k < state->N / 2; k++) {
        state->fft_buffer[state->N - k][0] = state->fft_buffer[k][0];
        state->fft_buffer[state->N - k][1] = -state->fft_buffer[k][1];
    }

    // Inverse FFT
    sit_fft_execute(&state->fft_plan, state->fft_buffer, 1);

    // Extract real part from FFT buffer (unnormalized)
    for (int i = 0; i < state->N; i++) {
        state->windowed_signal[i] = state->fft_buffer[i][0];
    }

    // Normalize output with SIMD (since custom IFFT is unnormalized like FFTW)
    __m128 inv_N_vec = _mm_set1_ps(state->inv_N);
    for (j = 0; j < state->N - 3; j += 4) {
        __m128 signal = _mm_load_ps(&state->windowed_signal[j]);
        __m128 result = _mm_mul_ps(signal, inv_N_vec);
        _mm_store_ps(&state->windowed_signal[j], result);
    }
    for (; j < state->N; j++) {
        state->windowed_signal[j] *= state->inv_N;
    }

    // Overlap-add with SIMD
    for (j = 0; j < state->W - 3; j += 4) {
        __m128 accum = _mm_load_ps(&state->output_accumulator[j]);
        __m128 signal = _mm_load_ps(&state->windowed_signal[j]);
        __m128 result = _mm_add_ps(accum, signal);
        _mm_store_ps(&state->output_accumulator[j], result);
    }
    for (; j < state->W; j++) {
        state->output_accumulator[j] += state->windowed_signal[j];
    }

    // Copy to output and shift accumulator
    memcpy(out_buffer, state->output_accumulator, state->H * sizeof(float));
    memmove(state->output_accumulator, state->output_accumulator + state->H, (state->W - state->H) * sizeof(float));
    memset(state->output_accumulator + (state->W - state->H), 0, state->H * sizeof(float));
}

static int maximizer_get_latency_samples(MaximizerState *state) {
    return state->H; // Latency equals the hop size due to STFT processing
}

#endif // SIT_AUX_MAXIMIZER_H