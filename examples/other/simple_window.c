/*
 * Simple Window - No Text Rendering
 * Just displays a colored window to test basic Vulkan functionality
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
// NOTE: Commenting out shader compiler to skip quad renderer init
// #define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  Simple Window - Vulkan Test\n");
    printf("========================================\n\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Simple Vulkan Window"
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
    printf("Press ESC or close window to exit\n\n");
    fflush(stdout);
    
    // Main render loop
    int frame = 0;
    
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }
        
        // Just clear the screen - no rendering
        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            ColorRGBA clearColor = {30, 30, 40, 255};
            SituationCmdBeginRenderToDisplay(cmd, -1, clearColor);
            SituationCmdEndRender(cmd);
            SituationEndFrame();
        }
        
        frame++;
        if (frame % 60 == 0) {
            printf("Frame %d\n", frame);
            fflush(stdout);
        }
    }
    
    printf("\nRendered %d frames\n", frame);
    SituationShutdown();
    
    return 0;
}
