#include "px_vm.h"
#include <stdio.h>
#include <math.h>

#define EPSILON 1e-6f

int main() {
    printf("Testing OP_SMOOTH_SELECT...\n");

    VmParams params = {0};

    // Test exact indices (param = 0.0, 1.0) -> v1, v2
    BytecodeChunk* chunk = compile_expression_to_bytecode("smooth_select(0.0, 10, 20, 30)");
    float result = execute_bytecode(chunk, &params);
    if (fabs(result - 10.0f) > EPSILON) {
        printf("Test failed (exact lower). Expected 10.0, got %f\n", result);
        return 1;
    }

    chunk = compile_expression_to_bytecode("smooth_select(1.0, 10, 20, 30)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 20.0f) > EPSILON) {
        printf("Test failed (exact middle). Expected 20.0, got %f\n", result);
        return 1;
    }

    chunk = compile_expression_to_bytecode("smooth_select(2.0, 10, 20, 30)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 30.0f) > EPSILON) {
        printf("Test failed (exact upper). Expected 30.0, got %f\n", result);
        return 1;
    }

    // Test lerp
    chunk = compile_expression_to_bytecode("smooth_select(0.5, 10, 20, 30)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 15.0f) > EPSILON) {
        printf("Test failed (lerp 1). Expected 15.0, got %f\n", result);
        return 1;
    }

    chunk = compile_expression_to_bytecode("smooth_select(1.25, 10, 20, 30)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 22.5f) > EPSILON) {
        printf("Test failed (lerp 2). Expected 22.5, got %f\n", result);
        return 1;
    }

    // Out of bounds upper
    chunk = compile_expression_to_bytecode("smooth_select(2.5, 10, 20, 30)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 30.0f) > EPSILON) {
        printf("Test failed (upper bounds clamp). Expected 30.0, got %f\n", result);
        return 1;
    }

    // Out of bounds lower
    chunk = compile_expression_to_bytecode("smooth_select(-0.5, 10, 20, 30)");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 10.0f) > EPSILON) {
        printf("Test failed (lower bounds clamp). Expected 10.0, got %f\n", result);
        return 1;
    }

    // Test nested expressions
    // smooth_select(0.5, sin(PI/2), max(5, 10), pow(2, 3))
    // 0.5 lerps between sin(PI_OVER_2) (1.0) and max(5, 10) (10.0) -> 1.0 + 0.5 * 9.0 = 5.5
    chunk = compile_expression_to_bytecode("smooth_select(0.5, sin(PI_OVER_2), max(5, 10), pow(2, 3))");
    result = execute_bytecode(chunk, &params);
    if (fabs(result - 5.5f) > EPSILON) {
        printf("Test failed (nested expressions). Expected 5.5, got %f\n", result);
        return 1;
    }

    printf("OP_SMOOTH_SELECT works perfectly with nested expressions!\n");
    return 0;
}
