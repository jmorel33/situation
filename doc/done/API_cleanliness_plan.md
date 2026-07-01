# API Cleanliness Plan — `sit/situation_api.h`

**Status:** **P2 complete** (v2.4.340); P2.4 opaque thread pool **deferred to v2.5** (by-value `SituationThreadPool` contract)  
**Last reviewed:** 2026-06-23 (header umbrella 94 lines; doc umbrella 96 lines; 581 `SITAPI`; `doc/guide/` live)  
**Goal:** improve user-centered ergonomics and header organization without ABI breakage, behavior change, or removal of shipping symbols in a minor release.  
**Constraint:** all shipping changes are **additive**, **comment-only**, or **compiler-warning-only** unless explicitly marked as a major-version cleanup.

**Related:** `doc/situation_api_index.md`, `doc/situation_sdk.md`, `doc/situation_command_reference.md`, `doc/guide/` _(planned — P2.5)_, `doc/plan/v2.5-api-expansion.md`, `doc/plan/FORTRAN_MODULA2_BINDINGS_PLAN.md`, `tools/situation_api_parser.py`, `tools/binding_common.py`, `tools/merge_api_doc_gaps.py`, `tools/run_all.bat`

---

## How to use this file

1. Work phases **P0 → P3** in order unless a item is blocked (e.g. bindings must be regenerated first).
2. Every actionable has a checkbox; tick when done and log in `doc/UPDATELOG.md`.
3. **Before / After** blocks describe the user-visible or maintainer-visible state — not just the diff.
4. Verify **exit criteria** at the end of each phase before moving on.
5. Do **not** rename enums, change numeric values, or remove public symbols without a documented deprecation cycle.

---

## Scope snapshot (baseline)

| Metric | Current state |
|--------|----------------|
| Public header | `sit/situation_api.h` (single umbrella; included via `situation.h`) |
| Split headers already in use | `situation_base_types.h`, `situation_base_callbacks.h`, `situation_base_errno.h`, `situation_base_etc.h` |
| Generated index | `doc/situation_api_index.md` (581 functions) |
| API reference guide | `doc/situation_api.md` (**96-line** umbrella) + **`doc/guide/*.md`** (22 module files; 581/581 coverage) |
| Guide split | ✅ **P2.5 shipped** (v2.4.340) — Option A thin TOC |
| Types vs functions | ~40% typedefs/structs/enums; ~32% function declarations; rest limits/macros/docs |
| Language bindings | **5** generated FFI sets (see **Binding toolchain** below) + `doc/situation_api_index.md` |

---

## Binding toolchain (5 FFI sets + shared parser)

All public API surface consumed by downstream languages flows through **`tools/situation_api_parser.py`**, which today reads primarily **`sit/situation_api.h`** (plus `situation_base_errno.h`, `situation_base_etc.h`, and — for types/callbacks — `situation_base_types.h` / `situation_base_callbacks.h` per generator). **`tools/binding_common.py`** shares signature normalization (strips `__attribute__((deprecated))`, etc.) across generators.

| # | Generator | Output (typical) | Notes |
|---|-----------|------------------|-------|
| 1 | `tools/generate_odin_bindings.py` | `wrappers/Odin/` | Types, callbacks, `foreign situation`, helpers |
| 2 | `tools/generate_zig_bindings.py` | `wrappers/Zig/` | `@extern` imports |
| 3 | `tools/generate_rust_bindings.py` | `wrappers/Rust/` | `extern "C"` block |
| 4 | `tools/generate_fortran_bindings.py` | `wrappers/Fortran/src/` | `bind(C)` procedures |
| 5 | `tools/generate_modula2_bindings.py` | `wrappers/Modula2/src/` | `EXTERN` procedures |

**Docs index (not a 6th language):** `tools/generate_api_index.py` → `doc/situation_api_index.md` — same parser, Markdown tables only.

**Batch regen:** `tools/run_all.bat` runs index + all five generators in order (see `doc/COMPILATION_GUIDE.md`).

### Before / after — header changes vs bindings

| Change type | Binding impact |
|-------------|----------------|
| Comment-only, section reorder | Regen produces cosmetic diffs (section names in index); **no FFI signature change** |
| `SIT_DEPRECATED` / `__attribute__((deprecated))` | Stripped by `binding_common.normalize_c_type` — **symbols still exported to all 5 languages** |
| New `static inline` helpers (P1.6) | **Not** picked up by `SITAPI` regex — C/C++ only unless a generator adds manual wrappers |
| Move `SITAPI` lines into submodule `.h` files (P2.2) | **Breaks regen** unless umbrella `situation_api.h` still exposes every declaration **or** `situation_api_parser.py` is updated to parse/include submodules |
| Opaque struct (`SituationThreadPool`) (P2.4) | All 5 bindings lose field layout — **intentional**; verify no generated struct literals in wrappers |
| `#ifdef` hiding deprecated block (P3.1) | Parser sees whatever the header contains at parse time — document that CI must parse with default `SITUATION_INCLUDE_DEPRECATED_API=1` |

### Binding verification checklist (after any P0+ header edit)

- [x] `python tools/generate_api_index.py` — procedure count unchanged unless symbols added/removed on purpose
- [x] `python tools/generate_odin_bindings.py`
- [x] `python tools/generate_zig_bindings.py`
- [x] `python tools/generate_rust_bindings.py`
- [x] `python tools/generate_fortran_bindings.py`
- [x] `python tools/generate_modula2_bindings.py`
- [x] Review diffs: expect **zero** dropped `Situation*` exports unless major-version removal
- [x] (Optional) `tools/run_all.bat` — full doc + binding regen in one step
- [ ] (Optional) smoke-build one wrapper per language touched by struct/layout changes (P2.4 — N/A, deferred)

**P2.2 gate:** Do not merge physical header split until parser strategy is chosen and all five generators are regen-tested:

- [x] **Option A (preferred):** submodule headers `#include`d into `situation_api.h`; parser follows `situation_api_*.h` includes via `read_expanded_api_lines()` (slice 1).
- [ ] **Option B:** extend `situation_api_parser.py` to concatenate/glob `situation_api_*.h` — update all five generators’ docs. _(Not needed if Option A holds.)_

---

## Regression safety rules (non-negotiable)

- [ ] Document in this plan’s PR description: **no signature changes** to existing public functions unless aliased.
- [ ] **No enum renumbering** — only new enum values at the end.
- [ ] Deprecated symbols stay callable; removal is **major-version** only.
- [ ] `SITUATION_INCLUDE_DEPRECATED_API` (if added) defaults to **1** (on).
- [ ] Physical header splits must preserve `#include "sit/situation_api.h"` as the stable entry point.
- [ ] After any declaration move or new `SITAPI` symbol: run **Binding verification checklist** (all 5 generators + API index).

---

## Phase P0 — Zero-risk hygiene (comments + dedupe)

**Purpose:** fix clear mistakes and unify deprecation signals with no runtime impact.

### P0.1 — Duplicate `SituationGetCPUThreadCount` declaration

| | |
|---|---|
| **Before** | Declared twice in `situation_api.h` (~2286 and ~3233) with the same signature; IDE outline and grep show duplicates; maintainers unsure which comment is canonical. |
| **After** | Single declaration in the **System / Threading** section with one comment: logical processor count from cached topology. |
| **Risk** | None — C allows redundant declarations; removing one is pure cleanup. |

- [x] Remove the duplicate declaration (keep the copy in the threading module, or co-locate with other CPU topology queries).
- [x] Confirm `tools/generate_api_index.py` still lists the symbol once.
- [x] Grep codebase for any doc referencing both line numbers.

### P0.2 — `SituationGetBasePathFromFile` in public API list

| | |
|---|---|
| **Before** | `static char* SituationGetBasePathFromFile(...)` appears in the Filesystem module (~3088) labeled “Internal helper”; actual `static` definition lives in `situation_impl_io.h`. Users may think it is callable API. |
| **After** | Symbol absent from public declaration block; remains `static` in impl only. Trace ID in `situation_base_trace.h` unchanged. |
| **Risk** | None — symbol was never linkable from user TUs. |

- [x] Remove the stray declaration from `situation_api.h`.
- [ ] If needed for trace/docs, add a one-line note in `situation_sdk.md` filesystem internals (optional).

### P0.3 — Unified `SIT_DEPRECATED` macro + attributes on all legacy entry points

| | |
|---|---|
| **Before** | Only `SituationCmdBeginRenderToDisplay` and `SituationCmdEndRender` use `__attribute__((deprecated))`. Others (`SituationUpdate`, `SituationGetDeviceInfo`, `SituationCmdBindUniformBuffer`, `SituationMemoryBarrier`, legacy compute loaders, etc.) are `[DEPRECATED]` in comments only — easy to keep using by accident. |
| **After** | Portable `SIT_DEPRECATED("message")` wraps GCC/Clang `deprecated` and MSVC `deprecated`; applied consistently to every symbol marked deprecated in comments or index. Compilers warn; binaries identical. |
| **Risk** | Low — may surface new warnings in user code that still calls legacy APIs (intended). |

- [x] Add `SIT_DEPRECATED(msg)` near `SITAPI` definition block (no-op on unknown compilers).
- [x] Apply to: `SituationUpdate`, `SituationGetDeviceInfo`, all `SituationCmd*` legacy bind paths, `SituationLoadComputeShader*`, `SituationMemoryBarrier`, and any other `[DEPRECATED]` in index.
- [x] Document in `situation_sdk.md`: “Deprecated APIs warn at compile time; still supported.”
- [x] Verify MSVC + GCC builds with `-Werror` in harness still pass (or suppress only in tests that intentionally call deprecated APIs). — GCC harness build passes; deprecated use in `test_get_device_info` emits `-Wdeprecated-declarations` only (not `-Werror`).

### P0.4 — Consolidated deprecated section at end of header

| | |
|---|---|
| **Before** | Deprecated graphics commands live ~2830 (mid-file, after Hot-Reload); `SituationUpdate` / `SituationGetDeviceInfo` sit in lifecycle/system sections without visual grouping. |
| **After** | One banner `// --- Deprecated API (see situation_api_index.md) ---` at file end lists all deprecated declarations (or forwards to same signatures with deprecated attribute). Active modules stay clean in IDE outline. |
| **Risk** | None for ABI — declaration order does not affect linkage. |

- [x] Move deprecated declarations to final section (can keep duplicate forward declarations removed — single location only).
- [x] Update `doc/situation_api_index.md` deprecated table if section anchors change (regenerate index).
- [x] Run full **Binding verification checklist** — all five FFI sets must still list every deprecated export (warnings are C-only).

**P0 exit criteria**

- [x] `situation_api.h` compiles as today for all harness + examples (`build_situation.bat` opengl + vulkan OK; `build_tests.bat` opengl + vulkan OK; core harness 44/44 each backend).
- [x] No duplicate `SituationGetCPUThreadCount` in header.
- [x] All deprecated symbols emit compiler warning on GCC/Clang when used (`SIT_DEPRECATED` applied; confirmed on `SituationGetDeviceInfo` in harness build).
- [x] `doc/UPDATELOG.md` entry for P0.

---

## Phase P1 — Discoverability (comments, canonical paths, small inlines)

**Purpose:** steer users to the right API without removing alternates.

### P1.1 — Module map at top of function declarations

| | |
|---|---|
| **Before** | Users open a 3k-line header with no in-file map; must know about generated index or scroll module banners scattered at ~2236+. |
| **After** | ~8-line comment block after includes lists modules with line anchors (or submodule filenames post-P2) and links to `doc/situation_api_index.md` + `doc/situation_sdk.md`. |
| **Risk** | None — comments only. Line numbers drift — prefer submodule filenames once P2 lands. |

- [x] Add module map comment block.
- [ ] Regenerate index after any reorder (P2); update map to reference section names not line numbers.

### P1.2 — Trim embedded usage guide

| | |
|---|---|
| **Before** | ~100 lines “SITUATION API USAGE GUIDE” (~2132–2234) duplicates SDK manual; inflates IDE parse time and duplicates maintenance. |
| **After** | Short stub: pointer to `doc/situation_sdk.md` + **five non-negotiable rules** (main thread, `SITUATION_BEGIN_FRAME`, command-buffer ordering, hot-reload dev-only, shutdown leak check). “Titanium tier checklist” lives only in docs. |
| **Risk** | Low — users who never open docs lose long-form narrative; mitigated by keeping critical rules inline. |

- [x] Replace long guide with stub + 5 rules.
- [x] Confirm `doc/situation_sdk.md` contains the moved content (add if missing).
- [ ] Cross-link from `doc/COMPILATION_GUIDE.md` if appropriate.

### P1.3 — Canonical backend query

| | |
|---|---|
| **Before** | Two parallel concepts: `SituationGraphicsBackend` + `SituationGetGraphicsBackend()` (~2316) vs `SituationRendererType` + `SituationGetRendererType()` (~776, ~2821). Examples pick either arbitrarily. |
| **After** | Comments state **`SituationGetGraphicsBackend()` + `SituationGetGraphicsCaps()`** are canonical. `SituationGetRendererType()` remains; comment says “legacy alias — prefer GetGraphicsBackend.” Optional: `static inline` mapper between enums (no new exported symbol required). |
| **Risk** | None if no removal. |

- [x] Add canonical-path comments on both query functions and in `SituationGraphicsCaps` struct.
- [ ] Audit examples/console for consistency (update to canonical names in comments only, or calls if zero behavioral change).
- [x] Update `situation_api_index.md` summaries for both entries.

### P1.4 — Canonical audio device enumeration

| | |
|---|---|
| **Before** | `SituationGetAudioDevices(int* count)` (~2908) with unclear free contract vs `SituationEnumerateAudioDevices` + `SituationFreeDeviceList` (~3077). |
| **After** | Index + comments mark **Enumerate + FreeDeviceList** as canonical. `SituationGetAudioDevices` deprecated with `SIT_DEPRECATED` (P0 macro) and comment describing free semantics or forwarding impl. |
| **Risk** | Low — deprecate only; keep implementation. |

- [x] Document ownership: caller frees with `SituationFreeDeviceList` in SDK.
- [x] Apply `SIT_DEPRECATED` to `SituationGetAudioDevices`.
- [x] Verify both paths still work in harness audio tests.

### P1.5 — “Recommended paths” table in Audio module

| | |
|---|---|
| **Before** | Three overlapping surfaces: handle API (`SituationLoadAudio`), struct API (`SituationSound*`), legacy tones (`SituationPlayTone` / `SituationPlayToneEx`), node graph — no in-header guidance. |
| **After** | Comment table in Audio section: |

| Use case | Preferred API |
|----------|----------------|
| One-shot SFX | `SituationLoadAudio` + `SituationPlayAudio` |
| Per-sound DSP / effects | `SituationSound*` + attach processor |
| Procedural / MIDI | `SituationPlayToneEx` + graph routing |
| Mix / FX chain | `SituationCreateGraph` + nodes |

| **Risk** | None — comments only. |

- [x] Add recommended-paths comment block above audio declarations.
- [x] Align `doc/situation_sdk.md` audio chapter with same table.

### P1.6 — Inline default helpers (match existing patterns)

| | |
|---|---|
| **Before** | `SituationInitInfo` zero-init is common but every example repeats width/height/title/`output_color_depth` boilerplate. Clear values built manually for `SituationCmdClear`. |
| **After** | Static inline helpers alongside existing `SituationRenderPassInfoDefault`: |

```c
static inline SituationInitInfo SituationInitInfoDefault(int width, int height, const char* title);
static inline SituationClearValue SituationClearValueColor(ColorRGBA c);
static inline SituationClearValue SituationClearValueDepth(float depth);
```

| **Risk** | None — new symbols only; `{0}` init behavior unchanged for existing code. |

- [x] Implement inlines next to related types (`SituationInitInfo`, `SituationClearValue`).
- [x] Add harness or unit test that `SituationInitInfoDefault` matches documented defaults (optional).
- [ ] Update one example to use helper (optional, non-blocking).
- [x] Confirm new `static inline` helpers are **not** expected in any of the 5 FFI outputs (document in SDK if C-only).

### P1.7 — Standard backend / thread / ownership tags on functions

| | |
|---|---|
| **Before** | Tags like `[OpenGL Only]`, `[VK+GL]`, `[DEPRECATED]` appear on some functions only; users discover `NOT_IMPLEMENTED` at runtime. |
| **After** | Convention documented in header + applied incrementally to high-traffic modules (Lifecycle, Graphics cmd, Audio): `[GL+VK]`, `[GL]`, `[VK]`, `[Main thread]`, `[Caller frees]`. |
| **Risk** | None — comments only; optional index generator enhancement later. |

- [x] Add tagging convention comment near module map.
- [x] Tag Lifecycle + top 30 graphics commands as pilot.
- [ ] (Optional) Extend `generate_api_index.py` to emit tags into `situation_api_index.md`.

### P1.8 — YPQ / HDR subsection banner

| | |
|---|---|
| **Before** | ~30 YPQ/HDR helpers in Miscellaneous (~3134–3193) mixed with timer APIs — looks like core surface area. |
| **After** | Sub-banner `// --- YPQ / HDR color science (optional tooling) ---` + SDK note that typical apps need only `SituationColorToYPQ` / `SituationColorFromYPQ`. |
| **Risk** | None. |

- [x] Add subsection banner in header.
- [x] One paragraph in `doc/situation_sdk.md` or `YPQ_COLOR_PLAN.md` cross-link.

**P1 exit criteria**

- [x] New user can find module map + SDK link from header alone.
- [x] Canonical backend + audio enumeration documented in header and index.
- [x] `SituationInitInfoDefault` available and documented.
- [x] `doc/UPDATELOG.md` entry for P1 (combined with v2.4.336 release note).

---

## Phase P2 — Structural organization (physical split, types-before-functions)

**Purpose:** make the header maintainable and IDE-friendly without changing the public include path.

### P2.1 — Types-before-functions rule

| | |
|---|---|
| **Before** | Types declared mid-function-section: `SituationOSInfo`, `SituationProcessInfo` (~2290), raster enums (~2510), `SituationMeshVertexLayout` (~2650), `SituationShaderStorageBlockInfo` (~2697), `SituationMidiDeviceInfo` (~3010), threading topology (~3203). |
| **After** | All public typedefs live in typed sections **before** any `SITAPI` prototype in that module (or in submodule headers). Function section is prototype-only. |
| **Risk** | Low — order of declarations unchanged for linkage; may affect binding generators that assume textual order (regenerate and test). |

- [x] Inventory mid-file typedefs (grep `typedef` after first `SITAPI` in each module).
- [x] Move each typedef up within module or into typed submodule (P2.2).
- [x] Full rebuild + **Binding verification checklist** (all 5 generators).

### P2.2 — Physical submodule split (umbrella unchanged)

**Purpose:** split the monolithic public header into domain files with clear ownership, while preserving **`#include "sit/situation_api.h"`** as the only supported user entry point and keeping all **581 `SITAPI`** symbols linkable with identical signatures.

**Assessment baseline:** v2.4.338 · measured 2026-06-23 · rerun: `python tools/audit_api_header_layout.py`

---

#### BEFORE state (current monolith)

**File topology today**

```
sit/
├── situation_base_version.h      (11 lines — canonical version)
├── situation_base_errno.h        (402 — SituationError)
├── situation_base_types.h        (180 — handles, math, stream types)
├── situation_base_callbacks.h      (266 — callback typedefs)
├── situation_base_etc.h          (190 — keys, colors, MIDI table)
├── situation_base_font.h         (282 — embedded font data)
├── situation_base_trace.h        (2761 — generated trace IDs; not user API)
├── situation.h                   (bridge → situation_api.h + impl)
└── situation_api.h                 ← 3,266 lines, SINGLE public API file
```

**`situation_api.h` size breakdown**

| Region | Lines | Share | Contents |
|--------|------:|------:|----------|
| License + `SITAPI` / `SIT_DEPRECATED` macros | ~170 | 5% | Portable export + deprecation |
| `#include` base chain + early mixed decls | ~40 | 1% | Pulls in `situation_base_*`; **`SituationLog*` / `SituationShowMessageBox` declared here** (before module banners) |
| Config, limits, flags, threading **type** exposure | ~1,350 | 41% | Init/window/feature flags; **full `SituationThreadPool` / `SituationJob` struct layout** (~200 lines); audio graph **type** blocks (~400 lines); render-pass / VD / barrier types |
| P2.1 consolidated public types block | ~242 | 7% | OS/backend, raster, mesh, camera, MIDI, YPQ stats, topology |
| Module map + API quick rules | ~24 | 1% | Maintainer map (P1) |
| **`SITAPI` function declarations** | ~835 | 26% | 582 prototypes across ~90 `// --- subsection ---` banners |
| Deprecated API block (EOF) | ~41 | 1% | 11 `SIT_DEPRECATED` symbols |

**API surface by domain** (function region only — approximate)

| Domain | `SITAPI` count | Function-region lines | Notes |
|--------|---------------:|----------------------:|-------|
| Graphics (cmds + resources + VD + compute) | 168 | ~216 | Largest single domain; justified as one file |
| Audio (playback + graph + MIDI + serialization) | 102 | ~171 | Second domain; graph/MIDI already type-heavy above |
| System services (FS + threading + timers + YPQ + color) | 107 | ~145 | FS ~24; threading ~43; YPQ ~28 |
| Core platform (lifecycle + introspection + profiling) | 56 | ~94 | Init, frame, callbacks, CPU/GPU queries |
| Window & display | 58 | ~75 | Monitors, cursor, clipboard |
| Input | 42 | ~58 | Keyboard / mouse / gamepad |
| Image & font | 29 | ~45 | Between window and graphics in docs, **not** in file order today |
| Deprecated (EOF block) | 11 | ~22 | Already isolated at EOF (P0) |
| Hot-reload (5 APIs) | 5 | ~10 | **Physically before Input module** in file (~2853) — ordering bug |
| Other / inline comments | ~4 | — | Legacy comment-only stubs at EOF |

**Structural pain points (why split)**

| Issue | Impact |
|-------|--------|
| **74% of file is non-function** (types/macros mixed with scattered early `SITAPI`) | Editing audio graph types risks breaking graphics includes; IDE outline noisy |
| **No boundary vs `situation_base_*`** | Unclear where new handles vs domain structs belong |
| **Module order ≠ doc order** | Hot-reload before input; tone APIs mid-type block; thread types at line ~307, thread API at ~3183 |
| **Thread pool fully exposed** | ~200 lines of atomics/queues in public header (P2.4 candidate) |
| **Single merge hotspot** | Every API PR touches one 3.3k-line file |
| **Binding parser reads one file** | Split is safe only if umbrella re-exports everything (Option A) |

**What already works (do not regress)**

| Item | Status |
|------|--------|
| User include path | `#include "sit/situation.h"` or `"sit/situation_api.h"` only |
| Base header split | `situation_base_types.h`, `_callbacks.h`, `_errno.h`, `_etc.h` — keep as-is |
| P2.1 types-before-functions | Typedefs consolidated before function block (lines 2165–2404) |
| P0 deprecated isolation | EOF deprecated section + `SIT_DEPRECATED` on 11 symbols |
| FFI / index | 581 symbols via `tools/situation_api_parser.py` — **must remain 581 after split** |

**Direct `#include "sit/situation_api.h"` consumers** (must not require changes)

- `sit/situation.h`
- `tests/harness/sit_api_include.h`
- `build/_offsetof_test.c`
- `doc/misc/rgl.h` (legacy path)

---

#### AFTER state (target architecture)

**Design principles**

1. **Umbrella-only public entry** — users and bindings never include submodule headers directly.
2. **Types before declarations within each domain file** — continue P2.1 rule per file.
3. **~7–9 content files** — enough separation for merge concurrency; avoid micro-headers (under ~80 lines each).
4. **`situation_base_*` = primitives**; **`situation_api_*` = domain** — document the boundary (table below).
5. **P2.3 reorder happens inside split files** — do not split first then reorder the monolith twice.

**Target file layout** (replaces earlier 11-file draft — fixes `lifecycle`/`misc` lumping)

| # | File | Est. lines | Owns |
|---|------|----------:|------|
| U | **`situation_api.h`** | **≤120** | Guards, module map, `#include` chain, optional `SITUATION_INCLUDE_DEPRECATED_API` gate |
| 1 | `situation_api_config.h` | ~450 | `SIT_DEPRECATED`, limits (`SITUATION_MAX_*`), init/window/feature/thread **compile** flags, barrier bit aliases, `SITUATION_BEGIN_FRAME`, log-level enums/macros — **no domain typedefs** |
| 2 | `situation_api_types_gpu.h` | ~650 | Render passes, attachments, barriers, VD structs, raster/camera/mesh typedefs (incl. P2.1 graphics block), texture/sampler enums tied to GPU |
| 3 | `situation_api_types_audio.h` | ~450 | Node graph, device metadata, mixer, MIDI port types, resonance/tone **type** blocks |
| 4 | `situation_api_types_system.h` | ~550 | `SituationInitInfo`, timers/oscillators, color/YPQ **structs**, OS/topology types (P2.1 system block), init-state enum |
| 5 | `situation_api_platform.h` | ~320 | **Lifecycle**, frame, callbacks, args, **window**, **input**, **image/font**, system introspection queries, early logging `SITAPI` moved here from preamble |
| 6 | `situation_api_graphics.h` | ~280 | All GPU **functions**: frame acquire, `SituationCmd*`, resources, shaders, textures, compute, VD, models, screenshots, backend accessors |
| 7 | `situation_api_audio.h` | ~200 | Playback, capture, tones, graph, MIDI, serialization, device registry |
| 8 | `situation_api_system.h` | ~200 | Filesystem, thread-pool **API**, timer/YPQ **functions**, hot-reload, remaining diagnostics |
| 9 | `situation_api_deprecated.h` | ~45 | All deprecated declarations (P3-gated) |

**Total content ~2,950 lines** (+ umbrella overhead) — same symbols, less merge contention.

**Include order (umbrella — must preserve dependency direction)**

```
situation_api.h
  → situation_base_errno.h / types / callbacks / etc   (unchanged)
  → situation_api_config.h
  → situation_api_types_system.h
  → situation_api_types_gpu.h
  → situation_api_types_audio.h
  → situation_api_platform.h
  → situation_api_graphics.h
  → situation_api_audio.h
  → situation_api_system.h
  → [if SITUATION_INCLUDE_DEPRECATED_API] situation_api_deprecated.h
```

System types before GPU types because `SituationInitInfo` and caps structs reference enums that GPU types may use; audio types last among type files (graph types reference handles from base). **Validate with full GL+VK compile after first cut.**

**`situation_base_*` vs `situation_api_*` boundary**

| Goes in `situation_base_*` | Goes in `situation_api_types_*` |
|----------------------------|----------------------------------|
| Opaque handles (`SituationMesh`, `SituationShader`, …) | Structs with multiple fields users stack-allocate |
| Math types (`Vector3`, `mat4`, `ColorRGBA`) | Render-pass / graph configuration structs |
| Callback typedef signatures | Enums tied to one domain (raster, node types) |
| `SituationError`, key codes, color constants | Anything referencing domain-specific nested arrays |

**Rule:** new types needed by **≥2 domains** → consider `situation_base_types.h`; otherwise domain type file.

---

#### Gap analysis (before → after)

| Current location (monolith) | Target file | Action |
|----------------------------|-------------|--------|
| Lines 205–207 logging `SITAPI` in preamble | `situation_api_platform.h` | Move with lifecycle |
| Thread pool struct ~307–560 | `situation_api_types_system.h` until P2.4 opaque | Keep public or shrink in P2.4 follow-up |
| Audio graph types ~1593–1848 | `situation_api_types_audio.h` | Move intact |
| Render/VD types ~1451–1540, ~1983+ | `situation_api_types_gpu.h` | Move intact |
| P2.1 block 2165–2404 | Split across types_gpu / types_audio / types_system | Partition by typedef domain |
| Function subsections Core…Window | `situation_api_platform.h` | Merge platform layer |
| Graphics cmd + resources | `situation_api_graphics.h` | Single GPU decl file |
| Audio + MIDI functions | `situation_api_audio.h` | Co-locate (P2.3 tone move) |
| FS + threading + YPQ + timers | `situation_api_system.h` | Services bucket |
| Hot-reload ~2853 (before input today) | `situation_api_system.h` | **Reorder** — no longer before input |
| Deprecated EOF | `situation_api_deprecated.h` | Extract (already grouped) |

**Explicitly deferred (not P2.2 scope)**

| Item | Phase |
|------|-------|
| Declaration order vs index narrative | **P2.3** (inside new files) |
| Opaque `SituationThreadPool` | **P2.4** — may collapse `types_system` thread section |
| Doc split to `doc/guide/` | **P2.5** — finer granularity than header (OK) |
| Moving types into `situation_base_*` | Out of scope — only reorganize `situation_api_*` |

---

#### Success criteria (measurable exit)

| Metric | Before | After (required) |
|--------|--------|------------------|
| Public include path | `sit/situation_api.h` | **Unchanged** |
| `SITAPI` symbol count | 581 | **581** (parser + index) |
| Deprecated symbols | 11 with warnings | **11** — still in `situation_api_deprecated.h` |
| Largest content file | 3,266 lines | **≤700 lines** (types_gpu target) |
| Umbrella `situation_api.h` | 3,266 lines | **≤120 lines** |
| Direct submodule includes in user code | 0 | **0** (grep repo — only umbrella + impl) |
| Harness + examples GL+VK | green | **green** |
| Binding regen diff | — | **zero dropped exports** |

---

#### Risks and mitigations

| Risk | Severity | Mitigation |
|------|----------|------------|
| `#ifdef` / `#if defined(SITUATION_ENABLE_THREADING)` order breaks compile | High | Move `#ifdef` blocks whole with their content; compile matrix after each file extraction |
| Include cycle between type files | Medium | One-way chain: config → system types → gpu types → audio types; no cross-includes between type files |
| Parser misses symbols in submodules | High | **Option A only:** submodules included by umbrella; parser unchanged |
| Merge conflict during long-running split PR | Medium | Land as sequenced commits: config → types (3) → platform → graphics → audio → system → deprecated → trim umbrella |
| P2.4 opaque pool invalidates `types_system` layout | Low | Document thread types as volatile until P2.4; split still worth doing now |

**Parser strategy (gate — unchanged from Binding toolchain section):**

- [x] **Option A (required):** submodule headers `#include`d from umbrella only; `read_expanded_api_lines()` in parser.
- [ ] **Option B:** not planned unless Option A fails a platform compile.

---

#### Implementation checklist

- [x] Run baseline audit: `python tools/audit_api_header_layout.py` → attach output to PR / UPDATELOG.
- [x] Create `situation_api_config.h`; move macros/limits (slice 1: `SITUATION_BEGIN_FRAME`, init flag, `SITUATION_MAX_*` — ~113 lines; window/feature flags deferred).
- [x] Create three type headers; partition P2.1 + existing typedef blocks per gap table. _(config + system + gpu + audio done)_
- [x] Create five declaration headers; move `SITAPI` blocks per gap table. _(platform, graphics, audio, system, deprecated done)_
- [x] Extract `situation_api_deprecated.h`; wire P3 `#ifndef SITUATION_INCLUDE_DEPRECATED_API` stub in umbrella.
- [x] Trim umbrella to guards + map + `#include` chain only.
- [x] Update `sit/situation.h` bridge comment with split file list.
- [x] **Binding verification checklist** (all 5 generators + index = 581).
- [x] Full harness + `examples/console` GL + VK matrix. _(Harness static-opengl/vulkan build + core init smoke; console is header-only module.)_
- [x] Grep: no new `#include "situation_api_*.h"` outside umbrella + impl.
- [x] `doc/UPDATELOG.md` entry for P2.2 with before/after line counts.

**P2.2 shipped as v2.4.339** — declaration split complete. **P2 complete as v2.4.340** (bindings, harness, P2.5 doc guide).

#### P2.2 migration log (piece-by-piece)

Re-run after each slice: `python tools/audit_api_header_layout.py` and `python tools/generate_api_index.py`.

| Slice | File | Status | Umbrella lines | Submodule lines | SITAPI (index) | Build |
|-------|------|--------|---------------:|----------------:|---------------:|-------|
| 0 (baseline) | — | done | 3266 | — | 581 | green |
| 1 | `situation_api_config.h` | **done** | 3162 (−104) | 113 | 581 | GL green |
| 2 | `situation_api_types_system.h` | **done** | 2616 (−546) | 558 | 581 | GL+VK green |
| 3 | `situation_api_types_gpu.h` | **done** | 1622 (−994) | 1007 | 581 | GL+VK green |
| 4 | `situation_api_types_audio.h` | **done** | 1385 (−237) | 254 | 581 | GL+VK green |
| 5 | `situation_api_platform.h` | **done** | 927 (−458) | 478 | 581 | GL+VK green |
| 6 | `situation_api_graphics.h` | **done** | 703 (−224) | 244 | 581 | GL+VK green |
| 7 | `situation_api_audio.h` | **done** | 497 (−206) | 219 | 581 | GL+VK green |
| 8 | `situation_api_system.h` + `situation_api_deprecated.h` | **done** | 320 (−177) | 179 + 37 | 581 | GL+VK green |
| 9 | Trim umbrella to ≤120 lines | **done** | **84** | — | 581 | GL+VK green |

**Slice 9 goal check:** public include unchanged ✅ · symbol count 581 ✅ · GL+VK build ✅ · umbrella **84 lines** (−97% from baseline) ✅ · orphan stubs removed ✅ · `SIT_MALLOC` moved to config ✅ · P2.2 declaration extraction **complete** ✅

**Slice 8 goal check:** public include unchanged ✅ · symbol count 581 ✅ · GL+VK build ✅ · umbrella 320 (−90% from baseline) ✅ · system 179 + deprecated 37 lines ✅ · `SITUATION_INCLUDE_DEPRECATED_API` gate wired (default 1) ✅

**Slice 7 goal check:** public include unchanged ✅ · symbol count 581 ✅ · GL+VK build ✅ · umbrella 497 (−85% from baseline) ✅ · audio 219 lines ✅ · `SituationPlayToneEx` co-located with audio ✅ · graph SFX routing included ✅

**Slice 6 goal check:** public include unchanged ✅ · symbol count 581 ✅ · GL+VK build ✅ · umbrella 703 (−78% from baseline) ✅ · graphics 244 lines ✅ · hot-reload left in umbrella for slice 8 ✅

**Slice 5 goal check:** public include unchanged ✅ · symbol count 581 ✅ · GL+VK build ✅ · umbrella 927 (−72% from baseline) ✅ · platform 478 lines ✅

**Slice 4 goal check:** public include unchanged ✅ · symbol count 581 ✅ · GL+VK build ✅ · umbrella 1385 ✅ · types_audio 254 lines ✅ · all type headers complete ✅

**Slice 4 fix (2026-06-23):** Do **not** typedef `ma_device_id` in `situation_api_types_audio.h` — `#ifndef ma_device_id` does not guard typedef names; a stub struct conflicts with `miniaudio.h` when included via `situation.h`. Rely on miniaudio (DLL path) or `tests/harness/sit_api_include.h` (standalone parse).

**Slice 3 goal check:** public include unchanged ✅ · symbol count 581 ✅ · GL+VK build ✅ · umbrella 1622 (−1644 from baseline) ✅ · types_gpu 1007 lines (largest file now submodule, not umbrella) ✅ · umbrella ≤120 ⬜

**Slice 2 goal check:** public include unchanged ✅ · symbol count 581 ✅ · GL+VK build ✅ · umbrella 2616 (−650 from baseline) ✅ · types_system 558 lines ✅ · largest file still umbrella ⬜

**Slice 1 goal check:** public include unchanged ✅ · symbol count 581 ✅ · zero dropped exports ✅ · no user `#include situation_api_config.h` ✅ · largest file still monolith (3162) ⬜ · umbrella ≤120 ⬜

---

### P2.3 — Module order aligned with docs

| | |
|---|---|
| **Before** | Thread types at ~307, thread API at ~3195; `SituationPlayToneEx` at ~1763; MIDI split from audio graph; deprecated block mid-graphics. |
| **After** | Declaration order matches `situation_api_index.md` / command reference narrative: Lifecycle → Window → Input → Image → Graphics (types → resources → commands) → Audio (+ graph + MIDI) → Filesystem → Timers/Color → Threading → Deprecated. |
| **Risk** | Low for ABI; medium for merge conflicts — do in dedicated PR. |

- [x] Reorder modules per table above (within split files if P2.2 done).
- [x] Move `SituationPlayToneEx` prototype into Audio module next to other tone APIs.
- [x] Co-locate MIDI with Audio graph section. _(already contiguous in `situation_api_audio.h`)_
- [x] Regenerate `doc/situation_api_index.md`.

**P2.3 goal check:** platform order Core → Window → Image → Input → Logging ✅ · audio tones block co-located ✅ · `tools/p23_reorder_api_modules.py` verifies 581 declarations unchanged ✅ · GL+VK build ✅

### P2.5 — Physical guide split (`doc/guide/`, umbrella unchanged)

**Purpose:** mirror P2.2 for documentation — split the monolithic API reference into maintainable module files while keeping **`doc/situation_api.md`** as the stable entry URL (same role as `#include "sit/situation_api.h"`).

| | |
|---|---|
| **Before** | Single `doc/situation_api.md` (~16k lines, 22 `<details>` module sections). Header metadata and symbol coverage were refreshed in v2.4.338, but narrative examples and struct prose still drift; any edit risks merge conflicts and broken anchors. |
| **After** | Module content lives under **`doc/guide/`**; umbrella `doc/situation_api.md` is a short front matter + module map + links (or optional build-time concat — see gate below). Each guide file owns one module’s structs, enums, and function entries. |

**Target layout** (doc modules map to P2.2 header files — doc may stay finer-grained than code)

| P2.2 header file | Primary `doc/guide/` files (P2.5) |
|------------------|-----------------------------------|
| `situation_api_platform.h` | `core.md`, `window_display.md`, `input.md`, `image.md`, `system_introspection.md` |
| `situation_api_graphics.h` | `graphics.md`, `renderer_bolster.md`, `compute.md`, `text_rendering.md`, `drawing_2d.md` |
| `situation_api_audio.h` | `audio.md`, `audio_graph.md`, `midi.md` |
| `situation_api_system.h` | `filesystem.md`, `threading.md`, `ypq_color.md`, `miscellaneous.md`, `hot_reload.md`, `logging.md` |
| `situation_api_deprecated.h` | `deprecated.md` |
| `_front_matter` + umbrella | `_front_matter.md`, thin `situation_api.md` |
| — | `examples_faq.md` (narrative only) |

Full file list (P2.5 detail):

| File | Contents (from current `situation_api.md` sections) |
|------|-----------------------------------------------------|
| `doc/guide/_front_matter.md` | Title blurb, version line, doc map links, naming conventions (shared intro only) |
| `doc/guide/core.md` | Core Module |
| `doc/guide/window_display.md` | Window and Display Module |
| `doc/guide/image.md` | Image Module |
| `doc/guide/graphics.md` | Graphics Module |
| `doc/guide/input.md` | Input Module |
| `doc/guide/audio.md` | Audio Module |
| `doc/guide/audio_graph.md` | Audio Node Graph Module |
| `doc/guide/midi.md` | MIDI Integration Module |
| `doc/guide/filesystem.md` | Filesystem Module |
| `doc/guide/threading.md` | Threading Module |
| `doc/guide/system_introspection.md` | System Introspection Module |
| `doc/guide/ypq_color.md` | YPQ Color Module |
| `doc/guide/renderer_bolster.md` | Renderer Bolster Commands |
| `doc/guide/compute.md` | Compute Shaders |
| `doc/guide/text_rendering.md` | Text Rendering |
| `doc/guide/drawing_2d.md` | 2D Rendering & Drawing |
| `doc/guide/drawing_3d.md` | 3D Rendering & Drawing |
| `doc/guide/virtual_display.md` | Virtual Display Compositor |
| `doc/guide/hot_reload.md` | Hot-Reloading Module |
| `doc/guide/logging.md` | Logging Module |
| `doc/guide/miscellaneous.md` | Miscellaneous Module |
| `doc/guide/deprecated.md` | Deprecated APIs table + migration notes |
| `doc/guide/examples_faq.md` | Examples & Tutorials + FAQ & Troubleshooting |

**Umbrella `doc/situation_api.md` after split:**

```markdown
# Situation v2.4.x API Programming Guide
…version blurb, links to SDK / index / command reference…
## Module reference
- [Core](guide/core.md)
- [Window & display](guide/window_display.md)
…
```

| **Risk** | Low for readers (additive paths); medium for maintainers — inbound links from SDK, plans, and `verify_doc_links.py` must be updated or redirected. |

**P2.5 gate — choose one umbrella strategy before moving content:**

- [x] **Option A (preferred):** `doc/situation_api.md` stays a **thin TOC + front matter** linking to `doc/guide/*.md`; no generated monolith. Update all internal links once.
- [ ] **Option B:** add `tools/build_api_guide.py` to **concatenate** `doc/guide/*.md` → `doc/situation_api.md` for single-file offline readers; CI verifies concat hash or regen diff. _(Not chosen.)_

**Extraction workflow (match P2.2 discipline):**

- [x] Inventory module boundaries — use existing `<summary><h3>…</h3></summary>` tags (22 sections today).
- [x] Create `doc/guide/` and move each section body into its file (preserve heading anchors inside module files).
- [x] Replace monolith section bodies with one-line links in umbrella (Option A) or wire concat script (Option B).
- [x] Point `tools/merge_api_doc_gaps.py` at **`doc/guide/*.md`** (not only monolith) so future header gaps land in the right module file.
- [x] Run `scripts/verify_doc_links.py`; fix broken `#` anchors and cross-module references.
- [ ] Update `doc/situation_sdk.md`, `doc/COMPILATION_GUIDE.md`, `.kiro/steering/situation-project.md` doc map to list `doc/guide/`.
- [x] Sync header metadata in `_front_matter.md` + umbrella on each release (steering patch-bump checklist).

**Content cleanup pass (same PR or follow-up PR — do not block file split):**

- [ ] Audit usage examples per module against current struct field names (Core `SituationInitInfo` pilot done v2.4.338).
- [ ] Align deprecated table in `doc/guide/deprecated.md` with header `SIT_DEPRECATED` list (P0 symbols).
- [ ] Optional: extend `generate_api_index.py` to report which **guide file** owns each missing symbol.

**P2.5 exit criteria**

- [x] Every former `<details>` module section has a dedicated file under `doc/guide/`.
- [x] `doc/situation_api.md` ≤ ~150 lines if Option A, or regen-verified concat if Option B.
- [x] `python tools/merge_api_doc_gaps.py` → 581/581 coverage across guide tree.
- [x] `scripts/verify_doc_links.py` passes (or documented exceptions list updated).
- [x] `doc/UPDATELOG.md` entry for P2.5.

### P2.4 — Opaque thread pool handles (optional, bindings-sensitive)

| | |
|---|---|
| **Before** | Full `SituationJob` and `SituationThreadPool` struct definitions (~200 lines) with atomics, ring buffers, `mtx_t` exposed in public header. |
| **After** | Public header: opaque `typedef struct SituationThreadPool SituationThreadPool;` + job ID type; full definitions in `situation_impl_thread.h` (or internal) included only from implementation. Advanced users inspecting impl unchanged. |
| **Risk** | Medium — code that embeds `SituationThreadPool` by value breaks; grep for stack allocation before doing. |

- [x] Grep for `SituationThreadPool` by-value usage outside impl — **found:** `examples/10_thread_pool/main.c`, harness tests, `situation_impl_decl.h`.
- [ ] If only pointer usage: switch to opaque forward decl in public header. _(Blocked — by-value contract is shipping API; defer to v2.5.)_
- [ ] Run **Binding verification checklist**; fix any wrapper that assumed public struct fields (all 5 languages). _(N/A until opaque change.)_
- [x] Document in UPDATELOG: deferred to v2.5 — caller-owned `SituationThreadPool` storage remains public.

**P2 exit criteria**

- [x] `situation_api.h` (umbrella) ≤ ~200 lines if split complete, or monolith with strict types-before-functions.
- [x] IDE outline: no typedef between function prototypes in any module file.
- [x] All tests green; **Binding verification checklist** complete (5 FFI + index).
- [x] **P2.5:** `doc/guide/` split complete; umbrella `doc/situation_api.md` stable; link verification green.
- [x] `doc/UPDATELOG.md` entry for P2.

---

## Phase P3 — Optional strict mode (major-version gated)

**Purpose:** allow “clean SDK” consumers to hide deprecated surface — **default remains fully compatible**.

### P3.1 — `SITUATION_INCLUDE_DEPRECATED_API`

| | |
|---|---|
| **Before** | Deprecated APIs always visible in header; no opt-out. |
| **After** | `#ifndef SITUATION_INCLUDE_DEPRECATED_API` → `#define SITUATION_INCLUDE_DEPRECATED_API 1` `#endif`; when set to `0` before include, deprecated section omitted from umbrella (implementations may remain for ABI on shared libs — document DLL caveat). |
| **Risk** | Medium for shared DLL users calling deprecated exports; static linking only recommended with `0`. |

- [ ] Implement include guard around `situation_api_deprecated.h`.
- [ ] Document default `1` and shared-library caveat in SDK.
- [ ] Do **not** change default to `0` without major version bump.

### P3.2 — Major-version removal backlog (documentation only)

| | |
|---|---|
| **Before** | Deprecated symbols accumulate without a removal schedule. |
| **After** | Table in this doc (or `UPDATELOG`) listing symbols eligible for removal in **v3.0+** after N releases with warnings. |
| **Risk** | N/A until major release. |

- [ ] List candidate removals: `SituationUpdate`, legacy bind commands, `SituationGetDeviceInfo`, duplicate audio enum path, etc.
- [ ] Set policy: minimum 2 minor releases with `SIT_DEPRECATED` before removal.

**P3 exit criteria**

- [ ] Opt-in deprecated hiding works in static build smoke test.
- [ ] Default build unchanged.
- [ ] Removal backlog published; no symbols removed in this phase.

---

## Patterns to preserve (do not regress)

These are already good — extend, don’t replace:

| Pattern | Location | Action |
|---------|----------|--------|
| `SITUATION_BEGIN_FRAME()` | ~552 | Keep in stub rules; mention in module map |
| `SituationRenderPassInfoDefault` / `Load` | ~1250 | Template for P1.6 helpers |
| `SituationScopedString` (C++) | ~675 | Document as pattern for `[Caller frees]` APIs |
| `sit_screenshot_format_ext[]` | ~765 | Template for enum+extension tables |
| Split base headers | `situation_base_*.h` | Continue migration of pure types |
| Generated index | `tools/generate_api_index.py` | Regenerate after every phase |

- [ ] Review each pattern after P2 split still visible from umbrella or documented.

---

## Naming convention (new symbols only — no renames)

| | |
|---|---|
| **Before** | Mixed prefixes: `SIT_*`, `SITUATION_*`, `Situation*` functions; search and docs inconsistent. |
| **After** | Documented convention in header comment: enum values → `SIT_*`; compile limits → `SITUATION_*`; functions → `Situation*`; new flags follow nearest module precedent. **No renames of shipping values.** |
| **Risk** | None if additive documentation only. |

- [ ] Add naming convention block to module map / SDK conventions section.
- [ ] Audit P1.6 new helpers for naming compliance.

---

## Verification checklist (run after each phase)

- [ ] `build_situation.bat` opengl — examples/console compile.
- [ ] `build_tests.bat` opengl — harness passes (or targeted modules touched).
- [ ] Grep `situation_api.h` for duplicate `SITAPI` function names.
- [ ] **Binding verification checklist** — all 5 generators + API index; diff reviewed.
- [ ] Wrapper smoke-build at least one language if struct layout or enum set changed (P2.4).
- [ ] No new compiler warnings in harness unless deprecated API tests intentionally suppress.

---

## Suggested PR sequence

| PR | Phase | Summary |
|----|-------|---------|
| 1 | P0 | Dedupe, remove stray static decl, `SIT_DEPRECATED`, deprecated section at end |
| 2 | P1 | Module map, trim guide, canonical comments, inlines, tags, YPQ banner |
| 3 | P2a | Types-before-functions moves (monolith) — **done v2.4.337** |
| 4 | P2b | Physical header split + module reorder + index regen |
| 5 | P2c | Opaque thread pool (only if grep clean) |
| 6 | P2d | **`doc/guide/` split** — extract module sections from `situation_api.md`; thin umbrella; update merge/verify scripts |
| 7 | P3 | Optional deprecated include gate + v3 removal backlog doc |

---

## Open questions (resolve before P2.4 / P3)

- [x] Do any third-party or example projects embed `SituationThreadPool` by value? **Yes** — see P2.4 deferral note.
- [x] Does `situation_api_parser.parse_api_header()` need multi-file input for P2.2, or is umbrella `#include` enough for all 5 generators? **Umbrella `#include` chain is enough** (`read_expanded_api_lines()`).
- [ ] Should `SituationGetRendererType` gain `SIT_DEPRECATED` in P1 or only after one release of comment-only canonical guidance?
- [ ] Shared DLL (`SITUATION_BUILD_SHARED`): is hiding deprecated declarations from header while exports remain acceptable for v2.x?

---

## Master progress tracker

| Phase | Status | Notes |
|-------|--------|-------|
| P0 — Zero-risk hygiene | ✅ Complete | Verified via `build/` scripts 2026-06-23 |
| P1 — Discoverability | ✅ Complete | v2.4.336; core harness 46/46 |
| P2 — Structural organization | ✅ Complete | v2.4.340 — P2.2 header split, P2.3 reorder, **P2.5 doc guide**; P2.4 deferred v2.5 |
| P3 — Optional strict mode | ⬜ Not started | |

**Document history**

| Date | Change |
|------|--------|
| 2026-06-23 | Initial plan from `situation_api.h` review (~v2.4.331 index) |
| 2026-06-23 | Added **Binding toolchain** section — all 5 FFI generators + shared parser gates |
| 2026-06-23 | **P0+P1 shipped as v2.4.336** — inline helpers, module map, canonical API guidance, `SituationGetAudioDevices` deprecated |
| 2026-06-23 | **P2.1 shipped as v2.4.337** — mid-file typedefs consolidated before SITAPI declarations; bindings regen 581 |
| 2026-06-23 | **P2.3** — module declaration reorder inside split headers (`p23_reorder_api_modules.py`); 581 SITAPI unchanged |
| 2026-06-23 | **P2.2 shipped as v2.4.339** — nine `situation_api_*.h` submodules; umbrella 94 lines; 581 SITAPI; GL+VK green |
| 2026-06-23 | **v2.4.338** — doc gap merge (`tools/merge_api_doc_gaps.py`); `situation_api.md` 581/581; steering patch-bump checklist |
| 2026-06-23 | **P2.5 added** — plan physical split of `doc/situation_api.md` → `doc/guide/*.md` (parallel to P2.2 header split) |
| 2026-06-23 | **P2.2 slice 5** — `situation_api_platform.h` (lifecycle/window/input/image/logging); umbrella 927 lines |
| 2026-06-23 | **P2.2 slice 4** — `situation_api_types_audio.h` (graph, mixer, MIDI, wave/filter); umbrella 1385 lines |
| 2026-06-23 | **P2.2 slice 3** — `situation_api_types_gpu.h` (render/VD/barriers/P2.1 graphics block); umbrella 1622 lines |
| 2026-06-23 | **P2.2 slice 2** — `situation_api_types_system.h` (init, timers, thread pool, OS/topology); umbrella 2616 lines |
| 2026-06-23 | **P2.2 slice 1** — `situation_api_config.h` (limits + `SITUATION_BEGIN_FRAME`); parser `read_expanded_api_lines()`; umbrella 3162 lines |
