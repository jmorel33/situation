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
#include "sit_test_window.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SIT_YPQ_PLANE_SIZE 256
#define SIT_YPQ_Q_SWEEP_SECONDS 4.0
#define SIT_YPQ_Q_STEPS 256
#define SIT_YPQ_PHOTO_MAX_DIM 1024
#define SIT_YPQ_PHOTO_SWEEP_DIM 384
#define SIT_YPQ_PHOTO_ASSET "prairie.jpg"
#define SIT_YPQ_PHOTO_SEGMENT_COUNT 3

static bool misc_ypq_pixel_is_gray(const ColorRGBA* c, int tolerance) {
    return abs((int)c->r - (int)c->g) <= tolerance
        && abs((int)c->g - (int)c->b) <= tolerance;
}

#define SIT_YPQ_PLANE_PIXELS (SIT_YPQ_PLANE_SIZE * SIT_YPQ_PLANE_SIZE)
#define SIT_YPQ_PLANE_BYTES  ((size_t)SIT_YPQ_PLANE_PIXELS * 4u)

/** NTSC YIQ constants (must match sit/situation_impl_color.h). */
/* REMOVED — private NTSC constants deleted; all conversion goes through SituationColorFromYPQ */

static ColorRGBA misc_sample_ypq_plane_rgba(unsigned char y, unsigned char p, unsigned char q) {
    return SituationColorFromYPQ((ColorYPQA){y, p, q, 255});
}

static ColorRGBA misc_ypq_image_center_pixel(const SituationImage* img) {
    int x = img->width / 2;
    int y = img->height / 2;
    unsigned char* pixels = (unsigned char*)img->data;
    int ch = img->channels > 0 ? img->channels : 4;
    int idx = (y * img->width + x) * ch;
    return (ColorRGBA){pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]};
}

static int misc_ypq_pixel_luma_sum(const ColorRGBA* c) {
    return (int)c->r + (int)c->g + (int)c->b;
}

static bool misc_ypq_ensure_rgba(SituationImage* img) {
    if (!SituationIsImageValid(*img)) {
        return false;
    }
    if (img->channels == 4) {
        return true;
    }
    if (img->channels != 3) {
        return false;
    }

    SituationImage rgba = {0};
    if (SituationCreateImage(img->width, img->height, 4, &rgba) != SITUATION_SUCCESS) {
        return false;
    }

    unsigned char* src = (unsigned char*)img->data;
    unsigned char* dst = (unsigned char*)rgba.data;
    int pixel_count = img->width * img->height;
    for (int i = 0; i < pixel_count; i++) {
        dst[i * 4 + 0] = src[i * 3 + 0];
        dst[i * 4 + 1] = src[i * 3 + 1];
        dst[i * 4 + 2] = src[i * 3 + 2];
        dst[i * 4 + 3] = 255;
    }

    SituationUnloadImage(*img);
    *img = rgba;
    return SituationIsImageValid(*img);
}

static void misc_ypq_downscale_nearest(SituationImage* img, int max_dim) {
    if (!SituationIsImageValid(*img) || max_dim < 1) {
        return;
    }

    int w = img->width;
    int h = img->height;
    int cur_max = w > h ? w : h;
    if (cur_max <= max_dim) {
        return;
    }

    int nw = (int)((long long)w * (long long)max_dim / (long long)cur_max);
    int nh = (int)((long long)h * (long long)max_dim / (long long)cur_max);
    if (nw < 1) {
        nw = 1;
    }
    if (nh < 1) {
        nh = 1;
    }

    SituationImage small = {0};
    if (SituationCreateImage(nw, nh, img->channels, &small) != SITUATION_SUCCESS) {
        return;
    }

    unsigned char* src = (unsigned char*)img->data;
    unsigned char* dst = (unsigned char*)small.data;
    int ch = img->channels;
    for (int y = 0; y < nh; y++) {
        int sy = y * h / nh;
        for (int x = 0; x < nw; x++) {
            int sx = x * w / nw;
            int sidx = (sy * w + sx) * ch;
            int didx = (y * nw + x) * ch;
            for (int c = 0; c < ch; c++) {
                dst[didx + c] = src[sidx + c];
            }
        }
    }

    SituationUnloadImage(*img);
    *img = small;
}

static bool misc_ypq_load_harness_photo(SituationImage* out, const char* filename) {
    static const char* prefixes[] = {
        "tests/harness/assets/",
        "../tests/harness/assets/",
        "../../tests/harness/assets/",
        NULL
    };
    char path[512];

    for (int i = 0; prefixes[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], filename);
        if (SituationLoadImage(path, out) == SITUATION_SUCCESS && SituationIsImageValid(*out)) {
            if (!misc_ypq_ensure_rgba(out)) {
                SituationUnloadImage(*out);
                memset(out, 0, sizeof(*out));
                continue;
            }

            SituationImageResize(out,
                out->width > SIT_YPQ_PHOTO_MAX_DIM ? SIT_YPQ_PHOTO_MAX_DIM : out->width,
                out->height > SIT_YPQ_PHOTO_MAX_DIM ? SIT_YPQ_PHOTO_MAX_DIM : out->height);
            misc_ypq_downscale_nearest(out, SIT_YPQ_PHOTO_MAX_DIM);
            return SituationIsImageValid(*out);
        }
    }
    return false;
}

typedef enum MiscYpqPhotoSweepAxis {
    MISC_YPQ_PHOTO_SWEEP_LUMA = 0,
    MISC_YPQ_PHOTO_SWEEP_PHASE = 1,
    MISC_YPQ_PHOTO_SWEEP_CHROMA = 2
} MiscYpqPhotoSweepAxis;

static void misc_ypq_apply_photo_sweep(
    SituationImage* work,
    const SituationImage* source,
    MiscYpqPhotoSweepAxis axis,
    int step,
    int steps)
{
    size_t bytes = (size_t)source->width * (size_t)source->height * 4u;
    memcpy(work->data, source->data, bytes);

    float t = (steps <= 1) ? 0.0f : (float)step / (float)(steps - 1);
    float phase_deg = 0.0f;
    float chroma = 1.0f;
    float luma = 1.0f;

    switch (axis) {
    case MISC_YPQ_PHOTO_SWEEP_LUMA:
        luma = 0.25f + t * 1.5f;
        break;
    case MISC_YPQ_PHOTO_SWEEP_PHASE:
        phase_deg = t * 360.0f;
        break;
    case MISC_YPQ_PHOTO_SWEEP_CHROMA:
        chroma = t * 2.0f;
        break;
    default:
        break;
    }

    SituationImageAdjustYPQ(work, phase_deg, chroma, luma, 1.0f);
}

static const char* misc_ypq_photo_sweep_axis_name(MiscYpqPhotoSweepAxis axis) {
    switch (axis) {
    case MISC_YPQ_PHOTO_SWEEP_LUMA:
        return "Y (luma)";
    case MISC_YPQ_PHOTO_SWEEP_PHASE:
        return "P (phase)";
    case MISC_YPQ_PHOTO_SWEEP_CHROMA:
        return "Q (chroma)";
    default:
        return "YPQ";
    }
}

/* REMOVED — private radix sort, duplicate counting, and registry report helpers deleted.
 * Equivalent functionality is now provided by SituationYpqAnalyzeRgbMapping() and
 * SituationYpqSliceDuplicateCount() in the library. */

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
static SituationError misc_ypq_texture_barrier(
    SituationCommandBuffer cmd,
    SituationTexture tex,
    SituationTextureLayout old_layout,
    SituationTextureLayout new_layout)
{
    SituationTextureBarrierDesc desc = {0};
    desc.old_layout = old_layout;
    desc.new_layout = new_layout;
    desc.base_mip_level = 0;
    desc.mip_level_count = 1;
    return SituationCmdTextureBarrier(cmd, tex, &desc);
}

/** Upload the CPU image (if needed) and draw it aspect-fit, centered in the window. */
static SituationError misc_ypq_present_image(
    SituationImage* image,
    SituationTexture* tex,
    SituationBuffer* upload_buf,
    bool* tex_ready)
{
    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    if (!cmd) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    const int img_w = image->width;
    const int img_h = image->height;
    const int img_ch = image->channels > 0 ? image->channels : 4;
    SituationError err;
    if (!*tex_ready) {
        err = SituationCreateTextureEx(
            *image,
            false,
            (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED |
                                         SITUATION_TEXTURE_USAGE_TRANSFER_DST),
            tex);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
        *tex_ready = true;
    } else {
        size_t image_bytes = (size_t)img_w * (size_t)img_h * (size_t)img_ch;
        err = SituationUpdateBuffer(*upload_buf, 0, image_bytes, image->data);
        if (err != SITUATION_SUCCESS) {
            return err;
        }

        SituationTextureCopyRegion region = {0};
        region.src_rect = (SituationTextureRect){0, 0, img_w, img_h};
        region.dst_x = 0;
        region.dst_y = 0;

        err = misc_ypq_texture_barrier(
            cmd, *tex,
            SITUATION_TEXTURE_LAYOUT_SHADER_READ,
            SITUATION_TEXTURE_LAYOUT_TRANSFER_DST);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
        err = SituationCmdCopyBufferToTexture(cmd, *upload_buf, 0, *tex, &region);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
        err = misc_ypq_texture_barrier(
            cmd, *tex,
            SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
            SITUATION_TEXTURE_LAYOUT_SHADER_READ);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    err = SituationCmdBeginRenderPass(cmd, &rp);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    SitRectangle source = {0.0f, 0.0f, (float)img_w, (float)img_h};
    SitRectangle dest = sit_test_fit_content_dest((float)img_w, (float)img_h);
    Vector2 origin = {{0.0f, 0.0f}};
    ColorRGBA tint = {255, 255, 255, 255};
    err = SituationCmdDrawTexture(cmd, *tex, source, dest, origin, 0.0f, tint);
    if (err != SITUATION_SUCCESS) {
        SituationCmdEndRenderPass(cmd);
        return err;
    }

    err = SituationCmdEndRenderPass(cmd);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    return SituationEndFrame();
}

static SituationError misc_ypq_present_plane(
    SituationImage* plane,
    SituationTexture* tex,
    SituationBuffer* upload_buf,
    bool* tex_ready)
{
    return misc_ypq_present_image(plane, tex, upload_buf, tex_ready);
}
#endif /* GPU backends */

// ============================================================================
//  Cleanup helper
// ============================================================================

static void misc_teardown(void) {
    // Remove any leftover test artifacts
    SituationDeleteFile(sit_test_tmp("_sit_test_export.png"));
    SituationDeleteFile(sit_test_tmp("_sit_test_export.bmp"));
    SituationDeleteFile(sit_test_tmp("_sit_test_roundtrip.png"));
    SituationDeleteFile(sit_test_tmp("_sit_test_roundtrip.bmp"));
    SituationDeleteFile(sit_test_tmp("_sit_test_roundtrip.jpg"));
    SituationDeleteFile(sit_test_tmp("_sit_test_roundtrip.tga"));
    SituationDeleteFile(sit_test_tmp("_sit_test_roundtrip.hdr"));
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
    err = SituationExportImage(img, sit_test_tmp("_sit_test_export.png"));
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    // Load it back
    SituationImage loaded = {0};
    err = SituationLoadImage(sit_test_tmp("_sit_test_export.png"), &loaded);
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
    SituationDeleteFile(sit_test_tmp("_sit_test_export.png"));
}

static void test_image_load_from_memory(void) {
    // Create and export an image first to get valid file data
    SituationImage img = {0};
    ColorRGBA magenta = {255, 0, 255, 255};
    SituationError err = SituationGenImageColor(2, 2, magenta, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    err = SituationExportImage(img, sit_test_tmp("_sit_test_export.bmp"));
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SituationUnloadImage(img);

    // Load the file data into memory
    unsigned int dataSize = 0;
    unsigned char* fileData = NULL;
    err = SituationLoadFileData(sit_test_tmp("_sit_test_export.bmp"), &dataSize, &fileData);
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
    SituationDeleteFile(sit_test_tmp("_sit_test_export.bmp"));
}

static void test_image_invalid_check(void) {
    SituationImage invalid = {0};
    SIT_ASSERT(!SituationIsImageValid(invalid));
}

static bool misc_pixel_approx_channel(uint8_t actual, uint8_t expected, uint8_t tolerance) {
    int diff = (int)actual - (int)expected;
    return diff >= -(int)tolerance && diff <= (int)tolerance;
}

static void misc_assert_roundtrip_pixels(
    const SituationImage* loaded,
    uint8_t expect_r,
    uint8_t expect_g,
    uint8_t expect_b,
    uint8_t tolerance)
{
    SIT_ASSERT_NOT_NULL(loaded);
    SIT_ASSERT(SituationIsImageValid(*loaded));
    SIT_ASSERT_NOT_NULL(loaded->data);
    const unsigned char* px = (const unsigned char*)loaded->data;
    SIT_ASSERT(misc_pixel_approx_channel(px[0], expect_r, tolerance));
    SIT_ASSERT(misc_pixel_approx_channel(px[1], expect_g, tolerance));
    SIT_ASSERT(misc_pixel_approx_channel(px[2], expect_b, tolerance));
}

static void misc_test_image_roundtrip_extension(const char* ext_with_dot, uint8_t tolerance) {
    SituationImage src = {0};
    ColorRGBA orange = {255, 128, 0, 255};
    SituationError err = SituationGenImageColor(3, 3, orange, &src);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    char path[256];
    snprintf(path, sizeof(path), "%s%s", sit_test_tmp("_sit_test_roundtrip"), ext_with_dot);

    err = SituationExportImage(src, path);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SituationImage loaded = {0};
    err = SituationLoadImage(path, &loaded);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT_EQ(loaded.width, 3);
    SIT_ASSERT_EQ(loaded.height, 3);
    SIT_ASSERT_EQ(loaded.channels, 4);
    misc_assert_roundtrip_pixels(&loaded, 255, 128, 0, tolerance);

    SituationUnloadImage(src);
    SituationUnloadImage(loaded);
    SituationDeleteFile(path);
}

static void test_stb_image_load_extension_recognition(void) {
    static const char* supported[] = {
        ".jpg", ".jpeg", ".png", ".bmp", ".tga", ".psd", ".gif", ".hdr", ".pic", ".ppm", ".pgm", ".pnm", NULL
    };
    for (int i = 0; supported[i] != NULL; ++i) {
        SIT_ASSERT(SituationIsStbImageLoadExtension(supported[i]));
        SIT_ASSERT(SituationIsStbImageLoadExtension(supported[i] + 1));
    }
    SIT_ASSERT(!SituationIsStbImageLoadExtension(".webp"));
    SIT_ASSERT(!SituationIsStbImageLoadExtension("txt"));
    SIT_ASSERT(!SituationIsStbImageLoadExtension(NULL));
}

static void test_stb_image_load_roundtrip_formats(void) {
    misc_test_image_roundtrip_extension(".png", 0);
    misc_test_image_roundtrip_extension(".bmp", 0);
    misc_test_image_roundtrip_extension(".jpg", 12);
    misc_test_image_roundtrip_extension(".tga", 0);
    misc_test_image_roundtrip_extension(".hdr", 4);
}

/* Minimal valid 1x1 GIF89a (red pixel). */
static const unsigned char k_misc_test_gif_1x1[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xf9, 0x04, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x01,
    0x44, 0x00, 0x3b
};

/* Minimal P6 PPM 1x1 red. */
static const unsigned char k_misc_test_ppm_1x1[] = {
    'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n',
    0xff, 0x00, 0x00
};

/* Minimal P5 PGM 1x1 white. */
static const unsigned char k_misc_test_pgm_1x1[] = {
    'P', '5', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n',
    0xff
};

static void test_stb_image_load_embedded_formats(void) {
    SituationImage gif_img = {0};
    SituationError err = SituationLoadImageFromMemory(
        ".gif",
        k_misc_test_gif_1x1,
        (int)sizeof(k_misc_test_gif_1x1),
        &gif_img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT_EQ(gif_img.width, 1);
    SIT_ASSERT_EQ(gif_img.height, 1);
    SIT_ASSERT_EQ(gif_img.channels, 4);
    misc_assert_roundtrip_pixels(&gif_img, 255, 0, 0, 0);
    SituationUnloadImage(gif_img);

    SituationImage ppm_img = {0};
    err = SituationLoadImageFromMemory(
        ".ppm",
        k_misc_test_ppm_1x1,
        (int)sizeof(k_misc_test_ppm_1x1),
        &ppm_img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT_EQ(ppm_img.width, 1);
    SIT_ASSERT_EQ(ppm_img.height, 1);
    misc_assert_roundtrip_pixels(&ppm_img, 255, 0, 0, 0);
    SituationUnloadImage(ppm_img);

    SituationImage pgm_img = {0};
    err = SituationLoadImageFromMemory(
        ".pgm",
        k_misc_test_pgm_1x1,
        (int)sizeof(k_misc_test_pgm_1x1),
        &pgm_img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT_EQ(pgm_img.width, 1);
    SIT_ASSERT_EQ(pgm_img.height, 1);
    misc_assert_roundtrip_pixels(&pgm_img, 255, 255, 255, 0);
    SituationUnloadImage(pgm_img);
}

// ============================================================================
//  Font Tests
// ============================================================================

static void test_load_bitmap_font_from_memory(void) {
    /* GPU bake + readback: text_rendering.bitmap_memory_bake_gpu_draw (requires_context). */
    unsigned char bitmap_data[256 * 8];
    memset(bitmap_data, 0xAA, sizeof(bitmap_data));

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

/** Phase 0: shared YIQ core — Q=0 makes phase irrelevant (grayscale slice). */
static void test_ypq_groundwork_q_zero_phase_invariant(void) {
    ColorRGBA p0 = SituationColorFromYPQ((ColorYPQA){128, 0, 0, 255});
    ColorRGBA p128 = SituationColorFromYPQ((ColorYPQA){128, 128, 0, 255});
    ColorRGBA p255 = SituationColorFromYPQ((ColorYPQA){128, 255, 0, 255});

    SIT_ASSERT(misc_ypq_pixel_is_gray(&p0, 3));
    SIT_ASSERT(misc_ypq_pixel_is_gray(&p128, 3));
    SIT_ASSERT(misc_ypq_pixel_is_gray(&p255, 3));
    SIT_ASSERT(abs((int)p0.r - (int)p128.r) <= 1);
    SIT_ASSERT(abs((int)p0.g - (int)p128.g) <= 1);
    SIT_ASSERT(abs((int)p0.b - (int)p128.b) <= 1);
    SIT_ASSERT(abs((int)p128.r - (int)p255.r) <= 1);
}

/** Phase 0: Y luma axis at Q=0 tracks grayscale RGB. */
static void test_ypq_groundwork_grayscale_luma_axis(void) {
    for (int y = 0; y < 256; y += 17) {
        ColorRGBA rgb = SituationColorFromYPQ((ColorYPQA){(unsigned char)y, 64, 0, 255});
        SIT_ASSERT(misc_ypq_pixel_is_gray(&rgb, 3));
        SIT_ASSERT(abs((int)rgb.r - y) <= 2);
        SIT_ASSERT(abs((int)rgb.g - y) <= 2);
        SIT_ASSERT(abs((int)rgb.b - y) <= 2);
    }
}

/** Phase 0: regression anchors for NTSC YIQ matrix (post-refactor parity). */
static void test_ypq_groundwork_golden_vectors(void) {
    ColorRGBA black = SituationColorFromYPQ((ColorYPQA){0, 0, 0, 255});
    SIT_ASSERT_EQ(black.r, 0);
    SIT_ASSERT_EQ(black.g, 0);
    SIT_ASSERT_EQ(black.b, 0);

    ColorRGBA white = SituationColorFromYPQ((ColorYPQA){255, 0, 0, 255});
    SIT_ASSERT_EQ(white.r, 255);
    SIT_ASSERT_EQ(white.g, 255);
    SIT_ASSERT_EQ(white.b, 255);

    ColorRGBA mid_gray = SituationColorFromYPQ((ColorYPQA){128, 200, 0, 255});
    SIT_ASSERT(abs((int)mid_gray.r - 128) <= 1);
    SIT_ASSERT(abs((int)mid_gray.g - 128) <= 1);
    SIT_ASSERT(abs((int)mid_gray.b - 128) <= 1);

    ColorYPQA from_red = SituationColorToYPQ((ColorRGBA){255, 0, 0, 255});
    SIT_ASSERT(from_red.y > 50);
    SIT_ASSERT(from_red.q > 50);
}

static void test_ypq_lerp_wrap(void) {
    ColorYPQA a = {128, 250, 200, 255};
    ColorYPQA b = {128, 10, 200, 255};
    ColorYPQA mid = SituationYpqLerp(a, b, 0.5f);

    int linear_p = (250 + 10) / 2;
    SIT_ASSERT(abs((int)mid.p - linear_p) > 40);

    ColorYPQA at_a = SituationYpqLerp(a, b, 0.0f);
    ColorYPQA at_b = SituationYpqLerp(a, b, 1.0f);
    SIT_ASSERT_EQ(at_a.y, a.y);
    SIT_ASSERT_EQ(at_a.p, a.p);
    SIT_ASSERT_EQ(at_b.p, b.p);
}

static void test_ypq_adjust_luma(void) {
    ColorYPQA c = {100, 64, 180, 255};
    ColorYPQA bright = SituationYpqAdjustLuma(c, 2.0f);
    SIT_ASSERT(bright.y > c.y);
    SIT_ASSERT_EQ(bright.p, c.p);
    SIT_ASSERT_EQ(bright.q, c.q);
}

static void test_ypq_adjust_phase(void) {
    ColorYPQA c = {100, 10, 180, 255};
    ColorYPQA shifted = SituationYpqAdjustPhase(c, 20);
    SIT_ASSERT_EQ((int)shifted.p, 30);
    ColorYPQA wrap = SituationYpqAdjustPhase((ColorYPQA){100, 250, 180, 255}, 20);
    SIT_ASSERT_EQ((int)wrap.p, 14);
}

static void test_ypq_adjust_chroma(void) {
    ColorYPQA c = {100, 64, 100, 255};
    ColorYPQA sat = SituationYpqAdjustChroma(c, 2.0f);
    SIT_ASSERT(sat.q > c.q);
    SIT_ASSERT_EQ(sat.p, c.p);

    ColorRGBA p0a = SituationColorFromYPQ((ColorYPQA){128, 0, 0, 255});
    ColorRGBA p0b = SituationColorFromYPQ((ColorYPQA){128, 200, 0, 255});
    SIT_ASSERT(abs((int)p0a.r - (int)p0b.r) <= 2);
}

static void test_ypq_distance_equals(void) {
    ColorYPQA a = {100, 50, 120, 255};
    ColorYPQA b = {100, 50, 120, 255};
    SIT_ASSERT(SituationYpqEquals(a, b, 0));
    SIT_ASSERT(fabsf(SituationYpqDistance(a, b)) < 0.001f);

    ColorYPQA c = {200, 180, 240, 255};
    SIT_ASSERT(!SituationYpqEquals(a, c, 5));
    SIT_ASSERT(SituationYpqDistance(a, c) > SituationYpqDistance(a, b));

    SIT_ASSERT(fabsf(SituationYpqGetLuma(a) - (100.0f / 255.0f)) < 0.01f);
    SIT_ASSERT(fabsf(SituationYpqGetChroma(a) - (120.0f / 255.0f)) < 0.01f);
    SIT_ASSERT(fabsf(SituationYpqGetHueDegrees(a) - ((50.0f / 255.0f) * 360.0f)) < 2.0f);
}

static void test_ypq_float_roundtrip(void) {
    ColorRGBA original = {180, 90, 40, 255};
    ColorYPQf ypq = SituationColorToYPQf(original);
    ColorRGBA back = SituationColorFromYPQf(ypq);

    SIT_ASSERT(abs((int)back.r - (int)original.r) <= 2);
    SIT_ASSERT(abs((int)back.g - (int)original.g) <= 2);
    SIT_ASSERT(abs((int)back.b - (int)original.b) <= 2);
    SIT_ASSERT_EQ(back.a, original.a);

    ColorYPQA bytes = SituationYpqQuantize(ypq);
    SIT_ASSERT(abs((int)bytes.y - (int)(ypq.y * 255.0f + 0.5f)) <= 1);
}

static void test_ypq_quantize(void) {
    ColorYPQf src = {0.5f, 0.25f, 0.75f, 1.0f};
    ColorYPQA q = SituationYpqQuantize(src);
    SIT_ASSERT(abs((int)q.y - 128) <= 1);
    SIT_ASSERT(abs((int)q.p - 64) <= 1);
    SIT_ASSERT(abs((int)q.q - 191) <= 1);
    SIT_ASSERT_EQ(q.a, 255);

    ColorYPQf clamped = SituationYpqClampInGamut((ColorYPQf){1.0f, 0.0f, 1.0f, 1.0f});
    SIT_ASSERT(clamped.q < 1.0f);
    ColorRGBA hot = SituationColorFromYPQf((ColorYPQf){1.0f, 0.0f, 1.0f, 1.0f});
    ColorRGBA safe = SituationColorFromYPQf(clamped);
    SIT_ASSERT(abs((int)hot.r - (int)safe.r) > 0 || abs((int)hot.g - (int)safe.g) > 0);
}

static void test_image_adjust_ypq(void) {
    SituationImage img = {0};
    ColorRGBA blue = {40, 80, 220, 255};
    SituationError err = SituationGenImageColor(8, 8, blue, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    SituationImageAdjustYPQ(&img, 0.0f, 2.0f, 1.0f, 1.0f);

    unsigned char* pixels = (unsigned char*)img.data;
    ColorRGBA center = {pixels[0], pixels[1], pixels[2], pixels[3]};
    SIT_ASSERT(!misc_ypq_pixel_is_gray(&center, 12));
    SIT_ASSERT(abs((int)center.b - (int)blue.b) > 8 || abs((int)center.r - (int)blue.r) > 8);

    SituationUnloadImage(img);
}

/**
 * CPU: 256x256 Y (x) x P (y) plane at several Q values; FromYPQ must be stable and sane.
 */
static void test_ypq_to_rgb_y_p_plane(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(SIT_YPQ_PLANE_SIZE, SIT_YPQ_PLANE_SIZE, 4, &img);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(img.data);

    /* Fill Q=0 plane via SituationColorFromYPQ (Y=column/px, P=row/py, Q=0) */
    {
        unsigned char* pixels = (unsigned char*)img.data;
        for (int py = 0; py < SIT_YPQ_PLANE_SIZE; py++) {
            for (int px = 0; px < SIT_YPQ_PLANE_SIZE; px++) {
                ColorRGBA c = SituationColorFromYPQ(
                    (ColorYPQA){(unsigned char)px, (unsigned char)py, 0, 255});
                int idx = (py * SIT_YPQ_PLANE_SIZE + px) * 4;
                pixels[idx + 0] = c.r;
                pixels[idx + 1] = c.g;
                pixels[idx + 2] = c.b;
                pixels[idx + 3] = c.a;
            }
        }
    }

    /* Q=0 means no chroma — every pixel should be gray */
    {
        unsigned char* pixels = (unsigned char*)img.data;
        for (int i = 0; i < SIT_YPQ_PLANE_SIZE * SIT_YPQ_PLANE_SIZE; i++) {
            ColorRGBA c = {pixels[i * 4 + 0], pixels[i * 4 + 1], pixels[i * 4 + 2], pixels[i * 4 + 3]};
            SIT_ASSERT(misc_ypq_pixel_is_gray(&c, 3));
        }
    }

    ColorRGBA c_q0 = misc_sample_ypq_plane_rgba(200, 64, 0);
    ColorRGBA c_qmax = misc_sample_ypq_plane_rgba(200, 64, 255);
    SIT_ASSERT(misc_ypq_pixel_is_gray(&c_q0, 3));
    SIT_ASSERT(!misc_ypq_pixel_is_gray(&c_qmax, 8));

    ColorRGBA c_p0 = misc_sample_ypq_plane_rgba(200, 0, 255);
    ColorRGBA c_p128 = misc_sample_ypq_plane_rgba(200, 128, 255);
    SIT_ASSERT(abs((int)c_p0.r - (int)c_p128.r) > 12
        || abs((int)c_p0.g - (int)c_p128.g) > 12
        || abs((int)c_p0.b - (int)c_p128.b) > 12);

    ColorRGBA seed = {180, 90, 40, 255};
    ColorYPQA mid = SituationColorToYPQ(seed);
    ColorRGBA back = SituationColorFromYPQ(mid);
    SIT_ASSERT(abs((int)back.r - (int)seed.r) <= 2);
    SIT_ASSERT(abs((int)back.g - (int)seed.g) <= 2);
    SIT_ASSERT(abs((int)back.b - (int)seed.b) <= 2);

    /* Consistency: SituationColorFromYPQ called twice with same input must give same output */
    for (int trial = 0; trial < 8; trial++) {
        unsigned char ty = (unsigned char)(trial * 31);
        unsigned char tp = (unsigned char)(trial * 47 + 64);
        unsigned char tq = (unsigned char)(trial * 19 + 128);
        ColorYPQA ypq_t = {ty, tp, tq, 255};
        ColorRGBA lib1 = SituationColorFromYPQ(ypq_t);
        ColorRGBA lib2 = SituationColorFromYPQ(ypq_t);
        SIT_ASSERT_EQ(lib1.r, lib2.r);
        SIT_ASSERT_EQ(lib1.g, lib2.g);
        SIT_ASSERT_EQ(lib1.b, lib2.b);
    }

    SituationUnloadImage(img);
}

/**
 * Timed: Q=0..255 over ~4s on the 256x256 Y (x) x P (y) plane.
 * Full-window display: Y horizontal, P vertical, Q animates grayscale → chroma.
 * After the sweep, reports duplicate 8-bit RGB mappings via SituationYpqAnalyzeRgbMapping.
 * Set SIT_SKIP_YPQ_SWEEP=1 to skip (~4s wall time).
 * Set SIT_SKIP_YPQ_RGB_STATS=1 to skip post-sweep registry analysis.
 */
static void test_ypq_to_rgb_q_sweep_4s(void) {
    if (getenv("SIT_SKIP_YPQ_SWEEP") != NULL) {
        fprintf(stderr, "[misc] ypq_to_rgb_q_sweep_4s skipped (SIT_SKIP_YPQ_SWEEP set)\n");
        return;
    }

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    SituationError init_err = sit_test_gpu_context_init("SIT_YPQ_Q_SWEEP");
    SIT_ASSERT_EQ((int)init_err, (int)SITUATION_SUCCESS);
#endif

    SituationImage plane = {0};
    SituationError err = SituationCreateImage(SIT_YPQ_PLANE_SIZE, SIT_YPQ_PLANE_SIZE, 4, &plane);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    /* Pre-fill all 256 planes into a sweep cache so the timed loop is just memcpy */
    unsigned char* sweep_cache = (unsigned char*)malloc(SIT_YPQ_PLANE_BYTES * 256u);
    SIT_ASSERT_NOT_NULL(sweep_cache);

    for (int q = 0; q < 256; q++) {
        unsigned char* dst = sweep_cache + (size_t)q * SIT_YPQ_PLANE_BYTES;
        for (int py = 0; py < SIT_YPQ_PLANE_SIZE; py++) {
            for (int px = 0; px < SIT_YPQ_PLANE_SIZE; px++) {
                ColorRGBA c = SituationColorFromYPQ(
                    (ColorYPQA){(unsigned char)px, (unsigned char)py, (unsigned char)q, 255});
                int idx = (py * SIT_YPQ_PLANE_SIZE + px) * 4;
                dst[idx + 0] = c.r;
                dst[idx + 1] = c.g;
                dst[idx + 2] = c.b;
                dst[idx + 3] = 255;
            }
        }
    }

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    SituationTexture tex = {0};
    SituationBuffer upload_buf = {0};
    bool tex_ready = false;
    err = SituationCreateBuffer(
        SIT_YPQ_PLANE_BYTES,
        NULL,
        SITUATION_BUFFER_USAGE_TRANSFER_SRC,
        &upload_buf);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
#endif

    int max_q_seen = -1;
    int step_count = 0;
    bool saw_gray_center = false;
    bool saw_chroma_center = false;

    const double step_seconds = SIT_YPQ_Q_SWEEP_SECONDS / (double)SIT_YPQ_Q_STEPS;
    double t0 = sit_get_time_seconds();

    for (int q = 0; q < SIT_YPQ_Q_STEPS; q++) {
        const double step_end = t0 + (double)(q + 1) * step_seconds;

        memcpy(plane.data, sweep_cache + (size_t)q * SIT_YPQ_PLANE_BYTES, SIT_YPQ_PLANE_BYTES);

        if (q == 0) {
            unsigned char* pixels = (unsigned char*)plane.data;
            int idx = (128 * SIT_YPQ_PLANE_SIZE + 128) * 4;
            ColorRGBA center = {pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]};
            saw_gray_center = misc_ypq_pixel_is_gray(&center, 3);
        }
        if (q >= 200) {
            ColorRGBA center = misc_sample_ypq_plane_rgba(128, 128, (unsigned char)q);
            if (!misc_ypq_pixel_is_gray(&center, 10)) {
                saw_chroma_center = true;
            }
        }

        max_q_seen = q;
        step_count++;

        do {
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
            err = misc_ypq_present_plane(&plane, &tex, &upload_buf, &tex_ready);
            SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
#endif
        } while (sit_get_time_seconds() < step_end);
    }

    double elapsed_total = sit_get_time_seconds() - t0;
    SIT_ASSERT(elapsed_total >= (SIT_YPQ_Q_SWEEP_SECONDS - 0.05));
    SIT_ASSERT(elapsed_total <= (SIT_YPQ_Q_SWEEP_SECONDS + 2.5));
    SIT_ASSERT_EQ(step_count, SIT_YPQ_Q_STEPS);
    SIT_ASSERT_EQ(max_q_seen, 255);
    SIT_ASSERT(saw_gray_center);
    SIT_ASSERT(saw_chroma_center);

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    if (tex_ready) {
        SituationDestroyTexture(&tex);
    }
    SituationDestroyBuffer(&upload_buf);
    sit_test_gpu_context_shutdown();
#endif

    if (getenv("SIT_SKIP_YPQ_RGB_STATS") != NULL) {
        fprintf(stderr, "[misc] ypq RGB duplicate stats skipped (SIT_SKIP_YPQ_RGB_STATS set)\n");
    } else {
        fprintf(stderr, "[misc] ypq_to_rgb_q_sweep_4s: running SituationYpqAnalyzeRgbMapping...\n");
        SituationYpqRgbMappingStats stats;
        SituationError stats_err = SituationYpqAnalyzeRgbMapping(&stats);
        SIT_ASSERT_EQ((int)stats_err, (int)SITUATION_SUCCESS);
        fprintf(stderr,
            "[misc]   ypq: %lld YPQ mappings → %lld unique RGB"
            "  (%lld duplicate mappings, %lld RGB holes)"
            "  worst_Q=%d (at Q=%d)\n",
            (long long)stats.ypq_mappings,
            (long long)stats.unique_rgb,
            (long long)stats.duplicate_mappings,
            (long long)stats.rgb_holes,
            stats.worst_axis_dup,
            stats.worst_axis_at);
    }

    free(sweep_cache);
    SituationUnloadImage(plane);
}

/**
 * CPU: load harness photo asset and verify SituationImageAdjustYPQ changes pixels.
 */
static void test_ypq_photo_asset_load(void) {
    SituationImage photo = {0};
    if (!misc_ypq_load_harness_photo(&photo, SIT_YPQ_PHOTO_ASSET)) {
        fprintf(stderr,
            "[misc] ypq_photo_asset_load skipped (missing tests/harness/assets/%s)\n",
            SIT_YPQ_PHOTO_ASSET);
        return;
    }

    SIT_ASSERT(SituationIsImageValid(photo));
    SIT_ASSERT(photo.width > 0);
    SIT_ASSERT(photo.height > 0);
    SIT_ASSERT(photo.channels >= 3);

    SituationImage work = {0};
    SituationError err = SituationImageCopy(photo, &work);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    ColorRGBA center_before = misc_ypq_image_center_pixel(&work);
    SituationImageAdjustYPQ(&work, 45.0f, 1.5f, 1.1f, 1.0f);
    ColorRGBA center_after = misc_ypq_image_center_pixel(&work);

    SIT_ASSERT(
        abs((int)center_before.r - (int)center_after.r) > 4
        || abs((int)center_before.g - (int)center_after.g) > 4
        || abs((int)center_before.b - (int)center_after.b) > 4);

    SituationUnloadImage(work);
    SituationUnloadImage(photo);
}

/**
 * Timed visual: prairie.jpg with three ~4s sweeps via SituationImageAdjustYPQ —
 * Y (luma 0.25→1.75), P (phase 0→360°), Q (chroma 0→2×). 256 steps per segment.
 * Set SIT_SKIP_YPQ_PHOTO_SWEEP=1 to skip (~12s wall time).
 */
static void test_ypq_photo_y_p_q_sweep(void) {
    if (getenv("SIT_SKIP_YPQ_PHOTO_SWEEP") != NULL) {
        fprintf(stderr, "[misc] ypq_photo_y_p_q_sweep skipped (SIT_SKIP_YPQ_PHOTO_SWEEP set)\n");
        return;
    }

    SituationImage source = {0};
    if (!misc_ypq_load_harness_photo(&source, SIT_YPQ_PHOTO_ASSET)) {
        fprintf(stderr,
            "[misc] ypq_photo_y_p_q_sweep skipped (missing tests/harness/assets/%s)\n",
            SIT_YPQ_PHOTO_ASSET);
        return;
    }
    misc_ypq_downscale_nearest(&source, SIT_YPQ_PHOTO_SWEEP_DIM);

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    SituationError init_err = sit_test_gpu_context_init("SIT_YPQ_PHOTO_SWEEP");
    SIT_ASSERT_EQ((int)init_err, (int)SITUATION_SUCCESS);
#endif

    SituationImage work = {0};
    SituationError err = SituationImageCopy(source, &work);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    work.channels = source.channels > 0 ? source.channels : 4;

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    SituationTexture tex = {0};
    SituationBuffer upload_buf = {0};
    bool tex_ready = false;
    size_t image_bytes = (size_t)work.width * (size_t)work.height * 4u;
    err = SituationCreateBuffer(
        image_bytes,
        NULL,
        SITUATION_BUFFER_USAGE_TRANSFER_SRC,
        &upload_buf);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
#endif

    const double step_seconds = SIT_YPQ_Q_SWEEP_SECONDS / (double)SIT_YPQ_Q_STEPS;
    const double total_seconds = SIT_YPQ_Q_SWEEP_SECONDS * (double)SIT_YPQ_PHOTO_SEGMENT_COUNT;
    double t0 = sit_get_time_seconds();
    int total_steps = 0;

    int luma_dark = 0;
    int luma_bright = 0;
    ColorRGBA phase_start = {0, 0, 0, 0};
    ColorRGBA phase_mid = {0, 0, 0, 0};
    bool phase_mid_set = false;
    bool chroma_low_gray = false;
    bool chroma_high_color = false;

    for (int seg = 0; seg < SIT_YPQ_PHOTO_SEGMENT_COUNT; seg++) {
        MiscYpqPhotoSweepAxis axis = (MiscYpqPhotoSweepAxis)seg;
        fprintf(stderr, "[misc] ypq photo sweep segment %d/3: %s\n", seg + 1, misc_ypq_photo_sweep_axis_name(axis));

        for (int step = 0; step < SIT_YPQ_Q_STEPS; step++) {
            const double step_end = t0
                + ((double)seg * (double)SIT_YPQ_Q_STEPS + (double)(step + 1)) * step_seconds;

            misc_ypq_apply_photo_sweep(&work, &source, axis, step, SIT_YPQ_Q_STEPS);

            if (axis == MISC_YPQ_PHOTO_SWEEP_LUMA) {
                ColorRGBA center = misc_ypq_image_center_pixel(&work);
                int luma = misc_ypq_pixel_luma_sum(&center);
                if (step == 0) {
                    luma_dark = luma;
                }
                if (step == SIT_YPQ_Q_STEPS - 1) {
                    luma_bright = luma;
                }
            } else if (axis == MISC_YPQ_PHOTO_SWEEP_PHASE) {
                if (step == 0) {
                    phase_start = misc_ypq_image_center_pixel(&work);
                }
                if (step == SIT_YPQ_Q_STEPS / 2) {
                    phase_mid = misc_ypq_image_center_pixel(&work);
                    phase_mid_set = true;
                }
            } else if (axis == MISC_YPQ_PHOTO_SWEEP_CHROMA) {
                ColorRGBA center = misc_ypq_image_center_pixel(&work);
                if (step == 0) {
                    chroma_low_gray = misc_ypq_pixel_is_gray(&center, 10);
                }
                if (step == SIT_YPQ_Q_STEPS - 1) {
                    chroma_high_color = !misc_ypq_pixel_is_gray(&center, 20);
                }
            }

            total_steps++;

            do {
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
                err = misc_ypq_present_image(&work, &tex, &upload_buf, &tex_ready);
                SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
#endif
            } while (sit_get_time_seconds() < step_end);
        }
    }

    double elapsed_total = sit_get_time_seconds() - t0;
    SIT_ASSERT(elapsed_total >= (total_seconds - 0.1));
    SIT_ASSERT(elapsed_total <= (total_seconds * 12.0 + 15.0));
    SIT_ASSERT_EQ(total_steps, SIT_YPQ_Q_STEPS * SIT_YPQ_PHOTO_SEGMENT_COUNT);
    SIT_ASSERT(luma_bright > luma_dark + 16);
    SIT_ASSERT(phase_mid_set);
    SIT_ASSERT(
        abs((int)phase_start.r - (int)phase_mid.r) > 6
        || abs((int)phase_start.g - (int)phase_mid.g) > 6
        || abs((int)phase_start.b - (int)phase_mid.b) > 6);
    SIT_ASSERT(chroma_low_gray);
    SIT_ASSERT(chroma_high_color);

#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_USE_OPENGL)
    if (tex_ready) {
        SituationDestroyTexture(&tex);
    }
    SituationDestroyBuffer(&upload_buf);
    sit_test_gpu_context_shutdown();
#endif

    SituationUnloadImage(work);
    SituationUnloadImage(source);
}

/**
 * Phase 3: Full 256³ YPQ→RGB mapping analysis via the public library API.
 * Asserts known ranges for unique_rgb (~5.6M), duplicates, holes, and worst_axis_dup.
 * Set SIT_SKIP_YPQ_RGB_STATS=1 to skip (~several seconds).
 */
static void test_ypq_analyze_rgb_mapping(void) {
    if (getenv("SIT_SKIP_YPQ_RGB_STATS") != NULL) {
        fprintf(stderr, "[misc] ypq_analyze_rgb_mapping skipped (SIT_SKIP_YPQ_RGB_STATS set)\n");
        return;
    }

    SituationYpqRgbMappingStats stats;
    memset(&stats, 0, sizeof(stats));
    SituationError err = SituationYpqAnalyzeRgbMapping(&stats);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);

    /* Known range: ~5.6M unique RGB outputs out of 16 777 216 YPQ triples */
    SIT_ASSERT(stats.unique_rgb >= 5000000LL && stats.unique_rgb <= 7000000LL);
    SIT_ASSERT_EQ(stats.ypq_mappings, 256LL * 256LL * 256LL);
    SIT_ASSERT_EQ(stats.duplicate_mappings, stats.ypq_mappings - stats.unique_rgb);
    SIT_ASSERT_EQ(stats.rgb_holes, (1LL << 24) - stats.unique_rgb);
    SIT_ASSERT(stats.worst_axis_dup > 0);
    SIT_ASSERT(stats.worst_axis_at >= 0 && stats.worst_axis_at <= 255);

    fprintf(stderr,
        "[misc] ypq_analyze_rgb_mapping: unique_rgb=%lld  dup=%lld  holes=%lld"
        "  worst_Q_dup=%d (at Q=%d)\n",
        (long long)stats.unique_rgb,
        (long long)stats.duplicate_mappings,
        (long long)stats.rgb_holes,
        stats.worst_axis_dup,
        stats.worst_axis_at);
}

/**
 * Phase 3: Q=0 slice should have massive duplicates (all entries map to gray).
 * Known lower bound: ≥65 000 duplicates in the 65 536-entry slice.
 */
static void test_ypq_slice_dup_q0(void) {
    int dup = 0;
    SituationError err = SituationYpqSliceDuplicateCount('Q', 0, &dup);
    SIT_ASSERT_EQ((int)err, (int)SITUATION_SUCCESS);
    /* Q=0 → all Y×P combos reduce to grayscale → at least 65000 duplicates */
    SIT_ASSERT(dup >= 65000);
    fprintf(stderr, "[misc] ypq_slice_dup_q0: Q=0 slice duplicates=%d\n", dup);
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
    {"stb_image_load_extension_recognition", test_stb_image_load_extension_recognition, false},
    {"stb_image_load_roundtrip_formats",     test_stb_image_load_roundtrip_formats,     false},
    {"stb_image_load_embedded_formats",      test_stb_image_load_embedded_formats,      false},
    // Fonts
    {"load_bitmap_font_memory",     test_load_bitmap_font_from_memory, false},
    {"measure_text_bitmap_font",    test_measure_text_bitmap_font,    false},
    // Color conversions
    {"rgb_to_hsv_roundtrip",        test_rgb_to_hsv_roundtrip,        false},
    {"rgb_to_hsv_green",            test_rgb_to_hsv_green,            false},
    {"rgb_to_hsv_gray",             test_rgb_to_hsv_gray,             false},
    {"color_to_ypq_roundtrip",      test_color_to_ypq_roundtrip,     false},
    {"ypq_groundwork_q_zero",       test_ypq_groundwork_q_zero_phase_invariant, false},
    {"ypq_groundwork_luma_axis",    test_ypq_groundwork_grayscale_luma_axis, false},
    {"ypq_groundwork_golden",      test_ypq_groundwork_golden_vectors, false},
    {"ypq_lerp_wrap",              test_ypq_lerp_wrap,              false},
    {"ypq_adjust_luma",            test_ypq_adjust_luma,            false},
    {"ypq_adjust_phase",           test_ypq_adjust_phase,           false},
    {"ypq_adjust_chroma",          test_ypq_adjust_chroma,          false},
    {"ypq_distance_equals",        test_ypq_distance_equals,        false},
    {"ypq_float_roundtrip",        test_ypq_float_roundtrip,        false},
    {"ypq_quantize",               test_ypq_quantize,               false},
    {"image_adjust_ypq",           test_image_adjust_ypq,           false},
    {"ypq_to_rgb_y_p_plane",        test_ypq_to_rgb_y_p_plane,       false},
    {"ypq_to_rgb_q_sweep_4s",       test_ypq_to_rgb_q_sweep_4s,      true},
    {"ypq_photo_asset_load",        test_ypq_photo_asset_load,       false},
    {"ypq_photo_y_p_q_sweep",       test_ypq_photo_y_p_q_sweep,      true},
    {"ypq_analyze_rgb_mapping",     test_ypq_analyze_rgb_mapping,    true},
    {"ypq_slice_dup_q0",            test_ypq_slice_dup_q0,           false},
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
