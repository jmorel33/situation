# MIDI Integration Analysis

**Date**: March 9, 2026  
**Status**: 🔍 Analysis  
**Priority**: HIGH

---

## Executive Summary

You're correct - the MIDI system is **NOT well integrated** with the Situation library. The MIDI infrastructure exists as a **separate, parallel system** with no direct connection to the node graph architecture.

---

## Current State

### What Exists

1. **MIDI Infrastructure** (`sit/aud/midi.h`, `sit/aud/midi_device.h`):
   - Low-level MIDI streams and routing
   - Virtual MIDI devices
   - Sample-accurate timing
   - Device callbacks

2. **MIDI Callbacks** (`sit/aud/midi_device_callbacks.h`):
   - Centralized CC → parameter mappings
   - 17 device types with callbacks
   - Normalization helpers

3. **Situation Node Graph** (`sit/aud/node_graph.h`):
   - Audio processing nodes
   - Control values array
   - Device metadata
   - **NO MIDI INTEGRATION**

### What's Missing

The `SituationNode` structure has:
```c
typedef struct SituationNode {
    SituationNodeType type;
    void* device_data;
    float* control_values;  // ← MIDI should write here
    // ... audio ports, patches, etc.
    
    // ❌ NO MIDI DEVICE FIELD
    // ❌ NO MIDI INTEGRATION
} SituationNode;
```

---

## The Gap

### Current Manual Integration (from examples)

Users must manually:

1. **Create MIDI device separately**:
```c
SIT_MidiDevice* midi = SIT_MidiDevice_Create("Compander", 
                                              SIT_MIDI_DEVICE_EFFECT,
                                              SIT_MIDI_CAP_INPUT,
                                              node);
```

2. **Set up callbacks manually**:
```c
SIT_MidiCallbacks callbacks = {0};
callbacks.on_control_change = _SituationCompanderOnControlChange;
callbacks.user_data = node->controls;  // ← Where are node->controls?
SIT_MidiDevice_SetCallbacks(midi, &callbacks);
```

3. **Process MIDI manually in audio callback**:
```c
PmEvent events[32];
int count = Pm_Read(midi_in, events, 32);
for (int i = 0; i < count; i++) {
    // Manually dispatch to device
    SIT_MidiDevice_ProcessControlChange(...);
}
```

4. **Store MIDI device somewhere** (not in node structure):
```c
// User must track this themselves!
compander_node->midi_device = midi;  // ← This field doesn't exist!
```

### Problems

1. **No automatic MIDI setup**: Users must manually create and configure MIDI devices
2. **No node-MIDI binding**: `SituationNode` has no `midi_device` field
3. **No control array access**: Callbacks expect `float* controls`, but nodes have `float* control_values`
4. **Manual MIDI processing**: Users must call `Pm_Read()` and dispatch in audio callback
5. **No cleanup integration**: Users must manually destroy MIDI devices
6. **No API functions**: No `SituationEnableMidiControl()` or similar

---

## What Should Exist

### Ideal Integration

```c
// In sit/aud/node_graph.h
typedef struct SituationNode {
    // ... existing fields ...
    
    // MIDI integration
    SIT_MidiDevice* midi_device;    // NULL if MIDI not enabled
    PmStream* midi_input;           // Hardware MIDI input (if connected)
} SituationNode;
```

### Ideal API

```c
// In sit/situation_api.h

/**
 * @brief Enable MIDI control for a node.
 * @param node Node to enable MIDI for.
 * @param device_id Hardware MIDI device ID (or PM_NO_DEVICE for auto-select).
 * @return 0 on success, -1 on error.
 */
SITAPI int SituationEnableMidiControl(SituationNode* node, PmDeviceID device_id);

/**
 * @brief Disable MIDI control for a node.
 * @param node Node to disable MIDI for.
 */
SITAPI void SituationDisableMidiControl(SituationNode* node);

/**
 * @brief Auto-select and connect first available MIDI input.
 * @param node Node to connect MIDI to.
 * @return 0 on success, -1 if no MIDI devices available.
 */
SITAPI int SituationAutoConnectMidi(SituationNode* node);

/**
 * @brief List available MIDI input devices.
 * @param devices Output array for device info.
 * @param max_count Maximum number of devices to return.
 * @return Number of devices found.
 */
SITAPI int SituationListMidiDevices(SituationMidiDeviceInfo* devices, int max_count);
```

### Ideal Usage

```c
// Create node
SituationNode* compander = SituationCreateNode(graph, SITUATION_NODE_COMPANDER);

// Enable MIDI control (automatic!)
SituationAutoConnectMidi(compander);

// That's it! MIDI now controls the compander automatically.
// No manual setup, no callbacks, no processing loop.
```

---

## Integration Levels

### Level 0: Current State (Manual)
- ❌ User creates MIDI device manually
- ❌ User sets up callbacks manually
- ❌ User processes MIDI manually
- ❌ User manages cleanup manually

### Level 1: Basic Integration (Recommended)
- ✅ Add `midi_device` field to `SituationNode`
- ✅ Add `SituationEnableMidiControl()` API
- ✅ Automatic callback setup (lookup from `midi_device_callbacks.h`)
- ✅ Automatic MIDI processing in graph processing
- ✅ Automatic cleanup when node destroyed

### Level 2: Advanced Integration (Future)
- ✅ MIDI Learn built-in
- ✅ Preset save/load
- ✅ Multi-device support
- ✅ MIDI routing matrix UI

---

## Recommended Implementation Plan

### Phase 1: Core Integration (1 day)

1. **Add MIDI fields to `SituationNode`**:
```c
typedef struct SituationNode {
    // ... existing fields ...
    SIT_MidiDevice* midi_device;
    PmStream* midi_input;
} SituationNode;
```

2. **Add public API functions** in `sit/situation_api.h`:
   - `SituationEnableMidiControl()`
   - `SituationDisableMidiControl()`
   - `SituationAutoConnectMidi()`
   - `SituationListMidiDevices()`

3. **Implement automatic callback setup**:
   - Use `SIT_GetMidiCallbackForDevice()` from `midi_device_callbacks.h`
   - Automatically connect `control_values` array to callbacks

4. **Integrate MIDI processing into graph**:
   - Process MIDI in `SituationProcessGraph()`
   - Call `SIT_MidiDevice_ProcessAudio()` for each node with MIDI enabled

5. **Add cleanup**:
   - Destroy MIDI device when node destroyed

### Phase 2: MIDI Learn Integration (2-3 days)

After core integration is complete, add MIDI Learn:
- `SituationStartMidiLearn(node, control_index)`
- `SituationCancelMidiLearn(node)`
- `SituationSaveMidiPreset(node, filename)`
- `SituationLoadMidiPreset(node, filename)`

---

## Benefits of Integration

### For Users
- **Simple API**: One function call to enable MIDI
- **Automatic setup**: No manual callback configuration
- **Automatic processing**: MIDI handled in graph processing
- **Automatic cleanup**: No memory leaks
- **Consistent behavior**: All devices work the same way

### For Developers
- **Cleaner code**: No boilerplate in examples
- **Better architecture**: MIDI is part of the node system
- **Easier testing**: Can test MIDI without manual setup
- **Future-proof**: Foundation for MIDI Learn and presets

---

## Example: Before vs After

### Before (Current - Manual)

```c
// Create node
SituationNode* compander = SituationCreateNode(graph, SITUATION_NODE_COMPANDER);

// Manually create MIDI device
SIT_MidiDevice* midi = SIT_MidiDevice_Create("Compander", 
                                              SIT_MIDI_DEVICE_EFFECT,
                                              SIT_MIDI_CAP_INPUT,
                                              compander);

// Manually set up callbacks
const SIT_MidiCallbackEntry* entry = SIT_GetMidiCallbackForDevice(SITUATION_NODE_COMPANDER);
SIT_MidiCallbacks callbacks = {0};
callbacks.on_control_change = entry->on_control_change;
callbacks.user_data = ???;  // Where do we get control_values?
SIT_MidiDevice_SetCallbacks(midi, &callbacks);

// Manually find and open MIDI input
int device_count = Pm_CountDevices();
PmDeviceID input_device = -1;
for (int i = 0; i < device_count; i++) {
    const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
    if (info->input) {
        input_device = i;
        break;
    }
}
PmStream* midi_in;
Pm_OpenInput(&midi_in, input_device, NULL, 512, NULL, NULL);

// Manually process MIDI in audio callback
void audio_callback(...) {
    PmEvent events[32];
    int count = Pm_Read(midi_in, events, 32);
    for (int i = 0; i < count; i++) {
        // Manually dispatch...
    }
    SituationProcessGraph(graph, output, frame_count);
}

// Manually cleanup
Pm_Close(midi_in);
SIT_MidiDevice_Destroy(midi);
```

### After (Proposed - Automatic)

```c
// Create node
SituationNode* compander = SituationCreateNode(graph, SITUATION_NODE_COMPANDER);

// Enable MIDI control (automatic!)
SituationAutoConnectMidi(compander);

// That's it! MIDI processing happens automatically in SituationProcessGraph()

// Cleanup is automatic when node is destroyed
```

---

## Conclusion

The MIDI system is well-designed but **completely disconnected** from the Situation node graph. Integration is needed to make MIDI a first-class feature.

**Recommendation**: Implement Phase 1 (Core Integration) before proceeding with MIDI Learn. This will provide a solid foundation and make MIDI Learn implementation much cleaner.

**Estimated Effort**:
- Phase 1 (Core Integration): 1 day
- Phase 2 (MIDI Learn): 2-3 days
- **Total**: 3-4 days

---

## Questions for Discussion

1. Should MIDI be enabled by default for all nodes, or opt-in?
2. Should we support multiple MIDI inputs per node?
3. Should MIDI processing be in the audio thread or a separate thread?
4. How should we handle MIDI device hotplug?
5. Should we expose low-level MIDI API or keep it internal?

---

**Status**: Awaiting decision on integration approach  
**Next Step**: Implement Phase 1 (Core Integration) or proceed with standalone MIDI Learn?
