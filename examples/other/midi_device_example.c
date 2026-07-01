/*
 * MIDI Device Example
 * 
 * Demonstrates how to create MIDI-enabled devices using the
 * SIT_MidiDevice interface. Shows:
 * - Creating a simple synthesizer
 * - Creating a simple sequencer
 * - Connecting them via MIDI
 * - Processing MIDI with callbacks
 */

#define MIDI_IMPLEMENTATION
#include "../sit/aud/midi.h"

#define MIDI_DEVICE_IMPLEMENTATION
#include "../sit/aud/midi_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ================================================================================================
// SIMPLE SYNTHESIZER
// ================================================================================================

typedef struct {
    // Active notes
    struct {
        int active;
        uint8_t note;
        uint8_t velocity;
        float phase;
        float frequency;
    } voices[16];
    
    int voice_count;
    double sample_rate;
} SimpleSynth;

// Create synth
SimpleSynth* SimpleSynth_Create(double sample_rate) {
    SimpleSynth *synth = (SimpleSynth*)calloc(1, sizeof(SimpleSynth));
    synth->sample_rate = sample_rate;
    return synth;
}

// Destroy synth
void SimpleSynth_Destroy(SimpleSynth *synth) {
    free(synth);
}

// MIDI callbacks for synth
void SimpleSynth_OnNoteOn(void *device_ptr, uint8_t note, uint8_t velocity, uint32_t sample_offset) {
    SimpleSynth *synth = (SimpleSynth*)device_ptr;
    
    printf("  [Sample %u] Synth: Note On %d (velocity %d)\n", sample_offset, note, velocity);
    
    // Find free voice
    for (int i = 0; i < 16; i++) {
        if (!synth->voices[i].active) {
            synth->voices[i].active = 1;
            synth->voices[i].note = note;
            synth->voices[i].velocity = velocity;
            synth->voices[i].phase = 0.0f;
            synth->voices[i].frequency = 440.0f * powf(2.0f, (note - 69) / 12.0f);
            synth->voice_count++;
            break;
        }
    }
}

void SimpleSynth_OnNoteOff(void *device_ptr, uint8_t note, uint8_t velocity, uint32_t sample_offset) {
    SimpleSynth *synth = (SimpleSynth*)device_ptr;
    (void)velocity;
    
    printf("  [Sample %u] Synth: Note Off %d\n", sample_offset, note);
    
    // Find and release voice
    for (int i = 0; i < 16; i++) {
        if (synth->voices[i].active && synth->voices[i].note == note) {
            synth->voices[i].active = 0;
            synth->voice_count--;
            break;
        }
    }
}

void SimpleSynth_OnControlChange(void *device_ptr, uint8_t controller, uint8_t value, uint32_t sample_offset) {
    printf("  [Sample %u] Synth: CC %d = %d\n", sample_offset, controller, value);
}

// ================================================================================================
// SIMPLE SEQUENCER
// ================================================================================================

typedef struct {
    int step;
    int steps_per_beat;
    uint8_t sequence[16];  // Note sequence
    int sequence_length;
} SimpleSequencer;

// Create sequencer
SimpleSequencer* SimpleSequencer_Create() {
    SimpleSequencer *seq = (SimpleSequencer*)calloc(1, sizeof(SimpleSequencer));
    seq->steps_per_beat = 4;
    seq->sequence_length = 8;
    
    // Simple C major scale
    seq->sequence[0] = 60;  // C
    seq->sequence[1] = 62;  // D
    seq->sequence[2] = 64;  // E
    seq->sequence[3] = 65;  // F
    seq->sequence[4] = 67;  // G
    seq->sequence[5] = 69;  // A
    seq->sequence[6] = 71;  // B
    seq->sequence[7] = 72;  // C
    
    return seq;
}

// Destroy sequencer
void SimpleSequencer_Destroy(SimpleSequencer *seq) {
    free(seq);
}

// Advance sequencer and generate MIDI
void SimpleSequencer_Step(SimpleSequencer *seq, SIT_MidiDevice *device) {
    // Send Note Off for previous note
    if (seq->step > 0) {
        uint8_t prev_note = seq->sequence[(seq->step - 1) % seq->sequence_length];
        SIT_MidiDevice_SendNoteOff(device, prev_note, 0, 0);
    }
    
    // Send Note On for current note
    uint8_t note = seq->sequence[seq->step % seq->sequence_length];
    SIT_MidiDevice_SendNoteOn(device, note, 100, 0);
    
    printf("Sequencer: Step %d, Note %d\n", seq->step, note);
    
    seq->step++;
}

// ================================================================================================
// MAIN
// ================================================================================================

int main() {
    printf("=== MIDI Device Example ===\n\n");
    printf("This demonstrates the SIT_MidiDevice interface:\n");
    printf("- Creating MIDI-enabled devices\n");
    printf("- Setting up callbacks\n");
    printf("- Connecting devices via MIDI\n");
    printf("- Processing MIDI events\n\n");
    
    // Initialize MIDI system
    Pm_Initialize();
    printf("[SUCCESS] MIDI initialized\n\n");
    
    // Create synthesizer
    SimpleSynth *synth = SimpleSynth_Create(48000.0);
    SIT_MidiDevice *synth_device = SIT_MidiDevice_Create(
        "Simple Synth",
        SIT_MIDI_DEVICE_SYNTH,
        SIT_MIDI_CAP_INPUT,
        synth
    );
    
    // Set synth callbacks
    SIT_MidiCallbacks synth_callbacks = {0};
    synth_callbacks.on_note_on = SimpleSynth_OnNoteOn;
    synth_callbacks.on_note_off = SimpleSynth_OnNoteOff;
    synth_callbacks.on_control_change = SimpleSynth_OnControlChange;
    SIT_MidiDevice_SetCallbacks(synth_device, &synth_callbacks);
    SIT_MidiDevice_SetChannel(synth_device, 0);  // Listen on channel 0
    
    printf("[SUCCESS] Created synthesizer device\n");
    printf("  Name: %s\n", synth_device->name);
    printf("  Type: Synth\n");
    printf("  Capabilities: Input\n");
    printf("  MIDI Channel: 0\n\n");
    
    // Create sequencer
    SimpleSequencer *seq = SimpleSequencer_Create();
    SIT_MidiDevice *seq_device = SIT_MidiDevice_Create(
        "Simple Sequencer",
        SIT_MIDI_DEVICE_SEQUENCER,
        SIT_MIDI_CAP_OUTPUT,
        seq
    );
    
    printf("[SUCCESS] Created sequencer device\n");
    printf("  Name: %s\n", seq_device->name);
    printf("  Type: Sequencer\n");
    printf("  Capabilities: Output\n");
    printf("  Sequence: C D E F G A B C\n\n");
    
    // Connect sequencer to synth
    if (SIT_MidiDevice_Connect(seq_device, synth_device)) {
        printf("[SUCCESS] Connected Sequencer -> Synth\n\n");
    } else {
        printf("[FAILED] Could not connect devices\n");
        return 1;
    }
    
    // Simulate sequencer steps
    printf("=== Running Sequencer ===\n\n");
    
    for (int step = 0; step < 8; step++) {
        printf("--- Step %d ---\n", step);
        
        // Sequencer generates MIDI
        SimpleSequencer_Step(seq, seq_device);
        
        // Simulate audio callback (synth processes MIDI)
        printf("Audio callback:\n");
        SIT_MidiDevice_ProcessAudio(synth_device, 512);
        
        printf("Active voices: %d\n\n", synth->voice_count);
    }
    
    // Send final Note Off
    uint8_t last_note = seq->sequence[7];
    SIT_MidiDevice_SendNoteOff(seq_device, last_note, 0, 0);
    SIT_MidiDevice_ProcessAudio(synth_device, 512);
    
    printf("=== Sequencer Complete ===\n\n");
    printf("Final active voices: %d\n\n", synth->voice_count);
    
    // Test All Notes Off
    printf("Testing All Notes Off...\n");
    SIT_MidiDevice_AllNotesOff(synth_device);
    SIT_MidiDevice_ProcessAudio(synth_device, 512);
    printf("Active voices after panic: %d\n\n", synth->voice_count);
    
    // Cleanup
    SIT_MidiDevice_Disconnect(seq_device, synth_device);
    SIT_MidiDevice_Destroy(synth_device);
    SIT_MidiDevice_Destroy(seq_device);
    SimpleSynth_Destroy(synth);
    SimpleSequencer_Destroy(seq);
    
    Pm_Terminate();
    
    printf("=== Summary ===\n");
    printf("SIT_MidiDevice provides:\n");
    printf("  ✓ Standard interface for MIDI-enabled devices\n");
    printf("  ✓ Automatic MIDI routing setup\n");
    printf("  ✓ Callback-based event handling\n");
    printf("  ✓ Sample-accurate timing support\n");
    printf("  ✓ Channel filtering\n");
    printf("  ✓ Easy device connection\n");
    printf("\nPerfect for Situation's component architecture!\n");
    printf("=== Example Complete ===\n");
    
    return 0;
}
