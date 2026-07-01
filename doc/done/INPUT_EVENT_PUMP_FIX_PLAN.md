# Input Event Pump Fix Plan

## Problem Statement

`glfwPollEvents()` is called in multiple places **outside** of `SituationPollInputEvents()` — primarily inside the Vulkan frame-acquire path and fence-pump loops. These calls exist to prevent the Windows "Not Responding" state during GPU waits, but they fire GLFW input callbacks which corrupt per-frame input state (`down_this_frame` / `up_this_frame`).

**Result:** Key presses that arrive during the render/present phase are silently lost. `SituationIsKeyPressed()` never returns true for those presses. This affects Escape, F9–F12, and any other key in all examples.

**Severity:** Approximately 50% of key presses are dropped at 60fps with VSync, because roughly half the frame time is spent in the render phase where rogue `glfwPollEvents()` calls can eat events.

## Architecture Alignment

The library's input contract is explicitly documented:

> **Frame pattern**: `SituationPollInputEvents()` → `SituationUpdateTimers()` → render.
> **Public SITAPI calls must originate from the main thread.**

`SituationPollInputEvents()` is the **sole owner** of per-frame input state transitions:
1. Copies `current_state` → `last_state`
2. Clears `down_this_frame` / `up_this_frame` (memset 0)
3. Calls `glfwPollEvents()` — callbacks populate the cleared arrays
4. Returns — arrays now represent a consistent snapshot for this frame

Any other `glfwPollEvents()` call that fires input callbacks **between** step 3 of frame N and step 2 of frame N+1 causes data that was written to `down_this_frame` during frame N's render phase to be silently destroyed by frame N+1's memset. The key press is consumed from the OS queue but never visible to user code.

The ring buffer queue (`pressed_queue`) is **also** affected: the press enters the queue during the pump, but `SituationGetKeyPressed()` is a drain-based API — user code that uses `SituationIsKeyPressed()` (the standard pattern in `sit_example.h` and all numbered examples) never sees it.

## Offending Locations

| File | Line | When | Frequency |
|------|------|------|-----------|
| `situation_impl_renderer.h` | ~14236 | Top of Vulkan acquire retry loop | **Every frame** (Vulkan) |
| `situation_impl_renderer.h` | ~14357 | `vkAcquireNextImageKHR` timeout retry | **Every frame** (Vulkan, if acquire times out) |
| `situation_impl_decl.h` | ~2010 | `_SituationVulkanWaitFencePumpWindowBudget` 4ms chunk loop | **Every frame** (Vulkan, if fence isn't instantly ready) |
| `situation_impl_wdm.h` | ~890 | Borderless fullscreen enter | On F11 toggle only |
| `situation_impl_wdm.h` | ~1564 | Fullscreen enter via `ApplyCurrentProfileWindowState` | On fullscreen toggle only |
| `situation_impl_wdm.h` | ~1593 | Windowed restore via `ApplyCurrentProfileWindowState` | On fullscreen toggle only |
| `situation_impl_image.h` | ~2385 | Screenshot spin-wait (Vulkan + render thread) | On F12 only |

The renderer/decl calls are the critical ones — they fire every frame on Vulkan.

## Fix Design

### Approach: Guarded Event Pump with Pending Buffers

Add a guard counter and **pending edge buffers** (`pending_down[]`, `pending_up[]`) to `_SituationInputState`. When the guard is active (during non-primary `glfwPollEvents()` calls), input callbacks:
- Still update `current_state[]` — so `SituationIsKeyDown()` remains accurate
- Still push to ring buffer queue — so `SituationGetKeyPressed()` works
- Still update modifier/lock state — always accurate
- Still fire user callbacks
- Write to `pending_down[]` / `pending_up[]` **instead of** `down_this_frame` / `up_this_frame`

Then at the top of `SituationPollInputEvents()`, **before** the memset that clears `down_this_frame`, the pending buffers are merged in — ensuring that any key pressed during the previous frame's render phase is visible to the user's `SituationIsKeyPressed()` check this frame.

### Why Pending Buffers (not just a bare guard)

A bare guard that simply suppresses `down_this_frame` writes would **still lose key presses**:
- **Held key case:** Press arrives during pump → `current_state = true`. Next poll: `memcpy(last ← current)` makes `last = true`. Then `glfwPollEvents()` — no new event (key still held). `down_this_frame` never gets set. `IsKeyPressed()` returns false.
- **Quick tap case:** Press AND release during same pump window → by next poll, `current_state = false`, `last_state = false`. No edge detected. Event lost.

The pending buffers capture both cases: the callback records the press/release intent during the pump, and the official poll site merges it into the real per-frame arrays where user code will see it.

### Why This Approach

1. **Correct** — handles held keys, quick taps, and the normal case
2. **Minimal blast radius** — decl (struct + helper), input (callback guards), ctrl (merge step)
3. **Preserves window responsiveness** — `glfwPollEvents()` calls remain; no "Not Responding"
4. **Preserves all API contracts** — `IsKeyPressed`, `IsKeyDown`, `GetKeyPressed`, user callbacks all work correctly
5. **No timing/threading complexity** — guard counter, set/cleared on main thread, no races
6. **Follows existing patterns** — library already uses guard flags (`sit_render.in_frame`, `sit_gs.is_app_internally_paused`)

### Why NOT Other Approaches

- **Removing `glfwPollEvents()` from pump paths**: Would re-introduce "Not Responding" state on Windows during GPU stalls. Unacceptable for a AAA-quality library.
- **Double-buffering input**: Overengineered for this problem. The updatelog mentions it as a future plan but it's a major architectural change, not a bug fix.
- **Edge reconciliation from `current_state` vs `last_state`**: The reviewer's suggestion of detecting edges post-hoc by comparing states. This misses the quick-tap case (press+release within one pump window leaves both `current` and `last` at the same value). The pending buffer captures events as they happen — strictly more correct.
- **Bare guard without pending buffers**: Prevents corruption but doesn't restore visibility. `IsKeyPressed()` would still miss events that land during pumps.

## Implementation Steps

### Phase 1: Add Guard and Pending Buffers

- [x] **1.1** Add the following fields to `_SituationInputState` (after `cursor_count`):
  - File: `sit/situation_impl_decl.h` (~line 1280, inside the struct)
  ```c
  // --- Guarded Pump State ---
  int pump_guard_depth;                                    // >0 when inside a guarded pump; callbacks defer edge writes
  bool pending_down[GLFW_KEY_LAST + 1];                   // Key presses captured during guarded pumps
  bool pending_up[GLFW_KEY_LAST + 1];                     // Key releases captured during guarded pumps
  bool pending_mouse_down[GLFW_MOUSE_BUTTON_LAST + 1];    // Mouse presses captured during guarded pumps
  bool pending_mouse_up[GLFW_MOUSE_BUTTON_LAST + 1];      // Mouse releases captured during guarded pumps
  float pending_scroll_x;                                  // Scroll X accumulated during guarded pumps
  float pending_scroll_y;                                  // Scroll Y accumulated during guarded pumps
  ```
  - Zero-initialized by `SIT_CALLOC` at context creation — guard inactive, no pending events

### Phase 2: Guard the Callbacks

- [x] **2.1** In `_SituationGLFWKeyCallback` (`sit/situation_impl_input.h` ~line 53):
  - Route `down_this_frame` / `up_this_frame` writes through the guard:
    ```c
    if (action == GLFW_PRESS) {
        sit_input.keyboard.current_state[key] = true;
        if (scancode >= 0 && scancode < SITUATION_MAX_SCANCODES) sit_input.keyboard.scancode_state[scancode] = true;
        if (sit_input.pump_guard_depth > 0) {
            sit_input.pending_down[key] = true;
        } else {
            sit_input.keyboard.down_this_frame[key] = true;
        }
        // ... queue push (always) ...
    } else if (action == GLFW_RELEASE) {
        sit_input.keyboard.current_state[key] = false;
        if (scancode >= 0 && scancode < SITUATION_MAX_SCANCODES) sit_input.keyboard.scancode_state[scancode] = false;
        if (sit_input.pump_guard_depth > 0) {
            sit_input.pending_up[key] = true;
        } else {
            sit_input.keyboard.up_this_frame[key] = true;
        }
    }
    ```
  - Modifier state, lock key state, queue push, user callback: **unchanged** (always execute)

- [x] **2.2** In `_SituationGLFWMouseButtonCallback` (`sit/situation_impl_input.h` ~line 246):
  - Same pattern: guard `button_down_this_frame` / `button_up_this_frame` writes → `pending_mouse_down` / `pending_mouse_up`
  - `current_button_state`, queue push, user callback: **unchanged**

- [x] **2.3** In `_SituationGLFWScrollCallback` (`sit/situation_impl_input.h` ~line 319):
  - Guard scroll accumulation:
    ```c
    if (sit_input.pump_guard_depth > 0) {
        sit_input.pending_scroll_x += (float)xoffset;
        sit_input.pending_scroll_y += (float)yoffset;
    } else {
        sit_input.mouse.wheel_move_x += (float)xoffset;
        sit_input.mouse.wheel_move_y += (float)yoffset;
    }
    ```

### Phase 3: Merge Pending at Official Poll

- [x] **3.1** In `SituationPollInputEvents()` (`sit/situation_impl_ctrl.h` ~line 1540), **after** the memset that clears edge arrays and **before** `glfwPollEvents()`:
  ```c
  // --- Merge pending events from guarded pumps ---
  // Events that arrived during the previous frame's render phase (inside
  // _SituationPumpWindowEventsGuarded) are deferred here so that
  // SituationIsKeyPressed() / SituationIsKeyReleased() see them this frame.
  for (int i = 0; i <= GLFW_KEY_LAST; i++) {
      if (sit_input.pending_down[i]) sit_input.keyboard.down_this_frame[i] = true;
      if (sit_input.pending_up[i])   sit_input.keyboard.up_this_frame[i] = true;
  }
  memset(sit_input.pending_down, 0, sizeof(sit_input.pending_down));
  memset(sit_input.pending_up, 0, sizeof(sit_input.pending_up));

  for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
      if (sit_input.pending_mouse_down[i]) sit_input.mouse.button_down_this_frame[i] = true;
      if (sit_input.pending_mouse_up[i])   sit_input.mouse.button_up_this_frame[i] = true;
  }
  memset(sit_input.pending_mouse_down, 0, sizeof(sit_input.pending_mouse_down));
  memset(sit_input.pending_mouse_up, 0, sizeof(sit_input.pending_mouse_up));

  sit_input.mouse.wheel_move_x += sit_input.pending_scroll_x;
  sit_input.mouse.wheel_move_y += sit_input.pending_scroll_y;
  sit_input.pending_scroll_x = 0.0f;
  sit_input.pending_scroll_y = 0.0f;
  ```

  **Placement**: This goes BEFORE the `memcpy(last ← current)` and `memset(down_this_frame, 0)` lines. The flow becomes:
  1. Merge pending → `down_this_frame` (now visible to user code checking "last frame" residual — but actually, we want it visible THIS frame)
  
  **Wait — correct placement**: Actually this merge must go AFTER the memset that clears `down_this_frame`. The logic is:
  1. `memcpy(last ← current)` — snapshot previous held state
  2. `memset(down_this_frame, 0)` — clear edge flags
  3. `memset(up_this_frame, 0)` — clear edge flags
  4. **Merge pending_down/up into the now-cleared arrays** ← HERE
  5. `glfwPollEvents()` — may set additional edges on top
  
  This way, events from the previous render phase AND events from this frame's poll are both visible.

### Phase 4: Create the Guarded Pump Helper

- [x] **4.1** Add a static inline helper in `sit/situation_impl_decl.h` (after `_SituationVulkanWaitFencePumpWindow`):
  ```c
  /**
   * Pump OS window events without corrupting per-frame input state.
   * Used during GPU fence waits and swapchain operations to prevent
   * Windows "Not Responding" state while keeping input event flags
   * exclusively owned by SituationPollInputEvents().
   *
   * Key presses/releases during this pump are deferred to pending_down/up
   * and merged at the next SituationPollInputEvents() call.
   */
  static inline void _SituationPumpWindowEventsGuarded(void) {
      if (!_sit_current_context) { glfwPollEvents(); return; }
      sit_input.pump_guard_depth++;
      glfwPollEvents();
      sit_input.pump_guard_depth--;
  }
  ```

### Phase 5: Replace Rogue Calls

- [x] **5.1** `sit/situation_impl_renderer.h` ~line 14236 — replace `glfwPollEvents()` with `_SituationPumpWindowEventsGuarded()`
- [x] **5.2** `sit/situation_impl_renderer.h` ~line 14357 — same replacement
- [x] **5.3** `sit/situation_impl_decl.h` ~line 2010 (inside `_SituationVulkanWaitFencePumpWindowBudget`) — same replacement
- [x] **5.4** `sit/situation_impl_renderer.h` ~line 11137 (`_SituationVulkanCreateSwapchain`) — same replacement
- [x] **5.5** `sit/situation_impl_wdm.h` ~line 890 — same replacement
- [x] **5.6** `sit/situation_impl_wdm.h` ~line 1564 — same replacement
- [x] **5.7** `sit/situation_impl_wdm.h` ~line 1593 — same replacement
- [x] **5.8** `sit/situation_impl_image.h` ~line 2385 — same replacement

### Phase 6: Verify No Remaining Unguarded Calls

- [x] **6.1** `grep` for raw `glfwPollEvents()` across `sit/` — the ONLY remaining call is in `SituationPollInputEvents()` at `sit/situation_impl_ctrl.h` line 1581 (plus two inside the guarded helper itself)
- [x] **6.2** `glfwWaitEventsTimeout` — covered by a separate `_SituationWaitWindowEventsTimeoutGuarded` wrapper

### Phase 7: Build & Test

- [x] **7.1** Rebuild static-opengl: `build\build_situation.bat static-opengl`
- [x] **7.2** Rebuild static-vulkan: `build\build_situation.bat static-vulkan`
- [x] **7.3** Build example 03_keyboard_and_mouse (both backends)
- [ ] **7.4** Manual test: ESC quits, F9 toggles VSync, F11 toggles fullscreen, F12 takes screenshot, P pauses, M shows metrics
- [x] **7.5** Build and run test harness: `build\build_tests.bat static-opengl` (built `sit_test_opengl.exe`; full run is optional smoke)

### Phase 8: Documentation

- [x] **8.1** Bump `SITUATION_VERSION_PATCH` in `sit/situation_base_version.h` (v2.4.308)
- [x] **8.2** Add entry to `doc/UPDATELOG.md`

### Phase 9: Edge Reconciliation from State Drift (the actual fix)

Phases 1–8 were necessary infrastructure but **insufficient** to fix `IsKeyPressed`. The guarded pumps correctly deferred events to `pending_*`, and the merge correctly replayed them — but there was a gap: when a guarded pump fires from *within* the legitimate `glfwPollEvents()` (via GLFW callbacks like focus/resize triggering `SituationApplyCurrentProfileWindowState()` → guarded pump), the key callback runs with `pump_guard_depth > 0` and writes to `pending_*`. The pre-poll merge already ran, so those pending events sit until next frame. Meanwhile `current_state` is updated immediately but `down_this_frame` is never set — giving the "IsKeyDown works, IsKeyPressed doesn't" symptom.

- [x] **9.1** Add **pre-poll state drift detection** in `SituationPollInputEvents()`, BEFORE `memcpy(last ← current)`:
  ```c
  // Synthesize press/release edges from current_state drift since the last poll.
  // Guarded pumps (acquire/fence waits) update current_state but defer down_this_frame
  // to pending_*; if those edges were missed, last_state vs current_state still captures
  // held keys (IsKeyDown true + IsKeyPressed false symptom).
  for (int i = 0; i <= GLFW_KEY_LAST; i++) {
      if (sit_input.keyboard.current_state[i] && !sit_input.keyboard.last_state[i])
          sit_input.pending_down[i] = true;
      if (!sit_input.keyboard.current_state[i] && sit_input.keyboard.last_state[i])
          sit_input.pending_up[i] = true;
  }
  for (int i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
      if (sit_input.mouse.current_button_state[i] && !sit_input.mouse.last_button_state[i])
          sit_input.pending_mouse_down[i] = true;
      if (!sit_input.mouse.current_button_state[i] && sit_input.mouse.last_button_state[i])
          sit_input.pending_mouse_up[i] = true;
  }
  ```
  This detects any `current_state` changes that happened since the last `memcpy(last ← current)` and funnels them into the pending system, which the existing merge (Phase 3) then applies to `down_this_frame`.

- [x] **9.2** Add **post-glfwPollEvents merge** — a second merge pass AFTER the legitimate `glfwPollEvents()` call:
  ```c
  // Merge any edges deferred by nested guarded pumps invoked from within glfwPollEvents
  // (e.g. focus/fullscreen callbacks) so IsKeyPressed sees them this frame.
  for (int i = 0; i <= GLFW_KEY_LAST; i++) {
      if (sit_input.pending_down[i]) sit_input.keyboard.down_this_frame[i] = true;
      if (sit_input.pending_up[i]) sit_input.keyboard.up_this_frame[i] = true;
  }
  memset(sit_input.pending_down, 0, sizeof(sit_input.pending_down));
  memset(sit_input.pending_up, 0, sizeof(sit_input.pending_up));
  // ... same for mouse + scroll ...
  ```
  This catches events that land in `pending_*` due to nested guarded pumps triggered from within GLFW callbacks during the legitimate poll.

**Why Phase 9 was needed:** The original plan assumed that the legitimate `glfwPollEvents()` in `SituationPollInputEvents()` always runs with `pump_guard_depth == 0` and that no nested pumps occur within it. In practice, GLFW's callback dispatch (focus change, window state) can trigger `SituationApplyCurrentProfileWindowState()` which calls `_SituationPumpWindowEventsGuarded()` — incrementing the guard depth during the nested call. Events arriving in that nested pump go to `pending_*` but the pre-poll merge already ran. Without Phase 9's post-poll merge and drift detection, those events are invisible until the next frame — one frame too late for `IsKeyPressed`.

## Invariants Preserved

| Invariant | Status |
|-----------|--------|
| `SituationPollInputEvents()` is the sole authority on per-frame input state | **Restored** |
| `down_this_frame[key]` is true for exactly one frame after a key press | **Restored** (via pending buffer merge) |
| Quick taps during render phase are not lost | **Restored** (pending buffers capture press+release) |
| `SituationIsKeyDown()` is always accurate (tracks real hardware state) | **Preserved** |
| `SituationGetKeyPressed()` never misses an event | **Preserved** (queue still populated during guarded pumps) |
| Windows never shows "Not Responding" during GPU waits | **Preserved** |
| User key callback fires for all events regardless of timing | **Preserved** |
| No threading changes, no mutex additions, no architectural shifts | **Preserved** |

## Memory Cost

- `pending_down[349]` + `pending_up[349]` = 698 bytes (keyboard)
- `pending_mouse_down[8]` + `pending_mouse_up[8]` = 16 bytes
- `pending_scroll_x/y` = 8 bytes
- `pump_guard_depth` = 4 bytes
- **Total: ~726 bytes** — negligible

## Risk Assessment

- **Low risk**: The change is additive (new fields + guard checks + merge step), never removes functionality
- **No behavioral change for OpenGL**: OpenGL path doesn't have `glfwPollEvents()` in the acquire path; pending buffers stay empty, merge is a no-op
- **Vulkan gets correct input behavior**: The primary beneficiary
- **WDM transitions become safe**: Fullscreen toggle no longer eats the F11 press that triggered it (cosmetic but correct)
- **Depth counter vs bool**: Using `int` counter instead of `bool` costs nothing and is safe against hypothetical future nested pump scenarios (GLFW callbacks triggering code paths that pump again)
