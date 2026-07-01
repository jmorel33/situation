# Error Code Gap-Fill Plan — Shutdown, Display Transition & Context Recovery

**Date**: 2026-06-21  
**Status**: ALL PHASES COMPLETE — verified against source 2026-06-21  
**Priority**: HIGH — exposed by v2.4.320 (monitor_hot_swap_recreate pass uncovers lifecycle hang)  
**Depends On**: ERROR_PROPAGATION_PLAN (complete), ERROR_XMACRO_UNIFICATION_PLAN (complete)  
**Scope**: Add missing error codes for display mode transitions, shutdown lifecycle, GL context management, and multi-init resilience. Fix the shutdown fullscreen release bug.

---

## The Problem

v2.4.320 fixed the canvas FBO failure on display mode transitions, but the fix exposed a deeper lifecycle bug: after `SituationShutdown` following multi-monitor exclusive fullscreen on OpenGL, the next `SituationInit` → `glfwCreateWindow` hangs indefinitely (NVIDIA Windows, GTX 1070, dual 2560×1440).

**Root cause hypothesis**: `_SituationCleanupPlatform()` calls `glfwDestroyWindow()` without first restoring the window to windowed mode. The NVIDIA OpenGL ICD retains exclusive display-mode ownership from the destroyed fullscreen window, blocking new context creation on subsequent `glfwCreateWindow`. Vulkan is unaffected because `VK_EXT_full_screen_exclusive` has explicit release semantics.

**Audit of the shutdown/transition paths reveals 6 error conditions with no dedicated code**, plus 3 failure scenarios that silently succeed but leave the system in a degraded state that cascades into the next init cycle.

---

## Design Principles (inherited from ERROR_PROPAGATION_PLAN)

1. Every failure path in a public API or critical internal function MUST call `_SituationSetErrorFromCode(CODE, "detail")`.
2. Prefer specific codes over generic ones — a caller should know *what* failed from the code alone.
3. Void cleanup functions remain void (HARDENING: best-effort teardown) but log at `SIT_LOG_WARNING` and set error state for post-mortem.
4. New codes use gaps in their section range; values are permanent (never renumber).
5. Each phase passes a build gate before proceeding.

---

## New Error Codes

| Code Name | Value | Section | Message |
|-----------|-------|---------|---------|
| `SITUATION_ERROR_FULLSCREEN_RELEASE_FAILED` | `-108` | Platform | `"Failed to restore windowed mode before window destruction"` |
| `SITUATION_ERROR_CONTEXT_RECLAIM_FAILED` | `-109` | Platform | `"Failed to reclaim GL context on main thread after render thread shutdown"` |
| `SITUATION_ERROR_DISPLAY_MODE_SETTLING` | `-215` | Display | `"Display mode transition still in progress (frame rendered at native resolution)"` |
| `SITUATION_ERROR_SHUTDOWN_INCOMPLETE` | `-13` | Core | `"Shutdown completed with one or more subsystem teardown failures"` |
| `SITUATION_ERROR_INIT_STALE_DRIVER_STATE` | `-14` | Core | `"Init detected stale driver state from a previous session (possible display mode ghost)"` |
| `SITUATION_ERROR_AUDIO_DEVICE_TRANSITION_STALE` | `-431` | Audio | `"Audio device in stale state from previous session exclusive-mode usage"` |

**Range justification**:
- `-108`, `-109`: Platform range `-100 to -199`, slots `-108` and `-109` are free.
- `-215`: Display range `-200 to -299`, next after `-214`.
- `-13`, `-14`: Core range `0 to -99`, slots after `-12` (before `-20` timer).
- `-431`: Audio range `-400 to -439`, slot after `-430`.

---

## Phased Implementation

### Phase 0: Add Error Codes to `situation_base_errno.h`

Add all 6 new codes. Rebuild to confirm compile.

- [x] Add `SITUATION_ERROR_SHUTDOWN_INCOMPLETE` (-13) to `SITUATION_ERRORS_CORE`
- [x] Add `SITUATION_ERROR_INIT_STALE_DRIVER_STATE` (-14) to `SITUATION_ERRORS_CORE`
- [x] Add `SITUATION_ERROR_FULLSCREEN_RELEASE_FAILED` (-108) to `SITUATION_ERRORS_PLATFORM`
- [x] Add `SITUATION_ERROR_CONTEXT_RECLAIM_FAILED` (-109) to `SITUATION_ERRORS_PLATFORM`
- [x] Add `SITUATION_ERROR_DISPLAY_MODE_SETTLING` (-215) to `SITUATION_ERRORS_DISPLAY`
- [x] Add `SITUATION_ERROR_AUDIO_DEVICE_TRANSITION_STALE` (-431) to `SITUATION_ERRORS_AUDIO`
- [x] Add string entries to `SituationGetErrorString()` in `situation_impl_ctrl.h` (auto from X-macro — verify it picks them up)
- [x] **GATE**: `build\build_situation.bat static-opengl` ✓

---

### Phase 1: Shutdown Fullscreen Release Fix (`situation_impl_ctrl.h`)

The core bug fix. Before destroying the window, force it back to windowed mode if it's currently in exclusive fullscreen. This prevents the NVIDIA driver from retaining display-mode state.

**Implementation** (in `_SituationCleanupPlatform`, before `glfwDestroyWindow`):

```c
// [v2.4.322] Force window out of exclusive fullscreen before destruction.
// NVIDIA's OpenGL ICD retains display-mode state from a destroyed fullscreen window,
// blocking new context creation on subsequent glfwCreateWindow in the same process.
if (sit_gs.sit_glfw_window && glfwGetWindowMonitor(sit_gs.sit_glfw_window) != NULL) {
    glfwSetWindowMonitor(sit_gs.sit_glfw_window, NULL,
                         sit_gs.windowed_x, sit_gs.windowed_y,
                         sit_gs.windowed_w > 0 ? sit_gs.windowed_w : 800,
                         sit_gs.windowed_h > 0 ? sit_gs.windowed_h : 600, 0);
    // Pump events to let the mode change settle before destruction.
    glfwPollEvents();
    // If mode change didn't take (still fullscreen), log but proceed with destroy.
    if (glfwGetWindowMonitor(sit_gs.sit_glfw_window) != NULL) {
        _SituationSetErrorFromCode(SITUATION_ERROR_FULLSCREEN_RELEASE_FAILED,
            "glfwSetWindowMonitor to windowed did not take effect before shutdown");
    }
}
```

**Also** in `SituationShutdown`, after the render thread is destroyed and before `_SituationCleanupRenderer()`:

```c
// [v2.4.322] Reclaim GL context on main thread after render thread shutdown.
#if defined(SITUATION_USE_OPENGL) && defined(SITUATION_ENABLE_RENDER_THREAD)
if (sit_gs.sit_glfw_window) {
    glfwMakeContextCurrent(sit_gs.sit_glfw_window);
    if (glfwGetCurrentContext() != sit_gs.sit_glfw_window) {
        _SituationSetErrorFromCode(SITUATION_ERROR_CONTEXT_RECLAIM_FAILED,
            "GL context not reclaimed on main thread after render thread join");
    }
}
#endif
```

- [x] Add fullscreen release logic to `_SituationCleanupPlatform()`
- [x] Use `SITUATION_ERROR_FULLSCREEN_RELEASE_FAILED` on failure
- [x] Verify GL context reclaim uses `SITUATION_ERROR_CONTEXT_RECLAIM_FAILED` (existing code at line ~1944 already does this but without the new code — confirm)
- [x] **GATE**: `build\build_situation.bat static-opengl` ✓, `static-vulkan` ✓

---

### Phase 2: Display Mode Settling — Non-Fatal Code (`situation_impl_renderer.h`)

The Phase A3 fail-soft fallback (FBO 0 rendering when canvas creation fails during mode change) should set the non-fatal `SITUATION_ERROR_DISPLAY_MODE_SETTLING` code so callers can detect it via `SituationGetLastErrorCode()` if they want to (e.g., skip screenshots during settling).

**Implementation** (in `SIT_OP_BEGIN_RENDER_PASS` executor, the fail-soft path):

```c
// Already falls back to FBO 0 — additionally set a non-fatal diagnostic code.
_SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_MODE_SETTLING,
    "_SituationGLEnsureCanvasResources failed; rendering to default FBO this frame");
```

This code is informational — the frame renders correctly at native resolution. Callers checking `SituationGetLastErrorCode()` after `SituationEndFrame()` will see it, but it clears on the next successful frame.

- [x] Set `SITUATION_ERROR_DISPLAY_MODE_SETTLING` in the fail-soft FBO 0 path
- [x] Verify the code is cleared on the next successful `_SituationGLEnsureCanvasResources()`
- [x] **GATE**: `build\build_situation.bat static-opengl` ✓

---

### Phase 3: Shutdown Completeness Tracking (`situation_impl_ctrl.h`)

`SituationShutdown` is currently void and best-effort. If any subsystem teardown fails, the next init cycle may encounter stale state. Track whether shutdown was clean:

**Implementation** (in `SituationShutdown`):

```c
bool shutdown_fully_clean = true;

// ... existing cleanup calls ...
// After each major step, check for error state:
if (render_thread_err != SITUATION_SUCCESS) shutdown_fully_clean = false;

// At the end:
if (!shutdown_fully_clean) {
    _SituationSetErrorFromCode(SITUATION_ERROR_SHUTDOWN_INCOMPLETE,
        "One or more subsystems reported errors during teardown");
}
```

- [x] Add `shutdown_fully_clean` tracking to `SituationShutdown`
- [x] Set `SITUATION_ERROR_SHUTDOWN_INCOMPLETE` if any teardown step fails
- [x] **GATE**: `build\build_situation.bat static-opengl` ✓

---

### Phase 4: Init Stale-State Detection (`situation_impl_ctrl.h`)

If the previous shutdown left stale driver state (detected heuristically), set `SITUATION_ERROR_INIT_STALE_DRIVER_STATE` as a non-fatal warning during init so the caller knows recovery was needed.

**Heuristic**: If `SituationInit` succeeds but the previous `SituationShutdown` left `SITUATION_ERROR_SHUTDOWN_INCOMPLETE` or `SITUATION_ERROR_FULLSCREEN_RELEASE_FAILED`, the new session should note this.

**Implementation**: Use a process-lifetime static flag:

```c
static bool _sit_previous_shutdown_had_errors = false;

// In SituationShutdown, at the end:
if (!shutdown_fully_clean) _sit_previous_shutdown_had_errors = true;

// In SituationInit, after successful init:
if (_sit_previous_shutdown_had_errors) {
    _SituationSetErrorFromCode(SITUATION_ERROR_INIT_STALE_DRIVER_STATE,
        "Previous session shutdown was incomplete; driver state may be stale");
    _sit_previous_shutdown_had_errors = false; // Consumed
}
```

- [x] Add static flag `_sit_previous_shutdown_had_errors`
- [x] Set flag in `SituationShutdown` when incomplete
- [x] Check and consume in `SituationInit` success path
- [x] **GATE**: `build\build_situation.bat static-opengl` ✓

---

### Phase 5: Audio Device Stale-State Warning (`situation_impl_audio.h`)

When `SituationSetAudioDevice()` is called with exclusive mode and the previous session also used exclusive, WASAPI can be in a bad state. Detect and report.

**Implementation** (in `_SituationSetAudioDeviceInternal`):

```c
// After ma_device_init fails with exclusive share mode:
if (result != MA_SUCCESS && playback_share_mode == ma_share_mode_exclusive) {
    _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_DEVICE_TRANSITION_STALE,
        "Exclusive audio device init failed; previous session may not have released device cleanly");
    // Fall through to retry with shared mode (existing behavior)
}
```

- [x] Add stale-state detection in exclusive-mode failure path
- [x] Use `SITUATION_ERROR_AUDIO_DEVICE_TRANSITION_STALE`
- [x] **GATE**: `build\build_situation.bat static-opengl` ✓

---

### Phase 6: Update Verification Script & Bump Version

- [x] Run `scripts\audit_errno.ps1` — confirm 0 phantoms, 0 unreferenced (EOL excluded)
- [x] Run `scripts\verify_impl_forward.py` — confirm OK (any new statics forward-declared)
- [x] Bump patch to v2.4.322 in `sit/situation_base_version.h`
- [x] Add UPDATELOG entry
- [x] **GATE**: `build\build_situation.bat static-opengl` ✓, `static-vulkan` ✓

---

## Test Strategy

### Automated (test harness)

The existing `output_color_depth.monitor_hot_swap_recreate` test exercises the exact path that triggers the hang. After Phase 1, this test should no longer hang when run in a loop:

```
build\tests\sit_test_opengl.exe --module output_color_depth --filter hot_swap --repeat 3
```

### Manual Verification

1. Run the test harness with `--module output_color_depth` and then immediately run again (same process wouldn't apply, but same GPU driver session does). Verify no hang.
2. Run `threading_visual_proof.exe` (uses SituationInit/Shutdown in a loop) — verify no hang after fullscreen toggle.

### Regression

All existing tests must continue to pass:
```
build\run_tests.bat opengl
build\run_tests.bat vulkan
```

---

## Risk Assessment

| Phase | Risk | Mitigation |
|-------|------|------------|
| Phase 1 (fullscreen release) | `glfwSetWindowMonitor` to windowed might itself hang on wedged driver | Added glfwPollEvents pump + check; if still fullscreen, log and proceed with destroy (best-effort) |
| Phase 2 (settling code) | Non-fatal code might confuse callers who check `GetLastErrorCode` every frame | Document as informational; clears on next successful frame |
| Phase 3 (shutdown tracking) | Adding error checks to best-effort void functions could mask the real first error | Only set SHUTDOWN_INCOMPLETE, don't overwrite any existing error state |
| Phase 4 (stale detection) | False positive if shutdown was clean but driver is just slow | Only fires when previous shutdown actually set the incomplete flag |
| Phase 5 (audio stale) | Exclusive mode retry-to-shared might not be desirable for pro-audio users | Only warns; the existing fallback behavior is preserved |

---

## Dependencies Between Phases

```
Phase 0 (codes)  →  Phase 1 (fix)  →  Phase 3 (tracking)  →  Phase 4 (detection)
                 →  Phase 2 (settling)
                 →  Phase 5 (audio)
Phase 6 runs last after all others.
```

Phases 2, 3, 4, 5 are independent of each other (only depend on Phase 0).
Phase 4 depends on Phase 3 (reads the flag Phase 3 sets).

---

## Files Touched (estimated)

| File | Phases |
|------|--------|
| `sit/situation_base_errno.h` | 0 |
| `sit/situation_impl_ctrl.h` | 1, 3, 4 |
| `sit/situation_impl_renderer.h` | 2 |
| `sit/situation_impl_audio.h` | 5 |
| `sit/situation_base_version.h` | 6 |
| `doc/UPDATELOG.md` | 6 |
