/* Standalone MIDI→audio frequency probe (virtual MIDI + graph tone synth). */
#include "sit_api_include.h"
#include "audio_freq_detect.h"
#include "sit_test_audio_levels.h"
#include "sit_test_audio_window.h"
#include "sit_test_stereo_scope.h"
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#define SIT_PROBE_SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SIT_PROBE_SLEEP_MS(ms) usleep((ms) * 1000)
#endif

int main(void) {
    printf("Situation MIDI audio probe\n");
    printf("Version: %s\n\n", SituationGetVersionString());

    SituationInitInfo config;
    sit_test_audio_window_init_info(&config, "SIT_MIDI_AUDIO_PROBE");

    SituationError err = SituationInit(0, NULL, &config);
    if (err != SITUATION_SUCCESS) {
        printf("SituationInit failed: %d\n", (int)err);
        return 1;
    }

    int sr = SituationGetAudioPlaybackSampleRate();
    if (sr <= 0) {
        printf("No audio playback device (sample rate %d)\n", sr);
        SituationShutdown();
        return 1;
    }
    printf("Playback sample rate: %d Hz\n", sr);

    int midi_in = -1;
    err = SituationSetupVirtualMidiLoopback(&midi_in);
    if (err != SITUATION_SUCCESS) {
        printf("Virtual MIDI setup failed: %d\n", (int)err);
        SituationShutdown();
        return 1;
    }
    printf("Virtual MIDI input device id: %d\n", midi_in);

    SituationInitDeviceRegistry();
    SituationAudioGraph* graph = SituationCreateGraph();
    SituationNodeHandle tone = SITUATION_INVALID_NODE_HANDLE;
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone);
    SituationSetControl(graph, tone, 1, 0.0f);
    SituationSetControl(graph, tone, 2, 0.0f);
    SituationEnableMidiControl(graph, tone, midi_in);
    SituationSetActiveGraph(graph);

    sit_test_audio_monitor_install();

    SitAudioFreqCapture cap;
    sit_audio_freq_capture_init(&cap, (uint32_t)sr, 2);
    sit_test_audio_monitor_set_capture(&cap);
    SituationSetAudioMasterVolume(0.8f);

    const int test_note = 69;
    const float expected_hz = sit_midi_note_to_hz(test_note);
    printf("\nInjecting MIDI note %d (expected %.2f Hz)...\n", test_note, expected_hz);
    SituationVirtualMidiNoteOn((uint8_t)test_note, 100);
    sit_test_audio_visual_pump_ms(500);

    float peak = 0.f, rms = 0.f;
    SituationGetMasterOutputMeter(&peak, &rms);
    printf("Master meter: peak=%.4f rms=%.4f\n", peak, rms);

    float measured = 0.f;
    int ok = sit_audio_freq_verify(&cap, (float)sr, expected_hz, 0.04f, 0.005f, &measured);
    {
        char msg[320];
        SitTestAudioLevelLimits limits;
        sit_test_audio_level_limits_tone_defaults(&limits);
        if (!sit_test_audio_levels_check(&cap, peak, rms, &limits, msg, sizeof(msg))) {
            printf("Level guard FAIL: %s\n", msg);
            ok = 0;
        } else {
            printf("Level guard: PASS (cap peak=%.4f rms=%.4f)\n", sit_audio_capture_peak(&cap),
                   sit_audio_capture_rms(&cap));
        }
    }
    printf("Frequency verify: %s (measured %.2f Hz, captured %u frames)\n",
           ok ? "PASS" : "FAIL", measured, cap.count);

    SituationVirtualMidiNoteOff((uint8_t)test_note);
    SituationDisableMidiControl(graph, tone);
    SituationSetActiveGraph(NULL);
    sit_test_audio_monitor_uninstall();
    sit_audio_freq_capture_free(&cap);
    SituationDestroyGraph(graph);
    SituationTeardownVirtualMidiLoopback();
    SituationShutdown();

    return ok ? 0 : 2;
}
