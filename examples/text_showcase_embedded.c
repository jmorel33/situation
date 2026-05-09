/*
 * text_showcase_embedded.c
 * 
 * Wrapper that loads embedded situation.dll before running text_showcase
 */

#include <windows.h>
#include <stdio.h>
#include "../dll_loader.h"

// Forward declare the main function from text_showcase
// We'll link text_showcase.o but rename its main to text_showcase_main
extern int text_showcase_main(int argc, char** argv);

int main(int argc, char** argv) {
    HMODULE hSituationDLL;
    int result;

    printf("========================================\n");
    printf("  Text Showcase - Embedded DLL Version\n");
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

    // Run the actual text_showcase program
    result = text_showcase_main(argc, argv);

    // Cleanup
    printf("\nUnloading embedded DLL...\n");
    UnloadEmbeddedDLL(hSituationDLL);

    return result;
}
