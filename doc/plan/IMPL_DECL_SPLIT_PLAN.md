# situation_impl_decl.h Extraction Plan

**Date:** 2026-05-04  
**Status:** Complete (v2.4.5)  
**Prerequisite:** Threading extraction (done), echo dependency removal (done)

## Goal

Extract all internal type definitions, struct declarations, static globals, macros, forward declarations, and embedded data (shaders) from `situation_impl.h` into a new `sit/situation_impl_decl.h`. This leaves `situation_impl.h` containing only function implementations.

## Current Architecture

```
situation.h
├── sit/situation_api.h              (public API: types, prototypes, enums)
├── sit/situation_impl.h             (~29,000 lines — everything)
│   ├── miniaudio implementation
│   ├── glad.c / vulkan includes
│   ├── internal structs & typedefs
│   ├── static globals (sit_gs, sit_render)
│   ├── macros & helpers
│   ├── forward declarations
│   ├── embedded shaders
│   ├── function implementations
│   └── #include "situation_impl_threading.h"
└── sit/situation_impl_audio.h       (audio subsystem)
```

## Target Architecture

```
situation.h (v2.4.5)
├── sit/situation_api.h                  (public API: types, prototypes, enums)
├── sit/situation_impl.h                 (~28,000 lines — function implementations)
│   ├── #include "situation_impl_deps.h"     (third-party: STB, miniaudio, glad, VMA)
│   ├── #include "situation_impl_decl.h"     (types, structs, globals, shaders)
│   ├── #include "situation_impl_forward.h"  (forward declarations)
│   ├── #include "situation_impl_threading.h" (thread pool & job system)
│   └── function implementations
└── sit/situation_impl_audio.h           (audio subsystem)
```

---

## Phase 1: Identify the Boundary

**Objective:** Determine the exact line where "declarations/types/globals" end and "function implementations" begin in `situation_impl.h`.

**Actions:**
- [x] Scan `situation_impl.h` for the first non-trivial function body (not inline/one-liner)
- [x] Document the line number boundary
- [x] Catalog what's above (types, globals, macros, shaders) vs below (functions)
- [ ] Flag any items that straddle the boundary (e.g., inline helpers used by both)

**Findings:**
- **Boundary line: ~2218** (first real function implementations: GL ring buffer helpers)
- Lines 1–2212: declarations zone
  - Lines 1–69: Header comment, platform includes, preprocessor
  - Lines 70–435: Miniaudio impl, glad.c, early macros, error helpers
  - Lines 370, 386: Two small inline helpers (`_SitGetMonotonicTimeNS`, `_SituationAssertMainThread`)
  - Lines 511–1722: All internal struct/typedef definitions (the bulk)
  - Lines 1741–1765: `SituationContext` struct
  - Lines 1766–1821: Forward declarations of internal static functions
  - Lines 1822–2212: Shader contract macros + embedded shader strings
- Lines 2218+: Function implementations (GL/VK helpers, then the main API functions)

**Grey zone items (small helpers that straddle):**
- `_SitGetMonotonicTimeNS` (line 370) — 10-line time helper, effectively a utility
- `_SituationAssertMainThread` (line 386) — 8-line debug assert
- `_SituationHashRenderPassKey` (line 797) — inline hash function
- `_SituationClampf`, `_SituationLerpf`, etc. (lines 2425–2428) — one-liner math helpers

These all belong in the decl file (they're utilities/helpers, not feature implementations).

**Verification:**
- [x] Line-count breakdown produced: types, globals, macros, shaders, forward decls, functions
- [ ] No function body exists above the boundary (a few small helpers do — acceptable, they move to decl)
- [ ] No type/struct definition exists below the boundary (except local to a function)

---

## Phase 2: Extract Embedded Shaders

**Objective:** Move all embedded shader source strings into `situation_impl_decl.h` first — they're pure data, zero dependencies on other code, and easy to verify.

**Actions:**
- [x] Identify all `static const char*` shader strings in `situation_impl.h`
- [x] Create `sit/situation_impl_decl.h` with header comment and include guard
- [x] Move shader strings (+ shader contract macros) into the new file
- [x] Add `#include "situation_impl_decl.h"` in `situation_impl.h` (after miniaudio/glad)
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes
- [x] Shader strings are no longer in `situation_impl.h`
- [x] No duplicate symbol errors

---

## Phase 3: Extract Internal Macros

**Objective:** Move internal-only macros (`SIT_DEBUG_LOG`, `SIT_UNIFORM_MAP_INITIAL_CAPACITY`) and associated utility helpers into the decl file.

**Actions:**
- [x] Identify all `#define` blocks that are implementation-internal (not in `situation_api.h`)
- [x] Move `SIT_DEBUG_LOG` macro into `situation_impl_decl.h`
- [x] Move `SIT_UNIFORM_MAP_INITIAL_CAPACITY` into `situation_impl_decl.h`
- [x] Move associated globals (`sit_gs_main_thread_id`, `sit_gs_thread_id_set`, `sit_render_policy_state`)
- [x] Move utility helpers (`_SitGetMonotonicTimeNS`, `_SituationAssertMainThread`)
- [x] Move forward declaration of `_SituationSetErrorFromCode`
- [x] Build both backends

**Note:** `SIT_ASSERT_MAIN_THREAD`, `SIT_MALLOC`, `SIT_FREE`, `SIT_CALLOC` are already in `situation_api.h` (public). No action needed for those.

**Verification:**
- [x] `build_situation.bat all` passes
- [x] Grep confirms no internal macro definitions remain in `situation_impl.h` (only usages)

---

## Phase 4: Extract Internal Struct/Typedef Definitions

**Objective:** Move all internal struct and typedef definitions into the decl file. This is the largest and most sensitive phase.

**Actions:**
- [x] Move slot types (`_SituationSoundSlot`, `_SituationMeshSlot`, `_SituationBufferSlot`, `_SituationTextureSlot`)
- [x] Move pipeline types (`_SituationComputePipeline`, `_SituationSpirvBlob`)
- [x] Move backend state (`_SituationGLState`, Vulkan state structs)
- [x] Move input state (joystick, keyboard, mouse structs)
- [x] Move the global state container (`_SituationGlobalStateContainer`)
- [x] Move the render state container (`_SituationRenderState`)
- [x] Move `SituationContext` struct and context macros (`sit_gs`, `sit_render`, etc.)
- [x] Preserve all `#if defined(...)` platform/backend guards as-is
- [x] Ensure include order: macros → structs → shaders
- [x] Fix include placement (moved before forward declarations that reference struct types)
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes
- [x] No "unknown type" errors
- [x] No "incomplete type" errors
- [ ] Grep confirms no `typedef struct` remains in `situation_impl.h` (except function-local)

---

## Phase 5: Extract Static Globals

**Objective:** Move static global variable definitions into the decl file.

**Actions:**
- [ ] Move `static _SituationGlobalStateContainer sit_gs = {0};`
- [ ] Move `static _SituationRenderState sit_render = {0};`
- [ ] Move `static thrd_t sit_gs_main_thread_id;`
- [ ] Move `static bool sit_gs_thread_id_set;`
- [ ] Move any other file-scope `static` variables that aren't function-local
- [ ] Place them after the struct definitions in the decl file
- [ ] Build both backends

**Verification:**
- [ ] `build_situation.bat all` passes
- [ ] No "undeclared identifier" errors for `sit_gs` or `sit_render`
- [ ] No duplicate definition warnings

---

## Phase 6: Extract Forward Declarations

**Objective:** Move all `static` function forward declarations into the decl file.

**Actions:**
- [ ] Identify all forward-declared `static` function prototypes (lines ending with `;`, no body)
- [ ] Move them into the decl file (after globals)
- [ ] Build both backends

**Verification:**
- [ ] `build_situation.bat all` passes
- [ ] No "implicit declaration" warnings
- [ ] Forward decls in decl file match actual function signatures in impl file

---

## Phase 7: Final Cleanup & Validation

**Objective:** Ensure the split is clean, documented, and the include is in the right place.

**Actions:**
- [ ] Verify the include placement in `situation_impl.h` is correct:
  ```c
  #define MINIAUDIO_IMPLEMENTATION
  #include <miniaudio.h>
  // ... glad.c / vulkan ...
  #include "situation_impl_decl.h"
  // ... function implementations ...
  #include "situation_impl_threading.h"
  ```
- [ ] Remove any dead/commented-out code exposed by the split
- [ ] Run a full clean build: `build_situation.bat clean && build_situation.bat all`
- [ ] Spot-check a few examples compile against the library

**Verification:**
- [ ] Clean build from scratch passes (both backends)
- [ ] `situation_impl.h` contains only function bodies
- [ ] `situation_impl_decl.h` contains only types, globals, macros, data, forward decls
- [ ] No warnings beyond the existing tinycthread redefinition (known benign)
- [ ] Line counts match expectations:
  - [ ] `situation_impl_decl.h`: ~7,000–9,000 lines
  - [ ] `situation_impl.h`: ~20,000–22,000 lines

---

## Constraints & Rules

- **Include order is sacred.** `situation_impl_decl.h` MUST come after miniaudio and glad/vulkan because structs reference `ma_delay`, `VkPipeline`, `GLuint`, etc.
- **No circular deps.** The decl file contains no function bodies and no includes of other impl files.
- **Platform ifdefs stay intact.** `#if defined(SITUATION_USE_VULKAN)` branches move as-is.
- **One phase at a time.** Each phase ends with a passing build. Never proceed to the next phase with a broken build.
- **No functional changes.** This is a pure structural refactor. Zero behavior changes.

---

## Risk Mitigation

- **If a phase breaks the build:** Revert that phase, investigate the dependency, and split the problematic item differently (e.g., keep it in impl if it has a forward-reference cycle).
- **If inline helpers are used by both decl-level code and functions:** Keep them in the decl file (they're effectively part of the type system).
- **If the boundary isn't clean:** Accept a small "grey zone" of helpers that live at the top of `situation_impl.h` right after the include of `situation_impl_decl.h`.
