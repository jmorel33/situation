/*******************************************************************************
 * Situation Library - TEXT TEST
 * ==============================
 * Tests GPU-accelerated text rendering with the built-in 8x8 bitmap font.
 *
 * Controls:
 *   ESC - Exit
 ******************************************************************************/

#define SITUATION_USE_OPENGL
#include "situation.h"
#include <stdio.h>
#include <math.h>

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Text Rendering Test",
        .window_width = 800,
        .window_height = 600,
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        fprintf(stderr, "Init failed: %s\n", err_msg ? err_msg : "unknown");
        return -1;
    }

    printf("Text rendering test. Press ESC to exit.\n");

    // Use default font (pass zero-initialized SituationFont)
    SituationFont font = {0};

    int frame = 0;
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {20, 20, 40, 255} } },
                .depth_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .depth = 1.0f } }
            };
            SituationCmdBeginRenderPass(cmd, &pass);

            // Basic text at different positions
            SituationCmdDrawText(cmd, font, "Hello, Situation!", (Vector2){{20.0f, 20.0f}}, (ColorRGBA){255, 255, 255, 255});
            SituationCmdDrawText(cmd, font, "OpenGL + Vulkan + Audio = Working!", (Vector2){{20.0f, 50.0f}}, (ColorRGBA){0, 255, 128, 255});
            SituationCmdDrawText(cmd, font, "v2.4.22 - All systems operational", (Vector2){{20.0f, 80.0f}}, (ColorRGBA){255, 200, 50, 255});

            // Scaled text
            SituationCmdDrawTextEx(cmd, font, "LARGE TEXT (2x)", (Vector2){{20.0f, 140.0f}}, 16.0f, 0.0f, (ColorRGBA){255, 100, 100, 255});
            SituationCmdDrawTextEx(cmd, font, "HUGE TEXT (4x)", (Vector2){{20.0f, 180.0f}}, 32.0f, 0.0f, (ColorRGBA){100, 150, 255, 255});

            // Animated text
            float t = frame * 0.03f;
            char buf[64];
            snprintf(buf, sizeof(buf), "Frame: %d  Time: %.1fs", frame, frame / 60.0f);
            SituationCmdDrawText(cmd, font, buf, (Vector2){{20.0f, 250.0f}}, (ColorRGBA){200, 200, 200, 255});

            // Color cycling text
            uint8_t r = (uint8_t)(128 + 127 * sinf(t));
            uint8_t g = (uint8_t)(128 + 127 * sinf(t + 2.0f));
            uint8_t b = (uint8_t)(128 + 127 * sinf(t + 4.0f));
            SituationCmdDrawText(cmd, font, "Rainbow cycling text!", (Vector2){{20.0f, 280.0f}}, (ColorRGBA){r, g, b, 255});

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }

        frame++;
    }

    SituationShutdown();
    return 0;
}
