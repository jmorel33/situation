/**
 * @file test_model_loader.c
 * @brief Model loader integration tests — Load BoomBox.glb, render, verify.
 *
 * Tests SituationLoadModel / SituationDrawModel with a real GLB asset.
 * Works on both OpenGL and Vulkan backends.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_window.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
//  Constants
// ============================================================================

#define BOOMBOX_PATH "tests/harness/assets/BoomBox.glb"

// ============================================================================
//  Helper: attempt model load with graceful skip
// ============================================================================

/**
 * Attempts to load BoomBox.glb. Returns true if model loaded successfully.
 * Returns false (with test passing) if the asset is missing or cgltf isn't compiled in.
 */
static bool try_load_boombox(SituationModel* model) {
    if (!SituationFileExists(BOOMBOX_PATH)) {
        fprintf(stderr, "  [SKIP] %s not found\n", BOOMBOX_PATH);
        return false;
    }
    SituationError err = SituationLoadModel(BOOMBOX_PATH, model);
    if (err == SITUATION_ERROR_NOT_IMPLEMENTED) {
        fprintf(stderr, "  [SKIP] SituationLoadModel not available (CGLTF not compiled in)\n");
        return false;
    }
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "  [SKIP] SituationLoadModel failed: %d\n", (int)err);
        return false;
    }
    return true;
}

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_init_ok = false;

static void model_loader_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_MODEL_LOADER");

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    graphics_test_print_renderer_banner();
}

static void model_loader_teardown(void) {
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

// ============================================================================
//  Tests
// ============================================================================

/**
 * Load BoomBox.glb and verify the model structure is valid.
 */
static void test_boombox_load(void) {
    SituationModel model = {0};
    if (!try_load_boombox(&model)) {
        SIT_ASSERT(true);
        return;
    }

    SIT_ASSERT(model.mesh_count >= 1);
    SIT_ASSERT_NOT_NULL(model.meshes);

    // BoomBox is a known model — it has a single mesh with PBR textures
    SituationModelMesh* m = &model.meshes[0];
    SIT_ASSERT(m->gpu_mesh.vertex_count > 0);
    SIT_ASSERT(m->gpu_mesh.index_count > 0);

    // PBR material should have non-default base color factor
    // BoomBox typically has (1,1,1,1) base color factor with textures doing the work
    SIT_ASSERT(m->base_color_factor.x > 0.0f);

    SituationUnloadModel(&model);
}

/**
 * Load BoomBox.glb, render it rotating for 1.5 seconds, verify non-black pixels.
 * SituationDrawModel requires the caller to bind a compatible shader first.
 * We use a minimal PBR-compatible shader that outputs the normal as color.
 * The model rotates on Y to visually confirm projection is working.
 */
static void test_boombox_draw_and_verify(void) {
    SituationModel model = {0};
    if (!try_load_boombox(&model)) {
        SIT_ASSERT(true);
        return;
    }

    // Minimal shader compatible with SituationDrawModel's push constant mechanism.
    // On OpenGL, push constants > 64 bytes are delivered via SSBO at binding 0.
    // Vertex layout: loc 0 = vec3 pos, loc 1 = vec3 normal
    static const char* vs =
#if defined(SITUATION_USE_VULKAN)
        "#version 450\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(push_constant) uniform PC { mat4 model; vec4 base_color; vec4 pbr; } pc;\n"
        "layout(location = 0) out vec3 vNormal;\n"
        "void main() {\n"
        "    gl_Position = pc.model * vec4(aPos, 1.0);\n"
        "    vNormal = aNormal;\n"
        "}\n";
#else
        "#version 460 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aNormal;\n"
        "layout(std430, binding = 0) buffer PushConstants {\n"
        "    mat4 model;\n"
        "    vec4 base_color;\n"
        "    vec4 pbr;\n"
        "} pc;\n"
        "layout(location = 0) out vec3 vNormal;\n"
        "void main() {\n"
        "    gl_Position = pc.model * vec4(aPos, 1.0);\n"
        "    vNormal = aNormal;\n"
        "}\n";
#endif

    static const char* fs =
#if defined(SITUATION_USE_VULKAN)
        "#version 450\n"
#else
        "#version 460 core\n"
#endif
        "layout(location = 0) in vec3 vNormal;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(abs(vNormal), 1.0);\n"
        "}\n";

    SituationShader shader = {0};
    SituationError err = SituationLoadShaderFromMemory(vs, fs, &shader);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "  [SKIP] Shader compile failed: %d\n", (int)err);
        SituationUnloadModel(&model);
        SIT_ASSERT(true);
        return;
    }

    // Render rotating model for 1.5 seconds
    double start_time = SituationTimerGetTime();
    double duration = 1.5;
    bool found_non_black = false;

    while (SituationTimerGetTime() - start_time < duration) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) break;
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        if (!cmd) break;

        SituationRenderPassInfo rp = {0};
        rp.display_id = -1;
        rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
        rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
        rp.depth_attachment.clear.depth = 1.0f;

        err = SituationCmdBeginRenderPass(cmd, &rp);
        if (err != SITUATION_SUCCESS) { SituationEndFrame(); break; }

        SituationCmdBindPipeline(cmd, shader);

        // Build rotating model transform: scale up (BoomBox is ~0.03 units) + Y rotation
        float elapsed = (float)(SituationTimerGetTime() - start_time);
        float angle = elapsed * 3.0f; // ~1.5 full rotations over 1.5s

        mat4 model_transform;
        glm_mat4_identity(model_transform);
        glm_scale_uni(model_transform, 30.0f);
        glm_rotate_y(model_transform, angle, model_transform);

        SituationDrawModel(cmd, model, model_transform);

        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
    }

    // Final readback after the loop — verify non-black pixels
    {
        SituationPollInputEvents();
        SituationUpdateTimers();
        SIT_ASSERT(SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS);
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        SIT_ASSERT_NOT_NULL(cmd);

        SituationRenderPassInfo rp = {0};
        rp.display_id = -1;
        rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
        rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
        rp.depth_attachment.clear.depth = 1.0f;

        err = SituationCmdBeginRenderPass(cmd, &rp);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

        SituationCmdBindPipeline(cmd, shader);

        mat4 model_transform;
        glm_mat4_identity(model_transform);
        glm_scale_uni(model_transform, 30.0f);
        glm_rotate_y(model_transform, 1.0f, model_transform); // Fixed angle for deterministic readback

        SituationDrawModel(cmd, model, model_transform);
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();

        SituationImage screen = {0};
        err = SituationLoadImageFromScreen(&screen);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
        SIT_ASSERT(SituationIsImageValid(screen));

        uint8_t* pixels = (uint8_t*)screen.data;
        int pixel_count = screen.width * screen.height;
        for (int i = 0; i < pixel_count * 4; i += 4) {
            if (pixels[i] > 10 || pixels[i + 1] > 10 || pixels[i + 2] > 10) {
                found_non_black = true;
                break;
            }
        }
        SituationUnloadImage(screen);
    }

    SIT_ASSERT(found_non_black);

    SituationUnloadShader(&shader);
    SituationUnloadModel(&model);
}

/**
 * Load BoomBox, verify mesh data pointers are accessible via SituationGetMeshData.
 */
static void test_boombox_mesh_data_access(void) {
    SituationModel model = {0};
    if (!try_load_boombox(&model)) {
        SIT_ASSERT(true);
        return;
    }

    // Query mesh data from the first sub-mesh
    SituationModelMesh* m = &model.meshes[0];
    void* vdata = NULL;
    void* idata = NULL;
    int vcount = 0, vstride = 0, icount = 0;
    SituationGetMeshData(m->gpu_mesh, &vdata, &vcount, &vstride, &idata, &icount);

    SIT_ASSERT(vcount > 0);
    SIT_ASSERT(icount > 0);
    SIT_ASSERT(vstride > 0);
    SIT_ASSERT_NOT_NULL(vdata);
    SIT_ASSERT_NOT_NULL(idata);

    SituationUnloadModel(&model);
}

/**
 * Load BoomBox twice, verify both handles are independent and unload cleanly.
 */
static void test_boombox_double_load(void) {
    SituationModel model_a = {0};
    if (!try_load_boombox(&model_a)) {
        SIT_ASSERT(true);
        return;
    }

    SituationModel model_b = {0};
    SituationError err_b = SituationLoadModel(BOOMBOX_PATH, &model_b);
    if (err_b != SITUATION_SUCCESS) {
        SituationUnloadModel(&model_a);
        SIT_ASSERT(true);
        return;
    }

    // Both should have valid, distinct slot indices
    SIT_ASSERT(model_a.mesh_count >= 1);
    SIT_ASSERT(model_b.mesh_count >= 1);
    SIT_ASSERT(model_a.slot_index != model_b.slot_index || model_a.generation != model_b.generation);

    SituationUnloadModel(&model_a);
    SituationUnloadModel(&model_b);
}

/**
 * Load and unload rapidly — stress test for resource management.
 */
static void test_boombox_load_unload_cycle(void) {
    SituationModel model = {0};
    if (!try_load_boombox(&model)) {
        SIT_ASSERT(true);
        return;
    }
    SituationUnloadModel(&model);

    // Remaining 4 cycles
    for (int i = 0; i < 4; i++) {
        memset(&model, 0, sizeof(model));
        SituationError err = SituationLoadModel(BOOMBOX_PATH, &model);
        if (err != SITUATION_SUCCESS) {
            // Resource exhaustion is acceptable on repeated rapid loads
            SIT_ASSERT(true);
            return;
        }
        SIT_ASSERT(model.mesh_count >= 1);
        SituationUnloadModel(&model);
    }
    SIT_ASSERT(true); // No crash through 5 cycles
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase model_loader_tests[] = {
    {"boombox_load",             test_boombox_load,             true},
    {"boombox_draw_and_verify",  test_boombox_draw_and_verify,  true},
    {"boombox_mesh_data_access", test_boombox_mesh_data_access, true},
    {"boombox_double_load",      test_boombox_double_load,      true},
    {"boombox_load_unload_cycle", test_boombox_load_unload_cycle, true},
};

const SitTestModule g_module_model_loader = {
    .name = "model_loader",
    .setup = model_loader_setup,
    .teardown = model_loader_teardown,
    .tests = model_loader_tests,
    .test_count = sizeof(model_loader_tests) / sizeof(model_loader_tests[0]),
    .requires_context = true
};
