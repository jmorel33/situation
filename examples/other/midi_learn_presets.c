/*
 * midi_learn_presets.c - MIDI Learn Preset Save/Load Example
 * 
 * Demonstrates Phase 2 MIDI Learn functionality:
 * - Learn multiple CC mappings
 * - Save mappings to JSON preset file
 * - Load mappings from JSON preset file
 * - Preset management (save/load/clear)
 * 
 * Usage:
 *   1. Run the program
 *   2. Learn some CC mappings (press 'l' and move controllers)
 *   3. Save preset (press 's')
 *   4. Clear mappings (press 'c')
 *   5. Load preset (press 'o')
 *   6. Verify mappings are restored
 */

#define SITUATION_USE_OPENGL
#include "situation.h"

#define MIDI_LEARN_IMPLEMENTATION
#include "../sit/aud/midi_learn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test device with 8 parameters
typedef struct {
    float controls[8];
    const char* control_names[8];
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
            // Find which control this maps to
            for (int i = 0; i < 8; i++) {
                const SIT_MidiLearnMapping *mapping = SIT_MidiLearn_GetMapping(dev->learn_state, i);
                if (mapping && mapping->cc_number == controller) {
                    dev->controls[i] = learned_value;
                    printf("  %s = %.3f (CC %d = %d)\n", 
                           dev->control_names[i], learned_value, controller, value);
                    return;
                }
            }
            return;
        }
    }
    
    // Fallback to hardcoded mappings (CC 16-23 → controls 0-7)
    if (controller >= 16 && controller <= 23) {
        int control_idx = controller - 16;
        dev->controls[control_idx] = value / 127.0f;
        printf("  %s = %.3f (CC %d = %d) [hardcoded]\n", 
               dev->control_names[control_idx], dev->controls[control_idx], controller, value);
    }
}

// Learn complete callback
void OnLearnComplete(void *user_data, const SIT_MidiLearnMapping *mapping) {
    (void)user_data;
    printf("\n✓ Learn complete!\n");
    printf("  CC %d → %s (control %d)\n", 
           mapping->cc_number, mapping->param_name, mapping->control_index);
    printf("  Range: %.2f - %.2f\n", mapping->min_value, mapping->max_value);
    printf("  Scaling: %s\n\n", 
           mapping->scaling == SIT_MIDI_SCALING_LINEAR ? "Linear" :
           mapping->scaling == SIT_MIDI_SCALING_LOG ? "Log" :
           mapping->scaling == SIT_MIDI_SCALING_DB ? "dB" : "Discrete");
}

// Learn timeout callback
void OnLearnTimeout(void *user_data, int control_index) {
    (void)user_data;
    printf("\n✗ Learn timeout for control %d\n\n", control_index);
}

void PrintCurrentValues(TestDevice *device) {
    printf("\nCurrent values:\n");
    for (int i = 0; i < 8; i++) {
        const SIT_MidiLearnMapping *mapping = SIT_MidiLearn_GetMapping(device->learn_state, i);
        if (mapping) {
            printf("  [%d] %s = %.3f (CC %d)\n", 
                   i, device->control_names[i], device->controls[i], mapping->cc_number);
        } else {
            printf("  [%d] %s = %.3f (not mapped)\n", 
                   i, device->control_names[i], device->controls[i]);
        }
    }
    printf("\n");
}

int main(void) {
    printf("=== MIDI Learn Preset Example ===\n\n");
    
    // Initialize PortMidi
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("Failed to initialize PortMidi: %s\n", Pm_GetErrorText(err));
        return 1;
    }
    
    // Create test device
    TestDevice device;
    memset(&device, 0, sizeof(device));
    
    // Initialize control names
    device.control_names[0] = "Volume";
    device.control_names[1] = "Pan";
    device.control_names[2] = "Filter Cutoff";
    device.control_names[3] = "Filter Resonance";
    device.control_names[4] = "Attack";
    device.control_names[5] = "Decay";
    device.control_names[6] = "Sustain";
    device.control_names[7] = "Release";
    
    // Create MIDI Learn state
    device.learn_state = SIT_MidiLearn_Create();
    if (!device.learn_state) {
        printf("Failed to create MIDI Learn state\n");
        Pm_Terminate();
        return 1;
    }
    
    // Set callbacks
    SIT_MidiLearn_SetLearnCompleteCallback(device.learn_state, OnLearnComplete, NULL);
    
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
    printf("  l0-l7 : Start learning control 0-7\n");
    printf("  s     : Save preset to 'my_preset.json'\n");
    printf("  o     : Load preset from 'my_preset.json'\n");
    printf("  c     : Clear all mappings\n");
    printf("  v     : View current values\n");
    printf("  m     : Show current mappings\n");
    printf("  q     : Quit\n\n");
    
    PrintCurrentValues(&device);
    
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
        
        // Check for keyboard input
        #ifdef _WIN32
        if (_kbhit()) {
            char cmd = _getch();
        #else
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;
        
        if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
            char cmd;
            if (read(STDIN_FILENO, &cmd, 1) > 0) {
        #endif
            
            switch (cmd) {
                case 'l':
                case 'L': {
                    printf("Enter control index (0-7): ");
                    int idx;
                    scanf("%d", &idx);
                    if (idx >= 0 && idx < 8) {
                        SIT_MidiScaling scaling = SIT_MIDI_SCALING_LINEAR;
                        float min_val = 0.0f, max_val = 1.0f;
                        
                        // Use appropriate scaling for certain parameters
                        if (idx == 2) {  // Filter Cutoff
                            scaling = SIT_MIDI_SCALING_LOG;
                            min_val = 20.0f;
                            max_val = 20000.0f;
                        }
                        
                        SIT_MidiLearn_Start(device.learn_state, idx, device.control_names[idx], 
                                           min_val, max_val, scaling);
                        printf("Learning %s... (move a MIDI controller)\n", device.control_names[idx]);
                    }
                    break;
                }
                
                case 's':
                case 'S': {
                    printf("Saving preset to 'my_preset.json'...\n");
                    if (SIT_MidiLearn_SavePreset(device.learn_state, "my_preset.json")) {
                        printf("✓ Preset saved successfully!\n\n");
                    } else {
                        printf("✗ Failed to save preset\n\n");
                    }
                    break;
                }
                
                case 'o':
                case 'O': {
                    printf("Loading preset from 'my_preset.json'...\n");
                    if (SIT_MidiLearn_LoadPreset(device.learn_state, "my_preset.json")) {
                        printf("✓ Preset loaded successfully!\n");
                        printf("  Loaded %d mappings\n\n", device.learn_state->mapping_count);
                        PrintCurrentValues(&device);
                    } else {
                        printf("✗ Failed to load preset\n\n");
                    }
                    break;
                }
                
                case 'c':
                case 'C':
                    SIT_MidiLearn_ClearAll(device.learn_state);
                    printf("All mappings cleared\n\n");
                    break;
                
                case 'v':
                case 'V':
                    PrintCurrentValues(&device);
                    break;
                
                case 'm':
                case 'M':
                    printf("\nCurrent mappings:\n");
                    for (int i = 0; i < 8; i++) {
                        const SIT_MidiLearnMapping *mapping = SIT_MidiLearn_GetMapping(device.learn_state, i);
                        if (mapping) {
                            printf("  [%d] %s: CC %d → %.2f - %.2f (%s)\n",
                                   i, mapping->param_name, mapping->cc_number,
                                   mapping->min_value, mapping->max_value,
                                   mapping->scaling == SIT_MIDI_SCALING_LINEAR ? "Linear" :
                                   mapping->scaling == SIT_MIDI_SCALING_LOG ? "Log" :
                                   mapping->scaling == SIT_MIDI_SCALING_DB ? "dB" : "Discrete");
                        } else {
                            printf("  [%d] Not mapped\n", i);
                        }
                    }
                    printf("\n");
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
