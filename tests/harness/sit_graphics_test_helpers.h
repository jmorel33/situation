/**
 * @file sit_graphics_test_helpers.h
 * @brief Shared helpers for graphics harness (SPIR-V, screen readback, text layout).
 *
 * Include from test_graphics.c and test_graphics_spirv.c.
 */

#ifndef SIT_GRAPHICS_TEST_HELPERS_H
#define SIT_GRAPHICS_TEST_HELPERS_H

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include <math.h>
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

#ifdef _WIN32
        Sleep(2);
#else
        usleep(2000);
#endif
    }
    return SITUATION_ERROR_GENERAL;
}

static SituationMesh s_graphics_test_fullscreen_mesh = {0};
static bool s_graphics_test_fullscreen_mesh_ready = false;

/** Cached fullscreen triangle (NDC large tri). */
static SituationMesh graphics_test_fullscreen_mesh(void) {
    if (s_graphics_test_fullscreen_mesh_ready) {
        return s_graphics_test_fullscreen_mesh;
    }
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f
    };
    uint32_t indices[] = {0, 1, 2};
    SituationError err = SituationCreateMesh(
        vertices, 3, sizeof(float) * 3, indices, 3, &s_graphics_test_fullscreen_mesh);
    if (err == SITUATION_SUCCESS) {
        s_graphics_test_fullscreen_mesh_ready = true;
    }
    return s_graphics_test_fullscreen_mesh;
}

/** Release the cached fullscreen mesh (call from module teardown to avoid leak warnings). */
static void graphics_test_destroy_fullscreen_mesh(void) {
    if (!s_graphics_test_fullscreen_mesh_ready) {
        return;
    }
    SituationDestroyMesh(&s_graphics_test_fullscreen_mesh);
    memset(&s_graphics_test_fullscreen_mesh, 0, sizeof(s_graphics_test_fullscreen_mesh));
    s_graphics_test_fullscreen_mesh_ready = false;
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

/** Cached screen readback for multi-pixel probes (one GPU readback per batch). */
static SituationImage s_graphics_test_screen_probe = {0};
static bool s_graphics_test_screen_probe_active = false;

static void graphics_test_end_screen_probe(void) {
    if (!s_graphics_test_screen_probe_active) {
        return;
    }
    SituationUnloadImage(s_graphics_test_screen_probe);
    s_graphics_test_screen_probe = (SituationImage){0};
    s_graphics_test_screen_probe_active = false;
}

/** Load the current framebuffer once; pair with read_pixel_rgba_probed + end_screen_probe. */
static SituationError graphics_test_begin_screen_probe(void) {
    graphics_test_end_screen_probe();
    SituationError err = SituationLoadImageFromScreen(&s_graphics_test_screen_probe);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    if (!SituationIsImageValid(s_graphics_test_screen_probe)
            || s_graphics_test_screen_probe.width < 1
            || s_graphics_test_screen_probe.height < 1) {
        SituationUnloadImage(s_graphics_test_screen_probe);
        s_graphics_test_screen_probe = (SituationImage){0};
        return SITUATION_ERROR_RESOURCE_INVALID;
    }
    s_graphics_test_screen_probe_active = true;
    return SITUATION_SUCCESS;
}

static SituationError graphics_test_read_pixel_rgba_from_image(
        const SituationImage* screen, int x, int y, uint8_t rgba[4]) {
    if (!screen || !SituationIsImageValid(*screen) || screen->width < 1 || screen->height < 1) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= screen->width) x = screen->width - 1;
    if (y >= screen->height) y = screen->height - 1;
    int idx = (y * screen->width + x) * 4;
    const uint8_t* pixels = (const uint8_t*)screen->data;
    rgba[0] = pixels[idx];
    rgba[1] = pixels[idx + 1];
    rgba[2] = pixels[idx + 2];
    rgba[3] = pixels[idx + 3];
    return SITUATION_SUCCESS;
}

/** Sample from an active screen probe (see begin_screen_probe). */
static SituationError graphics_test_read_pixel_rgba_probed(int x, int y, uint8_t rgba[4]) {
    if (!s_graphics_test_screen_probe_active) {
        return SITUATION_ERROR_RESOURCE_INVALID;
    }
    return graphics_test_read_pixel_rgba_from_image(&s_graphics_test_screen_probe, x, y, rgba);
}

/** Sample pixel at (x, y) in screen coordinates (origin top-left). */
static SituationError graphics_test_read_pixel_rgba(int x, int y, uint8_t rgba[4]) {
    SituationImage screen = {0};
    SituationError err = SituationLoadImageFromScreen(&screen);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    err = graphics_test_read_pixel_rgba_from_image(&screen, x, y, rgba);
    SituationUnloadImage(screen);
    return err;
}

/** Tolerance-based pixel channel comparison (matches test_graphics.c). */
static bool graphics_test_pixel_approx_eq(uint8_t actual, uint8_t expected, uint8_t tolerance) {
    int diff = (int)actual - (int)expected;
    return (diff >= -(int)tolerance && diff <= (int)tolerance);
}

/** Compare float color channel (0–1) to readback byte with tolerance in 8-bit steps. */
static bool graphics_test_pixel_approx_float(uint8_t actual, float expected, uint8_t tolerance) {
    int exp = (int)(expected * 255.0f + 0.5f);
    return graphics_test_pixel_approx_eq(actual, (uint8_t)exp, tolerance);
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

/** AABB center of mesh positions (object space). */
static void graphics_test_compute_mesh_aabb_center(vec3 out_center, SituationModelMesh* mesh) {
    out_center[0] = 0.0f;
    out_center[1] = 0.0f;
    out_center[2] = 0.0f;

    void* vdata = NULL;
    int vcount = 0;
    int vstride = 0;
    SituationGetMeshData(mesh->gpu_mesh, &vdata, &vcount, &vstride, NULL, NULL);
    if (vdata && vcount > 0 && vstride >= (int)(3 * sizeof(float))) {
        const float* verts = (const float*)vdata;
        const int floats_per_vertex = vstride / (int)sizeof(float);
        vec3 minv = {verts[0], verts[1], verts[2]};
        vec3 maxv = {minv[0], minv[1], minv[2]};

        for (int i = 1; i < vcount; ++i) {
            const float* p = verts + (i * floats_per_vertex);
            if (p[0] < minv[0]) minv[0] = p[0];
            if (p[1] < minv[1]) minv[1] = p[1];
            if (p[2] < minv[2]) minv[2] = p[2];
            if (p[0] > maxv[0]) maxv[0] = p[0];
            if (p[1] > maxv[1]) maxv[1] = p[1];
            if (p[2] > maxv[2]) maxv[2] = p[2];
        }

        out_center[0] = (minv[0] + maxv[0]) * 0.5f;
        out_center[1] = (minv[1] + maxv[1]) * 0.5f;
        out_center[2] = (minv[2] + maxv[2]) * 0.5f;
    }
    if (vdata) {
        SIT_FREE(vdata);
    }
}

/** Max axis-aligned half-extent from center (object space). */
static float graphics_test_compute_mesh_max_extent(SituationModelMesh* mesh, const vec3 center) {
    void* vdata = NULL;
    int vcount = 0;
    int vstride = 0;
    SituationGetMeshData(mesh->gpu_mesh, &vdata, &vcount, &vstride, NULL, NULL);
    float max_extent = 0.0f;
    if (vdata && vcount > 0 && vstride >= (int)(3 * sizeof(float))) {
        const float* verts = (const float*)vdata;
        const int floats_per_vertex = vstride / (int)sizeof(float);
        for (int i = 0; i < vcount; ++i) {
            const float* p = verts + (i * floats_per_vertex);
            const float dx = p[0] - center[0];
            const float dy = p[1] - center[1];
            const float dz = p[2] - center[2];
            const float ax = dx < 0.0f ? -dx : dx;
            const float ay = dy < 0.0f ? -dy : dy;
            const float az = dz < 0.0f ? -dz : dz;
            if (ax > max_extent) max_extent = ax;
            if (ay > max_extent) max_extent = ay;
            if (az > max_extent) max_extent = az;
        }
    }
    if (vdata) {
        SIT_FREE(vdata);
    }
    return max_extent;
}

/** Perspective camera on +Z looking at the origin (Y-up world, independent of 2D UI coords). */
static void graphics_test_camera_perspective_default(SituationCameraDesc* cam, float eye_distance) {
    if (!cam) {
        return;
    }
    *cam = (SituationCameraDesc){0};
    cam->eye = (Vector3){{0.0f, 0.0f, eye_distance}};
    cam->target = (Vector3){{0.0f, 0.0f, 0.0f}};
    cam->up = (Vector3){{0.0f, 1.0f, 0.0f}};
    cam->vertical_fov_deg = 45.0f;
    cam->aspect = 0.0f;
    cam->z_near = 0.05f;
    cam->z_far = eye_distance * 4.0f;
    cam->flags = SIT_CAMERA_FLAG_NONE;
}

/** Orthographic camera framing the origin (Y-up world). */
static void graphics_test_camera_ortho_default(SituationCameraDesc* cam, float ortho_height) {
    if (!cam) {
        return;
    }
    *cam = (SituationCameraDesc){0};
    cam->eye = (Vector3){{0.0f, 0.0f, 10.0f}};
    cam->target = (Vector3){{0.0f, 0.0f, 0.0f}};
    cam->up = (Vector3){{0.0f, 1.0f, 0.0f}};
    cam->ortho_height = ortho_height;
    cam->aspect = 0.0f;
    cam->z_near = 0.05f;
    cam->z_far = 100.0f;
    cam->flags = SIT_CAMERA_FLAG_ORTHOGRAPHIC;
}

/** Eye distance so a world-space sphere of world_radius fits vertically in fov_deg (margin >= 1). */
static float graphics_test_camera_distance_for_radius(float world_radius, float fov_deg, float margin) {
    if (world_radius <= 0.0f) {
        world_radius = 1.0f;
    }
    if (margin < 1.0f) {
        margin = 1.0f;
    }
    const float half_fov = glm_rad(fov_deg) * 0.5f;
    const float tan_half = tanf(half_fov);
    if (tan_half < 1e-6f) {
        return 10.0f * margin;
    }
    return (world_radius / tan_half) * margin;
}

/**
 * World model matrix: center mesh AABB at origin, uniform scale to world_radius, rotate Y.
 * Composed explicitly as R * S * T(-center). Do not chain glm_translate + glm_scale_uni on
 * the same matrix — cglm applies scale before the translation offset, which leaves the pivot
 * at (scale - 1) * center and shows up as a vertical shift (bunny ears high, body low).
 */
static void graphics_test_build_world_model_matrix(
    mat4 out,
    const vec3 mesh_center,
    float mesh_half_extent,
    float angle_rad,
    float world_radius)
{
    float scale = 1.0f;
    if (mesh_half_extent > 1e-6f) {
        scale = world_radius / mesh_half_extent;
    }

    mat4 to_center, scaled, rotated, tmp;
    glm_translate_make(to_center, (vec3){-mesh_center[0], -mesh_center[1], -mesh_center[2]});
    glm_scale_make(scaled, (vec3){scale, scale, scale});
    glm_rotate_make(rotated, angle_rad, (vec3){0.0f, 1.0f, 0.0f});
    glm_mat4_mul(scaled, to_center, tmp);
    glm_mat4_mul(rotated, tmp, out);
}

/**
 * SituationUnloadModel destroys textures from the model's internal texture table only.
 * Call this before unload when tests assign SituationLoadTexture to mesh->base_color_texture.
 */
static void graphics_test_destroy_model_external_textures(SituationModel* model) {
    if (!model || model->mesh_count <= 0 || !model->meshes) {
        return;
    }
    for (int i = 0; i < model->mesh_count; ++i) {
        SituationTexture* tex = &model->meshes[i].base_color_texture;
        if (tex->generation != 0) {
            SituationDestroyTexture(tex);
        }
    }
}

/** MVP = SituationCamera view-proj * world model (proper 3D path for harness shaders). */
static void graphics_test_build_model_mvp(
    mat4 out_mvp,
    const SituationCameraDesc* cam,
    const vec3 mesh_center,
    float mesh_half_extent,
    float angle_rad,
    float world_radius)
{
    mat4 model;
    mat4 vp;
    graphics_test_build_world_model_matrix(model, mesh_center, mesh_half_extent, angle_rad, world_radius);
    SituationCameraBuildViewProj(cam, vp);
    glm_mat4_mul(vp, model, out_mvp);
}

/**
 * Count non-black pixels in a horizontal band. Row 0 is the bottom of the framebuffer
 * (OpenGL readback origin); row (height - 1) is the top of the screen.
 */
static int graphics_test_count_non_black_rows(int y0, int y1, uint8_t threshold) {
    SituationImage screen = {0};
    if (SituationLoadImageFromScreen(&screen) != SITUATION_SUCCESS || !SituationIsImageValid(screen)) {
        if (SituationIsImageValid(screen)) {
            SituationUnloadImage(screen);
        }
        return 0;
    }

    if (y0 < 0) {
        y0 = 0;
    }
    if (y1 > screen.height) {
        y1 = screen.height;
    }
    if (y0 >= y1) {
        SituationUnloadImage(screen);
        return 0;
    }

    int count = 0;
    const uint8_t* pixels = (const uint8_t*)screen.data;
    for (int y = y0; y < y1; ++y) {
        for (int x = 0; x < screen.width; ++x) {
            const int idx = (y * screen.width + x) * 4;
            if (pixels[idx] > threshold || pixels[idx + 1] > threshold || pixels[idx + 2] > threshold) {
                count++;
            }
        }
    }

    SituationUnloadImage(screen);
    return count;
}

/** Vertex shader: transform pos/normal, visualize normals in FS (model matrix via push constant). */
static const char* graphics_test_model_normal_vs(void) {
#if defined(SITUATION_USE_VULKAN)
    return
        "#version 450\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(push_constant) uniform PC { mat4 uModel; } pc;\n"
        "layout(location = 0) out vec3 vNormal;\n"
        "void main() {\n"
        "    gl_Position = pc.uModel * vec4(aPos, 1.0);\n"
        "    vNormal = aNormal;\n"
        "}\n";
#else
    return
        "#version 460 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 0) uniform mat4 uModel;\n"
        "layout(location = 0) out vec3 vNormal;\n"
        "void main() {\n"
        "    gl_Position = uModel * vec4(aPos, 1.0);\n"
        "    vNormal = aNormal;\n"
        "}\n";
#endif
}

static const char* graphics_test_model_normal_fs(void) {
#if defined(SITUATION_USE_VULKAN)
    return
        "#version 450\n"
        "layout(location = 0) in vec3 vNormal;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(abs(vNormal), 1.0);\n"
        "}\n";
#else
    return
        "#version 460 core\n"
        "layout(location = 0) in vec3 vNormal;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(abs(vNormal), 1.0);\n"
        "}\n";
#endif
}

/** Descriptor set index for albedo sampler (set 1 on Vulkan, texture unit 0 on OpenGL). */
static uint32_t graphics_test_albedo_texture_set_index(void) {
#if defined(SITUATION_USE_VULKAN)
    return 1u;
#else
    return 0u;
#endif
}

/** Triplanar textured model VS — object-space pos for UV-less meshes (e.g. stanford-bunny.obj). */
static const char* graphics_test_model_triplanar_vs(void) {
#if defined(SITUATION_USE_VULKAN)
    return
        "#version 450\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(push_constant) uniform PC { mat4 uModel; } pc;\n"
        "layout(location = 0) out vec3 vObjPos;\n"
        "layout(location = 1) out vec3 vNormal;\n"
        "void main() {\n"
        "    vObjPos = aPos;\n"
        "    vNormal = aNormal;\n"
        "    gl_Position = pc.uModel * vec4(aPos, 1.0);\n"
        "}\n";
#else
    return
        "#version 460 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(location = 0) uniform mat4 uModel;\n"
        "layout(location = 0) out vec3 vObjPos;\n"
        "layout(location = 1) out vec3 vNormal;\n"
        "void main() {\n"
        "    vObjPos = aPos;\n"
        "    vNormal = aNormal;\n"
        "    gl_Position = uModel * vec4(aPos, 1.0);\n"
        "}\n";
#endif
}

static const char* graphics_test_model_triplanar_fs(void) {
#if defined(SITUATION_USE_VULKAN)
    return
        "#version 450\n"
        "layout(location = 0) in vec3 vObjPos;\n"
        "layout(location = 1) in vec3 vNormal;\n"
        "layout(set = 1, binding = 0) uniform sampler2D uAlbedo;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() {\n"
        "    vec3 n = abs(normalize(vNormal));\n"
        "    n = n / max(n.x + n.y + n.z, 1e-5);\n"
        "    vec3 p = vObjPos * 12.0;\n"
        "    vec4 cx = texture(uAlbedo, p.yz);\n"
        "    vec4 cy = texture(uAlbedo, p.xz);\n"
        "    vec4 cz = texture(uAlbedo, p.xy);\n"
        "    fragColor = cx * n.x + cy * n.y + cz * n.z;\n"
        "}\n";
#else
    return
        "#version 460 core\n"
        "layout(location = 0) in vec3 vObjPos;\n"
        "layout(location = 1) in vec3 vNormal;\n"
        "layout(binding = 0) uniform sampler2D uAlbedo;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() {\n"
        "    vec3 n = abs(normalize(vNormal));\n"
        "    n = n / max(n.x + n.y + n.z, 1e-5);\n"
        "    vec3 p = vObjPos * 12.0;\n"
        "    vec4 cx = texture(uAlbedo, p.yz);\n"
        "    vec4 cy = texture(uAlbedo, p.xz);\n"
        "    vec4 cz = texture(uAlbedo, p.xy);\n"
        "    fragColor = cx * n.x + cy * n.y + cz * n.z;\n"
        "}\n";
#endif
}

/**
 * Draw all valid sub-meshes with a mat4 model matrix.
 * Uses a 64-byte push constant (not SituationDrawModel's 96-byte PBR block) so OpenGL
 * uploads via layout(location=0) uniform mat4 (SIT_UNIFORM_LOC_MODEL_MATRIX).
 */
static SituationError graphics_test_draw_model_meshes(
    SituationCommandBuffer cmd,
    SituationShader shader,
    SituationModel model,
    const mat4 transform)
{
    if (!cmd) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (model.mesh_count <= 0 || !model.meshes) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SituationError err = SituationCmdBindPipeline(cmd, shader);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    err = SituationCmdSetCullMode(cmd, SIT_CULL_NONE);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    for (int i = 0; i < model.mesh_count; ++i) {
        SituationMesh gpu_mesh = model.meshes[i].gpu_mesh;
        if (gpu_mesh.slot_index == 0 && gpu_mesh.generation == 0) {
            continue;
        }
        err = SituationCmdSetPushConstant(cmd, 0, transform, sizeof(mat4));
        if (err != SITUATION_SUCCESS) {
            return err;
        }
        err = SituationCmdDrawMesh(cmd, gpu_mesh);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }
    return SITUATION_SUCCESS;
}

/** Draw sub-meshes with triplanar albedo sampling (binds mesh base_color_texture per draw). */
static SituationError graphics_test_draw_textured_model_meshes(
    SituationCommandBuffer cmd,
    SituationShader shader,
    SituationModel model,
    const mat4 transform)
{
    if (!cmd) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (model.mesh_count <= 0 || !model.meshes) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SituationError err = SituationCmdBindPipeline(cmd, shader);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    err = SituationCmdSetCullMode(cmd, SIT_CULL_NONE);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    err = SituationCmdSetDepthTest(cmd, true, SIT_DEPTH_COMPARE_LESS);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    err = SituationCmdSetDepthWrite(cmd, true);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    const uint32_t albedo_set = graphics_test_albedo_texture_set_index();

    for (int i = 0; i < model.mesh_count; ++i) {
        SituationModelMesh* mesh = &model.meshes[i];
        SituationMesh gpu_mesh = mesh->gpu_mesh;
        if (gpu_mesh.slot_index == 0 && gpu_mesh.generation == 0) {
            continue;
        }
        if (mesh->base_color_texture.generation != 0) {
            err = SituationCmdBindTextureSet(cmd, albedo_set, mesh->base_color_texture);
            if (err != SITUATION_SUCCESS) {
                return err;
            }
        }
        err = SituationCmdSetPushConstant(cmd, 0, transform, sizeof(mat4));
        if (err != SITUATION_SUCCESS) {
            return err;
        }
        err = SituationCmdDrawMesh(cmd, gpu_mesh);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }
    return SITUATION_SUCCESS;
}

/** True if any RGB channel in the screenshot exceeds threshold (default-style >10). */
static bool graphics_test_screen_any_non_black(int threshold) {
    for (int attempt = 0; attempt < 24; ++attempt) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        SituationImage screen = {0};
        SituationError err = SituationLoadImageFromScreen(&screen);
        if (err != SITUATION_SUCCESS || !SituationIsImageValid(screen) || !screen.data) {
            SituationUnloadImage(screen);
            continue;
        }

        bool found = false;
        uint8_t* pixels = (uint8_t*)screen.data;
        int pixel_count = screen.width * screen.height;
        for (int i = 0; i < pixel_count * 4; i += 4) {
            if (pixels[i] > threshold || pixels[i + 1] > threshold || pixels[i + 2] > threshold) {
                found = true;
                break;
            }
        }
        SituationUnloadImage(screen);
        if (found) {
            return true;
        }
    }
    return false;
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

/* ── Phase B′ (plan_handles_ssbo): vertex-pull via mesh BDA ───────────────────
 *
 * Minimal draw recipe (Vulkan):
 *   1. SituationCreateMesh(...) — vertex buffer already has STORAGE | DEVICE_ADDRESS on VK.
 *   2. uint64_t bda = SituationGetMeshVertexBufferAddress(mesh);
 *   3. SituationLoadShaderFromMemory(pull_vs, solid_fs, &shader);
 *   4. BeginRenderPass → SituationCmdBindPipeline → SituationCmdBindMeshPullBuffers(cmd, mesh)
 *      (or SituationCmdSetPushConstant with SituationMeshPullPushConstants @ offset 0)
 *   5. SituationCmdDrawMesh(cmd, mesh) — VS reads verts via buffer_reference + gl_VertexIndex.
 *
 * See sit/gpu/vertex_pull.glslh for canonical struct layouts (Simple / Legacy / PBR).
 */

/** Centered NDC quad mesh (stride 12) for pixel readback tests. */
static SituationError graphics_test_create_center_quad_mesh(SituationMesh* out_mesh) {
    if (!out_mesh) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    memset(out_mesh, 0, sizeof(*out_mesh));
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f
    };
    uint32_t indices[] = {0, 1, 2, 0, 2, 3};
    return SituationCreateMesh(
        vertices, 4, sizeof(float) * 3, indices, 6, out_mesh);
}

#if defined(SITUATION_USE_VULKAN)
static const char* graphics_test_glsl_vs_vertex_pull_simple(void) {
    return
        "#version 450\n"
        "#extension GL_EXT_buffer_reference : require\n"
        "#extension GL_EXT_buffer_reference2 : require\n"
        "#extension GL_EXT_scalar_block_layout : require\n"
        "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
        "layout(location=0) in vec3 aPos;\n"
        "layout(push_constant) uniform PC { uint64_t vertex_address; } pc;\n"
        "layout(buffer_reference, scalar) readonly buffer SitVertexBuffer_Simple { vec3 data[]; };\n"
        "void main() {\n"
        "    SitVertexBuffer_Simple verts = SitVertexBuffer_Simple(pc.vertex_address);\n"
        "    vec3 pos = verts.data[gl_VertexIndex];\n"
        "    gl_Position = vec4(pos + aPos * 0.0, 1.0);\n"
        "}\n";
}

static const char* graphics_test_glsl_cs_mesh_bda_read_vec3(void) {
    return
        "#version 450\n"
        "#extension GL_EXT_buffer_reference : require\n"
        "#extension GL_EXT_buffer_reference2 : require\n"
        "#extension GL_EXT_scalar_block_layout : require\n"
        "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
        "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
        "layout(push_constant) uniform PC { uint64_t vertex_addr; } pc;\n"
        "layout(buffer_reference, scalar) readonly buffer VertexBuf { vec3 data[]; };\n"
        "layout(set = 0, binding = 0, std430) buffer OutBuf { vec3 result; };\n"
        "void main() {\n"
        "    VertexBuf verts = VertexBuf(pc.vertex_addr);\n"
        "    result = verts.data[0];\n"
        "}\n";
}

/** Render mesh with solid-red FS; optional vertex-pull bind via SituationCmdBindMeshPullBuffers. */
static SituationError graphics_test_render_mesh_red(
    SituationShader shader,
    SituationMesh mesh,
    bool bind_pull_buffers)
{
    SituationCommandBuffer cmd = graphics_test_begin_frame();
    if (!cmd) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

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
    err = SituationCmdBindPipeline(cmd, shader);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    if (bind_pull_buffers) {
        err = SituationCmdBindMeshPullBuffers(cmd, mesh);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }
    err = SituationCmdDrawMesh(cmd, mesh);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    err = SituationCmdEndRenderPass(cmd);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    return SituationEndFrame();
}

static bool graphics_test_skip_vertex_pull_vulkan(void) {
    if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)) {
        SIT_ASSERT(true);
        return true;
    }
    return false;
}
#endif /* SITUATION_USE_VULKAN */

#endif /* SIT_GRAPHICS_TEST_HELPERS_H */
