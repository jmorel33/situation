# MIDI Device Callbacks Architecture

**Status**: ✅ Complete  
**Date**: March 9, 2026  
**Author**: Jacques Morel

## Overview

The MIDI Device Callbacks system provides a centralized, maintainable architecture for mapping MIDI control messages to device parameters in the Situation audio engine. This document describes the architecture, design decisions, and usage patterns.

## Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│                    MIDI Controller                          │
│              (Hardware or Virtual Device)                   │
└────────────────────────┬────────────────────────────────────┘
                         │ MIDI Messages (CC, Note, etc.)
                         ↓
┌─────────────────────────────────────────────────────────────┐
│                    midi.h (Low-Level)                       │
│  • Lock-free ring buffers                                   │
│  • Virtual/Hardware MIDI streams                            │
│  • Routing system                                           │
│  • Sample-accurate timing                                   │
└────────────────────────┬────────────────────────────────────┘
                         │ PmEvent structs
                         ↓
┌─────────────────────────────────────────────────────────────┐
│              midi_device.h (Infrastructure)                 │
│  • SIT_MidiDevice wrapper                                   │
│  • Callback dispatch (on_note_on, on_cc, etc.)             │
│  • Channel filtering                                        │
│  • Device capabilities                                      │
└────────────────────────┬────────────────────────────────────┘
                         │ Callback invocation
                         ↓
┌─────────────────────────────────────────────────────────────┐
│         midi_device_callbacks.h (THIS LAYER)                │
│  • Device-specific CC → control mappings                    │
│  • Parameter normalization (linear, log, dB)               │
│  • Callback lookup table                                    │
│  • Single source of truth for MIDI mappings                │
└────────────────────────┬────────────────────────────────────┘
                         │ Control array updates
                         ↓
┌─────────────────────────────────────────────────────────────┐
│           device_wrappers.h (Node Graph Layer)              │
│  • Create/Process/Destroy functions                         │
│  • Control array → DSP parameter mapping                    │
│  • Audio processing dispatch                                │
└────────────────────────┬────────────────────────────────────┘
                         │ DSP function calls
                         ↓
┌─────────────────────────────────────────────────────────────┐
│              Device DSP Implementation                      │
│         (compander.h, dynamics.h, filter.h, etc.)          │
│  • Pure DSP algorithms                                      │
│  • No MIDI knowledge                                        │
└─────────────────────────────────────────────────────────────┘
```

## Design Principles

### 1. Separation of Concerns

Each layer has a single, well-defined responsibility:

- **midi.h**: Low-level MIDI transport (buffers, streams, routing)
- **midi_device.h**: Callback infrastructure (dispatch, filtering)
- **midi_device_callbacks.h**: MIDI → control mapping (THIS IS THE BRIDGE)
- **device_wrappers.h**: Control → DSP parameter mapping
- **Device DSP**: Pure audio processing

### 2. Centralization

All MIDI mappings live in one file (`midi_device_callbacks.h`):

```c
// ✅ GOOD: Single source of truth
// All compander MIDI mappings in midi_device_callbacks.h

// ❌ BAD: Scattered across multiple files
// compander.h has some MIDI code
// device_wrappers.h has some MIDI code
// User code has some MIDI code
```

### 3. Discoverability

The callback lookup table makes it easy to see which devices support MIDI:

```c
static const SIT_MidiCallbackEntry g_midi_callback_table[] = {
    { SITUATION_NODE_COMPANDER,  _SituationCompanderOnControlChange,  "Compander" },
    { SITUATION_NODE_DYNAMICS,   _SituationDynamicsOnControlChange,   "Dynamics" },
    { SITUATION_NODE_FILTER,     _SituationFilterOnControlChange,     "Filter" },
    // ... etc
};

// Check if device supports MIDI:
if (SIT_DeviceSupportsMidi(SITUATION_NODE_COMPANDER)) {
    // Setup MIDI control
}
```

### 4. Documentation as Code

Each callback function documents its MIDI CC mapping:

```c
/**
 * @brief MIDI CC mapping for Compander.
 * 
 * MIDI CC MAPPING:
 * Band 0 (Low):
 *   CC 16 → Control 0  (comp_thresh: 0.0-1.0)
 *   CC 17 → Control 1  (exp_thresh: 0.0-1.0)
 *   ...
 */
static void _SituationCompanderOnControlChange(...) {
    // Implementation
}
```

This serves as both documentation and implementation.

## Control Flow Example: Compander

Let's trace a MIDI CC message through the entire system:

### Step 1: MIDI Input (Hardware Controller)

```
User turns knob on MIDI controller
→ Sends CC 16 (value 64) on channel 0
```

### Step 2: Low-Level MIDI (midi.h)

```c
// PortMidi receives message
PmEvent event;
event.message = Pm_Message(0xB0, 16, 64);  // CC on channel 0
event.timestamp = 12345;

// Written to lock-free ring buffer
Pm_Write(stream, &event, 1);
```

### Step 3: Audio Callback (User Code)

```c
void audio_callback(...) {
    // Read MIDI events
    PmEvent events[32];
    int count = Pm_Read(midi_in, events, 32);
    
    // Dispatch to MIDI device
    for (int i = 0; i < count; i++) {
        uint8_t status = Pm_MessageStatus(events[i].message);
        uint8_t cc_num = Pm_MessageData1(events[i].message);
        uint8_t cc_val = Pm_MessageData2(events[i].message);
        
        if ((status & 0xF0) == 0xB0) {  // Control Change
            SIT_MidiDevice_ProcessControlChange(midi_device, 
                                                 status & 0x0F, 
                                                 cc_num, 
                                                 cc_val);
        }
    }
}
```

### Step 4: Callback Infrastructure (midi_device.h)

```c
void SIT_MidiDevice_ProcessControlChange(SIT_MidiDevice* device, 
                                          uint8_t channel,
                                          uint8_t cc_number, 
                                          uint8_t cc_value) {
    // Channel filtering
    if (device->channel != SIT_MIDI_CHANNEL_ALL && 
        device->channel != channel) {
        return;
    }
    
    // Dispatch to callback
    if (device->callbacks.on_control_change) {
        device->callbacks.on_control_change(device->callbacks.user_data,
                                             channel, cc_number, cc_value);
    }
}
```

### Step 5: MIDI → Control Mapping (midi_device_callbacks.h)

```c
static void _SituationCompanderOnControlChange(void* user_data, 
                                                uint8_t channel,
                                                uint8_t cc_number, 
                                                uint8_t cc_value) {
    float* controls = (float*)user_data;  // Control array
    
    // CC 16 maps to control 0 (Band 0 comp_thresh)
    if (cc_number == 16) {
        // Normalize CC value (0-127) to parameter range (0.0-1.0)
        controls[0] = _SituationNormalizeMidiCC(cc_value, 0.0f, 1.0f);
        // controls[0] = 64/127 = 0.504
    }
}
```

### Step 6: Node Graph Processing (device_wrappers.h)

```c
static void _SituationProcessCompanderNode(void* device_data,
                                            SituationAudioPort* inputs,
                                            SituationAudioPort* outputs,
                                            float* controls,  // ← Updated by MIDI
                                            int frames) {
    CompanderProcessor* comp = (CompanderProcessor*)device_data;
    
    // Read control array (updated by MIDI callback)
    CompanderParams params = {
        .comp_thresh = controls[0],  // ← 0.504 from MIDI CC 16
        .exp_thresh = controls[1],
        // ... etc
    };
    
    // Update DSP parameters
    compander_update_band_params(comp, 0, &params, &bell_params);
    
    // Process audio
    compander_process(comp, inputs[0].buffer, outputs[0].buffer, frames);
}
```

### Step 7: DSP Processing (compander.h)

```c
void compander_process(CompanderProcessor* proc, 
                       const float* input, 
                       float* output, 
                       unsigned long frame_count) {
    // Pure DSP - no MIDI knowledge
    // Uses comp_thresh = 0.504 set by MIDI
    // ... audio processing ...
}
```

## Parameter Normalization

The system provides helper functions for common parameter scaling:

### Linear Normalization

```c
// CC 0-127 → 0.0-1.0
float value = _SituationNormalizeMidiCC(cc_value, 0.0f, 1.0f);

// CC 0-127 → -1.0 to +1.0 (pan)
float pan = _SituationNormalizeMidiCC(cc_value, -1.0f, 1.0f);
```

### Logarithmic Normalization

```c
// CC 0-127 → 20Hz-20000Hz (frequency)
float freq = _SituationNormalizeMidiCCLog(cc_value, 20.0f, 20000.0f);

// CC 0-127 → 0.1Hz-10Hz (LFO rate)
float rate = _SituationNormalizeMidiCCLog(cc_value, 0.1f, 10.0f);
```

### Decibel Normalization

```c
// CC 0-127 → -60dB to 0dB (threshold)
float thresh = _SituationNormalizeMidiCCDb(cc_value, -60.0f, 0.0f);

// CC 0-127 → -24dB to +24dB (gain)
float gain = _SituationNormalizeMidiCCDb(cc_value, -24.0f, 24.0f);
```

## MIDI CC Allocation Strategy

To avoid conflicts, we use a systematic CC allocation:

### Standard MIDI CCs (Preserved)

- CC 1: Modulation Wheel
- CC 7: Volume
- CC 10: Pan
- CC 11: Expression
- CC 64: Sustain Pedal
- CC 91-95: Effects (Reverb, Chorus, etc.)

### Custom CC Ranges

- **CC 16-39**: Compander (24 controls, 3 bands × 8 params)
- **CC 40-45**: Filter (6 controls)
- **CC 46-57**: EQ 4-Band (12 controls, 4 bands × 3 params)
- **CC 70-76**: Dynamics (7 controls)
- **CC 80-83**: Overdrive (4 controls)
- **CC 102-103**: LFO (2 controls)

### Guidelines

1. Group related parameters together (e.g., all compander CCs are contiguous)
2. Use standard CCs when available (e.g., CC 10 for pan, CC 91 for reverb)
3. Document CC allocations in callback function headers
4. Reserve CC 120-127 for channel mode messages (MIDI standard)

## Adding MIDI Support to a New Device

### Step 1: Define CC Mapping

Decide which MIDI CCs will control which parameters:

```c
// Example: New "Delay" device with 4 parameters
// CC 58 → time (10ms-2000ms, log)
// CC 59 → feedback (0.0-0.95)
// CC 60 → mix (0.0-1.0)
// CC 61 → filter_cutoff (20Hz-20kHz, log)
```

### Step 2: Implement Callback

Add to `midi_device_callbacks.h`:

```c
/**
 * @brief MIDI CC mapping for Delay.
 * 
 * DEVICE: Delay (SITUATION_NODE_DELAY)
 * CONTROLS: 4 total
 * 
 * MIDI CC MAPPING:
 *   CC 58 → Control 0 (time: 10ms-2000ms, log)
 *   CC 59 → Control 1 (feedback: 0.0-0.95)
 *   CC 60 → Control 2 (mix: 0.0-1.0)
 *   CC 61 → Control 3 (filter_cutoff: 20Hz-20kHz, log)
 */
static void _SituationDelayOnControlChange(void* user_data, 
                                            uint8_t channel,
                                            uint8_t cc_number, 
                                            uint8_t cc_value) {
    (void)channel;
    
    float* controls = (float*)user_data;
    if (!controls) return;
    
    switch (cc_number) {
        case 58: controls[0] = _SituationNormalizeMidiCCLog(cc_value, 10.0f, 2000.0f); break;
        case 59: controls[1] = _SituationNormalizeMidiCC(cc_value, 0.0f, 0.95f); break;
        case 60: controls[2] = _SituationNormalizeMidiCC(cc_value, 0.0f, 1.0f); break;
        case 61: controls[3] = _SituationNormalizeMidiCCLog(cc_value, 20.0f, 20000.0f); break;
    }
}
```

### Step 3: Add to Lookup Table

```c
static const SIT_MidiCallbackEntry g_midi_callback_table[] = {
    // ... existing entries ...
    { SITUATION_NODE_DELAY, _SituationDelayOnControlChange, "Delay" },
};
```

### Step 4: Use in Application

```c
// Create node
SituationNode* delay_node = SituationCreateNode(graph, SITUATION_NODE_DELAY);

// Setup MIDI control
const SIT_MidiCallbackEntry* entry = SIT_GetMidiCallbackForDevice(SITUATION_NODE_DELAY);
if (entry) {
    SIT_MidiDevice* midi = SIT_MidiDevice_Create(entry->device_name,
                                                  SIT_MIDI_DEVICE_EFFECT,
                                                  SIT_MIDI_CAP_INPUT,
                                                  delay_node);
    
    SIT_MidiCallbacks callbacks = {0};
    callbacks.on_control_change = entry->on_control_change;
    callbacks.user_data = delay_node->controls;
    
    SIT_MidiDevice_SetCallbacks(midi, &callbacks);
}
```

Done! The device now responds to MIDI CC messages.

## Currently Supported Devices

| Device | Node Type | CC Range | Parameters |
|--------|-----------|----------|------------|
| Compander | SITUATION_NODE_COMPANDER | 16-39 | 24 (3 bands × 8) |
| Dynamics | SITUATION_NODE_DYNAMICS | 70-76 | 7 |
| Filter | SITUATION_NODE_FILTER | 40-45 | 6 |
| EQ 4-Band | SITUATION_NODE_EQ_4BAND | 46-57 | 12 (4 bands × 3) |
| Reverb | SITUATION_NODE_REVERB | 91-95 | 5 |
| Chorus | SITUATION_NODE_CHORUS | 12-15 | 4 |
| Overdrive | SITUATION_NODE_OVERDRIVE | 80-83 | 4 |
| Panner | SITUATION_NODE_PANNER | 10 | 1 |
| LFO | SITUATION_NODE_LFO | 102-103 | 2 |

## Thread Safety

### Control Array Access

The control array is accessed from two threads:

1. **Audio thread**: Reads controls in `_SituationProcess*Node()`
2. **MIDI thread**: Writes controls in `_Situation*OnControlChange()`

This is safe because:

- Writes are atomic (single float assignment)
- Reads are atomic (single float read)
- No complex data structures
- No locks needed (lock-free by design)

### Best Practices

```c
// ✅ GOOD: Atomic write
controls[0] = new_value;

// ✅ GOOD: Atomic read
float value = controls[0];

// ❌ BAD: Non-atomic operation
controls[0] = controls[0] * 2.0f;  // Read-modify-write race

// ✅ GOOD: Use local copy for complex operations
float temp = controls[0];
temp = temp * 2.0f;
controls[0] = temp;
```

## Performance Considerations

### Callback Overhead

MIDI callbacks are extremely lightweight:

```c
// Typical callback: ~10 CPU cycles
controls[0] = cc_value / 127.0f;  // 1 division, 1 store
```

### Normalization Functions

Helper functions are inlined for zero overhead:

```c
static inline float _SituationNormalizeMidiCC(uint8_t cc_value, 
                                               float min, float max) {
    float normalized = (float)cc_value / 127.0f;
    return min + normalized * (max - min);
}
// Compiles to: ~5 instructions (div, mul, add, store)
```

### Lookup Table

Device lookup is O(n) but n is small (~10 devices):

```c
// Worst case: ~50 CPU cycles for 10 devices
const SIT_MidiCallbackEntry* entry = SIT_GetMidiCallbackForDevice(type);
```

This is done once at device creation, not per-frame.

## Future Enhancements

### MIDI Learn

```c
// Future API:
SIT_MidiDevice_StartLearn(midi_device, control_index);
// Next CC received maps to control_index
```

### Preset Management

```c
// Future API:
SIT_MidiDevice_SavePreset(midi_device, "my_preset.json");
SIT_MidiDevice_LoadPreset(midi_device, "my_preset.json");
```

### MIDI Out (Parameter Feedback)

```c
// Future API:
SIT_MidiDevice_SendControlChange(midi_device, cc_number, cc_value);
// Updates hardware controller LEDs/motorized faders
```

## Conclusion

The MIDI Device Callbacks architecture provides:

✅ **Centralized** - All MIDI mappings in one file  
✅ **Maintainable** - Easy to add/modify mappings  
✅ **Discoverable** - Lookup table shows all MIDI devices  
✅ **Documented** - Mappings documented in code  
✅ **Performant** - Zero-overhead abstractions  
✅ **Thread-safe** - Lock-free control array access  
✅ **Extensible** - Easy to add new devices  

This is the gold standard for MIDI control in Situation.
