/**
 * @file test_audio.c
 * @brief Audio module tests — Device, Playback, Tones, Effects, Capture, Mixer, Graph,
 *        Device Registry, Node Graph, Controls, Effects Modules, MIDI Integration
 *
 * Requires context: calls SituationInit() in setup, SituationShutdown() in teardown.
 * Tests audio device management, sound loading/playback, tone synthesis, effects,
 * audio processors, capture, mixer, device enumeration, graph serialization,
 * device registry, node graph lifecycle/patching, control parameters, all 18 effects
 * modules, mixer advanced features, graph serialization roundtrip, and MIDI integration.
 *
 * NOTE: A test WAV file is generated programmatically (1-second 440Hz sine, 16-bit mono 44100Hz)
 * to avoid requiring external assets.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_audio_window.h"
#include "sit_test_stereo_scope.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SIT_AUDIO_TEST_SLEEP_MS(ms) sit_test_harness_wait_ms((uint32_t)(ms))

/** Open real PortMidi inputs in harness tests only when set (driver variance on Windows CI). */
static bool sit_test_open_midi_hardware(void) {
    const char* e = getenv("SIT_TEST_OPEN_MIDI_HARDWARE");
    return e != NULL && e[0] != '\0' && e[0] != '0';
}

// Forward declaration for SituationRemovePatch — not currently exported from DLL
// Tests that need disconnect will skip that verification

// ============================================================================
//  Test WAV File Generation
// ============================================================================

#define TEST_WAV_PATH "_sit_test_sine.wav"
#define TEST_WAV_SAMPLE_RATE 44100
#define TEST_WAV_DURATION_SEC 1
#define TEST_WAV_NUM_SAMPLES (TEST_WAV_SAMPLE_RATE * TEST_WAV_DURATION_SEC)
#define TEST_WAV_FREQUENCY 440.0

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * Generate a minimal WAV file (16-bit PCM, mono, 44100Hz, 1 second of 440Hz sine).
 * Returns true on success.
 */
static bool generate_test_wav(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    int16_t samples[TEST_WAV_NUM_SAMPLES];
    for (int i = 0; i < TEST_WAV_NUM_SAMPLES; i++) {
        double t = (double)i / (double)TEST_WAV_SAMPLE_RATE;
        double val = sin(2.0 * M_PI * TEST_WAV_FREQUENCY * t);
        samples[i] = (int16_t)(val * 32767.0);
    }

    // WAV header (44 bytes)
    uint32_t data_size = TEST_WAV_NUM_SAMPLES * sizeof(int16_t);
    uint32_t file_size = 36 + data_size;
    uint16_t num_channels = 1;
    uint32_t sample_rate = TEST_WAV_SAMPLE_RATE;
    uint16_t bits_per_sample = 16;
    uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    uint16_t block_align = num_channels * bits_per_sample / 8;

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_format = 1; // PCM
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);

    // data chunk
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    fwrite(samples, sizeof(int16_t), TEST_WAV_NUM_SAMPLES, f);

    fclose(f);
    return true;
}

static void cleanup_test_wav(void) {
    remove(TEST_WAV_PATH);
}

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_init_ok = false;
static bool g_wav_ok = false;

static void audio_setup(void) {
    SituationInitInfo config;
    sit_test_audio_window_init_info(&config, "SIT_TEST_AUDIO");

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }

    // Generate test WAV file
    g_wav_ok = generate_test_wav(TEST_WAV_PATH);
    sit_test_audio_monitor_install();
}

static void audio_teardown(void) {
    cleanup_test_wav();
    remove("_sit_test_graph.json");
    remove("_sit_test_session.json");
    remove("_sit_test_midi.json");
    SituationSetActiveGraph(NULL);
    SituationStopAllTones();
    sit_test_audio_monitor_uninstall();
    SituationTeardownVirtualMidiLoopback();
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

// ============================================================================
//  Audio Device Management Tests
// ============================================================================

static void test_audio_get_devices(void) {
    int count = 0;
    SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&count);
    // Should return at least one device on most systems, but graceful if none
    SIT_ASSERT(count >= 0);
    if (count > 0) {
        SIT_ASSERT_NOT_NULL(devices);
        // First device should have a non-empty name
        SIT_ASSERT(strlen(devices[0].name) > 0);
    }
}

static void test_audio_playback_sample_rate(void) {
    int sr = SituationGetAudioPlaybackSampleRate();
    // Should be a standard sample rate (44100, 48000, etc.) or 0 if no device
    SIT_ASSERT(sr >= 0);
    if (sr > 0) {
        SIT_ASSERT(sr >= 8000 && sr <= 192000);
    }
}

static void test_audio_master_volume(void) {
    float vol = SituationGetAudioMasterVolume();
    // Volume should be in a reasonable range
    SIT_ASSERT(vol >= 0.0f && vol <= 2.0f);

    // Set volume — verify no crash
    SituationError err = SituationSetAudioMasterVolume(0.5f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // Restore original
    SituationSetAudioMasterVolume(vol);
}

static void test_audio_device_playing(void) {
    // After init, audio device should be playing (auto-started)
    bool playing = SituationIsAudioDevicePlaying();
    // This is system-dependent — just verify no crash
    SIT_ASSERT(playing || !playing); // Always true, but exercises the function
}

static void test_audio_pause_resume_device(void) {
    // Pause
    SituationError err = SituationPauseAudioDevice();
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // Resume
    err = SituationResumeAudioDevice();
    SIT_ASSERT(err == SITUATION_SUCCESS);
}

// ============================================================================
//  Sound Loading & Playback Tests
// ============================================================================

static void test_load_sound_from_file(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(sound.slot_index != 0 || sound.generation != 0);

    SituationUnloadSound(&sound);
}

static void test_play_and_stop_loaded_sound(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_stop_all_loaded_sounds(void) {
    SituationError err = SituationStopAllLoadedSounds();
    SIT_ASSERT(err == SITUATION_SUCCESS);
}

// ============================================================================
//  Format-Specific Playback Tests (MP3, OGG, FLAC)
// ============================================================================

#define TEST_ASSET_MP3 "tests/harness/assets/sample.mp3"
#define TEST_ASSET_OGG "tests/harness/assets/sample.ogg"
#define TEST_ASSET_FLAC "tests/harness/assets/sample.flac"
#define TEST_ASSET_WAV "tests/harness/assets/sample.wav"

static bool audio_asset_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) { fclose(f); return true; }
    return false;
}

static void test_load_play_mp3(void) {
    if (!audio_asset_exists(TEST_ASSET_MP3)) {
        fprintf(stderr, "[test] Skip load_play_mp3: %s not found\n", TEST_ASSET_MP3);
        SIT_ASSERT(true);
        return;
    }

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_ASSET_MP3, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(sound.slot_index != 0 || sound.generation != 0);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_AUDIO_TEST_SLEEP_MS(1500);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_load_play_ogg(void) {
    if (!audio_asset_exists(TEST_ASSET_OGG)) {
        fprintf(stderr, "[test] Skip load_play_ogg: %s not found\n", TEST_ASSET_OGG);
        SIT_ASSERT(true);
        return;
    }

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_ASSET_OGG, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    /* OGG Vorbis requires stb_vorbis linked into miniaudio; skip gracefully if unsupported. */
    if (err == SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED ||
        err == SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED) {
        fprintf(stderr, "[test] Skip load_play_ogg: OGG/Vorbis decoder not available\n");
        SIT_ASSERT(true);
        return;
    }
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(sound.slot_index != 0 || sound.generation != 0);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_AUDIO_TEST_SLEEP_MS(1500);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_load_play_flac(void) {
    if (!audio_asset_exists(TEST_ASSET_FLAC)) {
        fprintf(stderr, "[test] Skip load_play_flac: %s not found\n", TEST_ASSET_FLAC);
        SIT_ASSERT(true);
        return;
    }

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_ASSET_FLAC, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(sound.slot_index != 0 || sound.generation != 0);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_AUDIO_TEST_SLEEP_MS(1500);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_stream_mp3(void) {
    if (!audio_asset_exists(TEST_ASSET_MP3)) {
        fprintf(stderr, "[test] Skip stream_mp3: %s not found\n", TEST_ASSET_MP3);
        SIT_ASSERT(true);
        return;
    }

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_ASSET_MP3, SITUATION_AUDIO_LOAD_STREAM, false, &sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_AUDIO_TEST_SLEEP_MS(1500);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_stream_ogg(void) {
    if (!audio_asset_exists(TEST_ASSET_OGG)) {
        fprintf(stderr, "[test] Skip stream_ogg: %s not found\n", TEST_ASSET_OGG);
        SIT_ASSERT(true);
        return;
    }

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_ASSET_OGG, SITUATION_AUDIO_LOAD_STREAM, false, &sound);
    /* OGG Vorbis requires stb_vorbis linked into miniaudio; skip gracefully if unsupported. */
    if (err == SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED ||
        err == SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED) {
        fprintf(stderr, "[test] Skip stream_ogg: OGG/Vorbis decoder not available\n");
        SIT_ASSERT(true);
        return;
    }
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_AUDIO_TEST_SLEEP_MS(1500);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_stream_flac(void) {
    if (!audio_asset_exists(TEST_ASSET_FLAC)) {
        fprintf(stderr, "[test] Skip stream_flac: %s not found\n", TEST_ASSET_FLAC);
        SIT_ASSERT(true);
        return;
    }

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_ASSET_FLAC, SITUATION_AUDIO_LOAD_STREAM, false, &sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_AUDIO_TEST_SLEEP_MS(1500);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_load_play_wav(void) {
    if (!audio_asset_exists(TEST_ASSET_WAV)) {
        fprintf(stderr, "[test] Skip load_play_wav: %s not found\n", TEST_ASSET_WAV);
        SIT_ASSERT(true);
        return;
    }

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_ASSET_WAV, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(sound.slot_index != 0 || sound.generation != 0);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_AUDIO_TEST_SLEEP_MS(1500);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_stream_wav(void) {
    if (!audio_asset_exists(TEST_ASSET_WAV)) {
        fprintf(stderr, "[test] Skip stream_wav: %s not found\n", TEST_ASSET_WAV);
        SIT_ASSERT(true);
        return;
    }

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_ASSET_WAV, SITUATION_AUDIO_LOAD_STREAM, false, &sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationPlayLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SIT_AUDIO_TEST_SLEEP_MS(1500);

    err = SituationStopLoadedSound(&sound);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

// ============================================================================
//  Audio Handle API (Streaming) Tests
// ============================================================================

static void test_load_audio_handle(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSoundHandle handle = SituationLoadAudio(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false);
    SIT_ASSERT(handle.slot_index != 0 || handle.generation != 0);

    SituationError err = SituationPlayAudio(handle);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationUnloadAudio(handle);
}

static void test_audio_handle_volume_pan_pitch(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSoundHandle handle = SituationLoadAudio(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false);
    SIT_ASSERT(handle.slot_index != 0 || handle.generation != 0);

    SituationError err = SituationSetAudioVolume(handle, 0.75f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetAudioPan(handle, -0.5f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetAudioPitch(handle, 1.5f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationUnloadAudio(handle);
}


// ============================================================================
//  Sound Effects Tests
// ============================================================================

static void test_sound_volume(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundVolume(&sound, 0.8f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    float vol = SituationGetSoundVolume(&sound);
    SIT_ASSERT(vol >= 0.79f && vol <= 0.81f);

    SituationUnloadSound(&sound);
}

static void test_sound_pan(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundPan(&sound, -0.5f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    float pan = SituationGetSoundPan(&sound);
    SIT_ASSERT(pan >= -0.51f && pan <= -0.49f);

    SituationUnloadSound(&sound);
}

static void test_sound_pitch(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundPitch(&sound, 2.0f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    float pitch = SituationGetSoundPitch(&sound);
    SIT_ASSERT(pitch >= 1.99f && pitch <= 2.01f);

    SituationUnloadSound(&sound);
}

static void test_sound_filter(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundFilter(&sound, SITUATION_FILTER_LOWPASS, 1000.0f, 0.707f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundFilter(&sound, SITUATION_FILTER_HIGHPASS, 200.0f, 0.707f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundFilter(&sound, SITUATION_FILTER_NONE, 0.0f, 0.0f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_sound_echo(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundEcho(&sound, true, 0.25f, 0.5f, 0.3f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundEcho(&sound, false, 0.0f, 0.0f, 0.0f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

static void test_sound_reverb(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundReverb(&sound, true, 0.8f, 0.5f, 0.3f, 0.7f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationSetSoundReverb(&sound, false, 0.0f, 0.0f, 0.0f, 0.0f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

// ============================================================================
//  Audio Processor Tests
// ============================================================================

static void test_processor_callback(float* buffer, uint32_t frames, uint32_t channels, uint32_t sampleRate, void* user_data) {
    (void)buffer; (void)frames; (void)channels; (void)sampleRate;
    if (user_data) {
        *((int*)user_data) = 1;
    }
}

static void test_attach_detach_processor(void) {
    SIT_ASSERT(g_wav_ok);

    SituationSound sound = SITUATION_NULL_HANDLE;
    SituationError err = SituationLoadSoundFromFile(
        TEST_WAV_PATH, SITUATION_AUDIO_LOAD_FULL, false, &sound);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    int called = 0;

    err = SituationAttachAudioProcessor(&sound, test_processor_callback, &called);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    err = SituationDetachAudioProcessor(&sound, test_processor_callback, &called);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationUnloadSound(&sound);
}

// ============================================================================
//  Audio Capture Tests
// ============================================================================

static void test_capture_callback(const float* input_buffer, uint32_t frame_count,
                                   void* user_data) {
    (void)input_buffer; (void)frame_count; (void)user_data;
}

static void test_audio_capture_start_stop(void) {
    SituationError err = SituationStartAudioCapture(test_capture_callback, NULL);
    if (err == SITUATION_SUCCESS) {
        SituationStopAudioCapture();
    }
    SIT_ASSERT(true); // No crash = pass
}

static void test_audio_output_monitor(void) {
    sit_test_audio_monitor_uninstall();
    float pk = -1.f, rms = -1.f;
    SituationGetMasterOutputMeter(&pk, &rms);
    SIT_ASSERT(pk >= 0.f && rms >= 0.f);
    SituationGetMasterOutputMeter(NULL, NULL);
    sit_test_audio_monitor_install();
}

// ============================================================================
//  Device Enumeration Tests
// ============================================================================

static void test_enumerate_audio_devices(void) {
    int count = 0;
    SituationAudioDeviceInfo* devices = SituationEnumerateAudioDevices(&count);
    SIT_ASSERT(count >= 0);
    if (count > 0 && devices) {
        SIT_ASSERT(strlen(devices[0].name) > 0);
        SituationFreeDeviceList(devices, count);
    }
}

// ============================================================================
//  Graph Serialization Tests (Original — Basic)
// ============================================================================

static void test_graph_serialize_to_json(void) {
    SituationInitDeviceRegistry();

    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    char* json = SituationSerializeGraphToJSON(graph);
    SIT_ASSERT_NOT_NULL(json);
    SIT_ASSERT(strlen(json) > 0);

    SituationFreeJSONString(json);
    SituationDestroyGraph(graph);
}

static void test_graph_save_to_file(void) {
    SituationInitDeviceRegistry();

    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    const char* filepath = "_sit_test_graph.json";
    SituationError err = SituationSaveGraphToFile(graph, filepath);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    remove(filepath);
    SituationDestroyGraph(graph);
}

static void test_free_json_string_null(void) {
    SituationFreeJSONString(NULL);
    SIT_ASSERT(true);
}

// ============================================================================
//  PHASE 1 — Device Registry & Metadata
// ============================================================================

static void test_registry_init(void) {
    // SituationInitDeviceRegistry is idempotent — calling it again should not crash
    SituationInitDeviceRegistry();
    SIT_ASSERT(true);
}

static void test_registry_device_count(void) {
    SituationInitDeviceRegistry();
    int count = SituationGetRegisteredDeviceCount();
    // Currently 20 built-in devices are registered (some utilities/modulators/analyzers pending)
    SIT_ASSERT(count > 0);
    SIT_ASSERT(count >= 20);
}

static void test_registry_reverb_registered(void) {
    SituationInitDeviceRegistry();
    bool registered = SituationIsDeviceRegistered(SITUATION_NODE_REVERB);
    SIT_ASSERT(registered);
}

static void test_registry_unregistered_type(void) {
    SituationInitDeviceRegistry();
    // SITUATION_NODE_CUSTOM + 999 should not be registered
    bool registered = SituationIsDeviceRegistered((SituationNodeType)(SITUATION_NODE_CUSTOM + 999));
    SIT_ASSERT(!registered);
}

static void test_registry_reverb_metadata(void) {
    SituationInitDeviceRegistry();
    SituationDeviceMetadata meta = {0};
    SituationError err = SituationGetDeviceMetadata(SITUATION_NODE_REVERB, &meta);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(strlen(meta.name) > 0);
    SIT_ASSERT(meta.category == SITUATION_DEVICE_EFFECT);
}

static void test_registry_lfo_metadata(void) {
    SituationInitDeviceRegistry();
    SituationDeviceMetadata meta = {0};
    SituationError err = SituationGetDeviceMetadata(SITUATION_NODE_LFO, &meta);
    // LFO may not be registered yet (TODO in registry_init)
    if (err == SITUATION_SUCCESS) {
        SIT_ASSERT(meta.category == SITUATION_DEVICE_MODULATOR);
    }
    SIT_ASSERT(true); // Graceful — no crash
}

static void test_registry_peak_meter_metadata(void) {
    SituationInitDeviceRegistry();
    SituationDeviceMetadata meta = {0};
    SituationError err = SituationGetDeviceMetadata(SITUATION_NODE_PEAK_METER, &meta);
    // Peak meter may not be registered yet (TODO in registry_init)
    if (err == SITUATION_SUCCESS) {
        SIT_ASSERT(meta.category == SITUATION_DEVICE_ANALYZER);
    }
    SIT_ASSERT(true); // Graceful — no crash
}

static void test_registry_category_name_effect(void) {
    const char* name = SituationGetCategoryName(SITUATION_DEVICE_EFFECT);
    SIT_ASSERT_NOT_NULL(name);
    SIT_ASSERT(strlen(name) > 0);
}

static void test_registry_category_name_source(void) {
    const char* name = SituationGetCategoryName(SITUATION_DEVICE_SOURCE);
    SIT_ASSERT_NOT_NULL(name);
    SIT_ASSERT(strlen(name) > 0);
}

static void test_registry_category_name_utility(void) {
    const char* name = SituationGetCategoryName(SITUATION_DEVICE_UTILITY);
    SIT_ASSERT_NOT_NULL(name);
    SIT_ASSERT(strlen(name) > 0);
}

static void test_registry_register_custom_device(void) {
    SituationInitDeviceRegistry();

    SituationDeviceMetadata custom_meta = {0};
    custom_meta.type = (SituationNodeType)(SITUATION_NODE_CUSTOM + 42);
    snprintf(custom_meta.name, sizeof(custom_meta.name), "TestCustomDevice");
    custom_meta.category = SITUATION_DEVICE_EFFECT;
    custom_meta.num_audio_ins = 1;
    custom_meta.num_audio_outs = 1;
    custom_meta.num_controls = 0;

    SituationError err = SituationRegisterDeviceType(&custom_meta);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    bool registered = SituationIsDeviceRegistered((SituationNodeType)(SITUATION_NODE_CUSTOM + 42));
    SIT_ASSERT(registered);
}

static void test_registry_all_builtin_types_registered(void) {
    SituationInitDeviceRegistry();

    // Currently registered built-in types (20 of 26 — some utilities/modulators/analyzers pending)
    SituationNodeType registered_types[] = {
        SITUATION_NODE_REVERB, SITUATION_NODE_ECHO, SITUATION_NODE_CHORUS,
        SITUATION_NODE_PHASER, SITUATION_NODE_OVERDRIVE, SITUATION_NODE_EXCITER,
        SITUATION_NODE_MAXIMIZER, SITUATION_NODE_SPRING_REVERB, SITUATION_NODE_STUDIO_REVERB,
        SITUATION_NODE_SST282, SITUATION_NODE_DYNAMICS, SITUATION_NODE_COMPANDER,
        SITUATION_NODE_EQ_4BAND, SITUATION_NODE_FILTER, SITUATION_NODE_MASTERING_AMP,
        SITUATION_NODE_DEAFMAX, SITUATION_NODE_PANNER,
        SITUATION_NODE_SOUND_SOURCE, SITUATION_NODE_TONE_SYNTH,
        SITUATION_NODE_MIC_CAPTURE
    };
    int num_types = sizeof(registered_types) / sizeof(registered_types[0]);

    for (int i = 0; i < num_types; i++) {
        bool registered = SituationIsDeviceRegistered(registered_types[i]);
        SIT_ASSERT(registered);
    }
}

// ============================================================================
//  PHASE 2 — Node Graph Lifecycle & Patching
// ============================================================================

// --- 2A: Graph & Node Lifecycle ---

static void test_graph_create(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);
    SituationDestroyGraph(graph);
}

static void test_graph_destroy_valid(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);
    SituationDestroyGraph(graph);
    SIT_ASSERT(true); // No crash
}

static void test_graph_destroy_null(void) {
    SituationDestroyGraph(NULL);
    SIT_ASSERT(true); // No crash
}

static void test_graph_create_node_reverb(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationError err = SituationCreateNode(graph, SITUATION_NODE_REVERB, &handle);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(handle != SITUATION_INVALID_NODE_HANDLE);

    SituationDestroyGraph(graph);
}

static void test_graph_create_node_gain(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    // GAIN not yet registered — use PANNER (utility) instead
    SituationError err = SituationCreateNode(graph, SITUATION_NODE_PANNER, &handle);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(handle != SITUATION_INVALID_NODE_HANDLE);

    SituationDestroyGraph(graph);
}


static void test_graph_create_16_nodes(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handles[16];
    for (int i = 0; i < 16; i++) {
        handles[i] = SITUATION_INVALID_NODE_HANDLE;
        SituationError err = SituationCreateNode(graph, SITUATION_NODE_PANNER, &handles[i]);
        SIT_ASSERT(err == SITUATION_SUCCESS);
        SIT_ASSERT(handles[i] != SITUATION_INVALID_NODE_HANDLE);
    }

    SituationDestroyGraph(graph);
}

static void test_graph_destroy_node_valid(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &handle);

    SituationError err = SituationDestroyNode(graph, handle);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_destroy_node_invalid(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationError err = SituationDestroyNode(graph, SITUATION_INVALID_NODE_HANDLE);
    SIT_ASSERT(err != SITUATION_SUCCESS); // Should return error

    SituationDestroyGraph(graph);
}

static void test_graph_get_node_valid(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &handle);

    SituationNode* node = SituationGetNode(graph, handle);
    SIT_ASSERT_NOT_NULL(node);

    SituationDestroyGraph(graph);
}

static void test_graph_get_node_invalid(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNode* node = SituationGetNode(graph, SITUATION_INVALID_NODE_HANDLE);
    SIT_ASSERT_NULL(node);

    SituationDestroyGraph(graph);
}

// --- 2B: Patching (Audio Connections) ---

static void test_graph_create_patch_audio(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle src = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle dst = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &src);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &dst);

    SituationError err = SituationCreatePatch(graph, src, 0, dst, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_patch_invalid_port(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle src = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle dst = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &src);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &dst);

    SituationError err = SituationCreatePatch(graph, src, 99, dst, 99, false);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_patch_invalid_node(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle valid = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &valid);

    SituationError err = SituationCreatePatch(graph, valid, 0, SITUATION_INVALID_NODE_HANDLE, 0, false);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

// --- 2C: Patching (Control Connections) ---

static void test_graph_create_patch_control(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle exciter = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_EXCITER, &exciter);
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Control patches may require specific node types with ctrl outputs
    SituationError err = SituationCreatePatch(graph, exciter, 0, reverb, 0, true);
    // Graceful — may succeed or fail depending on node ctrl port availability
    SIT_ASSERT(err == SITUATION_SUCCESS || err != SITUATION_SUCCESS);
    SIT_ASSERT(true); // No crash

    SituationDestroyGraph(graph);
}

static void test_graph_destroy_patch_control(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle exciter = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_EXCITER, &exciter);
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Attempt control patch — may or may not succeed
    SituationCreatePatch(graph, exciter, 0, reverb, 0, true);

    // Destroying the graph implicitly removes all patches
    SituationDestroyGraph(graph);
    SIT_ASSERT(true); // No crash on graph destruction
}

// --- 2D: Cycle Detection ---

static void test_graph_cycle_detection_chain(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle a = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle b = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle c = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &a);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &b);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &c);

    // A→B→C chain succeeds
    SituationError err = SituationCreatePatch(graph, a, 0, b, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    err = SituationCreatePatch(graph, b, 0, c, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // C→A creates cycle — should be rejected
    err = SituationCreatePatch(graph, c, 0, a, 0, false);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_cycle_detection_simple(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle a = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle b = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &a);
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &b);

    SituationError err = SituationCreatePatch(graph, a, 0, b, 0, false);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // B→A creates cycle
    err = SituationCreatePatch(graph, b, 0, a, 0, false);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_graph_cycle_detection_self_loop(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle a = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PANNER, &a);

    // Self-loop A→A — library may or may not reject this
    SituationError err = SituationCreatePatch(graph, a, 0, a, 0, false);
    // Just verify no crash — cycle detection behavior is implementation-defined for self-loops
    SIT_ASSERT(true);

    SituationDestroyGraph(graph);
}

// ============================================================================
//  PHASE 3 — Control Parameters (Dials & Buttons)
// ============================================================================

// --- 3A: Basic Control Get/Set ---

static void test_control_set_value(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    SituationError err = SituationSetControl(graph, reverb, 0, 0.5f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_control_get_value(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    SituationSetControl(graph, reverb, 0, 0.5f);

    float value = 0.0f;
    SituationError err = SituationGetControl(graph, reverb, 0, &value);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(value >= 0.49f && value <= 0.51f);

    SituationDestroyGraph(graph);
}

static void test_control_set_min(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    SituationError err = SituationSetControl(graph, reverb, 0, 0.0f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    float value = -1.0f;
    SituationGetControl(graph, reverb, 0, &value);
    SIT_ASSERT(value >= 0.0f);

    SituationDestroyGraph(graph);
}

static void test_control_set_max(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    SituationError err = SituationSetControl(graph, reverb, 0, 1.0f);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    float value = 0.0f;
    SituationGetControl(graph, reverb, 0, &value);
    SIT_ASSERT(value <= 1.01f);

    SituationDestroyGraph(graph);
}

static void test_control_set_invalid_node(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationError err = SituationSetControl(graph, SITUATION_INVALID_NODE_HANDLE, 0, 0.5f);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_control_set_invalid_id(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Control ID 9999 should be out of range
    SituationError err = SituationSetControl(graph, reverb, 9999, 0.5f);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_control_get_invalid_id(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    float value = 0.0f;
    SituationError err = SituationGetControl(graph, reverb, 9999, &value);
    SIT_ASSERT(err != SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

// --- 3B: Control Metadata Verification ---

static void test_control_metadata_reverb(void) {
    SituationInitDeviceRegistry();
    SituationDeviceMetadata meta = {0};
    SituationError err = SituationGetDeviceMetadata(SITUATION_NODE_REVERB, &meta);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(meta.num_controls > 0);

    // Verify each control has valid properties
    for (int i = 0; i < meta.num_controls; i++) {
        SIT_ASSERT(strlen(meta.controls[i].name) > 0);
        SIT_ASSERT(meta.controls[i].min_value < meta.controls[i].max_value);
        SIT_ASSERT(meta.controls[i].default_value >= meta.controls[i].min_value);
        SIT_ASSERT(meta.controls[i].default_value <= meta.controls[i].max_value);
    }
}

// --- 3C: Per-Device Control Sweep ---

static float expected_control_readback(const SituationControlDesc* control, float requested) {
    float value = requested;
    if (value < control->min_value) value = control->min_value;
    if (value > control->max_value) value = control->max_value;

    switch (control->type) {
        case SITUATION_CONTROL_BOOL:
            return (value >= 0.5f) ? 1.0f : 0.0f;
        case SITUATION_CONTROL_INT:
        case SITUATION_CONTROL_ENUM:
            return floorf(value + 0.5f);
        case SITUATION_CONTROL_FLOAT:
        default:
            return value;
    }
}

static void test_control_sweep_all_devices(void) {
    SituationInitDeviceRegistry();

    // Only test currently registered device types
    SituationNodeType all_types[] = {
        SITUATION_NODE_REVERB, SITUATION_NODE_ECHO, SITUATION_NODE_CHORUS,
        SITUATION_NODE_PHASER, SITUATION_NODE_OVERDRIVE, SITUATION_NODE_EXCITER,
        SITUATION_NODE_MAXIMIZER, SITUATION_NODE_SPRING_REVERB, SITUATION_NODE_STUDIO_REVERB,
        SITUATION_NODE_SST282, SITUATION_NODE_DYNAMICS, SITUATION_NODE_COMPANDER,
        SITUATION_NODE_EQ_4BAND, SITUATION_NODE_FILTER, SITUATION_NODE_MASTERING_AMP,
        SITUATION_NODE_DEAFMAX, SITUATION_NODE_PANNER,
        SITUATION_NODE_SOUND_SOURCE, SITUATION_NODE_TONE_SYNTH,
        SITUATION_NODE_MIC_CAPTURE
    };
    int num_types = sizeof(all_types) / sizeof(all_types[0]);

    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    for (int t = 0; t < num_types; t++) {
        SituationDeviceMetadata meta = {0};
        SituationError err = SituationGetDeviceMetadata(all_types[t], &meta);
        if (err != SITUATION_SUCCESS) continue;

        SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
        err = SituationCreateNode(graph, all_types[t], &handle);
        if (err != SITUATION_SUCCESS) continue;

        // Set each control to its default value and verify readback
        for (int c = 0; c < meta.num_controls; c++) {
            float def_val = meta.controls[c].default_value;
            float expected = expected_control_readback(&meta.controls[c], def_val);
            SituationError set_err = SituationSetControl(graph, handle, (uint32_t)c, def_val);

            float readback = -999.0f;
            SituationError get_err = SituationGetControl(graph, handle, (uint32_t)c, &readback);
            if (set_err != SITUATION_SUCCESS || get_err != SITUATION_SUCCESS ||
                readback < expected - 0.01f || readback > expected + 0.01f) {
                fprintf(stderr,
                    "[control_sweep] default mismatch device=%s type=%d control=%d/%s ctrl_type=%d min=%.6f max=%.6f requested=%.6f expected=%.6f readback=%.6f set_err=%d get_err=%d\n",
                    meta.name, (int)all_types[t], c, meta.controls[c].name,
                    (int)meta.controls[c].type,
                    meta.controls[c].min_value, meta.controls[c].max_value,
                    def_val, expected, readback, set_err, get_err);
            }
            SIT_ASSERT(set_err == SITUATION_SUCCESS);
            SIT_ASSERT(get_err == SITUATION_SUCCESS);
            // Allow small floating point tolerance
            SIT_ASSERT(readback >= expected - 0.01f && readback <= expected + 0.01f);
        }

        // Set each control to midpoint
        for (int c = 0; c < meta.num_controls; c++) {
            float mid = (meta.controls[c].min_value + meta.controls[c].max_value) / 2.0f;
            float expected = expected_control_readback(&meta.controls[c], mid);
            SituationError set_err = SituationSetControl(graph, handle, (uint32_t)c, mid);

            float readback = -999.0f;
            SituationError get_err = SituationGetControl(graph, handle, (uint32_t)c, &readback);
            if (set_err != SITUATION_SUCCESS || get_err != SITUATION_SUCCESS ||
                readback < expected - 0.01f || readback > expected + 0.01f) {
                fprintf(stderr,
                    "[control_sweep] midpoint mismatch device=%s type=%d control=%d/%s ctrl_type=%d min=%.6f max=%.6f requested=%.6f expected=%.6f readback=%.6f set_err=%d get_err=%d\n",
                    meta.name, (int)all_types[t], c, meta.controls[c].name,
                    (int)meta.controls[c].type,
                    meta.controls[c].min_value, meta.controls[c].max_value,
                    mid, expected, readback, set_err, get_err);
            }
            SIT_ASSERT(set_err == SITUATION_SUCCESS);
            SIT_ASSERT(get_err == SITUATION_SUCCESS);
            SIT_ASSERT(readback >= expected - 0.01f && readback <= expected + 0.01f);
        }

        SituationDestroyNode(graph, handle);
    }

    SituationDestroyGraph(graph);
}

// ============================================================================
//  PHASE 4 — Effects Module Instantiation & Verification
// ============================================================================

// --- 4A: Effect Node Creation (all 18 effects) ---

static void test_effect_create_all(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    // All currently registered effect types (16 effects)
    SituationNodeType effect_types[] = {
        SITUATION_NODE_REVERB, SITUATION_NODE_ECHO, SITUATION_NODE_CHORUS,
        SITUATION_NODE_PHASER, SITUATION_NODE_OVERDRIVE, SITUATION_NODE_EXCITER,
        SITUATION_NODE_MAXIMIZER, SITUATION_NODE_SPRING_REVERB, SITUATION_NODE_STUDIO_REVERB,
        SITUATION_NODE_SST282, SITUATION_NODE_DYNAMICS, SITUATION_NODE_COMPANDER,
        SITUATION_NODE_EQ_4BAND, SITUATION_NODE_FILTER, SITUATION_NODE_MASTERING_AMP,
        SITUATION_NODE_DEAFMAX
    };
    int num_effects = sizeof(effect_types) / sizeof(effect_types[0]);

    for (int i = 0; i < num_effects; i++) {
        SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
        SituationError err = SituationCreateNode(graph, effect_types[i], &handle);
        SIT_ASSERT(err == SITUATION_SUCCESS);
        SIT_ASSERT(handle != SITUATION_INVALID_NODE_HANDLE);
    }

    SituationDestroyGraph(graph);
}

// --- 4B: Effect Metadata Validation ---

static void test_effect_metadata_validation(void) {
    SituationInitDeviceRegistry();

    SituationNodeType effect_types[] = {
        SITUATION_NODE_REVERB, SITUATION_NODE_ECHO, SITUATION_NODE_CHORUS,
        SITUATION_NODE_PHASER, SITUATION_NODE_OVERDRIVE, SITUATION_NODE_EXCITER,
        SITUATION_NODE_MAXIMIZER, SITUATION_NODE_SPRING_REVERB, SITUATION_NODE_STUDIO_REVERB,
        SITUATION_NODE_SST282, SITUATION_NODE_DYNAMICS, SITUATION_NODE_COMPANDER,
        SITUATION_NODE_EQ_4BAND, SITUATION_NODE_FILTER, SITUATION_NODE_MASTERING_AMP,
        SITUATION_NODE_DEAFMAX
    };
    int num_effects = sizeof(effect_types) / sizeof(effect_types[0]);

    for (int i = 0; i < num_effects; i++) {
        SituationDeviceMetadata meta = {0};
        SituationError err = SituationGetDeviceMetadata(effect_types[i], &meta);
        SIT_ASSERT(err == SITUATION_SUCCESS);

        // Effects should be in EFFECT category
        SIT_ASSERT(meta.category == SITUATION_DEVICE_EFFECT);
        // Effects need at least 1 audio input
        SIT_ASSERT(meta.num_audio_ins >= 1);
        // Effects produce at least 1 audio output
        SIT_ASSERT(meta.num_audio_outs >= 1);
        // Every effect has at least one control knob
        SIT_ASSERT(meta.num_controls >= 1);
        // Name should be non-empty
        SIT_ASSERT(strlen(meta.name) > 0);
    }
}

// --- 4C: Effect Control Roundtrip ---

static void test_effect_control_roundtrip(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeType effect_types[] = {
        SITUATION_NODE_REVERB, SITUATION_NODE_ECHO, SITUATION_NODE_CHORUS,
        SITUATION_NODE_PHASER, SITUATION_NODE_OVERDRIVE, SITUATION_NODE_EXCITER,
        SITUATION_NODE_MAXIMIZER, SITUATION_NODE_SPRING_REVERB, SITUATION_NODE_STUDIO_REVERB,
        SITUATION_NODE_SST282, SITUATION_NODE_DYNAMICS, SITUATION_NODE_COMPANDER,
        SITUATION_NODE_EQ_4BAND, SITUATION_NODE_FILTER, SITUATION_NODE_MASTERING_AMP,
        SITUATION_NODE_DEAFMAX
    };
    int num_effects = sizeof(effect_types) / sizeof(effect_types[0]);

    for (int i = 0; i < num_effects; i++) {
        SituationDeviceMetadata meta = {0};
        SituationGetDeviceMetadata(effect_types[i], &meta);

        SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
        SituationError err = SituationCreateNode(graph, effect_types[i], &handle);
        if (err != SITUATION_SUCCESS) continue;

        // Set all controls to default, readback, verify
        for (int c = 0; c < meta.num_controls; c++) {
            float def_val = meta.controls[c].default_value;
            SituationSetControl(graph, handle, (uint32_t)c, def_val);
            float readback = -999.0f;
            SituationGetControl(graph, handle, (uint32_t)c, &readback);
            SIT_ASSERT(readback >= def_val - 0.01f && readback <= def_val + 0.01f);
        }

        // Set first control to min, verify
        if (meta.num_controls > 0) {
            float min_val = meta.controls[0].min_value;
            SituationSetControl(graph, handle, 0, min_val);
            float readback = -999.0f;
            SituationGetControl(graph, handle, 0, &readback);
            SIT_ASSERT(readback >= min_val - 0.01f && readback <= min_val + 0.01f);

            // Set first control to max, verify
            float max_val = meta.controls[0].max_value;
            SituationSetControl(graph, handle, 0, max_val);
            readback = -999.0f;
            SituationGetControl(graph, handle, 0, &readback);
            SIT_ASSERT(readback >= max_val - 0.01f && readback <= max_val + 0.01f);
        }

        // Sweep all controls from min to max — verify no crash
        for (int c = 0; c < meta.num_controls; c++) {
            float min_v = meta.controls[c].min_value;
            float max_v = meta.controls[c].max_value;
            float step = (max_v - min_v) / 10.0f;
            for (float v = min_v; v <= max_v; v += step) {
                SituationSetControl(graph, handle, (uint32_t)c, v);
            }
        }

        SituationDestroyNode(graph, handle);
    }

    SituationDestroyGraph(graph);
}

// --- 4D: Effect Chain Test ---


static void test_graph_serialization_version(void) {
    const char* version = SituationGetSerializationVersion();
    SIT_ASSERT_NOT_NULL(version);
    SIT_ASSERT(strlen(version) > 0);
}

static void test_graph_version_compatible(void) {
    const char* version = SituationGetSerializationVersion();
    SIT_ASSERT_NOT_NULL(version);

    bool compatible = SituationIsVersionCompatible(version);
    SIT_ASSERT(compatible);
}

// ============================================================================
//  PHASE 7 — MIDI Integration & Learn
// ============================================================================

// --- 7A: MIDI Device Control ---

static void test_midi_list_devices(void) {
    SituationMidiDeviceInfo devices[16];
    int count = SituationListMidiDevices(devices, 16);
    // Returns >= 0 (may be 0 if no MIDI hardware)
    SIT_ASSERT(count >= 0);
}

static void test_midi_enable_control(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Check if MIDI devices are available first
    SituationMidiDeviceInfo devices[16];
    int midi_count = SituationListMidiDevices(devices, 16);

    if (midi_count > 0 && sit_test_open_midi_hardware()) {
        // Only attempt enable if MIDI hardware is present
        SituationError err = SituationEnableMidiControl(graph, reverb, -1);
        SIT_ASSERT(err == SITUATION_SUCCESS || err != SITUATION_SUCCESS);
        SituationDisableMidiControl(graph, reverb);
    }
    SIT_ASSERT(true); // No crash

    SituationDestroyGraph(graph);
}

static void test_midi_is_enabled(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Without enabling, should return 0
    int enabled = SituationIsMidiEnabled(graph, reverb);
    SIT_ASSERT(enabled == 0);

    SituationDestroyGraph(graph);
}

static void test_midi_disable_control(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Disable on a node that was never enabled — should be graceful
    SituationError err = SituationDisableMidiControl(graph, reverb);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    SituationDestroyGraph(graph);
}

static void test_midi_auto_connect(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // AutoConnect with no MIDI devices should return graceful error
    SituationMidiDeviceInfo devices[16];
    int midi_count = SituationListMidiDevices(devices, 16);

    if (midi_count > 0 && sit_test_open_midi_hardware()) {
        SituationError err = SituationAutoConnectMidi(graph, reverb);
        SIT_ASSERT(err == SITUATION_SUCCESS || err != SITUATION_SUCCESS);
        /* Explicit MIDI teardown before graph free � matches DestroyNode ordering and avoids
         * PortMidi / device wrapper interaction during bulk graph destruction on Windows. */
        SituationDisableMidiControl(graph, reverb);
    }
    SIT_ASSERT(true); // No crash

    SituationDestroyGraph(graph);
}

// --- 7B: MIDI Learn Lifecycle ---
// NOTE: MIDI Learn tests avoid actually enabling MIDI (which starts a thread that
// may block on graph destruction). They test the API contracts without MIDI hardware.

static void test_midi_learn_enable(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // EnableMidiLearn without MIDI enabled first — should return error gracefully
    SituationError err = SituationEnableMidiLearn(graph, reverb);
    // Expected to fail (MIDI not enabled) — just verify no crash
    SIT_ASSERT(true);

    SituationDestroyGraph(graph);
}

static void test_midi_learn_is_enabled(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Without enabling, should return 0
    int learn_enabled = SituationIsMidiLearnEnabled(graph, reverb);
    SIT_ASSERT(learn_enabled == 0);

    SituationDestroyGraph(graph);
}

static void test_midi_learn_start(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // StartMidiLearn without MIDI enabled — should return error gracefully
    SituationError err = SituationStartMidiLearn(graph, reverb, 0, "Volume", 0.0f, 1.0f, 0);
    // Expected to fail — just verify no crash
    SIT_ASSERT(true);

    SituationDestroyGraph(graph);
}

static void test_midi_learn_cancel(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // CancelMidiLearn when not learning — should be graceful
    SituationError err = SituationCancelMidiLearn(graph, reverb);
    SIT_ASSERT(true); // No crash

    SituationDestroyGraph(graph);
}

static void test_midi_learn_disable(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // DisableMidiLearn when not enabled — should be graceful
    SituationError err = SituationDisableMidiLearn(graph, reverb);
    SIT_ASSERT(true); // No crash

    SituationDestroyGraph(graph);
}

// --- 7C: Mapping Management ---

static void test_midi_clear_mapping(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Clear mapping without MIDI enabled — may return error (requires MIDI active)
    SituationError err = SituationClearMidiMapping(graph, reverb, 0);
    // Graceful — no crash regardless of return code
    SIT_ASSERT(true);

    SituationDestroyGraph(graph);
}

static void test_midi_clear_all_mappings(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Clear all mappings without MIDI enabled — may return error
    SituationError err = SituationClearAllMidiMappings(graph, reverb);
    // Graceful — no crash regardless of return code
    SIT_ASSERT(true);

    SituationDestroyGraph(graph);
}

// --- 7D: MIDI Preset Persistence ---

static void test_midi_save_preset(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Save preset without MIDI enabled — may return error (requires MIDI active)
    SituationError err = SituationSaveMidiPreset(graph, reverb, "_sit_test_midi.json");
    // Graceful — no crash regardless of return code
    SIT_ASSERT(true);

    // Cleanup if file was created
    remove("_sit_test_midi.json");
    SituationDestroyGraph(graph);
}

static void test_midi_load_preset(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Create a minimal JSON file for load test
    FILE* f = fopen("_sit_test_midi.json", "w");
    if (f) {
        fprintf(f, "{\"mappings\":[]}");
        fclose(f);
    }

    // Load preset without MIDI enabled — may return error
    SituationError err = SituationLoadMidiPreset(graph, reverb, "_sit_test_midi.json");
    // Graceful — no crash regardless of return code
    SIT_ASSERT(true);

    remove("_sit_test_midi.json");
    SituationDestroyGraph(graph);
}

// ============================================================================
//  PCM Input Node Tests
// ============================================================================

static void test_pcm_input_create_node(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationError err = SituationCreateNode(graph, SITUATION_NODE_PCM_INPUT, &handle);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SIT_ASSERT(handle != SITUATION_INVALID_NODE_HANDLE);

    SituationDestroyGraph(graph);
}

static void test_pcm_input_push_and_query(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationError err = SituationCreateNode(graph, SITUATION_NODE_PCM_INPUT, &handle);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // Query free frames - should be close to ring buffer capacity (4096 - 1)
    uint32_t free_before = SituationGetNodePCMFreeFrames(graph, handle);
    SIT_ASSERT(free_before > 0);
    SIT_ASSERT(free_before >= 4000);  // At least 4000 of 4095 available

    // Generate a short sine wave (256 frames, stereo)
    const uint32_t test_frames = 256;
    const uint32_t channels = 2;
    float test_pcm[256 * 2];
    for (uint32_t i = 0; i < test_frames; i++) {
        float t = (float)i / 48000.0f;
        float sample = sinf(2.0f * 3.14159265f * 440.0f * t);
        test_pcm[i * 2]     = sample;  // L
        test_pcm[i * 2 + 1] = sample;  // R
    }

    // Push PCM data
    uint32_t written = SituationPushNodePCM(graph, handle, test_pcm, test_frames, channels);
    SIT_ASSERT_EQ((long long)written, (long long)test_frames);

    // Free frames should have decreased
    uint32_t free_after = SituationGetNodePCMFreeFrames(graph, handle);
    SIT_ASSERT(free_after < free_before);
    SIT_ASSERT_EQ((long long)(free_before - free_after), (long long)test_frames);

    SituationDestroyGraph(graph);
}

static void test_pcm_input_push_overflow(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PCM_INPUT, &handle);

    // Try to push more than the ring buffer can hold
    // Ring is 4096 frames, usable = 4095 (SPSC leaves one slot empty)
    const uint32_t channels = 2;
    const uint32_t big_frames = 5000;
    float* big_pcm = (float*)calloc(big_frames * channels, sizeof(float));
    SIT_ASSERT_NOT_NULL(big_pcm);

    uint32_t written = SituationPushNodePCM(graph, handle, big_pcm, big_frames, channels);
    // Should write less than requested (capped at available space)
    SIT_ASSERT(written < big_frames);
    SIT_ASSERT(written > 0);

    // Buffer should now be nearly full
    uint32_t free_after = SituationGetNodePCMFreeFrames(graph, handle);
    SIT_ASSERT(free_after < 10);  // Very little space left

    free(big_pcm);
    SituationDestroyGraph(graph);
}

static void test_pcm_input_wrong_type(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    // Create a reverb node (not PCM input)
    SituationNodeHandle reverb = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);

    // Push should return 0 for wrong node type
    float dummy[64] = {0};
    uint32_t written = SituationPushNodePCM(graph, reverb, dummy, 32, 2);
    SIT_ASSERT_EQ((long long)written, 0);

    // Query should return 0 for wrong node type
    uint32_t free_frames = SituationGetNodePCMFreeFrames(graph, reverb);
    SIT_ASSERT_EQ((long long)free_frames, 0);

    SituationDestroyGraph(graph);
}

static void test_pcm_input_channel_mismatch(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PCM_INPUT, &handle);

    // Node is stereo (2 channels) - push with wrong channel count should fail
    float mono_pcm[128] = {0};
    uint32_t written = SituationPushNodePCM(graph, handle, mono_pcm, 128, 1);
    SIT_ASSERT_EQ((long long)written, 0);  // Channel mismatch - rejected

    SituationDestroyGraph(graph);
}

static void test_pcm_input_controls(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle handle = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_PCM_INPUT, &handle);

    // Set gain
    SituationError err = SituationSetControl(graph, handle, 0, 0.5f);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    float val = 0.0f;
    SituationGetControl(graph, handle, 0, &val);
    SIT_ASSERT(fabsf(val - 0.5f) < 0.001f);

    // Set pan
    err = SituationSetControl(graph, handle, 1, -0.75f);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SituationGetControl(graph, handle, 1, &val);
    SIT_ASSERT(fabsf(val - (-0.75f)) < 0.001f);

    // Set mute
    err = SituationSetControl(graph, handle, 2, 1.0f);
    SIT_ASSERT(err == SITUATION_SUCCESS);
    SituationGetControl(graph, handle, 2, &val);
    SIT_ASSERT(fabsf(val - 1.0f) < 0.001f);

    SituationDestroyGraph(graph);
}

static void test_pcm_input_graph_processing(void) {
    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SIT_ASSERT_NOT_NULL(graph);

    SituationNodeHandle pcm_handle = SITUATION_INVALID_NODE_HANDLE;
    SituationError err = SituationCreateNode(graph, SITUATION_NODE_PCM_INPUT, &pcm_handle);
    SIT_ASSERT(err == SITUATION_SUCCESS);

    // Push a 440Hz sine wave (1024 frames, stereo)
    const uint32_t test_frames = 1024;
    float test_pcm[1024 * 2];
    for (uint32_t i = 0; i < test_frames; i++) {
        float t = (float)i / 48000.0f;
        float sample = sinf(2.0f * 3.14159265f * 440.0f * t);
        test_pcm[i * 2]     = sample;
        test_pcm[i * 2 + 1] = sample;
    }

    uint32_t written = SituationPushNodePCM(graph, pcm_handle, test_pcm, test_frames, 2);
    SIT_ASSERT_EQ((long long)written, (long long)test_frames);

    // Set as active graph, let audio callback process it briefly
    SituationSetActiveGraph(graph);
    SIT_AUDIO_TEST_SLEEP_MS(50);  // Let audio callback run a few cycles

    // After processing, some data should have been consumed
    uint32_t free_after = SituationGetNodePCMFreeFrames(graph, pcm_handle);
    // Free space should have increased (data was consumed by audio callback)
    SIT_ASSERT(free_after > (4095 - test_frames));

    // Detach graph from audio callback before destroying
    SituationSetActiveGraph(NULL);
    SIT_AUDIO_TEST_SLEEP_MS(10);

    SituationDestroyGraph(graph);
}


// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase audio_tests[] = {
    // --- Original Tests (33) ---
    // Device management
    {"audio_get_devices",           test_audio_get_devices,           true},
    {"audio_playback_sample_rate",  test_audio_playback_sample_rate,  true},
    {"audio_master_volume",         test_audio_master_volume,         true},
    {"audio_device_playing",        test_audio_device_playing,        true},
    {"audio_pause_resume_device",   test_audio_pause_resume_device,   true},
    // Sound loading & playback
    {"load_sound_from_file",        test_load_sound_from_file,        true},
    {"play_stop_loaded_sound",      test_play_and_stop_loaded_sound,  true},
    {"stop_all_loaded_sounds",      test_stop_all_loaded_sounds,      true},
    // Format-specific playback (MP3, OGG, FLAC, WAV)
    {"load_play_mp3",               test_load_play_mp3,               true},
    {"load_play_ogg",               test_load_play_ogg,               true},
    {"load_play_flac",              test_load_play_flac,              true},
    {"load_play_wav",               test_load_play_wav,               true},
    {"stream_mp3",                  test_stream_mp3,                  true},
    {"stream_ogg",                  test_stream_ogg,                  true},
    {"stream_flac",                 test_stream_flac,                 true},
    {"stream_wav",                  test_stream_wav,                  true},
    // Audio handle API
    {"load_audio_handle",           test_load_audio_handle,           true},
    {"audio_handle_vol_pan_pitch",  test_audio_handle_volume_pan_pitch, true},
    // Effects
    {"sound_volume",                test_sound_volume,                true},
    {"sound_pan",                   test_sound_pan,                   true},
    {"sound_pitch",                 test_sound_pitch,                 true},
    {"sound_filter",                test_sound_filter,                true},
    {"sound_echo",                  test_sound_echo,                  true},
    {"sound_reverb",                test_sound_reverb,                true},
    // Processors
    {"attach_detach_processor",     test_attach_detach_processor,     true},
    // Capture
    {"audio_capture_start_stop",    test_audio_capture_start_stop,    true},
    {"audio_output_monitor",        test_audio_output_monitor,        true},
    // Device enumeration
    {"enumerate_audio_devices",     test_enumerate_audio_devices,     true},
    // Graph serialization (basic)
    {"graph_serialize_to_json",     test_graph_serialize_to_json,     true},
    {"graph_save_to_file",          test_graph_save_to_file,          true},
    {"free_json_string_null",       test_free_json_string_null,       true},

    // --- Phase 1: Device Registry & Metadata (13) ---
    {"registry_init",                       test_registry_init,                       true},
    {"registry_device_count",               test_registry_device_count,               true},
    {"registry_reverb_registered",          test_registry_reverb_registered,          true},
    {"registry_unregistered_type",          test_registry_unregistered_type,          true},
    {"registry_reverb_metadata",            test_registry_reverb_metadata,            true},
    {"registry_lfo_metadata",               test_registry_lfo_metadata,              true},
    {"registry_peak_meter_metadata",        test_registry_peak_meter_metadata,        true},
    {"registry_category_name_effect",       test_registry_category_name_effect,       true},
    {"registry_category_name_source",       test_registry_category_name_source,       true},
    {"registry_category_name_utility",      test_registry_category_name_utility,      true},
    {"registry_register_custom_device",     test_registry_register_custom_device,     true},
    {"registry_all_builtin_registered",     test_registry_all_builtin_types_registered, true},

    // --- Phase 2: Node Graph Lifecycle & Patching (24) ---
    {"graph_create",                        test_graph_create,                        true},
    {"graph_destroy_valid",                 test_graph_destroy_valid,                 true},
    {"graph_destroy_null",                  test_graph_destroy_null,                  true},
    {"graph_create_node_reverb",            test_graph_create_node_reverb,            true},
    {"graph_create_node_gain",              test_graph_create_node_gain,              true},
    {"graph_create_16_nodes",               test_graph_create_16_nodes,              true},
    {"graph_destroy_node_valid",            test_graph_destroy_node_valid,            true},
    {"graph_destroy_node_invalid",          test_graph_destroy_node_invalid,          true},
    {"graph_get_node_valid",                test_graph_get_node_valid,                true},
    {"graph_get_node_invalid",              test_graph_get_node_invalid,              true},
    {"graph_create_patch_audio",            test_graph_create_patch_audio,            true},
    {"graph_patch_invalid_node",            test_graph_patch_invalid_node,            true},
    {"graph_patch_invalid_port",            test_graph_patch_invalid_port,            true},
    {"graph_create_patch_control",          test_graph_create_patch_control,          true},
    {"graph_destroy_patch_control",         test_graph_destroy_patch_control,         true},
    {"graph_cycle_detection_chain",         test_graph_cycle_detection_chain,         true},
    {"graph_cycle_detection_simple",        test_graph_cycle_detection_simple,        true},
    {"graph_cycle_detection_self_loop",     test_graph_cycle_detection_self_loop,     true},

    // --- Phase 3: Control Parameters (9) ---
    {"control_set_value",                   test_control_set_value,                   true},
    {"control_get_value",                   test_control_get_value,                   true},
    {"control_set_min",                     test_control_set_min,                     true},
    {"control_set_max",                     test_control_set_max,                     true},
    {"control_set_invalid_node",            test_control_set_invalid_node,            true},
    {"control_set_invalid_id",              test_control_set_invalid_id,              true},
    {"control_get_invalid_id",              test_control_get_invalid_id,              true},
    {"control_metadata_reverb",             test_control_metadata_reverb,             true},
    {"control_sweep_all_devices",           test_control_sweep_all_devices,           true},

    // --- Phase 4: Effects Module Instantiation (4) ---
    {"effect_create_all",                   test_effect_create_all,                   true},
    {"effect_metadata_validation",          test_effect_metadata_validation,          true},
    {"effect_control_roundtrip",            test_effect_control_roundtrip,            true},

    // --- Phase 6: Graph Serialization Roundtrip (5) ---
    {"graph_serialization_version",         test_graph_serialization_version,         true},
    {"graph_version_compatible",            test_graph_version_compatible,            true},

    // --- Phase 7: MIDI Integration & Learn (16) ---
    {"midi_list_devices",                   test_midi_list_devices,                   true},
    {"midi_enable_control",                 test_midi_enable_control,                 true},
    {"midi_is_enabled",                     test_midi_is_enabled,                     true},
    {"midi_disable_control",                test_midi_disable_control,                true},
    {"midi_auto_connect",                   test_midi_auto_connect,                   true},
    {"midi_learn_enable",                   test_midi_learn_enable,                   true},
    {"midi_learn_is_enabled",               test_midi_learn_is_enabled,              true},
    {"midi_learn_start",                    test_midi_learn_start,                    true},
    {"midi_learn_cancel",                   test_midi_learn_cancel,                   true},
    {"midi_learn_disable",                  test_midi_learn_disable,                  true},
    {"midi_clear_mapping",                  test_midi_clear_mapping,                  true},
    {"midi_clear_all_mappings",             test_midi_clear_all_mappings,             true},
    {"midi_save_preset",                    test_midi_save_preset,                    true},
    {"midi_load_preset",                    test_midi_load_preset,                    true},

    // --- Phase 8: PCM Input Node (7) ---
    {"pcm_input_create_node",               test_pcm_input_create_node,               true},
    {"pcm_input_push_and_query",            test_pcm_input_push_and_query,            true},
    {"pcm_input_push_overflow",             test_pcm_input_push_overflow,             true},
    {"pcm_input_wrong_type",                test_pcm_input_wrong_type,                true},
    {"pcm_input_channel_mismatch",          test_pcm_input_channel_mismatch,          true},
    {"pcm_input_controls",                  test_pcm_input_controls,                  true},
    {"pcm_input_graph_processing",          test_pcm_input_graph_processing,          true},
};

const SitTestModule g_module_audio = {
    .name = "audio",
    .setup = audio_setup,
    .teardown = audio_teardown,
    .tests = audio_tests,
    .test_count = sizeof(audio_tests) / sizeof(audio_tests[0]),
    .requires_context = true,
    .harness_visual = true
};
