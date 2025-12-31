# Version 2.4 Roadmap: Codebase Modularization

## 1. Executive Summary

This document outlines the strategic roadmap for version 2.4, focusing on modularizing the `situation` library's codebase while strictly adhering to the "Single Header API" philosophy. The goal is to separate the implementation logic (~30k lines) from the public API definition (~3k lines) to improve maintainability, without complicating the integration process for end-users.

**Selected Strategy:** **Monolithic Header, Modular Source (Unity Build)**.

*   **`situation.h`**: Remains the **sole** public API reference. It contains all typedefs, enums, and function prototypes. It contains **no implementation logic**.
*   **`situation_impl.c`** (formerly `situation.c`): A new root-level file serving as the "Unity Build" entry point. It includes the modular implementation files from the `src/` directory.
    *   *Rationale:* `situation.c` sounds like source for the API. `situation_impl.c` makes intent crystal clear.
*   **`src/`**: A new private directory housing the implementation modules (e.g., `sit_audio.c`, `sit_render.c`).

## 2. Architecture & Directory Structure

### 2.1 Proposed Layout
```
root/
├── situation.h           # [PUBLIC] Pure API Header. No code, just declarations.
├── sit_config.h          # [OPTIONAL] User overrides (allocators, debug flags).
├── situation_impl.c      # [PUBLIC] Implementation Entry Point.
└── src/                  # [PRIVATE] Internal Implementation Modules.
    ├── sit_common.h      # Internal shared macros/types (hidden from user).
    ├── sit_core.c        # Logging, Error Handling, Memory, Math helpers.
    ├── sit_platform.c    # Windowing, Input, System Info (GLFW glue).
    ├── sit_audio.c       # Audio Engine (Miniaudio wrapper).
    ├── sit_render.c      # Graphics Engine (OpenGL/Vulkan logic).
    ├── sit_fs.c          # Filesystem & Hot-Reloading.
    └── sit_utils.c       # String helpers, Hash maps, etc.
```

## 3. Execution Roadmap

### Phase 0: Baseline & Safety Net
*Goal: Ensure the current state is stable and reproducible before major surgery.*

- [ ] **Verify Test Environment**
    - [ ] Run `test_limits.c` and ensure it passes.
    - [ ] Run `test_async_io.c` and ensure it passes.
    - [ ] Verify `situation_dll.c` compiles successfully.
- [ ] **Snapshot**
    - [ ] Create a backup of `situation.h` to `situation.h.bak` for quick comparison.

### Phase 0.5: Pre-Split Refactoring
*Goal: Prepare the codebase for separation by enforcing namespace hygiene and physical grouping.*

- [ ] **Symbol Sanitization**
    - [ ] Rename internal `static` helper functions to avoid collisions in the Unity Build.
    - [ ] Convention: `_Sit[Module]_[Name]`.
        -   `_SitGetTextureSlot` -> `_SitRender_GetTextureSlot`
        -   `_SitAudioInitPool` -> `_SitAudio_InitPool`
        -   `_SituationDeferredDestroyBuffer` -> `_SitRender_DeferDestroyBuffer`
- [ ] **Physical Grouping**
    - [ ] Reorder the implementation block within `situation.h` to group functions by module.
    -   **Order:**
        1.  Common/Core (Allocators, Logging, Math)
        2.  Filesystem (needed by everything)
        3.  Platform/Windowing (needed by Render)
        4.  Input (Keyboard/Mouse/Joystick)
        5.  Render (OpenGL/Vulkan)
        6.  Audio
    - [ ] Insert delimiter comments (e.g., `// --- MODULE: AUDIO ---`) to clearly mark cut-points.

### Phase 1: Infrastructure Setup
*Goal: Create the physical structure without moving code yet.*

- [ ] **Create Directories**
    - [ ] Create `src/` directory in root.
- [ ] **Create Skeleton Files**
    - [ ] Create `sit_config.h` (Optional, template). Included first in `situation.h`.
    - [ ] Create `src/sit_common.h` with **Internal Guards**:
      ```c
      #ifndef SITUATION_INTERNAL
      #error "Internal headers should not be included directly"
      #endif
      ```
    - [ ] Create `src/sit_core.c` (Empty).
    - [ ] Create `src/sit_platform.c` (Empty).
    - [ ] Create `src/sit_audio.c` (Empty).
    - [ ] Create `src/sit_render.c` (Empty).
    - [ ] Create `src/sit_fs.c` (Empty).
- [ ] **Create The Bridge**
    - [ ] Create `situation_impl.c` with the following content:
      ```c
      #define SITUATION_IMPLEMENTATION_INTERNAL
      #define SITUATION_INTERNAL // Allows including internal headers
      #include "situation.h"
      #include "src/sit_common.h"
      // Modules will be included here later
      ```
    - [ ] Update `situation.h` to include `sit_config.h` at the top (if exists).

### Phase 2: The "Great Separation"
*Goal: Separate the API from the Implementation. This is the most critical phase.*

- [ ] **Extract Implementation**
    - [ ] Cut the entire `SITUATION_IMPLEMENTATION` block from `situation.h`.
    - [ ] Paste it into `situation_impl.c` (temporarily monolithic).
    - [ ] Verify `situation.h` contains ONLY:
        -   License / Comments.
        -   Includes `sit_config.h`.
        -   Configuration Macros (`SITUATION_USE_OPENGL`, etc.).
        -   Typedefs, Enums, Structs.
        -   Function Prototypes (`SITAPI`).
- [ ] **Extract Internal Shared State**
    - [ ] Identify internal structs (`_SituationGlobalState`, `_SituationRenderState`, `_SituationAudioState`).
    - [ ] Identify internal macros (`SIT_LOG`, `SIT_CHECK`, etc.).
    - [ ] Move these from `situation_impl.c` (or `situation.h` if they were leaked) to `src/sit_common.h`.
    - [ ] Ensure `src/sit_common.h` is included by `situation_impl.c`.

### Phase 3: Module Colonization
*Goal: Move code from the monolithic `situation_impl.c` into specific modules.*

- [ ] **Move Core Module (`sit_core.c`)**
    - [ ] Move: `SituationLog`, `SituationError`, `SIT_MALLOC`, `Vector` math helpers.
    - [ ] Verify compilation.
- [ ] **Move Platform Module (`sit_platform.c`)**
    - [ ] Move: `SituationInit`, `SituationShutdown`, `SituationPollInputEvents`, `SituationGetDeviceInfo`.
    - [ ] Move: GLFW callbacks and window creation logic.
    - [ ] Verify compilation.
- [ ] **Move Audio Module (`sit_audio.c`)**
    - [ ] Move: `SituationInitAudio`, `SituationPlaySound`, `SituationUpdateAudio`.
    - [ ] Move: Miniaudio backend implementation.
    - [ ] Verify compilation.
- [ ] **Move Filesystem Module (`sit_fs.c`)**
    - [ ] Move: `SituationLoadFile`, `SituationSaveFile`.
    - [ ] Move: Hot-Reloading logic (`Velocity`).
    - [ ] Verify compilation.
- [ ] **Move Render Module (`sit_render.c`)**
    - [ ] Move: `SituationCreateTexture`, `SituationDraw*`, `SituationSubmitJob`.
    - [ ] Move: OpenGL/Vulkan specific backend logic.
    - [ ] **Critical:** Ensure internal render state (`sit_render` macro) is accessible via `sit_common.h`.
    - [ ] Verify compilation.

### Phase 4: The Legacy Bridge & Polish
*Goal: Restore backward compatibility for users who rely on the single-header behavior.*

- [ ] **Restore Header-Only Behavior**
    - [ ] Add the following to the bottom of `situation.h`:
      ```c
      #ifdef SITUATION_IMPLEMENTATION
          #include "situation_impl.c"
      #endif
      ```
    - [ ] **Note:** This assumes `situation_impl.c` is in the include path.
- [ ] **Verify Includes**
    - [ ] Ensure `situation_impl.c` includes all `src/*.c` files in the correct dependency order:
      1. `sit_core.c`
      2. `sit_fs.c` (Core dep)
      3. `sit_platform.c` (Core dep)
      4. `sit_render.c` (Platform, FS dep)
      5. `sit_audio.c` (Core dep)

### Phase 5: Documentation & Validation
*Goal: Prove that nothing broke and users know how to build.*

- [ ] **Update README.md**
    - [ ] Document the Build Modes:
        -   **Header-Only (Legacy)**: `#define SITUATION_IMPLEMENTATION` before `#include "situation.h"`
        -   **Modular (Recommended)**: Add `situation_impl.c` + `src/*.c` to your build.
        -   **Static Library / Object (Advanced)**: Compile `situation_impl.c` to `situation.o` (or `libsituation.a`) and link it.
- [ ] **Test 1: Standard Build (Split)**
    - [ ] Compile a test file that adds `situation_impl.c` to the compiler sources and includes `situation.h`.
- [ ] **Test 2: Legacy Build (Header-Only)**
    - [ ] Compile a test file that defines `SITUATION_IMPLEMENTATION` and includes `situation.h`.
- [ ] **Test 3: DLL Build**
    - [ ] Compile `situation_dll.c` (Updated to use `situation_impl.c`).
- [ ] **Test 4: Static Object Workflow**
    - [ ] Compile `situation_impl.c` into an object file (e.g., `gcc -c situation_impl.c -o situation.o`).
    - [ ] Compile a test file (e.g., `test_limits.c`) *without* `SITUATION_IMPLEMENTATION`.
    - [ ] Link them together (`gcc test_limits.o situation.o -o test_bin`) and verify it runs.
- [ ] **Regression Check**
    - [ ] Run `test_limits.c`.
    - [ ] Run `test_async_io.c`.

## 4. Risks & Mitigations

| Risk | Mitigation |
| :--- | :--- |
| **Circular Dependencies** | Strictly enforce a hierarchy: `Core` < `FS` < `Platform` < `Render`. Use `sit_common.h` for shared types. |
| **Static Function Visibility** | Functions in `src/*.c` must be `static` or `SIT_PRIVATE` if they are internal helpers, to avoid symbol clashes in the unity build. |
| **Include Path Hell** | Users might not set the include path correctly for `src/`. **Solution:** The user only ever includes `situation.h` or compiles `situation_impl.c`. The internal `src/` includes are relative to `situation_impl.c` (`#include "src/..."`), so as long as the user has the folder structure, it works. |
| **Macro Leakage** | Ensure internal macros in `sit_common.h` are undefined at the end of `situation_impl.c` or strictly namespaced. |

## 5. Hardening & Robustness

To ensure the split is production-ready and resilient to future changes, we implement the following hardening measures:

### 5.1 Namespace Hygiene
In a Unity Build, multiple `.c` files are textually included into one compilation unit (`situation_impl.c`). This effectively merges their file scopes.
*   **Rule:** All internal functions, even if declared `static`, MUST have a unique prefix to prevent collisions or confusion during debugging/profiling.
*   **Format:** `_Sit[Module]_[FunctionName]`
    *   Example: `_SitRender_Init()` instead of `_SituationInitRenderer()`.
    *   Example: `_SitAudio_MixVoices()` instead of `_MixVoices()`.
*   **Verification:** Use `grep` or `nm` to flag any function starting with just `_` that doesn't follow the module pattern.

### 5.2 Verification Steps
Automated checks to run after the split:
1.  **Symbol Visibility Check:**
    Compile `situation_impl.c` as a shared object (`.so` / `.dll`). Use `nm -D` (Linux) or `dumpbin /EXPORTS` (Windows) to verify that **only** `SITAPI` symbols are exported. Any `_Sit...` helper visible in the export table is a bug (missing `static` or visibility attribute).
2.  **Preprocessed Diff:**
    Run `gcc -E situation.h` (original) and `gcc -E situation_impl.c` (new). Normalize whitespace and comments. The resulting C code stream must be functionally identical (aside from line numbers).

### 5.3 Legacy Bridge Robustness
The single-header workflow is a core promise of the library. We ensure backward compatibility via a precise bridge in `situation.h`:

```c
#ifdef SITUATION_IMPLEMENTATION
    #ifndef SITUATION_IMPL_INCLUDED
    #define SITUATION_IMPL_INCLUDED

    // Check if the user has the split files available in the expected relative path
    // Ideally, we just include the unity build file.
    // If the user hasn't set up the include paths for src/, situation_impl.c handles the relative lookup.

    #include "situation_impl.c"

    #endif
#endif
```
**Constraint:** This requires `situation_impl.c` to be in the include path or the same directory as `situation.h`.

### 5.4 Editor Support (Intellisense)
Split files often confuse IDEs (VS Code, CLion, Visual Studio) because independent `.c` files in `src/` might be missing context (defines, types) if analyzed in isolation.
*   **Solution:** Add a "Master Include" guard at the top of every `src/*.c` file:
    ```c
    // src/sit_render.c
    #ifndef SITUATION_IMPLEMENTATION_INTERNAL
    // This file is a module part of the Situation library.
    // It is not intended to be compiled directly.
    // Please include "situation_impl.c" instead.
    #ifdef __INTELLISENSE__
    #include "../situation_impl.c" // Trick Intellisense into seeing the full context
    #endif
    #endif
    ```
