/***************************************************************************************************
*
*   examples/threading_raw.c - Raw Threading Test (Windows Sleep API)
*   (c) 2025-2026 Jacques Morel
*
*   Ultra-minimal test using Windows Sleep() instead of tinycthread's thrd_sleep.
*   Tests if the hang is caused by tinycthread implementation.
*   
***************************************************************************************************/

#define SITUATION_USE_VULKAN
#include "../situation.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Device function table (required by node graph processing)
extern const SituationDeviceFunctions g_device_function_table[];
extern const int g_device_function_table_count;

#if defined(_WIN32)
    #include <windows.h>
    #define PLATFORM_SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define PLATFORM_SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#if !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    #include <stdatomic.h>
    #define THREADS_AVAILABLE 1
#else
    #define THREADS_AVAILABLE 0
#endif

#define TEST_ITERATIONS 10

#if THREADS_AVAILABLE

typedef struct {
    SituationThreadSafeGraph* graph;
    SituationNodeHandle tone_handle;
    SituationNodeHandle reverb_handle;
    atomic_bool running;
    atomic_int counter;
} TestState;

static TestState g_state = {0};

static int audio_thread(void* arg) {
    (void)arg;
    
    float* buffer = (float*)malloc(256 * 2 * sizeof(float));
    if (!buffer) return 1;
    
    printf("[Audio] Thread started\n");
    fflush(stdout);
    
    while (atomic_load(&g_state.running)) {
        int current = atomic_load(&g_state.counter);
        
        if (current >= TEST_ITERATIONS) {
            break;
        }
        
        // Process
        SituationProcessGraphThreadSafe(
            g_state.graph,
            buffer,
            256,
            g_device_function_table,
            g_device_function_table_count
        );
        
        // Increment
        int new_count = atomic_fetch_add(&g_state.counter, 1) + 1;
        
        printf("[Audio] Iteration %d complete\n", new_count);
        fflush(stdout);
        
        // Use Windows Sleep instead of thrd_sleep
        PLATFORM_SLEEP_MS(5);
    }
    
    free(buffer);
    
    printf("[Audio] Thread exiting\n");
    fflush(stdout);
    
    return 0;
}

#endif

int main(void) {
    printf("========================================\n");
    printf("Raw Threading Test (Windows Sleep API)\n");
    printf("========================================\n\n");
    
#if !THREADS_AVAILABLE
    printf("ERROR: Threads not available\n");
    return 1;
#else
    
    // Init
    printf("[Main] Initializing registry...\n");
    SituationInitDeviceRegistry();
    
    printf("[Main] Creating graph...\n");
    g_state.graph = SituationCreateThreadSafeGraph();
    if (!g_state.graph) {
        printf("ERROR: Failed to create graph\n");
        return 1;
    }
    
    // Create nodes
    printf("[Main] Creating nodes...\n");
    SituationCreateNodeThreadSafe(g_state.graph, SITUATION_NODE_TONE_SYNTH, &g_state.tone_handle);
    SituationCreateNodeThreadSafe(g_state.graph, SITUATION_NODE_REVERB, &g_state.reverb_handle);
    SituationCreatePatchThreadSafe(g_state.graph, g_state.tone_handle, 0, g_state.reverb_handle, 0, false);
    
    printf("[Main] Graph ready, starting thread...\n");
    fflush(stdout);
    
    // Initialize atomics
    atomic_init(&g_state.running, true);
    atomic_init(&g_state.counter, 0);
    
    // Start thread
    thrd_t thread;
    if (thrd_create(&thread, audio_thread, NULL) != thrd_success) {
        printf("ERROR: Failed to create thread\n");
        return 1;
    }
    
    printf("[Main] Thread started, waiting for completion...\n");
    fflush(stdout);
    
    // Wait for completion
    int result;
    thrd_join(thread, &result);
    
    printf("[Main] Thread joined with result: %d\n", result);
    printf("[Main] Total iterations: %d\n", atomic_load(&g_state.counter));
    
    // Cleanup
    printf("[Main] Cleaning up...\n");
    SituationDestroyThreadSafeGraph(g_state.graph);
    
    printf("\n========================================\n");
    printf("Test COMPLETED!\n");
    printf("========================================\n");
    
    return 0;
    
#endif
}
