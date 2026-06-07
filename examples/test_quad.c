/*
 * Test Quad - Simple colored quad rendering test
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
    printf("  Test Quad - Vulkan Rendering Test\n");
    printf("========================================\n\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Test Quad - Vulkan"
    };
    
    printf("Initializing Vulkan backend...\n");
    fflush(stdout);
    
    int result = SituationInit(argc, argv, &config);
    
    if (result != SITUATION_SUCCESS) {
        printf("\n[ERROR] Initialization failed with code: %d\n", result);
        return -1;
    }
    
    printf("\n[SUCCESS] Vulkan window created!\n");
    printf("Starting render loop...\n\n");
    fflush(stdout);
    
    // Colors
    ColorRGBA bgColor = {30, 30, 40, 255};      // Dark blue-gray background
    ColorRGBA redColor = {255, 0, 0, 255};      // Red quad
    ColorRGBA greenColor = {0, 255, 0, 255};    // Green quad
    ColorRGBA blueColor = {0, 0, 255, 255};     // Blue quad
    
    // Main render loop
    int frame = 0;
    bool running = true;
    
    while (running && !SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            printf("ESC pressed - exiting...\n");
            running = false;
            break;
        }
        
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            printf("Failed to acquire frame buffer at frame %d\n", frame);
            break;
        }
        
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationCmdBeginRenderToDisplay(cmd, -1, bgColor);
        
        // Draw three colored quads using model matrices
        mat4 model1;
        glm_mat4_identity(model1);
        glm_translate(model1, (vec3){100.0f, 100.0f, 0.0f});
        glm_scale(model1, (vec3){150.0f, 150.0f, 1.0f});
        Vector4 red = {1.0f, 0.0f, 0.0f, 1.0f};
        SituationCmdDrawQuad(cmd, model1, red);
        
        mat4 model2;
        glm_mat4_identity(model2);
        glm_translate(model2, (vec3){325.0f, 100.0f, 0.0f});
        glm_scale(model2, (vec3){150.0f, 150.0f, 1.0f});
        Vector4 green = {0.0f, 1.0f, 0.0f, 1.0f};
        SituationCmdDrawQuad(cmd, model2, green);
        
        mat4 model3;
        glm_mat4_identity(model3);
        glm_translate(model3, (vec3){550.0f, 100.0f, 0.0f});
        glm_scale(model3, (vec3){150.0f, 150.0f, 1.0f});
        Vector4 blue = {0.0f, 0.0f, 1.0f, 1.0f};
        SituationCmdDrawQuad(cmd, model3, blue);
        
        SituationCmdEndRender(cmd);
        SituationEndFrame();
        
        frame++;
        
        if (frame % 60 == 0) {
            printf("Frame %d - FPS: %.1f - Quads rendering!\n", frame, SituationGetFPS());
            fflush(stdout);
        }
        
        if (frame > 300) break; // Auto-exit after 5 seconds
    }
    
    printf("\nShutting down...\n");
    fflush(stdout);
    
    SituationShutdown();
    
    printf("Shutdown complete!\n");
    return 0;
}
