/**
 * @file test_misc.c
 * @brief Miscellaneous module tests — Image CPU ops, Fonts, Color conversions
 *
 * Some tests (image CPU ops, color) are context-free.
 * Font tests that involve GPU atlas baking require SituationInit().
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include <math.h>

// ============================================================================
//  Cleanup helper
// ============================================================================

static void misc_teardown(void) {
    // Remove any leftover test artifacts
    SituationDeleteFile("_sit_test_export.png");
    SituationDeleteFile("_sit_test_export.bmp");
}

// ============================================================================
//  Image CPU Operations
// ============================================================================

static void test_create_image(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(img));
    SIT_ASSERT_EQ(img.width, 4);
    SIT_ASSERT_EQ(img.height, 4);
    SIT_ASSERT_EQ(img.channels, 4);
    SIT_ASSERT_NOT_NULL(img.data);
    SituationUnloadImage(img);
}

static void test_set_pixel_color(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    ColorRGBA red = {255, 0, 0, 255};
    SituationSetPixelColor(&img, 1, 2, red);

    // Verify pixel directly from raw data (RGBA, row-major)
    unsigned char* pixels = (unsigned char*)img.data;
    int offset = (2 * 4 + 1) * 4; // (y * width + x) * channels
    SIT_ASSERT_EQ(pixels[offset + 0], 255); // R
    SIT_ASSERT_EQ(pixels[offset + 1], 0);   // G
    SIT_ASSERT_EQ(pixels[offset + 2], 0);   // B
    SIT_ASSERT_EQ(pixels[offset + 3], 255); // A

    SituationUnloadImage(img);
}

static void test_gen_image_color(void) {
    SituationImage img = {0};
    ColorRGBA blue = {0, 0, 255, 255};
    SituationError err = SituationGenImageColor(8, 8, blue, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(img));
    SIT_ASSERT_EQ(img.width, 8);
    SIT_ASSERT_EQ(img.height, 8);

    // Verify a few pixels are blue
    unsigned char* pixels = (unsigned char*)img.data;
    int channels = img.channels;
    // Check pixel (0,0)
    SIT_ASSERT_EQ(pixels[0], 0);
    SIT_ASSERT_EQ(pixels[1], 0);
    SIT_ASSERT_EQ(pixels[2], 255);
    if (channels >= 4) SIT_ASSERT_EQ(pixels[3], 255);
    // Check pixel (7,7)
    int offset = (7 * 8 + 7) * channels;
    SIT_ASSERT_EQ(pixels[offset + 0], 0);
    SIT_ASSERT_EQ(pixels[offset + 1], 0);
    SIT_ASSERT_EQ(pixels[offset + 2], 255);

    SituationUnloadImage(img);
}

static void test_image_copy(void) {
    SituationImage src = {0};
    ColorRGBA green = {0, 255, 0, 255};
    SituationError err = SituationGenImageColor(4, 4, green, &src);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SituationImage dst = {0};
    err = SituationImageCopy(src, &dst);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(dst));
    SIT_ASSERT_EQ(dst.width, src.width);
    SIT_ASSERT_EQ(dst.height, src.height);
    SIT_ASSERT_EQ(dst.channels, src.channels);

    // Verify data matches
    int size = src.width * src.height * src.channels;
    SIT_ASSERT_MEM_EQ(src.data, dst.data, size);

    // Verify they are independent copies (different pointers)
    SIT_ASSERT(src.data != dst.data);

    SituationUnloadImage(src);
    SituationUnloadImage(dst);
}

static void test_image_crop(void) {
    SituationImage img = {0};
    ColorRGBA white = {255, 255, 255, 255};
    SituationError err = SituationGenImageColor(8, 8, white, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SitRectangle crop = {1.0f, 1.0f, 4.0f, 4.0f};
    SituationImageCrop(&img, crop);

    SIT_ASSERT_EQ(img.width, 4);
    SIT_ASSERT_EQ(img.height, 4);
    SIT_ASSERT(SituationIsImageValid(img));

    SituationUnloadImage(img);
}

static void test_image_resize(void) {
    SituationImage img = {0};
    ColorRGBA cyan = {0, 255, 255, 255};
    SituationError err = SituationGenImageColor(8, 8, cyan, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SituationImageResize(&img, 16, 16);

    // Resize may be a no-op if stb_image_resize is not compiled in.
    // Accept either the resized result or the original dimensions.
    SIT_ASSERT(img.width == 16 || img.width == 8);
    if (img.width == 16) {
        SIT_ASSERT_EQ(img.height, 16);
    }
    SIT_ASSERT(SituationIsImageValid(img));

    SituationUnloadImage(img);
}

static void test_image_flip_vertical(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Set top-left pixel to red
    ColorRGBA red = {255, 0, 0, 255};
    ColorRGBA black = {0, 0, 0, 255};
    // Fill with black first
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            SituationSetPixelColor(&img, x, y, black);
    SituationSetPixelColor(&img, 0, 0, red);

    SituationImageFlip(&img, SIT_FLIP_VERTICAL);

    // After vertical flip, top-left should now be at bottom-left (0, 3)
    unsigned char* pixels = (unsigned char*)img.data;
    int offset_bottom_left = (3 * 4 + 0) * 4;
    SIT_ASSERT_EQ(pixels[offset_bottom_left + 0], 255); // R
    SIT_ASSERT_EQ(pixels[offset_bottom_left + 1], 0);   // G
    SIT_ASSERT_EQ(pixels[offset_bottom_left + 2], 0);   // B

    SituationUnloadImage(img);
}

static void test_image_flip_horizontal(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Fill with black, set top-left to red
    ColorRGBA red = {255, 0, 0, 255};
    ColorRGBA black = {0, 0, 0, 255};
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            SituationSetPixelColor(&img, x, y, black);
    SituationSetPixelColor(&img, 0, 0, red);

    SituationImageFlip(&img, SIT_FLIP_HORIZONTAL);

    // After horizontal flip, (0,0) should now be at (3,0)
    unsigned char* pixels = (unsigned char*)img.data;
    int offset_top_right = (0 * 4 + 3) * 4;
    SIT_ASSERT_EQ(pixels[offset_top_right + 0], 255); // R
    SIT_ASSERT_EQ(pixels[offset_top_right + 1], 0);   // G
    SIT_ASSERT_EQ(pixels[offset_top_right + 2], 0);   // B

    SituationUnloadImage(img);
}

static void test_image_export_and_load(void) {
    // Create a 4x4 red image
    SituationImage img = {0};
    ColorRGBA red = {255, 0, 0, 255};
    SituationError err = SituationGenImageColor(4, 4, red, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Export to PNG
    err = SituationExportImage(img, "_sit_test_export.png");
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Load it back
    SituationImage loaded = {0};
    err = SituationLoadImage("_sit_test_export.png", &loaded);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(loaded));
    SIT_ASSERT_EQ(loaded.width, 4);
    SIT_ASSERT_EQ(loaded.height, 4);

    // Verify pixel data matches (red)
    unsigned char* pixels = (unsigned char*)loaded.data;
    SIT_ASSERT_EQ(pixels[0], 255); // R
    SIT_ASSERT_EQ(pixels[1], 0);   // G
    SIT_ASSERT_EQ(pixels[2], 0);   // B

    SituationUnloadImage(img);
    SituationUnloadImage(loaded);
    SituationDeleteFile("_sit_test_export.png");
}

static void test_image_load_from_memory(void) {
    // Create and export an image first to get valid file data
    SituationImage img = {0};
    ColorRGBA magenta = {255, 0, 255, 255};
    SituationError err = SituationGenImageColor(2, 2, magenta, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    err = SituationExportImage(img, "_sit_test_export.bmp");
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SituationUnloadImage(img);

    // Load the file data into memory
    unsigned int dataSize = 0;
    unsigned char* fileData = NULL;
    err = SituationLoadFileData("_sit_test_export.bmp", &dataSize, &fileData);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(fileData);
    SIT_ASSERT(dataSize > 0);

    // Load image from memory
    SituationImage loaded = {0};
    err = SituationLoadImageFromMemory(".bmp", fileData, dataSize, &loaded);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(SituationIsImageValid(loaded));
    SIT_ASSERT_EQ(loaded.width, 2);
    SIT_ASSERT_EQ(loaded.height, 2);

    SituationUnloadImage(loaded);
    free(fileData);
    SituationDeleteFile("_sit_test_export.bmp");
}

static void test_image_invalid_check(void) {
    SituationImage invalid = {0};
    SIT_ASSERT(!SituationIsImageValid(invalid));
}

// ============================================================================
//  Font Tests
// ============================================================================

static void test_load_bitmap_font_from_memory(void) {
    // Create a minimal 8x8 bitmap font (256 chars, each 8x8 = 1 byte per row)
    // Just a block of data — we only need to verify the API doesn't crash
    unsigned char bitmap_data[256 * 8]; // 256 chars, 8 bytes each (8x8 1-bit)
    memset(bitmap_data, 0xAA, sizeof(bitmap_data)); // Striped pattern

    SituationFont font = {0};
    SituationError err = SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT(font.is_bitmap);
    SIT_ASSERT_EQ(font.bitmap_width, 8);
    SIT_ASSERT_EQ(font.bitmap_height, 8);
    SIT_ASSERT_EQ(font.bitmap_count, 256);

    SituationUnloadFont(font);
}

static void test_measure_text_bitmap_font(void) {
    // Load a bitmap font and measure text
    unsigned char bitmap_data[256 * 8];
    memset(bitmap_data, 0xFF, sizeof(bitmap_data));

    SituationFont font = {0};
    SituationError err = SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, &font);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SitRectangle bounds = SituationMeasureText(font, "Hello", 8.0f);
    // For a bitmap font with 8px wide chars, "Hello" (5 chars) should be ~40px wide
    SIT_ASSERT(bounds.width > 0.0f);
    SIT_ASSERT(bounds.height > 0.0f);

    // Empty string should have zero or near-zero width
    SitRectangle empty_bounds = SituationMeasureText(font, "", 8.0f);
    SIT_ASSERT(empty_bounds.width <= 0.0f);

    SituationUnloadFont(font);
}

// ============================================================================
//  Color Conversion Tests
// ============================================================================

static void test_rgb_to_hsv_roundtrip(void) {
    // Test with a known color: pure red
    ColorRGBA red = {255, 0, 0, 255};
    ColorHSV hsv = SituationRgbToHsv(red);

    // Red should be H≈0, S≈1, V≈1
    SIT_ASSERT(hsv.h >= 0.0f && hsv.h <= 1.0f); // Could be 0 or 360 normalized
    SIT_ASSERT(hsv.s >= 0.9f);
    SIT_ASSERT(hsv.v >= 0.9f);

    // Convert back
    ColorRGBA back = SituationHsvToRgb(hsv);
    SIT_ASSERT(abs((int)back.r - (int)red.r) <= 1);
    SIT_ASSERT(abs((int)back.g - (int)red.g) <= 1);
    SIT_ASSERT(abs((int)back.b - (int)red.b) <= 1);
}

static void test_rgb_to_hsv_green(void) {
    ColorRGBA green = {0, 255, 0, 255};
    ColorHSV hsv = SituationRgbToHsv(green);

    // Green should have H≈120 (or normalized equivalent), S≈1, V≈1
    SIT_ASSERT(hsv.s >= 0.9f);
    SIT_ASSERT(hsv.v >= 0.9f);

    ColorRGBA back = SituationHsvToRgb(hsv);
    SIT_ASSERT(abs((int)back.r - (int)green.r) <= 1);
    SIT_ASSERT(abs((int)back.g - (int)green.g) <= 1);
    SIT_ASSERT(abs((int)back.b - (int)green.b) <= 1);
}

static void test_rgb_to_hsv_gray(void) {
    // Gray has no saturation
    ColorRGBA gray = {128, 128, 128, 255};
    ColorHSV hsv = SituationRgbToHsv(gray);
    SIT_ASSERT(hsv.s < 0.01f); // Saturation should be ~0 for gray

    ColorRGBA back = SituationHsvToRgb(hsv);
    SIT_ASSERT(abs((int)back.r - 128) <= 1);
    SIT_ASSERT(abs((int)back.g - 128) <= 1);
    SIT_ASSERT(abs((int)back.b - 128) <= 1);
}

static void test_color_to_ypq_roundtrip(void) {
    ColorRGBA original = {200, 100, 50, 255};
    ColorYPQA ypq = SituationColorToYPQ(original);

    // Y (luminance) should be non-zero for a non-black color
    SIT_ASSERT(ypq.y > 0);
    SIT_ASSERT_EQ(ypq.a, 255);

    // Convert back
    ColorRGBA back = SituationColorFromYPQ(ypq);
    // Allow ±2 tolerance for quantization
    SIT_ASSERT(abs((int)back.r - (int)original.r) <= 2);
    SIT_ASSERT(abs((int)back.g - (int)original.g) <= 2);
    SIT_ASSERT(abs((int)back.b - (int)original.b) <= 2);
    SIT_ASSERT_EQ(back.a, original.a);
}

static void test_color_to_vector4(void) {
    ColorRGBA white = {255, 255, 255, 255};
    Vector4 v = {0};
    SituationConvertColorToVector4(white, &v);

    // Normalized: 255/255 = 1.0
    SIT_ASSERT(fabsf(v.r - 1.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.g - 1.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.b - 1.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.a - 1.0f) < 0.01f);
}

static void test_color_to_vector4_half(void) {
    ColorRGBA half = {128, 128, 128, 128};
    Vector4 v = {0};
    SituationConvertColorToVector4(half, &v);

    // 128/255 ≈ 0.502
    SIT_ASSERT(fabsf(v.r - 128.0f/255.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.g - 128.0f/255.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.b - 128.0f/255.0f) < 0.01f);
    SIT_ASSERT(fabsf(v.a - 128.0f/255.0f) < 0.01f);
}

static void test_color_to_vector4_black(void) {
    ColorRGBA black = {0, 0, 0, 0};
    Vector4 v = {0};
    SituationConvertColorToVector4(black, &v);

    SIT_ASSERT(fabsf(v.r) < 0.01f);
    SIT_ASSERT(fabsf(v.g) < 0.01f);
    SIT_ASSERT(fabsf(v.b) < 0.01f);
    SIT_ASSERT(fabsf(v.a) < 0.01f);
}

// ============================================================================
//  Module Descriptor
// ============================================================================

static SitTestCase misc_tests[] = {
    // Image CPU operations
    {"create_image",                test_create_image,                false},
    {"set_pixel_color",             test_set_pixel_color,             false},
    {"gen_image_color",             test_gen_image_color,             false},
    {"image_copy",                  test_image_copy,                  false},
    {"image_crop",                  test_image_crop,                  false},
    {"image_resize",                test_image_resize,                false},
    {"image_flip_vertical",         test_image_flip_vertical,         false},
    {"image_flip_horizontal",       test_image_flip_horizontal,       false},
    {"image_export_and_load",       test_image_export_and_load,       false},
    {"image_load_from_memory",      test_image_load_from_memory,      false},
    {"image_invalid_check",         test_image_invalid_check,         false},
    // Fonts
    {"load_bitmap_font_memory",     test_load_bitmap_font_from_memory, false},
    {"measure_text_bitmap_font",    test_measure_text_bitmap_font,    false},
    // Color conversions
    {"rgb_to_hsv_roundtrip",        test_rgb_to_hsv_roundtrip,        false},
    {"rgb_to_hsv_green",            test_rgb_to_hsv_green,            false},
    {"rgb_to_hsv_gray",             test_rgb_to_hsv_gray,             false},
    {"color_to_ypq_roundtrip",      test_color_to_ypq_roundtrip,     false},
    {"color_to_vector4",            test_color_to_vector4,            false},
    {"color_to_vector4_half",       test_color_to_vector4_half,       false},
    {"color_to_vector4_black",      test_color_to_vector4_black,      false},
};

const SitTestModule g_module_misc = {
    .name = "misc",
    .setup = NULL,
    .teardown = misc_teardown,
    .tests = misc_tests,
    .test_count = sizeof(misc_tests) / sizeof(misc_tests[0]),
    .requires_context = false
};
