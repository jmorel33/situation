// Simple quad test - just one colored quad
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <cglm/cglm.h>

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Simple Quad Test",
        .window_width = 800,
        .window_height = 600
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("Failed to initialize\n");
        return -1;
    }
    
    printf("Simple Quad Test - Press ESC to exit\n");
    fflush(stdout);
    
    int frame_count = 0;
    
    while (!SituationWindowShouldClose() && frame_count < 60) {
        SITUATION_BEGIN_FRAME();
        
        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { 
                    .loadOp = SIT_LOAD_OP_CLEAR, 
                    .clear = { .color = {50, 50, 50, 255} } 
                }
            };
            
            SituationCmdBeginRenderPass(cmd, &pass);
            
            // Draw one simple quad in the center
            mat4 model;
            glm_mat4_identity(model);
            glm_translate(model, (vec3){400.0f, 300.0f, 0.0f});
            glm_scale(model, (vec3){100.0f, 100.0f, 1.0f});
            
            Vector4 color = {{1.0f, 0.0f, 0.0f, 1.0f}}; // Red
            
            printf("Frame %d: Drawing quad\n", frame_count);
            fflush(stdout);
            SituationCmdDrawQuad(cmd, model, color);
            printf("Frame %d: Quad drawn, ending pass\n", frame_count);
            fflush(stdout);
            
            SituationError err = SituationCmdEndRenderPass(cmd);
            printf("Frame %d: EndRenderPass returned: %d\n", frame_count, err);
            fflush(stdout);
            
            printf("Frame %d: Calling EndFrame\n", frame_count);
            fflush(stdout);
            
            SituationError end_err = SituationEndFrame();
            printf("Frame %d: EndFrame returned: %d\n", frame_count, end_err);
            fflush(stdout);
            fflush(stdout);
            SituationEndFrame();
            printf("Frame %d: Complete!\n", frame_count);
            fflush(stdout);
        } else {
            printf("Frame %d: Failed to acquire command buffer\n", frame_count);
            fflush(stdout);
        }
        
        frame_count++;
    }
    
    printf("Shutting down after %d frames\n", frame_count);
    SituationShutdown();
    return 0;
}
