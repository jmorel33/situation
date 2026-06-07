/**
 * @file audio_freq_detect.h
 * @brief Capture mixed audio output and detect a target frequency (Goertzel).
 *
 * Used by harness MIDI/audio integration tests to verify that a tone at an
 * expected pitch is present in SituationSetAudioOutputMonitor samples.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#ifndef SIT_AUDIO_FREQ_DETECT_H
#define SIT_AUDIO_FREQ_DETECT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float* samples;
    uint32_t capacity;
    uint32_t count;
    uint32_t channels;
} SitAudioFreqCapture;

/** Convert MIDI note number to equal-tempered frequency (A4 = 440 Hz). */
float sit_midi_note_to_hz(int note);

void sit_audio_freq_capture_init(SitAudioFreqCapture* cap, uint32_t max_frames, uint32_t channels);
void sit_audio_freq_capture_free(SitAudioFreqCapture* cap);
void sit_audio_freq_capture_reset(SitAudioFreqCapture* cap);

/** Monitor callback suitable for SituationSetAudioOutputMonitor(). */
void sit_audio_freq_capture_cb(const float* samples, uint32_t frame_count, void* user_data);

/**
 * Goertzel power at target_hz over interleaved stereo/mono samples.
 * Higher values indicate stronger energy at that frequency.
 */
float sit_goertzel_power(const float* samples, uint32_t frame_count, uint32_t channels,
                         float sample_rate, float target_hz);

/**
 * Verify captured audio contains expected_hz within tolerance_ratio (e.g. 0.03 = ±3%).
 * Sets out_measured_hz to the best-fit frequency when non-NULL.
 */
bool sit_audio_freq_verify(const SitAudioFreqCapture* cap, float sample_rate, float expected_hz,
                           float tolerance_ratio, float min_rms, float* out_measured_hz);

/** RMS of mono-mixed interleaved capture. */
float sit_audio_capture_rms(const SitAudioFreqCapture* cap);

/** Peak absolute sample (mono-mixed) in capture. */
float sit_audio_capture_peak(const SitAudioFreqCapture* cap);

/** Pearson correlation of mono-mixed samples (uses min length of a/b). */
float sit_audio_capture_correlation(const SitAudioFreqCapture* a, const SitAudioFreqCapture* b);

/**
 * True when wet capture differs audibly from dry (not silent, lower correlation
 * and/or meaningful RMS change vs dry reference).
 */
bool sit_audio_effect_heard(const SitAudioFreqCapture* dry, const SitAudioFreqCapture* wet,
                            float min_wet_rms);

/** RMS over [start_frame, start_frame + frame_count) mono-mixed samples. */
float sit_audio_capture_window_rms(const SitAudioFreqCapture* cap, uint32_t start_frame,
                                   uint32_t frame_count);

/** Pearson correlation between aligned windows in two captures. */
float sit_audio_capture_window_correlation(const SitAudioFreqCapture* a, uint32_t a_start,
                                             const SitAudioFreqCapture* b, uint32_t b_start,
                                             uint32_t frame_count);

#endif /* SIT_AUDIO_FREQ_DETECT_H */
