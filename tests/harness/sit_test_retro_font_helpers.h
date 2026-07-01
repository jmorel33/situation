/**
 * @file sit_test_retro_font_helpers.h
 * @brief Fixtures and staged checks for retro font builder certification (T6).
 *
 * Stage A — usable: builder succeeds, atlas + grid metadata sane, measure > 0.
 * Stage B — display surface: GPU draw, tint, multiline, boxed wrap, measure-vs-draw.
 */

#ifndef SIT_TEST_RETRO_FONT_HELPERS_H
#define SIT_TEST_RETRO_FONT_HELPERS_H

#include "sit_test_text_helpers.h"

static inline void sit_text_test_assert_grid_font_usable(
    const SituationFont* font,
    int expect_cell_w,
    int expect_cell_h,
    int expect_first_char)
{
    SIT_ASSERT_NOT_NULL(font);
    SIT_ASSERT(font->atlas_texture.generation != 0);
    SIT_ASSERT(font->is_bitmap);
    SIT_ASSERT(font->atlas_width > 0);
    SIT_ASSERT(font->atlas_height > 0);
    SIT_ASSERT_EQ(font->display_cell_width, expect_cell_w);
    SIT_ASSERT_EQ(font->display_cell_height, expect_cell_h);
    SIT_ASSERT_EQ(font->first_char, expect_first_char);
    SIT_ASSERT(font->chars_per_row > 0);
}

/** Solid-fill one glyph in VCR 16-bit row layout (128 glyphs × 14 rows). */
static inline void sit_text_test_fill_vcr_glyph_solid(
    uint16_t* data, int glyph_index, int char_height)
{
    if (!data || glyph_index < 0 || char_height <= 0) {
        return;
    }
    for (int row = 0; row < char_height; row++) {
        data[glyph_index * char_height + row] = 0xFFF0u;
    }
}

static inline void sit_text_test_build_cp437_grid(unsigned char* grid, size_t grid_bytes)
{
    memset(grid, 0, grid_bytes);
    sit_text_test_fill_terminal_glyph(grid, 16, 8, 16, 0xDB, 255);
    sit_text_test_fill_terminal_glyph(grid, 16, 8, 16, 0xC4, 255);
    sit_text_test_fill_terminal_glyph(grid, 16, 8, 16, 0xB3, 255);
}

static inline void sit_text_test_build_terminal_ascii_grid(
    unsigned char* grid, size_t grid_bytes,
    int chars_per_row, int char_w, int char_h, int first_char)
{
    memset(grid, 0, grid_bytes);
    sit_text_test_fill_terminal_glyph(
        grid, chars_per_row, char_w, char_h, (int)'A' - first_char, 255);
    sit_text_test_fill_terminal_glyph(
        grid, chars_per_row, char_w, char_h, (int)'B' - first_char, 255);
    sit_text_test_fill_terminal_glyph(
        grid, chars_per_row, char_w, char_h, (int)'C' - first_char, 255);
}

static inline SituationError sit_text_test_build_cp437_font(SituationFont* out_font)
{
    static unsigned char cp437_grid[128 * 256];
    sit_text_test_build_cp437_grid(cp437_grid, sizeof(cp437_grid));
    return SituationCreateCP437Font(cp437_grid, out_font);
}

static inline SituationError sit_text_test_build_terminal_font(SituationFont* out_font)
{
    const int char_count = 96;
    const int chars_per_row = 16;
    const int char_w = 8;
    const int char_h = 8;
    unsigned char grid[16 * 8 * 6 * 8];
    sit_text_test_build_terminal_ascii_grid(grid, sizeof(grid), chars_per_row, char_w, char_h, 32);
    return SituationCreateTerminalFontFromMemory(
        grid, char_w, char_h, char_count, chars_per_row, 32, out_font);
}

static inline SituationError sit_text_test_build_ascii_font(SituationFont* out_font)
{
    const int char_w = 8;
    const int char_h = 12;
    unsigned char grid[16 * char_w * 6 * char_h];
    sit_text_test_build_terminal_ascii_grid(grid, sizeof(grid), 16, char_w, char_h, 32);
    return SituationCreateASCIIFont(grid, char_w, char_h, out_font);
}

static inline SituationError sit_text_test_build_packed_font(SituationFont* out_font)
{
    unsigned char packed[4 * 8];
    memset(packed, 0, sizeof(packed));
    sit_text_test_fill_packed_glyph_solid(packed, 0, 8);
    sit_text_test_fill_packed_glyph_solid(packed, 1, 8);
    sit_text_test_fill_packed_glyph_solid(packed, 2, 8);

    SituationPackedFont config;
    sit_text_test_fill_minimal_packed_config(&config, 4, (int)'A');
    return SituationCreatePackedBitmapFont(packed, &config, out_font);
}

static inline SituationError sit_text_test_build_outlined_packed_font(SituationFont* out_font)
{
    unsigned char packed[4 * 8];
    memset(packed, 0, sizeof(packed));
    sit_text_test_fill_packed_glyph_solid(packed, 0, 8);
    sit_text_test_fill_packed_glyph_solid(packed, 1, 8);
    sit_text_test_fill_packed_glyph_solid(packed, 2, 8);

    SituationPackedFont config;
    sit_text_test_fill_minimal_packed_config(&config, 4, (int)'A');
    config.enable_outline = true;
    config.outline_thickness = 1;
    config.outline_r = 0;
    config.outline_g = 0;
    config.outline_b = 0;
    config.outline_a = 255;
    return SituationCreateOutlinedPackedBitmapFont(packed, &config, out_font);
}

static inline SituationError sit_text_test_build_vcr_font(SituationFont* out_font)
{
    static uint16_t vcr_data[128 * 14];
    memset(vcr_data, 0, sizeof(vcr_data));
    sit_text_test_fill_vcr_glyph_solid(vcr_data, (int)'H', 14);
    sit_text_test_fill_vcr_glyph_solid(vcr_data, (int)'I', 14);
    return SituationCreateVCRFont(vcr_data, out_font);
}

static inline SituationError sit_text_test_build_vga_font(SituationFont* out_font)
{
    unsigned char vga_data[256 * 8];
    memset(vga_data, 0, sizeof(vga_data));
    for (int c = (int)'A'; c <= (int)'C'; c++) {
        sit_text_test_fill_vga_glyph_solid(vga_data, c);
    }
    return SituationCreateVGA8x8Font(vga_data, out_font);
}

/**
 * Stage B — what the font can do on the main display surface:
 * draw, colored tint, multiline, boxed wrap, measure-vs-draw overlap.
 */
static inline void sit_text_test_retro_display_surface(
    SituationFont font,
    const char* text,
    const char* multiline_text,
    const char* boxed_wrap_text,
    float font_size,
    int text_x,
    int text_y)
{
    ColorRGBA white = {255, 255, 255, 255};
    ColorRGBA red = {255, 32, 32, 255};

    int line2_y0 = text_y + (int)font_size + 2;
    int line2_y1 = line2_y0 + (int)font_size + 8;

    SituationImage screen = {0};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(
            font, text, text_x, text_y, font_size, 0.0f, white, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(screen));
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, text_x + 120, text_y + 32, 40));
    SituationUnloadImage(screen);

    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(
            font, text, text_x, text_y, font_size, 0.0f, red, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(sit_text_test_region_any_red_dominant(
        &screen, text_x, text_y, text_x + 120, text_y + 32));
    SituationUnloadImage(screen);

    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(
            font, multiline_text, text_x, text_y, font_size, 0.0f, white, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y, text_x + 96, text_y + (int)font_size + 6, 40));
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, line2_y0, text_x + 96, line2_y1, 40));
    SituationUnloadImage(screen);

    SitRectangle bounds = {
        (float)text_x, (float)text_y, 80.0f, 48.0f};
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_boxed_frame(
            font, boxed_wrap_text, bounds, font_size, 0.0f, white, true, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y,
        text_x + (int)bounds.width - 2, text_y + (int)bounds.height - 2, 40));
    SituationUnloadImage(screen);

    SitRectangle measured = SituationMeasureTextEx(font, text, font_size, 0.0f);
    SIT_ASSERT(measured.width > 0.0f);
    SIT_ASSERT(measured.height > 0.0f);
    SIT_ASSERT_EQ(
        sit_text_test_draw_text_ex_frame(
            font, text, text_x, text_y, font_size, 0.0f, white, &screen),
        SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_region_any_bright(
        &screen, text_x, text_y,
        text_x + (int)measured.width + 8, text_y + (int)measured.height + 8, 40));
    SituationUnloadImage(screen);
}

#endif /* SIT_TEST_RETRO_FONT_HELPERS_H */
