#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define POLYSONIX_IMPLEMENTATION
#define PX_BENCHMARK_NATIVE_WAVES
#include "../polysonix.h"

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define VERIFY_EPSILON 1e-4f // Tolerance for float differences

int main() {
    // Initialize System (prints logs to stdout)
    // We can't easily suppress these without changing the lib, so we let them print
    // but we start our report after.
    px_vm_init_lfsr_tables();
    initialize_bytecode_cache();

    printf("\n# VM Accuracy Verification Report\n\n");
    printf("Comparing Bytecode Interpreter vs Transpiled Native C for all %d default waves.\n\n", NUM_DEFAULT_WAVES);
    printf("| ID | Wave Name | Max Diff | Status |\n");
    printf("|:---|:---|:---|:---|\n");

    int failures = 0;

    // Seed for LFSR consistency
    uint32_t rng_state = 12345;

    for (int i = 0; i < NUM_DEFAULT_WAVES; ++i) {
        const char* name = default_waves[i].name;
        const char* expr = default_waves[i].expression;

        // Skip placeholders/empty
        if (!name || strlen(name) == 0) continue;

        // Compile Bytecode (Fresh)
        BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
        if (!chunk) {
            printf("| %d | %s | N/A | **COMPILE FAIL** |\n", i, name);
            failures++;
            continue;
        }

        // Get Native Function
        NativeWaveFunc native_func = native_waves[i];
        if (!native_func) {
             printf("| %d | %s | N/A | **NO NATIVE FUNC** |\n", i, name);
             free_bytecode_chunk(chunk);
             free(chunk);
             failures++;
             continue;
        }

        float max_diff = 0.0f;
        int tests_run = 0;

        // Test Sweep: Phase + Modulations
        VmParams params = {0};
        params.frequency = 440.0f;
        params.rand_offset = 0.5f;
        params.rng_state_ptr = &rng_state;

        // Sweep logic
        for (int p_step = 0; p_step <= 16; ++p_step) {
            float phase = (float)p_step / 16.0f * (float)M_PI * 2.0f;
            params.x = phase;

            float mod_vals[] = {-1.0f, 0.0f, 1.0f};
            for (int ma=0; ma<3; ++ma) {
                for (int mb=0; mb<3; ++mb) {
                    params.modA = mod_vals[ma];
                    params.modB = mod_vals[mb];
                    params.modC = 0.5f; // Fixed test value

                    // Reset LFSR/RNG for deterministic comparison
                    uint32_t saved_lfsr = 0xACE1;
                    uint32_t saved_rng = 12345;

                    // Bytecode Run
                    params.lfsr_state = saved_lfsr;
                    *params.rng_state_ptr = saved_rng;
                    float bc_val = execute_bytecode(chunk, &params);

                    // Native Run
                    params.lfsr_state = saved_lfsr;
                    *params.rng_state_ptr = saved_rng;
                    float nat_val = native_func(&params);

                    float diff = fabsf(bc_val - nat_val);
                    if (diff > max_diff) max_diff = diff;
                    tests_run++;
                }
            }
        }

        // Output Row
        if (max_diff > VERIFY_EPSILON) {
            printf("| %d | %s | %.6f | **FAIL** |\n", i, name, max_diff);
            failures++;
        } else {
            printf("| %d | %s | %.6f | PASS |\n", i, name, max_diff);
        }

        free_bytecode_chunk(chunk);
        free(chunk);
    }

    printf("\n**Summary:** %d Failures / %d Waves Checked.\n", failures, NUM_DEFAULT_WAVES);

    // Cleanup
    free_bytecode_cache();
    px_vm_free_lfsr_tables(); // Prints "Freeing..."

    return (failures == 0) ? 0 : 1;
}
