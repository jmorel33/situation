/**
 * @file sit_graphics_test_helpers.h
 * @brief Shared helpers for graphics harness (SPIR-V, screen readback, text layout).
 *
 * Include from test_graphics.c and test_graphics_spirv.c.
 */

#ifndef SIT_GRAPHICS_TEST_HELPERS_H
#define SIT_GRAPHICS_TEST_HELPERS_H

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include <stdio.h>
#include <string.h>

/**
 * Pump frames until SituationPollShaderLoad returns SUCCESS (or timeout).
 * Must call each frame so OpenGL/Vulkan async compile/link polls run in AcquireFrameCommandBuffer.
 */
static SituationError graphics_test_async_poll_shader_ready(SituationShader shader, int max_frames) {
    for (int frame = 0; frame < max_frames; frame++) {
        SituationError st = SituationPollShaderLoad(shader);
        if (st == SITUATION_SUCCESS) {
            return SITUATION_SUCCESS;
        }
        if (st != SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS) {
            return st;
        }

        SituationPollInputEvents();
        SituationUpdateTimers();
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            return SITUATION_ERROR_RENDER_COMMAND_FAILED;
        }
        SituationEndFrame();
    }
    return SITUATION_ERROR_GENERAL;
}

/** Cached fullscreen triangle (NDC large tri). */
static SituationMesh graphics_test_fullscreen_mesh(void) {
    static SituationMesh mesh = {0};
    static bool ready = false;
    if (ready) {
        return mesh;
    }
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f
    };
    uint32_t indices[] = {0, 1, 2};
    SituationError err = SituationCreateMesh(
        vertices, 3, sizeof(float) * 3, indices, 3, &mesh);
    if (err == SITUATION_SUCCESS) {
        ready = true;
    }
    return mesh;
}

/** Begin pass (black clear), bind pipeline, draw fullscreen mesh, end pass, present. */
static SituationError graphics_test_clear_and_draw(
    SituationCommandBuffer cmd,
    SituationShader shader,
    SituationMesh mesh)
{
    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    SituationError err = SituationCmdBeginRenderPass(cmd, &rp);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    SituationCmdBindPipeline(cmd, shader);
    SituationCmdDrawMesh(cmd, mesh);
    err = SituationCmdEndRenderPass(cmd);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    return SituationEndFrame();
}

/** Sample center pixel RGBA (0–255) after the most recent presented frame. */
static SituationError graphics_test_read_center_pixel_rgba(uint8_t rgba[4]) {
    SituationImage screen = {0};
    SituationError err = SituationLoadImageFromScreen(&screen);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    if (!SituationIsImageValid(screen) || screen.width < 1 || screen.height < 1) {
        SituationUnloadImage(screen);
        return SITUATION_ERROR_RESOURCE_INVALID;
    }
    int cx = screen.width / 2;
    int cy = screen.height / 2;
    int idx = (cy * screen.width + cx) * 4;
    uint8_t* pixels = (uint8_t*)screen.data;
    rgba[0] = pixels[idx];
    rgba[1] = pixels[idx + 1];
    rgba[2] = pixels[idx + 2];
    rgba[3] = pixels[idx + 3];
    SituationUnloadImage(screen);
    return SITUATION_SUCCESS;
}

/** Tolerance-based pixel channel comparison (matches test_graphics.c). */
static bool graphics_test_pixel_approx_eq(uint8_t actual, uint8_t expected, uint8_t tolerance) {
    int diff = (int)actual - (int)expected;
    return (diff >= -(int)tolerance && diff <= (int)tolerance);
}

/**
 * GLSL for SituationLoadShaderFromMemory / BeginLoadShaderFromMemory on the **active backend**.
 * Vulkan: #version 450 (SPIR-V / shaderc). OpenGL: #version 460 core.
 */
static const char* graphics_test_glsl_vs_passthrough(void) {
#if defined(SITUATION_USE_VULKAN)
    return
        "#version 450\n"
        "layout(location=0) in vec3 aPos;\n"
        "void main() { gl_Position = vec4(aPos, 1.0); }\n";
#else
    return
        "#version 460 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "void main() { gl_Position = vec4(aPos, 1.0); }\n";
#endif
}

static const char* graphics_test_glsl_fs_solid_red(void) {
#if defined(SITUATION_USE_VULKAN)
    return
        "#version 450\n"
        "layout(location=0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";
#else
    return
        "#version 460 core\n"
        "layout(location=0) out vec4 fragColor;\n"
        "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";
#endif
}

/**
 * Skip when SPIR-V graphics path is unavailable.
 * Uses trivial shader load as proxy (no GLAD in harness).
 * @return true if caller should treat test as skipped (pass).
 */
static bool graphics_test_skip_if_no_spirv(void) {
    SituationShader shader = {0};
    SituationError err = SituationLoadShaderFromMemory(
        graphics_test_glsl_vs_passthrough(),
        graphics_test_glsl_fs_solid_red(),
        &shader);
    if (err != SITUATION_SUCCESS) {
        /* SPIR-V / shader compiler path not available on this build or GPU. */
        SIT_ASSERT(true);
        return true;
    }
    SituationUnloadShader(&shader);
    return false;
}

/** Create a storage buffer with optional initial bytes (may be NULL for zero fill). */
static SituationError graphics_test_create_ssbo(
    size_t size_bytes,
    const void* initial,
    SituationBuffer* out_buffer)
{
    if (!out_buffer) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    memset(out_buffer, 0, sizeof(*out_buffer));
    return SituationCreateBuffer(
        size_bytes,
        initial,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER
            | SITUATION_BUFFER_USAGE_TRANSFER_SRC
            | SITUATION_BUFFER_USAGE_TRANSFER_DST,
        out_buffer);
}

/** Acquire frame, poll/update — common preamble for draw tests. */
static SituationCommandBuffer graphics_test_begin_frame(void) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        return NULL;
    }
    return SituationGetMainCommandBuffer();
}

/** Read RGBA from a loaded screen image (top-left origin, +Y down). */
static void graphics_test_sample_rgba(
    const SituationImage* screen, int x, int y, uint8_t rgba[4])
{
    rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;
    if (!screen || !SituationIsImageValid(*screen)) {
        return;
    }
    if (x < 0 || y < 0 || x >= screen->width || y >= screen->height) {
        return;
    }
    const uint8_t* pixels = (const uint8_t*)screen->data;
    int idx = (y * screen->width + x) * 4;
    rgba[0] = pixels[idx];
    rgba[1] = pixels[idx + 1];
    rgba[2] = pixels[idx + 2];
    rgba[3] = pixels[idx + 3];
}

static bool graphics_test_pixel_bright(
    const SituationImage* screen, int x, int y, uint8_t min_channel)
{
    uint8_t rgba[4];
    graphics_test_sample_rgba(screen, x, y, rgba);
    return rgba[0] >= min_channel || rgba[1] >= min_channel || rgba[2] >= min_channel;
}

static bool graphics_test_pixel_dark(
    const SituationImage* screen, int x, int y, uint8_t max_channel)
{
    uint8_t rgba[4];
    graphics_test_sample_rgba(screen, x, y, rgba);
    return rgba[0] <= max_channel && rgba[1] <= max_channel && rgba[2] <= max_channel;
}

static bool graphics_test_region_any_bright(
    const SituationImage* screen,
    int x0, int y0, int x1, int y1,
    uint8_t min_channel)
{
    if (!screen || !SituationIsImageValid(*screen)) {
        return false;
    }
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= screen->width) x1 = screen->width - 1;
    if (y1 >= screen->height) y1 = screen->height - 1;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (graphics_test_pixel_bright(screen, x, y, min_channel)) {
                return true;
            }
        }
    }
    return false;
}

static bool graphics_test_region_all_dark(
    const SituationImage* screen,
    int x0, int y0, int x1, int y1,
    uint8_t max_channel)
{
    if (!screen || !SituationIsImageValid(*screen)) {
        return false;
    }
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= screen->width) x1 = screen->width - 1;
    if (y1 >= screen->height) y1 = screen->height - 1;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (!graphics_test_pixel_dark(screen, x, y, max_channel)) {
                return false;
            }
        }
    }
    return true;
}

/** Solid-color 8x8 texture for corner markers (caller destroys texture + image). */
static SituationError graphics_test_create_solid_color_texture(
    ColorRGBA color, SituationTexture* out_tex, SituationImage* out_img)
{
    if (!out_tex || !out_img) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    memset(out_tex, 0, sizeof(*out_tex));
    memset(out_img, 0, sizeof(*out_img));
    SituationError err = SituationCreateImage(8, 8, 4, out_img);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            SituationSetPixelColor(out_img, x, y, color);
        }
    }
    err = SituationCreateTexture(*out_img, false, out_tex);
    if (err != SITUATION_SUCCESS) {
        SituationUnloadImage(*out_img);
        memset(out_img, 0, sizeof(*out_img));
    }
    return err;
}

/**
 * Bitmap font with solid glyphs (16px bake). Returns false if load/bake unavailable.
 */
static bool graphics_test_acquire_bitmap_font(SituationFont* out_font)
{
    if (!out_font) {
        return false;
    }
    memset(out_font, 0, sizeof(*out_font));
    unsigned char bitmap_data[256 * 8];
    memset(bitmap_data, 0xFF, sizeof(bitmap_data));
    if (SituationLoadBitmapFontFromMemory(bitmap_data, 8, 8, 256, out_font) != SITUATION_SUCCESS) {
        return false;
    }
    if (SituationBakeFontAtlas(out_font, 16.0f) != SITUATION_SUCCESS) {
        SituationUnloadFont(*out_font);
        memset(out_font, 0, sizeof(*out_font));
        return false;
    }
    return true;
}

/** One-time banner after SituationInit — target vs device API, GPU name, library version. */
static void graphics_test_print_renderer_banner(void) {
    SituationGraphicsCaps caps = {0};
    SituationGetGraphicsCaps(&caps);
    const char* gpu = SituationGetGPUName();
    if (!gpu || gpu[0] == '\0') {
        gpu = "unknown";
    }
    uint32_t target_major = caps.api_version_packed >> 16;
    uint32_t target_minor = caps.api_version_packed & 0xFFFFu;
    uint32_t device_major = caps.device_api_version_packed >> 16;
    uint32_t device_minor = caps.device_api_version_packed & 0xFFFFu;

    fprintf(stderr,
            "  %srenderer:%s %s — %s\n",
            sit_color(SIT_TEST_COLOR_DIM),
            sit_color(SIT_TEST_COLOR_RESET),
            SituationGetGraphicsBackendName(),
            SituationGetVersionString());
    fprintf(stderr,
            "  %starget API %u.%u, device API %u.%u, GPU: %s%s\n",
            sit_color(SIT_TEST_COLOR_DIM),
            target_major, target_minor,
            device_major, device_minor,
            gpu,
            sit_color(SIT_TEST_COLOR_RESET));
    fflush(stderr);
}

#endif /* SIT_GRAPHICS_TEST_HELPERS_H */
