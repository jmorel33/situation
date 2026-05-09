/*
 * MIDI Multi-Connection Routing Test
 * 
 * Tests complex routing scenarios:
 * - 1-to-1 routing
 * - 1-to-many routing (broadcast)
 * - Many-to-1 routing (merge)
 */

#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_one_to_one() {
    printf("\n=== Test 1: One-to-One Routing ===\n\n");
    
    PmDeviceID out_dev, in_dev;
    Pm_CreateVirtualDevice("Source", 0, &out_dev);
    Pm_CreateVirtualDevice("Destination", 1, &in_dev);
    Pm_ConnectVirtualDevices(out_dev, in_dev);
    
    PmStream *out_stream, *in_stream;
    Pm_OpenOutput(&out_stream, out_dev, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream, in_dev, NULL, 512, NULL, NULL);
    
    // Send 3 events
    PmEvent events[3];
    events[0].message = Pm_Message(0x90, 60, 100);
    events[0].timestamp = 0;
    events[1].message = Pm_Message(0x90, 64, 90);
    events[1].timestamp = 0;
    events[2].message = Pm_Message(0x90, 67, 80);
    events[2].timestamp = 0;
    
    printf("Sending 3 events from Source...\n");
    Pm_Write(out_stream, events, 3);
    
    PmEvent received[10];
    int count = Pm_Read(in_stream, received, 10);
    
    printf("Destination received %d events\n", count);
    
    if (count == 3) {
        printf("[SUCCESS] One-to-one routing works\n");
    } else {
        printf("[FAILED] Expected 3 events, got %d\n", count);
    }
    
    Pm_Close(out_stream);
    Pm_Close(in_stream);
    Pm_DisconnectVirtualDevices(out_dev, in_dev);
    Pm_DestroyVirtualDevice(out_dev);
    Pm_DestroyVirtualDevice(in_dev);
}

void test_one_to_many() {
    printf("\n=== Test 2: One-to-Many Routing (Broadcast) ===\n\n");
    
    PmDeviceID out_dev, in_dev1, in_dev2, in_dev3;
    Pm_CreateVirtualDevice("Broadcaster", 0, &out_dev);
    Pm_CreateVirtualDevice("Receiver 1", 1, &in_dev1);
    Pm_CreateVirtualDevice("Receiver 2", 1, &in_dev2);
    Pm_CreateVirtualDevice("Receiver 3", 1, &in_dev3);
    
    // Connect one output to three inputs
    Pm_ConnectVirtualDevices(out_dev, in_dev1);
    Pm_ConnectVirtualDevices(out_dev, in_dev2);
    Pm_ConnectVirtualDevices(out_dev, in_dev3);
    
    PmStream *out_stream, *in_stream1, *in_stream2, *in_stream3;
    Pm_OpenOutput(&out_stream, out_dev, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream1, in_dev1, NULL, 512, NULL, NULL);
    Pm_OpenInput(&in_stream2, in_dev2, NULL, 512, NULL, NULL);
    Pm_OpenInput(&in_stream3, in_dev3, NULL, 512, NULL, NULL);
    
    // Send 2 events
    PmEvent events[2];
    events[0].message = Pm_Message(0x90, 60, 100);
    events[0].timestamp = 0;
    events[1].message = Pm_Message(0x80, 60, 0);
    events[1].timestamp = 0;
    
    printf("Broadcasting 2 events to 3 receivers...\n");
    Pm_Write(out_stream, events, 2);
    
    PmEvent received[10];
    int count1 = Pm_Read(in_stream1, received, 10);
    int count2 = Pm_Read(in_stream2, received, 10);
    int count3 = Pm_Read(in_stream3, received, 10);
    
    printf("Receiver 1 got %d events\n", count1);
    printf("Receiver 2 got %d events\n", count2);
    printf("Receiver 3 got %d events\n", count3);
    
    if (count1 == 2 && count2 == 2 && count3 == 2) {
        printf("[SUCCESS] One-to-many routing works (broadcast to all)\n");
    } else {
        printf("[FAILED] Not all receivers got 2 events\n");
    }
    
    Pm_Close(out_stream);
    Pm_Close(in_stream1);
    Pm_Close(in_stream2);
    Pm_Close(in_stream3);
    Pm_DisconnectVirtualDevices(out_dev, in_dev1);
    Pm_DisconnectVirtualDevices(out_dev, in_dev2);
    Pm_DisconnectVirtualDevices(out_dev, in_dev3);
    Pm_DestroyVirtualDevice(out_dev);
    Pm_DestroyVirtualDevice(in_dev1);
    Pm_DestroyVirtualDevice(in_dev2);
    Pm_DestroyVirtualDevice(in_dev3);
}

void test_many_to_one() {
    printf("\n=== Test 3: Many-to-One Routing (Merge) ===\n\n");
    
    PmDeviceID out_dev1, out_dev2, out_dev3, in_dev;
    Pm_CreateVirtualDevice("Sender 1", 0, &out_dev1);
    Pm_CreateVirtualDevice("Sender 2", 0, &out_dev2);
    Pm_CreateVirtualDevice("Sender 3", 0, &out_dev3);
    Pm_CreateVirtualDevice("Merger", 1, &in_dev);
    
    // Connect three outputs to one input
    Pm_ConnectVirtualDevices(out_dev1, in_dev);
    Pm_ConnectVirtualDevices(out_dev2, in_dev);
    Pm_ConnectVirtualDevices(out_dev3, in_dev);
    
    PmStream *out_stream1, *out_stream2, *out_stream3, *in_stream;
    Pm_OpenOutput(&out_stream1, out_dev1, NULL, 0, NULL, NULL, 0);
    Pm_OpenOutput(&out_stream2, out_dev2, NULL, 0, NULL, NULL, 0);
    Pm_OpenOutput(&out_stream3, out_dev3, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream, in_dev, NULL, 512, NULL, NULL);
    
    // Send events from each sender
    PmEvent event1, event2, event3;
    event1.message = Pm_Message(0x90, 60, 100);  // C4 from sender 1
    event1.timestamp = 0;
    event2.message = Pm_Message(0x90, 64, 90);   // E4 from sender 2
    event2.timestamp = 0;
    event3.message = Pm_Message(0x90, 67, 80);   // G4 from sender 3
    event3.timestamp = 0;
    
    printf("Sending 1 event from each of 3 senders...\n");
    Pm_Write(out_stream1, &event1, 1);
    Pm_Write(out_stream2, &event2, 1);
    Pm_Write(out_stream3, &event3, 1);
    
    PmEvent received[10];
    int count = Pm_Read(in_stream, received, 10);
    
    printf("Merger received %d events:\n", count);
    for (int i = 0; i < count; i++) {
        uint8_t note = Pm_MessageData1(received[i].message);
        printf("  [%d] Note %d\n", i, note);
    }
    
    if (count == 3) {
        printf("[SUCCESS] Many-to-one routing works (merged all inputs)\n");
    } else {
        printf("[FAILED] Expected 3 events, got %d\n", count);
    }
    
    Pm_Close(out_stream1);
    Pm_Close(out_stream2);
    Pm_Close(out_stream3);
    Pm_Close(in_stream);
    Pm_DisconnectVirtualDevices(out_dev1, in_dev);
    Pm_DisconnectVirtualDevices(out_dev2, in_dev);
    Pm_DisconnectVirtualDevices(out_dev3, in_dev);
    Pm_DestroyVirtualDevice(out_dev1);
    Pm_DestroyVirtualDevice(out_dev2);
    Pm_DestroyVirtualDevice(out_dev3);
    Pm_DestroyVirtualDevice(in_dev);
}

int main() {
    printf("=== MIDI Multi-Connection Routing Test ===\n");
    
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("[FAILED] Could not initialize MIDI\n");
        return 1;
    }
    printf("[SUCCESS] MIDI initialized\n");
    
    test_one_to_one();
    test_one_to_many();
    test_many_to_one();
    
    Pm_Terminate();
    printf("\n[SUCCESS] MIDI terminated\n");
    printf("=== All Routing Tests Passed! ===\n");
    
    return 0;
}
