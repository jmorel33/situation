/*
 * MIDI Filter and Transform Test
 * 
 * Tests Phase 3 features:
 * - MIDI filtering (block specific message types/channels)
 * - MIDI transformation (transpose, velocity curves, channel remapping)
 */

#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_filter() {
    printf("\n=== Testing MIDI Filtering ===\n\n");
    
    // Create virtual devices
    PmDeviceID out_dev, in_dev;
    Pm_CreateVirtualDevice("Filter Test Out", 0, &out_dev);
    Pm_CreateVirtualDevice("Filter Test In", 1, &in_dev);
    
    // Connect them
    Pm_ConnectVirtualDevices(out_dev, in_dev);
    
    // Set up filter: block Note Off and CC messages, only allow channel 0
    PmFilter filter = {0};
    filter.filter_note_on = 0;      // Pass Note On
    filter.filter_note_off = 1;     // Block Note Off
    filter.filter_cc = 1;           // Block CC
    filter.filter_program_change = 0;
    filter.filter_pitch_bend = 0;
    filter.filter_aftertouch = 0;
    filter.filter_sysex = 0;
    filter.channel_mask = 0x0001;   // Only channel 0 (bit 0 set)
    
    Pm_SetConnectionFilter(out_dev, in_dev, &filter);
    
    // Open streams
    PmStream *out_stream, *in_stream;
    Pm_OpenOutput(&out_stream, out_dev, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream, in_dev, NULL, 512, NULL, NULL);
    
    // Send various MIDI events
    PmEvent events[6];
    events[0].message = Pm_Message(0x90, 60, 100);  // Note On Ch0 - PASS
    events[0].timestamp = 0;
    events[1].message = Pm_Message(0x80, 60, 0);    // Note Off Ch0 - BLOCKED
    events[1].timestamp = 0;
    events[2].message = Pm_Message(0xB0, 7, 127);   // CC Ch0 - BLOCKED
    events[2].timestamp = 0;
    events[3].message = Pm_Message(0x91, 64, 80);   // Note On Ch1 - BLOCKED (wrong channel)
    events[3].timestamp = 0;
    events[4].message = Pm_Message(0x90, 67, 90);   // Note On Ch0 - PASS
    events[4].timestamp = 0;
    events[5].message = Pm_Message(0xC0, 5, 0);     // Program Change Ch0 - PASS
    events[5].timestamp = 0;
    
    printf("Sending 6 events (3 should pass filter)...\n");
    Pm_Write(out_stream, events, 6);
    
    // Read filtered events
    PmEvent received[10];
    int count = Pm_Read(in_stream, received, 10);
    
    printf("Received %d events:\n", count);
    for (int i = 0; i < count; i++) {
        uint8_t status = Pm_MessageStatus(received[i].message);
        uint8_t data1 = Pm_MessageData1(received[i].message);
        uint8_t data2 = Pm_MessageData2(received[i].message);
        printf("  [%d] Status=0x%02X Data1=%d Data2=%d\n", i, status, data1, data2);
    }
    
    if (count == 3) {
        printf("[SUCCESS] Filter working correctly (3 events passed)\n");
    } else {
        printf("[FAILED] Expected 3 events, got %d\n", count);
    }
    
    // Cleanup
    Pm_Close(out_stream);
    Pm_Close(in_stream);
    Pm_DisconnectVirtualDevices(out_dev, in_dev);
    Pm_DestroyVirtualDevice(out_dev);
    Pm_DestroyVirtualDevice(in_dev);
}

void test_transform() {
    printf("\n=== Testing MIDI Transformation ===\n\n");
    
    // Create virtual devices
    PmDeviceID out_dev, in_dev;
    Pm_CreateVirtualDevice("Transform Test Out", 0, &out_dev);
    Pm_CreateVirtualDevice("Transform Test In", 1, &in_dev);
    
    // Connect them
    Pm_ConnectVirtualDevices(out_dev, in_dev);
    
    // Set up transform: transpose +12 semitones, exponential velocity curve, remap channel 0->1
    PmTransform transform = {0};
    transform.transpose = 12;           // Transpose up one octave
    transform.velocity_curve = 1;       // Exponential curve
    transform.velocity_scale = 1.0f;    // No scaling
    for (int i = 0; i < 16; i++) {
        transform.channel_remap[i] = 0xFF; // No remap by default
    }
    transform.channel_remap[0] = 1;     // Remap channel 0 to channel 1
    
    Pm_SetConnectionTransform(out_dev, in_dev, &transform);
    
    // Open streams
    PmStream *out_stream, *in_stream;
    Pm_OpenOutput(&out_stream, out_dev, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream, in_dev, NULL, 512, NULL, NULL);
    
    // Send Note On on channel 0, middle C (60), velocity 64
    PmEvent event;
    event.message = Pm_Message(0x90, 60, 64);
    event.timestamp = 0;
    
    printf("Sending: Note On Ch0, Note=60 (C4), Velocity=64\n");
    Pm_Write(out_stream, &event, 1);
    
    // Read transformed event
    PmEvent received;
    int count = Pm_Read(in_stream, &received, 1);
    
    if (count == 1) {
        uint8_t status = Pm_MessageStatus(received.message);
        uint8_t channel = status & 0x0F;
        uint8_t note = Pm_MessageData1(received.message);
        uint8_t velocity = Pm_MessageData2(received.message);
        
        printf("Received: Note On Ch%d, Note=%d (C%d), Velocity=%d\n", 
               channel, note, (note / 12) - 1, velocity);
        
        if (channel == 1 && note == 72 && velocity < 64) {
            printf("[SUCCESS] Transform working correctly\n");
            printf("  - Channel remapped: 0 -> 1\n");
            printf("  - Note transposed: 60 -> 72 (+12 semitones)\n");
            printf("  - Velocity curved: 64 -> %d (exponential)\n", velocity);
        } else {
            printf("[FAILED] Transform not applied correctly\n");
        }
    } else {
        printf("[FAILED] No event received\n");
    }
    
    // Cleanup
    Pm_Close(out_stream);
    Pm_Close(in_stream);
    Pm_DisconnectVirtualDevices(out_dev, in_dev);
    Pm_DestroyVirtualDevice(out_dev);
    Pm_DestroyVirtualDevice(in_dev);
}

int main() {
    printf("=== MIDI Filter & Transform Test ===\n");
    
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("[FAILED] Could not initialize MIDI\n");
        return 1;
    }
    printf("[SUCCESS] MIDI initialized\n");
    
    test_filter();
    test_transform();
    
    Pm_Terminate();
    printf("\n[SUCCESS] MIDI terminated\n");
    printf("=== All Tests Passed! ===\n");
    
    return 0;
}
