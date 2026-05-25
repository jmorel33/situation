# Threading Bolstering — Manual Validation (Dual-Socket / NUMA)

Use this checklist on a reference host (e.g. dual Xeon) after building with `SITUATION_ENABLE_THREADING`.

## Build & run

```powershell
Set-Location "c:\Users\User\Desktop\hobby\_kiro\situation"
& ".\build_situation.bat" opengl
& ".\build_tests.bat" opengl
Copy-Item -Force "build\dll\situation_opengl.dll" "build\"
Set-Location "build"
$env:PATH = "dll;C:\msys64\mingw64\bin;$env:PATH"
& ".\sit_test.exe" --module threading
```

Expect **21/21** threading tests (or current count from harness), including **`cpu_stress_10s_taskmgr`** (~10 s — open Task Manager first).

```powershell
& ".\sit_test.exe" --module threading --filter cpu_stress
```

Set `SIT_SKIP_CPU_STRESS=1` to skip the burn test in CI. The test prints a **logical CPU histogram** and **worker last-seen CPU** lines — compare those indices to Task Manager’s per-logical-processor graphs.

## Init policy (application)

```c
SituationInitInfo info = {0};
info.thread_affinity_main   = 1ULL << 0;  // optional main pin
info.thread_affinity_render = 1ULL << 1;
info.thread_affinity_audio  = 1ULL << 2;
info.numa_prefer_local      = true;
info.worker_numa_spread     = true;
info.io_thread_numa_node    = 0;           // or -1 to skip I/O pin
info.thread_pool_reserved_threads = 3;     // main + render + audio
info.thread_pool_use_physical_cores = true;
SituationInit(argc, argv, &info);
```

Affinity failures are **fail-soft** (debug warning; init continues).

## Checks

- [ ] `SituationPrintThreadingStatus(NULL)` — logical/physical counts and NUMA node count match OS
- [ ] After heavy `SituationDispatchParallel`, `SituationDumpThreadPoolStatus(pool, stderr, false)` — workers on distinct CPUs (not all 0)
- [ ] Render/audio slots in snapshot show stable affinity masks and CPUs
- [ ] `SituationSetThreadAffinityEx` + `SituationGetThreadAffinity` round-trip on a test mask
- [ ] `SituationGetThreadPoolMetrics` — `dispatch_parallel_calls` > 0 after parallel work; `io_busy_ratio` sensible when I/O enabled
- [ ] `SituationDumpThreadingReport` — readable combined status + pool dump

## Regression

- [ ] `sit_test.exe --module threading` all green
- [ ] Copy DLL to `build\` before harness if you only rebuilt `build\dll\`
