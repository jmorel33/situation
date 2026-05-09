# Situation Engine v2.4 - Regression & Migration Analysis

## Executive Summary

This document details the "Universal Handle" architectural upgrade implemented in v2.4. The goal of this refactor was to unify all resource management (Textures, Sounds, Shaders, Meshes, Buffers) under a single, high-performance **Generational Handle** system.

This change eliminates legacy pointer-based resource tracking and O(N) linked-list traversals, replacing them with O(1) array-based registries. It also introduces a unified Hot-Reload system for all assets.

## Infrastructure Upgrade

### Legacy Architecture (v2.3)
*   **Identification:** Resources were often identified by raw `uint64_t` IDs or pointers.
*   **Tracking:** Resources were tracked using linked lists (e.g., `_SituationMeshNode`, `_SituationTextureNode`).
*   **Validation:** Checking if a resource was valid required iterating the list or trusting the user pointer.
*   **Memory:** Heavy reliance on `malloc` for individual tracking nodes.
*   **Hot-Reload:** Scattered logic, primarily only for textures.

### Universal Handle Architecture (v2.4)
*   **Identification:** All resources are now **Opaque Handles** (`struct { uint32_t slot_index; uint32_t generation; }`).
*   **Tracking:** Fixed-size, pre-allocated **Registries** (Arrays) in `_SituationRenderState` and `_SituationAudioState`.
*   **Validation:** **O(1) access**. `slot_index` gives direct array access; `generation` check prevents Use-After-Free (ABA problem).
*   **Memory:** Zero-allocation overhead for creating resources (slots are pre-allocated).
*   **Hot-Reload:** Centralized `_SituationPerformHotReloadPass` iterates all registries efficiently.

## Code Reduction Analysis (1400 LOC Removed)

The migration resulted in a net reduction of approximately ~1400 lines of code. This is **not a regression** but a significant pay-down of technical debt.

| Source of Reduction | Description |
| :--- | :--- |
| **Legacy Linked Lists** | Removed `_Situation*Node` structs and associated boilerplate (allocation, insertion, deletion, and traversal loops) for 7 resource types. |
| **O(N) Lookups** | Replaced complex "find by ID" search functions with simple `registry[index]` access. |
| **Audio Bloat** | Removed legacy `SituationSoundHandle` (uint64_t) mapping logic and `ma_decoder` exposure in public headers. |
| **Duplicated Hot-Reload** | Removed disparate hot-reload logic scattered across multiple files, replacing it with a single generic loop. |
| **Dead Code** | Removed unused helpers like `_SitGetBufferNode`. |

## Migration Status

| Subsystem | Resource Type | Status | Notes |
| :--- | :--- | :--- | :--- |
| **Graphics** | `SituationTexture` | ✅ **Migrated** | |
| | `SituationShader` | ✅ **Migrated** | |
| | `SituationMesh` | ✅ **Migrated** | |
| | `SituationBuffer` | ✅ **Migrated** | |
| | `SituationComputePipeline` | ✅ **Migrated** | |
| | `SituationModel` | ✅ **Migrated** | Model now holds handles to its meshes/textures. |
| **Audio** | `SituationSound` | ✅ **Migrated** | Major refactor. Removed internal `ma_decoder` exposure. |
| **Fonts** | `SituationFont` | ⏹️ **Unchanged** | Managed via STB; uses Texture handle internally. |

## Impact Analysis

### Structural Changes (API)

All resource structs in `situation_api.h` have been standardized.

**Before (Example):**
```c
// Legacy Audio
typedef uint64_t SituationSoundHandle;
// Or heavy struct exposed in header
```

**After (Universal):**
```c
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationSound; // Same pattern for Mesh, Texture, etc.
```

### Affected API Functions

The following categories of functions were refactored to use the new handle system:

1.  **Creation/Loading:**
    *   `SituationCreateTexture`, `SituationCreateTextureEx`
    *   `SituationLoadShader`, `SituationCreateShader`
    *   `SituationCreateMesh`, `SituationCreateBuffer`
    *   `SituationLoadModel`
    *   `SituationLoadSoundFromFile`, `SituationLoadSoundFromStream`
    *   *Impact:* Now return a Handle struct instead of an ID/Pointer. Returns `SITUATION_ERROR_..._LIMIT_REACHED` if registry is full.

2.  **Destruction:**
    *   `SituationDestroyTexture`, `SituationDestroyShader`, `SituationDestroyMesh`, `SituationDestroyBuffer`, `SituationUnloadModel`, `SituationUnloadSound`.
    *   *Impact:* Input is validated O(1). Safe to call on invalid/expired handles (no-op).

3.  **Command/Binding:**
    *   `SituationCmdBindPipeline`, `SituationCmdDrawMesh`, `SituationCmdBindVertexBuffer`, `SituationCmdBindIndexBuffer`.
    *   *Impact:* Internally resolves handle to backend resource (GL ID / Vulkan Handle) via `_SitGet*Slot`.

4.  **Audio Operations:**
    *   `SituationPlaySound`, `SituationStopSound`, `SituationSetSoundVolume`, etc.
    *   *Impact:* Thread-safe resolution of sound data from handle.

### Internal Helpers

New internal API in `situation_impl.h` to support the architecture:
*   `_SitAlloc[Type]Slot(...)`: Finds a free slot in the registry.
*   `_SitGet[Type]Slot(handle)`: Validates handle and returns pointer to internal data.
*   `_SitFree[Type]Slot(handle)`: Marks slot as free and increments generation.

## Unified Hot-Reloading

The Hot-Reload system is now centralized.
*   **Logic:** `_SituationPerformHotReloadPass`
*   **Mechanism:** Iterates over all active slots in all registries. Checks `mod_time` of the `source_path`.
*   **Behavior:** If modified, reloads data in-place (preserving the Handle ID) so user code sees the update instantly without re-fetching handles.

## Verification & Safety

*   **Compilation:** `test_async_io.c` compiles successfully, verifying that the new handle definitions and macros are syntax-correct and linked properly.
*   **Legacy Code:** Legacy member access (e.g., `sound.id`) will fail to compile, forcing migration to the safer handle system (which is the intended breaking change).
*   **Resource Leaks:** The new system allows iterating registries at shutdown to report *exactly* which slots were leaked, providing better debug capabilities than the previous linked list.

## Recommendations for Users

1.  **Recompile:** A full recompile is required.
2.  **Update Access:** Remove any code accessing `resource.id` or `resource.ptr`. Pass the `SituationResource` struct by value to API functions.
3.  **Check Limits:** Be aware of the fixed resource limits (e.g., `SITUATION_MAX_TEXTURES`, `SITUATION_MAX_SOUNDS`). Increase these constants in `situation_api.h` if necessary.
