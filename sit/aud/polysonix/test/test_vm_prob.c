#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#define POLYSONIX_IMPLEMENTATION
#include "../px_vm.h"

int main() {
    printf("Testing VM prob() implementation...\n");

    px_vm_init_lfsr_tables();
    initialize_bytecode_cache();

    VmParams params = {0};

    // 1. Deterministic: Set RAND_OFFSET = 0.4, prob(0.5, 1, 0) -> returns 1.0.
    {
        const char* expr = "prob(0.5, 1.0, 0.0)";
        BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
        assert(chunk != NULL);

        params.rand_offset = 0.4f;
        float result = execute_bytecode(chunk, &params);
        printf("Result (0.4 < 0.5): %f\n", result);
        assert(fabsf(result - 1.0f) < 1e-6f);

        // 2. Deterministic: Set RAND_OFFSET = 0.6, prob(0.5, 1, 0) -> returns 0.0.
        params.rand_offset = 0.6f;
        result = execute_bytecode(chunk, &params);
        printf("Result (0.6 < 0.5): %f\n", result);
        assert(fabsf(result - 0.0f) < 1e-6f);

        free_bytecode_chunk(chunk);
        free(chunk);
    }

    // 3. Edge: prob(0, 1, 0) -> returns 0.0.
    {
        const char* expr = "prob(0.0, 1.0, 0.0)";
        BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
        assert(chunk != NULL);

        params.rand_offset = 0.5f;
        float result = execute_bytecode(chunk, &params);
        printf("Result (prob 0.0): %f\n", result);
        assert(fabsf(result - 0.0f) < 1e-6f);

        params.rand_offset = 0.0f; // rand_offset < 0.0 is false
        result = execute_bytecode(chunk, &params);
        assert(fabsf(result - 0.0f) < 1e-6f);

        free_bytecode_chunk(chunk);
        free(chunk);
    }

    // 4. Edge: prob(1, 1, 0) -> returns 1.0.
    {
        const char* expr = "prob(1.0, 1.0, 0.0)";
        BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
        assert(chunk != NULL);

        params.rand_offset = 0.5f;
        float result = execute_bytecode(chunk, &params);
        printf("Result (prob 1.0): %f\n", result);
        assert(fabsf(result - 1.0f) < 1e-6f);

        params.rand_offset = 0.99f;
        result = execute_bytecode(chunk, &params);
        assert(fabsf(result - 1.0f) < 1e-6f);

        free_bytecode_chunk(chunk);
        free(chunk);
    }

    // 5. Script: "prob(RAND_OFFSET, sin(x), 0.0)" -> evaluates correctly.
    // wait, prob(RAND_OFFSET, ..., ...) implies chance = RAND_OFFSET.
    // rand_offset < chance => rand_offset < RAND_OFFSET -> false. So it always returns false.
    // Let's test prob(0.5, sin(x), saw(x)) instead. But we don't have saw(x). We can test prob(0.5, sin(x), cos(x)).
    {
        const char* expr = "prob(0.5, sin(x), cos(x))";
        BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
        assert(chunk != NULL);

        params.x = 0.0f;
        params.rand_offset = 0.4f; // Should return sin(0) = 0
        float result_true = execute_bytecode(chunk, &params);
        printf("Result Script (0.4 < 0.5, sin(0)): %f\n", result_true);
        assert(fabsf(result_true - 0.0f) < 1e-6f);

        params.rand_offset = 0.6f; // Should return cos(0) = 1
        float result_false = execute_bytecode(chunk, &params);
        printf("Result Script (0.6 < 0.5, cos(0)): %f\n", result_false);
        assert(fabsf(result_false - 1.0f) < 1e-6f);

        free_bytecode_chunk(chunk);
        free(chunk);
    }

    px_vm_free_lfsr_tables();
    free_bytecode_cache();

    printf("Test Passed!\n");
    return 0;
}
