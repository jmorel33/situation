# Test Harness — Text & Font Certification Plan

**Status:** **T0–T6 complete** (v2.4.369 — OpenGL + Vulkan **25/25** `text_rendering`, **14/14** `text_retro_builders`)  
**Scope:** Expand and harden **`tests/harness/test_text_rendering.c`**, **`tests/harness/test_text_retro_builders.c`**, and related font coverage in **`test_misc.c`** so every supported font load path, draw option, and layout API is **certified by the harness** — not merely documented.  
**Non-scope:** RGL wrapper rewiring (`doc/plan/font_migration_plan.md` F6), K-Term font stack, new public font APIs, shader changes.  
**Primary files:** `tests/harness/test_text_rendering.c`, `tests/harness/test_text_retro_builders.c`, `tests/harness/sit_test_text_helpers.h`, `tests/harness/sit_test_retro_font_helpers.h`, `tests/harness/test_misc.c`, `tests/harness/sit_graphics_test_helpers.h`, `tests/harness/sit_test_assets.h`  
**Related:** `doc/plan/font_migration_plan.md` (F7 harness matrix), `doc/guide/font.md`, `doc/guide/text_rendering.md`

---

## How to use this file

- [x] Execute phases **T0–T6** (all shipped).
- [ ] Before closing a phase, run **§8** verification and tick **§5.6** gates.
- [ ] Core GPU text tests live in `test_text_rendering.c`; retro builder series in `test_text_retro_builders.c`; CPU-only in `misc`.
- [ ] Use `SIT_TEST_SKIP` for missing optional assets; never vacuous `[ OK ]` on skip.
- [ ] When a phase ships: tick checkboxes here, update **§3 inventory**, optional harness-only `UPDATELOG` note.

---

## 1. Progress snapshot

| Phase | Status | Notes |
|-------|--------|-------|
| **T0** Hygiene | ✅ complete | Shared helpers, `HARNESS_ASSETS.txt`, honest Roboto skip |
| **T1** Bitmap GPU | ✅ complete | L2 + L4 readback tests; misc cross-ref |
| **T2** Draw options | ✅ complete | `DrawTextBoxed` wrap/clip, multiline, color |
| **T3** Measure parity | ✅ complete | TTF + baked bitmap measure; measure vs draw |
| **T4** Retro smoke (legacy) | ✅ superseded | One-shot smokes moved to dedicated **T6** module |
| **T5** Stamp & lifecycle | ✅ complete | CPU stamp, GPU blit, reload, double-unload |
| **T6** Retro builder series | ✅ complete | 7 families × 2 stages (usable → display); G1+G2 **14/14** |

**Matrix coverage (§5):** **22 / 32** items ✅ · **10** remaining

---

## 2. Why this plan exists

Library font work (F0–F4) shipped load/bake/draw/measure/stamp APIs. Harness coverage is improving but still uneven:

| Area | Library support | Harness (current) |
|------|-----------------|-------------------|
| Default grid font (`SituationFont` zeroed) | ✅ | ✅ pixel readback (3 draw/layout tests) |
| TTF load + bake + GPU draw | ✅ | 🟡 Roboto only; honest `[SKIP]` if asset missing |
| Custom bitmap → `BakeBitmapFontAtlas` → GPU | ✅ | ✅ `bitmap_memory_bake_gpu_draw` + unload lifecycle |
| `LoadBitmapFontFromTexture` | ✅ | ✅ `load_bitmap_font_from_texture` |
| Retro builders (CP437, terminal, ASCII, packed, outlined, VCR, VGA) | ✅ | ✅ **T6** module — usable + display per family |
| `CmdDrawTextBoxed` | ✅ | ✅ wrap + clip readback |
| `ImageStampText` / `StampTextBoxed` | ✅ | ✅ |
| `MeasureText` / `Ex`, `GetTextLineCount` | ✅ | ✅ default grid + Roboto/bitmap baked measure |
| OpenGL vs Vulkan parity | required | ✅ GL + VK **25/25** + **14/14** retro (2026-06-26) |
| Error paths (draw without atlas, bad load) | ✅ | ✅ |

**Goal:** One module run (+ small `misc` filter) certifies that **font options behave as documented** on both backends.

---

## 3. Certification model

Each test asserts **observable behavior**, not just “no crash”:

1. **Load/bake** — `SITUATION_SUCCESS`; atlas `generation != 0`; grid metadata sane.
2. **Measure** — bounds / line count match known strings (within tolerance).
3. **GPU draw** — clear pass → draw → `EndFrame` → readback: bright text band, dark controls (`graphics_test_region_*`).
4. **CPU stamp** — stamp into `SituationImage` → sample pixels or blit + readback.
5. **Lifecycle** — `UnloadFont` destroys atlas; reload works; single-call unload contract.
6. **Backend parity** — same tests on OpenGL and Vulkan (`requires_context = true`).

**Skip policy:** `SIT_TEST_SKIP` + reason. Resolve paths via `sit_test_assets.h` (multi-prefix CWD).

---

## 4. Current harness inventory

### 4.1 `text_rendering` module — **25 tests** (OpenGL + Vulkan verified)

Tests run in **error-first** order: no-atlas GPU fallback, bad `LoadFont`, unload contract, optional Roboto probe — then default grid, bake/draw, TTF/stamp.

| Test | What it checks |
|------|----------------|
| `draw_without_bake_default_fallback` | Unbaked bitmap GPU draw → built-in default grid fallback |
| `font_load_missing_file_fails` | Missing TTF path → error; zeroed handle |
| `double_unload_safe` | Second `UnloadFont` on zeroed handle — no crash |
| `font_unload_destroys_atlas` | Synthetic bitmap bake + unload destroys texture slot |
| `roboto_asset_optional_probe` | Early honest `[SKIP]` when Roboto asset absent |
| `cmd_draw_text_bitmap` | Default grid `CmdDrawText` readback |
| `measure_text_multiline` | `MeasureTextEx` height grows with `\n` (default grid) |
| `get_text_line_count` | Line count + wrap at narrow width (default grid) |
| `measure_vs_draw_bounds` | `MeasureTextEx` box overlaps GPU readback bright region |
| `cmd_draw_text_ex_bounds` | Default grid: size/spacing → more bright pixels |
| `cmd_draw_text_screen_layout` | TOP/BOT layout in render space |
| `cmd_draw_text_boxed_wrap` | `DrawTextBoxed` word wrap → two lines in bounds |
| `cmd_draw_text_boxed_clip` | `DrawTextBoxed` no wrap → overflow not drawn |
| `cmd_draw_text_multiline_gpu` | `DrawTextEx` with `\n` → two bright bands |
| `cmd_draw_text_colored` | Red tint visible in readback |
| `bitmap_memory_bake_gpu_draw` | `LoadBitmapFontFromMemory` + bake + GPU readback (glyph `A`) |
| `load_bitmap_font_from_texture` | RGBA sheet + `LoadBitmapFontFromTexture` + GPU draw |
| `measure_text_bitmap_baked` | Custom bitmap bake: width ~2× when fontSize 8→16 |
| `measure_text_ttf_baked` | Roboto: width ~2× when fontSize 16→32 |
| `roboto_ttf_bake_draw` | TTF load + bake + draw (Roboto; `[SKIP]` if missing) |
| `roboto_ttf_ex_bounds` | TTF `DrawTextEx` scaling (Roboto) |
| `image_stamp_text_default` | `SituationImageStampText` — Roboto CPU stamp + bg |
| `image_stamp_text_boxed` | `SituationImageStampTextBoxed` — multiline + bg fill |
| `stamp_then_gpu_blit` | Stamp → `CreateTexture` → `CmdDrawTexture` readback |
| `reload_font_after_unload` | TTF load/bake/unload/reload cycle |

**Retro builders** moved to **`text_retro_builders`** (phase T6) — see §4.3.

**Assets:** `static/Roboto-Regular.ttf` or `Roboto-Regular.ttf` — see `tests/harness/assets/HARNESS_ASSETS.txt`.  
**Helpers:** `sit_test_text_helpers.h`, `sit_graphics_test_helpers.h`.

### 4.2 `misc` module — font subset (2 tests, CPU-only)

| Test | What it checks |
|------|----------------|
| `load_bitmap_font_from_memory` | Load metadata only; GPU path → `text_rendering.bitmap_memory_bake_gpu_draw` |
| `measure_text_bitmap_font` | Measure on **unbaked** bitmap font |

### 4.3 `text_retro_builders` module — **14 tests** (OpenGL + Vulkan verified)

Dedicated module for retro font builder certification. Each family runs **two stages** in order:

1. **Usable** — builder succeeds; atlas `generation != 0`; grid metadata matches fixture; `MeasureTextEx` width/height > 0 (terminal also checks `SituationCreateTerminalFontEx` spacing).
2. **Display surface** — via `sit_text_test_retro_display_surface`: white draw readback, colored tint, multiline bands, boxed wrap in bounds, measure-vs-draw overlap.

| Family | Usable test | Display test | Builder API |
|--------|-------------|--------------|-------------|
| CP437 | `retro_cp437_usable` | `retro_cp437_display` | `SituationCreateCP437Font` |
| Terminal | `retro_terminal_usable` | `retro_terminal_display` | `SituationCreateTerminalFontFromMemory` + Ex spacing |
| ASCII | `retro_ascii_usable` | `retro_ascii_display` | `SituationCreateASCIIFont` |
| Packed | `retro_packed_usable` | `retro_packed_display` | `SituationCreatePackedBitmapFont` |
| Outlined packed | `retro_outlined_packed_usable` | `retro_outlined_packed_display` | packed + outline config |
| VCR | `retro_vcr_usable` | `retro_vcr_display` | `SituationCreateVCRFont` |
| VGA 8×8 | `retro_vga_usable` | `retro_vga_display` | `SituationCreateVGA8x8Font` |

**Helpers:** `sit_test_retro_font_helpers.h` (fixtures, `assert_grid_font_usable`, `retro_display_surface`).  
**Registry:** runs immediately after `text_rendering` in `sit_test_registry.c`.

**Library fixes (T6):** `SituationCreateVGA8x8Font` and `SituationCreateVCRFont` now set white `font_r/g/b/a` (was invisible alpha 0 on GPU).

---

## 5. Target test matrix (actionables)

Tick when implemented **and** green on **G1** (OpenGL). Tick **G2** column when Vulkan matches.

### 5.1 Load & bake paths

- [x] **L1** `default_font_zero_handle` — zeroed `SituationFont` → covered by `cmd_draw_text_bitmap`
- [x] **L2** `bitmap_memory_bake_gpu_draw` — `LoadBitmapFontFromMemory` + `BakeBitmapFontAtlas` + readback
- [ ] **L3** `load_font_from_memory_ttf` — `LoadFontFromMemory` + `BakeFontAtlas` (embed bytes or reuse Roboto file)
- [x] **L4** `load_bitmap_font_from_texture` — sheet texture + grid metadata + draw
- [x] **L5** `create_cp437_font` — `SituationCreateCP437Font` → `retro_cp437_*` (T6)
- [x] **L6** `create_terminal_font_ex` — spacing affects measure → `retro_terminal_*` (T6)
- [x] **L7** `create_ascii_font` — `SituationCreateASCIIFont` → `retro_ascii_*` (T6)
- [x] **L8** `create_packed_bitmap_font` — minimal inline packed fixture → `retro_packed_*` (T6)
- [x] **L9** `create_outlined_packed_font` — outline alpha in readback → `retro_outlined_packed_*` (T6)
- [x] **L10** `create_vga8x8_font` — ASCII strip draw → `retro_vga_*` (T6)
- [x] **L11** `create_vcr_font` — VCR 16-bit builder → `retro_vcr_*` (T6; outline variant still optional)

### 5.2 GPU draw options

- [x] **D1** `cmd_draw_text_default` — `SituationCmdDrawText` (default grid)
- [x] **D2** `cmd_draw_text_ex_size_spacing` — `SituationCmdDrawTextEx` (default + Roboto)
- [x] **D3** `cmd_draw_text_boxed_wrap` — text inside bounds, word wrap on
- [x] **D4** `cmd_draw_text_boxed_clip` — word wrap off, overflow clipped
- [x] **D5** `cmd_draw_text_colored` — non-white tint in readback
- [x] **D6** `cmd_draw_text_multiline_gpu` — `\n` → two bright bands
- [ ] **D7** `cmd_draw_text_in_vd_pass` — optional; text in VD render pass

### 5.3 Layout & measure (CPU)

- [x] **M1** `measure_text_ex_multiline` — default grid
- [x] **M2** `measure_text_ttf_baked` — Roboto 16 vs 32 px width ratio
- [x] **M3** `measure_text_bitmap_baked` — synthetic bitmap after L2 bake
- [x] **M4** `get_text_line_count_wrap` — wrap count (default grid)
- [x] **M5** `measure_vs_draw_bounds` — measure band overlaps readback bright region

### 5.4 CPU stamp

- [x] **S1** `image_stamp_text_default` — `SituationImageStampText`
- [x] **S2** `image_stamp_text_boxed` — `SituationImageStampTextBoxed` + bg
- [x] **S3** `stamp_then_gpu_blit` — stamp → texture → `CmdDrawTexture`

### 5.5 Lifecycle & errors

- [x] **E1** `font_unload_destroys_atlas` — unload destroys atlas slot
- [x] **E2** `draw_without_bake_default_fallback` — unbaked bitmap → default grid GPU draw
- [x] **E5** `font_load_missing_file_fails` — bad TTF path → error, zeroed handle
- [x] **E3** `reload_font_after_unload` — load → bake → unload → load
- [x] **E4** `double_unload_safe` — `UnloadFont` on zeroed handle after unload

### 5.6 Backend parity gates

- [x] **G1** OpenGL — `text_rendering` **25 pass** + `text_retro_builders` **14 pass** (2026-06-26)
- [x] **G2** Vulkan — `text_rendering` **25 pass** + `text_retro_builders` **14 pass** (2026-06-26)
- [x] **G3** OpenGL — `misc --filter font` → CPU font tests pass

---

## 6. Shared harness infrastructure

File: **`tests/harness/sit_test_text_helpers.h`**

- [x] **`sit_text_test_fill_main_clear_pass`** — black clear render pass
- [x] **`sit_text_test_default_font`** — zeroed handle helper
- [x] **`sit_text_test_try_load_roboto_baked` / `sit_text_test_require_roboto_baked`** — asset resolve + bake + honest skip
- [x] **`sit_text_test_destroy_font`** — unload + zero handle
- [x] **`sit_text_test_fill_synthetic_bitmap`** — 8×8×256 glyph fixture (L2/E)
- [x] **`sit_text_test_count_bright_pixels`** — readback pixel count
- [x] **`sit_text_test_draw_text_ex_frame`** — draw + EndFrame + `LoadImageFromScreen`
- [x] **`sit_text_test_draw_text_boxed_frame`** — boxed draw + readback helper (T2)
- [x] **`sit_text_test_pixel_red_dominant` / `sit_text_test_region_any_red_dominant`** — color readback (T2)
- [x] **`sit_text_test_fill_terminal_glyph`** — grayscale grid cell fill (T4)
- [x] **`sit_text_test_fill_minimal_packed_config`** — inline packed font for L8/L9 (T4)
- [x] **`sit_text_test_fill_packed_glyph_solid` / `sit_text_test_fill_vga_glyph_solid`** — packed row fixtures (T4)
- [x] **`sit_text_test_assert_measure_width_scales`** — 16→32 px width ratio helper (T3)
- [x] **`sit_text_test_try_load_roboto_cpu` / `require_roboto_cpu`** — CPU stamp path (T5)
- [x] **`sit_text_test_pixel_blue_dominant` / `pixel_green_dominant`** — stamp bg readback (T5)
- [x] **`sit_test_retro_font_helpers.h`** — retro fixtures, usable assert, display surface harness (T6)
- [x] **`tests/harness/assets/HARNESS_ASSETS.txt`** — asset list for text/audio/model tests

**Asset rule:** All file paths via `sit_test_resolve_harness_asset` / `sit_test_resolve_harness_asset_any`.

---

## 7. Phases

Execute **T0–T6** complete. Optional follow-ups: **T4.5** (VCR/VGA outline builders), **L3**, **D7**, **T0.4**.

### Phase T0 — Hygiene & honesty ✅

**Purpose:** Fix misleading passes and path resolution before adding tests.

- [x] **T0.1** — Roboto helpers use `sit_test_assets.h`; shared header extracted
- [x] **T0.2** — Font tests use `SIT_TEST_SKIP` (no skip + `[ OK ]` pairs in text module)
- [x] **T0.3** — `tests/harness/assets/HARNESS_ASSETS.txt` documents optional Roboto + inline fixtures
- [ ] **T0.4** — (Optional) `run_tests.bat` warning when `--module text_rendering` and Roboto missing

**Exit:** ✅ Met (T0.4 optional, not implemented).

---

### Phase T1 — Bitmap GPU certification ✅

**Purpose:** Certify custom bitmap fonts end-to-end (closes `font_migration_plan.md` F1 harness gap).

- [x] **T1.1** — `bitmap_memory_bake_gpu_draw` (L2)
- [x] **T1.2** — `load_bitmap_font_from_texture` (L4)
- [x] **T1.3** — `misc.load_bitmap_font_from_memory` documents GPU path → L2 (no bake in no-context module)

**Exit:** ✅ L2 + L4 green OpenGL; module header + `HARNESS_ASSETS.txt` updated.

---

### Phase T2 — Layout draw options ✅

**Purpose:** Certify `CmdDrawTextBoxed` and multiline/colored GPU draws.

- [x] **T2.1** — `cmd_draw_text_boxed_wrap` (D3): long string, narrow bounds, word wrap on
- [x] **T2.2** — `cmd_draw_text_boxed_clip` (D4): word wrap off, right edge clipped
- [x] **T2.3** — `cmd_draw_text_multiline_gpu` (D6): two lines, vertical separation in readback
- [x] **T2.4** — `cmd_draw_text_colored` (D5): red tint vs dark background
- [x] **T2.5** — Add `sit_text_test_draw_text_boxed_frame` helper

**Exit:** ✅ D3–D6 on **G1**.

---

### Phase T3 — Measure parity ✅

**Purpose:** Measure APIs on baked TTF and bitmap fonts, not only default grid.

- [x] **T3.1** — `measure_text_ttf_baked` (M2): Roboto 16 vs 32 px width ratio
- [x] **T3.2** — `measure_text_bitmap_baked` (M3): synthetic bitmap after bake
- [x] **T3.3** — `measure_vs_draw_bounds` (M5): measured rect overlaps bright readback band
- [x] **T3.4** — `sit_text_test_assert_measure_width_scales` shared helper

**Exit:** ✅ M2–M5 on **G1**; no API changes.

---

### Phase T4 — Retro builder smoke ✅ (superseded by T6)

**Purpose:** One readback test per builder family — catches atlas unpack regressions. **Moved** to dedicated `text_retro_builders` module in T6 (two-stage usable → display).

- [x] **T4.1** — CP437 smoke (was `create_cp437_font`)
- [x] **T4.2** — terminal Ex spacing smoke
- [x] **T4.3** — packed bitmap smoke
- [x] **T4.4** — VGA 8×8 smoke
- [ ] **T4.5** — (Optional) VCR/VGA **outline** builder variants
- [x] **T4.6** — Run **G2** Vulkan full module; fix text pipeline parity failures

**Exit:** ✅ Superseded by T6. Library fix retained: `SituationCreateVGA8x8Font` white `font_r/g/b/a`.

---

### Phase T6 — Retro builder series ✅

**Purpose:** The largest certification block in the text/font plan — decouple “is the font usable?” from “what can we do on the display surface?” for every retro builder family.

**Stage A — usable (`*_usable`):**
- Builder returns `SITUATION_SUCCESS`
- `sit_text_test_assert_grid_font_usable` — atlas, cell size, `first_char`, `chars_per_row`
- `MeasureTextEx` on fixture string → width/height > 0
- Terminal: extra `SituationCreateTerminalFontEx` spacing widens measure

**Stage B — display (`*_display`):**
- `sit_text_test_retro_display_surface` — white `DrawTextEx` readback
- Colored tint (green-dominant band)
- Multiline `\n` → two bright bands
- `DrawTextBoxed` word wrap inside bounds
- Measure rect overlaps draw readback band

| Step | Tests |
|------|-------|
| **T6.1** | CP437 usable + display |
| **T6.2** | Terminal usable (+ Ex spacing) + display |
| **T6.3** | ASCII usable + display |
| **T6.4** | Packed usable + display |
| **T6.5** | Outlined packed usable + display |
| **T6.6** | VCR usable + display |
| **T6.7** | VGA 8×8 usable + display |
| **T6.8** | New module `test_text_retro_builders.c` + `sit_test_retro_font_helpers.h`; registry + Makefile |
| **T6.9** | Remove T4 smokes from `text_rendering.c`; error-first ordering preserved there |

**Exit:** ✅ L5–L11 (except optional outline variants) on **G1 + G2** — OpenGL + Vulkan **14/14** @ 2026-06-26. VCR/VGA builder `font_r/g/b/a` fix; force-rebuild Vulkan DLL if display tests fail after library change.

---

### Phase T5 — CPU stamp & lifecycle ✅

**Purpose:** Certify stamp APIs and error paths.

- [x] **T5.1** — `image_stamp_text_default` (S1): Roboto CPU stamp + bg readback
- [x] **T5.2** — `image_stamp_text_boxed` (S2): multiline boxed stamp + bg
- [x] **T5.3** — `stamp_then_gpu_blit` (S3): stamp → texture → `CmdDrawTexture`
- [x] **T5.4** — `draw_without_bake_default_fallback` (E2): documents default-grid GPU fallback
- [x] **T5.5** — `reload_font_after_unload` (E3): TTF reload after unload
- [x] **T5.6** — `double_unload_safe` (E4): zeroed handle second unload

**Exit:** ✅ S1–S3, E2–E5 on **G1 + G2** (OpenGL + Vulkan **29/29** @ 2026-06-26). §5 matrix ≥ **80%** checked.

---

## 8. Verification commands

From repo root (preferred — matches `run_tests.bat`):

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\run_tests.bat" opengl --module text_rendering
& ".\build\run_tests.bat" vulkan --module text_rendering
& ".\build\run_tests.bat" opengl --module text_retro_builders
& ".\build\run_tests.bat" vulkan --module text_retro_builders
& ".\build\run_tests.bat" opengl --module misc --filter "font"
```

Focused during development:

```powershell
& ".\build\tests\sit_test_opengl.exe" --module text_retro_builders --filter "cp437|vcr"
```

Prefer `run_tests.bat` for CI parity (repo-root CWD). Direct `build/tests/` launch works for assets via `sit_test_assets.h` prefix search.

---

## 9. Definition of done

- [x] §5 matrix: **L1–L2, L4–L11, D1–D6, M1–M5, S1–S3, E1–E5** checked; **L3, D7** deferred (§10)
- [x] **G1** green on reference machine (OpenGL **25/25** + **14/14** retro @ 2026-06-26)
- [x] **G2** green on reference machine (Vulkan **25/25** + **14/14** retro @ 2026-06-26)
- [x] No vacuous skip/pass pairs in text module; Roboto → honest `[SKIP]`
- [x] `font_migration_plan.md` F7 cross-links this file
- [ ] Harness-only `UPDATELOG` / `whatsnew` note when T2–T5 phases ship (patch bump optional)

---

## 10. Deferred / out of scope

| Item | Reason |
|------|--------|
| GPU shadow/outline/gradient text | Not in Situation API; RGL-local |
| `SituationBakeFontAtlasEx` / 1024² atlas | Optional library feature; add test when API ships |
| `SituationGetDefaultFont` | Optional; zero-handle tests suffice |
| K-Term `KTerm_*` fonts | Separate stack |
| Visual pixel-perfect gold images | Bright/dark regions enough for harness |
| Example 18 (`18_text_showcase`) | `font_migration_plan.md` F5.3 |
| **T0.4** run_tests Roboto warning | Optional CI nicety |

---

## 11. Changelog (this document)

| Date | Change |
|------|--------|
| 2026-06-26 | Initial plan — inventory, certification model, matrix, phases T0–T5 |
| 2026-06-26 | T0 + T1 shipped — `sit_test_text_helpers.h`, L2/L4 tests, OpenGL 10/10 |
| 2026-06-26 | T2 shipped — boxed wrap/clip, multiline GPU, colored draw; OpenGL 14/14 |
| 2026-06-26 | T3 shipped — measure TTF/bitmap scaling + measure-vs-draw; OpenGL 17/17 |
| 2026-06-26 | T4 shipped — CP437, terminal Ex, packed, VGA builders; OpenGL + Vulkan 21/21; VGA builder font alpha fix |
| 2026-06-26 | T5 shipped — CPU stamp/boxed, GPU blit, lifecycle; OpenGL + Vulkan 27/27 |
| 2026-06-26 | T6 shipped — `text_retro_builders` module (14 tests, 7× usable+display); L5–L11 matrix; VCR/VGA font alpha fix; T4 smokes removed from `text_rendering` (now 25 tests) |
