/***************************************************************************************************
 *  Situation — first-person “Doom-era” raycast corridor shooter (CPU raycasting + flat sprites)
 *
 *  Ray/DDA maze, mouse-look camera, WASD strafe, hitscan fire, demons, exit.
 *  Directional sun: diffuse lighting on walls (face normals) and billboards; soft floor shadows under props.
 *  Skydome: optional fullscreen shader (sun + horizon) with maze shadow on the floor; falls back to flat bands if compile fails.
 *
 *  Title: Space / Enter — start    I — instructions    Esc — quit
 *  Play: WASD — move / strafe    Mouse — yaw + pitch    Space — jump
 *        Left click / Ctrl — shoot    R — restart after win
 *
 *  BGM: two-voice macabre phrases (triangle + square) on an 8-note Phrygian grid, stepped by timer oscillator.
 *
 *  ──────────────────────────────────────────────────────────────────────────────
 *  OPENGL SHADER LIMITATION (NVIDIA)
 *  ──────────────────────────────────────────────────────────────────────────────
 *  The demon_hunt_sky.fs fragment shader exceeds the NVIDIA OpenGL SPIR-V driver
 *  instruction limit on GTX 10xx/16xx/20xx series. When loaded via the SPIR-V path
 *  (glSpecializeShader), the driver returns error -641 (OPENGL_SPIRV_FS_SPECIALIZE_FAILED)
 *  with the message "too many instructions". The GLSL text path (glShaderSource) may
 *  succeed on some drivers but is not guaranteed.
 *
 *  This is NOT a bug in the shader or the library — it's a hard driver limit on the
 *  number of native GPU instructions the NVIDIA GL frontend will accept for a single
 *  shader stage. The same SPIR-V compiles and runs without issue on Vulkan (where
 *  pipeline compilation has no such frontend limit).
 *
 *  If you're debugging why the sky shader fails on OpenGL:
 *    - Check SituationGetLastErrorCode() — expect -641
 *    - Check SituationGetLastErrorMessage() — expect "too many instructions"
 *    - The game handles this gracefully: falls back to flat color bands for the sky
 *    - Build and run with Vulkan for the full sky shader experience:
 *        build_examples.bat vulkan demon_hunt
 *  ──────────────────────────────────────────────────────────────────────────────
 ***************************************************************************************************/

#if defined(_WIN32)
    #define NOMINMAX
#endif

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_VULKAN
#endif

#include "situation.h"
#include "examples/demon_hunt_sky_spirv_embed.h"
#include "examples/demon_hunt_sky_frame.h"
#include <cglm/cglm.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAP_MAX_W 32
#define MAP_MAX_H 32
#define MAP_DEFAULT_W 24
#define MAP_DEFAULT_H 24

/* Material IDs (4 bits, 0–15) for wall cells. Packed into materialRows[] SSBO. */
#define MAT_STONE        0
#define MAT_METAL        1
#define MAT_FLESH        2
#define MAT_EMISSIVE     3
#define MAT_WOOD         4
#define MAT_WATER        5
#define MAT_BONE         6
#define MAT_RUSTED_METAL 7

#define LEVEL_MAX_GRID 6
#define MM_CELL 6
#define MM_PAD 3
#define MM_MARGIN 14
#define PLAYER_RADIUS 0.22f
#define WALK_SPEED 2.0f
#define RUN_SPEED 4.0f
#define PLAYER_EYE_GROUND_Y 0.52f
#define JUMP_VELOCITY 3.0f  /* ~half prior apex; keeps eye below ~1.0 wall/arch height */
#define GRAVITY 12.5f
#define MOUSE_SENS 0.0022f
#define MOUSE_SENS_Y 0.002f
#define PITCH_LIM 1.18f
#define FOV_DEG 66.0f
#define PLAYER_SHOT_COUNT 16
#define PLAYER_SHOT_SPEED 12.0f

#define SHADER_SPRITE_PACK_MAX 128
#define SHADER_SPRITE_GPU_MAX 32
#define SHADER_SPRITE_SSBO_VEC4S (SHADER_SPRITE_GPU_MAX * 3)
#define SHADER_SPRITE_CULL_DIST 28.0f
#define SHADER_SPRITE_RESOLVER_AVAILABLE 1
/* Phase 3: portal, exit pillar, player shots, particles — CPU draw skipped when shader path on. */
#define SHADER_SPRITE_PHASE3_AVAILABLE 1
#define SHADER_SPRITE_DEMON        1
#define SHADER_SPRITE_HELLRAISER   2
#define SHADER_SPRITE_AMMO         3
#define SHADER_SPRITE_PARTICLE     4
#define SHADER_SPRITE_PLAYER_SHOT  5
#define SHADER_SPRITE_PORTAL       6
#define SHADER_SPRITE_EXIT_PILLAR  7
#define SHADER_SPRITE_DRONE_SHOT   8  /* white plasma bolt */
#define SHADER_SPRITE_HUNTER_DRONE 9  /* blue-gray hull, cyan glow, orange eye */

typedef struct {
    int width;
    int height;
    int grid_x;
    int grid_z;
    int room_min_w;
    int room_max_w;
    int room_min_h;
    int room_max_h;
} LevelConfig;

typedef enum {
    LEVEL_KIND_HUNT,
    LEVEL_KIND_ARENA,
    LEVEL_KIND_LONG_WALK
} LevelKind;

typedef struct {
    LevelKind kind;
    LevelConfig config;
    int teleporter_count;
} LevelDef;

static const LevelDef g_level_defs[] = {
    {LEVEL_KIND_HUNT, {16, MAP_DEFAULT_H, 2, 3, 3, 6, 3, 6}, 2},
    {LEVEL_KIND_HUNT, {MAP_DEFAULT_W, MAP_DEFAULT_H, 3, 3, 3, 6, 3, 6}, 4},
};
#define LEVEL_CURATED_COUNT ((int)(sizeof(g_level_defs) / sizeof(g_level_defs[0])))
#define LEVEL_COUNT 100

static LevelConfig g_level_config = {
    MAP_DEFAULT_W, MAP_DEFAULT_H,
    3, 3,
    3, 6,
    3, 6
};
static int g_map_w = MAP_DEFAULT_W;
static int g_map_h = MAP_DEFAULT_H;
static int g_current_level = 1;
static int g_teleporter_count = 4;

static LevelDef current_level_def(void);

#define CELL_EMPTY 0
#define CELL_WALL 1
#define CELL_EXIT 2
#define CELL_ARCH_NS 3
#define CELL_ARCH_EW 4

/* Empty/passable cells plus shader-rendered world geometry. */
static uint8_t g_map[MAP_MAX_H][MAP_MAX_W];
static uint8_t g_material_map[MAP_MAX_H][MAP_MAX_W]; /* 4-bit material ID per wall cell (Phase 2) */
static int g_exit_mx = -1;
static int g_exit_mz = -1;

typedef struct {
    float x, z;
    float ang;
    int hp;
    int alive;
    float teleport_cooldown;
    float hurt_flash;
} Demon;

#define DEMON_COUNT 8
static Demon g_demon[DEMON_COUNT];

#define HUNTER_DRONE_COUNT 6
#define HUNTER_DRONE_HOVER_Y 0.86f
#define HUNTER_DRONE_SPEED 1.65f
#define HUNTER_DRONE_SHOT_RADIUS 12.0f
#define HUNTER_DRONE_FIRE_COOLDOWN 1.0f
#define HUNTER_DRONE_FLY_HEAR_DIST 8.0f
#define DRONE_SHOT_COUNT 12
#define DRONE_SHOT_SPEED 8.0f
#define DRONE_SHOT_LIGHT_RADIUS 3.0f
#define SCORE_HUNTER_DRONE 150

typedef struct {
    float x, z;
    float ang;
    int hp;
    int active;
    float fire_cooldown;
    float hurt_flash;
    float fly_pulse;
} HunterDrone;
static HunterDrone g_hunter_drones[HUNTER_DRONE_COUNT];

typedef enum { PHASE_TITLE, PHASE_ENTERING, PHASE_PLAY, PHASE_PAUSE, PHASE_WIN, PHASE_DEATH } GamePhase;
static GamePhase g_phase = PHASE_TITLE;
static int g_title_show_instructions;

#define ENTERING_MIN_SEC 5.0
#define ENTERING_PANEL_BANDS 12
#define ENTERING_PANEL_REBUILD_SEC 0.22
#define GAME_RENDER_W 960
#define GAME_RENDER_H 600

typedef enum {
    DEATH_NONE = 0,
    DEATH_DEMON_MELEE,
    DEATH_HELLRAISER_TOUCH,
    DEATH_HUNTER_DRONE
} DeathReason;
static DeathReason g_death_reason = DEATH_NONE;

static float g_px = 1.5f;
static float g_pz = 1.5f;
static float g_yaw = 0.0f;
static float g_pitch = 0.0f;
static float g_eye_y = PLAYER_EYE_GROUND_Y;
static float g_player_vx;
static float g_player_vz;
static float g_player_vy;
static int g_player_grounded = 1;
static int g_health = 40;
static int g_ammo = 24;
static int g_kills;
static int g_score;
static int g_high_score;
static int g_spawn_number = 1;
static int g_last_demon_gain;
static int g_last_portal_gain;
static int g_last_exit_gain;
static int g_last_bonus_gain;
static int g_last_total_gain;
static int g_muzzle;
static float g_bob;
static float g_pain_flash;
static float g_melee_cooldown;
static float g_player_teleport_cooldown;
static float g_last_spawn_time;
static float g_spawn_elapsed;
static SituationFont g_font;
static int g_game_display = -1;
static int g_show_fps;
static int g_vsync_on;
static int g_screenshot_pending;
static float g_screenshot_msg_timer;
static char g_screenshot_msg[128];
static int g_had_window_focus = 1;
static double g_ignore_focus_loss_until;
static float g_shader_sprites0[SHADER_SPRITE_GPU_MAX][4];
static float g_shader_sprites1[SHADER_SPRITE_GPU_MAX][4];
static float g_shader_sprites2[SHADER_SPRITE_GPU_MAX][4];
static int g_shader_sprite_count;

typedef struct {
    int type;
    float priority;
    float dist;
    float x;
    float y;
    float z;
    float half_w;
    float height;
    float base_y;
    float p0;
    float p1;
    float p2;
    float p3;
} ShaderSpriteCandidate;

static ShaderSpriteCandidate g_sprite_candidates[SHADER_SPRITE_PACK_MAX];
static int g_sprite_candidate_count;
static int g_shader_sprite_debug_mode;

typedef struct {
    float origin_x, origin_y, origin_z;
    float x, y, z;
    float dir_x, dir_y, dir_z;
    float travel;
    float max_travel;
    int active;
} PlayerShot;
static PlayerShot g_player_shots[PLAYER_SHOT_COUNT];

typedef struct {
    float origin_x, origin_y, origin_z;
    float x, y, z;
    float dir_x, dir_y, dir_z;
    float travel;
    float max_travel;
    int active;
    int woosh_played;
} DroneShot;
static DroneShot g_drone_shots[DRONE_SHOT_COUNT];

#define DRONE_EXPLOSION_COUNT 10
#define PARTICLE_BRIGHT_EXPLOSION 1.35f
typedef struct {
    float x, y, z;
    float life;
    float max_life;
} DroneExplosionFlash;
static DroneExplosionFlash g_drone_explosions[DRONE_EXPLOSION_COUNT];

#define SCORE_DEMON 100
#define SCORE_PORTAL 500
#define SCORE_EXIT 1000
#define SCORE_AMMO_PICKUP 25
#define SCORE_HEALTH_BONUS 25
#define SCORE_AMMO_BONUS 5
#define HIGH_SCORE_FILE "demon_hunt_highscore.dat"

#define HELLRAISER_SPAWN_TIME 90.0f
#define HELLRAISER_GAUNTLET_SPAWN_TIME 10.0f
#define HELLRAISER_SPAWN_INTERVAL 10.0f
#define HELLRAISER_WARN_LEAD 10.0f
#define HELLRAISER_FREEZE_TIME 1.5f
#define HELLRAISER_SPEED 3.35f
#define HELLRAISER_TOUCH_RADIUS 0.48f
#define HELLRAISER_OSC_ID 67
#define HELLRAISER_COUNT 8

typedef struct {
    float x, z;
    int active;
    int warned;
    float teleport_cooldown;
    float spawn_freeze;
    uint64_t sync_ix;
} Hellraiser;
static Hellraiser g_hellraisers[HELLRAISER_COUNT];
static float g_next_hellraiser_spawn_time;

/* ─── Dungeon SFX / music bus (node graph) ─────────────────────────────────
 * Legacy SituationPlayToneEx voices mix after SituationProcessGraph unless
 * SituationSetToneRouting sends them into the graph Sound Source (see play_sfx_tone).
 * Melody and wet spatial SFX use that bus; dry spatial cues stay on the tone pool.
 */
static SituationAudioGraph* g_sfx_graph = NULL;
static int g_sfx_bus_ok = 0;
static SituationNodeHandle g_sfx_source = SITUATION_INVALID_NODE_HANDLE;
static SituationNodeHandle g_sfx_phaser = SITUATION_INVALID_NODE_HANDLE;
static SituationNodeHandle g_sfx_echo = SITUATION_INVALID_NODE_HANDLE;
static SituationNodeHandle g_sfx_verb = SITUATION_INVALID_NODE_HANDLE;
static SituationNodeHandle g_sfx_mixer = SITUATION_INVALID_NODE_HANDLE;

static void audio_destroy_sfx_graph(void) {
    if (g_sfx_graph) {
        SituationSetActiveGraph(NULL);
        SituationDestroyGraph(g_sfx_graph);
        g_sfx_graph = NULL;
    }
    g_sfx_source = SITUATION_INVALID_NODE_HANDLE;
    g_sfx_phaser = SITUATION_INVALID_NODE_HANDLE;
    g_sfx_echo = SITUATION_INVALID_NODE_HANDLE;
    g_sfx_verb = SITUATION_INVALID_NODE_HANDLE;
    g_sfx_mixer = SITUATION_INVALID_NODE_HANDLE;
    g_sfx_bus_ok = 0;
}

static void play_sfx_tone(SituationWaveType wave, float freq, float vol, float pan, float attack, float decay, float sustain, float release, float hold) {
    SituationToneHandle h = SituationPlayToneEx(wave, freq, vol, pan, attack, decay, sustain, release, hold);
    if (h != 0 && g_sfx_bus_ok) {
        SituationSetToneRouting(h, true);
    }
}

static void audio_init(void) {
    g_sfx_bus_ok = 0;
    /* Silence Situation's default graph tone synth until our bus is ready (platformer_plumber pattern). */
    SituationSetActiveGraph(NULL);
    (void)SituationResumeAudioDevice();

    g_sfx_graph = SituationCreateGraph();
    if (!g_sfx_graph) {
        fprintf(stderr, "[demon_hunt] SFX graph create failed; procedural audio is dry (no dungeon FX bus)\n");
        return;
    }

    if (SituationCreateNode(g_sfx_graph, SITUATION_NODE_SOUND_SOURCE, &g_sfx_source) != SITUATION_SUCCESS ||
        SituationCreateNode(g_sfx_graph, SITUATION_NODE_PHASER, &g_sfx_phaser) != SITUATION_SUCCESS ||
        SituationCreateNode(g_sfx_graph, SITUATION_NODE_ECHO, &g_sfx_echo) != SITUATION_SUCCESS ||
        SituationCreateNode(g_sfx_graph, SITUATION_NODE_REVERB, &g_sfx_verb) != SITUATION_SUCCESS ||
        SituationCreateNode(g_sfx_graph, SITUATION_NODE_MIXER, &g_sfx_mixer) != SITUATION_SUCCESS) {
        fprintf(stderr, "[demon_hunt] SFX graph node create failed; procedural audio is dry\n");
        audio_destroy_sfx_graph();
        return;
    }

    if (SituationCreatePatch(g_sfx_graph, g_sfx_source, 0, g_sfx_mixer, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g_sfx_graph, g_sfx_source, 0, g_sfx_echo, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g_sfx_graph, g_sfx_echo, 0, g_sfx_verb, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g_sfx_graph, g_sfx_verb, 0, g_sfx_mixer, 1, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g_sfx_graph, g_sfx_source, 0, g_sfx_phaser, 0, false) != SITUATION_SUCCESS ||
        SituationCreatePatch(g_sfx_graph, g_sfx_phaser, 0, g_sfx_mixer, 2, false) != SITUATION_SUCCESS) {
        fprintf(stderr, "[demon_hunt] SFX graph patch failed; procedural audio is dry\n");
        audio_destroy_sfx_graph();
        return;
    }

    SituationSetControl(g_sfx_graph, g_sfx_echo, 0, 0.50f); // delay time
    SituationSetControl(g_sfx_graph, g_sfx_echo, 1, 0.50f); // feedback
    SituationSetControl(g_sfx_graph, g_sfx_echo, 2, 1.00f); // wet mix

    SituationSetControl(g_sfx_graph, g_sfx_verb, 0, 0.75f); // room size
    SituationSetControl(g_sfx_graph, g_sfx_verb, 1, 0.50f); // hf damping
    SituationSetControl(g_sfx_graph, g_sfx_verb, 2, 0.50f); // wet mix
    SituationSetControl(g_sfx_graph, g_sfx_verb, 3, 0.00f); // dry mix
    SituationSetControl(g_sfx_graph, g_sfx_verb, 4, 1.00f); // stereo width

    SituationSetControl(g_sfx_graph, g_sfx_phaser, 0, 1.2f); // lfo frequency
    SituationSetControl(g_sfx_graph, g_sfx_phaser, 1, 0.45f); // feedback
    SituationSetControl(g_sfx_graph, g_sfx_phaser, 2, 0.20f); // mix
    SituationSetControl(g_sfx_graph, g_sfx_phaser, 3, 0.15f); // pan depth
    SituationSetControl(g_sfx_graph, g_sfx_phaser, 4, 0.80f); // stereo width
    SituationSetControl(g_sfx_graph, g_sfx_phaser, 5, 2.00f); // feedback delay

    SituationSetControl(g_sfx_graph, g_sfx_mixer, 0, 1.00f); // master gain

    //SituationTopologicalSort(g_sfx_graph); // *** not needed, Situation takes care of this
    SituationSetActiveGraph(g_sfx_graph);
    SituationSetGraphSFXSource(g_sfx_source);
    g_sfx_bus_ok = 1;
}

static void sfx_play_spatial_range(float max_dist, SituationWaveType wave, float freq, float vol, float attack, float hold, float release, float tx, float tz, bool wet) {
    float dx = tx - g_px;
    float dz = tz - g_pz;
    float dist = sqrtf(dx * dx + dz * dz);
    float pan = 0.0f;
    float hear = max_dist > 0.01f ? max_dist : 12.0f;
    float att_vol = vol * fmaxf(0.0f, 1.0f - (dist / hear));

    if (dist > 0.01f) {
        float fx = sinf(g_yaw);
        float fz = cosf(g_yaw);
        float rx = cosf(g_yaw);
        float rz = -sinf(g_yaw);
        float right_dot = (dx / dist) * rx + (dz / dist) * rz;
        pan = right_dot;
    }

    if (att_vol > 0.01f) {
        if (wet) {
            play_sfx_tone(wave, freq, att_vol, pan, attack, 0.02f, 0.5f, release, hold);
        } else {
            SituationPlayToneEx(wave, freq, att_vol, pan, attack, 0.02f, 0.5f, release, hold);
        }
    }
}

static void sfx_play_spatial(SituationWaveType wave, float freq, float vol, float attack, float hold, float release, float tx, float tz, bool wet) {
    sfx_play_spatial_range(12.0f, wave, freq, vol, attack, hold, release, tx, tz, wet);
}

static void sfx_drone_fly(float tx, float tz, float blade_phase, int drone_ix) {
    float t = (float)SituationTimerGetTime();
    float rotor = 58.0f + sinf(t * 10.5f + blade_phase) * 9.0f;
    float whir = 420.0f + sinf(t * 16.0f + blade_phase * 1.7f + (float)drone_ix * 0.9f) * 80.0f;
    float thump = 92.0f + sinf(t * 5.2f + blade_phase * 0.5f) * 14.0f;
    /* Overlapping rotor pulses read as a distant helicopter within 8 blocks. */
    sfx_play_spatial_range(HUNTER_DRONE_FLY_HEAR_DIST, SIT_WAVE_NOISE, thump, 0.30f, 0.02f, 0.20f, 0.24f, tx, tz, false);
    sfx_play_spatial_range(HUNTER_DRONE_FLY_HEAR_DIST, SIT_WAVE_SAW, rotor, 0.24f, 0.02f, 0.18f, 0.22f, tx, tz, false);
    sfx_play_spatial_range(HUNTER_DRONE_FLY_HEAR_DIST, SIT_WAVE_NOISE, whir, 0.16f, 0.01f, 0.14f, 0.18f, tx, tz, false);
    sfx_play_spatial_range(HUNTER_DRONE_FLY_HEAR_DIST, SIT_WAVE_TRIANGLE, rotor * 0.5f, 0.12f, 0.02f, 0.16f, 0.20f, tx, tz, false);
}

static void sfx_jump(void) {
    play_sfx_tone(SIT_WAVE_NOISE, 210.0f, 0.24f, 0.0f, 0.002f, 0.04f, 0.0f, 0.10f, 0.02f);
    play_sfx_tone(SIT_WAVE_TRIANGLE, 130.0f, 0.20f, 0.0f, 0.003f, 0.05f, 0.0f, 0.12f, 0.03f);
}

static void sfx_shoot(void) {
    /* Low frequency punch + high frequency crack */
    play_sfx_tone(SIT_WAVE_SAW, 110.0f, 0.35f, 0.0f, 0.001f, 0.08f, 0.0f, 0.12f, 0.02f);
    play_sfx_tone(SIT_WAVE_NOISE, 800.0f, 0.28f, 0.0f, 0.001f, 0.06f, 0.0f, 0.18f, 0.03f);
}

static void sfx_hit(float tx, float tz) {
    /* Low monster growl/hit sound with spatial audio */
    sfx_play_spatial(SIT_WAVE_SAW, 45.0f, 0.45f, 0.01f, 0.06f, 0.18f, tx, tz, false);
    sfx_play_spatial(SIT_WAVE_NOISE, 120.0f, 0.15f, 0.001f, 0.04f, 0.15f, tx, tz, false);
}

static void sfx_hurt(void) {
    /* Classic demon-touch hurt: dissonant dual saw (centered, not spatialized). */
    play_sfx_tone(SIT_WAVE_SAW, 140.0f, 0.35f, 0.0f, 0.005f, 0.10f, 0.0f, 0.15f, 0.05f);
    play_sfx_tone(SIT_WAVE_SAW, 148.0f, 0.35f, 0.0f, 0.005f, 0.10f, 0.0f, 0.15f, 0.05f);
}

static void player_hurt_from_enemy(int damage, DeathReason death_reason) {
    g_health -= damage;
    g_pain_flash = 1.0f;
    if (g_health <= 0 && g_death_reason == DEATH_NONE) {
        g_death_reason = death_reason;
    }
    if (g_melee_cooldown <= 0.0f) {
        sfx_hurt();
        g_melee_cooldown = 0.85f;
    }
}

static void sfx_drone_fire(float tx, float tz) {
    /* Noise/saw through the phaser branch; avoid a dry square beep dominating the cue. */
    sfx_play_spatial(SIT_WAVE_NOISE, 240.0f, 0.58f, 0.004f, 0.07f, 0.30f, tx, tz, true);
    sfx_play_spatial(SIT_WAVE_SAW, 165.0f, 0.42f, 0.005f, 0.06f, 0.26f, tx, tz, true);
}

static void sfx_drone_woosh(float tx, float tz) {
    sfx_play_spatial(SIT_WAVE_NOISE, 380.0f, 0.40f, 0.001f, 0.04f, 0.14f, tx, tz, true);
    sfx_play_spatial(SIT_WAVE_SAW, 260.0f, 0.28f, 0.002f, 0.03f, 0.12f, tx, tz, true);
}

static void sfx_drone_destroyed(float tx, float tz) {
    sfx_play_spatial(SIT_WAVE_NOISE, 180.0f, 0.72f, 0.01f, 0.08f, 0.24f, tx, tz, true);
    sfx_play_spatial(SIT_WAVE_SQUARE, 310.0f, 0.42f, 0.004f, 0.06f, 0.22f, tx, tz, true);
}

/* --- Two-voice grid melodies (oscillator clock; triangle + square) --- */
#define MELO_OSC_ID 44
#define MELO_REST 255
#define MELO_ROOT_MIDI 38 /* Db2 — grid cells add Phrygian-ish semitones below */
/* Eight pitch classes: root, 2, b3, 4, 5, b6, b7, octave (Aeolian/Dorian flavor) */
static const uint8_t g_melo_grid_semi[8] = {0, 2, 3, 5, 7, 8, 10, 12};

typedef enum {
    MELOK_NONE = 0,
    MELOK_START,
    MELOK_KILL,
    MELOK_DEATH,
    MELOK_WIN,
    MELOK_ABORT
} MeloKind;

#define MELO_QMAX 6
static uint8_t g_melo_q[MELO_QMAX];
static int g_melo_qn;

static int g_melo_run;
static int g_melo_step;
static int g_melo_len;
static float g_melo_bpm;
static const uint8_t* g_melo_va;
static const uint8_t* g_melo_vb;
static const uint8_t* g_melo_vc;
static const uint8_t* g_melo_vd;
static uint64_t g_melo_sync_ix;

static int melo_cell_to_midi(uint8_t cell) {
    if (cell == MELO_REST || cell >= 8) {
        return -1;
    }
    return (int)MELO_ROOT_MIDI + (int)g_melo_grid_semi[cell];
}

static void melo_note(int midi, SituationWaveType wave, float vol, float pan, float hold_sec) {
    if (midi < 0 || midi >= 128) {
        return;
    }
    /* Same ADSR as before; route through dungeon bus when graph init succeeded. */
    play_sfx_tone(
        wave,
        SITUATION_MIDI_NOTE_FREQUENCY[midi],
        vol,
        pan,
        0.006f,
        0.04f,
        0.42f,
        0.07f,
        hold_sec);
}

static void melo_emit_step(void) {
    if (!g_melo_run) {
        return;
    }
    if (g_melo_step >= g_melo_len) {
        g_melo_run = 0;
        return;
    }

    float step_sec = 60.0f / g_melo_bpm;
    float hold = step_sec * 0.86f;

    int ma = melo_cell_to_midi(g_melo_va[g_melo_step]);
    if (ma >= 0) {
        melo_note(ma, SIT_WAVE_TRIANGLE, 0.18f, -0.09f, hold);
    }
    int mb = melo_cell_to_midi(g_melo_vb[g_melo_step]);
    if (mb >= 0) {
        melo_note(mb, SIT_WAVE_SQUARE, 0.15f, 0.11f, hold * 0.92f);
    }
    int mc = melo_cell_to_midi(g_melo_vc[g_melo_step]);
    if (mc >= 0) {
        /* Bassline 1 octave down */
        melo_note(mc - 12, SIT_WAVE_SAW, 0.28f, 0.0f, hold * 1.5f);
    }
    uint8_t md = g_melo_vd[g_melo_step];
    if (md == 0) {
        /* Kick drum */
        play_sfx_tone(SIT_WAVE_TRIANGLE, 60.0f, 0.6f, 0.0f, 0.001f, 0.05f, 0.0f, 0.08f, 0.02f);
        play_sfx_tone(SIT_WAVE_SQUARE, 40.0f, 0.45f, 0.0f, 0.001f, 0.04f, 0.0f, 0.08f, 0.02f);
    } else if (md == 1) {
        /* Snare drum */
        play_sfx_tone(SIT_WAVE_NOISE, 200.0f, 0.45f, 0.0f, 0.001f, 0.05f, 0.0f, 0.1f, 0.02f);
        play_sfx_tone(SIT_WAVE_SAW, 150.0f, 0.35f, 0.0f, 0.001f, 0.04f, 0.0f, 0.08f, 0.02f);
    } else if (md == 2) {
        /* Hi-hat */
        play_sfx_tone(SIT_WAVE_NOISE, 800.0f, 0.18f, 0.0f, 0.001f, 0.01f, 0.0f, 0.02f, 0.01f);
    }

    g_melo_step++;
    if (g_melo_step >= g_melo_len) {
        g_melo_run = 0;
    }
}

/* Round start — creeping Phrygian, 14 eighths */
static const uint8_t g_ml_start_a[] = {5, MELO_REST, 4, 3, 2, 3, 5, 7, 6, 5, 3, 1, 0, 2};
static const uint8_t g_ml_start_b[] = {0, MELO_REST, 0, 2, 0, 1, 0, 1, 0, 1, 2, 0, 0, 1};
static const uint8_t g_ml_start_c[] = {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0};
static const uint8_t g_ml_start_d[] = {0, 2, 1, 2, 0, 2, 1, 2, 0, 2, 1, 2, 0, 1};
#define ML_START_LEN 14
#define ML_START_BPM 105.0f

/* Enemy killed — short stab */
static const uint8_t g_ml_kill_a[] = {7, 5, 3, 1, MELO_REST, MELO_REST};
static const uint8_t g_ml_kill_b[] = {5, 4, 2, 0, MELO_REST, MELO_REST};
static const uint8_t g_ml_kill_c[] = {1, 0, 1, 0, MELO_REST, MELO_REST};
static const uint8_t g_ml_kill_d[] = {0, 1, 0, 1, MELO_REST, MELO_REST};
#define ML_KILL_LEN 6
#define ML_KILL_BPM 130.0f

/* Player death / game over — slow descent */
static const uint8_t g_ml_death_a[] = {5, 4, 3, 2, 1, 0, 1, 2, 3, 2, 1, 0, MELO_REST, 1, 0, MELO_REST, 0, 0};
static const uint8_t g_ml_death_b[] = {0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 1, 2, MELO_REST, 0, MELO_REST, MELO_REST, 1, 0};
static const uint8_t g_ml_death_c[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MELO_REST, 0, 0, MELO_REST, 0, 0};
static const uint8_t g_ml_death_d[] = {0, MELO_REST, 0, MELO_REST, 0, MELO_REST, 0, MELO_REST, 0, MELO_REST, 0, MELO_REST, MELO_REST, 1, 0, MELO_REST, 1, 0};
#define ML_DEATH_LEN 18
#define ML_DEATH_BPM 85.0f

/* Win — victory fanfare (ascending i–IV–v–i8, octave hold, root cadence) */
static const uint8_t g_ml_win_a[] = {0, 3, 5, 7, 7, 7, 6, 5, 4, 5, 6, 7, 7, 5, 3, 0};
static const uint8_t g_ml_win_b[] = {0, 1, 3, 5, 5, 5, 4, 3, 2, 3, 4, 5, 5, 3, 2, 0};
static const uint8_t g_ml_win_c[] = {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0};
static const uint8_t g_ml_win_d[] = {0, 2, 0, 2, 0, 2, 1, 2, 0, 2, 0, 2, 0, 1, 0, 0};
#define ML_WIN_LEN 16
#define ML_WIN_BPM 152.0f

/* Esc to title — curt */
static const uint8_t g_ml_abort_a[] = {3, 2, 1, 0, MELO_REST, 2, 1, 0};
static const uint8_t g_ml_abort_b[] = {0, 1, 0, 1, MELO_REST, 0, 0, 0};
static const uint8_t g_ml_abort_c[] = {0, 0, 0, 0, MELO_REST, 0, 0, 0};
static const uint8_t g_ml_abort_d[] = {0, 1, 0, 1, MELO_REST, 1, 0, 0};
#define ML_ABORT_LEN 8
#define ML_ABORT_BPM 130.0f

static void melo_start_kind(MeloKind k) {
    const uint8_t* va = NULL;
    const uint8_t* vb = NULL;
    const uint8_t* vc = NULL;
    const uint8_t* vd = NULL;
    int len = 0;
    float bpm = ML_START_BPM; /* only used if switch falls through; each case sets its own BPM */

    switch (k) {
        case MELOK_START:
            va = g_ml_start_a;
            vb = g_ml_start_b;
            vc = g_ml_start_c;
            vd = g_ml_start_d;
            len = ML_START_LEN;
            bpm = ML_START_BPM;
            break;
        case MELOK_KILL:
            va = g_ml_kill_a;
            vb = g_ml_kill_b;
            vc = g_ml_kill_c;
            vd = g_ml_kill_d;
            len = ML_KILL_LEN;
            bpm = ML_KILL_BPM;
            break;
        case MELOK_DEATH:
            va = g_ml_death_a;
            vb = g_ml_death_b;
            vc = g_ml_death_c;
            vd = g_ml_death_d;
            len = ML_DEATH_LEN;
            bpm = ML_DEATH_BPM;
            break;
        case MELOK_WIN:
            va = g_ml_win_a;
            vb = g_ml_win_b;
            vc = g_ml_win_c;
            vd = g_ml_win_d;
            len = ML_WIN_LEN;
            bpm = ML_WIN_BPM;
            break;
        case MELOK_ABORT:
            va = g_ml_abort_a;
            vb = g_ml_abort_b;
            vc = g_ml_abort_c;
            vd = g_ml_abort_d;
            len = ML_ABORT_LEN;
            bpm = ML_ABORT_BPM;
            break;
        default:
            return;
    }

    g_melo_va = va;
    g_melo_vb = vb;
    g_melo_vc = vc;
    g_melo_vd = vd;
    g_melo_len = len;
    g_melo_bpm = bpm;
    g_melo_step = 0;
    g_melo_run = 1;
    SituationSetTimerOscillatorPeriod(MELO_OSC_ID, 60.0 / (double)bpm);
    g_melo_sync_ix = SituationTimerGetOscillatorTriggerCount(MELO_OSC_ID);
    melo_emit_step();
}

static void melo_update_tick(void) {
    for (;;) {
        uint64_t tr = SituationTimerGetOscillatorTriggerCount(MELO_OSC_ID);
        while (g_melo_run && g_melo_sync_ix < tr) {
            melo_emit_step();
            g_melo_sync_ix++;
        }
        if (!g_melo_run && g_melo_qn > 0) {
            MeloKind nk = (MeloKind)g_melo_q[0];
            memmove(g_melo_q, g_melo_q + 1, (size_t)(g_melo_qn - 1));
            g_melo_qn--;
            melo_start_kind(nk);
            continue;
        }
        break;
    }
}

static void melo_resync(void) {
    g_melo_sync_ix = SituationTimerGetOscillatorTriggerCount(MELO_OSC_ID);
}

static void audio_stop_ongoing(void) {
    g_melo_run = 0;
    g_melo_qn = 0;
    SituationStopAllTones();
}

static void melo_request(MeloKind k) {
    if (k == MELOK_NONE) {
        return;
    }
    if (k == MELOK_WIN) {
        audio_stop_ongoing();
        melo_start_kind(k);
        return;
    }
    if (!g_melo_run) {
        melo_start_kind(k);
        return;
    }
    if (g_melo_qn < MELO_QMAX) {
        g_melo_q[g_melo_qn++] = (uint8_t)k;
    }
}

static int map_solid(int mx, int mz) {
    if (mx < 0 || mz < 0 || mx >= g_map_w || mz >= g_map_h) {
        return 1;
    }
    return g_map[mz][mx] == CELL_WALL;
}

static int circle_hits_wall_r(float cx, float cz, float r) {
    const float samples[4][2] = {{-r, -r}, {r, -r}, {-r, r}, {r, r}};
    for (int i = 0; i < 4; i++) {
        int mx = (int)floorf(cx + samples[i][0]);
        int mz = (int)floorf(cz + samples[i][1]);
        if (map_solid(mx, mz)) {
            return 1;
        }
    }
    return 0;
}

static int circle_hits_wall(float cx, float cz) {
    return circle_hits_wall_r(cx, cz, PLAYER_RADIUS);
}

#define DEMON_RADIUS 0.16f
#define DEMON_WANDER 2.4f

static void demons_reset(void) {
    for (int i = 0; i < DEMON_COUNT; i++) {
        g_demon[i].alive = 0;
        g_demon[i].teleport_cooldown = 0.0f;
        g_demon[i].hurt_flash = 0.0f;
    }
}

static void hunter_drones_clear(void) {
    for (int i = 0; i < HUNTER_DRONE_COUNT; i++) {
        g_hunter_drones[i].active = 0;
        g_hunter_drones[i].hp = 0;
        g_hunter_drones[i].fire_cooldown = 0.0f;
        g_hunter_drones[i].hurt_flash = 0.0f;
        g_hunter_drones[i].fly_pulse = 0.0f;
    }
}

#define PORTAL_COUNT 4
typedef struct {
    float x, z;
    int hp;
    int alive;
} Portal;
static Portal g_portals[PORTAL_COUNT];

static void get_random_empty_cell(float* out_x, float* out_z) {
    while (1) {
        int x = 1 + rand() % (g_map_w - 2);
        int z = 1 + rand() % (g_map_h - 2);
            if (g_map[z][x] == CELL_EMPTY) {
            *out_x = (float)x + 0.5f;
            *out_z = (float)z + 0.5f;
            return;
        }
    }
}

static void set_exit_cell(int mx, int mz) {
    if (mx < 0 || mz < 0 || mx >= g_map_w || mz >= g_map_h) {
        g_exit_mx = -1;
        g_exit_mz = -1;
        return;
    }
    g_map[mz][mx] = CELL_EXIT;
    g_exit_mx = mx;
    g_exit_mz = mz;
}

static int get_exit_position(float* out_x, float* out_z) {
    if (g_exit_mx >= 0 && g_exit_mz >= 0 &&
        g_exit_mx < g_map_w && g_exit_mz < g_map_h &&
        g_map[g_exit_mz][g_exit_mx] == CELL_EXIT) {
        *out_x = (float)g_exit_mx + 0.5f;
        *out_z = (float)g_exit_mz + 0.5f;
        return 1;
    }

    for (int z = 0; z < g_map_h; z++) {
        for (int x = 0; x < g_map_w; x++) {
            if (g_map[z][x] == CELL_EXIT) {
                g_exit_mx = x;
                g_exit_mz = z;
                *out_x = (float)x + 0.5f;
                *out_z = (float)z + 0.5f;
                return 1;
            }
        }
    }
    return 0;
}

static int cell_empty_for_arch(int x, int z) {
    return x > 0 && z > 0 && x < g_map_w - 1 && z < g_map_h - 1 && g_map[z][x] == CELL_EMPTY;
}

static void map_place_arches(void) {
    for (int z = 1; z < g_map_h - 1; z++) {
        for (int x = 1; x < g_map_w - 1; x++) {
            if (g_map[z][x] != CELL_EMPTY || (rand() % 100) >= 16) {
                continue;
            }
            int ns_passage = cell_empty_for_arch(x, z - 1) && cell_empty_for_arch(x, z + 1) &&
                             map_solid(x - 1, z) && map_solid(x + 1, z);
            int ew_passage = cell_empty_for_arch(x - 1, z) && cell_empty_for_arch(x + 1, z) &&
                             map_solid(x, z - 1) && map_solid(x, z + 1);
            if (ns_passage && ew_passage) {
                g_map[z][x] = (rand() & 1) ? CELL_ARCH_NS : CELL_ARCH_EW;
            } else if (ns_passage) {
                g_map[z][x] = CELL_ARCH_NS;
            } else if (ew_passage) {
                g_map[z][x] = CELL_ARCH_EW;
            }
        }
    }
}

static void portals_reset(void) {
    for (int i = 0; i < PORTAL_COUNT; i++) {
        get_random_empty_cell(&g_portals[i].x, &g_portals[i].z);
        g_portals[i].hp = 10;
        g_portals[i].alive = 1;
    }
}

#define TELEPORTER_MAX_COUNT 6
typedef struct {
    float x, z;
    int active;
} Teleporter;
static Teleporter g_teleporters[TELEPORTER_MAX_COUNT];

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void level_apply_config(const LevelConfig* cfg) {
    LevelConfig c = cfg ? *cfg : g_level_config;
    c.width = clamp_int(c.width, 8, MAP_MAX_W);
    c.height = clamp_int(c.height, 8, MAP_MAX_H);
    int max_grid_x = (c.width - 2) / 3;
    int max_grid_z = (c.height - 2) / 3;
    if (max_grid_x < 1) max_grid_x = 1;
    if (max_grid_z < 1) max_grid_z = 1;
    if (max_grid_x > LEVEL_MAX_GRID) max_grid_x = LEVEL_MAX_GRID;
    if (max_grid_z > LEVEL_MAX_GRID) max_grid_z = LEVEL_MAX_GRID;
    c.grid_x = clamp_int(c.grid_x, 1, max_grid_x);
    c.grid_z = clamp_int(c.grid_z, 1, max_grid_z);
    c.room_min_w = clamp_int(c.room_min_w, 2, c.width - 2);
    c.room_max_w = clamp_int(c.room_max_w, c.room_min_w, c.width - 2);
    c.room_min_h = clamp_int(c.room_min_h, 2, c.height - 2);
    c.room_max_h = clamp_int(c.room_max_h, c.room_min_h, c.height - 2);

    g_level_config = c;
    g_map_w = c.width;
    g_map_h = c.height;
}

static void teleporters_reset(void) {
    LevelDef def = current_level_def();
    g_teleporter_count = clamp_int(def.teleporter_count, 0, TELEPORTER_MAX_COUNT);
    for (int i = 0; i < TELEPORTER_MAX_COUNT; i++) {
        if (i < g_teleporter_count) {
            get_random_empty_cell(&g_teleporters[i].x, &g_teleporters[i].z);
            g_teleporters[i].active = 1;
        } else {
            g_teleporters[i].x = 0.0f;
            g_teleporters[i].z = 0.0f;
            g_teleporters[i].active = 0;
        }
    }
}

static void gauntlet_place_teleporters(void) {
    const int pads[TELEPORTER_MAX_COUNT][2] = {
        {6, 6}, {25, 25},
        {6, 25}, {25, 6},
        {16, 8}, {16, 24},
    };
    for (int i = 0; i < TELEPORTER_MAX_COUNT; i++) {
        if (i >= g_teleporter_count) {
            g_teleporters[i].active = 0;
            continue;
        }
        int x = clamp_int(pads[i][0], 1, g_map_w - 2);
        int z = clamp_int(pads[i][1], 1, g_map_h - 2);
        if (g_map[z][x] != CELL_EMPTY) {
            g_map[z][x] = CELL_EMPTY;
        }
        g_teleporters[i].x = (float)x + 0.5f;
        g_teleporters[i].z = (float)z + 0.5f;
        g_teleporters[i].active = 1;
    }
}

typedef struct {
    float x, z;
    int active;
    float respawn_timer;
} AmmoBox;

#define AMMO_HUNT_COUNT 5
#define AMMO_COUNT 32
static AmmoBox g_ammo_box[AMMO_COUNT];

#define MAX_PARTICLES 256
typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life;
    float bright;
    float r, g, b;
} Particle;
static Particle g_particles[MAX_PARTICLES];

/* bright >= PARTICLE_BRIGHT_EXPLOSION selects the large burst billboard path (drone blasts). */
static int particle_is_demon_drip(const Particle* p) {
    return p->g > 0.55f && p->r < 0.45f && p->b < 0.55f;
}

static int particle_is_explosion(const Particle* p) {
    if (particle_is_demon_drip(p)) {
        return 0;
    }
    return p->bright >= PARTICLE_BRIGHT_EXPLOSION;
}

static int particle_is_bolt_trail(const Particle* p) {
    if (particle_is_explosion(p)) {
        return 0;
    }
    float bright = p->bright > 0.0f ? p->bright : 1.0f;
    return bright >= 0.70f && p->r >= 0.90f && p->g >= 0.90f && p->b >= 0.90f;
}

/* Magenta motes vented from live portals — smaller than generic debris. */
static int particle_is_portal_energy(const Particle* p) {
    if (particle_is_bolt_trail(p) || particle_is_explosion(p) || particle_is_demon_drip(p)) {
        return 0;
    }
    return p->r > 0.75f && p->g > 0.25f && p->g < 0.75f && p->b > 0.82f;
}

static void particle_evict_bolt_trails(int want) {
    for (int n = 0; n < want; n++) {
        int best = -1;
        float best_life = -1.0f;
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!particle_is_bolt_trail(&g_particles[i])) {
                continue;
            }
            if (g_particles[i].life > best_life) {
                best_life = g_particles[i].life;
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        g_particles[best].life = 0.0f;
    }
}

static Particle* particle_alloc(void) {
    for (int p = 0; p < MAX_PARTICLES; p++) {
        if (g_particles[p].life <= 0.0f) {
            return &g_particles[p];
        }
    }
    return NULL;
}

/* Prefer a free slot; for short-lived bolt trails, recycle the dying particle. */
static Particle* particle_alloc_trail(void) {
    Particle* p = particle_alloc();
    if (p) {
        return p;
    }
    int best = -1;
    float best_life = 1e9f;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_particles[i].life <= 0.0f) {
            return &g_particles[i];
        }
        if (g_particles[i].life < best_life) {
            best_life = g_particles[i].life;
            best = i;
        }
    }
    return best >= 0 ? &g_particles[best] : NULL;
}

/* Explosions must always get slots — recycle dying bolt trails first. */
static Particle* particle_alloc_burst(void) {
    Particle* p = particle_alloc();
    if (p) {
        return p;
    }
    int best = -1;
    float best_score = 1e9f;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* q = &g_particles[i];
        if (q->life <= 0.0f) {
            return q;
        }
        float score = q->life;
        if (particle_is_bolt_trail(q)) {
            score -= 0.35f;
        }
        if (score < best_score) {
            best_score = score;
            best = i;
        }
    }
    return best >= 0 ? &g_particles[best] : NULL;
}

static void particle_spawn(float x, float y, float z, float vx, float vy, float vz, float life, float bright, float r, float g, float b) {
    Particle* p = particle_alloc_burst();
    if (!p) {
        return;
    }
    p->x = x;
    p->y = y;
    p->z = z;
    p->vx = vx;
    p->vy = vy;
    p->vz = vz;
    p->life = life;
    p->bright = bright;
    p->r = r;
    p->g = g;
    p->b = b;
}

static void drone_shots_clear(void) {
    for (int i = 0; i < DRONE_SHOT_COUNT; i++) {
        g_drone_shots[i].active = 0;
        g_drone_shots[i].woosh_played = 0;
    }
    for (int i = 0; i < DRONE_EXPLOSION_COUNT; i++) {
        g_drone_explosions[i].life = 0.0f;
    }
}

static DroneExplosionFlash* drone_explosion_alloc(void) {
    int best = -1;
    float best_life = 1e9f;
    for (int i = 0; i < DRONE_EXPLOSION_COUNT; i++) {
        if (g_drone_explosions[i].life <= 0.0f) {
            return &g_drone_explosions[i];
        }
        if (g_drone_explosions[i].life < best_life) {
            best_life = g_drone_explosions[i].life;
            best = i;
        }
    }
    return best >= 0 ? &g_drone_explosions[best] : NULL;
}

static void drone_shot_explode(float x, float y, float z) {
    DroneExplosionFlash* flash = drone_explosion_alloc();
    if (flash) {
        flash->x = x;
        flash->y = y;
        flash->z = z;
        flash->max_life = 0.45f;
        flash->life = flash->max_life;
    }

    particle_evict_bolt_trails(56);
    for (int p = 0; p < 28; p++) {
        float spread = 0.55f + ((float)(rand() % 100) / 100.0f) * 0.75f;
        particle_spawn(
            x + ((float)(rand() % 100) / 100.0f - 0.5f) * spread,
            y + ((float)(rand() % 100) / 100.0f - 0.5f) * spread * 0.70f,
            z + ((float)(rand() % 100) / 100.0f - 0.5f) * spread,
            ((float)(rand() % 100) / 100.0f - 0.5f) * 4.2f,
            0.9f + ((float)(rand() % 100) / 100.0f) * 3.6f,
            ((float)(rand() % 100) / 100.0f - 0.5f) * 4.2f,
            0.55f + ((float)(rand() % 100) / 100.0f) * 0.85f,
            PARTICLE_BRIGHT_EXPLOSION,
            1.0f,
            0.42f + ((float)(rand() % 100) / 100.0f) * 0.28f,
            0.06f + ((float)(rand() % 100) / 100.0f) * 0.14f);
    }
    sfx_play_spatial(SIT_WAVE_NOISE, 140.0f, 0.58f, 0.004f, 0.06f, 0.30f, x, z, true);
    sfx_play_spatial(SIT_WAVE_SQUARE, 90.0f, 0.40f, 0.004f, 0.05f, 0.26f, x, z, true);
}

static void particle_spawn_trail(float x, float y, float z, float vx, float vy, float vz, float life, float bright, float r, float g, float b) {
    Particle* p = particle_alloc_trail();
    if (!p) {
        return;
    }
    p->x = x;
    p->y = y;
    p->z = z;
    p->vx = vx;
    p->vy = vy;
    p->vz = vz;
    p->life = life;
    p->bright = bright;
    p->r = r;
    p->g = g;
    p->b = b;
}

static void drone_shot_emit_trail_at(const DroneShot* shot, float px, float py, float pz) {
    float spread = 0.14f;
    float back = 0.12f + (float)(rand() % 100) / 100.0f * 0.16f;
    float drift = 0.25f + (float)(rand() % 100) / 100.0f * 0.35f;
    float life = 0.30f + ((float)(rand() % 100) / 100.0f) * 0.22f;
    particle_spawn_trail(
        px - shot->dir_x * back + ((float)(rand() % 100) / 100.0f - 0.5f) * spread,
        py - shot->dir_y * back + ((float)(rand() % 100) / 100.0f - 0.5f) * spread * 0.55f,
        pz - shot->dir_z * back + ((float)(rand() % 100) / 100.0f - 0.5f) * spread,
        -shot->dir_x * drift + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.12f,
        -shot->dir_y * drift + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.08f,
        -shot->dir_z * drift + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.12f,
        life,
        0.95f,
        1.0f, 0.98f, 0.94f);
    particle_spawn_trail(
        px - shot->dir_x * (back + 0.10f),
        py - shot->dir_y * (back + 0.10f),
        pz - shot->dir_z * (back + 0.10f),
        -shot->dir_x * (drift * 0.65f),
        -shot->dir_y * (drift * 0.65f),
        -shot->dir_z * (drift * 0.65f),
        life * 0.82f,
        0.72f,
        1.0f, 1.0f, 1.0f);
    if ((rand() % 3) != 0) {
        particle_spawn_trail(
            px - shot->dir_x * (back + 0.22f) + ((float)(rand() % 100) / 100.0f - 0.5f) * spread * 0.6f,
            py - shot->dir_y * (back + 0.22f),
            pz - shot->dir_z * (back + 0.22f) + ((float)(rand() % 100) / 100.0f - 0.5f) * spread * 0.6f,
            -shot->dir_x * 0.18f,
            -shot->dir_y * 0.18f,
            -shot->dir_z * 0.18f,
            life * 0.55f,
            0.55f,
            0.92f, 0.96f, 1.0f);
    }
}

static void drone_shot_spawn(float ox, float oy, float oz, float tx, float ty, float tz) {
    float dx = tx - ox;
    float dy = ty - oy;
    float dz = tz - oz;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    if (len < 0.05f) {
        return;
    }
    dx /= len;
    dy /= len;
    dz /= len;

    for (int i = 0; i < DRONE_SHOT_COUNT; i++) {
        DroneShot* shot = &g_drone_shots[i];
        if (shot->active) {
            continue;
        }
        shot->dir_x = dx;
        shot->dir_y = dy;
        shot->dir_z = dz;
        shot->origin_x = ox;
        shot->origin_y = oy;
        shot->origin_z = oz;
        shot->travel = 0.0f;
        shot->max_travel = len + 0.75f;
        shot->x = ox;
        shot->y = oy;
        shot->z = oz;
        shot->active = 1;
        shot->woosh_played = 0;
        for (int t = 0; t < 4; t++) {
            drone_shot_emit_trail_at(shot, ox, oy, oz);
        }
        return;
    }
}

static void player_shot_spawn(float dir_x, float dir_y, float dir_z, float max_travel) {
    for (int i = 0; i < PLAYER_SHOT_COUNT; i++) {
        PlayerShot* shot = &g_player_shots[i];
        if (shot->active) {
            continue;
        }
        shot->dir_x = dir_x;
        shot->dir_y = dir_y;
        shot->dir_z = dir_z;
        shot->travel = 0.28f;
        shot->max_travel = fmaxf(0.45f, max_travel);
        shot->origin_x = g_px;
        shot->origin_y = g_eye_y;
        shot->origin_z = g_pz;
        shot->x = shot->origin_x + shot->dir_x * shot->travel;
        shot->y = shot->origin_y + shot->dir_y * shot->travel;
        shot->z = shot->origin_z + shot->dir_z * shot->travel;
        shot->active = 1;
        return;
    }
}

static void ammo_reset(void) {
    for (int i = 0; i < AMMO_COUNT; i++) {
        if (i < AMMO_HUNT_COUNT) {
            get_random_empty_cell(&g_ammo_box[i].x, &g_ammo_box[i].z);
            g_ammo_box[i].active = 1;
            g_ammo_box[i].respawn_timer = 0.0f;
        } else {
            g_ammo_box[i].x = 0.0f;
            g_ammo_box[i].z = 0.0f;
            g_ammo_box[i].active = 0;
            g_ammo_box[i].respawn_timer = -1.0f;
        }
    }
}

static LevelDef current_level_def(void) {
    int level = clamp_int(g_current_level, 1, LEVEL_COUNT);
    int idx = level - 1;
    if (idx < LEVEL_CURATED_COUNT) {
        return g_level_defs[idx];
    }

    int tier = (level - 1) / 9;
    int step = (level - 1) % 9;

    if ((level % 9) == 0) {
        LevelDef walk = {
            LEVEL_KIND_LONG_WALK,
            {MAP_MAX_W, clamp_int(17 + tier * 2, MAP_DEFAULT_H, MAP_MAX_H), 1, 1, 5, 8, 5, 8},
            clamp_int(4 + tier / 2, 4, TELEPORTER_MAX_COUNT)
        };
        return walk;
    }

    if ((level % 3) == 0) {
        int room_w = clamp_int(18 + tier * 2, 18, MAP_MAX_W - 2);
        int room_h = clamp_int(16 + tier * 2, 16, MAP_MAX_H - 2);
        LevelDef arena = {
            LEVEL_KIND_ARENA,
            {clamp_int(room_w + 2, 20, MAP_MAX_W), clamp_int(room_h + 2, 18, MAP_MAX_H), 1, 1, room_w, room_w, room_h, room_h},
            clamp_int(3 + tier / 2, 3, TELEPORTER_MAX_COUNT)
        };
        return arena;
    }

    int width = clamp_int(16 + tier * 2 + step / 2, 16, MAP_MAX_W);
    int height = clamp_int(18 + tier * 2 + step / 3, MAP_DEFAULT_H, MAP_MAX_H);
    int grid_x = clamp_int(2 + tier / 2 + step / 3, 2, 5);
    int grid_z = clamp_int(3 + tier / 2 + step / 4, 3, 5);
    int room_min = clamp_int(3 + tier / 4, 3, 6);
    int room_max = clamp_int(6 + tier / 3 + (step >= 6 ? 1 : 0), room_min, 10);
    LevelDef hunt = {
        LEVEL_KIND_HUNT,
        {width, height, grid_x, grid_z, room_min, room_max, room_min, room_max},
        clamp_int(2 + tier / 2 + step / 5, 2, TELEPORTER_MAX_COUNT)
    };
    return hunt;
}

static LevelKind current_level_kind(void) {
    return current_level_def().kind;
}

static int current_level_is_gauntlet(void) {
    LevelKind kind = current_level_kind();
    return kind == LEVEL_KIND_ARENA || kind == LEVEL_KIND_LONG_WALK;
}

static int current_level_is_arena(void) {
    return current_level_kind() == LEVEL_KIND_ARENA;
}

static int current_level_is_long_walk(void) {
    return current_level_kind() == LEVEL_KIND_LONG_WALK;
}

static int current_level_allows_drones(void) {
    return ((clamp_int(g_current_level, 1, LEVEL_COUNT) - 1) % 4) >= 2;
}

static int hunter_drone_target_count(void) {
    if (!current_level_allows_drones()) {
        return 0;
    }
    if (current_level_is_arena() || current_level_is_long_walk()) {
        return 2;
    }
    return clamp_int(1 + (g_current_level - 4) / 14, 1, HUNTER_DRONE_COUNT);
}

static void hunter_drone_place_orbit(HunterDrone* d, int slot, int slots, float cx, float cz) {
    float orbit_r = 5.5f;
    if (current_level_is_arena()) {
        orbit_r = fminf((float)g_map_w, (float)g_map_h) * 0.20f;
    } else if (current_level_is_long_walk()) {
        orbit_r = fminf((float)g_map_w * 0.35f, 6.5f);
    }
    if (orbit_r < 4.0f) {
        orbit_r = 4.0f;
    }
    if (orbit_r > 7.5f) {
        orbit_r = 7.5f;
    }
    float ang = ((float)slot / (float)slots) * 6.2831853f;
    d->x = cx + cosf(ang) * orbit_r;
    d->z = cz + sinf(ang) * orbit_r;
    d->ang = ang;
    if (circle_hits_wall_r(d->x, d->z, DEMON_RADIUS * 0.9f)) {
        for (int attempt = 0; attempt < 20; attempt++) {
            get_random_empty_cell(&d->x, &d->z);
            float dx = d->x - cx;
            float dz = d->z - cz;
            if (dx * dx + dz * dz > 3.5f * 3.5f) {
                break;
            }
        }
    }
}

static void hunter_drones_reset(void) {
    hunter_drones_clear();
    int target_count = hunter_drone_target_count();
    int orbit_pair = current_level_is_arena() || current_level_is_long_walk();
    for (int i = 0; i < target_count; i++) {
        HunterDrone* d = &g_hunter_drones[i];
        if (orbit_pair) {
            hunter_drone_place_orbit(d, i, target_count, g_px, g_pz);
        } else {
            for (int attempt = 0; attempt < 20; attempt++) {
                get_random_empty_cell(&d->x, &d->z);
                float dx = d->x - g_px;
                float dz = d->z - g_pz;
                if (dx * dx + dz * dz > 4.0f * 4.0f) {
                    break;
                }
            }
            d->ang = ((float)(rand() % 360)) * (3.14159f / 180.0f);
        }
        d->hp = 2 + (g_current_level >= 40 ? 1 : 0);
        d->active = 1;
        d->fire_cooldown = 0.15f;
        d->hurt_flash = 0.0f;
        d->fly_pulse = 0.0f;
    }
}

static float hellraiser_first_spawn_time(void) {
    return current_level_is_gauntlet() ? HELLRAISER_GAUNTLET_SPAWN_TIME : HELLRAISER_SPAWN_TIME;
}

static int cheat_exit_pressed(void) {
    return SituationIsKeyPressed(SIT_KEY_KP_MULTIPLY) ||
           ((SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT)) &&
            SituationIsKeyPressed(SIT_KEY_8));
}

static int fullscreen_toggle_pressed(void) {
    int alt_down = SituationIsKeyDown(SIT_KEY_LEFT_ALT) || SituationIsKeyDown(SIT_KEY_RIGHT_ALT);
    return SituationIsKeyPressed(SIT_KEY_F11) ||
           (alt_down && SituationIsKeyPressed(SIT_KEY_ENTER));
}

static int g_demon_hunt_maximize_reentry;

static void demon_hunt_prepare_borderless_toggle(void) {
    if (SituationIsWindowMaximized()) {
        SituationRestoreWindow();
    }
    if (SituationIsWindowFullscreen()) {
        SituationToggleFullscreen();
    }
}

static void demon_hunt_toggle_borderless_presentation(void) {
    g_ignore_focus_loss_until = SituationTimerGetTime() + 1.0;
    demon_hunt_prepare_borderless_toggle();
    SituationToggleBorderlessWindowed();
}

static void demon_hunt_maximize_callback(bool maximized, void* user_data) {
    (void)user_data;
    if (g_demon_hunt_maximize_reentry) {
        return;
    }
    g_demon_hunt_maximize_reentry = 1;
    if (maximized) {
        if (!SituationIsWindowState(SITUATION_FLAG_BORDERLESS_WINDOWED_MODE)) {
            demon_hunt_toggle_borderless_presentation();
        } else if (SituationIsWindowMaximized()) {
            SituationRestoreWindow();
        }
    } else if (SituationIsWindowState(SITUATION_FLAG_BORDERLESS_WINDOWED_MODE)) {
        demon_hunt_toggle_borderless_presentation();
    }
    g_demon_hunt_maximize_reentry = 0;
}

static int pause_toggle_pressed(void) {
    return SituationIsKeyPressed(SIT_KEY_P) || SituationIsKeyPressed(SIT_KEY_ESCAPE);
}

static void queue_screenshot(void) {
    g_screenshot_pending = 1;
}

static void take_queued_screenshot(void) {
    if (!g_screenshot_pending) {
        return;
    }
    g_screenshot_pending = 0;

    char filename[96];
    snprintf(filename, sizeof(filename), "demon_hunt_shot_%08u", (unsigned int)(SituationTimerGetTime() * 1000.0));
    SituationError err = SituationTakeScreenshot(filename);
    if (err == SITUATION_SUCCESS) {
        snprintf(g_screenshot_msg, sizeof(g_screenshot_msg), "Saved %s", filename);
    } else {
        snprintf(g_screenshot_msg, sizeof(g_screenshot_msg), "Screenshot failed");
    }
    g_screenshot_msg_timer = 2.5f;
}

static void gauntlet_place_pillars(void) {
    const int px[] = {8, 13, 18, 23};
    const int pz[] = {7, 12, 20, 25};
    for (int zix = 0; zix < (int)(sizeof(pz) / sizeof(pz[0])); zix++) {
        for (int xix = 0; xix < (int)(sizeof(px) / sizeof(px[0])); xix++) {
            int x = px[xix];
            int z = pz[zix];
            if (x > 1 && x < g_map_w - 2 && z > 1 && z < g_map_h - 2) {
                g_map[z][x] = CELL_WALL;
            }
        }
    }
}

static void gauntlet_place_ammo_points(void) {
    const int rows[] = {5, 10, 15, 20, 25};
    int slot = 0;
    for (int r = 0; r < (int)(sizeof(rows) / sizeof(rows[0])) && slot < AMMO_COUNT; r++) {
        for (int x = 5; x <= g_map_w - 6 && slot < AMMO_COUNT; x += 4) {
            int z = rows[r];
            if (z <= 1 || z >= g_map_h - 1 || g_map[z][x] != CELL_EMPTY) {
                continue;
            }
            g_ammo_box[slot].x = (float)x + 0.5f;
            g_ammo_box[slot].z = (float)z + 0.5f;
            g_ammo_box[slot].active = 1;
            g_ammo_box[slot].respawn_timer = 0.0f;
            slot++;
        }
    }
    for (; slot < AMMO_COUNT; slot++) {
        g_ammo_box[slot].active = 0;
        g_ammo_box[slot].respawn_timer = -1.0f;
    }
}

static void carve_rect_cells(int x0, int z0, int x1, int z1, uint8_t cell) {
    if (x0 < 1) x0 = 1;
    if (z0 < 1) z0 = 1;
    if (x1 > g_map_w - 2) x1 = g_map_w - 2;
    if (z1 > g_map_h - 2) z1 = g_map_h - 2;
    for (int z = z0; z <= z1; z++) {
        for (int x = x0; x <= x1; x++) {
            g_map[z][x] = cell;
        }
    }
}

static void generate_long_walk_map(void) {
    int mid = g_map_h / 2;
    carve_rect_cells(1, mid - 2, g_map_w - 2, mid + 2, CELL_EMPTY);

    for (int x = 6; x < g_map_w - 5; x += 7) {
        int upper = ((x / 7) % 2) == 0;
        if (upper) {
            carve_rect_cells(x - 2, 2, x + 3, mid - 3, CELL_EMPTY);
        } else {
            carve_rect_cells(x - 2, mid + 3, x + 3, g_map_h - 3, CELL_EMPTY);
        }
        if (x + 2 < g_map_w - 2) {
            g_map[mid][x + 2] = CELL_WALL;
        }
    }

    g_px = 2.5f;
    g_pz = (float)mid + 0.5f;
    set_exit_cell(g_map_w - 3, mid);
}

/* Phase 2: Assign material IDs to wall cells based on position and level type. */
static void map_assign_materials(LevelKind kind) {
    memset(g_material_map, MAT_STONE, sizeof(g_material_map));
    int is_arena = (kind == LEVEL_KIND_ARENA);

    for (int z = 0; z < g_map_h && z < MAP_MAX_H; z++) {
        for (int x = 0; x < g_map_w && x < MAP_MAX_W; x++) {
            if (g_map[z][x] != CELL_WALL) continue; /* only walls get materials */

            /* Outer border is always stone */
            if (x == 0 || z == 0 || x == g_map_w - 1 || z == g_map_h - 1) {
                g_material_map[z][x] = MAT_STONE;
                continue;
            }

            /* Exit cell neighbors get emissive — only direct wall-adjacent cells */
            if (g_exit_mx >= 0 && g_exit_mz >= 0) {
                int dx = x - g_exit_mx;
                int dz = z - g_exit_mz;
                if (dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1 && (dx != 0 || dz != 0)) {
                    g_material_map[z][x] = MAT_EMISSIVE;
                    continue;
                }
            }

            /* Interior walls: weighted random per cell (seeded by position for stability) */
            unsigned int seed = (unsigned int)(z * 997 + x * 131 + g_current_level * 7919);
            seed ^= seed >> 16;
            seed *= 0x45d9f3b;
            seed ^= seed >> 16;
            int roll = (int)(seed % 100);

            if (is_arena) {
                /* Arena bias: more metal + rusted metal */
                if (roll < 40)      g_material_map[z][x] = MAT_STONE;
                else if (roll < 60) g_material_map[z][x] = MAT_METAL;
                else if (roll < 80) g_material_map[z][x] = MAT_RUSTED_METAL;
                else if (roll < 90) g_material_map[z][x] = MAT_BONE;
                else                g_material_map[z][x] = MAT_WOOD;
            } else {
                /* Standard hunt levels — stone dominant, others rare accents */
                if (roll < 70)      g_material_map[z][x] = MAT_STONE;
                else if (roll < 82) g_material_map[z][x] = MAT_WOOD;
                else if (roll < 90) g_material_map[z][x] = MAT_METAL;
                else if (roll < 96) g_material_map[z][x] = MAT_RUSTED_METAL;
                else                g_material_map[z][x] = MAT_BONE;
            }
        }
    }
}

static void map_generate(void) {
    LevelDef def = current_level_def();
    g_level_config = def.config;
    level_apply_config(&g_level_config);
    g_exit_mx = -1;
    g_exit_mz = -1;
    if (def.kind == LEVEL_KIND_ARENA || def.kind == LEVEL_KIND_LONG_WALK) {
        g_map_w = def.config.width;
        g_map_h = def.config.height;
    }

    for (int z = 0; z < MAP_MAX_H; z++) {
        for (int x = 0; x < MAP_MAX_W; x++) {
            g_map[z][x] = CELL_WALL;
        }
    }

    if (def.kind == LEVEL_KIND_ARENA) {
        for (int z = 1; z < g_map_h - 1; z++) {
            for (int x = 1; x < g_map_w - 1; x++) {
                g_map[z][x] = CELL_EMPTY;
            }
        }
        g_px = 2.5f;
        g_pz = (float)(g_map_h / 2) + 0.5f;
        gauntlet_place_pillars();
        set_exit_cell(g_map_w - 3, g_map_h / 2);
        map_assign_materials(def.kind);
        return;
    }

    if (def.kind == LEVEL_KIND_LONG_WALK) {
        generate_long_walk_map();
        map_assign_materials(def.kind);
        return;
    }
    
    struct { int cx, cz; } zone_centers[LEVEL_MAX_GRID][LEVEL_MAX_GRID];
    int grid_x = g_level_config.grid_x;
    int grid_z = g_level_config.grid_z;
    
    for (int gy = 0; gy < grid_z; gy++) {
        for (int gx = 0; gx < grid_x; gx++) {
            int zone_x0 = (gx * g_map_w) / grid_x;
            int zone_x1 = (((gx + 1) * g_map_w) / grid_x) - 1;
            int zone_z0 = (gy * g_map_h) / grid_z;
            int zone_z1 = (((gy + 1) * g_map_h) / grid_z) - 1;
            int zone_w = zone_x1 - zone_x0 + 1;
            int zone_h = zone_z1 - zone_z0 + 1;
            int max_room_w = g_level_config.room_max_w < zone_w - 2 ? g_level_config.room_max_w : zone_w - 2;
            int max_room_h = g_level_config.room_max_h < zone_h - 2 ? g_level_config.room_max_h : zone_h - 2;
            if (max_room_w < 2) max_room_w = 2;
            if (max_room_h < 2) max_room_h = 2;
            int min_room_w = g_level_config.room_min_w < max_room_w ? g_level_config.room_min_w : max_room_w;
            int min_room_h = g_level_config.room_min_h < max_room_h ? g_level_config.room_min_h : max_room_h;
            int w = min_room_w + (max_room_w > min_room_w ? rand() % (max_room_w - min_room_w + 1) : 0);
            int h = min_room_h + (max_room_h > min_room_h ? rand() % (max_room_h - min_room_h + 1) : 0);
            
            int min_x = zone_x0;
            int max_x = zone_x1 - w + 1;
            int min_z = zone_z0;
            int max_z = zone_z1 - h + 1;
            
            if (min_x < 1) min_x = 1;
            if (min_z < 1) min_z = 1;
            if (max_x >= g_map_w - 1) max_x = g_map_w - 1 - w;
            if (max_z >= g_map_h - 1) max_z = g_map_h - 1 - h;
            
            int x = min_x + (max_x > min_x ? rand() % (max_x - min_x + 1) : 0);
            int z = min_z + (max_z > min_z ? rand() % (max_z - min_z + 1) : 0);
            
            for (int rz = z; rz < z + h; rz++) {
                for (int rx = x; rx < x + w; rx++) {
                    g_map[rz][rx] = CELL_EMPTY;
                }
            }
            
            zone_centers[gy][gx].cx = x + w / 2;
            zone_centers[gy][gx].cz = z + h / 2;
            
            if (gx == 0 && gy == 0) {
                g_px = (float)(x + w / 2) + 0.5f;
                g_pz = (float)(z + h / 2) + 0.5f;
            }
            
            if (gx == grid_x - 1 && gy == grid_z - 1) {
                g_exit_mx = x + w;
                if (g_exit_mx >= g_map_w - 1) {
                    g_exit_mx = x + w - 1;
                }
                g_exit_mz = z + h / 2;
            }
        }
    }
    
    for (int gy = 0; gy < grid_z; gy++) {
        for (int gx = 0; gx < grid_x; gx++) {
            if (gx < grid_x - 1) {
                int x1 = zone_centers[gy][gx].cx;
                int z1 = zone_centers[gy][gx].cz;
                int x2 = zone_centers[gy][gx+1].cx;
                int z2 = zone_centers[gy][gx+1].cz;
                
                int cx = x1, cz = z1;
                while (cx != x2) { g_map[cz][cx] = CELL_EMPTY; cx += (x2 > cx) ? 1 : -1; }
                while (cz != z2) { g_map[cz][cx] = CELL_EMPTY; cz += (z2 > cz) ? 1 : -1; }
            }
            if (gy < grid_z - 1) {
                int x1 = zone_centers[gy][gx].cx;
                int z1 = zone_centers[gy][gx].cz;
                int x2 = zone_centers[gy+1][gx].cx;
                int z2 = zone_centers[gy+1][gx].cz;
                
                int cx = x1, cz = z1;
                while (cz != z2) { g_map[cz][cx] = CELL_EMPTY; cz += (z2 > cz) ? 1 : -1; }
                while (cx != x2) { g_map[cz][cx] = CELL_EMPTY; cx += (x2 > cx) ? 1 : -1; }
            }
        }
    }

    map_place_arches();
    set_exit_cell(g_exit_mx, g_exit_mz);
    map_assign_materials(def.kind);
}

static void score_save_high(void) {
    FILE* f = fopen(HIGH_SCORE_FILE, "w");
    if (!f) {
        return;
    }
    fprintf(f, "%d\n", g_high_score);
    fclose(f);
}

static void score_load_high(void) {
    FILE* f = fopen(HIGH_SCORE_FILE, "r");
    if (!f) {
        g_high_score = 0;
        return;
    }
    if (fscanf(f, "%d", &g_high_score) != 1 || g_high_score < 0) {
        g_high_score = 0;
    }
    fclose(f);
}

static void score_commit_high(void) {
    if (g_score > g_high_score) {
        g_high_score = g_score;
        score_save_high();
    }
}

static void score_add(int points) {
    if (points <= 0) {
        return;
    }
    g_score += points;
    score_commit_high();
}

static void hellraiser_reset(void) {
    SituationSetTimerOscillatorPeriod(HELLRAISER_OSC_ID, 0.75);
    uint64_t sync_ix = SituationTimerGetOscillatorTriggerCount(HELLRAISER_OSC_ID);
    for (int i = 0; i < HELLRAISER_COUNT; i++) {
        g_hellraisers[i].x = 0.0f;
        g_hellraisers[i].z = 0.0f;
        g_hellraisers[i].active = 0;
        g_hellraisers[i].warned = 0;
        g_hellraisers[i].teleport_cooldown = 0.0f;
        g_hellraisers[i].spawn_freeze = 0.0f;
        g_hellraisers[i].sync_ix = sync_ix;
    }
    g_next_hellraiser_spawn_time = hellraiser_first_spawn_time();
}

static int hellraisers_any_active(void) {
    for (int i = 0; i < HELLRAISER_COUNT; i++) {
        if (g_hellraisers[i].active) {
            return 1;
        }
    }
    return 0;
}

static int hellraisers_active_count(void) {
    int n = 0;
    for (int i = 0; i < HELLRAISER_COUNT; i++) {
        if (g_hellraisers[i].active) {
            n++;
        }
    }
    return n;
}

static Hellraiser* hellraiser_first_inactive(void) {
    for (int i = 0; i < HELLRAISER_COUNT; i++) {
        if (!g_hellraisers[i].active) {
            return &g_hellraisers[i];
        }
    }
    return NULL;
}

static int hellraisers_any_warned(void) {
    for (int i = 0; i < HELLRAISER_COUNT; i++) {
        if (g_hellraisers[i].warned) {
            return 1;
        }
    }
    return 0;
}

static const char* death_reason_text(void) {
    switch (g_death_reason) {
        case DEATH_HELLRAISER_TOUCH:
            return "Snared by the wire";
        case DEATH_DEMON_MELEE:
            return "Torn apart by demons";
        case DEATH_HUNTER_DRONE:
            return "Shot down by a hunter drone";
        case DEATH_NONE:
        default:
            return "Lost in the hunt";
    }
}

static void session_reset(int fresh_run) {
    if (fresh_run) {
        g_score = 0;
        g_spawn_number = 1;
        g_current_level = 1;
        g_last_demon_gain = 0;
        g_last_portal_gain = 0;
        g_last_exit_gain = 0;
        g_last_bonus_gain = 0;
        g_last_total_gain = 0;
    } else {
        g_spawn_number++;
    }
    map_generate();
    g_yaw = 0.35f;
    g_pitch = 0.0f;
    g_eye_y = PLAYER_EYE_GROUND_Y;
    g_player_vx = 0.0f;
    g_player_vz = 0.0f;
    g_player_vy = 0.0f;
    g_player_grounded = 1;
    g_health = 40;
    g_ammo = 24;
    g_kills = 0;
    g_death_reason = DEATH_NONE;
    g_muzzle = 0;
    g_bob = 0.0f;
    g_pain_flash = 0.0f;
    g_melee_cooldown = 0.0f;
    g_player_teleport_cooldown = 0.0f;
    g_last_spawn_time = (float)SituationTimerGetTime();
    g_spawn_elapsed = 0.0f;
    hellraiser_reset();
    demons_reset();
    portals_reset();
    teleporters_reset();
    ammo_reset();
    hunter_drones_reset();
    if (current_level_is_gauntlet()) {
        for (int i = 0; i < PORTAL_COUNT; i++) {
            g_portals[i].alive = 0;
        }
        for (int i = 0; i < DEMON_COUNT; i++) {
            g_demon[i].alive = 0;
        }
        gauntlet_place_teleporters();
        gauntlet_place_ammo_points();
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        g_particles[i].life = 0.0f;
    }
    for (int i = 0; i < PLAYER_SHOT_COUNT; i++) {
        g_player_shots[i].active = 0;
    }
    drone_shots_clear();
}

static SituationTexture g_entering_tex;
static int g_entering_tex_ok;
static int g_entering_tex_w;
static int g_entering_tex_h;
static double g_entering_start_time;
static double g_entering_panel_build_time;
static int g_entering_sky_init_done;
static int g_entering_fresh_run;

static ColorYPQA entering_lerp_ypq(ColorYPQA a, ColorYPQA b, float t) {
    ColorYPQA out;
    out.y = (unsigned char)(fminf(255.0f, fmaxf(0.0f, (1.0f - t) * (float)a.y + t * (float)b.y + 0.5f)));
    out.p = (unsigned char)(fminf(255.0f, fmaxf(0.0f, (1.0f - t) * (float)a.p + t * (float)b.p + 0.5f)));
    out.q = (unsigned char)(fminf(255.0f, fmaxf(0.0f, (1.0f - t) * (float)a.q + t * (float)b.q + 0.5f)));
    out.a = (unsigned char)(fminf(255.0f, fmaxf(0.0f, (1.0f - t) * (float)a.a + t * (float)b.a + 0.5f)));
    return out;
}

static void entering_panel_destroy(void) {
    if (g_entering_tex_ok) {
        SituationDestroyTexture(&g_entering_tex);
        memset(&g_entering_tex, 0, sizeof(g_entering_tex));
        g_entering_tex_ok = 0;
    }
    g_entering_tex_w = 0;
    g_entering_tex_h = 0;
}

static void entering_panel_rebuild(int sw, int sh, float anim_t) {
    if (sw < 1) {
        sw = 1;
    }
    if (sh < 1) {
        sh = 1;
    }

    SituationImage panel = {0};
    if (SituationCreateImage(sw, sh, 4, &panel) != SITUATION_SUCCESS) {
        return;
    }
    SituationGenImageColor(sw, sh, (ColorRGBA){10, 5, 8, 255}, &panel);

    ColorYPQA ypq_tl = SituationColorToYPQ((ColorRGBA){52, 12, 16, 255});
    ColorYPQA ypq_tr = SituationColorToYPQ((ColorRGBA){200, 48, 32, 255});
    ColorYPQA ypq_bl = SituationColorToYPQ((ColorRGBA){16, 10, 34, 255});
    ColorYPQA ypq_br = SituationColorToYPQ((ColorRGBA){108, 32, 78, 255});

    int band_h = (sh + ENTERING_PANEL_BANDS - 1) / ENTERING_PANEL_BANDS;
    if (band_h < 1) {
        band_h = 1;
    }

    for (int i = 0; i < ENTERING_PANEL_BANDS; i++) {
        float v = (ENTERING_PANEL_BANDS > 1) ? (float)i / (float)(ENTERING_PANEL_BANDS - 1) : 0.0f;
        float wave = sinf(anim_t * 3.1f + v * 6.2831853f) * 0.5f + 0.5f;

        ColorYPQA left_y = entering_lerp_ypq(ypq_bl, ypq_tl, v);
        ColorYPQA right_y = entering_lerp_ypq(ypq_br, ypq_tr, v);
        left_y.p = (unsigned char)fminf(255.0f, (float)left_y.p + wave * 24.0f);
        right_y.p = (unsigned char)fminf(255.0f, (float)right_y.p + (1.0f - wave) * 24.0f);
        left_y.q = (unsigned char)fminf(255.0f, (float)left_y.q * (0.86f + wave * 0.28f));
        right_y.q = (unsigned char)fminf(255.0f, (float)right_y.q * (0.86f + (1.0f - wave) * 0.28f));

        ColorRGBA left = SituationColorFromYPQ(left_y);
        ColorRGBA right = SituationColorFromYPQ(right_y);

        int y0 = i * band_h;
        int bh = band_h;
        if (y0 >= sh) {
            break;
        }
        if (y0 + bh > sh) {
            bh = sh - y0;
        }

        SituationImage band = {0};
        if (SituationGenImageGradient(sw, bh, left, right, left, right, &band) != SITUATION_SUCCESS) {
            continue;
        }
        SitRectangle src = {0.0f, 0.0f, (float)band.width, (float)band.height};
        SituationImageDrawAlpha(&panel, band, src, (Vector2){{0.0f, (float)y0}}, (ColorRGBA){255, 255, 255, 255});
        SituationUnloadImage(band);
    }

    SituationTexture new_tex = {0};
    if (SituationCreateTexture(panel, false, &new_tex) == SITUATION_SUCCESS) {
        if (g_entering_tex_ok) {
            SituationDestroyTexture(&g_entering_tex);
        }
        g_entering_tex = new_tex;
        g_entering_tex_ok = 1;
        g_entering_tex_w = sw;
        g_entering_tex_h = sh;
    }
    SituationUnloadImage(panel);
    g_entering_panel_build_time = SituationTimerGetTime();
}

static int g_sky_ok;
static int g_sky_async_started;
static int g_sky_async_failed;
static double g_sky_load_log_time;
static double g_sky_load_log_time;

static int sky_loading_overlay_active(void) {
    return !g_sky_ok && g_sky_async_started && !g_sky_async_failed;
}

static void begin_entering(int fresh_run) {
    g_entering_fresh_run = fresh_run ? 1 : 0;
    g_entering_start_time = SituationTimerGetTime();
    g_entering_panel_build_time = 0.0;
    g_entering_sky_init_done = 0;
    g_phase = PHASE_ENTERING;
    entering_panel_rebuild(GAME_RENDER_W, GAME_RENDER_H, 0.0f);
}

static void complete_entering(void) {
    srand((unsigned int)(SituationTimerGetTime() * 10000.0));
    if (g_entering_fresh_run) {
        session_reset(1);
    } else {
        session_reset(0);
    }
    g_phase = PHASE_PLAY;
    SituationDisableCursor();
    melo_request(MELOK_START);
}

static void update_entering(int sw, int sh) {
    float now = SituationTimerGetTime();
    float elapsed = (float)(now - g_entering_start_time);
    if (now - g_entering_panel_build_time >= ENTERING_PANEL_REBUILD_SEC) {
        entering_panel_rebuild(sw, sh, elapsed);
    }

    if (elapsed >= ENTERING_MIN_SEC) {
        complete_entering();
    }
}

static void begin_next_level_or_new_game(void) {
    if (g_current_level >= LEVEL_COUNT) {
        begin_entering(1);
    } else {
        g_current_level++;
        begin_entering(0);
    }
}

/* DDA ray on g_map toward sun. 1 = lit, 0 = shadowed by a wall. */
static int sun_ray_lit(float ox, float oy, float oz, float lit_x, float lit_y, float lit_z) {
    if (lit_y <= 0.0f) {
        return 0;
    }
    float lh = sqrtf(lit_x * lit_x + lit_z * lit_z);
    if (lh < 1e-5f) {
        return 1;
    }

    /* Ray travels in XZ until it rises above the wall top (y=1). \n"
       Multiply max_dist by 4.0 to simulate taller walls and cast longer, moodier shadows */
    float max_dist = ((1.0f - oy) / lit_y) * lh * 4.0f;
    if (max_dist < 0.0f) {
        max_dist = 0.0f;
    }

    float rdx = lit_x / lh;
    float rdz = lit_z / lh;
    ox += rdx * 0.01f;
    oz += rdz * 0.01f;

    int map_x = (int)floorf(ox);
    int map_z = (int)floorf(oz);
    float side_dist_x, side_dist_z;
    float delta_x = fabsf(rdx) < 1e-7f ? 1e30f : fabsf(1.0f / rdx);
    float delta_z = fabsf(rdz) < 1e-7f ? 1e30f : fabsf(1.0f / rdz);
    int step_x, step_z;
    if (rdx < 0.0f) {
        step_x = -1;
        side_dist_x = (ox - (float)map_x) * delta_x;
    } else {
        step_x = 1;
        side_dist_x = ((float)map_x + 1.0f - ox) * delta_x;
    }
    if (rdz < 0.0f) {
        step_z = -1;
        side_dist_z = (oz - (float)map_z) * delta_z;
    } else {
        step_z = 1;
        side_dist_z = ((float)map_z + 1.0f - oz) * delta_z;
    }

    float dist = 0.0f;
    for (int guard = 0; guard < 256; guard++) {
        if (map_x < 0 || map_z < 0 || map_x >= g_map_w || map_z >= g_map_h) {
            return 1;
        }
        if (g_map[map_z][map_x] == CELL_WALL) {
            if (dist < max_dist) {
                return 0;
            }
            return 1;
        }
        if (side_dist_x < side_dist_z) {
            dist = side_dist_x;
            side_dist_x += delta_x;
            map_x += step_x;
        } else {
            dist = side_dist_z;
            side_dist_z += delta_z;
            map_z += step_z;
        }
        if (dist > max_dist) {
            break;
        }
    }
    return 1;
}

typedef struct {
    float dist;  /* perpendicular (fisheye-corrected) for wall height */
    float along; /* distance along the ray to the hit (for hitscan vs sprites) */
    int side;    /* 0 = NS wall, 1 = EW */
    uint8_t cell;
} RayHit;

/* DDA distance along the ray, then fisheye-correct with camera forward (dirx, dirz). */
static void cast_ray(float ox, float oz, float rdx, float rdz, float dirx, float dirz, RayHit* out) {
    int map_x = (int)floorf(ox);
    int map_z = (int)floorf(oz);

    float side_dist_x, side_dist_z;
    float delta_x = fabsf(rdx) < 1e-7f ? 1e30f : fabsf(1.0f / rdx);
    float delta_z = fabsf(rdz) < 1e-7f ? 1e30f : fabsf(1.0f / rdz);
    int step_x, step_z;
    int side = 0;

    if (rdx < 0.0f) {
        step_x = -1;
        side_dist_x = (ox - (float)map_x) * delta_x;
    } else {
        step_x = 1;
        side_dist_x = ((float)map_x + 1.0f - ox) * delta_x;
    }
    if (rdz < 0.0f) {
        step_z = -1;
        side_dist_z = (oz - (float)map_z) * delta_z;
    } else {
        step_z = 1;
        side_dist_z = ((float)map_z + 1.0f - oz) * delta_z;
    }

    int hit = 0;
    uint8_t cell = 0;
    for (int guard = 0; guard < 512; guard++) {
        if (side_dist_x < side_dist_z) {
            side_dist_x += delta_x;
            map_x += step_x;
            side = 0;
        } else {
            side_dist_z += delta_z;
            map_z += step_z;
            side = 1;
        }
        if (map_x < 0 || map_z < 0 || map_x >= g_map_w || map_z >= g_map_h) {
            hit = 1;
            cell = 1;
            break;
        }
        cell = g_map[map_z][map_x];
        if (cell == CELL_WALL) {
            hit = 1;
            break;
        }
    }

    float raw = (side == 0) ? (side_dist_x - delta_x) : (side_dist_z - delta_z);
    if (raw < 0.0f) {
        raw = 0.0f;
    }
    float perp = raw * (rdx * dirx + rdz * dirz);
    if (perp < 0.02f) {
        perp = 0.02f;
    }
    out->along = raw;
    out->dist = perp;
    out->side = side;
    out->cell = cell;
    (void)hit;
}

static float segment_point_dist_sq(
    float ax, float ay, float az,
    float bx, float by, float bz,
    float px, float py, float pz) {
    float abx = bx - ax;
    float aby = by - ay;
    float abz = bz - az;
    float apx = px - ax;
    float apy = py - ay;
    float apz = pz - az;
    float ab_len_sq = abx * abx + aby * aby + abz * abz;
    float t = 0.0f;
    if (ab_len_sq > 1e-8f) {
        t = (apx * abx + apy * aby + apz * abz) / ab_len_sq;
        if (t < 0.0f) {
            t = 0.0f;
        } else if (t > 1.0f) {
            t = 1.0f;
        }
    }
    float cx = ax + abx * t - px;
    float cy = ay + aby * t - py;
    float cz = az + abz * t - pz;
    return cx * cx + cy * cy + cz * cz;
}

static int drone_shot_wall_hit_segment(
    float prev_x, float prev_y, float prev_z,
    float end_x, float end_y, float end_z,
    float dir_x, float dir_y, float dir_z,
    float seg_len_xz,
    float* hit_x, float* hit_y, float* hit_z) {
    if (seg_len_xz < 1e-5f) {
        return 0;
    }

    int steps = (int)(seg_len_xz / 0.10f) + 1;
    if (steps < 2) {
        steps = 2;
    }
    if (steps > 28) {
        steps = 28;
    }

    for (int s = 1; s <= steps; s++) {
        float t = (float)s / (float)steps;
        float px = prev_x + (end_x - prev_x) * t;
        float py = prev_y + (end_y - prev_y) * t;
        float pz = prev_z + (end_z - prev_z) * t;
        int mx = (int)floorf(px);
        int mz = (int)floorf(pz);
        if (mx < 0 || mz < 0 || mx >= g_map_w || mz >= g_map_h) {
            if (hit_x) {
                *hit_x = px;
            }
            if (hit_y) {
                *hit_y = py;
            }
            if (hit_z) {
                *hit_z = pz;
            }
            return 1;
        }
        if (g_map[mz][mx] == CELL_WALL) {
            if (hit_x) {
                *hit_x = px;
            }
            if (hit_y) {
                *hit_y = py;
            }
            if (hit_z) {
                *hit_z = pz;
            }
            return 1;
        }
    }
    (void)dir_x;
    (void)dir_y;
    (void)dir_z;
    return 0;
}

static void sun_direction(float* lx, float* ly, float* lz) {
    double tt = SituationTimerGetTime();
    float a = (float)tt * 0.055f;
    float sxv = 0.52f + sinf(a) * 0.22f;
    float szv = 0.68f + cosf(a * 1.17f) * 0.14f;
    float syv = 0.75f + sinf(a * 0.85f) * 0.15f;
    float invl = 1.0f / sqrtf(sxv * sxv + syv * syv + szv * szv);
    *lx = sxv * invl;
    *ly = syv * invl;
    *lz = szv * invl;
}

static void draw_rect_px(SituationCommandBuffer cmd, float x, float y, float w, float h, Vector4 color) {
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, color);
}

#define UI_BASE_W 960.0f
#define UI_BASE_H 600.0f

typedef struct {
    float scale;
    float ox;
    float oy;
} UiLayout;

static UiLayout ui_layout_for(int sw, int sh) {
    float sx = (float)sw / UI_BASE_W;
    float sy = (float)sh / UI_BASE_H;
    float scale = fminf(sx, sy);
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    UiLayout ui;
    ui.scale = scale;
    ui.ox = ((float)sw - UI_BASE_W * scale) * 0.5f;
    ui.oy = ((float)sh - UI_BASE_H * scale) * 0.5f;
    return ui;
}

static Vector2 ui_pos(UiLayout ui, float x, float y) {
    return (Vector2){{ui.ox + x * ui.scale, ui.oy + y * ui.scale}};
}

static float ui_text_width(const char* text, float size, float spacing) {
    size_t len = text ? strlen(text) : 0;
    if (len == 0) {
        return 0.0f;
    }
    return (float)len * size + (float)(len - 1) * spacing;
}

static void draw_text_ui(SituationCommandBuffer cmd, UiLayout ui, const char* text, float x, float y, float size, float spacing, ColorRGBA color) {
    SituationCmdDrawTextEx(cmd, g_font, text, ui_pos(ui, x, y), size * ui.scale, 0.0f, color);
}

static void draw_text_fit_ui(SituationCommandBuffer cmd, UiLayout ui, const char* text, float x, float y, float size, float spacing, float max_w, ColorRGBA color) {
    float fit_size = size;
    float fit_spacing = spacing;
    float w = ui_text_width(text, fit_size, fit_spacing);
    if (w > max_w && w > 0.0f) {
        float shrink = max_w / w;
        fit_size *= shrink;
        fit_spacing *= shrink;
    }
    draw_text_ui(cmd, ui, text, x, y, fit_size, fit_spacing, color);
}

static void draw_text_center_fit_ui(SituationCommandBuffer cmd, UiLayout ui, const char* text, float center_x, float y, float size, float spacing, float max_w, ColorRGBA color) {
    float fit_size = size;
    float fit_spacing = spacing;
    float w = ui_text_width(text, fit_size, fit_spacing);
    if (w > max_w && w > 0.0f) {
        float shrink = max_w / w;
        fit_size *= shrink;
        fit_spacing *= shrink;
        w = ui_text_width(text, fit_size, fit_spacing);
    }
    draw_text_ui(cmd, ui, text, center_x - w * 0.5f, y, fit_size, fit_spacing, color);
}

static void draw_rect_ui(SituationCommandBuffer cmd, UiLayout ui, float x, float y, float w, float h, Vector4 color) {
    draw_rect_px(cmd, ui.ox + x * ui.scale, ui.oy + y * ui.scale, w * ui.scale, h * ui.scale, color);
}

#define TITLE_BORDER_THICK 18.0f
#define TITLE_BORDER_SEG 40

static Vector4 title_border_vec4(ColorRGBA c, float alpha_mul) {
    return (Vector4){{
        c.r / 255.0f,
        c.g / 255.0f,
        c.b / 255.0f,
        (c.a / 255.0f) * alpha_mul}};
}

/* Copper title palette — lerp/modulate in YPQ (Y=luma, P=phase/hue, Q=sat), then RGB for DrawText. */
static ColorYPQA title_copper_ypq_stops[4];

static void title_copper_ypq_init_once(void) {
    static int done;
    if (done) {
        return;
    }
    title_copper_ypq_stops[0] = SituationColorToYPQ((ColorRGBA){34, 18, 12, 255});   /* deep bronze shadow */
    title_copper_ypq_stops[1] = SituationColorToYPQ((ColorRGBA){148, 72, 34, 255});  /* body copper */
    title_copper_ypq_stops[2] = SituationColorToYPQ((ColorRGBA){232, 148, 56, 255}); /* warm highlight */
    title_copper_ypq_stops[3] = SituationColorToYPQ((ColorRGBA){255, 218, 132, 255}); /* specular glint */
    done = 1;
}

static float title_copper_wrap_t(float t) {
    t -= floorf(t);
    if (t < 0.0f) {
        t += 1.0f;
    }
    return t;
}

/** Smooth copper field in YPQ; scissor bands sample this so adjacent lines stay close. */
static ColorRGBA title_copper_sample_bar(float v_norm, float scroll, float pulse_boost) {
    title_copper_ypq_init_once();

    /* Wide palette sweep (continuous stop chain), not 16 hard palette indices. */
    float u = title_copper_wrap_t(v_norm * 1.15f + scroll * 0.42f);
    float u4 = u * 4.0f;
    int i0 = (int)u4;
    if (i0 > 3) {
        i0 = 3;
    }
    float f = u4 - (float)i0;
    f = f * f * (3.0f - 2.0f * f);
    int i1 = (i0 + 1) & 3;

    ColorYPQA y = entering_lerp_ypq(title_copper_ypq_stops[i0], title_copper_ypq_stops[i1], f);

    /* Light scan/spec tied to v (no per-band floor — avoids line-to-line jumps). */
    float scan = 0.96f + 0.04f * sinf((v_norm + scroll) * 6.2831853f * 2.0f);
    float spec = sinf((v_norm + scroll) * 6.2831853f * 3.0f + pulse_boost * 2.0f) * 0.5f + 0.5f;
    y.p = (unsigned char)fminf(255.0f, (float)y.p + spec * 10.0f + pulse_boost * 10.0f);
    y.q = (unsigned char)fminf(255.0f, (float)y.q * (0.90f + spec * 0.14f) * scan);
    y.y = (unsigned char)fminf(255.0f, (float)y.y * (0.94f + spec * 0.10f + pulse_boost * 0.10f) * scan);
    y.a = 255;
    return SituationColorFromYPQ(y);
}

static void title_scissor_reset(SituationCommandBuffer cmd) {
    int rw = SituationGetRenderWidth();
    int rh = SituationGetRenderHeight();
    if (rw < 1) {
        rw = 1;
    }
    if (rh < 1) {
        rh = 1;
    }
    SituationCmdSetScissor(cmd, 0, 0, rw, rh);
}

static void title_scissor_ui_rect(SituationCommandBuffer cmd, UiLayout ui, float x, float y, float w, float h) {
    int sx = (int)floorf(ui.ox + x * ui.scale);
    int sy = (int)floorf(ui.oy + y * ui.scale);
    int sw = (int)ceilf(w * ui.scale);
    int sh = (int)ceilf(h * ui.scale);
    if (sw < 1) {
        sw = 1;
    }
    if (sh < 1) {
        sh = 1;
    }
    SituationCmdSetScissor(cmd, sx, sy, sw, sh);
}

/**
 * Copper title: composite the string in horizontal scan bands (scissored draws).
 * Each band gets a YPQ copper sample so color scrolls vertically through the letters.
 */
static void title_draw_copper_text_center(SituationCommandBuffer cmd, UiLayout ui, const char* text,
    float center_x, float y, float size, float spacing, float max_w, float scroll, float pulse, float blink) {
    if (!text || !text[0]) {
        return;
    }

    float fit_size = size;
    float fit_spacing = spacing;
    float w = ui_text_width(text, fit_size, fit_spacing);
    if (w > max_w && w > 0.0f) {
        float shrink = max_w / w;
        fit_size *= shrink;
        fit_spacing *= shrink;
        w = ui_text_width(text, fit_size, fit_spacing);
    }

    float x0 = center_x - w * 0.5f;
    const float cap_h = fit_size * 1.10f;
    /* Thin bands so scissor steps approximate a smooth vertical gradient. */
    const float stripe_h = 2.0f;
    int bands = (int)ceilf(cap_h / stripe_h);
    if (bands < 20) {
        bands = 20;
    }
    if (bands > 56) {
        bands = 56;
    }

    float spec = sinf((float)SituationTimerGetTime() * 3.4f + blink * 2.0f) * 0.5f + 0.5f;
    float pulse_boost = pulse * 0.35f + spec * 0.18f;

    title_copper_ypq_init_once();

    ColorRGBA shadow = SituationColorFromYPQ(title_copper_ypq_stops[0]);
    shadow.a = 220;
    draw_text_ui(cmd, ui, text, x0 + 3.0f, y + 3.0f, fit_size, fit_spacing, shadow);

    for (int b = 0; b < bands; b++) {
        float y0 = y + (float)b * stripe_h;
        float v = ((float)b + 0.5f) / (float)bands;
        ColorRGBA band_col = title_copper_sample_bar(v, scroll, pulse_boost);

        title_scissor_ui_rect(cmd, ui, x0 - 2.0f, y0, w + 4.0f, stripe_h + 0.5f);
        draw_text_ui(cmd, ui, text, x0, y, fit_size, fit_spacing, band_col);
    }

    /* Soft highlight pass on upper palette range only (sparse bands). */
    for (int b = 0; b < bands; b += 3) {
        float v = ((float)b + 0.5f) / (float)bands;
        float t_glint = title_copper_wrap_t(v + scroll * 0.9f + 0.18f);
        if (t_glint < 0.58f || t_glint > 0.88f) {
            continue;
        }
        float y0 = y + (float)b * stripe_h;
        ColorRGBA glint = title_copper_sample_bar(v + 0.04f, scroll + 0.03f, pulse_boost + 0.18f);
        glint.a = (unsigned char)fminf(255.0f, 48.0f + spec * 90.0f);

        title_scissor_ui_rect(cmd, ui, x0 - 2.0f, y0 - 0.5f, w + 4.0f, stripe_h + 1.0f);
        draw_text_ui(cmd, ui, text, x0 - 0.5f, y - 0.5f, fit_size, fit_spacing, glint);
    }

    title_scissor_reset(cmd);
}

static ColorRGBA title_border_sample_ypq(float t, float scroll) {
    t += scroll;
    t -= floorf(t);
    if (t < 0.0f) {
        t += 1.0f;
    }

    ColorYPQA ypq_stops[4] = {
        SituationColorToYPQ((ColorRGBA){200, 48, 32, 255}),
        SituationColorToYPQ((ColorRGBA){108, 32, 78, 255}),
        SituationColorToYPQ((ColorRGBA){16, 10, 34, 255}),
        SituationColorToYPQ((ColorRGBA){52, 12, 16, 255}),
    };

    float u = t * 4.0f;
    int i0 = (int)u;
    if (i0 >= 4) {
        i0 = 3;
    }
    int i1 = (i0 + 1) & 3;
    float f = u - (float)i0;

    ColorYPQA y = entering_lerp_ypq(ypq_stops[i0], ypq_stops[i1], f);
    float wave = sinf(t * 6.2831853f * 2.0f + scroll * 6.2831853f) * 0.5f + 0.5f;
    y.p = (unsigned char)fminf(255.0f, (float)y.p + wave * 30.0f);
    y.q = (unsigned char)fminf(255.0f, (float)y.q * (0.80f + wave * 0.40f));
    y.y = (unsigned char)fminf(255.0f, (float)y.y * (0.92f + (1.0f - wave) * 0.12f));
    y.a = 255;
    return SituationColorFromYPQ(y);
}

static void enter_title_phase(void) {
    g_phase = PHASE_TITLE;
    g_title_show_instructions = 0;
}

static void title_draw_ypq_border_scroll(SituationCommandBuffer cmd, UiLayout ui, float scroll) {
    const float gw = UI_BASE_W;
    const float gh = UI_BASE_H;
    const float thick = TITLE_BORDER_THICK;
    const float perimeter = 2.0f * (gw + gh);
    const int seg = TITLE_BORDER_SEG;
    const float alpha = 0.96f;
    const float dx = gw / (float)seg;
    const float dy = gh / (float)seg;
    int i;

    for (i = 0; i < seg; i++) {
        float x = (float)i * dx;
        float t = (x + dx * 0.5f) / perimeter;
        draw_rect_ui(cmd, ui, x, 0.0f, dx + 0.5f, thick, title_border_vec4(title_border_sample_ypq(t, scroll), alpha));
    }
    for (i = 0; i < seg; i++) {
        float y = (float)i * dy;
        float t = (gw + y + dy * 0.5f) / perimeter;
        draw_rect_ui(cmd, ui, gw - thick, y, thick, dy + 0.5f, title_border_vec4(title_border_sample_ypq(t, scroll), alpha));
    }
    for (i = 0; i < seg; i++) {
        float x = gw - (float)(i + 1) * dx;
        float t = (gw + gh + ((float)i + 0.5f) * dx) / perimeter;
        draw_rect_ui(cmd, ui, x, gh - thick, dx + 0.5f, thick, title_border_vec4(title_border_sample_ypq(t, scroll), alpha));
    }
    for (i = 0; i < seg; i++) {
        float y = gh - (float)(i + 1) * dy;
        float t = (gw + gh + gw + ((float)i + 0.5f) * dy) / perimeter;
        draw_rect_ui(cmd, ui, 0.0f, y, thick, dy + 0.5f, title_border_vec4(title_border_sample_ypq(t, scroll), alpha));
    }
}

static void title_draw_backdrop(SituationCommandBuffer cmd, UiLayout ui, int sw, int sh, float border_scroll) {
    Vector4 bg = {{0.04f, 0.03f, 0.06f, 1.0f}};
    draw_rect_px(cmd, 0.0f, 0.0f, (float)sw, (float)sh, bg);
    title_draw_ypq_border_scroll(cmd, ui, border_scroll);
    draw_rect_ui(cmd, ui, TITLE_BORDER_THICK, TITLE_BORDER_THICK, UI_BASE_W - TITLE_BORDER_THICK * 2.0f, UI_BASE_H - TITLE_BORDER_THICK * 2.0f,
                 (Vector4){{0.04f, 0.03f, 0.06f, 0.72f}});
}

static void title_draw_main_panel(SituationCommandBuffer cmd, UiLayout ui, float scroll, float pulse, float blink) {
    title_draw_copper_text_center(cmd, ui, "DEMON HUNT", 480.0f, 232.0f, 58.0f, 1.0f, 900.0f, scroll, pulse, blink);
    draw_text_center_fit_ui(
        cmd, ui, "SPACE / ENTER  begin",
        480.0f, 332.0f, 17.0f, 1.0f, 900.0f,
        (ColorRGBA){(unsigned char)(165 + blink * 55), 255, (unsigned char)(165 + blink * 35), 255});
    draw_text_center_fit_ui(cmd, ui, "I  instructions", 480.0f, 360.0f, 14.0f, 1.0f, 900.0f, (ColorRGBA){188, 205, 238, 235});
    draw_text_center_fit_ui(cmd, ui, "ESC  quit", 480.0f, 386.0f, 13.0f, 1.0f, 900.0f, (ColorRGBA){160, 178, 210, 220});
    draw_text_center_fit_ui(cmd, ui, "(P) 2026 - Jacques Morel", 480.0f, 558.0f, 11.5f, 1.0f, 900.0f, (ColorRGBA){140, 132, 155, 210});
}

static void title_draw_instructions_panel(SituationCommandBuffer cmd, UiLayout ui, float pulse) {
    draw_text_center_fit_ui(cmd, ui, "INSTRUCTIONS", 480.0f, 34.0f, 22.0f, 1.0f, 900.0f, (ColorRGBA){255, 212, 108, 255});
    draw_text_center_fit_ui(cmd, ui, "ONE HUNDRED FLOORS", 480.0f, 64.0f, 14.0f, 1.0f, 900.0f, (ColorRGBA){175, 168, 195, 235});
    {
        char score_line[96];
        snprintf(score_line, sizeof(score_line), "HIGH SCORE  %06d", g_high_score);
        draw_text_center_fit_ui(cmd, ui, score_line, 480.0f, 88.0f, 19.0f, 1.0f, 900.0f, (ColorRGBA){255, 208, 96, 255});
    }

    draw_rect_ui(cmd, ui, 268.0f, 108.0f, 424.0f, 2.0f, (Vector4){{0.55f, 0.14f, 0.08f, 0.40f + pulse * 0.22f}});
    draw_rect_ui(cmd, ui, 204.0f, 118.0f, 552.0f, 72.0f, (Vector4){{0.0f, 0.0f, 0.0f, 0.40f}});
    draw_text_center_fit_ui(cmd, ui, "Escalating mazes · seal the portals · reach the exit", 480.0f, 130.0f, 13.5f, 1.0f, 900.0f, (ColorRGBA){215, 205, 175, 245});
    draw_text_center_fit_ui(cmd, ui, "Every tenth floor is a gauntlet run — no room to stall", 480.0f, 152.0f, 12.5f, 1.0f, 900.0f, (ColorRGBA){195, 185, 165, 225});
    draw_text_center_fit_ui(cmd, ui, "Raycast corridors + painterly skydome (Situation sample)", 480.0f, 172.0f, 12.0f, 1.0f, 900.0f, (ColorRGBA){158, 168, 205, 215});
    draw_text_center_fit_ui(cmd, ui, "Mouse look: yaw + pitch", 480.0f, 192.0f, 12.0f, 1.0f, 900.0f, (ColorRGBA){188, 205, 238, 220});

    draw_rect_ui(cmd, ui, 214.0f, 210.0f, 532.0f, 78.0f, (Vector4){{0.09f, 0.04f, 0.06f, 0.42f}});
    draw_text_center_fit_ui(cmd, ui, "WASD / arrows walk   SHIFT run   SPACE jump   Click / Ctrl shoot", 480.0f, 222.0f, 12.5f, 1.0f, 900.0f, (ColorRGBA){172, 228, 188, 240});
    draw_text_center_fit_ui(cmd, ui, "P / ESC pause   F11 / Alt+Enter borderless   F10 FPS   V VSync", 480.0f, 244.0f, 12.0f, 1.0f, 900.0f, (ColorRGBA){160, 198, 232, 225});
    draw_text_center_fit_ui(cmd, ui, "F9 shader sprites   F12 screenshot", 480.0f, 264.0f, 12.0f, 1.0f, 900.0f, (ColorRGBA){150, 188, 222, 215});

    draw_rect_ui(cmd, ui, 292.0f, 298.0f, 376.0f, 138.0f, (Vector4){{0.0f, 0.0f, 0.0f, 0.36f}});
    draw_text_center_fit_ui(cmd, ui, "SCORING", 480.0f, 304.0f, 14.0f, 1.0f, 900.0f, (ColorRGBA){255, 212, 108, 255});
    {
        char score_line[96];
        snprintf(score_line, sizeof(score_line), "Demon kill       +%d", SCORE_DEMON);
        draw_text_center_fit_ui(cmd, ui, score_line, 480.0f, 328.0f, 12.5f, 1.0f, 900.0f, (ColorRGBA){218, 228, 242, 238});
        snprintf(score_line, sizeof(score_line), "Portal destroyed  +%d", SCORE_PORTAL);
        draw_text_center_fit_ui(cmd, ui, score_line, 480.0f, 348.0f, 12.5f, 1.0f, 900.0f, (ColorRGBA){218, 228, 242, 238});
        snprintf(score_line, sizeof(score_line), "Reach exit        +%d", SCORE_EXIT);
        draw_text_center_fit_ui(cmd, ui, score_line, 480.0f, 368.0f, 12.5f, 1.0f, 900.0f, (ColorRGBA){218, 228, 242, 238});
        snprintf(score_line, sizeof(score_line), "Gauntlet pickup / bonuses  +%d  ·  +%d / +%d", SCORE_AMMO_PICKUP, SCORE_HEALTH_BONUS, SCORE_AMMO_BONUS);
        draw_text_center_fit_ui(cmd, ui, score_line, 480.0f, 388.0f, 12.0f, 1.0f, 900.0f, (ColorRGBA){178, 222, 202, 228});
    }
    draw_text_center_fit_ui(cmd, ui, "Linger too long and something else joins the hunt.", 480.0f, 448.0f, 11.5f, 1.0f, 900.0f, (ColorRGBA){175, 132, 118, 195});

    if (!g_sky_ok) {
        draw_text_center_fit_ui(cmd, ui, "World shader loads when you start a hunt.", 480.0f, 478.0f, 12.0f, 1.0f, 900.0f, (ColorRGBA){180, 200, 220, 220});
    }

    draw_text_center_fit_ui(cmd, ui, "SPACE / ENTER  begin     ESC / I  back", 480.0f, 532.0f, 13.0f, 1.0f, 900.0f, (ColorRGBA){165, 255, 185, 240});
}

/* Builds a deliberately level view-projection matrix.
 * Pitch is applied later as a horizon shift so the raycaster keeps perfectly
 * vertical walls while still letting the player look up and down.
 */
static void build_level_wall_camera(SituationCameraDesc* cam, mat4 inv_vp, float aspect) {
    cam->eye = (Vector3){{g_px, g_eye_y, g_pz}};
    
    float fx = sinf(g_yaw);
    float fy = 0.0f;
    float fz = cosf(g_yaw);
    
    cam->target = (Vector3){{g_px + fx, g_eye_y + fy, g_pz + fz}};
    cam->up = (Vector3){{0.0f, 1.0f, 0.0f}};
    cam->vertical_fov_deg = FOV_DEG;
    cam->aspect = aspect;
    cam->z_near = 0.05f;
    cam->z_far = 100.0f;
    cam->flags = SIT_CAMERA_FLAG_NONE;

    SituationCameraBuildInvViewProj(cam, inv_vp);
}

static float compute_horizon_shift_px(float sh) {
    float half_fov_rad = (float)(FOV_DEG * (M_PI / 180.0) * 0.5);
    float focal_px = (sh * 0.5f) / tanf(half_fov_rad);
    /* Pitch only — jump height is already in cam.eye.y / uCamPos, not here. */
    return tanf(g_pitch) * focal_px;
}

static int project_point(Vector3 pos, int sw, int sh, mat4 flat_vp, float* out_sx, float* out_sy, float* out_perp) {
    vec4 world_pos = {pos.x, pos.y, pos.z, 1.0f};
    vec4 clip_pos;
    glm_mat4_mulv(flat_vp, world_pos, clip_pos);
    
    if (clip_pos[3] < 0.05f) {
        return 0; // Behind camera
    }
    
    float ndc_x = clip_pos[0] / clip_pos[3];
    float flat_ndc_y = clip_pos[1] / clip_pos[3];
    
    float sx = (float)sw * (0.5f + ndc_x * 0.5f);
    float sy = (float)sh * 0.5f * (1.0f - flat_ndc_y) + compute_horizon_shift_px((float)sh);
    
    *out_sx = sx;
    *out_sy = sy;
    *out_perp = clip_pos[3];
    return 1;
}


/* --- Fullscreen skydome + floor (GLSL); maze shadow raymarched on floor in XZ --- */
typedef struct {
    float pos[3];
    float nrm[3];
    float uv[2];
} SkyVtx;

/* std430 — must match demon_hunt_sky.fs ShaderScenePack (before spriteData[]). */
typedef struct {
    int map_size[2];
    int wall_rows[MAP_MAX_H];
    int arch_ns_rows[MAP_MAX_H];
    int arch_ew_rows[MAP_MAX_H];
    int material_rows[128];  /* 4-bit material ID per cell, packed 8 cells per int, 4 ints per row */
    int align_pad[2];
    float teleporters[TELEPORTER_MAX_COUNT][4];
    float hellraisers[HELLRAISER_COUNT][4];
    float player_shots[PLAYER_SHOT_COUNT][4];
    float drone_shots[DRONE_SHOT_COUNT][4];
    float drone_shot_dirs[DRONE_SHOT_COUNT][4];
} SkySceneSsboHeader;

#define SKY_SCENE_SSBO_HEADER_BYTES ((size_t)sizeof(SkySceneSsboHeader))
#define SKY_SCENE_SSBO_SPRITE_BYTES ((size_t)SHADER_SPRITE_SSBO_VEC4S * sizeof(float) * 4)
#define SKY_SCENE_SSBO_BYTES (SKY_SCENE_SSBO_HEADER_BYTES + SKY_SCENE_SSBO_SPRITE_BYTES)

static SituationShader g_sky_shader;
static SituationMesh g_sky_mesh;
static SituationBuffer g_sky_frame_ubo;
static int g_sky_frame_ubo_ok;
static SituationBuffer g_scene_ssbo;
static uint8_t g_scene_ssbo_cpu[SKY_SCENE_SSBO_BYTES];
static int g_scene_ssbo_ok;

/* Phase 1.5: Previous-frame feedback texture */
static SituationTexture g_feedback_tex;
static int g_feedback_tex_ok;
static int g_feedback_tex_w;
static int g_feedback_tex_h;

/* Phase 1.5 step 1.5.7: Recreate feedback texture if render size changed.
 * In practice GAME_RENDER_W/H are compile-time constants so this is a safety net. */
static void feedback_tex_ensure_size(int sw, int sh) {
    if (!g_feedback_tex_ok) return;
    if (g_feedback_tex_w == sw && g_feedback_tex_h == sh) return;

    /* Size mismatch — destroy and recreate */
    SituationDestroyTexture(&g_feedback_tex);
    memset(&g_feedback_tex, 0, sizeof(g_feedback_tex));
    g_feedback_tex_ok = 0;

    size_t fb_bytes = (size_t)sw * (size_t)sh * 4;
    uint8_t* fb_pixels = (uint8_t*)malloc(fb_bytes);
    if (fb_pixels) {
        for (size_t i = 0; i < fb_bytes; i += 4) {
            fb_pixels[i + 0] = 20;
            fb_pixels[i + 1] = 18;
            fb_pixels[i + 2] = 26;
            fb_pixels[i + 3] = 255;
        }
    }

    SituationImage feedback_img = {0};
    feedback_img.width = sw;
    feedback_img.height = sh;
    feedback_img.channels = 4;
    feedback_img.data = fb_pixels;
    SituationTextureUsageFlags fb_flags =
        SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_DST;
    SituationError fb_err = SituationCreateTextureEx(feedback_img, false, fb_flags, &g_feedback_tex);
    free(fb_pixels);
    if (fb_err == SITUATION_SUCCESS) {
        g_feedback_tex_ok = 1;
        g_feedback_tex_w = sw;
        g_feedback_tex_h = sh;
    }
}

/* When linked with -mwindows, stderr is invisible; append errors here and show status on HUD. */
static void sky_append_log(const char* line) {
    FILE* f = fopen("demon_hunt_sky.log", "a");
    if (!f) {
        return;
    }
    fputs(line, f);
    fputc('\n', f);
    fflush(f);
    fclose(f);
}

static char* read_file_to_string(const char* filename) {
    const char* paths[] = {
        filename,
        "examples/",
        "../../examples/"
    };
    
    FILE* f = NULL;
    for (int i = 0; i < 3; i++) {
        char full_path[512];
        if (i == 0) {
            snprintf(full_path, sizeof(full_path), "%s", filename);
        } else {
            snprintf(full_path, sizeof(full_path), "%s%s", paths[i], filename);
        }
        f = fopen(full_path, "rb");
        if (f) break;
    }
    
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    return buf;
}

static char* read_file_path(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    return buf;
}

static int resolve_shader_pair_paths(
    const char* vs_name, const char* fs_name,
    char* vs_out, size_t vs_cap, char* fs_out, size_t fs_cap) {
    const char* prefixes[] = { "", "examples/", "../../examples/" };
    for (int i = 0; i < 3; i++) {
        snprintf(vs_out, vs_cap, "%s%s", prefixes[i], vs_name);
        snprintf(fs_out, fs_cap, "%s%s", prefixes[i], fs_name);
        FILE* f = fopen(vs_out, "rb");
        if (!f) {
            continue;
        }
        fclose(f);
        f = fopen(fs_out, "rb");
        if (!f) {
            continue;
        }
        fclose(f);
        return 1;
    }
    return 0;
}

static int init_sky_gpu(void);

static int sky_shader_program_ready(void) {
    if (g_sky_shader.generation == 0) {
        return 0;
    }
    return SituationPollShaderLoad(g_sky_shader) == SITUATION_SUCCESS;
}

static void sky_log_situation_error(const char* prefix, SituationError err) {
    char buf[512];
    const char* code_name = SituationErrorToString(err);
    char* emsg = NULL;
    snprintf(
        buf, sizeof(buf), "%s code=%d (%s)",
        prefix, (int)err, code_name ? code_name : "?");
    sky_append_log(buf);
    if (SituationGetLastErrorMsg(&emsg) == SITUATION_SUCCESS && emsg && emsg[0]) {
        sky_append_log("[demon_hunt] driver log:");
        sky_append_log(emsg);
    }
    SituationFreeString(emsg);
}

static void sky_log_driver_error(const char* prefix) {
    sky_log_situation_error(prefix, SituationGetLastErrorCode());
}

static int sky_try_embedded_spirv_load(void) {
#if defined(SITUATION_USE_VULKAN)
    const unsigned char* vs_spv = demon_hunt_sky_vs_spv_vk;
    size_t vs_len = demon_hunt_sky_vs_spv_vk_len;
    const unsigned char* fs_spv = demon_hunt_sky_fs_spv_vk;
    size_t fs_len = demon_hunt_sky_fs_spv_vk_len;
    const char* backend_label = "Vulkan";
#else
    const unsigned char* vs_spv = demon_hunt_sky_vs_spv;
    size_t vs_len = demon_hunt_sky_vs_spv_len;
    const unsigned char* fs_spv = demon_hunt_sky_fs_spv;
    size_t fs_len = demon_hunt_sky_fs_spv_len;
    const char* backend_label = "OpenGL";
#endif

    if (vs_len == 0 || fs_len == 0) {
        char buf[192];
        snprintf(
            buf, sizeof(buf),
            "[demon_hunt] embedded %s SPIR-V missing (vs=%zu fs=%zu bytes) — run compile_demon_hunt_shaders.bat",
            backend_label, vs_len, fs_len);
        sky_append_log(buf);
        return 0;
    }
    {
        char buf[160];
        snprintf(
            buf, sizeof(buf),
            "[demon_hunt] loading embedded %s SPIR-V (vs=%zu fs=%zu bytes, no runtime GLSL).",
            backend_label, vs_len, fs_len);
        sky_append_log(buf);
    }
#if defined(SITUATION_USE_VULKAN)
    SituationError err = SituationBeginLoadShaderFromSpirvMemoryEx(
        vs_spv, vs_len, fs_spv, fs_len, SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER, &g_sky_shader);
#else
    SituationError err = SituationBeginLoadShaderFromSpirvMemory(
        vs_spv, vs_len, fs_spv, fs_len, &g_sky_shader);
#endif
    if (err != SITUATION_SUCCESS) {
        sky_append_log("[demon_hunt] embedded SPIR-V kickoff failed.");
        sky_log_situation_error("[demon_hunt] SPIR-V kickoff:", err);
        SituationUnloadShader(&g_sky_shader);
        memset(&g_sky_shader, 0, sizeof(g_sky_shader));
        return 0;
    }
    sky_append_log("[demon_hunt] embedded SPIR-V kickoff OK (specialize/link polled per frame).");
    g_sky_load_log_time = 0.0; /* force first sky_poll to log immediately */
    return 1;
}

static int sky_try_async_glsl_load(void) {
    char vs_path[512];
    char fs_path[512];
    if (!resolve_shader_pair_paths(
            "demon_hunt_sky.vs", "demon_hunt_sky.fs",
            vs_path, sizeof(vs_path), fs_path, sizeof(fs_path))) {
        sky_append_log(
            "[demon_hunt] no demon_hunt_sky.{vs,fs} beside exe — rebuild with "
            "compile_demon_hunt_shaders.bat or use embedded SPIR-V build.");
        return 0;
    }

    char* vs_src = read_file_path(vs_path);
    char* fs_src = read_file_path(fs_path);
    if (!vs_src || !fs_src) {
        sky_append_log("[demon_hunt] failed to read GLSL shader sources.");
        free(vs_src);
        free(fs_src);
        return 0;
    }

    sky_append_log("[demon_hunt] async GLSL world shader load started (fallback; may OOM on link).");
    SituationError err = SituationBeginLoadShaderFromMemory(vs_src, fs_src, &g_sky_shader);
    free(vs_src);
    free(fs_src);
    if (err != SITUATION_SUCCESS) {
        sky_append_log("[demon_hunt] async GLSL load kickoff failed.");
        sky_log_situation_error("[demon_hunt] GLSL kickoff:", err);
        SituationUnloadShader(&g_sky_shader);
        memset(&g_sky_shader, 0, sizeof(g_sky_shader));
        return 0;
    }
    return 1;
}

static void sky_begin_async_shader_load(void) {
    if (g_sky_ok || g_sky_async_started) {
        return;
    }
    g_sky_async_started = 1;

    if (sky_try_embedded_spirv_load()) {
        return;
    }
    if (sky_try_async_glsl_load()) {
        return;
    }
    g_sky_async_failed = 1;
}

static void sky_poll_async_shader(void) {
    if (g_sky_ok || g_sky_async_failed) {
        return;
    }
    if (g_sky_shader.generation == 0) {
        sky_append_log("[demon_hunt] sky_poll: shader handle invalid (generation=0).");
        return;
    }
    SituationError st = SituationPollShaderLoad(g_sky_shader);
    if (st == SITUATION_ERROR_RESOURCE_INVALID) {
        sky_append_log("[demon_hunt] sky_poll: SituationPollShaderLoad RESOURCE_INVALID (stale handle?).");
        return;
    }
    if (st == SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS) {
        double now = SituationTimerGetTime();
        if (g_sky_load_log_time <= 0.0 || (now - g_sky_load_log_time) >= 2.0) {
            sky_append_log(
                "[demon_hunt] SPIR-V still loading (VS specialize -> FS specialize -> link; FS ~1.3MB may take minutes)...");
            g_sky_load_log_time = now;
        }
        return;
    }
    if (st != SITUATION_SUCCESS) {
        sky_log_situation_error("[demon_hunt] world shader poll failed:", st);
        g_sky_async_failed = 1;
        return;
    }
    sky_append_log("[demon_hunt] SPIR-V world shader linked; finishing GPU init.");
    if (!init_sky_gpu()) {
        g_sky_async_failed = 1;
    }
}

static void sky_pack_wall_rows(int rows[MAP_MAX_H]) {
    memset(rows, 0, (size_t)MAP_MAX_H * sizeof(rows[0]));
    for (int z = 0; z < g_map_h && z < MAP_MAX_H; z++) {
        int bits = 0;
        for (int x = 0; x < g_map_w && x < 32; x++) {
            if (g_map[z][x] == CELL_WALL) {
                bits |= (1 << x);
            }
        }
        rows[z] = bits;
    }
}

static void sky_pack_arch_rows(int ns_rows[MAP_MAX_H], int ew_rows[MAP_MAX_H]) {
    memset(ns_rows, 0, (size_t)MAP_MAX_H * sizeof(ns_rows[0]));
    memset(ew_rows, 0, (size_t)MAP_MAX_H * sizeof(ew_rows[0]));
    for (int z = 0; z < g_map_h && z < MAP_MAX_H; z++) {
        int ns_bits = 0;
        int ew_bits = 0;
        for (int x = 0; x < g_map_w && x < 32; x++) {
            if (g_map[z][x] == CELL_ARCH_NS) {
                ns_bits |= (1 << x);
            } else if (g_map[z][x] == CELL_ARCH_EW) {
                ew_bits |= (1 << x);
            }
        }
        ns_rows[z] = ns_bits;
        ew_rows[z] = ew_bits;
    }
}

static void sky_pack_material_rows(int mat_rows[128]) {
    memset(mat_rows, 0, 128 * sizeof(int));
    for (int z = 0; z < g_map_h && z < MAP_MAX_H; z++) {
        for (int x = 0; x < g_map_w && x < 32; x++) {
            int idx = z * 4 + x / 8;
            int shift = (x % 8) * 4;
            mat_rows[idx] |= ((int)g_material_map[z][x] & 0xF) << shift;
        }
    }
}

static int shader_sprite_runtime_enabled(void) {
    return g_sky_ok && g_scene_ssbo_ok;
}

static int shader_sprite_phase3_runtime_enabled(void) {
    return shader_sprite_runtime_enabled() && SHADER_SPRITE_PHASE3_AVAILABLE;
}

static int sky_shader_has_core_uniforms(void);

static int init_sky_gpu(void) {
    if (g_sky_ok) {
        return 1;
    }
    sky_append_log("[demon_hunt] init_sky_gpu: finish GPU resources.");

    if (!sky_shader_program_ready()) {
        sky_append_log("[demon_hunt] init_sky_gpu: shader program not ready.");
        return 0;
    }

    if (g_sky_shader.generation == 0) {
        sky_append_log("[demon_hunt] init_sky_gpu: SPIR-V reported OK but shader handle is invalid.");
        return 0;
    }

    sky_append_log("[demon_hunt] init_sky_gpu: validating loose uniforms.");
    if (!sky_shader_has_core_uniforms()) {
        sky_append_log("[demon_hunt] init_sky_gpu: uniform validation failed.");
        SituationUnloadShader(&g_sky_shader);
        memset(&g_sky_shader, 0, sizeof(g_sky_shader));
        return 0;
    }

#if defined(SITUATION_USE_OPENGL)
    sky_append_log("[demon_hunt] init_sky_gpu: binding SkyFrame UBO block.");
    if (SituationBindUniformBlock(g_sky_shader, "SkyFrame", 0u) != SITUATION_SUCCESS) {
        sky_append_log("[demon_hunt] init_sky_gpu: SkyFrame UBO block bind to 0 failed.");
        SituationUnloadShader(&g_sky_shader);
        memset(&g_sky_shader, 0, sizeof(g_sky_shader));
        return 0;
    }

    /* SPIR-V reflection often reports SSBO binding 0; demon_hunt_sky.fs uses layout(binding = 1). */
    if (SituationBindShaderStorageBlock(g_sky_shader, "ShaderScenePack", 1u) != SITUATION_SUCCESS) {
        sky_append_log("[demon_hunt] init_sky_gpu: ShaderScenePack block bind to 1 failed.");
        SituationUnloadShader(&g_sky_shader);
        memset(&g_sky_shader, 0, sizeof(g_sky_shader));
        return 0;
    }
#else
    sky_append_log("[demon_hunt] init_sky_gpu: Vulkan UBO_SSBO profile (sets 0/1 via CmdBindDescriptorSet).");
#endif

    {
        SituationError ubo_err = SituationCreateBuffer(
            DEMON_HUNT_SKY_FRAME_UBO_BYTES,
            NULL,
            SITUATION_BUFFER_USAGE_UNIFORM_BUFFER,
            &g_sky_frame_ubo);
        if (ubo_err != SITUATION_SUCCESS) {
            sky_append_log("[demon_hunt] init_sky_gpu: SkyFrame UBO creation failed.");
            SituationUnloadShader(&g_sky_shader);
            memset(&g_sky_shader, 0, sizeof(g_sky_shader));
            return 0;
        }
        g_sky_frame_ubo_ok = 1;
        sky_append_log("[demon_hunt] init_sky_gpu: SkyFrame UBO OK.");
    }

    {
        SituationError ssbo_err = SituationCreateBuffer(
            SKY_SCENE_SSBO_BYTES,
            NULL,
            SITUATION_BUFFER_USAGE_STORAGE_BUFFER,
            &g_scene_ssbo);
        if (ssbo_err != SITUATION_SUCCESS) {
            char* msg = NULL;
            if (SituationGetLastErrorMsg(&msg) == SITUATION_SUCCESS && msg) {
                char buf[640];
                snprintf(buf, sizeof(buf),
                         "[demon_hunt] init_sky_gpu: scene SSBO failed (%zu bytes): %.450s",
                         SKY_SCENE_SSBO_BYTES, msg);
                sky_append_log(buf);
                SituationFreeString(msg);
            } else {
                sky_append_log("[demon_hunt] init_sky_gpu: scene SSBO creation failed.");
            }
            SituationUnloadShader(&g_sky_shader);
            memset(&g_sky_shader, 0, sizeof(g_sky_shader));
            g_sky_ok = 0;
            return 0;
        }
        g_scene_ssbo_ok = 1;
        sky_append_log("[demon_hunt] init_sky_gpu: scene SSBO OK.");
    }

    sky_append_log("[demon_hunt] init_sky_gpu: creating fullscreen mesh.");
    SkyVtx tri[3] = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{3.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-1.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
    };
    /* Indices 0,1,2 match shader_lab_torus fullscreen tri: CCW in clip space so GL_BACK cull keeps the front face.
     * Order 0,2,1 was incorrectly CW and the whole pass was culled (no sky, no shader floor/shadow). */
    uint32_t ix[3] = {0, 1, 2};
    SituationError err = SituationCreateMesh(tri, 3, sizeof(SkyVtx), ix, 3, &g_sky_mesh);
    if (err != SITUATION_SUCCESS) {
        char* msg = NULL;
        if (SituationGetLastErrorMsg(&msg) == SITUATION_SUCCESS && msg) {
            fprintf(stderr, "[demon_hunt] skydome mesh failed: %s\n", msg);
            {
                char buf[640];
                snprintf(buf, sizeof(buf), "[demon_hunt] skydome mesh failed: %.500s", msg);
                sky_append_log(buf);
            }
            SituationFreeString(msg);
        } else {
            sky_append_log("[demon_hunt] skydome mesh failed (no Situation error string).");
        }
        if (g_scene_ssbo_ok) {
            SituationDestroyBuffer(&g_scene_ssbo);
            memset(&g_scene_ssbo, 0, sizeof(g_scene_ssbo));
            g_scene_ssbo_ok = 0;
        }
        if (g_sky_frame_ubo_ok) {
            SituationDestroyBuffer(&g_sky_frame_ubo);
            memset(&g_sky_frame_ubo, 0, sizeof(g_sky_frame_ubo));
            g_sky_frame_ubo_ok = 0;
        }
        SituationUnloadShader(&g_sky_shader);
        memset(&g_sky_shader, 0, sizeof(g_sky_shader));
        g_sky_ok = 0;
        return 0;
    }
    g_sky_ok = 1;
    sky_append_log("[demon_hunt] skydome GPU path OK (SkyFrame UBO + scene SSBO + mesh).");

    /* Phase 1.5: Create previous-frame feedback texture (SAMPLED + TRANSFER_DST) */
    {
        int fb_w = GAME_RENDER_W;
        int fb_h = GAME_RENDER_H;

        /* Allocate a CPU image filled with fog color so frame 0 reads sane data (step 1.5.6) */
        size_t fb_bytes = (size_t)fb_w * (size_t)fb_h * 4;
        uint8_t* fb_pixels = (uint8_t*)malloc(fb_bytes);
        if (fb_pixels) {
            /* FOG_COLOR = vec3(0.08, 0.07, 0.10) → RGBA8 (20, 18, 26, 255) */
            for (size_t i = 0; i < fb_bytes; i += 4) {
                fb_pixels[i + 0] = 20;
                fb_pixels[i + 1] = 18;
                fb_pixels[i + 2] = 26;
                fb_pixels[i + 3] = 255;
            }
        }

        SituationImage feedback_img = {0};
        feedback_img.width = fb_w;
        feedback_img.height = fb_h;
        feedback_img.channels = 4;
        feedback_img.data = fb_pixels; /* NULL-safe: CreateTextureEx accepts NULL for undefined */
        SituationTextureUsageFlags fb_flags =
            SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_DST;
        SituationError fb_err = SituationCreateTextureEx(feedback_img, false, fb_flags, &g_feedback_tex);
        free(fb_pixels);
        if (fb_err == SITUATION_SUCCESS) {
            g_feedback_tex_ok = 1;
            g_feedback_tex_w = fb_w;
            g_feedback_tex_h = fb_h;
            sky_append_log("[demon_hunt] feedback texture created OK (filled with fog color).");
        } else {
            g_feedback_tex_ok = 0;
            sky_append_log("[demon_hunt] feedback texture creation failed (non-fatal).");
        }
    }

    return 1;
}

static int sky_shader_has_core_uniforms(void) {
    /* Skydome uses SkyFrame UBO + ShaderScenePack SSBO only (no layout(location) loose uniforms). */
    (void)g_sky_shader;
    return 1;
}

static void shutdown_sky_gpu(void) {
    /* Phase 1.5: feedback texture cleanup */
    if (g_feedback_tex_ok) {
        SituationDestroyTexture(&g_feedback_tex);
        memset(&g_feedback_tex, 0, sizeof(g_feedback_tex));
        g_feedback_tex_ok = 0;
    }

    if (g_scene_ssbo_ok) {
        SituationDestroyBuffer(&g_scene_ssbo);
        memset(&g_scene_ssbo, 0, sizeof(g_scene_ssbo));
        g_scene_ssbo_ok = 0;
    }
    if (g_sky_frame_ubo_ok) {
        SituationDestroyBuffer(&g_sky_frame_ubo);
        memset(&g_sky_frame_ubo, 0, sizeof(g_sky_frame_ubo));
        g_sky_frame_ubo_ok = 0;
    }
    if (!g_sky_ok) {
        return;
    }
    SituationDestroyMesh(&g_sky_mesh);
    memset(&g_sky_mesh, 0, sizeof(g_sky_mesh));
    SituationUnloadShader(&g_sky_shader);
    memset(&g_sky_shader, 0, sizeof(g_sky_shader));
    g_sky_ok = 0;
    g_sky_async_started = 0;
    g_sky_async_failed = 0;
    g_sky_load_log_time = 0.0;
}

static void upload_scene_ssbo(const int rows[MAP_MAX_H], const int arch_ns_rows[MAP_MAX_H], const int arch_ew_rows[MAP_MAX_H]) {
    SkySceneSsboHeader* hdr = (SkySceneSsboHeader*)g_scene_ssbo_cpu;
    float* sprite_vecs = (float*)(g_scene_ssbo_cpu + SKY_SCENE_SSBO_HEADER_BYTES);

    hdr->map_size[0] = g_map_w;
    hdr->map_size[1] = g_map_h;
    hdr->align_pad[0] = 0;
    hdr->align_pad[1] = 0;
    memcpy(hdr->wall_rows, rows, sizeof(hdr->wall_rows));
    memcpy(hdr->arch_ns_rows, arch_ns_rows, sizeof(hdr->arch_ns_rows));
    memcpy(hdr->arch_ew_rows, arch_ew_rows, sizeof(hdr->arch_ew_rows));
    sky_pack_material_rows(hdr->material_rows);

    for (int i = 0; i < TELEPORTER_MAX_COUNT; i++) {
        hdr->teleporters[i][0] = g_teleporters[i].x;
        hdr->teleporters[i][1] = g_teleporters[i].z;
        hdr->teleporters[i][2] = (float)g_teleporters[i].active;
        hdr->teleporters[i][3] = 0.0f;
    }
    for (int i = 0; i < HELLRAISER_COUNT; i++) {
        hdr->hellraisers[i][0] = g_hellraisers[i].x;
        hdr->hellraisers[i][1] = 0.58f;
        hdr->hellraisers[i][2] = g_hellraisers[i].z;
        hdr->hellraisers[i][3] = (float)g_hellraisers[i].active;
    }
    for (int i = 0; i < PLAYER_SHOT_COUNT; i++) {
        hdr->player_shots[i][0] = g_player_shots[i].x;
        hdr->player_shots[i][1] = g_player_shots[i].y;
        hdr->player_shots[i][2] = g_player_shots[i].z;
        hdr->player_shots[i][3] = (float)g_player_shots[i].active;
    }
    for (int i = 0; i < DRONE_SHOT_COUNT; i++) {
        hdr->drone_shots[i][0] = g_drone_shots[i].x;
        hdr->drone_shots[i][1] = g_drone_shots[i].y;
        hdr->drone_shots[i][2] = g_drone_shots[i].z;
        hdr->drone_shots[i][3] = (float)g_drone_shots[i].active;
        hdr->drone_shot_dirs[i][0] = g_drone_shots[i].dir_x;
        hdr->drone_shot_dirs[i][1] = g_drone_shots[i].dir_y;
        hdr->drone_shot_dirs[i][2] = g_drone_shots[i].dir_z;
        hdr->drone_shot_dirs[i][3] = g_drone_shots[i].travel;
    }

    memset(sprite_vecs, 0, SKY_SCENE_SSBO_SPRITE_BYTES);
    for (int i = 0; i < g_shader_sprite_count; i++) {
        memcpy(sprite_vecs + i * 4, g_shader_sprites0[i], sizeof(float) * 4);
        memcpy(sprite_vecs + (SHADER_SPRITE_GPU_MAX + i) * 4, g_shader_sprites1[i], sizeof(float) * 4);
        memcpy(sprite_vecs + (SHADER_SPRITE_GPU_MAX * 2 + i) * 4, g_shader_sprites2[i], sizeof(float) * 4);
    }
    {
        SituationError uerr = SituationUpdateBuffer(g_scene_ssbo, 0, SKY_SCENE_SSBO_BYTES, g_scene_ssbo_cpu);
        if (uerr != SITUATION_SUCCESS) {
            fprintf(
                stderr, "[demon_hunt] scene SSBO upload failed: %d (%s)\n",
                (int)uerr, SituationErrorToString(uerr) ? SituationErrorToString(uerr) : "?");
        }
    }
}

static void upload_sky_frame_ubo(int sw, int sh, float h_line, float lit_x, float lit_y, float lit_z) {
    if (!g_sky_frame_ubo_ok) {
        return;
    }

    DemonHuntSkyFrameUbo ubo;
    memset(&ubo, 0, sizeof(ubo));
    ubo.u_time = (float)SituationTimerGetTime();
    ubo.u_bob_phase = g_bob;
    ubo.u_threat_pulse = SituationTimerGetOscillatorState(HELLRAISER_OSC_ID) ? 1.0f : 0.0f;
    ubo.u_music_pulse = SituationTimerGetOscillatorState(MELO_OSC_ID) ? 1.0f : 0.0f;
    ubo.u_yaw = g_yaw;
    ubo.u_horizon_px_from_top = h_line;
    ubo.u_horizon_shift_px = compute_horizon_shift_px((float)sh);
    ubo.u_tan_half_fov = tanf((float)(FOV_DEG * (M_PI / 180.0) * 0.5));
    ubo.u_resolution[0] = (float)sw;
    ubo.u_resolution[1] = (float)sh;
    ubo.u_cam_pos[0] = g_px;
    ubo.u_cam_pos[1] = g_eye_y;
    ubo.u_cam_pos[2] = g_pz;
    ubo.u_sun_dir[0] = lit_x;
    ubo.u_sun_dir[1] = lit_y;
    ubo.u_sun_dir[2] = lit_z;
    ubo.u_map_size[0] = (float)g_map_w;
    ubo.u_map_size[1] = (float)g_map_h;
    ubo.u_sprite_count = g_shader_sprite_count;
    ubo.u_sprite_debug_mode = g_shader_sprite_debug_mode;
    ubo.u_shader_sprites_enabled = shader_sprite_runtime_enabled();
    ubo.u_pain_flash = g_pain_flash;

    {
        SituationCameraDesc flat_desc;
        mat4 flat_inv_vp;
        build_level_wall_camera(&flat_desc, flat_inv_vp, (float)sw / (float)sh);
        memcpy(ubo.u_flat_inv_vp, flat_inv_vp, sizeof(ubo.u_flat_inv_vp));
    }

    {
        SituationError uerr = SituationUpdateBuffer(g_sky_frame_ubo, 0, DEMON_HUNT_SKY_FRAME_UBO_BYTES, &ubo);
        if (uerr != SITUATION_SUCCESS) {
            fprintf(
                stderr, "[demon_hunt] SkyFrame UBO upload failed: %d (%s)\n",
                (int)uerr, SituationErrorToString(uerr) ? SituationErrorToString(uerr) : "?");
        }
    }
}

static void upload_shader_sprite_uniforms(void) {
    (void)0;
}

/* CPU column raycast when the unified world shader fails to link (Step E fallback). */
static void sky_draw_cpu_world_fallback(SituationCommandBuffer cmd, int sw, int sh, float h_line,
    float lit_x, float lit_y, float lit_z) {
    float fx = sinf(g_yaw);
    float fz = cosf(g_yaw);
    float half_fov = FOV_DEG * (float)(M_PI / 180.0) * 0.5f;
    float sky_h = h_line;
    if (sky_h < 0.0f) {
        sky_h = 0.0f;
    }
    if (sky_h > (float)sh) {
        sky_h = (float)sh;
    }
    draw_rect_px(cmd, 0.0f, 0.0f, (float)sw, sky_h, (Vector4){{0.14f, 0.06f, 0.20f, 1.0f}});
    draw_rect_px(cmd, 0.0f, sky_h, (float)sw, (float)sh - sky_h, (Vector4){{0.07f, 0.06f, 0.08f, 1.0f}});

    for (int x = 0; x < sw; x++) {
        float cam_x = (((float)x + 0.5f) / (float)sw) * 2.0f - 1.0f;
        float ray_angle = g_yaw + atanf(cam_x * tanf(half_fov));
        float rdx = sinf(ray_angle);
        float rdz = cosf(ray_angle);
        RayHit hit;
        cast_ray(g_px, g_pz, rdx, rdz, fx, fz, &hit);
        if (hit.cell != CELL_WALL) {
            continue;
        }

        float line_h = (float)sh / hit.dist;
        float mid = h_line;
        float wtop = mid - line_h * (1.0f - g_eye_y);
        float wbot = mid + line_h * g_eye_y;
        if (wtop < 0.0f) {
            wtop = 0.0f;
        }
        if (wbot > (float)sh) {
            wbot = (float)sh;
        }
        if (wbot <= wtop + 0.5f) {
            continue;
        }

        float wp_x = g_px + rdx * hit.along;
        float wp_z = g_pz + rdz * hit.along;
        int sl = sun_ray_lit(wp_x, 0.5f, wp_z, lit_x, lit_y, lit_z);
        float wall_n = (hit.side == 0) ? fabsf(rdx) : fabsf(rdz);
        float att = 1.0f / (1.0f + hit.dist * 0.11f);
        float diffuse = 0.78f * wall_n * att * (float)sl;
        float lum = 0.22f * att + diffuse;
        if (lum < 0.05f) {
            lum = 0.05f;
        }
        if (lum > 1.35f) {
            lum = 1.35f;
        }
        if (hit.side != 0) {
            lum *= 0.90f;
        }
        Vector4 wcol = {{0.55f * lum, 0.18f * lum, 0.12f * lum, 1.0f}};
        draw_rect_px(cmd, (float)x, wtop, 1.0f, wbot - wtop, wcol);
    }
}

static void sky_draw_fullscreen(SituationCommandBuffer cmd, int sw, int sh, float h_line, float lit_x, float lit_y, float lit_z) {
    if (!g_sky_ok) {
        sky_draw_cpu_world_fallback(cmd, sw, sh, h_line, lit_x, lit_y, lit_z);
        return;
    }
    int rows[MAP_MAX_H];
    int arch_ns_rows[MAP_MAX_H];
    int arch_ew_rows[MAP_MAX_H];
    sky_pack_wall_rows(rows);
    sky_pack_arch_rows(arch_ns_rows, arch_ew_rows);
    SituationCmdBindPipeline(cmd, g_sky_shader);
    upload_sky_frame_ubo(sw, sh, h_line, lit_x, lit_y, lit_z);
    if (g_sky_frame_ubo_ok) {
        SituationCmdBindDescriptorSet(cmd, 0, g_sky_frame_ubo);
    }
    if (g_scene_ssbo_ok) {
        upload_scene_ssbo(rows, arch_ns_rows, arch_ew_rows);
        SituationCmdBindDescriptorSet(cmd, 1, g_scene_ssbo);
    }
    /* Phase 1.5: Bind previous-frame feedback texture at set 2 (UBO_SSBO_SAMPLER profile). */
    if (g_feedback_tex_ok) {
        SituationCmdBindTextureSet(cmd, 2, g_feedback_tex);
    }
    SituationCmdDrawMesh(cmd, g_sky_mesh);
}

static float rel_angle_to_forward(float dx, float dz) {
    float fx = sinf(g_yaw);
    float fz = cosf(g_yaw);
    return atan2f(fx * dz - fz * dx, fx * dx + fz * dz);
}

static int los_between(float ox, float oz, float tx, float tz) {
    float dx = tx - ox;
    float dz = tz - oz;
    float len = sqrtf(dx * dx + dz * dz);
    if (len < 0.05f) {
        return 1;
    }
    dx /= len;
    dz /= len;

    RayHit hit;
    cast_ray(ox, oz, dx, dz, dx, dz, &hit);
    return hit.along + 0.02f >= len;
}

static int los_to_point(float tx, float tz) {
    return los_between(g_px, g_pz, tx, tz);
}

static void try_move(float mdx, float mdz) {
    if (circle_hits_wall(g_px + mdx, g_pz)) {
        mdx = 0.0f;
    }
    if (circle_hits_wall(g_px, g_pz + mdz)) {
        mdz = 0.0f;
    }
    g_px += mdx;
    g_pz += mdz;
}

static void demon_try_move(Demon* d, float mdx, float mdz) {
    if (!circle_hits_wall_r(d->x + mdx, d->z, DEMON_RADIUS)) {
        d->x += mdx;
    }
    if (!circle_hits_wall_r(d->x, d->z + mdz, DEMON_RADIUS)) {
        d->z += mdz;
    }
}

static void hunter_drone_try_move(HunterDrone* d, float mdx, float mdz) {
    if (!circle_hits_wall_r(d->x + mdx, d->z, DEMON_RADIUS * 0.9f)) {
        d->x += mdx;
    }
    if (!circle_hits_wall_r(d->x, d->z + mdz, DEMON_RADIUS * 0.9f)) {
        d->z += mdz;
    }
}

static void hellraiser_try_move(Hellraiser* h, float mdx, float mdz) {
    if (!circle_hits_wall_r(h->x + mdx, h->z, DEMON_RADIUS * 1.35f)) {
        h->x += mdx;
    }
    if (!circle_hits_wall_r(h->x, h->z + mdz, DEMON_RADIUS * 1.35f)) {
        h->z += mdz;
    }
}

static void hellraiser_pathfind_move(Hellraiser* h, float dt) {
    int sx = (int)floorf(h->x);
    int sz = (int)floorf(h->z);
    int tx = (int)floorf(g_px);
    int tz = (int)floorf(g_pz);
    if (map_solid(sx, sz) || map_solid(tx, tz)) {
        return;
    }

    int dist[MAP_MAX_H][MAP_MAX_W];
    int qx[MAP_MAX_W * MAP_MAX_H];
    int qz[MAP_MAX_W * MAP_MAX_H];
    for (int z = 0; z < MAP_MAX_H; z++) {
        for (int x = 0; x < MAP_MAX_W; x++) {
            dist[z][x] = -1;
        }
    }

    int head = 0;
    int tail = 0;
    dist[tz][tx] = 0;
    qx[tail] = tx;
    qz[tail] = tz;
    tail++;

    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (head < tail) {
        int cx = qx[head];
        int cz = qz[head];
        head++;
        for (int d = 0; d < 4; d++) {
            int nx = cx + dirs[d][0];
            int nz = cz + dirs[d][1];
            if (map_solid(nx, nz) || dist[nz][nx] >= 0) {
                continue;
            }
            dist[nz][nx] = dist[cz][cx] + 1;
            qx[tail] = nx;
            qz[tail] = nz;
            tail++;
        }
    }

    int best_x = sx;
    int best_z = sz;
    int best_d = dist[sz][sx];
    for (int d = 0; d < 4; d++) {
        int nx = sx + dirs[d][0];
        int nz = sz + dirs[d][1];
        if (map_solid(nx, nz) || dist[nz][nx] < 0) {
            continue;
        }
        if (best_d < 0 || dist[nz][nx] < best_d) {
            best_d = dist[nz][nx];
            best_x = nx;
            best_z = nz;
        }
    }

    float target_x = (best_x == tx && best_z == tz) ? g_px : (float)best_x + 0.5f;
    float target_z = (best_x == tx && best_z == tz) ? g_pz : (float)best_z + 0.5f;
    float dx = target_x - h->x;
    float dz = target_z - h->z;
    float len = sqrtf(dx * dx + dz * dz);
    if (len < 0.05f && !(best_x == tx && best_z == tz)) {
        target_x = g_px;
        target_z = g_pz;
        dx = target_x - h->x;
        dz = target_z - h->z;
        len = sqrtf(dx * dx + dz * dz);
    }
    if (len > 0.01f) {
        float speed = (HELLRAISER_SPEED + (float)(g_spawn_number - 1) * 0.16f) * dt;
        if (speed > len) {
            speed = len;
        }
        hellraiser_try_move(h, (dx / len) * speed, (dz / len) * speed);
    }
}

static void hellraiser_spawn(Hellraiser* h) {
    float best_x = g_px;
    float best_z = g_pz;
    float best_d2 = -1.0f;
    for (int i = 0; i < 18; i++) {
        float sx, sz;
        get_random_empty_cell(&sx, &sz);
        float dx = sx - g_px;
        float dz = sz - g_pz;
        float d2 = dx * dx + dz * dz;
        if (d2 > best_d2) {
            best_d2 = d2;
            best_x = sx;
            best_z = sz;
        }
    }

    h->x = best_x;
    h->z = best_z;
    h->active = 1;
    h->warned = 1;
    h->teleport_cooldown = 0.0f;
    h->spawn_freeze = HELLRAISER_FREEZE_TIME;
    play_sfx_tone(SIT_WAVE_SAW, 32.0f, 0.95f, 0.0f, 0.015f, 0.34f, 0.18f, 0.85f, 0.12f);
    play_sfx_tone(SIT_WAVE_SQUARE, 47.0f, 0.65f, -0.12f, 0.006f, 0.24f, 0.12f, 0.72f, 0.08f);
    play_sfx_tone(SIT_WAVE_NOISE, 95.0f, 0.55f, 0.14f, 0.002f, 0.32f, 0.0f, 0.62f, 0.08f);
}

static int exit_gate_open(void);

static void update_play(float dt) {
    if (dt > 0.08f) {
        dt = 0.08f;
    }
    if (dt <= 0.0f) {
        dt = 1.0f / 60.0f;
    }
    g_spawn_elapsed += dt;

    Vector2 md = SituationGetMouseDelta();
    g_yaw += md.x * MOUSE_SENS;
    g_pitch -= md.y * MOUSE_SENS_Y;
    if (g_pitch > PITCH_LIM) {
        g_pitch = PITCH_LIM;
    }
    if (g_pitch < -PITCH_LIM) {
        g_pitch = -PITCH_LIM;
    }

    float fx = sinf(g_yaw);
    float fz = cosf(g_yaw);
    float rx = cosf(g_yaw);
    float rz = -sinf(g_yaw);

    float wish_f = 0.0f;
    float wish_s = 0.0f;
    if (SituationIsKeyDown(SIT_KEY_W) || SituationIsKeyDown(SIT_KEY_UP)) {
        wish_f += 1.0f;
    }
    if (SituationIsKeyDown(SIT_KEY_S) || SituationIsKeyDown(SIT_KEY_DOWN)) {
        wish_f -= 1.0f;
    }
    if (SituationIsKeyDown(SIT_KEY_A)) {
        wish_s -= 1.0f;
    }
    if (SituationIsKeyDown(SIT_KEY_D)) {
        wish_s += 1.0f;
    }
    if (SituationIsKeyDown(SIT_KEY_LEFT)) {
        wish_s -= 1.0f;
    }
    if (SituationIsKeyDown(SIT_KEY_RIGHT)) {
        wish_s += 1.0f;
    }

    if (g_player_grounded && SituationIsKeyPressed(SIT_KEY_SPACE)) {
        g_player_vy = JUMP_VELOCITY;
        g_player_grounded = 0;
        sfx_jump();
    }

    g_player_vy -= GRAVITY * dt;
    g_eye_y += g_player_vy * dt;
    if (g_eye_y <= PLAYER_EYE_GROUND_Y) {
        g_eye_y = PLAYER_EYE_GROUND_Y;
        g_player_vy = 0.0f;
        g_player_grounded = 1;
    }

    int running = SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT);
    float move_speed = running ? RUN_SPEED : WALK_SPEED;
    float mdx;
    float mdz;
    if (g_player_grounded) {
        g_player_vx = (fx * wish_f + rx * wish_s) * move_speed;
        g_player_vz = (fz * wish_f + rz * wish_s) * move_speed;
        mdx = g_player_vx * dt;
        mdz = g_player_vz * dt;
    } else {
        mdx = g_player_vx * dt;
        mdz = g_player_vz * dt;
    }
    try_move(mdx, mdz);

    float old_bob = g_bob;
    int moving_on_ground = g_player_grounded && (fabsf(g_player_vx) + fabsf(g_player_vz) > 0.05f);
    if (moving_on_ground) {
        g_bob += dt * (running ? 18.0f : 10.5f);
    } else if (g_player_grounded) {
        g_bob += dt * 4.0f;
    }

    if (moving_on_ground && sinf(old_bob) >= 0.0f && sinf(g_bob) < 0.0f) {
        if (running) {
            SituationPlayToneEx(SIT_WAVE_NOISE, 190.0f, 0.12f, 0.0f, 0.001f, 0.018f, 0.0f, 0.05f, 0.012f);
            SituationPlayToneEx(SIT_WAVE_TRIANGLE, 58.0f, 0.035f, 0.0f, 0.001f, 0.025f, 0.0f, 0.045f, 0.008f);
        } else {
            SituationPlayToneEx(SIT_WAVE_NOISE, 125.0f, 0.055f, 0.0f, 0.001f, 0.024f, 0.0f, 0.035f, 0.01f);
        }
    }

    if (g_muzzle > 0) {
        g_muzzle--;
    }
    if (g_pain_flash > 0.0f) {
        g_pain_flash -= dt * 1.8f;
        if (g_pain_flash < 0.0f) {
            g_pain_flash = 0.0f;
        }
    }
    if (g_melee_cooldown > 0.0f) {
        g_melee_cooldown -= dt;
        if (g_melee_cooldown < 0.0f) {
            g_melee_cooldown = 0.0f;
        }
    }

    int fire = SituationIsMouseButtonPressed(0) || SituationIsKeyPressed(SIT_KEY_LEFT_CONTROL);
    if (fire && g_ammo > 0) {
        g_ammo--;
        g_muzzle = 4;
        sfx_shoot();

        int sw = SituationGetRenderWidth();
        int sh = SituationGetRenderHeight();
        SituationCameraDesc cam;
        mat4 inv_vp;
        build_level_wall_camera(&cam, inv_vp, (float)sw / (float)sh);
        float h_shift = compute_horizon_shift_px((float)sh);
        
        Vector2 center_px = {{(float)sw * 0.5f, (float)sh * 0.5f - h_shift}};
        Vector2 res = {{(float)sw, (float)sh}};
        
        Vector3 ray_origin, ray_dir;
        SituationCameraUnprojectPixel(&cam, inv_vp, center_px, res, &ray_origin, &ray_dir);
        
        float rdx = ray_dir.x;
        float rdz = ray_dir.z;
        float rlen = sqrtf(rdx * rdx + rdz * rdz);
        float rdy = 0.0f;
        if (rlen > 1e-5f) {
            rdx /= rlen;
            rdz /= rlen;
            rdy = ray_dir.y / rlen;
        } else {
            rdx = sinf(g_yaw);
            rdz = cosf(g_yaw);
        }
        
        /* Fisheye correction uses horizontal forward (yaw only). */
        RayHit wall_hit;
        cast_ray(g_px, g_pz, rdx, rdz, fx, fz, &wall_hit);
        
        int hit_type = 0;
        int hit_ix = -1;
        float best = wall_hit.along;
        
        for (int i = 0; i < DEMON_COUNT; i++) {
            if (!g_demon[i].alive) {
                continue;
            }
            float dx = g_demon[i].x - g_px;
            float dz = g_demon[i].z - g_pz;
            float t = dx * rdx + dz * rdz;
            if (t < 0.02f || t > best + 0.02f) {
                continue;
            }
            float cpx = g_px + rdx * t;
            float cpz = g_pz + rdz * t;
            float ex = cpx - g_demon[i].x;
            float ez = cpz - g_demon[i].z;
            const float hit_r = 0.52f;
            if (ex * ex + ez * ez < hit_r * hit_r) {
                best = t;
                hit_ix = i;
                hit_type = 1;
            }
        }

        for (int i = 0; i < HUNTER_DRONE_COUNT; i++) {
            HunterDrone* drone = &g_hunter_drones[i];
            if (!drone->active) {
                continue;
            }
            float dx = drone->x - g_px;
            float dz = drone->z - g_pz;
            float t = dx * rdx + dz * rdz;
            if (t < 0.02f || t > best + 0.02f) {
                continue;
            }
            float cpx = g_px + rdx * t;
            float cpz = g_pz + rdz * t;
            float y_at = g_eye_y + rdy * t;
            float ex = cpx - drone->x;
            float ez = cpz - drone->z;
            const float hit_r = 0.42f;
            if (ex * ex + ez * ez < hit_r * hit_r && fabsf(y_at - HUNTER_DRONE_HOVER_Y) < 0.34f) {
                best = t;
                hit_ix = i;
                hit_type = 3;
            }
        }
        
        for (int i = 0; i < PORTAL_COUNT; i++) {
            if (!g_portals[i].alive) {
                continue;
            }
            float dx = g_portals[i].x - g_px;
            float dz = g_portals[i].z - g_pz;
            float t = dx * rdx + dz * rdz;
            if (t < 0.02f || t > best + 0.02f) {
                continue;
            }
            float cpx = g_px + rdx * t;
            float cpz = g_pz + rdz * t;
            float ex = cpx - g_portals[i].x;
            float ez = cpz - g_portals[i].z;
            const float hit_r = 0.52f;
            if (ex * ex + ez * ez < hit_r * hit_r) {
                best = t;
                hit_ix = i;
                hit_type = 2;
            }
        }
        player_shot_spawn(rdx, rdy, rdz, best);
        
        if (hit_type == 1) {
            g_demon[hit_ix].hp--;
            g_demon[hit_ix].hurt_flash = 0.75f;
            sfx_hit(g_demon[hit_ix].x, g_demon[hit_ix].z);
            if (g_demon[hit_ix].hp <= 0) {
                g_demon[hit_ix].alive = 0;
                g_kills++;
                score_add(SCORE_DEMON);
                melo_request(MELOK_KILL);
                /* Death roar */
                sfx_play_spatial(SIT_WAVE_SAW, 35.0f, 0.6f, 0.02f, 0.1f, 0.4f, g_demon[hit_ix].x, g_demon[hit_ix].z, false);
                sfx_play_spatial(SIT_WAVE_NOISE, 60.0f, 0.4f, 0.01f, 0.1f, 0.3f, g_demon[hit_ix].x, g_demon[hit_ix].z, false);
            }
        } else if (hit_type == 2) {
            g_portals[hit_ix].hp--;
            sfx_hit(g_portals[hit_ix].x, g_portals[hit_ix].z);
            if (g_portals[hit_ix].hp <= 0) {
                g_portals[hit_ix].alive = 0;
                score_add(SCORE_PORTAL);
                sfx_play_spatial(SIT_WAVE_NOISE, 30.0f, 1.0f, 0.1f, 0.5f, 1.0f, g_portals[hit_ix].x, g_portals[hit_ix].z, false);
                sfx_play_spatial(SIT_WAVE_SQUARE, 40.0f, 0.8f, 0.05f, 0.5f, 0.8f, g_portals[hit_ix].x, g_portals[hit_ix].z, false);
            }
        } else if (hit_type == 3) {
            HunterDrone* drone = &g_hunter_drones[hit_ix];
            drone->hp--;
            drone->hurt_flash = 0.65f;
            sfx_hit(drone->x, drone->z);
            if (drone->hp <= 0) {
                drone->active = 0;
                score_add(SCORE_HUNTER_DRONE);
                melo_request(MELOK_KILL);
                for (int p = 0; p < 18; p++) {
                    particle_spawn(
                        drone->x + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.45f,
                        HUNTER_DRONE_HOVER_Y + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.26f,
                        drone->z + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.45f,
                        ((float)(rand() % 100) / 100.0f - 0.5f) * 1.4f,
                        ((float)(rand() % 100) / 100.0f - 0.5f) * 1.2f,
                        ((float)(rand() % 100) / 100.0f - 0.5f) * 1.4f,
                        0.45f + ((float)(rand() % 100) / 100.0f) * 0.7f,
                        1.8f,
                        0.95f, 0.40f, 0.12f);
                }
                sfx_drone_destroyed(drone->x, drone->z);
            }
        }
    }
    
    /* Portal Spawning */
    if (SituationTimerGetTime() - g_last_spawn_time >= 5.0) {
        g_last_spawn_time = (float)SituationTimerGetTime();
        for (int p = 0; p < PORTAL_COUNT; p++) {
            if (!g_portals[p].alive) continue;
            for (int i = 0; i < DEMON_COUNT; i++) {
                if (!g_demon[i].alive) {
                    g_demon[i].alive = 1;
                    g_demon[i].hp = 3;
                    g_demon[i].x = g_portals[p].x;
                    g_demon[i].z = g_portals[p].z;
                    g_demon[i].hurt_flash = 0.0f;
                    sfx_play_spatial(SIT_WAVE_TRIANGLE, 400.0f, 0.4f, 0.1f, 0.1f, 0.3f, g_portals[p].x, g_portals[p].z, false);
                    break;
                }
            }
        }
    }

    /* Live generators shed particles matching their purple/magenta energy. */
    for (int p = 0; p < PORTAL_COUNT; p++) {
        if (!g_portals[p].alive) {
            continue;
        }
        if ((rand() % 6) == 0) {
            float pulse = sinf((float)SituationTimerGetTime() * 8.0f + (float)p) * 0.5f + 0.5f;
            int drops = 1 + (rand() % 2);
            for (int drop = 0; drop < drops; drop++) {
                particle_spawn(
                    g_portals[p].x + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.62f,
                    0.12f + ((float)(rand() % 100) / 100.0f) * 1.1f,
                    g_portals[p].z + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.62f,
                    ((float)(rand() % 100) / 100.0f - 0.5f) * 0.32f,
                    0.35f + ((float)(rand() % 100) / 100.0f) * 0.95f,
                    ((float)(rand() % 100) / 100.0f - 0.5f) * 0.32f,
                    0.95f + ((float)(rand() % 100) / 100.0f) * 1.15f,
                    0.52f + pulse * 0.08f,
                    0.9f, 0.4f + 0.6f * pulse, 1.0f);
            }
        }
    }

    /* Once opened, the exit pillar vents a light upward stream (keep bright below burst sizing). */
    if (exit_gate_open()) {
        float ex, ez;
        if (get_exit_position(&ex, &ez) && (rand() % 3) == 0) {
            int drops = 1 + (rand() % 2);
            float pulse = sinf((float)SituationTimerGetTime() * 7.1f) * 0.5f + 0.5f;
            for (int drop = 0; drop < drops; drop++) {
                float spread_x = ((float)(rand() % 100) / 100.0f - 0.5f) * 0.38f;
                float spread_z = ((float)(rand() % 100) / 100.0f - 0.5f) * 0.38f;
                particle_spawn(
                    ex + spread_x,
                    0.12f + ((float)(rand() % 100) / 100.0f) * 0.35f,
                    ez + spread_z,
                    spread_x * 0.18f,
                    0.95f + ((float)(rand() % 100) / 100.0f) * 1.45f,
                    spread_z * 0.18f,
                    1.35f + ((float)(rand() % 100) / 100.0f) * 0.95f,
                    0.90f + pulse * 0.12f,
                    0.16f, 0.82f + 0.18f * pulse, 0.90f);
            }
        }
    }

    while (g_spawn_elapsed >= g_next_hellraiser_spawn_time) {
        Hellraiser* next = hellraiser_first_inactive();
        if (!next) {
            break;
        }
        hellraiser_spawn(next);
        g_next_hellraiser_spawn_time += HELLRAISER_SPAWN_INTERVAL;
    }
    {
        Hellraiser* next = hellraiser_first_inactive();
        if (next && !next->warned && g_spawn_elapsed >= g_next_hellraiser_spawn_time - HELLRAISER_WARN_LEAD) {
            next->warned = 1;
            next->sync_ix = SituationTimerGetOscillatorTriggerCount(HELLRAISER_OSC_ID);
            SituationPlayToneEx(SIT_WAVE_SAW, 58.0f, 0.55f, 0.0f, 0.02f, 0.20f, 0.0f, 0.35f, 0.06f);
        }
    }

    for (int hix = 0; hix < HELLRAISER_COUNT; hix++) {
        Hellraiser* h = &g_hellraisers[hix];
        if (h->warned || h->active) {
            float remaining = fmaxf(0.0f, g_next_hellraiser_spawn_time - g_spawn_elapsed);
            double pulse_period = h->active ? 0.20 : (0.16 + (double)(remaining / 30.0f) * 0.42);
            if (pulse_period < 0.16) {
                pulse_period = 0.16;
            }
            SituationSetTimerOscillatorPeriod(HELLRAISER_OSC_ID, pulse_period);
            uint64_t tr = SituationTimerGetOscillatorTriggerCount(HELLRAISER_OSC_ID);
            while (h->sync_ix < tr) {
                float urgency = h->active ? 1.0f : 1.0f - fminf(1.0f, remaining / HELLRAISER_WARN_LEAD);
                if (h->active) {
                    sfx_play_spatial(SIT_WAVE_SAW, 52.0f + urgency * 38.0f, 0.28f, 0.002f, 0.04f, 0.18f, h->x, h->z, true);
                } else {
                    SituationPlayToneEx(SIT_WAVE_SQUARE, 72.0f + urgency * 90.0f, 0.16f + urgency * 0.14f, 0.0f, 0.002f, 0.03f, 0.0f, 0.10f, 0.02f);
                }
                h->sync_ix++;
            }
        }
        if (h->active) {
            if (h->spawn_freeze > 0.0f) {
                h->spawn_freeze -= dt;
            } else {
                hellraiser_pathfind_move(h, dt);
            }
            float hx = g_px - h->x;
            float hz = g_pz - h->z;
            float hd = sqrtf(hx * hx + hz * hz);
            if (h->teleport_cooldown > 0.0f) {
                h->teleport_cooldown -= dt;
            } else {
                for (int t = 0; t < g_teleporter_count; t++) {
                    if (!g_teleporters[t].active) {
                        continue;
                    }
                    float tdx = h->x - g_teleporters[t].x;
                    float tdz = h->z - g_teleporters[t].z;
                    if (tdx * tdx + tdz * tdz < 0.5f * 0.5f) {
                        int dest = (t + 1) % g_teleporter_count;
                        h->x = g_teleporters[dest].x;
                        h->z = g_teleporters[dest].z;
                        h->teleport_cooldown = 2.0f;
                        sfx_play_spatial(SIT_WAVE_SAW, 95.0f, 0.85f, 0.01f, 0.10f, 0.45f, h->x, h->z, true);
                        break;
                    }
                }
            }
            hx = g_px - h->x;
            hz = g_pz - h->z;
            hd = sqrtf(hx * hx + hz * hz);
            if (hd < HELLRAISER_TOUCH_RADIUS) {
                g_death_reason = DEATH_HELLRAISER_TOUCH;
                g_health = 0;
                g_pain_flash = 1.0f;
            }
        }
    }

    /* Hunter drones hover above the base floor and punish long sightlines. */
    for (int i = 0; i < HUNTER_DRONE_COUNT; i++) {
        HunterDrone* drone = &g_hunter_drones[i];
        if (!drone->active) {
            continue;
        }
        if (drone->hurt_flash > 0.0f) {
            drone->hurt_flash -= dt * 1.8f;
            if (drone->hurt_flash < 0.0f) {
                drone->hurt_flash = 0.0f;
            }
        }
        if (drone->fire_cooldown > 0.0f) {
            drone->fire_cooldown -= dt;
        }

        float dx = g_px - drone->x;
        float dz = g_pz - drone->z;
        float dist = sqrtf(dx * dx + dz * dz);
        float inv_dist = dist > 0.01f ? 1.0f / dist : 0.0f;
        float to_px = dx * inv_dist;
        float to_pz = dz * inv_dist;
        float orbit_x = -to_pz;
        float orbit_z = to_px;
        int orbit_level = current_level_is_arena() || current_level_is_long_walk();
        float desired = orbit_level ? 6.2f : (current_level_is_gauntlet() ? 4.8f : 5.8f);
        float orbit_push = orbit_level ? 1.35f : 0.55f;
        float range_push = (dist - desired) * 0.34f;
        float mdx = (to_px * range_push + orbit_x * sinf((float)SituationTimerGetTime() * 0.9f + drone->ang) * orbit_push) * HUNTER_DRONE_SPEED * dt;
        float mdz = (to_pz * range_push + orbit_z * sinf((float)SituationTimerGetTime() * 0.9f + drone->ang) * orbit_push) * HUNTER_DRONE_SPEED * dt;
        hunter_drone_try_move(drone, mdx, mdz);
        drone->ang += dt * 1.7f;

        if (dist < HUNTER_DRONE_FLY_HEAR_DIST) {
            float blade_hz = 5.0f + sinf(drone->ang * 1.3f) * 0.6f;
            drone->fly_pulse -= dt;
            if (drone->fly_pulse <= 0.0f) {
                drone->fly_pulse = 1.0f / blade_hz;
                sfx_drone_fly(drone->x, drone->z, drone->ang, i);
            }
        } else {
            drone->fly_pulse = 0.0f;
        }

        if (dist < HUNTER_DRONE_SHOT_RADIUS && drone->fire_cooldown <= 0.0f && los_between(drone->x, drone->z, g_px, g_pz)) {
            drone->fire_cooldown = HUNTER_DRONE_FIRE_COOLDOWN;
            sfx_drone_fire(drone->x, drone->z);
            drone_shot_spawn(drone->x, HUNTER_DRONE_HOVER_Y, drone->z, g_px, g_eye_y, g_pz);
            float muzzle_dx = g_px - drone->x;
            float muzzle_dy = g_eye_y - HUNTER_DRONE_HOVER_Y;
            float muzzle_dz = g_pz - drone->z;
            float muzzle_len = sqrtf(muzzle_dx * muzzle_dx + muzzle_dy * muzzle_dy + muzzle_dz * muzzle_dz);
            if (muzzle_len > 0.01f) {
                muzzle_dx /= muzzle_len;
                muzzle_dy /= muzzle_len;
                muzzle_dz /= muzzle_len;
            }
            for (int p = 0; p < 6; p++) {
                particle_spawn(
                    drone->x + muzzle_dx * 0.18f,
                    HUNTER_DRONE_HOVER_Y + muzzle_dy * 0.18f,
                    drone->z + muzzle_dz * 0.18f,
                    muzzle_dx * (2.0f + (float)(rand() % 100) / 100.0f * 1.5f),
                    muzzle_dy * (2.0f + (float)(rand() % 100) / 100.0f * 1.5f),
                    muzzle_dz * (2.0f + (float)(rand() % 100) / 100.0f * 1.5f),
                    0.22f + ((float)(rand() % 100) / 100.0f) * 0.25f,
                    0.55f,
                    0.35f, 0.90f, 1.0f);
            }
        }
    }

    /* Demons wander + melee */
    int in_melee = 0;
    for (int i = 0; i < DEMON_COUNT; i++) {
        Demon* d = &g_demon[i];
        if (!d->alive) {
            continue;
        }
        if (d->teleport_cooldown > 0.0f) d->teleport_cooldown -= dt;
        if (d->hurt_flash > 0.0f) {
            d->hurt_flash -= dt * 1.6f;
            if (d->hurt_flash < 0.0f) {
                d->hurt_flash = 0.0f;
            }
        }
        
        float wander = sinf(d->ang) * DEMON_WANDER * dt;
        float wz = cosf(d->ang) * DEMON_WANDER * dt;
        
        float old_x = d->x;
        float old_z = d->z;
        demon_try_move(d, wander, wz);
        
        if (fabsf(d->x - old_x) < 0.001f && fabsf(d->z - old_z) < 0.001f) {
            d->ang = ((float)(rand() % 360)) * (3.14159f / 180.0f);
        }
        
        if (d->teleport_cooldown <= 0.0f) {
            for (int t = 0; t < g_teleporter_count; t++) {
                if (!g_teleporters[t].active) continue;
                float tdx = d->x - g_teleporters[t].x;
                float tdz = d->z - g_teleporters[t].z;
                if (tdx * tdx + tdz * tdz < 0.5f * 0.5f) {
                    int dest = (t + 1) % g_teleporter_count;
                    d->x = g_teleporters[dest].x;
                    d->z = g_teleporters[dest].z;
                    d->teleport_cooldown = 2.0f;
                    sfx_play_spatial(SIT_WAVE_TRIANGLE, 150.0f, 0.8f, 0.1f, 0.1f, 0.5f, d->x, d->z, false);
                    break;
                }
            }
        }
        float ddx = g_px - d->x;
        float ddz = g_pz - d->z;
        float dist = sqrtf(ddx * ddx + ddz * ddz);
        if (dist < 0.55f && dist > 0.01f) {
            in_melee = 1;
        }
        
        /* Ambient stereo growl */
        if ((rand() % 300) == 0) {
            sfx_play_spatial(SIT_WAVE_SAW, 30.0f, 0.25f, 0.4f, 0.5f, 0.8f, d->x, d->z, false);
        }
        
        /* Radiation particles intensify briefly after the demon is hurt. */
        float hurt = d->hurt_flash;
        int particle_chance = hurt > 0.0f ? 4 : 15;
        if ((rand() % particle_chance) == 0) {
            int drops = hurt > 0.0f ? 3 : 1;
            for (int drop = 0; drop < drops; drop++) {
                particle_spawn(
                    d->x + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.46f,
                    0.42f + ((float)(rand() % 100) / 100.0f) * 0.46f,
                    d->z + ((float)(rand() % 100) / 100.0f - 0.5f) * 0.46f,
                    ((float)(rand() % 100) / 100.0f - 0.5f) * (0.5f + hurt * 0.35f),
                    -(0.28f + ((float)(rand() % 100) / 100.0f) * (0.75f + hurt * 0.55f)),
                    ((float)(rand() % 100) / 100.0f - 0.5f) * (0.5f + hurt * 0.35f),
                    0.45f + ((float)(rand() % 100) / 100.0f) * (0.75f + hurt * 0.35f),
                    1.0f + hurt * 2.4f,
                    0.2f, 0.85f, 0.3f);
            }
        }
    }
    
    /* Update particles */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_particles[i].life > 0.0f) {
            g_particles[i].x += g_particles[i].vx * dt;
            g_particles[i].y += g_particles[i].vy * dt;
            g_particles[i].z += g_particles[i].vz * dt;
            g_particles[i].life -= dt;
        }
    }
    for (int i = 0; i < DRONE_EXPLOSION_COUNT; i++) {
        if (g_drone_explosions[i].life > 0.0f) {
            g_drone_explosions[i].life -= dt;
        }
    }
    for (int i = 0; i < PLAYER_SHOT_COUNT; i++) {
        PlayerShot* shot = &g_player_shots[i];
        if (!shot->active) {
            continue;
        }
        shot->travel += PLAYER_SHOT_SPEED * dt;
        if (shot->travel >= shot->max_travel) {
            shot->active = 0;
            continue;
        }
        shot->x = shot->origin_x + shot->dir_x * shot->travel;
        shot->y = shot->origin_y + shot->dir_y * shot->travel;
        shot->z = shot->origin_z + shot->dir_z * shot->travel;
    }

    for (int i = 0; i < DRONE_SHOT_COUNT; i++) {
        DroneShot* shot = &g_drone_shots[i];
        if (!shot->active) {
            continue;
        }
        float prev_x = shot->x;
        float prev_y = shot->y;
        float prev_z = shot->z;
        shot->travel += DRONE_SHOT_SPEED * dt;
        if (shot->travel >= shot->max_travel) {
            float ex = shot->origin_x + shot->dir_x * shot->max_travel;
            float ey = shot->origin_y + shot->dir_y * shot->max_travel;
            float ez = shot->origin_z + shot->dir_z * shot->max_travel;
            drone_shot_explode(ex, ey, ez);
            shot->active = 0;
            continue;
        }
        shot->x = shot->origin_x + shot->dir_x * shot->travel;
        shot->y = shot->origin_y + shot->dir_y * shot->travel;
        shot->z = shot->origin_z + shot->dir_z * shot->travel;

        float seg_dx = shot->x - prev_x;
        float seg_dy = shot->y - prev_y;
        float seg_dz = shot->z - prev_z;
        float seg_len = sqrtf(seg_dx * seg_dx + seg_dy * seg_dy + seg_dz * seg_dz);
        float seg_len_xz = sqrtf(seg_dx * seg_dx + seg_dz * seg_dz);

        {
            float hit_x = 0.0f;
            float hit_y = 0.0f;
            float hit_z = 0.0f;
            if (drone_shot_wall_hit_segment(
                    prev_x, prev_y, prev_z,
                    shot->x, shot->y, shot->z,
                    shot->dir_x, shot->dir_y, shot->dir_z,
                    seg_len_xz,
                    &hit_x, &hit_y, &hit_z)) {
                drone_shot_explode(hit_x, hit_y, hit_z);
                shot->active = 0;
                continue;
            }
        }

        int consumed = 0;
        for (int di = 0; di < DEMON_COUNT; di++) {
            Demon* demon = &g_demon[di];
            if (!demon->alive) {
                continue;
            }
            const float demon_hit_r = 0.45f;
            float hit_r2 = demon_hit_r * demon_hit_r;
            if (segment_point_dist_sq(prev_x, prev_y, prev_z, shot->x, shot->y, shot->z, demon->x, 0.5f, demon->z) >= hit_r2) {
                continue;
            }
            demon->hp--;
            demon->hurt_flash = 0.65f;
            sfx_hit(demon->x, demon->z);
            if (demon->hp <= 0) {
                demon->alive = 0;
                g_kills++;
                score_add(SCORE_DEMON);
                melo_request(MELOK_KILL);
                sfx_play_spatial(SIT_WAVE_SAW, 35.0f, 0.6f, 0.02f, 0.1f, 0.4f, demon->x, demon->z, false);
                sfx_play_spatial(SIT_WAVE_NOISE, 60.0f, 0.4f, 0.01f, 0.1f, 0.3f, demon->x, demon->z, false);
            }
            drone_shot_explode(shot->x, shot->y, shot->z);
            shot->active = 0;
            consumed = 1;
            break;
        }
        if (consumed) {
            continue;
        }

        const float hit_r = 0.40f;
        float hit_r2 = hit_r * hit_r;
        float pdist_sq = segment_point_dist_sq(prev_x, prev_y, prev_z, shot->x, shot->y, shot->z, g_px, g_eye_y, g_pz);
        if (pdist_sq < hit_r2) {
            float hx = shot->x;
            float hy = shot->y;
            float hz = shot->z;
            if (seg_len > 1e-5f) {
                float abx = shot->x - prev_x;
                float aby = shot->y - prev_y;
                float abz = shot->z - prev_z;
                float apx = g_px - prev_x;
                float apy = g_eye_y - prev_y;
                float apz = g_pz - prev_z;
                float t = (apx * abx + apy * aby + apz * abz) / (seg_len * seg_len);
                if (t < 0.0f) {
                    t = 0.0f;
                } else if (t > 1.0f) {
                    t = 1.0f;
                }
                hx = prev_x + abx * t;
                hy = prev_y + aby * t;
                hz = prev_z + abz * t;
            }
            g_health -= current_level_is_gauntlet() ? 7 : 5;
            g_pain_flash = 1.0f;
            if (g_health <= 0 && g_death_reason == DEATH_NONE) {
                g_death_reason = DEATH_HUNTER_DRONE;
            }
            sfx_hurt();
            shot->woosh_played = 1;
            drone_shot_explode(hx, hy, hz);
            shot->active = 0;
            continue;
        }

        float pdist = sqrtf(pdist_sq);
        if (!shot->woosh_played && shot->travel > 0.35f && pdist < 1.6f) {
            shot->woosh_played = 1;
            sfx_drone_woosh(shot->x, shot->z);
        }

        {
            int trail_steps = (int)(seg_len / 0.055f) + 1;
            if (trail_steps < 2) {
                trail_steps = 2;
            }
            if (trail_steps > 8) {
                trail_steps = 8;
            }
            for (int ts = 1; ts <= trail_steps; ts++) {
                float tf = (float)ts / (float)trail_steps;
                drone_shot_emit_trail_at(
                    shot,
                    prev_x + seg_dx * tf,
                    prev_y + seg_dy * tf,
                    prev_z + seg_dz * tf);
            }
        }
    }
    
    /* Ammo Box Pickups */
    for (int i = 0; i < AMMO_COUNT; i++) {
        if (g_ammo_box[i].active) {
            float adx = g_px - g_ammo_box[i].x;
            float adz = g_pz - g_ammo_box[i].z;
            float adist = sqrtf(adx * adx + adz * adz);
            if (adist < 0.6f) {
                g_ammo_box[i].active = 0;
                g_ammo_box[i].respawn_timer = current_level_is_gauntlet() ? 15.0f : 60.0f;
                g_ammo += 15;
                if (current_level_is_gauntlet()) {
                    score_add(SCORE_AMMO_PICKUP);
                }
                SituationPlayToneEx(SIT_WAVE_TRIANGLE, 400.0f, 0.3f, 0.0f, 0.01f, 0.05f, 0.0f, 0.1f, 0.05f);
                SituationPlayToneEx(SIT_WAVE_SQUARE, 600.0f, 0.2f, 0.0f, 0.01f, 0.05f, 0.0f, 0.1f, 0.05f);
            }
        } else if (g_ammo_box[i].respawn_timer > 0.0f) {
            g_ammo_box[i].respawn_timer -= dt;
            if (g_ammo_box[i].respawn_timer <= 0.0f) {
                g_ammo_box[i].active = 1;
                g_ammo_box[i].respawn_timer = 0.0f;
            }
        }
    }

    if (in_melee && g_melee_cooldown <= 0.0f) {
        player_hurt_from_enemy(10, DEATH_DEMON_MELEE);
    }
    
    if (g_player_teleport_cooldown > 0.0f) g_player_teleport_cooldown -= dt;
    else {
        for (int i = 0; i < g_teleporter_count; i++) {
            if (!g_teleporters[i].active) continue;
            float dx = g_px - g_teleporters[i].x;
            float dz = g_pz - g_teleporters[i].z;
            if (dx * dx + dz * dz < 0.5f * 0.5f) {
                int dest = (i + 1) % g_teleporter_count;
                g_px = g_teleporters[dest].x;
                g_pz = g_teleporters[dest].z;
                g_eye_y = PLAYER_EYE_GROUND_Y;
                g_player_vy = 0.0f;
                g_player_grounded = 1;
                g_player_teleport_cooldown = 1.0f;
                sfx_play_spatial(SIT_WAVE_SQUARE, 200.0f, 0.8f, 0.1f, 0.1f, 0.5f, g_px, g_pz, true);
                break;
            }
        }
    }

    if (g_health <= 0) {
        if (g_death_reason == DEATH_NONE) {
            g_death_reason = DEATH_DEMON_MELEE;
        }
        melo_request(MELOK_DEATH);
        score_commit_high();
        g_phase = PHASE_DEATH;
        SituationShowCursor();
        return;
    }

    /* Clear: hunt levels require closed portals; gauntlet only requires reaching the far exit. */
    int near_exit = 0;
    {
        float ex, ez;
        if (get_exit_position(&ex, &ez) && hypotf(g_px - ex, g_pz - ez) < 1.1f) {
            near_exit = 1;
        }
    }
    int cheat_exit = cheat_exit_pressed();
    if (cheat_exit) {
        near_exit = 1;
    }
    if ((exit_gate_open() && near_exit) || cheat_exit) {
        g_last_demon_gain = g_kills * SCORE_DEMON;
        g_last_portal_gain = current_level_is_gauntlet() ? 0 : PORTAL_COUNT * SCORE_PORTAL;
        g_last_exit_gain = SCORE_EXIT;
        g_last_bonus_gain = g_health * SCORE_HEALTH_BONUS + g_ammo * SCORE_AMMO_BONUS;
        g_last_total_gain = g_last_demon_gain + g_last_portal_gain + g_last_exit_gain + g_last_bonus_gain;
        score_add(g_last_exit_gain + g_last_bonus_gain);
        g_phase = PHASE_WIN;
        SituationShowCursor();
        melo_request(MELOK_WIN);
    }
}

static int demons_alive_count(void) {
    int n = 0;
    for (int i = 0; i < DEMON_COUNT; i++) {
        if (g_demon[i].alive) {
            n++;
        }
    }
    return n;
}

static int hunter_drones_alive_count(void) {
    int n = 0;
    for (int i = 0; i < HUNTER_DRONE_COUNT; i++) {
        if (g_hunter_drones[i].active) {
            n++;
        }
    }
    return n;
}

static int portals_alive_count(void) {
    int n = 0;
    for (int i = 0; i < PORTAL_COUNT; i++) {
        if (g_portals[i].alive) {
            n++;
        }
    }
    return n;
}

static int exit_gate_open(void) {
    return current_level_is_gauntlet() || portals_alive_count() == 0;
}

static void shader_sprite_reset(void) {
    int old_count = g_shader_sprite_count;
    g_shader_sprite_count = 0;
    if (old_count > 0) {
        memset(g_shader_sprites0, 0, (size_t)old_count * sizeof(g_shader_sprites0[0]));
        memset(g_shader_sprites1, 0, (size_t)old_count * sizeof(g_shader_sprites1[0]));
        memset(g_shader_sprites2, 0, (size_t)old_count * sizeof(g_shader_sprites2[0]));
    }
}

static int shader_sprite_push(int type, float x, float y, float z,
                              float half_width, float height, float base_y,
                              float p0, float p1, float p2, float p3) {
    if (g_shader_sprite_count >= SHADER_SPRITE_GPU_MAX) {
        return 0;
    }
    int i = g_shader_sprite_count++;
    g_shader_sprites0[i][0] = x;
    g_shader_sprites0[i][1] = y;
    g_shader_sprites0[i][2] = z;
    g_shader_sprites0[i][3] = 1.0f;
    g_shader_sprites1[i][0] = half_width;
    g_shader_sprites1[i][1] = height;
    g_shader_sprites1[i][2] = base_y;
    g_shader_sprites1[i][3] = (float)type;
    g_shader_sprites2[i][0] = p0;
    g_shader_sprites2[i][1] = p1;
    g_shader_sprites2[i][2] = p2;
    g_shader_sprites2[i][3] = p3;
    return 1;
}

/*
 * Render packing is intentionally separate from gameplay state. Phase 3 effect
 * sprites are still packed even when their shader shading is compile-gated so
 * they can be re-enabled one type at a time without changing simulation code.
 */
static int shader_sprite_in_view(float x, float z, float dist) {
    float fx = sinf(g_yaw);
    float fz = cosf(g_yaw);
    float dx = x - g_px;
    float dz = z - g_pz;
    float ahead = dx * fx + dz * fz;
    if (ahead < -0.35f) {
        return 0;
    }
    if (dist > SHADER_SPRITE_CULL_DIST) {
        return 0;
    }
    return 1;
}

static int shader_sprite_candidate_add(
    int type, float priority, float x, float y, float z,
    float half_w, float height, float base_y,
    float p0, float p1, float p2, float p3) {
    if (g_sprite_candidate_count >= SHADER_SPRITE_PACK_MAX) {
        return 0;
    }
    float dx = x - g_px;
    float dz = z - g_pz;
    float dist = sqrtf(dx * dx + dz * dz);
    if (!shader_sprite_in_view(x, z, dist)) {
        return 0;
    }
    if (!los_to_point(x, z)) {
        return 0;
    }
    ShaderSpriteCandidate* c = &g_sprite_candidates[g_sprite_candidate_count++];
    c->type = type;
    c->priority = priority;
    c->dist = dist;
    c->x = x;
    c->y = y;
    c->z = z;
    c->half_w = half_w;
    c->height = height;
    c->base_y = base_y;
    c->p0 = p0;
    c->p1 = p1;
    c->p2 = p2;
    c->p3 = p3;
    return 1;
}

static int shader_sprite_candidate_compare(const void* a, const void* b) {
    const ShaderSpriteCandidate* sa = (const ShaderSpriteCandidate*)a;
    const ShaderSpriteCandidate* sb = (const ShaderSpriteCandidate*)b;
    /* Lower priority values pack first (exit/demons before particles). */
    if (sa->priority < sb->priority) {
        return -1;
    }
    if (sa->priority > sb->priority) {
        return 1;
    }
    if (sa->dist < sb->dist) {
        return -1;
    }
    if (sa->dist > sb->dist) {
        return 1;
    }
    return 0;
}

/* Match CPU billboard sizing: screen_h = sh*0.92/perp, screen_w = 0.62*screen_h. */
static void shader_sprite_screen_size(
    int sw, int sh, float dist, float half_w_frac, float height_frac, float* half_w, float* height) {
    float perp = fmaxf(dist, 0.02f);
    float screen_h = (float)sh * 0.92f / perp;
    float screen_w = screen_h * 0.62f;
    float to_world = perp / ((float)sh * 0.92f);
    *height = screen_h * height_frac * to_world;
    *half_w = screen_w * half_w_frac * to_world;
}

static void shader_sprite_particle_size(
    int sw, int sh, float dist, const Particle* p, float* half_w, float* height) {
    float perp = fmaxf(dist, 0.02f);
    float screen_h = (float)sh * 0.92f / perp;
    float screen_w = screen_h * 0.62f;
    float scale = fminf(p->life, 1.25f);
    float bright = p->bright > 0.0f ? p->bright : 1.0f;
    int bolt = particle_is_bolt_trail(p);
    int explosion = particle_is_explosion(p);
    int drip = particle_is_demon_drip(p);
    int portal_energy = particle_is_portal_energy(p);
    float size_mul = bolt ? 0.30f : (explosion ? 0.38f : (drip ? 0.08f : (portal_energy ? 0.045f : 0.10f)));
    float pw = screen_w * size_mul * scale;
    float ph = screen_h * size_mul * scale;
    if (bolt) {
        pw = fmaxf(pw, 7.0f);
        ph = fmaxf(ph, 7.0f);
    } else if (explosion) {
        pw = fmaxf(pw, 18.0f);
        ph = fmaxf(ph, 18.0f);
    }
    float to_world = perp / ((float)sh * 0.92f);
    *half_w = pw * to_world;
    *height = ph * to_world;
}

static void pack_shader_sprites(int sw, int sh) {
    if (g_phase != PHASE_PLAY && g_phase != PHASE_PAUSE) {
        shader_sprite_reset();
        return;
    }
    float tt = (float)SituationTimerGetTime();
    float threat_pulse = SituationTimerGetOscillatorState(HELLRAISER_OSC_ID) ? 1.0f : 0.0f;
    g_sprite_candidate_count = 0;

    {
        float ex, ez;
        if (get_exit_position(&ex, &ez)) {
            float dx = ex - g_px;
            float dz = ez - g_pz;
            float dist = sqrtf(dx * dx + dz * dz);
            float hw, ht;
            shader_sprite_screen_size(sw, sh, dist, 0.40f, 1.05f, &hw, &ht);
            shader_sprite_candidate_add(
                SHADER_SPRITE_EXIT_PILLAR, 3.2f, ex, 0.0f, ez,
                hw, ht, 0.0f,
                (float)exit_gate_open(), 0.0f, 0.0f, 0.0f);
        }
    }
    for (int i = 0; i < DEMON_COUNT; i++) {
        if (!g_demon[i].alive) {
            continue;
        }
        float dx = g_demon[i].x - g_px;
        float dz = g_demon[i].z - g_pz;
        float dist = sqrtf(dx * dx + dz * dz);
        float hw, ht;
        shader_sprite_screen_size(sw, sh, dist, 0.35f, 1.0f, &hw, &ht);
        shader_sprite_candidate_add(
            SHADER_SPRITE_DEMON, 1.0f, g_demon[i].x, 0.5f, g_demon[i].z,
            hw, ht, 0.04f,
            g_demon[i].hurt_flash,
            tt * 3.5f + (float)i * 2.0f,
            tt * 8.0f + (float)i,
            tt * 4.0f + (float)i * 2.0f);
    }
    for (int i = 0; i < HELLRAISER_COUNT; i++) {
        if (!g_hellraisers[i].active) {
            continue;
        }
        float dx = g_hellraisers[i].x - g_px;
        float dz = g_hellraisers[i].z - g_pz;
        float dist = sqrtf(dx * dx + dz * dz);
        float hw, ht;
        shader_sprite_screen_size(sw, sh, dist, 0.35f * 1.15f, 1.35f, &hw, &ht);
        shader_sprite_candidate_add(
            SHADER_SPRITE_HELLRAISER, 2.0f, g_hellraisers[i].x, 0.58f, g_hellraisers[i].z,
            hw, ht, 0.02f,
            g_hellraisers[i].spawn_freeze,
            tt * 4.4f + (float)i,
            (float)i,
            threat_pulse);
    }
    for (int i = 0; i < PORTAL_COUNT; i++) {
        if (!g_portals[i].alive) {
            continue;
        }
        float dx = g_portals[i].x - g_px;
        float dz = g_portals[i].z - g_pz;
        float dist = sqrtf(dx * dx + dz * dz);
        float hw, ht;
        shader_sprite_screen_size(sw, sh, dist, 0.40f, 1.30f, &hw, &ht);
        shader_sprite_candidate_add(
            SHADER_SPRITE_PORTAL, 3.0f, g_portals[i].x, 0.0f, g_portals[i].z,
            hw, ht, 0.0f,
            1.0f, (float)i, 0.0f, 0.0f);
    }
    for (int i = 0; i < PLAYER_SHOT_COUNT; i++) {
        PlayerShot* shot = &g_player_shots[i];
        if (!shot->active) {
            continue;
        }
        float dx = shot->x - g_px;
        float dz = shot->z - g_pz;
        float dist = sqrtf(dx * dx + dz * dz);
        float hw, ht;
        shader_sprite_screen_size(sw, sh, dist, 0.045f * 0.62f, 0.045f, &hw, &ht);
        float fade = 1.0f - fminf(1.0f, shot->travel / fmaxf(0.01f, shot->max_travel));
        shader_sprite_candidate_add(
            SHADER_SPRITE_PLAYER_SHOT, 4.0f, shot->x, shot->y, shot->z,
            hw, ht, shot->y - ht * 0.5f,
            fade, (float)i, shot->travel, shot->max_travel);
    }
    for (int i = 0; i < AMMO_COUNT; i++) {
        if (!g_ammo_box[i].active) {
            continue;
        }
        float dx = g_ammo_box[i].x - g_px;
        float dz = g_ammo_box[i].z - g_pz;
        float dist = sqrtf(dx * dx + dz * dz);
        float hw, ht;
        shader_sprite_screen_size(sw, sh, dist, 0.15f, 0.20f, &hw, &ht);
        shader_sprite_candidate_add(
            SHADER_SPRITE_AMMO, 5.0f, g_ammo_box[i].x, 0.0f, g_ammo_box[i].z,
            hw, ht, 0.0f,
            1.0f,
            tt * 2.5f + (float)i * 1.2f,
            g_ammo_box[i].respawn_timer,
            0.0f);
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &g_particles[i];
        if (p->life <= 0.0f) {
            continue;
        }
        float dx = p->x - g_px;
        float dz = p->z - g_pz;
        float dist = sqrtf(dx * dx + dz * dz);
        float bright = p->bright > 0.0f ? p->bright : 1.0f;
        int explosion = particle_is_explosion(p);
        int drip = particle_is_demon_drip(p);
        int portal_energy = particle_is_portal_energy(p);
        float priority = particle_is_bolt_trail(p) ? 6.0f : (explosion ? 8.5f : 7.0f);
        float half_w, height;
        shader_sprite_particle_size(sw, sh, dist, p, &half_w, &height);
        float alpha_cap = drip ? 1.35f : (portal_energy ? 1.05f : 2.0f);
        float alpha_life = p->life * fminf(bright, alpha_cap);
        shader_sprite_candidate_add(
            SHADER_SPRITE_PARTICLE, priority, p->x, p->y, p->z,
            half_w, height, p->y - half_w,
            alpha_life, p->r, p->g, p->b);
    }

    /* Hunter drones — added to shader sprite path (type 9). */
    for (int i = 0; i < HUNTER_DRONE_COUNT; i++) {
        HunterDrone* drone = &g_hunter_drones[i];
        if (!drone->active) continue;
        float dx = drone->x - g_px;
        float dz = drone->z - g_pz;
        float dist = sqrtf(dx * dx + dz * dz);
        float hw, ht;
        shader_sprite_screen_size(sw, sh, dist, 0.28f, 0.40f, &hw, &ht);
        shader_sprite_candidate_add(
            SHADER_SPRITE_HUNTER_DRONE, 2.5f, drone->x, HUNTER_DRONE_HOVER_Y, drone->z,
            hw, ht, HUNTER_DRONE_HOVER_Y - ht,
            drone->hurt_flash,
            tt * 3.8f + (float)i,
            0.0f,
            drone->ang);
    }

    /* Drone shots — added to shader sprite path (type 8). */
    for (int i = 0; i < DRONE_SHOT_COUNT; i++) {
        DroneShot* bolt = &g_drone_shots[i];
        if (!bolt->active) continue;
        float dx = bolt->x - g_px;
        float dz = bolt->z - g_pz;
        float dist = sqrtf(dx * dx + dz * dz);
        float hw, ht;
        shader_sprite_screen_size(sw, sh, dist, 0.055f, 0.055f, &hw, &ht);
        shader_sprite_candidate_add(
            SHADER_SPRITE_DRONE_SHOT, 6.5f, bolt->x, bolt->y, bolt->z,
            hw, ht, bolt->y - ht * 0.5f,
            bolt->travel, 0.0f, 0.0f, 0.0f);
    }

    if (g_sprite_candidate_count > 1) {
        qsort(g_sprite_candidates, (size_t)g_sprite_candidate_count, sizeof(g_sprite_candidates[0]), shader_sprite_candidate_compare);
    }

    shader_sprite_reset();
    for (int i = 0; i < g_sprite_candidate_count && g_shader_sprite_count < SHADER_SPRITE_GPU_MAX; i++) {
        ShaderSpriteCandidate* c = &g_sprite_candidates[i];
        shader_sprite_push(
            c->type, c->x, c->y, c->z,
            c->half_w, c->height, c->base_y,
            c->p0, c->p1, c->p2, c->p3);
    }
}


static void render_world(SituationCommandBuffer cmd, int sw, int sh) {
    float lit_x, lit_y, lit_z;
    sun_direction(&lit_x, &lit_y, &lit_z);

    float aspect = (float)sw / (float)sh;
    SituationCameraDesc cam_desc;
    mat4 inv_vp, flat_vp;
    build_level_wall_camera(&cam_desc, inv_vp, aspect);
    SituationCameraBuildViewProj(&cam_desc, flat_vp);

    float h_line = (float)sh * 0.5f + compute_horizon_shift_px((float)sh);

    pack_shader_sprites(sw, sh);
    sky_draw_fullscreen(cmd, sw, sh, h_line, lit_x, lit_y, lit_z);

    float h0 = compute_horizon_shift_px((float)sh);

    /* Crosshair */
    Vector4 ch = {{0.9f, 0.9f, 0.95f, 0.85f}};
    float cx = (float)sw * 0.5f;
    float cy = (float)sh * 0.5f;
    draw_rect_px(cmd, cx - 10.0f, cy - 1.0f, 20.0f, 2.0f, ch);
    draw_rect_px(cmd, cx - 1.0f, cy - 10.0f, 2.0f, 20.0f, ch);
}

/* North-up: +X right, +Z down on overlay. Player facing = (sin yaw, cos yaw) in map plane. */
static void draw_minimap_overlay(SituationCommandBuffer cmd, UiLayout ui) {
    const float cw = (float)MM_CELL;
    const float pad = (float)MM_PAD;
    float gw = (float)g_map_w * cw + pad * 2.0f;
    float gh = (float)g_map_h * cw + pad * 2.0f;
    float ox = UI_BASE_W - (float)MM_MARGIN - gw;
    float oy = UI_BASE_H - (float)MM_MARGIN - gh;

    Vector4 frame = {{0.02f, 0.02f, 0.06f, 0.9f}};
    draw_rect_ui(cmd, ui, ox - 4.0f, oy - 4.0f, gw + 8.0f, gh + 8.0f, frame);

    Vector4 bg = {{0.05f, 0.06f, 0.09f, 0.94f}};
    draw_rect_ui(cmd, ui, ox, oy, gw, gh, bg);

    for (int mz = 0; mz < g_map_h; mz++) {
        for (int mx = 0; mx < g_map_w; mx++) {
            uint8_t c = g_map[mz][mx];
            Vector4 col;
            if (c == CELL_WALL) {
                col = (Vector4){{0.24f, 0.23f, 0.28f, 1.0f}};
            } else if (c == CELL_EXIT) {
                double mt = SituationTimerGetTime();
                int go = exit_gate_open();
                float mp = 0.5f + 0.5f * sinf((float)mt * (go ? 5.4f : 2.65f));
                if (go) {
                    col = (Vector4){{0.14f + 0.28f * mp, 0.52f + 0.32f * mp, 0.56f + 0.28f * mp, 1.0f}};
                } else {
                    col = (Vector4){{0.58f + 0.32f * mp, 0.16f + 0.12f * mp, 0.07f + 0.06f * mp, 1.0f}};
                }
            } else if (c == CELL_ARCH_NS || c == CELL_ARCH_EW) {
                col = (Vector4){{0.16f, 0.14f, 0.18f, 1.0f}};
            } else {
                col = (Vector4){{0.11f, 0.14f, 0.12f, 1.0f}};
            }
            draw_rect_ui(cmd, ui, ox + pad + (float)mx * cw, oy + pad + (float)mz * cw, cw - 0.5f, cw - 0.5f, col);
        }
    }

    for (int i = 0; i < DEMON_COUNT; i++) {
        if (!g_demon[i].alive) {
            continue;
        }
        float dx = ox + pad + g_demon[i].x * cw - 1.0f;
        float dy = oy + pad + g_demon[i].z * cw - 1.0f;
        Vector4 dm = {{0.88f, 0.22f, 0.28f, 1.0f}};
        draw_rect_ui(cmd, ui, dx, dy, 3.0f, 3.0f, dm);
    }

    for (int hix = 0; hix < HELLRAISER_COUNT; hix++) {
        Hellraiser* h = &g_hellraisers[hix];
        if (!h->active) {
            continue;
        }
        float hx = ox + pad + h->x * cw - 2.0f;
        float hy = oy + pad + h->z * cw - 2.0f;
        Vector4 hm = {{1.0f, 0.05f, 0.02f, 1.0f}};
        draw_rect_ui(cmd, ui, hx, hy, 5.0f, 5.0f, hm);
    }

    for (int i = 0; i < HUNTER_DRONE_COUNT; i++) {
        HunterDrone* drone = &g_hunter_drones[i];
        if (!drone->active) {
            continue;
        }
        float dx = ox + pad + drone->x * cw - 1.5f;
        float dy = oy + pad + drone->z * cw - 1.5f;
        Vector4 dm = {{0.20f, 0.75f, 1.0f, 1.0f}};
        draw_rect_ui(cmd, ui, dx, dy, 3.0f, 3.0f, dm);
    }
    
    for (int i = 0; i < AMMO_COUNT; i++) {
        if (!g_ammo_box[i].active) {
            continue;
        }
        float ax = ox + pad + g_ammo_box[i].x * cw - 1.0f;
        float ay = oy + pad + g_ammo_box[i].z * cw - 1.0f;
        Vector4 am = {{0.88f, 0.88f, 0.22f, 1.0f}};
        draw_rect_ui(cmd, ui, ax, ay, 2.0f, 2.0f, am);
    }

    float px = ox + pad + g_px * cw;
    float py = oy + pad + g_pz * cw;
    Vector4 pl = {{1.0f, 0.95f, 0.22f, 1.0f}};
    draw_rect_ui(cmd, ui, px - 1.5f, py - 1.5f, 4.0f, 4.0f, pl);

    float fx = sinf(g_yaw);
    float fz = cosf(g_yaw);
    Vector4 tip = {{1.0f, 1.0f, 0.55f, 1.0f}};
    for (int s = 1; s <= 5; s++) {
        float t = (float)s * 0.22f;
        draw_rect_ui(cmd, ui, px + fx * cw * t - 1.0f, py + fz * cw * t - 1.0f, 2.0f, 2.0f, tip);
    }

    draw_text_ui(cmd, ui, "MAP", ox + 2.0f, oy - 15.0f, 11.0f, 1.0f, (ColorRGBA){200, 210, 240, 230});
}

static double g_frame_acquire_fail_log_time;

static int demon_hunt_acquire_frame(void) {
    if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
        return 1;
    }
    double now = SituationTimerGetTime();
    if (g_frame_acquire_fail_log_time <= 0.0 || (now - g_frame_acquire_fail_log_time) >= 2.0) {
        g_frame_acquire_fail_log_time = now;
        char* msg = NULL;
        if (SituationGetLastErrorMsg(&msg) == SITUATION_SUCCESS && msg && msg[0]) {
            fprintf(stderr, "[demon_hunt] SituationAcquireFrameCommandBuffer failed: %s\n", msg);
            SituationFreeString(msg);
        } else {
            fprintf(
                stderr, "[demon_hunt] SituationAcquireFrameCommandBuffer failed (code %d)\n",
                (int)SituationGetLastErrorCode());
        }
    }
    return 0;
}

static void render_frame(void) {
    if (!demon_hunt_acquire_frame()) {
        return;
    }
    int render_to_game_display = g_game_display >= 0;
    int sw = render_to_game_display ? GAME_RENDER_W : SituationGetRenderWidth();
    int sh = render_to_game_display ? GAME_RENDER_H : SituationGetRenderHeight();
    if (sw < 1) {
        sw = 1;
    }
    if (sh < 1) {
        sh = 1;
    }

    UiLayout ui = ui_layout_for(sw, sh);
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    /* Depth must be cleared: BeginRenderPass enables GL_DEPTH_TEST; without a depth clear the
       default FBO keeps stale depth and the fullscreen sky tri fails the depth test (no sky,
       no floor shading from that pass). */
    SituationRenderPassInfo pass = SituationRenderPassInfoDefault(
        render_to_game_display ? g_game_display : -1,
        (ColorRGBA){12, 8, 18, 255});

    SituationCmdBeginRenderPass(cmd, &pass);

    if (g_phase == PHASE_ENTERING) {
        if (g_entering_tex_ok) {
            SitRectangle src = {0.0f, 0.0f, (float)g_entering_tex_w, (float)g_entering_tex_h};
            SitRectangle dst = {0.0f, 0.0f, (float)sw, (float)sh};
            SituationCmdDrawTexture(cmd, g_entering_tex, src, dst, (Vector2){{0.0f, 0.0f}}, 0.0f, (ColorRGBA){255, 255, 255, 255});
        } else {
            Vector4 bg = {{0.04f, 0.02f, 0.05f, 1.0f}};
            draw_rect_px(cmd, 0.0f, 0.0f, (float)sw, (float)sh, bg);
        }
        {
            Vector4 shade = {{0.0f, 0.0f, 0.0f, 0.45f}};
            draw_rect_ui(cmd, ui, 220.0f, 238.0f, 520.0f, 120.0f, shade);
        }
        {
            float pulse = sinf((float)SituationTimerGetTime() * 5.5f) * 0.5f + 0.5f;
            ColorRGBA title_col = {(unsigned char)(200 + pulse * 40), (unsigned char)(45 + pulse * 35), (unsigned char)(38 + pulse * 22), 255};
            draw_text_center_fit_ui(cmd, ui, "ENTERING...", 480.0f, 268.0f, 42.0f, 1.0f, 900.0f, title_col);
        }
        {
            char level_line[64];
            if (g_entering_fresh_run) {
                snprintf(level_line, sizeof(level_line), "SPAWN %d", g_spawn_number);
            } else {
                snprintf(level_line, sizeof(level_line), "LEVEL %d", g_current_level);
            }
            draw_text_center_fit_ui(cmd, ui, level_line, 480.0f, 332.0f, 16.0f, 1.0f, 900.0f, (ColorRGBA){220, 200, 210, 230});
        }
    } else if (g_phase == PHASE_TITLE) {
        float pulse = sinf((float)SituationTimerGetTime() * 3.1f) * 0.5f + 0.5f;
        float blink = sinf((float)SituationTimerGetTime() * 5.4f) * 0.5f + 0.5f;
        float border_scroll = (float)SituationTimerGetTime() * 0.14f;

        title_draw_backdrop(cmd, ui, sw, sh, border_scroll);
        if (g_title_show_instructions) {
            title_draw_instructions_panel(cmd, ui, pulse);
        } else {
            title_draw_main_panel(cmd, ui, border_scroll, pulse, blink);
        }
    } else if (g_phase == PHASE_WIN) {
        Vector4 bg = {{0.05f, 0.12f, 0.06f, 1.0f}};
        draw_rect_px(cmd, 0.0f, 0.0f, (float)sw, (float)sh, bg);
        draw_text_center_fit_ui(cmd, ui, g_current_level >= LEVEL_COUNT ? "HUNT COMPLETE" : "LEVEL CLEARED", 480.0f, 120.0f, 24.0f, 1.0f, 900.0f, (ColorRGBA){120, 255, 140, 255});
        {
            char b[96];
            snprintf(b, sizeof(b), "Level %d gains", g_current_level);
            draw_text_ui(cmd, ui, b, 402.0f, 180.0f, 15.0f, 1.0f, (ColorRGBA){255, 220, 120, 255});
            snprintf(b, sizeof(b), "Demons destroyed  +%d", g_last_demon_gain);
            draw_text_ui(cmd, ui, b, 355.0f, 222.0f, 13.0f, 1.0f, (ColorRGBA){200, 240, 210, 255});
            snprintf(b, sizeof(b), "Portals closed    +%d", g_last_portal_gain);
            draw_text_ui(cmd, ui, b, 355.0f, 252.0f, 13.0f, 1.0f, (ColorRGBA){200, 240, 210, 255});
            snprintf(b, sizeof(b), "Exit secured      +%d", g_last_exit_gain);
            draw_text_ui(cmd, ui, b, 355.0f, 282.0f, 13.0f, 1.0f, (ColorRGBA){200, 240, 210, 255});
            snprintf(b, sizeof(b), "Survivor bonus    +%d", g_last_bonus_gain);
            draw_text_ui(cmd, ui, b, 355.0f, 312.0f, 13.0f, 1.0f, (ColorRGBA){200, 240, 210, 255});
            snprintf(b, sizeof(b), "TOTAL GAIN        +%d", g_last_total_gain);
            draw_text_ui(cmd, ui, b, 355.0f, 354.0f, 15.0f, 1.0f, (ColorRGBA){255, 240, 160, 255});
            snprintf(b, sizeof(b), "SCORE %06d   HIGH %06d", g_score, g_high_score);
            draw_text_ui(cmd, ui, b, 330.0f, 408.0f, 16.0f, 1.0f, (ColorRGBA){220, 255, 220, 255});
        }
        if (g_current_level >= LEVEL_COUNT) {
            draw_text_ui(cmd, ui, "R  new hunt   ESC  title", 365.0f, 492.0f, 14.0f, 1.0f, (ColorRGBA){180, 255, 180, 255});
        } else {
            draw_text_ui(cmd, ui, "Ready for the next level?", 365.0f, 468.0f, 14.0f, 1.0f, (ColorRGBA){200, 240, 200, 255});
            draw_text_ui(cmd, ui, "R  next level   ESC  title", 350.0f, 504.0f, 14.0f, 1.0f, (ColorRGBA){180, 255, 180, 255});
        }
    } else if (g_phase == PHASE_DEATH) {
        Vector4 bg = {{0.10f, 0.02f, 0.02f, 1.0f}};
        draw_rect_px(cmd, 0.0f, 0.0f, (float)sw, (float)sh, bg);
        draw_text_center_fit_ui(cmd, ui, "GAME OVER", 480.0f, 132.0f, 28.0f, 1.0f, 900.0f, (ColorRGBA){255, 70, 50, 255});
        {
            char b[128];
            snprintf(b, sizeof(b), "Cause: %s", death_reason_text());
            draw_text_center_fit_ui(cmd, ui, b, 480.0f, 216.0f, 16.0f, 1.0f, 900.0f, (ColorRGBA){255, 210, 180, 255});
            snprintf(b, sizeof(b), "SCORE %06d   HIGH %06d", g_score, g_high_score);
            draw_text_ui(cmd, ui, b, 330.0f, 276.0f, 16.0f, 1.0f, (ColorRGBA){220, 255, 220, 255});
            snprintf(b, sizeof(b), "Spawn %d   survived %02d sec", g_spawn_number, (int)floorf(g_spawn_elapsed));
            draw_text_ui(cmd, ui, b, 355.0f, 330.0f, 14.0f, 1.0f, (ColorRGBA){190, 210, 230, 255});
        }
        draw_text_ui(cmd, ui, "R / SPACE  new hunt   ESC  title", 315.0f, 420.0f, 14.0f, 1.0f, (ColorRGBA){220, 220, 180, 255});
    } else if (g_phase == PHASE_PAUSE) {
        render_world(cmd, sw, sh);
        Vector4 shade = {{0.0f, 0.0f, 0.0f, 0.58f}};
        draw_rect_px(cmd, 0.0f, 0.0f, (float)sw, (float)sh, shade);
        draw_text_center_fit_ui(cmd, ui, "PAUSED", 480.0f, 228.0f, 32.0f, 1.0f, 900.0f, (ColorRGBA){255, 230, 160, 255});
        draw_text_center_fit_ui(cmd, ui, "P / ESC  resume", 480.0f, 300.0f, 15.0f, 1.0f, 900.0f, (ColorRGBA){220, 240, 255, 240});
        draw_text_center_fit_ui(cmd, ui, "R  restart hunt   Q  title", 480.0f, 342.0f, 14.0f, 1.0f, 900.0f, (ColorRGBA){200, 210, 230, 225});
    } else {
        render_world(cmd, sw, sh);
        int hellraiser_active = hellraisers_any_active();
        int hellraiser_warned = hellraisers_any_warned();
        if (hellraiser_warned || hellraiser_active) {
            float danger_pulse = sinf((float)SituationTimerGetTime() * (hellraiser_active ? 18.0f : 9.0f)) * 0.5f + 0.5f;
            Vector4 danger = {{0.55f, 0.02f, 0.0f, (hellraiser_active ? 0.16f : 0.07f) + danger_pulse * 0.08f}};
            draw_rect_px(cmd, 0.0f, 0.0f, (float)sw, (float)sh, danger);
        }
        if (g_pain_flash > 0.0f) {
            Vector4 red = {{0.55f, 0.0f, 0.0f, 0.28f * g_pain_flash}};
            draw_rect_px(cmd, 0.0f, 0.0f, (float)sw, (float)sh, red);
        }
        char hud[128];
        int demons_left = demons_alive_count();
        int drones_left = hunter_drones_alive_count();
        int portals_left = portals_alive_count();
        snprintf(hud, sizeof(hud), "SCORE %06d   HIGH %06d", g_score, g_high_score);
        draw_text_ui(cmd, ui, hud, 12.0f, 10.0f, 15.0f, 1.0f, (ColorRGBA){255, 240, 220, 255});
        snprintf(hud, sizeof(hud), "LEVEL %d/%d   HP %d   AMMO %d   DEMONS %d   DRONES %d   PORTALS %d", g_current_level, LEVEL_COUNT, g_health, g_ammo, demons_left, drones_left, portals_left);
        draw_text_ui(cmd, ui, hud, 12.0f, 32.0f, 13.0f, 1.0f, (ColorRGBA){220, 235, 255, 235});
        draw_text_ui(cmd, ui, current_level_is_gauntlet() ? "Reach the far exit. Do not let them touch you." : "Exit opens when portals are cleared.", 12.0f, 54.0f, 12.0f, 1.0f, (ColorRGBA){200, 200, 255, 220});
        {
            const char* sprite_hud;
            ColorRGBA sprite_col = {130, 210, 150, 240};
            if (!g_sky_ok) {
                if (g_sky_shader.generation != 0 && !g_sky_async_failed) {
                    sprite_hud = "GPU world compiling… playable on CPU (see demon_hunt_sky.log)";
                    sprite_col = (ColorRGBA){200, 220, 255, 240};
                } else {
                    sprite_hud = "World shader off — CPU fallback   see demon_hunt_sky.log";
                    sprite_col = (ColorRGBA){255, 170, 120, 240};
                }
            } else if (shader_sprite_runtime_enabled()) {
                sprite_hud = shader_sprite_phase3_runtime_enabled()
                    ? "Shader world: OK   Sprites: shader (all types)"
                    : "Shader world: OK   Sprites: shader (base)";
            } else {
                sprite_hud = "Shader world: OK   Sprite resolver gated off";
            }
            draw_text_ui(cmd, ui, sprite_hud, 12.0f, 76.0f, 11.0f, 1.0f, sprite_col);
        }
        if (g_shader_sprite_debug_mode != 0) {
            snprintf(hud, sizeof(hud), "Sprite debug mode %d   F8 cycles debug overlays", g_shader_sprite_debug_mode);
            draw_text_ui(cmd, ui, hud, 12.0f, 92.0f, 11.0f, 1.0f, (ColorRGBA){210, 170, 255, 240});
        }
        if (hellraiser_active) {
            char danger_hud[96];
            int active_count = hellraisers_active_count();
            if (active_count < HELLRAISER_COUNT) {
                float remaining = fmaxf(0.0f, g_next_hellraiser_spawn_time - g_spawn_elapsed);
                snprintf(danger_hud, sizeof(danger_hud), "HUNTERS %d/%d - NEXT IN %02d", active_count, HELLRAISER_COUNT, (int)ceilf(remaining));
            } else {
                snprintf(danger_hud, sizeof(danger_hud), "HUNTERS %d/%d RELEASED", active_count, HELLRAISER_COUNT);
            }
            draw_text_ui(cmd, ui, danger_hud, 12.0f, 98.0f, 13.0f, 1.0f, (ColorRGBA){255, 80, 50, 255});
        } else {
            char danger_hud[96];
            float remaining = fmaxf(0.0f, g_next_hellraiser_spawn_time - g_spawn_elapsed);
            ColorRGBA danger_col = hellraiser_warned ? (ColorRGBA){255, 150, 80, 255} : (ColorRGBA){190, 170, 130, 220};
            snprintf(danger_hud, sizeof(danger_hud), "SOMETHING IN %02d", (int)ceilf(remaining));
            draw_text_ui(cmd, ui, danger_hud, 12.0f, 98.0f, 13.0f, 1.0f, danger_col);
        }
        draw_minimap_overlay(cmd, ui);
    }

    if (g_show_fps) {
        char perf[96];
        snprintf(perf, sizeof(perf), "FPS %d   VSYNC %s", SituationGetFPS(),
                 g_vsync_on ? "ON" : "OFF");
        draw_text_fit_ui(cmd, ui, perf, 730.0f, 10.0f, 12.0f, 1.0f, 220.0f, (ColorRGBA){220, 245, 255, 245});
    }
    if (g_screenshot_msg_timer > 0.0f && g_screenshot_msg[0] != '\0') {
        draw_text_ui(cmd, ui, g_screenshot_msg, 12.0f, 572.0f, 12.0f, 1.0f, (ColorRGBA){220, 245, 210, 245});
    }

    if (sky_loading_overlay_active()) {
        float pulse = sinf((float)SituationTimerGetTime() * 6.5f) * 0.5f + 0.5f;
        ColorRGBA load_col = {
            (unsigned char)(185 + pulse * 50.0f),
            (unsigned char)(205 + pulse * 35.0f),
            (unsigned char)(255),
            248
        };
        draw_rect_ui(cmd, ui, 322.0f, 268.0f, 316.0f, 70.0f, (Vector4){{0.02f, 0.03f, 0.07f, 0.74f}});
        draw_text_center_fit_ui(cmd, ui, "LOADING SHADER...", 480.0f, 286.0f, 19.0f, 1.0f, 700.0f, load_col);
        draw_text_center_fit_ui(
            cmd, ui,
            "Compiling world sky pass - gameplay continues on CPU fallback",
            480.0f, 312.0f, 10.5f, 1.0f, 760.0f, (ColorRGBA){188, 204, 235, 235});
    }

    SituationCmdEndRenderPass(cmd);
    if (render_to_game_display) {
        /* Phase 1.5: Copy VD world texture → feedback texture (outside render pass) */
        if (g_feedback_tex_ok && g_sky_ok) {
            feedback_tex_ensure_size(sw, sh);  /* step 1.5.7: defensive resize check */
            SituationTexture vd_tex = {0};
            if (SituationGetVirtualDisplayTexture(g_game_display, &vd_tex) == SITUATION_SUCCESS) {
                SituationTextureBarrierDesc barrier_to_dst = {0};
                barrier_to_dst.old_layout = SITUATION_TEXTURE_LAYOUT_SHADER_READ;
                barrier_to_dst.new_layout = SITUATION_TEXTURE_LAYOUT_TRANSFER_DST;
                SituationCmdTextureBarrier(cmd, g_feedback_tex, &barrier_to_dst);

                SituationTextureCopyRegion copy_region = {0};
                copy_region.src_rect.x = 0;
                copy_region.src_rect.y = 0;
                copy_region.src_rect.width = g_feedback_tex_w;
                copy_region.src_rect.height = g_feedback_tex_h;
                copy_region.dst_x = 0;
                copy_region.dst_y = 0;
                SituationCmdCopyTexture(cmd, vd_tex, g_feedback_tex, &copy_region);

                SituationTextureBarrierDesc barrier_to_read = {0};
                barrier_to_read.old_layout = SITUATION_TEXTURE_LAYOUT_TRANSFER_DST;
                barrier_to_read.new_layout = SITUATION_TEXTURE_LAYOUT_SHADER_READ;
                SituationCmdTextureBarrier(cmd, g_feedback_tex, &barrier_to_read);
            }
        }

        SituationRenderPassInfo screen_pass = SituationRenderPassInfoDefault(-1, (ColorRGBA){0, 0, 0, 255});
        SituationCmdBeginRenderPass(cmd, &screen_pass);
        SituationCmdEndRenderPass(cmd);
        SituationRenderVirtualDisplays(cmd);
    }
    SituationEndFrame();
}

static void demon_hunt_fatal_situation_init(SituationError err) {
    char body[1536];
    char* detail = NULL;

    if (SituationGetLastErrorMsg(&detail) == SITUATION_SUCCESS && detail && detail[0] != '\0') {
        snprintf(body, sizeof(body),
                 "Demon Hunt could not start.\n\n"
                 "Situation failed while initializing graphics, audio, or the window:\n\n"
                 "%s\n\n"
                 "(error code %d)\n\n"
                 "This sample needs OpenGL 4.6 and a working GPU driver. "
                 "Try updating your graphics drivers, then run again.",
                 detail, (int)err);
        SituationFreeString(detail);
    } else {
        snprintf(body, sizeof(body),
                 "Demon Hunt could not start.\n\n"
                 "Situation initialization failed (error code %d).\n\n"
                 "This sample needs OpenGL 4.6 and a working GPU driver. "
                 "Try updating your graphics drivers, then run again.",
                 (int)err);
    }

    fprintf(stderr, "%s\n", body);
    SituationShowMessageBox("Demon Hunt — Could Not Start", body);
}

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Demon Hunt",
        .window_width = 960,
        .window_height = 600,
        .initial_active_window_flags = 0,
    };

    {
        SituationError init_err = SituationInit(argc, argv, &config);
        if (init_err != SITUATION_SUCCESS) {
            demon_hunt_fatal_situation_init(init_err);
            return -1;
        }
    }
    SituationSetMaximizeCallback(demon_hunt_maximize_callback, NULL);
    {
        Vector2 game_res = {{(float)GAME_RENDER_W, (float)GAME_RENDER_H}};
        SituationError vd_err = SituationCreateVirtualDisplay(game_res, 1.0, 0, SITUATION_SCALING_FIT, SITUATION_BLEND_ALPHA, &g_game_display);
        if (vd_err != SITUATION_SUCCESS) {
            g_game_display = -1;
            char* vd_msg = NULL;
            if (SituationGetLastErrorMsg(&vd_msg) == SITUATION_SUCCESS && vd_msg && vd_msg[0]) {
                fprintf(
                    stderr,
                    "[demon_hunt] virtual display unavailable (%s); rendering directly to framebuffer\n",
                    vd_msg);
                SituationFreeString(vd_msg);
            } else {
                fprintf(
                    stderr,
                    "[demon_hunt] virtual display unavailable (code %d); rendering directly to framebuffer\n",
                    (int)vd_err);
            }
        }
    }

    audio_init();
    memset(&g_font, 0, sizeof(g_font));
    score_load_high();
    session_reset(1);
    enter_title_phase();
    g_had_window_focus = SituationHasWindowFocus() ? 1 : 0;

    printf("Demon Hunt — title: Space/Enter begin  I instructions  Esc quit\n");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();
        int has_focus = SituationHasWindowFocus() ? 1 : 0;
        if (fullscreen_toggle_pressed()) {
            demon_hunt_toggle_borderless_presentation();
        }
        int focus_loss_grace = SituationTimerGetTime() < g_ignore_focus_loss_until;
        if (g_phase == PHASE_PLAY && g_had_window_focus && !has_focus && !focus_loss_grace) {
            g_phase = PHASE_PAUSE;
            SituationShowCursor();
            melo_resync();
        }

        float dt = SituationGetFrameTime();
        if (g_screenshot_msg_timer > 0.0f) {
            g_screenshot_msg_timer -= dt;
            if (g_screenshot_msg_timer < 0.0f) {
                g_screenshot_msg_timer = 0.0f;
            }
        }

        if (SituationIsKeyPressed(SIT_KEY_F10)) {
            g_show_fps = !g_show_fps;
        }
        if (SituationIsKeyPressed(SIT_KEY_F8)) {
            g_shader_sprite_debug_mode = (g_shader_sprite_debug_mode + 1) % 3;
        }
        if (SituationIsKeyPressed(SIT_KEY_V)) {
            g_vsync_on = !g_vsync_on;
            SituationSetVSync(g_vsync_on != 0);
        }
        if (SituationIsKeyPressed(SIT_KEY_F12)) {
            queue_screenshot();
        }

        if (g_phase == PHASE_TITLE) {
            int alt_down = SituationIsKeyDown(SIT_KEY_LEFT_ALT) || SituationIsKeyDown(SIT_KEY_RIGHT_ALT);
            if (g_title_show_instructions) {
                if (SituationIsKeyPressed(SIT_KEY_ESCAPE) || SituationIsKeyPressed(SIT_KEY_I) || SituationIsKeyPressed(SIT_KEY_H)) {
                    g_title_show_instructions = 0;
                } else if (SituationIsKeyPressed(SIT_KEY_SPACE) || (SituationIsKeyPressed(SIT_KEY_ENTER) && !alt_down)) {
                    begin_entering(1);
                }
            } else {
                if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
                    break;
                }
                if (SituationIsKeyPressed(SIT_KEY_I) || SituationIsKeyPressed(SIT_KEY_H)) {
                    g_title_show_instructions = 1;
                } else if (SituationIsKeyPressed(SIT_KEY_SPACE) || (SituationIsKeyPressed(SIT_KEY_ENTER) && !alt_down)) {
                    begin_entering(1);
                }
            }
        } else if (g_phase == PHASE_ENTERING) {
            /* Hold on entering panel for ENTERING_MIN_SEC. */
        } else if (g_phase == PHASE_PAUSE) {
            if (has_focus && pause_toggle_pressed()) {
                g_phase = PHASE_PLAY;
                SituationDisableCursor();
                melo_resync();
            } else if (SituationIsKeyPressed(SIT_KEY_Q)) {
                melo_request(MELOK_ABORT);
                enter_title_phase();
            } else if (SituationIsKeyPressed(SIT_KEY_R)) {
                begin_entering(1);
            }
        } else if (g_phase == PHASE_WIN) {
            if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
                enter_title_phase();
            }
            if (SituationIsKeyPressed(SIT_KEY_R)) {
                begin_next_level_or_new_game();
            }
        } else if (g_phase == PHASE_DEATH) {
            if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
                enter_title_phase();
            }
            if (SituationIsKeyPressed(SIT_KEY_R) || SituationIsKeyPressed(SIT_KEY_SPACE) || SituationIsKeyPressed(SIT_KEY_ENTER)) {
                begin_entering(1);
            }
        } else {
            if (pause_toggle_pressed()) {
                g_phase = PHASE_PAUSE;
                SituationShowCursor();
                melo_resync();
            } else {
                update_play(dt);
            }
        }

        if (g_phase != PHASE_PAUSE) {
            melo_update_tick();
        }

        {
            char title[160];
            if (g_phase == PHASE_TITLE) {
                snprintf(title, sizeof(title), "Demon Hunt — %s", g_title_show_instructions ? "instructions" : "title");
            } else if (g_phase == PHASE_ENTERING) {
                snprintf(title, sizeof(title), "Demon Hunt — entering");
            } else if (g_phase == PHASE_WIN) {
                snprintf(title, sizeof(title), "Demon Hunt — level %d cleared", g_current_level);
            } else if (g_phase == PHASE_DEATH) {
                snprintf(title, sizeof(title), "Demon Hunt — game over");
            } else if (g_phase == PHASE_PAUSE) {
                snprintf(title, sizeof(title), "Demon Hunt — paused");
            } else {
                snprintf(title, sizeof(title), "Demon Hunt — level %d  score %d", g_current_level, g_score);
            }
            SituationSetWindowTitle(title);
        }

        if (g_phase == PHASE_ENTERING) {
            int sw_enter = g_game_display >= 0 ? GAME_RENDER_W : SituationGetRenderWidth();
            int sh_enter = g_game_display >= 0 ? GAME_RENDER_H : SituationGetRenderHeight();
            if (!g_entering_sky_init_done) {
                g_entering_sky_init_done = 1;
                entering_panel_rebuild(sw_enter, sh_enter, 0.0f);
                g_entering_start_time = SituationTimerGetTime();
            }
            update_entering(sw_enter, sh_enter);
        }

        if (!g_sky_async_started) {
            sky_begin_async_shader_load();
        }
        sky_poll_async_shader();
        render_frame();
        take_queued_screenshot();
        g_had_window_focus = has_focus;
    }

    audio_destroy_sfx_graph();

    SituationShowCursor();
    entering_panel_destroy();
    shutdown_sky_gpu();
    if (g_game_display >= 0) {
        SituationDestroyVirtualDisplay(g_game_display);
        g_game_display = -1;
    }
    SituationShutdown();
    return 0;
}
