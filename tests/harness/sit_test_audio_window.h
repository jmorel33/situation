/**
 * @file sit_test_audio_window.h
 * @brief Back-compat alias for audible harness tests — see sit_test_window.h.
 */

#ifndef SIT_TEST_AUDIO_WINDOW_H
#define SIT_TEST_AUDIO_WINDOW_H

#include "sit_test_window.h"

#define SIT_TEST_AUDIO_WINDOW_WIDTH  SIT_TEST_WINDOW_WIDTH
#define SIT_TEST_AUDIO_WINDOW_HEIGHT SIT_TEST_WINDOW_HEIGHT

static inline void sit_test_audio_window_init_info(SituationInitInfo* config, const char* title) {
    sit_test_window_init_info(config, title);
}

#endif /* SIT_TEST_AUDIO_WINDOW_H */
