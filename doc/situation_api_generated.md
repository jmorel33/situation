# Situation API — Generated Supplement

_Auto-generated from `sit/situation_api.h` — Situation **2.4.191 (Vulkan Async Shader Poll + Harness Multi-Monitor VD + Phase 11-bis Plan)**._

This file documents **SITAPI symbols present in the header but not yet covered** in
[situation_api.md](situation_api.md). Re-generate with:

```bat
python scripts\generate_situation_api_docs.py
```

**Coverage:** 439/522 symbols documented in situation_api.md; **83** entries below.

---

## [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.

#### `SituationCmdBeginRenderToDisplay`
[DEPRECATED] Begins a render pass on a target (-1 for main window), clearing it.
```c
__attribute__((deprecated)) SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color);
```

---

#### `SituationLoadComputeShader`
[DEPRECATED] Load a compute shader from a file. Use SituationCreateComputePipeline instead.
```c
SituationError SituationLoadComputeShader(const char* cs_path, SituationShader* out_shader);
```

---

#### `SituationLoadComputeShaderFromMemory`
[DEPRECATED] Create a compute shader from memory. Use SituationCreateComputePipelineFromMemory instead.
```c
SituationError SituationLoadComputeShaderFromMemory(const char* cs_code, SituationShader* out_shader);
```

---

## Abstracted Rendering Commands

#### `SituationCmdBindDescriptorSetDynamic`
[Core] Binds a dynamic buffer descriptor set with an offset.
```c
SituationError SituationCmdBindDescriptorSetDynamic(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer, uint32_t dynamic_offset);
```

---

#### `SituationCmdBindIndexBufferEx`
[Core] Bind index buffer with 16- or 32-bit element type for subsequent SituationCmdDrawIndexed.
```c
SituationError SituationCmdBindIndexBufferEx(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset, SituationIndexType index_type);
```

---

#### `SituationCmdBindSampledTexture`
Binds a texture as a sampled image (sampler2D) to a binding point.
```c
SituationError SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);
```

---

#### `SituationCmdClear`
Mid-pass clear of active render-pass attachments; begin-pass clears use SituationRenderPassInfo loadOp.
```c
SituationError SituationCmdClear(SituationCommandBuffer cmd, uint32_t clear_flags, const SituationClearValue* clear_value);
```

---

#### `SituationCmdClearColor`
Mid-pass clear of the active color attachment.
```c
SituationError SituationCmdClearColor(SituationCommandBuffer cmd, ColorRGBA color);
```

---

#### `SituationCmdClearDepth`
Mid-pass clear of the active depth attachment.
```c
SituationError SituationCmdClearDepth(SituationCommandBuffer cmd, float depth);
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

#### `SituationCmdDrawIndexedIndirect`
[Core] Indexed indirect draw (32-bit indices; requires bound index buffer). firstIndex is relative to SituationCmdBindIndexBuffer offset.
```c
SituationError SituationCmdDrawIndexedIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset);
```

---

#### `SituationCmdDrawIndirect`
[Core] Draw from a CPU/GPU-filled SituationDrawIndirectCommand in an indirect buffer (requires active render pass, bound pipeline, and vertex buffers).
```c
SituationError SituationCmdDrawIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset);
```

---

#### `SituationCmdSetScissorIndexed`
Sets scissor at index (0 = default scissor).
```c
SituationError SituationCmdSetScissorIndexed(SituationCommandBuffer cmd, uint32_t index, int x, int y, int width, int height);
```

---

#### `SituationCmdSetVertexAttribute`
[OpenGL Only] Attribute format + vertex buffer binding index (must match SituationCmdBindVertexBuffer).
```c
SituationError SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, uint32_t binding, int size, SituationDataType type, bool normalized, size_t offset);
```

---

#### `SituationCmdSetViewportIndexed`
Sets viewport at index (0 = default viewport).
```c
SituationError SituationCmdSetViewportIndexed(SituationCommandBuffer cmd, uint32_t index, float x, float y, float width, float height);
```

---

## Command Buffer Recording

#### `SituationCmdBeginDebugGroup`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdBeginDebugGroup(SituationCommandBuffer cmd, const char* name, ColorRGBA color);
```

---

#### `SituationCmdEndDebugGroup`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdEndDebugGroup(SituationCommandBuffer cmd);
```

---

#### `SituationCmdPopRasterState`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdPopRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
```

---

#### `SituationCmdPushRasterState`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdPushRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
```

---

#### `SituationCmdSetBlendEnable`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetBlendEnable(SituationCommandBuffer cmd, bool enable);
```

---

#### `SituationCmdSetBlendFuncSeparate`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetBlendFuncSeparate(SituationCommandBuffer cmd, SituationBlendFactor src_rgb, SituationBlendFactor dst_rgb, SituationBlendFactor src_a, SituationBlendFactor dst_a);
```

---

#### `SituationCmdSetColorWriteMask`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetColorWriteMask(SituationCommandBuffer cmd, bool r, bool g, bool b, bool a);
```

---

#### `SituationCmdSetCullMode`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetCullMode(SituationCommandBuffer cmd, SituationCullMode mode);
```

---

#### `SituationCmdSetDepthBias`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetDepthBias(SituationCommandBuffer cmd, bool enable, float constant_factor, float clamp, float slope_factor);
```

---

#### `SituationCmdSetDepthTest`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetDepthTest(SituationCommandBuffer cmd, bool enable, SituationDepthCompareOp depth_op);
```

---

#### `SituationCmdSetDepthWrite`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetDepthWrite(SituationCommandBuffer cmd, bool enable);
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

#### `SituationCmdSetMultisampleState`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetMultisampleState(SituationCommandBuffer cmd, const SituationMultisampleState* state);
```

---

#### `SituationCmdSetPolygonMode`
_See `sit/situation_api.h` for the authoritative declaration._
```c
SituationError SituationCmdSetPolygonMode(SituationCommandBuffer cmd, SituationPolygonMode mode);
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

#### `SituationCmdBufferBarrier`
Record an explicit buffer-range memory barrier.
```c
SituationError SituationCmdBufferBarrier(SituationCommandBuffer cmd, const SituationBufferBarrierDesc* desc);
```

---

#### `SituationCmdDispatchEx`
Record a compute dispatch with validation and error reporting.
```c
SituationError SituationCmdDispatchEx(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
```

---

#### `SituationCmdDispatchIndirect`
Record an indirect compute dispatch.
```c
SituationError SituationCmdDispatchIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset);
```

---

#### `SituationCmdPipelineBarrierEx`
Record an explicit global memory barrier.
```c
SituationError SituationCmdPipelineBarrierEx(SituationCommandBuffer cmd, const SituationPipelineBarrierDesc* desc);
```

---

#### `SituationCmdTextureBarrier`
Record an explicit texture layout/memory barrier.
```c
SituationError SituationCmdTextureBarrier(SituationCommandBuffer cmd, SituationTexture texture, const SituationTextureBarrierDesc* desc);
```

---

## CPU & Thread Management

#### `SituationBuildNumaNodeMask`
All logical CPUs on a NUMA node
```c
uint64_t SituationBuildNumaNodeMask(int numa_node_index);
```

---

#### `SituationBuildPhysicalCoreMask`
All logical CPUs on one physical core
```c
uint64_t SituationBuildPhysicalCoreMask(int physical_core_index);
```

---

#### `SituationBuildUniqueCoreMask`
One LP per core
```c
uint64_t SituationBuildUniqueCoreMask(int start_physical_core, int count, bool avoid_siblings);
```

---

#### `SituationDumpThreadPoolMetrics`
Metrics-only dump
```c
void SituationDumpThreadPoolMetrics(SituationThreadPool* pool, FILE* out_stream, bool json_mode);
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

#### `SituationGetConfiguredAudioThreadAffinity`
Effective audio mask (init or default)
```c
uint64_t SituationGetConfiguredAudioThreadAffinity(void);
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

#### `SituationGetCpuTopology`
Pointer to cached topology (NULL on failure)
```c
bool SituationGetCpuTopology(const SituationCpuTopology** out_topology);
```

---

#### `SituationGetCurrentProcessorIndex`
Logical CPU index for current thread, or -1 if unknown
```c
int  SituationGetCurrentProcessorIndex(void);
```

---

#### `SituationGetHighQueueDepth`
High-priority queue depth
```c
size_t SituationGetHighQueueDepth(SituationThreadPool* pool);
```

---

#### `SituationGetNumaTopology`
Cached NUMA snapshot
```c
bool SituationGetNumaTopology(const SituationNumaTopology** out_topology);
```

---

#### `SituationGetPreferredNumaNode`
TLS: node for current thread, or -1 if unset
```c
int SituationGetPreferredNumaNode(void);
```

---

#### `SituationGetQueueDepth`
Pending jobs per queue mask
```c
size_t SituationGetQueueDepth(SituationThreadPool* pool, SituationJobQueueMask mask);
```

---

#### `SituationGetRecommendedWorkerCount`
Sizing helper (no pool required)
```c
uint32_t SituationGetRecommendedWorkerCount(uint32_t reserved_threads, bool use_physical_cores);
```

---

#### `SituationGetThreadAffinity`
Reads affinity mask for the CURRENT thread
```c
bool SituationGetThreadAffinity(uint64_t* out_mask);
```

---

#### `SituationGetThreadNumaNode`
NUMA node for current thread, or -1 if unknown
```c
int  SituationGetThreadNumaNode(void);
```

---

#### `SituationGetInternalThreadPool`
Returns pointer to the library's internal thread pool (NULL if not initialized).
```c
SituationThreadPool* SituationGetInternalThreadPool(void);
```

---

#### `SituationGetThreadPoolMetrics`
Scheduler counters snapshot
```c
bool SituationGetThreadPoolMetrics(SituationThreadPool* pool, SituationThreadPoolMetrics* out_metrics);
```

---

#### `SituationGetThreadPoolSnapshot`
Worker/I/O/render/audio placement snapshot
```c
bool SituationGetThreadPoolSnapshot(SituationThreadPool* pool, SituationThreadPoolSnapshot* out);
```

---

#### `SituationGetThreadingStatus`
Runtime threading capabilities + pool summary
```c
SituationThreadingStatus SituationGetThreadingStatus(void);
```

---

#### `SituationPrintThreadingStatus`
Human-readable threading status (stdout if NULL)
```c
void SituationPrintThreadingStatus(FILE* out_stream);
```

---

#### `SituationRefreshCpuTopology`
Rebuilds the process-wide topology cache
```c
bool SituationRefreshCpuTopology(void);
```

---

#### `SituationRefreshNumaTopology`
Rebuild NUMA summary from CPU topology + OS memory
```c
bool SituationRefreshNumaTopology(void);
```

---

#### `SituationResetThreadPoolStats`
Zero scheduler counters
```c
void SituationResetThreadPoolStats(SituationThreadPool* pool);
```

---

#### `SituationSetThreadAffinityEx`
Set affinity; optional previous mask
```c
bool SituationSetThreadAffinityEx(uint64_t core_mask, uint64_t* out_previous);
```

---

## Frame Lifecycle & Command Buffer

#### `SituationGetComputeCommandBuffer`
[v2.3.23] Get the compute-specific command buffer (Vulkan only).
```c
SituationCommandBuffer SituationGetComputeCommandBuffer(void);
```

---

## GPU Buffer Management

#### `SituationCmdCopyBufferEx`
Error-returning buffer-copy command with independent source/destination offsets.
```c
SituationError SituationCmdCopyBufferEx(SituationCommandBuffer cmd, SituationBuffer src, SituationBuffer dst, size_t src_offset, size_t dst_offset, size_t size);
```

---

## MIDI Device Control

#### `SituationGetMidiDeviceName`
PortMidi device name for device_id (hardware or virtual).
```c
SituationError SituationGetMidiDeviceName(int device_id, char* out_name, size_t out_name_size);
```

---

#### `SituationSetNodeMidiChannel`
Filter MIDI to channel 0-15, or -1 omni.
```c
SituationError SituationSetNodeMidiChannel(SituationAudioGraph* graph, SituationNodeHandle handle, int channel);
```

---

## Texture Management

#### `SituationCmdBlitTexture`
Blit between color 2D textures; caller owns explicit texture barriers.
```c
SituationError SituationCmdBlitTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureBlitRegion* region);
```

---

#### `SituationCmdCopyBufferToTexture`
Upload tightly packed RGBA8 rows from a buffer into a texture subregion; caller owns texture barriers.
```c
SituationError SituationCmdCopyBufferToTexture(SituationCommandBuffer cmd, SituationBuffer src, size_t src_offset, SituationTexture dst, const SituationTextureCopyRegion* dst_region);
```

---

#### `SituationCmdCopyTexture`
Exact-size copy between color 2D textures; caller owns explicit texture barriers.
```c
SituationError SituationCmdCopyTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureCopyRegion* region);
```

---

#### `SituationCmdCopyTextureToBuffer`
Copy a texture subregion into a buffer (`dst_row_pitch` 0 = width * 4); caller owns texture barriers.
```c
SituationError SituationCmdCopyTextureToBuffer(SituationCommandBuffer cmd, SituationTexture src, const SituationTextureCopyRegion* src_region, SituationBuffer dst, size_t dst_offset, size_t dst_row_pitch);
```

---

## Virtual MIDI loopback (integration testing; no hardware keyboard required)

#### `SituationSetupVirtualMidiLoopback`
Create connected virtual out→in pair. Returns input device_id for SituationEnableMidiControl().
```c
SituationError SituationSetupVirtualMidiLoopback(int* out_input_device_id);
```

---

#### `SituationTeardownVirtualMidiLoopback`
Close and destroy the virtual loopback devices.
```c
void SituationTeardownVirtualMidiLoopback(void);
```

---

#### `SituationVirtualMidiControlChange`
CC (e.g. mod wheel, expression).
```c
SituationError SituationVirtualMidiControlChange(uint8_t channel, uint8_t controller, uint8_t value);
```

---

#### `SituationVirtualMidiNoteOff`
Inject note-off on channel 0 (legacy wrapper).
```c
SituationError SituationVirtualMidiNoteOff(uint8_t note);
```

---

#### `SituationVirtualMidiNoteOffEx`
Channel-aware note-off (0-15).
```c
SituationError SituationVirtualMidiNoteOffEx(uint8_t channel, uint8_t note);
```

---

#### `SituationVirtualMidiNoteOn`
Inject note-on on channel 0 (legacy wrapper).
```c
SituationError SituationVirtualMidiNoteOn(uint8_t note, uint8_t velocity);
```

---

#### `SituationVirtualMidiNoteOnEx`
Channel-aware note-on (0-15).
```c
SituationError SituationVirtualMidiNoteOnEx(uint8_t channel, uint8_t note, uint8_t velocity);
```

---

#### `SituationVirtualMidiPitchBend`
Pitch bend 0..16383 (center 8192).
```c
SituationError SituationVirtualMidiPitchBend(uint8_t channel, int16_t bend);
```

---

#### `SituationVirtualMidiProgramChange`
Program change on channel 0-15.
```c
SituationError SituationVirtualMidiProgramChange(uint8_t channel, uint8_t program);
```

---
