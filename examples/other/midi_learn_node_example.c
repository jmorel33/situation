/*
 * MIDI Learn Node Integration Example
 * 
 * Demonstrates the integrated MIDI Learn system for Situation nodes.
 * Shows how to enable MIDI Learn, learn parameters, and save/load presets.
 * 
 * Features:
 *   • Enable MIDI control with one function call
 *   • Enable MIDI Learn with one function call
 *   • Learn any parameter dynamically
 *   • Save/load custom mappings as presets
 *   • Learned mappings override hardcoded mappings
 */

#define MINIAUDIO_IMPLEMENTATION
#include "../ext/miniaudio.h"

#include "situation.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    SituationAudioGraph* graph;
    SituationNodeHandle compander;
    double last_update_time;
} AppState;

static AppState g_app = {0};

static void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)device;
    (void)input;
    
    if (!g_app.graph) {
        memset(output, 0, frame_count * 2 * sizeof(float));
        return;
    }
    
    // Process graph (MIDI and MIDI Learn are handled automatically!)
    SituationProcessGraph(g_app.graph, (float*)output, frame_count, NULL, 0);
}

static double get_time_seconds(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

int main(void) {
    printf("=== MIDI Learn Node Integration Example ===\n\n");
    
    // Create graph
    g_app.graph = SituationCreateGraph();
    
    // Create compander node
    SituationError err = SituationCreateNode(g_app.graph, SITUATION_NODE_COMPANDER, &g_app.compander);
    if (err != SITUATION_SUCCESS) {
        printf("Failed to create compander node\n");
        return 1;
    }
    
    // Enable MIDI control
    printf("Enabling MIDI control...\n");
    err = SituationAutoConnectMidi(g_app.graph, g_app.compander);
    
    if (err == SITUATION_SUCCESS) {
        printf("✓ MIDI control enabled!\n\n");
    } else if (err == SITUATION_ERROR_MIDI_NO_DEVICES) {
        printf("⚠ No MIDI devices found. Exiting.\n");
        return 1;
    } else {
        printf("✗ Failed to enable MIDI control (error %d)\n", err);
        return 1;
    }
    
    // Enable MIDI Learn
    printf("Enabling MIDI Learn...\n");
    err = SituationEnableMidiLearn(g_app.graph, g_app.compander);
    
    if (err == SITUATION_SUCCESS) {
        printf("✓ MIDI Learn enabled!\n\n");
    } else {
        printf("✗ Failed to enable MIDI Learn (error %d)\n", err);
        return 1;
    }
    
    // List available MIDI devices
    SituationMidiDeviceInfo devices[32];
    int count = SituationListMidiDevices(devices, 32);
    printf("Available MIDI devices:\n");
    for (int i = 0; i < count; i++) {
        printf("  [%d] %s (%s)\n", i, devices[i].device_name,
               devices[i].is_input ? "Input" : "Output");
    }
    printf("\n");
    
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
    
    printf("Audio running. MIDI Learn is active.\n\n");
    
    // Interactive menu
    printf("Commands:\n");
    printf("  1 - Learn Band 0 Comp Threshold (control 0)\n");
    printf("  2 - Learn Band 0 Exp Threshold (control 1)\n");
    printf("  3 - Learn Band 1 Comp Threshold (control 8)\n");
    printf("  4 - Cancel learning\n");
    printf("  5 - Save preset to 'my_compander_preset.json'\n");
    printf("  6 - Load preset from 'my_compander_preset.json'\n");
    printf("  7 - Clear all learned mappings\n");
    printf("  q - Quit\n\n");
    
    g_app.last_update_time = get_time_seconds();
    
    char input[256];
    while (1) {
        // Update MIDI Learn timeout (call periodically from UI thread)
        double current_time = get_time_seconds();
        if (current_time - g_app.last_update_time > 0.1) {  // Every 100ms
            SituationNode* node = SituationGetNode(g_app.graph, g_app.compander);
            if (node && node->learn_state) {
                SIT_MidiLearn_Update(node->learn_state, current_time);
            }
            g_app.last_update_time = current_time;
        }
        
        printf("> ");
        if (!fgets(input, sizeof(input), stdin)) break;
        
        if (input[0] == 'q' || input[0] == 'Q') {
            break;
        }
        else if (input[0] == '1') {
            printf("Learning Band 0 Comp Threshold (control 0)...\n");
            printf("Move a knob/fader on your MIDI controller now!\n");
            err = SituationStartMidiLearn(g_app.graph, g_app.compander, 
                                          0, "Band 0 Comp Threshold", 
                                          0.0f, 1.0f, 0);  // 0 = linear
            if (err == SITUATION_SUCCESS) {
                printf("✓ Learning started (5 second timeout)\n\n");
            } else {
                printf("✗ Failed to start learning (error %d)\n\n", err);
            }
        }
        else if (input[0] == '2') {
            printf("Learning Band 0 Exp Threshold (control 1)...\n");
            printf("Move a knob/fader on your MIDI controller now!\n");
            err = SituationStartMidiLearn(g_app.graph, g_app.compander, 
                                          1, "Band 0 Exp Threshold", 
                                          0.0f, 1.0f, 0);
            if (err == SITUATION_SUCCESS) {
                printf("✓ Learning started (5 second timeout)\n\n");
            } else {
                printf("✗ Failed to start learning (error %d)\n\n", err);
            }
        }
        else if (input[0] == '3') {
            printf("Learning Band 1 Comp Threshold (control 8)...\n");
            printf("Move a knob/fader on your MIDI controller now!\n");
            err = SituationStartMidiLearn(g_app.graph, g_app.compander, 
                                          8, "Band 1 Comp Threshold", 
                                          0.0f, 1.0f, 0);
            if (err == SITUATION_SUCCESS) {
                printf("✓ Learning started (5 second timeout)\n\n");
            } else {
                printf("✗ Failed to start learning (error %d)\n\n", err);
            }
        }
        else if (input[0] == '4') {
            err = SituationCancelMidiLearn(g_app.graph, g_app.compander);
            if (err == SITUATION_SUCCESS) {
                printf("✓ Learning cancelled\n\n");
            } else {
                printf("✗ Failed to cancel learning (error %d)\n\n", err);
            }
        }
        else if (input[0] == '5') {
            err = SituationSaveMidiPreset(g_app.graph, g_app.compander, 
                                          "my_compander_preset.json");
            if (err == SITUATION_SUCCESS) {
                printf("✓ Preset saved to 'my_compander_preset.json'\n\n");
            } else {
                printf("✗ Failed to save preset (error %d)\n\n", err);
            }
        }
        else if (input[0] == '6') {
            err = SituationLoadMidiPreset(g_app.graph, g_app.compander, 
                                          "my_compander_preset.json");
            if (err == SITUATION_SUCCESS) {
                printf("✓ Preset loaded from 'my_compander_preset.json'\n\n");
            } else {
                printf("✗ Failed to load preset (error %d)\n\n", err);
            }
        }
        else if (input[0] == '7') {
            err = SituationClearAllMidiMappings(g_app.graph, g_app.compander);
            if (err == SITUATION_SUCCESS) {
                printf("✓ All learned mappings cleared\n\n");
            } else {
                printf("✗ Failed to clear mappings (error %d)\n\n", err);
            }
        }
        else {
            printf("Unknown command\n\n");
        }
    }
    
    // Cleanup (MIDI and MIDI Learn cleanup is automatic!)
    ma_device_uninit(&audio_device);
    SituationDestroyGraph(g_app.graph);
    
    printf("\nCleanup complete.\n");
    return 0;
}
