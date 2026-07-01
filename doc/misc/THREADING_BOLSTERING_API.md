# Threading Bolstering — Public API (v2.4.139–144)

**Release line:** v2.4.144 **"Threading Bolstering Complete"**  
**Plan:** `doc/plan/THREADING_BOLSTERING_PLAN.md`  
**Ops:** `doc/THREADING_TROUBLESHOOTING_GUIDE.md`, `doc/THREADING_MANUAL_VALIDATION.md`  
**Harness:** `sit_test.exe --module threading` — **21** tests (includes `cpu_stress_10s_taskmgr`, ~10 s)

The generational dual-queue pool is unchanged in spirit: **mutex per queue**, **atomics** for indices and job state, **main-thread helping** on the high queue during `SituationDispatchParallel` (not worker-to-worker steal). Bolstering adds topology, placement, observability, scheduler metrics, and init-time policy.

---

## Init (`SituationInitInfo`)

| Field | Default | Purpose |
|-------|---------|---------|
| `thread_affinity_main` | `0` | Pin main thread after window create; `0` = no pin |
| `thread_affinity_render` | `0` | Render thread mask; `0` → core 1 (or NUMA-local) |
| `thread_affinity_audio` | `0` | Audio callback mask; `0` → core 2 (or NUMA-local) |
| `numa_prefer_local` | `false` | When affinity masks are `0`, derive render/audio masks from NUMA of default cores |
| `worker_numa_spread` | `false` | Pin worker *i* to NUMA node `i % node_count` at worker entry |
| `io_thread_numa_node` | `< 0` | Dedicated I/O thread NUMA node; `< 0` = no pin |
| `thread_pool_use_physical_cores` | `false` | Auto worker count uses physical cores if true, else logical |
| `thread_pool_reserved_threads` | `0` | Reserved for main/render/audio/IO; `0` treated as **4** |

Affinity failures are **fail-soft** (debug warning; init continues).

---

## Topology & affinity (Epic A)

| API | Role |
|-----|------|
| `SituationRefreshCpuTopology()` | Rebuild process-wide CPU cache |
| `SituationGetCpuTopology()` | Read-only `SituationCpuTopology` |
| `SituationGetCPUThreadCount()` | Logical processor count |
| `SituationGetCPUCoreCount()` | Physical core count (topology) |
| `SituationSetThreadAffinity()` / `SituationSetThreadAffinityEx()` | Pin **current** thread; optional previous mask |
| `SituationGetThreadAffinity()` | Read current thread mask |
| `SituationGetCurrentProcessorIndex()` | Logical CPU index for current thread |
| `SituationGetThreadNumaNode()` | NUMA node for current thread |
| `SituationBuildPhysicalCoreMask()` | All logical CPUs on one physical core |
| `SituationBuildUniqueCoreMask()` | One LP per core, optional HT avoidance |
| `SituationBuildNumaNodeMask()` | All logical CPUs on a NUMA node |
| `SituationGetConfiguredMainThreadAffinity()` | Init main mask |
| `SituationGetConfiguredRenderThreadAffinity()` | Effective render mask |
| `SituationGetConfiguredAudioThreadAffinity()` | Effective audio mask |
| `SituationGetConfiguredIOThreadAffinity()` | Effective I/O mask (init or default CPU 3) |

---

## Observability (Epic B)

| API | Role |
|-----|------|
| `SituationGetThreadingStatus()` | Capabilities, topology OK, pool summary |
| `SituationPrintThreadingStatus()` | Human-readable status |
| `SituationGetQueueDepth()` / `SituationGetHighQueueDepth()` | Pending jobs |
| `SituationGetActiveJobCount()` | Running + pending counter |
| `SituationGetThreadPoolSnapshot()` | Per-role last logical CPU, NUMA, stats |
| `SituationDumpThreadPoolStatus()` | Pool + worker/I/O/render/audio placement |
| `SituationDumpThreadingReport()` | Status + topology line + pool dump |

Types: `SituationThreadingStatus`, `SituationThreadPoolSnapshot`, `SituationThreadSlotSnapshot`, `SituationJobQueueMask`.

---

## NUMA (Epic C)

| API | Role |
|-----|------|
| `SituationRefreshNumaTopology()` | NUMA summary from CPU cache + OS memory |
| `SituationGetNumaTopology()` | `SituationNumaTopology` / `SituationNumaNodeInfo` |
| `SituationGetPreferredNumaNode()` | TLS preferred node for allocators (workers/I/O/render/audio) |

---

## Scheduler metrics & sizing (Epic D)

| API | Role |
|-----|------|
| `SituationGetRecommendedWorkerCount(reserved, use_physical)` | Sizing without a pool |
| `SituationGetThreadPoolMetrics()` | `SituationThreadPoolMetrics` snapshot |
| `SituationResetThreadPoolStats()` | Zero scheduler counters |
| `SituationDumpThreadPoolMetrics()` | Metrics-only dump |

`SituationCreateThreadPool(..., num_threads=0, ...)` uses `thread_pool_*` init fields via `_SitResolveAutoWorkerCount()`. High-queue dequeue scans the **full pending range** with **in-place claim** (v2.4.232–233; the former 4–32 capped scan is removed).

**Thread naming (v2.4.239):** `SituationInitInfo::main_thread_name` (NULL → `SITUATION_MAIN_THREAD_NAME_DEFAULT`), **`SituationSetCurrentThreadName`** for the calling thread. Snapshot slot names reflect the configured main thread name.

---

## Existing pool API (unchanged entry points)

`SituationCreateThreadPool`, `SituationDestroyThreadPool`, `SituationSubmitJobEx`, `SituationDispatchParallel`, `SituationWaitForJob`, `SituationWaitForAllJobs`, dependency APIs, `SituationDumpTaskGraph`, async I/O job helpers.

---

## Deferred (not in v2.4.144)

- Lock-free MPMC high queue  
- `SITUATION_EXPERIMENTAL_STEALING` / worker-to-worker steal  
- mybuddy NUMA allocator integration (separate track)

---

## Quick validation

```powershell
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
Copy-Item -Force build\dll\situation_opengl.dll build\
Set-Location build
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\sit_test.exe" --module threading
& ".\sit_test.exe" --module threading --filter cpu_stress   # ~10 s, Task Manager / CPU counters
```

OpenGL full harness: **391** tests. Vulkan: **381** tests (10 OpenGL-only graphics tests).
