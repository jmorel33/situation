/*
 * Clean Vulkan Window - No debug logging
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
    printf("Vulkan Window Demo\n");
    printf("==================\n\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Situation Vulkan - Windows GCC Build"
    };
    
    printf("Initializing...\n");
    fflush(stdout);
    
    int result = SituationInit(argc, argv, &config);
    
    if (result != SITUATION_SUCCESS) {
        printf("ERROR: Init failed\n");
        return -1;
    }
    
    printf("SUCCESS! Window created.\n");
    printf("Press ESC to exit\n\n");
    fflush(stdout);
    
    // Main loop
    int frame = 0;
    while (!SituationWindowShouldClose() && frame < 600) {  // 10 seconds at 60fps
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }
        
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            break;
        }
        
        // Rainbow effect
        float t = frame / 60.0f;
        ColorRGBA color = {
            (uint8_t)(128 + 127 * sinf(t)),
            (uint8_t)(128 + 127 * sinf(t + 2.0f)),
            (uint8_t)(128 + 127 * sinf(t + 4.0f)),
            255
        };
        
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationCmdBeginRenderToDisplay(cmd, -1, color);
        SituationCmdEndRender(cmd);
        SituationEndFrame();
        
        frame++;
    }
    
    printf("\nShutting down...\n");
    SituationShutdown();
    printf("Done! Rendered %d frames\n", frame);
    
    return 0;
}
