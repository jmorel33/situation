/*
 * Hello Modern - Using the CORRECT modern Situation API
 * This uses SituationCmdBeginRenderPass instead of deprecated functions
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  Hello Modern - Correct API Usage\n");
    printf("========================================\n\n");
    
    SituationInitInfo config = {
        .window_width = 800,
        .window_height = 600,
        .window_title = "Hello Modern - Vulkan",
        .enable_vulkan_validation = true  // Enable validation layers for debugging
    };
    
    printf("Initializing...\n");
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("[ERROR] Initialization failed\n");
        return -1;
    }
    
    printf("[SUCCESS] Initialized!\n\n");
    
    // Main render loop
    int frame = 0;
    
    while (!SituationWindowShouldClose() && frame < 600) {  // Run for 10 seconds at 60fps
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;
        
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            printf("Failed to acquire frame\n");
            break;
        }
        
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        
        // CORRECT API: Use SituationRenderPassInfo
        SituationRenderPassInfo pass_info = {
            .display_id = -1,  // Main window
            .color_attachment = {
                .loadOp = SIT_LOAD_OP_CLEAR,
                .storeOp = SIT_STORE_OP_STORE,
                .clear = {
                    .color = {30, 30, 40, 255}  // Dark blue-gray
                }
            },
            .depth_attachment = {
                .loadOp = SIT_LOAD_OP_CLEAR,
                .storeOp = SIT_STORE_OP_DONT_CARE,
                .clear = {
                    .depth = 1.0f
                }
            }
        };
        
        // CORRECT API: Use SituationCmdBeginRenderPass
        SituationCmdBeginRenderPass(cmd, &pass_info);
        
        // Draw fancy text showcase!
        SituationFont font = {0};  // Default font
        
        // Title - Yellow
        Vector2 pos_title = {150.0f, 50.0f};
        ColorRGBA yellow = {255, 255, 100, 255};
        SituationCmdDrawText(cmd, font, "SITUATION v2.3.39 'TRIUMPH'", pos_title, yellow);
        
        // Subtitle - Green
        Vector2 pos_sub = {120.0f, 80.0f};
        ColorRGBA green = {100, 255, 100, 255};
        SituationCmdDrawText(cmd, font, "Vulkan Text Rendering - FULLY FUNCTIONAL!", pos_sub, green);
        
        // Feature list with different colors
        float y = 140;
        Vector2 pos_feat = {50.0f, y};
        ColorRGBA orange = {255, 200, 100, 255};
        SituationCmdDrawText(cmd, font, "Features Demonstrated:", pos_feat, orange);
        
        y += 30;
        Vector2 pos1 = {70.0f, y};
        ColorRGBA red = {255, 100, 100, 255};
        SituationCmdDrawText(cmd, font, "  * Multiple Colors", pos1, red);
        
        y += 25;
        Vector2 pos2 = {70.0f, y};
        ColorRGBA blue = {100, 150, 255, 255};
        SituationCmdDrawText(cmd, font, "  * Different Positions", pos2, blue);
        
        y += 25;
        Vector2 pos3 = {70.0f, y};
        ColorRGBA magenta = {255, 100, 255, 255};
        SituationCmdDrawText(cmd, font, "  * Proper UV Mapping", pos3, magenta);
        
        y += 25;
        Vector2 pos4 = {70.0f, y};
        ColorRGBA cyan = {100, 255, 255, 255};
        SituationCmdDrawText(cmd, font, "  * Correct Coordinate System", pos4, cyan);
        
        // Bottom banner - Gold
        Vector2 pos_banner = {100.0f, 520.0f};
        ColorRGBA gold = {255, 215, 0, 255};
        SituationCmdDrawText(cmd, font, "11 bugs fixed - 9+ hours - VICTORY!", pos_banner, gold);
        
        // CORRECT API: Use SituationCmdEndRenderPass
        SituationCmdEndRenderPass(cmd);
        
        SituationEndFrame();
        
        frame++;
        
        if (frame % 60 == 0) {
            printf("Frame %d - FPS: %.1f\n", frame, SituationGetFPS());
        }
    }
    
    printf("\nShutting down...\n");
    SituationShutdown();
    printf("Done!\n");
    
    return 0;
}
