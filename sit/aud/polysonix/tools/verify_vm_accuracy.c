#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define POLYSONIX_IMPLEMENTATION
#include "../polysonix.h"

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define EPSILON 1e-5f

void verify_patch(const char* name, const char* expr, float x_val, float expected) {
    BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
    if (!chunk) {
        printf("FAIL: %s - Compilation failed\n", name);
        return;
    }

    uint32_t rng = 123;
    VmParams params = {0};
    params.x = x_val;
    params.rng_state_ptr = &rng;

    // For deterministic tests, set other params to 0
    params.modA = 0.0f;
    params.modB = 0.0f;
    params.modC = 0.0f;

    float result = execute_bytecode(chunk, &params);

    // Check diff
    if (fabsf(result - expected) < EPSILON) {
        printf("PASS: %s (x=%.2f) -> Expected %.4f, Got %.4f\n", name, x_val, expected, result);
    } else {
        printf("FAIL: %s (x=%.2f) -> Expected %.4f, Got %.4f (Diff: %.4f)\n", name, x_val, expected, result, fabsf(result - expected));
    }

    free_bytecode_chunk(chunk);
    free(chunk);
}

int main() {
    printf("# VM Accuracy Verification\n");
    printf("Verifying mathematical correctness of Flat Opcode implementation.\n\n");

    px_vm_init_lfsr_tables();
    initialize_bytecode_cache();

    // 1. Sine Wave: sin(x)
    // x = 0 -> 0
    // x = PI/2 -> 1
    // x = PI -> 0
    // x = 3PI/2 -> -1
    verify_patch("Sine (0)", "sin(x)", 0.0f, 0.0f);
    verify_patch("Sine (PI/2)", "sin(x)", (float)M_PI / 2.0f, 1.0f);
    verify_patch("Sine (PI)", "sin(x)", (float)M_PI, 0.0f);
    verify_patch("Sine (3PI/2)", "sin(x)", 3.0f * (float)M_PI / 2.0f, -1.0f);

    // 2. Sawtooth: 1.0 - (x / PI) [Approx standard logic]
    // Expr: "1.0 - (x / PI)"
    // x=0 -> 1.0
    // x=PI -> 0.0
    // x=2PI -> -1.0
    verify_patch("Sawtooth (0)", "1.0 - (x / PI)", 0.0f, 1.0f);
    verify_patch("Sawtooth (PI)", "1.0 - (x / PI)", (float)M_PI, 0.0f);
    verify_patch("Sawtooth (2PI)", "1.0 - (x / PI)", 2.0f * (float)M_PI, -1.0f);

    // 3. Logic/Conditional: x < PI ? 0.5 : -0.5
    verify_patch("Ternary (True)", "x < PI ? 0.5 : -0.5", 1.0f, 0.5f);
    verify_patch("Ternary (False)", "x < PI ? 0.5 : -0.5", 4.0f, -0.5f);

    // 4. Sigma Summation: Additive Harmonics
    // sigma(k, 1, 3, 1, sin(x*k)/k)
    // x = PI/2
    // k=1: sin(PI/2)/1 = 1.0
    // k=2: sin(PI)/2 = 0.0
    // k=3: sin(3PI/2)/3 = -1/3 = -0.3333
    // Sum = 0.6666
    verify_patch("Sigma Additive", "sigma(k, 1, 3, 1, sin(x*k)/k)", (float)M_PI / 2.0f, 0.6666666f);

    // 5. Pow function
    verify_patch("Pow (2^3)", "pow(2, 3)", 0.0f, 8.0f);

    // 6. Abs
    verify_patch("Abs (-1.5)", "abs(-1.5)", 0.0f, 1.5f);

    // 7. Floor/Ceil
    verify_patch("Floor (1.9)", "floor(1.9)", 0.0f, 1.0f);
    verify_patch("Ceil (1.1)", "ceil(1.1)", 0.0f, 2.0f);

    // Cleanup
    px_vm_free_lfsr_tables();
    free_bytecode_cache();

    return 0;
}
