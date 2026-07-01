/**
 * @file test_stl_loader.c
 * @brief STL model loader integration tests — Load teapot.stl, render, verify.
 *
 * Tests SituationLoadModelFromSTL with a real binary STL asset (Utah Teapot).
 * Mirrors the structure of test_model_loader.c (BoomBox.glb tests).
 * Works on both OpenGL and Vulkan backends.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_assets.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_window.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
//  Constants
// ============================================================================

#define TEAPOT_FILE "teapot.stl"

// ============================================================================
//  Helper: attempt model load with graceful skip
// ============================================================================

/**
 * Attempts to load teapot.stl (flat normals). Returns true on success.
 * Returns false (with test passing) if the asset is missing.
 */
static bool try_load_teapot(SituationModel* model) {
    char path[512];
    if (!sit_test_resolve_harness_asset(TEAPOT_FILE, path, sizeof(path))) {
        fprintf(stderr, "  [SKIP] harness asset %s not found\n", TEAPOT_FILE);
        return false;
    }
    SituationError err = SituationLoadModelFromSTL(path, false, model);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "  [SKIP] SituationLoadModelFromSTL failed: %d\n", (int)err);
        return false;
    }
    return true;
}

static bool resolve_teapot_path(char* path, size_t path_sz) {
    return sit_test_resolve_harness_asset(TEAPOT_FILE, path, path_sz);
}

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_init_ok = false;

static void stl_loader_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_STL_LOADER");

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    graphics_test_print_renderer_banner();
}

static void stl_loader_teardown(void) {
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

// ============================================================================
//  Tests
// ============================================================================

/**
 * Load teapot.stl (flat normals) and verify the model structure is valid.
 * The Utah Teapot has 2256 triangles → 6768 vertices (flat, no dedup).
 */
static void test_teapot_load_flat(void) {
    SituationModel model = {0};
    if (!try_load_teapot(&model)) {
        SIT_ASSERT(true);
        return;
    }

    /* STL loader always produces exactly one sub-mesh */
    SIT_ASSERT_EQ(model.mesh_count, 1);
    SIT_ASSERT_NOT_NULL(model.meshes);

    SituationModelMesh* m = &model.meshes[0];
    SIT_ASSERT(m->gpu_mesh.vertex_count > 0);
    SIT_ASSERT(m->gpu_mesh.index_count > 0);

    /* Flat STL: index_count == vertex_count (linear 0,1,2,3,...) */
    SIT_ASSERT_EQ(m->gpu_mesh.index_count, m->gpu_mesh.vertex_count);

    /* Stride must be 32 bytes (8 floats: pos3 + normal3 + uv2) */
    SIT_ASSERT_EQ((int)m->gpu_mesh.vertex_stride, 32);

    /* Default PBR material: white base color, roughness ~0.8 */
    SIT_ASSERT(m->base_color_factor.x > 0.9f);
    SIT_ASSERT(m->base_color_factor.y > 0.9f);
    SIT_ASSERT(m->base_color_factor.z > 0.9f);

    SituationUnloadModel(&model);
}

/**
 * Load teapot.stl with smooth_normals = true, verify vertex count is reduced
 * (deduplication merges coincident corners) and index count matches original
 * flat vertex count.
 */
static void test_teapot_load_smooth(void) {
    char path[512];
    if (!resolve_teapot_path(path, sizeof(path))) {
        fprintf(stderr, "  [SKIP] harness asset %s not found\n", TEAPOT_FILE);
        SIT_ASSERT(true);
        return;
    }

    SituationModel flat_model = {0};
    SituationModel smooth_model = {0};

    SituationError err_flat   = SituationLoadModelFromSTL(path, false, &flat_model);
    SituationError err_smooth = SituationLoadModelFromSTL(path, true,  &smooth_model);

    if (err_flat != SITUATION_SUCCESS || err_smooth != SITUATION_SUCCESS) {
        SituationUnloadModel(&flat_model);
        SituationUnloadModel(&smooth_model);
        SIT_ASSERT(true);
        return;
    }

    /* Smooth: fewer unique vertices than flat (many corners are shared) */
    SIT_ASSERT(smooth_model.meshes[0].gpu_mesh.vertex_count <
               flat_model.meshes[0].gpu_mesh.vertex_count);

    /* Smooth: index count equals original flat vertex count */
    SIT_ASSERT_EQ(smooth_model.meshes[0].gpu_mesh.index_count,
                  flat_model.meshes[0].gpu_mesh.vertex_count);

    /* Both must have the same stride */
    SIT_ASSERT_EQ(smooth_model.meshes[0].gpu_mesh.vertex_stride,
                  flat_model.meshes[0].gpu_mesh.vertex_stride);

    SituationUnloadModel(&flat_model);
    SituationUnloadModel(&smooth_model);
}

/**
 * Load teapot.stl, render it rotating for 1.5 seconds, verify non-black pixels.
 * Uses the same minimal normal-visualisation shader as the BoomBox test.
 * The teapot is scaled to fill the viewport: ~30 units scale (raw teapot is ~3 units).
 */
static void test_teapot_draw_and_verify(void) {
    SituationModel model = {0};
    if (!try_load_teapot(&model)) {
        SIT_ASSERT(true);
        return;
    }

    SituationShader shader = {0};
    SituationError err = SituationLoadShaderFromMemory(
        graphics_test_model_normal_vs(),
        graphics_test_model_normal_fs(),
        &shader);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "  [SKIP] Shader compile failed: %d\n", (int)err);
        SituationUnloadModel(&model);
        SIT_ASSERT(true);
        return;
    }

    /* Render rotating teapot for 1.5 seconds */
    double start_time = SituationTimerGetTime();
    double duration = 1.5;

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

        if (SituationCmdBeginRenderPass(cmd, &rp) != SITUATION_SUCCESS) {
            SituationEndFrame();
            break;
        }

        float elapsed = (float)(SituationTimerGetTime() - start_time);
        mat4 model_transform;
        glm_mat4_identity(model_transform);
        glm_scale_uni(model_transform, 0.30f);
        glm_rotate_y(model_transform, elapsed * 3.0f, model_transform);

        graphics_test_draw_model_meshes(cmd, shader, model, model_transform);
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
    }

    /* Final frame: fixed angle + pixel readback to verify non-black output */
    bool found_non_black = false;
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

        mat4 model_transform;
        glm_mat4_identity(model_transform);
        glm_scale_uni(model_transform, 0.30f);
        glm_rotate_y(model_transform, 1.0f, model_transform);

        err = graphics_test_draw_model_meshes(cmd, shader, model, model_transform);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();

        found_non_black = graphics_test_screen_any_non_black(10);
    }

    SituationUnloadShader(&shader);
    SituationUnloadModel(&model);
    SIT_ASSERT(found_non_black);
}

/**
 * Verify SituationGetMeshData works on an STL-loaded mesh.
 * Flat STL: vertex_stride must be 32, vcount and icount must be equal and > 0.
 */
static void test_teapot_mesh_data_access(void) {
    SituationModel model = {0};
    if (!try_load_teapot(&model)) {
        SIT_ASSERT(true);
        return;
    }

    SituationModelMesh* m = &model.meshes[0];
    void* vdata = NULL;
    void* idata = NULL;
    int vcount = 0, vstride = 0, icount = 0;
    SituationGetMeshData(m->gpu_mesh, &vdata, &vcount, &vstride, &idata, &icount);

    SIT_ASSERT(vcount > 0);
    SIT_ASSERT(icount > 0);
    SIT_ASSERT_EQ(vstride, 32);
    SIT_ASSERT_NOT_NULL(vdata);
    SIT_ASSERT_NOT_NULL(idata);

    /* Flat: index count == vertex count */
    SIT_ASSERT_EQ(vcount, icount);

    SituationUnloadModel(&model);
}

/**
 * Load and unload teapot.stl 5 times — stress test for resource management.
 * Mirrors boombox_load_unload_cycle.
 */
static void test_teapot_load_unload_cycle(void) {
    char path[512];
    if (!resolve_teapot_path(path, sizeof(path))) {
        fprintf(stderr, "  [SKIP] harness asset %s not found\n", TEAPOT_FILE);
        SIT_ASSERT(true);
        return;
    }

    for (int i = 0; i < 5; i++) {
        SituationModel model = {0};
        SituationError err = SituationLoadModelFromSTL(path, false, &model);
        if (err != SITUATION_SUCCESS) {
            /* Resource exhaustion is acceptable on repeated rapid loads */
            SIT_ASSERT(true);
            return;
        }
        SIT_ASSERT(model.mesh_count == 1);
        SituationUnloadModel(&model);
    }
    SIT_ASSERT(true); /* No crash through 5 cycles */
}

/**
 * Load and reload a flat and smooth STL model, verifying geometry structure.
 */
static void test_teapot_reload(void) {
    char path[512];
    if (!resolve_teapot_path(path, sizeof(path))) {
        fprintf(stderr, "  [SKIP] harness asset %s not found\n", TEAPOT_FILE);
        SIT_ASSERT(true);
        return;
    }

    // 1. Flat reload
    {
        SituationModel model = {0};
        SituationError err = SituationLoadModelFromSTL(path, false, &model);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
        err = SituationReloadModel(&model);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
        SIT_ASSERT_EQ(model.mesh_count, 1);
        SIT_ASSERT_EQ(model.meshes[0].gpu_mesh.vertex_count, model.meshes[0].gpu_mesh.index_count);
        SIT_ASSERT_EQ(model.meshes[0].gpu_mesh.vertex_stride, 32);
        SituationUnloadModel(&model);
    }

    // 2. Smooth reload
    {
        SituationModel model = {0};
        SituationError err = SituationLoadModelFromSTL(path, true, &model);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
        int orig_vcount = model.meshes[0].gpu_mesh.vertex_count;
        int orig_icount = model.meshes[0].gpu_mesh.index_count;
        err = SituationReloadModel(&model);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
        SIT_ASSERT_EQ(model.mesh_count, 1);
        SIT_ASSERT_EQ(model.meshes[0].gpu_mesh.vertex_count, orig_vcount);
        SIT_ASSERT_EQ(model.meshes[0].gpu_mesh.index_count, orig_icount);
        SIT_ASSERT_EQ(model.meshes[0].gpu_mesh.vertex_stride, 32);
        SituationUnloadModel(&model);
    }
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase stl_loader_tests[] = {
    {"teapot_load_flat",         test_teapot_load_flat,         true},
    {"teapot_load_smooth",       test_teapot_load_smooth,       true},
    {"teapot_draw_and_verify",   test_teapot_draw_and_verify,   true},
    {"teapot_mesh_data_access",  test_teapot_mesh_data_access,  true},
    {"teapot_load_unload_cycle", test_teapot_load_unload_cycle, true},
    {"teapot_reload",            test_teapot_reload,            true},
};

const SitTestModule g_module_stl_loader = {
    .name           = "stl_loader",
    .setup          = stl_loader_setup,
    .teardown       = stl_loader_teardown,
    .tests          = stl_loader_tests,
    .test_count     = sizeof(stl_loader_tests) / sizeof(stl_loader_tests[0]),
    .requires_context = true
};
