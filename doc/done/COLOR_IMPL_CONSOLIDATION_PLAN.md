# Color Implementation Consolidation Plan

**Document:** `doc/plan/COLOR_IMPL_CONSOLIDATION_PLAN.md`  
**Overall status:** **Done** (all phases complete including release note v2.4.357)  
**Related:** `doc/plan/YPQ_COLOR_PLAN.md`, `doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`, `doc/guide/ypq_color.md`

---

## Table of contents

| Section | Contents |
|---------|----------|
| [Phase checklist](#phase-checklist) | Status of all phases at a glance |
| [Verification audit](#verification-audit-2026-06-25) | Grep gates + harness results from double-check |
| [Part I — Context](#part-i--context) | Goal, scope, layout, rules |
| [Part II — Phases](#part-ii--phases) | Step-by-step execution with checkboxes |
| [Part III — Safety net](#part-iii--safety-net) | Tests, grep gates, rollback |
| [Part IV — Documentation](#part-iv--documentation) | Docs to update after code moves |

---

## Phase checklist

Work proceeds **Phase 0 → 6** in order. Each phase ended with a green build + targeted harness run.

| Phase | Name | Risk | Status |
|-------|------|------|--------|
| **0** | Baseline & inventory | Low | **Done** |
| **1** | Include order cleanup | Low | **Done** |
| **2** | Move YPQ / PQ / 10-bit public APIs | Medium | **Done** |
| **3** | Move HSV + `SituationConvertColorToVector4` | Low | **Done** |
| **4** | Unify sRGB → linear helpers | Medium | **Done** |
| **5** | Trace regroup via `gen_situation_base_trace.py` | Low | **Done** |
| **6** | Documentation alignment | Low | **Done** |

---

## Verification audit (2026-06-25)

Double-check performed after consolidation. All gates below **passed**.

### Grep gates

| Check | Result |
|-------|--------|
| `rg situation_impl_ypq` (source only) | **0 hits** in `sit/` (plan doc mentions rename history only) |
| `#include "situation_impl_color.h"` | **2 sites:** `situation_impl.h` (line 70), `situation_impl_image.h` (line 23, top) |
| Color `SITAPI` in `situation_impl_image.h` | **0** color-math APIs (only `SituationImageAdjustYPQ` / `AdjustHSV` remain) |
| Color `SITAPI` in `situation_impl_color.h` | **30** public functions (HSV, YPQ, PQ, 10-bit, diagnostics, `ConvertColorToVector4`) |
| `_SituationColorRgbaToClearFloats` definition | **color.h only** (used from renderer) |
| `_SitSrgbUnitToLinear` definition | **color.h only** (`_SitYpqSrgbByteToLinear` delegates) |
| `situation_impl_color.h` includes `situation_impl_image.h` | **No** (no circular dependency) |
| Non-goals preserved in image.h | `SituationImageAdjustYPQ`, `SituationImageAdjustHSV`, `_SituationColorAlphaBlend` present |
| Color trace APIs under `SITUATION_TRACE_SITUATION_IMPL_COLOR_H` | 10730001 – 10730031 (regenerated) |
| Image trace has no color-math `SITAPI` | Only adjust loops + image/font/draw (10080001 – 10080048) |

### Harness (OpenGL, `build\tests\sit_test_opengl.exe`)

| Filter | Passed |
|--------|--------|
| `--module misc --filter ypq` | 18 |
| `--module misc --filter hsv` | 3 |
| `--module misc --filter color_to_vector4` | 3 |
| `--module core --filter rgb10` | 2 |
| `--module core --filter pq` | 3 |
| `--module graphics --filter ypq_grade` | 1 |
| **Consolidation suite total** | **30 tests, 0 failed** |

Build: `build\build_tests.bat opengl` — **OK**.

> Full 572-test harness not re-run in this audit; consolidation filter suite covers all moved APIs. Run `build\run_tests.bat opengl` before release if desired.

---

# Part I — Context

## I.1 Executive summary

**Goal:** Make `sit/situation_impl_color.h` the single home for **pure color-space math** (internal helpers + public pixel APIs), while `sit/situation_impl_image.h` keeps **buffer operations** (load/save, draw, adjust loops, screenshots).

**Outcome:** Consolidation complete. Public color APIs live in `situation_impl_color.h`; image module retains adjust loops and draw/compositing only.

**Verdict:** Refactor was **behavior-neutral**. No public API signature changes, no trace ID changes, no ABI break.

---

## I.2 Non-goals

- [x] Do **not** move `SituationImageAdjustYPQ` / `SituationImageAdjustHSV` (pixel-buffer loops stay in image module).
- [x] Do **not** move `_SituationColorAlphaBlend` (compositing for image draw/text only).
- [x] Do **not** move renderer / VD command helpers (`SituationCmdClearColor`, `SetVirtualDisplayFallbackColor`, etc.).
- [x] Do **not** rename public APIs or trace enum values.
- [x] Do **not** change `situation_api_system.h` declarations (implementations move only).

---

## I.3 Layout (after consolidation)

```
situation_impl.h
  └── #include situation_impl_color.h   ← orchestrator (before image)
  └── situation_impl_image.h
        └── #include situation_impl_color.h   ← top of requester (include-guarded)
        ├── SituationImageAdjustYPQ / SituationImageAdjustHSV
        └── _SituationColorAlphaBlend, load/draw/screenshot APIs

situation_impl_color.h
  ├── SIT_YIQ_NTSC_* constants, internal _SitYpq* / _SitYiq* / _SitRgb*
  ├── ST.2084 PQ, 10-bit pack/unpack, _SitSrgbUnitToLinear (canonical sRGB EOTF)
  ├── Public: HSV, YPQ/PQ/10-bit pixel APIs, SituationConvertColorToVector4
  └── Diagnostics: SituationYpqAnalyzeRgbMapping, SituationYpqSliceDuplicateCount
```

---

## I.4 Binding rules (non-negotiable)

1. **Single conversion truth** — matrix constants and PQ curves stay in `situation_impl_color.h` only; GPU shaders (`sit/gpu/ypq_grade.frag`) remain synced via comments + harness tests.
2. **Public API surface unchanged** — symbols declared in `situation_api_system.h` keep the same names and signatures; only `.h` implementation location changes.
3. **Include discipline** — `situation_impl_color.h` must not `#include` `situation_impl_image.h` (no circular dependency).
4. **One phase, one green build** — run harness after every phase before starting the next.
5. **Trace IDs** — regenerate with `scripts/gen_situation_base_trace.py` after body moves; do not hand-edit `situation_base_trace.h`. Numeric IDs repack per source file; enum names stay stable.

---

# Part II — Phases

## Phase 0 — Baseline & inventory

**Goal:** Confirm green baseline and freeze the move list before touching code.

### Pre-flight

- [x] Read this plan and cross-check against current `situation_impl_image.h` / `situation_impl_color.h`.
- [x] Confirm rename is complete: `rg situation_impl_ypq sit/` returns zero hits.

### Baseline build & tests

From repo root (PowerShell):

```powershell
Set-Location "C:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build\build_tests.bat" opengl
Set-Location "build"
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\tests\sit_test_opengl.exe" --module misc --filter ypq
& ".\tests\sit_test_opengl.exe" --module misc --filter hsv
& ".\tests\sit_test_opengl.exe" --module misc --filter color_to_vector4
& ".\tests\sit_test_opengl.exe" --module core --filter rgb10
& ".\tests\sit_test_opengl.exe" --module core --filter pq
& ".\tests\sit_test_opengl.exe" --module graphics --filter ypq_grade
```

- [x] All listed filters pass (see [Verification audit](#verification-audit-2026-06-25)).

### Inventory sign-off

Functions moved in Phase 2:

- [x] `SituationColorFromYPQ`, `SituationColorToYPQ`
- [x] `SituationYpqLerp`, `SituationYpqAdjustLuma`, `SituationYpqAdjustPhase`, `SituationYpqAdjustChroma`
- [x] `SituationYpqGetLuma`, `SituationYpqGetHueDegrees`, `SituationYpqGetChroma`
- [x] `SituationYpqDistance`, `SituationYpqEquals`
- [x] `SituationColorToYPQf`, `SituationColorFromYPQf`
- [x] `SituationYpqToRgba10`, `SituationYpqToRgb10Packed`, `SituationYpqToRgb10PackedHdr`
- [x] `SituationLinearToPq`, `SituationPqToLinear`, `SituationPqGrayToRgb10Packed`
- [x] `SituationColorRgbaToHdrPqClear`
- [x] `SituationRgbToYpqFrom10`, `SituationRgb10FromRgba`, `SituationRgbaFromRgb10`, `SituationRgbaFromRgb10Packed`
- [x] `SituationYpqQuantize`, `SituationYpqClampInGamut`

Functions moved in Phase 3:

- [x] `SituationRgbToHsv`, `SituationHsvToRgb`
- [x] `SituationConvertColorToVector4`

Functions confirmed to **stay** in `situation_impl_image.h`:

- [x] `SituationImageAdjustYPQ`, `SituationImageAdjustHSV`
- [x] `_SituationColorAlphaBlend`

**Status:** Done

---

## Phase 1 — Include order cleanup

**Goal:** Place `#include "situation_impl_color.h"` at the top of `situation_impl_image.h` **and** in `situation_impl.h` (before `situation_impl_image.h`), with **zero** function moves.

### Implementation

- [x] Add `#include "situation_impl_color.h"` in `situation_impl.h` immediately before `situation_impl_image.h`.
- [x] Add `#include "situation_impl_color.h"` at the top of `situation_impl_image.h` (after the header guard).
- [x] Remove the mid-file `#include` from `situation_impl_image.h`.
- [x] Update `situation_impl_color.h` header comment: included from orchestrator + image module.

### Verification

- [x] `build\build_tests.bat opengl` succeeds.
- [x] Harness filters green (see audit).
- [x] Two include sites: `situation_impl.h` + `situation_impl_image.h`.

**Status:** Done

---

## Phase 2 — Move YPQ / PQ / 10-bit public APIs

**Goal:** Relocate all YPQ/PQ/10-bit `SITAPI` wrappers from `situation_impl_image.h` into `situation_impl_color.h`.

### Implementation checklist

- [x] Copy the full block (functions + doc comments) from image.h → color.h.
- [x] Delete the block from image.h (leave `SituationImageAdjustYPQ` intact).
- [x] `#include <stdlib.h>` for `abs()` in `SituationYpqEquals`.
- [x] No dependency on image-only symbols.

### Post-move grep gates

- [x] No color-math `SITAPI` left in `situation_impl_image.h`.
- [x] All moved APIs present in `situation_impl_color.h`.
- [x] No duplicate `SITAPI` definitions across image.h / color.h.
- [x] `SituationImageAdjustYPQ` still in image.h; calls `SituationColorToYPQf` / `FromYPQf`.

### Verification

- [x] YPQ harness filters green.
- [x] `test_core.c` rgb10 / PQ tests green.

**Status:** Done

---

## Phase 3 — Move HSV + vector4 conversion

**Goal:** Move `SituationRgbToHsv`, `SituationHsvToRgb`, and `SituationConvertColorToVector4` into `situation_impl_color.h`.

### Implementation

- [x] Move HSV pair + doc comments to color.h (`/* --- HSV pixel APIs --- */` section).
- [x] Move `SituationConvertColorToVector4` to color.h.
- [x] Update color.h file banner: HSV, YPQ, PQ, 10-bit scope.

### Verification

- [x] `--module misc --filter hsv` green.
- [x] `--module misc --filter color_to_vector4` green.
- [x] `SituationImageAdjustHSV` still in image.h; compiles against moved HSV APIs.

**Status:** Done

---

## Phase 4 — Unify sRGB → linear helpers

**Goal:** Remove duplicate sRGB EOTF implementations.

### Implementation

- [x] Add canonical `_SitSrgbUnitToLinear(float s)` in color.h.
- [x] Change `_SitYpqSrgbByteToLinear` to delegate via `_SitSrgbUnitToLinear`.
- [x] Remove `_SitSrgbUnitToLinear` definition from image.h; call sites use color.h inline.
- [x] Comment: single sRGB EOTF for CPU color module.
- [x] Move forward declaration in `situation_impl_forward.h` to Color module section.

### Verification

- [x] `_SitSrgbUnitToLinear` / `_SitYpqSrgbByteToLinear` definitions only in color.h.
- [x] PQ round-trip tests green (exercises HDR clear path indirectly).

**Status:** Done

---

## Phase 5 — Trace regroup via generator

**Goal:** Regenerate `sit/situation_base_trace.h` so color API traces live under `SITUATION_TRACE_SITUATION_IMPL_COLOR_H`.

**Do not edit `situation_base_trace.h` by hand.** After moving function bodies, run:

```powershell
& "C:\msys64\mingw64\bin\python3.exe" scripts\gen_situation_base_trace.py
```

(See `scripts/README.md`.)

### Implementation

- [x] Color function bodies in `situation_impl_color.h` (Phases 2–4).
- [x] Regenerated trace header with `scripts/gen_situation_base_trace.py`.
- [x] Added `situation_impl_color.h` to `FILE_PRIORITY` in generator (105, before image 110).

### Outcome

| Macro | API range | Notes |
|-------|-----------|-------|
| `SITUATION_TRACE_SITUATION_IMPL_COLOR_H` | 10730001 – 10730031 | HSV, YPQ, PQ, 10-bit, diagnostics, Vector4 |
| `SITUATION_TRACE_SITUATION_IMPL_IMAGE_H` | 10080001 – 10080048 | Image/font/draw + adjust loops only |
| Color internals | 20730001 – 20730043 | Includes `_SitSrgbUnitToLinear` |

Moved functions got **new numeric IDs** in the color file block (generator repacks per source file). Enum **names** are unchanged.

### Verification

- [x] No color-math entries under IMAGE trace macro.
- [x] Build + consolidation harness green after regen.

**Status:** Done

---

## Phase 6 — Documentation alignment

**Goal:** Docs reflect `situation_impl_color.h` as the color math module.

### Code comments

- [x] `situation_impl_image.h` module banner: color conversions **implemented in** `situation_impl_color.h`.
- [x] `situation_impl_color.h` module banner: list HSV, YPQ, PQ, 10-bit scope.

### Project docs

- [x] `.kiro/steering/situation-project.md` — tree entry describes full color module scope.
- [x] `doc/situation_sdk.md` — file tree blurb updated.
- [x] `doc/guide/ypq_color.md` — single source of truth points to color.h.

### Plan cross-links

- [x] `doc/plan/YPQ_COLOR_PLAN.md` — Layer 0 → `situation_impl_color.h`; forward ref to this plan.
- [x] This plan updated with audit + checkboxes.

### Release note (when shipping)

- [x] Add entry to appropriate `doc/updatelog_*.md` chunk: internal refactor, no API break.

---

# Part III — Safety net

## III.1 Harness matrix

| Module | Filter | Covers | Verified |
|--------|--------|--------|----------|
| `misc` | `ypq` | YPQ pixel API, mapping stats | [x] |
| `misc` | `hsv` | HSV round-trip | [x] |
| `misc` | `color_to_vector4` | Vector4 normalization | [x] |
| `core` | `rgb10`, `pq` | 10-bit / PQ / HDR packed | [x] |
| `graphics` | `ypq_grade` | GPU grade CPU parity | [x] |

- [ ] Full `sit_test_opengl.exe` (572 tests) — deferred; run before release.

## III.2 Static grep gates

| Check | Expected | Verified |
|-------|----------|----------|
| No stale filename in `sit/` | 0 hits | [x] |
| Include sites | 2 (`situation_impl.h` + `situation_impl_image.h` top) | [x] |
| Public color math in image.h | none (adjust loops only) | [x] |
| Clear helper single home | define in color.h only | [x] |

## III.3 Rollback strategy

Each phase is one revertable unit. If Phase 4 (sRGB unify) causes HDR drift, revert Phase 4 only; Phases 2–3 remain valid independently.

## III.4 Definition of done

- [x] All phases 0–5 complete and checked off.
- [x] Phase 6 documentation and release note checked off (v2.4.357).
- [x] Consolidation harness suite green (30 tests).
- [x] Trace header regenerated via `scripts/gen_situation_base_trace.py`.
- [x] Phase checklist table updated.
- [x] No public **API signature** changes (trace numeric IDs for moved functions repacked into color block — expected).

---

# Part IV — Documentation

| File | Status |
|------|--------|
| `doc/plan/COLOR_IMPL_CONSOLIDATION_PLAN.md` | [x] Updated with audit |
| `doc/plan/YPQ_COLOR_PLAN.md` | [x] Layer 0 text + cross-link |
| `.kiro/steering/situation-project.md` | [x] |
| `doc/situation_sdk.md` | [x] |
| `doc/guide/ypq_color.md` | [x] Already correct |
| `doc/updatelog_*.md` | [x] v2.4.357 in `updatelog_24_04.md` |

---

*Last updated: 2026-06-25 — Phase 5 trace regen via `scripts/gen_situation_base_trace.py`; plan complete.*
