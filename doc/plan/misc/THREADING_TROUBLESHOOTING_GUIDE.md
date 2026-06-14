# Threading Troubleshooting Guide

Quick reference for the Situation generational thread pool, CPU topology (Epic A), and pool observability (Epic B).

## Sleep / worker hangs (Windows)

- Use **`SITUATION_SLEEP_MS()`** from `situation_impl_threading_diag.h` — never `thrd_sleep()` on Windows with tinycthread.
- Prefer native sleep: Windows `Sleep`, POSIX `usleep`.

## Check runtime capabilities

```c
SituationThreadingStatus st = SituationGetThreadingStatus();
SituationPrintThreadingStatus(stdout);
```

**Full API catalog (v2.4.144):** `doc/THREADING_BOLSTERING_API.md`

Fields added in v2.4.140+: `platform_topology_ok`, `numa_available`, `pool_thread_count`, `io_thread_enabled`.

**Scheduler metrics (v2.4.142+):** `SituationGetThreadPoolMetrics` / `SituationDumpThreadPoolMetrics` — high-queue lock time, main-thread steal counters, scan-forward stats, I/O busy ratio, inline submit and queue-full spin counts. Use after reproducing stalls or before changing affinity/NUMA policy.

**Manual dual-socket checklist:** `doc/THREADING_MANUAL_VALIDATION.md` (v2.4.143+).

## NUMA (Epic C)

```c
SituationRefreshNumaTopology();
const SituationNumaTopology* numa;
SituationGetNumaTopology(&numa);

int node = SituationGetPreferredNumaNode(); // -1 until thread is placed
```

**Init policy** (`SituationInitInfo`):

| Field | Effect |
|-------|--------|
| `main_thread_name` | OS-visible main thread name (NULL → `"Sit Main"`); applied at window creation |
| `numa_prefer_local` | Render/audio use NUMA node of default logical cores when affinity mask is 0 |
| `worker_numa_spread` | Worker `i` pins to NUMA node `i % node_count` at thread entry |
| `io_thread_numa_node` | If `>= 0`, dedicated I/O thread pins to that node |

## OS thread names (v2.4.239)

```c
SituationInitInfo info = {0};
info.main_thread_name = "My Game";  // Task Manager / debugger label

SituationSetCurrentThreadName("Custom Worker");  // any thread, any time
```

Built-in names: `"Sit Main"`, `"Sit Worker N"`, `"Sit I/O"`, `"Sit Render"`, `"Sit Audio"`. Linux truncates to 15 characters at the kernel.

Coordinates with **mybuddy** `MBD_FLAG_NUMA_AWARE` — align allocator node with `SituationGetPreferredNumaNode()` on worker threads.

## CPU topology & affinity (Epic A)

```c
SituationRefreshCpuTopology();
const SituationCpuTopology* topo;
SituationGetCpuTopology(&topo);

uint64_t prev;
SituationSetThreadAffinityEx(SituationBuildNumaNodeMask(0), &prev);

int cpu = SituationGetCurrentProcessorIndex();
int node = SituationGetThreadNumaNode();
```

- Affinity bit **N** = logical processor **N** (max **64** bits: `SITUATION_AFFINITY_MASK_BITS`).
- Override render/audio pins: `SituationInitInfo::thread_affinity_render` / `thread_affinity_audio` (0 = defaults core 1 / 2).

## Pool observability (Epic B)

```c
SituationThreadPoolSnapshot snap;
SituationGetThreadPoolSnapshot(&pool, &snap);

SituationDumpThreadingReport(&pool, stderr, false);  // status + topology line + pool
SituationDumpTaskGraph(&pool, stderr, false);      // active job graph
```

| API | Meaning |
|-----|---------|
| `SituationGetActiveJobCount` | Pending + running jobs |
| `SituationGetQueueDepth(pool, SIT_JOB_QUEUE_LOW)` | Low / I/O queue |
| `SituationGetHighQueueDepth` | High / physics queue |
| `SituationGetIOQueueDepth` | Same as low queue on global `sit_gs.thread_pool` |

Worker **last_logical_cpu** is sampled every 8 jobs and on idle wake; I/O thread samples each loop iteration.

## Common issues

| Symptom | Likely cause |
|---------|----------------|
| All workers on CPU 0 | No affinity set; OS scheduler — set masks and re-check snapshot |
| `GetThreadAffinity` fails on Windows | Older OS without `GetThreadGroupAffinity` — falls back to process mask |
| Jobs never finish | Dependency cycle or full queue without `SIT_SUBMIT_RUN_IF_FULL` |
| Main thread stall in `DispatchParallel` | Normal — main helps high queue until batches complete |
| `-557` after unload-during-load + new async shader | Pool job not retired on abandon — check v2.4.238+ progress driver; re-run `async_shader_poll_after_unload_during_load` |
| `-99` on async shader poll | Lost compile job (settled handle, `compile_done` still 0) — scheduler defect; see v2.4.230+ |

## Harness

```text
sit_test.exe --module threading
```

Tests: `cpu_topology_refresh`, `affinity_roundtrip`, `mask_builders`, `pool_snapshot_after_parallel`, `threading_status_export`.

See **`doc/plan/THREADING_BOLSTERING_PLAN.md`** for the full roadmap.
