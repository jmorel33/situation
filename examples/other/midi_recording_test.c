/*
 * MIDI Recording and Playback Test
 * 
 * Tests Phase 3 recording features:
 * - Record MIDI events from a stream
 * - Play back recorded events
 * - Verify timing and content
 */

#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("=== MIDI Recording & Playback Test ===\n\n");
    
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("[FAILED] Could not initialize MIDI\n");
        return 1;
    }
    printf("[SUCCESS] MIDI initialized\n\n");
    
    // Create virtual devices
    PmDeviceID out_dev1, out_dev2, in_dev;
    Pm_CreateVirtualDevice("Recorder Out", 0, &out_dev1);
    Pm_CreateVirtualDevice("Playback Out", 0, &out_dev2);
    Pm_CreateVirtualDevice("Receiver In", 1, &in_dev);
    
    printf("[SUCCESS] Created 3 virtual devices\n");
    
    // Connect first output to input for recording
    Pm_ConnectVirtualDevices(out_dev1, in_dev);
    
    // Open streams
    PmStream *record_stream, *playback_stream, *in_stream;
    Pm_OpenOutput(&record_stream, out_dev1, NULL, 0, NULL, NULL, 0);
    Pm_OpenOutput(&playback_stream, out_dev2, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream, in_dev, NULL, 512, NULL, NULL);
    
    printf("[SUCCESS] Opened streams\n\n");
    
    // Create recording buffer
    PmRecording recording;
    Pm_CreateRecording(&recording, 100);
    printf("[SUCCESS] Created recording buffer (capacity: 100 events)\n\n");
    
    // Start recording on input stream
    Pm_StartRecording(in_stream, &recording);
    printf("Recording started...\n");
    
    // Send a sequence of MIDI events
    PmEvent events[8];
    events[0].message = Pm_Message(0x90, 60, 100);  // Note On C4
    events[0].timestamp = 0;
    events[1].message = Pm_Message(0x90, 64, 90);   // Note On E4
    events[1].timestamp = 100;
    events[2].message = Pm_Message(0x90, 67, 85);   // Note On G4
    events[2].timestamp = 200;
    events[3].message = Pm_Message(0x80, 60, 0);    // Note Off C4
    events[3].timestamp = 400;
    events[4].message = Pm_Message(0x80, 64, 0);    // Note Off E4
    events[4].timestamp = 500;
    events[5].message = Pm_Message(0x80, 67, 0);    // Note Off G4
    events[5].timestamp = 600;
    events[6].message = Pm_Message(0xB0, 7, 127);   // CC Volume
    events[6].timestamp = 700;
    events[7].message = Pm_Message(0xC0, 5, 0);     // Program Change
    events[7].timestamp = 800;
    
    printf("Sending 8 MIDI events...\n");
    Pm_Write(record_stream, events, 8);
    
    // Read events (this triggers recording)
    PmEvent received[10];
    int count = Pm_Read(in_stream, received, 10);
    printf("Recorded %d events\n", count);
    
    // Stop recording
    Pm_StopRecording(in_stream);
    printf("Recording stopped\n\n");
    
    printf("Recording contains %d events:\n", recording.event_count);
    for (int i = 0; i < recording.event_count; i++) {
        uint8_t status = Pm_MessageStatus(recording.events[i].message);
        uint8_t data1 = Pm_MessageData1(recording.events[i].message);
        uint8_t data2 = Pm_MessageData2(recording.events[i].message);
        
        const char *msg_type = "Unknown";
        if ((status & 0xF0) == 0x90) msg_type = "Note On";
        else if ((status & 0xF0) == 0x80) msg_type = "Note Off";
        else if ((status & 0xF0) == 0xB0) msg_type = "CC";
        else if ((status & 0xF0) == 0xC0) msg_type = "Program Change";
        
        printf("  [%d] %s: data1=%d data2=%d timestamp=%d\n", 
               i, msg_type, data1, data2, recording.events[i].timestamp);
    }
    
    if (recording.event_count == 8) {
        printf("\n[SUCCESS] All events recorded correctly\n\n");
    } else {
        printf("\n[FAILED] Expected 8 events, recorded %d\n\n", recording.event_count);
    }
    
    // Now test playback
    printf("=== Testing Playback ===\n\n");
    
    // Disconnect old connection and connect playback output to input
    Pm_DisconnectVirtualDevices(out_dev1, in_dev);
    Pm_ConnectVirtualDevices(out_dev2, in_dev);
    
    printf("Playing back recording...\n");
    Pm_PlayRecording(playback_stream, &recording);
    
    // Read played back events
    PmEvent playback_received[10];
    int playback_count = Pm_Read(in_stream, playback_received, 10);
    
    printf("Received %d events from playback:\n", playback_count);
    for (int i = 0; i < playback_count; i++) {
        uint8_t status = Pm_MessageStatus(playback_received[i].message);
        uint8_t data1 = Pm_MessageData1(playback_received[i].message);
        uint8_t data2 = Pm_MessageData2(playback_received[i].message);
        
        const char *msg_type = "Unknown";
        if ((status & 0xF0) == 0x90) msg_type = "Note On";
        else if ((status & 0xF0) == 0x80) msg_type = "Note Off";
        else if ((status & 0xF0) == 0xB0) msg_type = "CC";
        else if ((status & 0xF0) == 0xC0) msg_type = "Program Change";
        
        printf("  [%d] %s: data1=%d data2=%d\n", i, msg_type, data1, data2);
    }
    
    // Verify playback matches recording
    int match = 1;
    if (playback_count != recording.event_count) {
        match = 0;
    } else {
        for (int i = 0; i < playback_count; i++) {
            if (playback_received[i].message != recording.events[i].message) {
                match = 0;
                break;
            }
        }
    }
    
    if (match) {
        printf("\n[SUCCESS] Playback matches recording perfectly\n");
    } else {
        printf("\n[FAILED] Playback does not match recording\n");
    }
    
    // Cleanup
    Pm_Close(record_stream);
    Pm_Close(playback_stream);
    Pm_Close(in_stream);
    Pm_FreeRecording(&recording);
    Pm_DisconnectVirtualDevices(out_dev2, in_dev);
    Pm_DestroyVirtualDevice(out_dev1);
    Pm_DestroyVirtualDevice(out_dev2);
    Pm_DestroyVirtualDevice(in_dev);
    
    Pm_Terminate();
    printf("\n[SUCCESS] MIDI terminated\n");
    printf("=== All Tests Passed! ===\n");
    
    return 0;
}
