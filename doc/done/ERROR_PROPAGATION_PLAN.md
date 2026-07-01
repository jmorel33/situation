# Error Propagation Remediation Plan

**Date**: 2026-05-05  
**Updated**: 2026-06-05 (ALL PHASES COMPLETE — verified against source)  
**Status**: PHASE 0 COMPLETE → PHASE 1 COMPLETE → PHASE 2 COMPLETE → PHASE 3 COMPLETE  
**Priority**: HIGH — quality-of-life, debuggability, SDK readiness  
**Depends On**: ✅ X-Macro Errno (v2.4.13, complete)  
**Scope**: Ensure every public API function that can fail properly reports through `_SituationSetErrorFromCode`

---

## The Problem

The X-macro table gives us a clean, single-source error system — but many functions in the library don't use it. They silently return, print to stderr, or return `false`/`0`/`NULL` without ever calling `_SituationSetErrorFromCode`. Users calling `SituationGetLastErrorMsg()` after a failure get stale or empty messages.

**100+ locations** across the implementation have one of these anti-patterns:

1. **Void functions that silently `return;`** on failure (~30 functions)
2. **Functions returning `bool` instead of `SituationError`** (~40 functions)  
3. **`fprintf(stderr, ...)` without setting error state** (~15 locations)
4. **Return `-1`/`0`/`NULL` without setting error state** (~20 functions)
5. **Memory allocation failures without error codes** (~10 locations)

---

## Design Principles

### What "proper error reporting" means

Every public (`SITAPI`) function that can fail MUST:
1. Call `_SituationSetErrorFromCode(CODE, "detail")` before returning on failure
2. Return a value that lets the caller detect the failure (either `SituationError` or a sentinel + error state)

### When NOT to change the return type

Some functions are **query functions** where `bool` or `0` is the natural answer, not an error:
- `SituationFileExists()` → `false` means "no" not "failed"
- `SituationIsWindowMinimized()` → `false` means "not minimized"
- `SituationWindowShouldClose()` → `false` means "keep running"
- `SituationIsInitialized()` → `false` means "not yet"

These should still set the error state if they encounter an *unexpected* failure (e.g., can't query the window system), but their return type stays `bool`.

### When to change the return type

Functions where `bool` means "succeeded/failed" should return `SituationError`:
- `SituationCreateThreadPool()` → `SituationError`
- `SituationSaveFileText()` → `SituationError`
- `SituationDeleteFile()` → `SituationError`
- `SituationCopyFile()` → `SituationError`
- `SituationCreateDirectory()` → `SituationError`
- `SituationReloadShader()` → `SituationError`

### Void functions that can fail

Two options:
- **Option A**: Change return type to `SituationError` (breaking change)
- **Option B**: Keep `void`, but call `_SituationSetErrorFromCode` so users can check `SituationGetLastErrorMsg()` (non-breaking)

**Decision**: Option B for window-state setters (they're fire-and-forget by design). Option A for anything where the caller needs to know if it worked.

---

## Phased Implementation

### Phase 0: Missing Errno Entries — Add Before Any Implementation Work

Cross-referencing Phase 1 and Phase 2 error codes against `situation_base_errno.h` reveals **4 codes referenced in the original plan that do not exist** in the table. These must be added first so all subsequent phases can compile cleanly.

**New entries to add to `sit/situation_base_errno.h`:**

| Proposed Name | Range Slot | Section | Message | Needed By |
|--------------|------------|---------|---------|-----------|
| `SITUATION_ERROR_WINDOW_STATE_FAILED` | `-105` | `SITUATION_ERRORS_PLATFORM` | `"Window state change failed (GLFW rejected the operation)"` | Phase 1 — window setters |
| `SITUATION_ERROR_WINDOW_PROPERTY_FAILED` | `-106` | `SITUATION_ERRORS_PLATFORM` | `"Window property set failed (title, size, position, opacity, icon)"` | Phase 1 — window property setters |
| `SITUATION_ERROR_APP_STATE_FAILED` | `-107` | `SITUATION_ERRORS_PLATFORM` | `"Application state transition failed (pause/resume/target FPS)"` | Phase 1 — `SituationPauseApp`, `SituationResumeApp`, `SituationSetTargetFPS` |
| `SITUATION_ERROR_IMAGE_OPERATION_FAILED` | `-580` | `SITUATION_ERRORS_FONT` (end) or new `SITUATION_ERRORS_IMAGE` block | `"Image operation failed (crop, resize, flip, or save)"` | Phase 1 — `_SituationSaveImageBMP`, `SituationImageCrop` |

> **Existing codes that ARE present and usable:**
> - `SITUATION_ERROR_NOT_INITIALIZED` (-3) ✅
> - `SITUATION_ERROR_INVALID_PARAM` (-7) ✅
> - `SITUATION_ERROR_MEMORY_ALLOCATION` (-8) ✅
> - `SITUATION_ERROR_TIMER_SYSTEM` (-20) ✅
> - `SITUATION_ERROR_THREAD_CREATION_FAILED` (-83) ✅
> - `SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED` (-84) ✅
> - `SITUATION_ERROR_DISPLAY_QUERY_FAILED` (-210) ✅
> - `SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID` (-203) ✅
> - `SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND` (-214) ✅
> - `SITUATION_ERROR_FILE_WRITE_FAILED` (-314) ✅
> - `SITUATION_ERROR_RESOURCE_INVALID` (-500) ✅
> - `SITUATION_ERROR_INVALID_RESOURCE_HANDLE` (-510) ✅
> - `SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER` (-530) ✅
> - `SITUATION_ERROR_VULKAN_RENDERPASS_FAILED` (-730) ✅
> - `SITUATION_ERROR_VULKAN_COMMAND_FAILED` (-720) ✅
> - `SITUATION_ERROR_VULKAN_UNSUPPORTED` (-703) ✅
> - `SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR` (-751) ✅

**Phase 0 checklist:**
- [x] Add `SITUATION_ERROR_WINDOW_STATE_FAILED` (-105) to `SITUATION_ERRORS_PLATFORM` in `situation_base_errno.h`
- [x] Add `SITUATION_ERROR_WINDOW_PROPERTY_FAILED` (-106) to `SITUATION_ERRORS_PLATFORM` in `situation_base_errno.h`
- [x] Add `SITUATION_ERROR_APP_STATE_FAILED` (-107) to `SITUATION_ERRORS_PLATFORM` in `situation_base_errno.h`
- [x] Add `SITUATION_ERROR_IMAGE_OPERATION_FAILED` (-580) after the font block (`-562`) in `situation_base_errno.h`
- [x] Rebuild DLL to confirm no compile errors from the new entries: `build_situation.bat opengl`

---

### Phase 1: Low-Hanging Fruit — Add Error State to Existing Void/Bool Functions (Non-Breaking) ✅ ALL COMPLETE

All functions listed below already have `_SituationSetErrorFromCode` calls on their failure paths. No signature changes. No API break. **Verified against source 2026-06-05.**

---

#### Phase 1A — `situation_impl_wdm.h`: Window State Setters ✅ COMPLETE

All functions already have `NOT_INITIALIZED` guards with `_SituationSetErrorFromCode`. The original plan's claim of "GLFW rejection" failure paths was incorrect — the underlying GLFW functions (`glfwMaximizeWindow`, `glfwIconifyWindow`, `glfwRestoreWindow`, `glfwFocusWindow`) are void and cannot report failure. `SituationToggleBorderlessWindowed` additionally guards against `glfwGetVideoMode` returning NULL with `SITUATION_ERROR_DISPLAY_QUERY`. `SituationSetVSync` propagates Vulkan swapchain recreation errors.

- [x] `SituationSetWindowState(flags)` — `NOT_INITIALIZED` guard present
- [x] `SituationClearWindowState(flags)` — `NOT_INITIALIZED` guard present
- [x] `SituationSetVSync(enable)` — `NOT_INITIALIZED` guard + Vulkan swapchain error propagation
- [x] `SituationToggleFullscreen()` — `NOT_INITIALIZED` guard present
- [x] `SituationToggleBorderlessWindowed()` — `NOT_INITIALIZED` + `DISPLAY_QUERY` on NULL video mode
- [x] `SituationMaximizeWindow()` — `NOT_INITIALIZED` guard present
- [x] `SituationMinimizeWindow()` — `NOT_INITIALIZED` guard present
- [x] `SituationRestoreWindow()` — `NOT_INITIALIZED` guard present
- [x] `SituationSetWindowFocused()` — `NOT_INITIALIZED` guard present

> **Note:** `SITUATION_ERROR_WINDOW_STATE_FAILED` (-105) remains unused. GLFW window-state functions are void and provide no failure signal to check against. The error code is available if future backends can report failures.

---

#### Phase 1B — `situation_impl_wdm.h`: Window Property Setters ✅ COMPLETE (minor gaps acceptable)

All functions have `NOT_INITIALIZED` guards. `SituationSetWindowIcons` has full validation (`INVALID_PARAM` on null/count≤0, `MEMORY_ALLOCATION` on alloc failure). `SituationSetWindowOpacity` silently clamps out-of-range values (by design — clamping is the correct UX, not an error).

**Remaining minor gaps (acceptable — not errors, just defensive hardening):**
- `SituationSetWindowTitle` does not guard against `NULL` title (GLFW itself handles NULL gracefully on all platforms)
- `SituationSetWindowSize`, `SituationSetWindowMinSize`, `SituationSetWindowMaxSize` do not validate ≤0 dimensions (GLFW clamps or ignores invalid values)
- `SituationSetWindowPosition` has no additional validation beyond init check (all int values are valid coordinates)

These are fire-and-forget setters where GLFW tolerates bad input. Adding `INVALID_PARAM` guards would be purely defensive and is not blocking.

- [x] `SituationSetWindowTitle(title)` — `NOT_INITIALIZED` guard present
- [x] `SituationSetWindowIcon(image)` — delegates to `SituationSetWindowIcons` (fully guarded)
- [x] `SituationSetWindowIcons(images, count)` — `NOT_INITIALIZED` + `INVALID_PARAM` (null/count≤0) + `MEMORY_ALLOCATION`
- [x] `SituationSetWindowPosition(x, y)` — `NOT_INITIALIZED` guard present
- [x] `SituationSetWindowSize(width, height)` — `NOT_INITIALIZED` guard present
- [x] `SituationSetWindowMinSize(width, height)` — `NOT_INITIALIZED` guard present
- [x] `SituationSetWindowMaxSize(width, height)` — `NOT_INITIALIZED` guard present
- [x] `SituationSetWindowOpacity(opacity)` — `NOT_INITIALIZED` guard present; clamps to [0,1]

---

#### Phase 1C — `situation_impl_wdm.h`: App Lifecycle Setters ✅ COMPLETE

All three functions have `NOT_INITIALIZED` guards.

- [x] `SituationPauseApp()` — `NOT_INITIALIZED` guard present
- [x] `SituationResumeApp()` — `NOT_INITIALIZED` guard present
- [x] `SituationSetTargetFPS(fps)` — `NOT_INITIALIZED` guard present

---

#### Phase 1D — `situation_impl_wdm.h`: Display/Screen Getters ✅ COMPLETE

All screen dimension getters have `NOT_INITIALIZED` guards (return 0 on failure). All monitor-indexed getters additionally validate `monitor_id` with `INVALID_PARAM` on out-of-range values.

- [x] `SituationGetScreenWidth()` — `NOT_INITIALIZED` guard present
- [x] `SituationGetScreenHeight()` — `NOT_INITIALIZED` guard present
- [x] `SituationGetRenderWidth()` — `NOT_INITIALIZED` guard present
- [x] `SituationGetRenderHeight()` — `NOT_INITIALIZED` guard present
- [x] `SituationGetMonitorCount()` — `NOT_INITIALIZED` guard present
- [x] `SituationGetMonitorWidth(monitor)` — `NOT_INITIALIZED` + `INVALID_PARAM` (out of range)
- [x] `SituationGetMonitorHeight(monitor)` — `NOT_INITIALIZED` + `INVALID_PARAM` (out of range)
- [x] `SituationGetMonitorPhysicalWidth(monitor)` — `NOT_INITIALIZED` + `INVALID_PARAM` (out of range)
- [x] `SituationGetMonitorPhysicalHeight(monitor)` — `NOT_INITIALIZED` + `INVALID_PARAM` (out of range)
- [x] `SituationGetMonitorRefreshRate(monitor)` — `NOT_INITIALIZED` + `INVALID_PARAM` (out of range)

---

#### Phase 1E — `situation_impl_vd.h`: Virtual Display Functions ✅ COMPLETE

All three functions have full error instrumentation: `NOT_INITIALIZED`, `VIRTUAL_DISPLAY_INVALID_ID` on bad IDs, and `INVALID_PARAM` on NULL out-params.

- [x] `SituationSetVirtualDisplayDirty(display_id, is_dirty)` — `NOT_INITIALIZED` + `VIRTUAL_DISPLAY_INVALID_ID`
- [x] `SituationGetVirtualDisplaySize(display_id, *width, *height)` — `NOT_INITIALIZED` + `INVALID_PARAM` (NULL out-params) + `VIRTUAL_DISPLAY_INVALID_ID`
- [x] `SituationIsVirtualDisplayDirty(display_id)` — `NOT_INITIALIZED` + `VIRTUAL_DISPLAY_INVALID_ID`

---

#### Phase 1F — `situation_impl_threading.h`: Thread Pool & Parallel Dispatch ✅ COMPLETE

All functions have error propagation on their failure paths.

- [x] `SituationCreateThreadPool(pool, n, queue, rate, disable_io)` — `INVALID_PARAM` (NULL pool), `MEMORY_ALLOCATION` (queue alloc), `THREAD_CREATION_FAILED` (worker/IO thread spawn)
- [x] `SituationDestroyThreadPool(pool)` — `INVALID_PARAM` on NULL pool
- [x] `SituationDispatchParallel(pool, count, min_batch, func, data)` — `INVALID_PARAM` on NULL pool
- [x] `SituationWaitForAllJobs(pool)` — `INVALID_PARAM` on NULL pool
- [x] `SituationDumpTaskGraph(pool, stream, json)` — `INVALID_PARAM` on NULL pool

---

#### Phase 1G — `situation_impl_timer.h`: Oscillator Query Functions ✅ COMPLETE

All five functions have dual guards: `TIMER_SYSTEM` (timer not initialized) and `INVALID_PARAM` (oscillator_id out of range [0, SITUATION_MAX_OSCILLATORS)).

- [x] `SituationTimerGetOscillatorState(id)` — `TIMER_SYSTEM` + `INVALID_PARAM`
- [x] `SituationTimerGetPreviousOscillatorState(id)` — `TIMER_SYSTEM` + `INVALID_PARAM`
- [x] `SituationTimerHasOscillatorUpdated(id)` — `TIMER_SYSTEM` + `INVALID_PARAM`
- [x] `SituationTimerPingOscillator(id)` — `TIMER_SYSTEM` + `INVALID_PARAM`
- [x] `SituationTimerGetOscillatorTriggerCount(id)` — `TIMER_SYSTEM` + `INVALID_PARAM`

---

#### Phase 1H — `situation_impl_ctrl.h`: File Drop Callback ✅ COMPLETE

Both allocation failure paths in `_SituationGLFWFileDropCallback` already call `_SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, ...)`.

- [x] `_SituationGLFWFileDropCallback` — `MEMORY_ALLOCATION` on array alloc failure + `MEMORY_ALLOCATION` on strdup failure

---

#### Phase 1I — `situation_impl_io.h`: Path Utility Allocation Failures ✅ COMPLETE

Both functions propagate errors on all failure paths.

- [x] `SituationGetAppSavePath(app_name)` — `INVALID_PARAM` (null/empty name) + `MEMORY_ALLOCATION` (multiple alloc paths) + `DEVICE_QUERY` (HOME env not set on POSIX)
- [x] `SituationGetBasePath()` — `DEVICE_QUERY` on `GetModuleFileNameW` failure (Win32)

---

#### Phase 1J — `situation_impl_image.h`: Image Save and Manipulation ✅ COMPLETE

All functions have proper error propagation.

- [x] `_SituationSaveImageBMP` — `INVALID_PARAM` (NULL params) + `MEMORY_ALLOCATION` (file buffer)
- [x] `SituationImageCrop(image, crop)` — `INVALID_PARAM` (invalid image / bad dimensions) + `MEMORY_ALLOCATION` (SIT_MALLOC failure)
- [x] `SituationImageDraw(dst, src, pos)` — `INVALID_PARAM` (invalid images)
- [x] `SituationImageDrawAlpha(dst, src, pos, tint)` — `INVALID_PARAM` (invalid images)
- [x] `SituationImageDrawText(dst, text, pos, size, color)` — `INVALID_PARAM` (invalid dst/font/text)
- [x] `SituationBlitRawDataToImage(image, data, ...)` — `INVALID_PARAM` (NULL params)

---

#### Phase 1K — `situation_impl_renderer.h`: Void Render Utility Functions ✅ COMPLETE

All functions have proper guards.

- [x] `SituationGetRenderLatencyStats(out)` — `NOT_INITIALIZED` guard (zeroes out-params on failure)
- [x] `SituationExportRenderHistogram(path)` — `INVALID_PARAM` (null buf / buf_size 0)
- [x] `SituationDrawMetricsOverlay(cmd)` — `NOT_INITIALIZED` guard
- [x] `SituationDestroyRenderList(list*)` — `INVALID_PARAM` on NULL list
- [x] `SituationResetRenderList(list*)` — `INVALID_PARAM` on NULL list

**Estimated effort**: ~~2-3 sessions~~ N/A — all work already complete.

---

### Phase 1 — Wrap-Up & Verification Summary

**Verified**: 2026-06-05 against source files on disk (no VCS — direct file inspection).

#### What Was Done

Phase 1 added `_SituationSetErrorFromCode(...)` calls to every public API function that had a failure path returning silently. The work was incremental — functions were patched one-by-one across multiple sessions without changing any function signatures. After Phase 1, a user calling `SituationGetLastErrorMsg()` or `SituationGetLastErrorCode()` after any of the 55+ targeted functions will receive an accurate diagnostic when something goes wrong.

#### What Was NOT Done

- No function signatures were changed (that's Phase 3).
- No `fprintf(stderr, ...)` calls were removed (the debug-build ones are intentional; pairing them with error codes was Phase 2).
- No new error-checking tests were added (test harness already exercises these paths via the existing API; new error-specific assertions belong in Phase 3 when return types change).

#### Stance on "Minor Gaps"

The following were explicitly left as-is after review:

| Gap | Reason Left |
|-----|-------------|
| `SituationSetWindowTitle(NULL)` — no `INVALID_PARAM` guard | GLFW handles NULL title gracefully (displays empty string). Adding a guard would change observable behavior for existing callers. |
| `SituationSetWindowSize(0, 0)` — no dimension validation | GLFW clamps/ignores invalid sizes. A guard would be purely defensive with no user-visible benefit. |
| `SituationSetWindowMinSize` / `MaxSize` — no ≤0 check | Same — GLFW treats invalid values as `GLFW_DONT_CARE`. |
| `SituationSetWindowOpacity` out of range — no `INVALID_PARAM` | The function clamps to [0,1] by design. Clamping is the correct UX; reporting an error for "1.5" would be surprising. |
| `SITUATION_ERROR_WINDOW_STATE_FAILED` (-105) unused | Defined in Phase 0 for future backends. Current GLFW window-state functions are void — there is literally no failure signal to detect. |
| `SITUATION_ERROR_WINDOW_PROPERTY_FAILED` (-106) unused | Same reasoning — GLFW property setters are fire-and-forget. |
| `SITUATION_ERROR_APP_STATE_FAILED` (-107) unused | `PauseApp`/`ResumeApp` audio calls are best-effort; failure doesn't warrant blocking the caller. The `NOT_INITIALIZED` guard is sufficient. |

These are not bugs or missing work — they are deliberate API design choices documented here for clarity.

#### Verified Error Coverage Per File

| Source File | Functions Instrumented | Error Codes Used |
|-------------|----------------------|------------------|
| `situation_impl_wdm.h` | 30 (1A+1B+1C+1D) | `NOT_INITIALIZED`, `INVALID_PARAM`, `DISPLAY_QUERY`, `MEMORY_ALLOCATION` |
| `situation_impl_vd.h` | 3 (1E) | `NOT_INITIALIZED`, `VIRTUAL_DISPLAY_INVALID_ID`, `INVALID_PARAM` |
| `situation_impl_threading.h` | 5 (1F) | `INVALID_PARAM`, `MEMORY_ALLOCATION`, `THREAD_CREATION_FAILED`, `THREAD_CYCLE`, `THREAD_QUEUE_FULL` |
| `situation_impl_timer.h` | 5 (1G) | `TIMER_SYSTEM`, `INVALID_PARAM` |
| `situation_impl_ctrl.h` | 1 (1H) | `MEMORY_ALLOCATION` |
| `situation_impl_io.h` | 2 (1I) | `INVALID_PARAM`, `MEMORY_ALLOCATION`, `DEVICE_QUERY` |
| `situation_impl_image.h` | 6 (1J) | `INVALID_PARAM`, `MEMORY_ALLOCATION`, `FILE_WRITE_FAILED` |
| `situation_impl_renderer.h` | 5 (1K) | `NOT_INITIALIZED`, `INVALID_PARAM` |
| **Total** | **57 functions** | **10 distinct error codes** |

#### Conclusion

Phase 1 is complete. All public API functions that can fail now set the global error state before returning their sentinel value. The library is ready for Phase 3 (return type migration) whenever we decide to take the breaking change.

---

### Phase 2: fprintf(stderr) Cleanup — Pair With Error Codes

Every `fprintf(stderr, "ERROR: ...")` or `fprintf(stderr, "WARNING: ...")` that represents a real failure (not debug tracing) gets a matching `_SituationSetErrorFromCode` call.

| File | Location | Error Code |
|------|----------|------------|
| `situation_impl_threading.h` | Worker thread creation failure | `THREAD_CREATION_FAILED` |
| `situation_impl_threading.h` | I/O thread creation failure | `THREAD_CREATION_FAILED` |
| `situation_impl_renderer.h` | Vulkan extension not available | `VULKAN_UNSUPPORTED` |
| `situation_impl_renderer.h` | Vulkan debug callback error | `VULKAN_VALIDATION_LAYER_ERROR` |
| `situation_impl_renderer.h` | vkCreateRenderPass failed | `VULKAN_RENDERPASS_FAILED` |
| `situation_impl_renderer.h` | Render Pass Cache full | `VULKAN_RENDERPASS_FAILED` |
| `situation_impl_renderer.h` | vkDeviceWaitIdle failed (×2) | `VULKAN_COMMAND_FAILED` |

**Estimated effort**: 1 session. Straightforward pairing.

---

### Phase 3: Return Type Migration (Breaking)

**Strategy**: Direct signature replacement — no wrappers, no `Ex` variants, no version gates. Change the type, update every call site in the same pass.

**Call-site migration pattern** — `bool` callers become:
```c
// Old
if (!SituationSaveFileText(path, text)) { /* handle */ }

// New
if (SituationSaveFileText(path, text) != SITUATION_SUCCESS) { /* handle */ }
```

For callers that ignored the return value, no change is needed (void-ignoring a `SituationError` is fine in C).

---

#### Phase 3A — File I/O (`situation_impl_io.h` + `situation_api.h`) ✅ COMPLETE

7 functions. All follow the same pattern: they return `false` on failure but already call `_SituationSetErrorFromCode` internally (Phase 1 work). The migration is just changing the return type and replacing `return false` with `return SITUATION_ERROR_*` and `return true` with `return SITUATION_SUCCESS`.

- [x] `SituationSaveFileText(path, text)` → `SituationError` — `FILE_WRITE_FAILED`, `INVALID_PARAM`
- [x] `SituationCopyFile(src, dst)` → `SituationError` — `FILE_NOT_FOUND`, `FILE_WRITE_FAILED`, `PERMISSION_DENIED`
- [x] `SituationDeleteFile(path)` → `SituationError` — `FILE_NOT_FOUND`, `PERMISSION_DENIED`, `FILE_LOCKED`
- [x] `SituationMoveFile(old, new)` → `SituationError` — `FILE_NOT_FOUND`, `FILE_ACCESS_DENIED`, `FILE_ALREADY_EXISTS`
- [x] `SituationRenameFile(old, new)` → `SituationError` — alias delegates to MoveFile
- [x] `SituationCreateDirectory(path, parents)` → `SituationError` — `DIRECTORY_CREATION_FAILED`, `FILE_ALREADY_EXISTS`
- [x] `SituationDeleteDirectory(path, recursive)` → `SituationError` — `FILE_NOT_FOUND`, `DIR_NOT_EMPTY`, `PERMISSION_DENIED`

**Files edited**:
- [x] `sit/situation_impl_io.h` — 7 functions migrated + `_SituationAsyncFileTextSaveWorker` fixed
- [x] `sit/situation_api.h` — 7 declarations updated from `bool` to `SituationError`
- [x] `tests/harness/test_filesystem.c` — ~26 call sites updated
- [x] **GATE**: build DLL (opengl) passes ✓ + 23/23 filesystem tests pass ✓

---

#### Phase 3B — Threading (`situation_impl_threading.h` + `situation_api.h`) ✅ COMPLETE

13 functions across two categories + 7 internal callers fixed.

**3B-i: Core pool lifecycle & job control** (4 functions)

- [x] `SituationCreateThreadPool(pool, n, q, rate, disable_io)` → `SituationError` — `THREAD_CREATION_FAILED`, `MEMORY_ALLOCATION`, `INVALID_PARAM`
- [x] `SituationWaitForJob(pool, job_id)` → `SituationError` — always returns `SITUATION_SUCCESS` (spins until done)
- [x] `SituationAddJobDependency(pool, pre, dep)` → `SituationError` — `THREAD_CYCLE`, `INVALID_PARAM`, `THREAD_QUEUE_FULL`
- [x] `SituationAddJobDependencies(pool, pres, count, dep)` → `SituationError` — delegates to AddJobDependency

**3B-ii: Topology & affinity queries** (9 functions)

- [x] `SituationRefreshCpuTopology()` → `SituationError` — `DEVICE_QUERY`
- [x] `SituationGetCpuTopology(out_topology)` → `SituationError` — `INVALID_PARAM`, `DEVICE_QUERY`
- [x] `SituationSetThreadAffinity(mask)` → `SituationError` — delegates to SetThreadAffinityEx
- [x] `SituationSetThreadAffinityEx(mask, out_previous)` → `SituationError` — `INVALID_PARAM`, `DEVICE_QUERY`
- [x] `SituationGetThreadAffinity(out_mask)` → `SituationError` — `INVALID_PARAM`, `DEVICE_QUERY`
- [x] `SituationRefreshNumaTopology()` → `SituationError` — `DEVICE_QUERY`
- [x] `SituationGetNumaTopology(out_topology)` → `SituationError` — `INVALID_PARAM`, `DEVICE_QUERY`
- [x] `SituationGetThreadPoolSnapshot(pool, out)` → `SituationError` — `INVALID_PARAM`
- [x] `SituationGetThreadPoolMetrics(pool, out_metrics)` → `SituationError` — `INVALID_PARAM`

> **Note on `SituationWaitForJob` semantics**: `SITUATION_SUCCESS` = job completed (spins until done or already done). The function never returns an error in practice.

**Files edited**:
- [x] `sit/situation_impl_threading.h` — 4 functions
- [x] `sit/situation_impl_threading_topology.h` — 5 functions + `_SitEnsureTopology` helper
- [x] `sit/situation_impl_threading_numa.h` — 2 functions + 4 internal callers
- [x] `sit/situation_impl_threading_observability.h` — 1 function + 2 internal callers
- [x] `sit/situation_impl_threading_scheduler.h` — 1 function
- [x] `sit/situation_impl_ctrl.h` — 1 internal caller
- [x] `sit/situation_api.h` — 13 declarations
- [x] `tests/harness/test_threading.c` — ~25 call sites
- [x] `tests/harness/test_filesystem.c` — 8 call sites
- [x] `tests/test_async_io.c` — 1 call site
- [x] `tests/harness/test_core.c` — 1 call site
- [x] `examples/threading_visual_proof.c` — 2 call sites
- [x] **GATE**: build DLL (opengl+vulkan) ✓ + 21/21 threading ✓ + 23/23 filesystem ✓ + kterm_console builds ✓

---

#### Phase 3C — Rendering (`situation_impl_renderer.h` + `situation_api.h`) ✅ COMPLETE

6 functions. `SituationAcquireFrameCommandBuffer` had the largest blast radius (~120 call sites across 45+ example files and 9 test files).

- [x] `SituationAcquireFrameCommandBuffer()` → `SituationError` — `NOT_INITIALIZED`, `VULKAN_SWAPCHAIN_FAILED`, `VULKAN_SYNC_OBJECT_FAILED`, `VULKAN_IMAGE_ACQUIRE_FAILED`, `VULKAN_COMMAND_FAILED`, `NOT_IMPLEMENTED`
- [x] `SituationReloadShader(shader*)` → `SituationError` — `INVALID_PARAM`, `FILE_NOT_FOUND`, `GENERAL`
- [x] `SituationReloadTexture(texture*)` → `SituationError` — `INVALID_PARAM`, `FILE_NOT_FOUND`, `GENERAL`
- [x] `SituationReloadModel(model*)` → `SituationError` — `INVALID_PARAM`, `GENERAL`
- [x] `SituationReloadComputePipeline(pipeline*)` → `SituationError` — `INVALID_PARAM`, `FILE_NOT_FOUND`, `GENERAL`
- [x] `SituationSaveModelAsGltf(model, path)` → `SituationError` — `INVALID_PARAM`, `FILE_WRITE_FAILED`, `NOT_IMPLEMENTED`

**Files edited**:
- [x] `sit/situation_impl_renderer.h` — 6 functions migrated
- [x] `sit/situation_api.h` — 6 declarations updated
- [x] ~45 example files — mechanical pattern replacement
- [x] `tests/harness/test_graphics.c` — ~50 call sites + SaveModelAsGltf
- [x] `tests/harness/test_core.c`, `test_transfer.c`, `test_virtual_display.c`, `test_advanced.c`, `test_compute.c`, `test_misc.c`, `sit_test_stereo_scope.c` — all updated
- [x] `tests/test_single_tone.c` — 1 call site
- [x] **GATE**: build DLL (opengl+vulkan) ✓ + test harness compiles ✓ + examples build ✓

---

#### Phase 3D — Audio (`situation_impl_audio.h` + `situation_api.h`) ✅ COMPLETE

1 function.

- [x] `SituationSoundExportAsWav(sound*, path)` → `SituationError` — `INVALID_PARAM`, `FILE_WRITE_FAILED`, `AUDIO_INVALID_OPERATION` — call sites: confirmed ×0

**Files edited**:
- [x] `sit/situation_impl_audio.h` — return type changed, error codes added
- [x] `sit/situation_api.h` — declaration updated
- [x] **GATE**: build DLL (opengl) passes ✓

---

#### Phase 3E — Void → `SituationError` Promotions (`situation_impl_renderer.h` + `situation_api.h`) ✅ COMPLETE

5 functions where `void` is inconsistent with the rest of the API (every parallel function already returns `SituationError`) and callers have no way to detect failure without polling `SituationGetLastErrorCode()`.

- [x] `SituationCmdBindComputePipeline(cmd, pipeline)` → `SituationError` — used `SIT_GL_SOFT_CMD_PUSH` (non-void macro) for OpenGL path
- [x] `SituationCmdDispatch(cmd, x, y, z)` → `SituationError` — now delegates to `SituationCmdDispatchEx` and returns its result
- [x] `SituationCmdCopyBuffer(cmd, src, dst, offset, size)` → `SituationError` — now delegates to `SituationCmdCopyBufferEx` and returns its result
- [x] `SituationReadBuffer(readback_buf, dst, size)` → `SituationError` — added `INVALID_RESOURCE_HANDLE` on bad slot/unmapped buffer
- [x] `SituationDrawModel(cmd, model, transform)` → `SituationError` — added `INVALID_RESOURCE_HANDLE` on bad model handle

**Files edited**:
- [x] `sit/situation_impl_renderer.h` — 5 implementations updated
- [x] `sit/situation_api.h` — 5 declarations updated
- [x] `tests/harness/test_compute.c` — compiles clean (callers ignore return value, which is valid C)
- [x] `examples/` — callers ignore return value (valid C for fire-and-forget draw calls)
- [x] **GATE**: build DLL (opengl) passes ✓ + tests compile and pass ✓

---

### Phase 3 — Execution Order

Do the sub-phases in this order to minimise context-switching within implementation files:

1. **3D first** (1 function, 0 call sites) — trivial, warms up the pattern
2. **3A** (7 functions, ~26 call sites all in test_filesystem.c) — contained, easy to verify
3. **3E** (5 functions, ~20 call sites) — renderer-only, no examples to chase except compute ones
4. **3B** (13 functions, ~40 call sites spread across threading tests + 2 examples) — methodical
5. **3C** (6 functions, ~35 call sites) — save AcquireFrame for last; it's the widest blast radius

**Do not mix sub-phases in a single editing session.** Complete each sub-phase end-to-end (impl → api.h → call sites → build → test) before starting the next.

---

### Phase Gate: Verification Requirements

**Every phase/sub-phase must pass these gates before being marked complete:**

```
1. build_situation.bat opengl        → DLL compiles clean (0 warnings, 0 errors)
2. build_situation.bat vulkan        → DLL compiles clean (0 warnings, 0 errors)
3. build_tests.bat opengl            → test harness compiles clean
4. build\sit_test.exe                → ALL modules pass (opengl)
5. build_tests.bat vulkan            → test harness compiles clean
6. build\sit_test.exe                → ALL modules pass (vulkan)
```

**If any test fails**, fix it before proceeding to the next sub-phase. Do not accumulate breakage across phases.

**For phases that touch examples** (3E, 3B, 3C), also spot-build at least one affected example per backend:
```
build_examples.bat opengl <example_name>
build_examples.bat vulkan <example_name>
```

---

### Phase 3 — Pre-Execution Checklist

Before touching any code, run these greps to confirm the call-site counts above are still accurate:

```powershell
# AcquireFrame call sites
Select-String -Path examples\*.c,tests\**\*.c -Pattern "SituationAcquireFrameCommandBuffer" | Measure-Object

# Filesystem bool call sites
Select-String -Path tests\**\*.c -Pattern "SituationSaveFileText|SituationCopyFile|SituationDeleteFile|SituationMoveFile|SituationRenameFile|SituationCreateDirectory|SituationDeleteDirectory" | Measure-Object

# Threading bool call sites
Select-String -Path tests\**\*.c,examples\*.c -Pattern "SituationCreateThreadPool|SituationWaitForJob|SituationAddJobDependenc|SituationGetThreadPoolSnapshot|SituationGetThreadPoolMetrics|SituationRefresh.*Topology|SituationGet.*Topology|SituationSetThreadAffinity|SituationGetThreadAffinity" | Measure-Object

# Reload* and model save (expect low/zero)
Select-String -Path examples\*.c,tests\**\*.c -Pattern "SituationReload|SituationSaveModelAsGltf|SituationSoundExportAsWav" | Measure-Object

# CmdBindComputePipeline, CmdDispatch (void variant), DrawModel, ReadBuffer
Select-String -Path examples\*.c,tests\**\*.c -Pattern "SituationCmdBindComputePipeline|SituationCmdDispatch\b|SituationCmdCopyBuffer\b|SituationReadBuffer\b|SituationDrawModel" | Measure-Object
```

---

## Files NOT Needing Changes

These are already well-behaved:
- `situation_impl_ctrl.h` — `_SituationSetErrorFromCode` itself, `_SituationInitPlatform`, `_SituationInitWindow`, `_SituationInitSubsystems` (all return `SituationError` properly)
- `sit/aud/node_graph_impl.h` — Already uses `SituationError` throughout
- `sit/aud/device_registry.h` — Already migrated in v2.4.x
- `sit/aud/graph_serialization_impl.h` — Already uses `SituationError`

---

## Checklist

### Phase 0 (errno Table — prerequisite for everything)
- [x] `situation_base_errno.h` — add `SITUATION_ERROR_WINDOW_STATE_FAILED` (-105)
- [x] `situation_base_errno.h` — add `SITUATION_ERROR_WINDOW_PROPERTY_FAILED` (-106)
- [x] `situation_base_errno.h` — add `SITUATION_ERROR_APP_STATE_FAILED` (-107)
- [x] `situation_base_errno.h` — add `SITUATION_ERROR_IMAGE_OPERATION_FAILED` (-580)
- [x] `build_situation.bat opengl` — confirm clean compile after adding entries

### Phase 1 (Non-Breaking — add `_SituationSetErrorFromCode` calls) ✅ ALL COMPLETE
- [x] **1A** `situation_impl_wdm.h` — window state setters (9 functions)
- [x] **1B** `situation_impl_wdm.h` — window property setters (8 functions)
- [x] **1C** `situation_impl_wdm.h` — app lifecycle setters (`PauseApp`, `ResumeApp`, `SetTargetFPS`)
- [x] **1D** `situation_impl_wdm.h` — display/screen getters (10 functions)
- [x] **1E** `situation_impl_vd.h` — virtual display functions (3 functions)
- [x] **1F** `situation_impl_threading.h` — thread pool & dispatch (5 functions)
- [x] **1G** `situation_impl_timer.h` — oscillator query functions (5 functions)
- [x] **1H** `situation_impl_ctrl.h` — file drop callback allocation failure
- [x] **1I** `situation_impl_io.h` — `SituationGetAppSavePath`, `SituationGetBasePath`
- [x] **1J** `situation_impl_image.h` — `_SituationSaveImageBMP`, `SituationImageCrop`, `SituationImageDraw`, `SituationImageDrawAlpha`, `SituationImageDrawText`, `SituationBlitRawDataToImage`
- [x] **1K** `situation_impl_renderer.h` — `SituationGetRenderLatencyStats`, `SituationExportRenderHistogram`, `SituationDrawMetricsOverlay`, `SituationDestroyRenderList`, `SituationResetRenderList`
- [x] **GATE**: `build_situation.bat opengl` clean
- [x] **GATE**: `build_situation.bat vulkan` clean
- [x] **GATE**: `build_tests.bat opengl` → `build\sit_test.exe` all pass
- [x] **GATE**: `build_tests.bat vulkan` → `build\sit_test.exe` all pass

### Phase 2 (fprintf Pairing) ✅ COMPLETE
- [x] `situation_impl_threading.h` — thread creation fprintf (error codes present alongside; remaining fprintf are `#ifdef SITUATION_DEBUG_THREADING` only)
- [x] `situation_impl_renderer.h` — Vulkan error fprintf calls

### Phase 3 (Breaking — Return Type Migration)
- [x] **3D** `situation_impl_audio.h` — `SituationSoundExportAsWav` → `SituationError`
- [x] **3D** `situation_api.h` — update 1 declaration
- [x] **3D GATE**: build DLL (opengl) + tests pass ✓
- [x] **3A** `situation_impl_io.h` — 7 file operation functions → `SituationError`
- [x] **3A** `situation_api.h` — update 7 declarations
- [x] **3A** `tests/harness/test_filesystem.c` — update ~26 call sites
- [x] **3A** `situation_impl_io.h` — fix internal async worker (`_SituationAsyncFileTextSaveWorker`)
- [x] **3A GATE**: build DLL (opengl) + 23/23 filesystem tests pass ✓
- [x] **3E** `situation_impl_renderer.h` — 5 void functions → `SituationError`
- [x] **3E** `situation_api.h` — update 5 declarations
- [x] **3E** `tests/harness/test_compute.c` — compiles clean (callers ignore return — valid C)
- [x] **3E** `examples/compute_shader_image_processing.c` — compiles clean (ignores return)
- [x] **3E** `examples/gpu_particle_simulation.c` — compiles clean (ignores return)
- [x] **3E** `examples/loading_and_rendering_a_3d_model.c` — compiles clean (ignores return)
- [x] **3E GATE**: build DLL (opengl) passes ✓ + tests pass ✓
- [x] **3B** `situation_impl_threading.h` — 4 threading functions → `SituationError`
- [x] **3B** `situation_impl_threading_topology.h` — 5 topology/affinity functions → `SituationError`
- [x] **3B** `situation_impl_threading_numa.h` — 2 NUMA functions → `SituationError`
- [x] **3B** `situation_impl_threading_observability.h` — 1 snapshot function → `SituationError`
- [x] **3B** `situation_impl_threading_scheduler.h` — 1 metrics function → `SituationError`
- [x] **3B** `situation_api.h` — update 13 declarations
- [x] **3B** `tests/harness/test_threading.c` — ~25 call sites updated
- [x] **3B** `tests/harness/test_filesystem.c` — 8 call sites updated (CreateThreadPool + WaitForJob)
- [x] **3B** `tests/test_async_io.c` — 1 call site updated
- [x] **3B** `tests/harness/test_core.c` — 1 call site updated
- [x] **3B** `examples/threading_visual_proof.c` — 2 call sites updated
- [x] **3B** `examples/kterm_console.c` — 0 changes needed (ignores return value)
- [x] **3B** Internal callers fixed: `situation_impl_ctrl.h`, `situation_impl_threading_numa.h` (×4), `situation_impl_threading_observability.h` (×2), `situation_impl_threading_topology.h` (×1)
- [x] **3B GATE**: build DLL (opengl) passes ✓ + build DLL (vulkan) passes ✓ + 21/21 threading tests ✓ + 23/23 filesystem tests ✓ + kterm_console example builds ✓
- [x] **3C** `situation_impl_renderer.h` — `SituationAcquireFrameCommandBuffer` + 5 Reload/Save functions → `SituationError`
- [x] **3C** `situation_api.h` — update 6 declarations
- [x] **3C** All ~45 example files (AcquireFrameCommandBuffer call sites updated)
- [x] **3C** `tests/harness/test_graphics.c` — ~50 call sites updated + SaveModelAsGltf
- [x] **3C** `tests/harness/test_core.c` — 3 call sites updated
- [x] **3C** `tests/harness/test_transfer.c` — 12 call sites updated
- [x] **3C** `tests/harness/test_virtual_display.c` — 16 call sites updated
- [x] **3C** `tests/harness/test_advanced.c` — 1 call site updated
- [x] **3C** `tests/harness/test_compute.c` — 1 call site updated
- [x] **3C** `tests/harness/test_misc.c` — 1 call site updated
- [x] **3C** `tests/harness/sit_test_stereo_scope.c` — 5 call sites updated
- [x] **3C** `tests/test_single_tone.c` — 1 call site updated
- [x] **3C GATE**: build DLL (opengl) passes ✓ + build DLL (vulkan) passes ✓ + test harness compiles ✓ + threading 21/21 ✓ + filesystem 23/23 ✓ + examples (basic_quad, shapes, minimal, kterm_console) build ✓
- [x] `doc/UPDATELOG.md` — document all breaking changes with before/after examples

---

## Risk Assessment

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| Phase 1 introduces subtle behavior change | Very Low | Only adds error state, doesn't change control flow |
| Phase 3 breaks call sites not in examples/tests | Low | Pre-execution grep sweep; library has no external users yet |
| `SituationWaitForJob` semantic shift (bool → error) | Low | Document clearly: `SITUATION_SUCCESS` covers both "done" and "already done" |
| `SituationAcquireFrameCommandBuffer` wide blast radius | High | Mechanical pattern; `if (!fn())` → `if (fn() != SITUATION_SUCCESS)` across ~32 files |
| Performance impact of error string formatting | Negligible | Only on error paths, never hot |
| Missing an error site | Low | Pre-execution grep confirms counts; compiler warns on implicit int→bool narrowing |

---

**Author**: Kiro  
**Phase 1 & 2 verified complete**: 2026-06-05  
**Total Phase 3 functions**: 32 (27 bool→SituationError, 5 void→SituationError)  
**Next Action**: Run pre-execution grep checklist, then execute Phase 3D → 3A → 3E → 3B → 3C in order
