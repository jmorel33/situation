/*
 * Debug Vulkan Test - Detailed logging to find crash point
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"
#include <stdio.h>

#define LOG(msg) do { printf("[DEBUG] %s\n", msg); fflush(stdout); } while(0)
#define LOGF(fmt, ...) do { printf("[DEBUG] " fmt "\n", __VA_ARGS__); fflush(stdout); } while(0)

int main(int argc, char** argv) {
    LOG("========================================");
    LOG("  Situation Vulkan Debug Test");
    LOG("========================================");
    LOG("");
    
    LOG("Step 1: Setting up init config...");
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Vulkan Debug Test"
    };
    LOG("Config created");
    
    LOG("Step 2: Calling SituationInit...");
    int result = SituationInit(argc, argv, &config);
    
    if (result != SITUATION_SUCCESS) {
        LOGF("[ERROR] SituationInit failed with code: %d", result);
        return -1;
    }
    
    LOG("[SUCCESS] Vulkan initialized!");
    LOGF("Window: %dx%d", config.window_width, config.window_height);
    LOG("");
    
    LOG("Step 3: Entering main loop (5 frames)...");
    int frame_count = 0;
    int max_frames = 5;
    
    while (!SituationWindowShouldClose() && frame_count < max_frames) {
        LOGF("Frame %d: Polling events...", frame_count);
        SituationPollInputEvents();
        
        LOGF("Frame %d: Updating timers...", frame_count);
        SituationUpdateTimers();
        
        LOGF("Frame %d: Acquiring frame buffer...", frame_count);
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            LOG("[ERROR] Failed to acquire frame buffer");
            break;
        }
        
        LOGF("Frame %d: Getting command buffer...", frame_count);
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        
        LOGF("Frame %d: Setting clear color...", frame_count);
        ColorRGBA clearColor = {100, 150, 200, 255};
        
        LOGF("Frame %d: Beginning render...", frame_count);
        SituationCmdBeginRenderToDisplay(cmd, -1, clearColor);
        
        LOGF("Frame %d: Ending render...", frame_count);
        SituationCmdEndRender(cmd);
        
        LOGF("Frame %d: Ending frame...", frame_count);
        SituationEndFrame();
        
        LOGF("Frame %d: Complete!", frame_count);
        frame_count++;
    }
    
    LOG("");
    LOGF("[SUCCESS] Rendered %d frames!", frame_count);
    
    LOG("Step 4: Shutting down...");
    SituationShutdown();
    LOG("Shutdown complete.");
    
    LOG("");
    LOG("========================================");
    LOG("  Test completed successfully!");
    LOG("========================================");
    
    return 0;
}
