/*
 * Hello Fancy - Text Rendering Showcase for Situation v2.3.39
 * Demonstrates multiple colors, positions, and dynamic effects
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

// When using the DLL, only define these feature flags
// DO NOT define SITUATION_IMPLEMENTATION
#ifndef SITUATION_USE_SHARED
    // For static/inline builds
    #define SITUATION_IMPLEMENTATION
#endif

#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "situation.h"
#include <stdio.h>
#include <math.h>

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  Text Showcase - Situation v2.3.39\n");
    printf("========================================\n\n");
    
    SituationInitInfo config = {
        .window_width = 1024,
        .window_height = 768,
        .window_title = "Situation v2.3.39 - Text Rendering Showcase",
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,  // Start with vsync ON
        .enable_vulkan_validation = false  // Disable for cleaner output
    };
    
    printf("Initializing...\n");
    
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        printf("[ERROR] Initialization failed\n");
        return -1;
    }
    
    printf("[SUCCESS] Initialized!\n");
    printf("Press ESC to exit, V to toggle VSync, F3 for debug overlay\n\n");
    
    // Main render loop
    int frame = 0;
    double last_fps_time = SituationTimerGetTime();
    int fps_frame_count = 0;
    int current_fps = 0;
    bool vsync_enabled = true;  // Start with vsync on
    bool show_debug_overlay = false;  // F3 to toggle
    
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        // Calculate FPS manually
        fps_frame_count++;
        double current_time = SituationTimerGetTime();
        double time_delta = current_time - last_fps_time;
        if (time_delta >= 1.0) {
            current_fps = (int)(fps_frame_count / time_delta);
            fps_frame_count = 0;
            last_fps_time = current_time;
        }
        
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;
        
        // Toggle vsync with V key
        if (SituationIsKeyPressed(SIT_KEY_V)) {
            vsync_enabled = !vsync_enabled;
            SituationSetVSync(vsync_enabled);
            printf("VSync: %s\n", vsync_enabled ? "ON (capped at monitor refresh rate)" : "OFF (unlimited FPS)");
        }
        
        // Toggle debug overlay with F3 key
        if (SituationIsKeyPressed(SIT_KEY_F3)) {
            show_debug_overlay = !show_debug_overlay;
            printf("Debug Overlay: %s\n", show_debug_overlay ? "ON" : "OFF");
        }
        
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            break;
        }
        
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        
        // Dark background
        SituationRenderPassInfo pass_info = {
            .display_id = -1,
            .color_attachment = {
                .loadOp = SIT_LOAD_OP_CLEAR,
                .clear = { .color = {15, 15, 25, 255} }
            }
        };
        
        SituationCmdBeginRenderPass(cmd, &pass_info);
        
        SituationFont font = {0};  // Default font
        
        // Title - Yellow - Moderate scaling (2x looks better than 16x)
        SituationCmdDrawTextEx(cmd, font, "SITUATION v2.3.39",
            (Vector2){100, 40}, 64.0f, 4.0f, (ColorRGBA){255, 255, 100, 255});
        
        // Subtitle - Green - Normal size for clarity
        SituationCmdDrawTextEx(cmd, font, "Vulkan Text Rendering - FULLY FUNCTIONAL!",
            (Vector2){80, 100}, 16.0f, 1.0f, (ColorRGBA){100, 255, 100, 255});
        
        // Feature list
        float y = 140;
        SituationCmdDrawTextEx(cmd, font, "Features Demonstrated:",
            (Vector2){50, y}, 16.0f, 1.0f, (ColorRGBA){255, 200, 100, 255});
        
        y += 50;
        SituationCmdDrawTextEx(cmd, font, "  * Multiple Colors",
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){255, 100, 100, 255});  // Red
        
        y += 40;
        SituationCmdDrawTextEx(cmd, font, "  * Different Positions",
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){100, 150, 255, 255});  // Blue
        
        y += 40;
        SituationCmdDrawTextEx(cmd, font, "  * Proper UV Mapping",
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){255, 100, 255, 255});  // Magenta
        
        y += 40;
        SituationCmdDrawTextEx(cmd, font, "  * Correct Coordinate System",
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){100, 255, 255, 255});  // Cyan
        
        y += 40;
        SituationCmdDrawTextEx(cmd, font, "  * Descriptor Set Management",
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){255, 255, 100, 255});  // Yellow
        
        y += 40;
        SituationCmdDrawTextEx(cmd, font, "  * Text Scaling",
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){255, 150, 50, 255});  // Orange
        
        // Animated section
        float time = (float)SituationTimerGetTime();
        float wave_y = 350 + sinf(time * 2.0f) * 20.0f;
        
        // Pulsing color - keep minimum brightness to avoid flicker
        // Keep at normal size - bitmap fonts don't scale well
        uint8_t pulse = (uint8_t)(180 + 75 * sinf(time * 3.0f));  // Range: 105-255
        SituationCmdDrawTextEx(cmd, font, "~ Animated Text (wave motion) ~",
            (Vector2){250, wave_y}, 16.0f, 1.0f, (ColorRGBA){pulse, 255, pulse, 255});
        
        // Rainbow text effect
        const char* rainbow_text = "RAINBOW COLORS!";
        float x_offset = 250;
        for (int i = 0; rainbow_text[i] != '\0'; i++) {
            float hue = (time * 0.5f + i * 0.1f);
            uint8_t r = (uint8_t)(127 + 127 * sinf(hue));
            uint8_t g = (uint8_t)(127 + 127 * sinf(hue + 2.0944f));
            uint8_t b = (uint8_t)(127 + 127 * sinf(hue + 4.1888f));
            
            char single_char[2] = {rainbow_text[i], '\0'};
            SituationCmdDrawTextEx(cmd, font, single_char,
                (Vector2){x_offset + i * 16, 480}, 16.0f, 1.0f, (ColorRGBA){r, g, b, 255});
        }
        
        // Stats section
        y = 550;
        SituationCmdDrawTextEx(cmd, font, "Technical Details:",
            (Vector2){50, y}, 16.0f, 1.0f, (ColorRGBA){200, 200, 200, 255});
        
        y += 50;
        char stats[256];
        snprintf(stats, sizeof(stats), "FPS: %d", current_fps);
        SituationCmdDrawTextEx(cmd, font, stats,
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){100, 255, 100, 255});  // Green for FPS
        
        y += 40;
        snprintf(stats, sizeof(stats), "Frame: %d", frame);
        SituationCmdDrawTextEx(cmd, font, stats,
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){180, 180, 180, 255});
        
        y += 40;
        snprintf(stats, sizeof(stats), "Time: %.2f seconds", time);
        SituationCmdDrawTextEx(cmd, font, stats,
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){180, 180, 180, 255});
        
        y += 40;
        SituationCmdDrawTextEx(cmd, font, "Backend: Vulkan 1.4",
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){180, 180, 180, 255});
        
        y += 40;
        SituationCmdDrawTextEx(cmd, font, "Font: 8x8 Grid (CP437)",
            (Vector2){70, y}, 16.0f, 1.0f, (ColorRGBA){180, 180, 180, 255});
        
        y += 40;
        snprintf(stats, sizeof(stats), "VSync: %s (Press V to toggle)", vsync_enabled ? "ON" : "OFF");
        SituationCmdDrawTextEx(cmd, font, stats,
            (Vector2){70, y}, 16.0f, 1.0f, vsync_enabled ? (ColorRGBA){100, 255, 100, 255} : (ColorRGBA){255, 100, 100, 255});
        
        // Bottom banner - Gold
        SituationCmdDrawTextEx(cmd, font,
            "11 bugs fixed - 9+ hours of debugging - VICTORY!",
            (Vector2){100, 720}, 2.0f, 1.0f, (ColorRGBA){255, 215, 0, 255});
        
        // Debug overlay (F3 to toggle) - Upper right corner
        if (show_debug_overlay) {
            // Vulkan coords: (0,0) is bottom-left, so upper-right is (high X, high Y)
            // Window: 1024x768, text width ~300px, so X = 1024 - 310 = 714
            // Upper edge: Y = 768 - 20 = 748
            SituationDrawMetricsOverlay(cmd, (Vector2){714, 748}, (ColorRGBA){0, 255, 0, 255});
        }
        
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
        
        frame++;
        
        if (frame % 60 == 0 && frame > 0) {
            float fps = SituationGetFPS();
            printf("Frame %d - FPS: %.1f (%.2f ms/frame)\n", frame, fps, fps > 0 ? 1000.0f/fps : 0.0f);
        }
    }
    
    printf("\nShutting down...\n");
    SituationShutdown();
    printf("Done!\n");
    
    return 0;
}
