/***************************************************************************************************
*
*   examples/simple_process_test.c - Simple Processing Test
*   (c) 2025-2026 Jacques Morel
*
*   Simple test to verify graph processing works before adding threading complexity.
*   
***************************************************************************************************/

#define SITUATION_USE_VULKAN
#include "../situation.h"
#include <stdio.h>
#include <stdlib.h>

// Device function table (required by node graph processing)
extern const SituationDeviceFunctions g_device_function_table[];
extern const int g_device_function_table_count;

int main(void) {
    printf("========================================\n");
    printf("Simple Processing Test\n");
    printf("========================================\n\n");
    
    // Initialize registry
    printf("Initializing device registry...\n");
    SituationInitDeviceRegistry();
    printf("Registry initialized with %d devices\n\n", SituationGetRegisteredDeviceCount());
    
    // Create graph
    printf("Creating graph...\n");
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) {
        printf("ERROR: Failed to create graph\n");
        return 1;
    }
    printf("Graph created\n\n");
    
    // Create nodes WITH device data
    printf("Creating Tone Synth node...\n");
    SituationNodeHandle tone_handle;
    SituationError err = SituationCreateNodeWithDevice(
        graph,
        SITUATION_NODE_TONE_SYNTH,
        &tone_handle,
        g_device_function_table,
        g_device_function_table_count
    );
    if (err != SITUATION_SUCCESS) {
        char* msg = NULL;
        SituationGetLastErrorMsg(&msg);
        printf("ERROR: Failed to create Tone Synth: %s\n", msg ? msg : "Unknown error");
        if (msg) SituationFreeString(msg);
        return 1;
    }
    printf("Tone Synth created (Handle: 0x%08X)\n\n", tone_handle);
    
    printf("Creating Reverb node...\n");
    SituationNodeHandle reverb_handle;
    err = SituationCreateNodeWithDevice(
        graph,
        SITUATION_NODE_REVERB,
        &reverb_handle,
        g_device_function_table,
        g_device_function_table_count
    );
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Failed to create Reverb: %s\n", SituationGetNodeErrorMessage(err));
        return 1;
    }
    printf("Reverb created (Handle: 0x%08X)\n\n", reverb_handle);
    
    // Create patch
    printf("Creating patch: Tone Synth[0] -> Reverb[0]...\n");
    err = SituationCreatePatch(graph, tone_handle, 0, reverb_handle, 0, false);
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Failed to create patch: %s\n", SituationGetNodeErrorMessage(err));
        return 1;
    }
    printf("Patch created\n\n");
    
    // Set parameters
    printf("Setting parameters...\n");
    SituationSetControl(graph, tone_handle, 0, 440.0f);  // Frequency
    SituationSetControl(graph, reverb_handle, 0, 0.7f);  // Room size
    printf("Parameters set\n\n");
    
    // Process graph multiple times
    printf("Processing graph 10 times...\n");
    float output_buffer[256 * 2];  // 256 frames, stereo
    
    for (int i = 0; i < 10; i++) {
        printf("  Processing iteration %d...\n", i);
        fflush(stdout);
        
        err = SituationProcessGraph(
            graph,
            output_buffer,
            256,
            g_device_function_table,
            g_device_function_table_count
        );
        
        if (err != SITUATION_NODE_SUCCESS) {
            printf("ERROR: Processing failed: %s\n", SituationGetNodeErrorMessage(err));
            return 1;
        }
        
        // Print first sample
        printf("    First sample: L=%.6f, R=%.6f\n", output_buffer[0], output_buffer[1]);
    }
    
    printf("\nProcessing completed successfully!\n\n");
    
    // Cleanup
    printf("Cleaning up...\n");
    SituationDestroyGraph(graph);
    printf("Cleanup complete\n\n");
    
    printf("========================================\n");
    printf("Test PASSED!\n");
    printf("========================================\n");
    
    return 0;
}
