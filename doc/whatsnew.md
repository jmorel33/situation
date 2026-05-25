<div align="center">
  <img src="situation_blackMetal_logo.jpg" alt="Situation logo">
</div>

# What's New in Situation API

_For Core API library v2.4.70_


*   **API Expansion Phase 4 (v2.4.70):** 🎉 **COMPLETE!** Transitioned to explicit rasterization and rendering state control. Added soft-command support for depth, culling, and blending (`SituationCmdSetDepthTest`, etc.), implemented debug groups (`vkCmdBeginDebugUtilsLabelEXT` / `glPushDebugGroup`), and correctly sandboxed implicit geometry (Quads/Text) with `_SitGLBackupState` to avoid user state clobbering.
*   **API Expansion Phase 3 (v2.4.69):** 🎉 **COMPLETE!** Added unified helpers for uniform arrays and matrices (`SituationSetShaderUniform1fv`), implemented shader uniform validation, expanded logging interfaces, and unified backend capability querying.
*   **Readback Test Harness Parity (v2.4.68):** 🛠️ **COMPLETE!** Expanded the C-level test harness (`sit_test.exe`) to cover all Phase 1 and Phase 2 Readback APIs. Discovered and resolved an OpenGL command buffer append linkage error in the process.
*   **Vulkan Diagnostic Parity & Readbacks (v2.4.67):** 🛠️ **COMPLETE!** Added a full `4x4` synchronous staging diagnostic readback for `K-Term` in the Vulkan backend, resolving dormant Vulkan structural references.
*   **Phase 1 Async Buffer Readback (v2.4.66):** 🛠️ **COMPLETE!** Implemented `SituationCreateReadbackBuffer`, `SituationCmdCopyBuffer`, and `SituationReadBuffer`. Backend parity achieved between OpenGL and Vulkan with `SIT_OP_COPY_BUFFER` tracking in the OpenGL execution loop and persistent mapping. Full sequential `sit_test` passed for both backends.
*   **OpenGL soft-buffer text over quads (v2.4.64):** 🛠️ **COMPLETE!** **`SIT_OP_DRAW_TEXT` / `SIT_OP_DRAW_TEXT_EX`** execution disables depth test and enables alpha blending around glyph draws so HUD and overlays are visible after quad geometry writes the same orthographic depth — full sequential **`sit_test`** (**OpenGL DLL**) **310/310** (see **`doc/UPDATELOG.md`**).
*   **Echo node graph dry/wet (v2.4.63):** 🛠️ **COMPLETE!** Delay line out-of-place mix; UI wet applied once (no **`w²`** tail). **`doc/UPDATELOG.md`**.
*   **OpenGL diagnostics & compute harness (v2.4.62):** 🛠️ **COMPLETE!** Default-font / text / VD init **`stdout`** lines are gated behind **`SITUATION_VERBOSE_DIAGNOSTICS`** (same knob as Vulkan extras). **`SIT_COMPUTE_LAYOUT_TWO_SSBOS`** assigns **`glShaderStorageBlockBinding`** for **`InBuffer` / `OutBuffer`** so **`compute_chained_dispatches`** matches Vulkan’s two-set SPIR-V layout — full sequential **`sit_test`** (**OpenGL DLL**) **310/310**.
*   **Vulkan VD compositing & graphics harness (v2.4.61):** 🎉 **COMPLETE!** Virtual-display compositing preserves the caller’s framebuffer (**resume render pass / LOAD**), screen-copy targets are recreated after swapchain rebuild, and Path A/B **`vkCmdPushConstants`** ranges match SPIR-V layouts. Vulkan **`sit_test --module graphics`** reaches **78/78** on the maintained harness.
*   **Vulkan VD advanced compositor (v2.4.60):** 🎉 **COMPLETE!** Three-set pipeline layout for Path A with correct destination sampler (**binding 5**). See **`doc/UPDATELOG.md`**.
*   **Vulkan test harness & backend hardening (v2.4.42 era):** 🛠️ **COMPLETE!** First sustained push: shaders, buffers, VD creation/compositing, pipeline layouts, screenshot readback — toward full Vulkan graphics parity (superseded by fixes through **v2.4.61**).
*   **Graphics regression sweep (v2.4.41 era):** 🎉 **COMPLETE!** Bulk clears for uniforms, textured draws, compute binding, buffer updates, GL re-init hygiene; per-module counts evolve — see **`doc/UPDATELOG.md`**.
*   **Node graph takeover — mixer removed (v2.4.36):** 🛠️ **COMPLETE!** Legacy mixer API removed; **`SituationAudioGraph`** + **`SituationProcessGraph`** are the routing path. miniaudio stays the device backend.
*   **Audio node graph — devices live (v2.4.35):** 🎉 **COMPLETE!** Device types registered and processed through the graph (**`SituationCreateNode`**).
*   **Test harness expansion (v2.4.33):** 🎉 **COMPLETE!** Broad audio coverage: registry, graph lifecycle, effects, serialization, MIDI.
*   **Test harness complete (v2.4.28):** 🎉 **COMPLETE!** **CTest**-based runs, reporters, leak detection.
*   **Test harness framework (v2.4.24):** 🛠️ **COMPLETE!** DLL-linked black-box **SITAPI** harness.
*   **Renderer robustness audit (v2.4.18–v2.4.20):** 🛠️ **COMPLETE!** GL/VK resource paths and frame lifecycle.
*   **X-macro errno (v2.4.13):** 🛠️ **COMPLETE!** **`SituationError`** table-driven messages.
*   **Threading manicure (v2.4.11):** 🛠️ **COMPLETE!** Thread-pool hardening.
*   **Internal subsystem extractions (v2.4.5–v2.4.9):** 🛠️ **COMPLETE!** Modular **`sit/situation_impl_*.h`** split.
*   **Virtual display compositing & UBO performance (v2.4.3):** 🚀 **COMPLETE!** VD compositing fixes; less main-thread UBO stall.
*   **OpenGL deferred rendering architecture (v2.4.2):** 🛠️ **COMPLETE!** GL/Vulkan structural parity.
*   **Complete MIDI architecture (v2.4.1):** 🎹 **COMPLETE!** Routing, transforms, recording, UD inquiry.
*   **OpenGL graveyard flush safety (v2.4.1):** 🧹 **COMPLETE!** **`_SitGLFlushGraveyard`** waits on prior-frame **`GL_ARB_sync`** before cleanup.
*   **Modular revolution (v2.4.0):** 🎉 **COMPLETE!** Monolithic impl split into **16** internal modules.

*   **Audio modularization (v2.3.61):** 🧹 **COMPLETE!** Extracted the internal Reverb (`sit/aud/reverb.h`) and Echo (`sit/aud/echo.h`) implementations into standalone headers to improve codebase modularity.
*   **Uniform Optimization (v2.3.60):** 🛠️ **COMPLETE!** Implemented dynamic resizing for the internal OpenGL uniform hash map. The map now doubles its capacity and rehashes entries when the load factor exceeds 0.75, ensuring stable performance for complex shaders.
*   **Mixer Persistence (v2.3.59):** 🎉 **COMPLETE!** Implemented Phase 5 of the Audio Mixer architecture. Added full session persistence (Save/Load) with cached EQ/Dynamics state, capture device binding, and thread-safe parameter caching.
*   **FX & Metering (v2.3.58):** 🎉 **COMPLETE!** Implemented Phase 4 of the Audio Mixer architecture. Added FX Insert slots for Aux buses and atomic peak metering for all tracks and buses.
*   **Mixer Routing (v2.3.57):** 🎉 **COMPLETE!** Implemented Phase 3 of the Audio Mixer architecture. Added flexible routing with 8 Aux/Send buses, Pre/Post-fader sends, and standard mixer controls (Pan, Mute, Solo-In-Place).
*   **Channel Strip (v2.3.56):** 🎉 **COMPLETE!** Implemented Phase 2 of the Audio Mixer architecture. Every track now features a professional Channel Strip with 4-Band EQ and Dynamics (Compressor/Limiter/Gate/Sidechain).
*   **Audio Mixer Foundation (v2.3.55):** 🎉 **COMPLETE!** Implemented Phase 0 and 1 of the new Audio Mixer architecture, including device enumeration, mixer lifecycle, and routing.
*   **Critical Stability (v2.3.54):** 🎉 **COMPLETE!** Addressed critical MDI batching and resource cleanup issues in the OpenGL backend.
*   **Virtual Bindless (v2.3.52):** 🎉 **COMPLETE!** Implemented a "Virtual Bindless" fallback system for OpenGL hardware lacking `GL_ARB_bindless_texture`. This system emulates bindless texture access by managing a virtual pool of texture units, allowing users to write unified bindless shader code that works across a wider range of hardware (including older Intel iGPUs).
*   **MDI Auto-Batching (v2.3.51):** 🎉 **COMPLETE!** Implemented Multi-Draw Indirect (MDI) auto-batching for the OpenGL backend. This optimization intelligently batches consecutive `SIT_OP_DRAW_MESH` commands sharing the same VAO into a single `glMultiDrawElementsIndirect` call, drastically reducing CPU overhead for repetitive geometry.
*   **Fence-Guarded Destruction (v2.3.50):** 🎉 **COMPLETE!** Implemented robust deferred destruction for OpenGL using GL_ARB_sync fences. This eliminates CPU stalls and ensures resources are only destroyed when the GPU is finished with them, matching Vulkan's safety and performance.
*   **Async Shader Linking (v2.3.49):** 🎉 **COMPLETE!** Implemented non-blocking shader linking for OpenGL hot-reloading using `KHR_parallel_shader_compile`.
*   **Vulkan Bindless (v2.3.45):** 🎉 **COMPLETE!** Implemented "Bindless" texturing for Vulkan using Descriptor Indexing. Textures are now accessed via a global unbounded array (`global_textures[]`) indexed by push constants, eliminating descriptor binding overhead and solving pool fragmentation.
*   **Vulkan Optimization (v2.3.44):** 🎉 **COMPLETE!** Added configurable staging buffer sizes and optimized I/O polling for hot-reloading to support a wider range of hardware targets.
*   **System Unification (v2.3.43):** 🎉 **COMPLETE!** Implemented the Universal Handle Architecture (v2.4 Milestone). All resources (Textures, Sounds, Shaders, Meshes) now use O(1) generational handles backed by fixed registries, eliminating legacy linked lists and enabling unified hot-reloading. See `REGRESSION_ANALYSIS.md` for details.
*   **Audio Capture Enhancements (v2.3.42):** 🎉 **COMPLETE!** Added `SituationStartAudioCaptureEx` for custom formats and updated the default capture to use native device settings (0, 0) for optimal performance.
*   **Flexible Texture Formats (v2.3.41):** 🎉 **COMPLETE!** Added `SituationColorEncoding` enum for automatic format selection. Storage images now use LINEAR format (UNORM) while sampled textures use SRGB for proper gamma correction. Works identically on OpenGL and Vulkan.
*   **Asset Pipeline (v2.3.38):** Added `SituationLoadBitmapFontFromMemory` and enhanced I/O thread controls for smoother background loading.
*   **OpenGL Optimization (v2.3.36):** Completed the "Max Out Core" plan with MDI batching, Zero-Copy Ring Buffers, and Bindless Textures.
*   **Texture Registry (v2.3.31):** Implemented a generational handle system for textures, enabling safe hot-reloading and O(1) validation.
*   **Universal Handles (v2.4):** 🎉 **COMPLETE!** All resources (Buffers, Shaders, Meshes) are now managed via the Registry System for uniform, bindless-ready access.
