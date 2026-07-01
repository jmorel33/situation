# KFS unified memory allocation plan

**Status:** 🟢 **Production-ready (M0–M8 + M7)** — unified heap; MyBuddy profile C validated; **KFS 2.3.0** (2026-06-30)  
**Goal:** Route **all** KFS heap traffic and **all** SQLite heap traffic through one controllable allocator backend. **Achieved.**  
**Gate:** CRT default `make build && make test` → **55/55**; MyBuddy `make lib-mybuddy` + `make test-mybuddy-perf-c` → **11/11** H7.

---

## 1. Problem statement

Today KFS runs on **two independent heaps**:

| Heap | Who uses it | Examples |
|------|-------------|----------|
| **CRT** (`malloc` / `free` / `strdup`) | KFS implementation (~109 alloc sites in auth/lc/core) | `GameDB` handle, entity strings, blob copies, temp arrays |
| **SQLite heap** (now via our vtable when `kfs_mem_init` ran) | Page cache, VDBE, temp B-trees, `sqlite3_exec` errors | Everything inside the three DB files |

There is no single place to enforce a budget, report combined usage, swap backends (MyBuddy), or catch mixed-allocator bugs before heap corruption.

**Prior art:** Situation `SIT_MALLOC`, MyBuddy `mbd_*`, K-Term `KTERM_FREE` → `SIT_FREE`. MyBuddy consistency plan lists `sit/kfs/` as out of scope — this plan closes that gap.

---

## 2. Target architecture

```
kfs_mem.h (KFS_* macros + public API)
    → kfs_impl_core.h (backend + stats)
        → KFS impl (core/auth/lc) via KFS_MALLOC / KFS_FREE / …
        → sqlite3_mem_methods (xMalloc, xFree, xRealloc, xSize, xRoundup, xInit, xShutdown)
        → optional: SIT_MALLOC / mbd_alloc alias (M7)
```

### 2.1 SQLite contract (must hold)

| Callback | Requirement | Notes |
|----------|-------------|-------|
| `xRoundup` | 8-byte `ROUND8` style; return `0` → alloc fails | Matches SQLite `mem0.c` |
| `xMalloc` | Like `malloc`; receives **rounded** size when `MEMSTATUS=1` | We store size in header cookie |
| `xRealloc` | Second arg is always prior `xRoundup` result (`R-46199-30249`) | Assert-rounded in SQLite core |
| `xSize` | Return size of prior `xMalloc`/`xRealloc` pointer | Cookie read |
| `xFree` | Like `free`; null-safe at our layer | |
| `xInit` / `xShutdown` | Paired; called around SQLite mem lifecycle | Minimal flag today |
| Alignment | Returned pointers 8-byte aligned (`R-11148-40995`) | Depends on underlying `malloc` |

**Install:** `sqlite3_config(SQLITE_CONFIG_MALLOC, …)` + `SQLITE_CONFIG_MEMSTATUS, 1` **before** first `sqlite3_initialize()` / `sqlite3_open`.

### 2.2 Memory SQLite does NOT route through `mem_methods`

Document and accept (or optionally configure later):

| Path | Under our control? |
|------|-------------------|
| `sqlite3_malloc` / `sqlite3_free` / internal heap | **Yes** (via vtable) |
| **Lookaside** slots (per-connection fast pool) | Bulk buffer via `sqlite3Malloc`; slot recycle **bypasses** `xFree` |
| **`mmap` I/O** (`PRAGMA mmap_size`) | **No** — OS mapping |
| `SQLITE_CONFIG_PAGECACHE` static pool | **No** unless we configure it (optional M6+) |
| Temp journal files on disk | **No** — file I/O |

“Control all memory” means **all heap allocations we can intercept** plus explicit policy for the exceptions above.

---

## 3. Pre-commit decisions — **LOCKED (2026-06-30)**

- [x] **D1 — Large-block policy:** **L1 unified** — all sizes through one `KFS_*` / vtable backend.
- [x] **D2 — Version policy:** **2.3.0 at M8 sign-off** when migration + docs complete (interim header may read 2.3.0 during M1–M7 work).
- [x] **D3 — Heap cap default:** `hard_limit_bytes = 0` (**unlimited**); embedders opt in via `kfs_mem_config_t`.
- [x] **D4 — Bootstrap order:** **`kfs_mem_init()` first**, before any KFS or SQLite use; `kfs_init()` calls it defensively if skipped.
- [x] **D5 — MEMSTATUS:** **`SQLITE_CONFIG_MEMSTATUS=1` required** — non-negotiable; init fails if unset.
- [x] **D6 — Open DB cap:** `max_open_db = 0` (**unlimited** concurrent `GameDB` handles); embedders set `kfs_mem_config_t.max_open_db > 0` to enforce.
- [x] **D7 — No Situation allocator dependency (2026-06-30):** KFS stays **standalone** — do **not** wire `SIT_MALLOC` / Situation headers. Situation/MyBuddy audio heap issues are unresolved; KFS must not inherit that dependency. **Future:** optional **MyBuddy** backend (`mbd_alloc` / `mbd_free`) via `KFS_*` macro override or `kfs_mem_config_t` hook — direct MyBuddy API only, no `situation_api_config.h` required.

| Field | Default | Meaning |
|-------|---------|---------|
| `hard_limit_bytes` | `0` | No heap budget |
| `max_open_db` | `0` | No cap on open `kfs_init` without matching `kfs_close` |
| `track_stats` | `1` | Peak / in-use counters on |
| Allocator | L1 | Single backend for KFS + SQLite heap |

---

## 4. Actionable checklist

### M0 — Baseline

- [x] Count `malloc`/`calloc`/`realloc`/`strdup`/`free` sites per fragment (core 2/46, auth 50/57, lc 57/102)
- [x] Capture H7 perf p50/p95 baseline log → `doc/done/mem_alloc_baseline_perf.log`
- [x] Gate: harness green before memory work (was 47/47; now 52/52 after M1 tests)

### M1 — Scaffold (SQLite vtable + `kfs_mem.h`)

**Code**

- [x] Add `kfs/kfs_mem.h` — macros + `kfs_mem_*` public API
- [x] Include `kfs_mem.h` from `kfs_api.h`
- [x] Implement backend in `kfs_impl_core.h` (header cookie, stats, limits hook)
- [x] Implement full `sqlite3_mem_methods`: `xMalloc`, `xFree`, `xRealloc`, `xSize`, `xRoundup`, `xInit`, `xShutdown`
- [x] `kfs_mem_init()` — `SQLITE_CONFIG_MALLOC` + `SQLITE_CONFIG_MEMSTATUS`
- [x] `kfs_init()` calls `kfs_mem_init(NULL)` idempotently
- [x] `GameDB` handle → `KFS_MALLOC` / `KFS_FREE`; open-db refcount for shutdown guard
- [x] Add `kfs_mem.h` to `kfs/Makefile` + `tests/Makefile` `KFS_HDR`
- [x] Version bump **2.3.0** + description in `kfs_version.h`
- [x] Add `scripts/check_kfs_mem.py` (grep gate) — stub in M1; **enforced in M5**
- [x] `get_current_timestamp()` → `KFS_MALLOC(21)`; all `free(timestamp)` / `free(created_at)` from timestamp → `kfs_mem_free` (M2)

**Harness (H0)**

- [x] `mem_init_idempotent` — double `kfs_mem_init` returns `KFS_OK`
- [x] `mem_stats_after_init` — `kfs_mem_bytes_in_use() > 0` and `kfs_sqlite_bytes_in_use() > 0` with open DB
- [x] `mem_roundtrip` — alloc → realloc grow/shrink → free; sizes consistent with cookie
- [x] `mem_roundup_matches_sqlite` — `xRoundup(n)` equals `ROUND8(n)` for sample sizes (1, 5, 8, 100, 65536)

**Harness (H1)**

- [x] `integrity_custom_alloc` — `PRAGMA integrity_check` passes on bootstrapped DB under custom allocator

**Gate**

- [x] `mingw32-make build` OK
- [x] **52/52** harness (41 correctness + 11 perf)

### M2 — Core migration (`kfs_impl_core.h`)

- [x] `get_current_timestamp()` → `KFS_MALLOC(21)`; all `free(timestamp)` / `free(created_at)` from timestamp in auth/lc → `kfs_mem_free`
- [ ] Migrate remaining raw `malloc`/`free` in `kfs_impl_core.h` entity free helpers (`kfs_*_free`) — **deferred to M3/M4** (paired with alloc sites; partial free-only migration causes heap corruption)
- [ ] Audit `kfs_entity_free` / `kfs_user_info_free` pointer ownership — every free pairs with `KFS_*` alloc
- [x] `exec_sql` — keep `sqlite3_free(errMsg)` (SQLite-owned; routes through our vtable)
- [x] Gate: **52/52**

### M3 — Auth migration (`kfs_impl_auth.h`)

- [x] Mechanical replace: `malloc` → `KFS_MALLOC`, `calloc` → `KFS_CALLOC`, `realloc` → `KFS_REALLOC`, `strdup` → `KFS_STRDUP`, `free` → `kfs_mem_free` (KFS-owned pointers only)
- [x] Review error-path cleanup in growable temp arrays (`realloc` failure branches — `kfs_list_scheme_actors`, `kfs_list_domains`)
- [x] Grep verify: zero raw allocators left in `kfs_impl_auth.h` (except comments/SQL strings)
- [x] `kfs_security_scheme_free_contents` in core uses `kfs_mem_free` (auth scheme data)
- [x] Harness: `kfs_list_domains` outputs freed with `kfs_mem_free` (`test_lifecycle.c`, `test_domains.c`)
- [x] Gate: **52/52**

### M4 — LC migration (`kfs_impl_lc.h`)

- [x] Same mechanical replace as M3 (~130 alloc/free sites in `kfs_impl_lc.h`)
- [x] Extra review: blob paths (`KFS_MALLOC(file_size)`, `KFS_MALLOC(asset_data_size)`) — L1 unified (D1)
- [ ] Extra review: misleading-indentation `for`+`free` lines (pre-existing; deferred)
- [x] Grep verify: zero raw allocators left in `kfs_impl_lc.h` (comments only)
- [x] Core entity free helpers (`kfs_note/asset/topic/epic_*`) → `kfs_mem_free`
- [x] Realloc failure cleanup: `kfs_list_epics`, `kfs_list_topics`, `kfs_list_notes`, `kfs_list_artifacts`
- [x] Harness: LC API outputs freed with `kfs_mem_free` (content/domains/permissions/perf/props tests)
- [x] Gate: **52/52**; H7 perf tests pass (p95 vs M0 baseline: informal compare; `kfs_test_perf_check_baseline` still stub)

### M5 — Enforce single heap (CI gate)

- [x] `scripts/check_kfs_mem.py` fails on raw `malloc`/`calloc`/`realloc`/`strdup`/`free` in `kfs_impl_*.h` (allowlist: comments, `sqlite3_free`, `COMMIT` SQL, `KFS_MEM_CRT_BACKEND` region in core)
- [x] Wire `check_kfs_mem.py` into `kfs/Makefile` `mem-check` + `build` / `lib` targets
- [x] Document macro override rules in `kfs_mem.h` comment block
- [x] Gate: **52/52** + script clean

### M6 — Policy & limits

- [x] `hard_limit_bytes` enforced on alloc/realloc; runtime setter `kfs_mem_set_hard_limit_bytes`
- [x] H0 `mem_limit_triggers_oom` — limit < alloc/realloc grow → `NULL` + OOM callback
- [x] H0 `mem_reset_peak` — peak tracks alloc; reset aligns peak to in_use
- [x] `kfs_mem_reset_peak()` documented; harness `--verbose` prints in_use/peak/sqlite
- [x] `sqlite3_soft_heap_limit64` synced when `hard_limit_bytes > 0` (init + setter)
- [x] H0 `open_db_cap` — `max_open_db=1` → second `kfs_init` returns `KFS_MISUSE` (D6)
- [x] Lookaside / cache notes in `kfs_mem.h` (§2.2 exceptions; optional `SQLITE_CONFIG_LOOKASIDE` / `PRAGMA cache_size` tuning deferred)
- [x] Gate: **55/55** + new H0 tests

### M7 — MyBuddy backend (optional — **no SIT_MALLOC**, per D7)

- [x] `KFS_MEM_USE_MYBUDDY` — backend island + **full SQLite vtable** → `mbd_alloc` / `mbd_free` / `mbd_realloc` (`MYBUDDY_IMPLEMENTATION` in `kfs.c`; **no** Situation tree)
- [x] **Production profile C** (bench winner): `MBD_FLAG_BUDDY_LARGE` + `pool_size = 256 MiB` — default for all `libkfs_mybuddy.a` builds
- [x] `libkfs_mybuddy.a`, `make mybuddy-smoke`, `make test-mybuddy` (H0), `make test-mybuddy-perf-c` (H7)
- [x] `MYBUDDY_ALLOCATOR_CONSISTENCY_PLAN.md` — KFS **MyBuddy-ready** standalone
- [x] H7 A/B perf study (2026-06-30, `--perf-iters 100`) — logs in `doc/done/mem_alloc_*_perf*.log`
- [x] Gate: **55/55** CRT default; MyBuddy smoke + H0 + H7 on profile C

### M8 — Documentation & sign-off

- [x] `doc/architecture.md` — §9 Memory subsystem (vtable, lookaside/mmap caveats, init order)
- [x] `doc/COMPILATION_GUIDE.md` — `kfs_mem_init` before any SQLite; `mem-check` target; caps
- [x] `doc/kfs_guide.md` — memory init contract for embedders (§7 + bootstrap)
- [x] `kfs.h` header comment — bootstrap order
- [x] Plan **Status: 🟢 complete** (this file)
- [x] Version **2.3.0** per D2 (no 2.4.0 — behavior shipped incrementally M1–M6)
- [x] Gate: **55/55** + `mem-check` clean

---

## 5. Mechanical migration rules

```
malloc(     → KFS_MALLOC(
calloc(     → KFS_CALLOC(
realloc(    → KFS_REALLOC(
strdup(     → KFS_STRDUP(
free(       → KFS_FREE(        /* KFS-owned pointers only */
```

**Keep as-is:** `sqlite3_free()`, `sqlite3_errmsg()` (never free), SQL `"COMMIT;"`, comments.

**Never partial-migrate a function** — alloc and all exit-path frees in the same PR hunk (timestamp lesson).

---

## 6. Risks & mitigations

| Risk | Severity | Mitigation | Tracked in |
|------|----------|------------|------------|
| `sqlite3_config` after first open | High | `kfs_mem_init` at `kfs_init` entry; document external users | M1, M8 |
| Mixed allocator (`free` on `KFS_MALLOC`) | High | M5 script; migrate per-function | M2–M4 |
| Wrong `xSize`/`xRoundup` | High | H0 roundtrip + H1 `integrity_check` | M1 |
| Lookaside not in `xFree` path | Low | Document §2.2; optional lookaside config M6 | M6, M8 |
| `mmap` outside vtable | Low | Document §2.2 | M8 |
| `MEMSTATUS` disabled | Medium | D5 — required | Pre-commit |
| H7 perf regression | Medium | M0 baseline log; 10% gate | M0, M4 |
| Shutdown with open `GameDB` | Medium | `open_db_count` guard (done) | M1 |
| Macro override vs vtable mismatch | Medium | Document in M5/M8 | M5 |

---

## 7. Files touched

| File | Phase |
|------|-------|
| `kfs/kfs_mem.h` | M1 ✅ |
| `kfs/kfs_api.h` | M1 ✅ |
| `kfs/kfs_impl_core.h` | M1 ✅, M2 |
| `kfs/kfs_impl_auth.h` | M3 ✅ |
| `kfs/kfs_impl_lc.h` | M4 ✅ |
| `kfs/kfs.h` | M8 ✅ |
| `kfs/kfs_version.h` | M1 ✅, M8 ✅ (2.3.0) |
| `scripts/check_kfs_mem.py` | M1 stub, M5 enforce |
| `tests/kfs_test_registry.c` | M1 ✅, M1+, M6 |
| `doc/architecture.md`, `COMPILATION_GUIDE.md`, `kfs_guide.md` | M8 ✅ |
| `doc/done/mem_alloc_baseline_perf.log` | M0 |
| `doc/done/mem_alloc_crt_perf_m7.log` | M7 CRT H7 |
| `doc/done/mem_alloc_mybuddy_perf_m7.log` | M7 MyBuddy default H7 |
| `doc/done/mem_alloc_mybuddy_profile_c_perf.log` | M7 profile C H7 |

---

## 8. Success criteria (final sign-off)

- [x] Zero raw CRT allocators on KFS-owned pointers in `kfs_impl_*.h` (M5 script; CRT backend island only)
- [x] Full SQLite `mem_methods` vtable installed before any `sqlite3_open` (`kfs_mem_init` → `kfs_init`)
- [x] `kfs_mem_bytes_in_use()` + `kfs_sqlite_bytes_in_use()` accurate with open DB (H0 `mem_stats_after_init`)
- [x] H0 memory tests: idempotent init, stats, roundtrip, roundup parity, OOM limit, reset_peak, open_db_cap
- [x] H0 `integrity_custom_alloc` under custom allocator (three DBs `PRAGMA integrity_check`)
- [x] **55/55** harness on CRT backend (44 correctness + 11 perf)
- [x] H7 p95 within 10% of M0 baseline (informal compare vs `doc/done/mem_alloc_baseline_perf.log`; automated gate still stub)
- [x] Optional MyBuddy backend buildable without mixed heap (M7 profile C; **no** `SIT_MALLOC` — D7)
- [x] MyBuddy H7 validated — reads faster than CRT; ingest matches/beats CRT with profile C (§10)
- [x] Docs list lookaside / mmap / pagecache exceptions (`architecture.md` §9.5, `kfs_mem.h`)

---

## 9. Production readiness sign-off (2026-06-30)

**KFS 2.3.0 is production-ready** for standalone embedding:

| Layer | Evidence |
|-------|----------|
| Unified heap | Single `kfs_mem` + SQLite `mem_methods`; auth/lc/core on `KFS_*`; build-enforced (`mem-check`) |
| Correctness | **55/55** harness (H0–H7), `integrity_custom_alloc`, policy caps (M6) |
| CRT backend | Default `libkfs.a` — zero regressions vs M0 informal perf gate |
| MyBuddy backend | Optional `libkfs_mybuddy.a` — profile C; smoke + H0 + H7 green; **same SQLite vtable path** |
| Standalone | No `SIT_MALLOC` / Situation dependency (D7) |
| Docs | `architecture.md` §9, `COMPILATION_GUIDE.md`, `kfs_guide.md`, this plan |

**MyBuddy is production-trustworthy for KFS** when linked via `KFS_MEM_USE_MYBUDDY`: one process heap for KFS impl + SQLite, hardened allocator (v1.6.x), and measured wins on real KFS workloads (not microbench only).

Remaining non-blockers (documented, not failures): lookaside/mmap outside vtable; automated H7 10% CI gate still stub; Situation DLL integration not wired yet.

---

## 10. M7 performance study — MyBuddy profile C (2026-06-30)

**Canonical metrics (core doc):** [PERFORMANCE.md](PERFORMANCE.md) — full CRT / profile C / M0 tables, throughput, reproduce steps.

**Setup:** Windows 10, MSYS2 mingw64, H7 `--perf-iters 100`, props synced. Three backends:

| Backend | Library | MyBuddy config |
|---------|---------|----------------|
| CRT (classic) | `libkfs.a` | N/A |
| MyBuddy default | *(bench only)* | `mbd_init(NULL)` — 128 MiB, no BUDDY_LARGE |
| **MyBuddy profile C** | `libkfs_mybuddy.a` | `MBD_FLAG_BUDDY_LARGE` + **256 MiB** pool |

**Logs:** `doc/done/mem_alloc_crt_perf_m7.log`, `mem_alloc_mybuddy_perf_m7.log`, `mem_alloc_mybuddy_profile_c_perf.log`

### Profile C vs CRT (p95 / mean — improvement = faster)

| Test | CRT p95 | Profile C p95 | vs CRT | CRT mean | C mean | vs CRT |
|------|---------|---------------|--------|----------|--------|--------|
| blob_read_small | 1.58 ms | 1.14 ms | **−28%** | 1.12 ms | 0.75 ms | **−33%** |
| blob_read_medium | 4.29 ms | 3.28 ms | **−24%** | 2.69 ms | 2.33 ms | **−13%** |
| blob_read_large_glb | 17.7 ms | 13.9 ms | **−21%** | 12.6 ms | 9.28 ms | **−26%** |
| blob_read_large_wav | 18.0 ms | 13.9 ms | **−23%** | 13.6 ms | 9.94 ms | **−27%** |
| load_by_topic_textures | 7.22 ms | 5.03 ms | **−30%** | 5.00 ms | 3.67 ms | **−27%** |
| load_by_topic_models | 36.9 ms | 21.7 ms | **−41%** | 27.0 ms | 18.3 ms | **−32%** |
| load_by_epic_geometry | 49.5 ms | 26.8 ms | **−46%** | 30.6 ms | 19.3 ms | **−37%** |
| blob_ingest_large | — | — | — | 58.3 ms | 56.6 ms | **−3%** |
| bulk_ingest_all_props | — | — | — | 375 ms | 365 ms | **−3%** |

**Typical read workload: ~25–40% faster than CRT** (p95/mean). Ingest: parity or slight win (default MyBuddy was ~10% slower on ingest; profile C fixes that).

### Profile C vs MyBuddy default (`mbd_init(NULL)`)

Profile C wins **balanced KFS**: **−11%** ingest/bulk mean, **−9% to −18%** topic-load p95; trades some raw GLB p95 vs default MyBuddy (~14 ms vs ~10 ms) — acceptable for full-app tuning.

**Decision (locked):** ship **profile C** as the only MyBuddy init path in `kfs_mybuddy_backend_init()`.

---

## 11. Suggested PR stack

| PR | Scope |
|----|--------|
| PR-1 | M0 baseline log + M1 remainder (roundtrip tests, `check_kfs_mem.py` stub) |
| PR-2 | M2 core migration |
| PR-3 | M3 auth migration |
| PR-4 | M4 lc migration + perf compare |
| PR-5 | M5 enforce + M6 policy tests |
| PR-6 | M7 MyBuddy hook (standalone; no Situation dep) |
| PR-7 | M8 docs + plan sign-off |

---

*Related: [architecture.md](architecture.md), [impl_split_plan.md](done/impl_split_plan.md), Situation `situation_api_config.h`, MyBuddy `MYBUDDY_ALLOCATOR_CONSISTENCY_PLAN.md`.*