/***************************************************************************************************
 *  Situation — 27: Grid Playfield (stacked grids on a compute VD)
 *
 *  Phase C demo: scrolling tile background grid + static foreground label grid,
 *  composited via SituationGridStackPresent into an integer-scaled Virtual Display.
 *
 *  Phase D: dedicated actor grid — clear + blit each frame (interim until sprites).
 *
 *  Assets: examples/assets/kenney_new-platformer-pack-1.1/ (CC0 — Kenney.nl)
 *
 *  Keys:
 *    A / D or Left / Right — scroll background
 *    Space                 — toggle auto-scroll
 *
 *  Build:
 *    build\build_examples.bat static-opengl  grid_playfield
 *    build\build_examples.bat static-vulkan   grid_playfield
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
    #define SITUATION_USE_OPENGL
#endif

#include "shared/sit_example.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define GRID_COLS  20
#define GRID_ROWS  15
#define CELL_PX    64
#define VD_W       (GRID_COLS * CELL_PX)
#define VD_H       (GRID_ROWS * CELL_PX)

#define ATLAS_COLS 18

/** Kenney spritesheet-tiles-default.png — row-major 64 px cells on a 1169×1169 atlas. */
#define TILE_AT(px, py) ((uint32_t)(((py) / 64) * ATLAS_COLS + ((px) / 64)))

#define TILE_GRASS  TILE_AT(455, 195)
#define TILE_BRICK  TILE_AT(325, 65)
#define TILE_HILL   TILE_AT(715, 195)
#define TILE_COIN   TILE_AT(0, 130)
#define TILE_BUSH   TILE_AT(845, 65)
#define TILE_SIGN   TILE_AT(845, 390)

static const char* const k_tiles_rel =
    "examples/assets/kenney_new-platformer-pack-1.1/Spritesheets/spritesheet-tiles-default.png";

static SituationGridSurface g_bg_grid;
static SituationGridSurface g_actor_grid;
static SituationGridSurface g_collide_grid;
static SituationGridSurface g_fg_grid;
static SituationGridStack   g_stack;
static SituationFont        g_tile_font;
static int                  g_vd_id = -1;
static float                g_scroll_x = 0.0f;
static float                g_actor_x = 4.0f;
static float                g_actor_dir = 1.0f;
static int                  g_auto_scroll = 1;

#define ENTITY_COLS 1
#define ENTITY_ROWS 2
#define ACTOR_PROBE_ID 1u

static SitGridCell g_entity_cells[ENTITY_COLS * ENTITY_ROWS];

static const uint32_t k_sky_bg   = SIT_GRID_COLOR_RGBA8(135, 206, 235, 255);
static const uint32_t k_dirt_bg  = SIT_GRID_COLOR_RGBA8(92, 64, 51, 255);
static const uint32_t k_clear_fg = SIT_GRID_COLOR_TRANSPARENT;
static const uint32_t k_white_fg = SIT_GRID_COLOR_RGBA8(255, 255, 255, 255);
static const uint32_t k_grass_fg = SIT_GRID_COLOR_RGBA8(76, 175, 80, 255);
static const uint32_t k_brick_fg = SIT_GRID_COLOR_RGBA8(160, 82, 45, 255);
static const uint32_t k_hill_fg  = SIT_GRID_COLOR_RGBA8(56, 142, 60, 255);
static const uint32_t k_coin_fg  = SIT_GRID_COLOR_RGBA8(255, 215, 0, 255);
static const uint32_t k_label_fg = SIT_GRID_COLOR_RGBA8(255, 255, 255, 255);
static const uint32_t k_label_bg = SIT_GRID_COLOR_RGBA8(0, 0, 0, 180);

static const uint32_t k_actor_fg = SIT_GRID_COLOR_RGBA8(255, 87, 34, 255);

static SitGridCell cell(uint32_t code, uint32_t fg, uint32_t bg) {
    return SIT_GRID_CELL(code, fg, bg);
}

static bool try_load_tiles(const char* path) {
    SituationTexture sheet = {0};
    if (SituationLoadTexture(path, false, &sheet) != SITUATION_SUCCESS) {
        return false;
    }
    if (SituationLoadBitmapFontFromTexture(sheet, CELL_PX, CELL_PX, 0, &g_tile_font) != SITUATION_SUCCESS) {
        return false;
    }
    printf("[27] Loaded tile atlas: %s\n", path);
    return true;
}

static bool load_tile_atlas(void) {
    static const char* prefixes[] = {
        "",
        "../",
        "../../",
        "../../../",
    };
    char path[512];
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], k_tiles_rel);
        if (try_load_tiles(path)) {
            return true;
        }
    }
    fprintf(stderr, "[27] Could not load Kenney tile sheet. Run from repo root (see README).\n");
    return false;
}

static int ground_height_at(int col) {
    (void)col;
    return 9;
}

static void fill_background_grid(void) {
    for (int y = 0; y < GRID_ROWS; ++y) {
        for (int x = 0; x < GRID_COLS; ++x) {
            int ground = ground_height_at(x);
            SitGridCell c = cell(0u, k_clear_fg, k_sky_bg);

            if (y < ground - 2) {
                c = cell(0u, k_clear_fg, k_sky_bg);
            } else if (y == ground - 2) {
                if ((x % 7) == 3) {
                    c = cell(TILE_BUSH, k_grass_fg, k_clear_fg);
                } else if ((x % 11) == 5) {
                    c = cell(TILE_HILL, k_hill_fg, k_clear_fg);
                } else {
                    c = cell(TILE_GRASS, k_grass_fg, k_clear_fg);
                }
            } else if (y == ground - 1) {
                c = cell(TILE_BRICK, k_brick_fg, k_clear_fg);
            } else {
                c = cell(TILE_BRICK, k_brick_fg, k_dirt_bg);
            }

            if (y == ground - 2 && (x % 5) == 2) {
                c = cell(TILE_COIN, k_coin_fg, k_clear_fg);
            }

            SituationGridSetCell(g_bg_grid, x, y, c);
        }
    }

    SituationGridSetCell(g_bg_grid, 2, ground_height_at(2) - 2, cell(TILE_SIGN, k_white_fg, k_clear_fg));
}

static void fill_foreground_grid(void) {
    for (int y = 0; y < GRID_ROWS; ++y) {
        for (int x = 0; x < GRID_COLS; ++x) {
            SituationGridSetCell(g_fg_grid, x, y, cell(0u, k_clear_fg, k_clear_fg));
        }
    }

    static const char* title = "27 GRID PLAYFIELD";
    int tx = 1;
    for (const char* p = title; *p; ++p) {
        SituationGridSetCell(g_fg_grid, tx++, 0,
            cell((uint32_t)(unsigned char)*p, k_label_fg, k_label_bg));
    }

    static const char* hint = "A/D scroll  Space auto";
    tx = 1;
    for (const char* p = hint; *p; ++p) {
        SituationGridSetCell(g_fg_grid, tx++, 1,
            cell((uint32_t)(unsigned char)*p, k_label_fg, SIT_GRID_COLOR_RGBA8(0, 0, 0, 120)));
    }

    SituationGridSetCell(g_fg_grid, GRID_COLS - 2, 0, cell(TILE_COIN, k_coin_fg, k_clear_fg));
    SituationGridSetRole(g_fg_grid, SIT_GRID_ROLE_UI);
}

static void fill_collision_grid(void) {
    const SitGridCell empty = cell(0u, k_clear_fg, k_clear_fg);
    const SitGridCell solid = cell(1u, 0u, SIT_GRID_COLOR_RGBA8(0, 0, 0, 255));

    for (int y = 0; y < GRID_ROWS; ++y) {
        for (int x = 0; x < GRID_COLS; ++x) {
            int ground = ground_height_at(x);
            SitGridCell c = empty;
            if (x == 0 || x == GRID_COLS - 1 || y >= ground - 1) {
                c = solid;
            }
            SituationGridSetCell(g_collide_grid, x, y, c);
        }
    }
    SituationGridSetRole(g_collide_grid, SIT_GRID_ROLE_COLLISION);
}

static int actor_ground_y(int ix) {
    int ground = ground_height_at(ix);
    int iy = ground - 1 - ENTITY_ROWS;
    if (iy < 0) iy = 0;
    return iy;
}

static void init_entity_template(void) {
    g_entity_cells[0] = cell(TILE_GRASS, k_actor_fg, k_clear_fg);
    g_entity_cells[1] = cell(TILE_BRICK, k_actor_fg, k_clear_fg);
}

static void update_actor_grid(void) {
    SitGridCell clear_cell = cell(0u, k_clear_fg, k_clear_fg);
    SituationGridClear(g_actor_grid, clear_cell);

    int ix = (int)(g_actor_x + 0.5f);
    if (ix < 0) ix = 0;
    if (ix >= GRID_COLS - ENTITY_COLS) ix = GRID_COLS - ENTITY_COLS - 1;

    int iy = actor_ground_y(ix);

    SituationGridBlitCells(g_actor_grid, ix, iy, g_entity_cells, ENTITY_COLS, ENTITY_ROWS);
}

static void resolve_actor_collision(SituationCommandBuffer cmd) {
    int ix = (int)(g_actor_x + 0.5f);
    if (ix < 0) ix = 0;
    if (ix >= GRID_COLS - ENTITY_COLS) ix = GRID_COLS - ENTITY_COLS - 1;
    int iy = actor_ground_y(ix);

    SitGridCollisionProbe probe = {
        .probe_id = ACTOR_PROBE_ID,
        .x = g_actor_x,
        .y = (float)iy,
        .w = (float)ENTITY_COLS,
        .h = (float)ENTITY_ROWS,
    };
    if (SituationGridSetCollisionProbe(g_stack, probe) != SITUATION_SUCCESS) {
        return;
    }

    SitGridCollisionEvent events[8];
    int hit_count = 0;
    if (SituationGridTestCollision(cmd, g_stack, ACTOR_PROBE_ID, events, 8, &hit_count) != SITUATION_SUCCESS) {
        return;
    }

    for (int i = 0; i < hit_count; ++i) {
        uint32_t norm = events[i].normal_flags;
        if (norm & SIT_GRID_COLLISION_NORM_LEFT) {
            g_actor_x = events[i].touch_x - (float)ENTITY_COLS - 0.01f;
            g_actor_dir = 1.0f;
        } else if (norm & SIT_GRID_COLLISION_NORM_RIGHT) {
            g_actor_x = events[i].touch_x + 0.01f;
            g_actor_dir = -1.0f;
        }
    }
}

static void update_actor_motion(SituationCommandBuffer cmd, float dt) {
    const float speed = 3.0f;
    g_actor_x += g_actor_dir * speed * dt;
    resolve_actor_collision(cmd);
    update_actor_grid();
}

static bool init_playfield(void) {
    if (!load_tile_atlas()) {
        return false;
    }

    g_bg_grid = SituationGridCreate(GRID_COLS, GRID_ROWS, CELL_PX, CELL_PX);
    g_actor_grid = SituationGridCreate(GRID_COLS, GRID_ROWS, CELL_PX, CELL_PX);
    g_collide_grid = SituationGridCreate(GRID_COLS, GRID_ROWS, CELL_PX, CELL_PX);
    g_fg_grid = SituationGridCreate(GRID_COLS, GRID_ROWS, CELL_PX, CELL_PX);
    if (!g_bg_grid || !g_actor_grid || !g_collide_grid || !g_fg_grid) {
        fprintf(stderr, "[27] SituationGridCreate failed\n");
        return false;
    }

    SituationGridSetFont(g_bg_grid, g_tile_font);
    SituationGridSetFont(g_actor_grid, g_tile_font);
    /* FG labels use the built-in default VGA font (centered inside 64 px cells). */

    fill_background_grid();
    fill_collision_grid();
    fill_foreground_grid();
    init_entity_template();
    update_actor_grid();

    g_stack = SituationGridStackCreate();
    if (!g_stack) {
        fprintf(stderr, "[27] SituationGridStackCreate failed\n");
        return false;
    }
    if (SituationGridStackAddGrid(g_stack, g_bg_grid, 0) != SITUATION_SUCCESS ||
        SituationGridStackAddGrid(g_stack, g_actor_grid, 1) != SITUATION_SUCCESS ||
        SituationGridStackAddGrid(g_stack, g_fg_grid, 2) != SITUATION_SUCCESS ||
        SituationGridStackAddGrid(g_stack, g_collide_grid, 3) != SITUATION_SUCCESS) {
        fprintf(stderr, "[27] SituationGridStackAddGrid failed\n");
        return false;
    }

    SituationError err = SituationCreateVirtualDisplayEx(
        (Vector2){{(float)VD_W, (float)VD_H}},
        1.0, 0,
        SITUATION_SCALING_INTEGER,
        SITUATION_BLEND_NONE,
        SITUATION_VD_FLAG_COMPUTE_TARGET,
        &g_vd_id);
    if (err != SITUATION_SUCCESS || g_vd_id < 0) {
        fprintf(stderr, "[27] CreateVirtualDisplayEx (compute target) failed\n");
        return false;
    }

    printf("[27] Playfield VD %dx%d (%d×%d cells @ %d px)\n", VD_W, VD_H, GRID_COLS, GRID_ROWS, CELL_PX);
    return true;
}

static void shutdown_playfield(void) {
    if (g_vd_id >= 0) {
        SituationDestroyVirtualDisplay(g_vd_id);
        g_vd_id = -1;
    }
    if (g_stack) {
        SituationGridStackDestroy(g_stack);
        g_stack = NULL;
    }
    if (g_fg_grid) {
        SituationGridDestroy(g_fg_grid);
        g_fg_grid = NULL;
    }
    if (g_actor_grid) {
        SituationGridDestroy(g_actor_grid);
        g_actor_grid = NULL;
    }
    if (g_collide_grid) {
        SituationGridDestroy(g_collide_grid);
        g_collide_grid = NULL;
    }
    if (g_bg_grid) {
        SituationGridDestroy(g_bg_grid);
        g_bg_grid = NULL;
    }
}

static void handle_input(float dt) {
    float speed = 4.0f;
    if (SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT)) {
        speed = 12.0f;
    }

    if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
        g_auto_scroll = !g_auto_scroll;
    }

    if (SituationIsKeyDown(SIT_KEY_A) || SituationIsKeyDown(SIT_KEY_LEFT)) {
        g_scroll_x -= speed * dt;
    }
    if (SituationIsKeyDown(SIT_KEY_D) || SituationIsKeyDown(SIT_KEY_RIGHT)) {
        g_scroll_x += speed * dt;
    }

    if (g_auto_scroll) {
        g_scroll_x += 1.5f * dt;
    }

    if (g_scroll_x < 0.0f) {
        g_scroll_x += (float)GRID_COLS;
    }
    if (g_scroll_x >= (float)GRID_COLS) {
        g_scroll_x -= (float)GRID_COLS;
    }

    SituationGridSetScroll(g_bg_grid, g_scroll_x, 0.0f);
}

/** Windowed: integer 1:1 when the window matches the VD. Fullscreen (F11): FIT to fill the screen. */
static void update_playfield_vd_scaling(void) {
    if (g_vd_id < 0) {
        return;
    }
    static int s_last_fullscreen = -1;
    int fullscreen = SituationIsWindowState(SITUATION_FLAG_BORDERLESS_WINDOWED_MODE) ? 1 : 0;
    if (fullscreen == s_last_fullscreen) {
        return;
    }
    s_last_fullscreen = fullscreen;
    SituationScalingMode mode = fullscreen ? SITUATION_SCALING_FIT : SITUATION_SCALING_INTEGER;
    SituationSetVirtualDisplayScalingMode(g_vd_id, mode);
}

int main(int argc, char** argv) {
    if (SitExample_Init(argc, argv, "27 — Grid Playfield") != SITUATION_SUCCESS) {
        return -1;
    }

    if (!init_playfield()) {
        SitExample_Shutdown();
        return -1;
    }

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) {
            break;
        }

        update_playfield_vd_scaling();

        if (SituationIsAppPaused()) {
            continue;
        }

        float dt = (float)SituationGetFrameTime();
        handle_input(dt);

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            continue;
        }

        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        update_actor_motion(cmd, dt);

        if (SituationGridStackPresent(cmd, g_stack, g_vd_id) != SITUATION_SUCCESS) {
            static int s_present_err_logged = 0;
            if (!s_present_err_logged) {
                char* err_msg = NULL;
                if (SituationGetLastErrorMsg(&err_msg) == SITUATION_SUCCESS && err_msg) {
                    fprintf(stderr, "[27] GridStackPresent failed: %s\n", err_msg);
                    SituationFreeString(err_msg);
                }
                s_present_err_logged = 1;
            }
        }

        SituationRenderPassInfo main_rp = {0};
        main_rp.display_id = -1;
        main_rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        main_rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        main_rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
        main_rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        main_rp.depth_attachment.clear.depth = 1.0f;

        SituationCmdBeginRenderPass(cmd, &main_rp);
        SituationRenderVirtualDisplays(cmd);
        SitExample_DrawHUD(cmd,
            "27 — Grid Playfield",
            "A/D: scroll BG  Space: auto-scroll  — actor + collision grid (Kenney tiles)");
        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    shutdown_playfield();
    SitExample_Shutdown();
    return 0;
}
