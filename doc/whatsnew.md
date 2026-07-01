<div align="center">
  <img src="situation_blackMetal_logo.jpg" alt="Situation logo">
</div>

# What's New in Situation API

_For Core API library v2.4.409 — see **`doc/UPDATELOG.md`** and **`doc/updatelog_24_05.md`** for full patch notes._

### v2.4.409 — K-Term display fix (Phase F)

*   **Font atlas GPU upload** — retry in `KTermCompositor_Prepare` when init-time upload fails before the loader GL context is ready (fixes missing glyphs with `KTERM_USE_SIT_GRID=1`).
*   **Unified terminal dispatch** — `KTERM_USE_SIT_GRID=1` uses the same `KTerm_UpdateBuffer` → `terminal.comp` path as legacy; only the SSBO owner differs (`SituationGridGetCellBuffer`).

### v2.4.408 — Grid Phase F (K-Term client path)

*   **`KTERM_USE_SIT_GRID=1`** is now the **default** — K-Term owns one `SituationGridSurface` instead of a duplicate terminal SSBO + pipeline.
*   Legacy **`terminal.comp`** path preserved behind **`#if !KTERM_USE_SIT_GRID`** — build with `KTERM_USE_SIT_GRID=0` for baseline comparison.
*   **`SituationGridDispatchPushConstants`** — K-Term bridge dispatch with full terminal push constants.
*   **Harness:** `kterm_console` green with default grid client path.

### v2.4.407 — Grid subsystem Phase E (collision probe)

*   **`SituationGridSetCollisionProbe`** / **`TestCollision`** — CPU AABB probe vs **`SIT_GRID_ROLE_COLLISION`** grid (`code != 0 && bg.a > 0`).
*   **Example 27** — **`g_collide_grid`** (walls + ground); actor bounce uses collision normals instead of hardcoded bounds.
*   **Harness:** `--module grid` **8/8** OpenGL (`collision_probe` added).
*   **Guide:** [`doc/guide/grid.md`](guide/grid.md) § Collision grid. GPU **`GRID_PASS_COLLIDE`** deferred to E.2.

### v2.4.406 — Grid subsystem Phase D (actor grid)

*   **`SituationGridClear`** / **`SituationGridBlitCells`** — bulk clear and rectangular cell stamp for actor layers (interim until Phase K sprites).
*   **Example 27** — dedicated **`g_actor_grid`** (z=1) between scrolling BG and UI labels; 1×2 entity repainted each frame.
*   **Harness:** `--module grid` **7/7** OpenGL (`actor_over_tiles`, `blit_and_clear` added).
*   **Guide:** [`doc/guide/grid.md`](guide/grid.md) § Actor grid. Plan: [`GRID_RENDER_PLAN.md`](plan/GRID_RENDER_PLAN.md) Phase D 🟡 (VK + kterm regression pending).

### v2.4.405 — Grid subsystem Phase C (stacked layers)

*   **`SituationGridStack*`** — stack up to 8 grids with matching topology; `StackAddGrid(stack, grid, z_order)`, **`StackPresent`** to compute-target VD.
*   **`SIT_GRID_PASS_BLEND`** — top layers composite over lower layers; transparent cells (`code == 0`, `bg.a == 0`) pass through.
*   **`SituationGridSetScroll`** — per-grid scroll (8.8 fixed in shader push constants); **`SituationGridSetRole`** — skip `SIT_GRID_ROLE_COLLISION` during blend.
*   **`grid_preamble.glslh`** — `#version 460` must be first line (fixes `-610` shader compile on OpenGL after disk load).
*   **Harness:** `--module grid` **5/5** OpenGL. Plan: [`GRID_RENDER_PLAN.md`](plan/GRID_RENDER_PLAN.md) Phase C in progress (example 27 + VK pending).

### v2.4.404 — Grid subsystem Phase B (cell layer)

*   **`SituationGrid*`** — first-party grid API in **`sit/situation_api_grid.h`**: create/destroy, `SetCell`/`UploadCells`, `SetFont`, `Dispatch`, **`Present`** to compute-target VD.
*   **`SitGridCell.code`** — public cell symbol field (console-facing; **not** `glyph`). K-Term bridge maps to `GPUCell.char_code`. `SIT_GRID_ATTR_*` flags + `attr0`/`attr1` color attrs documented.
*   **GPU shaders in `sit/gpu/` only** — **`grid_preamble.glslh`** + **`grid.comp`** loaded at runtime via `_SituationLoadCoreShaderFile`; removed embedded GLSL from **`situation_impl_grid.h`**.
*   **Harness:** `--module grid` **2/2** OpenGL (`cell_checkerboard`, `vd_present`). Plan: [`GRID_RENDER_PLAN.md`](plan/GRID_RENDER_PLAN.md) Phase B in progress.

### v2.4.403 — PE version stamping (no drift)

*   **`build/sit_version.mk`** + **`scripts/read_situation_version.py`** — single read path from **`sit/situation_base_version.h`** for windres `-DSIT_VERSION_*` (`--windres` / `--string` / `--make`).
*   **`sit/platform/windows/sit_app.rc`** — removed hardcoded **2.4.399** fallback; examples + harness pass version at compile time (same model as `situation_resource.rc`).
*   **`build/build_examples.bat`**, **`tests/harness/Makefile`**, **`sit/Makefile`** — all stamp PE resources from the canonical header.
*   **Docs** — `COMPILATION_GUIDE.md` § Application Identity; `architecture.md` recent-changes row; `scripts/README.md`; steering version-stamping notes.

### v2.4.402 — Tone synth patch memory fix

*   **`SituationSetControl`** — `patch_slot` (ctrl **37**) **recalls** even when the slot index is unchanged (e.g. recall slot **0** while already on slot **0**); `patch_store` (ctrl **38**) **saves** on every `SetControl` ≥ **0.5**, not only on value transitions.
*   **`sit/aud/tone_synth_graph.h`** — `PATCH_PARAM_FIRST` / `PATCH_PARAM_LAST` derive snapshot size; `_Static_assert` on `patch_slots[].param[]`.
*   **Docs** — [`tone_synth.md`](tone_synth.md) § Patch memory @ v2.4.402.
*   **Harness:** `tone_synth.patch_memory` green (GL + VK).

### v2.4.401 — Profiling file layout (discipline)

*   **`build/tracy_client.cpp`** — Tracy client single-TU moved out of `sit/` (build glue only; linked when `SIT_TRACY=1`).
*   **`sit/situation_profiling.h`** — `SIT_PROFILE_*` Tracy CPU zone macros; included from **`situation.h`**, not `situation_api.h` (instrumentation, not SITAPI).
*   **Removed** — `sit/tracy_client.cpp`, `sit/situation_impl_trace_prof.h`, `sit/situation_prof_macros.h` (superseded by above).
*   **Docs** — [`architecture.md`](architecture.md) § Profiling instrumentation layout; [`guide/profiling.md`](guide/profiling.md); steering tree; `COMPILATION_GUIDE.md` / SDK file hierarchy updated.

### v2.4.400 — Win32 identity WI-5 (plan complete)

*   **`SituationInitInfo::default_window_icon_path`** — optional PNG (stb) or Win32 `.ico`; applied at end of **`SituationInit`** via **`SituationSetWindowIcons`** (fail-soft on error).
*   **`sit/platform/windows/situation_win32_window_icon.h`** — multi-size `.ico` → RGBA for GLFW.
*   **Harness:** `--module identity_init` (2 tests); **application identity Phase I (Win32) complete** — see [`SIT_IDENTITY_PLAN`](plan/SIT_IDENTITY_PLAN.md) for WI-6+, Linux, macOS.

### v2.4.399 — Win32 executable identity (defaults + overrides)

*   **`SituationInitInfo::app_user_model_id`** — NULL → **`Situation.Application`** via `SetCurrentProcessExplicitAppUserModelID` before `glfwInit`.
*   **`SituationWin32SetAppUserModelId`** — pre-init author override (Win32).
*   **`sit_app.rc`** — EXE **`VS_VERSION_INFO`** + icon; **`sit_app_template.rc`** for author PE branding.
*   **`SIT_APP_RC` / `APP_RC=`** — override default RC in `build_examples.bat` and harness Makefile.
*   **Guide:** [`doc/guide/windows_app_identity.md`](guide/windows_app_identity.md). **Architecture:** [`doc/architecture.md`](architecture.md#application-identity-architecture-v24399). Plan: [`doc/plan/SIT_IDENTITY_PLAN.md`](plan/SIT_IDENTITY_PLAN.md) Phase I (WI-0–WI-5).
*   **Docs:** full sweep — `architecture.md`, `introduction.md`, `core.md`, `COMPILATION_GUIDE.md`, `situation_sdk.md` §2.4, steering rule #14, API index regen @ 399.
*   **Harness:** `--module window --filter win32` (2/2 GL).

### v2.4.398 — MSAA Phase 0 prep (VD-4b scaffolding)

*   **`SituationMultisampleQuality`** — unified attachment-quality tier enum (`OFF`, `2X`, `4X`, `8X`, `16X`) plus **`SituationMultisampleQualitySampleCount`**, **`FromSampleCount`**, **`Clamp`** helpers in **`situation_api_types_gpu.h`**.
*   **Internal only @ 398** — VD stores **`msaa_quality`** from **`desc.msaa_samples`**; **`> 1` still rejected** (no MSAA attachments / resolve yet). **`pending_gpu_rebuild`** field reserved for configure-time heavy rebuild.
*   **Vulkan prep** — **`_SituationVulkanCreateImage(..., samples)`**; VD pipeline variant key includes sample count; **`SituationCmdSetMultisampleState`** shadow fields bake into multisample pipeline state when samples > 1 (future VD-4b).
*   **Harness:** `multisample_quality_helpers` (graphics module).
*   **Plan:** [`doc/plan/renderer_bolster_plan.md`](plan/renderer_bolster_plan.md) § VD-4b Phase 0. End-to-end MSAA remains **v2.5 / VD-4b**-gated.

### v2.4.397 — P10.4 public query pool API

*   **`SituationCreateQueryPool` / `DestroyQueryPool`** — user pools for **`SITUATION_QUERY_TYPE_TIMESTAMP`** or **`SITUATION_QUERY_TYPE_OCCLUSION`** (up to 64 queries/pool).
*   **`SituationCmdResetQueryPool`**, **`SituationCmdWriteTimestamp`**, **`SituationCmdBeginOcclusionQuery`**, **`SituationCmdEndOcclusionQuery`**
*   **`SituationGetQueryPoolResults`** — non-blocking → **`SITUATION_ERROR_QUERY_RESULT_NOT_READY` (-566)**; **`SITUATION_QUERY_RESULT_WAIT_BIT`** for blocking readback.
*   **Harness:** `--module query_pool` (3/3 GL, 3/3 VK).

### v2.4.396 — P10.3 GPU timestamp zones

*   **`SituationCmdGPUZoneBegin` / `SituationCmdGPUZoneEnd`** — 16 fixed zone IDs (`SituationGPUProfileZone`); fills **`SituationGetFrameProfile` → `gpu_zone_ns[]`** (one frame late).
*   **`SIT_FEATURE_GPU_TIMESTAMPS`** — capability probe via `SituationIsFeatureSupported`.
*   **Internal zones:** VD composite, VD path A/B (GL), text batch (GL), VK composite pass (outside user render-pass zones).
*   **Harness:** `--module frame_profile` (4/4 GL, 4/4 VK).

### v2.4.395 — P10.2 Tracy CPU profiling (opt-in)

*   **`SIT_PROFILE_ZONE_SCOPED`** / **`SIT_PROFILE_ZONE_CTX`** — user-facing Tracy macros; no-ops when Tracy off.
*   Library zones: EndFrame frame mark, acquire backpressure/fence, render-thread frame, VK queue submit, poll/update, shader load poll, thread-pool jobs, audio callback.
*   **Build:** `SIT_TRACY=1` or `build_situation.bat opengl tracy`; Tracy client under `ext/tracy/public/`.

### v2.4.394 — P10.1 structured frame profile

*   **`SituationFrameProfile`** — versioned snapshot: frame time, max/spike counters, render-thread phase ns (backpressure, fence, execute, present), poll/update ns, latency avg/max, queue depth; `gpu_zone_ns[]` filled by P10.3 when supported.
*   **`SituationGetFrameProfile`** / **`SituationResetFrameProfileStats`** — non-allocating read + explicit spike/histogram reset.
*   **Harness** — `--module frame_profile` (2/2 GL+VK).

### v2.4.388–393 — Renderer behavior policy + render-target readback

*   **Phase 14 (388–391)** — `SituationCmdSet/Push/PopRendererBehavior` with strict defaults; opt-in blit filter downgrade, transfer usage fallback, assisted texture layout hints, validation WARN/COMPAT logging. Guide: [`guide/renderer_bolster.md`](guide/renderer_bolster.md) § Workflow 3b; cookbook: [`RENDERER_BARRIER_COOKBOOK.md`](misc/RENDERER_BARRIER_COOKBOOK.md).
*   **Phase 3b (392)** — `SituationCmdTextureBarrier` accepts `COLOR_ATTACHMENT` ↔ transfer layouts on transfer-capable color textures; VD end-pass keeps attachment layout until readback or composite. Harness `transfer.render_target_readback` (22/22 GL+VK).
*   **Phase 3c (393)** — `SituationRenderTarget` user offscreen targets: create/destroy, `BeginRenderPass` via `render_target` field, `SituationReadRenderTarget`. Harness `--module render_target` (3/3 GL+VK). MSAA RT deferred.
*   **Plan** — Phase 14 + 3b + 3c closed in `doc/plan/renderer_bolster_plan.md`.

### v2.4.387 — VD bolster resume (renderer_bolster_plan)

*   **VD-2 complete** — OpenGL `GL_SRGB8_ALPHA8` FBO when `SIT_VD_FORMAT_RGBA8_SRGB`; `GL_FRAMEBUFFER_SRGB` during VD passes; HDR-aware clear on main window only; harness `vd_default_clear_color`, `vd_srgb_format_composite`.
*   **VD-3** — `SituationSetVirtualDisplaySampler`; `SituationScalingMode` is layout-only (no longer changes filter).
*   **VD-4a** — `SituationSetVirtualDisplayMaxAnisotropy`, `SituationSetVirtualDisplayMipLevels` (sampler LOD clamp; storage mips at create via `SituationVirtualDisplayDesc.color_mip_levels`).
*   **VD-5** — `SituationSetVirtualDisplayUpdateMode` (static = frozen frame clock), `SituationSetVirtualDisplayMemoryHint`, `SituationSetVirtualDisplayClearColor`.
*   **Plan** — `doc/plan/renderer_bolster_plan.md` status refreshed (game-loop @ 385, Track D @ 367).

### v2.4.386 — Fractional display refresh Hz

*   **New getters** — `SituationGetMonitorRefreshRateHz`, `SituationGetDisplayRefreshRateHz`, `SituationGetMeasuredPresentRateHz`.
*   **59.94 Hz panels** — precise nominal rate stored internally; integer HUD getters unchanged; VSync FPS rounds to **60**.

### v2.4.384–385 — VSync-sharp game loop

*   **On-demand screen capture** — OpenGL captures only when requested; no per-frame readback on the hot path.
*   **Present-anchored timing** — `SituationGetFrameTime()` and FPS follow display pace, not main-thread jitter; **`SituationGetDisplayRefreshRate()`** for HUD.
*   **Paced pipeline** — VSync or target FPS caps queue depth at **2** frames (was 6); VSync-only pacing uses YIELD instead of SLEEP.
*   **Metrics overlay** — refresh, paced slots, present interval, capture state.
*   **Docs** — Frame Loop Contract in [`architecture.md`](architecture.md); debug loop-violation messages link to docs.
*   **Vulkan harness** — pattern tests **118/118** green.

### v2.4.380–383 — Tone synth polish & example 04

*   **Graph synth engine** — polyBLEP on saw, pulse, and triangle; hard-sync sub blep; exponential ADSR release tail; **`lfo_start_phase`** (CC117); **`glide_reset_on_key`** (CC118) for mono legato ADSR retrigger with glide preserved.
*   **Example 04** — virtual MIDI → tone synth → FX chain; tracker-style piano; numpad for synth/FX params; **travel editor** for all **42** controls; stereo **peak meters**; F2/F3 ±1 octave per press.
*   **Shared example audio** — `sit_example.h` helpers for examples 04, 06, 09, 19 (08 stays on legacy `PlayToneEx`).
*   **Fixes** — numpad vs piano key alias; FX wrapper control indices; window-state HUD after F11; no spurious 440 Hz from numpad edits.

### v2.4.371–379 — VD test-pattern / standby

*   **Idle snow** — new Virtual Displays default to animated B&W noise; calibration layers 0–8 remain opt-in.
*   **Chroma snow** — optional RGB idle noise (bit 16); **`SituationSetVirtualDisplayChromaSnow`** / **`Get`**.
*   **Calibration layers** — layer 8 is one lit cube; per-layer GPU params + stack compose (P10/P11).
*   **Unified config** — **`SitVdStandbyConfig`** in **`api_types_gpu.h`**; **`situation_impl_vd_standby.h`** + public **`SituationVdStandby*`** helpers.
*   **Example 25** — interactive VD pattern explorer (`25_vd_standby`); SMPTE **10%** safe-area defaults.
*   **Example HUD** — resolution-independent **`sit_example.h`** layout; **`build_examples.bat`** covers digestible set.

### v2.4.370 — Advanced font showcase; GL fullscreen canvas parity

*   **`advanced.font_capabilities_fullscreen_showcase` (new)** — ~30 s exclusive-fullscreen harness demo: 8 segments (typography, retro atlas, YPQ, motion, outline, rotation, layout, finale); readable showcase retro fixtures; thin YPQ scissor bands; honest outline/CPU-stamp labeling when Roboto missing or SDF unsafe.
*   **GPU text shader** — `text.frag` preserves atlas RGB (`texColor.rgb * tint`) so retro outlined fonts show real stroke + fill instead of a single tinted blob.
*   **OpenGL exclusive fullscreen** — fixed-canvas + stretch blit now matches Vulkan; removed fail-soft draw to the default framebuffer at native resolution when canvas FBO creation fails after `ToggleFullscreen()`.
*   **Plan** — `doc/plan/TEST_HARNESS_ADVANCED_FONT_SHOWCASE_PLAN.md`.

### v2.4.369 — Text & font harness certification (T0–T6)

The biggest font test sprint since v2.4.341: full GPU text certification plus a dedicated retro-builder module.

*   **`text_rendering` (25 tests)** — error paths first (no-atlas fallback, bad load, unload contract); default grid + boxed/multiline/colored draw; bitmap bake + sheet texture; TTF measure/draw; CPU stamp and lifecycle.
*   **`text_retro_builders` (14 tests, new module)** — seven retro families (CP437, terminal, ASCII, packed, outlined packed, VCR, VGA), each with *usable* (atlas + measure) then *display* (draw, tint, multiline, boxed, measure-vs-draw) stages.
*   **Library fix** — `SituationCreateVGA8x8Font` and `SituationCreateVCRFont` set white font color (was invisible alpha 0 on GPU).
*   **Plan** — `doc/plan/TEST_HARNESS_TEXT_FONT_PLAN.md` through phase T6; matrix L5–L11 green on OpenGL + Vulkan.

### v2.4.368 — Harness speed, master meter hardening

*   **Audio tests (~2× faster)** — halved MIDI and effects listen durations in the harness.
*   **Master bus meter** — clamps non-finite reads; cleared when audio is not ready or on shutdown.
*   **Shutdown log** — active GPU resources log as releasing, not “leaked”.
*   **Harness/build** — Track D VD streak test OpenGL-only at compile time; `build_tests all` / `all-static`.

### v2.4.364–367 — Vulkan shader cache Phase 6 & OpenGL `-600` fix

Vulkan graphics and shader reload got faster; OpenGL `EndFrame` no longer spuriously returns `-600` in consumer apps.

*   **Shader cache (364–366)** — Harness readback batching; redundant inline pipeline builds removed; Layer A cache hits and async reload skip redundant shaderc work. See [`VULKAN_SHADER_CACHE_PLAN.md`](plan/VULKAN_SHADER_CACHE_PLAN.md).
*   **OpenGL Track D (367)** — Scoped GL error drains at frame boundaries (screenshot readback, VD restore); fixes false `-600` the harness missed.

### v2.4.363 — Lua wrapper: self-contained embedded exe

*   **Track D (OPEN)** — Fresh `build/dll/situation_opengl.dll` can return render-thread **`-600`** (`SITUATION_ERROR_OPENGL_GENERAL`) on wrapper `hello_situation` frame 0 while `sit_test_opengl.exe` full suite stays green @ 362. **Do not embed fresh DLL** until [D-C1](plan/LIBRARY_RECOVERY_PLAN_244.md#track-d--opengl-render-thread-execute--600--wrapperharness-gap-library--open--v24363); use `situation_opengl_lua_embed.dll` (known-good copy). See [LIBRARY_RECOVERY_PLAN_244.md §Track D](plan/LIBRARY_RECOVERY_PLAN_244.md).
*   **`build/build_lua_example.bat`** — produces `build/examples/lua/hello_situation.exe` only; embeds Situation DLL + `lua51.dll` + Lua bytecode (extracted to `%TEMP%` at runtime).
*   **Embedded host** — `wrappers/lua/launcher/` (`sit_lua_host.c`, `sit_lua_runtime.c`, `sit_lua_draw_shim.c`); dynamic `lua51.dll` load via `LoadLibrary` (replaces unstable static `libluajit-5.1.a` link).
*   **Draw shim** — `sit.cmd_draw_text_ex()` / `sit.default_font()` avoid LuaJIT crash on `SituationFont` passed by value.
*   **Tooling** — `tools/gen_lua_embed.py`, `tools/gen_lua_dll_embed.py`; `generate_lua_bindings.py` preserves `const char*` in FFI cdef; included in `tools/run_all.bat`.
*   **Dev mode** — `build/run_lua_dev.bat` runs staged sources with external `build/dll/situation_opengl.dll`.
*   **Docs** — `wrappers/lua/README.md`, `doc/COMPILATION_GUIDE.md`, `tools/README.md`, `build/README.md` updated for compile/bindings/tooling.

### v2.4.361–362 — GL screenshot & readback hardening

OpenGL screen capture and GPU readback made reliable for apps and the harness.

*   **Screenshot capture (361)** — `SituationRequestScreenCapture()` implemented; arm before `EndFrame`, then `LoadImageFromScreen` copies the pre-swap framebuffer.
*   **Readback hardening (362)** — Pre-swap capture restored every frame; readback buffers and render-thread buffer copy work with the shared loader GL context.

### v2.4.360 — Renderer modularization complete (R0–R5)

Mechanical header-only split of the renderer into lifecycle, shader, resource, and frame-cmd slices. No public API changes — details in [`RENDERER_MODULARIZATION_PLAN.md`](done/RENDERER_MODULARIZATION_PLAN.md).

### v2.4.357 — Color module consolidation

*   **`situation_impl_color.h`** — single home for HSV, YPQ, PQ/HDR, 10-bit packing, and sRGB linearization (public pixel APIs + internal YIQ core).
*   **`situation_impl_image.h`** — image/font/draw and CPU adjust loops only (`SituationImageAdjustYPQ` / `AdjustHSV`).
*   **Trace** — `situation_base_trace.h` regenerated; color APIs grouped under `SITUATION_TRACE_SITUATION_IMPL_COLOR_H`.
*   **No API break** — public signatures unchanged; trace numeric IDs for moved functions repacked into the color file block.

### v2.4.355–356 — OpenGL render-thread hardening

Host-side GL work and render-thread frame execution made safer when a render thread is active.

*   **Texture upload (355)** — Main-thread texture creation binds the shared loader context; upload failures reported reliably; one-shot `build_shaderc.bat` for Vulkan builds.
*   **Frame lifecycle (356)** — Graveyard flush gated on fence signal; failed frames release slots without deadlock or garbage presents; shutdown path simplified.

### v2.4.354 — VD standby API / harness separation

*   **`SitVdStandbyConfig`** — VD idle pattern types (`SitVdStandbyLayer`, layer bitmasks, 144 B UBO size) consolidated in `situation_api_types_gpu.h`; removed fused `sit_vd_pattern_config.h`.
*   **Library vs harness** — compositor upload stays in `situation_impl_vd.h`; harness pack/init/draw helpers live under `tests/harness/` only (`sit_harness_pattern_ubo.h`, `sit_harness_test_pattern_helpers.*`).
*   **API** — `SituationSet/GetVirtualDisplayPatternConfig` use `SitVdStandbyConfig*` (replaces `SitTestPatternConfig*`).
*   **Build** — `sit/Makefile` builds library only; `tests/harness/Makefile` + `build/build_tests.bat` build the harness.

### v2.4.353 — Renderer fwd sync + internal errno hardening

*   **`situation_impl_renderer_fwd.h`** — synced to **345/345** static coverage (`verify_renderer_fwd.py` green); 16 missing forwards added, 3 stale decls removed.
*   **VK quad pipeline resolve** — `_SitVulkanResolveQuadPipeline` returns `SituationError`; VD-format pipeline failures no longer silently fall back to the default quad pipeline.
*   **VK bindless slot write** — `_SitVulkanWriteSlotToGlobalBindlessSet` returns `SituationError` with caller context; propagated from internal draws and VD paths.
*   **GL storage→sample prep** — `_SituationGLPrepareStorageTextureForSampling` returns `SituationError`; propagated from soft-buffer quad execute.
*   **Docs** — steering + architecture docs refreshed for v2.4 build/wrapper state.

### v2.4.346–349 — Vulkan recovery & GL/VK visual parity

Four-patch sprint: mesh/harness fixes, shutdown stability, and Vulkan 2D + VD rendering brought in line with OpenGL.

*   **Mesh & harness (346)** — PBR tangent layout for GLTF/OBJ; model/STL tests resolve assets reliably; async shader unload no longer orphans Vulkan compile tickets.
*   **Shutdown (347)** — Shader cache and descriptor pools no longer double-freed on teardown; render thread joined before pool/VMA cleanup.
*   **Quads & patterns (348–349)** — Vulkan `DrawQuad` depth, projection, and VD viewport/scissor match OpenGL; 3D test pattern grid orientation fixed on Vulkan.

### v2.4.345 — OpenGL VD PATTERN compositor (SPIR-V embed + fix)

*   **OpenGL compositor** — build-time glslc → `sit_vd_compositor_gl_spirv_embed.*`; same `#include sit_test_patterns.glslh` as Vulkan; `GL_ARB_gl_spirv` at init.
*   **Init/composite fix** — sampler `layout(location,binding)`, `u_model` @ loc 8, pattern UBO @ binding 6, explicit sampler uniform locations (SPIR-V strips names).
*   **345 regression (initial embed)** — `-641` SPIR-V link failure (sampler @ loc 0 vs `v_texCoord`); all GL modules SETUP FAILED until shader/init fixes landed in same patch.
*   **Known gaps** — NVIDIA spurious `GL_INVALID_OPERATION` after VD draw (drained; pixels OK); 4 VD tests flaky in full module run; §5.3 COLORBURST gate open.
*   **Verified** — `graphics --filter pattern` 10/10 GL; `vd_idle_pattern_standby`, `vd_composite_time` pass.

### v2.4.344 — VD idle PATTERN compositor (full config API)

*   **VD API** — `SituationSet/GetVirtualDisplayPatternConfig`, `Set/GetVirtualDisplayPatternLayers`; `standby_pattern` (`SitTestPatternConfig`) on each Virtual Display.
*   **Vulkan compositor** — idle `PATTERN` uploads 144 B std140 UBO (sets 2/3); layer bitmask + tuning (checker size, multiburst freqs, …) via `sit/gpu/test_patterns/`.
*   **Idle modes** — `SOLID`, `COLORBURST` (SMPTE subset), **`PATTERN`** (full compositor on Vulkan).
*   **Build** — `sit_test_pattern_config.c` in DLL; VMA teardown fix for pattern UBO.
*   **Harness** — `vd_pattern_config_api`, `vd_idle_pattern_standby`; `run_tests.bat` MinGW PATH fix.
*   **Docs** — `doc/guide/test_patterns.md` rewritten; plan §3.4 synced.

### v2.4.343 — Test pattern harness (std140 UBO, dual-backend)

*   **Config contract** — `sit_tp_config_ubo.glslh` + `sit/sit_test_pattern_config.h/.c`; std140 UBO @ `set=0, binding=0` on **GL and VK** (no push/SSBO split).
*   **Harness** — `test_graphics_patterns.c` readback suite; identical GL/VK FS sources; SPIR-V embed via `compile_harness_shaders.ps1`.
*   **Library** — `sit/gpu/test_patterns/` modular `.glslh` headers + `sit_tp_sample()` dispatcher (P0–P7 gate: VK **9/9**, GL **8/8**).
*   **Plan** — `doc/plan/RGL_TEST_PATTERN_SHADER_MIGRATION_PLAN.md` updated; P6 lab example + P8 user guide still open.
*   **Steering** — `.kiro/steering/situation-project.md` refreshed (chunked updatelog, test_patterns tree, Python bindings).

### v2.4.342 — Python ctypes bindings (sixth wrapper language)

*   **Generator** — `tools/generate_python_bindings.py` → `wrappers/Python/situation/` (**594** DLL exports, **18** callbacks).
*   **FFI** — stdlib **`ctypes`** only; `load_dll("opengl"|"vulkan")` + `foreign.bind_all(dll)`; `helpers.init_info_window()` for ABI-safe `SituationInitInfo`.
*   **Demo** — full `hello_situation.py` (torus raymarch, audio graph, virtual MIDI, HUD) via `build/build_python_example.bat`.
*   **Pipeline** — included in `tools/run_all.bat`; `scripts/wrapper_paths.bat` **`python`** row → `build/examples/python/`.

### v2.4.341 — Font migration (grid GPU text + RGL bitmap delegate)

*   **Lifecycle** — `SituationUnloadFont` tears down owned atlas textures and `glyph_info` (built-in default atlas preserved).
*   **Bitmap → GPU** — `SituationBakeBitmapFontAtlas` uploads 1bpp VGA-style bitmap fonts as NEAREST grid atlases; `SituationLoadBitmapFontFromTexture` for pre-baked sheets.
*   **Retro builders** — `SituationPackedFont` + terminal / CP437 / ASCII / packed / VCR / VGA (+ outline) `SituationCreate*` APIs (`situation_api_platform.h` → `situation_impl_image.h`).
*   **Layout** — `SituationMeasureTextEx`, `SituationGetTextLineCount`; multiline-aware `SituationMeasureText`.
*   **GPU draw** — `SituationCmdDrawTextEx` renders any grid atlas (not only default font); `SituationCmdDrawTextBoxed` with word wrap.
*   **CPU stamp** — `SituationImageStampText` / `StampTextBoxed` for labels on `SituationImage`.
*   **RGL** — bitmap `RGL_Create*Font`, `DrawText*`, `MeasureText`, `UnloadBitmapFont` delegate to Situation (TTF/stamp wrappers still pending).
*   **Harness** — OpenGL `text_rendering` **8/8**; see `doc/plan/font_migration_plan.md` for remaining F5–F7 work.

### v2.4.340 — P2 complete (doc guide split + sign-off)

*   **`doc/situation_api.md`** is now a **96-line** module map; full reference lives under **`doc/guide/`** (22 files).
*   **581/581** symbol coverage — `merge_api_doc_gaps.py` and `generate_api_index.py` scan umbrella + guide tree.
*   **Bindings verified** — all five FFI generators + index regen via `tools/run_all.bat`.
*   **Harness** — static OpenGL/Vulkan builds green; core init smoke tests pass both backends.
*   **P2.4 deferred** — `SituationThreadPool` stays a public struct (by-value usage in examples/tests); opaque handles planned for v2.5.

### v2.4.339 — API header split (P2.2)

*   **`sit/situation_api.h`** is now a **94-line umbrella** — guards, `SITAPI`, and an `#include` chain to nine domain files (`config`, three `types_*`, `platform`, `graphics`, `audio`, `system`, `deprecated`).
*   **No ABI change** — still **581** public `SITAPI` functions; include **`sit/situation.h`** as before.
*   **Tooling** — `read_expanded_api_lines()` in the API parser keeps bindings and `situation_api_index.md` accurate across submodules.
*   **Deprecated gate** — `SITUATION_INCLUDE_DEPRECATED_API` (default **1**); set to **0** before include to omit legacy declarations.

### v2.4.335 — Phase D backtrack (bindless internal migration **failed**)

*   **Tried and failed** — v2.4.334 retried internal `global_textures[]` bindless sampling (after v2.4.171 had fixed the same all-black failure with per-texture samplers). Extended to text, YPQ, VD. **Same black-frame failure** on reference GTX 1070. **Reverted** in v2.4.335.
*   **What shipped instead** — push `texture_id` scaffolding on text / YPQ / VD; all internal textured draws back on v2.4.171 `single_sampler_descriptor_set` model. D5 frame bindless pre-bind disabled.
*   **Phase D status** — bindless-as-sole-internal-binding goal **not achieved**. Documented as failed experiment in **`doc/plan/plan_handles_ssbo.md`** (backtrack section) and **`doc/UPDATELOG.md`**.

### v2.4.331 — Phase B′ vertex pull & mesh BDA

*   **Mesh BDA APIs** — `SituationGetMeshVertexBufferAddress`, `SituationGetMeshIndexBufferAddress` for Vulkan buffer-device-address pull draws.
*   **GLSL helper** — `sit/gpu/vertex_pull.glslh`; reference example `examples/other/vertex_pull_triangle.c`.
*   **Harness** — `test_vertex_pull_render`, `test_mesh_bda_compute_read` prove pull vs passthrough and compute BDA readback.

### v2.4.330 — OpenGL VSync + render thread

*   **VSync toggle** — With dedicated render thread, VSync-off no longer floods the queue or hangs when re-enabling VSync; `glfwSwapInterval` applied at present time.
*   **Backpressure** — `_SitShouldEngageBackpressure()` always engages when render thread is active.

### v2.4.329 — Fortran & Modula-2 language bindings

*   **Generators** — `tools/generate_fortran_bindings.py` and `tools/generate_modula2_bindings.py` emit 575 procedures + 18 callbacks into `wrappers/Fortran/` and `wrappers/Modula2/`; both wired into `tools/run_all.bat`.
*   **`hello_situation` demos** — full Raster Bars + Ambient Synth port in Fortran (`main.f90`) and Modula-2 (`Main.mod`), matching Rust/Zig/Odin.
*   **Build scripts** — `build/build_fortran_example.bat` and `build/build_modula2_example.bat` with shared `scripts/wrapper_compile_*.bat` and static-link fixes for Fortran.
*   **Fortran verified** — all four backends (`opengl`, `vulkan`, `static-opengl`, `static-vulkan`) build successfully.
*   **Modula-2 blocked** — bindings generate; end-to-end compile needs `gm2` (bundle under `_languages/gm2/` or GCC `--enable-languages=m2`).
*   **Fortran `hello_situation` Vulkan (demo, verified)** — shader persistence after focus loss / swapchain events; VSync toggle; borderless/fullscreen; cache-busting shader reload; **no core library changes**.

### v2.4.308–326 — Debugger tools, Vulkan UBO fix, OpenGL ring fence fix, error gap-fill, VSync spikes

*   **OpenGL ring fence fix** — Fixed a severe heap corruption bug where `ring_fences` was hardcoded to size 3 but indexed up to `SITUATION_MAX_FRAMES_IN_FLIGHT` (6). This prevented silent crashes/exits during test harness teardown. Removed obsolete `Sleep(50)` fullscreen release delays to optimize teardown performance.
*   **Error Code Gap-Fill Plan** — Added 6 new warning/error codes covering exclusive fullscreen release, display mode settling, context reclamation, audio transitions, and incomplete shutdowns.
*   **Forward-declaration coverage** — Canonicalized all internal static forward declarations into single headers (`situation_impl_forward.h` and `situation_impl_renderer_fwd.h`), with automated script validation (`verify_impl_forward.py`).
*   **VSync spike elimination** — Rolled out windowed latency metrics (resets every 120 frames), avoiding SLEEP lock-in from transient spikes and Alt-Tab stalls.
*   **Canvas stretch fail-soft** — OpenGL rendering falls back to default FBO 0 at native display resolution if canvas resources fail to allocate during display mode transitions, avoiding hard failures.

### v2.4.304–307 — Examples, VSync/uncapped FPS, render thread, piano audio, and diagnostics

*   **Examples & VSync** — F9/V now reliably gives uncapped FPS. Shared scaffolding forces 8-bit output and ties target FPS to VSync state. Example 2 shapes fixed.
*   **Render thread** — Fewer micro-stutters (more in-flight frames, removed per-frame spam, shorter timeouts). VSync toggle now works correctly with the dedicated render thread (context-safe swap interval, no GLFW errors).
*   **Stutter & spike diagnostics** — New general-purpose tools: `GetMaxFrameTime`, `GetFrameSpikeCount`, `GetLastFramePhases`. Core timing (poll/update + sub: glfw_poll, input_reset, joystick) + render phases (backpressure/fence/execute/present). Live overlay + improved histogram. Auto-logging on real outliers (relative thresholds). Left-anchored debug panel; polling text at standard 8px size to reduce jiggle/clipping.
*   **Build & robustness** — `clean` improvements, restored missing struct fields, fixed duplicate vars, eliminated GLFW context errors on VSync toggle.
*   **Piano demo audio** — `node_graph_piano_demo` now produces sound (final mixer sink + explicit TopoSort + legacy fallback; MIDI visuals already worked).
*   **Uncapped FPS** — VSync off (`F9`) now yields > refresh rate (relaxed backpressure when target==0, MAX_FRAMES=6; main no longer blocks on render queue).
*   **Log hygiene** — `[STUTTER]` no longer floods on normal 60 fps frames (60 ms floor + rate limit; real spikes still reported with phases).
*   **Key conflicts** — Piano keybed no longer hijacks globals (`V` removed from vsync toggle; filter env rebound to `[` / `]`).

These changes continue the "Lightweight Stutter Diagnostics" work described in `doc/plan/AAA_ARCHITECTURE_PLAN.md`.

See `doc/UPDATELOG.md` for the full list of changes.

### v2.4.299–2.4.303 — 10-bit / HDR color output (Phases 5–8)

*   **HDR10 support**: Proper detection via DXGI, HDR10_ST2084 swapchain selection (when policy AUTO or HDR10 and OS HDR enabled), forced FIFO present mode for compositor compatibility.
*   **PQ (ST.2084) pipeline**: Linear ↔ PQ conversion APIs, HDR clear colors, packed 10-bit helpers.
*   **Readback**: Raw A2R10G10B10 support via `SituationReadFramebuffer` + dedicated HDR readback.
*   **Harness & diagnostics**: Tests for HDR caps, PQ roundtrips, format logging. Better error messages for SDK/10-bit issues.
*   **Caps**: `output_hdr_active`, `wsi_supports_hdr10`, etc. exposed in `SituationGetGraphicsCaps`.

### v2.4.267–2.4.271 — Modernized build system

*   `build_situation.bat` is now a thin launcher for `sit/Makefile`.
*   Auto-builds GLFW when missing.
*   Much better error messages and self-documenting checks for Vulkan SDK, shaderc, etc.
*   Version info embedded in Windows DLL resources.
*   `clean` / build hygiene improvements (see 2.4.304 for latest).

### v2.4.259–2.4.266 — Bindless hardening, allocators, and misc

*   Better error handling and validation for bindless buffers/textures (`SituationGetBufferDeviceAddress`, `SituationGetTextureHandle`).
*   `aud/` subsystem and other areas migrated to `SIT_MALLOC`/`SIT_FREE` (MyBuddy integration ongoing).
*   Various small stability and diagnostics fixes.

### v2.4.258 — SSBO reflection API + harness completion

*   **`SituationQueryShaderStorageBlocks`** (OpenGL): New API to query active SSBO binding points after linking.
*   Added `SituationShaderStorageBlockInfo` struct.
*   Harness now has full graphics reflection coverage (Phase 27).

### v2.4.256 — Multisample state control

*   `SituationCmdSetMultisampleState` now fully works on OpenGL (sample shading, mask, alpha-to-coverage).
*   Vulkan records the state for later pipeline use.
*   Fixed `max_msaa_samples` reporting on Vulkan.
*   Raster push/pop now handles multisample state.

### v2.4.250 — Wrapper builders now support all backends

*   `build_odin_example.bat`, `build_zig_example.bat`, `build_rust_example.bat` now accept the same targets as the C builds (`opengl` / `vulkan` / static variants).

### v2.4.239 — Thread naming

*   `SituationSetCurrentThreadName` — sets the name visible in Task Manager/debuggers.
*   `SituationInitInfo::main_thread_name` for startup override.

### v2.4.238 — Async shader unload progress driver (Phase A)

*   **Shared progress driver (v2.4.238):** `_SituationVkAsyncCompileProgress` unifies Vulkan poll and unload wait — LOST detection, tiered timeouts, and abandon+job retire in one path.
*   **Unload timing (v2.4.238):** Normal abandon at **`SITUATION_VULKAN_ASYNC_UNLOAD_ABANDON_NS`** (2 s); **`SITUATION_VULKAN_ASYNC_UNLOAD_WAIT_NS`** (10 s) demoted to shutdown last-resort only.
*   **Leak fixes (v2.4.238):** Worker CAS fail on `compile_done == -2` frees ctx; poll timeout abandon retires pool job. See **`doc/plan/ASYNC_SHADER_LOAD_HARDENING_PLAN.md`**.

### v2.4.229–237 — Render thread & async shader hardening

*   Fixed render queue wedge that could lock the render thread.
*   Better slot reuse, completion ordering, and backpressure handling.
*   More reliable async shader compile/unload with proper error codes.

### v2.4.228 — Vulkan BindPipeline descriptor fix

*   **MESH layout bind (v2.4.228):** Restores `dynamic_ubo_layout` at set 0 for `SIT_SPIRV_LAYOUT_PROFILE_MESH` (reverts v2.4.227 regression).
*   **OpenGL canvas stretch (v2.4.228):** Canvas FBO helpers available in OpenGL builds (fixes static-opengl link).

### v2.4.227 — Vulkan screenshot slot & OBJ readback

*   **Screenshot cache (v2.4.227):** Per-frame-slot resolve tracking; stale cache rejected on `SituationLoadImageFromScreen`.
*   **OBJ/mesh (v2.4.227):** CPU mesh cache on create; degenerate normal fix; harness mesh leak cleanup.

### v2.4.226 — Vulkan render thread present fix

*   **Present after fence (v2.4.226):** Render thread presents only after successful submit + GPU fence signal; per-slot compute sync; threaded EndFrame compute handoff.

### v2.4.225 — Vulkan render sync & OBJ normals

*   **Render-thread deadlock (v2.4.225):** Fence wait ordering fixes from v2.4.224 follow-up; general OBJ normal handling.

### v2.4.224 — Vulkan acquire & screenshot readback

*   **Root cause (v2.4.224):** **v2.4.223** swapchain/framebuffer sync could make the **first** `SituationAcquireFrameCommandBuffer()` fail (-710 → harness **-502**) while the second succeeded; with the render thread, `SituationLoadImageFromScreen` could read **stale** swapchain pixels when `screenshot_valid` was not resolved yet.
*   **Acquire retry loop (v2.4.224):** Internal `for` loop (`SITUATION_VULKAN_ACQUIRE_SWAPCHAIN_RETRIES`, default **4**) retries fence wait + recreate + `vkAcquireNextImageKHR` after resize sync, out-of-date, acquire timeout, and render-thread recreate requests. `image_acquired` guard prevents silent success with `image_index == 0`. `_SituationVulkanEnsureSwapchainMatchesFramebuffer` returns **SUCCESS** after successful recreate (not -710).
*   **Screenshot resolve (v2.4.224):** `_SituationVulkanEnsureScreenshotResolvedForFrame` + `SituationLoadImageFromScreen` fence-wait hook — fixes VD pixel asserts (`vd_visibility_toggle`, `vd_scaling_fit`, blend/offset/opacity), `obj_loader.teapot_obj_draw_and_verify`, and related readback tests in visible-window runs.
*   **Harness (v2.4.224):** `sit_test_gpu_context_init` skips double-`SituationInit` (**`ypq_photo_y_p_q_sweep`**). Optional `sit_test_acquire_frame` helper added (not yet wired module-wide). See **`doc/UPDATELOG.md`** v2.4.224 for per-test table.

### v2.4.223 — Fullscreen canvas stretch parity

*   **Fullscreen canvas stretch (v2.4.223):** Exclusive fullscreen keeps the **monitor native resolution** while rendering stays at the **windowed canvas size**. The finished frame is upscaled to fill the display — OpenGL/Vulkan parity, **nearest-neighbor** filtering for crisp pixels. HUD and VD content no longer change scale on F11.
*   **Render canvas (v2.4.223):** **`SituationGetRenderWidth` / `SituationGetRenderHeight`** return canvas dimensions during fullscreen stretch. Example: **`examples/vd_idle_standby_demo.c`**. See **`doc/UPDATELOG.md`** v2.4.223.

### v2.4.221–222 — VD idle detection & standby compositor

Virtual Displays detect stale content and switch to a shader-only standby instead of sampling old pixels.

*   **Content tracking (221)** — `SituationGetVirtualDisplayUpdateInfo` reports last pixel update and idle elapsed time; per-VD threshold via `SituationSetVirtualDisplayIdleThreshold`.
*   **Standby fallback (222)** — Compositor draws solid color or SMPTE colorburst when idle; `SituationSetVirtualDisplayFallbackMode` / `SetVirtualDisplayFallbackColor`.

### v2.4.220 — Vulkan graphics viewport parity

*   **`_SitVulkanApplyGraphicsViewportScissor` (v2.4.220):** Tracked user draws (`SituationCmdDraw`, indexed/indirect, pipeline rebind) now re-apply viewport via **`_SitVulkanFillViewport2DOpenGLParity`** instead of a standard Vulkan viewport (`y = 0`, positive height). Aligns **v2.4.175** hygiene with **v2.4.189** pass-wide top-left convention (`BeginRenderPass`, internal quads/text, VD compositor). Supersedes the **v2.4.189** note that user draws kept a "standard" viewport. See **`doc/UPDATELOG.md`** v2.4.220.

### v2.4.219 — OBJ Model Support

*   **`SituationLoadModelFromOBJ` (v2.4.219):** Load Wavefront `.obj` files and their associated `.mtl` material libraries/textures with no new external dependencies, using the vendor-included `tinyobj_loader_c` library. Geometry is packed into the engine's standard stride-48 PBR layout `[Px Py Pz Nx Ny Nz Tx Ty Tz Tw U V]` with default tangents, making it fully compatible with existing shader contracts and drawing paths. Performs automatic index-based vertex deduplication and dynamic material texture caching.
*   **Model Hot-Reloading (`SituationReloadModel` v2.4.219):** Fully resolved reload routing for non-glTF models. When reloading a model slot, the system now inspects stored format flags and dispatches the request to `SituationLoadModelFromSTL` (preserving `smooth_normals` settings) or `SituationLoadModelFromOBJ` instead of routing all formats to the GLTF loader.

### v2.4.218 — STL model loader, Demon Hunt visual bolster fixes

*   **`SituationLoadModelFromSTL` (v2.4.218):** Load binary or ASCII `.stl` files with no external dependency. Auto-detects format. Flat shading (default) uses per-face normals directly; smooth shading (`smooth_normals = true`) merges coincident vertices and averages normals. Produces stride-32 `SituationModel` — works with `SituationDrawModel`, `SituationUnloadModel`, and `SituationReloadModel` unchanged.
*   **Demon Hunt material system enabled (v2.4.218):** `DH_ENABLE_MATERIALS` flipped to 1. Per-wall shading (Stone, Metal, Flesh, Emissive, Wood, Bone, Rusted Metal) now active.
*   **Demon Hunt bloom corrected (v2.4.218):** Replaced current-frame brightness stub with proper 2-iteration Kawase blur from the feedback texture (8 diagonal samples, luminance-thresholded). Film grain and shadow dithering added. Shadow dither scope fixed — was inside `pristine_shadow()` corrupting sprite/floor shadow lookups and breaking projectile trajectory visuals; moved to wall call site only.

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

### v2.4.192–193 — YPQ color toolset (CPU + GPU)

YPQ color editing on CPU and GPU — shared NTSC YIQ math, image adjust API, and a draw-time grade pass.

*   **CPU (192–193)** — `ColorYPQf`, `SituationImageAdjustYPQ`, in-gamut chroma clamp; core math in `situation_impl_color.h`.
*   **GPU grade (193)** — `SituationCmdDrawTextureYpqGrade` applies YPQ grade in the draw path.

### v2.4.191 — Vulkan async shader poll + harness multi-monitor VD

*   **Vulkan async GLSL load (v2.4.191):** **`SituationBeginLoadShaderFromMemory`** submit hardening — fixes intermittent **`graphics.async_shader_*`** timeouts after long harness runs.
*   **Audio output monitor (v2.4.191):** **`SituationSetAudioOutputMonitor`** callback pointer is atomic — safe read from the audio thread while harness installs/uninstalls the scope tap.
*   **Harness stability (v2.4.191):** Unified **1024×768** window, render pumping during long waits, module order fix for listen tests; new **`advanced`** module **`all_displays_windowed_fullscreen_cycle`** (multi-monitor via one window + one VD per monitor). See **`doc/plan/renderer_bolster_plan.md`** Phase 11-bis.
*   **OpenGL DLL build fix (v2.4.191):** **`build_situation.bat`** OpenGL path — fixed `^` line continuations (same class as Vulkan **v2.4.170** fix).

### v2.4.190 — Harness scope perf + tone synth tests

*   **Stereo scope overlay (v2.4.190):** Throttled spectrum analysis and fewer line-segment quads per frame — mitigates intermittent window/scope freezes during listen tests. Harness-only; **`tests/harness/sit_test_stereo_scope.c`**.
*   **Tone synth harness (v2.4.190):** **`legacy_midi_note_frequency`** and **`midi_complex_melody`** capture fixes.

### v2.4.189 — Tone synth sub osc + Vulkan 2D parity

*   **Tone synth sub oscillator (v2.4.189):** **`sub_coarse`** (CC111), **`sub_sync`** (CC112), **`sub_ring_mod`** (CC113). See **`doc/tone_synth.md`**.
*   **Vulkan 2D matches OpenGL (v2.4.189):** **`_SitVulkanFillViewport2DOpenGLParity`** for **`BeginRenderPass`**, internal 2D draws, and user-draw hygiene — same ortho, text, and textured-quad shaders as OpenGL; removed Vulkan-only Y/UV flips.

### v2.4.188 — Render-pass helpers and docs

*   **Render-pass foundation (v2.4.188):** Expanded **`SituationRenderPassInfo`** load/store/clear documentation; inline helpers **`SituationRenderPassInfoDefault`** / **`SituationRenderPassInfoLoad`**; public **`SituationRenderPassConfigurationKey`** for Vulkan render-pass caching. Harness: **`core.render_pass_*`**.

### v2.4.187 — Raster state complete

*   **Phase 6 closed (v2.4.187):** Fixed-function raster commands done on GL/VK. **`SituationCmdSetMultisampleState`** deferred to **v2.5** render-target work (MSAA surfaces + resolve).

### v2.4.186 — Vulkan color mask, stencil, push/pop raster

*   **Vulkan Phase 6B parity (v2.4.186):** **`SituationCmdSetColorWriteMask`**, **`SituationCmdSetStencilTest`**, **`SituationCmdPushRasterState`** / **`PopRasterState`** on Vulkan via **`VK_EXT_extended_dynamic_state3`**.

### v2.4.185 — Line width, color mask, stencil, push/pop raster

*   **Raster state Phase 6B (v2.4.185):** **`SituationCmdSetLineWidth`**, **`SituationCmdSetColorWriteMask`**, **`SituationCmdSetStencilTest`** (+ **`SituationStencilState`**). OpenGL push/pop raster stack captures/restores fixed-function state at execute time.

### v2.4.184 — Indexed viewport/scissor

*   **Indexed viewport/scissor API (v2.4.184):** **`SituationCmdSetViewportIndexed`** / **`SituationCmdSetScissorIndexed`**; **`SituationGraphicsCaps.max_viewports`**; legacy **`SetViewport`** / **`SetScissor`** remain index-0 wrappers. Harness: **`core.viewport_index_zero_parity`**, **`core.viewport_index_out_of_range`**.

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
*   **Threading Bolstering (v2.4.139–v2.4.144):** 🎉 **COMPLETE!** Promoted the generational C11 thread pool into a topology-aware execution layer: CPU topology cache, affinity query/set APIs, physical-core and NUMA mask builders, configurable render/audio/main affinity, NUMA placement policy, per-worker/I/O/render/audio observability, queue-depth APIs, pool snapshots, scheduler metrics, dynamic high-queue scan depth (later superseded by full-queue in-place claim in **v2.4.232–233**), auto worker sizing from `SituationInitInfo`, and a consolidated **`doc/THREADING_BOLSTERING_API.md`** reference.
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

