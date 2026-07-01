# Renderer Command Stack — Status & Navigation

**Last updated:** v2.4.398 (VD-4b MSAA Phase 0 prep)

This document is the **status map** for the bolstered command buffer: what shipped in v2.4.x, what is deferred, and where to read signatures. Per-command GL/VK notes live in **[situation_command_reference.md](situation_command_reference.md)**. Plan history: **[plan/renderer_bolster_plan.md](plan/renderer_bolster_plan.md)**.

---

## How recording works

```text
SituationPollInputEvents()
SituationUpdateTimers()
SituationAcquireFrameCommandBuffer()
SituationCommandBuffer cmd = SituationGetMainCommandBuffer()
  → SituationCmd* …
SituationEndFrame()
```

See **[architecture.md — Frame Loop Contract](architecture.md#frame-loop-contract-v2484)** for timing and capture rules.

**Configure vs command:** Virtual Display quality knobs (**sampler, clear, update mode**) are **configure APIs** (`SituationSetVirtualDisplay*`), not recorded commands in v1. Render-pass attachment behavior uses **`SituationRenderPassInfo`** + **`SituationRenderPassInfoInherit`**.

---

## Command stack status (v2.4.398)

| Category | Status | Commands / notes |
|----------|--------|------------------|
| **Render pass** | ✅ Shipped | `SituationCmdBeginRenderPass`, `SituationCmdEndRenderPass`; helpers `SituationRenderPassInfoDefault`, `SituationRenderPassInfoLoad`, `SituationRenderPassInfoInherit`, `SituationRenderPassConfigurationKey`. Main window: classic VkRenderPass (VK). **VD path:** dynamic rendering (VK), FBO (GL). |
| **Mid-pass clear** | ✅ Shipped (Phase 1) | `SituationCmdClear`, `ClearColor`, `ClearDepth`, `ClearStencil`, `ClearDepthStencil`. Require active render pass. Stencil mid-pass: `NOT_IMPLEMENTED` until stencil exposure is consistent. |
| **Viewport / scissor** | ✅ Shipped | `SetViewport`, `SetScissor`, `SetViewportIndexed`, `SetScissorIndexed`. |
| **Raster state** | ✅ Shipped (Phase 6 @ v2.4.187) | `PushRasterState` / `PopRasterState`, cull, front face, depth test/write, depth bias, polygon mode, line width, blend, color mask, stencil. **`SituationCmdSetMultisampleState`:** records + push/pop (**v2.4.256**); **no visible MSAA** until VD-4b attachments (**v2.5 default**). |
| **Binding / draw** | ✅ Shipped | Pipelines, descriptor sets, textures, vertex/index buffers, `Draw`, `DrawIndexed`, `DrawIndirect`, `DrawIndexedIndirect`, mesh/quad/texture/text helpers. **`BindIndexBufferEx`:** 16/32-bit indices (Phase 8). |
| **Compute** | ✅ Shipped | `BindComputePipeline`, `BindComputeTexture`, `Dispatch`, `DispatchEx`, `DispatchIndirect`. |
| **Synchronization** | ✅ Shipped (Phase 3) | `PipelineBarrier` (legacy flags), `PipelineBarrierEx`, `BufferBarrier`, `TextureBarrier`. Cookbook: **[RENDERER_BARRIER_COOKBOOK.md](misc/RENDERER_BARRIER_COOKBOOK.md)**. |
| **Transfer** | ✅ Shipped (Phase 4) | `CopyBuffer` / `CopyBufferEx`, `CopyTexture`, `BlitTexture`, `CopyBufferToTexture`, `CopyTextureToBuffer`. Color 2D mip 0 slice; explicit barriers required. Depth/stencil blit deferred. |
| **Present / debug** | ✅ Shipped | `SituationCmdPresent`, `BeginDebugGroup`, `EndDebugGroup`. |
| **Push constants** | ⚠️ Partial (Phase 9 open) | `SetPushConstant`, `SetPushConstantData` exist; GL `SetPushConstantData` path incomplete; layout metadata still raw. |
| **GPU queries / profiling** | ✅ Shipped (Phase 10) | **P10.0–P10.4:** frame phases, spike count, overlay, histogram, `GetFrameProfile`, Tracy CPU zones (opt-in), P10.3 internal GPU zones, **P10.4 user query pools**. |
| **VD configure (non-Cmd)** | ✅ Shipped (VD-1…VD-5) | `SituationSetVirtualDisplayAttachmentDefaults`, `ClearColor`, `Sampler`, `MaxAnisotropy`, `MipLevels`, `UpdateMode`, `MemoryHint` — see **[guide/virtual_display.md](guide/virtual_display.md)**. **MSAA quality:** **`SituationMultisampleQuality`** types @ v2.4.398; configure API + attachments **VD-4b (v2.5)**. |
| **Renderer behavior policy** | ✅ Shipped (Phase 14 @ v2.4.391) | `SituationRendererBehaviorPolicyDefault`, `Set/Push/PopRendererBehavior`. Axes: blit filter downgrade, transfer usage fallback, assisted layout hints, validation WARN/COMPAT logging. Cookbook: **[RENDERER_BARRIER_COOKBOOK.md](misc/RENDERER_BARRIER_COOKBOOK.md)**; guide: **[guide/renderer_bolster.md](guide/renderer_bolster.md)** § Workflow 3b. |
| **Attachment readback (3b)** | ✅ Shipped @ v2.4.392 | `TextureBarrier` `COLOR_ATTACHMENT` ↔ `TRANSFER_*` on transfer-capable color textures; harness `transfer.render_target_readback`. |
| **User render target (3c)** | ✅ Shipped @ v2.4.393 | `SituationCreateRenderTarget`, pass routing via `SituationRenderPassInfo.render_target`, `SituationReadRenderTarget`; harness `--module render_target`. MSAA deferred. |
| **Deprecated wrappers** | ✅ Kept (Phase 13 review) | `SituationCmdCopyBuffer`, `SituationCmdDispatch`, `SituationCmdPipelineBarrier`, `SituationCmdBindIndexBuffer`, `SituationMemoryBarrier`, legacy render-to-display — prefer `*Ex` / modern names. |

**Legend:** ✅ shipped and tested on GL+VK (unless noted GL-only); ⚠️ partial; 🔲 plan-only / v2.5.

---

## Full command catalog

**Authoritative signatures and tier matrix:** **[situation_command_reference.md](situation_command_reference.md)** (72 active + 7 deprecated @ v2.4.341+).

Regenerate API index: `python tools/generate_api_index.py`.

---

## Related user guides

| Topic | Document |
|-------|----------|
| When to use advanced commands | **[guide/renderer_bolster.md](guide/renderer_bolster.md)** |
| Virtual displays (non-Cmd configure) | **[guide/virtual_display.md](guide/virtual_display.md)** |
| Barriers cookbook | **[misc/RENDERER_BARRIER_COOKBOOK.md](misc/RENDERER_BARRIER_COOKBOOK.md)** |
| Transfer tests module | `tests/harness/test_transfer.c` (`--module transfer`) |
| Render target tests | `tests/harness/test_render_target.c` (`--module render_target`) |

---

## Open bolster work (not command-stack blockers)

- Phase 9 — push constant layout / GL parity  
- **VD-4b** — MSAA attachments + resolve + **`SituationSetVirtualDisplayMultisampleQuality`** (Phase 0 types/wiring ✅ v2.4.398)
