/**
 * @file test_proj.c
 * @brief Tests for the Situation Camera and Projection Subsystem
 */

#include "sit_test_framework.h"
#include "situation.h"

// ----------------------------------------------------------------------------
// Test Cases
// ----------------------------------------------------------------------------

static void test_camera_basic_projection(void) {
    SituationCameraDesc cam = {0};
    cam.eye = (Vector3){{0.0f, 0.0f, 10.0f}};
    cam.target = (Vector3){{0.0f, 0.0f, 0.0f}};
    cam.up = (Vector3){{0.0f, 1.0f, 0.0f}};
    cam.vertical_fov_deg = 90.0f;
    cam.aspect = 1.0f;
    cam.z_near = 0.1f;
    cam.z_far = 100.0f;
    cam.flags = SIT_CAMERA_FLAG_NONE;

    mat4 view, proj, vp, inv_vp;
    SituationCameraBuildView(&cam, view);
    SituationCameraBuildProj(&cam, proj);
    SituationCameraBuildViewProj(&cam, vp);
    SituationCameraBuildInvViewProj(&cam, inv_vp);

    // Verify view matrix roughly looks down -Z
    SIT_ASSERT(view[3][2] < 0.0f);

    // Unproject center pixel
    Vector2 pixel = {50.0f, 50.0f};
    Vector2 resolution = {100.0f, 100.0f};
    
    Vector3 ray_origin, ray_dir;
    SituationCameraUnprojectPixel(&cam, inv_vp, pixel, resolution, &ray_origin, &ray_dir);

    // Ray origin should be at the near plane, which is 0.1 units in front of the eye {0, 0, 10} looking down -Z
    SIT_ASSERT(fabsf(ray_origin.x) < 0.01f);
    SIT_ASSERT(fabsf(ray_origin.y) < 0.01f);
    SIT_ASSERT(fabsf(ray_origin.z - 9.9f) < 0.01f);

    // Ray dir should point straight down -Z
    SIT_ASSERT(fabsf(ray_dir.x) < 0.01f);
    SIT_ASSERT(fabsf(ray_dir.y) < 0.01f);
    SIT_ASSERT(fabsf(ray_dir.z - (-1.0f)) < 0.01f);
}

static void test_camera_ortho_projection(void) {
    SituationCameraDesc cam = {0};
    cam.eye = (Vector3){{0.0f, 0.0f, 10.0f}};
    cam.target = (Vector3){{0.0f, 0.0f, 0.0f}};
    cam.up = (Vector3){{0.0f, 1.0f, 0.0f}};
    cam.ortho_height = 20.0f;
    cam.aspect = 2.0f; // Width = 40.0f
    cam.z_near = 0.1f;
    cam.z_far = 100.0f;
    cam.flags = SIT_CAMERA_FLAG_ORTHOGRAPHIC;

    mat4 proj;
    SituationCameraBuildProj(&cam, proj);

    // In ortho, P[0][0] = 2 / width = 2 / 40 = 0.05
    // P[1][1] = 2 / height = 2 / 20 = 0.1
    SIT_ASSERT(fabsf(proj[0][0] - 0.05f) < 0.001f);
    SIT_ASSERT(fabsf(proj[1][1] - 0.1f) < 0.001f);
}

// ----------------------------------------------------------------------------
// Module Definition
// ----------------------------------------------------------------------------

static void sit_test_proj_setup(void) {
    // Context-free tests, no heavy setup
}

static void sit_test_proj_teardown(void) {
}

static SitTestCase proj_tests[] = {
    {"Basic Perspective & Unproject", test_camera_basic_projection, false},
    {"Orthographic Build", test_camera_ortho_projection, false},
};

const SitTestModule g_module_proj = {
    .name = "Projection",
    .setup = sit_test_proj_setup,
    .teardown = sit_test_proj_teardown,
    .tests = proj_tests,
    .test_count = sizeof(proj_tests) / sizeof(proj_tests[0]),
    .requires_context = false
};
