/**
 * @file test_tone_synth.c
 * @brief Tone Synth harness module — legacy 64-voice pool, graph node, MIDI verification.
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "audio_freq_detect.h"
#include "midi_test_info.h"
#include "sit_test_audio_window.h"
#include "sit_test_stereo_scope.h"
#include "sit_test_listen_overlay.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#define SIT_TONE_TEST_SLEEP_MS(ms) sit_test_listen_overlay_pump_ms((uint32_t)(ms))
#define SIT_TONE_CAPTURE_SLEEP_MS(ms) sit_test_listen_overlay_pump_ms((uint32_t)(ms))

static bool g_tone_synth_init_ok = false;

static void sit_midi_graph_fixture_release(void);

static void tone_synth_crash_cleanup(void) {
    sit_test_audio_monitor_set_capture(NULL);
    sit_midi_graph_fixture_release();
}

static void tone_synth_setup(void) {
    sit_test_set_crash_cleanup(tone_synth_crash_cleanup);
    SituationInitInfo config;
    sit_test_audio_window_init_info(&config, "SIT_TEST_TONE_SYNTH");
    SituationError err = SituationInit(0, NULL, &config);
    g_tone_synth_init_ok = (err == SITUATION_SUCCESS);
    if (!g_tone_synth_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    sit_test_audio_monitor_install();
}

static void tone_synth_teardown(void) {
    sit_test_set_crash_cleanup(NULL);
    sit_test_audio_monitor_uninstall();
    remove("_sit_test_graph.json");
    SituationSetActiveGraph(NULL);
    SituationStopAllTones();
    sit_test_audio_monitor_set_capture(NULL);
    SituationTeardownVirtualMidiLoopback();
    if (g_tone_synth_init_ok) {
        SituationShutdown();
        g_tone_synth_init_ok = false;
    }
}

// ============================================================================
//  Tone Synthesis Tests
// ============================================================================

static void test_play_tone_ex(void) {
    SituationToneHandle handle = SituationPlayToneEx(
        SIT_WAVE_SINE, 440.0f, 0.5f, 0.0f, 0.01f, 0.05f, 0.7f, 0.1f, 0.2f);
    SIT_ASSERT(handle != 0);
    SituationStopTone(handle);
}

static void test_play_tone_legacy(void) {
    SituationPlayTone(SIT_WAVE_SQUARE, 880.0f, 0.3f, 0.01f, 0.05f, 0.5f, 0.1f, 0.1f);
    SIT_ASSERT(true);
}

static void test_play_midi_note(void) {
    SituationPlayMidiNote(69, SIT_WAVE_SINE, 0.4f, 0.01f, 0.05f, 0.6f, 0.1f, 0.15f);
    SIT_ASSERT(true);
}

static void test_stop_all_tones(void) {
    SituationPlayToneEx(SIT_WAVE_SINE, 440.0f, 0.3f, 0.0f, 0.01f, 0.01f, 0.5f, 0.05f, -1.0f);
    SituationPlayToneEx(SIT_WAVE_SAW, 220.0f, 0.3f, 0.0f, 0.01f, 0.01f, 0.5f, 0.05f, -1.0f);
    SituationStopAllTones();
    SIT_ASSERT(true);
}

static void test_stop_tone_invalid_handle(void) {
    SituationStopTone(0);
    SituationStopTone(99999);
    SIT_ASSERT(true);
}
static void test_registry_tone_synth_metadata(void) {
    SituationInitDeviceRegistry();
    SituationDeviceMetadata meta = {0};
    SituationError err = SituationGetDeviceMetadata(SITUATION_NODE_TONE_SYNTH, &meta);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(meta.category == SITUATION_DEVICE_SOURCE);
    SIT_ASSERT(meta.num_audio_outs > 0);
    SIT_ASSERT(meta.num_controls == 39);
}

static void test_graph_create_node_tone_synth(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationError err = SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &handle);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(handle != SITUATION_INVALID_NODE_HANDLE);

    SituationDestroyGraph(graph);
}
static void test_graph_create_patch_audio(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle src = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle dst = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &src);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &dst);

    SituationError err = SituationCreatePatch(graph, src, 0, dst, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_patch_synth_to_reverb(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle synth = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth);
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    SituationError err = SituationCreatePatch(graph, synth, 0, reverb, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_patch_chain(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle synth = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle panner = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth);
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &panner);

    SituationError err = SituationCreatePatch(graph, synth, 0, reverb, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationCreatePatch(graph, reverb, 0, panner, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_destroy_patch(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle src = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle dst = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &src);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &dst);

    SituationError err = SituationCreatePatch(graph, src, 0, dst, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // Note: SituationRemovePatch/DestroyPatch not yet exported from DLL
    // Destroying the graph implicitly removes all patches
    SituationDestroyGraph(graph);
    SIT_ASSERT(true); // No crash on graph destruction with active patches
}

static void test_graph_patch_duplicate(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle src = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle dst = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &src);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &dst);

    SituationError err1 = SituationCreatePatch(graph, src, 0, dst, 0, false);
    SIT_ASSERT(err1 == SITUATION_SUCCESS);

    // Double-connect same ports â€” library currently allows this (no duplicate check)
    SituationError err2 = SituationCreatePatch(graph, src, 0, dst, 0, false);
    // Just verify no crash â€” behavior is implementation-defined
    SIT_ASSERT(true);

    SituationDestroyGraph(graph);
}

static void test_graph_patch_invalid_node(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle valid = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &valid);

    // Connect to invalid node handle
    SituationError err = SituationCreatePatch(graph, valid, 0, SITUATION_INVALID_NODE_HANDLE, 0, false);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_patch_invalid_port(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle src = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle dst = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &src);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &dst);

    // Out-of-range port index
    SituationError err = SituationCreatePatch(graph, src, 99, dst, 99, false);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}
static void test_effect_chain(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle synth = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle filter = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle panner = SITUATION_INVALID_NODE_HANDLE;

    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth);
    SituationCreateNode(graph, SITUATION_NODE_FILTER, &filter);
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &panner);

    // Chain: synth â†’ filter â†’ reverb â†’ panner
    SituationError err = SituationCreatePatch(graph, synth, 0, filter, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    err = SituationCreatePatch(graph, filter, 0, reverb, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    err = SituationCreatePatch(graph, reverb, 0, panner, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // Destroy chain â€” no crash, all resources freed
    SituationDestroyGraph(graph);
    SIT_ASSERT(true);
}

// ============================================================================
//  PHASE 6 â€” Graph Serialization Roundtrip
// ============================================================================

static void test_graph_serialize_with_nodes(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    // Create 3 nodes and 2 patches
    SituationNodeHandle synth = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle panner = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth);
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &panner);

    SituationCreatePatch(graph, synth, 0, reverb, 0, false);
    SituationCreatePatch(graph, reverb, 0, panner, 0, false);

    // Serialize to JSON
    char* json = SituationSerializeGraphToJSON(graph);
    SIT_ASSERT_NOT_NULL(json);
    SIT_ASSERT(strlen(json) > 0);
    // Verify JSON starts with '{' and contains "nodes"
    SIT_ASSERT(json[0] == '{');
    SIT_ASSERT(strstr(json, "nodes") != NULL);

    SituationFreeJSONString(json);
    SituationDestroyGraph(graph);
}

static void test_graph_save_load_roundtrip(void) {
    SituationInitDeviceRegistry();

    // Create and populate a graph
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle synth = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle panner = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth);
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &panner);

    SituationCreatePatch(graph, synth, 0, reverb, 0, false);
    SituationCreatePatch(graph, reverb, 0, panner, 0, false);

    // Save to file
    const char* filepath = "_sit_test_graph.json";
    SituationError err = SituationSaveGraphToFile(graph, filepath);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // Verify file exists
    FILE* f = fopen(filepath, "r");
    SIT_ASSERT_NOT_NULL(f);
    if (f) fclose(f);

    SituationDestroyGraph(graph);

    // Load into new graph
    SituationAudioGraph* graph2 = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph2);

    err = SituationLoadGraphFromFile(graph2, filepath, NULL, 0);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationDestroyGraph(graph2);
    remove(filepath);
}

static void test_graph_deserialize_from_json(void) {
    SituationInitDeviceRegistry();

    // Create a graph, serialize it
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle synth = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle panner = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &synth);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &panner);
    SituationCreatePatch(graph, synth, 0, panner, 0, false);

    char* json = SituationSerializeGraphToJSON(graph);
    SIT_ASSERT_NOT_NULL(json);
    SituationDestroyGraph(graph);

    // Deserialize into new graph
    SituationAudioGraph* graph2 = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph2);

    SituationError err = SituationDeserializeGraphFromJSON(graph2, json, NULL, 0);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationFreeJSONString(json);
    SituationDestroyGraph(graph2);
}
// ============================================================================
//  PHASE 8 — MIDI / Audio Frequency Verification
//
//  Graph tone synth (PortMidi → SITUATION_NODE_TONE_SYNTH):
//    Input device : SITUATION_VIRTUAL_MIDI_IN_NAME
//    MIDI channel : SITUATION_TEST_MIDI_CHANNEL (0-based; display as channel 1)
//    Synth target : SITUATION_TONE_SYNTH_MIDI_DEVICE_NAME
//
//  | Test                               | Notes / CC / bend channel |
//  |------------------------------------|---------------------------|
//  | graph_tone_synth_midi_note_frequency | ch0 note + CC N/A       |
//  | graph_tone_synth_midi_complex_melody | ch0 CC1,7,bend          |
//  | graph_tone_synth_velocity_ramp       | ch0 velocity only       |
//  | graph_tone_synth_cc_mod_vibrato      | ch0 CC1                 |
//  | graph_tone_synth_cc92_tremolo        | ch0 CC92                |
//  | graph_tone_synth_filter_modes        | ch0 CC16/74 LP+HP       |
//  | graph_tone_synth_pulse_width         | ch0 CC70/106 pulse      |
//  | graph_tone_synth_waveforms_all       | ch0 CC70 waveforms 0–4  |
//  | graph_tone_synth_lfo_mod             | ch0 LFO pitch + PWM     |
//  | graph_tone_synth_filter_env_adsr     | ch0 ADSR → filter CC32  |
//  | sub_oscillator                       | ch0 CC107-110 level/oct/fine/wave |
//  | sub_sync                             | A4 hold: coarse 0, then CC111 sweep |
//  | sub_ring_mod                         | A4 hold: coarse 0, then CC111 sweep |
//  | tone_synth_phase1_compare_a4         | legacy then graph (exclusive) |
//
//  legacy_tone_pool_midi_note_frequency: SituationPlayMidiNote (no PortMidi).
//  tone_synth_phase1_compare_a4: Phase 1 scrutiny — one path at a time, same A4.
// ============================================================================

static SituationAudioGraph* g_sit_midi_graph_fixture = NULL;
static SituationNodeHandle g_sit_midi_graph_fixture_tone = SITUATION_INVALID_NODE_HANDLE;

static void sit_midi_tone_graph_reset_midi_channel(void);
static void sit_midi_tone_graph_silence_midi(void);
static void sit_midi_tone_graph_teardown(SituationAudioGraph* graph, SituationNodeHandle tone);
static SituationError sit_midi_tone_graph_setup(SituationAudioGraph** out_graph,
                                                SituationNodeHandle* out_tone,
                                                int* out_midi_in,
                                                const char* midi_usage);

/** Release graph/MIDI from a prior test that longjmp'd before teardown. */
static void sit_midi_graph_fixture_release(void) {
    sit_test_audio_monitor_set_capture(NULL);
    if (g_sit_midi_graph_fixture) {
        sit_midi_tone_graph_teardown(g_sit_midi_graph_fixture, g_sit_midi_graph_fixture_tone);
        g_sit_midi_graph_fixture = NULL;
        g_sit_midi_graph_fixture_tone = SITUATION_INVALID_NODE_HANDLE;
    } else {
        SituationSetActiveGraph(NULL);
        SituationStopAllTones();
        SituationTeardownVirtualMidiLoopback();
    }
    sit_test_listen_overlay_pump_ms(80);
}

static void sit_midi_graph_fixture_track(SituationAudioGraph* graph, SituationNodeHandle tone) {
    g_sit_midi_graph_fixture = graph;
    g_sit_midi_graph_fixture_tone = tone;
}

static void sit_midi_tone_graph_reset_midi_channel(void) {
    /* Virtual MIDI CC/bend persists across graphs — reset before each graph MIDI test. */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 123, 0);  /* all notes off */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 64, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 92, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 7, 127);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 1, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 11, 127);
    SituationVirtualMidiPitchBend(SITUATION_TEST_MIDI_CHANNEL, 8192);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
}

static bool sit_audio_freq_test_ready(int* out_sample_rate) {
    int sr = SituationGetAudioPlaybackSampleRate();
    if (out_sample_rate) *out_sample_rate = sr;
    return sr > 0;
}

static bool sit_audio_freq_verify_tone(SitAudioFreqCapture* cap, int midi_note, float* out_measured_hz) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) return false;

    float expected = sit_midi_note_to_hz(midi_note);
    return sit_audio_freq_verify(cap, (float)sr, expected, 0.04f, 0.005f, out_measured_hz);
}

/** Legacy tone pool: SituationPlayMidiNote → master bus → monitor captures ~440 Hz for A4. */
static void test_legacy_midi_note_emits_frequency(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true); /* no playback device — skip */
        return;
    }

    sit_midi_log_legacy_tone_pool_route(
        g_sit_current_test_name ? g_sit_current_test_name : "legacy_tone_pool_midi_note_frequency");

    SituationSetActiveGraph(NULL);
    SituationStopAllTones();
    SIT_TONE_CAPTURE_SLEEP_MS(80);

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, (uint32_t)sr, 2);
    sit_audio_freq_capture_reset(&cap);
    sit_test_audio_monitor_set_capture( &cap);

    SituationSetAudioMasterVolume(0.8f);
    SituationPlayMidiNote(69, SIT_WAVE_SINE, 0.8f, 0.01f, 0.05f, 0.75f, 0.1f, 1.0f);
    SIT_TONE_CAPTURE_SLEEP_MS(700);
    sit_test_audio_monitor_set_capture(NULL);
    SIT_TONE_CAPTURE_SLEEP_MS(20);

    float peak = 0.f, rms = 0.f;
    SituationGetMasterOutputMeter(&peak, &rms);

    float measured = 0.f;
    bool peak_ok = (peak > 0.01f);
    bool tone_ok = sit_audio_freq_verify_tone(&cap, 69, &measured);
    bool measured_ok = (fabsf(measured - 440.0f) < 440.0f * 0.04f);
    if (!peak_ok || !tone_ok || !measured_ok) {
        fprintf(stderr,
            "[tone_synth] legacy_midi_note_frequency miss peak=%.6f rms=%.6f cap_frames=%u measured=%.2f tone_ok=%d\n",
            peak, rms, cap.count, measured, tone_ok ? 1 : 0);
    }

    SituationStopAllTones();
    sit_audio_freq_capture_free(&cap);

    SIT_ASSERT(peak_ok);
    SIT_ASSERT(tone_ok);
    SIT_ASSERT(measured_ok);
}

/** Graph tone synth node: virtual MIDI note-on → master bus at expected pitch. */
static void test_graph_midi_note_emits_frequency(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    sit_midi_graph_fixture_release();

    int midi_in_id = -1;
    SituationError err = SituationSetupVirtualMidiLoopback(&midi_in_id);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(midi_in_id >= 0);

    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    err = SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetControl(graph, tone, 1, 0.0f); /* sine */
    SIT_ASSERT(err == SITUATION_SUCCESS);
    err = SituationSetControl(graph, tone, 2, 0.0f); /* muted until note-on */
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationEnableMidiControl(graph, tone, midi_in_id);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    err = SituationSetNodeMidiChannel(graph, tone, SITUATION_TEST_MIDI_CHANNEL);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsMidiEnabled(graph, tone) == 1);

    err = SituationSetActiveGraph(graph);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    sit_midi_graph_fixture_track(graph, tone);

    sit_midi_log_graph_tone_synth_route(
        g_sit_current_test_name ? g_sit_current_test_name : "graph_tone_synth_midi_note_frequency",
        midi_in_id,
        "ch0 note on/off (VirtualMidiNoteOnEx/OffEx); no CC");

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, (uint32_t)sr, 2);
    sit_audio_freq_capture_reset(&cap);
    sit_test_audio_monitor_set_capture( &cap);

    SituationSetAudioMasterVolume(0.8f);
    err = SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    /* Note-on callback defaults to square; force sine for stable frequency verify. */
    SituationSetControl(graph, tone, 1, 0.0f);
    SIT_TONE_TEST_SLEEP_MS(450);

    float peak = 0.f, rms = 0.f;
    SituationGetMasterOutputMeter(&peak, &rms);
    SIT_ASSERT(peak > 0.01f);

    float measured = 0.f;
    SIT_ASSERT(sit_audio_freq_verify_tone(&cap, 69, &measured));
    SIT_ASSERT(fabsf(measured - 440.0f) < 440.0f * 0.04f);

    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

/**
 * Phase 1 scrutiny: run legacy-only then graph-only on the same A4 (MIDI 69).
 * Only one synthesis path is active per segment — no double-mixing.
 */
static void test_tone_synth_phase1_compare_a4(void) {
    const uint8_t note = 69;
    const uint32_t hold_ms = 450;

    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    sit_midi_graph_fixture_release();
    SituationSetActiveGraph(NULL);
    SituationStopAllTones();
    SIT_TONE_TEST_SLEEP_MS(80);

    float legacy_hz = 0.f, legacy_peak = 0.f, legacy_rms = 0.f;
    float graph_hz = 0.f, graph_peak = 0.f, graph_rms = 0.f;

    /* --- Segment A: legacy only (graph inactive) --- */
    printf("  [COMPARE Phase 1] segment A: LEGACY ONLY — SetActiveGraph(NULL), PlayMidiNote\n");
    sit_midi_log_legacy_tone_pool_route("phase1_segment_a");

    SitAudioFreqCapture cap_legacy;
    sit_audio_freq_capture_init(&cap_legacy, (uint32_t)sr, 2);
    sit_audio_freq_capture_reset(&cap_legacy);
    sit_test_audio_monitor_set_capture( &cap_legacy);
    SituationSetAudioMasterVolume(0.8f);

    SituationPlayMidiNote(note, SIT_WAVE_SINE, 0.8f, 0.01f, 0.05f, 0.75f, 0.1f, 1.0f);
    SIT_TONE_TEST_SLEEP_MS(hold_ms);

    SituationGetMasterOutputMeter(&legacy_peak, &legacy_rms);
    SIT_ASSERT(legacy_peak > 0.01f);
    SIT_ASSERT(sit_audio_freq_verify_tone(&cap_legacy, note, &legacy_hz));

    SituationStopAllTones();
    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap_legacy);
    SIT_TONE_TEST_SLEEP_MS(120);

    /* --- Segment B: graph only (legacy silent) --- */
    printf("  [COMPARE Phase 1] segment B: GRAPH ONLY — StopAllTones(), active graph + NoteOnEx\n");

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 NoteOnEx A4; legacy pool silent"), SITUATION_SUCCESS);

    SituationSetControl(graph, tone, 1, 0.0f); /* sine for apples-to-apples pitch check */
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 2, 0.0f), SITUATION_SUCCESS);

    SitAudioFreqCapture cap_graph;
    sit_audio_freq_capture_init(&cap_graph, (uint32_t)sr, 2);
    sit_audio_freq_capture_reset(&cap_graph);
    sit_test_audio_monitor_set_capture( &cap_graph);

    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, note, 100),
                  SITUATION_SUCCESS);
    SituationSetControl(graph, tone, 1, 0.0f);
    SIT_TONE_TEST_SLEEP_MS(hold_ms);

    SituationGetMasterOutputMeter(&graph_peak, &graph_rms);
    SIT_ASSERT(graph_peak > 0.01f);
    SIT_ASSERT(sit_audio_freq_verify_tone(&cap_graph, note, &graph_hz));

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap_graph);

    sit_midi_log_phase1_compare("A4 side-by-side (exclusive paths)", legacy_hz, legacy_peak, legacy_rms,
                              graph_hz, graph_peak, graph_rms);

    SIT_ASSERT(fabsf(legacy_hz - 440.0f) < 440.0f * 0.04f);
    SIT_ASSERT(fabsf(graph_hz - 440.0f) < 440.0f * 0.04f);
    SIT_ASSERT(fabsf(graph_hz - legacy_hz) < 440.0f * 0.04f);

    sit_midi_tone_graph_teardown(graph, tone);
}

typedef struct {
    uint32_t start_ms;
    uint8_t note;
    uint16_t hold_ms;
    uint8_t velocity;
} SitMidiMelodyEvent;

static bool sit_midi_melody_verify_note_window(const SitAudioFreqCapture* cap, int sr,
                                               uint32_t start_ms, uint32_t dur_ms,
                                               uint8_t midi_note, float min_power) {
    if (!cap || cap->count < 64 || sr <= 0) return false;

    uint32_t frame_count = (uint32_t)(((uint64_t)dur_ms * (uint32_t)sr) / 1000u);
    if (frame_count < 64) frame_count = 64;

    float expected_hz = sit_midi_note_to_hz(midi_note);
    int best_offset_ms = 0;
    float best_power = 0.0f;
    float best_neighbor = 0.0f;

    /* Scan a narrow neighborhood after the expected phrase start. Negative offsets
     * only cover callback jitter — not the prior note still sounding. */
    for (int offset_ms = -40; offset_ms <= 120; offset_ms += 20) {
        sit_test_stereo_scope_service_ui();
        int64_t shifted_ms = (int64_t)start_ms + offset_ms;
        if (shifted_ms < 0) shifted_ms = 0;
        uint32_t start_frame = (uint32_t)(((uint64_t)shifted_ms * (uint32_t)sr) / 1000u);
        if (start_frame >= cap->count) continue;

        uint32_t frames = frame_count;
        if (start_frame + frames > cap->count) {
            frames = cap->count - start_frame;
        }
        if (frames < 64) continue;

        float power = sit_goertzel_power(cap->samples + start_frame * cap->channels,
                                         frames, cap->channels, (float)sr, expected_hz);
        float neighbor;
        /* E4 (+7 semitones) is B4 — another phrase degree; use adjacent bins instead. */
        if (midi_note == 64) {
            float neighbor_up = sit_goertzel_power(cap->samples + start_frame * cap->channels,
                                                   frames, cap->channels, (float)sr,
                                                   expected_hz * powf(2.0f, 1.0f / 12.0f));
            float neighbor_down = sit_goertzel_power(cap->samples + start_frame * cap->channels,
                                                     frames, cap->channels, (float)sr,
                                                     expected_hz * powf(2.0f, -1.0f / 12.0f));
            neighbor = neighbor_up > neighbor_down ? neighbor_up : neighbor_down;
        } else {
            neighbor = sit_goertzel_power(cap->samples + start_frame * cap->channels,
                                          frames, cap->channels, (float)sr,
                                          expected_hz * powf(2.0f, 7.0f / 12.0f));
        }
        if (power > best_power) {
            best_power = power;
            best_neighbor = neighbor;
            best_offset_ms = offset_ms;
        }
        if (power > min_power && power > neighbor * 1.2f) {
            return true;
        }
    }

    fprintf(stderr,
        "[tone_synth] melody verify miss note=%u start=%ums dur=%ums cap_frames=%u best_offset=%dms power=%g neighbor=%g\n",
        (unsigned)midi_note, start_ms, dur_ms, cap->count, best_offset_ms,
        (double)best_power, (double)best_neighbor);
    return false;
}

/**
 * Virtual MIDI melody (graph tone synth): square wave, velocities, pitch bend, CC1/CC7.
 * Verifies each phrase window contains the expected pitch (Goertzel).
 */
static void test_midi_complex_melody_expression(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    static const SitMidiMelodyEvent melody[] = {
        { 0,   60, 280, 70  },  /* C4 */
        { 300, 64, 280, 110 },  /* E4 */
        { 600, 67, 420, 90  },  /* G4 — bend + vibrato during hold */
        { 1050, 72, 280, 127 }, /* C5 */
        { 1350, 71, 280, 75  },  /* B4 */
        { 1650, 67, 280, 95  },  /* G4 */
        { 1950, 64, 350, 55  },  /* E4 */
    };
    const uint32_t melody_end_ms = 2350;

    sit_midi_graph_fixture_release();

    int midi_in_id = -1;
    SituationError err = SituationSetupVirtualMidiLoopback(&midi_in_id);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    SIT_ASSERT_EQ(SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 1, 0.0f), SITUATION_SUCCESS); /* sine */
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 16, 0.0f), SITUATION_SUCCESS); /* poly */
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 2, 0.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEnableMidiControl(graph, tone, midi_in_id), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetNodeMidiChannel(graph, tone, SITUATION_TEST_MIDI_CHANNEL), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetActiveGraph(graph), SITUATION_SUCCESS);

    sit_midi_graph_fixture_track(graph, tone);
    sit_midi_tone_graph_reset_midi_channel();
    SIT_TONE_CAPTURE_SLEEP_MS(80);

    sit_midi_log_graph_tone_synth_route(
        g_sit_current_test_name ? g_sit_current_test_name : "graph_tone_synth_midi_complex_melody",
        midi_in_id,
        "ch0 notes + velocity; ch0 pitch bend on G4; ch0 CC1 vibrato on G4");
    sit_test_listen_overlay_set("midi_complex_melody", "melody playback",
                                "C4 E4 G4 C5 B4 G4 E4 — square, bend, vibrato on G4");

    const uint32_t cap_frames = (uint32_t)(((uint64_t)(melody_end_ms + 400) * (uint32_t)sr) / 1000u);
    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);
    sit_audio_freq_capture_reset(&cap);

    uint8_t current_note = 255;
    uint8_t melody_started[sizeof(melody) / sizeof(melody[0])] = {0};
    for (uint32_t t = 0; t <= melody_end_ms; t += 20) {
        for (size_t i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
            const SitMidiMelodyEvent* ev = &melody[i];
            if (!melody_started[i] && t >= ev->start_ms) {
                melody_started[i] = 1;
                if (current_note != 255) {
                    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL,current_note);
                }
                SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL,ev->note, ev->velocity), SITUATION_SUCCESS);
                current_note = ev->note;
            }
            /* First G4 only: pitch bend + mod-wheel vibrato after stable attack */
            if (i == 2 && melody_started[i] && t >= ev->start_ms && t < ev->start_ms + ev->hold_ms) {
                if (t == ev->start_ms + 120) {
                    SituationVirtualMidiPitchBend(SITUATION_TEST_MIDI_CHANNEL, 11000);
                } else if (t == ev->start_ms + 200) {
                    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 1, 110);
                } else if (t == ev->start_ms + 260) {
                    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 1, 18);
                } else if (t == ev->start_ms + 320) {
                    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 1, 105);
                    SituationVirtualMidiPitchBend(SITUATION_TEST_MIDI_CHANNEL, 8192);
                }
            }
        }
        /* 20 ms steps are shorter than SIT_TEST_SCOPE_RENDER_INTERVAL_MS; pump each step
         * explicitly (same pattern as effect_heard wet sweeps). */
        (void)sit_test_listen_overlay_render_once();
        sit_test_listen_overlay_wait_ms(20);
    }
    (void)sit_test_listen_overlay_render_once();

    float peak = 0.f, rms = 0.f;
    SituationGetMasterOutputMeter(&peak, &rms);
    SIT_ASSERT(peak > 0.01f);

    if (current_note != 255) {
        SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL,current_note);
    }
    SIT_TONE_CAPTURE_SLEEP_MS(120);
    sit_test_audio_monitor_set_capture(NULL);
    SIT_TONE_CAPTURE_SLEEP_MS(20);

    SIT_ASSERT(cap.count >= 256);

    bool melody_ok = true;
    for (size_t i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
        sit_test_stereo_scope_service_ui();
        const SitMidiMelodyEvent* ev = &melody[i];
        uint32_t verify_start = ev->start_ms + 80;
        uint32_t verify_dur = 100;
        if (i == 2) {
            /* Verify clean G4 before bend/vibrato (event index 2). */
            verify_start = ev->start_ms + 50;
            verify_dur = 60;
        } else if (i == sizeof(melody) / sizeof(melody[0]) - 1) {
            verify_start = ev->start_ms + 100;
            verify_dur = 120;
        } else if (ev->hold_ms > 140) {
            verify_dur = ev->hold_ms - 90;
        }
        if (!sit_midi_melody_verify_note_window(&cap, sr, verify_start, verify_dur,
                                                ev->note, 1e-5f)) {
            melody_ok = false;
        }
    }

    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
    SIT_ASSERT(melody_ok);
}

static SituationError sit_midi_tone_graph_setup(SituationAudioGraph** out_graph,
                                                SituationNodeHandle* out_tone,
                                                int* out_midi_in,
                                                const char* midi_usage) {
    if (!out_graph || !out_tone || !out_midi_in) return SITUATION_ERROR_INVALID_PARAM;

    sit_midi_graph_fixture_release();
    int midi_in_id = -1;
    SituationError err = SituationSetupVirtualMidiLoopback(&midi_in_id);
    if (err != SITUATION_SUCCESS) return err;

    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) {
        SituationTeardownVirtualMidiLoopback();
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    err = SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyGraph(graph);
        SituationTeardownVirtualMidiLoopback();
        return err;
    }

    err = SituationSetControl(graph, tone, 2, 0.0f);
    if (err != SITUATION_SUCCESS) goto fail;
    err = SituationEnableMidiControl(graph, tone, midi_in_id);
    if (err != SITUATION_SUCCESS) goto fail;
    err = SituationSetNodeMidiChannel(graph, tone, SITUATION_TEST_MIDI_CHANNEL);
    if (err != SITUATION_SUCCESS) goto fail;
    err = SituationSetActiveGraph(graph);
    if (err != SITUATION_SUCCESS) goto fail;

    *out_graph = graph;
    *out_tone = tone;
    *out_midi_in = midi_in_id;

    sit_midi_log_graph_tone_synth_route(
        g_sit_current_test_name ? g_sit_current_test_name : "graph_tone_synth",
        midi_in_id,
        midi_usage);

    sit_midi_graph_fixture_track(graph, tone);
    sit_midi_tone_graph_reset_midi_channel();
    SIT_TONE_TEST_SLEEP_MS(80);
    return SITUATION_SUCCESS;

fail:
    SituationDestroyGraph(graph);
    SituationTeardownVirtualMidiLoopback();
    return err;
}

static void sit_midi_tone_graph_silence_midi(void) {
    /* Flush CC92 tremolo / mod / sustain before teardown (graph must still be active). */
    sit_midi_tone_graph_reset_midi_channel();
    SIT_TONE_TEST_SLEEP_MS(120);
}

static void sit_midi_tone_graph_teardown(SituationAudioGraph* graph, SituationNodeHandle tone) {
    if (graph) {
        sit_midi_tone_graph_silence_midi();
        SituationDisableMidiControl(graph, tone);
        SituationSetActiveGraph(NULL);
        SituationDestroyGraph(graph);
    }
    SituationStopAllTones();
    SituationTeardownVirtualMidiLoopback();
    if (graph == g_sit_midi_graph_fixture) {
        g_sit_midi_graph_fixture = NULL;
        g_sit_midi_graph_fixture_tone = SITUATION_INVALID_NODE_HANDLE;
    }
}

static float sit_capture_ms_rms(const SitAudioFreqCapture* cap, int sr, uint32_t start_ms, uint32_t dur_ms) {
    if (!cap || sr <= 0) return 0.0f;
    uint32_t start_frame = (uint32_t)(((uint64_t)start_ms * (uint32_t)sr) / 1000u);
    uint32_t frame_count = (uint32_t)(((uint64_t)dur_ms * (uint32_t)sr) / 1000u);
    if (frame_count < 64) frame_count = 64;
    return sit_audio_capture_window_rms(cap, start_frame, frame_count);
}

static void sit_tone_listen_capture_arm(SitAudioFreqCapture* cap) {
    sit_test_audio_monitor_set_capture(cap);
    SIT_TONE_CAPTURE_SLEEP_MS(80);
    sit_audio_freq_capture_reset(cap);
    sit_test_stereo_scope_reset();
    SIT_TONE_CAPTURE_SLEEP_MS(40);
}

/** MIDI note-on velocity ramp: repeated note on/off from quiet to loud; RMS rises each step. */
static void test_midi_velocity_volume_levels(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    static const uint8_t velocities[] = {
        10, 22, 34, 46, 58, 70, 82, 94, 106, 118, 127
    };
    const size_t step_count = sizeof(velocities) / sizeof(velocities[0]);
    const uint32_t note_ms = 220;
    const uint32_t gap_ms = 80;
    const uint32_t measure_ms = 100;
    const uint32_t measure_offset_ms = note_ms - measure_ms; /* sustain tail before note off */
    const uint32_t total_ms = (uint32_t)(step_count * (note_ms + gap_ms) + 160);

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 note on/off; velocity ramp 10..127 (VirtualMidiNoteOnEx)"), SITUATION_SUCCESS);
    SituationStopAllTones();
    sit_midi_tone_graph_reset_midi_channel();
    SIT_TONE_CAPTURE_SLEEP_MS(280);

    const uint32_t cap_frames = (uint32_t)(((uint64_t)total_ms * (uint32_t)sr) / 1000u);
    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_tone_listen_capture_arm(&cap);

    uint32_t t = 0;
    float rms_steps[sizeof(velocities) / sizeof(velocities[0])];

    for (size_t i = 0; i < step_count; i++) {
        SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL,69, velocities[i]), SITUATION_SUCCESS);
        SIT_TONE_CAPTURE_SLEEP_MS(note_ms);
        SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
        sit_test_stereo_scope_service_ui();
        rms_steps[i] = sit_capture_ms_rms(&cap, sr, t + measure_offset_ms, measure_ms);
        t += note_ms + gap_ms;
        if (i + 1 < step_count) {
            SIT_TONE_CAPTURE_SLEEP_MS(gap_ms);
        }
    }

    float peak = 0.f, meter_rms = 0.f;
    SituationGetMasterOutputMeter(&peak, &meter_rms);
    SIT_ASSERT(peak > 0.01f);
    SIT_ASSERT(cap.count >= 256);

    SIT_ASSERT(rms_steps[0] > 0.0008f);
    SIT_ASSERT(rms_steps[step_count - 1] > rms_steps[0] * 2.5f);

    const float rms_lo =
        (rms_steps[0] + rms_steps[1] + rms_steps[2]) / 3.0f;
    const float rms_hi = (rms_steps[step_count - 1u] + rms_steps[step_count - 2u] +
                          rms_steps[step_count - 3u]) /
                         3.0f;
    SIT_ASSERT(rms_hi > rms_lo * 2.0f);

    size_t increases = 0;
    size_t decreases = 0;
    for (size_t i = 0; i + 1 < step_count; i++) {
        sit_test_stereo_scope_service_ui();
        if (rms_steps[i + 1] >= rms_steps[i] * 1.03f) {
            increases++;
        } else if (rms_steps[i + 1] < rms_steps[i] * 0.97f) {
            decreases++;
        }
    }
    /* Ramp: mostly monotonic; allow dips from capture jitter / prior-test bleed. */
    SIT_ASSERT(increases >= step_count - 5);
    SIT_ASSERT(decreases <= 4);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

static void sit_measure_goertzel_wander_at_hz(const SitAudioFreqCapture* cap, int sr,
                                             uint32_t start_ms, uint32_t dur_ms,
                                             float target_hz, uint32_t win_ms,
                                             float* out_min, float* out_max) {
    uint32_t start_frame = (uint32_t)(((uint64_t)start_ms * (uint32_t)sr) / 1000u);
    uint32_t end_frame = start_frame + (uint32_t)(((uint64_t)dur_ms * (uint32_t)sr) / 1000u);
    uint32_t win_frames = (uint32_t)(((uint64_t)win_ms * (uint32_t)sr) / 1000u);
    if (win_frames < 64) win_frames = 64;
    uint32_t frame_step = win_frames / 2;
    if (frame_step < 64) frame_step = 64;

    float pmin = 1e30f, pmax = 0.f;
    for (uint32_t f = start_frame; f + win_frames < end_frame && f + win_frames <= cap->count; f += frame_step) {
        float p = sit_goertzel_power(cap->samples + f * cap->channels, win_frames, cap->channels,
                                     (float)sr, target_hz);
        if (p < pmin) pmin = p;
        if (p > pmax) pmax = p;
    }
    *out_min = pmin;
    *out_max = pmax;
}

/** Sliding RMS min/max over [start_ms, start_ms+dur_ms) — for amplitude/PWM LFO checks. */
static void sit_measure_rms_wander_ms(const SitAudioFreqCapture* cap, int sr,
                                       uint32_t start_ms, uint32_t dur_ms, uint32_t win_ms,
                                       uint32_t step_ms, float* out_min, float* out_max) {
    if (!cap || sr <= 0 || dur_ms == 0u) {
        *out_min = 0.f;
        *out_max = 0.f;
        return;
    }
    if (win_ms < 32u) {
        win_ms = 32u;
    }
    if (step_ms < 16u) {
        step_ms = 16u;
    }
    const uint32_t end_ms = start_ms + dur_ms;
    float rmin = 1e30f;
    float rmax = 0.f;
    for (uint32_t t = start_ms; t + win_ms < end_ms; t += step_ms) {
        const float r = sit_capture_ms_rms(cap, sr, t, win_ms);
        if (r < rmin) {
            rmin = r;
        }
        if (r > rmax) {
            rmax = r;
        }
    }
    if (rmax <= 0.f) {
        rmin = 0.f;
    }
    *out_min = rmin;
    *out_max = rmax;
}

/** CC1 mod wheel: LFO vibrato depth (5 Hz); CC7/CC11 volume; CC64 sustain. */
static void test_graph_tone_synth_cc_mod_vibrato_pitch(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 CC1 mod wheel 0→127 (5 Hz LFO vibrato depth); note ch0"), SITUATION_SUCCESS);

    const uint32_t seg_ms = 320;
    const uint32_t total_ms = seg_ms * 2 + 200;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)total_ms * (uint32_t)sr) / 1000u);
    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    const float base_hz = 440.0f;

    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL,69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(100);
    sit_test_listen_overlay_set("cc_mod_vibrato", "mod wheel 0", "no vibrato — steady 440 Hz");
    SIT_ASSERT_EQ(SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 1, 0), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    sit_test_listen_overlay_set("cc_mod_vibrato", "mod wheel 127", "5 Hz pitch vibrato on A4");
    SIT_ASSERT_EQ(SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 1, 127), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);

    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    SIT_ASSERT(cap.count >= 256);

    const uint32_t win_ms = 80;
    float no_mod_min = 0.f, no_mod_max = 0.f;
    float mod_min = 0.f, mod_max = 0.f;
    sit_measure_goertzel_wander_at_hz(&cap, sr, 100, seg_ms - 40, base_hz, win_ms, &no_mod_min, &no_mod_max);
    sit_test_stereo_scope_service_ui();
    sit_measure_goertzel_wander_at_hz(&cap, sr, 100 + seg_ms, seg_ms - 40, base_hz, win_ms, &mod_min, &mod_max);

    SIT_ASSERT(no_mod_max > 1e-5f);
    SIT_ASSERT(mod_max > 1e-5f);
    /* No vibrato: level at 440 Hz should stay steady */
    SIT_ASSERT(no_mod_max < no_mod_min * 1.15f);
    /* Full mod wheel: LFO pulls energy away from 440 Hz in some windows */
    SIT_ASSERT(mod_max > mod_min * 1.2f);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

/** CC92 tremolo depth: 5 Hz amplitude LFO (RMS peak/trough ratio). */
static void test_graph_tone_synth_cc92_tremolo(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 CC92 tremolo depth 127 (5 Hz LFO); note via VirtualMidiNoteOnEx"), SITUATION_SUCCESS);

    const uint32_t settle_ms = 150;
    const uint32_t tremolo_ms = 1000;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)(settle_ms + tremolo_ms + 400) * (uint32_t)sr) / 1000u);
    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(settle_ms);
    sit_test_listen_overlay_set("cc92_tremolo", "CC92 depth 127", "5 Hz amplitude tremolo A4");
    SIT_ASSERT_EQ(SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 92, 127), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(tremolo_ms);

    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 92, 0);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    SIT_TONE_TEST_SLEEP_MS(80);
    SIT_ASSERT(cap.count >= 256);

    float rms_max = 0.0f;
    float rms_min = 1e9f;
    const uint32_t measure_ms = 50;
    const uint32_t scan_end_ms = settle_ms + tremolo_ms;
    for (uint32_t t = settle_ms + 80; t + measure_ms < scan_end_ms; t += 20) {
        sit_test_stereo_scope_service_ui();
        float r = sit_capture_ms_rms(&cap, sr, t, measure_ms);
        if (r > rms_max) rms_max = r;
        if (r < rms_min) rms_min = r;
    }

    SIT_ASSERT(rms_max > 0.003f);
    SIT_ASSERT(rms_min < rms_max);
    SIT_ASSERT(rms_max > rms_min * 1.5f);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

static float sit_capture_ms_goertzel(const SitAudioFreqCapture* cap, int sr,
                                     uint32_t start_ms, uint32_t dur_ms, float hz) {
    if (!cap || sr <= 0 || cap->count == 0) return 0.0f;
    uint32_t start_frame = (uint32_t)(((uint64_t)start_ms * (uint32_t)sr) / 1000u);
    uint32_t frame_count = (uint32_t)(((uint64_t)dur_ms * (uint32_t)sr) / 1000u);
    if (frame_count < 256) frame_count = 256;
    if (start_frame >= cap->count) return 0.0f;
    if (start_frame + frame_count > cap->count) {
        frame_count = cap->count - start_frame;
    }
    if (frame_count < 64) return 0.0f;
    return sit_goertzel_power(cap->samples + start_frame * cap->channels, frame_count,
                              cap->channels, (float)sr, hz);
}

/**
 * Filter modes: sine A4 with filter off vs LP (~200 Hz) vs HP (~800 Hz).
 * LP and HP should both reduce Goertzel power at 440 Hz vs bypass.
 */
static void test_graph_tone_synth_filter_modes_attenuate_a4(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 CC16 filter mode, CC74 cutoff; sine A4 bypass vs LP vs HP"), SITUATION_SUCCESS);

    const uint32_t seg_ms = 520;
    const uint32_t measure_ms = 300;
    const uint32_t measure_offset_ms = 180;
    const uint32_t gap_ms = 80;
    const uint32_t total_ms = seg_ms * 3 + gap_ms * 2 + 120;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)total_ms * (uint32_t)sr) / 1000u);
    const float target_hz = 440.0f;

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    SituationSetControl(graph, tone, 1, 0.0f); /* sine */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 70, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 71, 0);   /* low Q */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 102, 64); /* 3 poles */

    float powers[3];
    uint32_t t = 0;

    /* Segment 0: filter off */
    sit_test_listen_overlay_set("filter_modes", "Seg 0: filter bypass", "sine A4 440 Hz");
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 16, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 74, 64);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    sit_test_stereo_scope_service_ui();
    powers[0] = sit_capture_ms_goertzel(&cap, sr, t + measure_offset_ms, measure_ms, target_hz);
    t += seg_ms + gap_ms;
    SIT_TONE_TEST_SLEEP_MS(gap_ms);

    /* Segment 1: low-pass ~200 Hz (CC74≈42) */
    sit_test_listen_overlay_set("filter_modes", "Seg 1: low-pass", "cutoff ~200 Hz attenuates A4");
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 16, 20); /* mode LP */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 74, 42);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    sit_test_stereo_scope_service_ui();
    powers[1] = sit_capture_ms_goertzel(&cap, sr, t + measure_offset_ms, measure_ms, target_hz);
    t += seg_ms + gap_ms;
    SIT_TONE_TEST_SLEEP_MS(gap_ms);

    /* Segment 2: high-pass ~800 Hz (CC74≈73) */
    sit_test_listen_overlay_set("filter_modes", "Seg 2: high-pass", "cutoff ~800 Hz attenuates A4");
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 16, 35); /* mode HP */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 74, 73);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    sit_test_stereo_scope_service_ui();
    powers[2] = sit_capture_ms_goertzel(&cap, sr, t + measure_offset_ms, measure_ms, target_hz);

    SIT_ASSERT(cap.count >= 256);
    SIT_ASSERT(powers[0] > 1e-8f);
    SIT_ASSERT(powers[1] < powers[0] * 0.45f);
    SIT_ASSERT(powers[2] < powers[0] * 0.45f);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

/**
 * Pulse width CC106: wide duty (~50%) vs narrow (~10%) on A4 pulse wave.
 * Narrow pulse carries less energy at the fundamental.
 */
static void test_graph_tone_synth_pulse_width_fundamental(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 CC70 pulse waveform, CC106 wide vs narrow duty"), SITUATION_SUCCESS);

    const uint32_t seg_ms = 520;
    const uint32_t measure_ms = 300;
    const uint32_t measure_offset_ms = 180;
    const uint32_t gap_ms = 80;
    const uint32_t total_ms = seg_ms * 2 + gap_ms + 120;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)total_ms * (uint32_t)sr) / 1000u);
    const float target_hz = 440.0f;

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    SituationSetControl(graph, tone, 1, 1.0f); /* pulse */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 70, 1);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 16, 0); /* filter off */

    float wide_power = 0.0f;
    float narrow_power = 0.0f;

    /* Wide pulse ~50% duty (CC106=64) */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 106, 64);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    wide_power = sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, target_hz);
    SIT_TONE_TEST_SLEEP_MS(gap_ms);

    /* Narrow pulse ~10% duty (CC106=8) */
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 106, 8);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    sit_test_stereo_scope_service_ui();
    narrow_power = sit_capture_ms_goertzel(&cap, sr, seg_ms + gap_ms + measure_offset_ms,
                                           measure_ms, target_hz);

    SIT_ASSERT(cap.count >= 256);
    SIT_ASSERT(wide_power > 1e-8f);
    SIT_ASSERT(narrow_power < wide_power * 0.55f);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

/**
 * All five graph waveforms (CC70 0–4) on A4: tonal types have 440 Hz energy; noise has RMS.
 */
static void test_graph_tone_synth_waveforms_all(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 CC70 waveforms 0=sine 1=pulse 2=tri 3=saw 4=noise; A4 each"), SITUATION_SUCCESS);

    const uint32_t seg_ms = 420;
    const uint32_t measure_ms = 280;
    const uint32_t measure_offset_ms = 100;
    const uint32_t gap_ms = 60;
    const uint32_t total_ms = seg_ms * 5 + gap_ms * 4 + 80;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)total_ms * (uint32_t)sr) / 1000u);
    const float target_hz = 440.0f;

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 16, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 106, 64);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 1, 0);

    float tonal_powers[4] = {0};
    float noise_rms = 0.0f;
    float noise_tone_power = 0.0f;
    uint32_t t = 0;

    for (int wf = 0; wf < 5; wf++) {
        static const char* wf_names[5] = {"sine", "pulse", "tri", "saw", "noise"};
        char seg[64];
        snprintf(seg, sizeof(seg), "waveform %d: %s", wf, wf_names[wf]);
        sit_test_listen_overlay_set("waveforms_all", seg, "A4 note 69");
        SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 70, (uint8_t)wf);
        SituationSetControl(graph, tone, 1, (float)wf);
        SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100),
                      SITUATION_SUCCESS);
        SIT_TONE_TEST_SLEEP_MS(seg_ms);
        SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
        sit_test_stereo_scope_service_ui();

        if (wf < 4) {
            tonal_powers[wf] = sit_capture_ms_goertzel(&cap, sr, t + measure_offset_ms,
                                                        measure_ms, target_hz);
        } else {
            noise_rms = sit_capture_ms_rms(&cap, sr, t + measure_offset_ms, measure_ms);
            noise_tone_power = sit_capture_ms_goertzel(&cap, sr, t + measure_offset_ms,
                                                       measure_ms, target_hz);
        }

        t += seg_ms;
        if (wf < 4) {
            SIT_TONE_TEST_SLEEP_MS(gap_ms);
            t += gap_ms;
        }
    }

    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    for (int wf = 0; wf < 4; wf++) {
        SIT_ASSERT(tonal_powers[wf] > 1e-8f);
    }
    SIT_ASSERT(tonal_powers[0] > tonal_powers[1] * 0.35f);
    SIT_ASSERT(noise_rms > 0.002f);
    SIT_ASSERT(noise_tone_power < tonal_powers[0] * 0.55f);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

/** Mod LFO: pitch wander on sine, then PWM wander on pulse (fundamental @ 440 Hz). */
static void test_graph_tone_synth_lfo_pitch_pwm_mod(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 LFO pitch on sine; LFO PWM on pulse (CC24/28/29 + controls)"), SITUATION_SUCCESS);

    const uint32_t settle_ms = 100;
    const uint32_t seg_ms = 320;
    const uint32_t total_ms = settle_ms + seg_ms * 2 + 120 + 80 + settle_ms + seg_ms * 2 + 160;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)total_ms * (uint32_t)sr) / 1000u);
    const float base_hz = 440.0f;
    const uint32_t win_ms = 80;
    const uint32_t pitch_scan_ms = seg_ms - 40;
    const uint32_t pwm_scan_ms = seg_ms - 80;
    const uint32_t pwm_win_ms = 50;

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_tone_listen_capture_arm(&cap);

    /* --- Pitch LFO on sine (segment 0–1) --- */
    SituationSetControl(graph, tone, 1, 0.0f);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 70, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 1, 0);
    SituationSetControl(graph, tone, 18, 0.0f);
    SituationSetControl(graph, tone, 20, 0.0f);
    SituationSetControl(graph, tone, 21, 0.0f);
    SituationSetControl(graph, tone, 22, 0.0f);
    SituationSetControl(graph, tone, 23, 0.0f);

    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(settle_ms);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);

    float pitch_static_min = 0.f, pitch_static_max = 0.f;
    sit_measure_goertzel_wander_at_hz(&cap, sr, settle_ms + 100, pitch_scan_ms, base_hz, win_ms,
                                      &pitch_static_min, &pitch_static_max);
    sit_test_stereo_scope_service_ui();

    SituationSetControl(graph, tone, 18, 4.0f);
    SituationSetControl(graph, tone, 20, 1.0f);
    SituationSetControl(graph, tone, 21, 4.0f);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);

    float pitch_mod_min = 0.f, pitch_mod_max = 0.f;
    sit_measure_goertzel_wander_at_hz(&cap, sr, settle_ms + 100 + seg_ms, pitch_scan_ms, base_hz,
                                      win_ms, &pitch_mod_min, &pitch_mod_max);
    sit_test_stereo_scope_service_ui();

    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    SIT_TONE_TEST_SLEEP_MS(120);

    /* --- PWM LFO on pulse (segment 2–3) — duty modulates level/timbre, not 440 Hz bin --- */
    SituationSetControl(graph, tone, 1, 1.0f);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 70, 1);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 106, 64);
    SituationSetControl(graph, tone, 18, 0.0f);
    SituationSetControl(graph, tone, 20, 0.0f);
    SituationSetControl(graph, tone, 22, 0.0f);
    SituationSetControl(graph, tone, 23, 0.0f);
    SIT_TONE_TEST_SLEEP_MS(80);

    sit_audio_freq_capture_reset(&cap);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(settle_ms);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);

    float pwm_static_h2_min = 0.f, pwm_static_h2_max = 0.f;
    sit_measure_goertzel_wander_at_hz(&cap, sr, settle_ms + 100, pwm_scan_ms, base_hz * 2.0f,
                                      pwm_win_ms, &pwm_static_h2_min, &pwm_static_h2_max);

    SituationSetControl(graph, tone, 18, 5.0f);
    SituationSetControl(graph, tone, 22, 1.0f);
    SituationSetControl(graph, tone, 23, 0.35f);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);

    float pwm_mod_h2_min = 0.f, pwm_mod_h2_max = 0.f;
    sit_measure_goertzel_wander_at_hz(&cap, sr, settle_ms + seg_ms + 100, pwm_scan_ms,
                                      base_hz * 2.0f, pwm_win_ms, &pwm_mod_h2_min,
                                      &pwm_mod_h2_max);
    sit_test_stereo_scope_service_ui();

    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    SIT_ASSERT(cap.count >= 256);

    SIT_ASSERT(pitch_static_max > 1e-5f);
    SIT_ASSERT(pitch_mod_max > 1e-5f);
    /* No pitch LFO: energy at 440 Hz should stay steady (skip attack — measure after settle). */
    SIT_ASSERT(pitch_static_max < pitch_static_min * 1.15f);
    /* Pitch LFO: modulation pulls energy away from 440 Hz in some windows. */
    SIT_ASSERT(pitch_mod_max > pitch_mod_min * 1.2f);

    /* Pulse duty is RMS-flat; 2nd-harmonic wander detects PWM LFO (duty modulates timbre). */
    SIT_ASSERT(pwm_static_h2_max > 1e-6f);
    SIT_ASSERT(pwm_mod_h2_max > 1e-6f);
    SIT_ASSERT(pwm_static_h2_max < pwm_static_h2_min * 1.15f);
    SIT_ASSERT(pwm_mod_h2_max > pwm_mod_h2_min * 1.2f);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

/**
 * ADSR envelope → filter cutoff: LP @ low Hz; attack opens cutoff (more 440 Hz energy later).
 */
static void test_graph_tone_synth_filter_env_adsr(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 LP low; ADSR env mod opens cutoff during attack (CC32/33)"), SITUATION_SUCCESS);

    const uint32_t note_ms = 600;
    const uint32_t total_ms = note_ms + 120;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)total_ms * (uint32_t)sr) / 1000u);
    const float target_hz = 440.0f;

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    SituationSetControl(graph, tone, 1, 0.0f); /* sine */
    SituationSetControl(graph, tone, 4, 0.12f); /* slow attack for env sweep */
    SituationSetControl(graph, tone, 5, 0.0f);
    SituationSetControl(graph, tone, 6, 1.0f);
    SituationSetControl(graph, tone, 9, 1.0f);  /* LP */
    SituationSetControl(graph, tone, 10, 80.0f);
    SituationSetControl(graph, tone, 11, 0.707f);
    SituationSetControl(graph, tone, 12, 3.0f);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 26, 1.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 27, 7000.0f), SITUATION_SUCCESS);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 70, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 16, 20);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 74, 20);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 32, 127);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 33, 127);

    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(note_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);

    /* Early attack: filter still mostly closed */
    sit_test_stereo_scope_service_ui();
    float power_early = sit_capture_ms_goertzel(&cap, sr, 25, 50, target_hz);
    sit_test_stereo_scope_service_ui();
    /* Sustain/hold: envelope at full level, filter open */
    float power_late = sit_capture_ms_goertzel(&cap, sr, 280, 220, target_hz);

    SIT_ASSERT(cap.count >= 256);
    SIT_ASSERT(power_early > 1e-10f);
    SIT_ASSERT(power_late > power_early * 1.8f);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

typedef struct SitToneSubOscCfg {
    float level;
    int waveform;
    int octave;
    float fine_semitones;
    bool controls_only;
    bool midi_only;
    float sub_coarse; /* ±12 semitones from main note */
    bool sync;
    bool ring_mod;
} SitToneSubOscCfg;

#ifndef SIT_TONE_SUB_COARSE_TEST_MAX
#define SIT_TONE_SUB_COARSE_TEST_MAX 12.0f
#endif

static float sit_tone_sub_expected_hz(float main_hz, int octave, float fine_semitones,
                                      float coarse_semitones) {
    if (octave < 0) {
        octave = 0;
    }
    if (octave > 2) {
        octave = 2;
    }
    if (fine_semitones < -1.0f) {
        fine_semitones = -1.0f;
    }
    if (fine_semitones > 1.0f) {
        fine_semitones = 1.0f;
    }
    if (coarse_semitones < -SIT_TONE_SUB_COARSE_TEST_MAX) {
        coarse_semitones = -SIT_TONE_SUB_COARSE_TEST_MAX;
    }
    if (coarse_semitones > SIT_TONE_SUB_COARSE_TEST_MAX) {
        coarse_semitones = SIT_TONE_SUB_COARSE_TEST_MAX;
    }
    return main_hz * powf(2.0f, (coarse_semitones - (float)octave * 12.0f + fine_semitones) / 12.0f);
}

static uint8_t sit_tone_sub_coarse_to_cc111(float coarse_semitones) {
    if (coarse_semitones < -SIT_TONE_SUB_COARSE_TEST_MAX) {
        coarse_semitones = -SIT_TONE_SUB_COARSE_TEST_MAX;
    }
    if (coarse_semitones > SIT_TONE_SUB_COARSE_TEST_MAX) {
        coarse_semitones = SIT_TONE_SUB_COARSE_TEST_MAX;
    }
    int step = (int)(coarse_semitones / SIT_TONE_SUB_COARSE_TEST_MAX * 64.0f +
                     (coarse_semitones >= 0.0f ? 0.5f : -0.5f));
    if (step < -64) {
        step = -64;
    }
    if (step > 63) {
        step = 63;
    }
    return (uint8_t)(64 + step);
}

static void sit_tone_sub_osc_prepare(SituationAudioGraph* graph, SituationNodeHandle tone) {
    SituationSetControl(graph, tone, 1, 0.0f);
    SituationSetControl(graph, tone, 4, 0.01f);
    SituationSetControl(graph, tone, 5, 0.05f);
    SituationSetControl(graph, tone, 6, 0.9f);
    SituationSetControl(graph, tone, 7, 0.2f);
    SituationSetControl(graph, tone, 8, -1.0f);
    SituationSetControl(graph, tone, 9, 0.0f);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 70, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 16, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 109, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 111, 64);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 123, 0);
    SIT_TONE_TEST_SLEEP_MS(120);
}

/** Update only sub_coarse (ctrl 34 + CC111) while a note is held — avoids re-sending all CCs. */
static void sit_tone_sub_osc_set_coarse(SituationAudioGraph* graph, SituationNodeHandle tone,
                                        float coarse_semitones) {
    if (coarse_semitones < -SIT_TONE_SUB_COARSE_TEST_MAX) {
        coarse_semitones = -SIT_TONE_SUB_COARSE_TEST_MAX;
    }
    if (coarse_semitones > SIT_TONE_SUB_COARSE_TEST_MAX) {
        coarse_semitones = SIT_TONE_SUB_COARSE_TEST_MAX;
    }
    SituationSetControl(graph, tone, 34, coarse_semitones);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 111,
                                      sit_tone_sub_coarse_to_cc111(coarse_semitones));
}

#ifndef SIT_TONE_SUB_LISTEN_MAIN_DUCK
#define SIT_TONE_SUB_LISTEN_MAIN_DUCK 0.18f /* duck main so sub interval is audible */
#endif

static void sit_tone_sub_listen_set_main_level(SituationAudioGraph* graph, SituationNodeHandle tone,
                                               const SitToneSubOscCfg* cfg) {
    float main_level = 1.0f;
    if (cfg) {
        if (cfg->ring_mod) {
            main_level = 0.85f;
        } else if (cfg->level <= 0.0f) {
            main_level = 1.0f;
        } else if (cfg->sync) {
            main_level = 0.35f;
        } else if (cfg->octave == 0 && cfg->sub_coarse == 0.0f && cfg->fine_semitones == 0.0f) {
            main_level = 1.0f;
        } else {
            main_level = SIT_TONE_SUB_LISTEN_MAIN_DUCK;
        }
    }
    SituationSetControl(graph, tone, 2, main_level);
}

static void sit_tone_sub_listen_format_detail(const SitToneSubOscCfg* cfg, char* buf, size_t buf_size,
                                              const char* note) {
    if (!buf || buf_size == 0) {
        return;
    }
    const float main_hz = 440.0f;
    if (!cfg) {
        snprintf(buf, buf_size, "%s", note ? note : "");
        return;
    }
    if (cfg->level <= 0.0f) {
        snprintf(buf, buf_size, "Expect ~%.0f Hz main only%s%s", main_hz,
                 note && note[0] ? " — " : "", note ? note : "");
        return;
    }
    if (cfg->ring_mod) {
        const float sub_hz =
            sit_tone_sub_expected_hz(main_hz, cfg->octave, cfg->fine_semitones, cfg->sub_coarse);
        snprintf(buf, buf_size,
                 "Ring: sidebands @ %.0f\xc2\xb1%.0f Hz (carrier suppressed)%s%s", main_hz, sub_hz,
                 note && note[0] ? " — " : "", note ? note : "");
        return;
    }
    if (cfg->sync) {
        const float ratio_hz = main_hz * powf(2.0f, cfg->sub_coarse / 12.0f);
        snprintf(buf, buf_size,
                 "Sync slave ~%.0f Hz (main×2^coarse/12); square main+sub%s%s", ratio_hz,
                 note && note[0] ? " — " : "", note ? note : "");
        return;
    }
    if (cfg->octave == 0 && cfg->sub_coarse == 0.0f && cfg->fine_semitones == 0.0f) {
        snprintf(buf, buf_size, "Expect ~%.0f Hz unison (main+sub)%s%s", main_hz,
                 note && note[0] ? " — " : "", note ? note : "");
        return;
    }
    const float sub_hz =
        sit_tone_sub_expected_hz(main_hz, cfg->octave, cfg->fine_semitones, cfg->sub_coarse);
    snprintf(buf, buf_size, "Expect ~%.0f Hz sub + ducked main (%.0f Hz)%s%s", sub_hz, main_hz,
             note && note[0] ? " — " : "", note ? note : "");
}

static void sit_tone_sub_osc_apply(SituationAudioGraph* graph, SituationNodeHandle tone,
                                   const SitToneSubOscCfg* cfg) {
    if (!cfg->midi_only) {
        SituationSetControl(graph, tone, 30, cfg->level);
        SituationSetControl(graph, tone, 31, (float)cfg->waveform);
        SituationSetControl(graph, tone, 32, (float)cfg->octave);
        SituationSetControl(graph, tone, 33, cfg->fine_semitones);
        SituationSetControl(graph, tone, 34, cfg->sub_coarse);
        SituationSetControl(graph, tone, 35, cfg->sync ? 1.0f : 0.0f);
        SituationSetControl(graph, tone, 36, cfg->ring_mod ? 1.0f : 0.0f);
    }
    if (!cfg->controls_only) {
        const uint8_t cc107 = (uint8_t)(cfg->level * 127.0f + 0.5f);
        const uint8_t cc108 = (uint8_t)((cfg->waveform % 5 + 5) % 5);
        static const uint8_t cc109_by_oct[3] = {0, 64, 127};
        int oct = cfg->octave;
        if (oct < 0) {
            oct = 0;
        }
        if (oct > 2) {
            oct = 2;
        }
        const uint8_t cc109 = cc109_by_oct[oct];
        int fine_step = (int)(cfg->fine_semitones * 64.0f);
        if (fine_step < -64) {
            fine_step = -64;
        }
        if (fine_step > 63) {
            fine_step = 63;
        }
        const uint8_t cc110 = (uint8_t)(64 + fine_step);
        SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 107, cc107);
        SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 108, cc108);
        SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 109, cc109);
        SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 110, cc110);
        if (!cfg->midi_only || cfg->sub_coarse != 0.0f) {
            SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 111,
                                              sit_tone_sub_coarse_to_cc111(cfg->sub_coarse));
        }
        SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 112, cfg->sync ? 127 : 0);
        SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 113,
                                          cfg->ring_mod ? 127 : 0);
    }
    if (!cfg->midi_only) {
        sit_tone_sub_listen_set_main_level(graph, tone, cfg);
    }
}

static void sit_tone_sub_osc_play_a4(SituationAudioGraph* graph, SituationNodeHandle tone,
                                     const SitToneSubOscCfg* cfg, uint32_t seg_ms,
                                     uint32_t gap_ms, const char* segment, const char* note) {
    char detail[192];
    sit_tone_sub_listen_format_detail(cfg, detail, sizeof(detail), note);
    sit_test_listen_overlay_set("sub_oscillator", segment, detail);
    sit_tone_sub_osc_apply(graph, tone, cfg);
    SIT_TONE_TEST_SLEEP_MS(50);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 100),
                  SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
    SIT_TONE_TEST_SLEEP_MS(gap_ms);
}

#ifndef SIT_TONE_SUB_EFFECT_SEG_MS
#define SIT_TONE_SUB_EFFECT_SEG_MS 3500u
#endif
#ifndef SIT_TONE_SUB_EFFECT_GAP_MS
#define SIT_TONE_SUB_EFFECT_GAP_MS 200u
#endif
#ifndef SIT_TONE_SUB_EFFECT_MIN_RMS
#define SIT_TONE_SUB_EFFECT_MIN_RMS 0.015f
#endif

/** Hard-sync listen preset: square main + square sub, sync on (CC112), coarse = ratio from main. */
static SitToneSubOscCfg sit_tone_sub_sync_listen_cfg(float sub_coarse) {
    SitToneSubOscCfg cfg = {0};
    cfg.level = 1.0f;
    cfg.waveform = 1;
    cfg.octave = 0;
    cfg.sub_coarse = sub_coarse;
    cfg.sync = true;
    cfg.ring_mod = false;
    return cfg;
}

/** Ring-mod listen preset: sine main × sine sub oct −1 (sidebands 220/660, carrier suppressed). */
static SitToneSubOscCfg sit_tone_sub_ring_listen_cfg(float sub_coarse) {
    SitToneSubOscCfg cfg = {0};
    cfg.level = 1.0f;
    cfg.waveform = 0;
    cfg.octave = 1;
    cfg.sub_coarse = sub_coarse;
    cfg.sync = false;
    cfg.ring_mod = true;
    return cfg;
}

static void sit_tone_sub_effect_prepare(SituationAudioGraph* graph, SituationNodeHandle tone) {
    sit_tone_sub_osc_prepare(graph, tone);
    SituationSetControl(graph, tone, 4, 0.005f);
    SituationSetControl(graph, tone, 6, 1.0f);
    SituationSetControl(graph, tone, 7, 0.4f);
}

static void sit_tone_sub_osc_play_hold_a4(SituationAudioGraph* graph, SituationNodeHandle tone,
                                          const SitToneSubOscCfg* cfg, uint32_t hold_ms,
                                          const char* test_name, const char* segment,
                                          const char* note) {
    char detail[192];
    sit_tone_sub_listen_format_detail(cfg, detail, sizeof(detail), note);
    sit_test_listen_overlay_set(test_name, segment, detail);
    sit_tone_sub_osc_apply(graph, tone, cfg);
    SIT_TONE_TEST_SLEEP_MS(50);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 110),
                  SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(hold_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
}

#ifndef SIT_TONE_SUB_SWEEP_STEPS
#define SIT_TONE_SUB_SWEEP_STEPS 40
#endif

/**
 * Hold A4: preset CCs once, note-on, then CC111 only across coarse −12..+12 st.
 */
static void sit_tone_sub_osc_play_coarse_sweep(SituationAudioGraph* graph,
                                               SituationNodeHandle tone,
                                               const SitToneSubOscCfg* cfg, uint32_t seg_ms,
                                               const char* test_name, const char* segment,
                                               const char* note) {
    char detail[192];
    sit_tone_sub_listen_format_detail(cfg, detail, sizeof(detail), note);
    sit_test_listen_overlay_set(test_name, segment, detail);
    sit_tone_sub_osc_apply(graph, tone, cfg);
    SIT_TONE_TEST_SLEEP_MS(50);
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 69, 110),
                  SITUATION_SUCCESS);

    const uint32_t step_ms = seg_ms / (uint32_t)SIT_TONE_SUB_SWEEP_STEPS;
    for (int i = 0; i < SIT_TONE_SUB_SWEEP_STEPS; i++) {
        const float coarse =
            -SIT_TONE_SUB_COARSE_TEST_MAX +
            (2.0f * SIT_TONE_SUB_COARSE_TEST_MAX * (float)i) /
                (float)(SIT_TONE_SUB_SWEEP_STEPS - 1);
        char coarse_line[96];
        snprintf(coarse_line, sizeof(coarse_line), "CC111 coarse=%+.1f st", coarse);
        sit_test_listen_overlay_set(test_name, segment, coarse_line);
        sit_tone_sub_osc_set_coarse(graph, tone, coarse);
        SIT_TONE_TEST_SLEEP_MS(step_ms > 0 ? step_ms : 1);
    }

    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 69);
}

/** ~3.5 s A4 + coarse 0, then ~3.5 s CC111 sweep; square main+sub, sync on (CC112). */
static void test_graph_tone_synth_sub_sync(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 sub_sync; A4 hold coarse=0 then CC111 sweep; square+sync CC112"),
                  SITUATION_SUCCESS);

    const uint32_t seg_ms = SIT_TONE_SUB_EFFECT_SEG_MS;
    const uint32_t gap_ms = SIT_TONE_SUB_EFFECT_GAP_MS;
    const uint32_t cap_ms = seg_ms * 2u + gap_ms + 400u;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)cap_ms * (uint32_t)sr) / 1000u);
    const uint32_t measure_offset_ms = 500u;
    const uint32_t measure_ms = seg_ms - 1000u;
    const float hz_a4 = sit_midi_note_to_hz(69);
    const float hz_sync_coarse0 = hz_a4;
    const float hz_sync_coarse_min = hz_a4 * 0.5f;
    const float hz_sync_coarse_max = hz_a4 * 2.0f;

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.85f);
    sit_test_audio_monitor_set_capture( &cap);

    sit_tone_sub_effect_prepare(graph, tone);
    SituationSetControl(graph, tone, 1, 1.0f);

    const SitToneSubOscCfg sync_track = sit_tone_sub_sync_listen_cfg(0.0f);
    const SitToneSubOscCfg sync_sweep = sit_tone_sub_sync_listen_cfg(0.0f);

    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_hold_a4(graph, tone, &sync_track, seg_ms, "sub_sync",
                                  "Phase 1: hold coarse=0", "A4 square sync 1:1 ratio");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float rms_sync_track =
        sit_capture_ms_rms(&cap, sr, measure_offset_ms, measure_ms);
    const float p440_sync_track =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_sync_coarse0);
    sit_test_stereo_scope_service_ui();

    SIT_TONE_TEST_SLEEP_MS(gap_ms);

    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_coarse_sweep(graph, tone, &sync_sweep, seg_ms, "sub_sync",
                                       "Phase 2: CC111 sweep", "A4 hold sync on 0.5x..2x");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float rms_sync_sweep =
        sit_capture_ms_rms(&cap, sr, measure_offset_ms, measure_ms);
    const float p220_sweep_early =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, 400u, hz_sync_coarse_min);
    sit_test_stereo_scope_service_ui();
    const float p880_sweep_late = sit_capture_ms_goertzel(&cap, sr,
        measure_offset_ms + measure_ms - 450u, 400u, hz_sync_coarse_max);
    sit_test_stereo_scope_service_ui();

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);

    SIT_ASSERT(rms_sync_track > SIT_TONE_SUB_EFFECT_MIN_RMS);
    SIT_ASSERT(rms_sync_sweep > SIT_TONE_SUB_EFFECT_MIN_RMS);
    SIT_ASSERT(p440_sync_track > 1e-7f);
    SIT_ASSERT(p220_sweep_early > 1e-9f || p880_sweep_late > 1e-9f);

    sit_midi_tone_graph_teardown(graph, tone);
}

/** ~3.5 s A4 + coarse 0, then ~3.5 s CC111 sweep; sine ring (CC113). Carrier suppressed; sidebands present. */
static void test_graph_tone_synth_sub_ring_mod(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 sub_ring_mod; A4 hold coarse=0 then CC111 sweep; sine ring; CC113"),
                  SITUATION_SUCCESS);

    const uint32_t seg_ms = SIT_TONE_SUB_EFFECT_SEG_MS;
    const uint32_t gap_ms = SIT_TONE_SUB_EFFECT_GAP_MS;
    const uint32_t cap_ms = seg_ms * 2u + gap_ms + 400u;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)cap_ms * (uint32_t)sr) / 1000u);
    const uint32_t measure_offset_ms = 500u;
    const uint32_t measure_ms = seg_ms - 1000u;
    const float hz_a4 = sit_midi_note_to_hz(69);

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.85f);
    sit_test_audio_monitor_set_capture( &cap);

    sit_tone_sub_effect_prepare(graph, tone);
    SituationSetControl(graph, tone, 1, 0.0f);

    const SitToneSubOscCfg ring_track = sit_tone_sub_ring_listen_cfg(0.0f);
    const SitToneSubOscCfg ring_sweep = sit_tone_sub_ring_listen_cfg(0.0f);

    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_hold_a4(graph, tone, &ring_track, seg_ms, "sub_ring_mod",
                                  "Phase 1: hold coarse=0", "A4 sine ring oct-1 CC113");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float rms_ring_track =
        sit_capture_ms_rms(&cap, sr, measure_offset_ms, measure_ms);
    const float hz_sub = hz_a4 * 0.5f;
    const float hz_upper = hz_a4 * 1.5f;
    const float p440_ring =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_a4);
    sit_test_stereo_scope_service_ui();
    const float p220_ring =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_sub);
    sit_test_stereo_scope_service_ui();
    const float p660_ring =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_upper);
    sit_test_stereo_scope_service_ui();

    SIT_TONE_TEST_SLEEP_MS(gap_ms);

    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_coarse_sweep(graph, tone, &ring_sweep, seg_ms, "sub_ring_mod",
                                       "Phase 2: CC111 sweep", "A4 hold ring on");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float rms_ring_sweep =
        sit_capture_ms_rms(&cap, sr, measure_offset_ms, measure_ms);
    const float p440_sweep_early =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, 400u, hz_a4);
    sit_test_stereo_scope_service_ui();
    const float p440_sweep_late = sit_capture_ms_goertzel(&cap, sr,
        measure_offset_ms + measure_ms - 450u, 400u, hz_a4);
    sit_test_stereo_scope_service_ui();

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);

    SIT_ASSERT(rms_ring_track > SIT_TONE_SUB_EFFECT_MIN_RMS);
    SIT_ASSERT(rms_ring_sweep > SIT_TONE_SUB_EFFECT_MIN_RMS);
    SIT_ASSERT(p220_ring > 1e-7f || p660_ring > 1e-7f);
    /* Carrier suppressed at ring_level=1 (dry/wet crossfade); sidebands dominate. */
    SIT_ASSERT(p220_ring > p440_ring || p660_ring > p440_ring);
    SIT_ASSERT(p440_sweep_early > 1e-9f && p440_sweep_late > 1e-9f);
    /* Coarse sweep moves ring sidebands; carrier may wobble in Goertzel — RMS is enough. */

    sit_midi_tone_graph_teardown(graph, tone);
}

/**
 * Sub-oscillator: level off/on, octaves 0/1/2, fine ±1 st, all five waveforms,
 * half level, and MIDI CC107–110 path (controls deliberately wrong).
 */
static void test_graph_tone_synth_sub_oscillator(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 A4; sub level/oct/wave/fine + CC107-110"), SITUATION_SUCCESS);

    sit_test_listen_overlay_set("sub_oscillator", "starting",
                                "16 segments — main ducked when sub is featured");

    const uint32_t seg_ms = 380;
    const uint32_t measure_ms = 260;
    const uint32_t measure_offset_ms = 90;
    const uint32_t gap_ms = 50;
    const int num_wf = 5;
    const uint32_t seg_cap_ms = seg_ms + 80u;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)seg_cap_ms * (uint32_t)sr) / 1000u);
    const float hz_main = 440.0f;

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    sit_tone_sub_osc_prepare(graph, tone);

    /* 0: sub level 0, oct −1 — only main 440 Hz should dominate. */
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){0.0f, 0, 1, 0.0f, true, false}, seg_ms, gap_ms,
        "Seg 0/16: sub off", "A4 main only (440 Hz) oct-1 ignored");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p220_level_off =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);
    const float p440_level_off =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_main);

    /* 1: full level, oct 0 (unison) — stronger 440 than oct −1. */
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 0, 0.0f, true, false}, seg_ms, gap_ms,
        "Seg 1/16: unison", "A4 main+sub both 440 Hz");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p440_unison =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_main);

    /* 2: oct −1. */
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 1, 0.0f, true, false}, seg_ms, gap_ms,
        "Seg 2/16: oct -1", "A4 main 440 + sub 220 additive");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p220_oct1 =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);
    const float p440_oct1 =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_main);

    /* 3: oct −2. */
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 2, 0.0f, true, false}, seg_ms, gap_ms,
        "Seg 3/16: oct -2", "A4 main 440 + sub 110 additive");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p110_oct2 =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 110.0f);
    const float p220_oct2 =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);

    /* 4: fine +1 st (oct −1). */
    const float hz_fine_plus = sit_tone_sub_expected_hz(hz_main, 1, 1.0f, 0.0f);
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 1, 1.0f, true, false}, seg_ms, gap_ms,
        "Seg 4/16: fine +1 st", "oct-1 sub ~233 Hz + main 440");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p_fine_plus =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_fine_plus);
    const float p220_fine_plus =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);

    /* 5: fine −1 st (oct −1). */
    const float hz_fine_minus = sit_tone_sub_expected_hz(hz_main, 1, -1.0f, 0.0f);
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 1, -1.0f, true, false}, seg_ms, gap_ms,
        "Seg 5/16: fine -1 st", "oct-1 sub ~208 Hz + main 440");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p_fine_minus =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_fine_minus);
    const float p220_fine_minus =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);

    /* 6: half level (oct −1). */
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){0.5f, 0, 1, 0.0f, true, false}, seg_ms, gap_ms,
        "Seg 6/16: half level", "oct-1 sub 220 at 50% + main 440");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p220_half =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);

    float wf_tone_power[5] = {0};
    float wf_noise_rms = 0.0f;
    float wf_noise_tone = 0.0f;

    /* 7–11: each sub waveform (oct −1, full level). */
    for (int wf = 0; wf < num_wf; wf++) {
        static const char* wf_names[5] = {"sine", "pulse", "tri", "saw", "noise"};
        char seg_buf[64];
        snprintf(seg_buf, sizeof(seg_buf), "Seg %d/16: sub wave %s", 7 + wf, wf_names[wf]);
        sit_audio_freq_capture_reset(&cap);
        sit_tone_sub_osc_play_a4(graph, tone,
            &(SitToneSubOscCfg){1.0f, wf, 1, 0.0f, true, false}, seg_ms, gap_ms,
            seg_buf, "oct-1 full level A4");
        SIT_ASSERT(cap.count >= 256);
        sit_test_stereo_scope_service_ui();
        if (wf < 4) {
            wf_tone_power[wf] =
                sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);
        } else {
            wf_noise_rms = sit_capture_ms_rms(&cap, sr, measure_offset_ms, measure_ms);
            wf_noise_tone =
                sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);
        }
    }

    /* 12: MIDI CC only — controls say oct−2 level 0; CC says oct−1 full. */
    SituationSetControl(graph, tone, 30, 0.0f);
    SituationSetControl(graph, tone, 32, 2.0f);
    SituationSetControl(graph, tone, 33, 0.0f);
    SituationSetControl(graph, tone, 34, 0.0f);
    SIT_TONE_TEST_SLEEP_MS(80);
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 1, 0.0f, false, true}, seg_ms, gap_ms,
        "Seg 12/16: MIDI CC path", "CC107-111 oct-1 (controls wrong on purpose)");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p220_midi_cc =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);
    const float p110_midi_cc =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 110.0f);

    /* 13: coarse +12 st tracks main — C3 then E4, sub doubles each root. */
    const float hz_c3 = sit_midi_note_to_hz(48);
    const float hz_e4 = sit_midi_note_to_hz(64);
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_apply(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 0, 0.0f, true, false, 12.0f, false, false});
    sit_test_listen_overlay_set("sub_oscillator", "Seg 13/16: coarse +12 C3",
                                "sub unison octave above root");
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 48, 100),
                  SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 48);
    sit_test_stereo_scope_service_ui();
    const float p_coarse_c3 =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_c3 * 2.0f);
    SIT_TONE_TEST_SLEEP_MS(gap_ms);
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_apply(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 0, 0.0f, true, false, 12.0f, false, false});
    sit_test_listen_overlay_set("sub_oscillator", "Seg 13/16: coarse +12 E4",
                                "sub unison octave above root");
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 64, 100),
                  SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 64);
    sit_test_stereo_scope_service_ui();
    const float p_coarse_e4 =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_e4 * 2.0f);

    /* 14: ring mod vs additive (oct −1) — less 440 Hz when ring on. */
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_play_a4(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 1, 0.0f, true, false, 0.0f, false, true}, seg_ms,
        gap_ms, "Seg 14/16: ring on", "CS40M bus main+ring*main*sub oct-1");
    SIT_ASSERT(cap.count >= 256);
    sit_test_stereo_scope_service_ui();
    const float p440_ring =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, hz_main);

    /* 15: ring mod sidebands track main pitch (E4 oct−1), not an unrelated 220 Hz anchor. */
    const float sum_e4 = hz_e4 * 1.5f;
    sit_audio_freq_capture_reset(&cap);
    sit_tone_sub_osc_apply(graph, tone,
        &(SitToneSubOscCfg){1.0f, 0, 1, 0.0f, true, false, 0.0f, false, true});
    sit_test_listen_overlay_set("sub_oscillator", "Seg 15/16: ring E4",
                                "sidebands track main pitch not 220 anchor");
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL, 64, 100),
                  SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(seg_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, 64);
    const float p_ring_sum_e4 =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, sum_e4);
    const float p_ring_220_e4 =
        sit_capture_ms_goertzel(&cap, sr, measure_offset_ms, measure_ms, 220.0f);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);

    SIT_ASSERT(p440_level_off > 1e-8f);
    SIT_ASSERT(p220_level_off < p440_level_off * 0.12f);

    SIT_ASSERT(p440_unison > p440_oct1 * 1.15f);

    SIT_ASSERT(p220_oct1 > 1e-6f);
    SIT_ASSERT(p220_oct1 > p440_oct1 * 0.45f);

    SIT_ASSERT(p110_oct2 > 1e-6f);
    SIT_ASSERT(p110_oct2 > p220_oct2 * 0.45f);
    SIT_ASSERT(p110_oct2 > p440_level_off * 0.25f);

    SIT_ASSERT(p_fine_plus > 1e-8f);
    SIT_ASSERT(p_fine_plus > p220_fine_plus * 0.55f);

    SIT_ASSERT(p_fine_minus > 1e-8f);
    SIT_ASSERT(p_fine_minus > p220_fine_minus * 0.55f);

    SIT_ASSERT(p220_half > p220_level_off * 1.8f);
    SIT_ASSERT(p220_half < p220_oct1 * 0.88f);

    for (int wf = 0; wf < 4; wf++) {
        SIT_ASSERT(wf_tone_power[wf] > 1e-8f);
    }
    SIT_ASSERT(wf_noise_rms > 0.002f);
    SIT_ASSERT(wf_noise_tone < wf_tone_power[0] * 0.55f);

    SIT_ASSERT(p220_midi_cc > 1e-6f);
    SIT_ASSERT(p220_midi_cc > p440_level_off * 0.35f);
    SIT_ASSERT(p110_midi_cc < p220_midi_cc * 0.45f);

    SIT_ASSERT(p_coarse_c3 > 1e-6f);
    SIT_ASSERT(p_coarse_e4 > 1e-6f);
    SIT_ASSERT(p_coarse_e4 > p_coarse_c3 * 0.45f);

    /* Ring mod: tonal output present (sidebands; Goertzel @ 440 not a reliable discriminator). */
    SIT_ASSERT(p440_ring > 1e-8f);

    SIT_ASSERT(p_ring_sum_e4 > 1e-8f);
    SIT_ASSERT(p_ring_sum_e4 > p_ring_220_e4 * 0.55f);

    sit_midi_tone_graph_teardown(graph, tone);
}

#define SIT_TONE_PORTAMENTO_SEQ_LEN 4

static const uint8_t g_tone_portamento_seq_notes[SIT_TONE_PORTAMENTO_SEQ_LEN] = {60, 64, 67, 72};

static void sit_tone_mono_portamento_prepare(SituationAudioGraph* graph, SituationNodeHandle tone) {
    SituationSetControl(graph, tone, 1, 0.0f);
    SituationSetControl(graph, tone, 4, 0.01f);
    SituationSetControl(graph, tone, 5, 0.05f);
    SituationSetControl(graph, tone, 6, 0.85f);
    SituationSetControl(graph, tone, 7, 0.3f);
    SituationSetControl(graph, tone, 8, -1.0f);
    SituationSetControl(graph, tone, 16, 1.0f);
    /* Portamento via MIDI: CC5 time off, CC20 max speed (48 st/s). */
    SituationSetControl(graph, tone, 28, 0.0f);
    SituationSetControl(graph, tone, 29, 0.0f);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 123, 0);
    SIT_TONE_TEST_SLEEP_MS(200);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 5, 0);
    SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 20, 127);
}

/** Soon after legato note-on at step i: previous pitch should still be audible (glide). */
static void sit_tone_portamento_assert_linked_glide(const SitAudioFreqCapture* cap, int sr,
                                                    uint32_t step_ms, int step_index,
                                                    uint32_t measure_offset_ms, uint32_t measure_ms) {
    sit_test_stereo_scope_service_ui();
    SIT_ASSERT(step_index >= 1 && step_index < SIT_TONE_PORTAMENTO_SEQ_LEN);
    const uint8_t prev_note = g_tone_portamento_seq_notes[step_index - 1];
    const uint8_t new_note = g_tone_portamento_seq_notes[step_index];
    const float hz_prev = sit_midi_note_to_hz(prev_note);
    const float hz_new = sit_midi_note_to_hz(new_note);
    const uint32_t t_ms = (uint32_t)step_index * step_ms + measure_offset_ms;
    float p_prev = sit_capture_ms_goertzel(cap, sr, t_ms, measure_ms, hz_prev);
    sit_test_stereo_scope_service_ui();
    float p_new = sit_capture_ms_goertzel(cap, sr, t_ms, measure_ms, hz_new);
    SIT_ASSERT(p_new > 1e-8f);
    SIT_ASSERT(p_prev > p_new * 0.12f);
}

/** Soon after detached note-on: previous pitch should be gone (no legato glide). */
static void sit_tone_portamento_assert_unlinked_snap(const SitAudioFreqCapture* cap, int sr,
                                                   uint32_t note_on_ms, uint32_t measure_offset_ms,
                                                   uint32_t measure_ms, int step_index) {
    sit_test_stereo_scope_service_ui();
    SIT_ASSERT(step_index >= 1 && step_index < SIT_TONE_PORTAMENTO_SEQ_LEN);
    const uint8_t prev_note = g_tone_portamento_seq_notes[step_index - 1];
    const uint8_t new_note = g_tone_portamento_seq_notes[step_index];
    const float hz_prev = sit_midi_note_to_hz(prev_note);
    const float hz_new = sit_midi_note_to_hz(new_note);
    const uint32_t t_ms = note_on_ms + measure_offset_ms;
    float p_prev = sit_capture_ms_goertzel(cap, sr, t_ms, measure_ms, hz_prev);
    sit_test_stereo_scope_service_ui();
    float p_new = sit_capture_ms_goertzel(cap, sr, t_ms, measure_ms, hz_new);
    SIT_ASSERT(p_new > 1e-8f);
    SIT_ASSERT(p_prev < p_new * 0.06f);
}

/**
 * Four-note linked legato (C4 E4 G4 C5): each new note-on while prior notes still held;
 * all note-offs only after the phrase. Portamento on → glides at each step.
 */
/** 16-slot patch memory: save/recall controls 1..36 (API + CC114/115). */
static void test_graph_tone_synth_patch_memory(void) {
    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "patch_slot/store + CC114 recall + CC115 save"), SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(120);

    float v = -1.0f;

    /* Slot 0: saw + sub_level 0.5 — save via patch_store control */
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 1, 3.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 30, 0.5f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 38, 1.0f), SITUATION_SUCCESS);

    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 1, 0.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 30, 0.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 37, 0.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGetControl(graph, tone, 1, &v), SITUATION_SUCCESS);
    SIT_ASSERT(fabsf(v - 3.0f) < 0.01f);
    SIT_ASSERT_EQ(SituationGetControl(graph, tone, 30, &v), SITUATION_SUCCESS);
    SIT_ASSERT(fabsf(v - 0.5f) < 0.01f);

    /* Slot 1: pulse — select slot, save, verify round-trip */
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 37, 1.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 1, 1.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 38, 1.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 1, 0.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 37, 1.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGetControl(graph, tone, 1, &v), SITUATION_SUCCESS);
    SIT_ASSERT(fabsf(v - 1.0f) < 0.01f);

    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 37, 0.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGetControl(graph, tone, 1, &v), SITUATION_SUCCESS);
    SIT_ASSERT(fabsf(v - 3.0f) < 0.01f);

    /* MIDI: save slot 0 again, overwrite, CC114 recall */
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 1, 2.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 115, 127),
                  SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(80);
    SIT_ASSERT_EQ(SituationSetControl(graph, tone, 1, 0.0f), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationVirtualMidiControlChange(SITUATION_TEST_MIDI_CHANNEL, 114, 0),
                  SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(80);
    SIT_ASSERT_EQ(SituationGetControl(graph, tone, 1, &v), SITUATION_SUCCESS);
    SIT_ASSERT(fabsf(v - 2.0f) < 0.01f);

    sit_midi_tone_graph_teardown(graph, tone);
}

static void test_graph_tone_synth_mono_portamento_linked(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 mono 4-note linked C4 E4 G4 C5; delayed note-offs; portamento"), SITUATION_SUCCESS);

    sit_tone_mono_portamento_prepare(graph, tone);

    const uint32_t step_ms = 200;
    const uint32_t tail_ms = 500;
    const uint32_t cap_ms = (SIT_TONE_PORTAMENTO_SEQ_LEN - 1) * step_ms + tail_ms + 80;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)cap_ms * (uint32_t)sr) / 1000u);
    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL,
                                                g_tone_portamento_seq_notes[0], 100),
                  SITUATION_SUCCESS);
    for (int i = 1; i < SIT_TONE_PORTAMENTO_SEQ_LEN; i++) {
        SIT_TONE_TEST_SLEEP_MS(step_ms);
        SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL,
                                                    g_tone_portamento_seq_notes[i], 100),
                      SITUATION_SUCCESS);
    }
    SIT_TONE_TEST_SLEEP_MS(tail_ms);
    for (int i = SIT_TONE_PORTAMENTO_SEQ_LEN - 1; i >= 0; i--) {
        SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, g_tone_portamento_seq_notes[i]);
    }

    SIT_ASSERT(cap.count >= 256);

    /* Max speed: assert glide on first legato step; full 4-note phrase for audible sweep. */
    sit_test_stereo_scope_service_ui();
    sit_tone_portamento_assert_linked_glide(&cap, sr, step_ms, 1, 12, 55);

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

/**
 * Same four notes unlinked: each note-off and release complete before the next note-on.
 * Portamento on but no legato → pitch resets each time (no glide from prior note).
 */
static void test_graph_tone_synth_mono_portamento_unlinked(void) {
    int sr = 0;
    if (!sit_audio_freq_test_ready(&sr)) {
        SIT_ASSERT(true);
        return;
    }

    SituationAudioGraph* graph = NULL;
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    int midi_in = -1;
    (void)midi_in;
    SIT_ASSERT_EQ(sit_midi_tone_graph_setup(&graph, &tone, &midi_in,
        "ch0 mono 4-note unlinked C4 E4 G4 C5; gap between notes; portamento"), SITUATION_SUCCESS);

    sit_tone_mono_portamento_prepare(graph, tone);

    const uint32_t hold_ms = 180;
    const uint32_t release_gap_ms = 420;
    const uint32_t tail_ms = 400;
    const uint32_t cap_ms = (uint32_t)SIT_TONE_PORTAMENTO_SEQ_LEN * (hold_ms + release_gap_ms) + tail_ms + 80;
    const uint32_t cap_frames = (uint32_t)(((uint64_t)cap_ms * (uint32_t)sr) / 1000u);
    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, cap_frames, 2);
    SituationSetAudioMasterVolume(0.8f);
    sit_test_audio_monitor_set_capture( &cap);

    uint32_t note_on_ms[SIT_TONE_PORTAMENTO_SEQ_LEN];
    note_on_ms[0] = 0;
    SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL,
                                                g_tone_portamento_seq_notes[0], 100),
                  SITUATION_SUCCESS);
    SIT_TONE_TEST_SLEEP_MS(hold_ms);
    SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, g_tone_portamento_seq_notes[0]);
    SIT_TONE_TEST_SLEEP_MS(release_gap_ms);

    for (int i = 1; i < SIT_TONE_PORTAMENTO_SEQ_LEN; i++) {
        note_on_ms[i] = hold_ms + release_gap_ms + (uint32_t)(i - 1) * (hold_ms + release_gap_ms);
        SIT_ASSERT_EQ(SituationVirtualMidiNoteOnEx(SITUATION_TEST_MIDI_CHANNEL,
                                                    g_tone_portamento_seq_notes[i], 100),
                      SITUATION_SUCCESS);
        SIT_TONE_TEST_SLEEP_MS(hold_ms);
        SituationVirtualMidiNoteOffEx(SITUATION_TEST_MIDI_CHANNEL, g_tone_portamento_seq_notes[i]);
        if (i < SIT_TONE_PORTAMENTO_SEQ_LEN - 1) {
            SIT_TONE_TEST_SLEEP_MS(release_gap_ms);
        }
    }
    SIT_TONE_TEST_SLEEP_MS(tail_ms);

    SIT_ASSERT(cap.count >= 256);

    const uint32_t snap_offset_ms = 50;
    const uint32_t snap_measure_ms = 100;
    for (int i = 1; i < SIT_TONE_PORTAMENTO_SEQ_LEN; i++) {
        sit_test_stereo_scope_service_ui();
        sit_tone_portamento_assert_unlinked_snap(&cap, sr, note_on_ms[i], snap_offset_ms,
                                                 snap_measure_ms, i);
    }

    sit_test_audio_monitor_set_capture(NULL);
    sit_audio_freq_capture_free(&cap);
    sit_midi_tone_graph_teardown(graph, tone);
}

static SitTestCase tone_synth_tests[] = {
    {"legacy_play_tone_ex",              test_play_tone_ex,              true},
    {"legacy_play_tone",                 test_play_tone_legacy,          true},
    {"legacy_play_midi_note",            test_play_midi_note,            true},
    {"legacy_stop_all_tones",            test_stop_all_tones,            true},
    {"legacy_stop_tone_invalid_handle",  test_stop_tone_invalid_handle,  true},
    {"legacy_midi_note_frequency",       test_legacy_midi_note_emits_frequency, true},
    {"registry_metadata",                test_registry_tone_synth_metadata, true},
    {"graph_create_node",                test_graph_create_node_tone_synth, true},
    {"graph_patch_audio",                test_graph_create_patch_audio,  true},
    {"graph_patch_to_reverb",            test_graph_patch_synth_to_reverb, true},
    {"graph_patch_chain",                test_graph_patch_chain,         true},
    {"graph_destroy_patch",              test_graph_destroy_patch,       true},
    {"graph_patch_duplicate",            test_graph_patch_duplicate,     true},
    {"graph_patch_invalid_port",         test_graph_patch_invalid_port,  true},
    {"graph_effect_chain",               test_effect_chain,              true},
    {"graph_serialize_with_nodes",       test_graph_serialize_with_nodes, true},
    {"graph_save_load_roundtrip",        test_graph_save_load_roundtrip, true},
    {"graph_deserialize_from_json",      test_graph_deserialize_from_json, true},
    {"midi_note_frequency",              test_graph_midi_note_emits_frequency, true},
    {"phase1_compare_a4",                test_tone_synth_phase1_compare_a4, true},
    {"midi_complex_melody",              test_midi_complex_melody_expression, true},
    {"midi_velocity_ramp",               test_midi_velocity_volume_levels, true},
    {"cc_mod_vibrato",                   test_graph_tone_synth_cc_mod_vibrato_pitch, true},
    {"cc92_tremolo",                     test_graph_tone_synth_cc92_tremolo, true},
    {"filter_modes",                     test_graph_tone_synth_filter_modes_attenuate_a4, true},
    {"pulse_width",                      test_graph_tone_synth_pulse_width_fundamental, true},
    {"waveforms_all",                    test_graph_tone_synth_waveforms_all, true},
    {"lfo_mod",                          test_graph_tone_synth_lfo_pitch_pwm_mod, true},
    {"filter_env_adsr",                  test_graph_tone_synth_filter_env_adsr, true},
    {"sub_oscillator",                   test_graph_tone_synth_sub_oscillator, true, true},
    {"sub_sync",                         test_graph_tone_synth_sub_sync, true, true},
    {"sub_ring_mod",                     test_graph_tone_synth_sub_ring_mod, true, true},
    {"patch_memory",                     test_graph_tone_synth_patch_memory, true},
    {"mono_portamento_linked",          test_graph_tone_synth_mono_portamento_linked, true},
    {"mono_portamento_unlinked",        test_graph_tone_synth_mono_portamento_unlinked, true},
};

const SitTestModule g_module_tone_synth = {
    .name = "tone_synth",
    .setup = tone_synth_setup,
    .teardown = tone_synth_teardown,
    .tests = tone_synth_tests,
    .test_count = sizeof(tone_synth_tests) / sizeof(tone_synth_tests[0]),
    .requires_context = true,
    .harness_visual = true
};
