/*
 * Simple Clear Test - Just clear the screen to a color
 * No text rendering, just basic rendering
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
    printf("  Simple Clear Test - Vulkan\n");
    printf("========================================\n\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Simple Clear Test"
    };
    
    printf("Initializing...\n");
    fflush(stdout);
    
    int result = SituationInit(argc, argv, &config);
    
    if (result != SITUATION_SUCCESS) {
        printf("\n[ERROR] Initialization failed with code: %d\n", result);
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        if (err_msg) {
            printf("[ERROR] Message: %s\n", err_msg);
            SituationFreeString(err_msg);
        }
        return -1;
    }
    
    printf("\n[SUCCESS] Initialized!\n");
    printf("Starting render loop (will run for 180 frames / 3 seconds)...\n\n");
    fflush(stdout);
    
    // Dark blue-gray background
    ColorRGBA bgColor = {30, 30, 40, 255};
    
    // Run for exactly 180 frames (3 seconds at 60 FPS)
    for (int frame = 0; frame < 180; frame++) {
        // Poll events
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        // Check for exit
        if (SituationWindowShouldClose() || SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            printf("Exit requested at frame %d\n", frame);
            break;
        }
        
        // Acquire frame
        printf("[Frame %d] Acquiring frame...\n", frame);
        fflush(stdout);
        if (!SituationAcquireFrameCommandBuffer()) {
            printf("Failed to acquire frame buffer at frame %d\n", frame);
            break;
        }
        printf("[Frame %d] Frame acquired\n", frame);
        fflush(stdout);
        
        // Begin rendering
        printf("[Frame %d] Getting command buffer...\n", frame);
        fflush(stdout);
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        printf("[Frame %d] Command buffer: %p\n", frame, (void*)cmd);
        fflush(stdout);
        
        printf("[Frame %d] Beginning render pass...\n", frame);
        fflush(stdout);
        SituationError err = SituationCmdBeginRenderToDisplay(cmd, -1, bgColor);
        printf("[Frame %d] Begin render result: %d\n", frame, err);
        fflush(stdout);
        
        // NO DRAWING - just clear
        
        // End rendering
        printf("[Frame %d] Ending render pass...\n", frame);
        fflush(stdout);
        SituationCmdEndRender(cmd);
        printf("[Frame %d] Ending frame...\n", frame);
        fflush(stdout);
        SituationEndFrame();
        printf("[Frame %d] Frame complete\n", frame);
        fflush(stdout);
        
        // Print status every 60 frames
        if (frame % 60 == 0) {
            printf("Frame %d - FPS: %.1f\n", frame, SituationGetFPS());
            fflush(stdout);
        }
    }
    
    printf("\nShutting down...\n");
    fflush(stdout);
    
    SituationShutdown();
    
    printf("Done!\n");
    return 0;
}
