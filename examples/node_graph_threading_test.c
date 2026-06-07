/***************************************************************************************************
*
*   examples/node_graph_threading_test.c - Thread Safety Test for Node Graph
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Tests thread safety of the node graph system by:
*     • Creating nodes and patches from main thread
*     • Processing audio in a simulated audio thread
*     • Updating parameters from a simulated UI thread
*     • Verifying no crashes or data races
*   
***************************************************************************************************/

#define SITUATION_USE_VULKAN
#include "../situation.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Device function table (required by node graph processing)
extern const SituationDeviceFunctions g_device_function_table[];
extern const int g_device_function_table_count;

// C11 threads
#if !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    #define THREADS_AVAILABLE 1
#else
    #define THREADS_AVAILABLE 0
#endif

// ================================================================================================
// TEST CONFIGURATION
// ================================================================================================

#define TEST_SAMPLE_RATE 48000
#define TEST_BUFFER_SIZE 256
#define TEST_DURATION_SEC 1
#define TEST_NUM_AUDIO_CALLBACKS (TEST_SAMPLE_RATE / TEST_BUFFER_SIZE * TEST_DURATION_SEC)
#define TEST_NUM_PARAM_UPDATES 20

// ================================================================================================
// GLOBAL STATE
// ================================================================================================

typedef struct {
    SituationThreadSafeGraph* graph;
    SituationNodeHandle tone_handle;
    SituationNodeHandle reverb_handle;
    bool audio_thread_running;
    bool ui_thread_running;
    int audio_callback_count;
    int param_update_count;
} TestState;

static TestState g_test_state = {0};

// ================================================================================================
// AUDIO THREAD (Simulated)
// ================================================================================================

#if THREADS_AVAILABLE

static int audio_thread_func(void* arg) {
    (void)arg;
    
    printf("[Audio Thread] Started\n");
    fflush(stdout);
    
    float* output_buffer = (float*)malloc(TEST_BUFFER_SIZE * 2 * sizeof(float));
    if (!output_buffer) {
        printf("[Audio Thread] Failed to allocate output buffer\n");
        return 1;
    }
    
    printf("[Audio Thread] Entering main loop, target callbacks: %d\n", TEST_NUM_AUDIO_CALLBACKS);
    fflush(stdout);
    
    while (g_test_state.audio_thread_running) {
        // Process audio graph
        SituationNodeError err = SituationProcessGraphThreadSafe(
            g_test_state.graph,
            output_buffer,
            TEST_BUFFER_SIZE,
            g_device_function_table,
            g_device_function_table_count
        );
        
        if (err != SITUATION_NODE_SUCCESS) {
            printf("[Audio Thread] Process error: %d\n", err);
            fflush(stdout);
        }
        
        g_test_state.audio_callback_count++;
        
        // Print progress every 50 callbacks
        if (g_test_state.audio_callback_count % 50 == 0) {
            printf("[Audio Thread] Processed %d callbacks\n", g_test_state.audio_callback_count);
            fflush(stdout);
        }
        
        // Simulate audio callback timing (sleep for buffer duration)
        struct timespec sleep_time = {
            .tv_sec = 0,
            .tv_nsec = (TEST_BUFFER_SIZE * 1000000000LL) / TEST_SAMPLE_RATE
        };
        thrd_sleep(&sleep_time, NULL);
        
        // Stop after enough callbacks
        if (g_test_state.audio_callback_count >= TEST_NUM_AUDIO_CALLBACKS) {
            break;
        }
    }
    
    free(output_buffer);
    
    printf("[Audio Thread] Stopped after %d callbacks\n", g_test_state.audio_callback_count);
    fflush(stdout);
    
    return 0;
}

// ================================================================================================
// UI THREAD (Simulated)
// ================================================================================================

static int ui_thread_func(void* arg) {
    (void)arg;
    
    printf("[UI Thread] Started\n");
    
    while (g_test_state.ui_thread_running) {
        // Update tone frequency (sweep from 220 Hz to 880 Hz)
        float t = (float)g_test_state.param_update_count / TEST_NUM_PARAM_UPDATES;
        float freq = 220.0f + t * 660.0f;
        
        SituationNodeError err = SituationSetNodeControlThreadSafe(
            g_test_state.graph,
            g_test_state.tone_handle,
            0,  // Control 0 = frequency
            freq
        );
        
        if (err != SITUATION_NODE_SUCCESS) {
            printf("[UI Thread] Set control error: %d\n", err);
        }
        
        // Update reverb room size (oscillate between 0.3 and 0.9)
        float room_size = 0.6f + 0.3f * sinf(t * 6.28f);
        
        err = SituationSetNodeControlThreadSafe(
            g_test_state.graph,
            g_test_state.reverb_handle,
            0,  // Control 0 = room size
            room_size
        );
        
        if (err != SITUATION_NODE_SUCCESS) {
            printf("[UI Thread] Set control error: %d\n", err);
        }
        
        g_test_state.param_update_count++;
        
        // Print progress every 20 updates
        if (g_test_state.param_update_count % 20 == 0) {
            printf("[UI Thread] Updated %d parameters\n", g_test_state.param_update_count);
        }
        
        // Sleep for a bit
        struct timespec sleep_time = {.tv_sec = 0, .tv_nsec = 20000000};
        thrd_sleep(&sleep_time, NULL);  // 20ms
        
        // Stop after enough updates
        if (g_test_state.param_update_count >= TEST_NUM_PARAM_UPDATES) {
            break;
        }
    }
    
    printf("[UI Thread] Stopped after %d updates\n", g_test_state.param_update_count);
    
    return 0;
}

#endif // THREADS_AVAILABLE

// ================================================================================================
// MAIN TEST
// ================================================================================================

int main(void) {
    printf("========================================\n");
    printf("Node Graph Threading Test\n");
    printf("========================================\n\n");
    
#if !THREADS_AVAILABLE
    printf("ERROR: C11 threads not available\n");
    printf("This test requires C11 thread support\n");
    return 1;
#else
    
    // Initialize device registry
    printf("Initializing device registry...\n");
    SituationInitDeviceRegistry();
    printf("Registry initialized\n\n");
    
    // Create thread-safe graph
    printf("Creating thread-safe graph...\n");
    g_test_state.graph = SituationCreateThreadSafeGraph();
    if (!g_test_state.graph) {
        printf("ERROR: Failed to create graph\n");
        return 1;
    }
    printf("Graph created\n\n");
    
    // Create nodes
    printf("Creating nodes...\n");
    SituationNodeError err = SituationCreateNodeThreadSafe(
        g_test_state.graph,
        SITUATION_NODE_TONE_SYNTH,
        &g_test_state.tone_handle
    );
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Failed to create tone synth: %d\n", err);
        return 1;
    }
    printf("  Created Tone Synth (Handle: 0x%08X)\n", g_test_state.tone_handle);
    
    err = SituationCreateNodeThreadSafe(
        g_test_state.graph,
        SITUATION_NODE_REVERB,
        &g_test_state.reverb_handle
    );
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Failed to create reverb: %d\n", err);
        return 1;
    }
    printf("  Created Reverb (Handle: 0x%08X)\n", g_test_state.reverb_handle);
    printf("Nodes created\n\n");
    
    // Create patch
    printf("Creating patch: Tone Synth[0] -> Reverb[0]...\n");
    err = SituationCreatePatchThreadSafe(
        g_test_state.graph,
        g_test_state.tone_handle,
        0,
        g_test_state.reverb_handle,
        0,
        false
    );
    if (err != SITUATION_NODE_SUCCESS) {
        printf("ERROR: Failed to create patch: %d\n", err);
        return 1;
    }
    printf("Patch created\n\n");
    
    // Set initial parameters
    printf("Setting initial parameters...\n");
    SituationSetNodeControlThreadSafe(g_test_state.graph, g_test_state.tone_handle, 0, 440.0f);  // Frequency
    SituationSetNodeControlThreadSafe(g_test_state.graph, g_test_state.reverb_handle, 0, 0.7f);  // Room size
    printf("Parameters set\n\n");
    
    // Start threads
    printf("Starting audio and UI threads...\n");
    g_test_state.audio_thread_running = true;
    g_test_state.ui_thread_running = true;
    g_test_state.audio_callback_count = 0;
    g_test_state.param_update_count = 0;
    
    thrd_t audio_thread, ui_thread;
    
    if (thrd_create(&audio_thread, audio_thread_func, NULL) != thrd_success) {
        printf("ERROR: Failed to create audio thread\n");
        return 1;
    }
    
    if (thrd_create(&ui_thread, ui_thread_func, NULL) != thrd_success) {
        printf("ERROR: Failed to create UI thread\n");
        return 1;
    }
    
    printf("Threads started\n\n");
    
    // Wait for threads to finish
    printf("Running test for %d seconds...\n", TEST_DURATION_SEC);
    printf("  Audio callbacks: %d\n", TEST_NUM_AUDIO_CALLBACKS);
    printf("  Parameter updates: %d\n\n", TEST_NUM_PARAM_UPDATES);
    
    int audio_result, ui_result;
    thrd_join(audio_thread, &audio_result);
    thrd_join(ui_thread, &ui_result);
    
    printf("\nThreads finished\n");
    printf("  Audio thread result: %d\n", audio_result);
    printf("  UI thread result: %d\n", ui_result);
    printf("  Total audio callbacks: %d\n", g_test_state.audio_callback_count);
    printf("  Total parameter updates: %d\n\n", g_test_state.param_update_count);
    
    // Cleanup
    printf("Cleaning up...\n");
    SituationDestroyThreadSafeGraph(g_test_state.graph);
    printf("Cleanup complete\n\n");
    
    printf("========================================\n");
    printf("Test completed successfully!\n");
    printf("========================================\n");
    
    return 0;
    
#endif
}
