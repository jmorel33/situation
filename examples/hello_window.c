/*
 * Hello Window - Vulkan window that stays open
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
    printf("  Vulkan Window Test\n");
    printf("========================================\n\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Hello Vulkan Window!"
    };
    
    printf("Initializing Vulkan...\n");
    int result = SituationInit(argc, argv, &config);
    
    if (result != SITUATION_SUCCESS) {
        printf("ERROR: Init failed with code %d\n", result);
        return -1;
    }
    
    printf("\n[SUCCESS] Window created!\n");
    printf("Press ESC or close window to exit\n\n");
    
    // Main loop - stays open until user closes
    int frame = 0;
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        // Check for ESC key
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            printf("ESC pressed, exiting...\n");
            break;
        }
        
        // Acquire frame
        if (!SituationAcquireFrameCommandBuffer()) {
            break;
        }
        
        // Animate color based on frame
        float t = (frame % 360) / 360.0f;
        ColorRGBA clearColor = {
            (uint8_t)(128 + 127 * sinf(t * 6.28f)),
            (uint8_t)(128 + 127 * sinf((t + 0.33f) * 6.28f)),
            (uint8_t)(128 + 127 * sinf((t + 0.67f) * 6.28f)),
            255
        };
        
        // Render
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationCmdBeginRenderToDisplay(cmd, -1, clearColor);
        SituationCmdEndRender(cmd);
        
        SituationEndFrame();
        frame++;
        
        // Print status every 60 frames
        if (frame % 60 == 0) {
            printf("Frame %d - FPS: %.1f\n", frame, SituationGetFPS());
        }
    }
    
    printf("\nRendered %d frames\n", frame);
    printf("Shutting down...\n");
    
    SituationShutdown();
    
    printf("Done!\n");
    return 0;
}
