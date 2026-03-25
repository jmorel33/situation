#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define POLYSONIX_IMPLEMENTATION
#include "polysonix.h"

// Bring in ROM for Wave Defs
#define PX_WAVE_ROM_IMPLEMENTATION
#include "px_wave_rom.h"

double get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main() {
    // Corrected variable names
    const char* expr = "MOD_A > 0 ? (MOD_B > 0 ? (MOD_C > 0 ? 1 : 2) : (MOD_C > 0.5 ? 3 : 4)) : (MOD_B > 0.5 ? (MOD_C > 0 ? 5 : 6) : (MOD_C > 0.5 ? 7 : 8))";

    // Warm up
    for (int i = 0; i < 1000; i++) {
        BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
        if (chunk) {
            free_bytecode_chunk(chunk);
            free(chunk);
        }
    }

    int iterations = 200000;
    double start = get_time_ns();
    for (int i = 0; i < iterations; i++) {
        BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
        if (chunk) {
            volatile int count = chunk->code_count;
            (void)count;
            free_bytecode_chunk(chunk);
            free(chunk);
        } else {
            fprintf(stderr, "Failed to compile\n");
            return 1;
        }
    }
    double end = get_time_ns();

    printf("Compilation of %d complex expressions took %.2f ms (%.2f ns per compilation)\n",
           iterations, (end - start) / 1e6, (end - start) / (double)iterations);

    return 0;
}
