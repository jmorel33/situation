#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"
#include <stdio.h>
#include <windows.h>

void device_change_callback(void *user_data) {
    printf("[CALLBACK] MIDI device list changed!\n");
    (void)user_data;
}

int main(void) {
    printf("=== MIDI Library Test ===\n\n");
    
    // Initialize
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("[ERROR] Failed to initialize MIDI: %d\n", err);
        return 1;
    }
    printf("[SUCCESS] MIDI initialized\n");
    
    // Set device change callback
    Pm_SetDeviceChangeCallback(device_change_callback, NULL);
    printf("[SUCCESS] Device change callback set\n\n");
    
    // Count devices
    int device_count = Pm_CountDevices();
    printf("Found %d MIDI devices:\n", device_count);
    
    // List all devices
    for (int i = 0; i < device_count; i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if (info) {
            printf("  [%d] %s (%s) - %s%s%s\n", 
                i, 
                info->name, 
                info->interf,
                info->input ? "IN" : "",
                (info->input && info->output) ? "/" : "",
                info->output ? "OUT" : ""
            );
        }
    }
    printf("\n");
    
    // Get default devices
    PmDeviceID default_in = Pm_GetDefaultInputDeviceID();
    PmDeviceID default_out = Pm_GetDefaultOutputDeviceID();
    printf("Default input device: %d\n", default_in);
    printf("Default output device: %d\n\n", default_out);
    
    // Test opening output device if available
    if (default_out != PM_NO_DEVICE) {
        PmStream *out_stream = NULL;
        err = Pm_OpenOutput(&out_stream, default_out, NULL, 0, NULL, NULL, 0);
        if (err == pmNoError) {
            printf("[SUCCESS] Opened output device %d\n", default_out);
            
            // Send a test note (Middle C, velocity 64)
            PmEvent event;
            event.message = Pm_Message(0x90, 60, 64); // Note On, channel 0
            event.timestamp = 0;
            
            err = Pm_Write(out_stream, &event, 1);
            if (err == pmNoError) {
                printf("[SUCCESS] Sent Note On (Middle C)\n");
                Sleep(500); // Let note play
                
                // Note Off
                event.message = Pm_Message(0x80, 60, 0);
                Pm_Write(out_stream, &event, 1);
                printf("[SUCCESS] Sent Note Off\n");
            } else {
                printf("[ERROR] Failed to write MIDI: %d\n", err);
            }
            
            Pm_Close(out_stream);
            printf("[SUCCESS] Closed output device\n");
        } else {
            printf("[ERROR] Failed to open output device: %d\n", err);
        }
    } else {
        printf("[INFO] No output device available for testing\n");
    }
    
    printf("\n");
    
    // Test opening input device if available
    if (default_in != PM_NO_DEVICE) {
        PmStream *in_stream = NULL;
        err = Pm_OpenInput(&in_stream, default_in, NULL, 512, NULL, NULL);
        if (err == pmNoError) {
            printf("[SUCCESS] Opened input device %d\n", default_in);
            printf("[INFO] Listening for MIDI input for 3 seconds...\n");
            
            // Read MIDI for 3 seconds
            for (int i = 0; i < 30; i++) {
                PmEvent buffer[32];
                int count = Pm_Read(in_stream, buffer, 32);
                if (count > 0) {
                    for (int j = 0; j < count; j++) {
                        printf("  MIDI: status=0x%02X data1=%d data2=%d\n",
                            Pm_MessageStatus(buffer[j].message),
                            Pm_MessageData1(buffer[j].message),
                            Pm_MessageData2(buffer[j].message)
                        );
                    }
                }
                Sleep(100);
            }
            
            Pm_Close(in_stream);
            printf("[SUCCESS] Closed input device\n");
        } else {
            printf("[ERROR] Failed to open input device: %d\n", err);
        }
    } else {
        printf("[INFO] No input device available for testing\n");
    }
    
    // Cleanup
    Pm_Terminate();
    printf("\n[SUCCESS] MIDI terminated\n");
    printf("=== All Tests Complete ===\n");
    
    return 0;
}
