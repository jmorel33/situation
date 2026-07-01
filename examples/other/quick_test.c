/*
 * Quick Vulkan Init Test - Just init and shutdown
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_USE_VULKAN
// Disable threading to simplify
// #define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("Quick Vulkan Init Test\n");
    printf("======================\n\n");
    
    SituationInitInfo config = {
        .window_width = 640,
        .window_height = 480,
        .window_title = "Quick Test"
    };
    
    printf("Calling SituationInit...\n");
    fflush(stdout);
    
    int result = SituationInit(argc, argv, &config);
    
    if (result != SITUATION_SUCCESS) {
        printf("FAILED: SituationInit returned %d\n", result);
        return -1;
    }
    
    printf("SUCCESS: Vulkan initialized!\n");
    printf("Calling SituationShutdown...\n");
    fflush(stdout);
    
    SituationShutdown();
    
    printf("Done!\n");
    
    return 0;
}
