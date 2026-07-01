# Situation API — Generated Supplement

_Auto-generated from `sit/situation_api.h` — Situation **2.4.403 (EXE PE version from situation_base_version.h via sit_version.mk.)**._

Regenerate:

```bat
python tools\generate_api_index.py
```

**Coverage:** 601/644 symbols in situation_api.md; **43** below.

---

## Abstracted Rendering Commands

#### `SituationCmdBindDescriptorSetDynamic`
[GL+VK] [Core] Binds a dynamic buffer descriptor set with an offset.
```c
SituationError SituationCmdBindDescriptorSetDynamic(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer, uint32_t dynamic_offset);
```

---

#### `SituationCmdClearDepthStencil`
Mid-pass clear of active depth and stencil attachments.
```c
SituationError SituationCmdClearDepthStencil(SituationCommandBuffer cmd, float depth, uint32_t stencil);
```

---

#### `SituationCmdClearStencil`
Mid-pass clear of the active stencil attachment when supported by backend/attachment state.
```c
SituationError SituationCmdClearStencil(SituationCommandBuffer cmd, uint32_t stencil);
```

---

#### `SituationRenderPassConfigurationKey`
Vulkan render-pass cache key (load/store + target class).
```c
uint32_t SituationRenderPassConfigurationKey(const SituationRenderPassInfo* info);
```

---

#### `SituationRenderPassInfoForRenderTarget`
Clear user RT pass (display_id ignored).
```c
SituationRenderPassInfo SituationRenderPassInfoForRenderTarget(SituationRenderTarget render_target, ColorRGBA clear_color);
```

---

#### `SituationRenderPassInfoLoad`
Preserve attachment contents (composite / resume pass).
```c
SituationRenderPassInfo SituationRenderPassInfoLoad(int display_id);
```

---

## Command Buffer Recording

#### `SituationCmdPopRendererBehavior`
Restore policy saved by PushRendererBehavior.
```c
SituationError SituationCmdPopRendererBehavior(SituationCommandBuffer cmd, uint32_t scope_id);
```

---

#### `SituationCmdPushRendererBehavior`
Push current policy; mutate with Set inside scope.
```c
SituationError SituationCmdPushRendererBehavior(SituationCommandBuffer cmd, uint32_t scope_id);
```

---

#### `SituationCmdSetColorWriteMask`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetColorWriteMask(SituationCommandBuffer cmd, bool r, bool g, bool b, bool a);
```

---

#### `SituationCmdSetFrontFace`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetFrontFace(SituationCommandBuffer cmd, SituationFrontFace front_face);
```

---

#### `SituationCmdSetLineWidth`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetLineWidth(SituationCommandBuffer cmd, float width);
```

---

#### `SituationCmdSetPrimitiveTopology`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetPrimitiveTopology(SituationCommandBuffer cmd, SituationPrimitiveTopology topology);
```

---

#### `SituationCmdSetPushConstantData`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetPushConstantData(SituationCommandBuffer cmd, SituationShader shader, uint32_t offset, const void* data, size_t size);
```

---

#### `SituationCmdSetStencilTest`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetStencilTest(SituationCommandBuffer cmd, bool enable, const SituationStencilState* front, const SituationStencilState* back);
```

---

## Compute Shader Pipeline

#### `SituationCmdDispatchEx`
Record a compute dispatch with validation and error reporting.
```c
SituationError SituationCmdDispatchEx(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
```

---

## CPU & Thread Management

#### `SituationBuildUniqueCoreMask`
One LP per core
```c
uint64_t SituationBuildUniqueCoreMask(int start_physical_core, int count, bool avoid_siblings);
```

---

#### `SituationDumpThreadPoolStatus`
Pool metrics + per-role CPU snapshot
```c
void SituationDumpThreadPoolStatus(SituationThreadPool* pool, FILE* out_stream, bool json_mode);
```

---

#### `SituationDumpThreadingReport`
Status + topology line + pool dump
```c
void SituationDumpThreadingReport(SituationThreadPool* pool, FILE* out_stream, bool json_mode);
```

---

#### `SituationGetActiveJobCount`
active_jobs counter
```c
int SituationGetActiveJobCount(SituationThreadPool* pool);
```

---

#### `SituationGetCPUCoreCount`
Gets physical processors (Cores) from cached topology
```c
uint32_t SituationGetCPUCoreCount(void);
```

---

#### `SituationGetConfiguredAudioThreadAffinity`
Effective audio mask (init or default)
```c
uint64_t SituationGetConfiguredAudioThreadAffinity(void);
```

---

#### `SituationGetConfiguredIOThreadAffinity`
Effective I/O mask (init or default CPU 3)
```c
uint64_t SituationGetConfiguredIOThreadAffinity(void);
```

---

#### `SituationGetConfiguredMainThreadAffinity`
Init mask for main thread (0 = no pin)
```c
uint64_t SituationGetConfiguredMainThreadAffinity(void);
```

---

#### `SituationGetConfiguredRenderThreadAffinity`
Effective render mask (init or default)
```c
uint64_t SituationGetConfiguredRenderThreadAffinity(void);
```

---

#### `SituationGetCurrentProcessorIndex`
Logical CPU index for current thread, or -1 if unknown
```c
int SituationGetCurrentProcessorIndex(void);
```

---

#### `SituationGetHighQueueDepth`
High-priority queue depth
```c
size_t SituationGetHighQueueDepth(SituationThreadPool* pool);
```

---

#### `SituationGetPreferredNumaNode`
TLS: node for current thread, or -1 if unset
```c
int SituationGetPreferredNumaNode(void);
```

---

#### `SituationGetThreadAffinity`
Reads affinity mask for the CURRENT thread
```c
SituationError SituationGetThreadAffinity(uint64_t* out_mask);
```

---

#### `SituationGetThreadNumaNode`
NUMA node for current thread, or -1 if unknown
```c
int SituationGetThreadNumaNode(void);
```

---

#### `SituationSetThreadAffinityEx`
Set affinity; optional previous mask
```c
SituationError SituationSetThreadAffinityEx(uint64_t core_mask, uint64_t* out_previous);
```

---

## Device Enumeration (Phase 0)

#### `SituationFindBestDevice`
Find the best matching device by type and channel requirements.
```c
SituationAudioDeviceInfo* SituationFindBestDevice(SituationAudioDeviceType preferred_type, uint32_t min_channels_out, uint32_t min_channels_in);
```

---

## Device Registry Functions

#### `SituationIsDeviceRegistered`
Check if a device type is registered.
```c
bool SituationIsDeviceRegistered(SituationNodeType type);
```

---

## Frame Lifecycle & Command Buffer

#### `SituationReplayRenderList`
Replay a previously recorded render list into a command buffer.
```c
void SituationReplayRenderList(SituationCommandBuffer cmd, SituationRenderList list);
```

---

#### `SituationResetRenderList`
Reset a render list for reuse next frame.
```c
void SituationResetRenderList(SituationRenderList list);
```

---

## Graph Serialization Functions

#### `SituationDeserializeGraphFromJSON`
Deserialize a graph from a JSON string.
```c
SituationError SituationDeserializeGraphFromJSON(SituationAudioGraph* graph, const char* json_string, const SituationDeviceFunctions* device_funcs, int num_device_funcs);
```

---

#### `SituationGetSerializationVersion`
Get the current serialization format version string.
```c
char* SituationGetSerializationVersion(void);
```

---

## Node Graph Functions

#### `SituationDestroyPatch`
Disconnect a patch between two ports (legacy, no is_control param).
```c
SituationError SituationDestroyPatch(SituationAudioGraph* graph, SituationNodeHandle src, int src_port, SituationNodeHandle dst, int dst_port);
```

---

## User render targets (Phase 3c — offscreen without VD compositor)

#### `SituationCreateRenderTarget`
Create color (+ optional depth) offscreen target; msaa_samples must be 1.
```c
SituationError SituationCreateRenderTarget(const SituationRenderTargetDesc* desc, SituationRenderTarget* out_rt);
```

---

#### `SituationDestroyRenderTarget`
Destroy RT and invalidate handle.
```c
void SituationDestroyRenderTarget(SituationRenderTarget* rt);
```

---

#### `SituationGetRenderTargetTexture`
Registry color texture for sampling / transfer readback.
```c
SituationError SituationGetRenderTargetTexture(SituationRenderTarget rt, SituationTexture* out_tex);
```

---

#### `SituationReadRenderTarget`
Blocking RGBA8 readback of resolved color.
```c
SituationError SituationReadRenderTarget(SituationRenderTarget rt, const SituationReadPixelsDesc* desc, void* dst_pixels, size_t dst_size_bytes);
```

---

## Virtual Displays (Render Targets)

#### `SituationVdStandbyPackParamsStd430`
Layer params SSBO (P10).
```c
void SituationVdStandbyPackParamsStd430(uint8_t out[SIT_VD_STANDBY_PARAMS_SSBO_SIZE], const SitVdStandbyConfig* cfg);
```

---

## Window State Queries

#### `SituationIsWindowHidden`
Check if the window is currently hidden.
```c
bool SituationIsWindowHidden(void);
```

---
