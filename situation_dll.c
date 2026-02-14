/*
 * situation_dll.c
 * 
 * DLL Entry Point for Situation Library
 * 
 * This file includes the full situation implementation with SITUATION_IMPLEMENTATION
 * defined, allowing the entire library to be compiled into a shared library (DLL).
 * 
 * When building the DLL, compile with:
 *   -DSITUATION_BUILD_SHARED
 *   -DSITUATION_IMPLEMENTATION
 *   -DSITUATION_USE_VULKAN (or SITUATION_USE_OPENGL)
 */

#define SITUATION_IMPLEMENTATION
#include "situation.h"

#if defined(_WIN32)
#include <windows.h>

/*
 * DLL Entry Point
 * Called when the DLL is loaded or unloaded
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            // DLL is being loaded into a process
            break;
        case DLL_PROCESS_DETACH:
            // DLL is being unloaded from a process
            break;
        case DLL_THREAD_ATTACH:
            // A thread is being created in a process that has already loaded this DLL
            break;
        case DLL_THREAD_DETACH:
            // A thread is exiting in a process that has already loaded this DLL
            break;
    }
    return TRUE;
}

#endif // _WIN32
