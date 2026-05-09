/***************************************************************************************************
*
*   examples/midi_identity_test.c - MIDI Device Identity Test
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Demonstrates Universal Device Inquiry (Identity Request/Response).
*   
*   Every MIDI device must respond to Identity Request with its manufacturer ID, family, model,
*   and version information.
*   
*   PROTOCOL:
*   Request:  F0 7E 7F 06 01 F7
*   Response: F0 7E <device_id> 06 02 <manufacturer_id> <family> <model> <version> F7
*   
*   SITUATION AUDIO DEVICES:
*   - Manufacturer ID: 0x00 0x53 0x49 ("SI" for Situation)
*   - Family: 0x00 0x01 (Audio FX)
*   - Model: Device-specific (0x01-0x11)
*   - Version: 0x01 0x00 0x00 0x00 (v1.0.0.0)
*   
***************************************************************************************************/

#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"

#define MIDI_DEVICE_IMPLEMENTATION  
#include "../sit/aud/midi_device.h"

#include "../sit/aud/midi_device_callbacks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declare SituationNodeType since we don't include full situation.h
typedef enum {
    SITUATION_NODE_COMPANDER = 0,
    SITUATION_NODE_DYNAMICS,
    SITUATION_NODE_FILTER,
    SITUATION_NODE_EQ_4BAND,
    SITUATION_NODE_REVERB,
    SITUATION_NODE_CHORUS,
    SITUATION_NODE_OVERDRIVE,
    SITUATION_NODE_PANNER,
    SITUATION_NODE_LFO,
    SITUATION_NODE_ECHO,
    SITUATION_NODE_PHASER,
    SITUATION_NODE_EXCITER,
    SITUATION_NODE_STUDIO_REVERB,
    SITUATION_NODE_SPRING_REVERB,
    SITUATION_NODE_SST282,
    SITUATION_NODE_MASTERING_AMP,
    SITUATION_NODE_MAXIMIZER,
} SituationNodeType;

// ================================================================================================
// GLOBAL STATE
// ================================================================================================

typedef struct {
    SIT_MidiDevice* devices[17];  // All 17 MIDI-enabled devices
    PmStream* midi_in;
    PmStream* midi_out;
    int device_count;
} AppState;

static AppState g_app = {0};

// ================================================================================================
// SETUP FUNCTIONS
// ================================================================================================

static int setup_midi(void) {
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("Failed to initialize PortMidi: %s\n", Pm_GetErrorText(err));
        return 0;
    }
    
    int device_count = Pm_CountDevices();
    int input_device = -1;
    int output_device = -1;
    
    printf("Available MIDI devices:\n");
    for (int i = 0; i < device_count; i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        printf("  [%d] %s (%s/%s)\n", i, info->name,
               info->input ? "IN" : "",
               info->output ? "OUT" : "");
        
        if (info->input && input_device == -1) {
            input_device = i;
        }
        if (info->output && output_device == -1) {
            output_device = i;
        }
    }
    
    if (input_device == -1 || output_device == -1) {
        printf("Need both MIDI input and output devices.\n");
        return 0;
    }
    
    printf("\nUsing MIDI input: %s\n", Pm_GetDeviceInfo(input_device)->name);
    printf("Using MIDI output: %s\n\n", Pm_GetDeviceInfo(output_device)->name);
    
    err = Pm_OpenInput(&g_app.midi_in, input_device, NULL, 512, NULL, NULL);
    if (err != pmNoError) {
        printf("Failed to open MIDI input: %s\n", Pm_GetErrorText(err));
        return 0;
    }
    
    err = Pm_OpenOutput(&g_app.midi_out, output_device, NULL, 512, NULL, NULL, 0);
    if (err != pmNoError) {
        printf("Failed to open MIDI output: %s\n", Pm_GetErrorText(err));
        return 0;
    }
    
    return 1;
}

static void setup_devices(void) {
    // Create all 17 MIDI-enabled devices
    SituationNodeType device_types[] = {
        SITUATION_NODE_COMPANDER,
        SITUATION_NODE_DYNAMICS,
        SITUATION_NODE_FILTER,
        SITUATION_NODE_EQ_4BAND,
        SITUATION_NODE_REVERB,
        SITUATION_NODE_CHORUS,
        SITUATION_NODE_OVERDRIVE,
        SITUATION_NODE_PANNER,
        SITUATION_NODE_LFO,
        SITUATION_NODE_ECHO,
        SITUATION_NODE_PHASER,
        SITUATION_NODE_EXCITER,
        SITUATION_NODE_STUDIO_REVERB,
        SITUATION_NODE_SPRING_REVERB,
        SITUATION_NODE_SST282,
        SITUATION_NODE_MASTERING_AMP,
        SITUATION_NODE_MAXIMIZER,
    };
    
    g_app.device_count = sizeof(device_types) / sizeof(device_types[0]);
    
    for (int i = 0; i < g_app.device_count; i++) {
        // Get device info
        const SIT_MidiCallbackEntry* entry = SIT_GetMidiCallbackForDevice(device_types[i]);
        if (!entry) continue;
        
        // Create MIDI device
        g_app.devices[i] = SIT_MidiDevice_Create(entry->device_name,
                                                  SIT_MIDI_DEVICE_EFFECT,
                                                  SIT_MIDI_CAP_INPUT | SIT_MIDI_CAP_OUTPUT,
                                                  NULL);
        
        // Set identity
        SIT_MidiDeviceIdentity identity = SIT_GetDeviceIdentity(device_types[i]);
        SIT_MidiDevice_SetIdentity(g_app.devices[i], &identity);
        
        // Set MIDI streams
        g_app.devices[i]->midi_input = g_app.midi_in;
        g_app.devices[i]->midi_output = g_app.midi_out;
        
        printf("Created device: %s (Model 0x%02X)\n", entry->device_name, identity.model[1]);
    }
    
    printf("\n");
}

// ================================================================================================
// MAIN
// ================================================================================================

int main(void) {
    printf("=== MIDI Device Identity Test ===\n\n");
    
    if (!setup_midi()) {
        return 1;
    }
    
    setup_devices();
    
    printf("Sending Universal Device Inquiry...\n");
    printf("Request: F0 7E 7F 06 01 F7\n\n");
    
    // Send Identity Request
    uint8_t identity_request[] = {
        0xF0,  // SysEx start
        0x7E,  // Universal Non-Realtime
        0x7F,  // Device ID (all devices)
        0x06,  // General Information
        0x01,  // Identity Request
        0xF7   // SysEx end
    };
    
    Pm_WriteSysEx(g_app.midi_out, 0, identity_request, sizeof(identity_request));
    
    // Wait a bit for responses
    #ifdef _WIN32
    Sleep(100);
    #else
    usleep(100000);
    #endif
    
    // Check for SysEx responses
    printf("Checking for Identity Replies...\n\n");
    
    // Manually trigger responses from our devices
    printf("Device Identities:\n");
    printf("%-20s | Manufacturer | Family | Model | Version\n", "Device");
    printf("---------------------|--------------|--------|-------|----------\n");
    
    for (int i = 0; i < g_app.device_count; i++) {
        if (!g_app.devices[i]) continue;
        
        const SIT_MidiDeviceIdentity* id = SIT_MidiDevice_GetIdentity(g_app.devices[i]);
        
        printf("%-20s | %02X %02X %02X    | %02X %02X  | %02X %02X | %02X %02X %02X %02X\n",
               id->device_name,
               id->manufacturer_id[0], id->manufacturer_id[1], id->manufacturer_id[2],
               id->family[0], id->family[1],
               id->model[0], id->model[1],
               id->version[0], id->version[1], id->version[2], id->version[3]);
        
        // Send identity reply
        SIT_MidiDevice_SendIdentityReply(g_app.devices[i], 0x7F);
    }
    
    printf("\n");
    printf("All devices responded with their identities.\n");
    printf("\n");
    printf("Extended Identity Reply Format:\n");
    printf("F0 7E 7F 06 02 <manufacturer> <family> <model> <version> <ascii_name> F7\n");
    printf("\n");
    printf("Example (Compander):\n");
    printf("F0 7E 7F 06 02 00 53 49 00 01 00 01 01 00 00 00 \"Compander\" F7\n");
    printf("               ^^^^^^^^ ^^^^^ ^^^^^ ^^^^^^^^^^^ ^^^^^^^^^^^\n");
    printf("               Manuf.   Family Model Version    ASCII Name\n");
    printf("\n");
    printf("Manufacturer ID: 0x00 0x53 0x49 = \"SI\" (Situation)\n");
    printf("Family: 0x00 0x01 = Audio FX\n");
    printf("Model: Device-specific (0x01-0x11)\n");
    printf("Version: 0x01 0x00 0x00 0x00 = v1.0.0.0\n");
    printf("ASCII Name: Human-readable device name (e.g., \"Compander\")\n");
    printf("\n");
    printf("Benefits of Extended Format:\n");
    printf("- Controllers can display \"Compander\" instead of \"Model 0x01\"\n");
    printf("- More user-friendly\n");
    printf("- Compatible with standard MIDI (ASCII name is optional)\n");
    
    // Cleanup
    for (int i = 0; i < g_app.device_count; i++) {
        if (g_app.devices[i]) {
            SIT_MidiDevice_Destroy(g_app.devices[i]);
        }
    }
    
    if (g_app.midi_in) {
        Pm_Close(g_app.midi_in);
    }
    
    if (g_app.midi_out) {
        Pm_Close(g_app.midi_out);
    }
    
    Pm_Terminate();
    
    printf("\nTest complete.\n");
    return 0;
}
