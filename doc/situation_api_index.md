# Situation Public API Index

_Auto-generated from `sit/situation_api.h` — Situation **2.4.126 (Vertex Index Bind SituationError)**._

Complete alphabetical index of every **SITAPI** function. Narrative documentation:

- **[situation_sdk.md](situation_sdk.md)** — SDK manual (architecture, workflows, examples)
- **[situation_api.md](situation_api.md)** — detailed reference (primary non-`Cmd` APIs)
- **[situation_command_reference.md](situation_command_reference.md)** — all `SituationCmd*` commands
- **[situation_api_generated.md](situation_api_generated.md)** — header-sync supplement for gaps

**Total public functions:** 451 (`SituationCmd*`: 41)

---

## Command buffer (`SituationCmd*`)

Canonical narrative: **[situation_command_reference.md](situation_command_reference.md)**.

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBeginDebugGroup` | [command ref](situation_command_reference.md#9-debug-markers) | — |
| `SituationCmdBeginRenderPass` | [command ref](situation_command_reference.md#1-render-pass--framebuffer) | Begins a render pass with detailed configuration. |
| `SituationCmdBeginRenderToDisplay` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] Begins a render pass on a target (-1 for main window), clearing it. |
| `SituationCmdBindComputeBuffer` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] Bind a buffer to a compute shader binding point. |
| `SituationCmdBindComputePipeline` | [command ref](situation_command_reference.md#7-compute) | Bind a compute pipeline for a subsequent dispatch. |
| `SituationCmdBindComputeTexture` | [command ref](situation_command_reference.md#7-compute) | [Core] Binds a texture as a storage image for compute shaders. |
| `SituationCmdBindDescriptorSet` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Binds a buffer's descriptor set (UBO/SSBO) to a set index. |
| `SituationCmdBindDescriptorSetDynamic` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Binds a dynamic buffer descriptor set with an offset. |
| `SituationCmdBindIndexBuffer` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind a 32-bit index buffer (UINT32 / GL_UNSIGNED_INT) for SituationCmdDrawIndexed. Pass of... |
| `SituationCmdBindPipeline` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | Binds a graphics pipeline (shader program) for subsequent draws. |
| `SituationCmdBindSampledTexture` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | Binds a texture as a sampled image (sampler2D) to a binding point. |
| `SituationCmdBindTexture` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] [Core] Bind a texture and sampler to a shader binding point. |
| `SituationCmdBindTextureSet` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Binds a texture's descriptor set (sampler/storage) to a set index. |
| `SituationCmdBindUniformBuffer` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] [Core] Bind a Uniform Buffer Object (UBO) to a shader binding point. |
| `SituationCmdBindVertexBuffer` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind a vertex buffer for subsequent SituationCmdDraw / SituationCmdDrawIndexed. |
| `SituationCmdCopyBuffer` | [command ref](situation_command_reference.md#8-transfer--presentation) | [Phase 1] Record an async copy between buffers. |
| `SituationCmdDispatch` | [command ref](situation_command_reference.md#7-compute) | Record a command to dispatch compute shader work groups. |
| `SituationCmdDraw` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Record a non-indexed draw call. |
| `SituationCmdDrawIndexed` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Record an indexed draw call. |
| `SituationCmdDrawMesh` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Records a command to draw a complete, pre-configured mesh. |
| `SituationCmdDrawQuad` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Record a command to draw a simple, colored 2D quad. |
| `SituationCmdDrawText` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Draws a text string using GPU-accelerated textured quads. |
| `SituationCmdDrawTextEx` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Advanced text drawing (scaling/spacing). |
| `SituationCmdDrawTexture` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Draw a part of a texture defined by a rectangle. |
| `SituationCmdEndDebugGroup` | [command ref](situation_command_reference.md#9-debug-markers) | — |
| `SituationCmdEndRender` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] End the current render pass. |
| `SituationCmdEndRenderPass` | [command ref](situation_command_reference.md#1-render-pass--framebuffer) | Ends the current render pass. |
| `SituationCmdPipelineBarrier` | [command ref](situation_command_reference.md#7-compute) | Insert a fine-grained pipeline barrier for synchronization. |
| `SituationCmdPopRasterState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdPresent` | [command ref](situation_command_reference.md#8-transfer--presentation) | Submits a command to copy a texture to the main window's swapchain (Compute-Only). |
| `SituationCmdPushRasterState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetBlendEnable` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetBlendFuncSeparate` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetCullMode` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthTest` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthWrite` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetPushConstant` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Set a small block of per-draw uniform data (push constant). |
| `SituationCmdSetPushConstantData` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | — |
| `SituationCmdSetScissor` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets the dynamic scissor rectangle to clip rendering. |
| `SituationCmdSetVertexAttribute` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [OpenGL Only] Attribute format + vertex buffer binding index (must match SituationCmdBindVertexBu... |
| `SituationCmdSetViewport` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets the dynamic viewport and scissor for the current render pass. |

---

## 3D Model Utilities

| Function | Summary |
|----------|---------|
| `SituationDrawModel` | Draws all sub-meshes of a model with a single root transformation. |
| `SituationGetMeshData` | Get raw vertex/index data pointers from a mesh (read-only). |
| `SituationLoadModel` | Loads a complete 3D model and its textures from a GLTF file. |
| `SituationSaveModelAsGltf` | Exports a model to a human-readable .gltf and a .bin file for debugging. |
| `SituationUnloadModel` | Frees all GPU and CPU resources associated with a loaded model. |

## [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCheckHotReloads` | — | Checks all tracked resources for file changes and reloads them if necessary. |
| `SituationCmdBeginRenderToDisplay` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] Begins a render pass on a target (-1 for main window), clearing it. |
| `SituationCmdBindComputeBuffer` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] Bind a buffer to a compute shader binding point. |
| `SituationCmdBindTexture` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] [Core] Bind a texture and sampler to a shader binding point. |
| `SituationCmdBindUniformBuffer` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] [Core] Bind a Uniform Buffer Object (UBO) to a shader binding point. |
| `SituationCmdEndRender` | [command ref](situation_command_reference.md#11-deprecated-commands) | [DEPRECATED] End the current render pass. |
| `SituationLoadComputeShader` | — | [DEPRECATED] Load a compute shader from a file. Use SituationCreateComputePipeline instead. |
| `SituationLoadComputeShaderFromMemory` | — | [DEPRECATED] Create a compute shader from memory. Use SituationCreateComputePipelineFromMemory instead. |
| `SituationMemoryBarrier` | — | [DEPRECATED] Insert a coarse-grained memory barrier. Use SituationCmdPipelineBarrier instead. |
| `SituationReloadComputePipeline` | — | Recompiles a compute pipeline from its original source file (Synchronous/Stalls GPU). |
| `SituationReloadModel` | — | Re-parses GLTF/GLB file and rebuilds all meshes and textures (Synchronous/Stalls GPU). |
| `SituationReloadShader` | — | Recompiles and links a shader from its original source files (Synchronous/Stalls GPU). |
| `SituationReloadTexture` | — | Re-reads image file and recreates the GPU texture resource (Synchronous/Stalls GPU). |

## Abstracted Rendering Commands

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBeginRenderPass` | [command ref](situation_command_reference.md#1-render-pass--framebuffer) | Begins a render pass with detailed configuration. |
| `SituationCmdBindComputeTexture` | [command ref](situation_command_reference.md#7-compute) | [Core] Binds a texture as a storage image for compute shaders. |
| `SituationCmdBindDescriptorSet` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Binds a buffer's descriptor set (UBO/SSBO) to a set index. |
| `SituationCmdBindDescriptorSetDynamic` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Binds a dynamic buffer descriptor set with an offset. |
| `SituationCmdBindIndexBuffer` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind a 32-bit index buffer (UINT32 / GL_UNSIGNED_INT) for SituationCmdDrawIndexed. Pass offset 0 when indices ... |
| `SituationCmdBindPipeline` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | Binds a graphics pipeline (shader program) for subsequent draws. |
| `SituationCmdBindSampledTexture` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | Binds a texture as a sampled image (sampler2D) to a binding point. |
| `SituationCmdBindTextureSet` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Binds a texture's descriptor set (sampler/storage) to a set index. |
| `SituationCmdBindVertexBuffer` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Bind a vertex buffer for subsequent SituationCmdDraw / SituationCmdDrawIndexed. |
| `SituationCmdDraw` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Record a non-indexed draw call. |
| `SituationCmdDrawIndexed` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [Core] Record an indexed draw call. |
| `SituationCmdDrawMesh` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Records a command to draw a complete, pre-configured mesh. |
| `SituationCmdDrawQuad` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Record a command to draw a simple, colored 2D quad. |
| `SituationCmdDrawText` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Draws a text string using GPU-accelerated textured quads. |
| `SituationCmdDrawTextEx` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | Advanced text drawing (scaling/spacing). |
| `SituationCmdDrawTexture` | [command ref](situation_command_reference.md#6-high-level-draw-helpers) | [High-Level] Draw a part of a texture defined by a rectangle. |
| `SituationCmdEndRenderPass` | [command ref](situation_command_reference.md#1-render-pass--framebuffer) | Ends the current render pass. |
| `SituationCmdPresent` | [command ref](situation_command_reference.md#8-transfer--presentation) | Submits a command to copy a texture to the main window's swapchain (Compute-Only). |
| `SituationCmdSetPushConstant` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | [Core] Set a small block of per-draw uniform data (push constant). |
| `SituationCmdSetScissor` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets the dynamic scissor rectangle to clip rendering. |
| `SituationCmdSetVertexAttribute` | [command ref](situation_command_reference.md#5-vertex-input--manual-draw-core-path) | [OpenGL Only] Attribute format + vertex buffer binding index (must match SituationCmdBindVertexBuffer). |
| `SituationCmdSetViewport` | [command ref](situation_command_reference.md#2-dynamic-viewport--scissor) | Sets the dynamic viewport and scissor for the current render pass. |

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
| `SituationGetVersionString` | Returns a read-only static string (e.g., "2.3.3A"). Do not free. |
| `SituationInit` | Initialize the library, create window and graphics context. |
| `SituationIsAppPaused` | Check if the application is currently paused. |
| `SituationIsInitialized` | Check if the library has been successfully initialized. |
| `SituationPauseApp` | Pause the application's internal state (e.g., audio). |
| `SituationPollInputEvents` | Poll for all input events (keyboard, mouse, joystick). Call once per frame. |
| `SituationResumeApp` | Resume a paused application. |
| `SituationShutdown` | Shut down the library and release all resources. |
| `SituationUpdate` | DEPRECATED: Use SituationPollInputEvents() and SituationUpdateTimers(). |
| `SituationUpdateTimers` | Update all internal timers (frame timer, temporal system). Call after polling events. |
| `SituationWindowShouldClose` | Check if the application should close (e.g., user clicked X). |

## Audio Capture

| Function | Summary |
|----------|---------|
| `SituationStartAudioCapture` | Start capturing audio input with default format. |
| `SituationStartAudioCaptureEx` | Start capturing with explicit sample rate and channel count. |
| `SituationStopAudioCapture` | Stop audio capture and release the input device. |

## Audio Device Management

| Function | Summary |
|----------|---------|
| `SituationGetAudioDevices` | Get a list of available audio playback devices (caller must free). |
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
| `SituationPlayMidiNote` | Legacy: play a tone by MIDI note number (0-127). |
| `SituationPlayTone` | Legacy: play a simple ADSR tone (backward compat / quick UI sounds). |
| `SituationSetAudioPan` | Set stereo pan for a handle-based sound [-1.0 to 1.0]. |
| `SituationSetAudioPitch` | Set pitch multiplier for a handle-based sound (1.0 = normal). |
| `SituationSetAudioVolume` | Set volume for a handle-based sound [0.0 to 1.0+]. |
| `SituationStopAllLoadedSounds` | Stop all currently playing sounds. |
| `SituationStopAllTones` | Stop all active tones (triggers release on each). |
| `SituationStopLoadedSound` | Stop a specific sound from playing. |
| `SituationStopTone` | Gracefully stop a tone by triggering its release envelope. Invalid handles are ignored. |
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
| `SituationGetRendererType` | Get the current active renderer type (OpenGL or Vulkan). |
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
| `SituationColorFromYPQ` | Converts a YPQA color back to the standard RGBA color space. |
| `SituationColorToYPQ` | Converts a standard RGBA color to the YPQA (Luma, Phase, Quadrature) color space. |
| `SituationConvertColorToVector4` | Convert an 8-bit ColorRGBA struct to a normalized Vector4. |
| `SituationHsvToRgb` | Converts a Hue, Saturation, Value color back to the standard RGBA color space. |
| `SituationRgbToHsv` | Converts a standard RGBA color to the Hue, Saturation, Value color space. |

## Command Buffer Recording

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBeginDebugGroup` | [command ref](situation_command_reference.md#9-debug-markers) | — |
| `SituationCmdEndDebugGroup` | [command ref](situation_command_reference.md#9-debug-markers) | — |
| `SituationCmdPopRasterState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdPushRasterState` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetBlendEnable` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetBlendFuncSeparate` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetCullMode` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthTest` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetDepthWrite` | [command ref](situation_command_reference.md#3-raster-state-fixed-function) | — |
| `SituationCmdSetPushConstantData` | [command ref](situation_command_reference.md#4-graphics-pipeline--shader-data) | — |

## Command-Line Argument Queries

| Function | Summary |
|----------|---------|
| `SituationGetArgumentValue` | Get the value of an argument (e.g., "jungle" from "-level:jungle"). |
| `SituationIsArgumentPresent` | Check if a command-line argument (e.g., "-server") was provided. |

## Compute Shader Pipeline

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdBindComputePipeline` | [command ref](situation_command_reference.md#7-compute) | Bind a compute pipeline for a subsequent dispatch. |
| `SituationCmdDispatch` | [command ref](situation_command_reference.md#7-compute) | Record a command to dispatch compute shader work groups. |
| `SituationCreateComputePipeline` | — | Create a compute pipeline from a shader file. |
| `SituationCreateComputePipelineFromMemory` | — | Create a compute pipeline from in-memory GLSL source. |
| `SituationDestroyComputePipeline` | — | Destroy a compute pipeline and free its GPU resources. |
| `SituationGetMaxComputeWorkGroups` | — | Query maximum compute work group count per dispatch. |

## Config Flags

| Function | Summary |
|----------|---------|
| `SituationFreeString` | Free a string allocated by the library (e.g., from path helpers). |

## CPU & Thread Management

| Function | Summary |
|----------|---------|
| `SituationAddJobDependencies` | Adds multiple dependencies for a single dependent job. |
| `SituationAddJobDependency` | Adds a dependency between two jobs (prereq -> dependent). |
| `SituationCreateThreadPool` | Initializes the thread pool with dual-priority queues and worker threads. |
| `SituationDestroyThreadPool` | Shuts down the thread pool and releases resources. |
| `SituationDispatchParallel` | Executes a loop in parallel across worker threads (Fork-Join). |
| `SituationDumpTaskGraph` | Prints the current task graph state to the stream. |
| `SituationGetCPUCoreCount` | Gets physical processors (Cores) |
| `SituationGetCPUThreadCount` | Gets logical processors (Threads) |
| `SituationLoadSoundFromFileAsync` | Asynchronously loads and decodes a sound file. |
| `SituationSetThreadAffinity` | Pins the CURRENT thread to specific logical cores |
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
| `SituationEnumerateAudioDevices` | Enumerate available audio devices. Caller must free with SituationFreeDeviceList. |
| `SituationFindBestDevice` | Find the best matching device by type and channel requirements. |
| `SituationFreeDeviceList` | Free a device list returned by SituationEnumerateAudioDevices. |

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
| `SituationBakeFontAtlas` | Rasterize a font into a GPU-ready atlas at the given size. |
| `SituationImageDrawCodepoint` | Draw a single Unicode character with advanced styling onto an image. |
| `SituationImageDrawText` | Draw a simple, tinted text string onto an image. |
| `SituationImageDrawTextEx` | Draw a text string with advanced styling (rotation, outline) onto an image. |
| `SituationImageDrawTextFormatted` | Draw printf-style formatted text onto an image. |
| `SituationLoadBitmapFontFromMemory` | Loads a raw bitmap font (e.g. 8x8 array). |
| `SituationLoadFont` | Load a font from a TTF/OTF file for CPU rendering. |
| `SituationLoadFontFromMemory` | Loads a font directly from a memory buffer (e.g., embedded resource). |
| `SituationMeasureText` | Measure the pixel dimensions of a string before drawing. |
| `SituationUnloadFont` | Unload a CPU-side font and free its memory. |

## Frame Lifecycle & Command Buffer

| Function | Summary |
|----------|---------|
| `SituationAcquireFrameCommandBuffer` | Prepare the backend for a new frame of rendering commands. |
| `SituationEndFrame` | Submit all commands for the frame and present the result. |
| `SituationGetComputeCommandBuffer` | [v2.3.23] Get the compute-specific command buffer (Vulkan only). |
| `SituationGetMainCommandBuffer` | Get the primary command buffer for the current frame. |
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
| `SituationIsGamepad` | Check if a connected joystick has a standard gamepad mapping. |
| `SituationIsGamepadButtonDown` | Check if a gamepad button is currently held down (a state). |
| `SituationIsGamepadButtonPressed` | Check if a gamepad button was pressed down this frame (an event). |
| `SituationIsGamepadButtonReleased` | Check if a gamepad button was released this frame (an event). |
| `SituationIsJoystickPresent` | Check if a joystick/gamepad is connected. |
| `SituationSetGamepadMappings` | Load a new set of gamepad mappings from a string. |
| `SituationSetGamepadVibration` | Set gamepad vibration/rumble (Windows only). |
| `SituationSetJoystickCallback` | Set a callback for joystick connection events. |

## GPU Buffer Management

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationCmdCopyBuffer` | [command ref](situation_command_reference.md#8-transfer--presentation) | [Phase 1] Record an async copy between buffers. |
| `SituationCreateBuffer` | — | Create a generic GPU data buffer (e.g., SSBO). |
| `SituationCreateReadbackBuffer` | — | [Phase 1] Create an async GPU->CPU staging buffer. |
| `SituationDestroyBuffer` | — | Destroy a GPU buffer. |
| `SituationGetBufferData` | — | Read data from a GPU buffer (blocking). |
| `SituationReadBuffer` | — | [Phase 1] Read mapped buffer data safely. |
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

## Graphics Resource Management

| Function | Summary |
|----------|---------|
| `SituationCreateMesh` | Create a mesh from vertex and index data. |
| `SituationDestroyMesh` | Unload a mesh from GPU memory. |
| `SituationGetBufferDeviceAddress` | Retrieves the GPU device address of a buffer for bindless access. |
| `SituationGetTextureHandle` | Retrieves the bindless texture handle (OpenGL Only). |

## Image & Screenshot Utilities

| Function | Summary |
|----------|---------|
| `SituationLoadImageFromScreen` | Get a copy of the current screen backbuffer as an image. |
| `SituationTakeScreenshot` | Take a screenshot and save it to a file (PNG or BMP). |

## Image Exporting

| Function | Summary |
|----------|---------|
| `SituationExportImage` | Export image data to a file (PNG, BMP supported). |

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
| `SituationLoadImage` | Load an image from a file into CPU memory (RAM). |
| `SituationLoadImageFromMemory` | Load an image from a memory buffer. |
| `SituationUnloadImage` | Unload an image's pixel data from memory. |

## Image Manipulation (Modifies image in-place)

| Function | Summary |
|----------|---------|
| `SituationImageAdjustHSV` | Control an image by Hue Saturation and Brightness. |
| `SituationImageCrop` | Crop an image to a specific rectangle. |
| `SituationImageFlip` | Flip an image. |
| `SituationImageResize` | Resize an image using default bicubic scaling. |

## Initialization Configuration Structure (Passed to SituationInit)

| Function | Summary |
|----------|---------|
| `SituationIsFeatureSupported` | Check if a graphics feature is supported on current hardware. |

## Initialization State Management (v2.3.40)

| Function | Summary |
|----------|---------|
| `SituationLog` | Log a message at the specified level (SIT_LOG_*). |
| `SituationLogWarning` | Log a warning with an associated error code (debug builds only). |
| `SituationSetLogCallback` | Set a custom log callback. |
| `SituationSetTraceLogLevel` | Set the minimum log level for output filtering. |
| `SituationShowMessageBox` | Blocking UI message box; for fatal init errors. |

## Keyboard Input

| Function | Summary |
|----------|---------|
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

## Node Graph SFX Routing (v2.6.5)

| Function | Summary |
|----------|---------|
| `SituationSetGraphSFXSource` | Designate the Sound Source node in the active graph to receive routed SFX tones. |
| `SituationSetToneRouting` | Route a procedural tone to the active graph's SFX sound source. |

## Path Management & Special Directories

| Function | Summary |
|----------|---------|
| `SituationGetAppSavePath` | Get a safe, persistent path for saving application data (caller must free). |
| `SituationGetBasePath` | Get the path to the directory containing the executable (caller must free). |
| `SituationGetFileExtension` | Extract the file extension from a path. |
| `SituationGetFileName` | Extract the file name (including extension) from a full path. |
| `SituationJoinPath` | Join two path components with the correct OS separator (caller must free). |

## Physical Display (Monitor) Management

| Function | Summary |
|----------|---------|
| `SituationFreeDisplays` | Free a display info array returned by SituationGetDisplays. |
| `SituationGetCurrentMonitor` | Get the index of the monitor the window is on. |
| `SituationGetDisplays` | Get information for all displays (caller must free). |
| `SituationGetMonitorCount` | Get the number of connected monitors. |
| `SituationGetMonitorHeight` | Get the height of a monitor's current video mode. |
| `SituationGetMonitorName` | Get the human-readable name of a monitor. |
| `SituationGetMonitorPhysicalHeight` | Get the physical height of a monitor in millimeters. |
| `SituationGetMonitorPhysicalWidth` | Get the physical width of a monitor in millimeters. |
| `SituationGetMonitorPosition` | Get the top-left position of a monitor on the desktop. |
| `SituationGetMonitorRefreshRate` | Get the refresh rate of a monitor. |
| `SituationGetMonitorWidth` | Get the width of a monitor's current video mode. |
| `SituationRefreshDisplays` | Force a refresh of the cached display information. |
| `SituationSetDisplayMode` | Set the display mode for a monitor. |
| `SituationSetWindowMonitor` | Set the window to be fullscreen on a specific monitor. |

## Preset Persistence

| Function | Summary |
|----------|---------|
| `SituationLoadMidiPreset` | Load MIDI Learn mappings from JSON file. |
| `SituationSaveMidiPreset` | Save MIDI Learn mappings to JSON file. |

## Profiling & Diagnostics

| Function | Summary |
|----------|---------|
| `SituationDrawMetricsOverlay` | Draws FPS, Latency, and Memory stats |
| `SituationExportRenderHistogram` | Write a text-based frame time histogram into buf. |
| `SituationGetDrawCallCount` | Number of draw commands this frame |
| `SituationGetIOQueueDepth` | [v2.3.37] Get the current depth of the IO/Low Priority queue |
| `SituationGetRenderLatencyStats` | Get render thread latency metrics |
| `SituationGetRenderQueueDepth` | Get the current depth of the render queue |
| `SituationGetVRAMUsage` | Total GPU memory allocated (Bytes) |

## Shader Interaction & Synchronization

| Function | Doc | Summary |
|----------|-----|---------|
| `SituationBindShaderStorageBlock` | — | [OpenGL] glShaderStorageBlockBinding for SPIR-V when reflection reports binding 0 for layout(binding=N). |
| `SituationBindUniformBlock` | — | [OpenGL] glUniformBlockBinding for std140 UBO blocks (layout(binding=N)). |
| `SituationCmdPipelineBarrier` | [command ref](situation_command_reference.md#7-compute) | Insert a fine-grained pipeline barrier for synchronization. |
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

## System & Hardware Information

| Function | Summary |
|----------|---------|
| `SituationExecuteCommand` | Execute a shell command hidden, return exit code & combined output. |
| `SituationGetCPUThreadCount` | Get the number of logical CPU cores. |
| `SituationGetCurrentDriveLetter` | Get the drive letter of the running executable (Windows only). |
| `SituationGetDeviceInfo` | Get detailed information about system hardware (CPU, GPU, RAM, etc.). |
| `SituationGetDriveInfo` | Get info for a specific drive (Windows only). |
| `SituationGetGPUName` | Get the name of the active GPU. |
| `SituationGetGraphicsCaps` | Get backend capabilities for examples/frameworks. |
| `SituationGetUserDirectory` | Get the full path to the current user's home directory (caller must free). |
| `SituationOpenFile` | Open a file or folder with its default application. |

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

| Function | Summary |
|----------|---------|
| `SituationCreateTexture` | Create a texture from a CPU-side image. |
| `SituationCreateTextureEx` | Create a texture with specific usage flags. |
| `SituationDestroyTexture` | Unload a texture from GPU memory. |
| `SituationGetTextureInfo` | [Phase 2] Query texture metadata. |
| `SituationLoadTexture` | Loads a texture from disk and registers the path for hot-reloading. |
| `SituationReadFramebuffer` | [Phase 2] Blocking readback of framebuffer pixels. |
| `SituationReadTexture` | [Phase 2] Blocking readback of texture pixels. |
| `SituationReadTextureAlloc` | [Phase 2] Blocking readback into allocated SituationImage. |
| `SituationSetTextureSamplerParams` | [Phase 2] Update sampler state. |

## Virtual Displays (Render Targets)

| Function | Summary |
|----------|---------|
| `SituationConfigureVirtualDisplay` | Configure a virtual display's properties. |
| `SituationCreateVirtualDisplay` | Create an off-screen render target. |
| `SituationDestroyVirtualDisplay` | Destroy a virtual display. |
| `SituationGetLastVDCompositeTimeMS` | Get the time taken for the last virtual display composite pass. |
| `SituationGetVirtualDisplay` | Get a pointer to a virtual display's state. |
| `SituationGetVirtualDisplaySize` | Get the internal resolution of a virtual display. |
| `SituationIsVirtualDisplayDirty` | Check if a virtual display is marked as dirty. |
| `SituationRenderVirtualDisplays` | Composite all visible virtual displays to the current target. |
| `SituationSetVirtualDisplayDirty` | Mark a virtual display as needing to be re-rendered. |
| `SituationSetVirtualDisplayScalingMode` | Set the scaling/filtering mode for a virtual display. |

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

## Alphabetical (all)

- `SituationAcquireFrameCommandBuffer` — Frame Lifecycle & Command Buffer
- `SituationAddJobDependencies` — CPU & Thread Management
- `SituationAddJobDependency` — CPU & Thread Management
- `SituationApplyCurrentProfileWindowState` — Advanced Window Profile Management
- `SituationAttachAudioProcessor` — Custom Audio Processing
- `SituationAutoConnectMidi` — MIDI Device Control
- `SituationBakeFontAtlas` — Font Management
- `SituationBeginLoadShaderFromMemory` — Shader Management
- `SituationBeginLoadShaderFromSpirvMemory` — Shader Management
- `SituationBeginLoadShaderFromSpirvMemoryEx` — Shader Management
- `SituationBindShaderStorageBlock` — Shader Interaction & Synchronization
- `SituationBindUniformBlock` — Shader Interaction & Synchronization
- `SituationBlitRawDataToImage` — Image Generation & Copying
- `SituationCameraBuildInvViewProj` — Camera & Projection Math
- `SituationCameraBuildProj` — Camera & Projection Math
- `SituationCameraBuildView` — Camera & Projection Math
- `SituationCameraBuildViewProj` — Camera & Projection Math
- `SituationCameraUnprojectPixel` — Camera & Projection Math
- `SituationCancelMidiLearn` — Learning Operations
- `SituationCheckHotReloads` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationClearAllMidiMappings` — Mapping Management
- `SituationClearMidiMapping` — Mapping Management
- `SituationClearWindowState` — Window State Management
- `SituationCmdBeginDebugGroup` — Command Buffer Recording
- `SituationCmdBeginRenderPass` — Abstracted Rendering Commands
- `SituationCmdBeginRenderToDisplay` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationCmdBindComputeBuffer` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationCmdBindComputePipeline` — Compute Shader Pipeline
- `SituationCmdBindComputeTexture` — Abstracted Rendering Commands
- `SituationCmdBindDescriptorSet` — Abstracted Rendering Commands
- `SituationCmdBindDescriptorSetDynamic` — Abstracted Rendering Commands
- `SituationCmdBindIndexBuffer` — Abstracted Rendering Commands
- `SituationCmdBindPipeline` — Abstracted Rendering Commands
- `SituationCmdBindSampledTexture` — Abstracted Rendering Commands
- `SituationCmdBindTexture` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationCmdBindTextureSet` — Abstracted Rendering Commands
- `SituationCmdBindUniformBuffer` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationCmdBindVertexBuffer` — Abstracted Rendering Commands
- `SituationCmdCopyBuffer` — GPU Buffer Management
- `SituationCmdDispatch` — Compute Shader Pipeline
- `SituationCmdDraw` — Abstracted Rendering Commands
- `SituationCmdDrawIndexed` — Abstracted Rendering Commands
- `SituationCmdDrawMesh` — Abstracted Rendering Commands
- `SituationCmdDrawQuad` — Abstracted Rendering Commands
- `SituationCmdDrawText` — Abstracted Rendering Commands
- `SituationCmdDrawTextEx` — Abstracted Rendering Commands
- `SituationCmdDrawTexture` — Abstracted Rendering Commands
- `SituationCmdEndDebugGroup` — Command Buffer Recording
- `SituationCmdEndRender` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationCmdEndRenderPass` — Abstracted Rendering Commands
- `SituationCmdPipelineBarrier` — Shader Interaction & Synchronization
- `SituationCmdPopRasterState` — Command Buffer Recording
- `SituationCmdPresent` — Abstracted Rendering Commands
- `SituationCmdPushRasterState` — Command Buffer Recording
- `SituationCmdSetBlendEnable` — Command Buffer Recording
- `SituationCmdSetBlendFuncSeparate` — Command Buffer Recording
- `SituationCmdSetCullMode` — Command Buffer Recording
- `SituationCmdSetDepthTest` — Command Buffer Recording
- `SituationCmdSetDepthWrite` — Command Buffer Recording
- `SituationCmdSetPushConstant` — Abstracted Rendering Commands
- `SituationCmdSetPushConstantData` — Command Buffer Recording
- `SituationCmdSetScissor` — Abstracted Rendering Commands
- `SituationCmdSetVertexAttribute` — Abstracted Rendering Commands
- `SituationCmdSetViewport` — Abstracted Rendering Commands
- `SituationColorFromYPQ` — Color Space Conversions
- `SituationColorToYPQ` — Color Space Conversions
- `SituationConfigureVirtualDisplay` — Virtual Displays (Render Targets)
- `SituationConvertColorToVector4` — Color Space Conversions
- `SituationCopyFile` — File Operations
- `SituationCreateBuffer` — GPU Buffer Management
- `SituationCreateComputePipeline` — Compute Shader Pipeline
- `SituationCreateComputePipelineFromMemory` — Compute Shader Pipeline
- `SituationCreateDirectory` — Directory Operations
- `SituationCreateGraph` — Node Graph Functions
- `SituationCreateImage` — Image Generation & Copying
- `SituationCreateMesh` — Graphics Resource Management
- `SituationCreateNode` — Node Graph Functions
- `SituationCreatePatch` — Node Graph Functions
- `SituationCreateReadbackBuffer` — GPU Buffer Management
- `SituationCreateTexture` — Texture Management
- `SituationCreateTextureEx` — Texture Management
- `SituationCreateThreadPool` — CPU & Thread Management
- `SituationCreateVirtualDisplay` — Virtual Displays (Render Targets)
- `SituationDeleteDirectory` — Directory Operations
- `SituationDeleteFile` — File Operations
- `SituationDeserializeGraphFromJSON` — Graph Serialization Functions
- `SituationDestroyBuffer` — GPU Buffer Management
- `SituationDestroyComputePipeline` — Compute Shader Pipeline
- `SituationDestroyGraph` — Node Graph Functions
- `SituationDestroyMesh` — Graphics Resource Management
- `SituationDestroyNode` — Node Graph Functions
- `SituationDestroyPatch` — Node Graph Functions
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
- `SituationEnableMidiControl` — MIDI Device Control
- `SituationEnableMidiLearn` — MIDI Learn Lifecycle
- `SituationEndFrame` — Frame Lifecycle & Command Buffer
- `SituationEnumerateAudioDevices` — Device Enumeration (Phase 0)
- `SituationErrorToString` — Callbacks and Event Handling
- `SituationExecuteCommand` — System & Hardware Information
- `SituationExportImage` — Image Exporting
- `SituationExportRenderHistogram` — Profiling & Diagnostics
- `SituationFileExists` — File & Directory Queries
- `SituationFindBestDevice` — Device Enumeration (Phase 0)
- `SituationFreeDeviceList` — Device Enumeration (Phase 0)
- `SituationFreeDirectoryFileList` — Directory Operations
- `SituationFreeDisplays` — Physical Display (Monitor) Management
- `SituationFreeJSONString` — Graph Serialization Functions
- `SituationFreeString` — Config Flags
- `SituationGenImageColor` — Image Generation & Copying
- `SituationGenImageGradient` — Image Generation & Copying
- `SituationGetActiveGraph` — Active Graph (Audio Callback Integration)
- `SituationGetAppSavePath` — Path Management & Special Directories
- `SituationGetArgumentValue` — Command-Line Argument Queries
- `SituationGetAudioDevices` — Audio Device Management
- `SituationGetAudioMasterVolume` — Audio Device Management
- `SituationGetAudioPlaybackSampleRate` — Audio Device Management
- `SituationGetBasePath` — Path Management & Special Directories
- `SituationGetBufferData` — GPU Buffer Management
- `SituationGetBufferDeviceAddress` — Graphics Resource Management
- `SituationGetCPUCoreCount` — CPU & Thread Management
- `SituationGetCPUThreadCount` — System & Hardware Information
- `SituationGetCPUThreadCount` — CPU & Thread Management
- `SituationGetCategoryName` — Device Registry Functions
- `SituationGetCharFromScancode` — Keyboard Input
- `SituationGetCharPressed` — Keyboard Input
- `SituationGetClipboardText` — Cursor, Clipboard and File Drops
- `SituationGetComputeCommandBuffer` — Frame Lifecycle & Command Buffer
- `SituationGetControl` — Node Graph Functions
- `SituationGetCurrentActualWindowStateFlags` — Advanced Window Profile Management
- `SituationGetCurrentDriveLetter` — System & Hardware Information
- `SituationGetCurrentMonitor` — Physical Display (Monitor) Management
- `SituationGetDeviceInfo` — System & Hardware Information
- `SituationGetDeviceMetadata` — Device Registry Functions
- `SituationGetDisplays` — Physical Display (Monitor) Management
- `SituationGetDrawCallCount` — Profiling & Diagnostics
- `SituationGetDriveInfo` — System & Hardware Information
- `SituationGetFPS` — Frame Timing & FPS Management
- `SituationGetFileExtension` — Path Management & Special Directories
- `SituationGetFileModTime` — File & Directory Queries
- `SituationGetFileName` — Path Management & Special Directories
- `SituationGetFrameTime` — Frame Timing & FPS Management
- `SituationGetGLFWwindow` — Backend-Specific Accessors
- `SituationGetGPUName` — System & Hardware Information
- `SituationGetGamepadAxisCount` — Gamepad Input
- `SituationGetGamepadAxisValue` — Gamepad Input
- `SituationGetGamepadButtonPressed` — Gamepad Input
- `SituationGetGraphicsCaps` — System & Hardware Information
- `SituationGetIOQueueDepth` — Profiling & Diagnostics
- `SituationGetInitState` — Application Lifecycle & State
- `SituationGetJoystickName` — Gamepad Input
- `SituationGetKeyPressed` — Keyboard Input
- `SituationGetKeyPressedEx` — Keyboard Input
- `SituationGetKeyScancode` — Keyboard Input
- `SituationGetLastErrorCode` — Callbacks and Event Handling
- `SituationGetLastErrorMsg` — Callbacks and Event Handling
- `SituationGetLastVDCompositeTimeMS` — Virtual Displays (Render Targets)
- `SituationGetMainCommandBuffer` — Frame Lifecycle & Command Buffer
- `SituationGetMainWindowRenderPass` — Backend-Specific Accessors
- `SituationGetMasterOutputMeter` — Audio Output Monitoring (for visualization)
- `SituationGetMaxComputeWorkGroups` — Compute Shader Pipeline
- `SituationGetMeshData` — 3D Model Utilities
- `SituationGetMidiDeviceName` — MIDI Device Control
- `SituationGetMonitorCount` — Physical Display (Monitor) Management
- `SituationGetMonitorHeight` — Physical Display (Monitor) Management
- `SituationGetMonitorName` — Physical Display (Monitor) Management
- `SituationGetMonitorPhysicalHeight` — Physical Display (Monitor) Management
- `SituationGetMonitorPhysicalWidth` — Physical Display (Monitor) Management
- `SituationGetMonitorPosition` — Physical Display (Monitor) Management
- `SituationGetMonitorRefreshRate` — Physical Display (Monitor) Management
- `SituationGetMonitorWidth` — Physical Display (Monitor) Management
- `SituationGetMouseDelta` — Mouse Input
- `SituationGetMousePosition` — Mouse Input
- `SituationGetMouseWheelMove` — Mouse Input
- `SituationGetMouseWheelMoveV` — Mouse Input
- `SituationGetNode` — Node Graph Functions
- `SituationGetRegisteredDeviceCount` — Device Registry Functions
- `SituationGetRenderHeight` — Window & Screen Dimension Queries
- `SituationGetRenderLatencyStats` — Profiling & Diagnostics
- `SituationGetRenderQueueDepth` — Profiling & Diagnostics
- `SituationGetRenderWidth` — Window & Screen Dimension Queries
- `SituationGetRendererType` — Backend-Specific Accessors
- `SituationGetScreenHeight` — Window & Screen Dimension Queries
- `SituationGetScreenWidth` — Window & Screen Dimension Queries
- `SituationGetSerializationVersion` — Graph Serialization Functions
- `SituationGetSoundPan` — Sound Parameters and Effects
- `SituationGetSoundPitch` — Sound Parameters and Effects
- `SituationGetSoundVolume` — Sound Parameters and Effects
- `SituationGetTextureHandle` — Graphics Resource Management
- `SituationGetTextureInfo` — Texture Management
- `SituationGetUserDirectory` — System & Hardware Information
- `SituationGetVRAMUsage` — Profiling & Diagnostics
- `SituationGetVersionString` — Application Lifecycle & State
- `SituationGetVirtualDisplay` — Virtual Displays (Render Targets)
- `SituationGetVirtualDisplaySize` — Virtual Displays (Render Targets)
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
- `SituationInit` — Application Lifecycle & State
- `SituationInitDeviceRegistry` — Device Registry Functions
- `SituationIsAppPaused` — Application Lifecycle & State
- `SituationIsArgumentPresent` — Command-Line Argument Queries
- `SituationIsAudioDevicePlaying` — Audio Device Management
- `SituationIsDeviceRegistered` — Device Registry Functions
- `SituationIsFeatureSupported` — Initialization Configuration Structure (Passed to SituationInit)
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
- `SituationIsVersionCompatible` — Graph Serialization Functions
- `SituationIsVirtualDisplayDirty` — Virtual Displays (Render Targets)
- `SituationIsWindowFullscreen` — Window State Queries
- `SituationIsWindowHidden` — Window State Queries
- `SituationIsWindowMaximized` — Window State Queries
- `SituationIsWindowMinimized` — Window State Queries
- `SituationIsWindowResized` — Window State Queries
- `SituationIsWindowState` — Window State Queries
- `SituationJoinPath` — Path Management & Special Directories
- `SituationListDirectoryFiles` — Directory Operations
- `SituationListMidiDevices` — MIDI Device Control
- `SituationLoadAudio` — Audio Handle API
- `SituationLoadBitmapFontFromMemory` — Font Management
- `SituationLoadComputeShader` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationLoadComputeShaderFromMemory` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
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
- `SituationLoadShader` — Shader Management
- `SituationLoadShaderFromMemory` — Shader Management
- `SituationLoadShaderFromSpirv` — Shader Management
- `SituationLoadShaderFromSpirvMemory` — Shader Management
- `SituationLoadShaderFromSpirvMemoryEx` — Shader Management
- `SituationLoadSoundFromFile` — Audio Handle API
- `SituationLoadSoundFromFileAsync` — CPU & Thread Management
- `SituationLoadSoundFromStream` — Audio Handle API
- `SituationLoadTexture` — Texture Management
- `SituationLog` — Initialization State Management (v2.3.40)
- `SituationLogWarning` — Initialization State Management (v2.3.40)
- `SituationMaximizeWindow` — Window State Management
- `SituationMeasureText` — Font Management
- `SituationMemoryBarrier` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationMinimizeWindow` — Window State Management
- `SituationMoveFile` — File Operations
- `SituationOpenFile` — System & Hardware Information
- `SituationPauseApp` — Application Lifecycle & State
- `SituationPauseAudioDevice` — Audio Device Management
- `SituationPeekKeyPressed` — Keyboard Input
- `SituationPeekKeyPressedEx` — Keyboard Input
- `SituationPlayAudio` — Audio Handle API
- `SituationPlayLoadedSound` — Audio Handle API
- `SituationPlayMidiNote` — Audio Handle API
- `SituationPlayTone` — Audio Handle API
- `SituationPollInputEvents` — Application Lifecycle & State
- `SituationPollShaderLoad` — Shader Management
- `SituationReadBuffer` — GPU Buffer Management
- `SituationReadFramebuffer` — Texture Management
- `SituationReadTexture` — Texture Management
- `SituationReadTextureAlloc` — Texture Management
- `SituationRefreshDisplays` — Physical Display (Monitor) Management
- `SituationRegisterDeviceType` — Device Registry Functions
- `SituationReloadComputePipeline` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationReloadModel` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationReloadShader` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationReloadTexture` — [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead.
- `SituationRemovePatch` — Node Graph Functions
- `SituationRenameFile` — File Operations
- `SituationRenderVirtualDisplays` — Virtual Displays (Render Targets)
- `SituationReplayRenderList` — Frame Lifecycle & Command Buffer
- `SituationResetRenderList` — Frame Lifecycle & Command Buffer
- `SituationRestoreWindow` — Window State Management
- `SituationResumeApp` — Application Lifecycle & State
- `SituationResumeAudioDevice` — Audio Device Management
- `SituationRgbToHsv` — Color Space Conversions
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
- `SituationSetLogCallback` — Initialization State Management (v2.3.40)
- `SituationSetMaximizeCallback` — Callbacks and Event Handling
- `SituationSetMouseButtonCallback` — Mouse Input
- `SituationSetMouseOffset` — Mouse Input
- `SituationSetMousePosition` — Mouse Input
- `SituationSetMouseScale` — Mouse Input
- `SituationSetNodeMidiChannel` — MIDI Device Control
- `SituationSetPixelColor` — Image Generation & Copying
- `SituationSetResizeCallback` — Callbacks and Event Handling
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
- `SituationSetTimerOscillatorPeriod` — Temporal Oscillator System
- `SituationSetToneRouting` — Node Graph SFX Routing (v2.6.5)
- `SituationSetTraceLogLevel` — Initialization State Management (v2.3.40)
- `SituationSetVSync` — Window State Management
- `SituationSetVirtualDisplayDirty` — Virtual Displays (Render Targets)
- `SituationSetVirtualDisplayScalingMode` — Virtual Displays (Render Targets)
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
- `SituationShowMessageBox` — Initialization State Management (v2.3.40)
- `SituationShutdown` — Application Lifecycle & State
- `SituationSoundCopy` — Sound Data Manipulation (Wave Utilities)
- `SituationSoundCrop` — Sound Data Manipulation (Wave Utilities)
- `SituationSoundExportAsWav` — Sound Data Manipulation (Wave Utilities)
- `SituationStartAudioCapture` — Audio Capture
- `SituationStartAudioCaptureEx` — Audio Capture
- `SituationStartMidiLearn` — Learning Operations
- `SituationStopAllLoadedSounds` — Audio Handle API
- `SituationStopAllTones` — Audio Handle API
- `SituationStopAudioCapture` — Audio Capture
- `SituationStopLoadedSound` — Audio Handle API
- `SituationStopTone` — Audio Handle API
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
- `SituationUnloadAudio` — Audio Handle API
- `SituationUnloadDroppedFiles` — Cursor, Clipboard and File Drops
- `SituationUnloadFont` — Font Management
- `SituationUnloadImage` — Image Loading and Unloading
- `SituationUnloadModel` — 3D Model Utilities
- `SituationUnloadShader` — Shader Management
- `SituationUnloadSound` — Audio Handle API
- `SituationUpdate` — Application Lifecycle & State
- `SituationUpdateBuffer` — GPU Buffer Management
- `SituationUpdateTimers` — Application Lifecycle & State
- `SituationValidateShaderUniforms` — Shader Interaction & Synchronization
- `SituationVirtualMidiControlChange` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiNoteOff` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiNoteOffEx` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiNoteOn` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiNoteOnEx` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiPitchBend` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationVirtualMidiProgramChange` — Virtual MIDI loopback (integration testing; no hardware keyboard required)
- `SituationWaitForAllJobs` — CPU & Thread Management
- `SituationWaitForJob` — CPU & Thread Management
- `SituationWindowShouldClose` — Application Lifecycle & State
