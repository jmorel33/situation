/**
 * @file sit_test_listen_overlay.h
 * @brief On-window labels + stereo scope for audible harness tests.
 *
 * Uses Situation default font (zeroed SituationFont) and
 * SituationSetAudioOutputMonitor via sit_test_stereo_scope.h.
 */

#ifndef SIT_TEST_LISTEN_OVERLAY_H
#define SIT_TEST_LISTEN_OVERLAY_H

#include <stdbool.h>
#include <stdint.h>

void sit_test_listen_overlay_set_module(const char* module_name);
void sit_test_listen_overlay_set(const char* test_name, const char* segment, const char* detail);
bool sit_test_listen_overlay_render_once(void);
void sit_test_listen_overlay_pump_ms(uint32_t ms);
void sit_test_listen_overlay_wait_ms(uint32_t ms);
bool sit_test_listen_overlay_init(void);
void sit_test_listen_overlay_shutdown(void);

#endif /* SIT_TEST_LISTEN_OVERLAY_H */
