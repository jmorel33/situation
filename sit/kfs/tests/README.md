# KFS tests

Test sources and fixtures live here. The harness design and phased checklist are in
[`../doc/test_harness_plan.md`](../doc/test_harness_plan.md).

## Running

From `sit/kfs/tests/` (harness Makefile):

```bat
mingw32-make build
mingw32-make test          REM 49 tests (correctness + perf)
mingw32-make test-correctness   REM 38 only (fast)
```

From package root: `test_kfs.bat`. From `sit/kfs/kfs/`: `mingw32-make test` (delegates here).

Or run the binary directly:

```bat
..\build\kfs_test.exe --list
..\build\kfs_test.exe
..\build\kfs_test.exe --verbose
```

Default output is **grouped by phase/module** (H0–H7) with per-module pass counts. Library INFO/ERROR trace is suppressed during each test for readability; use `--verbose` when debugging a failure.

**Performance (H7)** runs as part of default `make test` (after correctness). Captured CRT vs MyBuddy metrics: [`../doc/PERFORMANCE.md`](../doc/PERFORMANCE.md). `test-perf` syncs props automatically. For perf only:

```bat
mingw32-make test-perf
kfs_test.exe --perf --perf-tier L
kfs_test.exe --perf --json
```

## Layout

| File | Role |
|------|------|
| `Makefile` | `make build`, `make test`, `make clean` |
| `kfs_test_main.c` | CLI entry (`--list`, `--module`, `--test`) |
| `kfs_test_registry.c` | Test case table |
| `kfs_test_fixture.c` | Temp DB trio, `kfs_test_bootstrap_admin()` (guide §7.1) |
| `kfs_test_assert.h` | `KFS_TEST_OK`, `KFS_TEST_EQ_INT`, etc. |
| `test_lifecycle.c` | H1: init/close, bootstrap, `kfs_create_god_user` |
| `test_actors.c` | H2: users, groups, membership, deactivate, lookup |
| `test_domains.c` | H3: domain CRUD, firewall, `DomainActors` |
| `test_security_schemes.c` | H4: scheme CRUD, grants, wrong-domain |
| `test_content.c` | H5: artifacts, assets, topics, epics, notes |
| `test_permissions.c` | H6: owner/scheme/admin/topic permission matrix |
| `test_perf.c` | H7: perf baselines on vendored harness props |
| `kfs_test_props.c` | Prop resolve + corpus seed (`fixtures/props/`) |
| `kfs_test_perf.c` | QPC timing, p50/p95 reporting |
| `fixtures/props/` | Blobs synced from `tests/harness/assets/` |
| `fixtures/scripts/sync_harness_props.ps1` | `make sync-props` |

Registered modules: `harness` (4), `lifecycle` (5), `actors` (6), `domains` (5), `security_schemes` (5), `content` (7), `permissions` (6), `perf` (11) — **49 tests** total (38 correctness + 11 perf). Temp databases live under `tests/tmp/` and are removed after each test.

```bat
kfs_test.exe --module lifecycle
kfs_test.exe --module actors
kfs_test.exe --list
```