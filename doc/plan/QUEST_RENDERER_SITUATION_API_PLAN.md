# Quest Spectral Renderer (QSR) — Situation API Gaps Plan

**Last reviewed:** 2026-06-22 (cross-checked against Situation **v2.4.335**)
**Original audit:** 2026-06-13 vs v2.4.x  
**Consumer:** Quest Spectral Renderer — `host/situation.h`, `host/nrc_pipeline.h` (Quest repo / stub; not in this tree)  
**Scope:** Gap between what Quest's stub expects and what Situation exposes. Three categories: **stub name fixes (Quest side)**, **missing Situation API**, **already shipped**.

**Related plans (AAA / product context):**

| Plan | QSR relevance |
|------|----------------|
| `doc/plan/v2.5-api-expansion.md` **Phase K** | ✅ Async readback shipped — NRC counter path |
| `doc/plan/plan_handles_ssbo.md` | Compute SSBO/BDA, bindless textures (if QSR uses `GetTextureHandle` on volumes) |
| `doc/plan/plan_dynamic_ubo.md` | Per-dispatch uniform rings (lower priority than QSR Phase 2) |
| `doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md` | User compute shader layout profiles (DUAL_SSBO, UBO_SSBO) |

---

## Status summary (v2.4.335)

| Category | Count | Action |
|----------|-------|--------|
| ✅ Already implemented, name matches | ~20 functions | Nothing on Situation side |
| ⚠ Already implemented, wrong name in Quest stub | 6 items | **Fix Quest `host/situation.h`** (Phase 1) |
| ❌ Not implemented in Situation | **3 blocking** + 1 deferred | **Phase 2** (Situation) |
| ✅ Shipped since original plan, not reflected in old doc | Phase K readback | Harness `test_async_buffer_readback` |

**Blocking QSR integration today (Situation):**

1. **`SituationCreateTexture3D`** — NRC radiance cache volumes  
2. **`SituationGetBufferMappedPointer`** — optional hot-path optimization  
3. **`SituationCmdFillBuffer`** — GPU-side atomic counter reset  

**Not blocking:** manual bindless residency (Phase 2.4 defer). Phase 1 stub renames are **Quest-side only**.

---

## Phase 1 — Fix name mismatches in Quest stub (`host/situation.h`)

These work in Situation under a different name. **Situation is correct; update the Quest stub.**

- [ ] `SituationGetTime()` → `SituationTimerGetTime()` — returns `double` seconds
- [ ] `SituationGetDeltaTime()` → `SituationGetFrameTime()` — returns `float` seconds
- [ ] `SituationCmdBindShaderBuffer(cmd, binding, buf)` → `SituationCmdBindDescriptorSet(cmd, set_index, buf)` — parameter is **set index**, not GL binding point; for dynamic offsets use `SituationCmdBindDescriptorSetDynamic`
- [ ] `SituationCmdPipelineBarrier(cmd)` → `SituationCmdPipelineBarrier(cmd, src_flags, dst_flags)` — or prefer `SituationCmdPipelineBarrierEx` / `SituationCmdBufferBarrier` for compute↔copy ordering
- [ ] `SituationCreateBuffer(usage, data, size)` → `SituationCreateBuffer(size, data, usage_flags, out_buffer)` — out-param, different arg order
- [ ] `SituationAcquireFrameCommandBuffer()` — stub expects `bool`; real API returns `SituationError` (`SITUATION_SUCCESS` = ready)

### Texture creation in stub

Quest stub: `SituationCreateTexture(w, h, fmt, usage)`. Real API uses `SituationImage` + `SituationCreateTextureEx`:

```c
SituationImage img = { .width = w, .height = h, .channels = 4, .data = NULL };
SituationCreateTextureEx(img, false,
    SITUATION_TEXTURE_USAGE_STORAGE | SITUATION_TEXTURE_USAGE_SAMPLED,
    &out_tex);
```

- [ ] Update Quest stub wrapper to use `SituationCreateTextureEx` with null-data `SituationImage`

**Phase 1 exit:** Quest builds against real headers with no fake symbol names. No Situation code changes required.

---

## Phase 2 — Implement missing API in Situation

Genuinely absent from `sit/situation_api.h` (verified grep v2.4.335). Follow **`v2.5-api-expansion.md` §O** parity checklist for each item.

### 2.1 `SituationCreateTexture3D` — **P0 for NRC**

Quest needs 3D textures for NRC radiance cache grids (`w×h×d` RGBA32F volume).

**Declaration:**
```c
SITAPI SituationError SituationCreateTexture3D(int width, int height, int depth,
    SituationTextureUsageFlags flags, SituationTexture* out_texture);
```

**Implementation notes:**
- GL: `glCreateTextures(GL_TEXTURE_3D, …)` + `glTextureStorage3D(…, GL_RGBA32F, w, h, d)`
- VK: `VkImageCreateInfo` — `imageType = VK_IMAGE_TYPE_3D`, `extent = {w,h,d}`, `arrayLayers = 1`
- Format: RGBA32F first (NRC use case); generalize later
- Extend `_SituationTextureSlot` (e.g. `depth`, `is_3d`) — **no** 3D in registry today (grep clean)
- `SituationDestroyTexture` — generic cleanup should extend for 3D views
- **`SituationGetTextureHandle`** — must work for 3D bindless/storage indexing if compute writes via bindless table
- Barriers: document who transitions 3D image layout (QSR compute vs library helper)

**Checklist:**
- [ ] Declaration in `sit/situation_api.h`
- [ ] GL + Vulkan paths in `situation_impl_renderer.h`
- [ ] Error strings + trace ID
- [ ] Harness: `graphics.create_texture_3d_basic` — create, optional compute write, destroy
- [ ] UPDATELOG + `situation_api.md`

### 2.2 `SituationGetBufferMappedPointer` — **P1 optimization**

Zero-copy readback — skip `memcpy` inside `SituationReadBuffer` on hot NRC telemetry path.

**Declaration:**
```c
SITAPI void* SituationGetBufferMappedPointer(SituationBuffer buffer);
```

**Notes:**
- Valid for `SituationCreateReadbackBuffer` only (`mapped_ptr` already stored internally)
- NULL + `SITUATION_ERROR_BUFFER_NOT_MAPPED` for other buffer types
- Caller contract: valid until `SituationDestroyBuffer`; read after `_SituationFlushRenderThread()` / frame boundary (same as `SituationReadBuffer` since v2.4.x flush hardening)

**Checklist:**
- [ ] API + `SITUATION_ERROR_BUFFER_NOT_MAPPED`
- [ ] Harness: `graphics.readback_buffer_mapped_pointer`
- [ ] Document coherence hazards in `situation_api.md`

### 2.3 `SituationCmdFillBuffer` — **P1 perf**

GPU-side fill for atomic counter reset between dispatches (vs `SituationUpdateBuffer` zero upload).

**Declaration:**
```c
SITAPI SituationError SituationCmdFillBuffer(SituationCommandBuffer cmd,
    SituationBuffer buffer, size_t offset, size_t size, uint32_t value);
```

**Notes:**
- VK: `vkCmdFillBuffer` — offset/size multiples of 4
- GL: `SIT_OP_FILL_BUFFER` soft command or `glClearNamedBufferSubData`
- Also listed in `v2.5-api-expansion.md` §P parking lot — **promote here for QSR**

**Checklist:**
- [ ] `SIT_OP_FILL_BUFFER` opcode (GL)
- [ ] VK + GL record/execute
- [ ] Harness: `graphics.cmd_fill_buffer`

### 2.4 Explicit texture handle residency — **defer**

Quest stub: `SituationMakeTextureHandleResident` / `NonResident`. Situation manages residency at create/destroy; sufficient for NRC.

- [x] **Decision:** defer unless QSR hot-swaps large texture volumes mid-frame

---

## Phase 3 — Already implemented (verify / harness)

Confirmed in `situation_api.h` v2.4.335. Original plan listed these; **Phase K (v2.4.66+)** added readback path with harness coverage.

| Function | Status | Notes |
|----------|--------|-------|
| `SituationCreateReadbackBuffer` | ✅ | `(size_t size, SituationBuffer* out)` |
| `SituationCmdCopyBuffer` / `CmdCopyBufferEx` | ✅ | Prefer **Ex** with explicit src/dst offsets |
| `SituationReadBuffer` | ✅ | Returns `SituationError`; flush render thread before read (v2.4.x) |
| `SituationCmdCopyBufferToTexture` | ✅ | Buffer → 2D texture upload |
| `SituationGetVirtualDisplayTexture` | ✅ | VD compute target handle |

**Harness (Situation repo):**
- [x] `test_async_buffer_readback` — create readback, copy, read, assert (`tests/harness/test_graphics.c`)

**Still open (Quest repo):**
- [ ] Smoke test: Quest NRC binary against current `situation_vulkan.dll` / static lib end-to-end
- [ ] Confirm NRC uses `SituationCmdCopyBufferEx` + frame-boundary read semantics documented in **v2.5 §K.3**

---

## Phase 4 — Version bump & docs (when Phase 2 lands)

- [ ] Bump `SITUATION_VERSION_PATCH` after 2.1–2.3 ship
- [ ] `doc/UPDATELOG.md` entries per API
- [ ] Move this plan to `doc/plan/done/` when Phase 1 (Quest) + Phase 2 (Situation) + Phase 3 smoke are complete

---

## Phase 5 — Cross-check: AAA roadmap necessities for QSR (not in original plan)

Items QSR likely needs long-term but **not** tracked as stub gaps. Use to avoid duplicate work with handles/SSBO + sprite plans.

| Need | Situation today | Plan owner | QSR priority |
|------|-----------------|------------|--------------|
| Compute SSBO binds (NRC scene data) | ✅ `SituationCmdBindDescriptorSet`, DUAL_SSBO profile | SPIR-V parity doc | **Now** |
| Async GPU→CPU counters | ✅ Phase K | v2.5 §K | **Now** |
| 3D storage / sampled volumes | ❌ Phase 2.1 | **This plan** | **P0** |
| Bindless `GetTextureHandle` on 3D | ⚠ slot write path exists for 2D; 3D untested | handles **D0** if bindless sampling | After 2.1 |
| `GetBufferDeviceAddress` | ✅ | handles B | If NRC moves to pull/BDA |
| Dynamic UBO ring (many param blocks/frame) | DU0 only | `plan_dynamic_ubo.md` DU1 | Medium |
| Compute↔graphics timeline sync | 🔲 AAA §7 | `AAA_ARCHITECTURE_PLAN.md` | If QSR shares queue with presentation |
| Scaler sprite system | 🔲 Phase G | `plan_handles_ssbo.md` | Product UI layer on top of QSR — separate |

**Non-regressive rule for QSR API work:** ship **harness tests per Phase 2 item** before Quest dogfood; do not bundle 3D textures + fill + mapped pointer in one unreviewable patch.

---

## Quick reference: Quest stub → real API

| Quest stub name | Real Situation API | Status |
|-----------------|-------------------|--------|
| `SituationGetTime()` | `SituationTimerGetTime()` | ⚠ stub fix |
| `SituationGetDeltaTime()` | `SituationGetFrameTime()` | ⚠ stub fix |
| `SituationCmdBindShaderBuffer` | `SituationCmdBindDescriptorSet` / `…Dynamic` | ⚠ stub fix |
| `SituationCmdPipelineBarrier(cmd)` | `SituationCmdPipelineBarrier(cmd, src, dst)` | ⚠ stub fix |
| `SituationCreateBuffer(usage, data, size)` | `SituationCreateBuffer(size, data, flags, &out)` | ⚠ stub fix |
| `SituationCreateTexture(w,h,fmt,usage)` | `SituationCreateTextureEx(img, …)` | ⚠ stub fix |
| `SituationAcquireFrameCommandBuffer()` → `bool` | → `SituationError` | ⚠ stub fix |
| `SituationCreateTexture3D` | ❌ Phase 2.1 | **Situation** |
| `SituationGetBufferMappedPointer` | ❌ Phase 2.2 | **Situation** |
| `SituationCmdFillBuffer` | ❌ Phase 2.3 | **Situation** |
| Readback / copy / VD texture | ✅ Phase 3 / Phase K | Done |

---

## Recommended order of work

1. **Quest Phase 1** — stub rename pass (unblocks compile against real headers)  
2. **Situation 2.1** — `CreateTexture3D` + harness (**unblocks NRC volume cache**)  
3. **Situation 2.3** — `CmdFillBuffer` (cheap win for counter reset)  
4. **Situation 2.2** — mapped pointer (optimize after correctness proven)  
5. **Quest Phase 3 smoke** — NRC binary on v2.4.335+ static/Vulkan build  
6. **Parallel (optional):** handles **D0** only if QSR compute must sample 3D via bindless array — prove with harness before QSR depends on it
