/*
 * Superseded by examples/18_text_showcase/ — kept for dev reference.
 * Text Rendering Showcase - Situation v2.3.39
 * 
 * Demonstrates the fully functional Vulkan text rendering system
 * with multiple colors, positions, and dynamic effects.
 */

// DLL-linked build — do NOT define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER
#include "situation.h"

#include <math.h>
#include <stdio.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  Text Rendering Showcase - v2.3.39\n");
    printf("========================================\n");
    printf("Initializing...\n");

    SituationInitInfo init_info = {
        .window_width = 1024,
        .window_height = 768,
        .window_title = "Situation v2.3.39 - Text Rendering Showcase",
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,
        .enable_vulkan_validation = false  // Disable for cleaner output
    };

    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        printf("Failed to initialize Situation!\n");
        return -1;
    }

    printf("Initialization complete!\n");
    printf("Press ESC to exit\n\n");

    // Main loop
    int frame = 0;
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        // Exit on ESC
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            // Begin render pass with dark background
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = { .color = {15, 15, 25, 255} }  // Dark blue-grey
                }
            };
            
            SituationCmdBeginRenderPass(cmd, &pass);

            // Use default font (zeroed struct will auto-fallback to default_font)
            SituationFont font = {0};

            // Title - Large and centered
            SituationCmdDrawText(cmd, font,
                "SITUATION v2.3.39 'TRIUMPH'",
                (Vector2){200, 50},
                (ColorRGBA){255, 255, 100, 255});  // Bright yellow

            // Subtitle
            SituationCmdDrawText(cmd, font,
                "Vulkan Text Rendering - FULLY FUNCTIONAL!",
                (Vector2){180, 80},
                (ColorRGBA){100, 255, 100, 255});  // Bright green

            // Feature list with different colors
            float y = 140;
            SituationCmdDrawText(cmd, font, "Features Demonstrated:", 
                (Vector2){50, y}, (ColorRGBA){255, 200, 100, 255});
            
            y += 30;
            SituationCmdDrawText(cmd, font, "  * Multiple Colors", 
                (Vector2){70, y}, (ColorRGBA){255, 100, 100, 255});  // Red
            
            y += 25;
            SituationCmdDrawText(cmd, font, "  * Different Positions", 
                (Vector2){70, y}, (ColorRGBA){100, 150, 255, 255});  // Blue
            
            y += 25;
            SituationCmdDrawText(cmd, font, "  * Proper UV Mapping", 
                (Vector2){70, y}, (ColorRGBA){255, 100, 255, 255});  // Magenta
            
            y += 25;
            SituationCmdDrawText(cmd, font, "  * Correct Coordinate System", 
                (Vector2){70, y}, (ColorRGBA){100, 255, 255, 255});  // Cyan
            
            y += 25;
            SituationCmdDrawText(cmd, font, "  * Descriptor Set Management", 
                (Vector2){70, y}, (ColorRGBA){255, 255, 100, 255});  // Yellow

            // Animated section
            float time = (float)SituationTimerGetTime();
            float wave_y = 350 + sin(time * 2.0f) * 20.0f;
            
            // Pulsing color
            uint8_t pulse = (uint8_t)(127 + 127 * sin(time * 3.0f));
            SituationCmdDrawText(cmd, font, "~ Animated Text ~",
                (Vector2){400, wave_y},
                (ColorRGBA){pulse, 255, pulse, 255});

            // Rainbow text effect
            const char* rainbow_text = "RAINBOW COLORS!";
            float x_offset = 300;
            for (int i = 0; rainbow_text[i] != '\0'; i++) {
                float hue = (time * 0.5f + i * 0.1f);
                uint8_t r = (uint8_t)(127 + 127 * sin(hue));
                uint8_t g = (uint8_t)(127 + 127 * sin(hue + 2.0944f));  // 120 degrees
                uint8_t b = (uint8_t)(127 + 127 * sin(hue + 4.1888f));  // 240 degrees
                
                char single_char[2] = {rainbow_text[i], '\0'};
                SituationCmdDrawText(cmd, font, single_char,
                    (Vector2){x_offset + i * 8, 450},
                    (ColorRGBA){r, g, b, 255});
            }

            // Stats section
            y = 550;
            SituationCmdDrawText(cmd, font, "Technical Details:",
                (Vector2){50, y}, (ColorRGBA){200, 200, 200, 255});
            
            y += 30;
            char stats[256];
            snprintf(stats, sizeof(stats), "Frame: %d", frame);
            SituationCmdDrawText(cmd, font, stats,
                (Vector2){70, y}, (ColorRGBA){180, 180, 180, 255});
            
            y += 25;
            snprintf(stats, sizeof(stats), "Time: %.2f seconds", time);
            SituationCmdDrawText(cmd, font, stats,
                (Vector2){70, y}, (ColorRGBA){180, 180, 180, 255});
            
            y += 25;
            SituationCmdDrawText(cmd, font, "Backend: Vulkan 1.4",
                (Vector2){70, y}, (ColorRGBA){180, 180, 180, 255});
            
            y += 25;
            SituationCmdDrawText(cmd, font, "Font: 8x8 Grid (CP437)",
                (Vector2){70, y}, (ColorRGBA){180, 180, 180, 255});

            // Bottom banner
            SituationCmdDrawText(cmd, font,
                "11 bugs fixed - 9+ hours of debugging - VICTORY!",
                (Vector2){200, 720},
                (ColorRGBA){255, 215, 0, 255});  // Gold

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }

        frame++;
    }

    printf("\nShutting down...\n");
    SituationShutdown();
    printf("Done!\n");

    return 0;
}
