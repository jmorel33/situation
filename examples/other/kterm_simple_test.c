// Simple K-Term test - based on working kterm_console pattern
#define SITUATION_USE_OPENGL
#define KTERM_IMPLEMENTATION
#include "situation.h"
#include "../sit/k-term/kterm.h"

#include <stdio.h>

int main(int argc, char** argv) {
    printf("K-Term Simple Test\n");
    
    // Init Situation
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 500,
        .window_title = "K-Term Simple Test",
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,
        .enable_vulkan_validation = false
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("ERROR: Situation init failed\n");
        return -1;
    }
    
    printf("Situation initialized\n");
    
    // Create K-Term
    KTermConfig kterm_config = {
        .width = 80,
        .height = 50
    };
    
    KTerm* term = KTerm_Create(kterm_config);
    if (!term) {
        printf("ERROR: KTerm creation failed\n");
        SituationShutdown();
        return -1;
    }
    
    printf("K-Term created\n");
    
    // Write test content
    KTerm_WriteString(term, "\x1B[2J\x1B[H");  // Clear and home
    KTerm_WriteString(term, "\x1B[1;1H\x1B[1;33mHELLO WORLD!\x1B[0m\n");
    KTerm_WriteString(term, "This is a test of K-Term rendering.\n");
    KTerm_WriteString(term, "Press ESC to exit.\n");
    
    printf("Content written, entering main loop\n");
    
    // Main loop - exactly like kterm_console
    int frame = 0;
    while (!SituationWindowShouldClose() && frame < 300) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;
        
        // Update and draw - exactly like kterm_console
        KTerm_Update(term);
        KTerm_Draw(term);
        
        frame++;
    }
    
    printf("Shutting down after %d frames\n", frame);
    KTerm_Destroy(term);
    SituationShutdown();
    
    return 0;
}
