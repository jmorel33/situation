/*
 * Sample-Accurate MIDI Demo
 * 
 * Demonstrates how to process MIDI events with sample-accurate timing
 * in an audio callback, suitable for Situation's audio engine.
 */

#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

// Sample-accurate MIDI event
typedef struct {
    PmMessage message;
    uint32_t sample_offset;  // Sample position within audio buffer
} SampleAccurateMidiEvent;

// Lock-free queue for passing MIDI to audio thread
typedef struct {
    SampleAccurateMidiEvent events[1024];
    _Atomic uint32_t write_pos;
    _Atomic uint32_t read_pos;
} MidiQueue;

// Audio context
typedef struct {
    PmStream *midi_input;
    MidiQueue midi_queue;
    double sample_rate;
    uint64_t sample_position;
    int active_notes[128];  // Track which notes are playing
} AudioContext;

// Initialize queue
void midi_queue_init(MidiQueue *queue) {
    atomic_store(&queue->write_pos, 0);
    atomic_store(&queue->read_pos, 0);
}

// Push event (non-audio thread)
int midi_queue_push(MidiQueue *queue, SampleAccurateMidiEvent event) {
    uint32_t write_pos = atomic_load(&queue->write_pos);
    uint32_t next_write = (write_pos + 1) & 1023;
    uint32_t read_pos = atomic_load(&queue->read_pos);
    
    if (next_write == read_pos) {
        return 0;  // Queue full
    }
    
    queue->events[write_pos] = event;
    atomic_store(&queue->write_pos, next_write);
    return 1;
}

// Pop event (audio thread)
int midi_queue_pop(MidiQueue *queue, SampleAccurateMidiEvent *event) {
    uint32_t read_pos = atomic_load(&queue->read_pos);
    uint32_t write_pos = atomic_load(&queue->write_pos);
    
    if (read_pos == write_pos) {
        return 0;  // Queue empty
    }
    
    *event = queue->events[read_pos];
    atomic_store(&queue->read_pos, (read_pos + 1) & 1023);
    return 1;
}

// Peek without removing
int midi_queue_peek(MidiQueue *queue, SampleAccurateMidiEvent *event) {
    uint32_t read_pos = atomic_load(&queue->read_pos);
    uint32_t write_pos = atomic_load(&queue->write_pos);
    
    if (read_pos == write_pos) {
        return 0;  // Queue empty
    }
    
    *event = queue->events[read_pos];
    return 1;
}

// Process MIDI message (called at exact sample position)
void process_midi_at_sample(AudioContext *ctx, PmMessage msg, uint32_t sample_offset) {
    uint8_t status = Pm_MessageStatus(msg);
    uint8_t note = Pm_MessageData1(msg);
    uint8_t velocity = Pm_MessageData2(msg);
    
    if ((status & 0xF0) == 0x90 && velocity > 0) {
        // Note On
        ctx->active_notes[note] = velocity;
        printf("  [Sample %llu + %u] Note On: %d (velocity %d)\n", 
               (unsigned long long)ctx->sample_position, sample_offset, note, velocity);
    } else if ((status & 0xF0) == 0x80 || ((status & 0xF0) == 0x90 && velocity == 0)) {
        // Note Off
        ctx->active_notes[note] = 0;
        printf("  [Sample %llu + %u] Note Off: %d\n", 
               (unsigned long long)ctx->sample_position, sample_offset, note);
    }
}

// Simulate audio callback with sample-accurate MIDI
void audio_callback(AudioContext *ctx, int frame_count) {
    printf("\nAudio callback: processing %d samples (position: %llu)\n", 
           frame_count, (unsigned long long)ctx->sample_position);
    
    // Read MIDI events from virtual buffer (real-time safe)
    PmEvent midi_events[32];
    int count = Pm_Read(ctx->midi_input, midi_events, 32);
    
    if (count > 0) {
        printf("  Read %d MIDI events from buffer\n", count);
    }
    
    // Convert to sample-accurate events and add to queue
    for (int i = 0; i < count; i++) {
        SampleAccurateMidiEvent event;
        event.message = midi_events[i].message;
        
        // Calculate sample offset within this buffer
        // For this demo, we'll use timestamp as relative sample offset
        event.sample_offset = midi_events[i].timestamp % frame_count;
        
        midi_queue_push(&ctx->midi_queue, event);
    }
    
    // Process audio buffer with sample-accurate MIDI
    for (int i = 0; i < frame_count; i++) {
        // Check for MIDI events at this exact sample
        SampleAccurateMidiEvent event;
        while (midi_queue_peek(&ctx->midi_queue, &event) && 
               event.sample_offset == i) {
            
            // Process MIDI at exact sample position
            process_midi_at_sample(ctx, event.message, i);
            midi_queue_pop(&ctx->midi_queue, &event);
        }
        
        // Here you would generate audio sample
        // float sample = generate_audio_sample(ctx);
    }
    
    ctx->sample_position += frame_count;
}

int main() {
    printf("=== Sample-Accurate MIDI Demo ===\n\n");
    printf("This demonstrates how to process MIDI with sample-accurate timing\n");
    printf("in an audio callback, suitable for Situation's audio engine.\n\n");
    
    // Initialize MIDI
    Pm_Initialize();
    
    // Create virtual MIDI devices
    PmDeviceID midi_out, midi_in;
    Pm_CreateVirtualDevice("Demo MIDI Out", 0, &midi_out);
    Pm_CreateVirtualDevice("Demo MIDI In", 1, &midi_in);
    Pm_ConnectVirtualDevices(midi_out, midi_in);
    
    // Initialize audio context
    AudioContext ctx = {0};
    ctx.sample_rate = 48000.0;
    ctx.sample_position = 0;
    midi_queue_init(&ctx.midi_queue);
    
    // Open MIDI streams
    PmStream *midi_output;
    Pm_OpenOutput(&midi_output, midi_out, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&ctx.midi_input, midi_in, NULL, 8192, NULL, NULL);
    
    printf("Audio settings:\n");
    printf("  Sample rate: %.0f Hz\n", ctx.sample_rate);
    printf("  Buffer size: 512 samples\n");
    printf("  Buffer duration: %.2f ms\n", (512.0 / ctx.sample_rate) * 1000.0);
    printf("  Sample precision: %.3f ms\n\n", (1.0 / ctx.sample_rate) * 1000.0);
    
    // Send MIDI events with sample offsets
    printf("Sending MIDI events:\n");
    PmEvent events[6];
    
    // Note On at sample 0
    events[0].message = Pm_Message(0x90, 60, 100);  // C4
    events[0].timestamp = 0;
    printf("  Event 0: Note On C4 at sample offset 0\n");
    
    // Note On at sample 100
    events[1].message = Pm_Message(0x90, 64, 90);   // E4
    events[1].timestamp = 100;
    printf("  Event 1: Note On E4 at sample offset 100\n");
    
    // Note On at sample 200
    events[2].message = Pm_Message(0x90, 67, 80);   // G4
    events[2].timestamp = 200;
    printf("  Event 2: Note On G4 at sample offset 200\n");
    
    // Note Off at sample 400
    events[3].message = Pm_Message(0x80, 60, 0);    // C4 off
    events[3].timestamp = 400;
    printf("  Event 3: Note Off C4 at sample offset 400\n");
    
    // Note Off at sample 450
    events[4].message = Pm_Message(0x80, 64, 0);    // E4 off
    events[4].timestamp = 450;
    printf("  Event 4: Note Off E4 at sample offset 450\n");
    
    // Note Off at sample 500
    events[5].message = Pm_Message(0x80, 67, 0);    // G4 off
    events[5].timestamp = 500;
    printf("  Event 5: Note Off G4 at sample offset 500\n");
    
    Pm_Write(midi_output, events, 6);
    
    // Simulate audio callback
    printf("\n=== Simulating Audio Callback ===\n");
    audio_callback(&ctx, 512);
    
    // Check active notes
    printf("\nActive notes after processing:\n");
    int active_count = 0;
    for (int i = 0; i < 128; i++) {
        if (ctx.active_notes[i] > 0) {
            printf("  Note %d: velocity %d\n", i, ctx.active_notes[i]);
            active_count++;
        }
    }
    if (active_count == 0) {
        printf("  (none)\n");
    }
    
    // Cleanup
    Pm_Close(midi_output);
    Pm_Close(ctx.midi_input);
    Pm_DisconnectVirtualDevices(midi_out, midi_in);
    Pm_DestroyVirtualDevice(midi_out);
    Pm_DestroyVirtualDevice(midi_in);
    Pm_Terminate();
    
    printf("\n=== Summary ===\n");
    printf("Sample-accurate MIDI processing:\n");
    printf("  ✓ MIDI events read from virtual buffer (real-time safe)\n");
    printf("  ✓ Events converted to sample offsets\n");
    printf("  ✓ Events processed at exact sample positions\n");
    printf("  ✓ Lock-free queue used for thread communication\n");
    printf("  ✓ Precision: 1 sample (0.02ms @ 48kHz)\n");
    printf("\nThis pattern is suitable for Situation's audio engine!\n");
    printf("=== Demo Complete ===\n");
    
    return 0;
}
