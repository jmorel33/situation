// Spinning 3D Cube Demo - No external assets needed
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <cglm/cglm.h>
#include <math.h>

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Spinning 3D Cube",
        .window_width = 800,
        .window_height = 600
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("Failed to initialize\n");
        return -1;
    }
    
    printf("3D Cube Demo - Press ESC to exit\n");
    fflush(stdout);
    
    float rotation = 0.0f;
    int frame_count = 0;
    
    printf("Checking window state...\n");
    fflush(stdout);
    bool should_close = SituationWindowShouldClose();
    printf("Window should close: %d\n", should_close);
    fflush(stdout);
    
    printf("About to enter main loop...\n");
    fflush(stdout);
    
    while (!SituationWindowShouldClose()) {
        printf("In loop, frame %d\n", frame_count);
        fflush(stdout);
        
        printf("  Calling SITUATION_BEGIN_FRAME\n");
        fflush(stdout);
        SITUATION_BEGIN_FRAME();
        printf("  BEGIN_FRAME done\n");
        fflush(stdout);
        
        frame_count++;
        if (frame_count % 60 == 0) {
            printf("Frame %d\n", frame_count);
            fflush(stdout);
        }
        
        printf("  Getting frame time\n");
        fflush(stdout);
        rotation += 0.5f * SituationGetFrameTime();
        printf("  Rotation updated\n");
        fflush(stdout);
        
        printf("  Acquiring frame buffer\n");
        fflush(stdout);
        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            printf("  Frame buffer acquired\n");
            fflush(stdout);
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            
            printf("  Setting up render pass\n");
            fflush(stdout);
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { 
                    .loadOp = SIT_LOAD_OP_CLEAR, 
                    .clear = { .color = {20, 20, 30, 255} } 
                }
            };
            
            printf("  Beginning render pass\n");
            fflush(stdout);
            SituationCmdBeginRenderPass(cmd, &pass);
            printf("  Render pass begun\n");
            fflush(stdout);
            
            // Draw 6 quads to make a cube
            printf("  Drawing cube\n");
            fflush(stdout);
            for (int face = 0; face < 6; face++) {
                mat4 model;
                glm_mat4_identity(model);
                
                // Rotate the whole cube
                glm_rotate(model, rotation, (vec3){0.0f, 1.0f, 0.0f});
                glm_rotate(model, rotation * 0.7f, (vec3){1.0f, 0.0f, 0.0f});
                
                // Position each face
                vec3 offset = {0, 0, 0};
                vec3 rot_axis = {0, 0, 0};
                float rot_angle = 0;
                
                switch(face) {
                    case 0: offset[2] = 0.5f; break; // Front
                    case 1: offset[2] = -0.5f; rot_axis[1] = 1; rot_angle = GLM_PI; break; // Back
                    case 2: offset[0] = 0.5f; rot_axis[1] = 1; rot_angle = GLM_PI/2; break; // Right
                    case 3: offset[0] = -0.5f; rot_axis[1] = 1; rot_angle = -GLM_PI/2; break; // Left
                    case 4: offset[1] = 0.5f; rot_axis[0] = 1; rot_angle = -GLM_PI/2; break; // Top
                    case 5: offset[1] = -0.5f; rot_axis[0] = 1; rot_angle = GLM_PI/2; break; // Bottom
                }
                
                glm_translate(model, offset);
                if (rot_angle != 0) glm_rotate(model, rot_angle, rot_axis);
                glm_scale(model, (vec3){0.3f, 0.3f, 0.01f});
                
                // Different color per face
                Vector4 colors[6] = {
                    {{1.0f, 0.0f, 0.0f, 1.0f}}, // Red
                    {{0.0f, 1.0f, 0.0f, 1.0f}}, // Green
                    {{0.0f, 0.0f, 1.0f, 1.0f}}, // Blue
                    {{1.0f, 1.0f, 0.0f, 1.0f}}, // Yellow
                    {{1.0f, 0.0f, 1.0f, 1.0f}}, // Magenta
                    {{0.0f, 1.0f, 1.0f, 1.0f}}  // Cyan
                };
                
                SituationCmdDrawQuad(cmd, model, colors[face]);
            }
            printf("  Cube drawn\n");
            fflush(stdout);
            
            printf("  Ending render pass\n");
            fflush(stdout);
            SituationCmdEndRenderPass(cmd);
            printf("  Calling EndFrame\n");
            fflush(stdout);
            SituationEndFrame();
            printf("  EndFrame done\n");
            fflush(stdout);
        }
        printf("  End of loop iteration\n");
        fflush(stdout);
    }
    
    printf("Shutting down...\n");
    SituationShutdown();
    return 0;
}
