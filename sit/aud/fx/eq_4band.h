/***************************************************************************************************
*
*   sit/aud/eq_4band.h - 4-Band Parametric EQ
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Simple 4-band parametric equalizer using biquad peaking filters.
*   Each band can boost or cut a specific frequency range.
*
***************************************************************************************************/

#ifndef SITUATION_EQ_4BAND_H
#define SITUATION_EQ_4BAND_H

#include <math.h>
#include <string.h>

// FMA detection
#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
    #define EQ4_HAS_FMA 1
    #if defined(__GNUC__) || defined(__clang__)
        #define EQ4_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
    #else
        #define EQ4_FMA(a, b, c) fmaf((a), (b), (c))
    #endif
#else
    #define EQ4_HAS_FMA 0
    #define EQ4_FMA(a, b, c) ((a) * (b) + (c))
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Single peaking filter band
typedef struct {
    float sample_rate;
    float frequency;
    float Q;
    float gain_db;
    
    // Biquad coefficients
    float b0, b1, b2;
    float a1, a2;
    
    // State variables (per channel)
    float x1_l, x2_l, y1_l, y2_l;
    float x1_r, x2_r, y1_r, y2_r;
} EQBand;

// 4-Band EQ state
typedef struct {
    EQBand band[4];
    float sample_rate;
} SituationEQ4Band;

// Update coefficients for a peaking filter
static void eq_band_update_coefficients(EQBand* band) {
    float w0 = 2.0f * M_PI * band->frequency / band->sample_rate;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * band->Q);
    float A = powf(10.0f, band->gain_db / 40.0f);
    
    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cos_w0;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cos_w0;
    float a2 = 1.0f - alpha / A;
    
    // Normalize
    band->b0 = b0 / a0;
    band->b1 = b1 / a0;
    band->b2 = b2 / a0;
    band->a1 = a1 / a0;
    band->a2 = a2 / a0;
}

// Initialize a single band
static void eq_band_init(EQBand* band, float sample_rate, float freq, float q, float gain_db) {
    memset(band, 0, sizeof(EQBand));
    band->sample_rate = sample_rate;
    band->frequency = freq;
    band->Q = q;
    band->gain_db = gain_db;
    eq_band_update_coefficients(band);
}

// Process a single band (FMA-optimized)
static void eq_band_process(EQBand* band, float* buffer, int frames, int channels) {
    if (channels == 1) {
        // Mono
        for (int i = 0; i < frames; i++) {
            float x = buffer[i];
            // FMA-optimized biquad
            float y = EQ4_FMA(band->b0, x, EQ4_FMA(band->b1, band->x1_l, 
                      EQ4_FMA(band->b2, band->x2_l, -EQ4_FMA(band->a1, band->y1_l, band->a2 * band->y2_l))));
            
            band->x2_l = band->x1_l;
            band->x1_l = x;
            band->y2_l = band->y1_l;
            band->y1_l = y;
            
            buffer[i] = y;
        }
    } else {
        // Stereo
        for (int i = 0; i < frames; i++) {
            float x_l = buffer[i * 2];
            float x_r = buffer[i * 2 + 1];
            
            // FMA-optimized biquad (left)
            float y_l = EQ4_FMA(band->b0, x_l, EQ4_FMA(band->b1, band->x1_l, 
                        EQ4_FMA(band->b2, band->x2_l, -EQ4_FMA(band->a1, band->y1_l, band->a2 * band->y2_l))));
            // FMA-optimized biquad (right)
            float y_r = EQ4_FMA(band->b0, x_r, EQ4_FMA(band->b1, band->x1_r, 
                        EQ4_FMA(band->b2, band->x2_r, -EQ4_FMA(band->a1, band->y1_r, band->a2 * band->y2_r))));
            
            band->x2_l = band->x1_l;
            band->x1_l = x_l;
            band->y2_l = band->y1_l;
            band->y1_l = y_l;
            
            band->x2_r = band->x1_r;
            band->x1_r = x_r;
            band->y2_r = band->y1_r;
            band->y1_r = y_r;
            
            buffer[i * 2] = y_l;
            buffer[i * 2 + 1] = y_r;
        }
    }
}

// Initialize EQ with 4 bands
static void eq4band_init(SituationEQ4Band* eq, float sample_rate) {
    eq->sample_rate = sample_rate;
    
    // Initialize 4 bands with default frequencies and neutral gain
    eq_band_init(&eq->band[0], sample_rate, 100.0f, 1.0f, 0.0f);    // Low
    eq_band_init(&eq->band[1], sample_rate, 500.0f, 1.0f, 0.0f);    // Low-mid
    eq_band_init(&eq->band[2], sample_rate, 2000.0f, 1.0f, 0.0f);   // High-mid
    eq_band_init(&eq->band[3], sample_rate, 8000.0f, 1.0f, 0.0f);   // High
}

// Set band parameters
static void eq4band_set_band(SituationEQ4Band* eq, int band_index, float freq, float q, float gain_db) {
    if (band_index < 0 || band_index >= 4) return;
    
    eq->band[band_index].frequency = freq;
    eq->band[band_index].Q = q;
    eq->band[band_index].gain_db = gain_db;
    eq_band_update_coefficients(&eq->band[band_index]);
}

// Process audio through all 4 bands
static void eq4band_process(SituationEQ4Band* eq, const float* input, float* output, int frames, int channels) {
    // Copy input to output
    int samples = frames * channels;
    for (int i = 0; i < samples; i++) {
        output[i] = input[i];
    }
    
    // Process through each band in series
    for (int band = 0; band < 4; band++) {
        eq_band_process(&eq->band[band], output, frames, channels);
    }
}

#endif // SITUATION_EQ_4BAND_H
