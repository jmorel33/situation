/***************************************************************************************************
 *  Situation — Shader Lab: Real-time GPU ray trace
 *  ----------------------------------------------
 *  Fullscreen ray tracer: spheres, AABB box, finite cylinder, checker floor, shadows, reflections, sky.
 *
 *  Build (OpenGL, MinGW):
 *    build_examples.bat opengl shader_lab_raytrace
 *
 *  Controls
 *    LMB drag      Orbit camera
 *    Mouse wheel   Zoom in / out
 *    Space         Pause / resume animation
 *    1 / 2 / 3     Scene preset (jewel box / studio / neon pool)
 *    R             Reset camera
 *    V             Toggle VSync
 *    F12           Save PNG screenshot (current working directory)
 *
 *  One draw call per frame (fullscreen triangle). Matrices via cglm; shading in GLSL 450.
 ***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
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
} RtvVertex;

static SituationShader g_shader = {0};
static SituationMesh g_fs_mesh = {0};

static const char* k_vs =
    "#version 450 core\n"
    "layout(location = 0) in vec3 inPos;\n"
    "void main() {\n"
    "    gl_Position = vec4(inPos.xy, 1.0 - 1.0e-4, 1.0);\n"
    "}\n";

static const char* k_fs =
    "#version 450 core\n"
    "uniform vec3 uCameraPos;\n"
    "uniform mat4 uInvVP;\n"
    "uniform vec2 uResolution;\n"
    "uniform float uTime;\n"
    "uniform int uPreset;\n"
    "out vec4 fragColor;\n"
    "\n"
    "struct Hit {\n"
    "    float t;\n"
    "    int id; /* 0 none 1 floor 2-4 spheres 5 box 6 cylinder 7 small sphere */\n"
    "    vec3 p;\n"
    "    vec3 n;\n"
    "};\n"
    "\n"
    "float hash11(float p) {\n"
    "    p = fract(p * 0.1031);\n"
    "    p *= p + 33.33;\n"
    "    return fract(p * p);\n"
    "}\n"
    "\n"
    "bool sphIntersect(vec3 ro, vec3 rd, vec3 c, float r, out float t) {\n"
    "    vec3 oc = ro - c;\n"
    "    float b = dot(oc, rd);\n"
    "    float cc = dot(oc, oc) - r * r;\n"
    "    float h = b * b - cc;\n"
    "    if (h < 0.0) return false;\n"
    "    h = sqrt(h);\n"
    "    t = -b - h;\n"
    "    if (t <= 1.0e-4) t = -b + h;\n"
    "    return t > 1.0e-4;\n"
    "}\n"
    "\n"
    "bool planeY(vec3 ro, vec3 rd, float y, out float t) {\n"
    "    if (abs(rd.y) < 1.0e-6) return false;\n"
    "    t = (y - ro.y) / rd.y;\n"
    "    return t > 1.0e-4;\n"
    "}\n"
    "\n"
    "bool boxAabb(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out float tHit, out vec3 nor) {\n"
    "    vec3 invRd = 1.0 / rd;\n"
    "    vec3 t0 = (bmin - ro) * invRd;\n"
    "    vec3 t1 = (bmax - ro) * invRd;\n"
    "    vec3 tsm = min(t0, t1);\n"
    "    vec3 tbg = max(t0, t1);\n"
    "    float tMin = max(max(tsm.x, tsm.y), tsm.z);\n"
    "    float tMax = min(min(tbg.x, tbg.y), tbg.z);\n"
    "    if (tMax < max(tMin, 0.0)) return false;\n"
    "    float t = tMin > 1.0e-4 ? tMin : tMax;\n"
    "    if (t < 1.0e-4) return false;\n"
    "    tHit = t;\n"
    "    vec3 p = ro + rd * t;\n"
    "    vec3 c = 0.5 * (bmin + bmax);\n"
    "    vec3 e = 0.5 * (bmax - bmin) + vec3(1.0e-4);\n"
    "    vec3 o = p - c;\n"
    "    vec3 a = abs(o / e);\n"
    "    if (a.x > a.y && a.x > a.z) nor = vec3(sign(o.x), 0.0, 0.0);\n"
    "    else if (a.y > a.z) nor = vec3(0.0, sign(o.y), 0.0);\n"
    "    else nor = vec3(0.0, 0.0, sign(o.z));\n"
    "    return true;\n"
    "}\n"
    "\n"
    "bool cylYFinite(vec3 ro, vec3 rd, vec2 cxz, float rad, float y0, float y1,\n"
    "                out float tHit, out vec3 nor) {\n"
    "    vec2 oc = ro.xz - cxz;\n"
    "    vec2 d = rd.xz;\n"
    "    float a = dot(d, d);\n"
    "    if (a < 1.0e-10) return false;\n"
    "    float b = 2.0 * dot(oc, d);\n"
    "    float c = dot(oc, oc) - rad * rad;\n"
    "    float h = b * b - 4.0 * a * c;\n"
    "    if (h < 0.0) return false;\n"
    "    float s = sqrt(h);\n"
    "    float ta = (-b - s) / (2.0 * a);\n"
    "    float tb = (-b + s) / (2.0 * a);\n"
    "    float t = -1.0;\n"
    "    if (ta > 1.0e-4) {\n"
    "        float yp = ro.y + ta * rd.y;\n"
    "        if (yp >= y0 && yp <= y1) t = ta;\n"
    "    }\n"
    "    if (t < 0.0 && tb > 1.0e-4) {\n"
    "        float yp = ro.y + tb * rd.y;\n"
    "        if (yp >= y0 && yp <= y1) t = tb;\n"
    "    }\n"
    "    if (t < 0.0) return false;\n"
    "    tHit = t;\n"
    "    vec3 p = ro + rd * t;\n"
    "    nor = normalize(vec3(p.x - cxz.x, 0.0, p.z - cxz.y));\n"
    "    return true;\n"
    "}\n"
    "\n"
    "vec3 skyColor(vec3 rd, int preset, float tm) {\n"
    "    float g = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);\n"
    "    vec3 zen = vec3(0.08, 0.12, 0.28);\n"
    "    vec3 hor = vec3(0.65, 0.72, 0.88);\n"
    "    if ((preset & 3) == 1) {\n"
    "        zen = vec3(0.02, 0.02, 0.05);\n"
    "        hor = vec3(0.55, 0.52, 0.58);\n"
    "    }\n"
    "    if ((preset & 3) == 2) {\n"
    "        float band = 0.5 + 0.5 * sin(rd.y * 6.0 + tm * 1.7);\n"
    "        zen = mix(vec3(0.05, 0.0, 0.12), vec3(0.0, 0.25, 0.35), band);\n"
    "        hor = mix(vec3(0.9, 0.15, 0.45), vec3(0.2, 0.85, 0.75), band);\n"
    "    }\n"
    "    vec3 col = mix(hor, zen, pow(g, 1.35));\n"
    "    float sun = pow(max(dot(rd, normalize(vec3(0.35, 0.92, 0.25))), 0.0), 256.0);\n"
    "    col += vec3(1.0, 0.96, 0.85) * sun * 2.2;\n"
    "    return col;\n"
    "}\n"
    "\n"
    "Hit sceneIntersect(vec3 ro, vec3 rd, float tm, int preset) {\n"
    "    Hit h;\n"
    "    h.t = 1.0e30;\n"
    "    h.id = 0;\n"
    "\n"
    "    float t;\n"
    "    if (planeY(ro, rd, 0.0, t) && t < h.t) {\n"
    "        h.t = t;\n"
    "        h.id = 1;\n"
    "        h.p = ro + rd * t;\n"
    "        h.n = vec3(0.0, 1.0, 0.0);\n"
    "    }\n"
    "\n"
    "    float orbitR = 2.75;\n"
    "    float oy = 0.52;\n"
    "    float os = 0.78;\n"
    "    float oc = 0.78;\n"
    "    if ((preset & 3) == 1) { orbitR = 2.45; oy = 0.48; os = 0.55; oc = 0.55; }\n"
    "    if ((preset & 3) == 2) { orbitR = 2.85; oy = 0.52; os = 1.1; oc = 0.95; }\n"
    "    vec3 c1 = vec3(sin(tm * os) * orbitR, oy + 0.07 * sin(tm * 1.35), cos(tm * oc) * orbitR);\n"
    "\n"
    "    vec3 c0 = vec3(0.0, 1.0, 0.0);\n"
    "    vec3 c2 = vec3(1.05, 0.42, -0.35);\n"
    "    vec3 c3 = vec3(-1.55, 0.3, 2.55);\n"
    "    if ((preset & 3) == 1) {\n"
    "        c0 = vec3(0.0, 0.85, 0.0);\n"
    "        c2 = vec3(0.9, 0.38, 0.75);\n"
    "        c3 = vec3(1.65, 0.26, -2.15);\n"
    "    }\n"
    "    if ((preset & 3) == 2) {\n"
    "        c0 = vec3(0.0, 0.95, -0.1);\n"
    "        c2 = vec3(0.0, 0.35, 0.0);\n"
    "        c3 = vec3(-2.05, 0.28, 1.85);\n"
    "    }\n"
    "\n"
    "    float r0 = 1.0;\n"
    "    float r1 = 0.45;\n"
    "    float r2 = 0.38;\n"
    "    float r3 = 0.22;\n"
    "    if ((preset & 3) == 1) { r0 = 0.85; r1 = 0.38; r2 = 0.32; r3 = 0.2; }\n"
    "    if ((preset & 3) == 2) { r0 = 1.15; r1 = 0.42; r2 = 0.55; r3 = 0.24; }\n"
    "\n"
    "    vec3 bmin = vec3(-2.88, 0.0, 1.22);\n"
    "    vec3 bmax = vec3(-2.02, 1.08, 2.08);\n"
    "    vec2 cylC = vec2(2.42, 1.48);\n"
    "    float cylR = 0.3;\n"
    "    float cylY0 = 0.0;\n"
    "    float cylY1 = 1.18;\n"
    "    if ((preset & 3) == 1) {\n"
    "        bmin = vec3(1.32, 0.0, -2.12);\n"
    "        bmax = vec3(2.18, 0.82, -1.32);\n"
    "        cylC = vec2(-2.48, -1.42);\n"
    "        cylR = 0.28;\n"
    "        cylY1 = 1.02;\n"
    "    }\n"
    "    if ((preset & 3) == 2) {\n"
    "        bmin = vec3(-0.48, 0.0, 1.95);\n"
    "        bmax = vec3(0.48, 1.32, 2.88);\n"
    "        cylC = vec2(2.28, -1.92);\n"
    "        cylR = 0.34;\n"
    "        cylY1 = 1.35;\n"
    "    }\n"
    "\n"
    "    vec3 bn;\n"
    "    if (boxAabb(ro, rd, bmin, bmax, t, bn) && t < h.t) {\n"
    "        h.t = t; h.id = 5; h.p = ro + rd * t; h.n = bn;\n"
    "    }\n"
    "    if (cylYFinite(ro, rd, cylC, cylR, cylY0, cylY1, t, bn) && t < h.t) {\n"
    "        h.t = t; h.id = 6; h.p = ro + rd * t; h.n = bn;\n"
    "    }\n"
    "\n"
    "    if (sphIntersect(ro, rd, c0, r0, t) && t < h.t) {\n"
    "        h.t = t; h.id = 2; h.p = ro + rd * t; h.n = normalize(h.p - c0);\n"
    "    }\n"
    "    if (sphIntersect(ro, rd, c1, r1, t) && t < h.t) {\n"
    "        h.t = t; h.id = 3; h.p = ro + rd * t; h.n = normalize(h.p - c1);\n"
    "    }\n"
    "    if (sphIntersect(ro, rd, c2, r2, t) && t < h.t) {\n"
    "        h.t = t; h.id = 4; h.p = ro + rd * t; h.n = normalize(h.p - c2);\n"
    "    }\n"
    "    if (sphIntersect(ro, rd, c3, r3, t) && t < h.t) {\n"
    "        h.t = t; h.id = 7; h.p = ro + rd * t; h.n = normalize(h.p - c3);\n"
    "    }\n"
    "    return h;\n"
    "}\n"
    "\n"
    "float shadowRay(vec3 ro, vec3 rd, float tm, int preset) {\n"
    "    Hit h = sceneIntersect(ro, rd, tm, preset);\n"
    "    return h.id == 0 ? 1.0 : 0.0;\n"
    "}\n"
    "\n"
    "vec3 shadePrimary(Hit h, vec3 rd, vec3 sunDir, float tm, int preset) {\n"
    "    vec3 base = vec3(0.72, 0.74, 0.78);\n"
    "    float specPw = 64.0;\n"
    "\n"
    "    if (h.id == 1) {\n"
    "        float cx = floor(h.p.x * 1.1);\n"
    "        float cz = floor(h.p.z * 1.1);\n"
    "        float ch = mod(cx + cz, 2.0);\n"
    "        vec3 a = vec3(0.09, 0.09, 0.11);\n"
    "        vec3 b = vec3(0.92, 0.92, 0.94);\n"
    "        if ((preset & 3) == 1) { a = vec3(0.06, 0.06, 0.08); b = vec3(0.55, 0.52, 0.62); }\n"
    "        if ((preset & 3) == 2) { a = vec3(0.02, 0.08, 0.07); b = vec3(0.15, 0.85, 0.72); }\n"
    "        base = mix(a, b, ch);\n"
    "        specPw = 48.0;\n"
    "    } else if (h.id == 2) {\n"
    "        base = vec3(0.18, 0.42, 0.95);\n"
    "        if ((preset & 3) == 1) base = vec3(0.92, 0.35, 0.28);\n"
    "        if ((preset & 3) == 2) base = vec3(0.95, 0.25, 0.65);\n"
    "        specPw = 128.0;\n"
    "    } else if (h.id == 3) {\n"
    "        base = vec3(0.35, 0.9, 0.45);\n"
    "        if ((preset & 3) == 1) base = vec3(0.35, 0.75, 0.95);\n"
    "        if ((preset & 3) == 2) base = vec3(0.95, 0.85, 0.25);\n"
    "        specPw = 96.0;\n"
    "    } else if (h.id == 4) {\n"
    "        base = vec3(0.85, 0.55, 0.18);\n"
    "        if ((preset & 3) == 1) base = vec3(0.75, 0.72, 0.78);\n"
    "        if ((preset & 3) == 2) base = vec3(0.45, 0.25, 0.95);\n"
    "        specPw = 72.0;\n"
    "    } else if (h.id == 5) {\n"
    "        base = vec3(0.88, 0.82, 0.72);\n"
    "        if ((preset & 3) == 1) base = vec3(0.62, 0.58, 0.68);\n"
    "        if ((preset & 3) == 2) base = vec3(0.75, 0.9, 0.82);\n"
    "        specPw = 56.0;\n"
    "    } else if (h.id == 6) {\n"
    "        base = vec3(0.55, 0.58, 0.62);\n"
    "        if ((preset & 3) == 1) base = vec3(0.72, 0.7, 0.68);\n"
    "        if ((preset & 3) == 2) base = vec3(0.35, 0.85, 0.95);\n"
    "        specPw = 110.0;\n"
    "    } else if (h.id == 7) {\n"
    "        base = vec3(0.95, 0.35, 0.42);\n"
    "        if ((preset & 3) == 1) base = vec3(0.4, 0.85, 0.45);\n"
    "        if ((preset & 3) == 2) base = vec3(0.98, 0.55, 0.2);\n"
    "        specPw = 88.0;\n"
    "    }\n"
    "\n"
    "    vec3 N = normalize(h.n);\n"
    "    vec3 L = sunDir;\n"
    "    float diff = max(dot(N, L), 0.0);\n"
    "    vec3 V = normalize(-rd);\n"
    "    vec3 Hv = normalize(L + V);\n"
    "    float spec = pow(max(dot(N, Hv), 0.0), specPw);\n"
    "\n"
    "    float sha = 1.0;\n"
    "    if (diff > 0.0) {\n"
    "        float eps = 0.02;\n"
    "        sha = shadowRay(h.p + N * eps, L, tm, preset);\n"
    "    }\n"
    "\n"
    "    vec3 amb = vec3(0.04, 0.045, 0.06);\n"
    "    if ((preset & 3) == 2) amb = vec3(0.02, 0.06, 0.05);\n"
    "\n"
    "    vec3 col = base * (amb + sha * diff * vec3(1.0, 0.98, 0.94));\n"
    "    col += sha * spec * vec3(1.0) * 0.35;\n"
    "    return col;\n"
    "}\n"
    "\n"
    "vec3 trace(vec3 ro, vec3 rd, float tm, int preset) {\n"
    "    Hit h = sceneIntersect(ro, rd, tm, preset);\n"
    "    if (h.id == 0) return skyColor(rd, preset, tm);\n"
    "\n"
    "    vec3 sunDir = normalize(vec3(0.35, 0.92, 0.25));\n"
    "    vec3 col = shadePrimary(h, rd, sunDir, tm, preset);\n"
    "\n"
    "    vec3 N = normalize(h.n);\n"
    "    vec3 V = normalize(-rd);\n"
    "    float reflBase = 0.08;\n"
    "    if (h.id == 1) reflBase = 0.22;\n"
    "    if (h.id >= 2) reflBase = 0.5;\n"
    "    float fres = reflBase + (1.0 - reflBase) * pow(1.0 - max(dot(N, V), 0.0), 5.0);\n"
    "\n"
    "    vec3 rdir = reflect(rd, N);\n"
    "    Hit h2 = sceneIntersect(h.p + N * 0.04, rdir, tm, preset);\n"
    "    vec3 reflCol;\n"
    "    if (h2.id == 0) {\n"
    "        reflCol = skyColor(rdir, preset, tm);\n"
    "    } else {\n"
    "        reflCol = shadePrimary(h2, rdir, sunDir, tm, preset);\n"
    "        vec3 N2 = normalize(h2.n);\n"
    "        vec3 V2 = normalize(-rdir);\n"
    "        float f2 = 0.15 + 0.85 * pow(1.0 - max(dot(N2, V2), 0.0), 5.0);\n"
    "        vec3 r2 = reflect(rdir, N2);\n"
    "        Hit h3 = sceneIntersect(h2.p + N2 * 0.04, r2, tm, preset);\n"
    "        vec3 rcol2 = (h3.id == 0) ? skyColor(r2, preset, tm) : shadePrimary(h3, r2, sunDir, tm, preset);\n"
    "        reflCol = mix(reflCol, rcol2, f2 * 0.55);\n"
    "    }\n"
    "\n"
    "    col = mix(col, reflCol, clamp(fres * 0.92, 0.0, 1.0));\n"
    "    return col;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    float w = max(uResolution.x, 1.0);\n"
    "    float h = max(uResolution.y, 1.0);\n"
    "    vec2 ndc;\n"
    "    ndc.x = (2.0 * (gl_FragCoord.x + 0.5) / w - 1.0);\n"
    "    ndc.y = (2.0 * (gl_FragCoord.y + 0.5) / h - 1.0);\n"
    "\n"
    "    vec4 wp = uInvVP * vec4(ndc, -1.0, 1.0);\n"
    "    wp.xyz /= wp.w;\n"
    "    vec3 ro = uCameraPos;\n"
    "    vec3 rd = normalize(wp.xyz - ro);\n"
    "\n"
    "    float tm = uTime;\n"
    "    int preset = uPreset;\n"
    "    vec3 col = trace(ro, rd, tm, preset);\n"
    "\n"
    "    float g = hash11(dot(gl_FragCoord.xy, vec2(12.9898, 78.233)));\n"
    "    col += (g - 0.5) * 0.01;\n"
    "\n"
    "    col = col / (col + vec3(1.0));\n"
    "    col = pow(col, vec3(1.0 / 2.2));\n"
    "\n"
    "    vec2 q = (gl_FragCoord.xy - 0.5 * uResolution) / min(w, h);\n"
    "    col *= 1.0 - dot(q, q) * 0.18;\n"
    "\n"
    "    fragColor = vec4(col, 1.0);\n"
    "}\n";

static int init_gpu(void) {
    SituationError err = SituationLoadShaderFromMemory(k_vs, k_fs, &g_shader);
    if (err != SITUATION_SUCCESS) {
        char* msg = NULL;
        SituationGetLastErrorMsg(&msg);
        fprintf(stderr, "[shader_lab_raytrace] Shader compile failed: %s\n", msg ? msg : "?");
        if (msg) {
            SituationFreeString(msg);
        }
        return -1;
    }

    RtvVertex tri[3] = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{3.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-1.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    };
    uint32_t ix[3] = {0, 1, 2};
    err = SituationCreateMesh(tri, 3, sizeof(RtvVertex), ix, 3, &g_fs_mesh);
    if (err != SITUATION_SUCCESS) {
        char* msg = NULL;
        SituationGetLastErrorMsg(&msg);
        fprintf(stderr, "[shader_lab_raytrace] Mesh failed: %s\n", msg ? msg : "?");
        if (msg) {
            SituationFreeString(msg);
        }
        SituationUnloadShader(&g_shader);
        return -1;
    }
    return 0;
}

static void ui_draw_line_centered(SituationCommandBuffer cmd, SituationFont* font, const char* text,
                                  float screen_w, float y, float font_size, ColorRGBA color) {
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
        .window_title = "Situation — Shader Lab (Ray trace)",
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

    float yaw = 0.85f;
    float pitch = 0.32f;
    float radius = 6.5f;
    vec3 target = {0.0f, 0.65f, 0.0f};
    bool anim = true;
    float time_scale = 1.0f;
    int preset = 0;
    bool vsync_on = true;
    SituationSetVSync(true);

    static const char* k_preset_names[] = {"jewel box", "studio", "neon pool"};

    printf(
        "Shader Lab — GPU ray trace (on-screen help at bottom)\n"
        "  F12: PNG screenshot   V: VSync   1-3: scene presets\n");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        float dt = SituationGetFrameTime();
        float rw = (float)SituationGetRenderWidth();
        float rh = (float)SituationGetRenderHeight();
        float aspect = rh > 1.0f ? rw / rh : 1.0f;

        if (SituationIsKeyPressed(SIT_KEY_R)) {
            yaw = 0.85f;
            pitch = 0.32f;
            radius = 6.5f;
        }
        if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
            anim = !anim;
        }
        if (SituationIsKeyPressed(SIT_KEY_1)) {
            preset = 0;
        }
        if (SituationIsKeyPressed(SIT_KEY_2)) {
            preset = 1;
        }
        if (SituationIsKeyPressed(SIT_KEY_3)) {
            preset = 2;
        }
        if (SituationIsKeyPressed(SIT_KEY_V)) {
            vsync_on = !vsync_on;
            SituationSetVSync(vsync_on);
        }

        if (SituationIsMouseButtonDown(0)) {
            Vector2 d = SituationGetMouseDelta();
            yaw -= d.x * 0.005f;
            pitch -= d.y * 0.005f;
            float lim = 1.45f;
            if (pitch > lim) {
                pitch = lim;
            }
            if (pitch < -lim) {
                pitch = -lim;
            }
        }

        radius -= SituationGetMouseWheelMove() * 0.45f;
        if (radius < 2.5f) {
            radius = 2.5f;
        }
        if (radius > 18.0f) {
            radius = 18.0f;
        }

        if (anim) {
            time_scale += dt;
        }

        vec3 eye;
        orbit_eye(yaw, pitch, radius, target, eye);

        mat4 proj;
        glm_perspective(glm_rad(55.0f), aspect, 0.08f, 200.0f, proj);

        mat4 view;
        glm_lookat(eye, target, (vec3){0.0f, 1.0f, 0.0f}, view);

        mat4 vp;
        glm_mat4_mul(proj, view, vp);
        mat4 inv_vp;
        glm_mat4_inv(vp, inv_vp);

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = {.color = {8, 10, 16, 255}},
                },
                .depth_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = {.depth = 1.0f},
                },
            };

            SituationCmdBeginRenderPass(cmd, &pass);

            SituationCmdBindPipeline(cmd, g_shader);
            float res[2] = {rw, rh};
            SituationSetShaderUniform(g_shader, "uCameraPos", eye, SIT_UNIFORM_VEC3);
            SituationSetShaderUniform(g_shader, "uInvVP", inv_vp, SIT_UNIFORM_MAT4);
            SituationSetShaderUniform(g_shader, "uResolution", res, SIT_UNIFORM_VEC2);
            SituationSetShaderUniform(g_shader, "uTime", &time_scale, SIT_UNIFORM_FLOAT);
            SituationSetShaderUniform(g_shader, "uPreset", &preset, SIT_UNIFORM_INT);

            SituationCmdDrawMesh(cmd, g_fs_mesh);

            {
                const float fs = 14.0f;
                const float lh = 17.0f;
                const float margin_bot = 14.0f;
                int fps = SituationGetFPS();
                char line[192];

                float y = rh - margin_bot - lh * 3.5f;
                ColorRGBA hi = {230, 235, 245, 255};
                ColorRGBA dim = {160, 175, 195, 255};

                snprintf(line, sizeof line, "GPU ray trace   FPS %d   VSync %s   [V]",
                         fps, vsync_on ? "ON" : "OFF");
                ui_draw_line_centered(cmd, &ui_font, line, rw, y, fs, hi);
                y += lh;

                snprintf(line, sizeof line, "Scene: %s   [1][2][3]   anim %s [Space]",
                         k_preset_names[preset < 3 ? preset : 0], anim ? "ON" : "PAUSED");
                ui_draw_line_centered(cmd, &ui_font, line, rw, y, fs, hi);
                y += lh;

                ui_draw_line_centered(cmd, &ui_font,
                    "LMB orbit   Wheel zoom   R reset   F12 PNG", rw, y, fs, dim);
            }

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();

            if (SituationIsKeyPressed(SIT_KEY_F12)) {
                static int screenshot_seq = 0;
                char path[256];
                time_t now = time(NULL);
                snprintf(path, sizeof path, "shader_lab_ray_%lld_%04d.png", (long long)now, screenshot_seq++);
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
    SituationDestroyMesh(&g_fs_mesh);
    SituationUnloadShader(&g_shader);
    SituationShutdown();
    return 0;
}
