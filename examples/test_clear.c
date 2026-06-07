/*
 * Test Clear - Simple Vulkan Clear Screen Test
 * Just clears the screen to a color to verify rendering works
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  Vulkan Clear Screen Test\n");
    printf("========================================\n\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Vulkan Clear Test"
    };
    
    printf("Initializing Vulkan backend...\n");
    fflush(stdout);
    
    int result = SituationInit(argc, argv, &config);
    
    if (result != SITUATION_SUCCESS) {
        printf("\n[ERROR] Initialization failed with code: %d\n", result);
        printf("Press Enter to exit...\n");
        getchar();
        return -1;
    }
    
    printf("\n[SUCCESS] Vulkan initialized!\n");
    printf("Starting render loop...\n\n");
    fflush(stdout);
    
    // Colors - cycle through different colors
    ColorRGBA colors[] = {
        {255, 0, 0, 255},    // Red
        {0, 255, 0, 255},    // Green
        {0, 0, 255, 255},    // Blue
        {255, 255, 0, 255},  // Yellow
        {255, 0, 255, 255},  // Magenta
        {0, 255, 255, 255},  // Cyan
    };
    int colorIndex = 0;
    int framesSinceColorChange = 0;
    
    // Main render loop
    int frame = 0;
    bool running = true;
    
    while (running && !SituationWindowShouldClose()) {
        // Poll events
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        // Check for exit
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            printf("ESC pressed - exiting...\n");
            running = false;
            break;
        }
        
        // Change color every 60 frames (1 second at 60 FPS)
        framesSinceColorChange++;
        if (framesSinceColorChange >= 60) {
            colorIndex = (colorIndex + 1) % 6;
            framesSinceColorChange = 0;
            printf("Switching to color %d\n", colorIndex);
        }
        
        // Acquire frame
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            printf("Failed to acquire frame buffer at frame %d\n", frame);
            break;
        }
        
        // Begin rendering - just clear to color
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationCmdBeginRenderToDisplay(cmd, -1, colors[colorIndex]);
        
        // End rendering (no drawing, just clear)
        SituationCmdEndRender(cmd);
        SituationEndFrame();
        
        frame++;
        
        // Print status every 60 frames
        if (frame % 60 == 0) {
            printf("Frame %d - FPS: %.1f - Rendering!\n", frame, SituationGetFPS());
            fflush(stdout);
        }
        
        // Exit after 10 seconds for testing
        if (frame >= 600) {
            printf("Test complete after 10 seconds\n");
            break;
        }
    }
    
    printf("\nShutting down...\n");
    fflush(stdout);
    
    SituationShutdown();
    
    printf("Shutdown complete!\n");
    return 0;
}
