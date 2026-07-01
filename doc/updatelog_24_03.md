# Situation UPDATELOG — v2.4.x (part 3 of 5)

Patches **2.4.201 "Error Propagation Phase 0"** through **2.4.300** (98 entries, oldest first).

Index: [`UPDATELOG.md`](UPDATELOG.md) · Previous: [`updatelog_24_02.md`](updatelog_24_02.md) · Next: [`updatelog_24_04.md`](updatelog_24_04.md)

---

## [v2.4.201 "Error Propagation Phase 0"] - 2026-06-05

### Description

**v2.4.201**: Errno table expansion — Phase 0 of the Error Propagation Remediation Plan. Adds 4 missing error codes that are prerequisites for Phase 1 implementation work (adding `_SituationSetErrorFromCode` calls to void/bool API functions that currently fail silently).

**Canonical version**: `sit/situation_base_version.h` → **2.4.201**.

### Library

- **`sit/situation_base_errno.h`**
  - Added `SITUATION_ERROR_WINDOW_STATE_FAILED` (-105): window state change rejected by GLFW (toggle fullscreen, maximize, minimize, restore, vsync).
  - Added `SITUATION_ERROR_WINDOW_PROPERTY_FAILED` (-106): window property set failed (title, size, position, opacity, icon).
  - Added `SITUATION_ERROR_APP_STATE_FAILED` (-107): application state transition failed (pause, resume, target FPS).
  - Added new section `SITUATION_ERRORS_IMAGE` with `SITUATION_ERROR_IMAGE_OPERATION_FAILED` (-580): image operation failed (crop, resize, flip, or save).
  - `SITUATION_ERRORS_IMAGE` wired into master `SITUATION_ERROR_TABLE` macro between `SITUATION_ERRORS_FONT` and `SITUATION_ERRORS_OPENGL`.
  - Range comment updated: `-500 to -599` now reads "Resource Management, Rendering Core, Fonts & Image".

### Plan

- **`doc/plan/ERROR_PROPAGATION_PLAN.md`**
  - Phase 0 fully detailed with per-entry rationale and insertion slots.
  - Phase 1 expanded with per-function checkboxes and explicit error code mapping for every failure path.
  - Phase 3 sub-phases (3A–3E) with complete call-site inventory per function.
  - Phase 0 marked complete.

---

---

---

---

---

---

## [v2.4.202 "Error Propagation Phase 1 & 2"] - 2026-06-05

### Description

**v2.4.202**: Error propagation complete for all existing API signatures. Every public `SITAPI` function that can fail now calls `_SituationSetErrorFromCode(...)` before returning its sentinel value. Users calling `SituationGetLastErrorMsg()` or `SituationGetLastErrorCode()` after a failure will receive an accurate diagnostic. No function signatures changed — this is fully non-breaking.

**Canonical version**: `sit/situation_base_version.h` → **2.4.202**.

### Library — Error Propagation (57 functions across 8 implementation files)

- **`sit/situation_impl_wdm.h`** (30 functions)
  - Window state setters (`SetWindowState`, `ClearWindowState`, `SetVSync`, `ToggleFullscreen`, `ToggleBorderlessWindowed`, `MaximizeWindow`, `MinimizeWindow`, `RestoreWindow`, `SetWindowFocused`): `NOT_INITIALIZED` guards; `ToggleBorderlessWindowed` also guards `DISPLAY_QUERY` on NULL video mode; `SetVSync` propagates Vulkan swapchain recreation errors.
  - Window property setters (`SetWindowTitle`, `SetWindowIcon`, `SetWindowIcons`, `SetWindowPosition`, `SetWindowSize`, `SetWindowMinSize`, `SetWindowMaxSize`, `SetWindowOpacity`): `NOT_INITIALIZED` guards; `SetWindowIcons` additionally checks `INVALID_PARAM` + `MEMORY_ALLOCATION`.
  - App lifecycle (`PauseApp`, `ResumeApp`, `SetTargetFPS`): `NOT_INITIALIZED` guards.
  - Display/screen getters (`GetScreenWidth`, `GetScreenHeight`, `GetRenderWidth`, `GetRenderHeight`, `GetMonitorCount`, `GetMonitorWidth`, `GetMonitorHeight`, `GetMonitorPhysicalWidth`, `GetMonitorPhysicalHeight`, `GetMonitorRefreshRate`): `NOT_INITIALIZED` guards; monitor-indexed getters additionally validate `INVALID_PARAM` on out-of-range monitor IDs.

- **`sit/situation_impl_vd.h`** (3 functions)
  - `SetVirtualDisplayDirty`, `GetVirtualDisplaySize`, `IsVirtualDisplayDirty`: `NOT_INITIALIZED` + `VIRTUAL_DISPLAY_INVALID_ID`; `GetVirtualDisplaySize` also checks `INVALID_PARAM` on NULL out-params.

- **`sit/situation_impl_threading.h`** (5 functions)
  - `CreateThreadPool`: `INVALID_PARAM` (NULL pool), `MEMORY_ALLOCATION` (queue alloc failure), `THREAD_CREATION_FAILED` (worker/IO thread spawn).
  - `DestroyThreadPool`, `DispatchParallel`, `WaitForAllJobs`, `DumpTaskGraph`: `INVALID_PARAM` on NULL pool.

- **`sit/situation_impl_timer.h`** (5 functions)
  - `TimerGetOscillatorState`, `TimerGetPreviousOscillatorState`, `TimerHasOscillatorUpdated`, `TimerPingOscillator`, `TimerGetOscillatorTriggerCount`: dual guards — `TIMER_SYSTEM` (not initialized) + `INVALID_PARAM` (ID out of range).

- **`sit/situation_impl_ctrl.h`** (1 function)
  - `_SituationGLFWFileDropCallback`: `MEMORY_ALLOCATION` on path array alloc failure + strdup failure.

- **`sit/situation_impl_io.h`** (2 functions)
  - `GetAppSavePath`: `INVALID_PARAM` (null/empty name), `MEMORY_ALLOCATION` (alloc paths), `DEVICE_QUERY` (HOME not set on POSIX).
  - `GetBasePath`: `DEVICE_QUERY` on `GetModuleFileNameW` failure (Win32).

- **`sit/situation_impl_image.h`** (6 functions)
  - `_SituationSaveImageBMP`: `INVALID_PARAM` + `MEMORY_ALLOCATION`.
  - `SituationImageCrop`: `INVALID_PARAM` (bad image/dimensions) + `MEMORY_ALLOCATION`.
  - `SituationImageDraw`, `SituationImageDrawAlpha`: `INVALID_PARAM` (invalid images).
  - `SituationImageDrawText`: `INVALID_PARAM` (invalid dst/font/text).
  - `SituationBlitRawDataToImage`: `INVALID_PARAM` (NULL params).

- **`sit/situation_impl_renderer.h`** (5 functions)
  - `GetRenderLatencyStats`: `NOT_INITIALIZED` (zeroes out-params).
  - `ExportRenderHistogram`: `INVALID_PARAM` (null buf / zero size).
  - `DrawMetricsOverlay`: `NOT_INITIALIZED`.
  - `DestroyRenderList`, `ResetRenderList`: `INVALID_PARAM` on NULL list.

### Library — fprintf Pairing (Phase 2)

- **`sit/situation_impl_threading.h`**: Worker/IO thread creation failure `fprintf(stderr)` calls now paired with `THREAD_CREATION_FAILED` error code (fprintf retained under `#ifdef SITUATION_DEBUG_THREADING` for debug builds).
- **`sit/situation_impl_renderer.h`**: Vulkan error `fprintf(stderr)` calls paired with corresponding error codes (`VULKAN_UNSUPPORTED`, `VULKAN_VALIDATION_LAYER_ERROR`, `VULKAN_RENDERPASS_FAILED`, `VULKAN_COMMAND_FAILED`).

### Plan

- **`doc/plan/ERROR_PROPAGATION_PLAN.md`**
  - Phase 1 (all 11 sub-phases 1A–1K) verified complete against source and marked `[x]`.
  - Phase 2 verified complete and marked `[x]`.
  - Phase 1 wrap-up section added with coverage summary, deliberate-gap rationale, and per-file accounting.
  - Phase 3 sub-phases reformatted with per-function `[ ]` checkboxes for execution tracking.
  - Status updated: Phase 1 & 2 complete; Phase 3 planned (next breaking change window).

### Notes

- **Non-breaking**: No function signatures changed. All `bool`/`void` return types preserved.
- **No unused-code risk**: Error codes `-105`, `-106`, `-107` (Phase 0) remain defined but intentionally unused — reserved for future backends that can report window/property failures.
- **10 distinct error codes** used across Phase 1: `NOT_INITIALIZED`, `INVALID_PARAM`, `MEMORY_ALLOCATION`, `TIMER_SYSTEM`, `DISPLAY_QUERY`, `VIRTUAL_DISPLAY_INVALID_ID`, `THREAD_CREATION_FAILED`, `THREAD_CYCLE`, `THREAD_QUEUE_FULL`, `DEVICE_QUERY`, `FILE_WRITE_FAILED`.

---

---

---

---

---

---

## [v2.4.203 "Error Propagation Phase 3 — Breaking Migration (Complete)"] - 2026-06-05

### Description

**v2.4.203**: Complete set of breaking return-type migrations — 32 public API functions changed from `bool`/`void` to `SituationError`. Callers that checked `if (!fn())` must now check `if (fn() != SITUATION_SUCCESS)`. Callers that ignored the return value (fire-and-forget) require no changes.

**Canonical version**: `sit/situation_base_version.h` → **2.4.203**.

### ⚠️ BREAKING CHANGES

**Phase 3D — Audio (1 function):**
- `SituationSoundExportAsWav` — `bool` → `SituationError`

**Phase 3A — File I/O (7 functions):**
- `SituationSaveFileText` — `bool` → `SituationError`
- `SituationCopyFile` — `bool` → `SituationError`
- `SituationDeleteFile` — `bool` → `SituationError`
- `SituationMoveFile` — `bool` → `SituationError`
- `SituationRenameFile` — `bool` → `SituationError`
- `SituationCreateDirectory` — `bool` → `SituationError`
- `SituationDeleteDirectory` — `bool` → `SituationError`

**Phase 3E — Renderer void promotions (5 functions):**
- `SituationCmdBindComputePipeline` — `void` → `SituationError`
- `SituationCmdDispatch` — `void` → `SituationError`
- `SituationCmdCopyBuffer` — `void` → `SituationError`
- `SituationReadBuffer` — `void` → `SituationError`
- `SituationDrawModel` — `void` → `SituationError`

**Phase 3B — Threading (13 functions):**
- `SituationCreateThreadPool` — `bool` → `SituationError`
- `SituationWaitForJob` — `bool` → `SituationError`
- `SituationAddJobDependency` — `bool` → `SituationError`
- `SituationAddJobDependencies` — `bool` → `SituationError`
- `SituationRefreshCpuTopology` — `bool` → `SituationError`
- `SituationGetCpuTopology` — `bool` → `SituationError`
- `SituationSetThreadAffinity` — `bool` → `SituationError`
- `SituationSetThreadAffinityEx` — `bool` → `SituationError`
- `SituationGetThreadAffinity` — `bool` → `SituationError`
- `SituationRefreshNumaTopology` — `bool` → `SituationError`
- `SituationGetNumaTopology` — `bool` → `SituationError`
- `SituationGetThreadPoolSnapshot` — `bool` → `SituationError`
- `SituationGetThreadPoolMetrics` — `bool` → `SituationError`

**Phase 3C — Rendering (6 functions):**
- `SituationAcquireFrameCommandBuffer` — `bool` → `SituationError`
- `SituationReloadShader` — `bool` → `SituationError`
- `SituationReloadTexture` — `bool` → `SituationError`
- `SituationReloadModel` — `bool` → `SituationError`
- `SituationReloadComputePipeline` — `bool` → `SituationError`
- `SituationSaveModelAsGltf` — `bool` → `SituationError`

### Migration Pattern

```c
// Old (bool)
if (!SituationDeleteFile(path)) { /* handle */ }
bool ok = SituationCreateDirectory(dir, true);

// New (SituationError)
if (SituationDeleteFile(path) != SITUATION_SUCCESS) { /* handle */ }
SituationError err = SituationCreateDirectory(dir, true);
```

For void-promoted functions, callers that previously ignored the return (fire-and-forget) need no changes — ignoring `SituationError` is valid C.

### Library

- **`sit/situation_api.h`** — 32 declarations updated (13 from 3D/3A/3E, 13 from 3B, 6 from 3C)
- **`sit/situation_impl_io.h`** — 7 file I/O functions migrated; `_SituationAsyncFileTextSaveWorker` internal caller fixed
- **`sit/situation_impl_audio.h`** — `SituationSoundExportAsWav` migrated with proper error codes (`INVALID_PARAM`, `AUDIO_INVALID_OPERATION`, `FILE_WRITE_FAILED`)
- **`sit/situation_impl_renderer.h`** — 11 functions migrated; `SituationAcquireFrameCommandBuffer` (widest blast radius: ~120 call sites across examples/tests), 5 Reload/Save functions, 5 void promotions
- **`sit/situation_impl_threading.h`** — 4 functions migrated (`CreateThreadPool`, `WaitForJob`, `AddJobDependency`, `AddJobDependencies`)
- **`sit/situation_impl_threading_topology.h`** — 5 functions migrated + `_SitEnsureTopology` helper fixed
- **`sit/situation_impl_threading_numa.h`** — 2 functions migrated + 4 internal callers fixed
- **`sit/situation_impl_threading_observability.h`** — 1 function migrated + 2 internal callers fixed (`DumpThreadPoolStatus`, `GetThreadingStatus`)
- **`sit/situation_impl_threading_scheduler.h`** — 1 function migrated
- **`sit/situation_impl_ctrl.h`** — Internal `SituationCreateThreadPool` caller fixed

### Tests & Examples

- **`tests/harness/test_filesystem.c`** — ~26 call sites updated; all 23 tests pass
- **`tests/harness/test_threading.c`** — ~25 call sites updated; all 21 tests pass
- **`tests/harness/test_graphics.c`** — ~50 call sites updated (AcquireFrame + SaveModelAsGltf)
- **`tests/harness/test_core.c`** — 4 call sites updated (AcquireFrame + GetThreadPoolSnapshot)
- **`tests/harness/test_transfer.c`** — 12 call sites updated
- **`tests/harness/test_virtual_display.c`** — 16 call sites updated
- **`tests/harness/sit_test_stereo_scope.c`** — 5 call sites updated
- **`tests/test_async_io.c`** — 1 call site updated
- **~45 example `.c` files** — mechanical `if (!fn())` → `if (fn() != SITUATION_SUCCESS)` and `if (fn())` → `if (fn() == SITUATION_SUCCESS)` across all examples
- 1 pre-existing failure (`viewport_index_zero_parity`) — headless GPU context limitation, unrelated to this change

### Plan

- **`doc/plan/ERROR_PROPAGATION_PLAN.md`** — All phases complete (0, 1, 2, 3D, 3A, 3E, 3B, 3C)

### Additional Fixes (same session)

- **`sit/k-term/kt_composite_sit.h`** — Fixed black screen: `KTerm_AcquireFrameCommandBuffer()` macro now checks `== SITUATION_SUCCESS` instead of treating 0 as truthy
- **`sit/situation_impl_renderer.h`** — Fixed FPS limiter CPU spin: replaced `Sleep(0)` busy-wait with `Sleep(1)` when >2ms remaining (15% → 3% CPU for idle vsync apps)
- **`sit/situation_api.h`** — `SITUATION_ENABLE_RENDER_THREAD` now auto-enables when `SITUATION_ENABLE_THREADING` is defined (compile-time; runtime still controlled by `init_info.render_thread_count`)
- **`examples/kterm_console.c`** — Fixed 16-color palette showcase alignment (`%-2d` for fixed-width labels); fixed title frame border off-by-2 spacing
- **`doc/situation_api.md`** / **`doc/situation_sdk.md`** — All 32 migrated function signatures updated from `bool` to `SituationError`

---

---

---

---

---

---

## [v2.4.204 "Errno Adoption (Phases 0-7)"] - 2026-06-06

### Description

**v2.4.204**: Systematic audit and wiring of unused error codes to their proper call sites. 26 error codes that were defined in `situation_base_errno.h` but never produced are now actively returned by the library when appropriate conditions occur. Audit scripts improved. Test helper aligned with `SituationError` return type.

**Canonical version**: `sit/situation_base_version.h` → **2.4.204**.

### Changes

**Errno Adoption — Platform & Windowing (Phase 1):**
- `SITUATION_ERROR_CLIPBOARD_FAILED` — now produced by `SituationGet/SetClipboardText` on GLFW error
- `SITUATION_ERROR_CURSOR_CREATION_FAILED` — now produced during cursor init if `glfwCreateStandardCursor` returns NULL
- `SITUATION_ERROR_WINDOW_STATE_FAILED` — now produced by `SituationApplyCurrentProfileWindowState` on GLFW error
- `SITUATION_ERROR_WINDOW_PROPERTY_FAILED` — now produced by title/size/position/opacity setters on GLFW error
- `SITUATION_ERROR_WINDOW_FOCUS_FAILED` — now produced by `SituationSetWindowFocused` on GLFW error
- `SITUATION_ERROR_DXGI_FAILED` — now produced by `SituationGetDeviceInfo` on DXGI query failures
- `SITUATION_ERROR_INPUT_DEVICE_DISCONNECTED` — now produced on gamepad disconnect events
- `SITUATION_ERROR_INPUT_MAPPING_INVALID` — now produced by `SituationSetGamepadMappings` on GLFW rejection
- `SITUATION_ERROR_INPUT_HAPTIC_FAILED` — now produced by `SituationSetGamepadVibration` on XInput failure
- `SITUATION_ERROR_DISPLAY_MODE_SET_FAILED` — replaces EOL `DISPLAY_SET` in `SituationSetDisplayMode`
- `SITUATION_ERROR_DISPLAY_MODE_UNSUPPORTED` — now produced for `DISP_CHANGE_BADMODE` on Windows
- `SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT_REACHED` — replaces EOL `VIRTUAL_DISPLAY_LIMIT` in `SituationCreateVirtualDisplay`
- `SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND` — now produced for valid-range but inactive slot in `SituationDestroyVirtualDisplay`

**Errno Adoption — Filesystem (Phase 2):**
- `SITUATION_ERROR_FILE_ACCESS_DENIED` — now produced (upgraded from EOL `PERMISSION_DENIED`) via platform error mapping
- `SITUATION_ERROR_PATH_INVALID` — now mapped from `ERROR_INVALID_NAME`/`ERROR_BAD_PATHNAME` on Windows

**Errno Adoption — Rendering Core (Phase 3):**
- `SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER` — now produced by shader uniform setters when no frame is active
- `SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE` — now guards `SituationCmdBeginRenderPass` against nested passes (GL + VK)
- `SITUATION_ERROR_NO_RENDER_PASS_ACTIVE` — now guards GL path of `SituationCmdEndRenderPass`

**Errno Adoption — Vulkan Backend (Phase 5):**
- `SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED` — replaces EOL `VULKAN_INSTANCE_FAILED` in init
- `SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE` — replaces EOL `VULKAN_DEVICE_FAILED` for GPU selection
- `SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED` — replaces EOL `VULKAN_DEVICE_FAILED` for logical device

**Errno Adoption — Fonts & Image (Phase 7):**
- `SITUATION_ERROR_FONT_LOAD_FAILED` — now produced by `SituationLoadFont`/`LoadFontFromMemory` on parse failure
- `SITUATION_ERROR_FONT_ATLAS_FULL` — now produced by `SituationBakeFontAtlas` when glyphs don't fit
- `SITUATION_ERROR_IMAGE_OPERATION_FAILED` — now produced by `SituationImageResize` on STB failure

**Phantom Fix (Phase 0):**
- Fixed `SITUATION_ERROR_IO` (undefined) → replaced with `SITUATION_ERROR_FILE_OPEN_FAILED` in Linux /proc path

**Test Harness Fix:**
- Fixed `graphics_test_begin_frame()` and `graphics_test_async_poll_shader_ready()` — aligned `bool`-style checks with `SituationError` return type (fixes 3 previously-broken graphics tests)

**Tooling:**
- `scripts/audit_errno.ps1` — improved strict pattern to detect error codes in variable assignments and helper function arguments; added comment stripping to prevent false phantom matches
- `scripts/audit_errno_report.ps1` — same improvements + candidate home analysis for unused errors
- `doc/ERRNO_USAGE_REPORT.md` — regenerated with 114 remaining (down from 140)
- `doc/plan/ERRNO_ADOPTION_PLAN.md` — phases 0-7 marked complete

---

---

---

---

---

---

## [v2.4.205 "Compute Virtual Displays"] - 2026-06-06

### Description

**v2.4.205**: Extends the Virtual Display system to support compute-shader-writable render targets. Previously, VDs could only be rendered into via rasterization (render pass + framebuffer). Now, VDs created with `SITUATION_VD_FLAG_COMPUTE_TARGET` skip depth buffer/render pass creation and expose their internal texture as a `SituationTexture` handle for direct compute shader writes. This enables subsystems like K-Term to render via compute dispatch into a compositable VD layer.

**Canonical version**: `sit/situation_base_version.h` → **2.4.205**.

### New API

- `SituationVDFlags` enum — `SITUATION_VD_FLAG_NONE`, `SITUATION_VD_FLAG_COMPUTE_TARGET`
- `SituationCreateVirtualDisplayEx(resolution, frame_time_mult, z_order, scaling_mode, blend_mode, flags, out_id)` — Extended VD creation with flags. Compute targets get `STORAGE` + `SAMPLED` + `TRANSFER_SRC` image usage (Vulkan: `VK_IMAGE_USAGE_STORAGE_BIT`; OpenGL: bindable via `glBindImageTexture`). No depth buffer, render pass, or framebuffer created for compute-only VDs.
- `SituationGetVirtualDisplayTexture(display_id, out_texture)` — Returns the VD's internal color texture as a `SituationTexture` handle. Valid for compute-target VDs; the returned handle is usable with `SituationCmdBindComputeTexture` and all standard texture APIs.

### Changes

- `SituationVirtualDisplay` struct gains `flags` (`SituationVDFlags`) and `texture_slot_index` (`int`) fields.
- `SituationCreateVirtualDisplay` is now a thin wrapper calling `SituationCreateVirtualDisplayEx` with `SITUATION_VD_FLAG_NONE` (fully backward compatible).
- Compute-target VDs register a `_SituationTextureSlot` view into the texture registry at creation, freed on destroy.
- `SituationDestroyVirtualDisplay` deactivates the texture registry slot for compute VDs before destroying backend resources. Guards depth image destruction (NULL for compute VDs).

### Backward Compatibility

- Existing code calling `SituationCreateVirtualDisplay` is unaffected — behavior is identical.
- No changes to `SituationRenderVirtualDisplays` compositing logic.
- No changes to existing VD tests (20/21 pass; 1 pre-existing headless failure unrelated).

---

---

---

---

---

---

## [v2.4.206 "Errno Adoption (Phases 8-10)"] - 2026-06-06

### Description

**v2.4.206**: Continues the errno adoption plan (Phases 8–10), wiring 21 previously never-produced error codes into their proper call sites across the audio subsystem, node graph, and device registry.

**Canonical version**: `sit/situation_base_version.h` → **2.4.206**.

### Phase 8 — Audio Subsystem (4 wired, 1 deferred)

- `SITUATION_ERROR_AUDIO_CAPTURE_NOT_AVAILABLE` — `SituationStartAudioCaptureEx` now returns this when `ma_device_init` reports `MA_NO_DEVICE` (no microphone found).
- `SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED` — Replaces generic `AUDIO_DECODING` in `SituationLoadSoundFromFile` (preload + stream paths) and `SituationLoadSoundFromStream`.
- `SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED` — Returned specifically when miniaudio reports `MA_NO_BACKEND` or `MA_FORMAT_NOT_SUPPORTED`.
- `SITUATION_ERROR_AUDIO_STREAM_ENDED` — Set atomically on `_SituationSound::last_status` when a non-looping stream reaches `MA_AT_END` (non-fatal, main-thread pollable).
- `SITUATION_ERROR_AUDIO_CONVERTER` — Deferred (no `ma_data_converter` usage in current code).

### Phase 9 — Audio Node Graph (7 wired, 4 deferred)

- `SITUATION_ERROR_NODE_GRAPH_NOT_INITIALIZED` — NULL graph guard on all node graph API entry points.
- `SITUATION_ERROR_NODE_NOT_FOUND` — Returned from `SituationDestroyNode`, `SituationCreatePatch`, `SituationSetControl`, `SituationGetControl` when node lookup fails.
- `SITUATION_ERROR_NODE_CHANNEL_MISMATCH` — `SituationCreatePatch` validates source/destination port channel counts for audio patches.
- `SITUATION_ERROR_NODE_PATCH_NOT_FOUND` — `SituationRemovePatch` returns this instead of generic error when no matching patch exists.
- `SITUATION_ERROR_NODE_CONTROL_OUT_OF_RANGE` — `SituationSetControl` now rejects values outside `[min, max]` instead of silently clamping.
- `SITUATION_ERROR_NODE_PROCESSING_FAILED` — `SituationProcessGraph` returns this for invalid processing state.
- `SITUATION_ERROR_NODE_DESERIALIZATION_FAILED` — All JSON parse failures in deserialization functions.

### Phase 10 — Audio Device Registry (7 wired, 3 deferred)

- `SITUATION_ERROR_DEVICE_REGISTRY_NOT_INITIALIZED` — Guard on `SituationGetDeviceMetadata`, `SituationGetDeviceMetadataByIndex`, `SituationIsDeviceRegistered`.
- `SITUATION_ERROR_DEVICE_TYPE_INVALID` — `SituationRegisterDeviceType` rejects NULL metadata.
- `SITUATION_ERROR_DEVICE_CATEGORY_INVALID` — `SituationValidateDeviceMetadata` validates category enum range.
- `SITUATION_ERROR_DEVICE_CONTROL_INVALID` — Validates control name/min/max/default in metadata.
- `SITUATION_ERROR_DEVICE_PORT_INVALID` — Validates `audio_channels` ≤ 8.
- `SITUATION_ERROR_DEVICE_QUERY_FAILED` — `SituationGetDeviceMetadataByIndex` for out-of-range index.
- `SITUATION_ERROR_DEVICE_CREATE_FAILED` — `SituationCreateNodeWithDevice` when `funcs->create` returns NULL.

### New Implementation

- `SituationDestroyPatch` — Implemented as legacy wrapper calling `SituationRemovePatch(... , false)`.

### Internal Changes

- `_SituationSound` gains `atomic_int last_status` field for non-fatal audio thread → main thread signaling.

### Backward Compatibility

- `SITUATION_ERROR_AUDIO_DECODING` is still returned by the old (generic) EOL path. New callers should check for `AUDIO_DECODER_INIT_FAILED` or `AUDIO_DECODER_FORMAT_UNSUPPORTED`.
- `SituationSetControl` now rejects out-of-range values instead of clamping. Callers that relied on silent clamping should pre-clamp values.
- Node graph NULL graph checks now return `NODE_GRAPH_NOT_INITIALIZED` instead of `NODE_ALLOCATION_FAILED`.

### Bugfixes

- **OpenGL: `_SituationGetMaxViewports` thread-context fix** — `glGetIntegerv(GL_MAX_VIEWPORTS)` was being called from the main thread after the GL context had migrated to the render thread. Without a current context, the call silently returned the local init value of 1, causing `SituationCmdSetViewportIndexed(cmd, 1, ...)` to incorrectly reject index 1 as out-of-range. Fixed by caching `GL_MAX_VIEWPORTS` into `sit_render.cached_max_viewports` during `_SituationInitOpenGL` (while the context is still current) and reading from cache in both `_SituationGetMaxViewports` and `SituationGetGraphicsCaps`.
- **Test: `render_virtual_displays` missing render pass** — The test called `SituationRenderVirtualDisplays(cmd)` without first beginning a render pass. The API requires an active main-window render pass for VD compositing (it draws quads into the current pass). Added the missing `SituationCmdBeginRenderPass` call.
- **Test: `sync_shader_after_async_cycle` Vulkan timeout** — The 600-frame polling budget (~1s) was insufficient for Vulkan's full async path (shaderc GLSL→SPIR-V compilation + `vkCreateGraphicsPipelines`) when preceded by an aborted compile that leaves the shaderc thread pool cold. Increased to 1200 frames (~2s) to accommodate real-world pipeline creation latency on first compile.

---

---

---

---

---

---

## [v2.4.207 "Split Device Info Queries"] - 2026-06-06

### Description

**v2.4.207**: Replaces the monolithic `SituationGetDeviceInfo()` platform logic with split query APIs. The deprecated aggregate function now composes its result from the new helpers (no duplicated OS queries).

**Canonical version**: `sit/situation_base_version.h` → **2.4.207**.

### New API

- `SituationCPUInfo`, `SituationGPUInfo`, `SituationMemoryInfo` — focused structs for CPU, GPU, and RAM queries.
- `SituationGetCPUInfo()`, `SituationGetGPUInfo()`, `SituationGetMemoryInfo()` — split device-info queries (platform logic lives here once).
- `SituationGetStorageDeviceCount()`, `SituationGetStorageDevice()` — storage volume enumeration.
- `SituationGetNetworkAdapterCount()`, `SituationGetNetworkAdapterName()` — network adapter enumeration.
- `SituationGetInputDeviceCount()`, `SituationGetInputDeviceName()` — input device enumeration.

### Changes

- `SituationGetDeviceInfo()` — thin deprecated wrapper; copies split-query results into `SituationDeviceInfo` plus GLFW monitor summary.

### Backward Compatibility

- `SituationGetDeviceInfo()` remains available and returns the same aggregate struct; callers should migrate to split queries to avoid deprecation warnings.

---

---

---

---

---

---

## [v2.4.208 "Audio Preload Resample & Vulkan Async Shader Hardening"] - 2026-06-06

### Description

**v2.4.208**: Fixes preloaded audio playing at wrong speed for non-48kHz sources, hardens Vulkan async shader cleanup against shutdown hangs, and removes resize capability from Demon Hunt.

**Canonical version**: `sit/situation_base_version.h` → **2.4.208**.

### Fixes

- **Audio preload sample rate mismatch** — `SituationLoadSoundFromFile` with `SITUATION_AUDIO_LOAD_FULL` now resamples to the device output rate (was decoding at native rate, causing e.g. 96kHz WAV to play at half speed on 48kHz devices).
- **Vulkan async shader shutdown hang** — `_SituationVulkanFreeAsyncShaderLoad` no longer spins forever if the thread pool is dead; adds a bounded spin count bailout to prevent infinite hangs during `SituationShutdown` when a leaked shader has `compile_done == 0`.
- **`sync_shader_after_async_cycle` Vulkan test** — increased poll budget from 1200 to 3000 frames to accommodate shaderc compile latency on the worker thread.

### Changes

- `demon_hunt.c` — window is no longer resizable (`SITUATION_FLAG_WINDOW_RESIZABLE` removed from init flags).

### Test Harness

- Added 8 format-specific audio playback tests: `load_play_mp3`, `load_play_ogg`, `load_play_flac`, `load_play_wav`, `stream_mp3`, `stream_ogg`, `stream_flac`, `stream_wav`. Each plays for 1.5 seconds; OGG tests skip gracefully when Vorbis decoder is unavailable.

---

---

---

---

---

---

## [v2.4.209 "Node Graph FX Wet/Dry Gain Staging"] - 2026-06-06

### Description

**v2.4.209**: Corrects excessive wet-signal and effect-input levels in the audio node graph. Spring/Studio reverb wrappers now deinterleave stereo buffers correctly; the main Reverb effect normalizes comb-filter sum and aligns tank input gain with Freeverb-scale staging.

**Canonical version**: `sit/situation_base_version.h` → **2.4.209**.

Further per-effect tuning may follow after deeper listening / analysis.

### Fixes

- **Spring Reverb & Studio Reverb node wrappers** — `device_wrappers.h` treated interleaved `L,R,L,R…` buffers as planar `L…` / `R…` arrays (`buffer` / `buffer+1`), corrupting input and output and making wet paths much too hot. Processing now deinterleaves → planar DSP → reinterleaves (same pattern as Chorus).
- **Reverb (`reverb.h`)** — wet tail was ~8× hot (8 comb outputs summed without normalization) plus a 1.5× early-reflection boost; tank input gain was 0.08 vs ~0.015 in classic Freeverb-style engines. Comb sum is normalized, ER boost removed, input gain lowered, default dry aligned to registry (0.7).

---

---

---

---

---

---

## [v2.4.210 "Configurable Screenshot Format"] - 2026-06-06

### Description

**v2.4.210**: Screenshot output format is now a library-wide setting (default: BMP). Adds enum, table-driven extension lookup, format setter/getter, and smart path handling.

**Canonical version**: `sit/situation_base_version.h` → **2.4.210**.

### New API

- `SituationScreenshotFormat` enum — `SIT_SCREENSHOT_BMP` (default), `SIT_SCREENSHOT_PNG`, `SIT_SCREENSHOT_JPG`, `SIT_SCREENSHOT_TGA`.
- `sit_screenshot_format_ext[]` — static table mapping each format to its file extension string.
- `SituationSetScreenshotFormat(format)` — set the default output format.
- `SituationGetScreenshotFormat()` — query the current format setting.

### Changes

- `SituationTakeScreenshot(fileName)` — reworked:
  - `NULL` or empty → auto-generates timestamped name with configured extension.
  - Name with a recognized extension (`.bmp`, `.png`, `.jpg`, `.tga`) → used as-is.
  - Name without extension or with unrecognized extension → configured extension appended.
  - Supports BMP, PNG, JPG (quality 90), TGA via stb_image_write.
  - Default format changed from PNG-only to BMP.

---

---

---

---

---

---

## [v2.4.211 "GLTF Model Loader Enabled"] - 2026-06-06

### Description

**v2.4.211**: Enables `SituationLoadModel` for both OpenGL and Vulkan backends by compiling cgltf into the DLL. Fixes two stale API calls in the loader. Adds a dedicated model loader test module exercising BoomBox.glb with a rotating render.

**Canonical version**: `sit/situation_base_version.h` → **2.4.211**.

### Fixes

- **`SituationLoadModel` was dead code** — the DLL was built without `CGLTF_IMPLEMENTATION`, so model loading always returned `SITUATION_ERROR_NOT_IMPLEMENTED`. Added cgltf to `situation_impl_deps.h` and the OpenGL include path (`-Iext\cgltf`) in `build_situation.bat`.
- **`SituationLoadModel` API mismatches** — `SituationLoadImage()` and `SituationCreateMesh()` signatures had changed but the model loader still used the old calling conventions. Updated to current `(path, &out)` style.

### New

- **`test_model_loader` harness module** — 5 tests: load BoomBox.glb, draw rotating (1.5s) + pixel verify, mesh data access, double load, load/unload cycle. Works on both OpenGL and Vulkan.

### Build

- `build_situation.bat` OpenGL step: added `-Iext\cgltf`.
- `build_tests.bat`: added `tests/harness/test_model_loader.c` to source list.

---

---

---

---

---

---

## [v2.4.212 "Chorus Stability & Graph Output Staging"] - 2026-06-06

### Description

**v2.4.212**: Stops chorus/echo runaway in the node graph, hardens master-bus summing for stereo FX that only fill output port 0, and aligns the effects-heard harness level guardrails with wet FX captures.

**Canonical version**: `sit/situation_base_version.h` → **2.4.212**.

Builds on **v2.4.209** reverb wet/dry staging; per-effect listening may still need tuning.

### Fixes

- **4-stage Chorus (`chorus_4stage.h`)** — four modulated taps were summed at full scale into a single delay line, so feedback near 1.0 could explode levels. Stage wet sum is now normalized (×0.25) and outputs are soft-limited to ±2.0.
- **Echo (`echo.h`)** — parallel dry/wet mix output soft-limited to ±2.0 as a safety rail on the delay tap path.
- **Node graph master bus (`node_graph_process.h`)** — output port buffers are cleared each block before processing; the master bus sums only **output port 0** (devices register two logical stereo outs but wrappers write port 0 only).

### Test harness

- **`sit_test_audio_levels`** — effect profile limits raised (peak 1.65 / RMS 1.05); non-finite and runaway (>4.0) detection.
- **`test_audio_effects_heard.c`** — wet captures no longer use tone-only limits mid-capture; overdrive/chorus test controls and reverb wet-sweep thresholds adjusted for normalized reverb tail.

---

---

---

---

---

---

## [v2.4.213 "SPIR-V Layout Profile UBO_SSBO_SAMPLER + Materials"] - 2026-06-06

### Description

**v2.4.213**: Adds `SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER` — a new SPIR-V pipeline layout profile for Vulkan that extends UBO_SSBO with a combined image sampler at descriptor set 2. Also implements **Phase 2: Material System** for the Demon Hunt demo — 7 material types with per-wall shading variety.

**Canonical version**: `sit/situation_base_version.h` → **2.4.213**.

### Library changes

- **`sit/situation_api.h`**: New enum value `SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER` (set 0 UBO, set 1 SSBO, set 2 combined image sampler + 128B push constants).
- **`sit/situation_impl_decl.h`**: New field `graphics_spirv_layout_ubo_ssbo_sampler` on the Vulkan render state struct.
- **`sit/situation_impl_renderer.h`**:
  - `_SituationVulkanInitGraphicsSpirvLayouts()` creates the 3-set pipeline layout (reuses `text_sampler_layout` for set 2).
  - `_SituationVulkanBuildGraphicsPipelinesOnSlot()` routes the new profile to the cached layout.
  - Descriptor binding validation: new `case SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER` allows sets 0–2.
  - `SituationCmdBindTextureSet()` accepts set 2 via `single_sampler_descriptor_set` (same path as set 1).
  - Range checks updated from `> UBO_SSBO` to `> UBO_SSBO_SAMPLER` in both sync and async load paths.
  - Cleanup destroys the new pipeline layout.

### Demo (examples only)

- **`examples/demon_hunt.c`**:
  - Shader load switched to `SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER`; feedback texture bind at set 2 active.
  - **Phase 2 Material System**: `SkySceneSsboHeader` extended with `material_rows[128]` (4-bit material IDs, 8 cells per int). New `g_material_map[32][32]` populated by `map_assign_materials()` after level generation. Materials: Stone (50%), Wood (20%), Metal (15%), Rusted Metal (10%), Bone (5%) on hunt levels; arena levels bias toward Metal + Rusted Metal. Exit-adjacent walls are Emissive.
  - `sky_pack_material_rows()` packs the material map into bitfield format for SSBO upload.
- **`examples/demon_hunt_sky.fs`**:
  - `DH_ENABLE_FRAME_FEEDBACK` = 1; `DH_ENABLE_MATERIALS` = 1.
  - SSBO extended with `materialRows[128]`.
  - New `extract_material(ivec2 cell)` uses `bitfieldExtract()` for efficient 4-bit extraction.
  - New `shade_material()` function (76 GLSL lines): Stone (hash-perturbed normals), Metal (specular + Fresnel), Flesh (wrap lighting + red shift), Emissive (pulsing green glow), Wood (anisotropic grain), Bone (hard specular + crevice darkening), Rusted Metal (patchy orange tint + rough specular).
  - Material extraction + shading applied after DDA wall hit in `main()`.
  - Shader instruction count: +243 SPIR-V instructions (~0.25% increase from 95,801 → 96,044).

---

---

---

---

---

---

## [v2.4.214 "Static Build System + Async Shader UAF Fix"] - 2026-06-07

### Description

**v2.4.214**: Overhauls the entire build model. Examples and the test harness now link against pre-built static libraries (`.a`) or DLLs instead of recompiling the full library on every build. Static builds produce self-contained exes with no external DLL dependency. Fixes a use-after-free race in the Vulkan async shader compile worker. Promotes `SituationTopologicalSort` to the public API. Reorganizes build output into `build/dll/`, `build/tests/`, `build/examples/`.

**Canonical version**: `sit/situation_base_version.h` → **2.4.214**.

### Build output layout

```
build/
├── dll/        ← library artifacts: situation_*.dll  situation_*.a  situation_*.lib
├── tests/      ← test harness: sit_test_opengl.exe  sit_test_vulkan.exe
├── examples/   ← example exes (+ DLL copies for DLL-linked builds)
└── *.exe       ← probe/bench tools (unchanged)
```

### Build system

- **`build_situation.bat`** — new `static-opengl` / `static-vulkan` targets produce `build/dll/situation_*.a` via `ar`. Static builds omit `SITUATION_BUILD_SHARED` so `SITAPI` resolves to nothing. Vulkan archive bundles `vma_wrapper.o` + `tinycthread.o`. DLL targets unchanged.
- **`build_tests.bat`** — rewritten. No argument → shows usage (no silent default). Four modes:
  - `static-opengl` / `static-vulkan` — link against `.a`, output `build/tests/sit_test_opengl.exe` / `sit_test_vulkan.exe`, no DLL needed at runtime. **Recommended.**
  - `opengl` / `vulkan` — DLL-linked fast build; use `run_tests.bat` to run.
  - Renamed from `sit_test.exe` → `sit_test_opengl.exe` / `sit_test_vulkan.exe`.
  - Vulkan static: per-file gcc compile + g++ link (avoids C++ header contamination, pulls shaderc/VMA from archive).
- **`run_tests.bat`** — new launcher. Requires explicit `opengl` or `vulkan` argument. Prepends `build/dll` to PATH for DLL-linked builds. Static builds can be run directly.
- **`build_examples.bat`** — rewritten. Modes: `opengl` / `vulkan` (DLL-linked, DLL copied next to exe), `static-opengl` / `static-vulkan` (self-contained). `demon_hunt` blocked on all OpenGL modes.
- **`examples/*.c`** — removed `#define SITUATION_IMPLEMENTATION` from ~63 files. `text_showcase.c` stale guard removed. `mt_safety_demo.c` duplicate include fixed.

### API

- **`sit/situation_api.h`** — `SituationTopologicalSort(SituationAudioGraph*)` added to the node graph public API. Was internal-only; explicit calls now redundant since `CreateNode`/`DestroyNode`/`CreatePatch`/`RemovePatch` all sort internally.
- **`sit/aud/node_graph_impl.h`** — `SituationTopologicalSort` marked `SITAPI` for DLL export.

### Fixes

- **`sit/situation_impl_renderer.h` — Vulkan async shader compile UAF**:
  - **Root cause**: `_SituationVulkanFreeAsyncShaderLoad` spin bailout (500k yields) freed `ctx` unconditionally even if the worker was still alive. Next `SIT_MALLOC` could reuse the address; old worker then corrupted the new ctx fields — `fs_src` read as 0x01, producing `async_fragment: error: '☺' : unexpected token`.
  - **Fix**: Bailout now writes `-2` sentinel to `compile_done` and returns without freeing. Worker uses `atomic_compare_exchange_strong` — loses CAS → self-frees. Exactly one party frees `ctx`.
  - **Test**: `graphics.sync_shader_after_async_cycle` passes consistently on both backends.

### Verification

- `build\tests\sit_test_vulkan.exe` — **481/484** (3 pre-existing: `maximizer` control IDs, `kterm` example not built, 1 audio flaky).
- `build\tests\sit_test_opengl.exe` — **492/494** (2 pre-existing: `maximizer`, `kterm`).
- Both exes verified self-contained via `objdump` — no `situation_*.dll` dependency.

---

---

---

---

---

---

## [v2.4.215 "DestroyGraph Audio Race Fix"] - 2026-06-07

### Description

**v2.4.215**: Fixes a use-after-free race in `SituationDestroyGraph` that caused access violations (null dereference at `0x0000000000000000`) when destroying a graph immediately after `SituationSetActiveGraph(nil)`. Also hardens the Odin `hello_situation` example against machines where PortMidi / virtual MIDI loopback is unavailable.

**Canonical version**: `sit/situation_base_version.h` → **2.4.215**.

### Fixes

- **`sit/aud/node_graph_impl.h` — `SituationDestroyGraph` TOCTOU race**:
  - **Root cause**: The previous sequence was: null `active_graph`, then `_SituationWaitUntilAudioCallbackIdle()`. This had a window where the audio thread could start a *new* callback tick between the null store and the wait — if the audio thread read the old cached graph pointer in that window, it entered `SituationProcessGraph` one final time while DestroyGraph was already freeing the graph's node buffers, causing a null dereference on freed memory.
  - **Fix**: Double-wait pattern — wait for idle *before* detaching the graph (ensures no in-progress callback holds a reference), then null `active_graph` / `default_graph`, then wait for idle again (ensures no new callback started with this graph). Free proceeds only after both waits.
  - **Observed crash**: `W32/0xC0000005` access violation reading `0x0000000000000000` in `SituationDestroyGraph` — null dereference on a freed node's port buffer while audio thread was mid-processing.

- **`wrappers/odin/examples/hello_situation/hello.odin`**:
  - `SituationSetupVirtualMidiLoopback` return value is now checked. If PortMidi is unavailable (no virtual MIDI support on the machine), MIDI is silently skipped and the example runs without it. Previously the unchecked failure left `midi_device_id = -1` and the code still called `SituationEnableMidiControl` with auto-select, which could fail on systems with no MIDI devices.
  - `defer` teardown guards `SituationTeardownVirtualMidiLoopback` behind `audio_ok` to match the setup path.

---

---

---

---

---

---

## [v2.4.216 "Window State Flag Cache"] - 2026-06-07

### Description

**v2.4.216**: `SituationGetCurrentActualWindowStateFlags` was querying multiple GLFW attributes on every call, causing per-frame stalls when called more than once per frame (e.g. for HUD display + VSync toggle check). The function is now O(1) — it returns a cached value that is refreshed exactly once per frame by `SituationPollInputEvents` and immediately invalidated by `SituationSetWindowState` / `SituationClearWindowState`.

**Canonical version**: `sit/situation_base_version.h` → **2.4.216**.

### Changes

- **`sit/situation_impl_decl.h`** — new field `cached_window_state_flags` (u32) on the global state struct.
- **`sit/situation_impl_wdm.h`**:
  - `_SituationComputeWindowStateFlags()` — internal static function containing the original GLFW multi-attribute query logic.
  - `SituationGetCurrentActualWindowStateFlags()` — now returns `sit_gs.cached_window_state_flags` (O(1)). Falls back to a live compute on first call before `SituationPollInputEvents` has run (cache is zero).
  - `SituationSetWindowState()` / `SituationClearWindowState()` — recompute and store the cache after applying the new profile, so callers see the updated state immediately without waiting for the next poll.
- **`sit/situation_impl_ctrl.h`** — `SituationPollInputEvents` calls `_SituationComputeWindowStateFlags()` immediately after `glfwPollEvents()` to refresh the cache once per frame.
- **`wrappers/odin/examples/hello_situation/hello.odin`** — comment updated to reflect the library-level fix; per-frame workaround comments removed.

---

---

---

---

---

---

## [v2.4.218 "STL Model Loader, Demon Hunt Visual Bolster Fixes"] - 2026-06-07

### Description

**v2.4.218**: Adds `SituationLoadModelFromSTL()` for loading binary and ASCII STL mesh files without any external dependency. Fixes incomplete and incorrect Phase 2/6 implementations in the Demon Hunt visual bolster work: enables the per-wall material system, replaces the stub bloom with proper Kawase feedback bloom, adds film grain and shadow dithering, and fixes a shadow dithering regression that broke projectile trajectory visuals.

**Canonical version**: `sit/situation_base_version.h` → **2.4.218**.

### New API

- **`SituationLoadModelFromSTL(file_path, smooth_normals, out_model)`** — loads a 3D model from a `.stl` file:
  - Auto-detects binary vs ASCII STL (handles the binary-files-starting-with-"solid" edge case).
  - Produces a single-mesh `SituationModel` in stride-32 layout `[Px Py Pz Nx Ny Nz U V]` — compatible with all existing draw paths; no new pipeline variants required.
  - `smooth_normals = false` (default): flat shading, one vertex per triangle corner, face normal from file. O(1) per vertex.
  - `smooth_normals = true`: coincident vertices (ε = 1e-5) merged and normals averaged — smooth appearance. O(n²) merge, suitable for < ~100k triangles.
  - UVs are zeroed (STL carries no UV data). `base_color_factor` defaults to white, `roughness_factor` to 0.8.
  - `SituationUnloadModel`, `SituationDrawModel`, and `SituationReloadModel` all work on the result unchanged.
  - No new external dependency — STL parsing is ~200 lines of C inline in `situation_impl_renderer.h`.

### Demon Hunt visual bolster fixes (`examples/demon_hunt_sky.fs`, `examples/demon_hunt.c`)

- **Phase 2 — Material system enabled**: `DH_ENABLE_MATERIALS` flipped from `0` to `1`. Per-wall material shading (Stone, Metal, Flesh, Emissive, Wood, Bone, Rusted Metal) is now active at runtime; all material packing code was already running but contributing nothing visually.
- **Phase 6 — Bloom rewritten to spec**: replaced the placeholder current-frame brightness boost with the specified 2-iteration Kawase blur from the feedback texture (8 diagonal samples: 4 at 1.5px + 4 at 3.5px at half weight, luminance-thresholded). The feedback texture is now actually used for bloom. `#else` fallback retained for `DH_ENABLE_FRAME_FEEDBACK 0` builds.
- **Phase 6 — Film grain added**: `(hash12(gl_FragCoord.xy + fract(frame.uTime) * 137.0) - 0.5) * 0.025` applied after vignette. Multiplier 137 is coprime with typical hash periods to prevent temporal repetition.
- **Phase 6 — Shadow dithering added and placed correctly**: screen-space jitter (`±0.015` units) applied to the wall shadow ray origin at the wall call site only — not inside `pristine_shadow()`. This was the source of a projectile trajectory regression: the dither was previously applied inside `pristine_shadow()` which is called for sprite, portal, and floor shading as well, corrupting their world-space position lookups.
- **Phase 1 cleanup**: removed dead `sprite_light_mul()` function (18 lines, zero call sites post-CPU-sprite retirement).

---



### Description

**v2.4.217**: Upgrades the Odin `hello_situation` example with a full delay (Echo) node in the signal chain plus interactive controls for delay wet and feedback. Adds timestamped test results files to `run_tests.bat` so every run is persisted for later consultation.

**Canonical version**: `sit/situation_base_version.h` → **2.4.217**.

### Tooling

- **`run_tests.bat`** — test output is now teed to `build/tests/results/YYYYMMDD_HHMM_backend.txt` via PowerShell `Tee-Object`. Every run produces a new timestamped file; the folder accumulates all runs. Output still prints to the console simultaneously. Results folder is created automatically.

### Odin bindings / examples

- **`wrappers/odin/examples/hello_situation/hello.odin`** — upgraded example:
  - Signal chain extended: `ToneSynth -> Echo -> Reverb`. Echo node uses `SITUATION_NODE_ECHO` with controls `0=delay_time`, `1=feedback`, `2=wet_level`.
  - New key bindings: `]`/`[` — delay wet up/down; `P`/`O` — delay feedback up/down (capped at 0.95 to prevent runaway).
  - HUD now shows two lines: system status (FPS, VSync, Audio) and FX levels (Reverb %, Delay %, Delay FB %).
  - `SituationGetCurrentActualWindowStateFlags()` comment updated to reflect the library-level O(1) cache added in v2.4.216; per-frame workaround removed.
  - Window title updated to include new key hints.
  - All comments restored and expanded (control ID documentation, signal chain explanation, shader section descriptions).

---

---

---

---

---

---

## [v2.4.219 "OBJ Model Support"] - 2026-06-08

### Description

**v2.4.219**: Adds `SituationLoadModelFromOBJ()` for loading Wavefront `.obj` files and associated `.mtl` material libraries/textures without any new external dependencies, utilizing the vendor-included `tinyobj_loader_c` parser. Audits and resolves `SituationReloadModel` routing for both STL and OBJ models, ensuring model hot-reloading functions correctly across all three formats.

**Canonical version**: `sit/situation_base_version.h` → **2.4.219**.

### New API

- **`SituationLoadModelFromOBJ(file_path, out_model)`** — loads a 3D model from a Wavefront `.obj` file:
  - Automatically parses associated material files (`.mtl`) and loads referenced textures (diffuse maps, normal maps) from disk.
  - Packs the parsed geometry (positions, normals, texture coordinates) into the standard stride-48 PBR vertex layout (`[Px Py Pz Nx Ny Nz Tx Ty Tz Tw U V]`) with defaulted tangents.
  - Performs index-based vertex deduplication using a hash table map to generate optimized index and vertex buffers.
  - Seamlessly integrates with existing draw, reload, and unload systems via `SituationDrawModel`, `SituationReloadModel`, and `SituationUnloadModel`.

### Fixes / Hardening

- **`SituationReloadModel`** — Corrected hot-reloading logic for STL and OBJ formats. Previously, reloading a model slot always invoked the GLTF-specific `SituationLoadModel`, which failed for other file types. Added loader-format tracking (`is_stl`, `is_obj`, and `stl_smooth_normals` stored on `_SituationModelSlot`) to dispatch reload requests to `SituationLoadModelFromSTL` or `SituationLoadModelFromOBJ` with original options.

---

---

---

---

---

---

## [v2.4.220 "Vulkan Graphics Viewport Parity"] - 2026-06-08

### Description

**v2.4.220**: Unifies tracked user-draw viewport hygiene with the OpenGL-parity convention already used by **`SituationCmdBeginRenderPass`**, internal 2D draws, and the VD compositor (**v2.4.189**). **`_SitVulkanApplyGraphicsViewportScissor`** no longer re-applies a standard Vulkan viewport (`y = 0`, positive height) before **`SituationCmdDraw`** / indexed / indirect paths — it now calls **`_SitVulkanFillViewport2DOpenGLParity`** so user draws stay in the same top-left coordinate frame as the rest of the active render pass.

**Canonical version**: `sit/situation_base_version.h` → **2.4.220**.

**Builds on**: **v2.4.175** (viewport/scissor hygiene on tracked raster draws) and **v2.4.189** (OpenGL-parity viewport for **`BeginRenderPass`** and internal 2D). Rebuild **`build_situation.bat vulkan`** after pulling — delete **`build/situation_dll_vulkan.o`** if the DLL object is stale (header-only renderer change).

### Library changes

#### `_SitVulkanApplyGraphicsViewportScissor` (`sit/situation_impl_renderer.h`)

Tracked user draws re-apply full render-area viewport + scissor from **`_SitVulkanApplyTrackedRasterDynamics`** (every **`SituationCmdDraw`**, indexed/indirect draw, and pipeline rebind). **v2.4.175** added that hygiene path with a **standard** viewport:

```c
VkViewport vp = {0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f};
```

**v2.4.220** replaces it with the same OpenGL-parity helper used everywhere else in the pass:

```c
VkViewport vp;
_SitVulkanFillViewport2DOpenGLParity((float)extent.width, (float)extent.height, &vp);
```

(`_SitVulkanFillViewport2DOpenGLParity` sets `y = height`, `height = -height` so Situation `(0,0)` top-left matches OpenGL.)

**Why:** Internal draws (VD compositor, text, quads) could leave viewport/scissor stale or clipped; **v2.4.175** fixed that by re-appablishing bounds before user draws. Once **`BeginRenderPass`** adopted parity viewport in **v2.4.189**, re-applying a **standard** viewport in the hygiene helper fought the pass-wide convention. This patch aligns tracked user draws with **`BeginRenderPass`** and **`_SitVulkanApply2DViewportScissor`**.

**Supersedes** the **v2.4.189** note that *"user 3D mesh draws keep standard viewport"* — the live model is one parity viewport per render pass.

### Helper roles (after v2.4.220)

| Helper | Role |
|--------|------|
| **`_SitVulkanFillViewport2DOpenGLParity`** | Builds parity `VkViewport` (`y = height`, `height = -height`) |
| **`_SitVulkanApply2DViewportScissor`** | Internal 2D draws (quad, text, VD compositor) |
| **`_SitVulkanApplyGraphicsViewportScissor`** | Tracked user-draw hygiene — **now same viewport math as 2D helper** |
| **`_SitVulkanApplyTrackedRasterDynamics`** | Calls graphics viewport/scissor + primitive topology, polygon mode, depth bias, stencil, etc. |

**Follow-up (not in this patch):** **`_SitVulkanApplyGraphicsViewportScissor`** and **`_SitVulkanApply2DViewportScissor`** are byte-identical after **v2.4.220** — a future cleanup can merge them into one helper. **`_SitVulkanApplyTrackedRasterDynamics`** may still run twice on some draw paths (bind + draw); redundant but harmless.

---

---

---

---

---

---

## [v2.4.221 "VD Content Update Tracking"] - 2026-06-08

### Description

**v2.4.221**: Phase 1 of the Virtual Display update-detection plan — per-VD **content write** timestamps and query API, distinct from the existing VD frame clock (`last_update_time_seconds`) and manual `is_dirty` flag. Compositor idle fallback (shader colorburst/solid) remains **Phase 2**; `idle_threshold_seconds` is stored now for that follow-up.

**Canonical version**: `sit/situation_base_version.h` → **2.4.221**.

**Plan**: `doc/plan/VIRTUAL_DISPLAY_UPDATE_DETECTION_PLAN.md` (Phase 1 marked complete).

### New API

- **`SituationGetVirtualDisplayUpdateInfo(display_id, …)`** — optional outputs: `last_content_update_time`, `last_content_update_frame`, `frames_since_update` (VD frame ticks), `seconds_since_update` (wall clock). Returns `SITUATION_ERROR_NOT_INITIALIZED` / `SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID` on failure.
- **`SituationSetVirtualDisplayIdleThreshold(display_id, threshold_seconds)`** — per-VD idle threshold (default **1.0** s at creation); used by Phase 2a compositor idle branch (not yet implemented).

### New struct fields (`SituationVirtualDisplay`)

- `last_content_update_time` — monotonic time of last pixel write (`_SitVDGetTimeSeconds()`).
- `last_content_update_frame` — `vd->frame_count` at last write.
- `idle_threshold_seconds` — reserved for Phase 2a idle compositor fallback.

Initialized at **`SituationCreateVirtualDisplayEx`** (including via **`SituationCreateVirtualDisplay`**).

### Content-write hooks (`situation_impl_renderer.h`)

Timestamps bump only on **actual writes**:

| Path | Hook |
|------|------|
| Raster VD | **`SituationCmdEndRenderPass`** when `recording_pass_had_draw` and `display_id >= 0` |
| Draw commands | Set `recording_pass_had_draw` via **`_SitVDRecordingNoteDrawCmd`** |
| Compute-target VD | **`SituationCmdDispatch`** / **`SituationCmdDispatchIndirect`** after bound storage-image dispatch |
| Copy to VD texture | **`SituationCmdCopyTexture`** / **`SituationCmdCopyBufferToTexture`** when destination maps to a compute-target VD slot |

**Not** hooked: **`SituationCmdBeginRenderPass`** (clear-only passes do not count), **`SituationCmdBindComputeTexture`** (bind ≠ write).

Implementation lives in **`situation_impl_vd.h`**; forward declarations in **`situation_impl_forward.h`**. Recording-state fields on **`SituationGLSoftCommandBuffer`** (OpenGL) and **`sit_render.vk`** (Vulkan).

### Harness (`tests/harness/test_virtual_display.c`)

- **`vd_content_update_info_after_draw`** — draw to VD → `seconds_since_update` ≈ 0.
- **`vd_content_update_info_idle`** — clear-only pass does not bump timestamp; `frames_since_update` advances; wall-clock idle after harness wait.

### Build fix

- **`_SitVDResetGLRecordingState`** guarded with **`#if defined(SITUATION_USE_OPENGL)`** in forward/VD headers — fixes Vulkan-only static builds where **`SituationGLSoftCommandBuffer`** is undefined.

---

---

---

---

---

---

## [v2.4.222 "VD Idle Compositor Fallback"] - 2026-06-08

### Description

**v2.4.222**: Phase 2a of the Virtual Display update-detection plan — **shader-only compositor idle fallback** when a VD exceeds `idle_threshold_seconds` without a content write. Non-idle compositing is unchanged (samples the VD texture as before).

**Canonical version**: `sit/situation_base_version.h` → **2.4.222**.

**Plan**: `doc/plan/VIRTUAL_DISPLAY_UPDATE_DETECTION_PLAN.md` (Phase 2a largely complete).

### New API

- **`SituationSetVirtualDisplayFallbackMode(display_id, mode)`** — `SITUATION_VD_FALLBACK_SOLID` (flat color) or `SITUATION_VD_FALLBACK_COLORBURST` (SMPTE **EG 1-1990** test card: top **⅔** seven 75% bars, **¹⁄₁₂** castellation strip, bottom **¼** with **−I**, 100% white, **+Q**, and **PLUGE**).
- **`SituationSetVirtualDisplayFallbackColor(display_id, color)`** — RGBA for SOLID mode (default deep blue `{13, 38, 102, 255}` at create).

### Compositor shaders (`situation_impl_decl.h`)

When idle, compositor fragment shaders **do not sample** the stale VD texture; they generate SOLID or COLORBURST RGB in-shader (Path A advanced blend + Path B alpha). COLORBURST follows SMPTE EG 1-1990 / FFmpeg `smptebars` layout. OpenGL and Vulkan share the same embedded GLSL with per-backend **y-down** UV normalization.

### OpenGL deferred execute fix (`situation_impl_renderer.h`)

- **`SituationCmdDrawTexture`** / **`DrawTextureYpqGrade`** now call **`_SitVDRecordingNoteDrawCmd`**.
- Soft-buffer replay tracks `exec_pass_display_id` / `exec_pass_had_draw` and calls **`_SitVDEndRenderPassCheck`** on **`SIT_OP_END_RENDER_PASS`** execute so content timestamps update under deferred GL.

### Harness (`tests/harness/test_virtual_display.c`)

- **`vd_idle_content_switch`** — 1.5s timed demo: SOLID idle → `prairie.jpg` live → SOLID idle.
- **`vd_idle_content_switch_colorburst`** — same timeline with COLORBURST standby; live phase uses any second harness photo or a built-in orange/teal grid.

---

---

---

---

---

---

## [v2.4.223 "Fullscreen Canvas Stretch Parity"] - 2026-06-08

### Description

**v2.4.223**: Exclusive fullscreen now matches OpenGL behavior — the **monitor keeps its native resolution** while rendering stays at the **windowed canvas size** (e.g. 1280×720) and the finished frame is **stretched to fill the display**. Fixes Vulkan (and OpenGL) HUD/VD scale mismatches between windowed and fullscreen, and removes the incorrect approach of forcing the display video mode to the window size.

**Canonical version**: `sit/situation_base_version.h` → **2.4.223**.

### Fullscreen window management (`situation_impl_wdm.h`)

- **`SituationToggleFullscreen`** / **`SituationApplyCurrentProfileWindowState`** — `glfwSetWindowMonitor` uses the monitor's **native** `mode->width` / `mode->height` again (display resolution unchanged).
- **`render_canvas_width` / `render_canvas_height`** — saved on fullscreen entry from the windowed backbuffer; updated on windowed resize only.
- **`SituationGetRenderWidth` / `SituationGetRenderHeight`** — return canvas dimensions during exclusive fullscreen stretch; swapchain extent (Vulkan) or `glfwGetFramebufferSize` (OpenGL) otherwise.

### Canvas stretch render path (`situation_impl_renderer.h`)

- **OpenGL** — offscreen canvas FBO at render-canvas size; main pass + VD composite draw there; **`_SituationGLBlitCanvasToDisplay`** upscales to the default framebuffer before swap (`GL_NEAREST`).
- **Vulkan** — offscreen canvas color/depth/framebuffer; main pass targets canvas when stretch is active; **`_SituationVulkanRecordCanvasStretchBlit`** blits to the swapchain before present (`VK_FILTER_NEAREST`).
- Stretch activates only when exclusive fullscreen and canvas size ≠ display/swapchain size.

### VD compositor filtering (`situation_impl_vd.h`)

- VD texture samplers and **`SituationSetVirtualDisplayScalingMode`** use **`NEAREST`** for all scaling modes (including STRETCH) so upscaled pixels stay sharp, not bilinear-soft.

### Example

- **`examples/vd_idle_standby_demo.c`** — benefits from consistent windowed/fullscreen scale for cube, VD standby, and HUD text (no DPI/fullscreen compensation hacks in the demo).

---

---

---

---

---

---

## [v2.4.224 "Vulkan Acquire & Screenshot Readback"] - 2026-06-09

### Description

**v2.4.224**: Closes Vulkan harness regressions after **v2.4.223** fullscreen canvas / swapchain sync work. Two root causes:

1. **Acquire** — `_SituationVulkanEnsureSwapchainMatchesFramebuffer()` and other recreate paths returned `SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED` (-710) even when recreation **succeeded**, so the first `SituationAcquireFrameCommandBuffer()` after init/resize failed while the second call worked. Harness helpers map that to **-502** (`SITUATION_ERROR_RENDER_COMMAND_FAILED`).
2. **Screenshot readback** — with `SITUATION_ENABLE_RENDER_THREAD`, main-thread `SituationEndFrame()` queues GPU work and returns before the render thread runs `_SituationVulkanResolveScreenshotAfterSubmit()`. `SituationLoadImageFromScreen()` then saw `screenshot_valid == false` and fell back to `last_presented_image_index` (previous test's pixels).

**Canonical version**: `sit/situation_base_version.h` → **2.4.224**.

**Builds on**: **v2.4.223** (`_SituationVulkanEnsureSwapchainMatchesFramebuffer`, canvas stretch). Rebuild **`build_situation.bat vulkan`** and **`build_tests.bat vulkan`** — stale `situation_dll_vulkan.o` will mask header-only renderer changes.

### Files touched

| File | Change |
|------|--------|
| `sit/situation_api.h` | New override **`SITUATION_VULKAN_ACQUIRE_SWAPCHAIN_RETRIES`** (default **4**). |
| `sit/situation_impl_renderer.h` | Acquire retry loop, `image_acquired` guard, swapchain sync return fix, **`_SituationVulkanEnsureScreenshotResolvedForFrame`**. |
| `sit/situation_impl_renderer_fwd.h` | Forward declaration for screenshot resolve helper. |
| `sit/situation_impl_image.h` | **`SituationLoadImageFromScreen`** calls resolve helper after fence wait. |
| `tests/harness/sit_test_window.h` | **`sit_test_acquire_frame`**, **`sit_test_gpu_context_init`** already-init guard. |

### Vulkan frame acquire (`situation_impl_renderer.h`, `situation_api.h`)

- **`SituationAcquireFrameCommandBuffer`** — refactored into an internal **`for` retry loop** (`SITUATION_VULKAN_ACQUIRE_SWAPCHAIN_RETRIES`, default **4**). Each attempt: `glfwPollEvents()` → fence wait → resize/sync checks → backpressure → staging/graveyard flush → render-thread recreate request → `vkAcquireNextImageKHR`.
- **Retry triggers** (successful recreate → `continue`, not error return): `framebuffer_resized`, `_SituationVulkanEnsureSwapchainMatchesFramebuffer()` recreate failure path (`SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED` from failed recreate only), `recreate_swapchain_request`, `VK_ERROR_OUT_OF_DATE_KHR`, `VK_TIMEOUT`.
- **`_SituationVulkanEnsureSwapchainMatchesFramebuffer`** — returns **`SITUATION_SUCCESS`** after successful recreate (was returning **-710** on success in **v2.4.223**, forcing a manual second acquire).
- **`image_acquired` guard** — if all retries exhaust without `vkAcquireNextImageKHR` success, returns **`SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED`** (fixes silent fall-through with `image_index == 0` while `swapchain_valid` remained true).
- **Pre-acquire validation** — returns **`SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID`** when `swapchain_valid` is false or `swapchain == VK_NULL_HANDLE` before calling `vkAcquireNextImageKHR`.
- **Diagnostics** — slow/timeout acquire stderr log now includes **attempt index** (`vkAcquireNextImageKHR %.2f ms result=%d (attempt %u)`).

### Vulkan screenshot readback (`situation_impl_renderer.h`, `situation_impl_image.h`)

- **`_SituationVulkanEnsureScreenshotResolvedForFrame(frame_index)`** — if `screenshot_valid` is false and `screenshot_copy_pending[frame_index]` is true, runs `_SituationVulkanResolveScreenshotAfterSubmit` on the main thread (fence wait + staging map → `screenshot_buffer`). Safe when render thread has not resolved yet; no-op when cache already valid or no pending copy.
- **`SituationLoadImageFromScreen`** — after waiting on the previous frame-slot fence, calls the helper so harness pixel asserts read the **current** frame's pre-present copy instead of the swapchain fallback path.

### Harness (`tests/harness/sit_test_window.h`)

- **`sit_test_gpu_context_init`** — returns **`SITUATION_SUCCESS`** when `SituationIsInitialized()` is already true (misc YPQ photo sweep no longer hits **`SITUATION_ERROR_ALREADY_INITIALIZED`** (-4) inside a live harness session).
- **`sit_test_acquire_frame`** — optional helper (poll + up to **6** acquire attempts). **Not yet adopted** across harness modules; library-side acquire retry is the primary fix. Tests still call `SituationAcquireFrameCommandBuffer()` directly.

### Harness failures targeted (visible-window `sit_test_vulkan.exe`)

| Failure | Symptom | Fix |
|---------|---------|-----|
| `core.viewport_index_zero_parity` | First acquire in module fails; next test passes | Acquire retry loop |
| `graphics.async_shader_begin_reports_in_progress` | Poll loop acquire → **-502** | Acquire retry loop |
| `compute.pipeline_barrier_no_crash` | First `compute_begin_frame` fails | Acquire retry loop |
| `transfer.texture_barrier_validation` | Acquire fails | Acquire retry loop |
| `misc.ypq_to_rgb_q_sweep_4s` | `misc_ypq_present_plane` acquire → **-502** | Acquire retry loop |
| `misc.ypq_photo_y_p_q_sweep` | `SituationInit` → **-4** | `sit_test_gpu_context_init` guard |
| `virtual_display.render_virtual_displays` | `acquired` assert | Acquire retry loop |
| `virtual_display.vd_*` (pixel asserts) | Stale red/black vs expected (visibility, fit, blend, offset, opacity) | Screenshot resolve before readback |
| `obj_loader.teapot_obj_draw_and_verify` | `found_non_black` false | Screenshot resolve before readback |

**Headless** (`--headless` / `SIT_TEST_HEADLESS`): largely passed before this patch (timing differed). **Not fixed here:** `audio_effects_heard.graph_tone_synth_effect_heard_maximizer` (flaky wet/dry audio; also fails headless).

### API / tuning

- Override before including `situation_api.h`: **`#define SITUATION_VULKAN_ACQUIRE_SWAPCHAIN_RETRIES N`** to change internal recreate/acquire retry budget (default **4**).

---

---

---

---

---

---

## [v2.4.225 "Vulkan Render Sync & OBJ Normals"] - 2026-06-09

### Description

**v2.4.225**: Follow-up to **v2.4.224** — fixes render-thread fence deadlock, general OBJ normal handling, and harness regressions from the full Vulkan test run.

**Canonical version**: `sit/situation_base_version.h` → **2.4.225**.

**Builds on**: **v2.4.224**. Rebuild **`build_situation.bat vulkan`** / **`static-vulkan`** and **`build_tests.bat`** after pulling.

### Vulkan render thread (`situation_impl_renderer.h`)

- **Screenshot resolve** — removed synchronous `_SituationVulkanResolveScreenshotAfterSubmit` from the render thread after `vkQueueSubmit`. Main thread resolves via `_SituationVulkanEnsureScreenshotResolvedForFrame` on readback (fixes 15s fence timeout / leaked frames when render thread blocked on `in_flight_fences`).

### OBJ loader (`situation_impl_renderer.h`, `situation_api.h`)

- **`_SitOBJFinalizeMeshNormals`** — normalize authored normals; fill missing (`vn` absent → zero sentinel) or degenerate corners from face geometry only. Preserves smooth per-corner normals from the file (not asset-specific flat overwrite).

### Harness

- **`sit_test_acquire_frame`** in core render-pass test; screenshot readback retry loop in **`graphics_test_screen_any_non_black`**.
- **Tone synth**: mono detached retrigger pitch snap; melody level check via capture; maximizer harmonic Goertzel verify.

---

---

---

---

---

---

## [v2.4.226 "Vulkan Render Thread Present Fix"] - 2026-06-09

### Description

**v2.4.226**: Fixes **v2.4.225** regression where the Vulkan render thread wedged after the first queued frames — `in_flight_fences` never signaled, acquire timed out (~15s), and shutdown reported frame refcount leaks.

**Canonical version**: `sit/situation_base_version.h` → **2.4.226**.

### Vulkan render thread (`situation_impl_renderer.h`, `situation_impl_decl.h`)

- **Present only after successful submit + GPU fence** — render thread waits `in_flight_fences[frame]` (with window pump) before `vkQueuePresentKHR`; skips present when submit or fence wait fails (avoids blocking forever on an unsignaled `render_finished` semaphore).
- **Per-slot compute sync** — `needs_compute_wait[SITUATION_MAX_FRAMES_IN_FLIGHT]` replaces the global flag so a later frame cannot make the render thread wait on another slot's compute semaphore.
- **Threaded EndFrame compute handoff** — submits async compute on the main thread before queueing graphics to the render thread (parity with single-threaded path).
- **`last_presented_image_index`** — updated on the render-thread present path.

Screenshot CPU resolve remains on the main thread via `_SituationVulkanEnsureScreenshotResolvedForFrame` (`SituationLoadImageFromScreen`).

---

---

---

---

---

---

## [v2.4.227 "Vulkan Screenshot Slot & OBJ Readback"] - 2026-06-09

### Description

**v2.4.227**: Fixes stale Vulkan screenshot cache across frame slots, improves OBJ mesh readback/normals, and cleans up harness mesh leaks.

**Canonical version**: `sit/situation_base_version.h` → **2.4.227**.

### Vulkan screenshot (`situation_impl_renderer.h`, `situation_impl_image.h`, `situation_impl_decl.h`)

- **`screenshot_resolved_frame_index`** — tracks which frame-in-flight slot populated `screenshot_buffer`; `SituationLoadImageFromScreen` rejects stale cache and resolves the correct slot when `screenshot_copy_pending` is set.
- **Render-thread readback** — brief `frames_pending` drain + window pump before fence wait on screenshot readback.

### Mesh / OBJ (`situation_impl_renderer.h`, `situation_impl_decl.h`)

- **CPU mesh cache on `SituationCreateMesh`** — `SituationGetMeshData` prefers upload-time CPU copy (reliable bounds/centering; avoids fragile GPU readback).
- **`_SitOBJFinalizeMeshNormals`** — final pass assigns `(0,0,1)` to any remaining degenerate normals (prevents all-black normal-visualization draws).
### Harness

- **`graphics_test_destroy_fullscreen_mesh`** — called from `graphics` module teardown (fixes “Leaked Mesh (Slot 0)” after SPIR-V tests).
- **`obj_loader` draw test** — AABB-derived fit scale (general for any OBJ bbox, not a fixed constant).

---

---

---

---

---

---

## [v2.4.228 "Vulkan BindPipeline Descriptor Fix"] - 2026-06-09

### Description

**v2.4.228**: Reverts **v2.4.227** regression — `SituationCmdBindPipeline` incorrectly bound `view_proj_ubo_descriptor_set` at descriptor set 0 for `SIT_SPIRV_LAYOUT_PROFILE_MESH` pipelines. That layout uses `dynamic_ubo_layout` at set 0, not `view_data_ubo_layout`, which caused validation errors, `ACCESS_VIOLATION` crashes, and cascading `SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED` (-746) across the graphics harness.

Also fixes **v2.4.223** fullscreen canvas stretch on OpenGL-only builds: canvas FBO helpers were declared and called in OpenGL code but defined inside `#if defined(SITUATION_USE_VULKAN)`, breaking `build_situation.bat static-opengl` and `build_tests.bat static-opengl`.

**Canonical version**: `sit/situation_base_version.h` → **2.4.228**.

### Vulkan bind pipeline (`situation_impl_renderer.h`)

- **`SituationCmdBindPipeline`** — restore correct descriptor-set-0 binding for `SIT_SPIRV_LAYOUT_PROFILE_MESH` pipelines (`dynamic_ubo_layout`, not `view_data_ubo_layout`).

### OpenGL canvas stretch (`situation_impl_renderer.h`)

- **`_SituationGLDestroyCanvasResources`**, **`_SituationGLEnsureCanvasResources`**, **`_SituationGLBlitCanvasToDisplay`** — moved definitions from the Vulkan-only preprocessor block into the OpenGL section (before `_SituationGLExecuteCommands`). Fixes GCC implicit-declaration warnings and `undefined reference` link errors when building/linking the static OpenGL library.

---

---

---

---

---

---

## [v2.4.229 "Render Queue Wedge & Async Shader Unload Fix"] - 2026-06-10

### Description

**v2.4.229**: Fixes two intermittent, scheduling-dependent wedges that have flickered on/off across recent patches:

1. **Render thread wedge (15s fence timeouts)** — the frame ring buffer `render_queue[]` (capacity `SITUATION_MAX_FRAMES_IN_FLIGHT`) used `head == tail` as its only "queue empty" test. Backpressure legally admits exactly `SITUATION_MAX_FRAMES_IN_FLIGHT` enqueues before any dequeue, so on the final enqueue `head` wraps onto `tail` and a **full** queue becomes indistinguishable from an **empty** one. The render thread slept forever, queued frames were never submitted, `in_flight_fences` never signaled, and every subsequent `SituationAcquireFrameCommandBuffer` burned the full 15s budget (`SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS` log spam). Race window widened by v2.4.226 (GPU fence wait inside the render-thread loop body) plus harness tests submitting two frames back-to-back.
2. **Async shader load wedge after unload-during-load** — `_SituationVulkanFreeAsyncShaderLoad` bounded its wait with **500k `thrd_yield()` iterations** (~100–450ms on Windows), *less* than a typical ~300ms shaderc compile, so routine `SituationUnloadShader` during an in-flight compile abandoned a *live* worker. The detached compile overlapped the next `SituationBeginLoadShaderFromMemory`, which then never reached `compile_done` (`sync_shader_after_async_cycle` polled `SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS` for 3000 frames). Audit of the job queue surfaced two adjacent races (in-flight slot reuse, completion ordering), both closed.

**Canonical version**: `sit/situation_base_version.h` → **2.4.229**.

### Render thread frame queue (`situation_impl_renderer.h`, `situation_impl_decl.h`)

- **`render_queue_count`** — explicit occupancy counter guarded by the existing `render_queue_mutex` (no new locks/atomics). Producers (GL + Vulkan `SituationEndFrame` handoff) increment; the render thread's wait predicate, shutdown-drain check, and dequeue use the count instead of `head == tail`. A full ring is now serviced correctly; shutdown still drains all queued frames before exit.
- **Vulkan enqueue modulus** — ring index arithmetic now uses `SITUATION_MAX_FRAMES_IN_FLIGHT` (the array's compile-time capacity) instead of `vk.max_frames_in_flight`, preventing head/tail desync if the runtime value is ever clamped below the constant.

### Async shader unload (`situation_impl_renderer.h`, `situation_api.h`)

- **`_SituationVulkanFreeAsyncShaderLoad`** — in-flight compile wait is now **wall-clock budgeted** via new override **`SITUATION_VULKAN_ASYNC_UNLOAD_WAIT_NS`** (default **10s**) instead of an iteration count. Routine unload-during-load reclaims the ctx deterministically; the abandon path (`compile_done = -2`, worker self-frees) remains strictly a last resort for wedged workers / dead pools.

### Thread pool job queue (`situation_impl_threading.h`, `situation_impl_io.h`)

- **`SituationSubmitJobEx`** — a target slot whose previous job is still in flight (`is_completed == false`) is now treated exactly like a full queue (same `RUN_IF_FULL` / `BLOCK_IF_FULL` / fail backpressure). Closes a wrap-around TOCTOU where a submit could overwrite an executing job's `func`/payload (lost job / double-run) and the late completion stamp corrupted the new occupant. Cost: one atomic load per submit.
- **Completion ordering (worker, `DispatchParallel` steal, I/O thread)** — `generation` is bumped **before** `is_completed = true`; since `is_completed` now gates slot reuse it must be the last write, otherwise a submitter could capture a stale generation and the new job's handle would falsely report complete via `SituationWaitForJob`'s gen-mismatch path.
- **Steal + I/O paths** — now free `owns_memory` payloads after execution (parity with the worker path; previously leaked).

### Verification

- Vulkan harness: `[core]` previously wedged at the first threaded frame handoff (repeating `[Vulkan] Frame fence wait timed out` ~15s); post-fix full run 500 tests / 21 modules with zero fence timeouts. `graphics.sync_shader_after_async_cycle` + the `Leaked Shader (Slot 0)` teardown warning addressed by the unload-path fix. Both bugs are races — re-run the suite multiple times to confirm.

---

---

---

---

---

---

## [v2.4.230 "Async Shader Terminal Error Codes"] - 2026-06-10

### Description

**v2.4.230**: `graphics.sync_shader_after_async_cycle` still failed on a verified v2.4.229 build (`expected 0, got -1` after 3000 frames), proving the v2.4.229 unload-wait fix was necessary but not the root cause: the second async compile's `compile_done` never leaves 0. The API defect this exposes is that `SituationPollShaderLoad` had **no way to report a compile that can never finish** — `compile_done == 0` returned `SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS` unconditionally, forever. A lost or wedged compile job was indistinguishable from a slow one, for callers and for the test harness alike.

**Canonical version**: `sit/situation_base_version.h` → **2.4.230**.

### New error codes (`situation_base_errno.h`)

- **`SITUATION_ERROR_THREAD_JOB_LOST` (-99)** — a job-queue slot settled (handle retired) but the submitted function never ran. Always indicates a scheduler defect.
- **`SITUATION_ERROR_SHADER_COMPILE_TIMEOUT` (-557)** — an async shader compile exceeded its wall-clock deadline (wedged or starved compile worker).

### Async shader poll (`situation_impl_renderer.h`, `situation_impl_threading.h`, `situation_api.h`)

- `_SituationVkAsyncShaderLoad` now records its **compile `SituationJobId`** and **monotonic submit timestamp**.
- New internal `_SitJobHandleSettled()` — non-blocking O(1) handle-status check (same generation/flag logic as `SituationWaitForJob`).
- `_SituationPollVkAsyncShaderLoad` with `compile_done == 0` now distinguishes three states instead of one:
  1. **Job retired without running** (`settled && compile_done == 0` — workers store `compile_done` *before* the queue stamps the slot, so this is provable): frees the ctx (no thread can touch it again) and returns **`SITUATION_ERROR_THREAD_JOB_LOST`**.
  2. **Compile past deadline** (new override **`SITUATION_VULKAN_ASYNC_COMPILE_DEADLINE_NS`**, default **5s** vs ~300ms typical compiles): abandons the ctx to the worker (`compile_done = -2`, worker self-frees — freeing here would UAF) and returns **`SITUATION_ERROR_SHADER_COMPILE_TIMEOUT`**.
  3. Otherwise: genuinely in progress → `SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS` as before.
- Both terminal paths clear `slot->vk_async_load`, so subsequent polls return the sticky error from `SituationGetLastErrorCode` rather than re-detecting.

### Diagnostic intent

The persistent `sync_shader_after_async_cycle` failure will now fail with a **meaningful code** instead of the harness's generic -1 timeout: **-99** means the job queue retired the compile job without executing it (scheduler bug — slot reuse/scan path); **-557** means a worker entered shaderc and never returned (wedged compile). Either way the failing class is identified by the error code itself, with zero per-frame cost for healthy loads (one atomic load + one branch per poll).

---

---

---

---

---

---

## [v2.4.231 "Async Shader UAF & Poll Contract Fix"] - 2026-06-10

### Description

**v2.4.231**: Fixes the real root cause behind `graphics.sync_shader_after_async_cycle` returning **-557** (`SITUATION_ERROR_SHADER_COMPILE_TIMEOUT`) on a verified v2.4.230 build. The v2.4.230 error codes correctly classified the failure (compile never finished within 5s); this patch removes the underlying defect.

**Root cause**: `_SituationVulkanFreeAsyncShaderLoad` waited only on `compile_done`, then freed `ctx` while the thread-pool job slot could still be in-flight (`is_completed == false`, `large_data_ptr == ctx`). The next `SIT_MALLOC` could reuse that address; the pool still held the stale pointer, corrupting the subsequent async compile's GLSL source (same failure class as v2.4.226's documented UAF — `fs_src` read as `0x01`, intermittent shaderc wedge). `async_shader_unload_during_load` immediately precedes `sync_shader_after_async_cycle` in the harness, so this race was deterministic.

Secondary defect: Vulkan `SituationAcquireFrameCommandBuffer` polled every active `vk_async_load` slot each frame, duplicating `SituationPollShaderLoad` (OpenGL SPIR-V explicitly does **not** do this). Double polling advanced the compile deadline twice per frame and could race pipeline creation.

**Canonical version**: `sit/situation_base_version.h` → **2.4.231**.

### Library changes (`situation_impl_renderer.h`)

- **`_SituationVulkanFreeAsyncShaderLoad`**: after `compile_done != 0`, call **`SituationWaitForJob(compile_job)`** before freeing `ctx` (restores v2.4.105 contract). Abandon path (`compile_done = -2`) unchanged — must not wait on a wedged job.
- **`SituationAcquireFrameCommandBuffer` (Vulkan)**: removed the all-slots `_SituationPollVkAsyncShaderLoad` loop; async GLSL compile + pipeline build are driven only from **`SituationPollShaderLoad`**, matching the OpenGL SPIR-V contract.
- **`SituationBeginLoadShaderFromMemory`**: `submit_time_ns` is stamped **after** `SituationSubmitJobEx` returns so `BLOCK_IF_FULL` queue wait is not charged against the compile deadline.

### Verification

Rebuild library + tests, confirm banner **2.4.231**. `graphics.sync_shader_after_async_cycle` should pass; `-557` / `-99` on that test indicate a remaining scheduler or compile hang.

---

---

---

---

---

---

## [v2.4.232 "Job Queue In-Place Claim Fix"] - 2026-06-10

### Description

**v2.4.232**: Fixes Epic D scan-forward **struct swap** breaking `SituationJobId` handles. IDs encode `(queue, generation, physical slot_idx)`; swapping two `SituationJob` structs in the ring moved work to a different slot while handles still pointed at the submission slot. That made `_SitJobHandleSettled` / `SituationWaitForJob` treat in-flight jobs as retired (generation mismatch), producing false **`SITUATION_ERROR_THREAD_JOB_LOST` (-99)** and breaking `continuation_id` resolution.

**Fix**: never move jobs in the ring. Workers/I/O/main-steal now **claim ready jobs in-place** with a CAS on `dependency_count` (`0 → SIT_JOB_DEP_CLAIM_BIT`); real dependency counts remain in the low bits. Tail **compacts lazily** past completed front slots. HOL mitigation preserved without invalidating handles.

**Canonical version**: `sit/situation_base_version.h` → **2.4.232**.

### Library changes

- **`situation_impl_threading_scheduler.h`**: `SIT_JOB_DEP_CLAIM_BIT`, `_SitJobDepCount`, `_SitQueueCompactTailLocked`, `_SitWorkerTryClaimReadyJob`.
- **`situation_impl_threading.h`**: worker dequeue + `SituationDispatchParallel` steal use in-place claim; pre-run dep wait uses `_SitJobDepCount`.
- **`situation_impl_io.h`**: I/O queue uses the same claim path (scan-forward on low queue when tail is blocked).

`stats_scan_forward_swap` now counts out-of-order **claims** (metric name unchanged for ABI).

---

---

---

---

---

---

## [v2.4.233 "Job Queue Full Scan HOL Fix"] - 2026-06-10

### Description

**v2.4.233**: Completes the Epic D HOL fix started in **v2.4.232**. In-place claim preserved valid `SituationJobId` handles (no struct swap), but workers still capped scan-forward at **`_SitWorkerScanDepthForPending`** (max **32** slots, or **`pending / 2`** below 64). When a long-running job blocked the tail (e.g. async shaderc compile), `head` kept advancing while `tail` could not compact. Ready jobs submitted beyond the scan window were **invisible** to workers until the blocker finished — producing **`SITUATION_ERROR_SHADER_COMPILE_TIMEOUT` (-557)** on `graphics.sync_shader_after_async_cycle` and false **`SITUATION_ERROR_THREAD_JOB_LOST` (-99)** when handles appeared settled while compile side effects never ran.

**Fix**: `_SitWorkerTryClaimReadyJob` now scans the **entire** pending range (`head - tail`). Jobs stay at their physical slot; workers claim ready slots in-place via `SIT_JOB_DEP_CLAIM_BIT` CAS only.

**Canonical version**: `sit/situation_base_version.h` → **2.4.233**.

### Library changes

- **`situation_impl_threading_scheduler.h`**: removed `SIT_WORKER_SCAN_DEPTH_*` and `_SitWorkerScanDepthForPending`; full-queue scan in `_SitWorkerTryClaimReadyJob`.
- **`situation_impl_forward.h`**: dropped `_SitWorkerScanDepthForPending` forward declaration.

---

---

---

---

---

---

## [v2.4.234 "Async Shader Compile Pump & Dep Decrement Fix"] - 2026-06-10

### Description

**v2.4.234**: Fixes `graphics.sync_shader_after_async_cycle` **-557** (`SITUATION_ERROR_SHADER_COMPILE_TIMEOUT`) that persisted after v2.4.233 full-queue HOL scan. Two defects:

1. **CLAIM_BIT + blind `fetch_sub`**: Epic D in-place claim stores `SIT_JOB_DEP_CLAIM_BIT` in `dependency_count`. Continuation handling used `atomic_fetch_sub(..., 1)` on the whole word — on a claimed slot (`0x40000000`) this corrupts the low bits to `0x3FFFFFFF`, wedging the worker in the pre-run dependency wait forever (`compile_done` stays 0, handle never settles → 5s poll timeout, not -99). Fixed via `_SitJobDecrementDependency()` (decrement low bits only; no-op when only CLAIM_BIT).

2. **Poll never drove compile progress**: `SituationPollShaderLoad` only observed `compile_done` and timed out. Added `_SituationVkAsyncShaderCompilePump()` — `WaitForJob` when the pool job is claimed/in-flight, inline `_SituationVkAsyncCompileWorker` when still queued (`dep==0`), also used from `_SituationVulkanFreeAsyncShaderLoad` wait loop. Worker uses `compile_done` CAS `0→-3→1` to avoid double shaderc with pump.

**Harness**: new `async_shader_poll_after_unload_during_load` (inserted before `sync_shader_after_async_cycle`) — repeats unload-during-load, asserts pool quiescent (`active_jobs==0`, `high_queue_depth==0`), then begin+poll.

**Canonical version**: `sit/situation_base_version.h` → **2.4.234**.

---

---

---

---

---

---

## [v2.4.235 "Async Compile Pump Non-Blocking Fix"] - 2026-06-10

### Description

**v2.4.235**: Fixes **main-thread deadlock** introduced in v2.4.234. `_SituationVkAsyncShaderCompilePump` called `SituationWaitForJob` from `SituationPollShaderLoad` / `_SituationVulkanFreeAsyncShaderLoad` spin paths. Poll and unload run on the **main thread**; if a pool worker was wedged (or slow), `WaitForJob` spun forever and **locked the whole test process** after `async_shader_unload_during_load`.

**Fix**: pump is cooperative only when the job is already claimed/in-flight — inline compile when still queued (`dependency_count == 0`), otherwise return and let workers progress between poll frames. `SituationWaitForJob(compile_job)` remains **only** in `_SituationVulkanFreeAsyncShaderLoad` after `compile_done == 1` (post-shaderc slot retire; v2.4.105 UAF fix).

**Canonical version**: `sit/situation_base_version.h` → **2.4.235**.

---

---

---

---

---

---

## [v2.4.236 "Async Compile Orphan Job Retire Fix"] - 2026-06-10

### Description

**v2.4.236**: Fixes the **process hang** after `async_shader_unload_during_load` (v2.4.234–235). The compile pump could **inline shaderc on the main thread** while a `SituationSubmitJobEx` handle was still **queued and unclaimed** (`dependency_count == 0`). That set `compile_done == 1` but left the pool job slot **`is_completed == false`**. `_SituationVulkanFreeAsyncShaderLoad` then called `SituationWaitForJob(compile_job)`, which **never returned** — the job was never executed on a worker.

**Fix**:
- Pump **never** inlines when `compile_job != 0`; workers own pool-submitted compiles.
- Before `SituationWaitForJob`, `_SitThreadPoolRetireOrphanedJobMain` retires any still-unclaimed slot (repairs the v2.4.234–235 race if it already happened).
- Keeps v2.4.233 `_SitJobDecrementDependency` and v2.4.233 full-queue HOL scan.

**Canonical version**: **2.4.236**.

---

---

---

---

---

---

## [v2.4.237 "Async Compile -3 In Progress Fix"] - 2026-06-10

### Description

**v2.4.237**: Fixes false **`SITUATION_ERROR_SHADER_COMPILATION_FAILED` (-752)** on async Vulkan shader polls. v2.4.234 introduced `compile_done == -3` while a worker runs shaderc (CAS `0→-3` anti-double-compile guard). `_SituationPollVkAsyncShaderLoad` treated **any** negative `compile_done` as terminal failure, so a poll that raced the worker returned -752 (~300ms) instead of `SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS`. `_SituationVulkanFreeAsyncShaderLoad` also exited its wait spin when `compile_done` flipped to -3, calling `SituationWaitForJob` before shaderc finished.

**Fix**: treat `-3` as in-progress in poll and unload wait (same as `0`); only `-1` / `-2` are terminal compile failures.

**Canonical version**: **2.4.237**.

---

---

---

---

---

---

## [v2.4.238 "Async Shader Unload Progress Driver"] - 2026-06-10

### Description

**v2.4.238**: Phase A of async shader load hardening. Unifies Vulkan poll and unload wait through `_SituationVkAsyncCompileProgress` so unload gets the same LOST detection and tiered timeouts as poll.

**Fix**:
- Shared `_SituationVkAsyncCompileFreeCtx` and `_SituationVkAsyncCompileAbandon` (CAS `-2`, retire pool job, clear `compile_job`).
- Unload abandon at **2 s** (`SITUATION_VULKAN_ASYNC_UNLOAD_ABANDON_NS`) instead of **10 s** last-resort wait.
- LOST job on unload: immediate ctx free (no 10 s spin).
- Worker CAS fail on `-2`: frees ctx (no leak).
- New timing constants: cooperative 500 ms, unclaimed-fast 100 ms, shutdown 500 ms.

**Test plan**: `sit_test_vulkan.exe --module graphics` — expect `async_shader_poll_after_unload_during_load` **< 1 s**.

**Canonical version**: **2.4.238**.

---

---

---

---

---

---

## [v2.4.239 "Main Thread OS Name API"] - 2026-06-10

### Description

**v2.4.239**: OS-visible thread naming for the main thread and any caller thread.

- **`SituationSetCurrentThreadName(const char* name)`** — sets debugger/Task Manager thread name for the calling thread.
- **`SituationInitInfo::main_thread_name`** — optional init override; `NULL` → **`SITUATION_MAIN_THREAD_NAME_DEFAULT`** (`"Sit Main"`).
- Pool snapshot reports the configured main thread name.

**Canonical version**: **2.4.239**.

---

---

---

---

---

---

## [v2.4.240 "Pool Snapshot Context Guard Fix"] - 2026-06-10

### Description

**v2.4.240**: Fixes **ACCESS_VIOLATION** in `SituationGetThreadPoolSnapshot` when called on a standalone pool without `SituationInit` (harness `threading.pool_snapshot_parallel`, `metrics_reset_and_dump`, `cpu_stress_10s_taskmgr`). v2.4.239 read `sit_gs.main_thread_name` unconditionally; `sit_gs` dereferences `_sit_current_context`, which is NULL in pool-only tests.

**Fix**: use `SITUATION_MAIN_THREAD_NAME_DEFAULT` when `_sit_current_context == NULL` (same guard pattern as `thread_affinity_main` in the snapshot).

**Canonical version**: **2.4.240**.

---

---

---

---

---

---

## [v2.4.241 "OpenGL Bindless Extension Guard Fix"] - 2026-06-10

### Description

**v2.4.241**: Fixes **`SituationInit` → -610** (`SITUATION_ERROR_OPENGL_SHADER_COMPILE`) on OpenGL builds when the GPU driver does not expose **`GL_ARB_bindless_texture`**. Internal `quad.frag` and `text.frag` used unconditional `#extension ... : enable`, which fails compile on non-bindless GPUs (common on Intel iGPU / older drivers). Extensions are now enabled only when the driver predefines the extension macro.

**Also**:
- **`_SituationFullCleanupOnError`** — frees `_sit_current_context` and uninits `error_mutex` after failed init so harness tests can retry `SituationInit` cleanly.
- **`core` harness setup** — prints `SituationGetLastErrorMsg` on init failure.

**Canonical version**: **2.4.241**.

---

---

---

---

---

---

## [v2.4.242 "OpenGL Init Error Log And Bindless Guard"] - 2026-06-10

### Description

**v2.4.242**: Follow-up to v2.4.241 OpenGL init **-610** failures.

- **`SituationInit`** now prints **`SituationGetLastErrorMsg`** to **stderr before** `_SituationFullCleanupOnError` frees `_sit_current_context` (harness previously saw only the errno).
- **`quad.frag` / `text.frag`**: `bindless_sampler` and `#extension` blocks require **both** `GL_ARB_bindless_texture` **and** `GL_ARB_gpu_shader_int64` (partial bindless drivers failed compile when only the texture extension was enabled).

**Canonical version**: **2.4.242**.

---

---

---

---

---

## [v2.4.243 "GLSL Extension Injection Order Fix"] - 2026-06-10

### Description

**v2.4.243**: Fixes OpenGL init **-610** `C7621: #extension directive must occur before any non-preprocessor token`.

**Root cause**: On GPUs without bindless, `_SituationCompileGLShaderEx` injected virtual-bindless `uniform` declarations immediately after `#version` / the first `#define`, but **before** `#extension` lines nested inside `#if defined(SITUATION_USE_OPENGL)` in `quad.frag` / `text.frag`.

**Fix**: `_SituationGLSLVirtualBindlessInjectionPoint` scans the full shader source and injects **after the last `#extension` line** (or after leading `#version`/`#define` when none).

**Canonical version**: **2.4.243**.

---

---

---

---

---

---

## [v2.4.244 "MinGW Thread Naming Fix"] - 2026-06-10

### Description

**v2.4.244**: Fixes MinGW/winpthreads threads still showing **"POSIX WinThreads for Windows"** in Task Manager / debugger.

**Root cause**: `thrd_create` worker threads get winpthreads' default name; our `SetThreadDescription` path alone did not update the pthread layer (and missed `kernelbase.dll` on some Win10 builds).

**Fix**:
- On MinGW, call **`pthread_setname_np(pthread_self(), name)`** in `_SituationSetCurrentThreadName`.
- Load **`SetThreadDescription`** from `kernel32.dll`, fallback to **`kernelbase.dll`**.
- Name the **main thread** at the start of `SituationInit` (before platform/window), not only after GLFW window creation.
- Harness **`main`** calls `SituationSetCurrentThreadName("Sit Test")` before any module runs.

**Canonical version**: **2.4.244**.

---

---

---

---

---

---

## [v2.4.245 "Main Thread Naming Hardening"] - 2026-06-10

### Description

**v2.4.245**: Main thread still showed **POSIX WinThreads** after v2.4.244.

**Root cause**: `SetThreadDescription(GetCurrentThread(), …)` on the pseudo-handle is unreliable for the process main thread on some MinGW builds; `pthread_setname_np` was gated on `__MINGW32__` only; naming ran too late (after `sit_test_init` / context alloc).

**Fix**:
- Duplicate the real thread handle (`DuplicateHandle`) before `SetThreadDescription` (same approach as winpthreads).
- Call `pthread_setname_np` on all Windows threaded builds (`!__STDC_NO_THREADS__`).
- Add MSVC debugger exception naming (`0x406D1388`) as a supplement.
- Name main thread at the **start** of `SituationInit` (before context alloc / `thrd_current`).
- Harness names main thread on the **first line** of `main()` via inline Win32 + pthread.

**Canonical version**: **2.4.245**.

---

---

---

---

---

---

## [v2.4.246 "Thread Naming RaiseException Crash Fix"] - 2026-06-10

### Description

**v2.4.246**: Test harness silent exit — **regression from v2.4.245**.

**Root cause**: `_SituationRaiseDebuggerThreadName` called `RaiseException(0x406D1388)` without registering an SEH handler; winpthreads documents this **crashes the process** if uncaught. Harness called `SituationSetCurrentThreadName` before any output.

**Fix**: Remove `RaiseException` debugger naming entirely. Harness calls `SituationSetCurrentThreadName` only after crash handlers are installed.

**Canonical version**: **2.4.246**.

---

---

---

---

---

---

## [v2.4.247 "Main Thread OpenThread Naming Fix"] - 2026-06-10

### Description

**v2.4.247**: Task Manager still showed **POSIX** on the **main** thread while a worker showed **SIT_TEST_** (window title on pthread-spawned threads only).

**Root cause**: `SetThreadDescription(GetCurrentThread(), …)` / `DuplicateHandle` does not reliably name the **process main thread** on MinGW; harness set `window_title` but not `main_thread_name`; main-thread name was not re-applied after render-thread spawn.

**Fix**:
- `OpenThread(THREAD_SET_LIMITED_INFORMATION, …, GetCurrentThreadId())` for OS thread description.
- Main-thread OS name resolves as **`main_thread_name` → `window_title` → `"Sit Main"`**.
- Harness sets `main_thread_name = title` (e.g. `SIT_TEST_CORE`).
- Re-apply main-thread name after render-thread creation and at end of `SituationInit`.
- Harness names main thread on **first line of `main()`** via `OpenThread` only.

**Canonical version**: **2.4.247**.

---

---

---

---

---

---

## [v2.4.248 "Thread Naming Hybrid Order Fix"] - 2026-06-10

### Description

**v2.4.248**: Consolidates Windows thread naming to the hybrid order: **`pthread_setname_np` → `SetThreadDescription` (OpenThread / DuplicateHandle) → `pthread_setname_np`**.

- Documents why pseudo-handles fail for Task Manager on the process main thread.
- Harness `main.c` uses the same order on the first line of `main()`.
- Linux/macOS: reject empty names consistently.

**Canonical version**: **2.4.248**.

---

---

---

---

---

---

## [v2.4.249 "STB Image Format Coverage"] - 2026-06-11

### Description

**v2.4.249**: Maximizes use of **stb_image** decode formats across the image loader and harness.

**API**:
- **`SituationIsStbImageLoadExtension()`** — returns true for all stb_image load extensions: `.jpg`/`.jpeg`, `.png`, `.bmp`, `.tga`, `.psd`, `.gif`, `.hdr`, `.pic`, `.ppm`/`.pgm`/`.pnm`.
- **`SituationLoadImage` / `SituationLoadImageFromMemory`** docs list the full supported set explicitly.
- **`SituationExportImage`** now also writes **`.jpg`/`.jpeg`**, **`.tga`**, and **`.hdr`** via stb_image_write (PNG/BMP unchanged).

**Harness**:
- `misc` module: `stb_image_load_extension_recognition`, `stb_image_load_roundtrip_formats` (png/bmp/jpg/tga/hdr), `stb_image_load_embedded_formats` (gif/ppm/pgm from memory).
- `virtual_display` asset scan uses `SituationIsStbImageLoadExtension()` instead of a hardcoded extension list.
- `obj_loader` module: **`bunny_obj_load`** and **`bunny_obj_rosewood_draw_and_verify`** — loads `stanford-bunny.obj`, applies `rosewood-veneer.png` via triplanar mapping (bunny mesh has no UVs), renders and readbacks non-black pixels.

**Note**: WebP is deferred; stb_image does not decode it.

**Canonical version**: **2.4.249**.

---

---

---

---

---

---

## [v2.4.250 "Bunny Rosewood Harness Assets"] - 2026-06-11

### Description

**v2.4.250**: Harness asset resolution and Stanford bunny + rosewood veneer model test polish.

**Harness**:
- **`sit_test_assets.h`** — shared `sit_test_resolve_harness_asset()` with multi-prefix lookup (`tests/harness/assets/`, `../`, `../../`); `sit_test_resolve_harness_asset_any()` and **`sit_test_resolve_harness_asset_name_contains()`** (directory scan for matching image basenames).
- **`obj_loader`** uses shared asset resolver for teapot/bunny OBJ paths; rosewood texture lookup tries known names (`rosewood-veneer.png`, `rosewood_veneer1.png`, etc.) then any **`rosewood*.png`** in harness assets.
- **`bunny_obj_rosewood_draw_and_verify`** — triplanar textured draw (bunny has no UVs); **`SIT_TEST_SKIP`** when rosewood PNG missing (no misleading pass).
- **`sit_graphics_test_helpers.h`** — triplanar model shaders and `graphics_test_draw_textured_model_meshes()` (albedo bind + depth test).

**Docs / tooling**:
- **Wrapper example builders** — `build_odin_example.bat`, `build_zig_example.bat`, `build_rust_example.bat` accept `opengl` / `vulkan` / `static-opengl` / `static-vulkan` (parity with `build_examples.bat`). Shared `scripts/wrapper_link_config.bat`, `wrapper_ensure_import_lib.bat`, `wrapper_patch_odin_foreign.bat`, `wrapper_gcc_link_static.bat`.
- **Documentation** — `COMPILATION_GUIDE.md`, `README.md`, `situation_api.md`, `situation_sdk.md`, `wrappers/Zig/README.md`, `wrappers/Rust/README.md` updated for new usage.

**Canonical version**: **2.4.250**.

---

---

---

---

---

---

## [v2.4.251] - 2026-06-12

### Description

**v2.4.251**: Typed modifier bitmask — `SituationModifiers`.

### Changes

- **`sit/situation_base_etc.h`** — Introduced `typedef int SituationModifiers`. The six `SIT_MOD_*` defines are now cast to `SituationModifiers` for ABI clarity. No breakage to existing C code — the underlying representation is unchanged.
- **`sit/situation_api.h`** — `SituationKeyCallback` and `SituationMouseButtonCallback` `mods` parameters typed as `SituationModifiers` (was `int`).
- **`sit/situation_impl_input.h`** — `SituationGetCharFromScancode`, `SituationIsLockKeyPressed`, and `SituationIsModifierPressed` parameters typed as `SituationModifiers` (was `int`). Internal GLFW handlers remain `int` (required by GLFW's function pointer type).

### Notes

- Fully backwards-compatible at the C level (implicit int↔SituationModifiers conversion).
- Binding generators (Zig/Rust/Odin) will benefit from the explicit type surface; `SituationModifiers` should be added to the CTYPE_MAP in the generators as a follow-up.

---

---

---

---

---

---

## [v2.4.252] - 2026-06-12

### Description

**v2.4.252**: Header decomposition — `situation_base_types.h` and `situation_base_callbacks.h`.

### Changes

- **`sit/situation_base_types.h`** *(new)* — Foundational primitive types extracted from `situation_api.h`. Zero external dependencies (stdint/stdbool/stddef only). Contains: `SituationModifiers` + `SIT_MOD_*` (moved from `situation_base_etc.h`), math types (`ColorHSV`, `ColorYPQA`, `ColorYPQf`, `ColorRGBA`/`Color`, `Vector2/3/4`, `SitRectangle`), all GPU resource handles (`SituationTexture`, `SituationMesh`, `SituationShader`, `SituationBuffer`, `SituationComputePipeline`), audio handles (`SituationSound`/`SituationSoundHandle`, `SITUATION_NULL_HANDLE`, `SituationToneHandle`, `SituationNodeHandle`/`SITUATION_INVALID_NODE_HANDLE`), and audio stream abstraction types (`SituationStreamSize`, `SituationSeekOrigin`, `SituationStreamResult`/`SIT_STREAM_SUCCESS`).
- **`sit/situation_base_callbacks.h`** *(new)* — All 18 public callback `typedef` signatures. Depends only on `situation_base_types.h` — completely miniaudio-free. Stream callbacks (`SituationStreamReadCallback`, `SituationStreamSeekCallback`) use `SituationStreamSize`/`SituationSeekOrigin`/`SituationStreamResult` instead of `ma_uint64`/`ma_seek_origin`/`ma_result`.
- **`sit/situation_api.h`** — Drops all type definitions that moved to the new base files (~120 lines). Gains `#include "situation_base_types.h"` and `#include "situation_base_callbacks.h"`. `_Static_assert` for `SituationBuffer` packing contract preserved in-file where `SituationBufferUsageFlags` is in scope. Zero API surface change for callers.
- **`sit/situation_base_etc.h`** — `SituationModifiers` block replaced with a redirect comment; definition now lives in `situation_base_types.h`.
- **`sit/situation_impl_renderer_fwd.h`** — OBJ/STL/VertexMap forward declarations removed (those types are defined inline inside `situation_impl_renderer.h` after tinyobj inclusion — they were never callable from outside the renderer). Section replaced with explanatory comment.
- **Steering** — `situation-project.md` architecture tree updated to reflect new files and include chain. Key Rules updated with `situation_base_*` ordering note.

### Notes

- Both backends (`static-opengl`, `static-vulkan`) compile cleanly with zero errors or warnings.
- Include order: `situation_base_types.h` → `situation_base_callbacks.h` → `situation_base_etc.h` → `situation_api.h`. Users include only `situation.h` or `situation_api.h` as before — nothing changes at the call site.
- Language binding generators (`tools/generate_*_bindings.py`) can now target `situation_base_types.h` and `situation_base_callbacks.h` directly for primitive type and callback extraction — no longer need to scan 3,200 lines of `situation_api.h`.

---

---

---

---

---

---

## [v2.4.253] - 2026-06-12

### Description

**v2.4.253**: Threading errno adoption — Phase 11.

Wire the threading error codes from the `ERRNO_ADOPTION_PLAN.md` Phase 11 list. Seven codes are now actively produced; eight remain deferred (no code path exists yet).

### Changes

- **`sit/situation_impl_threading.h`**
  - `SituationCreateThreadPool`: `mtx_init` calls for both queue locks now check the return value; failure sets `SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED` and returns early with proper cleanup.
  - `SituationSubmitJobEx`: `pool != NULL && !pool->is_active` path now sets `SITUATION_ERROR_THREAD_STATE_INVALID` (was silent return 0).
  - `SituationDestroyThreadPool`: `thrd_join` return value checked for both worker threads and the I/O thread; failure sets `SITUATION_ERROR_THREAD_JOIN_FAILED` (non-fatal — cleanup continues regardless).

- **`sit/situation_impl_threading_diag.h`**
  - `SITUATION_CHECK_MUTEX_LOCK` macro: now calls `_SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED, ...)` before returning the caller's error code.
  - `SITUATION_CHECK_MUTEX_UNLOCK` macro: now calls `_SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_UNLOCK_FAILED, ...)` before returning.
  - `SituationMutexTryLockTimeout`: on iteration exhaustion, now calls `_SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_TIMEOUT, ...)` before returning false.

- **`sit/situation_impl_ctrl.h`**
  - `SituationPollInputEvents`: fires `SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION` + stderr log (debug builds) when `sit_render.in_frame` is true at poll time.
  - `SituationUpdateTimers`: same guard — detects `SituationUpdateTimers` called without a preceding `SituationEndFrame`.

- **`sit/situation_api.h`**
  - Added `SITUATION_ASSERT(cond)` macro: in debug builds (`NDEBUG` not defined), a failed condition calls `_SituationSetErrorFromCode(SITUATION_ERROR_ASSERTION_FAILED, "Assertion failed: <cond> (<file>)")` and logs to `stderr`. No-op in release builds. Named `SITUATION_ASSERT` (not `SIT_ASSERT`) to avoid collision with the test harness's own `SIT_ASSERT` macro.

- **`doc/plan/ERRNO_ADOPTION_PLAN.md`** — Phase 11 checkboxes updated; deferred items documented with rationale.

### Notes

- Deferred: `THREAD_DETACH_FAILED` (no detach call sites), `THREAD_ATOMIC_FAILED` (C11 atomics have no failure return), `THREAD_BUFFER_OVERFLOW` (strncpy guards), `THREAD_DEADLOCK_DETECTED` (no lock-graph detector), `THREAD_NOT_AVAILABLE` (API not compiled when threading disabled), `RENDER_BACKPRESSURE_TIMEOUT` / `RENDER_LIST_INCOMPLETE` (render thread not yet implemented), `ARM_INTRINSICS_FAILED` (ARM platform not shipping).
- Both backends (`static-opengl`, `static-vulkan`) compile clean.

---

---

---

---

---

---

## [v2.4.254] - 2026-06-12

### Description

**v2.4.254**: Externalize GPU compute — Phases 0–1C verified and closed.

Core renderer pipeline GLSL sources are fully externalized to `sit/gpu/`. Embedded shader strings removed from `situation_impl_decl.h`. Path-fallback loader verified from both repo root and `build/` CWD. Stale Doxygen `@par` comments in renderer init updated to reference `sit/gpu/` paths.

### Changes

- **`sit/gpu/`** *(new folder, 8 files)* — Canonical GLSL sources for all core renderer pipelines: `compositor.vert`, `vd.frag`, `composite.frag`, `quad.vert`, `quad.frag`, `ypq_grade.frag`, `text.vert`, `text.frag`. Each file carries a complete `@internal`/`@shader`/`@brief`/`@details`/`@var`/`@see` Doxygen header. All stringified `SIT_STRINGIFY(...)` macros replaced with numeric literals matching `SIT_*` contract defines.

- **`sit/situation_impl_decl.h`** — Embedded GLSL string bodies removed entirely. No `#version` directives remain. Replaced with `SIT_GPU_PATH_*` string constants (`"sit/gpu/compositor.vert"`, etc.) and a one-line canonical-location comment.

- **`sit/situation_impl_renderer.h`**
  - `_SituationLoadCoreShaderFile`: new internal loader — tries CWD path as-is, then `../`, `../../`, `../../../`, `../../../../` prefixes, then exe-base-relative with the same prefix set (B5).
  - `_SituationInjectGLSLDefinesAfterVersion`: injects `#define SITUATION_USE_OPENGL` (or `_VULKAN`) immediately after `#version` line so backend `#if` branches resolve correctly at compile time.
  - `_SituationCreateGLCoreShaderProgram`: new GL helper — loads vert + frag from `sit/gpu/`, injects backend define, compiles and links.
  - `_SituationVulkanCompileCoreShaderFile`: new VK helper — loads GLSL from `sit/gpu/`, passes backend define and optional extra macros (used for `SIT_COMPOSITOR_PATH_A`/`PATH_B`) to shaderc.
  - `_SituationInitOpenGL`: VD + composite shader programs now loaded via `_SituationCreateGLCoreShaderProgram(SIT_GPU_PATH_COMPOSITOR_VERT, SIT_GPU_PATH_VD_FRAG)` etc.
  - `_SituationVulkanInitInternalRenderers`: VD, composite, text pipelines loaded via `_SituationVulkanCompileCoreShaderFile`. `compositor.vert` compiled twice — once with `SIT_COMPOSITOR_PATH_B` (VD) and once with `SIT_COMPOSITOR_PATH_A` (composite).
  - `_SituationInitQuadRenderer`, `_SituationInitYpqGradeRenderer`, `_SituationInitTextRenderer`: all wired to `SIT_GPU_PATH_*` constants.
  - Stale `@par` Doxygen comments referencing `SIT_VD_VERTEX_SHADER_SRC`, `SIT_QUAD_VERTEX_SHADER` updated to reflect `sit/gpu/` paths.

- **`doc/COMPILATION_GUIDE.md`** — Updated: `sit/gpu/` documented as the core library internal shader root; absent root `shaders/` reference removed.

- **`doc/plan/EXTERNALIZE_GPU_COMPUTE_PLAN.md`** — Phases 0–1C checkbox audit complete; Phase 1B verify steps confirmed; Phase 3 closed; B5 path-resolution search order documented.

### Verification

Both backends rebuilt and tested.

| Module | GL | VK |
|---|---|---|
| `core` | 40/40 ✅ | 40/40 ✅ |
| `graphics` | 98/98 ✅ | 88/88 ✅ |
| `virtual_display` | 26/26 ✅ | 26/26 ✅ |

B5 path-fallback confirmed: `virtual_display` 26/26 from `build/` CWD on both backends.

### Notes

- Phase 2 (SPIR-V embed, no shaderc) is **not** part of this patch — remains parked. Vulkan internal pipelines still require `SITUATION_ENABLE_SHADER_COMPILER` at runtime.
- K-Term and Polysonix shader paths are untouched.

---

---

---

---

---

---

## [v2.4.255] - 2026-06-12

### Description

**v2.4.255**: Fix silent VK init lie — `SITUATION_ERROR_SHADER_COMPILER_REQUIRED` (-757).

`_SituationVulkanInitInternalRenderers` previously returned `SITUATION_SUCCESS` when `SITUATION_ENABLE_SHADER_COMPILER` was not defined, leaving all internal VK pipelines (VD compositor, composite, quad, text, YPQ) as `VK_NULL_HANDLE`. Draw calls silently did nothing. Now returns a hard error with a descriptive message pointing to the build flag and Phase 2 of the externalize plan.

### Changes

- **`sit/situation_base_errno.h`** — New error code `SITUATION_ERROR_SHADER_COMPILER_REQUIRED` (-757): `"Vulkan: internal pipelines require SITUATION_ENABLE_SHADER_COMPILER; recompile the library with shaderc support"`. Added in `SITUATION_ERRORS_VULKAN`; auto-included in the X-macro enum and `SituationErrorToString` table.

- **`sit/situation_impl_renderer.h`** — `_SituationVulkanInitInternalRenderers`: the `#if !defined(SITUATION_ENABLE_SHADER_COMPILER)` early-exit now calls `_SituationSetErrorFromCode(SITUATION_ERROR_SHADER_COMPILER_REQUIRED, ...)` with a full diagnostic message instead of silently returning `SITUATION_SUCCESS`. Updated `@warning` and added `@return` tag in the doc comment.

### Verification

Both backends rebuilt from source and test harnesses rebuilt against fresh static libs.

| Module | GL | VK |
|---|---|---|
| `core` | 40/40 ✅ | 40/40 ✅ |
| `graphics` | 98/98 ✅ | 88/88 ✅ |
| `virtual_display` | 26/26 ✅ | 26/26 ✅ |

### Notes

- Phase 2 (SPIR-V embed, shaderc-free VK distribution) remains parked. See `doc/plan/EXTERNALIZE_GPU_COMPUTE_PLAN.md` Phase 2.

---

---

---

---

---

---

## [v2.4.256] - 2026-06-12

### Description

**v2.4.256**: `SituationCmdSetMultisampleState` — GL fully implemented; Vulkan shadow state tracked; `SituationGetGraphicsCaps::max_msaa_samples` corrected on Vulkan.

The function was a hard `NOT_IMPLEMENTED` stub since v2.4.185 with a misleading message ("deferred until MSAA render targets are exposed"). Root cause analysis established the correct split: GL multisample state (`GL_SAMPLE_SHADING`, `glMinSampleShading`, `glSampleMaski`, `GL_SAMPLE_ALPHA_TO_COVERAGE`) is genuine per-draw dynamic state and can be implemented right now. Vulkan multisample state is baked into `VkPipelineMultisampleStateCreateInfo` at pipeline compile time — it is not dynamic in Vulkan 1.4 core and the `VK_EXT_extended_dynamic_state3` sample-shading/alpha-to-coverage features are not loaded. The VK path now records the desired state into shadow fields so push/pop brackets are symmetric and Phase 6 MSAA pipeline variants can consult them when compiled.

### Changes

- **`sit/situation_impl_decl.h`**
  - `SIT_OP_SET_MULTISAMPLE_STATE` added to the `SitOpCode` enum.
  - `struct { SituationMultisampleState ms; } set_multisample_state` added to `SitCommandPacketArgs` union.
  - `_SitGLRasterStackEntry` — four new fields: `multisample_sample_shading`, `multisample_min_shading`, `multisample_sample_mask`, `multisample_alpha_to_coverage`. Push/pop now saves and restores all multisample GL state.
  - `_SitVulkanRasterStackEntry` — four new fields: `ms_sample_shading_enable`, `ms_min_sample_shading`, `ms_sample_mask`, `ms_alpha_to_coverage_enable`. Push/pop stores and restores shadow values.
  - Vulkan state struct — four new shadow fields: `dynamic_ms_sample_shading_enable`, `dynamic_ms_min_sample_shading`, `dynamic_ms_sample_mask`, `dynamic_ms_alpha_to_coverage_enable`.

- **`sit/situation_impl_renderer.h`**
  - `_SitGLCaptureRasterState` — captures multisample state via `glIsEnabled(GL_SAMPLE_SHADING)`, `glGetFloatv(GL_MIN_SAMPLE_SHADING_VALUE)`, `glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, 0, …)`, `glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE)`.
  - `_SitGLApplyRasterState` — restores all four multisample GL states.
  - `_SitVulkanCaptureRasterState` / `_SitVulkanApplyRasterState` — include the four new shadow fields.
  - GL execute-commands switch — `case SIT_OP_SET_MULTISAMPLE_STATE` implemented: applies all three GL enable/disable calls plus `glMinSampleShading`. `sample_mask == 0` maps to `0xFFFFFFFFu` ("all samples pass").
  - `SituationCmdSetMultisampleState` — GL path enqueues the packet; VK path stores shadow state and returns `SITUATION_SUCCESS` (no GPU command until Phase 6 MSAA pipeline variants ship).

- **`sit/situation_impl_ctrl.h`**
  - `SituationGetGraphicsCaps` Vulkan path — replaces the `max_msaa_samples = 1` placeholder with a real `vkGetPhysicalDeviceProperties` query: intersects `framebufferColorSampleCounts` and `framebufferDepthSampleCounts`, then returns the highest power-of-two sample count both support.

### Constraints and Remaining Work

The Vulkan path records intent but does not dispatch GPU commands. Full Vulkan MSAA requires:
1. MSAA render targets (Phase 6) — `VkImage` with `rasterizationSamples > 1`, matching render pass attachment, resolve attachment.
2. MSAA pipeline variants — `VkPipelineMultisampleStateCreateInfo` populated from the shadow fields at pipeline compile time, once a render target with `msaa_samples > 1` is bound.
3. Optional: load `VK_EXT_extended_dynamic_state3` sample-shading/alpha-to-coverage features for truly dynamic per-draw control without pipeline rebuilds.

The `v2.5-api-expansion.md` plan has been updated accordingly.

### Verification

GL backend: `SituationCmdSetMultisampleState` compiles, enqueues correctly, and the execute-commands path dispatches the four GL calls. `SituationGetGraphicsCaps` returns the correct GL `max_msaa_samples` (unchanged). Push/pop brackets save and restore multisample state alongside all existing raster state.

Vulkan backend: function returns `SITUATION_SUCCESS`, shadow fields are populated, push/pop is symmetric. `SituationGetGraphicsCaps::max_msaa_samples` now reflects actual device limits from `VkPhysicalDeviceLimits`.

Three tests added to `tests/harness/test_graphics.c`:
- `multisample_state_no_crash` — three state variants inside a render pass, verifies `SITUATION_SUCCESS` return and that subsequent draws produce pixels.
- `multisample_state_push_pop` — push/pop bracket around a non-default state, verifies the draw after pop still produces red pixels and the stack is symmetric.
- `multisample_caps_nonzero` — asserts `SituationGetGraphicsCaps::max_msaa_samples >= 1` on Vulkan (GL caps query has a pre-existing thread limitation; GL path passes unconditionally).

Both static libs and both static harnesses rebuilt from source and run clean.

| Module / filter | GL | VK |
|---|---|---|
| `graphics --filter multisample` | 3/3 ✅ | 3/3 ✅ |
| `graphics --filter raster` | 2/2 ✅ (no regression) | 2/2 ✅ (no regression) |

Hardware: NVIDIA GeForce GTX 1070, Windows 10, GCC 15.1.0 (MinGW-w64), Vulkan SDK 1.4.313.2.

---

---

---

---

---

---

## [v2.4.257] - 2026-06-12

### Description

**v2.4.257**: VD parity — two correctness fixes to the Virtual Display subsystem.

1. **Vulkan `CmdBeginRenderPass` VD load op ignored.** When targeting a VD with `SIT_LOAD_OP_LOAD`, Vulkan always cleared the VD instead of preserving content. The VD render pass was created once with `VK_ATTACHMENT_LOAD_OP_CLEAR` baked in; `info->color_attachment.loadOp` was never consulted. On OpenGL this worked correctly (GL execute path already branches on `loadOp`). Fix: each VD now pre-creates two `VkRenderPass` objects at construction time — a CLEAR variant (default) and a LOAD variant. `CmdBeginRenderPass` selects between them at record time based on the caller's `loadOp`.

2. **`SituationGetVirtualDisplayTexture` returned `RESOURCE_INVALID` for regular VDs.** The texture registry registration was gated on `SITUATION_VD_FLAG_COMPUTE_TARGET`. Non-compute VDs had `texture_slot_index == -1`, so `GetVirtualDisplayTexture` always failed with a misleading error suggesting the caller should use `COMPUTE_TARGET`. Fix: all VDs now register in the texture registry at creation time. Compute targets get `SAMPLED | STORAGE | TRANSFER_SRC` usage; raster VDs get `SAMPLED | TRANSFER_SRC`. The API is now unconditionally available.

### Changes

- **`sit/situation_api.h`** — `SituationVirtualDisplay::vk` struct gains `render_pass_load` (`VkRenderPass`) alongside the existing `render_pass`. Comment updated to distinguish the two variants.

- **`sit/situation_impl_vd.h`**
  - `SituationCreateVirtualDisplayEx` VK path — after creating `vd->vk.render_pass` (CLEAR), immediately creates `vd->vk.render_pass_load` (LOAD/LOAD initial layouts). Failure to create the LOAD variant is non-fatal: logs a warning and falls back to CLEAR.
  - VK failure-cleanup block — destroys `render_pass_load` if it was created before the failure point.
  - `SituationDestroyVirtualDisplay` VK path — defers `render_pass_load` via `_SituationDeferDestroyRenderPass` alongside `render_pass`.
  - Texture registration block changed from `if (is_compute_target)` to unconditional. Usage flags set per VD type: `SAMPLED | STORAGE | TRANSFER_SRC` for compute targets, `SAMPLED | TRANSFER_SRC` for raster VDs. Failure cleanup updated to destroy the full set of resources created up to that point.
  - `SituationGetVirtualDisplayTexture` doc comment updated to reflect it now works for all VDs. Stale error message removed.

- **`sit/situation_impl_renderer.h`** — `SituationCmdBeginRenderPass` VK path, VD branch: selects `vd->vk.render_pass_load` when `info->color_attachment.loadOp == SIT_LOAD_OP_LOAD` and the load variant is available; falls back to `vd->vk.render_pass` for CLEAR and DONT_CARE (and as a safety net if load variant is null).

### Verification

Both static libs and harnesses rebuilt from source. Full `virtual_display` module run on GL and VK — all 28 tests pass (was 26 before this patch).

| Module / filter | GL | VK |
|---|---|---|
| `virtual_display` (full) | 28/28 ✅ | 28/28 ✅ |
| `virtual_display --filter vd_get_texture_handle` | ✅ 6 assertions | ✅ 6 assertions |
| `virtual_display --filter vd_load_op_preserves_content` | ✅ 23 assertions, center pixel red | ✅ 23 assertions, center pixel red |

Hardware: NVIDIA GeForce GTX 1070, Windows 10, GCC 15.1.0, Vulkan SDK 1.4.313.2.

---

---

---

---

---

---

## [v2.4.258] - 2026-06-12

### Description

**v2.4.258**: Phase 27 — `SituationQueryShaderStorageBlocks` + `spirv_ssbo_reflection_bindings` harness test.

Completes the final optional phase of the graphics harness SSBO upgrade plan (`doc/plan/TEST_HARNESS_GRAPHICS_UPGRADE.md`). Phase 27 adds a public API to enumerate a shader's active SSBO blocks and their assigned GL binding points, enabling pre-draw reflection checks that fail fast before any framebuffer readback is needed.

The prior gap: Phases 23–26 proved correct binding via pixel readback (R/G channels encode tag values). Phase 27 proves the same contract at the reflection layer — catching the v2.4.81/82 duplicate-binding regression at the GL program interface level without drawing a frame.

### Changes

- **`sit/situation_api.h`**
  - New struct `SituationShaderStorageBlockInfo { char name[128]; uint32_t binding_point; uint32_t block_index; }`.
  - New function `SituationQueryShaderStorageBlocks(shader, out_blocks, capacity, out_count)` — `[OpenGL]` enumerates active SSBO blocks and their post-link binding points (after `_SituationBindGLProgramStorageBlocks`). Count-only mode when `out_blocks == NULL || capacity == 0`. Returns `NOT_IMPLEMENTED` on Vulkan.

- **`sit/situation_impl_renderer.h`**
  - `SituationQueryShaderStorageBlocks` implemented in the shared section with `#if !defined(SITUATION_USE_OPENGL)` stub (returns `NOT_IMPLEMENTED`, `*out_count = 0`) and `#else` OpenGL body using `glGetProgramInterfaceiv` / `glGetProgramResourceiv` / `glGetProgramResourceName`.

- **`tests/harness/test_graphics.c`**
  - New test `test_spirv_ssbo_reflection_unique_bindings` (registered as `spirv_ssbo_reflection_bindings`):
    - Loads the Phase 24 dual-SSBO fragment shader.
    - Count-only query: asserts 2 active blocks (skip if driver reports 0 — some stripped builds).
    - Full query: asserts `blocks[0].binding_point == 0`, `blocks[1].binding_point == 1` (unique, in order).
    - Logs block names for diagnostics (SPIR-V names may be empty/mangled).
    - Skips gracefully on non-SPIR-V builds.
  - Test registered in `graphics_tests[]` between `uniform_1iv_int_array` and `demon_hunt_sky_shader_link`.

### Verification

Rebuild: `build_situation.bat static-opengl` → `build_tests.bat static-opengl`

```
build\tests\sit_test_opengl.exe --filter spirv_ssbo_reflection
build\tests\sit_test_opengl.exe --module graphics
```

---

---

---

---

---

---

## [v2.4.259] - 2026-06-12

### Description

**v2.4.259**: Phase A (plan_handles_ssbo) — Bindless API hardening.

Closes all five items from Phase A of `doc/plan/plan_handles_ssbo.md`. No new public API surface beyond one new error code; all changes are correctness fixes and test hardening.

### Changes

- **`sit/situation_base_errno.h`**
  - New error code `SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED` (-515, `SITUATION_ERRORS_RENDERING`):
    "Mesh vertex/index buffer device address unavailable (no SHADER_DEVICE_ADDRESS support on this backend/hardware)".
    Reserved for Phase B mesh BDA API; documents the intended signal for GL fallback paths. Slotted between `SITUATION_ERROR_BUFFER_INVALID_USAGE` (-514) and the texture block (-520).

- **`sit/situation_impl_renderer.h` — `SituationGetBufferDeviceAddress`**
  - GL path: removed the silent zero-return when `GL_NV_shader_buffer_load` / `GL_EXT_buffer_reference` are absent.
    Now calls `_SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, ...)` with a diagnostic message
    directing callers to gate on `SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)`.
  - Minor cleanup: removed stale "1.0/1.1 extension" comment from VK path (library requires Vulkan 1.4).

- **`sit/situation_impl_renderer.h` — `SituationGetTextureHandle`**
  - VK path: replaced raw `texture.slot_index` return with a `_SitGetTextureSlot` validation call.
    Stale / destroyed handles now return 0 + `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED` instead of
    silently aliasing a recycled slot in `global_bindless_set`.
  - Docstring updated: VK return value described as "descriptor index into global_bindless_set" (not "unimplemented").

- **`tests/harness/test_graphics.c` — `test_get_texture_handle`** (A3)
  - VK: asserts returned handle is non-zero for a live texture (slot 0 is null/default; live textures must occupy a real slot).
  - GL + `SIT_FEATURE_BINDLESS_TEXTURES`: asserts non-zero.
  - GL without bindless: asserts zero (graceful skip).
  - Post-destroy: asserts that a destroyed texture yields zero (stale handle detection).

- **`tests/harness/test_graphics.c` — `test_buffer_device_address`** (A4)
  - VK: asserts returned address is non-zero (bufferDeviceAddress is required for STORAGE_COMPUTE buffers).
  - GL + `SIT_FEATURE_BINDLESS_BUFFERS`: asserts non-zero.
  - GL without bindless: asserts zero *and* that `SituationGetLastErrorCode()` returns `SITUATION_ERROR_OPENGL_UNSUPPORTED`.

### Plan status

`doc/plan/plan_handles_ssbo.md` — Phase A items A1–A5 checked.

### Verification

Both static backends rebuilt and graphics module run clean (same build run as v2.4.260):

```
sit_test_opengl.exe --module graphics   104 passed  0 failed
sit_test_vulkan.exe --module graphics    93 passed  0 failed
```

Hardened tests confirmed passing on both backends:

| Test | OpenGL | Vulkan |
|------|--------|--------|
| `get_texture_handle` (now asserts nonzero + stale-handle = 0) | OK | OK |
| `buffer_device_address` (now asserts nonzero on VK; GL checks error code) | OK | OK |

**Compile fix included**: A2 initially referenced `slot->slot_index` on `_SituationTextureSlot`, which has no such field — corrected to `texture.slot_index` (the handle carries the index; the slot is looked up by it). Caught by the VK static build; fixed before tests ran.

---

---

---

---

---

---

## [v2.4.260] - 2026-06-12

### Description

**v2.4.260**: Phase B (plan_handles_ssbo) — Mesh vertex/index buffer device address API.

Adds `SituationGetMeshVertexBufferAddress` and `SituationGetMeshIndexBufferAddress` to the public API,
the missing prerequisite for vertex-pull shaders and GPU-driven index fetch (Phase C / E).

### Changes

- **`sit/situation_impl_renderer.h` — `SituationCreateMesh` VK path**
  - Vertex buffer: added `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` to the usage flags
    (`VERTEX | STORAGE | TRANSFER_DST | SHADER_DEVICE_ADDRESS`).
    Previously calling `vkGetBufferDeviceAddress` on the vertex buffer was technically invalid.
  - Index buffer: same bit added (`INDEX | TRANSFER_DST | SHADER_DEVICE_ADDRESS`), enabling
    GPU-driven index fetch via `SituationGetMeshIndexBufferAddress`.

- **`sit/situation_api.h`**
  - New declaration: `SITAPI uint64_t SituationGetMeshVertexBufferAddress(SituationMesh mesh)`
  - New declaration: `SITAPI uint64_t SituationGetMeshIndexBufferAddress(SituationMesh mesh)`
  - Both inserted between `SituationDestroyMesh` and `SituationGetBufferDeviceAddress`.

- **`sit/situation_impl_renderer.h` — new SITAPI implementations**
  - `SituationGetMeshVertexBufferAddress`: VK → `vkGetBufferDeviceAddress` on `slot->vertex_buffer`,
    gated on `SIT_FEATURE_BINDLESS_BUFFERS`. GL → `glGetNamedBufferParameterui64v` via `GL_NV_shader_buffer_load`
    where available; returns 0 + `SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED` on AMD/Intel/Mesa.
  - `SituationGetMeshIndexBufferAddress`: same contract for `slot->index_buffer`; returns 0
    for vertex-only meshes (no index buffer allocated at creation).
  - Both functions return 0 on a stale/destroyed handle.

- **`sit/situation_base_trace.h`**
  - New trace entries: `SIT_TRACE_SituationGetMeshVertexBufferAddress` (10100028),
    `SIT_TRACE_SituationGetMeshIndexBufferAddress` (10100029). Existing entries from
    `CmdBindSampledTexture` onward renumbered +2.

- **`tests/harness/test_graphics.c`**
  - New test `test_mesh_vertex_buffer_address`: creates a 3-vertex indexed mesh, queries both
    addresses, asserts nonzero on VK / NV-GL, asserts zero + correct error code on non-NV GL.
    Also verifies stale-handle returns 0 after destroy.
  - New test `test_mesh_index_buffer_address_vertex_only`: creates a vertex-only mesh (no index
    buffer), asserts `GetMeshIndexBufferAddress` returns 0 on all backends.
  - Both tests registered in the `// Meshes` section of `graphics_tests[]`.

### Plan status

`doc/plan/plan_handles_ssbo.md` — Phase B items B1–B5 checked (B5 = this log + API doc comments).

### Verification

Both static backends rebuilt and graphics module run clean:

```
static-opengl → build\dll\situation_opengl.a   GCC 15.1.0  OpenGL 4.6  GTX 1070
static-vulkan → build\dll\situation_vulkan.a   GCC 15.1.0  Vulkan 1.4  GTX 1070

sit_test_opengl.exe --module graphics   104 passed  0 failed   1.08s
sit_test_vulkan.exe --module graphics    93 passed  0 failed  17.29s
```

New tests confirmed passing on both backends:

| Test | OpenGL | Vulkan |
|------|--------|--------|
| `mesh_vertex_buffer_address` | OK | OK |
| `mesh_index_buffer_address_vertex_only` | OK | OK |

(Vulkan count is 11 lower than OpenGL because GL-only tests — `uniform_*`, SSBO fragment, spirv explicit bind — are `#ifdef SITUATION_USE_OPENGL`-guarded. Expected.)

---

---

---

---

---

---

## [v2.4.261] - 2026-06-13

### Description

**v2.4.261**: Audio node graph correctness bugfixes + ISA 110 node integration.

Three fixes from the `plan_audio_registry` reality check session.

### Changes

- **`sit/aud/node_graph_impl.h` — `SituationRemovePatch` bugfix**
  - Added `_SituationRemovePatchFromArray()` helper (forward-declared at top of file).
  - `SituationRemovePatch` now removes the patch from both `graph->patches[]` (was done)
    and from `src->output_patches[]` and `dst->input_patches[]` (was TODO/missing).
  - Also clears `dst->ctrl_inputs[dst_port].is_modulated` when the last control patch
    targeting that port is removed.

- **`sit/aud/node_graph_impl.h` — `SituationDestroyNode` bugfix**
  - Previously had `// TODO: Implement patch removal` and `// ... (omitted for brevity)`
    stubs — neither patches nor port buffers were freed on node destruction.
  - Now walks `graph->patches[]` in reverse, removes all patches that reference the dying
    node, and cleans up peer nodes' per-node patch lists and modulation flags.
  - Now frees all port buffers (`audio_inputs`, `audio_outputs`), control ports, control
    values, and per-node patch arrays — matching `SituationDestroyGraph` behaviour exactly.

- **`sit/aud/fx/isa110.h`** (DSP — no change, was already complete)

- **`sit/situation_api.h`**
  - New enum value: `SITUATION_NODE_ISA110` — Focusrite ISA 110 preamp + 4-band inductor EQ.
    Inserted after `SITUATION_NODE_DEAFMAX` in the Effects block.

- **`sit/aud/registry_init.h`**
  - New function `_SituationRegisterISA110()` — registers 14 controls: drive, hpf_cutoff,
    hpf_enabled, band0–3 freq/gain (+ Q for the two bell bands), output_gain.
  - Called from `SituationInitDeviceRegistry()` in the Effects block (now 17 effects).

- **`sit/aud/device_wrappers.h`**
  - Added `#include "fx/isa110.h"`.
  - New `_SituationCreateISA110` — allocates `ISA110Processor`, calls `isa110_init` at
    playback sample rate (falls back to 48 kHz).
  - New `_SituationProcessISA110Node` — applies controls to preamp + 4 EQ bands per block,
    calls `isa110_process`, then applies post-EQ output_gain in a scalar loop.
  - New `_SituationDestroyISA110` — calls `isa110_reset` then frees.
  - Added `SITUATION_NODE_ISA110` entry to `g_device_function_table[]`.

- **`sit/aud/midi_device_callbacks.h`**
  - New `SIT_MODEL_ISA110 = 0x13` in the `SIT_MidiDeviceModel` enum.
  - New `_SituationISA110OnControlChange` — 14 CC mappings (CC 7, 17–27, 64, 80).
  - Added entry to `g_midi_callback_table[]` and `SIT_GetDeviceIdentity()` switch.

- **`doc/plan/plan_audio_registry.md`** — updated checkboxes and open-items table.

---

---

---

---

---

## [v2.4.262] - 2026-06-13

### Description

**v2.4.262**: All audio node create functions now use the actual device sample rate.

Every `_SituationCreate*` wrapper in `device_wrappers.h` was initializing its DSP state at a
hardcoded 48000 Hz. If the device runs at 44100, 96000, or 192000, all time-based parameters
(delay lengths, reverb comb filters, LFO rates, filter coefficients, EQ bands) were computed
at the wrong rate and would produce audibly incorrect results.

All create functions now call `SituationGetAudioPlaybackSampleRate()` and fall back to 48000 only
if the audio device is not yet active (the function returns 0 in that case).

The serialization sample rate field and the MIDI device default also benefit from the same fix.

### Changes

- **`sit/aud/device_wrappers.h`** — all `_SituationCreate*` functions (18 affected):
  - `_SituationCreateReverb` — `_SituationInitReverb(48000)` → live rate (uint32_t)
  - `_SituationCreateEcho` — `state->sample_rate = 48000` → live rate
  - `_SituationCreateChorus` — `SituationChorus4Stage_Init(chorus, 48000, ...)` → live rate; max_delay_samples also scales with rate
  - `_SituationCreatePhaser` — `initPhaseShifter(..., 48000, ...)` → live rate
  - `_SituationCreateOverdrive` — `sit_overdrive_init(..., 48000)` → live rate
  - `_SituationCreateExciter` — `init_exciter(..., 48000)` → live rate
  - `_SituationCreateStudioReverb` — `_SituationStudioReverbCreate(48000)` → live rate
  - `_SituationCreateSpringReverb` — `SpringReverb_init(..., 48000)` → live rate
  - `_SituationCreateSST282` — `sst282_init(..., 48000)` → live rate
  - `_SituationCreateFilter` — `filter_init(..., 48000)` → live rate
  - `_SituationCreateEQ4Band` — `eq4band_init(..., 48000)` → live rate
  - `_SituationCreateDynamics` — `dynamics_init(..., 48000)` → live rate
  - `_SituationCreateCompander` — `compander_init(..., 48000)` → live rate
  - `_SituationCreateLFO` — `lfo_init(..., 48000)` → live rate
  - `_SituationCreateSoundSource` — `sound_source_init(..., 48000)` → live rate
  - `_SituationCreateMaximizer` — `init_maximizer(..., 48000, ...)` → live rate
  - `_SituationCreateMasteringAmp` — `_SituationMasteringAmpInit(..., 48000)` → live rate
  - `_SituationCreateMicCapture` — `mic_capture_init(..., 48000)` → live rate
  - `_SituationCreateGain` — `situation_gain_init(..., 48000)` → live rate
  - `_SituationCreateEnvelopeFollower` — `situation_envf_init(..., 48000)` → live rate
  - `_SituationCreatePeakMeter` — `situation_peak_meter_init(..., 48000)` → live rate
  - Already correct: `_SituationCreateToneSynth`, `_SituationCreateISA110` (used live rate)

- **`sit/aud/node_graph_serialization_impl.h`**
  - `"sample_rate": 48000` hardcode → `SituationGetAudioPlaybackSampleRate()` with 48000 fallback.
  - Removes the `TODO: Get from audio context` comment.

- **`sit/aud/midi_device.h`**
  - `device->sample_rate = 48000.0` default → `SituationGetAudioPlaybackSampleRate()` with 48000 fallback.

- **`doc/plan/plan_audio_registry.md`** — item 8 resolved.

---

---

---

---

---

## [v2.4.264] - 2026-06-13

### Description

**v2.4.264**: Audio node graph correctness fixes — three small but meaningful bugs addressed.

### Changes

- **`sit/aud/device_wrappers.h`** — `_SituationProcessMixerNodeNode`
  - Changed `break` to `continue` in the input port summing loop. Previously a null
    buffer pointer in any port slot would silently abort summing, dropping all inputs
    from that index onward. Sparse port assignment is not used today, but the old code
    was a latent correctness hazard. Now correctly skips null ports and sums all connected
    inputs regardless of their indices.

- **`sit/aud/node_graph_serialization_impl.h`** — `SituationIsVersionCompatible()`
  - Replaced exact `strcmp` version match with semantic major-version comparison.
    Files written by any `2.x.y` library are now accepted by any other `2.x.y` library.
    Different major versions remain incompatible. Removes the previous silent breakage
    that would reject any valid file on the first format-version increment.
  - Removes `TODO: Implement semantic versioning comparison` comment.

- **`sit/aud/node_graph_serialization_impl.h`** — `_JSONParseNode()`
  - When a node type name from a JSON file cannot be resolved in the registry, the
    error now logs a descriptive message via `SituationLog(SIT_LOG_ERROR, ...)` naming
    the unresolvable type string and advising the caller to call
    `SituationRegisterDeviceType()` before loading. Previously the failure was silent
    (only `parser->error_message` was set, visible nowhere unless the caller inspected
    internal parser state).

- **`doc/plan/plan_audio_registry.md`** — items 9, 11, 16 marked resolved.



### Description

**v2.4.263**: Vertex-pull GLSL include + `SituationCmdSetVertexAttribute` deprecation.

Closes the two remaining open items in `doc/plan/AAA_ARCHITECTURE_PLAN.md` §2 (SSBO-First /
Vertex Pulling). Section 2 is now fully complete.

### Changes

- **`sit/gpu/vertex_pull.glslh`** *(new file)*
  - Canonical GLSL include for Vulkan vertex-pull shaders.
  - Three `layout(buffer_reference, scalar)` structs matching the three mesh layouts
    that `SituationCreateMesh` produces:
    - `SitVertexBuffer_Simple` / `SitVertex_Simple` — stride 12, position only (`vec3`).
    - `SitVertexBuffer_Legacy` / `SitVertex_Legacy` — stride 32, `pos + normal + texcoord`.
    - `SitVertexBuffer_PBR` / `SitVertex_PBR` — stride 48, `pos + normal + tangent(vec4) + texcoord`.
    - `SitIndexBuffer` — `uint[]` reference for GPU-driven index fetch via `SituationGetMeshIndexBufferAddress`.
  - `sit_bitangent(normal, tangent)` helper — reconstructs bitangent from stored tangent W handedness, avoiding a fourth attribute.
  - Full layout comment block documenting byte offsets, `SIT_ATTR_*` location assignments, and usage pattern.
  - Requires `GL_EXT_buffer_reference`, `GL_EXT_buffer_reference2`, `GL_EXT_scalar_block_layout`.
    All three are core to the Vulkan 1.2+ GLSL dialect used by shaderc with `--target-env vulkan1.2`.

- **`sit/situation_api.h`** — `SituationCmdSetVertexAttribute`
  - Comment updated: `[OpenGL Only]` → `[OpenGL Only, Deprecated v2.4]`.
  - Migration note added: prefer `SituationGetMeshVertexBufferAddress` + `vertex_pull.glslh`
    on Vulkan. Full removal is a v2.5 breaking change.

- **`doc/plan/AAA_ARCHITECTURE_PLAN.md`** — §2 items checked, section marked complete.

---

---

---

---

---

---

## [v2.4.265] - 2026-06-13

### Description

**v2.4.265**: YPQ Phase 3 — public mapping-quality diagnostics API + `test_misc.c` cleanup.

### Changes

- **`sit/situation_api.h`** — new types and API in the YPQ pixel section:
  - Added `SituationYpqRgbMappingStats` struct (ypq_mappings, unique_rgb, duplicate_mappings,
    rgb_holes, worst_axis_dup, worst_axis_at).
  - Added `SituationYpqAnalyzeRgbMapping(out)` — full 256³ scan; counts unique 8-bit RGB
    outputs, duplicate mappings, unreachable RGB holes, and worst fixed-Q slice duplicate
    count. O(16M) calls to `SituationColorFromYPQ`; ~1.8 s on a modern CPU. Guard with
    `SIT_SKIP_YPQ_RGB_STATS` in CI.
  - Added `SituationYpqSliceDuplicateCount(axis, value, out_dup)` — count duplicate RGB
    outputs in one 65 536-entry fixed-axis YPQ slice. axis ∈ {'Y','P','Q'}, value ∈ [0,255].
    Uses internal radix sort. Notable: Q=0 always yields ≥65 000 duplicates (all gray).

- **`sit/situation_impl_image.h`** — two SITAPI implementations after `SituationImageAdjustYPQ`:
  - ~~`SituationYpqAnalyzeRgbMapping`~~ → moved to `situation_impl_color.h` (see below)
  - ~~`SituationYpqSliceDuplicateCount`~~ → moved to `situation_impl_color.h` (see below)

- **`sit/situation_impl_color.h`** — two SITAPI implementations appended before `#endif`:
  - `SituationYpqAnalyzeRgbMapping` — 16 MB hit bitmap, two-pass scan (global unique + per-Q
    slice worst-dup), proper SIT_CALLOC/SIT_FREE. Uses `_SitRgbFromYpqBytes` directly.
  - `SituationYpqSliceDuplicateCount` — 65 536-key fill loop + 4-pass byte radix sort.
    Uses `_SitRgbFromYpqBytes` directly. Both functions belong here rather than in
    `situation_impl_image.h` — they are pure YPQ math with no image dependency.
  - Added `#include <stdint.h>` and `#include <string.h>` at top (previously only `<math.h>`).
  - Updated file header comment to reflect the new public API presence.

- **`sit/aud/fx/isa110.h`** — fixed pre-existing mismatched parenthesis in `isa110_apply_biquad`
  (one extra `)` on line 107 prevented the library from compiling; unrelated to YPQ).

- **`tests/harness/test_misc.c`** — Phase 3 promotion and cleanup:
  - **Deleted** all private NTSC YIQ constants (`MISC_YIQ_MAX_I/Q`, `MISC_YIQ_RI/GI/BI/RQ/GQ/BQ`,
    `MISC_INV255`) — zero harness-local matrix duplication remains.
  - **Deleted** private fast-path helpers: `misc_ypq_y_lin[]`, `misc_ypq_sin_p/cos_p[]`,
    `misc_ypq_init_lut()`, `misc_ypq_unit_to_byte()`, `misc_ypq_fast_rgb_key()`,
    `misc_ypq_rgb_key_from_bytes()`, `misc_ypq_fast_from_bytes()`.
  - **Deleted** sweep/cube builder: `misc_ypq_fill_y_p_plane_bytes()`, `misc_ypq_fill_ypq_y_p_plane()`,
    `misc_ypq_build_sweep_and_cube()`.
  - **Deleted** private analytics: `misc_ypq_radix_sort_u32()`, `misc_ypq_count_duplicates_sorted()`,
    `MiscYpqRegistryDupReport`, `misc_ypq_scan_registry_dup_report_from_cube()`,
    `misc_ypq_report_registry_from_cube()`, `misc_ypq_print_registry_dup_report()`,
    `misc_ypq_report_rgb_duplicate_stats()`.
  - **Removed** unused `SIT_YPQ_CUBE_KEYS`, `SIT_YPQ_RGB_KEY_COUNT`, `SIT_YPQ_CUBE_SIZE` defines.
  - **Refactored** `misc_sample_ypq_plane_rgba()` → calls `SituationColorFromYPQ` directly.
  - **Refactored** `test_ypq_to_rgb_y_p_plane` — replaced `misc_ypq_fill_ypq_y_p_plane` with inline
    `SituationColorFromYPQ` loop; replaced `lib vs fast` consistency check with double-call parity.
  - **Refactored** `test_ypq_to_rgb_q_sweep_4s` — sweep cache now filled via `SituationColorFromYPQ`;
    post-sweep stats call replaced with `SituationYpqAnalyzeRgbMapping`; cube_keys allocation removed.
  - **Added** `test_ypq_analyze_rgb_mapping` — full 256³ scan assertions; skip with
    `SIT_SKIP_YPQ_RGB_STATS`. Validates unique_rgb ∈ [5M, 7M], duplicate_mappings identity,
    rgb_holes identity, worst_axis_dup > 0. Measured: unique_rgb=5 636 038 in ~1.79 s.
  - **Added** `test_ypq_slice_dup_q0` — asserts Q=0 slice has ≥65 000 duplicates. Measured: 65 280.
  - Both new tests registered in `misc_tests[]`.

- **`doc/plan/YPQ_COLOR_PLAN.md`** — Phase 3 checkboxes updated (see below).

### Phase gate (Phase 3 complete)
- `sit_test_opengl.exe --module misc` (with sweeps/stats skipped): 40/40 ✓
- `ypq_analyze_rgb_mapping` (no env skip): PASSED — unique_rgb=5 636 038 in 1.79 s ✓
- `ypq_slice_dup_q0`: PASSED — Q=0 duplicates=65 280 ✓
- `grep MISC_YIQ_ tests/harness/test_misc.c` — zero hits ✓
- `grep misc_ypq_fast_rgb_key tests/harness/test_misc.c` — zero hits ✓

---

---

---

---

---

---

## [v2.4.266] - 2026-06-14

### Description

**v2.4.266**: Allocator consistency audit — `aud/` subsystem migrated from raw CRT allocator calls to `SIT_MALLOC`/`SIT_CALLOC`/`SIT_REALLOC`/`SIT_FREE`. One mixed-allocator bug fixed in `situation_impl_etc.h`. MyBuddy v1.6.2 is complete and ready at `sit/mybuddy/`; macro wiring is deferred pending identification of one remaining CRT allocation site in the OpenGL shutdown path.

### Changes

- **`sit/situation_impl_etc.h`** — `free(copy)` → `SIT_FREE(copy)` in `_sit_get_directory()`. The pointer was allocated with `_sit_strdup` (which uses `SIT_MALLOC`) but freed with raw `free()` — a latent mixed-allocator bug exposed by the integration attempt.
- **`sit/aud/midi.h`**, **`sit/aud/midi_device.h`**, **`sit/aud/midi_learn.h`** — All raw `malloc`/`calloc`/`realloc`/`free` calls replaced with `SIT_MALLOC`/`SIT_CALLOC`/`SIT_REALLOC`/`SIT_FREE`. The `dev->info.name` and `vdev->info.name` const-pointer frees remain as `free((void*)...)` because `SIT_FREE`'s `(p)=NULL` clause cannot assign to a cast expression.
- **`sit/aud/tone_synth_graph.h`**, **`sit/aud/fx/dynamics.h`**, **`sit/aud/fx/deafmax.h`**, **`sit/aud/fx/maximizer.h`** — Non-aligned allocations migrated to `SIT_MALLOC`/`SIT_FREE`. Aligned allocations in `maximizer.h` (`_aligned_malloc`/`_aligned_free`, `aligned_alloc`/`free`) left as-is with a comment — they require platform-specific alignment APIs.
- **`sit/aud/polysonix/polysonix.h`**, **`sit/aud/polysonix/px_patching.h`**, **`sit/aud/polysonix/dsp_math.h`**, **`sit/aud/polysonix/px_vm.h`**, **`sit/aud/polysonix/px_wave_rom.h`** — All raw allocator calls migrated to `SIT_MALLOC`/`SIT_CALLOC`/`SIT_REALLOC`/`SIT_FREE`.
- **`sit/aud/sound_source.h`** — `src->buffer` allocation migrated to `SIT_MALLOC`/`SIT_REALLOC`/`SIT_FREE`. This was the most fragile site: the struct owning `src->buffer` is `SIT_CALLOC`-allocated, but the buffer sub-field was `malloc`-allocated — a boundary that would cause corruption on any refactor routing it through `SIT_FREE`.

### Sub-library
MyBuddy at `sit/mybuddy/` is at version **1.6.2**, fully tested on Windows/MinGW. Integration into Situation is tracked at `doc/plan/MYBUDDY_SITUATION_INTEGRATION_PLAN.md` — Phase 1 is on hold pending ASAN-assisted identification of one remaining CRT allocation in the OpenGL shutdown path.

---

---

---

---

---

---

## [v2.4.267] - 2026-06-14

### Description

**v2.4.267**: Makefile build system migration. `build\build_situation.bat` is now a thin launcher that forwards targets to `sit/Makefile` via `mingw32-make`. The original build script is preserved verbatim as `build\build_situation_legacy.bat`. All six build targets (`opengl`, `vulkan`, `all`, `static-opengl`, `static-vulkan`, `clean`) are implemented with full flag/define/link parity against the legacy script. macOS and Linux scaffold blocks are included but inert on Windows. Verified: `static-opengl` forced rebuild matches the legacy compiler invocation exactly; `build_tests.bat static-opengl` links and produces `sit_test_opengl.exe` against the freshly built static lib.

### Changes

- **`build/build_situation.bat`** — rewritten as thin launcher. Validates target, resolves MinGW via `MINGW_PATH` env or `C:\msys64\mingw64\bin` default, forwards to `mingw32-make -C sit <target>`, propagates exit code. Does not set `VULKAN_SDK`, `SIT_OPTIMIZE_CFLAGS`, or `EXTRA_VULKAN_CFLAGS` (Makefile reads those from environment directly).
- **`build/build_situation_legacy.bat`** — verbatim copy of the original `build_situation.bat`, preserved as a fail-safe. Never run it unless the Makefile path is broken.
- **`sit/Makefile`** — new. Implements all six build targets for Windows (verified), with scaffold blocks for macOS and Linux (inert on Windows). Key design points:
  - `ROOT := ..` so all paths resolve from `sit/` via `../`
  - `BUILD_DIR` created at parse time via `$(shell ...)` — avoids recipe-shell portability issues
  - Vulkan SDK autodetected from `C:/VulkanSDK/*` when `VULKAN_SDK` is unset
  - `check-vulkan` split: `check-vulkan-sdk` (SDK + headers) required by both `vulkan` and `static-vulkan`; `check-vulkan-shaderc` (archive) required by `vulkan` DLL only — exact legacy parity
  - `STATIC_LDFLAGS_CXX` puts `-static-libstdc++` before `-Wl,-Bstatic` to match legacy link-line order
  - `EXTRA_VULKAN_CFLAGS` pass-through on Vulkan compile lines
  - `clean` mirrors legacy: removes `*.o` and `*.dll` only; `.def`/`.lib` intentionally preserved

---

---

---

---

---

---

## [v2.4.268] - 2026-06-14

### Description

**v2.4.268**: Makefile hardening pass. Three bugs found and fixed after initial deployment of the `sit/Makefile` build system (v2.4.267): a spurious sh syntax error on every invocation caused by CMD-syntax in a `$(shell ...)` call, silent no-output behaviour when a build target was already up to date, and Makefile file-encoding requirements documented and enforced.

### Changes

- **`sit/Makefile`** — three fixes applied:
  - `$(shell if not exist ... mkdir ...)` replaced with `$(shell mkdir -p ...)` for all platforms. The `if not exist` form is CMD syntax; GNU Make always invokes `$(shell ...)` through `sh.exe`, causing `/usr/bin/sh: -c: line 2: syntax error` on every invocation regardless of build success.
  - Per-target `_status` phony added to each build target (`opengl`, `vulkan`, `static-opengl`, `static-vulkan`). Always runs after the file target and prints the artifact path, source file, and whether it was just built or already up to date. Previously, an up-to-date target produced zero output.
  - File must be written as LF-only, 7-bit ASCII (no BOM, no CRLF, no non-ASCII in recipe-reachable lines). Em-dashes in comments replaced with `--`.

---

---

---

---

---

---

## [v2.4.269] - 2026-06-14

### Description

**v2.4.269**: Windows resource file added to DLL builds. `sit/situation_resource.rc` replaces the stale root-level `situation_resource.rc` and contains a proper `VS_VERSION_INFO` block. Version values (MAJOR, MINOR, PATCH, version string) are extracted from `situation_base_version.h` at Makefile parse time via `$(shell grep ...)` and injected into `windres` as `-D` flags. Two separate resource objects are compiled (`situation_resource_opengl.o` / `situation_resource_vulkan.o`) with per-backend `InternalName` and `OriginalFilename` values. Both are linked into their respective DLL targets. Static `.a` targets are unchanged. The old root-level `situation_resource.rc` (which contained an inert `101 RCDATA "situation.dll"` blob and was never compiled) has been deleted.

### Changes

- **`sit/situation_resource.rc`** — new. `VS_VERSION_INFO` block with `CompanyName`, `FileDescription`, `FileVersion` (string), `InternalName`, `LegalCopyright`, `OriginalFilename`, `ProductName`, `ProductVersion` (string), and numeric `FILEVERSION`/`PRODUCTVERSION` fields. All version values injected by the Makefile at `windres` compile time.
- **`sit/Makefile`** — three additions:
  - Section 2a: version extraction variables (`SIT_VERSION_MAJOR/MINOR/PATCH/STRING`) via `$(shell grep ...)`, Windows-only.
  - Section 6: two new file rules for `situation_resource_opengl.o` and `situation_resource_vulkan.o`, each depending on the RC file and `situation_base_version.h`.
  - OpenGL DLL and Vulkan DLL file targets: resource `.o` added as prerequisite and to the link line.
- **`situation_resource.rc`** (root) — deleted. Was inert and never wired into any build.
- **`doc/plan/MAKEFILE_BUILD_MIGRATION_PLAN.md`** — Phase 6 added and tasks 6.1–6.4, 6.6 marked complete. Task 6.5 (real-build verification) remains open pending next DLL rebuild.

---

---

---

---

---

---

## [v2.4.270] - 2026-06-14

### Description

**v2.4.270**: Makefile prerequisite improvements. `check-glfw` now auto-builds GLFW from source when `libglfw3.a` is missing instead of erroring out -- `cmake` and `mingw32-make` are already on PATH when the Makefile runs, so the first-time build is transparent. OpenGL and static-Vulkan targets are now fully zero-prereqs beyond the MinGW toolchain itself (plus cmake). `check-vulkan-shaderc` replaces its terse one-line error with a full self-contained build guide (exact commands, expected output path, and note that `static-vulkan` does not require shaderc). Compilation guide updated accordingly.

### Changes

- **`sit/Makefile`** -- two guard changes:
  - `check-glfw`: replaced `test -f ... || error` with an `if [ ! -f ] then cmake + mingw32-make fi` auto-build block, followed by a post-build existence check. Uses the same cmake flags as the existing GLFW build cache (`MinGW Makefiles`, `gcc`, docs/tests/examples OFF, `WIN32=ON`). Prints clear `[glfw]` prefixed progress lines.
  - `check-vulkan-shaderc`: replaced terse one-liner with a multi-line error block containing exact build commands, expected output path, and a reminder that `static-vulkan` does not require shaderc.
- **`doc/COMPILATION_GUIDE.md`** -- Prerequisites section updated: GLFW manual build step removed (now automatic); shaderc section updated to match the exact commands the Makefile now prints.

---

---

---

---

---

---

## [v2.4.271] - 2026-06-14

### Description

**v2.4.271**: `check-vulkan-sdk` is now self-documenting. The two terse one-liners are replaced with two distinct diagnostic blocks — one for "SDK not found at all" and one for "SDK found but invalid path". The "not found" block explains the autodetect mechanism (`C:/VulkanSDK/*/Include/vulkan/vulkan.h` scan), gives the download URL, current version, default install path, and notes that the installer sets `VULKAN_SDK` automatically. It also shows the manual override via `set VULKAN_SDK=`. The "invalid path" block identifies exactly which file is missing and offers reinstall or explicit path as remedies. Both blocks match the quality and format established by `check-vulkan-shaderc` and the GLFW failure block.

### Changes

- **`sit/Makefile`** -- `check-vulkan-sdk` rewritten from two `@test ... if [ $$? ]` one-liners to two `@if [ ... ]; then ... fi` blocks with full diagnostic output. No behavior change when the SDK is correctly installed.

---

---

---

---

---

---

## [v2.4.272] - 2026-06-14

### Description

**v2.4.272**: Library build reference extracted to its own document. `doc/BUILD_SITUATION_GUIDE.md` is a new dedicated reference covering the build system internals, all targets, GLFW auto-build behavior, Vulkan SDK setup, shaderc requirements, environment overrides, version stamping, and what the Makefile compiles vs. links as pre-built. `doc/COMPILATION_GUIDE.md` "Building the Library" section is replaced with a short summary + link to the new guide. This makes the compilation guide focused on application consumers while the build guide serves library contributors and integrators who need the full detail.

### Changes

- **`doc/BUILD_SITUATION_GUIDE.md`** -- new. Full reference for building the Situation library itself: build system architecture, all six targets, OpenGL prerequisites (GLFW auto-build), Vulkan prerequisites (SDK autodetect + shaderc manual build), environment overrides, Windows resource stamping, compiled-from-source vs. pre-built inputs table, fail-safe legacy script note.
- **`doc/COMPILATION_GUIDE.md`** -- "Building the Library" section replaced with a one-screen summary + link. ToC updated. Additional Resources table gains the new guide entry. Version bumped to 2.4.271.

---

---

---

---

---

---

## [v2.4.273] - 2026-06-14

### Description

**v2.4.273**: Makefile completeness pass. Four missing pieces addressed: `help` target (`.DEFAULT_GOAL` set so bare `make` no longer errors with "No rule to make target"), `all-static` aggregate target (builds both static archives in one shot, mirrors `all` for DLLs), `distclean` target (removes all build artifacts including `.a`, `.def`, `.lib` — fuller than `clean` which preserves those), and `build_situation.bat` updated to validate and forward `all-static` and `distclean` targets. Docs updated.

### Changes

- **`sit/Makefile`** -- four additions:
  - `.DEFAULT_GOAL := help` — bare `make` now prints usage instead of erroring
  - `help` phony target — prints all targets, environment overrides, and launcher invocation hint
  - `all-static` phony target — `static-opengl` + `static-vulkan` in one shot
  - `distclean` phony target — removes `*.o *.dll *.a *.def *.lib` from `build/dll/`
  - `.PHONY` list updated to include all four
- **`build/build_situation.bat`** -- `all-static` and `distclean` added to argument validation and usage text
- **`doc/BUILD_SITUATION_GUIDE.md`** -- targets table updated with new entries
- **`doc/COMPILATION_GUIDE.md`** -- targets summary table updated

---

---

---

---

---

---

## [v2.4.274] - 2026-06-14

### Description

**v2.4.274**: Icon resource embedded in DLL targets. `sit/situation_icon.ico` is a multi-resolution Windows icon (6 sizes: 16, 32, 48, 64, 128, 256) generated from `icon_source.PNG` (664x614 RGBA logo) via `scripts/gen_situation_icon.py` (Pillow). The icon appears in Explorer when browsing `build/dll/`, in the taskbar for windowed apps using the DLL, and in dependency viewers. `sit/situation_resource.rc` extended with `101 ICON "situation_icon.ico"`. Makefile `windres` rules updated with `--include-dir $(ROOT)/sit` so the icon path resolves correctly when the RC file is passed as an absolute path, and `situation_icon.ico` added as a Make dependency so icon changes trigger resource recompilation. Static `.a` targets unaffected.

### Changes

- **`sit/situation_icon.ico`** -- new. Multi-resolution icon, 6 sizes, 102KB.
- **`sit/situation_icon_src.png`** -- new. Clean RGBA version of the source logo (dark background removed).
- **`icon_source.PNG`** -- source asset at project root (664x614, RGBA).
- **`scripts/gen_situation_icon.py`** -- new. Generates `situation_icon.ico` from `icon_source.PNG` using Pillow. Background removal via corner color sampling + distance threshold.
- **`sit/situation_resource.rc`** -- added `101 ICON "situation_icon.ico"` above the `VS_VERSION_INFO` block.
- **`sit/Makefile`** -- both `windres` rules updated: `--include-dir $(ROOT)/sit` added (resolves icon path from absolute RC path), `$(ROOT)/sit/situation_icon.ico` added as prerequisite.

---

---

---

---

---

---

## [v2.4.275] - 2026-06-14

### Description

**v2.4.275**: Situation icon embedded in all example and test harness EXEs. `sit/sit_app.rc` is a minimal RC file containing `1 ICON "situation_icon.ico"` — the standard primary application icon slot that Windows uses for Explorer, taskbar, and Alt+Tab. Both `build_examples.bat` and `build_tests.bat` now compile this RC via `windres` into `sit_app_icon.o` right after creating the output directory, then pass it on every gcc/g++ link line. The windres step is non-fatal (icon is optional — a `[WARN]` is printed and the build continues if windres fails). Verified: `build_examples.bat static-opengl 01_open_a_window` produces an exe with the Situation hourglass icon visible in Explorer.

### Changes

- **`sit/sit_app.rc`** -- new. Minimal RC file: `1 ICON "situation_icon.ico"`. Used by all example and test EXEs. No version info -- that stays in `situation_resource.rc` for DLLs.
- **`build/build_examples.bat`** -- windres step added after `mkdir`, `%APP_ICON_OBJ%` added to all four gcc/g++ link lines (opengl DLL, vulkan DLL, static-opengl, static-vulkan).
- **`build/build_tests.bat`** -- same pattern: windres step added, `%APP_ICON_OBJ%` added to all four link lines (static-opengl, static-vulkan, opengl DLL, vulkan DLL).

---

---

---

---

---

---

## [v2.4.276] - 2026-06-14

### Description

**v2.4.276**: Icon source replaced. `icon_source.PNG` regenerated with a clean pure-black background, making the concave hourglass corner pockets unambiguously background-colored (RGB 0,0,0) and trivially reachable by the flood-fill. `scripts/gen_situation_icon.py` re-run: Pass 1 removed 229,497 outer background pixels, Pass 2 removed 418 enclosed pocket pixels (the 4 hourglass corners), Pass 3 softened 2,866 anti-aliased rim pixels. `sit/situation_icon.ico` and `sit/situation_icon_src.png` updated. Example exe rebuilt with clean icon.

### Changes

- **`icon_source.PNG`** -- replaced with clean black-background version.
- **`sit/situation_icon_src.png`** -- regenerated from new source.
- **`sit/situation_icon.ico`** -- regenerated, 106KB, all 6 sizes clean.

---

---

---

---

---

---

## [v2.4.277] - 2026-06-14

### Description

**v2.4.277**: Platform resource files reorganized into `sit/platform/windows/`. `situation_resource.rc`, `sit_app.rc`, and `situation_icon.ico` moved from `sit/` to `sit/platform/windows/`. Stub folders created for future ports: `sit/platform/linux/` and `sit/platform/macos/`. All 4 references updated: `sit/Makefile` (`RC_SRC`, `ICO_SRC`, `--include-dir`), `build/build_examples.bat` (windres path), `build/build_tests.bat` (windres path), `scripts/gen_situation_icon.py` (`OUT_ICO`). Build verified: `windres ../sit/platform/windows/situation_resource.rc --include-dir ../sit/platform/windows` resolves correctly.

### Changes

- **`sit/platform/windows/`** -- new. Contains `situation_resource.rc`, `sit_app.rc`, `situation_icon.ico`.
- **`sit/platform/linux/`** -- new empty stub for future Linux port resources.
- **`sit/platform/macos/`** -- new empty stub for future macOS port resources.
- **`sit/Makefile`** -- `RC_SRC` and `ICO_SRC` variables updated; `--include-dir` updated on both windres rules.
- **`build/build_examples.bat`** -- windres invocation updated to `sit\platform\windows\sit_app.rc --include-dir sit\platform\windows`.
- **`build/build_tests.bat`** -- same windres update.
- **`scripts/gen_situation_icon.py`** -- `OUT_ICO` updated to `sit/platform/windows/situation_icon.ico`.

---

---

---

---

---

---

## [v2.4.278] - 2026-06-16

### Description

**v2.4.278**: Phase 4 of the Vulkan SPIR-V user descriptor parity plan — `UBO_SSBO_SAMPLER` harness closure. Adds `test_spirv_memory_ubo_ssbo_sampler_readback` (pixel readback: R=77/SSBO, G=255/UBO, B=200/sampler — all correct). Adds the library guard that rejects `SituationCmdBindDescriptorSet` on set 2 under `UBO_SSBO_SAMPLER` (set 2 is always a sampler; callers must use `SituationCmdBindTextureSet`). Documents the deliberate `pushConstantRangeCount=0` on `UBO_SSBO` layout. Full graphics module: **94/94 Vulkan, 11/11 OpenGL SPIR-V** — 0 failures. `python scripts/spirv_desc_spike.py` now gates all three Vulkan harness FS shaders.

### Changes

- **`sit/situation_impl_renderer.h`**:
  - `_SituationVulkanResolveBufferDescriptor` `UBO_SSBO_SAMPLER` set 2 branch: now returns `SITUATION_ERROR_INVALID_PARAM` with explicit message (was silently setting sampler descriptor type on a buffer — misroute). Callers must use `SituationCmdBindTextureSet` for set 2.
  - `_SituationVulkanInitGraphicsSpirvLayouts`: added doc comment on `UBO_SSBO` `pushConstantRangeCount=0` (deliberate; use `UBO_SSBO_SAMPLER` if push constants needed).
- **`tests/harness/shaders/harness_ubo_ssbo_sampler_vk.fs`** -- new. Vulkan-target FS: set 0 UBO `Frame.color`, set 1 SSBO `TagBlock.tagB`, set 2 `sampler2D feedback`. Encodes SSBO→R, UBO→G, sampler.r→B.
- **`tests/harness/sit_harness_spirv_embed.h`** -- added `sit_harness_ubo_ssbo_sampler_fs_spv` / `_len` macro aliases and `extern` declarations (both backends; GL embed gets stub zero array from gen script).
- **`tests/harness/sit_harness_spirv_vk_embed.c`** -- regenerated by `compile_harness_shaders.bat`; includes real sampler FS SPIR-V blob.
- **`tests/harness/test_graphics_spirv.c`** -- added `spirv_harness_sampler_embed_ready()`, `spirv_harness_load_ubo_ssbo_sampler_shader()`, and `test_spirv_memory_ubo_ssbo_sampler_readback()` (Vulkan-gated). Test uses inline render pass to avoid stale-mesh issue from `post_link_resources`.
- **`tests/harness/test_graphics_spirv.h`** -- added forward declaration for `test_spirv_memory_ubo_ssbo_sampler_readback`.
- **`tests/harness/test_graphics.c`** -- registered `spirv_memory_ubo_ssbo_sampler_readback` in `SITUATION_USE_VULKAN` block (before `demon_hunt_sky_spirv_vk_begin_poll`).
- **`build/compile_harness_shaders.ps1`** -- added `harness_ubo_ssbo_sampler_vk.fs` compile; VK embed invocation passes `-FsSamplerSpv`.
- **`scripts/gen_spirv_embed.ps1`** -- added optional `-FsSamplerSpv` parameter; emits real blob when provided, stub zero array when absent (backward-compatible).
- **`scripts/spirv_desc_spike.py`** -- `EXPECTED_VARS` and `main()` loop extended to cover `harness_ubo_ssbo_sampler_vk.fs.spv` (expect `[(0,0),(1,0),(2,0)]`).
- **`doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`** -- Phase 4 checkboxes ticked; status updated to "Phases 0–4 complete"; verified snapshot row added.
- **`doc/plan/SHADER_DEBUG_PLAN.md`** -- §1 items all ticked; status updated.

---

---

---

---

---

---

## [v2.4.279] - 2026-06-16

### Description

**v2.4.279**: Vulkan shader cache Gate 1A — data structures only, no wire-up yet. Adds all Phase 1 types to `situation_impl_decl.h` and the hash/deref helpers to `situation_impl_renderer.h`. Both static-opengl and static-vulkan test harness builds verified clean.

### Changes

- **`sit/situation_impl_decl.h`**:
  - Added `SIT_VK_SHADER_CACHE_ENABLE` kill switch macro, `SIT_VK_SHADER_CACHE_MAX_ENTRIES` (256), `SIT_VK_SHADER_CACHE_EVICT_DELAY_FRAMES` (2).
  - Added `_SitVkShaderCacheKey` (three Phase 1 fields; struct sized for Phase 2 append).
  - Added `_SitVkBundleState` enum (`READY`, `EVICT_PENDING`, `STALE`, `DESTROYED`).
  - Added `_SitVkPipelineBundle` (minimal Phase 1: key, content_hash, generation, ref_count, last_used_frame, state, layout/owns_layout, vs/fs modules, default_pipeline).
  - Added `_SitVkPipelineBundleRef` (bundle pointer + generation for zero-lock draw path).
  - Added chained bucket entry types: `_SitVkSpirvBlobEntry` (Layer A), `_SitVkModulePairEntry` (Layer B), `_SitVkShaderCacheEntry` (Layer C).
  - Added `_SitVkShaderCache` struct (three `[256]` bucket arrays + `mtx_t mutex` + optional `stats` block in `!NDEBUG`).
  - Added `shader_cache` field to `_SituationVulkanState`.
  - Added `vk_bundle_ref` (`_SitVkPipelineBundleRef`) and `vk_bound_pipeline_cache` (`VkPipeline`) to `_SituationShaderSlot` under `SITUATION_USE_VULKAN`.
- **`sit/situation_impl_renderer.h`**:
  - Added `_SitVkHashBytes64` — FNV-1a 64-bit hash for SPIR-V bytes and GLSL source.
  - Added `_SitVkShadercOptionsFingerprint` — compile options fingerprint anchored to `_SituationVulkanCompileGLSLtoSPIRV` (~6714); must be updated if options change.
  - Added `_SitVkDerefBundle` — `static inline` safe deref; the only approved path to bundle-owned GPU state. Increments `stats.stale_derefs` in debug builds on generation mismatch.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.279.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: Gate 1A checkboxes ticked.

---

---

---

---

---

---

## [v2.4.280] - 2026-06-16

### Description

**v2.4.280**: Vulkan shader cache Gate 1B — full cache API. No load/unload wire-up yet; all cache machinery is in place and initialized at startup.

### Changes

- **`sit/situation_impl_renderer.h`**:
  - Added `_SitVkCacheBucket` / `_SitVkCacheKeyEqual` inline helpers.
  - Added `_SitVkShaderCacheInit` / `_SitVkShaderCacheShutdown` — mutex init/destroy + full GPU drain with optional debug stats banner at shutdown.
  - Added `_SitVkShaderCacheLookupOrInsertSpirv` (Layer A) — malloc copy of shaderc output; FNV-1a keyed; ref-counted chained bucket map.
  - Added `_SitVkShaderCacheAcquireModules` (Layer B) — `vkCreateShaderModule` ×2 outside the mutex; idempotent double-insert on racing sync loads.
  - Added `_SitVkShaderCacheAcquireBundle` (Layer C) — creates MESH-profile `VkPipelineLayout` + `_SitVkCreateDefaultSimplePipeline` on miss; ref-counted; updates `last_used_frame`.
  - Added `_SitVkShaderCacheReleaseBundle` — decrements ref; marks `EVICT_PENDING` when ref hits 0.
  - Added `_SitVkCreateDefaultSimplePipeline` — builds the Phase 1 default pipeline (simple stride, fill, no back-cull) directly from existing `VkShaderModule` handles to avoid duplicate module creation.
  - Added `_SitVkShaderCacheProcessEvictions` — post-graveyard LRU pass; queues evicted GPU objects through `_SituationDeferDestroyPipeline`.
  - Hooked `_SitVkShaderCacheProcessEvictions` into the per-frame graveyard flush path (after `_SituationFlushGraveyard`, guarded by `SIT_VK_SHADER_CACHE_ENABLE`).
  - Hooked `_SitVkShaderCacheInit` into `_SituationInitVulkan` (after screenshot mutex init).
  - Hooked `_SitVkShaderCacheShutdown` into `_SituationCleanupVulkan` (before graveyard drain loop).
- **`sit/situation_impl_renderer_fwd.h`**: added forward declarations for all 8 new cache functions.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.280.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: Gate 1B checkboxes ticked.

---

---

---

---

---

---

## [v2.4.281] - 2026-06-16

### Description

**v2.4.281**: Vulkan shader cache Gate 1C — load/unload/resolve wired. Cache is now live: repeat loads of the same shader source skip `_SituationVulkanBuildGraphicsPipelinesOnSlot` entirely. `--module graphics` baseline: **94/94 passed, ~19 s** (was ~30 s).

### Changes

- **`sit/situation_impl_renderer.h`**:
  - Added `_SitVkTryAttachBundle` — shared post-compile helper that runs Layer B module acquire + Layer C bundle acquire and attaches `bundle_ref` to the slot. MESH profile only; non-MESH paths leave `bundle_ref` zeroed and use legacy slot pipelines.
  - **`SituationLoadShaderFromMemory` (Vulkan/shaderc path)**: before `_SituationVulkanBuildGraphicsPipelinesOnSlot`, checks Layer C for a hit (inline fast path, avoids compile + 12 pipeline creates). On miss: runs full legacy build then calls `_SitVkTryAttachBundle` to insert the bundle.
  - **`_SituationPollVkAsyncShaderLoad`**: after successful `_SituationVulkanBuildGraphicsPipelinesOnSlot`, calls `_SitVkTryAttachBundle` so async loads also get a cached bundle for repeat loads.
  - **`SituationUnloadShader` (Vulkan)**: added cache bundle release branch before the per-slot `_SituationDeferDestroyPipeline` loop. `_SitVkShaderCacheReleaseBundle` decrements ref and marks `EVICT_PENDING`; the existing defer-destroy loop is safe with `VK_NULL_HANDLE` slot pipelines on repeat-load slots.
  - **`_SitVulkanResolveGraphicsPipeline`**: checks `_SitVkDerefBundle` first; returns `bundle->default_pipeline` when bundle is valid + simple stride (0 or `3*sizeof(float)`) + `POLYGON_MODE_FILL` + `CULL_MODE_NONE`. Also writes to `vk_bound_pipeline_cache` for hot-path draws. Falls through to existing raster variant resolver on non-matching conditions.
- **`sit/situation_impl_renderer_fwd.h`**: added forward declaration for `_SitVkTryAttachBundle`.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.281.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: Gate 1C checkboxes ticked; measured baseline noted.

---

---

---

---

---

---

## [v2.4.282] - 2026-06-16

### Description

**v2.4.282**: Shader cache review fixes (1B/1C blockers). Addresses all three "fix before merge" items from the Phase 1B review. 94/94 graphics tests green.

### Changes

- **`sit/situation_impl_renderer.h`**:
  - **Fix 1 — Wrong frame counter:** All three call sites (`_SituationFlushGraveyard` hook in EndFrame, `_SitVkTryAttachBundle`, and the inline Layer C hit path in `SituationLoadShaderFromMemory`) now use `sit_render.vk.current_frame_index` instead of `sit_render.current_frame_index` (the GL path). Eviction delay will now tick correctly on `sit_test_vulkan.exe`.
  - **Fix 2 — Shutdown layout leak:** `_SitVkShaderCacheShutdown` Layer C loop now calls `vkDestroyPipelineLayout` when `b->owns_layout && b->layout != VK_NULL_HANDLE`. Previous code left the layout comment noting "Layer B loop below" which was incorrect — Layer B only destroys modules.
  - **Fix 3 — Default pipeline state parity:** `_SitVkCreateDefaultSimplePipeline` rewritten to match `_SituationVulkanCreateGraphicsPipeline(flags=0u, topology=TRIANGLE_LIST)` exactly: `depthWriteEnable=VK_FALSE` (TRIANGLE_LIST + flags=0 branch), `blendEnable=VK_TRUE` with SRC_ALPHA/ONE_MINUS_SRC_ALPHA, and dynamic states via `_SitVulkanFillGraphicsDynamicStates` (viewport, scissor, line width + extended states when available). Previous implementation had depth write ON, blend OFF, and only viewport+scissor dynamic.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.282.

---

---

---

---

---

---

## [v2.4.283] - 2026-06-16

### Description

**v2.4.283**: Shader cache 1C second-pass — all six items from the re-review addressed. Layer A now gates shaderc; cache-hit slots carry the bundle layout; SPIRV path wired; G1 grep compliance restored. **94/94 graphics, ~17–19 s** (stable across multiple runs).

### Changes

- **`sit/situation_impl_renderer.h`**:
  - **Issue 1 — Layer A before shaderc:** `SituationLoadShaderFromMemory` now hashes VS+FS source strings before calling `_SituationVulkanCompileGLSLtoSPIRV`. On a Layer A hit it checks Layer C immediately; on a full hit (A+C) it skips shaderc and the 12-pipeline build entirely. The Layer C check on compiled SPIR-V (for A-miss / first-load paths) is retained as a second gate.
  - **Issue 2 — NULL layout on cache-hit slots:** Added `_SitVkAttachBundleRef` — the single approved write path for `slot->vk_bundle_ref.bundle`. In addition to setting `bundle_ref` and `generation`, it copies `bundle->layout` → `slot->vk_pipeline_layout` (with `vk_owns_pipeline_layout = false`) and sets `vk_spirv_layout_profile = MESH`. `SituationCmdBindPipeline`'s `current_pipeline_layout_for_push_constants` assignment is now correct for cache-hit-only slots.
  - **Issue 3 — SPIRV memory path:** `_SituationVulkanLoadShaderFromSpirvMemoryWithProfile` (called by both `SituationLoadShaderFromSpirvMemory` and `SituationLoadShaderFromSpirvMemoryEx`) now performs a Layer C pre-check before `_SituationVulkanBuildGraphicsPipelinesOnSlot`, and calls `_SitVkTryAttachBundle` on miss. Skips Layer A (no source to hash).
  - **Issue 4 — Async poll pre-check:** `_SituationPollVkAsyncShaderLoad` now checks Layer C before calling `_SituationVulkanBuildGraphicsPipelinesOnSlot`. On a hit, skips the 12-pipeline build and attaches bundle ref.
  - **Issue 5 — G1 grep compliance:** `_SitVkTryAttachBundle` refactored to call `_SitVkAttachBundleRef` instead of writing `vk_bundle_ref.bundle` directly. `SituationUnloadShader` now uses `_SitVkDerefBundle` for the primary release path (with a secondary raw-clear for stale/evicted refs). `grep 'vk_bundle_ref\.bundle\s*='` returns zero hits outside `_SitVkAttachBundleRef`.
  - **Issue 6 — Duplication eliminated:** Both inline hit paths in the sync load function now go through `_SitVkAttachBundleRef`; no `stats.hits++` duplication (each path has exactly one counter increment, inside the mutex).
- **`sit/situation_impl_renderer_fwd.h`**: added forward declaration for `_SitVkAttachBundleRef`.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.283.

---

---

---

---

---

---

## [v2.4.284] - 2026-06-16

### Description

**v2.4.284**: Vulkan shader cache Phase 1 complete — Gate 1D. Four cache tests added to the graphics harness; all pass on GTX 1070. Full `--module graphics` at **98/98, ~17–20 s** (was ~30 s). Phase 1 exit criteria met.

**Timings (GTX 1070, v2.4.284):**
- Baseline (pre-cache): `load_shader_from_memory` ~270 ms; full `--module graphics` ~30+ s
- Phase 1 repeat load: < 3 ms (Layer A+C hit — skips shaderc + 12 pipeline creates)
- Full `--module graphics`: ~17–20 s (stable across warm runs)

### Changes

- **`tests/harness/test_graphics.c`**:
  - Added `test_shader_cache_hit` — loads same GLSL twice; second load must be **< 3 ms** on Vulkan+shaderc (timing gate + `fprintf` trace). Passes on GL (no timing gate).
  - Added `test_shader_cache_reuse_after_unload` — load → draw red pixel → unload → reload → draw red pixel again. Verifies bundle reuse, layout propagation to `CmdBindPipeline`, and correct pixel output after cache-hit-only slot.
  - Added `test_shader_cache_concurrent_loads` — two `BeginLoad` of same source, poll both to SUCCESS. Phase 1: correctness only (may duplicate shaderc; dedup deferred to Phase 2 build tickets).
  - Added `test_shader_cache_no_memory_growth` — 50 load/unload cycles with frame pump per cycle; VRAM must not grow by more than 256 KiB after warm-up. Exercises full load → unload → eviction → graveyard loop.
  - Registered all four in `graphics_tests[]` after `sync_shader_after_async_cycle`.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: Phase 1 status → ✅ Complete (v2.4.284); Phase 1 dashboard row updated; 1D checkboxes ticked; `Phase 1 complete` checkbox ticked; plan status updated.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.284.

---

---

---

---

---

---

## [v2.4.285] - 2026-06-16

### Description

**v2.4.285**: Vulkan shader cache Phase 1 — final closure. Wires Layer A blob cache insert so compiled SPIR-V is persisted after shaderc, enabling the full "skip shaderc + skip 12 pipeline creates" fast path on repeat loads. All strict Phase 1 exit criteria now met.

**Measured timings (GTX 1070, v2.4.285):**
- First load `SituationLoadShaderFromMemory` (GLSL→shaderc→12 pipelines): ~620 ms cold
- Second load same source (Layer A+C hit — skip shaderc + pipelines): **< 1 ms**
- Full `--module graphics` (98 tests): ~17–20 s (was ~30+ s baseline)
- `shader_cache_hit`: passes `< 3 ms` gate ✅
- `shader_cache_reuse_after_unload`: pixel readback correct ✅
- `shader_cache_concurrent_loads`: both handles SUCCESS ✅
- `shader_cache_no_memory_growth`: VRAM stable after 50 cycles ✅

### Changes

- **`sit/situation_impl_renderer.h`**:
  - Added `_SitVkInsertLayerA` — inserts compiled SPIR-V blobs into `spirv_blob_cache[]` after shaderc succeeds. Main-thread only (never called from worker). Idempotent: if entry already exists (concurrent load), increments ref. Takes and releases `shader_cache.mutex` internally with no Vulkan calls under the lock.
  - **`SituationLoadShaderFromMemory` (Vulkan/shaderc path)**: captures `_layer_a_key` before the Layer A check, then calls `_SitVkInsertLayerA` immediately after both shaderc SPIR-V blobs are produced (before Layer C check). This closes the "Layer A read but never written" gap.
  - **`_SituationPollVkAsyncShaderLoad`**: calls `_SitVkInsertLayerA` on the main thread after `compile_done == 1` is confirmed, using source strings still alive on `ctx->vs_src` / `ctx->fs_src`. Worker contract preserved (no cache map access from worker threads).
- **`sit/situation_impl_renderer_fwd.h`**: added forward declaration for `_SitVkInsertLayerA`.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.285.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: status → "Phase 1 fully closed (v2.4.285)"; dashboard Phase 1 row updated; changelog entry added.

---

---

---

---

---

---

## [v2.4.286] - 2026-06-16

### Description

**v2.4.286**: Vulkan shader cache **Phase 2** — lazy pipeline variants, build-ticket dedup, and `VkPipelineCache`. First load of harness minimal shader uses bundle-only path (~1 pipeline + lazy variants on draw) instead of legacy 12-pipeline slot build. All four `shader_cache_*` filter tests green on GTX 1070; `compile_dedup_joins=1` on concurrent load test.

### Changes

- **`sit/situation_impl_decl.h`**: Phase 2 key fields (`render_pass_compatibility_id`, `dynamic_state_mask`, `caps_fingerprint`); bundle `variants[]`, `variant_ready_mask`, `pin_count`; `_SitVkShaderBuildTicket`; `VkPipelineCache` + compatibility epoch on vk state; `SIT_VK_SHADER_CACHE_PHASE2` gate.
- **`sit/situation_impl_renderer.h`**: `_SitVkFillCacheKey`, `_SitVkEnsurePipelineVariant`, `_SitVkPickVariantForDraw`, build-ticket acquire/release; bundle-only first load (sync + async + SPIR-V paths); hot-pin eviction; `VkPipelineCache` init/shutdown; resolve path uses lazy variants; `SituationPollShaderLoad` success when bundle attached; fixed orphaned `#if SITUATION_ENABLE_SHADER_COMPILER` before async typedef (pre-existing ifdef imbalance).
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: Phase 2 dashboard → ✅ v2.4.286.

---

---

---

---

---

---

## [v2.4.287] - 2026-06-16

### Description

**v2.4.287**: Phase 2 wiring fixes — render-pass epoch bump + stale bundle flush, and build-ticket dedup on sync `SituationLoadShaderFromMemory` (async path was already wired).

### Changes

- **`sit/situation_impl_renderer.h`**: `_SitVkShaderCacheOnMainRenderPassCreated` calls `_SitVkShaderCacheMarkBundlesStale` and bumps `render_pass_compatibility_id` on render-pass recreate; sync load acquires build ticket after Layer A miss and follower waits on Layer A (phase ≥ 2); leader sets ticket phases after Layer A insert and bundle attach.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: Phase 2B sync dedup + 2A stale hook documented.

---

---

---

---

---

---

## [v2.4.288] - 2026-06-16

### Description

**v2.4.288**: Vulkan shader cache Phase 3 — hot-reload in-place bundle swap. `SituationReloadShader` on the Vulkan+shaderc path now compiles GLSL to SPIR-V, computes `content_hash`, and:
- **No-op fast path:** if the new SPIR-V hashes to the same `content_hash` as the current bundle (touch, formatter, no real edit), returns immediately without touching any GPU objects.
- **In-place swap:** if SPIR-V changed, acquires a new bundle from the cache (reusing `VkShaderModule`s via Layer B if the binary was seen before), releases the old bundle ref, defers-destroys any slot-owned legacy pipelines, and calls `_SitVkAttachBundleRef` — the original `SituationShader` handle remains valid throughout.

Also adds `_SituationPerformHotReloadPass` — scans all active shader slots, detects mod-time changes, and calls `SituationReloadShader` with an appropriate error print on failure. Disk `VkPipelineCache` persist deferred per plan (optional Phase 3 add-on). 98/98 graphics tests green.

### Changes

- **`sit/situation_impl_renderer.h`**:
  - `SituationReloadShader` (Vulkan+cache path): full rewrite. Compiles to SPIR-V, checks `content_hash` vs live bundle — returns immediately on match. On change: `_SitVkInsertLayerA` → `_SitVkShaderCacheAcquireModules` → `_SitVkShaderCacheAcquireBundle` → release old bundle ref → defer-destroy slot-owned pipelines → `_SitVkAttachBundleRef`. GL / no-shaderc path unchanged (enclosed in `#else` block).
  - Added `_SituationPerformHotReloadPass` — file-watch scan over `sit_render.shader_registry[]`; builds a temporary handle from `slot_index + generation`; calls `SituationReloadShader`; logs failures to stderr.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.288.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: Phase 3 checkboxes ticked; dashboard row updated to ✅.

---

---

---

---

---

---

## [v2.4.289] - 2026-06-16

### Description

**v2.4.289**: Phase 3 **review fixes** — repairs merge-blocking compile errors from v2.4.288 and corrects hot-reload wiring. The in-place `SituationReloadShader` bundle-swap path from v2.4.288 is **unchanged**; this release mostly **reverts mistaken duplicate code** and integrates Phase 3 into the existing IO-thread hot-reload pass.

**What was wrong in v2.4.288 (fixed here):**
- A second `static void _SituationPerformHotReloadPass` was added alongside the existing `static SituationError _SituationPerformHotReloadPass` (IO thread, `situation_impl_io.h`) → **duplicate symbol / conflicting types**; build failed.
- `SituationReloadShader` had a broken `#if` / `#else` / `#endif` layout (orphan `{`, stray closing brace) → **syntax error**.
- `SituationGetErrorString` was referenced but does not exist in the API.
- `SITUATION_VERSION_DESCRIPTION` was defined twice in `situation_base_version.h` (second line silently overwrote the first).

**What we reverted / removed:**
- **Removed** the new void `_SituationPerformHotReloadPass` (~40 lines): duplicate of the v2.3.34 IO-thread scanner; not the function the thread pool actually calls.
- **Reverted** the Vulkan branch inside the existing IO pass that did `SituationLoadShaderFromMemory` + manual copy of 12 slot pipelines — replaced with a single `SituationReloadShader(&handle)` call so Phase 3 bundle swap and `content_hash` no-op apply on file-watch reload.
- **Did not ship** swapchain-recreate stale flush: trial bump in `_SituationVulkanRecreateSwapchain` broke `shader_cache_reuse_after_unload` (live slots still held refs to bundles marked `STALE` while the render pass was unchanged). Plan checkbox left unchecked with deferral note.

**What stayed from v2.4.288:**
- `SituationReloadShader` Vulkan+cache path: compile → `content_hash` compare → no-op or in-place `_SitVkAttachBundleRef` without invalidating the handle.

**Tests (GTX 1070, clean rebuild):** all four `shader_cache_*` filter tests green.

### Changes

- **`sit/situation_impl_renderer.h`**: fix `SituationReloadShader` `#if` structure; delete duplicate void hot-reload pass; IO-thread `_SituationPerformHotReloadPass` Vulkan path → `SituationReloadShader`.
- **`sit/situation_base_version.h`**: patch → 2.4.289.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: swapchain stale bump → deferred; dashboard Phase 3 row → v2.4.289.

---

---

---

---

---

---

## [v2.4.290] - 2026-06-16

### Description

**v2.4.290**: Shader cache Phase 3 fully closed. Adds `test_shader_cache_hot_reload` (V7 from the plan verification matrix): writes VS+FS to temp files, loads via `SituationLoadShader`, performs a no-op reload (identical GLSL → same SPIR-V → content_hash match, no GPU work, red pixel confirmed), then writes a green FS and reloads again (new SPIR-V → new bundle acquired in-place, green pixel confirmed, original handle slot still valid). Also fixes stray `SIT_ASSERT_MSG` call in `shader_cache_hit` test (macro does not exist; replaced with `SIT_ASSERT`). **99/99 graphics tests, 0 failures.**

### Changes

- **`tests/harness/test_graphics.c`**:
  - Added `test_shader_cache_hot_reload` — V7 plan test: no-op reload (content_hash unchanged, red pixel) + real-change reload (green FS, green pixel), both with stable handle.
  - Fixed `SIT_ASSERT_MSG` → `SIT_ASSERT` in `test_shader_cache_hit` (macro does not exist in harness).
  - Registered `shader_cache_hot_reload` in `graphics_tests[]`.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.290.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: V7 verification matrix row marked green.

---

---

---

---

---

---

## [v2.4.291] - 2026-06-16

### Description

**v2.4.291**: Vulkan shader cache plan **Phase 4** — OpenGL program cache. Repeat `SituationLoadShaderFromMemory` on OpenGL skips compile/link when the GLSL source hash matches a cached entry. Same chained-bucket + ref-count + 2-frame delayed eviction model as Vulkan Layer C (mirrors GL VAO cache shape).

### Changes

- **`sit/situation_impl_decl.h`**: `_SitGLProgramCache` / entry / ref types; `program_cache` on GL state; `gl_program_cache_ref` on shader slot; program defer queue in GL graveyard.
- **`sit/situation_impl_renderer.h`**: `_SitGLLayerAKeyFromSource`, acquire/release/evict, `_SitGLLoadShaderProgramCached`; PATH B load + `SituationUnloadShader`; `_SitGLDeferDestroyProgram`.
- **`tests/harness/test_graphics.c`**: `shader_cache_hit` < 3 ms gate on OpenGL when `SIT_GL_SHADER_CACHE_ENABLE`.
- **`doc/plan/VULKAN_SHADER_CACHE_PLAN.md`**: Phase 4 dashboard → v2.4.291.

---

---

---

---

---

---

## [v2.4.292] - 2026-06-17

### Description

**v2.4.292**: Shader cache code comment audit — three documentation fixes found during review.

1. **G1 invariant carve-out**: The `_SitVkAttachBundleRef` comment previously asserted "ZERO hits outside this function" for `vk_bundle_ref.bundle =` writes, which was incorrect — two intentional `NULL`-clear cleanup branches exist in `SituationUnloadShader` and `SituationReloadShader` (stale/evicted ref cleanup, no ref-count decrement). Comment now names the three legitimate sites and explains why the NULL-clears are not violations. Forward declaration comment in `situation_impl_renderer_fwd.h` updated to match.

2. **Hot-reload threading assumption**: The `SituationReloadShader` bundle-swap block now carries an explicit comment that `SituationReloadShader` must be called from the main thread between frames. The slot's `vk_pipeline_layout` briefly holds the old (released) bundle layout between `_SitVkShaderCacheReleaseBundle` and `_SitVkAttachBundleRef`; this is safe only because no command buffer is recording against the slot at that moment.

3. **Test generation comment**: `test_shader_cache_hot_reload` Round 2 comment corrected — the public `SituationShader.generation` is the slot registry generation and is *not* affected by `_SitVkAttachBundleRef` (which writes to the internal `vk_bundle_ref.generation`, not the public handle). The test correctly asserts only `slot_index` stability after a real-change reload.

No logic changes. No API changes.

### Changes

- **`sit/situation_impl_renderer.h`**: `_SitVkAttachBundleRef` comment block rewritten with exact carve-out; `SituationUnloadShader` stale-ref branch comment updated; `SituationReloadShader` bundle-swap comment expanded with threading assumption.
- **`sit/situation_impl_renderer_fwd.h`**: Forward declaration comment for `_SitVkAttachBundleRef` updated to match.
- **`tests/harness/test_graphics.c`**: Round 2 comment in `test_shader_cache_hot_reload` corrected.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.292.

---

---

---

---

---

---

## [v2.4.293] - 2026-06-17

### Description

**v2.4.293**: Renderer pipeline errno gap fixes — wire dedicated error codes at shader-cache, hot-reload, and render-pass failure sites per `RENDERER_ERRNO_GAPS_PLAN.md`.

1. **New codes `-758` / `-759`**: `SITUATION_ERROR_VULKAN_PIPELINE_CACHE_INIT_FAILED` and `SITUATION_ERROR_VULKAN_RENDER_PASS_CACHE_FULL` split off overloaded EOL/generic Vulkan init and render-pass codes.
2. **Hot-reload fallthrough**: `SituationReloadShader`, `SituationReloadTexture`, and `SituationReloadComputePipeline` now return `INTERNAL_STATE_CORRUPTED` when load/create succeeds but the new registry slot is inaccessible; failed loads propagate the underlying error instead of `GENERAL`.
3. **Shader cache diagnostics**: Vulkan bundle/module acquire paths and pipeline creation helpers set `VULKAN_PIPELINE_CREATION_FAILED` or `MEMORY_ALLOCATION` before returning NULL; GL/VK cache mutex init checks `mtx_init` return like the thread pool.

No API changes. Additive errno wiring only.

### Changes

- **`sit/situation_base_errno.h`**: add `-758`, `-759`.
- **`sit/situation_impl_renderer.h`**: GAP-1 through GAP-7 call-site fixes.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.293.
- **`doc/plan/ERRNO_ADOPTION_PLAN.md`**: cross-reference new Vulkan codes and hot-reload items.

---

---

---

---

---

---

## [v2.4.294] - 2026-06-17

### Description

**v2.4.294**: 10-bit color output plan **Phase 0** — API plumbing and honest capability reporting ahead of Vulkan swapchain / OpenGL framebuffer work (`doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`).

1. **`SituationOutputColorDepth` policy**: `SituationInitInfo.output_color_depth` (`AUTO` / `8BIT` / `10BIT`; zero-init → `AUTO`). Stored in `sit_gs.output_color_depth_policy` for Phase 1/3.
2. **Runtime caps**: `SituationGetGraphicsCaps()` now reports `output_bits_per_channel` (8 or 10) and `output_color_depth_active`. Defaults to 8-bit until a backend confirms 10-bit output.
3. **Honest `SIT_FEATURE_HDR_OUTPUT`**: removed unconditional OpenGL assignment at init; flag is set only when `_SituationSetOutputColorDepthState(true)` activates a 10-bit path (not yet wired — Phase 1/3).
4. **10-bit YPQ / RGB helpers**: `ColorRGBA10`, `SituationYpqToRgba10`, `SituationYpqToRgb10Packed` (A2R10G10B10 layout), `SituationRgbToYpqFrom10`, `SituationRgb10FromRgba`, `SituationRgbaFromRgb10`.

No swapchain or framebuffer format change yet — display output remains 8-bit until Phase 1 (Vulkan) and Phase 3 (OpenGL).

### Changes

- **`sit/situation_base_types.h`**: `ColorRGBA10`.
- **`sit/situation_api.h`**: `SituationOutputColorDepth`, init field, caps fields, public RGB10 API declarations.
- **`sit/situation_impl_decl.h`**: output depth state, `surface_supports_10bit_sdr` stub, `_SituationSetOutputColorDepthState()`.
- **`sit/situation_impl_ctrl.h`**: init policy copy/validate; caps query.
- **`sit/situation_impl_color.h`**: `_SitYpqUnitTo10Bit` and conversion helpers.
- **`sit/situation_impl_image.h`**: public RGB10 wrapper implementations.
- **`sit/situation_impl_renderer.h`**: gate `SIT_FEATURE_HDR_OUTPUT` on active 10-bit state.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.294.

---

---

---

---

---

---

## [v2.4.295] - 2026-06-17

### Description

**v2.4.295**: 10-bit color output plan **Phase 1** — Vulkan swapchain format selection and monitor-migration safety.

1. **`_SituationVulkanPickSurfaceFormat`**: when policy is `AUTO` or `10BIT` and WSI lists `A2R10G10B10_UNORM_PACK32` + `SRGB_NONLINEAR`, create a 10-bit SDR swapchain; otherwise fall back to existing 8-bit UNORM priority.
2. **`_SituationVulkanSurfaceSupports10BitSdr`**: probes WSI format list; result cached in `sit_render.vk.surface_supports_10bit_sdr`.
3. **Active state sync**: `_SituationSetOutputColorDepthState` runs at pick time (removed erroneous init-end reset on Vulkan).
4. **Swapchain recreate**: rebuilds main-window render pass when swapchain is recreated (format may change after surface re-probe / monitor move); resume framebuffers torn down in cleanup.
5. **Harness safety**: `sit_test_window_init_info` forces `SIT_OUTPUT_COLOR_8BIT` so CI pixel asserts stay on 8-bit until Phase 2 readback lands.

Readback/screenshot downconvert for 10-bit swapchain is still Phase 2.

### Changes

- **`sit/situation_impl_renderer.h`**: format pickers, surface 10-bit probe, render-pass recreate on swapchain rebuild, resume FB cleanup.
- **`tests/harness/sit_test_window.h`**: default `SIT_OUTPUT_COLOR_8BIT` for tests.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.295.

---

---

---

---

---

---

## [v2.4.296] - 2026-06-17

### Description

**v2.4.296**: 10-bit color output plan **Phase 2** — readback and screenshots downconvert 10-bit Vulkan swapchain texels to RGBA8.

1. **`_SituationVulkanCopyMappedColorToRGBA`**: handles `VK_FORMAT_A2R10G10B10_UNORM_PACK32` (10→8 bit quantize, A2→8 alpha); explicit `R8G8B8A8` / BGRA paths unchanged.
2. **`SituationRgbaFromRgb10Packed`**: public inverse of `SituationYpqToRgb10Packed` for packed texel → RGBA8.
3. **All Vulkan readback paths** wired through the converter: pre-present screenshot resolve, `SituationLoadImageFromScreen` fallback blit, `SituationReadFramebuffer`.
4. **Docs**: `SituationLoadImageFromScreen` notes v1 policy — public API always returns RGBA8.

OpenGL readback remains `GL_UNSIGNED_BYTE` (driver quantizes); full 10-bit GL FB path is Phase 3.

### Changes

- **`sit/situation_impl_color.h`**: `_SitUnpackA2R10G10B10ToRgba8`, `_SitRgbaFromRgb10Packed`.
- **`sit/situation_impl_renderer.h`**: extended copy helper; `SituationReadFramebuffer` row conversion.
- **`sit/situation_impl_image.h`**: `SituationRgbaFromRgb10Packed`; screen capture docs.
- **`sit/situation_api.h`**: API declaration.
- **`tests/harness/test_core.c`**: `rgb10_packed_readback` math test; caps assert 8-bit harness default.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.296.

---

---

---

---

---

---

## [v2.4.297] - 2026-06-17

### Description

**v2.4.297**: 10-bit color output plan **Phase 3** — OpenGL best-effort 10-bit default framebuffer.

1. **`_SituationInitWindow`**: when `output_color_depth` is AUTO or 10BIT, sets `GLFW_RED/GREEN/BLUE_BITS` to 10 before `glfwCreateWindow`.
2. **`_SituationOpenGLSetOutputColorDepthFromFramebuffer`**: after GLAD load, queries actual FB bit depth via `glfwGetWindowAttrib(GLFW_*_BITS)`; activates 10-bit state (and `SIT_FEATURE_HDR_OUTPUT`) only when all RGB channels report ≥ 10 bits.
3. **Fail-soft**: logs when `SIT_OUTPUT_COLOR_10BIT` was requested but the default FB stays 8-bit; no error returned to the app.
4. **Loader window**: resets color hints to 8-bit so the hidden async-load context does not inherit 10-bit hints.
5. **Shared policy helper**: `_SituationWants10BitOutput` in `situation_impl_decl.h` (Vulkan picker uses the same logic).

### Changes

- **`sit/situation_impl_ctrl.h`**: GLFW 10-bit window hints.
- **`sit/situation_impl_renderer.h`**: GL framebuffer probe; loader hint reset; dedupe Vulkan wants-10-bit helper.
- **`sit/situation_impl_decl.h`**: `_SituationWants10BitOutput`.
- **`sit/situation_api.h`**: docs — OpenGL is best-effort; Vulkan preferred for reliable 10-bit on Windows.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.297.

---

---

---

---

---

---

## [v2.4.298] - 2026-06-17

### Description

**v2.4.298**: 10-bit color output plan **Phase 4** — harness validation and opt-in hardware probes.

1. **CI default unchanged**: `sit_test_window_init_info` keeps `SIT_OUTPUT_COLOR_8BIT`; `output_color_depth_ci_default` and extended `get_graphics_caps` assert 8-bit + HDR flag parity.
2. **CPU math**: `ypq_to_rgba10_roundtrip` complements `rgb10_packed_readback`.
3. **New module `output_color_depth`**: `monitor_hot_swap_recreate` (skip when < 2 monitors); opt-in tests when `SIT_TEST_10BIT=1` (`opt_in_10bit_caps_and_hdr`, `opt_in_10bit_screenshot_readback`).
4. **Shared helper**: `sit_test_assert_output_color_depth_consistent()` in `sit_test_window.h`.

### Changes

- **`tests/harness/sit_test_window.h`**: `sit_test_10bit_enabled`, `sit_test_window_init_info_10bit`, consistency assert.
- **`tests/harness/test_core.c`**, **`test_graphics.c`**, **`test_output_color_depth.c`** (new).
- **`tests/harness/sit_test_registry.c`**, **`build/build_tests.bat`**: register and compile new module.
- **`doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`**: Phase 4 checklist + manual gradient procedure.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.298.

---

---

---

---

---

---

## [v2.4.299] - 2026-06-17

### Description

**v2.4.299**: 10-bit color output plan **Phase 5** — DXGI HDR / 10-bit display detection (Windows).

1. **`SituationDisplayInfo`**: `hdr_supported`, `hdr_enabled`, `bits_per_color`, `max_luminance_nits`, `dxgi_color_space`, `dxgi_metadata_valid` from `IDXGIOutput6::GetDesc1`.
2. **`_SituationDxgiFillDisplayHdrMetadata`**: Win32-only COM probe during `EnumDisplayMonitors`; maps output to cached display entry.
3. **`SituationGraphicsCaps.wsi_supports_10bit_sdr`**: Vulkan WSI 10-bit SDR probe surfaced in caps (from Phase 1 cache).
4. **Harness**: new always-on test `report_hdr_10bit_display_capability` in `output_color_depth` module.

Note: probe initially returned `metadata unavailable` until IID fix in v2.4.300.

### Changes

- **`sit/situation_api.h`**: extended `SituationDisplayInfo` and `SituationGraphicsCaps`.
- **`sit/situation_impl_wdm.h`**: DXGI HDR metadata fill (initial implementation).
- **`sit/situation_impl_ctrl.h`**: fill `wsi_supports_10bit_sdr` in caps.
- **`tests/harness/test_output_color_depth.c`**: `report_hdr_10bit_display_capability`.
- **`doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`**: Phase 5 tasks + partial status.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.299.

---

---

---

---

---

---

## [v2.4.300] - 2026-06-17

### Description

**v2.4.300**: 10-bit color output plan **Phase 5** (fix) — DXGI `GetDesc1` probe now returns real per-monitor metadata.

1. **Root cause**: `IDXGIOutput6` QueryInterface used wrong IID (`a068345e-…`); fixed to official `068346e8-…` so `GetDesc1` succeeds on Win10+.
2. **Matching**: HMONITOR from `EnumDisplayMonitors` + `\\.\DISPLAYn` name fallback (more reliable than UTF-8 string compare alone).
3. **COM guard removed**: DXGI factory probe no longer bails when `sit_gs.is_com_initialized` is false (e.g. `RPC_E_CHANGED_MODE`).
4. **Hot-plug**: `glfwSetMonitorCallback` refreshes display/HDR cache on monitor connect/disconnect.
5. **Harness**: `report_hdr_10bit_display_capability` summary counts (`dxgi_ok`, `hdr_enabled`, etc.).

### Changes

- **`sit/situation_impl_wdm.h`**: corrected `SIT_IID_IDXGIOutput6`, HMONITOR probe path, monitor callback.
- **`sit/situation_impl_forward.h`**, **`sit/situation_impl_ctrl.h`**: register monitor callback.
- **`tests/harness/test_output_color_depth.c`**: capability report test + summary line.
- **`doc/plan/10BIT_COLOR_OUTPUT_PLAN.md`**: Phase 5 marked done.
- **`sit/situation_base_version.h`**: bumped patch to 2.4.300.

---

---

---

---
