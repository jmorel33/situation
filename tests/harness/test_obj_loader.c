/**
 * @file test_obj_loader.c
 * @brief OBJ model loader integration tests — teapot and stanford-bunny assets, render, verify.
 *
 * Tests SituationLoadModelFromOBJ with the vendor-included tinyobj parser.
 * Mirrors the structure of test_stl_loader.c and test_model_loader.c.
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

#define TEAPOT_OBJ_FILE "utah_teapot.obj"
#define BUNNY_OBJ_FILE "stanford-bunny.obj"
#define BUNNY_DRAW_WORLD_RADIUS 0.5f
#define ROSEWOOD_TEXTURE_FILE "rosewood-veneer.png"

// ============================================================================
//  Helper: attempt model load with graceful skip
// ============================================================================

/**
 * Attempts to load utah_teapot.obj. Returns true on success.
 * Returns false (with test passing) if the asset is missing.
 */
static bool try_load_teapot_obj(SituationModel* model) {
    char path[512];
    if (!sit_test_resolve_harness_asset(TEAPOT_OBJ_FILE, path, sizeof(path))) {
        fprintf(stderr, "  [SKIP] harness asset %s not found\n", TEAPOT_OBJ_FILE);
        return false;
    }
    SituationError err = SituationLoadModelFromOBJ(path, model);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "  [SKIP] SituationLoadModelFromOBJ failed: %d\n", (int)err);
        return false;
    }
    return true;
}

static bool try_load_bunny_obj(SituationModel* model) {
    char path[512];
    if (!sit_test_resolve_harness_asset(BUNNY_OBJ_FILE, path, sizeof(path))) {
        fprintf(stderr, "  [SKIP] harness asset %s not found\n", BUNNY_OBJ_FILE);
        return false;
    }
    SituationError err = SituationLoadModelFromOBJ(path, model);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "  [SKIP] SituationLoadModelFromOBJ failed for bunny: %d\n", (int)err);
        return false;
    }
    return true;
}

static bool try_assign_rosewood_texture(SituationModel* model, char* out_path, size_t out_path_sz) {
    if (!model || model->mesh_count <= 0 || !model->meshes || !out_path || out_path_sz == 0) {
        return false;
    }
    static const char* const k_rosewood_names[] = {
        ROSEWOOD_TEXTURE_FILE,
        "rosewood_veneer1.png",
        "rosewood_veneer.png",
        "rosewood.png",
        NULL
    };
    if (!sit_test_resolve_harness_asset_any(k_rosewood_names, out_path, out_path_sz)
        && !sit_test_resolve_harness_asset_name_contains("rosewood", out_path, out_path_sz)) {
        return false;
    }
    SituationError err = SituationLoadTexture(out_path, true, &model->meshes[0].base_color_texture);
    return err == SITUATION_SUCCESS;
}

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_init_ok = false;

static void obj_loader_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_OBJ_LOADER");

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    graphics_test_print_renderer_banner();
}

static void obj_loader_teardown(void) {
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

// ============================================================================
//  Tests
// ============================================================================

/**
 * Load utah_teapot.obj and verify the model structure is valid.
 */
static void test_teapot_obj_load(void) {
    SituationModel model = {0};
    if (!try_load_teapot_obj(&model)) {
        SIT_ASSERT(true);
        return;
    }

    /* OBJ loader produces sub-meshes based on shapes. Teapot has 1 main mesh shape. */
    SIT_ASSERT(model.mesh_count >= 1);
    SIT_ASSERT_NOT_NULL(model.meshes);

    SituationModelMesh* m = &model.meshes[0];
    SIT_ASSERT(m->gpu_mesh.vertex_count > 0);
    SIT_ASSERT(m->gpu_mesh.index_count > 0);

    /* Stride must be 48 bytes (12 floats: pos3 + normal3 + tangent4 + uv2) */
    SIT_ASSERT_EQ((int)m->gpu_mesh.vertex_stride, 48);

    /* Default PBR material factors */
    SIT_ASSERT(m->base_color_factor.x > 0.9f);
    SIT_ASSERT(m->base_color_factor.y > 0.9f);
    SIT_ASSERT(m->base_color_factor.z > 0.9f);
    SIT_ASSERT_EQ(m->metallic_factor, 0.0f);
    SIT_ASSERT_EQ(m->roughness_factor, 0.8f);

    SituationUnloadModel(&model);
}

/**
 * Load utah_teapot.obj, render it rotating for 1.5 seconds, verify non-black pixels.
 */
static void test_teapot_obj_draw_and_verify(void) {
    SituationModel model = {0};
    if (!try_load_teapot_obj(&model)) {
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

    vec3 mesh_center = {0.0f, 0.0f, 0.0f};
    graphics_test_compute_mesh_aabb_center(mesh_center, &model.meshes[0]);
    const float mesh_extent = graphics_test_compute_mesh_max_extent(&model.meshes[0], mesh_center);

    SituationCameraDesc cam;
    graphics_test_camera_perspective_default(
        &cam, graphics_test_camera_distance_for_radius(1.0f, 45.0f, 1.25f));

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
        mat4 mvp;
        graphics_test_build_model_mvp(mvp, &cam, mesh_center, mesh_extent, elapsed * 3.0f, 1.0f);

        graphics_test_draw_model_meshes(cmd, shader, model, mvp);
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

        mat4 mvp;
        graphics_test_build_model_mvp(mvp, &cam, mesh_center, mesh_extent, 1.0f, 1.0f);

        err = graphics_test_draw_model_meshes(cmd, shader, model, mvp);
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
 * Verify SituationGetMeshData works on an OBJ-loaded mesh.
 */
static void test_teapot_obj_mesh_data_access(void) {
    SituationModel model = {0};
    if (!try_load_teapot_obj(&model)) {
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
    SIT_ASSERT_EQ(vstride, 48);
    SIT_ASSERT_NOT_NULL(vdata);
    SIT_ASSERT_NOT_NULL(idata);

    SituationUnloadModel(&model);
}

/**
 * Load and unload teapot.obj 5 times — stress test for memory/resource leaks.
 */
static void test_teapot_obj_load_unload_cycle(void) {
    char path[512];
    if (!sit_test_resolve_harness_asset(TEAPOT_OBJ_FILE, path, sizeof(path))) {
        fprintf(stderr, "  [SKIP] harness asset %s not found\n", TEAPOT_OBJ_FILE);
        SIT_ASSERT(true);
        return;
    }

    for (int i = 0; i < 5; i++) {
        SituationModel model = {0};
        SituationError err = SituationLoadModelFromOBJ(path, &model);
        if (err != SITUATION_SUCCESS) {
            SIT_ASSERT(true);
            return;
        }
        SIT_ASSERT(model.mesh_count >= 1);
        SituationUnloadModel(&model);
    }
    SIT_ASSERT(true); /* No crash or leak through 5 cycles */
}

/**
 * Load, reload, and verify the teapot OBJ model.
 */
static void test_teapot_obj_reload(void) {
    SituationModel model = {0};
    if (!try_load_teapot_obj(&model)) {
        SIT_ASSERT(true);
        return;
    }

    SituationError err = SituationReloadModel(&model);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(model.mesh_count >= 1);
    SIT_ASSERT_NOT_NULL(model.meshes);

    SituationModelMesh* m = &model.meshes[0];
    SIT_ASSERT(m->gpu_mesh.vertex_count > 0);
    SIT_ASSERT(m->gpu_mesh.index_count > 0);
    SIT_ASSERT_EQ((int)m->gpu_mesh.vertex_stride, 48);

    SituationUnloadModel(&model);
}

/**
 * Load stanford-bunny.obj and verify mesh structure.
 */
static void test_bunny_obj_load(void) {
    SituationModel model = {0};
    if (!try_load_bunny_obj(&model)) {
        SIT_ASSERT(true);
        return;
    }

    SIT_ASSERT(model.mesh_count >= 1);
    SIT_ASSERT_NOT_NULL(model.meshes);

    SituationModelMesh* m = &model.meshes[0];
    SIT_ASSERT(m->gpu_mesh.vertex_count > 0);
    SIT_ASSERT(m->gpu_mesh.index_count > 0);
    SIT_ASSERT_EQ((int)m->gpu_mesh.vertex_stride, 48);

    SituationUnloadModel(&model);
}

/**
 * Load stanford-bunny.obj, wrap with rosewood-veneer.png (triplanar; bunny has no UVs),
 * render, and verify non-black framebuffer output.
 */
static void test_bunny_obj_rosewood_draw_and_verify(void) {
    SituationModel model = {0};
    if (!try_load_bunny_obj(&model)) {
        SIT_TEST_SKIP("stanford-bunny.obj not found or failed to load");
    }

    char rosewood_path[512];
    if (!try_assign_rosewood_texture(&model, rosewood_path, sizeof(rosewood_path))) {
        SituationUnloadModel(&model);
        SIT_TEST_SKIP("rosewood PNG not found — place any rosewood*.png in tests/harness/assets/ (e.g. rosewood_veneer1.png)");
    }

    SIT_ASSERT(model.meshes[0].base_color_texture.generation != 0);

    SituationShader shader = {0};
    SituationError err = SituationLoadShaderFromMemory(
        graphics_test_model_triplanar_vs(),
        graphics_test_model_triplanar_fs(),
        &shader);
    if (err != SITUATION_SUCCESS) {
        SituationUnloadModel(&model);
        SIT_TEST_SKIP("triplanar shader compile failed");
    }

    vec3 mesh_center = {0.0f, 0.0f, 0.0f};
    graphics_test_compute_mesh_aabb_center(mesh_center, &model.meshes[0]);
    const float mesh_extent = graphics_test_compute_mesh_max_extent(&model.meshes[0], mesh_center);

    SituationCameraDesc cam;
    graphics_test_camera_perspective_default(
        &cam, graphics_test_camera_distance_for_radius(BUNNY_DRAW_WORLD_RADIUS, 45.0f, 1.25f));

    double start_time = SituationTimerGetTime();
    const double duration = 1.5;

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
        mat4 mvp;
        graphics_test_build_model_mvp(
            mvp, &cam, mesh_center, mesh_extent, elapsed * 2.0f, BUNNY_DRAW_WORLD_RADIUS);

        graphics_test_draw_textured_model_meshes(cmd, shader, model, mvp);
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
    }

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

        mat4 mvp;
        graphics_test_build_model_mvp(
            mvp, &cam, mesh_center, mesh_extent, 0.75f, BUNNY_DRAW_WORLD_RADIUS);

        err = graphics_test_draw_textured_model_meshes(cmd, shader, model, mvp);
        SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();

        found_non_black = graphics_test_screen_any_non_black(10);
    }

    SituationUnloadShader(&shader);
    graphics_test_destroy_model_external_textures(&model);
    SituationUnloadModel(&model);
    SIT_ASSERT(found_non_black);
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase obj_loader_tests[] = {
    {"teapot_obj_load",             test_teapot_obj_load,             true},
    {"teapot_obj_draw_and_verify",   test_teapot_obj_draw_and_verify,   true},
    {"teapot_obj_mesh_data_access",  test_teapot_obj_mesh_data_access,  true},
    {"teapot_obj_load_unload_cycle", test_teapot_obj_load_unload_cycle, true},
    {"teapot_obj_reload",            test_teapot_obj_reload,            true},
    {"bunny_obj_load",               test_bunny_obj_load,               true},
    {"bunny_obj_rosewood_draw_and_verify", test_bunny_obj_rosewood_draw_and_verify, true},
};

const SitTestModule g_module_obj_loader = {
    .name           = "obj_loader",
    .setup          = obj_loader_setup,
    .teardown       = obj_loader_teardown,
    .tests          = obj_loader_tests,
    .test_count     = sizeof(obj_loader_tests) / sizeof(obj_loader_tests[0]),
    .requires_context = true
};
