/**
 * @file test_advanced_font_showcase.c
 * @brief Advanced harness — fullscreen font + YPQ + timing showcase (~30 s).
 *
 * See doc/plan/TEST_HARNESS_ADVANCED_FONT_SHOWCASE_PLAN.md
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_hud.h"
#include "sit_test_text_helpers.h"
#include "sit_test_retro_font_helpers.h"
#include "situation_base_font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ADV_SEG_COUNT 8
#define ADV_CHROME_BAR 22.0f
#define ADV_CHW 10.0f
#define ADV_VGA_MAX_BODY 16.0f
#define ADV_VGA_MAX_HERO 24.0f
#define ADV_YPQ_STRIPE_PX 2.0f

typedef struct AdvFontBundle {
    bool has_roboto;
    bool has_roboto_cpu;
    SituationFont roboto_gpu;
    SituationFont roboto_cpu;
    SituationFont cp437;
    SituationFont terminal;
    SituationFont ascii;
    SituationFont packed;
    SituationFont outlined_packed;
    SituationFont vcr_outline;
    SituationFont vga_outline;
} AdvFontBundle;

typedef struct AdvCpuTextTex {
    SituationTexture tex;
    int w;
    int h;
    bool valid;
} AdvCpuTextTex;

typedef void (*AdvSegDrawFn)(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t);

typedef struct AdvFontSegment {
    const char* title;
    double duration_sec;
    AdvSegDrawFn draw;
} AdvFontSegment;

static ColorYPQA g_adv_ypq_stops[8];
static int g_adv_ypq_ready = 0;

static SituationFont adv_gpu_font(const AdvFontBundle* fb) {
    return fb->has_roboto ? fb->roboto_gpu : sit_text_test_default_font();
}

static SituationFont adv_cpu_font(const AdvFontBundle* fb) {
    if (fb->has_roboto_cpu) {
        return fb->roboto_cpu;
    }
    return sit_text_test_default_font();
}

static float adv_gpu_size(const AdvFontBundle* fb, float want, float max_no_roboto) {
    if (fb && fb->has_roboto) {
        return want;
    }
    return want > max_no_roboto ? max_no_roboto : want;
}

static const unsigned char* adv_vga_glyph_rows(int char_code) {
    return &sit_default_8x8_font[(unsigned char)char_code * 8u];
}

static void adv_terminal_copy_vga_char(
    unsigned char* grid,
    int chars_per_row,
    int char_w,
    int char_h,
    int first_char,
    int char_code)
{
    if (!grid || char_code < first_char) {
        return;
    }
    const int glyph_index = char_code - first_char;
    const unsigned char* src = adv_vga_glyph_rows(char_code);
    const int src_char_x = (glyph_index % chars_per_row) * char_w;
    const int src_char_y = (glyph_index / chars_per_row) * char_h;
    const int source_width = chars_per_row * char_w;
    for (int y = 0; y < char_w && y < char_h; y++) {
        for (int x = 0; x < char_w; x++) {
            const unsigned char on = (src[y] & (unsigned char)(0x80u >> x)) ? 255u : 0u;
            grid[(src_char_y + y) * source_width + (src_char_x + x)] = on;
        }
    }
}

static void adv_packed_copy_vga_char(unsigned char* packed, int glyph_index, int char_h, int char_code) {
    if (!packed || glyph_index < 0) {
        return;
    }
    memcpy(packed + (size_t)glyph_index * (size_t)char_h, adv_vga_glyph_rows(char_code), (size_t)char_h);
}

static uint16_t adv_vga_row_to_vcr(uint8_t row) {
    uint16_t out = 0;
    for (int x = 0; x < 8; x++) {
        if (row & (uint8_t)(0x80u >> x)) {
            out |= (uint16_t)(1u << (11 - (2 + x)));
        }
    }
    return out;
}

static void adv_vcr_copy_vga_char(uint16_t* vcr_data, int glyph_index, int char_height, int char_code) {
    if (!vcr_data || glyph_index < 0 || char_height <= 0) {
        return;
    }
    const unsigned char* src = adv_vga_glyph_rows(char_code);
    const int base = glyph_index * char_height;
    for (int row = 0; row < char_height; row++) {
        vcr_data[base + row] = 0;
    }
    for (int row = 0; row < 8 && (1 + row) < char_height; row++) {
        vcr_data[base + 1 + row] = adv_vga_row_to_vcr(src[row]);
    }
}

static SituationError adv_build_showcase_cp437_font(SituationFont* out_font) {
    static unsigned char cp437_grid[128 * 256];
    memset(cp437_grid, 0, sizeof(cp437_grid));
    static const int demo_chars[] = {0xDB, 0xC4, 0xB3, 'H', 'i', '!', 0};
    for (int i = 0; demo_chars[i] != 0; i++) {
        adv_terminal_copy_vga_char(cp437_grid, 16, 8, 16, 0, demo_chars[i]);
    }
    return SituationCreateCP437Font(cp437_grid, out_font);
}

static SituationError adv_build_showcase_terminal_font(SituationFont* out_font) {
    const int char_count = 96;
    const int chars_per_row = 16;
    const int char_w = 8;
    const int char_h = 8;
    unsigned char grid[16 * 8 * 6 * 8];
    memset(grid, 0, sizeof(grid));
    adv_terminal_copy_vga_char(grid, chars_per_row, char_w, char_h, 32, 'A');
    adv_terminal_copy_vga_char(grid, chars_per_row, char_w, char_h, 32, 'B');
    adv_terminal_copy_vga_char(grid, chars_per_row, char_w, char_h, 32, 'C');
    return SituationCreateTerminalFontFromMemory(
        grid, char_w, char_h, char_count, chars_per_row, 32, out_font);
}

static SituationError adv_build_showcase_ascii_font(SituationFont* out_font) {
    const int char_w = 8;
    const int char_h = 12;
    unsigned char grid[16 * char_w * 6 * char_h];
    memset(grid, 0, sizeof(grid));
    adv_terminal_copy_vga_char(grid, 16, char_w, char_h, 32, 'A');
    adv_terminal_copy_vga_char(grid, 16, char_w, char_h, 32, 'B');
    adv_terminal_copy_vga_char(grid, 16, char_w, char_h, 32, 'C');
    return SituationCreateASCIIFont(grid, char_w, char_h, out_font);
}

static SituationError adv_build_showcase_packed_font(SituationFont* out_font) {
    unsigned char packed[4 * 8];
    memset(packed, 0, sizeof(packed));
    adv_packed_copy_vga_char(packed, 0, 8, 'A');
    adv_packed_copy_vga_char(packed, 1, 8, 'B');
    adv_packed_copy_vga_char(packed, 2, 8, 'C');

    SituationPackedFont config;
    sit_text_test_fill_minimal_packed_config(&config, 4, (int)'A');
    return SituationCreatePackedBitmapFont(packed, &config, out_font);
}

static SituationError adv_build_showcase_outlined_packed_font(SituationFont* out_font) {
    unsigned char packed[4 * 8];
    memset(packed, 0, sizeof(packed));
    adv_packed_copy_vga_char(packed, 0, 8, 'A');
    adv_packed_copy_vga_char(packed, 1, 8, 'B');
    adv_packed_copy_vga_char(packed, 2, 8, 'C');

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

static void adv_text_ex(
    SituationCommandBuffer cmd,
    SituationFont font,
    const char* s,
    float x,
    float y,
    float size,
    float spacing,
    ColorRGBA c)
{
    SituationCmdDrawTextEx(cmd, font, s, (Vector2){{x, y}}, size, spacing, c);
}

static void adv_ypq_init_once(void) {
    if (g_adv_ypq_ready) {
        return;
    }
    static const ColorRGBA anchors[8] = {
        {12, 14, 28, 255},
        {34, 18, 12, 255},
        {48, 120, 140, 255},
        {80, 90, 110, 255},
        {200, 140, 48, 255},
        {232, 180, 90, 255},
        {240, 245, 250, 255},
        {100, 105, 120, 255},
    };
    for (int i = 0; i < 8; i++) {
        g_adv_ypq_stops[i] = SituationColorToYPQ(anchors[i]);
    }
    g_adv_ypq_ready = 1;
}

static ColorRGBA adv_ypq_lerp_rgba(ColorYPQA a, ColorYPQA b, float t) {
    return SituationColorFromYPQ(SituationYpqLerp(a, b, t));
}

static ColorRGBA adv_ypq_sample(float t) {
    adv_ypq_init_once();
    t -= floorf(t);
    if (t < 0.0f) {
        t += 1.0f;
    }
    float u = t * 7.0f;
    int i0 = (int)u;
    if (i0 > 6) {
        i0 = 6;
    }
    float f = u - (float)i0;
    return adv_ypq_lerp_rgba(g_adv_ypq_stops[i0], g_adv_ypq_stops[i0 + 1], f);
}

static ColorRGBA adv_ypq_modulate(ColorYPQA base, float time, float phase_rate, float chroma_rate, float luma_rate) {
    adv_ypq_init_once();
    ColorYPQA y = base;
    y = SituationYpqAdjustPhase(y, (int)(time * phase_rate * 40.0f));
    y = SituationYpqAdjustChroma(y, 1.0f + sinf(time * chroma_rate) * 0.25f);
    y = SituationYpqAdjustLuma(y, 1.0f + sinf(time * luma_rate * 1.7f) * 0.12f);
    return SituationColorFromYPQ(y);
}

static ColorRGBA adv_ypq_band_sample(float y_norm, float scroll) {
    adv_ypq_init_once();
    float u = y_norm + scroll;
    u -= floorf(u);
    if (u < 0.0f) {
        u += 1.0f;
    }
    u = u * 0.85f + 0.05f;
    float u7 = u * 7.0f;
    int i0 = (int)u7;
    if (i0 > 6) {
        i0 = 6;
    }
    float f = u7 - (float)i0;
    f = f * f * (3.0f - 2.0f * f);
    return adv_ypq_lerp_rgba(g_adv_ypq_stops[i0], g_adv_ypq_stops[i0 + 1], f);
}

static ColorRGBA adv_ypq_clear_color(void) {
    adv_ypq_init_once();
    return SituationColorFromYPQ(g_adv_ypq_stops[0]);
}

static void adv_scissor_rect(SituationCommandBuffer cmd, int x, int y, int w, int h) {
    if (w < 1) {
        w = 1;
    }
    if (h < 1) {
        h = 1;
    }
    SituationCmdSetScissor(cmd, x, y, w, h);
}

static void adv_scissor_reset(SituationCommandBuffer cmd, float sw, float sh) {
    adv_scissor_rect(cmd, 0, 0, (int)sw, (int)sh);
}

static void adv_draw_ypq_banded_text(
    SituationCommandBuffer cmd,
    SituationFont font,
    const char* text,
    float x,
    float y,
    float size,
    float spacing,
    float scroll,
    float sw,
    float sh)
{
    if (!text || !text[0]) {
        return;
    }
    SitRectangle m = SituationMeasureTextEx(font, text, size, spacing);
    const float cap_h = size * 1.10f;
    float stripe_h = ADV_YPQ_STRIPE_PX;
    int bands = (int)ceilf(cap_h / stripe_h);
    if (bands < 20) {
        bands = 20;
    }
    if (bands > 56) {
        bands = 56;
    }
    stripe_h = cap_h / (float)bands;
    const float tw = m.width + 8.0f;

    for (int b = 0; b < bands; b++) {
        const float y0 = y + (float)b * stripe_h;
        const float v = ((float)b + 0.5f) / (float)bands;
        ColorRGBA col = adv_ypq_band_sample(v, scroll);
        adv_scissor_rect(cmd, (int)x, (int)y0, (int)tw, (int)(stripe_h + 1.0f));
        adv_text_ex(cmd, font, text, x, y, size, spacing, col);
    }
    adv_scissor_reset(cmd, sw, sh);
}

static void adv_chrome_fill(SituationCommandBuffer cmd, float x, float y, float w, float h, Vector4 col) {
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
}

static void adv_chrome_text(SituationCommandBuffer cmd, const char* s, float x, float y, ColorRGBA col) {
    adv_text_ex(cmd, sit_text_test_default_font(), s, x, y, 16.0f, 1.0f, col);
}

static void adv_chrome_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    const char* title,
    int seg_idx,
    int seg_count,
    float sw,
    float sh)
{
    const Vector4 bar_bg = {{0.0f, 0.0f, 0.0f, 0.80f}};
    adv_chrome_fill(cmd, 0.0f, 0.0f, sw, ADV_CHROME_BAR, bar_bg);

    if (title && title[0]) {
        adv_chrome_text(cmd, title, 4.0f, 3.0f, (ColorRGBA){220, 220, 220, 255});
    }

    float rx = sw - 4.0f;
    {
        const char* tok = "VSync:OFF";
        rx -= (float)strlen(tok) * ADV_CHW;
        adv_chrome_text(cmd, tok, rx, 3.0f, (ColorRGBA){200, 80, 80, 255});
        rx -= 12.0f;
    }
    {
        char fps_buf[16];
        snprintf(fps_buf, sizeof(fps_buf), "FPS:%d", SituationGetFPS());
        rx -= (float)strlen(fps_buf) * ADV_CHW;
        adv_chrome_text(cmd, fps_buf, rx, 3.0f, (ColorRGBA){255, 240, 80, 255});
    }

    const float bot_y = sh - ADV_CHROME_BAR;
    adv_chrome_fill(cmd, 0.0f, bot_y, sw, ADV_CHROME_BAR, bar_bg);
    {
        char seg_buf[96];
        if (fonts && !fonts->has_roboto) {
            snprintf(
                seg_buf,
                sizeof(seg_buf),
                "Segment %d/%d · default VGA font (add Roboto for TTF) · ESC:quit",
                seg_idx + 1,
                seg_count);
        } else {
            snprintf(
                seg_buf,
                sizeof(seg_buf),
                "Segment %d/%d · advanced · ESC:quit",
                seg_idx + 1,
                seg_count);
        }
        adv_chrome_text(cmd, seg_buf, 4.0f, bot_y + 3.0f, (ColorRGBA){200, 220, 255, 220});
    }
}

static SituationError adv_build_vcr_outline_font(SituationFont* out_font) {
    static uint16_t vcr_data[128 * 14];
    memset(vcr_data, 0, sizeof(vcr_data));
    adv_vcr_copy_vga_char(vcr_data, (int)'O', 14, 'O');
    adv_vcr_copy_vga_char(vcr_data, (int)'U', 14, 'U');
    adv_vcr_copy_vga_char(vcr_data, (int)'T', 14, 'T');
    return SituationCreateVCRFontWithOutline(vcr_data, 1, out_font);
}

static SituationError adv_build_vga_outline_font(SituationFont* out_font) {
    return SituationCreateVGA8x8FontWithOutline(sit_default_8x8_font, 1, out_font);
}

static void adv_cpu_tex_destroy(AdvCpuTextTex* ct) {
    if (!ct) {
        return;
    }
    if (ct->valid) {
        SituationDestroyTexture(&ct->tex);
        ct->valid = false;
    }
    ct->w = 0;
    ct->h = 0;
}

static bool adv_cpu_text_ex_to_texture(
    const AdvFontBundle* fonts,
    AdvCpuTextTex* out,
    const char* text,
    float font_size,
    float spacing,
    float rotation,
    float skew,
    ColorRGBA fill,
    ColorRGBA outline,
    float outline_thickness)
{
    if (!fonts || !out || !text) {
        return false;
    }

    adv_cpu_tex_destroy(out);

    SituationFont font = adv_cpu_font(fonts);
    if (!fonts->has_roboto_cpu) {
        return false;
    }

    SitRectangle m = SituationMeasureTextEx(font, text, font_size, spacing);
    const int pad = 24;
    int w = (int)m.width + pad * 2;
    int h = (int)m.height + pad * 2;
    if (w < 32) {
        w = 32;
    }
    if (h < 32) {
        h = 32;
    }

    SituationImage img = {0};
    if (SituationCreateImage(w, h, 4, &img) != SITUATION_SUCCESS || !img.data) {
        return false;
    }
    memset(img.data, 0, (size_t)w * (size_t)h * 4u);

    const Vector2 pos = {{(float)pad, (float)pad}};
    const ColorRGBA no_outline = {0, 0, 0, 0};

    if (outline_thickness > 0.0f && outline.a > 0) {
        /*
         * Real SDF outline via SituationImageDrawTextEx can fault when a baked GPU
         * Roboto atlas is loaded in the same process; use offset shadow until fixed.
         */
        SituationImageDrawTextEx(
            &img, font, text, (Vector2){{pos.x + 2.0f, pos.y + 2.0f}},
            font_size, spacing, rotation, skew, outline, no_outline, 0.0f);
    }

    SituationImageDrawTextEx(
        &img, font, text, pos, font_size, spacing, rotation, skew, fill, no_outline, 0.0f);

    if (SituationCreateTexture(img, false, &out->tex) != SITUATION_SUCCESS) {
        SituationUnloadImage(img);
        return false;
    }
    SituationUnloadImage(img);
    out->w = w;
    out->h = h;
    out->valid = true;
    return true;
}

static void adv_blit_cpu_tex(
    SituationCommandBuffer cmd,
    const AdvCpuTextTex* ct,
    float x,
    float y,
    float rotation,
    ColorRGBA tint)
{
    if (!ct || !ct->valid) {
        return;
    }
    SitRectangle source = {0.0f, 0.0f, (float)ct->w, (float)ct->h};
    SitRectangle dest = {x, y, (float)ct->w, (float)ct->h};
    Vector2 origin = {{(float)ct->w * 0.5f, (float)ct->h * 0.5f}};
    SituationCmdDrawTexture(cmd, ct->tex, source, dest, origin, rotation, tint);
}

static AdvFontBundle g_adv_fonts;

static void adv_font_load_all(AdvFontBundle* fb) {
    memset(fb, 0, sizeof(*fb));
    (void)sit_text_test_try_load_roboto_baked(&fb->roboto_gpu, 32.0f);
    fb->has_roboto = fb->roboto_gpu.atlas_texture.generation != 0;
    fb->has_roboto_cpu = sit_text_test_try_load_roboto_cpu(&fb->roboto_cpu);
    (void)adv_build_showcase_cp437_font(&fb->cp437);
    (void)adv_build_showcase_terminal_font(&fb->terminal);
    (void)adv_build_showcase_ascii_font(&fb->ascii);
    (void)adv_build_showcase_packed_font(&fb->packed);
    (void)adv_build_showcase_outlined_packed_font(&fb->outlined_packed);
    (void)adv_build_vcr_outline_font(&fb->vcr_outline);
    (void)adv_build_vga_outline_font(&fb->vga_outline);
}

static void adv_font_unload_all(AdvFontBundle* fb) {
    if (!fb) {
        return;
    }
    sit_text_test_destroy_font(&fb->roboto_gpu);
    sit_text_test_destroy_font(&fb->roboto_cpu);
    sit_text_test_destroy_font(&fb->cp437);
    sit_text_test_destroy_font(&fb->terminal);
    sit_text_test_destroy_font(&fb->ascii);
    sit_text_test_destroy_font(&fb->packed);
    sit_text_test_destroy_font(&fb->outlined_packed);
    sit_text_test_destroy_font(&fb->vcr_outline);
    sit_text_test_destroy_font(&fb->vga_outline);
    fb->has_roboto = false;
    fb->has_roboto_cpu = false;
}

static void adv_seg_typography_baseline_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t)
{
    (void)seg_t;
    SituationFont font = adv_gpu_font(fonts);
    const float cx = sw * 0.5f;
    const char* title = "Situation Typography";
    const float title_size = adv_gpu_size(fonts, 28.0f, ADV_VGA_MAX_HERO);
    SitRectangle tm = SituationMeasureTextEx(font, title, title_size, 1.0f);
    adv_text_ex(cmd, font, title, cx - tm.width * 0.5f, sh * 0.18f, title_size, 1.0f,
                adv_ypq_sample(0.35f));

    static const float sizes[] = {12.0f, 18.0f, 24.0f, 32.0f};
    float y = sh * 0.30f;
    for (int i = 0; i < 4; i++) {
        char buf[32];
        const float draw_size = adv_gpu_size(fonts, sizes[i], ADV_VGA_MAX_BODY);
        snprintf(buf, sizeof(buf), "Size %.0f", sizes[i]);
        adv_text_ex(cmd, font, buf, sw * 0.12f, y, draw_size, 0.0f, adv_ypq_sample(0.2f + (float)i * 0.12f));
        y += draw_size + 10.0f;
    }

    const float track_size = adv_gpu_size(fonts, 18.0f, ADV_VGA_MAX_BODY);
    adv_text_ex(cmd, font, "TRACKING", sw * 0.12f, sh * 0.62f, track_size, -1.0f, adv_ypq_modulate(g_adv_ypq_stops[4], t, 0.2f, 0.0f, 0.0f));
    adv_text_ex(cmd, font, "TRACKING", sw * 0.12f, sh * 0.68f, track_size, 0.0f, adv_ypq_modulate(g_adv_ypq_stops[5], t, 0.2f, 0.0f, 0.0f));
    adv_text_ex(cmd, font, "TRACKING", sw * 0.12f, sh * 0.74f, track_size, 4.0f, adv_ypq_modulate(g_adv_ypq_stops[6], t, 0.2f, 0.0f, 0.0f));

    adv_text_ex(cmd, sit_text_test_default_font(), "GPU · CmdDrawTextEx · scale · spacing",
                sw * 0.12f, sh * 0.84f, 16.0f, 0.0f, SituationColorFromYPQ(g_adv_ypq_stops[7]));
}

static void adv_seg_retro_gallery_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t)
{
    (void)t;
    (void)seg_t;
    const struct {
        const char* label;
        SituationFont font;
        const char* sample;
    } panels[] = {
        {"CP437", fonts->cp437, "\xDB\xC4\xB3"},
        {"Terminal", fonts->terminal, "ABC"},
        {"ASCII", fonts->ascii, "ABC"},
        {"Packed", fonts->packed, "ABC"},
        {"OutlnPk", fonts->outlined_packed, "ABC"},
        {"VCR out", fonts->vcr_outline, "OUT"},
        {"VGA out", fonts->vga_outline, "OUT"},
    };
    const int count = (int)(sizeof(panels) / sizeof(panels[0]));
    const float pw = sw * 0.20f;
    const float ph = sh * 0.26f;

    for (int i = 0; i < count; i++) {
        const int col = i % 4;
        const int row = i / 4;
        const float px = sw * 0.06f + (float)col * (pw + sw * 0.025f);
        const float py = sh * 0.20f + (float)row * (ph + sh * 0.05f);
        ColorRGBA tint = adv_ypq_sample(0.15f + (float)i * 0.1f);
        adv_text_ex(cmd, sit_text_test_default_font(), panels[i].label, px, py, 14.0f, 0.0f,
                    (ColorRGBA){180, 190, 210, 255});
        adv_text_ex(cmd, panels[i].font, panels[i].sample, px, py + 22.0f, 20.0f, 1.0f, tint);
    }
}

static void adv_seg_ypq_color_field_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t)
{
    (void)seg_t;
    SituationFont font = adv_gpu_font(fonts);
    const char* hero = "PERCEPTUAL COLOR GRADING";
    const float hx = sw * 0.08f;
    const float hy = sh * 0.28f;
    const float hero_size = adv_gpu_size(fonts, 22.0f, ADV_VGA_MAX_HERO);
    const float band_size = adv_gpu_size(fonts, 20.0f, ADV_VGA_MAX_BODY);
    float cx = hx;

    for (int i = 0; hero[i] != '\0'; i++) {
        char ch[2] = {hero[i], '\0'};
        ColorYPQA base = g_adv_ypq_stops[2 + (i % 5)];
        ColorRGBA col = adv_ypq_modulate(base, t + (float)i * 0.15f, 0.35f, 0.5f, 0.4f);
        adv_text_ex(cmd, font, ch, cx, hy, hero_size, 1.0f, col);
        SitRectangle cm = SituationMeasureTextEx(font, ch, hero_size, 1.0f);
        cx += cm.width + 1.0f;
    }

    adv_draw_ypq_banded_text(cmd, font, hero, hx, hy + 56.0f, band_size, 1.0f, t * 0.12f, sw, sh);

    ColorYPQA sec_base = g_adv_ypq_stops[4];
    ColorRGBA sec = adv_ypq_modulate(sec_base, t, 0.6f, 0.8f, 0.5f);
    adv_text_ex(cmd, font, "Y luma · P phase · Q chroma", sw * 0.08f, sh * 0.58f,
                adv_gpu_size(fonts, 18.0f, ADV_VGA_MAX_BODY), 0.5f, sec);
}

static int g_adv_skew_tick = -1;

static void adv_seg_motion_timing_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t)
{
    (void)seg_t;
    SituationFont font = adv_gpu_font(fonts);
    const float wave_y = sh * 0.28f + sinf(t * 4.0f) * 24.0f;
    adv_text_ex(cmd, font, "~ sine wave baseline ~", sw * 0.28f, wave_y, adv_gpu_size(fonts, 18.0f, ADV_VGA_MAX_BODY), 1.0f,
                adv_ypq_modulate(g_adv_ypq_stops[5], t, 0.3f, 0.4f, 0.8f));

    const float ox = sw * 0.5f + cosf(t * 2.4f) * sw * 0.18f;
    const float oy = sh * 0.48f + sinf(t * 2.4f) * sh * 0.08f;
    adv_text_ex(cmd, font, "Situation", ox - 40.0f, oy, adv_gpu_size(fonts, 20.0f, ADV_VGA_MAX_BODY), 1.0f, adv_ypq_sample(t * 0.08f));

    const float breathe = adv_gpu_size(fonts, 20.0f, ADV_VGA_MAX_BODY) * (1.0f + 0.10f * sinf(t * 4.0f));
    adv_text_ex(cmd, font, "BREATHE", sw * 0.12f, sh * 0.55f, breathe, 2.0f,
                adv_ypq_modulate(g_adv_ypq_stops[4], t, 0.1f, 0.2f, 0.0f));

    adv_text_ex(cmd, font, "PULSE", sw * 0.12f, sh * 0.68f, adv_gpu_size(fonts, 22.0f, ADV_VGA_MAX_HERO), 1.0f,
                adv_ypq_modulate(g_adv_ypq_stops[6], t, 0.0f, 0.0f, 1.2f));

    char time_buf[32];
    snprintf(time_buf, sizeof(time_buf), "t = %.2f s", t);
    adv_text_ex(cmd, font, time_buf, sw * 0.65f, sh * 0.62f, 16.0f, 0.0f,
                SituationColorFromYPQ(g_adv_ypq_stops[6]));
}

static AdvCpuTextTex g_adv_outline_cpu_tex = {0};

static void adv_seg_outline_depth_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t)
{
    (void)t;
    const char* text = "OUTLINE DEPTH TEST";

    if (!g_adv_outline_cpu_tex.valid && fonts->has_roboto_cpu) {
        (void)adv_cpu_text_ex_to_texture(
            fonts,
            &g_adv_outline_cpu_tex,
            text,
            22.0f,
            1.0f,
            0.0f,
            0.0f,
            SituationColorFromYPQ(g_adv_ypq_stops[5]),
            SituationColorFromYPQ(g_adv_ypq_stops[1]),
            2.0f);
    }
    if (g_adv_outline_cpu_tex.valid) {
        adv_blit_cpu_tex(cmd, &g_adv_outline_cpu_tex, sw * 0.06f, sh * 0.38f, 0.0f, (ColorRGBA){255, 255, 255, 255});
    } else {
        SituationFont font = adv_gpu_font(fonts);
        adv_text_ex(cmd, font, text, sw * 0.06f, sh * 0.38f, adv_gpu_size(fonts, 22.0f, ADV_VGA_MAX_BODY), 1.0f,
                    SituationColorFromYPQ(g_adv_ypq_stops[5]));
    }

    adv_text_ex(cmd, sit_text_test_default_font(), "CPU stamp (offset shadow)", sw * 0.06f, sh * 0.30f, 14.0f, 0.0f,
                (ColorRGBA){160, 170, 190, 255});
    adv_text_ex(cmd, sit_text_test_default_font(), "GPU retro atlas outline", sw * 0.52f, sh * 0.30f, 14.0f, 0.0f,
                (ColorRGBA){160, 170, 190, 255});

    adv_text_ex(cmd, fonts->vga_outline, "OUTLINE", sw * 0.52f, sh * 0.40f, 16.0f, 1.0f,
                SituationColorFromYPQ(g_adv_ypq_stops[5]));
    adv_text_ex(cmd, fonts->vcr_outline, "OUT", sw * 0.52f, sh * 0.52f, 16.0f, 1.0f,
                SituationColorFromYPQ(g_adv_ypq_stops[5]));
}

static AdvCpuTextTex g_adv_rot_stamp = {0};
static AdvCpuTextTex g_adv_skew_tex = {0};

static void adv_seg_rotation_transform_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t)
{
    if (!g_adv_rot_stamp.valid && fonts->has_roboto_cpu) {
        (void)adv_cpu_text_ex_to_texture(
            fonts,
            &g_adv_rot_stamp,
            "ROTATION",
            24.0f,
            1.0f,
            0.0f,
            0.0f,
            SituationColorFromYPQ(g_adv_ypq_stops[5]),
            (ColorRGBA){0, 0, 0, 0},
            0.0f);
    }

    const float rot_deg = (seg_t / 4.0f) * 360.0f;
    const float rcx = sw * 0.5f;
    const float rcy = sh * 0.36f;
    if (g_adv_rot_stamp.valid) {
        SitRectangle source = {0.0f, 0.0f, (float)g_adv_rot_stamp.w, (float)g_adv_rot_stamp.h};
        SitRectangle dest = {
            rcx - (float)g_adv_rot_stamp.w * 0.5f,
            rcy - (float)g_adv_rot_stamp.h * 0.5f,
            (float)g_adv_rot_stamp.w,
            (float)g_adv_rot_stamp.h};
        Vector2 origin = {{(float)g_adv_rot_stamp.w * 0.5f, (float)g_adv_rot_stamp.h * 0.5f}};
        SituationCmdDrawTexture(
            cmd,
            g_adv_rot_stamp.tex,
            source,
            dest,
            origin,
            rot_deg,
            (ColorRGBA){255, 255, 255, 255});
    } else {
        SituationFont font = adv_gpu_font(fonts);
        adv_text_ex(cmd, font, "ROTATION", rcx - 48.0f, rcy - 8.0f, adv_gpu_size(fonts, 24.0f, ADV_VGA_MAX_BODY), 1.0f,
                    SituationColorFromYPQ(g_adv_ypq_stops[5]));
    }

    const float skew = sinf(t * 2.0f) * 0.3f;
    const int tick = (int)(seg_t * 10.0f);
    if (fonts->has_roboto_cpu && tick != g_adv_skew_tick) {
        g_adv_skew_tick = tick;
        (void)adv_cpu_text_ex_to_texture(
            fonts,
            &g_adv_skew_tex,
            "SKEW",
            22.0f,
            1.0f,
            0.0f,
            skew,
            SituationColorFromYPQ(g_adv_ypq_stops[2]),
            SituationColorFromYPQ(g_adv_ypq_stops[1]),
            1.0f);
    }
    if (fonts->has_roboto_cpu && g_adv_skew_tex.valid) {
        adv_blit_cpu_tex(cmd, &g_adv_skew_tex, sw * 0.38f, sh * 0.58f, 0.0f, (ColorRGBA){255, 255, 255, 255});
    } else if (!fonts->has_roboto_cpu) {
        SituationFont font = adv_gpu_font(fonts);
        adv_text_ex(cmd, font, "SKEW (Roboto required)", sw * 0.32f, sh * 0.58f, 16.0f, 0.0f,
                    SituationColorFromYPQ(g_adv_ypq_stops[7]));
    }
}

static void adv_seg_layout_clip_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t)
{
    (void)t;
    (void)seg_t;
    SituationFont font = adv_gpu_font(fonts);
    const char* para =
        "The quick brown fox jumps over the lazy dog while Situation fonts wrap and clip inside bounded rectangles.";
    const float bx = sw * 0.20f;
    const float by = sh * 0.24f;
    const float bw = sw * 0.60f;
    const float bh = sh * 0.22f;

    SitRectangle bounds = {bx, by, bw, bh};
    adv_text_ex(cmd, sit_text_test_default_font(), "Wrap ON", bx, by - 20.0f, 14.0f, 0.0f,
                (ColorRGBA){170, 180, 200, 255});
    SituationCmdDrawTextBoxed(cmd, font, para, bounds, 16.0f, 0.0f, adv_ypq_sample(0.4f), true);

    const float bx2 = sw * 0.20f;
    const float by2 = sh * 0.56f;
    SitRectangle bounds_clip = {bx2, by2, bw, bh * 0.55f};
    adv_text_ex(cmd, sit_text_test_default_font(), "Wrap OFF (clip)", bx2, by2 - 20.0f, 14.0f, 0.0f,
                (ColorRGBA){170, 180, 200, 255});
    SituationCmdDrawTextBoxed(cmd, font, para, bounds_clip, 16.0f, 0.0f, adv_ypq_sample(0.55f), false);

    SitRectangle measured = SituationMeasureTextEx(font, para, 16.0f, 0.0f);
    if (measured.width < 1.0f) {
        measured.width = 1.0f;
    }
    if (measured.height < 1.0f) {
        measured.height = 1.0f;
    }
    mat4 mq;
    glm_mat4_identity(mq);
    glm_translate(mq, (vec3){bx, by, 0.0f});
    glm_scale(mq, (vec3){measured.width, measured.height, 1.0f});
    SituationCmdDrawQuad(cmd, mq, (Vector4){{1.0f, 1.0f, 1.0f, 0.12f}});

    const int lines = SituationGetTextLineCount(font, para, bw);
    char line_buf[32];
    snprintf(line_buf, sizeof(line_buf), "Lines: %d", lines);
    adv_text_ex(cmd, sit_text_test_default_font(), line_buf, bx2, by2 + bh * 0.55f + 12.0f, 14.0f, 0.0f,
                SituationColorFromYPQ(g_adv_ypq_stops[7]));
}

static void adv_seg_composite_finale_draw(
    SituationCommandBuffer cmd,
    const AdvFontBundle* fonts,
    float sw,
    float sh,
    float t,
    float seg_t)
{
    (void)seg_t;
    SituationFont font = adv_gpu_font(fonts);
    const char* hero = "SITUATION";
    const float hero_size = adv_gpu_size(fonts, 42.0f, ADV_VGA_MAX_HERO);
    SitRectangle hm = SituationMeasureTextEx(font, hero, hero_size, 2.0f);
    const float hx = sw * 0.5f - hm.width * 0.5f;
    const float hy = sh * 0.30f;
    adv_draw_ypq_banded_text(cmd, font, hero, hx, hy, hero_size, 2.0f, t * 0.10f, sw, sh);

    const float sub_x = sw * 0.5f + cosf(t * 2.0f) * sw * 0.22f;
    const float sub_y = sh * 0.58f + sinf(t * 2.0f) * sh * 0.06f;
    adv_text_ex(cmd, font, "font · color · motion · outline", sub_x - 120.0f, sub_y, 16.0f, 1.0f,
                adv_ypq_modulate(g_adv_ypq_stops[5], t, 0.25f, 0.3f, 0.2f));

    adv_text_ex(cmd, fonts->vga_outline, "VGA", sw * 0.06f, sh * 0.78f, 18.0f, 1.0f, adv_ypq_sample(0.7f));
    adv_text_ex(cmd, sit_text_test_default_font(), SituationGetGraphicsBackendName(), sw * 0.06f, sh * 0.84f,
                14.0f, 0.0f, SituationColorFromYPQ(g_adv_ypq_stops[7]));
}

static const AdvFontSegment g_adv_font_segments[ADV_SEG_COUNT] = {
    {"Typography Baseline", 3.0, adv_seg_typography_baseline_draw},
    {"Retro Atlas Gallery", 4.0, adv_seg_retro_gallery_draw},
    {"YPQ Color Field", 5.0, adv_seg_ypq_color_field_draw},
    {"Motion & Timing", 3.0, adv_seg_motion_timing_draw},
    {"Outline & Depth", 3.0, adv_seg_outline_depth_draw},
    {"Rotation & Transform", 4.0, adv_seg_rotation_transform_draw},
    {"Layout & Clip", 3.0, adv_seg_layout_clip_draw},
    {"Composite Finale", 5.0, adv_seg_composite_finale_draw},
};

static double adv_segment_total_duration(void) {
    double total = 0.0;
    for (int i = 0; i < ADV_SEG_COUNT; i++) {
        total += g_adv_font_segments[i].duration_sec;
    }
    return total;
}

static void adv_font_showcase_enter_fullscreen(void) {
    if (!SituationIsWindowFullscreen()) {
        SituationToggleFullscreen();
        SituationPollInputEvents();
    }
}

static void adv_font_showcase_exit_fullscreen(void) {
    if (SituationIsWindowFullscreen()) {
        SituationToggleFullscreen();
        SituationPollInputEvents();
    }
}

static int adv_font_showcase_seg_index(double elapsed, double* out_seg_start) {
    double acc = 0.0;
    for (int i = 0; i < ADV_SEG_COUNT; i++) {
        const double dur = g_adv_font_segments[i].duration_sec;
        if (elapsed < acc + dur) {
            if (out_seg_start) {
                *out_seg_start = acc;
            }
            return i;
        }
        acc += dur;
    }
    if (out_seg_start) {
        *out_seg_start = acc - g_adv_font_segments[ADV_SEG_COUNT - 1].duration_sec;
    }
    return ADV_SEG_COUNT - 1;
}

static bool adv_font_showcase_draw_frame(
    const AdvFontBundle* fonts,
    int seg_idx,
    float t,
    float seg_t)
{
    adv_ypq_init_once();

    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        return false;
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        return false;
    }

    const float sw = (float)SituationGetRenderWidth();
    const float sh = (float)SituationGetRenderHeight();

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = adv_ypq_clear_color();
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    if (SituationCmdBeginRenderPass(cmd, &rp) != SITUATION_SUCCESS) {
        return false;
    }

    adv_scissor_reset(cmd, sw, sh);

    if (seg_idx >= 0 && seg_idx < ADV_SEG_COUNT && g_adv_font_segments[seg_idx].draw) {
        g_adv_font_segments[seg_idx].draw(cmd, fonts, sw, sh, t, seg_t);
    }

    adv_chrome_draw(cmd, fonts, g_adv_font_segments[seg_idx].title, seg_idx, ADV_SEG_COUNT, sw, sh);

    if (SituationCmdEndRenderPass(cmd) != SITUATION_SUCCESS) {
        return false;
    }
    return SituationEndFrame() == SITUATION_SUCCESS;
}

void test_font_capabilities_fullscreen_showcase(void) {
    g_adv_skew_tick = -1;
    adv_cpu_tex_destroy(&g_adv_outline_cpu_tex);
    adv_cpu_tex_destroy(&g_adv_rot_stamp);
    adv_cpu_tex_destroy(&g_adv_skew_tex);

    adv_ypq_init_once();
    SituationSetVSync(false);
    SituationSetTargetFPS(0);
    adv_font_showcase_enter_fullscreen();
    SituationPollInputEvents();

    AdvFontBundle* fonts = &g_adv_fonts;
    adv_font_load_all(fonts);

    const double total = adv_segment_total_duration();
    const double start = SituationTimerGetTime();
    int ok_frames = 0;

    while (SituationTimerGetTime() - start < total) {
        if (sit_test_hud_poll()) {
            break;
        }

        const double now = SituationTimerGetTime();
        const double elapsed = now - start;
        double seg_start = 0.0;
        const int seg_idx = adv_font_showcase_seg_index(elapsed, &seg_start);
        const float t = (float)elapsed;
        const float seg_t = (float)(elapsed - seg_start);

        if (adv_font_showcase_draw_frame(fonts, seg_idx, t, seg_t)) {
            ok_frames++;
        }
    }

    adv_cpu_tex_destroy(&g_adv_outline_cpu_tex);
    adv_cpu_tex_destroy(&g_adv_rot_stamp);
    adv_cpu_tex_destroy(&g_adv_skew_tex);
    adv_font_unload_all(fonts);
    adv_font_showcase_exit_fullscreen();

    int elapsed_sec = (int)(SituationTimerGetTime() - start);
    if (elapsed_sec < 1) {
        elapsed_sec = 1;
    }
    SIT_ASSERT(ok_frames > elapsed_sec);
    fprintf(stderr, "  [advanced] font_capabilities_fullscreen_showcase: %d frames in %d s\n",
            ok_frames, elapsed_sec);
}
