# Situation UPDATELOG — v2.4.x (part 5 of 5)

Patches **2.4.401 "Profiling File Layout"** through **2.4.409 "K-Term Display Fix"** (9 entries, oldest first).

Index: [`UPDATELOG.md`](UPDATELOG.md) · Previous: [`updatelog_24_04.md`](updatelog_24_04.md) · Next: —

---

## [v2.4.401 "Profiling File Layout"] - 2026-06-29

**Tracy / P10.2 housekeeping** — separate build glue, instrumentation headers, and SITAPI surface.

### Changes — Layout

| Was | Now | Rationale |
|-----|-----|-----------|
| `sit/tracy_client.cpp` | **`build/tracy_client.cpp`** | Build-only Tracy client TU; not library source |
| `sit/situation_prof_macros.h` | **`sit/situation_profiling.h`** | Clear naming; not an API submodule |
| Included via `situation_api.h` / `situation_impl_trace_prof.h` | **`sit/situation.h`** → `situation_profiling.h` | Zones are compile-time instrumentation, not SITAPI |
| `sit/situation_impl_trace_prof.h` | **removed** | Redundant pass-through |

### Changes — Code

| Area | Detail |
|------|--------|
| **`sit/situation.h`** | Includes `sit/situation_profiling.h` after `situation_api.h` |
| **`sit/situation_api.h`** | No longer includes profiling macros |
| **`sit/Makefile`** | `TRACY_CLIENT_SRC := $(ROOT)/build/tracy_client.cpp` |

### Changes — Documentation

| Area | Detail |
|------|--------|
| **`doc/architecture.md`** | § Profiling instrumentation layout (v2.4.401+) |
| **`doc/COMPILATION_GUIDE.md`**, **`doc/situation_sdk.md`** | File hierarchy + Tracy include contract |
| **`.kiro/steering/situation-project.md`** | Architecture tree, key rule #3, `SITUATION_ENABLE_TRACY` define |

### Verification

**Build @ v2.4.401:** `build_situation.bat opengl` and `build_situation.bat opengl tracy` — OK (Tracy path compiles `build/tracy_client.cpp`).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.401.

---

---

## [v2.4.402 "Tone Synth Patch Recall Fix"] - 2026-06-29

**Graph tone synth patch memory** — `SituationSetControl` no longer skips patch **recall** / **save** when the control value is unchanged.

### Changes — Library

| Area | Detail |
|------|--------|
| **`sit/aud/node_graph_impl.h`** | `patch_slot` (ctrl **37**): always run recall handler (e.g. slot **0** → **0** after editing waveform). `patch_store` (ctrl **38**): save on every `SetControl` ≥ **0.5**, not only when transitioning from **0**. |
| **`sit/aud/tone_synth_graph.h`** | `SITUATION_TONE_SYNTH_PATCH_PARAM_FIRST` / `_LAST` derive `PATCH_PARAM_COUNT`; `_Static_assert` on `patch_slots[].param[]`; save/recall `memcpy` uses `PATCH_PARAM_FIRST`. |

### Root cause

`SituationSetControl` returned early when `value == node->control_values[control_id]`, so **`patch_memory`** harness (save slot **0**, change waveform, `SetControl(..., 37, 0)`) never recalled. MIDI **CC114** still recalls only on CC value change (`patch_last_cc_slot` edge); API path is explicit recall.

### Changes — Documentation

| Area | Detail |
|------|--------|
| **`doc/tone_synth.md`** | Patch slot/store `SetControl` behaviour @ v2.4.402 |
| **`doc/whatsnew.md`**, **`doc/UPDATELOG.md`** | v2.4.402 release notes |

### Verification

**Results @ v2.4.402:** `tone_synth` **`patch_memory`** **1/1** OpenGL static; full GL+VK suite reported green by maintainer.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_tests.bat" static-opengl
Set-Location "build"
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\tests\sit_test_opengl.exe" --module tone_synth --filter patch_memory
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.402.

---

## [v2.4.403 "PE Version Stamping"] - 2026-06-29

**Application EXE PE resources** now read the version triple from **`sit/situation_base_version.h`** at build time — no hardcoded drift in `sit_app.rc`.

### Problem

DLL builds already injected `SIT_VERSION_*` via `sit/Makefile` (`situation_resource.rc`). Example and harness EXEs linked **`sit_app.rc`** without `-DSIT_VERSION_*`, falling back to a stale **2.4.399** default while the library reported **2.4.402** via `SituationGetVersionString()`.

### Changes — Build

| Area | Detail |
|------|--------|
| **`build/sit_version.mk`** | Shared Make fragment: `SIT_VERSION_WINDRES_FLAGS` from `situation_base_version.h` |
| **`scripts/read_situation_version.py`** | `--windres` / `--string` / `--make` for batch and manual PE stamping |
| **`sit/Makefile`** | Includes `sit_version.mk` (DLL + shared flags) |
| **`tests/harness/Makefile`** | Passes `$(SIT_VERSION_WINDRES_FLAGS)` to `windres`; depends on version header |
| **`build/build_examples.bat`** | Reads version via Python; passes flags to `windres` |
| **`sit/platform/windows/sit_app.rc`** | Removed hardcoded version defaults; requires build-supplied `-D` flags (same model as `situation_resource.rc`) |

### Changes — Documentation

| Area | Detail |
|------|--------|
| **`doc/COMPILATION_GUIDE.md`** | Application identity § PE version source |
| **`scripts/README.md`**, **`.kiro/steering/situation-project.md`** | Version stamping notes |

### Verification

**Build @ v2.4.403:** `read_situation_version.py --string` → **2.4.403**; `build_tests.bat static-opengl` and `build_examples.bat static-opengl 01_open_a_window` compile `sit_app.rc` with injected version — OK.

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.403.

---

## [v2.4.404 "Grid Subsystem Phase B"] - 2026-06-29

**Situation Grid** — first-party cell-layer API, GPU shaders in `sit/gpu/` only, console-facing `SitGridCell.code` naming.

### Changes — Grid API & implementation

| Area | Detail |
|------|--------|
| **`sit/situation_api_grid.h`** | `SituationGrid*` lifecycle, upload, dispatch, present; `SitGridCell` with **`code`** (not `glyph`), `fg`/`bg`, `flags`, `attr0`/`attr1`, `version`; `SIT_GRID_ATTR_*` + `SIT_GRID_COLOR_*` |
| **`sit/situation_impl_grid.h`** | Surface, SSBO pack (`code` → `GPUCell.char_code`), pipeline compile, `SituationGridPresent` (compute-target VD); **loads `sit/gpu/grid_preamble.glslh` + `grid.comp` via `_SituationLoadCoreShaderFile`** — no embedded GLSL in C |
| **`sit/gpu/grid_preamble.glslh`** | Siamese GL/VK binding preamble (runtime concat with `grid.comp`; VK gets `#define VULKAN_BACKEND` prefix) |
| **`sit/situation_api.h`** | Includes `situation_api_grid.h` |
| **`sit/k-term/kt_grid_sit.h`** | `KTerm_PackSitGridCell` maps to `SitGridCell.code` |

### Changes — Harness

| Area | Detail |
|------|--------|
| **`tests/harness/test_grid.c`** | `cell_checkerboard`, `vd_present` — `--module grid` |
| **`tests/harness/sit_test_registry.c`**, **`tests/harness/Makefile`** | Grid module registered |

### Changes — Plans & specs

| Area | Detail |
|------|--------|
| **`doc/plan/GRID_RENDER_PLAN.md`** | Phase A/B progress, locked decisions (`code`, gpu-folder shaders), checkbox updates |
| **`.kiro/specs/situation-grid/requirements.md`** | `SitGridCell.code` field documented |

### Verification

**Results @ v2.4.404:** `--module grid` **2/2** OpenGL static (`cell_checkerboard`, `vd_present`). Pixel readback luma check deferred (GL push-constant path). Vulkan grid harness pending.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_tests.bat" static-opengl
& ".\build\run_tests.bat" opengl --module grid
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.404.

---

## [v2.4.405 "Grid Subsystem Phase C"] - 2026-06-29

**Situation Grid stacking** — multiple `SituationGridSurface` layers composited bottom→top onto a compute-target VD; per-grid scroll and `GRID_PASS_BLEND`.

### Changes — Stack API & implementation

| Area | Detail |
|------|--------|
| **`sit/situation_api_grid.h`** | `SituationGridStack*`, `SitGridPassMode` (`CELL_ONLY` / `BLEND` / `COLLIDE`), `SitGridRole`, `SituationGridSetScroll`, `SituationGridSetRole`, `SituationGridStackPresent` |
| **`sit/situation_impl_grid.h`** | Stack topology validation (≤8 entries), z-order sort, first visual `CELL_ONLY` then `BLEND`; compute→compute barrier between layers; scroll packed as 8.8 fixed in push constants; shader loader injects `#define VULKAN_BACKEND` **after** `#version` |
| **`sit/gpu/grid_preamble.glslh`** | **`#version 460` first** (fixes OpenGL compile when Siamese `#if` preceded version) |
| **`sit/gpu/grid.comp`** | `GRID_PASS_BLEND` pass-through (`code == 0` && `bg.a == 0`); per-grid scroll wrap; sign convention: positive scroll shifts content left |

### Changes — Harness

| Area | Detail |
|------|--------|
| **`tests/harness/test_grid.c`** | `stack_two_layer`, `stack_scroll`, `stack_skip_collision` — **`--module grid` 5/5** OpenGL static |

### Changes — Plans

| Area | Detail |
|------|--------|
| **`doc/plan/GRID_RENDER_PLAN.md`** | Phase C.1–C.2 + C.4 checkboxes; tracker 🟡 in progress (example 27 + VK pending) |

### Verification

**Results @ v2.4.405:** `--module grid` **5/5** OpenGL static (`cell_checkerboard`, `vd_present`, `stack_two_layer`, `stack_scroll`, `stack_skip_collision`).

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_tests.bat" static-opengl
& ".\build\run_tests.bat" opengl --module grid
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.405.

---

## [v2.4.406 "Grid Subsystem Phase D"] - 2026-06-30

**Situation Grid actor layer** — interim entity path until Phase K sprites: dedicated stack layer cleared and restamped each frame via **`SituationGridClear`** / **`SituationGridBlitCells`**.

### Changes — API & implementation

| Area | Detail |
|------|--------|
| **`sit/situation_api_grid.h`** | **`SituationGridClear`**, **`SituationGridBlitCells`** (row-major source, destination clip) |
| **`sit/situation_impl_grid.h`** | Bulk fill + blit with dirty-row tracking; shared **`_SitGridMarkDirtyRows`** helper |

### Changes — Example & harness

| Area | Detail |
|------|--------|
| **`examples/27_grid_playfield/main.c`** | **`g_actor_grid`** between BG and UI (z=1); 1×2 entity walks ground row each frame |
| **`tests/harness/test_grid.c`** | **`actor_over_tiles`**, **`blit_and_clear`** — **`--module grid` 7/7** OpenGL static |

### Changes — Documentation & plan

| Area | Detail |
|------|--------|
| **`doc/guide/grid.md`** | Actor grid pattern (clear + blit + stack order); API quick reference |
| **`doc/plan/GRID_RENDER_PLAN.md`** | Phase D tasks checked; foundation/naming sections from prior planning pass |

### Verification

**Results @ v2.4.406:** `--module grid` **7/7** OpenGL static (`cell_checkerboard`, `vd_present`, `stack_two_layer`, `stack_scroll`, `stack_skip_collision`, `actor_over_tiles`, `blit_and_clear`).

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_tests.bat" static-opengl
& ".\build\run_tests.bat" opengl --module grid
& ".\build\build_examples.bat" static-opengl grid_playfield
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.406.

---

## [v2.4.407 "Grid Subsystem Phase E"] - 2026-06-30

### Summary

CPU collision probe vs dedicated **collision grid** in the stack — `SetCollisionProbe`, `DispatchCollide`, `ReadCollisions`, `TestCollision`. Example 27 actor bounces off wall cells; harness `collision_probe`.

### Changes — API & implementation

| Area | Detail |
|------|--------|
| **`sit/situation_api_grid.h`** | `SitGridCollisionProbe`, `SitGridCollisionEvent`, `SitGridCollisionHeader`, norm flags, `SituationGridSetCollisionProbe` |
| **`sit/situation_impl_grid.h`** | CPU AABB resolve vs collision grid `cpu_cells`; stack collision event buffer; SSBO flush for future GPU pass |

### Changes — Example & harness

| Area | Detail |
|------|--------|
| **`examples/27_grid_playfield/main.c`** | **`g_collide_grid`** (walls + ground); actor bounce via **`SituationGridTestCollision`** |
| **`tests/harness/test_grid.c`** | **`collision_probe`** — **`--module grid` 8/8** OpenGL static |

### Changes — Documentation & plan

| Area | Detail |
|------|--------|
| **`doc/guide/grid.md`** | Collision grid section + frame contract |
| **`doc/plan/GRID_RENDER_PLAN.md`** | Phase E v1 tasks checked; E.2 GPU `GRID_PASS_COLLIDE` open |

### Verification

**Results @ v2.4.407:** `--module grid` **8/8** OpenGL static (`cell_checkerboard`, `vd_present`, `stack_two_layer`, `stack_scroll`, `stack_skip_collision`, `actor_over_tiles`, `blit_and_clear`, `collision_probe`).

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_situation.bat" static-opengl
& ".\build\build_tests.bat" static-opengl
& ".\build\run_tests.bat" opengl --module grid
& ".\build\build_examples.bat" static-opengl grid_playfield
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.407.

---

## [v2.4.409 "K-Term Display Fix"] - 2026-06-30

### Summary

Fix K-Term terminal display with **`KTERM_USE_SIT_GRID=1`**: cell backgrounds rendered but glyphs were missing because **`font_texture`** was never uploaded to the GPU when init-time `CreateFontTexture` ran before the loader GL context was ready.

### Changes — K-Term compositor

| Area | Detail |
|------|--------|
| **`sit/k-term/kt_composite_sit.h`** | Lazy font atlas upload in **`KTermCompositor_Prepare`** when `font_texture.generation == 0`; unified terminal render block (direct **`KTerm_UpdateBuffer`** + **`terminal.comp`** for both grid and legacy SSBO paths) |
| **`sit/k-term/kterm_impl.h`** | Resize recreates legacy terminal buffer with **`KTERM_BUFFER_USAGE_STORAGE_COMPUTE`** (matches init path) |

### Verification

Rebuild **`kterm_console`** and confirm visible text with default **`KTERM_USE_SIT_GRID=1`**.

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation\build"
& ".\build_examples.bat" opengl kterm_console
& ".\examples\kterm_console.exe"
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.409.

---

## [v2.4.410 "Phase F G4 Toggle Regression"] - 2026-06-30

### Summary

Complete Phase F **G4** regression for the dual-path toggle: legacy `KTERM_USE_SIT_GRID=0` builds again; both toggles pass `kterm_console` harness and `--module grid` (8/8 GL). Includes console perf and deprecation cleanups from the same workstream.

### Changes — Legacy toggle build fix

| Area | Detail |
|------|--------|
| **`sit/k-term/kterm_api.h`** | Move `KTERM_*_SHADER_PATH` macros and `terminal_compute_preamble` **above** `#include "kt_composite_sit.h"` so `kt_grid_sit.h` (pulled in early) sees legacy symbols when `KTERM_USE_SIT_GRID=0` |

**Root cause:** `kt_composite_sit.h` → `kt_grid_sit.h` was included before shader-path definitions; the `#else` branch in `KTerm_GridShaderPath` / `KTerm_GridComputePreamble` failed to compile on the legacy toggle.

### Changes — kterm_console (prior in same Phase F pass)

| Area | Detail |
|------|--------|
| **`examples/console/console_host_app.c`** | `render_thread_count = 1` when `SITUATION_ENABLE_RENDER_THREAD` — fixes ~1 FPS refresh (sync framebuffer readback every frame) |
| **`build/build_examples.bat`** | `kterm_console` in `all` build; default `KTERM_GRID_CFLAGS=-DKTERM_USE_SIT_GRID=1` |
| **`examples/console/console_impl/console_commands.h`** | `SituationEnumerateAudioDevices` / `SituationFreeDeviceList` (no deprecated API warnings) |
| **`sit/k-term/kt_render_sit.h`** | `KTerm_CmdBindBuffer` → `SituationCmdBindDescriptorSet` |

### Verification

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
$env:PATH = "build\dll;C:\msys64\mingw64\bin;$env:PATH"

$env:KTERM_USE_SIT_GRID = "0"
& ".\build\build_examples.bat" opengl kterm_console
& ".\build\tests\sit_test_opengl.exe" --module kterm_console

$env:KTERM_USE_SIT_GRID = "1"
& ".\build\build_examples.bat" opengl kterm_console
& ".\build\tests\sit_test_opengl.exe" --module kterm_console
& ".\build\tests\sit_test_opengl.exe" --module grid
```

Both toggle builds: **SUCCESS**. Harness: **kterm_console 1/1**, **grid 8/8** (OpenGL static).

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.410.

---

## [v2.4.411 "Phase F.1 FX Split"] - 2026-06-30

### Summary

Phase **F.1** shader split: CRT / scanline / glow / noise moved out of `terminal.comp` into optional **`terminal_fx.comp`** post-pass. Grid-path K-Term now compiles **`sit/gpu/grid.comp`** (via `grid_preamble.glslh`) instead of always loading `terminal.comp`.

### Changes

| Area | Detail |
|------|--------|
| **`sit/k-term/shaders/terminal_fx.comp`** | New optional FX post-pass (barrel sample, scanlines, vignette, glow, noise) |
| **`sit/k-term/shaders/terminal.comp`** | Legacy cell kernel slimmed — retains sixel blend, visual bell, VU meter, debug grid |
| **`sit/k-term/kterm_impl.h`** | `fx_pipeline` + `KTerm_LoadGridComputeShaderSource`; `KTerm_InitCompute` uses `KTerm_GridShaderPath()` |
| **`sit/k-term/kt_composite_sit.h`** | Dispatches FX pass after terminal/grid when `KTerm_TerminalFxActive()` |
| **`sit/k-term/kterm_api.h`** | `terminal_fx_compute_preamble`, `KTERM_TERMINAL_FX_SHADER_PATH`, `KTERM_GRID_SHADER_PREAMBLE_PATH` |

### Verification

Both toggles build; `kterm_console` harness passes (OpenGL).

```powershell
$env:KTERM_USE_SIT_GRID = "1"; & ".\build\build_examples.bat" opengl kterm_console
$env:KTERM_USE_SIT_GRID = "0"; & ".\build\build_examples.bat" opengl kterm_console
& ".\build\tests\sit_test_opengl.exe" --module kterm_console
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.411.

---

## [v2.4.412 "Phase F.3 Docs"] - 2026-06-30

### Summary

Phase **F.3** documentation: Situation grid as canonical compute cell path; K-Term documented as client; `SIT_COMPUTE_LAYOUT_GRID` named in guides and language wrappers.

### Changes

| Area | Detail |
|------|--------|
| **`doc/guide/compute.md`** | Canonical `SituationGrid*` pattern; layout table lists `SIT_COMPUTE_LAYOUT_GRID`; Pattern E section |
| **`doc/guide/virtual_display.md`** | **Grid playfield present** pattern + cross-link to [grid.md](guide/grid.md) |
| **`doc/guide/graphics.md`** | Grid layout row; `TERMINAL` marked deprecated alias |
| **`doc/introduction.md`** | 2D grid subsystem bullet + guide index link |
| **`wrappers/{python,rust,zig,odin}/`** | `SIT_COMPUTE_LAYOUT_GRID` alias/comment; `TERMINAL` deprecated (value 6 retained) |
| **`sit/k-term/README.md`** | F.2.4 dual-path table (`grid.comp`, `terminal_fx.comp`) |
| **`sit/k-term/doc/updatelog.md`** | v2.7.21 Phase F client + FX notes |

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.412.

---

## [v2.4.413 "Phase F Complete"] - 2026-06-30

### Summary

Phase **F** signed off: `grid_kterm_parity` harness compares `SituationGridPresent` (`grid.comp`) vs legacy `terminal.comp` on a shared 4×1 `SitGridCell` fixture; `--module grid` **9/9 GL**; full `sit_test` GL pass count **643** (≥ 636 baseline).

### Changes

| Area | Detail |
|------|--------|
| **`tests/harness/test_grid.c`** | `grid_kterm_parity` — dual VD side-by-side pixel readback (tol 18); transparent sixel dummy so legacy sixel blend does not black out output |
| **`doc/plan/GRID_RENDER_PLAN.md`** | F.4.2/F.4.3 ✅; Phase F tracker ✅; B.2.3 + G4 ✅ |

### Verification

```powershell
& ".\build\build_tests.bat" static-opengl
& ".\build\tests\sit_test_opengl.exe" --module grid          # 9/9
& ".\build\tests\sit_test_opengl.exe"                        # 643 passed GL
$env:KTERM_USE_SIT_GRID = "1"; & ".\build\build_examples.bat" opengl kterm_console
$env:KTERM_USE_SIT_GRID = "0"; & ".\build\build_examples.bat" opengl kterm_console
```

### Changes — Version

- **`sit/situation_base_version.h`**: v2.4.413.

---
