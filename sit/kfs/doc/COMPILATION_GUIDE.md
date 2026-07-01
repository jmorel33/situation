# KFS — Compilation Guide

**Package:** `sit/kfs/`  
**Date:** 2026-06-30

This guide covers building, linking, and integrating the Kaizen Filing System (KFS) static library outside the main Situation DLL build.

---

## Package layout

```
sit/kfs/
├── kfs/                       # Source only
│   ├── kfs.h                  # Public orchestrator
│   ├── kfs_version.h          # Version macros (bump only here)
│   ├── kfs_mem.h              # Unified heap API + KFS_* macros
│   ├── kfs_api.h              # Public types and declarations
│   ├── kfs_impl.h             # Impl orchestrator (include chain only)
│   ├── kfs_impl_fwd.h         # Cross-module static forwards
│   ├── kfs_impl_core.h        # Platform, lifecycle, utilities, frees
│   ├── kfs_impl_auth.h        # registry.db — actors, domains, permission
│   ├── kfs_impl_lc.h          # architecture + artifacts — content & linking
│   ├── kfs.c                  # KFS_IMPLEMENTATION translation unit
│   └── Makefile               # Build driver
├── build/                     # Build inputs + generated artifacts
│   ├── link_check_main.c      # §5.7 link verification (compile+link libkfs.a)
│   ├── api_only.c             # API header compile check
│   ├── build_kfs.bat          # Windows: make build
│   ├── test_kfs.bat           # Windows: make test
│   ├── libkfs.a               # Static library (created by make, gitignored)
│   ├── kfs_test.exe           # Harness binary (gitignored)
│   └── obj/                   # Object files (gitignored)
├── scripts/
│   ├── sync_api.py            # Regenerate kfs_api.h from impl signatures
│   ├── check_kfs_mem.py       # Enforced allocator audit (make mem-check)
│   ├── s3_extract_core.py     # Split replay scripts (reference)
│   ├── s4_extract_auth.py
│   └── s5_extract_lc.py
├── tests/                     # Harness — 55 tests (H0–H7)
├── doc/                       # Documentation (this folder)
├── build_kfs.bat              # Wrapper → build/build_kfs.bat
├── test_kfs.bat               # Wrapper → build/test_kfs.bat
└── lib_kfs.h                  # Deprecated shim
```

---

## Prerequisites

| Tool | Notes |
|------|-------|
| **gcc** | C11 (`-std=c11`) |
| **ar** | Creates `libkfs.a` |
| **python 3** | For `make sync-api` only |
| **SQLite amalgamation** | `ext/sqlite3.c` + `ext/sqlite3.h` at repo root |

**Windows:** use `mingw32-make` if `make` is not on PATH (`C:\msys64\mingw64\bin`).

---

## Quick start

Two commands — **build** compiles, **test** runs the harness:

```bat
cd sit\kfs
build_kfs.bat
test_kfs.bat
```

Or from the Makefile directories:

```bat
cd sit\kfs\kfs
mingw32-make build

cd sit\kfs\tests
mingw32-make test
```

`kfs/Makefile` builds the library and delegates harness compile to `tests/Makefile`.  
`build` produces `build/libkfs.a`, `build/kfs_link_check.exe` (§5.7), and `build/kfs_test.exe`.  
`test` runs `kfs_test.exe` (**55** cases: H0–H6 correctness + H7 perf).

---

## Memory init (embedders)

Call **`kfs_mem_init()` before any KFS or SQLite use** if your process calls `sqlite3_*` outside `kfs_init`. `kfs_init` invokes `kfs_mem_init(NULL)` when skipped.

```c
#include "kfs/kfs.h"

kfs_mem_config_t mem = { .hard_limit_bytes = 0, .max_open_db = 0, .track_stats = 1 };
kfs_mem_init(&mem);   /* or NULL for defaults */

GameDB* db = NULL;
kfs_init(&db, "artifacts.db", "architecture.db", "registry.db");
/* ... */
kfs_close(db);
kfs_mem_shutdown();   /* only when all GameDB handles are closed */
```

Optional caps (see `kfs_mem.h`): `hard_limit_bytes` + `kfs_mem_set_hard_limit_bytes`, `max_open_db` + `kfs_mem_set_max_open_db`. API-returned pointers → `kfs_mem_free()`.

Build runs `scripts/check_kfs_mem.py` via `make mem-check` (also a dependency of `make lib` / `make build`).

### Optional MyBuddy backend (M7 — production profile C)

Header-only MyBuddy is instantiated in `kfs.c` when `-DKFS_MEM_USE_MYBUDDY` is set. Produces `build/libkfs_mybuddy.a` with **profile C** (`BUDDY_LARGE` + 256 MiB pool). Routes **KFS + SQLite heap** through `mbd_*`. Requires **pthread**; on Windows add **`-lbcrypt`**.

```bat
cd sit\kfs\kfs
mingw32-make lib-mybuddy
mingw32-make mybuddy-smoke
cd ..\tests
mingw32-make test-mybuddy         REM H0 on MyBuddy
mingw32-make test-mybuddy-perf-c  REM H7 perf (profile C)
```

Default `make build` / `make test` remain CRT-backed (**55/55**).

### Tested performance (H7)

Full metrics: **[PERFORMANCE.md](PERFORMANCE.md)**. Summary (2026-06-30, `--perf-iters 100`):

| Test | CRT p95 | Profile C p95 |
|------|---------|---------------|
| `blob_read_small` | 1.58 ms | 1.14 ms |
| `blob_read_large_glb` | 17.7 ms | 13.9 ms |
| `load_by_topic_models` | 36.9 ms | 21.7 ms |
| `load_by_epic_geometry` | 49.5 ms | 26.8 ms |
| `bulk_ingest_all_props` (mean) | 375 ms | 365 ms |

Migration context: [memory_alloc_plan.md](memory_alloc_plan.md) §10.

---

## Make targets

Run from `sit/kfs/kfs/`:

| Target | Output / action |
|--------|-----------------|
| `make build` | `mem-check` + compile lib, link-check, api-check, harness binary |
| `make test` | Run API harness (delegates to `../tests/Makefile`) |
| `make test-build` | Build harness only (delegates to `../tests/Makefile`) |
| `make mem-check` | Run `scripts/check_kfs_mem.py` (fails on raw CRT in impl headers) |
| `make lib` | `mem-check` + static library |
| `make lib-mybuddy` | `mem-check` + `libkfs_mybuddy.a` (MyBuddy backend) |
| `make mybuddy-smoke` | Build + run MyBuddy smoke binary |
| `make link-check` | Build + run §5.7 link verification |
| `make api-check` | Compile `build/api_only.c` with `-Werror` |
| `make sync-api` | Run `scripts/sync_api.py` |
| `make clean` | Remove `build/obj` and generated `libkfs.a` / binaries |

Deprecated aliases (still work): `make smoke` → `link-check`, `make test-kfs` → `test`, `make test-all` → `build` + `test`.  
Deprecated bats: `build_kfs_smoke.bat`, `build/build_smoke.bat` → forward to `build_kfs.bat`.

---

## Include contract

Add **`-I sit/kfs`** (or `-I path/to/sit/kfs`) to compile flags.

```c
// Exactly one translation unit:
#define KFS_IMPLEMENTATION
#include "kfs/kfs.h"

// All other translation units:
#include "kfs/kfs.h"
```

**Alternate** (only `-I sit`): `#include "kfs/kfs/kfs.h"`

**Deprecated:** `#include "kfs/lib_kfs.h"` (forwards to `kfs/kfs/kfs.h`).

---

## Linking `libkfs.a`

After `make lib`:

```
sit/kfs/build/libkfs.a
```

Example link line (adjust paths):

```bat
gcc my_app.c -I sit -Isit/kfs -L sit/kfs/build -lkfs -o my_app.exe
```

`libkfs.a` already contains the SQLite amalgamation object — do **not** link `sqlite3` again unless you split the archive.

---

## API maintenance

Public `kfs_*` function **bodies** live in `kfs_impl_core.h`, `kfs_impl_auth.h`, and `kfs_impl_lc.h`. `kfs_api.h` is generated from impl signatures.

After editing a public function in any impl fragment:

```bat
cd sit\kfs\kfs
mingw32-make sync-api
mingw32-make build
cd ..\tests
mingw32-make test
```

**Note:** `sync_api.py` still reads/writes `kfs_impl.h` (orchestrator) for `extract_decls` until **S7** teaches it to concatenate core → auth → lc. Until then, `make sync-api` remains valid because the orchestrator has no bodies — maintain signatures in the fragment files and verify with `git diff kfs_api.h` after sync.

`sync_api.py` strips inline `//` comments before parsing signatures so `kfs_api.h` stays valid. Cross-module `static` helpers belong in `kfs_impl_fwd.h` (not re-injected into the orchestrator).

---

## Verification checklist (§5.7)

1. `make build` — `mem-check` + lib + link-check + api-check + harness binary compile  
2. `make test` — harness runs with exit 0 (**55** tests: H0–H7)

### Test harness (Phase H0+)

From `sit/kfs/tests/`:

```bat
mingw32-make build
mingw32-make test
```

| Target | Action |
|--------|--------|
| `make build` | Compile `kfs_test` (builds `libkfs.a` via `../kfs` if needed) |
| `make test` | Build + run harness |
| `make clean` | Remove harness objects and `kfs_test` binary |

Module sources matching `test_*.c` are picked up automatically (`test_lifecycle.c`, `test_actors.c`, …).

Binary: `build/kfs_test.exe` (links `libkfs.a`; does **not** define `KFS_IMPLEMENTATION`).

```bat
kfs_test.exe              REM run all tests (grouped H0–H6, quiet library trace)
kfs_test.exe --list       REM print tests grouped by module
kfs_test.exe --verbose    REM show KFS INFO/ERROR + mem in_use/peak/sqlite per test
kfs_test.exe --module harness
kfs_test.exe --test ping
kfs_test.exe --help
```

Exit codes: `0` all pass, `1` test failure, `2` harness error (bad args, setup failure, no matches).

Each test gets an isolated temp directory under `tests/tmp/` (three SQLite files), created and removed by the fixture helper.

See [test_harness_plan.md](test_harness_plan.md) for harness modules (H0–H7). Impl split history: [done/impl_split_plan.md](done/impl_split_plan.md).

---

## Situation integration (planned)

KFS is not yet wired into `build_situation.bat`. See [done/refactor_plan.md](done/refactor_plan.md) and [`../../../doc/plan/AAA_ARCHITECTURE_PLAN.md`](../../../doc/plan/AAA_ARCHITECTURE_PLAN.md) §9 for the integration roadmap (`SITUATION_ENABLE_KFS`, `SituationKFSOpen`).

---

## Related docs

| Document | Description |
|----------|-------------|
| [architecture.md](architecture.md) | Databases, permission engine, memory §9, impl layout |
| [memory_alloc_plan.md](memory_alloc_plan.md) | Unified heap migration (complete) |
| [test_harness_plan.md](test_harness_plan.md) | Harness design, phases H0–H6, fixtures |
| [kfs_guide.md](kfs_guide.md) | Security model, permissions, bootstrap |
| [done/refactor_plan.md](done/refactor_plan.md) | `lib_kfs.h` refactor + impl split (archived) |
| [done/impl_split_plan.md](done/impl_split_plan.md) | Monolith → fwd/core/auth/lc playbook (archived) |