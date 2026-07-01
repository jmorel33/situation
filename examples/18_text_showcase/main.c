/***************************************************************************************************
 *  Situation — 18: Text Showcase
 *
 *  Catalog of text drawing: colors, sizes, spacing, animation, per-glyph rainbow.
 *
 *  Keys:
 *    1 / 2 / 3     Cycle demo section (features / animated / stats)
 *
 *  Build:
 *    build\build_examples.bat static-opengl  18_text_showcase
 *    build\build_examples.bat static-vulkan  18_text_showcase
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"

static void txt(SituationCommandBuffer cmd, const char* s, float x, float y,
                float size, float spacing, ColorRGBA c)
{
    SituationCmdDrawTextEx(cmd, (SituationFont){0}, s, (Vector2){{x, y}}, size, spacing, c);
}

static void draw_features(SituationCommandBuffer cmd, float y0)
{
    float y = y0;
    const float dy = 28.0f;

    txt(cmd, "Features demonstrated:", 50.0f, y, 18.0f, 1.0f, (ColorRGBA){255, 200, 100, 255});
    y += dy;

    txt(cmd, "  Multiple colors and positions", 70.0f, y, 16.0f, 1.0f, (ColorRGBA){255, 100, 100, 255});
    y += dy - 4.0f;
    txt(cmd, "  Font size + letter spacing (DrawTextEx)", 70.0f, y, 16.0f, 1.0f, (ColorRGBA){100, 150, 255, 255});
    y += dy - 4.0f;
    txt(cmd, "  Default 8x8 VGA bitmap font (CP437)", 70.0f, y, 16.0f, 1.0f, (ColorRGBA){255, 100, 255, 255});
    y += dy - 4.0f;
    txt(cmd, "  OpenGL + Vulkan parity", 70.0f, y, 16.0f, 1.0f, (ColorRGBA){100, 255, 255, 255});

    y += dy + 8.0f;
    txt(cmd, "Size ramp:", 50.0f, y, 16.0f, 0.0f, (ColorRGBA){220, 220, 220, 255});
    y += 22.0f;
    txt(cmd, "Small", 70.0f, y, 12.0f, 0.0f, (ColorRGBA){180, 220, 180, 255});
    txt(cmd, "Medium", 70.0f, y + 22.0f, 16.0f, 0.0f, (ColorRGBA){180, 220, 180, 255});
    txt(cmd, "Large", 70.0f, y + 50.0f, 24.0f, 0.0f, (ColorRGBA){180, 220, 180, 255});

    y += 90.0f;
    txt(cmd, "Spacing:", 50.0f, y, 16.0f, 0.0f, (ColorRGBA){220, 220, 220, 255});
    txt(cmd, "Tight", 70.0f, y + 24.0f, 16.0f, -1.0f, (ColorRGBA){255, 180, 120, 255});
    txt(cmd, "Wide   letters", 70.0f, y + 50.0f, 16.0f, 4.0f, (ColorRGBA){255, 180, 120, 255});
}

static void draw_animated(SituationCommandBuffer cmd, float y0, float time)
{
    float wave_y = y0 + 40.0f + sinf(time * 2.0f) * 20.0f;
    uint8_t pulse = (uint8_t)(127 + 127 * sinf(time * 3.0f));

    txt(cmd, "Animated text", 50.0f, y0, 18.0f, 1.0f, (ColorRGBA){255, 255, 100, 255});
    txt(cmd, "~ sine wave baseline ~", 320.0f, wave_y, 18.0f, 1.0f,
        (ColorRGBA){pulse, 255, pulse, 255});

    const char* rainbow = "RAINBOW COLORS!";
    float x = 280.0f;
    for (int i = 0; rainbow[i] != '\0'; ++i) {
        float hue = time * 0.5f + (float)i * 0.1f;
        uint8_t r = (uint8_t)(127 + 127 * sinf(hue));
        uint8_t g = (uint8_t)(127 + 127 * sinf(hue + 2.0944f));
        uint8_t b = (uint8_t)(127 + 127 * sinf(hue + 4.1888f));
        char ch[2] = { rainbow[i], '\0' };
        txt(cmd, ch, x + (float)i * 14.0f, y0 + 120.0f, 18.0f, 0.0f,
            (ColorRGBA){r, g, b, 255});
    }

    txt(cmd, "Rotating label (position orbit):", 50.0f, y0 + 180.0f, 16.0f, 0.0f,
        (ColorRGBA){200, 200, 200, 255});
    float ox = 640.0f + cosf(time * 1.2f) * 180.0f;
    float oy = y0 + 240.0f + sinf(time * 1.2f) * 60.0f;
    txt(cmd, "Situation", ox, oy, 20.0f, 1.0f, (ColorRGBA){255, 215, 0, 255});
}

static void draw_stats(SituationCommandBuffer cmd, float y0, int frame, float time)
{
    char buf[128];
    float y = y0;

    txt(cmd, "Runtime stats:", 50.0f, y, 18.0f, 0.0f, (ColorRGBA){200, 200, 200, 255});
    y += 30.0f;

    snprintf(buf, sizeof buf, "  Frame: %d", frame);
    txt(cmd, buf, 70.0f, y, 16.0f, 0.0f, (ColorRGBA){180, 180, 180, 255});
    y += 24.0f;

    snprintf(buf, sizeof buf, "  Time: %.2f s", time);
    txt(cmd, buf, 70.0f, y, 16.0f, 0.0f, (ColorRGBA){180, 180, 180, 255});
    y += 24.0f;

    snprintf(buf, sizeof buf, "  Backend: %s", SituationGetGraphicsBackendName());
    txt(cmd, buf, 70.0f, y, 16.0f, 0.0f, (ColorRGBA){180, 180, 180, 255});
    y += 24.0f;

    txt(cmd, "  Font: built-in 8x8 VGA (generation=0 fallback)", 70.0f, y, 16.0f, 0.0f,
        (ColorRGBA){180, 180, 180, 255});
    y += 36.0f;

    txt(cmd, "API: SituationCmdDrawText / SituationCmdDrawTextEx", 50.0f, y, 16.0f, 0.0f,
        (ColorRGBA){140, 200, 255, 255});
}

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "Situation — Text Showcase") != SITUATION_SUCCESS)
        return -1;

    int section = 0;
    int frame = 0;

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) break;

        if (SituationIsKeyPressed(SIT_KEY_1)) section = 0;
        if (SituationIsKeyPressed(SIT_KEY_2)) section = 1;
        if (SituationIsKeyPressed(SIT_KEY_3)) section = 2;

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = { .color = {15, 15, 25, 255} }
                }
            };
            SituationCmdBeginRenderPass(cmd, &pass);

            const char* hints[] = {
                "1/2/3: sections (features / animated / stats)",
                "1/2/3: sections (features / animated / stats)",
                "1/2/3: sections (features / animated / stats)"
            };
            SitExample_DrawHUD(cmd, "18 — Text Showcase", hints[section]);

            float time = (float)SituationTimerGetTime();
            float content_y = 36.0f;

            txt(cmd, "Situation Text Rendering", 200.0f, content_y, 20.0f, 1.0f,
                (ColorRGBA){255, 255, 100, 255});
            txt(cmd, "GPU-accelerated textured quads per glyph", 180.0f, content_y + 28.0f, 16.0f, 0.5f,
                (ColorRGBA){100, 255, 100, 255});

            if (section == 0) draw_features(cmd, content_y + 70.0f);
            if (section == 1) draw_animated(cmd, content_y + 70.0f, time);
            if (section == 2) draw_stats(cmd, content_y + 70.0f, frame, time);

            SituationCmdEndRenderPass(cmd);
            SitExample_EndFrame();
        }
        ++frame;
    }

    SitExample_Shutdown();
    return 0;
}
