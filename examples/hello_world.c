/***************************************************************************************************
*   Situation Library - Example: Hello World (Scales, Rotation, Colors)
*   -------------------------------------------------------------------
*   This example demonstrates drawing text with advanced styling parameters:
*   - Custom Fonts (using bitmap fonts provided in examples/font_data.h)
*   - Text Rotation
*   - Text Scaling
*   - Color Tinting
*
*   It demonstrates the power of the `SituationImageDrawTextEx` API which supports both
*   TrueType (SDF) and Bitmap (Pixel Art) fonts transparently.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"
#include <cglm/cglm.h>

// Include the font data header from the examples folder
#include "font_data.h"

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Situation - Hello World",
        .window_width = 800, .window_height = 600
    };
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;

    // 1. Load the embedded bitmap font into a SituationFont structure
    SituationFont font = {0};
    // ibm_font_8x8 is 256 chars, 8x8 pixels each.
    // The data is 1 byte per row (1 bit per pixel).
    SituationLoadBitmapFontFromMemory(ibm_font_8x8, 8, 8, 256, &font);

    // 2. Create a CPU Image to draw on
    SituationImage screen_buffer = {0};
    SituationCreateImage(800, 600, 4, &screen_buffer);

    // Clear to dark blue
    SituationGenImageColor(800, 600, (ColorRGBA){10, 10, 30, 255}, &screen_buffer);

    // 3. Draw "Hello World" with different params using the powerful Ex function
    printf("Rendering text to texture...\n");

    // Normal (White)
    // Note: fontSize for bitmaps is the target height in pixels. 8 is 1:1 scale.
    SituationImageDrawTextEx(&screen_buffer, font, "Hello World",
        (Vector2){50, 100}, 8.0f, 0.0f, 0.0f, 0.0f, (ColorRGBA){255, 255, 255, 255}, (ColorRGBA){0,0,0,0}, 0.0f);

    // Scaled & Colored (Red, Scale 2.0 -> size 16)
    SituationImageDrawTextEx(&screen_buffer, font, "BIG TEXT",
        (Vector2){50, 200}, 16.0f, 0.0f, 0.0f, 0.0f, (ColorRGBA){255, 100, 100, 255}, (ColorRGBA){0,0,0,0}, 0.0f);

    // Rotated (Green, Scale 1.5 -> size 12, Angle 45)
    SituationImageDrawTextEx(&screen_buffer, font, "Rotated Text",
        (Vector2){400, 300}, 12.0f, 0.0f, 45.0f, 0.0f, (ColorRGBA){100, 255, 100, 255}, (ColorRGBA){0,0,0,0}, 0.0f);

    // Rotated & Scaled (Blue, Scale 1.0 -> size 8, Angle 15)
    SituationImageDrawTextEx(&screen_buffer, font, "Spinning?",
        (Vector2){400, 100}, 8.0f, 0.0f, 15.0f, 0.0f, (ColorRGBA){100, 100, 255, 255}, (ColorRGBA){0,0,0,0}, 0.0f);

    // 4. Upload to GPU Texture
    SituationTexture text_texture;
    SituationCreateTexture(screen_buffer, false, &text_texture);

    // Free CPU memory
    SituationUnloadImage(screen_buffer);
    // Note: SituationLoadBitmapFontFromMemory doesn't allocate data copy for bitmaps (references static data),
    // so SituationUnloadFont is technically a no-op/safe but good practice.
    SituationUnloadFont(font);

    // 5. Main Loop
    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = { .display_id = -1, .color_attachment = { .loadOp = SIT_LOAD_OP_DONT_CARE } }; // We draw full screen texture, no clear needed

            SituationCmdBeginRenderPass(cmd, &pass);

            // Draw the texture covering the screen
            // Source: Full texture
            Rectangle src = {0, 0, 800, 600};
            // Dest: Full screen
            Rectangle dst = {0, 0, 800, 600};

            SituationCmdDrawTexture(cmd, text_texture, src, dst, (Vector2){0,0}, 0.0f, (ColorRGBA){255,255,255,255});

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    SituationDestroyTexture(&text_texture);
    SituationShutdown();
    return 0;
}
