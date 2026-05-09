// Minimal K-Term test - just display "HELLO WORLD"
#define SITUATION_USE_SHARED
#include "../situation.h"
#include "../sit/k-term/kterm_api.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("K-Term Minimal Test\n");
    
    // Init Situation
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 500,
        .window_title = "K-Term Minimal Test",
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,
        .enable_vulkan_validation = false
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("ERROR: Situation init failed\n");
        return -1;
    }
    
    // Create K-Term
    KTermConfig kterm_config = {
        .width = 80,
        .height = 50,
        .response_callback = NULL,
        .max_sixel_width = 0,
        .max_sixel_height = 0,
        .max_kitty_image_pixels = 0,
        .max_ops_per_flush = 0,
        .strict_mode = false
    };
    
    KTerm* term = KTerm_Create(kterm_config);
    if (!term) {
        printf("ERROR: KTerm creation failed\n");
        SituationShutdown();
        return -1;
    }
    
    printf("K-Term created successfully\n");
    
    // Write "HELLO WORLD" in big letters
    KTerm_WriteString(term, "\x1B[2J\x1B[H");  // Clear and home
    KTerm_WriteString(term, "\x1B[1;1H\x1B[1;33mHELLO WORLD!\x1B[0m");
    KTerm_WriteString(term, "\x1B[3;1HThis is a test of K-Term rendering.");
    KTerm_WriteString(term, "\x1B[5;1HPress ESC to exit.");
    
    printf("Text written, entering main loop\n");
    
    // Main loop
    int frame = 0;
    while (!SituationWindowShouldClose() && frame < 300) {  // Max 300 frames for testing
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;
        
        // Update and draw
        KTerm_Update(term);
        KTerm_Draw(term);
        
        frame++;
    }
    
    printf("Shutting down after %d frames\n", frame);
    KTerm_Destroy(term);
    SituationShutdown();
    
    return 0;
}
