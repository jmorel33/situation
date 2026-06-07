<div align="center">
  <img src="situation_blackMetal_logo.jpg" alt="Situation logo">
</div>

# What's New in Situation API

_For Core API library v2.4.217 — see **`doc/UPDATELOG.md`** for full patch notes per release._

### v2.4.217 — Odin echo+delay demo, test results log

*   **Odin example upgraded (v2.4.217):** `hello_situation` now chains `ToneSynth -> Echo -> Reverb`. New controls: `]`/`[` for delay wet, `P`/`O` for delay feedback (capped at 0.95). HUD shows reverb %, delay %, and feedback % on a second line.
*   **Test results log (v2.4.217):** `run_tests.bat` now tees output to `build/tests/results/YYYYMMDD_HHMM_backend.txt` via PowerShell `Tee-Object`. Every run is persisted for later consultation. Results folder created automatically.
*   **Language bindings relocated (v2.4.217):** Odin compiler (`dist/`) moved to `_languages/odin/dist/`. Generated bindings remain in `wrappers/Odin/`. `build_odin_example.bat` updated accordingly.

### v2.4.216 — Window state flag cache (O(1) VSync query)

*   **`SituationGetCurrentActualWindowStateFlags` cached (v2.4.216):** Was querying 7+ GLFW attributes on every call — caused per-frame stalls when called more than once per frame (e.g. HUD + toggle). Now O(1): refreshed once per frame inside `SituationPollInputEvents` after `glfwPollEvents()`, and immediately recomputed by `SituationSetWindowState`/`SituationClearWindowState`. New internal `_SituationComputeWindowStateFlags()` holds the actual GLFW query logic.
*   **Cache field (v2.4.216):** `sit_gs.cached_window_state_flags` added to the global state struct in `situation_impl_decl.h`.

### v2.4.215 — DestroyGraph audio race fix

*   **`SituationDestroyGraph` double-wait (v2.4.215):** The previous single-wait pattern had a TOCTOU window — the audio thread could start a new callback tick between the null store and the wait, reading the graph pointer one last time while DestroyGraph was already freeing it (crash: `W32/0xC0000005` null dereference). Fix: idle-wait *before* nulling `active_graph`, null it, idle-wait *again*. Exactly two waits, no race window.
*   **Odin example hardened (v2.4.215):** `SituationSetupVirtualMidiLoopback` return value now checked. If PortMidi is unavailable the example continues without MIDI rather than crashing on `SituationDestroyGraph`.

### v2.4.214 — Static build system, self-contained exes, async shader UAF fix

*   **Static library builds (v2.4.214):** `build_situation.bat static-opengl` / `static-vulkan` produces `build/dll/situation_*.a`. Link examples or the test harness against it for a self-contained exe with no DLL dependency at runtime — run from anywhere without copies or PATH tricks.
*   **Overhauled build model (v2.4.214):** Examples and the test harness no longer recompile the full library. All `#define SITUATION_IMPLEMENTATION` removed from ~63 example files. Build modes: `opengl`/`vulkan` (DLL-linked, fast) and `static-opengl`/`static-vulkan` (self-contained). `build_tests.bat` with no args now shows usage instead of silently defaulting.
*   **Test harness reorganized (v2.4.214):** Exes renamed `sit_test_opengl.exe` / `sit_test_vulkan.exe`, output to `build/tests/`. `run_tests.bat` is the DLL-mode launcher. Static builds run directly.
*   **Build output layout (v2.4.214):** `build/dll/` — library artifacts, `build/tests/` — harness exes, `build/examples/` — example exes.
*   **Async shader UAF fix (v2.4.214):** `_SituationVulkanFreeAsyncShaderLoad` spin-bailout no longer frees `ctx` while the worker may still be alive. Abandoned sentinel (`compile_done = -2`) transfers ownership to the worker, which self-frees via `atomic_compare_exchange_strong`. Fixes `graphics.sync_shader_after_async_cycle` corruption (observed as `async_fragment: error: '☺' : unexpected token`).
*   **`SituationTopologicalSort` now public (v2.4.214):** Added to `situation_api.h`. Was internal-only; explicit calls are redundant since `CreateNode`/`DestroyNode`/`CreatePatch`/`RemovePatch` all sort on the main thread, but the function is now exported for advanced use cases.

### v2.4.213 — SPIR-V UBO+SSBO+Sampler layout + Demon Hunt materials

*   **`SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER` (v2.4.213):** New Vulkan pipeline layout — UBO at set 0, SSBO at set 1, combined image sampler at set 2, 128B push constants. Enables shaders that need both structured data and a texture in a single pass.
*   **Demon Hunt Phase 2 materials (v2.4.213):** 7 material types (Stone, Wood, Metal, Rusted Metal, Bone, Flesh, Emissive) with per-wall shading variety. Material map packed as 4-bit bitfields in the SSBO. Sky shader uses `DH_ENABLE_MATERIALS = 1`.

### v2.4.212 — Chorus/echo stability, graph output staging

*   **Chorus runaway fix (v2.4.212):** 4-stage chorus stage sums normalized (×0.25), outputs soft-limited to ±2.0. Echo delay tap also soft-limited.
*   **Node graph master bus (v2.4.212):** Output port buffers cleared each block; master bus sums only port 0 (matches device wrapper convention).

### v2.4.211 — GLTF model loader

*   **`SituationLoadModel` / `SituationDrawModel` (v2.4.211):** glTF 2.0 load, draw, save, and unload. `test_model_loader` harness module (5 tests, BoomBox reference asset).

### v2.4.210 — Configurable screenshot format

*   **`SituationSetScreenshotFormat()` (v2.4.210):** BMP (default), PNG, JPG, TGA. `SituationTakeScreenshot()` accepts a base name or NULL for auto-naming.

### v2.4.208 — Audio preload sample rate fix, async shader shutdown

*   **Audio preload sample rate fix (v2.4.208):** `SituationLoadSoundFromFile` with `SITUATION_AUDIO_LOAD_FULL` now resamples to the device output rate. High sample-rate files (e.g. 96kHz WAV) no longer play at half speed on 48kHz devices.
*   **Async shader shutdown hardening (v2.4.208):** `_SituationVulkanFreeAsyncShaderLoad` no longer hangs if the thread pool is already destroyed during `SituationShutdown`.

### v2.4.207 — Split device info queries

*   **Split queries (v2.4.207):** `SituationGetCPUInfo`, `SituationGetGPUInfo`, `SituationGetMemoryInfo`, plus storage/network/input enumeration. `SituationGetDeviceInfo()` deprecated but still works.

### v2.4.205 — Compute Virtual Displays

*   **`SITUATION_VD_FLAG_COMPUTE_TARGET` (v2.4.205):** VDs writable by compute shaders. `SituationGetVirtualDisplayTexture()` exposes the internal texture for compute binding.

### v2.4.203 — Error propagation Phase 3 (breaking)

*   **⚠️ Breaking migration (v2.4.203):** 18 public API functions changed from `bool`/`void` to `SituationError`. Callers checking `if (!fn())` must change to `if (fn() != SITUATION_SUCCESS)`.

### v2.4.197 — Threading module dispatch + thread naming

*   **Worker core spread (v2.4.197):** `SITUATION_WORKER_NUMA_SPREAD_DEFAULT` build flag (defaults ON when `SITUATION_ENABLE_THREADING` defined). On single-NUMA systems, workers are pinned to distinct physical cores via `SituationBuildPhysicalCoreMask`. On multi-NUMA, spread across nodes. Override to `0` before including headers to disable.
*   **Thread naming (v2.4.197):** All library threads named at the OS level — visible in debuggers, Task Manager, Process Explorer: `Sit Main`, `Sit Worker 0`–`N`, `Sit I/O`, `Sit Render`, `Sit Audio`. Uses `SetThreadDescription` (Win10+), `pthread_setname_np` (Linux/macOS).
*   **Snapshot shows all roles (v2.4.197):** `SituationGetThreadPoolSnapshot` / `SituationDumpThreadPoolStatus` now includes the main thread and prints each slot's name (e.g. `[Sit Worker 3]`, `[Sit Audio]`).
*   **`SituationGetInternalThreadPool()` (v2.4.197):** New public API — returns the library's active pool pointer for diagnostics/tests.
*   **Render thread shutdown fix (v2.4.197):** `cnd_wait` → `cnd_timedwait` (50ms) in render loop; `_SituationDestroyRenderThread` broadcasts repeatedly. Fixes hang when no frames were submitted before shutdown.
*   **`SITUATION_ENABLE_RENDER_THREAD` in default builds (v2.4.197):** Both OpenGL and Vulkan DLL builds now include the render thread. Set `render_thread_count = 1` in `SituationInitInfo` to activate.

### v2.4.194 — Window init visibility, harness headless, Vulkan YPQ fix

*   **Startup window on top (v2.4.194):** **`_SituationInitWindow`** ORs **`SITUATION_FLAG_WINDOW_TOPMOST`** into the active profile so new apps are not buried behind existing windows at launch. Clear later with **`SituationClearWindowState(SITUATION_FLAG_WINDOW_TOPMOST)`**.
*   **GLFW visible hint leak fix (v2.4.194):** **`GLFW_VISIBLE`** is set explicitly from **`SITUATION_FLAG_WINDOW_HIDDEN`** at window creation; OpenGL loader window restores **`GLFW_VISIBLE TRUE`** after its hidden helper window. Fixes full harness runs where **graphics/audio** modules after **`core`** opened invisible (stale hint from loader window).
*   **Harness headless mode (v2.4.194):** **`--headless`** and **`SIT_TEST_HEADLESS=1`** — hidden window, minimal frame pump, no listen overlay; same path OpenGL and Vulkan. **`advanced`** module skipped (**`requires_visible_window`**). See **`tests/harness/sit_test_headless.h`**, **`sit_test_window.h`**.
*   **Vulkan YPQ grade compile fix (v2.4.194):** **`SituationCmdDrawTextureYpqGrade`** uses **`glm_vec4_one(push_data.color)`** ( **`vec4`** is a C array — no assignment).

### v2.4.193 — YPQ GPU grade + core optimizations (M4 WIP)

*   **YPQ CPU core (v2.4.193):** FMA and inverse-scale optimizations in **`situation_impl_ypq.h`**.
*   **GPU YPQ grade (v2.4.193):** public **`SituationCmdDrawTextureYpqGrade`**; harness **`graphics.ypq_grade_pass_cpu_parity`**.
*   **Harness (v2.4.193):** faster YPQ sweep/stats; **`build_situation.bat`** release **`SIT_OPTIMIZE_CFLAGS`**.

### v2.4.192 — YPQ color toolset M1+M2

*   **YPQ pixel API (v2.4.192):** **`ColorYPQf`**, **`SituationImageAdjustYPQ`**, float edit path, in-gamut chroma clamp; shared NTSC YIQ math in **`situation_impl_ypq.h`**. Harness **`misc`** YPQ tests.

### v2.4.191 — Vulkan async shader poll, harness multi-monitor VD, Phase 11-bis plan

*   **Vulkan async GLSL load (v2.4.191):** **`SituationBeginLoadShaderFromMemory`** submit hardening (**`POINTER_ONLY`**, **`RUN_IF_FULL`**, sync fallback); acquire polls **`vk_async_load`** slots only — fixes intermittent **`graphics.async_shader_*`** timeouts after long harness runs. See **`situation_impl_renderer.h`**.
*   **Audio output monitor (v2.4.191):** **`SituationSetAudioOutputMonitor`** callback pointer is atomic — safe read from the audio thread while harness installs/uninstalls the scope tap.
*   **Harness sleeps render (v2.4.191):** **`sit_test_harness_wait_ms`** and **`SIT_TONE_CAPTURE_SLEEP_MS`** pump scope/spectrum during waits — capture/listen tests no longer freeze the overlay for multi-second **`Sleep`** loops. **`tests/harness/sit_test_stereo_scope.c`**, **`sit_test_listen_overlay.h`**.
*   **Unified harness window (v2.4.191):** default **1024×768** via **`tests/harness/sit_test_window.h`** — shared init helpers and layout; graphics/VD/audio listen modules aligned. Harness-only.
*   **Harness module order (v2.4.191):** **`graphics`** → **`virtual_display`** → **`compute`** → **`transfer`** before **`audio`** / **`tone_synth`** so listen tests can render scope/spectrum on the main swapchain.
*   **`advanced` harness module (v2.4.191):** **`all_displays_windowed_fullscreen_cycle`** — simultaneous multi-monitor content through **one** host window + **one Virtual Display per monitor**, standard command-buffer frame loop; three phases (windowed panels → fullscreen layout → restore). Tier B (N OS windows) → v2.5. See **`doc/plan/renderer_bolster_plan.md`** Phase 11-bis.
*   **OpenGL DLL build fix (v2.4.191):** **`build_situation.bat`** OpenGL path — fixed `^` line continuations (same class as Vulkan **v2.4.170** fix).
*   **Phase 11-bis plan (v2.4.191):** convention-compliant multi-monitor + multi-VD design, consent gate, banned rogue presentation patterns.
*   **Verification (v2.4.191):** OpenGL **440/440**, Vulkan **430/430** full harness.

### Harness scope perf + tone synth tests (v2.4.190)

*   **Stereo scope overlay (v2.4.190):** throttled spectrum analysis, fewer line-segment quads per frame, atomic audio ring, Win32 message pump during waits — mitigates intermittent window/scope freezes during listen tests. Harness-only; **`tests/harness/sit_test_stereo_scope.c`**.
*   **Tone synth harness (v2.4.190):** **`legacy_midi_note_frequency`** and **`midi_complex_melody`** capture fixes; Vulkan full suite **429/429**.

### Tone synth sub osc + Vulkan 2D parity (v2.4.189)

*   **Tone synth sub oscillator (v2.4.189):** **`sub_coarse`** (CC111, ±12 st from main), **`sub_sync`** (CC112, main→sub hard sync), **`sub_ring_mod`** (CC113, CS-40M-style ring bus `main + level×main×sub`). See **`doc/tone_synth.md`**.
*   **Vulkan 2D matches OpenGL (v2.4.189):** one viewport adjustment (**`_SitVulkanFillViewport2DOpenGLParity`**) for internal 2D draws; same ortho, text, and textured-quad shaders as OpenGL — removed Vulkan-only Y/UV flips. Vulkan **`graphics` 89/89**, **`virtual_display` 21/21**.
*   **Harness listen overlays (v2.4.189):** stereo scope + log-spaced spectrum on tone/audio listen tests — render-pixel layout, fixed quad rects, AC-coupled zero-centered L/R traces. Not library API; lives under **`tests/harness/`**.

### Phase 11 foundation — render-pass helpers and docs (v2.4.188)

*   **Render-pass foundation (v2.4.188):** expanded **`SituationRenderPassInfo`** load/store/clear documentation; inline helpers **`SituationRenderPassInfoDefault`** / **`SituationRenderPassInfoLoad`**; public **`SituationRenderPassConfigurationKey`** for Vulkan render-pass caching (stencil load/store in key; MSAA deferred). Harness: **`core.render_pass_*`**. See **`doc/plan/renderer_bolster_plan.md`** Phase 11.

### Phase 6 closure (v2.4.187) — raster state complete

*   **Phase 6 closed (v2.4.187):** fixed-function raster commands done on GL/VK (**434/434**, **424/424**). **`SituationCmdSetMultisampleState`** explicitly **out of scope** — lands with **v2.5 render-target Phase 6** (MSAA surfaces + resolve), not a separate bolster phase. See **`renderer_bolster_plan.md` § MSAA / multisample**.

### Vulkan Phase 6-bisH — color mask, stencil, push/pop (v2.4.186)

*   **Vulkan Phase 6B parity (v2.4.186):** **`SituationCmdSetColorWriteMask`**, **`SituationCmdSetStencilTest`**, and **`SituationCmdPushRasterState`** / **`PopRasterState`** on Vulkan via **`VK_EXT_extended_dynamic_state3`** + tracked raster dynamics (color write, stencil op/ref/mask, depth, line width, bias, cull/front-face variant rebind). See **`doc/plan/renderer_bolster_plan.md`** **6-bisH**.

### Renderer bolster Phase 6B — line width, color mask, stencil, push/pop (v2.4.185)

*   **Raster state Phase 6B (v2.4.185):** **`SituationCmdSetLineWidth`**, **`SituationCmdSetColorWriteMask`**, **`SituationCmdSetStencilTest`** (+ **`SituationStencilState`** types); **`SituationCmdSetMultisampleState`** deferred. OpenGL push/pop raster stack now captures/restores fixed-function state at execute time. Harness: **`color_write_mask_blocks_red`**, **`push_pop_raster_color_mask`**, **`stencil_test_command_conditional`**, **`line_width_command`**. See **`doc/plan/renderer_bolster_plan.md`** Phase 6B.

### Renderer bolster Phase 8 — indexed viewport/scissor (v2.4.184)

*   **Indexed viewport/scissor API (v2.4.184):** 🎉 **COMPLETE (first slice)!** **`SituationCmdSetViewportIndexed`** / **`SituationCmdSetScissorIndexed`**; **`SituationGraphicsCaps.max_viewports`**; legacy **`SetViewport`** / **`SetScissor`** remain index-0 wrappers. OpenGL uses **`glViewportIndexedf`** / **`glScissorIndexed`** for index **> 0**. Harness: **`core.viewport_index_zero_parity`**, **`core.viewport_index_out_of_range`**. See **`doc/plan/renderer_bolster_plan.md`** Phase 8.

### OpenGL executor foundation + internal hardening (v2.4.178–v2.4.183)

*   **OpenGL executor harness green (v2.4.183):** 🎉 **COMPLETE!** Closes the **v2.4.182** OpenGL snapshot (**423/427** → **428/428**). Baseline raster reset on acquire/execute; indexed draw + pipeline/VAO hygiene; polygon mode / depth bias / textured quad execute fixes; **`glFinish`** before pre-swap screenshot (fixes flaky **`texture_format_preservation`** / **`screen_readback_corner_layout`**); NEAREST for non-mipmap textures; single readback flip on load. Harness **`module_order_point_then_polygon`** regression. See **`doc/UPDATELOG.md`** and **`doc/plan/renderer_bolster_plan.md`** Phase 7-ter.
*   **Audio graph RT teardown (v2.4.182):** 🎉 **COMPLETE!** Internal hardening **Phase 15:** **`is_in_audio_callback`** + **`_SituationWaitUntilAudioCallbackIdle()`** before **`SituationDestroyGraph`** — fixes intermittent use-after-free in MIDI/tone-synth harness on Windows. See **`doc/plan/INTERNAL_HARDENING_PLAN.md`** Phase 15.
*   **OpenGL deferred execute pass fix (v2.4.181):** 🎉 **COMPLETE!** **`exec_inside_render_pass`** tracked from packet stream at execute time (not **`recording_render_pass_active`** cleared at record). Fixes **`NO_RENDER_PASS_ACTIVE`** on **`SIT_OP_DRAW_QUAD`** / text / texture after **`SituationCmdEndRenderPass`**. See **`doc/UPDATELOG.md`** v2.4.181.
*   **OpenGL quad/text draw hardening (v2.4.180):** 🛠️ **COMPLETE!** Internal hardening **Phase 13:** **`_SituationGLValidateInternalQuadDrawReady`** / **`TextDrawReady`** on record + execute paths. See **`doc/plan/INTERNAL_HARDENING_PLAN.md`**.
*   **Vulkan quad/text draw hardening (v2.4.179):** 🛠️ **COMPLETE!** Internal hardening **Phase 12:** **`_SitVulkanValidateInternalQuadDrawReady`** / **`TextDrawReady`**. See **`doc/plan/INTERNAL_HARDENING_PLAN.md`**.
*   **Vulkan draw hygiene hardening (v2.4.178):** 🛠️ **COMPLETE!** Internal hardening **Phase 11:** **`_SitVulkanEnsureGraphicsPipelineBound`**, **`_SitVulkanFillOrthoProjection2D`** return **`SituationError`**; draw paths propagate errors. See **`doc/plan/INTERNAL_HARDENING_PLAN.md`**.

### Renderer bolster sprint (v2.4.147–v2.4.177)
*   **Vulkan 2D projection cleanup — Phase 7-bis (v2.4.177):** 🎉 **COMPLETE!** Replaces per-draw CPU UV/text mirrors with one convention: **`_SitVulkanFillOrthoProjection2D`** (`glm_ortho(0, w, h, 0, …)` only — **not** `projection[1][1] *= -1`) plus Vulkan quad/text **shader** V flip and text **`target_h`** push constant. Removed **`DrawTexture`** UV flip and text transform helpers. Full Vulkan harness **417/417** (83/83 **`graphics`**, 21/21 **`virtual_display`**) on reference GTX 1070 — see **`doc/UPDATELOG.md`** and **`doc/plan/renderer_bolster_plan.md`** Phase 7-bis.
*   **Vulkan text and metrics overlay (v2.4.176):** 🎉 **COMPLETE** (superseded for orientation debt by **v2.4.177**). Interim fix: CPU text Y/V transforms + **`DrawTexture`** UV flip so live-window HUD and **`draw_metrics_overlay`** passed before Phase 7-bis — see **`doc/UPDATELOG.md`** v2.4.176.
*   **Vulkan raster and compute interop (v2.4.175):** 🛠️ **PARTIAL.** Viewport/scissor on tracked user draws (**`_SitVulkanApplyGraphicsViewportScissor`**); storage-only textures get **`single_sampler_descriptor_set`** for **`SituationCmdDrawTexture`** (fixes **`compute_image_write`** **`-500`**); text bind order + grid-font UV/alpha fixes; harness indirect-compute VBO and line-list readback tweaks. Vulkan **`graphics`**: **81/83** (was **78/83**); **`virtual_display`**: **21/21**. Superseded for metrics overlay by **v2.4.176** — see **`doc/UPDATELOG.md`**.
*   **Vulkan VD compositor fix (v2.4.174):** 🎉 **COMPLETE!** **`_SitVulkanApplyVDCompositingDynamicState`** sets **`TRIANGLE_STRIP`** + depth-off before each VD composite draw — fixes diagonal half-frame split after user mesh draws left **`TRIANGLE_LIST`** dynamic state. Path B blend routing/pipelines, Path A resume-pass hygiene, **`virtual_display`** harness module (**21** tests, after **`graphics`**). Vulkan **`virtual_display`**: **21/21** — see **`doc/UPDATELOG.md`**.
*   **Graphics backend query API (v2.4.173):** 🎉 **COMPLETE!** **`SituationGetGraphicsBackend()`** / **`SituationGetGraphicsBackendName()`** report which renderer DLL is active (**OpenGL 4.6** vs **Vulkan 1.4** target) — callable before **`SituationInit`**. **`SituationGetGraphicsCaps`** adds **`backend`** + **`device_api_version_packed`** (driver/context version); **`api_version_packed`** now reports Situation target API (fixes stale Vulkan **1.2** placeholder). Harness **`core.get_graphics_backend`**, **`core.get_graphics_caps`** — see **`doc/UPDATELOG.md`**.
*   **Vulkan extended dynamic state proc fix (v2.4.172):** 🎉 **COMPLETE** (user-shader bind / async draw crash). **`vkCmdSetDepthTestEnable`**, **`vkCmdSetDepthWriteEnable`**, **`vkCmdSetDepthCompareOp`**, and **`vkCmdSetPrimitiveTopology`** are resolved with **`vkGetDeviceProcAddr`** instead of broken **`vulkan-1.dll`** IAT imports — fixes **SIGSEGV** in **`SituationCmdBindPipeline`** / **`async_shader_load_memory_draw`** / **`draw_pipeline_basic`**. Per-bind **`vkCmdSetPolygonModeEXT`** removed (static fill/line pipeline variants); dynamic polygon mode only for **POINT**. Vulkan **`graphics`**: **92/12**; all **`async_shader`** filters green — see **`doc/UPDATELOG.md`**.
*   **Vulkan quad per-texture sampler fix (v2.4.171):** 🎉 **COMPLETE** (internal quad path). `SituationCmdDrawTexture` binds each texture’s **`single_sampler_descriptor_set`** and samples **`u_QuadTexture`** instead of broken bindless `global_textures[]` indexing — fixes **all-black** `screen_readback_corner_layout` / strict 2×2 quadrant readback. Vulkan **V-only `uv_rect` flip** aligns `SituationImage` top row with screen top; harness uses **320×240** `dest` and **`w/8` / `5w/8`** texel-center samples (not `3w/8`). Verified: **`screen_readback_corner_layout`**, **`texture_format_preservation`**, **`draw_textured_checkerboard`**, **`draw_quad_red`** on Vulkan — see **`doc/UPDATELOG.md`**.
*   **Vulkan quad draw dynamics and build fix (v2.4.170):** 🛠️ **PARTIAL** (superseded for readback by **v2.4.171**). `build_situation.bat` Vulkan path works again (caret continuation fix); quad draws set viewport/topology/depth dynamic state before `DrawTexture`/`DrawQuad`; harness texel-center sampling groundwork. **`draw_quad_red`** readback passes; textured readback was still black until **171** — see **`doc/UPDATELOG.md`**.
*   **Vulkan raster reset and readback parity (v2.4.169):** 🛠️ **PARTIAL.** Per-acquire raster reset clears most **order-dependent** harness failures (**89/15** Vulkan `graphics`, was **79/25**); Vulkan screenshot readback drops erroneous vertical flip; dynamic polygon/depth-bias apply on draw; VD resume pass without spurious clears; harness indirect-draw and readback fixes. Strict quadrant readback, wireframe, and some VD scaling tests still open — see **`doc/UPDATELOG.md`**.
*   **Vulkan VD render-pass and frame lifecycle (v2.4.168):** 🛠️ **PARTIAL.** Virtual Display compositing syncs `inside_render_pass` with recorded passes (fixes **`render_virtual_displays`** **-540** and the old fence-timeout cascade); **`SituationAcquireFrameCommandBuffer`** recovers from **Acquire** without **EndFrame**; harness alignment test no longer wedges the GPU. Lingering graphics pixel/readback and **`tone_synth`** issues documented in **`doc/UPDATELOG.md`** — not closed here.
*   **Phase 6B polygon mode and depth bias (v2.4.167):** 🎉 **IN TREE.** `SituationCmdSetPolygonMode`, `SituationCmdSetDepthBias`; OpenGL soft replay; Vulkan line-pipeline variants and dynamic depth bias when supported. Harness: `polygon_mode_line_wireframe`, `depth_bias_overlap`. Vulkan isolated filters green; full **`graphics`** module may still need per-acquire raster reset.
*   **Phase 6A GL point topology fix (v2.4.166):** 🛠️ **COMPLETE!** `GL_POINTS` (enum 0) no longer treated as “unset” and drawn as triangles. OpenGL **`primitive_topology`** harness **2/2**.
*   **Index type flexibility — Phase 7 slice (v2.4.165):** 🎉 **COMPLETE!** `SituationIndexType`, `SituationCmdBindIndexBufferEx` (UINT16/UINT32, aligned offsets); legacy bind stays UINT32. Harness **3/3** on GL and Vulkan.
*   **Vulkan raster parity closure — Phase 6-bis (v2.4.164):** 🎉 **COMPLETE!** Static cull/front-face pipeline variants; deterministic topology on rebind; `primitive_topology_point_list` harness; **`doc/RENDERER_BARRIER_COOKBOOK.md`**. Vulkan **`front_face`** + **`primitive_topology`** filters green.
*   **Vulkan raster variant fallback — Phase 6-bis (v2.4.163):** 🛠️ **COMPLETE!** (superseded by v2.4.164 verification.) Static raster variants across PBR/legacy/simple pipeline families; routed through draw/indexed/indirect/mesh paths.
*   **Primitive topology — Phase 6A (v2.4.162):** 🎉 **COMPLETE!** `SituationPrimitiveTopology`, `SituationCmdSetPrimitiveTopology`; line-list harness. Vulkan **`front_face`** parity still open until v2.4.164.
*   **Front face — Phase 6A (v2.4.161):** 🎉 **COMPLETE!** `SituationFrontFace`, `SituationCmdSetFrontFace`; `front_face_cull_interaction` harness (OpenGL green; Vulkan parity completed in v2.4.164).
*   **Indirect draw commands — Phase 5 (v2.4.160):** 🎉 **COMPLETE!** `SituationCmdDrawIndirect`, `SituationCmdDrawIndexedIndirect`; CPU-filled and validation harness (**4** new graphics tests). Multi-draw batching deferred.
*   **Transfer harness split — Phase 4.1 (v2.4.159):** 🧪 **COMPLETE!** Dedicated **`transfer`** module (**12** tests): barriers, `CopyBufferEx`, blit/copy, buffer↔texture; removed duplicates from **`graphics`**.
*   **Buffer ↔ texture copy (v2.4.158):** 🎉 **COMPLETE!** `SituationCmdCopyBufferToTexture`, `SituationCmdCopyTextureToBuffer`; caller-owned transfer layouts on Vulkan.
*   **Texture copy region (v2.4.157):** 🎉 **COMPLETE!** `SituationCmdCopyTexture` / `SituationTextureCopyRegion`; `glCopyImageSubData` / `vkCmdCopyImage`.
*   **Texture blit region (v2.4.156):** 🎉 **COMPLETE!** `SituationCmdBlitTexture`; nearest/linear filters; asymmetric and scaled readback tests.
*   **Texture barrier desc (v2.4.155):** 🎉 **COMPLETE!** `SituationCmdTextureBarrier`, `SituationTextureLayout`; strict no-hidden-transition contract for Phase 4B.
*   **Copy buffer Ex — Phase 4A (v2.4.154):** 🎉 **COMPLETE!** `SituationCmdCopyBufferEx` with independent src/dst offsets; legacy `SituationCmdCopyBuffer` preserved.
*   **Renderer checkpoint hygiene (v2.4.153):** 🧹 **COMPLETE!** Default-font/resource cleanup before leak detection; API docs aligned with clear/dispatch/barrier surface; **`renderer_bolster_plan.md`** checkpoint before Phase 4 transfer work.
*   **Buffer barrier desc (v2.4.152):** 🎉 **COMPLETE!** `SituationBufferBarrierDesc`, `SituationCmdBufferBarrier`; buffer memory barriers on GL/Vulkan.
*   **Global barrier Ex (v2.4.151):** 🎉 **COMPLETE!** `SituationCmdPipelineBarrierEx`; explicit src/dst stages and access masks.
*   **Compute dispatch indirect (v2.4.150):** 🎉 **COMPLETE!** `SituationCmdDispatchIndirect` from indirect buffers (compute queue).
*   **Compute dispatch Ex (v2.4.149):** 🎉 **COMPLETE!** `SituationCmdDispatchEx` with explicit workgroup counts (foundation for indirect follow-up).
*   **Render clear commands (v2.4.148):** 🎉 **COMPLETE!** `SituationCmdClear` / color/depth/stencil clears inside an active render pass (Vulkan `vkCmdClearAttachments`).
*   **Compute harness split pilot — Phase -1 (v2.4.147):** 🧪 **COMPLETE!** New **`compute`** module (**8** tests); pure compute retired from **`graphics`**; **`doc/plan/renderer_bolster_plan.md`** Phase -1 done.

### v2.4.1–v2.4.146 — Infrastructure, Harness & Subsystem Buildout

*   **FFmpeg video subsystem groundwork (post-v2.4.146 plan work):** 🎬 **PROVEN!** The video plan now has a working MSYS2 MinGW64 FFmpeg static-library build path: `build_ffmpeg.bat` / `build_ffmpeg.sh` produce LGPL-safe **`libavcodec.a`**, **`libavformat.a`**, **`libswscale.a`**, and **`libavutil.a`** under `ext/ffmpeg/build/lib/`, with NASM enabled for optimized x86 assembly. Situation is still unchanged and does **not** link FFmpeg unless the future detachable `SITUATION_ENABLE_VIDEO` module is wired in.
*   **Renderer/control cleanup (v2.4.146):** 🧹 **COMPLETE!** Repaired corrupted renderer punctuation in diagnostics/comments, including the metric contention log, and restored `situation_impl_ctrl.h` indentation with a formatting-only pass. OpenGL and Vulkan DLL builds were verified after the cleanup.
*   **Backend naming clarity for GL-only internals (v2.4.146 era):** 🧹 **COMPLETE!** Renamed generic-looking internal OpenGL declarations to explicit `_SituationGL...` / `SITUATION_GL...` names, including MDI command structs and virtual bindless texture state, so shared Virtual Display concepts are no longer confused with OpenGL-specific implementation details.
*   **Current harness posture (v2.4.186):** ✅ **GREEN!** Full OpenGL **`sit_test.exe`**: **434/434**; full Vulkan **`sit_test_vulkan.exe`**: **424/424** on reference GTX 1070 (rebuild both DLLs before comparing backends).
*   **Typed audio controls and harness stability (v2.4.145):** 🛠️ **COMPLETE!** `SituationSetControl` now clamps and coerces values by declared `SituationControlType`: `FLOAT` stays continuous, `INT` / `ENUM` round to integer slots, and `BOOL` normalizes to `0/1`. The `audio.control_sweep_all_devices` test now validates the real typed readback contract with detailed diagnostics instead of fuzzy one-size-fits-all float comparisons.
*   **Tone synth full-run hardening (v2.4.145 era):** 🎹 **COMPLETE!** Stabilized MIDI/tone-synth harness behavior in full module runs by freezing output-monitor capture before analysis and widening melody verification around callback timing drift. The noisy `midi_complex_melody` failure path now reports useful note/window diagnostics when timing or capture state regresses.
*   **Threading Bolstering (v2.4.139–v2.4.144):** 🎉 **COMPLETE!** Promoted the generational C11 thread pool into a topology-aware execution layer: CPU topology cache, affinity query/set APIs, physical-core and NUMA mask builders, configurable render/audio/main affinity, NUMA placement policy, per-worker/I/O/render/audio observability, queue-depth APIs, pool snapshots, scheduler metrics, dynamic high-queue scan depth, auto worker sizing from `SituationInitInfo`, and a consolidated **`doc/THREADING_BOLSTERING_API.md`** reference.
*   **All-core CPU stress validation (v2.4.144):** 🔥 **COMPLETE!** Added **`threading.cpu_stress_10s_taskmgr`**, a 10-second `SituationDispatchParallel` burn that taxes all logical processors, reports logical-CPU distribution and pool status, and can be correlated directly with Windows Task Manager / performance counters. Skip with **`SIT_SKIP_CPU_STRESS`** when you need fast CI.
*   **Threading docs and README architecture (v2.4.143+):** 📈 **COMPLETE!** Added dual-socket/NUMA manual validation notes, threading troubleshooting updates, and a full **Mermaid threading architecture diagram** in `README.md` showing initialization policy, high/low queues, backpressure, worker/I/O lanes, main-thread helping, waits/dependencies, metrics, and diagnostics.
*   **Internal error propagation audit (v2.4.131–v2.4.138):** 🛠️ **COMPLETE!** Swept internal callers and void-return paths so failure cases flow through `SituationError` consistently instead of disappearing behind logging-only helpers. This covered GL soft-buffer execution, uniform map, async shader, render-thread, audio, and final internal hardening stragglers, with documentation for the remaining intentional void-by-design paths.
*   **Errno table, tooling, and binding API polish (v2.4.124–v2.4.130):** 🧩 **COMPLETE!** Expanded the table-driven errno layer, hardened Vulkan swapchain/init propagation, added internal hardening tooling, and pushed more graphics API calls onto explicit `SituationError` returns. Vertex attribute binding, index-buffer binding, and vertex/index binding validation now report structured errors rather than relying on ambiguous side effects.
*   **Tone synth instrument expansion (v2.4.112–v2.4.123):** 🎹 **COMPLETE!** The graph tone synth grew into a real programmable instrument: mono/poly behavior, SVF filtering, pulse width, modulation LFOs, filter-envelope modulation, portamento, sub oscillator, sub sync/ring behavior, sum limiting, patch memory, and comparison harness readiness.
*   **Graph MIDI and audio verification push (v2.4.106–v2.4.111):** 🎧 **COMPLETE!** Brought graph tone synthesis to legacy parity and then full MIDI routing: named test MIDI devices, deterministic test routing, MIDI frequency verification, effect-heard tests, harness repairs, and a Windows shared-audio auto-start fix.
*   **Async shader and SPIR-V hardening wave (v2.4.96–v2.4.105):** ⚙️ **COMPLETE!** Added GL/VK async shader loading, SPIR-V load/error reporting, poll diagnostics, driver log capture, async SPIR-V load, thread-pool I/O queue fixes, image resize/error mutex fixes, and a Vulkan async GLSL worker-queue fix. This made shader compilation failures observable instead of silent or timing-dependent.
*   **SPIR-V layout and shader binding parity (v2.4.83–v2.4.95):** 🧪 **COMPLETE!** Closed a long series of GL/VK shader parity gaps: memory SPIR-V loading, Demon Hunt shader embedding, uniform-cache initialization, host GL context fixes, SSBO binding, UBO binding, host link synchronization, queue-spin removal, renderer header guards, harness block binding, Vulkan descriptor binding, layout profiles, and screen-text parity tests.
*   **Shader storage block binding foundation (v2.4.81–v2.4.82):** 🧱 **COMPLETE!** Added GL shader-storage block binding support and fixed unique SSBO binding assignment so compute and SPIR-V harness paths could share predictable layouts.
*   **Windowing, VSync, and demo polish (v2.4.77–v2.4.80):** 🪟 **COMPLETE!** Added VSync state query fixes, borderless fullscreen and monitor targeting, maximize callback behavior, opaque blend-state cleanup, and Demon Hunt polish so window transitions and demo presentation stayed deterministic.
*   **Audio graph routing and mixer correctness (v2.4.72–v2.4.76):** 🎚️ **COMPLETE!** Cleaned up the audio graph routing path used by Demon Hunt, eliminated the default graph test tone, fixed sound-source control index mapping, patched a stolen tone-pool slot routing leak, and addressed parallel mixer / left-ear click synchronization issues.
*   **Unified camera and projection pipeline (v2.4.71):** 📷 **COMPLETE!** Added the Phase 5 camera/projection work immediately after the raster-state command finalization line, keeping projection behavior unified across demos and render paths.
*   **API Expansion Phase 4 (v2.4.70):** 🎉 **COMPLETE!** Transitioned to explicit rasterization and rendering state control. Added soft-command support for depth, culling, and blending (`SituationCmdSetDepthTest`, etc.), implemented debug groups (`vkCmdBeginDebugUtilsLabelEXT` / `glPushDebugGroup`), and correctly sandboxed implicit geometry (Quads/Text) with `_SitGLBackupState` to avoid user state clobbering.
*   **API Expansion Phase 3 (v2.4.69):** 🎉 **COMPLETE!** Added unified helpers for uniform arrays and matrices (`SituationSetShaderUniform1fv`), implemented shader uniform validation, expanded logging interfaces, and unified backend capability querying.
*   **Readback Test Harness Parity (v2.4.68):** 🛠️ **COMPLETE!** Expanded the C-level test harness (`sit_test.exe`) to cover all Phase 1 and Phase 2 Readback APIs. Discovered and resolved an OpenGL command buffer append linkage error in the process.
*   **Vulkan Diagnostic Parity & Readbacks (v2.4.67):** 🛠️ **COMPLETE!** Added a full `4x4` synchronous staging diagnostic readback for `K-Term` in the Vulkan backend, resolving dormant Vulkan structural references.
*   **Phase 1 Async Buffer Readback (v2.4.66):** 🛠️ **COMPLETE!** Implemented `SituationCreateReadbackBuffer`, `SituationCmdCopyBuffer`, and `SituationReadBuffer`. Backend parity achieved between OpenGL and Vulkan with `SIT_OP_COPY_BUFFER` tracking in the OpenGL execution loop and persistent mapping. Full sequential `sit_test` passed for both backends.
*   **Demon Hunt skydome path (v2.4.65):** 🌌 **COMPLETE!** Repaired the OpenGL Demon Hunt skydome path with Hi-DPI viewport handling, depth clear behavior, uniform updates, and demo-path validation.
*   **OpenGL soft-buffer text over quads (v2.4.64):** 🛠️ **COMPLETE!** **`SIT_OP_DRAW_TEXT` / `SIT_OP_DRAW_TEXT_EX`** execution disables depth test and enables alpha blending around glyph draws so HUD and overlays are visible after quad geometry writes the same orthographic depth — full sequential **`sit_test`** (**OpenGL DLL**) **310/310** (see **`doc/UPDATELOG.md`**).
*   **Echo node graph dry/wet (v2.4.63):** 🛠️ **COMPLETE!** Delay line out-of-place mix; UI wet applied once (no **`w²`** tail). **`doc/UPDATELOG.md`**.
*   **OpenGL diagnostics & compute harness (v2.4.62):** 🛠️ **COMPLETE!** Default-font / text / VD init **`stdout`** lines are gated behind **`SITUATION_VERBOSE_DIAGNOSTICS`** (same knob as Vulkan extras). **`SIT_COMPUTE_LAYOUT_TWO_SSBOS`** assigns **`glShaderStorageBlockBinding`** for **`InBuffer` / `OutBuffer`** so **`compute_chained_dispatches`** matches Vulkan’s two-set SPIR-V layout — full sequential **`sit_test`** (**OpenGL DLL**) **310/310**.
*   **Vulkan VD compositing & graphics harness (v2.4.61):** 🎉 **COMPLETE!** Virtual-display compositing preserves the caller’s framebuffer (**resume render pass / LOAD**), screen-copy targets are recreated after swapchain rebuild, and Path A/B **`vkCmdPushConstants`** ranges match SPIR-V layouts. Vulkan **`sit_test --module graphics`** reaches **78/78** on the maintained harness.
*   **Vulkan VD advanced compositor (v2.4.60):** 🎉 **COMPLETE!** Three-set pipeline layout for Path A with correct destination sampler (**binding 5**). See **`doc/UPDATELOG.md`**.
*   **Vulkan VD, shutdown, screenshot, and audio pipeline stabilization (v2.4.43–v2.4.59):** 🛠️ **COMPLETE!** Filled the big Vulkan/audio stability stretch: responsive waits, screenshot readback and swapchain recreation, VD composite guards and non-alpha blend routing, user-shader descriptors, bindless/harness alignment, shutdown ordering/VMA cleanup, MIDI graph teardown, locked harness stderr, master-bus meter contracts, default-graph loaded voices, unload-vs-snapshot safety, and callback-pipeline documentation alignment.
*   **Vulkan test harness & backend hardening (v2.4.42 era):** 🛠️ **COMPLETE!** First sustained push: shaders, buffers, VD creation/compositing, pipeline layouts, screenshot readback — toward full Vulkan graphics parity (superseded by fixes through **v2.4.61**).
*   **Graphics regression sweep (v2.4.41 era):** 🎉 **COMPLETE!** Bulk clears for uniforms, textured draws, compute binding, buffer updates, GL re-init hygiene; per-module counts evolve — see **`doc/UPDATELOG.md`**.
*   **Virtual display, callback, re-init, and harness stabilization (v2.4.37–v2.4.40):** 🧹 **COMPLETE!** Cleaned up VD compositing, callback guard behavior, renderer re-initialization, and early harness stability after the node-graph takeover. These patches made repeated init/shutdown and graphics regression passes much less fragile.
*   **Node graph takeover — mixer removed (v2.4.36):** 🛠️ **COMPLETE!** Legacy mixer API removed; **`SituationAudioGraph`** + **`SituationProcessGraph`** are the routing path. miniaudio stays the device backend.
*   **Audio node graph — devices live (v2.4.35):** 🎉 **COMPLETE!** Device types registered and processed through the graph (**`SituationCreateNode`**).
*   **Audio coverage gap tests (v2.4.34):** 🧪 **COMPLETE!** Added the Phase 19–21 coverage-gap tests that followed the broader audio harness expansion, tightening regression coverage around graph/device behavior.
*   **Test harness expansion (v2.4.33):** 🎉 **COMPLETE!** Broad audio coverage: registry, graph lifecycle, effects, serialization, MIDI.
*   **Graphics, VD, compute, and descriptor tests (v2.4.29–v2.4.32):** 🧪 **COMPLETE!** Added rendering pipeline tests, deeper virtual-display tests, compute-shader roundtrip tests, and data-flow / descriptor-binding tests before the audio harness wave landed.
*   **Test harness complete (v2.4.28):** 🎉 **COMPLETE!** **CTest**-based runs, reporters, leak detection.
*   **Early harness bring-up and audio fixes (v2.4.25–v2.4.27):** 🧪 **COMPLETE!** Added initial graphics tests, audio tests, and follow-up audio bug fixes after the black-box harness framework came online.
*   **Test harness framework (v2.4.24):** 🛠️ **COMPLETE!** DLL-linked black-box **SITAPI** harness.
*   **Audio/text online and Vulkan init/runtime repairs (v2.4.21–v2.4.23):** 🧯 **COMPLETE!** Restored Vulkan initialization paths, fixed renderer runtime behavior, and brought audio/text paths online in the modularized implementation.
*   **Renderer robustness audit (v2.4.18–v2.4.20):** 🛠️ **COMPLETE!** GL/VK resource paths and frame lifecycle.
*   **Init and renderer init hardening (v2.4.16–v2.4.17):** 🛡️ **COMPLETE!** Hardened initialization and renderer setup paths after the modular split so startup failures became cleaner and easier to diagnose.
*   **Error propagation and housekeeping (v2.4.14–v2.4.15):** 🧹 **COMPLETE!** Followed the X-macro errno work with explicit error propagation phases and housekeeping around the freshly split modules.
*   **X-macro errno (v2.4.13):** 🛠️ **COMPLETE!** **`SituationError`** table-driven messages.
*   **Threading manicure (v2.4.11):** 🛠️ **COMPLETE!** Thread-pool hardening.
*   **Internal subsystem extractions (v2.4.5–v2.4.9):** 🛠️ **COMPLETE!** Modular **`sit/situation_impl_*.h`** split.
*   **Virtual display compositing & UBO performance (v2.4.3):** 🚀 **COMPLETE!** VD compositing fixes; less main-thread UBO stall.
*   **OpenGL deferred rendering architecture (v2.4.2):** 🛠️ **COMPLETE!** GL/Vulkan structural parity.
*   **Complete MIDI architecture (v2.4.1):** 🎹 **COMPLETE!** Routing, transforms, recording, UD inquiry.
*   **OpenGL graveyard flush safety (v2.4.1):** 🧹 **COMPLETE!** **`_SitGLFlushGraveyard`** waits on prior-frame **`GL_ARB_sync`** before cleanup.
*   **Modular revolution & universal handles (v2.4.0):** 🎉 **COMPLETE!** Monolithic impl split into **16** internal modules, with resources moving toward uniform O(1) generational registries for bindless-ready access. Major architectural reorganization establishing a professional folder structure (100% backward compatible): core headers relocated to `sit/` (root retains only `situation.h` as public entry point), audio effects organized into `sit/aud/fx/` (16 effects: reverb, echo, chorus, phaser, overdrive, exciter, maximizer, dynamics, filter, eq_4band, mastering_amp, deafmax, spring_reverb, studio_reverb, sst282, lfo), Polysonix synthesizer moved to `sit/aud/polysonix/`, K-Term terminal emulation in `sit/k-term/`. See `doc/V2_4_0_FOLDER_REORGANIZATION_COMPLETE.md` for full details.

---

## v2.3.x — Pre-Modular Foundation

These entries document the foundational work that led to the v2.4 modular architecture.

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
*   **Fence-Guarded Destruction (v2.3.50):** 🎉 **COMPLETE!** Implemented robust deferred destruction for OpenGL using `GL_ARB_sync` fences. This eliminates CPU stalls and ensures resources are only destroyed when the GPU is finished with them, matching Vulkan's safety and performance.
*   **Async Shader Linking (v2.3.49):** 🎉 **COMPLETE!** Implemented non-blocking shader linking for OpenGL hot-reloading using `KHR_parallel_shader_compile`.
*   **Vulkan Bindless (v2.3.45):** 🎉 **COMPLETE!** Implemented "Bindless" texturing for Vulkan using Descriptor Indexing. Textures are now accessed via a global unbounded array (`global_textures[]`) indexed by push constants, eliminating descriptor binding overhead and solving pool fragmentation.
*   **Vulkan Optimization (v2.3.44):** 🎉 **COMPLETE!** Added configurable staging buffer sizes and optimized I/O polling for hot-reloading to support a wider range of hardware targets.
*   **System Unification (v2.3.43):** 🎉 **COMPLETE!** Implemented the Universal Handle Architecture (v2.4 Milestone). All resources (Textures, Sounds, Shaders, Meshes) now use O(1) generational handles backed by fixed registries, eliminating legacy linked lists and enabling unified hot-reloading. See `REGRESSION_ANALYSIS.md` for details.
*   **Audio Capture Enhancements (v2.3.42):** 🎉 **COMPLETE!** Added `SituationStartAudioCaptureEx` for custom formats and updated the default capture to use native device settings (0, 0) for optimal performance.
*   **Flexible Texture Formats (v2.3.41):** 🎉 **COMPLETE!** Added `SituationColorEncoding` enum for automatic format selection. Storage images now use LINEAR format (UNORM) while sampled textures use SRGB for proper gamma correction. Works identically on OpenGL and Vulkan.
*   **Vulkan Text Rendering (v2.3.39):** 🛠️ **COMPLETE!** Fixed 11 critical bugs in the Vulkan text rendering pipeline.
*   **Asset Pipeline (v2.3.38):** 🛠️ **COMPLETE!** Added `SituationLoadBitmapFontFromMemory` and enhanced I/O thread controls for smoother background loading.
*   **OpenGL Optimization (v2.3.36):** 🎉 **COMPLETE!** Completed the "Max Out Core" plan with MDI batching, Zero-Copy Ring Buffers, and Bindless Textures.
*   **Texture Registry (v2.3.31):** 🎉 **COMPLETE!** Implemented a generational handle system for textures, enabling safe hot-reloading and O(1) validation.

