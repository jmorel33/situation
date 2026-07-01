/**
 * @file sit_test_text_helpers.h
 * @brief Shared helpers for text_rendering harness (fonts, readback bands).
 *
 * Requires Situation API — include sit_api_include.h and sit_graphics_test_helpers.h first.
 */

#ifndef SIT_TEST_TEXT_HELPERS_H
#define SIT_TEST_TEXT_HELPERS_H

#include "sit_test_assets.h"
#include "sit_test_framework.h"
#include <string.h>

#define SIT_TEXT_TEST_ROBOTO_SKIP_MSG \
    "Roboto TTF not found — place static/Roboto-Regular.ttf under tests/harness/assets/"

static const char* const g_sit_text_roboto_font_candidates[] = {
    "static/Roboto-Regular.ttf",
    "Roboto-Regular.ttf",
    NULL
};

static inline void sit_text_test_fill_main_clear_pass(SituationRenderPassInfo* rp) {
    memset(rp, 0, sizeof(*rp));
    rp->display_id = -1;
    rp->color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp->color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp->color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp->depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp->depth_attachment.clear.depth = 1.0f;
}

/** Zeroed handle → built-in default grid atlas at draw time. */
static inline SituationFont sit_text_test_default_font(void) {
    SituationFont font = {0};
    return font;
}

static inline bool sit_text_test_try_load_roboto_baked(SituationFont* out_font, float bake_px) {
    if (!out_font) {
        return false;
    }
    memset(out_font, 0, sizeof(*out_font));

    char path[512];
    if (!sit_test_resolve_harness_asset_any(g_sit_text_roboto_font_candidates, path, sizeof(path))) {
        return false;
    }

    SituationError err = SituationLoadFont(path, out_font);
    if (err != SITUATION_SUCCESS || !out_font->fontData || !out_font->stbFontInfo) {
        SituationUnloadFont(*out_font);
        memset(out_font, 0, sizeof(*out_font));
        return false;
    }

    err = SituationBakeFontAtlas(out_font, bake_px);
    if (err != SITUATION_SUCCESS
        || out_font->atlas_texture.generation == 0
        || !out_font->glyph_info) {
        SituationUnloadFont(*out_font);
        memset(out_font, 0, sizeof(*out_font));
        return false;
    }
    return true;
}

static inline void sit_text_test_require_roboto_baked(SituationFont* out_font, float bake_px) {
    if (!sit_text_test_try_load_roboto_baked(out_font, bake_px)) {
        SIT_TEST_SKIP(SIT_TEXT_TEST_ROBOTO_SKIP_MSG);
    }
}

/** Load Roboto for CPU stamp / ImageDrawText (no GPU bake required). */
static inline bool sit_text_test_try_load_roboto_cpu(SituationFont* out_font) {
    if (!out_font) {
        return false;
    }
    memset(out_font, 0, sizeof(*out_font));

    char path[512];
    if (!sit_test_resolve_harness_asset_any(g_sit_text_roboto_font_candidates, path, sizeof(path))) {
        return false;
    }

    SituationError err = SituationLoadFont(path, out_font);
    if (err != SITUATION_SUCCESS || !out_font->fontData || !out_font->stbFontInfo) {
        SituationUnloadFont(*out_font);
        memset(out_font, 0, sizeof(*out_font));
        return false;
    }
    return true;
}

static inline void sit_text_test_require_roboto_cpu(SituationFont* out_font) {
    if (!sit_text_test_try_load_roboto_cpu(out_font)) {
        SIT_TEST_SKIP(SIT_TEXT_TEST_ROBOTO_SKIP_MSG);
    }
}

static inline bool sit_text_test_pixel_blue_dominant(
    const SituationImage* img, int x, int y)
{
    uint8_t rgba[4];
    graphics_test_sample_rgba(img, x, y, rgba);
    return rgba[2] >= 160 && rgba[0] <= 96 && rgba[1] <= 96;
}

static inline bool sit_text_test_pixel_green_dominant(
    const SituationImage* img, int x, int y)
{
    uint8_t rgba[4];
    graphics_test_sample_rgba(img, x, y, rgba);
    return rgba[1] >= 160 && rgba[0] <= 96 && rgba[2] <= 96;
}

static inline void sit_text_test_destroy_font(SituationFont* font) {
    if (!font) {
        return;
    }
    SituationUnloadFont(*font);
    memset(font, 0, sizeof(*font));
}

/**
 * 8×8 1-bpp grid font buffer (256 glyphs × 8 row bytes).
 * Solid-fill one glyph index; all others empty.
 */
static inline void sit_text_test_fill_synthetic_bitmap(
    unsigned char* bitmap_data, size_t bitmap_bytes, int glyph_index)
{
    memset(bitmap_data, 0, bitmap_bytes);
    if (!bitmap_data || glyph_index < 0 || glyph_index >= 256) {
        return;
    }
    memset(bitmap_data + (size_t)glyph_index * 8u, 0xFF, 8u);
}

static inline int sit_text_test_count_bright_pixels(
    const SituationImage* screen,
    int x0, int y0, int x1, int y1,
    uint8_t min_channel)
{
    if (!screen || !SituationIsImageValid(*screen)) {
        return 0;
    }
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= screen->width) {
        x1 = screen->width - 1;
    }
    if (y1 >= screen->height) {
        y1 = screen->height - 1;
    }

    int count = 0;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (graphics_test_pixel_bright(screen, x, y, min_channel)) {
                count++;
            }
        }
    }
    return count;
}

/** Draw text in a black-cleared main pass, end frame, load screen image. Caller unloads screen. */
static inline SituationError sit_text_test_draw_text_ex_frame(
    SituationFont font,
    const char* text,
    int text_x,
    int text_y,
    float font_size,
    float spacing,
    ColorRGBA color,
    SituationImage* out_screen)
{
    if (!text || !out_screen) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SituationPollInputEvents();
    SituationUpdateTimers();
    SituationError err = SituationAcquireFrameCommandBuffer();
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    SituationRenderPassInfo rp;
    sit_text_test_fill_main_clear_pass(&rp);
    err = SituationCmdBeginRenderPass(cmd, &rp);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    Vector2 pos = {{(float)text_x, (float)text_y}};
    err = SituationCmdDrawTextEx(cmd, font, text, pos, font_size, spacing, color);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    err = SituationCmdEndRenderPass(cmd);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    err = SituationEndFrame();
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    memset(out_screen, 0, sizeof(*out_screen));
    return SituationLoadImageFromScreen(out_screen);
}

/** Draw boxed text in a black-cleared main pass, end frame, load screen image. Caller unloads screen. */
static inline SituationError sit_text_test_draw_text_boxed_frame(
    SituationFont font,
    const char* text,
    SitRectangle bounds,
    float font_size,
    float spacing,
    ColorRGBA color,
    bool word_wrap,
    SituationImage* out_screen)
{
    if (!text || !out_screen) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SituationPollInputEvents();
    SituationUpdateTimers();
    SituationError err = SituationAcquireFrameCommandBuffer();
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    SituationRenderPassInfo rp;
    sit_text_test_fill_main_clear_pass(&rp);
    err = SituationCmdBeginRenderPass(cmd, &rp);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    err = SituationCmdDrawTextBoxed(
        cmd, font, text, bounds, font_size, spacing, color, word_wrap);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    err = SituationCmdEndRenderPass(cmd);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    err = SituationEndFrame();
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    memset(out_screen, 0, sizeof(*out_screen));
    return SituationLoadImageFromScreen(out_screen);
}

static inline bool sit_text_test_pixel_red_dominant(
    const SituationImage* screen, int x, int y)
{
    uint8_t rgba[4];
    graphics_test_sample_rgba(screen, x, y, rgba);
    return rgba[0] >= 160 && rgba[1] <= 96 && rgba[2] <= 96;
}

static inline bool sit_text_test_region_any_red_dominant(
    const SituationImage* screen, int x0, int y0, int x1, int y1)
{
    if (!screen || !SituationIsImageValid(*screen)) {
        return false;
    }
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (sit_text_test_pixel_red_dominant(screen, x, y)) {
                return true;
            }
        }
    }
    return false;
}

/** Assert MeasureTextEx width roughly doubles when fontSize doubles (grid or baked TTF). */
static inline void sit_text_test_assert_measure_width_scales(
    SituationFont font, const char* text, float size_small, float size_large)
{
    SitRectangle small = SituationMeasureTextEx(font, text, size_small, 0.0f);
    SitRectangle large = SituationMeasureTextEx(font, text, size_large, 0.0f);
    SIT_ASSERT(small.width > 0.0f);
    SIT_ASSERT(large.width > small.width);
    float ratio = large.width / small.width;
    SIT_ASSERT(ratio >= 1.75f && ratio <= 2.25f);
}

/** Fill one glyph cell in a grayscale terminal grid (CreateTerminalFont* / CP437). */
static inline void sit_text_test_fill_terminal_glyph(
    unsigned char* grid,
    int chars_per_row,
    int char_width,
    int char_height,
    int glyph_index,
    unsigned char value)
{
    if (!grid || glyph_index < 0 || chars_per_row <= 0 || char_width <= 0 || char_height <= 0) {
        return;
    }
    int src_char_x = (glyph_index % chars_per_row) * char_width;
    int src_char_y = (glyph_index / chars_per_row) * char_height;
    int source_width = chars_per_row * char_width;
    for (int y = 0; y < char_height; y++) {
        for (int x = 0; x < char_width; x++) {
            grid[(src_char_y + y) * source_width + (src_char_x + x)] = value;
        }
    }
}

/** Minimal 8×8 packed retro config (L8 / inline fixtures). */
static inline void sit_text_test_fill_minimal_packed_config(
    SituationPackedFont* cfg, int char_count, int first_char)
{
    if (!cfg || char_count <= 0) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->char_width = 8;
    cfg->char_height = 8;
    cfg->display_height = 10;
    cfg->char_count = char_count;
    cfg->first_char = first_char;
    cfg->chars_per_row = char_count;
    cfg->bits_per_row = 8;
    cfg->data_bits = 8;
    cfg->data_bit_offset = 0;
    cfg->bit_order_msb_first = true;
    cfg->top_padding = 1;
    cfg->bottom_padding = 1;
    cfg->left_padding = 1;
    cfg->right_padding = 1;
    cfg->atlas_chars_per_row = 16;
    cfg->atlas_chars_per_col = (char_count + 15) / 16;
    cfg->font_r = 255;
    cfg->font_g = 255;
    cfg->font_b = 255;
    cfg->font_a = 255;
}

/** Solid-fill one glyph in MSB-first 8×8 packed rows (one byte per row). */
static inline void sit_text_test_fill_packed_glyph_solid(
    unsigned char* packed, int glyph_index, int char_height)
{
    if (!packed || glyph_index < 0 || char_height <= 0) {
        return;
    }
    memset(packed + (size_t)glyph_index * (size_t)char_height, 0xFF, (size_t)char_height);
}

/** Solid-fill one glyph in VGA 8×8 packed layout (256 sequential glyphs). */
static inline void sit_text_test_fill_vga_glyph_solid(unsigned char* data, int glyph_index)
{
    sit_text_test_fill_packed_glyph_solid(data, glyph_index, 8);
}

#endif /* SIT_TEST_TEXT_HELPERS_H */
