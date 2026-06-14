# Window & Display Module Extraction Plan

**Date:** 2026-05-04  
**Status:** Planned  
**Target Version:** 2.4.7  
**Prerequisite:** IO & Input extraction complete (v2.4.6)

## Goal

Extract the Window and Display subsystem from `situation_impl.h` into a new self-contained module:
- `sit/situation_impl_wdm.h` — Window state, display enumeration, monitor queries, fullscreen management

## Final Target Architecture

```
situation.h (v2.4.7)
├── sit/situation_api.h
├── sit/situation_impl.h              (core lifecycle, renderer only)
│   ├── #include "situation_impl_deps.h"
│   ├── #include "situation_impl_decl.h"
│   ├── #include "situation_impl_forward.h"
│   ├── #include "situation_impl_threading.h"
│   ├── #include "situation_impl_io.h"
│   ├── #include "situation_impl_input.h"
│   ├── #include "situation_impl_wdm.h"       ← NEW
│   └── core function implementations (renderer, lifecycle, resources)
└── sit/situation_impl_audio.h
```

---

## Scope

### Physical Display Enumeration & Cache
- `_SituationMonitorEnumProc` (Windows callback)
- `_SituationMonitorEnumData` (Windows struct)
- `_SituationCachePhysicalDisplays`
- `SituationGetDisplays`
- `SituationRefreshDisplays`
- `_SituationGetCurrentDisplayIdentifier`
- `SituationSetDisplayMode`

### Window State Queries
- `SituationIsWindowFullscreen`
- `SituationIsWindowHidden`
- `SituationIsWindowMinimized`
- `SituationIsWindowMaximized`
- `SituationIsWindowResized`
- `SituationIsWindowState`
- `SituationHasWindowFocus`

### Window State Manipulation
- `SituationSetWindowState`
- `SituationClearWindowState`
- `SituationSetVSync`
- `SituationToggleFullscreen`
- `SituationToggleBorderlessWindowed`
- `SituationMaximizeWindow`
- `SituationMinimizeWindow`
- `SituationRestoreWindow`
- `SituationSetWindowIcons` / `SituationSetWindowIcon`
- `SituationSetWindowTitle`
- `SituationSetWindowPosition`
- `SituationSetWindowSize`
- `SituationSetWindowMonitor`
- `SituationSetWindowMinSize` / `SituationSetWindowMaxSize`
- `SituationSetWindowOpacity`
- `SituationSetWindowFocused`

### Screen & Monitor Queries
- `SituationGetScreenWidth` / `SituationGetScreenHeight`
- `SituationGetRenderWidth` / `SituationGetRenderHeight`
- `SituationGetMonitorCount`
- `SituationGetCurrentMonitor`
- `SituationGetMonitorPosition`
- `SituationGetMonitorWidth` / `SituationGetMonitorHeight`
- `SituationGetMonitorPhysicalWidth` / `SituationGetMonitorPhysicalHeight`
- `SituationGetMonitorRefreshRate`

### Application Lifecycle (Window-related)
- `SituationSetFocusCallback`
- `SituationPauseApp` / `SituationResumeApp` / `SituationIsAppPaused`
- `SituationGetGLFWwindow`
- `SituationGetWindowSize`
- `SituationWindowShouldClose`
- `SituationSetTargetFPS`
- `SituationGetFrameTime` / `SituationGetFPS`

### Excluded (stays in situation_impl.h)
- Virtual Display system (`SituationCreateVirtualDisplay`, `SituationConfigureVirtualDisplay`, `SituationRenderVirtualDisplays`, `_SituationSortVirtualDisplaysCallback`) — deeply coupled with renderer internals
- `_SituationInitWindow` — part of core lifecycle init sequence
- GLFW window callbacks (`_SituationGLFWWindowFocusCallback`, `_SituationGLFWWindowIconifyCallback`, `_SituationGLFWFramebufferSizeCallback`) — already in input module or tightly coupled with init

---

## Phase 1: Move Display Enumeration & Cache

**Actions:**
- [ ] Create `sit/situation_impl_wdm.h` with header/guard
- [ ] Move `_SituationMonitorEnumData` typedef (Windows)
- [ ] Move `_SituationMonitorEnumProc` (Windows callback)
- [ ] Move `_SituationCachePhysicalDisplays`
- [ ] Move `SituationGetDisplays`
- [ ] Move `SituationRefreshDisplays`
- [ ] Move `_SituationGetCurrentDisplayIdentifier`
- [ ] Move `SituationSetDisplayMode`
- [ ] Add `#include "situation_impl_wdm.h"` in `situation_impl.h` (after input)
- [ ] Build both backends

**Verification:**
- [ ] `build_situation.bat all` passes

---

## Phase 2: Move Window State & Manipulation (lines 21852-22742)

**Actions:**
- [ ] Move all `SituationIsWindow*` functions
- [ ] Move all `SituationSetWindow*` functions
- [ ] Move `SituationToggleFullscreen`, `SituationToggleBorderlessWindowed`
- [ ] Move `SituationMaximize/Minimize/RestoreWindow`
- [ ] Move `SituationSetVSync`
- [ ] Move screen/monitor query functions
- [ ] Move `SituationSetFocusCallback`
- [ ] Move `SituationPauseApp`, `SituationResumeApp`, `SituationIsAppPaused`
- [ ] Move `SituationGetGLFWwindow`, `SituationGetWindowSize`, `SituationWindowShouldClose`
- [ ] Move `SituationSetTargetFPS`, `SituationGetFrameTime`, `SituationGetFPS`
- [ ] Build both backends

**Verification:**
- [ ] `build_situation.bat all` passes

---

## Phase 3: Clean Build & Verification

**Actions:**
- [ ] Run `build_situation.bat clean && build_situation.bat all`
- [ ] Verify line counts:
  - [ ] `situation_impl_wdm.h`: ~1,800–2,500 lines
  - [ ] `situation_impl.h`: reduced to ~22,500–23,500 lines
- [ ] Bump version to 2.4.7
- [ ] Update `doc/UPDATELOG.md`

**Verification:**
- [ ] Clean build from scratch passes (both backends)
- [ ] No window/display function implementations remain in `situation_impl.h` (except Virtual Displays and init)
- [ ] All window/display subsystems functional

---

## Constraints

- **Single TU.** Everything is still `#include`d into one translation unit.
- **Include order:** deps → decl → forward → threading → io → input → wdm → core functions
- **WDM after input.** Some window functions reference input state (focus callbacks).
- **No functional changes.** Pure structural refactor.
- **One phase at a time.** Green build between every phase.

---

## Risk Mitigation

- **If display cache functions reference renderer state:** They reference `sit_gs` and `sit_render` which are in decl — fine.
- **If `SituationSetDisplayMode` references Vulkan swapchain:** It sets a flag (`framebuffer_resized`) which is in decl — fine.
- **If `SituationPauseApp`/`SituationResumeApp` are interleaved with audio:** They only set flags in `sit_gs` — fine.
- **If `SituationSetTargetFPS`/`GetFrameTime` are used by the render loop:** They read/write `sit_gs` timer state — fine since decl is included before everything.

---

## Dependencies (from earlier in situation_impl.h)

Functions in the WDM module will call:
- `SituationIsInitialized()` — defined earlier in impl
- `_SituationSetErrorFromCode()` — forward declared
- `SituationApplyCurrentProfileWindowState()` — defined in impl (may need forward decl)
- `_SituationGLFWFramebufferSizeCallback()` — in input module (already defined before WDM)
- `glfwGetMonitors`, `glfwGetVideoMode`, etc. — GLFW (available via deps)

If `SituationApplyCurrentProfileWindowState` is referenced, add a forward declaration in `situation_impl_forward.h`.
