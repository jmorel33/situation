
#include "px_vm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    // 1. Test emit_byte overflow directly
    BytecodeChunk chunk;
    init_bytecode_chunk(&chunk);

    for (int i = 0; i < MAX_BYTECODE_SIZE; i++) {
        emit_byte(&chunk, 0x00);
    }

    if (chunk.code_count != MAX_BYTECODE_SIZE) {
        fprintf(stderr, "Error: code_count (%d) != MAX_BYTECODE_SIZE (%d)\n", chunk.code_count, MAX_BYTECODE_SIZE);
        return 1;
    }
    if (chunk.has_error) {
        fprintf(stderr, "Error: has_error is true before overflow\n");
        return 1;
    }

    emit_byte(&chunk, 0x00);
    if (!chunk.has_error) {
        fprintf(stderr, "Error: has_error is false after overflow\n");
        return 1;
    }
    printf("Direct emit_byte overflow test passed.\n");

    // 2. Test constant pool overflow
    init_bytecode_chunk(&chunk);
    for (int i = 0; i < MAX_CONSTANTS; i++) {
        add_constant(&chunk, (float)i);
    }
    if (chunk.has_error) {
        fprintf(stderr, "Error: has_error is true before constant pool overflow\n");
        return 1;
    }
    add_constant(&chunk, 9999.0f);
    if (!chunk.has_error) {
        fprintf(stderr, "Error: has_error is false after constant pool overflow\n");
        return 1;
    }
    printf("Constant pool overflow test passed.\n");

    // 3. Test string pool overflow
    init_bytecode_chunk(&chunk);
    for (int i = 0; i < MAX_STRINGS; i++) {
        char name[16];
        sprintf(name, "s%d", i);
        add_string(&chunk, name);
    }
    if (chunk.has_error) {
        fprintf(stderr, "Error: has_error is true before string pool overflow\n");
        return 1;
    }
    add_string(&chunk, "overflow");
    if (!chunk.has_error) {
        fprintf(stderr, "Error: has_error is false after string pool overflow\n");
        return 1;
    }
    printf("String pool overflow test passed.\n");

    // 4. Test compiler failure on token overflow (pre-existing but good to verify it handles failure)
    char* long_expr = malloc(10000);
    strcpy(long_expr, "1.0");
    for (int i = 0; i < 1000; i++) {
        strcat(long_expr, "+1.0");
    }

    printf("Attempting to compile a very long expression (hits token limit)...\n");
    BytecodeChunk* overflow_chunk = compile_expression_to_bytecode(long_expr);
    if (overflow_chunk != NULL) {
        fprintf(stderr, "Error: compile_expression_to_bytecode should have returned NULL\n");
        return 1;
    }
    printf("Token overflow handled correctly.\n");
    free(long_expr);

    printf("All bytecode overflow tests passed!\n");
    return 0;
}
