#include "../px_vm.h"
#include <stdio.h>

// This file acts as a "Client" unit.
// It includes px_vm.h purely as a header.
// It should NOT see function definitions, only declarations.
// If definitions leak, linking this with test_px_vm_impl.o will fail (multiple definitions).
// If declarations are missing, compiling this will fail (undefined reference or implicit declaration).

extern void run_vm_impl_test(); // From test_px_vm_impl.c

int main() {
    printf("Client Unit: Starting Single-Header Compliance Test...\n");

    // Call function from the other translation unit to ensure linkage works
    run_vm_impl_test();

    // Call function from the header directly to ensure visibility
    // We pick a function that is part of the API
    const char* expr = "sin(x)";
    Token tokens[10];
    printf("Client Unit: Calling tokenize from header...\n");
    int count = tokenize(expr, tokens, 10);

    if (count > 0) {
        printf("Client Unit: Success. Token count: %d\n", count);
    } else {
        printf("Client Unit: Failed to tokenize.\n");
        return 1;
    }

    return 0;
}
