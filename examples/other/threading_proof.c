/*
 * Pure Threading Proof - No Situation, Just C11 Threads
 * 
 * This proves multi-core parallelism using only C11 threads.
 * Watch Task Manager to see all CPU cores working!
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

// Use tinycthread for C11 threads
#include "../ext/glfw/deps/tinycthread.h"

#define NUM_WORKERS 8
#define ITERATIONS_PER_JOB 100000000  // 100 million

typedef struct {
    int worker_id;
    volatile int computing;
    volatile int done;
    double result;
    double start_time;
    double end_time;
} WorkerData;

WorkerData workers[NUM_WORKERS];

// Simple timer
double get_time() {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
#endif
}

// Worker thread function
int worker_thread(void* arg) {
    WorkerData* worker = (WorkerData*)arg;
    
    worker->start_time = get_time();
    worker->computing = 1;
    
    printf("[WORKER %d] Starting...\n", worker->worker_id);
    
    // Monte Carlo pi estimation
    uint64_t inside_circle = 0;
    uint64_t seed = (uint64_t)worker->worker_id * 123456789ULL + 987654321ULL;
    
    for (uint64_t i = 0; i < ITERATIONS_PER_JOB; i++) {
        seed = (seed * 1103515245ULL + 12345ULL) & 0x7FFFFFFFULL;
        double x = (double)(seed % 10000) / 10000.0;
        
        seed = (seed * 1103515245ULL + 12345ULL) & 0x7FFFFFFFULL;
        double y = (double)(seed % 10000) / 10000.0;
        
        if (x*x + y*y <= 1.0) {
            inside_circle++;
        }
    }
    
    worker->result = 4.0 * (double)inside_circle / (double)ITERATIONS_PER_JOB;
    worker->end_time = get_time();
    worker->computing = 0;
    worker->done = 1;
    
    printf("[WORKER %d] DONE! pi ≈ %.6f (%.2f ms)\n", 
           worker->worker_id, worker->result, (worker->end_time - worker->start_time) * 1000.0);
    
    return 0;
}

int main() {
    printf("========================================\n");
    printf("  PURE THREADING PROOF\n");
    printf("========================================\n");
    printf("Computing pi using %d parallel threads\n", NUM_WORKERS);
    printf("Each thread: %d million iterations\n\n", ITERATIONS_PER_JOB / 1000000);
    printf("INSTRUCTIONS:\n");
    printf("1. Open Task Manager NOW\n");
    printf("2. Performance -> CPU\n");
    printf("3. Right-click -> Logical Processors\n");
    printf("4. Watch ALL cores light up!\n");
    printf("========================================\n\n");

    // Initialize workers
    for (int i = 0; i < NUM_WORKERS; i++) {
        workers[i].worker_id = i;
        workers[i].computing = 0;
        workers[i].done = 0;
        workers[i].result = 0.0;
    }

    printf("Starting in 3 seconds...\n");
    for (int i = 3; i > 0; i--) {
        printf("%d...\n", i);
        SLEEP_MS(1000);
    }
    
    printf("\n========================================\n");
    printf("STARTING NOW! WATCH TASK MANAGER!\n");
    printf("========================================\n\n");
    
    double start_time = get_time();
    
    // Create threads
    thrd_t threads[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (thrd_create(&threads[i], worker_thread, &workers[i]) != thrd_success) {
            printf("ERROR: Failed to create thread %d\n", i);
            return -1;
        }
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_WORKERS; i++) {
        thrd_join(threads[i], NULL);
    }
    
    double total_time = (get_time() - start_time) * 1000.0;
    
    printf("\n========================================\n");
    printf("ALL WORKERS COMPLETED!\n");
    printf("========================================\n\n");
    
    // Calculate statistics
    double avg_pi = 0.0;
    double min_time = (workers[0].end_time - workers[0].start_time) * 1000.0;
    double max_time = min_time;
    
    for (int i = 0; i < NUM_WORKERS; i++) {
        avg_pi += workers[i].result;
        double worker_time = (workers[i].end_time - workers[i].start_time) * 1000.0;
        if (worker_time < min_time) min_time = worker_time;
        if (worker_time > max_time) max_time = worker_time;
    }
    avg_pi /= NUM_WORKERS;
    
    printf("Results:\n");
    printf("  Average pi: %.6f\n", avg_pi);
    printf("  Actual pi:  %.6f\n", M_PI);
    printf("  Error:      %.6f\n\n", fabs(avg_pi - M_PI));
    
    printf("Timing:\n");
    printf("  Wall clock:  %.0f ms\n", total_time);
    printf("  Fastest:     %.0f ms\n", min_time);
    printf("  Slowest:     %.0f ms\n\n", max_time);
    
    printf("Parallelism Proof:\n");
    printf("  Single-threaded would take: ~%.0f ms\n", max_time * NUM_WORKERS);
    printf("  Actual (parallel):          %.0f ms\n", total_time);
    printf("  Speedup:                    %.2fx\n\n", (max_time * NUM_WORKERS) / total_time);
    
    printf("========================================\n");
    printf("Did you see multiple cores working?\n");
    printf("That's PROOF of multi-core parallelism!\n");
    printf("========================================\n");

    return 0;
}
