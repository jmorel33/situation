/*
 * dll_loader.c
 * 
 * Extracts embedded situation.dll from resources and loads it
 * This allows distributing a single .exe file
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// Resource ID for the embedded DLL
#define IDR_SITUATION_DLL 101

/*
 * Extract the embedded DLL to a temporary location and load it
 * Returns: Handle to the loaded DLL, or NULL on failure
 */
HMODULE LoadEmbeddedDLL(void) {
    HRSRC hResource;
    HGLOBAL hMemory;
    DWORD dwSize;
    LPVOID lpAddress;
    HANDLE hFile;
    DWORD dwBytesWritten;
    char tempPath[MAX_PATH];
    char dllPath[MAX_PATH];
    HMODULE hDLL;

    // Find the embedded DLL resource
    hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_SITUATION_DLL), RT_RCDATA);
    if (!hResource) {
        fprintf(stderr, "Failed to find embedded DLL resource\n");
        return NULL;
    }

    // Load the resource
    hMemory = LoadResource(NULL, hResource);
    if (!hMemory) {
        fprintf(stderr, "Failed to load DLL resource\n");
        return NULL;
    }

    // Get pointer to resource data
    lpAddress = LockResource(hMemory);
    dwSize = SizeofResource(NULL, hResource);

    if (!lpAddress || dwSize == 0) {
        fprintf(stderr, "Failed to lock DLL resource\n");
        return NULL;
    }

    // Get temp directory
    if (GetTempPath(MAX_PATH, tempPath) == 0) {
        fprintf(stderr, "Failed to get temp path\n");
        return NULL;
    }

    // Create path for extracted DLL
    snprintf(dllPath, MAX_PATH, "%s\\situation.dll", tempPath);

    // Write DLL to temp file
    hFile = CreateFile(dllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
                       FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to create temp DLL file: %s\n", dllPath);
        return NULL;
    }

    if (!WriteFile(hFile, lpAddress, dwSize, &dwBytesWritten, NULL) || 
        dwBytesWritten != dwSize) {
        fprintf(stderr, "Failed to write DLL to temp file\n");
        CloseHandle(hFile);
        DeleteFile(dllPath);
        return NULL;
    }

    CloseHandle(hFile);

    // Load the DLL from temp location
    hDLL = LoadLibrary(dllPath);
    if (!hDLL) {
        fprintf(stderr, "Failed to load DLL from: %s (Error: %lu)\n", 
                dllPath, GetLastError());
        DeleteFile(dllPath);
        return NULL;
    }

    printf("Successfully loaded embedded situation.dll from: %s\n", dllPath);
    
    // Note: We don't delete the file here because the DLL is still loaded
    // It will be cleaned up when the process exits or you can delete it
    // after FreeLibrary() is called

    return hDLL;
}

/*
 * Cleanup: Unload DLL and delete temp file
 */
void UnloadEmbeddedDLL(HMODULE hDLL) {
    if (hDLL) {
        char dllPath[MAX_PATH];
        char tempPath[MAX_PATH];
        
        // Get the DLL path before unloading
        GetTempPath(MAX_PATH, tempPath);
        snprintf(dllPath, MAX_PATH, "%s\\situation.dll", tempPath);
        
        // Unload the DLL
        FreeLibrary(hDLL);
        
        // Delete the temp file
        DeleteFile(dllPath);
    }
}
