/**
 * @file test_grid.c
 * @brief Grid subsystem harness — cell dispatch, stack composite, VD present (Phase B/C).
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include "sit_graphics_test_helpers.h"
#include "sit_test_text_helpers.h"
#include "sit_test_retro_font_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_grid_init_ok = false;

static void grid_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_GRID");
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    config.render_thread_count = 1;
#endif
    SituationError err = SituationInit(0, NULL, &config);
    g_grid_init_ok = (err == SITUATION_SUCCESS);
    if (!g_grid_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void grid_teardown(void) {
    if (g_grid_init_ok) {
        SituationShutdown();
        g_grid_init_ok = false;
    }
}

static bool grid_begin_frame(SituationCommandBuffer* out_cmd) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) return false;
    *out_cmd = SituationGetMainCommandBuffer();
    return *out_cmd != NULL;
}

static SitGridCell grid_make_cell(uint32_t code, uint32_t fg, uint32_t bg) {
    return SIT_GRID_CELL(code, fg, bg);
}

static void grid_fill_checkerboard(SituationGridSurface grid, int cols, int rows) {
    const uint32_t black = 0xFF000000u;
    const uint32_t white = 0xFFFFFFFFu;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            uint32_t bg = (((x + y) & 1) != 0) ? white : black;
            SitGridCell cell = grid_make_cell(' ', 0u, bg);
            SIT_ASSERT_EQ(SituationGridSetCell(grid, x, y, cell), SITUATION_SUCCESS);
        }
    }
}

static void grid_fill_solid(SituationGridSurface grid, int cols, int rows, uint32_t bg) {
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            SitGridCell cell = grid_make_cell(0u, 0u, bg);
            SIT_ASSERT_EQ(SituationGridSetCell(grid, x, y, cell), SITUATION_SUCCESS);
        }
    }
}

static bool grid_pixel_near(uint8_t v, uint8_t expected, int tol) {
    int d = (int)v - (int)expected;
    if (d < 0) d = -d;
    return d <= tol;
}

static int grid_vd_center_y(int vd_height, int cell_y, int cell_px) {
    int sample_y = cell_y * cell_px + cell_px / 2;
    return vd_height - 1 - sample_y;
}

static bool grid_read_vd_pixel(int vd_id, int x, int y, uint8_t* r, uint8_t* g, uint8_t* b) {
    SituationTexture tex = {0};
    if (SituationGetVirtualDisplayTexture(vd_id, &tex) != SITUATION_SUCCESS) return false;
    SituationImage img = {0};
    if (SituationReadTextureAlloc(tex, NULL, &img) != SITUATION_SUCCESS) return false;
    if (!img.data || x < 0 || y < 0 || x >= img.width || y >= img.height) {
        SituationUnloadImage(img);
        return false;
    }
    const uint8_t* px = img.data + ((size_t)y * (size_t)img.width + (size_t)x) * 4u;
    *r = px[0];
    *g = px[1];
    *b = px[2];
    SituationUnloadImage(img);
    return true;
}

static int grid_create_compute_vd(int cols, int rows, int cell, int* out_vd_id) {
    Vector2 resolution = {{(float)(cols * cell), (float)(rows * cell)}};
    return SituationCreateVirtualDisplayEx(
        resolution, 1.0, 0, SITUATION_SCALING_INTEGER, SITUATION_BLEND_NONE,
        SITUATION_VD_FLAG_COMPUTE_TARGET, out_vd_id);
}

/** Stack: red bottom grid + full green top grid — readback verifies z-order composite. */
static void test_grid_stack_two_layer(void) {
    const int cols = 4;
    const int rows = 4;
    const int cell = 8;
    const uint32_t red = SIT_GRID_COLOR_RGBA8(255, 0, 0, 255);
    const uint32_t green = SIT_GRID_COLOR_RGBA8(0, 255, 0, 255);

    SituationGridSurface back = SituationGridCreate(cols, rows, cell, cell);
    SituationGridSurface front = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(back);
    SIT_ASSERT_NOT_NULL(front);
    grid_fill_solid(back, cols, rows, red);
    grid_fill_solid(front, cols, rows, green);

    SituationGridStack stack = SituationGridStackCreate();
    SIT_ASSERT_NOT_NULL(stack);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, back, 0), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, front, 1), SITUATION_SUCCESS);

    int vd_id = -1;
    SIT_ASSERT_EQ(grid_create_compute_vd(cols, rows, cell, &vd_id), SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));
    SIT_ASSERT_EQ(SituationGridStackPresent(cmd, stack, vd_id), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    uint8_t r = 0, g = 0, b = 0;
    SIT_ASSERT(grid_read_vd_pixel(vd_id, 2 * cell + 4, 2 * cell + 4, &r, &g, &b));
    SIT_ASSERT(g > 200 && r < 80);

    /* Transparent top layer passes red through from the bottom grid. */
    grid_fill_solid(front, cols, rows, SIT_GRID_COLOR_TRANSPARENT);
    SIT_ASSERT(grid_begin_frame(&cmd));
    SIT_ASSERT_EQ(SituationGridStackPresent(cmd, stack, vd_id), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SIT_ASSERT(grid_read_vd_pixel(vd_id, 2, 2, &r, &g, &b));
    SIT_ASSERT(r > 200 && g < 80);

    SituationGridStackDestroy(stack);
    SituationGridDestroy(front);
    SituationGridDestroy(back);
    SituationDestroyVirtualDisplay(vd_id);
}

/** Horizontal scroll shifts solid column on a single grid. */
static void test_grid_stack_scroll(void) {
    const int cols = 4;
    const int rows = 2;
    const int cell = 8;
    const uint32_t blue = SIT_GRID_COLOR_RGBA8(0, 0, 255, 255);

    SituationGridSurface grid = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(grid);
    grid_fill_solid(grid, cols, rows, SIT_GRID_COLOR_TRANSPARENT);
    for (int y = 0; y < rows; ++y) {
        SIT_ASSERT_EQ(SituationGridSetCell(grid, 0, y, grid_make_cell(0u, 0u, blue)), SITUATION_SUCCESS);
    }
    SIT_ASSERT_EQ(SituationGridSetScroll(grid, 1.0f, 0.0f), SITUATION_SUCCESS);

    int vd_id = -1;
    SIT_ASSERT_EQ(grid_create_compute_vd(cols, rows, cell, &vd_id), SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));
    SIT_ASSERT_EQ(SituationGridPresent(cmd, grid, vd_id, SIT_GRID_PASS_CELL_ONLY), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    uint8_t r = 0, g = 0, b = 0;
    SIT_ASSERT(grid_read_vd_pixel(vd_id, cell + 2, cell / 2, &r, &g, &b));
    SIT_ASSERT(b > 200 && r < 80);

    SituationGridDestroy(grid);
    SituationDestroyVirtualDisplay(vd_id);
}

/** Collision grid in stack is not blended to the target. */
static void test_grid_stack_skips_collision_role(void) {
    const int cols = 2;
    const int rows = 2;
    const int cell = 8;
    const uint32_t yellow = SIT_GRID_COLOR_RGBA8(255, 255, 0, 255);

    SituationGridSurface visual = SituationGridCreate(cols, rows, cell, cell);
    SituationGridSurface collide = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(visual);
    SIT_ASSERT_NOT_NULL(collide);
    grid_fill_solid(visual, cols, rows, SIT_GRID_COLOR_RGBA8(0, 0, 0, 255));
    grid_fill_solid(collide, cols, rows, yellow);
    SIT_ASSERT_EQ(SituationGridSetRole(collide, SIT_GRID_ROLE_COLLISION), SITUATION_SUCCESS);

    SituationGridStack stack = SituationGridStackCreate();
    SIT_ASSERT_NOT_NULL(stack);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, visual, 0), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, collide, 1), SITUATION_SUCCESS);

    int vd_id = -1;
    SIT_ASSERT_EQ(grid_create_compute_vd(cols, rows, cell, &vd_id), SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));
    SIT_ASSERT_EQ(SituationGridStackPresent(cmd, stack, vd_id), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    uint8_t r = 0, g = 0, b = 0;
    SIT_ASSERT(grid_read_vd_pixel(vd_id, 4, 4, &r, &g, &b));
    SIT_ASSERT(r < 32 && g < 32 && b < 32);

    SituationGridStackDestroy(stack);
    SituationGridDestroy(collide);
    SituationGridDestroy(visual);
    SituationDestroyVirtualDisplay(vd_id);
}

/** 4x4 @ 8px — SituationGridPresent succeeds on compute-target VD. */
static void test_grid_cell_checkerboard(void) {
    const int cols = 4;
    const int rows = 4;
    const int cell = 8;
    const int px = cols * cell;

    SituationGridSurface grid = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(grid);
    grid_fill_checkerboard(grid, cols, rows);

    int vd_id = -1;
    Vector2 resolution = {{(float)px, (float)px}};
    SituationError err = SituationCreateVirtualDisplayEx(
        resolution, 1.0, 0, SITUATION_SCALING_INTEGER, SITUATION_BLEND_NONE,
        SITUATION_VD_FLAG_COMPUTE_TARGET, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));
    SIT_ASSERT_EQ(SituationGridPresent(cmd, grid, vd_id, SIT_GRID_PASS_CELL_ONLY), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    /* Center of (0,0) black and (1,0) white — GL texture readback is bottom-left origin. */
    uint8_t r = 0, g = 0, b = 0;
    int cy = grid_vd_center_y(px, 0, cell);
    SIT_ASSERT(grid_read_vd_pixel(vd_id, cell / 2, cy, &r, &g, &b));
    SIT_ASSERT(grid_pixel_near(r, 0, 16) && grid_pixel_near(g, 0, 16) && grid_pixel_near(b, 0, 16));
    SIT_ASSERT(grid_read_vd_pixel(vd_id, cell + cell / 2, cy, &r, &g, &b));
    SIT_ASSERT(grid_pixel_near(r, 255, 16) && grid_pixel_near(g, 255, 16) && grid_pixel_near(b, 255, 16));

    SituationGridDestroy(grid);
    SituationDestroyVirtualDisplay(vd_id);
}

/** Present path bumps VD content-update metadata. */
static void test_grid_vd_present(void) {
    const int cols = 2;
    const int rows = 2;
    const int cell = 8;
    const int px = cols * cell;

    SituationGridSurface grid = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(grid);
    grid_fill_checkerboard(grid, cols, rows);

    int vd_id = -1;
    Vector2 resolution = {{(float)px, (float)px}};
    SituationError err = SituationCreateVirtualDisplayEx(
        resolution, 1.0, 0, SITUATION_SCALING_INTEGER, SITUATION_BLEND_NONE,
        SITUATION_VD_FLAG_COMPUTE_TARGET, &vd_id);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));
    SIT_ASSERT_EQ(SituationGridPresent(cmd, grid, vd_id, SIT_GRID_PASS_CELL_ONLY), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    double seconds_since = -1.0;
    err = SituationGetVirtualDisplayUpdateInfo(vd_id, NULL, NULL, NULL, &seconds_since);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(seconds_since >= 0.0 && seconds_since < 0.5);

    SituationGridDestroy(grid);
    SituationDestroyVirtualDisplay(vd_id);
}

/** Actor grid between BG and FG: opaque stamp composites over bottom layer. */
static void test_grid_actor_over_tiles(void) {
    const int cols = 4;
    const int rows = 4;
    const int cell = 8;
    const uint32_t red = SIT_GRID_COLOR_RGBA8(255, 0, 0, 255);
    const uint32_t blue = SIT_GRID_COLOR_RGBA8(0, 0, 255, 255);
    const SitGridCell clear_cell = grid_make_cell(0u, 0u, SIT_GRID_COLOR_TRANSPARENT);

    SituationGridSurface back = SituationGridCreate(cols, rows, cell, cell);
    SituationGridSurface actor = SituationGridCreate(cols, rows, cell, cell);
    SituationGridSurface front = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(back);
    SIT_ASSERT_NOT_NULL(actor);
    SIT_ASSERT_NOT_NULL(front);

    grid_fill_solid(back, cols, rows, red);
    SIT_ASSERT_EQ(SituationGridClear(actor, clear_cell), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(
        SituationGridSetCell(actor, 2, 2, grid_make_cell(0u, 0u, blue)),
        SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGridClear(front, clear_cell), SITUATION_SUCCESS);

    SituationGridStack stack = SituationGridStackCreate();
    SIT_ASSERT_NOT_NULL(stack);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, back, 0), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, actor, 1), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, front, 2), SITUATION_SUCCESS);

    int vd_id = -1;
    SIT_ASSERT_EQ(grid_create_compute_vd(cols, rows, cell, &vd_id), SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));
    SIT_ASSERT_EQ(SituationGridStackPresent(cmd, stack, vd_id), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    uint8_t r = 0, g = 0, b = 0;
    int cy = grid_vd_center_y(cols * cell, 2, cell);
    SIT_ASSERT(grid_read_vd_pixel(vd_id, 2 * cell + 4, cy, &r, &g, &b));
    SIT_ASSERT(b > 200 && r < 80);

    cy = grid_vd_center_y(cols * cell, 0, cell);
    SIT_ASSERT(grid_read_vd_pixel(vd_id, 4, cy, &r, &g, &b));
    SIT_ASSERT(r > 200 && b < 80);

    SituationGridStackDestroy(stack);
    SituationGridDestroy(front);
    SituationGridDestroy(actor);
    SituationGridDestroy(back);
    SituationDestroyVirtualDisplay(vd_id);
}

/** BlitCells stamps a source block; Clear resets the layer. */
static void test_grid_blit_and_clear(void) {
    const int cols = 4;
    const int rows = 2;
    const int cell = 8;
    const uint32_t green = SIT_GRID_COLOR_RGBA8(0, 255, 0, 255);
    const SitGridCell clear_cell = grid_make_cell(0u, 0u, SIT_GRID_COLOR_TRANSPARENT);

    SituationGridSurface grid = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(grid);
    SIT_ASSERT_EQ(SituationGridClear(grid, clear_cell), SITUATION_SUCCESS);

    SitGridCell stamp[2] = {
        grid_make_cell(0u, 0u, green),
        grid_make_cell(0u, 0u, green),
    };
    SIT_ASSERT_EQ(SituationGridBlitCells(grid, 1, 0, stamp, 2, 1), SITUATION_SUCCESS);

    int vd_id = -1;
    SIT_ASSERT_EQ(grid_create_compute_vd(cols, rows, cell, &vd_id), SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));
    SIT_ASSERT_EQ(SituationGridPresent(cmd, grid, vd_id, SIT_GRID_PASS_CELL_ONLY), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    uint8_t r = 0, g = 0, b = 0;
    int cy = grid_vd_center_y(rows * cell, 0, cell);
    SIT_ASSERT(grid_read_vd_pixel(vd_id, cell + 4, cy, &r, &g, &b));
    SIT_ASSERT(g > 200 && r < 80);
    SIT_ASSERT(grid_read_vd_pixel(vd_id, 4, cy, &r, &g, &b));
    SIT_ASSERT(r < 32 && g < 32 && b < 32);

    SituationGridDestroy(grid);
    SituationDestroyVirtualDisplay(vd_id);
}

/** Probe vs collision grid: solid wall cell → hit + separation normal. */
static void test_grid_collision_probe(void) {
    const int cols = 4;
    const int rows = 3;
    const int cell = 8;
    const uint32_t probe_id = 7u;
    const SitGridCell empty = grid_make_cell(0u, 0u, SIT_GRID_COLOR_TRANSPARENT);
    const SitGridCell solid = grid_make_cell(1u, 0u, SIT_GRID_COLOR_RGBA8(0, 0, 0, 255));

    SituationGridSurface visual = SituationGridCreate(cols, rows, cell, cell);
    SituationGridSurface collide = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(visual);
    SIT_ASSERT_NOT_NULL(collide);

    grid_fill_solid(visual, cols, rows, SIT_GRID_COLOR_RGBA8(40, 40, 40, 255));
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            SitGridCell c = empty;
            if (x == 2 && y == 1) {
                c = solid;
            }
            SIT_ASSERT_EQ(SituationGridSetCell(collide, x, y, c), SITUATION_SUCCESS);
        }
    }
    SIT_ASSERT_EQ(SituationGridSetRole(collide, SIT_GRID_ROLE_COLLISION), SITUATION_SUCCESS);

    SituationGridStack stack = SituationGridStackCreate();
    SIT_ASSERT_NOT_NULL(stack);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, visual, 0), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGridStackAddGrid(stack, collide, 1), SITUATION_SUCCESS);

    SitGridCollisionProbe probe = {
        .probe_id = probe_id,
        .x = 1.5f,
        .y = 1.0f,
        .w = 1.0f,
        .h = 1.0f,
    };
    SIT_ASSERT_EQ(SituationGridSetCollisionProbe(stack, probe), SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));

    SitGridCollisionEvent events[SIT_GRID_COLLISION_MAX_EVENTS];
    int hit_count = 0;
    SIT_ASSERT_EQ(
        SituationGridTestCollision(cmd, stack, probe_id, events, (int)(sizeof(events) / sizeof(events[0])), &hit_count),
        SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    SIT_ASSERT(hit_count >= 1);
    SIT_ASSERT_EQ(events[0].probe_id, probe_id);
    SIT_ASSERT_EQ(events[0].kind, (uint32_t)SIT_GRID_HIT_CELL);
    SIT_ASSERT(events[0].normal_flags & SIT_GRID_COLLISION_NORM_LEFT);

    SituationGridStackDestroy(stack);
    SituationGridDestroy(collide);
    SituationGridDestroy(visual);
}

/* --- Phase F.4.2 / B.2.3: grid.comp vs legacy terminal.comp on shared SitGridCell fixture --- */

typedef struct {
    Vector2 screen_size;
    Vector2 char_size;
    Vector2 grid_size;
    float time;
    uint32_t cursor_index;
    uint32_t cursor_blink_state;
    uint32_t text_blink_state;
    uint32_t sel_start;
    uint32_t sel_end;
    uint32_t sel_active;
    uint32_t mouse_cursor_index;
    uint64_t terminal_buffer_addr;
    uint64_t vector_buffer_addr;
    uint64_t font_texture_handle;
    uint64_t sixel_texture_handle;
    uint64_t vector_texture_handle;
    uint64_t shader_config_addr;
    uint32_t atlas_cols;
    uint32_t vector_count;
    int sixel_y_offset;
    uint32_t grid_color;
    uint32_t conceal_char_code;
    uint32_t font_data_width;
    uint32_t font_data_height;
} GridParityPushConstants;

static char* grid_load_repo_text(const char* relative_path) {
    static const char* prefixes[] = {
        "",
        "sit/",
        "../",
        "../../",
        "../../../",
    };
    char path[512];
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        if (prefixes[i][0]) {
            snprintf(path, sizeof(path), "%s%s", prefixes[i], relative_path);
        } else {
            snprintf(path, sizeof(path), "%s", relative_path);
        }
        char* text = SituationLoadFileText(path);
        if (text) {
            return text;
        }
    }
    return NULL;
}

static char* grid_concat_shader_sources(const char* preamble_path, const char* body_path) {
    char* preamble = grid_load_repo_text(preamble_path);
    char* body = grid_load_repo_text(body_path);
    if (!preamble || !body) {
        if (preamble) free(preamble);
        if (body) free(body);
        return NULL;
    }

#if defined(SITUATION_USE_VULKAN)
    static const char vk_define[] = "#define VULKAN_BACKEND\n";
#else
    static const char vk_define[] = "";
#endif

    size_t pre_len = strlen(preamble);
    size_t body_len = strlen(body);
    char* combined = (char*)malloc(pre_len + body_len + 1);
    if (!combined) {
        free(preamble);
        free(body);
        return NULL;
    }
    memcpy(combined, preamble, pre_len);
    memcpy(combined + pre_len, body, body_len);
    combined[pre_len + body_len] = '\0';
    free(preamble);
    free(body);

    if (vk_define[0]) {
        const char* version_marker = "#version";
        char* p = strstr(combined, version_marker);
        if (p) {
            char* line_end = strchr(p, '\n');
            if (!line_end) {
                line_end = combined + strlen(combined);
            } else {
                ++line_end;
            }
            size_t prefix_len = (size_t)(line_end - combined);
            size_t def_len = strlen(vk_define);
            size_t rest_len = strlen(line_end);
            char* injected = (char*)malloc(prefix_len + def_len + rest_len + 1);
            if (!injected) {
                free(combined);
                return NULL;
            }
            memcpy(injected, combined, prefix_len);
            memcpy(injected + prefix_len, vk_define, def_len);
            memcpy(injected + prefix_len + def_len, line_end, rest_len + 1);
            free(combined);
            combined = injected;
        }
    }
    return combined;
}

static SituationTexture grid_create_dummy_sixel_texture(void) {
    SituationTexture tex = {0};
    /* Transparent — terminal.comp mixes sixel by alpha; opaque dummy would black out output. */
    uint8_t px[4] = {0, 0, 0, 0};
    SituationImage img = {0};
    img.width = 1;
    img.height = 1;
    img.channels = 4;
    img.data = px;
    if (SituationCreateTextureEx(
            img, false,
            SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_DST,
            &tex) != SITUATION_SUCCESS) {
        tex.generation = 0;
    }
    return tex;
}

static void grid_fill_parity_push_constants(
    SituationGridSurface grid,
    SituationFont font,
    SituationTexture sixel_tex,
    int target_w,
    int target_h,
    GridParityPushConstants* pc)
{
    memset(pc, 0, sizeof(*pc));
    pc->screen_size = (Vector2){{(float)target_w, (float)target_h}};
    pc->char_size = (Vector2){{8.0f, 8.0f}};
    pc->grid_size = (Vector2){{4.0f, 1.0f}};
    pc->time = (float)SituationTimerGetTime();
    pc->cursor_index = 0xFFFFFFFFu;
    pc->mouse_cursor_index = 0xFFFFFFFFu;
    pc->terminal_buffer_addr = SituationGetBufferDeviceAddress(SituationGridGetCellBuffer(grid));
    pc->font_texture_handle = SituationGetTextureHandle(font.atlas_texture);
    pc->sixel_texture_handle = SituationGetTextureHandle(sixel_tex);
    pc->atlas_cols = (uint32_t)(font.chars_per_row > 0 ? font.chars_per_row : 16);
    pc->vector_count = 0u;
    pc->font_data_width = (uint32_t)(font.display_cell_width > 0 ? font.display_cell_width : 8);
    pc->font_data_height = (uint32_t)(font.display_cell_height > 0 ? font.display_cell_height : 8);
}

static SituationError grid_dispatch_legacy_terminal(
    SituationCommandBuffer cmd,
    SituationGridSurface grid,
    SituationComputePipeline legacy_pipeline,
    SituationTexture target,
    SituationFont font,
    SituationTexture sixel_tex,
    const GridParityPushConstants* pc)
{
    SituationTextureInfo info = {0};
    SituationError err = SituationGetTextureInfo(target, &info);
    if (err != SITUATION_SUCCESS) return err;

    err = SituationCmdBindComputePipeline(cmd, legacy_pipeline);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindDescriptorSet(cmd, 0, SituationGridGetCellBuffer(grid));
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindComputeTexture(cmd, 1, target);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindSampledTexture(cmd, 2, font.atlas_texture);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindSampledTexture(cmd, 3, sixel_tex);
    if (err != SITUATION_SUCCESS) return err;

#if defined(SITUATION_USE_OPENGL)
    err = SituationCmdSetPushConstant(cmd, 4, pc, sizeof(*pc));
#else
    err = SituationCmdSetPushConstant(cmd, 0, pc, sizeof(*pc));
#endif
    if (err != SITUATION_SUCCESS) return err;

    uint32_t groups_x = ((uint32_t)info.width + 7u) / 8u;
    uint32_t groups_y = ((uint32_t)info.height + 15u) / 16u;
    err = SituationCmdDispatch(cmd, groups_x, groups_y, 1);
    if (err != SITUATION_SUCCESS) return err;

    SituationCmdPipelineBarrier(
        cmd, SITUATION_BARRIER_COMPUTE_SHADER_WRITE, SITUATION_BARRIER_TRANSFER_READ);
    return SITUATION_SUCCESS;
}

static bool grid_vd_pixels_match(int vd_a, int vd_b, int x, int y, int tol) {
    uint8_t r0, g0, b0, r1, g1, b1;
    if (!grid_read_vd_pixel(vd_a, x, y, &r0, &g0, &b0)) return false;
    if (!grid_read_vd_pixel(vd_b, x, y, &r1, &g1, &b1)) return false;
    return grid_pixel_near(r0, r1, tol) && grid_pixel_near(g0, g1, tol) && grid_pixel_near(b0, b1, tol);
}

static void test_grid_kterm_parity(void) {
    const int cols = 4;
    const int rows = 1;
    const int cell = 8;
    const uint32_t red = SIT_GRID_COLOR_RGBA8(220, 20, 20, 255);
    const uint32_t blue = SIT_GRID_COLOR_RGBA8(20, 20, 220, 255);
    const uint32_t white = SIT_GRID_COLOR_RGBA8(240, 240, 240, 255);
    const uint32_t yellow = SIT_GRID_COLOR_RGBA8(220, 220, 0, 255);
    const uint32_t green = SIT_GRID_COLOR_RGBA8(20, 200, 20, 255);

    SituationGridSurface grid = SituationGridCreate(cols, rows, cell, cell);
    SIT_ASSERT_NOT_NULL(grid);

    SituationFont font = {0};
    SIT_ASSERT_EQ(sit_text_test_build_cp437_font(&font), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(SituationGridSetFont(grid, font), SITUATION_SUCCESS);

    SitGridCell fixture[] = {
        { .code = ' ', .fg = 0u, .bg = red, .flags = 0u },
        { .code = ' ', .fg = white, .bg = blue, .flags = SIT_GRID_ATTR_REVERSE },
        { .code = ' ', .fg = yellow, .bg = 0u, .flags = SIT_GRID_ATTR_FAINT },
        { .code = ' ', .fg = 0u, .bg = green, .flags = 0u },
    };
    for (int x = 0; x < cols; ++x) {
        SIT_ASSERT_EQ(SituationGridSetCell(grid, x, 0, fixture[x]), SITUATION_SUCCESS);
    }

    char* legacy_src = grid_concat_shader_sources(
        "sit/gpu/grid_preamble.glslh", "sit/k-term/shaders/terminal.comp");
    if (!legacy_src) {
        fprintf(stderr, "[grid] grid_kterm_parity skipped (shader assets not found)\n");
        SituationUnloadFont(font);
        SituationGridDestroy(grid);
        SIT_ASSERT(true);
        return;
    }

    SituationComputePipeline legacy_pipeline = {0};
    SIT_ASSERT_EQ(
        SituationCreateComputePipelineFromMemory(
            legacy_src, SIT_COMPUTE_LAYOUT_GRID, &legacy_pipeline),
        SITUATION_SUCCESS);
    free(legacy_src);

    SituationTexture dummy_sixel = grid_create_dummy_sixel_texture();
    SIT_ASSERT(dummy_sixel.generation != 0);

    int vd_grid = -1;
    int vd_legacy = -1;
    SIT_ASSERT_EQ(grid_create_compute_vd(cols, rows, cell, &vd_grid), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(grid_create_compute_vd(cols, rows, cell, &vd_legacy), SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(grid_begin_frame(&cmd));

    SIT_ASSERT_EQ(
        SituationGridPresent(cmd, grid, vd_grid, SIT_GRID_PASS_CELL_ONLY),
        SITUATION_SUCCESS);

    GridParityPushConstants pc = {0};
    grid_fill_parity_push_constants(
        grid, font, dummy_sixel, cols * cell, rows * cell, &pc);

    SituationTexture legacy_target = {0};
    SIT_ASSERT_EQ(SituationGetVirtualDisplayTexture(vd_legacy, &legacy_target), SITUATION_SUCCESS);
    SIT_ASSERT_EQ(
        grid_dispatch_legacy_terminal(cmd, grid, legacy_pipeline, legacy_target, font, dummy_sixel, &pc),
        SITUATION_SUCCESS);

    SIT_ASSERT_EQ(SituationEndFrame(), SITUATION_SUCCESS);

    const int tol = 18;
    for (int x = 0; x < cols; ++x) {
        int px = x * cell + cell / 2;
        int py = grid_vd_center_y(rows * cell, 0, cell);
        SIT_ASSERT(grid_vd_pixels_match(vd_grid, vd_legacy, px, py, tol));
    }

    SituationDestroyComputePipeline(&legacy_pipeline);
    if (dummy_sixel.generation != 0) {
        SituationDestroyTexture(&dummy_sixel);
    }
    SituationUnloadFont(font);
    SituationDestroyVirtualDisplay(vd_grid);
    SituationDestroyVirtualDisplay(vd_legacy);
    SituationGridDestroy(grid);
}

static SitTestCase grid_tests[] = {
    { "cell_checkerboard", test_grid_cell_checkerboard },
    { "vd_present", test_grid_vd_present },
    { "stack_two_layer", test_grid_stack_two_layer },
    { "stack_scroll", test_grid_stack_scroll },
    { "stack_skip_collision", test_grid_stack_skips_collision_role },
    { "actor_over_tiles", test_grid_actor_over_tiles },
    { "blit_and_clear", test_grid_blit_and_clear },
    { "collision_probe", test_grid_collision_probe },
    { "grid_kterm_parity", test_grid_kterm_parity },
};

const SitTestModule g_module_grid = {
    .name = "grid",
    .setup = grid_setup,
    .teardown = grid_teardown,
    .tests = grid_tests,
    .test_count = sizeof(grid_tests) / sizeof(grid_tests[0]),
};
