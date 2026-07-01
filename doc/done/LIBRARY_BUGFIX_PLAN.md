# Library Bugfix Plan (Exposed by Test Harness)

**Date**: 2026-05-07  
**Updated**: 2026-05-23 (**v2.4.106** — Windows **`SituationInit`** step 7 always uses WASAPI **shared** auto-start; **`Bug 6`** closed on reference config; full sequential harness green. Prior **v2.4.105** — Vulkan async GLSL worker queue. Prior **v2.4.65** — OpenGL uniform defer / Hi-DPI viewport.  
**Scope**: Fix library bugs that cause test failures  
**DLL version**: Defined in **`sit/situation_base_version.h`** (`SITUATION_VERSION_*`); root **`situation.h`** includes that header only (no duplicate version macros). Narrative releases in `doc/UPDATELOG.md`.

**Patch bump policy**: Bump **`SITUATION_VERSION_PATCH`** when shipping user-visible library fixes documented in **`doc/UPDATELOG.md`** (canonical **`sit/situation_base_version.h`** only). Latest narrative: **v2.4.106** (Windows **`SituationInit`** auto-start uses WASAPI **shared** mode — fixes harness/system audio mute after repeated init/shutdown). Prior: **v2.4.105** (Vulkan async GLSL worker queue). **Bug 6**: **✅ closed** (**v2.4.106** — lifecycle + sequential suite on reference Windows config; see **Bug 6** section). **Harness**: OpenGL **`sit_test.exe`** **337/337**; Vulkan **`sit_test_vulkan.exe`** **327/327** (**2026-05-23**). Re-run when touching init/teardown/audio; **`copy /Y build\dll\*.dll build\`** so the exe does not load a stale DLL.

### Version milestones — **v2.5 is not “next patch”**

**`SITUATION_VERSION_MINOR` → 5** (marketing **v2.5.x**) is reserved for a **library-complete** milestone — **“everything works”** in the sense of the project’s shipping bar, **not** “a big feature landed.” Do **not** bump minor or call out **v2.5** for partial wins (e.g. node graph presence, individual modules green in isolation).

Before **v2.5**, at minimum align on and satisfy something like:

| Gate | Meaning | Status |
|------|--------|--------|
| **Bug 6** | Audio device **init / shutdown / re-init** contract **proven** — hang and AV paths understood; policy for exclusive vs shared documented. | **✅ Met** (**v2.4.106**) |
| **Harness** | Full sequential **`sit_test.exe`** (no `--module`) **green** on a reference Windows config, **or** every failure **classified** with owner (test vs library vs driver) — no silent crashes. | **✅ Met** (**440/440** OpenGL + **430/430** Vulkan, **v2.4.191**) |
| **Vulkan graphics** | **`build\sit_test_vulkan.exe --module graphics`** → **86/86** on the **NVIDIA GTX 1070** reference machine. Optional spot-checks on **Intel / AMD** when hardware exists — document any deltas; they do **not** block **v2.5** unless failures indicate a **cross-vendor** library bug. | **✅ Met** (**v2.4.94**) |
| **Audio pipeline** | **`doc/plan/AUDIO_NODE_COMPLETION_PLAN.md`** canonical callback contract **implemented and reviewed** (routing policy A vs B, unload/thread safety addressed or explicitly constrained). | **✅ Met** (**v2.4.35–v2.4.198**) |

Until those are true, stay on **v2.4.x** patches and narrative releases. Older docs that mention **“target v2.5.0”** for MIDI sub-features are **aspirational feature labels**, not permission to ship **Situation v2.5** early.

---

## Current State

**OpenGL Graphics module: 81/81 ✅** — all tests pass individually.

**Vulkan Graphics module**: **✅ 86/86** on reference **GTX 1070** (**Windows**) as of **v2.4.94** — **86** tests on **`sit_test_vulkan.exe`** (OpenGL-only entries excluded; includes **Phase 30** SPIR-V tests). Prior milestones: readback path (**V6** / **v2.4.43–v2.4.44**), acquire timeouts, VD composite render-pass discipline, **v2.4.55–v2.4.61** descriptor / VD / screenshot / push-constant fixes; **v2.4.93–94** Vulkan user SPIR-V descriptor profiles (**`doc/UPDATELOG.md`**, [`VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md)). **`render_virtual_displays`** no longer SIGSEGV when no main RP was begun.

**Full sequential suite (`sit_test.exe` all modules)**: **✅ 337/337** OpenGL + **327/327** Vulkan on reference Windows config (**2026-05-23**, **v2.4.106**). Prior milestones: **v2.4.53** — **`SituationShutdown`** teardown end-to-end (**Bug 11**); **v2.4.54** — Vulkan **`[VMA LEAK]`** shutdown path; **v2.4.52** — miniaudio stopped before **`vkDeviceWaitIdle`**; **v2.4.106** — Win32 auto-start always **shared** (**Bug 6**).

**Audio is first-class** for this library (games / multimedia): the goal is **correct playback device lifecycle** on init / shutdown / re-init — not avoiding the problem. See **Bug 6 — Audio & full sequential suite** below for **what we know**, **what is separate**, and the **ordered fix strategy**.

**Diagnostics**: Optional acquire timing on stderr — **`SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS`** in **`sit/situation_api.h`** (default **100** = log slow acquires + **`VK_TIMEOUT`**; **`0`** = log every acquire). **`build_situation.bat`** forwards **`%EXTRA_VULKAN_CFLAGS%`** (e.g. **`-DSITUATION_VERBOSE_DIAGNOSTICS`** for Vulkan init chatter during investigations).

**Other modules**: All pass individually (filesystem, threading, core, window, input, timer, audio, misc).

### What’s left

| Priority | Track | Goal |
|----------|--------|------|
| **P1** | **Bug 6 — Audio & sequential suite** | **✅ Closed** (**v2.4.106**): Win32 auto-start always **shared**; full sequential harness green; audio probe confirms init + output meter. Optional follow-ups: **`ma_device_init`** timeout surfacing, **`SituationInitInfo`** output policy flag — not blocking. |
| **P1** | **Vulkan graphics** | **✅ 86/86** on **GTX 1070** (**v2.4.94**). **Regression discipline**: after material Vulkan changes, run **`build\sit_test_vulkan.exe --module graphics`** (and **`--filter spirv`** when touching SPIR-V / descriptors); refresh **`UPDATELOG`** / this doc if the scorecard shifts. Optional **Intel / AMD** spot-checks. |
| **P2** | **Vulkan SPIR-V user descriptors** | **✅ Complete** (**v2.4.93–94**): [`VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md) — **`SituationLoadShaderFromSpirvMemoryEx`**, profile-aware binds, harness pixel readback **5/5** on Vulkan. |
| **P2** | **Teardown VMA / registry warnings** | **✅ Addressed v2.4.54** — root cause was **deferred** **`vmaDestroy*`** during **`_SituationCleanupDanglingResources`** while **`vmaDestroyAllocator`** ran later. **Residual** **`SITUATION WARNING: Leaked …`** lines still print when tests leave handles active (intentional nag); after cleanup, GPU memory should be freed without **`[VMA LEAK]`**. |
| **Gate** | **v2.5 minor** | **All gates satisfied** as of **v2.4.200**: Bug 6 ✅, Harness ✅ (440/440 GL, 430/430 VK), Vulkan graphics ✅ (86/86), Audio pipeline ✅ (Phases E0–H complete, Policy B shipped, PCM Input node shipped). **Minor version bump is a maintainer decision** — no technical blockers remain. |

### Vulkan graphics — harness status (**✅ 86/86** on **GTX 1070**)

**How to reproduce the scorecard**: Run **`build_situation.bat vulkan`** then **`build_tests.bat vulkan`** (copies **`build\dll\situation_vulkan.dll`** → **`build\situation_vulkan.dll`** next to **`sit_test_vulkan.exe`**). Run **`build\sit_test_vulkan.exe --module graphics`**. Harness prints **`[graphics] (86 tests)`**; summary line **`RESULTS`** includes skipped modules — use **passed/failed** vs **86** for graphics only. SPIR-V subset: **`build\sit_test_vulkan.exe --module graphics --filter spirv`** → **5/5** (**v2.4.94**).

**Reference** (2026-05-21, **NVIDIA GTX 1070**, Windows): **`sit_test_vulkan.exe --module graphics`** → **86 passed / 0 failed**. **Refresh this subsection** after material Vulkan changes (paste the harness **`RESULTS`** line if the scorecard changes).

**Closure narrative** (full entries in **`doc/UPDATELOG.md`**):

| Wave | Focus |
|------|--------|
| **v2.4.55** | User-shader pipeline layouts (**set 0** dynamic UBO, **set 1** sampled texture), per-texture **`single_sampler_descriptor_set`**, compute→graphics layout hygiene; **`compute_chained_dispatches`** harness SSBO sets (**two sets**, each **`binding = 0`**). |
| **v2.4.56** | **`main_window_render_pass_resume`** (**color LOAD**) so the post-VD main-window restart does not **CLEAR** the composite. |
| **v2.4.58** | Vulkan **`SituationLoadImageFromScreen`** vertical flip (parity with OpenGL); storage-only textures mirrored into bindless; **`SituationCmdDrawTexture`** fails **`RESOURCE_INVALID`** on bad handles — clears **`texture_*`** readback / storage tests. |
| **v2.4.59** | Non-alpha VD blend modes use **`advanced_compositing_pipeline`** (**`use_advanced`** for all modes except **ALPHA**). |
| **v2.4.60** | Three-set **`advanced_compositing_pipeline`** layout + **`composite_dest_sampler_layout`** at destination **`binding` 5** (screen copy). |
| **v2.4.61** | Preserve caller framebuffer when entering VD (**`vd_resume_swapchain_after_caller_rp`**, Path A multi-layer **LOAD**), explicit **`vkCmdPushConstants`** sizes for Path A/B (no MSVC tail-padding mismatch), recreate **screen-copy** image after **`_SituationVulkanRecreateSwapchain`**. |
| **v2.4.93–94** | Vulkan user SPIR-V **layout profiles** + profile-aware **`SituationCmdBindDescriptorSet`**; harness **`test_graphics_spirv.c`** pixel readback on Vulkan (no skip). |

**Historical “last eight” pixel failures** (pre-**v2.4.58** … **v2.4.61** triage — all green on the reference run): **`draw_metrics_overlay`**; **`vd_blend_additive`**, **`vd_blend_multiply`**, **`vd_blend_none_overwrite`**, **`vd_offset_position`**; **`texture_cpu_gpu_cpu_roundtrip`**, **`texture_storage_write_readback`**, **`texture_format_preservation`**.

#### Resolved earlier (archaeology — still accurate)

- **`draw_textured_checkerboard`** — **FIXED**: **`SAMPLED|STORAGE`** textures use bindless **`SHADER_READ_ONLY`** path; storage-only when **`STORAGE && !SAMPLED`**. **`sit/situation_impl_renderer.h`**, **`sit/situation_impl_decl.h`**.
- **`compute_chained_dispatches`** — **FIXED** (harness): two descriptor **sets**, not two bindings in one set. **`tests/harness/test_graphics.c`**.
- **`descriptor_bind_*`** cluster — **FIXED** (**v2.4.55**): **`SituationLoadShaderFromMemory`** layout + **`SituationCmdBindTextureSet`** / **`single_sampler_descriptor_set`**. **`sit/situation_impl_decl.h`**, **`sit/situation_impl_renderer.h`**, **`tests/harness/test_graphics.c`**.
- **Bulk `vd_*` readback after composite** — **v2.4.56** resume **LOAD**; **v2.4.59–v2.4.61** advanced compositor + caller/resume passes + push constants + screen-copy lifecycle — closes the remaining **VD blend / offset** pixel tests on reference. **`sit/situation_impl_vd.h`**, **`sit/situation_impl_renderer.h`**, **`sit/situation_impl_decl.h`**.

#### Regression discipline (going forward)

1. After **any** material Vulkan change: **`build\sit_test_vulkan.exe --module graphics`** on **GTX 1070** (or primary dev GPU) — target **86/86**; add **`--filter spirv`** when changing SPIR-V loads or descriptor binds.
2. If a test regresses, **`--filter <subtest>`** + optional validation layers; file a **`UPDATELOG`** entry and bump patch per policy above.
3. **`SITUATION WARNING: Leaked Texture`** during teardown still means a **test** left a handle live — cosmetic unless **`[VMA LEAK]`** returns (**v2.4.54** path).

**Note**: **V6** (pre-present screenshot, BGRA→RGBA, **`screenshot_copy_pending`**, swapchain-recreate cache discipline, acquire timeouts, etc.) remains the foundation for pixel tests; see **`Vulkan Bug V6`** later in this doc.

---

## ✅ Bug 1: Audio Callback Race Condition [FIXED]

**Fix**: `atomic_bool audio_ready` guard in callback + set after init completes. **`is_processing_snapshot`** (**v2.4.47**): set during voice snapshot decode/mix; **`SituationUnloadSound`** waits (**`thrd_yield`**) until clear before freeing decoder/buffers. Scratch-buffer failure **`goto tone_mixing`** (do not skip tones).  
**Files**: `sit/situation_impl_decl.h`, `sit/situation_impl_audio.h`, `sit/situation_impl_ctrl.h`

## ✅ Bug 2: Pixel Readback Returns Stale Data [FIXED]

**Fix**: Pre-swap screenshot capture — `SituationEndFrame` reads the back buffer into a CPU buffer before `glfwSwapBuffers`, and `SituationLoadImageFromScreen` uses that buffer. This eliminates DWM/driver-dependent behavior on Windows.  
**Files**: `sit/situation_impl_decl.h` (added `screenshot_buffer` fields), `sit/situation_impl_renderer.h` (pre-swap capture in EndFrame), `sit/situation_impl_image.h` (read from captured buffer)

## ✅ Bug 3: `SituationSaveFileText` Type Mismatch [FIXED]

**Fix**: Test bug — changed `SituationError err = SituationSaveFileText(...)` to `bool save_ok = ...`.  
**File**: `tests/harness/test_graphics.c`

## ✅ Bug 4: DrawMesh Counter Not Incremented [FIXED]

**Fix**: Added `sit_render.frame_draw_calls++` and triangle count to `SituationCmdDrawMesh`.  
**File**: `sit/situation_impl_renderer.h`

## ✅ Bug 5: VD Composite GL_INVALID_ENUM [FIXED]

**Fix**: `_SitGLBackupState()` now queries actual GL blend state via `glGetIntegerv` when shadow state is `GL_NONE` (invalid). Previously `_SitGLRestoreState()` passed 0 to `glBlendFuncSeparate`.  
**File**: `sit/situation_impl_renderer.h`

---

## ✅ Bug 7 — VD Composite Produces Black Output (12 tests) [FIXED]

**Root cause**: Three issues combined:

1. **Sampler uniform not bound**: The VD fragment shaders declare `uniform sampler2D` without `layout(binding=N)` for OpenGL, defaulting to texture unit 0. But the composite code binds textures to units 4 and 5 (`SIT_SAMPLER_BINDING_SOURCE_0/1`). Fixed by calling `glProgramUniform1i` after shader creation to set sampler uniforms to the correct texture units.

2. **VD quad geometry in wrong coordinate space**: The VD quad used NDC vertices [-1,+1] but the vertex shader applies an ortho projection (pixel coords). Changed to a unit quad [0,1] so that model matrix scale/translate maps correctly to pixel coordinates.

3. **SCALING_STRETCH used VD resolution instead of window size**: The model matrix for STRETCH mode scaled by `vd->resolution` (the internal texture size) instead of `target_width/target_height` (the window size). Fixed to scale by window dimensions.

**Files modified**:
- `sit/situation_impl_renderer.h` — sampler uniform setup after shader creation, unit quad vertices, STRETCH scale fix
- `sit/situation_impl_decl.h` — screenshot buffer fields
- `sit/situation_impl_image.h` — pre-swap buffer readback

---

## ✅ Bug 8 — Shader Uniform Data Flow (4 tests) [FIXED]

**Symptom**: Tests render with uniforms (float multiplier, vec4 color, mat4 transform, textured checkerboard) but pixel values don't match expectations. The readback returns non-black pixels but wrong values.

**Root cause**: `SituationSetShaderUniform` deferred uniform uploads to the soft command buffer via `SIT_OP_SET_UNIFORM`. But `SituationAcquireFrameCommandBuffer` resets the buffer (`packet_count=0`, `data_cursor=0`), so uniforms set before frame acquisition were silently lost.

**Fix**: Changed to immediate `glProgramUniform*` calls (DSA). These apply directly to the program object's state and persist until changed, regardless of frame lifecycle.

**File**: `sit/situation_impl_renderer.h` (SituationSetShaderUniform)

---

## ✅ Bug 9 — Texture Roundtrip Format (2 tests) [FIXED]

**Symptom**: `texture_cpu_gpu_cpu_roundtrip` and `texture_format_preservation` — pixel values don't survive upload→render→readback cycle.

**Root cause**: Two issues in `SIT_OP_DRAW_QUAD` execution:
1. `u_uv_rect` (location 5) was never uploaded to the quad shader — the shader computed `v_TexCoord = 0 + aPos * 0 = (0,0)`, always sampling the bottom-left corner
2. `u_use_texture` (location 6) was only set to 0 when no texture was bound, but never set to 1 when a texture WAS bound — the shader skipped texture sampling entirely

**Fix**: Added `glProgramUniform4fv` for UV rect (loc 5) and `glProgramUniform1i` for use_texture (loc 6) in the draw quad batch loop.

**File**: `sit/situation_impl_renderer.h` (SIT_OP_DRAW_QUAD case)

---

## ✅ Bug 10 — Compute State Leak (1 test, flaky) [FIXED]

**Symptom**: `compute_dispatch_write42` passes in isolation but fails when run after other graphics tests in the module.

**Root cause**: Three issues combined:
1. `SIT_OP_BIND_COMPUTE_PIPELINE` had no case in the execution switch — the compute program was never actually bound via `glUseProgram`, so dispatches used whatever program was previously active
2. `SituationCmdBindDescriptorSetDynamic` didn't pass `usage_flags` to the command packet — SSBOs were always bound as `GL_UNIFORM_BUFFER` instead of `GL_SHADER_STORAGE_BUFFER`
3. `SituationCmdBindComputeTexture` used wrong opcode (`SIT_OP_BIND_DESCRIPTOR_SET`) which tried to interpret the texture ID as a buffer handle — storage images were never bound

**Fix**: Added the missing `SIT_OP_BIND_COMPUTE_PIPELINE` case, passed `slot->usage_flags` in the packet, and changed `SituationCmdBindComputeTexture` to use `SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING`.

**Files**: `sit/situation_impl_renderer.h`

---

## ✅ Bug 11 — `SituationShutdown` skipped full teardown (multi-module AV) [FIXED v2.4.53]

**Symptom**: Full **`sit_test.exe`** (Vulkan) **access violation** when the **window** module ran after **core** — often right after printing **`[window]`**, before the first test line.

**Root cause**: **`SituationShutdown`** used **`atomic_exchange(&sit_gs.is_initialized, false)`** and then called **`SituationIsInitialized()`**, which reads the same atomic — always **false** after the exchange — so the function **returned early** and **never** ran **`vkDeviceWaitIdle`**, **`_SituationCleanupRenderer`**, **`_SituationCleanupSubsystems`**, **`_SituationCleanupPlatform`**, or **`SIT_FREE(_sit_current_context)`**. The next **`SituationInit`** **`memset`** the context while GLFW/Vulkan were still live → **undefined behavior / AV**.

**Fix**: Remove the post-exchange **`SituationIsInitialized()`** guard; document that the exchange’s **return value** is the only “was initialized” signal.

**File**: `sit/situation_impl_ctrl.h` (`SituationShutdown`)

**Harness** (same milestone): **`test_dump_task_graph`** writes to **`nul`** / **`/dev/null`**; results banner uses ASCII **`=`** lines; optional **`SITUATION_VERBOSE_DIAGNOSTICS`** for noisy Vulkan init (see **`UPDATELOG`**).

---

## ✅ Bug 12 — VMA leak at `vmaDestroyAllocator` (deferred destroys during shutdown) [FIXED v2.4.54]

**Symptom**: After a long **`sit_test.exe`** run, VMA printed **`[VMA LEAK] UNFREED ALLOCATION`** for many **`IMAGE_OPTIMAL`** chunks; sometimes **`SITUATION WARNING: Leaked …`** for every registry slot.

**Root cause**: **`_SituationCleanupDanglingResources`** runs **`SituationDestroyTexture`** / similar while **`SituationShutdown`** is in progress. On Vulkan, destroys **deferred** to the frame graveyard (**`_SituationDeferDestroyImage`**). **`vmaDestroyAllocator`** in **`_SituationCleanupVulkan`** ran while deferred **`vmaDestroyImage`** had not yet executed → VMA’s leak checker fired.

**Fix**: **`_SituationVulkanImmediateDestroyDuringShutdown()`** — when **`sit_render.init_state == SITUATION_STATE_SHUTTING_DOWN`**, perform **immediate** **`vmaDestroy*` / `vkDestroy*`** (GPU already **`vkDeviceWaitIdle`** in **`SituationShutdown`**). **`_SituationCleanupVulkan`** flushes **all** graveyards **first** (after idle), before quad/swapchain teardown.

**File**: `sit/situation_impl_renderer.h`

---

## Bug 6 — Audio & full sequential suite [FIXED — v2.4.106]

**Product stance**: Situation is intended as a **core library for games and multimedia**. Output audio must be **reliable** after **`SituationInit`**, and **re-init in one process** (editor restart, test harness, tooling) must either **work** or **fail loudly with a clear error** — never undefined hang without diagnosis.

### Closure checklist (wrap up Bug 6)

| Step | Action | Done when |
|------|--------|-----------|
| 1 | Run **`build\sit_test.exe`** (no args) with current **`build\dll\`** DLL copied beside exe — **OpenGL** reference | **✅ 337/337** (**2026-05-23**) |
| 2 | Same with **Vulkan** DLL — **`build\sit_test_vulkan.exe`** | **✅ 327/327** (**2026-05-23**) |
| 3 | Prove **`SituationInit`** step 7 / re-init policy on Windows (exclusive hijack vs shared auto-start) | **✅ Confirmed** — exclusive-first on session 1 caused system mute; **v2.4.106** always uses **`ma_share_mode_shared`** on Win32 auto-start |
| 4 | Document outcome in this file (**Bug 6** row + **`Remaining verification`** paragraph) | **✅ Done** — see **`doc/UPDATELOG.md`** v2.4.106 |

---

### What we know vs what is still hypothesis

| Topic | Status |
|-------|--------|
| **Playback path** | **`SituationSetAudioDevice`** configures **`ma_share_mode_exclusive`** for low latency (**`sit/situation_impl_audio.h`**). OpenGL and Vulkan builds share this path — audio is **not** Vulkan-specific. |
| **Historical hang (Bug 6)** | Second **`SituationInit()`** in one process could **block inside `ma_device_init`** or leave WASAPI in a bad state when reopening the default device — consistent with **Windows WASAPI exclusive-mode** hijacking the endpoint. **Fixed v2.4.106**: auto-start always **shared** on Win32; explicit **`SituationSetAudioDevice`** still exclusive. |
| **Lifecycle hardening (in tree)** | **`ma_device_stop`** before **`ma_device_uninit`** on teardown; Win32 **`SituationInit`** step 7 **always** uses **`ma_share_mode_shared`** for automatic default open (**v2.4.106** — removed session-1 exclusive-first policy); **`SituationSetAudioDevice()`** still requests **exclusive** for explicit low-latency calls. **`is_miniaudio_device_internally_paused`** reset on shutdown (**v2.4.106**). |
| **Graphics re-init** | OpenGL / Vulkan teardown **`memset`** fixes are **separate** — they address stale handles after GPU cleanup; they do **not** replace audio correctness. |
| **Full harness AV (Vulkan, observed)** | **Addressed for shutdown/teardown (v2.4.53 — Bug 11)**: the dominant repro was **`SituationShutdown`** skipping cleanup → second **`SituationInit`** corrupted state. Full sequential **327/327** Vulkan (**v2.4.106**). **New** AVs need a fresh **debugger stack**. |
| **Harness stderr vs audio thread (v2.4.50)** | Concurrent **`fprintf(stderr, …)`** from the **miniaudio** callback / drivers and the harness **main thread** could corrupt CRT **`stderr`** state on Windows (truncated lines, AV). **Fixed** in **`tests/harness/sit_test_framework.h`** with a **critical section** around harness result lines. **`sit_test.exe --module audio`** is **green** (96 tests); full sequential suite **337/337** OpenGL (**v2.4.106**). |
| **MIDI graph destroy + harness gate (v2.4.51)** | **`SituationDestroyGraph`** loop used **`node_count`** as an upper index — **sparse** **`nodes[]`** skipped **MIDI** teardown (**`Pm_Close`** leak). **Fixed**: iterate **`0 .. SITUATION_MAX_NODES-1`**. **`sit_test`** opens hardware **`Pm_OpenInput`** only when **`SIT_TEST_OPEN_MIDI_HARDWARE`** is set; otherwise **`midi_list`** / API smoke paths cover CI. **`SituationDisableMidiControl`** before **`SituationDestroyGraph`** when hardware MIDI was opened. |

**Separate from Bug 6 (DSP routing)**: The node-graph vs **`active_voices`** / **`default_graph`** contract, optional graph-only policy, and shutdown ordering are tracked in **`doc/plan/AUDIO_NODE_COMPLETION_PLAN.md`** § *Canonical miniaudio callback pipeline* (see also **`doc/plan/PHASE_H_DETAILED_PLAN.md`** revision note).

---

### No-nonsense resolution order (do not skip audio as policy)

1. **Prove the fault** — Add **narrow logging** (or break under debugger) immediately **before/after** **`ma_context_get_devices`** and **`ma_device_init`** in **`_SituationSetAudioDeviceInternal`**, plus **`SituationInit`** step 7. Confirm whether failures are **hang**, **AV**, or **error return**. Run **OpenGL** full **`sit_test.exe`** vs **Vulkan** full suite on the **same machine** to see if behavior diverges (same audio code path).
2. **Complete device lifecycle** — Treat **`ma_device_stop` → `ma_device_uninit`** as mandatory; audit **`SituationStopAudioCapture`** / capture **`ma_device`** so no parallel device holds the endpoint during playback teardown.
3. **Re-init policy (product)** — For **automatic** default open on later **`SituationInit`**: prefer **deterministic** reopen (**shared** or **retry-after-delay** before exclusive) rather than infinite block. For **explicit** **`SituationSetAudioDevice`**, keep **exclusive** unless we add a **`SituationInitInfo`** flag for “shared output / editor safe.”
4. **Safety net without silence** — If **`ma_device_init`** must not block the main thread forever (frozen driver), a **timeout** path should **surface **`SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED`** and stderr guidance**, not silently pretend audio works.
5. **Escape hatch (tests only)** — Optional **defer auto-open** on re-init is acceptable **only** as a harness knob or documented opt-in — **not** the default product behavior for shipped games.

**Verification (closed 2026-05-23)**: OpenGL **`sit_test.exe`** **337/337**; Vulkan **`sit_test_vulkan.exe`** **327/327**; audio probe (7× init/shutdown, 2 s tone, master meter peak ≈ **0.56**); system playback still works after full harness. Ensure **`build\dll\*.dll`** is copied beside the exe — stale **`build\situation_opengl.dll`** can silently load **v2.4.104**. See **`doc/UPDATELOG.md`** v2.4.106.

**Optional follow-ups** (not blocking Bug 6 closure): **`ma_device_init`** timeout surfacing; **`SituationInitInfo`** flag for shared vs exclusive auto-start; harness test asserting non-zero **`SituationGetMasterOutputMeter`** during tone playback.

---

## Resolution Priority

| # | Bug | Tests | Effort | Status |
|---|-----|-------|--------|--------|
| 7 | VD composite (sampler + quad + readback) | 12 | — | ✅ FIXED |
| 8 | Uniform data flow | 4 | 1-2h | ✅ FIXED |
| 9 | Texture format (UV rect + use_texture) | 2 | 30 min | ✅ FIXED |
| 10 | Compute state leak (pipeline bind + SSBO flags) | 2 | 15 min | ✅ FIXED |
| 6 | Audio lifecycle + full sequential suite | (suite) | — | **✅ FIXED** (**v2.4.106**) — Win32 shared auto-start; harness **337/337** OpenGL + **327/327** Vulkan |
| 11 | Shutdown skipped teardown (multi-module AV) | full harness | — | ✅ **FIXED** (**v2.4.53**) — see **Bug 11** above |
| 12 | VMA leak at allocator destroy (shutdown defer) | shutdown | — | ✅ **FIXED** (**v2.4.54**) — see **Bug 12** above |
| — | Vulkan graphics module (86 tests) | **86/86** on **GTX 1070** | — | ✅ **Gate met** (**v2.4.94**); **regression discipline** — *Vulkan graphics — harness status* above |

**OpenGL graphics module: 81/81 ✅**  
**Vulkan graphics module: 86/86 ✅** (reference **GTX 1070**; see *Vulkan graphics — harness status*).  
**Full sequential suite**: **✅ 337/337** OpenGL + **327/327** Vulkan (**v2.4.106**). **Bug 11** + **Bug 12** address AV and VMA teardown; optional **`Leaked …`** warnings when tests skip explicit **`SituationDestroy*`**.

---

## Fixes Applied (Session 3)

### Bug 8 — Shader Uniform Data Flow [FIXED]
**Root cause**: `SituationSetShaderUniform` deferred uniform uploads to the soft command buffer, but `SituationAcquireFrameCommandBuffer` resets the buffer (packet_count=0, data_cursor=0). Uniforms set before frame acquisition were silently lost.

**Fix**: Changed to immediate `glProgramUniform*` calls (DSA — doesn't require program to be bound). Uniform values persist in the GL program object state until explicitly changed.

**File**: `sit/situation_impl_renderer.h` (SituationSetShaderUniform)

### Bug 9 — Texture Roundtrip / DrawTexture [FIXED]
**Root cause**: Two issues in `SIT_OP_DRAW_QUAD` execution:
1. `u_uv_rect` (location 5) was never uploaded — shader always sampled UV (0,0)
2. `u_use_texture` (location 6) was only set to 0 (disabled) when no texture bound, never set to 1 when a texture WAS bound

**Fix**: Added `glProgramUniform4fv` for UV rect and `glProgramUniform1i` for use_texture flag in the draw quad batch loop.

**File**: `sit/situation_impl_renderer.h` (SIT_OP_DRAW_QUAD case)

### Bug 10 — Compute Pipeline + Buffer Binding [FIXED]
**Root cause**: Three issues:
1. `SIT_OP_BIND_COMPUTE_PIPELINE` had no case in the execution switch — compute program was never bound
2. `SituationCmdBindDescriptorSetDynamic` didn't pass `usage_flags` to the command packet — SSBOs were always bound as UBOs
3. `SituationCmdBindComputeTexture` used wrong opcode (`SIT_OP_BIND_DESCRIPTOR_SET` instead of `SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING`) — storage images weren't bound

**Fix**: Added the missing case, passed usage_flags in the packet, and fixed the opcode for compute texture binding.

**Files**: `sit/situation_impl_renderer.h` (execution switch, SituationCmdBindDescriptorSetDynamic, SituationCmdBindComputeTexture)

### Buffer Update Immediacy [FIXED]
**Root cause**: `SituationUpdateBuffer` deferred writes to the soft command buffer, but `SituationGetBufferData` reads immediately via `glGetNamedBufferSubData`. Tests that update+readback without a frame cycle saw stale data.

**Fix**: Changed to immediate `glNamedBufferSubData` (DSA). Works regardless of frame state.

**File**: `sit/situation_impl_renderer.h` (SituationUpdateBuffer)

### Bug 6 — Re-Init GL State [Graphics portion FIXED]
**Root cause (GL portion)**: `_SituationCleanupOpenGL` deleted GL objects but didn't zero state fields. On re-init, guard checks like `if (ring_buffer_id != 0) return;` skipped re-creation. Also, ring buffer and MDI buffer were never cleaned up at all.

**Fix**: Added ring buffer/MDI buffer cleanup and `memset(&sit_render.gl, 0, sizeof(sit_render.gl))` at end of cleanup.

**Audio**: Handled under **Bug 6 — Audio & full sequential suite** (same Symptom bucket historically called “Bug 6 hang,” separate from GL IDs).

**File**: `sit/situation_impl_renderer.h` (_SituationCleanupOpenGL)

---

**Author**: Kiro  
**Status**: OpenGL graphics **81/81 ✅** (full module count may differ with SPIR-V tests — use harness printout). Vulkan **graphics**: **86/86 ✅** on **GTX 1070** (**v2.4.94**). Teardown: **v2.4.53** (**Bug 11**), **v2.4.54** (**Bug 12**). **Bug 6**: **✅ closed** (**v2.4.106**). Full sequential: **337/337** OpenGL + **327/327** Vulkan.

---

## Next Steps — Remaining Work

*(See **Remaining work (checklist)** after the Vulkan summary table for a compact prioritized list.)*

### Bug 6 — Supplementary tactics (after lifecycle + proof)

These are **secondary levers**, not substitutes for a correct **`stop → uninit → reopen`** contract:

| Tactic | Role |
|--------|------|
| **Shared mode on Win32 auto-start** (**v2.4.106**, **in tree**) | **`SituationInit`** step 7 **always** opens the default device in **shared** mode — avoids WASAPI exclusive hijack and system mute during harness/editor re-init. Explicit **`SituationSetAudioDevice`** remains **exclusive** for latency. |
| **Retry + short delay before exclusive** | If driver needs milliseconds after uninit, bounded retry is preferable to infinite block. |
| **Timeout around `ma_device_init`** | **Safety net** for hung drivers — must return **error** and log; never pretend success. |
| **Full `sit_audio` reset after uninit** | Careful **`memset`** only after ordered teardown — avoids stale flags; validate no double-free. |
| **Defer step-7 auto-open on re-init** | **Harness / opt-in only** — documented flag or `#ifdef` test path — **not** default for shipped titles. |

**Scope reminder**: Failure domain for classic Bug 6 is **main playback `ma_device_init`**, not MIDI, node graph, or capture — those use different entry points.

### Additional Findings (Not Bugs — Design Notes)

These were discovered during investigation and are worth noting for future work:

1. **Deferred vs Immediate API pattern**: Several APIs (`SituationSetShaderUniform`, `SituationUpdateBuffer`) were deferred to the command buffer but their readback counterparts were immediate. This mismatch is now fixed (both immediate), but if the render thread architecture is ever re-enabled, these need to be revisited with proper synchronization.

2. **`SIT_OP_SET_UNIFORM` is now dead code**: The command buffer replay case for `SIT_OP_SET_UNIFORM` still exists but is never triggered (uniforms are now set immediately). It can be removed in a future cleanup pass, or kept as a fallback if deferred uniforms are ever needed for Vulkan push constants.

3. **`SIT_OP_UPDATE_BUFFER` is now dead code**: Same situation — the OpenGL path of `SituationUpdateBuffer` no longer pushes this opcode. The execution case can be removed or kept for potential future use.

4. **Compute pipeline state tracking**: The new `SIT_OP_BIND_COMPUTE_PIPELINE` case sets `sit_render.gl.current_program_id` which is shared with graphics pipelines. If a compute dispatch is followed by a graphics draw without an explicit `SIT_OP_BIND_PIPELINE`, the graphics draw will use the compute program. This works today because tests always bind a graphics pipeline before drawing, but it's fragile. Consider adding a separate `current_compute_program_id` field.

5. **Quad shader `u_Texture` sampler binding**: The quad fragment shader declares `uniform sampler2D u_Texture` without an explicit `layout(binding=N)`. It defaults to texture unit 0, which is where `SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING` binds textures (via `glBindTextureUnit(idx, ...)` with idx=0). This works but is implicit — if the virtual bindless system ever changes the binding point, the quad shader will break silently.

---

## Vulkan Backend — Test Results (First Run, v2.4.41) *(historical)*

> **Note**: Vulkan **`sit_test_vulkan.exe --module graphics`** counts **86** tests (OpenGL-only entries excluded; includes Phase 30 SPIR-V). For the **current** scorecard (**86/86** on reference **GTX 1070**, **v2.4.94**) and closure narrative, see **Vulkan graphics — harness status** at the top of this file.

**Date**: 2026-05-08  
**Result**: ~43/81 passing (estimated — test crashed before completion)  
**Verdict**: Pre-existing issues, NOT regressions from OpenGL fixes  

All OpenGL fixes are inside `#if defined(SITUATION_USE_OPENGL)` blocks. The Vulkan backend was never previously tested with the harness (TEST_HARNESS_PLAN.md only checked "verify clean build" for Vulkan). These are newly-exposed pre-existing bugs.

### ✅ Vulkan Bug V1 — Shader Compilation Check (was ~20 tests) [FIXED v2.4.42]

**Root cause**: `SituationLoadShaderFromMemory` checked `blob.internal_result != shaderc_compilation_status_success`. Since `internal_result` is a pointer (`shaderc_compilation_result_t = struct*`) and `shaderc_compilation_status_success` is 0, this compared a non-NULL pointer to 0 — always true on success. The function returned -748 without ever compiling the fragment shader.

**Fix**: Changed to `if (!blob.data)` which correctly detects compilation failure (the function returns zeroed blob on error). Same fix applied to compute pipeline creation paths.

**Files**: `sit/situation_impl_renderer.h` (SituationLoadShaderFromMemory, SituationCreateComputePipelineFromMemory)

---

### ✅ Vulkan Bug V2 — Buffer Update Not Visible to Readback (3 tests) [FIXED v2.4.42]

**Root cause**: `SituationUpdateBuffer` Vulkan fallback (when `vmaMapMemory` fails for GPU-only memory) used the current frame's command buffer which may not be in recording state outside a frame.

**Fix**: Changed fallback to use `_SituationVulkanBeginSingleTimeCommands` + `vkCmdUpdateBuffer` + `_SituationVulkanEndSingleTimeCommands`. Also added staging buffer path for updates >64KB. Additionally changed SSBO allocation to `VMA_MEMORY_USAGE_CPU_TO_GPU` so `vmaMapMemory` succeeds directly.

**Files**: `sit/situation_impl_renderer.h` (SituationUpdateBuffer, _SituationVulkanCreateAndUploadBuffer)

---

### Vulkan Bug V3 — DrawQuad / DrawTexture vs Harness (2 tests) [FIXED — v2.4.44 core path]

**Symptom**: `draw_quad_red` and `draw_textured_checkerboard` may fail in the harness while rendering looks correct on screen.

**Root cause**: Same bucket as **V6**: CPU readback path. A dominant bug was **`screenshot_valid` cleared** when **`vkQueuePresentKHR`** triggered swapchain recreate in **`SituationEndFrame`** — **`_SituationVulkanCleanupSwapchain`** destroyed screenshot resources **after** resolve, so **`SituationLoadImageFromScreen`** saw an empty cache.

**Fix**: Do not destroy pre-present screenshot buffers inside **`_SituationVulkanCleanupSwapchain`**; **`_SituationVulkanEnsureScreenshotResources`** reallocates when extent/format changes.

**Affected tests**: draw_quad_red (verified), draw_textured_checkerboard — re-verify in full graphics module pass.

---

### ✅ Vulkan Bug V4 — VD Composite Crash (SIGSEGV) [FIXED v2.4.42]

**Root cause**: Three issues combined:
1. **VD depth image mipLevels**: `_SituationVulkanCreateImage` call passed `depth_format` (VkFormat enum ~124) as the `mipLevels` parameter instead of `1`. Created an image with 124+ mip levels.
2. **Nested render pass**: `SituationRenderVirtualDisplays` starts its own render pass, but callers already had a render pass active — illegal in Vulkan, caused SIGSEGV.
3. **VD render target render pass mismatch**: `SituationCmdBeginRenderPass` targeting a VD used `_SituationVulkanGetOrCreateRenderPass` which creates a new render pass incompatible with the VD's framebuffer.

**Fix**: 
1. Fixed mipLevels to `1` in VD depth image creation.
2. Added `vkCmdEndRenderPass` at start of composite + restart after composite completes.
3. Changed to use `vd->vk.render_pass` directly for VD render targets.

**Files**: `sit/situation_impl_vd.h`, `sit/situation_impl_renderer.h`

---

### ✅ Vulkan Bug V5 — Draw Metrics Overlay (1 test) [FIXED — v2.4.58+ stack]

**Symptom** (historical): `draw_metrics_overlay` — no overlay pixels detected in CPU readback.

**Root cause** (cumulative with **V6**): Screenshot / readback path parity — vertical row order for **`SituationLoadImageFromScreen`** on Vulkan vs OpenGL (**`SituationImageFlip`**, **v2.4.58**), plus the broader **v2.4.55–v2.4.61** readback and compositor fixes. Metrics text uses the same post-frame capture as other pixel tests.

**Retest** (**2026-05-10**, GTX 1070): **`--module graphics`** is **78/78**; **`draw_metrics_overlay`** is green on the reference run. Re-open **V5** only if a **regression** reappears on this or another GPU.

---

### Vulkan Bug V6 — Screenshot Readback Returns Black / Stale [FIXED — v2.4.44 + prior bundles]

**Symptom**: Rendering tests that verify pixel values may fail even when the window looks correct — historically “black” or wrong channel CPU buffers vs GPU output.

#### Implemented fixes (reference — see `doc/UPDATELOG.md` v2.4.43 / v2.4.44)

1. **Pre-present capture** (same idea as OpenGL Bug 2): Before `vkQueuePresentKHR`, record copy swapchain → host-visible staging; after fence, resolve into `screenshot_buffer`. `SituationLoadImageFromScreen` prefers this cache (see `situation_impl_decl.h` / `situation_impl_renderer.h` / `situation_impl_image.h`).

2. **BGRA vs RGBA**: Swapchain `VK_FORMAT_B8G8R8A8_*` stores **B,G,R,A** in memory; tests expect **R,G,B,A** like `glReadPixels(..., GL_RGBA)`. `_SituationVulkanCopyMappedColorToRGBA` normalizes on resolve and in `_SituationVulkanBlitImageToHostVisibleBuffer`.

3. **Render-thread race**: Replaced a single global flag with **`screenshot_copy_pending[SITUATION_MAX_FRAMES_IN_FLIGHT]`** so `EndFrame` does not clear another slot before `_SituationVulkanResolveScreenshotAfterSubmit` runs.

4. **Timing**: `SituationLoadImageFromScreen` uses **`vkDeviceWaitIdle`** before trusting the cache so the main thread does not race ahead of resolve.

5. **Dimension / image index**: Loader aligns dimensions with **`swapchain_extent`** for cache hits; fallback blit uses **`last_presented_image_index`** when valid.

6. **Projection / clip space**: Vulkan DLL build adds **`-DCGLM_FORCE_DEPTH_ZERO_TO_ONE`** (`build_situation.bat`, `build_tests.bat`) so `glm_ortho` matches Vulkan NDC.

7. **Quad push constants**: **`vkCmdPushConstants`** uses explicit **104** bytes (shader layout size); `sizeof(struct)` could be **112** with tail padding on some ABIs.

8. **Swapchain recreate vs cache (v2.4.44)**: **`_SituationVulkanCleanupSwapchain`** must **not** call **`_SituationVulkanDestroyScreenshotResources`**. After present, **`vkQueuePresentKHR`** may return **SUBOPTIMAL** / **OUT_OF_DATE**, triggering recreate **in the same `SituationEndFrame`** — destroying screenshot buffers there cleared **`screenshot_valid`** before the app called **`SituationLoadImageFromScreen`**. Extent changes are handled when **`_SituationVulkanEnsureScreenshotResources`** runs with the new **`swapchain_extent`**.

9. **Ring buffer alignment**: Clamp **`max_frames_in_flight`** to **`SITUATION_MAX_FRAMES_IN_FLIGHT`** so **`screenshot_copy_pending`** indices stay valid.

10. **Acquire stall / black window (post‑v2.4.44 plan update)**: **`vkAcquireNextImageKHR`** no longer uses **`UINT64_MAX`** — **`SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS`** (default **1 s**) — so a stuck acquire eventually returns **`VK_TIMEOUT`** and triggers recreate instead of freezing indefinitely. stderr timing lines when **`SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS`** thresholds are met.

#### Residual checks (if any harness pixel test still fails on a given GPU)

| Check | Idea |
|-------|------|
| **Fallback path** | Cache miss forces **`_SituationVulkanBlitImageToHostVisibleBuffer`** — wrong image index or layout on some drivers (see earlier Gates C–D). |
| **Format** | Extend **`_SituationVulkanCopyMappedColorToRGBA`** if the surface is not **`B8G8R8A8`** / **`R8G8B8A8`**. |
| **Validation** | Khronos validation around **`_SituationVulkanRecordScreenshotCopy`** (Gate B / barrier story in v2.4.43 narrative). |

**Gate A resolution (observed)**: Failures with **`screenshot_valid == false`** and **`shot_wh == 0×0`** correlated with swapchain cleanup destroying buffers post-present — fixed in **v2.4.44**.

**Affected tests** (historical): draw_quad_red, draw_textured_checkerboard, draw_metrics_overlay, VD composite pixel checks, etc.

**Priority**: Re-measure full Vulkan graphics module; escalate only if failures persist after **v2.4.44** on the same GPU.

---

### Vulkan Summary Table

| Bug | Category | Tests Affected | Priority | Status |
|-----|----------|---------------|----------|--------|
| V1 | Shader compilation check | ~20 | — | ✅ FIXED (v2.4.42) |
| V2 | Buffer update out-of-frame | 3 | — | ✅ FIXED (v2.4.42) |
| V3 | Quad/Texture rendering vs readback | 2 | — | ✅ FIXED (v2.4.44 — same cache path as V6) |
| V4 | VD composite crash | 3 | — | ✅ FIXED (v2.4.42) |
| V5 | Text/overlay rendering | 1 | — | ✅ **FIXED** — **`draw_metrics_overlay`** passes with **v2.4.58+** readback parity and later bundles (see **Vulkan Bug V5** above) |
| V6 | Screenshot readback (pre-present + RGBA + threading + swapchain recreate + acquire timeout) | ~20 | — | ✅ **FIXED** (v2.4.43 + **v2.4.44** + acquire-timeout mitigation); **v2.4.58** adds Vulkan/OpenGL row parity for quadrant sampling |
| — | **Harness stability** | full graphics / suite | P2 | **Graphics module ✅** — **86/86** Vulkan on **GTX 1070** (**v2.4.94**). **Bug 11** / **Bug 12** fixed for shutdown/VMA; **Bug 6** **✅ closed** (**v2.4.106**); full sequential **337/337** + **327/327** |

**Total Vulkan graphics failures (reference GPU)**: **0** / **78** as of **v2.4.61**. Historical triage list → *Vulkan graphics — harness status* (**Historical “last eight”**). **Regression**: treat any new failure as **P1** until classified.

**Crashes**: V4 SIGSEGV fixes remain in place. **Multi-module AV** from **`SituationShutdown`** skipping teardown — **Bug 11**, **fixed v2.4.53**. Any **new** fault needs a fresh stack trace.

---

## Remaining work (checklist)

| Item | Owner | Notes |
|------|-------|------|
| **Bug 6** — audio lifecycle + sequential suite | Library | **✅ Closed v2.4.106**: stop-before-uninit; Win32 auto-start **always shared**; pause flag reset on shutdown; harness **337/337** OpenGL + **327/327** Vulkan; audio probe confirms output meter. Optional: **`ma_device_init`** timeout, **`SituationInitInfo`** output policy, harness meter assertion. |
| **Vulkan** — graphics regression | QA / dev | After material renderer changes: **`build\sit_test_vulkan.exe --module graphics`** (**86** tests) — expect **86/86** on **GTX 1070**; **`--filter spirv`** when touching SPIR-V descriptors. Optional **Intel / AMD** spot-checks; file deltas in **`UPDATELOG`**. |
| **Vulkan** — validation layers | Dev | On **any** new pixel failure, run with Khronos validation around screenshot/acquire/present/VD composite paths. |
| **V5** | — | **Closed** — see **Vulkan Bug V5** (**v2.4.58+**). |
| **Intermittent Vulkan faults** | Dev | **Shutdown-skip AV** — fixed (**Bug 11 / v2.4.53**). If new crashes appear, capture stack (present-acquire ordering, driver variance). |
| **VMA teardown** | — | **Fixed v2.4.54** (**Bug 12**). **`Leaked Texture`** stderr still means “test didn’t destroy handle” — cosmetic unless **`[VMA LEAK]`** returns (re-open). |

**Next focus**: All **v2.5 gates satisfied** (**v2.4.200**). Audio pipeline review complete (Phases E0–H, Policy B, PCM Input). Maintain **Vulkan graphics 86/86** via regression runs after renderer changes. Re-run full sequential harness when touching init/teardown/audio. **Minor version bump** (v2.5.0) is a maintainer decision — coordinate with `doc/plan/v2.5-api-expansion.md` phased roadmap.
