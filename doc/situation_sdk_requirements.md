# Situation SDK — Quest Engine Requirements

Reference document for the Situation SDK API surface as consumed by the Quest Spectral Renderer. Covers the existing API, pending additions needed for NRC Task 3.4, and backend implementation guidance for OpenGL and Vulkan.

**Current stub:** `host/situation.h`
**Consumers:** `host/nrc_pipeline.h`, `examples/nrc_test.c`, `examples/*.c`

---

## 1. Existing API (Implemented)

### 1.1 Types

```c
typedef struct { uint32_t id; size_t size; } SituationBuffer;
typedef struct { uint32_t id; }              SituationTexture;
typedef void*                                SituationCommandBuffer;
typedef struct { ... }                       SituationComputePipeline;
typedef struct { ... }                       SituationInitInfo;
typedef struct { ... }                       SituationRenderPassInfo;
```

### 1.2 Constants

```c
// Texture formats
SIT_FMT_RGBA32F, SIT_TEXTURE_FORMAT_R32F, SIT_TEXTURE_FORMAT_R16F

// Usage flags (combinable)
SIT_USAGE_STORAGE, SIT_USAGE_SAMPLED
SIT_BUFFER_USAGE_STORAGE, SIT_BUFFER_USAGE_TRANSFER_SRC,
SIT_BUFFER_USAGE_TRANSFER_DST, SIT_BUFFER_USAGE_UNIFORM

// Compute layout hints
SIT_COMPUTE_LAYOUT_STORAGE, SIT_COMPUTE_LAYOUT_IMAGE

// Window / input
SIT_SUCCESS, SITUATION_FLAG_WINDOW_RESIZABLE, SITUATION_FLAG_VSYNC_HINT
SIT_KEY_ESCAPE, SIT_KEY_N, SIT_KEY_M, SIT_MOUSE_BUTTON_RIGHT
SIT_LOAD_ACTION_CLEAR, SIT_SCALING_NONE, SIT_BLEND_NONE
```

### 1.3 Lifecycle

| Function | Purpose |
|----------|---------|
| `SituationInit(argc, argv, &info)` | Initialize window + GPU context. Returns `SIT_SUCCESS` or error. |
| `SituationShutdown()` / `SituationTerminate()` | Tear down everything. |
| `SituationGetLastErrorMsg()` | Last error as string. |
| `SituationGetRenderWidth()` / `Height()` | Current render resolution. |

### 1.4 Frame Loop

| Function | Purpose |
|----------|---------|
| `SituationWindowShouldClose()` | Returns true when window close requested. |
| `SituationPollInputEvents()` | Process OS input events. |
| `SituationUpdateTimers()` | Tick internal clocks. |
| `SituationGetTime()` | Wall clock time (seconds, double). |
| `SituationGetDeltaTime()` | Frame delta (seconds, double). |
| `SituationAcquireFrameCommandBuffer()` | Begin frame. Returns true if ready. |
| `SituationGetMainCommandBuffer()` | Get the command buffer for this frame. |
| `SituationEndFrame()` | Submit command buffer, present. **Implicit fence: all GPU work submitted this frame is guaranteed complete before the next frame's `AcquireFrameCommandBuffer` returns.** |

### 1.5 Input

| Function | Purpose |
|----------|---------|
| `SituationIsKeyPressed(key)` | Key just pressed this frame (edge trigger). |
| `SituationIsMouseButtonDown(button)` | Mouse button held. |
| `SituationGetMouseDelta()` | Mouse movement since last frame (vec2). |
| `SituationDisableCursor()` | Hide + capture cursor (FPS mode). |
| `SituationShowCursor()` | Restore cursor. |

### 1.6 Buffers

| Function | Purpose |
|----------|---------|
| `SituationCreateBuffer(usage, data, size)` | Allocate GPU buffer. `data` can be NULL (zero-init). |
| `SituationDestroyBuffer(&buf)` | Free GPU buffer. |
| `SituationUpdateBuffer(buf, data, size)` | CPU→GPU upload (full buffer or partial). Synchronous. |
| `SituationGetBufferDeviceAddress(buf)` | Get 64-bit BDA for bindless access in shaders. |

### 1.7 Textures

| Function | Purpose |
|----------|---------|
| `SituationCreateTexture(w, h, fmt, usage)` | Allocate 2D texture. |
| `SituationCreateTexture3D(w, h, d, fmt, usage)` | Allocate 3D texture. |
| `SituationGetTextureHandle(tex)` | Get bindless handle (sampler or image). |
| `SituationMakeTextureHandleResident(handle)` | Make handle accessible to shaders. |
| `SituationMakeTextureHandleNonResident(tex)` | Revoke residency before destroy. |
| `SituationDestroyTexture(&tex)` | Free texture. |

### 1.8 Compute

| Function | Purpose |
|----------|---------|
| `SituationCreateComputePipeline(spv_path, layout)` | Compile SPIR-V into a compute pipeline. |
| `SituationDestroyComputePipeline(&pipeline)` | Free pipeline. |
| `SituationCmdBindComputePipeline(cmd, pipeline)` | Bind pipeline for dispatch. |
| `SituationCmdBindShaderBuffer(cmd, binding, buf)` | Bind SSBO/UBO to a binding slot. |
| `SituationCmdBindComputeTexture(cmd, slot, tex)` | Bind texture to a compute image slot. |
| `SituationCmdDispatch(cmd, x, y, z)` | Dispatch compute workgroups. |
| `SituationCmdPipelineBarrier(cmd)` | Full pipeline barrier (execution + memory). |

### 1.9 Rendering

| Function | Purpose |
|----------|---------|
| `SituationCreateVirtualDisplay(size, scale, layer, scaling, blend)` | Create a virtual display for compositing. |
| `SituationGetVirtualDisplayTexture(display)` | Get the display's backing texture. |
| `SituationDestroyVirtualDisplay(&display)` | Free display. |
| `SituationCmdBeginRenderPass(cmd, &pass_info)` | Begin a render pass. |
| `SituationCmdEndRenderPass(cmd)` | End render pass. |
| `SituationRenderVirtualDisplays(cmd)` | Composite all virtual displays. |

---

## 2. Pending Additions (NRC Task 3.4)

Three new functions needed for async GPU→CPU buffer readback. The NRC adaptive training system reads back a 4-byte atomic counter each frame to decide how many training iterations to run.

### 2.1 `SituationCreateReadbackBuffer`

```c
SituationBuffer SituationCreateReadbackBuffer(size_t size);
```

**Purpose:** Allocate a host-visible staging buffer optimized for GPU→CPU readback.

**Contract:**
- The buffer must be persistently mapped (or mappable with zero overhead).
- CPU reads from this buffer must not require GPU synchronization beyond the frame boundary.
- The buffer is never written to by the CPU — it's a one-way GPU→CPU channel.

**OpenGL implementation:**
```c
GLuint buf;
glCreateBuffers(1, &buf);
glNamedBufferStorage(buf, size,  NULL,
    GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
void* mapped = glMapNamedBufferRange(buf, 0, size,
    GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
// Store `mapped` pointer alongside the buffer handle
```

**Vulkan implementation:**
```c
VkBufferCreateInfo ci = { .size = size, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT };
VkBuffer buf;
vkCreateBuffer(device, &ci, NULL, &buf);
// Allocate with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
// vkMapMemory once, store pointer
```

### 2.2 `SituationCmdCopyBuffer`

```c
void SituationCmdCopyBuffer(SituationCommandBuffer cmd,
                            SituationBuffer src, SituationBuffer dst,
                            size_t offset, size_t size);
```

**Purpose:** Schedule an async GPU→staging buffer copy within the current command buffer.

**Contract:**
- The copy executes after all preceding dispatches in the same command buffer complete (implicit execution dependency from `SituationCmdPipelineBarrier`).
- The copy must complete before the next frame's `SituationAcquireFrameCommandBuffer` returns (guaranteed by `SituationEndFrame`'s implicit fence).
- `offset` is the byte offset into `src`. `size` is bytes to copy. `dst` is written at offset 0.

**OpenGL implementation:**
```c
glCopyNamedBufferSubData(src.gl_id, dst.gl_id, offset, 0, size);
// Note: In OpenGL, this is synchronous w.r.t. the GL command stream but
// non-blocking w.r.t. the CPU. The persistent map + coherent bit means
// the CPU can read the result after the next glFinish/SwapBuffers.
```

**Vulkan implementation:**
```c
VkBufferCopy region = { .srcOffset = offset, .dstOffset = 0, .size = size };
vkCmdCopyBuffer(cmd, src.vk_buf, dst.vk_buf, 1, &region);
// The frame's submit + present fence guarantees completion before next acquire.
```

### 2.3 `SituationReadBuffer`

```c
void SituationReadBuffer(SituationBuffer readback_buf, void* dst, size_t size);
```

**Purpose:** CPU-side read from a readback buffer. Copies `size` bytes from the persistently mapped staging buffer into `dst`.

**Contract:**
- The caller guarantees the GPU copy has completed (by calling this in the *next* frame after `SituationCmdCopyBuffer`).
- This is a pure `memcpy` from the mapped pointer — zero GPU interaction, zero stall.
- If the buffer was created with `GL_MAP_COHERENT_BIT` (OpenGL) or `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` (Vulkan), no explicit flush/invalidate is needed.

**OpenGL implementation:**
```c
memcpy(dst, readback_buf.mapped_ptr, size);
```

**Vulkan implementation:**
```c
// If not host-coherent, call vkInvalidateMappedMemoryRanges first
memcpy(dst, readback_buf.mapped_ptr, size);
```

---

## 3. Usage Pattern: NRC Adaptive Training

```c
// === Init ===
SituationBuffer readback = SituationCreateReadbackBuffer(sizeof(uint32_t));

// === Frame N ===

// pre_trace:
uint32_t prev_count = 0;
SituationReadBuffer(readback, &prev_count, sizeof(uint32_t));  // Reads frame N-1's data
// ... use prev_count to set training iterations ...

// post_trace (after path tracing dispatch + barrier):
SituationCmdCopyBuffer(cmd, atomic_counter_buf, readback, 0, sizeof(uint32_t));
// ... dispatch training + inference ...

// EndFrame:
SituationEndFrame();  // Implicit fence — copy guaranteed complete for next frame's read
```

**Timeline:**
```
Frame 0: copy counter → readback       (no read yet, first frame)
Frame 1: read readback (frame 0 data)  → copy counter → readback
Frame 2: read readback (frame 1 data)  → copy counter → readback
...
```

Data is always 1 frame stale. This is acceptable — sample count is temporally coherent, and the camera flush (Task 3.2) handles discontinuities.

---

## 4. Critical Contract: EndFrame as Implicit Fence

`SituationEndFrame()` must guarantee that **all GPU work submitted in the current frame's command buffer is complete before the next frame's `SituationAcquireFrameCommandBuffer()` returns.**

This is the foundation of the 1-frame-delayed readback pattern. Without this guarantee, `SituationReadBuffer` in frame N+1 could read incomplete data from frame N's copy.

**OpenGL:** This is naturally satisfied by `glFinish()` or `SwapBuffers()` (which implies a finish on most drivers). If using triple buffering or async present, an explicit `glClientWaitSync` on a fence inserted after the copy may be needed.

**Vulkan:** The standard acquire/present fence pattern satisfies this. `vkAcquireNextImageKHR` waits on the fence from 2 frames ago (with triple buffering) or 1 frame ago (double buffering). For the readback to be safe with triple buffering, the readback buffer should be triple-buffered too (3 staging buffers, round-robin). For simplicity, double buffering (2 staging buffers) is recommended.

---

## 5. Future Considerations

| Need | When | Notes |
|------|------|-------|
| `SituationCmdCopyBufferToImage` | If NRC texture debug dump is needed | GPU→CPU texture readback for validation |
| `SituationCreateFence` / `SituationWaitFence` | If triple buffering is used | Explicit fence for readback safety beyond double-buffer |
| `SituationGetBufferMappedPointer` | If zero-copy readback is preferred | Skip the memcpy in `SituationReadBuffer`, return the mapped ptr directly |
| `SituationCmdFillBuffer` | Potential optimization for counter reset | Replace `SituationUpdateBuffer` for the atomic counter zero-fill |
