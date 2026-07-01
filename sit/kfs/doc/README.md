# KFS (Kaizen Filing System) documentation

**License:** MIT — (c) 2025-2026 Jacques Morel (see [`../LICENSE`](../LICENSE))

| Document | Description |
|----------|-------------|
| [architecture.md](architecture.md) | Internal design — databases, permissions, code layout |
| [COMPILATION_GUIDE.md](COMPILATION_GUIDE.md) | Build, link, include paths, make targets |
| [PERFORMANCE.md](PERFORMANCE.md) | **Tested H7 metrics** — CRT vs MyBuddy profile C (p95, mean, MB/s) |
| [kfs_guide.md](kfs_guide.md) | Security model manual (v2.3) |
| [test_harness_plan.md](test_harness_plan.md) | Test harness — H0–H7 (55 tests); H8 Situation integration planned |
| [clone_plan.md](clone_plan.md) | Clone & lineage — backtrack, registry, change advisory (planned) |
| [memory_alloc_plan.md](memory_alloc_plan.md) | Unified memory — **production-ready**; MyBuddy profile C validated (§9–§10) |

### Completed plans ([done/](done/))

| Document | Description |
|----------|-------------|
| [done/refactor_plan.md](done/refactor_plan.md) | `lib_kfs.h` refactor + impl split (S0–S6) — archived |
| [done/impl_split_plan.md](done/impl_split_plan.md) | Monolith → fwd / core / auth / lc playbook — archived |
| [done/split_baseline_test.log](done/split_baseline_test.log) | S0 harness baseline before impl split |