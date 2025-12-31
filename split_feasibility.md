# Situation SDK: Modularization Feasibility Study & Roadmap

This document outlines the strategy, phases, and actionables for decomposing the monolithic `situation.h` header into a modular, multi-header SDK structure.

## Goal
To refactor the ~32,000 line `situation.h` into a set of logical, maintainable, and loosely coupled header files (e.g., `situation_core.h`, `situation_render.h`, `situation_audio.h`) while preserving the "single-header library" convenience via a main umbrella header.

## 1. Analysis of Current Dependencies
The primary challenge is the tight coupling between subsystems.

*   **Core Types:** `Vector2`, `SituationError`, `SITAPI` macro, and `SITUATION_IMPLEMENTATION` logic are used everywhere.
*   **The "Font Sandwich":**
    *   `SituationFont` (Image Module) depends on `SituationTexture` (Renderer Module).
    *   The Renderer Module depends on `SituationFont` for `SituationCmdDrawText`.
    *   *Result:* Circular dependency between Image and Graphics modules.
*   **Audio & Threading:** Largely independent, but rely on `Vector2` (panning) and `SituationError`.
*   **Input:** Depends on Windowing types (GLFW) and Core vectors.

## 2. Proposed Split Phases

### Phase 1: The Core Foundation
**Goal:** Extract the "primitives" required by all other modules to prevent inclusion cycles.

*   **File:** `sit/situation_core.h`
*   **Content:**
    *   Version Macros & License.
    *   Platform detection (`_WIN32`, `__linux__`).
    *   `SITAPI` macro definition.
    *   `SituationError` enum.
    *   Logging functions/macros.
    *   Memory management macros (`SIT_MALLOC`, `SIT_FREE`).
    *   Basic Types: `Vector2`, `Vector3`, `Vector4`, `ColorRGBA`, `Rectangle`.
    *   Basic Callback typedefs.
*   **Actionables:**
    *   Ensure `SITUATION_IMPLEMENTATION` guards are respected (types vs implementation).
    *   Verify `SITAPI` visibility logic works when included by sub-headers.

### Phase 2: Leaf Modules (Low Dependency)
**Goal:** Move independent modules out.

*   **Files:**
    *   `sit/situation_filesystem.h` (Depends on Core).
    *   `sit/situation_utils.h` (Hash maps, string helpers).
*   **Actionables:**
    *   Verify these compile standalone when including just `situation_core.h`.

### Phase 3: Windowing & Input
**Goal:** Extract platform windowing and input handling.

*   **Files:**
    *   `sit/situation_window.h` (Window creation, monitor info).
    *   `sit/situation_input.h` (Keyboard, Mouse, Gamepad - depends on Windowing for context).
*   **Actionables:**
    *   Ensure `GLFW` includes are handled correctly (don't leak `GLFWwindow` type unless requested, or use forward decls).

### Phase 4: Audio Engine
**Goal:** Separate the audio subsystem.

*   **File:** `sit/situation_audio.h`
*   **Dependencies:** Core (Vectors, Errors), Threading (optional).
*   **Actionables:**
    *   Isolate `miniaudio.h` includes.
    *   Ensure audio thread logic remains encapsulated.

### Phase 5: The Rendering Split (Critical Path)
**Goal:** Break the circular dependency between Text/Image and Rendering.

*   **Step A: Type Extraction**
    *   Create `sit/situation_render_types.h`.
    *   Move `SituationTexture`, `SituationShader`, `SituationMesh`, `SituationFont` (struct definition only), and Enums here.
    *   *Why?* Both the Image module and Render module need these *types* defined before function signatures.

*   **Step B: The Renderer API**
    *   Create `sit/situation_render.h`.
    *   Includes `situation_render_types.h`.
    *   Contains `SituationCreateTexture`, `SituationCmdDrawMesh`, etc.

*   **Step C: The Image/Font Module**
    *   Create `sit/situation_image.h`.
    *   Includes `situation_render_types.h`.
    *   Contains `SituationLoadImage`, `SituationLoadFont`, `SituationBakeFontAtlas`.
    *   *Note:* `SituationCmdDrawText` logically belongs in `situation_render.h` (as a command), even if it uses `SituationFont`.

*   **Actionables:**
    *   Carefully split `SituationFont` definition from its "Methods".
    *   Resolve the `SituationCmdDrawText` dependency: It takes a `SituationFont` (from Image module) but is a Render command.
        *   *Solution:* Define `struct SituationFont` in `render_types.h`.

### Phase 6: The Umbrella Header
**Goal:** Reconstruct `situation.h` to maintain backward compatibility.

*   **File:** `situation.h` (Root)
*   **Content:**
    ```c
    #ifndef SITUATION_H
    #define SITUATION_H

    // Configuration flags checking...

    #include "sit/situation_core.h"
    #include "sit/situation_filesystem.h"
    #include "sit/situation_window.h"
    #include "sit/situation_input.h"
    #include "sit/situation_render_types.h"
    #include "sit/situation_image.h"
    #include "sit/situation_audio.h"
    #include "sit/situation_render.h"

    #ifdef SITUATION_IMPLEMENTATION
        // Include implementation files or define them here if still monolithic
        #include "sit/impl/situation_impl_core.c"
        // ...
    #endif

    #endif // SITUATION_H
    ```

## 3. Actionables for Review

1.  **Implementation Split Strategy:**
    *   Decide if `SITUATION_IMPLEMENTATION` will simply define *all* implementations (monolithic compilation unit), or if we allow granular implementations (e.g., `#define SITUATION_IMPLEMENTATION_AUDIO`).
    *   *Recommendation:* Keep it monolithic for simplicity first (`SITUATION_IMPLEMENTATION` builds everything included).

2.  **Include Guard Naming:**
    *   Standardize on `SITUATION_MODULE_NAME_H`.

3.  **C++ Compatibility:**
    *   Ensure every new header file has `extern "C" {` guards.

4.  **Documentation:**
    *   Ensure Doxygen comments move with the function declarations.
    *   Update `README.md` or `situation_api.md` to reflect the new structure if exposed to users (or keep it hidden behind the umbrella header).

5.  **Verification Test:**
    *   Create a test file that includes *only* `sit/situation_core.h` and verifies basic types work.
    *   Create a test that includes *only* `sit/situation_audio.h` (plus core) to verify modularity.

## 4. Risk Assessment

*   **Circular Inclusion:** High risk in the Render/Image/Font nexus. Mitigated by `render_types.h`.
*   **Macro Leakage:** Internal macros used across modules (like `SIT_LOG`) must be public in `situation_core.h` or duplicated.
*   **Build Breakage:** Users relying on specific include orders (unlikely with header guards) or undocumented internal types might break.

## 5. Timeline Estimate

*   **Phases 1-3:** Low Complexity (1-2 hours).
*   **Phase 4:** Low Complexity (1 hour).
*   **Phase 5 (Render):** High Complexity (3-4 hours) due to careful type extraction and "Font Sandwich" resolution.
*   **Phase 6:** Low Complexity (30 mins).
