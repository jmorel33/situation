/*
 * Minimal Situation Example
 * Tests basic initialization and window creation
 * Supports both OpenGL and Vulkan backends
 */

#if defined(_WIN32)
    // Don't include winsock2.h here - let situation.h handle it
    #define NOMINMAX   // Prevent min/max macro conflicts
#endif


// Backend selection (define one via compiler flag)
#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL  // Default to OpenGL if not specified
#endif

#define SITUATION_ENABLE_THREADING
#include "situation.h"

int main(int argc, char** argv) {
    // Initialize Situation
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Minimal Situation Test"
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize Situation\n");
        return -1;
    }
    
    printf("Situation initialized successfully!\n");
    printf("Window: %dx%d\n", config.window_width, config.window_height);
    
    // Main loop - run for a few frames then exit
    int frame_count = 0;
    while (!SituationWindowShouldClose() && frame_count < 60) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        // Acquire frame
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            break;
        }
        
        // Clear screen to a nice blue color (RGBA: 0-255)
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        ColorRGBA clearColor = {51, 76, 204, 255};  // Blue color
        SituationCmdBeginRenderToDisplay(cmd, -1, clearColor);
        SituationCmdEndRender(cmd);
        
        SituationEndFrame();
        frame_count++;
    }
    
    printf("Ran %d frames successfully!\n", frame_count);
    
    SituationShutdown();
    printf("Shutdown complete. Build system works!\n");
    
    return 0;
}
