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

**Harness:** `transfer.texture_barrier_validation`, `transfer.blit_texture_*`, `transfer.copy_texture_*`.

## Buffer → texture upload

1. CPU or compute fill source buffer (`SITUATION_BUFFER_USAGE_TRANSFER_SRC`).
2. Texture barrier into `SITUATION_TEXTURE_LAYOUT_TRANSFER_DST`.
3. `SituationCmdCopyBufferToTexture`.
4. Barrier to `SHADER_READ` before sampling.

**Harness:** `transfer.copy_buffer_to_texture_*`.

## Deferred (not yet cookbook-tested)

- Color attachment → texture copy/readback without hidden transitions.
- Full SSBO → vertex shader cookbook row in transfer module (graphics interop test planned in Phase 4.1D).
