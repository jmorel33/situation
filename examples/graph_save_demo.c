/***************************************************************************************************
*
*   graph_save_demo.c - Audio Graph Serialization Demo
*   (c) 2025-2026 Jacques Morel
*
*   Demonstrates saving audio graphs to JSON format.
*   Note: This demo only tests serialization, not audio processing.
*
***************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"

int main(void) {
    printf("========================================\n");
    printf("Audio Graph Serialization Demo\n");
    printf("========================================\n\n");
    
    // Initialize device registry (registers all 19 devices)
    SituationInitDeviceRegistry();
    
    printf("Registry initialized with %d devices\n\n", SituationGetRegisteredDeviceCount());
    
    // Create graph
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) {
        printf("ERROR: Failed to create graph\n");
        return 1;
    }
    
    printf("=== Creating Test Graph ===\n");
    
    // Create nodes (without device data - just for serialization test)
    SituationNodeHandle tone_synth, reverb, filter;
    
    // Tone Synth
    SituationError err = SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone_synth);
    if (err != SITUATION_SUCCESS) {
        printf("ERROR: Failed to create Tone Synth (error code %d)\n", err);
        goto cleanup;
    }
    printf("Created Tone Synth (handle: 0x%08X)\n", tone_synth);
    
    // Set tone synth controls
    SituationSetControl(graph, tone_synth, 0, 440.0f);  // frequency
    SituationSetControl(graph, tone_synth, 1, 0.3f);    // amplitude
    SituationSetControl(graph, tone_synth, 2, 0.0f);    // waveform (sine)
    
    // Filter
    err = SituationCreateNode(graph, SITUATION_NODE_FILTER, &filter);
    if (err != SITUATION_SUCCESS) {
        printf("ERROR: Failed to create Filter (error code %d)\n", err);
        goto cleanup;
    }
    printf("Created Filter (handle: 0x%08X)\n", filter);
    
    // Set filter controls
    SituationSetControl(graph, filter, 0, 0.0f);     // mode (lowpass)
    SituationSetControl(graph, filter, 1, 1000.0f);  // frequency
    SituationSetControl(graph, filter, 2, 0.7f);     // resonance
    
    // Reverb
    err = SituationCreateNode(graph, SITUATION_NODE_REVERB, &reverb);
    if (err != SITUATION_SUCCESS) {
        printf("ERROR: Failed to create Reverb (error code %d)\n", err);
        goto cleanup;
    }
    printf("Created Reverb (handle: 0x%08X)\n", reverb);
    
    // Set reverb controls
    SituationSetControl(graph, reverb, 0, 0.8f);  // room_size
    SituationSetControl(graph, reverb, 1, 0.5f);  // damp
    SituationSetControl(graph, reverb, 2, 0.3f);  // wet
    SituationSetControl(graph, reverb, 3, 0.7f);  // dry
    SituationSetControl(graph, reverb, 4, 1.0f);  // width
    
    // Create patches
    printf("\n=== Creating Patches ===\n");
    
    // Tone Synth → Filter
    err = SituationCreatePatch(graph, tone_synth, 0, filter, 0, false);
    if (err != SITUATION_SUCCESS) {
        printf("ERROR: Failed to patch Tone Synth → Filter (error code %d)\n", err);
        goto cleanup;
    }
    printf("Patched: Tone Synth[0] → Filter[0]\n");
    
    // Filter → Reverb
    err = SituationCreatePatch(graph, filter, 0, reverb, 0, false);
    if (err != SITUATION_SUCCESS) {
        printf("ERROR: Failed to patch Filter → Reverb (error code %d)\n", err);
        goto cleanup;
    }
    printf("Patched: Filter[0] → Reverb[0]\n");
    
    // Serialize to JSON
    printf("\n=== Serializing Graph to JSON ===\n");
    
    char* json = SituationSerializeGraphToJSON(graph);
    if (!json) {
        printf("ERROR: Failed to serialize graph\n");
        goto cleanup;
    }
    
    printf("JSON Output:\n");
    printf("%s\n", json);
    
    // Save to file
    printf("\n=== Saving to File ===\n");
    
    err = SituationSaveGraphToFile(graph, "demo_graph.json");
    if (err != SITUATION_SUCCESS) {
        printf("ERROR: Failed to save graph (error code %d)\n", err);
        SituationFreeJSONString(json);
        goto cleanup;
    }
    
    printf("Graph saved to: demo_graph.json\n");
    
    // Cleanup
    SituationFreeJSONString(json);
    
    printf("\n=== Success ===\n");
    printf("Graph with 3 nodes and 2 patches serialized successfully!\n");
    
cleanup:
    SituationDestroyGraph(graph);
    
    printf("\n========================================\n");
    printf("Demo Complete\n");
    printf("========================================\n");
    
    return 0;
}


