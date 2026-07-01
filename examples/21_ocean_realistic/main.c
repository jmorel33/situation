/***************************************************************************************************
 *  Example 21 — Realistic Ocean (Advanced Shader)
 *  ------------------------------------------------
 *  Original work — MIT (Situation). Visual inspiration only:
 *    Shadertoy 4sXGRM, 4dSBDt (Thomas Schander), MdXyzX — no third-party GLSL included.
 *  Host scaffolding derived from examples/other/shader_lab_raytrace2.c (MIT).
 *
 *  OpenGL / Vulkan ray-dir march: atmosphere + volumetric clouds + height-field sea.
 *  The sun is a world-space emissive sphere (SUN_DIST / SUN_RADIUS) along uSunDir — ray traced, not a post pass.
 *
 *  Build:
 *    build\build_examples.bat static-opengl 21_ocean_realistic
 *    build\build_examples.bat static-vulkan 21_ocean_realistic
 *
 *  Controls
 *    (default)     Camera travel along path
 *    T               Toggle travel / stationary orbit
 *    LMB drag        Orbit override (while held)
 *    Mouse wheel     Travel speed (travel on) or zoom (travel off)
 *    Space           Pause / resume animation + travel
 *    1 / 2 / 3       Sea preset: Calm / Moderate / Storm
 *    , / .           Cloud coverage down / up
 *    [ / ]           Fine-tune sea chop
 *    R               Reset camera to path start
 *    V               Toggle VSync
 *    F12             Save PNG screenshot
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
} OceanVertex;

/* std140 OceanFrame UBO — field order must match GLSL block in k_fs (no mat4 — rays built on camera basis). */
typedef struct OceanFrameUbo {
    float u_sea_state[4];
    float u_cloud_state[4];
    float u_camera_pos[4];
    float u_camera_target[4];
    float u_sun_dir[4];
    float u_resolution[2];
    float u_time;
    float u_travel_phase;
    float u_fov_tan;
    float u_exposure;
} OceanFrameUbo;

#define OCEAN_FRAME_UBO_BYTES ((size_t)sizeof(OceanFrameUbo))

static SituationShader g_shader = {0};
static SituationMesh g_fs_mesh = {0};
static SituationBuffer g_frame_ubo = {0};
static bool g_use_vertex_pull = false;

static const char* k_vs =
    "#version 450 core\n"
    "layout(location = 0) in vec3 inPos;\n"
    "void main() {\n"
    "    gl_Position = vec4(inPos.xy, 1.0 - 1.0e-4, 1.0);\n"
    "}\n";

#if defined(SITUATION_USE_VULKAN)
static const char* k_vs_pull =
    "#version 450\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_buffer_reference2 : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "layout(location = 0) in vec3 inPosAttr;\n"
    "layout(push_constant) uniform PC { uint64_t vertex_address; uint64_t index_address; } pc;\n"
    "struct SitVertex_Legacy { vec3 position; vec3 normal; vec2 texcoord; };\n"
    "layout(buffer_reference, scalar) readonly buffer SitVertexBuffer_Legacy { SitVertex_Legacy data[]; };\n"
    "void main() {\n"
    "    SitVertexBuffer_Legacy verts = SitVertexBuffer_Legacy(pc.vertex_address);\n"
    "    vec3 inPos = verts.data[gl_VertexIndex].position + inPosAttr * 0.0;\n"
    "    gl_Position = vec4(inPos.xy, 1.0 - 1.0e-4, 1.0);\n"
    "}\n";
#endif

/* Original GLSL — sky, horizon clouds, tiered sea march. OceanFrame UBO for GL + Vulkan. */
static const char* k_fs =
    "#version 450 core\n"
    "#if defined(SITUATION_USE_VULKAN)\n"
    "layout(std140, set = 0, binding = 0) uniform OceanFrame {\n"
    "#else\n"
    "layout(std140, binding = 0) uniform OceanFrame {\n"
    "#endif\n"
    "    vec4 uSeaState;\n"
    "    vec4 uCloudState;\n"
    "    vec4 uCameraPos;\n"
    "    vec4 uCameraTarget;\n"
    "    vec4 uSunDir;\n"
    "    vec2 uResolution;\n"
    "    float uTime;\n"
    "    float uTravelPhase;\n"
    "    float uFovTan;\n"
    "    float uExposure;\n"
    "} frame;\n"
    "layout(location = 0) out vec4 fragColor;\n"
    "\n"
    "/* OpenGL: stay under driver ~65k fragment-instruction cap. Vulkan: megashader tier. */\n"
    "#if defined(SITUATION_USE_VULKAN)\n"
    "const int FBM_OCTAVES = 5;\n"
    "const int CLOUD_MARCH_STEPS = 14;\n"
    "const int CLOUD_SHADOW_STEPS = 8;\n"
    "#else\n"
    "const int FBM_OCTAVES = 3;\n"
    "const int CLOUD_MARCH_STEPS = 7;\n"
    "const int CLOUD_SHADOW_STEPS = 4;\n"
    "#endif\n"
    "\n"
    "float hash11(float p) {\n"
    "    p = fract(p * 0.1031);\n"
    "    p *= p + 33.33;\n"
    "    return fract(p * p);\n"
    "}\n"
    "\n"
    "float hash21(vec2 p) {\n"
    "    return hash11(dot(p, vec2(127.1, 311.7)));\n"
    "}\n"
    "\n"
    "float hash31(vec3 p) {\n"
    "    return hash11(dot(p, vec3(127.1, 311.7, 74.7)));\n"
    "}\n"
    "\n"
    "float noise3(vec3 p) {\n"
    "    vec3 i = floor(p);\n"
    "    vec3 f = fract(p);\n"
    "    f = f * f * (3.0 - 2.0 * f);\n"
    "    float n000 = hash31(i);\n"
    "    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));\n"
    "    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));\n"
    "    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));\n"
    "    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));\n"
    "    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));\n"
    "    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));\n"
    "    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));\n"
    "    float nx00 = mix(n000, n100, f.x);\n"
    "    float nx10 = mix(n010, n110, f.x);\n"
    "    float nx01 = mix(n001, n101, f.x);\n"
    "    float nx11 = mix(n011, n111, f.x);\n"
    "    float nxy0 = mix(nx00, nx10, f.y);\n"
    "    float nxy1 = mix(nx01, nx11, f.y);\n"
    "    return mix(nxy0, nxy1, f.z);\n"
    "}\n"
    "\n"
    "float fbmCloud(vec3 p) {\n"
    "    float v = 0.0;\n"
    "    float a = 0.5;\n"
    "    for (int i = 0; i < FBM_OCTAVES; i++) {\n"
    "        v += a * noise3(p);\n"
    "        p = p * 2.12 + vec3(1.7, 2.3, 0.6);\n"
    "        a *= 0.5;\n"
    "    }\n"
    "    return v;\n"
    "}\n"
    "\n"
    "const float CLOUD_BOT = 4.0;\n"
    "const float CLOUD_TOP = 22.0;\n"
    "const float SUN_DIST = 5000.0;\n"
    "const float SUN_RADIUS = 90.0;\n"
    "\n"
    "vec3 sunCenter() {\n"
    "    return normalize(frame.uSunDir.xyz) * SUN_DIST;\n"
    "}\n"
    "\n"
    "bool sphHit(vec3 ro, vec3 rd, vec3 c, float r, out float t) {\n"
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
    "vec3 skyGradient(vec3 rd) {\n"
    "    vec3 S = normalize(frame.uSunDir.xyz);\n"
    "    float g = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);\n"
    "    float mu = max(dot(normalize(rd), S), 0.0);\n"
    "    float sunLift = pow(mu, 4.0) * 1.1 + pow(mu, 16.0) * 3.5;\n"
    "    vec3 zen = vec3(0.02, 0.18, 0.62) + vec3(sunLift * 0.28);\n"
    "    vec3 hor = vec3(0.55, 0.68, 0.92) + vec3(sunLift * 1.4, sunLift * 1.0, sunLift * 0.22);\n"
    "    vec3 sky = mix(hor, zen, pow(g, 0.95));\n"
    "    sky += vec3(1.0, 0.72, 0.28) * pow(mu, 12.0) * 0.45;\n"
    "    return sky;\n"
    "}\n"
    "\n"
    "vec3 horizonCloudDeck(vec3 rd, float tm) {\n"
    "    if (rd.y < 0.03) return vec3(0.0);\n"
    "    float elev = rd.y;\n"
    "    vec2 uv = rd.xz / (elev + 0.12) * 0.07;\n"
    "    uv += vec2(tm * frame.uCloudState.y * 0.32, tm * frame.uCloudState.y * 0.04);\n"
    "    float n = fbmCloud(vec3(uv * 2.8, 0.15));\n"
    "    n += 0.35 * fbmCloud(vec3(uv * 5.6 + 1.7, 0.4));\n"
    "    float cov = frame.uCloudState.x;\n"
    "    float d = smoothstep(cov - 0.10, cov + 0.14, n);\n"
    "    d *= smoothstep(0.03, 0.28, elev) * (1.0 - smoothstep(0.55, 0.95, elev));\n"
    "    vec3 base = mix(vec3(0.48, 0.54, 0.64), vec3(0.86, 0.90, 0.95), d);\n"
    "    return base * d * 0.72 * frame.uCloudState.z;\n"
    "}\n"
    "\n"
    "float cloudDensityAt(vec3 pos, float tm) {\n"
    "    float speed = frame.uCloudState.y;\n"
    "    vec3 wind = vec3(tm * speed * 0.42, tm * speed * 0.05, tm * speed * 0.28);\n"
    "    vec3 p = pos * 0.065 + wind;\n"
    "    float n = fbmCloud(p);\n"
    "    n += 0.4 * fbmCloud(p * 2.25 + vec3(3.4, 1.1, 2.2));\n"
    "#if defined(SITUATION_USE_VULKAN)\n"
    "    n += 0.22 * fbmCloud(p * 4.05 + vec3(8.2, 2.4, 5.1));\n"
    "    n += 0.12 * fbmCloud(p * 7.8 + vec3(1.6, 9.2, 3.7));\n"
    "#endif\n"
    "    float coverage = frame.uCloudState.x;\n"
    "    return smoothstep(coverage - 0.10, coverage + 0.12, n);\n"
    "}\n"
    "\n"
    "float cloudSunTrans(vec3 pos, vec3 sunDir, float tm) {\n"
    "    float trans = 1.0;\n"
    "    for (int j = 0; j < CLOUD_SHADOW_STEPS; j++) {\n"
    "        float lt = 1.8 + float(j) * 3.2;\n"
    "        vec3 lp = pos + sunDir * lt;\n"
    "        trans *= exp(-cloudDensityAt(lp, tm) * 1.6);\n"
    "    }\n"
    "    return max(trans, 0.50);\n"
    "}\n"
    "\n"
    "float cloudTransAlong(vec3 ro, vec3 rd, float tMax, float tm) {\n"
    "    float trans = 1.0;\n"
    "    const int N = 5;\n"
    "    for (int i = 0; i < N; i++) {\n"
    "        float u = tMax * (float(i) + 0.5) / float(N);\n"
    "        vec3 p = ro + rd * u;\n"
    "        if (p.y >= CLOUD_BOT && p.y <= CLOUD_TOP)\n"
    "            trans *= exp(-cloudDensityAt(p, tm) * 0.42);\n"
    "    }\n"
    "    return trans;\n"
    "}\n"
    "\n"
    "vec3 shadeSun(vec3 ro, vec3 rd, float tHit, float tm) {\n"
    "    vec3 C = sunCenter();\n"
    "    vec3 p = ro + rd * tHit;\n"
    "    vec3 N = normalize(p - C);\n"
    "    vec3 toSun = normalize(C - ro);\n"
    "    float limb = 0.68 + 0.32 * max(dot(N, toSun), 0.0);\n"
    "    float occ = cloudTransAlong(ro, rd, tHit, tm);\n"
    "    return vec3(1.0, 0.86, 0.40) * 12.0 * limb * occ;\n"
    "}\n"
    "\n"
    "vec3 marchClouds(vec3 ro, vec3 rd, float tm, out float outTrans, out float outGod) {\n"
    "    vec3 S = normalize(frame.uSunDir.xyz);\n"
    "    vec3 nrd = normalize(rd);\n"
    "    outTrans = 1.0;\n"
    "    outGod = 0.0;\n"
    "    if (abs(nrd.y) < 1.0e-5) return vec3(0.0);\n"
    "    float tA = (CLOUD_BOT - ro.y) / nrd.y;\n"
    "    float tB = (CLOUD_TOP - ro.y) / nrd.y;\n"
    "    float tNear = min(tA, tB);\n"
    "    float tFar = max(tA, tB);\n"
    "    if (tFar < 0.0) return vec3(0.0);\n"
    "    tNear = max(tNear, 0.0);\n"
    "    float trans = 1.0;\n"
    "    vec3 accum = vec3(0.0);\n"
    "    float god = 0.0;\n"
    "    const int STEPS = CLOUD_MARCH_STEPS;\n"
    "    float dt = (tFar - tNear) / float(STEPS);\n"
    "    float muViewSun = max(dot(nrd, S), 0.0);\n"
    "    for (int i = 0; i < STEPS; i++) {\n"
    "        float t = tNear + (float(i) + 0.5) * dt;\n"
    "        vec3 pos = ro + nrd * t;\n"
    "        float dens = cloudDensityAt(pos, tm);\n"
    "        if (dens < 0.008) continue;\n"
    "        float sunT = cloudSunTrans(pos, S, tm);\n"
    "        float depth = clamp((pos.y - CLOUD_BOT) / (CLOUD_TOP - CLOUD_BOT), 0.0, 1.0);\n"
    "        vec3 base = mix(vec3(0.58, 0.64, 0.74), vec3(0.90, 0.93, 0.97), depth);\n"
    "        base = mix(base, vec3(0.42, 0.46, 0.52), frame.uCloudState.w * 0.35);\n"
    "        vec3 lit = base * (0.45 + 0.65 * sunT);\n"
    "        lit += vec3(1.0, 0.85, 0.55) * sunT * pow(max(dot(S, vec3(0.0, 1.0, 0.0)), 0.0), 0.32) * 0.55;\n"
    "        float rim = pow(max(dot(normalize(pos - ro), S), 0.0), 2.5) * dens * 0.45;\n"
    "        lit += vec3(1.0, 0.92, 0.72) * rim * sunT;\n"
    "        float alpha = dens * 0.12 * frame.uCloudState.z;\n"
    "        accum += trans * lit * alpha;\n"
    "        god += trans * sunT * (1.0 - dens * 0.65) * alpha;\n"
    "        trans *= 1.0 - alpha;\n"
    "        if (trans < 0.03) break;\n"
    "    }\n"
    "    outTrans = trans;\n"
    "    outGod = god * pow(muViewSun, 4.0) * smoothstep(-0.05, 0.55, nrd.y) * 1.8;\n"
    "    return accum;\n"
    "}\n"
    "\n"
    "vec3 sampleAtmosphere(vec3 ro, vec3 rd, float tm) {\n"
    "    vec3 nrd = normalize(rd);\n"
    "    vec3 col = skyGradient(nrd);\n"
    "    col += horizonCloudDeck(nrd, tm);\n"
    "    float cloudTrans;\n"
    "    float god;\n"
    "    vec3 cloudCol = marchClouds(ro, nrd, tm, cloudTrans, god);\n"
    "    col = col * cloudTrans + cloudCol;\n"
    "    col += vec3(1.0, 0.85, 0.55) * god * 0.85;\n"
    "    return col;\n"
    "}\n"
    "\n"
    "vec3 sampleSkyFrom(vec3 ro, vec3 rd, float tm) {\n"
    "    float tSun;\n"
    "    if (sphHit(ro, rd, sunCenter(), SUN_RADIUS, tSun))\n"
    "        return shadeSun(ro, rd, tSun, tm);\n"
    "    return sampleAtmosphere(ro, rd, tm);\n"
    "}\n"
    "\n"
    "vec3 sampleSky(vec3 rd, float tm) {\n"
    "    return sampleSkyFrom(frame.uCameraPos.xyz, rd, tm);\n"
    "}\n"
    "\n"
    "float cloudShadowAt(vec3 p, float tm) {\n"
    "    vec3 S = normalize(frame.uSunDir.xyz);\n"
    "    vec3 origin = p + vec3(0.0, 5.0, 0.0);\n"
    "    return cloudSunTrans(origin, S, tm);\n"
    "}\n"
    "\n"
    "const float GA = 2.39996323;\n"
    "\n"
    "float waveDir(vec2 xz, vec2 dir, float freq, float phase) {\n"
    "    return sin(dot(dir, xz) * freq + phase);\n"
    "}\n"
    "\n"
    "float seaHeightBase(vec2 xz, float tm, bool detail) {\n"
    "    float wind = frame.uSeaState.x;\n"
    "    float chop = frame.uSeaState.y;\n"
    "    float amp = frame.uSeaState.z * (0.30 + chop * 1.25);\n"
    "    float ripple = frame.uSeaState.w;\n"
    "    float t = tm * (0.95 + wind * 1.10);\n"
    "    vec2 d0 = normalize(vec2(0.94, 0.34));\n"
    "    vec2 d1 = normalize(vec2(-0.48, 0.88));\n"
    "    vec2 d2 = normalize(vec2(0.18, -0.98));\n"
    "    vec2 d3 = normalize(vec2(0.72, 0.69));\n"
    "    float h = 0.0;\n"
    "    h += amp * waveDir(xz, d0, 0.62, t * 0.82);\n"
    "    h += amp * 0.72 * waveDir(xz, d1, 1.05, -t * 0.74);\n"
    "    h += amp * 0.48 * waveDir(xz, d2, 1.85, t * 1.12);\n"
    "    h += amp * 0.34 * waveDir(xz, d3, 3.2, -t * 1.35);\n"
    "#if defined(SITUATION_USE_VULKAN)\n"
    "    for (int i = 0; i < 3; i++) {\n"
    "        float ang = float(i) * GA;\n"
    "        vec2 dir = vec2(cos(ang), sin(ang));\n"
    "        h += amp * 0.18 * waveDir(xz, dir, 0.85 + float(i) * 0.35, t * (1.1 + float(i) * 0.2));\n"
    "    }\n"
    "#endif\n"
    "    if (detail) {\n"
    "        h += amp * 0.14 * waveDir(xz, d0, 5.4, t * 2.05);\n"
    "        h += amp * 0.09 * waveDir(xz, d1, 7.2, -t * 1.75);\n"
    "        h += amp * 0.06 * waveDir(xz, d2, 9.5, t * 2.35);\n"
    "    }\n"
    "    if (chop < 0.12) {\n"
    "        h *= chop / 0.12;\n"
    "        h += ripple * 0.016 * waveDir(xz, d0, 18.0, t * 2.5);\n"
    "        h += ripple * 0.012 * waveDir(xz, d1, 21.0, -t * 2.1);\n"
    "        if (detail) {\n"
    "            h += ripple * 0.008 * waveDir(xz, d3, 26.0, t * 3.0);\n"
    "        }\n"
    "    }\n"
    "    return h;\n"
    "}\n"
    "\n"
    "float seaHeight(vec2 xz, float tm) {\n"
    "    return seaHeightBase(xz, tm, false);\n"
    "}\n"
    "\n"
    "vec3 seaNormal(vec2 xz, float tm) {\n"
    "    const float e = 0.045;\n"
    "    float hx = seaHeightBase(xz + vec2(e, 0.0), tm, true)\n"
    "             - seaHeightBase(xz - vec2(e, 0.0), tm, true);\n"
    "    float hz = seaHeightBase(xz + vec2(0.0, e), tm, true)\n"
    "             - seaHeightBase(xz - vec2(0.0, e), tm, true);\n"
    "    return normalize(vec3(-hx / (2.0 * e), 1.0, -hz / (2.0 * e)));\n"
    "}\n"
    "\n"
    "bool rayPlaneY(vec3 ro, vec3 rd, float y, out float t) {\n"
    "    if (abs(rd.y) < 1.0e-6) return false;\n"
    "    t = (y - ro.y) / rd.y;\n"
    "    return t > 1.0e-4;\n"
    "}\n"
    "\n"
    "vec3 shadeSea(vec3 p, vec3 rd, float tm) {\n"
    "    float chop = frame.uSeaState.y;\n"
    "    float calm = 1.0 - smoothstep(0.0, 0.30, chop);\n"
    "    float dist = length(frame.uCameraPos.xyz - p);\n"
    "    vec3 N = seaNormal(p.xz, tm);\n"
    "    N = normalize(mix(N, vec3(0.0, 1.0, 0.0), smoothstep(50.0, 140.0, dist) * 0.25));\n"
    "    vec3 V = normalize(-rd);\n"
    "    const float F0 = 0.02;\n"
    "    float cosI = clamp(dot(N, V), 0.0, 1.0);\n"
    "    float fres = F0 + (1.0 - F0) * pow(1.0 - cosI, 5.0);\n"
    "    vec3 deep = vec3(0.01, 0.14, 0.32);\n"
    "    vec3 shallow = vec3(0.05, 0.48, 0.58);\n"
    "    vec3 body = mix(deep, shallow, fres * 0.70 + 0.22);\n"
    "    float absorb = exp(-dist * 0.014 * (1.05 - fres * 0.35));\n"
    "    body *= vec3(absorb * 0.88, absorb, absorb * 1.08);\n"
    "    float sha = mix(0.62, 1.0, cloudShadowAt(p, tm));\n"
    "    body *= sha;\n"
    "    vec3 reflDir = reflect(rd, N);\n"
    "    vec3 reflOrigin = p + N * 0.02;\n"
    "    vec3 refl = sampleSkyFrom(reflOrigin, reflDir, tm);\n"
    "    float reflAmt = mix(0.65 + fres * 0.28, 0.32 + fres * 0.52, smoothstep(0.08, 0.70, chop));\n"
    "    vec3 col = mix(body, refl, clamp(reflAmt, 0.0, 1.0));\n"
    "    vec3 L = normalize(frame.uSunDir.xyz);\n"
    "    vec3 H = normalize(L + V);\n"
    "    float specPw = mix(mix(720.0, 280.0, calm), 80.0, smoothstep(0.20, 1.0, chop));\n"
    "    float spec = pow(max(dot(N, H), 0.0), specPw);\n"
    "    col += sha * spec * vec3(1.0, 0.95, 0.82) * mix(0.85, 0.38, chop);\n"
    "    float sunGl = pow(max(dot(reflDir, L), 0.0), mix(120.0, 36.0, chop));\n"
    "    col += sha * sunGl * vec3(1.0, 0.88, 0.55) * mix(5.5, 2.0, chop);\n"
    "    float sunPath = pow(max(dot(reflect(-V, N), L), 0.0), mix(900.0, 120.0, chop));\n"
    "    col += sha * sunPath * vec3(1.0, 0.92, 0.65) * mix(3.5, 1.2, chop);\n"
    "    vec3 fogCol = mix(vec3(0.38, 0.58, 0.88), sampleSkyFrom(p + vec3(0.0, 6.0, 0.0), normalize(vec3(rd.x, 0.25, rd.z + 0.5)), tm), 0.55);\n"
    "    col = mix(col, fogCol, smoothstep(18.0, 120.0, dist) * 0.22);\n"
    "    const float fe = 0.055;\n"
    "    vec2 xz = p.xz;\n"
    "    float gx = seaHeight(xz + vec2(fe, 0.0), tm) - seaHeight(xz - vec2(fe, 0.0), tm);\n"
    "    float gz = seaHeight(xz + vec2(0.0, fe), tm) - seaHeight(xz - vec2(0.0, fe), tm);\n"
    "    float slope = length(vec2(gx, gz)) / (2.0 * fe);\n"
    "    float crest = smoothstep(0.45, 1.35, slope) * smoothstep(0.18, 0.88, chop);\n"
    "    col = mix(col, vec3(0.92, 0.96, 1.0), crest * 0.45);\n"
    "    return col;\n"
    "}\n"
    "\n"
    "vec3 traceScene(vec3 ro, vec3 rd, float tm) {\n"
    "    float tSun = 1.0e30;\n"
    "    bool hitSun = sphHit(ro, rd, sunCenter(), SUN_RADIUS, tSun);\n"
    "    float tSea = 1.0e30;\n"
    "    if (rd.y < -0.002) {\n"
    "        float tp;\n"
    "        if (rayPlaneY(ro, rd, 0.0, tp)) tSea = tp;\n"
    "    }\n"
    "    if (hitSun && tSun < tSea) return shadeSun(ro, rd, tSun, tm);\n"
    "    if (tSea < 1.0e29) {\n"
    "        vec3 p = ro + rd * tSea;\n"
    "        p.y = seaHeight(p.xz, tm);\n"
    "        return shadeSea(p, rd, tm);\n"
    "    }\n"
    "    return sampleSkyFrom(ro, rd, tm);\n"
    "}\n"
    "\n"
    "vec3 cameraRay(vec2 ndc, float aspect) {\n"
    "    vec3 ro = frame.uCameraPos.xyz;\n"
    "    vec3 fwd = normalize(frame.uCameraTarget.xyz - ro);\n"
    "    vec3 right = normalize(cross(fwd, vec3(0.0, 1.0, 0.0)));\n"
    "    vec3 camUp = normalize(cross(right, fwd));\n"
    "    float th = frame.uFovTan;\n"
    "    return normalize(fwd + right * (ndc.x * aspect * th) + camUp * (ndc.y * th));\n"
    "}\n"
    "\n"
    "vec3 acesTonemap(vec3 x) {\n"
    "    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;\n"
    "    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    float w = max(frame.uResolution.x, 1.0);\n"
    "    float h = max(frame.uResolution.y, 1.0);\n"
    "    float aspect = w / h;\n"
    "    vec2 ndc;\n"
    "    ndc.x = (2.0 * (gl_FragCoord.x + 0.5) / w - 1.0);\n"
    "    ndc.y = (2.0 * (gl_FragCoord.y + 0.5) / h - 1.0);\n"
    "    vec3 ro = frame.uCameraPos.xyz;\n"
    "    vec3 rd = cameraRay(ndc, aspect);\n"
    "    float tm = frame.uTime;\n"
    "    vec3 col = traceScene(ro, rd, tm);\n"
    "    col *= frame.uExposure;\n"
    "    vec3 bloom = max(col - vec3(0.75), 0.0);\n"
    "    col += bloom * bloom * vec3(0.28, 0.22, 0.12);\n"
    "    col = acesTonemap(col);\n"
    "    col = pow(max(col, vec3(0.0)), vec3(1.0 / 2.2));\n"
    "    vec2 q = (gl_FragCoord.xy - 0.5 * frame.uResolution) / min(w, h);\n"
    "    col *= 1.0 - dot(q, q) * 0.04;\n"
    "    fragColor = vec4(col, 1.0);\n"
    "}\n";

static void sea_preset_calm(float out[4]) {
    out[0] = 0.12f;
    out[1] = 0.05f;
    out[2] = 0.06f;
    out[3] = 0.22f;
}

static void sea_preset_moderate(float out[4]) {
    out[0] = 0.55f;
    out[1] = 0.62f;
    out[2] = 0.34f;
    out[3] = 0.14f;
}

static void sea_preset_storm(float out[4]) {
    out[0] = 0.92f;
    out[1] = 1.0f;
    out[2] = 0.48f;
    out[3] = 0.06f;
}

static int init_gpu(void) {
    OceanVertex tri[3] = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{3.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-1.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    };
    uint32_t ix[3] = {0, 1, 2};
    SituationError err;
#if defined(SITUATION_USE_VULKAN)
    err = SituationCreateMeshEx(tri, 3, sizeof(OceanVertex), ix, 3, SIT_MESH_LAYOUT_PULL, &g_fs_mesh);
#else
    err = SituationCreateMesh(tri, 3, sizeof(OceanVertex), ix, 3, &g_fs_mesh);
#endif
    if (err != SITUATION_SUCCESS) {
        char* msg = NULL;
        SituationGetLastErrorMsg(&msg);
        fprintf(stderr, "[21_ocean] Mesh failed: %s\n", msg ? msg : "?");
        if (msg) {
            SituationFreeString(msg);
        }
        return -1;
    }

#if defined(SITUATION_USE_VULKAN)
    if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)
        && SituationGetMeshVertexBufferAddress(g_fs_mesh) != 0) {
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
        fprintf(stderr, "[21_ocean] Shader compile failed: %s\n", msg ? msg : "?");
        if (msg) {
            SituationFreeString(msg);
        }
        SituationDestroyMesh(&g_fs_mesh);
        return -1;
    }

#if defined(SITUATION_USE_OPENGL)
    if (SituationBindUniformBlock(g_shader, "OceanFrame", 0u) != SITUATION_SUCCESS) {
        fprintf(stderr, "[21_ocean] OceanFrame UBO block bind failed\n");
        SituationDestroyMesh(&g_fs_mesh);
        SituationUnloadShader(&g_shader);
        return -1;
    }
#endif

    err = SituationCreateBuffer(OCEAN_FRAME_UBO_BYTES, NULL, SITUATION_BUFFER_USAGE_UNIFORM_BUFFER, &g_frame_ubo);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "[21_ocean] OceanFrame UBO creation failed\n");
        SituationDestroyMesh(&g_fs_mesh);
        SituationUnloadShader(&g_shader);
        return -1;
    }

    return 0;
}

static void upload_ocean_frame_ubo(const vec3 eye, const vec3 target, float rw, float rh,
                                   float time_anim, float travel_phase, const vec3 sun_dir,
                                   const float sea_state[4], const float cloud_state[4],
                                   float fov_tan, float exposure) {
    OceanFrameUbo ubo;
    memset(&ubo, 0, sizeof(ubo));
    memcpy(ubo.u_sea_state, sea_state, sizeof(ubo.u_sea_state));
    memcpy(ubo.u_cloud_state, cloud_state, sizeof(ubo.u_cloud_state));
    ubo.u_camera_pos[0] = eye[0];
    ubo.u_camera_pos[1] = eye[1];
    ubo.u_camera_pos[2] = eye[2];
    ubo.u_camera_pos[3] = 0.0f;
    ubo.u_camera_target[0] = target[0];
    ubo.u_camera_target[1] = target[1];
    ubo.u_camera_target[2] = target[2];
    ubo.u_camera_target[3] = 0.0f;
    ubo.u_sun_dir[0] = sun_dir[0];
    ubo.u_sun_dir[1] = sun_dir[1];
    ubo.u_sun_dir[2] = sun_dir[2];
    ubo.u_sun_dir[3] = 0.0f;
    ubo.u_resolution[0] = rw;
    ubo.u_resolution[1] = rh;
    ubo.u_time = time_anim;
    ubo.u_travel_phase = travel_phase;
    ubo.u_fov_tan = fov_tan;
    ubo.u_exposure = exposure;
    SituationUpdateBuffer(g_frame_ubo, 0, OCEAN_FRAME_UBO_BYTES, &ubo);
}

static void ui_fill_rect(SituationCommandBuffer cmd, float x, float y, float w, float h, Vector4 col) {
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, col);
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

/* Camera travel: drift over sea while looking toward the sun (hero framing). */
static void travel_eval(float travel_time, const vec3 sun_dir, vec3 out_eye, vec3 out_target,
                        float* out_phase) {
    const float loop_sec = 90.0f;
    float phase = fmodf(travel_time, loop_sec) / loop_sec;
    if (out_phase) {
        *out_phase = phase;
    }

    float z = phase * 140.0f - 35.0f;
    float yaw = 0.10f * sinf(phase * 6.2831853f * 2.0f);
    float lateral = 1.5f * sinf(phase * 6.2831853f * 3.0f);

    float cy = cosf(yaw);
    float sy = sinf(yaw);
    float bob = 0.06f * sinf(phase * 6.2831853f * 5.0f);
    out_eye[0] = lateral + sy * 1.2f;
    out_eye[1] = 1.05f + bob;
    out_eye[2] = z + cy * 1.2f;

    const float look_ahead = 90.0f;
    out_target[0] = out_eye[0] + sun_dir[0] * look_ahead;
    out_target[1] = out_eye[1] + sun_dir[1] * look_ahead;
    out_target[2] = out_eye[2] + sun_dir[2] * look_ahead;
}

int main(int argc, char** argv) {
    SituationInitInfo cfg = {
        .window_title = "Situation — Example 21: Realistic Ocean",
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

    bool anim = true;
    bool travel = true;
    bool orbit_override = false;
    bool vsync_on = true;
    SituationSetVSync(true);

    float time_anim = 0.0f;
    float travel_time = 0.0f;
    float travel_speed = 1.0f;

    float yaw = 0.85f;
    float pitch = 0.22f;
    float radius = 8.0f;
    vec3 orbit_target = {0.0f, 0.0f, 12.0f};

    float sea_state[4];
    sea_preset_moderate(sea_state);

    float cloud_state[4] = {0.36f, 0.035f, 1.0f, 0.08f};
    /* Golden-hour sun: low, forward, centered in default travel framing */
    vec3 sun_dir = {0.52f, 0.32f, 0.79f};
    glm_vec3_normalize(sun_dir);
    float exposure = 1.15f;

    static const char* k_sea_names[] = {"calm", "moderate", "storm"};

    printf(
        "Example 21 — Realistic Ocean (Advanced Shader)\n"
        "  Backend: %s — shader: %s — draw: %s\n"
        "  Default: camera travel + sunny sky + rolling ocean\n"
        "  T: travel toggle   1-3: sea presets   F12: screenshot\n",
        SituationGetGraphicsBackendName(),
#if defined(SITUATION_USE_VULKAN)
        "megashader (VK tier)",
#else
        "budget (GL ~65k instr cap)",
#endif
        g_use_vertex_pull ? "vertex pull (mesh BDA)" : "VAO attribute fetch");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        float dt = SituationGetFrameTime();
        float rw = (float)SituationGetRenderWidth();
        float rh = (float)SituationGetRenderHeight();
        float aspect = rh > 1.0f ? rw / rh : 1.0f;

        if (SituationIsKeyPressed(SIT_KEY_T)) {
            travel = !travel;
        }
        if (SituationIsKeyPressed(SIT_KEY_R)) {
            travel_time = 0.0f;
            yaw = 0.85f;
            pitch = 0.22f;
            radius = 8.0f;
        }
        if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
            anim = !anim;
        }
        if (SituationIsKeyPressed(SIT_KEY_1)) {
            sea_preset_calm(sea_state);
        }
        if (SituationIsKeyPressed(SIT_KEY_2)) {
            sea_preset_moderate(sea_state);
        }
        if (SituationIsKeyPressed(SIT_KEY_3)) {
            sea_preset_storm(sea_state);
        }
        if (SituationIsKeyPressed(SIT_KEY_COMMA)) {
            cloud_state[0] -= 0.04f;
            if (cloud_state[0] < 0.15f) {
                cloud_state[0] = 0.15f;
            }
        }
        if (SituationIsKeyPressed(SIT_KEY_PERIOD)) {
            cloud_state[0] += 0.04f;
            if (cloud_state[0] > 0.92f) {
                cloud_state[0] = 0.92f;
            }
        }
        if (SituationIsKeyPressed(SIT_KEY_LEFT_BRACKET)) {
            sea_state[1] -= 0.05f;
            if (sea_state[1] < 0.0f) {
                sea_state[1] = 0.0f;
            }
        }
        if (SituationIsKeyPressed(SIT_KEY_RIGHT_BRACKET)) {
            sea_state[1] += 0.05f;
            if (sea_state[1] > 1.0f) {
                sea_state[1] = 1.0f;
            }
        }
        if (SituationIsKeyPressed(SIT_KEY_V)) {
            vsync_on = !vsync_on;
            SituationSetVSync(vsync_on);
        }

        orbit_override = SituationIsMouseButtonDown(0);
        if (orbit_override) {
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

        float wheel = SituationGetMouseWheelMove();
        if (travel && !orbit_override) {
            travel_speed -= wheel * 0.08f;
            if (travel_speed < 0.15f) {
                travel_speed = 0.15f;
            }
            if (travel_speed > 4.0f) {
                travel_speed = 4.0f;
            }
        } else {
            radius -= wheel * 0.45f;
            if (radius < 2.5f) {
                radius = 2.5f;
            }
            if (radius > 24.0f) {
                radius = 24.0f;
            }
        }

        if (anim) {
            time_anim += dt;
            if (travel) {
                travel_time += dt * travel_speed;
            }
        }

        vec3 eye;
        vec3 target;
        float travel_phase = 0.0f;

        if (travel && !orbit_override) {
            travel_eval(travel_time, sun_dir, eye, target, &travel_phase);
        } else {
            orbit_eye(yaw, pitch, radius, orbit_target, eye);
            glm_vec3_copy(orbit_target, target);
            travel_phase = fmodf(travel_time, 90.0f) / 90.0f;
        }

        const float fov_tan = tanf(glm_rad(55.0f) * 0.5f);

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = {.color = {12, 42, 88, 255}},
                },
                .depth_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = {.depth = 1.0f},
                },
            };

            SituationCmdBeginRenderPass(cmd, &pass);

            SituationCmdBindPipeline(cmd, g_shader);
            upload_ocean_frame_ubo(eye, target, rw, rh, time_anim, travel_phase, sun_dir, sea_state,
                                   cloud_state, fov_tan, exposure);
            SituationCmdBindDescriptorSet(cmd, 0, g_frame_ubo);

            if (g_use_vertex_pull) {
                SituationCmdBindMeshPullBuffers(cmd, g_fs_mesh);
            }
            SituationCmdDrawMesh(cmd, g_fs_mesh);

            {
                const float fs = 14.0f;
                const float lh = 17.0f;
                const float pad = 10.0f;
                const int hud_lines = 3;
                const float panel_h = pad * 2.0f + lh * (float)hud_lines;
                const float panel_y = rh - panel_h - 8.0f;
                const float panel_w = rw - 24.0f;
                const float panel_x = 12.0f;
                int fps = SituationGetFPS();
                char line[192];

                int sea_idx = 1;
                if (sea_state[1] < 0.15f) {
                    sea_idx = 0;
                } else if (sea_state[1] > 0.75f) {
                    sea_idx = 2;
                }

                ui_fill_rect(cmd, panel_x, panel_y, panel_w, panel_h, (Vector4){{0.0f, 0.0f, 0.0f, 0.82f}});

                float y = panel_y + pad;
                ColorRGBA hi = {240, 244, 252, 255};
                ColorRGBA dim = {175, 190, 210, 255};

                snprintf(line, sizeof line, "Ocean 21   FPS %d (%.1f ms)   VSync %s   travel %s [T]",
                         fps, dt * 1000.0f, vsync_on ? "ON" : "OFF", travel ? "ON" : "OFF");
                ui_draw_line_centered(cmd, &ui_font, line, rw, y, fs, hi);
                y += lh;

                snprintf(line, sizeof line,
                         "Sea: %s chop %.0f%% [1-3][ ]   clouds %.0f%% [,][.]   anim %s [Space]",
                         k_sea_names[sea_idx], sea_state[1] * 100.0f, cloud_state[0] * 100.0f,
                         anim ? "ON" : "PAUSED");
                ui_draw_line_centered(cmd, &ui_font, line, rw, y, fs, hi);
                y += lh;

                snprintf(line, sizeof line, "LMB orbit   Wheel %s   R reset   F12 PNG",
                         (travel && !orbit_override) ? "speed" : "zoom");
                ui_draw_line_centered(cmd, &ui_font, line, rw, y, fs, dim);
            }

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();

            if (SituationIsKeyPressed(SIT_KEY_F12)) {
                static int screenshot_seq = 0;
                char path[256];
                time_t now = time(NULL);
                snprintf(path, sizeof path, "ocean_21_%lld_%04d.png", (long long)now, screenshot_seq++);
                SituationError se = SituationTakeScreenshot(path);
                if (se == SITUATION_SUCCESS) {
                    printf("Screenshot saved: %s\n", path);
                } else {
                    char* err = NULL;
                    if (SituationGetLastErrorMsg(&err) == SITUATION_SUCCESS && err) {
                        fprintf(stderr, "Screenshot failed: %s\n", err);
                        SituationFreeString(err);
                    }
                }
            }
        }
    }

    SituationUnloadFont(ui_font);
    SituationDestroyBuffer(&g_frame_ubo);
    SituationDestroyMesh(&g_fs_mesh);
    SituationUnloadShader(&g_shader);
    SituationShutdown();
    return 0;
}