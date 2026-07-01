# MIDI Integration Phase 2 - Intelligence Analysis

**Date**: March 9, 2026  
**Status**: 🔍 Analysis Complete  
**Confidence Level**: HIGH ✅

---

## Executive Summary

After thorough analysis of the codebase, I'm **confident** that Phase 2 (MIDI Learn Integration) is feasible and well-architected. The existing systems are designed to work together seamlessly.

**Recommendation**: ✅ **PROCEED** with Phase 2 implementation

---

## What We Have (Current State)

### 1. MIDI Learn System (Standalone)
**Location**: `sit/aud/midi_learn.h`

**Key Components**:
- `SIT_MidiLearnState` - Complete learning state machine
- `SIT_MidiLearnMapping` - CC → parameter mappings
- Full API (17 functions) for learning, presets, device selection
- JSON serialization (zero dependencies)
- 14-bit CC detection
- Channel filtering
- Thread-safe design

**Status**: ✅ Complete, tested, production-ready

### 2. MIDI Integration (Node Graph)
**Location**: `sit/aud/node_graph_midi.h`

**Key Components**:
- `SituationNode` has MIDI fields:
  - `SIT_MidiDevice* midi_device`
  - `void* midi_input` (PmStream*)
  - `int midi_device_id`
- Automatic MIDI processing in `SituationProcessGraph()`
- Automatic callback setup based on node type
- Automatic cleanup on node destruction

**Status**: ✅ Complete, tested, production-ready

### 3. Current MIDI Processing Flow

```c
// In SituationProcessGraph() - node_graph_process.h (Line 219)
if (node->midi_device && node->midi_input) {
    // Read MIDI events from hardware
    PmEvent midi_events[32];
    int midi_count = Pm_Read((PmStream*)node->midi_input, midi_events, 32);
    
    // Dispatch to MIDI device callbacks
    for (int j = 0; j < midi_count; j++) {
        PmMessage msg = midi_events[j].message;
        uint8_t status = Pm_MessageStatus(msg);
        uint8_t data1 = Pm_MessageData1(msg);
        uint8_t data2 = Pm_MessageData2(msg);
        
        // Handle Control Change messages
        if ((status & 0xF0) == 0xB0) {
            uint8_t channel = status & 0x0F;
            // Call hardcoded callback
            if (node->midi_device->callbacks.on_control_change) {
                node->midi_device->callbacks.on_control_change(
                    node->control_values,  // user_data = control array
                    channel,
                    data1,  // CC number
                    data2   // CC value
                );
            }
        }
    }
}
```

---

## Integration Strategy (Phase 2)

### Goal

Add MIDI Learn capability to nodes so users can:
1. Start learning a parameter: `SituationStartMidiLearn(graph, node, control_index, ...)`
2. Move a controller → CC is captured and mapped
3. Save/load custom mappings: `SituationSaveMidiPreset(graph, node, filename)`
4. Learned mappings override hardcoded mappings

### Architecture

```
SituationNode
├── midi_device (existing)
├── midi_input (existing)
├── midi_device_id (existing)
└── learn_state (NEW) ← SIT_MidiLearnState*
```

### Modified MIDI Processing Flow

```c
// In SituationProcessGraph()
if (node->midi_device && node->midi_input) {
    PmEvent midi_events[32];
    int midi_count = Pm_Read((PmStream*)node->midi_input, midi_events, 32);
    
    for (int j = 0; j < midi_count; j++) {
        // ... extract message ...
        
        if ((status & 0xF0) == 0xB0) {
            uint8_t channel = status & 0x0F;
            
            // NEW: Check MIDI Learn first
            if (node->learn_state) {
                float learned_value;
                if (SIT_MidiLearn_ProcessCC(node->learn_state, channel, 
                                             data1, data2, &learned_value)) {
                    // Learning captured this CC or applied learned mapping
                    // learned_value is already written to control_values
                    continue;  // Skip hardcoded callback
                }
            }
            
            // Fallback to hardcoded callback
            if (node->midi_device->callbacks.on_control_change) {
                node->midi_device->callbacks.on_control_change(
                    node->control_values, channel, data1, data2
                );
            }
        }
    }
}
```

---

## Implementation Plan

### Task 1: Add Learn State to SituationNode (15 min)

**File**: `sit/aud/node_graph.h`

```c
typedef struct SituationNode {
    // ... existing fields ...
    
    // MIDI integration (v2.5.0)
    struct SIT_MidiDevice* midi_device;
    void* midi_input;
    int midi_device_id;
    
    // MIDI Learn integration (v2.6.0) - NEW
    struct SIT_MidiLearnState* learn_state;  // NULL if MIDI Learn not enabled
    
} SituationNode;
```

### Task 2: Add Public API Functions (1 hour)

**File**: `sit/situation_api.h`

Add after existing MIDI functions (~line 2720):

```c
// ================================================================================================
// MIDI LEARN INTEGRATION (v2.6.0)
// ================================================================================================

/**
 * @brief Enable MIDI Learn for a node.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return SITUATION_SUCCESS on success, error code otherwise.
 * 
 * @details Enables dynamic MIDI CC learning for the node.
 *          MIDI must already be enabled via SituationEnableMidiControl().
 */
SITAPI SituationError SituationEnableMidiLearn(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);

/**
 * @brief Disable MIDI Learn for a node.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return SITUATION_SUCCESS on success, error code otherwise.
 */
SITAPI SituationError SituationDisableMidiLearn(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);

/**
 * @brief Start learning a parameter.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @param control_index Control index to learn (0-based).
 * @param param_name Human-readable parameter name.
 * @param min_value Minimum parameter value.
 * @param max_value Maximum parameter value.
 * @param scaling Scaling type (0=linear, 1=log, 2=dB, 3=discrete).
 * @return SITUATION_SUCCESS on success, error code otherwise.
 * 
 * @details Enters learn mode. The next MIDI CC received will be mapped to this parameter.
 *          Learning times out after 5 seconds if no CC is received.
 */
SITAPI SituationError SituationStartMidiLearn(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    int control_index,
    const char* param_name,
    float min_value,
    float max_value,
    int scaling
);

/**
 * @brief Cancel learning.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return SITUATION_SUCCESS on success, error code otherwise.
 */
SITAPI SituationError SituationCancelMidiLearn(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);

/**
 * @brief Save MIDI Learn preset to file.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @param filename Path to JSON file.
 * @return SITUATION_SUCCESS on success, error code otherwise.
 */
SITAPI SituationError SituationSaveMidiPreset(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    const char* filename
);

/**
 * @brief Load MIDI Learn preset from file.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @param filename Path to JSON file.
 * @return SITUATION_SUCCESS on success, error code otherwise.
 */
SITAPI SituationError SituationLoadMidiPreset(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    const char* filename
);

/**
 * @brief Clear a specific learned mapping.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @param control_index Control index to clear.
 * @return SITUATION_SUCCESS on success, error code otherwise.
 */
SITAPI SituationError SituationClearMidiMapping(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    int control_index
);

/**
 * @brief Clear all learned mappings.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return SITUATION_SUCCESS on success, error code otherwise.
 */
SITAPI SituationError SituationClearAllMidiMappings(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);

/**
 * @brief Check if MIDI Learn is enabled for a node.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return 1 if enabled, 0 otherwise.
 */
SITAPI int SituationIsMidiLearnEnabled(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);

/**
 * @brief Check if a node is currently in learn mode.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return 1 if learning, 0 otherwise.
 */
SITAPI int SituationIsLearning(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);
```

**Add error codes**:
```c
SITUATION_ERROR_MIDI_LEARN_NOT_ENABLED          = -498,  // MIDI Learn not enabled for node
SITUATION_ERROR_MIDI_LEARN_ALREADY_ENABLED      = -499,  // MIDI Learn already enabled
```

### Task 3: Implement MIDI Learn Integration (2 hours)

**File**: `sit/aud/node_graph_midi.h`

Add implementations after existing functions:

```c
// ================================================================================================
// MIDI LEARN INTEGRATION (v2.6.0)
// ================================================================================================

SituationError SituationEnableMidiLearn(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    // Check if MIDI is enabled
    if (!node->midi_device) {
        return SITUATION_ERROR_MIDI_NOT_SUPPORTED;
    }
    
    // Check if already enabled
    if (node->learn_state) {
        return SITUATION_ERROR_MIDI_LEARN_ALREADY_ENABLED;
    }
    
    // Create learn state
    node->learn_state = SIT_MidiLearn_Create();
    if (!node->learn_state) {
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    
    // Use the same MIDI device as the node
    SIT_MidiLearn_SetInputDevice(node->learn_state, node->midi_device_id);
    
    return SITUATION_SUCCESS;
}

SituationError SituationDisableMidiLearn(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (node->learn_state) {
        SIT_MidiLearn_Destroy(node->learn_state);
        node->learn_state = NULL;
    }
    
    return SITUATION_SUCCESS;
}

SituationError SituationStartMidiLearn(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    int control_index,
    const char* param_name,
    float min_value,
    float max_value,
    int scaling
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (!node->learn_state) {
        return SITUATION_ERROR_MIDI_LEARN_NOT_ENABLED;
    }
    
    SIT_MidiLearn_Start(node->learn_state, control_index, param_name,
                        min_value, max_value, (SIT_MidiScaling)scaling);
    
    return SITUATION_SUCCESS;
}

SituationError SituationCancelMidiLearn(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (!node->learn_state) {
        return SITUATION_ERROR_MIDI_LEARN_NOT_ENABLED;
    }
    
    SIT_MidiLearn_Cancel(node->learn_state);
    
    return SITUATION_SUCCESS;
}

SituationError SituationSaveMidiPreset(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    const char* filename
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    if (!filename) return SITUATION_ERROR_INVALID_PARAM;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (!node->learn_state) {
        return SITUATION_ERROR_MIDI_LEARN_NOT_ENABLED;
    }
    
    int result = SIT_MidiLearn_SavePreset(node->learn_state, filename);
    return result ? SITUATION_SUCCESS : SITUATION_ERROR_FILE_WRITE_FAILED;
}

SituationError SituationLoadMidiPreset(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    const char* filename
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    if (!filename) return SITUATION_ERROR_INVALID_PARAM;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (!node->learn_state) {
        return SITUATION_ERROR_MIDI_LEARN_NOT_ENABLED;
    }
    
    int result = SIT_MidiLearn_LoadPreset(node->learn_state, filename);
    return result ? SITUATION_SUCCESS : SITUATION_ERROR_FILE_READ_FAILED;
}

SituationError SituationClearMidiMapping(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    int control_index
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (!node->learn_state) {
        return SITUATION_ERROR_MIDI_LEARN_NOT_ENABLED;
    }
    
    SIT_MidiLearn_ClearMapping(node->learn_state, control_index);
    
    return SITUATION_SUCCESS;
}

SituationError SituationClearAllMidiMappings(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (!node->learn_state) {
        return SITUATION_ERROR_MIDI_LEARN_NOT_ENABLED;
    }
    
    SIT_MidiLearn_ClearAll(node->learn_state);
    
    return SITUATION_SUCCESS;
}

int SituationIsMidiLearnEnabled(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph) return 0;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return 0;
    
    return (node->learn_state != NULL) ? 1 : 0;
}

int SituationIsLearning(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph) return 0;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node || !node->learn_state) return 0;
    
    return node->learn_state->learning;
}
```

### Task 4: Modify MIDI Processing Loop (30 min)

**File**: `sit/aud/node_graph_process.h`

**Modify the MIDI processing section** (around line 219):

```c
// Process MIDI for this node (if enabled)
if (node->midi_device && node->midi_input) {
    PmEvent midi_events[32];
    int midi_count = Pm_Read((PmStream*)node->midi_input, midi_events, 32);
    
    for (int j = 0; j < midi_count; j++) {
        PmMessage msg = midi_events[j].message;
        uint8_t status = Pm_MessageStatus(msg);
        uint8_t data1 = Pm_MessageData1(msg);
        uint8_t data2 = Pm_MessageData2(msg);
        
        if ((status & 0xF0) == 0xB0) {
            uint8_t channel = status & 0x0F;
            
            // NEW: Check MIDI Learn first (if enabled)
            if (node->learn_state) {
                float learned_value;
                if (SIT_MidiLearn_ProcessCC(node->learn_state, channel, 
                                             data1, data2, &learned_value)) {
                    // Learning captured this CC or applied learned mapping
                    // Value already written to control_values by ProcessCC
                    continue;  // Skip hardcoded callback
                }
            }
            
            // Fallback to hardcoded callback
            if (node->midi_device->callbacks.on_control_change) {
                node->midi_device->callbacks.on_control_change(
                    node->control_values, channel, data1, data2
                );
            }
        }
    }
}
```

### Task 5: Update Node Destruction (15 min)

**File**: `sit/aud/node_graph_impl.h`

**Modify `SituationDestroyNode()`**:

```c
// Cleanup MIDI if enabled
if (node->midi_input) {
    Pm_Close((PmStream*)node->midi_input);
    node->midi_input = NULL;
}

// NEW: Cleanup MIDI Learn if enabled
if (node->learn_state) {
    SIT_MidiLearn_Destroy(node->learn_state);
    node->learn_state = NULL;
}

if (node->midi_device) {
    SIT_MidiDevice_Destroy(node->midi_device);
    node->midi_device = NULL;
}
```

### Task 6: Create Example (1 hour)

**File**: `examples/midi_learn_node_example.c`

Demonstrates:
- Creating a node
- Enabling MIDI control
- Enabling MIDI Learn
- Learning a parameter
- Saving/loading presets
- Using learned mappings

### Task 7: Update Documentation (1 hour)

**File**: `doc/midi_api.md`

Add new section: "MIDI Learn Integration with Nodes"

---

## Key Design Decisions

### 1. Learn State Ownership
**Decision**: Store `learn_state` directly in `SituationNode`  
**Rationale**: 
- Clean ownership model (node owns its learn state)
- Automatic cleanup on node destruction
- No separate tracking needed

### 2. MIDI Device Reuse
**Decision**: MIDI Learn uses the same MIDI device as the node  
**Rationale**:
- No need to open a second MIDI stream
- Consistent device selection
- Simpler API (no device parameter needed)

### 3. Processing Priority
**Decision**: Check MIDI Learn before hardcoded callbacks  
**Rationale**:
- Learned mappings override hardcoded mappings
- User customization takes precedence
- Fallback to hardcoded if no learned mapping exists

### 4. API Consistency
**Decision**: Follow existing pattern (`graph`, `handle` parameters)  
**Rationale**:
- Consistent with `SituationEnableMidiControl()` etc.
- Familiar to users
- Type-safe handles

---

## Potential Issues & Solutions

### Issue 1: MIDI Learn Requires MIDI to be Enabled First
**Impact**: Low  
**Solution**: Check `node->midi_device` in `SituationEnableMidiLearn()`  
**Error**: Return `SITUATION_ERROR_MIDI_NOT_SUPPORTED`

### Issue 2: ProcessCC Modifies control_values Directly
**Impact**: None (by design)  
**Solution**: `SIT_MidiLearn_ProcessCC()` already writes to `control_values`  
**Note**: This is the intended behavior - learned values go straight to controls

### Issue 3: Thread Safety
**Impact**: Low  
**Solution**: 
- Learn state modified from UI thread (Start/Cancel/Save/Load)
- Audio thread only reads mappings (ProcessCC)
- Single float writes are atomic on x86/x64
- NULL checks prevent crashes

### Issue 4: Timeout Checking
**Impact**: Medium  
**Solution**: Need to call `SIT_MidiLearn_Update()` periodically  
**Options**:
  1. Call in graph processing loop (adds overhead)
  2. Call from UI thread timer (recommended)
  3. Document that user must call it

**Recommendation**: Option 3 - Document that user should call `SIT_MidiLearn_Update()` from UI thread

---

## Testing Strategy

### Unit Tests
1. Enable/disable MIDI Learn
2. Start/cancel learning
3. Save/load presets
4. Clear mappings
5. Error handling (MIDI not enabled, invalid handle, etc.)

### Integration Tests
1. Learn CC → parameter
2. Learned mapping overrides hardcoded mapping
3. Multiple nodes with independent learn states
4. Preset save/load across sessions
5. Node destruction cleans up learn state

### Manual Tests
1. Real MIDI controller
2. Learn multiple parameters
3. Save preset, restart, load preset
4. Verify learned mappings work
5. Verify hardcoded mappings still work as fallback

---

## Estimated Effort

| Task | Time | Complexity |
|------|------|------------|
| 1. Add learn_state field | 15 min | Low |
| 2. Add public API | 1 hour | Low |
| 3. Implement functions | 2 hours | Medium |
| 4. Modify processing loop | 30 min | Low |
| 5. Update destruction | 15 min | Low |
| 6. Create example | 1 hour | Medium |
| 7. Update documentation | 1 hour | Low |
| **Total** | **6 hours** | **Medium** |

---

## Risk Assessment

### Low Risk ✅
- API design (follows existing patterns)
- Learn state ownership (clean model)
- MIDI device reuse (already working)
- Cleanup (straightforward)

### Medium Risk ⚠️
- Processing loop modification (critical path)
- Thread safety (need careful NULL checks)
- Timeout handling (user responsibility)

### Mitigation
- Thorough testing of processing loop
- Add NULL checks everywhere
- Clear documentation about timeout handling

---

## Conclusion

**Confidence Level**: ✅ **HIGH**

The integration is well-architected and straightforward. The existing systems (MIDI Learn and MIDI Integration) are designed to work together with minimal glue code.

**Key Strengths**:
1. Clean separation of concerns
2. Existing APIs are compatible
3. No architectural conflicts
4. Minimal code changes needed
5. Clear ownership model

**Recommendation**: ✅ **PROCEED** with Phase 2 implementation

**Estimated Time**: 6 hours (1 day)  
**Complexity**: Medium  
**Risk**: Low

---

**Next Step**: Create implementation plan and begin Task 1

