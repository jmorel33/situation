/*
 * Embedded wrapper for threading_raw
 */

#include <windows.h>
#include <stdio.h>
#include "../dll_loader.h"

extern int threading_raw_main(int argc, char** argv);

int main(int argc, char** argv) {
    HMODULE hSituationDLL = LoadEmbeddedDLL();
    
    if (!hSituationDLL) {
        fprintf(stderr, "Failed to load embedded situation.dll\n");
        return -1;
    }

    int result = threading_raw_main(argc, argv);

    UnloadEmbeddedDLL(hSituationDLL);
    return result;
}
