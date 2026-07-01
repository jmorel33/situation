/*******************************************************************************
*
*   node_graph_demo.c - Audio Node Graph Demo
*   
*   Demonstrates the node graph API with a simple audio processing chain:
*   Tone Synth → Reverb → Master Output
*
*******************************************************************************/

#define SITUATION_USE_VULKAN
#include "situation.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Device function table (required by node graph processing)
extern const SituationDeviceFunctions g_device_function_table[];
extern const int g_device_function_table_count;

// ============================================================================
// DEMO FUNCTIONS
// ============================================================================

void PrintNodeInfo(SituationNode* node) {
    if (!node || !node->metadata) return;
    
    printf("  Node: %s (Type: %d)\n", node->metadata->name, node->type);
    printf("    Handle: 0x%08X\n", node->handle);
    printf("    Audio Ins: %d, Outs: %d\n", 
           node->metadata->num_audio_ins, 
           node->metadata->num_audio_outs);
    printf("    Control Ins: %d, Outs: %d\n",
           node->metadata->num_ctrl_ins,
           node->metadata->num_ctrl_outs);
    printf("    Controls: %d\n", node->metadata->num_controls);
    printf("    Active: %s\n", node->is_active ? "Yes" : "No");
}

void PrintGraphInfo(SituationAudioGraph* graph) {
    if (!graph) return;
    
    printf("\n=== Graph Info ===\n");
    printf("Node Count: %d\n", graph->node_count);
    printf("Patch Count: %d\n", graph->patch_count);
    printf("Needs Resort: %s\n", graph->needs_resort ? "Yes" : "No");
    printf("Sorted Count: %d\n", graph->sorted_count);
    
    printf("\nNodes:\n");
    for (int i = 0; i < SITUATION_MAX_NODES; i++) {
        if (graph->nodes[i]) {
            PrintNodeInfo(graph->nodes[i]);
        }
    }
    
    printf("\nPatches:\n");
    for (int i = 0; i < graph->patch_count; i++) {
        SituationPatch* p = &graph->patches[i];
        printf("  Patch %d: Node 0x%08X[%d] → Node 0x%08X[%d] (%s)\n",
               i,
               p->src_node, p->src_port,
               p->dst_node, p->dst_port,
               p->is_control ? "Control" : "Audio");
    }
    
    if (graph->sorted_count > 0) {
        printf("\nEvaluation Order:\n");
        for (int i = 0; i < graph->sorted_count; i++) {
            if (graph->sorted_nodes[i]) {
                printf("  %d. %s\n", i + 1, graph->sorted_nodes[i]->metadata->name);
            }
        }
    }
}

// ============================================================================
// MAIN DEMO
// ============================================================================

int main(void) {
    printf("========================================\n");
    printf("Audio Node Graph Demo\n");
    printf("========================================\n\n");
    
    // Initialize device registry
    printf("Initializing device registry...\n");
    SituationInitDeviceRegistry();
    printf("Registry initialized with %d devices\n\n", SituationGetRegisteredDeviceCount());
    
    // Create graph
    printf("Creating audio graph...\n");
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) {
        printf("ERROR: Failed to create graph\n");
        return 1;
    }
    printf("Graph created successfully\n\n");
    
    // Create nodes
    printf("Creating nodes...\n");
    
    SituationNodeHandle tone_handle;
    SituationNodeError err = SituationCreateNodeWithDevice(
        graph,
        SITUATION_NODE_TONE_SYNTH,
        &tone_handle,
        g_device_function_table,
        g_device_function_table_count
    );
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Failed to create Tone Synth node: %s\n", 
               SituationGetNodeErrorMessage(err));
        SituationDestroyGraph(graph);
        return 1;
    }
    printf("  Created Tone Synth (Handle: 0x%08X)\n", tone_handle);
    
    SituationNodeHandle reverb_handle;
    err = SituationCreateNodeWithDevice(
        graph,
        SITUATION_NODE_REVERB,
        &reverb_handle,
        g_device_function_table,
        g_device_function_table_count
    );
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Failed to create Reverb node: %s\n",
               SituationGetNodeErrorMessage(err));
        SituationDestroyGraph(graph);
        return 1;
    }
    printf("  Created Reverb (Handle: 0x%08X)\n", reverb_handle);
    
    printf("Nodes created successfully\n\n");
    
    // Create patch: Tone Synth output → Reverb input
    printf("Creating patch: Tone Synth[0] → Reverb[0]...\n");
    err = SituationCreatePatch(graph, tone_handle, 0, reverb_handle, 0, false);
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Failed to create patch: %s\n",
               SituationGetNodeErrorMessage(err));
        SituationDestroyGraph(graph);
        return 1;
    }
    printf("Patch created successfully\n\n");
    
    // Set control parameters
    printf("Setting control parameters...\n");
    
    // Set Tone Synth frequency to 440 Hz (A4)
    err = SituationSetControl(graph, tone_handle, 0, 440.0f);
    if (err != SITUATION_NODE_SUCCESS) {
        printf("WARNING: Failed to set Tone Synth frequency: %s\n",
               SituationGetNodeErrorMessage(err));
    } else {
        printf("  Tone Synth frequency = 440 Hz\n");
    }
    
    // Set Reverb room size
    err = SituationSetControl(graph, reverb_handle, 0, 0.7f);
    if (err != SITUATION_NODE_SUCCESS) {
        printf("WARNING: Failed to set Reverb room size: %s\n",
               SituationGetNodeErrorMessage(err));
    } else {
        printf("  Reverb room size = 0.7\n");
    }
    
    printf("Control parameters set\n\n");
    
    // Print graph info
    PrintGraphInfo(graph);
    
    // Test topological sort
    printf("\n=== Testing Topological Sort ===\n");
    err = SituationTopologicalSort(graph);
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Topological sort failed: %s\n",
               SituationGetNodeErrorMessage(err));
    } else {
        printf("Topological sort successful\n");
        printf("Evaluation order:\n");
        for (int i = 0; i < graph->sorted_count; i++) {
            printf("  %d. %s\n", i + 1, graph->sorted_nodes[i]->metadata->name);
        }
    }
    
    // Test cycle detection
    printf("\n=== Testing Cycle Detection ===\n");
    printf("Attempting to create cycle: Reverb[0] → Tone Synth[0]...\n");
    err = SituationCreatePatch(graph, reverb_handle, 0, tone_handle, 0, false);
    if (err == SITUATION_NODE_ERR_CYCLE_DETECTED) {
        printf("SUCCESS: Cycle detected and prevented\n");
    } else if (err == SITUATION_NODE_SUCCESS) {
        printf("WARNING: Cycle was not detected (this is a bug!)\n");
    } else {
        printf("ERROR: Unexpected error: %s\n", SituationGetNodeErrorMessage(err));
    }
    
    // Test graph processing (with real devices)
    printf("\n=== Testing Graph Processing ===\n");
    float output_buffer[2048 * 2];  // Stereo buffer
    err = SituationProcessGraph(graph, output_buffer, 512, g_device_function_table, g_device_function_table_count);
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Graph processing failed: %s\n",
               SituationGetNodeErrorMessage(err));
    } else {
        printf("Graph processing successful\n");
        printf("Output buffer first 10 samples:\n");
        for (int i = 0; i < 10; i++) {
            printf("  [%d] L: %.3f, R: %.3f\n", i, output_buffer[i*2], output_buffer[i*2+1]);
        }
    }
    
    // Cleanup
    printf("\n=== Cleanup ===\n");
    printf("Destroying graph...\n");
    SituationDestroyGraph(graph);
    printf("Graph destroyed\n");
    
    printf("\n========================================\n");
    printf("Demo completed successfully!\n");
    printf("========================================\n");
    
    return 0;
}
