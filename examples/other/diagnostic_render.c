/*
 * Diagnostic Render Test
 * Tests if basic OpenGL rendering works at all.
 * Bypasses the soft command buffer to isolate the issue.
 */
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <cglm/cglm.h>
#include <stdio.h>
#include <math.h>

int main(int argc, char** argv) {
    SituationInitInfo config = { 
        .window_title = "Diagnostic Render",
        .window_width = 800, .window_height = 600 
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        printf("INIT FAILED: %s\n", err_msg ? err_msg : "unknown");
        return -1;
    }
    
    printf("Init OK. Entering loop...\n");
    fflush(stdout);
    
    int frame = 0;
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;
        
        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            
            // Clear to dark blue
            ColorRGBA color = { 20, 20, 60, 255 };
            
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = color } },
                .depth_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .depth = 1.0f } }
            };
            SituationCmdBeginRenderPass(cmd, &pass);
            
            // Draw a quad in the center
            mat4 model;
            glm_mat4_identity(model);
            glm_translate(model, (vec3){300.0f, 200.0f, 0.0f});
            glm_scale(model, (vec3){200.0f, 200.0f, 1.0f});
            
            Vector4 quad_color = {{1.0f, 0.0f, 0.0f, 1.0f}}; // Bright red
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
