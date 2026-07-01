/***************************************************************************************************
*
*   examples/midi_auto_connect_example.c - MIDI Auto-Connect Example
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Demonstrates the new integrated MIDI control system (Phase 1).
*   Shows how easy it is to enable MIDI control for any node with a single function call.
*   
*   This example shows:
*   - Automatic MIDI device selection
*   - Automatic callback setup based on node type
*   - Automatic MIDI processing in graph loop
*   - Automatic cleanup on node destruction
*   
*   BEFORE (Manual MIDI Setup):
*   - Create MIDI device manually
*   - Configure callbacks manually
*   - Open MIDI stream manually
*   - Process MIDI in audio callback manually
*   - Cleanup manually
*   
*   AFTER (Integrated MIDI):
*   - SituationAutoConnectMidi(graph, node)
*   - Done!
*   
***************************************************************************************************/

#define SITUATION_USE_OPENGL
#include "situation.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    SituationAudioGraph* graph;
    SituationNodeHandle compander;
} AppState;

static AppState g_app = {0};

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
        frame_count,
        g_device_function_table,
        g_device_function_table_count
    );
}

int main(void) {
    printf("=== MIDI Auto-Connect Example ===\n\n");

    SituationInitDeviceRegistry();

    g_app.graph = SituationCreateGraph();
    
    // Create compander node
    SituationError err = SituationCreateNode(g_app.graph, SITUATION_NODE_COMPANDER, &g_app.compander);
    if (err != SITUATION_SUCCESS) {
        printf("Failed to create compander node\n");
        return 1;
    }
    
    // Enable MIDI control (automatic!)
    printf("Enabling MIDI control...\n");
    err = SituationAutoConnectMidi(g_app.graph, g_app.compander);
    
    if (err == SITUATION_SUCCESS) {
        printf("✓ MIDI control enabled!\n\n");
        
        // List connected device
        SituationMidiDeviceInfo devices[32];
        int count = SituationListMidiDevices(devices, 32);
        printf("Available MIDI devices:\n");
        for (int i = 0; i < count; i++) {
            printf("  [%d] %s (%s)\n", i, devices[i].device_name,
                   devices[i].is_input ? "Input" : "Output");
        }
        printf("\n");
    } else if (err == SITUATION_ERROR_MIDI_NO_DEVICES) {
        printf("⚠ No MIDI devices found. Continuing without MIDI.\n\n");
    } else {
        printf("✗ Failed to enable MIDI control (error %d)\n\n", err);
    }
    
    // Setup audio
    ma_device audio_device;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = audio_callback;
    config.periodSizeInFrames = 512;
    
    if (ma_device_init(NULL, &config, &audio_device) != MA_SUCCESS) {
        printf("Failed to initialize audio device\n");
        return 1;
    }
    
    if (ma_device_start(&audio_device) != MA_SUCCESS) {
        printf("Failed to start audio device\n");
        return 1;
    }
    
    printf("Audio running. MIDI CC messages will control the compander.\n");
    printf("Press Enter to quit.\n\n");
    
    if (SituationIsMidiEnabled(g_app.graph, g_app.compander)) {
        printf("MIDI CC Mapping (Compander):\n");
        printf("  Band 0 (Low):  CC 16-23\n");
        printf("  Band 1 (Mid):  CC 24-31\n");
        printf("  Band 2 (High): CC 32-39\n\n");
    }
    
    getchar();
    
    // Cleanup (MIDI cleanup is automatic!)
    ma_device_uninit(&audio_device);
    SituationDestroyGraph(g_app.graph);
    
    printf("Cleanup complete.\n");
    return 0;
}
