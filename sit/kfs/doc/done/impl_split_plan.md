# KFS Implementation Split Plan

> **Archived** in `doc/done/` (2026-06-30). Active docs: [../README.md](../README.md).

**Status:** 🟢 **S0–S6 complete** — monolith relocated to `kfs_impl_{fwd,core,auth,lc}.h`; orchestrator **~22 lines**; harness **46/46** (2026-06-30). **S7** (`sync_api.py` multi-fragment) remains; **S8** docs largely done.  
**Date:** 2026-06-30  
**Scope:** Split **~13,910-line** `kfs/kfs_impl.h` into four implementation headers + thin orchestrator. **Zero API / behavior / SQL changes.**
**Safety net:** `sit/kfs/tests/` — run full suite after every sub-phase.  
**Related:** [architecture.md](../architecture.md) §7, [refactor_plan.md](refactor_plan.md), [test_harness_plan.md](../test_harness_plan.md), [COMPILATION_GUIDE.md](../COMPILATION_GUIDE.md)

---

## 0. Executive summary

The split is **sensible and low-risk** if treated as a **mechanical relocation** with hard gates — not a refactor. The old plan failed on brittleness because it relied on approximate line ranges and coarse phases. This revision adds:

1. A **function-level manifest** (148 symbols) — source of truth over banner lines  
2. **Sub-phases** (S3.1–S5.4) small enough to bisect compile failures in minutes  
3. A **harness gate matrix** mapping each sub-phase to the tests that must stay green  
4. Explicit handling of **duplicate banners** (epic, note, advanced-load)  
5. **`sync_api.py` migration** before S7 (today it assumes a single monolith and *writes* patches into it)  
6. **Rollback checkpoints** — one git commit per sub-phase  

**Do not** rename functions, change `static` linkage, edit SQL, or “clean up” `requesting_user_uuid` typos during the split. Those are separate PRs.

---

## 1. Readiness gate (S0 — must pass before S1)

| Check | Command | Expected |
|-------|---------|----------|
| Harness correctness | `cd sit/kfs/tests && mingw32-make test-correctness` | 35/35 pass |
| Harness perf | `cd sit/kfs/tests && mingw32-make test-perf` | 11/11 pass |
| Lib + api-check | `cd sit/kfs/kfs && mingw32-make build` | no errors |
| API sync baseline | `cd sit/kfs/kfs && mingw32-make sync-api` | note `git diff` — should be clean or committed |
| Monolith size | `wc -l kfs/kfs_impl.h` | **13,910** (±10) |
| Public API count | `grep -c '^int kfs_\|^void kfs_' kfs/kfs_api.h` | **~122** declarations |

Capture baseline:

```bat
cd sit\kfs\kfs
mingw32-make build
mingw32-make sync-api
cd ..\tests
mingw32-make test > ..\doc\done\split_baseline_test.log
```

Commit message: `kfs: split baseline — harness 46/46, sync-api clean`.

---

## 2. Goals (unchanged intent, stricter constraints)

| Goal | Constraint |
|------|------------|
| Navigable codebase | Auth (registry) vs LC (architecture + artifacts) |
| Single TU | Still `kfs.c` + `#define KFS_IMPLEMENTATION` — no link-model change |
| Zero behavior drift | Same symbols, same trace IDs, same SQL strings |
| Include DAG | `fwd → core → auth → lc`; only orchestrator chains includes |
| Tooling | `sync_api.py`, Makefiles, docs updated in **S7–S8** |

---

## 3. Target layout

```
sit/kfs/kfs/
├── kfs.h                  # Public orchestrator (unchanged)
├── kfs_api.h              # Public API (unchanged during split)
├── kfs_impl.h             # Impl orchestrator only (~50 lines post-S6)
├── kfs_impl_fwd.h         # Cross-module static forward declarations
├── kfs_impl_core.h        # Platform, DB lifecycle, utilities, memory frees
├── kfs_impl_auth.h        # registry.db — actors, domains, schemes, permission
├── kfs_impl_lc.h          # architecture.db + artifacts.db — content & linking
└── kfs.c                  # #define KFS_IMPLEMENTATION + #include "kfs.h"
```

**LC = Linking & Content** (not lifecycle — that stays in **core**).

---

## 4. Monolith anatomy (13,910 lines)

### 4.1 Banner map (navigation aids — **not** cut boundaries)

| Line | Banner | Typical owner |
|------|--------|---------------|
| 294 | STATIC HELPER FUNCTIONS | **core** (definitions) |
| 1121 | DATABASE INITIALIZATION | **core** |
| 2399 | ACTOR / GROUP MANAGEMENT | **auth** |
| 3936 | SECURITY SCHEME MANAGEMENT | **auth** |
| 5862 | FINAL MISSING FUNCTIONS & WRAP UP | **auth** (owner/scheme setters, scheme users) |
| 6438 | USER MANAGEMENT FUNCTIONS | **auth** (legacy users) |
| 6665 | EPIC MANAGEMENT w/ Permissions | **lc** |
| 7154 | EPIC MANAGEMENT FUNCTIONS | **lc** ⚠️ duplicate region |
| 7716 | TOPIC MANAGEMENT w/ Permissions | **lc** |
| 8760 | NOTE MANAGEMENT FUNCTIONS | **lc** |
| 9223 | NOTE MANAGEMENT w/ Permissions | **lc** ⚠️ duplicate region |
| 9875 | LINKING/ASSIGNMENT w/ Permissions | **lc** |
| 12124 | ASSET MANAGEMENT FUNCTIONS | **lc** |
| 12538 | ADVANCED LOADING FUNCTIONS | **lc** |
| 12841 | ADVANCED LOADING FUNCTIONS | **lc** ⚠️ duplicate region |
| 13248 | OTHER MISC FUNCTIONS | **lc** (validate_script, orphans) |
| 13543 | MEMORY MANAGEMENT (User) | **core** |

### 4.2 Non-contiguous traps (do **not** slice by line alone)

| Issue | Lines | Rule |
|-------|-------|------|
| Platform + forward decls | 12–77 | Move with **core** orchestrator preamble |
| Early FS helpers | 124–228 | `kfs_ensure_db_file_exists`, `kfs_delete_db_file`, `kfs_close` → **core** |
| Legacy user **read** APIs | 400–1118 | **auth** (registry iterators) — sits *before* DB init banner |
| `kfs_create_god_user` | 1463+ | **auth** — immediately after `kfs_init` |
| Duplicate epic blocks | 6665–7153 + 7154–7714 | Move **both** to lc; preserve order; dedupe only if byte-identical |
| Duplicate note blocks | 8760–9222 + 9223–9873 | Same |
| Duplicate advanced load | 12538–12840 + 12841–13246 | Same |
| `kfs_load_note` (no permission) | 9199 | **lc** — note before permission block ends |

**Rule:** The **function manifest (§5)** wins over banner lines.

### 4.3 Symbol inventory

| Class | Count |
|-------|-------|
| Public `kfs_*` implementations | 122 |
| `static` helpers | 26 |
| **Total function entry points** | **148** |

---

## 5. Function manifest (relocation checklist)

Use this table when moving code. Mark `[x]` as each function lands in its target file.

### 5.1 `kfs_impl_fwd.h` — forward declarations only

| Symbol | Defined in |
|--------|------------|
| `check_group_admin_or_owner_perm` | auth |
| `kfs_can_bootstrap_admin_group_member` | auth |
| `get_active_actor_info_by_uuid` | auth |
| `kfs_modify_scheme_user` | auth |
| `kfs_update_user_field` | auth |
| `hash_string` | core |
| `generate_kfs_uuid_64` | core |
| `exec_sql` | core |
| `get_current_timestamp` | core |
| `get_user_id` / `get_user_id_by_name` | core |
| `get_active_actor_id_by_uuid` | core |
| `is_user_in_group` | core |
| `kfs_get_topic_id_by_name` | lc |
| `kfs_get_epic_id_by_name` | lc |
| `kfs_save_asset` | lc |
| `kfs_load_asset_list` | lc |
| `kfs_link_topic_to_artifact_by_name_internal` | lc |
| `kfs_list_entities` | lc |

### 5.2 `kfs_impl_core.h` — infrastructure

**Platform (lines 12–44):** `stdio`, `stdlib`, `time`, Win32 `gettimeofday` shim.

**Public:**

- `kfs_ensure_db_file_exists`, `kfs_delete_db_file`
- `kfs_init`, `kfs_close`

**Static:**

- `hash_string`, `generate_kfs_uuid_64`, `exec_sql`, `get_current_timestamp`
- `get_user_id`, `get_user_id_by_name`, `get_active_actor_id_by_uuid`, `is_user_in_group`

**Memory frees (lines 13543–end):**

- `kfs_entity_free`, `kfs_user_free*`, `kfs_security_scheme_free*`, `kfs_note_free*`
- `kfs_asset_free*`, `kfs_assets_free`, `kfs_topic_free*`, `kfs_topics_free`
- `kfs_epic_free*`, `kfs_epics_free`, `kfs_actor_free*`, `kfs_user_info_free`
- `kfs_artifact_info_free_contents`

### 5.3 `kfs_impl_auth.h` — registry.db + permission engine

**Bootstrap & legacy users:**

- `kfs_create_god_user`
- `kfs_read_first_user`, `kfs_read_next_user`, `kfs_read_user`, `kfs_user_info_free`
- `kfs_list_user_files`, `kfs_delete_user`, `kfs_update_user_name`
- `kfs_create_user_file`, `kfs_link_epic_to_user_file`, `kfs_get_user_file_epics`, `kfs_unlink_epic_from_user_file`
- `kfs_add_user`, `kfs_get_user`, `kfs_get_user_by_name`

**Actors & groups:**

- `kfs_add_actor`, `kfs_get_actor`, `kfs_get_actor_by_uuid`, `kfs_get_actor_by_name`
- `kfs_update_actor_role`, `kfs_set_actor_active`, `kfs_deactivate_actor`, `kfs_reactivate_actor`
- `kfs_add_member_to_group`, `kfs_remove_member_from_group`, `kfs_is_member_of`
- `kfs_get_actor_info_by_uuid`

**Domains:**

- `kfs_add_domain`, `kfs_delete_domain`, `kfs_update_domain`
- `kfs_add_actor_to_domain`, `kfs_remove_actor_from_domain`, `kfs_list_domains`

**Security schemes & permission:**

- `kfs_check_permission` ← **critical path**
- `kfs_create_security_scheme`, `kfs_get_security_scheme`, `kfs_delete_security_scheme`
- `kfs_add_actor_to_scheme`, `kfs_remove_actor_from_scheme`, `kfs_list_scheme_actors`
- `kfs_add_user_to_scheme`, `kfs_remove_user_from_scheme`
- `kfs_set_entity_owner`, `kfs_set_entity_security_scheme`

**Static (auth-only):**

- `kfs_can_bootstrap_admin_group_member`, `check_group_admin_or_owner_perm`
- `get_active_actor_info_by_uuid`, `kfs_modify_scheme_user`, `kfs_update_user_field`

### 5.4 `kfs_impl_lc.h` — architecture + artifacts

**Epics (both banner regions):**

- `kfs_add_epic`, `kfs_get_epic`, `kfs_get_epic_by_name`, `kfs_delete_epic`
- `kfs_list_epics`, `kfs_update_epic`
- `kfs_save_text`, `kfs_save_script`, `kfs_save_file` (legacy save helpers)

**Topics:**

- `kfs_list_topics`, `kfs_add_topic`, `kfs_delete_topic`, `kfs_get_topic`, `kfs_get_topic_by_name`
- `kfs_link_related_topic*`, `kfs_unlink_related_topic`, `kfs_load_subtopics`, `kfs_update_topic`

**Notes:**

- `kfs_add_note`, `kfs_list_notes`, `kfs_load_note`, `kfs_assign_note`, `kfs_unassign_note`
- `kfs_update_note`, `kfs_get_note`, `kfs_delete_note`

**Linking:**

- `kfs_assign_epic_to_topic`, `kfs_assign_epic_to_topic_by_name`, `kfs_remove_epic_from_topic`

**Artifacts & assets:**

- `kfs_create_artifact*`, `kfs_update_artifact*`, `kfs_delete_artifact`, `kfs_erase_artifact`
- `kfs_get_artifact`, `kfs_load_artifact`, `kfs_list_artifacts*`
- `kfs_link_asset_to_artifact`, `kfs_unlink_asset_from_artifact`, `kfs_get_asset_data`, `kfs_delete_asset`

**Advanced load (both banner regions):**

- `kfs_load_by_topic`, `kfs_load_by_epic`, `kfs_load_scripts_by_epic`
- `kfs_handle_orphaned_artifacts`, `kfs_validate_script`

**Static (lc-only):**

- `kfs_save_asset`, `kfs_load_asset_list`, `kfs_get_topic_id_by_name`, `kfs_get_epic_id_by_name`
- `kfs_link_topic_to_artifact_by_name_internal`, `kfs_list_entities`

---

## 6. Include chain (orchestrator)

```c
#ifndef KFS_IMPL_H
#define KFS_IMPL_H

#ifdef KFS_IMPLEMENTATION

/* Platform + CRT */
#include <stdio.h>
/* ... win32 gettimeofday shim ... */

#include "kfs_api.h"
#include "kfs_impl_fwd.h"
#include "kfs_impl_core.h"
#include "kfs_impl_auth.h"
#include "kfs_impl_lc.h"

#endif /* KFS_IMPLEMENTATION */
#endif /* KFS_IMPL_H */
```

| Module | May call | Must not |
|--------|----------|----------|
| core | SQLite, CRT | `kfs_check_permission` |
| auth | core helpers, `kfs_check_permission` (internal) | LC entity mutations |
| lc | core + auth (`kfs_check_permission`) | — |

**No impl module `#include`s another impl module** — only the orchestrator orders visibility.

---

## 7. Risk register

| ID | Risk | Likelihood | Impact | Mitigation |
|----|------|------------|--------|------------|
| R1 | Cut inside function body | Medium | Build break | Move whole functions; use manifest; compile after each batch |
| R2 | Duplicate epic/note/load sections diverge | Low | Silent behavior change | `diff` the two regions before merge; keep both blocks if different |
| R3 | `static` helper not in `fwd` | High | C compile error | Update fwd when mv calls cross module |
| R4 | `sync_api.py` re-injects fwd into wrong file | High | Corrupts split | **S7:** teach script multi-file; disable `patch_impl` fwd injection |
| R5 | Memory frees reference types from auth/lc | Medium | Link OK, logic OK — but wrong file confuses readers | Frees stay in **core**; they only need struct defs from `kfs_api.h` |
| R6 | Legacy user read APIs (L400) orphaned | Medium | Missing symbols at link | Explicitly in auth manifest §5.3 |
| R7 | Harness passes but permission regression | Low | Security bug | Run **H6** after every auth sub-phase |
| R8 | Perf timing baseline drift | Low | False alarm | Split should not change perf; optional `make test-perf` on S5+ |

---

## 8. Migration phases (detailed)

### Phase overview

```
S0 baseline → S1 scaffold → S2 fwd → S3 core → S4 auth → S5 lc → S6 trim → S7 sync_api → S8 docs
```

**Commit after every sub-phase.** If a gate fails, `git reset --hard` to last green commit.

---

### S1 — Scaffold empty modules

**Work:**

1. Create `kfs_impl_fwd.h`, `kfs_impl_core.h`, `kfs_impl_auth.h`, `kfs_impl_lc.h` with guards + `@file` headers + `/* SECTION */` placeholders.
2. Change `kfs_impl.h` to include them **after** the monolith body initially, OR include empty files before monolith (both compile).

**Preferred bootstrap:** keep monolith body intact; add at end:

```c
/* SPLIT: fragments included below will replace monolith sections in S3–S5 */
```

Actually for S1: only add files to Makefile `KFS_HDR`; **do not** wire includes yet.

**Gate:** `mingw32-make lib` — green.

---

### S2 — Extract forward declarations

**Work:**

1. Move lines 48–56 (+ any new static forwards) → `kfs_impl_fwd.h`
2. Replace monolith block with `/* forwards → kfs_impl_fwd.h */`
3. Wire orchestrator: `#include "kfs_impl_fwd.h"` after `kfs_api.h`

**Gate:**

```bat
cd sit\kfs\kfs && mingw32-make build
cd ..\tests && mingw32-make test-correctness
```

---

### S3 — Extract core (four sub-phases)

| Sub | Move | Harness focus |
|-----|------|---------------|
| **S3.1** | Platform shim (L12–44) → top of `kfs_impl_core.h`; orchestrator includes CRT only once | `lifecycle.init_close`, `lifecycle.double_close` |
| **S3.2** | Static helpers (L294–1119) + early FS (L124–228) + `get_user_id*` | same |
| **S3.3** | `kfs_init` (L1121–~1455) | `lifecycle.*`, `harness.ping` |
| **S3.4** | Memory frees (L13543–end) | `content.*` (frees after artifact ops), `security_schemes.free_contents` |

**Mechanics:**

1. Copy function(s) to `kfs_impl_core.h` with banner comment `/* from kfs_impl.h:Lxxxx */`
2. Delete from monolith
3. `#include "kfs_impl_core.h"` in orchestrator **before** auth
4. Build + run gate

**Gate (S3 complete):**

```bat
mingw32-make build
cd ..\tests && mingw32-make test-correctness
```

---

### S4 — Extract auth (five sub-phases)

| Sub | Move | Harness focus |
|-----|------|---------------|
| **S4.1** | Legacy user read + user file APIs (L400–1118, L1463–2396) | `lifecycle.create_god_user` |
| **S4.2** | Actors & groups (L2399–3934) | `actors.*` |
| **S4.3** | Security schemes + `kfs_check_permission` (L3936–5140) | `security_schemes.*`, `perf.permission_check` |
| **S4.4** | Domains + wrap-up setters (L5141–6436) | `domains.*` |
| **S4.5** | Legacy `kfs_add_user` block (L6438–6662) | `lifecycle.bootstrap_admin` |

**Critical:** `kfs_check_permission` moves in **S4.3** — run `permissions.*` immediately after.

**Gate (S4 complete):**

```bat
mingw32-make build
cd ..\tests && mingw32-make test-correctness
cd ..\tests && mingw32-make test-perf --test permission_check   REM or full test-perf
```

---

### S5 — Extract lc (six sub-phases)

| Sub | Move | Harness focus |
|-----|------|---------------|
| **S5.1** | Epic blocks **both** (L6665–7714) | `content.epic_topic_link` |
| **S5.2** | Topics (L7716–8758) | `content.topic_assign`, `permissions.topic_read_gate` |
| **S5.3** | Notes both blocks (L8760–9873) | `content.note_assign` |
| **S5.4** | Linking + artifacts (L9875–12536) | `content.*`, `permissions.*` |
| **S5.5** | Advanced load both blocks (L12538–13246) | `perf.load_by_topic_*`, `perf.blob_read_*` |
| **S5.6** | Misc (L13248–13542) | `content.legacy_save_text` |

**Gate (S5 complete):**

```bat
cd sit\kfs\tests && mingw32-make test
```

**46/46 required** before S6.

---

### S6 — Orchestrator-only trim ✅

**Work (done 2026-06-30):**

1. Deleted all monolith bodies — relocated in S3–S5
2. `kfs_impl.h` is **~22 lines** (includes + guards only)
3. Grep invariants (§9) satisfied

**Gate:** full §9 checklist — passed with S5 `make test` (46/46).

---

### S7 — `sync_api.py` multi-file support

**Today (brittle):**

- Reads/writes single `kfs_impl.h`
- Injects forward-decl block into monolith
- `extract_decls` scans one file

**Target:**

```python
IMPL_FRAGMENTS = [
    KFS_SRC_DIR / "kfs_impl_core.h",
    KFS_SRC_DIR / "kfs_impl_auth.h",
    KFS_SRC_DIR / "kfs_impl_lc.h",
]
# Skip kfs_impl_fwd.h and kfs_impl.h orchestrator
```

**Work:**

1. Move `patch_impl` forward-decl injection → maintain `kfs_impl_fwd.h` directly (one-time manual sync, then remove auto-inject)
2. `extract_decls`: concatenate fragments in core → auth → lc order
3. **Never** write moved function bodies back to orchestrator

**Gate:**

```bat
mingw32-make sync-api
git diff kfs/kfs_api.h   REM must be empty
```

---

### S8 — Documentation & build deps 🟡

| File | Update | Status |
|------|--------|--------|
| `architecture.md` §7 | Post-split table + module boundaries | ✅ |
| `refactor_plan.md` | §8 Impl split (S0–S8) with status | ✅ |
| `COMPILATION_GUIDE.md` | Package tree, sync_api note | ✅ |
| `doc/README.md` | Index links | ✅ |
| `kfs/kfs/Makefile` | `KFS_HDR` lists all 5 impl headers | ✅ (S1) |
| `tests/Makefile` | `KFS_HDR` prerequisite list | ✅ (S1) |

**Gate:** `mingw32-make build && cd ../tests && mingw32-make test` — **46/46**

---

## 9. Verification checklist (run after S6)

```bat
cd sit\kfs\kfs
mingw32-make lib
mingw32-make api-check
mingw32-make sync-api
git diff kfs_api.h

cd ..\tests
mingw32-make test
```

| # | Invariant | Command |
|---|-----------|---------|
| 1 | No function bodies in orchestrator | `rg "^\s*(int|void)\s+kfs_" kfs/kfs_impl.h` → empty |
| 2 | All public impls accounted for | 122 `kfs_*` defs across core+auth+lc |
| 3 | Cross-module statics forwarded | `rg "static int" kfs_impl_*.h` vs fwd list |
| 4 | Harness | 46/46 pass |
| 5 | API header stable | `git diff kfs_api.h` empty |
| 6 | Line budget | `wc -l kfs_impl*.h` — lc largest (~7k), orchestrator <50 |

---

## 10. Harness gate matrix (quick reference)

| Module | Tests | Validates |
|--------|-------|-----------|
| harness | ping | core init |
| lifecycle | 5 | init/close/bootstrap/god user |
| actors | 6 | auth actors/groups |
| domains | 5 | auth domains + firewall |
| security_schemes | 5 | auth schemes |
| content | 7 | lc artifacts/topics/epics/notes |
| permissions | 6 | auth `kfs_check_permission` + lc reads |
| perf | 11 | lc blob/load throughput |

**Minimum per sub-phase:** always `lifecycle.init_close` + one module touching moved code.

---

## 11. Duplicate-section protocol

Before moving epic/note/advanced-load duplicate banners:

```bat
# Example: compare epic regions
sed -n '6665,7153p' kfs_impl.h > /tmp/epic_a.c
sed -n '7154,7714p' kfs_impl.h > /tmp/epic_b.c
diff /tmp/epic_a.c /tmp/epic_b.c
```

| diff result | Action |
|-------------|--------|
| Empty | Merge to single copy in lc; delete duplicate |
| Non-empty | Keep **both** blocks in lc in original order |
| Functions only in one block | Move that block; grep callers |

---

## 12. PR / branch strategy

| PR | Phase | Review focus |
|----|-------|--------------|
| PR-1 | S1–S2 | Scaffold + fwd only — trivial |
| PR-2 | S3 | Core extraction — lifecycle tests |
| PR-3 | S4 | Auth — permissions + domains |
| PR-4 | S5 | LC — content + perf |
| PR-5 | S6–S8 | Trim + sync_api + docs |

Avoid a single 13k-line diff — reviewers cannot reason about it.

---

## 13. Post-split size (measured 2026-06-30)

| File | Lines | Share | Notes |
|------|-------|-------|-------|
| `kfs_impl_fwd.h` | 23 | <1% | Cross-module `static` forwards |
| `kfs_impl_core.h` | 956 | 14% | Platform, `kfs_init`/`kfs_close`, utilities, memory frees |
| `kfs_impl_auth.h` | 5,278 | 39% | registry.db — actors, domains, schemes, `kfs_check_permission` |
| `kfs_impl_lc.h` | 6,106 | 45% | architecture + artifacts — epics, topics, notes, loaders |
| `kfs_impl.h` | 22 | — | Orchestrator (includes only) |
| **Total impl** | **~12,379** | 100% | Was ~13,910 in single monolith (headers/guards/accounting) |

`kfs_impl_lc.h` is slightly above the ~6k soft cap. A future **S9** sub-split (`kfs_impl_content.h` + `kfs_impl_link.h`) is optional — not required for navigation today.

Extraction scripts (replay / reference): `scripts/s3_extract_core.py`, `s4_extract_auth.py`, `s5_extract_lc.py`.

---

## 14. Future extensions (post-S8)

| Extension | Target |
|-----------|--------|
| Clone / lineage ([clone_plan.md](../clone_plan.md)) | `kfs_impl_lc.h` or `kfs_impl_clone.h` |
| `KFS_EntityKind` enum for permission engine | `kfs_impl_fwd.h` or auth |
| Situation worker thread (H8.9) | Document thread ownership per module |
| LC sub-split | Only if lc > 6k lines |

---

## 15. Success criteria

- [x] Four impl fragments + orchestrator exist (S1–S6)  
- [x] **46/46** harness tests pass (correctness 35 + perf 11)  
- [ ] `sync_api.py` reads fragments; `kfs_api.h` unchanged (**S7**)  
- [x] `architecture.md` reflects four-file layout (**S8**)  
- [x] No SQL / trace / signature changes in split commits  
- [x] Perf baselines stable post-split (informational; see [split_baseline_test.log](split_baseline_test.log))

---

## 16. Related documents

| Document | Status |
|----------|--------|
| [architecture.md](../architecture.md) | Updated — §7 post-split layout |
| [refactor_plan.md](refactor_plan.md) | Updated — §8 impl split track |
| [test_harness_plan.md](../test_harness_plan.md) | Harness unchanged; still the gate (46/46) |
| [COMPILATION_GUIDE.md](../COMPILATION_GUIDE.md) | Updated — package tree, `sync_api` note |

---

## Appendix A — Orchestrator (achieved S6)

Platform/CRT and the Win32 `gettimeofday` shim live in **`kfs_impl_core.h`** (S3.1). The orchestrator is includes only:

```c
/**
 * @file kfs_impl.h
 * @brief KFS implementation orchestrator — include chain only.
 */
#ifndef KFS_IMPL_H
#define KFS_IMPL_H

#ifdef KFS_IMPLEMENTATION

#include "kfs_api.h"
#include "kfs_impl_fwd.h"
#include "kfs_impl_core.h"
#include "kfs_impl_auth.h"
#include "kfs_impl_lc.h"

#endif /* KFS_IMPLEMENTATION */
#endif /* KFS_IMPL_H */
```

Include order is fixed: **fwd → core → auth → lc**. LC may call `kfs_check_permission` (auth); core must not.