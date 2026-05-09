/*
 * hello_fancy_embedded.c
 * 
 * Wrapper that loads embedded situation.dll before running hello_fancy
 */

#include <windows.h>
#include <stdio.h>
#include "../dll_loader.h"

// Forward declare the main function from hello_fancy
// We'll link hello_fancy.o but rename its main to hello_fancy_main
extern int hello_fancy_main(int argc, char** argv);

int main(int argc, char** argv) {
    HMODULE hSituationDLL;
    int result;

    printf("========================================\n");
    printf("  Hello Fancy - Embedded DLL Version\n");
    printf("========================================\n\n");

    // Extract and load the embedded situation.dll
    printf("Loading embedded situation.dll...\n");
    hSituationDLL = LoadEmbeddedDLL();
    
    if (!hSituationDLL) {
        fprintf(stderr, "\n[ERROR] Failed to load embedded situation.dll\n");
        fprintf(stderr, "Press any key to exit...\n");
        getchar();
        return -1;
    }

    printf("DLL loaded successfully!\n\n");

    // Run the actual hello_fancy program
    result = hello_fancy_main(argc, argv);

    // Cleanup
    printf("\nUnloading embedded DLL...\n");
    UnloadEmbeddedDLL(hSituationDLL);

    return result;
}
