/**
 * @file test_graphics_spirv.c
 * @brief Hardened SPIR-V / descriptor binding tests (OpenGL + Vulkan).
 *
 * Exercises SituationLoadShaderFromSpirvMemory, disk SPIR-V load, SSBO/UBO
 * descriptor sets, and OpenGL-only explicit block binding APIs.
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_graphics_test_helpers.h"
#include "sit_harness_spirv_embed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void spirv_harness_fail_missing_embed(void) {
    fprintf(stderr,
            "[test] Harness SPIR-V embed is empty. Run: compile_harness_shaders.bat\n");
    SIT_ASSERT(false);
}

static bool spirv_harness_embed_ready(void) {
    return sit_harness_passthrough_vs_spv_len > 0u
        && sit_harness_dual_ssbo_fs_spv_len > 0u
        && sit_harness_ubo_ssbo_fs_spv_len > 0u;
}

static unsigned char* spirv_test_read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        free(buf);
        return NULL;
    }
    if (out_len) {
        *out_len = (size_t)sz;
    }
    return buf;
}

static bool spirv_test_path_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

static const char* spirv_test_first_existing_path(const char* const* paths) {
    for (int i = 0; paths[i]; i++) {
        if (spirv_test_path_exists(paths[i])) {
            return paths[i];
        }
    }
    return NULL;
}

static int spirv_test_resolve_mesh_spirv_paths(const char** vs_out, const char** fs_out) {
    static const char* vs_paths[] = {
        "../../tests/harness/spirv_out/harness_passthrough_vk.vs.spv",
        "../../tests/harness/spirv_out/harness_passthrough.vs.spv",
        "tests/harness/spirv_out/harness_passthrough_vk.vs.spv",
        "tests/harness/spirv_out/harness_passthrough.vs.spv",
        "../tests/harness/spirv_out/harness_passthrough_vk.vs.spv",
        "../tests/harness/spirv_out/harness_passthrough.vs.spv",
        NULL
    };
    static const char* fs_paths[] = {
        "../../tests/harness/spirv_out/harness_solid_red_vk.fs.spv",
        "../../tests/harness/spirv_out/harness_solid_red_gl.fs.spv",
        "tests/harness/spirv_out/harness_solid_red_vk.fs.spv",
        "tests/harness/spirv_out/harness_solid_red_gl.fs.spv",
        "../tests/harness/spirv_out/harness_solid_red_vk.fs.spv",
        "../tests/harness/spirv_out/harness_solid_red_gl.fs.spv",
        NULL
    };
    *vs_out = spirv_test_first_existing_path(vs_paths);
    *fs_out = spirv_test_first_existing_path(fs_paths);
    return (*vs_out != NULL && *fs_out != NULL);
}

static SituationError spirv_harness_load_dual_ssbo_shader(SituationShader* out_shader) {
#if defined(SITUATION_USE_VULKAN)
    return SituationLoadShaderFromSpirvMemoryEx(
        sit_harness_passthrough_vs_spv,
        sit_harness_passthrough_vs_spv_len,
        sit_harness_dual_ssbo_fs_spv,
        sit_harness_dual_ssbo_fs_spv_len,
        SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO,
        out_shader);
#else
    return SituationLoadShaderFromSpirvMemory(
        sit_harness_passthrough_vs_spv,
        sit_harness_passthrough_vs_spv_len,
        sit_harness_dual_ssbo_fs_spv,
        sit_harness_dual_ssbo_fs_spv_len,
        out_shader);
#endif
}

static SituationError spirv_harness_load_ubo_ssbo_shader(SituationShader* out_shader) {
#if defined(SITUATION_USE_VULKAN)
    return SituationLoadShaderFromSpirvMemoryEx(
        sit_harness_passthrough_vs_spv,
        sit_harness_passthrough_vs_spv_len,
        sit_harness_ubo_ssbo_fs_spv,
        sit_harness_ubo_ssbo_fs_spv_len,
        SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO,
        out_shader);
#else
    return SituationLoadShaderFromSpirvMemory(
        sit_harness_passthrough_vs_spv,
        sit_harness_passthrough_vs_spv_len,
        sit_harness_ubo_ssbo_fs_spv,
        sit_harness_ubo_ssbo_fs_spv_len,
        out_shader);
#endif
}

void test_async_shader_spirv_memory_vulkan(void) {
#if !defined(SITUATION_USE_VULKAN)
    SIT_ASSERT(true);
    return;
#endif

    const char* vs_path = NULL;
    const char* fs_path = NULL;
    if (!spirv_test_resolve_mesh_spirv_paths(&vs_path, &fs_path)) {
        fprintf(stderr,
                "[test] Missing harness_solid_red_*.fs.spv — run compile_harness_shaders.bat\n");
        SIT_ASSERT(false);
    }

    size_t vs_len = 0;
    size_t fs_len = 0;
    unsigned char* vs_data = spirv_test_read_file(vs_path, &vs_len);
    unsigned char* fs_data = spirv_test_read_file(fs_path, &fs_len);
    SIT_ASSERT_NOT_NULL(vs_data);
    SIT_ASSERT_NOT_NULL(fs_data);
    SIT_ASSERT((vs_len & 3u) == 0);
    SIT_ASSERT((fs_len & 3u) == 0);

    SituationShader shader = {0};
    SituationError err = SituationBeginLoadShaderFromSpirvMemory(
        vs_data, vs_len, fs_data, fs_len, &shader);
    free(vs_data);
    free(fs_data);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationPollShaderLoad(shader);
    SIT_ASSERT(err == SITUATION_SUCCESS
               || err == SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS);
    SIT_ASSERT_EQ(graphics_test_async_poll_shader_ready(shader, 600), SITUATION_SUCCESS);

    SituationMesh mesh = graphics_test_fullscreen_mesh();
    SituationCommandBuffer cmd = graphics_test_begin_frame();
    SIT_ASSERT_NOT_NULL(cmd);
    SIT_ASSERT_EQ(graphics_test_clear_and_draw(cmd, shader, mesh), SITUATION_SUCCESS);

    uint8_t rgba[4] = {0};
    SIT_ASSERT_EQ(graphics_test_read_center_pixel_rgba(rgba), SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_pixel_approx_eq(rgba[0], 255, 16));

    SituationUnloadShader(&shader);
}

void test_spirv_memory_invalid_params(void) {
    SituationShader shader = {0};
    const unsigned char dummy[4] = {0, 0, 0, 0};

    SIT_ASSERT_EQ(
        SituationLoadShaderFromSpirvMemory(NULL, 4, dummy, 4, &shader),
        SITUATION_ERROR_INVALID_PARAM);
    SIT_ASSERT_EQ(
        SituationLoadShaderFromSpirvMemory(dummy, 4, NULL, 4, &shader),
        SITUATION_ERROR_INVALID_PARAM);
#if defined(SITUATION_USE_OPENGL)
    SIT_ASSERT_EQ(
        SituationLoadShaderFromSpirvMemory(dummy, 3, dummy, 4, &shader),
        SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY);
    SIT_ASSERT_EQ(
        SituationLoadShaderFromSpirvMemory(dummy, 4, dummy, 3, &shader),
        SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY);
#elif defined(SITUATION_USE_VULKAN)
    SIT_ASSERT_EQ(
        SituationLoadShaderFromSpirvMemory(dummy, 3, dummy, 4, &shader),
        SITUATION_ERROR_VULKAN_SPIRV_INVALID);
    SIT_ASSERT_EQ(
        SituationLoadShaderFromSpirvMemory(dummy, 4, dummy, 3, &shader),
        SITUATION_ERROR_VULKAN_SPIRV_INVALID);
#endif
    SIT_ASSERT_EQ(
        SituationLoadShaderFromSpirvMemory(dummy, 0, dummy, 4, &shader),
        SITUATION_ERROR_INVALID_PARAM);
}

void test_spirv_error_code_reporting(void) {
    const char* label = SituationErrorToString(SITUATION_ERROR_OPENGL_SPIRV_FS_SPECIALIZE_FAILED);
    SIT_ASSERT_NOT_NULL(label);
    SIT_ASSERT(strstr(label, "SPIR-V") != NULL);

    SituationShader shader = {0};
    const unsigned char dummy[4] = {0, 0, 0, 0};
#if defined(SITUATION_USE_OPENGL)
    SituationError err = SituationLoadShaderFromSpirvMemory(dummy, 3, dummy, 4, &shader);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY);
    SIT_ASSERT_EQ(SituationGetLastErrorCode(), SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY);
    char* msg = NULL;
    SIT_ASSERT_EQ(SituationGetLastErrorMsg(&msg), SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(msg);
    SIT_ASSERT(strstr(msg, "vertex") != NULL || strstr(msg, "multiple of 4") != NULL);
    SituationFreeString(msg);
#elif defined(SITUATION_USE_VULKAN)
    SituationError err = SituationLoadShaderFromSpirvMemory(dummy, 3, dummy, 4, &shader);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_VULKAN_SPIRV_INVALID);
    SIT_ASSERT_EQ(SituationGetLastErrorCode(), SITUATION_ERROR_VULKAN_SPIRV_INVALID);
#endif
}

void test_spirv_memory_dual_ssbo_readback(void) {
    if (!spirv_harness_embed_ready()) {
        spirv_harness_fail_missing_embed();
    }

    const uint32_t tag_a = 77u;
    const uint32_t tag_b = 203u;
    SituationBuffer ssbo_a = {0};
    SituationBuffer ssbo_b = {0};

    SituationError err = graphics_test_create_ssbo(sizeof(tag_a), &tag_a, &ssbo_a);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = graphics_test_create_ssbo(sizeof(tag_b), &tag_b, &ssbo_b);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationShader shader = {0};
    err = spirv_harness_load_dual_ssbo_shader(&shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationMesh mesh = graphics_test_fullscreen_mesh();
    SIT_ASSERT(mesh.slot_index != 0 || mesh.generation != 0);

    SituationCommandBuffer cmd = graphics_test_begin_frame();
    SIT_ASSERT_NOT_NULL(cmd);
    SituationCmdBindPipeline(cmd, shader);
    SIT_ASSERT_EQ(SituationCmdBindDescriptorSet(cmd, 0, ssbo_a), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdBindDescriptorSet(cmd, 1, ssbo_b), SITUATION_SUCCESS);

    err = graphics_test_clear_and_draw(cmd, shader, mesh);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    uint8_t rgba[4] = {0};
    err = graphics_test_read_center_pixel_rgba(rgba);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_pixel_approx_eq(rgba[0], (uint8_t)tag_a, 10));
    SIT_ASSERT(graphics_test_pixel_approx_eq(rgba[1], (uint8_t)tag_b, 10));
    SIT_ASSERT(graphics_test_pixel_approx_eq(rgba[2], 0, 10));

    SituationUnloadShader(&shader);
    SituationDestroyBuffer(&ssbo_a);
    SituationDestroyBuffer(&ssbo_b);
}

#if defined(SITUATION_USE_OPENGL)
void test_spirv_memory_dual_ssbo_explicit_bind(void) {
    if (!spirv_harness_embed_ready()) {
        spirv_harness_fail_missing_embed();
    }

    const uint32_t tag_a = 77u;
    const uint32_t tag_b = 203u;
    SituationBuffer ssbo_a = {0};
    SituationBuffer ssbo_b = {0};
    SIT_ASSERT_EQ(graphics_test_create_ssbo(sizeof(tag_a), &tag_a, &ssbo_a), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(graphics_test_create_ssbo(sizeof(tag_b), &tag_b, &ssbo_b), SITUATION_SUCCESS);

    SituationShader shader = {0};
    SIT_ASSERT_EQ(spirv_harness_load_dual_ssbo_shader(&shader), SITUATION_SUCCESS);
    /* Host API used by Demon Hunt after SPIR-V load (names may be absent in reflection). */
    SIT_ASSERT_EQ(SituationBindShaderStorageBlock(shader, "BlockA", 0), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationBindShaderStorageBlock(shader, "BlockB", 1), SITUATION_SUCCESS);

    SituationUnloadShader(&shader);
    SituationDestroyBuffer(&ssbo_a);
    SituationDestroyBuffer(&ssbo_b);
}
#endif

void test_spirv_memory_ubo_ssbo_readback(void) {
    if (!spirv_harness_embed_ready()) {
        spirv_harness_fail_missing_embed();
    }

    const uint32_t tag_b = 203u;
    float ubo_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};

    SituationBuffer ubo = {0};
    SituationBuffer ssbo = {0};
    SIT_ASSERT_EQ(
        SituationCreateBuffer(
            sizeof(ubo_color),
            ubo_color,
            SITUATION_BUFFER_USAGE_UNIFORM_BUFFER
                | SITUATION_BUFFER_USAGE_TRANSFER_DST,
            &ubo),
        SITUATION_SUCCESS);
    SIT_ASSERT_EQ(graphics_test_create_ssbo(sizeof(tag_b), &tag_b, &ssbo), SITUATION_SUCCESS);

    SituationShader shader = {0};
    SIT_ASSERT_EQ(spirv_harness_load_ubo_ssbo_shader(&shader), SITUATION_SUCCESS);

#if defined(SITUATION_USE_OPENGL)
    SIT_ASSERT_EQ(SituationBindUniformBlock(shader, "Frame", 0), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationBindShaderStorageBlock(shader, "TagBlock", 1), SITUATION_SUCCESS);
#endif

    SituationMesh mesh = graphics_test_fullscreen_mesh();
    SituationCommandBuffer cmd = graphics_test_begin_frame();
    SIT_ASSERT_NOT_NULL(cmd);
    SituationCmdBindPipeline(cmd, shader);
    SIT_ASSERT_EQ(SituationCmdBindDescriptorSet(cmd, 0, ubo), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationCmdBindDescriptorSet(cmd, 1, ssbo), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(graphics_test_clear_and_draw(cmd, shader, mesh), SITUATION_SUCCESS);

    uint8_t rgba[4] = {0};
    SIT_ASSERT_EQ(graphics_test_read_center_pixel_rgba(rgba), SITUATION_SUCCESS);
    SIT_ASSERT(graphics_test_pixel_approx_eq(rgba[0], (uint8_t)tag_b, 10));
    SIT_ASSERT(graphics_test_pixel_approx_eq(rgba[1], 255, 10));

    SituationUnloadShader(&shader);
    SituationDestroyBuffer(&ubo);
    SituationDestroyBuffer(&ssbo);
}

void test_spirv_memory_post_link_resources(void) {
    if (!spirv_harness_embed_ready()) {
        spirv_harness_fail_missing_embed();
    }

    SituationShader shader = {0};
    SIT_ASSERT_EQ(spirv_harness_load_dual_ssbo_shader(&shader), SITUATION_SUCCESS);

    const uint32_t tag = 55u;
    SituationBuffer ssbo = {0};
    SIT_ASSERT_EQ(graphics_test_create_ssbo(sizeof(tag), &tag, &ssbo), SITUATION_SUCCESS);

    SituationMesh mesh = graphics_test_fullscreen_mesh();
    SIT_ASSERT(mesh.slot_index != 0 || mesh.generation != 0);

    SituationUnloadShader(&shader);
    SituationDestroyBuffer(&ssbo);
    SituationDestroyMesh(&mesh);
}

void test_spirv_disk_roundtrip(void) {
    if (!spirv_harness_embed_ready()) {
        spirv_harness_fail_missing_embed();
    }

    static const char* vs_paths[] = {
        "../../tests/harness/spirv_out/harness_passthrough.vs.spv",
        "../../tests/harness/spirv_out/harness_passthrough_vk.vs.spv",
        "tests/harness/spirv_out/harness_passthrough.vs.spv",
        "../tests/harness/spirv_out/harness_passthrough.vs.spv",
        "tests/harness/spirv_out/harness_passthrough_vk.vs.spv",
        "../tests/harness/spirv_out/harness_passthrough_vk.vs.spv",
        NULL
    };
    static const char* fs_paths[] = {
#if defined(SITUATION_USE_OPENGL)
        "../../tests/harness/spirv_out/harness_dual_ssbo_gl.fs.spv",
        "tests/harness/spirv_out/harness_dual_ssbo_gl.fs.spv",
        "../tests/harness/spirv_out/harness_dual_ssbo_gl.fs.spv",
#else
        "../../tests/harness/spirv_out/harness_dual_ssbo_vk.fs.spv",
        "tests/harness/spirv_out/harness_dual_ssbo_vk.fs.spv",
        "../tests/harness/spirv_out/harness_dual_ssbo_vk.fs.spv",
#endif
        NULL
    };

    const char* vs_path = spirv_test_first_existing_path(vs_paths);
    const char* fs_path = spirv_test_first_existing_path(fs_paths);

    if (!vs_path || !fs_path) {
        fprintf(stderr, "[test] SPIR-V disk files missing under tests/harness/spirv_out/\n");
        SIT_ASSERT(false);
    }

    SituationShader shader = {0};
    SituationError err = SituationLoadShaderFromSpirv(vs_path, fs_path, &shader);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationUnloadShader(&shader);
}

#if defined(SITUATION_USE_VULKAN)
void test_demon_hunt_sky_spirv_vk_begin_poll(void) {
    static const char* vs_paths[] = {
        "examples/demon_hunt_sky.vk.vs.spv",
        "demon_hunt_sky.vk.vs.spv",
        "build/examples/demon_hunt_sky.vk.vs.spv",
        NULL
    };
    static const char* fs_paths[] = {
        "examples/demon_hunt_sky.vk.fs.spv",
        "demon_hunt_sky.vk.fs.spv",
        "build/examples/demon_hunt_sky.vk.fs.spv",
        NULL
    };

    size_t vs_len = 0;
    size_t fs_len = 0;
    unsigned char* vs_data = NULL;
    unsigned char* fs_data = NULL;
    for (int i = 0; vs_paths[i] && !vs_data; i++) {
        vs_data = spirv_test_read_file(vs_paths[i], &vs_len);
    }
    for (int i = 0; fs_paths[i] && !fs_data; i++) {
        fs_data = spirv_test_read_file(fs_paths[i], &fs_len);
    }

    if (!vs_data || !fs_data) {
        fprintf(stderr,
                "[test] Skip demon_hunt_sky_spirv_vk_begin_poll: run compile_demon_hunt_shaders.bat first\n");
        free(vs_data);
        free(fs_data);
        SIT_ASSERT(true);
        return;
    }

    SituationShader shader = {0};
    SituationError err = SituationBeginLoadShaderFromSpirvMemoryEx(
        vs_data, vs_len, fs_data, fs_len, SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO, &shader);
    free(vs_data);
    free(fs_data);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(shader.generation != 0);

    err = SituationPollShaderLoad(shader);
    SIT_ASSERT(err == SITUATION_SUCCESS || err == SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS);
    SIT_ASSERT_EQ(graphics_test_async_poll_shader_ready(shader, 600), SITUATION_SUCCESS);

    SituationUnloadShader(&shader);
}
#endif /* SITUATION_USE_VULKAN */

#if defined(SITUATION_USE_OPENGL)
void test_spirv_bind_api_invalid_shader(void) {
    SituationShader bad = {0};
    SIT_ASSERT_EQ(
        SituationBindShaderStorageBlock(bad, "BlockA", 0),
        SITUATION_ERROR_INVALID_PARAM);
    SIT_ASSERT_EQ(
        SituationBindUniformBlock(bad, "Frame", 0),
        SITUATION_ERROR_INVALID_PARAM);
}
#endif
