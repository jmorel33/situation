# IO & Input Subsystem Extraction Plan

**Date:** 2026-05-04  
**Status:** Planned  
**Target Version:** 2.4.6  
**Prerequisite:** Decl split complete (v2.4.5)

## Goal

Extract the IO and Input subsystems from `situation_impl.h` into two new self-contained modules:
- `sit/situation_impl_io.h` — File operations, async IO, IO thread, hot-reload
- `sit/situation_impl_input.h` — Keyboard, mouse, joystick, GLFW input callbacks

## Final Target Architecture

```
situation.h (v2.4.6)
├── sit/situation_api.h
├── sit/situation_impl.h              (core lifecycle, window, renderer only)
│   ├── #include "situation_impl_deps.h"
│   ├── #include "situation_impl_decl.h"
│   ├── #include "situation_impl_forward.h"
│   ├── #include "situation_impl_threading.h"
│   ├── #include "situation_impl_io.h"        ← NEW
│   ├── #include "situation_impl_input.h"     ← NEW
│   └── core function implementations (window, renderer, lifecycle)
└── sit/situation_impl_audio.h
```

---

# PART A: situation_impl_io.h

## Scope

### Sync File Operations
- `SituationLoadFileData`, `SituationSaveFileData`
- `SituationLoadFileText`, `SituationSaveFileText`

### Async File Operations
- `SituationLoadFileAsync`, `SituationSaveFileAsync`
- `SituationLoadFileTextAsync`, `SituationSaveFileTextAsync`

### Path Management
- `SituationGetAppSavePath`, `SituationGetBasePath`
- `SituationGetFileName`, `SituationGetFileExtension`
- `SituationGetUserDirectory`

### Directory Operations
- `SituationListDirectoryFiles`, `SituationClearDirectoryFiles`
- `SituationDeleteFile`, `SituationFileExists`, `SituationDirectoryExists`, `SituationCreateDirectory`
- `SituationGetFileModTime`, `SituationGetFileSize`

### IO Thread (from situation_impl_threading.h)
- `_SituationIOThreadEntry` — dedicated IO thread loop
- `SituationGetIOQueueDepth`

### Hot-Reload / Velocity Module
- `_SituationPerformHotReloadPass`
- File watcher logic, shader/texture/model reload triggers

### Internal Helpers
- `_SituationSetFilesystemError`
- `_sit_dirname`, `_sit_directory_exists` (if IO-exclusive)

---

## Phase A1: Identify IO Boundaries

**Actions:**
- [x] Map all IO function locations in `situation_impl.h` (line ranges)
- [x] Map `_SituationIOThreadEntry` location in `situation_impl_threading.h`
- [x] Map hot-reload function locations
- [x] Identify shared helpers (used by both IO and non-IO code)

**Verification:**
- [x] Complete function list with line numbers

---

## Phase A2: Create situation_impl_io.h with Sync File Ops

**Actions:**
- [x] Create `sit/situation_impl_io.h` with header/guard
- [x] Move `SituationLoadFileData`, `SituationSaveFileData`, `SituationLoadFileText`, `SituationSaveFileText`
- [x] Move `_SituationSetFilesystemError`
- [x] Add `#include "situation_impl_io.h"` in `situation_impl.h`
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes
- [x] Asset loaders still work (they call these functions)

---

## Phase A3: Move Path & Directory Operations

**Actions:**
- [x] Move all path management functions
- [x] Move all directory operation functions
- [x] Move file metadata functions
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes

---

## Phase A4: Move Async IO Wrappers

**Actions:**
- [x] Move `SituationLoadFileAsync`, `SituationSaveFileAsync`
- [x] Move `SituationLoadFileTextAsync`, `SituationSaveFileTextAsync`
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes

---

## Phase A5: Move IO Thread & Hot-Reload

**Actions:**
- [x] Move `_SituationIOThreadEntry` from `situation_impl_threading.h` to `situation_impl_io.h`
- [x] Move `SituationGetIOQueueDepth`
- [ ] Move `_SituationPerformHotReloadPass` — **DEFERRED**: depends on renderer internals (shader/texture/audio registries). Stays in `situation_impl.h`. IO thread calls it via forward declaration.
- [x] Update forward declarations as needed
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes
- [x] Threading module no longer contains IO logic
- [x] IO thread still spawned correctly (thread pool creates it, IO module defines its entry)

---

# PART B: situation_impl_input.h

## Scope

### GLFW Input Callbacks
- `_SituationGLFWKeyCallback`
- `_SituationGLFWCharCallback`
- `_SituationGLFWMouseButtonCallback`
- `_SituationGLFWCursorPosCallback`
- `_SituationGLFWScrollCallback`
- `_SituationGLFWJoystickCallback`

### Keyboard API
- `SituationIsKeyPressed`, `SituationIsKeyDown`, `SituationIsKeyReleased`, `SituationIsKeyUp`
- `SituationGetKeyPressed`, `SituationGetCharPressed`
- Key queue management

### Mouse API
- `SituationGetMousePosition`, `SituationGetMouseDelta`
- `SituationIsMouseButtonPressed`, `SituationIsMouseButtonDown`, `SituationIsMouseButtonReleased`
- `SituationGetMouseWheelMove`
- Cursor show/hide/lock

### Joystick/Gamepad API
- `SituationIsJoystickAvailable`, `SituationIsGamepad`
- `SituationGetJoystickAxes`, `SituationGetJoystickButtons`
- `SituationGetGamepadName`, `SituationGetGamepadState`
- `SituationSetGamepadVibration`
- Joystick connect/disconnect event queue

### Input State Management
- `SituationPollInputEvents` (or the internal input update logic)
- Input state reset between frames

---

## Phase B1: Identify Input Boundaries

**Actions:**
- [x] Map all input function locations in `situation_impl.h`
- [x] Map GLFW callback implementations
- [x] Identify the input polling/update section
- [x] Identify dependencies (input functions that reference `sit_gs` window state)

**Verification:**
- [x] Complete function list with line numbers

---

## Phase B2: Create situation_impl_input.h with Callbacks

**Actions:**
- [x] Create `sit/situation_impl_input.h` with header/guard
- [x] Move all GLFW input callbacks (key, char, mouse, scroll, joystick)
- [ ] Remove their forward declarations from `situation_impl_forward.h` (kept — redundant but harmless)
- [x] Add `#include "situation_impl_input.h"` in `situation_impl.h`
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes

---

## Phase B3: Move Keyboard & Mouse API

**Actions:**
- [x] Move all `SituationIsKey*`, `SituationGetKey*`, `SituationGetChar*` functions
- [x] Move all `SituationGetMouse*`, `SituationIsMouse*` functions
- [x] Move cursor control functions
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes

---

## Phase B4: Move Joystick/Gamepad API

**Actions:**
- [x] Move all joystick/gamepad functions
- [x] Move joystick event queue processing
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes

---

## Phase B5: Move Input Polling & State Management

**Actions:**
- [ ] Move `SituationPollInputEvents` (or internal equivalent) — **DEFERRED**: likely interleaved with frame logic in core
- [ ] Move frame-boundary input state reset logic — **DEFERRED**: same reason
- [x] Build both backends

**Verification:**
- [x] `build_situation.bat all` passes
- [ ] No input functions remain in `situation_impl.h` — Most moved; polling/reset may stay in core

---

# Final Phase: Clean Build & Version Bump

**Actions:**
- [ ] Run `build_situation.bat clean && build_situation.bat all`
- [ ] Verify line counts:
  - [ ] `situation_impl_io.h`: ~1,800–2,200 lines
  - [ ] `situation_impl_input.h`: ~1,500–2,000 lines
  - [ ] `situation_impl.h`: reduced to ~24,000–25,000 lines
- [ ] Bump version to 2.4.6
- [ ] Update `doc/UPDATELOG.md`

**Verification:**
- [ ] Clean build from scratch passes (both backends)
- [ ] No IO or input function implementations remain in `situation_impl.h`
- [ ] All subsystems functional (file loading, hot-reload, keyboard, mouse, gamepad)

---

## Constraints

- **Single TU.** Everything is still `#include`d into one translation unit — no linker issues.
- **Include order:** deps → decl → forward → threading → io → input → core functions
- **IO before input.** Input doesn't depend on IO, but IO must come before core (asset loaders use it).
- **No functional changes.** Pure structural refactor.
- **One phase at a time.** Green build between every phase.

---

## Risk Mitigation

- **If IO thread entry has circular deps with threading:** Keep a forward declaration in `situation_impl_forward.h` and include IO after threading.
- **If input callbacks reference renderer state:** They likely reference `sit_gs` (window size, etc.) which is in decl — fine.
- **If `SituationPollInputEvents` is interleaved with frame logic:** It may need to stay in core, with input just providing the helpers it calls.
