# MIDI Hybrid Architecture Plan
**Date:** March 8, 2026  
**Status:** 🔄 Phase 1 Complete - Phase 2 In Progress

## Progress Overview
- ✅ Phase 1: Virtual Infrastructure (100%)
- ✅ Phase 2: API Extensions & Routing (100%)
- ✅ Phase 3: Advanced Features (100%)
- ✅ Phase 4: Integration & Testing (85%)

**Overall Progress: 95% Complete**

**Status**: Core implementation complete and tested. Remaining items are future enhancements (Polysonix integration, examples, documentation).

## Overview

Expand `sit/aud/midi.h` to support both external hardware MIDI I/O (via WinMM on Windows) and internal cross-platform MIDI routing for connecting Situation components (sequencers, arpeggiators, effects) to synths (Polysonix, samplers, etc.).

## Design Goals

1. **Single Header** - Keep everything in `sit/aud/midi.h`, no additional files
2. **Dual Mode Operation**:
   - **External Mode**: Hardware MIDI devices via OS APIs (WinMM on Windows, future: ALSA/CoreMIDI)
   - **Internal Mode**: Pure in-memory MIDI routing (cross-platform, no OS dependencies)
3. **Unified API** - Same `PmStream` interface for both external and internal routing
4. **Zero Overhead** - Internal routing has no system calls, just memory operations
5. **Thread-Safe** - Lock-free ring buffers for real-time audio thread safety
6. **Backward Compatible** - Existing WinMM code continues to work

## Architecture

### Current State (External Only)
```
[Hardware MIDI Device] <--WinMM--> [PmStream] <--> [Application]
```

### Proposed State (Hybrid)
```
[Hardware MIDI Device] <--WinMM--> [PmStream (External)] <--> [Application]
                                           |
                                           v
                                    [MIDI Router]
                                           |
                                           v
[Sequencer/Arp] <--> [PmStream (Internal)] <--> [Polysonix/Sampler]
```

## Implementation Strategy

### Phase 1: Internal MIDI Infrastructure - ✅ COMPLETE

#### 1.1 Virtual Device System - ✅ COMPLETE
Add virtual MIDI devices that exist only in memory:

```c
#define PM_DEVICE_TYPE_HARDWARE  0  // Existing WinMM devices
#define PM_DEVICE_TYPE_VIRTUAL   1  // New internal devices

typedef struct {
    int device_type;  // Hardware or Virtual
    // ... existing PmDeviceInfo fields
} PmDeviceInfoEx;
```

#### 1.2 Lock-Free Ring Buffer - ✅ COMPLETE
Implement a lock-free SPSC (Single Producer Single Consumer) ring buffer for real-time safety:

```c
typedef struct {
    PmEvent events[8192];  // Power of 2 for fast modulo
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    char padding[64 - 2*sizeof(uint32_t)]; // Cache line alignment
} PmVirtualBuffer;
```

**Why lock-free?**
- No mutex overhead in audio thread
- No priority inversion risk
- Predictable latency
- Cache-friendly with padding

#### 1.3 Virtual Stream Structure - ✅ COMPLETE
```c
typedef struct {
    PmVirtualBuffer *buffer;
    int is_virtual;
    char name[64];
    void *user_data;  // For routing callbacks
} PmVirtualStream;
```

### Phase 2: API Extensions - ✅ COMPLETE

#### 2.1 Virtual Device Creation - ✅ COMPLETE
```c
// Create a virtual MIDI port (input or output)
PmError Pm_CreateVirtualDevice(
    const char *name,
    int is_input,
    PmDeviceID *out_device_id
);

// Destroy a virtual device
PmError Pm_DestroyVirtualDevice(PmDeviceID device_id);
```

**Usage Example:**
```c
// Create virtual output for sequencer
PmDeviceID seq_out;
Pm_CreateVirtualDevice("Sequencer Out", 0, &seq_out);

// Create virtual input for Polysonix
PmDeviceID synth_in;
Pm_CreateVirtualDevice("Polysonix In", 1, &synth_in);

// Connect them via router
Pm_ConnectVirtualDevices(seq_out, synth_in);
```

#### 2.2 MIDI Routing - ✅ COMPLETE
```c
// Connect two virtual devices (output -> input)
PmError Pm_ConnectVirtualDevices(
    PmDeviceID source_output,
    PmDeviceID dest_input
);

// Disconnect devices
PmError Pm_DisconnectVirtualDevices(
    PmDeviceID source_output,
    PmDeviceID dest_input
);

// Route external hardware to internal virtual device
PmError Pm_RouteHardwareToVirtual(
    PmDeviceID hardware_input,
    PmDeviceID virtual_output
);
```

#### 2.3 MIDI Router - ✅ COMPLETE
Central routing matrix for connecting devices:

```c
#define MAX_VIRTUAL_DEVICES 64
#define MAX_CONNECTIONS 128

typedef struct {
    PmDeviceID source;
    PmDeviceID destination;
    int active;
    // Optional: MIDI filtering, transposition, channel mapping
    uint8_t channel_filter;  // 0 = all channels, 1-16 = specific
    int8_t transpose;        // Semitones to transpose notes
} PmConnection;

typedef struct {
    PmConnection connections[MAX_CONNECTIONS];
    int connection_count;
    CRITICAL_SECTION router_lock;
} PmRouter;
```

### Phase 3: Stream Unification - ✅ COMPLETE

#### 3.1 Unified PmStream - ✅ COMPLETE
Extend existing `PmStream` to handle both hardware and virtual:

```c
struct PmStream {
    MidiDevice *device;           // Existing: hardware device
    MidiInputBuffer *input_buffer; // Existing: hardware buffer
    
    // NEW: Virtual device support
    PmVirtualBuffer *virtual_buffer;
    int is_virtual;
    
    // NEW: Routing connections
    PmStream *connected_streams[16]; // Max 16 connections per stream
    int connection_count;
    
    // Existing fields...
    MIDIHDR sysex_hdr_output;
    uint8_t sysex_buffer_output[SYSEX_BUFFER_SIZE];
    volatile int sysex_pending;
    PmTimestamp (*time_proc)(void *time_info);
    void *time_info;
    int32_t latency;
};
```

#### 3.2 Transparent Read/Write - ✅ COMPLETE
Modify `Pm_Read` and `Pm_Write` to automatically detect stream type:

```c
int32_t Pm_Read(PmStream *stream, PmEvent *buffer, int32_t length) {
    if (!stream || !buffer || length <= 0) return 0;
    
    // NEW: Check if virtual stream
    if (stream->is_virtual) {
        return Pm_ReadVirtual(stream, buffer, length);
    }
    
    // Existing: Hardware stream
    if (!stream->input_buffer || !stream->device || !stream->device->info.input) 
        return pmBadData;
    // ... existing hardware read code
}

PmError Pm_Write(PmStream *stream, PmEvent *buffer, int32_t length) {
    if (!stream || !buffer || length < 0) return pmBadPtr;
    if (length == 0) return pmNoError;
    
    // NEW: Check if virtual stream
    if (stream->is_virtual) {
        return Pm_WriteVirtual(stream, buffer, length);
    }
    
    // Existing: Hardware stream
    if (!stream->device || !stream->device->info.output || !stream->device->out_handle) 
        return pmBadData;
    // ... existing hardware write code
}
```

### Phase 4: Lock-Free Virtual I/O - ✅ COMPLETE

#### 4.1 Virtual Write (Producer) - ✅ COMPLETE
```c
static PmError Pm_WriteVirtual(PmStream *stream, PmEvent *buffer, int32_t length) {
    PmVirtualBuffer *vbuf = stream->virtual_buffer;
    if (!vbuf) return pmBadData;
    
    for (int i = 0; i < length; i++) {
        uint32_t write_pos = vbuf->write_pos;
        uint32_t next_write = (write_pos + 1) & 8191; // Fast modulo for power of 2
        
        // Check if buffer full (leave one slot empty for full/empty distinction)
        if (next_write == vbuf->read_pos) {
            return pmBufferOverflow; // Buffer full
        }
        
        // Write event
        vbuf->events[write_pos] = buffer[i];
        
        // Memory barrier: ensure write completes before updating write_pos
        _mm_sfence(); // x86/x64 store fence
        
        // Update write position (atomic on x86/x64 for aligned 32-bit writes)
        vbuf->write_pos = next_write;
        
        // Route to connected streams
        for (int j = 0; j < stream->connection_count; j++) {
            if (stream->connected_streams[j]) {
                Pm_Write(stream->connected_streams[j], &buffer[i], 1);
            }
        }
    }
    
    return pmNoError;
}
```

#### 4.2 Virtual Read (Consumer) - ✅ COMPLETE
```c
static int32_t Pm_ReadVirtual(PmStream *stream, PmEvent *buffer, int32_t length) {
    PmVirtualBuffer *vbuf = stream->virtual_buffer;
    if (!vbuf) return 0;
    
    int count = 0;
    uint32_t read_pos = vbuf->read_pos;
    uint32_t write_pos = vbuf->write_pos;
    
    while (count < length && read_pos != write_pos) {
        // Read event
        buffer[count] = vbuf->events[read_pos];
        count++;
        
        // Update read position
        read_pos = (read_pos + 1) & 8191;
        
        // Memory barrier: ensure read completes before updating read_pos
        _mm_lfence(); // x86/x64 load fence
        
        // Update read position (atomic)
        vbuf->read_pos = read_pos;
        
        // Refresh write_pos for next iteration
        write_pos = vbuf->write_pos;
    }
    
    return count;
}
```

### Phase 5: Platform Abstraction - ✅ COMPLETE

#### 5.1 Conditional Compilation - ✅ COMPLETE
```c
// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define PM_PLATFORM_WINDOWS 1
    #define PM_HAS_HARDWARE_MIDI 1
#elif defined(__linux__)
    #define PM_PLATFORM_LINUX 1
    #define PM_HAS_HARDWARE_MIDI 0  // TODO: ALSA support
#elif defined(__APPLE__)
    #define PM_PLATFORM_MACOS 1
    #define PM_HAS_HARDWARE_MIDI 0  // TODO: CoreMIDI support
#else
    #define PM_PLATFORM_UNKNOWN 1
    #define PM_HAS_HARDWARE_MIDI 0
#endif

// Virtual MIDI always available
#define PM_HAS_VIRTUAL_MIDI 1
```

#### 5.2 Graceful Degradation - ✅ COMPLETE
```c
PmError Pm_Initialize(void) {
    if (midi_context.initialized) return pmNoError;
    
    InitializeCriticalSection(&midi_context.context_lock);
    EnterCriticalSection(&midi_context.context_lock);
    
    midi_cleanup_internal();
    midi_context.device_count = 0;
    
#if PM_HAS_HARDWARE_MIDI
    // Existing: Enumerate hardware devices via WinMM
    UINT num_in_devs = midiInGetNumDevs();
    UINT num_out_devs = midiOutGetNumDevs();
    // ... existing enumeration code
#else
    // No hardware MIDI on this platform
    printf("[MIDI] Hardware MIDI not available on this platform\n");
#endif
    
    // NEW: Always initialize virtual MIDI system
    Pm_InitializeVirtualMidi();
    
    midi_context.initialized = 1;
    LeaveCriticalSection(&midi_context.context_lock);
    return pmNoError;
}
```

### Phase 6: Advanced Features - ⏳ PENDING

#### 6.1 MIDI Filtering - ⏳ PENDING
```c
typedef struct {
    uint8_t filter_note_on;
    uint8_t filter_note_off;
    uint8_t filter_cc;
    uint8_t filter_program_change;
    uint8_t filter_pitch_bend;
    uint8_t filter_aftertouch;
    uint8_t filter_sysex;
    uint8_t channel_mask;  // Bit mask for channels 0-15
} PmFilter;

PmError Pm_SetConnectionFilter(
    PmDeviceID source,
    PmDeviceID dest,
    const PmFilter *filter
);
```

#### 6.2 MIDI Transformation - ⏳ PENDING
```c
typedef struct {
    int8_t transpose;           // Semitones (-127 to +127)
    uint8_t velocity_curve;     // 0=linear, 1=exponential, 2=logarithmic
    float velocity_scale;       // Multiply velocity (0.0 to 2.0)
    uint8_t channel_remap[16];  // Map input channel to output channel
} PmTransform;

PmError Pm_SetConnectionTransform(
    PmDeviceID source,
    PmDeviceID dest,
    const PmTransform *transform
);
```

#### 6.3 MIDI Recording/Playback - ⏳ PENDING
```c
typedef struct {
    PmEvent *events;
    int32_t event_count;
    int32_t capacity;
    PmTimestamp start_time;
} PmRecording;

PmError Pm_StartRecording(PmStream *stream, PmRecording *recording);
PmError Pm_StopRecording(PmStream *stream);
PmError Pm_PlayRecording(PmStream *stream, const PmRecording *recording);
```

## Integration with Situation

### Polysonix Integration
```c
// In Polysonix initialization
PmDeviceID polysonix_in;
Pm_CreateVirtualDevice("Polysonix MIDI In", 1, &polysonix_in);

PmStream *polysonix_stream;
Pm_OpenInput(&polysonix_stream, polysonix_in, NULL, 512, NULL, NULL);

// In audio callback
PmEvent midi_events[32];
int count = Pm_Read(polysonix_stream, midi_events, 32);
for (int i = 0; i < count; i++) {
    uint8_t status = Pm_MessageStatus(midi_events[i].message);
    uint8_t data1 = Pm_MessageData1(midi_events[i].message);
    uint8_t data2 = Pm_MessageData2(midi_events[i].message);
    
    if (status == 0x90) { // Note On
        PX_NoteOn(polysonix, data1, data2);
    } else if (status == 0x80) { // Note Off
        PX_NoteOff(polysonix, data1);
    }
}
```

### Sequencer Integration
```c
// Create sequencer output
PmDeviceID seq_out;
Pm_CreateVirtualDevice("Sequencer Out", 0, &seq_out);

PmStream *seq_stream;
Pm_OpenOutput(&seq_stream, seq_out, NULL, 0, NULL, NULL, 0);

// Connect to Polysonix
Pm_ConnectVirtualDevices(seq_out, polysonix_in);

// Send notes from sequencer
PmEvent note_on;
note_on.message = Pm_Message(0x90, 60, 100); // Middle C
note_on.timestamp = 0;
Pm_Write(seq_stream, &note_on, 1);
```

### Hardware Passthrough
```c
// Route hardware MIDI keyboard to Polysonix
PmDeviceID hardware_keyboard = Pm_GetDefaultInputDeviceID();
Pm_RouteHardwareToVirtual(hardware_keyboard, polysonix_in);

// Now hardware keyboard plays Polysonix directly
```

## Memory Layout

### Virtual Device Registry
```c
typedef struct {
    // Existing hardware devices
    MidiDevice devices[MAX_DEVICES];
    int device_count;
    
    // NEW: Virtual devices
    struct {
        PmDeviceInfo info;
        PmVirtualBuffer *buffer;
        int active;
    } virtual_devices[MAX_VIRTUAL_DEVICES];
    int virtual_device_count;
    
    // NEW: Router
    PmRouter router;
    
    // Existing fields
    int initialized;
    CRITICAL_SECTION context_lock;
    volatile int device_list_changed_flag;
    PmDeviceChangeCallback device_change_callback;
    void *device_change_user_data;
} MidiContext;
```

## Performance Characteristics

### Virtual MIDI Performance
- **Latency**: ~0.1μs (single memory copy)
- **Throughput**: ~10M events/sec (lock-free ring buffer)
- **Memory**: 256KB per virtual device (8192 events × 32 bytes)
- **CPU**: Negligible (<0.1% on modern CPU)

### Hardware MIDI Performance (Existing)
- **Latency**: ~1-5ms (OS driver overhead)
- **Throughput**: ~3000 events/sec (MIDI 1.0 spec: 31.25 kbaud)
- **Memory**: Minimal (OS handles buffering)
- **CPU**: Low (<1% with callbacks)

## Testing Strategy

### Unit Tests
1. Virtual device creation/destruction
2. Lock-free ring buffer (producer/consumer)
3. MIDI routing (1-to-1, 1-to-many, many-to-1)
4. Hardware-to-virtual routing
5. MIDI filtering and transformation
6. Buffer overflow handling
7. Thread safety (concurrent read/write)

### Integration Tests
1. Sequencer → Polysonix
2. Hardware keyboard → Polysonix
3. Arpeggiator → Multiple synths
4. MIDI recording/playback
5. Real-time performance (audio thread)

### Example Test
```c
void test_virtual_routing() {
    Pm_Initialize();
    
    // Create virtual devices
    PmDeviceID out_dev, in_dev;
    Pm_CreateVirtualDevice("Test Out", 0, &out_dev);
    Pm_CreateVirtualDevice("Test In", 1, &in_dev);
    
    // Connect them
    Pm_ConnectVirtualDevices(out_dev, in_dev);
    
    // Open streams
    PmStream *out_stream, *in_stream;
    Pm_OpenOutput(&out_stream, out_dev, NULL, 0, NULL, NULL, 0);
    Pm_OpenInput(&in_stream, in_dev, NULL, 512, NULL, NULL);
    
    // Write event
    PmEvent out_event = {Pm_Message(0x90, 60, 100), 0};
    Pm_Write(out_stream, &out_event, 1);
    
    // Read event
    PmEvent in_event;
    int count = Pm_Read(in_stream, &in_event, 1);
    
    // Verify
    assert(count == 1);
    assert(in_event.message == out_event.message);
    
    // Cleanup
    Pm_Close(out_stream);
    Pm_Close(in_stream);
    Pm_DestroyVirtualDevice(out_dev);
    Pm_DestroyVirtualDevice(in_dev);
    Pm_Terminate();
}
```

## Migration Path

### Phase 1: Add Virtual Infrastructure (Week 1) - ✅ COMPLETE
- [x] Implement `PmVirtualBuffer` and lock-free operations
- [x] Add virtual device structures (`PmVirtualDevice`, `PmConnection`, `PmRouter`)
- [x] Extend `PmStream` to support virtual devices
- [x] Add platform detection macros
- [x] Implement `Pm_InitVirtualBuffer()`
- [x] Implement `Pm_WriteVirtual()` (lock-free producer)
- [x] Implement `Pm_ReadVirtual()` (lock-free consumer)
- [x] Implement `Pm_InitializeVirtualMidi()`
- [x] Update `Pm_Initialize()` to initialize virtual MIDI
- [x] Update `Pm_Terminate()` to cleanup virtual devices
- [x] Update `midi_cleanup_internal()` for virtual devices
- [x] Add conditional compilation for hardware MIDI

### Phase 2: Implement Routing (Week 2) - ✅ COMPLETE
- [x] Implement `Pm_CreateVirtualDevice()`
- [x] Implement `Pm_DestroyVirtualDevice()`
- [x] Implement `Pm_IsVirtualDevice()`
- [x] Build `PmRouter` connection management
- [x] Implement `Pm_ConnectVirtualDevices()`
- [x] Implement `Pm_DisconnectVirtualDevices()`
- [x] Update `Pm_OpenInput()` for virtual devices
- [x] Update `Pm_OpenOutput()` for virtual devices
- [x] Update `Pm_Close()` for virtual devices
- [x] Update `Pm_Read()` for transparent routing
- [x] Update `Pm_Write()` for transparent routing
- [x] Update `Pm_CountDevices()` to include virtual devices
- [x] Update `Pm_GetDeviceInfo()` for virtual devices

### Phase 3: Unify API (Week 3) - ✅ COMPLETE
- [x] Modify `Pm_Read()` for transparent operation (detect stream type)
- [x] Modify `Pm_Write()` for transparent operation (detect stream type)
- [x] Update `Pm_CountDevices()` to include virtual devices
- [x] Update `Pm_GetDeviceInfo()` for virtual devices
- [x] Update `Pm_GetDefaultInputDeviceID()` logic
- [x] Update `Pm_GetDefaultOutputDeviceID()` logic
- [x] Add MIDI filtering (`PmFilter` structure)
- [x] Add MIDI transformation (`PmTransform` structure)
- [x] Implement `Pm_SetConnectionFilter()`
- [x] Implement `Pm_SetConnectionTransform()`
- [x] Implement MIDI recording (`PmRecording` structure)
- [x] Implement `Pm_CreateRecording()`
- [x] Implement `Pm_FreeRecording()`
- [x] Implement `Pm_StartRecording()`
- [x] Implement `Pm_StopRecording()`
- [x] Implement `Pm_PlayRecording()`

### Phase 4: Integration & Testing (Week 4) - ✅ COMPLETE
- [x] Create virtual MIDI test program (`virtual_midi_test.c`)
- [x] Test lock-free ring buffer under load (performance test)
- [x] Test multiple connections (1-to-1, 1-to-many, many-to-1) (`routing_test.c`)
- [x] Test MIDI filtering (`filter_transform_test.c`)
- [x] Test MIDI transformation (`filter_transform_test.c`)
- [x] Test MIDI recording and playback (`recording_test.c`)
- [x] Performance testing and optimization (`performance_test.c`)
- [x] Buffer overflow handling verified
- [x] Thread safety verification (lock-free atomics)
- [x] Concurrent connections test (10 simultaneous pairs)
- [x] Comprehensive test suite created (`run_all_midi_tests.bat`)
- [ ] Integrate with Polysonix synth (future work)
- [ ] Create sequencer example (future work)
- [ ] Create hardware passthrough example (future work)
- [ ] Memory leak testing (future work)
- [ ] API reference documentation (future work)
- [ ] Usage examples and tutorials (future work)

## Backward Compatibility

All existing code using hardware MIDI continues to work unchanged:
```c
// Existing code - still works
Pm_Initialize();
PmDeviceID dev = Pm_GetDefaultOutputDeviceID();
PmStream *stream;
Pm_OpenOutput(&stream, dev, NULL, 0, NULL, NULL, 0);
PmEvent event = {Pm_Message(0x90, 60, 100), 0};
Pm_Write(stream, &event, 1);
Pm_Close(stream);
Pm_Terminate();
```

New virtual MIDI is opt-in via new API calls.

## Future Enhancements

1. **MIDI 2.0 Support** - 32-bit resolution, bidirectional communication
2. **ALSA Backend** - Linux hardware MIDI support
3. **CoreMIDI Backend** - macOS hardware MIDI support
4. **MIDI Learn** - Automatic CC mapping
5. **MIDI Clock Sync** - Tempo synchronization
6. **MPE Support** - Multi-dimensional polyphonic expression
7. **Network MIDI** - RTP-MIDI, OSC integration

## Summary

This plan extends `sit/aud/midi.h` to be a complete MIDI solution:
- **External**: Hardware MIDI I/O for development/testing (Windows WinMM, future: ALSA/CoreMIDI)
- **Internal**: Cross-platform virtual MIDI routing for production use
- **Unified**: Single API for both modes
- **Performant**: Lock-free, real-time safe, zero-copy where possible
- **Flexible**: Routing, filtering, transformation, recording
- **Compatible**: Existing code continues to work

The result is a professional-grade MIDI system suitable for DAWs, plugins, and game engines.
