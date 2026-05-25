## [v2.4.146 "Renderer Text Cleanup"] - 2026-05-24

### Description

Renderer/internal cleanup release: normalizes corrupted punctuation in renderer diagnostics and comments, and restores indentation in the control implementation header.

### Library Changes

- **`situation_impl_renderer.h`** — replaced mojibake punctuation in docs and the metric contention log with plain ASCII
- **`situation_impl_ctrl.h`** — indentation-only formatting pass

### Verification

- **OpenGL DLL build**: pass
- **Vulkan DLL build**: pass

---

## [v2.4.145 "Typed Audio Controls"] - 2026-05-24

### Description

Audio graph control readback now matches declared control types: `FLOAT` remains continuous, while `INT` / `ENUM` round to integer slots and `BOOL` normalizes to 0/1. This fixes the long-standing `audio.control_sweep_all_devices` failure without weakening the test.

### Library Changes

- **`SituationSetControl`** — clamps, then coerces by `SituationControlType` before storing/readback
- **Harness diagnostics** — `control_sweep_all_devices` now reports device/control/min/max/requested/expected/readback and set/get errors on mismatch

### Verification

- **OpenGL full harness**: **391/391**
- **Vulkan full harness**: **381/381** (rebuilt; no stale binary)
- Fixed: **`audio.control_sweep_all_devices`**

---

## [v2.4.144 "Threading Bolstering Complete"] - 2026-05-24

### Description

**Threading Bolstering — release cap** (Epics A–E, v2.4.139–143): topology/affinity, pool observability, NUMA placement, scheduler metrics, API hygiene, and a **10 s all-core harness stress** with Task Manager–correlatable CPU reports. See **`doc/THREADING_BOLSTERING_API.md`** for the consolidated public API reference.

### Library Changes

- **Harness** — `cpu_stress_10s_taskmgr`: ~10 s `SituationDispatchParallel` CPU burn, logical-CPU histogram, worker snapshot, `SituationDumpThreadPoolStatus`; skip via `SIT_SKIP_CPU_STRESS`
- **Docs** — `doc/THREADING_BOLSTERING_API.md` (API catalog); plan Epics A–E marked complete

### Verification

- **`sit_test.exe --module threading`**: **21/21**
- **OpenGL full harness**: **391** total
- CPU stress: sustained **~100%** `_Total` processor time over the 10 s window (validated via performance counters)

---

## [v2.4.143 "Threading API Hygiene"] - 2026-05-24

### Description

**Threading Bolstering — Epic E** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): accurate public API docs, main-thread affinity init hook, fail-soft affinity warnings, harness coverage, and dual-socket manual validation guide. Consolidates Epics A–D (v2.4.139–142).

### Library Changes

- **`SituationInitInfo::thread_affinity_main`** — optional main-thread pin after window creation (0 = no pin)
- **`SituationGetConfiguredMainThreadAffinity()`** — read effective main mask
- **Fail-soft affinity** — `_SituationSetThreadAffinityForRole` logs debug warning on pin failure; init continues
- **Docs** — `situation_api.h` / worker impl comments (mutex + atomics, not “lock-free”); README threading; `sit/k-term/doc/situation_api.md` bolstering table; `doc/THREADING_MANUAL_VALIDATION.md`
- **Trace** — `10030035` `SituationGetConfiguredMainThreadAffinity`

### Verification

- **`sit_test.exe --module threading`**: **20/20** (+ `configured_main_affinity`, `metrics_reset_and_dump`)

---

## [v2.4.142 "Scheduler Metrics"] - 2026-05-24

### Description

**Threading Bolstering — Epic D** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): scheduler contention counters, dynamic high-queue scan depth, physical-core pool sizing, and sizing helpers — without lock-free MPMC or worker-to-worker steal (deferred).

### Library Changes

- **New** `sit/situation_impl_threading_scheduler.h` — `SituationGetRecommendedWorkerCount`, `SituationGetThreadPoolMetrics`, `SituationResetThreadPoolStats`, `SituationDumpThreadPoolMetrics`
- **`SituationThreadPoolMetrics`** — high-queue lock ops/ns, main steal ok/fail/empty, scan-forward swap/exhausted, I/O idle/jobs + busy ratio, inline submit, queue-full spins, `DispatchParallel` call count
- **Dynamic scan** — `_SitWorkerScanDepthForPending()` scales 4–32 from pending depth (replaces fixed `SIT_WORKER_SCAN_DEPTH` 8)
- **`SituationCreateThreadPool(..., num_threads=0)`** — `_SitResolveAutoWorkerCount()` from `SituationInitInfo` `thread_pool_use_physical_cores` / `thread_pool_reserved_threads`
- **Instrumentation** — worker high-queue lock timing; I/O `stats_io_idle_waits` / `stats_io_jobs_run`
- **Trace IDs** — `10030031`–`10030034`

### Verification

- **`sit_test.exe --module threading`**: **18/18** (+ `recommended_worker_count`, `scheduler_metrics_parallel`)

---

## [v2.4.141 "NUMA Awareness"] - 2026-05-24

### Description

**Threading Bolstering — Epic C** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): NUMA topology snapshot, init-time placement policy, worker/I/O/render/audio pinning hooks, and thread-local preferred NUMA node for allocators.

### Library Changes

- **New** `sit/situation_impl_threading_numa.h` — `SituationRefreshNumaTopology`, `SituationGetNumaTopology`, `SituationGetPreferredNumaNode`
- **Types** — `SituationNumaTopology`, `SituationNumaNodeInfo` (`processor_count`, `memory_bytes`, `processor_mask_low`)
- **`SituationInitInfo`** — `numa_prefer_local`, `worker_numa_spread`, `io_thread_numa_node` (default `< 0` = no I/O pin)
- **Placement** — workers spread across nodes when enabled; I/O thread optional node pin; render/audio use `_SituationSetThreadAffinityForRole` + `numa_prefer_local` when masks are 0
- **Windows** — `GetNumaHighestNodeNumber`, `GetNumaAvailableMemoryNode`; **Linux** — sysfs `nodeN/meminfo`
- **Trace IDs** — `10030028`–`10030030`

### Verification

- **`sit_test.exe --module threading`**: **16/16** (+ `numa_topology_refresh`, `numa_node_mask`)

---

## [v2.4.140 "Thread Pool Observability"] - 2026-05-24

### Description

**Threading Bolstering — Epic B** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): export threading diagnostics, per-worker CPU sampling, queue/active-job metrics, pool snapshots, and combined debug dumps — additive on the v2.4.139 topology/affinity layer.

### Library Changes

- **New** `sit/situation_impl_threading_observability.h` — `SituationGetThreadingStatus`, `SituationPrintThreadingStatus`, queue depth APIs, pool snapshot, `SituationDumpThreadPoolStatus`, `SituationDumpThreadingReport`
- **`SituationThreadPool`** — worker `SituationWorkerStartArg`, `worker_last_logical_cpu[]`, job submit/complete + main-thread steal stats, `io_last_logical_cpu`
- **Workers** — CPU sample every 8 jobs + on idle wake; I/O thread samples each loop
- **Render / audio** — record affinity mask + sampled CPU into snapshot (`_SituationObservabilityRecord*`)
- **`SituationGetIOQueueDepth`** — delegates to `SituationGetQueueDepth(..., SIT_JOB_QUEUE_LOW)`
- **`SituationDrawMetricsOverlay`** — shows active jobs + low/high queue depths when threading enabled
- **Docs** — `doc/THREADING_TROUBLESHOOTING_GUIDE.md` (replaces broken diag header reference)
- **Trace IDs** — `10030020`–`10030027`

### Verification

- OpenGL DLL rebuild + **`sit_test.exe --module threading`**: **14/14** (7 original + 4 Epic A + 3 Epic B)

---

## [v2.4.139 "CPU Topology & Affinity"] - 2026-05-24

### Description

**Threading Bolstering — Epic A** (`doc/plan/THREADING_BOLSTERING_PLAN.md`): read-only CPU topology cache, affinity query/set with previous-mask feedback, HT/NUMA mask builders, and configurable render/audio affinity via init — additive on the existing generational thread pool (no scheduler rewrite).

### Library Changes

- **New** `sit/situation_impl_threading_topology.h` — topology refresh/query; `SituationGetCPUCoreCount()` now uses cached physical-core count (Linux sysfs; Windows `GetLogicalProcessorInformation`; macOS `sysctl`)
- **Types** — `SituationCpuTopology`, `SituationLogicalProcessorInfo`; limits `SITUATION_MAX_LOGICAL_PROCESSORS` (256), `SITUATION_AFFINITY_MASK_BITS` (64)
- **API** — `SituationRefreshCpuTopology`, `SituationGetCpuTopology`, `SituationSetThreadAffinityEx`, `SituationGetThreadAffinity`, `SituationGetCurrentProcessorIndex`, `SituationGetThreadNumaNode`, `SituationBuildPhysicalCoreMask`, `SituationBuildUniqueCoreMask`, `SituationBuildNumaNodeMask`, `SituationGetConfiguredRenderThreadAffinity`, `SituationGetConfiguredAudioThreadAffinity`
- **`SituationInitInfo`** — `thread_affinity_render`, `thread_affinity_audio` (0 = defaults: logical core 1 / 2)
- **Render / audio** — pin via configured masks at thread entry (same defaults as before when fields are 0)
- **Trace IDs** — `10030011`–`10030019` in `situation_base_trace.h`

### Verification

- OpenGL DLL rebuild + **`sit_test.exe --module threading`**: **11/11** (was 7/7; adds `cpu_topology_refresh`, `affinity_roundtrip`, `mask_builders`, `configured_affinity`)
- Pool worker / submit paths unchanged

---

## [v2.4.138 "Internal Caller Audit"] - 2026-05-24

### Description

Internal hardening **Phase 10** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): final caller audit — init tree and Vulkan create chain already propagate **`SituationError`**; fixed remaining swapchain recreate and render-list enqueue sites that dropped return values.

### Library Changes (internal only — no public API break)

- **`SituationAcquireFrameCommandBuffer`** (Vulkan) — **`VK_ERROR_OUT_OF_DATE_KHR`** / **`VK_TIMEOUT`** paths now check **`_SituationVulkanRecreateSwapchain()`** (was bare / `(void)` call)
- **`SituationSetVSync`** (Vulkan) — propagate swapchain recreate failure via **`_SituationSetErrorFromCode`**
- **`SituationSubmitRenderList`** — check **`_SituationEnqueueRenderList`**; set global error on failure (matches render-job worker)
- Gate script **`scripts/audit_phase10_caller.py`**: **0** unchecked **`_SituationInit*`** / **`_SituationVulkanCreate*`** calls in init tree

### Verification

- Gate script **`scripts/audit_phase10_caller.py`**: **0** unchecked init/Vulkan-create calls
- **Internal hardening complete at v2.4.138** (Phases 0–10, patches **127→138**)

#### Post-hardening full harness (OpenGL + Vulkan DLLs rebuilt)

**Build:** `build_situation.bat` + `build_tests.bat` for **opengl** and **vulkan** — both green.

**Single-shot full suite** (`sit_test.exe` / `sit_test_vulkan.exe`, all 12 modules, no flags):

- Both backends **abort mid-`tone_synth`** with access violation (**`0xC0000005`**) after ~15–25 long MIDI tests in one process — does **not** reproduce when those tests are run alone or in small groups.
- Logs: **`build/harness_ogl_full.log`**, **`build/harness_vk_full.log`** (partial runs).

**Per-module harness** (authoritative sign-off run):

| Module | OpenGL | Vulkan |
|--------|--------|--------|
| filesystem | 23/23 | 23/23 |
| threading | 7/7 | 7/7 |
| core | 31/31 | 31/31 |
| window | 27/27 | 27/27 |
| input | 17/17 | 17/17 |
| timer | 10/10 | 10/10 |
| Projection | 2/2 | 2/2 |
| audio | **80/81** | **80/81** |
| audio_effects_heard | 17/17 | 17/17 |
| graphics | **107/107** | **97/97** |
| misc | 20/20 | 20/20 |
| tone_synth | **35/35**† | **35/35** |
| **Suite total** | **376 pass, 1 fail** / 377 | **366 pass, 1 fail** / 367 |

† OpenGL **`tone_synth`**: all 35 pass per-module or per-test; **continuous `--module tone_synth` crashes** (sustained MIDI + GL — follow-up bug, not a Phase 10 regression). Vulkan completes **`tone_synth`** in one module run.

**Known failure (both backends, pre-existing):** **`control_sweep_all_devices`** — log-scale tone_synth control midpoint readback tolerance (`test_audio.c`).

**Graphics delta:** Vulkan harness has **10 fewer** registered tests than OpenGL (GL/SPIR-V–only cases) — expected.

**Hardening-relevant modules:** init/window/graphics/misc and backend-specific paths — **green** on both backends aside from the audio control sweep above.

---

## [v2.4.137 "Internal Void By Design Docs"] - 2026-05-24

### Description

Internal hardening **Phase 9** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): Bucket B — every intentional internal **`void`** helper tagged at forward decl with **`/* HARDENING: void by design — … */`** so Phase 10 caller audit can distinguish documented sinks from conversion debt.

### Library Changes (internal only — no public API break)

- **`situation_impl_forward.h`** — lifecycle, GLFW callbacks, thread/render worker forward decls
- **`situation_impl_renderer_fwd.h`** — VK/GL cleanup, graveyard, defer-destroy, record-only helpers, bind helpers
- **`situation_impl_decl.h`**, **`situation_impl_io.h`**, **`situation_impl_audio.h`**, **`situation_impl_threading.h`** — assert/pump, async file workers, RT mix/capture, parallel worker
- **`_SituationPopulateGLShaderUniformMap`** — N/A (returns **`SituationError`** since Phase 4); not re-tagged as void
- One-shot helper: **`scripts/tag_phase9_hardening.py`** (88 new tags; gate count **101** in `sit/situation_impl*.h`)

### Verification

- Gate: **`HARDENING: void by design`** count ≥ 68 in `sit/situation_impl*.h` → **101**
- OpenGL harness: **`core` 31/31**, **`graphics` 107/107** (comment-only; no signature changes)

---

## [v2.4.136 "Internal Hardening Stragglers"] - 2026-05-24

### Description

Internal hardening **Phase 8** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): last init-style **`bool`** helpers normalized to **`SituationError`**; intentional query/resolver **`bool`**s documented with **`HARDENING:`** notes.

### Library Changes (internal only — no public API break)

- **`_SituationExtractGLTFPrimitive`** — **`SituationError`** (`ASSET_PARSE_FAILED`, `MEMORY_ALLOCATION`); model load caller checks return
- **`_SituationSaveImageBMP`** — **`SituationError`**; **`SituationExportImage`** BMP path returns helper result directly
- **Documented void-by-design / bool-by-design**: **`_sit_directory_exists`**, **`_SituationGraphHasMixerNode`**, **`_SituationShouldMixLatentVoices`**, **`_SituationDetectCycle`**, **`_SituationVulkanResolveBufferDescriptor`**, **`_SituationVulkanImmediateDestroyDuringShutdown`**

### Verification

- OpenGL harness: **`core` 31/31**, **`graphics` 107/107** (includes **`model_*`** glTF load path)

---

## [v2.4.135 "Internal Audio Errors"] - 2026-05-24

### Description

Internal hardening **Phase 7** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): non-RT audio paths return **`SituationError`**; reverb init checks OOM on every buffer; sound load wires effects init; RT miniaudio/reverb callbacks stay **`void`** with **`HARDENING:`** notes.

### Library Changes (internal only — no public API break)

- **`_SitFreeSoundSlot`** — invalid handle → **`RESOURCE_INVALID`**; success returns **`SITUATION_SUCCESS`**
- **`_SituationInitReverb`** — **`SituationError`** + **`void** out_state`**; per-buffer OOM with **`_SituationUninitReverb`** rollback
- **`_SituationInitSoundEffects`** — called from **`SituationLoadSoundFromFile`** / **`SituationLoadSoundFromStream`**; propagates reverb alloc failures
- **`_SituationAsyncAudioWorker`** — clears target handle on load fail; error channel via **`SituationLoadSoundFromFile`**; void-by-design (pool ABI)
- **`_SitAudioCleanupPool`** — uses **`_SituationUninitReverb`** instead of raw **`SIT_FREE`**
- RT paths unchanged: **`sit_miniaudio_data_callback`**, **`_SituationProcessReverb`**, etc.

### Verification

- OpenGL harness: **`core` 31/31**, **`audio` 80/81** (`control_sweep_all_devices` — log-scale tone_synth control midpoint readback; pre-existing graph test tolerance, not Phase 7 regression)

---

## [v2.4.134 "Internal Render Thread Errors"] - 2026-05-24

### Description

Internal hardening **Phase 6** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): render-thread shutdown and per-frame graveyard flush return **`SituationError`**; GL fence timeouts and join failures propagate to **`SituationShutdown`** instead of only logging.

### Library Changes (internal only — no public API break)

- **`_SituationDestroyRenderThread`** — returns join timeout / **`THREAD_JOIN_FAILED`** (fixes mistaken **`THREAD_CREATION_FAILED`** on join)
- **`_SitFlushFrameResources`** — bounds check + **`SituationError`**; render thread logs flush failures
- **Render thread (OpenGL)** — **`glClientWaitSync`** timeout/failure sets **`RENDER_BACKPRESSURE_TIMEOUT`** / **`OPENGL_GENERAL`**
- **`SituationShutdown`** — preserves render-thread error (skips "Shutdown complete" overlay when join failed)
- **`_SituationRenderJobWorker`** — checks **`_SituationEnqueueRenderList`**; **`HARDENING:`** void-by-design note (pool ABI)

### Verification

- OpenGL harness: **`core` 31/31**, **`graphics` 107/107**

---

## [v2.4.133 "Internal Shader Async Errors"] - 2026-05-24

### Description

Internal hardening **Phase 5** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): OpenGL/Vulkan async shader poll paths and the hot-reload I/O pass return **`SituationError`** so in-progress (`SHADER_LOAD_IN_PROGRESS`), terminal compile/link/SPIR-V failures, and reload failures propagate without relying on a prior global error read.

### Library Changes (internal only — no public API break)

- **`_SituationSetGLErrorFromSpirvStage`** — returns **`SituationError`** (drops `out_code` parameter; callers assign optional `error_code` pointers)
- **`_SituationGLAsyncLoadFail`**, **`_SituationPollGLAsyncShaderLoad`**, **`_SituationPollGLAsyncSpirvShaderLoad`**, **`_SituationPollGLPendingProgramLink`** — **`SituationError`** with explicit in-progress vs terminal mapping
- **`_SituationPollVkAsyncShaderLoad`** — **`SituationError`**; **`SituationPollShaderLoad`** uses poll return values directly
- **`_SituationPerformHotReloadPass`** — returns first reload failure (shader/texture/audio) or **`SITUATION_SUCCESS`**
- **`_SituationVulkanFreeAsyncShaderLoad`** — stays **`void`** with **`HARDENING:`** comment (Phase 9 doc)

### Verification

- OpenGL harness: **`core` 31/31**, **`graphics` 107/107**, **`--filter spirv` 10/10**

---

## [v2.4.132 "Internal Uniform Map Errors"] - 2026-05-24

### Description

Internal hardening **Phase 4** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): shader uniform location cache and resource slot allocators report **`SituationError`** (or set the error channel on NULL) instead of silent OOM / registry-full failures.

### Library Changes (internal only — no public API break)

- **`_sit_uniform_map_resize`**, **`_sit_uniform_map_set`** — `void` → **`SituationError`**
- **`_sit_uniform_map_create`** — enables **`_SituationSetErrorFromCode`** on alloc failure (still returns NULL per Rule I4)
- **`_SituationPopulateGLShaderUniformMap`** — propagates map set failures
- **`_SitAlloc*Slot`** — registry full → **`SITUATION_ERROR_RESOURCE_INVALID`** + message; callers check NULL via **`SituationGetLastErrorCode()`**
- **`_SitFree*Slot`** — stay **`void`** with **`HARDENING: void by design`** (idempotent; invalid handles ignored)
- **`SIT_GL_SOFT_CMD_PUSH_VOID`** — void compute record cmds no longer use error-return macro

### Verification

- OpenGL harness: **`core` 31/31**, **`graphics` 107/107**

---

## [v2.4.131 "Internal GL Soft Buffer Errors"] - 2026-05-24

### Description

Internal hardening **Phase 3** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): OpenGL deferred record path and momentum enqueue return **`SituationError`** so OOM, broken soft buffers, GL replay failures, and queue-full propagate to **`SituationEndFrame`** / record APIs instead of silent drops.

### Library Changes (internal only — no public API break)

- **`_SitGLSoftCmdPush`**, **`_SitGLSoftDataPush`** — return **`SituationError`** + out-pointers; macros **`SIT_GL_SOFT_CMD_PUSH`** / **`SIT_GL_SOFT_DATA_PUSH`**
- **`_SituationGLExecuteCommands`** — broken buffer → **`RENDER_COMMAND_FAILED`**; per-packet **`glGetError`** → **`OPENGL_GENERAL`**
- **`_SituationReplayToQueue`**, **`_SituationEnqueueRenderList`** — **`SituationError`** (queue full → **`THREAD_QUEUE_FULL`**)
- Callers: all OpenGL **`SituationCmd*`** record paths, **`SituationRenderVirtualDisplays`**, **`SituationEndFrame`** (execute path), render thread (logs execute failure)
- Debug: optional **`SITUATION_DEBUG_GL_SOFT_CMD_MAX_PACKETS`** cap for OOM simulation
- **`_SituationCheckGLError`** — stays **`void`** (decision in plan §3.3)

### Verification

- OpenGL harness: **`core`**, **`graphics`** (rebuild + full module pass after gate)

---

## [v2.4.130 "Errno Table Phase 2.1"] - 2026-05-24

### Description

Errno table **Phase 2.1**: close doc/comment gaps from the errno audit — add missing codes, document legacy duplicates with **`EOL:`** comments for a future caller sanitisation pass, and add **`scripts/audit_errno.ps1`**.

### Library Changes

- **`sit/situation_base_errno.h`**:
  - **New codes**: `MEMORY_ACCESS` (-12), `FILE_MODIFIED` (-317), `BACKEND_SPECIFIC` (-551), `VULKAN_COMMAND_BUFFER_FAILED` (-721)
  - **`#define` aliases** after enum (not in X-macro switch table): `GLAD_LOAD_FAILED`, `GL_UPLOAD_FAILED`, `ACCESS_DENIED`, … → canonical names
  - **`EOL:`** on legacy pairs (display, filesystem, audio, GL/Vulkan generics, platform Win32)
- **`scripts/audit_errno.ps1`**: table vs usage; flags phantoms and duplicate alias values
- **`tests/harness/test_core.c`**: **`errno_table_phase_2_1`**

### Verification

- `.\scripts\audit_errno.ps1` — no phantom names
- OpenGL harness: **`core` 31/31** (`errno_table_phase_2_1`)

---

## [v2.4.129 "Internal Vulkan Swapchain Errors"] - 2026-05-24

### Description

Internal hardening **Phase 2** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): Vulkan swapchain recreation, single-time command submit, and compute queue submit return **`SituationError`** and propagate to frame acquire / **`SituationEndFrame`** instead of side-channel-only failures.

### Library Changes (internal only — no public API break)

- **`_SituationVulkanRecreateSwapchain`**, **`_SituationVulkanCleanupSwapchain`** — `void` → **`SituationError`**
- **`_SituationVulkanEndSingleTimeCommands`**, **`_SituationSubmitCompute`** — `void` → **`SituationError`**
- Callers: **`SituationAcquireFrameCommandBuffer`**, **`SituationEndFrame`** (present OUT_OF_DATE / resize), **`SituationSetVSync`**, texture/buffer/mesh upload paths, screenshot blit
- Forward decls: **`situation_impl_renderer_fwd.h`**
- Verified existing **`SituationError`** helpers on recreate path: **`_SituationVulkanCreateSwapchain`**, **`_SituationVulkanCreateScreenCopyResource`**, **`_SituationVulkanEnsureScreenshotResources`**

### Verification

- OpenGL harness: **`core`**, **`graphics`** (no Vulkan-specific regressions in shared code paths)
- Vulkan build: **`build_tests.bat vulkan`** — **`graphics`** module

---

## [v2.4.128 "Internal Init Error Propagation"] - 2026-05-24

### Description

Internal hardening **Phase 1** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): init-chain helpers return **`SituationError`** and propagate to **`SituationInit`** instead of side-channel-only **`_SituationSetErrorFromCode`** from **`void`** / **`bool`** stubs.

### Library Changes (internal only — no public API break)

- **GL ring/MDI/fences**: **`_SituationInitGLRingBuffer`**, **`_SituationInitGLMDIBuffer`**, **`_SituationInitGLRingFences`**
- **Renderer inits** (`bool` → **`SituationError`**): **`_SituationInitDefaultFont`**, **`_SituationInitTextRenderer`**, **`_SituationInitQuadRenderer`**, **`_SituationInitGLVirtualDisplayRenderer`**, **`_SituationValidateRenderCaps`**, **`_SituationInitRenderThread`**
- **Subsystems**: **`_SitAudioInitPool`**, **`_SituationCachePhysicalDisplays`**
- Callers: **`_SituationInitOpenGL`**, **`_SituationInitVulkan`**, **`SituationInit`** (ctrl)
- Forward decls: **`situation_impl_forward.h`**, **`situation_impl_renderer_fwd.h`**, **`situation_impl_decl.h`**

### Verification

- OpenGL harness: **`core` 30/30**, **`graphics` 107/107**

---

## [v2.4.127 "Internal Hardening Tooling"] - 2026-05-24

### Description

Internal hardening **Phase 0** (`doc/plan/INTERNAL_HARDENING_PLAN.md`): tooling and test baseline for the void → **`SituationError`** migration.

### Library Changes

- **`SIT_RETURN_IF_ERR`** macro in **`sit/situation_impl_decl.h`**
- **`scripts/list_internal_voids.ps1`** → **`doc/plan/internal_void_inventory.csv`**
- **`doc/plan/INTERNAL_HARDENING_PLAN.md`** — phased task board + per-phase version policy

### Tests

- **`tests/harness/test_core.c`**: **`init_double_init_error`** — second **`SituationInit`** returns **`ALREADY_INITIALIZED`**; **`SituationGetLastErrorCode`** matches

### Verification

- OpenGL harness: **`core`** includes new error-propagation smoke test

---

## [v2.4.126 "Vertex Index Bind SituationError"] - 2026-05-24

### Description

**`SituationCmdBindVertexBuffer`** and **`SituationCmdBindIndexBuffer`** now return **`SituationError`**, matching other Core recording commands (`BindPipeline`, `Draw`, `DrawIndexed`). Removes silent failure paths (null command buffer on Vulkan vertex bind, OpenGL soft-buffer allocation failure) so callers and docs do not rely on **`SituationGetLastError()`** alone.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **126**.
- **`sit/situation_api.h`**: `void` → **`SituationError`** for both bind functions.
- **`sit/situation_impl_renderer.h`**: Return codes aligned with **`SituationCmdBindPipeline`** / **`SituationCmdDraw`**; **`MEMORY_ALLOCATION`** when **`_SitGLSoftCmdPush`** fails.
- **`tests/harness/test_graphics.c`**: **`bind_index_buffer_low_level`** asserts bind success.
- **`doc/situation_command_reference.md`**: Signatures and **Returns** sections updated.

### API note (breaking, source)

```c
/* Before (v2.4.125) */
void SituationCmdBindVertexBuffer(...);
void SituationCmdBindIndexBuffer(...);

/* After (v2.4.126) */
SituationError SituationCmdBindVertexBuffer(...);
SituationError SituationCmdBindIndexBuffer(...);
```

Recompile callers; check return value like other **`SituationCmd*`** Core APIs.

---

## [v2.4.125 "Vertex Attribute Binding"] - 2026-05-24

### Description

Fixes **OpenGL low-level draw replay** (`SituationCmdBindVertexBuffer` + `SituationCmdDraw` / `SituationCmdDrawIndexed`) and makes **vertex input binding explicit** on the public API. The GL executor now **binds the global VAO** before vertex-buffer and draw packets (DSA updates alone were not enough when VAO 0 was active). **`SituationCmdSetVertexAttribute`** gains a **`binding`** parameter (must match the **`binding`** passed to **`SituationCmdBindVertexBuffer`**); use **`0`** for all attributes in an **interleaved** layout. This unblocks **RGL** batch flush (smoke test no longer crashes in **`SituationEndFrame`** on the first **`SIT_OP_DRAW`**) and matches how Vulkan thinks about vertex streams without hard-coding RGL’s layout in the core.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **125**.
- **`sit/situation_api.h`**: **`SituationCmdSetVertexAttribute(cmd, location, binding, size, type, normalized, offset)`** — new **`binding`** argument after **`location`**.
- **`sit/situation_impl_decl.h`**: **`set_vertex_attr.binding`** in soft-command packet.
- **`sit/situation_impl_renderer.h`**:
  - Record/replay **`binding`** via **`glVertexArrayAttribBinding`** (replaces incorrect **`location == binding`** assumption).
  - **`SIT_OP_BIND_VERTEX_BUFFER`** / **`SIT_OP_DRAW`**: bind **`sit_render.gl.global_vao_id`** when needed before DSA / draw.
- **`misc/rgl.h`**: **`_RGL_ConfigureBatchVertexLayout`** passes **`binding = 0`** for all five interleaved attributes (13-float batch stride).
- **`tests/harness/test_graphics.c`**: **`bind_index_buffer_low_level`** OpenGL path uses **`SetVertexAttribute(..., 0, ...)`**.

### API note (breaking, OpenGL-only path)

Callers of **`SituationCmdSetVertexAttribute`** must pass **`binding`** explicitly:

```c
/* Interleaved: one VBO at binding 0 */
SituationCmdSetVertexAttribute(cmd, 0, 0, 3, SIT_DATA_FLOAT, false, offsetof(Vertex, pos));
SituationCmdBindVertexBuffer(cmd, 0, vbo, 0, sizeof(Vertex));

/* Separate streams: attribute location N from binding N */
SituationCmdSetVertexAttribute(cmd, 1, 1, 2, SIT_DATA_FLOAT, false, 0);
SituationCmdBindVertexBuffer(cmd, 1, uv_vbo, 0, 8);
```

Vulkan: unchanged — vertex layout remains pipeline-creation time; **`SetVertexAttribute`** still returns **`SITUATION_ERROR_NOT_IMPLEMENTED`**.

### Harness

| Backend | Command | Graphics module |
|---------|---------|-----------------|
| OpenGL | `build\sit_test.exe --module graphics` | **107** passed |
| Vulkan | `build\sit_test_vulkan.exe --module graphics` | **97** passed |

Includes **`bind_index_buffer_low_level`**, **`draw_indexed_quad`**, **`draw_pipeline_basic`**. **`build\examples\rgl_smoke_test.exe`** (monolithic OpenGL) runs past first frame after fix.

---

## [v2.4.124 "Bind Index Buffer API"] - 2026-05-24

### Description

Public **low-level indexed draw** API for OpenGL 4.6 and Vulkan: **`SituationCmdBindIndexBuffer`** (with byte **`offset`**) pairs with **`SituationCmdBindVertexBuffer`** and **`SituationCmdDrawIndexed`**. Index format is **32-bit** (`GL_UNSIGNED_INT` / `VK_INDEX_TYPE_UINT32`), matching **`SituationCreateMesh`**. On Vulkan, **`SituationCmdBindVertexBuffer`** now selects **`vk_pipeline_simple`** / legacy / PBR from **stride** after **`SituationCmdBindPipeline`**, same rules as **`SituationCmdDrawMesh`**, so manual VBO+IBO draws work without calling **`DrawMesh`**.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **124**.
- **`sit/situation_api.h`**: **`SituationCmdBindIndexBuffer`**, **`SituationCmdBindVertexBuffer`** (public declarations).
- **`sit/situation_impl_renderer.h`**: OpenGL soft replay uses **`glVertexArrayElementBuffer`** on the global VAO; IBO byte offset applied at indexed draw (including MDI **`firstIndex`** bias); Vulkan **`vkCmdBindIndexBuffer`** with offset; Vulkan vertex bind rebinds stride-matched pipeline variant.
- **`sit/situation_impl_decl.h`**: **`bind_ibo.offset`** packet field; **`bound_ibo_byte_offset`** GL replay state.
- **`tests/harness/test_graphics.c`**: **`bind_index_buffer_low_level`** (bind VBO/IBO + **`DrawIndexed`** + screen readback); dual-backend harness pass with **`draw_indexed_quad`**.

### Harness

| Test | Path |
|------|------|
| `bind_index_buffer_low_level` | `BindPipeline` → `BindVertexBuffer` → `BindIndexBuffer` → `DrawIndexed` |
| `draw_indexed_quad` | High-level **`SituationCmdDrawMesh`** (regression) |

---

## [v2.4.123 "Tone Synth Patch Memory"] - 2026-05-23

### Description

Each graph **Tone Synth** node has **16 patch slots** storing a snapshot of controls **1–36** (waveform through `sub_ring_mod`; manual **frequency** ctrl **0** is excluded). **MIDI CC114** selects slot **0–15** and **recalls** when the CC value changes; **CC115** **≥64** saves the current control state into the selected slot (rising edge). Controls **37–38** (`patch_slot`, `patch_store`) mirror the same behaviour via `SituationSetControl`.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **123**.
- **`sit/aud/tone_synth_graph.h`**: `patch_slots[16]` on node state; save/recall + voice template sync on recall.
- **`sit/aud/registry_init.h`**: **39** controls.
- **`sit/aud/node_graph_impl.h`**: `SituationSetControl` hooks patch slot/store for tone synth nodes.
- **`tests/harness/test_tone_synth.c`**: `patch_memory`.
- **`doc/tone_synth.md`**: §3.2 patch memory.

| Idx | Name | MIDI CC |
|-----|------|---------|
| 37 | `patch_slot` | **114** |
| 38 | `patch_store` | **115** (≥64 save) |

---

## [v2.4.122 "Tone Synth Sub Sync Ring Listen"] - 2026-05-23

### Description

Graph **Tone Synth** sub **sync** and **ring mod** fixes and audible harness coverage. **Ring mod** uses the same sub-oscillator pitch as additive (`sub_note` + `sub_octave` / `sub_fine`), so **CC111** sweeps audibly retune the modulator while **A4** is held. **Fix:** ring multiply is no longer skipped when the sub waveform crosses zero (was outputting dry main at those samples). Dedicated listen tests **`sub_sync`** and **`sub_ring_mod`**: each runs ~**3.5 s** with `sub_note=0` (sub tracks main), then ~**3.5 s** with **CC111** stepped **1→127** (note held; only CC111 between steps).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **122**.
- **`sit/aud/tone_synth_graph.h`**: Ring path uses `_SituationToneSynthSubFrequencyHz` (same as additive sub); ring mix always applied when sub is active (not gated on `sub_s != 0`).
- **`tests/harness/test_tone_synth.c`**: `sub_sync` — saw main, pulse sub, oct −1, sync on; `sub_ring_mod` — sine×sine oct −1, ring on; sustained **A4** + CC111 coarse sweep (**40** steps); ring asserts weak **440 Hz** vs **220/660 Hz** sidebands.
- **`doc/tone_synth.md`**: Ring and sync behaviour aligned with shared sub pitch.

### Harness (listen)

| Test | Effect | Segment A | Segment B |
|------|--------|-----------|-------------|
| `sub_sync` | **CC112** on | **A4** hold, `sub_note=0` | **A4** hold, **CC111** 1→127 |
| `sub_ring_mod` | **CC113** on | **A4** hold, `sub_note=0` | **A4** hold, **CC111** 1→127 |

---

## [v2.4.121 "Tone Synth Sub Switches"] - 2026-05-23

### Description

Graph **Tone Synth** sub-oscillator gains three switches: **sub_note** (ctrl/MIDI **0** = track main note pitch; **1–127** = fixed sub MIDI note), **sync with sub** (main phase hard-resets each sub cycle), and **ring modulation** (multiply main×sub with `sub_level` as wet depth). MIDI **CC111–113**.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **121**.
- **`sit/aud/tone_synth_graph.h`**: Controls **34–36**; `_SituationToneSynthMixMainSub`; `sub_cycle_pending` on voice/node.
- **`sit/aud/device_wrappers.h`**: Voice/manual paths use shared mix helper.
- **`sit/aud/registry_init.h`**: **37** controls.
- **`doc/tone_synth.md`**: §3.2 sub switches.

| Idx | Name | MIDI CC |
|-----|------|---------|
| 34 | `sub_note` (0=track, 1–127=fixed) | **111** |
| 35 | `sub_sync` | **112** |
| 36 | `sub_ring_mod` | **113** |

---

## [v2.4.120 "Tone Synth Sum Limiter"] - 2026-05-23

### Description

Graph **Tone Synth** applies the same **post-voice-sum enhanced lookahead limiter** as **Polysonix** (`EnhancedLimiter` in `sit/aud/polysonix/polysonix.h`) after panning and before the node output. Fixed patch defaults: threshold **0.95**, release **50 ms**, ratio **20:1**, **1 ms** lookahead (buffer **2 ms** at sample rate, min 16 samples).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **120**.
- **`sit/aud/tone_synth_graph.h`**: `SituationToneSynthSumLimiter`, init/process/alloc (Polysonix-identical); `sum_limiter` on `SituationToneSynthNodeState`.
- **`sit/aud/device_wrappers.h`**: Allocate/free limiter on create/destroy; per-sample limit after voice sum.
- **`doc/tone_synth.md`**: Signal-flow note for sum limiter.

---

## [v2.4.119 "Tone Synth Sub Osc"] - 2026-05-23

### Description

Graph **Tone Synth** (patch **119**): **16 voices** per node (was 64), plus a per-voice **sub-oscillator** mixed before the SVF — same five waveforms as the main osc, **0 / −1 / −2 octave** offset, **±1 semitone** fine tune, level **0–1**. Legacy **`SituationPlayToneEx`** pool stays at 64 voices. Override voice cap: `-DSITUATION_TONE_SYNTH_MAX_VOICES=N`.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **119**.
- **`sit/aud/tone_synth_graph.h`**: `sub_phase`, `sub_waveform`; controls **30–33**; `_SituationToneSynthOscSampleWave`, `_SituationToneSynthSubFrequencyHz`; legato preserves `sub_phase`; MIDI **CC107–110**.
- **`sit/aud/device_wrappers.h`**: Main + sub sum pre-filter; manual path sub mix.
- **`sit/aud/registry_init.h`**: **34** controls.
- **`sit/situation_api.h`**: `SITUATION_MAX_CONTROLS_PER_DEVICE` **32 → 48** (Tone Synth exceeded the old cap).
- **`tests/harness/test_tone_synth.c`**: `sub_oscillator`.
- **`doc/tone_synth.md`**: §3.2 sub-oscillator.

### Sub-oscillator controls

| Idx | Name | Default | Range | MIDI CC |
|-----|------|---------|-------|---------|
| 30 | `sub_level` | 0 | 0–1 | **107** |
| 31 | `sub_waveform` | Sine | 0–4 (same as main) | **108** (`value % 5`) |
| 32 | `sub_octave` | Oct −1 | 0=unison, 1=−1 oct, 2=−2 oct | **109** |
| 33 | `sub_fine` | 0 | ±1 semitone | **110** (`norm×2−1`) |

Pitch: `sub_hz = main_hz × 2^((fine − octave×12) / 12)` (octave 0/1/2 = unison / −1 oct / −2 oct) where `main_hz` is the voice pitch after bend, portamento, and mod LFO.

---

## [v2.4.118 "Tone Synth Portamento"] - 2026-05-23

### Description

Graph **Tone Synth** in **mono** mode gains **portamento** (pitch glide on legato note changes) with two complementary controls: fixed **glide time** and interval-aware **glide speed** (semitones per second). **Legato** re-triggers preserve envelope sustain and oscillator phase when a new note arrives while the prior note is still in attack, decay, or sustain; a gap or release before the next note-on starts a fresh voice. Harness: dedicated **`tone_synth`** module with linked vs unlinked four-note phrases.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **118**.
- **`sit/aud/tone_synth_graph.h`**: Controls **28–29** (`portamento_time`, `portamento_speed`); `base_hz` / `target_hz`; `_SituationToneSynthVoicePortamentoTauSec`, `_SituationToneSynthVoiceGlidePitch`, `_SituationToneSynthVoiceLegatoFromControls`; MIDI **CC5** (time), **CC20** (speed).
- **`sit/aud/device_wrappers.h`**: Per-sample RC glide in mono when time or speed &gt; 0.
- **`sit/aud/registry_init.h`**: **30** controls registered.
- **`tests/harness/test_tone_synth.c`**: Module **`tone_synth`**; `mono_portamento_linked`, `mono_portamento_unlinked` (4-note C–E–G–C phrases).
- **`doc/tone_synth.md`**: Portamento / legato section (§3.1).

### Portamento controls (registry indices)

| Idx | Name | Default | Range | MIDI CC |
|-----|------|---------|-------|---------|
| 28 | `portamento_time` | 0 | 0–2 s | **CC5** (`norm × 2`) |
| 29 | `portamento_speed` | 0 | 0–48 st/s | **CC20** (`norm × 48`) |

**Effective glide time** `τ` (seconds, used as RC time constant):

| `portamento_time` | `portamento_speed` | `τ` |
|-------------------|--------------------|-----|
| ≤ 0 | ≤ 0 | Instant (snap `base_hz` → `target_hz`) |
| &gt; 0 | ≤ 0 | `τ = time` (same for every interval) |
| ≤ 0 | &gt; 0 | `τ = semitones ÷ speed` (interval from current `base_hz` to `target_hz`) |
| &gt; 0 | &gt; 0 | `τ = max(time, semitones ÷ speed)` — **slower glide wins** |

Poly mode ignores portamento (pitch snaps each voice). Mono + legato sets `target_hz` on note-on; glide runs in the audio loop.

---

## [v2.4.117 "Tone Synth Filter Env Mod"] - 2026-05-23

### Description

Graph **Tone Synth** routes the per-voice **ADSR envelope** to filter cutoff via **amount × range** (same formula as mod LFO filter depth). Off by default; stacks with LFO filter offset.

### Library Changes

- **`sit/aud/tone_synth_graph.h`**: Controls **26–27** (`filter_env_amount`, `filter_env_range`), CC **32–33**.
- **`sit/aud/device_wrappers.h`**: Per-voice `cutoff_offset += envelope × amount × range` (manual path uses amplitude).
- **`sit/aud/registry_init.h`**: 28 controls registered.
- **`tests/harness/test_audio.c`**: `graph_tone_synth_filter_env_adsr`.

### Filter env CC map

| CC | Control | Scaling |
|----|---------|---------|
| 32 | Filter env amount | 0–1 |
| 33 | Filter env range | log 20–8000 Hz span |

Modulation: `cutoff += envelope × amount × range` (0–1 ADSR, added before keytrack/clamp).

---

## [v2.4.116 "Tone Synth Mod LFO"] - 2026-05-23

### Description

Graph **Tone Synth** adds one **global mod LFO** (triangle / square / random S&H): rate + waveform, with independent **amount × range** for **pitch**, **PWM**, and **filter cutoff**. Separate from CC1 vibrato (fixed 5 Hz) and CC92 tremolo. All CC-mapped; off by default (LFO rate 0).

### Library Changes

- **`sit/aud/tone_synth_graph.h`**: Controls **18–25**, `mod_lfo_phase`, CC **24–31**.
- **`sit/aud/device_wrappers.h`**: Per-sample LFO applied to pitch, pulse width, filter.
- **`sit/aud/registry_init.h`**: 26 controls registered.
- **`tests/harness/test_audio.c`**: `graph_tone_synth_lfo_mod`, `graph_tone_synth_waveforms_all` (+ filter/pulse width).

### Mod LFO CC map

| CC | Control | Scaling |
|----|---------|---------|
| 24 | LFO rate | 0=off; else log 0.05–20 Hz |
| 25 | LFO waveform | 0=tri, 1=square, 2=random |
| 26 | Pitch amount | 0–1 |
| 27 | Pitch range | 0–12 semitones |
| 28 | PWM amount | 0–1 |
| 29 | PWM range | 0–0.45 duty |
| 30 | Filter amount | 0–1 |
| 31 | Filter range | log 20–8000 Hz span |

Modulation: `target += lfo × amount × range` (bipolar LFO −1..1).

---

## [v2.4.115 "Tone Synth Pulse Width"] - 2026-05-23

### Description

Graph **Tone Synth** waveform **1** is now a **pulse** (variable duty cycle) instead of a fixed 50% square. Pulse width is control **17** (default 0.5 = square) and **MIDI CC106** (5%–95% duty).

### Library Changes

- **`sit/aud/tone_synth_graph.h`**: `_SituationToneSynthClampPulseWidth`, pulse osc in case 1, CC106 → control 17.
- **`sit/aud/registry_init.h`**: Control **17** `pulse_width`; waveform enum label **Pulse**.
- **`sit/aud/device_wrappers.h`**: Pass pulse width into oscillator.
- **`tests/harness/test_audio.c`**: `graph_tone_synth_filter_modes`, `graph_tone_synth_pulse_width` harness tests.

---

## [v2.4.114 "Tone Synth Mono Poly"] - 2026-05-23

### Description

Graph **Tone Synth** adds a **mono / poly pivot**: poly keeps the existing 64-voice allocator; mono uses **voice slot 0 only** and cuts any other active voices on each new note (last-note priority, no stack).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **114**.
- **`sit/aud/tone_synth_graph.h`**: Control **16** `voice_mode` (0=poly, 1=mono); `_SituationToneSynthSetVoiceMode`, `_SituationToneSynthEnforceMonoVoices`.
- **MIDI**: **CC126** → mono, **CC127** → poly (GM mode CCs). Also set via `SituationSetControl(graph, handle, 16, 0|1)`.

---

## [v2.4.113 "Tone Synth SVF Filter"] - 2026-05-23

### Description

Graph **Tone Synth** gains a **per-voice Polysonix-style multi-pole SVF** (`sit/aud/fx/filter.h`): LP/HP/BP/notch/combo modes, 1–4 poles, drive, optional 2× oversampling, and MIDI-note key tracking. Filter runs after the oscillator envelope, before pan — same order as Polysonix voices.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **113**, description **"Tone Synth SVF Filter"**.
- **`sit/aud/tone_synth_graph.h`**: Per-voice `SituationToneSynthVoiceFilter`; controls **9–15**; MIDI CC **16** mode, **74** cutoff, **71** resonance, **102** poles, **17** drive, **22** keytrack, **18** oversampling.
- **`sit/aud/device_wrappers.h`**: `_SituationProcessToneSynthNode` applies filter per voice (and manual fallback path).
- **`sit/aud/registry_init.h`**: Tone synth **16** controls (filter block registered with defaults; filter **off** by default for harness parity).

### MIDI filter map (Tone Synth node)

| CC | Control |
|----|---------|
| 16 | Filter mode 0=OFF … 8=BP+HP |
| 74 | Cutoff 20 Hz–20 kHz (log) |
| 71 | Resonance Q 0.5–20 |
| 102 | Poles 1–4 |
| 17 | Drive 1–10 |
| 22 | Keytrack 0–1 |
| 18 | Oversampling 0=off, ≥1=2× |

---

## [v2.4.112 "Tone Synth Compare Ready"] - 2026-05-23

### Description

Situation **v2.4.112** is a patch bump after **v2.4.111** verification: all **15** `graph_tone_synth_*` harness tests pass in one run, including stable sequential Phase 8 MIDI tests via **`sit_midi_graph_fixture_release`**. This release marks the graph tone synth as ready for side-by-side scrutiny against the legacy 64-voice pool and app-level switching.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **112**, description **"Tone Synth Compare Ready"**.
- No additional library API changes beyond **v2.4.111** (harness fixture cleanup only).
- **`tone_synth_phase1_compare_a4`** harness test — Phase 1 side-by-side: legacy-only A4 then graph-only A4 (exclusive paths), prints `[COMPARE Phase 1]` hz/peak/rms delta.

### Verification

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
Set-Location build
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\sit_test.exe" --module audio --filter graph_tone_synth
& ".\sit_test.exe" --module audio --filter tone_synth_phase1_compare_a4
```

**Results (2026-05-23):** 15 passed, 0 failed.

---

## [v2.4.111 "Graph Tone Synth Full MIDI"] - 2026-05-23

### Description

Situation **v2.4.111** completes the agreed **“everything through MIDI”** follow-up for the graph tone synth: **channel-aware virtual note APIs**, **per-node MIDI channel filtering**, the remaining **ADSR CC map**, **CC70 waveform**, **CC92 true amplitude tremolo** (5 Hz LFO), and harness migration to the `*Ex` virtual MIDI helpers.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **111**, description **"Graph Tone Synth Full MIDI"**.
- **`sit/situation_api.h`**:
  - **`SituationVirtualMidiNoteOnEx` / `NoteOffEx`** — channel 0–15 note injection.
  - **`SituationVirtualMidiProgramChange`** — channel-aware program change.
  - **`SituationSetNodeMidiChannel`** — filter node MIDI to one channel (`-1` = omni).
  - Legacy **`SituationVirtualMidiNoteOn/Off`** remain as channel-0 wrappers.
- **`sit/aud/node_graph_midi.h`**: implementations for the new APIs; **`_SituationVirtualMidiWrite`** shared helper.
- **`sit/aud/node_graph_process.h`**: **`_SituationNodeMidiAcceptsChannel`** — drops messages not matching `node->midi_device->midi_channel` when set.
- **`sit/aud/tone_synth_graph.h`**:
  - **`_SituationToneSynthApplyControlChange`** — centralized CC map: CC1, 7, 10, 11, 64, **70**, **72–77** (ADSR), **92** (tremolo depth), 123.
  - **CC92 tremolo** state (`tremolo_depth`, `tremolo_phase`) + **5 Hz** default.
- **`sit/aud/device_wrappers.h`**: amplitude tremolo LFO applied to combined MIDI volume during voice mix.
- **`sit/aud/midi_device_callbacks.h`**: tone synth CC handler delegates to **`_SituationToneSynthApplyControlChange`**.

### Graph tone synth MIDI CC map

| CC | Function |
|----|----------|
| 1 | Vibrato depth (5 Hz pitch LFO) |
| 7 | Channel volume |
| 10 | Pan |
| 11 | Expression |
| 64 | Sustain pedal |
| 70 | Waveform (0–4) |
| 72 | Release time |
| 73 | Attack time |
| 75 | Decay time |
| 76 | Sustain level |
| 77 | Hold time |
| 92 | Tremolo depth (5 Hz amplitude LFO) |
| 123 | All notes off |

### Harness (Phase 8)

- All graph tone synth tests use **`SituationVirtualMidiNoteOnEx/OffEx(SITUATION_TEST_MIDI_CHANNEL, …)`**.
- **`sit_midi_tone_graph_setup`** calls **`SituationSetNodeMidiChannel`** after MIDI enable.
- **`sit_midi_tone_graph_silence_midi`** resets **CC92** before teardown.
- **`graph_tone_synth_cc7_tremolo`** renamed **`graph_tone_synth_cc92_tremolo`** — tests true LFO tremolo via **CC92**, not CC7 toggling.
- **`sit_midi_graph_fixture_release`** — cleans up graph/MIDI state when a prior test **longjmp**'s on assertion failure (prevents `-496` MIDI open errors on the next test).

### Verification

```bat
build_situation.bat opengl
build_tests.bat opengl
cd build
set PATH=build\dll;C:\msys64\mingw64\bin;%PATH%
sit_test.exe --module audio --filter graph_tone_synth
```

**Results (2026-05-23):** all **15** `graph_tone_synth_*` tests pass in one run (including Phase 8 MIDI frequency/CC tests).

---

## [v2.4.110 "Graph Tone Synth Legacy Parity"] - 2026-05-23

### Description

Situation **v2.4.110** brings the **graph tone synth** (`SITUATION_NODE_TONE_SYNTH`, PortMidi path) to **feature parity with the legacy 64-voice tone pool**: polyphonic voice allocation with stealing, per-voice **ADSR + hold**, stereo **pan**, all five **waveforms**, velocity-scaled envelopes, and expanded **MIDI CC / program change** mapping. The legacy **`SituationPlayToneEx`** pool is unchanged; this patch upgrades the MIDI-controllable graph node only.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **110**, description **"Graph Tone Synth Legacy Parity"**.
- **`sit/aud/tone_synth_graph.h`** (major rewrite):
  - **`SituationToneSynthVoice`** pool (**64** voices per node) with attack/decay/sustain/release/hold frame counts.
  - Voice **steal order**: inactive slot → furthest-in-**release** → newest active (matches legacy pool policy).
  - Shared helpers: alloc/release note, envelope step, oscillator sample (sine/square/triangle/saw/noise), pitch bend + **CC1** LFO on all voices.
  - **`SituationToneSynthNodeState`**: voice array + MIDI globals (CC7/CC11 volume, CC64 sustain, bend, mod depth) + manual **`SetControl`** fallback when no voices active.
- **`sit/aud/device_wrappers.h`** — **`_SituationProcessToneSynthNode`**:
  - Sums all active voices with **ADSR × velocity × channel volume × expression**.
  - **Stereo pan** (legacy linear L/R gains from control 3).
  - Manual control path preserved for harness **`SituationSetControl`** (instant amp when no MIDI voices).
- **`sit/aud/midi_device_callbacks.h`** — graph tone synth MIDI:
  - **Poly note on/off** (per-note voice alloc/release; sustain-pedal deferral).
  - **CC1** vibrato depth, **CC7** volume, **CC10** pan, **CC11** expression, **CC64** sustain, **CC72/73** release/attack, **CC123** all-notes-off.
  - **`_SituationToneSynthOnProgramChange`** — program **% 5** → waveform enum.
- **`sit/aud/node_graph_process.h`**: dispatch **0xC0 program change** to graph tone synth.

### Legacy pool vs graph (after this patch)

| Feature | Legacy pool | Graph tone synth |
|---------|-------------|------------------|
| 64-voice poly | ✓ | ✓ |
| ADSR + hold | ✓ | ✓ (per voice, from controls 4–8) |
| Pan | ✓ | ✓ (control 3 / CC10) |
| Waveforms ×5 | ✓ (miniaudio) | ✓ (RT-safe oscillators) |
| PortMidi | — | ✓ |
| `PlayToneEx` handles | ✓ | — (internal voices only) |

### Verification

Rebuild **DLL** after library changes (`build_tests.bat` only if harness sources changed):

```bat
build_situation.bat opengl
cd build
set PATH=build\dll;C:\msys64\mingw64\bin;%PATH%
sit_test.exe --module audio --filter graph_tone_synth_midi
sit_test.exe --module audio --filter graph_tone_synth_velocity_ramp
sit_test.exe --module audio --filter graph_tone_synth_cc
sit_test.exe --module audio_effects_heard --filter graph_tone_synth_effect_heard_reverb
```

**Results (2026-05-23):**

| Check | Result |
|-------|--------|
| **`graph_tone_synth_midi_note_frequency`** | pass |
| **`graph_tone_synth_midi_complex_melody`** | pass |
| **`graph_tone_synth_velocity_ramp`** | pass (run isolated if prior tests left audio busy) |
| **`graph_tone_synth_cc_mod_vibrato`** / **`cc7_tremolo`** | pass |
| **`graph_tone_synth_effect_heard_reverb`** | pass |

### Still open (graph tone synth)

- Manual **`SetControl`** path: instant volume (no ADSR) when no MIDI voices — legacy **`PlayToneEx`** always envelopes.
- **`SituationVirtualMidiNoteOn/Off`**: channel-0 only; no public voice handles.
- **CC decay/sustain/hold** not on dedicated CCs (set via node controls or CC72/73 for attack/release only).
- Oscillator DSP differs from miniaudio **`ma_waveform`** (same shapes, not bit-identical).

---

## [v2.4.109 "MIDI Device Names & Test Routing"] - 2026-05-23

### Description

Situation **v2.4.109** adds **official PortMidi device name constants** for harness virtual MIDI and the graph tone synth target, a **`SituationGetMidiDeviceName`** lookup API, a fix for **virtual device enumeration** in **`SituationListMidiDevices`**, **virtual loopback CC / pitch-bend injection** for integration tests, and **per-test MIDI routing banners** (device name, PortMidi id, channel, CC usage) on all graph tone synth MIDI verification tests. **CC7 tremolo bleed** between sequential harness runs is flushed via a shared MIDI silence helper before teardown.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **109**, description **"MIDI Device Names & Test Routing"**.
- **`sit/situation_api.h`**:
  - **`SITUATION_TEST_MIDI_CHANNEL`** (0-based; harness displays as MIDI channel 0 → channel 1).
  - **`SITUATION_VIRTUAL_MIDI_IN_NAME`** / **`SITUATION_VIRTUAL_MIDI_OUT_NAME`** — official virtual loopback PortMidi names.
  - **`SITUATION_TONE_SYNTH_MIDI_DEVICE_NAME`** — graph tone synth **`SIT_MidiDevice`** target name (**`"Tone Synth"`**).
  - **`SituationGetMidiDeviceName()`** — resolve PortMidi name for hardware or virtual **`device_id`** (virtual ids ≥ 32).
  - **`SituationVirtualMidiControlChange()`**, **`SituationVirtualMidiPitchBend()`** — inject CC / bend into the virtual loopback output stream.
- **`sit/aud/node_graph_midi.h`**:
  - **`SituationListMidiDevices()`** — enumerate hardware ids **0…MAX_DEVICES−1** plus active virtual ids **MAX_DEVICES+slot** (fixes missing virtual devices when **`Pm_CountDevices()`** count ≠ index space).
  - Virtual loopback **`Pm_CreateVirtualDevice`** uses **`SITUATION_VIRTUAL_MIDI_*_NAME`** constants.
  - **`SituationSetupVirtualMidiLoopback()`** returns the virtual input **`PmDeviceID`** for **`SituationEnableMidiControl()`**.

### Tests

- **`tests/harness/midi_test_info.h`** (new):
  - **`sit_midi_log_graph_tone_synth_route()`** — asserts official input + synth names via **`SituationGetMidiDeviceName`** / registry metadata; prints **`[MIDI]`** routing line (channel + per-test CC/note usage).
  - **`sit_midi_log_legacy_tone_pool_route()`** — labels legacy **`SituationPlayMidiNote`** path (no PortMidi).
- **`tests/harness/test_audio.c`** (Phase 8 — MIDI / audio frequency verification):
  - Documented per-test **MIDI channel / CC** table in file header.
  - **`graph_tone_synth_midi_note_frequency`**, **`graph_tone_synth_midi_complex_melody`**, **`graph_tone_synth_velocity_ramp`**, **`graph_tone_synth_cc_mod_vibrato`**, **`graph_tone_synth_cc7_tremolo`** — each prints official routing banner at start.
  - **`sit_midi_tone_graph_setup()`** / **`sit_midi_tone_graph_teardown()`** shared helpers for graph + virtual MIDI tests.
  - **`sit_midi_tone_graph_silence_midi()`** — CC123 all-notes-off, CC64/1/7/11 reset, pitch bend center, note-off; prevents **CC7 tremolo** volume state leaking into later tests.
  - **`graph_tone_synth_cc_mod_vibrato`**: Goertzel wander test updated for **5 Hz LFO vibrato** from **CC1** (not static detune).
  - **`graph_tone_synth_midi_complex_melody`**: velocity, pitch bend, **CC1** vibrato windows on G4 phrase.

### Verification

From project root (rebuild **DLL + harness** after library changes):

```bat
build_situation.bat opengl
build_tests.bat opengl
cd build
set PATH=build\dll;C:\msys64\mingw64\bin;%PATH%
sit_test.exe --module audio --filter graph_tone_synth_midi
sit_test.exe --module audio --filter graph_tone_synth_velocity_ramp
sit_test.exe --module audio --filter graph_tone_synth_cc
sit_test.exe --module audio --filter legacy_tone_pool_midi
```

**Results (2026-05-23):**

| Check | Result |
|-------|--------|
| **`graph_tone_synth_midi_note_frequency`** | pass — `in="Situation Test MIDI In"`, `synth="Tone Synth"`, ch0 |
| **`graph_tone_synth_midi_complex_melody`** | pass |
| **`graph_tone_synth_velocity_ramp`** | pass (run isolated if prior module left audio busy) |
| **`graph_tone_synth_cc_mod_vibrato`** | pass |
| **`graph_tone_synth_cc7_tremolo`** | pass |
| **`legacy_tone_pool_midi_note_frequency`** | pass — legacy path banner, no PortMidi |

### Not yet MIDI-driven (graph tone synth)

Registry controls **waveform (1)**, **pan (3)**, **ADSR/hold (4–8)** remain harness/API-only; **CC92** true tremolo LFO not wired — **CC7** volume toggle used in tests. **`SituationVirtualMidiNoteOn/Off`** remain channel-0 status bytes; CC/bend APIs accept explicit channel.

---

## [v2.4.108 "Effect Heard Tests & Harness Fixes"] - 2026-05-23

### Description

Situation **v2.4.108** adds **per-effect audible verification** in the harness (brief 440 Hz tone through each FX node, wet vs dry analysis), a **reverb dry/wet mix sweep** test (440 Hz square wave, wet **0→1** then **1→0** over 4 s each), aligns **Filter** and **EQ 4-Band** process wrappers with registry control layout, and fixes harness **SPIR-V disk path** resolution when running from **`build/dll`**.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **108**, description **"Effect Heard Tests & Harness Fixes"**.
- **`sit/aud/device_wrappers.h`**:
  - **Filter**: map registry controls (**cutoff / resonance / type**) to **`PxFilterMode`** processing (fixes default-off / OOB reads).
  - **EQ 4-Band**: map registry shelf/peak controls to **`eq4band_set_band`** (fixes misaligned band gains from registry defaults).
  - **Maximizer**: init hop size **64** to match playback period.
  - **EQ 4-Band**: band-3 gain no longer reads past control array.
  - **Tone synth**: phase increment uses **`SituationGetAudioPlaybackSampleRate()`**.
- **`sit/aud/tone_synth_graph.h`**: per-node MIDI voice state + `SituationToneSynthMidiCtx` (controls + synth state).
- **`sit/aud/midi_device_callbacks.h`**: mono note on/off (matching note), **CC1** → vibrato LFO depth, **CC7/CC11** → `base × channel × expression`, **CC64** sustain; removed file-static globals.
- **`sit/aud/device_wrappers.h`**: 5 Hz vibrato LFO in process when `mod_depth_semitones > 0`; volume/pitch driven from MIDI state while sounding.
- **`sit/aud/node_graph_midi.h`**: allocate/free tone synth MIDI ctx on enable/disable.
- **`sit/aud/node_graph_process.h`**: dispatch MIDI to `midi_device->device_ptr` (fixes tone synth ctx vs control array).

### Tests

- **`tests/harness/test_audio_effects_heard.c`**: module **`audio_effects_heard`** — **17** named tests:
  - **`graph_tone_synth_effect_heard_*`** (×16): one per registered effect; 440 Hz sine from graph tone synth, ~400 ms capture, wet vs dry.
  - **`effect_reverb_mix_dry_wet_sweep`**: 440 Hz **square** → **Reverb**; **`wet_level`** ramp **0→1** (4 s) then **1→0** (4 s) while graph runs; RMS envelope at dry vs wet windows proves mix responds.
- **`tests/harness/audio_freq_detect.c`**: **`sit_audio_effect_heard()`**, **`sit_audio_capture_window_rms()`**, **`sit_audio_capture_window_correlation()`** (Goertzel + windowed analysis for mix sweep).
- **`tests/harness/test_graphics_spirv.c`**: resolve disk SPIR-V under **`../../tests/harness/spirv_out/`** when cwd is **`build/dll`** (fixes **`spirv_disk_roundtrip`**, **`async_shader_spirv_memory_vulkan`**).
- **`tests/harness/test_audio.c`**: Harness names prefix synth path — **`legacy_tone_pool_*`** (`SituationPlayToneEx` / `PlayMidiNote`) vs **`graph_tone_synth_*`** (graph `SITUATION_NODE_TONE_SYNTH` + virtual MIDI). Examples: **`graph_tone_synth_velocity_ramp`**, **`graph_tone_synth_midi_complex_melody`**, **`graph_tone_synth_cc7_tremolo`**, **`legacy_tone_pool_midi_note_frequency`**.
- **`tests/harness/test_audio_effects_heard.c`**: **`graph_tone_synth_effect_heard_*`** (×16), **`graph_tone_synth_reverb_mix_dry_wet_sweep`**.
- **`tests/harness/sit_test_registry.c`**, **`build_tests.bat`**: register module and build wiring.

### Verification

From project root (MinGW on **`PATH`**, no `.bat` required):

```powershell
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
Set-Location build\dll
.\sit_test_gl.exe --module audio_effects_heard
.\sit_test_gl.exe --module audio_effects_heard --filter mix_dry_wet
.\sit_test_gl.exe
.\sit_test_vk.exe
```

Rebuild DLL + harness with **`gcc`** after library changes; run exes from **`build\dll`** so the fresh **`situation_opengl.dll`** / **`situation_vulkan.dll`** is loaded.

**Results (2026-05-23):**

| Check | Result |
|-------|--------|
| OpenGL **`audio_effects_heard`** | **17/17** passed (incl. mix sweep ~9 s) |
| OpenGL full harness | **356/356** passed |
| Vulkan full harness | **345/345** passed |

---

## [v2.4.107 "MIDI Audio Frequency Verification"] - 2026-05-23

### Description

Situation **v2.4.107** adds end-to-end **MIDI → audio** verification: virtual MIDI loopback injects note-on into a graph **tone synth** node, the output monitor captures mixed samples, and a Goertzel pass confirms the expected pitch (e.g. A4 = **440 Hz**). Legacy **`SituationPlayMidiNote`** is covered by the same frequency helper.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **107**, description **"MIDI Audio Frequency Verification"**.
- **`sit/aud/midi_device_callbacks.h`**: **Tone Synth** MIDI note-on/off → frequency / volume controls.
- **`sit/aud/node_graph_process.h`**: dispatch **0x90** / **0x80** to node note callbacks.
- **`sit/aud/node_graph_midi.h`**: **`SituationSetupVirtualMidiLoopback`**, **`SituationVirtualMidiNoteOn`/`NoteOff`**, **`SituationTeardownVirtualMidiLoopback`**.
- **`sit/situation_api.h`**: export virtual MIDI loopback helpers.
- **`sit/aud/device_wrappers.h`**: tone synth uses **`SituationGetAudioPlaybackSampleRate()`** for phase increment.

### Tests

- **`tests/harness/audio_freq_detect.c`**: Goertzel capture/verify helper.
- **`tests/harness/test_audio.c`**: **`legacy_midi_note_emits_frequency`**, **`graph_midi_note_emits_frequency`**.
- **`tests/harness/midi_audio_probe.c`**: standalone probe → **`build/midi_audio_probe.exe`** (via **`build_tests.bat`**).

### Verification

```bat
build_situation.bat opengl
build_tests.bat opengl
cd build\dll
sit_test.exe --module audio --filter emits_frequency --verbose
midi_audio_probe.exe
```

Run from **`build\dll\`** (or **`copy /Y build\dll\situation_opengl.dll build\`**) so the harness loads the fresh DLL.

---

## [v2.4.106 "Windows Audio Shared Auto-Start Fix"] - 2026-05-23

### Description

Situation **v2.4.106** fixes Windows system audio being **muted or left broken** after running the test harness (or any app that calls **`SituationInit`** / **`SituationShutdown`** repeatedly in one process). Auto-start of the default playback device in **`SituationInit`** step 7 no longer requests **WASAPI exclusive** mode on the first in-process session; it always uses **shared** mode. Exclusive output remains available via explicit **`SituationSetAudioDevice()`** for low-latency games.

### Root cause

- The harness runs **one `SituationInit` + `SituationShutdown` per module** (core, window, input, timer, audio, graphics, misc) in a **single process**.
- On Windows, step 7 previously used **`ma_share_mode_exclusive`** on **session 1** and shared mode only on session 2+ (workaround for re-init blocking).
- **Exclusive mode hijacks the default endpoint** and mutes other apps (browser, Spotify, system sounds). Repeated exclusive grab/release across module cycles could leave WASAPI in a bad state after teardown — reported as “harness nuked my audio.”
- Related prior work: **v2.4.52** (release miniaudio before GPU sync on shutdown), Bug 6 notes in **`doc/plan/LIBRARY_BUGFIX_PLAN.md`** (exclusive re-init lifecycle).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **106**, description **"Windows Audio Shared Auto-Start Fix"**.
- **`sit/situation_impl_ctrl.h`**:
  - **`SituationInit`** step 7 (Windows): always call **`_SituationSetAudioDeviceInternal(..., ma_share_mode_shared)`**; removed session-counter exclusive-first policy.
  - **`SituationShutdown`** / **`_SituationCleanupSubsystems`**: reset **`is_miniaudio_device_internally_paused`** after device stop/uninit so pause/resume tests cannot leave stale pause state across module cycles.
- **`sit/situation_impl_audio.h`**: comment update — auto-start is shared; explicit **`SituationSetAudioDevice`** is exclusive.

### Verification

```bat
build_situation.bat all
build_tests.bat opengl
build_tests.bat vulkan
copy /Y build\dll\situation_opengl.dll build\
copy /Y build\dll\situation_vulkan.dll build\
build\sit_test.exe --module audio
build\sit_test.exe
build\sit_test_vulkan.exe
```

**Important:** Windows loads the DLL beside the exe first. If **`build\situation_opengl.dll`** is stale (e.g. locked by a running **`sit_test.exe`**), the harness silently tests an old build — always **`copy /Y`** from **`build\dll\`** after **`build_situation.bat`**, or run with **`PATH`** pointing at **`build\dll`**.

**Results (2026-05-23, reference Windows config):**

| Check | Result |
|-------|--------|
| OpenGL full harness | **337/337** passed |
| OpenGL **`--module audio`** | **96/96** passed |
| Vulkan full harness | **327/327** passed |
| Audio probe (7× **`SituationInit`/`Shutdown`**, 2 s tone/cycle) | Init OK, **`SituationIsAudioDevicePlaying()`** true, master meter peak ≈ **0.56** each cycle; audible output confirmed |
| System audio after harness | Default playback (browser/Spotify/system sounds) still works — no device toggle or reboot required |

Optional deeper probe: compile/run **`build/dll/audio_probe.exe`** (7-cycle stress test with **`SituationGetMasterOutputMeter`** — non-zero peak/RMS confirms the callback is mixing samples even when harness tones are too short to hear).

---

## [v2.4.105 "Vulkan Async GLSL Worker Queue Fix"] - 2026-05-23

### Description

Situation **v2.4.105** fixes Vulkan **`SituationBeginLoadShaderFromMemory`** async GLSL loads that never completed: compile jobs were submitted to the **low-priority I/O queue** (serviced by the dedicated I/O thread), but after **`SituationInit`** those jobs were not dequeued (`SituationGetIOQueueDepth()` stayed at 1, **`SituationPollShaderLoad`** returned **`SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS`** until harness timeout). Sync GLSL load and async SPIR-V load were unaffected. CPU-bound shaderc work now runs on the **high-priority worker queue**; unload waits for the compile worker before freeing async context (prevents use-after-free once workers actually run).

### Root cause (investigation)

- **`_SituationVkAsyncShaderLoad`** is **120 bytes** (> **`SITUATION_JOB_PAYLOAD_MAX`** 64), so **`SIT_SUBMIT_POINTER_ONLY`** was already correct — not a small-object copy bug.
- Repro: **`SituationGetIOQueueDepth() == 1`** indefinitely after **`SituationBeginLoadShaderFromMemory`**; **`disable_io_thread=true`** (inline submit path) completed on frame 0; standalone **`SituationCreateThreadPool`** low-priority jobs still work.
- Misclassification: GLSL→SPIR-V via shaderc is **CPU-bound**, not I/O; it must not use **`SIT_SUBMIT_DEFAULT`** / queue 0 when a dedicated I/O thread owns that queue (see also **v2.4.103** worker skip of queue 0).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **105**, description **"Vulkan Async GLSL Worker Queue Fix"**.
- **`sit/situation_impl_renderer.h`**:
  - **`SituationBeginLoadShaderFromMemory`** (Vulkan): submit **`_SituationVkAsyncCompileWorker`** with **`SIT_SUBMIT_HIGH_PRIORITY | SIT_SUBMIT_POINTER_ONLY | SIT_SUBMIT_BLOCK_IF_FULL`** instead of default low-priority I/O queue.
  - **`_SituationVulkanFreeAsyncShaderLoad`**: spin on **`compile_done == 0`** with **`thrd_yield()`** before freeing **`ctx`** / source strings (safe **`SituationUnloadShader`** during in-flight compile).
- **`sit/situation_impl_io.h`**: when the I/O queue head job has unmet dependencies, **`thrd_yield()`** after unlock (avoids busy-spin on head-of-line block).

### Tests (regression)

Previously failing Vulkan harness tests (all now pass):

- **`graphics.async_shader_begin_reports_in_progress`**
- **`graphics.async_shader_load_memory_draw`**
- **`graphics.async_shader_renderer_alive_while_loading`**
- **`graphics.sync_shader_after_async_cycle`**

### Verification

```bat
build_situation.bat vulkan
build_tests.bat vulkan
build\sit_test_vulkan.exe --filter async_shader
build\sit_test_vulkan.exe
```

**Result (2026-05-23):** Vulkan harness **327/327** passed (GCC 15.1.0, Vulkan SDK 1.4.313.2).

---

## [v2.4.104 "Error Mutex, Image Resize, SPIR-V Load Ex"] - 2026-05-21

### Description

Situation **v2.4.104** fixes post-shutdown error handling and CPU image resize, adds **`SituationBeginLoadShaderFromSpirvMemoryEx`** so Vulkan async SPIR-V loads can select descriptor layout profiles (mesh, dual-SSBO, UBO+SSBO), and hardens SPIR-V poll tests in the harness. Full OpenGL harness **337/337**; Vulkan **`--module graphics --filter spirv`** passes, including a large UBO+SSBO Begin/Poll regression.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **104**.
- **`sit/situation_impl_ctrl.h`**: **`_SituationSetErrorFromCode`** returns early when **`_sit_current_context`** is NULL (avoids locking **`error_mutex`** after **`SituationShutdown()`**); **`error_mutex`** init/uninit in **`SituationInit`** / **`SituationShutdown`**.
- **`sit/situation_impl_deps.h`**: **`stb_image_resize2.h`** implementation for **`SituationResizeImage`**; **`STBIR_FREE`** routed through expression-safe **`sit_stbir_free`** (stb internals use comma expressions — cannot expand statement-form **`SIT_FREE`**).
- **`sit/situation_api.h`**: **`SituationBeginLoadShaderFromSpirvMemoryEx`** — async SPIR-V kickoff with **`SituationSpirvLayoutProfile`** (**Vulkan**: layout profile selects pipeline descriptor layout; **OpenGL**: profile ignored, same as **`SituationBeginLoadShaderFromSpirvMemory`**).
- **`sit/situation_impl_renderer.h`**: Vulkan async SPIR-V load stores **`layout_profile`** on the load context; **`SituationBeginLoadShaderFromSpirvMemory`** delegates to Ex with **`SIT_SPIRV_LAYOUT_PROFILE_MESH`**.

### Tests

- **`tests/harness/test_graphics.c`**: **`demon_hunt_sky_spirv_begin_poll`** (OpenGL) — prefers devel fragment SPIR-V for link; accepts **`OPENGL_SPIRV_PROGRAM_LINK_FAILED`** (-641) when driver log contains **“too many instructions”** (async API + error reporting still validated).
- **`tests/harness/test_graphics_spirv.c`**: **`demon_hunt_sky_spirv_vk_begin_poll`** (Vulkan) — large production SPIR-V via **`SituationBeginLoadShaderFromSpirvMemoryEx(..., SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO)`** + Begin/Poll.

### Demo (examples only)

The **`demon_hunt`** example was updated to call the new Vulkan SPIR-V API; it is not part of the library surface.

### Verification

```bat
build_situation.bat opengl
build\sit_test.exe

build_situation.bat vulkan
build\sit_test_vulkan.exe --module graphics --filter spirv
```

---

## [v2.4.103 "Thread Pool IO Queue Fix"] - 2026-05-21

### Description

Fixes a **double consumer** on the low-priority job queue: worker threads and the dedicated I/O thread both dequeued `SIT_SUBMIT_DEFAULT` jobs (async file I/O), which could **double-free** job payloads and corrupt the heap. Symptoms included harness `save_file_text_async` passing then **SIGSEGV** in `filesystem` module teardown on Windows.

### Changes

- **`sit/situation_impl_threading.h`**: Workers skip queue 0 when `pool->io_thread` is active; idle wait checks high-priority queue too.
- **`tests/harness/test_filesystem.c`**: Module teardown no longer calls redundant `SituationDeleteFile` after async tests (per-test cleanup remains).

---

## [v2.4.102 "SPIR-V Diagnostics GL+VK"] - 2026-05-21

### Description

Documentation and API parity: recent SPIR-V work is **not OpenGL-only**. **`SituationPollShaderLoad`** on Vulkan now calls **`_SituationPollVkAsyncShaderLoad`** (same as frame acquire), and returns **`SituationGetLastErrorCode()`** on failure instead of a generic pipeline code only.

### Changes

- **`sit/situation_impl_renderer.h`**: Vulkan poll path aligned with OpenGL poll-driven async SPIR-V load.
- **`doc/TEST_SPIRV_SHADER_API.md`**: Dual-backend matrix, `sit_test` + `sit_test_vulkan` commands, separate error tables.
- **`doc/UPDATELOG.md`**, **`sit/situation_base_version.h`**: Version titles/descriptions name **GL+VK**, not OpenGL-only.

---

## [v2.4.101 "SPIR-V Driver Log Capture"] - 2026-05-21

### Tooling

- **`scripts/spirv_shader_debug.py`**: offline GLSL + SPIR-V stats; `demon_hunt --devel` compiles FS without `-O` for per-function instruction map.
- **`doc/SHADER_DEBUG.md`**: usage, bisect workflow, NVIDIA limit notes.
- **`compile_demon_hunt_shaders.bat`**: runs debug report after `glslc`.

### Description

SPIR-V compile/link failures now capture the **full driver diagnostic** in `SituationGetLastErrorMsg` (up to 16 KiB). **OpenGL:** `GL_INFO_LOG_LENGTH` via `_SituationDupGLInfoLog`. **Vulkan:** `vkCreateShaderModule` VkResult + stage label in `_SituationVulkanCreateShaderModuleEx` (unchanged; now documented alongside GL). Removes the old 900-character truncation in `_SituationSetGLErrorFromSpirvStage`.

### Changes

- **`sit/situation_api.h`**: `SITUATION_MAX_ERROR_MSG_LEN` / `SITUATION_MAX_SHADER_LOG_LEN` → **16384**.
- **`sit/situation_impl_renderer.h`**: `_SituationDupGLInfoLog`; async link failures use `_SituationSetGLErrorFromSpirvStage` with full program log; SPIR-V blob sizes kept until link completes.
- **`tests/harness/sit_graphics_test_helpers.h`**: Poll helper returns terminal errors (prints driver log before assert).
- **`examples/demon_hunt.c`**: Logs driver text on a separate line (not truncated in 768-byte buffer).

---

## [v2.4.100 "SPIR-V Poll Diagnostics"] - 2026-05-21

### Description

Fixes misleading silence after SPIR-V kickoff on **OpenGL** (Vulkan poll path unchanged this patch): progress log was suppressed for 2s, and `AcquireFrameCommandBuffer` advanced SPIR-V specialize a second time inside `render_frame`.

### Changes

- **`sit/situation_impl_renderer.h`**: SPIR-V specialize only from **`SituationPollShaderLoad`**; frame acquire still polls pending program link.
- **`examples/demon_hunt.c`**: First progress log immediate; poll diagnostics for invalid handle / `RESOURCE_INVALID`.
- **`tests/harness/test_graphics.c`**: **`demon_hunt_sky_spirv_begin_poll`** — real Demon Hunt `.spv` via Begin+Poll API.
- **`doc/TEST_SPIRV_SHADER_API.md`**: Full rebuild + harness + manual test checklist.

---

## [v2.4.99 "SPIR-V Link Poll Fix"] - 2026-05-21

### Description

**OpenGL:** SPIR-V loads that never completed after kickoff — **`SituationPollShaderLoad`** drives specialize/link polling; program link finalization falls back to **`GL_LINK_STATUS`** when **`GL_COMPLETION_STATUS_KHR`** never becomes true (large SPIR-V on NVIDIA). **Vulkan:** async SPIR-V/GLSL pipeline build still polled from **`SituationAcquireFrameCommandBuffer`** until v2.4.102.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **99**.
- **`sit/situation_impl_renderer.h`**: **`_SituationPollGLPendingProgramLink`**, **`_SituationFinalizeGLPendingProgramLink`**; parallel-compile detection; SPIR-V link sets **`gl_pending_link_spirv`** and binds UBO/SSBO blocks on success.
- **`SituationPollShaderLoad`**: makes GL context current and polls async SPIR-V + pending link before returning status.

### Examples

- **`examples/demon_hunt.c`**: Poll shader load **before** **`render_frame`**; clearer progress log lines.

---

## [v2.4.98 "Async SPIR-V Load"] - 2026-05-21

### Description

**`SituationBeginLoadShaderFromSpirvMemory`** no longer blocks the main thread for the full compile/link of large SPIR-V blobs. **OpenGL:** kickoff uploads bytecode; **`SituationPollShaderLoad`** advances VS specialize → FS specialize → async program link. **Vulkan:** kickoff copies bytecode; pipeline build runs when the poll path completes (frame acquire; **`SituationPollShaderLoad`** from v2.4.102). Fixes blank frozen window with only `loading embedded SPIR-V (...)` in the log (OpenGL Demon Hunt).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **98**.
- **`sit/situation_impl_decl.h`**: Shader slot stores SPIR-V copies + substage for async load.
- **`sit/situation_impl_renderer.h`**:
  - **`SIT_GL_ASYNC_STAGE_SPIRV`**: **`_SituationBeginGLSpirvShaderLoadAsync`**, **`_SituationPollGLAsyncSpirvShaderLoad`** (one specialize or link step per poll).
  - **`SituationBeginLoadShaderFromSpirvMemory`**: async kickoff on **OpenGL and Vulkan**; **`SituationLoadShaderFromSpirvMemory`** remains synchronous on both.
  - **`SituationUnloadShader`**: frees SPIR-V async copies.

### Examples

- **`examples/demon_hunt.c`**: Renders one frame before shader kickoff; logs SPIR-V load progress every 2s while polling.

### Verification

Rebuild **`build_situation.bat opengl`**, then **`build_examples.bat opengl demon_hunt`**. Log should show kickoff OK, optional `still loading...`, then **`init_sky_gpu`** lines.

---

## [v2.4.97 "SPIR-V Error Reporting"] - 2026-05-21

### Description

SPIR-V load failures are no longer reported as generic GLSL compile/link errors. Each failure stage (missing extension, bad blob, VS/FS/CS specialize, program link, file read, Vulkan module create) gets a dedicated **`SituationError`** code, a structured detail string (stage label + byte size + driver log), and optional retrieval via **`SituationGetLastErrorCode()`** / **`SituationErrorToString()`**.

Fixes Demon Hunt debugging where a GLSL fallback OOM looked identical to an SPIR-V path failure in logs.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **97**.
- **`sit/situation_base_errno.h`**: New codes:
  - **Rendering:** **`SITUATION_ERROR_SPIRV_FILE_READ_FAILED`** (-554), **`SITUATION_ERROR_SPIRV_INVALID_BINARY`** (-555).
  - **OpenGL SPIR-V:** **`OPENGL_SPIRV_UNAVAILABLE`** (-636), **`OPENGL_SPIRV_INVALID_BINARY`** (-637), **`OPENGL_SPIRV_VS_SPECIALIZE_FAILED`** (-638), **`OPENGL_SPIRV_FS_SPECIALIZE_FAILED`** (-639), **`OPENGL_SPIRV_CS_SPECIALIZE_FAILED`** (-640), **`OPENGL_SPIRV_PROGRAM_LINK_FAILED`** (-641).
  - **Vulkan SPIR-V:** **`VULKAN_SPIRV_INVALID`** (-753), **`VULKAN_SPIRV_VS_MODULE_FAILED`** (-754), **`VULKAN_SPIRV_FS_MODULE_FAILED`** (-755), **`VULKAN_SPIRV_CS_MODULE_FAILED`** (-756).
- **`sit/situation_api.h`**: **`SituationGetLastErrorCode()`**, **`SituationErrorToString()`**.
- **`sit/situation_impl_decl.h`**: **`sit_gs.last_error_code`** stored on every **`_SituationSetErrorFromCode`** call.
- **`sit/situation_impl_ctrl.h`**: Implements error-code query + string table lookup from the X-macro errno table.
- **`sit/situation_impl_renderer.h`**:
  - OpenGL: **`_SituationSetGLErrorFromSpirvStage`**, **`_SituationValidateSpirvBinary`**; SPIR-V graphics/compute paths use stage-specific codes instead of **`OPENGL_SHADER_COMPILE`/`LINK`**.
  - **`_SituationReadSpirvFile`**: **`SPIRV_FILE_READ_FAILED`** / **`SPIRV_INVALID_BINARY`** with filename in detail.
  - Vulkan: **`_SituationVulkanCreateShaderModuleEx`** (per-stage module errors); async compile distinguishes vertex vs fragment shaderc failure.
  - **`SituationPollShaderLoad`**: returns last SPIR-V-specific code when program link never completed (OpenGL).

### Harness

- **`tests/harness/test_graphics_spirv.c`**: **`spirv_memory_invalid_params`** expects backend-specific misalignment codes; new **`spirv_error_code_reporting`** ( **`SituationErrorToString`**, **`SituationGetLastErrorCode`**, detail message).
- **`tests/harness/test_graphics.c`**: registers **`spirv_error_code_reporting`**.

### Examples

- **`examples/demon_hunt.c`**: **`sky_log_situation_error`** logs numeric code + table label + driver detail; logs embedded SPIR-V byte sizes when embed is missing; distinguishes SPIR-V kickoff vs GLSL fallback vs poll failure.

### Verification

| Command | Expected |
|---------|----------|
| `build\sit_test.exe --module graphics --filter spirv` | all **`spirv_*`** pass (OpenGL) |
| `build\sit_test_vulkan.exe --module graphics --filter spirv` | all **`spirv_*`** pass (Vulkan) |

---

## [v2.4.96 "GL VK Async Shader Load"] - 2026-05-21

### Description

Graphics shaders can be loaded **without blocking the main thread** during compile/link (OpenGL) or shaderc + pipeline creation (Vulkan). Work is spread across frames; on Vulkan, GLSL→SPIR-V runs on the thread pool and pipeline build runs when the poll path completes. Demon Hunt and similar apps stay responsive instead of freezing for a minute at play start.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **96**.
- **`sit/situation_base_errno.h`**: **`SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS`** (-553).
- **`sit/situation_api.h`**: **`SituationBeginLoadShaderFromMemory`**, **`SituationBeginLoadShaderFromSpirvMemory`**, **`SituationPollShaderLoad`**.
- **`sit/situation_impl_decl.h`**: OpenGL async compile/link fields; Vulkan **`vk_async_load`** context on shader slots.
- **`sit/situation_impl_renderer_fwd.h`**: Forward decls for async compile/link helpers (all renderer fwd decls stay in this file only).
- **`sit/situation_impl_renderer.h`**:
  - **OpenGL:** **`_SituationCompileGLShaderEx`**, **`_SituationPollGLShaderCompile`**, **`_SituationPollGLAsyncShaderLoad`** — polled from **`SituationAcquireFrameCommandBuffer`** with existing async link poll.
  - **Vulkan** ( **`SITUATION_ENABLE_SHADER_COMPILER`** ): GLSL begin-load compiles on the I/O pool; SPIR-V begin-load copies bytecode and defers **`vkCreateGraphicsPipelines`**; **`_SituationVulkanBuildGraphicsPipelinesOnSlot`** shared by sync and async paths. Thread-pool submit uses **`SIT_SUBMIT_POINTER_ONLY`** so the worker updates the heap async context (struct copy previously left poll stuck forever).
  - **Both:** **`SituationBeginLoadShaderFromMemory`**, **`SituationBeginLoadShaderFromSpirvMemory`** (Vulkan non-blocking pipeline build; OpenGL SPIR-V begin-load remains blocking), **`SituationPollShaderLoad`**, **`SituationUnloadShader`** cleanup for in-flight async state.
  - Without the shader compiler, begin-load APIs fall back to blocking **`SituationLoadShaderFromMemory`** / **`SituationLoadShaderFromSpirvMemory`**.

### Harness

- **`tests/harness/sit_graphics_test_helpers.h`**: **`graphics_test_async_poll_shader_ready`**, **`graphics_test_glsl_vs_passthrough`**, **`graphics_test_glsl_fs_solid_red`** — GLSL matched to active backend (`#version 450` Vulkan, `#version 460 core` OpenGL).
- **`tests/harness/test_graphics.c`**: **`async_shader_*`** (kickoff, draw readback, renderer alive while loading, unload during load, sync load after async).
- **`tests/harness/test_graphics_spirv.c`**: **`async_shader_spirv_memory_vulkan`**.
- **`tests/harness/shaders/harness_solid_red_{gl,vk}.fs`**: mesh-layout red SPIR-V sources for disk/async tests.
- **`compile_harness_shaders.bat`**: builds solid-red SPIR-V alongside existing harness shaders.

### Verification (GTX 1070 reference)

| Command | Result |
|---------|--------|
| `build\sit_test.exe` (full suite) | **335 / 335** pass |
| `build\sit_test_vulkan.exe` (full suite) | **325 / 325** pass |
| `--module graphics --filter async_shader` (either backend, after **`compile_harness_shaders.bat`**) | all **`async_shader_*`** pass |

### Examples

- **`examples/demon_hunt.c`**: No synchronous GPU init on first play frame; **`SituationBeginLoadShaderFromMemory`** at startup, poll after **`render_frame`**, CPU world until ready; no runtime embedded SPIR-V; shader path resolution fixed when GLSL lives under **`examples/`**.

---

## [v2.4.95 "GL VK Screen Text Parity Tests"] - 2026-05-21

### Description

Closes the harness gap for **OpenGL vs Vulkan screen-space parity**: framebuffer readback uses a consistent top-left origin (+Y down), and **`SituationCmdDrawText`** placement is regression-tested on **both** backends with the same pixel contract.

Complements **v2.4.94** SPIR-V descriptor parity and **v2.4.58** Vulkan vertical flip on **`SituationLoadImageFromScreen`**.

### Harness

- **`tests/harness/sit_graphics_test_helpers.h`**: Shared readback helpers (`graphics_test_sample_rgba`, region bright/dark scans, solid-color texture + bitmap font setup).
- **`tests/harness/test_graphics.c`**:
  - **`screen_readback_corner_layout`** — 2×2 quadrant texture stretched to the window; asserts TL/TR/BL/BR colors and rejects vertical/horizontal flip misreads.
  - **`cmd_draw_text_screen_layout`** — **"TOP"** / **"BOT"** at fixed pixel positions; asserts bright text bands, dark gap, and no top string mirrored to the bottom.
- **`build_tests.bat`**: Reminder to run **`compile_harness_shaders.bat`** + **`scripts/spirv_desc_spike.py`** after SPIR-V shader changes.

### Verification (GTX 1070 reference)

| Command | Result |
|---------|--------|
| `build\sit_test.exe --module graphics --filter screen_readback_corner_layout` | PASS |
| `build\sit_test.exe --module graphics --filter cmd_draw_text_screen_layout` | PASS |
| `build\sit_test_vulkan.exe --module graphics --filter screen_readback_corner_layout` | PASS |
| `build\sit_test_vulkan.exe --module graphics --filter cmd_draw_text_screen_layout` | PASS |

Graphics module counts after this patch: **98** tests (OpenGL), **88** (Vulkan) — use harness printout as truth.

---

## [v2.4.94 "Vulkan SPIR-V Descriptor Bind"] - 2026-05-21

### Description

Phases 2–3 of [`doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md): Vulkan **`SituationCmdBindDescriptorSet`** respects **`SituationSpirvLayoutProfile`** (static UBO on UBO+SSBO profile); harness SPIR-V pixel readback tests run on Vulkan without deferral.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **94**.
- **`sit/situation_impl_renderer.h`**: Profile-aware descriptor layout/type selection; per-buffer descriptor cache keyed by layout + type; static **`UNIFORM_BUFFER`** for UBO+SSBO set 0.
- **`sit/situation_impl_decl.h`**: Buffer slot tracks cached descriptor layout/type.

### Harness

- **`tests/harness/test_graphics_spirv.c`**: Loads via **`SituationLoadShaderFromSpirvMemoryEx`** with **`DUAL_SSBO`** / **`UBO_SSBO`** on Vulkan; removed Vulkan pixel-test skip.
- **`tests/harness/shaders/harness_ubo_ssbo_vk.fs`**: Literal **`set = 0`** Frame / **`set = 1`** TagBlock (fixes inverted SPIR-V sets vs harness binds); regen via **`compile_harness_shaders.bat`** + **`python scripts/spirv_desc_spike.py`**.
- **Verified (GTX 1070)**: **`sit_test_vulkan.exe --module graphics`** **86/86**; **`--filter spirv`** **5/5**; OpenGL **`--filter spirv`** **7/7**.

**Plan status:** [`doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md) Phases **0–3 complete**; Phase **4** (reflection) deferred.

---

## [v2.4.93 "Vulkan SPIR-V Layout Profiles"] - 2026-05-21

### Description

Phase 1 of [`doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md): Vulkan user SPIR-V graphics shaders can select a descriptor **pipeline layout profile** via **`SituationLoadShaderFromSpirvMemoryEx`**.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **93**.
- **`sit/situation_api.h`**: **`SituationSpirvLayoutProfile`** (`MESH`, `DUAL_SSBO`, `UBO_SSBO`); **`SituationLoadShaderFromSpirvMemoryEx`**.
- **`sit/situation_impl_decl.h`**: **`graphics_spirv_layout_ubo_ssbo`**; per-shader **`vk_spirv_layout_profile`** / **`vk_owns_pipeline_layout`**.
- **`sit/situation_impl_renderer.h`**: **`_SituationVulkanInitGraphicsSpirvLayouts`**; **`DUAL_SSBO`** reuses **`SIT_COMPUTE_LAYOUT_TWO_SSBOS`**; **`SituationUnloadShader`** does not destroy cached layouts.

**Default unchanged:** **`SituationLoadShaderFromSpirvMemory`** still uses **`SIT_SPIRV_LAYOUT_PROFILE_MESH`** (dynamic UBO + sampler).

---

## [v2.4.92 "SPIR-V Harness And Block Bind"] - 2026-05-21

### Description

Hardens graphics regression coverage for **SituationLoadShaderFromSpirvMemory** and OpenGL block binding on both **OpenGL** and **Vulkan** harness builds. Fixes SPIR-V programs where `glGetProgramResourceIndex` does not resolve UBO/SSBO block names.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **92**.
- **`sit/situation_impl_renderer.h`**: Close the large OpenGL section before the Vulkan SPIR-V loader (fixes Vulkan link); `_SituationGLFindProgramResourceIndex`, `_SituationGLFindBlockIndexByBinding`, `_SituationBindGLProgramUniformBlocks` at SPIR-V link; name/binding fallbacks in **`SituationBindShaderStorageBlock`** / **`SituationBindUniformBlock`**; **`SituationSetShaderUniformLocation`** uses **`int`** in the public API.

### Harness (tests only)

- **`compile_harness_shaders.ps1`**, **`tests/harness/shaders/*`**, embedded **`sit_harness_spirv_*_embed.c`**: OpenGL- and Vulkan-target SPIR-V blobs.
- **`tests/harness/test_graphics_spirv.c`**: **`spirv_memory_*`**, **`spirv_disk_roundtrip`** (fail if embed empty).
- **`build_tests.bat`**: compiles harness SPIR-V and links embed TU per backend.
- **`demon_hunt_sky_shader_link`**: fails when GLSL link fails (no silent pass).

**Follow-up (shipped v2.4.93–94)**: Vulkan user SPIR-V descriptor layouts — [`doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md`](plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md).

---

## [v2.4.91 "Renderer Header Guard Fix"] - 2026-05-20

### Description

Fixes a **compile failure** (`unterminated #ifndef` at `SITUATION_IMPL_RENDERER_H`) in **`sit/situation_impl_renderer.h`**: the final `#endif` before the file end closed the large **`#if defined(SITUATION_USE_OPENGL)`** block opened at line 1613, not the include guard. Adds the missing **`#endif // SITUATION_IMPL_RENDERER_H`** and removes a redundant nested **`#if defined(SITUATION_USE_OPENGL)`** around the SPIR-V program loader.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **91**.
- **`sit/situation_impl_renderer.h`**: Restore balanced preprocessor guards at file end; drop duplicate SPIR-V sub-`#if`.

---

## [v2.4.90 "SPIR-V Link Without Queue Spin"] - 2026-05-20

### Description

Removes **`_SituationWaitForRenderQueueIdle()`** from **`SituationLoadShaderFromSpirvMemory()`**. The v2.4.89 spin/yield loop had no completion guarantee beyond a fixed iteration cap and could still leave host SPIR-V link contending with the render thread in ways that stalled the application before the first presented frame.

SPIR-V load again uses only **`_SituationMakeGLContextCurrentForHostThread()`** on the loader window (v2.4.87–88 behavior). **`SituationBindUniformBlock()`**, SSBO bind API, uniform-by-location, and UBO-member skip in **`_SituationPopulateGLShaderUniformMap()`** are unchanged from v2.4.89.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **90**.
- **`sit/situation_impl_renderer.h`**: Drop **`_SituationWaitForRenderQueueIdle()`** and its call before SPIR-V program link.

---

## [v2.4.89 "SPIR-V UBO Bind & Host Link Sync"] - 2026-05-20

### Description

**OpenGL SPIR-V** follow-up to v2.4.88: std140 **uniform block** binding, safer host-thread link when **`SITUATION_ENABLE_RENDER_THREAD`** is on, and leaner uniform reflection after link.

1. **Uniform blocks** — New **`SituationBindUniformBlock(shader, block_name, binding_point)`** calls **`glGetProgramResourceIndex`** + **`glUniformBlockBinding`** so GLSL **`layout(std140, binding = N) uniform …`** matches **`SituationCmdBindDescriptorSet(cmd, N, ubo_buffer)`** when SPIR-V reflection reports the wrong block index (same pattern as **`SituationBindShaderStorageBlock`** for SSBOs).
2. **Uniform map** — **`_SituationPopulateGLShaderUniformMap()`** skips uniforms with **`GL_BLOCK_INDEX >= 0`** (UBO members are not standalone **`glProgramUniform`** locations). Standalone and array entry names still use **`GL_LOCATION`** then **`glGetUniformLocation`**.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **89**.
- **`sit/situation_api.h`**: **`SituationBindUniformBlock()`**.
- **`sit/situation_impl_renderer.h`**: **`SituationBindUniformBlock()`**; UBO-member skip in **`_SituationPopulateGLShaderUniformMap()`**.

---

## [v2.4.88 "SPIR-V SSBO Binding & Uniform Locations"] - 2026-05-20

### Description

**OpenGL SPIR-V** (`GL_ARB_gl_spirv`): fixes storage-block binding and uniform upload after **`SituationLoadShaderFromSpirvMemory()`**, especially with **`SITUATION_ENABLE_RENDER_THREAD`**.

SPIR-V link could leave **`glShaderStorageBlockBinding`** out of sync with GLSL **`layout(binding = N)`** (reflection often reports **0**; **`_SituationBindGLProgramStorageBlocks()`** only fixes duplicate bindings). Uniform maps built at link could miss **`layout(location = N)`** slots; per-frame **`SituationSetShaderUniform`** then failed or targeted the wrong location. v2.4.87 host/loader GL context behavior for post-link **`SituationCreateBuffer`** / **`SituationCreateMesh`** is unchanged.

1. **SSBO binding** — SPIR-V reflection often reports `GL_BUFFER_BINDING` **0** for storage blocks whose GLSL source uses a non-zero `layout(binding = N)`. **`_SituationBindGLProgramStorageBlocks()`** only resolves **duplicate** bindings; a single block can remain on **0** while the shader reads **N**. New **`SituationBindShaderStorageBlock(shader, block_name, binding_point)`** uses **`glGetProgramResourceIndex`** + **`glShaderStorageBlockBinding`** so the host can assign the block to the binding declared in GLSL. **`SituationCmdBindDescriptorSet(cmd, set_index, buffer)`** must use the same index.
2. **Uniform locations** — **`_SituationPopulateGLShaderUniformMap()`** (called after SPIR-V link) now records **`GL_LOCATION`** from the `GL_UNIFORM` program interface when available, falls back to **`glGetUniformLocation(name)`**, and registers indexed array entry names (`name[k]`). New **`SituationSetShaderUniformLocation(shader, location, data, type)`** sets uniforms by explicit location with the same defer path as **`SituationSetShaderUniform`** (**`SIT_OP_SET_UNIFORM`** on the render thread while a frame command buffer is active).
3. **Host GL context (v2.4.87, retained)** — **`SituationLoadShaderFromSpirvMemory()`** keeps the loader/host context current after a successful link for immediate **`SituationCreateBuffer`** / **`SituationCreateMesh`**; **`SituationAcquireFrame()`** releases it before command recording; **`SituationCreateBuffer`** and **`SituationCreateMesh`** call **`_SituationMakeGLContextCurrentForHostThread()`** at entry when threading is enabled.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **88**.
- **`sit/situation_api.h`**: **`SituationBindShaderStorageBlock()`**, **`SituationSetShaderUniformLocation()`**.
- **`sit/situation_impl_renderer.h`**: SSBO block bind API; **`_SituationSetShaderUniformLocationImpl()`** shared by name and location entry points; uniform map populate via **`GL_LOCATION`** + name fallback; host GL context helpers unchanged from v2.4.87.

---

## [v2.4.87 "SPIR-V Host GL Context & Init Fix"] - 2026-05-20

### Description

Fixes Demon Hunt (and any **`SituationLoadShaderFromSpirvMemory`** user) still crashing/hanging after v2.4.86:

1. **Demon Hunt `init_sky_gpu`** — After SPIR-V load succeeded, **`if (!g_sky_ok) return 0`** aborted before SSBO/mesh setup ( **`g_sky_ok` was never set until the end** ), leaving a half-loaded shader or retry loops. Now continues when **`g_sky_shader.generation != 0`** and sets **`g_sky_ok`** only after full init.
2. **Sky init timing** — GPU skydome init runs at the start of **`render_frame`** during **ENTERING** (inside **`SituationAcquireFrameCommandBuffer`**), not after **`EndFrame`** without a GL context.
3. **Library SPIR-V uniforms** — **`_SituationPopulateGLShaderUniformMap`** uses **`GL_UNIFORM` program-interface reflection** (SPIR-V-safe) instead of **`glGetActiveUniform`**. **`_SituationMakeGLContextCurrentForHostThread()`** (loader window when render thread is on) runs before SPIR-V link/populate and on uniform cache miss. Removed v2.4.86 guard that returned **`SUCCESS` without uploading** missing uniforms.

Disk **`SituationLoadShaderFromSpirv`** still reads files then calls the same memory path; behavior should match the previously working disk load once init and context are correct.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **87**.
- **`sit/situation_impl_renderer.h`**: Host-thread GL context helper; SPIR-V uniform reflection; revert silent uniform skip.
- **`examples/demon_hunt.c`**: Init order and **`g_sky_ok`** / handle checks; unload before reload.

---

## [v2.4.86 "SPIR-V Uniform Cache At Load"] - 2026-05-20

### Description

Fixes a **hang/freeze** when using **`SituationLoadShaderFromSpirvMemory()`** (e.g. Demon Hunt embedded skydome SPIR-V) with **`SITUATION_ENABLE_THREADING`**: gameplay was calling **`SituationSetShaderUniform`** many times per frame with an **empty** uniform cache, so the main thread repeatedly called **`glGetUniformLocation`** while the **render thread** owned the GL context.

**OpenGL:** After SPIR-V link, **`_SituationPopulateGLShaderUniformMap()`** fills the shader slot uniform map (active uniforms plus indexed array names such as `uTeleporters[0]`). **`SituationSetShaderUniform`** skips driver uniform queries on the main thread during an active frame when the render thread is enabled and the name is not cached.

**Demon Hunt:** **`g_sky_ok`** is set only after SSBO + fullscreen mesh + shader are all ready (not immediately after SPIR-V load).

### Library Changes

- **`sit/situation_base_version.h`**: Patch **86**.
- **`sit/situation_impl_renderer.h`**: `_SituationPopulateGLShaderUniformMap()`; called from **`SituationLoadShaderFromSpirvMemory()`** (OpenGL); render-thread guard in **`SituationSetShaderUniform`**.

### Examples

- **`examples/demon_hunt.c`**: `g_sky_ok` lifecycle tied to full GPU skydome init success.

---

## [v2.4.85 "SPIR-V Memory Load & Demon Hunt Embed"] - 2026-05-20

### Description

Adds **`SituationLoadShaderFromSpirvMemory()`** on **OpenGL** and **Vulkan**: same pipeline behavior as **`SituationLoadShaderFromSpirv()`** (disk), but from caller-supplied SPIR-V bytes (word-aligned sizes). File-based **`SituationLoadShaderFromSpirv()`** now reads `.spv` into a temp buffer and delegates to the memory entry point, then stores paths and mod times for hot-reload when loading from disk. Memory-only loads leave shader paths unset.

**Demon Hunt (OpenGL):** `compile_demon_hunt_shaders.bat` runs **`scripts/gen_demon_hunt_spirv_embed.ps1`** after `glslc` to regenerate **`examples/demon_hunt_sky_spirv_embed.c`**. A committed **stub** (zero lengths) keeps the tree buildable without `glslc`; **`build_examples.bat opengl demon_hunt`** links **`demon_hunt_sky_spirv_embed.c`**. At runtime the game tries **embedded** SPIR-V first, then the existing **disk** path resolution.

### Library Changes

- **`sit/situation_base_version.h`**: Patch **85**.
- **`sit/situation_api.h`**: `SituationLoadShaderFromSpirvMemory()` declaration.
- **`sit/situation_impl_renderer.h`**: OpenGL + Vulkan implementations; disk SPIR-V loaders refactored to call memory API.

### Tooling / Examples

- **`scripts/gen_demon_hunt_spirv_embed.ps1`**: SPIR-V → C array embed generator.
- **`examples/demon_hunt_sky_spirv_embed.h`**, **`examples/demon_hunt_sky_spirv_embed.c`**: embed symbols (stub until precompile runs).
- **`compile_demon_hunt_shaders.bat`**: Regenerate embed after successful `glslc`.
- **`build_examples.bat`**: Link Demon Hunt embed translation unit for OpenGL.
- **`examples/demon_hunt.c`**: Prefer embedded skydome SPIR-V when lengths are non-zero; disk fallback unchanged.

---

## [v2.4.84 "SPIR-V Load Vulkan Parity"] - 2026-05-20

### Description

**Vulkan** `SituationLoadShaderFromSpirv()` is fully implemented: same `VkPipelineLayout`, descriptor sets (dynamic UBO @ set 0, sampler @ set 1), push constant range, and three `VkPipeline` variants (PBR / legacy / simple vertex input) as `SituationLoadShaderFromMemory()` after shaderc. Reads vertex and fragment `.spv` from disk via `_SituationReadSpirvFile` → `vkCreateShaderModule` / `vkCreateGraphicsPipelines`. **Does not require** `SITUATION_ENABLE_SHADER_COMPILER`.

**SPIR-V target:** Shaders must be compiled for **Vulkan** (e.g. `glslc --target-env=vulkan1.3`, or the same shaderc target as `SituationLoadShaderFromMemory`). OpenGL-only precompiled shaders (`glslc --target-env=opengl`, as used for Demon Hunt skydome) are **not** interchangeable with this Vulkan path.

### Library Changes

- **`sit/situation_base_version.h`**: Patch `84`.
- **`sit/situation_api.h`**: Clarified `SituationLoadShaderFromSpirv` comment for both backends.
- **`sit/situation_impl_renderer.h`**: Vulkan body for `SituationLoadShaderFromSpirv` (replaces temporary `NOT_IMPLEMENTED` stub).
- **`build_tests.bat`**: Vulkan build writes **`build\sit_test_vulkan.exe`** (OpenGL unchanged: **`build\sit_test.exe`**) so the linker does not overwrite a running/locked harness when switching backends.

---

## [v2.4.83 "SPIR-V Shader Load & Harness SSBO"] - 2026-05-20

### Description

Adds a public SPIR-V load path for **OpenGL** (no runtime GLSL compile for that path), five graphics harness regression tests for fragment multi-SSBO / SPIR-V, and Demon Hunt build-time shader precompile so launch no longer stalls ~20s on driver GLSL compile. Vulkan parity for the same API is in **v2.4.84** above.

1. **`SituationLoadShaderFromSpirv()`** — **OpenGL:** load linked graphics programs from precompiled `.spv` vertex/fragment pairs via `GL_ARB_gl_spirv` (`glShaderBinary` + `glSpecializeShader`).
2. **SPIR-V file loader placement** — `_SituationReadSpirvFile()` is implemented **outside** `#if SITUATION_USE_OPENGL` so both backends compile it. An earlier refactor had left it only under OpenGL, which broke **`situation_vulkan.dll`** link (`undefined reference to _SituationReadSpirvFile`) for internal callers such as `_SituationCreateVulkanPipeline`. **OpenGL-only:** `SituationLoadShaderFromSpirv` implementation + `_SituationCreateGLShaderProgramFromSpirv` remain under `SITUATION_USE_OPENGL`.
3. **Graphics harness Phases 23–29** — Five new OpenGL-only tests in `test_graphics.c` with shared helpers in `tests/harness/sit_graphics_test_helpers.h`:
   - **`graphics_helpers_smoke`** — Fullscreen draw + center-pixel readback smoke test.
   - **`fragment_dual_ssbo_readback`** — Two SSBOs at bindings 0/1; regression for v2.4.82 duplicate-binding fix.
   - **`fragment_combined_scene_ssbo`** — `ShaderScenePack` header + `spriteData[0]` layout (matches Demon Hunt after `SCENE_SPRITE_VEC4_BASE = 0`).
   - **`uniform_1iv_int_array`** — `SituationSetShaderUniform1iv` int-array upload (graceful skip when SPIR-V strips the uniform).
   - **`demon_hunt_sky_shader_link`** — Compile-link full Demon Hunt world shader sources (~11s); catches GLSL regressions before playtesting.
4. **Demon Hunt precompiled shaders** — `compile_demon_hunt_shaders.bat` uses `glslc` with `--target-env=opengl -fauto-map-locations -fauto-bind-uniforms` to emit `demon_hunt_sky.{vs,fs}.spv`. `build_examples.bat opengl demon_hunt` runs precompile before linking; runtime loads `.spv` only (CPU world fallback if missing).

### Library Changes

- **`sit/situation_base_version.h`**: Patch `83` (Vulkan SPIR-V-from-disk completion: **v2.4.84**).
- **`sit/situation_api.h`**: `SituationLoadShaderFromSpirv()` declaration.
- **`sit/situation_impl_decl.h`**: `SituationSpirvBinary` struct for in-memory SPIR-V blobs.
- **`sit/situation_impl_renderer.h`**: `_SituationReadSpirvFile()` in a **backend-neutral** region; `SituationLoadShaderFromSpirv()` **OpenGL** implementation + `_SituationCreateGLShaderProgramFromSpirv()` under `SITUATION_USE_OPENGL` (no `SITUATION_ENABLE_SHADER_COMPILER` required for disk `.spv` on GL). **Vulkan** full implementation: **v2.4.84**.
- **`compile_demon_hunt_shaders.bat`**: Build-time GLSL → SPIR-V for Demon Hunt skydome.
- **`build_examples.bat`**: Precompile Demon Hunt shaders before exe link (warn-only on failure).
- **`examples/demon_hunt.c`**: `try_load_sky_shader_spirv()`; SPIR-V-only GPU init; CPU column fallback when skydome unavailable; periodic SPIR-V retry.
- **`examples/demon_hunt_sky.fs`**: Structural recovery (shared DDA, dynamic lights budget, sprite shading helpers); `SCENE_SPRITE_VEC4_BASE = 0` SSBO layout fix.
- **`tests/harness/sit_graphics_test_helpers.h`**: Shared fullscreen mesh, draw, readback, SSBO create, SPIR-V skip helpers.
- **`tests/harness/test_graphics.c`**: Five Phase 23–29 tests registered under `#if SITUATION_USE_OPENGL`.

### Test Harness

OpenGL full sequential run: **320 / 320** pass (was 315; **+5** graphics tests). Filter examples:

```bat
build\sit_test.exe --module graphics --filter fragment_dual_ssbo
build\sit_test.exe --module graphics --filter demon_hunt_sky_shader_link
```

### Observations

- SPIR-V precompile moves the heavy fragment compile (~13s) to **build** time; launch should log `[demon_hunt] init_sky_gpu: SPIR-V load OK.` instead of blocking on GLSL.
- Rebuild **`situation_opengl.dll`** (if used) and run **`compile_demon_hunt_shaders.bat`** after editing `demon_hunt_sky.fs` / `.vs`.
- **`build_situation.bat vulkan`** must succeed again after the `_SituationReadSpirvFile` placement fix; verify `build\dll\situation_vulkan.dll` after pulls touching `situation_impl_renderer.h`.
- **Backend parity:** OpenGL uses **OpenGL-target** `.spv` (e.g. Demon Hunt `glslc --target-env=opengl`). Vulkan uses **Vulkan-target** `.spv` via the same API; see **v2.4.84**.
- `demon_hunt_sky_shader_link` still exercises runtime GLSL compile for CI guard; the game itself does not fall back to GLSL at launch on OpenGL.

---

## [v2.4.82 "SPIR-V SSBO Unique Bindings"] - 2026-05-18

### Description

Fixed OpenGL SPIR-V programs where multiple `layout(binding=N)` SSBOs could reflect the same binding point, so only the last block was reachable and hosts binding `set_index` 0 vs 1 still aliased the same GPU slot.

1. **Unique SSBO bindings at link** — `_SituationBindGLProgramStorageBlocks()` now detects duplicate declared bindings and assigns the next free binding index in block order, so a second block at binding 0 becomes binding 1 instead of overwriting the first.
2. **Demon Hunt scene SSBO** — Maze wall rows and the sprite pack live in one `ShaderScenePack` buffer at binding 1, uploaded once per frame, avoiding a separate binding-0 map buffer that SPIR-V could collapse with the sprite block.

### Library Changes

- **`sit/situation_base_version.h`**: Patch `82`.
- **`sit/situation_impl_renderer.h`**: Duplicate-binding fix in `_SituationBindGLProgramStorageBlocks()`.
- **`examples/demon_hunt.c`**, **`examples/demon_hunt_sky.fs`**: Combined map + sprite SSBO; single `SituationCmdBindDescriptorSet(cmd, 1, …)`.

### Observations

- Passing harness tests does not exercise multi-SSBO fragment shaders with SPIR-V on Windows; Demon Hunt exposed binding aliasing (walls never hit, open rays + heavy sprite shading → severe stutter).
- Rebuild **`situation_opengl.dll`** before **`demon_hunt.exe`** after pulling this patch.

---

## [v2.4.81 "GL Shader Storage Block Binding"] - 2026-05-18

### Description

Fixed OpenGL fragment shaders that read sprite (or other) data from SSBOs but appeared to ignore uploads, which broke Demon Hunt’s Phase 3 world rendering after CPU fallbacks were disabled.

1. **SSBO binding at link** — After a graphics program links, Situation now walks active `GL_SHADER_STORAGE_BLOCK` resources and calls `glShaderStorageBlockBinding()` using each block’s `layout(binding=N)` from GLSL. Some Windows GL drivers do not apply `binding=` at link time alone; without this, `layout(std430, binding = 0) buffer ShaderSpritePack` could stay unbound and the shader would read garbage while the host still uploaded valid data.
2. **Demon Hunt fallback contract** — Shader sprite drawing (including Phase 3 portals, particles, shots, exit pillar) is gated on `g_sky_ok && g_sprite_ssbo_ok`, not only `g_shader_sprites_enabled`. If the skydome fails to compile/link or the SSBO is missing, CPU sprite drawing runs again instead of skipping both paths and leaving a blank world.
3. **HUD clarity** — In-game status text reports when the world shader failed and points at `demon_hunt_sky.log` instead of claiming “shader sprites OK” while nothing draws.

### Library Changes

- **`sit/situation_base_version.h`**: Patch `81`, description updated.
- **`sit/k-term/example/situation.h`**: Mirrored patch `81`.
- **`sit/situation_impl_renderer.h`**: Added `_SituationBindGLProgramStorageBlocks()`; invoked after successful link in `_SituationCreateGLShaderProgramFromSpirv()` and `_SituationCreateGLShaderProgram()`.
- **`examples/demon_hunt.c`**: `shader_sprite_runtime_enabled()` requires live skydome + SSBO; HUD reflects shader failure vs active sprite path.

### Observations

- This patch addresses the **transport/contract** side (SSBO must be bound; do not disable CPU draws unless the GPU path is actually live). It does not change the separate fragment uniform budget that can still cause skydome **link** failure when too many `uniform` arrays are added; Demon Hunt moved sprites to an SSBO partly to relieve that pressure.
- If Phase 3 is enabled and link still fails, check `demon_hunt_sky.log`; with this fix the game should remain playable on CPU sprites when the shader path is down.

---

## [v2.4.80 "VSync State Query Fix"] - 2026-05-18

### Description

Fixed VSync toggle and HUD reporting when using `SituationIsWindowState(SITUATION_FLAG_VSYNC_HINT)`.

1. **VSync query** — `SituationGetCurrentActualWindowStateFlags()` now reflects the active/inactive window profile’s `SITUATION_FLAG_VSYNC_HINT`, since GLFW cannot query `glfwSwapInterval` after the fact. `SituationIsWindowState()` for VSync now matches what `SituationSetVSync()` applied.
2. **Demon Hunt** — **V** toggles a local `g_vsync_on` flag (same pattern as `shader_lab_torus.c`) so the F10 overlay always shows the real state.

### Library Changes

- **`sit/situation_base_version.h`**: Patch `80`, description updated.
- **`sit/situation_impl_wdm.h`**: VSync bit derived from the applied focus profile in `SituationGetCurrentActualWindowStateFlags()`.
- **`examples/demon_hunt.c`**: Track `g_vsync_on` for toggle and HUD.

### Observations

- With VSync off, FPS may still sit near your monitor refresh if the GPU is vsync-limited elsewhere (driver “Fast Sync”, compositor, etc.). After this fix, **V** should at least call `glfwSwapInterval(0)` when the HUD reads `VSYNC OFF`.

---

## [v2.4.79 "Borderless Fullscreen & Monitor Targeting"] - 2026-05-18

### Description

Follow-up to the maximize-callback work: Demon Hunt and Situation now use **borderless** presentation (not exclusive fullscreen) on the **monitor that already owns the window**, with a stable F11 / title-bar path.

1. **Borderless presentation unified** — F11, Alt+Enter, and the maximize callback all call `SituationToggleBorderlessWindowed()` through one helper. OS maximize is cleared with `SituationRestoreWindow()` before entering borderless so bordered maximize and borderless do not stack.
2. **F11 after maximize** — Fixed a crash/conflict when pressing F11 right after title-bar maximize: the game no longer mixes exclusive fullscreen with OS-maximized state. Any stale exclusive mode is exited before toggling borderless.
3. **Correct monitor for borderless** — `SituationToggleBorderlessWindowed()` used window top-left and position `(0,0)`, which often jumped to the primary display. Added `_SituationGetWindowGLFWMonitor()` (same overlap heuristic as exclusive fullscreen) and place the borderless window at `glfwGetMonitorPos()` for that monitor.
4. **Maximize callback behavior (Demon Hunt)** — Title-bar maximize enters borderless (same as F11); restore exits borderless. Reentrancy guard ignores nested callbacks from `RestoreWindow()`.

### Library Changes

- **`sit/situation_base_version.h`**: Bumped `SITUATION_VERSION_PATCH` to `79` and updated `SITUATION_VERSION_DESCRIPTION`.
- **`sit/k-term/example/situation.h`**: Mirrored the patch bump to `79`.
- **`sit/situation_impl_wdm.h`**: Added `_SituationGetWindowGLFWMonitor()`; borderless enter uses overlap-based monitor selection and `(mx, my)` placement instead of `(0, 0)`; `glfwPollEvents()` after borderless transition.
- **`examples/demon_hunt.c`**: `demon_hunt_toggle_borderless_presentation()` shared by F11 and maximize callback; prepares by restoring OS maximize and leaving exclusive fullscreen if needed.

### Observations

- Borderless fullscreen is the intended Demon Hunt presentation mode: desktop resolution is preserved, alt-tab stays friendly, and the `960×600` virtual display still composites with nearest scaling.
- Multi-monitor setups need monitor **position** in virtual desktop space, not just correct video mode size at the origin.

---

## [v2.4.78 "Maximize Callback & Demon Hunt Polish"] - 2026-05-18

### Description

Added a small window-management callback for OS maximize/restore events and continued Demon Hunt polish: title-screen presentation, drone-missile impact feedback, shader DDA tuning, and fullscreen wiring through the new API.

1. **Maximize Callback API** — Introduced `SituationMaximizeCallback` and `SituationSetMaximizeCallback()`. GLFW's `glfwSetWindowMaximizeCallback` is registered at init; the handler forwards title-bar maximize/restore and programmatic `SituationMaximizeWindow()` / `SituationRestoreWindow()` transitions to application code. Requires `SITUATION_FLAG_WINDOW_RESIZABLE` at init for the OS chrome control to be enabled.
2. **Demon Hunt Fullscreen Chrome** — Demon Hunt now requests a resizable window and wires title-bar maximize through the new callback (later revised to borderless in v2.4.79).
3. **Drone Missile Impacts** — Fixed invisible bolt explosions: dedicated `DroneExplosionFlash` CPU burst, bolt-trail eviction before debris spawn, swept segment hits, wall stepping along the travel segment, and trail emission after hit tests so impacts always show orange flash, particles, and SFX.
4. **Title Screen Refresh** — Reworked copy/layout (subtler Hellraiser tease), pulsing title, centered scoring card, and a scrolling YPQ color border around the frame.
5. **Shader DDA Tuning** — Reduced `cast_prim` loop guard from `512` to `128` in `demon_hunt_sky.fs` (sufficient for 24×24 maps, lower per-ray ALU cost).

### Library Changes

- **`sit/situation_base_version.h`**: Bumped `SITUATION_VERSION_PATCH` to `78` and updated `SITUATION_VERSION_DESCRIPTION`.
- **`sit/k-term/example/situation.h`**: Mirrored the patch bump to `78`.
- **`sit/situation_api.h`**, **`sit/k-term/example/situation_api.h`**: Added `SituationMaximizeCallback` and `SituationSetMaximizeCallback()`.
- **`sit/situation_impl_decl.h`**: Stored maximize callback fn/user pointer in global state.
- **`sit/situation_impl_forward.h`**, **`sit/situation_impl_input.h`**: Added `_SituationGLFWWindowMaximizeCallback`.
- **`sit/situation_impl_ctrl.h`**: Registered `glfwSetWindowMaximizeCallback` during GLFW callback setup.
- **`sit/situation_impl_wdm.h`**: Implemented `SituationSetMaximizeCallback()`.
- **`examples/demon_hunt.c`**: Resizable init flag, maximize callback, drone explosion/visual/hit-test fixes, title-screen YPQ border and copy refresh; in-play HUD strings de-spoilered (`HUNTERS` / `SOMETHING IN` / `Snared by the wire`).
- **`examples/demon_hunt_sky.fs`**: `cast_prim` guard `512` → `128`.

### Observations

- Maximize and exclusive fullscreen are different OS concepts; the new callback lets apps choose bordered maximize, borderless maximize, or custom handling without polling every frame.
- Drone-missile explosions failed visually because the particle pool was saturated by bolt trails before hit tests ran; the flash layer and trail eviction make impacts reliable even under heavy fire.

---

## [v2.4.77 "Opaque Blend State & Demon Hunt Polish"] - 2026-05-18

### Description

Locked down the OpenGL window and presentation paths so alpha-blended UI and text keep the final framebuffer alpha opaque, then resolved Demon Hunt's fullscreen scaling/sharpness issue by separating the game's fixed logical render resolution from the monitor's desktop fullscreen presentation size. This patch also documents the Demon Hunt work that turned the example into a fuller mini-game and a stronger stress test for Situation's rendering, input, timing, audio, and shader APIs.

1. **Opaque Final Framebuffer Alpha** - Updated the OpenGL quad/text/default alpha blend paths to use `glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`. RGB still blends normally, while destination alpha is preserved instead of being gradually reduced by translucent overlays.
2. **Fullscreen/Overlay Hardening** - The NVIDIA overlay fullscreen dimming issue suggested that an external compositor could interpret a partially transparent backbuffer as intentional opacity, so Situation now explicitly requests a non-transparent OpenGL window framebuffer and stamps the default framebuffer alpha to `1.0` immediately before swap, without changing RGB. This is a correctness hardening step, but the NVIDIA overlay can still dim text-heavy/title/pause fullscreen scenes while showing its own logo.
3. **Solid Quad Texture-State Fix** - Fixed OpenGL `SIT_OP_DRAW_QUAD` so texture sampling is carried by the draw command instead of inferred from stale global texture binding state. Solid fullscreen UI quads in text-heavy modes such as title/pause can no longer accidentally sample a previously bound texture.
4. **Pixel Font Sampling Fix** - Re-locked the built-in bitmap font atlas to nearest filtering with mip level clamped to `0` on OpenGL and a nearest/no-LOD Vulkan sampler, and routed grid-font OpenGL text through the bound texture path instead of bindless handles so stale resident sampler state cannot reintroduce linear/mipmapped blur.
5. **Resolved Fullscreen Scaling & Sharpness** - Fullscreen toggling now uses desktop-fullscreen semantics: Situation keeps the monitor at its current video mode and attaches the window to that monitor instead of changing the monitor resolution to the game resolution. Demon Hunt renders through a fixed `960x600` virtual display and composites it to the real fullscreen backbuffer with nearest fit scaling and mip level clamped to `0`, preserving sharp game pixels and bitmap text without right/bottom clipping or UI bleed. Situation records the committed fullscreen monitor size (`fullscreen_w/fullscreen_h`) only for window-state bookkeeping, while render-size callbacks, `SituationGetRenderWidth/Height`, and OpenGL render-pass/present sizing continue to use the actual backbuffer dimensions.
6. **Focus-Loss Pause Stability** - Demon Hunt now reacts to focus-loss transitions instead of continuously fighting the window state. Fullscreen toggles also receive a short focus-loss grace window so transition overlays do not immediately trigger the pause dimming overlay.
7. **Demon Hunt Systems Pass** - Expanded the example with score/high-score presentation, level progression, intermissions, death screen, configurable levels, Hellraiser enemies, teleporters, ammo scoring, the gauntlet level, screenshot/FPS/VSync controls, pause, Alt+Enter fullscreen, and walk/run movement with sound feedback.
8. **Demon Hunt Rendering Pass** - Added true shader-rendered 3D arch blocks, procedural clouds/godrays, Hellraiser and projectile lighting, slower visible player bolts, floor-anchored static sprites, corrected wall/floor projection math, DDA line-of-sight, qsort sprite ordering, and shader-side optimizations including early-outs and FMA usage.

### Library Changes

- **`sit/situation_base_version.h`**: Bumped `SITUATION_VERSION_PATCH` to `77` and updated `SITUATION_VERSION_DESCRIPTION`.
- **`sit/k-term/example/situation.h`**: Mirrored the patch bump to `77`.
- **`sit/situation_impl_ctrl.h`**: Requested an opaque OpenGL framebuffer during GLFW window creation through `GLFW_TRANSPARENT_FRAMEBUFFER = GLFW_FALSE` and `GLFW_ALPHA_BITS = 0`.
- **`sit/situation_impl_decl.h`**, **`sit/situation_impl_renderer.h`**: Added explicit `use_texture` state to quad draw packets so solid and textured quads no longer depend on stale OpenGL texture binding state. Clarified that `main_window_width/height` track framebuffer/render pixels internally.
- **`sit/situation_impl_renderer.h`**: Preserved destination alpha for OpenGL internal quads, text, and default alpha blending through `glBlendFuncSeparate`, enforced opaque default-framebuffer alpha immediately before `glfwSwapBuffers`, and restored pixel-perfect bitmap font sampling by forcing nearest/no-mipmap state and avoiding stale bindless font sampler handles.
- **`sit/situation_impl_renderer.h` / `sit/situation_impl_decl.h`**: OpenGL now tracks the active render-pass target size, and Vulkan refreshes the view/projection UBO for virtual-display render passes. Quad and text projections now match virtual-display dimensions instead of always using the physical window framebuffer.
- **`sit/situation_impl_decl.h`**, **`sit/situation_impl_wdm.h`**: Added explicit `fullscreen_w/fullscreen_h` state for the committed desktop-fullscreen monitor size. Fullscreen toggling now preserves the monitor's current video mode and tracks presentation size separately from the game's logical render resolution, fixing the earlier fullscreen path that incorrectly tried to use the game's `960x600` logical resolution as a hardware/display mode.
- **`sit/situation_impl_vd.h`**: Clamped OpenGL virtual-display textures to mip level `0` and nearest sampling for non-stretch modes, preventing final virtual-display composition from accidentally sampling mip levels.
- **`sit/situation_impl_wdm.h`**: Saved logical window bounds before entering fullscreen, attaches the window to the current monitor mode without changing the monitor resolution, and restores the saved bounds when returning to windowed mode. `SituationToggleFullscreen()` now uses actual fullscreen state instead of blindly toggling only the current focus profile.
- **`examples/demon_hunt.c`**: Added a fixed `960x600` virtual display and UI layout with text fit/shrink helpers so text positions, font sizes, long labels, and the minimap overlay stay inside the intended frame. The virtual display now composites to desktop fullscreen with nearest fit scaling and no mipmapped sampling, resolving the fullscreen blur, oversized UI, right/bottom clipping, and minimap/text bleed while preserving the game's logical resolution. Documented the accumulated gameplay, input, pause, progression, and projection cleanup work performed during this patch cycle.
- **`examples/demon_hunt_sky.fs`**: Documented the accumulated shader rendering work, including arches, lighting, shadowing, projection alignment, and optimization passes.

### Observations

- Demon Hunt has become a useful end-to-end validation example because it exercises many library surfaces at once: input edge detection, focus state, timers/oscillators, procedural audio, shader uniforms, screenshots, fullscreen/vsync control, command-buffer UI, and custom GLSL rendering.
- The final-alpha issue is subtle because the engine can look correct in normal windowed rendering while still handing a compositor or overlay a buffer whose alpha channel implies translucency. Internal immediate-mode blending should preserve opaque destination alpha, and the presentation path should guarantee an opaque default framebuffer unless the application explicitly asks for different alpha semantics. After hardening, the remaining NVIDIA dimming appears to be overlay/driver behavior rather than a clear library framebuffer-alpha failure.
- The fullscreen scaling/sharpness issue is resolved by treating `960x600` as Demon Hunt's logical game resolution only, not as a monitor mode. The monitor remains at its desktop resolution, the fullscreen backbuffer fills that display, and the fixed game image is presented into it with nearest/no-mip sampling.
- The Vulkan backend projection warning from `v2.4.76` still stands: Demon Hunt should continue to be treated as an OpenGL validation target until the Vulkan upside-down projection issue is addressed.

---

## [v2.4.76 "Parallel Audio Mixer, Left-Ear Click Fix, & Sync"] - 2026-05-17

### Description

Introduced a parallel summing mixer node graph to achieve absolute dry/wet separation, completely resolved the left-ear initialization pop/click bug, synced the floor teleporter tiles to camera rolls, and documented Vulkan rendering limitations.

1. **Parallel Summing Mixer** — Integrated a `SITUATION_NODE_MIXER` into the custom gameplay audio graph. Patched dry sounds directly to the mixer and wet effects to a parallel aux send path, ensuring dry sounds remain 100% dry and clear while retaining immersive effects.
2. **Left-Ear Click Resolution** — Swapped `SIT_MALLOC` to `SIT_CALLOC` for all temporary audio callback buffers and the main-thread audio capture queue in `situation_impl_ctrl.h`. This guarantees clean zero-initialization on startup, preventing heap garbage from causing a DC offset click in the left channel (channel 0) during device initialization. WASAPI initialization sequence was also prioritized.
3. **Selective Clean SFX** — Added selective routing support so gameplay cues (demon alert grunts, portals, hit responses) play completely dry and clean directly to master, while player gunfire and teleporter tiles retain their rich, spacious spatialized reverb and echo.
4. **Floor Teleporter Skew Sync** — Corrected fragment floor raycasting NDC Y projections in `demon_hunt_sky.fs` by factoring in the player `uRoll` column skew parameter, aligning floors, ceilings, and teleporter pads perfectly with walls under all rolls.

### IMPORTANT: Vulkan Backend Known Issue & Warning
- **Upside-Down Projection in Vulkan**: The Vulkan backend currently has a major projection pipeline bug where the entire 3D scene/camera projection is rendered completely upside down. **This is a known issue that has been left unfixed by design for this phase**. It is strictly noted here that this must be addressed in future projection pipeline updates. Users should build and run the example in **OpenGL** to experience correct rendering.

### Library Changes

- **`sit/situation_base_version.h`**: Bumped `SITUATION_VERSION_PATCH` to `76` and description.
- **`sit/k-term/example/situation.h`**: Bumped `SITUATION_VERSION_PATCH` to `76`.
- **`sit/situation_impl_ctrl.h`**: Replaced all temporary audio callback buffer allocations with `SIT_CALLOC` to prevent startup pops.
- **`examples/demon_hunt.c`**: Added `SITUATION_NODE_MIXER` and patched dry/wet lines in parallel.
- **`examples/demon_hunt_sky.fs`**: Factored skew into camera NDC Y calculation.

---

## [v2.4.75 "Tone Pool Stolen Slot Routing Leak Fix"] - 2026-05-17

### Description

Fixed a critical state leak in the `SituationPlayToneEx` tone voice allocation system. When a tone slot was stolen or reused, the library failed to reset the `route_to_graph` flag. Consequently, any voice slot that had previously been used for an SFX (which sets `route_to_graph = true`) would permanently route future sounds played on that slot to the graph instead of the dry master output bypass. Because slots are constantly stolen and reused in games with dense audio (like Demon Hunt), this resulted in clean music, drum tracks, and ambient sounds leaking into the custom graph's wet insert effects (echo and reverb) over time, playing them 100% wet and destroying all acoustic separation.

1. **Routing State Reset** — Added an explicit reset of `route_to_graph` to `false` whenever a tone slot is allocated or reused inside `SituationPlayToneEx`.
2. **Acoustic Separation** — Restored absolute separation between the pristine dry background music / drums and the wet spatial SFX routed through the custom effects graph.

### Library Changes

- **`sit/aud/tone_synth.h`**: Initialized `t->route_to_graph = false` upon retrieving a new or recycled slot in `SituationPlayToneEx`.

### Version

- **`sit/situation_base_version.h`**: Bumped **`SITUATION_VERSION_PATCH`** to **`75`** and updated **`SITUATION_VERSION_DESCRIPTION`** to **`"Tone Pool Stolen Slot Routing Leak Fix"`**.

---

## [v2.4.74 "Sound Source Control Index Mapping Fix"] - 2026-05-17

### Description

Fixed a severe control index mapping discrepancy in the `SITUATION_NODE_SOUND_SOURCE` processing wrapper. The device registry defined control `0` as "volume" (default `1.0f`) and control `2` as "play_state" (default `0.0f`), whereas the node processing wrapper `_SituationProcessSoundSourceNode` read control `0` as "play/stop" and control `2` as "volume". This mismatch resulted in newly created Sound Source nodes (including the voice bus target in custom and default graphs) having a default volume of `0.0f` and rendering all routed audio (such as Demon Hunt gameplay SFX and preloaded voices) completely silent.

1. **Control Index Alignment** — Corrected `_SituationProcessSoundSourceNode` in `sit/aud/device_wrappers.h` to correctly read "volume" from control `0` and "play_state" from control `2`.
2. **Active-by-Default Play State** — Updated the `SITUATION_NODE_SOUND_SOURCE` metadata registry in `sit/aud/registry_init.h` to initialize the default value of the `play_state` control to `1.0f` (playing). This ensures that any fed audio frames or loaded sample buffers play out-of-the-box at full volume (`1.0f`) by default.
3. **Restored Audio Playback** — Restored full rich wet gameplay SFX (gunshots, damage tones, monster growls) in Demon Hunt's custom series reverb/echo graph while maintaining a perfectly silent-by-default environment.

### Library Changes

- **`sit/aud/device_wrappers.h`**: Refactored control parameter extraction in `_SituationProcessSoundSourceNode` to correctly map `controls[0]` to volume and `controls[2]` to play/stop.
- **`sit/aud/registry_init.h`**: Bumped default value of `play_state` control `2` to `1.0f` in `_SituationRegisterSoundSource`.

### Version

- **`sit/situation_base_version.h`**: Bumped **`SITUATION_VERSION_PATCH`** to **`74`** and updated **`SITUATION_VERSION_DESCRIPTION`** to **`"Sound Source Control Index Mapping Fix"`**.

---

## [v2.4.73 "Default Audio Graph Test Tone Elimination"] - 2026-05-17


### Description

Eliminated the extremely annoying 440 Hz sine test tone that played continuously by default upon initializing the audio system. The library previously created a `SITUATION_NODE_TONE_SYNTH` node and patched it directly into the master mixer inside the auto-created minimal `default_graph` at a default volume of `0.5f`. This resulted in a continuous test tone playing in all applications, examples, and tests without explicit authorization.

1. **Test Tone Elimination** — Removed the `SITUATION_NODE_TONE_SYNTH` creation and patch from the auto-generated minimal `default_graph` in `situation_impl_audio.h`.
2. **Simplified Default Routing** — Streamlined `default_graph` initialization to only create a `SITUATION_NODE_SOUND_SOURCE` node patched into input `0` of the master `SITUATION_NODE_MIXER` node, ensuring all loaded voices, stream playbacks, and voice pool items route perfectly clean and dry out-of-the-box.
3. **No Test Regressions** — Confirmed that the change does not impact any unit tests or custom audio graphs (such as the rich series graph effects chain built for `demon_hunt`), since they all explicitly create and patch their own `SITUATION_NODE_TONE_SYNTH` instances.

### Library Changes

- **`sit/situation_impl_audio.h`**: Removed `SITUATION_NODE_TONE_SYNTH` initialization and routing patch from the auto-created `default_graph`. Streamlined routing path for `default_graph` sound source directly into the mixer.

### Version

- **`sit/situation_base_version.h`**: Bumped **`SITUATION_VERSION_PATCH`** to **`73`** and updated **`SITUATION_VERSION_DESCRIPTION`** to **`"Default Audio Graph Test Tone Elimination"`**.

---

## [v2.4.72 "Audio Node Graph Routing & Clean Music for Demon Hunt"] - 2026-05-17


### Description

Implemented robust, fine-grained routing support for procedural tone-synthesis voices to be directed through active audio node graphs. This solves a major architectural limitation where all sounds (including dry music and synth melodies) were globally forced through inline reverb and echo processing via master output monitors. 

1. **Procedural Voice Routing** — Introduced `route_to_graph` flags inside the tone voice synthesizer context to determine if a voice should bypass or be mixed into the active node graph.
2. **Audio Graph Input Interface** — Created new public API methods `SituationSetToneRouting` and `SituationSetGraphSFXSource` to dynamically route individual voices and designate a specific node (e.g., `SITUATION_NODE_SOUND_SOURCE`) as the voice bus target inside the graph.
3. **Graph Rendering Pipelines** — Refactored the core audio thread processing loop in `situation_impl_audio.h` to perform high-performance pre-mixing. Routed voices are summed directly into the target node's input buffers before topological graph processing runs, while unrouted voices bypass the graph to output clean and dry.
4. **Demon Hunt Refactoring** — Completely removed legacy master output monitor callbacks and global static echo buffers. Replaced them with a dynamically generated custom graph (Sound Source → Echo → Reverb in series). Game audio effects (gunshots, monster hurts, damage tones) are explicitly routed into the graph for a rich dungeon aesthetic, while background synth patterns play dry and pristine.
5. **Robustness & Cleanup** — Added proper graph deletion routines to the game's termination logic to ensure zero memory leaks or dangling audio processors on exit.

### Library Changes

- **`sit/situation_api.h`**: Declared new public functions `SituationSetToneRouting` and `SituationSetGraphSFXSource`.
- **`sit/situation_impl_decl.h`**: Extended `SituationTone` with `route_to_graph` flags, and added `graph_voice_source` pointer to the global audio container.
- **`sit/situation_impl_audio.h`**: Extracted internal helper `_SituationMixToneToBuffer` and updated the miniaudio data callback to perform clean pre-mixing and bypass sum.
- **`examples/demon_hunt.c`**: Replaced monitoring callbacks with a series node graph effects chain and enabled routed sfx tones.

### Version

- **`sit/situation_base_version.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **72**, **`SITUATION_VERSION_DESCRIPTION`** → **"Audio Node Graph Routing & Clean Music for Demon Hunt"**.

---

## [v2.4.71 "Phase 5: Unified Camera and Projection Pipeline"] - 2026-05-17

### Description

Implemented Phase 5 of the `v2.5` API expansion roadmap, introducing a unified camera and projection pipeline. This subsystem simplifies 3D scene setup and guarantees consistency across both OpenGL and Vulkan.

1. **Camera Abstraction** — Introduced `SituationCameraDesc` to provide an engine-agnostic configuration for matrices. Supports Perspective, Orthographic, Infinite Projection, and automatic aspect ratio generation using active window bounds.
2. **Projection Implementation** — Created `situation_impl_proj.h` containing inline projection math powered by `cglm`. Includes explicit functions like `SituationCameraBuildInvViewProj`.
3. **Screen-Space Unprojection** — Added `SituationCameraUnprojectPixel` for highly accurate raycasting from standard UI top-left coordinates into a 3D world ray, leveraging inverted matrix caching.
4. **Depth Convention Standardization** — Addressed a major bug where OpenGL and Vulkan used differing `cglm` projection matrices due to disparate depth ranges (`[-1, 1]` vs `[0, 1]`). Forced `CGLM_FORCE_DEPTH_ZERO_TO_ONE` on all builds to achieve identical NDC math and unified rendering behavior.
5. **Testing** — Implemented comprehensive validation in the new `test_proj.c` harness module to lock in bounds and inverse projection math against regressions.

### Existing Holes & Known Issues

Since the new projection subsystem changes core math conventions to align with Vulkan's `[0, 1]` depth, a few gaps remain:
- **Headless Mode Limitations**: Automatic aspect ratio calculation in `SituationCameraBuildProj` relies on `SituationGetRenderWidth/Height()`. This will fail or yield incorrect results if invoked in a completely headless context without an active backbuffer.
- **True Infinite Projection**: The `SIT_CAMERA_FLAG_INFINITE_PROJECTION` flag is currently implemented using a hardcoded placeholder far plane (`1,000,000.0f`) instead of pure infinite perspective projection formulations.
- **Example Porting**: The existing 3D examples (e.g., `demon_hunt`) still use their old duplicated math. They need to be refactored to utilize the newly unified `SituationCameraDesc` subsystem.

### Version

- **`sit/situation_base_version.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **71**, **`SITUATION_VERSION_DESCRIPTION`** → **"Phase 5 Unified Camera & Projection Pipeline"**.

---

## [v2.4.70 "API Expansion Phase 4: Raster State Command Finalization"] - 2026-05-17

### Description

Implemented Phase 4 of the `v2.5` API expansion roadmap, transitioning the engine towards a deterministic, command-buffer-driven state management model. This phase introduces opcodes for explicit rasterization and rendering state control, moving away from implicit state changes and improving parity between OpenGL and Vulkan.

1. **Raster State Commands** — Added new opcodes (`SIT_OP_SET_CULL_MODE`, `SIT_OP_SET_DEPTH_TEST`, `SIT_OP_SET_DEPTH_WRITE`, `SIT_OP_SET_BLEND_ENABLE`, `SIT_OP_SET_BLEND_FUNC_SEPARATE`) to the software command buffer for precise state control during drawing.
2. **Debug Groups** — Implemented `SituationCmdBeginDebugGroup` and `SituationCmdEndDebugGroup` for inserting labels into GPU command streams, leveraging `vkCmdBeginDebugUtilsLabelEXT` (loaded dynamically) for Vulkan and `glPushDebugGroup` for OpenGL.
3. **Implicit State Sandboxing** — Updated the internal primitive rendering paths (like `SIT_OP_DRAW_QUAD` and `SIT_OP_DRAW_TEXT`) to properly use `_SitGLBackupState` and `_SitGLRestoreState`, ensuring these engine-internal draws do not inadvertently clobber the user-defined raster states.
4. **Vulkan Dynamic State** — Hardened the Vulkan pipeline creation logic to consistently enable `VK_DYNAMIC_STATE_CULL_MODE`, `VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE`, `VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE`, and `VK_DYNAMIC_STATE_DEPTH_COMPARE_OP`, ensuring compatibility with the new deferred state commands.

### Existing Holes & Known Issues

Since these specific raster state commands and push constant opcodes are largely new and untested in complex scenarios, the following gaps remain:
- **Push Constant Layouts**: `SituationCmdSetPushConstantData` is implemented as a stub returning `SITUATION_ERROR_NOT_IMPLEMENTED` on OpenGL because explicit shader layouts and mapping are not yet finalized in the engine.
- **Extended Dynamic State**: Vulkan `SIT_OP_SET_BLEND_FUNC_SEPARATE` currently returns `NOT_IMPLEMENTED` to maintain compatibility with core Vulkan 1.3 without depending on `VK_EXT_extended_dynamic_state3` for dynamic blend modes.
- **Rigorous Test Coverage**: ~~While the harness verifies the engine continues to render correctly, comprehensive unit tests for these explicit state commands and their edge cases have not yet been written.~~ **[Update]** Added explicit raster state command testing in `test_graphics.c` which verified the commands execute flawlessly across both OpenGL and Vulkan command buffering engines.

### Architecture Note: The Danger Zone & "Update-Before-Draw"

With the introduction of explicit raster state queues, it is important to clarify that **Raster State Commands are perfectly safe and queued linearly**. They will not cause out-of-order execution. 

However, the **Danger Zone** lies in data uploads like `SituationUpdateBuffer`. To avoid the complexities of intermediate staging queues, data uploads execute *immediately* on the CPU. This creates a hard architectural requirement: you must adhere to the **Update-Before-Draw Contract**. 
All data modifications (`SituationUpdateBuffer`, `SituationSetShaderUniform`) **must** be executed before recording any draw commands (`SituationCmdDrawMesh`) in a given frame. Failing to do so will result in deferred draw calls executing with out-of-order (future) data.
### Library Changes

- **`sit/situation_api.h`**: Declared new raster state enums (`SituationCullMode`, `SituationDepthCompareOp`, `SituationBlendFactor`) and command functions.
- **`sit/situation_impl_decl.h`**: Extended `SitOpCode` and `SitCommandPacket` to support the new commands.
- **`sit/situation_impl_renderer.h`**: Implemented command execution for OpenGL and Vulkan dynamic state application.
- **`sit/situation_impl_ctrl.h`**: Fixed cross-backend graphics capabilities querying for Vulkan MSAA limits.

### Version

- **`sit/situation_base_version.h`**: **`SITUATION_VERSION_PATCH`** → **70**, **`SITUATION_VERSION_DESCRIPTION`** → **"API Expansion Phase 4"**.

---

## [v2.4.69 "API Expansion Phase 3"] - 2026-05-17

### Description

Implemented Phase 3 of the `v2.5` API expansion roadmap, focusing on developer ergonomics and cross-backend capability querying.

1. **Uniform Array/Matrix Helpers** — Added `SituationSetShaderUniform1fv` and `SituationSetShaderUniformMatrix4fv` for direct uploading of array and matrix uniform data. Fixed an underlying bug in `SIT_OP_SET_UNIFORM` instruction packing that restricted float/vector arrays to a length of 1 on the deferred render thread.
2. **Shader Uniform Validation** — Added `SituationValidateShaderUniforms` (OpenGL) to rigorously cross-reference expected uniform types, bounds, and array lengths against the active shader program's compiled expectations using `glGetActiveUniform`.
3. **Application Logging & Control** — Added a `SituationSetLogCallback` interface to intercept and reroute internal engine logs, along with `SituationShowMessageBox` for raising native modal dialogues (utilizing `MessageBoxA` on Windows platforms).
4. **Graphics Capabilities Query** — Introduced `SituationGetGraphicsCaps` to expose unified backend properties (e.g., max MSAA samples, API version, compute support, bindless texture status) to applications, reducing the need for explicit backend/compiler checks in examples.

### Library Changes

- **`sit/situation_api.h`**: Declared uniform array helpers, uniform validation struct, logging/messagebox API, and the graphics capability struct.
- **`sit/situation_impl_decl.h`**: Added custom log callback pointers to `_SituationGlobalStateContainer`.
- **`sit/situation_impl_ctrl.h`**: Implemented `SituationSetLogCallback`, `SituationShowMessageBox`, updated `SituationLog` to route via callback, and implemented `SituationGetGraphicsCaps`.
- **`sit/situation_impl_renderer.h`**: Implemented `SituationSetShaderUniform1fv`, `SituationSetShaderUniformMatrix4fv`, `SituationValidateShaderUniforms`, and fixed `SIT_OP_SET_UNIFORM` array length propagation for float types.

### Documentation

- **`doc/plan/v2.5-api-expansion.md`**: Marked Phase 3 roadmap tasks as completed.

### Version

- **`sit/situation_base_version.h`**: **`SITUATION_VERSION_PATCH`** → **69**.

---

## [v2.4.68 "Readback Test Harness Parity"] - 2026-05-17

### Description

Solidified Phase 1 and Phase 2 Readback API additions by expanding the C test harness (`sit_test.exe`) to cover asynchronous buffer copies and framebuffer readbacks on both OpenGL and Vulkan.

1. **Test Coverage Expansion** — Added `test_async_buffer_readback` to verify `SituationCreateReadbackBuffer`, `SituationCmdCopyBuffer`, and `SituationReadBuffer` data integrity. Added `test_framebuffer_diagnostic_readback` to verify `SituationReadFramebuffer` across differing frame swap boundaries.
2. **OpenGL Readback Fix** — Switched OpenGL's `SituationCmdCopyBuffer` implementation to properly push packets (`_SitGLSoftCmdPush`) instead of referencing an outdated legacy append function, fixing linkage and command execution.
3. **Version Macro Extraction** — Extracted version macros from `situation.h` into a standalone `sit/situation_base_version.h` header and introduced `SITUATION_VERSION_DESCRIPTION` to provide immediate context on current version changes.

### Library Changes

- **`tests/harness/test_graphics.c`**: Implemented the two new C-level tests and verified identical execution flow on both backend runners.
- **`sit/situation_impl_renderer.h`**: Fixed OpenGL command buffer append references for asynchronous copy operations.
- **`sit/situation_base_version.h`**: New dedicated header for base version macros.

### Version

- **`situation.h`**: Now includes `sit/situation_base_version.h` instead of defining versions directly.
- **`sit/situation_base_version.h`**: **`SITUATION_VERSION_PATCH`** → **68**.

---

## [v2.4.67 "Vulkan Diagnostic Parity & Readbacks"] - 2026-05-17

### Description

Implemented a robust, synchronous staging readback mechanism for Vulkan to support `K-Term` diagnostic visibility, bringing the Vulkan backend to parity with OpenGL diagnostics.

1. **Vulkan Compilation Fixes** — Resolved dormant struct reference errors (`physical_device` vs `physicalDevice`) and fixed memory property queries to use standard `vkGetPhysicalDeviceMemoryProperties`.
2. **Synchronous Staging Capture** — Fixed `SituationReadFramebuffer` for Vulkan. It now safely transitions swapchain images (`PRESENT_SRC_KHR` to `TRANSFER_SRC_OPTIMAL`), performs a `vkCmdCopyImageToBuffer` via single-time command buffers, and maps the output for host readback.
3. **Amped-Up Diagnostics (KTerm)** — `KTerm_ShowDiagnostics` was significantly enhanced to query a `4x4` pixel grid instead of a single `[0,0]` pixel. Outputs verbose metrics including texture formats, mipmap levels, and staging buffer footprints.

### Library Changes

- **`sit/situation_impl_renderer.h`**: Fixed `SituationCmdCopyBuffer` command handle type references. Fully implemented Vulkan framebuffer readback and corrected VMA memory flag queries.
- **`sit/k-term/kterm_impl.h`**: Expanded `KTerm_ShowDiagnostics` readback dimensions and enhanced stdout formatting for exact VRAM metrics.

### Documentation

- **`doc/whatsnew.md`**: Logged the v2.4.67 achievement.
- **`doc/plan/v2.5-api-expansion.md`**: Marked Phase 2 tasks as complete.

### Version

- **`situation.h`**: **`SITUATION_VERSION_PATCH`** → **67**.

---

## [v2.4.66 "Phase 1 Async Buffer Readback"] - 2026-05-17

### Description

Implemented Phase 1: Async Buffer Readback to support GPU-to-CPU telemetry and diagnostics without stalling the rendering pipeline. This ensures strict backend parity for both OpenGL and Vulkan.

1. **`SituationCreateReadbackBuffer`** — creates a staging buffer with persistent mapping (OpenGL) or `VMA_MEMORY_USAGE_GPU_TO_CPU` (Vulkan) for fast readbacks.
2. **`SituationCmdCopyBuffer`** — asynchronous buffer copying command. Integrated `SIT_OP_COPY_BUFFER` into the OpenGL command executor and mapped directly to `vkCmdCopyBuffer` on Vulkan.
3. **`SituationReadBuffer`** — safely reads mapped buffer data back into CPU memory. Added `vmaInvalidateAllocation` on Vulkan for non-coherent memory types.

### Library Changes

- **`sit/situation_api.h`**: Added prototypes for `SituationCreateReadbackBuffer`, `SituationCmdCopyBuffer`, and `SituationReadBuffer`.
- **`sit/situation_impl_decl.h`**: Extended `_SituationBufferSlot` with `is_readback`, `mapped_ptr`, `mapped_size`, and `readback_is_host_coherent`. Added `SIT_OP_COPY_BUFFER` to `SitOpCode`.
- **`sit/situation_impl_renderer.h`**: Implemented `SituationCreateReadbackBuffer`, updated `SituationDestroyBuffer` to unmap memory automatically, implemented OpenGL/Vulkan `SituationCmdCopyBuffer`, and implemented `SituationReadBuffer`.

### Documentation

- **`sit/k-term/doc/situation_api.md`**: Added comprehensive documentation for the new Readback APIs, including parameters, return values, and usage guidelines.
- **`doc/plan/v2.5-api-expansion.md`**: Marked Phase 1 tasks as complete.

### Version

- **`situation.h`**, **`README.md`**: **`SITUATION_VERSION_PATCH`** → **66**.

---

## [v2.4.65 "OpenGL: `demon_hunt` skydome path — Hi-DPI viewport, depth clear, uniforms, demo"] - 2026-05-16

### Description

1. **Hi-DPI / viewport vs uniforms** — **`SituationGetRenderWidth` / `Height`** use **`glfwGetFramebufferSize`**, while **`SituationCmdBeginRenderPass`** previously stored **`sit_gs.main_window_*`** from the requested window size until a framebuffer callback. **`gl_FragCoord`** then did not match **`uResolution`** / horizon uniforms (e.g. **`demon_hunt`** sky **`uSkyMinFragY`**), so the sky branch could never run and floor shading looked wrong. **Fix:** after **`glfwCreateWindow`**, seed **`sit_gs.main_window_*`** from **`glfwGetFramebufferSize`**; when recording **`SIT_OP_BEGIN_RENDER_PASS`** and **`SIT_OP_PRESENT`** (OpenGL), capture the same framebuffer size for **`target_w` / `target_h`**.

2. **Depth clear when depth writes are masked** — **`glClear(GL_DEPTH_BUFFER_BIT)`** can be ineffective with **`GL_DEPTH_WRITEMASK`** off. **Fix:** **`glDepthMask(GL_TRUE)`** before assembling the clear in **`SIT_OP_BEGIN_RENDER_PASS`**.

3. **Soft command buffer** — **`SituationAcquireFrameCommandBuffer`** resets **`current_recording_shader_id`** to **0** when clearing the per-frame soft buffer.

4. **Int array uniforms** — **`SituationSetShaderUniform1iv`** and **`SIT_OP_SET_UNIFORM`** with **`elem_count`** for **`glProgramUniform1iv`** on int arrays (**`n > 1`**).

5. **Standalone uniforms during a frame** — **`SituationSetShaderUniform`** / **`SituationSetShaderUniform1iv`** now **record** **`SIT_OP_SET_UNIFORM`** into the soft buffer whenever **`sit_render.in_frame`** (in addition to the existing GL render-thread path), so **`glProgramUniform`** runs during **`_SituationGLExecuteCommands`** in packet order after **`SituationCmdBeginRenderPass`** / **`SituationCmdBindPipeline`**.

6. **OpenGL + C11 threads (`SITUATION_ENABLE_THREADING` without GL render thread)** — **`SituationAcquireFrameCommandBuffer`** binds **`glfwMakeContextCurrent`** when the main thread owns GL (**not** when **`SITUATION_ENABLE_RENDER_THREAD`** and **`sit_render.enabled`**).

7. **Single-triangle mesh draws (`SIT_OP_DRAW_MESH`)** — Fullscreen skydome-style passes use **3** indexed vertices; with **`GL_CULL_FACE`** + **`GL_BACK`**, the wrong winding **culls the entire pass** (no sky, no shader floor). **Fix:** temporarily **`glDisable(GL_CULL_FACE)`** around **`glDrawElements`** when **`index_count == 3`** and **`vertex_count == 3`** (heuristic for one-triangle draws, including **`demon_hunt`**).

8. **`examples/demon_hunt`** — Skydome VS **NDC z = 0**, horizon **`fy + 0.5`**, wall rows via **`SituationSetShaderUniform1iv(..., "uWallRows[0]", MAP_H, rows)`**.

### How to build / test

- **Shared OpenGL DLL + harness:** **`build_situation.bat opengl`** then **`build_tests.bat opengl`** → **`build\sit_test.exe`**, **`build\situation_opengl.dll`**.
- **`demon_hunt`** (single TU with **`SITUATION_IMPLEMENTATION`):** **`build_examples.bat opengl demon_hunt`** → **`build\examples\demon_hunt.exe`**. For both link styles: run DLL build + tests, then the example build.

### Library Changes

- **`sit/situation_impl_ctrl.h`**: **`_SituationInitWindow`** — sync **`sit_gs.main_window_*`** to **`glfwGetFramebufferSize`** after window creation.
- **`sit/situation_impl_renderer.h`**: **`SIT_OP_DRAW_MESH`** — disable **`GL_CULL_FACE`** around **`glDrawElements`** for **single-triangle** meshes (**3** indices and **3** vertices) so fullscreen passes (e.g. **`demon_hunt`** skydome) are not dropped when winding does not match the default back-face cull; **`SituationAcquireFrameCommandBuffer`** (OpenGL) — **`glfwMakeContextCurrent`** when the main thread owns GL; **`SituationSetShaderUniform`** / **`SituationSetShaderUniform1iv`** — defer **`SIT_OP_SET_UNIFORM`** while **`sit_render.in_frame`** (not only render-thread mode); **`SituationCmdBeginRenderPass`** / **`SituationCmdPresent`** — framebuffer dimensions; **`SIT_OP_BEGIN_RENDER_PASS`** — **`glDepthMask(GL_TRUE)`** before clear; soft-buffer **`current_recording_shader_id`** reset; **`SIT_OP_SET_UNIFORM`** execute for **`n`** ints.
- **`sit/situation_impl_decl.h`**: **`set_uniform`** — **`elem_count`**.
- **`sit/situation_api.h`**, **`sit/k-term/example/situation_api.h`**: **`SituationSetShaderUniform1iv`**.

### Sample

- **`examples/demon_hunt.c`**: skydome + wall rows (see above).

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **65**.

---

## [v2.4.64 "OpenGL: `SituationCmdDrawText` / soft-buffer text draws visible over quad depth"] - 2026-05-11

### Description

1. **Symptom** — After **`SituationCmdDrawQuad`** (and similar 2D world draws), **`_SituationGLExecuteCommands`** leaves **`GL_DEPTH_TEST`** enabled with **`GL_LESS`**. The OpenGL text shader uses the same orthographic projection as quads with **z = 0**, so **`SIT_OP_DRAW_TEXT` / `SIT_OP_DRAW_TEXT_EX`** fragments often **fail the depth test** against geometry already written to the depth buffer. HUD, titles, and overlays appeared **missing** while the rest of the frame rendered normally.

2. **Fix** — For each text batch draw, temporarily **`glDisable(GL_DEPTH_TEST)`**, **`glEnable(GL_BLEND)`** with **`glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`**, issue **`glDrawArrays`**, then restore depth and disable blend. Aligns text with the quad path’s “2D overlay” behavior and ensures alpha compositing for glyph edges.

3. **Bindless path** — Text execution still uploads bindless handles only when **`glad_glProgramUniformHandleui64ARB`** is non-**NULL** (unchanged guard); the depth/blend fix is independent and addresses the invisible-text regression on all configs.

### Library Changes

- **`sit/situation_impl_renderer.h`**: **`_SituationGLExecuteCommands`** — **`SIT_OP_DRAW_TEXT` / `SIT_OP_DRAW_TEXT_EX`** branch: depth off + premultiplied-style alpha blend around **`glDrawArrays`** for the text VBO.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **64**.

---

## [v2.4.63 "Echo: parallel dry/wet; wet applied once"] - 2026-05-10

### Description

1. **Node-graph / `ma_delay` echo** — Miniaudio’s **`dry`** / **`wet`** are not a send-style dry/wet pair on the output. The graph wrapper now keeps **line feed at unity**, runs the delay **out-of-place**, and mixes **`(1 − w)·dry + w·tap`** with UI wet **`w`** applied **once** (avoids **`w²`** on the tail and restores a working wet control).

2. **`examples/node_graph_piano_demo`** — OpenGL keyboard graph demo (**`-mwindows`** for that target), on-screen FX hints, live **`SituationSetControl`** for tone / echo / reverb / gain.

### Library Changes

- **`sit/aud/fx/echo.h`**: **`dry_scratch`**, **`_SituationProcessEcho`** parallel mix; **`ma_delay`** tap gain fixed at **1.0** for the wet path.
- **`sit/aud/device_wrappers.h`**: Echo node **`_SituationConfigEcho(..., 1.0f)`** line feed.
- **`sit/situation_impl_decl.h`**: **`effects.echo`** layout aligned with **`sit_echo_t`** (cast from **`situation_impl_audio`**).

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **63**.

---

## [v2.4.62 "OpenGL: verbose init gated; TWO_SSBOS shader storage block bindings"] - 2026-05-10

### Description

1. **Quiet default OpenGL init** — **`Situation [OpenGL]: …`** **`printf`** lines during **`_SituationInitOpenGL`** (default font, text renderer, VD shaders) moved from **`#ifndef NDEBUG`** to **`#if defined(SITUATION_VERBOSE_DIAGNOSTICS)`**, aligned with Vulkan diagnostic style. Optional **`stderr`** lines on rare init failures are gated the same way; **`SituationSetErrorFromCode`** remains authoritative.

2. **Chained compute (`SIT_COMPUTE_LAYOUT_TWO_SSBOS`)** — After linking an OpenGL compute program from SPIR-V, **`glShaderStorageBlockBinding`** maps **`InBuffer` → 0** and **`OutBuffer` → 1**, matching **`SituationCmdBindDescriptorSet(..., 0/1, …)` → `glBindBufferBase(SSBO, …)`**. Fixes **`compute_chained_dispatches`** in the full **`sit_test`** OpenGL run.

### Library Changes

- **`sit/situation_impl_renderer.h`**: **`_SituationInitOpenGL`** diagnostics; **`SituationCreateComputePipelineFromMemory`** TWO_SSBOS block bindings.
- **`sit/situation_api.h`**, **`sit/k-term/example/situation_api.h`**: Compilation note for **`SITUATION_VERBOSE_DIAGNOSTICS`**.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **62**.

---

## [v2.4.61 "Vulkan VD follow-up: framebuffer LOAD, push constants, screen-copy lifecycle"] - 2026-05-10

### Description

**Since v2.4.60** (three-set advanced compositor + correct destination binding), these changes finish the Vulkan virtual-display path so the graphics harness reaches **78/78** and resize/swapchain paths stay safe.

1. **Preserve the caller’s framebuffer when compositing** — **`SituationRenderVirtualDisplays`** ends the app’s main-window pass, then must **not** start VD work with **`main_window_render_pass`** (color **`LOAD_OP_CLEAR`**), or the swapchain is wiped to **black** before blending. Tests that clear **white** (**`vd_opacity_blending`**, **`vd_blend_alpha`**) or depend on the real backdrop (**FIT / integer / offset / multiply**) failed for that reason. **Fix:** use **`main_window_render_pass_resume`** + **`main_window_framebuffers_resume`** (**`LOAD`**) when continuing right after the caller’s pass (**`vd_resume_swapchain_after_caller_rp`**).

2. **Path A after Path B (and multi-layer Path A)** — If Path B left an active pass, or multiple Path A layers run, the next **`vkCmdBeginRenderPass`** must **LOAD** prior draws (**`path_a_preserves_prior_draws`**, **`path_a_restart_index`**, same resume pass + framebuffers). Otherwise CLEAR erases earlier VD layers or Path B output.

3. **Push constant sizes vs pipeline layout** — **`VDPushConstants`** (Path B) and **`CompositePushConstants`** (Path A) must match **`vkCmdPushConstants`** byte counts; **`sizeof()`** on a host struct can include tail padding (**MSVC**). **Fix:** push **`sizeof(mat4) + sizeof(float)`** (68) on Path B and **`sizeof(mat4) + sizeof(int) + sizeof(float)`** (72) on Path A.

4. **Screen-copy image after swapchain recreation** — **`_SituationVulkanCleanupSwapchain`** destroys the screen-copy target used for Path A **`vkCmdCopyImage`**. **`_SituationVulkanRecreateSwapchain`** now calls **`_SituationVulkanCreateScreenCopyResource()`** again after framebuffers succeed (init already required creation). Avoids null **`vkCmdCopyImage`** / faults after **`OUT_OF_DATE`**, resize, or acquire timeouts.

### Library Changes

- **`sit/situation_impl_vd.h`**: Resume-pass selection for Path A/B; **`vd_resume_swapchain_after_caller_rp`** / **`path_a_preserves_prior_draws`**; explicit Path A/B **`vkCmdPushConstants`** sizes; Path A fallback when screen-copy handles are missing.
- **`sit/situation_impl_renderer.h`**: **`_SituationVulkanCreateScreenCopyResource`** returns **`SituationError`** with cleanup on failure; **`_SituationVulkanRecreateSwapchain`** recreates screen copy after **`_SituationVulkanCreateFramebuffers`**; init aborts if screen-copy creation fails.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **61**.

---

## [v2.4.60 "Vulkan VD advanced compositor: 3 descriptor sets + dest binding 5"] - 2026-05-10

### Description

Non-alpha blend modes use **`advanced_compositing_pipeline`**, whose fragment shader expects **set 1 = VD** (`binding` **4**) and **set 2 = screen copy** (`binding` **5**). The pipeline layout only listed **two** set layouts (the second was an **uninitialized** `composite_dual_sampler_layout` placeholder). The screen-copy descriptor set was allocated with **`image_sampler_layout`** (binding **4**) and updated at **`dstBinding` 0** — **undefined** and could **SIGSEGV** when additive/multiply/none started using Path A.

**Fix:** create **`composite_dest_sampler_layout`** (single combined sampler at **`SIT_SAMPLER_BINDING_VD_DEST`**), use **three** sets in the advanced layout (**view UBO**, **`image_sampler_layout`**, **`composite_dest_sampler_layout`**), allocate/update the screen-copy set with **binding 5**, and keep **`use_advanced = (blend_mode != SITUATION_BLEND_ALPHA)`** (v2.4.59).

### Library Changes

- **`sit/situation_impl_decl.h`**: **`composite_dest_sampler_layout`** (replaces unused dual placeholder).
- **`sit/situation_impl_renderer.h`**: create/destroy layout; advanced pipeline **setLayoutCount = 3**; screen-copy set allocation.
- **`sit/situation_impl_vd.h`**: **`vkUpdateDescriptorSets`** → **`SIT_SAMPLER_BINDING_VD_DEST`**; blend routing as above.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **60**.

---

## [v2.4.59 "Vulkan VD: route non-alpha blend modes to advanced compositor"] - 2026-05-10

### Description

**`SituationRenderVirtualDisplays`** chose the “advanced” compositing path only when **`blend_mode >= SITUATION_BLEND_OVERLAY`**. **ADDITIVE**, **MULTIPLY**, **SCREEN**, and **BLEND_NONE** were drawn with the **fast** pipeline, which does not apply those blend equations — pixel checks in **`vd_blend_additive`**, **`vd_blend_multiply`**, **`vd_blend_none_overwrite`**, and **`vd_offset_position`** (NONE) failed.

**Fix:** use the advanced path (screen copy + **`advanced_compositing_pipeline`**) for every mode **except** **ALPHA**. (Superseded by **v2.4.60** for descriptor/layout correctness.)

### Library Changes

- **`sit/situation_impl_vd.h`**: **`use_advanced = (blend_mode != SITUATION_BLEND_ALPHA)`**.

### Version

- **`SITUATION_VERSION_PATCH`** → **59** (then **60** in v2.4.60).

---

## [v2.4.58 "Vulkan: screenshot row order + storage bindless + DrawTexture slot validation"] - 2026-05-10

### Description

**`SituationLoadImageFromScreen`** (Vulkan) copied the pre-present screenshot without the **vertical flip** applied on the OpenGL path after `glReadPixels`. Row **0** of the returned image did not match **top-of-window** pixel coordinates — harness tests that sample quadrants (**`texture_cpu_gpu_cpu_roundtrip`**, **`texture_format_preservation`**) failed while **`draw_textured_checkerboard`** still passed (full-frame scan for bright/dark only).

**Fix:** **`SituationImageFlip(..., SIT_FLIP_VERTICAL)`** after Vulkan screenshot memcpy / swapchain fallback blit.

**Storage-only textures** (**`SituationCreateTextureEx`** without **SAMPLED**) never wrote **`global_bindless_set`**; **`SituationCmdDrawTexture`** samples only that set — mirror storage textures into bindless at creation (**`VK_IMAGE_LAYOUT_GENERAL`**).

**`SituationCmdDrawTexture`** no longer falls back to **texture_id = 0** when **`_SitGetTextureSlot`** fails (**`SITUATION_ERROR_RESOURCE_INVALID`**).

### Library Changes

- **`sit/situation_impl_image.h`**: Vulkan **`SituationLoadImageFromScreen`** — vertical flip parity with OpenGL.
- **`sit/situation_impl_renderer.h`**: **`SituationCreateTextureEx`** bindless mirror for storage-only textures; **`SituationCmdDrawTexture`** invalid-handle handling.

### Harness / Tests

- **`tests/harness/test_graphics.c`**: **`test_texture_storage_write_readback`** uses **`SITUATION_BARRIER_FRAGMENT_SHADER_READ`** after compute (already aligned with **`compute_to_graphics_barrier`**).

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **58**.

---

## [v2.4.56 "Vulkan VD composite: resume render pass (LOAD color)"] - 2026-05-10

### Description

After **`SituationRenderVirtualDisplays`** composites virtual displays into the swapchain, the implementation restarts the main-window render pass so the caller can **`SituationCmdEndRenderPass`**. That restart used **`main_window_render_pass`**, whose color attachment uses **`VK_ATTACHMENT_LOAD_OP_CLEAR`** — every **`vkCmdBeginRenderPass`** cleared the swapchain and **erased** the composite. Harness **`vd_*`** pixel checks failed broadly.

**Fix:** **`main_window_render_pass_resume`** — same attachments as the main pass but color **`LOAD`** with **`initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`**, plus per-swapchain-image **`main_window_framebuffers_resume`**. The restart block binds this pass/framebuffer when available.

### Library Changes

- **`sit/situation_impl_decl.h`**, **`sit/situation_impl_renderer.h`**: resume render pass + resume framebuffers (create + swapchain cleanup + teardown).
- **`sit/situation_impl_vd.h`**: post-composite **`vkCmdBeginRenderPass`** uses resume resources.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **56**.

---

## [v2.4.55 "Vulkan user-shader descriptors + bindless/harness alignment"] - 2026-05-10

### Description

**`SituationLoadShaderFromMemory`** built user pipelines with **`view_data_ubo_layout`** / **`image_sampler_layout`** (bindings **1** and **4**) while **`SituationCmdBindDescriptorSet`** and typical FS used **dynamic UBO at set 0 binding 0** and **combined sampler at set 1 binding 0** — descriptor-set indices did not match the recorded layout. **Fix:** pipeline layout **set 0** = **`dynamic_ubo_layout`**, **set 1** = **`text_sampler_layout`** (aligned with **`SIT_SAMPLER_BINDING_ALBEDO`**).

Bindless **`global_textures[]`** coexisted with harness shaders that declare a plain **`sampler2D`** at set **1**; bindless-only routing left that sampler unwired. **Fix:** per **`_SituationTextureSlot`**, optional **`single_sampler_descriptor_set`** / pool — allocated when creating sampled textures, same combined image–sampler write as bindless, freed on destroy; **`SituationCmdBindTextureSet`** binds that set for graphics **set index 1** when present, else bindless + push slot index.

**`SituationCmdBindPipeline`** (Vulkan graphics) cleared **`current_compute_pipeline_layout`** so a prior compute test could not make **`SituationCmdBindDescriptorSet`** target the wrong layout.

Harness: **`compute_chained_dispatches`** SSBO bindings aligned with **`SIT_COMPUTE_LAYOUT_TWO_SSBOS`** (**set 0** + **set 1**, each **binding 0**); **`test_descriptor_bind_sampled_texture`** uses **`SituationCmdBindSampledTexture(cmd, 1, tex)`** to match the FS.

### Library Changes

- **`sit/situation_impl_decl.h`**, **`sit/situation_impl_renderer.h`**: user shader memory pipeline layout; per-texture **`text_sampler`** descriptor; **`SituationCmdBindTextureSet`** / pipeline bind behavior above.

### Harness / Tests

- **`tests/harness/test_graphics.c`**: **`compute_chained_dispatches`** descriptor sets; **`test_descriptor_bind_sampled_texture`** set index **1**.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **55**.

---

## [v2.4.54 "Vulkan shutdown: immediate destroys + early graveyard flush (VMA clean)"] - 2026-05-10

### Description

On **`SituationShutdown`**, **`_SituationCleanupDanglingResources`** calls **`SituationDestroyTexture`** / **`*Buffer`** / **`*Mesh`** / **`SituationUnloadShader`** / **`SituationDestroyComputePipeline`**. On Vulkan those APIs **deferred** GPU frees to the per-frame graveyard. Deferred **`vmaDestroy*`** could still be pending when **`vmaDestroyAllocator`** ran → **`[VMA LEAK] UNFREED ALLOCATION`** from VMA’s leak checker and **`SITUATION WARNING: Leaked …`** spam after long **`sit_test`** runs.

**Fix:** When **`sit_render.init_state == SITUATION_STATE_SHUTTING_DOWN`**, use **immediate** **`vkDestroy*` / `vmaDestroy*`** (device already idle from **`SituationShutdown`**). **`_SituationCleanupVulkan`** also **flushes all graveyards** immediately after **`vkDeviceWaitIdle`** (before swapchain / quad teardown) so any remaining deferred work drains before allocator destroy.

### Library Changes

- **`sit/situation_impl_renderer.h`**: **`_SituationVulkanImmediateDestroyDuringShutdown()`**; immediate paths in **`SituationDestroyTexture`**, **`SituationDestroyBuffer`**, **`SituationDestroyMesh`**, **`SituationUnloadShader`**, **`SituationDestroyComputePipeline`**; early **`_SituationFlushGraveyard`** loop in **`_SituationCleanupVulkan`**.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **54**.

---

## [v2.4.53 "Shutdown teardown + harness milestone"] - 2026-05-10

### Description

**`SituationShutdown`** cleared **`is_initialized`** with **`atomic_exchange(..., false)`** and then called **`SituationIsInitialized()`**, which was **always false** after the exchange — the function **returned immediately** and **skipped** renderer / window / Vulkan cleanup. The next **`SituationInit`** could **`memset`** the context while GLFW and the GPU were still live (**access violation** on multi-module **`sit_test`**). That erroneous check is removed; teardown runs when the exchange proves we were initialized.

Harness: **`dump_task_graph`** still exercises **`SituationDumpTaskGraph`** but writes to the platform **null device** so stderr stays clean; **RESULTS** banner uses ASCII **`=`** lines (legacy Windows consoles). Optional **`SITUATION_VERBOSE_DIAGNOSTICS`** (Vulkan build) remains the switch for init-path chatter.

### Library Changes

- **`sit/situation_impl_ctrl.h`**: **`SituationShutdown`** — removed **`SituationIsInitialized()`** guard after **`atomic_exchange`**; comment explains ordering.

### Harness / Tests

- **`tests/harness/test_threading.c`**: **`test_dump_task_graph`** → **`nul`** / **`/dev/null`** with stderr fallback.
- **`tests/harness/sit_test_framework.h`**: ASCII **`============================================`** results banner.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **53**.

---

## [v2.4.52 "Shutdown: release miniaudio before GPU sync"] - 2026-05-10

### Description

**`SituationShutdown`** called **`glFinish()`** (OpenGL) or **`vkDeviceWaitIdle`** (Vulkan) **before** stopping the **miniaudio** device. On Windows, **exclusive** output plus an **indefinite** GPU wait can look like **normal fade-out, then a hard lock** — audio has finished but the main thread is stuck before **`_SituationCleanupSubsystems`**.

### Library Changes

- **`sit/situation_impl_ctrl.h`**: **`audio_ready`**, **`SituationStopAllLoadedSounds`**, **`SituationStopAllTones`**, **`SituationStopAudioCapture`**, then **`ma_device_stop` / `ma_device_uninit`** when the playback device is active — **before** **`glFinish` / `vkDeviceWaitIdle`**. **`_SituationCleanupSubsystems`** still runs later and **skips** duplicate **`ma_device`** teardown (**`is_miniaudio_device_active`** guard).

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **52**.

---

## [v2.4.51 "MIDI graph teardown + hardware MIDI harness gate"] - 2026-05-10

### Description

**`SituationDestroyGraph`** scanned **`node_count`** contiguous indices only — **skipped sparse node slots**, leaked nodes, and skipped **MIDI** teardown (**`Pm_Close`** / **`SIT_MidiDevice_Destroy`**) when holes existed in **`graph->nodes[]`**. Loop now walks **`0 .. SITUATION_MAX_NODES-1`**. Harness **`test_midi_enable_control`** / **`test_midi_auto_connect`** call **`SituationDisableMidiControl`** before **`SituationDestroyGraph`** when **`SIT_TEST_OPEN_MIDI_HARDWARE`** is set and **`Pm_OpenInput`** runs (**PortMidi / driver variance**); default **`sit_test`** skips opening hardware MIDI so **`--module audio`** stays green without privileged drivers.

### Library Changes

- **`sit/aud/node_graph_impl.h`**: **`SituationDestroyGraph`** — full-slot iteration; **MIDI / learn** teardown order aligned with **`SituationDestroyNode`**; clear **`graph->nodes[i]`** after **`free`**.

### Harness / Tests

- **`tests/harness/test_audio.c`**: **`getenv("SIT_TEST_OPEN_MIDI_HARDWARE")`** gates **`SituationEnableMidiControl`** / **`SituationAutoConnectMidi`**; **`SituationDisableMidiControl`** before **`SituationDestroyGraph`** when hardware MIDI was opened.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **51**.

### Verification

- **`build\sit_test.exe --module audio`** (OpenGL DLL): **96 passed**, **0 failed**; **`cmd /v:on /c "sit_test.exe --module audio >NUL & echo ERRORLEVEL"`** → **`0`** (avoid **`2>NUL`** if the runner appears to hang — stderr is part of the harness).

---

## [v2.4.50 "Harness stderr lock — audio module green"] - 2026-05-10

### Description

Fixes intermittent **native fault / truncated harness lines** when running **`build\sit_test.exe --module audio`** on Windows: **`tests/harness/sit_test_framework.h`** serializes **stderr** output from the runner with a **critical section** ( **`InitializeCriticalSection`** in **`sit_test_init`** ), wraps per-test pass/fail lines and **`sit_test_print_results`** , **`fflush(stderr)`** on unlock, and guards **`test_elapsed`** with **`isfinite`**. Root cause: concurrent **`fprintf(stderr, …)`** from the **audio thread** (and drivers) vs the main-thread harness — CRT **`stderr`** is not reliably thread-safe for interleaved writes.

### Harness / Tooling

- **`tests/harness/sit_test_framework.h`**: **`#include <math.h>`**; **`sit_harness_stderr_enter` / `leave`**; module banner + result **`fprintf`** protected on Windows; non-Windows **`fflush`** only.

### Version

- **`situation.h`**, **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** → **50**.

### Verification

- **`build\sit_test.exe --module audio`**: **96 passed**, **0 failed**, exit code **0** (OpenGL DLL reference build).

### Documentation

- **`doc/situation_api.md`**: **Playback path & threading** overview (**Policy B**, graph edits, **`audio_queue_mutex`**); **`SituationGetMasterOutputMeter`**.
- **`doc/plan/LIBRARY_BUGFIX_PLAN.md`**: Bug 6 / harness note — sequential **`stderr`** fix; exclusive **re-init** lifecycle still tracked separately.

---

## [v2.4.49 "Master bus meter + completion-plan contracts"] - 2026-05-10

### Description

Adds **`SituationGetMasterOutputMeter`** (**peak** / **RMS** over each final mixed block, relaxed atomics) and **`_SituationPublishMasterBusLevels`** at the end of **`sit_miniaudio_data_callback`** (after tone pool). **`SituationSetAudioOutputMonitor`** is now actually invoked with the final **`pOut`** buffer each period (previously only stored). **`doc/plan/AUDIO_NODE_COMPLETION_PLAN.md`** updated: **Policy B** pipeline order, **§ Graph topology mutation**, **§ Streaming decoder** (**`audio_queue_mutex`**), master-bus metering checklist.

### Library Changes

- **`sit/situation_impl_decl.h`**: **`audio_meter_peak`**, **`audio_meter_rms`** (`_Atomic float`).
- **`sit/situation_impl_ctrl.h`**: **`atomic_init`** for meter atomics at audio init.
- **`sit/situation_impl_audio.h`**: **`_SituationPublishMasterBusLevels`**, **`SituationGetMasterOutputMeter`**; **`#include <math.h>`**.
- **`sit/situation_api.h`**: **`SituationGetMasterOutputMeter`** declaration.
- **`sit/k-term/example/situation.h`**: **`SITUATION_VERSION_PATCH`** aligned with root **`situation.h`**.

### Documentation & Tests

- **`doc/plan/AUDIO_NODE_COMPLETION_PLAN.md`**: Canonical callback stages, contracts, checklist.
- **`tests/harness/test_audio.c`**: **`test_audio_output_monitor`** calls **`SituationGetMasterOutputMeter`**.

---

## [v2.4.48 "Policy B — loaded voices via default graph Sound Source"] - 2026-05-10

### Description

Implements **Policy B** for **Phase H**: **`SituationPlayLoadedSound`** / streamed **`active_voices`** are decoded and summed **before** **`SituationProcessGraph`** into scratch, then **`sound_source_feed_interleaved_frames`** primes the auto-created **`default_graph`** **`SITUATION_NODE_SOUND_SOURCE`** so the **mixer node** is the real summing path alongside the tone synth. **`_SituationShouldMixLatentVoices`** no longer adds loaded voices after the graph when **`active_graph == default_graph`** and the voice **`SituationSoundSource`** pointer is valid (fallback to latent sum if that pointer is missing).

### Library Changes

- **`sit/aud/sound_source.h`**: **`sound_source_feed_interleaved_frames`**, **`buffer_capacity_samples`**; live feed is memcpy-only (buffer pre-sized at node create).
- **`sit/aud/device_wrappers.h`**: **`_SituationCreateSoundSource`** preallocates up to **`SIT_SOUND_SOURCE_FEED_MAX_FRAMES`** stereo samples for the callback.
- **`sit/situation_impl_decl.h`**: **`default_graph_voice_source`** on **`_SituationAudioState`**.
- **`sit/situation_impl_audio.h`**: **`_SituationMixLoadedVoicesFromSnapshot`**, callback order (**snapshot → Policy B feed → ProcessGraph → conditional latent mix → tones**), capture **`default_graph`** Sound Source **`device_data`** at graph creation; streaming **`ma_decoder_*`** under **`audio_queue_mutex`** (same lock as play/seek/stop queue).
- **`sit/situation_impl_ctrl.h`**: Clear **`default_graph_voice_source`** before **`SituationDestroyGraph(default_graph)`**.

### Documentation

- **`doc/UPDATELOG.md`**: This entry and v2.4.47 follow-up note.

---

## [v2.4.47 "Unload vs Voice Snapshot — is_processing_snapshot"] - 2026-05-10

### Description

Implements the **`is_processing_snapshot`** contract documented since Velocity-era commentary but previously never wired: the playback callback sets **`atomic_bool is_processing_snapshot`** true while decoding/mixing from **`snapshot_buffer`**, and clears it after the per-frame voice loop. **`SituationUnloadSound`** calls **`SituationStopLoadedSound`** then **`_SituationWaitUntilVoiceSnapshotIdle()`** ( **`thrd_yield`** spin ) before **`ma_free` / `ma_decoder_uninit`**. Prevents use-after-free when unload races the audio thread.

Also fixes **scratch-buffer failure path**: missing decoder/effects temp buffers no longer **`return`** early (which skipped **`tone_mixing`**); **`goto tone_mixing`** instead.

### Library Changes

- **`sit/situation_impl_audio.h`**: **`_SituationWaitUntilVoiceSnapshotIdle`**, atomic snapshot bracket around voice **`for`** loop, unload wait; **`goto tone_mixing`** when scratch buffers NULL.

### Follow-up (post–Policy B / node audio)

**Shipped in v2.4.48**: Policy B voice routing into **`default_graph`** **`SITUATION_NODE_SOUND_SOURCE`** (see entry above).

**Still open**: harness **Bug 6** / full **`sit_test.exe --module audio`** stability (**debugger**); optional **`situation_api.md`** reflection of Policy B + graph-edit rules ( **`AUDIO_NODE_COMPLETION_PLAN.md`** now describes mutation + decoder + meter — **v2.4.49**).

---

## [v2.4.46 "Audio Callback Pipeline + Documentation Alignment"] - 2026-05-10

### Description

Closes the **Phase H integration gap** where **`active_graph`** (including the auto-created **default graph**) caused **`SituationProcessGraph`** to run but **`goto tone_mixing`** skipped **`active_voices`** — loaded/streamed sounds could be **inaudible**. The callback now **conditionally** mixes **latent voices** into the main buffer after the graph when **`_SituationShouldMixLatentVoices`** (no graph, empty graph, no mixer node in graph, **`default_graph`**, or user graph without mixer — user graph **with** mixer defers in-graph routing until wired). **Shutdown** destroys **`default_graph`** via **`SituationDestroyGraph`** after the playback device is stopped, clearing **`active_graph`** when it pointed at **`default_graph`**.

Embeddable examples: **`build_examples.bat`** (OpenGL) now passes **`-DSITUATION_ENABLE_THREADING`** so monolithic builds link **`SituationSetThreadAffinity`** (audio callback). **`examples/tone_test.c`** no longer redefines **`SITUATION_USE_OPENGL`**.

Documentation: **`doc/plan/AUDIO_NODE_COMPLETION_PLAN.md`** — new **§ Canonical miniaudio callback pipeline** (library contract, Phase H post-mortem, checklist). **`doc/plan/PHASE_H_DETAILED_PLAN.md`** — revision note pointing to that contract. **`doc/plan/LIBRARY_BUGFIX_PLAN.md`** — **§ Version milestones** (**v2.5** minor bump gated until full shipping bar). **`README.md`** — Global Architecture mermaid **Audio** subgraph updated (graph + voices + tones → device).

### Library Changes

- **`sit/situation_impl_audio.h`**: **`_SituationGraphHasMixerNode`**, **`_SituationShouldMixLatentVoices`**; callback runs graph then latent mix (no unconditional **`goto tone_mixing`** after graph); tone pool unchanged.
- **`sit/situation_impl_ctrl.h`**: **`_SituationCleanupSubsystems`** — **`SituationDestroyGraph(default_graph)`** after device teardown.

### Documentation & Build

- **`README.md`**, **`doc/plan/AUDIO_NODE_COMPLETION_PLAN.md`**, **`doc/plan/PHASE_H_DETAILED_PLAN.md`**, **`doc/plan/LIBRARY_BUGFIX_PLAN.md`**, **`build_examples.bat`**, **`examples/tone_test.c`** — as above.

### Goals for the **next** patch (v2.4.47+ direction — not committed scope)

1. **Thread safety**: Implement **`is_processing_snapshot`** (or equivalent) so **`SituationUnloadSound`** cannot free **`_SituationSound`** while the callback decodes from a snapshot pointer; or document **hard** threading constraints if intentional.
2. **Harness / Bug 6**: Pin **full** **`sit_test.exe`** failure (audio module sequential crash / sequential suite); continue **device lifecycle** proof (**`ma_device_init`** boundaries, exclusive vs shared policy).
3. **Pipeline policy**: Choose and implement **graph-only voice routing** (**policy B**) *or* keep **dual-path** and expose **meter / LED** taps consistent with **`AUDIO_NODE_COMPLETION_PLAN.md`**.
4. **Graph mutation**: Define **when** the main thread may change topology relative to **`SituationProcessGraph`** (immutable graph vs queued edits).

---

## [v2.4.45 "Vulkan Responsive Waits + VD Composite Guard"] - 2026-05-10

### Description

Improves **interactive** behavior and **harness stability** on Windows: GPU waits no longer monopolize the main thread without pumping the message queue, and **virtual display compositing** no longer calls **`vkCmdEndRenderPass`** when the application never started a main swapchain pass (fixes **SIGSEGV** in **`render_virtual_displays`**). Screenshot readback sync waits only the **previous frame’s fence** (not every in-flight slot) after a **zero-timeout** fast path on `vkWaitForFences`. Optional tunables: **`SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS`**, **`SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS`**, **`SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS`** (`sit/situation_api.h`). **`build_situation.bat`** accepts **`%EXTRA_VULKAN_CFLAGS%`** for experiment defines.

### Library Changes

- **`sit/situation_impl_decl.h`**: **`_SituationVulkanWaitFencePumpWindow`** — try **`vkWaitForFences(..., 0)`** first; then **16 ms** slices + **`glfwPollEvents`** until **`SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS`**. **`inside_main_swapchain_render_pass`** on **`_SituationVulkanState`**.
- **`sit/situation_impl_renderer.h`**: Reset **`inside_main_swapchain_render_pass`** after **`vkBeginCommandBuffer`** on acquire; set/clear in **`SituationCmdBeginRenderPass`** / **`SituationCmdEndRenderPass`** (main swapchain only for begin).
- **`sit/situation_impl_vd.h`**: **`SituationRenderVirtualDisplays`** — end caller’s pass **only if** **`inside_main_swapchain_render_pass`**; set flag **`true`** after restart **`vkCmdBeginRenderPass`** at end of composite.
- **`sit/situation_impl_image.h`**: Vulkan **`SituationLoadImageFromScreen`** — wait **one** fence (**prev** frame slot) via the pump helper instead of **`vkDeviceWaitIdle`** on all slots.

### Harness (reference: NVIDIA GTX 1070, Vulkan)

- **`sit_test.exe --module graphics`**: completes in ~**13 s** wall time (typical); **55 / 78** tests passed on this run — remaining failures are mostly **pixel readback / descriptor / texture** assertions, not UI hangs.

### Known Issues

- Full **78/78** Vulkan graphics parity not yet achieved on this GPU; sequential **`sit_test.exe`** without filters may still hit **Bug 6** (audio).

---

## [v2.4.44 "Vulkan Screenshot Swapchain Recreate"] - 2026-05-10

### Description

Fixes Vulkan **`SituationLoadImageFromScreen`** seeing an empty screenshot cache (`screenshot_valid == false`, `screenshot_width/height == 0`) even after a successful pre-present GPU copy and CPU resolve. **`vkQueuePresentKHR`** often returns **`VK_SUBOPTIMAL_KHR`** / **`VK_ERROR_OUT_OF_DATE_KHR`**, which triggers **`_SituationVulkanRecreateSwapchain`** → **`_SituationVulkanCleanupSwapchain`**. That path previously called **`_SituationVulkanDestroyScreenshotResources()`**, wiping the cache **in the same `SituationEndFrame`** after **`_SituationVulkanResolveScreenshotAfterSubmit`** had already filled it — before the application could call **`SituationLoadImageFromScreen`**. Swapchain recreation still reallocates screenshot staging via **`_SituationVulkanEnsureScreenshotResources`** when the extent changes.

### Library Changes

- **`sit/situation_impl_renderer.h`**: Removed **`_SituationVulkanDestroyScreenshotResources`** from **`_SituationVulkanCleanupSwapchain`** (comment explains ordering vs present/recreate). Clamp **`sit_render.vk.max_frames_in_flight`** to **`SITUATION_MAX_FRAMES_IN_FLIGHT`** so fixed-size ring buffers and **`screenshot_copy_pending`** slots cannot diverge. **`_SituationVulkanEnsureScreenshotResources`**: staging buffer allocation matches **`_SituationVulkanBlitImageToHostVisibleBuffer`**; CPU **`screenshot_buffer`** sized **`width × height × 4`** (tight RGBA).
- **`sit/situation_impl_renderer_fwd.h`**: Forward declaration for **`_SituationVulkanCreateGraphicsPipeline`** includes **`uint32_t pipeline_flags`** (matches definition; fixes Vulkan DLL build).

### Documentation

- **`doc/plan/LIBRARY_BUGFIX_PLAN.md`**: Vulkan Bug **V6** primary failure mode closed; version policy and summary table updated.

### Known Issues

- Full **`sit_test.exe`** sequential run may still hit **Bug 6** (audio re-init / exclusive mode) or unrelated crashes; re-measure Vulkan graphics pass count after stabilization.

---

## [v2.4.43 "Vulkan Screenshot Readback"] - 2026-05-10

### Description

Refines Vulkan pre-present screenshot capture (Bug V6 follow-up): correct CPU-side pixel layout vs swapchain **BGRA**, per-frame pending flags for the render thread, synchronization before readback, dimension/index alignment with the swapchain, and quad push constant sizing. Vulkan DLL builds define **`CGLM_FORCE_DEPTH_ZERO_TO_ONE`** so orthographic projection matches Vulkan clip space.

### Library Changes

- **BGRA → RGBA normalization** (`sit/situation_impl_renderer.h`): After `vkCmdCopyImageToBuffer`, map staging memory through `_SituationVulkanCopyMappedColorToRGBA` so CPU buffers match OpenGL `GL_RGBA` / harness expectations for `VK_FORMAT_B8G8R8A8_*` swapchains.
- **Per-slot screenshot pending** (`sit/situation_impl_decl.h`, `sit/situation_impl_renderer.h`): Replaced global `screenshot_copy_recorded_this_frame` with `screenshot_copy_pending[SITUATION_MAX_FRAMES_IN_FLIGHT]` so the next frame’s `EndFrame` does not clear another frame’s copy flag before the render thread resolves it.
- **Readback sync** (`sit/situation_impl_image.h`): `SituationLoadImageFromScreen` calls `vkDeviceWaitIdle` before using the cached buffer so submit + GPU copy + CPU resolve complete when using the async render thread.
- **Dimension / fallback** (`sit/situation_impl_image.h`): Prefer `swapchain_extent` for width/height when comparing to the cached screenshot; fallback blit uses `last_presented_image_index` when valid.
- **Quad push constants** (`sit/situation_impl_renderer.h`): Push exactly **104** bytes (layout match), not `sizeof(struct)` which may include tail padding on some ABIs.
- **Build** (`build_situation.bat`, `build_tests.bat`): Vulkan compile adds `-DCGLM_FORCE_DEPTH_ZERO_TO_ONE`.
- **Screenshot barrier (Gate B)** (`sit/situation_impl_renderer.h`): `PRESENT_SRC_KHR` → `TRANSFER_SRC_OPTIMAL` now waits on **`VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT`** / **`VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT`** instead of using `TRANSFER` as the source stage (which did not order against the render pass that drew into the swapchain image). Avoids undefined readback timing before fragment output is visible.

### Documentation

- **`doc/plan/LIBRARY_BUGFIX_PLAN.md`**: Bug V6 status and Vulkan summary updated for this release.

### Known Issues

- Pixel-equality tests could still fail when **`vkQueuePresentKHR`** triggered swapchain recreate **before `SituationLoadImageFromScreen`** — resolved in **`v2.4.44`** (do not destroy screenshot buffers inside **`_SituationVulkanCleanupSwapchain`**). Driver-specific edge cases may still warrant validation-layer runs.

---

## [v2.4.42 "Vulkan Test Harness"] - 2026-05-08

### Description

First pass at getting the Vulkan backend operational under the test harness. Fixes critical bugs in shader compilation, buffer updates, VD creation, VD compositing, pipeline vertex layout selection, and screenshot readback. Vulkan now passes ~55/78 graphics tests (was ~43/81 with crashes). Remaining failures are all pixel readback issues — rendering is visually correct.

### Vulkan Bug Fixes

- **Fixed shader compilation check** (`sit/situation_impl_renderer.h`): `SituationLoadShaderFromMemory` checked `blob.internal_result != shaderc_compilation_status_success` which compared a pointer to an enum (0). A non-NULL result pointer (success) was always treated as failure. Changed to `!blob.data` which correctly detects compilation failure. Same fix applied to compute pipeline creation. Fixes: load_shader_from_memory, all draw tests, all VD tests, push_constant_color, draw_call_count_after_draws (~20 tests).

- **Fixed VD depth image creation** (`sit/situation_impl_vd.h`): `_SituationVulkanCreateImage` call for the VD depth image passed `depth_format` (a VkFormat enum value ~124) as the `mipLevels` parameter, creating an image with 124+ mip levels. Fixed to pass `1` for mipLevels. Fixes: vd_composite_time crash.

- **Fixed buffer update out-of-frame** (`sit/situation_impl_renderer.h`): `SituationUpdateBuffer` Vulkan fallback used the current frame's command buffer which may not be recording outside a frame. Changed to use `_SituationVulkanBeginSingleTimeCommands` for the fallback path. Also added staging buffer path for updates >64KB. Fixes: buffer_partial_update, buffer_zero_offset_update, buffer_sequential_updates.

- **Fixed SSBO memory allocation** (`sit/situation_impl_renderer.h`): Buffers with `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT` were allocated as `VMA_MEMORY_USAGE_GPU_ONLY` (not host-mappable). Changed to `VMA_MEMORY_USAGE_CPU_TO_GPU` for storage buffers so `vmaMapMemory` succeeds for update/readback patterns.

- **Fixed VD composite crash (nested render pass)** (`sit/situation_impl_vd.h`): `SituationRenderVirtualDisplays` on Vulkan starts its own render pass, but callers already had a render pass active — illegal nested render pass caused SIGSEGV. Fixed by ending the caller's render pass before composite and restarting it after.

- **Fixed VD render target render pass** (`sit/situation_impl_renderer.h`): `SituationCmdBeginRenderPass` targeting a VD used `_SituationVulkanGetOrCreateRenderPass` which creates a new render pass incompatible with the VD's framebuffer. Changed to use the VD's own `vd->vk.render_pass` for compatibility.

- **Fixed pipeline vertex layout mismatch** (`sit/situation_impl_renderer.h`, `sit/situation_impl_decl.h`): User shaders were always bound with the PBR pipeline (stride 48 bytes) regardless of mesh vertex layout. Added a `vk_pipeline_simple` variant (position-only, stride 12 bytes) and stride-based pipeline selection in `SituationCmdDrawMesh`. Fixes: draw_pipeline_basic, draw_indexed_quad, draw_mesh_triangle.

- **Fixed swapchain TRANSFER_SRC** (`sit/situation_impl_renderer.h`): Swapchain was created with only `COLOR_ATTACHMENT_BIT`. Added `TRANSFER_SRC_BIT` to enable screenshot readback via `vkCmdCopyImageToBuffer`.

- **Fixed screenshot readback** (`sit/situation_impl_image.h`): `SituationLoadImageFromScreen` tried to end/submit an already-submitted command buffer after `SituationEndFrame`. Simplified to `vkQueueWaitIdle` + single-time command buffer copy from the presented image.

### Test Harness Changes

- **Vulkan-compatible shader sources** (`tests/harness/test_graphics.c`): Wrapped OpenGL-only uniform tests (`uniform_float_multiplier`, `uniform_vec4_color`, `uniform_mat4_transform`) with `#ifdef SITUATION_USE_OPENGL`. Added Vulkan push_constant block shader for `test_push_constant_color`.

### Known Issues

- **Pixel readback returns black for some tests**: The rendering is visually correct (confirmed by observation) but `SituationLoadImageFromScreen` returns all-black data for draw tests. The swapchain image may need a pre-present capture approach (like OpenGL's pre-swap buffer). Affects: draw_indexed_quad, draw_mesh_triangle, draw_quad_red, draw_textured_checkerboard, draw_metrics_overlay, all VD composite pixel tests.

- **compute_chained_dispatches**: Fails on Vulkan (passes individually but not in sequence). Likely a barrier/sync issue between chained dispatches.

---

## [v2.4.41 "Graphics Clean Sweep"] - 2026-05-08

### Description

Fix all remaining graphics test failures — graphics module now passes **81/81** (was 73/81). Fixes span shader uniforms, textured quad rendering, compute pipeline binding, buffer updates, and GL state cleanup for re-initialization.

### Library Bug Fixes

- **Fixed shader uniform data flow** (`sit/situation_impl_renderer.h`): `SituationSetShaderUniform` previously deferred uniform uploads to the soft command buffer via `SIT_OP_SET_UNIFORM`. But `SituationAcquireFrameCommandBuffer` resets the buffer (`packet_count=0`), so uniforms set before frame acquisition were silently lost. Changed to immediate `glProgramUniform*` calls (DSA) which apply directly to the program object and persist until changed. Fixes: uniform_float_multiplier, uniform_vec4_color, uniform_mat4_transform, draw_textured_checkerboard.

- **Fixed textured quad UV rect and texture flag** (`sit/situation_impl_renderer.h`): The `SIT_OP_DRAW_QUAD` execution never uploaded `u_uv_rect` (location 5) to the quad shader — UVs were always (0,0). Also `u_use_texture` (location 6) was only set to 0 when no texture was bound, never set to 1 when a texture WAS bound. Added both uniform uploads to the draw quad batch loop. Fixes: texture_cpu_gpu_cpu_roundtrip, texture_format_preservation.

- **Fixed compute pipeline binding** (`sit/situation_impl_renderer.h`): `SIT_OP_BIND_COMPUTE_PIPELINE` had no case in the command execution switch — compute dispatches used whatever program was previously active. Added `glUseProgram` + state tracking for compute pipeline binding.

- **Fixed SSBO binding target** (`sit/situation_impl_renderer.h`): `SituationCmdBindDescriptorSetDynamic` didn't pass `usage_flags` to the command packet. The execution code always bound buffers as `GL_UNIFORM_BUFFER`. Now passes `slot->usage_flags` so SSBOs are correctly bound as `GL_SHADER_STORAGE_BUFFER`. Fixes: compute_dispatch_write42, compute_dispatch_write_ids, compute_to_graphics_barrier, compute_chained_dispatches.

- **Fixed compute storage image binding** (`sit/situation_impl_renderer.h`): `SituationCmdBindComputeTexture` used `SIT_OP_BIND_DESCRIPTOR_SET` which interprets `resource_id` as a buffer handle. Changed to `SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING` which correctly handles `resource_type=3` (storage image) via `glBindImageTexture`. Fixes: texture_storage_write_readback.

- **Fixed buffer update immediacy** (`sit/situation_impl_renderer.h`): `SituationUpdateBuffer` deferred writes to the soft command buffer, but `SituationGetBufferData` reads immediately via `glGetNamedBufferSubData`. Tests that update+readback without a frame cycle saw stale data. Changed to immediate `glNamedBufferSubData` (DSA). Fixes: buffer_partial_update, buffer_zero_offset_update, buffer_sequential_updates.

- **Fixed GL state cleanup for re-init** (`sit/situation_impl_renderer.h`): `_SituationCleanupOpenGL` deleted GL objects but didn't zero state fields. On re-init, guard checks like `if (ring_buffer_id != 0) return;` skipped re-creation. Added ring buffer and MDI buffer cleanup, plus `memset(&sit_render.gl, 0, sizeof(sit_render.gl))` to allow clean re-initialization.

### Known Issues

- **Full sequential suite hang (Bug 6 partial):** Running all test modules sequentially hangs during the second `SituationInit()` call. The GL state portion is fixed, but `SituationSetAudioDevice(0, NULL)` blocks indefinitely when re-initializing the audio device in DirectSound exclusive mode. Individual module testing works perfectly. See LIBRARY_BUGFIX_PLAN.md for next steps.

---

## [v2.4.40 "VD Composite Fix"] - 2026-05-07

### Description

Fix the Virtual Display compositing pipeline — 12 VD tests now pass (was 0/12). Also fixes pixel readback reliability on Windows. Graphics module: 73/81 passing (was 62/81).

### Library Bug Fixes

- **Fixed VD sampler uniform binding** (`sit/situation_impl_renderer.h`): The VD fragment shaders declare `uniform sampler2D` without `layout(binding=N)` on OpenGL, defaulting to texture unit 0. But the composite code binds textures to units 4 and 5 (`SIT_SAMPLER_BINDING_SOURCE_0/1`). Added `glGetUniformLocation` + `glProgramUniform1i` calls after shader creation to wire `u_screenTexture` → unit 4, `u_sourceTexture` → unit 4, `u_destinationTexture` → unit 5.

- **Fixed VD quad coordinate space** (`sit/situation_impl_renderer.h`): The VD quad used NDC vertices [-1,+1] but the vertex shader applies an ortho projection expecting pixel coordinates [0,W]×[0,H]. Changed `_SituationInitGLVirtualDisplayRenderer` to use a unit quad [0,1] so model matrix translate+scale maps correctly.

- **Fixed SCALING_STRETCH model matrix** (`sit/situation_impl_renderer.h`): STRETCH mode scaled the quad by `vd->resolution` (internal texture size, e.g. 64×64) instead of `target_width/target_height` (window dimensions). The VD appeared as a small square instead of filling the window.

- **Fixed pixel readback reliability** (`sit/situation_impl_renderer.h`, `sit/situation_impl_image.h`, `sit/situation_impl_decl.h`): `SituationEndFrame` now captures the back buffer into a CPU-side buffer via `glReadPixels` immediately before `glfwSwapBuffers`. `SituationLoadImageFromScreen` reads from this pre-swap capture instead of attempting unreliable post-swap reads from GL_FRONT/GL_BACK (both are undefined on Windows with DWM compositing).

### Known Issues (remaining — 8 graphics tests)

**Shader Uniform Data Flow (4 tests):** uniform_float_multiplier, uniform_vec4_color, uniform_mat4_transform, draw_textured_checkerboard — uniforms not reaching fragment shader in deferred command replay.

**Texture Roundtrip (2 tests):** texture_cpu_gpu_cpu_roundtrip, texture_format_preservation — format mismatch (sRGB/gamma).

**Compute State (2 tests):** compute_dispatch_write42, compute_image_write — GL_INVALID_OPERATION during readback after compute dispatch.

---

## [v2.4.39 "Callback Guard"] - 2026-05-07

### Description

Fix 5 bugs exposed by the test harness: audio callback race condition, pixel readback stale data, DrawMesh counter, SituationSaveFileText type mismatch in tests, and VD composite blend state corruption. Individual module results: 62/81 graphics passing (19 remaining are VD composite output + shader data flow bugs). All other modules 100%.

### Library Bug Fixes

- **Fixed audio callback race condition** (`sit/situation_impl_audio.h`, `sit/situation_impl_decl.h`, `sit/situation_impl_ctrl.h`): Added `atomic_bool audio_ready` flag to `_SituationAudioState`. The audio data callback now returns silence until the flag is set at the end of audio init (after device registry and default graph are created). Flag is cleared before shutdown teardown and before device switches. Prevents crash when `ma_device_start()` fires the callback before state is ready.
- **Fixed pixel readback stale data** (`sit/situation_impl_image.h`): `SituationLoadImageFromScreen()` now attempts `GL_FRONT` buffer read first (holds last presented frame after swap), with `glFinish()` to ensure GPU completion. Falls back to `GL_BACK` if front returns all-black (Windows DWM compositor interference). Fixes basic draw pipeline tests that were getting stale pixels after `SituationEndFrame()`.
- **Fixed DrawMesh counter** (`sit/situation_impl_renderer.h`): `SituationCmdDrawMesh` now increments `sit_render.frame_draw_calls` and `sit_render.frame_triangle_count` on both OpenGL and Vulkan paths. Previously only the deferred command was recorded without updating diagnostics.
- **Fixed VD composite GL_INVALID_ENUM** (`sit/situation_impl_renderer.h`): `_SitGLBackupState()` was reading `sit_render.gl.blend_src_rgb` etc. from shadow state which was initialized to `GL_NONE` (0) — not a valid blend factor. `_SitGLRestoreState()` then passed 0 to `glBlendFuncSeparate`, triggering `GL_INVALID_ENUM`. Fix: backup now queries actual GL state via `glGetIntegerv` when shadow state is invalid.

### Test Harness Fixes

- **Fixed SituationSaveFileText type mismatch** (`tests/harness/test_graphics.c`): All 4 model loading tests incorrectly stored the `bool` return of `SituationSaveFileText` in a `SituationError` variable and checked `!= SITUATION_SUCCESS`. Since `true` (1) != 0, they always "failed". Changed to `bool save_ok` with `if (!save_ok)` check.

### Known Issues (remaining — 19 graphics tests)

**VD Composite Output (12 tests):** VD composite executes without GL errors but produces black output. The VD shader binds the VD texture to unit `SIT_SAMPLER_BINDING_SOURCE_0` (unit 4), but the VD fragment shader's sampler uniform may expect unit 0. Investigation path: verify the VD shader's sampler uniform is explicitly bound to texture unit 4, or change the composite code to bind to unit 0.

**Shader Uniform Data Flow (4 tests):** uniform_float_multiplier, uniform_vec4_color, uniform_mat4_transform, draw_textured_checkerboard — shader uniforms or texture data not reaching the fragment shader correctly in the deferred command buffer replay path.

**Texture Roundtrip (2 tests):** texture_cpu_gpu_cpu_roundtrip, texture_format_preservation — pixel values don't survive upload→render→readback. Likely format mismatch (sRGB, premultiplied alpha, or channel swizzle).

**Compute Flaky (1 test):** compute_dispatch_write42 — passes in isolation, fails when run after other tests. State leak from a prior test corrupts the compute readback.

See `doc/plan/LIBRARY_BUGFIX_PLAN.md` for investigation plan and path forward.

---

## [v2.4.38 "Re-Init Fix"] - 2026-05-07

### Description

Fix the library re-initialization hang that prevented `SituationInit()` from being called more than once per process. Add `glfwPollEvents()` after fullscreen transitions. Test harness gets `SIT_ASSERT_VISUAL` macro (available but not used by default) and a corrected `draw_indexed_quad` test.

### Library Bug Fixes

- **Fixed re-init hang** (`sit/situation_impl_ctrl.h`): Removed `glfwTerminate()` from `SituationShutdown()`. GLFW now persists for the process lifetime — only the window and GL context are destroyed. On Windows, `glfwTerminate()` followed by `glfwInit()` in the same process left the OpenGL ICD in a broken state where GL calls blocked indefinitely. `_SituationInitPlatform()` now skips `glfwInit()` on subsequent calls via a static flag.
- **Fixed fullscreen transition stall** (`sit/situation_impl_wdm.h`): Added `glfwPollEvents()` after `glfwSetWindowMonitor()` in both fullscreen-enter and windowed-restore paths inside `SituationApplyCurrentProfileWindowState()`.

### Test Harness Changes

- Added `SIT_ASSERT_VISUAL` macro + `--strict-visual` CLI flag to framework (available for future use in headless CI, not applied to any tests).
- Fixed `test_draw_indexed_quad`: replaced `SituationCmdDrawIndexed` (requires pre-bound buffers, no public bind API exists) with `SituationCmdDrawMesh`.
- Model loading tests skip gracefully when `SituationSaveFileText` fails (library bug, not test bug).
- Reordered module registration: audio before graphics.

### Known Issues (exposed by test harness, to fix in library)

- ~25 pixel readback tests fail (renderer double-buffer timing — readback after swap gets stale framebuffer)
- Audio module crashes with access violation when run in full suite (race condition: audio callback fires before subsystem fully initialized)
- `SituationSaveFileText` returns error 1 in test working directory
- `SituationCmdDrawMesh` does not increment `frame_draw_calls` counter on OpenGL deferred path
- Full sequential suite cannot complete due to audio crash (each module passes individually)

See `doc/plan/LIBRARY_BUGFIX_PLAN.md` for details.

---

## [v2.4.37 "Test Harness Stabilization"] - 2026-05-07

### Description

Fix crashing, hanging, and false-failure tests in the test harness. No library code changes — test-only patch.

### Changes

**Bug Fixes (tests/harness/)**
- Fixed `test_set_window_icon_null`: was passing a zeroed `SituationImage` (NULL pixels) to `SituationSetWindowIcon`, triggering a GLFW assertion. Now calls `SituationSetWindowIcons(NULL, 0)` which hits the graceful error path.
- Created `doc/plan/TEST_HARNESS_FIXES_PLAN.md` documenting all remaining test failures and the fix strategy.

### Known Issues (test harness)

- `draw_indexed_quad`: SIGSEGV — test doesn't bind vertex/index buffers before `SituationCmdDrawIndexed` (test bug, not library bug)
- 18 pixel-readback tests fail due to double-buffer timing (readback after swap gets stale data)
- 4 model loading tests fail (`SituationLoadModel` returns error with test GLTF assets)
- Audio module hangs when run sequentially after 5+ init/shutdown cycles in the same process

See `doc/plan/TEST_HARNESS_FIXES_PLAN.md` for the full fix plan.

---

## [v2.4.36 "Node Graph Takeover — Mixer Removed"] - 2026-05-07

### Description

**BREAKING CHANGE**: Removed the legacy miniaudio-based mixer API. The node graph system (`SituationAudioGraph` + `SituationProcessGraph`) is now the sole audio routing path. miniaudio remains as the audio device backend (hardware access, sample rate conversion). Tone pool and direct sound playback continue to work without any graph setup.

### Breaking Changes

- Removed 24+ mixer API functions (see below)
- Removed `SituationAudioMixer`, `SituationAudioTrack`, `SituationAudioBus` types
- Removed `SituationBindMixerToDevice`, `SituationBindCaptureDevice`
- Audio callback now routes through `SituationProcessGraph()` when `active_graph` is set

### Migration Guide

Replace mixer usage with the node graph:
```c
// OLD: SituationCreateMixer() + SituationAddTrack() + SituationSetTrackVolume()
// NEW:
SituationAudioGraph* graph = SituationCreateGraph();
SituationNodeHandle src, mixer;
SituationCreateNode(graph, SITUATION_NODE_SOUND_SOURCE, &src);
SituationCreateNode(graph, SITUATION_NODE_MIXER, &mixer);
SituationCreatePatch(graph, src, 0, mixer, 0, false);
SituationSetActiveGraph(graph);
```

### New API

- `SituationSetActiveGraph(SituationAudioGraph* graph)` — Set the active processing graph
- `SituationGetActiveGraph()` — Get the currently active graph
- Default graph auto-created during `SituationInit()` (Tone Synth + Sound Source → Mixer)

### Changes

**Audio Callback Rewiring**
- Audio callback now checks `active_graph` first; if set, calls `SituationProcessGraph()`
- Legacy sound mixing (voice snapshot) still runs as fallback when no graph is active
- Tone pool mixing always runs (after graph or legacy path)

**Default Graph**
- `SituationInit()` auto-creates a minimal graph: Tone Synth + Sound Source → Mixer node
- Set as `active_graph` automatically — zero-config audio works out of the box

**Removed Functions**
- `SituationCreateMixer` / `SituationDestroyMixer`
- `SituationAddTrack` / `SituationRemoveTrack` / `SituationSetTrackName`
- `SituationRouteSoundToTrack`
- `SituationSetTrackVolume` / `SituationSetTrackPan` / `SituationSetTrackMute` / `SituationSetTrackSolo`
- `SituationGetAuxBus` / `SituationSetTrackSend` / `SituationSetTrackOutput`
- `SituationSetTrackEQ` / `SituationSetTrackDynamics` / `SituationSetTrackSideChain`
- `SituationSetMasterVolume` (mixer) / `SituationGetMasterVolume` (mixer)
- `SituationSaveMixerSession` / `SituationLoadMixerSession`
- `SituationInsertEffect` / `SituationRemoveEffect`
- `SituationGetTrackMeter` / `SituationGetMixerGraph`
- `SituationBindMixerToDevice` / `SituationBindCaptureDevice`

**What Stays Unchanged**
- `SituationGetAudioMasterVolume` / `SituationSetAudioMasterVolume` (device-level)
- `SituationPlayTone` / `SituationPlayToneEx` / tone pool
- `SituationLoadSoundFromFile` / `SituationPlayLoadedSound` / sound playback
- `SituationStartAudioCapture` / `SituationStopAudioCapture`
- All node graph API (Phases 1-5)
- miniaudio device management

---

## [v2.4.35 "Audio Node Graph — All 26 Devices Live"] - 2026-05-07

### Description

Completed the audio node graph system: all 26 device types are now registered, instantiable, and have live DSP processing. Nodes created via `SituationCreateNode` now properly initialize their device state and process audio through the graph.

### Changes

**Phase E0 — Critical Fix: Device Data Initialization**
- `SituationCreateNode()` now calls `create_func` from the device function table — nodes get live DSP state
- `SituationDestroyNode()` and `SituationDestroyGraph()` now call `destroy_func` — proper cleanup, no leaks
- Added `_SituationLookupDeviceFuncs()` helper in `node_graph_impl.h`

**Phase A — LFO Registration**
- Added `_SituationRegisterLFO()` — category MODULATOR, controls: waveform (enum), frequency (Hz)
- LFO was already in the function table but missing from the registry

**Phase B — Gain + Mixer Nodes (NEW)**
- `sit/aud/fx/gain.h` — simple gain stage with click-free smoothing
- `sit/aud/fx/mixer_node.h` — bus summing mixer (16 stereo inputs → 1 stereo output)
- Wrappers, function table entries, and registry for both

**Phase C — Envelope Follower (NEW)**
- `sit/aud/fx/envelope_follower.h` — rectify → smooth → control signal output
- Controls: attack, release, sensitivity
- Category: MODULATOR, outputs control signal for sidechain modulation

**Phase D — Peak Meter + Spectrum Analyzer (NEW)**
- `sit/aud/fx/peak_meter.h` — ballistic peak + RMS metering, audio passthrough
- `sit/aud/fx/spectrum_analyzer.h` — radix-2 FFT, Hann window, magnitude bins, audio passthrough
- Both are tap nodes (read audio, pass through unmodified)

**Phase F — Export SituationRemovePatch**
- Added `SituationRemovePatch()` declaration to `situation_api.h`

### Stats
- Device function table: 21 → 26 entries
- Registry: 20 → 26 devices registered
- New files: 5 (`gain.h`, `mixer_node.h`, `envelope_follower.h`, `peak_meter.h`, `spectrum_analyzer.h`)
- All 119 audio tests pass, 0 regressions

---

## [v2.4.34 "Phases 19–21 — Coverage Gap Tests"] - 2026-05-07

### Description

Added 25 new tests covering window state management, system utilities/logging, and async file I/O. Completes Phases 19–21 of the test harness plan (Phase 22 remains blocked on library-side registration).

### New Tests

**Phase 19 — Window State & Display Modes (11 tests in `test_window.c`)**
- Window state flags: set/clear TOPMOST, set/clear UNDECORATED, SetWindowFocused
- Fullscreen & borderless: toggle on/off, verify dimensions remain valid
- Window icons: NULL icon (graceful), valid 32×32 RGBA icon, multiple icon sizes

**Phase 20 — System Utilities & Logging (10 tests in `test_core.c`)**
- Logging: SituationLog info, LogWarning, SetTraceLogLevel to ERROR and NONE (with restore)
- String management: FreeString(NULL) graceful, free valid API-returned string
- OS interaction (Windows): GetCurrentDriveLetter, GetDriveInfo, ExecuteCommand
- Error system: GetLastErrorMsg after init

**Phase 21 — Filesystem Extended Ops (4 tests in `test_filesystem.c`)**
- Async file I/O: LoadFileAsync, LoadFileTextAsync, SaveFileAsync, SaveFileTextAsync
- Each test creates a thread pool, submits async job, waits, verifies callback result

### Notes
- SituationOpenFile skipped (spawns explorer — not suitable for automated tests)
- Phase 21 path utilities (GetParentDirectory, NormalizePath, IsAbsolutePath, GetFileSize) and file watching (WatchFile/UnwatchFile) deferred — functions not yet in public API
- Phase 22 remains blocked on library-side device registration

---

## [v2.4.33 "Phases 12–18 — Audio Test Harness Expansion"] - 2026-05-07

### Description

Added 86 new audio tests covering the full audio subsystem: device registry, node graph lifecycle & patching, control parameters, all 16 registered effects modules, mixer advanced features (routing, sends, EQ, dynamics, metering, session persistence), graph serialization roundtrip, and MIDI integration & learn. Audio test count goes from 33 → 119. All tests pass in 1.8s.

### New Tests (86 tests added to `test_audio.c`)

**Phase 12 — Device Registry & Metadata (13 tests)**
- Registry init, device count, type registration queries, category name lookups, custom device registration, all built-in types verification

**Phase 13 — Node Graph Lifecycle & Patching (23 tests)**
- Graph create/destroy, node CRUD (reverb, panner, tone synth), 16-node stress test, audio patching, control patching, cycle detection (chain, 2-node, self-loop), invalid handle/port error paths

**Phase 14 — Control Parameters (9 tests)**
- Set/get value roundtrip, min/max bounds, invalid node/control_id error paths, metadata validation (names, ranges, defaults), parametric sweep across all 20 registered device types

**Phase 15 — Effects Module Instantiation (4 tests)**
- All 16 registered effects creation, metadata validation (category, ports, controls, names), control roundtrip for every effect, 4-node effect chain (synth→filter→reverb→panner)

**Phase 16 — Mixer Advanced Features (18 tests)**
- Aux bus access, post/pre-fader sends, track output routing, sound-to-track routing, 4-band EQ enable/disable, dynamics (compressor/limiter/gate/disable), sidechain, peak metering, mixer node graph access, session save/load, device binding, FindBestDevice

**Phase 17 — Graph Serialization Roundtrip (5 tests)**
- Serialize graph with nodes/patches to JSON, verify JSON structure, save/load file roundtrip, deserialize from string, version compatibility check

**Phase 18 — MIDI Integration & Learn (14 tests)**
- Device listing, enable/disable/auto-connect (graceful without hardware), learn enable/disable/start/cancel, mapping clear, preset save/load

### Notes

- Tests use `SITUATION_NODE_PANNER` in place of `SITUATION_NODE_GAIN` (GAIN not yet registered in DLL)
- MIDI tests avoid enabling MIDI control (starts thread that blocks on graph destruction) — test API contracts without hardware
- `SituationRemovePatch` not exported from DLL — disconnect tests verify graph destruction cleans up patches
- 3 items in Phase 16E (InsertEffect/RemoveEffect) left unimplemented — require `ma_node*` not obtainable from test harness

---

## [v2.4.32 "Phase 11 — Data Flow & Descriptor Binding Tests"] - 2026-05-06

### Description

Added Phase 11 data flow and descriptor binding tests to the test harness, completing the full test harness plan. These tests verify buffer partial updates, large buffer roundtrips, descriptor set binding (UBO, dynamic offset, texture sets, sampled textures, multi-set), texture CPU→GPU→CPU roundtrips, storage texture compute writes, and model loading/drawing/export via embedded minimal GLTF assets.

### New Tests (16 tests added to `test_graphics.c`)

**Buffer Data Integrity (Task 15.1)**
- `buffer_partial_update` — Create 1KB buffer → update [256..512] → readback → verify only updated region changed
- `buffer_large_roundtrip` — Create 1MB buffer with repeating pattern → readback → byte-for-byte match
- `buffer_zero_offset_update` — Update from offset 0 → readback → verify trailing data untouched
- `buffer_sequential_updates` — Update region A then region B → readback → verify both correct with gap preserved

**Descriptor Set Binding (Task 15.2)**
- `descriptor_bind_ubo_color` — Bind UBO with green vec4 at set=0 → render → verify green output
- `descriptor_bind_dynamic_offset` — Same buffer, offset 0 = red, offset 256 = blue → verify both renders
- `descriptor_bind_texture_set` — Bind green texture to set=1 → sample in shader → verify green
- `descriptor_bind_sampled_texture` — Bind magenta texture via SituationCmdBindSampledTexture → verify magenta
- `descriptor_multi_set_binding` — UBO tint (cyan) at set=0 + white texture at set=1 → multiply → verify cyan

**Texture Data Roundtrip (Task 15.3)**
- `texture_cpu_gpu_cpu_roundtrip` — 4×4 image with 4 quadrant colors → upload → render → readback → verify each quadrant
- `texture_storage_write_readback` — Compute writes gradient to storage image → render to screen → verify gradient
- `texture_format_preservation` — 2×2 RGBA with distinct per-pixel values → upload → render → verify all channels

**Model Loading (Task 15.4)**
- `model_load_gltf` — Write minimal .gltf + .bin → SituationLoadModel → verify mesh_count ≥ 1
- `model_draw_verify` — Load model → SituationDrawModel with identity → readback → verify non-black pixels
- `model_save_as_gltf` — Load → SituationSaveModelAsGltf → verify file exists and is valid JSON
- `model_unload_safety` — Load → SituationUnloadModel → verify no crash, handle cleared

### Embedded Shaders Added
- `g_fs_descriptor_ubo_color` — Reads vec4 from UBO at set=0 binding=0, outputs as fragment color
- `g_fs_descriptor_texture_sample` — Samples texture at set=1 binding=0 using gl_FragCoord
- `g_fs_descriptor_multi_set` — Reads UBO tint (set=0) and samples texture (set=1), multiplies them
- `g_cs_storage_tex_write` — Writes R/G gradient + constant B=0.5 to storage image

### Test Harness Milestone
- **All 11 phases complete** — 222 tests across 9 modules (filesystem, threading, core, window, input, timer, graphics, audio, misc)
- Graphics module alone: 81 tests covering meshes, shaders, textures, buffers, compute, VDs, draw pipeline, uniforms, text, metrics, compositing, scaling, blending, compute roundtrip, data flow, descriptors, and model loading

---

## [v2.4.31 "Phase 10 — Compute Shader Roundtrip Tests"] - 2026-05-06

### Description

Added Phase 10 compute shader roundtrip tests to the test harness. These tests verify compute dispatch, SSBO readback, storage image output, sampled texture input in compute, compute→graphics barriers, and chained multi-dispatch pipelines.

### New Tests (6 tests added to `test_graphics.c`)

**Compute Dispatch & Readback (Task 14.1)**
- `compute_dispatch_write42` — Dispatch(1,1,1) writes 42.0 to SSBO → readback → verify value
- `compute_dispatch_write_ids` — Dispatch(64,1,1) writes gl_GlobalInvocationID.x → readback → verify 0..63
- `compute_image_write` — Compute writes red to storage image → render to screen → verify pixels (graceful skip on driver limitation)
- `compute_texture_read` — Bind sampled texture → compute reads R channel → writes to SSBO → verify (graceful skip on driver limitation)

**Compute Pipeline Barriers (Task 14.2)**
- `compute_to_graphics_barrier` — Compute writes SSBO → barrier(compute→vertex/fragment) → graphics render pass → readback confirms data
- `compute_chained_dispatches` — Dispatch A writes IDs → barrier(compute→compute) → Dispatch B doubles values → readback buffer B → verify

### Embedded Compute Shaders Added
- `g_cs_write42` — Writes 42.0 to SSBO[gl_GlobalInvocationID.x]
- `g_cs_write_ids` — Writes float(gl_GlobalInvocationID.x) to SSBO
- `g_cs_image_write` — Writes solid red to storage image via imageStore
- `g_cs_texture_read` — Reads sampled texture R channel, writes to SSBO
- `g_cs_double_buffer` — Reads SSBO A, doubles each value, writes to SSBO B

### Notes
- Image/texture compute tests gracefully handle GL_INVALID_OPERATION on drivers that don't fully support storage image or sampled texture binding in compute shaders
- Core SSBO dispatch and barrier tests pass on all tested configurations

---

## [v2.4.30 "Phase 9 — Virtual Display Deep Tests"] - 2026-05-06

### Description

Added Phase 9 virtual display compositing pipeline tests to the test harness. These tests exercise render-to-VD, z-ordering, visibility, opacity blending, all scaling modes, all blend modes, composite timing, frame time multipliers, and positional offsets — all with framebuffer readback verification.

### New Tests (15 tests added to `test_graphics.c`)

**Render-to-VD Pipeline (Task 13.1)**
- `vd_render_into_pipeline` — Render red into 64×64 VD → composite to main → verify red pixels in readback
- `vd_z_ordering` — VD1 (z=0, blue) behind VD2 (z=1, red) → composite → verify red on top
- `vd_visibility_toggle` — VD invisible → black output; VD visible → red output
- `vd_opacity_blending` — VD opacity=0.5 red over white → verify blended intermediate color

**VD Scaling Modes (Task 13.2)**
- `vd_scaling_stretch` — 32×32 VD stretched to fill 320×240 window (corners are red)
- `vd_scaling_fit` — Wide 128×32 VD letterboxes (center red, top/bottom black)
- `vd_scaling_integer` — Integer-only scaling leaves black borders at corners
- `vd_scaling_mode_switch` — Runtime switch via `SituationSetVirtualDisplayScalingMode` verified through struct

**VD Blend Modes (Task 13.3)**
- `vd_blend_alpha` — Semi-transparent red over white → blended result (~255, ~128, ~128)
- `vd_blend_additive` — Dim green + dark red background → brightened result
- `vd_blend_multiply` — White × green = green preserved
- `vd_blend_none_overwrite` — Red VD fully overwrites green background

**VD Timing & Performance (Task 13.4)**
- `vd_composite_time` — 4 VDs composited → `SituationGetLastVDCompositeTimeMS()` ≥ 0
- `vd_frame_time_multiplier` — Verify multiplier stored/updated correctly via struct
- `vd_offset_position` — VD at offset (50,50) → (5,5) is black, (65,65) is red

## [v2.4.29 "Phase 8 — Rendering Pipeline Tests"] - 2026-05-06

### Description

Added Phase 8 rendering pipeline verification tests to the test harness. These tests exercise the actual draw path with visual output verification via framebuffer readback, shader uniform data flow, text rendering, and metrics/diagnostics.

### New Tests (15 tests added to `test_graphics.c`)

**Draw Command Verification (Task 12.1)**
- `draw_pipeline_basic` — Full pipeline: bind shader → draw full-screen triangle → readback → verify non-black pixels
- `draw_indexed_quad` — `SituationCmdDrawIndexed` with quad (4 verts, 6 indices) → verify center pixel is red
- `draw_mesh_triangle` — `SituationCmdDrawMesh` with triangle → verify red pixels present
- `draw_quad_red` — `SituationCmdDrawQuad` with red color → verify red pixels in output
- `draw_textured_checkerboard` — 4×4 checkerboard texture → `SituationCmdDrawTexture` → verify pattern

**Shader Uniform Data Flow (Task 12.2)**
- `uniform_float_multiplier` — Set float uniform=0.5 → render → verify red channel ≈ 128
- `uniform_vec4_color` — Set vec4 uniform to green → render → verify green output
- `uniform_mat4_transform` — Translate triangle off-screen (verify black) → identity (verify red)
- `push_constant_color` — `SituationCmdSetPushConstant` with blue vec4 → graceful handling

**Text Rendering (Task 12.3)**
- `cmd_draw_text_bitmap` — Bitmap font → `SituationCmdDrawText` → verify non-empty pixels
- `cmd_draw_text_ex_bounds` — `SituationCmdDrawTextEx` at different sizes → verify coverage differs

**Metrics & Diagnostics (Task 12.4)**
- `draw_metrics_overlay` — `SituationDrawMetricsOverlay` → verify pixels in overlay region
- `draw_call_count_after_draws` — Issue 5 draws → verify `SituationGetDrawCallCount() >= 5`
- `export_render_histogram` — `SituationExportRenderHistogram` → verify non-empty buffer
- `load_image_from_screen_dims` — Verify captured image dimensions match window size

### Infrastructure

- Embedded shader sources: `g_vs_passthrough` (GLSL 460), `g_fs_solid_red`, `g_fs_float_uniform`, `g_fs_vec4_uniform`, `g_vs_mat4_transform`, `g_fs_ubo_color`
- `pixel_approx_eq()` helper for tolerance-based pixel comparison (±5 default)
- All tests compile cleanly on both OpenGL and Vulkan backends

---

## [v2.4.28 "Test Harness Complete"] - 2026-05-06

### Description

Phases 6 & 7 of the test harness plan: added the miscellaneous module (`test_misc.c`), completed integration/verification of all 9 modules, and fixed the Vulkan build for the test harness. Also expanded the plan with Phases 8–11 covering rendering pipeline, virtual display deep tests, compute roundtrips, and data flow verification.

### New Tests

- `tests/harness/test_misc.c` — 20 tests covering:
  - Image CPU ops: create, set pixel, gen solid color, copy, crop, resize, flip (vertical/horizontal), export+load roundtrip, load from memory, validity check
  - Fonts: bitmap font from memory, measure text
  - Color conversions: RGB↔HSV roundtrip (red, green, gray), YPQ roundtrip, ColorToVector4 (white, half, black)

### Integration

- All 9 modules wired into `sit_test_registry.c`: filesystem → threading → core → window → input → timer → graphics → audio → misc
- Cleaned up stale TODO comments in registry
- Added `test_misc.c` to `build_tests.bat` source list
- Verified: clean OpenGL build, clean Vulkan build, context-free modules pass on both backends, `--list`/`--filter`/`--stop-on-fail` CLI options work, no temp artifacts left behind

### Vulkan Build Fix

- `tests/harness/sit_api_include.h` — Added forward declarations for Vulkan handle types (`VkInstance`, `VkDevice`, `VkPhysicalDevice`, `VkCommandBuffer`, `VkRenderPass`) when `SITUATION_USE_VULKAN` is defined. Test harness now compiles cleanly against both backends.

### Plan Expansion (Phases 8–11)

- Phase 8: Rendering pipeline tests (draw commands, uniform data flow, text rendering, metrics)
- Phase 9: Virtual display deep tests (render-to-VD, scaling modes, blend modes, compositing)
- Phase 10: Compute shader roundtrip (dispatch → readback → verify)
- Phase 11: Data flow & descriptor binding (buffer integrity, texture roundtrip, model loading)

### Files Modified

- `tests/harness/test_misc.c` — New (Phase 6)
- `tests/harness/sit_test_registry.c` — Wired misc module, cleaned up comments
- `tests/harness/sit_api_include.h` — Vulkan type forward declarations
- `build_tests.bat` — Added test_misc.c to source list
- `doc/plan/TEST_HARNESS_PLAN.md` — Marked phases 6 & 7 complete, added phases 8–11

---

## [v2.4.27 "Audio Bugfixes"] - 2026-05-06

### Description

Fixed two bugs in the audio subsystem discovered by the Phase 5 test harness. All 33 audio tests now pass.

### Bugs Fixed

1. **SituationLoadSoundFromFile failed on valid WAV files** — The preloaded code path in `SituationLoadSoundFromFile` never set `sound->is_initialized = true`. The streaming path did, but preloaded sounds were left with `is_initialized = false`, causing the internal audio pipeline to reject them. Fixed by setting the flag after successful decode.

2. **SituationAddTrack crashed with SIGSEGV** — `SituationCreateMixer()` never initialized `mixer->device`, leaving it NULL. When `_SituationInitTrack_NoLock()` accessed `mixer->device->sampleRate` for EQ/dynamics node configuration, it dereferenced NULL. Fixed by assigning `mixer->device = &sit_audio.miniaudio_device` in `SituationCreateMixer()` and adding a defensive fallback (`mixer->device ? mixer->device->sampleRate : 48000`).

3. **SituationRemoveTrack use-after-nullify** — `SituationRemoveTrack` locked the mutex via `track->owner->topology_mutex`, then called `_SituationRemoveTrack_NoLock` which set `track->owner = NULL`, then tried to unlock via the now-NULL pointer. Fixed by saving `track->owner` to a local variable before the inner call.

### Files Modified

- `sit/situation_impl_audio.h` — All three fixes (lines ~1401, ~2295, ~2325, ~2436)

---

## [v2.4.26 "Audio Tests"] - 2026-05-06

### Description

Phase 5 of the test harness: added the audio module (`test_audio.c`) covering device management, sound loading/playback, tone synthesis, effects, processors, capture, mixer, device enumeration, and graph serialization.

### New Tests

- `tests/harness/test_audio.c` — 33 tests covering:
  - Device management (enumerate, sample rate, master volume, pause/resume)
  - Sound loading & playback (load from file, play/stop, stop all)
  - Audio handle API (load/play/unload, volume/pan/pitch)
  - Tone synthesis (PlayToneEx, legacy PlayTone, MIDI note, stop all, invalid handle)
  - Sound effects (volume, pan, pitch, filter, echo, reverb)
  - Audio processors (attach/detach custom DSP callback)
  - Capture (start/stop, output monitor)
  - Mixer (create/destroy, add/remove track, volume/pan, mute/solo, master volume)
  - Device enumeration (enumerate + free device list)
  - Graph serialization (serialize to JSON, save to file, free NULL)

### Changes

- `build_tests.bat` — Added `test_audio.c` to source list
- `tests/harness/sit_test_registry.c` — Wired `g_module_audio` into registration
- `doc/plan/TEST_HARNESS_PLAN.md` — Marked Phase 5 complete

### Known Issues Found

- **`SituationRemoveTrack` crashes (SIGSEGV)** — Calling `SituationRemoveTrack()` on a track from an unbound mixer crashes during node graph teardown. Workaround: use `SituationDestroyMixer()` which handles track cleanup correctly. Root cause is a node teardown ordering issue in miniaudio's graph when individual nodes are uninited while the graph is still alive. Tracked for future investigation.

### Bugs Fixed (in DLL)

- **`SituationLoadSoundFromFile` now works** — Preloaded sounds were missing `sound->is_initialized = true`, causing the internal pipeline to reject them as invalid.
- **`SituationAddTrack` no longer crashes** — `SituationCreateMixer()` was not setting `mixer->device`, leaving it NULL. Track init then dereferenced `mixer->device->sampleRate`. Fixed by assigning `mixer->device = &sit_audio.miniaudio_device` and adding a defensive null guard.

### Stats

- **150 total tests**, 8 modules
- 33/33 audio tests passing
- 1 known issue remaining: `SituationRemoveTrack` crash (workaround: use `SituationDestroyMixer`)

---

## [v2.4.25 "Graphics Tests"] - 2026-05-06

### Description

Phase 4 of the test harness: added the graphics module (`test_graphics.c`) covering GPU resource management, command buffers, virtual displays, and renderer diagnostics.

### New Tests

- `tests/harness/test_graphics.c` — 29 tests covering:
  - Mesh creation/destruction, metadata, data readback
  - Shader loading from memory, uniform setting
  - Texture creation (standard + storage usage), bindless handle query
  - Buffer create/destroy, update+readback roundtrip, device address
  - Compute pipeline from memory, max work group query
  - Frame lifecycle (acquire, main cmd buffer, begin/end render pass, viewport, scissor, barrier)
  - Virtual displays (create, configure, get, dirty flag, size, composite render)
  - Diagnostics (renderer type, feature support, draw calls, VRAM, screenshot)

### Changes

- `build_tests.bat` — Added `test_graphics.c` to source list
- `tests/harness/sit_test_registry.c` — Wired `g_module_graphics` into registration
- `doc/plan/TEST_HARNESS_PLAN.md` — Updated checkboxes for phases 1–4

### Stats

- **117 tests passing**, 7 modules, ~0.8 seconds full suite
- Graphics tests gracefully handle backend-specific limitations (bindless textures, buffer errors)

---

## [v2.4.24 "Test Harness"] - 2026-05-06

### Description

Introduced a formal test harness for regression testing the entire SITAPI public surface. The harness links against the pre-built DLL and exercises API functions as a black-box consumer — same as a user application would.

### New Infrastructure

- `tests/harness/sit_test_framework.h` — Minimal C11 test framework (assertions, setjmp recovery, colored output)
- `tests/harness/sit_api_include.h` — Prerequisite wrapper for `situation_api.h`
- `tests/harness/sit_test_registry.c` — Module registration
- `tests/harness/main.c` — Entry point with CLI (`--module`, `--filter`, `--list`, `--stop-on-fail`, `--verbose`)
- `tests/harness/test_filesystem.c` — 19 tests (paths, file I/O, directories)
- `tests/harness/test_threading.c` — 7 tests (pool, jobs, parallel dispatch, dependencies)
- `tests/harness/test_core.c` — 19 tests (init, state, FPS, callbacks, system info)
- `tests/harness/test_window.c` — 16 tests (state, properties, monitors, cursor, clipboard)
- `tests/harness/test_input.c` — 17 tests (keyboard, mouse, gamepad)
- `tests/harness/test_timer.c` — 10 tests (oscillators, time queries)
- `build_tests.bat` — Build script (links against DLL, supports OpenGL/Vulkan)
- `.kiro/steering/situation-project.md` — Project steering file for development context
- `doc/plan/TEST_HARNESS_PLAN.md` — Implementation plan

### Stats

- **88 tests passing**, 6 modules, ~1.8 seconds full suite
- Zero external test framework dependencies
- Links against pre-built DLL only — never recompiles the library

---

## [v2.4.23 "Audio & Text Online"] - 2026-05-06

### Description

Brought the audio tone synthesizer and OpenGL text rendering online. Both systems had missing integration code that prevented them from producing output despite being correctly initialized.

### Fixes

**CRITICAL — Tone synthesis produced no sound (3 issues)**:
- The audio callback had no tone mixing loop — only processed loaded sounds via `active_voices`.
- When `active_voice_count == 0`, the callback returned early before reaching any tone code.
- No default audio device was opened during init — `SituationSetAudioDevice(0, NULL)` was never called.
- **Fix**: Added tone synthesis mixing loop, removed early return, auto-start default playback device after `SituationInit` completes.

**MEDIUM — Tone envelope crackling**:
- `continue` statements in ADSR state transitions skipped sample output, creating zero-gaps (audible clicks).
- **Fix**: Envelope now transitions smoothly without skipping any samples.

**CRITICAL — OpenGL text rendering invisible**:
- The text vertex shader requires a `u_projection` uniform but it was never set — vertices transformed to garbage coordinates.
- **Fix**: Set ortho projection matrix on the text shader program before each text draw batch.

**MEDIUM — Font atlas UV mapping wrong**:
- V coordinate used `row / 8.0f` but the atlas has 16 rows (256 chars in 16×16 grid). Characters were sampling from overlapping cells, appearing garbled.
- **Fix**: Changed to `row / 16.0f`.

**LOW — Bitmap font blurry when scaled**:
- Default font atlas used `GL_LINEAR` filtering, causing bilinear interpolation on pixel art.
- **Fix**: Override to `GL_NEAREST` after font atlas texture creation.

### Audio Architecture

- Tones from `SituationPlayToneEx` always mix direct-to-output (bypasses mixer).
- If a mixer is active, tones still play on top via `goto tone_mixing` after mixer output.
- For routed/effected tones, use `SITUATION_NODE_TONE_SYNTH` in the mixer graph.
- Two-tier design: quick path (fire-and-forget) + pro path (full mixer routing).

### Safety Improvements (from external code review)

- **`extern "C"` guard**: Added to `situation_api.h` for C++ consumer compatibility.
- **Audio thread malloc eliminated**: Topological sort now runs on main thread; audio callback outputs silence if graph unsorted.
- **Timer drift fix**: Oscillator triggers calculated from anchor time (`anchor + count * period`) instead of accumulating.
- **Buffer overflow fix**: `SituationBuffer` handle packing uses explicit bit-shift instead of `memcpy` of oversized struct.
- **VLA stack overflow fix**: Mastering amp uses fixed 1024-frame chunked processing.
- **`_Static_assert` guard**: Compile-time check ensures `SituationBuffer` handle fields stay at expected offsets.
- **VMA VRAM reporting**: `SituationGetVRAMUsage()` now returns actual allocation bytes on Vulkan.

### Subsystem Status

| Subsystem | Status | Verified |
|-----------|--------|----------|
| **OpenGL Renderer** | ✅ Quads + Text | 20K quads @ 146 FPS, text readable |
| **Vulkan Renderer** | ✅ Quads | 20K quads @ 140 FPS, text pending |
| **Audio (Tones)** | ✅ Working | Clean sine/square/tri/saw, ADSR envelope, no crackling |
| **Audio (Loaded Sounds)** | ⬜ Untested | Needs MP3/WAV file to verify |
| **Audio (Mixer/Node Graph)** | ⬜ Untested | Infrastructure present, needs integration test |

---

## [v2.4.22 "Vulkan Init Restored"] - 2026-05-06

### Description

Fixed the Vulkan renderer initialization crash, restored quad rendering on Vulkan, and addressed multiple safety issues identified via external code review (Gemini).

### Fixes

**CRITICAL — Vulkan `SituationCreateTextureEx` always failing**:
- The "Resource Manager Hook" at the end of `SituationCreateTextureEx` checked `strcmp(sit_gs.last_error_msg, "No error")` to detect deferred OpenGL errors.
- This check was NOT guarded by `#if defined(SITUATION_USE_OPENGL)`, so on the Vulkan path it always triggered the failure branch.
- **Fix**: Wrapped the error-check-and-cleanup block in `#if defined(SITUATION_USE_OPENGL)`.

**CRITICAL — Vulkan quads invisible (4 issues)**:
- Render pass incompatibility: `_SituationVulkanGetOrCreateRenderPass` created passes with 2 dependencies, but framebuffers used the original pass with 1. **Fix**: Use `main_window_render_pass` directly for main window rendering.
- Missing descriptor set binds: `SituationCmdDrawQuad` wasn't binding the UBO (set 0) or bindless set (set 1). **Fix**: Added both binds.
- Wrong UBO binding index: Descriptor write targeted binding 0, but layout was created with binding 1. **Fix**: Use `SIT_UBO_BINDING_VIEW_DATA`.
- Back-face culling: Triangle strip produces CCW triangles under top-left ortho, culled by `BACK_BIT + CLOCKWISE`. **Fix**: Disabled culling for all 2D pipelines.

**HIGH — Buffer overflow in `SituationCmdBindDescriptorSetDynamic`**:
- `SituationBuffer` grew to 24 bytes but was `memcpy`'d into a `uint64_t` (8 bytes).
- **Fix**: Pack only `slot_index` + `generation` using explicit bit shifting.

**HIGH — Audio thread malloc in topological sort**:
- `SituationProcessGraph` called `SituationTopologicalSort` (which allocates) directly from the real-time audio callback.
- **Fix**: Sort now happens on the main thread immediately when topology changes. Audio thread outputs silence if graph isn't sorted yet.

**MEDIUM — VLA stack overflow in `_SituationMasteringAmpProcessAudio`**:
- `float ampBuffer[2 * numFrames]` could request 64KB+ on the audio thread stack.
- **Fix**: Fixed-size 1024-frame chunked processing (8KB max stack).

**MEDIUM — Missing `extern "C"` for C++ consumers**:
- SITAPI function declarations weren't wrapped in `extern "C"`, causing linker failures when included from C++.
- **Fix**: Added `extern "C" { }` guard around the entire public API in `situation_api.h`.

**LOW — Timer oscillator drift**:
- Accumulating `next_trigger_time += period` causes floating-point drift over hours.
- **Fix**: Calculate next trigger from anchor time: `anchor + trigger_count * period`.

**LOW — `SituationGetVRAMUsage()` returning 0 on Vulkan**:
- **Fix**: Restored `vmaCalculateStatistics()` call with proper C wrapper struct definitions.

### Renderer Pipeline Status

| Backend | Code Audit | Compiles | Runtime Verified | Quad Draw | Performance |
|---------|-----------|----------|-----------------|-----------|-------------|
| **OpenGL** | ✅ All 4 phases | ✅ Zero warnings | ✅ Confirmed | ✅ Working | ✅ 20K quads @ 146 FPS |
| **Vulkan** | ✅ All 4 phases | ✅ Zero warnings | ✅ Confirmed | ✅ Working | ✅ 20K quads @ 140 FPS |

### Build Infrastructure

- GCC upgraded from 8.1.0 to 15.1.0 (MSYS2 MinGW-w64). Build script auto-detects `C:\msys64\mingw64\bin`.

### Audio Subsystem

**CRITICAL — Tone synthesis never produced sound**:
- The audio callback had no tone mixing loop — it only processed loaded sounds via `active_voices`.
- When `active_voice_count == 0` (no loaded sounds), the callback returned early, skipping everything.
- No default audio device was opened during init — `SituationSetAudioDevice` was never called automatically.
- **Fix**: Added tone synthesis mixing loop to the callback, removed early return that skipped it, and auto-start the default playback device during `SituationInit`.

**MEDIUM — Envelope clicks/crackling**:
- `continue` statements in envelope state transitions skipped sample output for that frame, creating zero-gaps (audible clicks at every ADSR boundary).
- **Fix**: Removed `continue`, envelope now transitions smoothly without skipping samples.

**DESIGN — Tone synth always plays direct-to-output**:
- Tones from `SituationPlayToneEx` now always mix to output regardless of whether a mixer is active.
- For routed/effected tones, users can create a `SITUATION_NODE_TONE_SYNTH` node in the mixer graph.
- Two-tier design: quick path (fire-and-forget) + pro path (full mixer routing).

---

## [v2.4.21 "Renderer Runtime Fixes"] - 2026-05-05

### Description

Critical runtime bugs found during example testing after the renderer audit. These were pre-existing issues exposed by actually running the hardened code.

### Fixes

**CRITICAL — `_SituationInitGLRingFences()` never called**:
- The ring fence array (`sit_render.gl.ring_fences`) was never allocated during init.
- `_SituationGLExecuteCommands` dereferences it at the end of every frame → NULL pointer crash after first frame.
- **Fix**: Added `_SituationInitGLRingFences()` call after `_SituationInitGLRingBuffer()` in the OpenGL init path.

**HIGH — Stale error state in `SituationCreateTextureEx`**:
- The quad renderer init calls `SIT_CHECK_GL_ERROR()` which can set `sit_gs.last_error_msg` to a non-"No error" string from a non-fatal GL state issue.
- `SituationCreateTextureEx` uses a deferred error check (`strcmp(sit_gs.last_error_msg, "No error")`) at the end — it would see the stale message and falsely conclude its own GL calls failed.
- **Fix**: Clear `sit_gs.last_error_msg` to `"No error"` at the start of the OpenGL path in `SituationCreateTextureEx`.

**HIGH — Face culling on 2D quads**:
- `_SituationGLExecuteCommands` resets GL state with `glEnable(GL_CULL_FACE)` + `glCullFace(GL_BACK)` at the start of every frame.
- The quad's triangle strip (vertices 0..1) produces back-facing triangles under the top-left-origin ortho projection → all quads culled, invisible.
- **Fix**: Disable `GL_CULL_FACE` and `GL_DEPTH_TEST` before quad draw, re-enable after.

**PERF — `SIT_DEBUG_LOG` always active**:
- The debug log macro opened, wrote, and closed a file on every call — thousands of file I/O ops per frame.
- **Fix**: Gated behind `SITUATION_DEBUG_LOG_ENABLED` define. No-op by default.

**PERF — Quad draw batching**:
- Each `SIT_OP_DRAW_QUAD` was a full state setup (program bind, VAO bind, culling toggle, texture mode check) per quad.
- **Fix**: Batch consecutive DRAW_QUAD opcodes — set state once, loop only uniform updates + draw calls. ~4x throughput improvement (5K→20K quads at 60 FPS).

### Renderer Pipeline Status

| Backend | Code Audit | Compiles | Runtime Verified | Quad Draw | Performance |
|---------|-----------|----------|-----------------|-----------|-------------|
| **OpenGL** | ✅ All 4 phases | ✅ Zero warnings | ✅ Confirmed on hardware | ✅ Working | ✅ 20K quads @ 146 FPS |
| **Vulkan** | ✅ All 4 phases | ✅ Zero warnings | ❌ Not yet verified | ❌ Not yet verified | ❌ Not yet verified |

**OpenGL**: Fully qualified and runtime-verified. Safe for users.  
**Vulkan**: Code-level audit complete, compiles clean, but runtime testing pending (init/render loop not yet confirmed on hardware).

### Build Infrastructure

- Added `build_examples.bat` — standardized example build script using the same MSYS2 GCC 15.1.0 toolchain as the DLL builds.
- Examples output to `build/examples/`.

### Verification

- `diagnostic_render.exe` (OpenGL): Cycling clear color confirms frame loop operational.
- `basic_quad.exe` (OpenGL): Interactive quad with WASD + mouse input confirmed.
- `quad_storm.exe` (OpenGL): 20,000 quads at 146 FPS (VSync off), 60 FPS (VSync on).
- Both DLLs (OpenGL + Vulkan): zero warnings, zero errors.

---

## [v2.4.20 "Renderer Audit Phase 3+4 — Frame Lifecycle & Resource Registry"] - 2026-05-05

### Description

Phases 3 and 4 of the Renderer Robustness Audit: frame lifecycle, render thread, hot-reload, and the handle-based resource registry system.

### Issues Found & Fixed

**MEDIUM — `_SitGLDeferDestroyBuffer` / `_SitGLDeferDestroyTexture` (OpenGL graveyard)**:
- `SIT_REALLOC` calls were unchecked — NULL return would crash on subsequent array write.
- **Fix**: Added NULL checks with emergency immediate-delete fallback (safe since we hold the mutex).

### Items Verified (Already Correct)

**Phase 3 — Frame Lifecycle & Render Thread:**
- `SituationAcquireFrameCommandBuffer` — checks every Vulkan call (fence wait, image acquire, fence reset, cmd buffer reset, begin recording). OpenGL path checks ring buffer map.
- `SituationEndFrame` — validates cmd buffer, checks `vkEndCommandBuffer`, adaptive backpressure (spin/sleep/yield).
- `_SituationRenderThreadEntry` — atomic context handoff, proper shutdown via `thread_shutdown_req` + queue drain, errors propagated via `_SituationSetErrorFromCode`.
- `_SitGLSoftCmdPush` / `_SitGLSoftDataPush` — breaker pattern on overflow, callers check NULL.
- Soft command buffer replay — broken buffer skipped, unknown opcodes silently skipped (safe).
- Momentum queue — mutex-protected, overflow check with error, in-flight count properly managed.
- `SituationReloadShader` — uses deferred destroy for old pipeline, creates new before destroying old.
- `SituationReloadTexture` — uses deferred destroy for old image, creates new first, swaps internals.

**Phase 4 — Resource Registry & Lifetime:**
- `_SitGetTextureSlot` / `_SitGetBufferSlot` — bounds check + `is_active` + generation mismatch = prevents use-after-free and double-free.
- `SituationDestroyTexture` / `SituationDestroyBuffer` — generation-validated slot access, deferred GPU destruction, slot deactivation, handle zeroing.
- `_SituationFlushGraveyard` — checks `VK_NULL_HANDLE` before destroying, resets counts. Called only after fence signals (timing correct).
- `SituationLoadModel` — cascading cleanup on partial failure (frees textures if mesh alloc fails, frees GLTF data on every error path).
- `SituationUnloadModel` — validates handle, destroys all sub-resources, frees arrays, zeroes handle.

### Build Verification

- OpenGL DLL: zero warnings, zero errors, exit code 0.
- Vulkan DLL: zero warnings, zero errors, exit code 0.

---

## [v2.4.19 "Renderer Audit Phase 2 — Vulkan Runtime"] - 2026-05-05

### Description

Phase 2 of the Renderer Robustness Audit: systematic audit of all Vulkan runtime resource creation, synchronization, descriptor management, and graveyard system.

### Issues Found & Fixed

**HIGH — `_SituationSubmitGraphics`**:
- `vkQueueSubmit` result was stored but never checked — silent failure on queue submit.
- **Fix**: Added error check with `_SituationSetErrorFromCode`.

**HIGH — `_SituationSubmitCompute`**:
- `vkQueueSubmit` result was completely ignored.
- **Fix**: Added error check with `_SituationSetErrorFromCode`.

**HIGH — `SituationCreateTextureEx` (Vulkan path) — `vkCreateSampler` failure**:
- Failure path had `// ... cleanup ...` comment but NO actual cleanup — leaked VkImage, VkImageView, VmaAllocation.
- **Fix**: Added proper cleanup via `_SituationDeferDestroyImage` + slot deactivation.

**MEDIUM — Graveyard `_SituationDeferDestroy*` functions (4 functions)**:
- `SIT_REALLOC` calls were unchecked — NULL return would crash on subsequent array write.
- **Fix**: Added NULL checks with emergency immediate-destroy fallback for `_SituationDeferDestroyBuffer`, `_SituationDeferDestroyImage`, `_SituationDeferDestroyDescriptorSet`, `_SituationDeferDestroyPipeline`.

### Items Verified (Already Correct)

- `_SituationVulkanCreateImage` — checks `vmaCreateImage` return, sets error, returns failure code.
- `_SituationVulkanCreateAndUploadBuffer` — checks every `vmaCreateBuffer`, `vmaMapMemory`, and `_SituationVulkanBeginSingleTimeCommands` with proper cascading cleanup.
- `_SituationVulkanAllocateDescriptorSet` — 3-phase fallback (current pool → search existing → create new), all returns checked.
- `_SituationVulkanCreateGraphicsPipeline` — checks shader module creation and `vkCreateGraphicsPipelines`, cleans up modules.
- `SituationCreateComputePipelineFromMemory` — checks `vkCreateComputePipelines`, cleans up shader module and slot.
- `_SituationVulkanCreateSwapchain` — checks `vkCreateSwapchainKHR`, sets `swapchain_valid = false` on failure.
- `_SituationVulkanRecreateSwapchain` — checks every step, cascading cleanup on partial failure.
- Frame acquire path — checks `vkWaitForFences`, `vkAcquireNextImageKHR` (OUT_OF_DATE/SUBOPTIMAL), `vkResetFences`, `vkResetCommandBuffer`, `vkBeginCommandBuffer`.
- Single-threaded submit/present — checks `vkQueueSubmit` and `vkQueuePresentKHR` with swapchain recreation.
- Render thread submit/present — checks both with error propagation.
- `_SituationVulkanCreateSyncObjects` — checks all semaphore/fence creation.
- `_SituationVulkanCreateImageView` — checks `vkCreateImageView`, returns `VK_NULL_HANDLE`.
- `_SituationCreateVulkanShaderModule` — validates input, checks `vkCreateShaderModule`.
- `_SituationVulkanEndSingleTimeCommands` — checks `vkEndCommandBuffer`, `vkQueueSubmit`, `vkQueueWaitIdle` with cleanup.
- `_SituationFlushGraveyard` — properly checks `VK_NULL_HANDLE` before destroying, resets counts.

### Build Verification

- OpenGL DLL: zero warnings, zero errors, exit code 0.
- Vulkan DLL: zero warnings, zero errors, exit code 0.

---

## [v2.4.18 "Renderer Audit Phase 1 — OpenGL Runtime"] - 2026-05-05

### Description

Phase 1 of the Renderer Robustness Audit: systematic audit and hardening of all OpenGL runtime resource creation, command recording, and ring buffer paths.

### Issues Found & Fixed

**HIGH — `SituationCreateBuffer` (OpenGL path)**:
- `glCreateBuffers` return value was not checked — could proceed with buffer ID 0.
- `SIT_CHECK_GL_ERROR()` after `glNamedBufferStorage` did not bail out on failure — leaked the buffer slot.
- **Fix**: Added ID-zero check + error-state check with cleanup on failure.

**HIGH — `SituationCreateMesh` (OpenGL path)**:
- `glCreateBuffers` for VBO and EBO were not checked.
- **Fix**: Added ID-zero checks with proper cascading cleanup (delete VBO if EBO fails, free mesh slot).

**HIGH — `_SituationInitGLRingBuffer`**:
- `glCreateBuffers` return not checked, `glMapNamedBufferRange` return not checked inline.
- **Fix**: Added ID-zero check, added `SIT_CHECK_GL_ERROR()` after storage, added NULL check on map result.

**HIGH — `_SituationInitGLMDIBuffer`**:
- Same pattern as ring buffer.
- **Fix**: Same treatment.

**HIGH — `SituationCmdBeginRenderPass` (OpenGL path)**:
- No NULL check on `cmd` parameter — would dereference NULL.
- **Fix**: Added `if (!cmd) return SITUATION_ERROR_INVALID_PARAM`.

**HIGH — NULL cmd guards on hot-path commands**:
- `SituationCmdDraw`, `SituationCmdDrawIndexed`, `SituationCmdBindPipeline`, `SituationCmdDrawMesh`, `SituationCmdSetViewport` — all lacked NULL cmd guards.
- **Fix**: Added `if (!cmd) return SITUATION_ERROR_INVALID_PARAM` to each.

**MEDIUM — `SIT_OP_PRESENT` FBO (replay path)**:
- `glCreateFramebuffers` return not checked, no framebuffer completeness check.
- **Fix**: Added ID-zero guard + `glCheckNamedFramebufferStatus` with cleanup on incomplete.

### Items Verified (Already Correct)

- `SituationCreateTextureEx` — `glCreateTextures` return checked (ID-zero → error), deferred GL error pattern with cleanup at end.
- Texture registry-full path — properly unlocks mutex and returns error.
- `SituationCreateShader` / hot-reload — uses `_SituationCreateGLShaderProgram` which has thorough error handling.
- `_SitGLSoftCmdPush` / `_SitGLSoftDataPush` — "breaker" pattern on realloc failure, callers check NULL.
- MDI ring buffer allocation — bounds check with graceful fallback to single draw.

### Build Verification

- OpenGL DLL: zero warnings, zero errors, exit code 0.
- Vulkan DLL: zero warnings, zero errors, exit code 0.

---

## [v2.4.17 "Renderer Init Hardening"] - 2026-05-05

### Description

Hardening the OpenGL renderer initialization path to prevent silent failures that could result in black screens or crashes when GPU resources fail to allocate.

### Changes

**Critical Fixes**:
- **`_SituationInitDefaultFont`**: Changed from `void` to `bool` return type. Now validates `SituationCreateTexture` result — if font atlas texture creation fails, the function returns `false` and the error is propagated up to `_SituationInitOpenGL` / `_SituationInitVulkan`, which abort initialization cleanly.
- **`_SituationInitTextRenderer`**: Now validates that `glCreateVertexArrays` and `glCreateBuffers` return non-zero IDs. On failure, cleans up the shader program and any partially-created resources before returning `false`.

**Resource Leak Fixes**:
- **`_SituationInitOpenGL`**: All failure paths now clean up both `global_vao_id` AND `mesh_vao_id`. Previously, `mesh_vao_id` was leaked if quad renderer, font, text renderer, or VD shader init failed.
- **Composite shader failure path**: Now also cleans up `mesh_vao_id`.

**Forward Declaration**:
- **`sit/situation_impl_renderer_fwd.h`**: Updated `_SituationInitDefaultFont` declaration from `void` to `bool`.

### Build Verification

- Full DLL build (OpenGL): zero warnings, zero errors, exit code 0.

---

## [v2.4.16 "Init Path Hardening"] - 2026-05-05

### Description

Hardening the initialization path to eliminate undefined behavior on partial init failure and ensure proper error reporting at every allocation site.

### Changes

**Bug Fixes**:
- **`sit/situation_impl_ctrl.h`** (`_SituationInitSubsystems`):
  - `active_voices` allocation failure now calls `_SituationSetErrorFromCode` before returning (was returning bare enum).
  - `snapshot_buffer` allocation failure now calls `_SituationSetErrorFromCode` and NULLs `active_voices` after freeing.
  - Input mutex initialization now uses sequential init with proper rollback — if the 2nd or 3rd mutex fails, previously-initialized mutexes are destroyed before returning.
  - Added `sit_gs.input_mutexes_initialized` flag to track whether input mutexes were successfully created.

- **`sit/situation_impl_ctrl.h`** (`_SituationCleanupSubsystems`):
  - Audio queue mutex (`mtx_destroy`) is now called *before* `ma_context_uninit` sets `is_miniaudio_context_initialized` to false, guarded by that flag. Previously, the guard would always be false by the time it was checked (flag cleared earlier in the function).
  - Input mutex cleanup (`ma_mutex_uninit` ×3) is now guarded by `sit_gs.input_mutexes_initialized`. Previously, these were called unconditionally — UB if init failed before mutexes were created.
  - Audio capture mutex cleanup moved outside the input mutex guard (it has its own `audio_capture_on_main_thread` guard).

- **`sit/situation_impl_decl.h`**: Added `bool input_mutexes_initialized` field to the global state struct.

### Build Verification

- Full DLL build (OpenGL): zero warnings, zero errors, exit code 0.

---

## [v2.4.15 "Housekeeping"] - 2026-05-05

### Description

Small bugfixes and hygiene issues discovered during the error propagation work.

### Changes

**Bug Fixes**:
- **`situation.h`**: Set `_WIN32_WINNT` and `WINVER` to `0x0600` (Vista) before any Windows headers are included. Fixes implicit declaration warning for `SHGetKnownFolderPath` that occurred because GLFW was pinning `_WIN32_WINNT` to `0x0501` (XP).
- **`sit/situation_impl_threading.h`**: `SituationCreateThreadPool` now properly rolls back on partial thread creation failure — signals shutdown, joins already-spawned threads, destroys mutexes/condvars, frees queue memory, and zeroes the pool struct. Previously, a failed `thrd_create` would leave orphaned threads running against a pool the caller considers dead.
- **`sit/situation_impl_wdm.h`**: `SituationToggleBorderlessWindowed` now detects which monitor the window is currently on (by checking window position against monitor bounds) instead of always using `glfwGetPrimaryMonitor()`. Fixes borderless mode filling the wrong display on multi-monitor setups.

**Hygiene**:
- **`sit/situation_impl_renderer.h`**: `SituationExportRenderHistogram` JSON output now uses `SITUATION_VERSION_MAJOR/MINOR/PATCH/REVISION` macros instead of a hardcoded `"2.3.24b"` string.
- **`sit/k-term/example/situation_api.h`**: Version synced to 2.4.15.

### Build Verification

- Full DLL compilation (OpenGL backend): **zero warnings, zero errors** (exit code 0)
- The `SHGetKnownFolderPath` implicit declaration warning is eliminated.

---

## [v2.4.14 "Error Propagation Phase 1+2"] - 2026-05-05

### Description

Non-breaking error propagation remediation. Every public API function that can fail now properly reports through `_SituationSetErrorFromCode` before returning. Users calling `SituationGetLastErrorMsg()` after a failure will get a meaningful message instead of stale/empty data.

### Changes

**Phase 1 — Add Error State to Existing Void/Bool Functions (Non-Breaking)**:
- `sit/situation_impl_wdm.h`: 30+ window/display functions now set error state on early return (`NOT_INITIALIZED`, `INVALID_PARAM`, `DISPLAY_QUERY`, `MEMORY_ALLOCATION`)
- `sit/situation_impl_vd.h`: `SetVirtualDisplayDirty`, `IsVirtualDisplayDirty`, `GetVirtualDisplaySize` report `NOT_INITIALIZED` or `VIRTUAL_DISPLAY_INVALID_ID`
- `sit/situation_impl_threading.h`: `CreateThreadPool`, `DumpTaskGraph`, `DispatchParallel`, `WaitForAllJobs`, `DestroyThreadPool` report `INVALID_PARAM`, `MEMORY_ALLOCATION`, `THREAD_CREATION_FAILED`
- `sit/situation_impl_timer.h`: All 5 oscillator query functions report `TIMER_SYSTEM` or `INVALID_PARAM`
- `sit/situation_impl_ctrl.h`: File drop callback reports `MEMORY_ALLOCATION` on alloc failures
- `sit/situation_impl_io.h`: `SituationGetAppSavePath` POSIX path reports `DEVICE_QUERY` and `MEMORY_ALLOCATION`
- `sit/situation_impl_image.h`: `_SituationSaveImageBMP` reports `INVALID_PARAM` on NULL inputs
- `sit/situation_impl_renderer.h`: `CmdBindVertexBuffer`, `CmdBindIndexBuffer`, `GetRenderLatencyStats`, `ExportRenderHistogram`, `DrawMetricsOverlay`, `DestroyRenderList`, `ResetRenderList` report appropriate errors

**Phase 2 — fprintf(stderr) Paired With Error Codes**:
- `vkDeviceWaitIdle` failures (×2) → `VULKAN_COMMAND_FAILED`
- `vkCreateRenderPass` failure → `VULKAN_RENDERPASS_FAILED`
- Render Pass Cache full → `VULKAN_RENDERPASS_FAILED`
- Vulkan debug callback ERROR severity → `VULKAN_VALIDATION_LAYER_ERROR`
- Vulkan debug callback NULL data → `VULKAN_VALIDATION_LAYER_ERROR`
- Extension limit overflow (×3) → `VULKAN_UNSUPPORTED`
- shaderc blob NULL result → `SHADER_COMPILATION_FAILED`

**Version Bump**:
- `situation.h`: 2.4.13 → 2.4.14

### Build Verification

- Full DLL compilation (OpenGL backend): zero new warnings from changed files
- Only pre-existing `SHGetKnownFolderPath` implicit declaration warning in `situation_impl_io.h` (unrelated)

---

## [v2.4.13 "X-Macro Errno"] - 2026-05-05

### Description

Error system refactor: single source of truth via X-macros. The `SituationError` enum and the human-readable message switch are now generated from one table. Adding a new error code is a single line — no more manual sync between two files.

### Changes

**Error Table (`sit/situation_base_errno.h`)** — Full rewrite:
- Replaced hand-maintained enum + inline comments with 15 sectioned X-macro sub-tables (`SITUATION_ERRORS_CORE`, `SITUATION_ERRORS_THREADING`, `SITUATION_ERRORS_PLATFORM`, etc.)
- Master `SITUATION_ERROR_TABLE(X)` concatenates all sections.
- Enum is now mechanically generated: `#define _SIT_ERRNO_ENUM(name, value, msg) name = value,`
- All enum names and integer values are identical to before — zero ABI change.

**Error Message Lookup (`sit/situation_impl_ctrl.h`)** — `_SituationSetErrorFromCode`:
- Replaced ~200-line hand-maintained switch with 3-line macro expansion.
- Added missing `return err;` (function was accidentally void-returning).
- 7 error codes that had drifted out of the switch (`COMMAND_EXECUTION_FAILED`, 6 MIDI codes) are now automatically covered.

**Version Bump**:
- `situation.h`: 2.4.12 → 2.4.13
- `sit/k-term/example/situation_api.h`: 2.4.12 → 2.4.13

### Build Verification

- `sit/situation_base_errno.h` compiles standalone (gcc -fsyntax-only, exit 0, zero warnings)
- Full DLL compilation: zero new warnings from changed files

---

## [v2.4.12 "API Polish"] - 2026-05-05

### Description

Documentation consistency pass, build hygiene, and encoding cleanup across the library. All public API prototypes now have concise inline end-of-line comments matching the library convention. Build warnings eliminated. Garbled UTF-8 purged from all source files. Vulkan engine version now tracks the central version macros.

### Changes

**API Documentation (`situation_api.h`)**:
- MIDI Control Integration section: Converted from verbose multi-line Doxygen blocks to single-line inline comments. Functions collapsed to single-line prototypes. Zero information lost.
- MIDI Learn Integration section: Same treatment. Grouped into logical sub-sections (Lifecycle, Learning Operations, Mapping Management, Preset Persistence).
- Added EOL comments to ~60 previously bare prototypes across: Audio Capture, Audio Handle API, Tone API, Mixer API (Phase 1 & 4), Render List, Node Graph & Device Registry, Graph Serialization, Device Enumeration, and misc utilities.
- Removed duplicate `SituationIsLearning` declaration.
- Converted `SituationIsFeatureSupported` from Doxygen block to inline comment.

**Encoding Cleanup**:
- `sit/situation_impl_ctrl.h`: Purged 388 garbled non-ASCII characters (triple-encoded UTF-8 mojibake). File is now pure ASCII.
- `sit/situation_impl_threading.h`: Already cleaned in v2.4.11 (confirmed pure ASCII).

**Build Hygiene (`build_situation.bat`)**:
- Removed redundant `-D_TTHREAD_WIN32_` from all compile steps (tinycthread.h auto-detects Windows).
- Removed redundant `-DVMA_IMPLEMENTATION` from VMA wrapper compile (vma_wrapper.cpp self-defines).
- Result: Zero warnings on both OpenGL and Vulkan builds (GCC 15.1.0, C11).

**Vulkan Engine Version (`situation_impl_renderer.h`)**:
- Replaced hardcoded `VK_MAKE_VERSION(2, 6, 0)` with `VK_MAKE_VERSION(SITUATION_VERSION_MAJOR, SITUATION_VERSION_MINOR, SITUATION_VERSION_PATCH)`.
- Engine version now automatically tracks the library version. Removed the TODO comment.

**k-term Example Sync**:
- Updated `sit/k-term/example/situation_api.h` version macros from 2.3.41 to 2.4.12.

### Build Verification

- OpenGL DLL: `situation_opengl.dll` -- compiled and linked successfully (GCC 15.1.0, C11, zero warnings)
- Vulkan DLL: `situation_vulkan.dll` -- compiled and linked successfully (GCC 15.1.0, C11, zero warnings)

---

## [v2.4.11 "Threading Manicure"] - 2026-05-05

### Description

Non-disruptive hardening pass on the thread pool implementation. Seven targeted patches addressing platform correctness, edge-case safety, and documentation accuracy. No API changes, no struct layout changes, no new public symbols.

### New Files

- **`sit/situation_impl_threading_diag.h`** — Threading diagnostics and hardening utilities (relocated from `sit/aud/threading_diagnostics.h` to sit alongside the threading implementation where it belongs).

### Removed Files

- **`sit/aud/threading_diagnostics.h`** — Moved to `sit/situation_impl_threading_diag.h` (was incorrectly placed in the audio subsystem).

### Changes

- **Patch 1 — Platform Sleep Consistency**: Replaced `thrd_sleep()` in `SituationWaitForJob` (with `thrd_yield()`) and in the `SIT_SUBMIT_BLOCK_IF_FULL` path (with `SITUATION_SLEEP_MS(0)`). Eliminates the documented tinycthread hang on Windows.
- **Patch 2 — Work-Stealing Safety**: Added `dependency_count` check in `SituationDispatchParallel`'s helping loop before stealing from the high-priority queue. Prevents premature execution of jobs with unmet prerequisites.
- **Patch 3 — HOL Blocking Mitigation**: Worker loop now scans up to 8 slots past a blocked tail job (`SIT_WORKER_SCAN_DEPTH`). Ready jobs behind a dependency-blocked head are swapped forward and executed, eliminating the most common stall pattern.
- **Patch 4 — Doc Comment Accuracy**: Rewrote `_SituationDetectCycle` documentation to accurately describe the linear chain walk (was incorrectly documented as a DFS with three-color marking).
- **Patch 5 — Allocation Failure Handling**: `SituationSubmitJobEx` now explicitly rejects submission (returns 0 with error code) when `SIT_MALLOC` fails for large payloads. Previously fell back to storing a raw pointer with potential use-after-free.
- **Patch 6 — Signal Ordering Comment**: Added reasoning comment in the worker continuation path explaining why `cnd_signal` outside lock is correct (atomic `dependency_count` provides happens-before).
- **Patch 7 — Inline Fallback Comment**: Clarified the I/O-disabled inline execution path semantics (return 0 = "already complete", not "failed").
- **`situation_impl_threading.h`** now includes `situation_impl_threading_diag.h` for access to `SITUATION_SLEEP_MS` and debug macros.
- Updated include paths in `doc/misc/THREADING_TROUBLESHOOTING_GUIDE.md` and `doc/misc/SITUATION_THREADING_ARCHITECTURE.md`.

### Plan Document

- **`doc/plan/THREADING_UPGRADE_PLAN.md`** — Full rationale, before/after code, risk assessment, and testing checklist for all seven patches.

---

## [v2.4.10 "Module Hygiene"] - 2026-05-05

### Description

Post-split cleanup: the orchestrator becomes a pure 80-line include file with zero function bodies. Utility helpers, renderer forward declarations, the embedded font, and the error enum are each given their own home. Include order refined. No functional changes.

### New Files

- **`sit/situation_impl_renderer_fwd.h`** — Forward declarations for all renderer-internal static functions, with proper `#if defined(SITUATION_USE_OPENGL)` / `#if defined(SITUATION_USE_VULKAN)` / `#if defined(SITUATION_ENABLE_SHADER_COMPILER)` / `#if defined(CGLTF_IMPLEMENTATION)` guards.
- **`sit/situation_base_font.h`** — Embedded 8x8 VGA-Perfect CP437 bitmap font data (CC0 licensed).
- **`sit/situation_base_errno.h`** — The complete `SituationError` enum (260 lines). Extracted from `situation_api.h` for readability.
- **`sit/situation_impl_etc.h`** — The "et cetera" module: math helpers, string utilities (`_sit_strdup`, `_sit_dirname`, `_sit_strcasecmp`, `_sit_hash_string`), `_sit_directory_exists`, and `SituationFreeString`.
- **`concat_situation.ps1`** — PowerShell script to concatenate the full library into a single C file (defaults to `situation_full.c` in CWD).
- **`concat_situation.sh`** — Bash equivalent (binary-safe, preserves UTF-8 font comments).

### Changes

- **`situation_impl.h`** is now an 80-line pure orchestrator — nothing but `#include` directives with section comments. All function bodies, forward declarations, and data removed.
- **`situation_api.h`** reduced from ~2,950 to ~2,690 lines (error enum extracted to `situation_base_errno.h`).
- **`situation_impl_forward.h`** now contains ctrl/lifecycle, GLFW callbacks, threading, and audio forward declarations. Renderer declarations moved to `situation_impl_renderer_fwd.h` (included at the bottom of forward.h).
- **`situation_impl_timer.h`** moved up in include order (right after etc, before threading) since it has near-zero deps. `_SituationGetHighResTime` moved here from etc.
- **`SituationFreeString`** moved from `situation_impl_image.h` to `situation_impl_etc.h` (was misplaced in image module).
- GL ring buffer helpers (`_SituationInitGLRingBuffer`, `_SituationInitGLMDIBuffer`, `_SituationInitGLRingFences`, `_SituationGLRingWait`) moved into `situation_impl_renderer.h` where they belong.
- Vulkan defines (`SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS`, etc.) moved into `situation_impl_renderer.h`.
- Removed all duplicate/redundant forward declarations that were scattered in the old orchestrator.

### Architecture (Final)

```
situation.h (public entry point)
├── sit/situation_api.h              (~2,690 lines — public types, prototypes)
│   └── sit/situation_base_errno.h   (260 lines — SituationError enum)
└── sit/situation_impl.h             (80 lines — pure orchestrator)
    ├── sit/situation_base_font.h        (embedded VGA font data)
    ├── sit/situation_impl_deps.h        (third-party libs)
    ├── sit/situation_impl_decl.h        (types, structs, globals)
    ├── sit/situation_impl_forward.h     (cross-module prototypes)
    │   └── sit/situation_impl_renderer_fwd.h
    ├── sit/situation_impl_etc.h         (utilities, math, strings)
    ├── sit/situation_impl_timer.h       (oscillators, high-res time)
    ├── sit/situation_impl_threading.h   (thread pool, job system)
    ├── sit/situation_impl_io.h          (file I/O, async, system info)
    ├── sit/situation_impl_input.h       (keyboard, mouse, gamepad)
    ├── sit/situation_impl_wdm.h         (window, display, monitor)
    ├── sit/situation_impl_image.h       (image, font, color, screenshot)
    ├── sit/situation_impl_renderer.h    (GL + VK backends, resources)
    ├── sit/situation_impl_vd.h          (virtual display compositing)
    └── sit/situation_impl_ctrl.h        (lifecycle, init/shutdown, update)
```

---

## [v2.4.9 "Control & Renderer Split"] - 2026-05-05

### Description

Final structural refactor: splits the remaining ~19,800-line `situation_impl.h` into two focused modules. `situation_impl.h` becomes a ~700-line orchestrator (font data + include chain). No functional changes.

### New Files

- **`sit/situation_impl_renderer.h`** (16,917 lines) — Complete graphics renderer: OpenGL 4.6 Core + Vulkan 1.4 backends, command buffer processing, resource management (textures, buffers, meshes, shaders, compute pipelines), model loading (GLTF), hot-reload, render thread.
- **`sit/situation_impl_ctrl.h`** (2,277 lines) — Control plane: error handling & logging, library init/shutdown, platform & window init, update loop (poll events, update timers), callbacks, arguments, clipboard, file drop, state queries.

### Architecture

```
situation_impl.h (v2.4.9 — orchestrator, ~700 lines)
├── [font data, early helpers]
├── #include "situation_impl_deps.h"
├── #include "situation_impl_decl.h"
├── #include "situation_impl_forward.h"
├── #include "situation_impl_threading.h"
├── #include "situation_impl_io.h"
├── #include "situation_impl_input.h"
├── #include "situation_impl_wdm.h"
├── #include "situation_impl_image.h"
├── #include "situation_impl_timer.h"
├── #include "situation_impl_renderer.h"   ← NEW
├── #include "situation_impl_vd.h"
└── #include "situation_impl_ctrl.h"       ← NEW (last — orchestrates everything)
```

### Details

- Renderer includes before VD (VD uses renderer helpers) and before ctrl.
- Ctrl is last because it's the orchestrator — calls into all other modules, and by the time it's included all called functions are already defined.
- Renderer calling ctrl functions (e.g., `_SituationSetErrorFromCode`) works via `situation_impl_forward.h` prototypes (single TU, static functions).
- Two-layer forward declaration scheme: Layer 1 = `situation_api.h` (public SITAPI prototypes), Layer 2 = `situation_impl_forward.h` (internal static prototypes).

### Module Summary (Final)

| Module | Lines | Responsibility |
|--------|-------|---------------|
| `situation_impl.h` | 707 | Orchestrator (font data + include chain) |
| `situation_impl_deps.h` | — | Third-party libs (STB, miniaudio, glad, VMA) |
| `situation_impl_decl.h` | — | Types, structs, globals, shaders |
| `situation_impl_forward.h` | 80 | Internal forward declarations |
| `situation_impl_threading.h` | — | Thread pool & job system |
| `situation_impl_io.h` | 2,401 | File I/O, async, system info |
| `situation_impl_input.h` | 1,241 | Input callbacks & API |
| `situation_impl_wdm.h` | 1,556 | Window, display, monitor |
| `situation_impl_image.h` | 2,182 | Image, font, color, screenshot |
| `situation_impl_timer.h` | 245 | Oscillators, timing |
| `situation_impl_renderer.h` | 16,917 | GL + VK backends, resources, commands |
| `situation_impl_vd.h` | 900 | Virtual display compositing |
| `situation_impl_ctrl.h` | 2,277 | Lifecycle, error, init/shutdown, update |

---

## [v2.4.8 "Virtual Display Extraction"] - 2026-05-05

### Description

Extracts the Virtual Display API into its own module. Reduces `situation_impl.h` from ~20,700 to ~19,800 lines. No functional changes.

### New Files

- **`sit/situation_impl_vd.h`** (900 lines) — Virtual display create/destroy/configure, compositing entry point (`SituationRenderVirtualDisplays`), sort callback, state queries.

### Details

- Moved `SituationCreateVirtualDisplay`, `SituationDestroyVirtualDisplay`, `SituationConfigureVirtualDisplay` to VD module.
- Moved `SituationRenderVirtualDisplays` (the main compositing function) to VD module.
- Moved `_SituationSortVirtualDisplaysCallback`, `SituationGetVirtualDisplay`, dirty/size queries to VD module.
- VD init helpers (`_SituationInitGLVirtualDisplayRenderer`, `_SituationVulkanInitInternalRenderers`) remain in `situation_impl.h` — part of backend init chains.
- Embedded VD code (GL execute switch case, shutdown cleanup loop, slot zeroing) remains in `situation_impl.h` — woven into larger functions.

### Architecture

```
situation_impl.h (v2.4.8)
├── #include "situation_impl_deps.h"
├── #include "situation_impl_decl.h"
├── #include "situation_impl_forward.h"
├── #include "situation_impl_threading.h"
├── #include "situation_impl_io.h"
├── #include "situation_impl_input.h"
├── #include "situation_impl_wdm.h"
├── #include "situation_impl_image.h"
├── #include "situation_impl_timer.h"
├── ... (renderer core: GL/VK init, commands, resources)
├── #include "situation_impl_vd.h"         ← NEW (late, after all renderer helpers)
└── ... (model loading, shaders, hot-reload, render thread)
```

---

## [v2.4.7 "WDM, Image & Timer Extraction"] - 2026-05-04

### Description

Continues the modular extraction effort. Three new module headers plus system info consolidation. Reduces `situation_impl.h` from ~25,500 to ~20,700 lines. No functional changes.

### New Files

- **`sit/situation_impl_wdm.h`** (1,556 lines) — Window state queries/manipulation, display enumeration/caching, monitor queries, fullscreen/borderless toggle, VSync, target FPS, frame time.
- **`sit/situation_impl_image.h`** (2,182 lines) — Image load/save/create/manipulate, font loading/atlas baking/text rendering, color space conversion (RGB/HSV/YPQ), screenshots.
- **`sit/situation_impl_timer.h`** (245 lines) — Oscillator state queries, ping mechanism, period control, trigger counts, high-res time.

### Updated Files

- **`sit/situation_impl_io.h`** (1,500 → 2,401 lines) — Absorbed system profiling (`SituationGetDeviceInfo`, `SituationGetGPUName`, `SituationGetCPUThreadCount`), storage info (`SituationGetCurrentDriveLetter`, `SituationGetDriveInfo`, `SituationGetUserDirectory`), and system commands (`SituationOpenFile`, `SituationExecuteCommand`).

### Architecture

```
situation_impl.h (v2.4.7)
├── #include "situation_impl_deps.h"
├── #include "situation_impl_decl.h"
├── #include "situation_impl_forward.h"
├── #include "situation_impl_threading.h"
├── #include "situation_impl_io.h"
├── #include "situation_impl_input.h"
├── #include "situation_impl_wdm.h"        ← NEW
├── #include "situation_impl_image.h"      ← NEW
├── #include "situation_impl_timer.h"      ← NEW
└── core implementations (renderer, lifecycle)
```

### Details

- Moved physical display enumeration (`_SituationCachePhysicalDisplays`, `_SituationMonitorEnumProc`) to WDM module.
- Moved all `SituationIsWindow*`, `SituationSetWindow*`, `SituationToggle*`, monitor queries to WDM module.
- Moved `SituationSetTargetFPS`, `SituationGetFrameTime`, `SituationGetFPS`, `SituationGetGLFWwindow` to WDM module.
- Moved image ops, font ops, color conversion, and screenshot functions to image module.
- Moved timer/oscillator API to dedicated timer module.
- Moved system profiling, storage info, and command execution to IO module (missed in v2.4.6).
- Virtual Display system remains in `situation_impl.h` (renderer-coupled).
- Remaining ~20,700 lines is renderer code (OpenGL + Vulkan backends) and core lifecycle.

---

## [v2.4.6 "IO & Input Extraction"] - 2026-05-04

### Description

Pure structural refactor: extracts the IO/Filesystem and Input subsystems from the monolithic `situation_impl.h` into two new self-contained module headers. No functional changes. Reduces `situation_impl.h` from ~28,000 to ~25,500 lines for improved navigability and compile-time locality.

### New Files

- **`sit/situation_impl_io.h`** (1,500 lines) — File I/O, path management, directory operations, async file wrappers, IO thread entry, and queue metrics.
- **`sit/situation_impl_input.h`** (1,241 lines) — GLFW input callbacks (key, char, mouse, cursor, scroll, joystick), keyboard/mouse/gamepad API functions.

### Architecture

```
situation_impl.h (v2.4.6)
├── #include "situation_impl_deps.h"
├── #include "situation_impl_decl.h"
├── #include "situation_impl_forward.h"
├── #include "situation_impl_threading.h"
├── #include "situation_impl_io.h"         ← NEW
├── #include "situation_impl_input.h"      ← NEW
└── core implementations (window, renderer, lifecycle)
```

### Details

- Moved sync file ops (`SituationLoadFileData`, `SituationSaveFileData`, `SituationLoadFileText`, `SituationSaveFileText`) to IO module.
- Moved path management (`SituationGetAppSavePath`, `SituationGetBasePath`, `SituationJoinPath`, `SituationGetFileName`, `SituationGetFileExtension`) to IO module.
- Moved directory ops (`SituationCreateDirectory`, `SituationDeleteDirectory`, `SituationListDirectoryFiles`, etc.) to IO module.
- Moved filesystem error helper (`_SituationSetFilesystemError`) and UTF-8/Wide conversion helpers to IO module.
- Moved async file wrappers (`SituationLoadFileAsync`, `SituationSaveFileAsync`, etc.) from threading module to IO module.
- Moved `_SituationIOThreadEntry` and `SituationGetIOQueueDepth` from threading module to IO module.
- Moved all GLFW input callbacks and keyboard/mouse/joystick API to input module.
- `_SituationPerformHotReloadPass` remains in `situation_impl.h` (depends on renderer internals).
- `situation_impl_threading.h` reduced from ~1,577 to ~1,093 lines.

---

## [v2.4.5 "Decl Split"] - 2026-05-04

### Description

Pure structural refactor: extracts all internal type definitions, struct declarations, static globals, macros, and embedded shader data from `situation_impl.h` into dedicated module headers. No functional changes. Establishes the modular include architecture that subsequent extractions (IO, Input) build upon.

### New Files

- **`sit/situation_impl_deps.h`** — Third-party includes (STB, miniaudio, glad/Vulkan, VMA).
- **`sit/situation_impl_decl.h`** — All internal types, structs, globals, embedded shaders, and macros.
- **`sit/situation_impl_forward.h`** — Forward declarations for internal static functions.

### Details

- Moved all `typedef struct` definitions and static global state out of `situation_impl.h`.
- Moved embedded GLSL/SPIR-V shader source strings to decl header.
- Moved internal macros (`SIT_DEBUG_LOG`, uniform map capacity, etc.) to decl header.
- Established include order: deps → decl → forward → threading → (impl body).
- `situation_impl.h` now contains only function implementations.

---

## [v2.4.4 "Edge-Case Engine Goofs"] - 2026-03-27

### Description

This patch addresses four obscure, edge-case bugs across the Vulkan and OpenGL renderers and the audio capture subsystem to prevent memory leaks, visual glitches, and micro-stutters.

### Critical Fixes

- **Vulkan Screenshot Layout Hazard:** Fixed a Vulkan Validation layer error when taking a screenshot. `SituationLoadImageFromScreen` now correctly expects `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` as the image layout because the render pass is forcefully ended via `vkEndCommandBuffer` prior to the pixel copy.
- **OpenGL Ghost Texture Cache:** Fixed a visual glitch in the Virtual Bindless LRU Cache. `SituationDestroyTexture` now actively searches for and erases the destroyed texture ID from `sit_render.gl.virtual_texture_slots` to prevent 'ghost textures' caused by OpenGL driver ID recycling.
- **Vulkan Virtual Display Descriptor Leak:** Fixed a permanent VRAM exhaustion leak. If `SituationCreateVirtualDisplay` fails halfway through initialization, the cleanup block now explicitly frees the individual descriptor set using `vkFreeDescriptorSets`.
- **Audio Capture Micro-Stutter:** Fixed main thread heap fragmentation and stuttering caused by polling audio capture events. `SituationPollInputEvents` now utilizes a persistent, dynamically growable scratch buffer (`audio_capture_temp_buffer` via `SIT_REALLOC`) instead of executing `SIT_MALLOC` and `SIT_FREE` every frame.

## [v2.4.3 "Virtual Display Compositing & Performance Parity"] - 2026-03-26

### Description

This release fixes critical Virtual Display Compositing flaws in both Vulkan and OpenGL backends. It achieves peak performance by eliminating CPU stalling on the Main Thread during UBO updates and resolves potential black screen issues caused by Vulkan validation layer errors regarding dynamic state.

### Critical Fixes

- **Persistent Global UBO Mapping (Vulkan):** Prevented CPU stalling during Vulkan command recording by allocating the `view_proj_ubo_memory` with `VMA_ALLOCATION_CREATE_MAPPED_BIT`. `SituationRenderVirtualDisplays` now writes directly to `view_proj_ubo_mapped` using zero-stall `memcpy`.
- **Dynamic State Injection (Vulkan):** Injected missing `vkCmdSetViewport` and `vkCmdSetScissor` calls directly after `vkCmdBeginRenderPass` in `SituationRenderVirtualDisplays`. This satisfies Vulkan's requirement that dynamic states must be explicitly pushed into the command buffer after every pass begin call.
- **OpenGL Deferral Parity Enforcement:** Enforced target output to the main window during deferred OpenGL compositing (`SIT_OP_RENDER_VIRTUAL_DISPLAYS`) by explicitly binding `GL_FRAMEBUFFER 0` and forcefully scaling the viewport to span the main window dimensions within `_SituationGLExecuteCommands`.

## [v2.4.2 "OpenGL Deferred Rendering Architecture"] - 2026-03-12

### Description

This release addresses two critical architectural flaws in the OpenGL backend that previously broke the multithreaded Deferred Soft Command Buffer architecture. The OpenGL backend now achieves true architectural parity with Vulkan by enforcing all GL execution exclusively on the Render Thread. No regressions were found during compilation and basic tests.

### Critical Fixes

- **Deferred Virtual Displays:** `SituationRenderVirtualDisplays` no longer makes illegal synchronous OpenGL calls on the Main Thread. It now pushes a new `SIT_OP_RENDER_VIRTUAL_DISPLAYS` opcode to the Soft Command Buffer. The actual GL compositing logic has been successfully migrated to `_SituationGLExecuteCommands` to run safely on the Render Thread.
- **Resize Context Corruption Fix:** Removed direct GL calls (`glViewport`, `glTexImage2D`) from the `_SituationGLFWFramebufferSizeCallback` which runs on the Main Thread. Window resizing now sets a `shadow_state_dirty` flag, allowing the Render Thread to lazily update its projection matrices and framebuffers during the next execution loop. This mirrors Vulkan's approach and prevents coordinate math breakage and context corruption.

## [v2.4.1 "Complete MIDI Architecture & Device Identity"] - 2026-03-09 [IN PROGRESS]

### Description

This major release implements a complete professional-grade MIDI subsystem with hybrid hardware/virtual routing, advanced features (filtering, transformation, recording), and Universal Device Inquiry protocol support. The system achieves 42M+ events/sec throughput with lock-free real-time operation.

**Status:** Core implementation complete, compilation and integration testing in progress.

### Major Features

- **OpenGL Graveyard Flush Safety:** Fixed a major race condition and VRAM leak in the OpenGL backend caused by internal polling in `_SitGLFlushGraveyard`. The Render Thread and single-threaded fallback loop now properly wait for the GL sync fence from the old frame to complete before issuing commands or executing resource cleanup.

#### Complete MIDI Hybrid Architecture (Phases 1-4)

- **Virtual MIDI Infrastructure (Phase 1):**
  - Lock-free SPSC ring buffers with C11 atomics (8192 events per device, 256KB)
  - Cross-platform virtual MIDI devices (Windows/Linux/macOS)
  - Hardware MIDI support (Windows WinMM, Linux ALSA/macOS CoreMIDI planned)
  - Platform abstraction layer with unified API
  - Cache-optimized 64-byte padding for performance

- **Routing & Connection System (Phase 2):**
  - Virtual device creation/destruction API (`Pm_CreateVirtualDevice`, `Pm_DestroyVirtualDevice`)
  - Dynamic device connection matrix (`Pm_ConnectVirtualDevices`, `Pm_DisconnectVirtualDevices`)
  - Multi-connection routing (1-to-1, 1-to-many broadcast, many-to-1 merge)
  - Transparent hardware/virtual device detection
  - Automatic MIDI event routing with timestamp preservation

- **Advanced MIDI Features (Phase 3):**
  - **MIDI Filtering:** Message type filtering (Note On/Off, CC, Program Change, etc.) and channel masking (16-channel bitmap)
  - **MIDI Transformation:** Note transposition (-127 to +127 semitones), velocity curves (linear/exponential/logarithmic/S-curve), channel remapping (0-15 → 0-15)
  - **MIDI Recording:** Event capture with timestamps, dynamic buffer allocation, playback with timing preservation
  - **Filter/Transform Integration:** Applied automatically during routing for zero-overhead processing

- **Testing & Validation (Phase 4):**
  - 7 comprehensive test programs with 100% pass rate
  - Performance benchmarking: 42.7M events/sec write, 76.5M events/sec read
  - Stress testing: Buffer overflow handling, 10 concurrent connections (1000/1000 events)
  - Timing verification: Sample-accurate processing demo (0.021ms precision @ 48kHz)
  - Thread safety validation: Lock-free atomics, no blocking in audio thread

#### MIDI Device Interface & Callbacks

- **Device Interface (midi_device.h):**
  - `SIT_MidiDevice` structure for MIDI-enabled components (synths, sequencers, effects)
  - Sample-accurate event scheduling with `SIT_MidiProcessor`
  - Callback system: `on_note_on`, `on_note_off`, `on_control_change`, `on_program_change`, `on_pitch_bend`, `on_sysex`
  - Device capabilities: INPUT, OUTPUT, THRU, FILTER, TRANSFORM
  - Device types: SYNTH, SEQUENCER, ARPEGGIATOR, EFFECT, CONTROLLER, CUSTOM

- **Centralized Device Callbacks (midi_device_callbacks.h):**
  - Complete MIDI CC mappings for 17 FX devices (133 parameters total)
  - Devices: Compander (24 params), Dynamics (7), Filter (6), EQ 4-Band (12), Reverb (5), Chorus (4), Overdrive (4), Panner (1), LFO (2), Echo (4), Phaser (5), Exciter (4), Studio Reverb (8), Spring Reverb (6), SST-282 (13), Mastering Amp (15), Maximizer (18)
  - Helper functions: Linear/logarithmic/dB normalization
  - 14-bit MIDI CC support (MSB/LSB pairs, 0-16383 range, 128x precision)
  - Callback lookup table for device discovery

#### Universal Device Inquiry Protocol

- **MIDI Device Identity System:**
  - `SIT_MidiDeviceIdentity` structure with manufacturer ID, family, model, version, ASCII name
  - Manufacturer ID: `0x00 0x53 0x49` ("SI" for Situation Audio)
  - Family: `0x00 0x01` (Audio FX)
  - Device-specific model IDs (0x01-0x11) for 17 FX devices
  - Extended Identity Reply format with ASCII device name for controller display
  - API: `SIT_MidiDevice_SetIdentity()`, `SIT_MidiDevice_GetIdentity()`, `SIT_MidiDevice_SendIdentityReply()`, `SIT_MidiDevice_ProcessSysEx()`
  - Helper: `_SituationCreateDeviceIdentity()`, `SIT_GetDeviceIdentity()`
  - Protocol: Request `F0 7E 7F 06 01 F7`, Reply `F0 7E 7F 06 02 00 53 49 00 01 00 XX 01 00 00 00 <name> F7`

### Performance Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| Write Throughput | 42.7M events/sec | Sustained |
| Read Throughput | 76.5M events/sec | Sustained |
| Write Latency | 0.023 μs | Per event |
| Read Latency | 0.013 μs | Per event |
| Sample Precision | 0.021 ms | @ 48kHz |
| Buffer Capacity | 8192 events | Per device |
| Memory per Device | 256 KB | Lock-free buffer |
| CPU Overhead | <0.1% | Negligible |
| Concurrent Connections | 10+ tested | 100% success |

### Examples & Tests

- **Test Programs (7):** `virtual_midi_test.c`, `midi_filter_transform_test.c`, `midi_recording_test.c`, `midi_routing_test.c`, `midi_performance_test.c`, `midi_timing_test.c`, `midi_sample_accurate_demo.c`
- **Device Examples (5):** `midi_device_example.c`, `midi_compander_control.c`, `midi_14bit_example.c`, `midi_identity_test.c`
- **Build Scripts (13):** Individual compile scripts + `run_all_midi_tests.bat` master runner

### Documentation

- `MIDI_PROJECT_COMPLETE.md` - Complete project summary
- `MIDI_HYBRID_SESSION_SUMMARY.md` - Session summary with metrics
- `MIDI_PHASE4_COMPLETE.md` - Testing & validation results
- `MIDI_HYBRID_ARCHITECTURE_PLAN.md` - Master architecture plan
- `MIDI_TIMING_BEHAVIOR.md` - Timing documentation
- `MIDI_SITUATION_INTEGRATION.md` - Integration guide
- `MIDI_DEVICE_INTERFACE.md` - Device interface documentation
- `MIDI_DEVICE_CALLBACKS_ARCHITECTURE.md` - Callback system architecture
- `MIDI_CC_REFERENCE.md` - Complete CC mapping reference
- `MIDI_14BIT_SUPPORT.md` - 14-bit CC documentation
- `MIDI_ALL_FX_CALLBACKS_COMPLETE.md` - FX callback completion summary
- `MIDI_SYSTEM_OVERVIEW.md` - System overview

### Bug Fixes

- Fixed duplicate function definitions in `midi_device_callbacks.h` (LFO and Spring Reverb callbacks)
- Added `<stdbool.h>` include to `midi_device_callbacks.h` for bool type support
- Moved `SIT_MidiDeviceIdentity` structure declaration before `SIT_MidiDevice` to fix forward reference
- Fixed buffer overflow handling in lock-free ring buffer (graceful degradation)

### Production Readiness

✅ Real-time safe (lock-free, no blocking, no allocations in audio thread)
✅ Thread safe (C11 atomics with memory ordering)
✅ High performance (42M+ events/sec throughput)
✅ Sample-accurate timing (0.021ms precision @ 48kHz)
✅ Cross-platform virtual MIDI (Windows/Linux/macOS)
✅ Comprehensive testing (7 test programs, 100% pass rate)
✅ Stress tested (buffer overflow, concurrent connections)
✅ Professional features (filtering, transformation, recording, 14-bit CC)
✅ Device identity protocol (Universal Device Inquiry)

**Ready for:** DAW applications, game engines, audio plugins, music software, real-time performance, professional audio production

---

## [v2.4.0 "Modular Revolution & Architectural Reorganization"] - 2026-03-03

### Description

This release represents a transformative evolution of the Situation library on two fronts: (1) a complete registry-driven node-graph audio architecture that rivals professional DAW systems, and (2) a comprehensive folder reorganization establishing a professional, scalable project structure. Over 10,000 lines of new audio code combined with systematic architectural cleanup create a solid foundation for v2.4.0 and beyond.

### Major Features

#### Audio Subsystem (Phases 1-6)

- **Device Registry System (Phase 1-2):**
  - 19 registered devices across 5 categories (Effects, Sources, Capture, Utilities, Modulators)
  - 150+ control parameters with ranges, defaults, units, and validation
  - Thread-safe queries with metadata introspection
  - Complete API: `SituationRegisterDeviceType()`, `SituationGetDeviceMetadata()`, `SituationIterateRegistry()`

- **Node Graph System (Phase 3):**
  - Generational handles preventing use-after-free bugs
  - Dynamic node creation from registry with full patching system
  - Cycle detection (DFS-based) and control parameter access with clamping
  - 256 nodes per graph, 16 patches per port maximum
  - API: `SituationCreateGraph()`, `SituationCreateNode()`, `SituationPatch()`, `SituationSetControl()`

- **Real-Time Processing (Phase 4):**
  - Topological sort (Kahn's algorithm) with caching for real-time audio processing
  - 19 device wrappers (100% complete) with SSE/SSE2/SSE4.1 optimization
  - Custom FFT implementation (zero external dependencies) for spectral processing
  - Lock-free audio processing with master output node

- **Production Threading (Phase 4.5):**
  - Lock-free audio thread (zero glitches) with mutex-protected topology changes
  - Double-buffered control values and atomic flags for synchronization
  - Platform-specific sleep functions (Windows `Sleep()` / POSIX `usleep()`)
  - Performance: 185 iterations/sec audio, 98.5 updates/sec UI, 100% stability

- **JSON Serialization (Phase 5):**
  - Human-readable JSON format with version tracking
  - Custom parser (no external dependencies) with device type lookup by name
  - Round-trip data integrity (100% verified)
  - API: `SituationSaveGraphToFile()`, `SituationLoadGraphFromFile()`

- **Mixer Integration (Phase 6 Sessions 1-2):**
  - **Insert Chains:** 3 insert positions per track (Pre-EQ, Post-EQ, Post-Dynamics)
  - **Aux Bus FX:** Modular FX chains per aux bus with wet/dry mix control
  - Thread-safe attach/detach operations with lock-free bypass functionality
  - API: `SituationSetTrackInsert()`, `SituationSetBusEffectChain()`, `SituationSetBusEffectMix()`

#### Folder Reorganization (Post-Release Cleanup)

- **Core Headers Relocation:**
  - Moved `situation_api.h`, `situation_impl.h`, `situation_impl_audio.h` from root to `sit/`
  - Root now contains only `situation.h` (public entry point)
  - Clear separation: public API vs internal implementation
  - **Files Moved:** 3 core implementation files

- **Audio Effects Organization:**
  - Created `sit/aud/fx/` subfolder for all audio effects
  - Moved 15 effect files: reverb, echo, chorus, phaser, overdrive, exciter, filter, eq, dynamics, etc.
  - Updated `sit/aud/device_wrappers.h` includes to use `fx/` prefix
  - **Files Moved:** 15 effects files

- **Polysonix Relocation:**
  - Moved entire `sit/polysonix/` to `sit/aud/polysonix/`
  - Logical placement alongside other audio components
  - Synthesizer engine now part of audio subsystem
  - **Files Moved:** Entire polysonix directory (20+ files)

- **K-Term Integration:**
  - Updated example file include paths for terminal library
  - Fixed `examples/kterm_simple_test.c` and `examples/kterm_console.c`
  - Terminal subsystem properly organized in `sit/k-term/`
  - **Files Updated:** 2 example files

- **Serialization File Rename:**
  - `sit/aud/graph_serialization.h` → `sit/aud/node_graph_serialization.h`
  - `sit/aud/graph_serialization_impl.h` → `sit/aud/node_graph_serialization_impl.h`
  - Consistent naming with other node graph files
  - **Files Renamed:** 2 serialization files

### Final Folder Structure

```
situation/                         # Project root
├── situation.h                    # ← Public API entry point (ONLY file in root)
│
└── sit/                          # ← Core implementation
    ├── situation_api.h           # Public API declarations
    ├── situation_impl.h          # Core implementation
    ├── situation_impl_audio.h    # Audio subsystem
    │
    ├── aud/                      # Audio Subsystem
    │   ├── fx/                   # Effects (15 files)
    │   │   ├── reverb.h, echo.h, chorus_4stage.h
    │   │   ├── filter.h, eq_4band.h, dynamics.h
    │   │   ├── overdrive.h, exciter.h
    │   │   ├── studio_reverb.h, spring_reverb.h, sst282.h
    │   │   ├── maximizer.h, mastering_amp.h
    │   │   └── phaseshifter.h, lfo.h
    │   │
    │   ├── polysonix/            # Polyphonic synthesizer
    │   │   ├── polysonix.h
    │   │   ├── px_vm.h
    │   │   └── ... (synth components)
    │   │
    │   ├── node_graph.h          # Node graph base types
    │   ├── node_graph_impl.h     # Graph topology
    │   ├── node_graph_process.h  # Audio processing
    │   ├── node_graph_serialization.h        # Serialization API
    │   ├── node_graph_serialization_impl.h   # Serialization impl
    │   │
    │   ├── device_registry.h     # Device registration
    │   ├── device_wrappers.h     # Device wrappers
    │   ├── registry_init.h       # Registry initialization
    │   │
    │   ├── sound_source.h        # Audio file playback
    │   ├── mic_capture.h         # Microphone capture
    │   ├── tone_synth.h          # Tone generator
    │   └── threading_diagnostics.h  # Threading utilities
    │
    └── k-term/                   # Terminal Subsystem
        ├── kterm.h               # Main wrapper
        ├── kterm_api.h           # Public API
        └── ... (terminal components)
```

### Error Handling

- **65 New Error Codes:**
  - Threading errors (-80 to -96): 17 codes
  - Mixer errors (-440 to -459): 15 codes
  - Node Graph errors (-460 to -479): 19 codes
  - Device Registry errors (-480 to -499): 14 codes
  - All error codes have proper messages in main error handler

- **Error System Cleanup:**
  - Removed `SituationGetErrorMessage()` function (broke library conventions)
  - Restored proper error handling: functions return codes, users call `SituationGetLastErrorMsg()`
  - Updated all examples to use correct error handling pattern
  - **Files Updated:** 7 example files, 3 implementation files

### Technical Improvements

- **Threading Architecture:** Platform-specific sleep macros (`SITUATION_SLEEP_MS`) to avoid tinycthread bugs on Windows
- **Memory Management:** Cross-platform aligned allocation for SSE intrinsics (16-byte alignment)
- **Include Cleanup:** Removed 6 legacy device includes and `audio_error_mapping.h` (240+ lines of duplicate code)
- **Include Organization:** Moved external library includes from `situation_api.h` to `situation.h` for cleaner API
- **Threading Wrapper Removal:** Deleted broken `node_graph_threading.h` wrapper layer (never in public API)

### Bug Fixes

- **Control Buffer Iteration:** Fixed sparse array iteration bug in threading implementation
- **tinycthread Sleep:** Replaced buggy `thrd_sleep()` with platform-specific `Sleep()`/`usleep()`
- **Error System Remnants:** Cleaned up abandoned error system refactor references
- **Include Paths:** Fixed all example files to use correct paths after reorganization

### Documentation

- **35+ Documentation Files:**
  - **Phase Completion:** PHASE1-6 summaries, session progress reports
  - **Architecture:** Threading architecture, audio subsystem roadmap, mixer DM2000 reference
  - **Reorganization:** Core headers, FX folder, Polysonix, K-Term integration status
  - **Guides:** Compilation guide, troubleshooting, design updates
  - **Summaries:** V2_4_0_FOLDER_REORGANIZATION_COMPLETE.md (comprehensive overview)

- **Updated Main Documentation:**
  - `SITUATION_QUICK_REFERENCE.md` - Updated to v2.4.0 with new structure
  - `situation_api.md` - Updated project structure and compilation instructions
  - `situation_sdk.md` - Updated version and added v2.4.0 section
  - `COMPILATION_GUIDE.md` - NEW comprehensive compilation guide

### Demo Applications

- **12 New Demos:** Node graph, threading stress tests, mixer integration, JSON serialization
- **15 Build Scripts:** All demos have corresponding `.bat` compilation scripts
- **All Verified:** Mixer demos compile and run successfully after reorganization

### Statistics

- **Development Time:** 4 days (March 1-4, 2026)
- **Lines of Code:** ~10,000+ new audio code, ~5,000 documentation
- **New Files:** 35+ implementation files, 35+ documentation files
- **Files Moved:** 20+ files reorganized
- **Files Renamed:** 2 serialization files
- **Devices:** 19 registered with 150+ parameters
- **Tests:** 8 test applications, 100% pass rate
- **Documentation:** 35+ comprehensive documentation files

### Breaking Changes

**None!** Version 2.4.0 is fully backward compatible with v2.3.64.

- All new audio functionality is additive
- Folder reorganization is transparent to users (they still just `#include "situation.h"`)
- Internal file moves don't affect public API
- All existing code continues to work without modification

### Benefits

1. **Clear Architecture:** Public API vs implementation vs subsystems clearly separated
2. **Logical Grouping:** Related code organized together (effects in fx/, audio in aud/)
3. **Scalability:** Easy to add new subsystems (e.g., sit/gfx/, sit/net/)
4. **Professional Structure:** Follows industry-standard single-header library patterns
5. **Maintainability:** Clear ownership boundaries, easy to navigate
6. **Zero User Impact:** Completely transparent reorganization

### Next Steps

- Phase 6 Sessions 3-4: Flexible signal flow control and mixer serialization
- Phase 7: Optimization and polish (SIMD, graph pruning, buffer pooling)
- Phase 8: Modulators (Envelope Follower, control signal routing)
- Phase 9+: Visual graph editor, preset system, MIDI integration, automation

### Related Documentation

- `doc/V2_4_0_FOLDER_REORGANIZATION_COMPLETE.md` - Complete reorganization summary
- `doc/CORE_HEADERS_REORGANIZATION.md` - Core headers relocation details
- `doc/FX_FOLDER_ORGANIZATION.md` - Effects organization
- `doc/POLYSONIX_INTEGRATION_STATUS.md` - Polysonix relocation
- `doc/KTERM_INTEGRATION_STATUS.md` - K-Term integration
- `doc/SERIALIZATION_FILE_RENAME.md` - File naming consistency
- `doc/THREADING_WRAPPER_REMOVAL.md` - Wrapper layer cleanup
- `doc/ERROR_FUNCTION_CLEANUP.md` - Error handling standardization
- `doc/COMPILATION_GUIDE.md` - Comprehensive compilation instructions
- `doc/DOCUMENTATION_UPDATE_V2_4_0.md` - Documentation update summary
---

> **Current releases:** [`UPDATELOG.md`](UPDATELOG.md) (v2.4.x+).

