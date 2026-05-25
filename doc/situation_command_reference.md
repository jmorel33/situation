# Situation Command Buffer Reference

_Authoritative catalog of every `SituationCmd*` recording function — Situation **2.4.126**._

## How this fits the other docs (avoid triple maintenance)

| Document | Owns | Does **not** duplicate |
|----------|------|-------------------------|
| **This file** | Every `SituationCmd*` — signature, GL/VK, tier, order, use case, examples | Frame loop philosophy, shader loading, audio, windowing |
| **[situation_sdk.md](situation_sdk.md)** §3.1–3.1.5, §3.6–3.9 | **Why** command buffers exist, trap tables, render-pass *concepts*, VD compositor, text workflow, mermaid diagrams | Per-command signature tables (link here instead) |
| **[situation_sdk.md](situation_sdk.md)** §3.2–3.5 | Short intros + **worked examples** (clear screen, shader workflow, GLTF) — may show commands inline in context | Full 35-command matrix |
| **[situation_api.md](situation_api.md)** | **Non-Cmd** graphics API (`SituationCreateMesh`, `SituationLoadShader`, structs, errors, audio, input, …) | Per-command `SituationCmd*` prose (Graphics module links here via a single index table) |
| **[situation_api_index.md](situation_api_index.md)** | Auto-generated one-line index of **all** `SITAPI` symbols | Narrative |

**Rule of thumb:** If the name starts with `SituationCmd`, document it **here first**. SDK/API only link or show commands inside a larger recipe.

**Header source:** `sit/situation_api.h` (sections `// --- Command Buffer Recording ---` and `// --- Abstracted Rendering Commands ---`). Public **`SITAPI`** prototypes live only in that file; renderer internals use `situation_impl_renderer_fwd.h` (included via `situation_impl_forward.h`).

---

## How commands work

| Phase | API | Purpose |
|-------|-----|---------|
| 1 | `SituationAcquireFrameCommandBuffer()` | Acquire swapchain / frame slot |
| 2 | `SituationGetMainCommandBuffer()` | Opaque handle for all `SituationCmd*` |
| 3 | Record commands | Render passes, binds, draws, dispatch, barriers |
| 4 | `SituationEndFrame()` | **OpenGL:** replay soft buffer + swap. **Vulkan:** submit queue + present |

**Update-before-draw:** Call `SituationUpdateBuffer`, `SituationSetShaderUniform*`, and descriptor uploads **before** recording draws that consume them in the same frame (Vulkan executes at end-of-frame).

**Tiers:**

| Tier | Meaning |
|------|---------|
| **High** | Convenience; mesh/layout owned by Situation |
| **Core** | Explicit bind + draw; same on GL and VK (vertex layout on VK is pipeline-time) |
| **GL-only** | Recorded in soft buffer; `NOT_IMPLEMENTED` on Vulkan |
| **Deprecated** | Still compiled; prefer replacement |

---

## Command index (35 active + 7 deprecated)

| Command | Tier | GL | VK | One-line use case |
|---------|------|----|----|-------------------|
| `SituationCmdBeginRenderPass` | Core | ✓ | ✓ | Target FBO + load/store/clear |
| `SituationCmdEndRenderPass` | Core | ✓ | ✓ | Close pass, store attachments |
| `SituationCmdSetViewport` | Core | ✓ | ✓ | Map NDC → pixel rect |
| `SituationCmdSetScissor` | Core | ✓ | ✓ | Clip fragments (UI lists) |
| `SituationCmdPushRasterState` | Core | ✓ | ✓ | Save blend/depth/cull block |
| `SituationCmdPopRasterState` | Core | ✓ | ✓ | Restore raster block |
| `SituationCmdSetCullMode` | Core | ✓ | ✓ | Front/back/none |
| `SituationCmdSetDepthTest` | Core | ✓ | ✓ | Depth compare on/off |
| `SituationCmdSetDepthWrite` | Core | ✓ | ✓ | Z-write mask |
| `SituationCmdSetBlendEnable` | Core | ✓ | ✓ | Alpha blending on/off |
| `SituationCmdSetBlendFuncSeparate` | Core | ✓ | ✓ | RGB/A blend factors |
| `SituationCmdBindPipeline` | Core | ✓ | ✓ | Active graphics shader/PSO |
| `SituationCmdSetPushConstant` | Core | ✓ | ✓ | Small per-draw data (≤128 B typical) |
| `SituationCmdSetPushConstantData` | Core | ✓ | ✓ | Push constants by shader + byte offset |
| `SituationCmdBindDescriptorSet` | Core | ✓ | ✓ | UBO/SSBO at `set_index` |
| `SituationCmdBindDescriptorSetDynamic` | Core | ✓ | ✓ | UBO with dynamic offset |
| `SituationCmdBindTextureSet` | Core | ✓ | ✓ | Sampled texture at set slot |
| `SituationCmdBindSampledTexture` | Core | ✓ | ✓ | `sampler2D` at binding index |
| `SituationCmdSetVertexAttribute` | GL-only | ✓ | ✗ | Interleaved/separate vertex layout |
| `SituationCmdBindVertexBuffer` | Core | ✓ | ✓ | VBO + stride for manual draw |
| `SituationCmdBindIndexBuffer` | Core | ✓ | ✓ | UINT32 index buffer + byte offset |
| `SituationCmdDraw` | Core | ✓ | ✓ | `glDrawArraysInstanced` / `vkCmdDraw` |
| `SituationCmdDrawIndexed` | Core | ✓ | ✓ | Indexed mesh strip/list |
| `SituationCmdDrawMesh` | High | ✓ | ✓ | Draw `SituationCreateMesh` handle |
| `SituationCmdDrawQuad` | High | ✓ | ✓ | Internal unit quad (UI/debug) |
| `SituationCmdDrawTexture` | High | ✓ | ✓ | Sprite blit with tint/rotation |
| `SituationCmdDrawText` | High | ✓ | ✓ | Bitmap font string |
| `SituationCmdDrawTextEx` | High | ✓ | ✓ | Scaled/spaced text |
| `SituationCmdBindComputePipeline` | Core | ✓ | ✓ | Compute program |
| `SituationCmdBindComputeTexture` | Core | ✓ | ✓ | Storage image for compute |
| `SituationCmdDispatch` | Core | ✓ | ✓ | 3D work-group grid |
| `SituationCmdPipelineBarrier` | Core | ✓ | ✓ | Compute→graphics visibility |
| `SituationCmdCopyBuffer` | Core | ✓ | ✓ | GPU buffer→buffer copy |
| `SituationCmdPresent` | Core | ✓ | ✓ | Blit texture to swapchain |
| `SituationCmdBeginDebugGroup` | Core | ✓ | ✓ | RenderDoc/NSight marker |
| `SituationCmdEndDebugGroup` | Core | ✓ | ✓ | Close debug group |

**Deprecated:** `SituationCmdBeginRenderToDisplay`, `SituationCmdEndRender`, `SituationCmdBindUniformBuffer`, `SituationCmdBindTexture`, `SituationCmdBindComputeBuffer`, `SituationMemoryBarrier` — use rows in the table above.

---

<a id="1-render-pass--framebuffer"></a>
## 1. Render pass & framebuffer

### `SituationCmdBeginRenderPass`

```c
SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info);
```

**Use when:** Starting any sequence of draws to a target (main window `display_id = -1`, or virtual display `≥ 0`).

**Configures:** Color/depth load ops (`SIT_LOAD_OP_CLEAR` / `LOAD` / `DONT_CARE`), store ops, clear values, implicit viewport to target size.

**Rules:** Do not nest passes. All draws for this target go until `EndRenderPass`.

```c
SituationRenderPassInfo pass = {
    .display_id = -1,
    .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .storeOp = SIT_STORE_OP_STORE,
                          .clear.color = (ColorRGBA){20, 30, 40, 255} },
    .depth_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .storeOp = SIT_STORE_OP_DONT_CARE,
                          .clear.depth = 1.0f }
};
SituationCmdBeginRenderPass(cmd, &pass);
```

### `SituationCmdEndRenderPass`

```c
SituationError SituationCmdEndRenderPass(SituationCommandBuffer cmd);
```

**Use when:** Finished drawing to the pass target. Required before starting another pass or compute that assumes default framebuffer.

---

<a id="2-dynamic-viewport--scissor"></a>
## 2. Dynamic viewport & scissor

### `SituationCmdSetViewport`

```c
SituationError SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height);
```

**Use when:** Split-screen, picture-in-picture, or rendering to a sub-rectangle of the pass target. Default: full target (set at `BeginRenderPass`).

### `SituationCmdSetScissor`

```c
SituationError SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);
```

**Use when:** Clipping UI, scroll regions, or masking. Coordinates are top-left origin in pixels (Situation converts for GL bottom-left internally where needed).

---

<a id="3-raster-state-fixed-function"></a>
## 3. Raster state (fixed-function)

Scoped saves avoid leaking blend/depth overrides (e.g. UI over 3D).

| Command | Use case |
|---------|----------|
| `SituationCmdPushRasterState(cmd, scope_id)` | Before temporary state change |
| `SituationCmdPopRasterState(cmd, scope_id)` | Restore matching `scope_id` |
| `SituationCmdSetCullMode(cmd, SIT_CULL_BACK)` | 3D meshes vs 2D (`SIT_CULL_NONE`) |
| `SituationCmdSetDepthTest(cmd, true, SIT_DEPTH_COMPARE_LEQUAL)` | 3D ordering |
| `SituationCmdSetDepthWrite(cmd, true)` | Opaque depth; disable for transparent overlay |
| `SituationCmdSetBlendEnable(cmd, true)` | Sprites, text, particles |
| `SituationCmdSetBlendFuncSeparate(cmd, src_rgb, dst_rgb, src_a, dst_a)` | Standard alpha: `SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA` |

---

<a id="4-graphics-pipeline--shader-data"></a>
## 4. Graphics pipeline & shader data

### `SituationCmdBindPipeline`

```c
SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader);
```

**Use when:** Every batch that shares a shader. Required before `DrawMesh` / manual `Draw*`.

**Vulkan:** Selects graphics PSO; subsequent `BindVertexBuffer` may switch pipeline variant by **stride** (simple/legacy/PBR).

### `SituationCmdSetPushConstant` / `SituationCmdSetPushConstantData`

```c
SituationError SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id, const void* data, size_t size);
SituationError SituationCmdSetPushConstantData(SituationCommandBuffer cmd, SituationShader shader, uint32_t offset, const void* data, size_t size);
```

**Use when:** Per-draw model matrix, tint, or material ID — small POD (typically ≤128 bytes).

**Prefer:** Push constants for per-object data; UBOs for camera/lights shared across many draws.

### Descriptor / texture binds

| Command | Binds | Typical shader layout |
|---------|-------|------------------------|
| `SituationCmdBindDescriptorSet(cmd, set_index, buffer)` | UBO or SSBO | `layout(binding=0) uniform LightBlock { ... }` |
| `SituationCmdBindDescriptorSetDynamic(cmd, set_index, buffer, dynamic_offset)` | Ring-buffered UBO | Per-object slice in one big buffer |
| `SituationCmdBindTextureSet(cmd, set_index, texture)` | Combined sampler+image | `layout(binding=1) uniform sampler2D tex` |
| `SituationCmdBindSampledTexture(cmd, binding, texture)` | Texture unit / descriptor binding | Explicit `binding = N` in GLSL |

**OpenGL (during frame):** `SituationSetShaderUniform*` records `SIT_OP_SET_UNIFORM` instead of immediate GL — same deferral rules as command buffer.

---

<a id="5-vertex-input--manual-draw-core-path"></a>
## 5. Vertex input & manual draw (Core path)

Use this path for **custom VBO layouts** (e.g. RGL batch, procedural geometry) instead of `SituationCreateMesh`.

### `SituationCmdSetVertexAttribute` — **[OpenGL only]**

```c
SituationError SituationCmdSetVertexAttribute(
    SituationCommandBuffer cmd,
    uint32_t location,   /* layout(location=N) in VS */
    uint32_t binding,    /* must match SituationCmdBindVertexBuffer binding */
    int size,
    SituationDataType type,
    bool normalized,
    size_t offset);      /* byte offset within vertex stride */
```

**Vulkan:** Returns `SITUATION_ERROR_NOT_IMPLEMENTED` — vertex format is fixed at `SituationLoadShader*` / SPIR-V pipeline creation.

**Interleaved layout (one VBO, binding 0):**

```c
SituationCmdSetVertexAttribute(cmd, 0, 0, 3, SIT_DATA_FLOAT, false, 0);
SituationCmdSetVertexAttribute(cmd, 1, 0, 2, SIT_DATA_FLOAT, false, 12);
SituationCmdBindVertexBuffer(cmd, 0, vbo, 0, sizeof(Vertex));
```

**Separate buffers (binding per stream):**

```c
SituationCmdSetVertexAttribute(cmd, 0, 0, 3, SIT_DATA_FLOAT, false, 0);
SituationCmdSetVertexAttribute(cmd, 1, 1, 2, SIT_DATA_FLOAT, false, 0);
SituationCmdBindVertexBuffer(cmd, 0, pos_vbo, 0, 12);
SituationCmdBindVertexBuffer(cmd, 1, uv_vbo, 0, 8);
```

### `SituationCmdBindVertexBuffer`

```c
SituationError SituationCmdBindVertexBuffer(SituationCommandBuffer cmd, uint32_t binding,
    SituationBuffer buffer, size_t offset, size_t stride);
```

**Returns:** `SITUATION_SUCCESS`, or `NOT_INITIALIZED`, `INVALID_PARAM` (null `cmd`), `INVALID_RESOURCE_HANDLE`, `MEMORY_ALLOCATION` (OpenGL soft-buffer full).

**Use when:** Manual `Draw` / `DrawIndexed`. **`stride`** must match shader/pipeline expectations; on Vulkan, may rebind pipeline variant after `BindPipeline`.

### `SituationCmdBindIndexBuffer`

```c
SituationError SituationCmdBindIndexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset);
```

**Returns:** `SITUATION_SUCCESS`, or `NOT_INITIALIZED`, `INVALID_PARAM`, `INVALID_RESOURCE_HANDLE`, `MEMORY_ALLOCATION` (OpenGL).

**Use when:** `DrawIndexed`. Index type is **always UINT32** (`uint32_t` indices). **`offset`** is a **byte** offset into the IBO (added to indexed draw index base on GL replay).

### `SituationCmdDraw`

```c
SituationError SituationCmdDraw(SituationCommandBuffer cmd,
    uint32_t vertex_count, uint32_t instance_count,
    uint32_t first_vertex, uint32_t first_instance);
```

**Use when:** Non-indexed geometry (fullscreen triangle, point clouds, RGL batched quads as triangle lists).

**Prerequisites:** `BindPipeline`, vertex layout (GL: `SetVertexAttribute` + `BindVertexBuffer`), optional uniforms.

### `SituationCmdDrawIndexed`

```c
SituationError SituationCmdDrawIndexed(SituationCommandBuffer cmd,
    uint32_t index_count, uint32_t instance_count,
    uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
```

**Use when:** Meshes built with index buffers but not wrapped in `SituationMesh`.

**Harness pattern:** `bind_index_buffer_low_level` — bind pipeline → set attributes (GL) → `BindVertexBuffer` → `BindIndexBuffer` → `DrawIndexed`.

---

<a id="6-high-level-draw-helpers"></a>
## 6. High-level draw helpers

| Command | Use when | Notes |
|---------|----------|-------|
| `SituationCmdDrawMesh` | Standard PBR/legacy mesh from `SituationCreateMesh` | Binds VAO/pipeline layout internally |
| `SituationCmdDrawQuad` | Debug rects, particles, quick UI | Internal shared quad mesh |
| `SituationCmdDrawTexture` | 2D sprites with source/dest rect, rotation, tint | Texture sampling handled internally |
| `SituationCmdDrawText` | HUD labels | Requires baked `SituationFont` |
| `SituationCmdDrawTextEx` | Variable size/spacing text | Same font atlas |

**Prefer high-level** for application code; **prefer Core** for engines (RGL, custom renderers) that own batching.

---

<a id="7-compute"></a>
## 7. Compute

| Command | Use case |
|---------|----------|
| `SituationCmdBindComputePipeline(cmd, pipeline)` | Before dispatch |
| `SituationCmdBindComputeTexture(cmd, binding, texture)` | `image2D` / storage image write |
| `SituationCmdDispatch(cmd, gx, gy, gz)` | Parallel work (particles, culling, fill buffer) |
| `SituationCmdPipelineBarrier(cmd, src_flags, dst_flags)` | **Required** if compute writes data read by vertex/fragment draw |

**Barrier example (compute fill → vertex read):**

```c
SituationCmdPipelineBarrier(cmd,
    SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
    SITUATION_BARRIER_VERTEX_SHADER_READ);
```

See `SITUATION_BARRIER_*` and `SITUATION_BARRIER_*_SHADER_*` in `situation_api.h`.

---

<a id="8-transfer--presentation"></a>
## 8. Transfer & presentation

### `SituationCmdCopyBuffer`

```c
void SituationCmdCopyBuffer(SituationCommandBuffer cmd,
    SituationBuffer src, SituationBuffer dst, size_t offset, size_t size);
```

**Use when:** GPU-side buffer copies (readback staging, animation skinning buffers).

### `SituationCmdPresent`

```c
SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);
```

**Use when:** Compute or offscreen pass produced a texture that should appear on the swapchain (specialized paths; most apps present via `SituationEndFrame` after rendering to `display_id = -1`).

---

<a id="9-debug-markers"></a>
## 9. Debug markers

```c
SituationError SituationCmdBeginDebugGroup(SituationCommandBuffer cmd, const char* name, ColorRGBA color);
SituationError SituationCmdEndDebugGroup(SituationCommandBuffer cmd);
```

**Use when:** Capturing with RenderDoc / vendor tools — group passes ("Shadow", "UI", "GBuffer").

---

<a id="10-recommended-command-order-one-3d-object-core-path"></a>
## 10. Recommended command order (one 3D object, Core path)

```c
SituationCmdBeginRenderPass(cmd, &pass);
SituationCmdSetViewport(cmd, 0, 0, (float)w, (float)h);

SituationCmdBindPipeline(cmd, shader);
SituationCmdBindDescriptorSet(cmd, 0, camera_ubo);
SituationCmdBindDescriptorSet(cmd, 1, light_ubo);
SituationCmdBindTextureSet(cmd, 2, albedo);

#if defined(SITUATION_USE_OPENGL)
SituationCmdSetVertexAttribute(cmd, 0, 0, 3, SIT_DATA_FLOAT, false, offsetof(Vertex, pos));
/* ... */
#endif
SituationCmdBindVertexBuffer(cmd, 0, vbo, 0, sizeof(Vertex));
SituationCmdBindIndexBuffer(cmd, ibo, 0);

SituationCmdSetPushConstant(cmd, 0, &model, sizeof(mat4));
SituationCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);

SituationCmdEndRenderPass(cmd);
```

---

<a id="11-deprecated-commands"></a>
## 11. Deprecated commands

| Deprecated | Replacement |
|------------|-------------|
| `SituationCmdBeginRenderToDisplay` | `SituationCmdBeginRenderPass` + `SituationRenderPassInfo` |
| `SituationCmdEndRender` | `SituationCmdEndRenderPass` |
| `SituationCmdBindUniformBuffer` | `SituationCmdBindDescriptorSet` |
| `SituationCmdBindTexture` | `SituationCmdBindTextureSet` / `BindSampledTexture` |
| `SituationCmdBindComputeBuffer` | `SituationCmdBindDescriptorSet` with storage usage |
| `SituationMemoryBarrier` | `SituationCmdPipelineBarrier` |

---

_Regenerate API index after header changes: `python scripts\generate_situation_api_docs.py`_
