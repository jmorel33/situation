/**
 * @file test_text_retro_builders.c
 * @brief Retro font builder certification — staged usable → display surface tests.
 *
 * Covers CP437, terminal, ASCII, packed, outlined packed, VCR, and VGA builders.
 * Each family: (1) atlas/metadata/measure usability, (2) GPU display capabilities.
 *
 * See doc/plan/TEST_HARNESS_TEXT_FONT_PLAN.md phase T6 and sit_test_retro_font_helpers.h.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_text_helpers.h"
#include "sit_test_retro_font_helpers.h"
#include "sit_test_window.h"
#include <string.h>

static bool g_retro_init_ok = false;

static void retro_builders_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_RETRO_BUILDERS");

    SituationError err = SituationInit(0, NULL, &config);
    g_retro_init_ok = (err == SITUATION_SUCCESS);
    if (!g_retro_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    graphics_test_print_renderer_banner();
}

static void retro_builders_teardown(void) {
    if (g_retro_init_ok) {
        SituationShutdown();
        g_retro_init_ok = false;
    }
}

/* ---- CP437 ---- */

static void test_retro_cp437_usable(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_cp437_font(&font), SITUATION_SUCCESS);
    sit_text_test_assert_grid_font_usable(&font, 8, 16, 0);

    SitRectangle bounds = SituationMeasureTextEx(font, "\xDB\xDB", 16.0f, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    SIT_ASSERT(bounds.height > 0.0f);

    sit_text_test_destroy_font(&font);
}

static void test_retro_cp437_display(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_cp437_font(&font), SITUATION_SUCCESS);
    sit_text_test_retro_display_surface(
        font, "\xDB\xDB", "\xDB\n\xDB", "\xDB \xDB \xDB", 16.0f, 16, 20);
    sit_text_test_destroy_font(&font);
}

/* ---- Terminal ---- */

static void test_retro_terminal_usable(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_terminal_font(&font), SITUATION_SUCCESS);
    sit_text_test_assert_grid_font_usable(&font, 8, 8, 32);

    SitRectangle bounds = SituationMeasureTextEx(font, "ABC", 16.0f, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    sit_text_test_destroy_font(&font);

    const int char_count = 96;
    const int chars_per_row = 16;
    const int char_w = 8;
    const int char_h = 8;
    unsigned char grid[16 * 8 * 6 * 8];
    sit_text_test_build_terminal_ascii_grid(grid, sizeof(grid), chars_per_row, char_w, char_h, 32);

    SituationFont font_tight = {0};
    SituationFont font_loose = {0};
    SIT_ASSERT_EQ(
        SituationCreateTerminalFontEx(
            grid, char_w, char_h, char_count, chars_per_row, 32, 0.0f, 0.0f, &font_tight),
        SITUATION_SUCCESS);
    SIT_ASSERT_EQ(
        SituationCreateTerminalFontEx(
            grid, char_w, char_h, char_count, chars_per_row, 32, 12.0f, 0.0f, &font_loose),
        SITUATION_SUCCESS);

    SitRectangle tight = SituationMeasureTextEx(font_tight, "ABC", 16.0f, 0.0f);
    SitRectangle loose = SituationMeasureTextEx(font_loose, "ABC", 16.0f, 0.0f);
    SIT_ASSERT(loose.width > tight.width + 16.0f);

    sit_text_test_destroy_font(&font_tight);
    sit_text_test_destroy_font(&font_loose);
}

static void test_retro_terminal_display(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_terminal_font(&font), SITUATION_SUCCESS);
    sit_text_test_retro_display_surface(
        font, "ABC", "AB\nCD", "AAAA BBBB", 16.0f, 14, 18);
    sit_text_test_destroy_font(&font);
}

/* ---- ASCII ---- */

static void test_retro_ascii_usable(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_ascii_font(&font), SITUATION_SUCCESS);
    sit_text_test_assert_grid_font_usable(&font, 8, 12, 32);

    SitRectangle bounds = SituationMeasureTextEx(font, "ABC", 16.0f, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    sit_text_test_destroy_font(&font);
}

static void test_retro_ascii_display(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_ascii_font(&font), SITUATION_SUCCESS);
    sit_text_test_retro_display_surface(
        font, "ABC", "AB\nCD", "AAAA BBBB", 16.0f, 14, 18);
    sit_text_test_destroy_font(&font);
}

/* ---- Packed bitmap ---- */

static void test_retro_packed_usable(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_packed_font(&font), SITUATION_SUCCESS);
    sit_text_test_assert_grid_font_usable(&font, 10, 10, 65);

    SitRectangle bounds = SituationMeasureTextEx(font, "ABC", 8.0f, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    sit_text_test_destroy_font(&font);
}

static void test_retro_packed_display(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_packed_font(&font), SITUATION_SUCCESS);
    sit_text_test_retro_display_surface(
        font, "ABC", "AB\nCD", "ABC ABC ABC", 8.0f, 18, 22);
    sit_text_test_destroy_font(&font);
}

/* ---- Outlined packed ---- */

static void test_retro_outlined_packed_usable(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_outlined_packed_font(&font), SITUATION_SUCCESS);
    sit_text_test_assert_grid_font_usable(&font, 10, 10, 65);

    SitRectangle bounds = SituationMeasureTextEx(font, "ABC", 8.0f, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    sit_text_test_destroy_font(&font);
}

static void test_retro_outlined_packed_display(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_outlined_packed_font(&font), SITUATION_SUCCESS);
    sit_text_test_retro_display_surface(
        font, "ABC", "AB\nCD", "ABC ABC ABC", 8.0f, 18, 22);
    sit_text_test_destroy_font(&font);
}

/* ---- VCR ---- */

static void test_retro_vcr_usable(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_vcr_font(&font), SITUATION_SUCCESS);
    sit_text_test_assert_grid_font_usable(&font, 12, 16, 0);

    SitRectangle bounds = SituationMeasureTextEx(font, "HI", 16.0f, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    sit_text_test_destroy_font(&font);
}

static void test_retro_vcr_display(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_vcr_font(&font), SITUATION_SUCCESS);
    sit_text_test_retro_display_surface(
        font, "HI", "H\nI", "HI HI HI", 16.0f, 16, 20);
    sit_text_test_destroy_font(&font);
}

/* ---- VGA 8×8 ---- */

static void test_retro_vga_usable(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_vga_font(&font), SITUATION_SUCCESS);
    sit_text_test_assert_grid_font_usable(&font, 10, 10, 0);

    SitRectangle bounds = SituationMeasureTextEx(font, "ABC", 8.0f, 0.0f);
    SIT_ASSERT(bounds.width > 0.0f);
    sit_text_test_destroy_font(&font);
}

static void test_retro_vga_display(void) {
    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_vga_font(&font), SITUATION_SUCCESS);
    sit_text_test_retro_display_surface(
        font, "ABC", "AB\nCD", "ABC ABC ABC", 8.0f, 18, 22);
    sit_text_test_destroy_font(&font);
}

static SitTestCase retro_builder_tests[] = {
    /* Usable (atlas + metadata + measure) before display surface */
    {"retro_cp437_usable",             test_retro_cp437_usable,             true},
    {"retro_cp437_display",            test_retro_cp437_display,            true},
    {"retro_terminal_usable",          test_retro_terminal_usable,          true},
    {"retro_terminal_display",         test_retro_terminal_display,         true},
    {"retro_ascii_usable",             test_retro_ascii_usable,             true},
    {"retro_ascii_display",            test_retro_ascii_display,            true},
    {"retro_packed_usable",            test_retro_packed_usable,            true},
    {"retro_packed_display",           test_retro_packed_display,           true},
    {"retro_outlined_packed_usable",   test_retro_outlined_packed_usable,   true},
    {"retro_outlined_packed_display",  test_retro_outlined_packed_display,  true},
    {"retro_vcr_usable",               test_retro_vcr_usable,               true},
    {"retro_vcr_display",              test_retro_vcr_display,              true},
    {"retro_vga_usable",               test_retro_vga_usable,               true},
    {"retro_vga_display",              test_retro_vga_display,              true},
};

const SitTestModule g_module_text_retro_builders = {
    .name = "text_retro_builders",
    .setup = retro_builders_setup,
    .teardown = retro_builders_teardown,
    .tests = retro_builder_tests,
    .test_count = sizeof(retro_builder_tests) / sizeof(retro_builder_tests[0]),
    .requires_context = true
};
