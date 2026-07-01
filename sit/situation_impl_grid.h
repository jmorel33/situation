#ifndef SITUATION_IMPL_GRID_H
#define SITUATION_IMPL_GRID_H

/***************************************************************************************************
 *
 *   situation_impl_grid.h - Situation Grid Subsystem (implementation)
 *   One grid = code+fg+bg cells; layers via stack (Phase C+). Phase B: single-grid present.
 *
 **************************************************************************************************/

#ifdef SITUATION_IMPLEMENTATION

#include "k-term/kt_render_sit.h"

/* Mirror K-Term push constants (kt_composite_sit.h) — must match grid.comp pc layout. */
typedef struct _SituationGridPushConstants {
    Vector2 screen_size;
    Vector2 char_size;
    Vector2 grid_size;
    float time;
    union { uint32_t cursor_index; };
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
} _SituationGridPushConstants;

typedef struct _SituationGridGPUCell {
    uint32_t char_code;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t flags;
    uint32_t ul_color;
    uint32_t st_color;
} _SituationGridGPUCell;

typedef struct SituationGridSurface_T {
    int cols;
    int rows;
    int cell_w;
    int cell_h;
    int pixel_w;
    int pixel_h;
    SituationScalingMode scale_mode;
    SituationBuffer cell_buffer;
    SitGridCell* cpu_cells;
    bool cells_dirty;
    int dirty_row_begin;
    int dirty_row_end;
    SituationComputePipeline pipeline;
    bool pipeline_valid;
    SituationFont font;
    SituationTexture dummy_sixel_texture;
    uint32_t atlas_cols;
    uint32_t font_data_width;
    uint32_t font_data_height;
    float scroll_x;
    float scroll_y;
    SitGridRole role;
} SituationGridSurface_T;

#define SIT_GRID_STACK_MAX_ENTRIES 8

typedef struct _SitGridStackEntry {
    SituationGridSurface_T* grid;
    int z_order;
} _SitGridStackEntry;

typedef struct SituationGridStack_T {
    _SitGridStackEntry entries[SIT_GRID_STACK_MAX_ENTRIES];
    int count;
    int ref_cols;
    int ref_rows;
    int ref_cell_w;
    int ref_cell_h;
    bool topology_set;
    SitGridCollisionProbe probe;
    bool probe_valid;
    SitGridCollisionHeader collision_header;
    SitGridCollisionEvent collision_events[SIT_GRID_COLLISION_MAX_EVENTS];
} SituationGridStack_T;

#define SIT_GRID_SHADER_PREAMBLE_PATH "sit/gpu/grid_preamble.glslh"
#define SIT_GRID_SHADER_BODY_PATH     "sit/gpu/grid.comp"

static SituationError _SitGridLoadComputeSource(char** out_src) {
    if (!out_src) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGrid: out_src is NULL");
    *out_src = NULL;

    char* preamble = NULL;
    char* body = NULL;
    SituationError err = _SituationLoadCoreShaderFile(SIT_GRID_SHADER_PREAMBLE_PATH, &preamble);
    if (err != SITUATION_SUCCESS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_NOT_FOUND, "SituationGrid: failed to load sit/gpu/grid_preamble.glslh");
    }
    err = _SituationLoadCoreShaderFile(SIT_GRID_SHADER_BODY_PATH, &body);
    if (err != SITUATION_SUCCESS) {
        SIT_FREE(preamble);
        return _SituationSetErrorFromCode(SITUATION_ERROR_FILE_NOT_FOUND, "SituationGrid: failed to load sit/gpu/grid.comp");
    }

#if defined(SITUATION_USE_VULKAN)
    static const char backend_define[] = "#define VULKAN_BACKEND\n";
#else
    static const char backend_define[] = "";
#endif

    size_t pre_len = strlen(preamble);
    size_t body_len = strlen(body);
    char* combined = (char*)SIT_MALLOC(pre_len + body_len + 1);
    if (!combined) {
        SIT_FREE(preamble);
        SIT_FREE(body);
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGrid: shader source allocation failed");
    }
    memcpy(combined, preamble, pre_len);
    memcpy(combined + pre_len, body, body_len);
    combined[pre_len + body_len] = '\0';
    SIT_FREE(preamble);
    SIT_FREE(body);

    char* src = combined;
    if (backend_define[0]) {
        const char* version_marker = "#version";
        const char* p = strstr(combined, version_marker);
        if (p) {
            const char* line_end = strchr(p, '\n');
            if (!line_end) line_end = combined + strlen(combined);
            else ++line_end;
            size_t prefix_len = (size_t)(line_end - combined);
            size_t def_len = strlen(backend_define);
            size_t rest_len = strlen(line_end);
            char* injected = (char*)SIT_MALLOC(prefix_len + def_len + rest_len + 1);
            if (!injected) {
                SIT_FREE(combined);
                return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGrid: shader define injection failed");
            }
            memcpy(injected, combined, prefix_len);
            memcpy(injected + prefix_len, backend_define, def_len);
            memcpy(injected + prefix_len + def_len, line_end, rest_len + 1);
            SIT_FREE(combined);
            src = injected;
        }
    }

    *out_src = src;
    return SITUATION_SUCCESS;
}

static SituationFont _SitGridResolveFont(SituationFont font) {
    if (font.atlas_texture.generation == 0 && SituationIsInitialized()) {
        font = sit_render.default_font;
    }
    return font;
}

static void _SitGridRefreshFontMetrics(SituationGridSurface_T* grid) {
    if (!grid) return;
    SituationFont font = _SitGridResolveFont(grid->font);
    grid->font = font;
    grid->atlas_cols = (font.chars_per_row > 0) ? (uint32_t)font.chars_per_row : 16u;
    grid->font_data_width = (font.display_cell_width > 0) ? (uint32_t)font.display_cell_width : 8u;
    grid->font_data_height = (font.display_cell_height > 0) ? (uint32_t)font.display_cell_height : 8u;
}

static SituationError _SitGridCreateDummySixelTexture(SituationTexture* out_tex) {
    if (!out_tex) return SITUATION_ERROR_INVALID_PARAM;
    *out_tex = (SituationTexture){0};
    uint8_t px[4] = {0, 0, 0, 255};
    SituationImage img = {0};
    img.width = 1;
    img.height = 1;
    img.channels = 4;
    img.data = px;
    return SituationCreateTextureEx(
        img, false,
        SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED | SITUATION_TEXTURE_USAGE_TRANSFER_DST,
        out_tex);
}

static SituationError _SitGridCompilePipeline(SituationGridSurface_T* grid) {
    if (!grid || grid->pipeline_valid) return SITUATION_SUCCESS;

    char* src = NULL;
    SituationError err = _SitGridLoadComputeSource(&src);
    if (err != SITUATION_SUCCESS) return err;

    err = SituationCreateComputePipelineFromMemory(
        src, SIT_COMPUTE_LAYOUT_GRID, &grid->pipeline);
    SIT_FREE(src);

    if (err != SITUATION_SUCCESS) return err;
    grid->pipeline_valid = true;
    return SITUATION_SUCCESS;
}

static void _SitGridPackGPUCell(const SitGridCell* in, _SituationGridGPUCell* out) {
    out->char_code = in->code;
    out->fg_color = in->fg;
    out->bg_color = in->bg;
    out->flags = in->flags;
    out->ul_color = in->attr0;
    out->st_color = in->attr1;
}

static SituationError _SitGridFlushCells(SituationGridSurface_T* grid) {
    if (!grid || !grid->cells_dirty || !grid->cpu_cells) return SITUATION_SUCCESS;
    if (grid->cell_buffer.generation == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGrid: cell SSBO invalid");
    }

    size_t cell_count = (size_t)grid->cols * (size_t)grid->rows;
    _SituationGridGPUCell* gpu_cells = (_SituationGridGPUCell*)SIT_MALLOC(cell_count * sizeof(_SituationGridGPUCell));
    if (!gpu_cells) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGrid: staging allocation failed");
    }

    for (size_t i = 0; i < cell_count; ++i) {
        _SitGridPackGPUCell(&grid->cpu_cells[i], &gpu_cells[i]);
    }

    SituationError err = SituationUpdateBuffer(
        grid->cell_buffer, 0, cell_count * sizeof(_SituationGridGPUCell), gpu_cells);
    SIT_FREE(gpu_cells);

    if (err == SITUATION_SUCCESS) {
        grid->cells_dirty = false;
        grid->dirty_row_begin = grid->rows;
        grid->dirty_row_end = -1;
    }
    return err;
}

static SituationError _SitGridFillPushConstants(
    const SituationGridSurface_T* grid, int target_w, int target_h,
    SitGridPassMode pass_mode, _SituationGridPushConstants* pc)
{
    if (!grid || !pc) return SITUATION_ERROR_INVALID_PARAM;
    memset(pc, 0, sizeof(*pc));
    SituationFont font = _SitGridResolveFont(grid->font);
    pc->screen_size = (Vector2){{(float)target_w, (float)target_h}};
    pc->char_size = (Vector2){{(float)grid->cell_w, (float)grid->cell_h}};
    pc->grid_size = (Vector2){{(float)grid->cols, (float)grid->rows}};
    pc->time = (float)SituationTimerGetTime();
    pc->terminal_buffer_addr = SituationGetBufferDeviceAddress(grid->cell_buffer);
    pc->font_texture_handle = SituationGetTextureHandle(font.atlas_texture);
    pc->sixel_texture_handle = SituationGetTextureHandle(grid->dummy_sixel_texture);
    pc->atlas_cols = grid->atlas_cols;
    pc->font_data_width = grid->font_data_width;
    pc->font_data_height = grid->font_data_height;
    pc->vector_count = (uint32_t)pass_mode;
    pc->sel_start = (uint32_t)(int32_t)(grid->scroll_x * 256.0f);
    pc->sel_end = (uint32_t)(int32_t)(grid->scroll_y * 256.0f);
    return SITUATION_SUCCESS;
}

static SituationError _SitGridDispatchInternal(
    SituationCommandBuffer cmd, SituationGridSurface_T* grid, SituationTexture target, SitGridPassMode pass_mode)
{
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridDispatch: grid is NULL");
    if (!cmd) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridDispatch: cmd is NULL");
    if (target.generation == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridDispatch: target texture invalid");
    }
    if (pass_mode != SIT_GRID_PASS_CELL_ONLY && pass_mode != SIT_GRID_PASS_BLEND) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationGridDispatch: pass mode not implemented yet");
    }

    SituationError err = _SitGridCompilePipeline(grid);
    if (err != SITUATION_SUCCESS) return err;
    err = _SitGridFlushCells(grid);
    if (err != SITUATION_SUCCESS) return err;

    SituationFont font = _SitGridResolveFont(grid->font);
    if (font.atlas_texture.generation == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGridDispatch: font atlas missing");
    }

    SituationTextureInfo target_info = {0};
    err = SituationGetTextureInfo(target, &target_info);
    if (err != SITUATION_SUCCESS) return err;

    _SituationGridPushConstants pc = {0};
    err = _SitGridFillPushConstants(grid, target_info.width, target_info.height, pass_mode, &pc);
    if (err != SITUATION_SUCCESS) return err;

    err = SituationCmdBindComputePipeline(cmd, grid->pipeline);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindDescriptorSet(cmd, 0, grid->cell_buffer);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindComputeTexture(cmd, 1, target);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindSampledTexture(cmd, 2, font.atlas_texture);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindSampledTexture(cmd, 3, grid->dummy_sixel_texture);
    if (err != SITUATION_SUCCESS) return err;

#if defined(SITUATION_USE_OPENGL)
    err = KTerm_CmdSetTerminalConstants(cmd, &pc, sizeof(pc));
#else
    err = SituationCmdSetPushConstant(cmd, 0, &pc, sizeof(pc));
#endif
    if (err != SITUATION_SUCCESS) return err;

    uint32_t groups_x = ((uint32_t)target_info.width + 7u) / 8u;
    uint32_t groups_y = ((uint32_t)target_info.height + 15u) / 16u;
    err = SituationCmdDispatch(cmd, groups_x, groups_y, 1);
    if (err != SITUATION_SUCCESS) return err;

    SituationCmdPipelineBarrier(
        cmd, SITUATION_BARRIER_COMPUTE_SHADER_WRITE, SITUATION_BARRIER_TRANSFER_READ);
    return SITUATION_SUCCESS;
}

static void _SitGridStackSortEntries(SituationGridStack_T* stack) {
    if (!stack || stack->count < 2) return;
    for (int i = 0; i < stack->count - 1; ++i) {
        for (int j = i + 1; j < stack->count; ++j) {
            if (stack->entries[j].z_order < stack->entries[i].z_order) {
                _SitGridStackEntry tmp = stack->entries[i];
                stack->entries[i] = stack->entries[j];
                stack->entries[j] = tmp;
            }
        }
    }
}

static SituationError _SitGridStackPresentInternal(
    SituationCommandBuffer cmd, SituationGridStack_T* stack, SituationTexture target, int pixel_w, int pixel_h)
{
    if (!stack || stack->count <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridStackPresent: stack is empty");
    }
    _SitGridStackSortEntries(stack);

    SituationError err = SITUATION_SUCCESS;
    bool drew_visual = false;
    for (int i = 0; i < stack->count; ++i) {
        SituationGridSurface_T* grid = stack->entries[i].grid;
        if (!grid) continue;
        if (grid->role == SIT_GRID_ROLE_COLLISION) continue;

        SitGridPassMode mode = drew_visual ? SIT_GRID_PASS_BLEND : SIT_GRID_PASS_CELL_ONLY;
        if (mode == SIT_GRID_PASS_BLEND) {
            SituationCmdPipelineBarrier(
                cmd, SITUATION_BARRIER_COMPUTE_SHADER_WRITE, SITUATION_BARRIER_COMPUTE_SHADER_READ);
        }
        err = _SitGridDispatchInternal(cmd, grid, target, mode);
        if (err != SITUATION_SUCCESS) return err;
        drew_visual = true;
    }
    if (!drew_visual) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridStackPresent: no visual grids in stack");
    }
    (void)pixel_w;
    (void)pixel_h;
    return SITUATION_SUCCESS;
}

SITAPI SituationGridSurface SituationGridCreate(int cols, int rows, int cell_w, int cell_h) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGridCreate: library not initialized");
        return NULL;
    }
    if (cols <= 0 || rows <= 0 || cell_w <= 0 || cell_h <= 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridCreate: dimensions must be positive");
        return NULL;
    }

    SituationGridSurface_T* grid = (SituationGridSurface_T*)SIT_CALLOC(1, sizeof(SituationGridSurface_T));
    if (!grid) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGridCreate: allocation failed");
        return NULL;
    }

    grid->cols = cols;
    grid->rows = rows;
    grid->cell_w = cell_w;
    grid->cell_h = cell_h;
    grid->pixel_w = cols * cell_w;
    grid->pixel_h = rows * cell_h;
    grid->scale_mode = SITUATION_SCALING_INTEGER;
    grid->role = SIT_GRID_ROLE_VISUAL;
    grid->dirty_row_begin = rows;
    grid->dirty_row_end = -1;
    grid->font = (SituationFont){0};

    size_t cell_count = (size_t)cols * (size_t)rows;
    grid->cpu_cells = (SitGridCell*)SIT_CALLOC(cell_count, sizeof(SitGridCell));
    if (!grid->cpu_cells) {
        SIT_FREE(grid);
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGridCreate: cell staging failed");
        return NULL;
    }

    SituationError err = SituationCreateBuffer(
        cell_count * sizeof(_SituationGridGPUCell), NULL,
        SITUATION_BUFFER_USAGE_STORAGE_COMPUTE, &grid->cell_buffer);
    if (err != SITUATION_SUCCESS) {
        SIT_FREE(grid->cpu_cells);
        SIT_FREE(grid);
        return NULL;
    }

    err = _SitGridCreateDummySixelTexture(&grid->dummy_sixel_texture);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyBuffer(&grid->cell_buffer);
        SIT_FREE(grid->cpu_cells);
        SIT_FREE(grid);
        return NULL;
    }

    _SitGridRefreshFontMetrics(grid);
    grid->cells_dirty = true;
    return grid;
}

SITAPI void SituationGridDestroy(SituationGridSurface grid) {
    if (!grid) return;
    if (grid->pipeline_valid) SituationDestroyComputePipeline(&grid->pipeline);
    if (grid->cell_buffer.generation != 0) SituationDestroyBuffer(&grid->cell_buffer);
    if (grid->dummy_sixel_texture.generation != 0) SituationDestroyTexture(&grid->dummy_sixel_texture);
    SIT_FREE(grid->cpu_cells);
    SIT_FREE(grid);
}

SITAPI SituationError SituationGridSetCell(SituationGridSurface grid, int x, int y, SitGridCell cell) {
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridSetCell: grid is NULL");
    if (x < 0 || y < 0 || x >= grid->cols || y >= grid->rows) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridSetCell: out of bounds");
    }
    size_t idx = (size_t)y * (size_t)grid->cols + (size_t)x;
    grid->cpu_cells[idx] = cell;
    grid->cells_dirty = true;
    if (y < grid->dirty_row_begin) grid->dirty_row_begin = y;
    if (y > grid->dirty_row_end) grid->dirty_row_end = y;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridUploadCells(
    SituationGridSurface grid, const SitGridCell* cells, int count, int dirty_row_begin, int dirty_row_end)
{
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridUploadCells: grid is NULL");
    if (!cells || count <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridUploadCells: invalid cell array");
    }
    size_t max_cells = (size_t)grid->cols * (size_t)grid->rows;
    if ((size_t)count > max_cells) count = (int)max_cells;
    memcpy(grid->cpu_cells, cells, (size_t)count * sizeof(SitGridCell));
    grid->cells_dirty = true;
    if (dirty_row_begin >= 0 && dirty_row_end >= dirty_row_begin) {
        grid->dirty_row_begin = dirty_row_begin;
        grid->dirty_row_end = dirty_row_end;
    } else {
        grid->dirty_row_begin = 0;
        grid->dirty_row_end = grid->rows - 1;
    }
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridUploadGPUCells(
    SituationGridSurface grid, const void* gpu_cells, int count, int dirty_row_begin, int dirty_row_end)
{
    if (!grid) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridUploadGPUCells: grid is NULL");
    }
    if (!gpu_cells || count <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridUploadGPUCells: invalid cell array");
    }
    if (!grid->cpu_cells) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGridUploadGPUCells: cell buffer missing");
    }

    size_t max_cells = (size_t)grid->cols * (size_t)grid->rows;
    if ((size_t)count > max_cells) count = (int)max_cells;

    const _SituationGridGPUCell* src = (const _SituationGridGPUCell*)gpu_cells;
    for (int i = 0; i < count; ++i) {
        SitGridCell* dst = &grid->cpu_cells[i];
        dst->code = src[i].char_code;
        dst->fg = src[i].fg_color;
        dst->bg = src[i].bg_color;
        dst->flags = src[i].flags;
        dst->attr0 = src[i].ul_color;
        dst->attr1 = src[i].st_color;
        dst->version = 0u;
    }

    grid->cells_dirty = true;
    if (dirty_row_begin >= 0 && dirty_row_end >= dirty_row_begin) {
        grid->dirty_row_begin = dirty_row_begin;
        grid->dirty_row_end = dirty_row_end;
    } else {
        grid->dirty_row_begin = 0;
        grid->dirty_row_end = grid->rows - 1;
    }
    return _SitGridFlushCells(grid);
}

SITAPI SituationBuffer SituationGridGetCellBuffer(SituationGridSurface grid) {
    if (!grid) {
        SituationBuffer invalid = {0};
        return invalid;
    }
    return grid->cell_buffer;
}

static void _SitGridMarkDirtyRows(SituationGridSurface_T* grid, int row_begin, int row_end) {
    if (!grid) return;
    grid->cells_dirty = true;
    if (row_begin < grid->dirty_row_begin) grid->dirty_row_begin = row_begin;
    if (row_end > grid->dirty_row_end) grid->dirty_row_end = row_end;
}

SITAPI SituationError SituationGridClear(SituationGridSurface grid, SitGridCell cell) {
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridClear: grid is NULL");
    if (!grid->cpu_cells) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGridClear: cell buffer missing");
    }
    size_t cell_count = (size_t)grid->cols * (size_t)grid->rows;
    for (size_t i = 0; i < cell_count; ++i) {
        grid->cpu_cells[i] = cell;
    }
    grid->cells_dirty = true;
    grid->dirty_row_begin = 0;
    grid->dirty_row_end = grid->rows - 1;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridBlitCells(
    SituationGridSurface grid, int dst_x, int dst_y,
    const SitGridCell* src, int src_cols, int src_rows)
{
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridBlitCells: grid is NULL");
    if (!src || src_cols <= 0 || src_rows <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridBlitCells: invalid source");
    }
    if (!grid->cpu_cells) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGridBlitCells: cell buffer missing");
    }

    int dirty_begin = grid->rows;
    int dirty_end = -1;

    for (int sy = 0; sy < src_rows; ++sy) {
        int dy = dst_y + sy;
        if (dy < 0 || dy >= grid->rows) continue;
        for (int sx = 0; sx < src_cols; ++sx) {
            int dx = dst_x + sx;
            if (dx < 0 || dx >= grid->cols) continue;
            size_t idx = (size_t)dy * (size_t)grid->cols + (size_t)dx;
            grid->cpu_cells[idx] = src[(size_t)sy * (size_t)src_cols + (size_t)sx];
        }
        if (dy < dirty_begin) dirty_begin = dy;
        if (dy > dirty_end) dirty_end = dy;
    }

    if (dirty_end >= dirty_begin) {
        _SitGridMarkDirtyRows(grid, dirty_begin, dirty_end);
    }
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridSetFont(SituationGridSurface grid, SituationFont font) {
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridSetFont: grid is NULL");
    grid->font = font;
    _SitGridRefreshFontMetrics(grid);
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridSetScaleMode(SituationGridSurface grid, SituationScalingMode mode) {
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridSetScaleMode: grid is NULL");
    grid->scale_mode = mode;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridSetScroll(SituationGridSurface grid, float scroll_x, float scroll_y) {
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridSetScroll: grid is NULL");
    grid->scroll_x = scroll_x;
    grid->scroll_y = scroll_y;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridSetRole(SituationGridSurface grid, SitGridRole role) {
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridSetRole: grid is NULL");
    grid->role = role;
    return SITUATION_SUCCESS;
}

SITAPI SituationGridStack SituationGridStackCreate(void) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGridStackCreate: library not initialized");
        return NULL;
    }
    SituationGridStack_T* stack = (SituationGridStack_T*)SIT_CALLOC(1, sizeof(SituationGridStack_T));
    if (!stack) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationGridStackCreate: allocation failed");
        return NULL;
    }
    return stack;
}

SITAPI void SituationGridStackDestroy(SituationGridStack stack) {
    if (!stack) return;
    SIT_FREE(stack);
}

static SituationGridSurface_T* _SitGridStackFindCollisionGrid(SituationGridStack_T* stack) {
    if (!stack) return NULL;
    for (int i = 0; i < stack->count; ++i) {
        SituationGridSurface_T* grid = stack->entries[i].grid;
        if (grid && grid->role == SIT_GRID_ROLE_COLLISION) {
            return grid;
        }
    }
    return NULL;
}

static bool _SitGridCellIsSolid(const SitGridCell* cell) {
    if (!cell) return false;
    return cell->code != 0u && ((cell->bg >> 24) & 0xFFu) > 0u;
}

static bool _SitGridAabbOverlap(float ax, float ay, float aw, float ah,
                                float bx, float by, float bw, float bh)
{
    return ax < (bx + bw) && (ax + aw) > bx && ay < (by + bh) && (ay + ah) > by;
}

static uint32_t _SitGridCollisionNormal(float probe_x, float probe_y, float probe_w, float probe_h,
                                        int cell_x, int cell_y)
{
    float pcx = probe_x + probe_w * 0.5f;
    float pcy = probe_y + probe_h * 0.5f;
    float ccx = (float)cell_x + 0.5f;
    float ccy = (float)cell_y + 0.5f;
    uint32_t flags = 0u;
    if (pcx < ccx) flags |= SIT_GRID_COLLISION_NORM_LEFT;
    else if (pcx > ccx) flags |= SIT_GRID_COLLISION_NORM_RIGHT;
    if (pcy < ccy) flags |= SIT_GRID_COLLISION_NORM_TOP;
    else if (pcy > ccy) flags |= SIT_GRID_COLLISION_NORM_BOTTOM;
    return flags;
}

static SituationError _SitGridResolveCollisionsCPU(SituationGridStack_T* stack, uint32_t probe_id)
{
    if (!stack) return SITUATION_ERROR_INVALID_PARAM;
    if (!stack->probe_valid || stack->probe.probe_id != probe_id) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationGridDispatchCollide: call SituationGridSetCollisionProbe first");
    }

    SituationGridSurface_T* collide_grid = _SitGridStackFindCollisionGrid(stack);
    if (!collide_grid) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationGridDispatchCollide: stack has no SIT_GRID_ROLE_COLLISION grid");
    }

    stack->collision_header.count = 0u;
    stack->collision_header.overflow = 0u;

    const SitGridCollisionProbe* probe = &stack->probe;
    const float px = probe->x;
    const float py = probe->y;
    const float pw = probe->w;
    const float ph = probe->h;

    int x0 = (int)floorf(px);
    int y0 = (int)floorf(py);
    int x1 = (int)floorf(px + pw - 1e-4f);
    int y1 = (int)floorf(py + ph - 1e-4f);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= collide_grid->cols) x1 = collide_grid->cols - 1;
    if (y1 >= collide_grid->rows) y1 = collide_grid->rows - 1;

    for (int cy = y0; cy <= y1; ++cy) {
        for (int cx = x0; cx <= x1; ++cx) {
            if (!_SitGridAabbOverlap(px, py, pw, ph, (float)cx, (float)cy, 1.0f, 1.0f)) {
                continue;
            }
            size_t idx = (size_t)cy * (size_t)collide_grid->cols + (size_t)cx;
            const SitGridCell* cell = &collide_grid->cpu_cells[idx];
            if (!_SitGridCellIsSolid(cell)) {
                continue;
            }

            if (stack->collision_header.count >= SIT_GRID_COLLISION_MAX_EVENTS) {
                stack->collision_header.overflow = 1u;
                return SITUATION_SUCCESS;
            }

            SitGridCollisionEvent* ev =
                &stack->collision_events[stack->collision_header.count++];
            ev->probe_id = probe_id;
            ev->other_id = ((uint32_t)cy << 12) | ((uint32_t)cx & 0xFFFu);
            ev->kind = (uint32_t)SIT_GRID_HIT_CELL;
            ev->normal_flags = _SitGridCollisionNormal(px, py, pw, ph, cx, cy);
            ev->touch_x = (float)cx + 0.5f;
            ev->touch_y = (float)cy + 0.5f;
        }
    }
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridSetCollisionProbe(SituationGridStack stack, SitGridCollisionProbe probe) {
    if (!stack) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridSetCollisionProbe: stack is NULL");
    }
    if (probe.w <= 0.0f || probe.h <= 0.0f) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridSetCollisionProbe: invalid probe size");
    }
    stack->probe = probe;
    stack->probe_valid = true;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridStackAddGrid(SituationGridStack stack, SituationGridSurface grid, int z_order) {
    if (!stack) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridStackAddGrid: stack is NULL");
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridStackAddGrid: grid is NULL");
    if (stack->count >= SIT_GRID_STACK_MAX_ENTRIES) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridStackAddGrid: stack full");
    }
    if (!stack->topology_set) {
        stack->ref_cols = grid->cols;
        stack->ref_rows = grid->rows;
        stack->ref_cell_w = grid->cell_w;
        stack->ref_cell_h = grid->cell_h;
        stack->topology_set = true;
    } else if (grid->cols != stack->ref_cols || grid->rows != stack->ref_rows ||
               grid->cell_w != stack->ref_cell_w || grid->cell_h != stack->ref_cell_h) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationGridStackAddGrid: grid dimensions must match stack topology");
    }
    stack->entries[stack->count].grid = grid;
    stack->entries[stack->count].z_order = z_order;
    stack->count++;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridStackPresent(SituationCommandBuffer cmd, SituationGridStack stack, int vd_id) {
    if (!stack) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridStackPresent: stack is NULL");

    SituationVirtualDisplay* vd = SituationGetVirtualDisplay(vd_id);
    if (!vd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationGridStackPresent: invalid VD id");
    }
    if ((vd->flags & SITUATION_VD_FLAG_COMPUTE_TARGET) == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridStackPresent: VD is not COMPUTE_TARGET");
    }

    int vd_w = (int)vd->resolution.x;
    int vd_h = (int)vd->resolution.y;
    int expect_w = stack->topology_set ? stack->ref_cols * stack->ref_cell_w : 0;
    int expect_h = stack->topology_set ? stack->ref_rows * stack->ref_cell_h : 0;
    if (expect_w > 0 && (vd_w != expect_w || vd_h != expect_h)) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationGridStackPresent: VD resolution must match stack pixel dimensions");
    }

    SituationTexture tex = {0};
    SituationError err = SituationGetVirtualDisplayTexture(vd_id, &tex);
    if (err != SITUATION_SUCCESS) return err;

    err = _SitGridStackPresentInternal(cmd, stack, tex, vd_w, vd_h);
    if (err == SITUATION_SUCCESS) {
        SituationSetVirtualDisplayDirty(vd_id, true);
    }
    return err;
}

SITAPI SituationError SituationGridDispatchCollide(SituationCommandBuffer cmd, SituationGridStack stack, uint32_t probe_id) {
    (void)cmd;
    if (!stack) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridDispatchCollide: stack is NULL");
    }

    SituationGridSurface_T* collide_grid = _SitGridStackFindCollisionGrid(stack);
    if (!collide_grid) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationGridDispatchCollide: stack has no SIT_GRID_ROLE_COLLISION grid");
    }

    /* Upload collision grid SSBO so GPU GRID_PASS_COLLIDE can share the same cell data (Phase E.2). */
    SituationError err = _SitGridFlushCells(collide_grid);
    if (err != SITUATION_SUCCESS) return err;

    /* v1 resolve: CPU scan of collision grid cells (harness + gameplay). GPU collide pass follows in E.2. */
    return _SitGridResolveCollisionsCPU(stack, probe_id);
}

SITAPI SituationError SituationGridReadCollisions(
    SituationGridStack stack, SitGridCollisionEvent* out, int max_events, int* out_count)
{
    if (!stack) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridReadCollisions: stack is NULL");
    }
    if (!out_count) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridReadCollisions: out_count is NULL");
    }
    *out_count = 0;
    if (!out || max_events <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridReadCollisions: invalid output");
    }

    int count = (int)stack->collision_header.count;
    if (count > max_events) count = max_events;
    if (count > 0) {
        memcpy(out, stack->collision_events, (size_t)count * sizeof(SitGridCollisionEvent));
    }
    *out_count = count;
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGridTestCollision(
    SituationCommandBuffer cmd, SituationGridStack stack, uint32_t probe_id,
    SitGridCollisionEvent* out, int max_events, int* out_count)
{
    SituationError err = SituationGridDispatchCollide(cmd, stack, probe_id);
    if (err != SITUATION_SUCCESS) return err;
    return SituationGridReadCollisions(stack, out, max_events, out_count);
}

SITAPI SituationError SituationGridDispatch(
    SituationCommandBuffer cmd, SituationGridSurface grid, SituationTexture target, SitGridPassMode pass_mode)
{
    return _SitGridDispatchInternal(cmd, grid, target, pass_mode);
}

SITAPI SituationError SituationGridPresent(
    SituationCommandBuffer cmd, SituationGridSurface grid, int vd_id, SitGridPassMode pass_mode)
{
    if (!grid) return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridPresent: grid is NULL");

    SituationVirtualDisplay* vd = SituationGetVirtualDisplay(vd_id);
    if (!vd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID, "SituationGridPresent: invalid VD id");
    }
    if ((vd->flags & SITUATION_VD_FLAG_COMPUTE_TARGET) == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridPresent: VD is not COMPUTE_TARGET");
    }

    int vd_w = (int)vd->resolution.x;
    int vd_h = (int)vd->resolution.y;
    if (vd_w != grid->pixel_w || vd_h != grid->pixel_h) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM,
            "SituationGridPresent: VD resolution must match grid pixel dimensions");
    }

    SituationTexture tex = {0};
    SituationError err = SituationGetVirtualDisplayTexture(vd_id, &tex);
    if (err != SITUATION_SUCCESS) return err;

    err = _SitGridDispatchInternal(cmd, grid, tex, pass_mode);
    if (err == SITUATION_SUCCESS) {
        SituationSetVirtualDisplayDirty(vd_id, true);
    }
    return err;
}

SITAPI SituationError SituationGridDispatchPushConstants(
    SituationCommandBuffer cmd, SituationGridSurface grid, SituationTexture target, SitGridPassMode pass_mode,
    const void* push_constants, size_t push_constants_size,
    SituationTexture font_texture, SituationTexture sixel_texture)
{
    if (!grid) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridDispatchPushConstants: grid is NULL");
    }
    if (!cmd) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridDispatchPushConstants: cmd is NULL");
    }
    if (!push_constants || push_constants_size < sizeof(_SituationGridPushConstants)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridDispatchPushConstants: invalid push constants");
    }
    if (target.generation == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGridDispatchPushConstants: target texture invalid");
    }
    if (pass_mode != SIT_GRID_PASS_CELL_ONLY && pass_mode != SIT_GRID_PASS_BLEND) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationGridDispatchPushConstants: pass mode not supported");
    }
    if (font_texture.generation == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_RESOURCE_INVALID, "SituationGridDispatchPushConstants: font texture invalid");
    }
    if (sixel_texture.generation == 0) {
        sixel_texture = grid->dummy_sixel_texture;
    }

    SituationError err = _SitGridCompilePipeline(grid);
    if (err != SITUATION_SUCCESS) return err;
    err = _SitGridFlushCells(grid);
    if (err != SITUATION_SUCCESS) return err;

    SituationTextureInfo target_info = {0};
    err = SituationGetTextureInfo(target, &target_info);
    if (err != SITUATION_SUCCESS) return err;

    err = SituationCmdBindComputePipeline(cmd, grid->pipeline);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindDescriptorSet(cmd, 0, grid->cell_buffer);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindComputeTexture(cmd, 1, target);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindSampledTexture(cmd, 2, font_texture);
    if (err != SITUATION_SUCCESS) return err;
    err = SituationCmdBindSampledTexture(cmd, 3, sixel_texture);
    if (err != SITUATION_SUCCESS) return err;

#if defined(SITUATION_USE_OPENGL)
    err = KTerm_CmdSetTerminalConstants(cmd, push_constants, push_constants_size);
#else
    err = SituationCmdSetPushConstant(cmd, 0, push_constants, push_constants_size);
#endif
    if (err != SITUATION_SUCCESS) return err;

    uint32_t groups_x = ((uint32_t)target_info.width + 7u) / 8u;
    uint32_t groups_y = ((uint32_t)target_info.height + 15u) / 16u;
    err = SituationCmdDispatch(cmd, groups_x, groups_y, 1);
    if (err != SITUATION_SUCCESS) return err;

    SituationCmdPipelineBarrier(
        cmd, SITUATION_BARRIER_COMPUTE_SHADER_WRITE, SITUATION_BARRIER_TRANSFER_READ);
    return SITUATION_SUCCESS;
}

#endif /* SITUATION_IMPLEMENTATION */

#endif /* SITUATION_IMPL_GRID_H */
