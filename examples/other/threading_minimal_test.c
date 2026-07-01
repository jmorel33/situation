/***************************************************************************************************
*
*   examples/threading_minimal_test.c - Minimal Threading Test
*   (c) 2025-2026 Jacques Morel
*
*   Absolute minimal test - no printf in audio thread, just process and count.
*   
***************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#define MINIAUDIO_IMPLEMENTATION
#include "../ext/miniaudio.h"

#include "../sit/aud/node_graph_threading_impl.h"
#include "../sit/aud/device_registry.h"
#include "../sit/aud/registry_init.h"
#include "../sit/aud/device_wrappers.h"

#if !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    #define THREADS_AVAILABLE 1
#else
    #define THREADS_AVAILABLE 0
#endif

#define TEST_ITERATIONS 10

typedef struct {
    SituationThreadSafeGraph* graph;
    SituationNodeHandle tone_handle;
    SituationNodeHandle reverb_handle;
    volatile bool running;
    volatile int counter;
} TestState;

static TestState g_state = {0};

#if THREADS_AVAILABLE

static int audio_thread(void* arg) {
    (void)arg;
    
    float* buffer = (float*)malloc(256 * 2 * sizeof(float));
    if (!buffer) return 1;
    
    while (g_state.running && g_state.counter < TEST_ITERATIONS) {
        SituationProcessGraphThreadSafe(
            g_state.graph,
            buffer,
            256,
            g_device_function_table,
            g_device_function_table_count
        );
        
        g_state.counter++;
        
        // Tiny sleep
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000}; // 1ms
        thrd_sleep(&ts, NULL);
    }
    
    free(buffer);
    return 0;
}

#endif

int main(void) {
    printf("Minimal Threading Test\n\n");
    
#if !THREADS_AVAILABLE
    printf("ERROR: Threads not available\n");
    return 1;
#else
    
    // Init
    SituationInitDeviceRegistry();
    g_state.graph = SituationCreateThreadSafeGraph();
    
    // Create nodes
    SituationCreateNodeThreadSafe(g_state.graph, SITUATION_NODE_TONE_SYNTH, &g_state.tone_handle);
    SituationCreateNodeThreadSafe(g_state.graph, SITUATION_NODE_REVERB, &g_state.reverb_handle);
    SituationCreatePatchThreadSafe(g_state.graph, g_state.tone_handle, 0, g_state.reverb_handle, 0, false);
    
    printf("Graph created, starting thread...\n");
    
    // Start thread
    g_state.running = true;
    g_state.counter = 0;
    
    thrd_t thread;
    if (thrd_create(&thread, audio_thread, NULL) != thrd_success) {
        printf("ERROR: Failed to create thread\n");
        return 1;
    }
    
    printf("Thread started, waiting...\n");
    
    // Wait for completion
    int result;
    thrd_join(thread, &result);
    
    printf("Thread finished!\n");
    printf("Processed %d iterations\n", g_state.counter);
    
    // Cleanup
    SituationDestroyThreadSafeGraph(g_state.graph);
    
    printf("\nSUCCESS!\n");
    return 0;
    
#endif
}
