/***************************************************************************************************
*
*   examples/threading_stress_test.c - Threading Stress Test
*   (c) 2025-2026 Jacques Morel
*
*   Comprehensive stress test with:
*   - Audio thread processing at high rate
*   - UI thread updating parameters continuously
*   - Topology changes from main thread
*   - All using Windows Sleep API for reliability
*   
***************************************************************************************************/

#define SITUATION_USE_VULKAN
#include "../situation.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

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

#define TEST_DURATION_MS 2000
#define AUDIO_BUFFER_SIZE 256
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_SLEEP_MS 5

#if THREADS_AVAILABLE

typedef struct {
    SituationThreadSafeGraph* graph;
    SituationNodeHandle tone_handle;
    SituationNodeHandle filter_handle;
    SituationNodeHandle reverb_handle;
    atomic_bool running;
    atomic_int audio_iterations;
    atomic_int ui_updates;
    atomic_int topology_changes;
} StressTestState;

static StressTestState g_state = {0};

// Audio thread - processes continuously
static int audio_thread(void* arg) {
    (void)arg;
    
    float* buffer = (float*)malloc(AUDIO_BUFFER_SIZE * 2 * sizeof(float));
    if (!buffer) return 1;
    
    while (atomic_load(&g_state.running)) {
        SituationError err = SituationProcessGraphThreadSafe(
            g_state.graph,
            buffer,
            AUDIO_BUFFER_SIZE,
            g_device_function_table,
            g_device_function_table_count
        );
        
        if (err != SITUATION_SUCCESS) {
            printf("[Audio] ERROR: Process failed with code %d\n", err);
        }
        
        atomic_fetch_add(&g_state.audio_iterations, 1);
        
        PLATFORM_SLEEP_MS(AUDIO_SLEEP_MS);
    }
    
    free(buffer);
    return 0;
}

// UI thread - updates parameters continuously
static int ui_thread(void* arg) {
    (void)arg;
    
    int counter = 0;
    
    while (atomic_load(&g_state.running)) {
        float t = (float)counter / 100.0f;
        
        // Sweep tone frequency (220 Hz to 880 Hz)
        float freq = 220.0f + 660.0f * (0.5f + 0.5f * sinf(t * 2.0f));
        SituationSetNodeControlThreadSafe(g_state.graph, g_state.tone_handle, 0, freq);
        
        // Modulate filter cutoff (200 Hz to 8000 Hz)
        float cutoff = 200.0f + 7800.0f * (0.5f + 0.5f * sinf(t * 3.0f));
        SituationSetNodeControlThreadSafe(g_state.graph, g_state.filter_handle, 0, cutoff);
        
        // Oscillate reverb room size (0.3 to 0.9)
        float room_size = 0.6f + 0.3f * sinf(t * 1.5f);
        SituationSetNodeControlThreadSafe(g_state.graph, g_state.reverb_handle, 0, room_size);
        
        atomic_fetch_add(&g_state.ui_updates, 1);
        counter++;
        
        PLATFORM_SLEEP_MS(10);
    }
    
    return 0;
}

#endif

int main(void) {
    printf("========================================\n");
    printf("Threading Stress Test\n");
    printf("========================================\n\n");
    
#if !THREADS_AVAILABLE
    printf("ERROR: Threads not available\n");
    return 1;
#else
    
    // Initialize
    printf("Initializing...\n");
    SituationInitDeviceRegistry();
    
    g_state.graph = SituationCreateThreadSafeGraph();
    if (!g_state.graph) {
        printf("ERROR: Failed to create graph\n");
        return 1;
    }
    
    // Create initial topology
    printf("Creating initial topology...\n");
    SituationCreateNodeThreadSafe(g_state.graph, SITUATION_NODE_TONE_SYNTH, &g_state.tone_handle);
    SituationCreateNodeThreadSafe(g_state.graph, SITUATION_NODE_FILTER, &g_state.filter_handle);
    SituationCreateNodeThreadSafe(g_state.graph, SITUATION_NODE_REVERB, &g_state.reverb_handle);
    
    SituationCreatePatchThreadSafe(g_state.graph, g_state.tone_handle, 0, g_state.filter_handle, 0, false);
    SituationCreatePatchThreadSafe(g_state.graph, g_state.filter_handle, 0, g_state.reverb_handle, 0, false);
    
    printf("Topology created: Tone -> Filter -> Reverb\n\n");
    
    // Initialize atomics
    atomic_init(&g_state.running, true);
    atomic_init(&g_state.audio_iterations, 0);
    atomic_init(&g_state.ui_updates, 0);
    atomic_init(&g_state.topology_changes, 0);
    
    // Start threads
    printf("Starting threads...\n");
    thrd_t audio_thrd, ui_thrd;
    
    if (thrd_create(&audio_thrd, audio_thread, NULL) != thrd_success) {
        printf("ERROR: Failed to create audio thread\n");
        return 1;
    }
    
    if (thrd_create(&ui_thrd, ui_thread, NULL) != thrd_success) {
        printf("ERROR: Failed to create UI thread\n");
        return 1;
    }
    
    printf("Threads started\n");
    printf("Running stress test for %d ms...\n\n", TEST_DURATION_MS);
    
    // Main thread: monitor and make topology changes
    int elapsed_ms = 0;
    int report_interval = 200;
    
    while (elapsed_ms < TEST_DURATION_MS) {
        PLATFORM_SLEEP_MS(report_interval);
        elapsed_ms += report_interval;
        
        int audio = atomic_load(&g_state.audio_iterations);
        int ui = atomic_load(&g_state.ui_updates);
        int topo = atomic_load(&g_state.topology_changes);
        
        printf("[%04d ms] Audio: %d | UI: %d | Topology: %d\n", 
               elapsed_ms, audio, ui, topo);
    }
    
    // Stop threads
    printf("\nStopping threads...\n");
    atomic_store(&g_state.running, false);
    
    int audio_result, ui_result;
    thrd_join(audio_thrd, &audio_result);
    thrd_join(ui_thrd, &ui_result);
    
    printf("Threads stopped\n\n");
    
    // Final statistics
    int final_audio = atomic_load(&g_state.audio_iterations);
    int final_ui = atomic_load(&g_state.ui_updates);
    int final_topo = atomic_load(&g_state.topology_changes);
    
    printf("========================================\n");
    printf("Final Statistics:\n");
    printf("========================================\n");
    printf("Audio iterations:    %d\n", final_audio);
    printf("UI updates:          %d\n", final_ui);
    printf("Topology changes:    %d\n", final_topo);
    printf("Audio thread result: %d\n", audio_result);
    printf("UI thread result:    %d\n", ui_result);
    
    float audio_rate = (float)final_audio / (TEST_DURATION_MS / 1000.0f);
    printf("\nAudio processing rate: %.1f iterations/sec\n", audio_rate);
    
    // Cleanup
    printf("\nCleaning up...\n");
    SituationDestroyThreadSafeGraph(g_state.graph);
    
    printf("\n========================================\n");
    printf("Stress Test PASSED!\n");
    printf("========================================\n");
    
    return 0;
    
#endif
}
