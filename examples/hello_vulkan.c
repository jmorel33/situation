/*
 * Hello World Vulkan Demo
 * Creates a window with text rendering
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"

int main(int argc, char** argv) {
    // Initialize Situation with Vulkan backend
    SituationInitInfo config = {
        .window_width = 1024,
        .window_height = 768,
        .window_title = "Hello Vulkan - Situation Library Demo"
    };
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize Situation with Vulkan\n");
        return -1;
    }
    
    printf("===========================================\n");
    printf("  Vulkan Backend Initialized Successfully!\n");
    printf("===========================================\n");
    printf("Window: %dx%d\n", config.window_width, config.window_height);
    printf("Press ESC or close window to exit\n\n");
    
    // Load a font for text rendering
    SituationFont font = SituationLoadFont("C:\\Windows\\Fonts\\arial.ttf", 32);
    if (!font) {
        printf("Warning: Could not load font, text rendering disabled\n");
    }
    
    // Main loop
    int frame_count = 0;
    float hue = 0.0f;
    
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        // Check for ESC key
        if (SituationIsKeyPressed(SITUATION_KEY_ESCAPE)) {
            break;
        }
        
        // Acquire frame
        if (!SituationAcquireFrameCommandBuffer()) {
            break;
        }
        
        // Animate background color (HSV to RGB)
        hue += 0.5f;
        if (hue > 360.0f) hue = 0.0f;
        
        float h = hue / 60.0f;
        float c = 0.5f;  // Saturation
        float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
        float r = 0, g = 0, b = 0;
        
        if (h < 1) { r = c; g = x; }
        else if (h < 2) { r = x; g = c; }
        else if (h < 3) { g = c; b = x; }
        else if (h < 4) { g = x; b = c; }
        else if (h < 5) { r = x; b = c; }
        else { r = c; b = x; }
        
        ColorRGBA clearColor = {
            (uint8_t)((r + 0.3f) * 255),
            (uint8_t)((g + 0.3f) * 255),
            (uint8_t)((b + 0.3f) * 255),
            255
        };
        
        // Begin rendering
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SituationCmdBeginRenderToDisplay(cmd, -1, clearColor);
        
        // Draw text if font loaded
        if (font) {
            ColorRGBA white = {255, 255, 255, 255};
            
            // Title
            SituationCmdDrawText(cmd, font, "Hello Vulkan!", 
                               350, 250, white);
            
            // Subtitle
            SituationFont smallFont = SituationLoadFont("C:\\Windows\\Fonts\\arial.ttf", 20);
            if (smallFont) {
                char info[256];
                snprintf(info, sizeof(info), "Frame: %d | FPS: %.1f", 
                        frame_count, SituationGetFPS());
                SituationCmdDrawText(cmd, smallFont, info, 
                                   350, 350, white);
                
                SituationCmdDrawText(cmd, smallFont, 
                                   "Situation Library - C11 + Vulkan Backend", 
                                   250, 400, white);
                
                SituationCmdDrawText(cmd, smallFont, 
                                   "Press ESC to exit", 
                                   400, 500, white);
            }
        }
        
        SituationCmdEndRender(cmd);
        SituationEndFrame();
        
        frame_count++;
    }
    
    printf("\n===========================================\n");
    printf("  Demo completed successfully!\n");
    printf("  Total frames rendered: %d\n", frame_count);
    printf("===========================================\n");
    
    SituationShutdown();
    
    return 0;
}
