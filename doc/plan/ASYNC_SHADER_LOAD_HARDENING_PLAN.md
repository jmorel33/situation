# Async Shader Load Hardening Plan

**Date**: 2026-06-10  
**Status**: IN PROGRESS — Phase **A** shipped in **v2.4.238** (verify timing); Phase **B–E** open; **v2.4.239** added thread naming only  
**Priority**: HIGH  
**Baseline**: **v2.4.239** (`Main Thread OS Name API`)  
**Scope**: Public **`SituationBeginLoadShaderFromMemory` / `SituationPollShaderLoad` / `SituationUnloadShader`** (all renderer builds); implementation work below is **split by backend** — see § Scope & backends  
**Companion**: `doc/plan/LIBRARY_BUGFIX_PLAN.md`, `doc/plan/THREADING_BOLSTERING_PLAN.md`, `doc/plan/TEST_HARNESS_PLAN.md`, [`VULKAN_SHADER_CACHE_PLAN.md`](VULKAN_SHADER_CACHE_PLAN.md)

---

## Master progress dashboard

| Phase | Version | Status |
|-------|---------|--------|
| **A** — Fast unload (Vulkan progress driver) | v2.4.238 | 🟡 Implemented — verify build/tests |
| **B** — Starvation drive | v2.4.240 | ⬜ Not started |
| **C** — State machine cleanup | v2.4.240 | ⬜ Not started |
| **D** — Test harness hardening | harness / v2.4.241 | ⬜ Not started |
| **E** — Observability | v2.4.241+ | ⬜ Not started |

- [ ] **Plan complete** — all phases shipped, verification matrix green on both exes
- [ ] **This doc** — master dashboard updated to ✅ for each phase

---

## Goal

The **public async shader contract** is backend-agnostic: begin → poll until success or a defined error → unload safely, with **no hangs** and **predictable timing**.

This plan hardens that contract on **every active renderer build**:

- **Vulkan + `SITUATION_ENABLE_SHADER_COMPILER`**: shaderc on the thread pool — **primary bug surface** (10 s unload, LOST, `-557`, `-752`).
- **OpenGL**: driver async compile + link poll on the **host GL context** — separate mechanics; must stay correct and must not regress when Vulkan paths change.
- **Thread pool** (library-wide): job retire / HOL / CLAIM_BIT fixes benefit all pool users; only the **async shaderc worker** is Vulkan-specific.

Target: **~300 ms typical** end-to-end on reference hardware after unload-during-load sequences (Vulkan shaderc path); OpenGL should remain sub-second with no new leaks.

---

## Scope & backends

Situation is **not Vulkan-only**. Async shader loading is one **shared SITAPI** with **three distinct implementation paths** in `situation_impl_renderer.h`:

| Build | Begin | Poll | Unload | Thread pool | This plan |
|-------|-------|------|--------|-------------|-----------|
| **OpenGL — GLSL** | `glCompileShader` (non-blocking) on host GL ctx; `gl_async_load_stage = COMPILE` | `_SituationPollGLAsyncShaderLoad` → per-shader compile poll + link | Immediate `glDeleteShader` / reset stage — **no wall-clock wait** | Not used for compile | **Audit only** (Phase D tests); no `compile_done` / 10 s wait |
| **OpenGL — SPIR-V** | `BeginLoadShaderFromSpirvMemory*`; `gl_async_load_stage = SPIRV` | `_SituationPollGLAsyncSpirvShaderLoad` (specialize + link substages) | Same immediate GL teardown | Not used | **Audit only**; align error codes / unload-during-load with public contract |
| **Vulkan — GLSL→SPIR-V** | `_SituationVkAsyncShaderLoad` + `SituationSubmitJobEx(_SituationVkAsyncCompileWorker)` | `_SituationPollVkAsyncShaderLoad` | `_SituationVulkanFreeAsyncShaderLoad` — **2 s abandon** (`UNLOAD_ABANDON_NS`); **10 s `UNLOAD_WAIT_NS`** shutdown last-resort (v2.4.238) | **Core of this plan** | **Phases A–E** |
| **Vulkan — SPIR-V memory** | Copy bytecode; `compile_done = 1` (no shaderc job) | Pipeline build on poll | Same `FreeAsync` path if ctx present | Usually none | **Partial** — shared unload/progress helper if ctx used |
| **No shader compiler / fallback** | Sync `SituationLoadShaderFromMemory` | N/A | Normal slot free | N/A | Out of scope |

### Public API invariants (all backends)

These must hold in **both** `sit_test.exe` (OpenGL) and `sit_test_vulkan.exe`:

| # | Invariant |
|---|-----------|
| P1 | **`SituationPollShaderLoad` never blocks forever** — returns `SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS` or a terminal errno. |
| P2 | **`SituationUnloadShader` is safe during in-flight load** — no UAF, no slot leak warnings at shutdown. |
| P3 | **Same errno semantics where comparable** — e.g. compile fail, timeout, invalid resource (backend-specific detail strings OK). |
| P4 | **Harness async tests** (`test_graphics.c`) run on **whichever backend the exe was built for** — helpers use `graphics_test_glsl_*()` versioned per backend. |

**Verification checklist (run after each phase):**

- [ ] **P1** — OpenGL: no infinite poll loop in async tests
- [ ] **P1** — Vulkan: no infinite poll loop; `-557` / `-99` on true wedge only
- [ ] **P2** — OpenGL: unload-during-load + full harness → no “Leaked Shader”
- [ ] **P2** — Vulkan: unload-during-load + poll-after-unload → no “Leaked Shader”
- [ ] **P3** — Document any intentional backend errno differences in UPDATELOG if found
- [ ] **P4** — Both exes: all `async_shader_*` tests registered and green

### Vulkan-only invariants (shaderc + thread pool)

| # | Invariant |
|---|-----------|
| V1 | **`compile_done` state machine is complete** — see table below. |
| V2 | **Pool job handle and `compile_done` stay consistent** — never free `ctx` while `compile_job` is unretired. |
| V3 | **No inline shaderc while an unretired pool job exists** (v2.4.236). |
| V4 | **Worker always completes its job slot** — even on early return (CAS fail, abandon). |
| V5 | **Abandon never UAF** — `-2` ownership rules. |
| V6 | **Poll and unload share one Vulkan progress function** — no duplicated LOST/timeout logic. |

**Verification checklist:**

- [ ] **V1** — All `compile_done` values handled in poll, unload, worker (incl. `-3` in-progress)
- [ ] **V2** — `WaitForJob` after `compile_done==1` before ctx free (v2.4.105)
- [ ] **V3** — No inline pump when `compile_job != 0` unless slot retired first (Phase B)
- [ ] **V4** — Worker early-return still completes job slot in `threading.h`
- [ ] **V5** — Abandon path: slot detached; worker frees ctx on `-2` CAS fail
- [ ] **V6** — Single `_SituationVkAsyncCompileProgress` owns LOST/timeout (Phase A)

### OpenGL — known differences (not bugs by themselves)

- Compile is kicked on **Begin** on the **main/host thread** with GL current; poll drives **driver** completion — no `compile_done`, no shaderc worker.
- **Unload during load** deletes GL shader objects immediately; there is **no** 10 s wait (and no equivalent of `-557` / thread LOST on that path today).
- **SPIR-V async** uses multi-substage poll (`gl_async_load_stage`, `gl_spirv_substage`) — parallel concept to Vulkan poll, different code path.

**OpenGL gap checklist (Phase D):**

- [ ] Confirm `SituationPollShaderLoad` / `SituationUnloadShader` dispatch stays `#if SITUATION_USE_OPENGL` clean after Vulkan refactors
- [ ] Run `async_shader_*` sequence on `sit_test.exe` — green, no leak warnings
- [ ] Run `test_graphics_spirv.c` async cases on OpenGL — green
- [ ] Run `demon_hunt_sky_spirv_*` on OpenGL — green
- [ ] Document OpenGL unload-during-load behaviour in UPDATELOG if harness timing asserts added

### Constants naming

Keep **`SITUATION_VULKAN_*`** prefixes for Vulkan-specific nanosecond budgets. If OpenGL later needs deadlines (optional Phase D+), add **`SITUATION_OPENGL_ASYNC_*`** — do not overload Vulkan constants on GL paths.

- [ ] All new timing macros use correct backend prefix
- [ ] No OpenGL code references `SITUATION_VULKAN_*` unload/compile deadlines

---

## Current state (v2.4.239)

| Check | Result |
|-------|--------|
| All Vulkan graphics tests | ✅ Green (pre-238 baseline) |
| `async_shader_poll_after_unload_during_load` | 🟡 **Phase A target < 1 s** — was **~10.3 s** on v2.4.237; re-verify after v2.4.238 |
| `async_shader_unload_during_load` | ✅ ~300 ms |
| Leaked Shader warnings | Reduced in v2.4.238 (worker `-2` CAS fail frees ctx) |

**Fixed in v2.4.238 (Phase A)**: Poll and unload share `_SituationVkAsyncCompileProgress` — LOST detection, tiered abandon (**2 s**), job retire on abandon, worker `-2` ctx free.

**Still open (Phase B+)**: Main thread does not drive pool during unload wait (no event pump / unclaimed fast inline); abandon without full starvation mitigation may still wedge next compile under stress.

**Recent fix history** (see `doc/UPDATELOG.md`):

- **v2.4.229–232** — render queue wedge, Epic D in-place claim, handle validity
- **v2.4.233** — full-queue HOL scan (removed 32-slot cap)
- **v2.4.234** — `_SitJobDecrementDependency`, compile pump, `compile_done` CAS `0→-3`
- **v2.4.235–236** — pump orphan / `WaitForJob` hang fix, `_SitThreadPoolRetireOrphanedJobMain`
- **v2.4.237** — treat `-3` as in-progress in poll and unload wait (fixed false **-752**)

---

## Core invariants (never violate)

**Cross-backend**: P1–P4 above. **Vulkan shaderc path**: V1–V6 above. **Housekeeping**: forward declarations stay in `situation_impl_forward.h` + `situation_impl_renderer_fwd.h` only — no new fwd files.

- [ ] No new `*_fwd.h` files introduced by this plan
- [ ] New statics forward-declared only in `situation_impl_renderer_fwd.h` (Vulkan) or `situation_impl_forward.h` (thread pool)

### Formal `compile_done` states (Vulkan + shaderc only)

| Value | Name | Meaning |
|-------|------|---------|
| `0` | PENDING | Submitted; worker may not have started |
| `-3` | COMPILING | Worker owns shaderc (CAS `0→-3`) |
| `1` | SPIRV_READY | shaderc OK; poll builds pipelines |
| `-1` | FAILED | shaderc failed |
| `-2` | ABANDONED | Main released ctx; worker frees if it runs |

**Rule**: Only `-1` and `-2` are terminal failures from the API. `-3` is always in-progress.

---

## Root problems still open

```
Unload wait: only thrd_yield + no-op pump
        ↓
No LOST detection in FreeAsync
        ↓
10 s budget before abandon  ←── async_shader_poll_after_unload_during_load ~10.3 s
        ↓
Abandon does not retire compile_job → pool HOL wedge for next compile
        ↓
Worker CAS fail on -2 does not free ctx → Leaked Shader warnings
```

| Problem | Symptom | Fix phase |
|---------|---------|-----------|
| **Asymmetric wait logic** | ~~Poll detects LOST immediately; unload waits 10 s~~ **Fixed v2.4.238** | A ✅ |
| **No pool drive during unload** | Pump no-ops when `compile_job != 0`; no event/frame pump | A, B |
| **Abandon without job retire** | ~~`compile_done=-2`, return; job may sit in queue~~ **Fixed v2.4.238** | A ✅ |
| **Worker CAS fail on `-2`** | ~~Early return without freeing ctx~~ **Fixed v2.4.238** | A ✅ |
| **Single 10 s timeout** | ~~One knob for “slow compile” vs “never runs”~~ **Tiered constants v2.4.238** | A ✅ |
| **Duplicated LOST/timeout logic** | ~~Poll vs unload drift~~ **Shared driver v2.4.238**; state-machine cleanup in C | A ✅, C |
| **No compile content verification in harness** | Poll SUCCESS without draw/readback | D |

---

## Target architecture — one Vulkan progress driver

Replace three ad-hoc **Vulkan** paths (poll pump, unload spin, worker) with a **single internal function** (OpenGL keeps `_SituationPollGLAsyncShaderLoad` / SPIR-V substages; only shared surface is public `SituationPollShaderLoad` dispatch):

```c
/* situation_impl_renderer.h — internal only */
typedef enum {
    SIT_VK_ASYNC_PROGRESS_IN_PROGRESS,
    SIT_VK_ASYNC_PROGRESS_SPIRV_READY,
    SIT_VK_ASYNC_PROGRESS_FAILED,
    SIT_VK_ASYNC_PROGRESS_LOST,
    SIT_VK_ASYNC_PROGRESS_TIMEOUT,
    SIT_VK_ASYNC_PROGRESS_ABANDONED
} _SitVkAsyncCompileProgressResult;

static _SitVkAsyncCompileProgressResult
_SituationVkAsyncCompileProgress(
    _SituationVkAsyncShaderLoad* ctx,
    _SituationShaderSlot* slot,
    uint32_t mode_flags,
    uint64_t elapsed_ns);
```

**Mode flags**:

- `SIT_VK_ASYNC_PROGRESS_POLL` — may build pipelines on SUCCESS; uses compile deadline (5 s)
- `SIT_VK_ASYNC_PROGRESS_UNLOAD` — must not build pipelines; tiered timeouts (see below)
- `SIT_VK_ASYNC_PROGRESS_SHUTDOWN` — short budget; abandon aggressively

**Forward declare** in `situation_impl_renderer_fwd.h` only (existing convention).

### Progress driver algorithm (each call)

1. Load `compile_done`. If terminal (`1`, `-1`, `-2`), map and return.
2. If `0` or `-3`:
   - **LOST check** (threading): `compile_job != 0 && settled && compile_done == 0` → retire job, free ctx, return LOST.
   - **Starvation check**: elapsed > `UNCLAIMED_FAST_NS` and job still unclaimed → safe retire + inline (see § Safe main-thread drive).
   - **Compile deadline** (poll): elapsed > `COMPILE_DEADLINE_NS` → CAS abandon `-2`, retire job, return TIMEOUT.
   - **Unload tier-1**: elapsed > `UNLOAD_COOPERATIVE_NS` (~500 ms) → pump events + optional `WaitForJob` slice.
   - **Unload tier-2**: elapsed > `UNLOAD_ABANDON_NS` (~2 s) → abandon + retire job, return ABANDONED.
3. Return IN_PROGRESS.

**Poll** and **FreeAsync** both call this — no duplicated LOST/timeout branches.

---

## Tiered timing constants

Split the single 10 s knob into purpose-specific budgets in `sit/situation_api.h`:

| Constant | Proposed value | Purpose |
|----------|----------------|---------|
| `SITUATION_VULKAN_ASYNC_COMPILE_DEADLINE_NS` | **5 s** (keep) | Poll: wedged compile → `-557` |
| `SITUATION_VULKAN_ASYNC_UNLOAD_COOPERATIVE_NS` | **500 ms** (new) | Unload: wait for normal shaderc (~300 ms) + margin |
| `SITUATION_VULKAN_ASYNC_UNLOAD_ABANDON_NS` | **2 s** (new) | Unload: abandon if still `{0,-3}` |
| `SITUATION_VULKAN_ASYNC_UNCLAIMED_FAST_NS` | **100 ms** (new) | Job submitted but never claimed → steal/inline |
| `SITUATION_VULKAN_ASYNC_SHUTDOWN_NS` | **500 ms** (new) | Destroy path |

**`SITUATION_VULKAN_ASYNC_UNLOAD_WAIT_NS` (10 s)**: demote to shutdown / last-resort only, or remove after tier-2 ships. Normal unload must **not** reach it.

**Constants checklist (`sit/situation_api.h`):**

- [x] Add `SITUATION_VULKAN_ASYNC_UNLOAD_COOPERATIVE_NS` (500 ms) with comment
- [x] Add `SITUATION_VULKAN_ASYNC_UNLOAD_ABANDON_NS` (2 s) with comment
- [x] Add `SITUATION_VULKAN_ASYNC_UNCLAIMED_FAST_NS` (100 ms) with comment
- [x] Add `SITUATION_VULKAN_ASYNC_SHUTDOWN_NS` (500 ms) with comment
- [x] Update `SITUATION_VULKAN_ASYNC_UNLOAD_WAIT_NS` comment — shutdown/last-resort only
- [x] Keep `SITUATION_VULKAN_ASYNC_COMPILE_DEADLINE_NS` at 5 s (unchanged value)

### Expected timings after fix

| Scenario | Target |
|----------|--------|
| Normal async poll | ~300 ms |
| Unload during active compile | ~300 ms |
| Unload, compile never scheduled | ~100–500 ms (fast LOST/steal), not 10 s |
| Wedged compile (poll) | 5 s → `-557` |
| Wedged compile (unload) | 2 s → abandon + retire |

---

## Safe main-thread compile drive (no orphan regression)

v2.4.236 banned blind inline when `compile_job != 0`. Replace with **conditional drive** (Phase B):

- [ ] Detect job state: completed / claimed (`CLAIM_BIT`) / unclaimed (`dep==0`)
- [ ] If completed → `WaitForJob` is instant; proceed
- [ ] If claimed or `compile_done == -3` → do **not** inline; wake workers only
- [ ] If unclaimed and elapsed > `UNCLAIMED_FAST_NS` → `_SitThreadPoolRetireOrphanedJobMain` then `ctx->compile_job = 0` then inline worker
- [ ] If unclaimed and young → wake workers / yield (no inline)
- [ ] Add regression test or stress loop proving no `WaitForJob` hang after inline path

Inline only **after** the pool slot is retired — prevents v2.4.234–235 orphan / `WaitForJob` hang.

---

## Job lifecycle hardening (thread pool)

### Abandon path (FreeAsync + poll timeout)

On any abandon (`compile_done → -2`):

- [ ] `_SitThreadPoolRetireOrphanedJobMain(pool, compile_job)` if handle valid and not completed
- [ ] `SituationWaitForJob` (should return immediately after retire)
- [ ] Clear `ctx->compile_job = 0`
- [ ] Detach from slot (`slot->vk_async_load = NULL`); worker frees ctx if it ever runs
- [ ] Centralize in `_SituationVkAsyncCompileAbandon(ctx, slot, pool)` helper (optional)

### Worker early-return paths

In `_SituationVkAsyncCompileWorker`, when CAS `0→-3` fails:

- [ ] If `compile_done == -2` → `_SituationVkAsyncCompileFreeCtx(ctx)`
- [ ] Return; job slot still completed by worker wrapper in `threading.h`
- [ ] No ctx leak on abandon-before-worker path

### RetireOrphanedJobMain guardrails

- [ ] Never call on job with `CLAIM_BIT` set — verify guard remains
- [ ] Always call before inline compile on main (Phase B)
- [ ] Always call on abandon/timeout before detaching slot (Phase A)
- [ ] Optional: `_SitThreadPoolCancelJobMain(pool, job_id)` thin wrapper (retire + wake)

---

## Unload wait loop (replace bare spin)

**Target behaviour** in `_SituationVulkanFreeAsyncShaderLoad`:

- [ ] Replace bare `thrd_yield` loop with `_SituationVkAsyncCompileProgress(..., UNLOAD, elapsed)`
- [ ] Call `SituationPollInputEvents()` each iteration while IN_PROGRESS
- [ ] Optional: one `EndFrame` when render loop active (matches harness)
- [ ] Use `_SitThreadSleepMs(1)` instead of yield-only storm
- [ ] Abandon at `UNLOAD_ABANDON_NS` (2 s), not `UNLOAD_WAIT_NS` (10 s)
- [ ] LOST detection same as poll (via progress driver)
- [ ] After terminal progress: existing `RetireOrphanedJobMain` + `WaitForJob` when `compile_done==1`

---

## Leaked shader cleanup

- [ ] Poll terminal error path: slot reset after ctx freed
- [ ] Unload abandon path: `SituationUnloadShader` zeroes handle generation
- [ ] Shutdown sweep: slots with `vk_async_load != NULL` → progress driver SHUTDOWN mode
- [ ] Full graphics module on both exes → zero “Leaked Shader (Slot N)” warnings

---

## Implementation phases

### Phase A — Fast unload (low risk, immediate test win) — **Vulkan + shaderc**

**Version target**: **v2.4.238**  
**Files**: `sit/situation_impl_renderer.h`, `sit/situation_impl_renderer_fwd.h`, `sit/situation_api.h`  
**OpenGL**: no code change expected; smoke `sit_test.exe --module graphics` after patch.

#### A.1 — Types and forward declarations

- [x] Add `_SitVkAsyncCompileProgressResult` enum
- [x] Add mode flags (`POLL`, `UNLOAD`, `SHUTDOWN`)
- [x] Forward-declare `_SituationVkAsyncCompileProgress` in `situation_impl_renderer_fwd.h`
- [x] Forward-declare `_SituationVkAsyncCompileFreeCtx` in `situation_impl_renderer_fwd.h`

#### A.2 — Shared helpers

- [x] Implement `_SituationVkAsyncCompileFreeCtx(ctx)` — single free path for src, spirv blobs, ctx
- [x] Use `_SituationVkAsyncCompileFreeCtx` from poll LOST path, poll fail path, worker `-2` path, FreeAsync success path

#### A.3 — Progress driver

- [x] Implement `_SituationVkAsyncCompileProgress`
- [x] Handle terminal states: `1`, `-1`, `-2`
- [x] Treat `0` and `-3` as in-progress
- [x] LOST: settled handle + `compile_done==0` → retire, free, return LOST
- [x] Poll timeout: `COMPILE_DEADLINE_NS` → abandon CAS, retire job, return TIMEOUT
- [x] Unload tier-1: `UNLOAD_COOPERATIVE_NS` — event pump hook (stub OK in A)
- [x] Unload tier-2: `UNLOAD_ABANDON_NS` → abandon + retire, return ABANDONED

#### A.4 — Wire callers

- [x] Refactor `_SituationPollVkAsyncShaderLoad` to use progress driver (remove duplicated LOST/timeout blocks)
- [x] Refactor `_SituationVulkanFreeAsyncShaderLoad` wait loop to use progress driver
- [x] Keep `WaitForJob` + orphan retire after `compile_done==1` (v2.4.105 / v2.4.236)

#### A.5 — Worker fix

- [x] `_SituationVkAsyncCompileWorker`: on CAS fail, if `compile_done==-2`, call `_SituationVkAsyncCompileFreeCtx`

#### A.6 — Constants

- [x] All items in **Constants checklist** above

#### A.7 — Version and docs

- [x] `sit/situation_base_version.h` → **2.4.238**, description e.g. `Async Shader Unload Progress Driver`
- [x] `doc/UPDATELOG.md` — v2.4.238 entry with test plan and timing notes

#### A.8 — Phase A verification

- [ ] `build_situation.bat static-vulkan` — clean build
- [ ] `build_tests.bat static-vulkan` — clean build
- [ ] `build\tests\sit_test_vulkan.exe --module graphics` — all green
- [ ] `async_shader_poll_after_unload_during_load` **< 1 s** (target ~400 ms)
- [ ] `build\tests\sit_test.exe --module graphics` — smoke, no regressions
- [ ] Invariant checklists **P1–P2**, **V1–V6** (as applicable) checked above

#### Phase A completion gate

- [ ] All A.1–A.8 boxes checked
- [ ] Master dashboard: Phase **A** → ✅

---

### Phase B — Starvation drive (medium risk) — **Vulkan shaderc + thread pool**

**Version target**: **v2.4.240**  
**Files**: `sit/situation_impl_renderer.h`, `sit/situation_impl_threading.h`  
**Note**: validate `sit_test.exe --module threading` unchanged.

#### B.1 — Unclaimed fast path

- [ ] In progress driver: detect unclaimed job after `UNCLAIMED_FAST_NS`
- [ ] Retire job via `_SitThreadPoolRetireOrphanedJobMain`
- [ ] Set `ctx->compile_job = 0`
- [ ] Run `_SituationVkAsyncCompileWorker` inline on main
- [ ] Increment metric counter (stub OK until Phase E)

#### B.2 — Cooperative unload/poll pump

- [ ] Unload wait: `SituationPollInputEvents()` each IN_PROGRESS iteration
- [ ] Unload wait: `_SitThreadSleepMs(1)` instead of yield-only
- [ ] Optional: `EndFrame` when `SituationAcquireFrameCommandBuffer` would succeed (match harness)
- [ ] Poll path: ensure frame pump in harness helper remains sufficient; no main-thread `WaitForJob` in poll

#### B.3 — Optional main steal

- [ ] Evaluate whether retire+inline is sufficient on GTX 1070 reference machine
- [ ] If not: main-thread steal for high-priority compile queue only
- [ ] Document decision in UPDATELOG

#### B.4 — Version and docs

- [ ] `situation_base_version.h` → **2.4.239**
- [ ] `doc/UPDATELOG.md` — v2.4.239 entry

#### B.5 — Phase B verification

- [ ] `sit_test_vulkan.exe --module graphics` — green
- [ ] `sit_test.exe --module threading` — green, unchanged
- [ ] `sit_test.exe --module graphics` — green
- [ ] 20× manual or scripted repeat: unload-during-load → poll; p99 < 800 ms
- [ ] Zero `-557`, `-752`, `-99` in repeat run
- [ ] Safe main-thread drive checklist — all checked

#### Phase B completion gate

- [ ] All B.1–B.5 boxes checked
- [ ] Master dashboard: Phase **B** → ✅

---

### Phase C — State machine cleanup (hardening)

**Version target**: **v2.4.240**  
**Files**: `sit/situation_impl_renderer.h`, `sit/situation_impl_renderer_fwd.h`

- [ ] Add `#define SIT_VK_COMPILE_DONE_*` macros for `0`, `-3`, `1`, `-1`, `-2`
- [ ] Replace magic numbers in poll, unload, worker, progress driver
- [ ] Remove duplicate `_SituationVkAsyncShaderCompilePump` `#if threading` branches where progress driver subsumes them
- [ ] Confirm no duplicated LOST/timeout blocks remain outside progress driver
- [ ] `situation_base_version.h` → **2.4.240**
- [ ] `doc/UPDATELOG.md` — v2.4.240 entry
- [ ] Full graphics module both exes — green

#### Phase C completion gate

- [ ] All Phase C boxes checked
- [ ] Master dashboard: Phase **C** → ✅

---

### Phase D — Test harness hardening — **both backends**

**Version target**: harness-only or **v2.4.241** if tied to metrics  
**Files**: `tests/harness/test_graphics.c`, `tests/harness/sit_graphics_test_helpers.h`, `tests/harness/test_graphics_spirv.c`

#### D.1 — Helper improvements

- [ ] `graphics_test_async_poll_shader_ready` returns elapsed ms (out param or struct)
- [ ] Optional: `graphics_test_assert_pool_quiescent()` for Vulkan (active_jobs==0, high depth==0)

#### D.2 — Existing test hardening

- [ ] `async_shader_poll_after_unload_during_load`: assert elapsed **< 2000 ms** (Vulkan)
- [ ] `async_shader_poll_after_unload_during_load`: assert success on OpenGL (no timing flake)
- [ ] `sync_shader_after_async_cycle`: reduce poll cap **3000 → 120** frames (after Phase A green)

#### D.3 — New tests

- [ ] Add `async_shader_unload_stress_20x` — repeat unload-during-load → poll; pool quiescent each iter (Vulkan assert)
- [ ] Add `async_shader_compile_verify_draw` — poll SUCCESS then draw + red pixel readback
- [ ] Register new tests in `test_graphics.c` suite table (before or after existing async block)

#### D.4 — SPIR-V async coverage

- [ ] Re-run `test_async_shader_spirv_memory_vulkan` — green
- [ ] Re-run `demon_hunt_sky_spirv_vk_begin_poll` — green
- [ ] OpenGL SPIR-V async paths in `test_graphics_spirv.c` — green

#### D.5 — OpenGL gap checklist

- [ ] All OpenGL gap boxes in § Scope — checked

#### D.6 — Phase D verification

- [ ] `sit_test_vulkan.exe` — full graphics + spirv tests green
- [ ] `sit_test.exe` — full graphics + spirv tests green
- [ ] New timing asserts never flake on reference machine (3 consecutive runs)

#### Phase D completion gate

- [ ] All D.1–D.6 boxes checked
- [ ] Master dashboard: Phase **D** → ✅

---

### Phase E — Observability

**Version target**: **v2.4.241** (or bundled with D)  
**Files**: `sit/situation_impl_threading_scheduler.h`, harness dump hook

- [ ] Add `stats_async_compile_abandon` to thread pool metrics
- [ ] Add `stats_async_compile_lost`
- [ ] Add `stats_async_compile_unclaimed_steal`
- [ ] Expose in existing scheduler metrics dump / JSON export
- [ ] Harness: on async test failure, dump pool metrics to stderr (test code only — no library printf)
- [ ] `doc/UPDATELOG.md` entry if version bumped

#### Phase E completion gate

- [ ] All Phase E boxes checked
- [ ] Master dashboard: Phase **E** → ✅

---

## Decision log

| Question | Decision |
|----------|----------|
| Inline compile on main? | **Only after** retiring the pool job slot |
| `WaitForJob` on main during poll? | **No** |
| `WaitForJob` on main during unload after `compile_done==1`? | **Yes** — v2.4.105 UAF fix; keep after orphan retire |
| 10 s unload wait? | **Demote** to shutdown-only; normal abandon at 2 s |
| New fwd header? | **No** |

---

## Verification matrix

```bat
build_situation.bat static-vulkan
build_tests.bat static-vulkan
build\tests\sit_test_vulkan.exe
build\tests\sit_test_vulkan.exe --module graphics

build_situation.bat static
build_tests.bat static
build\tests\sit_test.exe --module graphics
```

**Final sign-off checklist:**

- [ ] Vulkan graphics module — all tests green
- [ ] OpenGL graphics module — all tests green, no async regressions
- [ ] `async_shader_poll_after_unload_during_load` — Vulkan **< 1 s** (Phase A) / **< 500 ms p50** (Phase B)
- [ ] `async_shader_poll_after_unload_during_load` — OpenGL **< 2 s**, no leak warnings
- [ ] No “Leaked Shader (Slot N)” after full graphics on **both** exes
- [ ] Threading module — unchanged green
- [ ] 10× full `sit_test_vulkan.exe` — no flake
- [ ] 10× full `sit_test.exe` — no flake
- [ ] **Plan complete** master checkbox checked

---

## What we deliberately do not do

- [ ] ~~Reintroduce struct swap in job ring~~ — **forbidden** (Epic D regression)
- [ ] ~~Blind `fetch_sub` on `dependency_count`~~ — **forbidden** (CLAIM_BIT corruption)
- [ ] ~~`WaitForJob` inside poll pump~~ — **forbidden** (v2.4.235 deadlock)
- [ ] ~~New `situation_impl_threading_fwd.h`~~ — **forbidden**
- [ ] ~~printf debugging in library~~ — **forbidden**

---

## Suggested implementation order

1. [ ] **Phase A** — shared progress driver + tiered unload + abandon retire + worker `-2` free
2. [ ] **Phase D (partial)** — timing assert on `poll_after_unload` only
3. [ ] **Phase B** — unclaimed fast path + event pump in unload wait
4. [ ] **Phase C** — refactor once green
5. [ ] **Phase D (rest)** — stress + draw verify tests
6. [ ] **Phase E** — metrics if flakes persist

---

## Phase completion gate (any patch)

Run at end of **every** library version shipped for this plan:

- [ ] Phase task boxes for this version checked (or deferred with note in UPDATELOG)
- [ ] Harness slice green — record pass count + timing in UPDATELOG
- [ ] `sit/situation_base_version.h` — PATCH +1, DESCRIPTION updated
- [ ] `doc/UPDATELOG.md` — new top entry for this version only
- [ ] **This doc** — master dashboard + phase section updated
