/***************************************************************************************************
*
*   examples/midi_compander_control.c - MIDI-Controlled Compander Example
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Demonstrates the centralized MIDI callback system (midi_device_callbacks.h) controlling
*   a compander device through the node graph system.
*   
*   This example shows:
*   - How to create a MIDI-enabled device using the callback lookup table
*   - How MIDI CC messages map to device control parameters
*   - How the control array flows from MIDI → callbacks → device wrapper → DSP
*   
*   ARCHITECTURE FLOW:
*   
*       MIDI Controller (CC 16-39)
*           ↓
*       midi.h (low-level MIDI routing)
*           ↓
*       midi_device.h (callback dispatch)
*           ↓
*       midi_device_callbacks.h (CC → control array mapping)
*           ↓
*       device_wrappers.h (control array → DSP parameters)
*           ↓
*       compander.h (DSP processing)
*   
*   MIDI CC MAPPING (Compander):
*   - Band 0 (Low):    CC 16-23 → Controls 0-7
*   - Band 1 (Mid):    CC 24-31 → Controls 8-15
*   - Band 2 (High):   CC 32-39 → Controls 16-23
*   
***************************************************************************************************/

#define MINIAUDIO_IMPLEMENTATION
#include "../sit/miniaudio.h"

#define SITUATION_IMPLEMENTATION
#include "../sit/situation.h"

#include "../sit/aud/midi.h"
#include "../sit/aud/midi_device.h"
#include "../sit/aud/midi_device_callbacks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ================================================================================================
// GLOBAL STATE
// ================================================================================================

typedef struct {
    SituationGraph* graph;
    SituationNode* input_node;
    SituationNode* compander_node;
    SituationNode* output_node;
    SIT_MidiDevice* midi_device;
    PmStream* midi_in;
    float* control_array;  // 24 controls for compander
} AppState;

static AppState g_app = {0};

// ================================================================================================
// AUDIO CALLBACK
// ================================================================================================

static void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)device;
    (void)input;
    
    if (!g_app.graph) {
        memset(output, 0, frame_count * 2 * sizeof(float));
        return;
    }
    
    // Process MIDI events (sample-accurate)
    if (g_app.midi_in) {
        PmEvent events[32];
        int count = Pm_Read(g_app.midi_in, events, 32);
        
        for (int i = 0; i < count; i++) {
            PmMessage msg = events[i].message;
            uint8_t status = Pm_MessageStatus(msg);
            uint8_t data1 = Pm_MessageData1(msg);
            uint8_t data2 = Pm_MessageData2(msg);
            
            // Dispatch to MIDI device (which calls our callback)
            if ((status & 0xF0) == 0xB0) {  // Control Change
                SIT_MidiDevice_ProcessControlChange(g_app.midi_device, status & 0x0F, data1, data2);
            }
        }
    }
    
    // Process audio through node graph
    SituationProcessGraph(g_app.graph, (float*)output, frame_count);
}

// ================================================================================================
// SETUP FUNCTIONS
// ================================================================================================

static bool setup_midi(void) {
    // Initialize PortMidi
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("Failed to initialize PortMidi: %s\n", Pm_GetErrorText(err));
        return false;
    }
    
    // Find first MIDI input device
    int device_count = Pm_CountDevices();
    int input_device = -1;
    
    for (int i = 0; i < device_count; i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info->input) {
            input_device = i;
            printf("Using MIDI input: %s\n", info->name);
            break;
        }
    }
    
    if (input_device == -1) {
        printf("No MIDI input devices found. Using virtual MIDI only.\n");
        return true;  // Not an error, just no hardware
    }
    
    // Open MIDI input
    err = Pm_OpenInput(&g_app.midi_in, input_device, NULL, 512, NULL, NULL);
    if (err != pmNoError) {
        printf("Failed to open MIDI input: %s\n", Pm_GetErrorText(err));
        return false;
    }
    
    return true;
}

static bool setup_audio_graph(void) {
    // Create graph
    g_app.graph = SituationCreateGraph(48000.0f, 512);
    if (!g_app.graph) {
        printf("Failed to create audio graph\n");
        return false;
    }
    
    // Create nodes
    g_app.input_node = SituationCreateNode(g_app.graph, SITUATION_NODE_PASSTHROUGH);
    g_app.compander_node = SituationCreateNode(g_app.graph, SITUATION_NODE_COMPANDER);
    g_app.output_node = SituationCreateNode(g_app.graph, SITUATION_NODE_PASSTHROUGH);
    
    if (!g_app.input_node || !g_app.compander_node || !g_app.output_node) {
        printf("Failed to create nodes\n");
        return false;
    }
    
    // Connect nodes: input → compander → output
    SituationConnectNodes(g_app.graph, g_app.input_node, 0, g_app.compander_node, 0);
    SituationConnectNodes(g_app.graph, g_app.compander_node, 0, g_app.output_node, 0);
    
    // Allocate control array (24 controls for compander)
    g_app.control_array = (float*)calloc(24, sizeof(float));
    if (!g_app.control_array) {
        printf("Failed to allocate control array\n");
        return false;
    }
    
    // Initialize with default values
    for (int band = 0; band < 3; band++) {
        int base = band * 8;
        g_app.control_array[base + 0] = 0.6f;   // comp_thresh
        g_app.control_array[base + 1] = 0.3f;   // exp_thresh
        g_app.control_array[base + 2] = 2.0f;   // comp_slope
        g_app.control_array[base + 3] = 2.0f;   // exp_slope
        g_app.control_array[base + 4] = -60.0f; // noise_gate
        g_app.control_array[base + 5] = 1000.0f;// bell_freq
        g_app.control_array[base + 6] = 0.0f;   // bell_gain
        g_app.control_array[base + 7] = 1.0f;   // bell_Q
    }
    
    // Setup MIDI control using callback lookup table
    const SIT_MidiCallbackEntry* entry = SIT_GetMidiCallbackForDevice(SITUATION_NODE_COMPANDER);
    if (!entry) {
        printf("No MIDI callback found for compander\n");
        return false;
    }
    
    g_app.midi_device = SIT_MidiDevice_Create(entry->device_name,
                                               SIT_MIDI_DEVICE_EFFECT,
                                               SIT_MIDI_CAP_INPUT,
                                               g_app.compander_node);
    if (!g_app.midi_device) {
        printf("Failed to create MIDI device\n");
        return false;
    }
    
    // Set callbacks
    SIT_MidiCallbacks callbacks = {0};
    callbacks.on_control_change = entry->on_control_change;
    callbacks.user_data = g_app.control_array;  // MIDI writes directly to control array
    
    SIT_MidiDevice_SetCallbacks(g_app.midi_device, &callbacks);
    
    printf("MIDI-controlled compander ready!\n");
    printf("Send CC 16-39 to control compander parameters\n");
    
    return true;
}

static bool setup_audio_device(ma_device* device) {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = audio_callback;
    config.periodSizeInFrames = 512;
    
    if (ma_device_init(NULL, &config, device) != MA_SUCCESS) {
        printf("Failed to initialize audio device\n");
        return false;
    }
    
    return true;
}

// ================================================================================================
// MAIN
// ================================================================================================

int main(void) {
    printf("=== MIDI-Controlled Compander Example ===\n\n");
    
    // Setup
    if (!setup_midi()) {
        return 1;
    }
    
    if (!setup_audio_graph()) {
        return 1;
    }
    
    ma_device audio_device;
    if (!setup_audio_device(&audio_device)) {
        return 1;
    }
    
    // Start audio
    if (ma_device_start(&audio_device) != MA_SUCCESS) {
        printf("Failed to start audio device\n");
        return 1;
    }
    
    printf("\nAudio running. Press Enter to quit.\n\n");
    printf("MIDI CC Mapping (STANDARDIZED v2.5.0):\n");
    printf("  Band 0 (Low, <200Hz):    CC 16-23\n");
    printf("  Band 1 (Mid, 200-4kHz):  CC 24-31\n");
    printf("  Band 2 (High, >4kHz):    CC 80-87\n\n");
    printf("Each band has 8 parameters:\n");
    printf("  0: comp_thresh (0.0-1.0)\n");
    printf("  1: exp_thresh (0.0-1.0)\n");
    printf("  2: comp_slope (1.0-10.0)\n");
    printf("  3: exp_slope (1.0-10.0)\n");
    printf("  4: noise_gate (-96dB to 0dB)\n");
    printf("  5: bell_freq (Hz, logarithmic)\n");
    printf("  6: bell_gain (-24dB to +24dB)\n");
    printf("  7: bell_Q (0.1-10.0)\n\n");
    
    getchar();
    
    // Cleanup
    ma_device_uninit(&audio_device);
    
    if (g_app.midi_device) {
        SIT_MidiDevice_Destroy(g_app.midi_device);
    }
    
    if (g_app.midi_in) {
        Pm_Close(g_app.midi_in);
    }
    
    Pm_Terminate();
    
    if (g_app.graph) {
        SituationDestroyGraph(g_app.graph);
    }
    
    if (g_app.control_array) {
        free(g_app.control_array);
    }
    
    printf("Cleanup complete.\n");
    return 0;
}
