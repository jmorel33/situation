# Plan: Universal Handles & SSBO Standardization — Hardened Roadmap

**Last reviewed:** 2026-06-12 (cross-checked against v2.4.260 codebase, Phases A–B complete)
**Goal:** Drive the renderer to a fully bindless internal architecture — no per-draw descriptor set allocation, no fixed-function vertex attribute paths, no legacy binding tables. Every resource is addressable by a GPU-resident 64-bit handle or a stable slot index into a globally-resident descriptor array.

---

## Architectural Summary (What "All Bindless Internally" Means)

### Target state

```
Texture  →  slot_index into global_bindless_set (VK) / ARB_bindless_texture handle (GL)
Buffer   →  GPU virtual address via vkGetBufferDeviceAddress (VK) / NV BDA (GL where supported)
Mesh     →  vertex buffer GPU VA passed as push constant → shader pulls via gl_VertexIndex (VK)
             falls back to VAO/VBO on GL unless NV_shader_buffer_load is available
Shader   →  push constant block carries: texture_id (uint32), model matrix (mat4), BDA (uint64)
             no per-draw BindDescriptorSets except the one-time global_bindless_set bind at frame start
```

### What is already built (v2.4.258)

| Subsystem | Status | Notes |
|-----------|--------|-------|
| Vulkan global bindless descriptor set | ✅ **Implemented** | `global_bindless_set` / `global_bindless_pool` / `bindless_descriptor_layout`; `PARTIALLY_BOUND + UPDATE_AFTER_BIND` flags; all standard textures written to it at `SituationCreateTexture` |
| Vulkan descriptorIndexing + bufferDeviceAddress feature gating | ✅ **Implemented** | Queried and enabled in `VkPhysicalDeviceVulkan12Features`; `SIT_FEATURE_BINDLESS_TEXTURES` / `SIT_FEATURE_BINDLESS_BUFFERS` set on `enabled_features_mask` |
| `SituationGetTextureHandle` (VK) | ✅ **Corrected** | Now validates slot liveness via `_SitGetTextureSlot`; stale handles return 0 + `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED`. |
| `SituationGetTextureHandle` (GL) | ✅ **Implemented** | `glGetTextureHandleARB` + `glMakeTextureHandleResidentARB`; gated on `GLAD_GL_ARB_bindless_texture`. Silently returns 0 when unavailable. |
| `SituationGetBufferDeviceAddress` (VK) | ✅ **Implemented** | `vkGetBufferDeviceAddress`; real GPU VA. Works for any buffer with `SITUATION_BUFFER_USAGE_DEVICE_ADDRESS`. |
| `SituationGetBufferDeviceAddress` (GL) | ✅ **Corrected** | Now emits `SITUATION_ERROR_OPENGL_UNSUPPORTED` + diagnostic message on AMD/Intel/Mesa instead of silent 0. |
| `SITUATION_BUFFER_USAGE_DEVICE_ADDRESS` flag | ✅ **Implemented** | Maps to `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` on VK; used by SSBO_COMPUTE preset. |
| Vulkan mesh vertex buffer `STORAGE_BUFFER_BIT` | ✅ **Implemented** | All meshes created with `VERTEX_BUFFER + STORAGE_BUFFER + TRANSFER_DST`. Vertex data already in SSBO-accessible buffer. |
| GL virtual bindless fallback (LRU texture unit cache) | ✅ **Implemented** | `_SituationGLVirtualTextureSlot[32]` LRU eviction; `gl_bindless_handle` cached on texture slot; `current_virtual_loc` cached. |
| `SituationFeature` flags exposed in `SituationGraphicsCaps` | ✅ **Implemented** | `SIT_FEATURE_BINDLESS_BUFFERS`, `SIT_FEATURE_BINDLESS_TEXTURES` queryable. |
| Harness tests for `GetTextureHandle` / `GetBufferDeviceAddress` | ✅ **Hardened** | Both tests now assert nonzero on supported hardware; `test_buffer_device_address` additionally checks `GetLastErrorCode == SITUATION_ERROR_OPENGL_UNSUPPORTED` on unsupported GL. `test_get_texture_handle` verifies stale handle returns 0. |
| `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` on mesh vertex buffer | ✅ **Fixed** | Both vertex and index buffers now created with `SHADER_DEVICE_ADDRESS_BIT`. BDA calls valid. |
| `SituationGetMeshVertexBufferAddress` | ✅ **Implemented** | Declared in `situation_api.h`; VK + GL (NV) paths implemented; stale-handle + unsupported error coverage. |
| Vertex pulling path in shaders | ❌ **Not implemented** | All rendering still goes through VAO/VBO attribute fetch. |
| Internal draw calls use BDA / bindless | ❌ **Not yet** | Internal VD compositor, text, quad draws bind individual descriptor sets per draw rather than pulling from global_bindless_set uniformly. |

---

## Gap Analysis — What Prevents Full Bindless Internally

### Gap 1 — Mesh vertex data has no GPU VA (VK)
The VK mesh vertex buffer is `VERTEX_BUFFER | STORAGE_BUFFER | TRANSFER_DST`. Missing `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`. Calling `vkGetBufferDeviceAddress` on it is invalid. Fix: add the bit at mesh creation; expose `SituationGetMeshVertexBufferAddress`.

### Gap 2 — `GetBufferDeviceAddress` on GL is silent-zero on non-NVIDIA
The OpenGL path requires `GL_NV_shader_buffer_load`. On AMD/Intel/Mesa it returns 0 with no error or log. Callers cannot distinguish "unsupported" from "valid zero address". Fix: emit `SITUATION_ERROR_OPENGL_UNSUPPORTED` + log; gate behavior on `SituationIsFeatureSupported`.

### Gap 3 — Harness tests are no-op smoke tests
`test_get_texture_handle` and `test_buffer_device_address` both discard the return value and always pass. They prove the API doesn't crash, nothing more. Fix: assert nonzero on hardware that supports it; add end-to-end shader use tests.

### Gap 4 — No `SituationGetMeshVertexBufferAddress` API
The API surface for mesh BDA doesn't exist. This is the primary thing needed before any vertex-pull shader example can be written.

### Gap 5 — Internal draws still per-draw bind
Text, quad, VD compositor use a mix of `global_bindless_set` (for textures) and per-draw individual descriptor set binds. Some paths already bind `global_bindless_set` correctly; others still allocate and bind per-texture sets. The goal is: bind `global_bindless_set` once per frame at set 0, carry all resource IDs in push constants.

### Gap 6 — No vertex-pull pipeline path
There is no `SituationCreateMeshEx` and no fullscreen-triangle primitive yet (v2.5 Phase 5a). Without these, the vertex-pull model requires users to hand-wire the VAO themselves, which defeats the point.

### Gap 7 — VK `GetTextureHandle` returns `slot_index` raw with no validation
It returns `(uint64_t)texture.slot_index` with only a `!texture.generation` guard. It doesn't check that the slot is actually in `global_bindless_set` (e.g. for textures created before the bindless infrastructure was initialized, or for storage-only textures). Should use `_SitGetTextureSlot` to validate liveness and set registration.

---

## Phase Plan

### Phase A — Hardening & correctness of what's already built
*Target: v2.4.x patch releases. No new API surface beyond minor fixes.*

- [x] **A1** — `SituationGetBufferDeviceAddress` GL: emit `SITUATION_ERROR_OPENGL_UNSUPPORTED` (not silent 0) when `GL_NV_shader_buffer_load` / `GL_EXT_buffer_reference` is unavailable. Log once. Return 0.
- [x] **A2** — `SituationGetTextureHandle` VK: validate slot liveness via `_SitGetTextureSlot` before returning `slot_index`. Return 0 + error on stale/invalid handle.
- [x] **A3** — Harness `test_get_texture_handle`: assert return is nonzero when `SIT_FEATURE_BINDLESS_TEXTURES` is set; skip with log when not supported. Add a second frame where the handle is passed as a push constant to a shader and sampled.
- [x] **A4** — Harness `test_buffer_device_address`: assert nonzero on VK (feature is required); assert nonzero on GL only when `SIT_FEATURE_BINDLESS_BUFFERS` is set. Add compute dispatch that reads from the BDA.
- [x] **A5** — Emit `SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED` (new error code) as a clear message when someone calls any future mesh BDA API on GL without NV extension.

---

### Phase B — Mesh vertex buffer device address
*Target: v2.4.x. Prerequisite for vertex pulling.*

- [x] **B1** — Add `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` to all VK mesh vertex buffer creation in `SituationCreateMesh` and `SituationCreateMeshEx` (once it exists from v2.5 Phase 5a).
- [x] **B2** — Declare and implement `SituationGetMeshVertexBufferAddress(SituationMesh mesh) → uint64_t`:
  - VK: `vkGetBufferDeviceAddress` on `slot->vertex_buffer`. Returns 0 if `!SIT_FEATURE_BINDLESS_BUFFERS`.
  - GL: `glGetNamedBufferParameterui64v(slot->vbo_id, GL_BUFFER_GPU_ADDRESS_NV)` + `glMakeNamedBufferResidentNV` where available. Returns 0 + `NOT_SUPPORTED` where not.
- [x] **B3** — Also expose `SituationGetMeshIndexBufferAddress(SituationMesh mesh) → uint64_t` (same contract for index buffer). Needed for GPU-driven index fetch.
- [x] **B4** — Harness test: assert both addresses nonzero on VK; pass vertex BDA to a compute shader that reads `position[0]` and writes to a readback buffer; compare against known upload data.
- [x] **B5** — Document in `situation_api.md`: BDA is only valid while the mesh is alive; destroying the mesh invalidates the address with no GPU-side fence. Users must ensure GPU-side work is complete before `SituationDestroyMesh`.

---

### Phase C — Vertex pull pipeline
*Target: v2.5 Phase 5a / 5b. Depends on Phase B and `SituationCreateMeshEx`.*

- [ ] **C1** — Define `SIT_MESH_LAYOUT_PULL` as a layout variant in `SituationMeshVertexLayout` where the mesh vertex buffer is bound at set N as an SSBO and `gl_VertexIndex` is the fetch key. The VAO/VBO path is still compiled for the fixed-layout draw path; the pull path is parallel.
- [ ] **C2** — Implement `SituationCmdBindMeshPullBuffers(cmd, mesh)`: on VK, push the vertex BDA as a push constant and bind no VAO; on GL, bind `vbo_id` as `GL_SHADER_STORAGE_BUFFER` at a well-known binding point (e.g. `binding = 15`) and pass its NV address if available.
- [ ] **C3** — Provide a standard pull-model vertex shader snippet (not a built-in pipeline — a documented GLSL include pattern) that the user drops in:
  ```glsl
  layout(push_constant) uniform PC { uint64_t vertex_ptr; mat4 mvp; } pc;
  struct Vertex { vec3 pos; float pad; vec3 norm; float pad2; vec2 uv; float pad3[2]; };
  layout(buffer_reference, std430) readonly buffer VB { Vertex v[]; };
  void main() {
      Vertex vert = VB(pc.vertex_ptr).v[gl_VertexIndex];
      gl_Position = pc.mvp * vec4(vert.pos, 1.0);
  }
  ```
- [ ] **C4** — Update `fps_ray_demo` and `shader_lab_torus` to use pull model as the primary path where BDA is available, VAO fallback otherwise.
- [ ] **C5** — Harness test: load a mesh, get its BDA, render via pull shader, readback pixel, compare against a reference render via the standard VAO path. Both must produce the same output.

---

### Phase D — Global bindless set as the sole frame-level binding
*Target: v2.5 / v2.6. Internal renderer refactor.*

This phase makes `global_bindless_set` the **only** descriptor set bound at frame start on VK for all internal rendering paths. All texture references carried as push constant `texture_id` (slot index). No per-draw `vkCmdBindDescriptorSets` for textures.

- [ ] **D1** — Audit all internal VK draw call sites: `DrawTexture`, `DrawQuad`, `CmdDrawText`, VD compositor, YPQ grade pass. For each: record whether it uses `global_bindless_set` + push constant `texture_id`, or still allocates/binds a per-draw set.
- [ ] **D2** — Migrate remaining per-draw texture sets to `global_bindless_set` + `texture_id` push constant. The infrastructure already exists — the per-texture `single_sampler_descriptor_set` path is a legacy holdover from before the global set was reliable.
- [ ] **D3** — Remove `single_sampler_descriptor_set` / `single_sampler_descriptor_pool` from `_SituationTextureSlot` once all internal paths are migrated. This frees one pool allocation per texture.
- [ ] **D4** — For user shaders loaded via `SituationLoadShaderFromMemory`: the current set 1 = `text_sampler_layout` binding is the correct user-facing API (`SituationCmdBindTextureSet(cmd, 1, tex)`). Keep this working. Internally, decide whether user pipelines opt into the global bindless set or use their own descriptor layout — document the contract clearly.
- [ ] **D5** — Bind `global_bindless_set` once per frame in `SituationAcquireFrameCommandBuffer` (VK) at set 3 (or the reserved bindless set index). All internal shaders are authored to expect it there.
- [ ] **D6** — GL equivalent: the virtual bindless LRU cache (`_SituationGLVirtualTextureSlot[32]`) is already in place. Verify it is engaged for all internal draw paths (not just some). Add stats to `SituationGetGraphicsCaps` or a diagnostic API so the hit/miss/eviction counters are externally visible.

---

### Phase E — Buffer device address in push constants (standard "Mega-Shader" contract)
*Target: v2.6+. Depends on Phase B and Phase C.*

- [ ] **E1** — Define `SIT_PUSH_CONSTANT_LAYOUT_BINDLESS` as a new push constant block layout:
  ```c
  struct SitBindlessPushConstants {
      mat4     mvp;           /* bytes 0-63  */
      uint64_t vertex_ptr;    /* bytes 64-71 — mesh vertex BDA */
      uint64_t index_ptr;     /* bytes 72-79 — mesh index BDA  */
      uint32_t texture_id;    /* bytes 80-83 — index into global_bindless_set */
      uint32_t material_id;   /* bytes 84-87 — SSBO material slot index */
      uint32_t object_id;     /* bytes 88-91 — for multi-draw / picking */
      uint32_t _pad;          /* bytes 92-95 */
  };                          /* total: 96 bytes — fits standard 128-byte push constant budget */
  ```
- [ ] **E2** — Ship `SIT_SPIRV_LAYOUT_PROFILE_BINDLESS` as a new `SituationSpirvLayoutProfile` value. Selects a VK pipeline layout with: set 0 = global_bindless_set (textures), push constants = `SitBindlessPushConstants`. No per-draw descriptor sets.
- [ ] **E3** — Provide `SituationCmdDrawMeshBindless(cmd, mesh, texture, mvp)` as a high-level call that fills the full push constant block from handles and dispatches.
- [ ] **E4** — Provide standard GLSL header `sit_bindless.glsl` (embedded as a string constant, includeable via `#extension` preamble) with the buffer-reference structs, the push constant block, and helper functions:
  ```glsl
  Vertex sit_fetch_vertex(uint i);    // reads from push_const.vertex_ptr
  vec4   sit_sample_texture(vec2 uv); // samples global_textures[push_const.texture_id]
  ```
- [ ] **E5** — Example `examples/bindless_mesh.c`: single mesh, single texture, entire frame with one `SituationCmdDrawMeshBindless` call and zero `BindDescriptorSet` calls. Document as the reference "Mega-Shader" pattern.

---

### Phase F — SSBO-first canonical mesh format (optional / future)
*Parking lot. Depends on Phase C and E.*

The current mesh on-GPU format is: vertex data in VBO (GL) / `VkBuffer` with `VERTEX_BUFFER | STORAGE_BUFFER` (VK). This is already SSBO-accessible on VK. Full "SSBO-first" means the vertex buffer is the primary and the IA-fed VAO is gone.

- [ ] **F1** — On VK: remove `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT` from meshes that use `SIT_MESH_LAYOUT_PULL`. The vertex fetch happens entirely through BDA; the IA is bypassed.
- [ ] **F2** — On GL: `SituationCmdSetVertexAttribute` is marked `[Legacy]` in docs. New examples do not use it. Existing tests continue to pass.
- [ ] **F3** — STD430 alignment helpers: `SituationMeshStd430Vertex` typedef published in `situation_base_types.h` so user shader structs match without manual padding math.

---

## Harness Coverage Plan

| Test name | Phase | What it proves |
|-----------|-------|----------------|
| `test_get_texture_handle` (hardened) | A3 | Nonzero on supported hardware; bindless sample in shader |
| `test_buffer_device_address` (hardened) | A4 | Nonzero on VK; compute read via BDA |
| `test_mesh_vertex_buffer_address` | B4 | VK mesh BDA nonzero; compute read of vertex[0].pos |
| `test_vertex_pull_render` | C5 | Pull-model render matches VAO-render pixel-for-pixel |
| `test_bindless_draw_mesh` | E5 | Full frame with `DrawMeshBindless`, zero per-draw BindDescriptorSets |

---

## Dependency Map

```
Phase A (hardening)
    │
    ├── Phase B (mesh BDA API)
    │       │
    │       └── Phase C (vertex pull pipeline)  ←── v2.5 Phase 5a (CreateMeshEx, CreateFullscreenTriangle)
    │               │
    │               └── Phase E (bindless push constant contract)
    │                       │
    │                       └── Phase F (full SSBO-first, IA bypass)
    │
    └── Phase D (global_bindless_set as sole frame binding)
            │
            └── feeds into Phase E (D's single-bind-per-frame = E's zero-per-draw-binds)
```

---

## What This Plan Does Not Do

- Does not change the public `SituationTexture` / `SituationBuffer` / `SituationMesh` struct shapes (no uint64_t handle collapse). The struct-with-generation-counter model provides type safety and is the right choice for C. BDA and bindless handles are *additional* query APIs on top of handles, not replacements.
- Does not require `ARB_bindless_texture` on OpenGL for the library to function. The virtual bindless LRU cache is the GL fallback. True GL bindless is a bonus when present.
- Does not break existing user code. All existing `SituationCmdBindDescriptorSet` / `SituationCmdBindTextureSet` calls remain valid. The bindless path is additive.
- Does not target Linux/macOS until those backends land.

---

## Current Status Summary (v2.4.258)

| Phase | Status |
|-------|--------|
| A — Hardening | ✅ Complete (v2.4.259) |
| B — Mesh BDA API | ✅ Complete (v2.4.260) |
| C — Vertex pull | 🔲 Blocked on Phase B + v2.5 Phase 5a |
| D — Global bindless as sole binding | 🔲 Not started (infrastructure exists, migration not done) |
| E — Bindless push constant contract | 🔲 Blocked on B + D |
| F — Full SSBO-first, IA bypass | 🔲 Parking lot |
