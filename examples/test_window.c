/*
 * Simple Window Test - Minimal Vulkan Demo
 * Just opens a window with a colored background
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  Situation Vulkan Window Test\n");
    printf("========================================\n\n");
    
    // Initialize Situation with Vulkan backend
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Vulkan Test Window"
    };
    
    printf("Initializing Situation with Vulkan...\n");
    int result = SituationInit(argc, argv, &config);
    
    if (result != SITUATION_SUCCESS) {
        fprintf(stderr, "\n[ERROR] Failed to initialize Situation\n");
        fprintf(stderr, "Error code: %d\n", result);
        fprintf(stderr, "\nPossible causes:\n");
        fprintf(stderr, "- Vulkan drivers not installed\n");
        fprintf(stderr, "- Graphics card doesn't support Vulkan\n");
        fprintf(stderr, "- Vulkan SDK validation layers missing\n");
        return -1;
    }
    
    printf("\n[SUCCESS] Vulkan initialized!\n");
    printf("Window: %dx%d\n", config.window_width, config.window_height);
    printf("\nWindow should appear now...\n");
    printf("It will run for 3 seconds then close automatically.\n\n");
    
    // Main loop - run for 180 frames (3 seconds at 60fps)
    int frame_count = 0;
    int max_frames = 180;
    
    while (!SituationWindowShouldClose() && frame_count < max_frames) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        // Acquire frame
        if (!SituationAcquireFrameCommandBuffer()) {
            fprintf(stderr, "Failed to acquire frame buffer\n");
            break;
        }
        
        // Clear screen - cycle through colors
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        float t = (float)frame_count / (float)max_frames;
        
        ColorRGBA clearColor = {
            (uint8_t)(100 + 155 * t),      // Red increases
            (uint8_t)(150 - 100 * t),      // Green decreases  
            (uint8_t)(200),                // Blue constant
            255
        };
        
        SituationCmdBeginRenderToDisplay(cmd, -1, clearColor);
        SituationCmdEndRender(cmd);
        
        SituationEndFrame();
        frame_count++;
        
        // Print progress every 60 frames
        if (frame_count % 60 == 0) {
            printf("Frame %d/%d rendered\n", frame_count, max_frames);
        }
    }
    
    printf("\n[SUCCESS] Rendered %d frames!\n", frame_count);
    
    SituationShutdown();
    printf("Shutdown complete.\n\n");
    printf("========================================\n");
    printf("  Test completed successfully!\n");
    printf("========================================\n");
    
    return 0;
}
