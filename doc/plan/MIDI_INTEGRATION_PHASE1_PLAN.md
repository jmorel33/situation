# MIDI Integration Phase 1 - Implementation Plan

**Date**: March 9, 2026  
**Status**: 📋 Ready to Implement  
**Estimated Time**: 1 day (8 hours)

---

## ✅ Prerequisites Check

I have everything needed to implement Phase 1:

### Code Understanding
- ✅ `SituationNode` structure location and fields (`sit/aud/node_graph.h`)
- ✅ Node creation process (`SituationCreateNode` in `sit/aud/node_graph_impl.h`)
- ✅ Node destruction process (`SituationDestroyNode`)
- ✅ Graph processing loop (`SituationProcessGraph` in `sit/aud/node_graph_process.h`)
- ✅ MIDI device infrastructure (`sit/aud/midi_device.h`)
- ✅ MIDI callback system (`sit/aud/midi_device_callbacks.h`)
- ✅ Public API structure (`sit/situation_api.h`)

### Existing Infrastructure
- ✅ MIDI device creation/destruction functions
- ✅ MIDI callback lookup table (`SIT_GetMidiCallbackForDevice`)
- ✅ MIDI processing function (`SIT_MidiDevice_ProcessAudio`)
- ✅ Device identity system
- ✅ Standardized CC mappings (just completed!)

---

## Implementation Tasks

### Task 1: Add MIDI Fields to SituationNode (30 min)

**File**: `sit/aud/node_graph.h`

**Changes**:
```c
typedef struct SituationNode {
    // ... existing fields ...
    
    // MIDI integration (v2.5.0)
    SIT_MidiDevice* midi_device;    // NULL if MIDI not enabled
    PmStream* midi_input;           // Hardware MIDI input stream (NULL if not connected)
    PmDeviceID midi_device_id;      // Hardware MIDI device ID (PM_NO_DEVICE if not set)
    
} SituationNode;
```

### Task 2: Add Public API Functions (1 hour)

**File**: `sit/situation_api.h`

**Add after node graph functions** (~line 2626):

```c
// ================================================================================================
// MIDI CONTROL INTEGRATION
// ================================================================================================

/**
 * @brief MIDI device information for device selection.
 */
typedef struct {
    PmDeviceID device_id;           // Device ID for use with SituationEnableMidiControl
    char device_name[128];          // Human-readable device name
    int is_input;                   // 1 if input device, 0 otherwise
    int is_output;                  // 1 if output device, 0 otherwise
} SituationMidiDeviceInfo;

/**
 * @brief Enable MIDI control for a node.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @param device_id Hardware MIDI device ID (or PM_NO_DEVICE for auto-select).
 * @return SITUATION_SUCCESS on success, error code otherwise.
 * 
 * @details Automatically sets up MIDI callbacks based on node type.
 *          MIDI CC messages will control the node's parameters.
 *          Use PM_NO_DEVICE to auto-select the first available MIDI input.
 */
SITAPI SituationError SituationEnableMidiControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    PmDeviceID device_id
);

/**
 * @brief Disable MIDI control for a node.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return SITUATION_SUCCESS on success, error code otherwise.
 */
SITAPI SituationError SituationDisableMidiControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);

/**
 * @brief Auto-select and connect first available MIDI input to a node.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return SITUATION_SUCCESS on success, SITUATION_ERROR_MIDI_NO_DEVICES if no devices available.
 * 
 * @details Convenience function equivalent to:
 *          SituationEnableMidiControl(graph, handle, PM_NO_DEVICE)
 */
SITAPI SituationError SituationAutoConnectMidi(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);

/**
 * @brief List available MIDI input devices.
 * @param devices Output array for device information.
 * @param max_count Maximum number of devices to return.
 * @return Number of devices found (may be less than max_count).
 * 
 * @details Use this to present a device selection UI to the user.
 */
SITAPI int SituationListMidiDevices(
    SituationMidiDeviceInfo* devices,
    int max_count
);

/**
 * @brief Check if a node has MIDI control enabled.
 * @param graph Graph containing the node.
 * @param handle Node handle.
 * @return 1 if MIDI enabled, 0 otherwise.
 */
SITAPI int SituationIsMidiEnabled(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);
```

**Add error codes** (~line 2600):
```c
typedef enum {
    // ... existing errors ...
    SITUATION_ERROR_MIDI_INIT_FAILED,
    SITUATION_ERROR_MIDI_NO_DEVICES,
    SITUATION_ERROR_MIDI_DEVICE_OPEN_FAILED,
    SITUATION_ERROR_MIDI_NOT_SUPPORTED,
} SituationError;
```

### Task 3: Implement MIDI Integration Functions (2 hours)

**File**: `sit/aud/node_graph_midi.h` (NEW)

```c
#ifndef SITUATION_NODE_GRAPH_MIDI_H
#define SITUATION_NODE_GRAPH_MIDI_H

#include "node_graph.h"
#include "midi.h"
#include "midi_device.h"
#include "midi_device_callbacks.h"

// ================================================================================================
// MIDI INTEGRATION IMPLEMENTATION
// ================================================================================================

/**
 * @brief Initialize MIDI system (called once on first use).
 */
static inline SituationError _SituationInitMidi(void) {
    static int midi_initialized = 0;
    if (!midi_initialized) {
        PmError err = Pm_Initialize();
        if (err != pmNoError) {
            return SITUATION_ERROR_MIDI_INIT_FAILED;
        }
        midi_initialized = 1;
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Auto-select first available MIDI input device.
 */
static inline PmDeviceID _SituationAutoSelectMidiInput(void) {
    int count = Pm_CountDevices();
    for (int i = 0; i < count; i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info && info->input && !info->opened) {
            return i;
        }
    }
    return PM_NO_DEVICE;
}

// ================================================================================================
// PUBLIC API IMPLEMENTATION
// ================================================================================================

SituationError SituationEnableMidiControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    PmDeviceID device_id
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    // Get node
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    // Check if MIDI already enabled
    if (node->midi_device) {
        return SITUATION_SUCCESS;  // Already enabled
    }
    
    // Initialize MIDI system
    SituationError err = _SituationInitMidi();
    if (err != SITUATION_SUCCESS) return err;
    
    // Auto-select device if needed
    if (device_id == PM_NO_DEVICE) {
        device_id = _SituationAutoSelectMidiInput();
        if (device_id == PM_NO_DEVICE) {
            return SITUATION_ERROR_MIDI_NO_DEVICES;
        }
    }
    
    // Validate device
    const PmDeviceInfo* info = Pm_GetDeviceInfo(device_id);
    if (!info || !info->input) {
        return SITUATION_ERROR_MIDI_DEVICE_OPEN_FAILED;
    }
    
    // Get MIDI callback for this device type
    const SIT_MidiCallbackEntry* callback_entry = SIT_GetMidiCallbackForDevice(node->type);
    if (!callback_entry) {
        return SITUATION_ERROR_MIDI_NOT_SUPPORTED;  // Device type doesn't support MIDI
    }
    
    // Create MIDI device
    node->midi_device = SIT_MidiDevice_Create(
        callback_entry->device_name,
        SIT_MIDI_DEVICE_EFFECT,
        SIT_MIDI_CAP_INPUT,
        node->control_values  // Pass control array as device_ptr
    );
    
    if (!node->midi_device) {
        return SITUATION_ERROR_MIDI_INIT_FAILED;
    }
    
    // Set device identity
    SIT_MidiDeviceIdentity identity = SIT_GetDeviceIdentity(node->type);
    SIT_MidiDevice_SetIdentity(node->midi_device, &identity);
    
    // Set callbacks
    SIT_MidiCallbacks callbacks = {0};
    callbacks.on_control_change = callback_entry->on_control_change;
    SIT_MidiDevice_SetCallbacks(node->midi_device, &callbacks);
    
    // Open MIDI input stream
    PmError pm_err = Pm_OpenInput(&node->midi_input, device_id, NULL, 512, NULL, NULL);
    if (pm_err != pmNoError) {
        SIT_MidiDevice_Destroy(node->midi_device);
        node->midi_device = NULL;
        return SITUATION_ERROR_MIDI_DEVICE_OPEN_FAILED;
    }
    
    node->midi_device_id = device_id;
    
    return SITUATION_SUCCESS;
}

SituationError SituationDisableMidiControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (node->midi_input) {
        Pm_Close(node->midi_input);
        node->midi_input = NULL;
    }
    
    if (node->midi_device) {
        SIT_MidiDevice_Destroy(node->midi_device);
        node->midi_device = NULL;
    }
    
    node->midi_device_id = PM_NO_DEVICE;
    
    return SITUATION_SUCCESS;
}

SituationError SituationAutoConnectMidi(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    return SituationEnableMidiControl(graph, handle, PM_NO_DEVICE);
}

int SituationListMidiDevices(
    SituationMidiDeviceInfo* devices,
    int max_count
) {
    if (!devices || max_count <= 0) return 0;
    
    // Initialize MIDI if needed
    if (_SituationInitMidi() != SITUATION_SUCCESS) return 0;
    
    int count = 0;
    int total_devices = Pm_CountDevices();
    
    for (int i = 0; i < total_devices && count < max_count; i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (!info) continue;
        
        devices[count].device_id = i;
        strncpy(devices[count].device_name, info->name, sizeof(devices[count].device_name) - 1);
        devices[count].device_name[sizeof(devices[count].device_name) - 1] = '\0';
        devices[count].is_input = info->input;
        devices[count].is_output = info->output;
        count++;
    }
    
    return count;
}

int SituationIsMidiEnabled(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph) return 0;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return 0;
    
    return (node->midi_device != NULL) ? 1 : 0;
}

#endif // SITUATION_NODE_GRAPH_MIDI_H
```

### Task 4: Integrate MIDI Processing into Graph (1 hour)

**File**: `sit/aud/node_graph_process.h`

**Modify `SituationProcessGraph` function** (around line 199):

Add MIDI processing before device processing:

```c
// Step 2: Process each node in topological order
for (int i = 0; i < graph->sorted_count; i++) {
    SituationNode* node = graph->sorted_nodes[i];
    if (!node || !node->is_active) continue;
    
    // NEW: Process MIDI for this node (if enabled)
    if (node->midi_device && node->midi_input) {
        // Read MIDI events from hardware
        PmEvent midi_events[32];
        int midi_count = Pm_Read(node->midi_input, midi_events, 32);
        
        // Dispatch to MIDI device (which calls callbacks)
        for (int j = 0; j < midi_count; j++) {
            PmMessage msg = midi_events[j].message;
            uint8_t status = Pm_MessageStatus(msg);
            uint8_t data1 = Pm_MessageData1(msg);
            uint8_t data2 = Pm_MessageData2(msg);
            
            // Handle Control Change messages
            if ((status & 0xF0) == 0xB0) {
                uint8_t channel = status & 0x0F;
                // Call callback directly (it writes to node->control_values)
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
    
    // Step 2a: Zero input buffers
    // ... rest of existing code ...
```

### Task 5: Update Node Destruction (30 min)

**File**: `sit/aud/node_graph_impl.h`

**Modify `SituationDestroyNode` function**:

Add MIDI cleanup before freeing node:

```c
SituationError SituationDestroyNode(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    // ... existing validation code ...
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    // NEW: Cleanup MIDI if enabled
    if (node->midi_input) {
        Pm_Close(node->midi_input);
        node->midi_input = NULL;
    }
    
    if (node->midi_device) {
        SIT_MidiDevice_Destroy(node->midi_device);
        node->midi_device = NULL;
    }
    
    // ... rest of existing cleanup code ...
}
```

### Task 6: Include New Header (15 min)

**File**: `sit/situation_impl.h`

Add after other audio includes:

```c
#include "aud/node_graph_midi.h"
```

### Task 7: Create Example (1 hour)

**File**: `examples/midi_auto_connect_example.c` (NEW)

```c
/*
 * MIDI Auto-Connect Example
 * 
 * Demonstrates the new integrated MIDI control system.
 * Shows how easy it is to enable MIDI control for any node.
 */

#define MINIAUDIO_IMPLEMENTATION
#include "../sit/miniaudio.h"

#define SITUATION_IMPLEMENTATION
#include "../sit/situation.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    SituationAudioGraph* graph;
    SituationNodeHandle compander;
} AppState;

static AppState g_app = {0};

static void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    (void)device;
    (void)input;
    
    if (!g_app.graph) {
        memset(output, 0, frame_count * 2 * sizeof(float));
        return;
    }
    
    // Process graph (MIDI is handled automatically!)
    SituationProcessGraph(g_app.graph, (float*)output, frame_count, NULL, 0);
}

int main(void) {
    printf("=== MIDI Auto-Connect Example ===\n\n");
    
    // Create graph
    g_app.graph = SituationCreateGraph();
    
    // Create compander node
    SituationError err = SituationCreateNode(g_app.graph, SITUATION_NODE_COMPANDER, &g_app.compander);
    if (err != SITUATION_SUCCESS) {
        printf("Failed to create compander node\n");
        return 1;
    }
    
    // Enable MIDI control (automatic!)
    printf("Enabling MIDI control...\n");
    err = SituationAutoConnectMidi(g_app.graph, g_app.compander);
    
    if (err == SITUATION_SUCCESS) {
        printf("✓ MIDI control enabled!\n\n");
        
        // List connected device
        SituationMidiDeviceInfo devices[32];
        int count = SituationListMidiDevices(devices, 32);
        printf("Available MIDI devices:\n");
        for (int i = 0; i < count; i++) {
            printf("  [%d] %s (%s)\n", i, devices[i].device_name,
                   devices[i].is_input ? "Input" : "Output");
        }
        printf("\n");
    } else if (err == SITUATION_ERROR_MIDI_NO_DEVICES) {
        printf("⚠ No MIDI devices found. Continuing without MIDI.\n\n");
    } else {
        printf("✗ Failed to enable MIDI control (error %d)\n\n", err);
    }
    
    // Setup audio
    ma_device audio_device;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = audio_callback;
    config.periodSizeInFrames = 512;
    
    if (ma_device_init(NULL, &config, &audio_device) != MA_SUCCESS) {
        printf("Failed to initialize audio device\n");
        return 1;
    }
    
    if (ma_device_start(&audio_device) != MA_SUCCESS) {
        printf("Failed to start audio device\n");
        return 1;
    }
    
    printf("Audio running. MIDI CC messages will control the compander.\n");
    printf("Press Enter to quit.\n\n");
    
    if (SituationIsMidiEnabled(g_app.graph, g_app.compander)) {
        printf("MIDI CC Mapping (Compander):\n");
        printf("  Band 0 (Low):  CC 16-23\n");
        printf("  Band 1 (Mid):  CC 24-31\n");
        printf("  Band 2 (High): CC 80-87\n\n");
    }
    
    getchar();
    
    // Cleanup (MIDI cleanup is automatic!)
    ma_device_uninit(&audio_device);
    SituationDestroyGraph(g_app.graph);
    
    printf("Cleanup complete.\n");
    return 0;
}
```

### Task 8: Create Compilation Script (15 min)

**File**: `compile_midi_auto_connect_example.bat` (NEW)

```batch
@echo off
gcc -o midi_auto_connect_example.exe examples/midi_auto_connect_example.c ^
    -I. -Isit ^
    -lwinmm -lole32 -lksuser -lmfplat -lmfuuid -lwmcodecdspuuid ^
    -O2 -Wall
if %errorlevel% equ 0 (
    echo Build successful!
    midi_auto_connect_example.exe
) else (
    echo Build failed!
)
```

### Task 9: Update Documentation (1 hour)

**File**: `doc/midi_api.md`

Add new section after "MIDI CC Reference":

```markdown
# MIDI Integration with Situation Nodes

**Version**: v2.5.0  
**Status**: ✅ Integrated

## Overview

MIDI control is now fully integrated into the Situation node graph system. Enabling MIDI for any node is as simple as calling `SituationAutoConnectMidi()`.

## Quick Start

```c
// Create node
SituationNodeHandle compander;
SituationCreateNode(graph, SITUATION_NODE_COMPANDER, &compander);

// Enable MIDI control (automatic!)
SituationAutoConnectMidi(graph, compander);

// That's it! MIDI now controls the compander.
```

## API Functions

### SituationEnableMidiControl

Enable MIDI control for a specific node with a specific MIDI device.

```c
SituationError SituationEnableMidiControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    PmDeviceID device_id
);
```

**Parameters**:
- `graph` - Graph containing the node
- `handle` - Node handle
- `device_id` - MIDI device ID (or `PM_NO_DEVICE` for auto-select)

**Returns**: `SITUATION_SUCCESS` or error code

### SituationAutoConnectMidi

Convenience function to auto-select and connect the first available MIDI input.

```c
SituationError SituationAutoConnectMidi(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);
```

### SituationListMidiDevices

List all available MIDI input/output devices.

```c
int SituationListMidiDevices(
    SituationMidiDeviceInfo* devices,
    int max_count
);
```

**Returns**: Number of devices found

### SituationDisableMidiControl

Disable MIDI control for a node.

```c
SituationError SituationDisableMidiControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);
```

### SituationIsMidiEnabled

Check if MIDI is enabled for a node.

```c
int SituationIsMidiEnabled(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
);
```

**Returns**: 1 if enabled, 0 otherwise

## Features

- **Automatic Setup**: No manual callback configuration
- **Automatic Processing**: MIDI handled in graph processing loop
- **Automatic Cleanup**: MIDI resources freed when node destroyed
- **Device Selection**: Auto-select or choose specific device
- **All Devices Supported**: Works with all 17 MIDI-enabled device types

## Error Codes

- `SITUATION_ERROR_MIDI_INIT_FAILED` - Failed to initialize MIDI system
- `SITUATION_ERROR_MIDI_NO_DEVICES` - No MIDI devices available
- `SITUATION_ERROR_MIDI_DEVICE_OPEN_FAILED` - Failed to open MIDI device
- `SITUATION_ERROR_MIDI_NOT_SUPPORTED` - Device type doesn't support MIDI

## Example

See `examples/midi_auto_connect_example.c` for a complete example.
```

---

## Testing Checklist

- [ ] Compile without errors
- [ ] Create node with MIDI enabled
- [ ] MIDI CC messages control parameters
- [ ] Multiple nodes with MIDI work independently
- [ ] Disable MIDI works correctly
- [ ] Node destruction cleans up MIDI
- [ ] No MIDI devices available handled gracefully
- [ ] Example program works

---

## Files to Create/Modify

### New Files (3)
1. `sit/aud/node_graph_midi.h` - MIDI integration implementation
2. `examples/midi_auto_connect_example.c` - Example program
3. `compile_midi_auto_connect_example.bat` - Build script

### Modified Files (6)
1. `sit/aud/node_graph.h` - Add MIDI fields to SituationNode
2. `sit/situation_api.h` - Add public API functions and error codes
3. `sit/aud/node_graph_process.h` - Add MIDI processing to graph loop
4. `sit/aud/node_graph_impl.h` - Add MIDI cleanup to node destruction
5. `sit/situation_impl.h` - Include new MIDI header
6. `doc/midi_api.md` - Add integration documentation

---

## Summary

**Total Effort**: ~8 hours (1 day)

**Breakdown**:
- Task 1: 30 min (Add fields)
- Task 2: 1 hour (Public API)
- Task 3: 2 hours (Implementation)
- Task 4: 1 hour (Graph integration)
- Task 5: 30 min (Cleanup)
- Task 6: 15 min (Include)
- Task 7: 1 hour (Example)
- Task 8: 15 min (Build script)
- Task 9: 1 hour (Documentation)

**Result**: MIDI becomes a first-class feature of Situation with a simple, clean API.

---

**Status**: ✅ Ready to implement  
**All prerequisites**: ✅ Confirmed  
**Next step**: Begin implementation
