#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#define POLYSONIX_IMPLEMENTATION
#include "../polysonix.h"
#include "../px_vm.h"

#define DSP_MATH_IMPLEMENTATION
#include "../dsp_math.h"

float evaluate_expr(const char* expr, bool* success) {
    BytecodeChunk* chunk = compile_expression_to_bytecode(expr);
    if (!chunk) {
        *success = false;
        return 0.0f;
    }
    *success = true;

    VmParams params = {
        .x = 0.0f,
        .frequency = 0.0f,
        .rand_offset = 0.0f,
        .modA = 0.0f,
        .modB = 0.0f,
        .modC = 0.0f,
        .lfsr_state = 1,
        .lfsr_type = LFSR_8BIT,
        .lfsr_position = 0,
        .lfsr_seed = 1
    };

    float result = execute_bytecode(chunk, &params);
    free_bytecode_chunk(chunk);
    free(chunk);
    return result;
}

int test_op_clamp() {
    printf("Running test_op_clamp...\n");

    float out;
    bool success;

    out = evaluate_expr("clamp(5.0, 0.0, 10.0)", &success);
    assert(success);
    assert(fabsf(out - 5.0f) < 1e-5f);

    out = evaluate_expr("clamp(-1.0, 0.0, 10.0)", &success);
    assert(success);
    assert(fabsf(out - 0.0f) < 1e-5f);

    out = evaluate_expr("clamp(15.0, 0.0, 10.0)", &success);
    assert(success);
    assert(fabsf(out - 10.0f) < 1e-5f);

    out = evaluate_expr("clamp(5.0, 10.0, 0.0)", &success);
    assert(success);
    assert(fabsf(out - 10.0f) < 1e-5f);

    out = evaluate_expr("clamp(nan(), 0.0, 1.0)", &success);
    assert(success);
    // NaN behavior depends on the C library, skipped strict assert

    return 0;
}

int test_op_mix() {
    printf("Running test_op_mix...\n");

    float out;
    bool success;

    out = evaluate_expr("mix(0.5, 0.0, 2.0)", &success);
    assert(success);
    assert(fabsf(out - 1.0f) < 1e-5f);

    out = evaluate_expr("mix(0.0, 1.0, 3.0)", &success);
    assert(success);
    assert(fabsf(out - 1.0f) < 1e-5f);

    out = evaluate_expr("mix(1.0, 1.0, 3.0)", &success);
    assert(success);
    assert(fabsf(out - 3.0f) < 1e-5f);

    out = evaluate_expr("mix(1.2, 1.0, 3.0)", &success);
    assert(success);
    assert(fabsf(out - 3.0f) < 1e-5f);

    out = evaluate_expr("mix(-0.5, 1.0, 3.0)", &success);
    assert(success);
    assert(fabsf(out - 1.0f) < 1e-5f);

    out = evaluate_expr("mix(nan(), 1.0, 3.0)", &success);
    assert(success);

    return 0;
}

int test_op_ramp() {
    printf("Running test_op_ramp...\n");

    float out;
    bool success;

    out = evaluate_expr("ramp(0.0, 2.0, 0.5)", &success);
    assert(success);
    assert(fabsf(out - 1.0f) < 1e-5f);

    out = evaluate_expr("ramp(0.0, 2.0, 0.0)", &success);
    assert(success);
    assert(fabsf(out - 0.0f) < 1e-5f);

    out = evaluate_expr("ramp(0.0, 2.0, 1.0)", &success);
    assert(success);
    assert(fabsf(out - 2.0f) < 1e-5f);

    out = evaluate_expr("ramp(0.0, 2.0, 1.2)", &success);
    assert(success);
    assert(fabsf(out - 2.0f) < 1e-5f);

    return 0;
}

int main() {
    InitFastDSP();

    test_op_clamp();
    test_op_mix();
    test_op_ramp();

    FreeFastDSP();
    printf("All VM taming opcode tests passed!\n");
    return 0;
}
