/***************************************************************************************************
*
*   silver_bullet_mk2.h - Silver Bullet MK2 Processor
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   High-end emulation of the Silver Bullet MK2 hardware processor.
*   Features 'MOJO' Amp simulation (American/British/Colour), 3-band EQ with Air,
*   and vintage saturation characteristics.
*
*   Optimized with SIMD (SSE) for performance.
*
*   This file is intended to be included within the audio subsystem implementation.
*
***************************************************************************************************/

#ifndef SILVERBULLETMK2_H
#define SILVERBULLETMK2_H

#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h> // For SSE

#ifndef PI
#define PI 3.14159265358979323846
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#define SB_LUT_SIZE 4096
#define SB_LUT_RANGE 10.0f // Range for saturation LUT

// Enum for MOJO amp selection (A: American, N: British, C: Colour)
typedef enum {
    MOJO_A,
    MOJO_N,
    MOJO_C
} MojoAmp;

// Biquad filter coefficients (aligned for SIMD)
typedef struct {
    __m128 a0, a1, a2, b1, b2;
} BiquadCoeffSIMD __attribute__((aligned(16)));

// Biquad filter state (per channel, aligned for SIMD)
typedef struct {
    __m128 x1, x2, y1, y2;
} BiquadStateSIMD __attribute__((aligned(16)));

// Main structure for the Silver Bullet MK2 processor
typedef struct {
    MojoAmp mojoAmp;        // MOJO amp selection (A, N, C)
    float mojoDrive;        // MOJO drive level (0.0 to 1.0)
    float lowFreq;          // Low shelf frequency (20Hz to 200Hz)
    float lowGain;          // Low shelf gain (-1.0 to 1.0 maps to -12dB to +12dB)
    float midFreq;          // Mid peak frequency (200Hz to 5kHz)
    float midGain;          // Mid peak gain (-1.0 to 1.0)
    float midQ;             // Mid peak Q factor (0.5 to 5.0)
    float highFreq;         // High shelf frequency (5kHz to 20kHz)
    float highGain;         // High shelf gain (-1.0 to 1.0)
    float airFreq;          // AIR high-shelf frequency (10kHz to 20kHz)
    float airGain;          // AIR high-shelf gain (0.0 to 1.0 maps to 0 to +12dB)
    float tightCutoff;      // TIGHT high-pass cutoff (20Hz to 200Hz)
    float aspectRatio;      // Stereo width control (0.0 to 2.0)
    bool vintage;           // VINTAGE mode switch
    bool circuitBending;    // Circuit bending switch
    float sampleRate;       // Sample rate for filter calculations
    BiquadCoeffSIMD eqCoeffs[5]; // Coefficients for TIGHT, LOW, MID, HIGH, AIR
    BiquadStateSIMD leftStates[5];  // Filter states for left channel
    BiquadStateSIMD rightStates[5]; // Filter states for right channel
    float* saturationLUT;   // Lookup table for saturation
    int lutSize;            // Size of the lookup table
    float cb_phase;         // Circuit bending LFO phase
} SilverBulletMK2 __attribute__((aligned(16)));

// Helper function: Soft clipping for MOJO C and non-linearities
static inline float sb_softClip(float x) {
    return x / (1.0f + fabsf(x));
}

// Helper function: Apply biquad filter using SIMD
static void sb_applyBiquadSIMD(const BiquadCoeffSIMD* coeff, BiquadStateSIMD* state, __m128* x) {
    __m128 y = _mm_add_ps(_mm_mul_ps(coeff->a0, *x),
                          _mm_add_ps(_mm_mul_ps(coeff->a1, state->x1),
                                     _mm_add_ps(_mm_mul_ps(coeff->a2, state->x2),
                                                _mm_sub_ps(_mm_mul_ps(coeff->b1, state->y1),
                                                           _mm_mul_ps(coeff->b2, state->y2)))));
    state->x2 = state->x1;
    state->x1 = *x;
    state->y2 = state->y1;
    state->y1 = y;
    *x = y;
}

// Compute coefficients for a low-shelf filter (SIMD-friendly)
static void sb_computeLowShelfCoeffSIMD(BiquadCoeffSIMD* coeff, float freq, float gainDB, float sampleRate) {
    float A = powf(10.0f, gainDB / 40.0f);
    float w0 = 2.0f * PI * freq / sampleRate;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / 2.0f;
    float sqrtA = sqrtf(A);

    float b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
    float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
    float b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
    float a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
    float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
    float a2 = (A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;

    coeff->a0 = _mm_set1_ps(b0 / a0);
    coeff->a1 = _mm_set1_ps(b1 / a0);
    coeff->a2 = _mm_set1_ps(b2 / a0);
    coeff->b1 = _mm_set1_ps(a1 / a0);
    coeff->b2 = _mm_set1_ps(a2 / a0);
}

// Compute coefficients for a peaking filter (SIMD-friendly)
static void sb_computePeakCoeffSIMD(BiquadCoeffSIMD* coeff, float freq, float gainDB, float Q, float sampleRate) {
    float A = powf(10.0f, gainDB / 40.0f);
    float w0 = 2.0f * PI * freq / sampleRate;
    float alpha = sinf(w0) / (2.0f * Q);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cosf(w0);
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cosf(w0);
    float a2 = 1.0f - alpha / A;

    coeff->a0 = _mm_set1_ps(b0 / a0);
    coeff->a1 = _mm_set1_ps(b1 / a0);
    coeff->a2 = _mm_set1_ps(b2 / a0);
    coeff->b1 = _mm_set1_ps(a1 / a0);
    coeff->b2 = _mm_set1_ps(a2 / a0);
}

// Compute coefficients for a high-shelf filter (SIMD-friendly)
static void sb_computeHighShelfCoeffSIMD(BiquadCoeffSIMD* coeff, float freq, float gainDB, float sampleRate) {
    float A = powf(10.0f, gainDB / 40.0f);
    float w0 = 2.0f * PI * freq / sampleRate;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / 2.0f;
    float sqrtA = sqrtf(A);

    float b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
    float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
    float b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
    float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
    float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
    float a2 = (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;

    coeff->a0 = _mm_set1_ps(b0 / a0);
    coeff->a1 = _mm_set1_ps(b1 / a0);
    coeff->a2 = _mm_set1_ps(b2 / a0);
    coeff->b1 = _mm_set1_ps(a1 / a0);
    coeff->b2 = _mm_set1_ps(a2 / a0);
}

// Compute coefficients for a high-pass filter (SIMD-friendly)
static void sb_computeHighPassCoeffSIMD(BiquadCoeffSIMD* coeff, float freq, float sampleRate) {
    float w0 = 2.0f * PI * freq / sampleRate;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / 2.0f;

    float b0 = (1.0f + cosw0) / 2.0f;
    float b1 = -(1.0f + cosw0);
    float b2 = (1.0f + cosw0) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosf(w0);
    float a2 = 1.0f - alpha;

    coeff->a0 = _mm_set1_ps(b0 / a0);
    coeff->a1 = _mm_set1_ps(b1 / a0);
    coeff->a2 = _mm_set1_ps(b2 / a0);
    coeff->b1 = _mm_set1_ps(a1 / a0);
    coeff->b2 = _mm_set1_ps(a2 / a0);
}

static void SilverBulletMK2_deinit(SilverBulletMK2* sb) {
    if (sb->saturationLUT) {
        _mm_free(sb->saturationLUT);
        sb->saturationLUT = NULL; // Prevent double-free
    }
    // Note: Caller must free sb if it was dynamically allocated
}

// Forward declarations for setters to be used in init
static void SilverBulletMK2_setLowFreq(SilverBulletMK2* sb, float freq);
static void SilverBulletMK2_setLowGain(SilverBulletMK2* sb, float gain);
static void SilverBulletMK2_setMidFreq(SilverBulletMK2* sb, float freq);
static void SilverBulletMK2_setMidGain(SilverBulletMK2* sb, float gain);
static void SilverBulletMK2_setMidQ(SilverBulletMK2* sb, float Q);
static void SilverBulletMK2_setHighFreq(SilverBulletMK2* sb, float freq);
static void SilverBulletMK2_setHighGain(SilverBulletMK2* sb, float gain);
static void SilverBulletMK2_setAirFreq(SilverBulletMK2* sb, float freq);
static void SilverBulletMK2_setAirGain(SilverBulletMK2* sb, float gain);
static void SilverBulletMK2_setTightCutoff(SilverBulletMK2* sb, float cutoff);

// Initialize the processor
static void SilverBulletMK2_init(SilverBulletMK2* sb, float sampleRate) {
    sb->mojoAmp = MOJO_A;
    sb->mojoDrive = 0.5f;
    sb->lowFreq = 100.0f;
    sb->lowGain = 0.0f;
    sb->midFreq = 1000.0f;
    sb->midGain = 0.0f;
    sb->midQ = 1.0f;
    sb->highFreq = 10000.0f;
    sb->highGain = 0.0f;
    sb->airFreq = 15000.0f;
    sb->airGain = 0.0f;
    sb->tightCutoff = 20.0f;
    sb->aspectRatio = 1.0f;
    sb->vintage = false;
    sb->circuitBending = false;
    sb->sampleRate = sampleRate;

    // Initialize filter states to zero
    for (int i = 0; i < 5; i++) {
        sb->leftStates[i].x1 = _mm_setzero_ps();
        sb->leftStates[i].x2 = _mm_setzero_ps();
        sb->leftStates[i].y1 = _mm_setzero_ps();
        sb->leftStates[i].y2 = _mm_setzero_ps();
        sb->rightStates[i].x1 = _mm_setzero_ps();
        sb->rightStates[i].x2 = _mm_setzero_ps();
        sb->rightStates[i].y1 = _mm_setzero_ps();
        sb->rightStates[i].y2 = _mm_setzero_ps();
    }

    // Precompute saturation lookup table
    sb->lutSize = SB_LUT_SIZE;
    sb->saturationLUT = (float*)_mm_malloc(sizeof(float) * SB_LUT_SIZE, 16);
    for (int i = 0; i < SB_LUT_SIZE; i++) {
        float x = (float)i / (SB_LUT_SIZE - 1) * 2.0f * SB_LUT_RANGE - SB_LUT_RANGE;
        sb->saturationLUT[i] = tanhf(x);
    }

    sb->cb_phase = 0.0f;

    // Set initial coefficients
    SilverBulletMK2_setLowFreq(sb, 100.0f);
    SilverBulletMK2_setLowGain(sb, 0.0f);
    SilverBulletMK2_setMidFreq(sb, 1000.0f);
    SilverBulletMK2_setMidGain(sb, 0.0f);
    SilverBulletMK2_setMidQ(sb, 1.0f);
    SilverBulletMK2_setHighFreq(sb, 10000.0f);
    SilverBulletMK2_setHighGain(sb, 0.0f);
    SilverBulletMK2_setAirFreq(sb, 15000.0f);
    SilverBulletMK2_setAirGain(sb, 0.0f);
    SilverBulletMK2_setTightCutoff(sb, 20.0f);
}

// Setter functions with improved EQ flexibility
static void SilverBulletMK2_setLowFreq(SilverBulletMK2* sb, float freq) {
    sb->lowFreq = MAX(20.0f, MIN(200.0f, freq));
    float gainDB = sb->lowGain * 12.0f;
    sb_computeLowShelfCoeffSIMD(&sb->eqCoeffs[1], sb->lowFreq, gainDB, sb->sampleRate);
}

static void SilverBulletMK2_setLowGain(SilverBulletMK2* sb, float gain) {
    sb->lowGain = MAX(-1.0f, MIN(1.0f, gain));
    float gainDB = sb->lowGain * 12.0f;
    sb_computeLowShelfCoeffSIMD(&sb->eqCoeffs[1], sb->lowFreq, gainDB, sb->sampleRate);
}

static void SilverBulletMK2_setMidFreq(SilverBulletMK2* sb, float freq) {
    sb->midFreq = MAX(200.0f, MIN(5000.0f, freq));
    float gainDB = sb->midGain * 12.0f;
    sb_computePeakCoeffSIMD(&sb->eqCoeffs[2], sb->midFreq, gainDB, sb->midQ, sb->sampleRate);
}

static void SilverBulletMK2_setMidGain(SilverBulletMK2* sb, float gain) {
    sb->midGain = MAX(-1.0f, MIN(1.0f, gain));
    float gainDB = sb->midGain * 12.0f;
    sb_computePeakCoeffSIMD(&sb->eqCoeffs[2], sb->midFreq, gainDB, sb->midQ, sb->sampleRate);
}

static void SilverBulletMK2_setMidQ(SilverBulletMK2* sb, float Q) {
    sb->midQ = MAX(0.5f, MIN(5.0f, Q));
    float gainDB = sb->midGain * 12.0f;
    sb_computePeakCoeffSIMD(&sb->eqCoeffs[2], sb->midFreq, gainDB, sb->midQ, sb->sampleRate);
}

static void SilverBulletMK2_setHighFreq(SilverBulletMK2* sb, float freq) {
    sb->highFreq = MAX(5000.0f, MIN(20000.0f, freq));
    float gainDB = sb->highGain * 12.0f;
    sb_computeHighShelfCoeffSIMD(&sb->eqCoeffs[3], sb->highFreq, gainDB, sb->sampleRate);
}

static void SilverBulletMK2_setHighGain(SilverBulletMK2* sb, float gain) {
    sb->highGain = MAX(-1.0f, MIN(1.0f, gain));
    float gainDB = sb->highGain * 12.0f;
    sb_computeHighShelfCoeffSIMD(&sb->eqCoeffs[3], sb->highFreq, gainDB, sb->sampleRate);
}

static void SilverBulletMK2_setAirFreq(SilverBulletMK2* sb, float freq) {
    sb->airFreq = MAX(10000.0f, MIN(20000.0f, freq));
    float gainDB = sb->airGain * 12.0f;
    sb_computeHighShelfCoeffSIMD(&sb->eqCoeffs[4], sb->airFreq, gainDB, sb->sampleRate);
}

static void SilverBulletMK2_setAirGain(SilverBulletMK2* sb, float gain) {
    sb->airGain = MAX(0.0f, MIN(1.0f, gain));
    float gainDB = sb->airGain * 12.0f;
    sb_computeHighShelfCoeffSIMD(&sb->eqCoeffs[4], sb->airFreq, gainDB, sb->sampleRate);
}

static void SilverBulletMK2_setTightCutoff(SilverBulletMK2* sb, float cutoff) {
    sb->tightCutoff = MAX(20.0f, MIN(200.0f, cutoff));
    sb_computeHighPassCoeffSIMD(&sb->eqCoeffs[0], sb->tightCutoff, sb->sampleRate);
}

static void SilverBulletMK2_setMojoAmp(SilverBulletMK2* sb, MojoAmp amp) {
    sb->mojoAmp = amp;
}

static void SilverBulletMK2_setMojoDrive(SilverBulletMK2* sb, float drive) {
    sb->mojoDrive = MAX(0.0f, MIN(1.0f, drive));
}

static void SilverBulletMK2_setAspectRatio(SilverBulletMK2* sb, float aspect) {
    sb->aspectRatio = MAX(0.0f, MIN(2.0f, aspect));
}

static void SilverBulletMK2_setVintage(SilverBulletMK2* sb, bool vintage) {
    sb->vintage = vintage;
}

static void SilverBulletMK2_setCircuitBending(SilverBulletMK2* sb, bool bending) {
    sb->circuitBending = bending;
}

// Helper function for vectorized hard clipping
static inline __m128 sb_hardClipSIMD(__m128 x) {
    __m128 minVal = _mm_set1_ps(-1.0f);  // Set all four floats to -1.0
    __m128 maxVal = _mm_set1_ps(1.0f);   // Set all four floats to 1.0
    return _mm_max_ps(_mm_min_ps(x, maxVal), minVal); // Clamp between -1.0 and 1.0
}

// Main function to process MOJO_N mode with SIMD
static void sb_processMojoNSIMD(const float* inputBuffer, float* outputBuffer, int numFrames, float mojoDrive) {
    __m128 drive = _mm_set1_ps(mojoDrive * 10.0f); // Apply drive to all four floats
    int i;

    // Process two stereo samples (four floats) at a time
    for (i = 0; i < numFrames - 1; i += 2) {
        // Load 4 floats: [left[i], right[i], left[i+1], right[i+1]]
        __m128 stereo = _mm_loadu_ps(&inputBuffer[2 * i]);
        __m128 x = _mm_mul_ps(stereo, drive);       // Apply drive
        __m128 clipped = sb_hardClipSIMD(x);           // Hard clip
        _mm_storeu_ps(&outputBuffer[2 * i], clipped); // Store result
    }

    // Handle remaining single stereo sample if numFrames is odd
    for (; i < numFrames; i++) {
        float x_left = inputBuffer[2 * i] * mojoDrive * 10.0f;
        float x_right = inputBuffer[2 * i + 1] * mojoDrive * 10.0f;
        outputBuffer[2 * i] = fminf(fmaxf(x_left, -1.0f), 1.0f);     // Clamp left
        outputBuffer[2 * i + 1] = fminf(fmaxf(x_right, -1.0f), 1.0f); // Clamp right
    }
}

// Process stereo audio stream with SIMD and LUT
static void SilverBulletMK2_processAudio(SilverBulletMK2* sb, const float* inputBuffer, float* outputBuffer, int numFrames) {
    // Temporary buffer for MOJO saturation output (stereo: 2 * numFrames)
    // WARNING: VLA on stack for large numFrames is dangerous.
    // Better to use a fixed scratch buffer or process in chunks.
    // For now, assuming numFrames is small (e.g. 512, 1024).
    float mojoBuffer[2 * numFrames];

    // Step 1: Apply MOJO saturation based on mode
    if (sb->mojoAmp == MOJO_N) {
        // Vectorized hard clipping for MOJO_N using SIMD
        sb_processMojoNSIMD(inputBuffer, mojoBuffer, numFrames, sb->mojoDrive);
    } else if (sb->mojoAmp == MOJO_C) {
        // Scalar soft clipping for MOJO_C
        for (int i = 0; i < numFrames; i++) {
            float drive = sb->mojoDrive * 10.0f;
            float x_left = inputBuffer[2 * i] * drive;
            float x_right = inputBuffer[2 * i + 1] * drive;
            mojoBuffer[2 * i] = sb_softClip(x_left);
            mojoBuffer[2 * i + 1] = sb_softClip(x_right);
        }
    } else { // MOJO_A with LUT
        for (int i = 0; i < numFrames; i++) {
            float drive = sb->mojoDrive * 10.0f;
            float x_left = inputBuffer[2 * i] * drive;
            float x_right = inputBuffer[2 * i + 1] * drive;
            // LUT index calculation
            float idx_left = (x_left + SB_LUT_RANGE) / (2.0f * SB_LUT_RANGE) * (sb->lutSize - 1);
            float idx_right = (x_right + SB_LUT_RANGE) / (2.0f * SB_LUT_RANGE) * (sb->lutSize - 1);
            int idx_l = (int)idx_left;
            int idx_r = (int)idx_right;
            // Clamp indices and apply LUT
            mojoBuffer[2 * i] = sb->saturationLUT[MAX(0, MIN(sb->lutSize - 1, idx_l))];
            mojoBuffer[2 * i + 1] = sb->saturationLUT[MAX(0, MIN(sb->lutSize - 1, idx_r))];
        }
    }

    // Step 2: Process the rest of the audio chain sample-by-sample
    for (int i = 0; i < numFrames; i++) {
        __m128 left = _mm_set1_ps(mojoBuffer[2 * i]);
        __m128 right = _mm_set1_ps(mojoBuffer[2 * i + 1]);

        // EQ Section with per-filter non-linearities
        for (int f = 0; f < 5; f++) {
            sb_applyBiquadSIMD(&sb->eqCoeffs[f], &sb->leftStates[f], &left);
            left = _mm_set1_ps(sb_softClip(left[0] * 1.1f) * 0.9f);  // Non-linearity after each filter
            sb_applyBiquadSIMD(&sb->eqCoeffs[f], &sb->rightStates[f], &right);
            right = _mm_set1_ps(sb_softClip(right[0] * 1.1f) * 0.9f); // Non-linearity after each filter
        }

        // VINTAGE Effect (applied after EQ)
        if (sb->vintage) {
            left = _mm_set1_ps(tanhf(left[0] * 2.0f) * 0.8f);  // Gentle saturation for vintage warmth
            right = _mm_set1_ps(tanhf(right[0] * 2.0f) * 0.8f);
        }

        // Circuit Bending (applied after VINTAGE)
        if (sb->circuitBending) {
            float mod = sinf(sb->cb_phase) * 0.1f;
            left = _mm_set1_ps(left[0] + mod);
            right = _mm_set1_ps(right[0] - mod);
            sb->cb_phase += 0.1f;
            if (sb->cb_phase >= 2.0f * 3.14159f) sb->cb_phase -= 2.0f * 3.14159f;
        }

        // ASPECT RATIO (Stereo Imaging)
        float mid = (left[0] + right[0]) / 2.0f;
        float side = (left[0] - right[0]) / 2.0f;
        float width = sb->aspectRatio;
        side *= width;
        left = _mm_set1_ps(mid + side);
        right = _mm_set1_ps(mid - side);

        // Output with clipping prevention
        outputBuffer[2 * i] = MIN(MAX(left[0], -1.0f), 1.0f);
        outputBuffer[2 * i + 1] = MIN(MAX(right[0], -1.0f), 1.0f);
    }
}

static int SilverBulletMK2_get_latency_samples(SilverBulletMK2* sb) {
    (void)sb;
    return 0;  // No latency introduced
}
#endif // SILVERBULLETMK2_H
