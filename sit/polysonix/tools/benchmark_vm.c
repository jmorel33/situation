#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define PX_BENCHMARK_NATIVE_WAVES
#define POLYSONIX_IMPLEMENTATION
#include "../polysonix.h"

// Bring in ROM for Wave Defs
#define PX_WAVE_ROM_IMPLEMENTATION
#include "../px_wave_rom.h"

#define NUM_SAMPLES_PER_BENCH 48000
#define NUM_ITERATIONS 10 // Average over 10 runs

double get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main() {
    printf("# Polysonix VM v1.10.0 (FMA) Performance Report\n\n");
    printf("This report compares the execution time of the new Flat Opcode VM (v1.10.0 (FMA)) against the v1.8.10 baseline.\n");
    printf("Both sets of measurements were taken on this cloud environment for direct comparison.\n\n");
    printf("| Patch ID | Name | v1.8.10 (Before) (ns) | v1.10.0 (After) (ns) | Improvement |\n");
    printf("| :--- | :--- | :--- | :--- | :--- |\n");

    // Initialize Global Tables
    px_vm_init_lfsr_tables();
    initialize_bytecode_cache();

    // Prepare VmParams
    uint32_t rng_state = 12345;
    VmParams params = {
        .x = 0.0f,
        .frequency = 440.0f,
        .rand_offset = 0.5f,
        .modA = 0.0f,
        .modB = 0.0f,
        .modC = 0.0f,
        .lfsr_state = 1,
        .lfsr_type = LFSR_8BIT,
        .lfsr_position = 0,
        .lfsr_seed = 1,
        .rng_state_ptr = &rng_state
    };

    double total_avg_time = 0.0;
    double total_baseline_time = 0.0;
    int valid_patches = 0;

    // Baseline times from v1.8.10 (Cloud VM) - Extracted from PERFORMANCE_REPORT2.md
    double baseline_times[256];
    for (int i = 0; i < 256; i++) baseline_times[i] = 0.0; // Init

    // Injected data from parsing step
    #include "../test/baseline_data.c"

    for (int i = 0; i < NUM_DEFAULT_WAVES; i++) {
        const char* name = default_waves[i].name;
        const char* expr = default_waves[i].expression;

        // Skip empty slots
        if (!name || strlen(name) == 0 || strcmp(name, "Placeholder") == 0) continue;

        // Pre-compile
        BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
        if (!chunk) {
            fprintf(stderr, "Failed to compile patch %d: %s\n", i, name);
            continue;
        }

        // Disable Native Function linking to benchmark Bytecode Interpreter
        // if (i >= 0 && i < sizeof(native_waves)/sizeof(native_waves[0])) {
        //     chunk->native_func = native_waves[i];
        // }

        double patch_total_time = 0.0;

        for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
            double start = get_time_ns();
            for (int s = 0; s < NUM_SAMPLES_PER_BENCH; s++) {
                params.x += 0.01f;
                if (params.x > 6.28f) params.x -= 6.28f;
                volatile float res = execute_bytecode(chunk, &params);
                (void)res;
            }
            double end = get_time_ns();
            patch_total_time += (end - start);
        }

        double avg_ns_per_sample = patch_total_time / (double)NUM_ITERATIONS / (double)NUM_SAMPLES_PER_BENCH;
        double baseline = baseline_times[i];

        char improvement_str[32];
        if (baseline > 0.1) {
             double impr = (baseline - avg_ns_per_sample) / baseline * 100.0;
             if (impr > 0) snprintf(improvement_str, 32, "**%.2f%%**", impr);
             else snprintf(improvement_str, 32, "%.2f%%", impr);
             total_baseline_time += baseline;
        } else {
             snprintf(improvement_str, 32, "N/A");
             // For summary calc, assume no change if no baseline, or skip?
             // Better to skip for average improvement calc, but keep for total avg time.
             total_baseline_time += avg_ns_per_sample; // Treat as same for neutral impact on avg
        }

        printf("| %d | %s | %.2f | %.2f | %s |\n", i, name, baseline, avg_ns_per_sample, improvement_str);

        total_avg_time += avg_ns_per_sample;
        valid_patches++;

        free_bytecode_chunk(chunk);
        free(chunk);
    }

    double overall_improvement = 0.0;
    if (total_baseline_time > 0.001) {
        overall_improvement = (total_baseline_time - total_avg_time) / total_baseline_time * 100.0;
    }

    printf("\n## Summary\n\n");
    printf("* **Total Patches Benchmarked:** %d\n", valid_patches);
    printf("* **Average Time per Sample (v1.8.10):** %.2f ns\n", total_baseline_time / valid_patches);
    printf("* **Average Time per Sample (v1.10.0 (FMA)):** %.2f ns\n", total_avg_time / valid_patches);
    printf("* **Overall Performance Improvement:** **%.2f%%**\n", overall_improvement);

    // Cleanup
    px_vm_free_lfsr_tables();
    free_bytecode_cache();

    return 0;
}
