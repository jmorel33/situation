// Minimal test - just init and one frame
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "../situation.h"

int main(int argc, char** argv) {
    printf("=== MINIMAL TEST ===\n");
    fflush(stdout);
    
    SituationInitInfo config = {
        .window_title = "Minimal Test",
        .window_width = 800,
        .window_height = 600
    };
    
    printf("Calling SituationInit...\n");
    fflush(stdout);
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("Failed to initialize\n");
        return -1;
    }
    printf("Init complete\n");
    fflush(stdout);
    
    printf("Entering main loop...\n");
    fflush(stdout);
    
    int frame_count = 0;
    while (!SituationWindowShouldClose() && frame_count < 300) {  // Run for 300 frames (~5 seconds at 60fps)
        printf("Frame %d: Acquiring...\n", frame_count);
        fflush(stdout);
        
        if (SituationAcquireFrameCommandBuffer()) {
            printf("Frame %d: Acquired, ending...\n", frame_count);
            fflush(stdout);
            
            SituationEndFrame();
            
            printf("Frame %d: Complete!\n", frame_count);
            fflush(stdout);
        }
        
        frame_count++;
    }
    
    printf("Exited loop after %d frames\n", frame_count);
    fflush(stdout);
    
    printf("Shutting down...\n");
    fflush(stdout);
    SituationShutdown();
    printf("Shutdown complete!\n");
    fflush(stdout);
    return 0;
}
