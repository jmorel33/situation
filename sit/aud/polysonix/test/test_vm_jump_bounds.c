#include "px_vm.h"
#include <stdio.h>
#include <string.h>

int main() {
    BytecodeChunk chunk;
    init_bytecode_chunk(&chunk);

    // Create a malicious jump
    // OP_JUMP (0x15), Offset High, Offset Low
    // Offset = 2000 (0x07D0) -> 0x15, 0x07, 0xD0

    // Fill with HALT so if it doesn't jump it stops
    for(int i=0; i<100; i++) chunk.code[i] = OP_HALT;

    chunk.code[0] = OP_JUMP;
    chunk.code[1] = 0x07; // High byte of 2000
    chunk.code[2] = 0xD0; // Low byte of 2000
    chunk.code_count = 3; // Claim we only have 3 bytes of code

    VmParams params = {
        .x = 0.0f,
        .frequency = 440.0f,
        .rand_offset = 0.0f,
        .modA = 0.0f,
        .modB = 0.0f,
        .modC = 0.0f,
        .lfsr_state = 1,
        .lfsr_type = LFSR_8BIT,
        .lfsr_position = 0,
        .lfsr_seed = 1
    };

    printf("Executing malicious bytecode...\n");
    // This should ideally trigger a VM error or crash without the fix
    float result = execute_bytecode(&chunk, &params);
    printf("Execution finished. Result: %f\n", result);

    return 0;
}
