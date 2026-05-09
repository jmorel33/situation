/*
 * midi_learn_14bit.c - MIDI Learn 14-bit CC Example
 * 
 * Demonstrates Phase 3 MIDI Learn functionality:
 * - 14-bit CC learning (automatic MSB/LSB detection)
 * - MIDI channel filtering
 * - High-resolution parameter control
 * 
 * Usage:
 *   1. Run the program
 *   2. Set channel filter (optional)
 *   3. Learn a parameter
 *   4. Move a 14-bit controller (e.g., mod wheel, pitch bend as CC)
 *   5. System automatically detects MSB/LSB pair
 *   6. Enjoy 16384 steps of resolution!
 */

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "../sit/situation.h"

#define MIDI_LEARN_IMPLEMENTATION
#include "../sit/aud/midi_learn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test device with high-resolution parameters
typedef struct {
    float controls[4];
    const char* control_names[4];
    SIT_MidiLearnState *learn_state;
    PmStream *midi_input;
} TestDevice;

// MIDI callback
void TestDevice_OnControlChange(void *device, uint8_t controller, uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    TestDevice *dev = (TestDevice*)device;
    
    // Check MIDI Learn
    if (dev->learn_state) {
        float learned_value;
        if (SIT_MidiLearn_ProcessCC(dev->learn_state, 0xFF, controller, value, &learned_value)) {
            // Find which control this maps to
            for (int i = 0; i < 4; i++) {
                const SIT_MidiLearnMapping *mapping = SIT_MidiLearn_GetMapping(dev->learn_state, i);
                if (mapping && mapping->cc_number == controller) {
                    dev->controls[i] = learned_value;
                    
                    // Show resolution info
                    if (mapping->cc_lsb != 0xFF) {
                        printf("  %s = %.6f (14-bit: CC %d+%d)\n", 
                               dev->control_names[i], learned_value, 
                               mapping->cc_number, mapping->cc_lsb);
                    } else {
                        printf("  %s = %.3f (7-bit: CC %d)\n", 
                               dev->control_names[i], learned_value, controller);
                    }
                    return;
                }
            }
            return;
        }
    }
}

// Learn complete callback
void OnLearnComplete(void *user_data, const SIT_MidiLearnMapping *mapping) {
    (void)user_data;
    printf("\n✓ Learn complete!\n");
    printf("  CC %d → %s (control %d)\n", 
           mapping->cc_number, mapping->param_name, mapping->control_index);
    
    if (mapping->cc_lsb != 0xFF) {
        printf("  14-bit: MSB=%d, LSB=%d (16384 steps)\n", 
               mapping->cc_number, mapping->cc_lsb);
    } else {
        printf("  7-bit: CC %d (128 steps)\n", mapping->cc_number);
    }
    
    printf("  Range: %.2f - %.2f\n", mapping->min_value, mapping->max_value);
    printf("  Channel: %s\n", 
           mapping->channel == 0xFF ? "Omni" : "Filtered");
    printf("\n");
}

void PrintCurrentValues(TestDevice *device) {
    printf("\nCurrent values:\n");
    for (int i = 0; i < 4; i++) {
        const SIT_MidiLearnMapping *mapping = SIT_MidiLearn_GetMapping(device->learn_state, i);
        if (mapping) {
            if (mapping->cc_lsb != 0xFF) {
                printf("  [%d] %s = %.6f (14-bit: CC %d+%d)\n", 
                       i, device->control_names[i], device->controls[i],
                       mapping->cc_number, mapping->cc_lsb);
            } else {
                printf("  [%d] %s = %.3f (7-bit: CC %d)\n", 
                       i, device->control_names[i], device->controls[i],
                       mapping->cc_number);
            }
        } else {
            printf("  [%d] %s = %.6f (not mapped)\n", 
                   i, device->control_names[i], device->controls[i]);
        }
    }
    printf("\n");
}

int main(void) {
    printf("=== MIDI Learn 14-bit Example ===\n\n");
    
    // Initialize PortMidi
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("Failed to initialize PortMidi: %s\n", Pm_GetErrorText(err));
        return 1;
    }
    
    // Create test device
    TestDevice device;
    memset(&device, 0, sizeof(device));
    
    // Initialize control names (high-resolution parameters)
    device.control_names[0] = "Filter Cutoff (Hz)";
    device.control_names[1] = "Modulation Depth";
    device.control_names[2] = "Pitch Bend (cents)";
    device.control_names[3] = "Expression";
    
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
    printf("14-bit CC Learning:\n");
    printf("  - Move a controller slowly to send MSB only (7-bit)\n");
    printf("  - Move a controller quickly to send MSB+LSB (14-bit)\n");
    printf("  - System automatically detects 14-bit pairs\n\n");
    
    printf("Commands:\n");
    printf("  l0-l3 : Start learning control 0-3\n");
    printf("  f0-15 : Set channel filter (0-15)\n");
    printf("  f     : Clear channel filter (omni)\n");
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
                    printf("Enter control index (0-3): ");
                    int idx;
                    scanf("%d", &idx);
                    if (idx >= 0 && idx < 4) {
                        SIT_MidiScaling scaling = SIT_MIDI_SCALING_LINEAR;
                        float min_val = 0.0f, max_val = 1.0f;
                        
                        // Use appropriate scaling
                        if (idx == 0) {  // Filter Cutoff
                            scaling = SIT_MIDI_SCALING_LOG;
                            min_val = 20.0f;
                            max_val = 20000.0f;
                        } else if (idx == 2) {  // Pitch Bend
                            min_val = -200.0f;  // -200 cents
                            max_val = 200.0f;   // +200 cents
                        }
                        
                        SIT_MidiLearn_Start(device.learn_state, idx, device.control_names[idx], 
                                           min_val, max_val, scaling);
                        printf("Learning %s... (move a MIDI controller)\n", device.control_names[idx]);
                        printf("Tip: Move slowly for 7-bit, quickly for 14-bit\n");
                    }
                    break;
                }
                
                case 'f':
                case 'F': {
                    printf("Enter channel (0-15, or press Enter for omni): ");
                    char input[10];
                    if (fgets(input, sizeof(input), stdin)) {
                        if (input[0] == '\n') {
                            SIT_MidiLearn_SetChannelFilter(device.learn_state, 0xFF);
                            printf("Channel filter: Omni (all channels)\n");
                        } else {
                            int ch = atoi(input);
                            if (ch >= 0 && ch <= 15) {
                                SIT_MidiLearn_SetChannelFilter(device.learn_state, (uint8_t)ch);
                                printf("Channel filter: %d\n", ch);
                            }
                        }
                    }
                    break;
                }
                
                case 'v':
                case 'V':
                    PrintCurrentValues(&device);
                    break;
                
                case 'm':
                case 'M':
                    printf("\nCurrent mappings:\n");
                    for (int i = 0; i < 4; i++) {
                        const SIT_MidiLearnMapping *mapping = SIT_MidiLearn_GetMapping(device.learn_state, i);
                        if (mapping) {
                            if (mapping->cc_lsb != 0xFF) {
                                printf("  [%d] %s: 14-bit CC %d+%d (16384 steps)\n",
                                       i, mapping->param_name, 
                                       mapping->cc_number, mapping->cc_lsb);
                            } else {
                                printf("  [%d] %s: 7-bit CC %d (128 steps)\n",
                                       i, mapping->param_name, mapping->cc_number);
                            }
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
