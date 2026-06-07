# Errno Adoption Plan

## Goal

Wire up the 140 never-produced error codes to their proper call sites, eliminate
inappropriate uses of `SITUATION_ERROR_GENERAL`, fix phantom references, and clean
up EOL entries. End state: every non-reserved error code in `situation_base_errno.h`
is produced by at least one code path in the library.

**Source report:** `doc/ERRNO_USAGE_REPORT.md`
**Audit script:** `scripts/audit_errno_report.ps1`

---

## Phase 0 — Phantom Fixes (Pre-requisite)

Fix error names referenced in code that don't exist in the X-macro table.
These are either typos, prefix-match regex artifacts, or missing table entries.

- [x] Investigate `SITUATION_ERROR_IO` — find usage, add to table or rename to `SITUATION_ERROR_FILE_*`
  - Fixed: replaced with `SITUATION_ERROR_FILE_OPEN_FAILED` in `situation_impl_io.h` (Linux /proc path)
- [x] Investigate `SITUATION_ERROR_DISPLAY_MODE_` — regex artifact from EOL comment; fixed scanner
- [x] Investigate `SITUATION_ERROR_FILE_` — regex artifact from EOL comment; fixed scanner
- [x] Investigate `SITUATION_ERROR_VIRTUAL_DISPLAY_` — regex artifact from `_xxx` in doc-comment; fixed scanner
- [x] Investigate `SITUATION_ERROR_VULKAN_` — regex artifact from EOL `*_FAILED` comment; fixed scanner
- [x] Re-run audit script, confirm 0 phantoms

---

## Phase 1 — Platform, Windowing & Input (16 errors)

Low-risk, self-contained. These live in `situation_impl_wdm.h` and `situation_impl_input.h`.

### Windowing (10)

- [x] `SITUATION_ERROR_CLIPBOARD_FAILED` — wired into `SituationSetClipboardText`, `SituationGetClipboardText` (glfwGetError check)
- [x] `SITUATION_ERROR_CURSOR_CREATION_FAILED` — wired into cursor init in `_SituationInitPlatform` (NULL check after glfwCreateStandardCursor)
- [x] `SITUATION_ERROR_WINDOW_STATE_FAILED` — wired into `SituationApplyCurrentProfileWindowState` (glfwGetError after all state changes)
- [x] `SITUATION_ERROR_WINDOW_PROPERTY_FAILED` — wired into title/size/position/opacity setters (glfwGetError after each)
- [x] `SITUATION_ERROR_WINDOW_FOCUS_FAILED` — wired into `SituationSetWindowFocused` (glfwGetError check)
- [ ] `SITUATION_ERROR_APP_STATE_FAILED` — deferred: no failure path in current void PauseApp/ResumeApp/SetTargetFPS
- [x] `SITUATION_ERROR_COM_INITIALIZATION_FAILED` [EOL] — verified dead; COM init already uses `COM_FAILED` (-123)
- [x] `SITUATION_ERROR_DXGI_QUERY_FAILED` [EOL] — verified dead; DXGI code now uses `DXGI_FAILED` (-124)
- [x] `SITUATION_ERROR_DXGI_FAILED` — wired into `SituationGetDeviceInfo` DXGI paths (factory/adapter/desc failures)
- [x] `SITUATION_ERROR_WINDOW_FOCUS` [EOL] — verified dead; no code produces it

### Input & HID (3)

- [x] `SITUATION_ERROR_INPUT_DEVICE_DISCONNECTED` — wired into joystick disconnect processing + vibration guard
- [x] `SITUATION_ERROR_INPUT_MAPPING_INVALID` — wired into `SituationSetGamepadMappings` (GLFW rejection)
- [x] `SITUATION_ERROR_INPUT_HAPTIC_FAILED` — wired into `SituationSetGamepadVibration` (XInputSetState failure)

### Display (4)

- [x] `SITUATION_ERROR_DISPLAY_MODE_SET_FAILED` — wired into `SituationSetDisplayMode` (replaced EOL `DISPLAY_SET`)
- [x] `SITUATION_ERROR_DISPLAY_MODE_UNSUPPORTED` — wired into `SituationSetDisplayMode` (DISP_CHANGE_BADMODE case)
- [x] `SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT_REACHED` — wired into `SituationCreateVirtualDisplay` (replaced EOL `VIRTUAL_DISPLAY_LIMIT`)
- [x] `SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND` — wired into `SituationDestroyVirtualDisplay` (split from INVALID_ID: range-valid but inactive slot)

---

## Phase 2 — Filesystem & Hot-Reload (11 errors)

Lives in `situation_impl_io.h`. Many of these replace overly-generic `SITUATION_ERROR_FILE_ACCESS`.

- [x] `SITUATION_ERROR_FILE_ACCESS_DENIED` — wired via `_SituationSetFilesystemError` (ERROR_ACCESS_DENIED/EACCES mapping, upgraded from EOL PERMISSION_DENIED)
- [x] `SITUATION_ERROR_FILE_ALREADY_EXISTS` — wired via `_SituationSetFilesystemError` (ERROR_ALREADY_EXISTS/EEXIST mapping)
- [x] `SITUATION_ERROR_FILE_LOCKED` — wired via `_SituationSetFilesystemError` (ERROR_SHARING_VIOLATION/EBUSY mapping)
- [x] `SITUATION_ERROR_DISK_FULL` — wired via `_SituationSetFilesystemError` (ERROR_HANDLE_DISK_FULL/ENOSPC mapping)
- [x] `SITUATION_ERROR_PATH_IS_DIRECTORY` — wired via `_SituationSetFilesystemError` (ERROR_DIRECTORY/EISDIR mapping)
- [x] `SITUATION_ERROR_PATH_IS_FILE` — wired via `_SituationSetFilesystemError` (ENOTDIR mapping)
- [x] `SITUATION_ERROR_PATH_INVALID` — wired via `_SituationSetFilesystemError` (ERROR_INVALID_NAME/ERROR_BAD_PATHNAME mapping, newly added)
- [ ] `SITUATION_ERROR_FILE_MODIFIED` — deferred: aspirational for race-condition detection during file ops
- [ ] `SITUATION_ERROR_HOTRELOAD_WATCHER_FAILED` — reserved: no push-based watcher (uses stat() polling)
- [ ] `SITUATION_ERROR_HOTRELOAD_FILE_CHANGED_TOO_FAST` — reserved: no debounce system yet
- [ ] `SITUATION_ERROR_HOTRELOAD_GPU_SYNC_FAILED` — reserved: graveyard system replaced inline vkDeviceWaitIdle
- [x] `SITUATION_ERROR_PERMISSION_DENIED` [EOL] — redirected: `_SituationSetFilesystemError` now produces `FILE_ACCESS_DENIED` instead

---

## Phase 3 — Rendering Core (6 errors)

Lives in `situation_impl_renderer.h`. Focus on command buffer and render pass guard rails.

- [x] `SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER` — wired into 4 shader uniform setters (replaced INVALID_PARAM for !in_frame guard)
- [x] `SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE` — wired into `SituationCmdBeginRenderPass` (nested pass guard, both GL and VK paths)
- [x] `SITUATION_ERROR_NO_RENDER_PASS_ACTIVE` — wired into GL path of `SituationCmdEndRenderPass` (VK path already had it)
- [ ] `SITUATION_ERROR_RENDER_PASS_ACTIVE` — deferred: needs identification of operations illegal during active pass
- [ ] `SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED` — deferred: current design returns NULL silently for stale handles (idempotent pattern)
- [ ] `SITUATION_ERROR_BACKEND_MISMATCH` — deferred: backend-specific APIs are behind #ifdef, runtime mismatch is exotic
- [ ] `SITUATION_ERROR_BACKEND_SPECIFIC` — deferred: aspirational for detailed backend-specific failures

---

## Phase 4 — OpenGL Backend (7 errors)

Lives in `situation_impl_renderer.h` (GL sections). SPIR-V specialization paths.

- [x] `SITUATION_ERROR_OPENGL_CONTEXT_CREATION_FAILED` — verified: produced during GL init failure path
- [x] `SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED` — verified: produced in text shader init + async link path
- [x] `SITUATION_ERROR_OPENGL_PROGRAM_VALIDATION_FAILED` — verified: produced in glValidateProgram check path
- [x] `SITUATION_ERROR_OPENGL_SPIRV_VS_SPECIALIZE_FAILED` — verified: produced in `_SituationSetGLErrorFromSpirvStage` + async specialization
- [x] `SITUATION_ERROR_OPENGL_SPIRV_FS_SPECIALIZE_FAILED` — verified: produced in `_SituationSetGLErrorFromSpirvStage` + async specialization
- [x] `SITUATION_ERROR_OPENGL_SPIRV_CS_SPECIALIZE_FAILED` — verified: produced in `_SituationCreateGLComputeProgramFromSpirv`
- [x] `SITUATION_ERROR_OPENGL_SPIRV_PROGRAM_LINK_FAILED` — verified: produced in SPIR-V program link + async link path

**Note:** All 7 were already wired in the codebase. The audit script's line-by-line strict mode couldn't
detect them because error codes are passed as function arguments on continuation lines. Manual verification
confirmed all 7 are actively produced through `_SituationSetGLErrorFromSpirvStage` and ternary expressions.

---

## Phase 5 — Vulkan Backend (8 errors)

Lives in `situation_impl_renderer.h` (VK sections).

- [x] `SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED` — wired into `_SituationVulkanCreateInstance` + debug messenger (replaced EOL `VULKAN_INSTANCE_FAILED`)
- [x] `SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE` — wired into `_SituationVulkanPickPhysicalDevice` + bindless check (replaced EOL `VULKAN_DEVICE_FAILED`)
- [x] `SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED` — wired into `_SituationVulkanCreateLogicalDevice` (replaced EOL `VULKAN_DEVICE_FAILED`)
- [x] `SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED` — verified: already produced in `SituationAcquireFrameCommandBuffer` cmd buffer paths
- [ ] `SITUATION_ERROR_VULKAN_PIPELINE_FAILED` [EOL] — redirect callers to `VULKAN_PIPELINE_CREATION_FAILED`
- [x] `SITUATION_ERROR_VULKAN_SPIRV_VS_MODULE_FAILED` — verified: produced via `_SituationVulkanCreateShaderModuleEx`
- [x] `SITUATION_ERROR_VULKAN_SPIRV_FS_MODULE_FAILED` — verified: produced via `_SituationVulkanCreateShaderModuleEx`
- [x] `SITUATION_ERROR_VULKAN_SPIRV_CS_MODULE_FAILED` — verified: produced via compute pipeline SPIR-V path

---

## Phase 6 — Compute (3 errors)

Lives in `situation_impl_renderer.h` (compute dispatch section).

- [x] `SITUATION_ERROR_COMPUTE_PIPELINE_CREATION_FAILED` — verified: VK path uses `VULKAN_PIPELINE_CREATION_FAILED` (-747) at `vkCreateComputePipelines`, GL path uses `OPENGL_SHADER_COMPILE_FAILED` — both are more specific. Generic code reserved for future backend-agnostic wrapper.
- [ ] `SITUATION_ERROR_COMPUTE_DISPATCH_FAILED` — deferred: dispatch just records to cmd buffer; failure is deferred to GPU submission
- [ ] `SITUATION_ERROR_COMPUTE_BUFFER_BINDING_MISSING` — deferred: no bind-state tracking at dispatch time yet

---

## Phase 7 — Fonts & Image (4 errors)

Lives in `situation_impl_image.h`.

- [x] `SITUATION_ERROR_FONT_LOAD_FAILED` — wired into `SituationLoadFont` + `SituationLoadFontFromMemory` (parse failure) + `SituationBakeFontAtlas` (texture creation failure)
- [ ] `SITUATION_ERROR_FONT_GLYPH_MISSING` — deferred: STB truetype silently returns empty quad for missing glyphs; no explicit lookup API exists
- [x] `SITUATION_ERROR_FONT_ATLAS_FULL` — wired into `SituationBakeFontAtlas` (stbtt_BakeFontBitmap returns ≤ 0)
- [x] `SITUATION_ERROR_IMAGE_OPERATION_FAILED` — wired into `SituationImageResize` (stbir failure path)

---

## Phase 8 — Audio Subsystem (5 errors)

Lives in `situation_impl_audio.h` and `sit/aud/`.

- [x] `SITUATION_ERROR_AUDIO_CAPTURE_NOT_AVAILABLE` — wired into `SituationStartAudioCaptureEx` (MA_NO_DEVICE check before generic device-init-failed)
- [x] `SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED` — wired into `SituationLoadSoundFromFile` (preload + stream paths) and `SituationLoadSoundFromStream` (replaces generic AUDIO_DECODING)
- [x] `SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED` — wired into same sites (MA_NO_BACKEND / MA_FORMAT_NOT_SUPPORTED differentiation)
- [x] `SITUATION_ERROR_AUDIO_STREAM_ENDED` — wired into `_SituationMixLoadedVoicesFromSnapshot` (sets atomic `last_status` on non-looping stream EOF; main-thread pollable)
- [ ] `SITUATION_ERROR_AUDIO_CONVERTER` — deferred: `ma_data_converter` field exists but is not initialized/used yet; no failure path to wire

---

## Phase 9 — Audio Node Graph (11 errors)

Lives in `sit/aud/` node graph implementation.

- [x] `SITUATION_ERROR_NODE_GRAPH_NOT_INITIALIZED` — wired as NULL graph guard in `SituationCreateNode`, `SituationCreatePatch`, `SituationRemovePatch`, `SituationDestroyNode`, `SituationSetControl`, `SituationGetControl`, `SituationTopologicalSort`, `SituationProcessGraph`
- [ ] `SITUATION_ERROR_NODE_ALREADY_EXISTS` — deferred: no name/ID deduplication system exists; nodes are handle-based with free-slot allocation
- [x] `SITUATION_ERROR_NODE_NOT_FOUND` — wired into `SituationDestroyNode`, `SituationCreatePatch`, `SituationSetControl`, `SituationGetControl` (replaces generic INVALID_HANDLE where node lookup fails)
- [x] `SITUATION_ERROR_NODE_CHANNEL_MISMATCH` — wired into `SituationCreatePatch` (audio patch port channel count check)
- [ ] `SITUATION_ERROR_NODE_PORT_TYPE_MISMATCH` — deferred: structurally prevented by `is_control` param design; audio/control ports use separate arrays
- [x] `SITUATION_ERROR_NODE_PATCH_NOT_FOUND` — wired into `SituationRemovePatch` (replaces generic PORT_INVALID) + implemented `SituationDestroyPatch` as legacy wrapper
- [x] `SITUATION_ERROR_NODE_CONTROL_OUT_OF_RANGE` — wired into `SituationSetControl` (returns error instead of silent clamp)
- [ ] `SITUATION_ERROR_NODE_CONTROL_TYPE_MISMATCH` — deferred: control API is float-only; type enforcement is per-device semantic, not API-level
- [x] `SITUATION_ERROR_NODE_PROCESSING_FAILED` — wired into `SituationProcessGraph` (NULL output_buffer / device_funcs)
- [x] `SITUATION_ERROR_NODE_DESERIALIZATION_FAILED` — wired into `SituationDeserializeGraphFromJSON` (all parse failures) and `SituationLoadGraphFromFile` (file read failures)
- [ ] `SITUATION_ERROR_NODE_TOPOLOGY_INVALID` — deferred: only cycle detection exists (`NODE_PATCH_CYCLE_DETECTED`); no disconnected-output or structural validation yet

---

## Phase 10 — Audio Device Registry (10 errors)

Lives in `sit/aud/` device registry implementation.

- [x] `SITUATION_ERROR_DEVICE_REGISTRY_NOT_INITIALIZED` — wired as guard in `SituationGetDeviceMetadata`, `SituationGetDeviceMetadataByIndex`, `SituationIsDeviceRegistered`
- [x] `SITUATION_ERROR_DEVICE_TYPE_INVALID` — wired into `SituationRegisterDeviceType` (NULL meta check)
- [x] `SITUATION_ERROR_DEVICE_CATEGORY_INVALID` — wired into `SituationValidateDeviceMetadata` (enum range check)
- [x] `SITUATION_ERROR_DEVICE_CONTROL_INVALID` — wired into `SituationValidateDeviceMetadata` (empty name, invalid min/max/default)
- [x] `SITUATION_ERROR_DEVICE_PORT_INVALID` — wired into `SituationValidateDeviceMetadata` (audio_channels > 8)
- [ ] `SITUATION_ERROR_DEVICE_FUNCTION_TABLE_INVALID` — deferred: function table lookup is optional by design (devices may lack create/process)
- [x] `SITUATION_ERROR_DEVICE_QUERY_FAILED` — wired into `SituationGetDeviceMetadataByIndex` (out-of-range index)
- [x] `SITUATION_ERROR_DEVICE_CREATE_FAILED` — wired into `SituationCreateNodeWithDevice` (funcs->create returns NULL)
- [ ] `SITUATION_ERROR_DEVICE_DESTROY_FAILED` — deferred: destroy is void return, no failure path
- [ ] `SITUATION_ERROR_DEVICE_PROCESS_FAILED` — deferred: process is void return, no failure path

---

## Phase 11 — Threading (15 errors)

Lives in `situation_impl_threading*.h`. Many are defensive guards that may rarely fire.

- [ ] `SITUATION_ERROR_THREAD_JOIN_FAILED` — wire into `thrd_join` failure
- [ ] `SITUATION_ERROR_THREAD_DETACH_FAILED` — wire into `thrd_detach` failure
- [ ] `SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED` — wire into `mtx_lock` failure
- [ ] `SITUATION_ERROR_THREAD_MUTEX_UNLOCK_FAILED` — wire into `mtx_unlock` failure
- [ ] `SITUATION_ERROR_THREAD_MUTEX_TIMEOUT` — wire into timed lock (deadlock prevention)
- [ ] `SITUATION_ERROR_THREAD_ATOMIC_FAILED` — wire into atomic op failure (platform-specific)
- [ ] `SITUATION_ERROR_THREAD_STATE_INVALID` — wire into invalid state transitions
- [ ] `SITUATION_ERROR_THREAD_BUFFER_OVERFLOW` — wire into TLS buffer overflow detection
- [ ] `SITUATION_ERROR_THREAD_DEADLOCK_DETECTED` — wire into deadlock detector
- [ ] `SITUATION_ERROR_THREAD_NOT_AVAILABLE` — wire into threading disabled / platform unsupported guard
- [ ] `SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT` — wire into render thread join timeout
- [ ] `SITUATION_ERROR_RENDER_LIST_INCOMPLETE` — wire into render list validation
- [ ] `SITUATION_ERROR_ARM_INTRINSICS_FAILED` — reserve (ARM platform, not yet shipping)
- [ ] `SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION` — wire into architectural rule enforcement
- [ ] `SITUATION_ERROR_ASSERTION_FAILED` — wire into `SIT_ASSERT` macro (if not already)

---

## Phase 12 — EOL Cleanup Pass

Verify EOL-tagged codes: either remove if truly dead, or redirect remaining callers.

- [ ] `SITUATION_ERROR_WINDOW_FOCUS` (-120) — verify 0 producers, leave as EOL alias
- [ ] `SITUATION_ERROR_COM_INITIALIZATION_FAILED` (-110) — redirect to `COM_FAILED` (-123)
- [ ] `SITUATION_ERROR_DXGI_QUERY_FAILED` (-111) — redirect to `DXGI_FAILED` (-124)
- [ ] `SITUATION_ERROR_DISPLAY_QUERY` (-200) — redirect to `DISPLAY_QUERY_FAILED` (-210)
- [ ] `SITUATION_ERROR_DISPLAY_SET` (-201) — redirect to `DISPLAY_MODE_SET_FAILED` (-212)
- [ ] `SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT` (-202) — redirect to `VIRTUAL_DISPLAY_LIMIT_REACHED` (-213)
- [ ] `SITUATION_ERROR_FILE_ACCESS` (-300) — redirect to specific `FILE_*` codes
- [ ] `SITUATION_ERROR_PERMISSION_DENIED` (-303) — redirect to `FILE_ACCESS_DENIED` (-311)
- [ ] `SITUATION_ERROR_AUDIO_CONTEXT` (-400) — redirect to `AUDIO_BACKEND_INIT_FAILED` (-410)
- [ ] `SITUATION_ERROR_AUDIO_DEVICE` (-401) — redirect to `AUDIO_DEVICE_INIT_FAILED` (-411)
- [ ] `SITUATION_ERROR_AUDIO_SOUND_LIMIT` (-402) — redirect to `AUDIO_SOUND_LIMIT_REACHED` (-420)
- [ ] `SITUATION_ERROR_RESOURCE_INVALID` (-500) — redirect to `INVALID_RESOURCE_HANDLE` (-510)
- [ ] `SITUATION_ERROR_OPENGL_SHADER_COMPILE` (-610) — redirect to `OPENGL_SHADER_COMPILE_FAILED` (-632)
- [ ] `SITUATION_ERROR_OPENGL_SHADER_LINK` (-611) — redirect to `OPENGL_SHADER_LINK_FAILED` (-633)
- [ ] `SITUATION_ERROR_VULKAN_INIT_FAILED` (-700) — redirect to specific VK init codes
- [ ] `SITUATION_ERROR_VULKAN_INSTANCE_FAILED` (-701) — redirect to `VULKAN_INSTANCE_CREATION_FAILED` (-740)
- [ ] `SITUATION_ERROR_VULKAN_DEVICE_FAILED` (-702) — redirect to `VULKAN_DEVICE_CREATION_FAILED` (-742)
- [ ] `SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED` (-710) — redirect to `VULKAN_SWAPCHAIN_CREATION_FAILED` (-743)
- [ ] `SITUATION_ERROR_VULKAN_PIPELINE_FAILED` (-732) — redirect to `VULKAN_PIPELINE_CREATION_FAILED` (-747)
- [ ] `SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED` (-734) — redirect to `VULKAN_MEMORY_ALLOCATION_FAILED` (-750)
- [ ] After all redirects: consider removing EOL entries from table (ABI decision)

---

## Phase 13 — Reserved / Future Subsystems (No Action Yet)

These errors exist for subsystems not yet implemented. No action required until the
subsystem ships. Keep in table for ABI stability.

### Mixer (bus/track/insert/send/topology) — 7 errors
- `SITUATION_ERROR_MIXER_NOT_INITIALIZED`
- `SITUATION_ERROR_MIXER_TRACK_LIMIT`
- `SITUATION_ERROR_MIXER_TRACK_INVALID`
- `SITUATION_ERROR_MIXER_BUS_LIMIT`
- `SITUATION_ERROR_MIXER_BUS_INVALID`
- `SITUATION_ERROR_MIXER_INSERT_INVALID`
- `SITUATION_ERROR_MIXER_INSERT_ALREADY_ATTACHED`
- `SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED`
- `SITUATION_ERROR_MIXER_SEND_INVALID`

### Network — 11 errors
- `SITUATION_ERROR_NETWORK_INIT_FAILED`
- `SITUATION_ERROR_NETWORK_SOCKET_CREATION_FAILED`
- `SITUATION_ERROR_NETWORK_CONNECTION_FAILED`
- `SITUATION_ERROR_NETWORK_SEND_FAILED`
- `SITUATION_ERROR_NETWORK_RECEIVE_FAILED`
- `SITUATION_ERROR_NETWORK_BIND_FAILED`
- `SITUATION_ERROR_NETWORK_LISTEN_FAILED`
- `SITUATION_ERROR_NETWORK_ACCEPT_FAILED`
- `SITUATION_ERROR_NETWORK_TIMEOUT`
- `SITUATION_ERROR_NETWORK_DNS_RESOLUTION_FAILED`
- `SITUATION_ERROR_NETWORK_TLS_HANDSHAKE_FAILED`

### Plugins — 3 errors
- `SITUATION_ERROR_PLUGIN_LOAD_FAILED`
- `SITUATION_ERROR_PLUGIN_SYMBOL_NOT_FOUND`
- `SITUATION_ERROR_PLUGIN_ABI_MISMATCH`

### Asset Pipeline — 4 errors
- `SITUATION_ERROR_ASSET_PARSE_FAILED`
- `SITUATION_ERROR_ASSET_CORRUPTED`
- `SITUATION_ERROR_ASSET_VERSION_MISMATCH`
- `SITUATION_ERROR_ASSET_DECOMPRESSION_FAILED`

### Other reserved
- `SITUATION_ERROR_MEMORY_ACCESS` (signal handler, not normal return path)
- `SITUATION_ERROR_ARM_INTRINSICS_FAILED` (ARM platform not yet shipping)

---

## Phase 14 — Compat Alias Review

These `#define` aliases exist for historical reasons. No code path produces them
directly (they resolve to the target). Review whether any external code references them.

- [ ] `SITUATION_ERROR_ACCESS_DENIED` → `FILE_ACCESS_DENIED` — keep (external users may reference)
- [ ] `SITUATION_ERROR_GLAD_LOAD_FAILED` → `OPENGL_LOADER_FAILED` — keep
- [ ] `SITUATION_ERROR_GL_ERROR` → `OPENGL_GENERAL` — keep
- [ ] `SITUATION_ERROR_GL_EXTENSION_MISSING` → `OPENGL_UNSUPPORTED` — keep
- [ ] `SITUATION_ERROR_GL_UPLOAD_FAILED` → `TEXTURE_UPLOAD_FAILED` — keep
- [ ] `SITUATION_ERROR_GL_VERSION_TOO_LOW` → `OPENGL_UNSUPPORTED_VERSION` — keep
- [ ] `SITUATION_ERROR_VULKAN_UPLOAD_FAILED` → `TEXTURE_UPLOAD_FAILED` — keep
- [ ] `SITUATION_ERROR_VULKAN_PIPELINE_CREATE_FAILED` → `VULKAN_PIPELINE_CREATION_FAILED` — keep
- [ ] `SITUATION_ERROR_SHADER_LINK_FAILED` → `OPENGL_SHADER_LINK_FAILED` — keep
- [ ] `SITUATION_ERROR_SHADER_MODULE_CREATE_FAILED` → `VULKAN_SHADER_MODULE_FAILED` — keep
- [ ] Document all aliases in `doc/situation_api.md` deprecation section

---

## Validation

After each phase:
1. Rebuild DLL: `build_situation.bat opengl` (and/or `vulkan`)
2. Build tests: `build_tests.bat`
3. Run audit: `powershell -ExecutionPolicy Bypass -File scripts\audit_errno_report.ps1`
4. Verify count of never-produced errors decreases
5. Run tests: `build\sit_test.exe`

**Target:** never-produced count drops from 140 to ~33 (reserved-only).

---

## Priority Order

| Priority | Phase | Errors | Rationale |
|----------|-------|--------|-----------|
| P0 | 0 — Phantoms | 5 | Bugs/inconsistencies in current code |
| P1 | 1 — Platform/Window/Input | 16 | User-facing, most visible |
| P1 | 2 — Filesystem | 11 | Critical for error diagnosis |
| P2 | 3 — Rendering Core | 6 | Developer-facing guard rails |
| P2 | 4 — OpenGL | 7 | Backend specificity |
| P2 | 5 — Vulkan | 8 | Backend specificity |
| P2 | 6 — Compute | 3 | Small, self-contained |
| P2 | 7 — Fonts & Image | 4 | User-facing |
| P3 | 8 — Audio | 5 | Audio subsystem |
| P3 | 9 — Node Graph | 11 | Audio subsystem |
| P3 | 10 — Device Registry | 10 | Audio subsystem |
| P3 | 11 — Threading | 15 | Defensive/internal |
| P4 | 12 — EOL Cleanup | 21 | Housekeeping |
| — | 13 — Reserved | ~33 | No action until subsystem ships |
| P4 | 14 — Compat Aliases | 10 | Documentation |
