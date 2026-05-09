/*
 * dll_loader.h
 * 
 * Header for embedded DLL loader
 */

#ifndef DLL_LOADER_H
#define DLL_LOADER_H

#include <windows.h>

/*
 * Extract and load the embedded situation.dll
 * Returns: Handle to the loaded DLL, or NULL on failure
 */
HMODULE LoadEmbeddedDLL(void);

/*
 * Unload the DLL and cleanup temp files
 */
void UnloadEmbeddedDLL(HMODULE hDLL);

#endif // DLL_LOADER_H
