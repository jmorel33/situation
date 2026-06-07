/* One-off audio init probe — prints device state and plays an audible tone. */
#include "tests/harness/sit_api_include.h"
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#define SIT_PROBE_SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SIT_PROBE_SLEEP_MS(ms) usleep((ms) * 1000)
#endif

static void print_audio_state(const char* label) {
    int count = 0;
    SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&count);
    int sr = SituationGetAudioPlaybackSampleRate();
    bool playing = SituationIsAudioDevicePlaying();
    float vol = SituationGetAudioMasterVolume();
    char* err_msg = NULL;
    SituationError err_code = SituationGetLastErrorCode();

    if (SituationGetLastErrorMsg(&err_msg) == SITUATION_SUCCESS && err_msg) {
        printf("\n--- %s ---\n", label);
        printf("  devices enumerated: %d\n", count);
        if (count > 0 && devices) {
            printf("  default device[0]: \"%s\"\n", devices[0].name);
        }
        printf("  playback sample rate: %d Hz\n", sr);
        printf("  SituationIsAudioDevicePlaying(): %s\n", playing ? "true" : "false");
        printf("  master volume: %.2f\n", vol);
        printf("  last error code: %d\n", (int)err_code);
        printf("  last error msg: %s\n", err_msg);
        free(err_msg);
    } else {
        printf("\n--- %s ---\n", label);
        printf("  devices enumerated: %d\n", count);
        if (count > 0 && devices) {
            printf("  default device[0]: \"%s\"\n", devices[0].name);
        }
        printf("  playback sample rate: %d Hz\n", sr);
        printf("  SituationIsAudioDevicePlaying(): %s\n", playing ? "true" : "false");
        printf("  master volume: %.2f\n", vol);
        printf("  last error code: %d\n", (int)err_code);
    }
}

static void run_one_cycle(int cycle) {
    SituationInitInfo config = {0};
    config.window_width = 320;
    config.window_height = 240;
    config.window_title = "SIT_AUDIO_PROBE";
    config.initial_active_window_flags = 0;

    printf("\n========== cycle %d: SituationInit ==========\n", cycle);
    SituationError err = SituationInit(0, NULL, &config);
    printf("SituationInit returned: %d (%s)\n", (int)err,
           err == SITUATION_SUCCESS ? "SUCCESS" : "FAIL");

    print_audio_state("after init");

    if (err != SITUATION_SUCCESS) {
        SituationShutdown();
        return;
    }

    printf("\n  playing 440 Hz sine for 2 seconds (volume 0.8)...\n");
    SituationSetAudioMasterVolume(0.8f);
    SituationToneHandle tone = SituationPlayToneEx(
        SIT_WAVE_SINE, 440.0f, 0.8f, 0.0f, 0.01f, 0.05f, 0.7f, 0.1f, 2.0f);
    printf("  SituationPlayToneEx handle: %u\n", (unsigned)tone);
    print_audio_state("while tone playing");

    float peak = 0.f, rms = 0.f;
    SIT_PROBE_SLEEP_MS(100);
    SituationGetMasterOutputMeter(&peak, &rms);
    printf("  master output meter: peak=%.6f rms=%.6f (non-zero => callback is mixing audio)\n", peak, rms);

    SIT_PROBE_SLEEP_MS(2100);
    SituationStopTone(tone);

    printf("\n========== cycle %d: SituationShutdown ==========\n", cycle);
    SituationShutdown();
}

int main(void) {
    printf("Situation audio probe\n");
    printf("Version: %s\n", SituationGetVersionString());

    /* Mirror harness: 7 init/shutdown cycles in one process. */
    for (int i = 1; i <= 7; i++) {
        run_one_cycle(i);
        SIT_PROBE_SLEEP_MS(200);
    }

    printf("\nProbe finished. If you heard nothing during any cycle, audio init or output path is broken.\n");
    return 0;
}
