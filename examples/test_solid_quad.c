/*
 * Test Solid Quad - Verify rendering works without textures
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_USE_VULKAN
#define SITUATION_VULKAN_DEBUG

#include "situation.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("Testing solid quad rendering...\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Solid Quad Test"
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("Init failed\n");
        return -1;
    }
    
    printf("Initialized! Press ESC to exit.\n");
    
    int frame = 0;
    while (!SituationWindowShouldClose() && frame < 600) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;
        
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) break;
        
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        
        SituationRenderPassInfo pass_info = {
            .display_id = -1,
            .color_attachment = {
                .loadOp = SIT_LOAD_OP_CLEAR,
                .storeOp = SIT_STORE_OP_STORE,
                .clear = { .color = {30, 30, 40, 255} }
            },
            .depth_attachment = {
                .loadOp = SIT_LOAD_OP_CLEAR,
                .storeOp = SIT_STORE_OP_DONT_CARE,
                .clear = { .depth = 1.0f }
            }
        };
        
        SituationCmdBeginRenderPass(cmd, &pass_info);
        
        // Draw a solid white quad in the center
        mat4 model;
        glm_mat4_identity(model);
        glm_translate(model, (vec3){400.0f, 300.0f, 0.0f});
        glm_scale(model, (vec3){200.0f, 100.0f, 1.0f});
        
        ColorRGBA white = {255, 255, 255, 255};
        SituationCmdDrawQuad(cmd, model, white);
        
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
        
        frame++;
        if (frame % 60 == 0) {
            printf("Frame %d\n", frame);
        }
    }
    
    SituationShutdown();
    return 0;
}
