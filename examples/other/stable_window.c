/*
 * Stable Vulkan Window - Simplified render loop
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
    printf("  Situation Vulkan - Windows Build\n");
    printf("========================================\n\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Vulkan Window - Press ESC to Exit"
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
    
    printf("\n[SUCCESS] Vulkan window created!\n");
    printf("Window size: %dx%d\n", config.window_width, config.window_height);
    printf("\nControls:\n");
    printf("  ESC - Exit\n");
    printf("  Close window - Exit\n\n");
    printf("Entering render loop...\n\n");
    fflush(stdout);
    
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
        
        // Try to acquire frame
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            printf("Failed to acquire frame buffer at frame %d\n", frame);
            break;
        }
        
        // Simple color animation
        float t = (frame % 360) / 360.0f;
        ColorRGBA clearColor = {
            (uint8_t)(100 + 155 * t),
            (uint8_t)(150 - 50 * t),
            (uint8_t)(200),
            255
        };
        
        // Render
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationCmdBeginRenderToDisplay(cmd, -1, clearColor);
        SituationCmdEndRender(cmd);
        
        // End frame
        SituationEndFrame();
        
        frame++;
        
        // Print status every 60 frames (1 second at 60fps)
        if (frame % 60 == 0) {
            printf("Frame %d - FPS: %.1f\n", frame, SituationGetFPS());
            fflush(stdout);
        }
    }
    
    printf("\n========================================\n");
    printf("Rendered %d frames total\n", frame);
    printf("Shutting down...\n");
    fflush(stdout);
    
    SituationShutdown();
    
    printf("Shutdown complete!\n");
    printf("========================================\n\n");
    
    return 0;
}
