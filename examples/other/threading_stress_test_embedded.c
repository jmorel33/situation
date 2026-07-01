/*
 * Threading Stress Test - Embedded Wrapper
 * This is the entry point that loads the embedded DLL and calls the real main
 */

#include <stdio.h>
#include "../dll_loader.h"

// External main function (renamed from threading_stress_test_console.c)
extern int threading_main(int argc, char** argv);

int main(int argc, char** argv) {
    // Load embedded DLL
    if (!load_embedded_dll()) {
        fprintf(stderr, "Failed to load embedded situation.dll\n");
        return -1;
    }

    // Call the real main
    int result = threading_main(argc, argv);

    // Cleanup
    cleanup_embedded_dll();

    return result;
}
