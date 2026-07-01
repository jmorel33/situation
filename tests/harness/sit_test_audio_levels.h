/**
 * @file sit_test_audio_levels.h
 * @brief Peak/RMS guardrails for harness tone and FX captures.
 *
 * Include after sit_test_framework.h and sit_test_stereo_scope.h when using
 * finish helpers or SIT_ASSERT_*_CAPTURE_LEVELS macros.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#ifndef SIT_TEST_AUDIO_LEVELS_H
#define SIT_TEST_AUDIO_LEVELS_H

#include "audio_freq_detect.h"
#include "sit_api_include.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SitTestAudioLevelLimits {
    uint32_t min_frames;     /* minimum captured frames (default 128) */
    float min_rms;           /* inaudible / broken capture floor */
    float max_peak;          /* absolute clip guard on capture (+ meter when provided) */
    float max_rms;           /* hot RMS guard on capture (+ meter when provided) */
    float max_peak_vs_ref;   /* 0 = off; wet peak <= ref_peak * this (FX vs dry) */
    float max_rms_vs_ref;    /* 0 = off; wet rms <= ref_rms * this (FX vs dry) */
} SitTestAudioLevelLimits;

void sit_test_audio_level_limits_tone_defaults(SitTestAudioLevelLimits* limits);
void sit_test_audio_level_limits_effect_defaults(SitTestAudioLevelLimits* limits);

/**
 * Validate capture (and optional master meter) against limits.
 * Pass meter_peak or meter_rms < 0 to skip that meter check.
 * On failure, writes a short reason into out_msg when out_msg_cap > 0.
 */
bool sit_test_audio_levels_check(const SitAudioFreqCapture* cap,
                                 float meter_peak,
                                 float meter_rms,
                                 const SitTestAudioLevelLimits* limits,
                                 char* out_msg,
                                 size_t out_msg_cap);

/** Tone capture + optional dry reference for wet FX path. */
bool sit_test_audio_levels_check_effect(const SitAudioFreqCapture* dry_ref,
                                        const SitAudioFreqCapture* wet,
                                        float meter_peak,
                                        float meter_rms,
                                        const SitTestAudioLevelLimits* limits,
                                        char* out_msg,
                                        size_t out_msg_cap);

#if defined(SIT_TEST_FRAMEWORK_H) && defined(SIT_TEST_STEREO_SCOPE_H)

#include <stdio.h>

static inline void sit_test_audio_levels_fail_tone(const SitAudioFreqCapture* cap,
                                                  float meter_peak,
                                                  float meter_rms,
                                                  const SitTestAudioLevelLimits* limits,
                                                  const char* label) {
    char msg[320];
    if (sit_test_audio_levels_check(cap, meter_peak, meter_rms, limits, msg, sizeof(msg))) {
        return;
    }
    fprintf(stderr,
            "[audio_levels] %s: %s (cap peak=%.4f rms=%.4f frames=%u",
            label ? label : "tone",
            msg,
            cap ? sit_audio_capture_peak(cap) : 0.0f,
            cap ? sit_audio_capture_rms(cap) : 0.0f,
            cap ? cap->count : 0u);
    if (meter_peak >= 0.0f) {
        fprintf(stderr, " meter peak=%.4f rms=%.4f", meter_peak, meter_rms);
    }
    fputc('\n', stderr);
    sit_test_fail_impl(__FILE__, __LINE__, "tone capture levels", msg);
}

static inline void sit_test_audio_monitor_finish_tone_capture(SitAudioFreqCapture* cap,
                                                              const char* label) {
    if (cap && cap->count > 0) {
        float meter_peak = -1.0f;
        float meter_rms = -1.0f;
        SituationGetMasterOutputMeter(&meter_peak, &meter_rms);
        SitTestAudioLevelLimits limits;
        sit_test_audio_level_limits_tone_defaults(&limits);
        sit_test_audio_levels_fail_tone(cap, meter_peak, meter_rms, &limits, label);
    }
    sit_test_audio_monitor_set_capture(NULL);
}

static inline void sit_test_audio_levels_fail_effect(const SitAudioFreqCapture* dry_ref,
                                                     const SitAudioFreqCapture* wet,
                                                     float meter_peak,
                                                     float meter_rms,
                                                     const SitTestAudioLevelLimits* limits,
                                                     const char* label) {
    char msg[320];
    if (sit_test_audio_levels_check_effect(dry_ref, wet, meter_peak, meter_rms, limits, msg,
                                           sizeof(msg))) {
        return;
    }
    fprintf(stderr,
            "[audio_levels] %s: %s (wet peak=%.4f rms=%.4f",
            label ? label : "effect",
            msg,
            wet ? sit_audio_capture_peak(wet) : 0.0f,
            wet ? sit_audio_capture_rms(wet) : 0.0f);
    if (dry_ref && dry_ref->count > 0) {
        fprintf(stderr, " dry peak=%.4f rms=%.4f", sit_audio_capture_peak(dry_ref),
                sit_audio_capture_rms(dry_ref));
    }
    if (meter_peak >= 0.0f) {
        fprintf(stderr, " meter peak=%.4f rms=%.4f", meter_peak, meter_rms);
    }
    fputc('\n', stderr);
    sit_test_fail_impl(__FILE__, __LINE__, "effect capture levels", msg);
}

static inline void sit_test_audio_monitor_finish_effect_capture(SitAudioFreqCapture* dry_ref,
                                                                SitAudioFreqCapture* wet,
                                                                const char* label) {
    if (wet && wet->count > 0) {
        float meter_peak = -1.0f;
        float meter_rms = -1.0f;
        SituationGetMasterOutputMeter(&meter_peak, &meter_rms);
        SitTestAudioLevelLimits limits;
        sit_test_audio_level_limits_effect_defaults(&limits);
        sit_test_audio_levels_fail_effect(dry_ref, wet, meter_peak, meter_rms, &limits, label);
    }
    sit_test_audio_monitor_set_capture(NULL);
}

#define SIT_ASSERT_TONE_CAPTURE_LEVELS(cap, meter_peak, meter_rms, label)              \
    do {                                                                               \
        SitTestAudioLevelLimits _sit_tone_lim;                                         \
        sit_test_audio_level_limits_tone_defaults(&_sit_tone_lim);                    \
        sit_test_audio_levels_fail_tone((cap), (meter_peak), (meter_rms), &_sit_tone_lim, \
                                        (label));                                      \
    } while (0)

#define SIT_ASSERT_EFFECT_CAPTURE_LEVELS(dry_ref, wet, meter_peak, meter_rms, label)   \
    do {                                                                               \
        SitTestAudioLevelLimits _sit_fx_lim;                                           \
        sit_test_audio_level_limits_effect_defaults(&_sit_fx_lim);                      \
        sit_test_audio_levels_fail_effect((dry_ref), (wet), (meter_peak), (meter_rms), \
                                          &_sit_fx_lim, (label));                      \
    } while (0)

#endif /* framework + stereo_scope */

#endif /* SIT_TEST_AUDIO_LEVELS_H */
