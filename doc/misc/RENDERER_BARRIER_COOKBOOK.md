# Renderer barrier cookbook (Phase 3 follow-up)

Short, harness-backed recipes for explicit synchronization. Each case uses public command-buffer APIs only.

## Compute indirect buffer → indirect dispatch

1. Fill `SituationDispatchIndirectCommand` in a buffer with `SITUATION_BUFFER_USAGE_INDIRECT_BUFFER`.
2. `SituationCmdPipelineBarrierEx` or `SituationCmdBufferBarrier`: compute write → indirect-command read.
3. `SituationCmdDispatchIndirect`.

**Harness:** `compute.dispatch_indirect_compute_generated`, `compute.dispatch_indirect_buffer_barrier`.

## Compute SSBO → graphics vertex read

1. Compute shader writes SSBO.
2. Global or buffer barrier: `SITUATION_BARRIER_COMPUTE_SHADER_WRITE` → `SITUATION_BARRIER_VERTEX_SHADER_READ` (or shader-read stage).
3. Bind SSBO descriptor set; draw.

**Harness:** SPIR-V / SSBO graphics tests in `test_graphics_spirv.c` and related graphics filters.

## Compute storage image → fragment sample

1. `SituationCmdBindComputePipeline` + `SituationCmdBindComputeTexture`; dispatch.
2. `SituationCmdPipelineBarrier`: compute write → transfer read (or shader read per path).
3. Graphics pass: sample or `SituationCmdDrawTexture`.

**Harness:** `graphics.compute_image_write` (graceful skip when driver cannot bind storage images).

## Transfer texture layout → blit/copy

1. `SituationCmdTextureBarrier` with explicit `old_layout` / `new_layout` (e.g. shader read → transfer src).
2. `SituationCmdBlitTexture` or `SituationCmdCopyTexture`.
3. Optional barrier out of transfer usage before next draw.

**Strict default (recommended):** textures must declare `SITUATION_TEXTURE_USAGE_TRANSFER_SRC` / `TRANSFER_DST` at create time.

**Opt-in (Phase 14 Slice 2):** `SituationCmdSetRendererBehavior` with `transfer_usage = SIT_TRANSFER_USAGE_COMPATIBLE_FALLBACK` allows **sampled RGBA8 color sources** without `TRANSFER_SRC` for read-only transfer commands. Destination `TRANSFER_DST` remains required. Buffers stay strict. Log warnings when `validation >= SIT_RENDERER_VALIDATION_WARN`.

**Opt-in (Phase 14 Slice 3):** `texture_layout = SIT_TEXTURE_LAYOUT_ASSISTED` tracks a **layout hint** per texture (updated by `SituationCmdTextureBarrier` and transfer commands). When a copy/blit needs transfer-src/dst layouts and the hint differs, the library inserts transition barriers before the transfer. Attachment and present layouts are **not** synthesized (Phase 3b remains explicit in strict mode).

**Validation tone (Phase 14 Slice 4):** set `validation = SIT_RENDERER_VALIDATION_WARN` (or `COMPAT`, equivalent in v2.4) to emit `renderer behavior:` warnings when a policy fallback runs. Warnings respect **`SituationSetTraceLogLevel`** — use `SIT_LOG_INFO` or `SIT_LOG_WARNING` as the minimum level. `STRICT` suppresses fallback logs entirely.

**Harness:** `transfer.texture_barrier_validation`, `transfer.blit_texture_*`, `transfer.copy_texture_*`, `transfer.behavior_transfer_usage_*`, `transfer.behavior_layout_assisted_transfer`, `transfer.behavior_validation_*`.

## Buffer → texture upload

1. CPU or compute fill source buffer (`SITUATION_BUFFER_USAGE_TRANSFER_SRC`).
2. Texture barrier into `SITUATION_TEXTURE_LAYOUT_TRANSFER_DST`.
3. `SituationCmdCopyBufferToTexture`.
4. Barrier to `SHADER_READ` before sampling.

**Harness:** `transfer.copy_buffer_to_texture_*`.

## Render target readback (Phase 3b)

After rendering to a color attachment (Virtual Display pass today; user `SituationRenderTarget` in v2.5):

1. `SituationCmdEndRenderPass` — target layout hint becomes `COLOR_ATTACHMENT`.
2. `SituationCmdTextureBarrier` — `COLOR_ATTACHMENT` → `TRANSFER_SRC` (texture must include `TRANSFER_SRC`).
3. `SituationCmdCopyTextureToBuffer` (or `CopyTexture` / `BlitTexture`).
4. Optional barrier back to `SHADER_READ` before sampling.

**Strict default:** no hidden post-pass transitions on mip-0 targets — you own step 2. Under **`SIT_TEXTURE_LAYOUT_ASSISTED`**, step 2 can be synthesized from the layout hint when it still reads `COLOR_ATTACHMENT`.

**Not this path:** swapchain / `SituationReadFramebuffer` / `SituationLoadImageFromScreen` — use those for screen capture, not `CopyTextureToBuffer` on the swapchain image.

**Harness:** `transfer.render_target_readback` (VD path), `render_target.*` (user `SituationRenderTarget`).

## User render target (Phase 3c)

For offscreen work **without** Virtual Display compositor semantics:

1. `SituationCreateRenderTarget` → `SituationRenderPassInfoForRenderTarget(rt, clear_color)`.
2. `SituationCmdBeginRenderPass` / draw / `SituationCmdEndRenderPass` — `info.render_target` overrides `display_id`.
3. Readback: **`SituationReadRenderTarget`** (blocking), or barrier + `CopyTextureToBuffer` on `SituationGetRenderTargetTexture`.

Same layout contract as Phase 3b: end pass leaves `COLOR_ATTACHMENT` hint; you own transfer barriers in strict mode.

**Harness:** `render_target.render_target_read_render_target`, `render_target.render_target_cmd_readback`.

## Deferred (not yet cookbook-tested)
- Full SSBO → vertex shader cookbook row in transfer module (graphics interop test planned in Phase 4.1D).
