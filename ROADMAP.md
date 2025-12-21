# Situation Roadmap: Path to AAA & Beyond

The Situation SDK is an evolving project. To take Situation from "Production-Ready" to "AAA Worthy", the focus must shift from **Stability** (not crashing) to **Throughput** (processing massive amounts of data with zero CPU overhead) and **Reach** (Web & Ecosystem).

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
The high-level command buffer API still relies on `SituationCmdBindTextureSet` and `SituationCmdBindSampledTexture`, which enforce the slot-based model. A purely index-based workflow is not yet exposed to the user.

### Tasks
- [x] **Feature Flags:** `SIT_FEATURE_BINDLESS_TEXTURES` and `SIT_FEATURE_BINDLESS_BUFFERS` defined.
- [x] **API (Buffer):** `SituationGetBufferDeviceAddress` implemented (Vulkan/GL).
- [x] **API (Texture - GL):** `SituationGetTextureHandle` implemented for OpenGL.
- [ ] **API (Texture - VK):** Implement `SituationGetTextureHandle` for Vulkan (requires descriptor indexing).
- [ ] **Internal Manager:** Refactor `DescriptorSet` manager to maintain a global "Bindless Array".
- [ ] **Shader Compiler:** Update compiler to support `GL_EXT_nonuniform_qualifier` automatically.
- [ ] **Deprecation:** Deprecate `SituationCmdBindTexture` in favor of passing texture indices.

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

### Tasks
- [x] **Foundation:** `SIT_FEATURE_DRAW_INDIRECT_COUNT` / `SIT_FEATURE_MULTI_DRAW_INDIRECT` flags defined.
- [x] **Buffer Usage:** `SITUATION_BUFFER_USAGE_INDIRECT_BUFFER` defined.
- [ ] **API:** Implement `SituationCmdDrawIndirect` and `SituationCmdDrawIndexedIndirect`.
- [ ] **Struct Layout:** Define strict C struct layout for indirect commands matching Vulkan/GL standards.
- [ ] **Example:** Create "Compute Culling -> Indirect Draw" example.

---

## 3. The Render Graph (Frame Graph)

Currently, synchronization relies on manual calls to `SituationCmdPipelineBarrier` and `SituationCmdBeginRenderPass`. This is brittle; reordering passes can break synchronization.

### The AAA Way
You describe **what** you want to do ("Pass A writes to Texture X", "Pass B reads Texture X"), not **how**.

### The Goal
*   Build a **Render Graph** system that automatically calculates optimal barriers.
*   **Memory Aliasing:** Automatically reuse memory (e.g., reuse Texture X's memory for Texture Y if they don't overlap in time), saving gigabytes of VRAM.

### Tasks
- [ ] **Design:** Design `SituationRenderGraph` and `SituationRenderPassNode` structs.
- [ ] **Solver:** Implement topological sort and barrier solver.
- [ ] **Aliasing:** Implement Transient Resource Allocator for memory aliasing (Phase 2).

---

## 4. Asset Baking (The Pipeline)

Currently, `.png` and `.gltf` files are loaded at runtime. This is slow and memory-inefficient.

### The AAA Way
Assets are "cooked" offline into binary formats that match the GPU's internal layout.

### The Goal
*   Create a **"Situation Cooker"** tool.
*   **Images:** Convert PNG -> BC7/ASTC (Block Compressed). The GPU reads these directly; no decoding to RAM required.
*   **Models:** Convert GLTF -> Flat Binary Mesh. No parsing, just `fread` and upload.

### Tasks
- [ ] **CLI Tool:** Develop a standalone CLI tool `situation-cook`.
- [ ] **Compression:** Integrate texture compressor (e.g., `ispc_texcomp` or `stb_dxt`).
- [ ] **Binary Format:** Define `.sita` (Situation Asset) binary header format for fast loading.

---

## 5. Advanced Profiling (Tracy Integration)

Currently, `SituationGetFPS` acts as a speedometer. You need an X-Ray machine.

### The AAA Way
You need to see exactly how long the "Shadow Pass" took on the GPU vs. the CPU.

### The Goal
*   Integrate **Tracy** or a similar profiler.
*   Add `SituationProfileZone("Physics")` macros.
*   Implement **GPU Timestamps** (query pools) to visualize GPU work on the same timeline.

### Tasks
- [ ] **Dependency:** Add optional dependency on `tracy`.
- [ ] **Macros:** Wrap Tracy C-API macros behind `SIT_PROFILE_ZONE`.
- [ ] **GPU Timestamps:** Implement Vulkan Timestamp Queries and map them to profiling zones.

---

## 6. Timeline Semaphores

Currently, synchronization is frame-to-frame (binary semaphores).

### The AAA Way
Granular synchronization allows the **Compute Queue** to run physics asynchronously while the **Graphics Queue** renders the shadow map, synchronizing only at the exact moment data is needed.

### The Goal
*   Move to **Timeline Semaphores**.
*   Refactor synchronization to be event-based rather than frame-based.

### Tasks
- [ ] **Verification:** Verify Vulkan 1.2+ requirements (Timeline Semaphores are core in 1.2).
- [ ] **Refactor:** Update `_SituationRenderState` to use `uint64_t` timeline values instead of fence arrays for frame tracking.

---

## 7. Async I/O (Momentum v2.4)

Blocking file I/O causes frame spikes. We need a dedicated I/O highway.

### The Goal
*   Dedicated I/O queues for non-blocking asset streaming.
*   Lock-free audio streaming.

### Tasks
- [ ] **I/O Queue:** Implement a dedicated thread/queue for file operations.
- [ ] **Streaming:** Update `SituationLoadTexture` to support async streaming.
- [ ] **Audio:** Implement lock-free job system for audio processing.

---

## 8. Virtual Mounts

Loading loose files is messy. Packed archives are cleaner and faster.

### The Goal
*   Support for loading assets from packed archives (.zip, .pak).
*   Virtual file system abstraction.

### Tasks
- [ ] **Archive Support:** Implement `.zip` / `.pak` reader.
- [ ] **VFS:** Abstract file paths to virtual mounts (e.g., `/textures/` -> `assets.pak`).

---

## 9. Web & Reach (v2.5+)

Bringing the Titanium-grade experience to the browser.

### The Goal
*   **Emscripten Support:** Full WASM compilation target.
*   **WebGPU (Dawn):** New backend for high-performance web rendering.

### Tasks
- [ ] **Platform Layer:** Port windowing/input to Emscripten HTML5 API.
- [ ] **Backend:** Implement WebGPU backend (using Dawn or Emscripten WebGPU).

---

## 10. Ecosystem (v3.0)

Tools to make development faster.

### The Goal
*   **UI Toolkit:** Lightweight immediate-mode UI.
*   **Visual Profiler:** Standalone tool to visualize task graphs.

### Tasks
- [ ] **UI:** Design and implement basic UI widgets.
- [ ] **Profiler:** Build visualizer for `SituationDumpTaskGraph` output.

---

## Immediate "Next Step" for v2.4

**Priority: Bindless Textures.**

It fundamentally changes how you write shaders and engine code. It removes the concept of "Slots" and "Bindings" for assets, which is the biggest shackle holding back performance in modern APIs.

You have the architecture. Now give it the power.
