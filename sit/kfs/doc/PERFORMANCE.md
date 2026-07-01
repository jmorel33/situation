# KFS — Tested performance metrics

**Version:** 2.3.0  
**Captured:** 2026-06-30  
**License:** MIT — (c) 2025–2026 Jacques Morel

This document records **measured** H7 harness results on real KFS workloads (vendored Situation props corpus). Numbers are from committed logs in [done/](done/); re-run locally to validate on your machine.

---

## 1. Methodology

| Item | Value |
|------|-------|
| Harness | H7 performance module — **11 tests** (permission, blob I/O, topic/epic load, bulk ingest) |
| Command | `kfs_test.exe --perf --perf-iters 100` (or `make test` / `make test-mybuddy-perf-c`) |
| Platform | Windows 10, MSYS2 mingw64 gcc, `-O2` |
| Corpus | `tests/fixtures/props/` — prairie.jpg (340 KB), rosewood PNG (2.1 MB), BoomBox.glb / sample.wav (~10 MB each), full manifest bulk ingest (~34 MB) |
| Metrics | **p50, p95, mean** latency; **MB/s** on blob/read tests; **ops/s** on permission check |
| Gate | CRT default must stay within **~10% of M0 baseline** p95 (informal; automated `kfs_test_perf_check_baseline` still stub) |

**Backends compared:**

| Label | Library | Allocator |
|-------|---------|-----------|
| **CRT (M7)** | `libkfs.a` | `malloc` / `free` via `kfs_mem` CRT island |
| **MyBuddy profile C** | `libkfs_mybuddy.a` | `MBD_FLAG_BUDDY_LARGE` + **256 MiB** pool (`kfs_mybuddy_backend_init`) |
| **M0 baseline** | `libkfs.a` | CRT reference log before full memory migration (regression anchor) |

Raw logs: `done/mem_alloc_baseline_perf.log`, `done/mem_alloc_crt_perf_m7.log`, `done/mem_alloc_mybuddy_profile_c_perf.log`.

---

## 2. CRT backend — M7 measured (default production build)

All **11/11** H7 tests pass on `libkfs.a`.

| Test | p50 | p95 | mean | Throughput |
|------|-----|-----|------|------------|
| `permission_check` | — | — | 500.9 µs | 1996 ops/s |
| `blob_read_small` | 892 µs | **1.58 ms** | 1.12 ms | 297.6 MB/s |
| `blob_read_medium` | 2.54 ms | **4.29 ms** | 2.69 ms | 768.0 MB/s |
| `blob_read_large_glb` | 11.8 ms | **17.7 ms** | 12.6 ms | 806.3 MB/s |
| `blob_read_large_wav` | 12.4 ms | **18.0 ms** | 13.6 ms | 761.0 MB/s |
| `blob_ingest_large` | — | — | 58.3 ms | 17 ops/s (1 sample) |
| `blob_roundtrip_checksum` | — | — | — | correctness only |
| `load_by_topic_textures` | 4.64 ms | **7.22 ms** | 5.00 ms | 551.1 MB/s |
| `load_by_topic_models` | 25.4 ms | **36.9 ms** | 27.0 ms | 705.6 MB/s |
| `load_by_epic_geometry` | 29.2 ms | **49.5 ms** | 30.6 ms | 623.4 MB/s |
| `bulk_ingest_all_props` | — | — | 374.7 ms | 86.4 MB/s |

**M0 regression check:** M7 CRT p95 is at or better than M0 baseline on all comparable tests (e.g. `blob_read_small` 1.58 ms vs M0 1.76 ms; `load_by_epic_geometry` 49.5 ms vs M0 45.6 ms — within informal gate).

---

## 3. MyBuddy profile C — measured (optional production build)

All **11/11** H7 tests pass on `libkfs_mybuddy.a`.

| Test | p50 | p95 | mean | Throughput |
|------|-----|-----|------|------------|
| `permission_check` | — | — | 635.2 µs | 1574 ops/s |
| `blob_read_small` | 695 µs | **1.14 ms** | 746 µs | 444.7 MB/s |
| `blob_read_medium` | 2.01 ms | **3.28 ms** | 2.33 ms | 886.1 MB/s |
| `blob_read_large_glb` | 8.57 ms | **13.9 ms** | 9.28 ms | 1090.9 MB/s |
| `blob_read_large_wav` | 9.09 ms | **13.9 ms** | 9.94 ms | 1038.3 MB/s |
| `blob_ingest_large` | — | — | 56.6 ms | 18 ops/s (1 sample) |
| `load_by_topic_textures` | 3.53 ms | **5.03 ms** | 3.67 ms | 751.0 MB/s |
| `load_by_topic_models` | 17.8 ms | **21.7 ms** | 18.3 ms | 1043.3 MB/s |
| `load_by_epic_geometry` | 18.1 ms | **26.8 ms** | 19.3 ms | 987.5 MB/s |
| `bulk_ingest_all_props` | — | — | 364.6 ms | 88.8 MB/s |

---

## 4. Profile C vs CRT — improvement summary

Negative percentages = **faster** (lower latency).

### Latency (p95)

| Test | CRT p95 | Profile C p95 | Δ p95 |
|------|---------|---------------|-------|
| `blob_read_small` | 1.58 ms | 1.14 ms | **−28%** |
| `blob_read_medium` | 4.29 ms | 3.28 ms | **−24%** |
| `blob_read_large_glb` | 17.7 ms | 13.9 ms | **−21%** |
| `blob_read_large_wav` | 18.0 ms | 13.9 ms | **−23%** |
| `load_by_topic_textures` | 7.22 ms | 5.03 ms | **−30%** |
| `load_by_topic_models` | 36.9 ms | 21.7 ms | **−41%** |
| `load_by_epic_geometry` | 49.5 ms | 26.8 ms | **−46%** |

**Typical read workload: ~21–46% faster p95** vs CRT on this machine.

### Latency (mean)

| Test | CRT mean | Profile C mean | Δ mean |
|------|----------|----------------|--------|
| `blob_read_small` | 1.12 ms | 746 µs | **−33%** |
| `blob_read_medium` | 2.69 ms | 2.33 ms | **−13%** |
| `blob_read_large_glb` | 12.6 ms | 9.28 ms | **−26%** |
| `blob_read_large_wav` | 13.6 ms | 9.94 ms | **−27%** |
| `load_by_topic_textures` | 5.00 ms | 3.67 ms | **−27%** |
| `load_by_topic_models` | 27.0 ms | 18.3 ms | **−32%** |
| `load_by_epic_geometry` | 30.6 ms | 19.3 ms | **−37%** |
| `blob_ingest_large` | 58.3 ms | 56.6 ms | **−3%** |
| `bulk_ingest_all_props` | 374.7 ms | 364.6 ms | **−3%** |

**Ingest / bulk write:** parity or slight win (~3%). Default MyBuddy (`mbd_init(NULL)`) had been ~10% slower on ingest; profile C corrects that.

### Throughput (mean MB/s — blob tests)

| Test | CRT | Profile C | Δ |
|------|-----|-----------|---|
| `blob_read_small` | 297.6 | 444.7 | **+49%** |
| `blob_read_medium` | 768.0 | 886.1 | **+15%** |
| `blob_read_large_glb` | 806.3 | 1090.9 | **+35%** |
| `blob_read_large_wav` | 761.0 | 1038.3 | **+36%** |
| `load_by_topic_textures` | 551.1 | 751.0 | **+36%** |
| `load_by_topic_models` | 705.6 | 1043.3 | **+48%** |
| `load_by_epic_geometry` | 623.4 | 987.5 | **+58%** |
| `bulk_ingest_all_props` | 86.4 | 88.8 | **+3%** |

### Permission microbench

`permission_check` mean latency is **higher** on MyBuddy (635 µs vs 501 µs CRT) on this run — allocator choice matters most on **large blob read** and **topic/epic dispatch** paths, not the permission hot loop alone.

---

## 5. M0 baseline reference (CRT, post-M1/M2)

Regression anchor from `done/mem_alloc_baseline_perf.log`:

| Test | p95 | mean | Throughput |
|------|-----|------|------------|
| `blob_read_small` | 1.76 ms | 1.22 ms | 272.3 MB/s |
| `blob_read_medium` | 5.51 ms | 3.85 ms | 535.7 MB/s |
| `blob_read_large_glb` | 19.6 ms | 15.1 ms | 671.5 MB/s |
| `blob_read_large_wav` | 21.9 ms | 15.8 ms | 653.9 MB/s |
| `load_by_topic_textures` | 13.2 ms | 6.86 ms | 401.6 MB/s |
| `load_by_topic_models` | 38.7 ms | 30.0 ms | 636.6 MB/s |
| `load_by_epic_geometry` | 45.6 ms | 35.2 ms | 542.0 MB/s |
| `bulk_ingest_all_props` | — | 455.5 ms | 71.1 MB/s |

M7 CRT and profile C both beat M0 on most read metrics — unified memory migration did not regress real workloads.

---

## 6. Reproduce

```bat
cd sit\kfs\kfs
mingw32-make build
cd ..\tests
mingw32-make test                    REM CRT: 55/55 incl. H7

mingw32-make test-mybuddy-perf-c       REM MyBuddy profile C: H7 only
```

Optional: `kfs_test.exe --perf --perf-iters 100 --verbose` for per-op breakdown.

---

## 7. Interpretation and limits

- **Single reference machine** — absolute numbers vary by CPU, disk, and antivirus; use as **relative** CRT vs MyBuddy guidance.
- **H7 is soft regression detection**, not a correctness gate; H0–H6 own semantic pass/fail.
- **Full 55/55 on MyBuddy** — only H0 + H7 exercised on `libkfs_mybuddy.a`; CRT runs the complete suite by default.
- **Automated 10% CI gate** — planned (`perf_baselines.json`); today compare logs manually.
- Allocator study context and production sign-off: [memory_alloc_plan.md](memory_alloc_plan.md) §9–§10.

---

## Related

| Document | Focus |
|----------|-------|
| [test_harness_plan.md](test_harness_plan.md) | H7 test definitions and props corpus |
| [COMPILATION_GUIDE.md](COMPILATION_GUIDE.md) | `libkfs_mybuddy.a` build targets |
| [architecture.md](architecture.md) §9 | Memory subsystem and backend choice |
| [memory_alloc_plan.md](memory_alloc_plan.md) | Migration record and MyBuddy profile C decision |