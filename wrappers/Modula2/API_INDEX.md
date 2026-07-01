# Situation Modula-2 bindings — API index

_Generated 2026-06-25 18:33 UTC from `sit/situation_api.h` — Situation **2.4.357 (Consolidate color-space math into situation_impl_color.h; trace table regenerated.)**._

**Foreign imports:** 598

| Function | Section | Modula-2 | Notes |
|----------|---------|----------|-------|
| `SituationAcquireFrameCommandBuffer` | Frame Lifecycle & Command Buffer | auto | [GL+VK] Prepare the backend for a new frame of rendering commands. |
| `SituationAddJobDependencies` | CPU & Thread Management | auto | Adds multiple dependencies for a single dependent job. |
| `SituationAddJobDependency` | CPU & Thread Management | auto | Adds a dependency between two jobs (prereq -> dependent). |
| `SituationApplyCurrentProfileWindowState` | Advanced Window Profile Management | auto | Manually apply the appropriate window state profile based on current focus. |
| `SituationAttachAudioProcessor` | Custom Audio Processing | auto | Attach a custom DSP processor to a sound's effect chain. |
| `SituationAutoConnectMidi` | MIDI Device Control | auto | Convenience: auto-select first available MIDI input. Equivalent to EnableMidiCon... |
| `SituationBakeBitmapFontAtlas` | Font Management | auto | Upload bitmap_data as a NEAREST-filtered grid atlas for GPU text. |
| `SituationBakeFontAtlas` | Font Management | auto | Rasterize a font into a GPU-ready atlas at the given size. |
| `SituationBeginLoadShaderFromMemory` | Shader Management | auto | Start non-blocking GLSL load: [OpenGL] async compile/link; [Vulkan] shaderc on w... |
| `SituationBeginLoadShaderFromSpirvMemory` | Shader Management | auto | [Vulkan] Non-blocking pipeline build from in-memory SPIR-V (bytecode copied). [O... |
| `SituationBeginLoadShaderFromSpirvMemoryEx` | Shader Management | auto | [Vulkan] Async SPIR-V with layout profile (e.g. UBO_SSBO for Demon Hunt). [OpenG... |
| `SituationBindShaderStorageBlock` | Shader Interaction & Synchronization | auto | [OpenGL] glShaderStorageBlockBinding for SPIR-V when reflection reports binding ... |
| `SituationBindUniformBlock` | Shader Interaction & Synchronization | auto | [OpenGL] glUniformBlockBinding for std140 UBO blocks (layout(binding=N)). |
| `SituationBlitRawDataToImage` | Image Generation & Copying | auto | Copies raw byte data into a specific region of an image. |
| `SituationBuildNumaNodeMask` | CPU & Thread Management | auto | All logical CPUs on a NUMA node |
| `SituationBuildPhysicalCoreMask` | CPU & Thread Management | auto | All logical CPUs on one physical core |
| `SituationBuildUniqueCoreMask` | CPU & Thread Management | auto | One LP per core |
| `SituationCameraBuildInvViewProj` | Camera & Projection Math | auto | — |
| `SituationCameraBuildProj` | Camera & Projection Math | auto | — |
| `SituationCameraBuildView` | Camera & Projection Math | auto | — |
| `SituationCameraBuildViewProj` | Camera & Projection Math | auto | — |
| `SituationCameraUnprojectPixel` | Camera & Projection Math | auto | — |
| `SituationCancelMidiLearn` | Learning Operations | auto | Cancel an active learn operation. |
| `SituationCheckHotReloads` | Node Graph SFX Routing (v2.6.5) | auto | Checks all tracked resources for file changes and reloads them if necessary. |
| `SituationClearAllMidiMappings` | Mapping Management | auto | Clear all learned mappings for a node. |
| `SituationClearMidiMapping` | Mapping Management | auto | Clear a specific learned CC mapping. |
| `SituationClearWindowState` | Window State Management | auto | Clear window configuration state flags. |
| `SituationCmdBeginDebugGroup` | Command Buffer Recording | auto | — |
| `SituationCmdBeginRenderPass` | Abstracted Rendering Commands | auto | [GL+VK] Begins a render pass with detailed configuration. |
| `SituationCmdBeginRenderToDisplay` | Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) | auto | — |
| `SituationCmdBindComputeBuffer` | Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) | auto | — |
| `SituationCmdBindComputePipeline` | Compute Shader Pipeline | auto | Bind a compute pipeline for a subsequent dispatch. |
| `SituationCmdBindComputeTexture` | Abstracted Rendering Commands | auto | [Core] Binds a texture as a storage image for compute shaders. |
| `SituationCmdBindDescriptorSet` | Abstracted Rendering Commands | auto | [GL+VK] [Core] Binds a buffer's descriptor set (UBO/SSBO) to a set index. |
| `SituationCmdBindDescriptorSetDynamic` | Abstracted Rendering Commands | auto | [GL+VK] [Core] Binds a dynamic buffer descriptor set with an offset. |
| `SituationCmdBindIndexBuffer` | Abstracted Rendering Commands | auto | [Core] Bind a 32-bit index buffer (SIT_INDEX_UINT32). Pass offset 0 when indices... |
| `SituationCmdBindIndexBufferEx` | Abstracted Rendering Commands | auto | [Core] Bind index buffer with 16- or 32-bit element type for subsequent Situatio... |
| `SituationCmdBindMeshPullBuffers` | Graphics Resource Management | auto | [VK+GL] Push mesh vertex/index BDA block for pull shaders (SituationMeshPullPush... |
| `SituationCmdBindPipeline` | Abstracted Rendering Commands | auto | [GL+VK] Binds a graphics pipeline (shader program) for subsequent draws. |
| `SituationCmdBindSampledTexture` | Abstracted Rendering Commands | auto | Binds a texture as a sampled image (sampler2D) to a binding point. |
| `SituationCmdBindTexture` | Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) | auto | — |
| `SituationCmdBindTextureSet` | Abstracted Rendering Commands | auto | [GL+VK] [Core] Binds a texture's descriptor set (sampler/storage) to a set index... |
| `SituationCmdBindUniformBuffer` | Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) | auto | — |
| `SituationCmdBindVertexBuffer` | Abstracted Rendering Commands | auto | [Core] Bind a vertex buffer for subsequent SituationCmdDraw / SituationCmdDrawIn... |
| `SituationCmdBlitTexture` | Texture Management | auto | Blit between color 2D textures; caller owns explicit texture barriers. |
| `SituationCmdBufferBarrier` | Compute Shader Pipeline | auto | Record an explicit buffer-range memory barrier. |
| `SituationCmdClear` | Abstracted Rendering Commands | auto | Mid-pass clear of active render-pass attachments; begin-pass clears use Situatio... |
| `SituationCmdClearColor` | Abstracted Rendering Commands | auto | Mid-pass clear of the active color attachment. |
| `SituationCmdClearDepth` | Abstracted Rendering Commands | auto | Mid-pass clear of the active depth attachment. |
| `SituationCmdClearDepthStencil` | Abstracted Rendering Commands | auto | Mid-pass clear of active depth and stencil attachments. |
| `SituationCmdClearStencil` | Abstracted Rendering Commands | auto | Mid-pass clear of the active stencil attachment when supported by backend/attach... |
| `SituationCmdCopyBuffer` | GPU Buffer Management | auto | Legacy buffer-copy command (now returns error). |
| `SituationCmdCopyBufferEx` | GPU Buffer Management | auto | Error-returning buffer-copy command with independent source/destination offsets. |
| `SituationCmdCopyBufferToTexture` | Texture Management | auto | Upload tightly packed RGBA8 rows from a buffer into a texture subregion; caller ... |
| `SituationCmdCopyTexture` | Texture Management | auto | Exact-size copy between color 2D textures; caller owns explicit texture barriers... |
| `SituationCmdCopyTextureToBuffer` | Texture Management | auto | Copy a texture subregion into a buffer (`dst_row_pitch` 0 = width * 4); caller o... |
| `SituationCmdDispatch` | Compute Shader Pipeline | auto | Record a command to dispatch compute shader work groups. |
| `SituationCmdDispatchEx` | Compute Shader Pipeline | auto | Record a compute dispatch with validation and error reporting. |
| `SituationCmdDispatchIndirect` | Compute Shader Pipeline | auto | Record an indirect compute dispatch. |
| `SituationCmdDraw` | Abstracted Rendering Commands | auto | [GL+VK] [Core] Record a non-indexed draw call. |
| `SituationCmdDrawIndexed` | Abstracted Rendering Commands | auto | [GL+VK] [Core] Record an indexed draw call. |
| `SituationCmdDrawIndexedIndirect` | Abstracted Rendering Commands | auto | [Core] Indexed indirect draw (32-bit indices; requires bound index buffer). firs... |
| `SituationCmdDrawIndirect` | Abstracted Rendering Commands | auto | [Core] Draw from a CPU/GPU-filled SituationDrawIndirectCommand in an indirect bu... |
| `SituationCmdDrawMesh` | Abstracted Rendering Commands | auto | [High-Level] Records a command to draw a complete, pre-configured mesh. |
| `SituationCmdDrawQuad` | Abstracted Rendering Commands | auto | [High-Level] Record a command to draw a simple, colored 2D quad. |
| `SituationCmdDrawText` | Abstracted Rendering Commands | auto | Draws a text string using GPU-accelerated textured quads. |
| `SituationCmdDrawTextBoxed` | Abstracted Rendering Commands | auto | Text clipped to a rectangle with optional word wrap. |
| `SituationCmdDrawTextEx` | Abstracted Rendering Commands | auto | Advanced text drawing (scaling/spacing). |
| `SituationCmdDrawTexture` | Abstracted Rendering Commands | auto | [High-Level] Draw a part of a texture defined by a rectangle. |
| `SituationCmdDrawTextureYpqGrade` | Abstracted Rendering Commands | auto | [High-Level] Draw texture with YPQ grade (matches SituationImageAdjustYPQ). |
| `SituationCmdEndDebugGroup` | Command Buffer Recording | auto | — |
| `SituationCmdEndRender` | Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) | auto | — |
| `SituationCmdEndRenderPass` | Abstracted Rendering Commands | auto | [GL+VK] Ends the current render pass. |
| `SituationCmdPipelineBarrier` | Shader Interaction & Synchronization | auto | Legacy convenience barrier; prefer SituationCmdPipelineBarrierEx, SituationCmdBu... |
| `SituationCmdPipelineBarrierEx` | Compute Shader Pipeline | auto | Record an explicit global memory barrier. |
| `SituationCmdPopRasterState` | Command Buffer Recording | auto | — |
| `SituationCmdPresent` | Abstracted Rendering Commands | auto | Submits a command to copy a texture to the main window's swapchain (Compute-Only... |
| `SituationCmdPushRasterState` | Command Buffer Recording | auto | — |
| `SituationCmdSetBlendEnable` | Command Buffer Recording | auto | — |
| `SituationCmdSetBlendFuncSeparate` | Command Buffer Recording | auto | — |
| `SituationCmdSetColorWriteMask` | Command Buffer Recording | auto | — |
| `SituationCmdSetCullMode` | Command Buffer Recording | auto | — |
| `SituationCmdSetDepthBias` | Command Buffer Recording | auto | — |
| `SituationCmdSetDepthTest` | Command Buffer Recording | auto | — |
| `SituationCmdSetDepthWrite` | Command Buffer Recording | auto | — |
| `SituationCmdSetFrontFace` | Command Buffer Recording | auto | — |
| `SituationCmdSetLineWidth` | Command Buffer Recording | auto | — |
| `SituationCmdSetMultisampleState` | Command Buffer Recording | auto | — |
| `SituationCmdSetPolygonMode` | Command Buffer Recording | auto | — |
| `SituationCmdSetPrimitiveTopology` | Command Buffer Recording | auto | — |
| `SituationCmdSetPushConstant` | Abstracted Rendering Commands | auto | [Core] Set a small block of per-draw uniform data (push constant). |
| `SituationCmdSetPushConstantData` | Command Buffer Recording | auto | — |
| `SituationCmdSetScissor` | Abstracted Rendering Commands | auto | Sets the dynamic scissor rectangle to clip rendering. |
| `SituationCmdSetScissorIndexed` | Abstracted Rendering Commands | auto | Sets scissor at index (0 = default scissor). |
| `SituationCmdSetStencilTest` | Command Buffer Recording | auto | — |
| `SituationCmdSetVertexAttribute` | Abstracted Rendering Commands | auto | [OpenGL Only, Deprecated v2.4] Attribute format + vertex buffer binding index. P... |
| `SituationCmdSetViewport` | Abstracted Rendering Commands | auto | Sets the dynamic viewport and scissor for the current render pass. |
| `SituationCmdSetViewportIndexed` | Abstracted Rendering Commands | auto | Sets viewport at index (0 = default viewport). |
| `SituationCmdTextureBarrier` | Compute Shader Pipeline | auto | Record an explicit texture layout/memory barrier. |
| `SituationColorFromYPQ` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Converts a YPQA color back to the standard RGBA color space. |
| `SituationColorFromYPQf` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Float YPQ → RGBA (linear YIQ, clamped RGB). |
| `SituationColorRgbaToHdrPqClear` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | sRGB 0–255 → PQ clear color as RGBA floats×255 for debugging. |
| `SituationColorToYPQ` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Converts a standard RGBA color to the YPQA (Luma, Phase, Quadrature) color space... |
| `SituationColorToYPQf` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | RGBA → normalized float YPQ (no 8-bit quantize). |
| `SituationConfigureVirtualDisplay` | Virtual Displays (Render Targets) | auto | Configure a virtual display's properties. |
| `SituationConsumeKeyPress` | Keyboard Input | auto | Eat a key press this frame so later IsKeyPressed() returns false (e.g. for globa... |
| `SituationConvertColorToVector4` | Color Space Conversions | auto | Convert an 8-bit ColorRGBA struct to a normalized Vector4. |
| `SituationCopyFile` | File Operations | auto | Copy a file. |
| `SituationCreateASCIIFont` | Font Management | auto | — |
| `SituationCreateBuffer` | GPU Buffer Management | auto | Create a generic GPU data buffer (e.g., SSBO). |
| `SituationCreateCP437Font` | Font Management | auto | — |
| `SituationCreateComputePipeline` | Compute Shader Pipeline | auto | Create a compute pipeline from a shader file. |
| `SituationCreateComputePipelineFromMemory` | Compute Shader Pipeline | auto | Create a compute pipeline from in-memory GLSL source. |
| `SituationCreateDirectory` | Directory Operations | auto | Create a directory, optionally creating parent directories. |
| `SituationCreateGraph` | Node Graph Functions | auto | Create a new audio processing graph. |
| `SituationCreateImage` | Image Generation & Copying | auto | Allocates a new SituationImage container with UNINITIALIZED data. |
| `SituationCreateMesh` | Graphics Resource Management | auto | Create a mesh from vertex and index data. |
| `SituationCreateMeshEx` | Graphics Resource Management | auto | Create a mesh with an explicit vertex layout tag (includes SIT_MESH_LAYOUT_PULL)... |
| `SituationCreateNode` | Node Graph Functions | auto | Create a node of the given type in the graph. |
| `SituationCreateOutlinedPackedBitmapFont` | Font Management | auto | — |
| `SituationCreatePackedBitmapFont` | Font Management | auto | — |
| `SituationCreatePatch` | Node Graph Functions | auto | Connect an output port to an input port. |
| `SituationCreateReadbackBuffer` | GPU Buffer Management | auto | [Phase 1] Create an async GPU->CPU staging buffer. |
| `SituationCreateTerminalFontEx` | Font Management | auto | — |
| `SituationCreateTerminalFontFromMemory` | Font Management | auto | — |
| `SituationCreateTexture` | Texture Management | auto | Create a texture from a CPU-side image. |
| `SituationCreateTextureEx` | Texture Management | auto | Create a texture with specific usage flags. |
| `SituationCreateThreadPool` | CPU & Thread Management | auto | Initializes the thread pool with dual-priority queues and worker threads. |
| `SituationCreateVCRFont` | Font Management | auto | — |
| `SituationCreateVCRFontWithOutline` | Font Management | auto | — |
| `SituationCreateVGA8x8Font` | Font Management | auto | — |
| `SituationCreateVGA8x8FontWithOutline` | Font Management | auto | — |
| `SituationCreateVirtualDisplay` | Virtual Displays (Render Targets) | auto | Create an off-screen render target. |
| `SituationCreateVirtualDisplayEx` | Virtual Displays (Render Targets) | auto | Create a virtual display with extended flags (e.g. COMPUTE_TARGET for compute sh... |
| `SituationCreateVirtualDisplayFromDesc` | Virtual Displays (Render Targets) | auto | Create a virtual display from a full desc (VD-1 attachment config). |
| `SituationDeleteDirectory` | Directory Operations | auto | Delete a directory, optionally deleting all its contents. |
| `SituationDeleteFile` | File Operations | auto | Delete a file. |
| `SituationDeserializeGraphFromJSON` | Graph Serialization Functions | auto | Deserialize a graph from a JSON string. |
| `SituationDestroyBuffer` | GPU Buffer Management | auto | Destroy a GPU buffer. |
| `SituationDestroyComputePipeline` | Compute Shader Pipeline | auto | Destroy a compute pipeline and free its GPU resources. |
| `SituationDestroyGraph` | Node Graph Functions | auto | Destroy a graph and all its nodes/patches. |
| `SituationDestroyMesh` | Graphics Resource Management | auto | Unload a mesh from GPU memory. |
| `SituationDestroyNode` | Node Graph Functions | auto | Remove and destroy a node from the graph. |
| `SituationDestroyPatch` | Node Graph Functions | auto | Disconnect a patch between two ports (legacy, no is_control param). |
| `SituationDestroyTexture` | Texture Management | auto | Unload a texture from GPU memory. |
| `SituationDestroyThreadPool` | CPU & Thread Management | auto | Shuts down the thread pool and releases resources. |
| `SituationDestroyVirtualDisplay` | Virtual Displays (Render Targets) | auto | Destroy a virtual display. |
| `SituationDetachAudioProcessor` | Custom Audio Processing | auto | Detach a custom DSP processor from a sound. |
| `SituationDirectoryExists` | File & Directory Queries | auto | Check if a directory exists at the given path. |
| `SituationDisableCursor` | Cursor, Clipboard and File Drops | auto | Hide and lock the cursor, providing raw mouse motion. |
| `SituationDisableMidiControl` | MIDI Device Control | auto | Disable MIDI control for a node. |
| `SituationDisableMidiLearn` | MIDI Learn Lifecycle | auto | Disable MIDI Learn for a node. |
| `SituationDispatchParallel` | CPU & Thread Management | auto | Executes a loop in parallel across worker threads (Fork-Join). |
| `SituationDrawMetricsOverlay` | Profiling & Diagnostics | auto | Draws FPS, Latency, and Memory stats |
| `SituationDrawModel` | 3D Model Utilities | auto | Draws all sub-meshes of a model with a single root transformation. |
| `SituationDumpTaskGraph` | CPU & Thread Management | auto | Prints the current task graph state to the stream. |
| `SituationDumpThreadPoolMetrics` | CPU & Thread Management | auto | Metrics-only dump |
| `SituationDumpThreadPoolStatus` | CPU & Thread Management | auto | Pool metrics + per-role CPU snapshot |
| `SituationDumpThreadingReport` | CPU & Thread Management | auto | Status + topology line + pool dump |
| `SituationEnableMidiControl` | MIDI Device Control | auto | Enable MIDI CC control for a node. Pass device_id=-1 for auto-select. |
| `SituationEnableMidiLearn` | MIDI Learn Lifecycle | auto | Enable MIDI Learn capability for a node. MIDI must already be enabled. |
| `SituationEndFrame` | Frame Lifecycle & Command Buffer | auto | [GL+VK] Submit all commands for the frame and present the result. |
| `SituationEnumerateAudioDevices` | Device Enumeration (Phase 0) | auto | [Caller frees via SituationFreeDeviceList] Canonical device enumeration. |
| `SituationErrorToString` | Callbacks and Event Handling | auto | Human-readable base label for an error code (from the errno table). |
| `SituationExecuteCommand` | Active Audio Device Query (v2.4.199) | auto | Execute a shell command hidden, return exit code & combined output. |
| `SituationExportImage` | Image Exporting | auto | Export image data (.png, .bmp, .jpg, .tga, .hdr). |
| `SituationExportRenderHistogram` | Profiling & Diagnostics | auto | Write a text-based frame time histogram into buf. |
| `SituationFileExists` | File & Directory Queries | auto | Check if a file exists at the given path. |
| `SituationFindBestDevice` | Device Enumeration (Phase 0) | auto | Find the best matching device by type and channel requirements. |
| `SituationFreeDeviceList` | Device Enumeration (Phase 0) | auto | Free a device list returned by SituationEnumerateAudioDevices. |
| `SituationFreeDirectoryFileList` | Directory Operations | auto | Free the memory allocated by SituationListDirectoryFiles. |
| `SituationFreeDisplays` | Physical Display (Monitor) Management | auto | Free a display info array returned by SituationGetDisplays. |
| `SituationFreeJSONString` | Graph Serialization Functions | auto | Free a JSON string returned by SituationSerializeGraphToJSON. |
| `SituationFreeProcessList` | Process Enumeration (v2.4.199) | auto | Free a process list returned by SituationGetProcessList(). |
| `SituationFreeString` | Device Function Table (for processing) | auto | Free a string allocated by the library (e.g., from path helpers). |
| `SituationGenImageColor` | Image Generation & Copying | auto | Generate a new image of a solid color. |
| `SituationGenImageGradient` | Image Generation & Copying | auto | Generate a new image with a gradient. |
| `SituationGetActiveAudioDeviceName` | Active Audio Device Query (v2.4.199) | auto | Get the name of the currently active playback device (static buffer, do not free... |
| `SituationGetActiveGraph` | Active Graph (Audio Callback Integration) | auto | Get the currently active audio processing graph (NULL if none). |
| `SituationGetActiveJobCount` | CPU & Thread Management | auto | active_jobs counter |
| `SituationGetAppSavePath` | Path Management & Special Directories | auto | Get a safe, persistent path for saving application data (caller must free). |
| `SituationGetArgumentValue` | Command-Line Argument Queries | auto | Get the value of an argument (e.g., "jungle" from "-level:jungle"). |
| `SituationGetAudioDevices` | Audio | auto | — |
| `SituationGetAudioMasterVolume` | Audio Device Management | auto | Get the master volume for the audio device. |
| `SituationGetAudioPlaybackSampleRate` | Audio Device Management | auto | Get the sample rate of the current audio device. |
| `SituationGetBasePath` | Path Management & Special Directories | auto | Get the path to the directory containing the executable (caller must free). |
| `SituationGetBufferData` | GPU Buffer Management | auto | Read data from a GPU buffer (blocking). |
| `SituationGetBufferDeviceAddress` | Graphics Resource Management | auto | Retrieves the GPU device address of a buffer for bindless access. |
| `SituationGetCPUCoreCount` | CPU & Thread Management | auto | Gets physical processors (Cores) from cached topology |
| `SituationGetCPUInfo` | System & Hardware Information (split queries; v2.4.207) | auto | CPU name, core/thread counts, and clock speed. |
| `SituationGetCPUThreadCount` | CPU & Thread Management | auto | Logical processor count (cached topology). |
| `SituationGetCategoryName` | Device Registry Functions | auto | Get the display name for a device category. |
| `SituationGetCharFromScancode` | Keyboard Input | auto | Maps a physical key scancode (plus modifiers) to a Unicode character, respecting... |
| `SituationGetCharPressed` | Keyboard Input | auto | Get the next character from the text input queue. |
| `SituationGetClipboardText` | Cursor, Clipboard and File Drops | auto | Get text from the system clipboard. |
| `SituationGetComputeCommandBuffer` | Frame Lifecycle & Command Buffer | auto | [VK] Get the compute-specific command buffer (Vulkan only). |
| `SituationGetConfiguredAudioThreadAffinity` | CPU & Thread Management | auto | Effective audio mask (init or default) |
| `SituationGetConfiguredIOThreadAffinity` | CPU & Thread Management | auto | Effective I/O mask (init or default CPU 3) |
| `SituationGetConfiguredMainThreadAffinity` | CPU & Thread Management | auto | Init mask for main thread (0 = no pin) |
| `SituationGetConfiguredRenderThreadAffinity` | CPU & Thread Management | auto | Effective render mask (init or default) |
| `SituationGetControl` | Node Graph Functions | auto | Get the current value of a node's control parameter. |
| `SituationGetCpuTopology` | CPU & Thread Management | auto | Pointer to cached topology (NULL on failure) |
| `SituationGetCurrentActualWindowStateFlags` | Advanced Window Profile Management | auto | Gets flags based on current GLFW window state |
| `SituationGetCurrentDriveLetter` | Active Audio Device Query (v2.4.199) | auto | Get the drive letter of the running executable (Windows only). |
| `SituationGetCurrentMonitor` | Physical Display (Monitor) Management | auto | Get the index of the monitor the window is on. |
| `SituationGetCurrentProcessorIndex` | CPU & Thread Management | auto | Logical CPU index for current thread, or -1 if unknown |
| `SituationGetDeviceInfo` | System | auto | — |
| `SituationGetDeviceMetadata` | Device Registry Functions | auto | Get metadata for a registered device type. |
| `SituationGetDisplays` | Physical Display (Monitor) Management | auto | Get information for all displays (caller must free). |
| `SituationGetDrawCallCount` | Profiling & Diagnostics | auto | Number of draw commands this frame |
| `SituationGetDriveInfo` | Active Audio Device Query (v2.4.199) | auto | Get info for a specific drive (Windows only). |
| `SituationGetFPS` | Frame Timing & FPS Management | auto | Get the current frames-per-second value. |
| `SituationGetFileExtension` | Path Management & Special Directories | auto | Extract the file extension from a path. |
| `SituationGetFileModTime` | File & Directory Queries | auto | Get the last modification time of a file (Unix timestamp). |
| `SituationGetFileName` | Path Management & Special Directories | auto | Extract the file name (including extension) from a full path. |
| `SituationGetFrameSpikeCount` | Profiling & Diagnostics | auto | Count of detected frame spikes (general debugging aid) |
| `SituationGetFrameTime` | Frame Timing & FPS Management | auto | Get the time in seconds for the last frame to complete (deltaTime). |
| `SituationGetGLFWwindow` | Backend-Specific Accessors | auto | Get the raw GLFW window handle. |
| `SituationGetGPUInfo` | System & Hardware Information (split queries; v2.4.207) | auto | GPU name and dedicated VRAM (when available). |
| `SituationGetGPUName` | System & Hardware Information (split queries; v2.4.207) | auto | Get the name of the active GPU. |
| `SituationGetGamepadAxisCount` | Gamepad Input | auto | Get the number of axes for a gamepad. |
| `SituationGetGamepadAxisValue` | Gamepad Input | auto | Get the value of a gamepad axis (deadzone applied). |
| `SituationGetGamepadButtonPressed` | Gamepad Input | auto | Get the next gamepad button from the press queue. |
| `SituationGetGraphicsBackend` | Active Audio Device Query (v2.4.199) | auto | — |
| `SituationGetGraphicsBackendName` | Active Audio Device Query (v2.4.199) | auto | — |
| `SituationGetGraphicsCaps` | Active Audio Device Query (v2.4.199) | auto | Get backend capabilities for examples/frameworks. |
| `SituationGetHighQueueDepth` | CPU & Thread Management | auto | High-priority queue depth |
| `SituationGetIOQueueDepth` | Profiling & Diagnostics | auto | [v2.3.37] Get the current depth of the IO/Low Priority queue |
| `SituationGetInitState` | Application Lifecycle & State | auto | Query the current initialization state (thread-safe). |
| `SituationGetInputDeviceCount` | System & Hardware Information (split queries; v2.4.207) | auto | Number of input devices (keyboard/mouse/gamepad). |
| `SituationGetInputDeviceName` | System & Hardware Information (split queries; v2.4.207) | auto | — |
| `SituationGetInternalThreadPool` | CPU & Thread Management | auto | Returns pointer to the library's internal thread pool (NULL if not initialized). |
| `SituationGetJoystickName` | Gamepad Input | auto | Get the human-readable name of a joystick/gamepad. |
| `SituationGetKeyPressed` | Keyboard Input | auto | Get the next key from the press queue (no repeats). |
| `SituationGetKeyPressedEx` | Keyboard Input | auto | Get the next key and its scancode from the queue. |
| `SituationGetKeyScancode` | Keyboard Input | auto | Get the platform-specific scancode for a logical key. |
| `SituationGetLastErrorCode` | Callbacks and Event Handling | auto | Get the SituationError enum from the most recent _SituationSetErrorFromCode call... |
| `SituationGetLastErrorMsg` | Callbacks and Event Handling | auto | Get the last error message as a string (caller must free). |
| `SituationGetLastFramePhases` | Profiling & Diagnostics | auto | — |
| `SituationGetLastVDCompositeTimeMS` | Virtual Displays (Render Targets) | auto | Get the time taken for the last virtual display composite pass. |
| `SituationGetMainCommandBuffer` | Frame Lifecycle & Command Buffer | auto | [GL+VK] Get the primary command buffer for the current frame. |
| `SituationGetMainWindowRenderPass` | Backend-Specific Accessors | auto | Get the render pass for the main window. |
| `SituationGetMasterOutputMeter` | Audio Output Monitoring (for visualization) | auto | Last playback callback block: peak sample magnitude & RMS (optional pointers; sa... |
| `SituationGetMaxComputeWorkGroups` | Compute Shader Pipeline | auto | Query maximum compute work group count per dispatch. |
| `SituationGetMaxFrameTime` | Profiling & Diagnostics | auto | Highest observed frame delta (general spike debugging) |
| `SituationGetMemoryInfo` | System & Hardware Information (split queries; v2.4.207) | auto | Total and available physical RAM. |
| `SituationGetMeshData` | 3D Model Utilities | auto | Get raw vertex/index data pointers from a mesh (read-only). |
| `SituationGetMeshIndexBufferAddress` | Graphics Resource Management | auto | Retrieves the GPU device address of the mesh index buffer. [VK] requires SIT_FEA... |
| `SituationGetMeshVertexBufferAddress` | Graphics Resource Management | auto | GPU VA of mesh vertex buffer. [VK] SIT_FEATURE_BINDLESS_BUFFERS; pull draw: Situ... |
| `SituationGetMeshVertexLayout` | Graphics Resource Management | auto | Query layout tag stored at creation. |
| `SituationGetMidiDeviceName` | MIDI Device Control | auto | PortMidi device name for device_id (hardware or virtual). |
| `SituationGetMonitorCount` | Physical Display (Monitor) Management | auto | Get the number of connected monitors. |
| `SituationGetMonitorHeight` | Physical Display (Monitor) Management | auto | Get the height of a monitor's current video mode. |
| `SituationGetMonitorName` | Physical Display (Monitor) Management | auto | Get the human-readable name of a monitor. |
| `SituationGetMonitorPhysicalHeight` | Physical Display (Monitor) Management | auto | Get the physical height of a monitor in millimeters. |
| `SituationGetMonitorPhysicalWidth` | Physical Display (Monitor) Management | auto | Get the physical width of a monitor in millimeters. |
| `SituationGetMonitorPosition` | Physical Display (Monitor) Management | auto | Get the top-left position of a monitor on the desktop. |
| `SituationGetMonitorRefreshRate` | Physical Display (Monitor) Management | auto | Get the refresh rate of a monitor. |
| `SituationGetMonitorWidth` | Physical Display (Monitor) Management | auto | Get the width of a monitor's current video mode. |
| `SituationGetMouseDelta` | Mouse Input | auto | Get the mouse movement since the last frame. |
| `SituationGetMousePosition` | Mouse Input | auto | Get the mouse position within the window. |
| `SituationGetMouseWheelMove` | Mouse Input | auto | Get vertical mouse wheel movement. |
| `SituationGetMouseWheelMoveV` | Mouse Input | auto | Get vertical and horizontal mouse wheel movement. |
| `SituationGetNetworkAdapterCount` | System & Hardware Information (split queries; v2.4.207) | auto | Number of network adapters reported by the OS. |
| `SituationGetNetworkAdapterName` | System & Hardware Information (split queries; v2.4.207) | auto | — |
| `SituationGetNode` | Node Graph Functions | auto | Get a direct pointer to a node (for advanced use). |
| `SituationGetNodePCMFreeFrames` | PCM Input Node (user-fed ring buffer source) | auto | Query how many frames of space are available in the PCM_INPUT node's ring buffer... |
| `SituationGetNumaTopology` | CPU & Thread Management | auto | Cached NUMA snapshot |
| `SituationGetOSInfo` | OS Information (v2.4.199) | auto | Get operating system name, version, and build number. |
| `SituationGetPreferredNumaNode` | CPU & Thread Management | auto | TLS: node for current thread, or -1 if unset |
| `SituationGetProcessList` | Process Enumeration (v2.4.199) | auto | Get snapshot of running OS processes. Caller must free with SituationFreeProcess... |
| `SituationGetQueueDepth` | CPU & Thread Management | auto | Pending jobs per queue mask |
| `SituationGetRecommendedWorkerCount` | CPU & Thread Management | auto | Sizing helper (no pool required) |
| `SituationGetRegisteredDeviceCount` | Device Registry Functions | auto | Get the number of registered audio device types. |
| `SituationGetRenderHeight` | Window & Screen Dimension Queries | auto | Get the current render height (backbuffer size, considers HiDPI). |
| `SituationGetRenderLatencyStats` | Profiling & Diagnostics | auto | Get render thread latency metrics |
| `SituationGetRenderQueueDepth` | Profiling & Diagnostics | auto | Get the current depth of the render queue |
| `SituationGetRenderWidth` | Window & Screen Dimension Queries | auto | Get the current render width (backbuffer size, considers HiDPI). |
| `SituationGetRendererType` | Backend-Specific Accessors | auto | Legacy — prefer SituationGetGraphicsBackend() + SituationGetGraphicsCaps(). |
| `SituationGetScreenHeight` | Window & Screen Dimension Queries | auto | Get the current logical height of the window. |
| `SituationGetScreenWidth` | Window & Screen Dimension Queries | auto | Get the current logical width of the window. |
| `SituationGetScreenshotFormat` | Image & Screenshot Utilities | auto | Get the current default screenshot format. |
| `SituationGetSerializationVersion` | Graph Serialization Functions | auto | Get the current serialization format version string. |
| `SituationGetSoundPan` | Sound Parameters and Effects | auto | Get the stereo pan of a sound. |
| `SituationGetSoundPitch` | Sound Parameters and Effects | auto | Get the pitch of a sound. |
| `SituationGetSoundVolume` | Sound Parameters and Effects | auto | Get the volume of a specific sound. |
| `SituationGetStorageDevice` | System & Hardware Information (split queries; v2.4.207) | auto | — |
| `SituationGetStorageDeviceCount` | System & Hardware Information (split queries; v2.4.207) | auto | Number of storage volumes reported by the OS. |
| `SituationGetTextLineCount` | Font Management | auto | Lines required for width constraint (grid/TTF). |
| `SituationGetTextureHandle` | Graphics Resource Management | auto | Retrieves the bindless texture handle (OpenGL Only). |
| `SituationGetTextureInfo` | Texture Management | auto | [Phase 2] Query texture metadata. |
| `SituationGetThreadAffinity` | CPU & Thread Management | auto | Reads affinity mask for the CURRENT thread |
| `SituationGetThreadNumaNode` | CPU & Thread Management | auto | NUMA node for current thread, or -1 if unknown |
| `SituationGetThreadPoolMetrics` | CPU & Thread Management | auto | Scheduler counters snapshot |
| `SituationGetThreadPoolSnapshot` | CPU & Thread Management | auto | Worker/I/O/render/audio placement snapshot |
| `SituationGetThreadingStatus` | CPU & Thread Management | auto | Runtime threading capabilities + pool summary |
| `SituationGetUserDirectory` | Active Audio Device Query (v2.4.199) | auto | [Caller frees] Get the full path to the current user's home directory. |
| `SituationGetVRAMUsage` | Profiling & Diagnostics | auto | Total GPU memory allocated (Bytes) |
| `SituationGetVersionString` | Application Lifecycle & State | auto | [Main thread] Returns a read-only static string (e.g., "2.4.336"). Do not free. |
| `SituationGetVirtualDisplay` | Virtual Displays (Render Targets) | auto | Get a pointer to a virtual display's state. |
| `SituationGetVirtualDisplayPatternConfig` | Virtual Displays (Render Targets) | auto | Copy current standby config (no-op if invalid id). |
| `SituationGetVirtualDisplayPatternLayers` | Virtual Displays (Render Targets) | auto | Current standby layer bitmask (0 if invalid id). |
| `SituationGetVirtualDisplaySize` | Virtual Displays (Render Targets) | auto | Get the internal resolution of a virtual display. |
| `SituationGetVirtualDisplayTexture` | Virtual Displays (Render Targets) | auto | Get the VD's internal texture as a SituationTexture handle (valid for compute-ta... |
| `SituationGetVirtualDisplayUpdateInfo` | Virtual Displays (Render Targets) | auto | Query last VD content write (not the frame clock). |
| `SituationGetVulkanDevice` | Backend-Specific Accessors | auto | Get the raw Vulkan logical device handle. |
| `SituationGetVulkanInstance` | Backend-Specific Accessors | auto | Get the raw Vulkan instance handle. |
| `SituationGetVulkanPhysicalDevice` | Backend-Specific Accessors | auto | Get the raw Vulkan physical device handle. |
| `SituationGetWindowPosition` | Window & Screen Dimension Queries | auto | Get the window's top-left position on the screen. |
| `SituationGetWindowScaleDPI` | Window & Screen Dimension Queries | auto | Get the DPI scaling factor for the window. |
| `SituationGetWindowSize` | Window & Screen Dimension Queries | auto | Get the current logical window size. |
| `SituationHasWindowFocus` | Window State Queries | auto | Check if the window is currently focused. |
| `SituationHideCursor` | Cursor, Clipboard and File Drops | auto | Hide the mouse cursor. |
| `SituationHsvToRgb` | Color Space Conversions | auto | Converts a Hue, Saturation, Value color back to the standard RGBA color space. |
| `SituationImageAdjustHSV` | Image Manipulation (Modifies image in-place) | auto | Control an image by Hue Saturation and Brightness. |
| `SituationImageAdjustYPQ` | Image Manipulation (Modifies image in-place) | auto | Grade an image in YPQ (phase/chroma/luma). |
| `SituationImageCopy` | Image Generation & Copying | auto | Create a new image by copying another. |
| `SituationImageCrop` | Image Manipulation (Modifies image in-place) | auto | Crop an image to a specific rectangle. |
| `SituationImageDraw` | Image Generation & Copying | auto | Copying portion of one image into another image at destination placement |
| `SituationImageDrawAlpha` | Image Generation & Copying | auto | Draw a portion of a source image onto dst with alpha tinting. |
| `SituationImageDrawCodepoint` | Font Management | auto | Draw a single Unicode character with advanced styling onto an image. |
| `SituationImageDrawText` | Font Management | auto | Draw a simple, tinted text string onto an image. |
| `SituationImageDrawTextEx` | Font Management | auto | Draw a text string with advanced styling (rotation, outline) onto an image. |
| `SituationImageDrawTextFormatted` | Font Management | manual | variadic C function — wrap manually in Odin |
| `SituationImageFlip` | Image Manipulation (Modifies image in-place) | auto | Flip an image. |
| `SituationImageResize` | Image Manipulation (Modifies image in-place) | auto | Resize an image using default bicubic scaling. |
| `SituationImageStampText` | Font Management | auto | — |
| `SituationImageStampTextBoxed` | Font Management | auto | — |
| `SituationInit` | Application Lifecycle & State | auto | [Main thread] Initialize the library, create window and graphics context. |
| `SituationInitDeviceRegistry` | Device Registry Functions | auto | Initialize the built-in device registry (call once at startup). |
| `SituationIsAppPaused` | Application Lifecycle & State | auto | Check if the application is currently paused. |
| `SituationIsArgumentPresent` | Command-Line Argument Queries | auto | Check if a command-line argument (e.g., "-server") was provided. |
| `SituationIsAudioDevicePlaying` | Audio Device Management | auto | Check if the audio device is currently playing. |
| `SituationIsDeviceRegistered` | Device Registry Functions | auto | Check if a device type is registered. |
| `SituationIsFeatureSupported` | Gamepad Input | auto | Check if a graphics feature is supported on current hardware. |
| `SituationIsFileDropped` | Cursor, Clipboard and File Drops | auto | Check if a file was dropped into the window this frame. |
| `SituationIsGamepad` | Gamepad Input | auto | Check if a connected joystick has a standard gamepad mapping. |
| `SituationIsGamepadButtonDown` | Gamepad Input | auto | Check if a gamepad button is currently held down (a state). |
| `SituationIsGamepadButtonPressed` | Gamepad Input | auto | Check if a gamepad button was pressed down this frame (an event). |
| `SituationIsGamepadButtonReleased` | Gamepad Input | auto | Check if a gamepad button was released this frame (an event). |
| `SituationIsImageValid` | Image Loading and Unloading | auto | Check if an image has been loaded successfully. |
| `SituationIsInitialized` | Application Lifecycle & State | auto | [Main thread] Check if the library has been successfully initialized. |
| `SituationIsJoystickPresent` | Gamepad Input | auto | Check if a joystick/gamepad is connected. |
| `SituationIsKeyDown` | Keyboard Input | auto | Check if a key is currently held down (a state). |
| `SituationIsKeyPressed` | Keyboard Input | auto | Check if a key was pressed down this frame (an event). |
| `SituationIsKeyReleased` | Keyboard Input | auto | Check if a key was released this frame (an event). |
| `SituationIsKeyUp` | Keyboard Input | auto | Check if a key is currently up (a state). |
| `SituationIsLearning` | Learning Operations | auto | Check if currently in learn mode. Returns 1/0. |
| `SituationIsLockKeyPressed` | Keyboard Input | auto | Check if a lock key (Caps, Num) is currently active. |
| `SituationIsMidiEnabled` | MIDI Device Control | auto | Check if a node has MIDI control enabled. Returns 1/0. |
| `SituationIsMidiLearnEnabled` | MIDI Learn Lifecycle | auto | Check if MIDI Learn is enabled. Returns 1/0. |
| `SituationIsModifierPressed` | Keyboard Input | auto | Check if a modifier key (Shift, Ctrl, Alt) is pressed. |
| `SituationIsMouseButtonDown` | Mouse Input | auto | Check if a mouse button is currently held down (a state). |
| `SituationIsMouseButtonPressed` | Mouse Input | auto | Check if a mouse button was pressed down this frame (an event). |
| `SituationIsMouseButtonReleased` | Mouse Input | auto | Check if a mouse button was released this frame. |
| `SituationIsScancodeDown` | Keyboard Input | auto | Check if a physical key (scancode) is currently held down. |
| `SituationIsScrollLockOn` | Keyboard Input | auto | Check if Scroll Lock is currently toggled on. |
| `SituationIsStbImageLoadExtension` | Image Loading and Unloading | auto | True for stb_image decode extensions (.jpg, .png, .bmp, .tga, .psd, .gif, .hdr, ... |
| `SituationIsVersionCompatible` | Graph Serialization Functions | auto | Check if a serialized version is compatible with this library. |
| `SituationIsVirtualDisplayDirty` | Virtual Displays (Render Targets) | auto | Check if a virtual display is marked as dirty. |
| `SituationIsWindowFullscreen` | Window State Queries | auto | Check if the window is currently in fullscreen mode. |
| `SituationIsWindowHidden` | Window State Queries | auto | Check if the window is currently hidden. |
| `SituationIsWindowMaximized` | Window State Queries | auto | Check if the window is currently maximized. |
| `SituationIsWindowMinimized` | Window State Queries | auto | Check if the window is currently minimized. |
| `SituationIsWindowResized` | Window State Queries | auto | Check if the window was resized in the last frame. |
| `SituationIsWindowState` | Window State Queries | auto | Check if a specific window state flag is set. |
| `SituationJoinPath` | Path Management & Special Directories | auto | Join two path components with the correct OS separator (caller must free). |
| `SituationLinearToPq` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Linear display light [0,1] → ST.2084 PQ [0,1]. |
| `SituationListDirectoryFiles` | Directory Operations | auto | List files and subdirectories in a path (caller must free with SituationFreeDire... |
| `SituationListMidiDevices` | MIDI Device Control | auto | List available MIDI input devices. Returns number found. |
| `SituationLoadAudio` | Audio Handle API | auto | Load audio and return a lightweight handle for playback control. |
| `SituationLoadBitmapFontFromMemory` | Font Management | auto | Loads a raw bitmap font (e.g. 8x8 array). |
| `SituationLoadBitmapFontFromTexture` | Font Management | auto | Grid atlas already on GPU — fills layout metadata. |
| `SituationLoadComputeShader` | Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) | auto | — |
| `SituationLoadComputeShaderFromMemory` | Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) | auto | — |
| `SituationLoadDroppedFiles` | Cursor, Clipboard and File Drops | auto | Get the paths of dropped files (returns a copy, caller must free). |
| `SituationLoadFileAsync` | File Operations | auto | Asynchronously load a file. |
| `SituationLoadFileData` | File Operations | auto | Load an entire file into a memory buffer (caller must free). |
| `SituationLoadFileText` | File Operations | auto | Load a text file into a null-terminated string (caller must free). |
| `SituationLoadFileTextAsync` | File Operations | auto | Asynchronously load a text file. |
| `SituationLoadFont` | Font Management | auto | Load a font from a TTF/OTF file for CPU rendering. |
| `SituationLoadFontFromMemory` | Font Management | auto | Loads a font directly from a memory buffer (e.g., embedded resource). |
| `SituationLoadGraphFromFile` | Graph Serialization Functions | auto | Load a graph from a JSON file, re-creating nodes via device_funcs. |
| `SituationLoadImage` | Image Loading and Unloading | auto | Load an image via stb_image (JPEG, PNG, BMP, TGA, PSD, GIF, HDR, PIC, PNM). |
| `SituationLoadImageFromMemory` | Image Loading and Unloading | auto | Load an image from a memory buffer. |
| `SituationLoadImageFromScreen` | Image & Screenshot Utilities | auto | Get a copy of the current screen backbuffer as an image. |
| `SituationLoadMidiPreset` | Preset Persistence | auto | Load MIDI Learn mappings from JSON file. |
| `SituationLoadModel` | 3D Model Utilities | auto | Loads a complete 3D model and its textures from a GLTF file. |
| `SituationLoadModelFromOBJ` | 3D Model Utilities | auto | Wavefront OBJ: triangulated meshes, MTL/textures; missing/degenerate normals fil... |
| `SituationLoadModelFromSTL` | 3D Model Utilities | auto | Loads a 3D model from a binary or ASCII STL file. UVs are zeroed; normals are fl... |
| `SituationLoadShader` | Shader Management | auto | Load a graphics shader pipeline from vertex and fragment files. |
| `SituationLoadShaderFromMemory` | Shader Management | auto | Create a graphics shader pipeline from in-memory GLSL source. |
| `SituationLoadShaderFromSpirv` | Shader Management | auto | Precompiled .spv: OpenGL via GL_ARB_gl_spirv; Vulkan same pipeline contract as S... |
| `SituationLoadShaderFromSpirvMemory` | Shader Management | auto | Same as SituationLoadShaderFromSpirv but from in-memory SPIR-V (e.g. build-time ... |
| `SituationLoadShaderFromSpirvMemoryEx` | Shader Management | auto | [Vulkan] `layout_profile` selects user SSBO/UBO layouts; [OpenGL] profile ignore... |
| `SituationLoadSoundFromFile` | Audio Handle API | auto | Load a sound from a file. |
| `SituationLoadSoundFromFileAsync` | CPU & Thread Management | auto | Asynchronously loads and decodes a sound file. |
| `SituationLoadSoundFromStream` | Audio Handle API | auto | Load a sound from a custom stream. |
| `SituationLoadTexture` | Texture Management | auto | Loads a texture from disk and registers the path for hot-reloading. |
| `SituationLog` | Gamepad Input | manual | variadic C function — wrap manually in Odin |
| `SituationLogWarning` | Gamepad Input | manual | variadic C function — wrap manually in Odin |
| `SituationMaximizeWindow` | Window State Management | auto | Maximize the window if it's resizable. |
| `SituationMeasureText` | Font Management | auto | Measure the pixel dimensions of a string before drawing. |
| `SituationMeasureTextEx` | Font Management | auto | Measure with extra per-character spacing. |
| `SituationMemoryBarrier` | Graphics / compute (prefer SituationCmdBeginRenderPass, SituationCmdBindDescriptorSet, SituationCmdBindTextureSet, SituationCreateComputePipeline*, barrier Ex helpers) | auto | — |
| `SituationMinimizeWindow` | Window State Management | auto | Minimize the window (iconify). |
| `SituationMoveFile` | File Operations | auto | Move/rename a file, even across drives on Windows. |
| `SituationOpenFile` | Active Audio Device Query (v2.4.199) | auto | Open a file or folder with its default application. |
| `SituationPauseApp` | Application Lifecycle & State | auto | Pause the application's internal state (e.g., audio). |
| `SituationPauseAudioDevice` | Audio Device Management | auto | Pause audio playback on the device. |
| `SituationPeekKeyPressed` | Keyboard Input | auto | Peek at the next key in the press queue without consuming it. |
| `SituationPeekKeyPressedEx` | Keyboard Input | auto | Peek at the next key and its scancode. |
| `SituationPlayAudio` | Audio Handle API | auto | Play audio by handle (restarts if already playing). |
| `SituationPlayLoadedSound` | Audio Handle API | auto | Play a loaded sound (restarts if already playing). |
| `SituationPlayMidiNote` | Resonance (Procedural Synthesis) | auto | Legacy: play a tone by MIDI note number (0-127). |
| `SituationPlayTone` | Resonance (Procedural Synthesis) | auto | Legacy: play a simple ADSR tone (backward compat / quick UI sounds). |
| `SituationPlayToneEx` | Resonance (Procedural Synthesis) | auto | @brief Plays an extended procedural tone with full control. @param type         ... |
| `SituationPollInputEvents` | Application Lifecycle & State | auto | [Main thread] Poll for all input events (keyboard, mouse, joystick). Call once p... |
| `SituationPollShaderLoad` | Shader Management | auto | SITUATION_SUCCESS when ready, SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS while comp... |
| `SituationPqGrayToRgb10Packed` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Uniform PQ gray → A2R10G10B10 packed pixel. |
| `SituationPqToLinear` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | ST.2084 PQ [0,1] → linear display light [0,1]. |
| `SituationPrintThreadingStatus` | CPU & Thread Management | auto | Human-readable threading status (stdout if NULL) |
| `SituationPushNodePCM` | PCM Input Node (user-fed ring buffer source) | auto | Push interleaved float PCM into a PCM_INPUT node's ring buffer (any thread). Ret... |
| `SituationQueryShaderStorageBlocks` | Shader Interaction & Synchronization | auto | [OpenGL] Enumerate all active SSBO blocks and their assigned binding points. out... |
| `SituationReadBuffer` | GPU Buffer Management | auto | Read mapped buffer data safely. |
| `SituationReadFramebuffer` | Texture Management | auto | [Phase 2] Blocking readback of framebuffer pixels (RGBA8 or raw RGB10 packed). |
| `SituationReadFramebufferHdr` | Texture Management | auto | [Phase 8] Raw A2R10G10B10 readback when HDR10 swapchain active. |
| `SituationReadTexture` | Texture Management | auto | [Phase 2] Blocking readback of texture pixels. |
| `SituationReadTextureAlloc` | Texture Management | auto | [Phase 2] Blocking readback into allocated SituationImage. |
| `SituationRefreshCpuTopology` | CPU & Thread Management | auto | Rebuilds the process-wide topology cache |
| `SituationRefreshDisplays` | Physical Display (Monitor) Management | auto | Force a refresh of the cached display information. |
| `SituationRefreshNumaTopology` | CPU & Thread Management | auto | Rebuild NUMA summary from CPU topology + OS memory |
| `SituationRegisterDeviceType` | Device Registry Functions | auto | Register a custom device type with the registry. |
| `SituationReloadComputePipeline` | Node Graph SFX Routing (v2.6.5) | auto | Recompiles a compute pipeline from its original source file (Synchronous/Stalls ... |
| `SituationReloadModel` | Node Graph SFX Routing (v2.6.5) | auto | Re-parses GLTF/GLB file and rebuilds all meshes and textures (Synchronous/Stalls... |
| `SituationReloadShader` | Node Graph SFX Routing (v2.6.5) | auto | Recompiles and links a shader from its original source files (Synchronous/Stalls... |
| `SituationReloadTexture` | Node Graph SFX Routing (v2.6.5) | auto | Re-reads image file and recreates the GPU texture resource (Synchronous/Stalls G... |
| `SituationRemovePatch` | Node Graph Functions | auto | Disconnect a specific patch between two ports. |
| `SituationRenameFile` | File Operations | auto | Alias for SituationMoveFile. |
| `SituationRenderPassInfoInherit` | Virtual Displays (Render Targets) | auto | Fill pass struct from VD attachment defaults (tier C helper). |
| `SituationRenderVirtualDisplays` | Virtual Displays (Render Targets) | auto | Composite all visible virtual displays to the current target. |
| `SituationReplayRenderList` | Frame Lifecycle & Command Buffer | auto | Replay a previously recorded render list into a command buffer. |
| `SituationRequestScreenCapture` | Image & Screenshot Utilities | auto | — |
| `SituationResetRenderList` | Frame Lifecycle & Command Buffer | auto | Reset a render list for reuse next frame. |
| `SituationResetThreadPoolStats` | CPU & Thread Management | auto | Zero scheduler counters |
| `SituationRestoreWindow` | Window State Management | auto | Restore a minimized or maximized window. |
| `SituationResumeApp` | Application Lifecycle & State | auto | Resume a paused application. |
| `SituationResumeAudioDevice` | Audio Device Management | auto | Resume audio playback on the device. |
| `SituationRgb10FromRgba` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Upscale 8-bit RGBA → 10-bit. |
| `SituationRgbToHsv` | Color Space Conversions | auto | Converts a standard RGBA color to the Hue, Saturation, Value color space. |
| `SituationRgbToYpqFrom10` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | 10-bit RGBA → float YPQ. |
| `SituationRgbaFromRgb10` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Downscale 10-bit RGBA → 8-bit. |
| `SituationRgbaFromRgb10Packed` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | A2R10G10B10 texel → RGBA8 (readback parity). |
| `SituationSaveFileAsync` | File Operations | auto | Asynchronously save a file. |
| `SituationSaveFileData` | File Operations | auto | Save a block of memory to a file. |
| `SituationSaveFileText` | File Operations | auto | Save a null-terminated string to a text file. |
| `SituationSaveFileTextAsync` | File Operations | auto | Asynchronously save a text file. |
| `SituationSaveGraphToFile` | Graph Serialization Functions | auto | Save a graph to a JSON file. |
| `SituationSaveMidiPreset` | Preset Persistence | auto | Save MIDI Learn mappings to JSON file. |
| `SituationSaveModelAsGltf` | 3D Model Utilities | auto | Exports a model to a human-readable .gltf and a .bin file for debugging. |
| `SituationSerializeGraphToJSON` | Graph Serialization Functions | auto | Serialize a graph to a JSON string (caller must free with SituationFreeJSONStrin... |
| `SituationSetActiveGraph` | Active Graph (Audio Callback Integration) | auto | Set the active audio processing graph (replaces default). NULL disables graph pr... |
| `SituationSetAudioDevice` | Audio Device Management | auto | Set the active audio device. |
| `SituationSetAudioMasterVolume` | Audio Device Management | auto | Set the master volume for the audio device. |
| `SituationSetAudioOutputMonitor` | Audio Output Monitoring (for visualization) | auto | Set a callback to receive mixed output samples (for VU meters, FFT, etc.). |
| `SituationSetAudioPan` | Audio Handle API | auto | Set stereo pan for a handle-based sound [-1.0 to 1.0]. |
| `SituationSetAudioPitch` | Audio Handle API | auto | Set pitch multiplier for a handle-based sound (1.0 = normal). |
| `SituationSetAudioPlaybackSampleRate` | Audio Device Management | auto | Re-initialize the audio device with a new sample rate. |
| `SituationSetAudioVolume` | Audio Handle API | auto | Set volume for a handle-based sound [0.0 to 1.0+]. |
| `SituationSetClipboardText` | Cursor, Clipboard and File Drops | auto | Set text in the system clipboard. |
| `SituationSetControl` | Node Graph Functions | auto | Set a control parameter on a node. |
| `SituationSetCurrentThreadName` | CPU & Thread Management | auto | OS-visible name for the calling thread (UTF-8); no-op if NULL/empty |
| `SituationSetCursor` | Cursor, Clipboard and File Drops | auto | Set the mouse cursor to a standard shape. |
| `SituationSetCursorPosCallback` | Mouse Input | auto | Set a callback for mouse movement events. |
| `SituationSetDisplayMode` | Physical Display (Monitor) Management | auto | Set the display mode for a monitor. |
| `SituationSetExitCallback` | Callbacks and Event Handling | auto | Set a callback to run just before shutdown. |
| `SituationSetFileDropCallback` | Callbacks and Event Handling | auto | Set a callback for file drop events. |
| `SituationSetFocusCallback` | Callbacks and Event Handling | auto | Set a callback for window focus events. |
| `SituationSetGamepadMappings` | Gamepad Input | auto | Load a new set of gamepad mappings from a string. |
| `SituationSetGamepadVibration` | Gamepad Input | auto | Set gamepad vibration/rumble (Windows only). |
| `SituationSetGraphSFXSource` | Node Graph SFX Routing (v2.6.5) | auto | Designate the Sound Source node in the active graph to receive routed SFX tones. |
| `SituationSetJoystickCallback` | Gamepad Input | auto | Set a callback for joystick connection events. |
| `SituationSetKeyCallback` | Keyboard Input | auto | Set a callback for key events. |
| `SituationSetLogCallback` | Gamepad Input | manual | callback registration with nested proc type |
| `SituationSetMaximizeCallback` | Callbacks and Event Handling | auto | Set a callback for window maximize / restore events. |
| `SituationSetMouseButtonCallback` | Mouse Input | auto | Set a callback for mouse button events. |
| `SituationSetMouseOffset` | Mouse Input | auto | Set a software offset for the mouse position. |
| `SituationSetMousePosition` | Mouse Input | auto | Set the mouse position within the window. |
| `SituationSetMouseScale` | Mouse Input | auto | Set a software scale for the mouse position and delta. |
| `SituationSetNodeMidiChannel` | MIDI Device Control | auto | Filter MIDI to channel 0-15, or -1 omni. |
| `SituationSetPixelColor` | Image Generation & Copying | auto | Helper to set a specific pixel color (CPU-side). |
| `SituationSetResizeCallback` | Callbacks and Event Handling | auto | Set a callback for window framebuffer resize events. |
| `SituationSetScreenshotFormat` | Image & Screenshot Utilities | auto | Set the default screenshot file format (default: BMP). |
| `SituationSetScrollCallback` | Mouse Input | auto | Set a callback for mouse scroll events. |
| `SituationSetShaderUniform` | Shader Interaction & Synchronization | auto | [OpenGL] Set a standalone uniform by name (location cache). While a frame is act... |
| `SituationSetShaderUniform1fv` | Shader Interaction & Synchronization | auto | [OpenGL] Set float uniform array. |
| `SituationSetShaderUniform1iv` | Shader Interaction & Synchronization | auto | [OpenGL] Set int uniform array in one call (e.g. name "uWallRows[0]", count=24).... |
| `SituationSetShaderUniformLocation` | Shader Interaction & Synchronization | auto | [OpenGL] Set uniform by explicit location (SPIR-V layout(location=)); defers dur... |
| `SituationSetShaderUniformMatrix4fv` | Shader Interaction & Synchronization | auto | [OpenGL] Set mat4 uniform array. |
| `SituationSetSoundEcho` | Sound Parameters and Effects | auto | Apply an echo effect to a sound. |
| `SituationSetSoundFilter` | Sound Parameters and Effects | auto | Apply a low-pass or high-pass filter to a sound. |
| `SituationSetSoundPan` | Sound Parameters and Effects | auto | Set the stereo pan for a sound [-1.0 to 1.0]. |
| `SituationSetSoundPitch` | Sound Parameters and Effects | auto | Set the pitch for a sound (resamples). |
| `SituationSetSoundReverb` | Sound Parameters and Effects | auto | Apply a reverb effect to a sound. |
| `SituationSetSoundVolume` | Sound Parameters and Effects | auto | Set the volume for a specific sound. |
| `SituationSetTargetFPS` | Frame Timing & FPS Management | auto | Set a desired frame rate cap (0 for uncapped). |
| `SituationSetTextureSamplerParams` | Texture Management | auto | [Phase 2] Update sampler state. |
| `SituationSetThreadAffinity` | CPU & Thread Management | auto | Pins the CURRENT thread (logical CPU bitmask, bits 0..63) |
| `SituationSetThreadAffinityEx` | CPU & Thread Management | auto | Set affinity; optional previous mask |
| `SituationSetTimerOscillatorPeriod` | Temporal Oscillator System | auto | Set the period of an oscillator. |
| `SituationSetToneRouting` | Node Graph SFX Routing (v2.6.5) | auto | Route a procedural tone to the active graph's SFX sound source. |
| `SituationSetTraceLogLevel` | Gamepad Input | auto | Set the minimum log level for output filtering. |
| `SituationSetVSync` | Window State Management | auto | Enable or disable VSync (vertical synchronization). |
| `SituationSetVirtualDisplayAttachmentDefaults` | Virtual Displays (Render Targets) | auto | Tier B storage-only attachment defaults. |
| `SituationSetVirtualDisplayDirty` | Virtual Displays (Render Targets) | auto | Mark a virtual display as needing to be re-rendered. |
| `SituationSetVirtualDisplayFallbackColor` | Virtual Displays (Render Targets) | auto | SOLID idle tint (normalized by compositor). |
| `SituationSetVirtualDisplayFallbackMode` | Virtual Displays (Render Targets) | auto | SOLID, COLORBURST, or PATTERN when idle. |
| `SituationSetVirtualDisplayIdleThreshold` | Virtual Displays (Render Targets) | auto | Set idle threshold for compositor fallback (Phase 2a). |
| `SituationSetVirtualDisplayPatternConfig` | Virtual Displays (Render Targets) | auto | Full standby tuning + PATTERN mode. |
| `SituationSetVirtualDisplayPatternLayers` | Virtual Displays (Render Targets) | auto | Toggle bitmask + PATTERN fallback (plan §3.4). |
| `SituationSetVirtualDisplayScalingMode` | Virtual Displays (Render Targets) | auto | Set the scaling/filtering mode for a virtual display. |
| `SituationSetWindowFocused` | Window State Management | auto | Set the window to be focused. |
| `SituationSetWindowIcon` | Window Property Management | auto | Set the icon for the window (single image). |
| `SituationSetWindowIcons` | Window Property Management | auto | Set the icon for the window (multiple sizes). |
| `SituationSetWindowMaxSize` | Window Property Management | auto | Set the window maximum dimensions. |
| `SituationSetWindowMinSize` | Window Property Management | auto | Set the window minimum dimensions. |
| `SituationSetWindowMonitor` | Physical Display (Monitor) Management | auto | Set the window to be fullscreen on a specific monitor. |
| `SituationSetWindowOpacity` | Window Property Management | auto | Set window opacity [0.0f to 1.0f]. |
| `SituationSetWindowPosition` | Window Property Management | auto | Set the window position on the screen. |
| `SituationSetWindowSize` | Window Property Management | auto | Set the window dimensions. |
| `SituationSetWindowState` | Window State Management | auto | Set window configuration state using flags (additive). |
| `SituationSetWindowStateProfiles` | Advanced Window Profile Management | auto | Set the flag profiles for when the window is focused vs. unfocused. |
| `SituationSetWindowTitle` | Window Property Management | auto | Set the title for the window. |
| `SituationSetupVirtualMidiLoopback` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | Create connected virtual out→in pair. Returns input device_id for SituationEnabl... |
| `SituationShowCursor` | Cursor, Clipboard and File Drops | auto | Show the mouse cursor. |
| `SituationShowMessageBox` | Gamepad Input | auto | Blocking UI message box; for fatal init errors. |
| `SituationShutdown` | Application Lifecycle & State | auto | [Main thread] Shut down the library and release all resources. |
| `SituationSoundCopy` | Sound Data Manipulation (Wave Utilities) | auto | Create a new sound by copying the raw PCM data from a source. |
| `SituationSoundCrop` | Sound Data Manipulation (Wave Utilities) | auto | Crop a sound's PCM data in-place to a new range. |
| `SituationSoundExportAsWav` | Sound Data Manipulation (Wave Utilities) | auto | Export the sound's raw PCM data to a WAV file. |
| `SituationStartAudioCapture` | Audio Capture | auto | Start capturing audio input with default format. |
| `SituationStartAudioCaptureEx` | Audio Capture | auto | Start capturing with explicit sample rate and channel count. |
| `SituationStartMidiLearn` | Learning Operations | auto | Start learning: next CC received maps to this param. Scaling: 0=linear, 1=log, 2... |
| `SituationStopAllLoadedSounds` | Audio Handle API | auto | Stop all currently playing sounds. |
| `SituationStopAllTones` | Resonance (Procedural Synthesis) | auto | Stop all active tones (triggers release on each). |
| `SituationStopAudioCapture` | Audio Capture | auto | Stop audio capture and release the input device. |
| `SituationStopLoadedSound` | Audio Handle API | auto | Stop a specific sound from playing. |
| `SituationStopTone` | Resonance (Procedural Synthesis) | auto | Gracefully stop a tone by triggering its release envelope. Invalid handles are i... |
| `SituationSubmitJobEx` | CPU & Thread Management | auto | Submits a job with priority flags and optional data payload. |
| `SituationSubmitRenderList` | Frame Lifecycle & Command Buffer | auto | Submit a render list for async recording on a worker thread. |
| `SituationSubmitRenderList` | Frame Lifecycle & Command Buffer | auto | Submit a render list for immediate recording (single-threaded fallback). |
| `SituationTakeScreenshot` | Image & Screenshot Utilities | auto | Take a screenshot. fileName is the base name (no extension) or NULL for auto-nam... |
| `SituationTeardownVirtualMidiLoopback` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | Close and destroy the virtual loopback devices. |
| `SituationTimerGetOscillatorPeriod` | Temporal Oscillator System | auto | Get the period of an oscillator in seconds. |
| `SituationTimerGetOscillatorState` | Temporal Oscillator System | auto | Get the current binary state (0 or 1) of an oscillator. |
| `SituationTimerGetOscillatorTriggerCount` | Temporal Oscillator System | auto | Get the total number of times an oscillator has triggered. |
| `SituationTimerGetPingProgress` | Temporal Oscillator System | auto | Get progress [0.0 to 1.0+] of the interval since the last successful ping. |
| `SituationTimerGetPreviousOscillatorState` | Temporal Oscillator System | auto | Get the previous frame's state of an oscillator. |
| `SituationTimerGetTime` | Temporal Oscillator System | auto | Get the total time elapsed since initialization. |
| `SituationTimerHasOscillatorUpdated` | Temporal Oscillator System | auto | Check if an oscillator's state has changed this frame. |
| `SituationTimerPingOscillator` | Temporal Oscillator System | auto | Check if an oscillator's period has elapsed since the last ping. |
| `SituationToggleBorderlessWindowed` | Window State Management | auto | Toggle window between borderless and decorated mode. |
| `SituationToggleFullscreen` | Window State Management | auto | Toggle window between fullscreen and windowed mode. |
| `SituationToggleWindowStateFlags` | Advanced Window Profile Management | auto | Toggle flags in the current profile and apply the result. |
| `SituationTopologicalSort` | Node Graph Functions | auto | Re-sort the graph processing order after topology changes. Call from the main th... |
| `SituationUnloadAudio` | Audio Handle API | auto | Unload audio by handle and free resources. |
| `SituationUnloadDroppedFiles` | Cursor, Clipboard and File Drops | auto | Unload the file path list returned by SituationLoadDroppedFiles. |
| `SituationUnloadFont` | Font Management | auto | Frees CPU font data, glyph metrics, and owned GPU atlas (not the built-in defaul... |
| `SituationUnloadImage` | Image Loading and Unloading | auto | Unload an image's pixel data from memory. |
| `SituationUnloadModel` | 3D Model Utilities | auto | Frees all GPU and CPU resources associated with a loaded model. |
| `SituationUnloadShader` | Shader Management | auto | Unload a graphics shader pipeline and free its GPU resources. |
| `SituationUnloadSound` | Audio Handle API | auto | Unload a sound and free its resources. |
| `SituationUpdate` | Lifecycle | auto | — |
| `SituationUpdateBuffer` | GPU Buffer Management | auto | Update data in a GPU buffer. |
| `SituationUpdateTimers` | Application Lifecycle & State | auto | [Main thread] Update all internal timers (frame timer, temporal system). Call af... |
| `SituationValidateShaderUniforms` | Shader Interaction & Synchronization | auto | Returns first missing/wrong-type uniform, or SUCCESS if all resolved. |
| `SituationVirtualMidiControlChange` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | CC (e.g. mod wheel, expression). |
| `SituationVirtualMidiNoteOff` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | Inject note-off on channel 0 (legacy wrapper). |
| `SituationVirtualMidiNoteOffEx` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | Channel-aware note-off (0-15). |
| `SituationVirtualMidiNoteOn` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | Inject note-on on channel 0 (legacy wrapper). |
| `SituationVirtualMidiNoteOnEx` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | Channel-aware note-on (0-15). |
| `SituationVirtualMidiPitchBend` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | Pitch bend 0..16383 (center 8192). |
| `SituationVirtualMidiProgramChange` | Virtual MIDI loopback (integration testing; no hardware keyboard required) | auto | Program change on channel 0-15. |
| `SituationWaitForAllJobs` | CPU & Thread Management | auto | Blocks until all queued jobs are finished. |
| `SituationWaitForJob` | CPU & Thread Management | auto | Waits for a specific job to complete (O(1) check). |
| `SituationWindowShouldClose` | Application Lifecycle & State | auto | Check if the application should close (e.g., user clicked X). |
| `SituationYpqAdjustChroma` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Scale Q (chroma amplitude); preserve luma and phase. |
| `SituationYpqAdjustLuma` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Scale Y (luma); preserve phase and chroma. |
| `SituationYpqAdjustPhase` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Rotate hue; P shifts by byte steps mod 256. |
| `SituationYpqAnalyzeRgbMapping` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | — |
| `SituationYpqClampInGamut` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Reduce chroma if linear RGB would clip. |
| `SituationYpqDistance` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Weighted distance in YPQ space. |
| `SituationYpqEquals` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Per-channel tolerance compare. |
| `SituationYpqGetChroma` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Normalized chroma amplitude [0, 1]. |
| `SituationYpqGetHueDegrees` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Hue in degrees [0, 360). |
| `SituationYpqGetLuma` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Normalized luma [0, 1]. |
| `SituationYpqLerp` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Interpolate YPQ; phase uses shortest arc on the hue wheel. |
| `SituationYpqQuantize` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Float YPQ → 8-bit ColorYPQA. |
| `SituationYpqSliceDuplicateCount` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | — |
| `SituationYpqToRgb10Packed` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Float YPQ → A2R10G10B10 packed pixel (10-bit SDR). |
| `SituationYpqToRgb10PackedHdr` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Float YPQ → PQ-encoded A2R10G10B10 (HDR10 swapchain). |
| `SituationYpqToRgba10` | YPQ / HDR color science (optional tooling; typical apps: SituationColorToYPQ / SituationColorFromYPQ only) | auto | Float YPQ → 10-bit RGBA (linear YIQ, clamped). |
