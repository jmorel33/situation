/***************************************************************************************************
 *  Situation — 20: Load and Draw Model
 *
 *  Load a mesh from disk (Utah teapot OBJ, fallback STL), orbit camera, simple lit draw.
 *
 *  Asset: tests/harness/assets/utah_teapot.obj (or teapot.stl)
 *
 *  Keys:
 *    LMB drag      Orbit
 *    Wheel         Zoom
 *    R             Reset camera
 *    SPACE         Toggle auto-rotate
 *
 *  Build:
 *    build\build_examples.bat static-opengl  20_load_and_draw_model
 *    build\build_examples.bat static-vulkan  20_load_and_draw_model
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <cglm/cglm.h>
#include <stdio.h>
#include <string.h>

static SituationModel  g_model  = {0};
static SituationShader g_shader = {0};
static int             g_loaded = 0;

static float g_yaw   = 0.6f;
static float g_pitch = 0.35f;
static float g_dist  = 4.0f;
static int   g_auto  = 1;
static int   g_drag  = 0;

static vec3  g_center = {0.0f, 0.0f, 0.0f};
static float g_extent = 1.0f;

static const char* k_vs =
#if defined(SITUATION_USE_VULKAN)
    "#version 450\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "layout(push_constant) uniform PC { mat4 uMVP; } pc;\n"
    "layout(location = 0) out vec3 vNormal;\n"
    "void main() {\n"
    "    vNormal = aNormal;\n"
    "    gl_Position = pc.uMVP * vec4(aPos, 1.0);\n"
    "}\n";
#else
    "#version 460 core\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "layout(location = 0) uniform mat4 uMVP;\n"
    "layout(location = 0) out vec3 vNormal;\n"
    "void main() {\n"
    "    vNormal = aNormal;\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "}\n";
#endif

static const char* k_fs =
#if defined(SITUATION_USE_VULKAN)
    "#version 450\n"
    "layout(location = 0) in vec3 vNormal;\n"
    "layout(location = 0) out vec4 fragColor;\n"
    "void main() {\n"
    "    vec3 n = normalize(vNormal);\n"
    "    vec3 light = normalize(vec3(0.4, 0.9, 0.3));\n"
    "    float diff = max(dot(n, light), 0.15);\n"
    "    vec3 base = vec3(0.85, 0.55, 0.25);\n"
    "    fragColor = vec4(base * diff, 1.0);\n"
    "}\n";
#else
    "#version 460 core\n"
    "layout(location = 0) in vec3 vNormal;\n"
    "layout(location = 0) out vec4 fragColor;\n"
    "void main() {\n"
    "    vec3 n = normalize(vNormal);\n"
    "    vec3 light = normalize(vec3(0.4, 0.9, 0.3));\n"
    "    float diff = max(dot(n, light), 0.15);\n"
    "    vec3 base = vec3(0.85, 0.55, 0.25);\n"
    "    fragColor = vec4(base * diff, 1.0);\n"
    "}\n";
#endif

static void compute_mesh_bounds(void)
{
    if (g_model.mesh_count <= 0 || !g_model.meshes) return;

    SituationModelMesh* m = &g_model.meshes[0];
    void* vdata = NULL;
    int vcount = 0, vstride = 0;
    SituationGetMeshData(m->gpu_mesh, &vdata, &vcount, &vstride, NULL, NULL);
    if (!vdata || vcount <= 0 || vstride < (int)(3 * sizeof(float))) {
        if (vdata) SIT_FREE(vdata);
        return;
    }

    const float* verts = (const float*)vdata;
    const int fpv = vstride / (int)sizeof(float);
    vec3 mn = { verts[0], verts[1], verts[2] };
    vec3 mx = { verts[0], verts[1], verts[2] };

    for (int i = 1; i < vcount; ++i) {
        const float* p = verts + i * fpv;
        for (int c = 0; c < 3; ++c) {
            if (p[c] < (&mn[0])[c]) (&mn[0])[c] = p[c];
            if (p[c] > (&mx[0])[c]) (&mx[0])[c] = p[c];
        }
    }
    SIT_FREE(vdata);

    g_center[0] = 0.5f * (mn[0] + mx[0]);
    g_center[1] = 0.5f * (mn[1] + mx[1]);
    g_center[2] = 0.5f * (mn[2] + mx[2]);
    g_extent = 0.0f;
    for (int c = 0; c < 3; ++c) {
        float e = fmaxf(fabsf(mx[c] - g_center[c]), fabsf(mn[c] - g_center[c]));
        if (e > g_extent) g_extent = e;
    }
    if (g_extent < 1e-4f) g_extent = 1.0f;
}

static int try_load_model(void)
{
    static const char* paths[] = {
        "tests/harness/assets/utah_teapot.obj",
        "tests/harness/assets/teapot.stl",
        NULL
    };

    for (int i = 0; paths[i]; ++i) {
        if (!SituationFileExists(paths[i])) continue;

        SituationError err;
        if (strstr(paths[i], ".obj")) {
            err = SituationLoadModelFromOBJ(paths[i], &g_model);
        } else {
            err = SituationLoadModelFromSTL(paths[i], true, &g_model);
        }
        if (err == SITUATION_SUCCESS && g_model.mesh_count > 0) {
            printf("[20] Loaded: %s (%d mesh(es))\n", paths[i], g_model.mesh_count);
            compute_mesh_bounds();
            g_loaded = 1;
            return 0;
        }
        SituationUnloadModel(&g_model);
        memset(&g_model, 0, sizeof g_model);
    }

    fprintf(stderr, "[20] No model found. Run from repo root; need utah_teapot.obj or teapot.stl\n");
    return -1;
}

static void build_mvp(mat4 out, float angle_y)
{
    SituationCameraDesc cam = {0};
    const float eye_dist = g_dist * g_extent * 2.5f;
    cam.eye = (Vector3){{ eye_dist * sinf(g_yaw) * cosf(g_pitch),
                          eye_dist * sinf(g_pitch),
                          eye_dist * cosf(g_yaw) * cosf(g_pitch) }};
    cam.target = (Vector3){{ g_center[0], g_center[1], g_center[2] }};
    cam.up = (Vector3){{0.0f, 1.0f, 0.0f}};
    cam.vertical_fov_deg = 45.0f;
    cam.aspect = 0.0f;
    cam.z_near = 0.01f;
    cam.z_far = eye_dist * 8.0f;

    mat4 vp, model, tmp, scaled, centered, rotated;
    SituationCameraBuildViewProj(&cam, vp);

    glm_translate_make(centered, (vec3){-g_center[0], -g_center[1], -g_center[2]});
    glm_scale_make(scaled, (vec3){1.0f / g_extent, 1.0f / g_extent, 1.0f / g_extent});
    glm_rotate_make(rotated, angle_y, (vec3){0.0f, 1.0f, 0.0f});
    glm_mat4_mul(scaled, centered, tmp);
    glm_mat4_mul(rotated, tmp, model);
    glm_mat4_mul(vp, model, out);
}

static void draw_model(SituationCommandBuffer cmd, const mat4 mvp)
{
    SituationCmdBindPipeline(cmd, g_shader);
    SituationCmdSetCullMode(cmd, SIT_CULL_BACK);

    for (int i = 0; i < g_model.mesh_count; ++i) {
        SituationMesh mesh = g_model.meshes[i].gpu_mesh;
        if (mesh.generation == 0 && mesh.slot_index == 0) continue;
        SituationCmdSetPushConstant(cmd, 0, mvp, sizeof(mat4));
        SituationCmdDrawMesh(cmd, mesh);
    }
}

static void handle_camera_input(float dt)
{
    if (SituationIsKeyPressed(SIT_KEY_R)) {
        g_yaw = 0.6f; g_pitch = 0.35f; g_dist = 4.0f;
    }
    if (SituationIsKeyPressed(SIT_KEY_SPACE)) g_auto = !g_auto;

    if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) g_drag = 1;
    if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_LEFT)) g_drag = 0;

    if (g_drag) {
        Vector2 d = SituationGetMouseDelta();
        g_yaw   += d.x * 0.008f;
        g_pitch += d.y * 0.008f;
        if (g_pitch > 1.4f) g_pitch = 1.4f;
        if (g_pitch < -1.4f) g_pitch = -1.4f;
    }

    {
        float scroll = SituationGetMouseWheelMove();
        if (scroll != 0.0f) {
            g_dist = fmaxf(1.5f, fminf(12.0f, g_dist - scroll * 0.4f));
        }
    }

    if (g_auto) g_yaw += dt * 0.35f;
}

int main(int argc, char** argv)
{
    if (SitExample_Init(argc, argv, "Situation — Load Model") != SITUATION_SUCCESS)
        return -1;

    if (SituationLoadShaderFromMemory(k_vs, k_fs, &g_shader) != SITUATION_SUCCESS) {
        SitExample_Shutdown();
        return -1;
    }
    if (try_load_model() != 0) {
        SituationUnloadShader(&g_shader);
        SitExample_Shutdown();
        return -1;
    }

    float angle = 0.0f;

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) break;

        float dt = SituationGetFrameTime();
        handle_camera_input(dt);
        if (g_auto) angle += dt * 0.8f;

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = { .color = {30, 30, 40, 255} }
                },
                .depth_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = { .depth = 1.0f }
                }
            };
            SituationCmdBeginRenderPass(cmd, &pass);

            mat4 mvp;
            build_mvp(mvp, angle);
            draw_model(cmd, mvp);

            SitExample_DrawHUD(cmd, "20 — Load and Draw Model",
                "LMB:orbit  Wheel:zoom  R:reset  SPACE:auto-rotate");

            SituationCmdEndRenderPass(cmd);
            SitExample_EndFrame();
        }
    }

    if (g_model.mesh_count > 0) SituationUnloadModel(&g_model);
    SituationUnloadShader(&g_shader);
    SitExample_Shutdown();
    return 0;
}
