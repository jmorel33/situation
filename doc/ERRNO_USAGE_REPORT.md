# Errno Usage Report

Generated: 2026-06-06 01:24
Script: `scripts/audit_errno_report.ps1`

## Summary

| Metric | Count |
|--------|-------|
| Defined in X-macro table | 246 |
| Compat aliases (#define) | 10 |
| Total unique names | 256 |
| Used (any reference in sit/ + tests/) | 258 |
| Used strictly (error-producing lines) | 133 |
| **Never produced (strict)** | **123** |
| Never referenced at all | 0 |
| EOL-tagged (deprecated) | 15 |
| Phantom (used but undefined) | 0 |

## Never Produced - With Candidate Homes

These error codes are defined but never appear on a `return` or
`_SituationSetErrorFromCode` line. For each, candidate functions are
identified where they could logically be used (based on keyword matching
and detection of `SITUATION_ERROR_GENERAL` in domain-relevant code).

### Asset & Serialization (4)

#### `SITUATION_ERROR_ASSET_CORRUPTED`

> Asset data is corrupted or failed checksum validation

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_ASSET_DECOMPRESSION_FAILED`

> Failed to decompress asset payload

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_ASSET_PARSE_FAILED`

> Failed to parse asset file (malformed JSON/XML/Binary)

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_ASSET_VERSION_MISMATCH`

> Asset was built for an incompatible engine version

**Related API functions** (keyword match: `asset``, ``version`)**:**

- `SituationIsVersionCompatible`

### Audio (5)

#### `SITUATION_ERROR_AUDIO_CAPTURE_NOT_AVAILABLE`

> No microphone or capture device found

**Candidate homes:**

- `SituationStartAudioCapture` - API name matches keywords: audio, capture
- `SituationStartAudioCaptureEx` - API name matches keywords: audio, capture
- `SituationStopAudioCapture` - API name matches keywords: audio, capture

#### `SITUATION_ERROR_AUDIO_CONVERTER`

> Failed to configure a data format/rate converter

**Related API functions** (keyword match: `audio``, ``converter`)**:**

- `SituationGetAudioPlaybackSampleRate`
- `SituationSetAudioDevice`
- `SituationSetAudioPlaybackSampleRate`

#### `SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED`

> Codec/container not supported

**Related API functions** (keyword match: `audio``, ``decoder``, ``format`)**:**

- `SituationGetAudioPlaybackSampleRate`
- `SituationSetAudioDevice`
- `SituationSetAudioPlaybackSampleRate`

#### `SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED`

> ma_decoder_init failed

**Related API functions** (keyword match: `audio``, ``decoder``, ``init`)**:**

- `SituationGetAudioPlaybackSampleRate`
- `SituationGetInitState`
- `SituationInit`
- `SituationInitDeviceRegistry`
- `SituationSetAudioDevice`

#### `SITUATION_ERROR_AUDIO_STREAM_ENDED`

> Stream reached EOF (not fatal)

**Related API functions** (keyword match: `audio``, ``stream``, ``ended`)**:**

- `SituationGetAudioPlaybackSampleRate`
- `SituationLoadSoundFromStream`
- `SituationSetAudioDevice`
- `SituationSetAudioPlaybackSampleRate`

### Compat Alias -> SITUATION_ERROR_FILE_ACCESS_DENIED (1)

#### `SITUATION_ERROR_ACCESS_DENIED`

Compat alias for `SITUATION_ERROR_FILE_ACCESS_DENIED` - use the target name instead.

### Compat Alias -> SITUATION_ERROR_OPENGL_GENERAL (1)

#### `SITUATION_ERROR_GL_ERROR`

Compat alias for `SITUATION_ERROR_OPENGL_GENERAL` - use the target name instead.

### Compat Alias -> SITUATION_ERROR_OPENGL_LOADER_FAILED (1)

#### `SITUATION_ERROR_GLAD_LOAD_FAILED`

Compat alias for `SITUATION_ERROR_OPENGL_LOADER_FAILED` - use the target name instead.

### Compat Alias -> SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED (1)

#### `SITUATION_ERROR_SHADER_LINK_FAILED`

Compat alias for `SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED` - use the target name instead.

### Compat Alias -> SITUATION_ERROR_OPENGL_UNSUPPORTED (1)

#### `SITUATION_ERROR_GL_EXTENSION_MISSING`

Compat alias for `SITUATION_ERROR_OPENGL_UNSUPPORTED` - use the target name instead.

### Compat Alias -> SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION (1)

#### `SITUATION_ERROR_GL_VERSION_TOO_LOW`

Compat alias for `SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION` - use the target name instead.

### Compat Alias -> SITUATION_ERROR_TEXTURE_UPLOAD_FAILED (2)

#### `SITUATION_ERROR_GL_UPLOAD_FAILED`

Compat alias for `SITUATION_ERROR_TEXTURE_UPLOAD_FAILED` - use the target name instead.

#### `SITUATION_ERROR_VULKAN_UPLOAD_FAILED`

Compat alias for `SITUATION_ERROR_TEXTURE_UPLOAD_FAILED` - use the target name instead.

### Compat Alias -> SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED (1)

#### `SITUATION_ERROR_VULKAN_PIPELINE_CREATE_FAILED`

Compat alias for `SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED` - use the target name instead.

### Compat Alias -> SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED (1)

#### `SITUATION_ERROR_SHADER_MODULE_CREATE_FAILED`

Compat alias for `SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED` - use the target name instead.

### Compute (3)

#### `SITUATION_ERROR_COMPUTE_BUFFER_BINDING_MISSING`

> Missing storage buffer binding

**Candidate homes:**

- `SituationCmdBindComputeBuffer` - API name matches keywords: compute, buffer, binding
- `SituationGetComputeCommandBuffer` - API name matches keywords: compute, buffer, binding

#### `SITUATION_ERROR_COMPUTE_DISPATCH_FAILED`

> Compute dispatch failed

**Related API functions** (keyword match: `compute``, ``dispatch`)**:**

- `SituationCmdBindComputeTexture`
- `SituationCmdDispatch`
- `SituationCmdDispatchEx`
- `SituationCmdDispatchIndirect`
- `SituationCreateComputePipeline`

#### `SITUATION_ERROR_COMPUTE_PIPELINE_CREATION_FAILED`

> Compute pipeline creation failed

**Candidate homes:**

- `SituationDestroyComputePipeline` - API name matches keywords: compute, pipeline, creation
- `SituationCreateComputePipeline` - API name matches keywords: compute, pipeline, creation
- `SituationReloadComputePipeline` - API name matches keywords: compute, pipeline, creation
- `SituationCreateComputePipelineFromMemory` - API name matches keywords: compute, pipeline, creation
- `SituationCmdBindComputePipeline` - API name matches keywords: compute, pipeline, creation

### Core & System (15)

#### `SITUATION_ERROR_ARM_INTRINSICS_FAILED`

> ARM-specific WFE/SEV intrinsic failure

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_ASSERTION_FAILED`

> Debug assertion tripped

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MEMORY_ACCESS`

> Invalid or unmapped memory access

**Related API functions** (keyword match: `memory``, ``access`)**:**

- `SituationLoadBitmapFontFromMemory`
- `SituationLoadFontFromMemory`
- `SituationLoadImageFromMemory`

#### `SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT`

> Render thread join timeout

**Related API functions** (keyword match: `render``, ``backpressure`)**:**

- `SituationExportRenderHistogram`
- `SituationGetRenderHeight`
- `SituationGetRenderWidth`

#### `SITUATION_ERROR_THREAD_ATOMIC_FAILED`

> Atomic operation failed or not supported

**Related API functions** (keyword match: `thread``, ``atomic`)**:**

- `SituationGetCPUThreadCount`
- `SituationSetThreadAffinity`
- `SituationSetThreadAffinityEx`

#### `SITUATION_ERROR_THREAD_BUFFER_OVERFLOW`

> Thread-local buffer overflow

**Related API functions** (keyword match: `thread``, ``buffer`)**:**

- `SituationAcquireFrameCommandBuffer`
- `SituationGetComputeCommandBuffer`
- `SituationGetCPUThreadCount`
- `SituationGetMainCommandBuffer`
- `SituationSetThreadAffinity`

#### `SITUATION_ERROR_THREAD_DEADLOCK_DETECTED`

> Potential deadlock detected

**Related API functions** (keyword match: `thread``, ``deadlock``, ``detected`)**:**

- `SituationGetCPUThreadCount`
- `SituationSetThreadAffinity`
- `SituationSetThreadAffinityEx`

#### `SITUATION_ERROR_THREAD_DETACH_FAILED`

> Thread detach operation failed (thrd_detach)

**Related API functions** (keyword match: `thread``, ``detach`)**:**

- `SituationDetachAudioProcessor`
- `SituationGetCPUThreadCount`
- `SituationSetThreadAffinity`
- `SituationSetThreadAffinityEx`

#### `SITUATION_ERROR_THREAD_JOIN_FAILED`

> Thread join operation failed (thrd_join)

**Related API functions** (keyword match: `thread``, ``join`)**:**

- `SituationGetCPUThreadCount`
- `SituationSetThreadAffinity`
- `SituationSetThreadAffinityEx`

#### `SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED`

> Mutex lock operation failed (mtx_lock)

**Related API functions** (keyword match: `thread``, ``mutex``, ``lock`)**:**

- `SituationGetCPUThreadCount`
- `SituationIsLockKeyPressed`
- `SituationIsScrollLockOn`
- `SituationSetThreadAffinity`
- `SituationSetThreadAffinityEx`

#### `SITUATION_ERROR_THREAD_MUTEX_TIMEOUT`

> Mutex lock timeout (deadlock prevention)

**Related API functions** (keyword match: `thread``, ``mutex`)**:**

- `SituationGetCPUThreadCount`
- `SituationSetThreadAffinity`
- `SituationSetThreadAffinityEx`

#### `SITUATION_ERROR_THREAD_MUTEX_UNLOCK_FAILED`

> Mutex unlock operation failed (mtx_unlock)

**Related API functions** (keyword match: `thread``, ``mutex``, ``unlock`)**:**

- `SituationGetCPUThreadCount`
- `SituationSetThreadAffinity`
- `SituationSetThreadAffinityEx`

#### `SITUATION_ERROR_THREAD_NOT_AVAILABLE`

> Threading not available on this platform

**Candidate homes:**

- `SituationSetThreadAffinity` - API name matches keywords: thread
- `SituationResetThreadPoolStats` - API name matches keywords: thread
- `SituationDumpThreadPoolMetrics` - API name matches keywords: thread
- `SituationGetConfiguredIOThreadAffinity` - API name matches keywords: thread
- `SituationDestroyThreadPool` - API name matches keywords: thread

#### `SITUATION_ERROR_THREAD_STATE_INVALID`

> Invalid thread state for requested operation

**Related API functions** (keyword match: `thread``, ``state`)**:**

- `SituationClearWindowState`
- `SituationGetCPUThreadCount`
- `SituationGetInitState`
- `SituationSetThreadAffinity`
- `SituationSetThreadAffinityEx`

#### `SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION`

> Architectural rule broken: Update called after Draw

**Related API functions** (keyword match: `update``, ``after``, ``draw``, ``violation`)**:**

- `SituationImageDraw`
- `SituationImageDrawAlpha`
- `SituationImageDrawCodepoint`
- `SituationUpdate`
- `SituationUpdateBuffer`

### Device Registry (10)

#### `SITUATION_ERROR_DEVICE_CATEGORY_INVALID`

> Invalid device category

**Related API functions** (keyword match: `device``, ``category`)**:**

- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`
- `SituationGetVulkanDevice`

#### `SITUATION_ERROR_DEVICE_CONTROL_INVALID`

> Invalid control definition

**Related API functions** (keyword match: `device``, ``control`)**:**

- `SituationEnableMidiControl`
- `SituationGetBufferDeviceAddress`
- `SituationGetControl`
- `SituationGetDeviceInfo`
- `SituationGetVulkanDevice`

#### `SITUATION_ERROR_DEVICE_CREATE_FAILED`

> Device creation failed

**Related API functions** (keyword match: `device``, ``create`)**:**

- `SituationCreateImage`
- `SituationCreateMesh`
- `SituationCreateTexture`
- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`

#### `SITUATION_ERROR_DEVICE_DESTROY_FAILED`

> Device destruction failed

**Related API functions** (keyword match: `device``, ``destroy`)**:**

- `SituationDestroyComputePipeline`
- `SituationDestroyMesh`
- `SituationDestroyTexture`
- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`

#### `SITUATION_ERROR_DEVICE_FUNCTION_TABLE_INVALID`

> Invalid device function table

**Related API functions** (keyword match: `device``, ``function``, ``table`)**:**

- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`
- `SituationGetVulkanDevice`

#### `SITUATION_ERROR_DEVICE_PORT_INVALID`

> Invalid port definition

**Related API functions** (keyword match: `device``, ``port`)**:**

- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`
- `SituationGetVulkanDevice`

#### `SITUATION_ERROR_DEVICE_PROCESS_FAILED`

> Device processing failed

**Related API functions** (keyword match: `device``, ``process`)**:**

- `SituationFreeProcessList`
- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`
- `SituationGetVulkanDevice`

#### `SITUATION_ERROR_DEVICE_QUERY_FAILED`

> Device query operation failed

**Related API functions** (keyword match: `device``, ``query`)**:**

- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`
- `SituationGetVulkanDevice`

#### `SITUATION_ERROR_DEVICE_REGISTRY_NOT_INITIALIZED`

> Device registry not initialized

**Related API functions** (keyword match: `device``, ``registry`)**:**

- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`
- `SituationGetVulkanDevice`
- `SituationInitDeviceRegistry`

#### `SITUATION_ERROR_DEVICE_TYPE_INVALID`

> Invalid device type ID

**Related API functions** (keyword match: `device``, ``type`)**:**

- `SituationGetBufferDeviceAddress`
- `SituationGetDeviceInfo`
- `SituationGetRendererType`
- `SituationGetVulkanDevice`
- `SituationRegisterDeviceType`

### Display (2)

#### `SITUATION_ERROR_DISPLAY_SET` [EOL]

> Failed to set a display mode on a physical monitor

**Candidate homes:**

- `SituationSetDisplayMode` - API name matches keywords: display, set
- `SituationSetVirtualDisplayDirty` - API name matches keywords: display, set
- `SituationSetVirtualDisplayScalingMode` - API name matches keywords: display, set

#### `SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT` [EOL]

> The maximum number of virtual displays has been reached

**Candidate homes:**

- `SituationDestroyVirtualDisplay` - API name matches keywords: virtual, display
- `SituationIsVirtualDisplayDirty` - API name matches keywords: virtual, display
- `SituationCreateVirtualDisplay` - API name matches keywords: virtual, display
- `SituationSetVirtualDisplayScalingMode` - API name matches keywords: virtual, display
- `SituationConfigureVirtualDisplay` - API name matches keywords: virtual, display

### Filesystem (5)

#### `SITUATION_ERROR_FILE_MODIFIED`

> File changed on disk during operation (hot-reload / race)

**Related API functions** (keyword match: `file``, ``modified`)**:**

- `SituationIsFileDropped`
- `SituationOpenFile`
- `SituationSetFileDropCallback`

#### `SITUATION_ERROR_HOTRELOAD_FILE_CHANGED_TOO_FAST`

> File changed faster than debounce window

**Related API functions** (keyword match: `hotreload``, ``file``, ``changed``, ``too``, ``fast`)**:**

- `SituationIsFileDropped`
- `SituationOpenFile`
- `SituationSetFileDropCallback`

#### `SITUATION_ERROR_HOTRELOAD_GPU_SYNC_FAILED`

> GPU sync failed during hot-reload (vkDeviceWaitIdle/glFinish)

**Related API functions** (keyword match: `hotreload``, ``gpu``, ``sync`)**:**

- `SituationSetVSync`

#### `SITUATION_ERROR_HOTRELOAD_WATCHER_FAILED`

> Hot-reload watcher failed (inotify/ReadDirectoryChangesW)

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_PERMISSION_DENIED` [EOL]

> Permission was denied for the requested file operation

*No candidate homes found - likely reserved for future subsystem.*

### Fonts (3)

#### `SITUATION_ERROR_FONT_ATLAS_FULL`

> Font texture atlas is full; cannot pack more glyphs

**Related API functions** (keyword match: `font``, ``atlas``, ``full`)**:**

- `SituationBakeFontAtlas`
- `SituationLoadBitmapFontFromMemory`
- `SituationLoadFont`
- `SituationLoadFontFromMemory`

#### `SITUATION_ERROR_FONT_GLYPH_MISSING`

> Requested glyph is not present in the font

**Related API functions** (keyword match: `font``, ``glyph`)**:**

- `SituationLoadBitmapFontFromMemory`
- `SituationLoadFont`
- `SituationLoadFontFromMemory`

#### `SITUATION_ERROR_FONT_LOAD_FAILED`

> Failed to load font face (e.g., FreeType/stb_truetype error)

**Candidate homes:**

- `SituationLoadFontFromMemory` - API name matches keywords: font, load
- `SituationLoadBitmapFontFromMemory` - API name matches keywords: font, load
- `SituationLoadFont` - API name matches keywords: font, load

### Image (1)

#### `SITUATION_ERROR_IMAGE_OPERATION_FAILED`

> Image operation failed (crop, resize, flip, or save)

**Related API functions** (keyword match: `image``, ``operation`)**:**

- `SituationLoadImage`
- `SituationLoadImageFromMemory`
- `SituationUnloadImage`

### Mixer (15)

#### `SITUATION_ERROR_MIXER_BUS_INVALID`

> Invalid bus ID or bus not active

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MIXER_BUS_LIMIT`

> Maximum number of aux buses reached

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MIXER_INSERT_ALREADY_ATTACHED`

> Insert chain already attached at this position

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MIXER_INSERT_INVALID`

> Invalid insert position

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED`

> No insert chain at this position

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MIXER_NOT_INITIALIZED`

> Mixer not initialized

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MIXER_ROUTING_CYCLE`

> Routing would create a cycle (feedback loop)

**Related API functions** (keyword match: `mixer``, ``routing``, ``cycle`)**:**

- `SituationSetToneRouting`

#### `SITUATION_ERROR_MIXER_ROUTING_INVALID`

> Invalid routing configuration

**Related API functions** (keyword match: `mixer``, ``routing`)**:**

- `SituationSetToneRouting`

#### `SITUATION_ERROR_MIXER_SCENE_LOAD_FAILED`

> Failed to load mixer scene

**Related API functions** (keyword match: `mixer``, ``scene``, ``load`)**:**

- `SituationLoadFont`
- `SituationLoadImage`
- `SituationLoadImageFromMemory`

#### `SITUATION_ERROR_MIXER_SCENE_SAVE_FAILED`

> Failed to save mixer scene

**Related API functions** (keyword match: `mixer``, ``scene``, ``save`)**:**

- `SituationSaveGraphToFile`
- `SituationSaveMidiPreset`
- `SituationSaveModelAsGltf`

#### `SITUATION_ERROR_MIXER_SCENE_VERSION_MISMATCH`

> Scene file version incompatible

**Related API functions** (keyword match: `mixer``, ``scene``, ``version`)**:**

- `SituationIsVersionCompatible`

#### `SITUATION_ERROR_MIXER_SEND_INVALID`

> Invalid aux send configuration

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MIXER_TOPOLOGY_LOCKED`

> Cannot modify topology while processing

**Related API functions** (keyword match: `mixer``, ``topology`)**:**

- `SituationCmdSetPrimitiveTopology`
- `SituationGetCpuTopology`
- `SituationRefreshCpuTopology`

#### `SITUATION_ERROR_MIXER_TRACK_INVALID`

> Invalid track ID or track not active

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_MIXER_TRACK_LIMIT`

> Maximum number of tracks reached

*No candidate homes found - likely reserved for future subsystem.*

### Network (11)

#### `SITUATION_ERROR_NETWORK_ACCEPT_FAILED`

> Network: Accept failed

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_NETWORK_BIND_FAILED`

> Network: Bind failed

**Related API functions** (keyword match: `network``, ``bind`)**:**

- `SituationCmdBindDescriptorSet`
- `SituationCmdBindDescriptorSetDynamic`
- `SituationCmdBindPipeline`

#### `SITUATION_ERROR_NETWORK_CONNECTION_FAILED`

> Network: Connection failed

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_NETWORK_DNS_RESOLUTION_FAILED`

> Network: DNS resolution failed

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_NETWORK_INIT_FAILED`

> Network: Initialization failed

**Related API functions** (keyword match: `network``, ``init`)**:**

- `SituationGetInitState`
- `SituationInit`
- `SituationInitDeviceRegistry`

#### `SITUATION_ERROR_NETWORK_LISTEN_FAILED`

> Network: Listen failed

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_NETWORK_RECEIVE_FAILED`

> Network: Receive failed

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_NETWORK_SEND_FAILED`

> Network: Send failed

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_NETWORK_SOCKET_CREATION_FAILED`

> Network: Socket creation failed

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_NETWORK_TIMEOUT`

> Network: Operation timed out

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_NETWORK_TLS_HANDSHAKE_FAILED`

> Network: TLS/SSL handshake failed

*No candidate homes found - likely reserved for future subsystem.*

### Node Graph (11)

#### `SITUATION_ERROR_NODE_ALREADY_EXISTS`

> Node with this ID already exists

**Candidate homes:**

- `SituationDestroyNode` - API name matches keywords: node
- `SituationSetNodeMidiChannel` - API name matches keywords: node
- `SituationGetNodePCMFreeFrames` - API name matches keywords: node
- `SituationPushNodePCM` - API name matches keywords: node
- `SituationCreateNode` - API name matches keywords: node

#### `SITUATION_ERROR_NODE_CHANNEL_MISMATCH`

> Channel count mismatch (mono vs stereo)

**Related API functions** (keyword match: `node``, ``channel`)**:**

- `SituationCreateNode`
- `SituationDestroyNode`
- `SituationPushNodePCM`
- `SituationSetNodeMidiChannel`

#### `SITUATION_ERROR_NODE_CONTROL_OUT_OF_RANGE`

> Control value out of valid range

**Related API functions** (keyword match: `node``, ``control``, ``out``, ``range`)**:**

- `SituationCreateNode`
- `SituationDestroyNode`
- `SituationEnableMidiControl`
- `SituationGetControl`
- `SituationPushNodePCM`

#### `SITUATION_ERROR_NODE_CONTROL_TYPE_MISMATCH`

> Control type mismatch (float vs int vs bool)

**Related API functions** (keyword match: `node``, ``control``, ``type`)**:**

- `SituationCreateNode`
- `SituationDestroyNode`
- `SituationEnableMidiControl`
- `SituationGetControl`
- `SituationGetRendererType`

#### `SITUATION_ERROR_NODE_DESERIALIZATION_FAILED`

> Failed to deserialize node graph

**Related API functions** (keyword match: `node``, ``deserialization`)**:**

- `SituationCreateNode`
- `SituationDestroyNode`
- `SituationPushNodePCM`

#### `SITUATION_ERROR_NODE_GRAPH_NOT_INITIALIZED`

> Node graph not initialized

**Related API functions** (keyword match: `node``, ``graph`)**:**

- `SituationCreateNode`
- `SituationDestroyGraph`
- `SituationDestroyNode`
- `SituationPushNodePCM`
- `SituationSaveGraphToFile`

#### `SITUATION_ERROR_NODE_NOT_FOUND`

> Node not found in graph

**Candidate homes:**

- `SituationDestroyNode` - API name matches keywords: node
- `SituationSetNodeMidiChannel` - API name matches keywords: node
- `SituationGetNodePCMFreeFrames` - API name matches keywords: node
- `SituationPushNodePCM` - API name matches keywords: node
- `SituationCreateNode` - API name matches keywords: node

#### `SITUATION_ERROR_NODE_PATCH_NOT_FOUND`

> Patch not found

**Related API functions** (keyword match: `node``, ``patch`)**:**

- `SituationCreateNode`
- `SituationCreatePatch`
- `SituationDestroyNode`
- `SituationDestroyPatch`
- `SituationPushNodePCM`

#### `SITUATION_ERROR_NODE_PORT_TYPE_MISMATCH`

> Port type mismatch (audio vs control)

**Related API functions** (keyword match: `node``, ``port``, ``type`)**:**

- `SituationCreateNode`
- `SituationDestroyNode`
- `SituationGetRendererType`
- `SituationPushNodePCM`
- `SituationRegisterDeviceType`

#### `SITUATION_ERROR_NODE_PROCESSING_FAILED`

> Node processing failed (device error)

**Related API functions** (keyword match: `node``, ``processing`)**:**

- `SituationCreateNode`
- `SituationDestroyNode`
- `SituationPushNodePCM`

#### `SITUATION_ERROR_NODE_TOPOLOGY_INVALID`

> Invalid graph topology (disconnected, no output, etc.)

**Related API functions** (keyword match: `node``, ``topology`)**:**

- `SituationCmdSetPrimitiveTopology`
- `SituationCreateNode`
- `SituationDestroyNode`
- `SituationGetCpuTopology`
- `SituationPushNodePCM`

### OpenGL (7)

#### `SITUATION_ERROR_OPENGL_CONTEXT_CREATION_FAILED`

> OpenGL: Context creation failed

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_OPENGL_PROGRAM_VALIDATION_FAILED`

> OpenGL: Program validation failed

**Related API functions** (keyword match: `opengl``, ``program``, ``validation`)**:**

- `SituationVirtualMidiProgramChange`

#### `SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED`

> OpenGL: Detailed shader linking error

**Related API functions** (keyword match: `opengl``, ``shader``, ``link`)**:**

- `SituationBeginLoadShaderFromMemory`
- `SituationLoadShader`
- `SituationLoadShaderFromMemory`

#### `SITUATION_ERROR_OPENGL_SPIRV_CS_SPECIALIZE_FAILED`

> OpenGL: SPIR-V compute shader specialization failed

**Related API functions** (keyword match: `opengl``, ``spirv``, ``specialize`)**:**

- `SituationBeginLoadShaderFromSpirvMemory`
- `SituationBeginLoadShaderFromSpirvMemoryEx`
- `SituationLoadShaderFromSpirv`

#### `SITUATION_ERROR_OPENGL_SPIRV_FS_SPECIALIZE_FAILED`

> OpenGL: SPIR-V fragment shader specialization failed

**Related API functions** (keyword match: `opengl``, ``spirv``, ``specialize`)**:**

- `SituationBeginLoadShaderFromSpirvMemory`
- `SituationBeginLoadShaderFromSpirvMemoryEx`
- `SituationLoadShaderFromSpirv`

#### `SITUATION_ERROR_OPENGL_SPIRV_PROGRAM_LINK_FAILED`

> OpenGL: SPIR-V graphics program link failed

**Related API functions** (keyword match: `opengl``, ``spirv``, ``program``, ``link`)**:**

- `SituationBeginLoadShaderFromSpirvMemory`
- `SituationBeginLoadShaderFromSpirvMemoryEx`
- `SituationLoadShaderFromSpirv`
- `SituationVirtualMidiProgramChange`

#### `SITUATION_ERROR_OPENGL_SPIRV_VS_SPECIALIZE_FAILED`

> OpenGL: SPIR-V vertex shader specialization failed

**Related API functions** (keyword match: `opengl``, ``spirv``, ``specialize`)**:**

- `SituationBeginLoadShaderFromSpirvMemory`
- `SituationBeginLoadShaderFromSpirvMemoryEx`
- `SituationLoadShaderFromSpirv`

### Platform & Windowing (4)

#### `SITUATION_ERROR_APP_STATE_FAILED`

> Application state transition failed (pause/resume/target FPS)

**Related API functions** (keyword match: `app``, ``state`)**:**

- `SituationClearWindowState`
- `SituationGetInitState`
- `SituationIsAppPaused`
- `SituationPauseApp`
- `SituationResumeApp`

#### `SITUATION_ERROR_COM_INITIALIZATION_FAILED`

> COM initialization failed (Windows)

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_DXGI_QUERY_FAILED`

> DXGI GPU query failed (Windows)

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_WINDOW_FOCUS` [EOL]

> An operation related to window focus failed

**Related API functions** (keyword match: `window``, ``focus`)**:**

- `SituationClearWindowState`
- `SituationHasWindowFocus`
- `SituationSetFocusCallback`
- `SituationSetWindowState`
- `SituationWindowShouldClose`

### Plugins & Scripting (3)

#### `SITUATION_ERROR_PLUGIN_ABI_MISMATCH`

> Plugin ABI version does not match the host engine

*No candidate homes found - likely reserved for future subsystem.*

#### `SITUATION_ERROR_PLUGIN_LOAD_FAILED`

> Failed to load dynamic library / shared object

**Related API functions** (keyword match: `plugin``, ``load`)**:**

- `SituationLoadFont`
- `SituationLoadImage`
- `SituationLoadImageFromMemory`

#### `SITUATION_ERROR_PLUGIN_SYMBOL_NOT_FOUND`

> Required symbol or function not found in plugin

*No candidate homes found - likely reserved for future subsystem.*

### Rendering Core (6)

#### `SITUATION_ERROR_BACKEND_MISMATCH`

> Operation requested on wrong backend

**Related API functions** (keyword match: `backend`)**:**

- `SituationGetGraphicsBackend`

#### `SITUATION_ERROR_BACKEND_SPECIFIC`

> Backend-specific GPU operation failed (see detail)

**Related API functions** (keyword match: `backend``, ``specific`)**:**

- `SituationGetGraphicsBackend`

#### `SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER`

> No frame acquired

**Candidate homes:**

- `SituationGetMainCommandBuffer` - API name matches keywords: active, command, buffer
- `SituationAcquireFrameCommandBuffer` - API name matches keywords: active, command, buffer
- `SituationGetComputeCommandBuffer` - API name matches keywords: active, command, buffer

#### `SITUATION_ERROR_RENDER_PASS_ACTIVE`

> Operation illegal during an active render pass

**Candidate homes:**

- `SituationCmdEndRenderPass` - API name matches keywords: render, pass, active
- `SituationCmdBeginRenderPass` - API name matches keywords: render, pass, active
- `SituationGetMainWindowRenderPass` - API name matches keywords: render, pass, active

#### `SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE`

> Nested render pass attempted

**Candidate homes:**

- `SituationCmdEndRenderPass` - API name matches keywords: render, pass
- `SituationCmdBeginRenderPass` - API name matches keywords: render, pass
- `SituationGetMainWindowRenderPass` - API name matches keywords: render, pass

#### `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED`

> Use-after-free attempt

*No candidate homes found - likely reserved for future subsystem.*

### Vulkan (8)

#### `SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED`

> Vulkan: Command buffer record, submit, or sync failed

**Candidate homes:**

- `SituationGetMainCommandBuffer` - API name matches keywords: vulkan, command, buffer
- `SituationAcquireFrameCommandBuffer` - API name matches keywords: vulkan, command, buffer
- `SituationGetComputeCommandBuffer` - API name matches keywords: vulkan, command, buffer

#### `SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED`

> Vulkan: Logical device creation failed

**Candidate homes:**

- `SituationGetVulkanDevice` - API name matches keywords: vulkan, device, creation
- `SituationGetVulkanPhysicalDevice` - API name matches keywords: vulkan, device, creation

#### `SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED`

> Vulkan: Detailed instance creation failure

**Related API functions** (keyword match: `vulkan``, ``instance``, ``creation`)**:**

- `SituationGetVulkanDevice`
- `SituationGetVulkanInstance`
- `SituationGetVulkanPhysicalDevice`

#### `SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE`

> Vulkan: No suitable physical device found

**Candidate homes:**

- `SituationGetVulkanPhysicalDevice` - API name matches keywords: vulkan, physical, device, unsuitable
- `SituationGetVulkanDevice` - API name matches keywords: vulkan, physical, device, unsuitable

#### `SITUATION_ERROR_VULKAN_PIPELINE_FAILED` [EOL]

> Vulkan: Failed to create graphics or compute pipeline

**Related API functions** (keyword match: `vulkan``, ``pipeline`)**:**

- `SituationCmdBindPipeline`
- `SituationCmdPipelineBarrier`
- `SituationCreateComputePipeline`
- `SituationGetVulkanDevice`
- `SituationGetVulkanInstance`

#### `SITUATION_ERROR_VULKAN_SPIRV_CS_MODULE_FAILED`

> Vulkan: SPIR-V compute shader module creation failed

**Related API functions** (keyword match: `vulkan``, ``spirv``, ``module`)**:**

- `SituationBeginLoadShaderFromSpirvMemory`
- `SituationBeginLoadShaderFromSpirvMemoryEx`
- `SituationGetVulkanDevice`
- `SituationGetVulkanInstance`
- `SituationGetVulkanPhysicalDevice`

#### `SITUATION_ERROR_VULKAN_SPIRV_FS_MODULE_FAILED`

> Vulkan: SPIR-V fragment shader module creation failed

**Related API functions** (keyword match: `vulkan``, ``spirv``, ``module`)**:**

- `SituationBeginLoadShaderFromSpirvMemory`
- `SituationBeginLoadShaderFromSpirvMemoryEx`
- `SituationGetVulkanDevice`
- `SituationGetVulkanInstance`
- `SituationGetVulkanPhysicalDevice`

#### `SITUATION_ERROR_VULKAN_SPIRV_VS_MODULE_FAILED`

> Vulkan: SPIR-V vertex shader module creation failed

**Related API functions** (keyword match: `vulkan``, ``spirv``, ``module`)**:**

- `SituationBeginLoadShaderFromSpirvMemory`
- `SituationBeginLoadShaderFromSpirvMemoryEx`
- `SituationGetVulkanDevice`
- `SituationGetVulkanInstance`
- `SituationGetVulkanPhysicalDevice`

---

## Actionability Summary

| Category | Count |
|----------|-------|
| Unused errors with candidate homes | 82 |
| Unused errors without candidates (reserved/future) | 31 |
| Phantom errors (need table entry or rename) | 0 |

## Recommendations

1. **Phantom errors** - add to the table or rename the usage to a valid code.
2. **Errors with candidate homes** - replace `SITUATION_ERROR_GENERAL` with the specific code in identified functions.
3. **EOL + never produced** - safe candidates for removal in a future cleanup pass.
4. **No candidates (reserved)** - keep in table; wire up when subsystem ships.


