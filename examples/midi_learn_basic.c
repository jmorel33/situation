/*
 * midi_learn_basic.c - Basic MIDI Learn Example
 * 
 * Demonstrates Phase 1 MIDI Learn functionality:
 * - Auto-select first available MIDI input device
 * - List available MIDI devices
 * - Learn CC mappings for parameters
 * - Apply learned mappings in real-time
 * 
 * Usage:
 *   1. Run the program
 *   2. Press 'l' to start learning a parameter
 *   3. Move a knob/fader on your MIDI controller
 *   4. The CC is captured and mapped to that parameter
 *   5. Move the same control to see the parameter update
 */

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "../sit/situation.h"

#define MIDI_LEARN_IMPLEMENTATION
#include "../sit/aud/midi_learn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple test device with 4 parameters
typedef struct {
    float controls[4];
    SIT_MidiLearnState *learn_state;
    PmStream *midi_input;
} TestDevice;

// MIDI callback with MIDI Learn integration
void TestDevice_OnControlChange(void *device, uint8_t controller, uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    TestDevice *dev = (TestDevice*)device;
    
    // Check MIDI Learn first
    if (dev->learn_state) {
        float learned_value;
        if (SIT_MidiLearn_ProcessCC(dev->learn_state, 0xFF, controller, value, &learned_value)) {
            // Learning captured this CC or it's a learned mapping
            if (dev->learn_state->learning) {
                printf("Learned: CC %d → Control %d\n", controller, dev->learn_state->learn_control_index);
            }
            
            // Find which control this maps to
            const SIT_MidiLearnMapping *mapping = SIT_MidiLearn_GetMapping(dev->learn_state, dev->learn_state->learn_control_index);
            if (mapping) {
                dev->controls[mapping->control_index] = learned_value;
                printf("Control %d = %.3f (CC %d = %d)\n", 
                       mapping->control_index, learned_value, controller, value);
            }
            return;
        }
    }
    
    // Fallback to hardcoded mappings (CC 16-19 → controls 0-3)
    if (controller >= 16 && controller <= 19) {
        int control_idx = controller - 16;
        dev->controls[control_idx] = value / 127.0f;
        printf("Control %d = %.3f (CC %d = %d) [hardcoded]\n", 
               control_idx, dev->controls[control_idx], controller, value);
    }
}

// Learn complete callback
void OnLearnComplete(void *user_data, const SIT_MidiLearnMapping *mapping) {
    (void)user_data;
    printf("\n✓ Learn complete!\n");
    printf("  CC %d → %s (control %d)\n", 
           mapping->cc_number, mapping->param_name, mapping->control_index);
    printf("  Range: %.2f - %.2f\n", mapping->min_value, mapping->max_value);
    printf("  Scaling: %s\n", 
           mapping->scaling == SIT_MIDI_SCALING_LINEAR ? "Linear" :
           mapping->scaling == SIT_MIDI_SCALING_LOG ? "Log" :
           mapping->scaling == SIT_MIDI_SCALING_DB ? "dB" : "Discrete");
    printf("\n");
}

// Learn timeout callback
void OnLearnTimeout(void *user_data, int control_index) {
    (void)user_data;
    printf("\n✗ Learn timeout for control %d\n\n", control_index);
}

// Conflict callback
int OnConflict(void *user_data, int cc_number, int existing_control) {
    (void)user_data;
    printf("\n⚠ Warning: CC %d is already mapped to control %d\n", cc_number, existing_control);
    printf("Overwrite? (y/n): ");
    char response;
    scanf(" %c", &response);
    return (response == 'y' || response == 'Y');
}

int main(void) {
    printf("=== MIDI Learn Basic Example ===\n\n");
    
    // Initialize PortMidi
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("Failed to initialize PortMidi: %s\n", Pm_GetErrorText(err));
        return 1;
    }
    
    // Create test device
    TestDevice device;
    memset(&device, 0, sizeof(device));
    
    // Create MIDI Learn state
    device.learn_state = SIT_MidiLearn_Create();
    if (!device.learn_state) {
        printf("Failed to create MIDI Learn state\n");
        Pm_Terminate();
        return 1;
    }
    
    // Set callbacks
    SIT_MidiLearn_SetLearnCompleteCallback(device.learn_state, OnLearnComplete, NULL);
    SIT_MidiLearn_SetConflictCallback(device.learn_state, OnConflict, NULL);
    
    // List available MIDI devices
    printf("Available MIDI Devices:\n");
    SIT_MidiLearnDeviceInfo devices[32];
    int device_count = SIT_MidiLearn_ListInputDevices(devices, 32);
    
    if (device_count == 0) {
        printf("  No MIDI devices found\n\n");
    } else {
        for (int i = 0; i < device_count; i++) {
            printf("  [%d] %s %s\n", 
                   i, 
                   devices[i].device_name,
                   devices[i].is_input ? "(Input)" : "(Output)");
        }
        printf("\n");
    }
    
    // Auto-select first MIDI input
    PmDeviceID selected_device = SIT_MidiLearn_AutoSelectInput();
    if (selected_device == PM_NO_DEVICE) {
        printf("No MIDI input devices available\n");
        printf("Please connect a MIDI controller and restart\n");
        SIT_MidiLearn_Destroy(device.learn_state);
        Pm_Terminate();
        return 1;
    }
    
    // Set input device
    if (!SIT_MidiLearn_SetInputDevice(device.learn_state, selected_device)) {
        printf("Failed to open MIDI device\n");
        SIT_MidiLearn_Destroy(device.learn_state);
        Pm_Terminate();
        return 1;
    }
    
    const PmDeviceInfo *info = Pm_GetDeviceInfo(selected_device);
    printf("Learning from: %s\n\n", info ? info->name : "Unknown");
    
    // Open MIDI input for processing
    err = Pm_OpenInput(&device.midi_input, selected_device, NULL, 512, NULL, NULL);
    if (err != pmNoError) {
        printf("Failed to open MIDI input: %s\n", Pm_GetErrorText(err));
        SIT_MidiLearn_Destroy(device.learn_state);
        Pm_Terminate();
        return 1;
    }
    
    // Print instructions
    printf("Commands:\n");
    printf("  l0-l3 : Start learning control 0-3\n");
    printf("  c     : Cancel learning\n");
    printf("  s     : Show current mappings\n");
    printf("  r     : Clear all mappings\n");
    printf("  q     : Quit\n\n");
    
    printf("Current values:\n");
    for (int i = 0; i < 4; i++) {
        printf("  Control %d: %.3f\n", i, device.controls[i]);
    }
    printf("\n");
    
    // Main loop
    int running = 1;
    while (running) {
        // Process MIDI events
        PmEvent events[32];
        int count = Pm_Read(device.midi_input, events, 32);
        
        for (int i = 0; i < count; i++) {
            PmMessage msg = events[i].message;
            uint8_t status = Pm_MessageStatus(msg);
            uint8_t msg_type = status & 0xF0;
            
            if (msg_type == 0xB0) {  // Control Change
                uint8_t controller = Pm_MessageData1(msg);
                uint8_t value = Pm_MessageData2(msg);
                TestDevice_OnControlChange(&device, controller, value, 0);
            }
        }
        
        // Check for timeout
        SIT_MidiLearn_Update(device.learn_state, Pm_Time() / 1000.0);
        
        // Check for keyboard input (non-blocking)
        #ifdef _WIN32
        if (_kbhit()) {
            char cmd = _getch();
        #else
        // Simple blocking input for Unix
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;  // 10ms timeout
        
        if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
            char cmd;
            if (read(STDIN_FILENO, &cmd, 1) > 0) {
        #endif
            
            switch (cmd) {
                case 'l':
                case 'L': {
                    // Read control index
                    printf("Enter control index (0-3): ");
                    int idx;
                    scanf("%d", &idx);
                    if (idx >= 0 && idx < 4) {
                        char param_name[64];
                        snprintf(param_name, sizeof(param_name), "Control %d", idx);
                        SIT_MidiLearn_Start(device.learn_state, idx, param_name, 0.0f, 1.0f, SIT_MIDI_SCALING_LINEAR);
                        printf("Learning control %d... (move a MIDI controller)\n", idx);
                    }
                    break;
                }
                
                case 'c':
                case 'C':
                    SIT_MidiLearn_Cancel(device.learn_state);
                    printf("Learning cancelled\n");
                    break;
                
                case 's':
                case 'S':
                    printf("\nCurrent mappings:\n");
                    for (int i = 0; i < 4; i++) {
                        const SIT_MidiLearnMapping *mapping = SIT_MidiLearn_GetMapping(device.learn_state, i);
                        if (mapping) {
                            printf("  Control %d: CC %d → %s (%.2f - %.2f)\n",
                                   i, mapping->cc_number, mapping->param_name,
                                   mapping->min_value, mapping->max_value);
                        } else {
                            printf("  Control %d: Not mapped\n", i);
                        }
                    }
                    printf("\n");
                    break;
                
                case 'r':
                case 'R':
                    SIT_MidiLearn_ClearAll(device.learn_state);
                    printf("All mappings cleared\n");
                    break;
                
                case 'q':
                case 'Q':
                    running = 0;
                    break;
            }
            
        #ifndef _WIN32
            }
        }
        #endif
        
        // Small delay
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }
    
    // Cleanup
    printf("\nCleaning up...\n");
    Pm_Close(device.midi_input);
    SIT_MidiLearn_Destroy(device.learn_state);
    Pm_Terminate();
    
    printf("Done!\n");
    return 0;
}
