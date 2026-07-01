# Phase 5: Persistence, Validation, and Extensibility

**Start Date**: 2026-03-02  
**Status**: ✅ Session 1 & 2 COMPLETE (JSON Serialization 100%)  
**Estimated Time**: 2-3 days  
**Library Version**: v2.3.63 → v2.4.0-alpha

## 🎯 Goals

1. **Persistence**: Save/load audio graphs to JSON format ✅ COMPLETE
2. **Validation**: Runtime error checking and protection (Future)
3. **Extensibility**: Custom device registration API (Future)
4. **Full Demo**: Showcase all 19 devices in complex graph (Future)

## 📋 Implementation Plan

### Session 1: JSON Serialization ✅ COMPLETE

**Objective**: Implement graph save/load functionality

#### 1.1 JSON Format Design
```json
{
  "version": "2.4.0",
  "sample_rate": 48000,
  "nodes": [
    {
      "id": 1,
      "handle": "0x00010000",
      "type": "ToneSynth",
      "active": true,
      "controls": {
        "frequency": 440.0,
        "amplitude": 0.3,
        "waveform": 0
      }
    },
    {
      "id": 2,
      "handle": "0x00010001",
      "type": "Reverb",
      "active": true,
      "controls": {
        "room_size": 0.8,
        "damp": 0.5,
        "wet": 0.3,
        "dry": 0.7,
        "width": 1.0
      }
    }
  ],
  "patches": [
    {
      "id": 1,
      "src_node": 1,
      "src_port": 0,
      "dst_node": 2,
      "dst_port": 0,
      "type": "audio"
    }
  ]
}
```

#### 1.2 Implementation Files
- `sit/aud/graph_serialization.h` - Serialization API
- `sit/aud/graph_serialization_impl.h` - Implementation

#### 1.3 API Functions
```c
// Save graph to JSON file
SituationNodeError SituationSaveGraphToFile(
    const SituationAudioGraph* graph,
    const char* filepath
);

// Load graph from JSON file
SituationNodeError SituationLoadGraphFromFile(
    SituationAudioGraph* graph,
    const char* filepath,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
);

// Save to JSON string (for in-memory operations)
char* SituationSerializeGraphToJSON(const SituationAudioGraph* graph);

// Load from JSON string
SituationNodeError SituationDeserializeGraphFromJSON(
    SituationAudioGraph* graph,
    const char* json_string,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
);

// Free JSON string
void SituationFreeJSONString(char* json_string);
```

#### 1.4 JSON Library Choice
**Option 1**: Use existing lightweight JSON library (cJSON, parson, jsmn)
- **Recommendation**: Use `parson` (single header, MIT license, easy to integrate)
- Add to `ext/parson/` directory

**Option 2**: Write minimal custom JSON writer/parser
- Pros: No dependencies, full control
- Cons: More work, potential bugs

**Decision**: Use parson for reliability and speed

#### 1.5 Serialization Logic
1. Iterate through all nodes in graph
2. For each node:
   - Get device metadata from registry
   - Serialize node ID, type name, active state
   - Serialize all control values
3. Iterate through all patches
4. Write to JSON file with pretty formatting

#### 1.6 Deserialization Logic
1. Parse JSON file
2. Create nodes by type name lookup in registry
3. Restore control values
4. Create patches
5. Validate graph (cycle detection)
6. Return error if any step fails

**Deliverables**:
- ✅ JSON serialization implementation
- ✅ Save/load functions
- ✅ Error handling for missing types
- ✅ Version compatibility checking

---

### Session 2: Validation & Error Handling (0.5 day)

**Objective**: Add robust runtime validation and error reporting

#### 2.1 Error Callback System
```c
// Error callback type
typedef void (*SituationErrorCallback)(
    SituationNodeError error,
    const char* message,
    void* user_data
);

// Set global error callback
void SituationSetErrorCallback(
    SituationErrorCallback callback,
    void* user_data
);

// Get last error message
const char* SituationGetLastErrorMessage(void);
```

#### 2.2 Validation Functions
```c
// Validate node handle
bool SituationValidateNodeHandle(
    const SituationAudioGraph* graph,
    SituationNodeHandle handle
);

// Validate patch
SituationNodeError SituationValidatePatch(
    const SituationAudioGraph* graph,
    SituationNodeHandle src_node,
    int src_port,
    SituationNodeHandle dst_node,
    int dst_port,
    SituationPatchType type
);

// Validate control value
SituationNodeError SituationValidateControlValue(
    const SituationDeviceMetadata* metadata,
    int control_id,
    float value
);

// Validate entire graph
SituationNodeError SituationValidateGraph(
    const SituationAudioGraph* graph
);
```

#### 2.3 Runtime Checks
- Invalid node handles → error callback
- Out-of-range control values → clamp and warn
- Invalid patches (port out of range) → error callback
- Cycles in graph → error callback
- Missing device types on load → error callback

#### 2.4 Implementation File
- `sit/aud/graph_validation.h` - Validation API

**Deliverables**:
- ✅ Error callback system
- ✅ Validation functions
- ✅ Runtime checks in all operations
- ✅ Helpful error messages

---

### Session 3: Custom Device Registration (0.5 day)

**Objective**: Allow users to register custom audio devices

#### 3.1 Custom Device API
```c
// Register custom device type
SituationNodeError SituationRegisterCustomDevice(
    const char* name,
    SituationDeviceCategory category,
    int num_audio_ins,
    int num_audio_outs,
    const SituationControlDesc* controls,
    int num_controls,
    SituationCreateFunc create_func,
    SituationProcessFunc process_func,
    SituationDestroyFunc destroy_func
);

// Unregister custom device
SituationNodeError SituationUnregisterCustomDevice(const char* name);

// Check if device is custom
bool SituationIsCustomDevice(SituationNodeType type);
```

#### 3.2 Custom Device Example
```c
// Example: Simple gain device
typedef struct {
    float gain;
} CustomGainState;

void* CreateGain(const SituationDeviceMetadata* meta) {
    CustomGainState* state = malloc(sizeof(CustomGainState));
    state->gain = 1.0f;
    return state;
}

void ProcessGain(void* data, SituationAudioPort* ins, 
                 SituationAudioPort* outs, float* controls, int frames) {
    CustomGainState* state = (CustomGainState*)data;
    if (controls) state->gain = controls[0];
    
    for (int i = 0; i < frames * ins[0].channels; i++) {
        outs[0].buffer[i] = ins[0].buffer[i] * state->gain;
    }
}

void DestroyGain(void* data) {
    free(data);
}

// Register it
SituationControlDesc gain_control = {
    .id = 0,
    .name = "gain",
    .type = SITUATION_CONTROL_FLOAT,
    .min = 0.0f,
    .max = 2.0f,
    .default_value = 1.0f
};

SituationRegisterCustomDevice(
    "CustomGain",
    SITUATION_DEVICE_EFFECT,
    2, 2,  // stereo in/out
    &gain_control, 1,
    CreateGain, ProcessGain, DestroyGain
);
```

#### 3.3 Implementation Details
- Extend registry to support dynamic device types
- Assign custom type IDs (e.g., SITUATION_NODE_CUSTOM_BASE + index)
- Store custom device metadata separately
- Serialize custom devices with full metadata

#### 3.4 Implementation File
- `sit/aud/custom_devices.h` - Custom device API

**Deliverables**:
- ✅ Custom device registration API
- ✅ Example custom device
- ✅ Documentation for custom devices
- ✅ Serialization support for custom devices

---

### Session 4: Full Demo Application (0.5-1 day)

**Objective**: Create comprehensive demo showcasing all features

#### 4.1 Demo Features
- Create complex graph with multiple devices
- Demonstrate all 19 built-in devices
- Show custom device registration
- Save graph to JSON
- Load graph from JSON
- Real-time parameter modulation
- Audio output verification

#### 4.2 Demo Graph Design
```
[Tone Synth] ──┬──> [Filter] ──> [Overdrive] ──> [Reverb] ──┬──> [Master Out]
               │                                              │
[Mic Capture] ─┘                                              │
                                                              │
[LFO] ─────────────> [Filter.cutoff] (modulation)           │
                                                              │
[Sound Source] ──> [Chorus] ──> [Dynamics] ─────────────────┘
```

#### 4.3 Demo Application Structure
```c
// examples/full_graph_demo.c

int main() {
    // 1. Initialize audio system
    // 2. Create graph
    // 3. Add all device types
    // 4. Create patches
    // 5. Set control values
    // 6. Save to "demo_graph.json"
    // 7. Destroy graph
    // 8. Load from "demo_graph.json"
    // 9. Verify loaded graph matches
    // 10. Process audio
    // 11. Cleanup
}
```

#### 4.4 Demo Files
- `examples/full_graph_demo.c` - Main demo
- `examples/custom_device_example.c` - Custom device demo
- `examples/demo_graph.json` - Example saved graph
- `compile_full_graph_demo.bat` - Build script

**Deliverables**:
- ✅ Full demo application
- ✅ Custom device example
- ✅ Example JSON files
- ✅ Build scripts
- ✅ Demo documentation

---

## 🔧 Technical Considerations

### JSON Library Integration
- Add parson to `ext/parson/`
- Single header file: `parson.h`, `parson.c`
- MIT license compatible
- ~1000 lines of code

### Error Handling Strategy
- Use error codes for all operations
- Provide detailed error messages
- Log errors via callback
- Never crash on invalid input

### Memory Management
- JSON strings allocated with malloc
- User must free with `SituationFreeJSONString()`
- Graph owns all node/patch memory
- Custom devices manage their own state

### Version Compatibility
- Store version string in JSON
- Check version on load
- Warn if version mismatch
- Future: migration functions for old versions

### Thread Safety
- Serialization requires graph lock
- Don't serialize while processing
- Use thread-safe API if available

---

## 📊 Success Metrics

- ✅ Save/load round-trip preserves graph exactly
- ✅ All 19 devices serializable
- ✅ Custom devices work correctly
- ✅ Error handling catches all invalid operations
- ✅ Demo runs without crashes
- ✅ JSON files human-readable
- ✅ Performance: Save/load < 100ms for typical graphs

---

## 🚀 Next Steps After Phase 5

**Phase 6**: Testing & Optimization
- Fuzz testing
- Performance benchmarking
- SIMD optimization
- Documentation
- Version bump to v2.4.0

---

**Document Created**: 2026-03-02  
**Ready to Start**: Session 1 - JSON Serialization  
**Maintained By**: Kiro AI Assistant
