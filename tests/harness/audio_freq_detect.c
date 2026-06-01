/**
 * @file audio_freq_detect.c
 * @brief Goertzel-based frequency detection for harness audio verification.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "audio_freq_detect.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
#define GOERTZEL_HAS_FMA 1
#if defined(__GNUC__) || defined(__clang__)
#define GOERTZEL_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
#else
#define GOERTZEL_FMA(a, b, c) fmaf((a), (b), (c))
#endif
#else
#define GOERTZEL_HAS_FMA 0
#define GOERTZEL_FMA(a, b, c) ((a) * (b) + (c))
#endif

float sit_midi_note_to_hz(int note) {
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

void sit_audio_freq_capture_init(SitAudioFreqCapture* cap, uint32_t max_frames, uint32_t channels) {
    if (!cap) return;
    cap->channels = channels > 0 ? channels : 2;
    cap->capacity = max_frames;
    cap->count = 0;
    cap->samples = (float*)malloc(max_frames * cap->channels * sizeof(float));
}

void sit_audio_freq_capture_free(SitAudioFreqCapture* cap) {
    if (!cap) return;
    free(cap->samples);
    cap->samples = NULL;
    cap->capacity = 0;
    cap->count = 0;
}

void sit_audio_freq_capture_reset(SitAudioFreqCapture* cap) {
    if (!cap) return;
    cap->count = 0;
}

void sit_audio_freq_capture_cb(const float* samples, uint32_t frame_count, void* user_data) {
    SitAudioFreqCapture* cap = (SitAudioFreqCapture*)user_data;
    if (!cap || !cap->samples || !samples || frame_count == 0) return;

    uint32_t ch = cap->channels;
    for (uint32_t f = 0; f < frame_count; f++) {
        if (cap->count >= cap->capacity) return;
        for (uint32_t c = 0; c < ch; c++) {
            cap->samples[cap->count * ch + c] = samples[f * ch + c];
        }
        cap->count++;
    }
}

static float sit_mix_to_mono(const float* samples, uint32_t index, uint32_t channels) {
    if (channels == 1) return samples[index];
    const float l = samples[index * channels];
    const float r = samples[index * channels + 1];
    return GOERTZEL_FMA(0.5f, l, GOERTZEL_FMA(0.5f, r, 0.0f));
}

float sit_goertzel_power(const float* samples, uint32_t frame_count, uint32_t channels,
                         float sample_rate, float target_hz) {
    if (!samples || frame_count < 8 || sample_rate <= 0.0f || target_hz <= 0.0f) return 0.0f;

    const float k = GOERTZEL_FMA((float)frame_count, target_hz, 0.0f) / sample_rate;
    const float w = GOERTZEL_FMA(2.0f * (float)M_PI, k, 0.0f) / (float)frame_count;
    float coeff = 2.0f * cosf(w);
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;

    for (uint32_t i = 0; i < frame_count; i++) {
        const float x = sit_mix_to_mono(samples, i, channels);
        s0 = GOERTZEL_FMA(-1.0f, s2, GOERTZEL_FMA(coeff, s1, x));
        s2 = s1;
        s1 = s0;
    }

    const float s1_sq = GOERTZEL_FMA(s1, s1, 0.0f);
    const float s1s2 = s1 * s2;
    return GOERTZEL_FMA(s2, s2, GOERTZEL_FMA(-coeff, s1s2, s1_sq));
}

static float sit_capture_rms(const SitAudioFreqCapture* cap) {
    if (!cap || !cap->samples || cap->count == 0) return 0.0f;
    double sum = 0.0;
    uint32_t ch = cap->channels;
    for (uint32_t i = 0; i < cap->count; i++) {
        float x = sit_mix_to_mono(cap->samples, i, ch);
        sum += (double)x * (double)x;
    }
    return (float)sqrt(sum / (double)cap->count);
}

bool sit_audio_freq_verify(const SitAudioFreqCapture* cap, float sample_rate, float expected_hz,
                           float tolerance_ratio, float min_rms, float* out_measured_hz) {
    if (!cap || !cap->samples || cap->count < 256 || sample_rate <= 0.0f || expected_hz <= 0.0f) {
        return false;
    }

    float rms = sit_capture_rms(cap);
    if (rms < min_rms) return false;

    float best_hz = expected_hz;
    float best_power = 0.0f;

    /* Coarse search around expected frequency. */
    float lo = expected_hz * (1.0f - tolerance_ratio * 2.0f);
    float hi = expected_hz * (1.0f + tolerance_ratio * 2.0f);
    if (lo < 20.0f) lo = 20.0f;

    for (int step = 0; step <= 40; step++) {
        float t = (float)step / 40.0f;
        const float hz = GOERTZEL_FMA(hi - lo, t, lo);
        float power = sit_goertzel_power(cap->samples, cap->count, cap->channels, sample_rate, hz);
        if (power > best_power) {
            best_power = power;
            best_hz = hz;
        }
    }

  /* Expected bin must dominate immediate neighbors. */
    float at_expected = sit_goertzel_power(cap->samples, cap->count, cap->channels, sample_rate, expected_hz);
    float below = expected_hz * 0.90f;
    float above = expected_hz * 1.10f;
    float power_below = sit_goertzel_power(cap->samples, cap->count, cap->channels, sample_rate, below);
    float power_above = sit_goertzel_power(cap->samples, cap->count, cap->channels, sample_rate, above);

    if (at_expected < power_below * 1.5f || at_expected < power_above * 1.5f) {
        return false;
    }

    float delta = fabsf(best_hz - expected_hz) / expected_hz;
    if (delta > tolerance_ratio) return false;

    if (out_measured_hz) *out_measured_hz = best_hz;
    return true;
}

float sit_audio_capture_rms(const SitAudioFreqCapture* cap) {
    return sit_capture_rms(cap);
}

float sit_audio_capture_correlation(const SitAudioFreqCapture* a, const SitAudioFreqCapture* b) {
    if (!a || !b || !a->samples || !b->samples || a->count == 0 || b->count == 0) return 0.0f;

    uint32_t n = a->count < b->count ? a->count : b->count;
    if (n < 32) return 0.0f;

    uint32_t ch = a->channels;
    if (b->channels < ch) ch = b->channels;

    double sum_a = 0.0, sum_b = 0.0, sum_ab = 0.0, sum_a2 = 0.0, sum_b2 = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double va = (double)sit_mix_to_mono(a->samples, i, ch);
        double vb = (double)sit_mix_to_mono(b->samples, i, ch);
        sum_a += va;
        sum_b += vb;
        sum_ab += va * vb;
        sum_a2 += va * va;
        sum_b2 += vb * vb;
    }

    double inv_n = 1.0 / (double)n;
    double mean_a = sum_a * inv_n;
    double mean_b = sum_b * inv_n;
    double cov = sum_ab * inv_n - mean_a * mean_b;
    double var_a = sum_a2 * inv_n - mean_a * mean_a;
    double var_b = sum_b2 * inv_n - mean_b * mean_b;
    if (var_a <= 1e-12 || var_b <= 1e-12) return 0.0f;

    return (float)(cov / sqrt(var_a * var_b));
}

bool sit_audio_effect_heard(const SitAudioFreqCapture* dry, const SitAudioFreqCapture* wet,
                            float min_wet_rms) {
    if (!dry || !wet) return false;

    float wet_rms = sit_audio_capture_rms(wet);
    float dry_rms = sit_audio_capture_rms(dry);
    if (dry_rms < min_wet_rms * 0.5f) return false;

    /* Strong attenuation (e.g. high-pass on a tone) still proves the effect ran. */
    if (dry_rms > min_wet_rms && wet_rms < dry_rms * 0.65f) return true;

    /* Boost (e.g. EQ band gain) also proves processing. */
    if (dry_rms > min_wet_rms && wet_rms > dry_rms * 1.08f) return true;

    if (wet_rms < min_wet_rms) return false;

    float corr = sit_audio_capture_correlation(dry, wet);
    float rms_delta = fabsf(wet_rms - dry_rms) / fmaxf(dry_rms, 1e-6f);

    return corr < 0.995f || rms_delta > 0.06f;
}

float sit_audio_capture_window_rms(const SitAudioFreqCapture* cap, uint32_t start_frame,
                                     uint32_t frame_count) {
    if (!cap || !cap->samples || frame_count == 0) return 0.0f;
    if (start_frame >= cap->count) return 0.0f;

    uint32_t end = start_frame + frame_count;
    if (end > cap->count) end = cap->count;
    if (end <= start_frame) return 0.0f;

    double sum = 0.0;
    uint32_t n = end - start_frame;
    uint32_t ch = cap->channels;
    for (uint32_t i = start_frame; i < end; i++) {
        float x = sit_mix_to_mono(cap->samples, i, ch);
        sum += (double)x * (double)x;
    }
    return (float)sqrt(sum / (double)n);
}

float sit_audio_capture_window_correlation(const SitAudioFreqCapture* a, uint32_t a_start,
                                           const SitAudioFreqCapture* b, uint32_t b_start,
                                           uint32_t frame_count) {
    if (!a || !b || !a->samples || !b->samples || frame_count < 32) return 0.0f;
    if (a_start + frame_count > a->count || b_start + frame_count > b->count) return 0.0f;

    uint32_t ch = a->channels;
    if (b->channels < ch) ch = b->channels;

    double sum_a = 0.0, sum_b = 0.0, sum_ab = 0.0, sum_a2 = 0.0, sum_b2 = 0.0;
    double inv_n = 1.0 / (double)frame_count;
    for (uint32_t i = 0; i < frame_count; i++) {
        double va = (double)sit_mix_to_mono(a->samples, a_start + i, ch);
        double vb = (double)sit_mix_to_mono(b->samples, b_start + i, ch);
        sum_a += va;
        sum_b += vb;
        sum_ab += va * vb;
        sum_a2 += va * va;
        sum_b2 += vb * vb;
    }

    double mean_a = sum_a * inv_n;
    double mean_b = sum_b * inv_n;
    double cov = sum_ab * inv_n - mean_a * mean_b;
    double var_a = sum_a2 * inv_n - mean_a * mean_a;
    double var_b = sum_b2 * inv_n - mean_b * mean_b;
    if (var_a <= 1e-12 || var_b <= 1e-12) return 0.0f;

    return (float)(cov / sqrt(var_a * var_b));
}
