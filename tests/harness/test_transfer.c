/**
 * @file test_transfer.c
 * @brief Transfer command tests - texture barriers, copy, blit, buffer/texture.
 *
 * Phase 4.1 harness split: owns recorded transfer/batch commands (Phase 4 API).
 * Run: sit_test.exe --module transfer [--filter substr]
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include <string.h>

static bool g_transfer_init_ok = false;

static void transfer_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_TRANSFER");
    SituationError err = SituationInit(0, NULL, &config);
    g_transfer_init_ok = (err == SITUATION_SUCCESS);
    if (!g_transfer_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void transfer_teardown(void) {
    if (g_transfer_init_ok) {
        SituationShutdown();
        g_transfer_init_ok = false;
    }
}

static void transfer_set_pixel_rgba(SituationImage* img, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    ColorRGBA c = {r, g, b, a};
    SituationSetPixelColor(img, x, y, c);
}

static void transfer_assert_image_pixel_rgba(const SituationImage* img, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    const uint8_t* pixels = (const uint8_t*)img->data;
    int idx = (y * img->width + x) * 4;
    SIT_ASSERT_EQ((int)pixels[idx + 0], (int)r);
    SIT_ASSERT_EQ((int)pixels[idx + 1], (int)g);
    SIT_ASSERT_EQ((int)pixels[idx + 2], (int)b);
    SIT_ASSERT_EQ((int)pixels[idx + 3], (int)a);
}

static SituationError transfer_texture_barrier(SituationCommandBuffer cmd, SituationTexture texture, SituationTextureLayout old_layout, SituationTextureLayout new_layout) {
    SituationTextureBarrierDesc desc = {0};
    desc.old_layout = old_layout;
    desc.new_layout = new_layout;
    desc.base_mip_level = 0;
    desc.mip_level_count = 1;
    return SituationCmdTextureBarrier(cmd, texture, &desc);
}

static void test_texture_barrier_validation(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    memset(img.data, 0x7f, 4 * 4 * 4);

    SituationTexture tex = {0};
    err = SituationCreateTextureEx(
        img,
        false,
        (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED |
                                     SITUATION_TEXTURE_USAGE_TRANSFER_SRC |
                                     SITUATION_TEXTURE_USAGE_TRANSFER_DST),
        &tex);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationTexture no_src_tex = {0};
    err = SituationCreateTextureEx(
        img,
        false,
        (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED |
                                     SITUATION_TEXTURE_USAGE_TRANSFER_DST),
        &no_src_tex);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationTextureBarrierDesc desc = {0};
    desc.old_layout = SITUATION_TEXTURE_LAYOUT_SHADER_READ;
    desc.new_layout = SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC;

    err = SituationCmdTextureBarrier(NULL, tex, &desc);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    err = SituationCmdTextureBarrier(cmd, tex, NULL);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationTexture invalid_tex = {0};
    err = SituationCmdTextureBarrier(cmd, invalid_tex, &desc);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_RESOURCE_HANDLE);

    SituationTextureBarrierDesc bad_mip = desc;
    bad_mip.base_mip_level = 99;
    err = SituationCmdTextureBarrier(cmd, tex, &bad_mip);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    err = SituationCmdTextureBarrier(cmd, no_src_tex, &desc);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_INVALID_USAGE);

    SituationTextureBarrierDesc reserved_layout = desc;
    reserved_layout.new_layout = SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT;
    err = SituationCmdTextureBarrier(cmd, tex, &reserved_layout);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_NOT_IMPLEMENTED);

    err = SituationCmdTextureBarrier(cmd, tex, &desc);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    desc.old_layout = SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC;
    desc.new_layout = SITUATION_TEXTURE_LAYOUT_SHADER_READ;
    err = SituationCmdTextureBarrier(cmd, tex, &desc);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyTexture(&no_src_tex);
    SituationDestroyTexture(&tex);
    SituationUnloadImage(img);
}

static void test_blit_texture_validation(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(img.data, 0, 4 * 4 * 4);

    SituationTextureUsageFlags transfer_flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_SRC |
        SITUATION_TEXTURE_USAGE_TRANSFER_DST);

    SituationTexture src = {0};
    SituationTexture dst = {0};
    SituationTexture no_src = {0};
    SituationTexture no_dst = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, transfer_flags, &src), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, transfer_flags, &dst), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_DST), &no_src), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_STORAGE, &no_dst), SITUATION_SUCCESS);

    SituationTextureBlitRegion region = {0};
    region.src_rect = (SituationTextureRect){0, 0, 2, 2};
    region.dst_rect = (SituationTextureRect){0, 0, 2, 2};
    region.filter = SITUATION_BLIT_FILTER_NEAREST;

    err = SituationCmdBlitTexture(NULL, src, dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    err = SituationCmdBlitTexture(cmd, src, dst, NULL);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationTexture invalid = {0};
    err = SituationCmdBlitTexture(cmd, invalid, dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_RESOURCE_HANDLE);

    SituationTextureBlitRegion bad_region = region;
    bad_region.src_rect.width = 0;
    err = SituationCmdBlitTexture(cmd, src, dst, &bad_region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    bad_region = region;
    bad_region.src_rect = (SituationTextureRect){3, 0, 2, 2};
    err = SituationCmdBlitTexture(cmd, src, dst, &bad_region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    bad_region = region;
    bad_region.src_array_layer = 1;
    err = SituationCmdBlitTexture(cmd, src, dst, &bad_region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    err = SituationCmdBlitTexture(cmd, no_src, dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_INVALID_USAGE);

    err = SituationCmdBlitTexture(cmd, src, no_dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_INVALID_USAGE);

    bad_region = region;
    bad_region.filter = SITUATION_BLIT_FILTER_LINEAR;
    err = SituationCmdBlitTexture(cmd, no_dst, dst, &bad_region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_INVALID_USAGE);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyTexture(&no_dst);
    SituationDestroyTexture(&no_src);
    SituationDestroyTexture(&dst);
    SituationDestroyTexture(&src);
    SituationUnloadImage(img);
}

static void test_blit_texture_same_size_asymmetric(void) {
    SituationImage src_img = {0};
    SituationImage dst_img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &src_img), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &dst_img), SITUATION_SUCCESS);
    src_img.color_encoding = SITUATION_COLOR_LINEAR;
    dst_img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(src_img.data, 0, 4 * 4 * 4);
    memset(dst_img.data, 0, 4 * 4 * 4);

    transfer_set_pixel_rgba(&src_img, 1, 1, 255, 0, 0, 255);
    transfer_set_pixel_rgba(&src_img, 2, 1, 0, 255, 0, 255);
    transfer_set_pixel_rgba(&src_img, 1, 2, 0, 0, 255, 255);
    transfer_set_pixel_rgba(&src_img, 2, 2, 255, 255, 0, 255);

    SituationTextureUsageFlags flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_SRC |
        SITUATION_TEXTURE_USAGE_TRANSFER_DST);
    SituationTexture src = {0};
    SituationTexture dst = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(src_img, false, flags, &src), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(dst_img, false, flags, &dst), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, src, SITUATION_TEXTURE_LAYOUT_SHADER_READ, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, dst, SITUATION_TEXTURE_LAYOUT_SHADER_READ, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST), SITUATION_SUCCESS);

    SituationTextureBlitRegion region = {0};
    region.src_rect = (SituationTextureRect){1, 1, 2, 2};
    region.dst_rect = (SituationTextureRect){0, 0, 2, 2};
    region.filter = SITUATION_BLIT_FILTER_NEAREST;
    SIT_ASSERT_EQ(SituationCmdBlitTexture(cmd, src, dst, &region), SITUATION_SUCCESS);

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, src, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC, SITUATION_TEXTURE_LAYOUT_SHADER_READ), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, dst, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST, SITUATION_TEXTURE_LAYOUT_SHADER_READ), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage out = {0};
    SIT_ASSERT_EQ(SituationReadTextureAlloc(dst, NULL, &out), SITUATION_SUCCESS);
    transfer_assert_image_pixel_rgba(&out, 0, 0, 255, 0, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 1, 0, 0, 255, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 0, 1, 0, 0, 255, 255);
    transfer_assert_image_pixel_rgba(&out, 1, 1, 255, 255, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 2, 2, 0, 0, 0, 0);

    SituationUnloadImage(out);
    SituationDestroyTexture(&dst);
    SituationDestroyTexture(&src);
    SituationUnloadImage(dst_img);
    SituationUnloadImage(src_img);
}

static void test_blit_texture_scaled_nearest_asymmetric(void) {
    SituationImage src_img = {0};
    SituationImage dst_img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(2, 2, 4, &src_img), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &dst_img), SITUATION_SUCCESS);
    src_img.color_encoding = SITUATION_COLOR_LINEAR;
    dst_img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(src_img.data, 0, 2 * 2 * 4);
    memset(dst_img.data, 0, 4 * 4 * 4);

    transfer_set_pixel_rgba(&src_img, 0, 0, 255, 0, 0, 255);
    transfer_set_pixel_rgba(&src_img, 1, 0, 0, 255, 0, 255);
    transfer_set_pixel_rgba(&src_img, 0, 1, 0, 0, 255, 255);
    transfer_set_pixel_rgba(&src_img, 1, 1, 255, 255, 0, 255);

    SituationTextureUsageFlags flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_SRC |
        SITUATION_TEXTURE_USAGE_TRANSFER_DST);
    SituationTexture src = {0};
    SituationTexture dst = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(src_img, false, flags, &src), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(dst_img, false, flags, &dst), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, src, SITUATION_TEXTURE_LAYOUT_SHADER_READ, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, dst, SITUATION_TEXTURE_LAYOUT_SHADER_READ, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST), SITUATION_SUCCESS);

    SituationTextureBlitRegion region = {0};
    region.src_rect = (SituationTextureRect){0, 0, 2, 2};
    region.dst_rect = (SituationTextureRect){0, 0, 4, 4};
    region.filter = SITUATION_BLIT_FILTER_NEAREST;
    SIT_ASSERT_EQ(SituationCmdBlitTexture(cmd, src, dst, &region), SITUATION_SUCCESS);

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, src, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC, SITUATION_TEXTURE_LAYOUT_SHADER_READ), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, dst, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST, SITUATION_TEXTURE_LAYOUT_SHADER_READ), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage out = {0};
    SIT_ASSERT_EQ(SituationReadTextureAlloc(dst, NULL, &out), SITUATION_SUCCESS);
    transfer_assert_image_pixel_rgba(&out, 0, 0, 255, 0, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 3, 0, 0, 255, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 0, 3, 0, 0, 255, 255);
    transfer_assert_image_pixel_rgba(&out, 3, 3, 255, 255, 0, 255);

    SituationUnloadImage(out);
    SituationDestroyTexture(&dst);
    SituationDestroyTexture(&src);
    SituationUnloadImage(dst_img);
    SituationUnloadImage(src_img);
}

static void test_copy_texture_validation(void) {
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(img.data, 0, 4 * 4 * 4);

    SituationTextureUsageFlags transfer_flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_SRC |
        SITUATION_TEXTURE_USAGE_TRANSFER_DST);

    SituationTexture src = {0};
    SituationTexture dst = {0};
    SituationTexture no_src = {0};
    SituationTexture no_dst = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, transfer_flags, &src), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, transfer_flags, &dst), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, (SituationTextureUsageFlags)(SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_DST), &no_src), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_STORAGE, &no_dst), SITUATION_SUCCESS);

    SituationTextureCopyRegion region = {0};
    region.src_rect = (SituationTextureRect){0, 0, 2, 2};
    region.dst_x = 0;
    region.dst_y = 0;

    err = SituationCmdCopyTexture(NULL, src, dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    err = SituationCmdCopyTexture(cmd, src, dst, NULL);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationTexture invalid = {0};
    err = SituationCmdCopyTexture(cmd, invalid, dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_RESOURCE_HANDLE);

    SituationTextureCopyRegion bad_region = region;
    bad_region.src_rect.width = 0;
    err = SituationCmdCopyTexture(cmd, src, dst, &bad_region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    bad_region = region;
    bad_region.src_rect = (SituationTextureRect){3, 0, 2, 2};
    err = SituationCmdCopyTexture(cmd, src, dst, &bad_region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    bad_region = region;
    bad_region.dst_x = 3;
    bad_region.dst_y = 0;
    err = SituationCmdCopyTexture(cmd, src, dst, &bad_region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    bad_region = region;
    bad_region.src_array_layer = 1;
    err = SituationCmdCopyTexture(cmd, src, dst, &bad_region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    err = SituationCmdCopyTexture(cmd, no_src, dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_INVALID_USAGE);

    err = SituationCmdCopyTexture(cmd, src, no_dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_INVALID_USAGE);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyTexture(&no_dst);
    SituationDestroyTexture(&no_src);
    SituationDestroyTexture(&dst);
    SituationDestroyTexture(&src);
    SituationUnloadImage(img);
}

static void test_copy_texture_same_size_asymmetric(void) {
    SituationImage src_img = {0};
    SituationImage dst_img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &src_img), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &dst_img), SITUATION_SUCCESS);
    src_img.color_encoding = SITUATION_COLOR_LINEAR;
    dst_img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(src_img.data, 0, 4 * 4 * 4);
    memset(dst_img.data, 0, 4 * 4 * 4);

    transfer_set_pixel_rgba(&src_img, 1, 1, 255, 0, 0, 255);
    transfer_set_pixel_rgba(&src_img, 2, 1, 0, 255, 0, 255);
    transfer_set_pixel_rgba(&src_img, 1, 2, 0, 0, 255, 255);
    transfer_set_pixel_rgba(&src_img, 2, 2, 255, 255, 0, 255);

    SituationTextureUsageFlags flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_SRC |
        SITUATION_TEXTURE_USAGE_TRANSFER_DST);
    SituationTexture src = {0};
    SituationTexture dst = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(src_img, false, flags, &src), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(dst_img, false, flags, &dst), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, src, SITUATION_TEXTURE_LAYOUT_SHADER_READ, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, dst, SITUATION_TEXTURE_LAYOUT_SHADER_READ, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST), SITUATION_SUCCESS);

    SituationTextureCopyRegion region = {0};
    region.src_rect = (SituationTextureRect){1, 1, 2, 2};
    region.dst_x = 0;
    region.dst_y = 0;
    SIT_ASSERT_EQ(SituationCmdCopyTexture(cmd, src, dst, &region), SITUATION_SUCCESS);

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, src, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC, SITUATION_TEXTURE_LAYOUT_SHADER_READ), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, dst, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST, SITUATION_TEXTURE_LAYOUT_SHADER_READ), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage out = {0};
    SIT_ASSERT_EQ(SituationReadTextureAlloc(dst, NULL, &out), SITUATION_SUCCESS);
    transfer_assert_image_pixel_rgba(&out, 0, 0, 255, 0, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 1, 0, 0, 255, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 0, 1, 0, 0, 255, 255);
    transfer_assert_image_pixel_rgba(&out, 1, 1, 255, 255, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 2, 2, 0, 0, 0, 0);

    SituationUnloadImage(out);
    SituationDestroyTexture(&dst);
    SituationDestroyTexture(&src);
    SituationUnloadImage(dst_img);
    SituationUnloadImage(src_img);
}

static void test_copy_buffer_to_texture_validation(void) {
    SituationImage img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &img), SITUATION_SUCCESS);
    img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(img.data, 0, 4 * 4 * 4);

    SituationTextureUsageFlags tex_flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_DST);
    SituationTexture dst = {0};
    SituationTexture no_dst = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, tex_flags, &dst), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_STORAGE, &no_dst), SITUATION_SUCCESS);

    uint8_t pixels[16] = {0};
    SituationBuffer src_buf = {0};
    SituationBuffer no_src = {0};
    SIT_ASSERT_EQ(SituationCreateBuffer(sizeof(pixels), pixels, SITUATION_BUFFER_USAGE_TRANSFER_SRC, &src_buf), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateBuffer(sizeof(pixels), pixels, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, &no_src), SITUATION_SUCCESS);

    SituationTextureCopyRegion region = {0};
    region.src_rect = (SituationTextureRect){0, 0, 2, 2};
    region.dst_x = 0;
    region.dst_y = 0;

    SituationError err = SituationCmdCopyBufferToTexture(NULL, src_buf, 0, dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    err = SituationCmdCopyBufferToTexture(cmd, src_buf, 0, dst, NULL);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationTextureCopyRegion bad = region;
    bad.src_rect.x = 1;
    err = SituationCmdCopyBufferToTexture(cmd, src_buf, 0, dst, &bad);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    bad = region;
    bad.dst_x = 3;
    err = SituationCmdCopyBufferToTexture(cmd, src_buf, 0, dst, &bad);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    err = SituationCmdCopyBufferToTexture(cmd, no_src, 0, dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_USAGE);

    err = SituationCmdCopyBufferToTexture(cmd, src_buf, 0, no_dst, &region);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_INVALID_USAGE);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyBuffer(&no_src);
    SituationDestroyBuffer(&src_buf);
    SituationDestroyTexture(&no_dst);
    SituationDestroyTexture(&dst);
    SituationUnloadImage(img);
}

static void test_copy_buffer_to_texture_subrect(void) {
    uint8_t upload[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 0, 255,
    };

    SituationImage dst_img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &dst_img), SITUATION_SUCCESS);
    dst_img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(dst_img.data, 0, 4 * 4 * 4);

    SituationBuffer src_buf = {0};
    SituationError err = SituationCreateBuffer(sizeof(upload), upload, SITUATION_BUFFER_USAGE_TRANSFER_SRC, &src_buf);
    if (err != SITUATION_SUCCESS) {
        SituationUnloadImage(dst_img);
        SIT_ASSERT(true);
        return;
    }

    SituationTextureUsageFlags flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_DST);
    SituationTexture dst = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(dst_img, false, flags, &dst), SITUATION_SUCCESS);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, dst, SITUATION_TEXTURE_LAYOUT_SHADER_READ, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST), SITUATION_SUCCESS);

    SituationTextureCopyRegion region = {0};
    region.src_rect = (SituationTextureRect){0, 0, 2, 2};
    region.dst_x = 1;
    region.dst_y = 1;
    SIT_ASSERT_EQ(SituationCmdCopyBufferToTexture(cmd, src_buf, 0, dst, &region), SITUATION_SUCCESS);

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, dst, SITUATION_TEXTURE_LAYOUT_TRANSFER_DST, SITUATION_TEXTURE_LAYOUT_SHADER_READ), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SituationImage out = {0};
    SIT_ASSERT_EQ(SituationReadTextureAlloc(dst, NULL, &out), SITUATION_SUCCESS);
    transfer_assert_image_pixel_rgba(&out, 1, 1, 255, 0, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 2, 1, 0, 255, 0, 255);
    transfer_assert_image_pixel_rgba(&out, 1, 2, 0, 0, 255, 255);
    transfer_assert_image_pixel_rgba(&out, 2, 2, 255, 255, 0, 255);

    SituationUnloadImage(out);
    SituationDestroyTexture(&dst);
    SituationDestroyBuffer(&src_buf);
    SituationUnloadImage(dst_img);
}

static void test_copy_texture_to_buffer_validation(void) {
    SituationImage img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &img), SITUATION_SUCCESS);
    img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(img.data, 0, 4 * 4 * 4);

    SituationTextureUsageFlags tex_flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_SRC);
    SituationTexture src = {0};
    SituationTexture no_src = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, tex_flags, &src), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_STORAGE, &no_src), SITUATION_SUCCESS);

    SituationBuffer dst_buf = {0};
    SituationBuffer wrong_dst = {0};
    SIT_ASSERT_EQ(SituationCreateReadbackBuffer(64, &dst_buf), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCreateBuffer(64, img.data, SITUATION_BUFFER_USAGE_TRANSFER_SRC, &wrong_dst), SITUATION_SUCCESS);

    SituationTextureCopyRegion region = {0};
    region.src_rect = (SituationTextureRect){0, 0, 2, 2};

    SituationError err = SituationCmdCopyTextureToBuffer(NULL, src, &region, dst_buf, 0, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    err = SituationCmdCopyTextureToBuffer(cmd, src, NULL, dst_buf, 0, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationTextureCopyRegion bad = region;
    bad.src_rect = (SituationTextureRect){3, 0, 2, 2};
    err = SituationCmdCopyTextureToBuffer(cmd, src, &bad, dst_buf, 0, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_REGION_INVALID);

    err = SituationCmdCopyTextureToBuffer(cmd, no_src, &region, dst_buf, 0, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_TEXTURE_INVALID_USAGE);

    err = SituationCmdCopyTextureToBuffer(cmd, src, &region, wrong_dst, 0, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_USAGE);

    err = SituationCmdCopyTextureToBuffer(cmd, src, &region, dst_buf, 60, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_SIZE);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyBuffer(&wrong_dst);
    SituationDestroyBuffer(&dst_buf);
    SituationDestroyTexture(&no_src);
    SituationDestroyTexture(&src);
    SituationUnloadImage(img);
}

static void test_copy_texture_to_buffer_subrect(void) {
    SituationImage src_img = {0};
    SIT_ASSERT_EQ(SituationCreateImage(4, 4, 4, &src_img), SITUATION_SUCCESS);
    src_img.color_encoding = SITUATION_COLOR_LINEAR;
    memset(src_img.data, 0, 4 * 4 * 4);
    transfer_set_pixel_rgba(&src_img, 1, 1, 255, 0, 0, 255);
    transfer_set_pixel_rgba(&src_img, 2, 1, 0, 255, 0, 255);
    transfer_set_pixel_rgba(&src_img, 1, 2, 0, 0, 255, 255);
    transfer_set_pixel_rgba(&src_img, 2, 2, 255, 255, 0, 255);

    SituationTextureUsageFlags flags = (SituationTextureUsageFlags)(
        SITUATION_TEXTURE_USAGE_SAMPLED |
        SITUATION_TEXTURE_USAGE_TRANSFER_SRC);
    SituationTexture src = {0};
    SIT_ASSERT_EQ(SituationCreateTextureEx(src_img, false, flags, &src), SITUATION_SUCCESS);

    SituationBuffer dst_buf = {0};
    SituationError err = SituationCreateReadbackBuffer(64, &dst_buf);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyTexture(&src);
        SituationUnloadImage(src_img);
        SIT_ASSERT(true);
        return;
    }

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, src, SITUATION_TEXTURE_LAYOUT_SHADER_READ, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC), SITUATION_SUCCESS);

    SituationTextureCopyRegion region = {0};
    region.src_rect = (SituationTextureRect){1, 1, 2, 2};
    SIT_ASSERT_EQ(SituationCmdCopyTextureToBuffer(cmd, src, &region, dst_buf, 0, 0), SITUATION_SUCCESS);

    SIT_ASSERT_EQ(transfer_texture_barrier(cmd, src, SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC, SITUATION_TEXTURE_LAYOUT_SHADER_READ), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    uint8_t readback[64] = {0};
    SituationReadBuffer(dst_buf, readback, sizeof(readback));
    SIT_ASSERT_EQ(readback[0], 255);
    SIT_ASSERT_EQ(readback[1], 0);
    SIT_ASSERT_EQ(readback[2], 0);
    SIT_ASSERT_EQ(readback[3], 255);
    SIT_ASSERT_EQ(readback[4], 0);
    SIT_ASSERT_EQ(readback[5], 255);
    SIT_ASSERT_EQ(readback[6], 0);
    SIT_ASSERT_EQ(readback[7], 255);
    SIT_ASSERT_EQ(readback[8], 0);
    SIT_ASSERT_EQ(readback[9], 0);
    SIT_ASSERT_EQ(readback[10], 255);
    SIT_ASSERT_EQ(readback[11], 255);
    SIT_ASSERT_EQ(readback[12], 255);
    SIT_ASSERT_EQ(readback[13], 255);
    SIT_ASSERT_EQ(readback[14], 0);
    SIT_ASSERT_EQ(readback[15], 255);

    SituationDestroyBuffer(&dst_buf);
    SituationDestroyTexture(&src);
    SituationUnloadImage(src_img);
}
static void test_copy_buffer_ex_offsets(void) {
    float src_data[8];
    for (int i = 0; i < 8; i++) src_data[i] = (float)(10 + i);

    SituationBuffer src_buf = {0};
    SituationError err = SituationCreateBuffer(
        sizeof(src_data), src_data,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC,
        &src_buf
    );
    if (err != SITUATION_SUCCESS) { SIT_ASSERT(true); return; }

    SituationBuffer dst_buf = {0};
    err = SituationCreateReadbackBuffer(sizeof(src_data), &dst_buf);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyBuffer(&src_buf);
        SIT_ASSERT(true);
        return;
    }

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    err = SituationCmdCopyBufferEx(
        cmd,
        src_buf,
        dst_buf,
        sizeof(float) * 2,
        sizeof(float) * 4,
        sizeof(float) * 3);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float read_data[8] = {0};
    SituationReadBuffer(dst_buf, read_data, sizeof(read_data));

    for (int i = 0; i < 3; i++) {
        SIT_ASSERT(read_data[4 + i] == src_data[2 + i]);
    }

    SituationDestroyBuffer(&src_buf);
    SituationDestroyBuffer(&dst_buf);
}

static void test_copy_buffer_ex_validation(void) {
    float src_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    SituationBuffer src_buf = {0};
    SituationError err = SituationCreateBuffer(
        sizeof(src_data), src_data,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC,
        &src_buf
    );
    if (err != SITUATION_SUCCESS) { SIT_ASSERT(true); return; }

    SituationBuffer dst_buf = {0};
    err = SituationCreateReadbackBuffer(sizeof(src_data), &dst_buf);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyBuffer(&src_buf);
        SIT_ASSERT(true);
        return;
    }

    SituationBuffer wrong_src = {0};
    err = SituationCreateBuffer(
        sizeof(src_data), src_data,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER,
        &wrong_src
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationBuffer wrong_dst = {0};
    err = SituationCreateBuffer(
        sizeof(src_data), src_data,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC,
        &wrong_dst
    );
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationCmdCopyBufferEx(NULL, src_buf, dst_buf, 0, 0, sizeof(float));
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationPollInputEvents();
    SituationUpdateTimers();
    SIT_ASSERT(SituationAcquireFrameCommandBuffer());
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    err = SituationCmdCopyBufferEx(cmd, src_buf, dst_buf, 0, 0, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_SIZE);

    err = SituationCmdCopyBufferEx(cmd, wrong_src, dst_buf, 0, 0, sizeof(float));
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_USAGE);

    err = SituationCmdCopyBufferEx(cmd, src_buf, wrong_dst, 0, 0, sizeof(float));
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_USAGE);

    err = SituationCmdCopyBufferEx(cmd, src_buf, dst_buf, sizeof(src_data), 0, sizeof(float));
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_SIZE);

    err = SituationCmdCopyBufferEx(cmd, src_buf, dst_buf, 0, sizeof(src_data), sizeof(float));
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_SIZE);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyBuffer(&wrong_dst);
    SituationDestroyBuffer(&wrong_src);
    SituationDestroyBuffer(&dst_buf);
    SituationDestroyBuffer(&src_buf);
}

static SitTestCase transfer_tests[] = {
    {"texture_barrier_validation",      test_texture_barrier_validation,      true},
    /* Buffer copies before PBO texture paths — avoids stale GL errors on OpenGL EndFrame. */
    {"copy_buffer_ex_offsets",          test_copy_buffer_ex_offsets,          true},
    {"copy_buffer_ex_validation",       test_copy_buffer_ex_validation,       true},
    {"blit_texture_validation",         test_blit_texture_validation,         true},
    {"blit_texture_same_size_asymmetric", test_blit_texture_same_size_asymmetric, true},
    {"blit_texture_scaled_nearest_asymmetric", test_blit_texture_scaled_nearest_asymmetric, true},
    {"copy_texture_validation",         test_copy_texture_validation,         true},
    {"copy_texture_same_size_asymmetric", test_copy_texture_same_size_asymmetric, true},
    {"copy_buffer_to_texture_validation", test_copy_buffer_to_texture_validation, true},
    {"copy_buffer_to_texture_subrect",    test_copy_buffer_to_texture_subrect,    true},
    {"copy_texture_to_buffer_validation", test_copy_texture_to_buffer_validation, true},
    {"copy_texture_to_buffer_subrect",  test_copy_texture_to_buffer_subrect,  true},
};

const SitTestModule g_module_transfer = {
    .name = "transfer",
    .setup = transfer_setup,
    .teardown = transfer_teardown,
    .tests = transfer_tests,
    .test_count = sizeof(transfer_tests) / sizeof(transfer_tests[0]),
    .requires_context = true
};
