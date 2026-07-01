/**
 * @file test_text_rendering.c
 * @brief Text rendering harness module — default grid font, Roboto TTF bake/draw, layout.
 *
 * Split from test_graphics.c so text failures are isolated and run after core graphics,
 * immediately before [virtual_display] in the harness registry.
 *
 * Retro builder certification: tests/harness/test_text_retro_builders.c (module text_retro_builders).
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_text_helpers.h"
#include "sit_test_window.h"
#include <stdio.h>
#include <string.h>

static bool g_text_init_ok = false;

static void text_rendering_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_TEXT_RENDERING");

    SituationError err = SituationInit(0, NULL, &config);
    g_text_init_ok = (err == SITUATION_SUCCESS);
    if (!g_text_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    graphics_test_print_renderer_banner();
}

static void text_rendering_teardown(void) {
    if (g_text_init_ok) {
        SituationShutdown();
        g_text_init_ok = false;
    }
}

static void text_test_fill_glyph_cell_rgba(
    SituationImage* img, int cell_col, int cell_row, int cell_w, int cell_h, uint8_t v)
{
    if (!img || !img->data) {
        return;
    }
    uint8_t* pixels = (uint8_t*)img->data;
    int x0 = cell_col * cell_w;
    int y0 = cell_row * cell_h;
    for (int y = y0; y < y0 + cell_h && y < img->height; ++y) {
        for (int x = x0; x < x0 + cell_w && x < img->width; ++x) {
            int idx = (y * img->width + x) * img->channels;
            pixels[idx + 0] = v;
            pixels[idx + 1] = v;
            pixels[idx + 2] = v;
            pixels[idx + 3] = v;
        }
    }
}

/** Built-in default grid font — SituationCmdDrawText with zero-initialized handle. */
static void test_cmd_draw_text_bitmap(void) {
    SituationFont font = sit_text_test_default_font();

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp;
    sit_text_test_fill_main_clear_pass(&rp);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);

    const int text_x = 10;
    const int text_y = 10;
    ColorRGBA white = {255, 255, 255, 255};
    SIT_ASSERT_EQ(
        SituationCmdDrawText(cmd, font, "Hello Test",
            (Vector2){{(float)text_x, (float)text_y}}, white),
        SITUATION_SUCCESS);

    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage screen = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen), SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, text_x + 160, text_y + 32, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, text_x + 180, text_y, screen.width - 1, text_y + 32, 12));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, text_x, text_y + 40, text_x + 160, screen.height - 1, 12));

    SituationUnloadImage(screen);
}

static void test_cmd_draw_text_ex_bounds(void) {
    SituationFont font = sit_text_test_default_font();

    const int text_x = 10;
    const int text_y = 10;
    const int band_right = 280;
    const int band_bottom = 56;
    Vector2 text_pos = {{(float)text_x, (float)text_y}};
    ColorRGBA white = {255, 255, 255, 255};

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp;
    sit_text_test_fill_main_clear_pass(&rp);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(
        SituationCmdDrawTextEx(cmd, font, "ABCDEF", text_pos, 16.0f, 0.0f, white),
        SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage screen_default = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen_default), SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen_default));
    int count_default = sit_text_test_count_bright_pixels(
        &screen_default, text_x, text_y, band_right, band_bottom, 40);
    SituationUnloadImage(screen_default);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    sit_text_test_fill_main_clear_pass(&rp);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(
        SituationCmdDrawTextEx(cmd, font, "ABCDEF", text_pos, 32.0f, 4.0f, white),
        SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage screen_large = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen_large), SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen_large));
    int count_large = sit_text_test_count_bright_pixels(
        &screen_large, text_x, text_y, band_right, band_bottom, 40);
    SituationUnloadImage(screen_large);

    SIT_ASSERT(count_default > 0);
    SIT_ASSERT(count_large > count_default + (count_default / 4));
}

static void test_cmd_draw_text_screen_layout(void) {
    SituationFont font = sit_text_test_default_font();

    const int top_x = 24;
    const int top_y = 20;
    const int bot_x = 24;
    const int bot_y = SituationGetRenderHeight() - (int)(40.0f * ((float)SituationGetRenderHeight() /
                                                                 (float)(SituationGetScreenHeight() > 0
                                                                             ? SituationGetScreenHeight()
                                                                             : 1)));
    SIT_ASSERT(bot_y > top_y + 48);

    SituationCommandBuffer cmd = graphics_test_begin_frame();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp;
    sit_text_test_fill_main_clear_pass(&rp);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);

    ColorRGBA white = {255, 255, 255, 255};
    SIT_ASSERT_EQ(
        SituationCmdDrawText(cmd, font, "TOP", (Vector2){{(float)top_x, (float)top_y}}, white),
        SITUATION_SUCCESS);
    SIT_ASSERT_EQ(
        SituationCmdDrawText(cmd, font, "BOT", (Vector2){{(float)bot_x, (float)bot_y}}, white),
        SITUATION_SUCCESS);

    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage screen = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen), SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(graphics_test_region_any_bright(&screen, top_x, top_y, top_x + 72, top_y + 28, 40));
    SIT_ASSERT(graphics_test_region_any_bright(&screen, bot_x, bot_y, bot_x + 72, bot_y + 28, 40));
    SIT_ASSERT(graphics_test_region_all_dark(&screen, top_x, 4, top_x + 72, top_y - 6, 12));
    SIT_ASSERT(graphics_test_region_all_dark(&screen, top_x, top_y + 40, bot_x + 72, bot_y - 8, 12));
    SIT_ASSERT(graphics_test_region_all_dark(&screen, top_x, bot_y + 36, bot_x + 72, bot_y + 52, 12));

    int cx = screen.width / 2;
    int cy = screen.height / 2;
    SIT_ASSERT(graphics_test_region_all_dark(&screen, cx - 20, cy - 20, cx + 20, cy + 20, 12));
    SIT_ASSERT(graphics_test_region_all_dark(&screen, top_x, bot_y - 10, top_x + 72, bot_y - 2, 12));

    SituationUnloadImage(screen);
}

static void test_roboto_ttf_bake_draw(void) {
    SituationFont font = {0};
    sit_text_test_require_roboto_baked(&font, 24.0f);

    SitRectangle bounds = SituationMeasureText(font, "Roboto", 24.0f);
    SIT_ASSERT(bounds.width > 8.0f);
    SIT_ASSERT(bounds.height > 8.0f);

    const int text_x = 12;
    const int text_y = 14;
    ColorRGBA white = {255, 255, 255, 255};

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(font, "Roboto", text_x, text_y, 24.0f, 0.0f, white, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    const int band_right = text_x + (int)bounds.width + 16;
    const int band_bottom = text_y + (int)bounds.height + 12;
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, band_right, band_bottom, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, band_right + 8, text_y, screen.width - 1, band_bottom, 12));

    SituationUnloadImage(screen);
    sit_text_test_destroy_font(&font);
}

static void test_roboto_ttf_ex_bounds(void) {
    SituationFont font = {0};
    sit_text_test_require_roboto_baked(&font, 16.0f);

    const int text_x = 10;
    const int text_y = 10;
    const int band_right = 320;
    const int band_bottom = 72;
    ColorRGBA white = {255, 255, 255, 255};

    SituationImage screen_default = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(font, "Roboto", text_x, text_y, 16.0f, 0.0f, white, &screen_default),
        SITUATION_SUCCESS);
    int count_default = sit_text_test_count_bright_pixels(
        &screen_default, text_x, text_y, band_right, band_bottom, 40);
    SituationUnloadImage(screen_default);

    SituationImage screen_large = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(font, "Roboto", text_x, text_y, 32.0f, 4.0f, white, &screen_large),
        SITUATION_SUCCESS);
    int count_large = sit_text_test_count_bright_pixels(
        &screen_large, text_x, text_y, band_right, band_bottom, 40);
    SituationUnloadImage(screen_large);

    SIT_ASSERT(count_default > 0);
    SIT_ASSERT(count_large > count_default + (count_default / 4));

    sit_text_test_destroy_font(&font);
}

/**
 * Custom 8×8 bitmap (not the default grid) → BakeBitmapFontAtlas → GPU readback.
 * Only glyph 'A' (index 65) is solid; verifies a user bitmap atlas draws.
 */
static void test_bitmap_memory_bake_gpu_draw(void) {
    unsigned char bitmap_data[256 * 8];
    sit_text_test_fill_synthetic_bitmap(bitmap_data, sizeof(bitmap_data), 65);

    SituationFont font = {0};
    SIT_ASSERT_EQ(SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationBakeBitmapFontAtlas(&font), SITUATION_SUCCESS);
    SIT_ASSERT(font.atlas_texture.generation != 0);
    SIT_ASSERT(font.is_bitmap);

    const int text_x = 20;
    const int text_y = 24;
    ColorRGBA white = {255, 255, 255, 255};

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(font, "A", text_x, text_y, 8.0f, 0.0f, white, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, text_x + 24, text_y + 16, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, text_x + 40, text_y, text_x + 120, text_y + 16, 12));

    SituationUnloadImage(screen);
    sit_text_test_destroy_font(&font);
}

/** Pre-uploaded RGBA sheet → LoadBitmapFontFromTexture → GPU draw. */
static void test_load_bitmap_font_from_texture(void) {
    const int cell_w = 8;
    const int cell_h = 8;
    const int sheet_w = 128;
    const int sheet_h = 128;
    const int first_char = 0;
    const int glyph_index = 65; /* 'A' */
    const int chars_per_row = sheet_w / cell_w;
    const int cell_col = glyph_index % chars_per_row;
    const int cell_row = glyph_index / chars_per_row;

    SituationImage sheet_img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(sheet_w, sheet_h, 4, &sheet_img), SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(sheet_img.data);
    memset(sheet_img.data, 0, (size_t)sheet_w * (size_t)sheet_h * 4u);
    text_test_fill_glyph_cell_rgba(&sheet_img, cell_col, cell_row, cell_w, cell_h, 255);

    SituationTexture sheet_tex = {0};
    SIT_ASSERT_EQ(SituationCreateTexture(sheet_img, false, &sheet_tex), SITUATION_SUCCESS);
    SituationUnloadImage(sheet_img);

    SituationFont font = {0};
    SIT_ASSERT_EQ(
        SituationLoadBitmapFontFromTexture(sheet_tex, cell_w, cell_h, first_char, &font),
        SITUATION_SUCCESS);
    SIT_ASSERT(font.atlas_texture.generation != 0);
    SIT_ASSERT_EQ(font.chars_per_row, chars_per_row);
    SIT_ASSERT_EQ(font.display_cell_width, cell_w);

    const int text_x = 24;
    const int text_y = 28;
    ColorRGBA white = {255, 255, 255, 255};

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(font, "A", text_x, text_y, 8.0f, 0.0f, white, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, text_x + 24, text_y + 16, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, text_x + 40, text_y, text_x + 120, text_y + 16, 12));

    SituationUnloadImage(screen);
    sit_text_test_destroy_font(&font);
}

static void test_font_unload_destroys_atlas(void) {
    unsigned char bitmap_data[256 * 8];
    sit_text_test_fill_synthetic_bitmap(bitmap_data, sizeof(bitmap_data), 65);

    SituationFont font = {0};
    SIT_ASSERT_EQ(SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationBakeBitmapFontAtlas(&font), SITUATION_SUCCESS);
    SIT_ASSERT(font.atlas_texture.generation != 0);

    SituationTexture atlas = font.atlas_texture;
    SituationUnloadFont(font);

    SituationTextureInfo info = {0};
    SIT_ASSERT(SituationGetTextureInfo(atlas, &info) != SITUATION_SUCCESS);

    SIT_ASSERT_EQ(SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationBakeBitmapFontAtlas(&font), SITUATION_SUCCESS);
    SituationUnloadFont(font);
}

static void test_measure_text_multiline(void) {
    SituationFont font = sit_text_test_default_font();

    SitRectangle single = SituationMeasureTextEx(font, "A", 16.0f, 0.0f);
    SIT_ASSERT(single.width > 0.0f);
    SIT_ASSERT(single.height > 0.0f);

    SitRectangle multi = SituationMeasureTextEx(font, "A\nB", 16.0f, 0.0f);
    SIT_ASSERT(multi.height > single.height);
    SIT_ASSERT(multi.width <= single.width + 1.0f);
}

static void test_get_text_line_count(void) {
    SituationFont font = sit_text_test_default_font();

    SIT_ASSERT_EQ(SituationGetTextLineCount(font, "one\ntwo", 0.0f), 2);

    int wrapped = SituationGetTextLineCount(font, "abcdefghijklmnop", 32.0f);
    SIT_ASSERT(wrapped >= 2);
}

/** Word wrap on — long string splits across two lines inside bounds. */
static void test_cmd_draw_text_boxed_wrap(void) {
    SituationFont font = sit_text_test_default_font();
    const int bx = 24;
    const int by = 24;
    const int bw = 80;
    const int bh = 56;
    SitRectangle bounds = {(float)bx, (float)by, (float)bw, (float)bh};

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_boxed_frame(
            font, "AAAA BBBB", bounds, 16.0f, 0.0f, (ColorRGBA){255, 255, 255, 255}, true, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(graphics_test_region_any_bright(&screen, bx, by, bx + bw - 1, by + 22, 40));
    SIT_ASSERT(graphics_test_region_any_bright(&screen, bx, by + 24, bx + bw - 1, by + bh - 1, 40));
    SIT_ASSERT(graphics_test_region_all_dark(&screen, bx, by + bh + 8, bx + bw - 1, by + bh + 24, 12));

    SituationUnloadImage(screen);
}

/** Word wrap off — glyphs that do not fit entirely inside bounds are not drawn. */
static void test_cmd_draw_text_boxed_clip(void) {
    SituationFont font = sit_text_test_default_font();
    const int bx = 24;
    const int by = 28;
    const int bw = 48;
    const int bh = 32;
    SitRectangle bounds = {(float)bx, (float)by, (float)bw, (float)bh};

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_boxed_frame(
            font, "ZZZZZZZZ", bounds, 16.0f, 0.0f, (ColorRGBA){255, 255, 255, 255}, false, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(graphics_test_region_any_bright(&screen, bx, by, bx + bw - 1, by + 20, 40));
    SIT_ASSERT(graphics_test_region_all_dark(&screen, bx + bw + 12, by, bx + bw + 80, by + 20, 12));

    SituationUnloadImage(screen);
}

/** DrawTextEx with embedded newline — two vertical bright bands. */
static void test_cmd_draw_text_multiline_gpu(void) {
    SituationFont font = sit_text_test_default_font();
    const int text_x = 20;
    const int text_y = 20;
    ColorRGBA white = {255, 255, 255, 255};

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(
            font, "LINE1\nLINE2", text_x, text_y, 16.0f, 0.0f, white, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, text_x + 80, text_y + 18, 40));
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y + 20, text_x + 80, text_y + 40, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, text_x + 120, text_y, text_x + 200, text_y + 40, 12));

    SituationUnloadImage(screen);
}

/** Non-white tint visible in readback (red-dominant pixels in text band). */
static void test_cmd_draw_text_colored(void) {
    SituationFont font = sit_text_test_default_font();
    const int text_x = 24;
    const int text_y = 24;
    ColorRGBA red = {255, 32, 32, 255};

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(
            font, "RED", text_x, text_y, 16.0f, 0.0f, red, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(sit_text_test_region_any_red_dominant(
        &screen, text_x, text_y, text_x + 64, text_y + 24));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, text_x + 80, text_y, text_x + 160, text_y + 24, 12));

    SituationUnloadImage(screen);
}

/** Baked TTF — measured width scales ~2× when fontSize doubles. */
static void test_measure_text_ttf_baked(void) {
    SituationFont font = {0};
    sit_text_test_require_roboto_baked(&font, 24.0f);
    sit_text_test_assert_measure_width_scales(font, "Roboto", 16.0f, 32.0f);
    sit_text_test_destroy_font(&font);
}

/** Baked custom bitmap — measured width scales ~2× when fontSize doubles. */
static void test_measure_text_bitmap_baked(void) {
    unsigned char bitmap_data[256 * 8];
    sit_text_test_fill_synthetic_bitmap(bitmap_data, sizeof(bitmap_data), 65);

    SituationFont font = {0};
    SIT_ASSERT_EQ(SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationBakeBitmapFontAtlas(&font), SITUATION_SUCCESS);
    SIT_ASSERT(font.atlas_texture.generation != 0);

    sit_text_test_assert_measure_width_scales(font, "AAA", 8.0f, 16.0f);
    sit_text_test_destroy_font(&font);
}

/** Measured bounds overlap the bright region from a matching GPU draw. */
static void test_measure_vs_draw_bounds(void) {
    SituationFont font = sit_text_test_default_font();
    const char* text = "HELLO";
    const int text_x = 24;
    const int text_y = 28;
    const float font_size = 16.0f;

    SitRectangle bounds = SituationMeasureTextEx(font, text, font_size, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    SIT_ASSERT(bounds.height > 0.0f);

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(
            font, text, text_x, text_y, font_size, 0.0f, (ColorRGBA){255, 255, 255, 255}, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    const int band_right = text_x + (int)bounds.width + 6;
    const int band_bottom = text_y + (int)bounds.height + 6;
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, band_right, band_bottom, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, band_right + 12, text_y, band_right + 96, band_bottom, 12));

    SituationUnloadImage(screen);
}

/** CPU stamp — Roboto label with solid background into SituationImage. */
static void test_image_stamp_text_default(void) {
    SituationFont font = {0};
    sit_text_test_require_roboto_cpu(&font);

    SituationImage img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(160, 64, 4, &img), SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(img.data);
    memset(img.data, 0, (size_t)img.width * (size_t)img.height * 4u);

    const int stamp_x = 12;
    const int stamp_y = 10;
    ColorRGBA text_color = {255, 255, 255, 255};
    ColorRGBA bg_color = {16, 24, 200, 255};

    SIT_ASSERT_EQ(
        SituationImageStampText(
            &img, font, "Hi", (Vector2){{(float)stamp_x, (float)stamp_y}},
            20.0f, text_color, bg_color),
        SITUATION_SUCCESS);

    SIT_ASSERT(sit_text_test_pixel_blue_dominant(&img, stamp_x + 1, stamp_y + 1));
    SIT_ASSERT(graphics_test_region_any_bright(
        &img, stamp_x + 4, stamp_y + 4, stamp_x + 48, stamp_y + 28, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &img, stamp_x + 56, stamp_y, img.width - 1, stamp_y + 32, 12));

    SituationUnloadImage(img);
    sit_text_test_destroy_font(&font);
}

/** CPU boxed stamp — background fill + multiline text inside bounds. */
static void test_image_stamp_text_boxed(void) {
    SituationFont font = {0};
    sit_text_test_require_roboto_cpu(&font);

    SituationImage img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(128, 96, 4, &img), SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(img.data);
    memset(img.data, 0, (size_t)img.width * (size_t)img.height * 4u);

    const int bx = 8;
    const int by = 8;
    const int bw = 96;
    const int bh = 56;
    SitRectangle bounds = {(float)bx, (float)by, (float)bw, (float)bh};
    ColorRGBA text_color = {255, 255, 255, 255};
    ColorRGBA bg_color = {24, 180, 48, 255};
    int out_w = 0;
    int out_h = 0;

    SIT_ASSERT_EQ(
        SituationImageStampTextBoxed(
            &img, font, "Line1\nLine2", bounds, 16.0f, text_color, bg_color, true, &out_w, &out_h),
        SITUATION_SUCCESS);
    SIT_ASSERT(out_w > 0);
    SIT_ASSERT(out_h > 0);

    SIT_ASSERT(sit_text_test_pixel_green_dominant(&img, bx + bw - 4, by + 2));
    SIT_ASSERT(graphics_test_region_any_bright(
        &img, bx + 4, by + 4, bx + bw - 4, by + 20, 40));
    SIT_ASSERT(graphics_test_region_any_bright(
        &img, bx + 4, by + 22, bx + bw - 4, by + bh - 4, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &img, bx + bw + 8, by, img.width - 1, by + bh, 12));

    SituationUnloadImage(img);
    sit_text_test_destroy_font(&font);
}

/** Stamp label to image, upload texture, blit with CmdDrawTexture, readback. */
static void test_stamp_then_gpu_blit(void) {
    SituationFont font = {0};
    sit_text_test_require_roboto_cpu(&font);

    const int label_w = 96;
    const int label_h = 32;

    SituationImage label = {0};
    SIT_ASSERT_EQ(SituationCreateImage(label_w, label_h, 4, &label), SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(label.data);
    memset(label.data, 0, (size_t)label.width * (size_t)label.height * 4u);

    SIT_ASSERT_EQ(
        SituationImageStampText(
            &label, font, "OK", (Vector2){{4.0f, 4.0f}}, 18.0f,
            (ColorRGBA){255, 255, 255, 255}, (ColorRGBA){32, 32, 48, 255}),
        SITUATION_SUCCESS);

    SituationTexture tex = {0};
    SIT_ASSERT_EQ(SituationCreateTexture(label, false, &tex), SITUATION_SUCCESS);
    SituationUnloadImage(label);

    const int dx = 24;
    const int dy = 24;
    const int dw = label_w;
    const int dh = label_h;

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SIT_ASSERT_NOT_NULL(cmd);

    SituationRenderPassInfo rp;
    sit_text_test_fill_main_clear_pass(&rp);
    SIT_ASSERT_EQ(SituationCmdBeginRenderPass(cmd, &rp), SITUATION_SUCCESS);

    SitRectangle source = {0.0f, 0.0f, (float)label_w, (float)label_h};
    SitRectangle dest = {(float)dx, (float)dy, (float)dw, (float)dh};
    Vector2 origin = {{0.0f, 0.0f}};
    SIT_ASSERT_EQ(
        SituationCmdDrawTexture(
            cmd, tex, source, dest, origin, 0.0f, (ColorRGBA){255, 255, 255, 255}),
        SITUATION_SUCCESS);

    SIT_ASSERT_EQ(SituationCmdEndRenderPass(cmd), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage screen = {0};
    SIT_ASSERT_EQ(SituationLoadImageFromScreen(&screen), SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));

    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, dx, dy, dx + 72, dy + 24, 40));
    SIT_ASSERT(graphics_test_region_all_dark(
        &screen, dx + 80, dy, dx + 160, dy + 24, 12));

    SituationUnloadImage(screen);
    SituationDestroyTexture(&tex);
    sit_text_test_destroy_font(&font);
}

/** Missing font file — LoadFont fails cleanly; handle stays zeroed. */
static void test_font_load_missing_file_fails(void) {
    SituationFont font = {0};
    SituationError err = SituationLoadFont(
        "tests/harness/assets/__sit_test_font_not_present__.ttf", &font);
    SIT_ASSERT(err != SITUATION_SUCCESS);
    SIT_ASSERT_EQ(font.atlas_texture.generation, 0);
    SIT_ASSERT_NULL(font.stbFontInfo);
    SIT_ASSERT_NULL(font.fontData);
    SituationUnloadFont(font);
}

/**
 * Optional Roboto — honest [SKIP] when asset absent so later TTF tests are expected skips.
 * When present, trivial load/unload only (no bake).
 */
static void test_roboto_asset_optional_probe(void) {
    SituationFont font = {0};
    if (!sit_text_test_try_load_roboto_cpu(&font)) {
        SIT_TEST_SKIP(SIT_TEXT_TEST_ROBOTO_SKIP_MSG);
    }
    sit_text_test_destroy_font(&font);
}

/** Unbaked bitmap font on GPU draw falls back to the built-in default grid atlas. */
static void test_draw_without_bake_default_fallback(void) {
    unsigned char bitmap_data[256 * 8];
    sit_text_test_fill_synthetic_bitmap(bitmap_data, sizeof(bitmap_data), 65);

    SituationFont font = {0};
    SIT_ASSERT_EQ(SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(font.atlas_texture.generation, 0);

    const int text_x = 18;
    const int text_y = 22;
    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(
            font, "A", text_x, text_y, 16.0f, 0.0f, (ColorRGBA){255, 255, 255, 255}, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, text_x + 24, text_y + 20, 40));

    SituationUnloadImage(screen);
    SituationUnloadFont(font);
}

/** TTF load → bake → unload → load again succeeds. */
static void test_reload_font_after_unload(void) {
    SituationFont font = {0};
    sit_text_test_require_roboto_baked(&font, 16.0f);
    sit_text_test_destroy_font(&font);

    sit_text_test_require_roboto_baked(&font, 16.0f);
    SitRectangle bounds = SituationMeasureTextEx(font, "Again", 16.0f, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    SIT_ASSERT(font.atlas_texture.generation != 0);
    sit_text_test_destroy_font(&font);
}

/** Second UnloadFont on a zeroed handle after unload must not crash. */
static void test_double_unload_safe(void) {
    unsigned char bitmap_data[256 * 8];
    sit_text_test_fill_synthetic_bitmap(bitmap_data, sizeof(bitmap_data), 65);

    SituationFont font = {0};
    SIT_ASSERT_EQ(SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationBakeBitmapFontAtlas(&font), SITUATION_SUCCESS);

    SituationUnloadFont(font);
    memset(&font, 0, sizeof(font));
    SituationUnloadFont(font);
}

static SitTestCase text_rendering_tests[] = {
    /* Error paths & atlas / missing-asset hygiene — run first */
    {"draw_without_bake_default_fallback", test_draw_without_bake_default_fallback, true},
    {"font_load_missing_file_fails",       test_font_load_missing_file_fails,       true},
    {"double_unload_safe",                 test_double_unload_safe,                 true},
    {"font_unload_destroys_atlas",         test_font_unload_destroys_atlas,         true},
    {"roboto_asset_optional_probe",        test_roboto_asset_optional_probe,        true},
    /* Default grid — no external assets */
    {"cmd_draw_text_bitmap",               test_cmd_draw_text_bitmap,               true},
    {"measure_text_multiline",             test_measure_text_multiline,             true},
    {"get_text_line_count",                test_get_text_line_count,                true},
    {"measure_vs_draw_bounds",             test_measure_vs_draw_bounds,             true},
    {"cmd_draw_text_ex_bounds",            test_cmd_draw_text_ex_bounds,            true},
    {"cmd_draw_text_screen_layout",        test_cmd_draw_text_screen_layout,        true},
    {"cmd_draw_text_boxed_wrap",           test_cmd_draw_text_boxed_wrap,           true},
    {"cmd_draw_text_boxed_clip",           test_cmd_draw_text_boxed_clip,           true},
    {"cmd_draw_text_multiline_gpu",        test_cmd_draw_text_multiline_gpu,        true},
    {"cmd_draw_text_colored",              test_cmd_draw_text_colored,              true},
    /* Bitmap / sheet atlas paths */
    {"bitmap_memory_bake_gpu_draw",        test_bitmap_memory_bake_gpu_draw,        true},
    {"load_bitmap_font_from_texture",      test_load_bitmap_font_from_texture,      true},
    {"measure_text_bitmap_baked",          test_measure_text_bitmap_baked,          true},
    /* Optional Roboto TTF (skip if asset missing) */
    {"measure_text_ttf_baked",             test_measure_text_ttf_baked,             true},
    {"roboto_ttf_bake_draw",               test_roboto_ttf_bake_draw,               true},
    {"roboto_ttf_ex_bounds",               test_roboto_ttf_ex_bounds,               true},
    /* CPU stamp + lifecycle */
    {"image_stamp_text_default",           test_image_stamp_text_default,           true},
    {"image_stamp_text_boxed",             test_image_stamp_text_boxed,             true},
    {"stamp_then_gpu_blit",                test_stamp_then_gpu_blit,                true},
    {"reload_font_after_unload",           test_reload_font_after_unload,           true},
};

const SitTestModule g_module_text_rendering = {
    .name = "text_rendering",
    .setup = text_rendering_setup,
    .teardown = text_rendering_teardown,
    .tests = text_rendering_tests,
    .test_count = sizeof(text_rendering_tests) / sizeof(text_rendering_tests[0]),
    .requires_context = true
};
