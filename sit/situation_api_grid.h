#ifndef SITUATION_API_GRID_H
#define SITUATION_API_GRID_H

/***************************************************************************************************
 *
 *   situation_api_grid.h - Situation Grid Subsystem (public API)
 *
 *   One grid = uniform 2D cells, each **code + fg + bg** (K-Term core model).
 *   **Layers = stacked grids** (bottom→top), not multi-plane SSBOs inside one surface.
 *   Collision targets a dedicated grid in the stack (Phase E).
 *
 *   See doc/guide/grid.md and doc/plan/GRID_RENDER_PLAN.md
 *
 ***************************************************************************************************/

#include "situation_base_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief `grid.comp` pass modes (single shader file).
 * - @c SIT_GRID_PASS_CELL_ONLY — render one grid into a cleared target (Phase B, K-Term parity).
 * - @c SIT_GRID_PASS_BLEND — blend one grid onto an existing target; transparent cells pass through (Phase C stack).
 * - @c SIT_GRID_PASS_COLLIDE — probe vs collision grid cells → SSBO (Phase E).
 */
typedef enum SitGridPassMode {
    SIT_GRID_PASS_CELL_ONLY = 0,
    SIT_GRID_PASS_BLEND     = 1,
    SIT_GRID_PASS_COLLIDE   = 2,
} SitGridPassMode;

/** @deprecated Renamed @c SIT_GRID_PASS_BLEND — stack composite pass. */
#define SIT_GRID_PASS_COMPOSITE SIT_GRID_PASS_BLEND

/**
 * @brief Role of a grid when used in a stack (Phase C+).
 * @details Collision grids are usually not blended to the screen (`SIT_GRID_ROLE_COLLISION`).
 */
typedef enum SitGridRole {
    SIT_GRID_ROLE_VISUAL    = 0,
    SIT_GRID_ROLE_COLLISION = 1,
    SIT_GRID_ROLE_UI        = 2,
} SitGridRole;

/**
 * @brief Pack 8-bit RGBA into a grid cell color word (same layout as K-Term GPUCell fg/bg).
 * @details Component order: `r | (g<<8) | (b<<16) | (a<<24)`.
 */
#define SIT_GRID_COLOR_RGBA8(r, g, b, a) \
    ((uint32_t)(uint8_t)(r) | ((uint32_t)(uint8_t)(g) << 8) | \
     ((uint32_t)(uint8_t)(b) << 16) | ((uint32_t)(uint8_t)(a) << 24))

#define SIT_GRID_COLOR_BLACK       SIT_GRID_COLOR_RGBA8(0, 0, 0, 255)
#define SIT_GRID_COLOR_WHITE       SIT_GRID_COLOR_RGBA8(255, 255, 255, 255)
#define SIT_GRID_COLOR_TRANSPARENT SIT_GRID_COLOR_RGBA8(0, 0, 0, 0)

/** @brief Core grid cell: symbol + foreground + background. */
#define SIT_GRID_CELL(c, f, b) ((SitGridCell){ .code = (c), .fg = (f), .bg = (b) })

/**
 * @brief K-Term terminal bridge — @c SIT_GRID_ATTR_* OR into @c SitGridCell.flags (match @c GPU_ATTR_* / @c grid.comp).
 * @details @c attr0 = underline color word (@c ul_color); @c attr1 = strike color word (@c st_color). Non-terminal grids: all zero.
 */
#define SIT_GRID_ATTR_BOLD              (1u << 0)   // flags       — SGR bold glyph
#define SIT_GRID_ATTR_FAINT             (1u << 1)   // flags       — SGR faint / half-bright fg
#define SIT_GRID_ATTR_ITALIC            (1u << 2)   // flags       — slanted glyph sampling
#define SIT_GRID_ATTR_UNDERLINE         (1u << 3)   // flags→attr0 — single underline; ul RGBA in attr0
#define SIT_GRID_ATTR_BLINK             (1u << 4)   // flags       — fast fg blink
#define SIT_GRID_ATTR_REVERSE           (1u << 5)   // flags       — swap fg/bg
#define SIT_GRID_ATTR_STRIKE            (1u << 6)   // flags→attr1 — mid-row strike; st RGBA in attr1
#define SIT_GRID_ATTR_DOUBLE_WIDTH      (1u << 7)   // flags       — double-width glyph (line layout)
#define SIT_GRID_ATTR_DOUBLE_HEIGHT_TOP (1u << 8)   // flags       — double-height top half (line leader)
#define SIT_GRID_ATTR_DOUBLE_HEIGHT_BOT (1u << 9)   // flags       — double-height bottom half (line leader)
#define SIT_GRID_ATTR_CONCEAL           (1u << 10)  // flags       — conceal / password char
#define SIT_GRID_ATTR_OVERLINE          (1u << 11)  // flags       — overline using fg (not attr0)
#define SIT_GRID_ATTR_DOUBLE_UNDERLINE  (1u << 12)  // flags→attr0 — double underline; ul RGBA in attr0
#define SIT_GRID_ATTR_BLINK_BG          (1u << 13)  // flags       — background blink
#define SIT_GRID_ATTR_BLINK_SLOW        (1u << 14)  // flags       — slow fg blink
#define SIT_GRID_ATTR_FAINT_BG          (1u << 15)  // flags       — half-bright background
#define SIT_GRID_ATTR_FRAMED            (1u << 16)  // flags       — SGR framed cell border
#define SIT_GRID_ATTR_ENCIRCLED         (1u << 17)  // flags       — SGR encircled cell border
#define SIT_GRID_ATTR_GRID              (1u << 18)  // flags       — debug cell grid overlay
#define SIT_GRID_ATTR_SUPERSCRIPT       (1u << 19)  // flags       — superscript glyph scale/offset
#define SIT_GRID_ATTR_UL_STYLE_MASK     (7u << 20)  // flags→attr0 — underline style field (bits 20–22)
#define SIT_GRID_ATTR_UL_STYLE_NONE     (0u << 20)  // flags→attr0 — clear underline style override
#define SIT_GRID_ATTR_UL_STYLE_SINGLE   (1u << 20)  // flags→attr0 — single underline style
#define SIT_GRID_ATTR_UL_STYLE_DOUBLE   (2u << 20)  // flags→attr0 — double underline style
#define SIT_GRID_ATTR_UL_STYLE_CURLY    (3u << 20)  // flags→attr0 — curly underline style
#define SIT_GRID_ATTR_UL_STYLE_DOTTED   (4u << 20)  // flags→attr0 — dotted underline style
#define SIT_GRID_ATTR_UL_STYLE_DASHED   (5u << 20)  // flags→attr0 — dashed underline style
#define SIT_GRID_ATTR_SUBSCRIPT         (1u << 23)  // flags       — subscript glyph scale/offset
#define SIT_GRID_ATTR_PROTECTED         (1u << 28)  // flags       — DECSCA protected cell
#define SIT_GRID_ATTR_SOFT_HYPHEN       (1u << 29)  // flags       — soft hyphen marker

/**
 * @brief One cell on a grid surface (32 B, std430-friendly).
 *
 * **Core model:** @c code + @c fg + @c bg — use @ref SIT_GRID_CELL for symbol + color cells.
 * Empty / transparent pass-through when @c code == 0 and @c bg alpha is 0.
 *
 * **K-Term bridge** (first 24 B match @c GPUCell): @c flags, @c attr0 (ul_color), @c attr1 (st_color).
 * @c version is CPU upload generation only (not sent in the 24-byte GPU prefix).
 */
typedef struct SitGridCell {
    uint32_t code;    /**< Symbol / atlas index; 0 = empty */
    uint32_t fg;      /**< Foreground @ref SIT_GRID_COLOR_RGBA8 */
    uint32_t bg;      /**< Background @ref SIT_GRID_COLOR_RGBA8 */
    uint32_t flags;   /**< OR @ref SIT_GRID_ATTR_* (see EOL: flags / flags→attr0 / flags→attr1) */
    uint32_t attr0;   /**< Underline RGBA (@ref SIT_GRID_COLOR_RGBA8) — used when flags→attr0 */
    uint32_t attr1;   /**< Strikethrough RGBA (@ref SIT_GRID_COLOR_RGBA8) — used when flags→attr1 */
    uint32_t version;
} SitGridCell;

typedef struct SituationGridSurface_T* SituationGridSurface;
typedef struct SituationGridStack_T* SituationGridStack;

typedef enum SitGridHitKind {
    SIT_GRID_HIT_CELL = 0,
    SIT_GRID_HIT_GRID = 1,
} SitGridHitKind;

/** @brief One collision query result (Phase E). */
typedef struct SitGridCollisionEvent {
    uint32_t probe_id;
    uint32_t other_id;      /**< Packed grid index + cell (grid<<24 | ty<<12 | tx) */
    uint32_t kind;          /**< @ref SitGridHitKind */
    uint32_t normal_flags;  /**< @ref SIT_GRID_COLLISION_NORM_* */
    float touch_x;
    float touch_y;
} SitGridCollisionEvent;

/** @brief Collision output header (CPU mirror; GPU SSBO in a later pass). */
typedef struct SitGridCollisionHeader {
    uint32_t count;
    uint32_t overflow;
} SitGridCollisionHeader;

/** @brief Probe AABB in **grid cell coordinates** (same space as @c SituationGridSetCell). */
typedef struct SitGridCollisionProbe {
    uint32_t probe_id;
    float x;
    float y;
    float w;
    float h;
} SitGridCollisionProbe;

#define SIT_GRID_COLLISION_MAX_EVENTS 64

/** @brief Separation normals — direction to move the probe out of overlap. */
#define SIT_GRID_COLLISION_NORM_LEFT   (1u << 0)
#define SIT_GRID_COLLISION_NORM_RIGHT  (1u << 1)
#define SIT_GRID_COLLISION_NORM_TOP    (1u << 2)
#define SIT_GRID_COLLISION_NORM_BOTTOM (1u << 3)

/* --- Single grid lifecycle --- */
SITAPI SituationGridSurface SituationGridCreate(int cols, int rows, int cell_w, int cell_h);
SITAPI void SituationGridDestroy(SituationGridSurface grid);

/* --- Cells (code + fg + bg) --- */
SITAPI SituationError SituationGridSetCell(SituationGridSurface grid, int x, int y, SitGridCell cell);
SITAPI SituationError SituationGridUploadCells(
    SituationGridSurface grid, const SitGridCell* cells, int count, int dirty_row_begin, int dirty_row_end);
/**
 * @brief Upload K-Term @c GPUCell rows (24 B/cell) into a grid surface.
 * @details Same field layout as the first 24 bytes of @ref SitGridCell (`code`/`fg`/`bg`/…).
 *          K-Term compositor uses this instead of @c SituationGridUploadCells (32 B stride).
 */
SITAPI SituationError SituationGridUploadGPUCells(
    SituationGridSurface grid, const void* gpu_cells, int count, int dirty_row_begin, int dirty_row_end);
/** @brief GPU cell SSBO for K-Term bridge dispatch (bind set 0). */
SITAPI SituationBuffer SituationGridGetCellBuffer(SituationGridSurface grid);
/** @brief Fill every cell slot with @p cell (Phase D — actor layer repaint). */
SITAPI SituationError SituationGridClear(SituationGridSurface grid, SitGridCell cell);
/**
 * @brief Stamp a @p src_cols × @p src_rows block at grid coord (@p dst_x, @p dst_y).
 * @details Clips to grid bounds. @p src is row-major (`src[sy * src_cols + sx]`).
 */
SITAPI SituationError SituationGridBlitCells(
    SituationGridSurface grid, int dst_x, int dst_y,
    const SitGridCell* src, int src_cols, int src_rows);

/* --- Per-grid config --- */
SITAPI SituationError SituationGridSetFont(SituationGridSurface grid, SituationFont font);
SITAPI SituationError SituationGridSetScaleMode(SituationGridSurface grid, SituationScalingMode mode);
SITAPI SituationError SituationGridSetScroll(SituationGridSurface grid, float scroll_x, float scroll_y);
SITAPI SituationError SituationGridSetRole(SituationGridSurface grid, SitGridRole role);

/* --- Single-grid draw --- */
SITAPI SituationError SituationGridDispatch(
    SituationCommandBuffer cmd, SituationGridSurface grid, SituationTexture target, SitGridPassMode pass_mode);
SITAPI SituationError SituationGridPresent(
    SituationCommandBuffer cmd, SituationGridSurface grid, int vd_id, SitGridPassMode pass_mode);

/**
 * @brief Dispatch with caller-owned push constants (K-Term Phase F bridge).
 * @details Uses the grid SSBO + compiled `grid.comp` pipeline. @p font_texture and @p sixel_texture
 * override atlas bindings; @p push_constants must match `KTermPushConstants` / `grid.comp` layout.
 */
SITAPI SituationError SituationGridDispatchPushConstants(
    SituationCommandBuffer cmd, SituationGridSurface grid, SituationTexture target, SitGridPassMode pass_mode,
    const void* push_constants, size_t push_constants_size,
    SituationTexture font_texture, SituationTexture sixel_texture);

/* --- Stack (layers = stacked grids) — Phase C --- */
SITAPI SituationGridStack SituationGridStackCreate(void);
SITAPI void SituationGridStackDestroy(SituationGridStack stack);
SITAPI SituationError SituationGridStackAddGrid(SituationGridStack stack, SituationGridSurface grid, int z_order);
SITAPI SituationError SituationGridStackPresent(SituationCommandBuffer cmd, SituationGridStack stack, int vd_id);

/* --- Collision (vs collision grid in stack) — Phase E --- */
/** @brief Set probe AABB for the next @c SituationGridDispatchCollide / @c SituationGridTestCollision. */
SITAPI SituationError SituationGridSetCollisionProbe(SituationGridStack stack, SitGridCollisionProbe probe);
SITAPI SituationError SituationGridDispatchCollide(SituationCommandBuffer cmd, SituationGridStack stack, uint32_t probe_id);
SITAPI SituationError SituationGridReadCollisions(
    SituationGridStack stack, SitGridCollisionEvent* out, int max_events, int* out_count);
SITAPI SituationError SituationGridTestCollision(
    SituationCommandBuffer cmd, SituationGridStack stack, uint32_t probe_id,
    SitGridCollisionEvent* out, int max_events, int* out_count);

#ifdef __cplusplus
}
#endif

#endif /* SITUATION_API_GRID_H */
