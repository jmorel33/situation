/**
 * @file sit_test_window.h
 * @brief Default harness window size (1024×768 windowed) and runtime layout helpers.
 */

#ifndef SIT_TEST_WINDOW_H
#define SIT_TEST_WINDOW_H

#include "sit_api_include.h"
#include "sit_test_headless.h"
#include <stdlib.h>
#ifdef _WIN32
#include <string.h>
#else
#include <strings.h>
#endif

#define SIT_TEST_WINDOW_WIDTH  1024
#define SIT_TEST_WINDOW_HEIGHT 768

/** Headless for init_info: runtime flag (harness) or SIT_TEST_HEADLESS env (standalone tools). */
static inline bool sit_test_window_headless_active(void) {
    if (sit_test_headless()) {
        return true;
    }
    const char* env = getenv("SIT_TEST_HEADLESS");
    if (!env || !env[0]) {
        return false;
    }
#ifdef _WIN32
    return _stricmp(env, "0") != 0 && _stricmp(env, "false") != 0 && _stricmp(env, "no") != 0;
#else
    return strcasecmp(env, "0") != 0 && strcasecmp(env, "false") != 0 && strcasecmp(env, "no") != 0;
#endif
}

/** Opt-in HDR10 swapchain verification (set SIT_TEST_HDR=1). Off by default for CI. */
static inline bool sit_test_hdr_enabled(void) {
    const char* env = getenv("SIT_TEST_HDR");
    if (!env || !env[0]) {
        return false;
    }
#ifdef _WIN32
    return _stricmp(env, "0") != 0 && _stricmp(env, "false") != 0 && _stricmp(env, "no") != 0;
#else
    return strcasecmp(env, "0") != 0 && strcasecmp(env, "false") != 0 && strcasecmp(env, "no") != 0;
#endif
}

/** Opt-in 5s on-screen HDR PQ grading bands (set SIT_TEST_HDR=1 and SIT_TEST_HDR_VISUAL=1). */
static inline bool sit_test_hdr_visual_enabled(void) {
    const char* env = getenv("SIT_TEST_HDR_VISUAL");
    if (!env || !env[0]) {
        return false;
    }
#ifdef _WIN32
    return _stricmp(env, "0") != 0 && _stricmp(env, "false") != 0 && _stricmp(env, "no") != 0;
#else
    return strcasecmp(env, "0") != 0 && strcasecmp(env, "false") != 0 && strcasecmp(env, "no") != 0;
#endif
}

/** Opt-in hardware 10-bit verification (set SIT_TEST_10BIT=1). Off by default for CI. */
static inline bool sit_test_10bit_enabled(void) {
    const char* env = getenv("SIT_TEST_10BIT");
    if (!env || !env[0]) {
        return false;
    }
#ifdef _WIN32
    return _stricmp(env, "0") != 0 && _stricmp(env, "false") != 0 && _stricmp(env, "no") != 0;
#else
    return strcasecmp(env, "0") != 0 && strcasecmp(env, "false") != 0 && strcasecmp(env, "no") != 0;
#endif
}

/** Opt-in 5s on-screen 10-bit grading bands (set SIT_TEST_10BIT_VISUAL=1). */
static inline bool sit_test_10bit_visual_enabled(void) {
    const char* env = getenv("SIT_TEST_10BIT_VISUAL");
    if (!env || !env[0]) {
        return false;
    }
#ifdef _WIN32
    return _stricmp(env, "0") != 0 && _stricmp(env, "false") != 0 && _stricmp(env, "no") != 0;
#else
    return strcasecmp(env, "0") != 0 && strcasecmp(env, "false") != 0 && strcasecmp(env, "no") != 0;
#endif
}

static inline void sit_test_window_init_info(SituationInitInfo* config, const char* title) {
    if (!config) {
        return;
    }
    *config = (SituationInitInfo){0};
    config->window_width = SIT_TEST_WINDOW_WIDTH;
    config->window_height = SIT_TEST_WINDOW_HEIGHT;
    config->window_title = title;
    config->main_thread_name = title;
    config->output_color_depth = SIT_OUTPUT_COLOR_8BIT;
    config->initial_active_window_flags = 0;
    if (sit_test_window_headless_active()) {
        config->initial_active_window_flags |= SITUATION_FLAG_WINDOW_HIDDEN;
    }
}

/** Init info with AUTO/10-bit request (still fail-soft to 8-bit when unsupported). */
static inline void sit_test_window_init_info_10bit(SituationInitInfo* config, const char* title) {
    sit_test_window_init_info(config, title);
    if (config) {
        config->output_color_depth = SIT_OUTPUT_COLOR_AUTO;
    }
}

/** Init info requesting HDR10 swapchain (fail-soft to 10-bit SDR / 8-bit). */
static inline void sit_test_window_init_info_hdr10(SituationInitInfo* config, const char* title) {
    sit_test_window_init_info(config, title);
    if (config) {
        config->output_color_depth = SIT_OUTPUT_COLOR_HDR10;
    }
}

static inline void sit_test_window_init_info_flags(
    SituationInitInfo* config,
    const char* title,
    uint32_t window_flags)
{
    sit_test_window_init_info(config, title);
    if (config) {
        config->initial_active_window_flags |= window_flags;
    }
}

/** One-shot GPU context for tests outside module setup (e.g. misc YPQ sweeps). */
static inline SituationError sit_test_gpu_context_init(const char* title) {
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    if (SituationIsInitialized()) {
        return SITUATION_SUCCESS;
    }
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, title);
    SituationError err = SituationInit(0, NULL, &config);
    if (err == SITUATION_SUCCESS) {
        SituationSetCurrentThreadName(config.main_thread_name ? config.main_thread_name : title);
    }
    if (err != SITUATION_SUCCESS) {
        char* err_msg = NULL;
        if (SituationGetLastErrorMsg(&err_msg) == SITUATION_SUCCESS && err_msg && err_msg[0]) {
            fprintf(stderr, "[harness] SituationInit failed: %d — %s\n", (int)err, err_msg);
        }
    }
    return err;
#else
    (void)title;
    return SITUATION_ERROR_INIT_FAILED;
#endif
}

/** Acquire frame with swapchain-recreation retries (visible-window harness). */
static inline SituationError sit_test_acquire_frame(void) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    for (int attempt = 0; attempt < 6; ++attempt) {
        SituationError err = SituationAcquireFrameCommandBuffer();
        if (err == SITUATION_SUCCESS) {
            return SITUATION_SUCCESS;
        }
        SituationPollInputEvents();
        SituationUpdateTimers();
    }
    return SituationAcquireFrameCommandBuffer();
}

static inline void sit_test_gpu_context_shutdown(void) {
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    if (SituationIsInitialized()) {
        SituationShutdown();
    }
#endif
}

/** Logical client area (DrawTexture dest, UI layout). Falls back to defaults before init. */
static inline int sit_test_window_width(void) {
    int w = SituationGetScreenWidth();
    return w > 0 ? w : SIT_TEST_WINDOW_WIDTH;
}

static inline int sit_test_window_height(void) {
    int h = SituationGetScreenHeight();
    return h > 0 ? h : SIT_TEST_WINDOW_HEIGHT;
}

static inline int sit_test_window_render_width(void) {
    int w = SituationGetRenderWidth();
    return w > 0 ? w : SIT_TEST_WINDOW_WIDTH;
}

static inline int sit_test_window_render_height(void) {
    int h = SituationGetRenderHeight();
    return h > 0 ? h : SIT_TEST_WINDOW_HEIGHT;
}

/** Restore harness-default resizable limits after a forced-size visual test. */
static inline void sit_test_window_restore_default_size_limits(void) {
    SituationSetWindowMinSize(1, 1);
    SituationSetWindowMaxSize(8192, 8192);
}

/**
 * Lock the **framebuffer** (SituationGetRenderWidth/Height) to an exact pixel size.
 * SituationSetWindowSize sets logical client units; on Hi-DPI the backbuffer is client × scale.
 * This helper derives client size from content scale and refines until render pixels match.
 */
static inline bool sit_test_force_render_resolution(int fb_width, int fb_height, int max_attempts) {
    if (!SituationIsInitialized() || fb_width <= 0 || fb_height <= 0) {
        return false;
    }
    if (max_attempts < 1) {
        max_attempts = 1;
    }

    SituationRestoreWindow();
    SituationPollInputEvents();
    SituationSetWindowState(SITUATION_FLAG_WINDOW_RESIZABLE);
    SituationClearWindowState(SITUATION_FLAG_WINDOW_MAXIMIZED | SITUATION_FLAG_WINDOW_MINIMIZED |
        SITUATION_FLAG_FULLSCREEN_MODE);

    Vector2 scale = SituationGetWindowScaleDPI();
    if (scale.x < 0.01f) {
        scale.x = 1.0f;
    }
    if (scale.y < 0.01f) {
        scale.y = 1.0f;
    }

    int client_w = (int)((float)fb_width / scale.x + 0.5f);
    int client_h = (int)((float)fb_height / scale.y + 0.5f);
    if (client_w < 1) {
        client_w = 1;
    }
    if (client_h < 1) {
        client_h = 1;
    }

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        SituationSetWindowMinSize(client_w, client_h);
        SituationSetWindowMaxSize(client_w, client_h);
        SituationSetWindowSize(client_w, client_h);
        SituationSetWindowFocused();
        SituationPollInputEvents();

        SituationError err = sit_test_acquire_frame();
        if (err == SITUATION_SUCCESS) {
            (void)SituationEndFrame();
        }
        SituationPollInputEvents();

        const int render_w = SituationGetRenderWidth();
        const int render_h = SituationGetRenderHeight();
        if (render_w == fb_width && render_h == fb_height) {
            fprintf(stderr,
                "[harness] locked framebuffer %dx%d (client %dx%d, scale %.2fx%.2f)\n",
                render_w,
                render_h,
                SituationGetScreenWidth(),
                SituationGetScreenHeight(),
                scale.x,
                scale.y);
            return true;
        }

        const int screen_w = SituationGetScreenWidth();
        const int screen_h = SituationGetScreenHeight();
        if (screen_w > 0 && screen_h > 0 && render_w > 0 && render_h > 0) {
            client_w = (int)((float)fb_width * (float)screen_w / (float)render_w + 0.5f);
            client_h = (int)((float)fb_height * (float)screen_h / (float)render_h + 0.5f);
            if (client_w < 1) {
                client_w = 1;
            }
            if (client_h < 1) {
                client_h = 1;
            }
        }
        scale = SituationGetWindowScaleDPI();
        if (scale.x < 0.01f) {
            scale.x = 1.0f;
        }
        if (scale.y < 0.01f) {
            scale.y = 1.0f;
        }
    }

    fprintf(stderr,
        "[harness] framebuffer lock failed: want %dx%d, got render %dx%d client %dx%d scale %.2fx%.2f\n",
        fb_width,
        fb_height,
        SituationGetRenderWidth(),
        SituationGetRenderHeight(),
        SituationGetScreenWidth(),
        SituationGetScreenHeight(),
        scale.x,
        scale.y);
    return false;
}

static inline SitRectangle sit_test_full_window_dest(void) {
    /* DrawTexture dest must match render-pass projection (SituationGetRender*), not client
     * screen size — mismatch skews UV at readback center after long module runs (Bug B). */
    SitRectangle r = {0.0f, 0.0f, (float)sit_test_window_render_width(), (float)sit_test_window_render_height()};
    return r;
}

/** Aspect-fit dest rect centered in the window (letterbox/pillarbox). */
static inline SitRectangle sit_test_fit_content_dest(float content_w, float content_h) {
    float win_w = (float)sit_test_window_width();
    float win_h = (float)sit_test_window_height();
    if (content_w <= 0.0f) {
        content_w = 1.0f;
    }
    if (content_h <= 0.0f) {
        content_h = 1.0f;
    }
    const float sx = win_w / content_w;
    const float sy = win_h / content_h;
    const float s = sx < sy ? sx : sy;
    const float fit_w = content_w * s;
    const float fit_h = content_h * s;
    SitRectangle r = {
        (win_w - fit_w) * 0.5f,
        (win_h - fit_h) * 0.5f,
        fit_w,
        fit_h
    };
    return r;
}

#endif /* SIT_TEST_WINDOW_H */
