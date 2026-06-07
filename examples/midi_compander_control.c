/***************************************************************************************************
*
*   examples/midi_compander_control.c - MIDI-Controlled Compander Example
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Demonstrates enabling MIDI on a compander node using SituationEnableMidiControl with an
*   explicit PortMidi input device id (first available input). Processing uses SituationProcessGraph
*   with the built-in device function table (same wiring as the engine audio callback).
*
*   MIDI CC mapping follows the compander callback table (see Situation docs / midi_device_callbacks).
*
***************************************************************************************************/

#define SITUATION_USE_OPENGL
#include "../situation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SituationAudioGraph* graph;
    SituationNodeHandle compander;
} AppState;

static AppState g_app = {0};

static int find_first_midi_input_device(void) {
    int n = Pm_CountDevices();
    for (int i = 0; i < n; i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info && info->input) {
            return i;
        }
    }
    return -1;
}

static void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)device;
    (void)input;

    if (!g_app.graph) {
        memset(output, 0, frame_count * 2 * sizeof(float));
        return;
    }

    SituationProcessGraph(
        g_app.graph,
        (float*)output,
        (int)frame_count,
        g_device_function_table,
        g_device_function_table_count
    );
}

int main(void) {
    printf("=== MIDI-Controlled Compander Example ===\n\n");

    SituationInitDeviceRegistry();

    g_app.graph = SituationCreateGraph();
    if (!g_app.graph) {
        printf("Failed to create audio graph.\n");
        return 1;
    }

    SituationError err = SituationCreateNode(g_app.graph, SITUATION_NODE_COMPANDER, &g_app.compander);
    if (err != SITUATION_SUCCESS) {
        printf("Failed to create compander node (%d).\n", (int)err);
        SituationDestroyGraph(g_app.graph);
        return 1;
    }

    int midi_in_id = find_first_midi_input_device();
    if (midi_in_id < 0) {
        printf("No MIDI input devices found. Continuing without MIDI routing.\n\n");
    } else {
        printf("Enabling MIDI on device id %d...\n", midi_in_id);
        err = SituationEnableMidiControl(g_app.graph, g_app.compander, midi_in_id);
        if (err != SITUATION_SUCCESS) {
            printf("SituationEnableMidiControl failed (%d).\n", (int)err);
        } else {
            printf("MIDI connected.\n\n");
        }
    }

    ma_device audio_device;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = audio_callback;
    config.periodSizeInFrames = 512;

    if (ma_device_init(NULL, &config, &audio_device) != MA_SUCCESS) {
        printf("Failed to initialize audio device.\n");
        SituationDestroyGraph(g_app.graph);
        return 1;
    }

    if (ma_device_start(&audio_device) != MA_SUCCESS) {
        printf("Failed to start audio device.\n");
        ma_device_uninit(&audio_device);
        SituationDestroyGraph(g_app.graph);
        return 1;
    }

    printf("Audio running. CC messages adjust compander parameters.\n");
    printf("Press Enter to quit.\n\n");

    getchar();

    ma_device_uninit(&audio_device);
    SituationDestroyGraph(g_app.graph);

    printf("Cleanup complete.\n");
    return 0;
}
