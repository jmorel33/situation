/**
 * @file midi_test_info.h
 * @brief Harness MIDI routing labels — channel, PortMidi device names, synth target.
 *
 * Graph tone synth tests use:
 *   - Input: SITUATION_VIRTUAL_MIDI_IN_NAME (virtual loopback)
 *   - Channel: SITUATION_TEST_MIDI_CHANNEL (0-based → display as MIDI channel 1)
 *   - Note on/off: SituationVirtualMidiNoteOnEx/OffEx with SITUATION_TEST_MIDI_CHANNEL
 *   - Target: graph SITUATION_NODE_TONE_SYNTH / SITUATION_TONE_SYNTH_MIDI_DEVICE_NAME
 *
 * Legacy tone pool tests (legacy_tone_pool_*) use SituationPlayMidiNote — no PortMidi path.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#ifndef SIT_MIDI_TEST_INFO_H
#define SIT_MIDI_TEST_INFO_H

#include "sit_test_framework.h"
#include "sit_api_include.h"
#include <stdio.h>
#include <string.h>

/**
 * Assert and log MIDI route for a graph tone synth harness test.
 * Uses SituationGetMidiDeviceName (PortMidi) and registry synth name.
 *
 * @param midi_usage Short description of channel/CC usage for this test (may be NULL).
 */
static inline void sit_midi_log_graph_tone_synth_route(const char* test_name,
                                                       int midi_in_device_id,
                                                       const char* midi_usage) {
    char in_name[128] = {0};
    SituationDeviceMetadata meta = {0};

    SIT_ASSERT_EQ(SituationGetMidiDeviceName(midi_in_device_id, in_name, sizeof(in_name)),
                  SITUATION_SUCCESS);
    SIT_ASSERT(strcmp(in_name, SITUATION_VIRTUAL_MIDI_IN_NAME) == 0);

    SIT_ASSERT_EQ(SituationGetDeviceMetadata(SITUATION_NODE_TONE_SYNTH, &meta), SITUATION_SUCCESS);
    SIT_ASSERT(strcmp(meta.name, SITUATION_TONE_SYNTH_MIDI_DEVICE_NAME) == 0);

    printf("  [MIDI] %s: in=\"%s\" (id=%d) synth=\"%s\" ch=%d (MIDI ch %d) | %s\n",
           test_name ? test_name : "graph_tone_synth",
           in_name,
           midi_in_device_id,
           meta.name,
           SITUATION_TEST_MIDI_CHANNEL,
           SITUATION_TEST_MIDI_CHANNEL + 1,
           midi_usage ? midi_usage : "note/CC on test channel");
    fflush(stdout);
}

/** Legacy 64-voice pool: no PortMidi device; direct API only. */
static inline void sit_midi_log_legacy_tone_pool_route(const char* test_name) {
    printf("  [MIDI] %s: path=legacy_tone_pool (SituationPlayMidiNote) — no PortMidi device | "
           "note params only, no MIDI channel routing\n",
           test_name ? test_name : "legacy_tone_pool");
    fflush(stdout);
}

/** Phase 1 side-by-side: log measured A4 stats from legacy vs graph captures. */
static inline void sit_midi_log_phase1_compare(const char* label,
                                               float legacy_hz, float legacy_peak, float legacy_rms,
                                               float graph_hz, float graph_peak, float graph_rms) {
    printf("  [COMPARE Phase 1] %s\n", label ? label : "A4 (midi note 69)");
    printf("    legacy: hz=%.2f peak=%.4f rms=%.4f  (PlayMidiNote, graph inactive)\n",
           legacy_hz, legacy_peak, legacy_rms);
    printf("    graph:  hz=%.2f peak=%.4f rms=%.4f  (VirtualMidiNoteOnEx, legacy silent)\n",
           graph_hz, graph_peak, graph_rms);
    printf("    delta:  hz=%+.2f peak=%+.4f rms=%+.4f\n",
           graph_hz - legacy_hz, graph_peak - legacy_peak, graph_rms - legacy_rms);
    fflush(stdout);
}

#endif /* SIT_MIDI_TEST_INFO_H */
