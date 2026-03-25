/***************************************************************************************************
*
*   mastering_amp.h - Internal Mastering Amp Processor
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   High-end emulation of a classic mastering console processor.
*   Features 'Color' Amp simulation (Type A/N/C), 3-band EQ with Air,
*   and vintage saturation characteristics.
*
*   Optimized with SIMD (SSE) for performance.
*
*   This file is intended to be included within the audio subsystem implementation.
*
***************************************************************************************************/

#ifndef SIT_AUX_MASTERING_AMP_H
#define SIT_AUX_MASTERING_AMP_H

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

#define SIT_MASTERING_AMP_LUT_SIZE 4096
#define SIT_MASTERING_AMP_LUT_RANGE 10.0f // Range for saturation LUT

// Enum for Amp selection (A: Type A, N: Type N, C: Type C)
typedef enum {
    SIT_AMP_TYPE_A,
    SIT_AMP_TYPE_N,
    SIT_AMP_TYPE_C
} SituationMasteringAmpType;

// Biquad filter coefficients (aligned for SIMD)
typedef struct {
    __m128 a0, a1, a2, b1, b2;
} SituationBiquadCoeffSIMD __attribute__((aligned(16)));

// Biquad filter state (per channel, aligned for SIMD)
typedef struct {
    __m128 x1, x2, y1, y2;
} SituationBiquadStateSIMD __attribute__((aligned(16)));

// Main structure for the Mastering Amp processor
typedef struct {
    SituationMasteringAmpType ampType;        // Amp selection (A, N, C)
    float drive;            // Drive level (0.0 to 1.0)
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
    SituationBiquadCoeffSIMD eqCoeffs[5]; // Coefficients for TIGHT, LOW, MID, HIGH, AIR
    SituationBiquadStateSIMD leftStates[5];  // Filter states for left channel
    SituationBiquadStateSIMD rightStates[5]; // Filter states for right channel
    float* saturationLUT;   // Lookup table for saturation
    int lutSize;            // Size of the lookup table
    float cb_phase;         // Circuit bending LFO phase
} SituationMasteringAmp __attribute__((aligned(16)));

// Helper function: Soft clipping for Type C and non-linearities
static inline float _sit_ma_softClip(float x) {
    return x / (1.0f + fabsf(x));
}

// Helper function: Apply biquad filter using SIMD
static void _sit_ma_applyBiquadSIMD(const SituationBiquadCoeffSIMD* coeff, SituationBiquadStateSIMD* state, __m128* x) {
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
static void _sit_ma_computeLowShelfCoeffSIMD(SituationBiquadCoeffSIMD* coeff, float freq, float gainDB, float sampleRate) {
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
static void _sit_ma_computePeakCoeffSIMD(SituationBiquadCoeffSIMD* coeff, float freq, float gainDB, float Q, float sampleRate) {
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
static void _sit_ma_computeHighShelfCoeffSIMD(SituationBiquadCoeffSIMD* coeff, float freq, float gainDB, float sampleRate) {
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
static void _sit_ma_computeHighPassCoeffSIMD(SituationBiquadCoeffSIMD* coeff, float freq, float sampleRate) {
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

static void _SituationMasteringAmpDestroy(SituationMasteringAmp* amp) {
    if (amp->saturationLUT) {
        _mm_free(amp->saturationLUT);
        amp->saturationLUT = NULL; // Prevent double-free
    }
    // Note: Caller must free amp if it was dynamically allocated
}

// Forward declarations for setters to be used in init
static void _SituationMasteringAmpSetLowFreq(SituationMasteringAmp* amp, float freq);
static void _SituationMasteringAmpSetLowGain(SituationMasteringAmp* amp, float gain);
static void _SituationMasteringAmpSetMidFreq(SituationMasteringAmp* amp, float freq);
static void _SituationMasteringAmpSetMidGain(SituationMasteringAmp* amp, float gain);
static void _SituationMasteringAmpSetMidQ(SituationMasteringAmp* amp, float Q);
static void _SituationMasteringAmpSetHighFreq(SituationMasteringAmp* amp, float freq);
static void _SituationMasteringAmpSetHighGain(SituationMasteringAmp* amp, float gain);
static void _SituationMasteringAmpSetAirFreq(SituationMasteringAmp* amp, float freq);
static void _SituationMasteringAmpSetAirGain(SituationMasteringAmp* amp, float gain);
static void _SituationMasteringAmpSetTightCutoff(SituationMasteringAmp* amp, float cutoff);

// Initialize the processor
static void _SituationMasteringAmpInit(SituationMasteringAmp* amp, float sampleRate) {
    amp->ampType = SIT_AMP_TYPE_A;
    amp->drive = 0.5f;
    amp->lowFreq = 100.0f;
    amp->lowGain = 0.0f;
    amp->midFreq = 1000.0f;
    amp->midGain = 0.0f;
    amp->midQ = 1.0f;
    amp->highFreq = 10000.0f;
    amp->highGain = 0.0f;
    amp->airFreq = 15000.0f;
    amp->airGain = 0.0f;
    amp->tightCutoff = 20.0f;
    amp->aspectRatio = 1.0f;
    amp->vintage = false;
    amp->circuitBending = false;
    amp->sampleRate = sampleRate;

    // Initialize filter states to zero
    for (int i = 0; i < 5; i++) {
        amp->leftStates[i].x1 = _mm_setzero_ps();
        amp->leftStates[i].x2 = _mm_setzero_ps();
        amp->leftStates[i].y1 = _mm_setzero_ps();
        amp->leftStates[i].y2 = _mm_setzero_ps();
        amp->rightStates[i].x1 = _mm_setzero_ps();
        amp->rightStates[i].x2 = _mm_setzero_ps();
        amp->rightStates[i].y1 = _mm_setzero_ps();
        amp->rightStates[i].y2 = _mm_setzero_ps();
    }

    // Precompute saturation lookup table
    amp->lutSize = SIT_MASTERING_AMP_LUT_SIZE;
    amp->saturationLUT = (float*)_mm_malloc(sizeof(float) * SIT_MASTERING_AMP_LUT_SIZE, 16);
    if (!amp->saturationLUT) {
        // Ideally should handle OOM
        return;
    }

    for (int i = 0; i < SIT_MASTERING_AMP_LUT_SIZE; i++) {
        float x = (float)i / (SIT_MASTERING_AMP_LUT_SIZE - 1) * 2.0f * SIT_MASTERING_AMP_LUT_RANGE - SIT_MASTERING_AMP_LUT_RANGE;
        amp->saturationLUT[i] = tanhf(x);
    }

    amp->cb_phase = 0.0f;

    // Set initial coefficients
    _SituationMasteringAmpSetLowFreq(amp, 100.0f);
    _SituationMasteringAmpSetLowGain(amp, 0.0f);
    _SituationMasteringAmpSetMidFreq(amp, 1000.0f);
    _SituationMasteringAmpSetMidGain(amp, 0.0f);
    _SituationMasteringAmpSetMidQ(amp, 1.0f);
    _SituationMasteringAmpSetHighFreq(amp, 10000.0f);
    _SituationMasteringAmpSetHighGain(amp, 0.0f);
    _SituationMasteringAmpSetAirFreq(amp, 15000.0f);
    _SituationMasteringAmpSetAirGain(amp, 0.0f);
    _SituationMasteringAmpSetTightCutoff(amp, 20.0f);
}

// Setter functions with improved EQ flexibility
static void _SituationMasteringAmpSetLowFreq(SituationMasteringAmp* amp, float freq) {
    amp->lowFreq = MAX(20.0f, MIN(200.0f, freq));
    float gainDB = amp->lowGain * 12.0f;
    _sit_ma_computeLowShelfCoeffSIMD(&amp->eqCoeffs[1], amp->lowFreq, gainDB, amp->sampleRate);
}

static void _SituationMasteringAmpSetLowGain(SituationMasteringAmp* amp, float gain) {
    amp->lowGain = MAX(-1.0f, MIN(1.0f, gain));
    float gainDB = amp->lowGain * 12.0f;
    _sit_ma_computeLowShelfCoeffSIMD(&amp->eqCoeffs[1], amp->lowFreq, gainDB, amp->sampleRate);
}

static void _SituationMasteringAmpSetMidFreq(SituationMasteringAmp* amp, float freq) {
    amp->midFreq = MAX(200.0f, MIN(5000.0f, freq));
    float gainDB = amp->midGain * 12.0f;
    _sit_ma_computePeakCoeffSIMD(&amp->eqCoeffs[2], amp->midFreq, gainDB, amp->midQ, amp->sampleRate);
}

static void _SituationMasteringAmpSetMidGain(SituationMasteringAmp* amp, float gain) {
    amp->midGain = MAX(-1.0f, MIN(1.0f, gain));
    float gainDB = amp->midGain * 12.0f;
    _sit_ma_computePeakCoeffSIMD(&amp->eqCoeffs[2], amp->midFreq, gainDB, amp->midQ, amp->sampleRate);
}

static void _SituationMasteringAmpSetMidQ(SituationMasteringAmp* amp, float Q) {
    amp->midQ = MAX(0.5f, MIN(5.0f, Q));
    float gainDB = amp->midGain * 12.0f;
    _sit_ma_computePeakCoeffSIMD(&amp->eqCoeffs[2], amp->midFreq, gainDB, amp->midQ, amp->sampleRate);
}

static void _SituationMasteringAmpSetHighFreq(SituationMasteringAmp* amp, float freq) {
    amp->highFreq = MAX(5000.0f, MIN(20000.0f, freq));
    float gainDB = amp->highGain * 12.0f;
    _sit_ma_computeHighShelfCoeffSIMD(&amp->eqCoeffs[3], amp->highFreq, gainDB, amp->sampleRate);
}

static void _SituationMasteringAmpSetHighGain(SituationMasteringAmp* amp, float gain) {
    amp->highGain = MAX(-1.0f, MIN(1.0f, gain));
    float gainDB = amp->highGain * 12.0f;
    _sit_ma_computeHighShelfCoeffSIMD(&amp->eqCoeffs[3], amp->highFreq, gainDB, amp->sampleRate);
}

static void _SituationMasteringAmpSetAirFreq(SituationMasteringAmp* amp, float freq) {
    amp->airFreq = MAX(10000.0f, MIN(20000.0f, freq));
    float gainDB = amp->airGain * 12.0f;
    _sit_ma_computeHighShelfCoeffSIMD(&amp->eqCoeffs[4], amp->airFreq, gainDB, amp->sampleRate);
}

static void _SituationMasteringAmpSetAirGain(SituationMasteringAmp* amp, float gain) {
    amp->airGain = MAX(0.0f, MIN(1.0f, gain));
    float gainDB = amp->airGain * 12.0f;
    _sit_ma_computeHighShelfCoeffSIMD(&amp->eqCoeffs[4], amp->airFreq, gainDB, amp->sampleRate);
}

static void _SituationMasteringAmpSetTightCutoff(SituationMasteringAmp* amp, float cutoff) {
    amp->tightCutoff = MAX(20.0f, MIN(200.0f, cutoff));
    _sit_ma_computeHighPassCoeffSIMD(&amp->eqCoeffs[0], amp->tightCutoff, amp->sampleRate);
}

static void _SituationMasteringAmpSetAmpType(SituationMasteringAmp* amp, SituationMasteringAmpType type) {
    amp->ampType = type;
}

static void _SituationMasteringAmpSetDrive(SituationMasteringAmp* amp, float drive) {
    amp->drive = MAX(0.0f, MIN(1.0f, drive));
}

static void _SituationMasteringAmpSetAspectRatio(SituationMasteringAmp* amp, float aspect) {
    amp->aspectRatio = MAX(0.0f, MIN(2.0f, aspect));
}

static void _SituationMasteringAmpSetVintage(SituationMasteringAmp* amp, bool vintage) {
    amp->vintage = vintage;
}

static void _SituationMasteringAmpSetCircuitBending(SituationMasteringAmp* amp, bool bending) {
    amp->circuitBending = bending;
}

// Helper function for vectorized hard clipping
static inline __m128 _sit_ma_hardClipSIMD(__m128 x) {
    __m128 minVal = _mm_set1_ps(-1.0f);  // Set all four floats to -1.0
    __m128 maxVal = _mm_set1_ps(1.0f);   // Set all four floats to 1.0
    return _mm_max_ps(_mm_min_ps(x, maxVal), minVal); // Clamp between -1.0 and 1.0
}

// Main function to process SIT_AMP_TYPE_N mode with SIMD
static void _sit_ma_processAmpNSIMD(const float* inputBuffer, float* outputBuffer, int numFrames, float drive) {
    __m128 d = _mm_set1_ps(drive * 10.0f); // Apply drive to all four floats
    int i;

    // Process two stereo samples (four floats) at a time
    for (i = 0; i < numFrames - 1; i += 2) {
        // Load 4 floats: [left[i], right[i], left[i+1], right[i+1]]
        __m128 stereo = _mm_loadu_ps(&inputBuffer[2 * i]);
        __m128 x = _mm_mul_ps(stereo, d);       // Apply drive
        __m128 clipped = _sit_ma_hardClipSIMD(x);           // Hard clip
        _mm_storeu_ps(&outputBuffer[2 * i], clipped); // Store result
    }

    // Handle remaining single stereo sample if numFrames is odd
    for (; i < numFrames; i++) {
        float x_left = inputBuffer[2 * i] * drive * 10.0f;
        float x_right = inputBuffer[2 * i + 1] * drive * 10.0f;
        outputBuffer[2 * i] = fminf(fmaxf(x_left, -1.0f), 1.0f);     // Clamp left
        outputBuffer[2 * i + 1] = fminf(fmaxf(x_right, -1.0f), 1.0f); // Clamp right
    }
}

// Process stereo audio stream with SIMD and LUT
static void _SituationMasteringAmpProcessAudio(SituationMasteringAmp* amp, const float* inputBuffer, float* outputBuffer, int numFrames) {
    // Temporary buffer for Amp saturation output (stereo: 2 * numFrames)
    // WARNING: VLA on stack for large numFrames is dangerous.
    // Better to use a fixed scratch buffer or process in chunks.
    // For now, assuming numFrames is small (e.g. 512, 1024).
    float ampBuffer[2 * numFrames];

    // Step 1: Apply Amp saturation based on mode
    if (amp->ampType == SIT_AMP_TYPE_N) {
        // Vectorized hard clipping for SIT_AMP_TYPE_N using SIMD
        _sit_ma_processAmpNSIMD(inputBuffer, ampBuffer, numFrames, amp->drive);
    } else if (amp->ampType == SIT_AMP_TYPE_C) {
        // Scalar soft clipping for SIT_AMP_TYPE_C
        for (int i = 0; i < numFrames; i++) {
            float drive = amp->drive * 10.0f;
            float x_left = inputBuffer[2 * i] * drive;
            float x_right = inputBuffer[2 * i + 1] * drive;
            ampBuffer[2 * i] = _sit_ma_softClip(x_left);
            ampBuffer[2 * i + 1] = _sit_ma_softClip(x_right);
        }
    } else { // SIT_AMP_TYPE_A with LUT
        for (int i = 0; i < numFrames; i++) {
            float drive = amp->drive * 10.0f;
            float x_left = inputBuffer[2 * i] * drive;
            float x_right = inputBuffer[2 * i + 1] * drive;
            // LUT index calculation
            float idx_left = (x_left + SIT_MASTERING_AMP_LUT_RANGE) / (2.0f * SIT_MASTERING_AMP_LUT_RANGE) * (amp->lutSize - 1);
            float idx_right = (x_right + SIT_MASTERING_AMP_LUT_RANGE) / (2.0f * SIT_MASTERING_AMP_LUT_RANGE) * (amp->lutSize - 1);
            int idx_l = (int)idx_left;
            int idx_r = (int)idx_right;
            // Clamp indices and apply LUT
            ampBuffer[2 * i] = amp->saturationLUT[MAX(0, MIN(amp->lutSize - 1, idx_l))];
            ampBuffer[2 * i + 1] = amp->saturationLUT[MAX(0, MIN(amp->lutSize - 1, idx_r))];
        }
    }

    // Step 2: Process the rest of the audio chain sample-by-sample
    for (int i = 0; i < numFrames; i++) {
        __m128 left = _mm_set1_ps(ampBuffer[2 * i]);
        __m128 right = _mm_set1_ps(ampBuffer[2 * i + 1]);

        // EQ Section with per-filter non-linearities
        for (int f = 0; f < 5; f++) {
            _sit_ma_applyBiquadSIMD(&amp->eqCoeffs[f], &amp->leftStates[f], &left);
            left = _mm_set1_ps(_sit_ma_softClip(left[0] * 1.1f) * 0.9f);  // Non-linearity after each filter
            _sit_ma_applyBiquadSIMD(&amp->eqCoeffs[f], &amp->rightStates[f], &right);
            right = _mm_set1_ps(_sit_ma_softClip(right[0] * 1.1f) * 0.9f); // Non-linearity after each filter
        }

        // VINTAGE Effect (applied after EQ)
        if (amp->vintage) {
            left = _mm_set1_ps(tanhf(left[0] * 2.0f) * 0.8f);  // Gentle saturation for vintage warmth
            right = _mm_set1_ps(tanhf(right[0] * 2.0f) * 0.8f);
        }

        // Circuit Bending (applied after VINTAGE)
        if (amp->circuitBending) {
            float mod = sinf(amp->cb_phase) * 0.1f;
            left = _mm_set1_ps(left[0] + mod);
            right = _mm_set1_ps(right[0] - mod);
            amp->cb_phase += 0.1f;
            if (amp->cb_phase >= 2.0f * 3.14159f) amp->cb_phase -= 2.0f * 3.14159f;
        }

        // ASPECT RATIO (Stereo Imaging)
        float mid = (left[0] + right[0]) / 2.0f;
        float side = (left[0] - right[0]) / 2.0f;
        float width = amp->aspectRatio;
        side *= width;
        left = _mm_set1_ps(mid + side);
        right = _mm_set1_ps(mid - side);

        // Output with clipping prevention
        outputBuffer[2 * i] = MIN(MAX(left[0], -1.0f), 1.0f);
        outputBuffer[2 * i + 1] = MIN(MAX(right[0], -1.0f), 1.0f);
    }
}

static int _SituationMasteringAmpGetLatencySamples(SituationMasteringAmp* amp) {
    (void)amp;
    return 0;  // No latency introduced
}
#endif // SIT_AUX_MASTERING_AMP_H
