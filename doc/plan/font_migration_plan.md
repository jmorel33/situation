# Font Migration Plan — RGL → Situation

**Status:** in progress (v2.4.341: **F0–F4 complete**; **F5 partial** docs/bindings; **F6 partial** RGL bitmap path; **F7** harness/CI open)  
**Scope:** Move all font loading, atlas baking, layout, GPU/CPU text drawing, and stamp-to-texture workflows from `doc/misc/rgl.h` into Situation (`sit/`). End state: RGL text/font APIs are thin wrappers that delegate to Situation; no duplicated glyph loops or atlas builders in RGL.  
**Strategy:** Situation-first — implement and test core APIs before rewiring RGL. Preserve RGL public names where possible so game code does not rewrite.  
**Primary files:** `sit/situation_api_platform.h`, `sit/situation_api_graphics.h`, `sit/situation_impl_image.h`, `sit/situation_impl_renderer.h`, `sit/situation_base_font.h`, `doc/misc/rgl.h` (wrappers + deletion), `tests/harness/test_text_rendering.c`, `tests/harness/test_misc.c`  
**Related plans:** `doc/misc/RGL_MIGRATION_PLAN.md` (Phase 7A — fonts/text), `doc/plan/TEST_HARNESS_TEXT_FONT_PLAN.md` (harness certification matrix), `doc/plan/DIGESTIBLE_EXAMPLES_PLAN.md` (example 18 text showcase), `doc/plan/API_cleanliness_plan.md` (image/font module split), `doc/plan/plan_handles_ssbo.md` (internal text sampler / bindless history), `doc/guide/font.md`, `doc/guide/text_rendering.md`, `doc/guide/image.md`  
**Constraint:** OpenGL + Vulkan parity on every new GPU text command. CPU paths must not require an active render pass.

### Shipped in v2.4.341

| Phase | Status | Highlights |
|-------|--------|------------|
| **F0** | ✅ | `SituationUnloadFont` destroys owned atlas + `glyph_info`; idempotent default font; harness `font_unload_destroys_atlas`; docs unified on single-call unload |
| **F1** | ✅ | `SituationBakeBitmapFontAtlas`; grid GPU draw for any baked bitmap font in `SituationCmdDrawTextEx` |
| **F2** | ✅ | `SituationPackedFont` + terminal / CP437 / ASCII / packed / VCR / VGA builders; `SituationLoadBitmapFontFromTexture`; builders in `situation_impl_image.h` |
| **F3** | ✅ | `SituationMeasureTextEx`, `SituationGetTextLineCount`; `SituationMeasureText` delegates to Ex |
| **F4** | ✅ | `SituationCmdDrawTextBoxed`, `SituationImageStampText` / `StampTextBoxed` (CPU); optional GPU shadow/outline **not** shipped |
| **F5** | 🟡 | `doc/guide/font.md` module guide; `text_rendering.md` GPU-only split; `situation_command_reference.md` + index; FFI wrappers regen — **example 18** pending |
| **F6** | 🟡 | RGL **bitmap** create / draw / measure / unload / boxed → Situation; TTF load/draw + stamp wrappers + dead atlas code removal **pending** |
| **F7** | 🟡 | OpenGL `text_rendering` 8/8; Vulkan harness startup issue on some machines; builder/stamp/boxed harness cases still to add |

**Still open:** F5.3–F5.4 (example 18 + DIGESTIBLE plan), F6 TTF + `RGL_StampTextToTexture*` + delete legacy RGL font atlas upload, F7 full harness matrix + Vulkan parity, optional `SituationBakeFontAtlasEx` / `SituationGetDefaultFont`.

---

## How to use this file

- [ ] Execute phases **F0 → F7** in order. Do not delete RGL font implementations until **F6** exit criteria are green.
- [ ] Before each phase merge, run **§10.5** regression checklist and satisfy **§10.6** phase gate.
- [ ] Every new public API: single-line `SITAPI` in the appropriate `situation_api_*.h`, implementation in `situation_impl_*`, trace entry in `situation_base_trace.h`.
- [ ] Every shipped GPU feature needs harness coverage in `test_text_rendering.c` (or `test_misc.c` for pure CPU/load).
- [x] When a phase ships: update `doc/UPDATELOG.md`, `doc/guide/font.md` + `text_rendering.md`, `doc/situation_api_index.md`, and cross-link from `RGL_MIGRATION_PLAN.md` §7A.
- [ ] **No rogue paths:** RGL must not upload font atlases via `_RGL_UploadTextureFromPixels` after **F6**. All atlas creation goes through Situation.
- [ ] **End goal:** `RGL_DrawText*` → `SituationCmdDrawText*` (or internal equivalent recorded during `RGL_End` / flush).

---

## 1. Goals and non-goals

### Goals

- **Single font handle:** `SituationFont` for TTF, grid bitmap, packed retro, and terminal fonts (no split `RGLBitmapFont` / `RGLTrueTypeFont` in Situation).
- **Full RGL font parity** on GPU and CPU where RGL had real implementations (not procedural stubs).
- **Command-buffer GPU text** as the fast path; CPU `SituationImageDrawText*` for baking, stamps, and SDF quality.
- **RGL delegates:** preserve `RGL_DrawText`, `RGL_CreateCP437Font`, etc. as wrappers; delete duplicated logic from `rgl.h`.
- **Fix documented gaps:** `SituationLoadBitmapFontFromMemory` must produce a GPU-drawable font after bake; multiline measure; lifecycle cleanup.

### Non-goals (initially)

- Full Unicode / CJK atlas baking (ASCII 32–126 + CP437 0–255 is the parity target).
- GPU SDF text shader (keep SDF on CPU `SituationImageDrawTextEx`).
- FreeType dependency (stay on stb_truetype + bitmap builders).
- `RGL_CreateBitmapFontFromSystemFont` procedural stub quality — replace with `SituationLoadFont` + `SituationBakeFontAtlas` or defer.
- Rewriting K-Term font atlas pipeline in this plan (may consume Situation builders later; not a blocker).

---

## 2. Current state audit

| Area | Situation today | RGL today | Gap |
|------|-----------------|-----------|-----|
| **GPU draw** | `SituationCmdDrawText` / `Ex` / `Boxed` — batched quads | Bitmap: `SituationCmdDrawText*` via wrappers ✅; TTF still legacy sprite path | TTF RGL → Situation (F6) |
| **Default font** | Built-in 8×8 CP437 atlas at init; zeroed `SituationFont` fallback | `RGL_CreateCP437Font` delegates to Situation ✅ | — |
| **TTF load** | `SituationLoadFont`, `LoadFontFromMemory` | `RGL_LoadTrueTypeFont` — **legacy** direct bake in `rgl.h` | F6.3 |
| **TTF bake** | `SituationBakeFontAtlas` — 512², ASCII 32–126 | 1024² in RGL TTF path | Optional `BakeFontAtlasEx` (F2.7) |
| **Bitmap load** | `LoadBitmapFontFromMemory` + `BakeBitmapFontAtlas` → GPU ✅ | Delegates to Situation create/bake ✅ | — |
| **Grid GPU draw** | Any grid atlas with layout fields ✅ | Bitmap draw delegates ✅ | — |
| **Terminal / packed** | All `SituationCreate*` builders ✅ | Thin wrappers → Situation ✅ | Delete duplicate bodies (F6.14) |
| **Atlas from sheet** | `SituationLoadBitmapFontFromTexture` ✅ | Via Situation builders / load | — |
| **Measure** | `MeasureText` / `Ex`, multiline ✅ | `RGL_MeasureText*` → `MeasureTextEx` ✅ | TTF measure via legacy font |
| **Line count** | `SituationGetTextLineCount` ✅ | Delegates ✅ | — |
| **Boxed / wrap** | `SituationCmdDrawTextBoxed` ✅ | `RGL_DrawTextBoxed` → Situation ✅ | — |
| **Styled text** | CPU SDF via `ImageDrawTextEx` | GPU shadow/outline/gradient still RGL multi-pass | Optional F4.4 / keep RGL-local |
| **Stamp** | `SituationImageStampText*` ✅ | `RGL_StampTextToTexture*` **legacy** | F6.10 |
| **Unload** | Single-call `SituationUnloadFont` ✅ | Bitmap unload delegates ✅ | TTF unload path |
| **Filtering** | NEAREST on bitmap atlases ✅ | Via Situation uploads | Remove RGL font `_RGL_UploadTextureFromPixels` (F6.14) |
| **Harness** | 8 OpenGL `text_rendering` tests ✅ | No dedicated RGL font module | F7 expand + Vulkan fix |

### 2.1 Known bug (blocks examples) — **resolved in F1**

`tests/harness/test_text_rendering.c` — custom bitmap fonts bake and draw on GPU (`font_unload_destroys_atlas`, `cmd_draw_text_bitmap`). Examples can use `LoadBitmapFontFromMemory` → `BakeBitmapFontAtlas` → `CmdDrawTextEx`.

---

## 3. Target architecture

```
  Embedded data / TTF file / packed bits / texture sheet
              │
              ▼
     SituationFont (unified handle)
              │
    ┌─────────┴─────────┐
    ▼                   ▼
 CPU metadata          GPU atlas
 (bitmap_data,         (atlas_texture,
  stbFontInfo,          glyph_info OR grid layout)
  grid fields)
    │                   │
    ▼                   ▼
 SituationImage*      SituationCmdDrawText*
 SituationImageStamp* SituationCmdDrawTextBoxed
 Measure / line count  (recorded on command buffer)
```

**RGL end state:**

```c
// rgl.h — after F6
SITAPI void RGL_DrawText(const char* text, Vector2 pos, RGLBitmapFont font, Color color) {
    SituationCommandBuffer cmd = _RGL_GetActiveCommandBuffer(); // or flush-time buffer
    SituationCmdDrawTextEx(cmd, _RGL_FontToSit(font), text, pos, 0.0f, font.char_spacing, _RGL_ColorToRGBA(color));
}
```

RGL **does not** loop glyphs or upload font pixels after migration.

---

## 4. `SituationFont` struct extension

- [x] Add grid layout fields to `SituationFont` in `situation_api_platform.h` (names stable before implementation)

Proposed additions to `situation_api_platform.h` (names stable before implementation):

```c
typedef struct SituationFont {
    void *fontData;
    void *stbFontInfo;

    SituationTexture atlas_texture;
    void* glyph_info;           /* stbtt_bakedchar[96] when TTF baked */
    int atlas_width;
    int atlas_height;
    float font_height_pixels;

    bool is_bitmap;
    const unsigned char* bitmap_data;
    int bitmap_width;
    int bitmap_height;
    int bitmap_count;

    /* --- NEW: grid font layout (RGL parity) --- */
    int first_char;             /* ASCII code of index 0 (often 0 or 32) */
    int chars_per_row;          /* cells per row in atlas (often 16) */
    int chars_per_col;          /* cells per column in atlas */
    int display_cell_width;     /* glyph cell width in atlas (may include padding) */
    int display_cell_height;    /* glyph cell height in atlas */
    float char_spacing;         /* default extra horizontal spacing */
    float line_spacing;         /* default extra vertical spacing between lines */
} SituationFont;
```

**Rules:**

- **TTF baked:** `is_bitmap = false`, `glyph_info != NULL`, grid fields zero/ignored.
- **Grid bitmap baked:** `is_bitmap = true`, `glyph_info == NULL`, `atlas_texture` valid, grid fields set.
- **CPU-only bitmap (legacy):** `is_bitmap = true`, no atlas until `SituationBakeBitmapFontAtlas`.

---

## 5. Proposed public API surface

Signatures are proposals until implemented. Prefer `SituationError` for load/bake; keep draw commands consistent with existing `SituationCmdDrawTextEx`.

### 5.1 Loading and baking

- [x] Ship lifecycle fixes for existing load/bake APIs (F0)
- [ ] Ship `SituationFontBakeConfig` + `SituationBakeFontAtlasEx` (F2.7 — optional)
- [x] Ship `SituationBakeBitmapFontAtlas` (F1)
- [x] Ship `SituationLoadBitmapFontFromTexture` (F2)

```c
/* Existing — fix lifecycle in F0 */
SITAPI SituationError SituationLoadFont(const char* fileName, SituationFont* out_font);
SITAPI SituationError SituationLoadFontFromMemory(const void* data, int dataSize, SituationFont* out_font);
SITAPI SituationError SituationLoadBitmapFontFromMemory(
    const unsigned char* data, int char_width, int char_height, int num_chars, SituationFont* out_font);
SITAPI SituationError SituationBakeFontAtlas(SituationFont* font, float fontSizePixels);

/* NEW */
typedef struct SituationFontBakeConfig {
    int atlas_width;            /* 0 = default (512 TTF, auto bitmap) */
    int atlas_height;           /* 0 = default */
    int first_char;             /* TTF bake start codepoint */
    int char_count;             /* TTF bake count */
} SituationFontBakeConfig;

SITAPI SituationError SituationBakeFontAtlasEx(
    SituationFont* font, float fontSizePixels, const SituationFontBakeConfig* config);

SITAPI SituationError SituationBakeBitmapFontAtlas(SituationFont* font);
/* Expands bitmap_data into RGBA atlas, uploads atlas_texture, sets grid layout.
 * Requires is_bitmap && bitmap_data. Uses NEAREST, no mips. */

SITAPI SituationError SituationLoadBitmapFontFromTexture(
    SituationTexture sheet, int char_width, int char_height, int first_char, SituationFont* out_font);
/* Grid atlas already on GPU — fills grid metadata only (RGL_LoadBitmapFont parity). */
```

### 5.2 Packed / retro builders (move from RGL)

- [x] Ship `SituationPackedFont` + all `SituationCreate*Font` builders (F2)
- [x] Ship internal grid atlas build in `situation_impl_image.h` (F2)

```c
typedef struct SituationPackedFont {
    /* Same fields as RGLPackedFontConfig — bit layout, padding, outline colors, atlas layout */
    int char_width;
    int char_height;
    int display_height;
    int char_count;
    int first_char;
    int chars_per_row;
    int bits_per_row;
    int data_bits;
    int data_bit_offset;
    bool bit_order_msb_first;
    int top_padding, bottom_padding, left_padding, right_padding;
    int atlas_chars_per_row;
    int atlas_chars_per_col;
    bool enable_outline;
    int outline_thickness;
    unsigned char outline_r, outline_g, outline_b, outline_a;
    unsigned char font_r, font_g, font_b, font_a;
} SituationPackedFont;

SITAPI SituationError SituationCreateTerminalFontFromMemory(
    const unsigned char* data, int char_width, int char_height,
    int char_count, int chars_per_row, int first_char, SituationFont* out_font);

SITAPI SituationError SituationCreateTerminalFontEx(
    const unsigned char* data, int char_width, int char_height,
    int char_count, int chars_per_row, int first_char,
    float char_spacing, float line_spacing, SituationFont* out_font);

SITAPI SituationError SituationCreateCP437Font(
    const unsigned char* font_data_8x16, SituationFont* out_font);

SITAPI SituationError SituationCreateASCIIFont(
    const unsigned char* data, int cw, int ch, SituationFont* out_font);

SITAPI SituationError SituationCreatePackedBitmapFont(
    const void* packed_data, const SituationPackedFont* config, SituationFont* out_font);

SITAPI SituationError SituationCreateOutlinedPackedBitmapFont(
    const void* packed_data, const SituationPackedFont* config, SituationFont* out_font);

SITAPI SituationError SituationCreateVCRFont(
    const uint16_t* font_data, SituationFont* out_font);

SITAPI SituationError SituationCreateVCRFontWithOutline(
    const uint16_t* data, int outline_thickness, SituationFont* out_font);

SITAPI SituationError SituationCreateVGA8x8Font(
    const unsigned char* data, SituationFont* out_font);

SITAPI SituationError SituationCreateVGA8x8FontWithOutline(
    const unsigned char* data, int outline_thickness, SituationFont* out_font);
```

Convenience builders call internal shared `_Situation_BuildGridAtlasFromCells` (ported from RGL `CreateTerminalFont` / packed unpack).

### 5.3 Layout and measurement

- [x] Extend `SituationMeasureText` / ship `SituationMeasureTextEx` (F3)
- [x] Ship `SituationGetTextLineCount` (F3)

```c
SITAPI SitRectangle SituationMeasureText(
    SituationFont font, const char* text, float fontSize); /* extend: multiline, spacing */

SITAPI SitRectangle SituationMeasureTextEx(
    SituationFont font, const char* text, float fontSize, float spacing);

SITAPI int SituationGetTextLineCount(
    SituationFont font, const char* text, float max_width);
```

### 5.4 GPU drawing

- [x] Extend `SituationCmdDrawTextEx` grid path (F1)
- [x] Ship `SituationCmdDrawTextBoxed` (F4)
- [ ] (Optional) Ship `SituationCmdDrawTextWithShadow` / `WithOutline` (F4)

```c
SITAPI SituationError SituationCmdDrawText(...);      /* existing */
SITAPI SituationError SituationCmdDrawTextEx(...);   /* existing — extend grid path */

SITAPI SituationError SituationCmdDrawTextBoxed(
    SituationCommandBuffer cmd, SituationFont font, const char* text,
    SitRectangle bounds, float fontSize, float spacing,
    ColorRGBA color, bool word_wrap);
```

Optional (F4 — can stay RGL-only if time-constrained):

```c
SITAPI SituationError SituationCmdDrawTextWithShadow(...);
SITAPI SituationError SituationCmdDrawTextWithOutline(...); /* GPU multi-pass; CPU SDF preferred */
```

### 5.5 CPU stamp (preferred over RGL GPU stamp)

- [x] Ship `SituationImageStampText` (F4)
- [x] Ship `SituationImageStampTextBoxed` (F4)

```c
SITAPI SituationError SituationImageStampText(
    SituationImage* dst, SituationFont font, const char* text,
    Vector2 pos, float fontSize, ColorRGBA text_color, ColorRGBA bg_color);

SITAPI SituationError SituationImageStampTextBoxed(
    SituationImage* dst, SituationFont font, const char* text,
    SitRectangle bounds, float fontSize, ColorRGBA text_color,
    ColorRGBA bg_color, bool word_wrap, int* out_width, int* out_height);
```

### 5.6 Lifecycle

- [x] Fix `SituationUnloadFont` full teardown (F0)
- [ ] (Optional) Ship `SituationGetDefaultFont` (F5)

```c
SITAPI void SituationUnloadFont(SituationFont font);
/* F0: also SituationDestroyTexture on atlas, free glyph_info, clear grid fields */

SITAPI SituationFont SituationGetDefaultFont(void); /* optional: explicit default handle */
```

---

## 6. Implementation layout (new `sit/` code)

To avoid further bloat in `situation_impl_renderer.h`:

- [x] Grid/bitmap font builder helpers live in `situation_impl_image.h` (merged from former `situation_font_builders.h`)
- [x] Implement builders in `situation_impl_image.h` (ported from `rgl.h`)
- [x] Extend `situation_impl_image.h` — bake, measure, stamp, unload
- [x] Extend `situation_impl_renderer.h` — grid `CmdDrawTextEx`, `CmdDrawTextBoxed`, GL/VK execute
- [ ] Keep embedded glyph data deduped across examples → `sit_default_8x8_font` (ongoing cleanup)

---

## 7. Phases, actionables, exit criteria

### Phase F0 — Hygiene and lifecycle ✅

**Purpose:** Fix ownership bugs and doc lies before adding features.

- [x] **F0.1** — `SituationUnloadFont`: destroy `atlas_texture`, free `glyph_info`, zero grid fields — **idempotent** if atlas already destroyed (`situation_impl_image.h`)
- [x] **F0.2** — Align docs: single-call unload contract (`font.md`, `text_rendering.md`, `image.md`)
- [x] **F0.3** — Harness: load TTF → bake → draw → unload → reload (`font_unload_destroys_atlas`, Roboto tests)
- [x] **F0.4** — Harness: `LoadBitmapFontFromMemory` + bake path (`test_text_rendering.c`)
- [x] **F0.5** — `SITUATION_ERROR_RESOURCE_INVALID` when draw without atlas
- [x] **F0.6** — Trace entries for font APIs (`situation_base_trace.h`)
- [x] **F0.7** — Harness destroy helper uses `UnloadFont` only (no double `DestroyTexture`)
- [x] **F0.8** — OpenGL `text_rendering` green at F0 merge

**Exit criteria:**

- [x] Font unload is single-call
- [x] Harness green OpenGL
- [ ] Harness green Vulkan (blocked on harness env on some machines — F7)
- [x] Docs match behavior

---

### Phase F1 — Bitmap atlas bake + GPU grid draw ✅

**Purpose:** Close the `LoadBitmapFontFromMemory` → GPU gap; foundation for all retro fonts.

- [x] **F1.1** — `SituationBakeBitmapFontAtlas` — 1bpp row-major → grid RGBA atlas
- [x] **F1.2** — Grid layout fields on `SituationFont`
- [x] **F1.3** — NEAREST filtering, mip clamp 0
- [x] **F1.4** — `SituationCmdDrawTextEx` uses grid fields for any baked bitmap atlas
- [x] **F1.5** — `\n`, `first_char`, `char_spacing` / `line_spacing` in grid path
- [x] **F1.6** — Harness: synthetic 8×8 bitmap bake + GPU draw (`font_unload_destroys_atlas`, `cmd_draw_text_bitmap`)
- [x] **F1.7** — Default grid + CP437-style paths via builders (F2)

**Exit criteria:**

- [x] `LoadBitmapFontFromMemory` → `BakeBitmapFontAtlas` → `CmdDrawTextEx` on OpenGL
- [ ] Same on Vulkan (F7)
- [x] Custom bitmap GPU text without zeroed-font hack

---

### Phase F2 — Move retro / packed builders to Situation ✅

**Purpose:** Port RGL font factory code into `sit/`; RGL wrappers call Situation (F6 partial).

- [x] **F2.1** — `SituationPackedFont` + packed/outlined unpack
- [x] **F2.2** — `SituationCreateTerminalFontFromMemory` / `Ex`
- [x] **F2.3** — `CreateCP437Font`, `CreateASCIIFont`
- [x] **F2.4** — `CreateVCRFont`, `CreateVGA8x8Font` (+ outline variants)
- [x] **F2.5** — `SituationLoadBitmapFontFromTexture`
- [ ] **F2.6** — Harness per builder (atlas nonzero, readback) — **not** dedicated tests yet
- [ ] **F2.7** — Optional: `SituationBakeFontAtlasEx` with 1024² for RGL TTF parity

**Exit criteria:**

- [x] All RGL `RGL_Create*Font` (bitmap) have Situation equivalents
- [ ] Byte-compare atlas harness (optional)

---

### Phase F3 — Layout and measurement parity ✅

- [x] **F3.1** — `SituationMeasureTextEx` with `spacing`; multiline height for `\n`
- [x] **F3.2** — `SituationGetTextLineCount`
- [x] **F3.3** — TTF measure uses baked metrics when `glyph_info` present
- [x] **F3.4** — Harness: `measure_text_multiline`, `get_text_line_count`

**Exit criteria:**

- [x] Measure APIs pass harness (grid + TTF)
- [ ] Formal RGL vs Situation 1px comparison (optional)

---

### Phase F4 — GPU layout draw + CPU stamp ✅ (optional GPU styled text deferred)

- [x] **F4.1** — `SituationCmdDrawTextBoxed` — cmd recorder + GL/VK execute
- [x] **F4.2** — `SituationImageStampText` / `StampTextBoxed`
- [ ] **F4.3** — Harness: boxed draw within scissor; stamp → texture readback
- [ ] **F4.4** — (Optional) `SituationCmdDrawTextWithShadow` — not shipped

**Defer to RGL wrapper layer:** `DrawTextGradient`, `DrawTextWave` (cosmetic multi-draw).

**Exit criteria:**

- [x] `RGL_DrawTextBoxed` implemented as wrapper without RGL glyph code
- [ ] `RGL_StampTextToTexture*` wrappers (F6.10)

---

### Phase F5 — Documentation, examples, wrappers 🟡

- [x] **F5.1** — `doc/guide/font.md` module guide; `text_rendering.md` GPU-only; `situation_command_reference.md` (`DrawTextBoxed`, `DrawMetricsOverlay`); cross-links in SDK / `situation_api.md`
- [x] **F5.2** — `situation_api_index.md` regen; FFI wrappers (Rust/Odin/Zig/Fortran/Modula2) with `SituationFont` + `SituationPackedFont` ABI
- [ ] **F5.3** — Example `18_text_showcase` — builders + boxed + stamp
- [ ] **F5.4** — `DIGESTIBLE_EXAMPLES_PLAN.md` — mark text arc complete

**Exit criteria:**

- [x] New user can follow `font.md` for CP437 / TTF / packed load without RGL
- [ ] Example 18 demonstrates full surface

---

### Phase F6 — RGL hook-up (delete duplicated logic) 🟡

**Purpose:** RGL calls Situation; remove atlas upload and glyph loops from `rgl.h`.

- [x] **F6.1** — `_RGL_SitFontFromBitmap` / `_RGL_BitmapFontFromSit` mapping helpers in `rgl.h`
- [x] **F6.2** — `RGL_Create*Font` (bitmap) → `SituationCreate*` + texture handle map
- [ ] **F6.3** — `RGL_LoadTrueTypeFont` → `SituationLoadFont` + bake (**legacy** `stbtt` path still in `rgl.h`)
- [x] **F6.4** — `RGL_UnloadBitmapFont` → `SituationUnloadFont` (TTF unload path partial)
- [x] **F6.5** — Text draw uses `_RGL_GetCmd()` during batching
- [x] **F6.6** — `RGL_DrawText` / `Ex` (bitmap) → `SituationCmdDrawTextEx`; `RGL_DrawTextTTF` **still legacy**
- [x] **F6.7** — `RGL_DrawTextBoxed` → `SituationCmdDrawTextBoxed`
- [x] **F6.8** — `RGL_MeasureText` → `SituationMeasureTextEx` (bitmap); TTF measure partial
- [x] **F6.9** — `RGL_GetTextLineCount` → `SituationGetTextLineCount`
- [ ] **F6.10** — `RGL_StampTextToTexture*` → `SituationImageStamp*` (**legacy** VD render path remains)
- [ ] **F6.11** — `RGL_DrawTextWithShadow/Outline` — still RGL multi-pass sprites
- [ ] **F6.12** — `_RGL_InitDebugTextSystem` — verify uses Situation CP437/default
- [ ] **F6.13** — Delete ported function bodies from `rgl.h` (TTF + stamp still have legacy bodies)
- [ ] **F6.14** — `_RGL_UploadTextureFromPixels` still used in `RGL_LoadTrueTypeFont` / legacy paths

**RGL command-buffer integration notes:**

- [x] Text records via `_RGL_GetCmd()` → `SituationGetMainCommandBuffer()` when batching
- [x] Bitmap text uses `SIT_OP_DRAW_TEXT_EX` path (not sprite batch per glyph)
- [ ] Document host frame contract in RGL header comment block

**Exit criteria:**

- [ ] `rgl_smoke_test.exe` with text labels verified
- [ ] Debug overlay text via Situation path only
- [ ] Zero `_RGL_UploadTextureFromPixels` in font create functions
- [ ] OpenGL + Vulkan smoke green

---

### Phase F7 — Hardening and CI 🟡

- [x] **F7.1** — OpenGL `text_rendering`: 8 tests (default, Roboto, bitmap unload, measure, line count)
- [ ] **F7.1b** — Add harness: boxed GPU draw, stamp→texture, per-builder smoke — see **`doc/plan/TEST_HARNESS_TEXT_FONT_PLAN.md`**
- [ ] **F7.2** — `test_misc` bitmap tests upgraded to GPU readback
- [ ] **F7.3** — Mark `RGL_MIGRATION_PLAN.md` §7A complete (after F6 exit)
- [ ] **F7.4** — Optional grep CI: no `stbtt_BakeFontBitmap` in `rgl.h`
- [ ] **F7.5** — Performance spot-check vs per-glyph RGL sprites

**Exit criteria:**

- [x] OpenGL `text_rendering` green (8/8)
- [ ] Vulkan `text_rendering` green (harness `STATUS_ENTRYPOINT_NOT_FOUND` on some envs)
- [ ] `misc` font filter + `rgl_smoke_test` with text

---

## 8. RGL API → Situation mapping (F6 hook-up checklist)

- [ ] `RGL_LoadBitmapFont` → `SituationLoadTexture` + `SituationLoadBitmapFontFromTexture`
- [ ] `RGL_LoadTrueTypeFont` → `SituationLoadFont` + `SituationBakeFontAtlas` (or Ex when shipped)
- [x] `RGL_CreateTerminalFont` → `SituationCreateTerminalFontFromMemory`
- [x] `RGL_CreateTerminalFontEx` → `SituationCreateTerminalFontEx`
- [x] `RGL_CreateCP437Font` → `SituationCreateCP437Font`
- [x] `RGL_CreateASCIIFont` → `SituationCreateASCIIFont`
- [x] `RGL_CreatePackedBitmapFont` → `SituationCreatePackedBitmapFont`
- [x] `RGL_CreateOutlinedPackedBitmapFont` → `SituationCreateOutlinedPackedBitmapFont`
- [x] `RGL_CreateVCRFont` (+ outline) → `SituationCreateVCRFont` (+ outline)
- [x] `RGL_CreateVGA8x8Font` (+ outline) → `SituationCreateVGA8x8Font` (+ outline)
- [ ] `RGL_CreateBitmapFontFromSystemFont` → `SituationLoadFont` + bake **or** keep RGL stub
- [x] `RGL_DrawText` → `SituationCmdDrawText` (bitmap)
- [x] `RGL_DrawTextEx` → `SituationCmdDrawTextEx` (bitmap)
- [ ] `RGL_DrawTextTTF` → `SituationCmdDrawTextEx` (unified font) — **legacy sprite path**
- [x] `RGL_DrawTextBoxed` → `SituationCmdDrawTextBoxed`
- [ ] `RGL_DrawTextWithShadow` → 2× `CmdDrawTextEx` or future cmd
- [ ] `RGL_DrawTextWithOutline` → multi-pass RGL sprites
- [ ] `RGL_DrawTextGradient` → RGL-local loop
- [ ] `RGL_DrawTextWave` → RGL-local loop
- [x] `RGL_MeasureText` → `SituationMeasureTextEx` (bitmap)
- [ ] `RGL_MeasureTextTTF` → `SituationMeasureTextEx` (partial / legacy)
- [x] `RGL_GetTextLineCount` → `SituationGetTextLineCount`
- [ ] `RGL_StampTextToTexture` → `SituationImageStampText` + `SituationCreateTexture`
- [ ] `RGL_StampTextToTextureAdvanced` → `SituationImageStampTextBoxed` + texture create
- [x] `RGL_UnloadBitmapFont` → `SituationUnloadFont`
- [ ] `RGL_UnloadTrueTypeFont` → `SituationUnloadFont`
- [ ] `_RGL_DrawDebugText` → `SituationCmdDrawTextEx` + default font

---

## 9. Test plan matrix

- [x] **`font_unload_destroys_atlas`** (F0) — reload after unload
- [x] **`cmd_draw_text_bitmap`** (F1) — custom bitmap GPU draw
- [ ] **`bitmap_cp437_8x16_draw`** (F1) — dedicated 8×16 cell harness
- [ ] **`create_terminal_font`** (F2) — atlas nonzero
- [ ] **`create_packed_vga_font`** (F2) — packed unpack
- [ ] **`create_vcr_font_outline`** (F2) — outline alpha
- [ ] **`load_bitmap_font_from_texture`** (F2) — sheet metadata
- [x] **`measure_text_multiline`** (F3) — `\n` height
- [x] **`get_text_line_count`** (F3) — wrap count
- [ ] **`draw_text_boxed`** (F4) — clipping / wrap
- [ ] **`image_stamp_text`** (F4) — CPU stamp + texture draw
- [x] **`roboto_ttf_bake_draw`** / **`roboto_ttf_ex_bounds`** (existing) — TTF regression
- [x] **`cmd_draw_text_ex_bounds`** / **`cmd_draw_text_screen_layout`** — default grid

Run after each phase:

- [x] OpenGL `text_rendering` module green (8/8 as of v2.4.341)
- [ ] Vulkan `text_rendering` module green
- [ ] OpenGL `misc` font filter green
- [ ] RGL smoke test green after F6 complete

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation\build"
$env:PATH = "build\dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\tests\sit_test_opengl.exe" --module text_rendering
& ".\tests\sit_test_vulkan.exe" --module text_rendering
& ".\tests\sit_test_opengl.exe" --module misc --filter font
```

RGL smoke (F6):

```powershell
& ".\examples\rgl_smoke_test.exe"
```

---

## 10. Impact analysis & regression mitigation

The original plan listed risks but did **not** inventory every shader, internal path, and caller. This section is the regression map. **No shader changes are required for F0–F4** unless optional styled-GPU text (F4.4) is shipped; most work is CPU atlas build + vertex generation + lifecycle.

### 10.1 Shaders and GPU pipelines (frozen unless noted)

| Asset | Role | Migration impact |
|-------|------|------------------|
| `sit/gpu/text.vert` | Screen-space glyph quads; projection from view UBO (VK) or `u_projection` (GL) | **No change** for F0–F4. Vertex *data* changes in `SituationCmdDrawTextEx` only. |
| `sit/gpu/text.frag` | Atlas sample; `max(tex.a, tex.r)` alpha; discard &lt; 0.01 | **No change** for F0–F4. Still not the quad shader. |
| `sit/gpu/quad.vert` / `quad.frag` | `SituationCmdDrawTexture`, `DrawQuad` | **Independent** — do not route text through quad pipeline. |
| `sit/gpu/ypq_grade.frag` | YPQ texture grade | **No text coupling** |
| Vulkan `text_pipeline` + `text_pipeline_layout` | Separate from `quad_pipeline` | Re-init only if text shader changes (not planned F0–F4). |
| GL `text_shader_program` | `SIT_GPU_PATH_TEXT_VERT` / `FRAG` | Same as VK. |
| Descriptor / sampler | `text_sampler_layout` (binding 0); font atlas uses `single_sampler_descriptor_set` per texture (v2.4.335 model) | **High regression surface** — any new baked bitmap atlas must get correct sampler + NEAREST (historical Vulkan invisible-text bugs). |

**Regression tests tied to shaders:** `test_text_rendering` (default grid + Roboto), `SituationDrawMetricsOverlay`, VD tests that composite HUD text.

### 10.2 Internal renderer / command-buffer touchpoints

| Location | What changes | Regression risk |
|----------|--------------|-----------------|
| `_SituationInitDefaultFont` (`situation_impl_renderer.h`) | Default 8×8 CP437 atlas; sets `sit_render.default_font` | **Critical** — all zeroed-font draws depend on this. Do not break `is_grid_font` fallback. |
| `SituationCmdDrawText` / `DrawTextEx` record path | Vertex loop, `is_grid_font` detection, TTF `stbtt_GetBakedQuad` | **Critical** — F1 extends grid branch; must keep default-font identity path green. |
| `SIT_OP_DRAW_TEXT` / `SIT_OP_DRAW_TEXT_EX` execute (GL) | `_SituationGLValidateInternalTextDrawReady`, bindless vs bound sampler | **High** — GL text execute path; run full `graphics` + `text_rendering` after F1. |
| Vulkan text draw in `SituationCmdDrawTextEx` | Ring buffer VBO, `text_pipeline` bind, font atlas descriptor | **High** — GTX 1070 reference machine. |
| `SitCommandPacket.args.draw_text_ex` | Embeds full **`SituationFont` struct** | **Medium** — struct extension (§4) grows packet; must remain ABI-stable within same build; zero new fields before use. |
| `SituationBakeFontAtlas` (`situation_impl_image.h`) | TTF 512² bake | **Medium** — F2 optional `BakeFontAtlasEx`; default path must stay 512/96 glyphs. |
| `SituationLoadBitmapFontFromMemory` | CPU metadata today | **Low** until F1; then callers must bake or use builders. |
| `SituationUnloadFont` (F0) | Will destroy `atlas_texture` + free `glyph_info` | **High** — breaks double-destroy pattern in `test_text_rendering.c` (`DestroyTexture` then `UnloadFont`). Make unload idempotent or update callers in F0. |
| `SituationMeasureText` (`situation_impl_image.h`) | F3 multiline | **Low** for GPU; CPU examples using measure for layout. |
| `SituationImageDrawText*` / `DrawCodepoint` | CPU stb paths; `is_bitmap` branch | **Medium** — must not regress SDF/`hello_world.c` CPU demo. |
| `SituationDrawMetricsOverlay` | Uses `sit_render.default_font` + `CmdDrawTextEx` | **Smoke** every phase. |

### 10.3 Call-site inventory (`SituationCmdDrawText*`)

**Pattern A — zeroed `SituationFont` (default atlas fallback)** — must never regress:

- `examples/shared/sit_example.h` — all numbered examples HUD
- `examples/02_draw_shapes`, `03_keyboard_and_mouse`, `04_play_a_sound`, `05_virtual_display_retro`, `06_audio_node_graph`, `07_ypq_color_grading`, `08_temporal_oscillators`, `09_midi_control`, `10_thread_pool`
- `examples/18_text_showcase`, `19_node_graph_piano`
- `examples/other/text_showcase.c`, `node_graph_piano_demo.c`, `digital_rain.c`, `mandelbrot.c`, `hello_vulkan.c`, `hello_modern.c`, `hello_fancy.c`, `text_test.c`
- `examples/demon_hunt/demon_hunt.c`
- `tests/harness/sit_test_hud.h`, `sit_test_stereo_scope.c`, `test_virtual_display.c` (where used)

**Pattern B — baked TTF (`LoadFont` + `BakeFontAtlas`)**:

- `tests/harness/test_text_rendering.c` (Roboto)
- `tests/harness/sit_graphics_test_helpers.h` (`graphics_test_acquire_baked_font`)
- `sit/aud/polysonix/examples/polysonix_test_situation.c`

**Pattern C — `LoadBitmapFontFromMemory` (CPU today; GPU after F1)**:

- `examples/other/hello_world.c` (CPU `ImageDrawTextEx` only)
- `examples/other/shader_lab_torus.c`, `shader_lab_raytrace2.c`, `vd_idle_standby_demo.c`
- `tests/harness/test_misc.c`, `sit_graphics_test_helpers.h`

**Pattern D — RGL only (F6)**:

- `doc/misc/rgl.h` — all `RGL_DrawText*`, debug overlay `_RGL_DrawDebugText`, drive scene labels

**Out of scope (separate stack — do not break accidentally)**:

- `sit/k-term/*` — own bitmap fonts, atlas, `KTerm_*` (not `SituationFont`). Future convergence optional.
- `examples/console/*` — K-Term host; no direct Situation GPU text in hot path.

### 10.4 ABI, wrappers, and struct extension

| Consumer | Risk | Mitigation |
|----------|------|------------|
| `SituationFont` size in public header | Rust/Odin/Zig/Fortran/Modula2 FFI mirrors struct | Extend **only at end** of struct; run wrapper regen (F5.2); no reordering existing fields. |
| `SituationUnloadFont` behavior change | Callers that `DestroyTexture` then `UnloadFont` | F0: idempotent destroy **or** grep-fix all callers (see §10.3 Pattern C). |
| `SituationCmdDrawTextEx` signature | Stable | No planned signature change. |
| Monolithic vs DLL | Same struct in app and `situation.dll` | Rebuild all examples/tests in same batch when struct changes. |

### 10.5 Regression mitigation checklist (run every phase)

- [ ] **Pre-change grep** — record baseline: `SituationCmdDrawText`, `SituationUnloadFont`, `DestroyTexture.*font`, `is_grid_font`, `default_font`
- [ ] **Default font** — `test_text_rendering.default_grid_font_draw` + any example using `sit_example.h` HUD (visual or harness)
- [ ] **TTF path** — `test_text_rendering` Roboto bake + draw + `DrawTextEx` size/spacing tests
- [ ] **Bitmap path** — after F1: `bitmap_bake_gpu_draw` + Pattern C examples (`shader_lab_*` if switched to GPU)
- [ ] **CPU path** — `hello_world.c` build; `SituationImageDrawTextEx` rotation/outline smoke
- [ ] **Unload** — after F0: reload font after unload; no double-free when `DestroyTexture` + `UnloadFont`
- [ ] **OpenGL full modules** — `text_rendering`, `misc` (font filter), `graphics` (no new text failures)
- [ ] **Vulkan full modules** — same three modules on reference GPU
- [ ] **VD compositing** — `test_virtual_display` if text drawn to VD pass
- [ ] **Metrics overlay** — `SituationDrawMetricsOverlay` / `M` key in examples
- [ ] **RGL** — after F6: `rgl_smoke_test.exe` + debug overlay text
- [ ] **No quad shader bleed** — textured quad tests still green (`descriptor_bind_sampled_texture`, checkerboard draw)
- [ ] **Sampler / NEAREST** — new bitmap atlases: confirm filtering in texture create path (regression: blurry or invisible text)
- [ ] **Wrapper bindings** — after public API/struct change: spot-check one wrapper compile (Rust or Odin)

### 10.6 Per-phase regression gates (do not advance until green)

| After phase | Required green |
|-------------|----------------|
| **F0** | `text_rendering` (default + Roboto); unload test; no crash on double-destroy callers |
| **F1** | + bitmap GPU readback; Pattern A default font unchanged |
| **F2** | + builder harness tests; Roboto + default still green |
| **F3** | + measure/line-count tests; draw paths unchanged |
| **F4** | + boxed + stamp tests |
| **F5** | Docs-only gate: full harness repeat |
| **F6** | + `rgl_smoke_test`; full `graphics` module both backends |
| **F7** | Full suite policy per harness README |

### 10.7 Commands (baseline capture before first edit)

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation\build"
$env:PATH = "build\dll;C:\msys64\mingw64\bin;$env:PATH"

# Call-site inventory (re-run after F6)
rg -l "SituationCmdDrawText" ..\examples ..\tests ..\sit\aud

# Internal hot paths
rg "is_grid_font|default_font|SIT_OP_DRAW_TEXT" ..\sit\situation_impl_renderer.h

# Shader surface (should stay stable F0-F4)
rg "SIT_GPU_PATH_TEXT|text\.frag|text\.vert" ..\sit

# Full regression sweep
& ".\tests\sit_test_opengl.exe" --module text_rendering
& ".\tests\sit_test_vulkan.exe" --module text_rendering
& ".\tests\sit_test_opengl.exe" --module graphics
& ".\tests\sit_test_vulkan.exe" --module graphics
```

---

## 11. Risk register

| Risk | Mitigation |
|------|------------|
| RGL text outside render pass | Document; assert in debug; defer record to `RGL_End` inside pass |
| Vulkan text sampler regression | Harness readback every phase; NEAREST on all bitmap atlases |
| Struct size / wrapper breakage | Keep `RGLBitmapFont` as view over `SituationFont` or static cache with id |
| Duplicate stb in RGL and Sit | RGL removes direct `stbtt_BakeFontBitmap` calls in F6 |
| K-Term font paths diverge | Later task: K-Term uses `SituationCreateTerminalFont` |
| Atlas too small (1024 vs 512) | `SituationBakeFontAtlasEx` config; default 512, RGL wrapper passes 1024 |

---

## 12. Definition of done (project complete)

- [ ] All phases F0–F7 exit criteria met (F0–F4 ✅; F5–F7 partial)
- [ ] `doc/misc/rgl.h` font section is **wrappers only** (no atlas pixel loops, no `stbtt_BakeFontBitmap` in TTF path)
- [ ] Every `RGL_DrawText*` call path reaches `SituationCmdDrawText*` on the main command buffer
- [x] `doc/guide/font.md` documents full Situation font surface without requiring RGL
- [ ] `RGL_MIGRATION_PLAN.md` Phase 7A marked complete
- [ ] OpenGL + Vulkan harness green for `text_rendering` + `misc` font tests + `rgl_smoke_test`

---

## 13. Suggested execution order (single maintainer)

- [ ] **Week 1** — F0 + F1: custom bitmap GPU text works
- [ ] **Week 2** — F2: all builders in Situation
- [ ] **Week 3** — F3 + F4: layout + stamp
- [ ] **Week 4** — F5 + F6: docs + RGL hooks
- [ ] **Week 5** — F7: CI hardening

Parallel work: wrapper bindings (F5.2) can trail public API by one phase.

---

## 14. Changelog (this document)

| Date | Change |
|------|--------|
| 2026-06-23 | Initial plan — audit, API proposals, phases F0–F7, RGL mapping, test matrix |
| 2026-06-23 | All actionables converted to `- [ ]` checkboxes |
| 2026-06-23 | §10 impact analysis, call-site inventory, shader audit, regression checklist |
| 2026-06-23 | v2.4.341 completion ledger: F0–F4 ✅; F5 docs/wrappers ✅; F6 bitmap RGL ✅; audit table + phase checkboxes updated |
