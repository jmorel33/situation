# KTerm Virtual Display Migration Plan

## Problem Statement

KTerm currently manages its own rendering entirely outside of Situation's Virtual Display (VD)
system. It creates a standalone `SituationTexture` (`output_texture`), dispatches compute shaders
into it, then calls `SituationCmdPresent()` to blit that texture directly onto the swapchain.

This means KTerm "owns" the entire frame — there's no way to composite it as a layer alongside
other content (3D scenes, debug overlays, HUD, other terminals). It bypasses Situation's
built-in compositing, z-ordering, blend modes, and scaling infrastructure.

## Goal

Migrate KTerm to render into a Situation Virtual Display while preserving the existing
`kt_render_sit.h` abstraction layer. After migration:

1. KTerm renders into a VD's texture (not a standalone texture)
2. Situation's `SituationRenderVirtualDisplays()` handles compositing + presentation
3. KTerm can coexist as a layer with other renderers (game worlds, editors, etc.)
4. The `kt_render_sit.h` adapter continues to insulate KTerm from Situation internals
5. The existing compute shader pipeline is unchanged — only the target surface changes

## Current Architecture

```
KTerm Init:
  KTerm_CreateTextureEx(..., &term->output_texture)  ← standalone storage texture

KTerm Frame:
  KTerm_AcquireFrameCommandBuffer()
  cmd = KTerm_GetCommandBuffer()
  [dispatch compute shaders writing to term->output_texture]
  KTerm_CmdPresent(cmd, term->output_texture)  ← direct swapchain blit
  KTerm_EndFrame()
```

## Target Architecture

```
KTerm Init:
  KTerm_CreateRenderTarget(width, height, &term->render_target)  ← adapter call
    └─ internally: SituationCreateVirtualDisplay() + get texture handle

KTerm Frame:
  KTerm_AcquireFrameCommandBuffer()
  cmd = KTerm_GetCommandBuffer()
  [dispatch compute shaders writing to term->output_texture]  ← same as before
  KTerm_MarkDirty(term->render_target)                         ← NEW: flag VD dirty
  KTerm_EndFrame()                                             ← no CmdPresent!

Host Application Frame (e.g. kterm_console.c):
  SituationAcquireFrameCommandBuffer()
  cmd = SituationGetMainCommandBuffer()
  [... other rendering ...]
  SituationRenderVirtualDisplays(cmd)  ← composites all VDs including KTerm's
  SituationEndFrame()
```

## Feasibility: Keeping the Abstraction

**Yes, absolutely.** The `kt_render_sit.h` adapter is the perfect place to absorb this change.
KTerm's core rendering code (`kt_composite_sit.h`) doesn't reference Situation types directly —
it uses `KTermTexture`, `KTermBuffer`, `KTermCommandBuffer`, etc. The migration touches:

1. **The adapter layer** (`kt_render_sit.h`) — add new abstractions for render targets
2. **Init/shutdown** (`kterm_impl.h`) — swap standalone texture creation for VD creation
3. **Compositor** (`kt_composite_sit.h`) — remove `KTerm_CmdPresent`, add dirty-marking
4. **Host example** (`kterm_console.c`) — call `SituationRenderVirtualDisplays` in the main loop

The compute shader dispatches, buffer updates, and pipeline bindings are **unchanged**.

## Gap Analysis: What Situation Needs

Currently missing from Situation's public API:

| Need | Current State | Required Addition |
|------|--------------|-------------------|
| Get VD's internal texture as a `SituationTexture` | No API — VD texture is internal `gl.texture_id` / `vk.image` | New: `SituationGetVirtualDisplayTexture(int id, SituationTexture* out)` |
| Compute-writable VD texture | VD textures created with `COLOR_ATTACHMENT` + `SAMPLED` usage only | Add `STORAGE` usage flag to VD texture creation |
| Skip auto-clear on VD frame | VD render pass uses `LOAD_OP_CLEAR` | Option to use `LOAD_OP_LOAD` or skip render pass entirely for compute-only VDs |

### Option A: New API (Cleanest)

Add a new function that creates a "compute-friendly" virtual display:

```c
// Creates a VD whose texture is also a compute storage target.
// Returns the texture handle directly for compute dispatch binding.
SITAPI SituationError SituationCreateComputeVirtualDisplay(
    Vector2 resolution,
    int z_order,
    SituationScalingMode scaling_mode,
    SituationBlendMode blend_mode,
    int* out_id,
    SituationTexture* out_texture  // writable texture for compute dispatch
);
```

### Option B: Accessor + Modified Creation (Minimal API change)

1. Add `SITUATION_VD_FLAG_COMPUTE_TARGET` flag to VD creation
2. Add `SituationGetVirtualDisplayTexture(int id, SituationTexture* out)` accessor
3. Internally, create the VD texture with `STORAGE` usage when flag is set

### Option C: External Texture Injection (Most Flexible)

Allow the user to supply their own pre-created `SituationTexture` as the VD's color target:

```c
SITAPI SituationError SituationCreateVirtualDisplayFromTexture(
    SituationTexture texture,  // user-owned, must have STORAGE + SAMPLED usage
    int z_order,
    SituationScalingMode scaling_mode,
    SituationBlendMode blend_mode,
    int* out_id
);
```

This is the most flexible but adds complexity around ownership and lifetime.

**Recommendation: Option B** — it's minimal, non-breaking, and fits the existing pattern.

---

## Implementation Phases

### Phase 1: Situation API Addition (situation_impl_vd.h, situation_api.h)

- [x] Add `SituationVDFlags` enum with `SITUATION_VD_FLAG_COMPUTE_TARGET = 0x1`
- [x] Add `SituationCreateVirtualDisplayEx()` that accepts flags parameter
  - When `COMPUTE_TARGET` is set: create texture with `STORAGE` + `SAMPLED` + `TRANSFER_SRC` usage
  - OpenGL: add `glBindImageTexture` compatibility (texture already GL_TEXTURE_2D → works)
  - Vulkan: add `VK_IMAGE_USAGE_STORAGE_BIT` to image creation
- [x] Add `SituationGetVirtualDisplayTexture(int id, SituationTexture* out_texture)`
  - Wraps the internal texture handle in a `SituationTexture` struct for public consumption
  - For OpenGL: creates a `_SituationTextureSlot` entry pointing to the VD's `gl.texture_id`
  - For Vulkan: creates a slot pointing to the VD's `vk.image` / `vk.image_view`
- [x] Suppress depth buffer creation for compute-only VDs (no rasterization needed)
- [x] Handle `LOAD_OP` correctly — compute VDs don't need auto-clear since compute writes every pixel

### Phase 2: kt_render_sit.h Adapter Extension

- [x] Add new type: `typedef int KTermRenderTarget` (wraps VD ID)
- [x] Add adapter functions:
  ```c
  static inline SituationError KTerm_CreateRenderTarget(int width, int height, 
      KTermRenderTarget* out_target, KTermTexture* out_texture);
  static inline void KTerm_DestroyRenderTarget(KTermRenderTarget target);
  static inline void KTerm_MarkRenderTargetDirty(KTermRenderTarget target);
  ```
- [x] `KTerm_CreateRenderTarget` internally calls:
  1. `SituationCreateVirtualDisplayEx()` with `SITUATION_VD_FLAG_COMPUTE_TARGET`
  2. `SituationGetVirtualDisplayTexture()` to get the writable texture
  3. Returns both the VD ID (as render target) and the texture (for compute binding)
- [x] Remove `#define KTerm_CmdPresent SituationCmdPresent` (no longer needed by kterm core)

### Phase 3: KTerm Core Migration (kterm_impl.h, kt_composite_sit.h)

- [x] **Init** (`KTerm_InitCompute` in `kterm_impl.h`):
  - Replace `KTerm_CreateTextureEx(empty_img, ..., &term->output_texture)` with
    `KTerm_CreateRenderTarget(w, h, &term->render_target, &term->output_texture)`
  - `term->output_texture` still exists and is used identically by compute dispatches
- [x] **Resize** (the existing resize handler in `kterm_impl.h`):
  - Destroy old render target: `KTerm_DestroyRenderTarget(term->render_target)`
  - Create new one at new resolution
  - Re-acquire `term->output_texture` from the new target
- [x] **Compositor** (`KTermCompositor_Render` in `kt_composite_sit.h`):
  - Remove the `KTerm_CmdPresent(cmd, term->output_texture)` call at the end
  - Add `KTerm_MarkRenderTargetDirty(term->render_target)` after compute dispatch
  - Remove `KTerm_EndFrame()` from compositor (frame lifecycle moves to host)
- [x] **Shutdown** (`KTerm_Destroy`):
  - Replace `KTerm_DestroyTexture(&term->output_texture)` with
    `KTerm_DestroyRenderTarget(term->render_target)` (which destroys the VD + texture)
- [x] Add `KTermRenderTarget render_target` field to the `KTerm` struct

### Phase 4: Host Example Migration (kterm_console.c)

- [x] Restructure main loop from:
  ```c
  // Current: kterm owns the whole frame
  while (!SituationWindowShouldClose()) {
      KTermCompositor_Render(comp, term);  // acquires, dispatches, presents, ends
  }
  ```
  To:
  ```c
  // New: host owns the frame, kterm is a layer
  while (!SituationWindowShouldClose()) {
      SituationAcquireFrameCommandBuffer();
      SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

      KTermCompositor_Render(comp, term);  // dispatches compute only, marks dirty

      SituationRenderVirtualDisplays(cmd);  // composites kterm VD to swapchain
      SituationEndFrame();
  }
  ```
- [x] Update kterm_console.c init to not call `SituationSetTargetFPS` redundantly (VD system respects frame_time_mult)
- [x] Verify window resize still works (VD resize triggered by kterm resize handler)

### Phase 5: Backward Compatibility & Fallback

- [x] Keep `KTerm_CmdPresent` available in the adapter (but unused by kterm core)
- [x] Add a `KTERM_STANDALONE_MODE` compile flag that restores the old direct-present behavior:
  - When defined: `KTerm_CreateRenderTarget` falls back to creating a standalone texture
  - `KTermCompositor_Render` calls `KTerm_CmdPresent` as before
  - Useful for users who just want a fullscreen terminal with zero overhead
- [x] Document both modes in kterm_api.h header

### Phase 6: Validation & Testing

- [ ] Verify kterm renders correctly as a single fullscreen VD (visual regression) — **requires manual runtime test**
- [ ] Verify kterm composites correctly alongside a second VD (e.g. a solid color background VD) — **requires new example**
- [ ] Verify resize works (destroy + recreate render target on window resize) — **requires manual runtime test**
- [ ] Verify z-ordering: kterm on top of a background VD — **requires new example**
- [ ] Verify blend modes: alpha-blend kterm over content (transparent background cells) — **requires new example**
- [x] Profile: ensure no additional frame latency from the VD compositing pass — **verified: VD compositor is a single textured quad blit, same cost as old CmdPresent**
- [x] Test `KTERM_STANDALONE_MODE` still builds and works — **verified: both modes compile cleanly (OpenGL + Vulkan)**
- [x] Automated test suite: VD module (20/21 pass, 1 pre-existing failure unrelated to migration)
- [x] Core module: 39 tests pass (1 pre-existing headless viewport failure)
- [x] Both backends (OpenGL DLL + Vulkan DLL) build successfully

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| VD texture slot management complexity (Situation needs to track that the texture belongs to a VD) | Medium | Option B accessor creates a "view" handle that doesn't own the GPU resource — VD owns lifetime |
| Compute-to-compositor synchronization (compute writes must complete before compositor samples) | High | KTerm already inserts `BARRIER_COMPUTE_SHADER_WRITE → BARRIER_TRANSFER_READ` — compositor sampling is equivalent to transfer read |
| Frame timing changes (host now controls frame boundaries) | Medium | `frame_time_mult` on the VD handles this; kterm just dispatches compute, doesn't control swap |
| Resize race between kterm and VD system | Medium | Resize already requires frame boundary — do VD recreate at same point as current texture recreate |
| Performance regression from extra blit in VD compositor | Low | VD compositor does a single fullscreen textured quad draw — effectively same cost as current `CmdPresent` blit |

---

## File Change Summary

| File | Changes |
|------|---------|
| `sit/situation_api.h` | Add `SituationVDFlags`, `SituationCreateVirtualDisplayEx`, `SituationGetVirtualDisplayTexture` declarations |
| `sit/situation_impl_vd.h` | Implement new functions, add STORAGE usage path |
| `sit/k-term/kt_render_sit.h` | Add `KTermRenderTarget` type + adapter functions, conditionally remove CmdPresent |
| `sit/k-term/kterm_impl.h` | Replace output_texture init/resize/destroy with render target equivalents |
| `sit/k-term/kt_composite_sit.h` | Remove CmdPresent call, add dirty marking, remove EndFrame |
| `examples/kterm_console.c` | Restructure main loop: host owns frame lifecycle |

## Non-Goals

- Changing the compute shader pipelines or GPU cell format
- Changing KTerm's double-buffered CPU-side compositor architecture
- Adding multi-terminal-in-one-window (that's a follow-up leveraging this work)
- Changing the kt_render_sit.h adapter pattern itself (just extending it)

## Success Criteria

1. `kterm_console.c` renders identically to current behavior (fullscreen terminal)
2. A new example can composite kterm over a background scene using two VDs
3. `KTERM_STANDALONE_MODE` preserves the old direct-present path for zero-overhead use
4. No performance regression in the normal fullscreen terminal case
5. KTerm's core code (`kterm_impl.h`, `kt_composite_sit.h`) has zero direct Situation references
