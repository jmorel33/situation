/***************************************************************************************************
 *  Situation — 2D side-scrolling platformer (keyboard-only, Mario-style toy level)
 *
 *  Run / jump on bricks, collect coins, stomp red “goombas”, reach the flag.
 *
 *  Title: Space / Enter — start    Esc — quit
 *  Play: A D or arrows — move    Space W Up — jump (short coyote after leaving a ledge)    R — skip death wait
 *  Game over / win: Space Enter or R — back to title
 *  BGM: 3-channel pattern + temporal oscillator clock (see OSC_BGM_CLOCK / BGM_BPM). Parallax clouds + wind drift.
 ***************************************************************************************************/

#if defined(_WIN32)
    #define NOMINMAX
#endif

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "situation.h"
#include <cglm/cglm.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TILE 32.0f
#define CLOUD_PARALLAX 0.34f
#define CLOUD_LAYER_DRIFT 18.0f
#define COYOTE_MAX_FRAMES 8
#define PHYS_SUBSTEPS 8

#define STARTING_LIVES 3
#define DEATH_SEQUENCE_FRAMES 72
#define PLAT_COUNT 13
#define COIN_COUNT 12
#define ENEMY_COUNT 4

typedef struct {
    float x, y, w, h;
} Rect;

typedef struct {
    float x, y;
    float vx;
    int alive;
} Enemy;

static Rect g_plat[PLAT_COUNT];
static float g_coin_x[COIN_COUNT];
static float g_coin_y[COIN_COUNT];
static int g_coin_taken[COIN_COUNT];
static Enemy g_enemy[ENEMY_COUNT];

static float g_spawn_x;
static float g_spawn_y;
static float g_flag_x, g_flag_y, g_flag_w, g_flag_h;

static float g_px, g_py, g_vx, g_vy;
static int g_facing;
static int g_on_ground;
static int g_score;
static int g_lives;
static int g_deaths;
static int g_dead_timer;
static int g_death_sfx_armed;
static int g_coyote_frames;
static float g_cloud_wind;

typedef enum {
    PHASE_TITLE,
    PHASE_PLAY,
    PHASE_DEAD,
    PHASE_WIN,
    PHASE_GAMEOVER
} GamePhase;
static GamePhase g_phase = PHASE_TITLE;

static SituationFont g_font;

static const float PW = 26.0f;
static const float PH = 40.0f;

/*
 * Procedural BGM — OSC clock = one quarter note. Four layers: noise kick, bass (square),
 * harmony (sine, mid register), lead (triangle). Three tracks: title loop, gameplay loop,
 * win one-shot ditty (no wrap).
 */
#define OSC_BGM_CLOCK 42
#define BGM_REST 0

#define BGM_BPM_TITLE 102
#define BGM_BPM_GAME 112
#define BGM_BPM_WIN 112

#define BGM_TITLE_LEN 16
#define BGM_GAME_LEN 32
#define BGM_WIN_LEN 14

typedef enum {
    BGM_MODE_OFF = 0,
    BGM_MODE_TITLE,
    BGM_MODE_GAME,
    BGM_MODE_WIN
} BgmMode;

/* --- Title (menu) — short bright loop --- */
static const uint8_t g_bgm_title_lead[BGM_TITLE_LEN] = {
    60, 64, 67, 72,
    71, 67, 64, 60,
    62, 65, 69, 74,
    72, 69, 67, 64,
};
static const uint8_t g_bgm_title_bass[BGM_TITLE_LEN] = {
    48, 48, 48, 48,
    43, 43, 43, 43,
    41, 41, 41, 41,
    48, 48, 48, 48,
};
static const uint8_t g_bgm_title_harm[BGM_TITLE_LEN] = {
    55, 59, 64, 67,
    67, 64, 60, 57,
    58, 62, 65, 69,
    67, 65, 62, 59,
};

/* --- Gameplay — full 8-bar line + harmony pad --- */
static const uint8_t g_bgm_game_lead[BGM_GAME_LEN] = {
    60, 67, 69, 71,
    72, 71, 69, 67,
    64, 67, 71, 74,
    72, 69, 67, 64,
    62, 64, 67, 71,
    69, 67, 65, 64,
    62, 64, 67, BGM_REST,
    60, 64, 67, 72,
};
static const uint8_t g_bgm_game_bass[BGM_GAME_LEN] = {
    48, 48, 48, 48,
    48, 48, 48, 48,
    40, 40, 40, 40,
    48, 48, 48, 48,
    43, 43, 43, 43,
    41, 41, 41, 41,
    38, 38, 38, 38,
    48, 48, 48, 48,
};
static const uint8_t g_bgm_game_harm[BGM_GAME_LEN] = {
    55, 64, 67, 67,
    67, 67, 67, 64,
    60, 64, 67, 71,
    71, 67, 64, 60,
    57, 59, 64, 67,
    64, 64, 62, 59,
    59, 59, 64, BGM_REST,
    55, 59, 64, 67,
};

/* --- Course clear — one-shot victory ditty (does not loop) --- */
static const uint8_t g_bgm_win_lead[BGM_WIN_LEN] = {
    72, 74, 76, 79, 76, 74, 72,
    71, 72, 74, 76, 84, 79, 72,
};
static const uint8_t g_bgm_win_bass[BGM_WIN_LEN] = {
    48, 48, 48, 48, 48, 48, 48,
    43, 43, 43, 43, 36, 43, 48,
};
static const uint8_t g_bgm_win_harm[BGM_WIN_LEN] = {
    60, 62, 64, 67, 64, 62, 60,
    59, 60, 62, 64, 71, 67, 64,
};

static int g_bgm_running;
static BgmMode g_bgm_mode;
static uint32_t g_bgm_step_ix;
/* Last oscillator trigger_count we have turned into pattern rows (robust vs. even number of flips in one frame). */
static uint64_t g_bgm_sync_trigger_ix;

/* Situation's default node graph patches a tone synth into the mixer — silence it; procedural SFX still mix via the tone pool. */
static void audio_init_for_game(void) {
    SituationSetActiveGraph(NULL);
    (void)SituationResumeAudioDevice();
}

static void bgm_note(uint8_t midi, SituationWaveType wave, float vol, float pan, float sustain_level, float hold_sec) {
    if (midi == BGM_REST || midi >= 128) {
        return;
    }
    SituationPlayToneEx(
        wave,
        SITUATION_MIDI_NOTE_FREQUENCY[midi],
        vol,
        pan,
        0.008f,
        0.045f,
        sustain_level,
        0.08f,
        hold_sec);
}

/* Percussion layer — noise reads clearly as a separate voice vs sine/square/triangle tones. */
static void bgm_kick(float step_sec, int win_jingle_row) {
    float dur = step_sec * (win_jingle_row ? 0.26f : 0.38f);
    SituationPlayToneEx(SIT_WAVE_NOISE, 220.0f, 0.078f, 0.0f, 0.001f, 0.055f, 0.0f, dur, 0.003f);
}

static void bgm_stop(void) {
    g_bgm_running = 0;
    g_bgm_mode = BGM_MODE_OFF;
}

static void bgm_play_pattern_row(void) {
    if (!g_bgm_running) {
        return;
    }
    if (g_bgm_mode == BGM_MODE_WIN && g_bgm_step_ix >= BGM_WIN_LEN) {
        bgm_stop();
        return;
    }

    uint32_t len = 0;
    const uint8_t *lead = NULL;
    const uint8_t *bass = NULL;
    const uint8_t *harm = NULL;
    float bpm = (float)BGM_BPM_GAME;

    switch (g_bgm_mode) {
        case BGM_MODE_TITLE:
            len = BGM_TITLE_LEN;
            lead = g_bgm_title_lead;
            bass = g_bgm_title_bass;
            harm = g_bgm_title_harm;
            bpm = (float)BGM_BPM_TITLE;
            break;
        case BGM_MODE_GAME:
            len = BGM_GAME_LEN;
            lead = g_bgm_game_lead;
            bass = g_bgm_game_bass;
            harm = g_bgm_game_harm;
            bpm = (float)BGM_BPM_GAME;
            break;
        case BGM_MODE_WIN:
            len = BGM_WIN_LEN;
            lead = g_bgm_win_lead;
            bass = g_bgm_win_bass;
            harm = g_bgm_win_harm;
            bpm = (float)BGM_BPM_WIN;
            break;
        default:
            return;
    }

    uint32_t i;
    if (g_bgm_mode == BGM_MODE_WIN) {
        i = g_bgm_step_ix;
        g_bgm_step_ix++;
    } else {
        i = g_bgm_step_ix % len;
        g_bgm_step_ix++;
    }

    float step_sec = 60.0f / bpm;
    float hold_melody = step_sec * 0.90f;
    float hold_harm = step_sec * 0.82f;
    float hold_bass = step_sec * 0.66f;

    int win_row = (g_bgm_mode == BGM_MODE_WIN);
    if (win_row) {
        bgm_kick(step_sec, 1);
    } else if ((i % 4u) == 0u || (i % 4u) == 2u) {
        bgm_kick(step_sec, 0);
    }

    bgm_note(bass[i], SIT_WAVE_SQUARE, 0.070f, -0.14f, 0.44f, hold_bass);
    bgm_note(harm[i], SIT_WAVE_SINE, 0.068f, 0.16f, 0.48f, hold_harm);
    bgm_note(lead[i], SIT_WAVE_TRIANGLE, 0.086f, 0.0f, 0.54f, hold_melody);
}

static void bgm_start_mode(BgmMode mode) {
    if (mode == BGM_MODE_OFF) {
        bgm_stop();
        return;
    }
    bgm_stop();
    g_bgm_mode = mode;
    double bpm = (double)BGM_BPM_GAME;
    if (mode == BGM_MODE_TITLE) {
        bpm = (double)BGM_BPM_TITLE;
    } else if (mode == BGM_MODE_WIN) {
        bpm = (double)BGM_BPM_WIN;
    } else if (mode == BGM_MODE_GAME) {
        bpm = (double)BGM_BPM_GAME;
    }
    SituationSetTimerOscillatorPeriod(OSC_BGM_CLOCK, 60.0 / bpm);
    g_bgm_step_ix = 0;
    g_bgm_running = 1;
    g_bgm_sync_trigger_ix = SituationTimerGetOscillatorTriggerCount(OSC_BGM_CLOCK);
    bgm_play_pattern_row();
}

static void bgm_update_tick(void) {
    if (!g_bgm_running) {
        return;
    }
    uint64_t triggers = SituationTimerGetOscillatorTriggerCount(OSC_BGM_CLOCK);
    while (g_bgm_sync_trigger_ix < triggers) {
        bgm_play_pattern_row();
        g_bgm_sync_trigger_ix++;
    }
}

/* Short procedural one-shots (legacy tone pool — works with active graph disabled). */
static void sfx_jump(void) {
    SituationPlayToneEx(SIT_WAVE_SQUARE, 380.0f, 0.12f, 0.0f, 0.002f, 0.05f, 0.0f, 0.08f, 0.02f);
}
static void sfx_coin(void) {
    SituationPlayToneEx(SIT_WAVE_SINE, 880.0f, 0.14f, 0.0f, 0.001f, 0.02f, 0.0f, 0.12f, 0.04f);
    SituationPlayToneEx(SIT_WAVE_SINE, 1320.0f, 0.1f, 0.0f, 0.001f, 0.02f, 0.0f, 0.1f, 0.05f);
}
static void sfx_stomp(void) {
    SituationPlayToneEx(SIT_WAVE_SQUARE, 140.0f, 0.18f, 0.0f, 0.001f, 0.08f, 0.0f, 0.1f, 0.02f);
}
static void sfx_hurt(void) {
    SituationPlayToneEx(SIT_WAVE_SQUARE, 180.0f, 0.32f, 0.0f, 0.001f, 0.06f, 0.0f, 0.12f, 0.04f);
    SituationPlayToneEx(SIT_WAVE_SQUARE, 120.0f, 0.28f, 0.0f, 0.001f, 0.1f, 0.0f, 0.18f, 0.02f);
}
static void sfx_win(void) {
    SituationPlayToneEx(SIT_WAVE_TRIANGLE, 523.0f, 0.12f, 0.0f, 0.005f, 0.05f, 0.0f, 0.2f, 0.08f);
    SituationPlayToneEx(SIT_WAVE_TRIANGLE, 784.0f, 0.1f, 0.0f, 0.005f, 0.05f, 0.0f, 0.25f, 0.12f);
}

/* Obvious "you died" sting — longer and louder than generic hurt. */
static void sfx_death(void) {
    SituationPlayToneEx(SIT_WAVE_NOISE, 200.0f, 0.45f, 0.0f, 0.002f, 0.15f, 0.0f, 0.25f, 0.12f);
    SituationPlayToneEx(SIT_WAVE_SQUARE, 90.0f, 0.38f, 0.0f, 0.001f, 0.2f, 0.0f, 0.35f, 0.05f);
    SituationPlayToneEx(SIT_WAVE_SQUARE, 55.0f, 0.35f, 0.0f, 0.001f, 0.25f, 0.0f, 0.4f, 0.08f);
}

static void level_reset(void) {
    int i = 0;
    /* Ground */
    g_plat[i++] = (Rect){0.0f, 520.0f, 3200.0f, 200.0f};
    /* Ledges & blocks */
    g_plat[i++] = (Rect){320.0f, 440.0f, 4.0f * TILE, TILE};
    g_plat[i++] = (Rect){520.0f, 380.0f, 3.0f * TILE, TILE};
    g_plat[i++] = (Rect){700.0f, 320.0f, 2.0f * TILE, TILE};
    g_plat[i++] = (Rect){900.0f, 400.0f, 5.0f * TILE, TILE};
    g_plat[i++] = (Rect){1200.0f, 340.0f, 2.0f * TILE, TILE};
    g_plat[i++] = (Rect){1400.0f, 280.0f, 4.0f * TILE, TILE};
    g_plat[i++] = (Rect){1750.0f, 360.0f, 3.0f * TILE, TILE};
    g_plat[i++] = (Rect){2000.0f, 300.0f, 2.0f * TILE, TILE};
    g_plat[i++] = (Rect){2200.0f, 420.0f, 6.0f * TILE, TILE};
    g_plat[i++] = (Rect){2600.0f, 360.0f, 2.0f * TILE, TILE};
    g_plat[i++] = (Rect){2750.0f, 300.0f, 3.0f * TILE, TILE};
    g_plat[i++] = (Rect){2950.0f, 440.0f, 4.0f * TILE, TILE};

    g_spawn_x = 80.0f;
    g_spawn_y = 420.0f;
    g_px = g_spawn_x;
    g_py = g_spawn_y;
    g_vx = 0.0f;
    g_vy = 0.0f;
    g_facing = 1;
    g_on_ground = 0;
    g_dead_timer = 0;
    g_death_sfx_armed = 0;

    g_flag_x = 3050.0f;
    g_flag_y = 520.0f - 160.0f;
    g_flag_w = 24.0f;
    g_flag_h = 160.0f;

    float cx[] = {
        360, 400, 440, 480,
        750, 790,
        1280, 1320,
        1820, 1860,
        2340, 2380
    };
    float cy[] = {
        400, 400, 400, 400,
        280, 280,
        300, 300,
        320, 320,
        380, 380
    };
    for (int c = 0; c < COIN_COUNT; c++) {
        g_coin_x[c] = cx[c];
        g_coin_y[c] = cy[c];
        g_coin_taken[c] = 0;
    }

    g_enemy[0] = (Enemy){550.0f, 520.0f - 36.0f, -70.0f, 1};
    g_enemy[1] = (Enemy){1100.0f, 520.0f - 36.0f, 70.0f, 1};
    g_enemy[2] = (Enemy){1850.0f, 520.0f - 36.0f, -70.0f, 1};
    g_enemy[3] = (Enemy){2450.0f, 520.0f - 36.0f, 70.0f, 1};
}

static void session_new_game(void) {
    g_score = 0;
    g_lives = STARTING_LIVES;
    g_deaths = 0;
    g_phase = PHASE_PLAY;
    level_reset();
    g_coyote_frames = COYOTE_MAX_FRAMES;
    bgm_start_mode(BGM_MODE_GAME);
}

static int rects_overlap(float ax, float ay, float aw, float ah, const Rect* b) {
    const float eps = 1.25f;
    return ax < b->x + b->w && ax + aw > b->x && ay < b->y + b->h && ay + ah > b->y - eps;
}

/* Horizontal overlap of player feet with platform top (not center-only — avoids “lost ledge” at edges). */
static int feet_span_over_platform(float px, float pw, const Rect* b) {
    const float inset = 0.5f;
    float pl = px + inset;
    float pr = px + pw - inset;
    return pr > b->x && pl < b->x + b->w;
}

static int feet_on_platform_top(float px, float pw, float feet_y, const Rect* b, float tol_below, float tol_above) {
    if (!feet_span_over_platform(px, pw, b)) {
        return 0;
    }
    return feet_y <= b->y + tol_above && feet_y >= b->y - tol_below;
}

/*
 * While feet stay at or above this platform's deck (standing or jumping straight up), never treat
 * its vertical sides as walls — otherwise the first jump frame clears "on surface" and nx snaps off the ledge.
 * Below the deck (same height as ground beside a wall), sides still block.
 */
static int skip_platform_side_collision(float nx, float py, float pw, float ph, const Rect* b) {
    float feet = py + ph;
    if (!feet_span_over_platform(nx, pw, b)) {
        return 0;
    }
    return feet <= b->y + 18.0f;
}

static void draw_rect_pixels(SituationCommandBuffer cmd, float x, float y, float w, float h, Vector4 color) {
    mat4 m;
    glm_mat4_identity(m);
    glm_translate(m, (vec3){x, y, 0.0f});
    glm_scale(m, (vec3){w, h, 1.0f});
    SituationCmdDrawQuad(cmd, m, color);
}

static void draw_world_rect(SituationCommandBuffer cmd, float wx, float wy, float ww, float wh, float cam_x, float cam_y, Vector4 color) {
    draw_rect_pixels(cmd, wx - cam_x, wy - cam_y, ww, wh, color);
}

static void draw_clouds_world(SituationCommandBuffer cmd, float cam_x, float wind_off) {
    Vector4 c_light = {{1.0f, 1.0f, 1.0f, 0.88f}};
    Vector4 c_shadow = {{0.88f, 0.93f, 1.0f, 0.76f}};
    float cam_eff = cam_x * CLOUD_PARALLAX;
    float wmod = wind_off * 0.28f;
    for (int i = 0; i < 15; i++) {
        float wx = -480.0f + (float)i * 318.0f + wmod;
        wx -= floorf(wx / 2900.0f) * 2900.0f;
        float wy = 22.0f + (float)(i % 5) * 36.0f + sinf((float)i * 2.17f + wind_off * 0.012f) * 16.0f;
        float bw = 88.0f + (float)(i % 5) * 34.0f;
        draw_world_rect(cmd, wx, wy, bw, 34.0f, cam_eff, 0.0f, c_shadow);
        draw_world_rect(cmd, wx + bw * 0.14f, wy - 14.0f, bw * 0.5f, 28.0f, cam_eff, 0.0f, c_light);
        draw_world_rect(cmd, wx + bw * 0.38f, wy + 7.0f, bw * 0.46f, 22.0f, cam_eff, 0.0f, c_light);
    }
}

static void draw_clouds_title(SituationCommandBuffer cmd, int sw, int sh) {
    Vector4 c_light = {{1.0f, 1.0f, 1.0f, 0.82f}};
    Vector4 c_shadow = {{0.88f, 0.93f, 1.0f, 0.72f}};
    double t = SituationTimerGetTime();
    float scroll = (float)(t * 36.0) + g_cloud_wind;
    for (int i = 0; i < 14; i++) {
        float px = fmodf(scroll + (float)i * 238.0f, (float)sw + 500.0f) - 180.0f;
        float py = 28.0f + (float)(i % 5) * ((float)sh * 0.06f);
        float bw = 85.0f + (float)(i % 4) * 32.0f;
        draw_rect_pixels(cmd, px, py, bw, 32.0f, c_shadow);
        draw_rect_pixels(cmd, px + bw * 0.12f, py - 12.0f, bw * 0.52f, 26.0f, c_light);
        draw_rect_pixels(cmd, px + bw * 0.4f, py + 8.0f, bw * 0.45f, 20.0f, c_light);
    }
}

static void solve_axis_x(float* px, float* py, float pw, float ph, float* vx, float dt) {
    float nx = *px + *vx * dt;
    if (*vx == 0.0f) {
        *px = nx;
        return;
    }
    for (int p = 0; p < PLAT_COUNT; p++) {
        const Rect* b = &g_plat[p];
        /*
         * Never treat a surface we're standing on as a left/right wall (full-width ground included).
         * Must use the *horizontal destination* nx — otherwise feet_span flickers at edges and nx snaps.
         */
        if (skip_platform_side_collision(nx, *py, pw, ph, b)) {
            continue;
        }
        if (rects_overlap(nx, *py, pw, ph, b)) {
            if (*vx > 0.0f) {
                nx = b->x - pw;
            } else {
                nx = b->x + b->w;
            }
            *vx = 0.0f;
            break;
        }
    }
    *px = nx;
}

static void begin_player_death(void);

static void solve_axis_y(float* px, float* py, float pw, float ph, float* vy, float dt, int* landed) {
    float ny = *py + *vy * dt;
    *landed = 0;

    if (*vy > 0.0f) {
        /*
         * Ledges first (p>=1), then ground — avoids snapping to the floor when still over a platform.
         * Highest surface wins inside each group (minimum b->y).
         */
        float feet_n = ny + ph;
        int best = -1;
        float best_top_y = 1.0e9f;

        for (int pass = 0; pass < 2 && best < 0; pass++) {
            int p0 = (pass == 0) ? 1 : 0;
            int p1 = (pass == 0) ? PLAT_COUNT : 1;
            best_top_y = 1.0e9f;
            for (int p = p0; p < p1; p++) {
                const Rect* b = &g_plat[p];
                if (!rects_overlap(*px, ny, pw, ph, b)) {
                    continue;
                }
                if (!feet_span_over_platform(*px, pw, b)) {
                    continue;
                }
                if (feet_n < b->y - 22.0f || feet_n > b->y + 30.0f) {
                    continue;
                }
                if (b->y < best_top_y) {
                    best_top_y = b->y;
                    best = p;
                }
            }
        }

        if (best >= 0) {
            const Rect* b = &g_plat[best];
            ny = b->y - ph;
            *vy = 0.0f;
            *landed = 1;
        }
    } else if (*vy < 0.0f) {
        /*
         * Head/ceiling vs platform bottoms. Ground already avoided treating its slab as a sky —
         * ledges need the same: on the first frames of a jump the AABB still overlaps the brick,
         * and without this we "bonk" b->y+h and spawn under the block, then fall to the floor.
         * Skip any underside hit when feet are still at/near that surface's top (rising off the deck).
         * Real hits from below have feet well under the deck (feet_y >> b->y in +y-down space).
         */
        int best = -1;
        float best_bottom = -1.0e9f;
        for (int p = 0; p < PLAT_COUNT; p++) {
            const Rect* b = &g_plat[p];
            if (!rects_overlap(*px, ny, pw, ph, b)) {
                continue;
            }
            float feet = ny + ph;
            if (feet <= b->y + 14.0f) {
                continue;
            }
            float bot = b->y + b->h;
            if (bot > best_bottom) {
                best_bottom = bot;
                best = p;
            }
        }
        if (best >= 0) {
            const Rect* b = &g_plat[best];
            ny = b->y + b->h;
            *vy = 0.0f;
        }
    }
    *py = ny;
}

static void begin_player_death(void) {
    if (g_phase != PHASE_PLAY) {
        return;
    }
    bgm_stop();
    if (g_lives > 0) {
        g_lives--;
    }
    g_deaths++;
    g_phase = PHASE_DEAD;
    g_dead_timer = DEATH_SEQUENCE_FRAMES;
    g_death_sfx_armed = 1;
    g_vx = 0.0f;
    g_vy = 0.0f;
}

static void update_game(float dt) {
    /* Avoid huge integration steps after stalls / focus loss (would tunnel or spike velocity). */
    if (dt > 0.05f) {
        dt = 0.05f;
    }
    if (dt <= 0.0f) {
        dt = 1.0f / 60.0f;
    }

    const float GRAVITY = 2400.0f;
    const float MOVE_ACCEL = 2200.0f;
    const float MAX_RUN = 300.0f;
    const float FRICTION_GROUND = 2000.0f;
    const float FRICTION_AIR = 400.0f;
    const float JUMP_VEL = -720.0f;
    const float LEVEL_W = 3200.0f;

    if (g_phase == PHASE_TITLE) {
        bgm_update_tick();
        if (SituationIsKeyPressed(SIT_KEY_SPACE) || SituationIsKeyPressed(SIT_KEY_ENTER)) {
            session_new_game();
        }
        g_cloud_wind += dt * CLOUD_LAYER_DRIFT * 0.6f;
        SituationSetWindowTitle("Plumber — Space / Enter to play");
        return;
    }

    if (g_phase == PHASE_GAMEOVER) {
        if (SituationIsKeyPressed(SIT_KEY_SPACE) || SituationIsKeyPressed(SIT_KEY_ENTER) || SituationIsKeyPressed(SIT_KEY_R)) {
            bgm_start_mode(BGM_MODE_TITLE);
            g_phase = PHASE_TITLE;
        }
        SituationSetWindowTitle("Game over — Space / Enter for title");
        return;
    }

    if (g_phase == PHASE_WIN) {
        bgm_update_tick();
        g_cloud_wind += dt * CLOUD_LAYER_DRIFT * 0.4f;
        if (SituationIsKeyPressed(SIT_KEY_SPACE) || SituationIsKeyPressed(SIT_KEY_ENTER) || SituationIsKeyPressed(SIT_KEY_R)) {
            bgm_start_mode(BGM_MODE_TITLE);
            g_phase = PHASE_TITLE;
        }
        SituationSetWindowTitle("You cleared the course! — Space for title");
        return;
    }

    if (g_phase == PHASE_DEAD) {
        if (g_death_sfx_armed) {
            sfx_death();
            g_death_sfx_armed = 0;
        }
        g_dead_timer--;
        if (g_dead_timer <= 0 || SituationIsKeyPressed(SIT_KEY_R)) {
            g_dead_timer = 0;
            if (g_lives <= 0) {
                g_phase = PHASE_GAMEOVER;
            } else {
                g_px = g_spawn_x;
                g_py = g_spawn_y;
                g_vx = 0.0f;
                g_vy = 0.0f;
                g_phase = PHASE_PLAY;
                g_coyote_frames = COYOTE_MAX_FRAMES;
                bgm_start_mode(BGM_MODE_GAME);
            }
        }
        SituationSetWindowTitle("Plumber — respawning…");
        return;
    }

    /* PHASE_PLAY */
    g_cloud_wind += dt * CLOUD_LAYER_DRIFT;
    bgm_update_tick();

    if (SituationIsKeyDown(SIT_KEY_ESCAPE)) {
        /* optional */
    }

    float wish = 0.0f;
    if (SituationIsKeyDown(SIT_KEY_A) || SituationIsKeyDown(SIT_KEY_LEFT)) {
        wish -= 1.0f;
        g_facing = -1;
    }
    if (SituationIsKeyDown(SIT_KEY_D) || SituationIsKeyDown(SIT_KEY_RIGHT)) {
        wish += 1.0f;
        g_facing = 1;
    }

    float fric = g_on_ground ? FRICTION_GROUND : FRICTION_AIR;
    if (wish != 0.0f) {
        g_vx += wish * MOVE_ACCEL * dt;
        if (g_vx > MAX_RUN) g_vx = MAX_RUN;
        if (g_vx < -MAX_RUN) g_vx = -MAX_RUN;
    } else {
        if (g_vx > 0.0f) {
            g_vx -= fric * dt;
            if (g_vx < 0.0f) g_vx = 0.0f;
        } else if (g_vx < 0.0f) {
            g_vx += fric * dt;
            if (g_vx > 0.0f) g_vx = 0.0f;
        }
    }

    {
        int jump_ok = g_on_ground || g_coyote_frames > 0;
        if ((SituationIsKeyPressed(SIT_KEY_SPACE) || SituationIsKeyPressed(SIT_KEY_W) || SituationIsKeyPressed(SIT_KEY_UP)) && jump_ok) {
            g_vy = JUMP_VEL;
            g_on_ground = 0;
            g_coyote_frames = 0;
            sfx_jump();
        }
    }

    g_vy += GRAVITY * dt;

    g_on_ground = 0;
    int substeps = PHYS_SUBSTEPS;
    float sdt = dt / (float)substeps;
    for (int s = 0; s < substeps; s++) {
        int landed = 0;
        solve_axis_y(&g_px, &g_py, PW, PH, &g_vy, sdt, &landed);
        solve_axis_x(&g_px, &g_py, PW, PH, &g_vx, sdt);
        if (landed) {
            g_on_ground = 1;
        }
    }

    /* vy==0 while resting does not set "landed" — re-detect ground for jump / friction. */
    if (!g_on_ground && g_vy >= -40.0f) {
        float feet = g_py + PH;
        for (int p = 0; p < PLAT_COUNT; p++) {
            const Rect* b = &g_plat[p];
            if (feet_on_platform_top(g_px, PW, feet, b, 8.0f, 10.0f)) {
                g_on_ground = 1;
                break;
            }
        }
    }

    if (g_on_ground) {
        g_coyote_frames = COYOTE_MAX_FRAMES;
    } else if (g_coyote_frames > 0) {
        g_coyote_frames--;
    }

    if (g_px < 0.0f) {
        g_px = 0.0f;
        g_vx = 0.0f;
    }
    if (g_px > LEVEL_W - PW) {
        g_px = LEVEL_W - PW;
        g_vx = 0.0f;
    }

    /* Coins */
    for (int c = 0; c < COIN_COUNT; c++) {
        if (g_coin_taken[c]) continue;
        float cx = g_coin_x[c] - 10.0f;
        float cy = g_coin_y[c] - 10.0f;
        if (rects_overlap(g_px, g_py, PW, PH, &(Rect){cx, cy, 20.0f, 20.0f})) {
            g_coin_taken[c] = 1;
            g_score += 100;
            sfx_coin();
        }
    }

    /* Enemies patrol on ground line */
    const float EW = 34.0f;
    const float EH = 34.0f;
    const float ground_y = 520.0f - EH;

    for (int e = 0; e < ENEMY_COUNT; e++) {
        if (!g_enemy[e].alive) continue;
        Enemy* en = &g_enemy[e];
        en->x += en->vx * dt;
        if (en->x < 200.0f || en->x > 3000.0f) {
            en->vx = -en->vx;
        }
        /* Reverse if no solid a few pixels under the front foot */
        float front_x = en->vx < 0.0f ? en->x - 4.0f : en->x + EW + 4.0f;
        float probe_y = en->y + EH + 4.0f;
        int supported = 0;
        for (int p = 0; p < PLAT_COUNT; p++) {
            if (front_x >= g_plat[p].x && front_x <= g_plat[p].x + g_plat[p].w && probe_y >= g_plat[p].y && probe_y <= g_plat[p].y + g_plat[p].h) {
                supported = 1;
                break;
            }
        }
        if (!supported) {
            en->vx = -en->vx;
        }

        en->y = ground_y;

        if (rects_overlap(g_px, g_py, PW, PH, &(Rect){en->x, en->y, EW, EH})) {
            /*
             * While moving upward (jump arc), AABB can still overlap a goomba without a stomp.
             * Stomp requires vy > 0, so we'd wrongly take hurt and respawn — felt like "jump resets me".
             */
            if (g_vy < 0.0f) {
                continue;
            }
            int stomp = g_vy > 120.0f && (g_py + PH) < en->y + EH * 0.55f;
            if (stomp) {
                en->alive = 0;
                g_vy = -380.0f;
                g_score += 200;
                sfx_stomp();
            } else {
                begin_player_death();
            }
        }
    }

    if (rects_overlap(g_px, g_py, PW, PH, &(Rect){g_flag_x, g_flag_y, g_flag_w, g_flag_h})) {
        bgm_start_mode(BGM_MODE_WIN);
        g_phase = PHASE_WIN;
        g_score += 500;
        sfx_win();
        {
            char title[160];
            snprintf(title, sizeof(title), "You cleared the course! Score %d", g_score);
            SituationSetWindowTitle(title);
        }
        return;
    }

    if (g_py > 800.0f) {
        begin_player_death();
    }

    {
        char title[160];
        snprintf(title, sizeof(title), "Plumber — score %d  lives %d  deaths %d", g_score, g_lives, g_deaths);
        SituationSetWindowTitle(title);
    }
}

static void draw_player_screen(SituationCommandBuffer cmd, float sx, float sy, float sc) {
    Vector4 blue = {{0.15f, 0.35f, 0.85f, 1.0f}};
    Vector4 red = {{0.9f, 0.15f, 0.12f, 1.0f}};
    Vector4 skin = {{0.96f, 0.78f, 0.62f, 1.0f}};
    Vector4 capc = {{0.85f, 0.12f, 0.1f, 1.0f}};
    draw_rect_pixels(cmd, sx, sy + 22.0f * sc, PW * sc, 18.0f * sc, blue);
    draw_rect_pixels(cmd, sx, sy + 10.0f * sc, PW * sc, 14.0f * sc, red);
    draw_rect_pixels(cmd, sx + (g_facing > 0 ? 14.0f : 4.0f) * sc, sy + 4.0f * sc, 12.0f * sc, 12.0f * sc, skin);
    draw_rect_pixels(cmd, sx + 2.0f * sc, sy, (PW - 4.0f) * sc, 10.0f * sc, capc);
}

static void draw_hud(SituationCommandBuffer cmd) {
    char line[120];
    snprintf(line, sizeof(line), "SCORE %d    LIVES %d    DEATHS %d", g_score, g_lives, g_deaths);
    SituationCmdDrawTextEx(cmd, g_font, line, (Vector2){{10.0f, 8.0f}}, 14.0f, 1.0f, (ColorRGBA){255, 255, 255, 255});
}

/* Stroke drawn first so title/menu lines stay readable when bright clouds drift behind. */
static void draw_title_text_outlined(SituationCommandBuffer cmd, const char* text, Vector2 pos, float font_size, float spacing,
                                     ColorRGBA fg, float stroke_px) {
    const ColorRGBA stroke = {28, 42, 72, 255};
    static const float ox[8] = {-1.0f, 0.0f, 1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 1.0f};
    static const float oy[8] = {-1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    float s = stroke_px;
    for (int i = 0; i < 8; i++) {
        SituationCmdDrawTextEx(
            cmd,
            g_font,
            text,
            (Vector2){{pos.x + ox[i] * s, pos.y + oy[i] * s}},
            font_size,
            spacing,
            stroke);
    }
    SituationCmdDrawTextEx(cmd, g_font, text, pos, font_size, spacing, fg);
}

static void render_frame(void) {
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        return;
    }

    int sw = SituationGetRenderWidth();
    int sh = SituationGetRenderHeight();
    if (sw < 1) {
        sw = 1;
    }
    if (sh < 1) {
        sh = 1;
    }

    float cam_x = g_px - (float)sw * 0.35f;
    if (cam_x < 0.0f) {
        cam_x = 0.0f;
    }
    if (cam_x > 3200.0f - (float)sw) {
        cam_x = 3200.0f - (float)sw;
    }
    const float cam_y = 0.0f;

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
    SituationRenderPassInfo pass = {
        .display_id = -1,
        .color_attachment = {.loadOp = SIT_LOAD_OP_CLEAR, .clear = {.color = {118, 186, 255, 255}}}};

    SituationCmdBeginRenderPass(cmd, &pass);

    if (g_phase == PHASE_TITLE) {
        draw_clouds_title(cmd, sw, sh);
        Vector4 hill = {{0.35f, 0.62f, 0.28f, 1.0f}};
        for (int h = 0; h < 6; h++) {
            draw_rect_pixels(cmd, -40.0f + (float)h * 180.0f, (float)sh * 0.45f, 200.0f, 140.0f, hill);
        }
        g_facing = 1;
        draw_player_screen(cmd, (float)sw * 0.5f - PW * 1.75f, (float)sh * 0.28f, 3.5f);
        /* Text last so soft-command order draws it above quads (same pass). */
        draw_title_text_outlined(cmd, "PLUMBER DEMO", (Vector2){{(float)sw * 0.5f - 140.0f, 36.0f}}, 22.0f, 1.0f, (ColorRGBA){255, 255, 255, 255}, 2.5f);
        draw_title_text_outlined(cmd, "Situation sample", (Vector2){{(float)sw * 0.5f - 110.0f, 72.0f}}, 14.0f, 1.0f, (ColorRGBA){240, 240, 200, 255}, 2.0f);
        draw_title_text_outlined(cmd, "SPACE or ENTER — start", (Vector2){{(float)sw * 0.5f - 150.0f, 110.0f}}, 16.0f, 1.0f, (ColorRGBA){255, 220, 64, 255}, 2.0f);
        draw_title_text_outlined(cmd, "ESC — quit", (Vector2){{(float)sw * 0.5f - 60.0f, 140.0f}}, 12.0f, 1.0f, (ColorRGBA){200, 200, 220, 255}, 2.0f);
    } else if (g_phase == PHASE_GAMEOVER) {
        draw_rect_pixels(cmd, 0.0f, 0.0f, (float)sw, (float)sh, (Vector4){{0.12f, 0.08f, 0.14f, 0.92f}});
        SituationCmdDrawTextEx(cmd, g_font, "GAME OVER", (Vector2){{(float)sw * 0.5f - 100.0f, (float)sh * 0.25f}}, 28.0f, 1.0f, (ColorRGBA){255, 80, 80, 255});
        {
            char b[96];
            snprintf(b, sizeof(b), "Final score %d   Total deaths %d", g_score, g_deaths);
            SituationCmdDrawTextEx(cmd, g_font, b, (Vector2){{(float)sw * 0.5f - 180.0f, (float)sh * 0.38f}}, 14.0f, 1.0f, (ColorRGBA){220, 220, 240, 255});
        }
        SituationCmdDrawTextEx(cmd, g_font, "SPACE / ENTER / R — title", (Vector2){{(float)sw * 0.5f - 150.0f, (float)sh * 0.52f}}, 14.0f, 1.0f, (ColorRGBA){180, 255, 180, 255});
    } else {
        /* In-game world (PLAY, DEAD, WIN) */
        draw_clouds_world(cmd, cam_x, g_cloud_wind);
        Vector4 hill = {{0.35f, 0.62f, 0.28f, 1.0f}};
        for (int h = 0; h < 5; h++) {
            float hx = -200.0f + (float)h * 700.0f;
            draw_world_rect(cmd, hx, 380.0f, 400.0f, 200.0f, cam_x, cam_y, hill);
        }

        Vector4 brick_top = {{0.72f, 0.38f, 0.18f, 1.0f}};
        Vector4 brick_body = {{0.55f, 0.28f, 0.12f, 1.0f}};
        for (int p = 0; p < PLAT_COUNT; p++) {
            const Rect* b = &g_plat[p];
            draw_world_rect(cmd, b->x, b->y, b->w, b->h, cam_x, cam_y, brick_body);
            draw_world_rect(cmd, b->x, b->y, b->w, 6.0f, cam_x, cam_y, brick_top);
        }

        Vector4 gold = {{0.95f, 0.82f, 0.15f, 1.0f}};
        for (int c = 0; c < COIN_COUNT; c++) {
            if (g_coin_taken[c]) {
                continue;
            }
            draw_world_rect(cmd, g_coin_x[c] - 10.0f, g_coin_y[c] - 10.0f, 20.0f, 20.0f, cam_x, cam_y, gold);
        }

        Vector4 pole = {{0.85f, 0.85f, 0.88f, 1.0f}};
        Vector4 cloth = {{0.15f, 0.75f, 0.2f, 1.0f}};
        draw_world_rect(cmd, g_flag_x, g_flag_y, 8.0f, g_flag_h, cam_x, cam_y, pole);
        draw_world_rect(cmd, g_flag_x + 8.0f, g_flag_y, 40.0f, 48.0f, cam_x, cam_y, cloth);

        Vector4 goom = {{0.78f, 0.2f, 0.15f, 1.0f}};
        Vector4 eye = {{0.95f, 0.95f, 0.95f, 1.0f}};
        for (int e = 0; e < ENEMY_COUNT; e++) {
            if (!g_enemy[e].alive) {
                continue;
            }
            float ex = g_enemy[e].x;
            float ey = g_enemy[e].y;
            draw_world_rect(cmd, ex, ey, 34.0f, 34.0f, cam_x, cam_y, goom);
            draw_world_rect(cmd, ex + 6.0f, ey + 10.0f, 8.0f, 8.0f, cam_x, cam_y, eye);
            draw_world_rect(cmd, ex + 20.0f, ey + 10.0f, 8.0f, 8.0f, cam_x, cam_y, eye);
        }

        float sx = g_px - cam_x;
        float sy = g_py - cam_y;
        draw_player_screen(cmd, sx, sy, 1.0f);

        if (g_phase == PHASE_WIN) {
            draw_rect_pixels(cmd, 0.0f, 0.0f, (float)sw, (float)sh, (Vector4){{0.05f, 0.15f, 0.08f, 0.55f}});
            SituationCmdDrawTextEx(cmd, g_font, "COURSE CLEAR!", (Vector2){{(float)sw * 0.5f - 140.0f, (float)sh * 0.35f}}, 24.0f, 1.0f, (ColorRGBA){255, 255, 120, 255});
            SituationCmdDrawTextEx(cmd, g_font, "SPACE / R — title", (Vector2){{(float)sw * 0.5f - 110.0f, (float)sh * 0.5f}}, 14.0f, 1.0f, (ColorRGBA){220, 255, 220, 255});
            draw_hud(cmd);
        } else if (g_phase == PHASE_DEAD) {
            float pulse = 0.22f + 0.12f * (float)((DEATH_SEQUENCE_FRAMES - g_dead_timer) % 10 > 5);
            draw_rect_pixels(cmd, 0.0f, 0.0f, (float)sw, (float)sh, (Vector4){{0.35f, 0.0f, 0.02f, pulse}});
            SituationCmdDrawTextEx(cmd, g_font, "YOU DIED", (Vector2){{(float)sw * 0.5f - 88.0f, (float)sh * 0.38f}}, 26.0f, 1.0f, (ColorRGBA){255, 220, 220, 255});
            SituationCmdDrawTextEx(cmd, g_font, "R — skip", (Vector2){{(float)sw * 0.5f - 48.0f, (float)sh * 0.48f}}, 12.0f, 1.0f, (ColorRGBA){255, 200, 200, 255});
            draw_hud(cmd);
        } else if (g_phase == PHASE_PLAY) {
            draw_hud(cmd);
        }
    }

    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}

int main(int argc, char** argv) {
    SituationInitInfo config = {
        .window_title = "Situation — Plumber platformer demo",
        .window_width = 960,
        .window_height = 540,
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        fprintf(stderr, "SituationInit failed\n");
        return -1;
    }

    audio_init_for_game();

    memset(&g_font, 0, sizeof(g_font));
    g_phase = PHASE_TITLE;
    bgm_start_mode(BGM_MODE_TITLE);
    printf("Plumber platformer — title: Space/Enter  play: arrows + Space jump  Esc quit\n");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }
        update_game(SituationGetFrameTime());
        render_frame();
    }

    bgm_stop();
    SituationShutdown();
    return 0;
}
