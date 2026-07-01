/*
 * Threading Visual Proof
 * 
 * Runs 4 threads in INFINITE LOOPS doing heavy CPU work.
 * Each thread continuously calculates pi - never stops.
 * 
 * Watch Task Manager -> Performance -> CPU (Logical Processors view)
 * You should see 4 cores at 100% usage!
 */

#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER
#define SITUATION_USE_SHARED
#include "situation.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>

#define NUM_THREADS 4

typedef struct {
    int thread_id;
    volatile bool active;
    volatile uint64_t iterations_completed;
    volatile double result;
} ThreadData;

ThreadData threads[NUM_THREADS];

// CPU-intensive worker - does ONE batch then returns
void compute_worker_batch(void* data, void* ctx) {
    (void)ctx;
    ThreadData* td = (ThreadData*)data;
    
    if (!td->active) return;
    
    uint64_t inside_circle = 0;
    uint64_t seed = (uint64_t)td->thread_id * 123456789ULL + td->iterations_completed;
    
    // ONE batch of 100 million iterations
    for (uint64_t i = 0; i < 100000000; i++) {
        seed = (seed * 1103515245ULL + 12345ULL) & 0x7FFFFFFFULL;
        double x = (double)(seed % 10000) / 10000.0;
        
        seed = (seed * 1103515245ULL + 12345ULL) & 0x7FFFFFFFULL;
        double y = (double)(seed % 10000) / 10000.0;
        
        if (x*x + y*y <= 1.0) {
            inside_circle++;
        }
    }
    
    td->iterations_completed += 100000000;
    td->result = 4.0 * (double)inside_circle / 100000000.0;
}

int main(int argc, char** argv) {
    SituationInitInfo init_info = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Threading Proof - Watch Task Manager!",
        .initial_active_window_flags = 0,  // No VSync - run as fast as possible
        .enable_vulkan_validation = false
    };

    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        return -1;
    }

    printf("========================================\n");
    printf("  THREADING VISUAL PROOF\n");
    printf("========================================\n");
    printf("Running %d threads continuously.\n", NUM_THREADS);
    printf("Each thread does 1 BILLION iterations per batch.\n\n");
    printf("INSTRUCTIONS:\n");
    printf("1. Open Task Manager NOW\n");
    printf("2. Performance -> CPU\n");
    printf("3. Right-click -> Logical Processors\n");
    printf("4. Watch %d cores working hard!\n", NUM_THREADS);
    printf("5. Press ESC to stop\n");
    printf("========================================\n\n");

    // Create thread pool
    SituationThreadPool pool;
    if (SituationCreateThreadPool(&pool, NUM_THREADS, 256, 0.0, true) != SITUATION_SUCCESS) {
        printf("Failed to create thread pool!\n");
        return -1;
    }

    // Initialize threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].thread_id = i;
        threads[i].active = true;
        threads[i].iterations_completed = 0;
        threads[i].result = 0.0;
    }

    printf("Starting compute jobs on %d threads...\n", NUM_THREADS);
    printf("Smart job submission - only submit when workers are ready!\n");
    fflush(stdout);

    // Track active jobs per thread
    SituationJobId active_jobs[NUM_THREADS] = {0};
    
    // Submit initial batch
    for (int i = 0; i < NUM_THREADS; i++) {
        active_jobs[i] = SituationSubmitJob(&pool, compute_worker_batch, &threads[i]);
    }

    int frame = 0;
    double last_print = SituationTimerGetTime();
    int jobs_submitted = NUM_THREADS;

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        // Exit on ESC
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            printf("ESC pressed - stopping...\n");
            for (int i = 0; i < NUM_THREADS; i++) {
                threads[i].active = false;
            }
            break;
        }

        // Check each thread's job - if complete, submit new one immediately
        for (int i = 0; i < NUM_THREADS; i++) {
            if (threads[i].active && active_jobs[i] != 0) {
                // Non-blocking check if job is done
                if (SituationWaitForJob(&pool, active_jobs[i]) == SITUATION_SUCCESS) {
                    // Job completed! Submit new one immediately
                    active_jobs[i] = SituationSubmitJob(&pool, compute_worker_batch, &threads[i]);
                    jobs_submitted++;
                }
            }
        }

        // Print stats every 2 seconds
        double now = SituationTimerGetTime();
        if (now - last_print >= 2.0) {
            printf("Thread stats (jobs submitted: %d):\n", jobs_submitted);
            for (int i = 0; i < NUM_THREADS; i++) {
                printf("  [%d] pi=%.6f, iterations=%llu\n", 
                       i, threads[i].result, 
                       (unsigned long long)threads[i].iterations_completed);
            }
            printf("\n");
            fflush(stdout);
            last_print = now;
        }

        // Small sleep to prevent busy-waiting
        Sleep(1);  // 1ms

        frame++;
    }

    SituationDestroyThreadPool(&pool);
    SituationShutdown();
    return 0;
}
