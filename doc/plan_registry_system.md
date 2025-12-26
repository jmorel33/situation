# Architectural Blueprint: The Registry System (Generational Indices)

## 1. Executive Summary & Credibility Assessment

**Verdict:** The proposed "Grand Plan" is **Architecturally Sound** and represents the industry standard for high-performance resource management in C/C++ game engines.

### Why this is the correct path:
1.  **Safety (The "ABA" Problem):** Traditional pointer handles (`Structure*`) are dangerous. If memory is freed and re-allocated for a new object, a dangling pointer will modify the wrong object. Generational Indices (Index + Version) solve this mathematically. If the generation doesn't match, the handle is dead.
2.  **Stability (ABI):** Public structs become simple 16-byte "tickets". The internal data layout (Vulkan vs OpenGL fields) can change wildly without breaking binary compatibility for the user or requiring recompilation of dependent code (if using shared libs).
3.  **Performance (Cache Locality):** Resources are stored in contiguous arrays (Registries) rather than scattered heap allocations. This improves CPU cache hit rates during iteration (e.g., Hot-Reload checking or Shutdown cleanup).
4.  **Simplicity:** It eliminates the complexity of linked lists (`next`, `prev` pointers) for tracking resources. Cleanup becomes a simple `for` loop.

This pattern is widely used in engines like **Bitsquid/Stingray** (ID lookup tables), **The Machinery**, and modern **idTech**.

---

## 2. Implementation Specifications

### 2.1. Fixed Limits (The Constants)
We will define strict upper bounds to allow for static array allocation (or fixed-size heap allocation at startup). This simplifies memory management.

```c
// situation.h
#define SITUATION_MAX_TEXTURES          4096
#define SITUATION_MAX_BUFFERS           4096
#define SITUATION_MAX_MESHES            4096
#define SITUATION_MAX_SHADERS           1024
#define SITUATION_MAX_COMPUTE_PIPELINES 256
#define SITUATION_MAX_MODELS            1024
```

### 2.2. Unified Handle Definitions (Public API)
All resource handles in `situation.h` will follow the standard "Ticket" layout. This replaces the opaque padding arrays.

```c
// situation.h

// Already implemented (v2.3.x)
typedef struct { uint32_t slot_index; uint32_t generation; int width; int height; } SituationTexture;

// NEW: Standardized Handles
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
    size_t size_in_bytes;
    SituationBufferUsageFlags usage_flags;
} SituationBuffer;

typedef struct {
    uint32_t slot_index;
    uint32_t generation;
    int index_count;
    int vertex_count;
    size_t vertex_stride;
} SituationMesh;

typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationShader;

typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationComputePipeline;

typedef struct {
    uint32_t slot_index;
    uint32_t generation;
    int mesh_count;
    struct SituationModelMesh* meshes;
} SituationModel;
```

### 2.3. Internal Slot Structures (Implementation Detail)
Inside `SITUATION_IMPLEMENTATION`, we define the actual storage.

```c
// situation.h (Implementation Section)

typedef struct {
    bool is_active;
    uint32_t generation;

    // Hot-Reload Metadata
    char* vs_path;
    char* fs_path;
    long vs_mod_time;
    long fs_mod_time;

    // Backend Resources
#if defined(SITUATION_USE_VULKAN)
    VkPipeline pipeline;
    VkPipeline pipeline_legacy;
    VkPipelineLayout layout;
#elif defined(SITUATION_USE_OPENGL)
    GLuint program_id;
    struct _SituationUniformMap* uniform_map;
#endif
} _SituationShaderSlot;

typedef struct {
    bool is_active;
    uint32_t generation;

#if defined(SITUATION_USE_VULKAN)
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDescriptorSet descriptor_set;
    VkBufferUsageFlags vk_usage;
#elif defined(SITUATION_USE_OPENGL)
    GLuint buffer_id;
#endif
} _SituationBufferSlot;

// ... Similar structs for Mesh, ComputePipeline, Model ...
```

### 2.4. Render State Update
The global state container `_SituationRenderState` will host these registries.

```c
typedef struct {
    // ... Existing state ...

    // Registries (Heap allocated at Init to avoid stack overflow)
    _SituationTextureSlot* texture_registry; // [SITUATION_MAX_TEXTURES]
    _SituationBufferSlot*  buffer_registry;  // [SITUATION_MAX_BUFFERS]
    _SituationShaderSlot*  shader_registry;  // [SITUATION_MAX_SHADERS]
    _SituationMeshSlot*    mesh_registry;    // [SITUATION_MAX_MESHES]

    // ...
} _SituationRenderState;
```

---

## 3. Operations & Transitions

### 3.1. Allocation Strategy (Create)
1.  **Scan:** Iterate the registry array to find the first slot where `is_active == false`.
2.  **Alloc:** If found, mark `is_active = true`.
3.  **Gen:** Increment `generation`. If it wraps to 0, set to 1.
4.  **Init:** Create backend resources (Vulkan/GL).
5.  **Return:** Construct public Handle `{ index, generation }`.

*Optimization:* Maintain a `last_free_index` or a freelist stack if linear scanning becomes a perf bottleneck (unlikely for < 10k objects).

### 3.2. Lookup Strategy (Use)
Every API function (e.g., `SituationCmdBindPipeline`) validates the handle.

```c
_SituationShaderSlot* slot = &sit_render.shader_registry[handle.slot_index];
if (!slot->is_active || slot->generation != handle.generation) {
    // HANDLE IS DEAD OR INVALID
    return SITUATION_ERROR_RESOURCE_INVALID;
}
// Use slot->pipeline...
```

### 3.3. Deallocation Strategy (Destroy)
1.  **Lookup:** Validate handle.
2.  **Backend Destroy:** Call `vkDestroyPipeline`, `glDeleteProgram`, etc.
    *   *Note:* Use the existing "Graveyard" deferred destruction for Vulkan resources to ensure safety for in-flight frames.
3.  **Free:** Mark `is_active = false`.
4.  **Cleanup:** Free auxiliary memory (paths, uniform maps).

### 3.4. Cleanup Strategy (Shutdown)
Iterate the entire array. If `is_active == true`, force destroy and log a "Leak Warning". This replaces the old linked list traversal.

---

## 4. Transition Roadmap

1.  **Phase 1: Shaders & Compute**
    *   Implement Registry for `SituationShader` and `SituationComputePipeline`.
    *   Simplifies Hot-Reloading significantly.
2.  **Phase 2: Buffers & Meshes**
    *   Implement Registry for `SituationBuffer` and `SituationMesh`.
    *   Enables standard SSBO pulling.
3.  **Phase 3: Models**
    *   Implement Registry for `SituationModel`.
4.  **Phase 4: Cleanup**
    *   Remove all `_Situation...Node` linked list definitions.
    *   Remove `SituationCmdBindUniformBuffer` legacy paths that rely on pointers.

## 5. Hot-Reloading Integration ("Velocity")
The `SituationCheckHotReloads` function currently iterates a linked list. It will be refactored to iterate the active slots in the `shader_registry` and `texture_registry` arrays. This is faster (linear memory access) and safer (no pointer chasing).
