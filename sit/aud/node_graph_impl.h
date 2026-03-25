/***************************************************************************************************
*
*   sit/aud/node_graph_impl.h - Audio Node Graph Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Implementation of the node graph system for Phase 3.
*   This file is intended to be included within the audio subsystem implementation.
*   
***************************************************************************************************/

#ifndef SITUATION_NODE_GRAPH_IMPL_H
#define SITUATION_NODE_GRAPH_IMPL_H

#include "node_graph.h"
#include <stdlib.h>
#include <string.h>

#ifndef SIT_MALLOC
#define SIT_MALLOC(s) malloc(s)
#define SIT_CALLOC(n,s) calloc(n,s)
#define SIT_FREE(p) free(p)
#endif

// Forward declarations
static bool SituationWouldCreateCycle(SituationAudioGraph* graph, SituationNodeHandle src_handle, SituationNodeHandle dst_handle);

// ================================================================================================
// HANDLE MANAGEMENT
// ================================================================================================

static inline uint16_t _SituationGetHandleIndex(SituationNodeHandle handle) {
    return (uint16_t)(handle & 0xFFFF);
}

static inline uint16_t _SituationGetHandleGeneration(SituationNodeHandle handle) {
    return (uint16_t)((handle >> 16) & 0xFFFF);
}

static inline SituationNodeHandle _SituationMakeHandle(uint16_t index, uint16_t generation) {
    return ((uint32_t)generation << 16) | (uint32_t)index;
}

// ================================================================================================
// GRAPH MANAGEMENT
// ================================================================================================

SituationAudioGraph* SituationCreateGraph(void) {
    SituationAudioGraph* graph = (SituationAudioGraph*)SIT_CALLOC(1, sizeof(SituationAudioGraph));
    if (!graph) return NULL;
    
    // Allocate patch array
    graph->patch_capacity = 256;  // Initial capacity
    graph->patches = (SituationPatch*)SIT_CALLOC(graph->patch_capacity, sizeof(SituationPatch));
    if (!graph->patches) {
        SIT_FREE(graph);
        return NULL;
    }
    
    // Allocate sorted nodes array
    graph->sorted_nodes = (SituationNode**)SIT_CALLOC(SITUATION_MAX_NODES, sizeof(SituationNode*));
    if (!graph->sorted_nodes) {
        SIT_FREE(graph->patches);
        SIT_FREE(graph);
        return NULL;
    }
    
    graph->needs_resort = true;
    
    return graph;
}

void SituationDestroyGraph(SituationAudioGraph* graph) {
    if (!graph) return;
    
    // Destroy all nodes
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i]) {
            SituationNode* node = graph->nodes[i];
            
            // Free port buffers
            if (node->audio_inputs) {
                for (int j = 0; j < node->metadata->num_audio_ins; j++) {
                    if (node->audio_inputs[j].buffer) {
                        SIT_FREE(node->audio_inputs[j].buffer);
                    }
                }
                SIT_FREE(node->audio_inputs);
            }
            
            if (node->audio_outputs) {
                for (int j = 0; j < node->metadata->num_audio_outs; j++) {
                    if (node->audio_outputs[j].buffer) {
                        SIT_FREE(node->audio_outputs[j].buffer);
                    }
                }
                SIT_FREE(node->audio_outputs);
            }
            
            if (node->ctrl_inputs) SIT_FREE(node->ctrl_inputs);
            if (node->ctrl_outputs) SIT_FREE(node->ctrl_outputs);
            if (node->control_values) SIT_FREE(node->control_values);
            if (node->input_patches) SIT_FREE(node->input_patches);
            if (node->output_patches) SIT_FREE(node->output_patches);
            
            // TODO Phase 4: Call device-specific destroy function
            // if (node->metadata->destroy_func && node->device_data) {
            //     node->metadata->destroy_func(node->device_data);
            // }
            
            SIT_FREE(node);
        }
    }
    
    SIT_FREE(graph->patches);
    SIT_FREE(graph->sorted_nodes);
    SIT_FREE(graph);
}

// ================================================================================================
// NODE MANAGEMENT
// ================================================================================================

SituationError SituationCreateNode(
    SituationAudioGraph* graph,
    SituationNodeType type,
    SituationNodeHandle* handle
) {
    if (!graph || !handle) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    // Check if graph is full
    if (graph->node_count >= SITUATION_MAX_NODES) {
        return SITUATION_ERROR_NODE_LIMIT_REACHED;
    }
    
    // Lookup device metadata in registry (get pointer to registry entry)
    const SituationDeviceMetadata* metadata = SituationGetDeviceMetadataPtr(type);
    if (!metadata) {
        return SITUATION_ERROR_NODE_TYPE_INVALID;
    }
    
    // Allocate node
    SituationNode* node = (SituationNode*)SIT_CALLOC(1, sizeof(SituationNode));
    if (!node) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    // Find free slot and assign handle
    int index = -1;
    for (int i = 0; i < SITUATION_MAX_NODES; i++) {
        if (graph->nodes[i] == NULL) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        SIT_FREE(node);
        return SITUATION_ERROR_NODE_LIMIT_REACHED;
    }
    
    // Initialize node
    node->type = type;
    node->generation = 1;  // Start at generation 1
    node->handle = _SituationMakeHandle((uint16_t)index, node->generation);
    node->metadata = metadata;  // Store pointer to registry entry (stable)
    node->is_active = true;
    node->needs_processing = true;
    
    // Allocate audio ports
    if (metadata->num_audio_ins > 0) {
        node->audio_inputs = (SituationAudioPort*)SIT_CALLOC(metadata->num_audio_ins, sizeof(SituationAudioPort));
        if (!node->audio_inputs) {
            SIT_FREE(node);
            return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
        }
        
        // Allocate buffers for each input port
        for (int i = 0; i < metadata->num_audio_ins; i++) {
            node->audio_inputs[i].channels = metadata->audio_channels;
            node->audio_inputs[i].frames = SITUATION_MAX_AUDIO_BUFFER;
            node->audio_inputs[i].buffer = (float*)SIT_CALLOC(
                SITUATION_MAX_AUDIO_BUFFER * metadata->audio_channels,
                sizeof(float)
            );
            if (!node->audio_inputs[i].buffer) {
                // Cleanup on failure
                for (int j = 0; j < i; j++) {
                    SIT_FREE(node->audio_inputs[j].buffer);
                }
                SIT_FREE(node->audio_inputs);
                SIT_FREE(node);
                return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
            }
        }
    }
    
    if (metadata->num_audio_outs > 0) {
        node->audio_outputs = (SituationAudioPort*)SIT_CALLOC(metadata->num_audio_outs, sizeof(SituationAudioPort));
        if (!node->audio_outputs) {
            // Cleanup
            if (node->audio_inputs) {
                for (int i = 0; i < metadata->num_audio_ins; i++) {
                    SIT_FREE(node->audio_inputs[i].buffer);
                }
                SIT_FREE(node->audio_inputs);
            }
            SIT_FREE(node);
            return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
        }
        
        // Allocate buffers for each output port
        for (int i = 0; i < metadata->num_audio_outs; i++) {
            node->audio_outputs[i].channels = metadata->audio_channels;
            node->audio_outputs[i].frames = SITUATION_MAX_AUDIO_BUFFER;
            node->audio_outputs[i].buffer = (float*)SIT_CALLOC(
                SITUATION_MAX_AUDIO_BUFFER * metadata->audio_channels,
                sizeof(float)
            );
            if (!node->audio_outputs[i].buffer) {
                // Cleanup on failure
                for (int j = 0; j < i; j++) {
                    SIT_FREE(node->audio_outputs[j].buffer);
                }
                SIT_FREE(node->audio_outputs);
                if (node->audio_inputs) {
                    for (int j = 0; j < metadata->num_audio_ins; j++) {
                        SIT_FREE(node->audio_inputs[j].buffer);
                    }
                    SIT_FREE(node->audio_inputs);
                }
                SIT_FREE(node);
                return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
            }
        }
    }
    
    // Allocate control ports
    if (metadata->num_ctrl_ins > 0) {
        node->ctrl_inputs = (SituationControlPort*)SIT_CALLOC(metadata->num_ctrl_ins, sizeof(SituationControlPort));
        if (!node->ctrl_inputs) {
            // Cleanup (omitted for brevity - would free all allocated resources)
            return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
        }
    }
    
    if (metadata->num_ctrl_outs > 0) {
        node->ctrl_outputs = (SituationControlPort*)SIT_CALLOC(metadata->num_ctrl_outs, sizeof(SituationControlPort));
        if (!node->ctrl_outputs) {
            // Cleanup
            return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
        }
    }
    
    // Allocate and initialize control values to defaults
    if (metadata->num_controls > 0) {
        node->control_values = (float*)SIT_CALLOC(metadata->num_controls, sizeof(float));
        if (!node->control_values) {
            // Cleanup
            return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
        }
        
        // Set default values from metadata
        for (int i = 0; i < metadata->num_controls; i++) {
            node->control_values[i] = metadata->controls[i].default_value;
        }
    }
    
    // Allocate patch arrays
    node->input_patches = (SituationPatch*)SIT_CALLOC(SITUATION_MAX_PATCHES_PER_PORT, sizeof(SituationPatch));
    node->output_patches = (SituationPatch*)SIT_CALLOC(SITUATION_MAX_PATCHES_PER_PORT, sizeof(SituationPatch));
    if (!node->input_patches || !node->output_patches) {
        // Cleanup
        return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    }
    
    // TODO Phase 4: Call device-specific create function
    // if (metadata.create_func) {
    //     node->device_data = metadata.create_func(&metadata);
    // }
    
    // Add to graph
    graph->nodes[index] = node;
    graph->node_count++;
    graph->needs_resort = true;
    
    *handle = node->handle;
    return SITUATION_SUCCESS;
}

SituationNode* SituationGetNode(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    if (!graph || handle == SITUATION_INVALID_NODE_HANDLE) return NULL;
    
    uint16_t index = _SituationGetHandleIndex(handle);
    uint16_t generation = _SituationGetHandleGeneration(handle);
    
    if (index >= SITUATION_MAX_NODES) return NULL;
    
    SituationNode* node = graph->nodes[index];
    if (!node || node->generation != generation) return NULL;
    
    return node;
}

SituationError SituationDestroyNode(
    SituationAudioGraph* graph,
    SituationNodeHandle handle
) {
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    uint16_t index = _SituationGetHandleIndex(handle);
    
    // NEW: Cleanup MIDI if enabled
    if (node->midi_input) {
        Pm_Close((PmStream*)node->midi_input);
        node->midi_input = NULL;
    }
    
    // NEW (v2.6.0): Cleanup MIDI Learn if enabled
    if (node->learn_state) {
        SIT_MidiLearn_Destroy(node->learn_state);
        node->learn_state = NULL;
    }
    
    if (node->midi_device) {
        SIT_MidiDevice_Destroy(node->midi_device);
        node->midi_device = NULL;
    }
    
    // Remove all patches involving this node
    // (Implementation would iterate through patches and remove matching ones)
    // TODO: Implement patch removal
    
    // Free resources (same as in DestroyGraph)
    // ... (omitted for brevity)
    
    // Increment generation to invalidate handle
    node->generation++;
    
    // Remove from graph
    graph->nodes[index] = NULL;
    graph->node_count--;
    graph->needs_resort = true;
    
    SIT_FREE(node);
    
    return SITUATION_SUCCESS;
}

// ================================================================================================
// PATCHING
// ================================================================================================

SituationError SituationCreatePatch(
    SituationAudioGraph* graph,
    SituationNodeHandle src_handle,
    int src_port,
    SituationNodeHandle dst_handle,
    int dst_port,
    bool is_control
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    // Get nodes
    SituationNode* src = SituationGetNode(graph, src_handle);
    SituationNode* dst = SituationGetNode(graph, dst_handle);
    
    if (!src || !dst) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    // Validate port indices
    if (is_control) {
        if (src_port < 0 || src_port >= src->metadata->num_ctrl_outs) {
            return SITUATION_ERROR_NODE_PORT_INVALID;
        }
        if (dst_port < 0 || dst_port >= dst->metadata->num_ctrl_ins) {
            return SITUATION_ERROR_NODE_PORT_INVALID;
        }
    } else {
        if (src_port < 0 || src_port >= src->metadata->num_audio_outs) {
            return SITUATION_ERROR_NODE_PORT_INVALID;
        }
        if (dst_port < 0 || dst_port >= dst->metadata->num_audio_ins) {
            return SITUATION_ERROR_NODE_PORT_INVALID;
        }
    }
    
    // Check for cycles (optional - can be disabled for feedback)
    if (SituationWouldCreateCycle(graph, src_handle, dst_handle)) {
        return SITUATION_ERROR_NODE_PATCH_CYCLE_DETECTED;
    }
    
    // Check if we have room for more patches
    if (graph->patch_count >= graph->patch_capacity) {
        // Reallocate patch array (double capacity)
        int new_capacity = graph->patch_capacity * 2;
        SituationPatch* new_patches = (SituationPatch*)SIT_CALLOC(new_capacity, sizeof(SituationPatch));
        if (!new_patches) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
        
        memcpy(new_patches, graph->patches, graph->patch_count * sizeof(SituationPatch));
        SIT_FREE(graph->patches);
        graph->patches = new_patches;
        graph->patch_capacity = new_capacity;
    }
    
    // Create patch
    SituationPatch patch = {
        .src_node = src_handle,
        .src_port = src_port,
        .dst_node = dst_handle,
        .dst_port = dst_port,
        .is_control = is_control
    };
    
    // Add to graph patch list
    graph->patches[graph->patch_count++] = patch;
    
    // Add to node patch lists
    if (src->num_output_patches < SITUATION_MAX_PATCHES_PER_PORT) {
        src->output_patches[src->num_output_patches++] = patch;
    } else {
        return SITUATION_ERROR_NODE_PATCH_ALREADY_EXISTS;
    }
    
    if (dst->num_input_patches < SITUATION_MAX_PATCHES_PER_PORT) {
        dst->input_patches[dst->num_input_patches++] = patch;
    } else {
        return SITUATION_ERROR_NODE_PATCH_ALREADY_EXISTS;
    }
    
    // Mark control port as modulated if control patch
    if (is_control) {
        dst->ctrl_inputs[dst_port].is_modulated = true;
    }
    
    // Mark graph for resort
    graph->needs_resort = true;
    
    return SITUATION_SUCCESS;
}

SituationError SituationRemovePatch(
    SituationAudioGraph* graph,
    SituationNodeHandle src_handle,
    int src_port,
    SituationNodeHandle dst_handle,
    int dst_port,
    bool is_control
) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    // Find and remove patch from graph patch list
    for (int i = 0; i < graph->patch_count; i++) {
        SituationPatch* p = &graph->patches[i];
        if (p->src_node == src_handle && p->src_port == src_port &&
            p->dst_node == dst_handle && p->dst_port == dst_port &&
            p->is_control == is_control) {
            
            // Remove by swapping with last patch
            graph->patches[i] = graph->patches[graph->patch_count - 1];
            graph->patch_count--;
            
            // TODO: Remove from node patch lists as well
            
            graph->needs_resort = true;
            return SITUATION_SUCCESS;
        }
    }
    
    return SITUATION_ERROR_NODE_PORT_INVALID;  // Patch not found
}

// ================================================================================================
// CONTROL ACCESS
// ================================================================================================

SituationError SituationSetControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    uint32_t control_id,
    float value
) {
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (control_id >= (uint32_t)node->metadata->num_controls) {
        return SITUATION_ERROR_NODE_CONTROL_INVALID;
    }
    
    // Clamp value to min/max
    const SituationControlDesc* ctrl = &node->metadata->controls[control_id];
    if (value < ctrl->min_value) value = ctrl->min_value;
    if (value > ctrl->max_value) value = ctrl->max_value;
    
    node->control_values[control_id] = value;
    node->needs_processing = true;
    
    return SITUATION_SUCCESS;
}

SituationError SituationGetControl(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    uint32_t control_id,
    float* value
) {
    if (!value) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    if (control_id >= (uint32_t)node->metadata->num_controls) {
        return SITUATION_ERROR_NODE_CONTROL_INVALID;
    }
    
    *value = node->control_values[control_id];
    return SITUATION_SUCCESS;
}

// ================================================================================================
// CYCLE DETECTION
// ================================================================================================

static bool _SituationDFSCycleCheck(
    SituationAudioGraph* graph,
    SituationNode* current,
    SituationNodeHandle target,
    bool* visited,
    bool* rec_stack
) {
    uint16_t current_idx = _SituationGetHandleIndex(current->handle);
    
    visited[current_idx] = true;
    rec_stack[current_idx] = true;
    
    // Check all output patches
    for (int i = 0; i < current->num_output_patches; i++) {
        SituationPatch* patch = &current->output_patches[i];
        
        // If we reached the target, cycle detected
        if (patch->dst_node == target) {
            return true;
        }
        
        SituationNode* next = SituationGetNode(graph, patch->dst_node);
        if (!next) continue;
        
        uint16_t next_idx = _SituationGetHandleIndex(next->handle);
        
        if (!visited[next_idx]) {
            if (_SituationDFSCycleCheck(graph, next, target, visited, rec_stack)) {
                return true;
            }
        } else if (rec_stack[next_idx]) {
            return true;
        }
    }
    
    rec_stack[current_idx] = false;
    return false;
}

bool SituationWouldCreateCycle(
    SituationAudioGraph* graph,
    SituationNodeHandle src_handle,
    SituationNodeHandle dst_handle
) {
    if (!graph) return false;
    
    SituationNode* dst = SituationGetNode(graph, dst_handle);
    if (!dst) return false;
    
    // Use DFS to check if there's a path from dst to src
    // If yes, adding src->dst would create a cycle
    bool visited[SITUATION_MAX_NODES] = {0};
    bool rec_stack[SITUATION_MAX_NODES] = {0};
    
    return _SituationDFSCycleCheck(graph, dst, src_handle, visited, rec_stack);
}

// ================================================================================================
// ERROR MESSAGES
// ================================================================================================
// TOPOLOGICAL SORT (Kahn's Algorithm)
// ================================================================================================

/**
 * @brief Perform topological sort on graph nodes using Kahn's algorithm.
 * @details Computes evaluation order for audio processing. Source nodes (no inputs)
 *          are processed first, followed by their dependents in topological order.
 *          Result is cached in graph->sorted_nodes until topology changes.
 * 
 * @param graph Graph to sort.
 * @return Error code.
 * 
 * @note Kahn's Algorithm:
 *       1. Find all nodes with in-degree 0 (sources)
 *       2. Add them to queue
 *       3. While queue not empty:
 *          - Remove node from queue, add to sorted list
 *          - For each output patch, decrement destination in-degree
 *          - If destination in-degree becomes 0, add to queue
 *       4. If sorted count != node count, graph has cycle (error)
 */
SituationError SituationTopologicalSort(SituationAudioGraph* graph) {
    if (!graph) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    // Allocate temporary in-degree array
    int* in_degree = (int*)SIT_CALLOC(SITUATION_MAX_NODES, sizeof(int));
    if (!in_degree) return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    
    // Allocate queue for nodes with in-degree 0
    SituationNode** queue = (SituationNode**)SIT_CALLOC(SITUATION_MAX_NODES, sizeof(SituationNode*));
    if (!queue) {
        SIT_FREE(in_degree);
        return SITUATION_ERROR_NODE_ALLOCATION_FAILED;
    }
    
    int queue_head = 0;
    int queue_tail = 0;
    
    // Step 1: Calculate in-degree for each node (count incoming audio patches only)
    for (int i = 0; i < SITUATION_MAX_NODES; i++) {
        if (graph->nodes[i]) {
            SituationNode* node = graph->nodes[i];
            uint16_t idx = _SituationGetHandleIndex(node->handle);
            
            // Count incoming audio patches (not control patches)
            for (int j = 0; j < node->num_input_patches; j++) {
                if (!node->input_patches[j].is_control) {
                    in_degree[idx]++;
                }
            }
        }
    }
    
    // Step 2: Find all source nodes (in-degree 0) and add to queue
    for (int i = 0; i < SITUATION_MAX_NODES; i++) {
        if (graph->nodes[i]) {
            uint16_t idx = _SituationGetHandleIndex(graph->nodes[i]->handle);
            if (in_degree[idx] == 0) {
                queue[queue_tail++] = graph->nodes[i];
            }
        }
    }
    
    // Step 3: Process queue (Kahn's algorithm)
    int sorted_count = 0;
    
    while (queue_head < queue_tail) {
        // Dequeue node
        SituationNode* node = queue[queue_head++];
        
        // Add to sorted list
        graph->sorted_nodes[sorted_count++] = node;
        
        // Process all output patches
        for (int i = 0; i < node->num_output_patches; i++) {
            SituationPatch* patch = &node->output_patches[i];
            
            // Skip control patches (don't affect evaluation order)
            if (patch->is_control) continue;
            
            // Get destination node
            SituationNode* dst = SituationGetNode(graph, patch->dst_node);
            if (!dst) continue;
            
            uint16_t dst_idx = _SituationGetHandleIndex(dst->handle);
            
            // Decrement in-degree
            in_degree[dst_idx]--;
            
            // If in-degree becomes 0, add to queue
            if (in_degree[dst_idx] == 0) {
                queue[queue_tail++] = dst;
            }
        }
    }
    
    // Step 4: Check for cycles
    if (sorted_count != graph->node_count) {
        // Graph has cycle (some nodes not processed)
        SIT_FREE(in_degree);
        SIT_FREE(queue);
        return SITUATION_ERROR_NODE_PATCH_CYCLE_DETECTED;
    }
    
    // Success - update graph state
    graph->sorted_count = sorted_count;
    graph->needs_resort = false;
    
    // Cleanup
    SIT_FREE(in_degree);
    SIT_FREE(queue);
    
    return SITUATION_SUCCESS;
}

#endif // SITUATION_NODE_GRAPH_IMPL_H




