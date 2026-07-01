// Window debug test - check why window closes immediately
#define SITUATION_USE_OPENGL
#include "situation.h"

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Window Debug Test",
        .window_width = 800,
        .window_height = 600
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("Failed to initialize\n");
        return -1;
    }
    
    printf("Initialization complete\n");
    printf("Checking initial window state...\n");
    fflush(stdout);
    
    bool should_close = SituationWindowShouldClose();
    printf("Initial SituationWindowShouldClose: %d\n", should_close);
    fflush(stdout);
    
    printf("Entering main loop...\n");
    fflush(stdout);
    
    int frame_count = 0;
    while (!SituationWindowShouldClose() && frame_count < 10) {
        printf("Frame %d start\n", frame_count);
        fflush(stdout);
        
        SITUATION_BEGIN_FRAME();
        
        printf("Frame %d: After BEGIN_FRAME\n", frame_count);
        fflush(stdout);
        
        should_close = SituationWindowShouldClose();
        printf("Frame %d: SituationWindowShouldClose = %d\n", frame_count, should_close);
        fflush(stdout);
        
        // Check if ESC is pressed
        bool esc_pressed = SituationIsKeyPressed(256); // GLFW_KEY_ESCAPE = 256
        printf("Frame %d: ESC pressed = %d\n", frame_count, esc_pressed);
        fflush(stdout);
        
        printf("Frame %d: About to call SituationAcquireFrameCommandBuffer\n", frame_count);
        fflush(stdout);
        
        bool acquired = (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
        printf("Frame %d: SituationAcquireFrameCommandBuffer returned %d\n", frame_count, acquired);
        fflush(stdout);
        
        if (acquired) {
            printf("Frame %d: Getting command buffer\n", frame_count);
            fflush(stdout);
            
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            printf("Frame %d: Got command buffer: %p\n", frame_count, cmd);
            fflush(stdout);
            
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { 
                    .loadOp = SIT_LOAD_OP_CLEAR, 
                    .clear = { .color = {50, 100, 150, 255} } 
                }
            };
            
            printf("Frame %d: Beginning render pass\n", frame_count);
            fflush(stdout);
            
            SituationCmdBeginRenderPass(cmd, &pass);
            
            printf("Frame %d: Ending render pass\n", frame_count);
            fflush(stdout);
            
            SituationCmdEndRenderPass(cmd);
            
            printf("Frame %d: Calling EndFrame\n", frame_count);
            fflush(stdout);
            
            SituationEndFrame();
            
            printf("Frame %d: EndFrame complete\n", frame_count);
            fflush(stdout);
        }
        
        printf("Frame %d complete\n", frame_count);
        fflush(stdout);
        
        frame_count++;
    }
    
    printf("Exited loop after %d frames\n", frame_count);
    printf("Final SituationWindowShouldClose: %d\n", SituationWindowShouldClose());
    fflush(stdout);
    
    SituationShutdown();
    return 0;
}
