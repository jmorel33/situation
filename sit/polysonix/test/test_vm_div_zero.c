#define PX_VM_IMPLEMENTATION
#include "px_vm.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>

int test_div_zero() {
    printf("\n--- Test: Division by zero ---\n");
    const char *expr = "(10.0 / 0.0) + 5.0";
    Token tokens[32];
    int pos = 0;
    tokenize(expr, tokens, 32);
    Node *ast = parseExpression(tokens, &pos);

    BytecodeChunk chunk;
    init_bytecode_chunk(&chunk);
    bool compiled = compile_ast_to_bytecode(ast, &chunk);
    if (!compiled) {
        printf("FAILURE: Compilation failed.\n");
        return 1;
    }

    VmParams params = {0};
    params.lfsr_state = 1;

    float result = execute_bytecode(&chunk, &params);
    printf("Result of '%s': %f (Expected: inf/nan)\n", expr, result);

    freeAST(ast);
    free_bytecode_chunk(&chunk);

    if (isinf(result) || isnan(result)) {
        printf("SUCCESS: Division by zero handled correctly.\n");
        return 0;
    } else {
        printf("FAILURE: Division by zero NOT handled correctly. Got %f\n", result);
        return 1;
    }
}

int test_mod_zero() {
    printf("\n--- Test: Modulo by zero ---\n");
    const char *expr = "(10.0 % 0.0) + 5.0";
    Token tokens[32];
    int pos = 0;
    tokenize(expr, tokens, 32);
    Node *ast = parseExpression(tokens, &pos);

    BytecodeChunk chunk;
    init_bytecode_chunk(&chunk);
    bool compiled = compile_ast_to_bytecode(ast, &chunk);
    if (!compiled) {
        printf("FAILURE: Compilation failed.\n");
        return 1;
    }

    VmParams params = {0};
    params.lfsr_state = 1;

    float result = execute_bytecode(&chunk, &params);
    printf("Result of '%s': %f (Expected: inf/nan)\n", expr, result);

    freeAST(ast);
    free_bytecode_chunk(&chunk);

    if (isinf(result) || isnan(result)) {
        printf("SUCCESS: Modulo by zero handled correctly.\n");
        return 0;
    } else {
        printf("FAILURE: Modulo by zero NOT handled correctly. Got %f\n", result);
        return 1;
    }
}

int test_div_small() {
    printf("\n--- Test: Division by small value (< EPSILON) ---\n");
    // EPSILON is 1e-6f.
    const char *expr = "(1.0 / 0.0000001) + 5.0";
    Token tokens[32];
    int pos = 0;
    tokenize(expr, tokens, 32);
    Node *ast = parseExpression(tokens, &pos);

    BytecodeChunk chunk;
    init_bytecode_chunk(&chunk);
    bool compiled = compile_ast_to_bytecode(ast, &chunk);
    if (!compiled) {
        printf("FAILURE: Compilation failed.\n");
        return 1;
    }

    VmParams params = {0};
    params.lfsr_state = 1;

    float result = execute_bytecode(&chunk, &params);
    printf("Result of '%s': %f (Expected: inf/nan)\n", expr, result);

    freeAST(ast);
    free_bytecode_chunk(&chunk);

    if (isinf(result) || isnan(result) || result > 10000.0f) {
        printf("SUCCESS: Small divisor handled correctly.\n");
        return 0;
    } else {
        printf("FAILURE: Small divisor NOT handled correctly. Got %f\n", result);
        return 1;
    }
}

int test_sigma_div_zero() {
    printf("\n--- Test: Division by zero in Sigma ---\n");
    // sigma(var, start, end, step, body)
    const char *expr = "sigma(i, 0, 2, 1, 1 / 0)";
    Token tokens[64];
    int pos = 0;
    tokenize(expr, tokens, 64);
    Node *ast = parseExpression(tokens, &pos);

    BytecodeChunk chunk;
    init_bytecode_chunk(&chunk);
    bool compiled = compile_ast_to_bytecode(ast, &chunk);
    if (!compiled) {
        printf("FAILURE: Compilation failed.\n");
        return 1;
    }

    VmParams params = {0};
    params.lfsr_state = 1;

    float result = execute_bytecode(&chunk, &params);
    printf("Result of '%s': %f (Expected: inf/nan)\n", expr, result);

    freeAST(ast);
    free_bytecode_chunk(&chunk);

    if (isinf(result) || isnan(result)) {
        printf("SUCCESS: Division by zero in Sigma handled correctly.\n");
        return 0;
    } else {
        printf("FAILURE: Division by zero in Sigma NOT handled correctly. Got %f\n", result);
        return 1;
    }
}

int main() {
    px_vm_init_lfsr_tables();

    int failed = 0;
    failed += test_div_zero();
    failed += test_mod_zero();
    failed += test_div_small();
    failed += test_sigma_div_zero();

    if (failed > 0) {
        printf("\nSome tests FAILED (%d).\n", failed);
        return 1;
    } else {
        printf("\nAll division tests PASSED.\n");
        return 0;
    }
}
