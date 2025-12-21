# Situation 2.3.x Roadmap: Path to AAA

To take Situation from "Production-Ready" to "AAA Worthy", the focus must shift from **Stability** (not crashing) to **Throughput** (processing massive amounts of data with zero CPU overhead).

AAA engines (like Decima, idTech, or Unreal 5) are defined by one principle: **The CPU should only submit work, never manage it.**

This document outlines the roadmap to the big leagues.

---

## 1. The "Bindless" Revolution (The GPU as a Pointer Machine)

Currently, `SituationCmdBindTexture` maps a texture to a slot. This is the "Old Way" (pre-2016).

### The AAA Way
The GPU has access to every texture at once via a single massive descriptor array (Descriptor Indexing).

### The Goal
*   **Stop binding textures.** Pass a `uint32_t textureID` to the shader via a UBO or Push Constant.
*   The shader executes: `color = texture(global_textures[textureID], uv)`.
*   **Impact:** Render an entire scene with thousands of unique materials in a single draw call.

### Current Status
*   **Partially Supported:** The feature flags `SIT_FEATURE_BINDLESS_TEXTURES` and `SIT_FEATURE_BINDLESS_BUFFERS` are defined in `SituationRenderFeature`.
*   **API availability:**
    *   `SituationGetBufferDeviceAddress` is implemented for Vulkan/GL.
    *   `SituationGetTextureHandle` exists for OpenGL.
*   **Gap:** The high-level command buffer API still relies on `SituationCmdBindTextureSet` and `SituationCmdBindSampledTexture`, which enforce the slot-based model. A purely index-based workflow is not yet exposed to the user.

### Strategy
1.  Refactor the internal `DescriptorSet` manager to maintain a global "Bindless Array" that grows dynamically.
2.  Update the shader compiler/preprocessor to support `extension GL_EXT_nonuniform_qualifier` automatically.
3.  Deprecate `SituationCmdBindTexture` in favor of passing texture indices via `SituationCmdSetPushConstant`.

---

## 2. GPU-Driven Rendering (Indirect Draw)

Currently, the CPU iterates over objects and calls `SituationCmdDraw` for each one. This bottlenecks the CPU at approximately 5,000-10,000 objects.

### The AAA Way
The CPU uploads a buffer of structs (mesh ID, transform, material ID).

### The Goal
*   Implement `SituationCmdDrawIndirect`.
*   The CPU records **one command**: `vkCmdDrawIndexedIndirect`.
*   The GPU reads the buffer and executes thousands of draws instantly.
*   **Next Level:** Use a Compute Shader to cull objects (frustum/occlusion) and write the Indirect Buffer. The CPU doesn't even know what is being drawn.

### Current Status
*   **Not Implemented.**
*   **Foundation Exists:** `SIT_FEATURE_DRAW_INDIRECT_COUNT` and `SIT_FEATURE_MULTI_DRAW_INDIRECT` feature flags are present. `SITUATION_BUFFER_USAGE_INDIRECT_BUFFER` is defined.
*   **Gap:** No public API exists to issue indirect draw commands (`SituationCmdDrawIndirect` is missing).

### Strategy
1.  Implement `SituationCmdDrawIndirect` and `SituationCmdDrawIndexedIndirect` in the backend.
2.  Create a strict struct layout definition for the indirect command structure (matching Vulkan/GL standards).
3.  Create an example demonstrating "Compute Culling -> Indirect Draw" pipeline.

---

## 3. The Render Graph (Frame Graph)

Currently, synchronization relies on manual calls to `SituationCmdPipelineBarrier` and `SituationCmdBeginRenderPass`. This is brittle; reordering passes can break synchronization.

### The AAA Way
You describe **what** you want to do, not **how**.
*   "Pass A writes to Texture X."
*   "Pass B reads Texture X."

### The Goal
*   Build a **Render Graph** system.
*   The library automatically calculates the optimal barriers.
*   **Memory Aliasing:** Automatically reuse memory (e.g., reuse Texture X's memory for Texture Y if they don't overlap in time), saving gigabytes of VRAM.

### Current Status
*   **Not Implemented.**
*   **Current State:** Synchronization is fully manual via `SituationCmdPipelineBarrier`.

### Strategy
1.  Design `SituationRenderGraph` and `SituationRenderPassNode` structs.
2.  Implement a topological sort and barrier solver.
3.  (Phase 2) Implement a Transient Resource Allocator for memory aliasing.

---

## 4. Asset Baking (The Pipeline)

Currently, `.png` and `.gltf` files are loaded at runtime. This is slow and memory-inefficient.

### The AAA Way
Assets are "cooked" offline into binary formats that match the GPU's internal layout.

### The Goal
*   Create a **"Situation Cooker"** tool.
*   **Images:** Convert PNG -> BC7/ASTC (Block Compressed). The GPU reads these directly; no decoding to RAM required.
*   **Models:** Convert GLTF -> Flat Binary Mesh. No parsing, just `fread` and upload.

### Current Status
*   **Not Implemented.**
*   **Current State:** `SituationLoadModel` parses GLTF at runtime. `SituationLoadTexture` decodes images at runtime.

### Strategy
1.  Develop a standalone CLI tool (`situation-cook`).
2.  Integrate a texture compressor (e.g., `ispc_texcomp` or `stb_dxt`).
3.  Define a `.sita` (Situation Asset) binary header format for fast loading.

---

## 5. Advanced Profiling (Tracy Integration)

Currently, `SituationGetFPS` acts as a speedometer. You need an X-Ray machine.

### The AAA Way
You need to see exactly how long the "Shadow Pass" took on the GPU vs. the CPU.

### The Goal
*   Integrate **Tracy** or a similar profiler.
*   Add `SituationProfileZone("Physics")` macros.
*   Implement **GPU Timestamps** (query pools) to visualize GPU work on the same timeline.

### Current Status
*   **Basic Metrics Only.**
*   `SituationGetRenderLatencyStats` and `SituationDrawMetricsOverlay` provide high-level stats.
*   No deep instrumentation or external profiler support.

### Strategy
1.  Add optional dependency on `tracy`.
2.  Wrap Tracy C-API macros behind `SIT_PROFILE_ZONE`.
3.  Implement Vulkan Timestamp Queries and map them to profiling zones.

---

## 6. Timeline Semaphores

Currently, synchronization is frame-to-frame (binary semaphores).

### The AAA Way
Granular synchronization.

### The Goal
*   Move to **Timeline Semaphores**.
*   This allows the **Compute Queue** to run physics asynchronously while the **Graphics Queue** renders the shadow map, synchronizing only at the exact moment data is needed.

### Current Status
*   **Not Implemented.**
*   The backend currently uses standard `VkSemaphore` (binary).

### Strategy
1.  Verify Vulkan 1.2+ requirement (Timeline Semaphores are core in 1.2).
2.  Refactor `_SituationRenderState` to use `uint64_t` timeline values instead of fence arrays for frame tracking.

---

## Immediate "Next Step" for v2.4

**Priority: Bindless Textures.**

It fundamentally changes how you write shaders and engine code. It removes the concept of "Slots" and "Bindings" for assets, which is the biggest shackle holding back performance in modern APIs.

You have the architecture. Now give it the power.
