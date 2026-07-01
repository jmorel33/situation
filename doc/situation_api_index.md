# Situation Public API Index

_Auto-generated from `sit/situation_api.h` — Situation **2.4.403 (EXE PE version from situation_base_version.h via sit_version.mk.)**._

Regenerate: `python tools/generate_api_index.py`

Bindings: `python tools/generate_odin_bindings.py` → [bindings/odin/](../bindings/odin/)

- **[situation_sdk.md](situation_sdk.md)** — SDK manual
- **[situation_api.md](situation_api.md)** — detailed reference
- **[situation_command_reference.md](situation_command_reference.md)** — `SituationCmd*`
- **[situation_api_generated.md](situation_api_generated.md)** — gaps

**Total public functions:** 644 (`SituationCmd*`: 81)

---

## Command buffer (`SituationCmd*`)

Canonical narrative: **[situation_command_reference.md](situation_command_reference.md)**.

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBeginDebugGroup` | [command ref](situation_command_reference.md#11-debug-markers) | — |
| `SituationCmdBeginOcclusionQuery` | [command ref](situation_command_reference.md#10-user-query-pools-p104) | — |
| `SituationCmdBeginRenderPass` | [command ref](situation_command_reference.md#1-render-pass--framebuffer) | [GL+VK] Begins a render pass with detailed configuration. |
| `SituationCmdBeginRenderToDisplay` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdBindComputeBuffer` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdBindComputePipeline` | [command ref](situation_command_reference.md#7-compute) | Bind a compute pipeline for a subsequent dispatch. |
| `SituationCmdBindComputeTexture` | [command ref](situation_command_reference.md#7-compute) | [Core] Binds a texture as a storage image for compute shaders. |
| `SituationCmdBindDescriptorSet` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [GL+VK] [Core] Binds a buffer's descriptor set (UBO/SSBO) to a set index. |
| `SituationCmdBindDescriptorSetDynamic` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [GL+VK] [Core] Binds a dynamic buffer descriptor set with an offset. |
| `SituationCmdBindIndexBuffer` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind a 32-bit index buffer (SIT_INDEX_UINT32). Pass offset 0 when indices start at the beg... |
| `SituationCmdBindIndexBufferEx` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind index buffer with 16- or 32-bit element type for subsequent SituationCmdDrawIndexed. |
| `SituationCmdBindMeshPullBuffers` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [VK+GL] Push mesh vertex/index BDA block for pull shaders (SituationMeshPullPushConstants @ offse... |
| `SituationCmdBindPipeline` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [GL+VK] Binds a graphics pipeline (shader program) for subsequent draws. |
| `SituationCmdBindSampledTexture` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | Binds a texture as a sampled image (sampler2D) to a binding point. |
| `SituationCmdBindTexture` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdBindTextureSet` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [GL+VK] [Core] Binds a texture's descriptor set (sampler/storage) to a set index. |
| `SituationCmdBindUniformBuffer` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdBindVertexBuffer` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind a vertex buffer for subsequent SituationCmdDraw / SituationCmdDrawIndexed. |
| `SituationCmdBlitTexture` | [command ref](situation_command_reference.md#8-transfer--presentation) | Blit between color 2D textures; caller owns explicit texture barriers. |
| `SituationCmdBufferBarrier` | [command ref](situation_command_reference.md#7-compute) | Record an explicit buffer-range memory barrier. |
| `SituationCmdClear` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of active render-pass attachments; begin-pass clears use SituationRenderPassInfo l... |
| `SituationCmdClearColor` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of the active color attachment. |
| `SituationCmdClearDepth` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of the active depth attachment. |
| `SituationCmdClearDepthStencil` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of active depth and stencil attachments. |
| `SituationCmdClearStencil` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of the active stencil attachment when supported by backend/attachment state. |
| `SituationCmdCopyBuffer` | [command ref](situation_command_reference.md#8-transfer--presentation) | Legacy buffer-copy command (now returns error). |
| `SituationCmdCopyBufferEx` | [command ref](situation_command_reference.md#8-transfer--presentation) | Error-returning buffer-copy command with independent source/destination offsets. |
| `SituationCmdCopyBufferToTexture` | [command ref](situation_command_reference.md#8-transfer--presentation) | Upload tightly packed RGBA8 rows from a buffer into a texture subregion; caller owns texture barr... |
| `SituationCmdCopyTexture` | [command ref](situation_command_reference.md#8-transfer--presentation) | Exact-size copy between color 2D textures; caller owns explicit texture barriers. |
| `SituationCmdCopyTextureToBuffer` | [command ref](situation_command_reference.md#8-transfer--presentation) | Copy a texture subregion into a buffer (`dst_row_pitch` 0 = width * 4); caller owns texture barri... |
| `SituationCmdDispatch` | [command ref](situation_command_reference.md#7-compute) | Record a command to dispatch compute shader work groups. |
| `SituationCmdDispatchEx` | [command ref](situation_command_reference.md#7-compute) | Record a compute dispatch with validation and error reporting. |
| `SituationCmdDispatchIndirect` | [command ref](situation_command_reference.md#7-compute) | Record an indirect compute dispatch. |
| `SituationCmdDraw` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [GL+VK] [Core] Record a non-indexed draw call. |
| `SituationCmdDrawIndexed` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [GL+VK] [Core] Record an indexed draw call. |
| `SituationCmdDrawIndexedIndirect` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Indexed indirect draw (32-bit indices; requires bound index buffer). firstIndex is relativ... |
| `SituationCmdDrawIndirect` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Draw from a CPU/GPU-filled SituationDrawIndirectCommand in an indirect buffer (requires ac... |
| `SituationCmdDrawMesh` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Records a command to draw a complete, pre-configured mesh. |
| `SituationCmdDrawQuad` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Record a command to draw a simple, colored 2D quad. |
| `SituationCmdDrawText` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Draws a text string using GPU-accelerated textured quads. |
| `SituationCmdDrawTextBoxed` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Text clipped to a rectangle with optional word wrap. |
| `SituationCmdDrawTextEx` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Advanced text drawing (scaling/spacing). |
| `SituationCmdDrawTexture` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Draw a part of a texture defined by a rectangle. |
| `SituationCmdDrawTextureYpqGrade` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Draw texture with YPQ grade (matches SituationImageAdjustYPQ). |
| `SituationCmdEndDebugGroup` | [command ref](situation_command_reference.md#11-debug-markers) | — |
| `SituationCmdEndOcclusionQuery` | [command ref](situation_command_reference.md#10-user-query-pools-p104) | — |
| `SituationCmdEndRender` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdEndRenderPass` | [command ref](situation_command_reference.md#1-render-pass--framebuffer) | [GL+VK] Ends the current render pass. |
| `SituationCmdGPUZoneBegin` | [command ref](situation_command_reference.md#9-gpu-profiling-zones-p103) | P10.3: GPU elapsed zone begin (no-op when unsupported) |
| `SituationCmdGPUZoneEnd` | [command ref](situation_command_reference.md#9-gpu-profiling-zones-p103) | P10.3: GPU elapsed zone end |
| `SituationCmdPipelineBarrier` | [command ref](situation_command_reference.md#7-compute) | Legacy convenience barrier; prefer SituationCmdPipelineBarrierEx, SituationCmdBufferBarrier, or S... |
| `SituationCmdPipelineBarrierEx` | [command ref](situation_command_reference.md#7-compute) | Record an explicit global memory barrier. |
| `SituationCmdPopRasterState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdPopRendererBehavior` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | Restore policy saved by PushRendererBehavior. |
| `SituationCmdPresent` | [command ref](situation_command_reference.md#8-transfer--presentation) | Submits a command to copy a texture to the main window's swapchain (Compute-Only). |
| `SituationCmdPushRasterState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdPushRendererBehavior` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | Push current policy; mutate with Set inside scope. |
| `SituationCmdResetQueryPool` | [command ref](situation_command_reference.md#10-user-query-pools-p104) | — |
| `SituationCmdSetBlendEnable` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetBlendFuncSeparate` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetColorWriteMask` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetCullMode` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthBias` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthTest` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthWrite` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetFrontFace` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetLineWidth` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetMultisampleState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetPolygonMode` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetPrimitiveTopology` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetPushConstant` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Set a small block of per-draw uniform data (push constant). |
| `SituationCmdSetPushConstantData` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | — |
| `SituationCmdSetRendererBehavior` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | Replace active renderer behavior policy. |
| `SituationCmdSetScissor` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets the dynamic scissor rectangle to clip rendering. |
| `SituationCmdSetScissorIndexed` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets scissor at index (0 = default scissor). |
| `SituationCmdSetStencilTest` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetVertexAttribute` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [OpenGL Only, Deprecated v2.4] Attribute format + vertex buffer binding index. Prefer vertex pull... |
| `SituationCmdSetViewport` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets the dynamic viewport and scissor for the current render pass. |
| `SituationCmdSetViewportIndexed` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets viewport at index (0 = default viewport). |
| `SituationCmdTextureBarrier` | [command ref](situation_command_reference.md#7-compute) | Record an explicit texture layout/memory barrier. |
| `SituationCmdWriteTimestamp` | [command ref](situation_command_reference.md#10-user-query-pools-p104) | — |

---

## 3D Model Utilities

| Function | Summary |
|----------|---------|
| `SituationDrawModel` | Draws all sub-meshes of a model with a single root transformation. |
| `SituationGetMeshData` | Get raw vertex/index data pointers from a mesh (read-only). |
| `SituationLoadModel` | Loads a complete 3D model and its textures from a GLTF file. |
| `SituationLoadModelFromOBJ` | Wavefront OBJ: triangulated meshes, MTL/textures; missing/degenerate normals filled from face geometry, authored norm... |
| `SituationLoadModelFromSTL` | Loads a 3D model from a binary or ASCII STL file. UVs are zeroed; normals are flat (per-face) by default, or smooth (... |
| `SituationSaveModelAsGltf` | Exports a model to a human-readable .gltf and a .bin file for debugging. |
| `SituationUnloadModel` | Frees all GPU and CPU resources associated with a loaded model. |

## Abstracted Rendering Commands

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBeginRenderPass` | [command ref](situation_command_reference.md#1-render-pass--framebuffer) | [GL+VK] Begins a render pass with detailed configuration. |
| `SituationCmdBindComputeTexture` | [command ref](situation_command_reference.md#7-compute) | [Core] Binds a texture as a storage image for compute shaders. |
| `SituationCmdBindDescriptorSet` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [GL+VK] [Core] Binds a buffer's descriptor set (UBO/SSBO) to a set index. |
| `SituationCmdBindDescriptorSetDynamic` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [GL+VK] [Core] Binds a dynamic buffer descriptor set with an offset. |
| `SituationCmdBindIndexBuffer` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind a 32-bit index buffer (SIT_INDEX_UINT32). Pass offset 0 when indices start at the beginning of the buffer. |
| `SituationCmdBindIndexBufferEx` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind index buffer with 16- or 32-bit element type for subsequent SituationCmdDrawIndexed. |
| `SituationCmdBindPipeline` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [GL+VK] Binds a graphics pipeline (shader program) for subsequent draws. |
| `SituationCmdBindSampledTexture` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | Binds a texture as a sampled image (sampler2D) to a binding point. |
| `SituationCmdBindTextureSet` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [GL+VK] [Core] Binds a texture's descriptor set (sampler/storage) to a set index. |
| `SituationCmdBindVertexBuffer` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind a vertex buffer for subsequent SituationCmdDraw / SituationCmdDrawIndexed. |
| `SituationCmdClear` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of active render-pass attachments; begin-pass clears use SituationRenderPassInfo loadOp. |
| `SituationCmdClearColor` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of the active color attachment. |
| `SituationCmdClearDepth` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of the active depth attachment. |
| `SituationCmdClearDepthStencil` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of active depth and stencil attachments. |
| `SituationCmdClearStencil` | [command ref](situation_command_reference.md#8-transfer--presentation) | Mid-pass clear of the active stencil attachment when supported by backend/attachment state. |
| `SituationCmdDraw` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [GL+VK] [Core] Record a non-indexed draw call. |
| `SituationCmdDrawIndexed` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [GL+VK] [Core] Record an indexed draw call. |
| `SituationCmdDrawIndexedIndirect` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Indexed indirect draw (32-bit indices; requires bound index buffer). firstIndex is relative to SituationCmdBin... |
| `SituationCmdDrawIndirect` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Draw from a CPU/GPU-filled SituationDrawIndirectCommand in an indirect buffer (requires active render pass, bo... |
| `SituationCmdDrawMesh` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Records a command to draw a complete, pre-configured mesh. |
| `SituationCmdDrawQuad` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Record a command to draw a simple, colored 2D quad. |
| `SituationCmdDrawText` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Draws a text string using GPU-accelerated textured quads. |
| `SituationCmdDrawTextBoxed` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Text clipped to a rectangle with optional word wrap. |
| `SituationCmdDrawTextEx` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Advanced text drawing (scaling/spacing). |
| `SituationCmdDrawTexture` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Draw a part of a texture defined by a rectangle. |
| `SituationCmdDrawTextureYpqGrade` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Draw texture with YPQ grade (matches SituationImageAdjustYPQ). |
| `SituationCmdEndRenderPass` | [command ref](situation_command_reference.md#1-render-pass--framebuffer) | [GL+VK] Ends the current render pass. |
| `SituationCmdPresent` | [command ref](situation_command_reference.md#8-transfer--presentation) | Submits a command to copy a texture to the main window's swapchain (Compute-Only). |
| `SituationCmdSetPushConstant` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Set a small block of per-draw uniform data (push constant). |
| `SituationCmdSetScissor` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets the dynamic scissor rectangle to clip rendering. |
| `SituationCmdSetScissorIndexed` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets scissor at index (0 = default scissor). |
| `SituationCmdSetVertexAttribute` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [OpenGL Only, Deprecated v2.4] Attribute format + vertex buffer binding index. Prefer vertex pulling via SituationGet... |
| `SituationCmdSetViewport` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets the dynamic viewport and scissor for the current render pass. |
| `SituationCmdSetViewportIndexed` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets viewport at index (0 = default viewport). |
| `SituationRenderPassConfigurationKey` | — | Vulkan render-pass cache key (load/store + target class). |
| `SituationRenderPassInfoDefault` | — | Clear color+depth; store color; discard depth/stencil. |
| `SituationRenderPassInfoForRenderTarget` | — | Clear user RT pass (display_id ignored). |
| `SituationRenderPassInfoLoad` | — | Preserve attachment contents (composite / resume pass). |

## Active Audio Device Query (v2.4.199)

| Function | Summary |
|----------|---------|
| `SituationExecuteCommand` | Execute a shell command hidden, return exit code & combined output. |
| `SituationGetActiveAudioDeviceName` | Get the name of the currently active playback device (static buffer, do not free). |
| `SituationGetCurrentDriveLetter` | Get the drive letter of the running executable (Windows only). |
| `SituationGetDriveInfo` | Get info for a specific drive (Windows only). |
| `SituationGetGraphicsBackend` | — |
| `SituationGetGraphicsBackendName` | — |
| `SituationGetGraphicsCaps` | Get backend capabilities for examples/frameworks. |
| `SituationGetUserDirectory` | [Caller frees] Get the full path to the current user's home directory. |
| `SituationOpenFile` | Open a file or folder with its default application. |
| `SituationWin32SetAppUserModelId` | Set shell AppUserModelID before first window (UTF-8); idempotent per process. |

## Active Graph (Audio Callback Integration)

| Function | Summary |
|----------|---------|
| `SituationGetActiveGraph` | Get the currently active audio processing graph (NULL if none). |
| `SituationSetActiveGraph` | Set the active audio processing graph (replaces default). NULL disables graph processing. |

## Advanced Window Profile Management

| Function | Summary |
|----------|---------|
| `SituationApplyCurrentProfileWindowState` | Manually apply the appropriate window state profile based on current focus. |
| `SituationGetCurrentActualWindowStateFlags` | Gets flags based on current GLFW window state |
| `SituationSetWindowStateProfiles` | Set the flag profiles for when the window is focused vs. unfocused. |
| `SituationToggleWindowStateFlags` | Toggle flags in the current profile and apply the result. |

## Application Lifecycle & State

| Function | Summary |
|----------|---------|
| `SituationGetInitState` | Query the current initialization state (thread-safe). |
| `SituationGetVersionString` | [Main thread] Returns a read-only static string (e.g., "2.4.336"). Do not free. |
| `SituationInit` | [Main thread] Initialize the library, create window and graphics context. |
| `SituationIsAppPaused` | Check if the application is currently paused. |
| `SituationIsInitialized` | [Main thread] Check if the library has been successfully initialized. |
| `SituationPauseApp` | Pause the application's internal state (e.g., audio). |
| `SituationPollInputEvents` | [Main thread] Poll for all input events (keyboard, mouse, joystick). Call once per frame. |
| `SituationResumeApp` | Resume a paused application. |
| `SituationShutdown` | [Main thread] Shut down the library and release all resources. |
| `SituationUpdateTimers` | [Main thread] Update all internal timers (frame timer, temporal system). Call after polling events. |
| `SituationWindowShouldClose` | Check if the application should close (e.g., user clicked X). |

## Audio

| Function | Summary |
|----------|---------|
| `SituationGetAudioDevices` | — |

## Audio Capture

| Function | Summary |
|----------|---------|
| `SituationStartAudioCapture` | Start capturing audio input with default format. |
| `SituationStartAudioCaptureEx` | Start capturing with explicit sample rate and channel count. |
| `SituationStopAudioCapture` | Stop audio capture and release the input device. |

## Audio Device Management

| Function | Summary |
|----------|---------|
| `SituationGetAudioMasterVolume` | Get the master volume for the audio device. |
| `SituationGetAudioPlaybackSampleRate` | Get the sample rate of the current audio device. |
| `SituationIsAudioDevicePlaying` | Check if the audio device is currently playing. |
| `SituationPauseAudioDevice` | Pause audio playback on the device. |
| `SituationResumeAudioDevice` | Resume audio playback on the device. |
| `SituationSetAudioDevice` | Set the active audio device. |
| `SituationSetAudioMasterVolume` | Set the master volume for the audio device. |
| `SituationSetAudioPlaybackSampleRate` | Re-initialize the audio device with a new sample rate. |

## Audio Handle API

| Function | Summary |
|----------|---------|
| `SituationLoadAudio` | Load audio and return a lightweight handle for playback control. |
| `SituationLoadSoundFromFile` | Load a sound from a file. |
| `SituationLoadSoundFromStream` | Load a sound from a custom stream. |
| `SituationPlayAudio` | Play audio by handle (restarts if already playing). |
| `SituationPlayLoadedSound` | Play a loaded sound (restarts if already playing). |
| `SituationSetAudioPan` | Set stereo pan for a handle-based sound [-1.0 to 1.0]. |
| `SituationSetAudioPitch` | Set pitch multiplier for a handle-based sound (1.0 = normal). |
| `SituationSetAudioVolume` | Set volume for a handle-based sound [0.0 to 1.0+]. |
| `SituationStopAllLoadedSounds` | Stop all currently playing sounds. |
| `SituationStopLoadedSound` | Stop a specific sound from playing. |
| `SituationUnloadAudio` | Unload audio by handle and free resources. |
| `SituationUnloadSound` | Unload a sound and free its resources. |

## Audio Output Monitoring (for visualization)

| Function | Summary |
|----------|---------|
| `SituationGetMasterOutputMeter` | Last playback callback block: peak sample magnitude & RMS (optional pointers; safe from main/UI thread). |
| `SituationSetAudioOutputMonitor` | Set a callback to receive mixed output samples (for VU meters, FFT, etc.). |

## Backend-Specific Accessors

| Function | Summary |
|----------|---------|
| `SituationGetGLFWwindow` | Get the raw GLFW window handle. |
| `SituationGetMainWindowRenderPass` | Get the render pass for the main window. |
| `SituationGetRendererType` | Legacy — prefer SituationGetGraphicsBackend() + SituationGetGraphicsCaps(). |
| `SituationGetVulkanDevice` | Get the raw Vulkan logical device handle. |
| `SituationGetVulkanInstance` | Get the raw Vulkan instance handle. |
| `SituationGetVulkanPhysicalDevice` | Get the raw Vulkan physical device handle. |

## Callbacks and Event Handling

| Function | Summary |
|----------|---------|
| `SituationErrorToString` | Human-readable base label for an error code (from the errno table). |
| `SituationGetLastErrorCode` | Get the SituationError enum from the most recent _SituationSetErrorFromCode call. |
| `SituationGetLastErrorMsg` | Get the last error message as a string (caller must free). |
| `SituationSetExitCallback` | Set a callback to run just before shutdown. |
| `SituationSetFileDropCallback` | Set a callback for file drop events. |
| `SituationSetFocusCallback` | Set a callback for window focus events. |
| `SituationSetMaximizeCallback` | Set a callback for window maximize / restore events. |
| `SituationSetResizeCallback` | Set a callback for window framebuffer resize events. |

## Camera & Projection Math

| Function | Summary |
|----------|---------|
| `SituationCameraBuildInvViewProj` | — |
| `SituationCameraBuildProj` | — |
| `SituationCameraBuildView` | — |
| `SituationCameraBuildViewProj` | — |
| `SituationCameraUnprojectPixel` | — |

## Color Space Conversions

| Function | Summary |
|----------|---------|
| `SituationConvertColorToVector4` | Convert an 8-bit ColorRGBA struct to a normalized Vector4. |
| `SituationHsvToRgb` | Converts a Hue, Saturation, Value color back to the standard RGBA color space. |
| `SituationRgbToHsv` | Converts a standard RGBA color to the Hue, Saturation, Value color space. |

## Command Buffer Recording

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBeginDebugGroup` | [command ref](situation_command_reference.md#11-debug-markers) | — |
| `SituationCmdEndDebugGroup` | [command ref](situation_command_reference.md#11-debug-markers) | — |
| `SituationCmdPopRasterState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdPopRendererBehavior` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | Restore policy saved by PushRendererBehavior. |
| `SituationCmdPushRasterState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdPushRendererBehavior` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | Push current policy; mutate with Set inside scope. |
| `SituationCmdSetBlendEnable` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetBlendFuncSeparate` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetColorWriteMask` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetCullMode` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthBias` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthTest` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthWrite` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetFrontFace` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetLineWidth` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetMultisampleState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetPolygonMode` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetPrimitiveTopology` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetPushConstantData` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | — |
| `SituationCmdSetRendererBehavior` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | Replace active renderer behavior policy. |
| `SituationCmdSetStencilTest` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationRendererBehaviorPolicyDefault` | — | Strict defaults on every axis (Phase 14). |

## Command-Line Argument Queries

| Function | Summary |
|----------|---------|
| `SituationGetArgumentValue` | Get the value of an argument (e.g., "jungle" from "-level:jungle"). |
| `SituationIsArgumentPresent` | Check if a command-line argument (e.g., "-server") was provided. |

## Compute Shader Pipeline

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBindComputePipeline` | [command ref](situation_command_reference.md#7-compute) | Bind a compute pipeline for a subsequent dispatch. |
| `SituationCmdBufferBarrier` | [command ref](situation_command_reference.md#7-compute) | Record an explicit buffer-range memory barrier. |
| `SituationCmdDispatch` | [command ref](situation_command_reference.md#7-compute) | Record a command to dispatch compute shader work groups. |
| `SituationCmdDispatchEx` | [command ref](situation_command_reference.md#7-compute) | Record a compute dispatch with validation and error reporting. |
| `SituationCmdDispatchIndirect` | [command ref](situation_command_reference.md#7-compute) | Record an indirect compute dispatch. |
| `SituationCmdPipelineBarrierEx` | [command ref](situation_command_reference.md#7-compute) | Record an explicit global memory barrier. |
| `SituationCmdTextureBarrier` | [command ref](situation_command_reference.md#7-compute) | Record an explicit texture layout/memory barrier. |
| `SituationCreateComputePipeline` | — | Create a compute pipeline from a shader file. |
| `SituationCreateComputePipelineFromMemory` | — | Create a compute pipeline from in-memory GLSL source. |
| `SituationDestroyComputePipeline` | — | Destroy a compute pipeline and free its GPU resources. |
| `SituationGetMaxComputeWorkGroups` | — | Query maximum compute work group count per dispatch. |

## CPU & Thread Management

| Function | Summary |
|----------|---------|
| `SituationAddJobDependencies` | Adds multiple dependencies for a single dependent job. |
| `SituationAddJobDependency` | Adds a dependency between two jobs (prereq -> dependent). |
| `SituationBuildNumaNodeMask` | All logical CPUs on a NUMA node |
| `SituationBuildPhysicalCoreMask` | All logical CPUs on one physical core |
| `SituationBuildUniqueCoreMask` | One LP per core |
| `SituationCreateThreadPool` | Initializes the thread pool with dual-priority queues and worker threads. |
| `SituationDestroyThreadPool` | Shuts down the thread pool and releases resources. |
| `SituationDispatchParallel` | Executes a loop in parallel across worker threads (Fork-Join). |
| `SituationDumpTaskGraph` | Prints the current task graph state to the stream. |
| `SituationDumpThreadPoolMetrics` | Metrics-only dump |
| `SituationDumpThreadPoolStatus` | Pool metrics + per-role CPU snapshot |
| `SituationDumpThreadingReport` | Status + topology line + pool dump |
| `SituationGetActiveJobCount` | active_jobs counter |
| `SituationGetCPUCoreCount` | Gets physical processors (Cores) from cached topology |
| `SituationGetCPUThreadCount` | Logical processor count (cached topology). |
| `SituationGetConfiguredAudioThreadAffinity` | Effective audio mask (init or default) |
| `SituationGetConfiguredIOThreadAffinity` | Effective I/O mask (init or default CPU 3) |
| `SituationGetConfiguredMainThreadAffinity` | Init mask for main thread (0 = no pin) |
| `SituationGetConfiguredRenderThreadAffinity` | Effective render mask (init or default) |
| `SituationGetCpuTopology` | Pointer to cached topology (NULL on failure) |
| `SituationGetCurrentProcessorIndex` | Logical CPU index for current thread, or -1 if unknown |
| `SituationGetHighQueueDepth` | High-priority queue depth |
| `SituationGetInternalThreadPool` | Returns pointer to the library's internal thread pool (NULL if not initialized). |
| `SituationGetNumaTopology` | Cached NUMA snapshot |
| `SituationGetPreferredNumaNode` | TLS: node for current thread, or -1 if unset |
| `SituationGetQueueDepth` | Pending jobs per queue mask |
| `SituationGetRecommendedWorkerCount` | Sizing helper (no pool required) |
| `SituationGetThreadAffinity` | Reads affinity mask for the CURRENT thread |
| `SituationGetThreadNumaNode` | NUMA node for current thread, or -1 if unknown |
| `SituationGetThreadPoolMetrics` | Scheduler counters snapshot |
| `SituationGetThreadPoolSnapshot` | Worker/I/O/render/audio placement snapshot |
| `SituationGetThreadingStatus` | Runtime threading capabilities + pool summary |
| `SituationLoadSoundFromFileAsync` | Asynchronously loads and decodes a sound file. |
| `SituationPrintThreadingStatus` | Human-readable threading status (stdout if NULL) |
| `SituationRefreshCpuTopology` | Rebuilds the process-wide topology cache |
| `SituationRefreshNumaTopology` | Rebuild NUMA summary from CPU topology + OS memory |
| `SituationResetThreadPoolStats` | Zero scheduler counters |
| `SituationSetCurrentThreadName` | OS-visible name for the calling thread (UTF-8); no-op if NULL/empty |
| `SituationSetThreadAffinity` | Pins the CURRENT thread (logical CPU bitmask, bits 0..63) |
| `SituationSetThreadAffinityEx` | Set affinity; optional previous mask |
| `SituationSubmitJobEx` | Submits a job with priority flags and optional data payload. |
| `SituationWaitForAllJobs` | Blocks until all queued jobs are finished. |
| `SituationWaitForJob` | Waits for a specific job to complete (O(1) check). |

## Cursor, Clipboard and File Drops

| Function | Summary |
|----------|---------|
| `SituationDisableCursor` | Hide and lock the cursor, providing raw mouse motion. |
| `SituationGetClipboardText` | Get text from the system clipboard. |
| `SituationHideCursor` | Hide the mouse cursor. |
| `SituationIsFileDropped` | Check if a file was dropped into the window this frame. |
| `SituationLoadDroppedFiles` | Get the paths of dropped files (returns a copy, caller must free). |
| `SituationSetClipboardText` | Set text in the system clipboard. |
| `SituationSetCursor` | Set the mouse cursor to a standard shape. |
| `SituationShowCursor` | Show the mouse cursor. |
| `SituationUnloadDroppedFiles` | Unload the file path list returned by SituationLoadDroppedFiles. |

## Custom Audio Processing

| Function | Summary |
|----------|---------|
| `SituationAttachAudioProcessor` | Attach a custom DSP processor to a sound's effect chain. |
| `SituationDetachAudioProcessor` | Detach a custom DSP processor from a sound. |

## Device Enumeration (Phase 0)

| Function | Summary |
|----------|---------|
| `SituationEnumerateAudioDevices` | [Caller frees via SituationFreeDeviceList] Canonical device enumeration. |
| `SituationFindBestDevice` | Find the best matching device by type and channel requirements. |
| `SituationFreeDeviceList` | Free a device list returned by SituationEnumerateAudioDevices. |

## Device Function Table (for processing)

| Function | Summary |
|----------|---------|
| `SituationFreeString` | Free a string allocated by the library (e.g., from path helpers). |

## Device Registry Functions

| Function | Summary |
|----------|---------|
| `SituationGetCategoryName` | Get the display name for a device category. |
| `SituationGetDeviceMetadata` | Get metadata for a registered device type. |
| `SituationGetRegisteredDeviceCount` | Get the number of registered audio device types. |
| `SituationInitDeviceRegistry` | Initialize the built-in device registry (call once at startup). |
| `SituationIsDeviceRegistered` | Check if a device type is registered. |
| `SituationRegisterDeviceType` | Register a custom device type with the registry. |

## Directory Operations

| Function | Summary |
|----------|---------|
| `SituationCreateDirectory` | Create a directory, optionally creating parent directories. |
| `SituationDeleteDirectory` | Delete a directory, optionally deleting all its contents. |
| `SituationFreeDirectoryFileList` | Free the memory allocated by SituationListDirectoryFiles. |
| `SituationListDirectoryFiles` | List files and subdirectories in a path (caller must free with SituationFreeDirectoryFileList). |

## File & Directory Queries

| Function | Summary |
|----------|---------|
| `SituationDirectoryExists` | Check if a directory exists at the given path. |
| `SituationFileExists` | Check if a file exists at the given path. |
| `SituationGetFileModTime` | Get the last modification time of a file (Unix timestamp). |

## File Operations

| Function | Summary |
|----------|---------|
| `SituationCopyFile` | Copy a file. |
| `SituationDeleteFile` | Delete a file. |
| `SituationLoadFileAsync` | Asynchronously load a file. |
| `SituationLoadFileData` | Load an entire file into a memory buffer (caller must free). |
| `SituationLoadFileText` | Load a text file into a null-terminated string (caller must free). |
| `SituationLoadFileTextAsync` | Asynchronously load a text file. |
| `SituationMoveFile` | Move/rename a file, even across drives on Windows. |
| `SituationRenameFile` | Alias for SituationMoveFile. |
| `SituationSaveFileAsync` | Asynchronously save a file. |
| `SituationSaveFileData` | Save a block of memory to a file. |
| `SituationSaveFileText` | Save a null-terminated string to a text file. |
| `SituationSaveFileTextAsync` | Asynchronously save a text file. |

## Font Management

| Function | Summary |
|----------|---------|
| `SituationBakeBitmapFontAtlas` | Upload bitmap_data as a NEAREST-filtered grid atlas for GPU text. |
| `SituationBakeFontAtlas` | Rasterize a font into a GPU-ready atlas at the given size. |
| `SituationCreateASCIIFont` | — |
| `SituationCreateCP437Font` | — |
| `SituationCreateOutlinedPackedBitmapFont` | — |
| `SituationCreatePackedBitmapFont` | — |
| `SituationCreateTerminalFontEx` | — |
| `SituationCreateTerminalFontFromMemory` | — |
| `SituationCreateVCRFont` | — |
| `SituationCreateVCRFontWithOutline` | — |
| `SituationCreateVGA8x8Font` | — |
| `SituationCreateVGA8x8FontWithOutline` | — |
| `SituationGetTextLineCount` | Lines required for width constraint (grid/TTF). |
| `SituationImageDrawCodepoint` | Draw a single Unicode character with advanced styling onto an image. |
| `SituationImageDrawText` | Draw a simple, tinted text string onto an image. |
| `SituationImageDrawTextEx` | Draw a text string with advanced styling (rotation, outline) onto an image. |
| `SituationImageDrawTextFormatted` | Draw printf-style formatted text onto an image. |
| `SituationImageStampText` | — |
| `SituationImageStampTextBoxed` | — |
| `SituationLoadBitmapFontFromMemory` | Loads a raw bitmap font (e.g. 8x8 array). |
| `SituationLoadBitmapFontFromTexture` | Grid atlas already on GPU — fills layout metadata. |
| `SituationLoadFont` | Load a font from a TTF/OTF file for CPU rendering. |
| `SituationLoadFontFromMemory` | Loads a font directly from a memory buffer (e.g., embedded resource). |
| `SituationMeasureText` | Measure the pixel dimensions of a string before drawing. |
| `SituationMeasureTextEx` | Measure with extra per-character spacing. |
| `SituationUnloadFont` | Frees CPU font data, glyph metrics, and owned GPU atlas (not the built-in default). |

## Frame Lifecycle & Command Buffer

| Function | Summary |
|----------|---------|
| `SituationAcquireFrameCommandBuffer` | [GL+VK] Prepare the backend for a new frame of rendering commands. |
| `SituationEndFrame` | [GL+VK] Submit all commands for the frame and present the result. |
| `SituationGetComputeCommandBuffer` | [VK] Get the compute-specific command buffer (Vulkan only). |
| `SituationGetMainCommandBuffer` | [GL+VK] Get the primary command buffer for the current frame. |
| `SituationReplayRenderList` | Replay a previously recorded render list into a command buffer. |
| `SituationResetRenderList` | Reset a render list for reuse next frame. |
| `SituationSubmitRenderList` | Submit a render list for async recording on a worker thread. |
| `SituationSubmitRenderList` | Submit a render list for immediate recording (single-threaded fallback). |

## Frame Timing & FPS Management

| Function | Summary |
|----------|---------|
| `SituationGetFPS` | Get the current frames-per-second value. |
| `SituationGetFrameTime` | Get the time in seconds for the last frame to complete (deltaTime). |
| `SituationSetTargetFPS` | Set a desired frame rate cap (0 for uncapped). |

## Gamepad Input

| Function | Summary |
|----------|---------|
| `SituationGetGamepadAxisCount` | Get the number of axes for a gamepad. |
| `SituationGetGamepadAxisValue` | Get the value of a gamepad axis (deadzone applied). |
| `SituationGetGamepadButtonPressed` | Get the next gamepad button from the press queue. |
| `SituationGetJoystickName` | Get the human-readable name of a joystick/gamepad. |
| `SituationIsFeatureSupported` | Check if a graphics feature is supported on current hardware. |
| `SituationIsGamepad` | Check if a connected joystick has a standard gamepad mapping. |
| `SituationIsGamepadButtonDown` | Check if a gamepad button is currently held down (a state). |
| `SituationIsGamepadButtonPressed` | Check if a gamepad button was pressed down this frame (an event). |
| `SituationIsGamepadButtonReleased` | Check if a gamepad button was released this frame (an event). |
| `SituationIsJoystickPresent` | Check if a joystick/gamepad is connected. |
| `SituationLog` | Log a message at the specified level (SIT_LOG_*). |
| `SituationLogWarning` | Log a warning with an associated error code (debug builds only). |
| `SituationSetGamepadMappings` | Load a new set of gamepad mappings from a string. |
| `SituationSetGamepadVibration` | Set gamepad vibration/rumble (Windows only). |
| `SituationSetJoystickCallback` | Set a callback for joystick connection events. |
| `SituationSetLogCallback` | Set a custom log callback. |
| `SituationSetTraceLogLevel` | Set the minimum log level for output filtering. |
| `SituationShowMessageBox` | Blocking UI message box; for fatal init errors. |

## GPU Buffer Management

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdCopyBuffer` | [command ref](situation_command_reference.md#8-transfer--presentation) | Legacy buffer-copy command (now returns error). |
| `SituationCmdCopyBufferEx` | [command ref](situation_command_reference.md#8-transfer--presentation) | Error-returning buffer-copy command with independent source/destination offsets. |
| `SituationCreateBuffer` | — | Create a generic GPU data buffer (e.g., SSBO). |
| `SituationCreateReadbackBuffer` | — | [Phase 1] Create an async GPU->CPU staging buffer. |
| `SituationDestroyBuffer` | — | Destroy a GPU buffer. |
| `SituationGetBufferData` | — | Read data from a GPU buffer (blocking). |
| `SituationReadBuffer` | — | Read mapped buffer data safely. |
| `SituationUpdateBuffer` | — | Update data in a GPU buffer. |

## Graph Serialization Functions

| Function | Summary |
|----------|---------|
| `SituationDeserializeGraphFromJSON` | Deserialize a graph from a JSON string. |
| `SituationFreeJSONString` | Free a JSON string returned by SituationSerializeGraphToJSON. |
| `SituationGetSerializationVersion` | Get the current serialization format version string. |
| `SituationIsVersionCompatible` | Check if a serialized version is compatible with this library. |
| `SituationLoadGraphFromFile` | Load a graph from a JSON file, re-creating nodes via device_funcs. |
| `SituationSaveGraphToFile` | Save a graph to a JSON file. |
| `SituationSerializeGraphToJSON` | Serialize a graph to a JSON string (caller must free with SituationFreeJSONString). |

## Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBeginRenderToDisplay` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdBindComputeBuffer` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdBindTexture` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdBindUniformBuffer` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationCmdEndRender` | [command ref](situation_command_reference.md#11-deprecated-commands) | — |
| `SituationLoadComputeShader` | — | — |
| `SituationLoadComputeShaderFromMemory` | — | — |
| `SituationMemoryBarrier` | — | — |

## Graphics Resource Management

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBindMeshPullBuffers` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [VK+GL] Push mesh vertex/index BDA block for pull shaders (SituationMeshPullPushConstants @ offset 0). Requires SIT_F... |
| `SituationCreateMesh` | — | Create a mesh from vertex and index data. |
| `SituationCreateMeshEx` | — | Create a mesh with an explicit vertex layout tag (includes SIT_MESH_LAYOUT_PULL). |
| `SituationDestroyMesh` | — | Unload a mesh from GPU memory. |
| `SituationGetBufferDeviceAddress` | — | Retrieves the GPU device address of a buffer for bindless access. |
| `SituationGetMeshIndexBufferAddress` | — | Retrieves the GPU device address of the mesh index buffer. [VK] requires SIT_FEATURE_BINDLESS_BUFFERS; [GL] NVIDIA-on... |
| `SituationGetMeshVertexBufferAddress` | — | GPU VA of mesh vertex buffer. [VK] SIT_FEATURE_BINDLESS_BUFFERS; pull draw: SituationCmdBindMeshPullBuffers + buffer_... |
| `SituationGetMeshVertexLayout` | — | Query layout tag stored at creation. |
| `SituationGetTextureHandle` | — | Retrieves the bindless texture handle (OpenGL Only). |

## Image & Screenshot Utilities

| Function | Summary |
|----------|---------|
| `SituationGetScreenshotFormat` | Get the current default screenshot format. |
| `SituationLoadImageFromScreen` | Get a copy of the current screen backbuffer as an image. |
| `SituationRequestScreenCapture` | — |
| `SituationSetScreenshotFormat` | Set the default screenshot file format (default: BMP). |
| `SituationTakeScreenshot` | Take a screenshot. fileName is the base name (no extension) or NULL for auto-name. Extension added from format setting. |

## Image Exporting

| Function | Summary |
|----------|---------|
| `SituationExportImage` | Export image data (.png, .bmp, .jpg, .tga, .hdr). |

## Image Generation & Copying

| Function | Summary |
|----------|---------|
| `SituationBlitRawDataToImage` | Copies raw byte data into a specific region of an image. |
| `SituationCreateImage` | Allocates a new SituationImage container with UNINITIALIZED data. |
| `SituationGenImageColor` | Generate a new image of a solid color. |
| `SituationGenImageGradient` | Generate a new image with a gradient. |
| `SituationImageCopy` | Create a new image by copying another. |
| `SituationImageDraw` | Copying portion of one image into another image at destination placement |
| `SituationImageDrawAlpha` | Draw a portion of a source image onto dst with alpha tinting. |
| `SituationSetPixelColor` | Helper to set a specific pixel color (CPU-side). |

## Image Loading and Unloading

| Function | Summary |
|----------|---------|
| `SituationIsImageValid` | Check if an image has been loaded successfully. |
| `SituationIsStbImageLoadExtension` | True for stb_image decode extensions (.jpg, .png, .bmp, .tga, .psd, .gif, .hdr, .pic, .ppm, .pgm, .pnm). |
| `SituationLoadImage` | Load an image via stb_image (JPEG, PNG, BMP, TGA, PSD, GIF, HDR, PIC, PNM). |
| `SituationLoadImageFromMemory` | Load an image from a memory buffer. |
| `SituationUnloadImage` | Unload an image's pixel data from memory. |

## Image Manipulation (Modifies image in-place)

| Function | Summary |
|----------|---------|
| `SituationImageAdjustHSV` | Control an image by Hue Saturation and Brightness. |
| `SituationImageAdjustYPQ` | Grade an image in YPQ (phase/chroma/luma). |
| `SituationImageCrop` | Crop an image to a specific rectangle. |
| `SituationImageFlip` | Flip an image. |
| `SituationImageResize` | Resize an image using default bicubic scaling. |

## Keyboard Input

| Function | Summary |
|----------|---------|
| `SituationConsumeKeyPress` | Eat a key press this frame so later IsKeyPressed() returns false (e.g. for global hotkeys to take priority over app c... |
| `SituationGetCharFromScancode` | Maps a physical key scancode (plus modifiers) to a Unicode character, respecting the current OS keyboard layout. |
| `SituationGetCharPressed` | Get the next character from the text input queue. |
| `SituationGetKeyPressed` | Get the next key from the press queue (no repeats). |
| `SituationGetKeyPressedEx` | Get the next key and its scancode from the queue. |
| `SituationGetKeyScancode` | Get the platform-specific scancode for a logical key. |
| `SituationIsKeyDown` | Check if a key is currently held down (a state). |
| `SituationIsKeyPressed` | Check if a key was pressed down this frame (an event). |
| `SituationIsKeyReleased` | Check if a key was released this frame (an event). |
| `SituationIsKeyUp` | Check if a key is currently up (a state). |
| `SituationIsLockKeyPressed` | Check if a lock key (Caps, Num) is currently active. |
| `SituationIsModifierPressed` | Check if a modifier key (Shift, Ctrl, Alt) is pressed. |
| `SituationIsScancodeDown` | Check if a physical key (scancode) is currently held down. |
| `SituationIsScrollLockOn` | Check if Scroll Lock is currently toggled on. |
| `SituationPeekKeyPressed` | Peek at the next key in the press queue without consuming it. |
| `SituationPeekKeyPressedEx` | Peek at the next key and its scancode. |
| `SituationSetKeyCallback` | Set a callback for key events. |

## Learning Operations

| Function | Summary |
|----------|---------|
| `SituationCancelMidiLearn` | Cancel an active learn operation. |
| `SituationIsLearning` | Check if currently in learn mode. Returns 1/0. |
| `SituationStartMidiLearn` | Start learning: next CC received maps to this param. Scaling: 0=linear, 1=log, 2=dB, 3=discrete. Times out after 5s. |

## Lifecycle

| Function | Summary |
|----------|---------|
| `SituationUpdate` | — |

## Mapping Management

| Function | Summary |
|----------|---------|
| `SituationClearAllMidiMappings` | Clear all learned mappings for a node. |
| `SituationClearMidiMapping` | Clear a specific learned CC mapping. |

## MIDI Device Control

| Function | Summary |
|----------|---------|
| `SituationAutoConnectMidi` | Convenience: auto-select first available MIDI input. Equivalent to EnableMidiControl(..., -1). |
| `SituationDisableMidiControl` | Disable MIDI control for a node. |
| `SituationEnableMidiControl` | Enable MIDI CC control for a node. Pass device_id=-1 for auto-select. |
| `SituationGetMidiDeviceName` | PortMidi device name for device_id (hardware or virtual). |
| `SituationIsMidiEnabled` | Check if a node has MIDI control enabled. Returns 1/0. |
| `SituationListMidiDevices` | List available MIDI input devices. Returns number found. |
| `SituationSetNodeMidiChannel` | Filter MIDI to channel 0-15, or -1 omni. |

## MIDI Learn Lifecycle

| Function | Summary |
|----------|---------|
| `SituationDisableMidiLearn` | Disable MIDI Learn for a node. |
| `SituationEnableMidiLearn` | Enable MIDI Learn capability for a node. MIDI must already be enabled. |
| `SituationIsMidiLearnEnabled` | Check if MIDI Learn is enabled. Returns 1/0. |

## Mouse Input

| Function | Summary |
|----------|---------|
| `SituationGetMouseDelta` | Get the mouse movement since the last frame. |
| `SituationGetMousePosition` | Get the mouse position within the window. |
| `SituationGetMouseWheelMove` | Get vertical mouse wheel movement. |
| `SituationGetMouseWheelMoveV` | Get vertical and horizontal mouse wheel movement. |
| `SituationIsMouseButtonDown` | Check if a mouse button is currently held down (a state). |
| `SituationIsMouseButtonPressed` | Check if a mouse button was pressed down this frame (an event). |
| `SituationIsMouseButtonReleased` | Check if a mouse button was released this frame. |
| `SituationSetCursorPosCallback` | Set a callback for mouse movement events. |
| `SituationSetMouseButtonCallback` | Set a callback for mouse button events. |
| `SituationSetMouseOffset` | Set a software offset for the mouse position. |
| `SituationSetMousePosition` | Set the mouse position within the window. |
| `SituationSetMouseScale` | Set a software scale for the mouse position and delta. |
| `SituationSetScrollCallback` | Set a callback for mouse scroll events. |

## Node Graph Functions

| Function | Summary |
|----------|---------|
| `SituationCreateGraph` | Create a new audio processing graph. |
| `SituationCreateNode` | Create a node of the given type in the graph. |
| `SituationCreatePatch` | Connect an output port to an input port. |
| `SituationDestroyGraph` | Destroy a graph and all its nodes/patches. |
| `SituationDestroyNode` | Remove and destroy a node from the graph. |
| `SituationDestroyPatch` | Disconnect a patch between two ports (legacy, no is_control param). |
| `SituationGetControl` | Get the current value of a node's control parameter. |
| `SituationGetNode` | Get a direct pointer to a node (for advanced use). |
| `SituationRemovePatch` | Disconnect a specific patch between two ports. |
| `SituationSetControl` | Set a control parameter on a node. |
| `SituationTopologicalSort` | Re-sort the graph processing order after topology changes. Call from the main thread after CreateNode/DestroyNode/Cre... |

## Node Graph SFX Routing (v2.6.5)

| Function | Summary |
|----------|---------|
| `SituationCheckHotReloads` | Checks all tracked resources for file changes and reloads them if necessary. |
| `SituationReloadComputePipeline` | Recompiles a compute pipeline from its original source file (Synchronous/Stalls GPU). |
| `SituationReloadModel` | Re-parses GLTF/GLB file and rebuilds all meshes and textures (Synchronous/Stalls GPU). |
| `SituationReloadShader` | Recompiles and links a shader from its original source files (Synchronous/Stalls GPU). |
| `SituationReloadTexture` | Re-reads image file and recreates the GPU texture resource (Synchronous/Stalls GPU). |
| `SituationSetGraphSFXSource` | Designate the Sound Source node in the active graph to receive routed SFX tones. |
| `SituationSetToneRouting` | Route a procedural tone to the active graph's SFX sound source. |

## OS Information (v2.4.199)

| Function | Summary |
|----------|---------|
| `SituationGetOSInfo` | Get operating system name, version, and build number. |

## Path Management & Special Directories

| Function | Summary |
|----------|---------|
| `SituationGetAppSavePath` | Get a safe, persistent path for saving application data (caller must free). |
| `SituationGetBasePath` | Get the path to the directory containing the executable (caller must free). |
| `SituationGetFileExtension` | Extract the file extension from a path. |
| `SituationGetFileName` | Extract the file name (including extension) from a full path. |
| `SituationJoinPath` | Join two path components with the correct OS separator (caller must free). |

## PCM Input Node (user-fed ring buffer source)

| Function | Summary |
|----------|---------|
| `SituationGetNodePCMFreeFrames` | Query how many frames of space are available in the PCM_INPUT node's ring buffer. |
| `SituationPushNodePCM` | Push interleaved float PCM into a PCM_INPUT node's ring buffer (any thread). Returns frames written. |

## Physical Display (Monitor) Management

| Function | Summary |
|----------|---------|
| `SituationFreeDisplays` | Free a display info array returned by SituationGetDisplays. |
| `SituationGetCurrentMonitor` | Get the index of the monitor the window is on. |
| `SituationGetDisplayRefreshRate` | Primary display refresh rate (monitor 0, integer Hz). |
| `SituationGetDisplayRefreshRateHz` | Primary display fractional nominal refresh (monitor 0). |
| `SituationGetDisplays` | Get information for all displays (caller must free). |
| `SituationGetMeasuredPresentRateHz` | Measured present rate from latest present interval (0 if unknown). |
| `SituationGetMonitorCount` | Get the number of connected monitors. |
| `SituationGetMonitorHeight` | Get the height of a monitor's current video mode. |
| `SituationGetMonitorName` | Get the human-readable name of a monitor. |
| `SituationGetMonitorPhysicalHeight` | Get the physical height of a monitor in millimeters. |
| `SituationGetMonitorPhysicalWidth` | Get the physical width of a monitor in millimeters. |
| `SituationGetMonitorPosition` | Get the top-left position of a monitor on the desktop. |
| `SituationGetMonitorRefreshRate` | Get the refresh rate of a monitor (integer Hz from OS/GLFW). |
| `SituationGetMonitorRefreshRateHz` | Fractional nominal refresh (DXGI rational when available). |
| `SituationGetMonitorWidth` | Get the width of a monitor's current video mode. |
| `SituationRefreshDisplays` | Force a refresh of the cached display information. |
| `SituationSetDisplayMode` | Set the display mode for a monitor. |
| `SituationSetWindowMonitor` | Set the window to be fullscreen on a specific monitor. |

## Preset Persistence

| Function | Summary |
|----------|---------|
| `SituationLoadMidiPreset` | Load MIDI Learn mappings from JSON file. |
| `SituationSaveMidiPreset` | Save MIDI Learn mappings to JSON file. |

## Process Enumeration (v2.4.199)

| Function | Summary |
|----------|---------|
| `SituationFreeProcessList` | Free a process list returned by SituationGetProcessList(). |
| `SituationGetProcessList` | Get snapshot of running OS processes. Caller must free with SituationFreeProcessList(). |

## Profiling & Diagnostics

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdGPUZoneBegin` | [command ref](situation_command_reference.md#9-gpu-profiling-zones-p103) | P10.3: GPU elapsed zone begin (no-op when unsupported) |
| `SituationCmdGPUZoneEnd` | [command ref](situation_command_reference.md#9-gpu-profiling-zones-p103) | P10.3: GPU elapsed zone end |
| `SituationDrawMetricsOverlay` | — | Draws FPS, Latency, and Memory stats |
| `SituationExportRenderHistogram` | — | Write a text-based frame time histogram into buf. |
| `SituationGetDrawCallCount` | — | Number of draw commands this frame |
| `SituationGetFrameProfile` | — | P10.1: single snapshot of frame pacing + phase metrics |
| `SituationGetFrameSpikeCount` | — | Count of detected frame spikes (general debugging aid) |
| `SituationGetIOQueueDepth` | — | [v2.3.37] Get the current depth of the IO/Low Priority queue |
| `SituationGetLastFramePhases` | — | — |
| `SituationGetMaxFrameTime` | — | Highest observed frame delta (general spike debugging) |
| `SituationGetRenderLatencyStats` | — | Get render thread latency metrics |
| `SituationGetRenderQueueDepth` | — | Get the current depth of the render queue |
| `SituationGetVRAMUsage` | — | Total GPU memory allocated (Bytes) |
| `SituationResetFrameProfileStats` | — | P10.1: reset spike counter, max frame time, histogram |

## Resonance (Procedural Synthesis)

| Function | Summary |
|----------|---------|
| `SituationPlayMidiNote` | Legacy: play a tone by MIDI note number (0-127). |
| `SituationPlayTone` | Legacy: play a simple ADSR tone (backward compat / quick UI sounds). |
| `SituationPlayToneEx` | @brief Plays an extended procedural tone with full control. @param type          Waveform type (Sine, Square, Triangl... |
| `SituationStopAllTones` | Stop all active tones (triggers release on each). |
| `SituationStopTone` | Gracefully stop a tone by triggering its release envelope. Invalid handles are ignored. |

## Shader Interaction & Synchronization

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationBindShaderStorageBlock` | — | [OpenGL] glShaderStorageBlockBinding for SPIR-V when reflection reports binding 0 for layout(binding=N). |
| `SituationBindUniformBlock` | — | [OpenGL] glUniformBlockBinding for std140 UBO blocks (layout(binding=N)). |
| `SituationCmdPipelineBarrier` | [command ref](situation_command_reference.md#7-compute) | Legacy convenience barrier; prefer SituationCmdPipelineBarrierEx, SituationCmdBufferBarrier, or SituationCmdTextureBa... |
| `SituationQueryShaderStorageBlocks` | — | [OpenGL] Enumerate all active SSBO blocks and their assigned binding points. out_count receives actual block count (m... |
| `SituationSetShaderUniform` | — | [OpenGL] Set a standalone uniform by name (location cache). While a frame is active, defers to SIT_OP_SET_UNIFORM. |
| `SituationSetShaderUniform1fv` | — | [OpenGL] Set float uniform array. |
| `SituationSetShaderUniform1iv` | — | [OpenGL] Set int uniform array in one call (e.g. name "uWallRows[0]", count=24). While a frame is active, records SIT... |
| `SituationSetShaderUniformLocation` | — | [OpenGL] Set uniform by explicit location (SPIR-V layout(location=)); defers during frames like SituationSetShaderUni... |
| `SituationSetShaderUniformMatrix4fv` | — | [OpenGL] Set mat4 uniform array. |
| `SituationValidateShaderUniforms` | — | Returns first missing/wrong-type uniform, or SUCCESS if all resolved. |

## Shader Management

| Function | Summary |
|----------|---------|
| `SituationBeginLoadShaderFromMemory` | Start non-blocking GLSL load: [OpenGL] async compile/link; [Vulkan] shaderc on worker thread, pipelines on next frame... |
| `SituationBeginLoadShaderFromSpirvMemory` | [Vulkan] Non-blocking pipeline build from in-memory SPIR-V (bytecode copied). [OpenGL] blocking SPIR-V load (unchanged). |
| `SituationBeginLoadShaderFromSpirvMemoryEx` | [Vulkan] Async SPIR-V with layout profile (e.g. UBO_SSBO for Demon Hunt). [OpenGL] profile ignored; same as Situation... |
| `SituationLoadShader` | Load a graphics shader pipeline from vertex and fragment files. |
| `SituationLoadShaderFromMemory` | Create a graphics shader pipeline from in-memory GLSL source. |
| `SituationLoadShaderFromSpirv` | Precompiled .spv: OpenGL via GL_ARB_gl_spirv; Vulkan same pipeline contract as SituationLoadShaderFromMemory (no shad... |
| `SituationLoadShaderFromSpirvMemory` | Same as SituationLoadShaderFromSpirv but from in-memory SPIR-V (e.g. build-time embedded .spv). No hot-reload paths (... |
| `SituationLoadShaderFromSpirvMemoryEx` | [Vulkan] `layout_profile` selects user SSBO/UBO layouts; [OpenGL] profile ignored. |
| `SituationPollShaderLoad` | SITUATION_SUCCESS when ready, SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS while compiling/linking/building pipelines. |
| `SituationUnloadShader` | Unload a graphics shader pipeline and free its GPU resources. |

## Sound Data Manipulation (Wave Utilities)

| Function | Summary |
|----------|---------|
| `SituationSoundCopy` | Create a new sound by copying the raw PCM data from a source. |
| `SituationSoundCrop` | Crop a sound's PCM data in-place to a new range. |
| `SituationSoundExportAsWav` | Export the sound's raw PCM data to a WAV file. |

## Sound Parameters and Effects

| Function | Summary |
|----------|---------|
| `SituationGetSoundPan` | Get the stereo pan of a sound. |
| `SituationGetSoundPitch` | Get the pitch of a sound. |
| `SituationGetSoundVolume` | Get the volume of a specific sound. |
| `SituationSetSoundEcho` | Apply an echo effect to a sound. |
| `SituationSetSoundFilter` | Apply a low-pass or high-pass filter to a sound. |
| `SituationSetSoundPan` | Set the stereo pan for a sound [-1.0 to 1.0]. |
| `SituationSetSoundPitch` | Set the pitch for a sound (resamples). |
| `SituationSetSoundReverb` | Apply a reverb effect to a sound. |
| `SituationSetSoundVolume` | Set the volume for a specific sound. |

## System

| Function | Summary |
|----------|---------|
| `SituationGetDeviceInfo` | — |

## System & Hardware Information (split queries; v2.4.207)

| Function | Summary |
|----------|---------|
| `SituationGetCPUInfo` | CPU name, core/thread counts, and clock speed. |
| `SituationGetGPUInfo` | GPU name and dedicated VRAM (when available). |
| `SituationGetGPUName` | Get the name of the active GPU. |
| `SituationGetInputDeviceCount` | Number of input devices (keyboard/mouse/gamepad). |
| `SituationGetInputDeviceName` | — |
| `SituationGetMemoryInfo` | Total and available physical RAM. |
| `SituationGetNetworkAdapterCount` | Number of network adapters reported by the OS. |
| `SituationGetNetworkAdapterName` | — |
| `SituationGetStorageDevice` | — |
| `SituationGetStorageDeviceCount` | Number of storage volumes reported by the OS. |

## Temporal Oscillator System

| Function | Summary |
|----------|---------|
| `SituationSetTimerOscillatorPeriod` | Set the period of an oscillator. |
| `SituationTimerGetOscillatorPeriod` | Get the period of an oscillator in seconds. |
| `SituationTimerGetOscillatorState` | Get the current binary state (0 or 1) of an oscillator. |
| `SituationTimerGetOscillatorTriggerCount` | Get the total number of times an oscillator has triggered. |
| `SituationTimerGetPingProgress` | Get progress [0.0 to 1.0+] of the interval since the last successful ping. |
| `SituationTimerGetPreviousOscillatorState` | Get the previous frame's state of an oscillator. |
| `SituationTimerGetTime` | Get the total time elapsed since initialization. |
| `SituationTimerHasOscillatorUpdated` | Check if an oscillator's state has changed this frame. |
| `SituationTimerPingOscillator` | Check if an oscillator's period has elapsed since the last ping. |

## Texture Management

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBlitTexture` | [command ref](situation_command_reference.md#8-transfer--presentation) | Blit between color 2D textures; caller owns explicit texture barriers. |
| `SituationCmdCopyBufferToTexture` | [command ref](situation_command_reference.md#8-transfer--presentation) | Upload tightly packed RGBA8 rows from a buffer into a texture subregion; caller owns texture barriers. |
| `SituationCmdCopyTexture` | [command ref](situation_command_reference.md#8-transfer--presentation) | Exact-size copy between color 2D textures; caller owns explicit texture barriers. |
| `SituationCmdCopyTextureToBuffer` | [command ref](situation_command_reference.md#8-transfer--presentation) | Copy a texture subregion into a buffer (`dst_row_pitch` 0 = width * 4); caller owns texture barriers. |
| `SituationCreateTexture` | — | Create a texture from a CPU-side image. |
| `SituationCreateTextureEx` | — | Create a texture with specific usage flags. |
| `SituationDestroyTexture` | — | Unload a texture from GPU memory. |
| `SituationGetTextureInfo` | — | [Phase 2] Query texture metadata. |
| `SituationLoadTexture` | — | Loads a texture from disk and registers the path for hot-reloading. |
| `SituationReadFramebuffer` | — | [Phase 2] Blocking readback of framebuffer pixels (RGBA8 or raw RGB10 packed). |
| `SituationReadFramebufferHdr` | — | [Phase 8] Raw A2R10G10B10 readback when HDR10 swapchain active. |
| `SituationReadTexture` | — | [Phase 2] Blocking readback of texture pixels. |
| `SituationReadTextureAlloc` | — | [Phase 2] Blocking readback into allocated SituationImage. |
| `SituationSetTextureSamplerParams` | — | [Phase 2] Update sampler state. |

## User query pools (P10.4 — timestamps + occlusion)

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBeginOcclusionQuery` | [command ref](situation_command_reference.md#10-user-query-pools-p104) | — |
| `SituationCmdEndOcclusionQuery` | [command ref](situation_command_reference.md#10-user-query-pools-p104) | — |
| `SituationCmdResetQueryPool` | [command ref](situation_command_reference.md#10-user-query-pools-p104) | — |
| `SituationCmdWriteTimestamp` | [command ref](situation_command_reference.md#10-user-query-pools-p104) | — |
| `SituationCreateQueryPool` | — | — |
| `SituationDestroyQueryPool` | — | — |
| `SituationGetQueryPoolResults` | — | — |

## User render targets (Phase 3c — offscreen without VD compositor)

| Function | Summary |
|----------|---------|
| `SituationCreateRenderTarget` | Create color (+ optional depth) offscreen target; msaa_samples must be 1. |
| `SituationDestroyRenderTarget` | Destroy RT and invalidate handle. |
| `SituationGetRenderTargetTexture` | Registry color texture for sampling / transfer readback. |
| `SituationReadRenderTarget` | Blocking RGBA8 readback of resolved color. |

## Virtual Displays (Render Targets)

| Function | Summary |
|----------|---------|
| `SituationConfigureVirtualDisplay` | Configure a virtual display's properties. |
| `SituationCreateVirtualDisplay` | Create an off-screen render target. |
| `SituationCreateVirtualDisplayEx` | Create a virtual display with extended flags (e.g. COMPUTE_TARGET for compute shader writes). |
| `SituationCreateVirtualDisplayFromDesc` | Create a virtual display from a full desc (VD-1 attachment config). |
| `SituationDestroyVirtualDisplay` | Destroy a virtual display. |
| `SituationGetLastVDCompositeTimeMS` | Get the time taken for the last virtual display composite pass. |
| `SituationGetVirtualDisplay` | Get a pointer to a virtual display's state. |
| `SituationGetVirtualDisplayChromaSnow` | Chroma snow flag on standby bitmask. |
| `SituationGetVirtualDisplayPatternConfig` | Copy current standby config (no-op if invalid id). |
| `SituationGetVirtualDisplayPatternLayers` | Current standby layer bitmask (0 if invalid id). |
| `SituationGetVirtualDisplaySize` | Get the internal resolution of a virtual display. |
| `SituationGetVirtualDisplayTexture` | Get the VD's internal texture as a SituationTexture handle (valid for compute-target VDs). |
| `SituationGetVirtualDisplayUpdateInfo` | Query last VD content write (not the frame clock). |
| `SituationIsVirtualDisplayDirty` | Check if a virtual display is marked as dirty. |
| `SituationRenderPassInfoInherit` | Fill pass struct from VD attachment defaults (tier C helper). |
| `SituationRenderVirtualDisplays` | Composite all visible virtual displays to the current target. |
| `SituationSetVirtualDisplayAttachmentDefaults` | Tier B storage-only attachment defaults. |
| `SituationSetVirtualDisplayChromaSnow` | Bit 16: RGB noise when idle snow (no layers 0–8). |
| `SituationSetVirtualDisplayClearColor` | Tier B: sets attachment_defaults.clear.color only. |
| `SituationSetVirtualDisplayDirty` | Mark a virtual display as needing to be re-rendered. |
| `SituationSetVirtualDisplayFallbackColor` | SOLID idle tint (normalized by compositor). |
| `SituationSetVirtualDisplayFallbackMode` | SOLID, COLORBURST, or PATTERN (create default: PATTERN + zero layers = snow). |
| `SituationSetVirtualDisplayIdleThreshold` | Set idle threshold for compositor fallback (Phase 2a). |
| `SituationSetVirtualDisplayMaxAnisotropy` | VD-4a aniso on composite sampler. |
| `SituationSetVirtualDisplayMemoryHint` | VD-5 stored hint (best-effort; no FBO rebuild). |
| `SituationSetVirtualDisplayMipLevels` | VD-4a mip LOD clamp; storage mips are create-time only. |
| `SituationSetVirtualDisplayPatternConfig` | Full standby tuning + PATTERN mode. |
| `SituationSetVirtualDisplayPatternLayers` | Toggle bitmask + PATTERN fallback (plan §3.4). |
| `SituationSetVirtualDisplaySampler` | VD-3 composite sampler (light rebuild). |
| `SituationSetVirtualDisplayScalingMode` | Layout rect only (v2.4.387+); filter via SetVirtualDisplaySampler. |
| `SituationSetVirtualDisplayUpdateMode` | VD-5 dynamic vs static (static: frame clock frozen, dirty-driven). |
| `SituationVdStandbyConfigInitDefaults` | RGL defaults + optional single layer bit. |
| `SituationVdStandbyLayerBit` | 1u << layer_index (0–8). |
| `SituationVdStandbyPackParamsStd430` | Layer params SSBO (P10). |
| `SituationVdStandbyPackStd140` | 160 B header UBO (P10). |
| `SituationVdStandbySetLayerOrder` | Replace compose stack. |
| `SituationVdStandbyToggleLayer` | Flip layer bit + sync stack. |

## Virtual MIDI loopback (integration testing; no hardware keyboard required)

| Function | Summary |
|----------|---------|
| `SituationSetupVirtualMidiLoopback` | Create connected virtual out→in pair. Returns input device_id for SituationEnableMidiControl(). |
| `SituationTeardownVirtualMidiLoopback` | Close and destroy the virtual loopback devices. |
| `SituationVirtualMidiControlChange` | CC (e.g. mod wheel, expression). |
| `SituationVirtualMidiNoteOff` | Inject note-off on channel 0 (legacy wrapper). |
| `SituationVirtualMidiNoteOffEx` | Channel-aware note-off (0-15). |
| `SituationVirtualMidiNoteOn` | Inject note-on on channel 0 (legacy wrapper). |
| `SituationVirtualMidiNoteOnEx` | Channel-aware note-on (0-15). |
| `SituationVirtualMidiPitchBend` | Pitch bend 0..16383 (center 8192). |
| `SituationVirtualMidiProgramChange` | Program change on channel 0-15. |

## Window & Screen Dimension Queries

| Function | Summary |
|----------|---------|
| `SituationGetRenderHeight` | Get the current render height (backbuffer size, considers HiDPI). |
| `SituationGetRenderWidth` | Get the current render width (backbuffer size, considers HiDPI). |
| `SituationGetScreenHeight` | Get the current logical height of the window. |
| `SituationGetScreenWidth` | Get the current logical width of the window. |
| `SituationGetWindowPosition` | Get the window's top-left position on the screen. |
| `SituationGetWindowScaleDPI` | Get the DPI scaling factor for the window. |
| `SituationGetWindowSize` | Get the current logical window size. |

## Window Property Management

| Function | Summary |
|----------|---------|
| `SituationSetWindowIcon` | Set the icon for the window (single image). |
| `SituationSetWindowIcons` | Set the icon for the window (multiple sizes). |
| `SituationSetWindowMaxSize` | Set the window maximum dimensions. |
| `SituationSetWindowMinSize` | Set the window minimum dimensions. |
| `SituationSetWindowOpacity` | Set window opacity [0.0f to 1.0f]. |
| `SituationSetWindowPosition` | Set the window position on the screen. |
| `SituationSetWindowSize` | Set the window dimensions. |
| `SituationSetWindowTitle` | Set the title for the window. |

## Window State Management

| Function | Summary |
|----------|---------|
| `SituationClearWindowState` | Clear window configuration state flags. |
| `SituationMaximizeWindow` | Maximize the window if it's resizable. |
| `SituationMinimizeWindow` | Minimize the window (iconify). |
| `SituationRestoreWindow` | Restore a minimized or maximized window. |
| `SituationSetVSync` | Enable or disable VSync (vertical synchronization). |
| `SituationSetWindowFocused` | Set the window to be focused. |
| `SituationSetWindowState` | Set window configuration state using flags (additive). |
| `SituationToggleBorderlessWindowed` | Toggle window between borderless and decorated mode. |
| `SituationToggleFullscreen` | Toggle window between fullscreen and windowed mode. |

## Window State Queries

| Function | Summary |
|----------|---------|
| `SituationHasWindowFocus` | Check if the window is currently focused. |
| `SituationIsWindowFullscreen` | Check if the window is currently in fullscreen mode. |
| `SituationIsWindowHidden` | Check if the window is currently hidden. |
| `SituationIsWindowMaximized` | Check if the window is currently maximized. |
| `SituationIsWindowMinimized` | Check if the window is currently minimized. |
| `SituationIsWindowResized` | Check if the window was resized in the last frame. |
| `SituationIsWindowState` | Check if a specific window state flag is set. |

## YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)

| Function | Summary |
|----------|---------|
| `SituationColorFromYPQ` | Converts a YPQA color back to the standard RGBA color space. |
| `SituationColorFromYPQf` | Float YPQ → RGBA (linear YIQ, clamped RGB). |
| `SituationColorRgbaToHdrPqClear` | sRGB 0–255 → PQ clear color as RGBA floats×255 for debugging. |
| `SituationColorToYPQ` | Converts a standard RGBA color to the YPQA (Luma, Phase, Quadrature) color space. |
| `SituationColorToYPQf` | RGBA → normalized float YPQ (no 8-bit quantize). |
| `SituationLinearToPq` | Linear display light [0,1] → ST.2084 PQ [0,1]. |
| `SituationPqGrayToRgb10Packed` | Uniform PQ gray → A2R10G10B10 packed pixel. |
| `SituationPqToLinear` | ST.2084 PQ [0,1] → linear display light [0,1]. |
| `SituationRgb10FromRgba` | Upscale 8-bit RGBA → 10-bit. |
| `SituationRgbToYpqFrom10` | 10-bit RGBA → float YPQ. |
| `SituationRgbaFromRgb10` | Downscale 10-bit RGBA → 8-bit. |
| `SituationRgbaFromRgb10Packed` | A2R10G10B10 texel → RGBA8 (readback parity). |
| `SituationYpqAdjustChroma` | Scale Q (chroma amplitude); preserve luma and phase. |
| `SituationYpqAdjustLuma` | Scale Y (luma); preserve phase and chroma. |
| `SituationYpqAdjustPhase` | Rotate hue; P shifts by byte steps mod 256. |
| `SituationYpqAnalyzeRgbMapping` | — |
| `SituationYpqClampInGamut` | Reduce chroma if linear RGB would clip. |
| `SituationYpqDistance` | Weighted distance in YPQ space. |
| `SituationYpqEquals` | Per-channel tolerance compare. |
| `SituationYpqGetChroma` | Normalized chroma amplitude [0, 1]. |
| `SituationYpqGetHueDegrees` | Hue in degrees [0, 360). |
| `SituationYpqGetLuma` | Normalized luma [0, 1]. |
| `SituationYpqLerp` | Interpolate YPQ; phase uses shortest arc on the hue wheel. |
| `SituationYpqQuantize` | Float YPQ → 8-bit ColorYPQA. |
| `SituationYpqSliceDuplicateCount` | — |
| `SituationYpqToRgb10Packed` | Float YPQ → A2R10G10B10 packed pixel (10-bit SDR). |
| `SituationYpqToRgb10PackedHdr` | Float YPQ → PQ-encoded A2R10G10B10 (HDR10 swapchain). |
| `SituationYpqToRgba10` | Float YPQ → 10-bit RGBA (linear YIQ, clamped). |

## Alphabetical (all)

- `SituationAcquireFrameCommandBuffer` — Frame Lifecycle & Command Buffer
- `SituationAddJobDependencies` — CPU & Thread Management
- `SituationAddJobDependency` — CPU & Thread Management
- `SituationApplyCurrentProfileWindowState` — Advanced Window Profile Management
- `SituationAttachAudioProcessor` — Custom Audio Processing
- `SituationAutoConnectMidi` — MIDI Device Control
- `SituationBakeBitmapFontAtlas` — Font Management
- `SituationBakeFontAtlas` — Font Management
- `SituationBeginLoadShaderFromMemory` — Shader Management
- `SituationBeginLoadShaderFromSpirvMemory` — Shader Management
- `SituationBeginLoadShaderFromSpirvMemoryEx` — Shader Management
- `SituationBindShaderStorageBlock` — Shader Interaction & Synchronization
- `SituationBindUniformBlock` — Shader Interaction & Synchronization
- `SituationBlitRawDataToImage` — Image Generation & Copying
- `SituationBuildNumaNodeMask` — CPU & Thread Management
- `SituationBuildPhysicalCoreMask` — CPU & Thread Management
- `SituationBuildUniqueCoreMask` — CPU & Thread Management
- `SituationCameraBuildInvViewProj` — Camera & Projection Math
- `SituationCameraBuildProj` — Camera & Projection Math
- `SituationCameraBuildView` — Camera & Projection Math
- `SituationCameraBuildViewProj` — Camera & Projection Math
- `SituationCameraUnprojectPixel` — Camera & Projection Math
- `SituationCancelMidiLearn` — Learning Operations
- `SituationCheckHotReloads` — Node Graph SFX Routing (v2.6.5)
- `SituationClearAllMidiMappings` — Mapping Management
- `SituationClearMidiMapping` — Mapping Management
- `SituationClearWindowState` — Window State Management
- `SituationCmdBeginDebugGroup` — Command Buffer Recording
- `SituationCmdBeginOcclusionQuery` — User query pools (P10.4 — timestamps + occlusion)
- `SituationCmdBeginRenderPass` — Abstracted Rendering Commands
- `SituationCmdBeginRenderToDisplay` — Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)
- `SituationCmdBindComputeBuffer` — Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)
- `SituationCmdBindComputePipeline` — Compute Shader Pipeline
- `SituationCmdBindComputeTexture` — Abstracted Rendering Commands
- `SituationCmdBindDescriptorSet` — Abstracted Rendering Commands
- `SituationCmdBindDescriptorSetDynamic` — Abstracted Rendering Commands
- `SituationCmdBindIndexBuffer` — Abstracted Rendering Commands
- `SituationCmdBindIndexBufferEx` — Abstracted Rendering Commands
- `SituationCmdBindMeshPullBuffers` — Graphics Resource Management
- `SituationCmdBindPipeline` — Abstracted Rendering Commands
- `SituationCmdBindSampledTexture` — Abstracted Rendering Commands
- `SituationCmdBindTexture` — Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)
- `SituationCmdBindTextureSet` — Abstracted Rendering Commands
- `SituationCmdBindUniformBuffer` — Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)
- `SituationCmdBindVertexBuffer` — Abstracted Rendering Commands
- `SituationCmdBlitTexture` — Texture Management
- `SituationCmdBufferBarrier` — Compute Shader Pipeline
- `SituationCmdClear` — Abstracted Rendering Commands
- `SituationCmdClearColor` — Abstracted Rendering Commands
- `SituationCmdClearDepth` — Abstracted Rendering Commands
- `SituationCmdClearDepthStencil` — Abstracted Rendering Commands
- `SituationCmdClearStencil` — Abstracted Rendering Commands
- `SituationCmdCopyBuffer` — GPU Buffer Management
- `SituationCmdCopyBufferEx` — GPU Buffer Management
- `SituationCmdCopyBufferToTexture` — Texture Management
- `SituationCmdCopyTexture` — Texture Management
- `SituationCmdCopyTextureToBuffer` — Texture Management
- `SituationCmdDispatch` — Compute Shader Pipeline
- `SituationCmdDispatchEx` — Compute Shader Pipeline
- `SituationCmdDispatchIndirect` — Compute Shader Pipeline
- `SituationCmdDraw` — Abstracted Rendering Commands
- `SituationCmdDrawIndexed` — Abstracted Rendering Commands
- `SituationCmdDrawIndexedIndirect` — Abstracted Rendering Commands
- `SituationCmdDrawIndirect` — Abstracted Rendering Commands
- `SituationCmdDrawMesh` — Abstracted Rendering Commands
- `SituationCmdDrawQuad` — Abstracted Rendering Commands
- `SituationCmdDrawText` — Abstracted Rendering Commands
- `SituationCmdDrawTextBoxed` — Abstracted Rendering Commands
- `SituationCmdDrawTextEx` — Abstracted Rendering Commands
- `SituationCmdDrawTexture` — Abstracted Rendering Commands
- `SituationCmdDrawTextureYpqGrade` — Abstracted Rendering Commands
- `SituationCmdEndDebugGroup` — Command Buffer Recording
- `SituationCmdEndOcclusionQuery` — User query pools (P10.4 — timestamps + occlusion)
- `SituationCmdEndRender` — Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)
- `SituationCmdEndRenderPass` — Abstracted Rendering Commands
- `SituationCmdGPUZoneBegin` — Profiling & Diagnostics
- `SituationCmdGPUZoneEnd` — Profiling & Diagnostics
- `SituationCmdPipelineBarrier` — Shader Interaction & Synchronization
- `SituationCmdPipelineBarrierEx` — Compute Shader Pipeline
- `SituationCmdPopRasterState` — Command Buffer Recording
- `SituationCmdPopRendererBehavior` — Command Buffer Recording
- `SituationCmdPresent` — Abstracted Rendering Commands
- `SituationCmdPushRasterState` — Command Buffer Recording
- `SituationCmdPushRendererBehavior` — Command Buffer Recording
- `SituationCmdResetQueryPool` — User query pools (P10.4 — timestamps + occlusion)
- `SituationCmdSetBlendEnable` — Command Buffer Recording
- `SituationCmdSetBlendFuncSeparate` — Command Buffer Recording
- `SituationCmdSetColorWriteMask` — Command Buffer Recording
- `SituationCmdSetCullMode` — Command Buffer Recording
- `SituationCmdSetDepthBias` — Command Buffer Recording
- `SituationCmdSetDepthTest` — Command Buffer Recording
- `SituationCmdSetDepthWrite` — Command Buffer Recording
- `SituationCmdSetFrontFace` — Command Buffer Recording
- `SituationCmdSetLineWidth` — Command Buffer Recording
- `SituationCmdSetMultisampleState` — Command Buffer Recording
- `SituationCmdSetPolygonMode` — Command Buffer Recording
- `SituationCmdSetPrimitiveTopology` — Command Buffer Recording
- `SituationCmdSetPushConstant` — Abstracted Rendering Commands
- `SituationCmdSetPushConstantData` — Command Buffer Recording
- `SituationCmdSetRendererBehavior` — Command Buffer Recording
- `SituationCmdSetScissor` — Abstracted Rendering Commands
- `SituationCmdSetScissorIndexed` — Abstracted Rendering Commands
- `SituationCmdSetStencilTest` — Command Buffer Recording
- `SituationCmdSetVertexAttribute` — Abstracted Rendering Commands
- `SituationCmdSetViewport` — Abstracted Rendering Commands
- `SituationCmdSetViewportIndexed` — Abstracted Rendering Commands
- `SituationCmdTextureBarrier` — Compute Shader Pipeline
- `SituationCmdWriteTimestamp` — User query pools (P10.4 — timestamps + occlusion)
- `SituationColorFromYPQ` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationColorFromYPQf` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationColorRgbaToHdrPqClear` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationColorToYPQ` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationColorToYPQf` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationConfigureVirtualDisplay` — Virtual Displays (Render Targets)
- `SituationConsumeKeyPress` — Keyboard Input
- `SituationConvertColorToVector4` — Color Space Conversions
- `SituationCopyFile` — File Operations
- `SituationCreateASCIIFont` — Font Management
- `SituationCreateBuffer` — GPU Buffer Management
- `SituationCreateCP437Font` — Font Management
- `SituationCreateComputePipeline` — Compute Shader Pipeline
- `SituationCreateComputePipelineFromMemory` — Compute Shader Pipeline
- `SituationCreateDirectory` — Directory Operations
- `SituationCreateGraph` — Node Graph Functions
- `SituationCreateImage` — Image Generation & Copying
- `SituationCreateMesh` — Graphics Resource Management
- `SituationCreateMeshEx` — Graphics Resource Management
- `SituationCreateNode` — Node Graph Functions
- `SituationCreateOutlinedPackedBitmapFont` — Font Management
- `SituationCreatePackedBitmapFont` — Font Management
- `SituationCreatePatch` — Node Graph Functions
- `SituationCreateQueryPool` — User query pools (P10.4 — timestamps + occlusion)
- `SituationCreateReadbackBuffer` — GPU Buffer Management
- `SituationCreateRenderTarget` — User render targets (Phase 3c — offscreen without VD compositor)
- `SituationCreateTerminalFontEx` — Font Management
- `SituationCreateTerminalFontFromMemory` — Font Management
- `SituationCreateTexture` — Texture Management
- `SituationCreateTextureEx` — Texture Management
- `SituationCreateThreadPool` — CPU & Thread Management
- `SituationCreateVCRFont` — Font Management
- `SituationCreateVCRFontWithOutline` — Font Management
- `SituationCreateVGA8x8Font` — Font Management
- `SituationCreateVGA8x8FontWithOutline` — Font Management
- `SituationCreateVirtualDisplay` — Virtual Displays (Render Targets)
- `SituationCreateVirtualDisplayEx` — Virtual Displays (Render Targets)
- `SituationCreateVirtualDisplayFromDesc` — Virtual Displays (Render Targets)
- `SituationDeleteDirectory` — Directory Operations
- `SituationDeleteFile` — File Operations
- `SituationDeserializeGraphFromJSON` — Graph Serialization Functions
- `SituationDestroyBuffer` — GPU Buffer Management
- `SituationDestroyComputePipeline` — Compute Shader Pipeline
- `SituationDestroyGraph` — Node Graph Functions
- `SituationDestroyMesh` — Graphics Resource Management
- `SituationDestroyNode` — Node Graph Functions
- `SituationDestroyPatch` — Node Graph Functions
- `SituationDestroyQueryPool` — User query pools (P10.4 — timestamps + occlusion)
- `SituationDestroyRenderTarget` — User render targets (Phase 3c — offscreen without VD compositor)
- `SituationDestroyTexture` — Texture Management
- `SituationDestroyThreadPool` — CPU & Thread Management
- `SituationDestroyVirtualDisplay` — Virtual Displays (Render Targets)
- `SituationDetachAudioProcessor` — Custom Audio Processing
- `SituationDirectoryExists` — File & Directory Queries
- `SituationDisableCursor` — Cursor, Clipboard and File Drops
- `SituationDisableMidiControl` — MIDI Device Control
- `SituationDisableMidiLearn` — MIDI Learn Lifecycle
- `SituationDispatchParallel` — CPU & Thread Management
- `SituationDrawMetricsOverlay` — Profiling & Diagnostics
- `SituationDrawModel` — 3D Model Utilities
- `SituationDumpTaskGraph` — CPU & Thread Management
- `SituationDumpThreadPoolMetrics` — CPU & Thread Management
- `SituationDumpThreadPoolStatus` — CPU & Thread Management
- `SituationDumpThreadingReport` — CPU & Thread Management
- `SituationEnableMidiControl` — MIDI Device Control
- `SituationEnableMidiLearn` — MIDI Learn Lifecycle
- `SituationEndFrame` — Frame Lifecycle & Command Buffer
- `SituationEnumerateAudioDevices` — Device Enumeration (Phase 0)
- `SituationErrorToString` — Callbacks and Event Handling
- `SituationExecuteCommand` — Active Audio Device Query (v2.4.199)
- `SituationExportImage` — Image Exporting
- `SituationExportRenderHistogram` — Profiling & Diagnostics
- `SituationFileExists` — File & Directory Queries
- `SituationFindBestDevice` — Device Enumeration (Phase 0)
- `SituationFreeDeviceList` — Device Enumeration (Phase 0)
- `SituationFreeDirectoryFileList` — Directory Operations
- `SituationFreeDisplays` — Physical Display (Monitor) Management
- `SituationFreeJSONString` — Graph Serialization Functions
- `SituationFreeProcessList` — Process Enumeration (v2.4.199)
- `SituationFreeString` — Device Function Table (for processing)
- `SituationGenImageColor` — Image Generation & Copying
- `SituationGenImageGradient` — Image Generation & Copying
- `SituationGetActiveAudioDeviceName` — Active Audio Device Query (v2.4.199)
- `SituationGetActiveGraph` — Active Graph (Audio Callback Integration)
- `SituationGetActiveJobCount` — CPU & Thread Management
- `SituationGetAppSavePath` — Path Management & Special Directories
- `SituationGetArgumentValue` — Command-Line Argument Queries
- `SituationGetAudioDevices` — Audio
- `SituationGetAudioMasterVolume` — Audio Device Management
- `SituationGetAudioPlaybackSampleRate` — Audio Device Management
- `SituationGetBasePath` — Path Management & Special Directories
- `SituationGetBufferData` — GPU Buffer Management
- `SituationGetBufferDeviceAddress` — Graphics Resource Management
- `SituationGetCPUCoreCount` — CPU & Thread Management
- `SituationGetCPUInfo` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetCPUThreadCount` — CPU & Thread Management
- `SituationGetCategoryName` — Device Registry Functions
- `SituationGetCharFromScancode` — Keyboard Input
- `SituationGetCharPressed` — Keyboard Input
- `SituationGetClipboardText` — Cursor, Clipboard and File Drops
- `SituationGetComputeCommandBuffer` — Frame Lifecycle & Command Buffer
- `SituationGetConfiguredAudioThreadAffinity` — CPU & Thread Management
- `SituationGetConfiguredIOThreadAffinity` — CPU & Thread Management
- `SituationGetConfiguredMainThreadAffinity` — CPU & Thread Management
- `SituationGetConfiguredRenderThreadAffinity` — CPU & Thread Management
- `SituationGetControl` — Node Graph Functions
- `SituationGetCpuTopology` — CPU & Thread Management
- `SituationGetCurrentActualWindowStateFlags` — Advanced Window Profile Management
- `SituationGetCurrentDriveLetter` — Active Audio Device Query (v2.4.199)
- `SituationGetCurrentMonitor` — Physical Display (Monitor) Management
- `SituationGetCurrentProcessorIndex` — CPU & Thread Management
- `SituationGetDeviceInfo` — System
- `SituationGetDeviceMetadata` — Device Registry Functions
- `SituationGetDisplayRefreshRate` — Physical Display (Monitor) Management
- `SituationGetDisplayRefreshRateHz` — Physical Display (Monitor) Management
- `SituationGetDisplays` — Physical Display (Monitor) Management
- `SituationGetDrawCallCount` — Profiling & Diagnostics
- `SituationGetDriveInfo` — Active Audio Device Query (v2.4.199)
- `SituationGetFPS` — Frame Timing & FPS Management
- `SituationGetFileExtension` — Path Management & Special Directories
- `SituationGetFileModTime` — File & Directory Queries
- `SituationGetFileName` — Path Management & Special Directories
- `SituationGetFrameProfile` — Profiling & Diagnostics
- `SituationGetFrameSpikeCount` — Profiling & Diagnostics
- `SituationGetFrameTime` — Frame Timing & FPS Management
- `SituationGetGLFWwindow` — Backend-Specific Accessors
- `SituationGetGPUInfo` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetGPUName` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetGamepadAxisCount` — Gamepad Input
- `SituationGetGamepadAxisValue` — Gamepad Input
- `SituationGetGamepadButtonPressed` — Gamepad Input
- `SituationGetGraphicsBackend` — Active Audio Device Query (v2.4.199)
- `SituationGetGraphicsBackendName` — Active Audio Device Query (v2.4.199)
- `SituationGetGraphicsCaps` — Active Audio Device Query (v2.4.199)
- `SituationGetHighQueueDepth` — CPU & Thread Management
- `SituationGetIOQueueDepth` — Profiling & Diagnostics
- `SituationGetInitState` — Application Lifecycle & State
- `SituationGetInputDeviceCount` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetInputDeviceName` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetInternalThreadPool` — CPU & Thread Management
- `SituationGetJoystickName` — Gamepad Input
- `SituationGetKeyPressed` — Keyboard Input
- `SituationGetKeyPressedEx` — Keyboard Input
- `SituationGetKeyScancode` — Keyboard Input
- `SituationGetLastErrorCode` — Callbacks and Event Handling
- `SituationGetLastErrorMsg` — Callbacks and Event Handling
- `SituationGetLastFramePhases` — Profiling & Diagnostics
- `SituationGetLastVDCompositeTimeMS` — Virtual Displays (Render Targets)
- `SituationGetMainCommandBuffer` — Frame Lifecycle & Command Buffer
- `SituationGetMainWindowRenderPass` — Backend-Specific Accessors
- `SituationGetMasterOutputMeter` — Audio Output Monitoring (for visualization)
- `SituationGetMaxComputeWorkGroups` — Compute Shader Pipeline
- `SituationGetMaxFrameTime` — Profiling & Diagnostics
- `SituationGetMeasuredPresentRateHz` — Physical Display (Monitor) Management
- `SituationGetMemoryInfo` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetMeshData` — 3D Model Utilities
- `SituationGetMeshIndexBufferAddress` — Graphics Resource Management
- `SituationGetMeshVertexBufferAddress` — Graphics Resource Management
- `SituationGetMeshVertexLayout` — Graphics Resource Management
- `SituationGetMidiDeviceName` — MIDI Device Control
- `SituationGetMonitorCount` — Physical Display (Monitor) Management
- `SituationGetMonitorHeight` — Physical Display (Monitor) Management
- `SituationGetMonitorName` — Physical Display (Monitor) Management
- `SituationGetMonitorPhysicalHeight` — Physical Display (Monitor) Management
- `SituationGetMonitorPhysicalWidth` — Physical Display (Monitor) Management
- `SituationGetMonitorPosition` — Physical Display (Monitor) Management
- `SituationGetMonitorRefreshRate` — Physical Display (Monitor) Management
- `SituationGetMonitorRefreshRateHz` — Physical Display (Monitor) Management
- `SituationGetMonitorWidth` — Physical Display (Monitor) Management
- `SituationGetMouseDelta` — Mouse Input
- `SituationGetMousePosition` — Mouse Input
- `SituationGetMouseWheelMove` — Mouse Input
- `SituationGetMouseWheelMoveV` — Mouse Input
- `SituationGetNetworkAdapterCount` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetNetworkAdapterName` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetNode` — Node Graph Functions
- `SituationGetNodePCMFreeFrames` — PCM Input Node (user-fed ring buffer source)
- `SituationGetNumaTopology` — CPU & Thread Management
- `SituationGetOSInfo` — OS Information (v2.4.199)
- `SituationGetPreferredNumaNode` — CPU & Thread Management
- `SituationGetProcessList` — Process Enumeration (v2.4.199)
- `SituationGetQueryPoolResults` — User query pools (P10.4 — timestamps + occlusion)
- `SituationGetQueueDepth` — CPU & Thread Management
- `SituationGetRecommendedWorkerCount` — CPU & Thread Management
- `SituationGetRegisteredDeviceCount` — Device Registry Functions
- `SituationGetRenderHeight` — Window & Screen Dimension Queries
- `SituationGetRenderLatencyStats` — Profiling & Diagnostics
- `SituationGetRenderQueueDepth` — Profiling & Diagnostics
- `SituationGetRenderTargetTexture` — User render targets (Phase 3c — offscreen without VD compositor)
- `SituationGetRenderWidth` — Window & Screen Dimension Queries
- `SituationGetRendererType` — Backend-Specific Accessors
- `SituationGetScreenHeight` — Window & Screen Dimension Queries
- `SituationGetScreenWidth` — Window & Screen Dimension Queries
- `SituationGetScreenshotFormat` — Image & Screenshot Utilities
- `SituationGetSerializationVersion` — Graph Serialization Functions
- `SituationGetSoundPan` — Sound Parameters and Effects
- `SituationGetSoundPitch` — Sound Parameters and Effects
- `SituationGetSoundVolume` — Sound Parameters and Effects
- `SituationGetStorageDevice` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetStorageDeviceCount` — System & Hardware Information (split queries; v2.4.207)
- `SituationGetTextLineCount` — Font Management
- `SituationGetTextureHandle` — Graphics Resource Management
- `SituationGetTextureInfo` — Texture Management
- `SituationGetThreadAffinity` — CPU & Thread Management
- `SituationGetThreadNumaNode` — CPU & Thread Management
- `SituationGetThreadPoolMetrics` — CPU & Thread Management
- `SituationGetThreadPoolSnapshot` — CPU & Thread Management
- `SituationGetThreadingStatus` — CPU & Thread Management
- `SituationGetUserDirectory` — Active Audio Device Query (v2.4.199)
- `SituationGetVRAMUsage` — Profiling & Diagnostics
- `SituationGetVersionString` — Application Lifecycle & State
- `SituationGetVirtualDisplay` — Virtual Displays (Render Targets)
- `SituationGetVirtualDisplayChromaSnow` — Virtual Displays (Render Targets)
- `SituationGetVirtualDisplayPatternConfig` — Virtual Displays (Render Targets)
- `SituationGetVirtualDisplayPatternLayers` — Virtual Displays (Render Targets)
- `SituationGetVirtualDisplaySize` — Virtual Displays (Render Targets)
- `SituationGetVirtualDisplayTexture` — Virtual Displays (Render Targets)
- `SituationGetVirtualDisplayUpdateInfo` — Virtual Displays (Render Targets)
- `SituationGetVulkanDevice` — Backend-Specific Accessors
- `SituationGetVulkanInstance` — Backend-Specific Accessors
- `SituationGetVulkanPhysicalDevice` — Backend-Specific Accessors
- `SituationGetWindowPosition` — Window & Screen Dimension Queries
- `SituationGetWindowScaleDPI` — Window & Screen Dimension Queries
- `SituationGetWindowSize` — Window & Screen Dimension Queries
- `SituationHasWindowFocus` — Window State Queries
- `SituationHideCursor` — Cursor, Clipboard and File Drops
- `SituationHsvToRgb` — Color Space Conversions
- `SituationImageAdjustHSV` — Image Manipulation (Modifies image in-place)
- `SituationImageAdjustYPQ` — Image Manipulation (Modifies image in-place)
- `SituationImageCopy` — Image Generation & Copying
- `SituationImageCrop` — Image Manipulation (Modifies image in-place)
- `SituationImageDraw` — Image Generation & Copying
- `SituationImageDrawAlpha` — Image Generation & Copying
- `SituationImageDrawCodepoint` — Font Management
- `SituationImageDrawText` — Font Management
- `SituationImageDrawTextEx` — Font Management
- `SituationImageDrawTextFormatted` — Font Management
- `SituationImageFlip` — Image Manipulation (Modifies image in-place)
- `SituationImageResize` — Image Manipulation (Modifies image in-place)
- `SituationImageStampText` — Font Management
- `SituationImageStampTextBoxed` — Font Management
- `SituationInit` — Application Lifecycle & State
- `SituationInitDeviceRegistry` — Device Registry Functions
- `SituationIsAppPaused` — Application Lifecycle & State
- `SituationIsArgumentPresent` — Command-Line Argument Queries
- `SituationIsAudioDevicePlaying` — Audio Device Management
- `SituationIsDeviceRegistered` — Device Registry Functions
- `SituationIsFeatureSupported` — Gamepad Input
- `SituationIsFileDropped` — Cursor, Clipboard and File Drops
- `SituationIsGamepad` — Gamepad Input
- `SituationIsGamepadButtonDown` — Gamepad Input
- `SituationIsGamepadButtonPressed` — Gamepad Input
- `SituationIsGamepadButtonReleased` — Gamepad Input
- `SituationIsImageValid` — Image Loading and Unloading
- `SituationIsInitialized` — Application Lifecycle & State
- `SituationIsJoystickPresent` — Gamepad Input
- `SituationIsKeyDown` — Keyboard Input
- `SituationIsKeyPressed` — Keyboard Input
- `SituationIsKeyReleased` — Keyboard Input
- `SituationIsKeyUp` — Keyboard Input
- `SituationIsLearning` — Learning Operations
- `SituationIsLockKeyPressed` — Keyboard Input
- `SituationIsMidiEnabled` — MIDI Device Control
- `SituationIsMidiLearnEnabled` — MIDI Learn Lifecycle
- `SituationIsModifierPressed` — Keyboard Input
- `SituationIsMouseButtonDown` — Mouse Input
- `SituationIsMouseButtonPressed` — Mouse Input
- `SituationIsMouseButtonReleased` — Mouse Input
- `SituationIsScancodeDown` — Keyboard Input
- `SituationIsScrollLockOn` — Keyboard Input
- `SituationIsStbImageLoadExtension` — Image Loading and Unloading
- `SituationIsVersionCompatible` — Graph Serialization Functions
- `SituationIsVirtualDisplayDirty` — Virtual Displays (Render Targets)
- `SituationIsWindowFullscreen` — Window State Queries
- `SituationIsWindowHidden` — Window State Queries
- `SituationIsWindowMaximized` — Window State Queries
- `SituationIsWindowMinimized` — Window State Queries
- `SituationIsWindowResized` — Window State Queries
- `SituationIsWindowState` — Window State Queries
- `SituationJoinPath` — Path Management & Special Directories
- `SituationLinearToPq` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationListDirectoryFiles` — Directory Operations
- `SituationListMidiDevices` — MIDI Device Control
- `SituationLoadAudio` — Audio Handle API
- `SituationLoadBitmapFontFromMemory` — Font Management
- `SituationLoadBitmapFontFromTexture` — Font Management
- `SituationLoadComputeShader` — Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)
- `SituationLoadComputeShaderFromMemory` — Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)
- `SituationLoadDroppedFiles` — Cursor, Clipboard and File Drops
- `SituationLoadFileAsync` — File Operations
- `SituationLoadFileData` — File Operations
- `SituationLoadFileText` — File Operations
- `SituationLoadFileTextAsync` — File Operations
- `SituationLoadFont` — Font Management
- `SituationLoadFontFromMemory` — Font Management
- `SituationLoadGraphFromFile` — Graph Serialization Functions
- `SituationLoadImage` — Image Loading and Unloading
- `SituationLoadImageFromMemory` — Image Loading and Unloading
- `SituationLoadImageFromScreen` — Image & Screenshot Utilities
- `SituationLoadMidiPreset` — Preset Persistence
- `SituationLoadModel` — 3D Model Utilities
- `SituationLoadModelFromOBJ` — 3D Model Utilities
- `SituationLoadModelFromSTL` — 3D Model Utilities
- `SituationLoadShader` — Shader Management
- `SituationLoadShaderFromMemory` — Shader Management
- `SituationLoadShaderFromSpirv` — Shader Management
- `SituationLoadShaderFromSpirvMemory` — Shader Management
- `SituationLoadShaderFromSpirvMemoryEx` — Shader Management
- `SituationLoadSoundFromFile` — Audio Handle API
- `SituationLoadSoundFromFileAsync` — CPU & Thread Management
- `SituationLoadSoundFromStream` — Audio Handle API
- `SituationLoadTexture` — Texture Management
- `SituationLog` — Gamepad Input
- `SituationLogWarning` — Gamepad Input
- `SituationMaximizeWindow` — Window State Management
- `SituationMeasureText` — Font Management
- `SituationMeasureTextEx` — Font Management
- `SituationMemoryBarrier` — Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers)
- `SituationMinimizeWindow` — Window State Management
- `SituationMoveFile` — File Operations
- `SituationOpenFile` — Active Audio Device Query (v2.4.199)
- `SituationPauseApp` — Application Lifecycle & State
- `SituationPauseAudioDevice` — Audio Device Management
- `SituationPeekKeyPressed` — Keyboard Input
- `SituationPeekKeyPressedEx` — Keyboard Input
- `SituationPlayAudio` — Audio Handle API
- `SituationPlayLoadedSound` — Audio Handle API
- `SituationPlayMidiNote` — Resonance (Procedural Synthesis)
- `SituationPlayTone` — Resonance (Procedural Synthesis)
- `SituationPlayToneEx` — Resonance (Procedural Synthesis)
- `SituationPollInputEvents` — Application Lifecycle & State
- `SituationPollShaderLoad` — Shader Management
- `SituationPqGrayToRgb10Packed` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationPqToLinear` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationPrintThreadingStatus` — CPU & Thread Management
- `SituationPushNodePCM` — PCM Input Node (user-fed ring buffer source)
- `SituationQueryShaderStorageBlocks` — Shader Interaction & Synchronization
- `SituationReadBuffer` — GPU Buffer Management
- `SituationReadFramebuffer` — Texture Management
- `SituationReadFramebufferHdr` — Texture Management
- `SituationReadRenderTarget` — User render targets (Phase 3c — offscreen without VD compositor)
- `SituationReadTexture` — Texture Management
- `SituationReadTextureAlloc` — Texture Management
- `SituationRefreshCpuTopology` — CPU & Thread Management
- `SituationRefreshDisplays` — Physical Display (Monitor) Management
- `SituationRefreshNumaTopology` — CPU & Thread Management
- `SituationRegisterDeviceType` — Device Registry Functions
- `SituationReloadComputePipeline` — Node Graph SFX Routing (v2.6.5)
- `SituationReloadModel` — Node Graph SFX Routing (v2.6.5)
- `SituationReloadShader` — Node Graph SFX Routing (v2.6.5)
- `SituationReloadTexture` — Node Graph SFX Routing (v2.6.5)
- `SituationRemovePatch` — Node Graph Functions
- `SituationRenameFile` — File Operations
- `SituationRenderPassConfigurationKey` — Abstracted Rendering Commands
- `SituationRenderPassInfoDefault` — Abstracted Rendering Commands
- `SituationRenderPassInfoForRenderTarget` — Abstracted Rendering Commands
- `SituationRenderPassInfoInherit` — Virtual Displays (Render Targets)
- `SituationRenderPassInfoLoad` — Abstracted Rendering Commands
- `SituationRenderVirtualDisplays` — Virtual Displays (Render Targets)
- `SituationRendererBehaviorPolicyDefault` — Command Buffer Recording
- `SituationReplayRenderList` — Frame Lifecycle & Command Buffer
- `SituationRequestScreenCapture` — Image & Screenshot Utilities
- `SituationResetFrameProfileStats` — Profiling & Diagnostics
- `SituationResetRenderList` — Frame Lifecycle & Command Buffer
- `SituationResetThreadPoolStats` — CPU & Thread Management
- `SituationRestoreWindow` — Window State Management
- `SituationResumeApp` — Application Lifecycle & State
- `SituationResumeAudioDevice` — Audio Device Management
- `SituationRgb10FromRgba` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationRgbToHsv` — Color Space Conversions
- `SituationRgbToYpqFrom10` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationRgbaFromRgb10` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationRgbaFromRgb10Packed` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationSaveFileAsync` — File Operations
- `SituationSaveFileData` — File Operations
- `SituationSaveFileText` — File Operations
- `SituationSaveFileTextAsync` — File Operations
- `SituationSaveGraphToFile` — Graph Serialization Functions
- `SituationSaveMidiPreset` — Preset Persistence
- `SituationSaveModelAsGltf` — 3D Model Utilities
- `SituationSerializeGraphToJSON` — Graph Serialization Functions
- `SituationSetActiveGraph` — Active Graph (Audio Callback Integration)
- `SituationSetAudioDevice` — Audio Device Management
- `SituationSetAudioMasterVolume` — Audio Device Management
- `SituationSetAudioOutputMonitor` — Audio Output Monitoring (for visualization)
- `SituationSetAudioPan` — Audio Handle API
- `SituationSetAudioPitch` — Audio Handle API
- `SituationSetAudioPlaybackSampleRate` — Audio Device Management
- `SituationSetAudioVolume` — Audio Handle API
- `SituationSetClipboardText` — Cursor, Clipboard and File Drops
- `SituationSetControl` — Node Graph Functions
- `SituationSetCurrentThreadName` — CPU & Thread Management
- `SituationSetCursor` — Cursor, Clipboard and File Drops
- `SituationSetCursorPosCallback` — Mouse Input
- `SituationSetDisplayMode` — Physical Display (Monitor) Management
- `SituationSetExitCallback` — Callbacks and Event Handling
- `SituationSetFileDropCallback` — Callbacks and Event Handling
- `SituationSetFocusCallback` — Callbacks and Event Handling
- `SituationSetGamepadMappings` — Gamepad Input
- `SituationSetGamepadVibration` — Gamepad Input
- `SituationSetGraphSFXSource` — Node Graph SFX Routing (v2.6.5)
- `SituationSetJoystickCallback` — Gamepad Input
- `SituationSetKeyCallback` — Keyboard Input
- `SituationSetLogCallback` — Gamepad Input
- `SituationSetMaximizeCallback` — Callbacks and Event Handling
- `SituationSetMouseButtonCallback` — Mouse Input
- `SituationSetMouseOffset` — Mouse Input
- `SituationSetMousePosition` — Mouse Input
- `SituationSetMouseScale` — Mouse Input
- `SituationSetNodeMidiChannel` — MIDI Device Control
- `SituationSetPixelColor` — Image Generation & Copying
- `SituationSetResizeCallback` — Callbacks and Event Handling
- `SituationSetScreenshotFormat` — Image & Screenshot Utilities
- `SituationSetScrollCallback` — Mouse Input
- `SituationSetShaderUniform` — Shader Interaction & Synchronization
- `SituationSetShaderUniform1fv` — Shader Interaction & Synchronization
- `SituationSetShaderUniform1iv` — Shader Interaction & Synchronization
- `SituationSetShaderUniformLocation` — Shader Interaction & Synchronization
- `SituationSetShaderUniformMatrix4fv` — Shader Interaction & Synchronization
- `SituationSetSoundEcho` — Sound Parameters and Effects
- `SituationSetSoundFilter` — Sound Parameters and Effects
- `SituationSetSoundPan` — Sound Parameters and Effects
- `SituationSetSoundPitch` — Sound Parameters and Effects
- `SituationSetSoundReverb` — Sound Parameters and Effects
- `SituationSetSoundVolume` — Sound Parameters and Effects
- `SituationSetTargetFPS` — Frame Timing & FPS Management
- `SituationSetTextureSamplerParams` — Texture Management
- `SituationSetThreadAffinity` — CPU & Thread Management
- `SituationSetThreadAffinityEx` — CPU & Thread Management
- `SituationSetTimerOscillatorPeriod` — Temporal Oscillator System
- `SituationSetToneRouting` — Node Graph SFX Routing (v2.6.5)
- `SituationSetTraceLogLevel` — Gamepad Input
- `SituationSetVSync` — Window State Management
- `SituationSetVirtualDisplayAttachmentDefaults` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayChromaSnow` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayClearColor` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayDirty` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayFallbackColor` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayFallbackMode` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayIdleThreshold` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayMaxAnisotropy` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayMemoryHint` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayMipLevels` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayPatternConfig` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayPatternLayers` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplaySampler` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayScalingMode` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayUpdateMode` — Virtual Displays (Render Targets)
- `SituationSetWindowFocused` — Window State Management
- `SituationSetWindowIcon` — Window Property Management
- `SituationSetWindowIcons` — Window Property Management
- `SituationSetWindowMaxSize` — Window Property Management
- `SituationSetWindowMinSize` — Window Property Management
- `SituationSetWindowMonitor` — Physical Display (Monitor) Management
- `SituationSetWindowOpacity` — Window Property Management
- `SituationSetWindowPosition` — Window Property Management
- `SituationSetWindowSize` — Window Property Management
- `SituationSetWindowState` — Window State Management
- `SituationSetWindowStateProfiles` — Advanced Window Profile Management
- `SituationSetWindowTitle` — Window Property Management
- `SituationSetupVirtualMidiLoopback` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationShowCursor` — Cursor, Clipboard and File Drops
- `SituationShowMessageBox` — Gamepad Input
- `SituationShutdown` — Application Lifecycle & State
- `SituationSoundCopy` — Sound Data Manipulation (Wave Utilities)
- `SituationSoundCrop` — Sound Data Manipulation (Wave Utilities)
- `SituationSoundExportAsWav` — Sound Data Manipulation (Wave Utilities)
- `SituationStartAudioCapture` — Audio Capture
- `SituationStartAudioCaptureEx` — Audio Capture
- `SituationStartMidiLearn` — Learning Operations
- `SituationStopAllLoadedSounds` — Audio Handle API
- `SituationStopAllTones` — Resonance (Procedural Synthesis)
- `SituationStopAudioCapture` — Audio Capture
- `SituationStopLoadedSound` — Audio Handle API
- `SituationStopTone` — Resonance (Procedural Synthesis)
- `SituationSubmitJobEx` — CPU & Thread Management
- `SituationSubmitRenderList` — Frame Lifecycle & Command Buffer
- `SituationSubmitRenderList` — Frame Lifecycle & Command Buffer
- `SituationTakeScreenshot` — Image & Screenshot Utilities
- `SituationTeardownVirtualMidiLoopback` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationTimerGetOscillatorPeriod` — Temporal Oscillator System
- `SituationTimerGetOscillatorState` — Temporal Oscillator System
- `SituationTimerGetOscillatorTriggerCount` — Temporal Oscillator System
- `SituationTimerGetPingProgress` — Temporal Oscillator System
- `SituationTimerGetPreviousOscillatorState` — Temporal Oscillator System
- `SituationTimerGetTime` — Temporal Oscillator System
- `SituationTimerHasOscillatorUpdated` — Temporal Oscillator System
- `SituationTimerPingOscillator` — Temporal Oscillator System
- `SituationToggleBorderlessWindowed` — Window State Management
- `SituationToggleFullscreen` — Window State Management
- `SituationToggleWindowStateFlags` — Advanced Window Profile Management
- `SituationTopologicalSort` — Node Graph Functions
- `SituationUnloadAudio` — Audio Handle API
- `SituationUnloadDroppedFiles` — Cursor, Clipboard and File Drops
- `SituationUnloadFont` — Font Management
- `SituationUnloadImage` — Image Loading and Unloading
- `SituationUnloadModel` — 3D Model Utilities
- `SituationUnloadShader` — Shader Management
- `SituationUnloadSound` — Audio Handle API
- `SituationUpdate` — Lifecycle
- `SituationUpdateBuffer` — GPU Buffer Management
- `SituationUpdateTimers` — Application Lifecycle & State
- `SituationValidateShaderUniforms` — Shader Interaction & Synchronization
- `SituationVdStandbyConfigInitDefaults` — Virtual Displays (Render Targets)
- `SituationVdStandbyLayerBit` — Virtual Displays (Render Targets)
- `SituationVdStandbyPackParamsStd430` — Virtual Displays (Render Targets)
- `SituationVdStandbyPackStd140` — Virtual Displays (Render Targets)
- `SituationVdStandbySetLayerOrder` — Virtual Displays (Render Targets)
- `SituationVdStandbyToggleLayer` — Virtual Displays (Render Targets)
- `SituationVirtualMidiControlChange` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiNoteOff` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiNoteOffEx` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiNoteOn` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiNoteOnEx` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiPitchBend` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiProgramChange` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationWaitForAllJobs` — CPU & Thread Management
- `SituationWaitForJob` — CPU & Thread Management
- `SituationWin32SetAppUserModelId` — Active Audio Device Query (v2.4.199)
- `SituationWindowShouldClose` — Application Lifecycle & State
- `SituationYpqAdjustChroma` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqAdjustLuma` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqAdjustPhase` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqAnalyzeRgbMapping` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqClampInGamut` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqDistance` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqEquals` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqGetChroma` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqGetHueDegrees` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqGetLuma` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqLerp` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqQuantize` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqSliceDuplicateCount` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqToRgb10Packed` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqToRgb10PackedHdr` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
- `SituationYpqToRgba10` — YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only)
