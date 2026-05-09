/***************************************************************************************************
*
*   examples/graph_load_demo.c - Graph Load/Round-Trip Test Demo
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Demonstrates loading a saved audio graph from JSON and verifying round-trip integrity.
*   
*   This example:
*   1. Loads the graph saved by graph_save_demo.c
*   2. Verifies node count, types, and control values
*   3. Verifies patch connections
*   4. Demonstrates error handling
*   
***************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SITUATION_IMPLEMENTATION
#include "situation.h"

// ================================================================================================
// MAIN
// ================================================================================================

int main(void) {
    printf("=== Situation Audio Graph Load Demo ===\n\n");
    
    // Initialize device registry (registers all 19 devices)
    SituationInitDeviceRegistry();
    
    printf("Registered %d device types\n\n", SituationGetRegisteredDeviceCount());
    
    // Create empty graph
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) {
        fprintf(stderr, "ERROR: Failed to create graph\n");
        return 1;
    }
    
    printf("Loading graph from 'demo_graph.json'...\n");
    
    // Load graph from file
    SituationError err = SituationLoadGraphFromFile(graph, "demo_graph.json", NULL, 0);
    
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to load graph (error code %d)\n", err);
        SituationDestroyGraph(graph);
        return 1;
    }
    
    printf("SUCCESS: Graph loaded!\n\n");
    
    // ============================================================================================
    // VERIFY GRAPH CONTENTS
    // ============================================================================================
    
    printf("=== Graph Verification ===\n\n");
    
    // Count nodes
    int node_count = 0;
    for (int i = 0; i < SITUATION_MAX_NODES; i++) {
        if (graph->nodes[i] != NULL) {
            node_count++;
        }
    }
    
    printf("Node count: %d\n", node_count);
    
    if (node_count != 3) {
        fprintf(stderr, "ERROR: Expected 3 nodes, got %d\n", node_count);
        SituationDestroyGraph(graph);
        return 1;
    }
    
    // Verify nodes
    printf("\nNodes:\n");
    for (int i = 0; i < SITUATION_MAX_NODES; i++) {
        if (graph->nodes[i] != NULL) {
            SituationNode* node = graph->nodes[i];
            printf("  [%d] %s (type=%d, active=%s)\n",
                   i,
                   node->metadata->name,
                   node->type,
                   node->is_active ? "true" : "false");
            
            // Print controls
            if (node->metadata->num_controls > 0) {
                printf("      Controls:\n");
                for (int j = 0; j < node->metadata->num_controls; j++) {
                    printf("        %s = %.6f\n",
                           node->metadata->controls[j].name,
                           node->control_values[j]);
                }
            }
        }
    }
    
    // Verify patches
    printf("\nPatches: %d\n", graph->patch_count);
    
    if (graph->patch_count != 2) {
        fprintf(stderr, "ERROR: Expected 2 patches, got %d\n", graph->patch_count);
        SituationDestroyGraph(graph);
        return 1;
    }
    
    for (int i = 0; i < graph->patch_count; i++) {
        SituationPatch* patch = &graph->patches[i];
        
        uint16_t src_id = (uint16_t)(patch->src_node & 0xFFFF);
        uint16_t dst_id = (uint16_t)(patch->dst_node & 0xFFFF);
        
        SituationNode* src = graph->nodes[src_id];
        SituationNode* dst = graph->nodes[dst_id];
        
        printf("  [%d] %s:%d -> %s:%d (%s)\n",
               i,
               src ? src->metadata->name : "???",
               patch->src_port,
               dst ? dst->metadata->name : "???",
               patch->dst_port,
               patch->is_control ? "control" : "audio");
    }
    
    // ============================================================================================
    // VERIFY SPECIFIC VALUES (from graph_save_demo.c)
    // ============================================================================================
    
    printf("\n=== Value Verification ===\n\n");
    
    bool all_correct = true;
    
    // Find Tone Synth node (should be at index 0)
    SituationNode* tone_node = graph->nodes[0];
    if (tone_node && tone_node->type == SITUATION_NODE_TONE_SYNTH) {
        printf("Tone Synth found at index 0\n");
        
        // Check frequency (should be 440.0)
        float freq = tone_node->control_values[0];
        if (freq != 440.0f) {
            fprintf(stderr, "  ERROR: Frequency mismatch (expected 440.0, got %.6f)\n", freq);
            all_correct = false;
        } else {
            printf("  Frequency: %.6f ✓\n", freq);
        }
    } else {
        fprintf(stderr, "ERROR: Tone Synth not found at index 0\n");
        all_correct = false;
    }
    
    // Find Filter node (should be at index 1)
    SituationNode* filter_node = graph->nodes[1];
    if (filter_node && filter_node->type == SITUATION_NODE_FILTER) {
        printf("Filter found at index 1\n");
        
        // Check cutoff (should be 20.0)
        float cutoff = filter_node->control_values[0];
        if (cutoff != 20.0f) {
            fprintf(stderr, "  ERROR: Cutoff mismatch (expected 20.0, got %.6f)\n", cutoff);
            all_correct = false;
        } else {
            printf("  Cutoff: %.6f ✓\n", cutoff);
        }
    } else {
        fprintf(stderr, "ERROR: Filter not found at index 1\n");
        all_correct = false;
    }
    
    // Find Reverb node (should be at index 2)
    SituationNode* reverb_node = graph->nodes[2];
    if (reverb_node && reverb_node->type == SITUATION_NODE_REVERB) {
        printf("Reverb found at index 2\n");
        
        // Check room_size (should be 0.8)
        float room_size = reverb_node->control_values[0];
        if (room_size != 0.8f) {
            fprintf(stderr, "  ERROR: Room size mismatch (expected 0.8, got %.6f)\n", room_size);
            all_correct = false;
        } else {
            printf("  Room size: %.6f ✓\n", room_size);
        }
    } else {
        fprintf(stderr, "ERROR: Reverb not found at index 2\n");
        all_correct = false;
    }
    
    // ============================================================================================
    // FINAL RESULT
    // ============================================================================================
    
    printf("\n=== Round-Trip Test Result ===\n");
    
    if (all_correct) {
        printf("✓ SUCCESS: All values match!\n");
        printf("✓ Round-trip serialization working correctly\n");
    } else {
        printf("✗ FAILURE: Some values don't match\n");
    }
    
    // Cleanup
    SituationDestroyGraph(graph);
    
    printf("\nGraph destroyed. Demo complete.\n");
    
    return all_correct ? 0 : 1;
}


