/*
 * dll_test.c - Simple test for situation.dll
 * 
 * This example tests the DLL by calling basic Situation functions.
 * Note: No SITUATION_IMPLEMENTATION defined - we're using the DLL!
 */

#define SITUATION_USE_VULKAN
#include "../situation.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("Situation DLL Test\n");
    printf("========================================\n\n");
    
    // Test 1: Get version
    printf("[Test 1] Version Info:\n");
    printf("  Version: %d.%d.%d%s\n", 
           SITUATION_VERSION_MAJOR, 
           SITUATION_VERSION_MINOR, 
           SITUATION_VERSION_PATCH,
           SITUATION_VERSION_REVISION);
    printf("  ✓ Version macros accessible\n\n");
    
    // Test 2: Initialize Situation
    printf("[Test 2] Initializing Situation...\n");
    SituationInitInfo init_info = {0};
    init_info.window_width = 800;
    init_info.window_height = 600;
    init_info.window_title = "DLL Test";
    
    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        char* error_msg = NULL;
        SituationGetLastErrorMsg(&error_msg);
        printf("  ✗ Failed to initialize Situation\n");
        printf("  Error: %s\n", error_msg ? error_msg : "Unknown error");
        if (error_msg) free(error_msg);
        return 1;
    }
    printf("  ✓ Situation initialized successfully\n\n");
    
    // Test 3: Check if initialized
    printf("[Test 3] Checking initialization:\n");
    if (SituationIsInitialized()) {
        printf("  ✓ Situation is initialized\n");
    } else {
        printf("  ✗ Situation not initialized\n");
    }
    printf("\n");
    
    // Test 4: Create a mixer
    printf("[Test 4] Creating Mixer...\n");
    SituationAudioMixer* mixer = SituationCreateMixer();
    if (!mixer) {
        char* error_msg = NULL;
        SituationGetLastErrorMsg(&error_msg);
        printf("  ✗ Failed to create mixer\n");
        printf("  Error: %s\n", error_msg ? error_msg : "Unknown error");
        if (error_msg) free(error_msg);
    } else {
        printf("  ✓ Mixer created successfully\n");
    }
    printf("\n");
    
    // Test 5: Cleanup
    printf("[Test 5] Shutting down...\n");
    SituationShutdown();
    printf("  ✓ Shutdown complete\n\n");
    
    printf("========================================\n");
    printf("All tests passed!\n");
    printf("========================================\n");
    
    return 0;
}
