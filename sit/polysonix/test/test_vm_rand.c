#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

// Define implementation to get function bodies in px_vm.h
// Assuming px_vm.h doesn't require a specific define, but let's define POLYSONIX_IMPLEMENTATION just in case dependencies need it.
#define POLYSONIX_IMPLEMENTATION
#include "../px_vm.h"

int main() {
    printf("Testing VM rand() implementation...\n");

    // 1. Initialize LFSR tables (required for VM init mostly, though rand() might not need it, others might)
    px_vm_init_lfsr_tables();
    initialize_bytecode_cache();

    // 2. Compile "rand()"
    const char* expr = "rand()";
    BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
    if (!chunk) {
        fprintf(stderr, "Failed to compile rand()\n");
        return 1;
    }

    // 3. Setup VmParams with custom RNG state
    uint32_t my_rng_state = 12345;
    uint32_t initial_state = my_rng_state;

    VmParams params = {0};
    params.rng_state_ptr = &my_rng_state;

    // 4. Execute bytecode first time
    float result1 = execute_bytecode(chunk, &params);
    printf("Result 1: %f, State: %u\n", result1, my_rng_state);

    // Verify result is in [0, 1]
    assert(result1 >= 0.0f && result1 <= 1.0f);
    // Verify state updated
    assert(my_rng_state != initial_state);

    // 5. Execute bytecode second time
    uint32_t state_after_first = my_rng_state;
    float result2 = execute_bytecode(chunk, &params);
    printf("Result 2: %f, State: %u\n", result2, my_rng_state);

    // Verify state updated again
    assert(my_rng_state != state_after_first);
    // Verify results likely different (probabilistic, but highly likely)
    if (result1 == result2) {
        printf("Warning: Result 1 and 2 are equal (could happen but rare)\n");
    }

    // 6. Verify deterministic behavior with same seed
    my_rng_state = 12345;
    float result3 = execute_bytecode(chunk, &params);
    printf("Result 3 (reset): %f\n", result3);
    assert(fabsf(result1 - result3) < 1e-9f);

    // 7. Verify handling of NULL rng_state_ptr (Safe fallback check)
    params.rng_state_ptr = NULL;
    float result_fallback = execute_bytecode(chunk, &params);
    printf("Result Fallback (NULL ptr): %f\n", result_fallback);
    // Expect 0.0f as per implementation
    assert(result_fallback == 0.0f);

    // Cleanup
    free_bytecode_chunk(chunk);
    free(chunk);
    px_vm_free_lfsr_tables();
    free_bytecode_cache();

    printf("Test Passed!\n");
    return 0;
}
