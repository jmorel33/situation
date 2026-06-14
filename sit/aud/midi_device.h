/*
 * midi_device.h - MIDI Device Interface for Situation
 * 
 * Provides a standard interface for any device that processes MIDI:
 * - Synthesizers (Polysonix)
 * - Sequencers
 * - Arpeggiators
 * - Effects processors
 * - MIDI controllers
 * 
 * Each device gets its own MIDI input/output and processes events
 * with sample-accurate timing in the audio callback.
 */

#ifndef MIDI_DEVICE_H
#define MIDI_DEVICE_H

#include "midi.h"
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct SIT_MidiDevice SIT_MidiDevice;
typedef struct SIT_MidiProcessor SIT_MidiProcessor;

// MIDI event with sample-accurate timing
typedef struct {
    PmMessage message;
    uint32_t sample_offset;  // Sample position within audio buffer
} SIT_MidiEvent;

// MIDI event buffer for audio callback
typedef struct {
    SIT_MidiEvent events[256];
    int event_count;
} SIT_MidiEventBuffer;

// MIDI device capabilities
typedef enum {
    SIT_MIDI_CAP_INPUT      = 1 << 0,  // Can receive MIDI
    SIT_MIDI_CAP_OUTPUT     = 1 << 1,  // Can send MIDI
    SIT_MIDI_CAP_THRU       = 1 << 2,  // Can pass MIDI through
    SIT_MIDI_CAP_FILTER     = 1 << 3,  // Can filter MIDI
    SIT_MIDI_CAP_TRANSFORM  = 1 << 4,  // Can transform MIDI
} SIT_MidiCapabilities;

// MIDI device type
typedef enum {
    SIT_MIDI_DEVICE_SYNTH,       // Synthesizer (receives MIDI, generates audio)
    SIT_MIDI_DEVICE_SEQUENCER,   // Sequencer (generates MIDI)
    SIT_MIDI_DEVICE_ARPEGGIATOR, // Arpeggiator (receives and generates MIDI)
    SIT_MIDI_DEVICE_EFFECT,      // Effect (receives MIDI, modifies audio)
    SIT_MIDI_DEVICE_CONTROLLER,  // Controller (generates MIDI from input)
    SIT_MIDI_DEVICE_CUSTOM       // Custom device
} SIT_MidiDeviceType;

// MIDI processor callbacks (implement these for your device)
typedef struct {
    // Called when MIDI event arrives (sample-accurate)
    void (*on_note_on)(void *device, uint8_t note, uint8_t velocity, uint32_t sample_offset);
    void (*on_note_off)(void *device, uint8_t note, uint8_t velocity, uint32_t sample_offset);
    void (*on_control_change)(void *device, uint8_t controller, uint8_t value, uint32_t sample_offset);
    void (*on_program_change)(void *device, uint8_t program, uint32_t sample_offset);
    void (*on_pitch_bend)(void *device, int16_t value, uint32_t sample_offset);
    void (*on_aftertouch)(void *device, uint8_t note, uint8_t pressure, uint32_t sample_offset);
    void (*on_channel_pressure)(void *device, uint8_t pressure, uint32_t sample_offset);
    void (*on_sysex)(void *device, const uint8_t *data, int length, uint32_t sample_offset);
    
    // Called for any MIDI message (if you want raw access)
    void (*on_midi_message)(void *device, PmMessage message, uint32_t sample_offset);
} SIT_MidiCallbacks;

// ================================================================================================
// DEVICE IDENTITY (Universal Device Inquiry)
// ================================================================================================

/**
 * @brief Device identity information for Universal Device Inquiry response.
 * 
 * Standard MIDI Device Inquiry format:
 * Request:  F0 7E 7F 06 01 F7
 * Response: F0 7E <device_id> 06 02 <manufacturer_id> <family> <model> <version> F7
 * 
 * Manufacturer IDs:
 * - 1-byte: 0x01-0x7D (legacy, avoid)
 * - 3-byte: 0x00 <msb> <lsb> (modern, recommended)
 * 
 * Example: Situation Audio
 * - Manufacturer: 0x00 0x53 0x49 ("SI" for Situation)
 * - Family: 0x00 0x01 (Audio FX)
 * - Model: Device-specific
 * - Version: 0x01 0x00 0x00 0x00 (v1.0.0.0)
 */
typedef struct {
    uint8_t manufacturer_id[3];  // 3-byte manufacturer ID (0x00 <msb> <lsb>)
    uint8_t family[2];           // Device family
    uint8_t model[2];            // Device model
    uint8_t version[4];          // Software version
    char device_name[32];        // Human-readable name (not sent in SysEx)
} SIT_MidiDeviceIdentity;

// Forward declaration for MIDI Learn
typedef struct SIT_MidiLearnState SIT_MidiLearnState;

// MIDI device structure
struct SIT_MidiDevice {
    // Device info
    char name[64];
    SIT_MidiDeviceType type;
    uint32_t capabilities;
    
    // Device identity (for Universal Device Inquiry)
    SIT_MidiDeviceIdentity identity;
    
    // MIDI streams
    PmStream *midi_input;
    PmStream *midi_output;
    PmDeviceID input_device_id;
    PmDeviceID output_device_id;
    
    // MIDI processor
    SIT_MidiProcessor *processor;
    
    // User device pointer (your synth, sequencer, etc.)
    void *device_ptr;
    
    // Callbacks
    SIT_MidiCallbacks callbacks;
    
    // MIDI channel (0-15, or -1 for omni)
    int midi_channel;
    
    // MIDI Learn state (NULL if not enabled)
    SIT_MidiLearnState *learn_state;
    
    // State
    int active;
    double sample_rate;
};

// MIDI processor (handles sample-accurate event scheduling)
struct SIT_MidiProcessor {
    // Lock-free event queue
    struct {
        SIT_MidiEvent events[1024];
        _Atomic uint32_t write_pos;
        _Atomic uint32_t read_pos;
    } event_queue;
    
    // Current audio position
    uint64_t sample_position;
    
    // Timing
    double sample_rate;
    PmTimestamp (*time_proc)(void *time_info);
    void *time_info;
};

// ================================================================================================
// API FUNCTIONS
// ================================================================================================

/* Create a MIDI device */
SIT_MidiDevice* SIT_MidiDevice_Create(
    const char *name,
    SIT_MidiDeviceType type,
    uint32_t capabilities,
    void *device_ptr
);

/* Destroy a MIDI device */
void SIT_MidiDevice_Destroy(SIT_MidiDevice *device);

/* Set MIDI callbacks */
void SIT_MidiDevice_SetCallbacks(SIT_MidiDevice *device, const SIT_MidiCallbacks *callbacks);

/* Set MIDI channel (0-15, or -1 for omni) */
void SIT_MidiDevice_SetChannel(SIT_MidiDevice *device, int channel);

/* Connect this device's output to another device's input */
int SIT_MidiDevice_Connect(SIT_MidiDevice *source, SIT_MidiDevice *destination);

/* Disconnect devices */
int SIT_MidiDevice_Disconnect(SIT_MidiDevice *source, SIT_MidiDevice *destination);

/* Set device identity (for Universal Device Inquiry response) */
void SIT_MidiDevice_SetIdentity(SIT_MidiDevice *device, const SIT_MidiDeviceIdentity *identity);

/* Get device identity */
const SIT_MidiDeviceIdentity* SIT_MidiDevice_GetIdentity(const SIT_MidiDevice *device);

/* Send Identity Reply (in response to Identity Request) */
void SIT_MidiDevice_SendIdentityReply(SIT_MidiDevice *device, uint8_t device_id);

/* Process incoming SysEx (checks for Identity Request and responds automatically) */
void SIT_MidiDevice_ProcessSysEx(SIT_MidiDevice *device, const uint8_t *data, int32_t length);

/* Process MIDI in audio callback (call this every audio buffer) */
void SIT_MidiDevice_ProcessAudio(SIT_MidiDevice *device, int frame_count);

/* Send MIDI event (for devices that generate MIDI) */
void SIT_MidiDevice_SendEvent(SIT_MidiDevice *device, PmMessage message, uint32_t sample_offset);

/* Send Note On */
void SIT_MidiDevice_SendNoteOn(SIT_MidiDevice *device, uint8_t note, uint8_t velocity, uint32_t sample_offset);

/* Send Note Off */
void SIT_MidiDevice_SendNoteOff(SIT_MidiDevice *device, uint8_t note, uint8_t velocity, uint32_t sample_offset);

/* Send Control Change */
void SIT_MidiDevice_SendCC(SIT_MidiDevice *device, uint8_t controller, uint8_t value, uint32_t sample_offset);

/* Send Program Change */
void SIT_MidiDevice_SendProgramChange(SIT_MidiDevice *device, uint8_t program, uint32_t sample_offset);

/* All Notes Off (panic) */
void SIT_MidiDevice_AllNotesOff(SIT_MidiDevice *device);

// ================================================================================================
// IMPLEMENTATION
// ================================================================================================

#ifdef MIDI_DEVICE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Create MIDI processor */
static SIT_MidiProcessor* SIT_MidiProcessor_Create(double sample_rate) {
    SIT_MidiProcessor *proc = (SIT_MidiProcessor*)calloc(1, sizeof(SIT_MidiProcessor));
    if (!proc) return NULL;
    
    proc->sample_rate = sample_rate;
    proc->sample_position = 0;
    atomic_store(&proc->event_queue.write_pos, 0);
    atomic_store(&proc->event_queue.read_pos, 0);
    
    return proc;
}

/* Destroy MIDI processor */
static void SIT_MidiProcessor_Destroy(SIT_MidiProcessor *proc) {
    if (proc) {
        free(proc);
    }
}

/* Push event to processor queue */
static int SIT_MidiProcessor_PushEvent(SIT_MidiProcessor *proc, SIT_MidiEvent event) {
    uint32_t write_pos = atomic_load(&proc->event_queue.write_pos);
    uint32_t next_write = (write_pos + 1) & 1023;
    uint32_t read_pos = atomic_load(&proc->event_queue.read_pos);
    
    if (next_write == read_pos) {
        return 0;  // Queue full
    }
    
    proc->event_queue.events[write_pos] = event;
    atomic_store(&proc->event_queue.write_pos, next_write);
    return 1;
}

/* Pop event from processor queue */
static int SIT_MidiProcessor_PopEvent(SIT_MidiProcessor *proc, SIT_MidiEvent *event) {
    uint32_t read_pos = atomic_load(&proc->event_queue.read_pos);
    uint32_t write_pos = atomic_load(&proc->event_queue.write_pos);
    
    if (read_pos == write_pos) {
        return 0;  // Queue empty
    }
    
    *event = proc->event_queue.events[read_pos];
    atomic_store(&proc->event_queue.read_pos, (read_pos + 1) & 1023);
    return 1;
}

/* Peek event without removing */
static int SIT_MidiProcessor_PeekEvent(SIT_MidiProcessor *proc, SIT_MidiEvent *event) {
    uint32_t read_pos = atomic_load(&proc->event_queue.read_pos);
    uint32_t write_pos = atomic_load(&proc->event_queue.write_pos);
    
    if (read_pos == write_pos) {
        return 0;  // Queue empty
    }
    
    *event = proc->event_queue.events[read_pos];
    return 1;
}

/* Create MIDI device */
SIT_MidiDevice* SIT_MidiDevice_Create(
    const char *name,
    SIT_MidiDeviceType type,
    uint32_t capabilities,
    void *device_ptr
) {
    SIT_MidiDevice *device = (SIT_MidiDevice*)calloc(1, sizeof(SIT_MidiDevice));
    if (!device) return NULL;
    
    // Set device info
    strncpy(device->name, name, sizeof(device->name) - 1);
    device->type = type;
    device->capabilities = capabilities;
    device->device_ptr = device_ptr;
    device->midi_channel = -1;  // Omni by default
    device->active = 1;
    {
        int sr = SituationGetAudioPlaybackSampleRate();
        device->sample_rate = (sr > 0) ? (double)sr : 48000.0;
    }
    
    // Create MIDI processor
    device->processor = SIT_MidiProcessor_Create(device->sample_rate);
    if (!device->processor) {
        free(device);
        return NULL;
    }
    
    // Create virtual MIDI devices if needed
    if (capabilities & SIT_MIDI_CAP_INPUT) {
        char input_name[128];
        snprintf(input_name, sizeof(input_name), "%s MIDI In", name);
        Pm_CreateVirtualDevice(input_name, 1, &device->input_device_id);
        Pm_OpenInput(&device->midi_input, device->input_device_id, NULL, 8192, NULL, NULL);
    }
    
    if (capabilities & SIT_MIDI_CAP_OUTPUT) {
        char output_name[128];
        snprintf(output_name, sizeof(output_name), "%s MIDI Out", name);
        Pm_CreateVirtualDevice(output_name, 0, &device->output_device_id);
        Pm_OpenOutput(&device->midi_output, device->output_device_id, NULL, 0, NULL, NULL, 0);
    }
    
    return device;
}

/* Destroy MIDI device */
void SIT_MidiDevice_Destroy(SIT_MidiDevice *device) {
    if (!device) return;
    
    // Close MIDI streams
    if (device->midi_input) {
        Pm_Close(device->midi_input);
        Pm_DestroyVirtualDevice(device->input_device_id);
    }
    
    if (device->midi_output) {
        Pm_Close(device->midi_output);
        Pm_DestroyVirtualDevice(device->output_device_id);
    }
    
    // Destroy processor
    SIT_MidiProcessor_Destroy(device->processor);
    
    free(device);
}

/* Set callbacks */
void SIT_MidiDevice_SetCallbacks(SIT_MidiDevice *device, const SIT_MidiCallbacks *callbacks) {
    if (device && callbacks) {
        device->callbacks = *callbacks;
    }
}

/* Set MIDI channel */
void SIT_MidiDevice_SetChannel(SIT_MidiDevice *device, int channel) {
    if (device) {
        device->midi_channel = channel;
    }
}

/* Connect devices */
int SIT_MidiDevice_Connect(SIT_MidiDevice *source, SIT_MidiDevice *destination) {
    if (!source || !destination) return 0;
    if (!source->midi_output || !destination->midi_input) return 0;
    
    return Pm_ConnectVirtualDevices(source->output_device_id, destination->input_device_id) == pmNoError;
}

/* Disconnect devices */
int SIT_MidiDevice_Disconnect(SIT_MidiDevice *source, SIT_MidiDevice *destination) {
    if (!source || !destination) return 0;
    
    return Pm_DisconnectVirtualDevices(source->output_device_id, destination->input_device_id) == pmNoError;
}

/* Dispatch MIDI event to callbacks */
static void SIT_MidiDevice_DispatchEvent(SIT_MidiDevice *device, PmMessage message, uint32_t sample_offset) {
    uint8_t status = Pm_MessageStatus(message);
    uint8_t channel = status & 0x0F;
    uint8_t msg_type = status & 0xF0;
    uint8_t data1 = Pm_MessageData1(message);
    uint8_t data2 = Pm_MessageData2(message);
    
    // Check channel filter
    if (device->midi_channel >= 0 && device->midi_channel != channel) {
        return;  // Wrong channel
    }
    
    // Call raw message callback if set
    if (device->callbacks.on_midi_message) {
        device->callbacks.on_midi_message(device->device_ptr, message, sample_offset);
    }
    
    // Dispatch to specific callbacks
    switch (msg_type) {
        case 0x80:  // Note Off
            if (device->callbacks.on_note_off) {
                device->callbacks.on_note_off(device->device_ptr, data1, data2, sample_offset);
            }
            break;
            
        case 0x90:  // Note On
            if (data2 == 0) {
                // Note On with velocity 0 = Note Off
                if (device->callbacks.on_note_off) {
                    device->callbacks.on_note_off(device->device_ptr, data1, 0, sample_offset);
                }
            } else {
                if (device->callbacks.on_note_on) {
                    device->callbacks.on_note_on(device->device_ptr, data1, data2, sample_offset);
                }
            }
            break;
            
        case 0xA0:  // Polyphonic Aftertouch
            if (device->callbacks.on_aftertouch) {
                device->callbacks.on_aftertouch(device->device_ptr, data1, data2, sample_offset);
            }
            break;
            
        case 0xB0:  // Control Change
            if (device->callbacks.on_control_change) {
                device->callbacks.on_control_change(device->device_ptr, data1, data2, sample_offset);
            }
            break;
            
        case 0xC0:  // Program Change
            if (device->callbacks.on_program_change) {
                device->callbacks.on_program_change(device->device_ptr, data1, sample_offset);
            }
            break;
            
        case 0xD0:  // Channel Pressure
            if (device->callbacks.on_channel_pressure) {
                device->callbacks.on_channel_pressure(device->device_ptr, data1, sample_offset);
            }
            break;
            
        case 0xE0:  // Pitch Bend
            if (device->callbacks.on_pitch_bend) {
                int16_t bend = ((data2 << 7) | data1) - 8192;
                device->callbacks.on_pitch_bend(device->device_ptr, bend, sample_offset);
            }
            break;
    }
}

/* Process MIDI in audio callback */
void SIT_MidiDevice_ProcessAudio(SIT_MidiDevice *device, int frame_count) {
    if (!device || !device->active || !device->midi_input) return;
    
    SIT_MidiProcessor *proc = device->processor;
    
    // Read MIDI events from virtual buffer (real-time safe)
    PmEvent midi_events[32];
    int count = Pm_Read(device->midi_input, midi_events, 32);
    
    // Convert to sample-accurate events and queue
    for (int i = 0; i < count; i++) {
        SIT_MidiEvent event;
        event.message = midi_events[i].message;
        
        // Calculate sample offset (simplified - use timestamp if available)
        event.sample_offset = 0;  // Process immediately for now
        
        SIT_MidiProcessor_PushEvent(proc, event);
    }
    
    // Process events at their sample positions
    for (int i = 0; i < frame_count; i++) {
        // Check for events at this sample
        SIT_MidiEvent event;
        while (SIT_MidiProcessor_PeekEvent(proc, &event) && event.sample_offset == i) {
            // Dispatch to callbacks
            SIT_MidiDevice_DispatchEvent(device, event.message, i);
            SIT_MidiProcessor_PopEvent(proc, &event);
        }
    }
    
    proc->sample_position += frame_count;
}

/* Send MIDI event */
void SIT_MidiDevice_SendEvent(SIT_MidiDevice *device, PmMessage message, uint32_t sample_offset) {
    if (!device || !device->midi_output) return;
    
    PmEvent event;
    event.message = message;
    event.timestamp = sample_offset;  // Use sample offset as timestamp
    
    Pm_Write(device->midi_output, &event, 1);
}

/* Send Note On */
void SIT_MidiDevice_SendNoteOn(SIT_MidiDevice *device, uint8_t note, uint8_t velocity, uint32_t sample_offset) {
    int channel = (device->midi_channel >= 0) ? device->midi_channel : 0;
    PmMessage msg = Pm_Message(0x90 | channel, note, velocity);
    SIT_MidiDevice_SendEvent(device, msg, sample_offset);
}

/* Send Note Off */
void SIT_MidiDevice_SendNoteOff(SIT_MidiDevice *device, uint8_t note, uint8_t velocity, uint32_t sample_offset) {
    int channel = (device->midi_channel >= 0) ? device->midi_channel : 0;
    PmMessage msg = Pm_Message(0x80 | channel, note, velocity);
    SIT_MidiDevice_SendEvent(device, msg, sample_offset);
}

/* Send Control Change */
void SIT_MidiDevice_SendCC(SIT_MidiDevice *device, uint8_t controller, uint8_t value, uint32_t sample_offset) {
    int channel = (device->midi_channel >= 0) ? device->midi_channel : 0;
    PmMessage msg = Pm_Message(0xB0 | channel, controller, value);
    SIT_MidiDevice_SendEvent(device, msg, sample_offset);
}

/* Send Program Change */
void SIT_MidiDevice_SendProgramChange(SIT_MidiDevice *device, uint8_t program, uint32_t sample_offset) {
    int channel = (device->midi_channel >= 0) ? device->midi_channel : 0;
    PmMessage msg = Pm_Message(0xC0 | channel, program, 0);
    SIT_MidiDevice_SendEvent(device, msg, sample_offset);
}

/* All Notes Off */
void SIT_MidiDevice_AllNotesOff(SIT_MidiDevice *device) {
    if (!device || !device->midi_output) return;
    
    int channel = (device->midi_channel >= 0) ? device->midi_channel : 0;
    
    // Send All Notes Off CC (123)
    PmMessage msg = Pm_Message(0xB0 | channel, 123, 0);
    PmEvent event = {msg, 0};
    Pm_Write(device->midi_output, &event, 1);
}

// ================================================================================================
// DEVICE IDENTITY IMPLEMENTATION
// ================================================================================================

/* Set device identity */
void SIT_MidiDevice_SetIdentity(SIT_MidiDevice *device, const SIT_MidiDeviceIdentity *identity) {
    if (!device || !identity) return;
    device->identity = *identity;
}

/* Get device identity */
const SIT_MidiDeviceIdentity* SIT_MidiDevice_GetIdentity(const SIT_MidiDevice *device) {
    return device ? &device->identity : NULL;
}

/* Send Identity Reply */
void SIT_MidiDevice_SendIdentityReply(SIT_MidiDevice *device, uint8_t device_id) {
    if (!device || !device->midi_output) return;
    
    // Build Extended Identity Reply SysEx with ASCII device name:
    // F0 7E <device_id> 06 02 <manufacturer_id> <family> <model> <version> <ascii_name> F7
    
    // Calculate name length (null-terminated, but don't send null)
    int name_len = 0;
    while (device->identity.device_name[name_len] && name_len < 32) {
        name_len++;
    }
    
    // Total length: 17 (standard) + name_len + 1 (F7)
    int total_len = 17 + name_len;
    uint8_t* reply = (uint8_t*)malloc(total_len);
    if (!reply) return;
    
    // Standard Identity Reply header
    reply[0] = 0xF0;  // SysEx start
    reply[1] = 0x7E;  // Universal Non-Realtime
    reply[2] = device_id;  // Device ID (0x7F = respond to all)
    reply[3] = 0x06;  // Sub-ID #1: General Information
    reply[4] = 0x02;  // Sub-ID #2: Identity Reply
    
    // Manufacturer ID (3 bytes)
    reply[5] = device->identity.manufacturer_id[0];
    reply[6] = device->identity.manufacturer_id[1];
    reply[7] = device->identity.manufacturer_id[2];
    
    // Family (2 bytes)
    reply[8] = device->identity.family[0];
    reply[9] = device->identity.family[1];
    
    // Model (2 bytes)
    reply[10] = device->identity.model[0];
    reply[11] = device->identity.model[1];
    
    // Version (4 bytes)
    reply[12] = device->identity.version[0];
    reply[13] = device->identity.version[1];
    reply[14] = device->identity.version[2];
    reply[15] = device->identity.version[3];
    
    // Extended: ASCII device name (optional, but we always include it)
    for (int i = 0; i < name_len; i++) {
        reply[16 + i] = (uint8_t)device->identity.device_name[i];
    }
    
    reply[16 + name_len] = 0xF7;  // SysEx end
    
    // Send via Pm_WriteSysEx
    Pm_WriteSysEx(device->midi_output, 0, reply, total_len);
    
    free(reply);
}

/* Process incoming SysEx */
void SIT_MidiDevice_ProcessSysEx(SIT_MidiDevice *device, const uint8_t *data, int32_t length) {
    if (!device || !data || length < 6) return;
    
    // Check for Universal Device Inquiry (Identity Request)
    // F0 7E 7F 06 01 F7 (6 bytes)
    // or F0 7E <device_id> 06 01 F7
    if (length == 6 &&
        data[0] == 0xF0 &&  // SysEx start
        data[1] == 0x7E &&  // Universal Non-Realtime
        (data[2] == 0x7F || data[2] == device->midi_channel) &&  // Device ID (7F = all)
        data[3] == 0x06 &&  // General Information
        data[4] == 0x01 &&  // Identity Request
        data[5] == 0xF7) {  // SysEx end
        
        // Respond with Identity Reply
        SIT_MidiDevice_SendIdentityReply(device, data[2]);
        return;
    }
    
    // Pass to user callback if set
    if (device->callbacks.on_sysex) {
        device->callbacks.on_sysex(device->device_ptr, data, length, 0);
    }
}

#endif /* MIDI_DEVICE_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* MIDI_DEVICE_H */
