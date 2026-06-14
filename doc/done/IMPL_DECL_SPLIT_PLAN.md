# situation_impl_decl.h Extraction Plan

**Date:** 2026-05-04  
**Completed:** v2.4.5  
**Status:** ✅ COMPLETE — split executed and architecture has since evolved further

---

## Goal

Extract all internal type definitions, struct declarations, static globals, macros, forward
declarations, and embedded data from a monolithic `situation_impl.h` into a set of dedicated
headers, leaving `situation_impl.h` as a pure orchestrator (includes only, no code).

---

## Final Architecture (as shipped)

```
situation.h
└── sit/situation_impl.h          ← Pure orchestrator — 10 #includes, zero function bodies
    ├── situation_base_font.h     (1) embedded font data
    ├── situation_impl_deps.h     (2) third-party: STB, miniaudio, glad, VMA
    ├── situation_impl_decl.h     (3) types, structs, globals, shaders, inline helpers
    ├── situation_impl_forward.h  (4) cross-module forward declarations
    ├── situation_impl_etc.h      (5) math/string utilities
    ├── situation_impl_proj.h     (5) projection/camera helpers
    ├── situation_impl_timer.h    (6) oscillators, high-res time
    ├── situation_impl_threading*.h (7) thread pool + topology + NUMA
    ├── situation_impl_io.h       (7) file I/O, async, system info
    ├── situation_impl_input.h    (7) keyboard, mouse, gamepad
    ├── situation_impl_wdm.h      (7) window, display, monitor
    ├── situation_impl_image.h    (7) image, font, color
    ├── situation_impl_renderer.h (8) GL + VK backends (~26K LOC)
    ├── situation_impl_vd.h       (9) virtual display compositing
    └── situation_impl_ctrl.h     (10) lifecycle, init/shutdown
```

---

## Implementation phases — verified state

### Phase 1 — Identify the boundary
- [x] Scanned `situation_impl.h` for first non-trivial function body
- [x] Documented line-number boundary (~2218 at the time)
- [x] Catalogued declarations vs implementations
- [x] Grey-zone helpers identified and moved to decl file

### Phase 2 — Extract embedded shaders
- [x] All `static const char*` shader strings moved to `situation_impl_decl.h`
- [x] Shader contract macros moved
- [x] Both backends build clean

### Phase 3 — Extract internal macros
- [x] `SIT_DEBUG_LOG`, `SIT_UNIFORM_MAP_INITIAL_CAPACITY` moved to decl file
- [x] Utility helpers `_SitGetMonotonicTimeNS` (line 60), `_SituationAssertMainThread` (line 77) in decl file
- [x] Forward declaration of `_SituationSetErrorFromCode` (line 50) in decl file
- [x] `sit_gs_main_thread_id` (line 53), `sit_gs_thread_id_set` (line 54),
  `sit_render_policy_state` (line 58) are static globals in decl file

### Phase 4 — Extract struct/typedef definitions
- [x] All slot types, pipeline types, GL/VK state structs moved to decl file
- [x] `SituationContext` struct (line 1680) in decl file
- [x] `#if defined(...)` backend guards preserved intact

### Phase 5 — Static globals
- [x] `sit_gs` and `sit_render` are **no longer raw static globals** — the architecture
  evolved: `SituationContext` owns both fields; a single `static SituationContext*
  _sit_current_context = NULL` (line 1691) is defined in decl file; `sit_gs` and
  `sit_render` are macros (`#define sit_gs (_sit_current_context->gs)`, line 1695–1696)
  preserving backward compatibility with all call sites
- [x] `sit_gs_main_thread_id`, `sit_gs_thread_id_set`, `sit_render_policy_state` are static
  variables defined in decl file (lines 53–58)

### Phase 6 — Forward declarations
- [x] `situation_impl_forward.h` exists as a dedicated file for cross-module forward decls
- [x] `situation_impl_renderer_fwd.h` exists for renderer-specific forward decls
- [x] Thread-entry forward decls (`_SituationWorkerEntry`, `_SituationRenderThreadEntry`,
  etc.) in decl file (lines 1664–1674)

### Phase 7 — Final cleanup
- [x] `situation_impl.h` is a pure orchestrator — 10 numbered `#include` sections, zero
  function bodies, zero typedef definitions
- [x] Include order documented in file header comment
- [x] `_SituationVulkanWaitFencePumpWindow*` fence helpers and `_SituationVulkanResignalFrameFence`
  live in `situation_impl_decl.h` (lines 1706–1796) — small inline Vulkan fence utilities that
  depend on structs defined earlier in the same file; this is the accepted grey zone

### Line count (current)
- [x] `situation_impl_decl.h`: ~1800 lines (leaner than original estimate; most structs
  are large but shader strings moved to `gpu/` embed files)
- [x] `situation_impl.h`: 64 lines (pure orchestrator)
- [x] `situation_impl_renderer.h`: ~26K lines (the renderer is the bulk; this was expected)

---

## Constraints — all honoured

- [x] Include order preserved: deps → decl → forward → impl modules
- [x] No circular dependencies
- [x] Platform `#ifdef` guards intact throughout
- [x] Zero functional changes — pure structural refactor
