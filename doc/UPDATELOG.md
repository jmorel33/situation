## [v2.3.58 "FX & Metering" (Phase 4)] - 2026-03-07

### Description

This release implements Phase 4 of the Audio Mixer roadmap, introducing a powerful "Plug-in" architecture for Aux Buses and real-time metering for tracks. Developers can now insert any standard `miniaudio` node (filters, reverbs, delays) directly into the signal chain of an Aux bus, creating a modular FX system. Additionally, thread-safe metering APIs provide real-time visualization of signal levels and compressor gain reduction.

### New Features

- **FX Slots:** Aux Buses now support an insert chain of up to 8 generic `ma_node` effects.
  - `SituationInsertEffect`: Dynamically inserts an effect node into a specific slot, automatically rewiring the audio graph.
  - `SituationRemoveEffect`: Safely removes an effect node and bridges the connection gap.
  - `SituationGetMixerGraph`: Exposes the mixer's internal graph, allowing users to initialize their own custom `ma_node` instances.
- **Metering System:**
  - **Track Peak Metering:** Added `SituationGetTrackMeter` to retrieve real-time Left/Right peak levels safely from the audio thread.
  - **Gain Reduction:** `SituationGetTrackMeter` also reports the instantaneous gain reduction (dB) from the track's dynamics processor, essential for visualizing compression.
  - **Zero-Lock Monitoring:** All metering data is exposed via atomic variables, ensuring that UI visualization never blocks or stalls the audio processing thread.

## [v2.3.57 "Mixer Routing" (Phase 3)] - 2026-03-06

### Description

This release delivers the routing infrastructure for the Situation Mixer (Phase 3). It introduces 8 Auxiliary Buses, comprehensive Send controls (Pre/Post-Fader), and flexible output routing. Additionally, standard mixer controls like Pan, Mute, and Solo (Solo-In-Place) are now fully implemented, turning the engine into a capable mixing console.

### New Features

- **Auxiliary Buses:** The mixer now initializes 8 stereo Aux buses (`SituationAudioBus`). These can be used for effects sends (Reverb/Delay) or sub-grouping.
- **Flexible Sends:** Tracks can now send audio to any Aux bus via `SituationSetTrackSend`.
  - **Pre-Fader:** Sends signal before the fader (useful for monitoring/foldback).
  - **Post-Fader:** Sends signal after the fader (useful for FX sends).
- **Output Routing:** Tracks can be routed to the Master Bus (default) or any Aux Bus (for subgroups like "Drums") using `SituationSetTrackOutput`.
- **Mixer Controls:**
  - **Pan:** Added `SituationPannerNode` for stereo positioning (`SituationSetTrackPan`).
  - **Mute:** `SituationSetTrackMute` silences the track (and its Pre-Fader sends).
  - **Solo:** `SituationSetTrackSolo` implements exclusive listening logic, automatically muting all non-soloed tracks.

## [v2.3.56 "Channel Strip" (Phase 2)] - 2026-03-05

### Description

This release completes Phase 2 of the Audio Mixer roadmap, transforming each track into a professional-grade mixing console channel. Every `SituationAudioTrack` now includes a hard-wired Channel Strip featuring a 4-band Parametric EQ and a full Dynamics Processor (Compressor/Limiter/Gate) with side-chain support.

### New Features

- **4-Band Parametric EQ:** Each track now has a built-in EQ chain: High-Pass Filter, Low-Shelf, Peaking, and High-Shelf. Configurable via `SituationSetTrackEQ`.
- **Dynamics Processor:** Implemented `SituationDynamicsNode` providing Compressor, Limiter, and Noise Gate functionality per track. Configurable via `SituationSetTrackDynamics`.
- **Side-Chain Ducking:** Added `SituationSetTrackSideChain` to route audio from one track (Key) to control the compression of another (Target), enabling classic radio ducking and EDM pumping effects.
- **Zero-Allocation Routing:** The new EQ and Dynamics nodes are pre-allocated within the `SituationAudioTrack` struct, ensuring no memory allocation occurs when enabling or tweaking effects during playback.

## [v2.3.55 "Audio Mixer Foundation" (Phase 0 & 1)] - 2026-03-04

### Description

This release lays the groundwork for the new professional-grade Audio Mixer architecture. It introduces the core infrastructure for track management, device enumeration, and graph-based routing, moving beyond simple sound playback towards a full mixing console model.

### New Features

- **Device Enumeration (Phase 0):** Added `SituationEnumerateAudioDevices` and `SituationFindBestDevice`. Applications can now query detailed capabilities (channels, sample rates) of all available playback and capture devices and intelligently select the best match.
- **Mixer Core (Phase 1):** Introduced `SituationAudioMixer`, `SituationAudioTrack`, and `SituationAudioBus` structures. Implemented the lifecycle functions `SituationCreateMixer` and `SituationDestroyMixer`.
- **Track Management:** Added `SituationAddTrack` to create tracks dynamically. Implemented basic volume controls (`SituationSetTrackVolume`).
- **Graph Routing:** Implemented `SituationRouteSoundToTrack`. Sounds can now be routed into specific mixer tracks instead of playing directly to the endpoint, enabling per-track processing.
- **Thread-Safe Integration:** Updated the main audio callback to support the new mixer graph. If a mixer is active, the callback safely locks the topology mutex and delegates processing to the mixer's node graph (`ma_node_graph_read_pcm_frames`).

## [2.3.54] - 2026-02-xx
**"Documentation Fortress" Release**

### Documentation Overhaul (Massive)
- Added **over 2,000 lines** of comprehensive, consistent inline documentation across the entire codebase.
- Covered nearly every major subsystem with detailed headers:
  - Full init chain (`SituationInit` → `_SituationInitSubsystems` → backend-specific `_SituationInitOpenGL`/`_SituationInitVulkan`)
  - Render thread startup, handoff, and queue management (`_SituationInitRenderThread`, `_SituationRenderThreadEntry`, `_SituationEnqueueRenderList`, `_SituationReplayToQueue`)
  - Resource slot patterns (alloc/get/free/validate for shaders, meshes, buffers, compute pipelines, models)
  - Virtual display & compositing paths (`SituationCreateVirtualDisplay`, `SituationRenderVirtualDisplays`)
  - Texture/buffer creation wrappers (`SituationCreateTexture`, `SituationCreateTextureEx`, `SituationUpdateBuffer`)
  - Quad renderer init (`_SituationInitQuadRenderer`) and draw command (`SituationCmdDrawQuad`)
  - Text renderer bootstrap (`_SituationInitTextRenderer`, bitmap font loading)
  - Bindless GL glue for virtual displays (`_SituationVirtualBindlessInit`, `_SituationVirtualBindlessBind`)
  - Async submission paths (`SituationSubmitRenderList` variants with thread pool)
  - Performance/debug helpers (`SituationExportRenderHistogram` — JSON latency export)
  - Filesystem & hot-reload helpers (load/save async workers, path utils)
  - And many more internal helpers, getters, and Vulkan/OpenGL specifics
- Standardized style: `[INTERNAL]` for private helpers, full SITAPI docs with error codes, thread safety notes, cross-references, and usage examples where helpful.
- Result: The codebase is now **self-documenting** — much easier to navigate, maintain, extend, or hand off.

### Critical Stability Fixes (from v2.3.53)

This release also addresses critical stability issues identified in the OpenGL backend, specifically targeting Multi-Draw Indirect (MDI) batching, resource destruction safety, and ring buffer management. These fixes prevent driver crashes, visual corruption, and potential memory leaks during high-load scenarios and application shutdown.

- **MDI Pipeline Consistency:** Fixed a severe bug in the MDI auto-batcher (`_SituationGLExecuteCommands`) where batches were formed based solely on VAO continuity, ignoring shader program changes. This could cause meshes to be drawn with the wrong shader, leading to corruption or crashes. The batcher now strictly enforces pipeline consistency (`current_recording_shader_id`) during lookahead.
- **Fence Cleanup on Shutdown:** `_SituationCleanupOpenGL` now performs a timed wait (`glClientWaitSync` with 100ms timeout) on any remaining fences before deletion. This prevents driver stalls or crashes caused by deleting active sync objects during context teardown.
- **VAO Restore Safety:** Added a safety check to ensure `global_vao_id` is valid before attempting to restore it after an MDI batch. This prevents undefined behavior if the global VAO was never created or has been destroyed.
- **MDI Ring Buffer Overflow:** Implemented a lower-bound check in the MDI ring buffer allocation logic. This prevents the atomic offset from wrapping around (negative values) or overlapping with the frame start offset, protecting against silent buffer corruption during long sessions.

## [v2.3.52 "Virtual Bindless" (OpenGL Fallback)] - 2026-03-02

### Description

This release introduces the "Virtual Bindless" texture system, a powerful compatibility layer for the OpenGL backend. It allows users to write modern, bindless-style shader code (accessing global texture arrays via indices) that runs transparently on hardware lacking native `GL_ARB_bindless_texture` support (e.g., older Intel iGPUs).

### New Features

- **Virtual Bindless Fallback:** Implemented a CPU-side texture slot manager (`_SituationVirtualBindlessBind`) that emulates bindless access using a limited pool of 32 texture units and an LRU eviction strategy.
- **Shader Injection:** The shader compiler (`_SituationCompileGLShader`) now automatically detects if the fallback is active and injects compatibility macros and uniforms (`_sit_virtual_textures`, `_sit_texture_slot_id`). This allows standard bindless shaders (`global_textures[nonuniformEXT(id)]`) to compile and run without modification on legacy hardware.
- **Unified Command Execution:** `SituationCmdBindTextureSet` and `_SituationGLExecuteCommands` now intelligently switch between native bindless, virtual bindless, and standard binding paths based on runtime feature detection.
- **Debug Stats:** Added real-time tracking of Virtual Bindless cache hits and misses to the `SituationDrawMetricsOverlay`.

## [v2.3.51 "MDI-Boosted" (OpenGL Multi-Draw Indirect)] - 2026-03-02

### Description

This release implements the "Max Out Core" optimization strategy for the OpenGL backend with Multi-Draw Indirect (MDI) auto-batching. It drastically reduces CPU-to-driver overhead for repetitive mesh rendering by intelligently collapsing consecutive draw commands into a single dispatch. This brings OpenGL performance significantly closer to Vulkan for high-instance-count scenarios while maintaining the simple "Immediate Mode" API surface.

### New Features

- **MDI Auto-Batching:** The Soft Command Buffer executor (`_SituationGLExecuteCommands`) now detects sequences of `SIT_OP_DRAW_MESH` commands that share the same Vertex Array Object (VAO). Instead of issuing individual `glDrawElements` calls, it batches them into a persistent, mapped `GL_DRAW_INDIRECT_BUFFER` ring and executes them with a single `glMultiDrawElementsIndirect` call.
- **Robust Detection:** Implemented strict lookahead logic to ensure batching only occurs when it is safe (same VAO, same pipeline implied by opcode continuity).
- **Persistent Ring Buffer:** Added `_SituationInitGLMDIBuffer` to manage a per-frame segmented ring buffer (1MB per frame) for zero-copy command generation.

## [v2.3.50 "Fence-Guarded" (OpenGL Deferred Destruction)] - 2026-03-01

### Description

This release introduces a robust, fence-guarded deferred destruction system for the OpenGL backend. It eliminates CPU stalls caused by blocking `glFinish` or unsafe immediate resource deletion. By utilizing `GL_ARB_sync` fences and per-frame graveyards, the engine now ensures resources are only destroyed once the GPU has fully completed the frame in which they were queued, matching the reliability of the Vulkan backend.

### Critical Fixes

- **Non-Blocking Destruction:** Replaced the global OpenGL graveyard with per-frame queues.
- **Fence Synchronization:** Implemented `glFenceSync` tracking for every frame.
- **Safe Flushing:** `_SitGLFlushGraveyard` now polls fences using `glClientWaitSync` with a timeout of 0, ensuring deletions only occur when safe without stalling the main thread.
- **Polish:** Fence creation is now correctly sequenced after `glfwSwapBuffers` with an explicit `glFlush` for maximum driver compatibility.

## [v2.3.49 "Async Shader Linking" (Eliminate Hot-Reload Stalls)] - 2026-02-26

### Description

This release introduces Asynchronous Shader Linking for the OpenGL backend, utilizing `KHR_parallel_shader_compile`. This eliminates the CPU stall previously caused by `glLinkProgram` during hot-reloading, making OpenGL hot-reloading instantaneous and stutter-free, matching the performance of the Vulkan backend.

### Critical Fixes

- **Async Linking:** Implemented `_SituationCreateGLShaderProgramAsync` to initiate linking without blocking.
- **Non-Blocking Hot-Reload:** Refactored `_SituationPerformHotReloadPass` to use the async creation path.
- **Background Polling:** Modified `SituationAcquireFrameCommandBuffer` to poll for `GL_COMPLETION_STATUS_KHR` and finalize the shader swap only when linking is complete.

## [v2.3.48 "Hardening" (Thread Safety & Verification)] - 2026-02-25

### Description

This release hardens the library against critical race conditions between the main thread and the I/O thread (Hot-Reloading), and ensures safer shutdown sequences. It specifically targets potential crashes during resource creation and cleanup in multi-threaded environments.

### Critical Fixes

- **Resource Registry Locking:** Protected resource slot allocation in `SituationCreateTextureEx`, `SituationCreateMesh`, and `SituationLoadShaderFromMemory` with `resource_registry_mutex`. This prevents the I/O thread from corrupting the registry while the main thread creates resources.
- **Bindless Descriptor Safety:** Protected `vkUpdateDescriptorSets` calls in `SituationCreateTextureEx` (both bindless and standard paths) with `resource_registry_mutex`. This prevents race conditions where the hot-reload system might be updating descriptors concurrently with new resource creation.
- **Shutdown Safety:** Reordered `SituationShutdown` to destroy the thread pool *before* waiting for the GPU and cleaning up resources. This eliminates a class of shutdown crashes where background threads attempted to access resources that were already being destroyed.

### Documentation

- **Verification Suite:** Added `situation_verify.cpp`, a standalone test suite to validate registry stress, hot-reload logic, and bindless descriptor integrity.

## [v2.3.47 "Renderer Stability" (Vulkan Push Constant Fix)] - 2026-02-24

### Description

This release addresses a critical alignment issue in the Vulkan backend's push constant logic. It resolves a state corruption bug where `SituationCmdDrawQuad` would overwrite the texture ID bound by `SituationCmdBindTextureSet`, ensuring consistent "bind-then-draw" behavior across both OpenGL and Vulkan backends.

### Critical Fixes

- **Vulkan State Preservation:** Fixed `SituationCmdDrawQuad` to use split `vkCmdPushConstants` calls. This preserves the `texture_id` (located at offset 96) when updating the `use_texture` flag (offset 100), preventing the shader from reverting to untextured rendering unexpectedly.
- **Struct Alignment:** Corrected the internal push constant structure in `SituationCmdDrawQuad` to include padding for the `texture_id` field, ensuring the `use_texture` flag aligns correctly with the shader's memory layout.

### Documentation

- **Custom Shader Warning:** Added a warning to `SituationCmdBindTextureSet` clarifying that custom shaders using this command must adhere to the standard push constant layout (Model 64b + Color 16b + UVRect 16b = 96b offset for `texture_id`).

## [v2.3.46 "Bindless" (Hotfix: Text Crash)] - 2026-02-23

### Description

This is a critical hotfix for the v2.3.45 "Bindless" release. It resolves a crash in the Vulkan Text Renderer where legacy "Bindful" logic (expecting specific descriptor sets) was incompatible with the new Bindless architecture (global descriptor array).

### Critical Fixes

- **Vulkan Text Renderer:** Updated `SIT_TEXT_FRAGMENT_SHADER` and pipeline logic to correctly use the global bindless descriptor set.
- **Shader Correction:** Moved GLSL extension directives to the top of the shader source to comply with strict driver validation.
- **Draw Logic:** `SituationCmdDrawTextEx` now pushes texture IDs via Push Constants instead of attempting to bind non-existent descriptor sets.

## [v2.3.45 "Bindless" (Vulkan Descriptor Indexing)] - 2026-02-17

### Description

This release migrates the Vulkan backend to a "Bindless" architecture using Descriptor Indexing. This eliminates the CPU overhead of binding individual descriptor sets for every texture and enables massive draw call batching.

### New Features

- **Bindless Textures (Vulkan):**
  - Textures are now accessed via a global descriptor array (`global_textures[]`) indexed by a push constant (`texture_id`).
  - Removed per-texture `VkDescriptorSet` allocation, solving pool fragmentation issues.
  - Enabled Vulkan 1.2+ features: `shaderSampledImageArrayNonUniformIndexing`, `runtimeDescriptorArray`, `descriptorBindingPartiallyBound`.

### Technical Details

- **Global Descriptor Set:** A single `VkDescriptorSet` (Set 1) now contains all active textures (up to 4096).
- **Zero-Bind Draw Loop:** `SituationCmdDrawTexture` no longer calls `vkCmdBindDescriptorSets` for textures, reducing driver overhead.
- **Shader Update:** Updated internal Quad shaders to use `GL_EXT_nonuniform_qualifier` for accessing the global texture array.

---

## [v2.3.44 "Optimization" (Vulkan Memory & Hot-Reload)] - 2026-02-10

### Description

This release addresses key performance and scalability feedback from architectural reviews. It introduces runtime configuration for Vulkan memory usage and optimizes the Hot-Reloading system to prevent I/O storms. These changes allow the engine to scale down to lower-end devices and scale up to large projects with thousands of assets.

### New Features

- **Configurable Staging Buffers:** Added `staging_buffer_size` to `SituationInitInfo`.
  - **Default:** 128MB (same as before).
  - **Customizable:** Users can now reduce this value (e.g., to 16MB) to significantly lower the VRAM/RAM footprint on constrained devices, or increase it for massive bandwidth requirements.
- **Optimized Hot-Reloading:** The hot-reload logic now respects the user-defined `hot_reload_poll_rate` strictly.
  - **Reduced Overhead:** Removed hardcoded internal throttling. The I/O thread now sleeps efficiently based on the configured rate, eliminating redundant file system checks.

---

## [v2.3.43 "System Unification" (Universal Handles)] - 2026-02-09

### Description

This release represents a monumental architectural shift for the Situation engine, codenamed "System Unification". It implements the **Universal Handle Architecture** (v2.4 Milestone), unifying all resource management (Textures, Sounds, Shaders, Meshes, Buffers, Models, Compute Pipelines) under a single, high-performance **Generational Handle** system.

This upgrade eliminates ~1400 lines of legacy code, replacing O(N) linked-list traversals with O(1) array-based registries. It provides mathematically provable resource safety (preventing Use-After-Free via generation counters) and enables a unified, robust Hot-Reloading system for all asset types.

### Architectural Changes

- **Universal Handles:** All resources are now opaque 64-bit handles (`{ index, generation }`) backed by fixed-size static registries.
- **Legacy Removal:** Deleted all `_Situation*Node` linked-list structures and associated traversal logic.
- **O(1) Access:** Resource validation and retrieval is now constant-time, eliminating performance degradation as scene complexity grows.
- **Unified Hot-Reload:** Centralized the hot-reload logic into a single generic pass that iterates registries, replacing scattered per-resource logic.

### Critical Fixes

- **Compilation Fix:** Removed the dead code function `_SitGetBufferNode`, which was causing compilation errors by referencing deleted structs.
- **Registry Safety:** Implemented atomic generation counters for all resource slots to prevent ABA problems during rapid load/unload cycles.

### Documentation

- **Regression Analysis:** Added `REGRESSION_ANALYSIS.md` detailing the migration, code reduction stats, and impact analysis.

---

## [v2.3.42 "Flexible Formats" (Audio Capture & Native Formats)] - 2026-02-08

### Description

This release significantly enhances the Audio subsystem by introducing support for multi-channel audio capture (e.g., Stereo Microphones) and custom sample rates. Crucially, the default `SituationStartAudioCapture` API now utilizes the device's **Native Format** (0, 0) instead of hardcoding 44.1kHz/Mono. This eliminates unnecessary resampling overhead and ensures optimal latency and quality on professional audio interfaces running at 48kHz or higher.

### New Features

- **`SituationStartAudioCaptureEx`** - New API to start audio capture with specific `sample_rate` and `channels`.
- **Native Format Default** - `SituationStartAudioCapture` now defaults to the device's native configuration (via Miniaudio's auto-negotiation) instead of forcing Mono/44.1kHz.
- **Multi-Channel Support** - The internal ring buffer logic (`_sit_miniaudio_capture_callback` and `SituationPollInputEvents`) now correctly handles and linearizes interleaved multi-channel audio data.

### Critical Fixes

- **Buffer Safety:** Updated the ring buffer read/write logic to calculate sizes based on *samples* rather than *frames*. This prevents potential buffer overflows or misalignment when capturing stereo or multi-channel audio.
- **Resampling Overhead:** By defaulting to the native format, the engine avoids the CPU cost and latency of Miniaudio's internal resampler when the requested format doesn't match the hardware.

---

## [v2.3.41 "Flexible Formats" (Color Encoding & Format Selection)] - 2026-02-07

### Description

This release introduces flexible texture format selection through the new `SituationColorEncoding` enum. Images can now specify whether their data is in linear or SRGB color space, enabling automatic GPU format selection that works identically across both OpenGL and Vulkan backends. This fixes storage image compatibility issues while maintaining proper gamma correction for sampled textures.

### Critical Fix: Storage Image Format Compatibility

**Problem:** All textures were hardcoded to use SRGB format (`VK_FORMAT_R8G8B8A8_SRGB` in Vulkan, `GL_SRGB8_ALPHA8` in OpenGL). This format is incompatible with storage images (textures writable by compute shaders) on most GPUs, causing validation errors and black screens in applications like K-Term that use compute shaders for rendering.

**Solution:** Implemented color encoding abstraction with automatic format selection:
- Added `SituationColorEncoding` enum with `LINEAR` and `SRGB` values
- Added `color_encoding` field to `SituationImage` struct
- Texture creation now selects format based on color encoding:
  - `SITUATION_COLOR_LINEAR` → `VK_FORMAT_R8G8B8A8_UNORM` (Vulkan) or `GL_RGBA8` (OpenGL)
  - `SITUATION_COLOR_SRGB` → `VK_FORMAT_R8G8B8A8_SRGB` (Vulkan) or `GL_SRGB8_ALPHA8` (OpenGL)
- Storage images automatically use LINEAR format regardless of specified encoding

### New Features

- **`SituationColorEncoding` enum** - Describes color space of image data
  - `SITUATION_COLOR_LINEAR` (0) - Linear color space, required for storage images
  - `SITUATION_COLOR_SRGB` (1) - SRGB color space with gamma correction
- **`color_encoding` field** - Added to `SituationImage` struct
- **Automatic format selection** - Texture creation uses encoding to select GPU format
- **Backend-neutral API** - Same enum works for both OpenGL and Vulkan
- **Storage image override** - Textures with `SITUATION_TEXTURE_USAGE_STORAGE` flag automatically use LINEAR

### Technical Details

**Format Mappings:**

| Color Encoding | Vulkan Format | OpenGL Format | Use Case |
|----------------|---------------|---------------|----------|
| `SITUATION_COLOR_LINEAR` | `VK_FORMAT_R8G8B8A8_UNORM` | `GL_RGBA8` | Storage images, compute writes |
| `SITUATION_COLOR_SRGB` | `VK_FORMAT_R8G8B8A8_SRGB` | `GL_SRGB8_ALPHA8` | Sampled textures, photos, UI |

**Key Rules:**
- Storage images MUST use LINEAR encoding (SRGB doesn't support storage operations)
- Sampled-only textures SHOULD use SRGB encoding for proper gamma correction
- Format selection happens automatically during texture creation
- Both backends enforce the same rules for consistency

### Usage Examples

**Creating a storage image for compute shader:**
```c
SituationImage img;
SituationCreateImage(1024, 768, 4, &img);
img.color_encoding = SITUATION_COLOR_LINEAR;  // Required for storage!

SituationTexture tex;
SituationCreateTextureEx(img, false, 
    SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_STORAGE, &tex);
```

**Loading a texture for display:**
```c
SituationImage img;
SituationLoadImage("photo.png", &img);
img.color_encoding = SITUATION_COLOR_SRGB;  // Gamma correction for display

SituationTexture tex;
SituationCreateTexture(img, false, &tex);
```

### Documentation Updates

- Updated `doc/SITUATION_QUICK_REFERENCE.md` with color encoding examples
- Added format mapping table for both backends
- Updated common patterns to show correct usage
- Added storage image compatibility warnings

### Architecture Impact

This change provides a clean abstraction layer for color space management:
- Single API works across both OpenGL and Vulkan
- Fixes K-Term black screen issue (storage image format incompatibility)
- Maintains proper gamma correction for sampled textures
- Sets foundation for future color space extensions (HDR, wide gamut)
- No breaking changes (existing code continues to work)

### Platform Support

- ✅ Windows (MSVC, MinGW, GCC 15.1.0)
- ✅ Vulkan 1.4.313.2
- ✅ OpenGL 4.6
- ✅ Backend-neutral API design

### Migration Notes

**For existing code:**
- No changes required - existing textures continue to work
- To use storage images, set `img.color_encoding = SITUATION_COLOR_LINEAR` before creating texture
- Loaded images default to SRGB (when image loading functions are updated)

**For new code:**
- Always set `color_encoding` explicitly for clarity
- Use LINEAR for storage images and compute shader outputs
- Use SRGB for photos, UI elements, and sampled-only textures

---

## [v2.3.40 "State Machine" (Multi-Threaded Initialization Safety)] - 2026-02-07

### Description

This release introduces atomic state management to prevent initialization race conditions and deadlocks in multi-threaded environments. The addition of `SituationInitState` provides thread-safe queries for initialization status, enabling safe integration with external libraries like K-Term that create GPU resources during startup.

### Critical Fix: Mutex Deadlock Prevention

**Problem:** Applications creating GPU resources (pipelines, textures) immediately after `SituationInit()` would deadlock. The render thread was still initializing and held the `resource_registry_mutex`, causing the main thread to block indefinitely when attempting resource creation.

**Solution:** Implemented atomic state tracking with explicit initialization phases:
- `SITUATION_STATE_UNINITIALIZED` - Library not initialized
- `SITUATION_STATE_INITIALIZING` - Init in progress, render thread starting (unsafe for resource creation)
- `SITUATION_STATE_READY` - Fully initialized, safe to create resources
- `SITUATION_STATE_SHUTTING_DOWN` - Cleanup in progress

### New Features

- **`SituationInitState` enum** - Atomic state tracking for initialization phases
- **`SituationGetInitState()`** - Thread-safe API to query current initialization state
- **`atomic_int init_state`** - Added to `_SituationRenderState` struct for lock-free state queries
- **State transitions** - Automatic state updates in `SituationInit()` and `SituationShutdown()`

### Technical Details

- State is set to `INITIALIZING` at the start of `SituationInit()`
- State transitions to `READY` after render thread successfully spawns
- State transitions to `SHUTTING_DOWN` when `SituationShutdown()` is called
- All state queries use `atomic_load()` for thread-safe, lock-free access
- Debug logging added: `[Situation] Initialization complete - state: READY`

### Integration Pattern

Applications can now safely defer resource creation until Situation is ready:

```c
SituationInit(...);

// Wait for Situation to be fully ready (optional - can defer to first frame)
while (SituationGetInitState() != SITUATION_STATE_READY) {
    // Spin or yield
}

// Now safe to create pipelines, textures, etc.
ExternalLibrary_Init();
```

### Architecture Impact

This change is critical for the "house of cards" multi-threaded architecture:
- Prevents race conditions during initialization
- Enables safe integration with external GPU libraries (K-Term, Quest renderer)
- Provides clear synchronization points for complex startup sequences
- Maintains backward compatibility (existing code continues to work)

### Platform Support

- ✅ Windows (MSVC, MinGW, GCC 15.1.0)
- ✅ Vulkan 1.4.313.2
- ✅ C11 with atomic operations
- ✅ Thread-safe state queries

---

## [v2.3.39 "Triumph" (Vulkan Text Rendering Complete)] - 2026-02-07

### Description

This release represents a major milestone in the Situation library's Vulkan backend development. After extensive debugging across multiple sessions, all text rendering issues have been resolved. The library now features fully functional, production-ready text rendering with proper transparency, alpha blending, and runtime VSync control.

### Major Achievement: Vulkan Text Rendering Fixed

Fixed 15 critical bugs in the Vulkan text rendering pipeline:
1. Internal renderer return value check (treating SUCCESS as failure)
2. Texture generation initialization (generation=0 → generation=1)
3. Descriptor set binding (UBO now bound in text pipeline)
4. Projection matrix updates (now set in `SituationCmdBeginRenderToDisplay`)
5. UBO memory type (GPU_ONLY → CPU_TO_GPU for dynamic updates)
6. Vertex attribute offset (texcoord at correct 8-byte offset)
7. Backface culling (disabled for text quads)
8. Viewport/scissor state (now properly set)
9. Fragment shader descriptor set (added `set = 1` qualifier)
10. Font atlas descriptor layout (uses `text_sampler_layout` binding 0)
11. UV calculation (fixed row division and v0/v1 swap for Vulkan Y-down)
12. Font atlas transparency (background pixels now (0,0,0,0) instead of (255,255,255,0))
13. Fragment shader alpha masking (proper colored text with transparency)
14. Depth write disabled for text (transparent pixels don't block background)
15. VSync present mode selection (respects flag, prefers IMMEDIATE for unlimited FPS)

### New Features

- **`SituationSetVSync(bool enable)`** - Convenience function for runtime VSync control
  - Automatically recreates Vulkan swapchain with new present mode
  - VSync ON: `VK_PRESENT_MODE_FIFO_KHR` (~60 FPS)
  - VSync OFF: `VK_PRESENT_MODE_IMMEDIATE_KHR` (unlimited FPS, 2000+)
  - OpenGL: Uses `glfwSwapInterval()` for immediate effect

### Technical Improvements

- Text rendering now uses alpha masking for proper colored text
- Depth writing disabled for text to allow transparent spacing
- Present mode selection based on VSync flag
- Fixed `glfwSwapInterval()` to only be called for OpenGL (prevents GLFW errors)
- Swapchain recreation on VSync toggle for immediate effect

### Platform Support

- ✅ Windows (MSVC, MinGW, GCC 15.1.0)
- ✅ Vulkan 1.4.313.2
- ✅ C11 with C++ linking for VMA

---

## [v2.3.38 "Native Bitmap Font Support"] - 2025-12-27

### New Features

- Added `SituationLoadBitmapFontFromMemory` to load raw bitmap fonts.
- Extended `SituationFont` struct to support bitmap fonts alongside TrueType.
- Updated `SituationImageDrawCodepoint` to implement a nearest-neighbor forward-mapping rasterizer for bitmap fonts, supporting rotation and scaling.
- Updated `SituationImageDrawTextEx` and `SituationImageDrawText` to seamlessly handle bitmap fonts using the new rasterizer and simplified layout logic.
- Updated `SituationMeasureText` to correctly calculate dimensions for monospaced bitmap fonts.
- Refactored `examples/hello_world.c` to use the new native bitmap font API instead of manual pixel pushing.

---

## [v2.3.37 "Trinity Polish" (Async I/O Hardening)] - 2025-12-25

### Description

This release solidifies the "Trinity" architecture by addressing critical thread-safety and runtime configuration issues in the I/O subsystem. It ensures clean shutdowns by properly joining the I/O thread, exposes configuration options to disable the I/O thread or adjust hot-reload polling, and adds robust fallback paths for single-threaded environments.

### Critical Fixes

*   **Shutdown Safety:** `SituationDestroyThreadPool` now explicitly joins the `io_thread` if it exists. This prevents race conditions where the application would exit while the I/O thread was still accessing memory or filesystem resources.
*   **Fallback Execution:** `SituationSubmitJobEx` now includes a "Synchronous Fallback" path for Low Priority (I/O) jobs. If the I/O thread is disabled (via config or failure), these jobs are executed immediately on the calling thread, preventing infinite stalls.

### New Features

*   **Runtime Configuration:** Added `disable_io_thread` and `hot_reload_poll_rate` to `SituationInitInfo`.
    *   **Disable IO Thread:** Useful for debugging or restricted environments (e.g., WASM) where spawning background threads is undesirable.
    *   **Poll Rate Control:** Developers can now tune the hot-reload frequency (default 0.5s) or disable it entirely (0.0) to save CPU cycles in production.
*   **I/O Metrics:** Added `SituationGetIOQueueDepth()` to monitor pending background tasks.

---

## [v2.3.36 "Velocity" (OpenGL 4.6 Optimization Complete)] - 2025-12-24

### Description

This release marks the completion of the "Max Out Core" OpenGL upgrade plan. It finalizes the transition to a high-performance, parallel-friendly architecture by implementing Multi-Draw Indirect (MDI) batching in the Soft Command Buffer replay loop. This optimization automatically collapses consecutive draw calls into a single driver invocation, significantly reducing CPU overhead for high-count rendering scenarios.

### New Features

*   **Multi-Draw Indirect (MDI):** The OpenGL backend now automatically detects consecutive `SituationCmdDraw` and `SituationCmdDrawIndexed` commands. Instead of issuing individual GL calls, it batches them into a persistent `GL_DRAW_INDIRECT_BUFFER` and executes them with a single `glMultiDraw*Indirect` call.
*   **Persistent MDI Ring Buffer:** Introduced `_SituationInitGLMDIBuffer` to manage a multi-megabyte persistent ring buffer for indirect command data, segmented by frame to ensure thread-safe, lock-free batching.
*   **Bindless Textures:** Completed implementation of `GL_ARB_bindless_texture` logic. `SituationCreateTexture` now automatically retrieves and makes resident a 64-bit handle (`glGetTextureHandleARB`), storing it for high-performance access in shaders.

### Completion Status

*   **Phase 1 (Zero-Copy):** Complete (Persistent Mapping).
*   **Phase 2 (Stateless):** Complete (DSA Adoption).
*   **Phase 3 (Bindless):** Complete (Bindless Textures).
*   **Phase 4 (GPU-Driven):** Complete (MDI Optimizer).

The OpenGL backend now operates with a modern, "console-like" efficiency profile, rivaling Vulkan in many CPU-bound scenarios while maintaining the ease of use of the Situation.

---

## [2.3.35D - Stability & Safety Hardening] - 2025-12-23
This release addresses critical integration issues and runtime safety hazards identified in the v2.3.34 "Velocity" codebase.

#### Critical Fixes
- **Linkage:** Removed erroneous `static` keyword from `SituationGetMeshData` declaration in the public header. This fixes compilation errors when linking against the library.
- **Vulkan Screenshots:** Fixed a severe race condition in `SituationLoadImageFromScreen` (and `SituationTakeScreenshot`). The function now correctly flushes the current command buffer and waits for the GPU to idle before attempting to transition the swapchain image layout, preventing validation errors and driver crashes.

#### Threading & Safety
- **Thread Pool Safety:** `SituationSubmitJobEx` now defaults to **Copy-by-Value** for data payloads larger than 64 bytes. This prevents "Stack Use-After-Free" crashes where a worker thread attempts to read a struct from a stack frame that has already unwound.
    - Added internal flag to track and free these heap allocations automatically.
    - **New Flag:** Added `SIT_SUBMIT_POINTER_ONLY` for advanced users who wish to opt-out of this safety copy (e.g., when passing pointers to static/global data).
- **Audio Callbacks:** Fixed a potential 32-bit truncation issue in the audio stream thunk where `size_t` was implicitly cast to `ma_uint64`, ensuring stability on 32-bit build targets.

#### Backend Internals
- **Vulkan Buffer Usage:** `SituationCreateBuffer` now automatically appends the `VK_BUFFER_USAGE_TRANSFER_DST_BIT` flag. This ensures that buffers created for Uniforms or Storage can be legally updated via `SituationUpdateBuffer` without triggering Vulkan validation errors.
- **Model Saving:** Added a preprocessor guard to `SituationSaveModelAsGltf`. Calls to this function will now trigger a compile-time `#error` if `CGLTF_WRITE_H` is not defined, preventing confusing runtime `NOT_IMPLEMENTED` returns.
- **API Clarity:** Explicitly documented `SituationCmdSetVertexAttribute` as **[OpenGL Only]** to reflect the immutable nature of Vulkan pipelines.

---

## [v2.3.35C - API Refactor & Backend Isolation] - 2025-12-23
- [API] Refactored core resource creation functions to return `SituationError` and output handles via pointers, replacing direct handle returns. This standardizes error handling across the entire API.
  - Updated: `SituationCreateBuffer`, `SituationCreateMesh`, `SituationLoadImage`, `SituationLoadTexture`, `SituationLoadModel`, `SituationCreateTexture`, `SituationCreateTextureEx`.
  - Updated: `SituationCreateComputePipeline`, `SituationCreateComputePipelineFromMemory`.
  - Updated: `SituationLoadImageFromScreen`, `SituationTakeScreenshot`.
- [Fix] Fixed internal variable scoping issues in the new implementations of `SituationCreateBuffer` and `SituationCreateMesh`.
- [Fix] Added missing error checks in `SituationLoadModel` when creating textures.
- [Fix] Verified `SituationRenderVirtualDisplays` backend guards to ensure no regression.
- [Fix] Updated `SituationReloadTexture` implementation to handle the new `SituationLoadImage` signature correctly (though the function itself still returns `bool` for now).

---

## [v2.3.34A "Trinity Threads" (Missing PR Restoration)] - 2025-12-22

### Description

This release restores the "Trinity Threads" architecture changes that were accidentally omitted in a previous merge. It completes the asynchronous I/O vision by introducing a dedicated I/O thread, thread-safe resource registry, and offloading hot-reload polling from the main thread.

### New Features

*   **Dedicated I/O Thread:** Introduced a specialized thread for handling low-priority jobs (Asset Loading) and periodic maintenance tasks.
    *   **Hot-Reload Offloading:** The file system polling for hot-reloading (Shaders, Textures, Models) now runs exclusively on the I/O thread, eliminating file system stalls from the main thread.
    *   **Priority Queue:** The thread pool now strictly segregates High Priority (Physics/Logic) and Low Priority (IO) work, with the I/O thread servicing the latter.

### Architectural Changes

*   **Thread-Safe Resource Registry:** Added a `resource_registry_mutex` to the render state.
    *   **Protected Access:** All `SituationLoad*` and `SituationUnload*` functions now acquire this lock when modifying the global linked lists of tracked resources.
    *   **Safe Traversal:** The hot-reload logic safely iterates these lists under lock, preventing race conditions during concurrent loading/unloading.

### Critical Fixes

*   **Restored Functionality:** Re-integrated the `_SituationIOThreadEntry` function and updated `SituationCreateThreadPool` to spawn the IO thread, ensuring the async architecture functions as designed.

---

## [v2.3.34 "Velocity" (Async I/O & Loader Safety)] - 2025-12-22

### Description

This release fulfills the "Velocity" promise of a complete Asynchronous I/O system. It introduces a fully featured Async Text File API, mirroring the existing binary loaders, allowing developers to load level data, configuration files, and large text blobs on background threads without stalling the main loop. Additionally, it hardens the Hot-Reloading system against race conditions in multi-threaded environments.

### New Features

*   **Async Text API:** Completed the Async I/O suite with `SituationLoadFileTextAsync` and `SituationSaveFileTextAsync`.
    *   **Architecture:** Leverages the `SituationThreadPool` (Small Object Optimization) to dispatch I/O tasks.
    *   **Context Safety:** Inputs (file paths and content) are atomically duplicated (`_sit_strdup`) before job submission, ensuring thread safety and preventing use-after-free errors on the worker thread.
    *   **Callback Model:** Uses `SituationFileTextLoadCallback` to return null-terminated, caller-owned strings directly to the main thread.

### Critical Fixes

*   **Loader Race Condition:** Fixed a thread-safety hazard in the Hot-Reload resource tracking logic for `SituationLoadShader`, `SituationLoadTexture`, and `SituationCreateComputePipeline`.
    *   *The Issue:* Previously, the code assumed the newly created resource would always be at the *head* of the global tracking list (`sit_render.all_*`). In a threaded environment, another thread could insert a resource immediately after creation but before tracking, causing the wrong resource to be tagged.
    *   *The Fix:* The tracking logic now performs a safe linked-list traversal to locate the *exact* resource ID before updating its source path and modification time.

### API Changes

*   **New Typedef:** Added `SituationFileTextLoadCallback` for async text loading results.
*   **New Prototypes:** Added `SituationLoadFileTextAsync` and `SituationSaveFileTextAsync` to the public API.

---

## [v2.3.33A - Cross-Platform Hidden Command Execution] - 2025-12-21
- [Feature] Added `SituationExecuteCommand` to run system shell commands in a hidden window/process while capturing stdout/stderr output.
- [Feature] Implemented cross-platform support using `CreateProcess` (Windows) and `fork/exec/pipe` (Linux/macOS) with output redirection.
- [Safety] Ensures no console windows pop up on Windows and no terminal allocation on Unix-like systems.
- [API] Returns the process exit code and provides a heap-allocated output string that must be freed by the user.

---

## [v2.3.33 "Velocity" - Audio Hardening (Titanium Standard)] - 2025-12-21
- [Audio] Implemented the "Titanium Standard" Audio Action Plan (Section 4 of Audio Analysis).
- [Safety] Enforced consistent locking across all audio setters (`SituationSetSoundVolume`, `SituationSetSoundPan`) to eliminate data races.
- [Optimization] Converted real-time audio parameters (`volume`, `pan`, `pitch`) to `_Atomic float` for lock-free access on the mixing thread.
- [Architecture] Introduced a Generational Handle System (`SituationSoundHandle`) to replace raw pointers, enabling O(1) validation and eliminating Use-After-Free errors.

---

## [v2.3.32G - Cross-Platform CPU Thread Count Utility] - 2025-12-21
- [Feature] Added `SituationGetCPUThreadCount` to reliably query the number of logical CPU cores on Windows, macOS, and Linux.
- [Improvement] Updated `SituationGetDeviceInfo` to use the new utility, standardizing `cpu_cores` to report logical cores across all platforms (fixing macOS inconsistency).
- [Improvement] Updated `SituationCreateThreadPool` to use the new utility for auto-detecting thread counts, replacing ad-hoc logic.

---

## [v2.3.32F - Compute Limits Helper (Max Work Groups)] - 2025-12-21
- [Feature] Added `SituationGetMaxComputeWorkGroups` to query hardware limits for local work group counts (X, Y, Z) per dispatch.
- [Feature] Implemented backend-specific limit queries for both Vulkan (`maxComputeWorkGroupCount`) and OpenGL (`GL_MAX_COMPUTE_WORK_GROUP_COUNT`).
- [Safety] Added `SituationIsInitialized` checks to `SituationGetMaxComputeWorkGroups` to prevent unsafe access to internal state.

---

## [v2.3.32E - SituationError Return Type Migration & Docs] - 2025-12-20
- [Breaking Change] Updated `SituationCmd*` functions to return `SituationError` instead of `void` for better error propagation (e.g., `SituationCmdDraw`, `SituationCmdEndRenderPass`).
- [Breaking Change] Updated `SituationCmdDraw` and `SituationCmdDrawIndexed` parameter types (`int` -> `uint32_t`) and added `instance_count` to support instanced rendering directly.
- [Docs] Updated `situation_api.md` to reflect new signatures and added documentation for `SituationCmdDrawText`, `SituationCmdDrawTextEx`, and `SituationCmdPresent`.
- [Examples] Updated `examples/handling_keyboard_and_mouse_input.c` to use `Vector4` and fix `SituationGetMousePosition` usage.

---

## [v2.3.32D - Terminal VT UTF-8 & REP Support] - 2025-12-20
- [Feature] Implemented UTF-8 decoding in `ProcessNormalChar` (Terminal), enabling full multibyte Unicode support (e.g., Box Drawing characters, international text).
- [Feature] Implemented `MapUnicodeToCP437` helper to map decoded Unicode codepoints to the internal CP437 font atlas indices.
- [Feature] Implemented `ExecuteREP` (CSI b) for Repeat Preceding Graphic Character, significantly optimizing rendering for repetitive text patterns.
- [Fix] Hardened `ProcessNormalChar` state machine to robustly handle invalid UTF-8 sequences by resetting state and reprocessing the byte.
- [Fix] Fixed potential logic duplication in `ExecuteREP` by reusing core insertion logic.

---

## [v2.3.32C - Complete VT Support (Sixel, Soft Fonts, Window Ops, Pipeline Fix)] - 2025-12-20
- [Critical] Fixed `SIT_COMPUTE_LAYOUT_TERMINAL` in `situation.h` to include the 4th descriptor set (Sixel texture sampler), ensuring the Vulkan pipeline matches the Compute Shader expectations.
- [Feature] Implemented `ProcessSoftFontDownload` (DECDLD) in `sit/terminal/terminal.h` with robust Sixel-encoded bitmap decoding and texture atlas regeneration.
- [Feature] Updated `CreateFontTexture` to seamlessly support active Soft Fonts, falling back to the built-in font for missing glyphs.
- [Feature] Implemented `ExecuteWindowOps` (CSI t), mapping terminal sequences to `Situation` window management APIs (Resize, Move, Restore, Minimize, Maximize, Fullscreen).
- [Feature] Wired up `DrawSixelGraphics` to trigger dirty state updates for texture uploads.
- [Improvement] Added support for standard DECDLD format (`DCS ... {`) in `ExecuteDCSCommand`.

---

## [v2.3.32B - Complete VT Sixel Support & Logging API (Terminal Deep Dive)] - 2025-12-19
- Implemented `ProcessSixelData` in `sit/terminal/terminal.h` for full Sixel graphics parsing support.
- Added `SituationLog` and `SituationSetTraceLogLevel` to `situation.h` with ANSI color-coded output.
- Fixed Linux compilation issue (`IFF_LOOPBACK` undefined) by adding `_DEFAULT_SOURCE`.
- Verified and fixed missing function definitions in the single-header implementation.

# Situation Update Log

This document tracks the evolution of the Situation library, detailing new features, architectural changes, and critical fixes.

---

## [2.3.32A "Velocity" (VT Console Support)] - 2025-12-14

### Description

This update introduces native Virtual Terminal (VT) support for the Windows console subsystem. This enhancement enables correct rendering of ANSI escape codes in `cmd.exe` and PowerShell, allowing for colored text output in logs and diagnostic messages. This aligns the Windows development experience with Linux and macOS, where ANSI support is standard.

### New Features

*   **Windows Console VT Support:** Added logic to `_SituationInitPlatform` to explicitly enable `ENABLE_VIRTUAL_TERMINAL_PROCESSING` on the standard output and error handles. This ensures that `SituationLogWarning` and other console output functions can use color coding for better readability.

---

## [2.3.32 "Velocity" (Vulkan 1.4 Upgrade)] - 2025-12-14

### Description

This release updates the engine to target **Vulkan 1.4**, preparing the architecture for modern high-performance rendering techniques such as full Bindless Descriptor support and Dynamic Uniform Buffer Objects (Dynamic UBOs). This strategic update aligns the library with the latest industry standards used in AAA development, enabling more efficient GPU resource management and execution.

### Architectural Updates

*   **Vulkan 1.4 Target:** The `VkApplicationInfo` and `VmaAllocatorCreateInfo` structures now explicitly request `VK_API_VERSION_1_4`. This ensures the application is initialized with a Vulkan 1.4 context, unlocking access to core features like `VK_KHR_dynamic_rendering`, `VK_KHR_maintenance4`, and improved synchronization primitives that were previously extensions.
*   **Documentation Alignment:** All documentation and version strings have been updated to reflect the new API target. The README.md section order has also been corrected for better readability.

---

## [2.3.31A "Velocity" (Hotfix: Compilation & Thread Safety)] - 2025-12-14

### Description

This is a critical hotfix addressing several compilation errors and thread-safety hazards introduced during the recent texture system refactor. It ensures proper visibility of internal helpers, fixes type mismatches in mutex usage, and corrects structural errors in the render thread loop.


### Critical Fixes

*   **Compilation Fix:** Moved the `_SitGetTextureSlot` forward declaration to the top of the implementation block to resolve "implicit declaration" errors and visibility issues with internal structs.
*   **Mutex Safety:** Resolved a type mismatch where `sit_render.momentum_mutex` and `sit_audio.audio_queue_mutex` (declared as C11 `mtx_t`) were being accessed via Miniaudio's `ma_mutex_*` API. Replaced all invalid calls with standard C11 `mtx_*` equivalents (`mtx_lock`, `mtx_unlock`, `mtx_destroy`), ensuring correct locking behavior and preventing undefined behavior.
*   **Render Thread Logic:** Fixed a syntax error in `_SituationRenderThreadEntry` where a missing closing brace caused compilation failure.
*   **API Resilience:** Added missing return statements to `SituationCmdBeginRenderToDisplay` to prevent "control reaches end of non-void function" warnings.


### Validation

*   **Compilation:** Clean compilation with `-Wall -Wextra` on standard GCC setup.
*   **Thread Safety:** Verified mutex initialization and locking calls align with C11 threading primitives.


---

## [2.3.31 "Velocity" (Texture System Refactor)] - 2025-12-13

### Description

This release executes a major refactor of the Texture System to align with Bindless architecture standards. It replaces direct texture handles with a Registry ID system (Generation + Index), enabling robust hot-reloading and eliminating use-after-free risks for GPU resources.


### Architectural Changes

*   **Registry ID System:** Textures are now referenced by a 64-bit ID combining a generation counter and a slot index. This allows O(1) lookups while preventing access to stale or destroyed resources.
*   **Bindless Compliance:** The new ID structure prepares the engine for full Bindless Descriptor support, where resources are accessed directly by index in shaders.
*   **Safe Hot-Reloading:** The Registry system ensures that reloading a texture updates the underlying GPU resource while keeping the handle ID valid for the user application, or safely invalidates it if necessary.


---

## [v2.3.30A (Hotfix) - Performance & Roadmap Correction]

### Description

- [Fix] Performance Regression: Removed `glGetIntegerv` from `SIT_OP_DRAW_QUAD` handler.
- Replaced slow driver query with local state tracking (`current_bound_texture_id`) in `_SituationGLExecuteCommands`.
- Eliminates pipeline stall when drawing quads (e.g., UI/Debug) in OpenGL backend.
- [Doc] Updated README roadmap to correctly reflect that Dynamic UBOs were implemented in v2.3.29.

---

## [2.3.30 "Velocity" (Bindless Revolution)] - 2025-12-13

### Description

This release introduces the Bindless Texture architecture, a transformative optimization for the OpenGL backend. It eliminates the need for manual texture unit management by treating textures as resident, 64-bit GPU handles. This significantly reduces CPU overhead in high-draw-count scenarios (e.g., UI, particles) and prepares the API for the upcoming unified bindless model.

### New Features

*   **Bindless Texture Support (OpenGL):** Implemented full support for `GL_ARB_bindless_texture`.
*   **Handle Retrieval:** Added `SituationGetTextureHandle()` to retrieve 64-bit resident handles for textures on OpenGL.
*   **Internal Optimization:** Updated internal Quad (`SituationCmdDrawQuad`) and Text (`SituationCmdDrawText`) renderers to automatically utilize bindless handles when available, bypassing `glBindTextureUnit` calls entirely.
*   **Shader Support:** Added internal shader capabilities for `GL_ARB_gpu_shader_int64` to support 64-bit sampler types.

---

## [2.3.29 "Velocity" (Dynamic UBOs)] - 2025-12-13

### Description

This release implements proper support for Dynamic Uniform Buffer Objects (Dynamic UBOs) in the Vulkan backend, enabling efficient rendering by binding different ranges of a single large buffer without re-binding descriptor sets. This addresses a key performance limitation in scenarios requiring frequent per-draw data updates.


### New Features

*   **Dynamic UBO Support:** Introduced `SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM`. When creating a buffer with this flag, the Vulkan backend now allocates a descriptor set using the new `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` layout.
*   **Dynamic Binding API:** Added `SituationCmdBindDescriptorSetDynamic`. This new API allows binding a descriptor set with a dynamic offset, essential for utilizing the Dynamic UBO capability.
*   **Backward Compatibility:** Updated `SituationCmdBindDescriptorSet` to seamlessly handle dynamic buffers by defaulting the offset to 0, ensuring existing code continues to function correctly while enabling the new optimization.


### Architectural Changes

*   **Dynamic Layout:** A new `dynamic_ubo_layout` is initialized in the Vulkan render state to support the dynamic descriptor type.
*   **Cleanup:** The system correctly cleans up the new layout resource during shutdown.


---

## [2.3.28 "Titanium Core C" (Velocity & Concurrency)]

### Description

The final installment of the "Titanium" stability trilogy, focusing on eliminating architectural bottlenecks and CPU overhead. While v2.3.27 fixed crashes, v2.3.28 achieves "Zero-Allocation" behavior in hot paths. This release introduces the "Velocity" Ring Buffer for Vulkan, a "Snapshot-and-Unlock" audio mixer, and intelligent resource recycling, resulting in a dramatic reduction in frame-time variance and CPU usage.


### Critical Fixes

*   **Vulkan Buffer Velocity (Staging Ring Buffer):** Replaced the per-update allocation strategy in `SituationUpdateBuffer` with a persistent, mapped, per-frame ring buffer (32MB default). Data uploads now use a fast `memcpy` path with zero API calls or allocations, eliminating the #1 cause of micro-stutter in dynamic scenes. Includes a robust fallback allocator for overflow cases.
*   **Audio Concurrency (Snapshot-and-Unlock):** Overhauled `sit_miniaudio_data_callback` to minimize mutex contention. The mixer now snapshots the active sound list and releases the lock *before* processing effects/mixing. Added an atomic `is_processing_snapshot` guard to `SituationUnloadSound` to prevent Use-After-Free race conditions with zero regressions.
*   **Render Thread Efficiency (No Spinlock):** Removed the busy-wait spinlock in `SituationEndFrame` backpressure logic. Replaced with a Condition Variable (`cnd_wait`/`cnd_signal`) synchronization model. The main thread now sleeps (0% CPU) rather than spinning (100% Core Usage) when the GPU queue is full, significantly reducing battery drain and thermal throttling.
*   **Descriptor Pool Recycling (Best-Fit):** Upgraded `_SituationVulkanAllocateDescriptorSet` from a "Linear Growth" to a "Recycling" strategy. The allocator now scans existing pools for freed slots (reclaimed via the Graveyard) before creating new pools, preventing unbounded memory growth during long sessions with frequent level loads.
*   **Hot-Reload IO Debounce:** Throttled `SituationCheckHotReloads` to a 2Hz polling rate (down from 60Hz+). Added `#ifndef NDEBUG` guards to compile the system out entirely in Release builds. This eliminates thousands of redundant filesystem syscalls per second, resolving CPU spikes in development builds.


### KNOWN LIMITATIONS (Deferred to v2.4)

*   **Render Graph:** Manual barriers are still required for complex compute-to-graphics dependencies. v2.4 will introduce automatic barrier insertion.
*   **Vulkan Pipeline Cache:** Pipeline creation still compiles from SPIR-V every run. v2.4 will implement on-disk `VkPipelineCache` serialization for faster startup.


### Validation

*   **Performance:** `SituationUpdateBuffer` call overhead reduced by ~98%.
*   **Thermals:** Main thread CPU usage dropped from ~15% to <1% in GPU-bound scenarios due to spinlock removal.
*   **Audio:** Seamless playback of new sounds while mixing heavy reverb loads; no main-thread stalling.
*   **Stability:** 24-hour soak test with random asset loading/unloading showed stable VRAM usage (Descriptor Recycling).


---

## [2.3.27B "Titanium Core B" (Hardening Patch)] - 2025-12-12

### Description

A rapid-response hardening patch building on v2.3.27, addressing post-release audit findings for concurrency deadlocks, memory leaks, and race conditions. This release fortifies the library's core invariants—ensuring recursive safety in audio callbacks, leak-free descriptor management, and race-proof render list handling—elevating it from "Production-Ready" to "Audit-Proof" for mission-critical deployments.


### Critical Fixes

*   **Audio Deadlock Prevention (Recursive Mutex):** Resolved recursive locking hazards in `sit_miniaudio_data_callback` where user processors could trigger API calls (e.g., `SituationPlayLoadedSound`) under the same mutex. Switched to `mtx_recursive` initialization for safe nesting without deadlocks.
*   **Vulkan Descriptor Leak Fix (Pool Recycling):** Re-enabled `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT` and restored `vkFreeDescriptorSets` in `SituationDestroyTexture` et al., with refcounted pool recycling to eliminate OOM crashes in asset-streaming workloads (e.g., open-world unloads).
*   **Momentum Race Guard (In-Flight Tracking):** Added atomic `in_flight_count` to `SituationRenderList` structs. `SituationSubmitRenderList` increments on enqueue; `SituationReplayRenderList` decrements post-execution. `SituationResetRenderList` now spin-waits or errors on active lists, preventing data corruption during MT replays.
*   **Soft Command Buffer Resilience (Realloc Guards):** Hardened `_SitGLSoftCmdPush` and `_SitGLSoftDataPush` with explicit failure paths: On `SIT_REALLOC` OOM, mark the buffer invalid and abort frame commands, avoiding dangling pointers and partial renders.
*   **Input Unified Processing (Atomic Polling):** Consolidated joystick event handling into `SituationPollInputEvents` (from `SituationUpdateTimers`), ensuring consistent state queries and eliminating lag windows between poll and logic phases.
*   **Hot-Reload TOCTOU Safety (Staged Validation):** Refined `SituationReloadTexture` (and siblings) to stage new resource creation in temp buffers before destroying old ones, preventing black screens from mid-save file locks.
*   **Global Context Threading (TLS Safeguard):** Enforced thread-local checks on `_sit_current_context` access with explicit guards in all API entrypoints, mitigating data races in unauthorized MT usage while preserving singleton semantics.
*   **Swapchain Recreate Sync (Barrier Hardening):** In `SituationAcquireFrameCommandBuffer` and `SituationEndFrame`, added pre-present validity checks and immediate aborts on pending recreates, averting validation errors during resize storms.


### KNOWN LIMITATIONS (Deferred to v2.4)

*   **Vulkan Dynamic UBOs:** `SituationUpdateBuffer` still relies on staging for non-dynamic paths; serialization persists for large buffers. v2.4 will fully implement `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` with ring versioning.
*   **Render Graph Absence:** Manual barriers remain brittle for complex passes; auto-aliasing deferred to v2.4 for VRAM optimization.


### Validation

*   **Concurrency Stress:** 100+ recursive audio callbacks (user processors calling Play/Stop) no deadlocks; TSan clean under 10k iterations.
*   **Memory Audit:** 1M texture load/unload cycles: Zero leaks (Valgrind/ASan); descriptor pools recycle without fragmentation.
*   **MT Replay Test:** 4-worker submits of in-flight lists: No races/corruption; 20k packets at 144FPS stable.
*   **Input/Resize Fury:** Rapid poll-logic queries + window resizes: Zero lag or validation errors; consistent joystick state.
*   **Hot-Reload Edge:** Editor-save collisions during reload: Graceful fallbacks, no asset loss.


---

## [2.3.27 "Titanium Core" (Architectural Hardening)] - 2025-12-11

### Description

A comprehensive stability overhaul addressing critical thread-safety hazards, memory fragmentation, and cross-backend parity. This release transforms the library from "Functional" to "Production-Ready" by eliminating race conditions in the Audio and Rendering subsystems and optimizing high-frequency text rendering.


### Critical Fixes

*   **Audio Safety (Lock-the-World):** Fixed a Use-After-Free race condition where unloading a sound during playback could crash the audio thread. Implemented a robust mutex strategy and fused mixing loop for stability.
*   **Vulkan Text Perf (Ring Buffer):** Replaced per-draw buffer allocations with a persistent mapped ring buffer. Text rendering is now zero-copy and allocation-free in the hot path.
*   **Momentum Thread Safety:** Decoupled Render List submission from execution. `SituationSubmitRenderList` now safely enqueues pointers; `SituationEndFrame` replays them serially on the main thread, preventing Vulkan command buffer corruption.
*   **OpenGL State Hardening:** `_SituationGLExecuteCommands` now explicitly resets critical GL state (Blend, Depth, Cull) before execution, preventing "state poisoning" from external middleware (e.g., ImGui).
*   **Vulkan Descriptor Stability:** Switched to a Linear "Allocate-Only" strategy for descriptors. Removed `vkFreeDescriptorSets` calls to prevent pool fragmentation crashes during long sessions.


### KNOWN LIMITATIONS (Deferred to v2.3.x)

*   **Vulkan Dynamic UBOs:** `SituationUpdateBuffer` currently uses a staging path with barriers. Updating the same UBO multiple times per frame is safe (correct barriers added) but serializes execution on the GPU. Future v2.3.x will implement `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` for high-performance versioned updates.


### Validation

*   **Stress Test:** 50+ concurrent sounds with reverb/echo no longer crash on unload.
*   **UI Test:** Rendering 1000+ text labels per frame no longer spikes CPU/VRAM usage.
*   **Thread Test:** Parallel submission of Render Lists from 4 worker threads is now stable.


---

## [2.3.26 "Silent Zenith" (Micro-Polish)] - 2025-12-10

### Description

Tweaks v2.3.25 metrics: Release-mute warns, namespace unify, retry thresh 20—silent prod.


### Fixes

*   **Warn Mute:** #ifndef NDEBUG on drift log.
*   **Namespace:** sit_frame_ -> sit_render.
*   **Retry:** Log >20 for early hint.


### Validation

*   Silent under load; consistent.


---

## [2.3.25 "Polish Zenith" (Micro-Hotfix)] - 2025-12-10

### Description

Tweaks v2.3.24b metrics: Namespace unify, TS store, once-warn drift, retry log—silent & accurate.


### Fixes

*   **Namespace:** sit_frame_ -> sit_render—consistency.
*   **TS Store:** Push monotonic—non-zero latencies.
*   **Warn Polish:** Drift log once; retries >50 flagged.


### Validation

*   1k frames: Accurate avg/max; no spam; TSan clean.


---

## [2.3.24b "Integration Zenith"] - 2025-12-10

### Description

Integrates PR1 safety: Batched replay (multi-queue), init val (sema checks), histogram export (JSON safe)—overlap + tuning.


### Integration

*   **Batched Replay:** One compute submit post-barrier; graphics waits DRAW_INDIRECT_BIT.


### Validation

*   **Init Suite:** Sema/queue scans; fallback warn.
*   **Export Guard:** Min 256B JSON.


### Validation

*   20k packets 140FPS; Valgrind clean.

---

## [2.3.24a "Safety Zenith"] - 2025-12-10

### Description

Hardens v2.3.23 MT: Race-free refcounts, FPS-relative adaptive policies, basic histogram—leak-free, self-tuning queue.


### Safety

*   **Refcounts:** fetch_sub==1 flush—0 leaks 20k handoffs.
*   **Adaptive:** Relative thresh (144Hz=6.9ms spike → SLEEP).


### Metrics

*   **Basic Histogram:** Sum/count avg; max atomic.


### Validation

*   <0.05ms overhead; TSan clean.

---

## [2.3.23 "Velocity" (Multi-Queue & ARM Hotfix)] - 2025-12-09

### Description

Elevates v2.3.22 MT with VK sema-synced multi-queue (compute/graphics overlap), ARM wfe/yield spins (<10% CPU), and zero-config overlay (8x8 default font)—mobile/prod decoupling.


### Performance

*   **VK Multi-Queue:** Sema waits + concurrent sharing; 1.5x FPS on compute/draw (e.g., 8k particles).
*   **ARM Spins:** wfe primary, yield fallback—no deadlocks, battery-friendly.


### Debug Ux

*   **Metrics Overlay:** NULL-safe DrawText (hardcoded font); depth bars + latency text.


### Safety

*   **Force Single-Queue:** InitInfo opt-in for debug; new -86 for ARM intrinsics fail.


### Validation

*   **VK:** Overlap 130FPS (vs. 90); no ownership barriers.
*   **ARM:** QEMU <5% CPU; fallback yield clean.
*   **Overlay:** <0.3ms; TSan/Valgrind ok.


---

## [2.3.22 "Velocity" (Backpressure & Metrics Hotfix)] - 2025-12-07

### Description

Armors v2.3.21 MT with hybrid backpressure (pause-hinted spins), Momentum queuing (replay → auto-push), and drift-proof latency metrics—burst-resilient decoupling.


### Resilience

*   **Hybrid Backpressure:** Policies (SPIN w/ _mm_pause/ARM yield | YIELD | SLEEP 1ms); EndFrame applies on full queue.
*   **ARM64 Compatibility:** Added `__yield()` support for MSVC ARM64 (_M_ARM64) builds in spin loops.


### Integration

*   **Momentum Bridge:** Replay lists to per-frame cmds → queue indices; reset safe via offsets.
*   **Render Lists:** Promoted `_SituationQueueRenderList` to public `SituationSubmitRenderList` for streamlined render list submission.


### Metrics

*   **Latency Stats:** Monotonic clocks (CLOCK_MONOTONIC/QPC); `GetRenderLatencyStats(avg/max_ns)` for stutter hunts.


### Safety

*   **Timed Joins:** 1s loop (100ms ticks, log every 500ms); new `-84` for timeouts.


### Validation

*   SPIN: <50% CPU during bursts; no thrash.
*   Latency: ±1ns; no neg on drifts.
*   Replay: 10k packets MT → FPS match.


---

## [2.3.21 "Velocity" (Render Thread Polish)] - 2025-12-06

### Description

This release polishes the v2.3.20 render thread isolation with four targeted refinements, boosting robustness, debuggability, and integration without perf hits or API breaks. It addresses GL context pitfalls, graceful shutdowns, queue visibility, and seamless integration for MT-enabled flows.


### New Features

*   **Metrics API:** Introduced `SituationGetRenderQueueDepth()` to expose the current depth of the render thread's queue. This allows UI overlays to visualize backpressure (e.g., "Queue: 2/3") in real-time.
*   **EndFrame Integration:** `SituationEndFrame` now automatically queues frames when the render thread is enabled. Users enable threading via `SituationInitInfo.render_thread_count > 0`, and the loop handles the rest transparently.


### Critical Fixes & Safety

*   **Context Handover (GL):** Implemented `_SituationInitRenderThread` with explicit GL context release on the main thread before spawning the render thread. This prevents "current context" conflicts that could crash drivers on thread startup.
*   **Shutdown Robustness:** Hardened `_SituationDestroyRenderThread` with a comprehensive shutdown sequence:
*   Sets shutdown flag.
*   Broadcasts condition variables to wake both producer (Main) and consumer (Render) threads if blocked.
*   Joins the thread to ensure clean termination before resource cleanup.
*   Releases the GL context on the render thread before exit.
*   **Error Reporting:** Added `SITUATION_ERROR_THREAD_CREATION_FAILED` (-83) to report thread spawning failures specifically.


---

## [2.3.20 "Velocity" (Phase 2.5: High-Performance Mesh Architecture)] - 2025-12-06

### Description

This PR implements Phase 2.5 of the rendering engine refactor, introducing a High-Performance Mesh Architecture with a Lazy VAO Cache.


### Key Changes

*   **Lazy VAO Cache:** Replaces the shared global VAO with a per-mesh VAO cache. VAOs are created and configured lazily on the Render Thread inside `_SituationGLExecuteCommands` using `_SitGLGetCachedVAO`. This restores optimal VAO usage while respecting OpenGL context rules.
*   **OpenGL Graveyard:** Implements a deferred deletion system (`_SituationGLGraveyard`). Resources destroyed on the Main Thread (`SituationDestroyMesh`, etc.) are queued and safely deleted on the Render Thread via `_SitGLFlushGraveyard` to prevent race conditions.
*   **Renaming:** Renamed internal graveyards to `_SituationVKGraveyard` and `_SituationGLGraveyard` for clarity.


---

## [2.3.19 "Velocity" (Phase 2: Threading Infrastructure)] - 2025-12-06

### Description

This release delivers the core infrastructure for the decoupled rendering system (Phase 2). It successfully establishes the Render Thread, Frame Queue, and Context Handover mechanisms for both OpenGL and Vulkan, enabling the main thread to produce frames while the background thread consumes and renders them. This separation paves the way for higher frame rates and smoother gameplay by unblocking logic updates from VSync.


### Architectural Changes

*   **Render Thread:** Implemented `_SituationRenderThreadEntry`. The main thread records commands into double-buffered Soft Command Buffers (GL) or uses standard Vulkan Command Buffers, which are pushed to a thread-safe ring buffer and consumed by the Render Thread.
*   **Context Handover (OpenGL):**
*   **Main Thread (Loader):** Now manages a hidden `loader_window` (shared context) for async asset loading.
*   **Render Thread (Presenter):** Takes ownership of the main window context (`glfwMakeContextCurrent`) to perform presentation (`glfwSwapBuffers`).
*   **Vulkan Threading:**
*   **Queue Submission:** `vkQueueSubmit` and `vkQueuePresentKHR` are now executed exclusively on the Render Thread to prevent driver stalls on the Main Thread.
*   **Swapchain Signal:** Introduced `recreate_swapchain_request` (atomic bool). The Render Thread signals this flag if presentation fails (`VK_ERROR_OUT_OF_DATE_KHR`), and the Main Thread handles the recreation logic safely during the next Acquire phase.
*   **Image Index Tracking:** Added `acquired_image_indices` array to robustly track which swapchain image corresponds to which frame slot, solving race conditions between the Main Thread's `vkAcquireNextImageKHR` and the Render Thread's `vkQueuePresentKHR`.
*   **Frame Queue & Backpressure:** Implemented a ring buffer for frame submission. `SituationAcquireFrameCommandBuffer` now waits on a condition variable if the queue is full (Backpressure), ensuring the main thread doesn't overrun the GPU.


### Critical Fixes

*   **Race Condition Prevention:** `SIT_OP_BEGIN_RENDER_PASS` and `SIT_OP_PRESENT` now capture the current window resolution at record-time (on the Main Thread), preventing race conditions where the Render Thread might read a changing `main_window_width/height` during a resize event.
*   **Fallback Path:** Restored the non-threaded execution path in `SituationEndFrame`. The library continues to function correctly (albeit synchronously) on platforms without C11 thread support (`__STDC_NO_THREADS__`).


### DEFERRED IMPLEMENTATION (Phase 2.5)

*   **Shared Global VAO Strategy:** To resolve immediate context sharing issues (VAOs are not shared between contexts), the current implementation uses a single, global shared VAO (`mesh_vao_id`) for all meshes.
*   *Impact:* This works correctly but incurs a small CPU overhead as VBOs must be re-bound for every draw call.
*   *Plan:* Phase 2.5 will introduce a "Lazy Per-Mesh VAO Cache" on the Render Thread to restore maximum performance by caching fully configured VAOs.


---

## [2.3.18A "Velocity" (Hotfix: Phase 1 Completion)] - 2025-12-05

### Description

This release completes Phase 1 of the OpenGL backend refactor by implementing the missing deferred operations for buffer updates and vertex attribute configuration. This ensures that all `SituationCmd*` and `SituationUpdate*` calls respect the "Soft Command Buffer" architecture, preventing any immediate GL calls during the render recording phase.


### Critical Fixes

*   **Deferred Buffer Updates:** Implemented `SIT_OP_UPDATE_BUFFER`. `SituationUpdateBuffer` now correctly records an update packet instead of calling `glNamedBufferSubData` immediately.
*   **Deferred Vertex Attributes:** Implemented `SIT_OP_SET_VERTEX_ATTRIBUTE`. `SituationCmdSetVertexAttribute` now records a configuration packet.
*   **Soft Command Buffer Execution:** Updated `_SituationGLExecuteCommands` to handle the new opcodes, ensuring data and state changes occur in the correct order during replay.


---

## [2.3.18 "Velocity" (Phase 1: Deferred OpenGL)] - 2025-12-05

### Description

This release implements Phase 1 of the OpenGL backend refactor, introducing a Soft Command Buffer for deferred execution. It addresses a critical regression where `SIT_OP_DRAW_MESH` would leave a mesh-specific VAO bound, breaking subsequent generic draw calls. It also fixes a memory leak in the soft buffer cleanup logic.


### Architectural Changes

*   **Soft Command Buffer:** Introduced `SituationSoftCommandBuffer` and `SitOpCode` infrastructure for OpenGL.
*   **Deferred Execution:** Refactored all `SituationCmd*` functions to record packets instead of executing GL calls directly. Implemented `_SituationGLExecuteCommands` to replay recorded commands at `SituationEndFrame`.


### Critical Fixes

*   **VAO State Corruption:** `SIT_OP_DRAW_MESH` now explicitly restores `sit_render.gl.global_vao_id` after execution. This prevents subsequent generic draw calls (like `SituationCmdDrawQuad`) from failing due to incorrect vertex attribute bindings.
*   **Memory Leak:** Added cleanup logic for `soft_buffer.packets` and `soft_buffer.data_buffer` in `_SituationCleanupOpenGL`.


---

## [2.3.17 "Velocity" (Refactor: Render State Separation)] - 2025-12-05

### Description

This release implements a massive architectural refactor to decouple the rendering state from the core global state. This change is purely structural and preserves identical API behavior and performance, but paves the way for future multi-context support and cleaner internal modularity.


### Architectural Changes

*   **State Decoupling:** The monolithic `_SituationGlobalStateContainer` has been split. A new `_SituationRenderState` struct now encapsulates all graphics-related state (Vulkan handles, OpenGL context data, virtual displays, resource trackers).
*   **Context Object:** Introduced a heap-allocated `SituationContext` that holds the Global, Render, Audio, and Input states, replacing static global variables with a structured context pointer.
*   **Access:** Internal access is now routed through context-aware macros (`sit_render`, `sit_gs`, etc.) that resolve to the active context instance.
*   **Memory Management:** `SituationInit` now allocates the context on the heap (previously BSS/static), and `SituationShutdown` frees it. This ensures cleaner memory lifetime management and better compatibility with hot-reloading entire DLLs.


### Migration

*   **Internal:** All internal references to `sit_gs.vk` or `sit_gs.gl` have been updated to `sit_render.vk` and `sit_render.gl`.
*   **Public API:** No breaking changes to the public API surface.


---

## [2.3.16A "Velocity" (Hotfix: Vector & Docs)] - 2025-12-03

### Description

This release solidifies the API type system and documentation. It replaces legacy `vec2`/`vec3`/`vec4` array typedefs with C11-compliant `Vector2`/`Vector3`/`Vector4` unions, ensuring strict standards compliance and resolving ambiguity. It also elevates the threading module documentation to "Titanium" standards.


### Api Changes

*   **Vector Standardization:** Replaced `vec2`, `vec3`, `vec4` array typedefs with `Vector2`, `Vector3`, `Vector4` unions.
*   **Impact:** Access members via `.x`, `.y`, etc., instead of array indexing `[0]`. Use `.raw` for CGLM interop.
*   **Compatibility:** This is a breaking change for code directly accessing vector components via array syntax on Situation structs.


### Documentation

*   **Titanium Threading Docs:** Moved all detailed threading documentation from `situation.h` header declarations to the implementation section, ensuring a clean API surface while maintaining exhaustive developer reference.
*   **Cleanup:** Fixed typos in `SituationWaitForAllJobs` and `SituationLoadSoundFromFileAsync` documentation.


---

## [2.3.16 "Velocity" (Task Safety Hotfix)] - 2025-12-03

### Description

Bolsters v2.3.15 tasks with lock-free dep linking (CAS), cycle guards (depth traversal), and graph viz—deadlock-proof chains for physics/cull flows.


### Safety Enhancements

*   **Cycle Detection:** Traverse cont chains in `AddJobDependency`; error/assert on loops (>32 hops/self). New: `-82` code.
*   **Lock-Free Links:** CAS for `continuation_id`; atomic dec + cond signal on ready.
*   **HoL Mitigation:** Workers skip blocked deps, yield on full-block.


### Debug Tools

*   **Graph Dump:** `SituationDumpTaskGraph` snapshots active jobs (prio/depth/deps); JSON mode for scripts. Racy warning included.


### Api

*   `AddJobDependencies` for fan-in (N prereqs → 1 dep).
*   Examples: `task_graph_demo.c` (chains + injects).


### Validation

*   Cycles: 100% detected (loops/self/deep).
*   Perf: <0.1% overhead; TSan/Valgrind clean.
*   C11: Atomics explicit; no extra deps.


---

## [2.3.15 "Velocity" (Generational Task System)] - 2025-12-02

### Description

This release replaces the previous basic threading implementation with a hardened Generational Task System. It introduces O(1) job tracking, dual priority queues (High/Low) to prevent asset loading from stalling gameplay physics, and "Small Object Optimization" to remove malloc overhead for 95% of tasks.


### New Features

*   **Generational Ring Buffer:** Replaced the linear-scan thread pool with a Generational Ring Buffer System, enabling O(1) job submission and tracking.
*   **Dual Priority Queues:** Jobs can now be submitted to High (Physics/Logic) or Low (Assets/IO) priority queues. Workers prioritize the High queue to prevent frame spikes.
*   **Small Object Optimization (SOO):** Job payloads <= 64 bytes are now embedded directly in the job structure, eliminating heap allocation overhead for most tasks.
*   **Parallel Dispatch:** Added `SituationDispatchParallel` for easy fork-join parallelism (parallel-for loops). The calling thread actively participates in execution ("helping") to prevent stalls.
*   **Advanced Submission Control:** Introduced `SituationSubmitJobEx` with flags for Backpressure handling:
*   `SIT_SUBMIT_BLOCK_IF_FULL`: Spin/yield until a slot opens.
*   `SIT_SUBMIT_RUN_IF_FULL`: Execute immediately on the calling thread if the queue is full.
*   **Generational IDs:** `SituationJobId` now packs a generation counter to prevent ABA problems and allow safe O(1) validity checks.


### Api Changes

*   Replaced previous threading API with the new Generational Task System API.
*   Updated `SituationLoadSoundFromFileAsync` to utilize the new Generational Task System (now utilizes Small Object Optimization for zero-allocation submission).
*   Added `<threads.h>` and `<stdatomic.h>` as hard dependencies when `SITUATION_ENABLE_THREADING` is defined (C11 support required).


---

## [2.3.14A "Velocity" (Stability / Bug Fix / Compatibility)] - 2025-12-02

### Description

This patch hardens the "Velocity" architecture, focusing on audio stability, graphics compatibility, and backend robustness. It addresses critical issues that could cause audio dropouts, crashes with legacy meshes, and state desynchronization in OpenGL.


### Critical Fixes

*   **Audio Snapshot Mixing:** Replaced the v2.3.14 `try_lock` strategy in the audio callback with a Snapshot-Mixing strategy. This reduces the critical section to pointer copying only (O(1)), eliminating silence and dropouts during main-thread contention (e.g., asset loading).
*   **Graphics Auto-Padding:** Added an Auto-Padding Layer to `SituationCreateMesh`. Legacy 32-byte vertex data (Pos/Norm/UV) is now automatically detected and upgraded to the required 48-byte format (Pos/Norm/Tan/UV) by inserting default tangents. This prevents crashes and validation errors when using new PBR shaders with older assets.
*   **OpenGL Shadow State Invalidation:** Introduced `_SitGLInvalidateShadowState()` and integrated it into the frame start sequence. This invalidates internal state tracking at the beginning of every frame, allowing the library to recover gracefully if external tools (like ImGui) modify the GL state behind its back.
*   **Vulkan Staging Buffer Cleanup:** Consolidated logic in `_SituationVulkanCreateAndUploadBuffer` to ensure robust cleanup of staging buffers. It now safely handles both synchronous (initialization) and asynchronous (runtime) upload paths, preventing potential double-frees or leaks.
*   **Header Cleanup:** Removed the empty `SITUATION_VERSION_STRING` macro definition to clean up the public header.


---

## [2.3.14 "Velocity" (Stability & Performance)] - 2025-12-01

### Description

This release addresses critical stability issues and performance bottlenecks identified in the Velocity architecture. It introduces key optimizations for both OpenGL and Vulkan backends, fixes a severe heap corruption bug, and adds support for Tangent Space geometry, enabling advanced PBR rendering.


### Critical Fixes

*   **Heap Corruption Fix:** Removed invalid pointer poisoning logic in `SituationFreeString` that caused undefined behavior and heap corruption.
*   **Audio Thread Safety:** Implemented `try_lock` logic in the audio callback to prevent the high-priority audio thread from stalling if the main thread hangs during asset loading.
*   **Vulkan Buffer Race Condition:** Fixed a race condition in `SituationUpdateBuffer` by forcing the use of staging buffers for all updates within the render loop, ensuring correct synchronization.
*   **Vulkan PBR Regression Fix:** Resolved a blocking regression where new 48-byte stride PBR pipelines broke compatibility with legacy 32-byte meshes. The Vulkan backend now dynamically selects between Legacy and PBR pipelines based on mesh vertex stride.


### Performance Optimizations

*   **OpenGL Shadow State:** Implemented software tracking of GL state (program, VAO, FBO, blend modes) to eliminate redundant `glGetIntegerv` calls from the hot render loop. This removes significant CPU-GPU synchronization bubbles.
*   **Vulkan Asset Descriptor Pool:** Introduced a dedicated, freeable `VkDescriptorPool` for long-lived assets (Textures/Models). This prevents descriptor exhaustion and fragmentation during level transitions, which was a risk with the previous linear-only allocator.
*   **Text Rendering Allocations:** Replaced per-frame `malloc/free` calls in `SituationCmdDrawText` with a persistent, auto-growing scratch buffer (`text_batch_scratch`), significantly reducing heap allocator pressure during UI rendering.


### New Features

*   **Tangent Space Support:** Updated `SituationCreateMesh` and the internal GLTF loader to extract and store Tangent data (12-float stride: Pos, Norm, Tangent, UV). This enables correct normal mapping for PBR shaders.


### Migration Guide

*   **Shader Contract Update:** The vertex input layout has changed to support Tangents. Custom shaders using `SituationCmdDrawMesh` must update their input layout:
*   **Location 0:** Position (vec3)
*   **Location 1:** Normal (vec3)
*   **Location 2:** TexCoord0 (vec2)
*   **Location 3:** Color (vec4) - *Reserved/Legacy*
*   **Location 4:** Tangent (vec4) - **[NEW]**


---

## [2.3.13 "Velocity" (Async Threading Module)] - 2025-11-30

### Description

This release introduces the **Async Threading Module**, a C11-compliant job system designed to eliminate main-thread stalls caused by heavy operations like audio decoding and file I/O. It provides a high-performance, lock-minimized ring buffer for job submission and worker management, paving the way for the upcoming v2.4 "Momentum" engine architecture.


### New Features

*   **SituationThreadPool:** A robust, user-managed thread pool implementation using C11 primitives (`<threads.h>`, `<stdatomic.h>`). Features a fixed-size ring buffer (default 256 slots) for zero-allocation job submission at runtime.
*   **Async Audio Loading:** Added `SituationLoadSoundFromFileAsync`, allowing audio files to be decoded to RAM in the background without blocking the rendering loop.
*   **Job System API:**
*   `SituationCreateThreadPool`: Auto-detects logical cores to spawn an optimal number of worker threads.
*   `SituationSubmitJob`: Pushes generic work units to the background workers.
*   `SituationWaitForJob` / `SituationWaitForAllJobs`: Provides flexible synchronization options, using condition variables to sleep efficiently (zero CPU usage) while waiting.
*   `SituationDestroyThreadPool`: Signals shutdown, wakes all workers, drains the pending queue, and joins threads for a clean exit.
*   **Safety Mechanisms:**
*   **Main Thread Assertions:** New `SIT_ASSERT_MAIN_THREAD()` macro ensures thread-sensitive APIs (OpenGL, Windowing) are never called from worker threads.
*   **Atomic State Tracking:** Job completion status and active worker counts are managed atomically to prevent race conditions.


### Architectural Changes

*   **Worker Logic:** Implemented a robust worker loop that handles spurious wakeups and ensures the `active_jobs` counter is decremented only *after* job execution is fully complete, preventing race conditions in `WaitForAll`.
*   **Error Handling:** Added threading-specific error codes (`SITUATION_ERROR_THREAD_QUEUE_FULL`, `SITUATION_ERROR_THREAD_VIOLATION`) to the core error enum.


### Validation

*   **Sanitizer Clean:** Passed 1k-job stress tests under ThreadSanitizer (TSan) and Helgrind with zero data races or deadlocks.


---

## [2.3.12A "Velocity" (critical fixes)] - 2025-11-30

### Description

This release was strictly critical fixes done to help compile the library.


### Critical Fixes

*   **Audio Hardening:** Fixes to SituationLoadSoundFromStream, SituationSetSoundPitch, SituationSetSoundFilter fixes to signatures when calling miniaudio.
*   **Input Hardening:** Fixes to SituationSetMousePosition, SituationSetMouseOffset, SituationSetMouseScale rewrite of the functions for accuracy.


---

## [2.3.12 "Velocity" (Input Subsystem Refactor)] - 2025-11-30

### Description

This release executes a major architectural refactor of the Input Subsystem. All Human Interface Device (HID) state—Keyboard, Mouse, Joysticks, and Cursors—has been decoupled from the monolithic global state container and moved into a dedicated `_SituationInputState` structure within the main context.


### Architectural Changes

*   **Input State Isolation:** Introduced `_SituationInputState` to encapsulate all input-related data structures. This cleanly separates input logic from windowing and rendering state.
*   **Context Expansion:** Updated `SituationContext` to include the new `input` container. Added the `sit_input` macro for internal access.
*   **Thread Safety Prep:** This refactor is the foundational prerequisite for the upcoming "Double-Buffered Input" system, which will allow game logic and rendering to run on separate threads without locking or race conditions.


---

## [2.3.11 "Velocity" (Vulkan Stability & Errno Fixes)] - 2025-11-30

### Description

This release addresses critical stability issues in the Vulkan backend regarding descriptor set allocation and resource cleanup. It also removes invalid errno checks in memory management functions to prevent false error reporting.


### Critical Fixes

*   **Vulkan Descriptor Hardening:** `_SituationVulkanAllocateDescriptorSet` now strictly checks for `VK_ERROR_OUT_OF_POOL_MEMORY` or `VK_ERROR_FRAGMENTED_POOL` before attempting to grow the pool. Other errors fail fast to prevent infinite loops.
*   **Zombie Resource Prevention:** Hardened `SituationCreateBuffer` and `SituationCreateTexture` to safely defer destruction of underlying Vulkan resources (Buffers/Images) if descriptor set allocation fails, preventing VRAM leaks and GPU stalls.
*   **Errno Safety:** Removed invalid `if (errno != 0)` checks after `SIT_FREE` calls in `SituationUnloadImage` and `SituationFreeDisplays`, which could lead to false positive error reports.
*   **Cosmetic:** Added braces to single-line statements in `SituationUnloadFont`.


---

## [2.3.10C "Velocity" (Error Reporting Refactor)] - 2025-11-30

### Description

This update completes the overhaul of the error reporting system, ensuring that every failure case reports a specific, granular `SituationError` code rather than a generic failure. This allows for precise programmatic handling of errors across all subsystems (Filesystem, Audio, Display, Graphics).


### Critical Fixes

*   **Filesystem Error Fidelity:** `_SituationSetFilesystemError` now accepts a `SituationError` code argument. This allows filesystem operations (load, save, list) to report specific errors like `SITUATION_ERROR_FILE_NOT_FOUND` or `SITUATION_ERROR_ACCESS_DENIED` while still preserving the OS-specific error string (strerror/FormatMessage) for logging.
*   **Audio Decoder Reporting:** Fixed `SituationLoadSoundFromStream`. Previously, if the decoder initialization failed, it returned a generic context error. It now explicitly returns `SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED`.
*   **Global Error Refactor:** Replaced all remaining instances of generic error codes (like `-1` or `0`) with specific `SituationError` enums in:
*   **Audio:** `SituationSetAudioDevice`, `SituationStartAudioCapture`.
*   **Display:** `SituationSetDisplayMode`.
*   **Graphics:** `SituationCreateTexture`, `SituationCreateShader`, `SituationCreateComputePipeline`.
*   **Filesystem:** `SituationLoadFileData`, `SituationSaveFileData`, `SituationListDirectoryFiles`.


---

## [2.3.10B "Velocity" (Feature Parity & Error Reporting)] - 2025-11-29

### Description

This release significantly enhances the robustness of the library by expanding error reporting and ensuring correct feature management across both OpenGL and Vulkan backends. It addresses critical gaps in error code handling and implements proper feature detection and enablement for Vulkan extensions.


### Critical Fixes & Safety

*   **Comprehensive Error Support:** Added a full block of `NETWORK` error codes (`-900` to `-907`) and updated `_SituationSetErrorFromCode` to include specific case handlers for *every* `SituationError` defined in the enum. This eliminates generic "Unknown Error" messages for defined failure states.
*   **Renaming:** Renamed `SituationFeature` to `SituationRenderFeature` to better reflect its scope and purpose within the graphics subsystem.
*   **Vulkan Feature Management:**
*   Fixed a critical issue in `_SituationVulkanCreateLogicalDevice` where optional features (Mesh Shaders, Ray Tracing) were not being correctly enabled.
*   Implemented robust `pNext` chaining logic to properly link `VkPhysicalDeviceMeshShaderFeaturesEXT`, `VkPhysicalDeviceRayTracingPipelineFeaturesKHR`, and other feature structs to the `VkDeviceCreateInfo` chain. This ensures that requested features are actually activated on the logical device.
*   **OpenGL Feature Detection:** Updated `_SituationInitOpenGL` to correctly populate the `enabled_features_mask` based on available GLAD extensions and core version capabilities, ensuring `SituationIsFeatureSupported` returns accurate results.


---

## [2.3.10A "Velocity" (Stability Fixes & Optimization)] - 2025-11-29

### Description

This is a "surgical fix" release targeting stability, memory safety, and text rendering performance. It introduces configurable memory allocators, C++ RAII wrappers for strings, and a significant optimization for text rendering that replaces character-by-character draw calls with batched rendering.


### Critical Fixes & Safety

*   **Memory Safety Macros:** Replaced all internal calls to `malloc`, `calloc`, `realloc` with overridable macros `SIT_MALLOC`, `SIT_CALLOC`, `SIT_REALLOC`. This allows users to integrate custom allocators (e.g., for tracking or pools) by defining these macros before including the header.
*   **RAII String Wrapper (C++):** Added `SituationScopedString` struct for C++ users. This RAII wrapper automatically calls `SituationFreeString()` when it goes out of scope, preventing memory leaks from API functions that return heap-allocated strings (like `SituationGetLastErrorMsg`).


### Optimizations

*   **Batched Text Rendering:** Completely rewrote `SituationCmdDrawText`.
*   **Old Behavior:** Issued one draw call per character, causing massive driver overhead.
*   **New Behavior:** Batches all characters into a single dynamic vertex buffer and issues one draw call per string.
*   **Backend Support:** Implemented efficient dynamic buffer updates for both OpenGL (`glNamedBufferSubData`) and Vulkan (Staging Buffer + Pipeline Barrier).


---

## [2.3.10 "Velocity" (Feature Flag System & API Refinement)] - 2025-11-29

### Description

This release introduces a comprehensive Feature Flag system to `situation.h`, enabling applications to query granular GPU capabilities at runtime. It also resolves critical compilation issues in the OpenGL backend related to extension macros and duplicate definitions.


### New Features

*   **Feature Flag System:** Introduced the `SituationFeature` enum and `SituationIsFeatureSupported()` function.
*   Allows querying support for advanced features like `SIT_FEATURE_BINDLESS_BUFFERS`, `SIT_FEATURE_MESH_SHADER`, `SIT_FEATURE_RAY_TRACING`, and more.
*   Automatically populated during backend initialization based on available extensions and driver limits.


### Critical Fixes

*   **Compilation Fixes:**
*   Corrected the definition order of `SITAPI` to resolve "expected ‘;’ before ‘void’" errors.
*   Removed duplicate function definitions (`SituationGetBufferDeviceAddress`, `SituationGetTextureHandle`) that caused redefinition errors.
*   Added proper `#ifdef` guards around OpenGL extension macros (`GLAD_GL_NV_shader_buffer_load`, etc.) to prevent compile-time failures when extensions are missing from the loader.
*   **Missing Definitions:** Verified presence of `_SituationCachePhysicalDisplays` and `_SituationGLFWJoystickCallback` to resolve linker warnings.


### Documentation

*   **Versioning:** Updated all version macros and documentation to 2.3.10.


---

## [2.3.9 "Velocity" (Vulkan 1.2 & Buffer Device Address)] - 2025-11-29

### Description

This release marks a critical update to the Situation SDK, bumping the minimum Vulkan requirement to version 1.2. This change enables access to advanced features like Bindless Descriptors and Buffer Device Address, paving the way for high-performance GPU-driven rendering architectures.


### Api Additions

*   **Bindless Graphics & Compute:**
*   `SituationGetBufferDeviceAddress`: Retrieves the 64-bit physical GPU address of a buffer, enabling direct pointer access in shaders via `GL_EXT_buffer_reference`.
*   `SituationGetTextureHandle`: Retrieves a 64-bit handle for bindless texture access (OpenGL only).
*   `SituationCmdBindSampledTexture`: Binds a texture specifically for sampling (sampler2D) operations, distinct from storage image bindings.

*   **Compute Workflow Enhancements:**
*   `SituationCmdPresent`: Introduced a command to manually present a texture to the swapchain. This is essential for "Compute-Only" pipelines where the final image is generated by a compute shader rather than a rasterization pass.
*   `SIT_COMPUTE_LAYOUT_BUFFER_IMAGE`: Added a new compute layout configuration supporting one SSBO and one Storage Image, optimizing binding for common post-processing shaders.

*   **Image Module Utilities:**
*   `SituationCreateImage`: Added helper to allocate an uninitialized CPU-side image buffer.
*   `SituationBlitRawDataToImage`: Efficiently copies raw byte arrays into an image region (useful for font atlas generation).
*   `SituationSetPixelColor`: CPU-side helper for setting individual pixels.


### Technical Changes

*   **Vulkan 1.2 Mandate:**
*   The internal Vulkan initialization sequence now explicitly requests API version 1.2.
*   `bufferDeviceAddress` feature is enabled during logical device creation if supported by the GPU.
*   VMA (Vulkan Memory Allocator) configuration updated to target Vulkan 1.2.

*   **Internal Refactoring:**
*   Updated `_SituationVulkanState` and initialization logic to support the expanded compute layout array.
*   Standardized `SituationCmdPresent` implementation across OpenGL (using `glBlitNamedFramebuffer`) and Vulkan (using `vkCmdBlitImage` and barrier transitions).


---

## [2.3.8B "Velocity" (Production Readiness)] - 2025-11-29

### Description

This release is an affirming hardening production readiness release. It aims to polish and wrap up the progressive work done in the "Velocity" saga from codebase to the SDK documentation.


### Improvements

*   **Documentation Polish:** Comprehensive review and update of the SDK documentation (`situation_sdk_238.md`) to reflect the finalized state of the Velocity module.
*   **Version Synchronization:** Aligned version numbers across all documentation and header files to 2.3.8B.


---

## [2.3.8A "Velocity" (Hotfix A)] - 2025-11-29

### Description

This release achieves production-readiness for the "Velocity" hot-reloading module. It introduces a robust "Fail-Safe" reloading architecture that prevents application crashes or visual corruption when reloading assets with errors.


### New Features

*   **Fail-Safe Hot-Reloading:**
*   **Shaders:** Refactored `SituationReloadShader` and `SituationReloadComputePipeline` to use a "Load-Swap-Destroy" pattern. New shaders are compiled and verified *before* the old ones are destroyed. If compilation fails, the old shader remains active, and an error is logged. This prevents "black screen" states during shader development.
*   **Textures:** Applied fail-safe logic to `SituationReloadTexture`. Invalid image files or load errors no longer invalidate the existing texture handle.
*   **Models:** Implemented deep-swap reloading for `SituationReloadModel`. The entire model hierarchy (meshes and textures) is rebuilt in the background and swapped atomically on success.
*   **Stability:** The hot-reload loop (`SituationCheckHotReloads`) is now resilient to list mutation and ensures safe iteration even if resources are added or removed during the reload process.


### Critical Fixes

*   **Internal robustness:** Corrected resource tracking node management during reloads to prevent memory leaks and ensure persistent tracking of reloaded assets.


---

## [2.3.8 "Velocity" (Hot-Reload Implementation & API Fixes)] - 2025-11-29

### Description

This release finalizes the "Velocity" feature set by implementing the Hot-Reloading module and addressing critical API inconsistencies. It introduces the `SituationCheckHotReloads` function, enabling runtime asset reloading, and corrects strict typing issues in the Input and Graphics modules.


### New Features

*   **Hot-Reloading Implementation:** Fully implemented `SituationCheckHotReloads` and resource tracking.
*   **Resource Tracking:** Internal resource nodes (`_SituationShaderNode`, `_SituationTextureNode`, `_SituationModelNode`, `_SituationComputePipelineNode`) now store source file paths and last modification times.
*   **Loader Integration:** `SituationLoadShader`, `SituationLoadTexture`, `SituationLoadModel`, and `SituationCreateComputePipeline` now capture file modification times upon successful load.
*   **Polling:** `SituationCheckHotReloads` (intended for development builds) polls these files for changes and triggers the appropriate `SituationReload*` function.


### Critical Fixes

*   **Input API Strict Typing:** Corrected function signatures in `situation.h` for `SituationSetMousePosition`, `SituationSetMouseOffset`, and `SituationSetMouseScale`. They now correctly accept `Vector2` structs instead of `vec2` arrays, matching their implementations and ensuring Strict C11 compliance.
*   **Vulkan/OpenGL State Consistency:** Moved `last_vd_composite_time_ms` from the backend-specific structs to the common `_SituationGlobalStateContainer`. This fixes a bug where profiling data was inaccessible or reading from uninitialized memory depending on the active backend.
*   **OpenGL Error Macro Visibility:** Added a forward declaration for `_SituationLogGLError` in the public section of `situation.h`. This ensures the `SIT_CHECK_GL_ERROR` macro compiles correctly in user applications when `SITUATION_USE_OPENGL` is defined.
*   **Memory Leaks Plugged:** Fixed memory leaks in `SituationDestroyTexture`, `SituationUnloadShader`, `SituationUnloadModel`, and `SituationDestroyComputePipeline` where the path strings (`source_path`, `vs_path`, `fs_path`) tracked for hot-reloading were not being freed during destruction.
*   **Hot-Reload Loop Prevention:** Updated `SituationCheckHotReloads` to update the file modification timestamp *before* attempting a reload. This prevents infinite retry loops and GPU stalls if a file has syntax errors (avoiding a "reload -> fail -> retry next frame" cycle).


---

## [2.3.7C "Velocity" (Graveyard & Reverb Documentation)] - 2025-11-29

### Description

This update focuses on documentation completeness and internal clarity. It retroactively documents the "Graveyard" deferred destruction system introduced in previous versions and finalizes the description of the embedded reverb implementation.


### Documentation

*   **Vulkan Graveyard:** Added comprehensive internal documentation for `SituationGraveyard` and its associated helper functions (`_SituationInitGraveyard`, `_SituationFlushGraveyard`). This clarifies how the library prevents GPU stalls during resource destruction.
*   **Reverb Internals:** Documented the Schroeder/Freeverb implementation details, including the structure of the `SituationReverbState` and the logic behind the parallel comb filters and all-pass filters.


---

## [2.3.7B "Velocity" (Embedded Reverb Implementation)] - 2025-11-28

### Description

This patch implements a custom Schroeder/Freeverb reverberation algorithm directly within the `situation.h` header, replacing the missing `miniaudio` reverb dependency. This ensures that the `SituationSetSoundReverb` function is fully operational and self-contained, providing high-quality environmental audio effects without requiring external DSP libraries.


### New Features

*   **Embedded Reverb Algorithm:** Implemented a complete Schroeder/Freeverb reverb engine (8 comb filters, 4 all-pass filters) within the library's implementation block. This restores full functionality to the `SituationSetSoundReverb` API, enabling Room Size, Damping, and Wet/Dry mix controls.
*   **Opaque State Management:** The public `SituationSound` struct now uses an opaque `void* reverb_state` pointer, completely hiding the internal reverb data structures (`SituationReverbState`, `SituationReverbComb`, etc.) from the public API. This improves encapsulation and prevents ABI breakage if the reverb implementation changes in the future.


### Critical Fixes

*   **Missing Dependency Resolution:** Replaced calls to non-existent `ma_reverb` functions with internal helpers (`_SituationInitReverb`, `_SituationProcessReverb`, `_SituationUninitReverb`). The Audio Engine now initializes and processes the custom reverb chain seamlessly as part of the standard audio pipeline.
*   **Initialization Safety:** Added robust null checks for memory allocation in `_SituationInitReverb` to prevent potential crashes during sound loading or effect initialization.
*   **API Cleanliness:** Removed redundant macro definitions (`SIT_REVERB_COMB_COUNT`, etc.) from the public header section, keeping the global namespace clean.


---

## [2.3.7A "Velocity" (Audio Safety Hotfix)] - 2025-11-28

### Description

This hotfix addresses critical stability regressions introduced in the 2.3.7 Audio Architecture Refactor. It focuses on ensuring that the Audio API is robust against invalid usage and that the core initialization check is correctly exported for external use.


### Critical Fixes

*   **Audio API Safety Guards:** Implemented explicit `SituationIsInitialized()` checks at the entry point of every public Audio API function. This prevents the application from crashing with a null pointer dereference if audio functions (like `SituationPlayLoadedSound` or `SituationSetAudioDevice`) are called before `SituationInit()` or after `SituationShutdown()`. Instead of crashing, these functions now safely return an error code.
*   **SITAPI Export Fix:** The implementation of `SituationIsInitialized()` was missing the `SITAPI` macro in its definition. This has been corrected to ensure the function is properly exported in shared library (DLL) builds, matching its forward declaration.
*   **Error Reporting Safety:** Hardened the `_SituationSetError` internal helper to check for a valid context pointer before attempting to write error messages. This prevents a secondary crash when the library attempts to report an "Uninitialized" error.


---

## [2.3.7 "Velocity" (Audio Architecture Refactor)] - 2025-11-26

### Description

This release finalizes the architectural separation of the Audio subsystem from the central global state. It moves all audio-related state into a dedicated `_SituationAudioState` container (`sit_audio`), improving modularity and memory organization. This update also includes critical fixes for audio capture and device management that were identified during the refactor.


### Critical Fixes

*   **Audio State Separation:** Completed the migration of audio state variables (MiniAudio context, device handles, capture queues) from `sit_gs` to the new `sit_audio` static container. This decoupling ensures cleaner subsystem isolation.
*   **Audio Capture Logic:** Fixed a severe logic error in `SituationPollInputEvents` where the audio capture ring buffer's write head was being incorrectly initialized with the read head's value, and where state was being read from the wrong structure. This restores functional audio capture on the main thread.
*   **User Data Pointer Safety:** Corrected `SituationStartAudioCapture` and `SituationSetAudioDevice` to pass the correct `&sit_audio` pointer to MiniAudio callbacks. Previously, they passed `&sit_gs`, which would have caused a crash or memory corruption when the callback cast it to `_SituationAudioState*`.


---

## [2.3.6 "Velocity" (OpenGL Hardening & DSA Optimization)] - 2025-11-25

### Description

This release hardens the OpenGL backend by strictly enforcing OpenGL 4.6 Core Profile at context creation. It also refactors the internal rendering pipeline to utilize Direct State Access (DSA), resulting in significantly faster internal rendering passes by eliminating CPU-GPU pipeline stalls.


### Breaking Changes

*   **macOS Support Dropped:** By enforcing OpenGL 4.6, this library is no longer compatible with macOS (which is capped at OpenGL 4.1). Users on macOS must now use the Vulkan backend (via MoltenVK) or remain on v2.3.5.
*   **Legacy GPU Support Dropped:** Older integrated graphics (pre-Intel Skylake/HD 500 series) that do not support GL_ARB_direct_state_access will now fail to initialize SituationInit.


### Technical Details

*   **Strict Context Creation:** `_SituationInitWindow` now sends strict hints to GLFW to request an OpenGL 4.6 Core Profile context. If the driver cannot provide it, window creation fails immediately.
*   **Elimination of State Query Stalls:** The `_SitGLBackupState` function has been optimized to remove slow `glGetIntegerv` calls that queried texture unit bindings, which are no longer needed with DSA.
*   **DSA Implementation:** `SituationRenderVirtualDisplays` now uses `glBindTextureUnit` for direct texture binding, eliminating the need to modify `glActiveTexture` state.
*   **Immutable Buffer Storage:** `SituationCreateBuffer` now mandates `glNamedBufferStorage` (Immutable Storage) over `glBufferData` (Mutable), providing better optimization hints to the driver.


---

## [2.3.5B "Velocity" (Documentation Overhaul)] - 2025-11-25

### Description

This release is a comprehensive documentation overhaul, bringing the `situation_api.md` programming guide to 100% parity with the `v2.3.5A` "Velocity" header. It addresses the significant documentation debt accrued over multiple hotfix and feature releases, ensuring every public function, struct, and feature is now fully and accurately documented. This makes the library significantly easier to learn, use, and maintain.


### Documentation

*   **Complete API Parity:** Performed a full audit of `situation.h` against `situation_api.md`. Every undocumented function has been added to the guide.
*   **New Modules Documented:**
*   **Hot-Reloading Module:** Added a complete section for the "Velocity" Hot-Reloading feature set (`SituationCheckHotReloads`, `SituationReloadShader`, `SituationReloadTexture`, etc.), explaining its usage and benefits for rapid development.
*   **Audio Capture API:** Added a new "Audio Capture" subsection to the Audio Module, documenting `SituationStartAudioCapture`, `SituationStopAudioCapture`, and related functions.
*   **Signature Corrections & Refinements:**
*   Updated dozens of function signatures and usage examples throughout `situation_api.md` to reflect the strict C11 `struct`-based approach (e.g., changing `vec2` return types to `Vector2`).
*   Corrected parameter lists and descriptions for functions whose behavior had diverged from the old documentation (e.g., `SituationSetSoundReverb`, `SituationGenImageGradient`).
*   Added documentation for recently introduced performance and hardware query functions (`SituationGetVRAMUsage`, `SituationGetDrawCallCount`).
*   **Structural Improvements:** Re-organized sections for better logical flow and readability. Ensured all new entries follow the established documentation format with clear signatures, descriptions, and copy-paste-friendly usage examples.


### Consistency & Aesthetics

*   **Version Sync:** The version number in `situation_api.md` is now correctly updated to `v2.3.5B`.
*   **Formatting:** Ensured consistent Markdown formatting, code blocks, and section headers across the entire document.


---

## [2.3.5A "Velocity" (Pristine)] - 2025-11-25

### Description

This is a documentation and refinement release. Following the major performance overhaul in 2.3.5, this update focuses on making the solutions "absolutely pristine" by adding comprehensive documentation and ensuring the code is clean, consistent, and easy to understand. It verifies that the fixes for the critical performance bottlenecks are robust and clearly explains the "why" behind the architecture.


### Documentation & Refinement

*   **Vulkan Graveyard System:** Added a detailed documentation block explaining the entire deferred deletion ("graveyard") system. It explicitly details how this architecture solves the `vkDeviceWaitIdle` abuse problem by queuing resources for deletion instead of stalling the GPU.
*   **Asynchronous Uploads:** Added a comprehensive header to the `_SituationVulkanCreateAndUploadBuffer` function. It now clearly documents the dual-path (asynchronous/synchronous) mechanism and explains how the asynchronous path eliminates CPU-GPU stalls during in-game asset streaming.
*   **Linear Descriptor Allocation:** Verified that `vkFreeDescriptorSets` is not used in the hot path. Added documentation to the `_SituationVulkanAllocateDescriptorSet` function explaining how the "Dynamic Descriptor Manager" acts as a high-performance, auto-growing linear allocator, solving the descriptor pool fragmentation issue.
*   **API Consistency:** Refined internal function signatures for the deferred deletion system for better consistency.


---

## [2.3.5 "Velocity"] - 2025-11-25

### Description

This release focuses on critical performance optimization for the Vulkan backend. It eliminates severe CPU-side stalls during resource management and data transfer, transforming the engine's streaming capabilities. It also implements a "Linear Allocator" strategy for descriptor pools to prevent fragmentation and reduce allocation overhead.


### Critical Performance Fixes

*   **Deferred Resource Destruction:** Implemented a `SituationGraveyard` system. Resources (Buffers, Images, Pipelines, Descriptor Sets) destroyed during a frame are no longer deleted immediately (which required a stalling `vkDeviceWaitIdle`). Instead, they are queued and safely destroyed only after the frame that used them has completed execution on the GPU. This eliminates the massive frame spikes previously seen during asset unloading.
*   **Asynchronous Buffer Uploads:** Refactored `_SituationVulkanCreateAndUploadBuffer`. It now uses the main command buffer to perform data transfers when inside a frame, inserting pipeline barriers for synchronization. This replaces the previous synchronous "allocate-record-submit-wait" cycle for every single buffer creation, significantly speeding up asset loading during gameplay.
*   **Linear Descriptor Allocation:** Modified the `_SituationFlushGraveyard` logic to skip individual `vkFreeDescriptorSets` calls. By treating descriptor pools as append-only and resetting/destroying them only when full or at shutdown, we eliminate memory fragmentation and the high CPU cost of freeing sets individually.


---

## [2.3.4M "Velocity" (Hotfix M)] - 2025-11-24

### Description

This release focuses on "surgical precision" in error handling and reporting. It ensures that every error code defined in the library is correctly mapped to a human-readable string, eliminating "Unknown Error" responses for defined failures. It also hardens the Hot-Reloading module against race conditions and resource invalidation.


### Critical Fixes

*   **Exhaustive Error Mapping:** Updated `_SituationSetErrorFromCode` to include `case` statements for *every* `SITUATION_ERROR_*` constant defined in the enum. This guarantees that all internal failures report specific, actionable error messages instead of falling back to generic codes.
*   **Hot-Reload Safety:** Enhanced `SituationReloadShader`, `SituationReloadTexture`, `SituationReloadModel`, and `SituationReloadComputePipeline` to explicitly check for GPU synchronization failures (`vkDeviceWaitIdle`). If the GPU cannot be idled (e.g., device lost), the reload operation now safely aborts with `SITUATION_ERROR_HOTRELOAD_GPU_SYNC_FAILED` instead of risking a crash or undefined behavior.
*   **Precise Error Reporting:**
*   `_SituationInitOpenGL` now returns `SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION` (instead of generic unsupported) when the GL version check fails.
*   Vulkan internal resource creation (Depth Resources) now specifically reports `SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED` on allocation failure.
*   Hot-reload functions now return `SITUATION_ERROR_RESOURCE_INVALID` if the source file path was not correctly tracked, aiding in debugging.


---

## [2.3.4L "Velocity" (Hotfix L)] - 2025-11-24

### Description

This release brings the SDK documentation and codebase into perfect synchronization regarding error handling. It resolves long-standing discrepancies in error code values and introduces granular, actionable error reporting for the Vulkan backend. Additionally, it addresses critical header dependency issues to ensure robust compilation in strict C environments.


### Critical Fixes

*   **Header Compilation & Dependency Ordering:** Solved a compilation order dependency in `situation.h`. Moved `SituationError` and `SituationBufferUsageFlags` typedefs to the top of the header. This ensures that these types are defined before they are used in function prototypes or macros (like `SITUATION_LOG_WARNING`), preventing "unknown type" errors in single-pass C compilers.
*   **Vulkan Descriptor Logic:** Fixed a memory leak in `_SituationVulkanAllocateDescriptorSet`. Previously, if `realloc` failed, the original pointer was lost. The logic now safely handles reallocation failures.


### Error Handling & Documentation

*   **Error Code Synchronization:** Performed a comprehensive audit of `situation.h` and `situation_sdk_234.md`.
*   Verified `SITUATION_ERROR_UNKNOWN_ERROR` is explicitly `-999`.
*   Updated documentation to match implementation values for `SITUATION_ERROR_INVALID_ENUM` (-4), `SITUATION_ERROR_ALREADY_INITIALIZED` (-310), and filesystem errors (-550, -551).
*   **Granular Vulkan Errors:** Implemented specific, high-value error codes for the Vulkan backend to aid in debugging resource exhaustion:
*   `_SituationVulkanAllocateDescriptorSet` now returns `SITUATION_ERROR_VULKAN_DESCRIPTOR_POOL_EXHAUSTED` (-749) instead of a generic error.
*   `_SituationVulkanCreateImage` and `_SituationVulkanCreateAndUploadBuffer` now return `SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED` (-750) on memory failures.


---

## [2.3.4K "Velocity" (Hotfix K)] - 2025-11-24

### Description

This micro-hotfix delivers a single-line safeguard for OpenGL backend portability, ensuring seamless compilation in header-only environments without external GL headers.


### Critical Fixes

*   **GLFW Include Guard:** Added `#define GLFW_INCLUDE_NONE` immediately before `<GLFW/glfw3.h>` inclusion to prevent GLFW from auto-including system `GL/gl.h` (or GL ES equivalents). This resolves "missing header" compilation failures in minimal setups (e.g., cross-compiles, embedded toolchains, or when bundling with GLAD). The change is non-intrusive, with zero runtime impact, and maintains full compatibility with existing builds.


### Documentation

*   **Inline Comment:** Accompanied the define with a precise, self-explanatory comment detailing the rationale, environments affected, and synergy with GLAD loader. This enhances developer onboarding without bloating the header.


---

## [2.3.4J "Velocity" (Hotfix J)] - 2025-11-24

### Description

This release delivers critical stability fixes for the audio subsystem and resource management, specifically targeting initialization logic, 64-bit system compatibility, and memory safety during asset loading.


### Critical Fixes

*   **Audio Initialization Logic:** Resolved a critical logic error in `_SituationInitSubsystems` where the audio capture initialization block was unreachable due to incorrect nesting within an error check. The `SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD` flag now correctly initializes the capture ring buffer.
*   **64-bit Pointer Safety:** Fixed a truncation bug where Vulkan and OpenGL resource handles (which are pointers) were being cast to `uint32_t` before being assigned to `uint64_t` IDs. This caused invalid handles on 64-bit systems. Handles are now correctly cast to `uintptr_t` first.
*   **Model Loading Safety:** Added robust `NULL` checks for `calloc` memory allocations in `SituationLoadModel`.
*   **Resource Cleanup on Failure:** Implemented proper cleanup logic in `SituationLoadModel`. If mesh allocation fails after textures have been loaded, the function now correctly destroys the loaded textures before returning, preventing resource leaks.
*   **API Consistency:** Updated `_SituationInitSubsystems` to accept `const SituationInitInfo*` to match the initialization flow.


---

## [2.3.4I "Velocity" (Hotfix I)] - 2025-11-23

### Description

This release, designated "Surgical Fixes," focuses on resolving specific defects identified in the codebase, ranging from strict C11 compliance issues to logic bugs in resource creation and input handling.


### Critical Fixes

*   **Resource Initialization:** Fixed a critical bug in `SituationCreateBuffer` where `buffer.usage_flags` was not being assigned, potentially leading to undefined behavior or validation errors in backend resource creation.
*   **Pointer Safety:** Corrected a pointer access error in `SituationDestroyTexture` (`texture.id` -> `texture->id`), preventing compilation errors and potential crashes during cleanup.
*   **Clipboard Logic:** Fixed `SituationSetClipboardText` to correctly pass the `text` argument to the underlying GLFW function, restoring clipboard functionality.


### Api Compliance & Refactoring

*   **C11 Compliance (Input Module):** Refactored mouse input functions (`SituationGetMousePosition`, `SituationGetMouseDelta`, `SituationGetMouseWheelMoveV`) to return `Vector2` structs instead of `vec2` arrays. This resolves a strict C11 compliance violation regarding returning arrays from functions.
*   **Implementation Correctness:** Updated the implementation of the above mouse functions to correctly cast `Vector2*` to `float*` (or `vec2`) when interfacing with the `cglm` math library, ensuring correct data layout and processing.


---

## [2.3.4H "Velocity" (Hotfix H)] - 2025-11-23

### Description

The "Titanium Robustness" release.
This hotfix completes the error handling overhaul, eliminating every silent failure, double-free risk, and vague diagnostic in the codebase. Every allocation now pairs perfectly with SIT_FREE, and every failure path now returns a precise, actionable SituationError code. The library is now truly unbreakable — no more "it just didn't work" mysteries.


### Critical Fixes

*   **Universal SIT_FREE Adoption:** Replaced every instance of `free()` with `SIT_FREE()` throughout the implementation. This ensures full allocator override compatibility (e.g., debug trackers, memory pools) without breaking any existing code.
*   **Exhaustive Error Path Coverage:** Audited and fixed 12+ silent failure spots across core functions (`SituationLoadImageFromScreen`, `SituationTakeScreenshot`, `SituationUnloadImage`, etc.):
*   Added specific error codes for backend validations (e.g., GLAD loader fail → `SITUATION_ERROR_OPENGL_LOADER_FAILED`).
*   Proactive checks for invalid states (e.g., zero dimensions in screenshot → `SITUATION_ERROR_NOT_INITIALIZED` with context).
*   Defensive free checks (e.g., post-SIT_FREE errno validation in `SituationUnloadImage` and `SituationFreeDisplays`).
*   **Debug Double-Free Detection:** Enhanced `SituationFreeString` with poison-pointer tracking (debug-only) to catch use-after-free bugs immediately, preventing heap corruption.
*   **Void Function Warnings:** Introduced `SITUATION_LOG_WARNING` macro for non-fatal issues in void APIs (e.g., null params in unloads), with debug-only stderr output for zero-overhead diagnostics.


### Documentation

*   **Error Enum Finalization:** Fully documented the expanded `SituationError` enum with every original code preserved, merged duplicates intelligently, and filled gaps for complete range utilization. Every code now has precise EOL comments explaining triggers and platform specifics.
*   **API Safety Notes:** Updated function docs for new return types (e.g., `SituationUnloadImage` now bool for failure propagation) and emphasized error querying via `SituationGetLastError()`.


### Consistency & Aesthetics

*   **Error Uniformity:** All error sets now use `_SituationSetErrorFromCode` with specific enums and contextual messages (e.g., "%dx%d RGBA alloc failed" for screenshots). No more generic fallbacks.
*   **Memory Hygiene:** SIT_FREE macro now consistently nulls pointers post-free (updated definition: `#define SIT_FREE(p) do { if (p) { free(p); (p) = NULL; } } while(0)`).
*   **Debug Polish:** All new checks guarded by `#ifndef NDEBUG` for release-zero overhead; warnings follow the exact same tone and format as other logs.


---

## [2.3.4G "Velocity" (Hotfix G)] - 2025-11-23

### Description

The "Polish" release.


### Documentation

*   Every single struct in the public and internal API now carries full comments with logical grouping, separator lines, and deep explanatory notes.
*   The entire callback section has been rewritten for consistency, real-time safety warnings, exact format guarantees.
*   The API Usage Guide section has been replaced with a more complete version.
*   All enum blocks (SituationDataType, SituationBufferUsageFlags, barrier flags, etc.) received full professional commentary, useful combination presets, and performance guidance.
*   Custom DSP and audio capture callbacks finalised with correct float* buffers and complete real-time safety documentation.


### Consistency & Aesthetics

*   Universal adoption of snake_case: every callback parameter is now user_data (including the MiniAudio stream callbacks).
*   SituationFocusCallback parameter renamed from focused → gained_focus for instant, unambiguous clarity.
*   All comment blocks now follow the exact same visual structure, density, and tone.
*   Minor spacing, alignment, and comment style harmonisation across the entire file.


---

## [2.3.4F "Velocity" (Hotfix F)] - 2025-11-22

### Description

This update focuses purely on code hygiene and developer reference. It finalizes the internal state architecture by applying professional, line-by-line documentation to the global state container and its subsystems. It also adds comprehensive API documentation for several core modules.


### Internal Refactoring

*   **State Structure Finalization:** Completed the cleanup of `_SituationGlobalStateContainer`.
*   Moved all subsystem struct definitions (`_SituationKeyboardState`, `_SituationVulkanState`, etc.) outside the main container.
*   Applied "Professional Grade" formatting: Grouped fields by logical category and added explicit End-of-Line (EOL) comments for every single variable in the global state.


### Documentation

*   **API Reference:** Added detailed Doxygen-style headers for the following functions:
*   **Display:** `SituationSetDisplayMode`, `SituationRefreshDisplays`, `_SituationGetCurrentDisplayIdentifier`, `_SituationCachePhysicalDisplays`.
*   **Filesystem:** `SituationGetUserDirectory`, `SituationLoadDroppedFiles`, `SituationUnloadDroppedFiles`.
*   **Input:** `SituationIsFileDropped`, `SituationSetFileDropCallback`, `SituationSetClipboardText`, `SituationGetClipboardText`.
*   **Lifecycle:** `SituationIsInitialized`, `_SituationCleanupDanglingResources`.
*   **Rendering:** `SituationCmdBeginRenderPass`, `SituationCmdEndRenderPass`.


---

## [2.3.4E "Velocity" (Hotfix E)] - 2025-11-22

### Description

This update completes the architectural refactoring started in Hotfix D, solidifying the internal state management and fixing critical initialization bugs. It also standardizes the documentation for core APIs, making the library easier to maintain and integrate.


### Critical Fixes

*   **Input Subsystem Initialization:** Fixed a severe regression in `_SituationInitSubsystems` where `memset` was called *after* mutex initialization, corrupting the keyboard event lock and causing potential deadlocks. The memory zeroing now correctly happens before resource allocation.

*   **Render Pass Logic:** Replaced placeholder pseudo-code in `SituationCmdBeginRenderPass` with a fully functional implementation.
*   **OpenGL:** Now correctly handles Virtual Display targets and `glClearColor` type casting.
*   **Vulkan:** Now correctly delegates standard clear operations and safely rejects unsupported `LOAD_OP_LOAD` requests instead of crashing.


### Documentation

*   **API Reference:** Added comprehensive Doxygen-style documentation for:
*   Display Management (`SituationSetDisplayMode`, `SituationRefreshDisplays`)
*   Filesystem (`SituationGetUserDirectory`, `SituationLoadDroppedFiles`)
*   Input & Clipboard (`SituationIsFileDropped`, `SituationGetClipboardText`)
*   Core Lifecycle (`SituationIsInitialized`, `_SituationCleanupDanglingResources`)
*   Render Pass Control (`SituationCmdBeginRenderPass`, `SituationCmdEndRenderPass`)


---

## [2.3.4D "Velocity" (Hotfix D)] - 2025-11-22

### Description

This update focuses on code hygiene and architectural clarity. It refactors the internal global state container, breaking it down into distinct, self-documenting structures for each subsystem (Input, Audio, Backend). This change improves readability and maintainability without altering the public API or runtime behavior.


### Internal Refactoring

*   **State Container Modernization:**
*   Decomposed `_SituationGlobalStateContainer` into logical sub-structures:
*   `_SituationKeyboardState`: Encapsulates key arrays, ring buffers, and the event mutex.
*   `_SituationMouseState`: Encapsulates position, buttons, and scrolling data.
*   `_SituationJoystickManager`: Encapsulates controller states and connection events.
*   `_SituationGLState` / `_SituationVulkanState`: Segregated backend-specific resources.
*   This grouping makes the global state definition significantly easier to parse and manage.

*   **Naming Consistency:**
*   Moved the keyboard event mutex inside the keyboard state struct (`sit_gs.keyboard.event_queue_mutex`) to match the pattern used by the mouse and joystick subsystems.
*   Standardized initialization order in `_SituationInitSubsystems` to prevent mutex corruption during state zeroing.

*   **Documentation:** Added comprehensive Doxygen headers to all new internal structures to explain their specific roles in the engine lifecycle.


---

## [2.3.4C "Velocity" (Hotfix C)] - 2025-11-22

### Description

This update finalizes the strict C11 compliance overhaul and refactors the internal state architecture for better consistency between backends. It resolves several compilation errors related to function signatures and macro definitions that appeared when building against strict standards.


### Critical Fixes

*   **Function Signature Mismatch:** Fixed a critical bug in `SituationCmdBindVertexBuffer` where the implementation signature did not match the header declaration, causing immediate compilation failure.

*   **Extension Macro Safety:** Fixed `SituationGetVRAMUsage` to correctly guard GLAD extension macros (like `GL_NVX_gpu_memory_info`) with `#ifdef`. This prevents "undeclared identifier" errors when compiling with headers that don't include specific vendor extensions.

*   **Control Flow Safety:** Refactored `SituationCreateVirtualDisplay` to replace `goto` error handling with a structured control flow. This eliminates potential "jump bypasses variable initialization" warnings in strict C modes.


### Internal Refactoring

*   **Global State Architecture:** Refactored the `_SituationGlobalStateContainer`. OpenGL state variables are now grouped into a dedicated `_SituationGLState` struct (`sit_gs.gl`), mirroring the Vulkan backend's structure. This improves code organization and maintainability.

*   **Standard Compliance:**
*   Added `_POSIX_C_SOURCE` and `_XOPEN_SOURCE` feature macros to correctly expose system headers on Linux/macOS.
*   Replaced all C++ style empty initializers (`{}`) with C11 universal zero initializers (`{0}`).
*   Implemented internal helpers `_sit_strdup` and `_sit_strcasecmp` to remove dependency on non-standard headers.
*   Replaced deprecated `usleep` with `nanosleep` for POSIX frame limiting.


---

## [2.3.4B "Velocity" (Hotfix B)] - 2025-11-22

### Description

This update focuses on achieving strict standard compliance and cross-platform portability. It eliminates non-standard C extensions, ensuring the library compiles cleanly under strict C11 environments (e.g., `gcc -std=c11 -pedantic`) while maintaining full backend fidelity.


### Critical Fixes

*   **Strict C11 Syntax:** Replaced all instances of C++ style empty struct initialization (`{}`) with the universal zero initializer (`{0}`). This resolves syntax errors in strict C compilers throughout the Vulkan backend and internal structures.

*   **Portability Layer:** Replaced non-standard POSIX string functions (`strdup`, `strcasecmp`) and threading calls (`usleep`) with internal, standard-compliant helper implementations (`_sit_strdup`, `_sit_strcasecmp`, `nanosleep`). Added necessary feature test macros (`_POSIX_C_SOURCE`) to correctly expose system headers on Linux/macOS.

*   **Control Flow Refactoring:** Rewrote `SituationCreateVirtualDisplay` to eliminate `goto` statements and fix variable scoping issues. This prevents "jump bypasses initialization" warnings and improves code safety during resource cleanup.

*   **Math Constants:** Added fallback definitions for `M_PI_2` to ensure compilation on MSVC and strict C11 math environments where non-standard constants are not defined by default.


---

## [2.3.4A "Velocity" (Hotfix)] - 2025-11-22

### Description

This patch solidifies the "Velocity" feature set, addressing specific architectural constraints in the Vulkan backend and ensuring strict C standard compliance. It transforms the Virtual Display compositor into a "Titanium" grade implementation, guaranteeing validation-free operation for advanced blending modes.


### Critical Fixes

*   **Vulkan Compositor Architecture:** Completely rewrote the `SituationRenderVirtualDisplays` logic for Vulkan. It now automatically manages Render Pass state (starting/stopping) to perform legal `vkCmdCopyImage` operations. This fixes validation errors when using "Screen Grab" blend modes (Overlay, Soft Light) and ensures correct layering over the main scene.

*   **Scaling Math Correction:** Fixed the matrix calculation for `SITUATION_SCALING_STRETCH`. It now correctly scales content to fill the *target* framebuffer dimensions rather than preserving the source resolution 1:1.

*   **Strict C Compliance:** Removed C++-style syntax (lambdas and anonymous struct initializers) from the implementation block. The library now compiles cleanly on strict C99/C11 compilers (MSVC/GCC/Clang) without warnings.


### Documentation

*   **API Reference:** Added comprehensive Doxygen-style header documentation for the entire Hot-Reloading module (`SituationReloadShader`, `SituationReloadTexture`, etc.), detailing synchronization behavior and usage constraints.


---

## [2.3.4 "Velocity"] - 2025-11-22

### Description

This release transforms "Situation" from a static framework into a live development environment. The "Velocity" update introduces a comprehensive **Hot-Reloading Module**, allowing developers to modify Shaders, Compute Pipelines, Textures, and 3D Models on disk and see the changes instantly in the running application without restarting. This feature significantly accelerates the iteration loop for visual adjustments and shader programming.


### New Features

*   **Hot-Reloading Module:** Added a suite of functions to safely reload assets at runtime. The engine handles the complex task of synchronization with the GPU, ensuring the device is idle, destroying old resources, and seamlessly swapping in the new data while maintaining the original handle IDs.
*   `SituationReloadShader`: Recompiles and links graphics pipelines.
*   `SituationReloadComputePipeline`: Recompiles compute shaders, preserving the original layout configuration.
*   `SituationReloadTexture`: Re-uploads image data to GPU memory (requires texture to be loaded from file).
*   `SituationReloadModel`: Re-parses GLTF/GLB files and rebuilds all sub-meshes and material textures.

*   **Texture Loading Helper:** Added `SituationLoadTexture(path, mips)`. This new high-level function combines image loading, texture creation, and cleanup into one call. Crucially, it registers the file path with the internal resource tracker, making the texture eligible for hot-reloading.

*   **Shader #include Support:** The runtime GLSL compiler (`shaderc` integration) now supports `#include "filename.glsl"` directives. This allows developers to construct complex "Uber Shaders" by sharing common logic and struct definitions across multiple shader files.


### Critical Bug Fixes

*   **Vulkan Stale Layout Crash:** Fixed a "time bomb" crash in the Vulkan backend where a failed shader hot-reload (e.g., due to syntax error) would destroy the pipeline layout but leave a dangling pointer in the global state cache. Subsequent calls to `SituationCmdSetPushConstant` would then crash the driver. The system now correctly invalidates the cached layout on binding failure.


### Internal Improvements

*   **Resource Path Tracking:** The internal linked-list resource managers have been upgraded to store the source file paths of loaded assets.
*   Updated all `SituationLoad*` functions to capture and store these paths upon successful creation.
*   Updated all `SituationDestroy*` and `SituationUnload*` functions to correctly free these path strings, ensuring zero memory leaks.

*   **Compute Pipeline State:** Updated `_SituationComputePipelineNode` to cache the `SituationComputeLayoutType` used during creation. This ensures that when a compute pipeline is hot-reloaded, it is rebuilt with the exact same descriptor layout as the original.


---

## [2.3.3D "Production"] - 2025-11-22

### Description

This patch resolves three high-priority logic errors discovered in the Vulkan and Audio backends of the "Hardened" release. While 2.3.3C introduced the architecture for robustness, 2.3.3D connects the final wires to ensure those systems function correctly under real-world stress tests. It is highly recommended for all users to update to this version immediately.


### Critical Bug Fixes

*   **Audio Capture Dispatch:** Fixed a "phantom" logic bug where the audio capture callback was executing on the high-priority audio thread, completely bypassing the thread-safe ring buffer intended for the main thread. The callback now correctly linearizes data into the ring buffer, ensuring thread safety for user logic.

*   **Vulkan Screenshot Crash:** Fixed a validation error and potential device loss when calling `SituationTakeScreenshot` inside a render loop. The image layout transition was incorrectly assuming `PRESENT_SRC`, causing barriers to fail. It now correctly handles `COLOR_ATTACHMENT_OPTIMAL` transitions.

*   **Pipeline Layout Leak:** Added missing cleanup logic in `_SituationVulkanCreateComputePipeline`. Previously, if shader compilation failed or the pipeline creation errored, the intermediate `VkPipelineLayout` object was leaked on the GPU.


### Architectural Improvements

*   **Dynamic Descriptor Manager:** Finalized the implementation of the dynamic descriptor pool system.
*   Added the `descriptor_manager` struct to the global Vulkan state.
*   Implemented `_SituationVulkanAllocateDescriptorSet` to automatically create and register new descriptor pools when the current one fills up.
*   Fixed initialization logic to correctly "seed" the manager with the initial persistent pool, preventing immediate duplicate pool creation on startup.

*   **Backend Consistency:** Added internal state tracking (`debug_draw_command_issued_this_frame`) to detect and warn developers (in debug builds) if `SituationUpdateBuffer` is called after draw commands, preventing divergent behavior between OpenGL (Immediate) and Vulkan (Deferred) backends.


---

## [2.3.3C "Hardened"] - 2025-11-21

### Description

This release is a major stability overhaul focused on thread safety, resource management, and preventing runtime crashes in long-running applications. It addresses several critical architectural flaws identified in the Audio and Vulkan backends, transforming the library from a prototype into a production-ready framework.


### Breaking Api Changes

*   **Audio Loading Strategy:** The signature of `SituationLoadSoundFromFile` has changed.
*   *Old:* `(path, looping, out_sound)`
*   *New:* `(path, mode, looping, out_sound)`
*   *Reason:* Users must now specify `SITUATION_AUDIO_LOAD_AUTO`, `FULL` (RAM decode), or `STREAM` (Disk I/O) to prevent the audio thread from blocking on disk operations.


### Critical Stability Fixes

*   **Audio Thread "Death Spiral":** Completely rewrote the audio loading logic. Short sounds (SFX) are now decoded fully to RAM upon load. The audio callback no longer performs blocking disk I/O for these sounds, eliminating stuttering/popping during gameplay or background loading.

*   **Vulkan Descriptor Exhaustion:** Replaced the fixed-size descriptor pool (limit 512) with a **Dynamic Descriptor Manager**. The engine now automatically allocates new pools as needed, allowing for an effectively infinite number of textures and materials.

*   **OpenGL State Leak:** Implemented a "State Guard" in `SituationRenderVirtualDisplays`. The compositor now backs up the active Shader Program, VAO, Texture Units, and Blend Modes before rendering and restores them exactly afterwards. This prevents internal rendering passes from corrupting user rendering state.

*   **Compute Pipeline Crash:** Fixed a severe memory offset bug in `SituationDestroyComputePipeline`. The function was passing a public struct pointer to an internal helper expecting a different memory layout, which would have caused heap corruption or driver crashes on cleanup.


### Optimizations

*   **O(1) Input Processing:** Replaced the `O(N)` memory-shifting queues in the Keyboard, Mouse, and Gamepad subsystems with **Ring Buffers**. Input processing time is now constant regardless of queue depth.

*   **Audio Capture Logic:** Fixed a logic hole in `SituationPollInputEvents` where captured audio data was locked but never dispatched to the user. Added a thread-safe linearization step to correctly pass ring-buffer data to the user callback.


---

## [2.3.3B "Refinement"] - 2025-11-21

### Description

This patch release resolves critical compilation errors in the Vulkan backend introduced in 2.3.3A. It solidifies the "Unified Resource" system, ensuring textures and compute pipelines are correctly configured and stable across both backends.


### Api Changes & Improvements

*   **Unified Texture Creation:** `SituationCreateTexture()` now automatically applies `VK_IMAGE_USAGE_STORAGE_BIT` (Vulkan) and compatible storage flags (OpenGL) to all new textures.
*   *Impact:* All textures created via the standard API are now "Compute-Ready" by default. Users can bind any texture to a Compute Shader without needing special creation flags or distinct API calls.


### Bug Fixes

*   **Vulkan Compilation:** Resolved an undefined variable error (`usage_flags`) inside `SituationCreateTexture` that prevented the library from compiling when `SITUATION_USE_VULKAN` was defined.

*   **Missing Definitions:** Added the missing `SituationTextureUsageFlags` enum and replaced undefined macros in `_SituationInitVulkan` with defined numeric constants for descriptor pool sizing.

*   **Compute Binding Crash:** Fixed a runtime crash during Vulkan initialization by ensuring the `storage_image_layout` is correctly created. This resolves issues when binding textures to Compute Shaders.


---

## [2.3.3A "Refinement"] - 2025-11-21

### Description

This maintenance release tightens the API surface and improves developer ergonomics. It addresses several "friction points" identified in previous versions, particularly around string handling and file export safety.


### Api Changes & Improvements

*   **Static Version String:** `SituationGetVersionString()` now returns a pointer to a static, read-only string buffer.
*   *Impact:* Users no longer need to `free()` the returned pointer, making version logging a simple one-liner: `printf("Situation v%s\n", SituationGetVersionString());`.

*   **Strict PNG Screenshots:** `SituationTakeScreenshot()` now strictly enforces the `.png` file extension.
*   *Impact:* Prevents silent failures or garbage output when users attempt to save with unsupported extensions (like `.jpg` or `.txt`). The function now returns `false` and sets a clear error message if a non-PNG path is provided.
*   *Cleanup:* The legacy BMP fallback writer has been removed to reduce binary size and maintenance surface area.

*   **Documentation:** Added comprehensive internal documentation for complex Vulkan helpers (e.g., `_SituationVulkanBlitImageToHostVisibleBuffer`) to aid future maintenance and auditing.


### Bug Fixes

*   **VRAM Reporting:** Implemented a multi-backend strategy for `SituationGetVRAMUsage()`. It now supports Windows (DXGI), Vulkan (VMA), and NVIDIA OpenGL extensions, providing accurate memory tracking across a wider range of configurations.


---

## [2.3.3 "Insight"] - 2025-11-21

### Description

Version 2.3.3 is a "quality of life" feature release that expands the developer's ability to monitor performance and embed assets. It introduces the "Small but Deadly" feature set: direct memory loading for fonts, formatted text drawing, and hardware profiling hooks.


### New Features

*   **Memory Asset Loading:** Added `SituationLoadFontFromMemory`. This allows developers to embed fonts (e.g., using `xxd -i`) directly into their executable for truly single-file distribution, bypassing the filesystem.
*   **Formatted Text:** Added `SituationImageDrawTextFormatted`. This convenience function accepts `printf`-style format strings (e.g., `"Score: %d", score`), eliminating the need for users to manually `snprintf` into temporary buffers before drawing text.
*   **Hardware Info:** Added `SituationGetGPUName()` to retrieve the human-readable model name of the active graphics adapter.


### Profiling & Diagnostics

*   **Draw Call Counting:** The engine now tracks the number of draw commands issued per frame. This data is accessible via `SituationGetDrawCallCount()`.
*   **VRAM Monitoring:** On Vulkan, `SituationGetVRAMUsage()` now returns the precise number of bytes allocated by the engine's internal allocator (VMA), allowing for real-time memory budget monitoring.


### Internal Improvements

*   **Quad Renderer Refactor:** The internal 2D quad renderer has been modernized (Phase 2.5 prep). It now supports dynamic UV coordinates via push constants, paving the way for future GPU-accelerated text rendering.
*   **Pipeline Layout Optimization:** Vulkan internal pipelines now use more efficient layout creation strategies, reducing initialization overhead.


---

## [2.3.2D "Integrity"] - 2025-11-20

### Description

This is a critical stability release. It addresses a severe race condition in the audio streaming subsystem, plugs memory leaks in the Vulkan swapchain recreation logic, and ensures OpenGL state isolation.


### Critical Fixes

*   **Audio Stream Thread-Safety:** Completely refactored `SituationLoadSoundFromStream`. Previously, a shared global vtable caused race conditions and data corruption when loading multiple streams. This has been replaced with instance-based callback storage and static thunks, ensuring complete isolation and thread safety.
*   **Vulkan Swapchain Leak:** Fixed a memory leak where the `swapchain_images` handle array was not freed during swapchain recreation (e.g., window resize).
*   **OpenGL State Corruption:** `SituationRenderVirtualDisplays` now correctly saves and restores the Depth Test state (`GL_DEPTH_TEST`), preventing it from accidentally enabling depth testing for subsequent 2D rendering passes.


### Logic & Safety

*   **Vulkan Pipeline Barriers:** Fixed logic that prevented Execution-Only barriers (barriers with 0 access masks but valid stage masks) from being recorded.
*   **Model Loading Safety:** `SituationLoadModel` now validates texture creation. If a texture file is missing, it logs a warning instead of assigning a null ID, preventing silent rendering failures.
*   **Shader Debugging:** Increased the internal error message buffer size (to 2048 bytes) to prevent truncation of complex GLSL compilation errors.
*   **API Clarity:** `SituationCmdSetVertexAttribute` now returns a precise error message on Vulkan, explaining that vertex formats are immutable in that backend.


---

## [2.3.2C "Zero Friction"] - 2025-11-20

### Description

This micro-release focuses purely on developer experience and removal of friction. It transforms "Situation" into a true "drop-in" library where advanced features like image loading, screenshots, and text rendering work immediately without external configuration.


### Zero Friction Updates

*   **Embedded STB Libraries:** `stb_image`, `stb_image_write`, and `stb_truetype` are now automatically implemented by `SITUATION_IMPLEMENTATION`.
*   **Impact:** `SituationLoadImage`, `SituationTakeScreenshot(.png)`, and `SituationDrawTextStyled` work out-of-the-box with zero additional defines or includes.
*   **Opt-Out:** Users can define `SITUATION_NO_STB` (or specific flags like `SITUATION_NO_STB_IMAGE`) to disable this if they manage dependencies externally.


### Api Additions

*   **Version Querying:** Added `SITUATION_VERSION_MAJOR/MINOR/PATCH` macros and `SituationGetVersionString()` for runtime version checking.


### Summary

With 2.3.2C, the "Hello World" for a graphical, audio-enabled application with font support is now literally one C file with two #defines.


---

## [2.3.2B "Consistency"] - 2025-11-20

### Description

This hotfix eliminates cross-backend inconsistencies and improves safety. The execution-model difference between OpenGL and Vulkan is now actively enforced in debug builds.


### Critical Fixes & Behavioral Guarantees

*   **Enforced Buffer-Update-Before-Draw Rule:** In OpenGL debug builds, `SituationUpdateBuffer` now triggers a loud warning/error if called *after* any draw command in the current frame. This prevents the most common cause of cross-backend logic divergence.
*   **SituationSetGamepadVibration Return Type:** Now returns `bool` (true on success) instead of `void`. It correctly returns `false` and sets an error on non-Windows platforms.
*   **PNG Screenshots:** `stb_image_write.h` is now automatically implemented if available (unless `SITUATION_NO_STB_PNG` is defined), fixing silent failures when saving PNGs.


### Usability

*   **SITUATION_BEGIN_FRAME():** Added a macro to standardize the start of the render loop (`PollInput` + `UpdateTimers`).
*   **Main-Thread Audio Capture:** Added `SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD` flag to `SituationInitInfo`. When set, audio capture callbacks are safely routed to the main thread during `SituationPollInputEvents`, preventing threading crashes for users.
*   **Vulkan No-Shaderc Fallback:** Internal 2D renderers now elegantly disable themselves if `SITUATION_ENABLE_SHADER_COMPILER` is not defined, removing the hard dependency on `shaderc` for basic initialization.


### Internal Refactoring

*   **Vulkan Pipeline Optimization:** Refactored the internal Advanced Compositing pipeline initialization. It now reuses existing descriptor set layouts instead of allocating temporary ones, reducing code size and initialization overhead.


---

## [2.3.2A "Hotfix"] - 2025-11-20

### Description

This is a maintenance release addressing critical issues identified in the v2.3.2 "Parity" release. It focuses on correcting data capture logic in Vulkan, improving cross-platform error reporting, and loosening dependency requirements.


### Critical Fixes

*   **Fixed Vulkan Screenshot Source:** `SituationLoadImageFromScreen` (and by extension `SituationTakeScreenshot`) now captures the `current_image_index` instead of `last_presented_image_index`. Previously, taking a screenshot in Vulkan would capture the *previous* frame's output.

*   **Non-Windows Gamepad Error:** `SituationSetGamepadVibration` now properly sets the `SITUATION_ERROR_NOT_IMPLEMENTED` error code on Linux and macOS, rather than failing silently.

*   **Optional Shaderc Dependency:** The `#error` forcing `SITUATION_ENABLE_SHADER_COMPILER` for Vulkan has been removed. Users can now compile the Vulkan backend without `shaderc` if they provide their own pre-compiled SPIR-V pipelines.
*   *Note:* Disabling the compiler disables the internal 2D renderers (`SituationCmdDrawQuad` and Virtual Displays) on Vulkan, as they rely on runtime GLSL compilation.


### Documentation

*   **Execution Model Warning:** Added a critical warning to the documentation regarding the Immediate (OpenGL) vs. Deferred (Vulkan) execution models. Developers are strictly advised to update all buffer data *before* recording draw commands to ensure consistent behavior across backends.


---

## [2.3.2 "Parity"] - 2025-11-19

### Description

Version 2.3.2 addresses the major feature gaps identified in previous versions, achieving functional parity between the OpenGL and Vulkan backends, and introducing key new capabilities. This release enables "Advanced Blending" for Vulkan Virtual Displays, adds a complete Audio Capture (Microphone) API, and finalizes the 3D Model Exporting tools. Under the hood, it includes critical fixes for Vulkan synchronization (pipeline barriers), memory safety, and descriptor binding logic.


### New Features

*   **Audio Capture API:** Added `SituationStartAudioCapture`, `SituationStopAudioCapture`, and the `SituationAudioCaptureCallback` type. This allows applications to record raw audio data (Mono, 32-bit Float, 44.1kHz) from the system's default input device for real-time processing.

*   **Vulkan Advanced Blending:** Implemented the complex "Copy-Before-Draw" architecture for the Vulkan backend. `SituationRenderVirtualDisplays` now correctly handles advanced blend modes (Overlay, Soft Light, etc.) on Vulkan by copying the swapchain image to a readable texture before rendering, matching the visual fidelity of the OpenGL backend.

*   **3D Model Exporting:** Finalized the `SituationSaveModelAsGltf` utility. This required implementing the previously stubbed `SituationGetMeshData` function to perform geometry readback from the GPU to CPU memory, enabling users to save runtime-generated or modified meshes to standard `.gltf` files.


### Improvements & Fixes

### [CRITICAL] Vulkan Synchronization & Stability
*   **Fixed Buffer Readback Synchronization:** Completely refactored `SituationGetBufferData`. It now uses a new internal helper `_SituationVulkanReadBackBuffer` that correctly inserts `vkCmdPipelineBarrier` commands before and after transfers. This fixes race conditions where the CPU would read stale data before the GPU finished writing.
*   **Fixed Texture Usage Flags:** `SituationCreateTexture` now automatically includes the `VK_IMAGE_USAGE_STORAGE_BIT`. This fixes a crash/validation error when binding textures to Compute Shaders (`SituationCmdBindComputeTexture`).
*   **Fixed Memory Leaks in Buffer Updates:** Resolved memory leaks in `SituationUpdateBuffer` where temporary staging buffers were not freed if mapping or command allocation failed.

### [BUG FIXES]
*   **OpenGL Initialization:** Implemented the missing `_SituationInitGLVirtualDisplayRenderer` function. Previously, the OpenGL Virtual Display compositor relied on uninitialized Vertex Array Objects, leading to potential rendering failures.
*   **Vulkan Compute Binding:** Fixed logic in `SituationCmdBindComputeTexture` that ignored the user's `binding` parameter and always bound to index 0.
*   **API Safety:** Explicitly disabled `SituationCmdSetVertexAttribute` on the Vulkan backend (returning `SITUATION_ERROR_NOT_IMPLEMENTED`), as dynamic vertex format changes are architecturally impossible in Vulkan pipelines.

### [REFACTORING]
*   **Internal Helpers:** Refactored massive logic blocks from `SituationGetBufferData` into reusable internal helpers, which allowed `SituationGetMeshData` to share the robust memory readback logic without code duplication.
*   **Documentation:** Added comprehensive header documentation for all new internal helpers (`_SituationVulkanCreateScreenCopyResource`, `_SituationVulkanReadBackBuffer`, etc.) and updated public API headers to reflect the new capabilities.


---

## [2.3.1A "Refinement"] - 2025-10-31

### Description

Version 2.3.1A is a significant quality-of-life and performance refinement of the "Base" API. This update focuses on improving API safety, enhancing performance on the Vulkan backend, achieving greater feature parity between backends, and improving documentation clarity. While introducing no new major features, this release makes the existing API more robust, efficient, and easier to use correctly.


### Changes & Improvements

### [CRITICAL] API & Main Loop Refinement

*   **Deprecated SituationUpdate():** The monolithic SituationUpdate() function has been deprecated. It encouraged a main loop structure that was less explicit and prone to off-by-one-frame input bugs.

*   **New Main Loop Workflow:** Introduced two new core functions, SituationPollInputEvents() and SituationUpdateTimers().
-   SituationPollInputEvents() is now the dedicated function for gathering all OS events and updating input state for the current frame.
-   SituationUpdateTimers() is the dedicated function for advancing all internal clocks, calculating delta time, and updating joystick/gamepad state.

*   **Updated Documentation:** The API documentation has been updated to reflect this new, clearer main loop structure, providing a best-practice example to guide users.

### [PERFORMANCE] Vulkan Backend Enhancements

*   **High-Performance Virtual Display Compositing:** The SituationRenderVirtualDisplays() function on the Vulkan backend has been completely overhauled. It now uses the "Persistent Descriptor Set" pattern, pre-allocating a descriptor set for each virtual display at creation time. This eliminates all runtime descriptor allocation and updates from the main render loop, resulting in a massive performance improvement when compositing many virtual displays.

*   **Unified & Performant Resource Binding:**
-   Introduced SituationCmdBindDescriptorSet() and SituationCmdBindTextureSet() as the new, primary API for binding buffers and textures.
-   Deprecated the older, less explicit SituationCmdBindUniformBuffer, SituationCmdBindTexture, and SituationCmdBindComputeBuffer functions, which now wrap the new API.
-   This change leverages the persistent descriptor set model for ALL buffers and textures, making resource binding a consistently fast, low-overhead operation on Vulkan.

### [BUG FIXES & STABILITY]

*   **Fixed Input System Crash:** Resolved a critical bug where the mouse input callbacks would attempt to use an uninitialized mutex, leading to a crash. The mouse state struct now correctly contains and initializes its mutex, ensuring thread-safe event handling.

*   **Fixed Vulkan Resource Leak:** Corrected a resource leak in the internal Vulkan pipeline creation logic. The VkPipelineLayout is now properly destroyed if the subsequent vkCreateGraphicsPipelines call fails, preventing leaks on shader compilation or linking errors.

### [DOCUMENTATION & API CLARITY]

*   **Detailed Color Function Docs:** The documentation for all color conversion functions (SituationRgbToHsv, SituationColorToYPQ, etc.) and SituationImageAdjustHSV has been significantly expanded to explain the color spaces, parameter ranges, and algorithms used.

*   **Improved Image Drawing Docs:** The documentation for all SituationImageDraw...() functions has been updated to clarify their distinct rendering methods (bitmap vs. SDF), performance characteristics, boundary handling, and alpha blending formulas.

*   **Added API Safety Functions:** Introduced new helper functions to make the library's manual memory management safer and more explicit:
-   SituationFreeDisplays() for correctly deallocating the complex SituationDisplayInfo array.
-   SituationFreeString() as the designated function for freeing strings returned by the library.
-   Documentation for all functions that return heap-allocated data now explicitly points to these new, safe deallocation functions.


### Known Issues & Feature Gaps

*   **Vulkan Advanced Blending:** The Vulkan backend's SituationRenderVirtualDisplays function currently only supports simple blend modes (Alpha, Additive, etc.). The complex, multi-pass logic required for advanced Photoshop-style blend modes (Overlay, Soft Light), which is fully implemented in the OpenGL backend, remains a feature gap. This will be addressed in a future update.


---

## [2.3.1 "Base"] - 2025-10-18

### Description

Version 2.3.1, designated as the "Base" version, establishes the foundational public API for the "Situation" library. This release provides a single-file, cross-platform C/C++ library designed to abstract low-level system interactions for windowing, graphics, audio, and input. The primary goal of this version is to offer a stable, lean, and powerful foundation for building sophisticated, high-performance software, such as games, creative coding projects, and data visualization tools.


### Scope & Key Features

This version includes a comprehensive feature set across several core domains:

*   **Lifecycle & Windowing:** Full application lifecycle management (`SituationInit`, `SituationShutdown`) and robust window controls (fullscreen, borderless, multi-monitor awareness) via a GLFW3 backend.
*   **Dual Graphics Backend:** A unified graphics API with compile-time support for both modern OpenGL (4.6+ Core) and Vulkan (1.1+). This includes abstractions for shaders, meshes, textures, and generic buffers.
*   **Command Buffer Model:** A core architectural feature for recording rendering and compute commands. This provides a modern, explicit model for GPU interaction, inspired by Vulkan.
*   **Compute Shaders:** A unified API for GPGPU tasks, supporting both OpenGL Compute Shaders and Vulkan Compute Pipelines, with runtime GLSL-to-SPIR-V compilation via `shaderc`.
*   **2D & 3D Rendering:** High-level helpers for drawing 2D primitives (quads) and textured sprites, alongside a robust system for rendering 3D meshes. Includes a Virtual Display system for off-screen rendering, UI layering, and post-processing.
*   **Audio System:** A full-featured audio engine powered by `miniaudio`, supporting playback, capture, device enumeration, and a real-time effects chain (Filters, Echo, Reverb) with support for custom DSP callbacks.
*   **Input Handling:** Unified polling and event-based handling for keyboard, mouse, and gamepads.
*   **Timing System:** Includes high-resolution timers, FPS management, and an advanced "Temporal Oscillator System" for creating rhythmically synchronized events.
*   **Filesystem Utilities:** A cross-platform API for path manipulation and file I/O, including access to standard application directories.


### Implementation Details

*   **Header-Only Library:** The library is distributed as a single header file (`situation.h`). The implementation is included by defining `SITUATION_IMPLEMENTATION` in one C/C++ file.
*   **Dependencies:**
*   **Required:** GLFW3, cglm.
*   **Optional (Backend-Specific):** GLAD (for OpenGL), Vulkan SDK (for Vulkan).
*   **Optional (Features):** `stb_image`, `stb_truetype`, `miniaudio`.
*   **Resource Management:** The library follows an explicit, manual resource management philosophy. All resources created with `SituationCreate*` or `SituationLoad*` functions must be manually destroyed with their corresponding `SituationDestroy*` or `SituationUnload*` functions. The library includes leak detection at shutdown to assist developers.


### Quirks & Notable Design Decisions

*   **[CRITICAL] Single-Threaded API:** All `SITAPI` functions **must** be called from the main thread (the thread that called `SituationInit`). The library is not internally synchronized, and calling API functions from other threads will lead to undefined behavior and likely crashes. Any multithreading must be managed by the client application, with communication back to the main thread for any API calls.
*   **[CRITICAL] Emulated OpenGL Command Buffer:** While the API presents a unified command buffer model, its execution differs significantly between backends. On Vulkan, commands are deferred and executed upon `SituationEndFrame()`. On OpenGL, the command buffer is an *emulation*, and `SituationCmd*` calls often translate to immediate OpenGL API calls. Developers must not write code that depends on the deferred execution of commands when using the OpenGL backend.
*   **Explicit Backend Selection:** The graphics backend (OpenGL or Vulkan) must be selected at compile time by defining either `SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`.
*   **Manual Memory Management for Returned Data:** Functions that return dynamically allocated data (e.g., `SituationGetLastErrorMsg()`, `SituationGetDisplays()`) explicitly state that the caller is responsible for freeing the memory to prevent leaks.

---
--------------------------------------------------------------------------------
v2.3.38 (2025-??-??) - Resonance Module
--------------------------------------------------------------------------------
- [NEW] Added **Resonance Module** for procedural audio synthesis (zero-allocation).
  - `SituationPlayTone`: Play sine, square, triangle, or saw waves with full ADSR envelopes.
  - `SituationPlayMidiNote`: Play tones using MIDI note numbers (0-127).
  - `SituationStopAllTones`: Panic function to stop all procedural sounds.
  - Supports 64-voice polyphony with intelligent voice stealing (prioritizes releasing/oldest notes).
  - Frame-perfect timing using integer frame counters instead of floats.
  - Zero-allocation design: All voices pre-allocated in `sit_audio.tone_pool`.
- [INT] Integrated Resonance mixer into the main audio callback (runs after SFX/DSP).
- [INT] Added `SituationWaveType` enum and `SituationTone` structure.
- [FIX] Updated cleanup routines to properly uninitialize synth waveforms.
