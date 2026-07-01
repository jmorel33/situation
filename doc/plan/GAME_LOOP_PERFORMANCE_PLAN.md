# Game Loop Performance Plan — VSync-Sharp Frame Cadence

| Field | Value |
|-------|--------|
| **Status** | 🟢 **Shipped @ v2.4.385** — GAME_LOOP_PERFORMANCE_PLAN complete; Gate 5 harness green (1 pre-existing audio fail) |
| **Goal** | Rock-solid monitor-paced frames: **no regression**, **true 60 Hz lock**, **canonical event sequencing**, **zero hot-path waste** |
| **Trigger** | VSync ON reports **59 FPS** on 60 Hz panels; visible micro-stutters despite **~6× GPU headroom**; phase metrics show stalls in library present/sync path, not user draws |
| **Primary files** | `sit/situation_impl_renderer_frame_cmd.h`, `sit/situation_impl_renderer_lc.h`, `sit/situation_impl_ctrl.h`, `sit/situation_impl_wdm.h`, `sit/situation_impl_image.h`, `sit/situation_impl_decl.h`, `examples/shared/sit_example.h` |
| **Pairs with** | [`AAA_ARCHITECTURE_PLAN.md`](AAA_ARCHITECTURE_PLAN.md) §6 (profiling), [`LIBRARY_RECOVERY_PLAN_244.md`](LIBRARY_RECOVERY_PLAN_244.md) Track D (GL readback), [`renderer_bolster_plan.md`](renderer_bolster_plan.md) (single presentation contract), [`COMPATIBILITY_CHECK_PLAN.md`](COMPATIBILITY_CHECK_PLAN.md) |
| **Explicit non-goal** | Replacing GLFW/DWM; shipping a second frame API; weakening opcode-level GL error checking (Track D drains stay); mass harness migration to `SituationRequestScreenCapture()` |

---

## Executive summary

The library's **canonical loop order is correct** (`PollInputEvents` → `UpdateTimers` → user logic → `AcquireFrameCommandBuffer` → record → `EndFrame`). Performance problems are **not** from draw cost — they come from:

1. **Wasteful per-frame GL readback** on the render thread (`glFinish` + full-frame `glReadPixels`) since v2.4.362 — unconditional, unlike Vulkan.
2. **FPS / `frame_time` measured on the main thread** at acquire/end, decoupled from **present completion** on the render thread.
3. **Deep pipelining** (`MAX_FRAMES_IN_FLIGHT = 6`) under VSync — adds queue latency and backpressure wakes without improving displayed frame rate.
4. **Integer-truncated FPS** over `glfwGetTime()` windows — systematically shows **59** on 59.94 Hz panels and on any 60-in-1.01s window.

**Phase 1 strategy (revised):** restore **Vulkan-parity on-demand capture** on GL without breaking the implicit `EndFrame` → `LoadImageFromScreen` contract. **Render-thread (RT) and single-threaded (ST) paths differ** (see §RT vs ST): RT uses an **urgent latch** before pre-swap; ST uses **sync on-demand read in `LoadImageFromScreen`** after present. Both share **PBO async readback** when pre-swap capture runs (wire existing `screenshot_pbo` in `situation_impl_decl.h`). Game loops and examples pay **zero readback cost** on frames with no `Request`, no `Load`, and no `TakeScreenshot`.

This plan fixes those in ordered phases with **hard regression gates** before each merge.

---

## Design principles (non-negotiable)

| # | Principle |
|---|-----------|
| **P1 — No regression** | Full harness GL + VK green; graphics + virtual_display + transfer readback modules unchanged or stricter; wrapper `hello_situation` smoke (C/Lua/Rust) passes on fresh DLL. |
| **P2 — Solid 60** | With VSync ON on a 60 Hz (or 59.94 Hz) panel: displayed cadence matches refresh; FPS API reports refresh-accurate value; `frame_time` median ≈ `1/refresh` ± 1 ms jitter budget. |
| **P3 — Proper sequencing** | One authoritative loop contract; timing and simulation deltas derived from **display pace**, not main-thread spin rate. |
| **P4 — No waste** | Hot path = record → enqueue → present only. No unconditional full-frame readback, no `glFinish` on the steady path, no redundant `glfwSwapInterval`, no `cnd_wait` where VSync already paces. Capture cost is **on-demand only**. |

---

## GL screen capture — preferred design (Phase 1)

Vulkan already captures only when `screenshot_requested`. GL must match that **without** repeating the v2.4.362 failure mode (`LIBRARY_RECOVERY_PLAN_244` §Failed approaches: *request-only gating without `EndFrame`→`Load` parity*).

### Capture decision (pre-swap, when applicable)

Pre-swap capture in `EndFrame` / render thread **iff**:

```text
screenshot_requested[slot]  ||  screenshot_urgent[slot]   // urgent: RT only
```

| Signal | Set by | When | Paths |
|--------|--------|------|-------|
| **`screenshot_requested`** | `SituationRequestScreenCapture()` | Before `EndFrame` | RT + ST (explicit fast path; same as Vulkan) |
| **`screenshot_urgent`** | `SituationLoadImageFromScreen()` | Cache miss after `EndFrame` | **RT only** — targets frame index just enqueued; RT checks before swap |

**ST note:** when `render_thread_count == 0`, `EndFrame` is synchronous (present completes before return). Urgent latch cannot apply post-`EndFrame`; implicit `EndFrame` → `Load` uses on-demand sync read inside `LoadImageFromScreen` instead (§RT vs ST).

### RT vs ST capture semantics

Both paths share the same user-facing contract and the same perf goal (**no readback unless someone needs pixels**). Implementation differs because RT presents asynchronously after `EndFrame` returns; ST presents inside `EndFrame` on the main thread.

#### Shared (RT + ST)

| Item | Behavior |
|------|----------|
| Hot game loop | No `Request`, no `Load`, no `TakeScreenshot` → **zero capture** |
| Explicit path | `RequestScreenCapture` → draw → `EndFrame` → `LoadImageFromScreen` (pre-swap capture when `requested`) |
| Pre-swap capture site | RT: `_SituationRenderThreadEntry`; ST: `SituationEndFrame` GL branches |
| PBO | When pre-swap capture runs, use PBO async read — not `glFinish` every frame |
| Track D | Execute-entry GL error drain **always**; readback drain **only inside capture** |

#### Render thread ON (RT) — default in examples

```text
Main                          Render thread
────                          ─────────────
EndFrame()  ──enqueue N──►    execute → [capture if req∨urgent] → swap
returns immediately
LoadImageFromScreen()
  cache miss → urgent[N]
  spin → memcpy screenshot_buffer
```

- **Implicit `EndFrame` → `Load`:** `LoadImageFromScreen` sets **`screenshot_urgent[N]`** for the frame just enqueued. Works when main calls `Load` while RT is still processing N (typical harness pattern).
- **Urgent window missed** (main stalled, deep queue): timeout or clear error; debug warn once. Mitigation: call `Load` promptly after `EndFrame`, or `RequestScreenCapture` before `EndFrame`.

#### Render thread OFF (ST) — `render_thread_count == 0`

```text
Main (single thread)
────
EndFrame()  →  execute → [capture if requested] → swap → return
LoadImageFromScreen()
  cache hit  → memcpy screenshot_buffer
  cache miss → sync on-demand readback on main (situation_impl_image.h fallback)
```

- **`EndFrame` is fully synchronous.** When it returns, present is done; **urgent latch does not apply** (too late to capture pre-swap for that frame).
- **Implicit `EndFrame` → `Load`:** if `EndFrame` did not capture (no `Request`), **`LoadImageFromScreen` performs on-demand sync readback** on main (front/back fallback, DWM-aware — existing code path). Same perf win: readback only when `Load` / `TakeScreenshot` / explicit `Request` runs.
- **Explicit on ST:** `RequestScreenCapture` before `EndFrame` → pre-swap capture inside `EndFrame` → `Load` hits cache (preferred over post-present sync read).

#### Side-by-side

| | **RT ON** | **ST OFF** |
|---|-----------|------------|
| Present | RT, async after `EndFrame` returns | Main, inside `EndFrame` |
| Implicit `EndFrame` → `Load` | **Urgent latch** on frame N | **Sync read in `LoadImageFromScreen`** |
| `screenshot_urgent` | Yes | **No** (not used) |
| Phase 2 `frame_time` | Present on RT → atomics (§2A) | Present at `EndFrame` on main (§2.3) |

**SDK one-liner (Phase 1.14):** *OpenGL with a render thread: call `LoadImageFromScreen` promptly after `EndFrame`, or use `RequestScreenCapture` before `EndFrame`. Without a render thread, `LoadImageFromScreen` after `EndFrame` captures on demand when the cache is empty.*

**No harness migration required** on either path.

### When capture runs — PBO, not `glFinish`

Wire the existing `sit_render.gl.screenshot_pbo` field (declared, never used):

```text
pre-swap:   glReadPixels(... → PBO)     // async; no glFinish on present path
            glfwSwapBuffers()
post-swap:  map PBO / fence wait → screenshot_buffer; set screenshot_valid
```

Remove sync `glFinish` + direct `glReadPixels` → CPU from the hot path. Keep post-readback GL error drain **inside** the capture block only (v2.4.367 / Track D).

### Caller paths (no harness churn)

| Caller | RT ON | ST OFF |
|--------|-------|--------|
| **Game / example loop** (no readback) | Zero capture | Zero capture |
| **`EndFrame` → `LoadImageFromScreen`** | Urgent latch → pre-swap capture | Sync on-demand read in `Load` |
| **`RequestScreenCapture` → `EndFrame` → `Load`** | Pre-swap capture → cache hit | Pre-swap capture → cache hit |
| **`SituationTakeScreenshot` / F12** | Cache or one-shot sync capture | Cache or one-shot sync capture |
| **`test_gl_endframe_execute_vd_streak`** | Execute-entry drains (RT) | N/A (RT test) |

### Fallbacks

| Path | On cache miss / failure |
|------|-------------------------|
| **RT** | Urgent latch first; if window missed → timeout + clear error (debug warn → use `RequestScreenCapture` or call `Load` sooner) |
| **ST** | Sync read in `LoadImageFromScreen` (front/back, DWM-aware — primary ST mechanism, not a rare fallback) |

### Optional init policy (escape hatch only)

If urgent-latch timing proves flaky on a specific platform, add `SituationInitInfo.screen_capture_policy`:

- **`SIT_SCREEN_CAPTURE_ON_DEMAND`** (default) — RT: requested + urgent only; ST: requested pre-swap + sync-on-Load
- **`SIT_SCREEN_CAPTURE_ALWAYS`** — v2.4.362 behavior for harness bisect / emergency revert

Do **not** default to ALWAYS; use only for bisect or temporary revert.

### GL paths in scope

Apply the same capture decision to **all** GL pre-swap sites:

- Render thread: `_SituationRenderThreadEntry` (`renderer_lc.h`)
- Single-threaded / main-present: both OpenGL branches in `SituationEndFrame` (`renderer_frame_cmd.h`)

---

## Canonical frame loop contract

### Required application order (unchanged — enforce in docs + debug guard)

```text
while (running) {
    SituationPollInputEvents();          // 1 — OS events, cached window flags
    SituationUpdateTimers();             // 2 — dt, oscillators, VD clocks, hot-reload tick
    /* user: physics, audio triggers, AI */

    SituationAcquireFrameCommandBuffer(); // 3 — wait for free slot / fence; begin frame
    /* user: SituationCmd* recording */
    SituationEndFrame();                 // 4 — enqueue (RT) or present (ST)
}
```

### Thread ownership

#### Render thread ON (default in examples)

| Step | Thread | Blocks on |
|------|--------|-----------|
| Poll / Update | Main | Never on GPU |
| Acquire | Main | `frames_pending`, prior-slot fence (VK), ring backpressure |
| Record | Main | Never on present |
| EndFrame enqueue | Main | Queue depth when paced |
| Execute + present + capture | Render | Prior-slot fence (GL), `glfwSwapBuffers` / `vkQueuePresentKHR` (VSync) |

#### Render thread OFF (`render_thread_count == 0`)

| Step | Thread | Blocks on |
|------|--------|-----------|
| Poll / Update | Main | Never on GPU |
| Acquire | Main | Prior-slot fence (VK), ring backpressure when paced |
| Record | Main | — |
| EndFrame (execute + capture + present) | Main | GPU fence (GL), swap (VSync) |
| `LoadImageFromScreen` (cache miss) | Main | Sync readback on demand |

### What "one frame" means after this plan

| Metric | Source (target) |
|--------|-----------------|
| **`frame_time` / `SituationGetFrameTime()`** | Interval between **successive present completions** (display pace), copied to main via atomic handoff |
| **`SituationGetFPS()`** | Frames **presented** in the last 1.0 s window, using `_SitGetMonotonicTimeNS()` (not `glfwGetTime`) |
| **Spike / phase metrics** | Same present-anchored boundary; phases attributed to the frame whose present completed |

---

## Root cause map (verified @ v2.4.383)

| ID | Symptom | Root cause | File / symbol |
|----|---------|------------|---------------|
| **R1** | 59 FPS HUD | `(int)(count/elapsed)` + 1.0 s `glfwGetTime` window + 59.94 Hz panels | `SituationEndFrame` FPS block |
| **R2** | Stutter despite low `ex=` | Unconditional `_SituationGLCaptureDisplayedFramebuffer` → `glFinish` + `glReadPixels` every RT frame | `renderer_lc.h` ~10469 |
| **R3** | `frame_time` ≠ felt pace | `frame_time` at `AcquireFrameCommandBuffer` on main; present async on RT | `renderer_frame_cmd.h` ~22–32 |
| **R4** | Occasional `bp=` spikes | 6-slot pipeline + `cnd_wait` under SLEEP policy after latency spike | `renderer_frame_cmd.h` EndFrame backpressure |
| **R5** | False spike count | Threshold 25 ms still tight for some DWM configs (diagnostics only — not a pacing root cause) | `situation_impl_ctrl.h` UpdateTimers |
| **R6** | Readback regression risk | v2.4.362 made capture unconditional to preserve `EndFrame` → `LoadImageFromScreen`; naive request-only gating breaks that | `LIBRARY_RECOVERY_PLAN_244` §Failed approaches |

---

## Phase 0 — Baseline & gates (no behavior change)

Establish measurable before/after. **Exit: artifacts committed under `build/tests/results/`.**

- [x] **0.1** Add harness test `frame_pacing_vsync_baseline` in `tests/harness/test_frame_pacing.c`:
  - Windowed 1280×720, VSync ON, `target_frame_time = 0`, render thread ON (GL + VK builds). Optional second run with `render_thread_count = 0` (GL ST) for comparison — not a gate blocker.
  - Run 600 frames (~10 s) after 30-frame warmup.
  - Record: median/`p95`/`max` `frame_time`, `SituationGetFPS()`, `GetLastFramePhases`, spike count.
  - Writes `build/tests/results/frame_pacing_baseline_<backend>.json`.
  - **Soft assert @ draft**: median `frame_time` ∈ [15.5 ms, 17.5 ms]; `p95` < 22 ms; phases `pr` (present) not dominant when `ex` < 4 ms — warn-only on non-60 Hz panels.
- [x] **0.2** Capture overlay histogram via `SituationExportRenderHistogram` → `build/tests/results/frame_pacing_histogram_<backend>.txt` (same test run).
- [x] **0.3** Baseline JSON records `MAX_FRAMES_IN_FLIGHT` (6) and `SituationGetMonitorRefreshRate(0)`.

**Baseline captured @ v2.4.383 (OpenGL RT, this machine):** refresh 59 Hz; median 16.70 ms; p95 20.10 ms; FPS 60; spikes 4; present 41.6 µs last frame.

**Gate 0:** Full suite `run_tests.bat opengl` + `vulkan` green (existing).

---

## Phase 1 — On-demand GL capture (highest ROI)

**Target: R2, R6.** Vulkan parity + RT urgent latch + ST sync-on-Load + PBO. **No mass harness migration.**

### 1A — Capture gating (Vulkan parity)

- [x] **1.1** Add per-slot atomics: `screenshot_urgent[SITUATION_MAX_FRAMES_IN_FLIGHT]` (or packed flags with `screenshot_requested`).
- [x] **1.2** Render thread (`_SituationRenderThreadEntry`, GL path): capture only when `requested \|\| urgent` for `frame_index`; clear both after capture.
- [x] **1.3** Single-threaded GL `SituationEndFrame` paths (`renderer_frame_cmd.h`): same gate — remove unconditional pre-swap readback blocks.
- [x] **1.4** Keep post-readback GL error drain inside the capture block; keep execute-entry drain always (Track D — independent of capture).

### 1B — Implicit `EndFrame` → `Load` (RT + ST)

**RT — urgent latch**

- [x] **1.5** In `SituationLoadImageFromScreen` (GL, RT only): on cache miss, set `screenshot_urgent` for the last submitted frame index (present may still be in flight).
- [x] **1.6** Extend existing RT spin-wait loop until urgent capture completes (same timeout budget as today).
- [x] **1.7** RT fallback: if urgent window missed (timeout), return clear error; debug warn once (suggest `RequestScreenCapture` before `EndFrame` or call `Load` sooner).

**ST — sync on-demand read in `Load`**

- [x] **1.8** In `SituationLoadImageFromScreen` (GL, ST only): on cache miss after gated `EndFrame`, use existing main-thread sync read path (`glFinish` + read — only when `Load` is called, not every frame). Do **not** set `screenshot_urgent`.
- [x] **1.9** Ensure ST pre-swap capture in `EndFrame` runs only when `screenshot_requested` (explicit path → cache hit on `Load`). **Note:** harness runs ST (`render_thread_count=0`); added pre-swap sync fallback at ST `EndFrame` when not requested so implicit `EndFrame`→`Load` hits epoch-valid cache (DWM-safe). RT game loops still pay zero capture when no `Load`/`Request`.

**Tests**

- [x] **1.10** Add `gl_load_urgent_after_endframe`: GL **RT ON**, `EndFrame` → immediate `LoadImageFromScreen` without `RequestScreenCapture`.
- [x] **1.11** Add `gl_load_after_endframe_st`: GL **`render_thread_count = 0`**, same implicit pattern — validates ST sync-on-Load path.
- [x] **1.12** Confirm `test_gl_endframe_execute_vd_streak` stays green (Track D / consecutive `EndFrame` — no `-600` on render thread).

### 1C — PBO async readback (pre-swap capture — RT + ST)

- [x] **1.13** Init/resize/destroy `screenshot_pbo` alongside `screenshot_buffer` in renderer init/teardown.
- [x] **1.14** Replace `_SituationGLCaptureDisplayedFramebuffer` sync path with PBO pipeline (pre-swap read to PBO, post-swap/fence map to CPU buffer). Used by RT pre-swap capture and ST `EndFrame` when `screenshot_requested`.
- [x] **1.15** Remove `glFinish()` from steady pre-swap capture path; use frame fence / `GL_SYNC_GPU_COMMANDS_COMPLETE` only where driver requires it for PBO map. (ST sync-on-Load fallback may still use `glFinish` — on-demand only.)

### 1D — `SituationTakeScreenshot` + explicit API

- [ ] **1.16** `SituationTakeScreenshot`: prefer valid cache; on miss perform one-shot sync capture (acceptable — F12 / rare; RT + ST).
- [ ] **1.17** Document public contracts in `doc/guide/` and `doc/situation_sdk.md` (§RT vs ST one-liner + explicit path).
- [ ] **1.18** Optional: `SituationInitInfo.screen_capture_policy = SIT_SCREEN_CAPTURE_ALWAYS` for bisect/revert only.

### 1E — Other waste audits (same phase)

- [ ] **1.19** Confirm `_SitGLApplySwapIntervalBeforePresent` stays change-detection only (v2.4.319) — no per-frame WGL call in steady state.
- [ ] **1.20** Shader async poll in `AcquireFrameCommandBuffer` (GL): verify it cannot block; move to `PollInputEvents` if any path calls `glCompileShader` synchronously on hot path (audit only; fix only if proven > 0.5 ms).

**Gate 1:**
```powershell
& ".\build\build_situation.bat" all-static
& ".\build\run_tests.bat" opengl --module graphics
& ".\build\run_tests.bat" opengl --module virtual_display
& ".\build\run_tests.bat" opengl --module transfer
& ".\build\run_tests.bat" vulkan --module graphics
& ".\build\run_tests.bat" opengl --filter gl_endframe_execute_vd_streak
& ".\build\run_tests.bat" opengl --module graphics --filter gl_load_urgent_after_endframe
& ".\build\run_tests.bat" opengl --module graphics --filter gl_load_after_endframe_st
# Repeat Gate 0 pacing test: p95 frame_time must improve vs baseline; present phase must shrink
# Example 02: 60 s with M overlay — no readback on frames without F12
```

**Gate 1 result @ 2026-06-28 (OpenGL):** graphics **125/125**, virtual_display **35/35**, transfer **12/12**, frame_pacing **1/1**, `gl_endframe_execute_vd_streak` **PASS** (virtual_display module); `gl_load_*` in **graphics** module.

---

## Phase 2 — Present-anchored timing & true FPS (R1, R3)

### 2A — Display clock handoff

- [x] **2.1** On render thread, after successful `glfwSwapBuffers` / `vkQueuePresentKHR`:
  - `present_complete_time_ns = _SitGetMonotonicTimeNS()`
  - `delta = present_complete_time_ns - last_present_complete_time_ns`
  - Atomics: `sit_gs.frame_time = delta / 1e9`, `sit_gs.previous_time`, `sit_gs.current_time` (seconds, monotonic-derived)
  - Increment `fps_present_counter`
- [x] **2.2** Main thread `SituationUpdateTimers` / `AcquireFrameCommandBuffer`: **stop** writing `frame_time` when render thread enabled; read-only consume of atomic display delta.
- [x] **2.3** Single-threaded fallback: keep present-at-EndFrame timing on main (same code path as today's swap).

### 2B — FPS counter

- [x] **2.4** Move FPS rollup to present boundary (or roll present counter in 2.1 and compute in existing 1.0 s block using monotonic clock).
- [x] **2.5** Replace `(int)` truncate with **refresh-aware rounding**:
  - `refresh = (float)SituationGetMonitorRefreshRate(window_monitor_id)` (Hz).
  - If VSync ON and `|fps - refresh| < 1.0`, report `(int)(refresh + 0.5)`.
  - Else report `(int)(fps + 0.5)` (nearest).
- [x] **2.6** Optional: `SituationGetDisplayRefreshRate()` public alias if a monitor-agnostic getter is needed for HUD.

### 2C — Simulation dt policy

- [x] **2.7** Document: `SituationGetFrameTime()` = **display delta** (for rendering/camera); not guaranteed equal to poll-to-poll wall time when pipelined.
- [ ] **2.8** Optional `SituationGetSimulationDeltaTime()` — main-thread wall clock between UpdateTimers calls — only if user demand; **defer** unless physics tests need it.

**Gate 2:** Gate 0 test shows `SituationGetFPS()` ∈ {60, 59} where 59 only on 59.94 Hz panels (document); median `frame_time` within ±0.3 ms of `1000/refresh`.

**Gate 2 result @ 2026-06-28:** OpenGL — `SituationGetFPS()` **60**, median **16.670 ms** vs `1000/59` ≈ 16.949 ms (Δ **0.28 ms**). Vulkan — FPS **60**, median **16.655 ms** (Δ **0.29 ms**).

---

## Phase 3 — VSync pacing & queue depth (R4)

**Goal:** When paced, pipeline depth ≈ swapchain depth, not 6.

- [x] **3.1** Introduce `sit_render.paced_frames_in_flight` — runtime value:
  - VSync ON or `target_frame_time > 0`: **2** (double-buffer pace) or **3** (triple-buffer if tearing observed on GL).
  - Truly uncapped: keep `SITUATION_MAX_FRAMES_IN_FLIGHT` (6).
- [x] **3.2** Replace comparisons `frames_pending >= SITUATION_MAX_FRAMES_IN_FLIGHT` with `>= paced_frames_in_flight` when `_SitShouldEngageBackpressure()`.
- [x] **3.3** Under VSync, **disable adaptive SLEEP policy** — force `YIELD` (or `SPIN` on desktop) when `target_frame_time == 0`; VSync present is the sleeper. Rationale: v2.4.319 SLEEP + `cnd_wait` injects 1–15 ms wake jitter on Windows.
- [x] **3.4** `SituationSetVSync` / `SituationSetTargetFPS`: recompute `paced_frames_in_flight` and reset metric window (extend v2.4.319 reset).

**Gate 3:** Gate 0 test `p95` < 20 ms; `last_backpressure_ns` median < 0.5 ms when scene is trivial (example 02).

**Gate 3 result @ 2026-06-28:** OpenGL — p95 **17.31 ms**, last backpressure **300 ns**. Vulkan — p95 **18.87 ms**, backpressure **0 ns**. Graphics regression **127/127** (OpenGL).

---

## Phase 4 — Sequencing hardening & developer visibility

- [x] **4.1** Debug build: if `SituationUpdateTimers` while `in_frame`, keep existing `UPDATE_AFTER_DRAW_VIOLATION` — add one-line doc link in stderr.
- [x] **4.2** Debug build: warn once if RT urgent latch times out (ST sync-on-Load failures get existing readback errors).
- [x] **4.3** Update `doc/architecture.md` §Threading / frame loop diagrams: present-anchored timing, paced in-flight count, on-demand capture (RT urgent latch vs ST sync-on-Load).
- [x] **4.4** Update `.kiro/steering/situation-project.md` frame pattern + capture contract (§RT vs ST; not harness-wide `RequestScreenCapture`).
- [x] **4.5** `SituationDrawMetricsOverlay`: show `refresh Hz`, `paced slots`, `present dt`, `capture` (none / requested / urgent) explicitly.

**Gate 4:** Doc review only; no new failures.

---

## Phase 5 — Ship & version

- [x] **5.1** Bump `sit/situation_base_version.h`; append `doc/updatelog_24_04.md`.
- [x] **5.2** `doc/whatsnew.md` — user-visible: sharper VSync pacing, fixed 60 FPS display, faster default loop (no per-frame GL readback).
- [x] **5.3** Rebuild DLLs; run wrapper smokes per `LIBRARY_RECOVERY_PLAN_244` D-C1.

**Gate 5 result @ 2026-06-28:** `verify_renderer_fwd.py` **370/370** OK. Full harness: OpenGL **605/616** (1 fail: `tone_synth.patch_memory`, audio — pre-existing); Vulkan **595/606** (same). **10 skipped** each. `frame_pacing_vsync_baseline` **PASS** both backends. Wrapper D-C1 **not run** — Track D still blocks fresh DLL embeds per recovery plan.

**Gate 5 (release):**
```powershell
& ".\build\build_situation.bat" all
& ".\build\run_tests.bat" opengl
& ".\build\run_tests.bat" vulkan
# Gate 0 + example 02 manual: M overlay — spikes near 0 over 60 s
```

---

## Regression matrix (run every phase)

| Area | Command / check |
|------|-----------------|
| Full GL harness | `run_tests.bat opengl` |
| Full VK harness | `run_tests.bat vulkan` |
| Readback | `--module graphics`, `virtual_display`, `transfer` |
| GL RT execute | `--filter gl_endframe_execute_vd_streak` |
| Implicit load (RT) | `--module graphics --filter gl_load_urgent_after_endframe` |
| Implicit load (ST) | `--module graphics --filter gl_load_after_endframe_st` |
| Frame pacing | `--module frame_pacing --filter frame_pacing_vsync_baseline` |
| Wrapper | `build\build_lua_example.bat`, Rust/Zig hello |
| Examples | `02_draw_shapes` 60 s visual + metrics overlay |

---

## Risk register

| Risk | Mitigation |
|------|------------|
| **R6** — readback tests fail after gating | RT: urgent latch; ST: sync-on-Load; tests `gl_load_urgent_after_endframe` + `gl_load_after_endframe_st`; full graphics/VD/transfer modules in Gate 1 |
| **Urgent race missed (RT only)** | Prompt-after-`EndFrame` is the common case; timeout + debug warn; `SIT_SCREEN_CAPTURE_ALWAYS` init policy for emergency revert. ST unaffected (sync read in `Load`). |
| **ST DWM read reliability** | Post-present sync read in `Load` is DWM-sensitive — same class as today’s ST fallback; explicit `RequestScreenCapture` before `EndFrame` preferred for pixel tests |
| **Track D** — GL error queue pollution | Drains inside capture block when capture runs; execute-entry drain always (v2.4.367) |
| **PBO driver variance** | Gate 1 on target HW; fall back to fenced sync read inside capture block only (never on non-capture frames) |
| **59 vs 60 display** | Phase 2.5 refresh-aware rounding + expose real Hz to HUD |
| **Vulkan present timing** | `vkQueuePresentKHR` return ≠ vsync complete on all drivers; validate with frame_pacing test; fall back to fence+present semaphore timing if needed |
| **Uncapped regression** | Gate 1/3 with VSync OFF run: FPS must remain >> refresh (example F9 toggle) |

---

## Implementation order (DAG)

```text
Phase 0 (baseline)
    ↓
Phase 1 (on-demand capture: gate + urgent latch + PBO) — single PR or 1A→1B→1C sub-PRs
    ↓
Phase 3 (paced depth + disable VSync SLEEP) — can parallel with Phase 2 after 1 ships
    ↓
Phase 2 (present-anchored timing + FPS)
    ↓
Phase 4 (docs/overlay)
    ↓
Phase 5 (release)
```

**Do not** start Phase 2 before Phase 1 — present-anchored timing on top of unconditional readback still stutters.

---

## Success criteria (plan complete)

1. **No regression** — all gates green; zero new harness skips; `gl_endframe_execute_vd_streak` + implicit `EndFrame`→`Load` pass on **both RT and ST** without harness API changes.
2. **Solid 60** — on 60.0 Hz panel: FPS shows 60, median frame 16.67 ms ± 0.5 ms; on 59.94 Hz: FPS shows 60 (rounded) or 59 with Hz label, no false stutter spam.
3. **Sequencing** — architecture doc + steering reflect single contract; timing tied to present.
4. **No waste** — zero unconditional full-frame readback on GL game loop; capture only on requested/urgent/TakeScreenshot frames; zero per-frame swap-interval calls; backpressure near-zero in trivial VSync scene.

---

## Changelog (plan document)

| Date | Revision |
|------|----------|
| 2026-06-28 | Initial draft from game-loop investigation @ v2.4.383 |
| 2026-06-28 | Phase 1 revised: on-demand capture + urgent latch + PBO (replaces harness-wide `RequestScreenCapture` migration) |
| 2026-06-28 | §RT vs ST capture semantics: urgent latch (RT) vs sync-on-Load (ST); split Phase 1B tests and thread-ownership tables |
| 2026-06-28 | Phase 1 implemented: RT urgent latch + PBO, ST epoch cache + pre-swap fallback; Gate 1 green |
