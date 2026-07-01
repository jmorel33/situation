# KFS Test Harness Plan

**Status:** 🟢 **H6 complete** — 35 correctness tests green; **H7–H8 planned** (perf baselines + optional Situation integration)
**Date:** 2026-06-30  
**Scope:** Tiered KFS test harness (MyBuddy / K-Term style): correctness (H0–H6), optional perf baselines (H7), optional Situation integration (H8).  
**Non-scope:** Situation GPU harness (`tests/harness/`) for SPIR-V/render/audio — KFS integration tests live under `sit/kfs/tests/` until §9 loaders warrant `tests/harness/test_kfs.c`.  
**Primary paths:** `sit/kfs/tests/`, `sit/kfs/build/libkfs.a`, `sit/kfs/doc/kfs_guide.md`  
**Related:** [COMPILATION_GUIDE.md](COMPILATION_GUIDE.md), [done/refactor_plan.md](done/refactor_plan.md), [kfs_guide.md](kfs_guide.md), [AAA_ARCHITECTURE_PLAN.md](../../../doc/plan/AAA_ARCHITECTURE_PLAN.md) §9

---

## How to use this file

- Execute phases **H0 → H6** in order for correctness; **H7–H8** are optional tiers (see §1.1).
- Mark `[x]` only when the test is implemented **and** the relevant target passes locally (`make test` for H0–H6; `make test-perf` / `make test-integration` for H7–H8).
- Every test must use an **isolated temp DB trio** (`artifacts.db`, `architecture.db`, `registry.db`) — never the developer's live data.
- **H0–H6:** assert on **return codes and observable DB state**, not merely "no crash".
- **H7:** assert on **latency/throughput thresholds** (soft regression gates), not correctness semantics.
- **H8:** assert on **integration contracts** (Situation error codes, frame-phase budgets, thread safety).
- Update the **§1 progress snapshot** when a phase completes.

**Default command (target):**

```bat
cd sit\kfs\kfs
mingw32-make test
```

Build first with `mingw32-make build` (or `build_kfs.bat` from `sit/kfs/`).

---

## 1. Progress snapshot

| Phase | Status | Notes |
|-------|--------|-------|
| **H0** Harness foundation | ✅ done | Runner, registry, fixtures, `make test-kfs` |
| **H1** Bootstrap & lifecycle | ✅ done | `lifecycle.*` (5 tests), fixture bootstrap, impl fixes |
| **H2** Actors & groups | ✅ done | `actors.*` (6 tests), group membership + deactivate |
| **H3** Domains & firewall | ✅ done | `domains.*` (5 tests), domain firewall + admin delete |
| **H4** Security schemes | ✅ done | `security_schemes.*` (5 tests), grants + wrong-domain |
| **H5** Content model | ✅ done | `content.*` (7 tests), artifact/topic/epic/note paths |
| **H6** Permissions matrix | ✅ done | `permissions.*` (6 tests), owner/scheme/admin/topic gate |
| **H7** Performance baselines | 📋 planned | `perf.*`, Situation harness **props** vendored under `fixtures/props/`, `--perf` / `make test-perf` |
| **H8** Situation integration | 📋 planned | `integration.*`, links built Situation after AAA §9.1a; worker thread §13.3 (H8.6–H8.9) |

**Coverage target (H0–H6):** ≥1 positive + ≥1 negative test per permission-sensitive API family (~40 core scenarios before exhaustive expansion).

### 1.1 Three-tier architecture (collaborative proposal)

Keep correctness, performance, and Situation integration **separate targets** — do not fold Situation into default `make test`.

| Tier | Phase | What | When to run | Depends on |
|------|-------|------|-------------|------------|
| **1 — Correctness** | H0–H6 | Return codes, DB state, permissions | Every `make test` / CI | `libkfs.a` only |
| **2 — Perf baselines** | H7 | Latency p50/p95, ops/sec, blob I/O on **real harness props** + synthetic scale | Nightly, `--perf`, `make test-perf` | `libkfs.a` + vendored props (files copied from Situation harness; no Situation link) |
| **3 — Integration** | H8 | `SituationKFSOpen`, loaders, frame metrics, worker thread | After AAA §9 gates; manual / integration CI | `libsituation.a`, `SITUATION_ENABLE_KFS` |

**Why not merge tiers:** default CI must stay fast and headless (&lt;1s for 35 tests). Situation link time, window init, and GPU harness noise fight that goal. Situation adds value for **correlating KFS work with frame phases** and **proving off-thread I/O** — not for permission-matrix regression.

**Situation assets to reuse (H8 only):**

| Asset | Location | Harness use |
|-------|----------|-------------|
| Frame profile API | `tests/harness/test_frame_profile.c` | KFS load during real frames; `SituationGetFrameProfile` budgets |
| Thread pool / jobs | `tests/harness/test_threading.c` | Model for KFS I/O worker (`SituationCreateThreadPool`, `SituationSubmitJobEx`) |
| Tracy zones | `sit/situation_profiling.h` | Optional zones around hot `kfs_*` paths when `SITUATION_ENABLE_TRACY` |
| Trace IDs | `sit/situation_base_trace.h` | KFS already registered (10140001–10140121); avoid new harness-only IDs |

---

## 2. Why this plan exists

KFS today has:

| Check | What it proves | Gap |
|-------|----------------|-----|
| `make api-check` | Headers compile without impl | No runtime behavior |
| `make test` (smoke) | `libkfs.a` links | No `kfs_init`, no SQL |
| Manual use | Ad hoc | Not CI-repeatable |

The security model in `kfs_guide.md` is rich (domain firewall, schemes, admin bypass). Without a harness, API/impl drift and permission regressions are invisible until integration.

**Goal:** A self-contained harness under `sit/kfs/tests/` that boots temp databases, runs named modules, and reports pass/fail/skip like K-Term.

---

## 3. Target layout

```
sit/kfs/tests/
├── Makefile                     # make build | make test
├── README.md
├── kfs_test_main.c              # argc/argv entry, summary line
├── kfs_test_registry.c          # module table + filter (--module, --test)
├── kfs_test_fixture.h           # temp dir, kfs_init helper, bootstrap actors
├── kfs_test_assert.h            # KFS_TEST_EQ, KFS_TEST_OK macros
├── test_lifecycle.c             # H1
├── test_actors.c                # H2
├── test_domains.c               # H3
├── test_security_schemes.c      # H4
├── test_content.c               # H5
├── test_permissions.c           # H6
├── test_perf.c                  # H7 (optional module; gated by --perf)
├── kfs_test_perf.h              # H7 timing helpers (QPC / clock_gettime)
├── kfs_test_props.h             # H7 prop path resolve + corpus seed helpers
├── test_integration.c           # H8.1–H8.2 (optional; links Situation)
├── test_integration_thread.c    # H8.3 (optional; concurrency after worker API)
├── perf_baselines.json          # H7 golden thresholds (committed or CI artifact)
└── fixtures/
    ├── props/                   # H7 vendored blobs (synced from tests/harness/assets/)
    │   ├── PROPS_MANIFEST.json  # source path, size, KFS type, topic/epic mapping
    │   ├── textures/            # prairie.jpg, rosewood_veneer1.png, thoc.jpg, …
    │   ├── models/              # BoomBox.glb, stanford-bunny.obj, utah_teapot.obj, teapot.stl
    │   ├── audio/               # sample.wav
    │   ├── shaders/             # demon_hunt_sky.vs, demon_hunt_sky.fs
    │   └── fonts/               # Roboto-Regular.ttf (single file; not full static/ tree)
    └── scripts/
        └── sync_harness_props.ps1  # copy curated list from Situation harness → fixtures/props/
```

Build outputs:

| Binary | Makefile target | Role |
|--------|-----------------|------|
| `build/kfs_test.exe` | `make test` | H0–H6 correctness (default) |
| same binary, `--perf` | `make test-perf` | H7 perf module only |
| `build/kfs_test_integration.exe` | `make test-integration` | H8 (separate link line; Situation + KFS) |

`kfs/Makefile` delegates `make test` → `../tests`. H7 reuses `kfs_test.exe`; H8 is a second executable to avoid pulling Situation into every dev build.

---

## 4. Harness design

### 4.1 Fixture helper (`kfs_test_fixture.h`)

Each test function receives or creates:

```c
typedef struct KFS_TestCtx {
    GameDB* db;
    char tmp_dir[MAX_PATH];
    uint64_t admin_uuid;
    int admin_id;
    int admin_group_id;
    int domain_id;
} KFS_TestCtx;
```

- `kfs_test_ctx_create()` — `mkdtemp` / `GetTempPath` + unique subdir, `kfs_init` on three paths beneath it.
- `kfs_test_bootstrap_admin(ctx)` — implements `kfs_guide.md` §7.1 steps 1–5 (AdminGroup, god user, domain).
- `kfs_test_ctx_destroy()` — `kfs_close`, delete temp files.

### 4.2 Registry pattern

Mirror K-Term / Situation harness:

```c
typedef struct KFS_TestCase {
    const char* module;
    const char* name;
    int (*fn)(KFS_TestCtx* ctx);
} KFS_TestCase;
```

- Modules (correctness): `lifecycle`, `actors`, `domains`, `security_schemes`, `content`, `permissions`.
- Modules (optional): `perf` (H7), `integration` (H8 — separate binary or `--integration` flag).
- CLI: `kfs_test [--module X] [--test Y] [--list] [--verbose] [--perf]`.
- Exit code: 0 all pass, 1 any fail, 2 harness error.
- **Quiet by default (H0+):** stdout/stderr redirected during each test; `--verbose` restores KFS `[INFO]`/`[ERROR]` trace.

### 4.3 Assertion macros

```c
#define KFS_TEST_OK(rc)       /* rc == KFS_OK */
#define KFS_TEST_DENIED(rc)   /* rc == KFS_PERMISSION_DENIED */
#define KFS_TEST_NOTFOUND(rc) /* rc == KFS_NOTFOUND */
```

Log format: `[PASS] module.name` / `[FAIL] module.name: reason`.

### 4.4 Relationship to smoke

| Target | Role |
|--------|------|
| `make test` (in `kfs/`) | Link-only smoke (keeps §5.7 fast) |
| `make test` (in `tests/`) | Full correctness harness H0–H6 |
| `make test-kfs` | Alias for correctness harness |
| `make test-perf` | H7 perf module (`kfs_test --perf`) |
| `make test-integration` | H8 binary (requires built Situation + `SITUATION_ENABLE_KFS`) |
| `make test-all` | Smoke + correctness (default CI); perf/integration opt-in |

---

## 5. Phase H0 — Harness foundation

- [x] **H0.1** Add `tests/kfs_test_assert.h` with `KFS_TEST_*` macros.
- [x] **H0.2** Add `tests/kfs_test_fixture.h` + `kfs_test_fixture.c` (temp DB paths, create/destroy).
- [x] **H0.3** Add `tests/kfs_test_main.c` (parse args, run cases, print summary).
- [x] **H0.4** Add `tests/kfs_test_registry.c` with one stub test `harness.ping` → `KFS_OK`.
- [x] **H0.5** Makefile target `test-kfs` → `build/kfs_test.exe`.
- [x] **H0.6** `make test-kfs` passes stub from clean checkout (Windows + note Linux path).
- [x] **H0.7** Document harness CLI in [COMPILATION_GUIDE.md](COMPILATION_GUIDE.md) §Tests.

**Exit criteria:** `kfs_test --list` shows modules; `kfs_test` exits 0.

---

## 6. Phase H1 — Bootstrap & lifecycle

- [x] **H1.1** `lifecycle.init_close` — `kfs_init` + `kfs_close`, files exist on disk.
- [x] **H1.2** `lifecycle.init_null_paths` — invalid paths → error, no partial open.
- [x] **H1.3** `lifecycle.double_close` — second `kfs_close` safe or documented error.
- [x] **H1.4** `lifecycle.bootstrap_admin` — fixture matches guide §7.1; admin can `kfs_add_domain`.
- [x] **H1.5** `lifecycle.create_god_user` — `kfs_create_god_user` path (if distinct from manual bootstrap).

**Exit criteria:** H1 module green; temp dirs cleaned even on failure.

---

## 7. Phase H2 — Actors & groups

- [x] **H2.1** `actors.add_user` — `kfs_add_actor` USER; UUID and ID populated.
- [x] **H2.2** `actors.add_group` — GROUP actor; `kfs_add_member_to_group` round-trip.
- [x] **H2.3** `actors.is_member_of` — positive and negative membership.
- [x] **H2.4** `actors.deactivate_blocks_action` — inactive actor denied write.
- [x] **H2.5** `actors.get_by_name` — `kfs_get_actor_by_name` finds seeded user.
- [x] **H2.6** `actors.remove_member` — `kfs_remove_member_from_group` updates membership.

---

## 8. Phase H3 — Domains & firewall

- [x] **H3.1** `domains.add_and_list` — `kfs_add_domain`, `kfs_list_domains` (or equivalent).
- [x] **H3.2** `domains.add_actor_to_domain` — member can see domain-scoped entity.
- [x] **H3.3** `domains.firewall_deny` — non-member `kfs_get_artifact` → `KFS_PERMISSION_DENIED`.
- [x] **H3.4** `domains.update_metadata` — rename/description with admin requester.
- [x] **H3.5** `domains.delete_requires_admin` — non-admin denied.

---

## 9. Phase H4 — Security schemes

- [x] **H4.1** `security_schemes.create` — scheme bound to `domain_id`.
- [x] **H4.2** `security_schemes.add_actor_grant` — read/write/delete flags stored.
- [x] **H4.3** `security_schemes.wrong_domain` — scheme from domain A not applied in domain B.
- [x] **H4.4** `security_schemes.free_contents` — `kfs_security_scheme_free_contents` no leak (valgrind optional).
- [x] **H4.5** `security_schemes.delete` — owner/admin can delete; stranger denied.

---

## 10. Phase H5 — Content model

- [x] **H5.1** `content.create_artifact` — `kfs_create_artifact` + `kfs_get_artifact` round-trip.
- [x] **H5.2** `content.link_asset` — blob/text asset attach and load.
- [x] **H5.3** `content.topic_assign` — `kfs_assign_topic_to_artifact_by_name` with domain_id.
- [x] **H5.4** `content.epic_topic_link` — epic ↔ topic association.
- [x] **H5.5** `content.note_assign` — note create + assign to artifact.
- [x] **H5.6** `content.delete_artifact` — cascade expectations per guide.
- [x] **H5.7** `content.legacy_save_text` — smoke path for `kfs_save_text` (documented bypass behavior).

---

## 11. Phase H6 — Permissions matrix

Implement scenarios from `kfs_guide.md` §5 worked examples:

- [x] **H6.1** Owner-only artifact — owner read OK, stranger denied.
- [x] **H6.2** Admin bypass — AdminGroup member reads without scheme.
- [x] **H6.3** Scheme group grant — group member read via `SchemeAllowedActors`.
- [x] **H6.4** Scheme without grant — domain member still denied on artifact.
- [x] **H6.5** Write vs delete separation — `can_write` without `can_delete`.
- [x] **H6.6** Topic-level read gate — `kfs_load_by_topic` respects topic permission.

**Exit criteria (harness v1):** 35 tests green; grouped module output; quiet default + `--verbose`.

---

## 12. Phase H7 — Performance baselines (props + synthetic scale)

**Today:** no performance coverage exists — H6 only asserts correctness. H7 is the first perf layer.

**Runtime dependency:** `libkfs.a` only (no Situation init, no GPU). **Data dependency:** vendored copies of curated files from `tests/harness/assets/` (see §12.1). Same blobs Situation uses for model/audio/texture tests (`test_model_loader.c`, `test_obj_loader.c`, `test_audio.c`, etc.) — so KFS numbers are comparable to real loader workloads when H8 lands.

### 12.0 Design goals

| Goal | How |
|------|-----|
| Realistic blob I/O | Copy Situation harness props into `fixtures/props/` by size tier |
| Regression detection | `perf_baselines.json` p50/p95 ceilings; fail if &gt;2× baseline |
| Fast default CI | `make test` unchanged; props + perf gated behind `--perf` |
| H8 bridge | Prop names/topics match future `SituationLoadTextureFromKFS` / `LoadModelFromKFS` scenarios |
| Scale stress | Synthetic micro-blobs (§12.3) complement real props for catalog-size tests |

### 12.1 Situation harness props (vendored corpus)

**Source of truth:** `tests/harness/assets/` (+ `sit_test_assets.h` resolution rules).  
**KFS copy:** `sit/kfs/tests/fixtures/props/` — committed to the KFS tree (or populated by sync script before first perf run).

**Sync workflow:**

```bat
cd sit\kfs\tests
powershell -File fixtures\scripts\sync_harness_props.ps1
REM or: mingw32-make sync-props
```

Script copies a **curated manifest** (not the whole assets tree). Idempotent; logs SHA-256 + byte size into `PROPS_MANIFEST.json`.

#### 12.1.1 Curated prop set (v1)

| Tier | File (harness source) | ~Size | KFS type | Topic | Situation consumer |
|------|----------------------|-------|----------|-------|-------------------|
| **S** | `prairie.jpg` | 340 KB | `image` | `perf_textures` | `test_misc`, virtual_display |
| **S** | `thoc.jpg` | 366 KB | `image` | `perf_textures` | misc / photo tests |
| **M** | `rosewood_veneer1.png` | 2.1 MB | `image` | `perf_textures` | `test_obj_loader`, projection_3d |
| **M** | `stanford-bunny.obj` | 2.3 MB | `model` | `perf_models` | `test_obj_loader` |
| **M** | `utah_teapot.obj` | 2.5 MB | `model` | `perf_models` | geometry tests |
| **L** | `teapot.stl` | 4.3 MB | `model` | `perf_models` | `test_stl_loader` |
| **L** | `BoomBox.glb` | 10 MB | `model` | `perf_models` | `test_model_loader` |
| **L** | `sample.wav` | 10.5 MB | `audio` | `perf_audio` | `test_audio` |
| **S** | `demon_hunt_sky.vs` | small | `shader` | `perf_shaders` | graphics / SPIR-V harness |
| **S** | `demon_hunt_sky.fs` | small | `shader` | `perf_shaders` | graphics harness |
| **M** | `Roboto-Regular.ttf` | 477 KB | `font` | `perf_fonts` | `test_text_rendering` |

**Epic layout** (seeded into temp DB by `kfs_test_seed_props_corpus`):

| Epic | Topics | Members |
|------|--------|---------|
| `perf_graphics` | `perf_textures`, `perf_shaders` | prairie, thoc, rosewood, demon_hunt sky VS/FS |
| `perf_geometry` | `perf_models` | bunny, teapot OBJ/STL, BoomBox |
| `perf_media` | `perf_audio`, `perf_fonts` | sample.wav, Roboto |

**Excluded from v1 copy** (documented in `PROPS_MANIFEST.json` as optional XL):

| Asset | ~Size | Reason |
|-------|-------|--------|
| `*.blend.zip` (4K packs) | 60–80 MB each | Repo bloat; opt-in via `KFS_PERF_XL_PATH` env → read directly from harness path |
| `prairie.mp4`, `sample.mp4` | 7–8 MB | Video not a KFS v1 target; defer to H8 |
| `spirv_out/*.spv`, `demon_hunt_sky.*.spv` | varies | GPU bytecode; H8 loader tests use source `.vs`/`.fs` blobs |
| `static/Roboto_*` (54 files) | — | One `Roboto-Regular.ttf` suffices for font blob perf |
| `rosewood_veneer1.webp` | 408 KB | PNG is the canonical harness texture |

#### 12.1.2 `PROPS_MANIFEST.json` schema

```json
{
  "version": 1,
  "source_root": "tests/harness/assets/",
  "props": [
    {
      "id": "boombox_glb",
      "file": "models/BoomBox.glb",
      "source": "BoomBox.glb",
      "bytes": 10610176,
      "sha256": "...",
      "kfs_type": "model",
      "topic": "perf_models",
      "epic": "perf_geometry",
      "tier": "L"
    }
  ]
}
```

Harness loads manifest at runtime; missing required prop → `[SKIP] perf.* (run make sync-props)` not a silent pass.

### 12.2 Prop resolution & seed helpers (`kfs_test_props.h`)

Mirror `sit_test_assets.h` without `SituationFileExists` — use `fopen` / `_access`:

```c
/* Search order (first hit wins) */
"fixtures/props/<subdir>/file"
"../fixtures/props/..."
"../../tests/harness/assets/file"   /* dev fallback: read from Situation tree in-place */
```

- [ ] **H7.0a** `kfs_test_resolve_prop(id, out_path)` — manifest lookup + prefix scan.
- [ ] **H7.0b** `kfs_test_read_prop_bytes(id, &buf, &len)` — load blob for ingest benchmarks.
- [ ] **H7.0c** `kfs_test_seed_props_corpus(ctx, flags)` — ingest manifest into temp DB: artifacts + topics + epics + scheme (admin read-all). Flags: `KFS_SEED_PROPS_ONLY`, `KFS_SEED_PROPS_PLUS_SYNTHETIC`.
- [ ] **H7.0d** `fixtures/scripts/sync_harness_props.ps1` + `make sync-props` — copy curated files, rewrite manifest checksums.

### 12.3 Timing infrastructure (`kfs_test_perf.h`)

- [ ] **H7.1** `KFS_TEST_PERF_BEGIN` / `KFS_TEST_PERF_END` — nanosecond accumulator (QPC on Windows).
- [ ] **H7.2** Warmup iterations (discard) + measured iterations (configurable via `--perf-iters N`).
- [ ] **H7.3** Report: **p50, p95, p99, mean, ops/sec, bytes/sec** (for blob tests).
- [ ] **H7.4** `perf_baselines.json` — per-test ceilings; machine tag optional (`"host": "ci-win11"`).
- [ ] **H7.5** Synthetic scale helper — generate N × 4 KB pseudo-random blobs in-memory (no files) for catalog-size tests without copying XL assets.

Defaults: warmup 100, measure 1000 (micro); warmup 5, measure 50 (L-tier blob ingest).

### 12.4 Perf test matrix

#### 12.4.1 Micro — SQL / permission path (synthetic corpus)

Uses `KFS_SEED_PROPS_PLUS_SYNTHETIC`: props corpus + 500 small generated blobs, 50 actors, 10 schemes.

- [ ] **H7.10** `perf.permission_check` — `kfs_check_permission` p50/p95 over 10k iterations (scheme + domain firewall).
- [ ] **H7.11** `perf.get_artifact_meta` — `kfs_get_artifact` metadata-only on mixed corpus (no blob read).
- [ ] **H7.12** `perf.list_artifacts` — `kfs_list_artifacts` cursor over full catalog.

#### 12.4.2 Blob I/O — real harness props (by tier)

Each test: seed prop once, then time hot loop.

- [ ] **H7.20** `perf.blob_read_small` — `kfs_get_asset_data` on `prairie.jpg` (S, ~340 KB).
- [ ] **H7.21** `perf.blob_read_medium` — `kfs_get_asset_data` on `rosewood_veneer1.png` (M, ~2 MB).
- [ ] **H7.22** `perf.blob_read_large_glb` — `kfs_get_asset_data` on `BoomBox.glb` (L, ~10 MB).
- [ ] **H7.23** `perf.blob_read_large_wav` — `kfs_get_asset_data` on `sample.wav` (L, ~10 MB).
- [ ] **H7.24** `perf.blob_ingest_large` — cold ingest: `kfs_create_artifact` + link from file for BoomBox.glb (measure insert + first read).
- [ ] **H7.25** `perf.blob_roundtrip_checksum` — ingest → read → SHA-256 compare (correctness guard inside perf module).

#### 12.4.3 Semantic load paths (topic / epic)

- [ ] **H7.30** `perf.load_by_topic_textures` — `kfs_load_by_topic("perf_textures")` (3 images).
- [ ] **H7.31** `perf.load_by_topic_models` — `kfs_load_by_topic("perf_models")` (4 geometry files).
- [ ] **H7.32** `perf.load_by_epic_geometry` — `kfs_load_by_epic("perf_geometry")` full dispatch.
- [ ] **H7.33** `perf.load_by_epic_graphics` — epic spanning textures + shaders (mixed small/medium).

#### 12.4.4 Throughput & database scale

- [ ] **H7.40** `perf.bulk_ingest_all_props` — ingest entire v1 manifest sequentially; report total bytes/sec.
- [ ] **H7.41** `perf.catalog_db_size` — `artifacts.db` byte size after full seed (regression on schema bloat).
- [ ] **H7.42** `perf.concurrent_permission_read` — single-thread baseline before H8.9 (documents current ceiling).

#### 12.4.5 Reporting

- [ ] **H7.50** `perf.report_json` — `--perf --json` machine-readable summary for nightly CI.
- [ ] **H7.51** `perf.report_table` — human table: test, tier, bytes, p50, p95, baseline, pass/fail.

### 12.5 Makefile / CLI

- [ ] **H7.60** `make sync-props` — run `sync_harness_props.ps1`; verify manifest checksums.
- [ ] **H7.61** `make test-perf` — `kfs_test --perf` (perf module only); **excluded** from default `make test`.
- [ ] **H7.62** `kfs_test --perf [--perf-iters N] [--perf-tier S|M|L|all] [--json]` — filter by prop tier.
- [ ] **H7.63** Document props + perf CLI in `tests/README.md` and [COMPILATION_GUIDE.md](COMPILATION_GUIDE.md).

**Assertions:** soft thresholds only (regression detection). Never mix perf noise into H0–H6 pass/fail counts. Checksum test H7.25 is the only hard correctness assert inside perf.

**Optional later:**

- Tracy zones in impl hot paths (`kfs_impl_auth.h` permission, `kfs_impl_lc.h` load) when `SITUATION_ENABLE_TRACY` — overlay on H7.20–H7.24.
- XL stress: `KFS_PERF_XL_PATH=tests/harness/assets` + ingest `aerial_rocks_02_4k.blend.zip` (manual/nightly only).

---

## 13. Phase H8 — Situation integration & threading

**Prerequisites (AAA §9 — must land before H8 compiles):**

| Gate | AAA item | Why |
|------|----------|-----|
| SQLite in Situation build | §9.2 | KFS links `sqlite3` |
| `SituationKFSOpen` / `SituationKFSClose` | §9.1a | Harness must not touch raw `GameDB*` |
| `SITUATION_ENABLE_KFS` compile flag | §9.3 | Opt-in; stubs when off |
| KFS error code mapping | §9.4 | Assert `SITUATION_ERROR_KFS_*` not raw `KFS_*` |

### 13.1 Integration smoke (single-threaded)

- [ ] **H8.1** `integration.kfs_open_close` — `SituationKFSOpen` on temp trio, `SituationKFSClose`, files on disk.
- [ ] **H8.2** `integration.load_texture_from_kfs` — `SituationLoadTextureFromKFS` on `rosewood_veneer1.png` / `prairie.jpg` from H7 props corpus (§12.1).
- [ ] **H8.3** `integration.load_model_from_kfs` — `SituationLoadModelFromKFS` on `BoomBox.glb` (§9.1c); same blob H7.22 measured.
- [ ] **H8.4** `integration.load_by_topic_dispatch` — `SituationLoadAssetsByTopic("perf_models")` (§9.1e); count matches H7.31 artifact set.

### 13.2 Frame-phase correlation

- [ ] **H8.5** `integration.frame_profile_budget` — load `BoomBox.glb` mid-frame; `SituationGetFrameProfile` execute_ns under H7.22-derived ceiling (pattern from `test_frame_profile.c`).
- [ ] **H8.6** `integration.no_main_thread_stall` — KFS read during `SituationAcquireFrameCommandBuffer` … `SituationEndFrame`; frame_time_ms spike count bounded.

### 13.3 KFS worker thread (concurrency)

**Current constraint:** KFS is single-threaded — one `GameDB*` owns three `sqlite3*` handles with no mutex across impl fragments. Do **not** call `kfs_*` from `SituationSubmitJobEx` workers until ownership is defined.

**Recommended model (A — dedicated KFS thread + command queue):**

```
Main / render thread                KFS I/O thread (owns GameDB*)
        |                                    |
        |-- SituationKFSQueueLoad(req) ----->| drain queue
        |<-- completion / fence -------------|
```

- [ ] **H8.7** Design doc in [done/refactor_plan.md](done/refactor_plan.md) or AAA §9 — queue API, lifetime, shutdown order.
- [ ] **H8.8** `SituationKFSQueue*` / `SituationKFSWait` (internal or `SITAPI`) — single consumer thread.
- [ ] **H8.9** `integration_thread.concurrent_submit` — 4 client threads × 100 `BoomBox.glb` load requests; no corruption, deterministic ordering per request id.
- [ ] **H8.10** `integration_thread.shutdown_clean` — in-flight jobs complete or cancel documented; no sqlite use-after-close.

**Rejected for v1:** global mutex around all `kfs_*` (serializes bulk load); per-thread SQLite connections (three linked DBs + permission state).

### 13.4 Build layout

- [ ] **H8.11** `tests/Makefile` target `test-integration` — link `libsituation.a`, `-DSITUATION_ENABLE_KFS`, include `sit/situation_api.h`.
- [ ] **H8.12** Optional relocation: once §9 loaders are stable, add `tests/harness/test_kfs.c` mirroring other Situation modules (keep correctness in `sit/kfs/tests/`; reuse `fixtures/props/` paths).

**Exit criteria (harness v2):** H7 perf green on dev machine with synced props; H8.1–H8.4 green with `-DSITUATION_ENABLE_KFS` build; H8.9 green after worker API lands.

---

## 14. Makefile integration (H0.5 detail)

Add to `sit/kfs/kfs/Makefile`:

```makefile
TEST_SRCS := $(wildcard $(KFS_PKG_DIR)/tests/*.c)
# filter out per-module files once split; link kfs_test_main + registry + fixture + modules
test-kfs: $(BIN_DIR)/kfs_test$(EXE_EXT)
test-all: test test-kfs
```

Link: `-L$(BUILD_DIR) -lkfs`, include `-I$(KFS_PKG_DIR)/tests` for headers.

---

## 15. CI / future

- [ ] Wire `make test` (correctness) into a repo-level script (post–§5.7 green) — **H7/H8 not required for merge gate**.
- [ ] Nightly job: `make sync-props` → `make test-perf` → compare against `perf_baselines.json`.
- [ ] Verify `fixtures/props/PROPS_MANIFEST.json` checksums when Situation harness assets change (`tests/harness/assets/HARNESS_ASSETS.txt` is the upstream catalog).
- [ ] Integration job (manual or post–§9): `make test-integration` with `SITUATION_ENABLE_KFS` build.
- [ ] Optional: AddressSanitizer build (`CFLAGS=-fsanitize=address`) on fixture teardown.
- [ ] Optional: `tests/fuzz/` for SQL injection / malformed metadata JSON (low priority).
- [ ] Tracy: add zones in `kfs_impl_auth.h` / `kfs_impl_lc.h` hot paths only when profiling regressions; do not register harness-only symbols in `situation_base_trace.h` (avoid ID churn).

---

## 16. Success criteria

### Harness v1 (H0–H6) — ✅ met

- [x] `make test-kfs` runs ≥30 tests across H1–H6 with 0 failures.
- [x] Grouped module output (H0–H6 phases); quiet default; `--verbose` / `--list`.
- [ ] Every permission test asserts a specific `KFS_*` code, not generic `!= KFS_OK`.
- [ ] No test writes outside its temp fixture directory.
- [x] [done/refactor_plan.md](done/refactor_plan.md) §5.7 extended: smoke **and** harness green.
- [ ] [COMPILATION_GUIDE.md](COMPILATION_GUIDE.md) documents `test-kfs` / `test-all`.

### Harness v2 (H7–H8) — target

- [ ] `make sync-props` copies curated Situation harness assets; `PROPS_MANIFEST.json` checksums match.
- [ ] `make test-perf` runs H7 module (≥15 perf cases §12.4); JSON/table output; excluded from default `make test`.
- [ ] `perf_baselines.json` checked in after first green run on reference machine.
- [ ] H7.22 + H7.24 establish baseline for `BoomBox.glb`; H7.23 for `sample.wav`.
- [ ] `make test-integration` builds only when Situation + `SITUATION_ENABLE_KFS` available.
- [ ] H8.1–H8.4 pass: open/close + texture + model + topic dispatch using H7 props.
- [ ] H8.9 pass: concurrent queue submits with dedicated KFS thread (after H8.7–H8.8).

---

## 17. Maintenance

When adding a new public `kfs_*` API:

1. Add declaration via `scripts/sync_api.py` if signature changed.
2. Add at least one harness test in the appropriate module (or mark intentional gap in this plan).
3. Run `make test` (correctness) before merging KFS changes.
4. If Situation harness assets change, run `make sync-props` and refresh `PROPS_MANIFEST.json`.
5. If touching hot paths (`kfs_check_permission`, blob I/O), run `make test-perf` and update baselines if intentional.
6. If touching AAA §9 integration or worker queue, run `make test-integration`.