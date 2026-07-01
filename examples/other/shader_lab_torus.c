/***************************************************************************************************
 *  Situation — Shader Lab: Interactive 3D Torus
 *  -------------------------------------------
 *  Custom vertex + fragment shaders, procedural surface shading, orbit camera, depth test.
 *
 *  Build:
 *    build\build_examples.bat opengl shader_lab_torus
 *    build\build_examples.bat static-vulkan shader_lab_torus
 *
 *  Vulkan: torus mesh uses vertex-pull (BDA + buffer_reference) when available; VAO fallback otherwise.
 *  Controls
 *    LMB drag      Orbit camera
 *    Mouse wheel   Zoom in / out
 *    Space         Toggle slow auto-rotation of the torus
 *    1 / 2 / 3     Visual mode: interference bands / iridescent / clean metal + rim
 *    R             Reset camera
 *    V             Toggle VSync
 *    C             Cycle torus colour preset (4)
 *    B             Cycle background copper palette (4)
 *    F12           Save PNG screenshot (current working directory)
 *
 *  Mesh layout matches Situation's legacy vertex contract: position, normal, UV (32 bytes / vertex).
 *
 *  Background: fullscreen triangle with a vertically scrolling OCS-style raster / copper bar palette
 *  (saturated horizontal stripes, per-scanline phase — demo scene vibe).
 ***************************************************************************************************/

#include "situation.h"
#include <cglm/cglm.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "font_data.h"

typedef struct {
    float pos[3];
    float nrm[3];
    float uv[2];
} TorusVertex;

static SituationShader g_shader = {0};
static SituationMesh g_mesh = {0};
static bool g_use_vertex_pull = false;

static SituationShader g_bg_shader = {0};
static SituationMesh g_bg_mesh = {0};

/* Fullscreen clip-space triangle (covers NDC; z = w => far plane, draws behind scene with LEQUAL depth). */
static const char* k_bg_vs =
    "#version 450 core\n"
    "layout(location = 0) in vec3 inPos;\n"
    "void main() {\n"
    "    /* NDC z slightly below 1.0 so GL_LESS passes vs cleared depth 1.0 (far plane). */\n"
    "    gl_Position = vec4(inPos.xy, 1.0 - 1.0e-4, 1.0);\n"
    "}\n";

static const char* k_bg_fs =
    "#version 450 core\n"
    "uniform float uTime;\n"
    "uniform vec2 uResolution;\n"
    "uniform int uBgScheme;\n"
    "out vec4 fragColor;\n"
    "\n"
    "vec3 hsv2rgb(vec3 c) {\n"
    "    vec3 rgb = clamp(abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);\n"
    "    return c.z * mix(vec3(1.0), rgb, c.y);\n"
    "}\n"
    "\n"
    "vec3 copper8(int k) {\n"
    "    k = k & 7;\n"
    "    int s = uBgScheme & 3;\n"
    "    if (s != 0) {\n"
    "        float hue = fract(float(k) / 8.0 + float(s) * 0.21);\n"
    "        float sat = 0.55 + 0.25 * float(k & 1);\n"
    "        float val = 0.22 + 0.55 * float(k + 1) / 8.0;\n"
    "        return hsv2rgb(vec3(hue, sat, val));\n"
    "    }\n"
    "    if (k == 0) return vec3(0.02, 0.02, 0.12);\n"
    "    if (k == 1) return vec3(0.00, 0.25, 0.55);\n"
    "    if (k == 2) return vec3(0.00, 0.55, 0.85);\n"
    "    if (k == 3) return vec3(0.35, 0.85, 1.00);\n"
    "    if (k == 4) return vec3(0.95, 0.55, 0.10);\n"
    "    if (k == 5) return vec3(0.75, 0.15, 0.55);\n"
    "    if (k == 6) return vec3(0.45, 0.20, 0.75);\n"
    "    return vec3(0.55, 0.60, 0.72);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    float x = gl_FragCoord.x;\n"
    "    float y = gl_FragCoord.y;\n"
    "    float w = max(uResolution.x, 1.0);\n"
    "    float h = max(uResolution.y, 1.0);\n"
    "\n"
    "    /* Vertical copper bars: floor(v) steps colour every stripePx; mod 8 cycles palette. */\n"
    "    const float stripePx = 7.0;\n"
    "    float scrollY = uTime * 55.0;\n"
    "    float v = (y + scrollY) / stripePx;\n"
    "    int stripe = int(floor(v));\n"
    "    int i0 = int(mod(float(stripe), 8.0));\n"
    "    int i1 = int(mod(float(stripe + 1), 8.0));\n"
    "    float blendV = fract(v);\n"
    "    vec3 cV = mix(copper8(i0), copper8(i1), smoothstep(0.12, 0.88, blendV));\n"
    "\n"
    "    /* Horizontal: phase wraps 0..1 every full framebuffer width (no edge seam). */\n"
    "    float scrollX = uTime * 22.0;\n"
    "    float u = fract((x + scrollX) / w);\n"
    "    float hShade = 0.90 + 0.10 * sin(u * 6.2831853 * 2.0);\n"
    "    vec3 col = cV * hShade;\n"
    "\n"
    "    /* Very light scanline (same period as stripes — stays aligned while scrolling). */\n"
    "    float scan = 0.94 + 0.06 * sin(fract(v) * 6.2831853);\n"
    "    col *= scan;\n"
    "\n"
    "    vec2 uv = (vec2(x, y) - 0.5 * vec2(w, h)) / min(w, h);\n"
    "    float vig = 1.0 - dot(uv, uv) * 0.28;\n"
    "    col *= clamp(vig, 0.62, 1.0);\n"
    "\n"
    "    fragColor = vec4(col, 1.0);\n"
    "}\n";

/* --- GLSL: animated procedural shading on a lit torus ---------------------------------------- */

static const char* k_vs =
    "#version 450 core\n"
    "layout(location = 0) in vec3 inPos;\n"
    "layout(location = 1) in vec3 inNormal;\n"
    "layout(location = 2) in vec2 inUV;\n"
    "\n"
    "uniform mat4 uModel;\n"
    "uniform mat4 uView;\n"
    "uniform mat4 uProj;\n"
    "\n"
    "layout(location = 0) out vec3 vWorldPos;\n"
    "layout(location = 1) out vec3 vNormal;\n"
    "layout(location = 2) out vec2 vUV;\n"
    "/* Intrinsic torus angles (radians), continuous — no UV wrap seam. Must match build_torus_mesh R. */\n"
    "layout(location = 3) out float vUTube;\n"
    "layout(location = 4) out float vVRing;\n"
    "\n"
    "void main() {\n"
    "    const float R_MAJ = 1.0;\n"
    "    vec2 xz = inPos.xz;\n"
    "    float lenxz = length(xz);\n"
    "    vec2 dir = lenxz > 1e-5 ? xz / lenxz : vec2(1.0, 0.0);\n"
    "    vec3 ringCen = vec3(R_MAJ * dir.x, 0.0, R_MAJ * dir.y);\n"
    "    vec3 w = inPos - ringCen;\n"
    "    vUTube = atan(w.y, length(w.xz));\n"
    "    vVRing = atan(inPos.z, inPos.x);\n"
    "\n"
    "    vec4 wp = uModel * vec4(inPos, 1.0);\n"
    "    vWorldPos = wp.xyz;\n"
    "    vNormal = mat3(uModel) * inNormal;\n"
    "    vUV = inUV;\n"
    "    gl_Position = uProj * uView * wp;\n"
    "}\n";

#if defined(SITUATION_USE_VULKAN)
/* Phase C4: same shading as k_vs; positions/normals/UV fetched via mesh BDA (32-byte legacy layout). */
static const char* k_vs_pull =
    "#version 450\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_buffer_reference2 : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "layout(location = 0) in vec3 inPosAttr;\n"
    "layout(location = 1) in vec3 inNormalAttr;\n"
    "layout(location = 2) in vec2 inUVAttr;\n"
    "layout(push_constant) uniform PC { uint64_t vertex_address; uint64_t index_address; } pc;\n"
    "struct SitVertex_Legacy { vec3 position; vec3 normal; vec2 texcoord; };\n"
    "layout(buffer_reference, scalar) readonly buffer SitVertexBuffer_Legacy { SitVertex_Legacy data[]; };\n"
    "uniform mat4 uModel;\n"
    "uniform mat4 uView;\n"
    "uniform mat4 uProj;\n"
    "layout(location = 0) out vec3 vWorldPos;\n"
    "layout(location = 1) out vec3 vNormal;\n"
    "layout(location = 2) out vec2 vUV;\n"
    "layout(location = 3) out float vUTube;\n"
    "layout(location = 4) out float vVRing;\n"
    "void main() {\n"
    "    SitVertexBuffer_Legacy verts = SitVertexBuffer_Legacy(pc.vertex_address);\n"
    "    SitVertex_Legacy v = verts.data[gl_VertexIndex];\n"
    "    vec3 inPos = v.position + inPosAttr * 0.0;\n"
    "    vec3 inNormal = v.normal + inNormalAttr * 0.0;\n"
    "    vec2 inUV = v.texcoord + inUVAttr * 0.0;\n"
    "    const float R_MAJ = 1.0;\n"
    "    vec2 xz = inPos.xz;\n"
    "    float lenxz = length(xz);\n"
    "    vec2 dir = lenxz > 1e-5 ? xz / lenxz : vec2(1.0, 0.0);\n"
    "    vec3 ringCen = vec3(R_MAJ * dir.x, 0.0, R_MAJ * dir.y);\n"
    "    vec3 w = inPos - ringCen;\n"
    "    vUTube = atan(w.y, length(w.xz));\n"
    "    vVRing = atan(inPos.z, inPos.x);\n"
    "    vec4 wp = uModel * vec4(inPos, 1.0);\n"
    "    vWorldPos = wp.xyz;\n"
    "    vNormal = mat3(uModel) * inNormal;\n"
    "    vUV = inUV;\n"
    "    gl_Position = uProj * uView * wp;\n"
    "}\n";
#endif

static const char* k_fs =
    "#version 450 core\n"
    "layout(location = 0) in vec3 vWorldPos;\n"
    "layout(location = 1) in vec3 vNormal;\n"
    "layout(location = 2) in vec2 vUV;\n"
    "layout(location = 3) in float vUTube;\n"
    "layout(location = 4) in float vVRing;\n"
    "\n"
    "uniform vec3 uCameraPos;\n"
    "uniform float uTime;\n"
    "uniform int uMode; /* 0 bands 1 irid 2 metal */\n"
    "uniform int uColorSet; /* 0..3 torus palette */\n"
    "\n"
    "out vec4 fragColor;\n"
    "\n"
    "vec3 hash33(vec3 p) {\n"
    "    p = fract(p * 0.1031);\n"
    "    p += dot(p, p.yzx + 33.33);\n"
    "    return fract((p.xxy + p.yxx) * p.zyx);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec3 N = normalize(vNormal);\n"
    "    vec3 V = normalize(uCameraPos - vWorldPos);\n"
    "    float NdotV = clamp(dot(N, V), 0.0, 1.0);\n"
    "\n"
    "    vec3 lightDir = normalize(vec3(0.35, 0.85, 0.4));\n"
    "    vec3 H = normalize(lightDir + V);\n"
    "    float diff = max(dot(N, lightDir), 0.0);\n"
    "    float spec = pow(max(dot(N, H), 0.0), 64.0);\n"
    "\n"
    "    float rim = pow(1.0 - NdotV, 3.2);\n"
    "    int cs = uColorSet & 3;\n"
    "    vec3 rimA = vec3(0.1, 0.6, 1.0);\n"
    "    vec3 rimB = vec3(1.0, 0.2, 0.75);\n"
    "    if (cs == 1) { rimA = vec3(1.0, 0.4, 0.15); rimB = vec3(0.2, 0.9, 0.35); }\n"
    "    if (cs == 2) { rimA = vec3(0.9, 0.85, 0.2); rimB = vec3(0.5, 0.2, 0.95); }\n"
    "    if (cs == 3) { rimA = vec3(0.3, 0.95, 0.95); rimB = vec3(0.95, 0.3, 0.5); }\n"
    "    vec3 rimCol = mix(rimA, rimB, 0.5 + 0.5 * sin(uTime * 0.7));\n"
    "\n"
    "    /* Angles from mesh geometry (not UV) — periodic, no seam where u wraps 0/1. */\n"
    "    float bands = 0.5 + 0.5 * sin(vUTube * 6.5 + uTime * 2.2) * sin(vVRing * 4.5 - uTime * 1.6);\n"
    "    float grain = hash33(vWorldPos * 8.0 + uTime).x;\n"
    "\n"
    "    vec3 base;\n"
    "    if (uMode == 0) {\n"
    "        vec3 c1 = vec3(0.05, 0.08, 0.12);\n"
    "        vec3 c2 = vec3(0.15, 0.85, 0.95);\n"
    "        if (cs == 1) { c1 = vec3(0.12, 0.04, 0.06); c2 = vec3(0.95, 0.35, 0.55); }\n"
    "        if (cs == 2) { c1 = vec3(0.04, 0.10, 0.05); c2 = vec3(0.45, 0.95, 0.40); }\n"
    "        if (cs == 3) { c1 = vec3(0.10, 0.06, 0.14); c2 = vec3(0.85, 0.75, 0.25); }\n"
    "        base = mix(c1, c2, bands * 0.65 + 0.2 * grain);\n"
    "    } else if (uMode == 1) {\n"
    "        float phase = vUTube + vVRing * 0.65 + uTime * 1.3 + float(cs) * 1.2;\n"
    "        vec3 c = 0.5 + 0.5 * cos(vec3(phase, phase + 2.1, phase + 4.2));\n"
    "        vec3 dark = vec3(0.04, 0.05, 0.07);\n"
    "        if (cs == 1) dark = vec3(0.08, 0.03, 0.04);\n"
    "        if (cs == 2) dark = vec3(0.03, 0.06, 0.04);\n"
    "        if (cs == 3) dark = vec3(0.05, 0.04, 0.08);\n"
    "        base = mix(dark, c, 0.85);\n"
    "    } else {\n"
    "        vec3 metal = vec3(0.72, 0.76, 0.82);\n"
    "        if (cs == 1) metal = vec3(0.85, 0.62, 0.58);\n"
    "        if (cs == 2) metal = vec3(0.58, 0.78, 0.68);\n"
    "        if (cs == 3) metal = vec3(0.72, 0.68, 0.88);\n"
    "        base = metal * (0.15 + 0.85 * diff);\n"
    "    }\n"
    "\n"
    "    vec3 color = base * (0.25 + 0.95 * diff) + vec3(1.0) * spec * 0.35;\n"
    "    color += rimCol * rim * 1.1;\n"
    "\n"
    "    float vignette = pow(NdotV, 0.35);\n"
    "    color *= mix(0.85, 1.0, vignette);\n"
    "\n"
    "    fragColor = vec4(color, 1.0);\n"
    "}\n";

static void build_torus_mesh(float R, float r, int major_seg, int minor_seg,
                             TorusVertex** out_v, int* out_vc, uint32_t** out_ix, int* out_ic) {
    int vmaj = major_seg + 1;
    int vmin = minor_seg + 1;
    int vc = vmaj * vmin;
    TorusVertex* v = (TorusVertex*)calloc((size_t)vc, sizeof(TorusVertex));
    if (!v) {
        *out_v = NULL;
        *out_vc = 0;
        *out_ix = NULL;
        *out_ic = 0;
        return;
    }

    for (int i = 0; i < vmaj; i++) {
        float v_angle = (float)i / (float)major_seg * 2.0f * (float)M_PI;
        float cv = cosf(v_angle);
        float sv = sinf(v_angle);
        for (int j = 0; j < vmin; j++) {
            float u_angle = (float)j / (float)minor_seg * 2.0f * (float)M_PI;
            float cu = cosf(u_angle);
            float su = sinf(u_angle);

            float ring_r = R + r * cu;
            float x = ring_r * cv;
            float y = r * su;
            float z = ring_r * sv;

            /* Outward unit normal on the tube (same as normalize(P - ring_center)). */
            float nx = cu * cv;
            float ny = su;
            float nz = cu * sv;

            int idx = i * vmin + j;
            v[idx].pos[0] = x;
            v[idx].pos[1] = y;
            v[idx].pos[2] = z;
            v[idx].nrm[0] = nx;
            v[idx].nrm[1] = ny;
            v[idx].nrm[2] = nz;
            v[idx].uv[0] = (float)j / (float)minor_seg;
            v[idx].uv[1] = (float)i / (float)major_seg;
        }
    }

    int tri_pairs = major_seg * minor_seg * 2;
    int ic = tri_pairs * 3;
    uint32_t* ix = (uint32_t*)malloc((size_t)ic * sizeof(uint32_t));
    if (!ix) {
        free(v);
        *out_v = NULL;
        *out_vc = 0;
        *out_ix = NULL;
        *out_ic = 0;
        return;
    }

    int w = 0;
    for (int i = 0; i < major_seg; i++) {
        for (int j = 0; j < minor_seg; j++) {
            int i0 = i * vmin + j;
            int i1 = i0 + 1;
            int i2 = i0 + vmin;
            int i3 = i2 + 1;
            /* CCW when viewed from outside / along outward normal (matches tube normals). */
            ix[w++] = (uint32_t)i0;
            ix[w++] = (uint32_t)i1;
            ix[w++] = (uint32_t)i2;
            ix[w++] = (uint32_t)i1;
            ix[w++] = (uint32_t)i3;
            ix[w++] = (uint32_t)i2;
        }
    }

    *out_v = v;
    *out_vc = vc;
    *out_ix = ix;
    *out_ic = ic;
}

static int init_gpu(void) {
    SituationError err = SituationLoadShaderFromMemory(k_bg_vs, k_bg_fs, &g_bg_shader);
    if (err != SITUATION_SUCCESS) {
        char* msg = NULL;
        SituationGetLastErrorMsg(&msg);
        fprintf(stderr, "[shader_lab_torus] Background shader compile failed: %s\n", msg ? msg : "?");
        if (msg) {
            SituationFreeString(msg);
        }
        return -1;
    }

    {
        TorusVertex tri[3] = {
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{3.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{-1.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        };
        uint32_t ix[3] = {0, 1, 2};
        err = SituationCreateMesh(tri, 3, sizeof(TorusVertex), ix, 3, &g_bg_mesh);
        if (err != SITUATION_SUCCESS) {
            char* msg = NULL;
            SituationGetLastErrorMsg(&msg);
            fprintf(stderr, "[shader_lab_torus] Background mesh failed: %s\n", msg ? msg : "?");
            if (msg) {
                SituationFreeString(msg);
            }
            SituationUnloadShader(&g_bg_shader);
            return -1;
        }
    }

    TorusVertex* vbuf = NULL;
    uint32_t* ibuf = NULL;
    int vcount = 0, icount = 0;
    build_torus_mesh(1.0f, 0.38f, 48, 24, &vbuf, &vcount, &ibuf, &icount);
    if (!vbuf || !ibuf || vcount <= 0 || icount <= 0) {
        fprintf(stderr, "[shader_lab_torus] Mesh allocation failed.\n");
        if (vbuf) {
            free(vbuf);
        }
        if (ibuf) {
            free(ibuf);
        }
        SituationDestroyMesh(&g_bg_mesh);
        SituationUnloadShader(&g_bg_shader);
        return -1;
    }

#if defined(SITUATION_USE_VULKAN)
    err = SituationCreateMeshEx(vbuf, vcount, sizeof(TorusVertex), ibuf, icount,
        SIT_MESH_LAYOUT_PULL, &g_mesh);
#else
    err = SituationCreateMesh(vbuf, vcount, sizeof(TorusVertex), ibuf, icount, &g_mesh);
#endif
    free(vbuf);
    free(ibuf);

    if (err != SITUATION_SUCCESS) {
        char* msg = NULL;
        SituationGetLastErrorMsg(&msg);
        fprintf(stderr, "[shader_lab_torus] Mesh upload failed: %s\n", msg ? msg : "?");
        if (msg) {
            SituationFreeString(msg);
        }
        SituationDestroyMesh(&g_bg_mesh);
        SituationUnloadShader(&g_bg_shader);
        return -1;
    }

#if defined(SITUATION_USE_VULKAN)
    if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)
        && SituationGetMeshVertexBufferAddress(g_mesh) != 0) {
        g_use_vertex_pull = true;
        err = SituationLoadShaderFromMemory(k_vs_pull, k_fs, &g_shader);
    } else {
        g_use_vertex_pull = false;
        err = SituationLoadShaderFromMemory(k_vs, k_fs, &g_shader);
    }
#else
    g_use_vertex_pull = false;
    err = SituationLoadShaderFromMemory(k_vs, k_fs, &g_shader);
#endif
    if (err != SITUATION_SUCCESS) {
        char* msg = NULL;
        SituationGetLastErrorMsg(&msg);
        fprintf(stderr, "[shader_lab_torus] Shader compile failed: %s\n", msg ? msg : "?");
        if (msg) {
            SituationFreeString(msg);
        }
        SituationDestroyMesh(&g_mesh);
        SituationDestroyMesh(&g_bg_mesh);
        SituationUnloadShader(&g_bg_shader);
        return -1;
    }
    return 0;
}

static void ui_draw_line_centered(SituationCommandBuffer cmd, SituationFont* font, const char* text,
                                  float screen_w, float y, float font_size, ColorRGBA color) {
    /* Grid/8x8 path advances by one scaled cell per char (~font_size px), not ~0.52em TTF average. */
    float est_w = (float)strlen(text) * font_size;
    float x = (screen_w - est_w) * 0.5f;
    if (x < 4.0f) {
        x = 4.0f;
    }
    SituationCmdDrawTextEx(cmd, *font, text, (Vector2){{x, y}}, font_size, 0.0f, color);
}

static void orbit_eye(float yaw, float pitch, float radius, vec3 center, vec3 out_eye) {
    float cp = cosf(pitch);
    float sp = sinf(pitch);
    float cy = cosf(yaw);
    float sy = sinf(yaw);
    vec3 offset = {radius * cp * sy, radius * sp, radius * cp * cy};
    glm_vec3_add(center, offset, out_eye);
}

int main(int argc, char** argv) {
    SituationInitInfo cfg = {
        .window_title = "Situation — Shader Lab (Torus)",
        .window_width = 1280,
        .window_height = 720,
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT,
    };

    if (SituationInit(argc, argv, &cfg) != SITUATION_SUCCESS) {
        return -1;
    }

    if (init_gpu() != 0) {
        SituationShutdown();
        return -1;
    }

    SituationFont ui_font = {0};
    SituationLoadBitmapFontFromMemory(ibm_font_8x8, 8, 8, 256, &ui_font);

    float yaw = 0.9f;
    float pitch = 0.35f;
    float radius = 4.2f;
    vec3 target = {0.0f, 0.0f, 0.0f};
    bool auto_spin = true;
    float spin = 0.0f;
    int mode = 0;
    int torus_palette = 0;
    int bg_scheme = 0;
    bool vsync_on = true;
    SituationSetVSync(true);

    static const char* k_mode_names[] = {"bands", "iridescent", "metal"};

    printf(
        "Shader Lab — 3D torus (on-screen help at bottom)\n"
        "  Backend: %s — torus draw: %s\n"
        "  F12: PNG screenshot (cwd)   V: VSync   C/B: colours   see screen for full keys\n",
        SituationGetGraphicsBackendName(),
        g_use_vertex_pull ? "vertex pull (mesh BDA)" : "VAO attribute fetch");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        float dt = SituationGetFrameTime();
        float rw = (float)SituationGetRenderWidth();
        float rh = (float)SituationGetRenderHeight();
        float aspect = rh > 1.0f ? rw / rh : 1.0f;

        if (SituationIsKeyPressed(SIT_KEY_R)) {
            yaw = 0.9f;
            pitch = 0.35f;
            radius = 4.2f;
        }
        if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
            auto_spin = !auto_spin;
        }
        if (SituationIsKeyPressed(SIT_KEY_1)) {
            mode = 0;
        }
        if (SituationIsKeyPressed(SIT_KEY_2)) {
            mode = 1;
        }
        if (SituationIsKeyPressed(SIT_KEY_3)) {
            mode = 2;
        }
        if (SituationIsKeyPressed(SIT_KEY_V)) {
            vsync_on = !vsync_on;
            SituationSetVSync(vsync_on);
        }
        if (SituationIsKeyPressed(SIT_KEY_C)) {
            torus_palette = (torus_palette + 1) & 3;
        }
        if (SituationIsKeyPressed(SIT_KEY_B)) {
            bg_scheme = (bg_scheme + 1) & 3;
        }

        if (SituationIsMouseButtonDown(0)) {
            Vector2 d = SituationGetMouseDelta();
            yaw -= d.x * 0.005f;
            pitch -= d.y * 0.005f;
            float lim = 1.4f;
            if (pitch > lim) {
                pitch = lim;
            }
            if (pitch < -lim) {
                pitch = -lim;
            }
        }

        radius -= SituationGetMouseWheelMove() * 0.45f;
        if (radius < 2.0f) {
            radius = 2.0f;
        }
        if (radius > 14.0f) {
            radius = 14.0f;
        }

        if (auto_spin) {
            spin += dt * 0.55f;
        }

        vec3 eye;
        orbit_eye(yaw, pitch, radius, target, eye);

        mat4 proj;
        glm_perspective(glm_rad(58.0f), aspect, 0.1f, 200.0f, proj);

        mat4 view;
        glm_lookat(eye, target, (vec3){0.0f, 1.0f, 0.0f}, view);

        mat4 model;
        glm_mat4_identity(model);
        glm_rotate(model, spin, (vec3){0.0f, 1.0f, 0.0f});
        glm_rotate(model, spin * 0.37f, (vec3){1.0f, 0.0f, 0.0f});

        float t = (float)SituationTimerGetTime();

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = {.color = {6, 8, 14, 255}},
                },
                .depth_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = {.depth = 1.0f},
                },
            };

            SituationCmdBeginRenderPass(cmd, &pass);

            /* Background: copper raster (fullscreen tri at far depth). */
            SituationCmdBindPipeline(cmd, g_bg_shader);
            {
                float res[2] = {rw, rh};
                SituationSetShaderUniform(g_bg_shader, "uTime", &t, SIT_UNIFORM_FLOAT);
                SituationSetShaderUniform(g_bg_shader, "uResolution", res, SIT_UNIFORM_VEC2);
                SituationSetShaderUniform(g_bg_shader, "uBgScheme", &bg_scheme, SIT_UNIFORM_INT);
            }
            SituationCmdDrawMesh(cmd, g_bg_mesh);

            SituationCmdBindPipeline(cmd, g_shader);
            SituationSetShaderUniform(g_shader, "uModel", model, SIT_UNIFORM_MAT4);
            SituationSetShaderUniform(g_shader, "uView", view, SIT_UNIFORM_MAT4);
            SituationSetShaderUniform(g_shader, "uProj", proj, SIT_UNIFORM_MAT4);
            SituationSetShaderUniform(g_shader, "uCameraPos", eye, SIT_UNIFORM_VEC3);
            SituationSetShaderUniform(g_shader, "uTime", &t, SIT_UNIFORM_FLOAT);
            SituationSetShaderUniform(g_shader, "uMode", &mode, SIT_UNIFORM_INT);
            SituationSetShaderUniform(g_shader, "uColorSet", &torus_palette, SIT_UNIFORM_INT);

            if (g_use_vertex_pull) {
                SituationCmdBindMeshPullBuffers(cmd, g_mesh);
            }
            SituationCmdDrawMesh(cmd, g_mesh);

            /* UI overlay (after 3D — text draws as quads, typically depth-tested last). */
            {
                const float fs = 14.0f;
                const float lh = 17.0f;
                const float margin_bot = 14.0f;
                int fps = SituationGetFPS();
                char line[192];

                float y = rh - margin_bot - lh * 4.5f;
                ColorRGBA hi = {230, 235, 245, 255};
                ColorRGBA dim = {160, 175, 195, 255};

                snprintf(line, sizeof line, "FPS %d   VSync %s   [V] toggle",
                         fps, vsync_on ? "ON" : "OFF");
                ui_draw_line_centered(cmd, &ui_font, line, rw, y, fs, hi);
                y += lh;

                snprintf(line, sizeof line, "Mode: %s   Torus colours %d/%d [C]   BG %d/%d [B]",
                         k_mode_names[mode < 3 ? mode : 0],
                         torus_palette + 1, 4, bg_scheme + 1, 4);
                ui_draw_line_centered(cmd, &ui_font, line, rw, y, fs, hi);
                y += lh;

                ui_draw_line_centered(cmd, &ui_font,
                    "LMB drag orbit   Wheel zoom   Space spin   1-3 mode   R reset   F12 PNG", rw, y, fs, dim);
            }

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();

            if (SituationIsKeyPressed(SIT_KEY_F12)) {
                static int screenshot_seq = 0;
                char path[256];
                time_t now = time(NULL);
                snprintf(path, sizeof path, "shader_lab_%lld_%04d.png", (long long)now, screenshot_seq++);
                SituationError se = SituationTakeScreenshot(path);
                if (se == SITUATION_SUCCESS) {
                    printf("Screenshot saved: %s\n", path);
                } else {
                    char* err = NULL;
                    if (SituationGetLastErrorMsg(&err) == SITUATION_SUCCESS && err) {
                        fprintf(stderr, "Screenshot failed: %s\n", err);
                        SituationFreeString(err);
                    } else {
                        fprintf(stderr, "Screenshot failed (error code %d)\n", (int)se);
                    }
                }
            }
        }
    }

    SituationUnloadFont(ui_font);
    SituationDestroyMesh(&g_mesh);
    SituationDestroyMesh(&g_bg_mesh);
    SituationUnloadShader(&g_bg_shader);
    SituationUnloadShader(&g_shader);
    SituationShutdown();
    return 0;
}
