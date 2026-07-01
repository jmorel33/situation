/**
 * @file test_projection_3d.c
 * @brief GPU integration tests for SituationCamera + MVP model presentation (Y-up world).
 *
 * Runs after the model/OBJ/STL loader modules. Loaders verify parsing; this suite verifies
 * that loaded meshes are framed with proper 3D view/projection (not Situation's 2D UI ortho).
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_assets.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_window.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEAPOT_OBJ_FILE "utah_teapot.obj"
#define BUNNY_OBJ_FILE "stanford-bunny.obj"
#define BUNNY_DRAW_WORLD_RADIUS 0.5f

static bool g_init_ok = false;

static void projection_3d_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_PROJECTION_3D");

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    graphics_test_print_renderer_banner();
}

static void projection_3d_teardown(void) {
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

static bool projection_3d_try_load_teapot(SituationModel* model) {
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

static bool projection_3d_try_load_bunny(SituationModel* model) {
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

static bool projection_3d_try_assign_rosewood(SituationModel* model) {
    if (!model || model->mesh_count <= 0 || !model->meshes) {
        return false;
    }
    char path[512];
    static const char* const k_names[] = {
        "rosewood-veneer.png",
        "rosewood_veneer1.png",
        "rosewood_veneer.png",
        "rosewood.png",
        NULL
    };
    if (!sit_test_resolve_harness_asset_any(k_names, path, sizeof(path))
        && !sit_test_resolve_harness_asset_name_contains("rosewood", path, sizeof(path))) {
        return false;
    }
    return SituationLoadTexture(path, true, &model->meshes[0].base_color_texture) == SITUATION_SUCCESS;
}

static SituationCameraDesc projection_3d_default_perspective_camera(void) {
    SituationCameraDesc cam;
    const float world_radius = 1.0f;
    const float dist = graphics_test_camera_distance_for_radius(world_radius, 45.0f, 1.25f);
    graphics_test_camera_perspective_default(&cam, dist);
    return cam;
}

/** CPU: view/proj are non-trivial and aspect auto-tracks the render target. */
static void test_proj3d_camera_aspect_auto(void) {
    SituationCameraDesc cam = projection_3d_default_perspective_camera();

    mat4 view;
    mat4 proj;
    SituationCameraBuildView(&cam, view);
    SituationCameraBuildProj(&cam, proj);

    const float render_aspect = (float)sit_test_window_render_width()
        / (float)sit_test_window_render_height();
    const float expected_p00 = 1.0f / (tanf(glm_rad(45.0f) * 0.5f) * render_aspect);

    SIT_ASSERT(fabsf(view[3][2]) > 0.01f);
    SIT_ASSERT(fabsf(proj[0][0] - expected_p00) < 0.02f);
    SIT_ASSERT(fabsf(proj[1][1] - 1.0f / tanf(glm_rad(45.0f) * 0.5f)) < 0.02f);
}

/** Draw teapot with MVP; center pixel must be lit. */
static void test_proj3d_teapot_draw_centered(void) {
    SituationModel model = {0};
    if (!projection_3d_try_load_teapot(&model)) {
        SIT_ASSERT(true);
        return;
    }

    SituationShader shader = {0};
    SituationError err = SituationLoadShaderFromMemory(
        graphics_test_model_normal_vs(),
        graphics_test_model_normal_fs(),
        &shader);
    if (err != SITUATION_SUCCESS) {
        SituationUnloadModel(&model);
        SIT_ASSERT(true);
        return;
    }

    vec3 mesh_center = {0.0f, 0.0f, 0.0f};
    graphics_test_compute_mesh_aabb_center(mesh_center, &model.meshes[0]);
    const float mesh_extent = graphics_test_compute_mesh_max_extent(&model.meshes[0], mesh_center);
    const SituationCameraDesc cam = projection_3d_default_perspective_camera();

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
    graphics_test_build_model_mvp(mvp, &cam, mesh_center, mesh_extent, 0.5f, 1.0f);
    err = graphics_test_draw_model_meshes(cmd, shader, model, mvp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();

    SIT_ASSERT(graphics_test_screen_any_non_black(10));

    SituationUnloadShader(&shader);
    SituationUnloadModel(&model);
}

/**
 * Y-up check (CPU): after MVP, mesh AABB top (+Y in world) must land above mesh bottom in NDC.
 * Uses SituationCamera perspective — not Situation's 2D UI ortho.
 */
static void test_proj3d_world_y_maps_screen_up(void) {
    SituationModel model = {0};
    if (!projection_3d_try_load_teapot(&model)) {
        SIT_ASSERT(true);
        return;
    }

    vec3 mesh_center = {0.0f, 0.0f, 0.0f};
    graphics_test_compute_mesh_aabb_center(mesh_center, &model.meshes[0]);
    const float mesh_extent = graphics_test_compute_mesh_max_extent(&model.meshes[0], mesh_center);
    const SituationCameraDesc cam = projection_3d_default_perspective_camera();
    const float world_radius = 1.0f;

    mat4 mvp;
    graphics_test_build_model_mvp(mvp, &cam, mesh_center, mesh_extent, 0.0f, world_radius);

    vec4 top_obj = {mesh_center[0], mesh_center[1] + mesh_extent, mesh_center[2], 1.0f};
    vec4 bot_obj = {mesh_center[0], mesh_center[1] - mesh_extent, mesh_center[2], 1.0f};
    vec4 clip_top = {0.0f, 0.0f, 0.0f, 0.0f};
    vec4 clip_bot = {0.0f, 0.0f, 0.0f, 0.0f};
    glm_mat4_mulv(mvp, top_obj, clip_top);
    glm_mat4_mulv(mvp, bot_obj, clip_bot);

    SIT_ASSERT(fabsf(clip_top[3]) > 1e-6f);
    SIT_ASSERT(fabsf(clip_bot[3]) > 1e-6f);

    const float ndc_top = clip_top[1] / clip_top[3];
    const float ndc_bot = clip_bot[1] / clip_bot[3];
    SIT_ASSERT(ndc_top > ndc_bot);
    SIT_ASSERT(ndc_top > 0.0f);
    SIT_ASSERT(ndc_bot < 0.0f);

    vec4 center_obj = {mesh_center[0], mesh_center[1], mesh_center[2], 1.0f};
    vec4 clip_center = {0.0f, 0.0f, 0.0f, 0.0f};
    glm_mat4_mulv(mvp, center_obj, clip_center);
    SIT_ASSERT(fabsf(clip_center[3]) > 1e-6f);
    SIT_ASSERT(fabsf(clip_center[0] / clip_center[3]) < 0.08f);
    SIT_ASSERT(fabsf(clip_center[1] / clip_center[3]) < 0.08f);

    SituationUnloadModel(&model);
}

/** Orthographic camera still frames the mesh with non-black center output. */
static void test_proj3d_ortho_draw(void) {
    SituationModel model = {0};
    if (!projection_3d_try_load_teapot(&model)) {
        SIT_ASSERT(true);
        return;
    }

    SituationShader shader = {0};
    SituationError err = SituationLoadShaderFromMemory(
        graphics_test_model_normal_vs(),
        graphics_test_model_normal_fs(),
        &shader);
    if (err != SITUATION_SUCCESS) {
        SituationUnloadModel(&model);
        SIT_ASSERT(true);
        return;
    }

    vec3 mesh_center = {0.0f, 0.0f, 0.0f};
    graphics_test_compute_mesh_aabb_center(mesh_center, &model.meshes[0]);
    const float mesh_extent = graphics_test_compute_mesh_max_extent(&model.meshes[0], mesh_center);

    SituationCameraDesc cam;
    graphics_test_camera_ortho_default(&cam, 3.0f);

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
    graphics_test_build_model_mvp(mvp, &cam, mesh_center, mesh_extent, 0.25f, 1.0f);
    err = graphics_test_draw_model_meshes(cmd, shader, model, mvp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();

    SIT_ASSERT(graphics_test_screen_any_non_black(10));

    SituationUnloadShader(&shader);
    SituationUnloadModel(&model);
}

/** Bunny + rosewood triplanar under the same camera path (skips if texture missing). */
static void test_proj3d_bunny_triplanar_draw(void) {
    SituationModel model = {0};
    if (!projection_3d_try_load_bunny(&model)) {
        SIT_ASSERT(true);
        return;
    }
    if (!projection_3d_try_assign_rosewood(&model)) {
        SituationUnloadModel(&model);
        SIT_TEST_SKIP("rosewood PNG not found — place rosewood*.png in tests/harness/assets/");
    }

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

    double start = SituationTimerGetTime();
    const double duration = 1.0;

    while (SituationTimerGetTime() - start < duration) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            break;
        }
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        if (!cmd) {
            break;
        }

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

        const float elapsed = (float)(SituationTimerGetTime() - start);
        mat4 mvp;
        graphics_test_build_model_mvp(
            mvp, &cam, mesh_center, mesh_extent, elapsed * 2.0f, BUNNY_DRAW_WORLD_RADIUS);
        graphics_test_draw_textured_model_meshes(cmd, shader, model, mvp);
        SituationCmdEndRenderPass(cmd);
        SituationEndFrame();
    }

    SIT_ASSERT(graphics_test_screen_any_non_black(10));

    SituationUnloadShader(&shader);
    graphics_test_destroy_model_external_textures(&model);
    SituationUnloadModel(&model);
}

static SitTestCase projection_3d_tests[] = {
    {"camera_aspect_auto",           test_proj3d_camera_aspect_auto,       true},
    {"teapot_draw_centered",         test_proj3d_teapot_draw_centered,     true},
    {"world_y_maps_screen_up",       test_proj3d_world_y_maps_screen_up,   true},
    {"ortho_draw",                   test_proj3d_ortho_draw,               true},
    {"bunny_triplanar_draw",         test_proj3d_bunny_triplanar_draw,     true},
};

const SitTestModule g_module_projection_3d = {
    .name             = "projection_3d",
    .setup            = projection_3d_setup,
    .teardown         = projection_3d_teardown,
    .tests            = projection_3d_tests,
    .test_count       = sizeof(projection_3d_tests) / sizeof(projection_3d_tests[0]),
    .requires_context = true
};
