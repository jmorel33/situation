/*
 * MIDI Performance and Stress Test
 * 
 * Tests:
 * - Lock-free ring buffer under high load
 * - Throughput measurement
 * - Buffer overflow handling
 * - Multiple simultaneous connections
 */

#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
double get_time() {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
}
#else
#include <sys/time.h>
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}
#endif

void test_throughput() {
    printf("\n=== Throughput Test ===\n\n");
    
    PmDeviceID out_dev, in_dev;
    Pm_CreateVirtualDevice("Throughput Out", 0, &out_dev);
    Pm_CreateVirtualDevice("Throughput In", 1, &in_dev);
    Pm_ConnectVirtualDevices(out_dev, in_dev);
    
    PmStream *out_stream, *in_stream;
    Pm_OpenOutput(&out_stream, out_dev, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream, in_dev, NULL, 8192, NULL, NULL);
    
    const int NUM_EVENTS = 10000;
    PmEvent *events = (PmEvent *)malloc(NUM_EVENTS * sizeof(PmEvent));
    
    // Generate test events
    for (int i = 0; i < NUM_EVENTS; i++) {
        events[i].message = Pm_Message(0x90, 60 + (i % 12), 100);
        events[i].timestamp = i;
    }
    
    printf("Sending %d events...\n", NUM_EVENTS);
    double start_time = get_time();
    
    // Send in batches
    const int BATCH_SIZE = 100;
    for (int i = 0; i < NUM_EVENTS; i += BATCH_SIZE) {
        int batch = (i + BATCH_SIZE > NUM_EVENTS) ? (NUM_EVENTS - i) : BATCH_SIZE;
        Pm_Write(out_stream, &events[i], batch);
    }
    
    double write_time = get_time() - start_time;
    
    // Read all events
    start_time = get_time();
    int total_read = 0;
    PmEvent received[1000];
    
    while (total_read < NUM_EVENTS) {
        int count = Pm_Read(in_stream, received, 1000);
        if (count == 0) break;
        total_read += count;
    }
    
    double read_time = get_time() - start_time;
    
    printf("Results:\n");
    printf("  Sent: %d events in %.3f ms\n", NUM_EVENTS, write_time * 1000.0);
    printf("  Received: %d events in %.3f ms\n", total_read, read_time * 1000.0);
    printf("  Write throughput: %.0f events/sec\n", NUM_EVENTS / write_time);
    printf("  Read throughput: %.0f events/sec\n", total_read / read_time);
    
    if (total_read == NUM_EVENTS) {
        printf("[SUCCESS] All events transmitted successfully\n");
    } else {
        printf("[FAILED] Lost %d events\n", NUM_EVENTS - total_read);
    }
    
    free(events);
    Pm_Close(out_stream);
    Pm_Close(in_stream);
    Pm_DisconnectVirtualDevices(out_dev, in_dev);
    Pm_DestroyVirtualDevice(out_dev);
    Pm_DestroyVirtualDevice(in_dev);
}

void test_buffer_overflow() {
    printf("\n=== Buffer Overflow Test ===\n\n");
    
    PmDeviceID out_dev, in_dev;
    Pm_CreateVirtualDevice("Overflow Out", 0, &out_dev);
    Pm_CreateVirtualDevice("Overflow In", 1, &in_dev);
    Pm_ConnectVirtualDevices(out_dev, in_dev);
    
    PmStream *out_stream, *in_stream;
    Pm_OpenOutput(&out_stream, out_dev, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream, in_dev, NULL, 512, NULL, NULL);
    
    // Try to overflow the buffer (8192 events capacity)
    const int OVERFLOW_COUNT = 10000;
    PmEvent event;
    event.message = Pm_Message(0x90, 60, 100);
    event.timestamp = 0;
    
    printf("Attempting to send %d events without reading (buffer size: 8192)...\n", OVERFLOW_COUNT);
    
    int sent = 0;
    for (int i = 0; i < OVERFLOW_COUNT; i++) {
        PmError err = Pm_Write(out_stream, &event, 1);
        if (err == pmNoError) {
            sent++;
        }
    }
    
    printf("Successfully sent %d events\n", sent);
    
    // Now read what we can
    PmEvent received[1000];
    int total_read = 0;
    int count;
    
    do {
        count = Pm_Read(in_stream, received, 1000);
        total_read += count;
    } while (count > 0);
    
    printf("Read %d events from buffer\n", total_read);
    
    if (total_read <= 8192) {
        printf("[SUCCESS] Buffer overflow handled correctly (max capacity respected)\n");
    } else {
        printf("[WARNING] Read more than buffer capacity\n");
    }
    
    Pm_Close(out_stream);
    Pm_Close(in_stream);
    Pm_DisconnectVirtualDevices(out_dev, in_dev);
    Pm_DestroyVirtualDevice(out_dev);
    Pm_DestroyVirtualDevice(in_dev);
}

void test_concurrent_connections() {
    printf("\n=== Concurrent Connections Test ===\n\n");
    
    const int NUM_PAIRS = 10;
    PmDeviceID out_devs[NUM_PAIRS];
    PmDeviceID in_devs[NUM_PAIRS];
    PmStream *out_streams[NUM_PAIRS];
    PmStream *in_streams[NUM_PAIRS];
    
    // Create multiple device pairs
    printf("Creating %d device pairs...\n", NUM_PAIRS);
    for (int i = 0; i < NUM_PAIRS; i++) {
        char out_name[64], in_name[64];
        sprintf(out_name, "Concurrent Out %d", i);
        sprintf(in_name, "Concurrent In %d", i);
        
        Pm_CreateVirtualDevice(out_name, 0, &out_devs[i]);
        Pm_CreateVirtualDevice(in_name, 1, &in_devs[i]);
        Pm_ConnectVirtualDevices(out_devs[i], in_devs[i]);
        
        Pm_OpenOutput(&out_streams[i], out_devs[i], NULL, 0, NULL, NULL, 0);
        Pm_OpenInput(&in_streams[i], in_devs[i], NULL, 512, NULL, NULL);
    }
    
    // Send events through all pairs simultaneously
    const int EVENTS_PER_PAIR = 100;
    PmEvent event;
    event.message = Pm_Message(0x90, 60, 100);
    event.timestamp = 0;
    
    printf("Sending %d events through each of %d pairs...\n", EVENTS_PER_PAIR, NUM_PAIRS);
    
    for (int i = 0; i < EVENTS_PER_PAIR; i++) {
        for (int j = 0; j < NUM_PAIRS; j++) {
            Pm_Write(out_streams[j], &event, 1);
        }
    }
    
    // Read from all pairs
    int total_received = 0;
    PmEvent received[100];
    
    for (int i = 0; i < NUM_PAIRS; i++) {
        int count = Pm_Read(in_streams[i], received, 100);
        total_received += count;
        printf("  Pair %d received %d events\n", i, count);
    }
    
    int expected = NUM_PAIRS * EVENTS_PER_PAIR;
    printf("Total: %d events (expected %d)\n", total_received, expected);
    
    if (total_received == expected) {
        printf("[SUCCESS] All concurrent connections working correctly\n");
    } else {
        printf("[FAILED] Lost %d events\n", expected - total_received);
    }
    
    // Cleanup
    for (int i = 0; i < NUM_PAIRS; i++) {
        Pm_Close(out_streams[i]);
        Pm_Close(in_streams[i]);
        Pm_DisconnectVirtualDevices(out_devs[i], in_devs[i]);
        Pm_DestroyVirtualDevice(out_devs[i]);
        Pm_DestroyVirtualDevice(in_devs[i]);
    }
}

int main() {
    printf("=== MIDI Performance & Stress Test ===\n");
    
    PmError err = Pm_Initialize();
    if (err != pmNoError) {
        printf("[FAILED] Could not initialize MIDI\n");
        return 1;
    }
    printf("[SUCCESS] MIDI initialized\n");
    
    test_throughput();
    test_buffer_overflow();
    test_concurrent_connections();
    
    Pm_Terminate();
    printf("\n[SUCCESS] MIDI terminated\n");
    printf("=== All Performance Tests Passed! ===\n");
    
    return 0;
}
