# Error Propagation Remediation Plan

**Date**: 2026-05-05  
**Status**: PHASE 1+2 COMPLETE  
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

### Phase 1: Low-Hanging Fruit — Add Error State to Existing Void/Bool Functions (Non-Breaking)

Add `_SituationSetErrorFromCode` calls to functions that already have failure paths but don't report them. No signature changes. No API break.

| File | Functions | Error Code |
|------|-----------|------------|
| `situation_impl_wdm.h` | `SituationSetWindowState`, `SituationClearWindowState`, `SituationSetVSync`, `SituationToggleBorderlessWindowed`, `SituationMaximizeWindow`, `SituationMinimizeWindow`, `SituationRestoreWindow`, `SituationSetWindowIcons`, `SituationSetWindowTitle`, `SituationSetWindowPosition`, `SituationSetWindowSize`, `SituationSetWindowMonitor`, `SituationSetWindowMinSize`, `SituationSetWindowMaxSize`, `SituationSetWindowOpacity`, `SituationSetWindowFocused`, `SituationPauseApp`, `SituationResumeApp`, `SituationSetTargetFPS` | `NOT_INITIALIZED` or `INVALID_PARAM` |
| `situation_impl_wdm.h` | `SituationGetScreenWidth/Height`, `SituationGetRenderWidth/Height`, `SituationGetMonitorCount`, `SituationGetMonitorWidth/Height/PhysicalWidth/PhysicalHeight/RefreshRate` | `NOT_INITIALIZED` or `DISPLAY_QUERY` |
| `situation_impl_vd.h` | `SituationSetVirtualDisplayDirty`, `SituationGetVirtualDisplaySize`, `SituationIsVirtualDisplayDirty` | `NOT_INITIALIZED` or `VIRTUAL_DISPLAY_INVALID_ID` |
| `situation_impl_threading.h` | `SituationCreateThreadPool` (all failure paths), `SituationDispatchParallel`, `SituationWaitForAllJobs`, `SituationDestroyThreadPool`, `SituationDumpTaskGraph` | `THREAD_CREATION_FAILED`, `INVALID_PARAM`, `MEMORY_ALLOCATION` |
| `situation_impl_timer.h` | `SituationTimerGetOscillatorState`, `SituationTimerGetPreviousOscillatorState`, `SituationTimerHasOscillatorUpdated`, `SituationTimerPingOscillator`, `SituationTimerGetOscillatorTriggerCount` | `NOT_INITIALIZED` or `TIMER_SYSTEM` |
| `situation_impl_ctrl.h` | `_SituationGLFWFileDropCallback` (allocation failures) | `MEMORY_ALLOCATION` |
| `situation_impl_io.h` | `SituationGetAppSavePath`, `SituationGetBasePath` (allocation failures) | `MEMORY_ALLOCATION` |
| `situation_impl_image.h` | `_SituationSaveImageBMP` | `FILE_WRITE_FAILED` or `MEMORY_ALLOCATION` |
| `situation_impl_renderer.h` | ~~`SituationCmdBindVertexBuffer`, `SituationCmdBindIndexBuffer`~~ (v2.4.126: return `SituationError`), `SituationGetRenderLatencyStats`, `SituationExportRenderHistogram`, `SituationDrawMetricsOverlay`, `SituationDestroyRenderList`, `SituationResetRenderList` | `NOT_INITIALIZED` or `INVALID_PARAM` |

**Estimated effort**: 2-3 sessions. Mechanical — add one line before each early return.

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

### Phase 3: Return Type Migration (Breaking — Major Version or Opt-In)

Change functions from `bool` → `SituationError` where the bool means "succeeded/failed":

**File I/O** (`situation_impl_io.h`):
- `SituationSaveFileText()` → `SituationError`
- `SituationDeleteFile()` → `SituationError`
- `SituationRenameFile()` → `SituationError`
- `SituationMoveFile()` → `SituationError`
- `SituationCopyFile()` → `SituationError`
- `SituationCreateDirectory()` → `SituationError`
- `SituationDeleteDirectory()` → `SituationError`

**Threading** (`situation_impl_threading.h`):
- `SituationCreateThreadPool()` → `SituationError`
- `SituationAddJobDependency()` → `SituationError`
- `SituationAddJobDependencies()` → `SituationError`
- `SituationWaitForJob()` → `SituationError`

**Rendering** (`situation_impl_renderer.h`):
- `SituationAcquireFrameCommandBuffer()` → `SituationError`
- `SituationSaveModelAsGltf()` → `SituationError`
- `SituationReloadShader()` → `SituationError`
- `SituationReloadTexture()` → `SituationError`
- `SituationReloadModel()` → `SituationError`
- `SituationReloadComputePipeline()` → `SituationError`

**Estimated effort**: 3-4 sessions. Requires updating all call sites + examples + API header.

---

## Migration Strategy for Phase 3

Since this is a breaking change, two approaches:

### Option A: Big Bang (v2.5.0)
Change all signatures at once in a major release. Clean but disruptive.

### Option B: Deprecation Wrapper (Gradual)
```c
// New function with proper return type
SITAPI SituationError SituationSaveFileTextEx(const char* path, const char* text);

// Old function marked deprecated, calls new one
SITAPI bool SituationSaveFileText(const char* path, const char* text) {
    return SituationSaveFileTextEx(path, text) == SITUATION_SUCCESS;
}
```

**Recommendation**: Option A. The library is pre-1.0 in terms of user base. Clean break is better than carrying deprecated wrappers.

---

## Files NOT Needing Changes

These are already well-behaved:
- `situation_impl_ctrl.h` — `_SituationSetErrorFromCode` itself, `_SituationInitPlatform`, `_SituationInitWindow`, `_SituationInitSubsystems` (all return `SituationError` properly)
- `sit/aud/node_graph_impl.h` — Already uses `SituationError` throughout
- `sit/aud/device_registry.h` — Already migrated in v2.4.x
- `sit/aud/graph_serialization_impl.h` — Already uses `SituationError`

---

## Checklist

### Phase 1 (Non-Breaking)
- [x] `situation_impl_wdm.h` — void setters: add error state on early return
- [x] `situation_impl_wdm.h` — getter functions: add error state when returning 0
- [x] `situation_impl_vd.h` — virtual display functions
- [x] `situation_impl_threading.h` — thread pool functions
- [x] `situation_impl_timer.h` — oscillator query functions
- [x] `situation_impl_ctrl.h` — file drop callback allocation failures
- [x] `situation_impl_io.h` — path utility allocation failures
- [x] `situation_impl_image.h` — image save failures
- [x] `situation_impl_renderer.h` — void render utility functions

### Phase 2 (fprintf Pairing)
- [x] `situation_impl_threading.h` — thread creation fprintf
- [x] `situation_impl_renderer.h` — Vulkan error fprintf calls

### Phase 3 (Breaking — Return Type Migration)
- [ ] `situation_impl_io.h` — file operation functions
- [ ] `situation_impl_threading.h` — thread pool lifecycle
- [ ] `situation_impl_renderer.h` — reload/acquire functions
- [ ] `situation_api.h` — update declarations
- [ ] `examples/` — update all affected examples
- [ ] `doc/UPDATELOG.md` — document breaking changes

---

## Risk Assessment

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| Phase 1 introduces subtle behavior change | Very Low | Only adds error state, doesn't change control flow |
| Phase 3 breaks user code | Certain | Major version bump, clear migration guide |
| Performance impact of error string formatting | Negligible | Only on error paths, never hot |
| Missing an error site | Medium | Grep audit + compiler warnings on unhandled enum |

---

**Author**: Kiro  
**Next Action**: Phase 1, file by file, starting with `situation_impl_wdm.h` (highest function count)
