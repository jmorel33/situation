/**
 * @file sit_test_stereo_scope.h
 * @brief Stereo oscilloscope ring buffer for harness tests (SituationSetAudioOutputMonitor).
 */

#ifndef SIT_TEST_STEREO_SCOPE_H
#define SIT_TEST_STEREO_SCOPE_H

#include "sit_api_include.h"
#include "audio_freq_detect.h"
#include <stdint.h>

#ifndef SIT_TEST_SCOPE_CAPACITY
#define SIT_TEST_SCOPE_CAPACITY 16384u
#endif

/** Max waveform columns drawn per frame (full width is resampled to this many segments). */
#ifndef SIT_TEST_SCOPE_DRAW_COLUMNS
#define SIT_TEST_SCOPE_DRAW_COLUMNS 160u
#endif

/** Minimum ms between log-spectrum Goertzel passes (scope waveform updates every render). */
#ifndef SIT_TEST_SCOPE_RENDER_INTERVAL_MS
#define SIT_TEST_SCOPE_RENDER_INTERVAL_MS 33u
#endif
/** Recompute log-spectrum at most this often (match overlay refresh; 0 = every frame_prepare). */
#ifndef SIT_TEST_SPECTRUM_UPDATE_MS
#define SIT_TEST_SPECTRUM_UPDATE_MS 33u
#endif

/** Per-update falloff when bin level drops (fast release — no multi-second peak trails). */
#ifndef SIT_TEST_SPECTRUM_ATTACK
#define SIT_TEST_SPECTRUM_ATTACK 0.55f
#endif

#ifndef SIT_TEST_SPECTRUM_RELEASE
#define SIT_TEST_SPECTRUM_RELEASE 0.78f
#endif

/** Display floor in dB below the current frame peak (shows harmonics / sidebands). */
#ifndef SIT_TEST_SPECTRUM_DB_FLOOR
#define SIT_TEST_SPECTRUM_DB_FLOOR (-42.0f)
#endif

/** Slow decay on the display reference peak (clears stale headroom after silence). */
#ifndef SIT_TEST_SPECTRUM_DISPLAY_PEAK_DECAY
#define SIT_TEST_SPECTRUM_DISPLAY_PEAK_DECAY 0.94f
#endif

/** Spectrum panel height (bottom of window). */
#ifndef SIT_TEST_SPECTRUM_PANEL_H
#define SIT_TEST_SPECTRUM_PANEL_H 132.0f
#endif

#ifndef SIT_TEST_SPECTRUM_BARS
#define SIT_TEST_SPECTRUM_BARS 56u
#endif

/** Oscilloscope timebase: show this many cycles at SIT_TEST_SCOPE_TUNED_HZ across the panel. */
#ifndef SIT_TEST_SCOPE_TUNED_HZ
#define SIT_TEST_SCOPE_TUNED_HZ 1000.0f
#endif
#ifndef SIT_TEST_SCOPE_CYCLES
#define SIT_TEST_SCOPE_CYCLES 5.0f
#endif

#ifndef SIT_TEST_SPECTRUM_MIN_HZ
#define SIT_TEST_SPECTRUM_MIN_HZ 40.0f
#endif

#ifndef SIT_TEST_SPECTRUM_MAX_HZ
#define SIT_TEST_SPECTRUM_MAX_HZ 12000.0f
#endif

void sit_test_stereo_scope_init(void);
void sit_test_stereo_scope_reset(void);
/** Clear spectrum bar state only (listen segment changes, capture arm). */
void sit_test_stereo_scope_reset_spectrum(void);

/** Once per overlay frame: snapshot ring + throttled spectrum (call before any draw_*). */
void sit_test_stereo_scope_frame_prepare(void);

/** Push interleaved stereo (or mono duplicated) samples from the audio thread. */
void sit_test_stereo_scope_push(const float* samples, uint32_t frame_count, uint32_t channels);

/** Monitor callback: always feeds scope; optionally feeds active Goertzel capture. */
void sit_test_audio_monitor_cb(const float* samples, uint32_t frame_count, void* user_data);

/** Set capture target for the chained monitor (NULL = scope only). */
void sit_test_audio_monitor_set_capture(SitAudioFreqCapture* cap);

/**
 * Arm Goertzel capture for a listen segment: clear scope ring, attach capture,
 * brief pump so the live monitor feed animates (display always uses the ring).
 */
void sit_test_audio_monitor_arm_capture(SitAudioFreqCapture* cap);

/** Install scope monitor on the Situation master bus (call once from module setup). */
void sit_test_audio_monitor_install(void);

void sit_test_audio_monitor_uninstall(void);

/** Panel background + snapshot (internal 2D quads only). */
void sit_test_stereo_scope_draw(SituationCommandBuffer cmd, float x, float y, float w, float h);

/** Spectrum panel background + labels (internal 2D; call after scope draw). */
void sit_test_stereo_scope_draw_spectrum(SituationCommandBuffer cmd, float x, float y, float w,
                                         float h);

/** Waveform/grid/spectrum traces (internal 2D line quads). */
void sit_test_stereo_scope_draw_lines(SituationCommandBuffer cmd, float scope_x, float scope_y,
                                      float scope_w, float scope_h, float spec_x, float spec_y,
                                      float spec_w, float spec_h);

/** Y coordinate for the spectrum panel (pass SituationGetRenderHeight()). */
static inline float sit_test_stereo_scope_spectrum_y(float render_h) {
    if (render_h < 1.0f) {
        render_h = (float)SituationGetRenderHeight();
    }
    return render_h - SIT_TEST_SPECTRUM_PANEL_H - 8.0f;
}

/** Master peak/RMS + scope legend (uses Situation default font when font atlas is zero). */
void sit_test_stereo_scope_draw_hud(SituationCommandBuffer cmd, SituationFont font, float x,
                                    float y);

/** Wall-clock wait; pumps input/timers and refreshes scope/spectrum when Situation is initialized. */
void sit_test_harness_wait_ms(uint32_t ms);

/** Win32 message dispatch (no-op elsewhere). Call before poll/render in harness pumps. */
void sit_test_harness_poll_platform_events(void);

/** Poll/render scope frame; keeps window responsive during waits. */
void sit_test_audio_visual_pump_ms(uint32_t ms);

/** One overlay frame (scope + spectrum) when Situation is up; use during CPU-only analysis. */
void sit_test_stereo_scope_service_ui(void);

/** Peak |sample| in the last scope snapshot (for HUD). */
float sit_test_stereo_scope_last_window_peak(void);

/** Smoothed ± reference used for auto-fit scaling (for HUD). */
float sit_test_stereo_scope_last_fit_peak(void);

/** Dominant frequency (Hz) from the last spectrum analysis, or 0 if none. */
float sit_test_stereo_scope_last_dominant_hz(void);

/** Call after each successful harness scope/spectrum EndFrame (rolling UI refresh rate). */
void sit_test_stereo_scope_note_ui_frame(void);

/** Smoothed scope/spectrum overlay FPS (harness presents per second, not Situation target FPS). */
float sit_test_stereo_scope_hud_fps(void);

/** Fill HUD meter line: bus peak/rms, scope_fps, dominant Hz or scope window peak. */
void sit_test_stereo_scope_format_bus_meter(char* buf, size_t buf_size);

/** Begin module-level listen overlay (scope + labels); call after SituationInit + monitor install. */
void sit_test_harness_visual_begin(const char* module_name);

/** End listen overlay for this module; call before SituationShutdown. */
void sit_test_harness_visual_end(void);

/** Update overlay title and present one frame before a harness test runs. */
void sit_test_harness_visual_on_test_start(const char* test_name);

/** Present one frame after a harness test completes. */
void sit_test_harness_visual_on_test_end(void);

#endif /* SIT_TEST_STEREO_SCOPE_H */
