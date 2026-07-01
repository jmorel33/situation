/**
 * @file sit_test_headless.h
 * @brief Global harness headless flag (OpenGL and Vulkan — hidden window, no overlay).
 *
 * Set by sit_test_init() from --headless or SIT_TEST_HEADLESS; defaults to false.
 */

#ifndef SIT_TEST_HEADLESS_H
#define SIT_TEST_HEADLESS_H

#include <stdbool.h>

extern bool g_sit_test_headless;

static inline bool sit_test_headless(void) {
    return g_sit_test_headless;
}

static inline bool sit_test_harness_visual_enabled(void) {
    return !g_sit_test_headless;
}

#endif /* SIT_TEST_HEADLESS_H */
