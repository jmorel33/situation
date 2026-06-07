#version 450 core
/* Per-frame camera + scene scalars (std140) — one upload per draw instead of layout(location) soup. */
#if defined(VULKAN)
layout(std140, set = 0, binding = 0) uniform SkyFrame {
#else
layout(std140, binding = 0) uniform SkyFrame {
#endif
    float uTime;
    float uBobPhase;
    float uThreatPulse;
    float uMusicPulse;
    float uYaw;
    float uHorizonPxFromTop;
    float uHorizonShiftPx;
    float uTanHalfFov;
    vec2  uResolution;
    vec3  uCamPos;
    float _padCam;
    vec3  uSunDir;
    float _padSun;
    vec2  uMapSize;
    int   uSpriteCount;
    int   uSpriteDebugMode;
    int   uShaderSpritesEnabled;
    float uPainFlash;
    mat4  uFlatInvVP;
} frame;

/* Match demon_hunt.c: TELEPORTER_MAX_COUNT, HELLRAISER_COUNT, PLAYER_SHOT_COUNT, DRONE_SHOT_COUNT */
#define DH_DYN_TELEPORTER_COUNT   6
#define DH_DYN_HELLRAISER_COUNT   8
#define DH_DYN_PLAYER_SHOT_COUNT 16
#define DH_DYN_DRONE_SHOT_COUNT  12
#define DH_DYN_SLOT_COUNT        DH_DYN_PLAYER_SHOT_COUNT
#define DH_DYN_LIGHT_MAX_DIST    24.0
#define DH_DYN_LIGHT_MAX_DIST_SQ (DH_DYN_LIGHT_MAX_DIST * DH_DYN_LIGHT_MAX_DIST)

const int MAX_SHADER_SPRITES = 32;

/* Recovery toggles — see doc/plan/DEMON_HUNT_SHADER_STRUCTURAL_REFACTOR.md § Execution */
#define DH_ENABLE_SOFT_SHADOW        1  /* 1 = 4-tap sun (heavier link); 0 = single tap */
#define DH_ENABLE_BLOOM              1
#define DH_SPRITE_LITE_LIGHTING      0  /* 0 = maze ray shadows on sprites; 1 = cheaper flat sun */
#define DH_MAX_POINT_SHADOW_TESTS    8  /* cap point_shadow DDAs per gather_dynamic_lights call */
#define DH_ENABLE_FRAME_FEEDBACK     1  /* Phase 1.5: previous-frame texture for CA, bloom, SSR, TAA */
/* Pipeline layout profile UBO_SSBO_SAMPLER provides set 2 sampler for feedback texture. */
#define DH_ENABLE_MATERIALS          1  /* Phase 2: per-wall material shading */
#define DH_ENABLE_ATMOSPHERE         1  /* Phase 3: height fog, edge AO, volumetric godrays, damage CA */
#define DH_ENABLE_POSTPROCESS        1  /* Phase 6: bloom, vignette, film grain, shadow dither */

#define ENABLE_SPRITE_RESOLVER       1
const int SHADER_SPRITE_DEMON        = 1;
const int SHADER_SPRITE_HELLRAISER   = 2;
const int SHADER_SPRITE_AMMO         = 3;
const int SHADER_SPRITE_PARTICLE     = 4;
const int SHADER_SPRITE_PLAYER_SHOT  = 5;
const int SHADER_SPRITE_PORTAL       = 6;
const int SHADER_SPRITE_EXIT_PILLAR  = 7;
const int SHADER_SPRITE_DRONE_SHOT   = 8;  /* white plasma bolt + comet tail */
const int SHADER_SPRITE_HUNTER_DRONE = 9;  /* blue-gray hull, cyan glow, orange eye */

/* Map + dynamic lights + sprite pack — one SSBO at binding 1 (std430). */
#if defined(VULKAN)
layout(std430, set = 1, binding = 0) readonly buffer ShaderScenePack {
#else
layout(std430, binding = 1) readonly buffer ShaderScenePack {
#endif
    int mapSize[2];
    int wallRows[32];
    int archNsRows[32];
    int archEwRows[32];
    int materialRows[128];  /* 4-bit material ID per cell, packed 8 cells/int, 4 ints/row */
    int _alignPad[2];
    vec4 teleporters[DH_DYN_TELEPORTER_COUNT];
    vec4 hellraisers[DH_DYN_HELLRAISER_COUNT];
    vec4 playerShots[DH_DYN_PLAYER_SHOT_COUNT];
    vec4 droneShots[DH_DYN_DRONE_SHOT_COUNT];
    vec4 droneShotDirs[DH_DYN_DRONE_SHOT_COUNT];
    vec4 spriteData[];
} scene;

/* Phase 1.5: Previous-frame feedback texture */
#if DH_ENABLE_FRAME_FEEDBACK
#if defined(VULKAN)
layout(set = 2, binding = 0) uniform sampler2D uPrevFrame;
#else
layout(binding = 3) uniform sampler2D uPrevFrame;
#endif
#endif

layout(location = 0) out vec4 fragColor;

const int SCENE_SPRITE_VEC4_BASE = 0;

vec4 sprite_row0(int i) { return scene.spriteData[SCENE_SPRITE_VEC4_BASE + i]; }
vec4 sprite_row1(int i) { return scene.spriteData[SCENE_SPRITE_VEC4_BASE + MAX_SHADER_SPRITES + i]; }
vec4 sprite_row2(int i) { return scene.spriteData[SCENE_SPRITE_VEC4_BASE + MAX_SHADER_SPRITES * 2 + i]; }

/* Phase 1.5: Previous-frame sampling helpers */
#if DH_ENABLE_FRAME_FEEDBACK
vec3 sample_prev_frame(vec2 screen_uv) {
    return texture(uPrevFrame, screen_uv).rgb;
}
vec3 sample_prev_frame_offset(vec2 screen_uv, vec2 pixel_offset) {
    return texture(uPrevFrame, screen_uv + pixel_offset / frame.uResolution).rgb;
}
#endif

const vec3 FOG_COLOR = vec3(0.08, 0.07, 0.10);

/* Per-fragment projection + column ray; portable fields live here, not as function args. */
struct DhColCtx {
    vec3 ro;
    vec3 view_rd;
    vec2 rdxz;
    vec2 fwd;
    vec3 col_rd;
    float col_xz_comp;
    float ray_ang;
    float pct;
    float mid;
    float focal_px;
    float screen_tan_pitch;
    float line_h;
    float wtop;
    float wbot;
    float sprite_max_raw;
};

struct DhDdaArchCtx {
    vec3 ro;
    vec3 col_rd;
    float col_xz_comp;
};

/* Per gather_dynamic_lights() call — receiver + type constants, not per-emitter args. */
struct DhDynLightCtx {
    vec2 pos_xz;
    vec3 accum;
    float radius_sq;
    float inv_radius;
    float energy;
    vec3 col;
    int shadow_budget;
};

DhColCtx g_col;
DhDdaArchCtx g_arch;
DhDynLightCtx g_dyn;

/* Matches C compute_horizon_shift_px / focal_px — global pitch for column + arches. */
float dh_column_tan_pitch(void) {
    return frame.uHorizonShiftPx / g_col.focal_px;
}

void dh_col_derive(void) {
    g_col.view_rd = normalize(g_col.view_rd);
    g_col.rdxz = normalize(g_col.rdxz);
    g_col.focal_px = (frame.uResolution.y * 0.5) / max(frame.uTanHalfFov, 1e-5);
    /* Per-row tan for sky dome; column uses global tan in col_rd (raycaster convention). */
    g_col.screen_tan_pitch = (g_col.mid - g_col.pct) / g_col.focal_px;
    float col_tp = dh_column_tan_pitch();
    g_col.col_rd = normalize(vec3(g_col.rdxz.x, col_tp, g_col.rdxz.y));
    g_col.col_xz_comp = max(length(g_col.col_rd.xz), 1e-5);
}

void dh_col_set_wall_span(float perp) {
    g_col.line_h = max(frame.uResolution.y / perp, 1e-4);
    g_col.wtop = fma(-g_col.line_h, 1.0 - frame.uCamPos.y, g_col.mid);
    g_col.wbot = fma(g_col.line_h, frame.uCamPos.y, g_col.mid);
    if (g_col.wtop > g_col.wbot) {
        float swap = g_col.wtop;
        g_col.wtop = g_col.wbot;
        g_col.wbot = swap;
    }
}

void dh_arch_bind(vec3 ro, vec3 rd3, vec2 rdxz) {
    g_arch.ro = ro;
    g_arch.col_rd = normalize(vec3(rdxz.x, rd3.y, rdxz.y));
    g_arch.col_xz_comp = max(length(g_arch.col_rd.xz), 1e-5);
}

void dh_arch_bind_column(void) {
    g_arch.ro = g_col.ro;
    /* Per-pixel pitch (view_rd.y): arches are 3D; global col_rd.y is for vertical wall columns only. */
    float vy = g_col.view_rd.y;
    if (abs(vy) < 1e-4) {
        vy = g_col.col_rd.y;
    }
    g_arch.col_rd = normalize(vec3(g_col.rdxz.x, vy, g_col.rdxz.y));
    g_arch.col_xz_comp = max(length(g_arch.col_rd.xz), 1e-5);
}

/* ARCH CONTRACT — do not mix these paths:
 *   Visible walls (cast_prim): arch_hit_interval_column → dh_arch_bind_column + arch_hit_interval.
 *     bind_column uses rdxz yaw + view_rd.y (per fragment). Walls keep g_col.col_rd (global tan pitch).
 *   Sun/segment shadow DDA: arch_hit_interval only after dh_arch_bind(ro, rd3, rdxz). */

/* tan(pitch_row) from horizon -> sin/cos elevation; zenith collapses xz without caps. */
vec3 dh_dir_from_row_tan(float tan_pitch) {
    float elev = atan(tan_pitch);
    float y = sin(elev);
    float ch = cos(elev);
    return vec3(g_col.rdxz.x * ch, y, g_col.rdxz.y * ch);
}

vec3 dh_sky_ray(void) {
    return dh_dir_from_row_tan(max(g_col.screen_tan_pitch, 1e-6));
}

vec3 dh_floor_ray(void) {
    return dh_dir_from_row_tan(min(g_col.screen_tan_pitch, -1e-6));
}

// -----------------------------------------------------------------------------
// Shared helpers and forward declarations
// -----------------------------------------------------------------------------

vec3 fog_mix(vec3 color, float fog) {
    return fma(color - FOG_COLOR, vec3(fog), FOG_COLOR);
}

#if DH_ENABLE_ATMOSPHERE
/* Phase 3A: Height-gradient fog — floor-hugging, clears for upward rays.
 * Tuned so horizontal rays match the original 0.06 density; downward rays
 * get slightly more fog (floor-hug effect); upward rays get less. */
float height_fog(float dist, vec3 ray_dir) {
    float base_density = 0.06;
    float height_falloff = clamp(fma(-ray_dir.y, 1.5, 1.0), 0.3, 1.3);
    return exp(-dist * base_density * height_falloff);
}
#endif

float pristine_shadow(vec2 pos_xz, float pos_y);
float point_shadow(vec2 from_xz, vec2 light_xz);
bool segment_world_visible(vec3 from_pos, vec3 to_pos);
float hash12(vec2 p); /* forward decl — definition in sky/cloud section */

const int DH_DDA_SUN = 0;
const int DH_DDA_POINT = 1;
const int DH_DDA_SEGMENT = 2;
const int MAP_DDA_MAX_STEPS = 64;
bool map_dda_occluded(int mode, vec2 origin, vec2 dir_norm, float max_dist);
vec3 gather_dynamic_lights(vec2 pos_xz);

float sprite_debug_signal() {
    float active_sum = 0.0;
    int count = clamp(frame.uSpriteCount, 0, MAX_SHADER_SPRITES);
    for (int i = 0; i < MAX_SHADER_SPRITES; i++) {
        if (i >= count) break;
        active_sum += sprite_row0(i).w;
        active_sum += sprite_row1(i).w * 0.01;
        active_sum += dot(abs(sprite_row2(i)), vec4(0.0001));
    }
    return active_sum;
}

struct SpriteHit {
    float raw;
    float perp;
    int index;
    int type;
    vec2 uv;
    vec3 world_pos;
    float opaque_coverage;
    float alpha_coverage;
};

SpriteHit no_sprite_hit() {
    SpriteHit h;
    h.raw = 1e20;
    h.perp = 1e20;
    h.index = -1;
    h.type = 0;
    h.uv = vec2(0.0);
    h.world_pos = vec3(0.0);
    h.opaque_coverage = 0.0;
    h.alpha_coverage = 0.0;
    return h;
}

struct SpriteResolve {
    SpriteHit nearest_opaque;
    SpriteHit nearest_alpha;
};

SpriteResolve no_sprite_resolve() {
    SpriteResolve r;
    r.nearest_opaque = no_sprite_hit();
    r.nearest_alpha = no_sprite_hit();
    return r;
}

#if ENABLE_SPRITE_RESOLVER

// -----------------------------------------------------------------------------
// Sprite masks and coverage
// -----------------------------------------------------------------------------

float rect_mask(vec2 uv, vec2 lo, vec2 hi) {
    vec2 m = step(lo, uv) * step(uv, hi);
    return m.x * m.y;
}

float radial_mask(vec2 uv, vec2 center, vec2 radius) {
    vec2 q = (uv - center) / radius;
    float d = dot(q, q);
    return clamp(1.0 - d, 0.0, 1.0);
}

float ammo_coverage(vec2 uv) {
    vec2 body = step(vec2(0.12, 0.08), uv) * step(uv, vec2(0.88, 0.78));
    vec2 core = step(vec2(0.38, 0.28), uv) * step(uv, vec2(0.62, 0.62));
    return max(body.x * body.y, core.x * core.y);
}

float player_shot_core_coverage(vec2 uv) {
    float core = radial_mask(uv, vec2(0.5), vec2(0.18));
    return smoothstep(0.45, 0.95, core);
}

float player_shot_glow_coverage(vec2 uv, float fade) {
    float glow = radial_mask(uv, vec2(0.5), vec2(0.72));
    return smoothstep(0.02, 0.70, glow) * (0.34 + fade * 0.30);
}

bool particle_is_drip_tint(vec4 params) {
    return params.z > 0.55 && params.y < 0.45 && params.w < 0.55;
}

bool particle_is_bolt_tint(vec4 params) {
    return params.y >= 0.90 && params.z >= 0.90 && params.w >= 0.90;
}

bool particle_is_portal_energy_tint(vec4 params) {
    return params.y > 0.75 && params.z > 0.25 && params.z < 0.72 && params.w > 0.82;
}

/* Fade billboard quad corners — avoids visible square hull on soft particles. */
float particle_billboard_edge_fade(vec2 uv) {
    vec2 edge = abs(uv - vec2(0.5)) / vec2(0.5);
    return smoothstep(1.0, 0.72, max(edge.x, edge.y));
}

float particle_alpha_coverage(vec2 uv, vec4 params) {
    float life = clamp(params.x, 0.0, 1.0);
    vec2 radii = vec2(0.36, 0.36);
    if (particle_is_bolt_tint(params)) {
        radii = vec2(0.40, 0.40);
    } else if (particle_is_drip_tint(params)) {
        radii = vec2(0.30, 0.30);
    } else if (particle_is_portal_energy_tint(params)) {
        radii = vec2(0.24, 0.24);
    }
    vec2 q = (uv - vec2(0.5)) / radii;
    float r = dot(q, q);
    float a = smoothstep(1.0, 0.08, r) * life;
    if (particle_is_bolt_tint(params)) {
        float halo = smoothstep(1.15, 0.22, r) * life * 0.38;
        a = max(a, halo);
    }
    return a * particle_billboard_edge_fade(uv);
}

float portal_opaque_coverage(vec2 uv) {
    float core = rect_mask(uv, vec2(0.25, 0.00), vec2(0.75, 1.00));
    float trim = rect_mask(uv, vec2(0.43, 0.08), vec2(0.57, 0.92));
    float energy = rect_mask(uv, vec2(0.36, 0.38), vec2(0.64, 0.70));
    return max(core, max(trim, energy * 0.85));
}

float portal_alpha_coverage(vec2 uv) {
    float aura = radial_mask(uv, vec2(0.5, 0.52), vec2(0.80, 0.72));
    return smoothstep(0.03, 0.78, aura) * 0.60;
}

float exit_pillar_opaque_coverage(vec2 uv) {
    return rect_mask(uv, vec2(0.18, 0.04), vec2(0.82, 1.00));
}

float exit_pillar_alpha_coverage(vec2 uv, float open_state) {
    float aura = radial_mask(uv, vec2(0.5, 0.56), vec2(0.70, 0.64));
    return smoothstep(0.06, 0.82, aura) * mix(0.42, 0.50, open_state);
}

float demon_opaque_coverage(vec2 uv, vec4 params) {
    float hand_bob = cos(params.w) * 0.045;
    float body = rect_mask(uv, vec2(0.25, 0.70), vec2(0.75, 0.98));
    float core = rect_mask(uv, vec2(0.45, 0.40), vec2(0.55, 0.55));
    float left_eye = rect_mask(uv, vec2(0.25, 0.84), vec2(0.40, 0.94));
    float right_eye = rect_mask(uv, vec2(0.60, 0.84), vec2(0.75, 0.94));
    float left_arm = rect_mask(uv, vec2(0.00, 0.45 + hand_bob), vec2(0.18, 0.70 + hand_bob));
    float right_arm = rect_mask(uv, vec2(0.82, 0.45 - hand_bob), vec2(1.00, 0.70 - hand_bob));
    return max(max(body, core), max(max(left_eye, right_eye), max(left_arm, right_arm)));
}

float demon_alpha_coverage(vec2 uv, float hurt) {
    float hurt_glow = fma(hurt, hurt, 0.0);
    const vec2 center = vec2(0.5, 0.65);
    vec2 extent = vec2(fma(hurt_glow, 0.12, 0.70), fma(hurt_glow, 0.18, 0.90));
    vec2 inv_extent = vec2(1.0) / extent;
    vec2 q = abs(uv - center);
    float box_falloff = fma(-max(q.x * inv_extent.x, q.y * inv_extent.y), 1.0, 1.0);
    float broad_aura = smoothstep(0.00, 0.22, box_falloff);
    vec2 rq = (uv - center) * inv_extent;
    float core_haze = clamp(fma(-dot(rq, rq), 1.0, 1.0), 0.0, 1.0);
    float aura = max(fma(broad_aura, 0.90, 0.0), fma(core_haze, 0.50, 0.0));
    return fma(aura, fma(hurt_glow, 0.32, 0.40), 0.0);
}

float hellraiser_opaque_coverage(vec2 uv) {
    float body = rect_mask(uv, vec2(0.28, 0.18), vec2(0.72, 0.76));
    float left_horn = rect_mask(uv, vec2(0.10, 0.72), vec2(0.34, 0.98));
    float right_horn = rect_mask(uv, vec2(0.66, 0.72), vec2(0.90, 0.98));
    float left_eye = rect_mask(uv, vec2(0.34, 0.58), vec2(0.46, 0.68));
    float right_eye = rect_mask(uv, vec2(0.54, 0.58), vec2(0.66, 0.68));
    float core = rect_mask(uv, vec2(0.42, 0.28), vec2(0.58, 0.48));
    return max(max(body, core), max(max(left_horn, right_horn), max(left_eye, right_eye)));
}

float hellraiser_alpha_coverage(vec2 uv) {
    float aura = radial_mask(uv, vec2(0.5, 0.52), vec2(0.82, 0.54));
    return smoothstep(0.06, 0.82, aura) * 0.40;
}

/* Hunter drone: hull (center rect) + two wing panels + eye dot. */
float hunter_drone_opaque_coverage(vec2 uv) {
    float hull  = rect_mask(uv, vec2(0.22, 0.32), vec2(0.78, 0.74));
    float lwing = rect_mask(uv, vec2(0.02, 0.38), vec2(0.22, 0.64));
    float rwing = rect_mask(uv, vec2(0.78, 0.38), vec2(0.98, 0.64));
    float eye   = rect_mask(uv, vec2(0.40, 0.46), vec2(0.60, 0.60));
    return max(max(hull, eye), max(lwing, rwing));
}

float hunter_drone_alpha_coverage(vec2 uv) {
    /* Cyan glow halo around hull. */
    float aura = radial_mask(uv, vec2(0.5, 0.50), vec2(0.58, 0.52));
    return smoothstep(0.04, 0.76, aura) * 0.38;
}

/* Drone shot: tight plasma sphere — fully opaque core. */
float drone_shot_opaque_coverage(vec2 uv) {
    return radial_mask(uv, vec2(0.5), vec2(0.36, 0.36));
}

float drone_shot_alpha_coverage(vec2 uv) {
    /* Wide soft halo around the sphere. */
    float halo = radial_mask(uv, vec2(0.5), vec2(0.72, 0.72));
    return smoothstep(0.02, 0.68, halo) * 0.38;
}

float sprite_opaque_coverage(int sprite_type, vec2 uv, vec4 params) {
    if (sprite_type == SHADER_SPRITE_AMMO)         return ammo_coverage(uv);
    if (sprite_type == SHADER_SPRITE_DEMON)         return demon_opaque_coverage(uv, params);
    if (sprite_type == SHADER_SPRITE_HELLRAISER)    return hellraiser_opaque_coverage(uv);
    if (sprite_type == SHADER_SPRITE_HUNTER_DRONE)  return hunter_drone_opaque_coverage(uv);
    if (sprite_type == SHADER_SPRITE_DRONE_SHOT)    return drone_shot_opaque_coverage(uv);
#if ENABLE_PHASE3_SPRITES
    if (sprite_type == SHADER_SPRITE_PLAYER_SHOT)   return player_shot_core_coverage(uv);
    if (sprite_type == SHADER_SPRITE_PORTAL)        return portal_opaque_coverage(uv);
    if (sprite_type == SHADER_SPRITE_EXIT_PILLAR)   return exit_pillar_opaque_coverage(uv);
#endif
    return 0.0;
}

float sprite_vertical_offset(int sprite_type, vec4 params) {
    float sway = sin(params.y);
    if (sprite_type == SHADER_SPRITE_AMMO) {
        return fma(sway, 0.035, 0.0);
    }
    if (sprite_type == SHADER_SPRITE_DEMON) {
        return fma(sway, 0.070, fma(sin(frame.uBobPhase), 0.010, 0.0));
    }
    if (sprite_type == SHADER_SPRITE_HELLRAISER) {
        return fma(sway, 0.045, fma(frame.uThreatPulse, 0.020, fma(sin(frame.uBobPhase), 0.008, 0.0)));
    }
    return 0.0;
}

float sprite_alpha_coverage(int sprite_type, vec2 uv, vec4 params) {
    if (sprite_type == SHADER_SPRITE_DEMON)        return demon_alpha_coverage(uv, params.x);
    if (sprite_type == SHADER_SPRITE_HELLRAISER)   return hellraiser_alpha_coverage(uv);
    if (sprite_type == SHADER_SPRITE_HUNTER_DRONE) return hunter_drone_alpha_coverage(uv);
    if (sprite_type == SHADER_SPRITE_DRONE_SHOT)   return drone_shot_alpha_coverage(uv);
    if (sprite_type == SHADER_SPRITE_PLAYER_SHOT)  return player_shot_glow_coverage(uv, params.x);
    if (sprite_type == SHADER_SPRITE_PARTICLE)     return particle_alpha_coverage(uv, params);
    if (sprite_type == SHADER_SPRITE_PORTAL)       return portal_alpha_coverage(uv);
    if (sprite_type == SHADER_SPRITE_EXIT_PILLAR)  return exit_pillar_alpha_coverage(uv, params.x);
    return 0.0;
}

bool ray_billboard_hit(int sprite_index, out SpriteHit hit) {
    hit = no_sprite_hit();
    vec4 s0 = sprite_row0(sprite_index);
    if (s0.w <= 0.0) {
        return false;
    }

    vec4 s1 = sprite_row1(sprite_index);
    vec4 s2 = sprite_row2(sprite_index);
    int sprite_type = int(fma(s1.w, 1.0, 0.5));
    bool supported_sprite = sprite_type == SHADER_SPRITE_AMMO ||
                            sprite_type == SHADER_SPRITE_DEMON ||
                            sprite_type == SHADER_SPRITE_HELLRAISER ||
                            sprite_type == SHADER_SPRITE_DRONE_SHOT ||
                            sprite_type == SHADER_SPRITE_HUNTER_DRONE ||
                           sprite_type == SHADER_SPRITE_PLAYER_SHOT ||
                           sprite_type == SHADER_SPRITE_PARTICLE ||
                           sprite_type == SHADER_SPRITE_PORTAL ||
                           sprite_type == SHADER_SPRITE_EXIT_PILLAR;
    if (!supported_sprite) {
        return false;
    }

    vec2 center = s0.xz;
    vec2 right = vec2(g_col.fwd.y, -g_col.fwd.x);
    /* Column xz ray (same yaw as cast_prim) through billboard plane facing fwd. */
    float denom = fma(g_col.rdxz.x, g_col.fwd.x, g_col.rdxz.y * g_col.fwd.y);
    if (abs(denom) < 1e-5) {
        return false;
    }
    float raw = dot(center - g_col.ro.xz, g_col.fwd) / denom;
    if (raw <= 0.01 || raw >= g_col.sprite_max_raw) {
        return false;
    }

    vec2 hit_xz = fma(g_col.rdxz, vec2(raw), g_col.ro.xz);
    float xz_len = max(length(g_col.view_rd.xz), 1e-5);
    float hit_y = fma(g_col.view_rd.y / xz_len, raw, g_col.ro.y);

    float local_x = dot(hit_xz - center, right);
    float half_width = max(s1.x, 1e-4);
    if (abs(local_x) > half_width) {
        return false;
    }

    float base_y = fma(sprite_vertical_offset(sprite_type, s2), 1.0, s1.z);
    float height = max(s1.y, 1e-4);
    float inv_height = 1.0 / height;
    float local_y = hit_y - base_y;
    if (local_y < 0.0 || local_y > height) {
        return false;
    }

    float inv_uv_span = 0.5 / half_width;
    vec2 uv = vec2(fma(local_x, inv_uv_span, 0.5), local_y * inv_height);
    float opaque_coverage = sprite_opaque_coverage(sprite_type, uv, s2);
    float alpha_coverage = sprite_alpha_coverage(sprite_type, uv, s2);
    if (max(opaque_coverage, alpha_coverage) <= 0.0) {
        return false;
    }

    hit.raw = raw;
    hit.perp = raw * dot(g_col.rdxz, g_col.fwd);
    hit.index = sprite_index;
    hit.type = sprite_type;
    hit.uv = uv;
    hit.world_pos = vec3(hit_xz.x, hit_y, hit_xz.y);
    hit.opaque_coverage = opaque_coverage;
    hit.alpha_coverage = alpha_coverage;
    return true;
}

// -----------------------------------------------------------------------------
// Sprite visibility resolver
// -----------------------------------------------------------------------------

SpriteResolve resolve_sprites(void) {
    SpriteResolve result = no_sprite_resolve();
    if (frame.uShaderSpritesEnabled == 0) return result;

    int count = clamp(frame.uSpriteCount, 0, MAX_SHADER_SPRITES);
    for (int i = 0; i < MAX_SHADER_SPRITES; i++) {
        if (i >= count) break;
        SpriteHit candidate;
        if (ray_billboard_hit(i, candidate)) {
            if (candidate.opaque_coverage > 0.0 && candidate.raw < result.nearest_opaque.raw) {
                result.nearest_opaque = candidate;
            }
            if (candidate.alpha_coverage > 0.0 && candidate.raw < result.nearest_alpha.raw) {
                result.nearest_alpha = candidate;
            }
        }
    }
    return result;
}

float dh_sprite_sun_lum(vec2 world_xz, float sl, float lum_min) {
    vec2 to_cam = g_col.ro.xz - world_xz;
    float dist = length(to_cam);
    float invd = 1.0 / fma(dist, 1.0, 0.08);
    vec2 tc = to_cam * invd;
    float ndotl = max(dot(tc, normalize(frame.uSunDir.xz)), 0.0);
    float att = 1.0 / fma(dist, 0.10, 1.0);
    float nd_att = fma(ndotl, att, 0.0);
    float diffuse = fma(sl, fma(nd_att, 0.80, 0.0), 0.0);
    return clamp(fma(0.20, att, diffuse), lum_min, 1.25);
}

float dh_sprite_sun_tint(float lum) {
    return lum * fma(max(frame.uSunDir.y, 0.0), 0.64, 0.36);
}

vec3 sprite_light(vec3 world_pos, float floor_lift) {
#if DH_SPRITE_LITE_LIGHTING
    float sl = 1.0;
#else
    float sl = pristine_shadow(world_pos.xz, floor_lift);
#endif
    float lum = dh_sprite_sun_lum(world_pos.xz, sl, 0.045);
    vec3 sun = vec3(dh_sprite_sun_tint(lum));
    return fma(gather_dynamic_lights(world_pos.xz), vec3(1.0), sun);
}

/* Portals keep purple palette — sun only, no hellraiser/demon orange bleed when spawning. */
float portal_lum(vec3 world_pos) {
    float sl = pristine_shadow(world_pos.xz, 0.5);
    return dh_sprite_sun_lum(world_pos.xz, sl, 0.05);
}

// -----------------------------------------------------------------------------
// Sprite shading (coverage from resolver hit — no second mask pass)
// -----------------------------------------------------------------------------

vec3 dh_demon_hurt_boost(float hurt_glow) {
    return vec3(fma(hurt_glow, 1.05, 0.0), fma(hurt_glow, 0.32, 0.0), fma(hurt_glow, 0.48, 0.0));
}

float dh_demon_highlight(float light_level, float hurt_glow, float pulse) {
    return fma(frame.uMusicPulse, 0.08, fma(pulse, 0.30, fma(hurt_glow, 1.15, light_level)));
}

float dh_sprite_opaque_fog_k(int sprite_type) {
    if (sprite_type == SHADER_SPRITE_HELLRAISER)   return 0.055;
    if (sprite_type == SHADER_SPRITE_PORTAL ||     sprite_type == SHADER_SPRITE_EXIT_PILLAR) return 0.06;
    if (sprite_type == SHADER_SPRITE_AMMO ||       sprite_type == SHADER_SPRITE_PLAYER_SHOT) return 0.07;
    if (sprite_type == SHADER_SPRITE_DRONE_SHOT)   return 0.060;
    if (sprite_type == SHADER_SPRITE_HUNTER_DRONE) return 0.065;
    return 0.065;
}

vec4 shade_sprite_opaque(SpriteHit hit) {
    if (hit.opaque_coverage <= 0.0) {
        return vec4(0.0);
    }
    vec4 params = sprite_row2(hit.index);
    vec2 uv = hit.uv;
    vec3 col = vec3(0.0);

    if (hit.type == SHADER_SPRITE_AMMO) {
        float core = step(0.36, uv.x) * step(uv.x, 0.64) * step(0.25, uv.y) * step(uv.y, 0.66);
        vec3 light = sprite_light(hit.world_pos, 0.22);
        vec3 base = vec3(0.82, 0.72, 0.12) * light;
        col = mix(base, vec3(1.0, 0.96, 0.58), core);
    } else if (hit.type == SHADER_SPRITE_PLAYER_SHOT) {
        float fade = clamp(params.x, 0.0, 1.0);
        float pulse = fma(sin(fma(frame.uTime, 42.0, params.y)), 0.25, 0.75);
        float gain_a = fma(pulse, 0.35, 0.95);
        float gain_b = fma(fade, 0.45, 0.65);
        col = vec3(1.0, 0.92, 0.18) * fma(gain_a, gain_b, 0.0);
    } else if (hit.type == SHADER_SPRITE_PORTAL) {
        float pulse = fma(sin(fma(frame.uTime, 8.0, params.y)), 0.5, 0.5);
        float lum = portal_lum(hit.world_pos);
        vec3 core_col = vec3(0.70, 0.10, 0.90) * lum;
        vec3 trim_col = vec3(0.90, 0.60, 1.00) * lum;
        vec3 energy_col = vec3(1.0, fma(pulse, 0.60, 0.40), 1.0) * lum;
        float core = rect_mask(uv, vec2(0.25, 0.00), vec2(0.75, 1.00));
        float trim = rect_mask(uv, vec2(0.43, 0.08), vec2(0.57, 0.92));
        float energy = rect_mask(uv, vec2(0.36, 0.38), vec2(0.64, 0.70));
        col = fma(core_col, vec3(core), vec3(0.0));
        col = mix(col, trim_col, trim);
        col = mix(col, energy_col, energy);
    } else if (hit.type == SHADER_SPRITE_EXIT_PILLAR) {
        float open_state = clamp(params.x, 0.0, 1.0);
        float pulse = fma(sin(fma(frame.uTime, mix(2.65, 5.4, open_state), 0.0)), 0.48, 0.52);
        float pulse2 = fma(sin(fma(frame.uTime, mix(3.4, 7.1, open_state), 1.3)), 0.12, 0.88);
        vec3 light = sprite_light(hit.world_pos, 0.5);
        vec3 sealed = vec3(fma(pulse, 0.14, 0.82), fma(pulse2, 0.12, 0.22), fma(pulse, 0.06, 0.06));
        vec3 open_col = vec3(fma(pulse, 0.35, 0.12), fma(pulse2, 0.35, 0.55), fma(pulse, 0.32, 0.58));
        vec3 pillar = mix(sealed, open_col, open_state) * light;
        float trim = rect_mask(uv, vec2(0.42, 0.08), vec2(0.58, 0.94));
        col = mix(pillar, fma(pillar, vec3(1.0), vec3(0.16, 0.18, 0.20)), trim);
    } else if (hit.type == SHADER_SPRITE_DEMON) {
        float hurt_glow = params.x * params.x;
        float body = rect_mask(uv, vec2(0.25, 0.70), vec2(0.75, 0.98));
        float core = rect_mask(uv, vec2(0.45, 0.40), vec2(0.55, 0.55));
        float eyes = max(rect_mask(uv, vec2(0.25, 0.84), vec2(0.40, 0.94)),
                         rect_mask(uv, vec2(0.60, 0.84), vec2(0.75, 0.94)));
        float dark = rect_mask(uv, vec2(0.35, 0.30), vec2(0.65, 0.65));
        float hand_bob = fma(cos(params.w), 0.045, 0.0);
        float arms = max(rect_mask(uv, vec2(0.00, fma(hand_bob, 1.0, 0.45)), vec2(0.18, fma(hand_bob, 1.0, 0.70))),
                         rect_mask(uv, vec2(0.82, fma(-hand_bob, 1.0, 0.45)), vec2(1.00, fma(-hand_bob, 1.0, 0.70))));
        vec3 light = clamp(sprite_light(hit.world_pos, 0.5) + dh_demon_hurt_boost(hurt_glow), 0.0, 1.8);
        float pulse = sin(params.z);
        float light_level = min(1.15, fma(max(max(light.r, light.g), light.b), 1.0, 0.18));
        float highlight = dh_demon_highlight(light_level, hurt_glow, pulse);
        vec3 body_col = vec3(0.68, 0.12, 0.25) * light;
        vec3 dark_col = vec3(0.22, 0.05, 0.15) * light;
        vec3 eye_col = vec3(1.0, 0.20, 0.10) * highlight;
        vec3 core_col = vec3(1.0, 0.80, 0.20) * highlight;
        col = mix(col, body_col, max(body, arms));
        col = mix(col, dark_col, dark);
        col = mix(col, core_col, core);
        col = mix(col, eye_col, eyes);
    } else if (hit.type == SHADER_SPRITE_HELLRAISER) {
        float free_pulse = fma(sin(fma(frame.uTime, 12.0, fma(params.z, 1.7, 0.0))), 0.5, 0.5);
        float pulse = max(fma(free_pulse, 0.65, 0.0), params.w);
        float aura = rect_mask(uv, vec2(0.02, 0.06), vec2(0.98, 0.96));
        float aura_soft = radial_mask(uv, vec2(0.5, 0.52), vec2(0.82, 0.54));
        float body = rect_mask(uv, vec2(0.28, 0.18), vec2(0.72, 0.76));
        float horns = max(rect_mask(uv, vec2(0.10, 0.72), vec2(0.34, 0.98)),
                          rect_mask(uv, vec2(0.66, 0.72), vec2(0.90, 0.98)));
        float eyes = max(rect_mask(uv, vec2(0.34, 0.58), vec2(0.46, 0.68)),
                         rect_mask(uv, vec2(0.54, 0.58), vec2(0.66, 0.68)));
        float core = rect_mask(uv, vec2(0.42, 0.28), vec2(0.58, 0.48));
        vec3 light = max(sprite_light(hit.world_pos, 0.58), vec3(0.32, 0.18, 0.22));
        float aura_mix = max(aura, fma(aura_soft, 0.55, 0.0));
        float aura_gain = fma(pulse, 0.7, 0.7);
        vec3 aura_col = vec3(0.95, 0.06, 0.02) * light * aura_gain;
        vec3 body_col = vec3(0.12, 0.015, 0.02) * light;
        vec3 horn_col = vec3(0.85, 0.12, 0.04) * light;
        vec3 eye_col = vec3(1.0, fma(pulse, 0.42, 0.15), 0.02);
        col = fma(aura_col, vec3(aura_mix), vec3(0.0));
        col = mix(col, body_col, body);
        col = mix(col, horn_col, max(horns, core));
        col = mix(col, eye_col, eyes);
    } else if (hit.type == SHADER_SPRITE_HUNTER_DRONE) {
        /* params: (hurt_flash, hover_px_phase, ?, drone_ang) */
        float pulse    = fma(sin(fma(frame.uTime, 9.0, params.w)), 0.5, 0.5);
        float hull_m   = rect_mask(uv, vec2(0.22, 0.32), vec2(0.78, 0.74));
        float lwing_m  = rect_mask(uv, vec2(0.02, 0.38), vec2(0.22, 0.64));
        float rwing_m  = rect_mask(uv, vec2(0.78, 0.38), vec2(0.98, 0.64));
        float eye_m    = rect_mask(uv, vec2(0.40, 0.46), vec2(0.60, 0.60));
        vec3 light     = max(sprite_light(hit.world_pos, 0.86), vec3(0.30, 0.34, 0.42));
        float hurt     = params.x;
        light.r = fma(hurt, 0.90, light.r);
        light.g = fma(hurt, 0.35, light.g);
        vec3 hull_col  = vec3(0.18, 0.32, 0.42) * light;
        vec3 wing_col  = vec3(0.08, 0.18, 0.24) * light;
        vec3 eye_col   = vec3(1.0, fma(pulse, 0.32, 0.56), 0.12);
        col = fma(hull_col, vec3(hull_m), vec3(0.0));
        col = mix(col, wing_col, max(lwing_m, rwing_m));
        col = mix(col, eye_col, eye_m);
    } else if (hit.type == SHADER_SPRITE_DRONE_SHOT) {
        /* params: (travel, ?, ?, ?) — used for pulse phase */
        float pulse  = fma(sin(fma(frame.uTime, 32.0, params.x * 9.0)), 0.18, 0.82);
        vec3 light   = max(sprite_light(hit.world_pos, hit.world_pos.y), vec3(0.55, 0.55, 0.58));
        /* Bright white-blue plasma core; tint modulated by environment light. */
        col = fma(light * pulse, vec3(1.0, 1.0, 1.0), vec3(0.0));
    } else {
        return vec4(0.0);
    }

    float fog_k = dh_sprite_opaque_fog_k(hit.type);
    col = fog_mix(clamp(col, 0.0, 1.0), exp(-max(hit.perp, 0.0) * fog_k));
    return vec4(col, hit.opaque_coverage);
}

float dh_sprite_alpha_cap(int sprite_type) {
    if (sprite_type == SHADER_SPRITE_DEMON)        return 0.72;
    if (sprite_type == SHADER_SPRITE_PORTAL)       return 0.90;
    if (sprite_type == SHADER_SPRITE_PARTICLE)     return 0.95;
    if (sprite_type == SHADER_SPRITE_EXIT_PILLAR)  return 0.82;
    if (sprite_type == SHADER_SPRITE_PLAYER_SHOT)  return 0.75;
    if (sprite_type == SHADER_SPRITE_DRONE_SHOT)   return 0.42;
    if (sprite_type == SHADER_SPRITE_HUNTER_DRONE) return 0.40;
    return 0.58;
}

vec4 shade_sprite_alpha(SpriteHit hit) {
    if (hit.alpha_coverage <= 0.0) {
        return vec4(0.0);
    }

    vec4 params = sprite_row2(hit.index);
    vec2 uv = hit.uv;
    vec3 col = vec3(0.0);
    int skip_fill_light = 0;

    if (hit.type == SHADER_SPRITE_DEMON) {
        float hurt_glow = params.x * params.x;
        vec3 light = clamp(sprite_light(hit.world_pos, 0.5) + dh_demon_hurt_boost(hurt_glow), 0.0, 1.8);
        vec3 tint = vec3(fma(hurt_glow, 0.45, 0.30), fma(hurt_glow, 0.12, 0.05), fma(hurt_glow, 0.20, 0.40));
        col = tint * light;
        skip_fill_light = 1;
    } else if (hit.type == SHADER_SPRITE_HELLRAISER) {
        float free_pulse = fma(sin(fma(frame.uTime, 12.0, fma(params.z, 1.7, 0.0))), 0.5, 0.5);
        float pulse = max(fma(free_pulse, 0.65, 0.0), params.w);
        col = vec3(0.95, fma(pulse, 0.05, 0.06), 0.02);
    } else if (hit.type == SHADER_SPRITE_PLAYER_SHOT) {
        float fade = clamp(params.x, 0.0, 1.0);
        float pulse = fma(sin(fma(frame.uTime, 42.0, params.y)), 0.25, 0.75);
        float gain = fma(fade, 0.45, 0.75);
        col = vec3(1.0, 0.72, 0.10) * fma(pulse, gain, 0.0);
    } else if (hit.type == SHADER_SPRITE_PARTICLE) {
        float cover = hit.alpha_coverage;
        col = fma(clamp(vec3(params.y, params.z, params.w), 0.0, 1.0), vec3(cover), vec3(0.0));
        skip_fill_light = 1;
    } else if (hit.type == SHADER_SPRITE_PORTAL) {
        float pulse = fma(sin(fma(frame.uTime, 8.0, params.y)), 0.5, 0.5);
        float lum = portal_lum(hit.world_pos);
        float aura = radial_mask(uv, vec2(0.5, 0.52), vec2(0.80, 0.72));
        col = vec3(0.40, 0.05, 0.50) * fma(lum, smoothstep(0.02, 0.70, aura), 0.0);
        vec3 energy = vec3(1.0, fma(pulse, 0.60, 0.40), 1.0);
        col = fma(energy, vec3(fma(lum, 0.12 * pulse, 0.0)), col);
        skip_fill_light = 1;
    } else if (hit.type == SHADER_SPRITE_EXIT_PILLAR) {
        float open_state = clamp(params.x, 0.0, 1.0);
        float pulse = fma(sin(fma(frame.uTime, mix(2.65, 5.4, open_state), 0.0)), 0.48, 0.52);
        vec3 sealed = vec3(1.0, 0.38, 0.08);
        vec3 open_col = vec3(0.15, 0.75, 0.85);
        col = mix(sealed, open_col, open_state) * fma(pulse, 0.55, 0.45);
    } else if (hit.type == SHADER_SPRITE_HUNTER_DRONE) {
        float pulse = fma(sin(fma(frame.uTime, 9.0, sprite_row2(hit.index).w)), 0.5, 0.5);
        col = vec3(fma(pulse, 0.20, 0.10), fma(pulse, 0.20, 0.42), 0.95);
        skip_fill_light = 1;
    } else if (hit.type == SHADER_SPRITE_DRONE_SHOT) {
        float pulse = fma(sin(fma(frame.uTime, 32.0, sprite_row2(hit.index).x * 9.0)), 0.18, 0.82);
        /* Broad cold-white halo — slightly blue-tinted. */
        col = vec3(1.0, 1.0, fma(pulse, 0.12, 0.92));
        skip_fill_light = 1;
    } else {
        return vec4(0.0);
    }

    float fog = exp(-max(hit.perp, 0.0) * 0.06);
    if (skip_fill_light == 0) {
        col = fma(sprite_light(hit.world_pos, hit.world_pos.y), vec3(0.16), col);
    }
    col = fog_mix(clamp(col, 0.0, 1.0), fog);
    return vec4(col, clamp(hit.alpha_coverage, 0.0, dh_sprite_alpha_cap(hit.type)));
}

// -----------------------------------------------------------------------------
// Sprite/world composition and debug overlays
// -----------------------------------------------------------------------------

bool sprite_in_front_of_world(SpriteHit hit, float world_raw, int hit_wall) {
    if (hit.opaque_coverage <= 0.0 && hit.alpha_coverage <= 0.0) {
        return false;
    }
    float wall_limit = hit_wall != 0 ? world_raw : 1000.0;
    return hit.raw < wall_limit - 0.03;
}

vec4 composite_sprites(vec4 world_color, SpriteResolve resolved, float world_raw, int hit_wall) {
    vec4 out_color = world_color;

    if (resolved.nearest_alpha.alpha_coverage > 0.0 &&
        sprite_in_front_of_world(resolved.nearest_alpha, world_raw, hit_wall) &&
        (resolved.nearest_opaque.opaque_coverage <= 0.0 || resolved.nearest_alpha.raw <= resolved.nearest_opaque.raw + 0.02)) {
        vec4 alpha_color = shade_sprite_alpha(resolved.nearest_alpha);
        float alpha = clamp(alpha_color.a, 0.0, 1.0);
        out_color.rgb = mix(out_color.rgb, alpha_color.rgb, alpha);
    }

    if (resolved.nearest_opaque.opaque_coverage > 0.0 &&
        sprite_in_front_of_world(resolved.nearest_opaque, world_raw, hit_wall)) {
        vec4 opaque_color = shade_sprite_opaque(resolved.nearest_opaque);
        float alpha = clamp(opaque_color.a, 0.0, 1.0);
        out_color.rgb = mix(out_color.rgb, opaque_color.rgb, alpha);
    }

    out_color.a = 1.0;
    return out_color;
}

vec4 sprite_occlusion_debug_color(SpriteResolve resolved, float world_raw, int hit_wall) {
    float nearest_sprite_raw = min(resolved.nearest_opaque.raw, resolved.nearest_alpha.raw);
    float depth_band = clamp(1.0 - nearest_sprite_raw / 12.0, 0.12, 1.0);
    vec3 base = hit_wall != 0 ? vec3(0.05, 0.12, 0.28) : vec3(0.03, 0.08, 0.05);
    base += vec3(clamp(1.0 - world_raw / 12.0, 0.0, 0.28));
    if (resolved.nearest_opaque.opaque_coverage > 0.0) {
        return vec4(vec3(depth_band, 0.08, 0.04), 1.0);
    }
    if (resolved.nearest_alpha.alpha_coverage > 0.0) {
        return vec4(vec3(0.55, 0.08, 0.85) * depth_band, 1.0);
    }
    return vec4(base, 1.0);
}
#else
SpriteResolve resolve_sprites(void) {
    return no_sprite_resolve();
}

vec4 composite_sprites(vec4 world_color, SpriteResolve resolved, float world_raw, int hit_wall) {
    return world_color;
}

vec4 sprite_occlusion_debug_color(SpriteResolve resolved, float world_raw, int hit_wall) {
    return hit_wall != 0 ? vec4(0.05, 0.12, 0.28, 1.0) : vec4(0.03, 0.08, 0.05, 1.0);
}
#endif

ivec2 world_map_size() {
    return ivec2(scene.mapSize[0], scene.mapSize[1]);
}

int row_bits(int z) {
    return scene.wallRows[z];
}

bool cell_wall(ivec2 c) {
    ivec2 map_size = world_map_size();
    if (c.x < 0 || c.x >= map_size.x || c.y < 0 || c.y >= map_size.y) return false;
    int row = row_bits(c.y);
    return ((row >> c.x) & 1) != 0;
}

int cell_arch(ivec2 c) {
    ivec2 map_size = world_map_size();
    if (c.x < 0 || c.x >= map_size.x || c.y < 0 || c.y >= map_size.y) return 0;
    if (((scene.archNsRows[c.y] >> c.x) & 1) != 0) return 1;
    if (((scene.archEwRows[c.y] >> c.x) & 1) != 0) return 2;
    return 0;
}

bool arch_stone_at(vec3 p, ivec2 cell, int arch_kind);
bool arch_hit_interval_column(ivec2 cell, int arch_kind, float entry_raw, float exit_raw, out float raw);
bool arch_hit_interval(ivec2 cell, int arch_kind, float entry_raw, float exit_raw, out float raw);

float maze_shadow(vec2 pos_xz, float pos_y) {
    vec3 sun_dir = frame.uSunDir;
    if (sun_dir.y <= 0.0) return 0.0;
    float max_dist = max(0.0, ((1.0 - pos_y) / sun_dir.y) * length(sun_dir.xz)) * 4.0;
    float lh = length(sun_dir.xz);
    if (lh < 1e-5) return 1.0;
    vec2 rd = sun_dir.xz / lh;
    dh_arch_bind(vec3(pos_xz.x, pos_y, pos_xz.y), sun_dir, rd);
    bool blocked = map_dda_occluded(DH_DDA_SUN, pos_xz, rd, max_dist);
    return blocked ? 0.0 : 1.0;
}

float pristine_shadow(vec2 pos_xz, float pos_y) {
#if DH_ENABLE_SOFT_SHADOW
    float sum = 0.0;
    vec3 right = normalize(cross(frame.uSunDir, vec3(0.0, 1.0, 0.0)));
    vec2 rdxz = right.xz * 0.015;
    sum += maze_shadow(pos_xz + rdxz, pos_y);
    sum += maze_shadow(pos_xz - rdxz, pos_y);
    sum += maze_shadow(pos_xz + rdxz * 0.5, pos_y);
    sum += maze_shadow(pos_xz - rdxz * 0.5, pos_y);
    return sum * 0.25;
#else
    return maze_shadow(pos_xz, pos_y);
#endif
}

float point_shadow(vec2 from_xz, vec2 light_xz) {
    float max_dist = length(light_xz - from_xz);
    if (max_dist < 1e-5) return 1.0;
    vec3 from3 = vec3(from_xz.x, 0.5, from_xz.y);
    vec3 to3 = vec3(light_xz.x, 0.5, light_xz.y);
    vec3 delta3 = to3 - from3;
    vec3 rd3 = delta3 / length(delta3);
    vec2 rd_xz = rd3.xz / max(length(rd3.xz), 1e-5);
    dh_arch_bind(from3, rd3, rd_xz);
    bool blocked = map_dda_occluded(DH_DDA_POINT, from_xz, rd_xz, max_dist);
    return blocked ? 0.0 : 1.0;
}

float point_shadow_budgeted(vec2 light_xz) {
    if (g_dyn.shadow_budget <= 0) {
        return 1.0;
    }
    g_dyn.shadow_budget--;
    return point_shadow(g_dyn.pos_xz, light_xz);
}

/* Squared falloff: t = max(1 - dist/r, 0), atten = t² */
float dh_point_falloff(float dist, float inv_radius) {
    float t = max(fma(-dist, inv_radius, 1.0), 0.0);
    return t * t;
}

void dh_dyn_begin(vec2 pos_xz) {
    g_dyn.pos_xz = pos_xz;
    g_dyn.accum = vec3(0.0);
    g_dyn.shadow_budget = DH_MAX_POINT_SHADOW_TESTS;
}

/* Hot path: only per-emitter src + pulse vary; rest lives in g_dyn. */
void dh_accum_point_light(vec2 src, float pulse) {
    vec2 cam_delta = src - frame.uCamPos.xz;
    if (dot(cam_delta, cam_delta) > DH_DYN_LIGHT_MAX_DIST_SQ) {
        return;
    }
    vec2 delta = g_dyn.pos_xz - src;
    float dist_sq = dot(delta, delta);
    if (dist_sq >= g_dyn.radius_sq) {
        return;
    }
    float atten = dh_point_falloff(sqrt(dist_sq), g_dyn.inv_radius);
    float intensity = fma(atten, fma(pulse, g_dyn.energy, 0.0), 0.0);
    if (intensity < 0.01) {
        return;
    }
    float ps = point_shadow_budgeted(src);
    g_dyn.accum = fma(g_dyn.col, vec3(intensity * ps), g_dyn.accum);
}

vec3 gather_dynamic_lights(vec2 pos_xz) {
    dh_dyn_begin(pos_xz);
    for (int i = 0; i < DH_DYN_SLOT_COUNT; i++) {
        if (i < DH_DYN_TELEPORTER_COUNT) {
            if (scene.teleporters[i].z > 0.0) {
                g_dyn.radius_sq = 4.5 * 4.5;
                g_dyn.inv_radius = 1.0 / 4.5;
                g_dyn.energy = 1.5;
                g_dyn.col = vec3(1.0, 0.8, 0.2);
                float pulse = fma(sin(fma(frame.uTime, 8.0, float(i))), 0.2, 0.8);
                dh_accum_point_light(scene.teleporters[i].xy, pulse);
            }
        }
        if (i < DH_DYN_HELLRAISER_COUNT) {
            if (scene.hellraisers[i].w > 0.0) {
                g_dyn.radius_sq = 4.8 * 4.8;
                g_dyn.inv_radius = 1.0 / 4.8;
                g_dyn.energy = 1.25;
                g_dyn.col = vec3(1.0, 0.14, 0.03);
                float pulse = fma(sin(fma(frame.uTime, 11.0, float(i) * 1.7)), 0.28, 0.72);
                dh_accum_point_light(scene.hellraisers[i].xz, pulse);
            }
        }
        if (i < DH_DYN_PLAYER_SHOT_COUNT) {
            if (scene.playerShots[i].w > 0.0) {
                g_dyn.radius_sq = 3.2 * 3.2;
                g_dyn.inv_radius = 1.0 / 3.2;
                g_dyn.energy = 0.52;
                g_dyn.col = vec3(1.0, 0.72, 0.08);
                float pulse = fma(sin(fma(frame.uTime, 18.0, float(i) * 1.3)), 0.18, 0.82);
                dh_accum_point_light(scene.playerShots[i].xz, pulse);
            }
        }
        if (i < DH_DYN_DRONE_SHOT_COUNT) {
            if (scene.droneShots[i].w > 0.0) {
                g_dyn.radius_sq = 3.0 * 3.0;
                g_dyn.inv_radius = 1.0 / 3.0;
                g_dyn.energy = 0.68;
                g_dyn.col = vec3(1.0, 0.98, 0.94);
                float pulse = fma(sin(fma(frame.uTime, 24.0, fma(scene.droneShots[i].y, 10.0, float(i)))), 0.15, 0.85);
                dh_accum_point_light(scene.droneShots[i].xz, pulse);
            }
        }
    }
    return g_dyn.accum;
}

vec3 player_shot_bloom(void) {
    vec3 bloom = vec3(0.0);
    for (int i = 0; i < 16; i++) {
        if (scene.playerShots[i].w <= 0.0) continue;
        vec3 shot = scene.playerShots[i].xyz;
        float shot_raw = dot(shot.xz - g_col.ro.xz, g_col.rdxz);
        if (shot_raw <= 0.0 || shot_raw >= g_col.sprite_max_raw) continue;
        if (!segment_world_visible(g_col.ro, shot)) continue;

        float t = max(dot(shot - g_col.ro, g_col.view_rd), 0.0);
        vec3 closest = fma(g_col.view_rd, vec3(t), g_col.ro);
        float ray_dist = length(closest - shot);
        float core = smoothstep(0.105, 0.012, ray_dist);
        float halo = smoothstep(0.46, 0.04, ray_dist);
        float pulse = fma(sin(fma(frame.uTime, 42.0, float(i))), 0.20, 0.80);
        float depth_fade = exp(-max(shot_raw, 0.0) * 0.045);
        bloom += vec3(1.0, 0.72, 0.10) * ((halo * 0.42 + core * 1.25) * pulse * depth_fade);
    }
    return bloom;
}

void ray_segment_nearest(vec3 ro, vec3 rd, vec3 seg_a, vec3 seg_b, out float ray_dist, out float seg_param) {
    vec3 u = seg_b - seg_a;
    vec3 v = rd;
    vec3 w = ro - seg_a;
    float a = dot(u, u);
    float b = dot(u, v);
    float c = dot(v, v);
    float d = dot(u, w);
    float e = dot(v, w);
    float denom = a * c - b * b;
    float sc;
    float tc;
    if (denom < 1e-7) {
        sc = 0.0;
        tc = max(e / max(c, 1e-5), 0.0);
    } else {
        sc = clamp((b * e - c * d) / denom, 0.0, 1.0);
        tc = max((a * e - b * d) / denom, 0.0);
    }
    vec3 p_seg = seg_a + u * sc;
    vec3 p_ray = ro + v * tc;
    ray_dist = length(p_seg - p_ray);
    seg_param = sc;
}

vec3 drone_shot_bloom(void) {
    vec3 bloom = vec3(0.0);
    const float TRAIL_LEN = 0.82;
    for (int i = 0; i < 12; i++) {
        if (scene.droneShots[i].w <= 0.0) continue;
        vec3 shot = scene.droneShots[i].xyz;
        vec3 shot_dir = scene.droneShotDirs[i].xyz;
        float dir_len = length(shot_dir);
        if (dir_len < 1e-5) continue;
        shot_dir /= dir_len;

        vec3 trail_tail = shot - shot_dir * TRAIL_LEN;
        float head_raw = dot(shot.xz - g_col.ro.xz, g_col.rdxz);
        if (head_raw <= 0.0 || head_raw >= g_col.sprite_max_raw) continue;
        if (!segment_world_visible(g_col.ro, shot)) continue;

        float ray_dist;
        float seg_param;
        ray_segment_nearest(g_col.ro, g_col.view_rd, trail_tail, shot, ray_dist, seg_param);

        float pulse = fma(sin(fma(frame.uTime, 32.0, scene.droneShots[i].y * 9.0 + float(i))), 0.18, 0.82);
        float tail_raw = dot(trail_tail.xz - g_col.ro.xz, g_col.rdxz);
        float depth_raw = mix(tail_raw, head_raw, seg_param);
        float depth_fade = exp(-max(depth_raw, 0.0) * 0.045);
        float streak_weight = mix(0.40, 1.0, smoothstep(0.0, 0.65, seg_param));

        float core = smoothstep(0.16, 0.008, ray_dist);
        float halo = smoothstep(0.62, 0.02, ray_dist);
        bloom += vec3(1.0, 0.98, 0.94) * ((halo * 0.55 + core * 1.50) * pulse * depth_fade * streak_weight);

        float t_head = max(dot(shot - g_col.ro, g_col.view_rd), 0.0);
        vec3 closest_head = fma(g_col.view_rd, vec3(t_head), g_col.ro);
        float head_dist = length(closest_head - shot);
        float head_core = smoothstep(0.09, 0.008, head_dist);
        float head_halo = smoothstep(0.36, 0.02, head_dist);
        bloom += vec3(1.0, 0.98, 0.94) * ((head_halo * 0.40 + head_core * 1.15) * pulse * exp(-max(head_raw, 0.0) * 0.045));
    }
    return bloom;
}

bool arch_stone_at(vec3 p, ivec2 cell, int arch_kind) {
    vec2 local = p.xz - vec2(cell);
    float across = arch_kind == 1 ? local.x : local.y;
    if (local.x < 0.0 || local.x > 1.0 || local.y < 0.0 || local.y > 1.0) return false;
    if (p.y < 0.5 || p.y > 1.0) return false;

    const float radius = 0.495;
    vec2 arch = vec2(across - 0.5, p.y - 0.5);
    return dot(arch, arch) >= radius * radius;
}

/* Visible arches: same 3D solver as shadows, but column ray (never tan*t on Y). */
bool arch_hit_interval_column(ivec2 cell, int arch_kind, float entry_raw, float exit_raw, out float raw) {
    dh_arch_bind_column();
    return arch_hit_interval(cell, arch_kind, entry_raw, exit_raw, raw);
}

/* 3D arches for sun/point/segment shadow DDA — caller must dh_arch_bind() first. */
bool arch_hit_interval(ivec2 cell, int arch_kind, float entry_raw, float exit_raw, out float raw) {
    float t0 = max(entry_raw + 0.001, 0.0);
    float t1 = max(exit_raw - 0.001, t0);
    float s0 = t0 / g_arch.col_xz_comp;
    float s1 = t1 / g_arch.col_xz_comp;
    float best = 1e20;

    const int ARCH_STEPS = 40;
    for (int k = 0; k <= ARCH_STEPS; k++) {
        float s = mix(s0, s1, float(k) / float(ARCH_STEPS));
        vec3 p = g_arch.ro + g_arch.col_rd * s;
        if (arch_stone_at(p, cell, arch_kind)) {
            best = s * g_arch.col_xz_comp;
            break;
        }
    }

    /* Analytic circle crossings in the arch (across, y) plane — catches thin rims between steps. */
    float axis0 = arch_kind == 1 ? g_arch.ro.x - float(cell.x) - 0.5 : g_arch.ro.z - float(cell.y) - 0.5;
    float ax = arch_kind == 1 ? g_arch.col_rd.x : g_arch.col_rd.z;
    float ay = g_arch.col_rd.y;
    float y0 = g_arch.ro.y - 0.5;
    const float R2 = 0.495 * 0.495;
    float a_coef = ax * ax + ay * ay;
    float b_coef = 2.0 * (axis0 * ax + y0 * ay);
    float c_coef = axis0 * axis0 + y0 * y0 - R2;
    float disc = b_coef * b_coef - 4.0 * a_coef * c_coef;
    if (disc >= 0.0 && a_coef > 1e-8) {
        float sdisc = sqrt(disc);
        float inv2a = 0.5 / a_coef;
        float s_roots[2];
        s_roots[0] = (-b_coef - sdisc) * inv2a;
        s_roots[1] = (-b_coef + sdisc) * inv2a;
        for (int j = 0; j < 2; j++) {
            float s = s_roots[j];
            if (s < s0 || s > s1) continue;
            float t = s * g_arch.col_xz_comp;
            if (t >= best) continue;
            vec3 p = g_arch.ro + g_arch.col_rd * s;
            if (arch_stone_at(p, cell, arch_kind)) {
                best = t;
            } else {
                for (int n = 0; n < 2; n++) {
                    float s_in = s + (n == 0 ? 0.003 : -0.003);
                    if (s_in < s0 || s_in > s1) continue;
                    p = g_arch.ro + g_arch.col_rd * s_in;
                    if (arch_stone_at(p, cell, arch_kind)) {
                        best = min(best, s_in * g_arch.col_xz_comp);
                    }
                }
            }
        }
    }

    if (abs(g_arch.col_rd.y) > 1e-5) {
        for (int yk = 0; yk < 2; yk++) {
            float y_tgt = yk == 0 ? 0.5 : 1.0;
            float s = (y_tgt - g_arch.ro.y) / g_arch.col_rd.y;
            if (s < s0 || s > s1) continue;
            float t = s * g_arch.col_xz_comp;
            if (t >= best) continue;
            if (arch_stone_at(g_arch.ro + g_arch.col_rd * s, cell, arch_kind)) {
                best = t;
            }
        }
    }

    if (best < 1e19) {
        raw = best;
        return true;
    }
    return false;
}

bool map_dda_occluded(int mode, vec2 origin, vec2 dir_norm, float max_dist) {
    float eps = (mode == DH_DDA_SEGMENT) ? 0.004 : 0.005;
    vec2 p = fma(dir_norm, vec2(eps), origin);
    ivec2 cell = ivec2(floor(p));
    vec2 del;
    del.x = abs(dir_norm.x) < 1e-7 ? 1e30 : abs(1.0 / dir_norm.x);
    del.y = abs(dir_norm.y) < 1e-7 ? 1e30 : abs(1.0 / dir_norm.y);
    ivec2 st = ivec2(dir_norm.x < 0.0 ? -1 : 1, dir_norm.y < 0.0 ? -1 : 1);
    vec2 side;
    if (dir_norm.x < 0.0) side.x = (p.x - float(cell.x)) * del.x;
    else                 side.x = (float(cell.x) + 1.0 - p.x) * del.x;
    if (dir_norm.y < 0.0) side.y = (p.y - float(cell.y)) * del.y;
    else                  side.y = (float(cell.y) + 1.0 - p.y) * del.y;

    float dist = 0.0;
    float entry_raw = 0.0;
    ivec2 map_size = world_map_size();
    for (int i = 0; i < MAP_DDA_MAX_STEPS; i++) {
        if (cell.x < 0 || cell.x >= map_size.x || cell.y < 0 || cell.y >= map_size.y) {
            return mode == DH_DDA_SEGMENT;
        }
        if (cell_wall(cell)) {
            return dist < max_dist;
        }

        float exit_raw = min(side.x, side.y);
        if (mode != DH_DDA_POINT) {
            int arch_kind = cell_arch(cell);
            if (arch_kind != 0) {
                float arch_raw;
                float arch_exit = (mode == DH_DDA_SEGMENT) ? min(exit_raw, max_dist) : min(side.x, side.y);
                float arch_entry = (mode == DH_DDA_SEGMENT) ? entry_raw : dist;
                if (arch_hit_interval(cell, arch_kind, arch_entry, arch_exit, arch_raw)) {
                    if (mode == DH_DDA_SEGMENT) {
                        if (arch_raw < max_dist - 0.004) return true;
                    } else if (arch_raw < max_dist) {
                        return true;
                    }
                }
            }
        }

        if (mode == DH_DDA_SEGMENT) {
            if (exit_raw >= max_dist) return false;
            if (side.x < side.y) {
                entry_raw = side.x;
                side.x += del.x;
                cell.x += st.x;
            } else {
                entry_raw = side.y;
                side.y += del.y;
                cell.y += st.y;
            }
        } else {
            if (side.x < side.y) {
                dist = side.x;
                side.x += del.x;
                cell.x += st.x;
            } else {
                dist = side.y;
                side.y += del.y;
                cell.y += st.y;
            }
            if (dist > max_dist) return false;
        }
    }
    return mode == DH_DDA_SEGMENT;
}

bool segment_world_visible(vec3 from_pos, vec3 to_pos) {
    vec3 delta3 = to_pos - from_pos;
    float max_raw = length(delta3.xz);
    if (max_raw < 1e-5) return true;
    vec2 rdxz = delta3.xz / max_raw;
    vec3 rd3 = delta3 / max(length(delta3), 1e-5);
    dh_arch_bind(from_pos, rd3, rdxz);
    return !map_dda_occluded(DH_DDA_SEGMENT, from_pos.xz, rdxz, max_raw);
}

void cast_prim(out float raw, out float perp, out int side_out, out int hit_wall) {
    float rdx = g_col.rdxz.x;
    float rdz = g_col.rdxz.y;
    float ox = g_col.ro.x;
    float oz = g_col.ro.z;
    int mx = int(floor(ox));
    int mz = int(floor(oz));
    vec2 sd;
    vec2 del;
    del.x = abs(rdx) < 1e-7 ? 1e30 : abs(1.0 / rdx);
    del.y = abs(rdz) < 1e-7 ? 1e30 : abs(1.0 / rdz);
    ivec2 st;
    int side;
    if (rdx < 0.0) {
        st.x = -1;
        sd.x = (ox - float(mx)) * del.x;
    } else {
        st.x = 1;
        sd.x = (float(mx) + 1.0 - ox) * del.x;
    }
    if (rdz < 0.0) {
        st.y = -1;
        sd.y = (oz - float(mz)) * del.y;
    } else {
        st.y = 1;
        sd.y = (float(mz) + 1.0 - oz) * del.y;
    }
    hit_wall = 0;
    side = 0;
    int guard;
    float entry_raw = 0.0;
    int start_arch_kind = cell_arch(ivec2(mx, mz));
    if (start_arch_kind != 0) {
        float arch_raw;
        float seg_exit = min(sd.x, sd.y);
        bool arch_hit = arch_hit_interval_column(ivec2(mx, mz), start_arch_kind, 0.0, seg_exit, arch_raw);
        if (!arch_hit) {
            arch_hit = arch_hit_interval_column(ivec2(mx, mz), start_arch_kind, 0.0, seg_exit + 0.015, arch_raw);
        }
        if (arch_hit) {
            hit_wall = 1;
            raw = arch_raw;
            side_out = side;
            perp = raw * dot(g_col.rdxz, g_col.fwd);
            return;
        }
    }
    for (guard = 0; guard < 128; guard++) {
        if (sd.x < sd.y) {
            entry_raw = sd.x;
            sd.x += del.x;
            mx += st.x;
            side = 0;
        } else {
            entry_raw = sd.y;
            sd.y += del.y;
            mz += st.y;
            side = 1;
        }
        ivec2 map_size = world_map_size();
        if (mx < 0 || mz < 0 || mx >= map_size.x || mz >= map_size.y) {
            hit_wall = 1;
            break;
        }
        if (cell_wall(ivec2(mx, mz))) {
            hit_wall = 1;
            break;
        }
        int arch_kind = cell_arch(ivec2(mx, mz));
        if (arch_kind != 0) {
            float arch_raw;
            float seg_exit = min(sd.x, sd.y);
            bool arch_hit = arch_hit_interval_column(ivec2(mx, mz), arch_kind, entry_raw, seg_exit, arch_raw);
            if (!arch_hit) {
                arch_hit = arch_hit_interval_column(ivec2(mx, mz), arch_kind, max(entry_raw - 0.015, 0.0), seg_exit + 0.015, arch_raw);
            }
            if (arch_hit) {
                hit_wall = 1;
                raw = arch_raw;
                side_out = side;
                perp = raw * dot(g_col.rdxz, g_col.fwd);
                return;
            }
        }
    }
    if (hit_wall == 0) {
        raw = 1000.0;
        side_out = side;
        perp = raw * dot(g_col.rdxz, g_col.fwd);
        return;
    }
    raw = (side == 0) ? (sd.x - del.x) : (sd.y - del.y);
    if (raw < 0.0) raw = 0.0;
    perp = max(raw * dot(g_col.rdxz, g_col.fwd), 0.02);
    side_out = side;
}

/* Procedural sky clouds. Lower CLOUD_COVERAGE to sprout more clouds; raise it for a clearer sky. */
const float CLOUD_COVERAGE = 0.52;
const float CLOUD_SOFTNESS = 0.30;
const float CLOUD_SCALE = 1.65;
const float CLOUD_SPEED = 0.018;
const float CLOUD_OPACITY = 0.55;
const float GODRAY_STRENGTH = 0.95;
const float GODRAY_REACH = 3.6;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise2d(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    float ab = fma(b - a, u.x, a);
    float cd = fma(d - c, u.x, c);
    return fma(cd - ab, u.y, ab);
}

float fbm(vec2 p) {
    float v = 0.0;
    float amp = 0.5;
    mat2 drift = mat2(1.62, 1.17, -1.17, 1.62);
    for (int i = 0; i < 5; i++) {
        v = fma(noise2d(p), amp, v);
        p = drift * p + vec2(8.3, 2.7);
        amp *= 0.5;
    }
    return v;
}

/* World-locked clouds (sun queries). */
float cloud_mask_world(vec3 rd) {
    if (rd.y <= 0.01) return 0.0;

    vec3 n = normalize(rd);
    vec2 wind = vec2(frame.uTime * CLOUD_SPEED, frame.uTime * CLOUD_SPEED * 0.37);
    vec2 uv = n.xz / (1.0 + max(n.y, 0.05));
    uv = fma(uv, vec2(CLOUD_SCALE), wind);

    float broad = fbm(uv);
    float wisps = fbm(uv * 2.8 + vec2(19.1, -7.4));
    float cloud = fma(broad, 0.78, wisps * 0.22);

    float horizon_fade = smoothstep(0.02, 0.34, rd.y);
    float zenith_fade = fma(-smoothstep(0.92, 1.0, rd.y), 0.12, 1.0);
    return smoothstep(CLOUD_COVERAGE, CLOUD_COVERAGE + CLOUD_SOFTNESS, cloud) * horizon_fade * zenith_fade;
}

/* Viewer clouds: ray_ang (smooth horizontal) + pitch — matches turn direction, no column stripes. */
float cloud_mask_sky(float rd_y) {
    if (rd_y <= 0.01) return 0.0;

    vec2 wind = vec2(frame.uTime * CLOUD_SPEED, frame.uTime * CLOUD_SPEED * 0.37);
    float elev = asin(clamp(rd_y, -1.0, 1.0));
    vec2 uv = vec2(g_col.ray_ang * 2.15, elev * 1.35);
    uv = fma(uv, vec2(CLOUD_SCALE), wind);

    float broad = fbm(uv);
    float wisps = fbm(uv * 2.8 + vec2(19.1, -7.4));
    float cloud = fma(broad, 0.78, wisps * 0.22);

    float horizon_fade = smoothstep(0.02, 0.34, rd_y);
    float zenith_fade = fma(-smoothstep(0.92, 1.0, rd_y), 0.12, 1.0);
    return smoothstep(CLOUD_COVERAGE, CLOUD_COVERAGE + CLOUD_SOFTNESS, cloud) * horizon_fade * zenith_fade;
}

float godray_mask(vec3 rd, vec3 sund, float local_clouds) {
    if (rd.y <= 0.0) return 0.0;

    vec3 Su = normalize(sund);
    vec3 Ru = normalize(rd);
    float mu = max(dot(Ru, Su), 0.0);

    vec3 right = normalize(cross(Su, abs(Su.y) > 0.95 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0)));
    vec3 up = normalize(cross(right, Su));
    float sun_clouds = cloud_mask_world(Su);
    sun_clouds = max(sun_clouds, cloud_mask_world(normalize(Su + right * 0.11)));
    sun_clouds = max(sun_clouds, cloud_mask_world(normalize(Su - right * 0.11)));
    sun_clouds = max(sun_clouds, cloud_mask_world(normalize(Su + up * 0.09)));

    float near_clouds = cloud_mask_sky(normalize(mix(Su, Ru, 0.20)).y);
    float mid_clouds = cloud_mask_sky(normalize(mix(Su, Ru, 0.52)).y);
    float cloud_edge = abs(sun_clouds - near_clouds) + abs(near_clouds - mid_clouds);
    float obscured_sun = smoothstep(0.08, 0.42, sun_clouds);
    float cone = pow(mu, GODRAY_REACH) * smoothstep(0.01, 0.22, Ru.y);

    vec2 streak_uv = vec2(g_col.ray_ang * 2.15, asin(clamp(Ru.y, -1.0, 1.0)));
    float streak_noise = fbm(fma(streak_uv, vec2(18.0), vec2(frame.uTime * 0.035, -frame.uTime * 0.02)));
    float streaks = smoothstep(0.34, 0.82, streak_noise);
    float clear_path = fma(-local_clouds, 0.45, 1.0);

    return cone * obscured_sun * clear_path * fma(cloud_edge, 2.8, 0.55) * fma(streaks, 0.95, 0.45);
}

vec3 sky_shade(vec3 rd, vec3 sund) {
    float h = clamp(fma(rd.y, 0.55, 0.48), 0.0, 1.0);
    vec3 zen = vec3(0.06, 0.09, 0.20);
    vec3 hor = vec3(0.52, 0.32, 0.22);
    vec3 base = mix(hor, zen, pow(h, 0.62));
    vec3 Su = normalize(sund);
    vec3 Ru = normalize(rd);
    float mu = max(dot(Ru, Su), 0.0);
    float disk = smoothstep(0.991, 0.9998, mu);
    vec3 glow = vec3(1.0, 0.94, 0.74) * disk * 11.5;
    glow = fma(vec3(1.0, 0.48, 0.12), vec3(step(0.988, mu) * pow(mu, 96.0) * 3.1), glow);
    glow = fma(vec3(1.0, 0.58, 0.22), vec3(pow(mu, 18.0) * 0.20), glow);
    float stm = fma(sin(frame.uTime * 0.55), 0.02, 1.0);
    vec3 sky = fma(base, vec3(stm), glow);

    float clouds = cloud_mask_sky(Ru.y);
    float rays = godray_mask(Ru, Su, clouds);
    float sun_warmth = clamp(fma(pow(mu, 5.0), 1.8, sund.y * 0.35), 0.0, 1.0);
    vec3 cloud_shadow = vec3(0.16, 0.15, 0.21);
    vec3 cloud_light = vec3(0.86, 0.66, 0.48);
    vec3 cloud_col = mix(cloud_shadow, cloud_light, sun_warmth);
    float sun_cutout = fma(-disk, 0.45, 1.0);

    sky = mix(sky, cloud_col + glow * 0.10, clouds * CLOUD_OPACITY * sun_cutout);
    sky = fma(vec3(1.0, 0.68, 0.30), vec3(rays * GODRAY_STRENGTH), sky);
    return sky;
}

/* Phase 2: Material IDs (must match CPU-side MAT_* defines) */
#if DH_ENABLE_MATERIALS
const int MAT_STONE        = 0;
const int MAT_METAL        = 1;
const int MAT_FLESH        = 2;
const int MAT_EMISSIVE     = 3;
const int MAT_WOOD         = 4;
const int MAT_WATER        = 5;
const int MAT_BONE         = 6;
const int MAT_RUSTED_METAL = 7;

int extract_material(ivec2 cell) {
    int idx = cell.y * 4 + cell.x / 8;
    int shift = (cell.x % 8) * 4;
    return bitfieldExtract(scene.materialRows[idx], shift, 4);
}

/* shade_material: subtly modifies base wall color based on material type.
 * base_col is already fully lit. Materials add gentle tints — NOT drastic recoloring. */
vec3 shade_material(int mat_id, vec3 base_col, float ndotl, float sl, float perp,
                    vec3 view_rd, vec3 normal, vec2 wp) {
    /* Stone: subtle per-texel brightness noise (procedural roughness) */
    if (mat_id == MAT_STONE) {
        float h = hash12(wp * 4.0) * 0.08 - 0.04;
        return base_col * (1.0 + h);
    }

    /* Metal: slight cool desaturation + faint specular glint */
    if (mat_id == MAT_METAL) {
        vec3 R = reflect(-normalize(frame.uSunDir), normal);
        float spec = pow(max(dot(R, -normalize(view_rd)), 0.0), 32.0) * 0.25 * sl;
        float luma = dot(base_col, vec3(0.299, 0.587, 0.114));
        vec3 cool = mix(base_col, vec3(luma) * vec3(0.85, 0.88, 1.0), 0.3);
        return cool + vec3(spec);
    }

    /* Flesh/Organic: very slight warm-red push */
    if (mat_id == MAT_FLESH) {
        return base_col * vec3(1.08, 0.95, 0.90);
    }

    /* Emissive: warm amber glow, gentle pulse */
    if (mat_id == MAT_EMISSIVE) {
        float pulse = 0.6 + 0.2 * sin(frame.uTime * 2.5 + wp.x * 1.5 + wp.y * 1.2);
        return mix(base_col, vec3(0.9, 0.6, 0.15), 0.7) * (1.0 + pulse);
    }

    /* Wood: very subtle warm shift, not a recolor */
    if (mat_id == MAT_WOOD) {
        float grain = abs(sin(wp.y * 10.0 + hash12(floor(wp)) * 6.28)) * 0.04 * sl;
        return base_col * vec3(1.06, 0.97, 0.85) + vec3(grain, grain * 0.5, 0.0);
    }

    /* Bone: slight pale desaturation + specular */
    if (mat_id == MAT_BONE) {
        float luma = dot(base_col, vec3(0.299, 0.587, 0.114));
        vec3 R = reflect(-normalize(frame.uSunDir), normal);
        float spec = pow(max(dot(R, -normalize(view_rd)), 0.0), 64.0) * 0.2 * sl;
        return mix(base_col, vec3(luma * 1.3), 0.25) + vec3(spec);
    }

    /* Rusted Metal: subtle orange-brown tint variation */
    if (mat_id == MAT_RUSTED_METAL) {
        float rust_var = hash12(floor(wp) * 3.0) * 0.06;
        vec3 R = reflect(-normalize(frame.uSunDir), normal);
        float spec = pow(max(dot(R, -normalize(view_rd)), 0.0), 8.0) * 0.15 * sl;
        return base_col * vec3(1.05 + rust_var, 0.95, 0.85) + vec3(spec * 0.5, spec * 0.2, 0.0);
    }

    return base_col;
}
#endif

vec3 shade_floor_at(vec2 fxz, float floor_dist) {
    
    bool on_pad = false;
    float pad_pulse = 1.0;
    for (int i = 0; i < 6; i++) {
        if (scene.teleporters[i].z > 0.0) {
            vec2 tp = scene.teleporters[i].xy;
            if (abs(fxz.x - tp.x) < 0.45 && abs(fxz.y - tp.y) < 0.45) {
                on_pad = true;
                pad_pulse = fma(sin(fma(frame.uTime, 8.0, float(i))), 0.2, 0.8);
                if (abs(fxz.x - tp.x) < 0.35 && abs(fxz.y - tp.y) < 0.35) {
                    pad_pulse = 1.0;
                }
                break;
            }
        }
    }
    if (on_pad) {
        float fog = exp(-floor_dist * 0.09);
        vec3 tcol = vec3(1.0, 0.8, 0.2) * pad_pulse;
        return fog_mix(tcol, fog);
    }
    
    float sl = pristine_shadow(fxz, 0.0);
    
    float fog = exp(-floor_dist * 0.09);
    
    vec3 fcol = vec3(0.18, 0.11, 0.07);
    float ndly = clamp(frame.uSunDir.y, 0.0, 1.0);
    float shade = mix(0.46, 1.0, sl);
    fcol *= fma(ndly, 0.82 * shade, 0.18);
    
    float base_r = fcol.r;
    float base_g = fcol.g;
    float base_b = fcol.b;
    float shadow_blend = 1.0 - min(1.0, fcol.r * 5.0);
    base_r = fma(shadow_blend, 0.05, base_r);
    base_g = fma(shadow_blend, 0.03, base_g);
    base_b = fma(shadow_blend, 0.22, base_b);
    
    fcol = vec3(base_r, base_g, base_b) + gather_dynamic_lights(fxz);
    fcol = clamp(fcol, 0.0, 1.0);
    
    fcol = fog_mix(fcol, fog);
    return fcol;
}

/* Floor below a wall column — distance scales from wall hit (matches wtop/wbot geometry). */
vec3 shade_floor_under_wall(float wall_raw) {
    float p = max(g_col.pct - g_col.mid, 1.0);
    float wall_span = max(g_col.wbot - g_col.mid, 1.0);
    float floor_raw = wall_raw * wall_span / p;
    vec2 fxz = fma(g_col.rdxz, vec2(floor_raw), g_col.ro.xz);
    float floor_dist = length(fxz - g_col.ro.xz);
    return shade_floor_at(fxz, floor_dist);
}

vec3 shade_floor_open(void) {
    if (g_col.screen_tan_pitch >= 0.0)
        return vec3(0.1, 0.08, 0.07);
    vec3 frd = dh_floor_ray();
    if (abs(frd.y) < 1e-5)
        return vec3(0.08, 0.07, 0.1);
    float tf = -g_col.ro.y / frd.y;
    if (tf <= 0.0)
        return vec3(0.08, 0.07, 0.1);
    vec2 fxz = fma(frd.xz, vec2(tf), g_col.ro.xz);
    float floor_dist = length(fxz - g_col.ro.xz);
    return shade_floor_at(fxz, floor_dist);
}

void main() {
    float fx = gl_FragCoord.x;
    float fy = gl_FragCoord.y;
    float sh = frame.uResolution.y;
    /* Normalize fragment Y to the raycaster's canonical orientation before
     * deriving NDC/horizon math, so backend presentation details do not leak
     * into game-space shading behavior. */
    fy = sh - fy;
    if (frame.uSpriteDebugMode != 0) {
        float signal = sprite_debug_signal();
        if (fx < 8.0 && fy < 8.0) {
            fragColor = vec4(fract(signal * 0.11), fract(signal * 0.07), fract(signal * 0.03), 1.0);
            return;
        }
    }
    vec2 ndc = fma((vec2(fx, fy) + vec2(0.5)) / frame.uResolution, vec2(2.0), vec2(-1.0));

    float flat_ndc_y = fma(2.0 / frame.uResolution.y, frame.uHorizonShiftPx, ndc.y);
    vec4 un_near = frame.uFlatInvVP * vec4(ndc.x, flat_ndc_y, -1.0, 1.0);
    vec4 un_far = frame.uFlatInvVP * vec4(ndc.x, flat_ndc_y, 1.0, 1.0);
    vec3 view_rd = normalize(un_far.xyz / un_far.w - un_near.xyz / un_near.w);

    /* Walls/floor/sprites xz: column rdxz; sprite height + bloom use view_rd pitch. */
    float ray_ang = frame.uYaw + atan(ndc.x * frame.uTanHalfFov);
    vec2 rdxz = normalize(vec2(sin(ray_ang), cos(ray_ang)));
    vec2 fwd = normalize(vec2(sin(frame.uYaw), cos(frame.uYaw)));
    g_col.ro = frame.uCamPos;
    g_col.view_rd = view_rd;
    g_col.rdxz = rdxz;
    g_col.fwd = fwd;
    g_col.ray_ang = ray_ang;
    g_col.pct = sh - fy - 0.5;
    g_col.mid = frame.uHorizonPxFromTop;
    g_col.sprite_max_raw = 1000.0;
    dh_col_derive();
    float raw, perp;
    int side_hit, hit_w;
    cast_prim(raw, perp, side_hit, hit_w);
    g_col.sprite_max_raw = hit_w == 0 ? 1000.0 : raw;
    SpriteResolve sprite_resolve = resolve_sprites();
#if DH_ENABLE_BLOOM
    vec3 shot_bloom = player_shot_bloom() + drone_shot_bloom();
#else
    vec3 shot_bloom = vec3(0.0);
#endif

#if DH_ENABLE_ATMOSPHERE
    /* Phase 3C: Volumetric god ray enhancement — 4 samples along primary ray. */
    {
        float sun_dot = max(dot(normalize(view_rd), normalize(frame.uSunDir)), 0.0);
        if (sun_dot > 0.3 && frame.uSunDir.y > 0.1) {
            float volumetric = 0.0;
            float sample_max = (hit_w == 0) ? 12.0 : raw;
            for (int i = 1; i <= 4; i++) {
                float sample_raw = sample_max * float(i) * 0.2;
                vec2 sample_pos = g_col.ro.xz + g_col.rdxz * sample_raw;
                ivec2 sc = ivec2(floor(sample_pos));
                if (sc.x >= 0 && sc.y >= 0 && sc.x < int(frame.uMapSize.x) && sc.y < int(frame.uMapSize.y)) {
                    if (!cell_wall(sc)) volumetric += 0.25;
                }
            }
            volumetric *= pow(sun_dot, 3.0) * 0.12;
            shot_bloom += vec3(1.0, 0.75, 0.35) * volumetric;
        }
    }
#endif
    if (frame.uSpriteDebugMode == 2) {
        fragColor = sprite_occlusion_debug_color(sprite_resolve, hit_w == 0 ? 1000.0 : raw, hit_w);
        return;
    }

    if (hit_w == 0) {
        vec4 world_color = (g_col.pct < g_col.mid) ? vec4(sky_shade(dh_sky_ray(), frame.uSunDir), 1.0)
                                                   : vec4(shade_floor_open(), 1.0);
        world_color.rgb = clamp(world_color.rgb + shot_bloom, 0.0, 1.0);
        fragColor = composite_sprites(world_color, sprite_resolve, raw, hit_w);
    } else {
        perp = max(perp, 0.02);
        dh_col_set_wall_span(perp);
        if (g_col.pct < g_col.wtop) {
            vec4 world_color = vec4(clamp(sky_shade(dh_sky_ray(), frame.uSunDir) + shot_bloom, 0.0, 1.0), 1.0);
            fragColor = composite_sprites(world_color, sprite_resolve, raw, hit_w);
        } else if (g_col.pct <= g_col.wbot) {
            vec2 wp = fma(g_col.rdxz, vec2(raw), g_col.ro.xz);
            float hit_y = clamp(fma(-(g_col.pct - g_col.wtop), 1.0 / g_col.line_h, 1.0), 0.0, 1.0);
#if DH_ENABLE_POSTPROCESS
            /* 6D: Shadow dithering — screen-space jitter on the wall shadow ray origin only.
             * Applied here rather than inside pristine_shadow() so sprites and floors are unaffected. */
            vec2 shadow_wp = wp + (vec2(hash12(gl_FragCoord.xy * 0.1), hash12(gl_FragCoord.yx * 0.1)) - 0.5) * 0.03;
            float sl = pristine_shadow(shadow_wp, hit_y);
#else
            float sl = pristine_shadow(wp, hit_y);
#endif
            float nx, nz;
            if (side_hit == 0) {
                nx = g_col.rdxz.x < 0.0 ? 1.0 : -1.0;
                nz = 0.0;
            } else {
                nx = 0.0;
                nz = g_col.rdxz.y < 0.0 ? 1.0 : -1.0;
            }
            float ndotl = max(0.0, nx * frame.uSunDir.x + nz * frame.uSunDir.z);
            float att = 1.0 / (1.0 + perp * 0.11);
            float diffuse = 0.78 * ndotl * att * sl;
            float lum = fma(0.22, att, diffuse);
            lum = clamp(lum, 0.05, 1.35);
            if (side_hit != 0) lum *= 0.90;
            vec3 wcol = vec3(0.55, 0.18, 0.12) * lum;
        
#if DH_ENABLE_MATERIALS
            {
                ivec2 hit_cell = ivec2(floor(wp));
                int mat_id = extract_material(hit_cell);
                wcol = shade_material(mat_id, wcol, ndotl, sl, perp,
                                      view_rd, vec3(nx, 0.0, nz), wp);
            }
#endif
        
            wcol += gather_dynamic_lights(wp);
            wcol = clamp(wcol, 0.0, 1.0);

#if DH_ENABLE_ATMOSPHERE
            /* Phase 3B: Corner ambient occlusion.
             * Darkens only at actual inside corners where two walls meet at 90°.
             * Uses the hit point's distance to the corner junction point. */
            {
                float ao = 1.0;
                ivec2 hit_cell = ivec2(floor(wp));
                if (side_hit == 0) {
                    /* Hit an X-face (wall runs in Z). Corners exist at Z integers. */
                    float fz = fract(wp.y); /* distance along Z within cell */
                    /* Check both ends of this cell face for perpendicular walls. */
                    ivec2 check_lo = hit_cell + ivec2(0, -1);
                    ivec2 check_hi = hit_cell + ivec2(0,  1);
                    float ao_lo = cell_wall(check_lo) ? smoothstep(0.35, 0.0, fz) : 0.0;
                    float ao_hi = cell_wall(check_hi) ? smoothstep(0.65, 1.0, fz) : 0.0;
                    ao = 1.0 - max(ao_lo, ao_hi) * 0.3;
                } else {
                    /* Hit a Z-face (wall runs in X). Corners exist at X integers. */
                    float fx = fract(wp.x);
                    ivec2 check_lo = hit_cell + ivec2(-1, 0);
                    ivec2 check_hi = hit_cell + ivec2( 1, 0);
                    float ao_lo = cell_wall(check_lo) ? smoothstep(0.35, 0.0, fx) : 0.0;
                    float ao_hi = cell_wall(check_hi) ? smoothstep(0.65, 1.0, fx) : 0.0;
                    ao = 1.0 - max(ao_lo, ao_hi) * 0.3;
                }
                wcol *= ao;
            }
            /* Phase 3A: Height-gradient wall fog. */
            float fog = height_fog(perp, g_col.view_rd);
#else
            float fog = exp(-perp * 0.06);
#endif
            wcol = fog_mix(wcol, fog);
            wcol = clamp(wcol + shot_bloom, 0.0, 1.0);
            fragColor = composite_sprites(vec4(wcol, 1.0), sprite_resolve, raw, hit_w);
        } else {
            fragColor = composite_sprites(vec4(clamp(shade_floor_under_wall(raw) + shot_bloom, 0.0, 1.0), 1.0), sprite_resolve, raw, hit_w);
        }
    }

#if DH_ENABLE_POSTPROCESS
    /* Phase 6: Post-process polish. */
    {
        /* 6A: Bloom.
         * With feedback: 2-iteration Kawase blur from previous frame — real screen-space bloom.
         * 8 diagonal samples (4 at 1.5px offset, 4 at 3.5px offset), luminance-thresholded.
         * Without feedback: simple luminance-threshold brightness boost on the current frame. */
#if DH_ENABLE_FRAME_FEEDBACK
        {
            vec2 screen_uv = gl_FragCoord.xy / frame.uResolution;
            vec2 texel = 1.0 / frame.uResolution;
            vec3 bloom = vec3(0.0);
            /* Iteration 1: 1.5-pixel diagonal offset */
            bloom += sample_prev_frame(screen_uv + vec2(-1.5, -1.5) * texel);
            bloom += sample_prev_frame(screen_uv + vec2( 1.5, -1.5) * texel);
            bloom += sample_prev_frame(screen_uv + vec2(-1.5,  1.5) * texel);
            bloom += sample_prev_frame(screen_uv + vec2( 1.5,  1.5) * texel);
            /* Iteration 2: 3.5-pixel diagonal offset, half weight */
            bloom += sample_prev_frame(screen_uv + vec2(-3.5, -3.5) * texel) * 0.5;
            bloom += sample_prev_frame(screen_uv + vec2( 3.5, -3.5) * texel) * 0.5;
            bloom += sample_prev_frame(screen_uv + vec2(-3.5,  3.5) * texel) * 0.5;
            bloom += sample_prev_frame(screen_uv + vec2( 3.5,  3.5) * texel) * 0.5;
            bloom /= 6.0; /* normalize: 4 full + 4 half-weight */
            /* Threshold: only bright pixels contribute to bloom */
            float brightness = dot(bloom, vec3(0.2126, 0.7152, 0.0722));
            bloom *= smoothstep(0.5, 1.0, brightness) * 0.35;
            fragColor.rgb += bloom;
        }
#else
        {
            /* Fallback: proximity brightness boost on the current frame. */
            float lum = dot(fragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
            float bloom_knee = smoothstep(0.6, 1.1, lum);
            fragColor.rgb += fragColor.rgb * bloom_knee * 0.25;
        }
#endif

        /* 6B: Vignette — subtle edge darkening (0.20 strength, tuning range 0.15–0.25). */
        float vignette = 1.0 - 0.20 * pow(length(ndc * 0.7), 2.0);
        fragColor.rgb *= vignette;

        /* 6C: Film grain — subtle temporal noise breaks up banding, sells retro aesthetic.
         * Multiplier 137.0 is coprime with typical hash periods, prevents repetition patterns. */
        float grain = (hash12(gl_FragCoord.xy + fract(frame.uTime) * 137.0) - 0.5) * 0.025;
        fragColor.rgb += grain;

        fragColor.rgb = clamp(fragColor.rgb, 0.0, 1.0);
    }
#endif

#if DH_ENABLE_ATMOSPHERE
    /* Phase 3D: Damage chromatic aberration / color shift. */
    if (frame.uPainFlash > 0.0) {
#if DH_ENABLE_FRAME_FEEDBACK
        /* Radial CA: blend offset previous-frame R/B with current frame to avoid green flash. */
        vec2 screen_uv = gl_FragCoord.xy / frame.uResolution;
        vec2 center = vec2(0.5);
        vec2 ca_dir = normalize(screen_uv - center) * frame.uPainFlash;
        float ca_offset = 2.0 / frame.uResolution.x;
        float ca_r = sample_prev_frame_offset(screen_uv, ca_dir * ca_offset).r;
        float ca_b = sample_prev_frame_offset(screen_uv, -ca_dir * ca_offset).b;
        /* Blend: 60% shifted previous-frame channel, 40% current — prevents green dominance. */
        float blend = 0.6 * frame.uPainFlash;
        fragColor.r = mix(fragColor.r, ca_r, blend);
        fragColor.b = mix(fragColor.b, ca_b, blend);
#else
        /* Fallback: color tint shift (no feedback texture). */
        fragColor.r *= fma(frame.uPainFlash, 0.15, 1.0);
        fragColor.b *= fma(frame.uPainFlash, -0.10, 1.0);
#endif
    }
#endif
}
