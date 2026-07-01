/***************************************************************************************************
*
*   sit/aud/node_graph.h - Audio Node Graph Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Internal implementation file for the node graph system.
*   
*   PUBLIC TYPES AND API: All public types, enums, and function declarations are in situation_api.h
*   This file contains ONLY implementation structures and helper functions.
*   
*   Users should never include this file directly - it's included automatically via situation_impl.h
*   
***************************************************************************************************/

#ifndef SITUATION_NODE_GRAPH_H
#define SITUATION_NODE_GRAPH_H

#include "device_registry.h"
#include <stdint.h>
#include <stdbool.h>

// NOTE: All public types (SituationNodeHandle, SituationAudioPort, SituationPatch, etc.)
// are defined in situation_api.h. This file contains only implementation structures.

// ================================================================================================
// INTERNAL NODE STRUCTURE (Implementation Detail)
// ================================================================================================

/**
 * @brief Runtime node instance (internal structure).
 * @details Represents an instantiated device in the audio graph.
 */
typedef struct SituationNode {
    // Identity
    SituationNodeType type;         // Device type from registry
    SituationNodeHandle handle;     // Generational handle
    uint16_t generation;            // Current generation (for handle validation)
    
    // Metadata reference
    const SituationDeviceMetadata* metadata;  // Pointer to registry metadata
    
    // Device-specific state
    void* device_data;              // Opaque pointer to device state (e.g., ReverbState*)
    
    // Audio ports
    SituationAudioPort* audio_inputs;   // Array of input ports
    SituationAudioPort* audio_outputs;  // Array of output ports
    
    // Control ports
    SituationControlPort* ctrl_inputs;  // Array of control input ports
    SituationControlPort* ctrl_outputs; // Array of control output ports
    
    // Control values (from metadata)
    float* control_values;          // Array of current control values
    
    // Patch connections
    SituationPatch* input_patches;  // Patches feeding this node
    int num_input_patches;
    SituationPatch* output_patches; // Patches from this node
    int num_output_patches;
    
    // Processing state
    bool is_active;                 // True if node should be processed
    bool needs_processing;          // True if inputs changed (optimization)
    
    // MIDI integration (v2.5.0)
    struct SIT_MidiDevice* midi_device;    // NULL if MIDI not enabled
    void* midi_input;                      // Hardware MIDI input stream (PmStream*, NULL if not connected)
    int midi_device_id;                    // Hardware MIDI device ID (PM_NO_DEVICE if not set)
    
    // MIDI Learn integration (v2.6.0)
    struct SIT_MidiLearnState* learn_state;  // NULL if MIDI Learn not enabled
    
} SituationNode;

// ================================================================================================
// INTERNAL GRAPH STRUCTURE (Implementation Detail)
// ================================================================================================

/**
 * @brief Audio processing graph (internal structure).
 * @details Container for all nodes and their connections.
 */
typedef struct SituationAudioGraph {
    SituationNode* nodes[SITUATION_MAX_NODES];  // Array of node pointers
    int node_count;                             // Number of active nodes
    
    SituationPatch* patches;                    // Array of all patches
    int patch_count;                            // Number of active patches
    int patch_capacity;                         // Allocated patch capacity
    
    // Topological sort cache
    SituationNode** sorted_nodes;               // Evaluation order (cached)
    int sorted_count;                           // Number of nodes in sorted list
    bool needs_resort;                          // True if topology changed
    
    // Master output (implicit)
    SituationNode* master_output;               // Hidden master output node
    
} SituationAudioGraph;

#endif // SITUATION_NODE_GRAPH_H
