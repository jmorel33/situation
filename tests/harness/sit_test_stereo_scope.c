/**
 * @file sit_test_stereo_scope.c
 * @brief Harness-only stereo scope / spectrum (SituationSetAudioOutputMonitor tap).
 *
 * Display math (layout, AC-couple, 1 kHz scope window, line-strip plots) lives here — not in the library.
 */

#include "sit_test_stereo_scope.h"
#include "sit_test_listen_overlay.h"
#include "sit_test_visual_layout.h"
#include "sit_test_headless.h"
#include <math.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if defined(__FP_FAST_FMAF) || defined(__FMA__) || (defined(_MSC_VER) && defined(__AVX2__))
#define SCOPE_HAS_FMA 1
#if defined(__GNUC__) || defined(__clang__)
#define SCOPE_FMA(a, b, c) __builtin_fmaf((a), (b), (c))
#else
#define SCOPE_FMA(a, b, c) fmaf((a), (b), (c))
#endif
#else
#define SCOPE_HAS_FMA 0
#define SCOPE_FMA(a, b, c) ((a) * (b) + (c))
#endif

#define SCOPE_FMUL(a, b) SCOPE_FMA((a), (b), 0.0f)
#define SCOPE_FSUB(a, b) SCOPE_FMA(1.0f, (a), -(b))
#define SCOPE_FIDX3(i) ((uint32_t)SCOPE_FMUL(3.0f, (float)(i)))

#ifndef SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES
#define SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES 2048u
#endif

bool g_sit_test_headless = false;

static float g_scope_l[SIT_TEST_SCOPE_CAPACITY];
static float g_scope_r[SIT_TEST_SCOPE_CAPACITY];
static float g_scope_snap_l[SIT_TEST_SCOPE_CAPACITY];
static float g_scope_snap_r[SIT_TEST_SCOPE_CAPACITY];
static _Atomic uint32_t g_scope_write;
static uint32_t g_scope_last_spec_ms;
static uint32_t g_scope_snap_count;
static uint32_t g_scope_display_start;
static uint32_t g_scope_display_count;
static float g_scope_snap_peak;
static float g_scope_smoothed_peak;
static float g_spec_bar[SIT_TEST_SPECTRUM_BARS];
static float g_spec_smoothed[SIT_TEST_SPECTRUM_BARS];
static float g_spec_bar_hz[SIT_TEST_SPECTRUM_BARS];
static float g_spec_dominant_hz;
static float g_spec_display_peak;
static float g_spec_mono[SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES];
static SitAudioFreqCapture* g_monitor_capture;
static int g_monitor_installed;

/** Rolling harness UI present rate (listen overlay + audio_visual paths). */
static uint32_t g_scope_ui_frame_count;
static uint32_t g_scope_ui_fps_window_ms;
static float g_scope_ui_fps;

static void sit_scope_draw_quad_rect(SituationCommandBuffer cmd, float x, float y, float w,
                                     float h, ColorRGBA color) {
    mat4 model;
    glm_mat4_identity(model);
    /* Unit quad 0..1 — same transform as SituationCmdDrawTexture dest rect. */
    glm_translate(model, (vec3){x, y, 0.0f});
    glm_scale(model, (vec3){w, h, 1.0f});
    Vector4 tint = {{SCOPE_FMUL(color.r, 1.0f / 255.0f), SCOPE_FMUL(color.g, 1.0f / 255.0f),
                     SCOPE_FMUL(color.b, 1.0f / 255.0f), SCOPE_FMUL(color.a, 1.0f / 255.0f)}};
    (void)SituationCmdDrawQuad(cmd, model, tint);
}

static void sit_scope_draw_quad_center(SituationCommandBuffer cmd, float cx, float cy, float w,
                                       float h, ColorRGBA color) {
    sit_scope_draw_quad_rect(cmd, SCOPE_FMA(-0.5f, w, cx), SCOPE_FMA(-0.5f, h, cy), w, h, color);
}

/** Thin segment via internal 2D (SituationCmdDrawQuad) — no custom pipeline. */
static void sit_scope_draw_line(SituationCommandBuffer cmd, float x0, float y0, float x1,
                                float y1, float thickness, ColorRGBA color) {
    const float dx = SCOPE_FSUB(x1, x0);
    const float dy = SCOPE_FSUB(y1, y0);
    const float len = sqrtf(SCOPE_FMA(dx, dx, SCOPE_FMUL(dy, dy)));
    if (len < 0.5f) {
        sit_scope_draw_quad_center(cmd, x0, y0, thickness, thickness, color);
        return;
    }
    const float cx = SCOPE_FMA(0.5f, x1, SCOPE_FMA(0.5f, x0, 0.0f));
    const float cy = SCOPE_FMA(0.5f, y1, SCOPE_FMA(0.5f, y0, 0.0f));
    const float angle = atan2f(dy, dx);
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){cx, cy, 0.0f});
    glm_rotate(model, angle, (vec3){0.0f, 0.0f, 1.0f});
    glm_scale(model, (vec3){len, thickness, 1.0f});
    glm_translate(model, (vec3){-0.5f, -0.5f, 0.0f});
    Vector4 tint = {{SCOPE_FMUL(color.r, 1.0f / 255.0f), SCOPE_FMUL(color.g, 1.0f / 255.0f),
                     SCOPE_FMUL(color.b, 1.0f / 255.0f), SCOPE_FMUL(color.a, 1.0f / 255.0f)}};
    (void)SituationCmdDrawQuad(cmd, model, tint);
}

static void sit_scope_draw_line_segment(SituationCommandBuffer cmd, float x0, float y0, float x1,
                                        float y1, ColorRGBA color, float line_width) {
    sit_scope_draw_line(cmd, x0, y0, x1, y1, line_width, color);
}

static void sit_scope_draw_line_strip(SituationCommandBuffer cmd, const float* pix_xyz,
                                      uint32_t vertex_count, ColorRGBA color, float line_width) {
    for (uint32_t i = 1; i < vertex_count; i++) {
        sit_scope_draw_line(cmd, pix_xyz[SCOPE_FIDX3(i - 1u)], pix_xyz[SCOPE_FIDX3(i - 1u) + 1u],
                            pix_xyz[SCOPE_FIDX3(i)], pix_xyz[SCOPE_FIDX3(i) + 1u], line_width,
                            color);
    }
}

static uint32_t sit_scope_tuned_window_samples(int sample_rate) {
    if (sample_rate <= 0) {
        sample_rate = 48000;
    }
    const float samples_per_cycle = SCOPE_FMUL((float)sample_rate, 1.0f / SIT_TEST_SCOPE_TUNED_HZ);
    uint32_t window =
        (uint32_t)SCOPE_FMA(samples_per_cycle, SIT_TEST_SCOPE_CYCLES, 0.5f);
    if (window < 2u) {
        window = 2u;
    }
    if (window > SIT_TEST_SCOPE_CAPACITY) {
        window = SIT_TEST_SCOPE_CAPACITY;
    }
    return window;
}

static void sit_scope_log_hz_to_panel_x(float panel_x, float panel_w, float hz, float f_min,
                                        float f_max, float* out_x) {
    if (hz < f_min) {
        hz = f_min;
    }
    if (hz > f_max) {
        hz = f_max;
    }
    const float log_min = logf(f_min);
    const float log_max = logf(f_max);
    const float log_span = SCOPE_FSUB(log_max, log_min);
    float t = 0.0f;
    if (log_span > 1e-6f) {
        t = SCOPE_FMUL(SCOPE_FSUB(logf(hz), log_min), 1.0f / log_span);
    }
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    *out_x = SCOPE_FMA(t, panel_w, panel_x);
}

static void sit_test_audio_pump_messages(void) {
    if (!SituationIsInitialized()) {
        return;
    }
    SituationPollInputEvents();
    SituationUpdateTimers();
}

#if defined(_WIN32)
static void sit_test_harness_dispatch_win32_messages(void) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}
#endif

void sit_test_harness_poll_platform_events(void) {
#if defined(_WIN32)
    sit_test_harness_dispatch_win32_messages();
#endif
}

#if defined(_WIN32)
/** Internal slice wait (messages + timers only). Prefer sit_test_harness_wait_ms for test sleeps. */
static void sit_test_audio_wait_ms(uint32_t ms) {
    const DWORD deadline = GetTickCount() + ms;
    for (;;) {
        sit_test_harness_dispatch_win32_messages();
        sit_test_audio_pump_messages();
        DWORD now = GetTickCount();
        if (now >= deadline) {
            break;
        }
        DWORD left = deadline - now;
        DWORD slice = left > 8u ? 8u : left;
        if (slice == 0u) {
            slice = 1u;
        }
        (void)MsgWaitForMultipleObjectsEx(0, NULL, slice, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}
#else
static void sit_test_audio_wait_ms(uint32_t ms) {
    usleep((unsigned int)ms * 1000u);
}
#endif

static void sit_scope_clampf(float* v, float lo, float hi) {
    if (*v < lo) {
        *v = lo;
    } else if (*v > hi) {
        *v = hi;
    }
}

static uint32_t sit_test_stereo_scope_now_ms(void) {
#if defined(_WIN32)
    return GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
#endif
}

void sit_test_stereo_scope_note_ui_frame(void) {
    const uint32_t now_ms = sit_test_stereo_scope_now_ms();
    g_scope_ui_frame_count++;
    if (g_scope_ui_fps_window_ms == 0u) {
        g_scope_ui_fps_window_ms = now_ms;
        return;
    }
    const uint32_t elapsed = now_ms - g_scope_ui_fps_window_ms;
    if (elapsed >= 400u) {
        const float inv_elapsed = 1.0f / (float)(elapsed > 0u ? elapsed : 1u);
        g_scope_ui_fps =
            SCOPE_FMUL(SCOPE_FMA((float)g_scope_ui_frame_count, 1000.0f, 0.0f), inv_elapsed);
        g_scope_ui_frame_count = 0u;
        g_scope_ui_fps_window_ms = now_ms;
    }
}

float sit_test_stereo_scope_hud_fps(void) {
    return g_scope_ui_fps;
}

void sit_test_stereo_scope_format_bus_meter(char* buf, size_t buf_size) {
    if (!buf || buf_size == 0u) {
        return;
    }
    float peak = 0.0f;
    float rms = 0.0f;
    SituationGetMasterOutputMeter(&peak, &rms);
    const float fps = sit_test_stereo_scope_hud_fps();
    if (g_spec_dominant_hz > 1.0f) {
        snprintf(buf, buf_size, "bus peak=%.3f  rms=%.3f  scope_fps=%.1f  dom=%.0f Hz", peak,
                 rms, fps, g_spec_dominant_hz);
    } else {
        snprintf(buf, buf_size, "bus peak=%.3f  rms=%.3f  scope_fps=%.1f  scope_win=%.3f", peak,
                 rms, fps, g_scope_snap_peak);
    }
}

void sit_test_stereo_scope_init(void) {
    g_scope_ui_frame_count = 0u;
    g_scope_ui_fps_window_ms = 0u;
    g_scope_ui_fps = 0.0f;
    memset(g_scope_l, 0, sizeof(g_scope_l));
    memset(g_scope_r, 0, sizeof(g_scope_r));
    memset(g_spec_bar, 0, sizeof(g_spec_bar));
    memset(g_spec_smoothed, 0, sizeof(g_spec_smoothed));
    g_scope_write = 0;
    g_scope_snap_peak = 0.0f;
    g_scope_smoothed_peak = 1.0f;
    g_spec_dominant_hz = 0.0f;
    g_spec_display_peak = 0.0f;
    g_monitor_capture = NULL;
}

void sit_test_stereo_scope_reset_spectrum(void) {
    memset(g_spec_bar, 0, sizeof(g_spec_bar));
    memset(g_spec_smoothed, 0, sizeof(g_spec_smoothed));
    g_spec_dominant_hz = 0.0f;
    g_spec_display_peak = 0.0f;
    g_scope_last_spec_ms = 0u;
}

/** Map bin magnitude to 0..1 panel height using dB below the reference peak. */
static float sit_scope_spec_display_rel(float mag, float peak_ref) {
    if (mag <= 1e-9f || peak_ref <= 1e-9f) {
        return 0.0f;
    }
    const float db = 20.0f * log10f(mag / peak_ref);
    if (db <= SIT_TEST_SPECTRUM_DB_FLOOR) {
        return 0.0f;
    }
    return SCOPE_FMUL(SCOPE_FSUB(db, SIT_TEST_SPECTRUM_DB_FLOOR),
                      -1.0f / SIT_TEST_SPECTRUM_DB_FLOOR);
}

void sit_test_stereo_scope_reset(void) {
    g_scope_ui_frame_count = 0u;
    g_scope_ui_fps_window_ms = 0u;
    g_scope_ui_fps = 0.0f;
    atomic_store(&g_scope_write, 0u);
    g_scope_snap_count = 0;
    g_scope_display_start = 0;
    g_scope_display_count = 0;
    g_scope_snap_peak = 0.0f;
    g_scope_smoothed_peak = 1.0f;
    memset(g_scope_l, 0, sizeof(g_scope_l));
    memset(g_scope_r, 0, sizeof(g_scope_r));
    sit_test_stereo_scope_reset_spectrum();
}

static void sit_test_stereo_scope_finish_snap(uint32_t copy_count, uint32_t scope_window) {
    g_scope_snap_count = copy_count;
    g_scope_display_start = copy_count > scope_window ? copy_count - scope_window : 0u;
    g_scope_display_count = copy_count - g_scope_display_start;
    const uint32_t display_end = g_scope_display_start + g_scope_display_count;

    if (g_scope_display_count > 0u) {
        float sum_l = 0.0f;
        float sum_r = 0.0f;
        for (uint32_t i = g_scope_display_start; i < display_end; i++) {
            sum_l += g_scope_snap_l[i];
            sum_r += g_scope_snap_r[i];
        }
        const float inv = 1.0f / (float)g_scope_display_count;
        const float mean_l = SCOPE_FMA(sum_l, inv, 0.0f);
        const float mean_r = SCOPE_FMA(sum_r, inv, 0.0f);
        for (uint32_t i = g_scope_display_start; i < display_end; i++) {
            g_scope_snap_l[i] = SCOPE_FSUB(g_scope_snap_l[i], mean_l);
            g_scope_snap_r[i] = SCOPE_FSUB(g_scope_snap_r[i], mean_r);
        }
    }

    float peak = 0.0f;
    for (uint32_t i = g_scope_display_start; i < display_end; i++) {
        const float al = fabsf(g_scope_snap_l[i]);
        const float ar = fabsf(g_scope_snap_r[i]);
        if (al > peak) {
            peak = al;
        }
        if (ar > peak) {
            peak = ar;
        }
    }
    g_scope_snap_peak = peak;
    const float target_peak = peak > 0.08f ? peak : 0.08f;
    g_scope_smoothed_peak =
        SCOPE_FMA(target_peak, 0.15f, SCOPE_FMA(g_scope_smoothed_peak, 0.85f, 0.0f));
    if (g_scope_smoothed_peak < 0.08f) {
        g_scope_smoothed_peak = 0.08f;
    }
}

static bool sit_test_stereo_scope_snapshot_from_capture(const SitAudioFreqCapture* cap) {
    if (!cap || !cap->samples || cap->count < 2u) {
        return false;
    }

    const uint32_t count = cap->count;
    const uint32_t ch = cap->channels > 0u ? cap->channels : 2u;
    uint32_t copy_count = count;
    if (copy_count > SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES) {
        copy_count = SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES;
    }

    const uint32_t start_frame = count - copy_count;
    for (uint32_t i = 0; i < copy_count; i++) {
        const uint32_t frame = start_frame + i;
        const uint32_t sample_base = (uint32_t)SCOPE_FMUL((float)frame, (float)ch);
        const float l = cap->samples[sample_base];
        const float r = (ch > 1u) ? cap->samples[sample_base + 1u] : l;
        g_scope_snap_l[i] = l;
        g_scope_snap_r[i] = r;
    }

    const int sr = SituationGetAudioPlaybackSampleRate();
    const uint32_t scope_window = sit_scope_tuned_window_samples(sr);
    sit_test_stereo_scope_finish_snap(copy_count, scope_window);
    return true;
}

static void sit_test_stereo_scope_snapshot(void) {
    /* While capture is filling, show the live capture tail (effects sweeps). */
    if (g_monitor_capture && g_monitor_capture->samples && g_monitor_capture->count >= 64u &&
        g_monitor_capture->count < g_monitor_capture->capacity) {
        if (sit_test_stereo_scope_snapshot_from_capture(g_monitor_capture)) {
            return;
        }
    }

    const uint32_t write = atomic_load_explicit(&g_scope_write, memory_order_acquire);
    const uint32_t count =
        write > SIT_TEST_SCOPE_CAPACITY ? SIT_TEST_SCOPE_CAPACITY : write;
    if (count == 0u) {
        g_scope_snap_count = 0u;
        g_scope_display_start = 0u;
        g_scope_display_count = 0u;
        g_scope_snap_peak = 0.0f;
        return;
    }

    const int sr = SituationGetAudioPlaybackSampleRate();
    const uint32_t scope_window = sit_scope_tuned_window_samples(sr);
    uint32_t copy_count = count;
    if (copy_count > SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES) {
        copy_count = SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES;
    }

    for (uint32_t i = 0; i < copy_count; i++) {
        const uint32_t idx = (write - copy_count + i) % SIT_TEST_SCOPE_CAPACITY;
        g_scope_snap_l[i] = g_scope_l[idx];
        g_scope_snap_r[i] = g_scope_r[idx];
    }

    sit_test_stereo_scope_finish_snap(copy_count, scope_window);
}

static void sit_test_stereo_scope_compute_spectrum(void) {
    const uint32_t available = g_scope_snap_count;
    if (available < 256u) {
        g_spec_dominant_hz = 0.0f;
        memset(g_spec_bar, 0, sizeof(g_spec_bar));
        memset(g_spec_smoothed, 0, sizeof(g_spec_smoothed));
        return;
    }

    const int sr = SituationGetAudioPlaybackSampleRate();
    if (sr <= 0) {
        g_spec_dominant_hz = 0.0f;
        return;
    }

    const uint32_t n =
        available < SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES ? available
                                                       : SIT_TEST_SPECTRUM_ANALYSIS_SAMPLES;
    const uint32_t offset = available - n;
    const float nyquist = SCOPE_FMA((float)sr, 0.5f, 0.0f);
    const float f_max =
        SIT_TEST_SPECTRUM_MAX_HZ < nyquist ? SIT_TEST_SPECTRUM_MAX_HZ
                                           : SCOPE_FMA(nyquist, 0.98f, 0.0f);
    const float f_min = SIT_TEST_SPECTRUM_MIN_HZ;
    const float log_min = logf(f_min);
    const float log_max = logf(f_max);
    const float log_span = SCOPE_FSUB(log_max, log_min);

    for (uint32_t i = 0; i < n; i++) {
        const float hann = SCOPE_FMA(
            -0.5f,
            cosf(SCOPE_FMUL(SCOPE_FMUL(2.0f, (float)M_PI),
                            SCOPE_FMUL((float)i, 1.0f / (float)(n - 1u)))),
            0.5f);
        const float mono =
            SCOPE_FMA(0.5f, g_scope_snap_l[offset + i],
                      SCOPE_FMA(0.5f, g_scope_snap_r[offset + i], 0.0f));
        g_spec_mono[i] = SCOPE_FMA(mono, hann, 0.0f);
    }

    float max_mag = 0.0f;
    float dominant_hz = 0.0f;
    float dominant_mag = 0.0f;

    for (uint32_t b = 0; b < SIT_TEST_SPECTRUM_BARS; b++) {
        const float t =
            (SIT_TEST_SPECTRUM_BARS > 1u)
                ? SCOPE_FMUL((float)b, 1.0f / (float)(SIT_TEST_SPECTRUM_BARS - 1u))
                : 0.0f;
        const float hz = expf(SCOPE_FMA(log_span, t, log_min));
        g_spec_bar_hz[b] = hz;
        const float power =
            sit_goertzel_power(g_spec_mono, n, 1u, (float)sr, hz);
        const float mag = sqrtf(SCOPE_FMUL(power, 1.0f / (float)n));
        g_spec_bar[b] = mag;
        const float prev = g_spec_smoothed[b];
        if (mag >= prev) {
            g_spec_smoothed[b] =
                SCOPE_FMA(mag, SIT_TEST_SPECTRUM_ATTACK,
                          SCOPE_FMUL(prev, 1.0f - SIT_TEST_SPECTRUM_ATTACK));
        } else {
            g_spec_smoothed[b] =
                SCOPE_FMA(mag, 1.0f - SIT_TEST_SPECTRUM_RELEASE,
                          SCOPE_FMUL(prev, SIT_TEST_SPECTRUM_RELEASE));
        }
        if (g_spec_smoothed[b] > max_mag) {
            max_mag = g_spec_smoothed[b];
        }
    }

    const float inv_norm = 1.0f / (max_mag > 1e-8f ? max_mag : 1e-8f);
    for (uint32_t b = 0; b < SIT_TEST_SPECTRUM_BARS; b++) {
        const float rel = SCOPE_FMUL(g_spec_smoothed[b], inv_norm);
        if (rel > 0.55f && g_spec_smoothed[b] > dominant_mag) {
            dominant_mag = g_spec_smoothed[b];
            dominant_hz = g_spec_bar_hz[b];
        }
    }
    g_spec_dominant_hz = dominant_hz;
}

void sit_test_stereo_scope_frame_prepare(void) {
    sit_test_stereo_scope_snapshot();

#if defined(_WIN32)
    const uint32_t now_ms = GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint32_t now_ms =
        (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
#endif
#if SIT_TEST_SPECTRUM_UPDATE_MS == 0u
    (void)now_ms;
    sit_test_stereo_scope_compute_spectrum();
#else
    if (g_scope_last_spec_ms == 0u ||
        (now_ms - g_scope_last_spec_ms) >= SIT_TEST_SPECTRUM_UPDATE_MS) {
        sit_test_stereo_scope_compute_spectrum();
        g_scope_last_spec_ms = now_ms;
    }
#endif
}

void sit_test_stereo_scope_push(const float* samples, uint32_t frame_count, uint32_t channels) {
    if (!samples || frame_count == 0) {
        return;
    }
    const uint32_t ch = channels > 0 ? channels : 2u;
    for (uint32_t f = 0; f < frame_count; f++) {
        const uint32_t frame_base = (uint32_t)SCOPE_FMUL((float)f, (float)ch);
        const float l = samples[frame_base];
        const float r = (ch > 1u) ? samples[frame_base + 1u] : l;
        const uint32_t idx =
            atomic_fetch_add_explicit(&g_scope_write, 1u, memory_order_relaxed) %
            SIT_TEST_SCOPE_CAPACITY;
        g_scope_l[idx] = l;
        g_scope_r[idx] = r;
    }
    atomic_thread_fence(memory_order_release);
}

void sit_test_audio_monitor_set_capture(SitAudioFreqCapture* cap) {
    g_monitor_capture = cap;
}

void sit_test_audio_monitor_cb(const float* samples, uint32_t frame_count, void* user_data) {
    (void)user_data;
    /* Master bus from _SituationPublishMasterBusLevels is always interleaved stereo. */
    const uint32_t bus_ch = 2u;
    sit_test_stereo_scope_push(samples, frame_count, bus_ch);
    if (g_monitor_capture) {
        const uint32_t cap_ch =
            g_monitor_capture->channels > 0u ? g_monitor_capture->channels : bus_ch;
        if (cap_ch == bus_ch) {
            sit_audio_freq_capture_cb(samples, frame_count, g_monitor_capture);
        } else if (cap_ch == 1u) {
            for (uint32_t f = 0; f < frame_count; f++) {
                if (g_monitor_capture->count >= g_monitor_capture->capacity) {
                    break;
                }
                const uint32_t frame_base = (uint32_t)SCOPE_FMUL((float)f, (float)bus_ch);
                const float mono =
                    SCOPE_FMA(0.5f, samples[frame_base],
                              SCOPE_FMA(0.5f, samples[frame_base + 1u], 0.0f));
                g_monitor_capture->samples[g_monitor_capture->count++] = mono;
            }
        }
    }
}

void sit_test_audio_monitor_install(void) {
    if (!SituationIsInitialized()) {
        return;
    }
    sit_test_stereo_scope_init();
    SituationSetAudioOutputMonitor(sit_test_audio_monitor_cb, NULL);
    g_monitor_installed = 1;
}

void sit_test_audio_monitor_uninstall(void) {
    if (g_monitor_installed) {
        SituationSetAudioOutputMonitor(NULL, NULL);
        g_monitor_installed = 0;
    }
    g_monitor_capture = NULL;
}

void sit_test_stereo_scope_draw(SituationCommandBuffer cmd, float x, float y, float w, float h) {
    if (!cmd || w < 8.0f || h < 8.0f) {
        return;
    }

    const ColorRGBA panel = {24, 28, 40, 255};
    sit_scope_draw_quad_center(cmd, SCOPE_FMA(0.5f, w, x), SCOPE_FMA(0.5f, h, y), w, h, panel);
}

void sit_test_stereo_scope_draw_spectrum(SituationCommandBuffer cmd, float x, float y, float w,
                                         float h) {
    if (!cmd || w < 8.0f || h < 8.0f) {
        return;
    }

    const float sh = (float)SituationGetScreenHeight();
    const float rh = (float)SituationGetRenderHeight();
    const float fs = (sh > 0.0f) ? SCOPE_FMUL(rh, 1.0f / sh) : 1.0f;

    const ColorRGBA panel = {18, 22, 34, 255};
    sit_scope_draw_quad_center(cmd, SCOPE_FMA(0.5f, w, x), SCOPE_FMA(0.5f, h, y), w, h, panel);

    const SituationFont font = {0};
    const ColorRGBA label = {150, 158, 175, 255};
    const float label_y = SCOPE_FMA(4.0f, fs, y);
    const float label_size = SCOPE_FMA(11.0f, fs, 0.0f);
    SituationCmdDrawTextEx(cmd, font, "40Hz",
                           (Vector2){{SCOPE_FMA(4.0f, fs, x), label_y}}, label_size,
                           1.0f, label);
    SituationCmdDrawTextEx(
        cmd, font, "1k",
        (Vector2){{SCOPE_FMA(-10.0f, fs, SCOPE_FMA(0.5f, w, x)), label_y}},
        label_size, 1.0f, label);
    SituationCmdDrawTextEx(cmd, font, "12k",
                           (Vector2){{SCOPE_FMA(1.0f, w, SCOPE_FMA(-34.0f, fs, x)), label_y}},
                           label_size, 1.0f, label);

    char peak_line[96];
    if (g_spec_dominant_hz > 1.0f) {
        if (g_spec_dominant_hz >= 1000.0f) {
            snprintf(peak_line, sizeof(peak_line), "dom %.2f kHz",
                     SCOPE_FMUL(g_spec_dominant_hz, 0.001f));
        } else {
            snprintf(peak_line, sizeof(peak_line), "dom %.0f Hz", g_spec_dominant_hz);
        }
    } else {
        snprintf(peak_line, sizeof(peak_line), "spectrum L+R mono");
    }
    SituationCmdDrawTextEx(cmd, font, peak_line,
                           (Vector2){{SCOPE_FMA(0.32f, w, x), label_y}},
                           SCOPE_FMA(12.0f, fs, 0.0f), 1.0f, (ColorRGBA){200, 205, 220, 255});
}

void sit_test_stereo_scope_draw_lines(SituationCommandBuffer cmd, float scope_x, float scope_y,
                                      float scope_w, float scope_h, float spec_x, float spec_y,
                                      float spec_w, float spec_h) {
    if (!cmd) {
        return;
    }

    if (scope_w >= 8.0f && scope_h >= 8.0f) {
        const ColorRGBA grid = {55, 62, 82, 255};
        const ColorRGBA left_color = {80, 220, 120, 255};
        const ColorRGBA right_color = {110, 180, 255, 255};

        const float mid_y = SCOPE_FMA(0.5f, scope_h, scope_y);
        const float q1_y = SCOPE_FMA(0.25f, scope_h, scope_y);
        const float q3_y = SCOPE_FMA(0.75f, scope_h, scope_y);
        const float scope_right = SCOPE_FMA(1.0f, scope_w, scope_x);
        sit_scope_draw_line_segment(cmd, scope_x, mid_y, scope_right, mid_y, grid, 1.0f);
        sit_scope_draw_line_segment(cmd, scope_x, q1_y, scope_right, q1_y, grid, 1.0f);
        sit_scope_draw_line_segment(cmd, scope_x, q3_y, scope_right, q3_y, grid, 1.0f);

        const uint32_t span = g_scope_display_count;
        if (span >= 2u) {
            int columns = (int)scope_w;
            if (columns > (int)SIT_TEST_SCOPE_DRAW_COLUMNS) {
                columns = (int)SIT_TEST_SCOPE_DRAW_COLUMNS;
            }
            if (columns >= 2) {
                const float half_h = SCOPE_FMA(scope_h, 0.24f, 0.0f);
                const float left_mid = SCOPE_FMA(0.25f, scope_h, scope_y);
                const float right_mid = SCOPE_FMA(0.75f, scope_h, scope_y);
                const float l_y_min = SCOPE_FMA(2.0f, 1.0f, scope_y);
                const float l_y_max = SCOPE_FMA(-2.0f, 1.0f, SCOPE_FMA(0.5f, scope_h, scope_y));
                const float r_y_min = SCOPE_FMA(2.0f, 1.0f, SCOPE_FMA(0.5f, scope_h, scope_y));
                const float r_y_max = SCOPE_FMA(-2.0f, 1.0f, SCOPE_FMA(1.0f, scope_h, scope_y));
                const float amp =
                    SCOPE_FMUL(SCOPE_FMA(half_h, 0.90f, 0.0f), 1.0f / g_scope_smoothed_peak);
                const float x_span = SCOPE_FSUB(scope_w, 1.0f);
                const float x_step =
                    (columns > 1) ? SCOPE_FMUL(x_span, 1.0f / (float)(columns - 1)) : 0.0f;
                const uint32_t base = g_scope_display_start;

                float clip_l[SIT_TEST_SCOPE_DRAW_COLUMNS * 3];
                float clip_r[SIT_TEST_SCOPE_DRAW_COLUMNS * 3];

                for (int px = 0; px < columns; px++) {
                    const uint32_t off = (uint32_t)((int64_t)px * (int64_t)(span - 1u) /
                                                    (int64_t)(columns - 1));
                    const uint32_t snap_idx = base + off;
                    const float l = g_scope_snap_l[snap_idx];
                    const float r = g_scope_snap_r[snap_idx];
                    const float xf = SCOPE_FMA(x_step, (float)px, scope_x);
                    float l_y = SCOPE_FMA(-l, amp, left_mid);
                    float r_y = SCOPE_FMA(-r, amp, right_mid);
                    sit_scope_clampf(&l_y, l_y_min, l_y_max);
                    sit_scope_clampf(&r_y, r_y_min, r_y_max);
                    const uint32_t clip_base = SCOPE_FIDX3(px);
                    clip_l[clip_base] = xf;
                    clip_l[clip_base + 1u] = l_y;
                    clip_l[clip_base + 2u] = 0.0f;
                    clip_r[clip_base] = xf;
                    clip_r[clip_base + 1u] = r_y;
                    clip_r[clip_base + 2u] = 0.0f;
                }

                sit_scope_draw_line_strip(cmd, clip_l, (uint32_t)columns, left_color, 1.5f);
                sit_scope_draw_line_strip(cmd, clip_r, (uint32_t)columns, right_color, 1.5f);
            }
        }
    }

    if (spec_w < 8.0f || spec_h < 8.0f) {
        return;
    }

    const float sh = (float)SituationGetScreenHeight();
    const float rh = (float)SituationGetRenderHeight();
    const float fs = (sh > 0.0f) ? SCOPE_FMUL(rh, 1.0f / sh) : 1.0f;

    const ColorRGBA grid = {45, 52, 70, 255};
    const ColorRGBA trace = {90, 180, 255, 255};

    const int sr = SituationGetAudioPlaybackSampleRate();
    const float nyquist =
        (sr > 0) ? SCOPE_FMA((float)sr, 0.5f, 0.0f) : 24000.0f;
    const float f_max =
        SIT_TEST_SPECTRUM_MAX_HZ < nyquist ? SIT_TEST_SPECTRUM_MAX_HZ
                                           : SCOPE_FMA(nyquist, 0.98f, 0.0f);
    const float f_min = SIT_TEST_SPECTRUM_MIN_HZ;

    const float floor_y = SCOPE_FMA(-4.0f, 1.0f, SCOPE_FMA(1.0f, spec_h, spec_y));
    const float spec_right = SCOPE_FMA(1.0f, spec_w, spec_x);
    sit_scope_draw_line_segment(cmd, spec_x, floor_y, spec_right, floor_y, grid, 1.0f);
    static const float grid_hz[] = {40.0f, 1000.0f, 12000.0f};
    for (size_t g = 0; g < sizeof(grid_hz) / sizeof(grid_hz[0]); g++) {
        float gx = spec_x;
        sit_scope_log_hz_to_panel_x(spec_x, spec_w, grid_hz[g], f_min, f_max, &gx);
        sit_scope_draw_line_segment(cmd, gx, SCOPE_FMA(18.0f, fs, spec_y), gx,
                                    floor_y, grid, 1.0f);
    }

    float max_mag = 0.0f;
    for (uint32_t b = 0; b < SIT_TEST_SPECTRUM_BARS; b++) {
        if (g_spec_smoothed[b] > max_mag) {
            max_mag = g_spec_smoothed[b];
        }
    }
    if (max_mag > g_spec_display_peak) {
        g_spec_display_peak = max_mag;
    } else {
        g_spec_display_peak =
            fmaxf(max_mag, SCOPE_FMUL(g_spec_display_peak, SIT_TEST_SPECTRUM_DISPLAY_PEAK_DECAY));
    }
    const float peak_ref = g_spec_display_peak > 1e-9f ? g_spec_display_peak : max_mag;
    const float plot_h = SCOPE_FMA(-28.0f, fs, spec_h);

    if (SIT_TEST_SPECTRUM_BARS >= 2u) {
        float clip_spec[SIT_TEST_SPECTRUM_BARS * 3];
        for (uint32_t b = 0; b < SIT_TEST_SPECTRUM_BARS; b++) {
            const float rel = sit_scope_spec_display_rel(g_spec_smoothed[b], peak_ref);
            float xf = spec_x;
            sit_scope_log_hz_to_panel_x(spec_x, spec_w, g_spec_bar_hz[b], f_min, f_max, &xf);
            const float yf = SCOPE_FMA(-rel, plot_h, floor_y);
            const uint32_t clip_base = SCOPE_FIDX3(b);
            clip_spec[clip_base] = xf;
            clip_spec[clip_base + 1u] = yf;
            clip_spec[clip_base + 2u] = 0.0f;
        }
        sit_scope_draw_line_strip(cmd, clip_spec, SIT_TEST_SPECTRUM_BARS, trace, 1.5f);
    }
}

void sit_test_stereo_scope_draw_hud(SituationCommandBuffer cmd, SituationFont font, float x,
                                    float y) {
    if (!cmd) {
        return;
    }
    float peak = 0.0f;
    float rms = 0.0f;
    SituationGetMasterOutputMeter(&peak, &rms);
    char hud[256];
    if (g_spec_dominant_hz > 1.0f) {
        snprintf(hud, sizeof(hud),
                 "L/R scope @ %.0f Hz x %.0f cyc  scope_fps=%.1f  peak=%.3f  dom=%.0f Hz",
                 SIT_TEST_SCOPE_TUNED_HZ, SIT_TEST_SCOPE_CYCLES, sit_test_stereo_scope_hud_fps(),
                 peak, g_spec_dominant_hz);
    } else {
        snprintf(hud, sizeof(hud),
                 "L/R scope @ %.0f Hz x %.0f cyc  scope_fps=%.1f  peak=%.3f  fit=±%.2f",
                 SIT_TEST_SCOPE_TUNED_HZ, SIT_TEST_SCOPE_CYCLES, sit_test_stereo_scope_hud_fps(),
                 peak, g_scope_smoothed_peak);
    }
    const ColorRGBA white = {220, 225, 235, 255};
    SituationCmdDrawTextEx(cmd, font, hud, (Vector2){{x, y}}, 14.0f, 1.0f, white);
    SituationCmdDrawTextEx(cmd, font, "L", (Vector2){{x, SCOPE_FMA(22.0f, 1.0f, y)}}, 13.0f,
                           1.0f, (ColorRGBA){80, 220, 120, 255});
    SituationCmdDrawTextEx(cmd, font, "R", (Vector2){{x, SCOPE_FMA(38.0f, 1.0f, y)}}, 13.0f,
                           1.0f, (ColorRGBA){110, 180, 255, 255});
}

float sit_test_stereo_scope_last_window_peak(void) {
    return g_scope_snap_peak;
}

float sit_test_stereo_scope_last_fit_peak(void) {
    return g_scope_smoothed_peak;
}

float sit_test_stereo_scope_last_dominant_hz(void) {
    return g_spec_dominant_hz;
}

static bool sit_test_harness_minimal_frame_pump_once(void) {
    if (!SituationIsInitialized()) {
        return false;
    }
    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        return false;
    }
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        SituationEndFrame();
        return false;
    }

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    if (SituationCmdBeginRenderPass(cmd, &rp) != SITUATION_SUCCESS) {
        SituationEndFrame();
        return false;
    }
    if (SituationCmdEndRenderPass(cmd) != SITUATION_SUCCESS) {
        SituationEndFrame();
        return false;
    }
    SituationEndFrame();
    return true;
}

static void sit_test_harness_minimal_pump_ms(uint32_t ms) {
    if (ms == 0u) {
        return;
    }
    const uint32_t slice_ms = 8u;
    const uint32_t frame_interval_ms = SIT_TEST_SCOPE_RENDER_INTERVAL_MS;
    (void)sit_test_harness_minimal_frame_pump_once();
#if defined(_WIN32)
    const DWORD deadline = GetTickCount() + ms;
    DWORD last_frame = GetTickCount();
    while (GetTickCount() < deadline) {
        sit_test_harness_poll_platform_events();
        const DWORD now = GetTickCount();
        if (now - last_frame >= frame_interval_ms) {
            (void)sit_test_harness_minimal_frame_pump_once();
            last_frame = GetTickCount();
        } else if (SituationIsInitialized()) {
            SituationPollInputEvents();
            SituationUpdateTimers();
        }
        if (GetTickCount() >= deadline) {
            break;
        }
        const DWORD left = deadline - GetTickCount();
        const DWORD step = left > slice_ms ? slice_ms : left;
        sit_test_audio_wait_ms(step);
    }
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint64_t deadline_ms =
        (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u + (uint64_t)ms;
    uint64_t last_frame_ms =
        (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    for (;;) {
        sit_test_harness_poll_platform_events();
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const uint64_t now_ms =
            (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
        if (now_ms >= deadline_ms) {
            break;
        }
        if (now_ms - last_frame_ms >= (uint64_t)frame_interval_ms) {
            (void)sit_test_harness_minimal_frame_pump_once();
            last_frame_ms = now_ms;
        } else if (SituationIsInitialized()) {
            SituationPollInputEvents();
            SituationUpdateTimers();
        }
        const uint64_t left = deadline_ms - now_ms;
        const uint32_t step =
            left > (uint64_t)slice_ms ? slice_ms : (uint32_t)(left > 0u ? left : 1u);
        sit_test_audio_wait_ms(step);
    }
#endif
    (void)sit_test_harness_minimal_frame_pump_once();
}

static bool sit_test_audio_visual_render_once(void) {
    if (!SituationIsInitialized()) {
        return false;
    }
    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            return false;
        }
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        SituationEndFrame();
        return false;
    }

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){16, 20, 32, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    if (SituationCmdBeginRenderPass(cmd, &rp) != SITUATION_SUCCESS) {
        SituationEndFrame();
        return false;
    }

    const SitTestVisualLayout layout = sit_test_visual_layout_compute(72.0f);
    const SituationFont default_font = {0};
    sit_test_stereo_scope_frame_prepare();
    const float inner_w = SCOPE_FMA(-2.0f, layout.margin, layout.w);
    sit_test_stereo_scope_draw(cmd, layout.margin, layout.scope_y, inner_w, layout.scope_h);
    sit_test_stereo_scope_draw_spectrum(cmd, layout.margin, layout.spec_y, inner_w,
                                        layout.spec_h);
    sit_test_stereo_scope_draw_hud(cmd, default_font, layout.margin,
                                   sit_test_visual_y(12.0f, layout.scale));
    sit_test_stereo_scope_draw_lines(cmd, layout.margin, layout.scope_y, inner_w,
                                     layout.scope_h, layout.margin, layout.spec_y, inner_w,
                                     layout.spec_h);

    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
    sit_test_stereo_scope_note_ui_frame();
    return true;
}

/** Wall-clock wait; pumps input/timers and refreshes scope/spectrum when Situation is up. */
void sit_test_harness_wait_ms(uint32_t ms) {
    if (SituationIsInitialized()) {
        if (sit_test_harness_visual_enabled()) {
            sit_test_audio_visual_pump_ms(ms);
        } else {
            sit_test_harness_minimal_pump_ms(ms);
        }
    } else {
        sit_test_audio_wait_ms(ms);
    }
}

void sit_test_stereo_scope_service_ui(void) {
    if (!SituationIsInitialized()) {
        return;
    }
    if (sit_test_harness_visual_enabled()) {
        (void)sit_test_audio_visual_render_once();
    } else {
        (void)sit_test_harness_minimal_frame_pump_once();
    }
}

void sit_test_audio_visual_pump_ms(uint32_t ms) {
    if (ms == 0u) {
        return;
    }
    const uint32_t slice_ms = 8u;
    const uint32_t render_interval_ms = SIT_TEST_SCOPE_RENDER_INTERVAL_MS;
    (void)sit_test_audio_visual_render_once();
#if defined(_WIN32)
    const DWORD deadline = GetTickCount() + ms;
    DWORD last_render = GetTickCount();
    while (GetTickCount() < deadline) {
        sit_test_harness_poll_platform_events();
        sit_test_audio_pump_messages();
        const DWORD now = GetTickCount();
        if (now - last_render >= render_interval_ms) {
            (void)sit_test_audio_visual_render_once();
            last_render = GetTickCount();
        }
        if (GetTickCount() >= deadline) {
            break;
        }
        const DWORD left = deadline - GetTickCount();
        const DWORD step = left > slice_ms ? slice_ms : left;
        sit_test_audio_wait_ms(step);
    }
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint64_t deadline_ms =
        (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u + (uint64_t)ms;
    uint64_t last_render_ms =
        (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    for (;;) {
        sit_test_harness_poll_platform_events();
        sit_test_audio_pump_messages();
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const uint64_t now_ms =
            (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
        if (now_ms >= deadline_ms) {
            break;
        }
        if (now_ms - last_render_ms >= (uint64_t)render_interval_ms) {
            (void)sit_test_audio_visual_render_once();
            last_render_ms = now_ms;
        }
        const uint64_t left = deadline_ms - now_ms;
        const uint32_t step =
            left > (uint64_t)slice_ms ? slice_ms : (uint32_t)(left > 0u ? left : 1u);
        sit_test_audio_wait_ms(step);
    }
#endif
    (void)sit_test_audio_visual_render_once();
}

/* ============================================================================
 *  Listen overlay (single TU — shared by audio / tone_synth / effects_heard)
 * ============================================================================ */

static char g_sit_listen_module[96] = "harness";
static char g_sit_listen_test[96] = "(idle)";
static char g_sit_listen_segment[160] = "";
static char g_sit_listen_detail[160] = "";

void sit_test_listen_overlay_set_module(const char* module_name) {
    if (module_name && module_name[0]) {
        snprintf(g_sit_listen_module, sizeof(g_sit_listen_module), "%s", module_name);
    }
}

void sit_test_listen_overlay_set(const char* test_name, const char* segment, const char* detail) {
    if (test_name && test_name[0]) {
        snprintf(g_sit_listen_test, sizeof(g_sit_listen_test), "%s", test_name);
    }
    snprintf(g_sit_listen_segment, sizeof(g_sit_listen_segment), "%s", segment ? segment : "");
    snprintf(g_sit_listen_detail, sizeof(g_sit_listen_detail), "%s", detail ? detail : "");
    sit_test_stereo_scope_reset_spectrum();

    if (SituationIsInitialized()) {
        SituationSetWindowTitle(g_sit_listen_test);
        SituationPollInputEvents();
        SituationUpdateTimers();
    }
}

#if defined(_WIN32)
void sit_test_listen_overlay_wait_ms(uint32_t ms) {
    const DWORD deadline = GetTickCount() + ms;
    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (SituationIsInitialized()) {
            SituationPollInputEvents();
            SituationUpdateTimers();
        }
        DWORD now = GetTickCount();
        if (now >= deadline) {
            break;
        }
        DWORD left = deadline - now;
        DWORD slice = left > 8u ? 8u : left;
        if (slice == 0u) {
            slice = 1u;
        }
        (void)MsgWaitForMultipleObjectsEx(0, NULL, slice, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}
#else
void sit_test_listen_overlay_wait_ms(uint32_t ms) {
    usleep((unsigned int)ms * 1000u);
}
#endif

bool sit_test_listen_overlay_render_once(void) {
    if (!SituationIsInitialized()) {
        return false;
    }
    if (!sit_test_harness_visual_enabled()) {
        return sit_test_harness_minimal_frame_pump_once();
    }

    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            return false;
        }
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        SituationEndFrame();
        return false;
    }

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){16, 20, 32, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    if (SituationCmdBeginRenderPass(cmd, &rp) != SITUATION_SUCCESS) {
        SituationEndFrame();
        return false;
    }

    sit_test_stereo_scope_frame_prepare();

    const SituationFont font = {0};
    const ColorRGBA white = {235, 235, 245, 255};
    const ColorRGBA cyan = {120, 210, 255, 255};
    const ColorRGBA dim = {150, 155, 170, 255};
    const SitTestVisualLayout layout = sit_test_visual_layout_compute(100.0f);
    const float s = layout.scale;
    const float panel_w = SCOPE_FMA(-2.0f, layout.margin, layout.w);
    const float label_l_x = SCOPE_FSUB(layout.w, SCOPE_FMUL(36.0f, s));
    const float label_l_y = SCOPE_FMA(8.0f, s, layout.scope_y);
    const float label_r_y = SCOPE_FMA(8.0f, s, SCOPE_FMA(0.5f, layout.scope_h, layout.scope_y));
    const float text_18 = SCOPE_FMUL(18.0f, s);
    const float text_16 = SCOPE_FMUL(16.0f, s);
    const float text_14 = SCOPE_FMUL(14.0f, s);
    const float text_13 = SCOPE_FMUL(13.0f, s);
    const float text_12 = SCOPE_FMUL(12.0f, s);

    char line0[128];
    snprintf(line0, sizeof(line0), "%s / %s", g_sit_listen_module, g_sit_listen_test);
    SituationCmdDrawTextEx(cmd, font, line0, (Vector2){{layout.margin, sit_test_visual_y(8.0f, s)}},
                           text_18, 1.0f, cyan);

    if (g_sit_listen_segment[0]) {
        SituationCmdDrawTextEx(cmd, font, g_sit_listen_segment,
                               (Vector2){{layout.margin, sit_test_visual_y(32.0f, s)}}, text_16,
                               1.0f, white);
    }
    if (g_sit_listen_detail[0]) {
        SituationCmdDrawTextEx(cmd, font, g_sit_listen_detail,
                               (Vector2){{layout.margin, sit_test_visual_y(54.0f, s)}}, text_14,
                               1.0f, dim);
    }

    sit_test_stereo_scope_draw(cmd, layout.margin, layout.scope_y, panel_w, layout.scope_h);
    sit_test_stereo_scope_draw_spectrum(cmd, layout.margin, layout.spec_y, panel_w, layout.spec_h);

    char meter[256];
    sit_test_stereo_scope_format_bus_meter(meter, sizeof(meter));
    SituationCmdDrawTextEx(cmd, font, meter,
                           (Vector2){{layout.margin, sit_test_visual_y(76.0f, s)}}, text_13,
                           1.0f, dim);

    SituationCmdDrawTextEx(cmd, font, "L",
                           (Vector2){{label_l_x, label_l_y}}, text_12,
                           1.0f, (ColorRGBA){80, 220, 120, 255});
    SituationCmdDrawTextEx(cmd, font, "R",
                           (Vector2){{label_l_x, label_r_y}},
                           text_12, 1.0f, (ColorRGBA){110, 180, 255, 255});

    sit_test_stereo_scope_draw_lines(cmd, layout.margin, layout.scope_y, panel_w, layout.scope_h,
                                     layout.margin, layout.spec_y, panel_w, layout.spec_h);

    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
    sit_test_stereo_scope_note_ui_frame();
    return true;
}

void sit_test_listen_overlay_pump_ms(uint32_t ms) {
    if (!sit_test_harness_visual_enabled()) {
        sit_test_harness_wait_ms(ms);
        return;
    }
    const uint32_t slice_ms = 8u;
    const uint32_t render_interval_ms = SIT_TEST_SCOPE_RENDER_INTERVAL_MS;
    (void)sit_test_listen_overlay_render_once();
#if defined(_WIN32)
    const DWORD deadline = GetTickCount() + ms;
    DWORD last_render = GetTickCount();
    while (GetTickCount() < deadline) {
        const DWORD now = GetTickCount();
        if (now - last_render >= render_interval_ms) {
            (void)sit_test_listen_overlay_render_once();
            last_render = GetTickCount();
        } else if (SituationIsInitialized()) {
            SituationPollInputEvents();
            SituationUpdateTimers();
        }
        const DWORD left = deadline - now;
        if (left == 0u) {
            break;
        }
        const DWORD step = left > slice_ms ? slice_ms : left;
        sit_test_listen_overlay_wait_ms(step);
    }
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint64_t deadline_ms =
        (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u + (uint64_t)ms;
    uint64_t last_render_ms =
        (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const uint64_t now_ms =
            (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
        if (now_ms >= deadline_ms) {
            break;
        }
        if (now_ms - last_render_ms >= (uint64_t)render_interval_ms) {
            (void)sit_test_listen_overlay_render_once();
            last_render_ms = now_ms;
        } else if (SituationIsInitialized()) {
            SituationPollInputEvents();
            SituationUpdateTimers();
        }
        const uint64_t left = deadline_ms - now_ms;
        const uint32_t step =
            left > (uint64_t)slice_ms ? slice_ms : (uint32_t)(left > 0u ? left : 1u);
        sit_test_listen_overlay_wait_ms(step);
    }
#endif
    (void)sit_test_listen_overlay_render_once();
}

bool sit_test_listen_overlay_init(void) {
    sit_test_listen_overlay_set("(starting)", "", "Stereo scope on master bus");
    return sit_test_listen_overlay_render_once();
}

void sit_test_listen_overlay_shutdown(void) {
    /* scope monitor torn down in module teardown */
}

void sit_test_harness_visual_begin(const char* module_name) {
    if (!sit_test_harness_visual_enabled() || !SituationIsInitialized()) {
        return;
    }
    sit_test_listen_overlay_set_module(module_name);
    (void)sit_test_listen_overlay_init();
}

void sit_test_harness_visual_end(void) {
    if (!sit_test_harness_visual_enabled()) {
        return;
    }
    sit_test_listen_overlay_shutdown();
}

void sit_test_harness_visual_on_test_start(const char* test_name) {
    if (!sit_test_harness_visual_enabled() || !SituationIsInitialized()) {
        return;
    }
    sit_test_listen_overlay_set(test_name, "", "Stereo scope on master bus");
    (void)sit_test_listen_overlay_render_once();
}

void sit_test_harness_visual_on_test_end(void) {
    if (!sit_test_harness_visual_enabled() || !SituationIsInitialized()) {
        return;
    }
    (void)sit_test_listen_overlay_render_once();
}
