#define PX_VM_IMPLEMENTATION
#include "../px_vm.h"
#include <stdio.h>

// This file acts as the "Implementation" unit.
// It compiles the actual function bodies of px_vm.h.

void run_vm_impl_test() {
    printf("Implementation Unit: Initializing LFSR...\n");
    px_vm_init_lfsr_tables();

    Token t[10];
    int count = tokenize("1 + 1", t, 10);
    printf("Implementation Unit: Tokenize returned %d tokens.\n", count);
}
