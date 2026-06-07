/**
 * @file sit_test_audio_levels.c
 * @brief Peak/RMS guardrails for harness tone and FX captures.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_test_audio_levels.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void sit_test_audio_level_limits_tone_defaults(SitTestAudioLevelLimits* limits) {
    if (!limits) return;
    limits->min_frames = 128u;
    limits->min_rms = 0.003f;
    limits->max_peak = 1.05f;
    limits->max_rms = 0.92f;
    limits->max_peak_vs_ref = 0.0f;
    limits->max_rms_vs_ref = 0.0f;
}

void sit_test_audio_level_limits_effect_defaults(SitTestAudioLevelLimits* limits) {
    if (!limits) return;
    limits->min_frames = 128u;
    limits->min_rms = 0.003f;
    limits->max_peak = 1.65f;
    limits->max_rms = 1.05f;
    limits->max_peak_vs_ref = 1.55f;
    limits->max_rms_vs_ref = 1.85f;
}

static void sit_levels_write_msg(char* out_msg, size_t out_msg_cap, const char* fmt, ...) {
    if (!out_msg || out_msg_cap == 0) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(out_msg, out_msg_cap, fmt, args);
    va_end(args);
}

bool sit_test_audio_levels_check(const SitAudioFreqCapture* cap,
                                 float meter_peak,
                                 float meter_rms,
                                 const SitTestAudioLevelLimits* limits,
                                 char* out_msg,
                                 size_t out_msg_cap) {
    if (!limits) {
        sit_levels_write_msg(out_msg, out_msg_cap, "null limits");
        return false;
    }
    if (!cap || !cap->samples) {
        sit_levels_write_msg(out_msg, out_msg_cap, "null capture");
        return false;
    }
    if (cap->count < limits->min_frames) {
        sit_levels_write_msg(out_msg, out_msg_cap, "capture too short (%u frames, need %u)",
                             cap->count, limits->min_frames);
        return false;
    }

    const float cap_rms = sit_audio_capture_rms(cap);
    const float cap_peak = sit_audio_capture_peak(cap);

    if (!isfinite(cap_peak) || !isfinite(cap_rms)) {
        sit_levels_write_msg(out_msg, out_msg_cap, "capture non-finite levels (peak %.4f rms %.4f)",
                             cap_peak, cap_rms);
        return false;
    }
    if (cap_peak > 4.0f || cap_rms > 4.0f) {
        sit_levels_write_msg(out_msg, out_msg_cap, "capture runaway (peak %.4f rms %.4f)", cap_peak,
                             cap_rms);
        return false;
    }
    if (cap_rms < limits->min_rms) {
        sit_levels_write_msg(out_msg, out_msg_cap, "capture rms %.4f below min %.4f", cap_rms,
                             limits->min_rms);
        return false;
    }
    if (cap_peak > limits->max_peak) {
        sit_levels_write_msg(out_msg, out_msg_cap, "capture peak %.4f exceeds max %.4f", cap_peak,
                             limits->max_peak);
        return false;
    }
    if (cap_rms > limits->max_rms) {
        sit_levels_write_msg(out_msg, out_msg_cap, "capture rms %.4f exceeds max %.4f", cap_rms,
                             limits->max_rms);
        return false;
    }

    if (meter_peak >= 0.0f && meter_peak > limits->max_peak) {
        sit_levels_write_msg(out_msg, out_msg_cap, "meter peak %.4f exceeds max %.4f", meter_peak,
                             limits->max_peak);
        return false;
    }
    if (meter_rms >= 0.0f && meter_rms > limits->max_rms) {
        sit_levels_write_msg(out_msg, out_msg_cap, "meter rms %.4f exceeds max %.4f", meter_rms,
                             limits->max_rms);
        return false;
    }

    return true;
}

bool sit_test_audio_levels_check_effect(const SitAudioFreqCapture* dry_ref,
                                        const SitAudioFreqCapture* wet,
                                        float meter_peak,
                                        float meter_rms,
                                        const SitTestAudioLevelLimits* limits,
                                        char* out_msg,
                                        size_t out_msg_cap) {
    if (!sit_test_audio_levels_check(wet, meter_peak, meter_rms, limits, out_msg, out_msg_cap)) {
        return false;
    }
    if (!dry_ref || dry_ref->count < limits->min_frames || !wet || wet->count < limits->min_frames) {
        return true;
    }

    const float dry_peak = sit_audio_capture_peak(dry_ref);
    const float dry_rms = sit_audio_capture_rms(dry_ref);
    const float wet_peak = sit_audio_capture_peak(wet);
    const float wet_rms = sit_audio_capture_rms(wet);

    if (limits->max_peak_vs_ref > 0.0f && dry_peak > 1e-6f &&
        wet_peak > dry_peak * limits->max_peak_vs_ref) {
        sit_levels_write_msg(out_msg, out_msg_cap, "wet peak %.4f > dry peak %.4f * %.2f", wet_peak,
                             dry_peak, limits->max_peak_vs_ref);
        return false;
    }
    if (limits->max_rms_vs_ref > 0.0f && dry_rms > 1e-6f &&
        wet_rms > dry_rms * limits->max_rms_vs_ref) {
        sit_levels_write_msg(out_msg, out_msg_cap, "wet rms %.4f > dry rms %.4f * %.2f", wet_rms,
                             dry_rms, limits->max_rms_vs_ref);
        return false;
    }

    return true;
}
