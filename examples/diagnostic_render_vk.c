/*
 * Diagnostic Render Test - VULKAN
 * Tests if basic Vulkan rendering works end-to-end.
 */
#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"
#include <cglm/cglm.h>
#include <stdio.h>
#include <math.h>

int main(int argc, char** argv) {
    SituationInitInfo config = { 
        .window_title = "Vulkan Diagnostic",
        .window_width = 800, .window_height = 600,
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        printf("INIT FAILED: %s\n", err_msg ? err_msg : "unknown");
        printf("Press Enter to exit...\n");
        getchar();
        return -1;
    }
    
    printf("Vulkan init OK. Entering loop...\n");
    fflush(stdout);
    
    int frame = 0;
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;
        
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            
            // Phase 1: Just clear with cycling color
            float t = frame * 0.02f;
            ColorRGBA color = {
                (uint8_t)(128 + 127 * sinf(t)),
                (uint8_t)(50),
                (uint8_t)(128 + 127 * sinf(t + 2.0f)),
                255
            };
            
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = color } },
                .depth_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .depth = 1.0f } }
            };
            SituationCmdBeginRenderPass(cmd, &pass);
            
            // Phase 2: Draw a quad
            mat4 model;
            glm_mat4_identity(model);
            glm_translate(model, (vec3){300.0f, 200.0f, 0.0f});
            glm_scale(model, (vec3){200.0f, 200.0f, 1.0f});
            Vector4 quad_color = {{1.0f, 0.2f, 0.2f, 1.0f}}; // Red
            SituationCmdDrawQuad(cmd, model, quad_color);
            
            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
        
        frame++;
        if (frame % 60 == 0) {
            printf("Frame %d\n", frame);
            fflush(stdout);
        }
    }
    
    printf("Shutting down after %d frames\n", frame);
    SituationShutdown();
    return 0;
}
