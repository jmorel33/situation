# 10 — Thread Pool: Parallel Work

**Tier 2 — Feature Spotlight** | OpenGL + Vulkan | Requires `SITUATION_ENABLE_THREADING` (on by default in `build_examples.bat`)

A **64×64 Game of Life** grid. Each frame runs **24 generations** on the CPU, uploads the result as an RGBA texture, and draws it scaled on screen. Press **`T`** to compare **serial** (main thread loop) vs **parallel** (`SituationDispatchParallel` fork-join) — the per-cell update includes extra hashing so the timing difference is obvious.

## Build & run

```bat
build\build_situation.bat static-opengl
build\build_examples.bat static-opengl 10_thread_pool
build\examples\10_thread_pool.exe
```

Vulkan: replace `static-opengl` with `static-vulkan`.

## Keys

| Key | Action |
|-----|--------|
| `T` | Toggle serial / parallel update |
| `R` | Re-seed grid (async jobs with dependency) |
| `SPACE` | Pause simulation |
| Universal | `ESC`, `F11`, `F9`, `P`, `F12` via `sit_example.h` |

## APIs demonstrated

| API | Role |
|-----|------|
| `SituationCreateThreadPool` / `SituationDestroyThreadPool` | Worker pool lifecycle (`num_threads=0` → auto size) |
| `SituationDispatchParallel` | Fork-join over 4096 cells per generation |
| `SituationSubmitJobEx` + `SituationAddJobDependency` | Re-seed: random fill → stamp gliders |
| `SituationGetThreadPoolSnapshot` | HUD: worker count, active jobs |
| `SituationCreateImage` / `SituationCreateTexture` | CPU grid → GPU texture each frame |

## Update-before-draw

Simulation writes **`g_buf`** on the CPU first, then **`upload_texture()`** fills `SituationImage` and creates the GPU texture, **then** draw commands reference that texture. Never dispatch parallel work that touches GPU resources from worker threads.

## Notes

- `build_examples.bat` defines `-DSITUATION_ENABLE_THREADING` for static example builds.
- Parallel mode uses `min_batch_size=32` in `SituationDispatchParallel` — tune for your core count if bars look uneven.
- At 64×64 without the extra per-cell work, both modes would be too fast to compare; the demo deliberately burns CPU per cell.
