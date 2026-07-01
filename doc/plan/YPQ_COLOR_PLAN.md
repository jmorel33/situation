# YPQ Color (Core API Expansion)

**Document:** `doc/plan/YPQ_COLOR_PLAN.md`  
**Overall status:** Phases 0–4 **done** · Phases 5, 6 **open**

---

## Table of contents

| Section | Contents |
|---------|----------|
| [Phase checklist](#phase-checklist) | Status of all phases at a glance |
| [Part I — Context](#part-i--context) | Why, rules, layers, types, CPU vs GPU |
| [Part II — Phases](#part-ii--phases) | **Execution detail per phase** (goal → tasks → tests → docs) |
| [Part III — Cross-cutting](#part-iii--cross-cutting) | Testing matrix, doc deliverables, non-goals, success |

---

## Phase checklist

Work proceeds **Phase 0 → 6** in order. Each phase below uses the same subsection layout: **Goal · Scope · Files · Implementation · Tests · Documentation · Status**.

| Phase | Name | Layer | Status |
|-------|------|-------|--------|
| **0** | YIQ core groundwork | 0 | Done |
| **1** | Core pixel API (HSV parity) | 0 | Done |
| **2** | Image CPU adjust + float `ColorYPQf` | 1 | Done |
| **3** | Quality & diagnostics | 2 | **Done** |
| **4** | GPU framebuffer grade pass | 3 | Done |
| **5** | RGL integration | 4 | Open |
| **6** | Authoring / debug overlay (optional) | 5 | Open |

> **Note on old naming:** Earlier drafts called Phase 3 **“Phase 2b”** and Phase 4 **“Phase 3.”** This document uses **Phase 3 = diagnostics** and **Phase 4 = GPU** so phase numbers always increase and match delivery order.

---

# Part I — Context

## I.1 Executive summary

**Goal:** Make YPQ a first-class **authoring and manipulation** color space in Situation — parallel to the existing HSV path — without replacing RGB as the **GPU display and rendering** encoding.

**Verdict:** Aligns with library philosophy. YPQ pixel math lives in **`situation_impl_color.h`** (Layer 0 CPU, same module as HSV). **`situation_impl_image.h`** owns Layer 1 CPU adjust loops (`SituationImageAdjustYPQ`, `SituationImageAdjustHSV`). GPU stays linear/sRGB RGB; YPQ is applied at **boundaries** (grade, import, effects, debug) and via **`SituationCmdDrawTextureYpqGrade`** — a single textured draw that grades every sampled pixel on the GPU (typical use: blit an offscreen color attachment through the grade shader into the current framebuffer).

**Performance (v2.4.193+):** Prefer the GPU path for full-frame or texture-sized grading in real time. Keep **`SituationImageAdjustYPQ`** for offline tools, capture pipelines, and small CPU buffers. The internal YIQ core (`sit/situation_impl_color.h`) uses **FMA** and precomputed inverse scales for CPU hot paths; release builds may pass **`SIT_OPTIMIZE_CFLAGS`** via `build_situation.bat`.

**Relationship to RGL:** `misc/rgl.h` already prototypes many YPQ helpers (lerp, TV look, palettes). This plan **lifts the generic, engine-neutral subset into core** (Phases 0–4) and leaves CRT-specific composites in RGL until **Phase 5**.

**Remaining work:** Phase 3 (public mapping-stats API, gamut/dither polish), Phase 5 (RGL wrappers), Phase 6 (optional tooling UI).

> **2026-06-25:** Pixel math consolidated into `sit/situation_impl_color.h` — see `doc/plan/COLOR_IMPL_CONSOLIDATION_PLAN.md`.

---

## I.2 Binding with Situation philosophy

| Pillar | How YPQ color API fits |
|--------|------------------------|
| **Awareness** | Phase 3 diagnostics expose mapping quality: duplicate RGB counts, gamut clamp flags, round-trip loss. Developers *see* the 66% collision problem instead of guessing. |
| **Control** | Explicit knobs: luma / phase / chroma adjust, in-gamut clamp, float edit → quantize at export. No hidden color magic. |
| **Timing** | Per-pixel CPU ops are synchronous (like `SituationImageAdjustHSV`). GPU grade pass records one draw into the command buffer; runs in draw phase only. |

**Architectural rules (non-negotiable):**

1. **RGB is the highway** — textures, framebuffers, shaders, swapchain remain RGB(A) / linear float. YPQ is not a new `SituationTextureFormat`.
2. **HSV parity** — wherever HSV has a function, YPQ gets an equivalent when semantically valid (`AdjustImage`, lerp, accessors).
3. **Single conversion truth** — `SituationColorToYPQ` / `SituationColorFromYPQ` in `situation_impl_color.h` stay authoritative; all new ops compose them or share their YIQ matrix constants (no drift).
4. **Explicit lifecycle** — no hidden allocations in hot loops; image ops mutate or write to caller buffers; optional out-params for analysis structs.
5. **Context-free CPU** — Phases 0–2 require no `SituationInit`. Phase 4 (GPU grade) follows existing graphics module rules.
6. **Errors where it matters** — Phase 3 analysis APIs return `SituationError`; simple pixel ops stay `void`/`ColorYPQA` like HSV.

---

## I.3 Layer model

Layers describe **architecture**; phases describe **delivery order**. Phases map to layers as in the [phase checklist](#phase-checklist).

```
Layer 0 — Pixel math (situation_api_system.h + situation_impl_color.h)
  ToYPQ / FromYPQ [exists]
  YpqLerp, Adjust*, accessors, distance, gamut clamp
  Float edit type (ColorYPQf); quantize helpers

Layer 1 — Image CPU (situation_impl_image.h)
  SituationImageAdjustYPQ
  SituationImageConvert* (RGB↔YPQ plane export for tools)
  Blend / mix helpers

Layer 2 — Quality & diagnostics (situation_impl_image.h + harness)
  Gamut-aware Q limit per P
  Duplicate / hole statistics API
  Optional dither on FromYPQ

Layer 3 — GPU (situation_impl_decl.h + situation_impl_renderer.h)
  SIT_YPQ_GRADE_FRAGMENT_SHADER + SituationCmdDrawTextureYpqGrade
  Textured quad: sample RGB → YPQ adjust → clamp RGB (phase/chroma/luma/mix)
  Golden parity: CPU SituationImageAdjustYPQ vs GPU readback (test_graphics)

Layer 4 — Retro / game chrome (misc/rgl.h)
  Scanline, ghost, TV noise, channel colors
  Thin wrappers calling Layer 0 where possible

Layer 5 — Authoring (future, optional)
  Debug picker overlay, Y×P slice viewer (reuse harness sweep code)
```

**Build order:** Layer 0 → 1 → 2 → 3 (Phases 0–4). Layers 4–5 (Phases 5–6) reuse lower layers; no inversion.

**Layer completion:**

- [x] Layer 0 partial — `SituationColorToYPQ` / `SituationColorFromYPQ` (pre-plan)
- [x] Layer 0 groundwork — `situation_impl_color.h` internal YIQ core (Phase 0)
- [x] Layer 0 complete — pixel API (Phase 1); float helpers (Phase 2)
- [x] Layer 1 complete — `SituationImageAdjustYPQ` (Phase 2)
- [x] Layer 2 complete — gamut clamp + diagnostics API (Phase 3)
- [x] Layer 3 complete — GPU grade pass (Phase 4)
- [ ] Layer 4 complete — RGL wrappers (Phase 5)
- [ ] Layer 5 complete — debug overlay / palettes (Phase 6, optional)

---

## I.4 Types and encoding conventions

### I.4.1 Existing types (keep)

```c
typedef struct ColorYPQA { unsigned char y, p, q, a; } ColorYPQA;  // storage / 8-bit
typedef struct ColorHSV   { float h, s, v; } ColorHSV;             // edit / float
```

### I.4.2 Float edit type (Phase 2)

```c
typedef struct ColorYPQf {
    float y;   /* 0..1 luma */
    float p;   /* 0..1 phase → maps to 0..2π internally */
    float q;   /* 0..1 chroma amplitude */
    float a;   /* 0..1 alpha */
} ColorYPQf;
```

**Rationale:** HSV edits in float; YPQ should too. Quantize to `ColorYPQA` only at storage/display boundary — reduces banding and makes gamut logic easier.

**Type tasks (Phase 2):**

- [x] Add `ColorYPQf` to `sit/situation_api.h`
- [x] Document alongside `ColorYPQA` / `ColorHSV` in `doc/situation_api.md`

### I.4.3 `SituationImage.color_encoding`

**Do not overload** `SITUATION_COLOR_LINEAR / SRGB` for YPQ (those describe gamma/legalization for GPU).

If needed later, add a **separate metadata flag** (e.g. `SituationImageLayout` or `channels_semantic`) — **deferred** to avoid breaking texture upload paths. Until then, YPQ images are explicit sidecar buffers or documented channel packing (R=Y, G=P, B=Q) in tool-only code.

- [ ] **Deferred:** separate YPQ channel-semantics flag (only if tool export needs it)

---

## I.5 CPU vs GPU grading (Phase 4 — implemented)

| Path | API | When |
|------|-----|------|
| **CPU** | `SituationImageAdjustYPQ` | Authoring, tests, saving PNGs, buffers not yet on GPU |
| **GPU** | `SituationCmdDrawTextureYpqGrade` | Real-time: draw a `SituationTexture` (e.g. color attachment from previous pass) into the active render pass with YPQ knobs |

**Typical framebuffer workflow:** render scene to offscreen color texture → begin display (or post) pass → `SituationCmdDrawTextureYpqGrade(cmd, scene_tex, …)` fullscreen → present. No YPQ texture format; RGB in, RGB out, grade in the fragment shader.

**Implementation anchors:**

- Shader source: `SIT_YPQ_GRADE_FRAGMENT_SHADER` in `sit/situation_impl_decl.h` (matrix constants must match `sit/situation_impl_color.h`).
- Init: `_SituationInitYpqGradeRenderer()` from renderer startup (`sit/situation_impl_renderer.h`).
- Public draw: `SituationCmdDrawTextureYpqGrade` in `sit/situation_api.h` — same `phase_shift_deg`, `chroma_factor`, `luma_factor`, `mix` as `SituationImageAdjustYPQ`.
- Parity test: `ypq_grade_pass_cpu_parity` in `tests/harness/test_graphics.c` (±12 per 8-bit channel).

Early harness-only grade shaders were superseded by the in-library shader; do not add duplicate GLSL under `tests/harness/shaders/`.

---

# Part II — Phases

Each phase follows this structure:

1. **Goal** — what we are trying to achieve  
2. **Scope** — in / out  
3. **Target files**  
4. **Implementation** — checklist  
5. **Tests**  
6. **Documentation**  
7. **Status**

---

## Phase 0 — YIQ core groundwork

**Status:** Done  
**Layer:** 0  
**Depends on:** —  
**Blocks:** Phase 1

### Goal

Establish a single internal conversion core before adding public pixel helpers. No intended behavior change — refactor only.

### Scope

**In:** Shared NTSC matrices, byte/float YPQ↔YIQ↔RGB paths, delegate existing `ToYPQ`/`FromYPQ`.  
**Out:** Public `SituationYpq*` pixel API (Phase 1); `ColorYPQf` (Phase 2).

### Target files

- `sit/situation_impl_color.h` (new)
- `sit/situation_impl_image.h`
- `tests/harness/test_misc.c`

### Implementation

- [x] Add `sit/situation_impl_color.h` — `SIT_YIQ_NTSC` constants, `SitYpqYiqLinear`, byte/float helpers
- [x] `_SitYiqFromRgbLinear` / `_SitRgbLinearFromYiq(Clamped)` — shared NTSC matrices
- [x] `_SitYiqFromYpqBytes` / `_SitYpqBytesFromYiqLinear` / `_SitRgbFromYpqBytes` / `_SitYpqBytesFromRgb`
- [x] Refactor `SituationColorToYPQ` / `SituationColorFromYPQ` to delegate to internal core
- [x] Groundwork tests: Q=0 phase invariant, luma axis, golden vectors
- [x] Brief `ColorYPQA` comment in `situation_api.h`

### Tests

- [x] Q=0 phase invariant, luma axis, golden vectors (`test_misc.c`)

### Documentation

- [x] `ColorYPQA` comment in `situation_api.h`

---

## Phase 1 — Core pixel API (HSV parity)

**Status:** Done  
**Layer:** 0  
**Depends on:** Phase 0  
**Blocks:** Phase 2

### Goal

Per-color YPQ operations with the same role as HSV pixel helpers. Port generic logic from `misc/rgl.h` using shared internal constants — **do not** `#include rgl.h`.

### Scope

**In:** Lerp, adjust luma/phase/chroma, accessors, distance, equals.  
**Out:** Whole-image adjust (Phase 2); diagnostics (Phase 3).

### Target files

- `sit/situation_api.h`
- `sit/situation_impl_image.h`
- `tests/harness/test_misc.c`

### Implementation — API (`situation_api.h`)

- [x] `SituationYpqLerp(a, b, t)` — circular **P** interpolation (shortest arc)
- [x] `SituationYpqAdjustLuma(c, factor)` — scale Y, preserve P/Q
- [x] `SituationYpqAdjustPhase(c, shift)` — add to P mod 256 (byte steps)
- [x] `SituationYpqAdjustChroma(c, factor)` — scale Q
- [x] `SituationYpqGetLuma(c)` — normalized 0..1
- [x] `SituationYpqGetHueDegrees(c)` — degrees 0..360
- [x] `SituationYpqGetChroma(c)` — normalized 0..1
- [x] `SituationYpqDistance(a, b)` — weighted Y + circular P + Q
- [x] `SituationYpqEquals(a, b, tol)` — per-channel tolerance

### Implementation — core (`situation_impl_image.h`)

- [x] Extract shared YIQ constants (single source with `ToYPQ` / `FromYPQ`) — done in Phase 0
- [x] Implement all Phase 1 functions (no `#include rgl.h`)
- [ ] Add trace IDs in `situation_base_trace.h` if other color APIs are traced

### Tests (`tests/harness/test_misc.c`)

- [x] `ypq_lerp_wrap` — P=250 → P=10 shortest arc
- [x] `ypq_adjust_luma` — Y scales, P/Q unchanged
- [x] `ypq_adjust_phase` — P wraps mod 256
- [x] `ypq_adjust_chroma` — Q scales; Q=0 phase independence
- [x] `ypq_distance_equals` — sanity vs known pairs
- [x] Register tests in `misc_tests[]`
- [x] `ypq_to_rgb_q_sweep_4s` — deterministic 256 Q steps over ~4s (visual; stats → Phase 3 API)

### Documentation

- [x] Add YPQ subsection next to HSV in `doc/situation_api.md`
- [ ] Run `python scripts/generate_situation_api_docs.py`
- [ ] Run `python scripts/verify_doc_links.py`
- [ ] `doc/UPDATELOG.md` entry for Phase 1

---

## Phase 2 — Image CPU adjust and float path

**Status:** Done  
**Layer:** 1  
**Depends on:** Phase 1  
**Blocks:** Phase 3 (polish), Phase 4 (GPU uses same grade semantics)

### Goal

Whole-image CPU grading in YPQ and a float edit path (`ColorYPQf`) before quantizing to bytes — HSV parity for `SituationImageAdjustHSV`.

### Scope

**In:** `SituationImageAdjustYPQ`, float convert/quantize, basic in-gamut clamp.  
**Out:** Dither (Phase 3); public RGB mapping stats (Phase 3).

### Target files

- `sit/situation_api.h`, `sit/situation_impl_image.h`
- `tests/harness/test_misc.c`
- `doc/situation_api.md`, `doc/situation_sdk.md`

### Implementation

- [x] `SituationImageAdjustYPQ(image, phase_shift_deg, chroma_factor, luma_factor, mix)` — mirror `SituationImageAdjustHSV`
- [x] `SituationColorToYPQf(ColorRGBA)` / `SituationColorFromYPQf(ColorYPQf)`
- [x] `SituationYpqQuantize(ColorYPQf)` → `ColorYPQA`
- [x] `SituationYpqClampInGamut(ColorYPQf)` — binary search on Q to avoid RGB clip
- [ ] `SituationYpqFromYPQfDithered(...)` — optional ordered dither *(Phase 3)*

### Tests

- [x] `image_adjust_ypq` — solid fill, boost chroma, verify not gray
- [x] `ypq_float_roundtrip` — RGB → YPQf → RGB within tolerance
- [x] `ypq_quantize` — float → byte; clamp reduces hot chroma

### Documentation

- [x] Document `SituationImageAdjustYPQ` in `doc/situation_api.md`
- [x] SDK blurb: when to use YPQ vs HSV vs RGB (`doc/situation_sdk.md`)
- [x] `doc/UPDATELOG.md` entry for Phase 2 (v2.4.192)

---

## Phase 3 — Quality and diagnostics

**Status:** Done (v2.4.265)  
**Layer:** 2  
**Depends on:** Phase 2  
**Blocks:** Nothing critical (Phase 4 GPU already shipped in parallel)

### Goal

**Awareness and polish** on top of Phase 2 editing:

1. **Public diagnostics** — how many YPQ triples collide to the same RGB, RGB “holes,” per-axis slice stats.  
2. **Gamut clamp completion** — per-P max Q so extreme phase/chroma does not collapse to identical RGB after clip.  
3. **Optional dither** — export-quality `FromYPQf` path.

Phase 2 lets you *change* colors; Phase 3 lets you *measure* 8-bit YPQ↔RGB quality. Work already exists in harness (`ypq_to_rgb_q_sweep_4s`, duplicate scan); this phase **promotes it to library API**.

### Scope

**In:** Analyze API, slice duplicate count, clamp/dither completion, CI skip env documented.  
**Out:** GPU paths; RGL (Phase 5).

**Philosophy:** Pure **Awareness** — opt-in, potentially slow (~20s full scan); no side effects. Harness supports `SIT_SKIP_YPQ_SWEEP`, `SIT_SKIP_YPQ_RGB_STATS`.

### Target files

- **`sit/situation_impl_color.h`** — all new function implementations (3.2, 3.3). The diagnostics functions use `_SitRgbFromYpqBytes` directly and have no image dependency, so they belong here alongside the rest of the YPQ math core.
- `sit/situation_api.h` — struct + two SITAPI declarations (3.1)
- `sit/situation_impl_image.h` — no changes needed for 3.2/3.3; `SituationImageAdjustYPQ` and all existing pixel API remains here
- `tests/harness/test_misc.c` — delete ~180 lines of private helpers (3.4), refactor two tests (3.5), add two tests (3.6)

### Implementation — planned API

```c
typedef struct SituationYpqRgbMappingStats {
    int64_t ypq_mappings;        /* 256³ */
    int64_t unique_rgb;          /* distinct 8-bit RGB outputs */
    int64_t duplicate_mappings;  /* ypq_mappings - unique_rgb */
    int64_t rgb_holes;           /* 2²⁴ - unique_rgb */
    int     worst_q_slice_dup;   /* per fixed-Q slice, max duplicates */
    int     worst_q_slice_at;
    /* optional: per-Y / per-P totals for tools */
} SituationYpqRgbMappingStats;

SITAPI SituationError SituationYpqAnalyzeRgbMapping(SituationYpqRgbMappingStats* out);
SITAPI SituationError SituationYpqSliceDuplicateCount(char fixed_axis, int fixed_value, int* out_dup);
```

**Context — what already exists in `tests/harness/test_misc.c`:**
The full algorithm is proven and working. This phase is promotion, not invention.

| Harness helper | Lines | Becomes |
|---|---|---|
| `misc_ypq_build_sweep_and_cube()` | ~192 | Core of `SituationYpqAnalyzeRgbMapping` |
| `misc_ypq_radix_sort_u32()` + `misc_ypq_count_duplicates_sorted()` | ~382–424 | Internalized into `SituationYpqSliceDuplicateCount` |
| `MiscYpqRegistryDupReport` + `misc_ypq_scan_registry_dup_report_from_cube()` | ~425–480 | Drives `worst_axis_dup` / `worst_axis_at` fields |
| `misc_ypq_init_lut()`, `misc_ypq_fast_rgb_key()`, NTSC constants | ~20–127 | **Deleted** — replaced by `SituationColorFromYPQ` calls |

### 3.1 — Add `SituationYpqRgbMappingStats` struct to `situation_api.h`

- [x] Add immediately after the `ColorYPQf` block:
  ```c
  typedef struct SituationYpqRgbMappingStats {
      int64_t ypq_mappings;    /* always 256³ = 16 777 216 */
      int64_t unique_rgb;      /* distinct 8-bit RGB triples reachable from YPQ */
      int64_t duplicate_mappings; /* ypq_mappings - unique_rgb */
      int64_t rgb_holes;       /* 2²⁴ - unique_rgb — RGB triples never produced */
      int     worst_axis_dup;  /* max duplicates in any single fixed-Q slice */
      int     worst_axis_at;   /* Q value where worst_axis_dup occurs */
  } SituationYpqRgbMappingStats;
  ```
- [x] Add two SITAPI declarations in the YPQ pixel API block:
  ```c
  SITAPI SituationError SituationYpqAnalyzeRgbMapping(SituationYpqRgbMappingStats* out);
  SITAPI SituationError SituationYpqSliceDuplicateCount(char axis, int value, int* out_dup);
  ```

### 3.2 — Implement `SituationYpqAnalyzeRgbMapping` in `situation_impl_image.h`

Source: `misc_ypq_build_sweep_and_cube` (test_misc.c ~192) rewritten to use `SituationColorFromYPQ`:

- [x] Guard: `if (!out) return SITUATION_ERROR_INVALID_PARAM`
- [x] Alloc `uint8_t* hit = SIT_CALLOC(1 << 24, 1)` — 16 MB bitmap; on fail return `SITUATION_ERROR_MEMORY_ALLOCATION`
- [x] Iterate all 16 777 216 (Y, P, Q) triples; for each call `SituationColorFromYPQ`, compute `key = (r<<16)|(g<<8)|b`, set `hit[key]=1`, increment `unique_rgb` on first set
- [x] Populate `out->ypq_mappings = 256*256*256`, `unique_rgb`, `duplicate_mappings`, `rgb_holes`
- [x] For `worst_axis_dup` / `worst_axis_at`: iterate each of the 256 fixed-Q slices (65 536 entries); use a per-slice 16 MB hit bitmap (reuse the outer one, reset per slice) to count duplicates; record max
- [x] `SIT_FREE(hit)`; return `SITUATION_SUCCESS`

### 3.3 — Implement `SituationYpqSliceDuplicateCount` in `situation_impl_image.h`

Source: `misc_ypq_scan_registry_dup_report_from_cube` for a single slice (test_misc.c ~435):

- [x] Validate `axis ∈ {'Y','P','Q'}`, `value ∈ [0,255]`, `out_dup != NULL` → `SITUATION_ERROR_INVALID_PARAM`
- [x] Alloc two `uint32_t[65536]` arrays (keys + temp) via `SIT_MALLOC`; on fail return `SITUATION_ERROR_MEMORY_ALLOCATION`
- [x] Fill 65 536 RGB keys for the given axis/value slice using `SituationColorFromYPQ`
- [x] 4-pass byte radix sort (identical to `misc_ypq_radix_sort_u32`)
- [x] Count adjacent duplicates → `*out_dup`
- [x] `SIT_FREE` both arrays; return `SITUATION_SUCCESS`

### 3.4 — Delete duplicate math from `test_misc.c`

Once 3.1–3.3 are in place, the harness's private YPQ math engine is dead weight:

- [x] Delete lines ~39–50: `MISC_YIQ_MAX_I/Q`, `MISC_YIQ_RI/GI/BI/RQ/GQ/BQ`, `MISC_INV255` constants
- [x] Delete lines ~51–68: `misc_ypq_y_lin[]`, `misc_ypq_sin_p[]`, `misc_ypq_cos_p[]`, `misc_ypq_init_lut()`
- [x] Delete lines ~70–127: `misc_ypq_unit_to_byte()`, `misc_ypq_fast_rgb_key()`, `misc_ypq_rgb_key_from_bytes()`, `misc_ypq_fast_from_bytes()`
- [x] Delete lines ~129–190: `misc_ypq_fill_y_p_plane_bytes()`, `misc_ypq_fill_ypq_y_p_plane()`
- [x] Delete lines ~192–207: `misc_ypq_build_sweep_and_cube()`
- [x] Delete lines ~382–560: `misc_ypq_radix_sort_u32()`, `misc_ypq_count_duplicates_sorted()`, `MiscYpqRegistryDupReport`, `misc_ypq_scan_registry_dup_report_from_cube()`, `misc_ypq_report_registry_from_cube()`, `misc_ypq_report_rgb_duplicate_stats()`
- [x] Fix `test_ypq_to_rgb_y_p_plane` (uses `misc_ypq_fill_ypq_y_p_plane`) — replace plane-fill loop with direct `SituationColorFromYPQ` calls for the golden-vector checks
- [x] Verify `test_ypq_to_rgb_y_p_plane` consistency checks (the `lib` vs `fast` comparison, lines ~1355–1370) still pass after removing the fast path — simplify to just call `SituationColorFromYPQ` twice

### 3.5 — Refactor `test_ypq_to_rgb_q_sweep_4s`

- [x] Remove `sweep_cache` / `cube_keys` allocation and `misc_ypq_build_sweep_and_cube` call
- [x] The plane-sweep loop (visual frames, timing) stays exactly as-is; generate each plane frame via `SituationColorFromYPQ` inline or a simple helper that replaces `misc_ypq_fill_y_p_plane_bytes`
- [x] Replace the `misc_ypq_report_rgb_duplicate_stats(cube_keys, unique_rgb)` call at the end with: `SituationYpqRgbMappingStats stats; SituationYpqAnalyzeRgbMapping(&stats);` + one `fprintf` summary line, guarded by `SIT_SKIP_YPQ_RGB_STATS`

### 3.6 — Add new harness tests

- [x] **`ypq_analyze_rgb_mapping`** in `test_misc.c`:
  - Skip if `SIT_SKIP_YPQ_RGB_STATS` set
  - Call `SituationYpqAnalyzeRgbMapping(&stats)`; assert `SITUATION_SUCCESS`
  - Assert `stats.unique_rgb` in `[5 000 000, 7 000 000]` (measured: 5 636 038)
  - Assert `stats.duplicate_mappings == 256*256*256 - stats.unique_rgb`
  - Assert `stats.rgb_holes == (1<<24) - stats.unique_rgb`
  - Assert `stats.worst_axis_dup > 0`
- [x] **`ypq_slice_dup_q0`** in `test_misc.c`:
  - Call `SituationYpqSliceDuplicateCount('Q', 0, &dup)`; assert `SITUATION_SUCCESS`
  - Assert `dup >= 65000` (Q=0 → all map to gray → measured: 65 280 duplicates)
- [x] Register both in `misc_tests[]`

### 3.7 — Gamut clamp and optional dither

- [ ] Verify `SituationYpqClampInGamut` handles the edge case where even Q=0 clips (pure luma out-of-gamut) — should return Q=0 as the safe floor, not loop infinitely
- [ ] (Optional) Implement `SituationYpqFromYPQfDithered` — 4×4 ordered Bayer dither on Q before quantize; declare in `situation_api.h` alongside `SituationYpqQuantize`; add `ypq_dither_reduces_banding` test

### 3.8 — Documentation

- [x] `SituationYpqRgbMappingStats`, `SituationYpqAnalyzeRgbMapping`, `SituationYpqSliceDuplicateCount` declared with doc-comments in `sit/situation_api.h`
- [ ] Add entries to `doc/situation_api.md` under the YPQ section
- [x] Document `SIT_SKIP_YPQ_RGB_STATS` and `SIT_SKIP_YPQ_SWEEP` env skip flags (in code comments and UPDATELOG)
- [x] `doc/UPDATELOG.md` entry for Phase 3 (v2.4.265)

### Phase 3 gate

- [x] `sit_test_opengl.exe --module misc` — all misc tests pass including new ones (40/40)
- [x] `ypq_analyze_rgb_mapping` passes without skip env (unique_rgb=5 636 038 in ~1.79 s)
- [x] `ypq_slice_dup_q0` passes (measured: 65 280 duplicates)
- [x] `test_ypq_to_rgb_q_sweep_4s` still runs cleanly (sweep loop intact)
- [x] `grep "MISC_YIQ_" tests/harness/test_misc.c` — zero hits
- [x] `grep "misc_ypq_fast_rgb_key\|misc_ypq_build_sweep" tests/harness/test_misc.c` — zero hits
- [x] Version bump (v2.4.265) + UPDATELOG entry

---

## Phase 4 — GPU framebuffer grade pass

**Status:** Done (v2.4.193)  
**Layer:** 3  
**Depends on:** Phase 2 (matching grade semantics)  
**Blocks:** —

### Goal

One textured draw pass: sample RGB from a texture (including FBO color attachment), apply the same YPQ grade as `SituationImageAdjustYPQ`, write clamped RGB. Real-time full-frame grading without CPU readback.

### Scope

**In:** Library shader, GL + Vulkan pipelines, `SituationCmdDrawTextureYpqGrade`, CPU/GPU parity test, CPU core perf (FMA).  
**Out:** YPQ framebuffer format; compute YPQ storage images; duplicate harness-only grade GLSL.

### Target files

- `sit/situation_impl_decl.h` — `SIT_YPQ_GRADE_FRAGMENT_SHADER`
- `sit/situation_impl_renderer.h` — `_SituationInitYpqGradeRenderer`, command recording
- `sit/situation_api.h` — public draw API
- `tests/harness/test_graphics.c`

### Implementation — shader and renderer

- [x] `SIT_YPQ_GRADE_FRAGMENT_SHADER` in `sit/situation_impl_decl.h` — RGB→YIQ→YPQ adjust→clamped RGB; reuses `SIT_QUAD_VERTEX_SHADER`
- [x] Matrix / scale constants kept in sync with `sit/situation_impl_color.h` (`SIT_YIQ_NTSC_*`)
- [x] OpenGL program + Vulkan pipeline via `_SituationInitYpqGradeRenderer()`
- [x] Push constants (Vulkan) / uniforms (GL): `phase_shift_deg`, `chroma_factor`, `luma_factor`, `mix` plus standard quad model/UV

### Implementation — public API

- [x] `SituationCmdDrawTextureYpqGrade(cmd, texture, source, dest, origin, rotation, …)` — records grade draw into command buffer
- [x] Semantics match `SituationImageAdjustYPQ` (same four grade knobs + mix)
- [x] Works with textures backed by framebuffer color attachments (offscreen → grade → swapchain pattern)

### Implementation — CPU performance (companion)

- [x] `sit/situation_impl_color.h` — `fma` in dot products and byte scaling; `SIT_YIQ_NTSC_INV_*` precomputed
- [x] Release `SIT_OPTIMIZE_CFLAGS` in `build_situation.bat` (v2.4.193)
- [x] Harness Q-sweep / RGB-stats paths optimized (`tests/harness/test_misc.c`)

### Tests (`tests/harness/test_graphics.c`)

- [x] `ypq_grade_pass_cpu_parity` — CPU `SituationImageAdjustYPQ` vs GPU `SituationCmdDrawTextureYpqGrade` center-pixel readback
- [x] Tolerance documented (±12 per 8-bit channel; sRGB sample vs linear CPU path)

### Documentation

- [x] Note GPU framebuffer grade + CPU-vs-GPU table in `doc/situation_sdk.md` (see also [I.5](#i5-cpu-vs-gpu-grading-phase-4--implemented))
- [x] `doc/UPDATELOG.md` entry for v2.4.193

---

## Phase 5 — RGL integration

**Status:** Open  
**Layer:** 4  
**Depends on:** Phases 0–2 stable  
**Blocks:** —

### Goal

After core Layers 0–1 are stable, refactor `misc/rgl.h` so generic YPQ helpers call Situation core. CRT-specific TV effects remain in RGL.

### Scope

**In:** Wrapper delegation for lerp, adjust, accessors, equals, gradients.  
**Out:** Changing TV effect behavior; moving scanline/ghost/noise to core.

### Target files

- `misc/rgl.h`
- `misc/RGL_MIGRATION_PLAN.md`
- RGL smoke build (`build_rgl_smoke.bat` if applicable)

### Implementation

- [ ] `RGL_YPQLerp` → calls `SituationYpqLerp`
- [ ] `RGL_YPQAdjustLuminance` → `SituationYpqAdjustLuma`
- [ ] `RGL_YPQAdjustPhase` → `SituationYpqAdjustPhase`
- [ ] `RGL_YPQAdjustQuadrature` → `SituationYpqAdjustChroma`
- [ ] `RGL_YPQGetLuma / Hue / Chroma` → core accessors
- [ ] `RGL_YPQEquals` / `RGL_YPQClosest` → core equivalents
- [ ] `RGL_GenerateYPQGradient` → uses `SituationYpqLerp`
- [ ] TV effects (scanline, ghost, noise, bloom) unchanged — still RGL; use `ColorYPQA` from core
- [ ] No duplicate YIQ matrix constants left in RGL color path

### Tests / verification

- [ ] RGL smoke build passes
- [ ] No behavior change in RGL examples after wrapper refactor

### Documentation

- [ ] Note in `misc/RGL_MIGRATION_PLAN.md` Layer 0 YPQ delegation done
- [ ] `doc/UPDATELOG.md` entry for Phase 5

---

## Phase 6 — Authoring (optional)

**Status:** Open  
**Layer:** 5  
**Depends on:** Phases 0–4; Phase 3 nice-to-have  
**Priority:** Low until Phases 3 and 5 ship

### Goal

Developer-facing tooling: Y×P slice viewer, named palette files, debug overlay — reuse harness sweep / present code.

### Scope

**In:** Dev-only debug draw, palette spec TBD.  
**Out:** Shipping in core library without explicit dev flag.

### Implementation

- [ ] `SituationDrawYpqDebugPlane` (dev-only) — Y×P slice at scrubbed Q
- [ ] Reuse harness sweep / present helpers
- [ ] Palette file format: named `ColorYPQA` list (spec + loader TBD)

### Documentation

- [ ] `doc/UPDATELOG.md` entry for Phase 6 (if shipped)

---

# Part III — Cross-cutting

## III.1 Testing strategy

| Layer | Module | Tests | Phase | Done |
|-------|--------|-------|-------|------|
| Pixel API | `misc` | roundtrip, lerp, adjust, gamut clamp | 1–2 | [x] |
| Image adjust | `misc` | CPU image fill + `image_adjust_ypq` | 2 | [x] |
| Diagnostics | `misc` | slice dup Q=0 ≈ 65280; global unique ≈ 5.6M (±1%) | 3 | [x] |
| Visual sweep | `misc` | 4s Q sweep (256 steps) + stats via public API | 1/3 | [x] sweep / [x] uses Phase 3 API |
| GPU grade | `graphics` | `ypq_grade_pass_cpu_parity` | 4 | [x] |
| RGL | smoke / examples | no behavior change after wrapper refactor | 5 | [ ] |

**Regression anchors:** Store expected `unique_rgb` / `duplicate_mappings` with ±1% tolerance or freeze snapshot on first run.

**Harness env skips (CI):**

- `SIT_SKIP_YPQ_SWEEP` — skip ~4s Q sweep visual
- `SIT_SKIP_YPQ_RGB_STATS` — skip post-sweep registry analysis
- `SIT_SKIP_YPQ_PHOTO_SWEEP` — skip photo Y/P/Q sweep (~12s)

---

## III.2 Documentation deliverables

- [x] `doc/plan/YPQ_COLOR_PLAN.md` (this file)
- [x] `doc/situation_api.md` — YPQ types, pixel API, float path, `SituationImageAdjustYPQ`
- [x] `doc/situation_sdk.md` — when to use RGB vs HSV vs YPQ; GPU grade (Phase 4)
- [x] `doc/situation_api.md` — Phase 3 analyze API (when shipped)
- [ ] `doc/UPDATELOG.md` — entry per merged phase as needed
- [ ] Run `python scripts/generate_situation_api_docs.py` after API changes
- [ ] Run `python scripts/verify_doc_links.py`

---

## III.3 Non-goals

- Replacing linear RGB in PBR/lighting pipeline
- YPQ as primary texture format on GPU
- Automatic YPQ for all loaded assets
- CIELAB / OKLab (different perceptual space; out of scope)
- Fixing 8-bit holes entirely without float/dither (mitigate, not eliminate)

---

## III.4 Open questions (resolve before Phase 3)

- [ ] **Phase shift units** — byte steps (0–255), degrees, or normalized 0–1 in `AdjustYPQ`? *Proposal:* degrees for `SituationImageAdjustYPQ`; byte shift for `SituationYpqAdjustPhase`.
- [ ] **Dither default** — off for real-time, on for export tool? *Proposal:* off by default; separate `FromYPQfDithered`.
- [ ] **Public name** — `Chroma` vs `Quadrature` in API? *Proposal:* **Chroma** in Situation API; RGL keeps `Quadrature` alias for retro docs.

---

## III.5 Success criteria

- [ ] Developer can grade a `SituationImage` in YPQ with the same ease as HSV.
- [x] CPU and GPU grade paths match within documented tolerance (harness `ypq_grade_pass_cpu_parity`).
- [ ] Diagnostics explain RGB collision/holes without custom harness code (Phase 3).
- [ ] RGL retro effects compose on core YPQ types without duplicate math (Phase 5).
- [x] Zero change required to existing RGB rendering or texture upload code paths.
