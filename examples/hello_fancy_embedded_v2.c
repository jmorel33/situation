/*
 * hello_fancy_embedded_v2.c
 * 
 * Self-contained version that extracts and dynamically loads situation.dll
 * This version doesn't link against situation.dll at compile time
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

// Resource ID for the embedded DLL
#define IDR_SITUATION_DLL 101

// Function pointer types for Situation API functions we need
typedef int (*SituationInitFunc)(int, char**, void*);
typedef void (*SituationShutdownFunc)(void);
typedef int (*SituationWindowShouldCloseFunc)(void);
// Add more function pointers as needed...

int main(int argc, char** argv) {
    HRSRC hResource;
    HGLOBAL hMemory;
    DWORD dwSize;
    LPVOID lpAddress;
    HANDLE hFile;
    DWORD dwBytesWritten;
    char tempPath[MAX_PATH];
    char dllPath[MAX_PATH];
    HMODULE hDLL;
    
    printf("========================================\n");
    printf("  Hello Fancy - Fully Embedded Version\n");
    printf("========================================\n\n");

    // Find the embedded DLL resource
    printf("Extracting embedded situation.dll...\n");
    hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_SITUATION_DLL), RT_RCDATA);
    if (!hResource) {
        fprintf(stderr, "[ERROR] Failed to find embedded DLL\n");
        return -1;
    }

    hMemory = LoadResource(NULL, hResource);
    lpAddress = LockResource(hMemory);
    dwSize = SizeofResource(NULL, hResource);

    // Get temp directory and create DLL path
    GetTempPath(MAX_PATH, tempPath);
    snprintf(dllPath, MAX_PATH, "%s\\situation_%d.dll", tempPath, GetCurrentProcessId());

    // Write DLL to temp file
    hFile = CreateFile(dllPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
                       FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[ERROR] Failed to create temp DLL file\n");
        return -1;
    }

    WriteFile(hFile, lpAddress, dwSize, &dwBytesWritten, NULL);
    CloseHandle(hFile);

    // Load the DLL
    printf("Loading DLL from: %s\n", dllPath);
    hDLL = LoadLibrary(dllPath);
    if (!hDLL) {
        fprintf(stderr, "[ERROR] Failed to load DLL (Error: %lu)\n", GetLastError());
        DeleteFile(dllPath);
        return -1;
    }

    printf("DLL loaded successfully!\n\n");

    // Get function pointers
    SituationInitFunc SituationInit = (SituationInitFunc)GetProcAddress(hDLL, "SituationInit");
    // ... get more functions as needed

    if (!SituationInit) {
        fprintf(stderr, "[ERROR] Failed to get Situation functions\n");
        FreeLibrary(hDLL);
        DeleteFile(dllPath);
        return -1;
    }

    // Now run your application using the loaded functions
    printf("Running application...\n\n");
    
    // TODO: Call Situation functions here
    // For now, just a placeholder
    printf("[INFO] This is a template - integrate your hello_fancy code here\n");
    printf("[INFO] Use the function pointers to call Situation API\n");

    // Cleanup
    printf("\nCleaning up...\n");
    FreeLibrary(hDLL);
    DeleteFile(dllPath);

    return 0;
}
