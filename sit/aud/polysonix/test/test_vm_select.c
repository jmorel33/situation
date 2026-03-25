#include "px_vm.h"
#include <stdio.h>
#include <math.h>

#define EPSILON 1e-6f

int main() {
    printf("Testing OP_SELECT...\n");

    VmParams params = {0};

    // Test basic selection
    BytecodeChunk* chunk = compile_expression_to_bytecode("select(0.3, 1, 2, 3)");
    float result = execute_bytecode(chunk, &params);
    if (fabs(result - 1.0f) > EPSILON) {
        printf("Test failed (basic). Expected 1.0, got %f\n", result);
        return 1;
    }

    // Out of bounds
    chunk = compile_expression_to_bytecode("select(1.5, 1, 2)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 2.0f) > EPSILON) {
        printf("Test failed (upper bounds). Expected 2.0, got %f\n", result);
        return 1;
    }

    // Out of bounds lower
    chunk = compile_expression_to_bytecode("select(-0.5, 3, 4)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 3.0f) > EPSILON) {
        printf("Test failed (lower bounds). Expected 3.0, got %f\n", result);
        return 1;
    }

    // Test nested expressions
    // select(0.8, sin(PI/2), max(5, 10), pow(2, 3))
    // 0.8 scales over 3 items -> floor(0.8 * 3) = 2 -> picks index 2 (the 3rd item, pow(2,3))
    chunk = compile_expression_to_bytecode("select(0.8, sin(PI_OVER_2), max(5, 10), pow(2, 3))");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 8.0f) > EPSILON) {
        printf("Test failed (nested expressions). Expected 8.0, got %f\n", result);
        return 1;
    }

    // Test prob logic inside select
    // rand_offset = 0.5. prob(0.4, 100, 200) -> 0.5 < 0.4 is false, returns 200
    // select(0.1, prob(0.4, 100, 200), 300) -> picks index 0 (the prob)
    params.rand_offset = 0.5f;
    chunk = compile_expression_to_bytecode("select(0.1, prob(0.4, 100, 200), 300)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 200.0f) > EPSILON) {
        printf("Test failed (nested prob). Expected 200.0, got %f\n", result);
        return 1;
    }

    printf("OP_SELECT works perfectly with nested expressions!\n");
    return 0;
}
