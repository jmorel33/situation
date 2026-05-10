/***************************************************************************************************
*
*   sit/aud/node_graph_midi.h - MIDI Integration for Node Graph
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Phase 1: Core MIDI integration into Situation node graph system.
*   
*   This file implements the MIDI control API that makes MIDI a first-class feature of the
*   Situation library. Nodes can now have MIDI control enabled with a single function call.
*   
*   Key Features:
*     • Automatic MIDI device selection
*     • Automatic callback setup based on node type
*     • Automatic MIDI processing in graph loop
*     • Automatic cleanup on node destruction
*     • Device enumeration for UI
*   
***************************************************************************************************/

#ifndef SITUATION_NODE_GRAPH_MIDI_H
#define SITUATION_NODE_GRAPH_MIDI_H

#include "node_graph.h"
#include "midi.h"
#include "midi_device.h"
#include "midi_device_callbacks.h"
#include "midi_learn.h"
#include <string.h>

// ================================================================================================
// MIDI INITIALIZATION
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
static inline int _SituationAutoSelectMidiInput(void) {
    int count = Pm_CountDevices();
    for (int i = 0; i < count; i++) {
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info && info->input && !info->opened) {
            return i;
        }
    }
    return -1;  // PM_NO_DEVICE equivalent
}

// ================================================================================================
// PUBLIC API IMPLEMENTATION
// ================================================================================================

SituationError SituationEnableMidiControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    int device_id
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
    if (device_id == -1) {
        device_id = _SituationAutoSelectMidiInput();
        if (device_id == -1) {
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
    
    // Create MIDI device (processor + identity + callbacks; virtual input opened here — closed in Disable/Destroy)
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
    PmStream* midi_input = NULL;
    PmError pm_err = Pm_OpenInput(&midi_input, device_id, NULL, 512, NULL, NULL);
    if (pm_err != pmNoError) {
        SIT_MidiDevice_Destroy(node->midi_device);
        node->midi_device = NULL;
        return SITUATION_ERROR_MIDI_DEVICE_OPEN_FAILED;
    }
    
    node->midi_input = midi_input;
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
        Pm_Close((PmStream*)node->midi_input);
        node->midi_input = NULL;
    }
    
    if (node->midi_device) {
        SIT_MidiDevice_Destroy(node->midi_device);
        node->midi_device = NULL;
    }
    
    node->midi_device_id = -1;
    
    return SITUATION_SUCCESS;
}

SituationError SituationAutoConnectMidi(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    return SituationEnableMidiControl(graph, handle, -1);
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

#endif // SITUATION_NODE_GRAPH_MIDI_H
