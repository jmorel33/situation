# Renderer Bolster Plan

**Status:** in progress — **P10.1 ✅ v2.4.394**. **P10.2 ✅ v2.4.395** (Tracy opt-in). **P10.3 ✅ v2.4.396** (GPU timestamp zones). **P10.4 ✅ v2.4.397** (public query pool). **VD-4b Phase 0 ✅ v2.4.398** (MSAA prep — types + internal wiring; attachments deferred). **Open next:** Phase 9, Phase 11-bis **MM-0**, Phase 12. **VD-4b** MSAA attachments remain **v2.5-gated**.  
**Scope:** command-buffer, render-pass, synchronization, copy/blit, indirect draw, raster-state, Virtual Display target control, **multi-monitor presentation via WDM + VD compositing (v2.4.x)**, **frame profiling / GPU queries**, and query API hardening for the existing OpenGL 4.6 / Vulkan 1.4 renderer stack.  
**Target line:** v2.4.x worksheet; keep coordinated with v2.5 planning, but do not treat this as the v2.5 boundary.  
**Primary files:** `sit/situation_api.h`, `sit/situation_impl_decl.h`, **`sit/situation_impl_renderer_frame_cmd.h`** (Cmd*, frame loop), **`sit/situation_impl_renderer_lc.h`** (init/lifecycle), **`sit/situation_impl_renderer_resources.h`** (resources + transfer Cmd*), `sit/situation_impl_vd.h`, **`sit/situation_impl_wdm.h`**, `sit/situation_base_trace.h`, `tests/harness/test_graphics.c`, `tests/harness/test_virtual_display.c`, **`tests/harness/test_advanced.c`**, `tests/harness/test_compute.c` (Phase -1 pilot), `tests/harness/test_transfer.c` (Phase 4.1 pilot). Orchestrator: `sit/situation_impl_renderer.h`.
**Related plans:** `doc/plan/v2.5-api-expansion.md`, **`doc/plan/PHASE_14_RENDERER_BEHAVIOR_POLICY.md`**, `doc/plan/AAA_ARCHITECTURE_PLAN.md` **§6 (profiling — canonical detail)**, `doc/plan/QUEST_RENDERER_SITUATION_API_PLAN.md` (QSR compute readback), `doc/plan/plan_handles_ssbo.md` (Phase G sprites — pass timing), **`doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`** (main-window HDR10 — feeds VD pass clears on Vulkan), `doc/plan/RENDERER_AUDIT_PLAN.md`, `doc/plan/TEST_HARNESS_GRAPHICS_UPGRADE.md`, `doc/plan/VD_EXTRACTION_PLAN.md`, [`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`](VULKAN_SHADER_CACHE_PLAN.md), [`doc/plan/LIBRARY_RECOVERY_PLAN_244.md`](LIBRARY_RECOVERY_PLAN_244.md) **§B.5 (internal VK 2D lifecycle)**.  
**Constraint:** keep Situation's public API backend-neutral; tag exceptions as `[GL only]`, `[VK only]`, or `[deferred]`.

---

## How to use this file

1. Execute phases in order, starting with Phase -1, unless a maintainer explicitly reprioritizes.
2. Treat signatures as proposals until implemented; keep final public declarations in `situation_api.h` as single-line `SITAPI` entries with EOL comments.
3. Every shipped command must have both a public API entry and backend behavior documented here.
4. Every command-buffer feature needs OpenGL and Vulkan tests unless explicitly tagged `[backend-specific]`.
5. When a phase ships, add/update `doc/UPDATELOG.md`, `doc/whatsnew.md`, and any relevant API docs.
6. **No rogue implementation:** do not add parallel frame lifecycles, auxiliary GLFW/GL presentation paths, or test-only renderer bypasses. New work must extend the existing **`AcquireFrameCommandBuffer` → command buffer → `SituationEndFrame`** contract unless a phase is explicitly approved and documented here first.

---

## Current state audit

The command stack is stronger than the initial gap list implies, but uneven. Some primitives already exist, while others are hidden behind render-pass setup or internal helpers.

| Area | Current state | Gap |
|------|---------------|-----|
| Render-pass clears | `SituationRenderPassInfo` already has `color_attachment`, `depth_attachment`, `stencil_attachment`, `loadOp`, `storeOp`, and `clear`. OpenGL honors color/depth clear on `SIT_OP_BEGIN_RENDER_PASS`; Vulkan passes `VkClearValue`s into `vkCmdBeginRenderPass`. | No standalone recorded clear command. OpenGL path does not visibly clear stencil in the begin-pass code path. Docs/readme do not make clear-values discoverable enough. |
| Compute dispatch | `SituationCmdDispatch` exists and maps to `SIT_OP_DISPATCH` / `glDispatchCompute` / `vkCmdDispatch`. | No `SituationCmdDispatchIndirect`; dispatch API returns `void` despite invalid command/pipeline cases being possible. |
| Barriers | `SituationCmdPipelineBarrier(cmd, src_flags, dst_flags)` exists; `SituationMemoryBarrier` is deprecated. OpenGL maps to `glMemoryBarrier`; Vulkan builds stage/access masks. | Barrier model is flag-only and underspecified for resource/image layout transitions, transfer usage, indirect commands, render target transitions, and compute -> graphics cases. |
| Buffer copy | `SituationCmdCopyBuffer` exists and records `SIT_OP_COPY_BUFFER` / `glCopyBufferSubData` / `vkCmdCopyBuffer`. | Only one offset and one size; no source/destination offset split; no typed error result; no buffer -> texture or texture -> buffer command. |
| Texture copy/blit | Public texture copy/blit plus `SituationCmdCopyBufferToTexture` / `SituationCmdCopyTextureToBuffer` for matching RGBA8 color 2D textures with explicit barriers. | Depth/stencil, array layers beyond layer 0, and buffer-row pitch on upload deferred or partial. |
| Indirect drawing | OpenGL internally uses MDI (`glMultiDrawArraysIndirect`, `glMultiDrawElementsIndirect`) to batch normal draw calls. Internal GL command structs are named `_SituationGLDraw*IndirectCommand`. | No public `SituationCmdDrawIndirect`, `SituationCmdDrawIndexedIndirect`, or indirect count/multi variants. Vulkan public path lacks matching command exposure. |
| Raster state | **Phase 6 closed (v2.4.187).** Public fixed-function raster commands through stencil, push/pop (**`SITUATION_MAX_RASTER_STACK_DEPTH` 256**), line width, color mask, polygon mode, depth bias, front face, topology. GL soft replay + VK dynamic state / pipeline variants. Harness **434/434** GL, **424/424** VK (GTX 1070). | **MSAA not end-to-end** — see **§ MSAA / multisample**. **`SituationCmdSetMultisampleState`** plumbing shipped **v2.4.256** (no visible MSAA until **VD-4b** attachments). Not part of Phase 6 closure criteria. |
| Index buffer | `SituationCmdBindIndexBuffer(cmd, buffer, offset)` exists and is documented as 32-bit only. | No `SituationIndexType`; 16-bit indices are impossible through the public command stack. |
| Viewports | `SituationCmdSetViewport` and `SituationCmdSetScissor` exist for one viewport/scissor. | No indexed viewport/scissor array for future multi-viewport. |
| Push constants | `SituationCmdSetPushConstant` and `SituationCmdSetPushConstantData` exist, but the GL path marks `SIT_OP_SET_PUSH_CONSTANT_DATA` as not implemented. | API is still raw: no named range/layout metadata, unclear layout contracts, and mixed contract-id vs shader/offset forms. |
| Queries / profiling | **P10.0–P10.4 shipped:** frame phases, spike count, overlay, histogram, `GetFrameProfile`, Tracy CPU zones (opt-in), P10.3 internal GPU zones, **P10.4 user `SituationQueryPool`**. | See **Phase 10** (layers) + **`AAA_ARCHITECTURE_PLAN.md` §6**. errno `-566` query-not-ready; `-563`…`-565` GPU profiling. |
| Render target model | Virtual displays are the composited offscreen abstraction. **Compositing** (blend, opacity, z-order, visibility, scaling, offset) **shipped**. **VD bolster v2.4.x COMPLETE @ v2.4.387:** **VD-1** attachment/load/store/depth-none; **VD-2** GL `GL_SRGB8_ALPHA8` + `GL_FRAMEBUFFER_SRGB`, inherit-clear + sRGB harness; **VD-3** explicit **`SituationVirtualDisplaySamplerDesc`** / **`SituationSetVirtualDisplaySampler`** (layout decoupled from **`SituationScalingMode`**); **VD-4a** aniso/mip LOD configure + create-time **`color_mip_levels`** + post-draw mipgen; **VD-5** **`SituationSetVirtualDisplayUpdateMode`** / **`SituationSetVirtualDisplayMemoryHint`**. Main-window **HDR10** → **`10BIT_COLOR_OUTPUT_PLAN.md`**. **Open:** float/HDR **VD attachments** (VD-6), **`SituationRenderTarget`**, **VD-4b MSAA** (v2.5-gated). | **VD-6** float/HDR attachments; **VD-4b** MSAA+resolve; main-window dynamic render-pass cache (Phase 11 remainder). Readback doc: **`guide/virtual_display.md`**. Command stack status: **`doc/RENDERER_COMMAND_STACK.md`**. |
| Test harness layout | `test_compute.c` owns dispatch/barriers; `test_transfer.c` owns Phase 4 copy/blit/barrier command tests (Phase 4.1); **`test_virtual_display.c`** owns VD API/compositing/scaling/blend + **VD bolster gates** (**40** tests GL, **39** VK @ v2.4.387). `test_graphics.c` keeps draw/raster/SPIR-V interop coverage. **`test_advanced.c`** holds multi-monitor **spanning-host + multi-VD** integration (design: **Phase 11-bis**). | Further splits (e.g. raster-only) deferred until module count stays manageable. True **per-monitor OS window** presentation deferred to **v2.5 multi-present** (see **Phase 11-bis Tier B**). |

---

## Design principles

- **Command-buffer first:** anything affecting GPU ordering or render state should be recordable on `SituationCommandBuffer`.
- **Prefer explicit `SituationError`:** new commands should return `SituationError` whenever validation, resource lookup, backend capability, or command-buffer state can fail. Keep `void` only for true fire-and-forget helpers with no meaningful caller action.
- **Preserve errno transparency:** public APIs and internal helpers should propagate specific `SituationError` values where logically possible. Avoid silent returns in new code; use the existing error-setting/reporting path so users can understand invalid handles, unsupported capabilities, bad state, and backend failures.
- **GL + VK parity by default:** Vulkan semantics should drive synchronization clarity; OpenGL should emulate safely with state tracking and conservative barriers where exact mapping is impossible.
- **Keep high-level helpers:** do not remove `SituationCmdDrawMesh`, `SituationCmdDrawQuad`, `SituationCmdDrawText`, or Virtual Display helpers. Bolster the lower-level layer they sit on.
- **VD owns target policy:** each **`SituationVirtualDisplay`** stores defaults for attachments, color/HDR, rendering quality, compositing, and performance hints. **`SituationRenderPassInfo`** may override per pass (inherit sentinel → VD default). See **§ Phase 11 — Virtual Display bolster**.
- **Owner defaults, configure-first:** tier **B** VD fields (quality, sampler, attachment defaults, compositing) change via **`SituationSetVirtualDisplay*`** / **`SituationConfigureVirtualDisplay`** — **configure-only in v1**. Pass mechanics (viewport/scissor/mid-pass clear, **`SituationRenderPassInfo`**) stay on the command buffer. **`SituationCmdSetVirtualDisplay*`** recorded mirrors are **post-v1 / optional** — add only when command-stream ordering is proven necessary; not part of v1 scope.
- **Avoid leaking backend structs:** no `Vk*` / `GL*` types in public signatures. Use Situation enums/descs.
- **Make tests black-box:** prefer image/readback/harness validation over private state inspection.
- **Single presentation contract:** one Situation instance, one primary command buffer per frame, one **`SituationEndFrame`** that presents the main swapchain. Multi-monitor output in **v2.4.x** is achieved by **WDM host placement + N Virtual Displays composited into that swapchain** — not by inventing a second frame API.
- **WDM owns window geometry; VD owns pixels:** monitor queries, host size/position, fullscreen, and undecorated state are **Window & Display Module** (`situation_impl_wdm.h`). Offscreen content, per-monitor panels, and compositing layout are **Virtual Display Module** (`situation_impl_vd.h`). The renderer executes both through the **same** command buffer.

---

## Proposed public API surface

Signatures are proposals unless marked **shipped** in phase checklists. Names should be settled before implementation. **Phases -1 through 8 + Phase 11 VD bolster (VD-1 … VD-5 @ v2.4.387) are shipped**; **P10.0–P10.4 shipped @ v2.4.397**; remaining blocks below are **open** (Phase 9 push constants, VD-4b, VD-6, Phase 12+).

### Clears

```c
SITAPI SituationError SituationCmdClearColor(SituationCommandBuffer cmd, ColorRGBA color);
SITAPI SituationError SituationCmdClearDepth(SituationCommandBuffer cmd, float depth);
SITAPI SituationError SituationCmdClearStencil(SituationCommandBuffer cmd, uint32_t stencil);
SITAPI SituationError SituationCmdClear(SituationCommandBuffer cmd, uint32_t clear_flags, const SituationClearValue* clear_value);
```

Proposed flags:

```c
typedef enum SituationClearFlags {
    SIT_CLEAR_COLOR   = 1u << 0,
    SIT_CLEAR_DEPTH   = 1u << 1,
    SIT_CLEAR_STENCIL = 1u << 2
} SituationClearFlags;
```

### Compute dispatch

```c
SITAPI SituationError SituationCmdDispatchEx(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
SITAPI SituationError SituationCmdDispatchIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset);
```

`SituationCmdDispatch` remains as the existing void compatibility wrapper during this plan. Wrapper retirement/deprecation decisions are deferred to Phase 13.

### Barriers and synchronization

```c
SITAPI SituationError SituationCmdPipelineBarrierEx(SituationCommandBuffer cmd, const SituationPipelineBarrierDesc* desc);
SITAPI SituationError SituationCmdBufferBarrier(SituationCommandBuffer cmd, const SituationBufferBarrierDesc* desc);
SITAPI SituationError SituationCmdTextureBarrier(SituationCommandBuffer cmd, SituationTexture texture, const SituationTextureBarrierDesc* desc);
```

The current `SituationCmdPipelineBarrier(cmd, src_flags, dst_flags)` should remain as a convenience wrapper for common compute/read/write transitions during this plan. Wrapper retirement/deprecation decisions are deferred to Phase 13.

### Copy and blit

```c
SITAPI SituationError SituationCmdCopyBufferEx(SituationCommandBuffer cmd, SituationBuffer src, size_t src_offset, SituationBuffer dst, size_t dst_offset, size_t size);
SITAPI SituationError SituationCmdCopyTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureCopyRegion* region);
SITAPI SituationError SituationCmdBlitTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureBlitRegion* region);
SITAPI SituationError SituationCmdCopyBufferToTexture(SituationCommandBuffer cmd, SituationBuffer src, size_t src_offset, SituationTexture dst, const SituationTextureCopyRegion* dst_region);
SITAPI SituationError SituationCmdCopyTextureToBuffer(SituationCommandBuffer cmd, SituationTexture src, const SituationTextureCopyRegion* src_region, SituationBuffer dst, size_t dst_offset, size_t dst_row_pitch);
```

### Indirect draw

```c
SITAPI SituationError SituationCmdDrawIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset, uint32_t draw_count, uint32_t stride);
SITAPI SituationError SituationCmdDrawIndexedIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset, uint32_t draw_count, uint32_t stride);
```

Potential future variants:

```c
SITAPI SituationError SituationCmdDrawIndirectCount(...);
SITAPI SituationError SituationCmdDrawIndexedIndirectCount(...);
```

Keep count variants deferred unless there is an immediate example/test need.

### Raster state completion

```c
SITAPI SituationError SituationCmdSetFrontFace(SituationCommandBuffer cmd, SituationFrontFace front_face);
SITAPI SituationError SituationCmdSetPrimitiveTopology(SituationCommandBuffer cmd, SituationPrimitiveTopology topology);
SITAPI SituationError SituationCmdSetPolygonMode(SituationCommandBuffer cmd, SituationPolygonMode mode);
SITAPI SituationError SituationCmdSetDepthBias(SituationCommandBuffer cmd, bool enable, float constant_factor, float clamp, float slope_factor);
SITAPI SituationError SituationCmdSetLineWidth(SituationCommandBuffer cmd, float width);
SITAPI SituationError SituationCmdSetColorWriteMask(SituationCommandBuffer cmd, bool r, bool g, bool b, bool a);
SITAPI SituationError SituationCmdSetStencilTest(SituationCommandBuffer cmd, bool enable, const SituationStencilState* front, const SituationStencilState* back);
SITAPI SituationError SituationCmdSetMultisampleState(SituationCommandBuffer cmd, const SituationMultisampleState* state);
```

### Index buffer flexibility

```c
typedef enum SituationIndexType {
    SIT_INDEX_UINT16 = 0,
    SIT_INDEX_UINT32 = 1
} SituationIndexType;

SITAPI SituationError SituationCmdBindIndexBufferEx(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset, SituationIndexType index_type);
```

Keep `SituationCmdBindIndexBuffer` as a `UINT32` wrapper during this plan. Wrapper retirement/deprecation decisions are deferred to Phase 13.

### Multi-viewport future-proofing

```c
SITAPI SituationError SituationCmdSetViewportIndexed(SituationCommandBuffer cmd, uint32_t index, float x, float y, float width, float height);
SITAPI SituationError SituationCmdSetScissorIndexed(SituationCommandBuffer cmd, uint32_t index, int x, int y, int width, int height);

/* Phase 11 foundation (v2.4.188): begin-pass helpers — see SituationRenderPassInfo docs in situation_api.h */
static inline SituationRenderPassInfo SituationRenderPassInfoDefault(int display_id, ColorRGBA clear_color);
static inline SituationRenderPassInfo SituationRenderPassInfoLoad(int display_id);
static inline uint32_t SituationRenderPassConfigurationKey(const SituationRenderPassInfo* info);
```

Defer viewport/scissor **array** commands until multi-viewport examples exist; indexed API covers interim use.

### Virtual Display configuration (Phase 11 — **shipped VD-1 … VD-5 @ v2.4.387**; see taxonomy + delivery slices)

Single creation desc; **`SituationCreateVirtualDisplayFromDesc`** (**shipped v2.4.316**). Legacy **`SituationCreateVirtualDisplay`** / **`CreateVirtualDisplayEx`** wrap today’s defaults. Field groups match **§ Configuration taxonomy** (reference only — actionables live in delivery slices).

```c
typedef enum SituationVirtualDisplayColorFormat {
    SIT_VD_FORMAT_RGBA8_UNORM = 0,
    SIT_VD_FORMAT_RGBA8_SRGB  = 1,
    /* VD-6 / v2.5: R16G16B16A16_SFLOAT, B10G11R11_UFLOAT, R8_UNORM, … */
} SituationVirtualDisplayColorFormat;

typedef enum SituationVirtualDisplayDepthStencilMode {
    SIT_VD_DEPTH_NONE    = 0,
    SIT_VD_DEPTH_D24     = 1,  /* default today */
    SIT_VD_DEPTH_D24S8   = 2
} SituationVirtualDisplayDepthStencilMode;

typedef enum SituationFilterMode { SIT_FILTER_NEAREST = 0, SIT_FILTER_LINEAR = 1 } SituationFilterMode;
typedef enum SituationMipmapFilterMode { SIT_MIP_FILTER_NEAREST = 0, SIT_MIP_FILTER_LINEAR = 1 } SituationMipmapFilterMode;
typedef enum SituationAddressMode {
    SIT_ADDRESS_CLAMP_TO_EDGE = 0, SIT_ADDRESS_REPEAT = 1, SIT_ADDRESS_MIRRORED_REPEAT = 2
} SituationAddressMode;

typedef enum SituationVirtualDisplayUpdateMode {
    SIT_VD_UPDATE_DYNAMIC = 0,  /* default: dirty + frame_time drive refresh */
    SIT_VD_UPDATE_STATIC  = 1   /* frame_time_mult=0 + manual dirty only */
} SituationVirtualDisplayUpdateMode;

typedef enum SituationVirtualDisplayMemoryHint {
    SIT_VD_MEMORY_DEFAULT = 0,
    SIT_VD_MEMORY_PREFER_SPEED = 1,
    SIT_VD_MEMORY_PREFER_QUALITY = 2
} SituationVirtualDisplayMemoryHint;

typedef struct SituationVirtualDisplayAttachmentDefaults {
    SituationAttachmentLoadOp  color_load;
    SituationAttachmentStoreOp color_store;
    SituationAttachmentLoadOp  depth_load;
    SituationAttachmentStoreOp depth_store;
    SituationAttachmentLoadOp  stencil_load;   /* when D24S8 */
    SituationAttachmentStoreOp stencil_store;
    SituationClearValue        clear;
} SituationVirtualDisplayAttachmentDefaults;

typedef struct SituationVirtualDisplaySamplerDesc {
    SituationFilterMode       min_filter;
    SituationFilterMode       mag_filter;
    SituationMipmapFilterMode mip_filter;
    SituationAddressMode      wrap_u;
    SituationAddressMode      wrap_v;
    float                     max_anisotropy;  /* 1.f = off */
    uint32_t                  max_mip_level;     /* LOD clamp when compositing */
} SituationVirtualDisplaySamplerDesc;

typedef struct SituationVirtualDisplayDesc {
    /* Resolution */
    Vector2 resolution;

    /* §2 Attachment configuration */
    SituationVirtualDisplayColorFormat     color_format;
    SituationVirtualDisplayDepthStencilMode depth_stencil_mode;

    /* §3 Color / HDR + §2 load/store */
    SituationVirtualDisplayAttachmentDefaults attachments;

    /* §1 Rendering quality (into + composite) — VD-4b / v2.5: replace int with enum below */
    SituationMultisampleQuality   msaa_quality;    /* v2.4 shipped: int msaa_samples (must be 1) until VD-4b */
    uint32_t                  color_mip_levels;    /* storage mips to allocate / generate */
    SituationVirtualDisplaySamplerDesc composite_sampler;

    /* §4 Compositing / presentation */
    Vector2              offset;
    float                opacity;
    int                  z_order;
    bool                 visible;
    SituationScalingMode scaling_mode;   /* layout rect only */
    SituationBlendMode   blend_mode;

    /* §5 Performance / memory */
    SituationVirtualDisplayUpdateMode  update_mode;
    SituationVirtualDisplayMemoryHint  memory_hint;

    /* Timing (existing) */
    double frame_time_mult;
} SituationVirtualDisplayDesc;

SITAPI SituationError SituationCreateVirtualDisplayFromDesc(const SituationVirtualDisplayDesc* desc, int* out_id);

/* Configure subsets (non-recorded); mirror fields on SituationConfigureVirtualDisplay where overlap exists */
SITAPI SituationError SituationSetVirtualDisplayAttachmentDefaults(int id, const SituationVirtualDisplayAttachmentDefaults* d);
SITAPI SituationError SituationSetVirtualDisplayClearColor(int id, ColorRGBA color);
SITAPI SituationError SituationSetVirtualDisplaySampler(int id, const SituationVirtualDisplaySamplerDesc* sampler);
SITAPI SituationError SituationSetVirtualDisplayMaxAnisotropy(int id, float max_anisotropy);
SITAPI SituationError SituationSetVirtualDisplayMipLevels(int id, uint32_t color_mip_levels, uint32_t sampler_max_mip_level);

/* VD-4b / v2.5 default — omit from v2.4 implementation unless scope gate passes */
SITAPI SituationError SituationSetVirtualDisplayMultisampleQuality(int id, SituationMultisampleQuality quality);

static inline SituationRenderPassInfo SituationRenderPassInfoInherit(int display_id);

/* Post-v1 (optional): SituationCmdSetVirtualDisplayMultisampleQuality / MaxAnisotropy / MipLevels —
 * recorded mirrors of configure APIs. Not in v1 public surface; see § Decision (v1). */
```

**§6 Advanced / future** (not in desc v1): multisample resolve mode, per-sample shading rate, custom resolve/compositing shaders, **`SituationTextureUsage`**-style render-target flags, **`SituationCmdSetVirtualDisplay*`** — see delivery slices **VD-6** / post-v1.

**MSAA quality enum (VD-4b / v2.5 — shipped @ v2.4.398 as types only):**

```c
typedef enum SituationMultisampleQuality {
    SITUATION_MULTISAMPLE_OFF = 0,
    SITUATION_MULTISAMPLE_2X,
    SITUATION_MULTISAMPLE_4X,   /* default "good" — aligns with SITUATION_FLAG_MSAA_4X_HINT */
    SITUATION_MULTISAMPLE_8X,
    SITUATION_MULTISAMPLE_16X,
    /* Future: SITUATION_MULTISAMPLE_ADAPTIVE, etc. */
} SituationMultisampleQuality;

/* Inline helpers (clamp to SituationGetGraphicsCaps::max_msaa_samples at configure/create) */
static inline int SituationMultisampleQualitySampleCount(SituationMultisampleQuality q);
static inline SituationMultisampleQuality SituationMultisampleQualityClamp(SituationMultisampleQuality q, int max_samples);
```

**Unified MSAA direction:** one user-facing quality system (attachment tier via **`SituationMultisampleQuality`**), not three parallel knobs. **`SituationMultisampleState`** + **`SituationCmdSetMultisampleState`** remain **in-pass raster flags** (layer 2) — meaningful only when layer 1 is not **`OFF`**. Retire raw **`int msaa_samples`** and fold **`SITUATION_FLAG_MSAA_4X_HINT`** into the enum at init (main-window track, v2.5). See **§ MSAA / multisample**.

### Queries and profiling (layered — see Phase 10)

**Shipped (P10.0):** `SituationGetMaxFrameTime`, `SituationGetFrameSpikeCount`, `SituationGetLastFramePhases`, `SituationGetRenderLatencyStats`, `SituationDrawMetricsOverlay`, `SituationExportRenderHistogram` — already in `situation_api.h`.

**Shipped (P10.1 @ v2.4.394):** `SituationFrameProfile`, `SituationGetFrameProfile`, `SituationResetFrameProfileStats` — headless snapshot wrapping P10.0 getters + poll/update phase ns.

**Shipped (P10.3 @ v2.4.396):** `SituationCmdGPUZoneBegin/End` — fills `gpu_zone_ns[]` when `SIT_FEATURE_GPU_TIMESTAMPS` supported. See **`AAA_ARCHITECTURE_PLAN.md` §6.6–6.7**.

**Shipped (P10.4 @ v2.4.397):** public **`SituationQueryPool`** — create/destroy, timestamp + occlusion query cmds, **`SituationGetQueryPoolResults`**. errno **`-566`** query-not-ready (not **`-557`**, reserved for shader compile timeout). Harness **`query_pool`** 3/3 GL + VK.

```c
SITAPI SituationError SituationCreateQueryPool(SituationQueryType type, uint32_t count, SituationQueryPool* out_pool);
SITAPI void SituationDestroyQueryPool(SituationQueryPool* pool);
SITAPI SituationError SituationCmdResetQueryPool(SituationCommandBuffer cmd, SituationQueryPool pool, uint32_t first_query, uint32_t query_count);
SITAPI SituationError SituationCmdWriteTimestamp(SituationCommandBuffer cmd, SituationPipelineStage stage, SituationQueryPool pool, uint32_t query_index);
SITAPI SituationError SituationCmdBeginOcclusionQuery(SituationCommandBuffer cmd, SituationQueryPool pool, uint32_t query_index);
SITAPI SituationError SituationCmdEndOcclusionQuery(SituationCommandBuffer cmd);
SITAPI SituationError SituationGetQueryPoolResults(SituationQueryPool pool, uint32_t first_query, uint32_t query_count, uint64_t* out_results, uint32_t flags);
/* Non-blocking not-ready → SITUATION_ERROR_QUERY_RESULT_NOT_READY (-566) */
```

---

## Phase -1 — Compute harness split pilot

**Purpose:** create the right test landing zone before renderer API work starts. Move existing compute-only coverage out of `test_graphics.c` into a new `tests/harness/test_compute.c`, wire it into the harness/build flow, and define where future compute-vs-graphics interop tests belong.

Current state: compute dispatch, barriers, SSBO readback, and related validation are mixed into `test_graphics.c`. That made sense while the command stack was small, but Phase 2 and Phase 3 will add more compute-specific coverage and should not keep expanding the graphics module.

### Phase -1A — Create the pilot compute module

- [x] Inventory compute-adjacent tests in `tests/harness/test_graphics.c`.
- [x] Classify tests as:
  - [x] **Pure compute:** compute pipeline creation, bind compute pipeline, dispatch, SSBO writes, barriers, async/readback counters, workgroup-limit checks.
  - [x] **Graphics interop:** compute output consumed by a graphics pass, compute-generated draw arguments, texture/image results validated through rendered pixels.
  - [x] **Graphics-only:** render pass, raster state, draw, texture sampling, virtual display, framebuffer readback.
- [x] Create `tests/harness/test_compute.c` as a copy-and-fit pilot module, using the existing harness module pattern instead of designing a new framework.
- [x] Copy pure compute tests into `test_compute.c` with minimal behavior changes.
- [x] Keep the source tests in `test_graphics.c` during this first step so coverage is duplicated, not moved, while fitment is still in progress.
- [x] Add module-local setup/teardown, helper includes, shader strings, and shared helpers needed for `test_compute.c` to compile independently.

### Phase -1B — Wire the module into the harness

- [x] Add compute module registration/list labels that match the existing harness style.
- [x] Wire `test_compute.c` into `build_tests.bat` and any backend-specific test build paths.
- [x] Build the same `test_compute.c` into both OpenGL and Vulkan harness executables, matching the existing `test_graphics.c` pattern.
- [x] Confirm module filters can run compute independently:
  - [x] OpenGL: `sit_test.exe --module compute`
  - [x] Vulkan: `sit_test_vulkan.exe --module compute`
- [x] Add labels for current and future tests:
  - [x] `compute.dispatch_basic`
  - [x] `compute.barrier_ssbo_readback`
  - [ ] `compute.readback_counter` (test not yet implemented)
  - [x] `compute.workgroup_limits`
  - [x] `compute.dispatch_indirect` (Phase 2 — shipped: `dispatch_indirect_cpu_filled`, `dispatch_indirect_validation`, `dispatch_indirect_compute_generated`, `dispatch_indirect_buffer_barrier`)

### Phase -1C — Prove backend coverage and behavior

- [x] Keep backend separation explicit:
  - [x] Use compile-time backend guards only for backend-specific shader text, setup details, or unsupported behavior.
  - [x] Do not split into separate `test_compute_gl.c` / `test_compute_vk.c` files unless a test is genuinely backend-specific.
  - [x] Add an inline backend coverage note for each moved test: `[GL+VK]`, `[GL only]`, `[VK only]`, or `[deferred]`.
  - [x] Record any backend skip with the reason and expected follow-up, not as an unlabelled pass.
- [x] Add a Phase -1 compute test matrix:
  - [x] test label
  - [x] source test copied from `test_graphics.c`
  - [x] backend coverage (`GL+VK`, `GL only`, `VK only`)
  - [x] assertion type (`buffer/readback`, `compile/link`, `no-crash`, `capability query`)
  - [x] owner phase for future expansion
- [x] Run/confirm both backends for the new compute module before removing the legacy graphics copy.
- [x] Compare compute test results against the duplicated graphics coverage and investigate any mismatched skip/pass/fail behavior.

### Phase -1D — Retire the legacy graphics copy

- [x] Remove pure compute duplicates from `test_graphics.c` after `test_compute.c` proves equivalent backend coverage.
- [x] Keep graphics interop tests in `test_graphics.c` unless the final assertion is buffer/readback-only.
- [x] Re-run/confirm graphics module filters so removing duplicated compute tests did not drop graphics coverage.
- [x] Update this plan if the split reveals better names or ownership boundaries for compute/graphics interop coverage.
- [x] Update `doc/UPDATELOG.md` with test count/module changes once the split lands.

**Exit criteria:** `test_compute.c` exists, owns pure compute dispatch/barrier/readback coverage, is included in both OpenGL and Vulkan test builds, has a visible backend coverage matrix, and `test_graphics.c` keeps only graphics and graphics-interop coverage.

---

## Phase 0 — Scope lock and current-state cleanup

**Purpose:** document what already exists and prevent duplicate APIs from being designed around stale assumptions.

- [x] Confirm whether this bolster targets the next patch line (`v2.4.x`) or a larger minor line (`v2.5+`): **target v2.4.x as a renderer-hardening worksheet, coordinated with but not blocked on v2.5**.
- [x] Decide whether new commands should all return `SituationError`, including replacements for existing void commands (`SituationCmdDispatch`, `SituationCmdCopyBuffer`, `SituationCmdPipelineBarrier`): **yes where logically sound; any command that can validate inputs, handles, backend support, or state should return `SituationError`**.
- [x] Decide compatibility policy: wrappers stay forever, or deprecated once `Ex` versions ship: **keep wrappers through this plan; move wrapper retirement/deprecation to Phase 13 (last phase + 1)**.
- [x] Add a "Renderer command stack status" table to API docs, marking existing, new, and deferred commands — **`doc/RENDERER_COMMAND_STACK.md`** @ v2.4.387.
- [x] Audit `situation_api.h` for stale comments that still imply no dispatch/barrier/copy support. Phase 3C updated clear, barrier, memory-barrier, and legacy copy comments.
- [x] Audit `sit/situation_impl_renderer.h` comments that mention internal-only barriers/copies and update them once public commands land. Phase 3C updated dispatch and deprecated memory-barrier guidance.
- [ ] Create a backend parity matrix for every proposed command: OpenGL, Vulkan, tests, fallback behavior, and exact `SituationError` behavior.
- [ ] Define common validation helper style for command buffer APIs: null command, invalid handle, out-of-bounds offset/size, unsupported backend caps, invalid state, and backend failure propagation.
- [ ] Audit new/internal helper boundaries for silent returns and convert them to specific `SituationError` returns where callers can act on the failure.

### Backend parity and errno matrix template

Every new command should have one row before implementation starts:

| Command | Phase | OpenGL behavior | Vulkan behavior | Tests | Fallback/unsupported behavior | Error returns |
|---------|-------|-----------------|-----------------|-------|-------------------------------|---------------|
| `SituationCmdExample` | Phase N | Record/execute GL mapping, or `[GL only]` reason | Record/execute VK mapping, or `[VK only]` reason | GL+VK harness labels | `SITUATION_ERROR_NOT_IMPLEMENTED` / backend-specific unsupported error / documented no-op / deferred | null cmd, invalid handle, invalid range, invalid state, backend failure |
| `SituationCmdClear*` / `SituationCmdClear` | Phase 1 | Record `SIT_OP_CLEAR`; execute color/depth via GL clear commands; stencil command returns `SITUATION_ERROR_NOT_IMPLEMENTED` until stencil attachments/state are exposed consistently | Use `vkCmdClearAttachments` for active render-pass attachment clears when the selected format supports the requested aspects; begin-pass load clears continue through `VkClearValue` | `graphics.clear_color_command`, `graphics.clear_depth_command`, `graphics.clear_stencil_conditional` | Return `SITUATION_ERROR_NO_RENDER_PASS_ACTIVE` outside an active render pass; texture clears remain deferred to transfer/texture phases; unsupported stencil aspect returns `SITUATION_ERROR_NOT_IMPLEMENTED` | not initialized, null cmd, invalid clear flags/value, no active render pass, command buffer full, GL/VK command failure |
| `SituationCmdDispatchEx` / `SituationCmdDispatch` | Phase 2 | Validate command/groups, record existing `SIT_OP_DISPATCH`; legacy wrapper drops the returned error after setting last-error state | Validate command/groups, record `vkCmdDispatch`; legacy wrapper drops the returned error after setting last-error state | `compute.dispatch_basic`, `compute.dispatch_ex_invalid_params` | Keep `SituationCmdDispatch` as compatibility wrapper through Phase 13; bound-pipeline validation waits for compute bind-state tracking | not initialized, null cmd, zero group count, command buffer full, VK command failure, future missing pipeline/binding errors |
| `SituationCmdDispatchIndirect` | Phase 2 | Validate indirect buffer/range/usage, record `SIT_OP_DISPATCH_INDIRECT`, execute `glDispatchComputeIndirect` from `GL_DISPATCH_INDIRECT_BUFFER` | Validate indirect buffer/range/usage, call `vkCmdDispatchIndirect` with a `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT` buffer | `compute.dispatch_indirect_cpu_filled`, `compute.dispatch_indirect_validation` | CPU-filled indirect commands are supported now; compute-generated indirect commands require Phase 3 barrier guidance before proof | not initialized, null cmd, invalid handle, missing indirect usage, misaligned/out-of-range command, command buffer full |
| `SituationCmdPipelineBarrierEx` | Phase 3 | Validate global stage/access descriptor, translate to existing GL soft barrier packet and `glMemoryBarrier` bits | Validate global stage/access descriptor, map directly to `VkMemoryBarrier` stage/access masks | `compute.pipeline_barrier_no_crash`, `compute.dispatch_indirect_compute_generated` | Old `SituationCmdPipelineBarrier` remains as wrapper/convenience API | not initialized, null cmd/desc, missing stage masks, command buffer full, backend command failure |
| `SituationCmdBufferBarrier` | Phase 3 | Validate buffer handle/range and stage/access descriptor, then conservatively translate through the existing GL soft barrier packet | Validate buffer handle/range and stage/access descriptor, then record `VkBufferMemoryBarrier` for the requested byte range | `compute.pipeline_barrier_no_crash`, `compute.dispatch_indirect_buffer_barrier` | GL has no buffer-layout equivalent, so range is validation-only there | not initialized, null cmd/desc, invalid buffer handle, missing stage masks, zero/out-of-range size, unsupported stage mask |
| `SituationCmdTextureBarrier` | Phase 3/4 prerequisite | Validate texture handle/usage/mip range, then record a conservative GL soft barrier packet; no GL layout state is tracked | Validate texture handle/usage/mip range, then record `VkImageMemoryBarrier` for explicit old/new layout and color aspect | `transfer.texture_barrier_validation` | First slice is color-only 2D textures, layer 0, explicit caller-provided old/new layouts; attachment/present ownership returns `SITUATION_ERROR_NOT_IMPLEMENTED` until render-target ownership is exposed | not initialized, null cmd/desc, invalid texture handle, missing sampled/storage/transfer usage, invalid mip/layer range, unsupported layout |
| `SituationCmdCopyBufferEx` / `SituationCmdCopyBuffer` | Phase 4 | Validate transfer usage/ranges, record `SIT_OP_COPY_BUFFER` with independent source/destination offsets, execute `glCopyNamedBufferSubData`; legacy wrapper maps `src_offset=offset`, `dst_offset=0` | Validate transfer usage/ranges, record `vkCmdCopyBuffer` with independent source/destination offsets; legacy wrapper maps `src_offset=offset`, `dst_offset=0` | `transfer.copy_buffer_ex_offsets`, `transfer.copy_buffer_ex_validation`; `graphics.async_buffer_readback` | Legacy wrapper preserves shipped readback semantics | not initialized, null cmd, invalid handle, missing transfer usage, zero/out-of-range size, command buffer full |
| `SituationCmdBlitTexture` | Phase 4 | Validate color 2D texture handles/usage/rects, record `SIT_OP_BLIT_TEXTURE`, execute via temporary read/draw FBOs and `glBlitNamedFramebuffer` | Validate color 2D texture handles/usage/rects, record `vkCmdBlitImage` using `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` / `TRANSFER_DST_OPTIMAL` | `transfer.blit_texture_validation`, `transfer.blit_texture_same_size_asymmetric`, `transfer.blit_texture_scaled_nearest_asymmetric` | First slice is matching RGBA8 color formats, layer 0, explicit barriers before/after; no implicit flips or hidden transitions | not initialized, null cmd/region, invalid handles, missing transfer usage, invalid rect/mip/layer, unsupported format/filter |
| `SituationCmdCopyTexture` | Phase 4 | Validate color 2D texture handles/usage/region, record `SIT_OP_COPY_TEXTURE`, execute `glCopyImageSubData` | Validate color 2D texture handles/usage/region, record `vkCmdCopyImage` using `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` / `TRANSFER_DST_OPTIMAL` | `transfer.copy_texture_validation`, `transfer.copy_texture_same_size_asymmetric` | Exact-size copy only (`src_rect` extent written at `dst_x`/`dst_y`); same coordinate contract as blit; explicit barriers before/after | not initialized, null cmd/region, invalid handles, missing transfer usage, invalid rect/mip/layer, unsupported format |
| `SituationCmdCopyBufferToTexture` / `SituationCmdCopyTextureToBuffer` | Phase 4 | Validate buffer/texture transfer usage and region bounds; record soft ops; execute via PBO `glTextureSubImage2D` / `glGetTextureSubImage` | Validate buffer/texture transfer usage and region bounds; record `vkCmdCopyBufferToImage` / `vkCmdCopyImageToBuffer` with caller-owned transfer layouts | `transfer.copy_buffer_to_texture_*`, `transfer.copy_texture_to_buffer_*` | Buffer upload uses tightly packed RGBA8 rows; texture readback supports optional `dst_row_pitch`; explicit texture barriers before/after | not initialized, null cmd/region, invalid handles, missing transfer usage, invalid rect/mip/layer/buffer range, unsupported format |

### Shared errno mapping table

Use existing `sit/situation_base_errno.h` codes first. Add new error codes only when an existing code would hide a materially different failure mode.

| Failure class | Preferred `SituationError` | Notes |
|---------------|----------------------------|-------|
| API called before init | `SITUATION_ERROR_NOT_INITIALIZED` | Return before touching renderer state. |
| Null pointer / bad enum / impossible scalar value | `SITUATION_ERROR_INVALID_PARAM` | Include the parameter name in `_SituationSetErrorFromCode` detail. |
| Null/corrupt/stale resource handle | `SITUATION_ERROR_INVALID_RESOURCE_HANDLE` | Prefer over EOL `SITUATION_ERROR_RESOURCE_INVALID` in new code. |
| Already destroyed resource | `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED` | Use when generation/slot proves use-after-free rather than generic invalid handle. |
| Buffer offset/size out of bounds | `SITUATION_ERROR_BUFFER_INVALID_SIZE` or `SITUATION_ERROR_BUFFER_OVERFLOW` | Use `BUFFER_INVALID_SIZE` for read/copy ranges, `BUFFER_OVERFLOW` for writes/appends beyond capacity. |
| Wrong usage flags for resource role | `SITUATION_ERROR_BUFFER_INVALID_USAGE` | Applies to transfer src/dst, indirect buffer, storage/uniform role checks. Add a texture-specific errno only if texture usage validation needs distinct reporting. |
| No frame command buffer acquired | `SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER` | Prefer over silent return for command-buffer APIs. |
| Command buffer packet/data capacity exhausted | `SITUATION_ERROR_COMMAND_BUFFER_FULL` | GL soft command buffer already uses this; propagate it from new recorders. |
| Command recording failed for other renderer reasons | `SITUATION_ERROR_RENDER_COMMAND_FAILED` | Use when the command cannot be recorded but no narrower code exists. |
| Command requires an active render pass | `SITUATION_ERROR_NO_RENDER_PASS_ACTIVE` | Example: attachment clears if Phase 1 chooses render-pass-only clear commands. |
| Command is illegal during an active render pass | `SITUATION_ERROR_RENDER_PASS_ACTIVE` | Example: future transfer commands if backend requires them outside a render pass. |
| Nested render pass attempted | `SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE` | Use for begin-pass while a pass is already active. |
| Backend-neutral feature not implemented yet | `SITUATION_ERROR_NOT_IMPLEMENTED` | Use for deferred API surface before a backend implementation exists. |
| Backend-specific capability unsupported | `SITUATION_ERROR_OPENGL_UNSUPPORTED` / `SITUATION_ERROR_VULKAN_UNSUPPORTED` | Use the backend-specific code when a concrete backend lacks a required version, extension, feature, or limit. |
| OpenGL command/backend failure | `SITUATION_ERROR_OPENGL_GENERAL` or narrower OpenGL code | Include `glGetError`/context detail where available. |
| Vulkan command/backend failure | `SITUATION_ERROR_VULKAN_COMMAND_FAILED` / `SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED` / narrower Vulkan code | Include `VkResult` in the detail string. |
| Pipeline bind/layout failure | `SITUATION_ERROR_PIPELINE_BIND_FAIL` | Use for incompatible graphics/compute pipeline state, descriptor layout mismatch, or invalid bound pipeline. |
| Compute pipeline creation/dispatch failure | `SITUATION_ERROR_COMPUTE_PIPELINE_CREATION_FAILED` / `SITUATION_ERROR_COMPUTE_DISPATCH_FAILED` | Keep compute-specific failures in the compute errno range where appropriate. |
| Required compute binding missing | `SITUATION_ERROR_COMPUTE_BUFFER_BINDING_MISSING` | Use for dispatch validation when required SSBO/storage binding is absent. |
| Internal invariant violation | `SITUATION_ERROR_INTERNAL_STATE_CORRUPTED` | Fatal/library bug; detail must name the violated invariant. |

### Existing renderer-relevant errno inventory

These codes already exist in `sit/situation_base_errno.h` and should be reused before proposing additions.

| Code | Value | Use in this plan |
|------|-------|------------------|
| `SITUATION_SUCCESS` | `0` | Successful validation/record/execute. |
| `SITUATION_ERROR_GENERAL` | `-1` | Last-resort fallback only; prefer a narrower code below. |
| `SITUATION_ERROR_NOT_IMPLEMENTED` | `-2` | Deferred backend-neutral API or deliberately unimplemented command path. |
| `SITUATION_ERROR_NOT_INITIALIZED` | `-3` | Command/API called before `SituationInit`. |
| `SITUATION_ERROR_INVALID_PARAM` | `-7` | Null pointers, bad enum values, invalid scalar inputs, invalid descriptor structs. |
| `SITUATION_ERROR_MEMORY_ALLOCATION` | `-8` | CPU-side allocation failure while recording/creating command metadata. |
| `SITUATION_ERROR_INTERNAL_STATE_CORRUPTED` | `-9` | Broken internal invariant; should include high-detail diagnostics. |
| `SITUATION_ERROR_RESOURCE_INVALID` | `-500` | Existing/EOL invalid handle code; new code should prefer `SITUATION_ERROR_INVALID_RESOURCE_HANDLE`. |
| `SITUATION_ERROR_BUFFER_INVALID_SIZE` | `-501` | Buffer copy/read/write range is out of bounds or size is zero when invalid. |
| `SITUATION_ERROR_RENDER_COMMAND_FAILED` | `-502` | Command could not be recorded and no narrower command-buffer errno applies. |
| `SITUATION_ERROR_RENDER_PASS_ACTIVE` | `-503` | Command is illegal while a render pass is active. |
| `SITUATION_ERROR_INVALID_RESOURCE_HANDLE` | `-510` | Null, corrupt, stale, or generation-mismatched resource handle. |
| `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED` | `-511` | Proven use-after-destroy when slot/generation metadata can distinguish it. |
| `SITUATION_ERROR_BUFFER_MAP_FAILED` | `-512` | Readback/query/copy staging map failure. |
| `SITUATION_ERROR_BUFFER_OVERFLOW` | `-513` | Append/write would exceed backing capacity. |
| `SITUATION_ERROR_BUFFER_INVALID_USAGE` | `-514` | Buffer missing transfer/indirect/storage/uniform usage required by command. |
| `SITUATION_ERROR_TEXTURE_UPLOAD_FAILED` | `-520` | Texture upload/create path failed before command execution. |
| `SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER` | `-530` | No frame command buffer is available/acquired. |
| `SITUATION_ERROR_COMMAND_BUFFER_FULL` | `-531` | Command packet/data arena is full or already broken. |
| `SITUATION_ERROR_NO_RENDER_PASS_ACTIVE` | `-540` | Command requires an active render pass but none is active. |
| `SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE` | `-541` | Nested begin-render-pass attempt. |
| `SITUATION_ERROR_BACKEND_MISMATCH` | `-550` | Wrong backend path or handle/backend mismatch. |
| `SITUATION_ERROR_BACKEND_SPECIFIC` | `-551` | Backend failed but the plan has no narrower GL/VK errno. Include detail string. |
| `SITUATION_ERROR_PIPELINE_BIND_FAIL` | `-552` | Incompatible/missing bound pipeline, layout mismatch, invalid pipeline handle. |
| `SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS` | `-553` | Shader/pipeline not ready yet; relevant when command validates bound shader state. |
| `SITUATION_ERROR_INDIRECT_COMMAND_INVALID` | `-556` | Indirect command offset, range, alignment, or backing payload is invalid. |
| `SITUATION_ERROR_OPENGL_GENERAL` | `-600` | `glGetError` or OpenGL command failure with no narrower code. |
| `SITUATION_ERROR_OPENGL_UNSUPPORTED` | `-602` | Required OpenGL version/extension/capability missing. |
| `SITUATION_ERROR_OPENGL_FBO_INCOMPLETE` | `-620` | Clear/copy/blit path requires an FBO and completeness fails. |
| `SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE` | `-636` | GL SPIR-V path unavailable; keep shader-related command validation explicit. |
| `SITUATION_ERROR_VULKAN_UNSUPPORTED` | `-703` | Required Vulkan feature/extension/capability missing. |
| `SITUATION_ERROR_VULKAN_COMMAND_FAILED` | `-720` | Vulkan command pool/buffer record helper failed. |
| `SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED` | `-721` | Vulkan command record/submit/sync failed. |
| `SITUATION_ERROR_VULKAN_RENDERPASS_FAILED` | `-730` | Render pass creation/selection failure. |
| `SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED` | `-731` | Framebuffer creation/selection failure. |
| `SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED` | `-747` | Graphics/compute pipeline creation failure. |
| `SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED` | `-735` | Descriptor set/pool/layout operation failed. |
| `SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED` | `-750` | VMA/Vulkan allocation failure. |
| `SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR` | `-751` | Validation-layer-reported backend error. |
| `SITUATION_ERROR_SHADER_COMPILATION_FAILED` | `-752` | shaderc/compiler failure. |
| `SITUATION_ERROR_COMPUTE_PIPELINE_CREATION_FAILED` | `-800` | Compute pipeline creation failed outside a backend-specific narrower code. |
| `SITUATION_ERROR_COMPUTE_DISPATCH_FAILED` | `-801` | Compute dispatch validation/execution failed. |
| `SITUATION_ERROR_COMPUTE_BUFFER_BINDING_MISSING` | `-802` | Required compute buffer/storage binding missing before dispatch. |

### Proposed errno additions for this plan

Add these to `sit/situation_base_errno.h` before first code use. Values below intentionally occupy gaps in the existing rendering range.

| Proposed code | Proposed value | Needed by | Message |
|---------------|----------------|-----------|---------|
| `SITUATION_ERROR_TEXTURE_INVALID_USAGE` | `-521` | Phase 3/4 texture barriers, copy/blit, buffer-texture transfers | Texture missing required sampled/storage/transfer/render-target usage flags |
| `SITUATION_ERROR_TEXTURE_REGION_INVALID` | `-522` | Phase 4 copy/blit/texture readback region validation | Texture region, mip, layer, extent, or row pitch is invalid or out of bounds |
| `SITUATION_ERROR_TEXTURE_FORMAT_UNSUPPORTED` | `-523` | Phase 4 blit/copy, Phase 11 render-target follow-up | Texture format cannot be used for requested operation |
| `SITUATION_ERROR_QUERY_RESULT_NOT_READY` | `-557` | Phase 10 non-blocking query result reads | Query result is not available yet; caller may poll again later |

If a phase repeatedly needs another missing generic code, add it to the X-macro errno table first, preserve numbering gaps, then use it in the API and tests.

**Exit criteria:** final scope and naming are locked; no phase implements a duplicate of an already shipped command.

---

## Phase 1 — Clear commands and render-pass clarity

**Purpose:** make clearing discoverable and recordable outside begin-render-pass setup.

Current state: `SituationRenderPassInfo` can clear attachments through `loadOp = SIT_LOAD_OP_CLEAR`, and recorded `SituationCmdClear*` commands now cover explicit mid-pass color/depth clears. OpenGL begin-pass color/depth clears are implemented; stencil remains deferred until stencil attachments/state are exposed consistently. Vulkan clear values are supplied to `vkCmdBeginRenderPass`, and mid-pass clears use `vkCmdClearAttachments`.

Clear path guidance:

- Use `SituationRenderPassInfo` + `SIT_LOAD_OP_CLEAR` when an attachment should be cleared as the render pass begins.
- Use `SituationCmdClear*` when command order matters inside an already active render pass, such as mid-pass reset, explicit intent in a command stream, or future sub-region clear work.
- OpenGL stencil command clears currently report `SITUATION_ERROR_NOT_IMPLEMENTED` when the active backend/attachment state cannot guarantee a real stencil target. Begin-pass stencil load clears are also deliberately deferred until the stencil phase exposes consistent attachment ownership.

- [x] Add `SituationClearFlags` in `situation_api.h`.
- [x] Add `SituationCmdClearColor`, `SituationCmdClearDepth`, `SituationCmdClearStencil`, and `SituationCmdClear`.
- [x] Add `SIT_OP_CLEAR` or split opcodes (`SIT_OP_CLEAR_COLOR`, `SIT_OP_CLEAR_DEPTH_STENCIL`) in `situation_impl_decl.h`.
- [x] Add packet fields for clear flags and `SituationClearValue`.
- [x] Implement OpenGL clears with `glClearBufferfv`, `glClearBufferfi`, or `glClear` after preserving/restoring relevant color/depth/stencil write masks as needed.
- [x] Implement Vulkan clears with `vkCmdClearAttachments` inside a render pass for attachment clears.
- [x] Define behavior outside an active render pass:
  - [x] Option A: return `SITUATION_ERROR_NO_RENDER_PASS_ACTIVE`.
  - [x] Option B: allow texture clears in a later texture-clear command only.
- [ ] Fix/verify OpenGL begin-pass stencil clear when `stencil_attachment.loadOp == SIT_LOAD_OP_CLEAR`. Deferred until stencil attachments/state are exposed consistently; current command test deliberately avoids making begin-pass stencil load clear part of Phase 1 proof.
- [x] Add docs explaining two clear paths:
  - [x] render-pass load clear (`SituationRenderPassInfo`) for beginning a pass.
  - [x] recorded clear commands for mid-pass clear regions / explicit intent.
- [x] Add harness test: clear color then read framebuffer pixel.
- [x] Add harness test: clear depth affects depth-tested draw outcome.
- [x] Add harness test: stencil clear path if stencil attachment is actually exposed/available in both backends. Current coverage accepts `SITUATION_SUCCESS` when the active attachment supports stencil and `SITUATION_ERROR_NOT_IMPLEMENTED` when it does not; stencil-tested draw outcome remains deferred to the raster-state/stencil phase.

**Exit criteria:** users can clear color/depth explicitly, unsupported stencil clears fail transparently, and tests prove the implemented command paths behave consistently. Full stencil load/store and stencil-tested draw proof remain with the raster-state/stencil work.

---

## Phase 2 — Error-return compute and indirect dispatch

**Purpose:** complete compute command ergonomics without breaking current callers.

Current state: `SituationCmdDispatchEx` and `SituationCmdDispatchIndirect` exist, while legacy `SituationCmdDispatch` remains as a `void` compatibility wrapper. `SituationCmdBindComputePipeline` now returns `SituationError` (v2.4.202).

- [x] Add `SituationCmdDispatchEx` returning `SituationError`.
- [x] Keep `SituationCmdDispatch` as a compatibility wrapper around `SituationCmdDispatchEx`.
- [x] Add `SituationDispatchIndirectCommand` public struct:
  - [x] `uint32_t group_count_x`
  - [x] `uint32_t group_count_y`
  - [x] `uint32_t group_count_z`
- [x] Add `SituationCmdDispatchIndirect`.
- [x] Add `SIT_OP_DISPATCH_INDIRECT`.
- [x] Validate indirect buffer handle and offset alignment.
- [x] Add/confirm buffer usage flag for indirect arguments (`SITUATION_BUFFER_USAGE_INDIRECT` or equivalent): **existing `SITUATION_BUFFER_USAGE_INDIRECT_BUFFER` is used**.
- [x] OpenGL: bind `GL_DISPATCH_INDIRECT_BUFFER`, call `glDispatchComputeIndirect`, unbind/restore tracked state.
- [x] Vulkan: call `vkCmdDispatchIndirect`.
- [x] Add barrier guidance: writes to indirect argument buffer require `SituationCmdPipelineBarrierEx` or `SituationCmdBufferBarrier` before dispatch indirect.
- [x] Add harness test: CPU-filled indirect dispatch writes expected SSBO count.
- [x] Add harness test: compute-generated indirect dispatch argument, barrier, indirect dispatch.
- [x] Add harness test: direct dispatch `Ex` success path and invalid parameter validation.

**Exit criteria:** direct dispatch has validated error-return path; indirect dispatch is available and tested on GL+VK.

---

## Phase 3 — Barrier model upgrade

**Purpose:** move from narrow flag-only barriers to a resource-aware model suitable for compute -> graphics, transfer -> shader, render target -> texture, and indirect workflows.

Current state: `SituationCmdPipelineBarrier(cmd, src_flags, dst_flags)` exists and maps to `glMemoryBarrier` / `vkCmdPipelineBarrier`. It is useful but underspecified for resource transitions, Vulkan layouts, and transfer/copy commands.

### Probable deep dive: `SituationTextureLayout`

Texture layout is the first Phase 3 item expected to disrupt multiple renderer paths. Unlike a global memory barrier, it implies a resource state contract over time:

- Vulkan needs explicit old/new layouts for attachment, sampling, transfer, and presentation workflows.
- OpenGL has no equivalent layout state, so the API must express intent without making GL users reason in Vulkan-only terms.
- Existing texture upload, mip generation, render-pass attachment, present/readback, and future copy/blit paths already perform backend-local transitions.
- Automatic internal layout tracking would touch command-buffer recording, deferred execution, render-pass ownership, cross-frame texture reuse, and future transfer commands.

Policy for the first layout slice: treat `SituationTextureLayout` as an explicit vocabulary for user-declared intent, not as an immediate engine-wide state tracker. Prefer an opt-in `SituationTextureBarrierDesc` where callers provide `old_layout` and `new_layout`; Vulkan maps those directly to `VkImageMemoryBarrier`, while OpenGL maps to conservative `glMemoryBarrier` bits or no-op behavior where layout has no GL meaning. Do not add automatic current-layout tracking until the explicit barrier path is proven by cookbook tests and the ownership rules are documented.

Open questions to answer in the deep dive:

- Which paths already own implicit texture transitions, and should any become public barriers instead?
- What initial layout should `SituationCreateTexture` promise after upload and mip generation?
- How should render-pass load/store transitions interact with an explicit post-pass texture barrier?
- How should swapchain/present layout be represented without exposing backend-specific handles?
- Which validation errors should distinguish invalid layout pairs from unsupported backend behavior?

- [x] Inventory existing `SITUATION_BARRIER_*` flags and document exact GL/VK mapping.
- [x] Add `SituationPipelineStage` enum with backend-neutral stages:
  - [x] top/bottom
  - [x] transfer
  - [x] vertex input
  - [x] vertex shader
  - [x] fragment shader
  - [x] color attachment
  - [x] depth/stencil attachment
  - [x] compute shader
  - [x] indirect command
  - [x] host
- [x] Add `SituationAccessFlags` enum:
  - [x] indirect command read
  - [x] index/vertex read
  - [x] uniform read
  - [x] shader read/write
  - [x] color attachment read/write
  - [x] depth/stencil read/write
  - [x] transfer read/write
  - [x] host read/write
- [x] Add `SituationTextureLayout` enum for Vulkan-style intent:
  - [x] undefined
  - [x] general
  - [x] color attachment
  - [x] depth/stencil attachment
  - [x] shader read
  - [x] transfer src
  - [x] transfer dst
  - [x] present
- [x] Add `SituationPipelineBarrierDesc` for global memory barriers.
- [x] Add `SituationBufferBarrierDesc` for a buffer range.
- [x] Add `SituationTextureBarrierDesc` for a texture subresource range and old/new layout.
- [x] Implement OpenGL mapping conservatively with `glMemoryBarrier` bits and optional texture barriers where applicable.
  - [x] Current slices cover global and buffer-range barriers through the existing GL soft barrier packet.
  - [x] Texture barrier first slice validates texture ranges/usages and maps to the existing conservative GL soft barrier packet.
- [x] Implement Vulkan mapping with `VkMemoryBarrier`, `VkBufferMemoryBarrier`, and `VkImageMemoryBarrier`.
  - [x] Current slices cover `VkMemoryBarrier`, `VkBufferMemoryBarrier`, and first-slice color `VkImageMemoryBarrier`.
- [x] Keep old `SituationCmdPipelineBarrier(src_flags, dst_flags)` as a convenience wrapper.
- [x] Add docs with cookbook examples (`doc/RENDERER_BARRIER_COOKBOOK.md`):
  - [x] compute writes SSBO -> vertex shader reads (harness-backed; see SPIR-V/SSBO graphics tests)
  - [x] compute writes image -> fragment samples texture (`graphics.compute_image_write`)
  - [x] transfer writes texture -> fragment samples texture (transfer barrier + blit/copy tests)
  - [x] compute writes indirect buffer -> indirect dispatch
  - [x] Color attachment → transfer readback (cookbook + harness)
    - [x] **3a Doc:** swapchain / `SituationReadFramebuffer` / `SituationLoadImageFromScreen` — not `CopyTextureToBuffer` on swapchain
    - [x] **3b Implement:** `SituationCmdTextureBarrier` `COLOR_ATTACHMENT` ↔ `TRANSFER_SRC` for transfer-capable color textures
    - [x] **3b Harness:** `transfer.render_target_readback` (GL + VK): `EndRenderPass` → barrier → `CopyTextureToBuffer` → pixel assert
    - [x] **3c Implement:** `SituationRenderTarget` create/destroy, `SituationRenderPassInfo.render_target` routing, `SituationReadRenderTarget` @ v2.4.393
    - [x] **3c Harness:** `render_target` module (3 tests GL+VK)
- [x] Add tests for the cookbook cases where existing harness resources permit. Current proofs: `compute.dispatch_indirect_compute_generated`, `compute.dispatch_indirect_buffer_barrier`, `graphics.texture_barrier_validation`.

**Exit criteria:** synchronization API can express real resource transitions without users needing backend-specific knowledge. **Met for Phase 3 core + 3b @ v2.4.392:** global stage/access barriers, buffer-range barriers, explicit texture layout barriers, and **color attachment ↔ transfer readback** on transfer-capable color textures. Texture barriers use caller-provided old/new layout plus optional assisted hints (Phase 14); **`PRESENT`** layout and **3c** (`SituationRenderTarget` export) remain deferred.

---

## Phase 4 — Copy and blit operations

**Purpose:** expose general transfer commands instead of one special buffer-copy path and one special present path.

Current state: `SituationCmdCopyBufferEx` exists for buffer-to-buffer copies with independent source/destination offsets and explicit validation. Legacy `SituationCmdCopyBuffer` remains as a `void` wrapper that preserves shipped behavior: source offset is the old `offset` parameter and destination offset is `0`. The Phase 3/4 prerequisite `SituationCmdTextureBarrier` exposes explicit texture layout transitions for color-only 2D texture mip ranges. `SituationCmdBlitTexture`, `SituationCmdCopyTexture`, `SituationCmdCopyBufferToTexture`, and `SituationCmdCopyTextureToBuffer` share matching RGBA8 color textures, 2D/layer 0, explicit caller-owned transfer layouts, and no implicit Y flip.

### Blit region design notes

`SituationTextureBlitRegion` is a high-leverage API surface and should not be implemented until its semantics are pinned down. Target shape: source rectangle, destination rectangle, source/destination mip level, source/destination array layer, and filter mode. Rectangles use `x`, `y`, `width`, `height` fields in Situation API space; backend implementations convert to endpoint pairs where needed. It should map cleanly to Vulkan `VkImageBlit` while staying understandable for OpenGL/FBO blit users.

Design decisions and remaining TODOs:

- [x] Define coordinate convention: use `x`, `y`, `width`, `height` rectangles in public API structs; backend code derives endpoint pairs internally.
- [x] Define Situation-space Y origin for blits, and whether backend Y-flip is ever implicit. **Decision after code scan:** public blit rectangles use Situation 2D API space: origin is top-left, `y` increases downward, matching scissor, text/quad ortho, Virtual Display offsets, and `SitRectangle` draw APIs. Backends may convert internally for GL/VK coordinate systems, but `SituationCmdBlitTexture` must not introduce backend-dependent implicit content flips: source top-left maps to destination top-left. Explicit flip/mirror behavior is deferred to a future flag or transform API, not hidden in backend mapping.
- [x] Decide first slice scope: likely color-only, 2D textures, mip 0, layer 0, nearest/linear filter. **Decision:** first implementation slice is color-only 2D texture blit, mip 0, layer 0.
- [x] Define filter behavior: nearest vs linear, and which formats forbid linear filtering. **Decision:** default/zero filter is nearest. Linear is explicit and only valid for color formats/usages that are filterable on the active backend. Reject linear for depth/stencil, integer/data formats, and storage-only/data-oriented textures unless later capability checks prove safe support.
- [x] Defer or explicitly reject depth/stencil blits until depth/stencil attachment ownership is settled. **Decision:** first blit slice is color-only. Depth/stencil blit requests should return an explicit unsupported/error result until render-target depth/stencil ownership and stencil behavior are settled.
- [x] Define usage requirements: source must have transfer-src/read usage, destination must have transfer-dst/write usage. **Decision:** first copy/blit command slices are strict by default. Missing transfer usage returns `SITUATION_ERROR_TEXTURE_INVALID_USAGE` for textures or `SITUATION_ERROR_BUFFER_INVALID_USAGE` for buffers. Future convenience/fallback behavior should be opt-in through renderer behavior policy commands, not silent backend-specific fallback.
- [x] Define layout expectations: explicit texture barriers before/after blit once `SituationTextureLayout` lands. **Decision:** strict/default mode does not perform hidden pre/post layout transitions. Users should express `barrier into blit usage -> blit -> barrier into next usage` with `SituationCmdTextureBarrier`. Future assisted transitions belong to Phase 14 behavior policy commands, not the default blit contract.
- [x] Add tests for same-size blit/copy parity and scaled blit readback before broadening scope. **Decision:** first blit tests must include asymmetric source pixels/rectangles so vertical flips, wrong origin, and nearest/linear sampling mistakes are visible in readback.

- [x] Add `SituationCmdCopyBufferEx` with independent `src_offset` and `dst_offset`.
- [x] Keep `SituationCmdCopyBuffer` as compatibility wrapper using the same offset for source/destination or define it as source offset 0 -> destination offset 0 (must match current behavior after audit). **Audit result:** legacy behavior was `src_offset = offset`, `dst_offset = 0`; the wrapper preserves that.
- [x] Define `SituationTextureCopyRegion`:
  - [x] mip level
  - [x] array layer (layer 0 only in first slice)
  - [x] source rect plus destination `dst_x`/`dst_y` (exact-size copy; no scaling)
  - [ ] row pitch for buffer copies where needed (deferred to buffer↔texture commands)
- [x] Define `SituationTextureBlitRegion` with source/destination rectangles and mip/layer fields.
- [x] Add `SituationCmdCopyTexture`.
- [x] Add `SituationCmdBlitTexture`.
- [x] Add `SituationCmdCopyBufferToTexture`.
- [x] Add `SituationCmdCopyTextureToBuffer`.
- [x] Add usage flag validation for transfer source/destination buffers/textures.
  - [x] Buffer copy validates `SITUATION_BUFFER_USAGE_TRANSFER_SRC` and `SITUATION_BUFFER_USAGE_TRANSFER_DST`.
  - [x] Texture barriers validate transfer usage before entering `TRANSFER_SRC` / `TRANSFER_DST`.
- [x] OpenGL:
  - [x] `glCopyImageSubData` for texture -> texture copy.
  - [x] FBO blit fallback for compatible color textures (blit path).
  - [x] PBO path for buffer <-> texture (`glTextureSubImage2D` / `glGetTextureSubImage`).
- [x] Vulkan:
  - [x] `vkCmdCopyImage` (public copy command)
  - [x] `vkCmdBlitImage`
  - [x] `vkCmdCopyBufferToImage` (public upload command)
  - [x] `vkCmdCopyImageToBuffer` (public readback command)
  - [x] integrate with new texture barriers/layouts for the first blit slice.
- [x] Add tests:
  - [x] buffer copy with different source/destination offsets.
  - [x] texture -> texture copy, read back destination.
  - [x] texture blit scaling, read back destination.
  - [x] buffer -> texture upload via command buffer.
  - [x] texture -> buffer readback via command buffer.

**Exit criteria:** users can express common transfer workflows without special-case APIs or immediate stalls.

---

## Phase 4.1 — Transfer harness split

**Purpose:** keep Phase 4 command-buffer transfer tests in a dedicated harness module so `test_graphics.c` does not accumulate another large batch of recorded-command tests. Enables fast, focused runs: `sit_test.exe --module transfer`.

**Scope (moved out of `test_graphics.c`):**

- `SituationCmdTextureBarrier` validation
- `SituationCmdBlitTexture` validation + readback
- `SituationCmdCopyTexture` validation + readback
- `SituationCmdCopyBufferToTexture` / `SituationCmdCopyTextureToBuffer` validation + readback
- `SituationCmdCopyBufferEx` validation + offset readback

**Not in this module (stay elsewhere):**

- **Compute:** dispatch, pipeline/buffer barriers, SSBO proofs → `test_compute.c`
- **Graphics interop:** compute writes texture then graphics samples → `test_graphics.c` (future)
- **SPIR-V / draw / VD / raster:** → `test_graphics.c` + `test_graphics_spirv.c`

### Phase 4.1A — Create the transfer module

- [x] Create `tests/harness/test_transfer.c` using the existing `SitTestModule` pattern (`g_module_transfer`, module name `transfer`).
- [x] Move Phase 4 transfer tests out of `test_graphics.c` (no duplicate registration in graphics).
- [x] Add module-local setup/teardown and small helpers (`transfer_texture_barrier`, pixel assert helpers).

### Phase 4.1B — Wire the harness

- [x] Register `g_module_transfer` in `tests/harness/sit_test_registry.c` (after compute, before misc).
- [x] Add `test_transfer.c` to `build_tests.bat` for OpenGL and Vulkan harness executables.
- [x] Confirm filters:
  - `sit_test.exe --module transfer`
  - `sit_test.exe --module transfer --filter copy_texture`

### Phase 4.1C — Plan matrix labels

| Command / area | Harness labels |
|----------------|----------------|
| `SituationCmdTextureBarrier` | `transfer.texture_barrier_validation` |
| `SituationCmdBlitTexture` | `transfer.blit_texture_*` |
| `SituationCmdCopyTexture` | `transfer.copy_texture_*` |
| `SituationCmdCopyBufferToTexture` / `SituationCmdCopyTextureToBuffer` | `transfer.copy_buffer_to_texture_*`, `transfer.copy_texture_to_buffer_*` |
| `SituationCmdCopyBufferEx` | `transfer.copy_buffer_ex_*` |

- [x] Use `transfer.*` labels in the API matrix (replacing `graphics.*` for moved tests). **Policy:** any new copy/blit/barrier harness tests register under `transfer.*`, not `graphics.copy_*`.

### Phase 4.1D — Optional transfer follow-ups (non-blocking)

Phase **4.1 exit criteria met** @ transfer module split. Items below are **API-gated or cookbook-gated** — do not block Phases 9–13.

- [ ] Buffer-row pitch upload tests for `CopyBufferToTexture` when API gains `src_row_pitch` (today: tightly packed rows only).
- [ ] Transfer-only interop in **`test_transfer.c`**: SSBO fill → barrier → `CopyBufferToTexture` (buffer-only assertion). **Compute → fragment sample** stays in **`test_graphics.c`**.
- [x] Phase **3 cookbook 3b** harness **`transfer.render_target_readback`** @ v2.4.392 (GL + VK; also in **`test_transfer.c`**, not graphics).

**Exit criteria (Phase 4.1 — met):** `--module transfer` runs all Phase 4 transfer command tests on GL+VK without pulling the full graphics module; `test_graphics.c` no longer registers those cases.

---

## Phase 5 — Indirect draw support

**Purpose:** expose explicit indirect draw commands while preserving internal MDI batching.

Current state: OpenGL internally uses MDI to batch normal draws, but users cannot supply an indirect command buffer. Vulkan public indirect draw commands are not exposed.

- [x] Add public structs matching API layout, not backend names:
  - [x] `SituationDrawIndirectCommand`
  - [x] `SituationDrawIndexedIndirectCommand`
- [x] Verify struct memory layout matches Vulkan and OpenGL requirements:
  - [x] draw arrays: vertex count, instance count, first vertex, first instance.
  - [x] indexed: index count, instance count, first index, vertex offset, first instance.
- [x] Add `SituationCmdDrawIndirect`.
- [x] Add `SituationCmdDrawIndexedIndirect`.
- [x] Add `SIT_OP_DRAW_INDIRECT` and `SIT_OP_DRAW_INDEXED_INDIRECT`.
- [x] Add buffer usage validation for indirect command buffers.
- [x] Define required stride behavior and minimum stride. **Decision:** first slice `draw_count = 1`; stride = `sizeof(command struct)`.
- [x] Define interaction with currently bound vertex/index buffers and pipeline. **Decision:** same bindings as `SituationCmdDraw` / `DrawIndexed`; active render pass required.
- [x] OpenGL:
  - [x] bind `GL_DRAW_INDIRECT_BUFFER`.
  - [x] call `glDrawArraysIndirect` (single draw; MDI batching remains internal for `SIT_OP_DRAW`).
  - [x] call `glDrawElementsIndirect` (`GL_UNSIGNED_INT`; Phase 7 for index-type flexibility).
  - [ ] `glMultiDraw*Indirect` from user buffer (deferred).
- [x] Vulkan:
  - [x] call `vkCmdDrawIndirect`.
  - [x] call `vkCmdDrawIndexedIndirect`.
- [x] Add optional future note for count variants requiring GL `ARB_indirect_parameters` / Vulkan draw-indirect-count feature.
- [x] Add tests:
  - [x] CPU-filled indirect draw draws expected pixels.
  - [x] CPU-filled indexed indirect draw draws expected pixels.
  - [x] compute-generated indirect draw arguments with barrier.

**Exit criteria:** public indirect draws work on both backends and coexist with internal OpenGL MDI batching. **First slice met**; multi-draw/count variants remain follow-up.

---

## Phase 6 — Raster state completion

**Purpose:** finish the common fixed-function state layer so examples do not need backend-specific escape hatches.

**Status:** **Closed (v2.4.187).** Fixed-function raster command layer is complete on both backends (harness green). **End-to-end MSAA** is intentionally **out of scope** — see **§ MSAA / multisample (unified model)** below. **`SituationCmdSetMultisampleState`** command plumbing shipped separately (**v2.4.256**).

**Exit criteria (met):** common fixed-function state can be recorded and replayed deterministically on both backends. Multisample **attachment quality** (VD-4b) is not part of Phase 6 closure.

### MSAA / multisample — unified model (not Phase 6; no dedicated MSAA phase)

There is **no separate “MSAA phase”** and **no three parallel multisample APIs**. MSAA is **one feature** with **two layers**:

| Layer | Public surface | Role | v2.4 today | Ships with |
|-------|----------------|------|------------|------------|
| **1 — Attachment quality** | **`SituationMultisampleQuality`** on VD/RT desc + configure; main-window init (folds **`SITUATION_FLAG_MSAA_4X_HINT`**) | How many samples the target has; drives MSAA FBO, resolve, pipeline **`sample_count`** | Shipped as **`int msaa_samples`** — values **`> 1` rejected**; **`SituationGetGraphicsCaps::max_msaa_samples`** queried | **VD-4b** + RT MSAA (**v2.5 default**) |
| **2 — In-pass raster flags** | **`SituationMultisampleState`** + **`SituationCmdSetMultisampleState`** | Sample shading, sample mask, alpha-to-coverage on an **already multisampled** attachment | **Plumbing shipped v2.4.256**: GL soft replay (no-op on single-sample FBOs); VK shadow + push/pop; pipelines always **`VK_SAMPLE_COUNT_1_BIT`** until layer 1 lands | **VD-4b** wires VK pipeline bake + meaningful draws |

**Public stance until VD-4b:** MSAA is **not a supported end-user feature** — caps query and cmd record/replay exist as **foundation**, not as optional modes.

**Legacy entry points (to fold, not maintain in parallel):**

| Legacy | Fate at VD-4b |
|--------|----------------|
| **`int msaa_samples`** on VD/RT desc | Replace with **`SituationMultisampleQuality msaa_quality`** |
| **`SituationSetVirtualDisplayMsaaSamples`** (planned name) | **`SituationSetVirtualDisplayMultisampleQuality`** |
| **`SITUATION_FLAG_MSAA_4X_HINT`** | Deprecate → **`SITUATION_MULTISAMPLE_4X`** at init (main-window / swapchain track) |

**Where work lives in this plan:**

| Track | Location | Scope |
|-------|----------|--------|
| **VD attachment + color + sampler** | **§ Phase 11 — VD-1 … VD-5 ✅ v2.4.387** | Load/store inherit, optional depth, sRGB/clear, composite sampler, aniso/mips, update mode |
| **VD MSAA + resolve (layer 1 + layer 2 completion)** | **VD-4b** (v2.5 default) | MSAA attachments, resolve, enum migration, pipeline **`sample_count`**, **`SituationCmdSetMultisampleState`** end-to-end |
| **RT MSAA policy** | **`doc/plan/v2.5-api-expansion.md` Phase 6-MSAA** | Same enum + resolve for **`SituationRenderTarget`** |

**Why Phase 6 stopped short of end-to-end MSAA:** layer-2 flags only affect rendering once layer-1 attachments exist and resolve before composite. Phase 6 closure covers fixed-function raster **excluding** attachment MSAA.

**When MSAA is “done”:** **VD-4b** checklist green (see **§ Scope gate — VD-4b MSAA**) — not when **`SituationCmdSetMultisampleState`** alone returns success.

### Phase 6A — Front face + primitive topology (**done**, v2.4.161–167)

- [x] Add `SituationFrontFace` enum:
  - [x] `SIT_FRONT_FACE_CCW`
  - [x] `SIT_FRONT_FACE_CW`
- [x] Add `SituationPrimitiveTopology` enum:
  - [x] triangle list
  - [x] triangle strip
  - [x] line list
  - [x] line strip
  - [x] point list
  - [ ] **[v2.4.187]** deferred: patches/tessellation
- [x] Add `SituationPolygonMode` enum:
  - [x] fill
  - [x] line
  - [x] point
- [x] Add `SituationStencilState`:
  - [x] compare op
  - [x] fail op
  - [x] depth fail op
  - [x] pass op
  - [x] compare mask
  - [x] write mask
  - [x] reference
- [x] Add `SituationMultisampleState` (type only — fields match planned command):
  - [x] sample shading enable
  - [x] min sample shading
  - [x] sample mask
  - [x] alpha-to-coverage
- [x] Implement `SituationCmdSetFrontFace`.
- [x] Implement `SituationCmdSetPrimitiveTopology`.
- [x] Implement `SituationCmdSetPolygonMode`.
- [x] Implement `SituationCmdSetDepthBias`.
- [x] Implement `SituationCmdSetLineWidth`.
- [x] Implement `SituationCmdSetColorWriteMask`.
- [x] Implement `SituationCmdSetStencilTest`.
- [x] Implement `SituationCmdSetMultisampleState` — **plumbing only (v2.4.256)**; **end-to-end MSAA deferred** to **VD-4b** (GL no-op on single-sample targets; VK shadow until pipeline **`sample_count`** variants).
- [x] Document Vulkan dynamic-state vs static-variant policy (**§ Vulkan raster state policy**, v2.4.187).
- [ ] `[future port]` Decide GL fallback/error behavior for polygon mode on ES/Web targets if/when ported.
- [x] Implement actual push/pop raster state stack or mark `SituationCmdPushRasterState` / `SituationCmdPopRasterState` deprecated until stack exists.
- [x] Add tests for front-face/cull interaction.
  - [x] Verify front-face/cull interaction parity on Vulkan harness (6-bisG closure).
- [x] Add tests for topology line drawing where harness can read pixels.
- [x] Add tests for topology point drawing where harness can read pixels.
- [x] Add tests for depth bias using a deterministic overlap case.
- [x] Add tests for color write mask.
- [x] Add stencil tests if stencil attachment is available.

**Exit criteria:** common fixed-function state can be recorded and replayed deterministically on both backends.

**Phase 6A harness (verified 2026-05-28):**

- [x] `sit_test.exe --module graphics --filter front_face` — 1/1
- [x] `sit_test.exe --module graphics --filter primitive_topology` — 2/2 (GL point-list required `current_primitive_mode_set`; `GL_POINTS` is 0)
- [x] `sit_test_vulkan.exe --module graphics --filter front_face` — 1/1
- [x] `sit_test_vulkan.exe --module graphics --filter primitive_topology` — 2/2

### Phase 6B — Remaining raster state (**done**, v2.4.183–186)

- [x] Polygon mode + depth bias APIs + GL soft replay + Vulkan dynamic state (`POLYGON_MODE`, `DEPTH_BIAS_*`)
- [x] Tests: `polygon_mode_line_wireframe`, `depth_bias_overlap` (**OpenGL module-order green at v2.4.183**)
- [x] Vulkan metrics overlay / default-font text orientation (**v2.4.176**) — **`draw_metrics_overlay`** green; live window top-left HUD
- [x] **`polygon_mode_line_wireframe`** on Vulkan — green (GTX 1070, verified v2.4.187)
- [x] Line width, color write mask, stencil, push/pop APIs + GL/VK paths
- [x] OpenGL: line width, color write mask, stencil test, push/pop stack (**v2.4.185**)
- [x] Vulkan: line width dynamic state in pipelines (**v2.4.185**)
- [x] Vulkan: color write / stencil / push-pop (**v2.4.186**, **6-bisH**)
- [x] Tests: color write mask, stencil (if attachment available), push/pop, line width validation
- [x] Push/pop raster stack (**`SITUATION_MAX_RASTER_STACK_DEPTH` 256**; GL execute-time; VK tracked-state **v2.4.186**)

### Phase 6 closure — **v2.4.187** (shipped)

Documentation and hygiene slice that **closes Phase 6** without implying multisample shipped:

| Item | Status | Notes |
|------|--------|-------|
| Vulkan **`polygon_mode_line_wireframe`** | **Done** | Verified green post–v2.4.186 |
| **§ Vulkan raster state policy** | **Done** | Dynamic vs static-variant table below |
| **6-bisC** variant debug counters | **Done** | Debug builds; log on **`_SituationCleanupVulkan`** |
| **6-bisD** harness assert cleanup | **Done** | **`sit_test_fail_impl`** → crash cleanup |
| **`SITUATION_MAX_RASTER_STACK_DEPTH` (256)** | **Done** | Unified in **`situation_api.h`** |
| **`SituationCmdSetMultisampleState`** | **Plumbing only (v2.4.256)** | End-to-end MSAA → **VD-4b** (see **§ MSAA / multisample**) |
| **6-bisA** topology in variant key | **Optional / open** | Not required for Phase 6 closure |
| Patches/tessellation topology | **`[future]`** | Not Phase 6 |
| GL ES/Web polygon-mode port | **`[future port]`** | Not Phase 6 |
| Vulkan 2D projection debt audit | **`[closed v2.4.177]`** | Phase 7-bis |

### Vulkan raster state policy (**v2.4.187**)

Internal policy for Phase 6 public raster APIs on Vulkan (OpenGL uses soft-command GL state replay unless noted):

| Public API / state | Mechanism | Notes |
|--------------------|-----------|-------|
| Viewport / scissor | **Dynamic** (`VK_DYNAMIC_STATE_VIEWPORT`, `SCISSOR`) | Re-applied on pipeline bind via **`_SitVulkanApplyTrackedRasterDynamics`**. |
| Primitive topology | **Dynamic** (`VK_EXT_extended_dynamic_state`) | Tracked default **`TRIANGLE_LIST`** on frame acquire + pipeline rebind. |
| Cull mode + front face | **Static pipeline variants** (6-bis) | **`_SitVulkanSelectRasterVariant`** when **`CULL_BACK`**; dynamic cull/front not used for determinism. |
| Polygon mode **LINE** | **Static `vk_pipeline_*_line` variants** | **`extendedDynamicState3PolygonMode` intentionally off** — dyn3 would override static line mode. |
| Polygon mode **POINT** | **Dynamic** (`vkCmdSetPolygonModeEXT`) when dyn3 enabled | Otherwise **`NOT_IMPLEMENTED`**. |
| Depth test / write / compare | **Dynamic** (`VK_EXT_extended_dynamic_state`) | Tracked fields; push/pop restores via **`_SitVulkanApplyTrackedExtendedRasterDynamics`**. |
| Depth bias | **Dynamic** (`VK_EXT_extended_dynamic_state2`) when enabled | **`NOT_IMPLEMENTED`** for enable when feature missing. |
| Line width | **Dynamic** (`VK_DYNAMIC_STATE_LINE_WIDTH`) | Non-1.0 requires **`wideLines`**. |
| Color write mask | **Dynamic** (`VK_EXT_extended_dynamic_state3`) | Partial mask **`NOT_IMPLEMENTED`** without feature. |
| Stencil test / ops | **Dynamic** (ext dyn1 test/op + core compare/ref/mask) | Enable requires D24S8/D32S8 depth format. |
| Push / pop raster stack | **Tracked CPU stack** + **`vkCmd*`** on pop | Depth **256** — **`SITUATION_MAX_RASTER_STACK_DEPTH`**. |
| Multisample command | **Plumbing (v2.4.256)** | **`SituationCmdSetMultisampleState`** records + push/pop; **no MSAA attachments** → no visible effect until **VD-4b** (see **§ MSAA / multisample**). |

---

## Phase 6-bis — Vulkan raster-state parity fallback

**Purpose:** guarantee front-face/cull/topology correctness on Vulkan stacks where dynamic raster-state behavior is unreliable, while preserving backend-neutral public APIs.

**Why this exists:** Phase 6 introduced `SituationCmdSetFrontFace` and `SituationCmdSetPrimitiveTopology`, but Vulkan harness parity for front-face/cull interaction remains unstable. Dynamic-state commands alone are not sufficient on all driver/feature combinations.

### Scope

- Keep current public API unchanged (`SituationCmdSetCullMode`, `SituationCmdSetFrontFace`, `SituationCmdSetPrimitiveTopology`).
- Add a Vulkan-internal **pipeline variant fallback** path for raster state tuples.
- Keep dynamic topology state; use static pipeline raster variants for cull/front-face determinism.
- Do not introduce backend-specific public handles or knobs in this phase.

### Current implementation status

- Vulkan builds static raster variants for shader pipeline families (`vk_pipeline`, `vk_pipeline_legacy`, `vk_pipeline_simple`) with explicit back-face + front-face combinations.
- Unified selector (`_SitVulkanResolveGraphicsPipeline` / `_SitVulkanEnsureGraphicsPipelineBound`) routes all graphics bind/draw entry points.
- Topology uses tracked dynamic default (`TRIANGLE_LIST`) applied on pipeline bind/rebind.
- **Closure:** `front_face_cull_interaction` green on Vulkan after rebuild + topology-default hardening (6-bisG).

### 6-bisA — Raster state key + variant policy

- [x] Define internal Vulkan raster-state key for graphics draw pipelines:
  - [x] cull mode (`NONE/BACK/FRONT`) - implemented fallback coverage for `BACK` parity path.
  - [x] front face (`CW/CCW`)
  - [ ] **[v2.4.187]** primitive topology (initially list/strip/line/point values already in API)
- [x] Define policy:
  - [x] dynamic-state path remains enabled (topology)
  - [x] variant path is authoritative fallback for cull/front-face
- [x] Ensure keying works with existing pipeline family (`vk_pipeline`, `vk_pipeline_legacy`, `vk_pipeline_simple`) without exploding combinatorics.

### 6-bisB — Variant creation and selection

- [x] Add internal helper to materialize/select rasterized Vulkan pipeline variants for the active shader slot.
- [x] Ensure draw paths that currently rebind based on vertex stride also apply the requested raster-state variant.
- [x] Keep current descriptor/push-constant layout behavior unchanged.
- [x] Ensure fallback works for:
  - [x] `SituationCmdDraw`
  - [x] `SituationCmdDrawIndexed`
  - [x] `SituationCmdDrawIndirect`
  - [x] `SituationCmdDrawIndexedIndirect`
  - [x] `SituationCmdDrawMesh` stride-based pipeline selection

### 6-bisC — Lifetime and cache discipline

- [x] Add cache ownership/lifetime rules so variant pipelines are destroyed with shader slot teardown/hot-reload.
- [x] Prevent duplicate variant creation across repeated state toggles in one run.
- [x] **[v2.4.187]** Add debug diagnostics counter/logging for variant hits/misses (debug-only; shutdown log in **`_SituationCleanupVulkan`**).

### 6-bisD — Validation matrix

- [x] OpenGL regression:
  - [x] `sit_test.exe --module graphics --filter front_face` stays green
  - [x] `sit_test.exe --module graphics --filter primitive_topology` stays green
- [x] Vulkan parity:
  - [x] `sit_test_vulkan.exe --module graphics --filter front_face` green (`front_face_cull_interaction`)
  - [x] `sit_test_vulkan.exe --module graphics --filter primitive_topology` green
- [x] **[v2.4.187]** Harness assert path invokes registered crash cleanup (**`sit_test_fail_impl`**).

### 6-bisE — Documentation and status updates

- [x] Update `doc/UPDATELOG.md` with explicit OpenGL and Vulkan verification lines for the parity fix.
- [x] Update this plan section:
  - [x] mark Vulkan parity sub-item under Phase 6 front-face tests complete
  - [x] note whether dynamic state remains active fast path vs fully replaced by variant selection

### 6-bisF — Deterministic selection hardening (closure slice)

- [x] Consolidate Vulkan graphics pipeline choice into one internal selector (family + raster variant) shared by:
  - [x] `SituationCmdBindPipeline`
  - [x] `SituationCmdBindVertexBuffer`
  - [x] `SituationCmdDraw` / `SituationCmdDrawIndexed`
  - [x] `SituationCmdDrawIndirect` / `SituationCmdDrawIndexedIndirect`
  - [x] `SituationCmdDrawMesh`
- [x] Remove residual mixed-policy behavior (no split dynamic/static cull-front paths).
- [x] Add debug-only invariants for impossible selection states (null/mismatched family-variant tuple).
- [x] Re-run focused Vulkan parity (`front_face_cull_interaction`) and promote only when green.
- [x] Keep this slice narrow: architecture cleanup only for deterministic behavior, no public API changes.

### 6-bisG — Vulkan front-face parity closure

- [x] Isolate remaining mismatch in `front_face_cull_interaction` with a strict cause-first audit:
  - [x] confirm clip-space orientation + viewport transform assumptions used by this test on Vulkan
  - [x] confirm rasterizer `frontFace` mapping aligns with the actual winding at rasterization time (static variants + unified selector)
  - [x] confirm no hidden state mutation between pass 1 and pass 2 (tracked topology default on rebind)
- [x] Apply the smallest deterministic fix (topology default on pipeline bind/rebind + selector hardening), without introducing backend-specific API behavior.
- [x] Re-run focused validation:
  - [x] `sit_test_vulkan.exe --module graphics --filter front_face_cull_interaction`
  - [x] `sit_test_vulkan.exe --module graphics --filter primitive_topology_line_list`
  - [x] OpenGL front-face regression still green
- [x] Promote 6-bis from in-progress to complete only after Vulkan front-face parity is green and documented in `UPDATELOG.md`.

**Exit criteria:** Vulkan front-face/cull interaction and primitive topology tests are green with deterministic behavior across targeted drivers, and the fallback architecture is documented as an internal renderer policy (not a public API fork).

### 6-bisH — Vulkan Phase 6B raster dynamic parity

**Purpose:** bring **`SituationCmdSetColorWriteMask`**, **`SituationCmdSetStencilTest`**, and **`SituationCmdPushRasterState`** / **`PopRasterState`** to Vulkan parity with OpenGL using **`VK_EXT_extended_dynamic_state3`** color write mask, **`VK_EXT_extended_dynamic_state`** stencil test/op, core stencil compare/write/reference dynamics, plus tracked depth/line-width state.

**Scope:**

- Keep public API unchanged; no backend-specific knobs.
- Track raster fields per frame (`dynamic_color_write_mask`, depth/stencil/line-width/bias, cull/front-face) and re-apply on pipeline rebind.
- Push/pop captures/restores tracked state and re-issues `vkCmd*` (pipeline rebind when cull/front-face variant changes).
- Stencil enable requires depth/stencil attachment (`D32S8` / `D24S8`); disable via `vkCmdSetStencilTestEnable(false)`.
- Multisample **end-to-end** remains deferred until **VD-4b**; cmd plumbing shipped v2.4.256.

- [x] Enable `extendedDynamicState3ColorWriteMask` at device create (polygon mode dyn3 stays off).
- [x] Load `vkCmdSetColorWriteMaskEXT`, `vkCmdSetStencilTestEnable`, `vkCmdSetStencilOp` (+ core compare/write/reference).
- [x] Add dynamic pipeline states: `COLOR_WRITE_MASK_EXT`, `STENCIL_TEST_ENABLE`, `STENCIL_OP`, compare/write/reference masks.
- [x] Wire **`SituationCmdSetColorWriteMask`**, **`SetStencilTest`**, **`PushRasterState`**, **`PopRasterState`**; track depth/line width for stack restore.
- [x] Reset tracked raster + stack depth on frame acquire.
- [x] Validation matrix:
  - [x] `sit_test_vulkan.exe --module graphics --filter color_write_mask_blocks_red`
  - [x] `sit_test_vulkan.exe --module graphics --filter push_pop_raster_color_mask`
  - [x] `sit_test_vulkan.exe --module graphics --filter stencil_test_command_conditional`
  - [x] `sit_test_vulkan.exe --module graphics --filter line_width_command`
  - [x] OpenGL regression filters above stay green
  - [x] Full Vulkan harness **424/424** after rebuild (**v2.4.186**, GTX 1070)
- [x] Document in **`doc/UPDATELOG.md`** (**v2.4.186**)

---

## Phase 7 — Index type flexibility

**Purpose:** unblock compact meshes and common asset formats that use 16-bit indices.

Current state: `SituationCmdBindIndexBuffer` is hardcoded/documented as 32-bit.

- [x] Add `SituationIndexType`.
- [x] Add `SituationCmdBindIndexBufferEx(cmd, buffer, offset, index_type)`.
- [x] Keep existing `SituationCmdBindIndexBuffer` as `SIT_INDEX_UINT32`.
- [x] Extend `bind_ibo` packet with index type.
- [x] Track current index type in GL state for `glDrawElements*`.
- [x] Map `SIT_INDEX_UINT16` to `GL_UNSIGNED_SHORT` / `VK_INDEX_TYPE_UINT16`.
- [x] Map `SIT_INDEX_UINT32` to `GL_UNSIGNED_INT` / `VK_INDEX_TYPE_UINT32`.
- [x] Validate index-buffer offset alignment based on type.
- [ ] Update high-level mesh creation if it remains strictly `uint32_t`; decide if `SituationCreateMeshEx` should allow `uint16_t`. **Deferred:** `SituationCreateMesh` remains `uint32_t` indices; use buffers + `BindIndexBufferEx` for UINT16 paths.
- [x] Add tests for 16-bit indexed draw.
- [x] Add tests for invalid offset alignment.

**Exit criteria:** indexed draw can use 16-bit or 32-bit indices with clear validation.

**Verified (2026-05-28):** OpenGL and Vulkan harness `bind_index_buffer` filter — 3/3 pass each.

---

## Phase 7-bis — Vulkan 2D projection / orientation cleanup

**Purpose:** replace the accumulated per-path Y/UV compensations with **one** clip-space adjustment on the view-projection UBO, so on-screen rendering, VD compositing, text, textured quads, and CPU readback all agree without ad hoc flips.

**Status (v2.4.177):** **Complete.** Unified fix: `_SitVulkanFillOrthoProjection2D` + Vulkan quad/text **shader** V flip and text vertex `target_h - y` push constant. CPU workarounds removed. **`projection[1][1] *= -1`** after top-left `glm_ortho` remains **invalid** (see blocker note below).

**Blocker note (do not regress):** Applying `projection[1][1] *= -1.0f` after `glm_ortho(0, w, h, 0, …)` clips internal-quad geometry because that ortho already uses negative Y scale (`bottom=h`, `top=0`).

**Readback / `SituationImageFlip` policy (do not conflate with the above):**

- **OpenGL `SituationLoadImageFromScreen`:** keep `SituationImageFlip(SIT_FLIP_VERTICAL)` — required because `glReadPixels` returns bottom row first (including pre-swap `screenshot_buffer` in `EndFrame`). **v2.4.183:** call **`glFinish()`** before capture so the back buffer is complete (fixes flaky strict readback tests).
- **Vulkan `SituationLoadImageFromScreen`:** **no** `SituationImageFlip` on the pre-present screenshot cache path — `vkCmdCopyImageToBuffer` rows already match top-left display order (v2.4.169). Adding a readback flip to “match OpenGL” would invert harness pixels vs the live window.
- **VD:** no separate image flip; orientation follows the same ortho + compositor quad path as the main window.

**Why piecemeal removal broke things:** a naïve projection-only change without removing UV/text workarounds regressed multiple `graphics` readback tests; v2.4.176 fixed live-window text by **adding** Y/V helpers, not by unified projection. Removing one workaround in isolation breaks **on-screen** output, not just tests.

### 7-bisA — Audit ortho / view-proj write sites

- [x] `SituationCmdBeginRenderPass` and VD compositor use `_SitVulkanFillOrthoProjection2D`.
- [x] OpenGL paths unchanged (no Vulkan shader convention on GL).

### 7-bisB — Single 2D ortho helper (not `[1][1]` negation)

- [x] `_SitVulkanFillOrthoProjection2D` — `glm_ortho(0, w, h, 0, …)` at UBO write time.
- [x] Vulkan quad/text shaders: V flip in FS; text VS uses `target_h` push constant.
- [x] No `SituationImageFlip` on Vulkan readback.

### 7-bisC — Remove draw-time workarounds (same commit as 7-bisB)

- [x] Removed Vulkan UV flip in `SituationCmdDrawTexture`.
- [x] Removed CPU text transforms and grid-font V swap.

### 7-bisD — Validation

- [x] Full Vulkan harness on reference GTX 1070: **417/417** (83/83 graphics, 21/21 virtual_display) at v2.4.177.
- [x] `SituationLoadImageFromScreen` doc: shader convention, no Vulkan flip on cache path.
- [x] `doc/UPDATELOG.md` v2.4.177.

**Exit criteria:** Vulkan 2D draws (quads, textures, text, VD composite) and `SituationLoadImageFromScreen` readback match live-window orientation **without** per-path UV/Y mirrors; OpenGL readback flip unchanged; no new readback flip on Vulkan cache path.

### 7-bisE — Viewport/scissor sanitisation (proposed v2.4.221)

**v2.4.220** aligned tracked user-draw hygiene with OpenGL-parity viewport. **Structural cleanup** (merge duplicate helpers, dedupe `_SitVulkanApplyTrackedRasterDynamics` on draw paths) is specified in **`doc/plan/VULKAN_VIEWPORT_SCISSOR_SANITISATION_PLAN.md`** — behaviour must not change.

**Non-goals:**

- [ ] Do not “fix” orientation by flipping readback to pass harness tests.
- [ ] Do not remove OpenGL `SituationImageFlip` in `LoadImageFromScreen`.
- [ ] `build/situation_full.c` is a regenerated amalgamation — not source of truth for flip behavior.

**Suggested timing:** after Phase 7 index-type work is stable; can proceed in parallel with remaining Phase 6 raster items but **must** land UV/text/projection changes atomically.

---

## Phase 7-bisF — Internal 2D projection contract (Vulkan VD recording order)

**Added @ v2.4.352 plan revision.** Documents correctness fix for VK vs GL VD quad mismatch; complements Phase 7-bis (orientation) — **orthogonal** to Y-flip / readback policy.

**Status:** **Correctness shipped (v2.4.351)** — push-constant `mat4 projection` on every internal `SituationCmdDrawQuad` / `DrawTexture` / YPQ draw. **Lifecycle** for interim `quad_solid_texture` remains open — **`LIBRARY_RECOVERY_PLAN_244.md` §B.5 (B-L6, B-L7)**.

### Problem

OpenGL sets `u_projection` **per draw at execute time**. Vulkan historically wrote ortho into the **shared view UBO** during command recording. When VD panel draws are recorded, then `SituationRenderVirtualDisplays` runs and overwrites the same UBO with main-window ortho **before** `vkQueueSubmit`, the GPU executes VD draws with the **final** UBO contents → wrong scale (tiny flat tiles in `advanced.all_displays_windowed_fullscreen_cycle`).

### Contract (do not regress)

| Rule | Detail |
| ---- | ------ |
| **Per-draw projection** | Internal 2D draws carry ortho in **push constants** (`_SitVulkanFillQuadPushProjectionForActiveTarget` from active VD/target size). |
| **View UBO scope** | Shared view-projection UBO is for compositor / user mesh paths that explicitly refresh it for the **current** pass — not a stable carrier across interleaved VD + main-window recording. |
| **OpenGL parity** | Behavior parity (VD card size/orbit), not identical mechanism (GL still uses per-draw uniform refresh at execute). |
| **Set 1 on solid quad** | Vulkan FS declares `layout(set=1) sampler2D`; solid draws bind internal white sampler (v352 interim). GL uses `use_texture=0` without registry texture — see **`plan_handles_ssbo.md` D1**. |

### Checklist

- [x] **7-bisF.1** Push `projection` in quad / texture / YPQ push blocks (`sit/gpu/quad.vert`, `quad.frag`, `ypq_grade.frag`).
- [x] **7-bisF.2** `_SitVulkanFillQuadPushProjectionForActiveTarget()` — ortho from active render target / VD dimensions.
- [x] **7-bisF.3** Harness: `advanced.all_displays_windowed_fullscreen_cycle` VK visual parity with GL (manual + `[ OK ]`).
- [ ] **7-bisF.4** Optional harness pixel probe on VD pass (not visual-only) — defer until B-L6 lands.
- [ ] **7-bisF.5** **B-L6** — internal solid sampler outside user `texture_registry` (lifecycle; not projection).

**Exit criteria:** VD internal quad draws remain correct when main-window compositor UBO is updated later in the same frame’s command buffer; no reintroduction of UBO-only projection for internal 2D paths.

**Non-goals:**

- Rewriting harness to `SituationCamera` + `DrawMesh` for advanced VD test (library fix stays on `SituationCmdDrawQuad`).
- Bindless internal migration (Phase D0 gate unchanged).

---

## Phase 7-ter — OpenGL deferred executor foundation (v2.4.181–183)

**Purpose:** make the OpenGL **record → execute-at-EndFrame** model production-safe: correct render-pass tracking at execute time, deterministic raster/VAO/texture state across soft-buffer replay, and reliable pre-swap screenshot readback. **Prerequisite for RGL** and any further OpenGL graphics work.

**Status (v2.4.183):** **Complete (foundation slice).** Full OpenGL harness **428/428**; **`graphics`** **94/94** on reference GTX 1070.

### 7-terA — Execute-time render pass tracking (v2.4.181)

- [x] **`exec_inside_render_pass`** derived from **`SIT_OP_BEGIN_RENDER_PASS` / `SIT_OP_END_RENDER_PASS`** packet stream at execute (not **`recording_render_pass_active`**, which **`SituationCmdEndRenderPass`** clears at record time).
- [x] **`_SituationGLValidateInternalQuadDrawReady` / `TextDrawReady`:** `require_recorded_render_pass` — `true` on record, `false` on execute.
- [x] Fixes **`NO_RENDER_PASS_ACTIVE` (-540)** on internal quad/text/texture draws after EndRenderPass record.

### 7-terB — Execute hygiene: raster, indexed draw, polygon/depth (v2.4.183)

- [x] **`_SituationGLApplyBaselineRasterState()`** — front face CCW, polygon fill, polygon offset off, program point size off, stencil off, depth mask on, index defaults, global VAO.
- [x] Called from **`SituationAcquireFrameCommandBuffer`**, start/end of **`_SituationGLExecuteCommands`** (after shadow-cache reset at start).
- [x] **`SIT_OP_DRAW_INDEXED`** — bind global VAO before draw.
- [x] Texture bind execute — virtual bindless **`glProgramUniform1i`** only when user program active; else **`glBindTextureUnit`**; track **`current_bound_texture_id`**.
- [x] **`SIT_OP_DRAW_QUAD`** — rebind texture unit 0, refresh ortho from pass target size each batch.
- [x] Harness green: **`polygon_mode_line_wireframe`**, **`depth_bias_overlap`**, **`module_order_point_then_polygon`**.

### 7-terC — Textured quad readback + screenshot sync (v2.4.183)

- [x] **`glFinish()`** before pre-swap **`glReadPixels`** into **`screenshot_buffer`** (both EndFrame paths).
- [x] **`SituationLoadImageFromScreen` (OpenGL):** cached path **`memcpy`** + **one** vertical flip (no double-flip).
- [x] **`SituationCreateTextureEx`:** non-mipmap textures use **`GL_NEAREST`** (stable strict pixel readback).
- [x] Harness green: **`texture_format_preservation`**, **`screen_readback_corner_layout`**, **`texture_cpu_gpu_cpu_roundtrip`**, **`draw_textured_checkerboard`**.

### 7-terD — Validation

- [x] `build\sit_test.exe` → **428/428**
- [x] `build\sit_test.exe --module graphics` → **94/94**
- [x] **`doc/UPDATELOG.md`** v2.4.181, v2.4.183

**Exit criteria:** OpenGL full harness green with deferred execute; no module-order-only failures in polygon/depth-bias/textured-readback class; screenshot readback stable (not flaky).

**Non-goals (deferred):**

- [ ] RGL / bindless-first public API (starts after this foundation).
- [ ] OpenGL quad FS V-flip to match Vulkan (not required when readback + upload convention is consistent).
- [ ] Automatic **`SituationEndFrame`** GL error string surfacing for all **`-600`** paths.

**Related hardening (not bolster phase slots):** **v2.4.179–180** quad/text pre-draw validation — see **`doc/plan/INTERNAL_HARDENING_PLAN.md`** Phases 12–13.

---

## Phase 8 — Multi-viewport and scissor future-proofing

**Purpose:** add indexed viewport/scissor without forcing every user into arrays.

**Status (v2.4.184):** **Indexed slice complete.** Array API and multi-viewport example/tests remain open — batch commands ship when a real multi-viewport example lands (planned; indexed + loop is interim).

Current state: single viewport/scissor commands exist; indexed variants added.

- [x] Query backend support:
  - [x] OpenGL max viewports (`GL_MAX_VIEWPORTS`).
  - [x] Vulkan `maxViewports`.
- [x] Add `SituationGraphicsCaps.max_viewports`.
- [x] Add `SituationCmdSetViewportIndexed`.
- [x] Add `SituationCmdSetScissorIndexed`.
- [ ] Add optional `SituationCmdSetViewports` / `SituationCmdSetScissors` array API when multi-viewport examples need it (examples planned; indexed API is interim).
- [x] OpenGL: use `glViewportIndexedf` and `glScissorIndexed` when index > 0; index 0 can use existing path.
- [x] Vulkan: use `vkCmdSetViewport` / `vkCmdSetScissor` with first viewport/scissor index.
- [x] Define behavior when index exceeds backend limit (`SITUATION_ERROR_INVALID_PARAM`).
- [x] Add tests for index 0 wrapper parity (`core.viewport_index_zero_parity`).
- [x] Add backend capability test for max viewport count (`core.get_graphics_caps`, `core.viewport_index_out_of_range`).
- [ ] Defer multi-viewport rendering tests until a real multi-viewport example exists.

**Exit criteria:** API is future-ready without breaking simple single-viewport users. **Indexed slice met; array API + multi-viewport example pending.**

---

## Phase 9 — Push constant layout cleanup

**Purpose:** make push/root constants less raw and more consistent across GL/VK.

Current state: two forms exist (`contract_id` and shader/offset/data), and GL has an unimplemented packet for `SIT_OP_SET_PUSH_CONSTANT_DATA`.

- [ ] Audit all uses of `SituationCmdSetPushConstant`.
- [ ] Audit all uses of `SituationCmdSetPushConstantData`.
- [ ] Decide canonical form:
  - [ ] contract-id based for existing standard constants.
  - [ ] shader + named range for user constants.
  - [ ] raw offset only for advanced users.
- [ ] Add `SituationPushConstantRange` or `SituationPushConstantLayout` descriptor if needed.
- [ ] Implement OpenGL `SIT_OP_SET_PUSH_CONSTANT_DATA` by mapping to uniforms/UBO update according to shader metadata, or return `SITUATION_ERROR_UNSUPPORTED` until reflection exists.
- [ ] Vulkan: validate offset/size against pipeline layout push constant ranges.
- [ ] Add docs describing alignment/size limits.
- [ ] Add tests for push constant update affecting draw output.
- [ ] Add tests for invalid range/size errors.

**Exit criteria:** there is one recommended push-constant path, and the alternate path is either deprecated or fully implemented.

---

## Phase 10 — Profiling, stutter attribution, and GPU queries

**Purpose:** objective frame spike detection and cause attribution for **any** Situation consumer (examples, **QSR/NRC**, future **scaler sprite** batches, VD compositor) — plus optional GPU timing and user-facing query pools. **Not** “timestamp queries only.”

**Canonical detail:** **`doc/plan/AAA_ARCHITECTURE_PLAN.md` §6** (Tracy, GPU pools, safety principles). This section is the **bolster-plan execution view** with checkboxes and harness gates.

**Naming note:** Bolster **Phase 6** = raster state (closed v2.4.187). **AAA §6** = profiling (partially shipped same era). Do not conflate them.

---

### Why this phase matters (product context)

| Consumer | Profiling need |
|----------|----------------|
| **General apps / ex02** | Random micro-stutters invisible in avg FPS — backpressure, fence, present |
| **QSR / NRC** | Compute dispatch + readback cadence; shader compile hitches; frame-boundary semantics |
| **Phase G sprites** | Batch build vs draw vs compositor — CPU zones + optional GPU pass timing |
| **VD compositor** | Path A vs B cost; resume-pass overhead |
| **Harness CI** | Programmatic spike/phase asserts without Tracy |

---

### Layer model (ship in order; non-regressive)

| Layer | ID | What | Status |
|-------|-----|------|--------|
| **Lightweight CPU** | **P10.0** | Spikes + phase breakdown + overlay/histogram | ✅ **Shipped v2.4.187** |
| **Structured headless API** | **P10.1** | `SituationFrameProfile` / `GetFrameProfile` wrapping existing getters | ✅ v2.4.394 |
| **Tracy CPU zones** | **P10.2** | Opt-in `SITUATION_ENABLE_TRACY`, library + user zones | ✅ v2.4.395 |
| **Internal GPU timestamps** | **P10.3** | `SituationCmdGPUZoneBegin/End`, `gpu_zone_ns[]` in profile struct | ✅ v2.4.396 |
| **Public query pool API** | **P10.4** | User `SituationQueryPool` — occlusion + timestamps for custom pipelines | ✅ v2.4.397 |

**Safe minimum already shipped (P10.0):** spike detection + phase attribution **without** Tracy or GPU queries.

**Hard no-go rules (from AAA §6 — apply to all P10.1+ work):**

1. **Zero-cost when disabled** — compile-time or cheap runtime guard; no hot-path syscalls.
2. **Do not regress render-thread mitigations** (in-flight frames, VSync-aware backpressure, conditional screenshot).
3. **Extend** `metric_*` / `submit_timestamps` / `frame_time` — do not duplicate counters.
4. **Thread-safe** when render thread enabled (atomics or handoff).
5. **Harness gate** before enabling always-on GPU query recording in internal draws.

---

### P10.0 — Lightweight stutter diagnostics ✅ Shipped (v2.4.187)

Cross-ref **AAA §6.0** — all items complete.

- [x] `SituationGetMaxFrameTime`, `SituationGetFrameSpikeCount`, `frame_time_hist[]`
- [x] `SituationGetLastFramePhases` — backpressure, fence, execute, present (ns)
- [x] `SituationDrawMetricsOverlay` — FPS, max ms, spike count, phases
- [x] `SituationExportRenderHistogram` — JSON with histogram + spikes + phases
- [x] `SituationGetRenderLatencyStats`, `SituationGetRenderQueueDepth`, draw call counters
- [x] Instrumentation in acquire / render thread / present paths

**Remaining P10.0 follow-ups (not blockers for P10.1):**

- [ ] Harness: **`test_frame_spike_phases`** — synthetic slow present/backpressure path asserts spike count + dominant phase (programmatic, not overlay pixels)
- [ ] Document spike threshold constant + reset policy in `situation_api.md`
- [ ] **`draw_metrics_overlay`** harness: track remaining readback fragility separately (orientation/pixel path — not profiling logic)

**P10.0 exit criteria:** ✅ Met (AAA §6.0).

---

### P10.1 — Structured headless frame profile ✅ (v2.4.394)

Wrap P10.0 + thread pool + queue depth into one snapshot API for **QSR telemetry loops** and automated regression.

- [x] **`SituationFrameProfile`** struct (version field, size):
  - [x] `frame_time_ms`, `max_frame_time_ms`, `spike_count`
  - [x] Phase ns: backpressure, fence, execute, present
  - [x] `render_latency_avg_ns`, `render_latency_max_ns`, `queue_depth`
  - [x] Optional: `poll_ns`, `update_ns` (extend phase struct if already partially in overlay)
  - [x] `gpu_zone_ns[]` populated when P10.3 supported (zero when unsupported)
- [x] **`SituationGetFrameProfile(SituationFrameProfile* out)`** — memcpy snapshot; no allocation
- [x] **`SituationResetFrameProfileStats()`** — optional reset of spike counter / histogram (explicit opt-in)
- [x] Harness: **`test_get_frame_profile`** — matches individual getters (`--module frame_profile`)

**Defer:** rolling window percentiles (p95/p99) until proven need.

---

### P10.2 — Tracy CPU integration (opt-in) ✅ (v2.4.395)

Full checklist: **AAA §6.3–6.5**. Summary:

- [x] `SITUATION_ENABLE_TRACY` compile gate + `sit/situation_profiling.h` (via `situation.h`)
- [x] `SIT_PROFILE_ZONE` / `SIT_PROFILE_ZONE_SCOPED` / user-facing aliases in `situation_api.h`
- [x] Instrument (priority order): `SituationEndFrame`, `SituationAcquireFrameCommandBuffer`, render thread entry, submit, **`SituationPollShaderLoad`**, job workers, audio callback
- [x] `build_situation.bat opengl tracy` / `SIT_TRACY=1` documented in `doc/COMPILATION_GUIDE.md`
- [x] Tracy client vendored under `ext/tracy/public/` (upstream Tracy)

**QSR note:** zone NRC training iteration + readback wait separately from present (use `SIT_PROFILE_ZONE` in app code).

**Exit criteria:** Tracy capture shows main + render threads with frame marks; **no FPS regression** with Tracy **off** (default build).

---

### P10.3 — GPU timestamp zones (internal first) ✅ (v2.4.396)

Full checklist: **AAA §6.6–6.7**. Library-internal pass markers + user slots 4–15.

- [x] Capability probe: VK `timestampComputeAndGraphics`, GL `ARB_timer_query` / `GL_TIME_ELAPSED`
- [x] Internal `VkQueryPool` ring (`2 * FRAMES_IN_FLIGHT * MAX_ZONES`) — **not** swapchain-scoped
- [x] **`SituationCmdGPUZoneBegin/End(cmd, zone_id)`** — no-op / `-563` when unsupported
- [x] Readback one frame late (after fence); feeds **P10.1** `gpu_zone_ns[]` (Tracy GPU track still open)
- [x] Predefined zone IDs in `SituationGPUProfileZone`: composite, VD Path A/B (GL), text batch (GL), user 0..11
- [x] errno: `-563` GPU unsupported, `-564` zone overflow, `-565` zone state (VK in-pass user zones)

**Consumers:** QSR (compute vs graphics overlap), Phase G sprite batch draw, VD compositor tuning.

**Exit criteria:** harness reports non-zero GPU ns for known heavy pass ✅; disabled build = zero overhead ✅.

---

### P10.4 — Public query pool API (user occlusion + advanced) ✅ (v2.4.397)

- [x] Opaque **`SituationQueryPool`** handle
- [x] **`SituationQueryType`**: timestamp, occlusion (pipeline statistics deferred)
- [x] **`SituationCreateQueryPool` / `DestroyQueryPool`**
- [x] **`SituationCmdResetQueryPool`**, **`SituationCmdWriteTimestamp`**, **`SituationCmdBeginOcclusionQuery`**, **`SituationCmdEndOcclusionQuery`**
- [x] **`SituationGetQueryPoolResults`** — non-blocking → **`SITUATION_ERROR_QUERY_RESULT_NOT_READY` (-566)**; **`SITUATION_QUERY_RESULT_WAIT_BIT`**
- [x] GL: `glGenQueries`, `glQueryCounter`, `glBeginQuery` / `glEndQuery`, `glGetQueryObjectui64v`
- [x] VK: `VkQueryPool`, `vkCmdWriteTimestamp`, `vkGetQueryPoolResults`, timestamp period conversion
- [x] Harness: **`query_timestamp_monotonic`**, **`occlusion_query_visible_vs_clipped`**

**Exit criteria:** harness reports non-zero occlusion samples for visible draw and zero for empty query ✅; disabled/unsupported timestamp pools return `-563` ✅.

**Relationship to P10.3:** P10.3 = fixed internal zones for library diagnostics; P10.4 = arbitrary user pools. Implement P10.3 first to avoid two query-pool allocators.

---

### Phase 10 — Cross-plan dependencies

| Plan | Needs from Phase 10 |
|------|---------------------|
| **QSR** | P10.0 ✅ + P10.1 for training telemetry; P10.3 for compute/GPU attribution |
| **Scaler sprites (Phase G)** | P10.2 CPU batch build; P10.3 GPU draw pass |
| **AAA §7 timeline semaphores** | P10.0 phase timers help validate migration without Tracy |
| **VD bolster** | P10.3 zones for Path A/B A/B testing |

---

### Phase 10 — Suggested execution order

1. **P10.0 follow-up** — harness `test_frame_spike_phases` (small, high value)
2. **P10.1** — `SituationGetFrameProfile` (QSR-friendly, no GPU work) ✅ v2.4.394
3. **P10.2** — Tracy opt-in ✅ v2.4.395
4. **P10.3** — internal GPU zones ✅ v2.4.396
5. **P10.4** — public query pool ✅ v2.4.397

**Exit criteria (whole Phase 10 “done” for v2.4.x):** **P10.0 ✅ + P10.1 + P10.2 + P10.3 + P10.4 ✅ @ v2.4.397** (Phase 10 closed for v2.4.x).

---

## Phase 11 — Render-pass follow-up + Virtual Display bolster

**Status (v2.4.398):** **VD-0 foundation shipped (v2.4.188).** **VD-1 COMPLETE (v2.4.316).** **VD-4a / VD-2 / VD-3 / VD-5 COMPLETE (v2.4.387)** — v2.4.x VD bolster **exit criteria met** (see delivery slices below). **VD-4b Phase 0 ✅ v2.4.398** (MSAA types + internal wiring). **VD-4b attachments** remain **v2.5-gated**. **Next bolster targets:** Phase 9 (push constants), Phase 11-bis **MM-0** doc, Phase 12 docs.

**Primary code:** `sit/situation_impl_vd.h`, `SituationCmdBeginRenderPass` VD branch, `sit/situation_impl_renderer_lc.h` (GL execute clear + `GL_FRAMEBUFFER_SRGB`), `sit/situation_impl_color.h` (HDR clear conversion), `tests/harness/test_virtual_display.c`.

Current state: `SituationRenderPassInfo` supports load/store/clear for main window or VD. Compositing presentation is shipped. **Attachment defaults + inherit (VD-1).** **sRGB storage GL+VK, compositor gamma, inherit-clear + sRGB harness (VD-2 @ v2.4.387).** **Explicit composite sampler decoupled from scaling (VD-3).** **Aniso/mip LOD configure + create-time storage mips (VD-4a).** **Static update mode + memory hint storage (VD-5).** **Vulkan** VD pass clear still uses `_SituationColorRgbaToClearFloats(..., output_hdr_active)` when main-window HDR is active; **OpenGL** uses HDR clear helper on **main window only** (`display_id == -1`) — VD 8-bit attachments always use SDR clear floats. **Main-window HDR10** (swapchain, readback, caps) is **`10BIT_COLOR_OUTPUT_PLAN.md`** — orthogonal to VD-6 float attachments. General **`SituationRenderTarget`** → **VD-6** / v2.5.

**VD-0 — Render-pass foundation (shipped):** see delivery slice **VD-0** below (no duplicate checklist here).

### Virtual Display bolster — configuration taxonomy

**Reference only** (no checkboxes here). **Actionables:** delivery slices **VD-1 … VD-6** below. **Shipped** compositing fields are documented for completeness; new work targets gaps. *Note:* "Target slice" in this table is a **taxonomy classification** — actual implementation scope and deferral decisions live in the delivery-slice checklists below, which are authoritative.

| Category | Knob | Today | Target slice |
|----------|------|-------|--------------|
| **1. Rendering quality** | MSAA attachment quality | **`SituationMultisampleQuality`** types @ **v2.4.398**; create still **`msaa_samples = 1`** only (**`> 1` rejected**) | **VD-4b** attachments + configure API (**v2.5 default**; v2.4 optional — see scope gate) |
| | Anisotropic filtering level | **`SituationSetVirtualDisplayMaxAnisotropy`** — **VD-4a ✅ v2.4.387** | maintain |
| | Mipmap generation / mip filter mode | **`color_mip_levels`** at create + post-draw mipgen; **`SituationSetVirtualDisplayMipLevels`** — **VD-4a ✅ v2.4.387** | maintain |
| | Min / mag filter modes | **`SituationVirtualDisplaySamplerDesc`** + **`SituationSetVirtualDisplaySampler`** — **VD-3 ✅ v2.4.387** | maintain |
| **2. Attachment configuration** | Color format | UNORM default; **GL+VK: SRGB storage when `SIT_VD_FORMAT_RGBA8_SRGB`** — **VD-2 ✅ v2.4.387** | **VD-6** (float/HDR **attachment** formats) |
| | Depth buffer present | optional (`SIT_VD_DEPTH_NONE`) — **VD-1** | maintain |
| | Stencil buffer present | bundled in depth path only | **VD-1** (D24S8 in enum; impl deferred to follow-up patch) |
| | Depth/stencil format | device default / `GL_DEPTH_COMPONENT24` | **VD-1** (explicit mode enum; no exotic formats v1) |
| | Load/store per attachment | **`attachment_defaults` + per-pass override** — **VD-1** | maintain |
| **3. Color / HDR behavior** | Color space / encoding | **GL+VK:** UNORM or SRGB storage at create; **`GL_FRAMEBUFFER_SRGB`** on VD passes (GL); compositor samples stored encoding — **VD-2 ✅ v2.4.387**. **Main window:** HDR10 swapchain — **`10BIT_COLOR_OUTPUT_PLAN.md`** | **VD-6** (HDR **on VD texture**, metadata, tone-map hints) |
| | HDR metadata / tone-map hints | — | **VD-6** (v2.5+) |
| | Per-VD default clear color | **`attachment_defaults.clear` + `RenderPassInfoInherit`** + **`SituationSetVirtualDisplayClearColor`** — **VD-2 ✅ v2.4.387**; harness **`vd_default_clear_color`** | maintain |
| **4. Compositing / presentation** | Blend mode | **shipped** (`SituationBlendMode`) | maintain |
| | Opacity | **shipped** | maintain |
| | Z-order / layer | **shipped** | maintain |
| | Visibility flag | **shipped** | maintain |
| | Scaling mode (stretch/fit/integer) | **shipped** | maintain (layout only) |
| | Position / offset | **shipped** (`offset`) | maintain |
| **5. Performance / memory** | Mipmap levels to generate | **`SituationVirtualDisplayDesc.color_mip_levels`** (create-time) — **VD-4a ✅ v2.4.387** | maintain |
| | Memory usage hints (speed vs quality) | **`SituationSetVirtualDisplayMemoryHint`** (stored; best-effort at create) — **VD-5 ✅ v2.4.387** | maintain |
| | Update frequency (static vs dynamic) | **`SituationSetVirtualDisplayUpdateMode`** (`SIT_VD_UPDATE_STATIC` freezes frame clock) — **VD-5 ✅ v2.4.387** | maintain |
| **6. Advanced / future** | Multisample resolve mode | implicit end-pass | **VD-6** |
| | Sample shading / mask / alpha-to-coverage | **`SituationCmdSetMultisampleState`** plumbing (**v2.4.256**); no MSAA attachments yet | **VD-4b** completes layer 2; **VD-6** for advanced resolve modes |
| | Custom resolve / compositing shader | internal compositor only | **VD-6** |
| | Render-target usage flags (color/depth/storage/…) | fixed at create | **VD-6** / **`SituationRenderTarget`** (v2.5) |

**Own → operate → command** (see **§ Mutability & rebuild policy** for which layer applies):

| Layer | Responsibility |
|-------|----------------|
| **Own** | Fields on **`SituationVirtualDisplay`** / **`SituationVirtualDisplayDesc`** (taxonomy columns). |
| **Operate** | VD create/destroy, begin/end pass, resolve, mipgen, compositor sampler rebuild — backend reads VD storage. |
| **Configure** | **`SituationSetVirtualDisplay*`** / **`SituationConfigureVirtualDisplay`** — immediate owner update; rebuild per policy below. |
| **Pass override** | **`SituationRenderPassInfo`** on **`SituationCmdBeginRenderPass`** — per-pass only; does not mutate VD defaults. |

**Decision (v1): configure-only for tier B.** All VD quality and attachment-default mutations ship as **`SituationSetVirtualDisplay*`** (and existing **`SituationConfigureVirtualDisplay`** where overlap exists). **Do not** add **`SituationCmdSetVirtualDisplay*`** in v1 — recorded mirrors remain a documented future option if a harness/example requires same-buffer ordering with draws. Tier **A** is never command-recordable.

**Invariants:**

- Compositor and harness read **single-sample, resolved** VD color (never MSAA views).
- **`SituationRenderPassConfigurationKey`** retains its role for main-window pass caching. VD passes use **dynamic rendering** (`vkCmdBeginRendering`) and do not participate in the render-pass cache — format and depth/stencil presence are specified directly via `VkPipelineRenderingCreateInfo` on VD pipeline variants. **MSAA sample count** is in the VD pipeline variant key (**Phase 0 ✅ v2.4.398**); multisampled attachments + resolve are **VD-4b**.
- **`SituationCmdSetMultisampleState`**: layer-2 in-pass raster flags only; **not** a substitute for **`SituationMultisampleQuality`** (layer 1).
- Pixel-art preset: **`SITUATION_MULTISAMPLE_OFF`**, `aniso=1`, `max_mip_level=0`, `NEAREST` sampler, optional `SRGB` + integer scaling.

### Mutability & rebuild policy

Three **configuration tiers** — avoids mixing “VD personality” with per-pass mechanics.

| Tier | When | Examples | Changes GPU attachments? |
|------|------|----------|---------------------------|
| **A — Create-only** | **`SituationCreateVirtualDisplayEx`** (or destroy + recreate) | **`resolution`**, **`color_format`**, **`depth_stencil_mode`**, **`color_mip_levels`** (storage count) | Yes — full FBO/FB/pass allocation |
| **B — Configure-time** | **`SituationSetVirtualDisplay*`** / **`SituationConfigureVirtualDisplay`** | Attachment **defaults** (incl. default clear), **composite sampler**, **`msaa_quality`** (today: **`msaa_samples`**, must be 1), compositing (opacity, blend, offset, …), **`update_mode`**, **`memory_hint`** | Depends — see rebuild classes below |
| **C — Pass override** | **`SituationCmdBeginRenderPass`** + **`SituationRenderPassInfo`** | This pass’s load/store/clear — **does not write back** to VD defaults | No — selects/clears attachments for **one pass** only |

**Recorded VD owner updates (post-v1, not v1 scope):** a future **`SituationCmdSetVirtualDisplay*`** family could mirror tier **B** when an update must land in the command stream (e.g. MSAA toggle synchronized with a scripted cut). **v1 ships configure APIs only** for quality and attachment defaults. Add recorded cmds in a later slice when a concrete test proves ordering value. Do **not** duplicate tier **A** on the command buffer.

**Rebuild classes (tier B):**

| Class | Fields | When rebuild runs | Notes |
|-------|--------|-------------------|-------|
| **Light** | min/mag/mip filter, wrap, **`max_anisotropy`**, **`sampler.max_mip_level`** | Before next **`SituationRenderVirtualDisplays`** that samples this VD | Sampler + descriptor only |
| **Heavy** | **`msaa_quality`** (today **`msaa_samples`**), attachment default changes that alter pass compatibility | **Frame boundary** — after **`SituationEndFrame`**, before next **`BeginRenderPass`** into this VD | Set **`pending_gpu_rebuild`**; fail configure if VD is **inside an active pass** |
| **None** | Default clear color, load/store **defaults** (storage only) | Next **`BeginRenderPass`** merge | No FBO rebuild — affects inherit helper output only |

**Rules:**

1. **Never** rebuild FBO/render pass **mid-pass** on a VD that is the current render target.
2. **Heavy** changes queue to end-of-frame; compositor continues using **previous** attachments until the boundary (document one-frame delay).
3. **Create-only** fields require **`SituationDestroyVirtualDisplay`** + recreate (or explicit **`SituationRecreateVirtualDisplay`** later — not v1).
4. Pass overrides (**tier C**) never trigger rebuild; they only affect the current pass instance.

### Pass inherit semantics (tier C)

**Mental model:** create/configure sets **VD defaults**; begin-pass chooses **this frame’s** attachment behavior.

- **Do not** add **`SIT_LOAD_OP_INHERIT`** to the public load-op enum in v1 — avoids polluting every **`SituationRenderPassInfo`** call site.
- **Do** provide helpers that **read** VD defaults and fill a normal pass struct:

```c
/* All attachment ops/clears copied from VD storage — typical game-world VD each frame */
static inline SituationRenderPassInfo SituationRenderPassInfoInherit(int display_id);

/* Explicit override pattern: start from inherit, then patch one field */
SituationRenderPassInfo pass = SituationRenderPassInfoInherit(world_vd);
pass.color_attachment.loadOp = SIT_LOAD_OP_LOAD;  /* multi-pass: keep previous color */
SituationCmdBeginRenderPass(cmd, &pass);
```

- **`SituationRenderPassInfoDefault(display_id, clear)`** — sugar for “inherit but force this clear color **this pass only**” (does not change VD stored default unless caller also calls **`SituationSetVirtualDisplayClearColor`**).
- **`SituationRenderPassInfoLoad(display_id)`** — sugar for all **`SIT_LOAD_OP_LOAD`** (multi-pass composite inside VD).

Pass structs are always **explicit ops** at the API boundary; “inherit” is a **helper**, not a third load-op state.

### Prioritized delivery order (maintainer)

Agreed priority — **VD-1 first**. Quality work splits: **VD-4a** (sampler/mips/aniso) is comfortable in **v2.4.x**; **VD-4b** (MSAA + resolve) is **borderline** — default **v2.5** unless explicitly committed after a spike (see scope gate).

| Priority | Slice | v2.4.x | Rationale |
|----------|-------|--------|-----------|
| **High** | **VD-1** | **committed** | Multi-pass LOAD, optional depth, dynamic rendering — foundation |
| **Medium–High** | **VD-4a** | **✅ v2.4.387** | Aniso + mipgen — light rebuild, no MSAA resolve graph |
| **Medium** | **VD-2** | **✅ v2.4.387** | sRGB storage GL+VK, compositor gamma, inherit-clear + sRGB harness |
| **Medium** | **VD-3** | **✅ v2.4.387** | Explicit composite sampler; scaling layout-only |
| **Lower** | **VD-5** | **✅ v2.4.387** | Update mode / memory hints |
| **Gated** | **VD-4b** | **v2.5 default** | MSAA attachments + resolve + compositor invariant — see below |

**v2.4.x line after VD-1:** **VD-4a → VD-2 → VD-3 → VD-5**. Do **not** start VD-4b until maintainer sign-off.

### Scope gate — VD-4b MSAA (commit before implementation)

MSAA on composited VDs is deceptively heavy. It is **not** the same class of work as aniso or mip LOD (sampler-only). Treat **VD-4b** like **VD-6** unless a spike proves otherwise.

**What VD-4b adds beyond VD-1:**

| Area | Complexity |
|------|------------|
| Attachments | MSAA color + MSAA depth (or separate resolve target); 2× VRAM vs single-sample |
| Render passes | Per-(VD, samples, format, depth) **`VkRenderPass`** variants; GL MSAA FBO + resolve FBO |
| Resolve | Implicit end-pass resolve (VK subpass dependency or explicit blit); must complete before composite |
| Compositor invariant | Composite **always** samples resolved single-sample color — never MSAA view |
| Reconfigure | **`pending_gpu_rebuild`** at frame boundary; in-flight pass safety |
| Harness | Pixel proof that edges soften with 4× and composite/readback still matches single-sample path |
| Public API | **`SituationMultisampleQuality`** replaces **`int msaa_samples`**; fold **`SITUATION_FLAG_MSAA_4X_HINT`**; caps clamp at create/configure |
| **`SituationCmdSetMultisampleState`** | Layer 2 — complete end-to-end wiring when attachments are multisampled (VK pipeline bake from shadow; GL already dispatches) |

**Default plan (recommended):** defer **VD-4b** to **v2.5** alongside **`SituationRenderTarget`** MSAA policy. **v2.4.x ships VD-4a** (aniso, mips, explicit sampler) on top of VD-1 — real quality wins without resolve plumbing.

**Do not confuse this gate with VD-1 completion.** VD-1 is **COMPLETE (v2.4.316)** with its own checklist below. This section only governs whether **VD-4b MSAA** ships in **v2.4.x** instead of the default **v2.5** deferral.

#### Prerequisite — VD-1 foundation (required before *considering* MSAA)

1. [x] **VD-1 harness green on GL + VK** — includes `vd_load_color_attachment`, `vd_color_only_no_depth`, `vd_configure_attachment_defaults` plus existing compositing tests. **Vulkan VD render-into** uses **dynamic rendering** (`vkCmdBeginRendering`); **OpenGL** uses FBO (no dynamic rendering — expected).
   - Verified: **v2.4.316** shipped VD-1; **v2.4.387** `virtual_display` **40/40 GL**, **39/39 VK** (reference GTX 1070).

#### Commit gates — VD-4b MSAA in v2.4.x (all must pass; **open by default**)

Override the v2.5 deferral **only if all** of:

2. [ ] **Spike doc or PR notes** — resolve path chosen (VK subpass vs explicit resolve pass vs blit), GL MSAA FBO + resolve layout, **`pending_gpu_rebuild`** one-frame delay accepted, **`SituationMultisampleQuality`** migration from legacy **`msaa_samples`** / window hint. *(Not the shader-cache “resolve path” — that is pipeline variants, unrelated.)*
3. [ ] **Explicit maintainer OK** — MSAA is optional polish for current examples (pixel-art / tone synth), not blocking sub-osc or scope work.
4. [ ] **Time budget acknowledged** — multi-file GL+VK, not a single-sitting patch.

If **any commit gate (2–4) fails** → **VD-4b stays v2.5**; **v2.4 VD bolster closed @ v2.4.387** at **VD-4a + VD-2 + VD-3 + VD-5** (MSAA actionables remain gated).

### Virtual Display bolster — delivery slices (actionables)

Execute **VD-1 → VD-4a → VD-2 → VD-3 → VD-5** in **v2.4.x**; **VD-4b** only after scope-gate sign-off (else **v2.5**). **VD-6** and main-window RT track **v2.5**. One checklist per slice — no duplicate tasks elsewhere in this file.

#### VD-0 — Render-pass foundation (**shipped v2.4.188**)

- [x] **`SituationRenderPassInfo`** load/store/clear docs.
- [x] **`SituationRenderPassInfoDefault`** / **`Load`** helpers.
- [x] **`SituationRenderPassConfigurationKey`** (load/store + target class).
- [x] Harness **`core.render_pass_*`**.

#### VD-1 — Attachment configuration

**Pre-implementation decisions (resolved):**

| Question | Decision | Rationale |
|----------|----------|-----------|
| Classic `VkRenderPass` cache vs `VK_KHR_dynamic_rendering`? | **Dynamic rendering** (core in Vulkan 1.3+; we target 1.4) | Eliminates render-pass/framebuffer compatibility — no pass-cache key explosion, no pipeline compat constraints for depth-none VDs, dual-pass hack retired cleanly. |
| `SituationCreateVirtualDisplayEx` signature — break the shipped flat form? | **No.** Introduce **`SituationCreateVirtualDisplayFromDesc`** for the desc-struct form. Existing `SituationCreateVirtualDisplayEx` (7-param + flags) stays as-is — it becomes a convenience wrapper around `FromDesc` internally. Deprecation/removal is a Phase 13 decision. | Avoids breaking examples, k-term, Rust/Zig/Odin bindings, and `.def` exports. |
| Pipeline compatibility for depth-none VDs (Vulkan)? | **Per-shader VD pipeline variants** using `VkPipelineRenderingCreateInfo` (renderPass = VK_NULL_HANDLE). Main-window pipelines remain render-pass-based. `SituationCmdBindPipeline` / draw paths select the VD variant when the active pass target is a VD. | Pipelines bound to a `VkRenderPass` cannot be used inside `vkCmdBeginRendering`. Dual-variant approach limits blast radius to VD rendering only; main-window migration to dynamic rendering is a separate follow-up (not VD-1). |

**Implementation addendum:**

**Vulkan dynamic rendering migration (VD path only):**

- `SituationCmdBeginRenderPass` for VD targets: emit `vkCmdBeginRendering` (not `vkCmdBeginRenderPass`) with `VkRenderingInfo` populated from the resolved `SituationRenderPassInfo`.
- `SituationCmdEndRenderPass` for VD targets: emit `vkCmdEndRendering`.
- Remove `vd->vk.render_pass`, `vd->vk.render_pass_load`, and `vd->vk.framebuffer` from VD storage. Replace with per-frame `VkImageView` lookups (already stored: `vd->vk.image_view`, `vd->vk.depth_image_view`).
- **Main window** keeps classic `VkRenderPass` / swapchain framebuffers for now (swapchain acquire + present mechanics are tied to render-pass-based flow). Migration of main window to dynamic rendering is a separate follow-up (not VD-1 scope).
- Pipelines for VD rendering use **VD dynamic-rendering variants** (`renderPass = VK_NULL_HANDLE` + `VkPipelineRenderingCreateInfo`). Existing main-window pipelines remain render-pass-based. Both coexist — bind/draw paths select the appropriate variant based on whether the active pass targets a VD or the main window.

**API migration:**

```c
/* New desc-struct creation (VD-1) */
SITAPI SituationError SituationCreateVirtualDisplayFromDesc(
    const SituationVirtualDisplayDesc* desc, int* out_id);

/* Existing signatures — unchanged, become wrappers: */
SITAPI SituationError SituationCreateVirtualDisplay(...);    /* maps to desc with defaults */
SITAPI SituationError SituationCreateVirtualDisplayEx(...);  /* maps to desc + flags */
```

**Expanded checklist:**

- [x] Store **`SituationVirtualDisplayAttachmentDefaults`** + **`color_format`** + **`depth_stencil_mode`** on **`SituationVirtualDisplay`** (tier A/B fields).
- [x] Add **`SituationCreateVirtualDisplayFromDesc`** API (desc-struct form). Wire legacy `CreateVirtualDisplay` / `CreateVirtualDisplayEx` as wrappers.
- [x] **`SituationRenderPassInfoInherit(display_id)`** — copies VD attachment defaults into explicit pass struct. Validate: invalid `display_id` → `SITUATION_ERROR_INVALID_PARAM`; compute-target VD → valid (attachment defaults still meaningful for potential future render).
- [x] **`SituationSetVirtualDisplayAttachmentDefaults(id, defaults)`** — tier B configure API. Storage-only ("None" rebuild class — no FBO/pass rebuild). Reject if VD is inside active pass (`SITUATION_ERROR_RENDER_PASS_ACTIVE`).
- [x] **`SituationCmdBeginRenderPass`** VD path uses pass struct as-is (no hidden VD merge except via helpers caller chooses).
- [x] **OpenGL:** omit depth/stencil FB attachment when **`SIT_VD_DEPTH_NONE`**. Create color-only FBO at VD create time based on `depth_stencil_mode`. Begin-pass disables `GL_DEPTH_TEST` when VD has no depth.
- [x] **Vulkan — dynamic rendering path:**
  - [x] **Prerequisite:** Enable `dynamicRendering` in the Vulkan 1.3 features struct at device creation (lines 10331–10332).
  - [x] Replace `vkCmdBeginRenderPass` / `vkCmdEndRenderPass` with `vkCmdBeginRendering` / `vkCmdEndRendering` for VD targets (`_SitVkBeginVDDynamicRendering` / `_SitVkEndVDDynamicRendering`).
  - [x] Populate `VkRenderingAttachmentInfo` for color (always) and depth (only when `depth_stencil_mode != NONE`) from resolved pass info load/store/clear.
  - [x] **Layout transitions:** `_SitVkTransitionVDColorForRendering` before begin (→ `COLOR_ATTACHMENT_OPTIMAL`); `_SitVkEndVDDynamicRendering` after end (→ `SHADER_READ_ONLY_OPTIMAL`). Tracked in `vd->vk.color_image_layout`.
  - [x] Remove `vd->vk.render_pass`, `vd->vk.render_pass_load`, and `vd->vk.framebuffer` from VD storage.
  - [x] **Pipeline variant strategy:** Per-shader VD dynamic-rendering pipeline variant with `renderPass = VK_NULL_HANDLE` + `VkPipelineRenderingCreateInfo`. Selected at draw time when active pass targets a VD. Main-window pipelines remain render-pass-based.
  - [x] VD pipeline variant cache: `vk_vd_dynamic_pipelines[32]` on shader slot, keyed by format tuple. Destroyed on shader teardown/hot-reload.
- [x] **D24S8 scoping:** Enum present (`SIT_VD_DEPTH_D24S8 = 2`); `CreateFromDesc` rejects with `SITUATION_ERROR_NOT_IMPLEMENTED` until follow-up patch.
- [x] **In-pass guard:** Both `SituationSetVirtualDisplayAttachmentDefaults` and `SituationConfigureVirtualDisplay` reject with `SITUATION_ERROR_RENDER_PASS_ACTIVE`. Uses `_SitVDIsInsideActivePass` (checks GL recording buffers and VK `recording_pass_display_id`).
- [x] Harness:
  - [x] `vd_load_color_attachment` — multi-frame LOAD preserves content (renamed from `test_vd_load_op_preserves_content`).
  - [x] `vd_color_only_no_depth` — FromDesc with `SIT_VD_DEPTH_NONE`, render green + pixel assert.
  - [x] `vd_configure_attachment_defaults` — roundtrip through SetDefaults + Inherit, field-by-field assertion.
- [x] Update `doc/UPDATELOG.md` — shipped as v2.4.316.

**Status: COMPLETE (v2.4.316).**

#### VD-2 — Color / HDR behavior (first slice)

**Status: COMPLETE (v2.4.387).**

**Scope split (do not conflate):**

| Track | What | Plan |
|-------|------|------|
| **Main-window HDR10** | Swapchain `A2R10` + PQ, caps/features, readback | **`10BIT_COLOR_OUTPUT_PLAN.md`** ✅ shipped v2.4.301–303 |
| **VD pass clear under HDR** | Same sRGB→linear→PQ clear helper on **Vulkan VD dynamic rendering** | **VD-2** ✅ (rides main-window `output_hdr_active`) |
| **VD attachment sRGB** | Offscreen UNORM vs SRGB **storage** + correct composite | **VD-2** ✅ GL+VK + compositor gamma |
| **VD float/HDR textures** | HDR metadata, tone-map on composite | **VD-6** — still open |

**Checklist:**

- [x] **Stored default clear** — `SituationVirtualDisplayAttachmentDefaults.clear` on create/configure; **`SituationRenderPassInfoInherit`** copies into pass clear (tier B/C). Reject configure inside active pass (same as VD-1).
- [x] **Vulkan `SIT_VD_FORMAT_RGBA8_SRGB`** — `VK_FORMAT_R8G8B8A8_SRGB` image + view at VD create; texture registry `format_api` set accordingly.
- [x] **Vulkan HDR-aware VD pass clear** — `_SitVkBeginVDDynamicRendering` calls `_SituationColorRgbaToClearFloats(..., output_hdr_active)` (same helper as main-window pass; **`10BIT_COLOR_OUTPUT_PLAN.md`** Phase 6).
- [x] **`SIT_VD_FORMAT_RGBA8_SRGB` on OpenGL** — `GL_SRGB8_ALPHA8` FBO texture when `color_format == SRGB` (`_SitVDGlColorInternalFormat` in `SituationCreateVirtualDisplayFromDesc`).
- [x] **OpenGL begin-pass clear** — `_SituationColorRgbaToClearFloats` on GL execute path; **HDR PQ only when `display_id == -1`** (main window). VD 8-bit attachments use SDR clear floats (intentional — not PQ-encoded into sRGB8/UNORM8 FBOs).
- [x] **Compositor sampling / gamma** — **`GL_FRAMEBUFFER_SRGB`** enabled during VD render passes when attachment is SRGB; hardware decode on sample; **`vd.frag`** passes through RGB (no extra gamma in shader). Verified by **`vd_srgb_format_composite`** harness.
- [x] **`SituationSetVirtualDisplayClearColor`** — tier-B sugar (sets `attachment_defaults.clear.color` only).
- [x] Document **readback** encodes stored attachment values (linear vs sRGB vs screen readback after composite) — **`doc/guide/virtual_display.md`** § Color encoding & readback (v2.4.387 doc pass).
- [x] Harness **`vd_configure_attachment_defaults`** — inherit copies clear color + depth/stencil defaults.
- [x] Harness **`vd_default_clear_color`** — inherit clear, no draw, composite readback.
- [x] Harness **`vd_srgb_format_composite`** — SRGB vs UNORM VD with linear shader output; composite brightness within tolerance.
- [x] Update `doc/whatsnew.md` — v2.4.387.
- [x] Update `doc/UPDATELOG.md` + index (`updatelog_24_04.md` append) — v2.4.387; harness **40/40 GL**, **39/39 VK** verified 2026-06-29.

**Cross-ref:** Main-window HDR does **not** close VD-6. VD compositor still samples UNORM/SRGB8 attachments into HDR swapchain without a dedicated YPQ composite path.

#### VD-3 — Explicit composite sampler

**Status: COMPLETE (v2.4.387).**

- [x] Decouple **`composite_sampler`** from **`scaling_mode`** — **`SituationSetVirtualDisplayScalingMode`** is layout-only (no filter side effects).
- [x] **`SituationVirtualDisplaySamplerDesc`** on **`SituationVirtualDisplay`** / **`SituationVirtualDisplayDesc`**; default via **`SituationVirtualDisplaySamplerDescDefault()`**.
- [x] **`SituationSetVirtualDisplaySampler`** — light rebuild (sampler + descriptor before next composite).
- [x] Harness **`vd_sampler_nearest_upscale`** — NEAREST + stretch layout.
- [ ] Harness **`vd_sampler_repeat_wrap`** — **deferred** (repeat wrap not required for v2.4.x closure; API supports `SIT_TEXTURE_WRAP_REPEAT`).
- [x] Update `doc/whatsnew.md` — v2.4.387.

Compositing presentation (**blend, opacity, z-order, visibility, scaling, offset**) — **no new actionables** (already shipped + **`test_virtual_display.c`**).

#### VD-4a — Rendering quality (sampler / mips — v2.4.x committed)

**Status: COMPLETE (v2.4.387).**

- [x] **`SituationSetVirtualDisplayMaxAnisotropy`** / **`SituationSetVirtualDisplayMipLevels`** — configure APIs only (**v1: no recorded mirror**).
- [x] **`max_anisotropy`** on composite sampler (**light** rebuild); clamp via **`_SitVDMaxAnisotropyLimit()`** (device max anisotropy / `SIT_FEATURE_SAMPLER_ANISOTROPY`).
- [x] **`color_mip_levels`** on **`SituationVirtualDisplayDesc`** (create-time storage) + post-draw mipgen (GL **`glGenerateTextureMipmap`**; VK **`_SituationVulkanGenerateMipmaps`** in **`_SitVkEndVDDynamicRendering`**); **`sampler.max_mip_level`** / **`mip_filter`** on composite sampler.
- [x] Reject sampler/mip configure while VD is inside active pass (`SITUATION_ERROR_RENDER_PASS_ACTIVE`).
- [x] **`SituationSetVirtualDisplayMipLevels`**: runtime **`color_mip_levels`** change returns **`NOT_IMPLEMENTED`** (create-time only); **`sampler_max_mip_level`** updates apply immediately.
- [x] Harness **`vd_aniso_sampler_configure`** — configure API smoke (aniso + mip LOD clamp).
- [ ] Harness: aniso/mip **visible** on scaled composite where measurable — **deferred** (optional visual follow-up; not required for v2.4.x exit).
- [x] Update `doc/whatsnew.md` — v2.4.387.

#### VD-4b Phase 0 — MSAA prep (**shipped v2.4.398**)

Scaffolding only — **no MSAA attachments, no resolve, `> 1` still rejected**. Unblocks VD-4b without scope-gate override.

- [x] **`SituationMultisampleQuality`** + **`SampleCount` / `FromSampleCount` / `Clamp`** helpers in **`situation_api_types_gpu.h`**.
- [x] Store **`msaa_quality`** + **`pending_gpu_rebuild`** on **`SituationVirtualDisplay`** (from **`desc.msaa_samples`** at create).
- [x] **`_SituationVulkanCreateImage(..., VkSampleCountFlagBits samples)`** — all call sites pass **`VK_SAMPLE_COUNT_1_BIT`** until attachments land.
- [x] VD pipeline variant key includes **sample count**; **`_SitVkCreateVDDynamicPipelineFromModules`** / **`CreateGraphicsPipelineEx`** take **`rasterization_samples`** (bakes **`dynamic_ms_*`** when samples > 1).
- [x] Harness **`multisample_quality_helpers`**.
- [ ] **`pending_gpu_rebuild` end-frame hook** — deferred to VD-4b configure API.
- [ ] Desc **`msaa_quality`** field (replace **`int msaa_samples`**) — deferred to VD-4b public slice.

#### VD-4b — MSAA + resolve (**v2.5 default — v2.4 only if scope gate passes**)

Unified **layer 1** (attachment quality) + **layer 2** completion (in-pass flags). See **§ MSAA / multisample**. **Phase 0 prep ✅ v2.4.398.**

- [x] Add **`SituationMultisampleQuality`** + sample-count / caps-clamp helpers in **`situation_api_types_gpu.h`** *(Phase 0)*.
- [ ] Migrate VD desc **`int msaa_samples`** → **`SituationMultisampleQuality msaa_quality`**; same for **`SituationRenderTargetDesc`** (coordinate with **`v2.5-api-expansion.md` Phase 6-MSAA).
- [ ] **`SituationSetVirtualDisplayMultisampleQuality`** — tier B, **heavy** rebuild (replace planned **`SituationSetVirtualDisplayMsaaSamples`**).
- [ ] MSAA color+depth during pass; **resolve → single-sample** before composite.
- [ ] Add **MSAA sample count** to VD pipeline variant key (via `VkPipelineRenderingCreateInfo` / GL FBO sample count). *(VK key + pipeline param: Phase 0 ✅; GL FBO: VD-4b.)*
- [ ] Wire **`SituationCmdSetMultisampleState`** end-to-end (VK: bake shadow into pipeline variants; GL: meaningful on MSAA FBOs).
- [ ] Fold **`SITUATION_FLAG_MSAA_4X_HINT`** into enum at init (main-window track — may land with VD-6 / swapchain MSAA).
- [ ] Harness: **`vd_msaa_resolve_composite`**.

*Do not check these off in v2.4.x unless scope gate items are all satisfied.*

#### VD-5 — Performance / memory

**Status: COMPLETE (v2.4.387).**

- [x] **`SituationVirtualDisplayUpdateMode`** + **`SituationSetVirtualDisplayUpdateMode`** — **`SIT_VD_UPDATE_STATIC`** sets **`frame_time_multiplier = 0`** and skips frame-clock advance in **`SituationUpdateTimers`**; dynamic restores multiplier when re-enabled.
- [x] **`SituationVirtualDisplayMemoryHint`** + **`SituationSetVirtualDisplayMemoryHint`** — stored on VD; **`SituationCreateVirtualDisplayFromDesc`** passes hint to VMA allocation (best-effort; same GPU-only path today).
- [x] **`SituationConfigureVirtualDisplay`**: **`frame_time_mult == 0`** selects static mode; **`> 0`** selects dynamic.
- [x] Harness **`vd_update_mode_static`** — frame clock frozen under static mode.
- [x] Document interaction with **`SituationSetVirtualDisplayDirty`** and **`frame_time_mult`** in user-facing guide — **`doc/guide/virtual_display.md`** § Update mode (v2.4.387 doc pass).
- [x] Update `doc/whatsnew.md` — v2.4.387.

#### VD-6 — Advanced / future (v2.5 — planning only, no v2.4 actionables)

- [ ] Multisample **resolve mode** (single-sample average vs custom).
- [ ] **HDR** formats + metadata / tone-map hints on composite.
- [ ] **Custom compositing** / resolve shader hook (replace internal VD quad path).
- [ ] **Usage flags** on VD color/depth (sampled, storage, transfer) — or defer to **`SituationRenderTarget`**.
- [ ] **`SituationRenderTarget`** for non-composited offscreen + resolve/readback helpers.

#### VD bolster — implementation order

**v2.4.x (committed):**

1. **VD-1** — attachment + dynamic rendering (VD path).
2. **VD-4a** — aniso + mips (no MSAA).
3. **VD-2** — sRGB + stored clear.
4. **VD-3** — explicit composite sampler.
5. **VD-5** — update/memory hints.

**v2.5 (default) or v2.4 only after scope gate:**

6. **VD-4b** — MSAA + resolve.
7. **VD-6** / main-window RT.

#### VD bolster — test matrix (`test_virtual_display.c`)

| Test | Slice | Status @ v2.4.387 |
|------|-------|-------------------|
| `vd_default_clear_color` | VD-2 | ✅ |
| `vd_load_color_attachment` | VD-1 | ✅ |
| `vd_color_only_no_depth` | VD-1 | ✅ |
| `vd_configure_attachment_defaults` | VD-1 | ✅ |
| `vd_srgb_format_composite` | VD-2 | ✅ |
| `vd_sampler_nearest_upscale` | VD-3 | ✅ |
| `vd_aniso_sampler_configure` | VD-4a | ✅ |
| `vd_update_mode_static` | VD-5 | ✅ |
| `vd_msaa_resolve_composite` | VD-4b | 🔲 v2.5 / gated |
| *(compositing/scaling/blend/z-order + idle tests)* | — | ✅ (31 legacy + 5 new bolster = **40 GL** / **39 VK**) |

#### VD bolster — non-goals (v2.4)

- Stencil-only VD without color.
- Main-window swapchain sRGB/HDR policy — **`10BIT_COLOR_OUTPUT_PLAN.md`** (shipped for HDR10 path; not VD attachment HDR).
- **`SituationRenderTarget`** without compositor stack (**VD-6** / v2.5).
- **VD-4b MSAA** — unless scope gate passes (default: v2.5).

**Exit criteria (Phase 11 VD bolster — v2.4.x):** **VD-1, VD-4a, VD-2, VD-3, VD-5** actionables complete ✅ **@ v2.4.387**; harness covers those slices (**`virtual_display` 40/40 GL, 39/39 VK**); legacy **`SituationCreateVirtualDisplay`** preserved via wrapper. **VD-4b not required** to close v2.4.x VD work. **Optional follow-ups (non-blocking):** **`vd_sampler_repeat_wrap`**, visual aniso/mip harness.

### Main window + generic render target (Phase 11 remainder)

Separate from VD compositor stack:

- [ ] Main window dynamic render-pass cache wired to **`SituationCmdBeginRenderPass`** (today: fixed **`main_window_render_pass`**).
- [ ] Coordinate non-VD **`SituationRenderTarget`** with copy/blit/readback (**VD-6** / v2.5).

**Exit criteria (Phase 11 overall):** render-pass foundation shipped ✅; **VD bolster v2.4.x exit criteria met @ v2.4.387** ✅; main-window dynamic RT + **VD-6** remain v2.5.

---

## Phase 11-bis — Multi-monitor Virtual Display presentation (WDM + compositor)

**Status:** **design / consent gate only** — **do not implement** until maintainer explicitly approves a slice below.  
**Purpose:** document the **only** convention-compliant way to show **independent content on every monitor at once** with Situation today, and the **v2.5** path for true per-monitor OS windows without breaking the command-buffer model.  
**Motivation:** harness **`advanced.all_displays_windowed_fullscreen_cycle`** and future multi-monitor examples must not reintroduce rogue GLFW side windows, parallel **`BeginFrame`/`EndFrame`** APIs, or OpenGL-on-Vulkan bypass paths.

### What users want (reference scenario)

One integration test / example, three phases (e.g. 2 s each):

| Phase | Expected behavior |
|-------|-------------------|
| **1 — Windowed panels** | Every connected monitor shows a **1024×768** panel with **independent** animated content, **all visible simultaneously**. |
| **2 — Fullscreen** | Each panel expands to **fill its monitor** (still simultaneous). |
| **3 — Restore** | Back to **1024×768 windowed** panels on every monitor. |

Timing, harness window default (**`SIT_TEST_WINDOW_WIDTH` / `HEIGHT`**), and teardown restoring primary monitor window mode must stay aligned with **`tests/harness/sit_test_window.h`**.

### Architectural facts (non-negotiable)

| Fact | Implication |
|------|-------------|
| Situation owns **one** primary GLFW window (`sit_gs.sit_glfw_window`) and **one** main swapchain presentation path per frame. | Simultaneous output uses **one** composited framebuffer presented once per **`SituationEndFrame`**. |
| Virtual Displays are **offscreen render targets** composited by **`SituationRenderVirtualDisplays`** inside a **main-window** render pass (`display_id == -1`). | Per-monitor content = **one VD per monitor**, each rendered via **`SituationCmdBeginRenderPass(cmd, &rp)`** with **`rp.display_id = vd_id`**. |
| Window placement and fullscreen are **WDM** (`SituationSetWindowSize`, **`SituationSetWindowPosition`**, **`SituationSetWindowState`**, **`SituationSetDisplayMode`**, monitor queries). | Phase transitions change **host window geometry**, not the frame-loop shape. |
| Command-buffer first; no hidden second renderer. | **Forbidden:** auxiliary present loops, shared-context monitor windows, **`SituationMonitorWindow*`**-style APIs, test-only GL draws outside **`SituationCmd*`** / **`SituationEndFrame`**. |

### Tier A — Spanning host + multi-VD (v2.4.x, **in scope** for bolster)

**Model:** treat the **virtual desktop** as one host surface; map each physical monitor to a VD panel inside it.

```mermaid
flowchart TB
    subgraph frame ["One frame — existing API only"]
        A["SituationAcquireFrameCommandBuffer()"]
        B["For each monitor VD: BeginRenderPass(display_id=vd) → Draw → EndRenderPass"]
        C["BeginRenderPass(display_id=-1)"]
        D["SituationRenderVirtualDisplays(cmd)"]
        E["EndRenderPass → SituationEndFrame()"]
        A --> B --> C --> D --> E
    end
    subgraph wdm ["WDM — between phases, not per frame"]
        W1["Host spans virtual desktop bounds"]
        W2["Undecorated + full desktop for phase 2"]
        W3["Restore decorated + panel layout phase 3"]
    end
    E --> Present["Single OS window / swapchain present"]
    wdm -.-> Host["Primary GLFW window geometry"]
    Host --> Present
```

**Per-frame sequence (unchanged from any other VD app):**

1. **`SituationPollInputEvents`** / **`SituationUpdateTimers`**
2. **`SituationAcquireFrameCommandBuffer`**
3. For each monitor panel: **`SituationCmdBeginRenderPass`** → draw (e.g. **`SituationCmdDrawQuad`**) → **`SituationCmdEndRenderPass`**
4. Main pass: **`display_id = -1`**, clear → **`SituationRenderVirtualDisplays`** → **`SituationCmdEndRenderPass`**
5. **`SituationEndFrame`**

**Between phases (WDM only):**

| Phase | Host window (WDM) | VD layout (configure) |
|-------|-------------------|------------------------|
| 1 | Decorated; size = virtual desktop bounds; position = desktop origin | One VD per monitor; **1024×768** internal resolution; **`offset`** centers panel in monitor rect (relative to host client origin) |
| 2 | **Undecorated**; host covers full virtual desktop | Recreate or reconfigure VDs to **monitor width×height**; offset = monitor origin relative to host |
| 3 | Same as phase 1 | Same as phase 1 |

**Important honesty boundary:** Tier A is **one OS window** spanning monitors (or the virtual desktop). It is **not** N separate taskbar entries. It **does** satisfy simultaneous independent VD content and phased fullscreen *layout* when phase 2 fills each monitor region. Document this in examples so users are not surprised.

**Current harness reference:** `tests/harness/test_advanced.c` — implements Tier A today. Keep it; refine only through approved slices below.

### Tier B — True per-monitor OS windows (v2.5+, **out of scope** for v2.4 bolster)

**User-visible goal:** N monitors → N native windows, each with its own client area and optional per-monitor fullscreen — still **one command buffer**, **one `SituationEndFrame`**.

**Parking lot item:** **`doc/plan/v2.5-api-expansion.md` § P — Multi-window command-buffer association**.

**Design constraints (preview only — no code until v2.5 spike + consent):**

| Layer | Responsibility |
|-------|----------------|
| **WDM** | Create/destroy **secondary present surfaces** bound to **`situation_monitor_id`**; **`SituationSetDisplayMode`**-style fullscreen **per surface**; no raw GLFW in public API. |
| **Renderer** | Register secondary swapchains/surfaces; **`SituationEndFrame`** presents **all** registered surfaces that received a main or dedicated pass this frame. |
| **VD / RT** | Optional: bind a VD to a **present surface id** for compositing target, **or** render pass **`display_id`** maps to surface — **one** model chosen at spike time; do not duplicate Tier A offset math and Tier B surface binding without migration story. |
| **Command buffer** | **No** parallel lifecycle. Same **`SituationCmdBeginRenderPass`**, **`SituationRenderVirtualDisplays`**, **`SituationEndFrame`**. |

**Spike deliverables before any Tier B code:**

1. [ ] Present model: single shared swapchain image vs N swapchains vs hybrid.
2. [ ] Vulkan: N **`VkSurfaceKHR`** / swapchains; GL: shared context vs per-window (document choice).
3. [ ] Input/focus routing when multiple visible windows.
4. [ ] Harness strategy: **`advanced`** Tier B variant or separate **`window.multi_present_*`** tests.
5. [ ] **Explicit maintainer sign-off.**

### Anti-patterns — **banned** (learned from v2.4.192 revert)

Do **not** repeat these; they break conventions and were reverted:

| Anti-pattern | Why it is wrong |
|--------------|-----------------|
| New **`SituationMonitorWindowBeginFrame` / `DrawQuad` / `EndFrame`** | Bypasses command buffer, render-pass cache, render thread, and backend parity. |
| Hidden OpenGL windows on Vulkan builds for “monitor output” | Second presentation stack; not backend-neutral; untestable against main renderer. |
| Aux GLFW + GLAD test harness outside **`SituationCmd*`** | Same as above; couples tests to implementation detail. |
| Cycling **`SituationSetDisplayMode`** on the single host window | Only one monitor visible at a time — fails the simultaneous requirement. |
| Implementing Tier B APIs under time pressure without a plan section | Exactly what Phase 11-bis exists to prevent. |

### Proposed v2.4.x delivery slices (Tier A hardening)

Execute **only after maintainer picks a slice**. Default: **documentation + harness stability first**, optional thin WDM helpers second.

#### MM-0 — Document & stabilize (no new public API)

- [ ] Add **`doc/MULTI_MONITOR_VD.md`** (or SDK §3.6 appendix): Tier A diagram, phase table, honesty boundary (one OS window).
- [ ] Align **`test_advanced.c`** comments with this plan; no behavioral change unless fixing bugs.
- [ ] Cross-link from **`doc/situation_sdk.md` §3.6** Virtual Display compositor.
- [ ] **`doc/UPDATELOG.md`** entry when doc lands.

**Exit criteria:** a maintainer can implement multi-monitor VD without reading harness internals or re-discovering banned patterns.

#### MM-1 — WDM query helpers (optional convenience)

**Maintainer note:** small, low-risk win — pure WDM geometry, no renderer/frame-loop changes. **Not blocking**; ship only after **MM-0** doc if Tier A multi-monitor examples need public helpers.

Thin wrappers in **`situation_impl_wdm.h`** — **no renderer changes**:

```c
typedef struct SituationVirtualDesktopBounds {
    int x, y, width, height;
} SituationVirtualDesktopBounds;

SITAPI SituationError SituationGetVirtualDesktopBounds(SituationVirtualDesktopBounds* out_bounds);
SITAPI SituationError SituationSetWindowVirtualDesktopSpan(bool undecorated);
```

- [ ] **`SituationGetVirtualDesktopBounds`** — union of **`SituationGetMonitor*`** / **`SituationGetDisplays`** (same math as harness **`advanced_desktop_bounds`**).
- [ ] **`SituationSetWindowVirtualDesktopSpan`** — applies size/position + optional **`SITUATION_FLAG_WINDOW_UNDECORATED`**; does **not** create VDs.
- [ ] Harness refactor: **`test_advanced.c`** uses helpers if shipped; otherwise keep local math.
- [ ] Tests in **`test_window.c`**: bounds on 1- and 2-monitor setups (no pixel animation).

**Exit criteria:** WDM helpers are pure geometry; frame loop unchanged.

#### MM-2 — VD placement helper (optional)

Reduce duplicated offset math; stays **configure-time**, tier B mutate policy from Phase 11:

```c
typedef enum SituationMonitorPanelMode {
    SIT_MONITOR_PANEL_WINDOWED = 0,  /* centered panel_w x panel_h on monitor */
    SIT_MONITOR_PANEL_FULLSCREEN = 1 /* panel = monitor pixel rect */
} SituationMonitorPanelMode;

SITAPI SituationError SituationConfigureVirtualDisplayForMonitor(
    int display_id,
    int situation_monitor_id,
    int panel_width,
    int panel_height,
    SituationMonitorPanelMode mode,
    const SituationVirtualDesktopBounds* host_span);
```

- [ ] Computes **`offset`** + internal **`resolution`** for **`SituationConfigureVirtualDisplay`** / recreate policy.
- [ ] Does **not** auto-create VDs; caller still **`SituationCreateVirtualDisplay`**.
- [ ] Harness **`advanced`** uses helper when present.

**Exit criteria:** one call replaces manual offset math; compositor and command buffer untouched.

#### MM-3 — Advanced harness phases (validation)

- [ ] **`all_displays_windowed_fullscreen_cycle`**: assert phase 1 panel sizes, phase 2 fullscreen layout, phase 3 restore (existing test + clearer stderr diagnostics).
- [ ] Optional readback spot-check per VD id (if stable on CI) — **black-box**, via existing readback APIs only.
- [ ] Document CI risk: spanning window may be intrusive; keep **`requires_context = true`**; no headless claim.

**Exit criteria:** OpenGL + Vulkan harness green; test documents Tier A limitation in stderr banner.

### Consent gate (mandatory)

**No agent or contributor may implement Tier A slices MM-1+ or any Tier B code unless:**

1. [ ] This **Phase 11-bis** section is reviewed.
2. [ ] Maintainer names the approved slice (**MM-0**, **MM-1**, **MM-2**, **MM-3**, or Tier B spike).
3. [ ] Implementation stays in listed primary files; **no** new `situation_impl_mwin.h`-style modules without plan update.
4. [ ] Full OpenGL + Vulkan harness run recorded in **`doc/UPDATELOG.md`**.

### Relationship to Phase 11 VD bolster

| Phase 11 VD slice | Multi-monitor interaction |
|-------------------|---------------------------|
| **VD-1 … VD-5** | Independent — attachment/sampler/clear quality applies per monitor VD like any VD. |
| **VD-4b MSAA** | Each monitor VD may set **`SituationMultisampleQuality`**; compositor still samples resolved single-sample color. |
| **Pass helpers** | **`SituationRenderPassInfoInherit(vd_id)`** per monitor pass; same frame loop. |
| **Tier B multi-present** | **VD-6** / v2.5 — may supersede offset-based Tier A for apps that need N OS windows. |

### Phase 11-bis — non-goals

- [ ] N separate OS windows in v2.4.x (Tier B only).
- [ ] Parallel frame APIs or second **`SituationEndFrame`**.
- [ ] Virtual Display changes that bypass **`SituationRenderVirtualDisplays`** for multi-monitor.
- [ ] **`SituationSetDisplayMode`** loop as a “multi-monitor” strategy.
- [ ] Implementation without consent gate checklist above.

**Exit criteria (Phase 11-bis v2.4.x):** **MM-0** doc shipped; harness stable; optional **MM-1/MM-2** only if approved; Tier B remains design-only in v2.5 parking lot.

---

## Phase 12 — Documentation, examples, and migration

**Purpose:** make the bolstered command stack usable without reading renderer internals.

- [x] Add `doc/RENDERER_COMMAND_STACK.md` or extend existing API docs with a command stack chapter — **`doc/RENDERER_COMMAND_STACK.md`** shipped @ v2.4.387; per-command detail remains in **`situation_command_reference.md`**.
- [ ] Add a README subsection listing the command categories:
  - [ ] render pass
  - [ ] state
  - [ ] binding
  - [ ] draw
  - [ ] compute
  - [ ] transfer
  - [ ] synchronization
  - [ ] queries
- [ ] Add cookbook snippets:
  - [ ] clear + draw triangle
  - [ ] compute writes texture + barrier + draw texture
  - [ ] copy texture to readback buffer
  - [ ] indirect draw from CPU-filled command buffer
  - [ ] timestamp scope
  - [ ] **multi-monitor Tier A:** spanning host + one VD per monitor + **`SituationRenderVirtualDisplays`** (see **Phase 11-bis** / **`doc/MULTI_MONITOR_VD.md`** when MM-0 lands)
- [ ] Mark deprecated wrappers in docs, not just attributes:
  - [ ] `SituationMemoryBarrier`
  - [ ] `SituationCmdBeginRenderToDisplay`
  - [ ] `SituationCmdEndRender`
  - [ ] old `SituationCmdBindIndexBuffer` once `Ex` ships
- [ ] Update `doc/UPDATELOG.md` for each shipped phase.
- [ ] Update `doc/whatsnew.md` when a group of phases lands.

**Exit criteria:** users can find the command they need and understand when to use old wrappers versus new explicit APIs.

---

## Phase 13 — Compatibility wrapper review

**Purpose:** review old `void` wrappers only after the bolstered `SituationError`-returning APIs have shipped and real examples/tests prove the replacement paths.

Current policy: wrappers are kept through Phases 1-12. They are not promised forever, but removal/deprecation should not be mixed into feature phases.

- [ ] Inventory wrappers whose `Ex` or replacement APIs shipped:
  - [ ] `SituationCmdDispatch`
  - [ ] `SituationCmdPipelineBarrier`
  - [ ] `SituationCmdCopyBuffer`
  - [ ] `SituationCmdBindIndexBuffer`
  - [ ] any other compatibility helpers introduced during this plan.
- [ ] For each wrapper, decide one of:
  - [ ] keep as stable convenience API.
  - [ ] document as compatibility API with no deprecation warning.
  - [ ] mark deprecated in docs and headers for a later removal window.
- [ ] Confirm examples and docs prefer the `SituationError`-returning path when the caller can act on failures.
- [ ] Confirm wrappers call the new implementation internally and do not duplicate backend behavior.
- [ ] Add migration notes for any wrapper marked deprecated.

**Exit criteria:** compatibility wrappers have an explicit post-plan policy without blocking v2.4 renderer hardening.

---

## Phase 14 — Renderer behavior policy commands

**Status:** **COMPLETE @ v2.4.391** — see **[PHASE_14_RENDERER_BEHAVIOR_POLICY.md](PHASE_14_RENDERER_BEHAVIOR_POLICY.md)**. Phase 14 command-stack slices 1–4 shipped; companion **Phase 3b** readback harness shipped @ **v2.4.392**.  
**Why now:** closes deferred “strict vs assisted” decisions from Phases 3–4 (texture barriers, blit layout, filter fallback, validation level) with **explicit command-stack state** instead of hidden backend fallbacks. Cross-ref **`doc/RENDERER_COMMAND_STACK.md`** and Phase 3 cookbook strict-default notes.

**Purpose:** expose opt-in behavior settings as command-buffer state, so advanced users can tune strictness, fallbacks, and convenience behavior without weakening default backend parity.

Current policy: new renderer commands should be strict and explicit by default. Convenience behavior can be powerful, but it must be visible in the command stream and testable instead of hidden behind backend-specific fallback.

### Design summary (canonical detail in linked doc)

- **`SituationRendererBehaviorPolicy`** — one struct, explicit enum per axis (not bitflags): transfer usage, texture layout, blit filter, coordinate (reserved strict), validation level.
- **`SituationRendererBehaviorPolicyDefault()`** + **`SituationCmdSet/Push/PopRendererBehavior`** — stack semantics mirror **`PushRasterState`**; reset at frame begin; max depth **32**.
- **Strict default unchanged** when no policy commands are recorded; VD/internal compositor paths **exempt**.
- **Delivery slices:** (1) infrastructure + blit filter downgrade, (2) transfer usage fallback, (3) layout assisted + 3b readback ergonomics, (4) validation WARN/COMPAT + user docs.

### Implementation checklist

- [x] Define `SituationRendererBehaviorPolicy` and axis enums (**design frozen in linked doc**).
- [x] Add command-stack APIs: `SituationCmdSetRendererBehavior`, `SituationCmdPushRendererBehavior`, `SituationCmdPopRendererBehavior`.
- [x] GL soft-buffer opcodes + executor; VK CPU-side stack on `sit_render.vk`.
- [x] Frame-begin reset alongside `raster_stack_depth`.
- [x] **Slice 1:** `SIT_BLIT_FILTER_DOWNGRADE_NEAREST` wired in `SituationCmdBlitTexture`.
- [x] **Slice 2:** `SIT_TRANSFER_USAGE_COMPATIBLE_FALLBACK` (narrow sampled RGBA8 src table).
- [x] **Slice 3:** `SIT_TEXTURE_LAYOUT_ASSISTED` (hint-based barriers); unblocks optional 3b assisted path.
- [x] **Slice 4:** validation WARN/COMPAT + `guide/renderer_bolster.md` + command reference.
- [x] Harness: `transfer.behavior_policy_*`, `transfer.behavior_blit_filter_downgrade`, stack bounds, usage fallback, layout assisted (GL+VK @ v2.4.390).

**Exit criteria:** tweakable renderer behavior exists as explicit command state, not hidden global magic, and strict defaults remain the documented best-practice path.

---

## Test strategy

- [ ] Add narrow API validation tests for every new command:
  - [ ] null command buffer
  - [ ] invalid resource handle
  - [ ] offset/size out of bounds
  - [ ] unsupported capability
- [ ] Add GL+VK black-box behavior tests for every command that affects pixels or buffers.
- [ ] Prefer existing readback helpers:
  - [ ] `SituationReadFramebuffer`
  - [ ] `SituationReadTexture`
  - [ ] `SituationCreateReadbackBuffer`
  - [ ] `SituationReadBuffer`
- [ ] Add list mode labels so users can filter new tests:
  - [ ] `graphics.clear_*`
  - [ ] `graphics.copy_blit_*`
  - [ ] `graphics.indirect_*`
  - [ ] `graphics.raster_state_*`
  - [ ] `compute.dispatch_basic`
  - [ ] `compute.barrier_ssbo_readback`
  - [ ] `compute.readback_counter`
  - [ ] `compute.dispatch_indirect`
  - [ ] `graphics.query_*`
- [ ] Track expected OpenGL/Vulkan test count deltas in `doc/UPDATELOG.md`.

---

## Open questions

- [ ] Should clear commands be valid only inside render passes, or should texture clears be separate commands?
- [ ] Should old `SituationCmdPipelineBarrier(src_flags, dst_flags)` remain the common path, with resource barriers as advanced API?
- [ ] Should indirect command structs be public C structs, or should users fill raw buffers using documented byte layouts?
- [ ] How much primitive topology should Vulkan pipeline creation pre-enable as dynamic state?
- [ ] Should polygon line mode be exposed if future Web/GLES targets cannot support it cleanly?
- [ ] Should query results be frame-delayed by design to avoid blocking, matching readback guidance?
- [x] **VD inherit:** helper-only (**`SituationRenderPassInfoInherit`**) — no public **`SIT_LOAD_OP_INHERIT`** in v1 (**decided @ VD-1**).
- [x] **VD dynamic render passes (Vulkan):** ~~render-pass cache vs **`VK_KHR_dynamic_rendering`**?~~ **Decided (VD-1 addendum): dynamic rendering** — core in Vulkan 1.3+/1.4 (our target). Eliminates render-pass cache, framebuffer compat, and pipeline compat concerns for VD paths.
- [x] **sRGB composite:** **VK+GL** — SRGB storage at create; **`GL_FRAMEBUFFER_SRGB`** on VD passes; **`vd_srgb_format_composite`** harness green @ v2.4.387. **Open:** dedicated YPQ composite path when VD-6 HDR attachments land.
- [x] **VD command vs configure:** **v1 = configure-only** for tier B quality/settings; **`SituationCmdSetVirtualDisplay*`** post-v1 optional.
- [ ] **VD-6 vs v2.5:** which advanced knobs stay on VD vs move to **`SituationRenderTarget`**?
- [ ] **Multi-monitor Tier B:** single spike owner — **`SituationEndFrame` multi-present** vs per-surface command-buffer targets? (**Phase 11-bis Tier B** / v2.5 § P)
- [x] **Multi-monitor Tier A:** spanning host + multi-VD is the v2.4.x approach; no parallel frame API (**Phase 11-bis** — decided)
- [x] **MM-1/MM-2 helpers:** optional v2.4.x convenience after **MM-0**; harness-local math remains valid default (**MM-1** only if Tier A doc/examples need public WDM helpers)

---

## Suggested execution order

1. ~~Phase -1: compute harness split pilot.~~ **DONE**
2. ~~Phase 0: scope lock and audit.~~ **Mostly done** (doc items open: command stack table, parity matrix, validation style)
3. ~~Phase 1: clear commands.~~ **DONE** (stencil begin-pass clear deferred to stencil exposure)
4. ~~Phase 2 + Phase 3: compute error returns, indirect dispatch, and stronger barriers.~~ **DONE** (Phase **3b** color-attachment → transfer readback @ **v2.4.392**; **3c** deferred)
5. ~~Phase 4: transfer commands.~~ **DONE** (optional follow-ups in Phase 4.1D — non-blocking)
6. ~~Phase 5: indirect draw.~~ **DONE** (first slice; multi-draw deferred)
7. ~~Phase 6 + Phase 7: raster completeness and index type flexibility.~~ **DONE (v2.4.187)**
8. ~~**Phase 7-bis:** Vulkan 2D projection cleanup.~~ **DONE (v2.4.177)**
9. ~~**Phase 7-ter:** OpenGL deferred executor foundation.~~ **DONE (v2.4.183)**
10. ~~Phase 8~~ **indexed slice DONE (v2.4.184)** + Phase 9: push constants. **Phase 9 OPEN.**
11. **Phase 10 profiling:** **P10.0–P10.4 DONE** (v2.4.187 … v2.4.397) — Phase 10 exit criteria met for v2.4.x.
12. ~~**Phase 11 VD bolster (v2.4.x):** **VD-4a → VD-2 → VD-3 → VD-5**.~~ **DONE @ v2.4.387** (**VD-1 ✅ v2.4.316**). **VD-4b** MSAA only after scope gate (else v2.5 with **VD-6**).
13. ~~**Phase 14:** renderer behavior policy commands (`Set/Push/PopRendererBehavior`).~~ **DONE @ v2.4.391** (slices 1–4; transfer harness **21/21** GL+VK).
14. ~~**Phase 3b:** color attachment → transfer readback (`transfer.render_target_readback`).~~ **DONE @ v2.4.392** (transfer **22/22** GL+VK; VD composite **39/39** VK).
15. **Phase 11-bis (consent-gated):** **MM-0** doc first; **MM-1 … MM-3** only if maintainer approves ( **MM-1** = optional WDM helpers); **Tier B** design stays in v2.5 parking lot until spike.
16. **Next maintainer picks (parallel axes):** Phase **3c** (VD export / `SituationRenderTarget`); Phase **9** push constants; **P10.4** public query pool (P10.0–P10.3 ✅).
17. Phase 12: docs, examples, migration. **OPEN.**
18. Phase 13: compatibility wrapper review. **OPEN.**

---

## Non-goals for this plan

- [ ] Do not redesign the entire renderer module split.
- [ ] Do not remove Virtual Displays.
- [ ] Do not expose raw `VkCommandBuffer` / GL object state as the primary solution.
- [ ] Do not make FFmpeg/video depend on this work, except where future video readback benefits from copy/blit/readback commands.
- [ ] Do not require lock-free command buffers; correctness and backend parity come first.
- [ ] **Do not add parallel presentation stacks** (rogue monitor-window APIs, aux GL contexts, or test-only bypasses of **`SituationEndFrame`**). See **Phase 11-bis — Anti-patterns**.
- [ ] **Do not implement Phase 11-bis MM-1+ or Tier B without maintainer consent** on the checklist in that section.
