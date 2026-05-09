/***************************************************************************************************
*
*   examples/midi_14bit_example.c - 14-bit MIDI CC Example
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Demonstrates high-resolution 14-bit MIDI control using MSB/LSB CC pairs.
*   
*   14-BIT MIDI CC PROTOCOL:
*   - MSB (Most Significant Byte): CC 0-31 (coarse control, 7 bits)
*   - LSB (Least Significant Byte): CC 32-63 (fine control, 7 bits)
*   - Combined: (MSB << 7) | LSB = 0-16383 (14 bits)
*   
*   EXAMPLE MAPPING:
*   - CC 1 (MSB) + CC 33 (LSB) → Filter Cutoff (20Hz-20kHz)
*     - 7-bit:  128 steps, ~157Hz resolution
*     - 14-bit: 16384 steps, ~1.2Hz resolution (128x more precise!)
*   
*   BENEFITS:
*   - Smooth parameter sweeps (no zipper noise)
*   - Precise control for critical parameters (cutoff, pitch, volume)
*   - Standard MIDI protocol (widely supported)
*   - Backward compatible (MSB works alone as 7-bit)
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
// 14-BIT FILTER STATE
// ================================================================================================

typedef struct {
    float* controls;                // Control array (6 controls for filter)
    SIT_MidiCC14Bit cutoff_14bit;   // Track CC 1 (MSB) + CC 33 (LSB)
    SIT_MidiCC14Bit resonance_14bit;// Track CC 2 (MSB) + CC 34 (LSB)
} Filter14BitState;

// ================================================================================================
// 14-BIT CALLBACK
// ================================================================================================

/**
 * @brief MIDI CC callback with 14-bit support for filter.
 * 
 * MIDI CC MAPPING:
 *   CC 1 (MSB) + CC 33 (LSB) → Cutoff (20Hz-20kHz, 14-bit, ~1.2Hz resolution)
 *   CC 2 (MSB) + CC 34 (LSB) → Resonance (0.1-10.0, 14-bit)
 *   CC 40 → Type (7-bit, discrete)
 *   CC 43 → Gain (7-bit)
 *   CC 44 → Drive (7-bit)
 *   CC 45 → Oversampling (7-bit, discrete)
 */
static void Filter14BitOnControlChange(void* user_data, uint8_t channel,
                                        uint8_t cc_number, uint8_t cc_value) {
    (void)channel;
    
    Filter14BitState* state = (Filter14BitState*)user_data;
    if (!state || !state->controls) return;
    
    // Handle 14-bit cutoff (CC 1 MSB + CC 33 LSB)
    if (cc_number == 1 || cc_number == 33) {
        if (_SituationUpdate14BitCC(&state->cutoff_14bit, cc_number, cc_value)) {
            uint16_t value_14bit = _SituationGet14BitValue(&state->cutoff_14bit);
            state->controls[1] = _SituationNormalize14BitCCLog(value_14bit, 20.0f, 20000.0f);
            
            printf("Cutoff 14-bit: %5d → %7.2f Hz (MSB=%3d, LSB=%3d)\n",
                   value_14bit, state->controls[1],
                   state->cutoff_14bit.msb, state->cutoff_14bit.lsb);
        }
    }
    // Handle 14-bit resonance (CC 2 MSB + CC 34 LSB)
    else if (cc_number == 2 || cc_number == 34) {
        if (_SituationUpdate14BitCC(&state->resonance_14bit, cc_number, cc_value)) {
            uint16_t value_14bit = _SituationGet14BitValue(&state->resonance_14bit);
            state->controls[2] = _SituationNormalize14BitCC(value_14bit, 0.1f, 10.0f);
            
            printf("Resonance 14-bit: %5d → %5.3f (MSB=%3d, LSB=%3d)\n",
                   value_14bit, state->controls[2],
                   state->resonance_14bit.msb, state->resonance_14bit.lsb);
        }
    }
    // Handle regular 7-bit CCs
    else {
        switch (cc_number) {
            case 40:
                state->controls[0] = floorf(_SituationNormalizeMidiCC(cc_value, 0.0f, 6.99f));
                printf("Type: %d\n", (int)state->controls[0]);
                break;
            case 43:
                state->controls[3] = _SituationNormalizeMidiCCDb(cc_value, -24.0f, 24.0f);
                printf("Gain: %.2f dB\n", state->controls[3]);
                break;
            case 44:
                state->controls[4] = _SituationNormalizeMidiCC(cc_value, 1.0f, 10.0f);
                printf("Drive: %.2f\n", state->controls[4]);
                break;
            case 45:
                state->controls[5] = floorf(_SituationNormalizeMidiCC(cc_value, 0.0f, 2.99f));
                printf("Oversampling: %d\n", (int)state->controls[5]);
                break;
        }
    }
}

// ================================================================================================
// GLOBAL STATE
// ================================================================================================

typedef struct {
    SIT_MidiDevice* midi_device;
    PmStream* midi_in;
    Filter14BitState filter_state;
} AppState;

static AppState g_app = {0};

// ================================================================================================
// SETUP FUNCTIONS
// ================================================================================================

static bool setup_midi(void) {
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("Failed to initialize PortMidi: %s\n", Pm_GetErrorText(err));
        return false;
    }
    
    int device_count = Pm_CountDevices();
    int input_device = -1;
    
    printf("Available MIDI input devices:\n");
    for (int i = 0; i < device_count; i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info->input) {
            printf("  [%d] %s\n", i, info->name);
            if (input_device == -1) {
                input_device = i;
            }
        }
    }
    
    if (input_device == -1) {
        printf("No MIDI input devices found.\n");
        return false;
    }
    
    printf("\nUsing MIDI input: %s\n\n", Pm_GetDeviceInfo(input_device)->name);
    
    err = Pm_OpenInput(&g_app.midi_in, input_device, NULL, 512, NULL, NULL);
    if (err != pmNoError) {
        printf("Failed to open MIDI input: %s\n", Pm_GetErrorText(err));
        return false;
    }
    
    return true;
}

static bool setup_midi_device(void) {
    // Allocate control array (6 controls for filter)
    g_app.filter_state.controls = (float*)calloc(6, sizeof(float));
    if (!g_app.filter_state.controls) {
        printf("Failed to allocate control array\n");
        return false;
    }
    
    // Initialize with defaults
    g_app.filter_state.controls[0] = 0.0f;     // type (lowpass)
    g_app.filter_state.controls[1] = 1000.0f;  // cutoff
    g_app.filter_state.controls[2] = 1.0f;     // Q
    g_app.filter_state.controls[3] = 0.0f;     // gain
    g_app.filter_state.controls[4] = 1.0f;     // drive
    g_app.filter_state.controls[5] = 0.0f;     // oversampling
    
    // Initialize 14-bit state
    memset(&g_app.filter_state.cutoff_14bit, 0, sizeof(SIT_MidiCC14Bit));
    memset(&g_app.filter_state.resonance_14bit, 0, sizeof(SIT_MidiCC14Bit));
    
    // Create MIDI device
    g_app.midi_device = SIT_MidiDevice_Create("Filter 14-bit",
                                               SIT_MIDI_DEVICE_EFFECT,
                                               SIT_MIDI_CAP_INPUT,
                                               NULL);
    if (!g_app.midi_device) {
        printf("Failed to create MIDI device\n");
        return false;
    }
    
    // Set callbacks
    SIT_MidiCallbacks callbacks = {0};
    callbacks.on_control_change = Filter14BitOnControlChange;
    callbacks.user_data = &g_app.filter_state;
    
    SIT_MidiDevice_SetCallbacks(g_app.midi_device, &callbacks);
    
    printf("14-bit MIDI filter ready!\n\n");
    return true;
}

// ================================================================================================
// MAIN LOOP
// ================================================================================================

static void process_midi(void) {
    PmEvent events[32];
    int count = Pm_Read(g_app.midi_in, events, 32);
    
    for (int i = 0; i < count; i++) {
        PmMessage msg = events[i].message;
        uint8_t status = Pm_MessageStatus(msg);
        uint8_t data1 = Pm_MessageData1(msg);
        uint8_t data2 = Pm_MessageData2(msg);
        
        // Dispatch CC messages
        if ((status & 0xF0) == 0xB0) {  // Control Change
            SIT_MidiDevice_ProcessControlChange(g_app.midi_device, status & 0x0F, data1, data2);
        }
    }
}

// ================================================================================================
// MAIN
// ================================================================================================

int main(void) {
    printf("=== 14-bit MIDI CC Example ===\n\n");
    
    if (!setup_midi()) {
        return 1;
    }
    
    if (!setup_midi_device()) {
        return 1;
    }
    
    printf("MIDI CC Mapping (14-bit):\n");
    printf("  CC 1 (MSB) + CC 33 (LSB) → Cutoff (20Hz-20kHz, 16384 steps, ~1.2Hz resolution)\n");
    printf("  CC 2 (MSB) + CC 34 (LSB) → Resonance (0.1-10.0, 16384 steps)\n");
    printf("\n");
    printf("MIDI CC Mapping (7-bit):\n");
    printf("  CC 40 → Type (0-6: LP/HP/BP/Notch/Peak/LS/HS)\n");
    printf("  CC 43 → Gain (-24dB to +24dB)\n");
    printf("  CC 44 → Drive (1.0-10.0)\n");
    printf("  CC 45 → Oversampling (0-2: off/2x/4x)\n");
    printf("\n");
    printf("Resolution Comparison:\n");
    printf("  7-bit:  128 steps   → ~157Hz per step (20Hz-20kHz)\n");
    printf("  14-bit: 16384 steps → ~1.2Hz per step (20Hz-20kHz)\n");
    printf("  Improvement: 128x more precise!\n");
    printf("\n");
    printf("Press Enter to quit.\n\n");
    
    // Main loop
    while (1) {
        process_midi();
        
        // Check for Enter key (non-blocking would be better, but this is simple)
        // In real app, this would be in a separate thread or use select()
        
        // Sleep to avoid busy-waiting
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
        
        // Simple exit check (press Ctrl+C)
    }
    
    // Cleanup
    if (g_app.midi_device) {
        SIT_MidiDevice_Destroy(g_app.midi_device);
    }
    
    if (g_app.midi_in) {
        Pm_Close(g_app.midi_in);
    }
    
    Pm_Terminate();
    
    if (g_app.filter_state.controls) {
        free(g_app.filter_state.controls);
    }
    
    printf("Cleanup complete.\n");
    return 0;
}
