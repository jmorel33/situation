/***************************************************************************************************
*
*   sit/aud/node_graph_process.h - Audio Node Graph Processing Loop
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Phase 4: Real-time audio processing loop for node graph evaluation.
*   
*   This file implements the audio callback integration, buffer summing/splitting, and
*   device-specific process function dispatch. Designed for lock-free operation in the
*   real-time audio thread.
*   
*   Key Features:
*     • Topological evaluation order (cached)
*     • Implicit buffer summing (multiple sources to one input)
*     • Implicit buffer splitting (one source to multiple outputs)
*     • Device-specific process function dispatch
*     • Master output node for final mix
*     • Lock-free parameter reads
*   
***************************************************************************************************/

#ifndef SITUATION_NODE_GRAPH_PROCESS_H
#define SITUATION_NODE_GRAPH_PROCESS_H

#include "node_graph.h"
#include "node_graph_impl.h"
#include "tone_synth_graph.h"
#include "midi_device.h"
#include <string.h>

static inline int _SituationNodeMidiAcceptsChannel(SituationNode* node, uint8_t channel) {
    if (!node || !node->midi_device) return 0;
    if (node->midi_device->midi_channel < 0) return 1;
    return node->midi_device->midi_channel == (int)channel;
}

void _SituationToneSynthOnPitchBend(SituationToneSynthMidiCtx* ctx, int16_t bend);

// ================================================================================================
// DEVICE PROCESS FUNCTION TYPES
// ================================================================================================

/**
 * @brief Device-specific process function signature.
 * @param device_data Opaque pointer to device state.
 * @param inputs Array of input audio ports.
 * @param outputs Array of output audio ports.
 * @param controls Array of control values.
 * @param frames Number of frames to process.
 * 
 * @details Process function must:
 *          - Read from input buffers (interleaved stereo: L, R, L, R, ...)
 *          - Write to output buffers (interleaved stereo)
 *          - Use control values for parameters
 *          - Process exactly 'frames' samples
 *          - Be real-time safe (no malloc, no locks, no syscalls)
 */
typedef void (*SituationProcessFunc)(
    void* device_data,
    SituationAudioPort* inputs,
    SituationAudioPort* outputs,
    float* controls,
    int frames
);

/**
 * @brief Device-specific create function signature.
 * @param metadata Device metadata from registry.
 * @return Opaque pointer to device state, or NULL on failure.
 * 
 * @details Create function allocates and initializes device-specific state.
 *          Called from non-real-time thread during node creation.
 */
typedef void* (*SituationCreateFunc)(const SituationDeviceMetadata* metadata);

/**
 * @brief Device-specific destroy function signature.
 * @param device_data Opaque pointer to device state.
 * 
 * @details Destroy function frees device-specific state.
 *          Called from non-real-time thread during node destruction.
 */
typedef void (*SituationDestroyFunc)(void* device_data);

// NOTE: SituationDeviceFunctions struct is defined in situation_api.h

// ================================================================================================
// BUFFER OPERATIONS
// ================================================================================================

/**
 * @brief Zero an audio buffer.
 * @param port Audio port to zero.
 * 
 * @details Sets all samples in buffer to 0.0f.
 *          Used to clear input buffers before summing.
 */
static inline void _SituationZeroBuffer(SituationAudioPort* port) {
    if (!port || !port->buffer) return;
    memset(port->buffer, 0, port->frames * port->channels * sizeof(float));
}

/**
 * @brief Sum source buffer into destination buffer.
 * @param dst Destination buffer (accumulator).
 * @param src Source buffer to add.
 * @param frames Number of frames to sum.
 * @param channels Number of channels (1 = mono, 2 = stereo).
 * 
 * @details Performs element-wise addition: dst[i] += src[i].
 *          Used for implicit summing when multiple sources connect to one input.
 */
static inline void _SituationSumBuffers(
    float* dst,
    const float* src,
    int frames,
    int channels
) {
    if (!dst || !src) return;
    
    int samples = frames * channels;
    for (int i = 0; i < samples; i++) {
        dst[i] += src[i];
    }
}

/**
 * @brief Copy source buffer to destination buffer.
 * @param dst Destination buffer.
 * @param src Source buffer.
 * @param frames Number of frames to copy.
 * @param channels Number of channels.
 * 
 * @details Performs direct copy: dst[i] = src[i].
 *          Used for implicit splitting when one source connects to multiple outputs.
 */
static inline void _SituationCopyBuffer(
    float* dst,
    const float* src,
    int frames,
    int channels
) {
    if (!dst || !src) return;
    memcpy(dst, src, frames * channels * sizeof(float));
}

// ================================================================================================
// GRAPH PROCESSING
// ================================================================================================

/**
 * @brief Process audio graph for one block.
 * @param graph Graph to process.
 * @param output_buffer Final output buffer (interleaved stereo).
 * @param frames Number of frames to process.
 * @param device_funcs Device function table.
 * @param num_device_funcs Number of entries in device function table.
 * @return Error code.
 * 
 * @details Processing steps:
 *          1. Check if topological sort is needed
 *          2. For each node in sorted order:
 *             a. Zero input buffers
 *             b. Sum all connected sources into inputs
 *             c. Apply control modulation (if any)
 *             d. Call device process function
 *             e. Output buffers now ready for next nodes
 *          3. Sum all unpatched outputs to master output buffer
 * 
 * @note This function is designed to be called from the real-time audio callback.
 *       It must be lock-free and real-time safe.
 */
SituationError SituationProcessGraph(
    SituationAudioGraph* graph,
    float* output_buffer,
    int frames,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
);

/**
 * @brief Find device functions for a given node type.
 * @param type Node type to lookup.
 * @param device_funcs Device function table.
 * @param num_device_funcs Number of entries in table.
 * @return Pointer to device functions, or NULL if not found.
 */
static inline const SituationDeviceFunctions* _SituationFindDeviceFunctions(
    SituationNodeType type,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
) {
    for (int i = 0; i < num_device_funcs; i++) {
        if (device_funcs[i].type == type) {
            return &device_funcs[i];
        }
    }
    return NULL;
}

// ================================================================================================
// IMPLEMENTATION
// ================================================================================================

SituationError SituationProcessGraph(
    SituationAudioGraph* graph,
    float* output_buffer,
    int frames,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
) {
    if (!graph) return SITUATION_ERROR_NODE_GRAPH_NOT_INITIALIZED;
    if (!output_buffer || !device_funcs) {
        return SITUATION_ERROR_NODE_PROCESSING_FAILED;
    }
    
    // Step 1: Check if topology is valid
    // NOTE: Sorting must happen on the main thread (via SituationTopologicalSort)
    // when topology changes. The audio thread must NEVER allocate memory.
    if (graph->needs_resort || graph->sorted_count == 0) {
        // Graph not sorted yet — output silence until main thread sorts it
        memset(output_buffer, 0, frames * 2 * sizeof(float));  // stereo silence
        return SITUATION_SUCCESS;
    }
    
    // Step 2: Process each node in topological order
    for (int i = 0; i < graph->sorted_count; i++) {
        SituationNode* node = graph->sorted_nodes[i];
        if (!node || !node->is_active) continue;
        
        // NEW: Process MIDI for this node (if enabled)
        if (node->midi_device && node->midi_input) {
            // Read MIDI events from hardware
            PmEvent midi_events[32];
            int midi_count = Pm_Read((PmStream*)node->midi_input, midi_events, 32);
            
            // Dispatch to MIDI device (which calls callbacks)
            for (int j = 0; j < midi_count; j++) {
                PmMessage msg = midi_events[j].message;
                uint8_t status = Pm_MessageStatus(msg);
                uint8_t data1 = Pm_MessageData1(msg);
                uint8_t data2 = Pm_MessageData2(msg);
                uint8_t channel = status & 0x0F;

                if (!_SituationNodeMidiAcceptsChannel(node, channel)) {
                    continue;
                }
                
                // Handle Control Change messages
                if ((status & 0xF0) == 0xB0) {
                    // NEW (v2.6.0): Check MIDI Learn first (if enabled)
                    if (node->learn_state) {
                        float learned_value;
                        if (SIT_MidiLearn_ProcessCC(node->learn_state, channel, 
                                                     data1, data2, &learned_value)) {
                            // Learning captured this CC or applied learned mapping
                            // Value already written to control_values by ProcessCC
                            continue;  // Skip hardcoded callback
                        }
                    }
                    
                    // Fallback to hardcoded callback (device_ptr: control array or tone synth ctx)
                    void* midi_dev = node->midi_device->device_ptr;
                    if (node->midi_device->callbacks.on_control_change) {
                        node->midi_device->callbacks.on_control_change(
                            midi_dev,
                            data1,
                            data2,
                            0
                        );
                    }
                } else if ((status & 0xF0) == 0x90) {
                    void* midi_dev = node->midi_device->device_ptr;
                    if (data2 == 0) {
                        if (node->midi_device->callbacks.on_note_off) {
                            node->midi_device->callbacks.on_note_off(
                                midi_dev, data1, 0, 0);
                        }
                    } else if (node->midi_device->callbacks.on_note_on) {
                        node->midi_device->callbacks.on_note_on(
                            midi_dev, data1, data2, 0);
                    }
                } else if ((status & 0xF0) == 0x80) {
                    void* midi_dev = node->midi_device->device_ptr;
                    if (node->midi_device->callbacks.on_note_off) {
                        node->midi_device->callbacks.on_note_off(
                            midi_dev, data1, data2, 0);
                    }
                } else if ((status & 0xF0) == 0xE0) {
                    if (node->type == SITUATION_NODE_TONE_SYNTH) {
                        uint16_t bend = (uint16_t)(data1 | (data2 << 7));
                        SituationToneSynthMidiCtx* ctx =
                            (SituationToneSynthMidiCtx*)node->midi_device->device_ptr;
                        _SituationToneSynthOnPitchBend(ctx, (int16_t)bend);
                    }
                } else if ((status & 0xF0) == 0xC0) {
                    if (node->type == SITUATION_NODE_TONE_SYNTH) {
                        SituationToneSynthMidiCtx* ctx =
                            (SituationToneSynthMidiCtx*)node->midi_device->device_ptr;
                        _SituationToneSynthOnProgramChange(ctx, data1);
                    }
                }
            }
        }
        
        // Step 2a: Zero input buffers
        for (int j = 0; j < node->metadata->num_audio_ins; j++) {
            _SituationZeroBuffer(&node->audio_inputs[j]);
        }
        
        // Step 2b: Sum all connected sources into input buffers
        for (int j = 0; j < node->num_input_patches; j++) {
            SituationPatch* patch = &node->input_patches[j];
            
            // Skip control patches (handled separately)
            if (patch->is_control) continue;
            
            // Get source node
            SituationNode* src = SituationGetNode(graph, patch->src_node);
            if (!src) continue;
            
            // Validate port indices
            if (patch->src_port < 0 || patch->src_port >= src->metadata->num_audio_outs) continue;
            if (patch->dst_port < 0 || patch->dst_port >= node->metadata->num_audio_ins) continue;
            
            // Sum source output into destination input
            SituationAudioPort* src_port = &src->audio_outputs[patch->src_port];
            SituationAudioPort* dst_port = &node->audio_inputs[patch->dst_port];
            
            _SituationSumBuffers(
                dst_port->buffer,
                src_port->buffer,
                frames,
                dst_port->channels
            );
        }
        
        // Step 2c: Apply control modulation
        for (int j = 0; j < node->num_input_patches; j++) {
            SituationPatch* patch = &node->input_patches[j];
            
            // Only process control patches
            if (!patch->is_control) continue;
            
            // Get source node
            SituationNode* src = SituationGetNode(graph, patch->src_node);
            if (!src) continue;
            
            // Validate port indices
            if (patch->src_port < 0 || patch->src_port >= src->metadata->num_ctrl_outs) continue;
            if (patch->dst_port < 0 || patch->dst_port >= node->metadata->num_ctrl_ins) continue;
            
            // Copy control value from source to destination
            node->ctrl_inputs[patch->dst_port].value = src->ctrl_outputs[patch->src_port].value;
        }
        
        // Step 2c: Clear output buffers for this block (many devices only write port 0).
        for (int j = 0; j < node->metadata->num_audio_outs; j++) {
            SituationAudioPort* out_port = &node->audio_outputs[j];
            if (out_port->buffer && out_port->channels > 0) {
                memset(out_port->buffer, 0,
                       (size_t)frames * (size_t)out_port->channels * sizeof(float));
            }
        }

        // Step 2d: Call device process function
        const SituationDeviceFunctions* funcs = _SituationFindDeviceFunctions(
            node->type,
            device_funcs,
            num_device_funcs
        );
        
        if (funcs && funcs->process) {
            funcs->process(
                node->device_data,
                node->audio_inputs,
                node->audio_outputs,
                node->control_values,
                frames
            );
        }
        
        // Mark node as processed
        node->needs_processing = false;
    }
    
    // Step 3: Sum all unpatched outputs to master output buffer
    // (For now, sum all node outputs - will be refined with explicit master output node)
    memset(output_buffer, 0, frames * 2 * sizeof(float));  // Zero master output (stereo)
    
    for (int i = 0; i < graph->sorted_count; i++) {
        SituationNode* node = graph->sorted_nodes[i];
        if (!node || !node->is_active) continue;
        
        // Sum all output ports to master
        for (int j = 0; j < node->metadata->num_audio_outs; j++) {
            SituationAudioPort* port = &node->audio_outputs[j];
            
            // Check if this output is patched to another node
            bool is_patched = false;
            for (int k = 0; k < node->num_output_patches; k++) {
                if (node->output_patches[k].src_port == j && !node->output_patches[k].is_control) {
                    is_patched = true;
                    break;
                }
            }
            
            /* Most stereo FX expose two logical outs but only fill port 0. */
            if (j > 0 && node->metadata->num_audio_outs > 1) {
                continue;
            }

            // If not patched, sum to master output
            if (!is_patched && port->buffer) {
                if (port->channels == 1) {
                    for (int f = 0; f < frames; f++) {
                        float s = port->buffer[f];
                        output_buffer[f * 2]     += s;
                        output_buffer[f * 2 + 1] += s;
                    }
                } else {
                    _SituationSumBuffers(output_buffer, port->buffer, frames, port->channels);
                }
            }
        }
    }
    
    return SITUATION_SUCCESS;
}

// ================================================================================================
// HELPER: CREATE NODE WITH DEVICE DATA
// ================================================================================================

/**
 * @brief Create node and initialize device-specific data.
 * @param graph Graph to add node to.
 * @param type Device type.
 * @param handle Output parameter for node handle.
 * @param device_funcs Device function table.
 * @param num_device_funcs Number of entries in table.
 * @return Error code.
 * 
 * @details Extended version of SituationCreateNode that also calls device create function.
 */
static inline SituationError SituationCreateNodeWithDevice(
    SituationAudioGraph* graph,
    SituationNodeType type,
    SituationNodeHandle* handle,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
) {
    // Create node structure
    SituationError err = SituationCreateNode(graph, type, handle);
    if (err != SITUATION_SUCCESS) return err;
    
    // Get node
    SituationNode* node = SituationGetNode(graph, *handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    // Find device functions
    const SituationDeviceFunctions* funcs = _SituationFindDeviceFunctions(
        type,
        device_funcs,
        num_device_funcs
    );
    
    // Call device create function
    if (funcs && funcs->create) {
        node->device_data = funcs->create(node->metadata);
        if (!node->device_data) {
            // Creation failed - destroy node
            SituationDestroyNode(graph, *handle);
            return SITUATION_ERROR_DEVICE_CREATE_FAILED;
        }
    }
    
    return SITUATION_SUCCESS;
}

/**
 * @brief Destroy node and device-specific data.
 * @param graph Graph containing node.
 * @param handle Node handle.
 * @param device_funcs Device function table.
 * @param num_device_funcs Number of entries in table.
 * @return Error code.
 * 
 * @details Extended version of SituationDestroyNode that also calls device destroy function.
 */
static inline SituationError SituationDestroyNodeWithDevice(
    SituationAudioGraph* graph,
    SituationNodeHandle handle,
    const SituationDeviceFunctions* device_funcs,
    int num_device_funcs
) {
    // Get node
    SituationNode* node = SituationGetNode(graph, handle);
    if (!node) return SITUATION_ERROR_NODE_INVALID_HANDLE;
    
    // Find device functions
    const SituationDeviceFunctions* funcs = _SituationFindDeviceFunctions(
        node->type,
        device_funcs,
        num_device_funcs
    );
    
    // Call device destroy function
    if (funcs && funcs->destroy && node->device_data) {
        funcs->destroy(node->device_data);
        node->device_data = NULL;
    }
    
    // Destroy node structure
    return SituationDestroyNode(graph, handle);
}

#endif // SITUATION_NODE_GRAPH_PROCESS_H


