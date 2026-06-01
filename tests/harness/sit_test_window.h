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

static inline void sit_test_window_init_info(SituationInitInfo* config, const char* title) {
    if (!config) {
        return;
    }
    *config = (SituationInitInfo){0};
    config->window_width = SIT_TEST_WINDOW_WIDTH;
    config->window_height = SIT_TEST_WINDOW_HEIGHT;
    config->window_title = title;
    config->initial_active_window_flags = 0;
    if (sit_test_window_headless_active()) {
        config->initial_active_window_flags |= SITUATION_FLAG_WINDOW_HIDDEN;
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
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, title);
    return SituationInit(0, NULL, &config);
#else
    (void)title;
    return SITUATION_ERROR_INIT_FAILED;
#endif
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

static inline SitRectangle sit_test_full_window_dest(void) {
    SitRectangle r = {0.0f, 0.0f, (float)sit_test_window_width(), (float)sit_test_window_height()};
    return r;
}

#endif /* SIT_TEST_WINDOW_H */
