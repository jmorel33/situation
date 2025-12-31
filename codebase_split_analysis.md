# Codebase Split Analysis: "Unity Build" Refactoring Strategy

## 1. Executive Summary

This document analyzes the strategy to modularize the `situation` library's codebase while strictly adhering to the "Single Header API" philosophy. The goal is to separate the implementation logic (~30k lines) from the public API definition (~3k lines) to improve maintainability, without complicating the integration process for end-users.

**Selected Strategy:** **Monolithic Header, Modular Source (Unity Build)**.

*   **`situation.h`**: Remains the **sole** public API reference. It contains all typedefs, enums, and function prototypes. It contains **no implementation logic**.
*   **`situation.c`**: A new root-level file serving as the "Unity Build" entry point. It includes the modular implementation files from the `src/` directory.
*   **`src/`**: A new private directory housing the implementation modules (e.g., `sit_audio.c`, `sit_render.c`).

## 2. Architecture & Directory Structure

### 2.1 Proposed Layout
```
root/
├── situation.h           # [PUBLIC] Pure API Header. No code, just declarations.
├── situation.c           # [PUBLIC] Implementation Entry Point.
└── src/                  # [PRIVATE] Internal Implementation Modules.
    ├── sit_common.h      # Internal shared macros/types (hidden from user).
    ├── sit_core.c        # Logging, Error Handling, Memory, Math helpers.
    ├── sit_platform.c    # Windowing, Input, System Info (GLFW glue).
    ├── sit_audio.c       # Audio Engine (Miniaudio wrapper).
    ├── sit_render.c      # Graphics Engine (OpenGL/Vulkan logic).
    ├── sit_fs.c          # Filesystem & Hot-Reloading.
    └── sit_utils.c       # String helpers, Hash maps, etc.
```

### 2.2 The Roles

#### `situation.h` (The Contract)
*   **Purpose:** The single source of truth for the user.
*   **Content:** strictly `SITAPI` declarations, `typedef struct`, `enum`.
*   **Constraint:** Must be C11 compliant and header-only safe (no double definitions).

#### `situation.c` (The Bridge)
*   **Purpose:** Preserves the simplicity of building the library. The user adds *one file* to their build system.
*   **Content:**
    ```c
    // situation.c - Implementation Unity Build
    #define SITUATION_IMPLEMENTATION_INTERNAL // Guard to allow internal headers
    #include "situation.h"
    #include "src/sit_common.h"

    #include "src/sit_core.c"
    #include "src/sit_platform.c"
    #include "src/sit_audio.c"
    #include "src/sit_render.c"
    #include "src/sit_fs.c"
    ```

#### `src/*.c` (The Logic)
*   **Purpose:** Logical separation of concerns.
*   **Constraint:** These files are **not** standalone translation units. They are "header implementations" designed to be included by `situation.c`. This allows them to share internal static globals (like the global context `sit_gs`) without complex extern linking.

## 3. Non-Regression & Backward Compatibility

### 3.1 Legacy "Header-Only" Usage
Many users integrate the library using the STB-style macro in their `main.c`:
```c
#define SITUATION_IMPLEMENTATION
#include "situation.h"
```

To prevent breaking this workflow, `situation.h` will be updated to automatically include the implementation file if the macro is detected:

```c
// Bottom of situation.h
#ifdef SITUATION_IMPLEMENTATION
    #include "situation.c"
#endif
```

*   **Risk:** This requires `situation.c` to be in the include path.
*   **Mitigation:** Since `situation.c` is in the root (alongside `situation.h`), this works out-of-the-box for standard project structures.

### 3.2 Standard C Usage (Recommended)
Users can now simply add `situation.c` to their build sources (e.g., in CMake or Makefiles) and just `#include "situation.h"` in their headers. This is cleaner and speeds up incremental builds since the 30k lines of implementation are compiled once, not every time `main.c` changes.

## 4. Module Boundaries & Dependencies

Based on analysis of `situation.h` (v2.3.38):

1.  **Core Module** (`sit_core.c`)
    *   **Dependencies:** None.
    *   **Content:** `SituationError`, `SituationLog`, `SIT_MALLOC`, `Vector` math types.

2.  **Platform Module** (`sit_platform.c`)
    *   **Dependencies:** Core, GLFW.
    *   **Content:** Window creation, Input polling (`SituationPollInputEvents`), Device Info (`SituationGetDeviceInfo`).

3.  **Audio Module** (`sit_audio.c`)
    *   **Dependencies:** Core, Threading, Miniaudio.
    *   **Content:** `SituationSound`, `SituationPlayToneEx`, DSP effects.
    *   **Note:** Contains its own internal state `_SituationAudioState`.

4.  **Filesystem Module** (`sit_fs.c`)
    *   **Dependencies:** Core, Platform (for paths).
    *   **Content:** `SituationLoadFile`, Hot-Reloading (`Velocity` module).

5.  **Render Module** (`sit_render.c`)
    *   **Dependencies:** Core, Platform, Filesystem (for shaders).
    *   **Content:** `SituationCmd*`, Vulkan/OpenGL backends, Text rendering (`SituationFont`).
    *   **Complexity:** The "Font Sandwich" dependency (Text uses Render commands) is resolved because `sit_render.c` sees all declarations from `situation.h`.

## 5. Refactoring Plan

1.  **Preparation**: Create `src/` directory and `src/sit_common.h`.
2.  **Extraction**:
    *   Move internal macros/structs (like `_SituationGlobalState`) to `src/sit_common.h`.
    *   Cut the `SITUATION_IMPLEMENTATION` block from `situation.h`.
    *   Paste it into `situation.c`.
3.  **Modularization**:
    *   Iteratively move code sections from `situation.c` to `src/sit_*.c`.
    *   Use `git grep` to identify internal dependencies and move shared helpers to `sit_common.h` if needed.
4.  **Verification**:
    *   Compile `situation_dll.c` (which defines `SITUATION_IMPLEMENTATION`) to verify the unity build works.
    *   Verify `situation.h` is clean (declarations only).

## 6. Conclusion

This strategy fulfills the requirement of keeping `situation.h` as the clean API reference. It simplifies the user experience (drag-and-drop `situation.h` and `situation.c`) while providing the developer (you) with a modular, maintainable codebase.
