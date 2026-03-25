#include "px_vm.h"
#include <stdio.h>
#include <math.h>

#define EPSILON 1e-6f

int main() {
    printf("Testing OP_MARKOV...\n");

    VmParams params = {0};
    // Initialize markov state explicitly as it should be done
    params.markov_state[0] = 0;
    params.markov_prev_trigger[0] = false;

    // Test matrix bounds and rising edge (trigger true, prev false)
    params.x = 1.0f; // trigger > 0 => true
    // Matrix:
    // State 0: 0.0 (stay), 1.0 (jump to 1)
    // State 1: 1.0 (jump to 0), 0.0 (stay)
    BytecodeChunk* chunk = compile_expression_to_bytecode("markov(0, x > 0.0, 0.0, 1.0, 1.0, 0.0)");

    // First execution: state 0 -> 1 because row 0 is (0.0, 1.0).
    float result = execute_bytecode(chunk, &params);
    if (fabs(result - 1.0f) > EPSILON) {
        printf("Test failed (state 0 -> 1). Expected 1.0, got %f\n", result);
        return 1;
    }

    if (params.markov_prev_trigger[0] != true) {
        printf("Test failed (prev trigger updated). Expected true, got false\n");
        return 1;
    }

    // Second execution: Trigger is still true (x > 0.0 is true), but prev_trigger is true
    // No rising edge, should NOT transition, returns 1.0
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 1.0f) > EPSILON) {
        printf("Test failed (no rising edge). Expected 1.0, got %f\n", result);
        return 1;
    }

    // Third execution: Trigger becomes false
    params.x = -1.0f; // trigger <= 0 => false
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 1.0f) > EPSILON) {
        printf("Test failed (falling edge). Expected 1.0, got %f\n", result);
        return 1;
    }

    if (params.markov_prev_trigger[0] != false) {
        printf("Test failed (prev trigger updated). Expected false, got true\n");
        return 1;
    }

    // Fourth execution: Trigger becomes true again (rising edge)
    params.x = 1.0f;
    // State 1: row 1 is (1.0, 0.0) -> transitions to 0
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 0.0f) > EPSILON) {
        printf("Test failed (state 1 -> 0). Expected 0.0, got %f\n", result);
        return 1;
    }

    printf("OP_MARKOV works perfectly!\n");
    return 0;
}
