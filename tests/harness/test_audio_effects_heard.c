/**
 * @file test_audio_effects_heard.c
 * @brief Per-effect audible verification — graph tone synth node through each FX.
 *
 * Each test emits a brief 440 Hz sine and verifies the processed output differs
 * from a dry tone-only reference (correlation / RMS change + non-silent meter).
 *
 * Also includes a reverb dry/wet mix sweep: 440 Hz square, wet 0→1 (1 s), hold (0.5 s),
 * then 1→0 (1 s), verified via RMS envelope at dry vs wet windows.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "audio_freq_detect.h"
#include "sit_test_audio_window.h"
#include "sit_test_stereo_scope.h"
#include "sit_test_listen_overlay.h"
#include "sit_test_audio_levels.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t control_id;
    float value;
} SitEffectHeardControl;

typedef struct {
    SituationNodeType effect_type;
    const SitEffectHeardControl* controls;
    int num_controls;
    uint32_t wet_capture_ms; /* 0 = default */
} SitEffectHeardSpec;

#define SIT_EFFECT_HEARD_DRY_CAPTURE_MS 200u
#define SIT_EFFECT_HEARD_WET_CAPTURE_MS 200u
/* Maximizer harmonic check needs a full square capture — do not halve these. */
#define SIT_EFFECT_HEARD_SQUARE_DRY_MS 1000u
#define SIT_EFFECT_HEARD_SQUARE_WET_MS 800u

static bool sit_effect_heard_audio_ready(int* out_sr) {
    int sr = SituationGetAudioPlaybackSampleRate();
    if (out_sr) *out_sr = sr;
    return sr > 0;
}

static void sit_effect_heard_configure_tone(SituationAudioGraph* graph, SituationNodeHandle tone) {
    SituationSetControl(graph, tone, 0, 440.0f);
    SituationSetControl(graph, tone, 1, 0.0f);   /* sine */
    SituationSetControl(graph, tone, 2, 0.75f);  /* volume */
}

static void sit_effect_heard_configure_square_tone(SituationAudioGraph* graph, SituationNodeHandle tone) {
    SituationSetControl(graph, tone, 0, 440.0f);
    SituationSetControl(graph, tone, 1, 1.0f);   /* square */
    SituationSetControl(graph, tone, 2, 0.75f);  /* volume */
}

static void sit_effect_heard_apply_controls(SituationAudioGraph* graph, SituationNodeHandle effect,
                                            const SitEffectHeardControl* controls, int num_controls) {
    for (int i = 0; i < num_controls; i++) {
        SituationSetControl(graph, effect, controls[i].control_id, controls[i].value);
    }
}

static void sit_effect_heard_patch_tone_to_effect(SituationAudioGraph* graph,
                                                  SituationNodeHandle tone,
                                                  SituationNodeHandle effect,
                                                  int effect_audio_ins) {
    SituationCreatePatch(graph, tone, 0, effect, 0, false);
    if (effect_audio_ins >= 2) {
        SituationCreatePatch(graph, tone, 0, effect, 1, false);
    }
}

static void sit_effect_heard_capture(SitAudioFreqCapture* cap, uint32_t sleep_ms) {
    sit_audio_freq_capture_reset(cap);
    sit_test_listen_overlay_pump_ms(sleep_ms);
}

/** Wall-clock wet ramp with ~30 Hz scope/spectrum refresh (listen overlay render path). */
static void sit_effect_heard_pump_wet_sweep(SituationAudioGraph* graph, SituationNodeHandle reverb,
                                            uint32_t duration_ms, float wet_from, float wet_to,
                                            const char* phase_label) {
    if (duration_ms == 0u) {
        return;
    }
    const uint32_t slice_ms = 8u;
#if defined(_WIN32)
    const DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < duration_ms) {
        const DWORD elapsed = GetTickCount() - t0;
        float t = (float)elapsed / (float)duration_ms;
        if (t > 1.0f) {
            t = 1.0f;
        }
        const float wet = wet_from + (wet_to - wet_from) * t;

        (void)sit_test_listen_overlay_render_once();

        SituationSetControl(graph, reverb, 2, wet);

        {
            char seg[72];
            snprintf(seg, sizeof(seg), "%s  wet %.0f%%", phase_label ? phase_label : "sweep",
                     wet * 100.0f);
            sit_test_listen_overlay_set("reverb_mix_dry_wet_sweep", seg,
                                        "square 440 Hz through reverb");
        }

        const DWORD left = duration_ms - elapsed;
        const DWORD step = left > slice_ms ? slice_ms : (left > 0u ? left : 1u);
        sit_test_listen_overlay_wait_ms(step);
    }
    (void)sit_test_listen_overlay_render_once();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint64_t t0_ms =
        (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const uint64_t now_ms =
            (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
        const uint64_t elapsed_ms = now_ms - t0_ms;
        if (elapsed_ms >= (uint64_t)duration_ms) {
            break;
        }
        float t = (float)elapsed_ms / (float)duration_ms;
        const float wet = wet_from + (wet_to - wet_from) * t;

        (void)sit_test_listen_overlay_render_once();

        SituationSetControl(graph, reverb, 2, wet);

        char seg[72];
        snprintf(seg, sizeof(seg), "%s  wet %.0f%%", phase_label ? phase_label : "sweep",
                 wet * 100.0f);
        sit_test_listen_overlay_set("reverb_mix_dry_wet_sweep", seg,
                                    "square 440 Hz through reverb");

        const uint64_t left = (uint64_t)duration_ms - elapsed_ms;
        const uint32_t step =
            left > (uint64_t)slice_ms ? slice_ms : (uint32_t)(left > 0u ? left : 1u);
        sit_test_listen_overlay_wait_ms(step);
    }
    (void)sit_test_listen_overlay_render_once();
#endif
}

/** Drain the audio thread after clearing the active graph (avoid use-after-free on destroy). */
static void sit_effect_heard_release_graph(SituationAudioGraph* graph) {
    sit_test_audio_monitor_set_capture(NULL);
    SituationSetActiveGraph(NULL);
    SituationStopAllTones();
    sit_test_listen_overlay_pump_ms(40);
    if (graph) {
        SituationDestroyGraph(graph);
    }
}

static bool sit_effect_heard_capture_tone_only(int sample_rate, SitAudioFreqCapture* cap) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) return false;

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    if (SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone) != SITUATION_SUCCESS) {
        sit_effect_heard_release_graph(graph);
        return false;
    }

    sit_effect_heard_configure_tone(graph, tone);
    if (SituationSetActiveGraph(graph) != SITUATION_SUCCESS) {
        sit_effect_heard_release_graph(graph);
        return false;
    }

    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_arm_capture(cap);
    sit_effect_heard_capture(cap, SIT_EFFECT_HEARD_DRY_CAPTURE_MS);

    float peak = 0.f, rms = 0.f;
    SituationGetMasterOutputMeter(&peak, &rms);

    sit_effect_heard_release_graph(graph);

    float cap_rms = sit_audio_capture_rms(cap);
    const bool audible = cap->count >= 256 && (peak > 0.005f || cap_rms > 0.003f);
    if (audible) {
        SIT_ASSERT_TONE_CAPTURE_LEVELS(cap, peak, rms, "effect_heard_dry_tone");
    }
    return audible;
}

static bool sit_effect_heard_capture_through_effect(int sample_rate, SituationNodeType effect_type,
                                                    const SitEffectHeardControl* controls, int num_controls,
                                                    SitAudioFreqCapture* cap, uint32_t capture_ms) {
    SituationDeviceMetadata meta = {0};
    if (SituationGetDeviceMetadata(effect_type, &meta) != SITUATION_SUCCESS) {
        return false;
    }

    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) return false;

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle effect = SITUATION_INVALID_NODE_HANDLE;

    if (SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone) != SITUATION_SUCCESS ||
        SituationCreateNode(graph, effect_type, &effect) != SITUATION_SUCCESS) {
        sit_effect_heard_release_graph(graph);
        return false;
    }

    sit_effect_heard_configure_tone(graph, tone);
    sit_effect_heard_apply_controls(graph, effect, controls, num_controls);
    sit_effect_heard_patch_tone_to_effect(graph, tone, effect, meta.num_audio_ins);

    if (SituationSetActiveGraph(graph) != SITUATION_SUCCESS) {
        sit_effect_heard_release_graph(graph);
        return false;
    }

    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_arm_capture(cap);
    sit_effect_heard_capture(cap, capture_ms > 0u ? capture_ms : SIT_EFFECT_HEARD_WET_CAPTURE_MS);

    float peak = 0.f, rms = 0.f;
    SituationGetMasterOutputMeter(&peak, &rms);

    sit_effect_heard_release_graph(graph);

    float cap_rms = sit_audio_capture_rms(cap);
    const bool audible = cap->count >= 256 && (peak > 0.005f || cap_rms > 0.003f);
    return audible;
}

static bool sit_effect_heard_verify_wet(const SitAudioFreqCapture* dry, const SitAudioFreqCapture* wet) {
    sit_test_stereo_scope_service_ui();
    return sit_audio_effect_heard(dry, wet, 0.005f);
}

static void sit_effect_heard_run_named(const char* test_name, const SitEffectHeardSpec* spec) {
    int sr = 0;
    if (!sit_effect_heard_audio_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SitAudioFreqCapture dry;
    SitAudioFreqCapture wet;
    sit_audio_freq_capture_init(&dry, (uint32_t)sr, 2);
    sit_audio_freq_capture_init(&wet, (uint32_t)sr, 2);

    sit_test_listen_overlay_set(test_name ? test_name : "effect_heard", "dry tone capture",
                                "440 Hz sine, no FX");
    if (!sit_effect_heard_capture_tone_only(sr, &dry)) {
        sit_test_listen_overlay_pump_ms(60);
        SIT_ASSERT(sit_effect_heard_capture_tone_only(sr, &dry));
    }
    sit_test_listen_overlay_set(test_name ? test_name : "effect_heard", "wet capture",
                                "tone through effect");
    if (!sit_effect_heard_capture_through_effect(sr, spec->effect_type, spec->controls,
                                                 spec->num_controls, &wet,
                                                 spec->wet_capture_ms)) {
        sit_test_listen_overlay_pump_ms(60);
        SIT_ASSERT(sit_effect_heard_capture_through_effect(sr, spec->effect_type, spec->controls,
                                                           spec->num_controls, &wet,
                                                           spec->wet_capture_ms));
    }
    sit_test_stereo_scope_service_ui();
    SIT_ASSERT_TONE_CAPTURE_LEVELS(&dry, -1.0f, -1.0f, test_name ? test_name : "effect_heard_dry");
    SIT_ASSERT_EFFECT_CAPTURE_LEVELS(&dry, &wet, -1.0f, -1.0f, test_name ? test_name : "effect_heard_wet");
    SIT_ASSERT(sit_effect_heard_verify_wet(&dry, &wet));
    sit_test_stereo_scope_service_ui();

    sit_audio_freq_capture_free(&dry);
    sit_audio_freq_capture_free(&wet);
}

#define SIT_EFFECT_HEARD_DEF(name, type, ...)                                          \
    static const SitEffectHeardControl sit_effect_heard_##name##_ctrls[] = { __VA_ARGS__ }; \
    static void test_graph_tone_synth_effect_heard_##name(void) {                      \
        static const SitEffectHeardSpec spec = {                                       \
            type,                                                                        \
            sit_effect_heard_##name##_ctrls,                                            \
            (int)(sizeof(sit_effect_heard_##name##_ctrls) /                             \
                   sizeof(sit_effect_heard_##name##_ctrls[0])),                        \
            0u                                                                           \
        };                                                                               \
        sit_effect_heard_run_named(#name, &spec);                                      \
    }

#define SIT_EFFECT_HEARD_DEF_WET_MS(name, wet_ms, type, ...)                           \
    static const SitEffectHeardControl sit_effect_heard_##name##_ctrls[] = { __VA_ARGS__ }; \
    static void test_graph_tone_synth_effect_heard_##name(void) {                      \
        static const SitEffectHeardSpec spec = {                                       \
            type,                                                                        \
            sit_effect_heard_##name##_ctrls,                                            \
            (int)(sizeof(sit_effect_heard_##name##_ctrls) /                             \
                   sizeof(sit_effect_heard_##name##_ctrls[0])),                        \
            (wet_ms)                                                                     \
        };                                                                               \
        sit_effect_heard_run_named(#name, &spec);                                      \
    }

SIT_EFFECT_HEARD_DEF(reverb,
    SITUATION_NODE_REVERB,
    { 2, 1.0f }, { 3, 0.2f }, { 0, 0.85f })

SIT_EFFECT_HEARD_DEF_WET_MS(echo, 380u,
    SITUATION_NODE_ECHO,
    { 0, 0.35f }, { 1, 0.55f }, { 2, 0.85f })

SIT_EFFECT_HEARD_DEF(chorus,
    SITUATION_NODE_CHORUS,
    { 18, 0.45f }, { 17, 0.5f }, { 1, 2.0f }, { 2, 8.0f })

SIT_EFFECT_HEARD_DEF(phaser,
    SITUATION_NODE_PHASER,
    { 2, 1.0f }, { 0, 1.5f }, { 1, 0.7f })

SIT_EFFECT_HEARD_DEF(overdrive,
    SITUATION_NODE_OVERDRIVE,
    { 0, 8.0f }, { 1, 1.0f }, { 3, 0.85f })

SIT_EFFECT_HEARD_DEF(exciter,
    SITUATION_NODE_EXCITER,
    { 1, 25.0f }, { 2, 0.9f }, { 0, 3000.0f })

static bool sit_effect_heard_capture_square_tone_only(int sample_rate, SitAudioFreqCapture* cap);
static bool sit_effect_heard_capture_square_through_effect(int sample_rate, SituationNodeType effect_type,
                                                           const SitEffectHeardControl* controls, int num_controls,
                                                           SitAudioFreqCapture* cap);

static bool sit_effect_heard_capture_square_through_effect(int sample_rate, SituationNodeType effect_type,
                                                           const SitEffectHeardControl* controls, int num_controls,
                                                           SitAudioFreqCapture* cap) {
    SituationDeviceMetadata meta = {0};
    if (SituationGetDeviceMetadata(effect_type, &meta) != SITUATION_SUCCESS) {
        return false;
    }

    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) return false;

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle effect = SITUATION_INVALID_NODE_HANDLE;

    if (SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone) != SITUATION_SUCCESS ||
        SituationCreateNode(graph, effect_type, &effect) != SITUATION_SUCCESS) {
        sit_effect_heard_release_graph(graph);
        return false;
    }

    sit_effect_heard_configure_square_tone(graph, tone);
    sit_effect_heard_apply_controls(graph, effect, controls, num_controls);
    sit_effect_heard_patch_tone_to_effect(graph, tone, effect, meta.num_audio_ins);

    if (SituationSetActiveGraph(graph) != SITUATION_SUCCESS) {
        sit_effect_heard_release_graph(graph);
        return false;
    }

    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_arm_capture(cap);
    sit_effect_heard_capture(cap, SIT_EFFECT_HEARD_SQUARE_WET_MS);

    sit_effect_heard_release_graph(graph);
    return cap->count >= 256;
}

static void test_graph_tone_synth_effect_heard_maximizer(void) {
    static const SitEffectHeardControl ctrls[] = {
        { 0, 440.0f }, { 1, 1.5f }, { 2, 8.0f }, { 3, 8.0f },
        { 4, 880.0f }, { 6, 7.0f }, { 7, 6.0f },
        { 16, 40.0f }, { 17, 12000.0f }
    };
    int sr = 0;
    if (!sit_effect_heard_audio_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SitAudioFreqCapture dry;
    SitAudioFreqCapture wet;
    sit_audio_freq_capture_init(&dry, (uint32_t)sr, 2);
    sit_audio_freq_capture_init(&wet, (uint32_t)sr, 2);

    SIT_ASSERT(sit_effect_heard_capture_square_tone_only(sr, &dry));
    SIT_ASSERT(sit_effect_heard_capture_square_through_effect(sr, SITUATION_NODE_MAXIMIZER,
                                                                ctrls, (int)(sizeof(ctrls) / sizeof(ctrls[0])),
                                                                &wet));

    SIT_ASSERT_TONE_CAPTURE_LEVELS(&dry, -1.0f, -1.0f, "maximizer_dry");
    SIT_ASSERT_EFFECT_CAPTURE_LEVELS(&dry, &wet, -1.0f, -1.0f, "maximizer_wet");

    /* FFT maximizer adds harmonics — compare 2nd partial power, not raw correlation. */
    const float dry_h2 = sit_goertzel_power(dry.samples, dry.count, dry.channels, (float)sr, 880.0f);
    const float wet_h2 = sit_goertzel_power(wet.samples, wet.count, wet.channels, (float)sr, 880.0f);
    const bool harmonic_boost = wet_h2 > dry_h2 * 1.15f;
    const bool heard = sit_effect_heard_verify_wet(&dry, &wet);
    SIT_ASSERT(harmonic_boost || heard);

    sit_audio_freq_capture_free(&dry);
    sit_audio_freq_capture_free(&wet);
}

SIT_EFFECT_HEARD_DEF(spring_reverb,
    SITUATION_NODE_SPRING_REVERB,
    { 8, 8.0f }, { 7, 3.0f }, { 2, 2.5f })

SIT_EFFECT_HEARD_DEF(studio_reverb,
    SITUATION_NODE_STUDIO_REVERB,
    { 8, 0.85f }, { 1, 3.0f }, { 0, 11.0f })

SIT_EFFECT_HEARD_DEF(sst282,
    SITUATION_NODE_SST282,
    { 6, 10.0f }, { 3, 180.0f }, { 4, 6.0f }, { 5, 4.0f })

SIT_EFFECT_HEARD_DEF(dynamics,
    SITUATION_NODE_DYNAMICS,
    { 0, -18.0f }, { 1, 8.0f }, { 2, 0.003f }, { 4, 6.0f })

SIT_EFFECT_HEARD_DEF(compander,
    SITUATION_NODE_COMPANDER,
    { 6, 6.0f }, { 14, 4.0f }, { 22, 4.0f })

SIT_EFFECT_HEARD_DEF(mastering_amp,
    SITUATION_NODE_MASTERING_AMP,
    { 1, 0.85f }, { 10, 0.6f }, { 3, 0.3f })

SIT_EFFECT_HEARD_DEF(deafmax,
    SITUATION_NODE_DEAFMAX,
    { 0, 40.0f }, { 2, -0.3f }, { 3, 3.0f })

#undef SIT_EFFECT_HEARD_DEF

static void test_graph_tone_synth_effect_heard_eq_4band(void) {
    /* Registry peak band: freq / gain / Q */
    static const SitEffectHeardControl ctrls[] = {
        { 5, 440.0f }, { 6, 6.0f }, { 7, 1.5f }
    };
    static const SitEffectHeardSpec spec = {
        SITUATION_NODE_EQ_4BAND, ctrls, (int)(sizeof(ctrls) / sizeof(ctrls[0]))
    };
    int sr = 0;
    if (!sit_effect_heard_audio_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SitAudioFreqCapture dry;
    SitAudioFreqCapture wet;
    sit_audio_freq_capture_init(&dry, (uint32_t)sr, 2);
    sit_audio_freq_capture_init(&wet, (uint32_t)sr, 2);

    SIT_ASSERT(sit_effect_heard_capture_tone_only(sr, &dry));
    SIT_ASSERT(sit_effect_heard_capture_through_effect(sr, spec.effect_type,
                                                         spec.controls, spec.num_controls, &wet,
                                                         0u));

    float dry_tone = sit_goertzel_power(dry.samples, dry.count, dry.channels, (float)sr, 440.0f);
    float wet_tone = sit_goertzel_power(wet.samples, wet.count, wet.channels, (float)sr, 440.0f);
    SIT_ASSERT(wet_tone > dry_tone * 1.3f);

    sit_audio_freq_capture_free(&dry);
    sit_audio_freq_capture_free(&wet);
}

static void test_graph_tone_synth_effect_heard_filter(void) {
    /* Registry: cutoff / resonance / type (0=lowpass) */
    static const SitEffectHeardControl ctrls[] = {
        { 0, 120.0f }, { 1, 2.5f }, { 2, 0.0f }
    };
    static const SitEffectHeardSpec spec = {
        SITUATION_NODE_FILTER, ctrls, (int)(sizeof(ctrls) / sizeof(ctrls[0]))
    };
    int sr = 0;
    if (!sit_effect_heard_audio_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SitAudioFreqCapture dry;
    SitAudioFreqCapture wet;
    sit_audio_freq_capture_init(&dry, (uint32_t)sr, 2);
    sit_audio_freq_capture_init(&wet, (uint32_t)sr, 2);

    SIT_ASSERT(sit_effect_heard_capture_tone_only(sr, &dry));
    SIT_ASSERT(sit_effect_heard_capture_through_effect(sr, spec.effect_type,
                                                         spec.controls, spec.num_controls, &wet,
                                                         0u));

    sit_test_stereo_scope_service_ui();
    float dry_tone = sit_goertzel_power(dry.samples, dry.count, dry.channels, (float)sr, 440.0f);
    sit_test_stereo_scope_service_ui();
    float wet_tone = sit_goertzel_power(wet.samples, wet.count, wet.channels, (float)sr, 440.0f);
    sit_test_stereo_scope_service_ui();
    SIT_ASSERT(wet_tone < dry_tone * 0.6f);

    sit_audio_freq_capture_free(&dry);
    sit_audio_freq_capture_free(&wet);
}

static bool sit_effect_heard_capture_square_tone_only(int sample_rate, SitAudioFreqCapture* cap) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) return false;

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    if (SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone) != SITUATION_SUCCESS) {
        SituationDestroyGraph(graph);
        return false;
    }

    sit_effect_heard_configure_square_tone(graph, tone);
    if (SituationSetActiveGraph(graph) != SITUATION_SUCCESS) {
        SituationDestroyGraph(graph);
        return false;
    }

    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_arm_capture(cap);
    sit_effect_heard_capture(cap, SIT_EFFECT_HEARD_SQUARE_DRY_MS);

    float peak = 0.f, rms = 0.f;
    SituationGetMasterOutputMeter(&peak, &rms);

    sit_test_audio_monitor_set_capture(NULL);
    SituationSetActiveGraph(NULL);
    SituationDestroyGraph(graph);

    float cap_rms = sit_audio_capture_rms(cap);
    const bool audible = cap->count >= 256 && (peak > 0.005f || cap_rms > 0.003f);
    if (audible) {
        SitTestAudioLevelLimits limits;
        sit_test_audio_level_limits_tone_defaults(&limits);
        limits.max_rms = 0.98f;
        char msg[320];
        if (!sit_test_audio_levels_check(cap, peak, rms, &limits, msg, sizeof(msg))) {
            fprintf(stderr, "[audio_levels] square_tone_ref: %s\n", msg);
            SIT_ASSERT(false);
        }
    }
    return audible;
}

static void test_graph_tone_synth_reverb_mix_dry_wet_sweep(void) {
    int sr = 0;
    if (!sit_effect_heard_audio_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    const uint32_t ramp_up_ms = 1200u;
    const uint32_t hold_wet_ms = 600u;
    const uint32_t ramp_down_ms = 1200u;
    const uint32_t total_ms = ramp_up_ms + hold_wet_ms + ramp_down_ms;
    const uint32_t window_ms = 250u;
    const uint32_t window_frames = (uint32_t)(((uint64_t)window_ms * (uint32_t)sr) / 1000u);
    const uint32_t capture_frames = (uint32_t)(((uint64_t)(total_ms + 150) * (uint32_t)sr) / 1000u);

    SitAudioFreqCapture dry_ref;
    SitAudioFreqCapture sweep;
    sit_audio_freq_capture_init(&dry_ref, (uint32_t)sr + 256, 2);
    sit_audio_freq_capture_init(&sweep, capture_frames, 2);

    SIT_ASSERT(sit_effect_heard_capture_square_tone_only(sr, &dry_ref));

    SituationDeviceMetadata meta = {0};
    SIT_ASSERT_EQ(SituationGetDeviceMetadata(SITUATION_NODE_REVERB, &meta), SITUATION_SUCCESS);

    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SIT_ASSERT_EQ(SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb), SITUATION_SUCCESS);

    sit_effect_heard_configure_square_tone(graph, tone);
    SituationSetControl(graph, reverb, 0, 0.92f);  /* room_size */
    SituationSetControl(graph, reverb, 1, 0.25f);  /* damping */
    SituationSetControl(graph, reverb, 2, 0.0f);   /* wet_level */
    SituationSetControl(graph, reverb, 3, 0.40f);  /* dry_level — leave headroom for wet tail */
    SituationSetControl(graph, reverb, 4, 1.0f);   /* width */
    sit_effect_heard_patch_tone_to_effect(graph, tone, reverb, meta.num_audio_ins);

    SIT_ASSERT_EQ(SituationSetActiveGraph(graph), SITUATION_SUCCESS);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_arm_capture(&sweep);
    sit_test_listen_overlay_set("reverb_mix_dry_wet_sweep", "wet 0% (dry)",
                                "square 440 Hz through reverb");
    (void)sit_test_listen_overlay_render_once();

    sit_effect_heard_pump_wet_sweep(graph, reverb, ramp_up_ms, 0.0f, 1.0f, "ramp up");
    sit_effect_heard_pump_wet_sweep(graph, reverb, hold_wet_ms, 1.0f, 1.0f, "hold wet");
    sit_effect_heard_pump_wet_sweep(graph, reverb, ramp_down_ms, 1.0f, 0.0f, "ramp down");

    sit_effect_heard_release_graph(graph);

    SIT_ASSERT(sweep.count >= window_frames * 3);

    sit_test_stereo_scope_service_ui();
    sit_test_listen_overlay_pump_ms(40);

    const uint32_t peak_frame =
        (uint32_t)(((uint64_t)(ramp_up_ms + hold_wet_ms / 2u) * (uint32_t)sr) / 1000u);
    const uint32_t end_frame = sweep.count > window_frames ? sweep.count - window_frames : 0;

    float rms_wet0 = 0.0f;
    float rms_wet_peak = 0.0f;
    float rms_dry_end = 0.0f;
    const struct {
        uint32_t frame;
        float* out;
    } rms_windows[] = {
        {0, &rms_wet0},
        {peak_frame, &rms_wet_peak},
        {end_frame, &rms_dry_end},
    };
    for (size_t wi = 0; wi < sizeof(rms_windows) / sizeof(rms_windows[0]); wi++) {
        sit_test_stereo_scope_service_ui();
        *rms_windows[wi].out =
            sit_audio_capture_window_rms(&sweep, rms_windows[wi].frame, window_frames);
    }
    sit_test_stereo_scope_service_ui();

    /* Wet ramp should lift level vs the dry-only start of the same capture. */
    SIT_ASSERT(rms_wet_peak > rms_wet0 * 1.002f);
    SIT_ASSERT(rms_dry_end <= rms_wet_peak * 0.995f);
    SIT_ASSERT_EFFECT_CAPTURE_LEVELS(&dry_ref, &sweep, -1.0f, -1.0f, "reverb_mix_dry_wet_sweep");

    sit_audio_freq_capture_free(&dry_ref);
    sit_audio_freq_capture_free(&sweep);
}

static void effect_heard_setup(void) {
    SituationInitInfo config;
    sit_test_audio_window_init_info(&config, "SIT_TEST_EFFECTS_HEARD");

    SituationError err = SituationInit(0, NULL, &config);
    if (err != SITUATION_SUCCESS) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    sit_test_audio_monitor_install();
}

static void effect_heard_teardown(void) {
    sit_test_audio_monitor_uninstall();
    SituationSetActiveGraph(NULL);
    SituationStopAllTones();
    SituationShutdown();
}

static SitTestCase effect_heard_tests[] = {
    {"graph_tone_synth_effect_heard_reverb",         test_graph_tone_synth_effect_heard_reverb,         true},
    {"graph_tone_synth_effect_heard_echo",           test_graph_tone_synth_effect_heard_echo,           true},
    {"graph_tone_synth_effect_heard_chorus",         test_graph_tone_synth_effect_heard_chorus,         true},
    {"graph_tone_synth_effect_heard_phaser",         test_graph_tone_synth_effect_heard_phaser,         true},
    {"graph_tone_synth_effect_heard_overdrive",      test_graph_tone_synth_effect_heard_overdrive,      true},
    {"graph_tone_synth_effect_heard_exciter",        test_graph_tone_synth_effect_heard_exciter,        true},
    {"graph_tone_synth_effect_heard_maximizer",      test_graph_tone_synth_effect_heard_maximizer,      true},
    {"graph_tone_synth_effect_heard_spring_reverb",  test_graph_tone_synth_effect_heard_spring_reverb,  true},
    {"graph_tone_synth_effect_heard_studio_reverb",  test_graph_tone_synth_effect_heard_studio_reverb,  true},
    {"graph_tone_synth_effect_heard_sst282",         test_graph_tone_synth_effect_heard_sst282,         true},
    {"graph_tone_synth_effect_heard_dynamics",       test_graph_tone_synth_effect_heard_dynamics,       true},
    {"graph_tone_synth_effect_heard_compander",      test_graph_tone_synth_effect_heard_compander,      true},
    {"graph_tone_synth_effect_heard_eq_4band",       test_graph_tone_synth_effect_heard_eq_4band,       true},
    {"graph_tone_synth_effect_heard_filter",         test_graph_tone_synth_effect_heard_filter,         true},
    {"graph_tone_synth_effect_heard_mastering_amp",  test_graph_tone_synth_effect_heard_mastering_amp,  true},
    {"graph_tone_synth_effect_heard_deafmax",        test_graph_tone_synth_effect_heard_deafmax,        true},
    {"graph_tone_synth_reverb_mix_dry_wet_sweep",    test_graph_tone_synth_reverb_mix_dry_wet_sweep,    true},
};

const SitTestModule g_module_audio_effects_heard = {
    .name = "audio_effects_heard",
    .setup = effect_heard_setup,
    .teardown = effect_heard_teardown,
    .tests = effect_heard_tests,
    .test_count = sizeof(effect_heard_tests) / sizeof(effect_heard_tests[0]),
    .requires_context = true,
    .harness_visual = true,
};
