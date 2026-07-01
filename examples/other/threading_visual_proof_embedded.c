/*
 * Embedded wrapper for threading_visual_proof
 * Extracts and loads situation.dll from resources
 */

#include <windows.h>
#include <stdio.h>
#include "../dll_loader.h"

// Forward declare the renamed main
extern int threading_visual_proof_main(int argc, char** argv);

int main(int argc, char** argv) {
    HMODULE hSituationDLL;
    int result;

    // Extract and load the embedded situation.dll
    hSituationDLL = LoadEmbeddedDLL();
    
    if (!hSituationDLL) {
        fprintf(stderr, "Failed to load embedded situation.dll\n");
        return -1;
    }

    // Call the actual main function
    result = threading_visual_proof_main(argc, argv);

    // Cleanup
    UnloadEmbeddedDLL(hSituationDLL);

    return result;
}
