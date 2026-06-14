# Quest Spectral Renderer — Situation API Gaps Plan

**Date:** 2026-06-13  
**Audited against:** Situation v2.4.x (`sit/situation_api.h`)  
**Consumer:** Quest Spectral Renderer — `host/situation.h`, `host/nrc_pipeline.h`  
**Scope:** Gap between what Quest's stub expects and what Situation actually exposes. Three categories: name mismatches in the stub (fix the stub), missing API (implement in Situation), and already-done.

---

## Status summary

| Category | Count | Action |
|----------|-------|--------|
| ✅ Already implemented, name matches | ~20 functions | Nothing to do |
| ⚠ Already implemented, wrong name in stub | 6 items | Fix Quest stub |
| ❌ Not implemented in Situation | 4 items | Implement |

---

## Phase 1 — Fix name mismatches in Quest stub (`host/situation.h`)

These are all working in Situation under a different name. The Quest stub is wrong — update it.

- [ ] `SituationGetTime()` → `SituationTimerGetTime()` — returns `double` seconds
- [ ] `SituationGetDeltaTime()` → `SituationGetFrameTime()` — returns `float` seconds
- [ ] `SituationCmdBindShaderBuffer(cmd, binding, buf)` → `SituationCmdBindDescriptorSet(cmd, set_index, buf)` — note: parameter is a set index, not a raw binding point
- [ ] `SituationCmdPipelineBarrier(cmd)` → `SituationCmdPipelineBarrier(cmd, src_flags, dst_flags)` — the zero-arg form does not exist; use `SITUATION_BARRIER_COMPUTE_SHADER_WRITE` / `SITUATION_BARRIER_COMPUTE_SHADER_READ` for the NRC dispatch→copy transition
- [ ] `SituationCreateBuffer(usage, data, size)` → `SituationCreateBuffer(size, data, usage_flags, out_buffer)` — different arg order, output is an out-param not a return value
- [ ] `SituationAcquireFrameCommandBuffer()` — stub expects `bool`; real API returns `SituationError` (`SITUATION_SUCCESS` = ready, non-zero = skip frame)

### Also fix in stub: texture creation

The Quest stub uses `SituationCreateTexture(w, h, fmt, usage)`. The real API takes a `SituationImage` struct. For Quest's use case (creating an uninitialized GPU texture with specific usage flags), the correct call is:

```c
// Quest pattern to replace stub:
SituationImage img = { .width = w, .height = h, .channels = 4, .data = NULL };
SituationCreateTextureEx(img, false, SITUATION_TEXTURE_USAGE_STORAGE | SITUATION_TEXTURE_USAGE_SAMPLED, &out_tex);
```

- [ ] Update stub wrapper to use `SituationCreateTextureEx` with a null-data `SituationImage`

---

## Phase 2 — Implement missing API in Situation

These are genuinely absent from `sit/situation_api.h` and need to be added.

### 2.1 `SituationCreateTexture3D`

Quest needs 3D textures for NRC radiance cache grids (e.g. a `w×h×d` RGBA32F volume).

**Declaration to add** (`sit/situation_api.h`):
```c
SITAPI SituationError SituationCreateTexture3D(int width, int height, int depth,
    SituationTextureUsageFlags flags, SituationTexture* out_texture); // Create an uninitialized 3D GPU texture.
```

**Implementation notes:**
- `sit/situation_impl_renderer.h` — add GL path: `glCreateTextures(GL_TEXTURE_3D, ...)` + `glTextureStorage3D(..., GL_RGBA32F, w, h, d)`
- Vulkan path: `VkImageCreateInfo` with `imageType = VK_IMAGE_TYPE_3D`, `extent = {w, h, d}`, `arrayLayers = 1`
- For now, format is fixed to RGBA32F (the only NRC use case). Can be generalized later.
- Reuse existing `_SituationTextureSlot` registry — add a `is_3d` flag or store depth in the slot
- `SituationDestroyTexture` already handles cleanup generically, no changes needed there
- Add `SituationGetTextureHandle` support for 3D textures (bindless image handle for compute)

**Checklist:**
- [ ] Add declaration to `sit/situation_api.h`
- [ ] Add string entry to `SituationGetErrorString()` for any new error codes
- [ ] Implement GL path in `sit/situation_impl_renderer.h`
- [ ] Implement Vulkan path in `sit/situation_impl_renderer.h`
- [ ] Add trace ID in `sit/situation_base_trace.h`
- [ ] Harness test: `graphics.create_texture_3d_basic` — create, get handle, dispatch compute that writes to it, destroy

### 2.2 `SituationGetBufferMappedPointer`

Quest wants zero-copy readback — skip the `memcpy` in `SituationReadBuffer` and work directly from the mapped pointer. Lower priority than 3D textures but removes a copy on the hot readback path.

**Declaration to add** (`sit/situation_api.h`):
```c
SITAPI void* SituationGetBufferMappedPointer(SituationBuffer buffer); // Get the persistently mapped CPU pointer for a readback buffer; NULL if not mappable.
```

**Implementation notes:**
- Only valid for buffers created with `SituationCreateReadbackBuffer` (the `_SituationBufferSlot` already stores `mapped_ptr` internally)
- Returns `NULL` and sets last error `SITUATION_ERROR_BUFFER_NOT_MAPPED` for regular SSBO/UBO buffers
- No GL/Vulkan calls needed at call time — just return the stored pointer
- Caller contract: pointer valid until `SituationDestroyBuffer`; do not write through it

**Checklist:**
- [ ] Add declaration to `sit/situation_api.h`
- [ ] Implement in `sit/situation_impl_renderer.h` (trivial — expose the already-stored `mapped_ptr`)
- [ ] Add `SITUATION_ERROR_BUFFER_NOT_MAPPED` to `sit/situation_base_errno.h` + error string table
- [ ] Harness test: `graphics.readback_buffer_mapped_pointer` — create readback buf, get pointer, copy via cmd, read via pointer next frame, assert value

### 2.3 `SituationCmdFillBuffer`

Atomic counter reset between frames. Currently done via `SituationUpdateBuffer` which does a full CPU→GPU upload. `CmdFillBuffer` records the zero-fill into the command buffer so it runs on the GPU, avoiding the staging copy overhead.

**Declaration to add** (`sit/situation_api.h`):
```c
SITAPI SituationError SituationCmdFillBuffer(SituationCommandBuffer cmd,
    SituationBuffer buffer, size_t offset, size_t size, uint32_t value); // Fill a buffer range with a 4-byte repeating pattern (GPU-side).
```

**Implementation notes:**
- GL path: `glClearNamedBufferSubData(buf.gl_id, GL_R32UI, offset, size, GL_RED_INTEGER, GL_UNSIGNED_INT, &value)` — or record as a `SIT_OP_FILL_BUFFER` soft-command for replay on render thread
- Vulkan path: `vkCmdFillBuffer(cmd, buf.vk_buf, offset, size, value)` — already a first-class Vulkan command
- `offset` and `size` must be multiples of 4; validate and return `SITUATION_ERROR_INVALID_PARAM` otherwise
- Add `SIT_OP_FILL_BUFFER` opcode to the OpenGL soft command buffer

**Checklist:**
- [ ] Add opcode `SIT_OP_FILL_BUFFER` to `_SitGLOpcode` enum in `sit/situation_impl_decl.h`
- [ ] Add declaration to `sit/situation_api.h`
- [ ] Implement GL record path + execute path in `sit/situation_impl_renderer.h`
- [ ] Implement Vulkan path in `sit/situation_impl_renderer.h`
- [ ] Add trace ID in `sit/situation_base_trace.h`
- [ ] Harness test: `graphics.cmd_fill_buffer` — fill with known pattern, readback, assert

### 2.4 Explicit texture handle residency (low priority)

Quest's stub has `SituationMakeTextureHandleResident` / `SituationMakeTextureHandleNonResident`. Situation currently manages residency automatically at create/destroy time, which is correct for the common case. Quest would only need manual control if it hot-swaps large volumes of textures within a frame (not the NRC use case).

**Decision:** Skip for now. The automatic model is sufficient for Quest NRC. If this becomes needed, add:
```c
SITAPI SituationError SituationMakeTextureHandleResident(uint64_t handle);
SITAPI void SituationMakeTextureHandleNonResident(uint64_t handle);
```
Both would be GL-specific (`glMakeTextureHandleResidentARB`); on Vulkan, bindless residency is descriptor-table-based and already handled.

- [ ] Defer — revisit if Quest explicitly needs it

---

## Phase 3 — Already implemented, verify correctness

These were listed as "pending" in the old requirements doc but are now confirmed implemented. Just need a smoke test against the actual Quest integration.

| Function | Status | Notes |
|----------|--------|-------|
| `SituationCreateReadbackBuffer` | ✅ | Signature: `(size_t size, SituationBuffer* out)` |
| `SituationCmdCopyBuffer` | ✅ | Legacy form; prefer `SituationCmdCopyBufferEx(cmd, src, dst, src_off, dst_off, size)` |
| `SituationReadBuffer` | ✅ | Signature: `(SituationBuffer buf, void* dst, size_t size)` returns `SituationError` |
| `SituationCmdCopyBufferToTexture` | ✅ | Already in API, was listed as "future" in old doc |
| `SituationGetVirtualDisplayTexture` | ✅ | — |

- [ ] Smoke test: run Quest NRC test binary against the current DLL, confirm readback path works end-to-end

---

## Phase 4 — Version bump & docs

- [ ] Bump `SITUATION_VERSION_PATCH` after Phase 2 items land
- [ ] Add entries to `doc/UPDATELOG.md`
- [ ] Update this plan's checkboxes when done; move to `doc/plan/done/` when all phases complete

---

## Quick reference: Quest stub → real API name mapping

| Quest stub name | Real Situation API | Notes |
|-----------------|-------------------|-------|
| `SituationGetTime()` | `SituationTimerGetTime()` | returns `double` |
| `SituationGetDeltaTime()` | `SituationGetFrameTime()` | returns `float` |
| `SituationCmdBindShaderBuffer(cmd, binding, buf)` | `SituationCmdBindDescriptorSet(cmd, set_index, buf)` | — |
| `SituationCmdPipelineBarrier(cmd)` | `SituationCmdPipelineBarrier(cmd, src, dst)` | needs flag args |
| `SituationCreateBuffer(usage, data, size)` | `SituationCreateBuffer(size, data, flags, &out)` | different order |
| `SituationCreateTexture(w, h, fmt, usage)` | `SituationCreateTextureEx(img, false, flags, &out)` | needs SituationImage |
| `SituationAcquireFrameCommandBuffer()` → `bool` | → `SituationError` | `SITUATION_SUCCESS` = ready |
| `SituationCreateTexture3D(...)` | ❌ not yet — see Phase 2.1 | — |
| `SituationGetBufferMappedPointer(buf)` | ❌ not yet — see Phase 2.2 | — |
| `SituationCmdFillBuffer(cmd, buf, off, sz, val)` | ❌ not yet — see Phase 2.3 | — |
