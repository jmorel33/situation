# Console Merge & Deprecation Plan

**Status:** Phases 1–3 done (verified 2026-06-06). Phase 4.2–4.3 done; 4.1 and 4.4 on hold.

**Completed:** 2026-06-06  
**Scope:** Merge `examples/console.c` into `examples/kterm_console.c`, delete the legacy file, and establish **one canonical KaOS Terminal** — a **K-Term product** hosted under Situation examples for development.

## Current status (where we stand)

| Area | State | Notes |
|------|--------|--------|
| **Code merge (P1–P6)** | Done | `ConsoleCopyString`, `sys_info`/`sys_displays`, screenshot env vars, tab completion, `term_vtlevel`, dead code removed |
| **Legacy `console.c`** | Deleted | No longer in `examples/` |
| **CMake / bat build** | Done | Target `kterm_console`; `console_example` removed |
| **Docs & ownership** | Done | K-Term updatelog, COMPILATION_GUIDE, goals/integration plans; **not** Situation `UPDATELOG.md` |
| **Product model** | Documented | K-Term core product; Situation host; source stays in `examples/` for dev |
| **Tab arg completion (1.4.2)** | Done | `font` names; `cd`/`type` path glob (Windows) |
| **Verification** | Done | Bat build + manual console test |
| **Phase 4** | Done | 4.2 mouse commands, 4.3 screenshot harness (4.1, 4.4 on hold) |

**Next:** none (4.1, 4.4 on hold).

## Executive summary

**Was:** two terminal console examples — **`kterm_console.c`** (~2,119 LOC, active) and **`console.c`** (~1,434 LOC, legacy, CMake-only).

**Now:** one canonical **`examples/kterm_console.c`** (KaOS Terminal). Legacy removed; parity items ported; docs unified under **K-Term ownership** (not Situation core).  
**Primary files (touched):** `examples/kterm_console.c`, `examples/CMakeLists.txt`, `build_examples.bat`, `doc/COMPILATION_GUIDE.md`, `sit/k-term/doc/updatelog.md`.  
**Removed:** `examples/console.c`.  
**Related plans:** [KTERM_CONSOLE_GOALS_PLAN.md](KTERM_CONSOLE_GOALS_PLAN.md), [KTERM_VD_MIGRATION_PLAN.md](KTERM_VD_MIGRATION_PLAN.md), [KTERM_CONSOLE_RENDERING_BUGFIX_PLAN.md](KTERM_CONSOLE_RENDERING_BUGFIX_PLAN.md).

---

## How to use this file

- [x] Read **§ Canonical console definition** — naming/ownership contract going forward.
- [x] Execute phases in order; do not delete `console.c` until Phase 2 gate passes.
- [x] When complete, update `sit/k-term/doc/updatelog.md`, redirect stale doc references, and remove `console.c`. *(Do **not** add console merge to `doc/UPDATELOG.md` — that is Situation library releases only.)*

---

## Problem statement *(resolved)*

The repo **previously** shipped two nearly identical terminal console examples:

| File | LOC | Was | Resolution |
|------|-----|-----|------------|
| `examples/kterm_console.c` | ~2,119 | Active KaOS terminal | **Canonical** — K-Term product, Situation-hosted example |
| ~~`examples/console.c`~~ | ~1,434 | Legacy stub, CMake `console_example` | **Deleted** — parity merged into kterm |

**Was confusing:** dual entry points, safety fixes only in legacy, CMake mislabeling, inconsistent docs, `MA_NO_MP3`/`MA_NO_FLAC` in legacy build.

**Resolved:** one file, one target (`kterm_console`), K-Term-owned changelog/docs, ownership model documented in § Canonical console definition.

---

## Canonical console definition

### Product ownership (important)

KaOS Terminal is **not** Situation core. It is a **K-Term core product** — the reference app for terminal emulation, CLI, ConPTY shell, and VD integration.

| Layer | Role |
|-------|------|
| **K-Term** (`sit/k-term/`) | **Product owner** — terminal engine, shell backend, console commands, regression target. Changelog: `sit/k-term/doc/updatelog.md`. Plans: `doc/plan/KTERM_*`. |
| **Situation** (`situation.h`) | **Host platform** — window, input, VD compositor, sysinfo APIs used by the console. Not responsible for console CLI semantics. Changelog: `doc/UPDATELOG.md` (library only). |
| **Situation examples** (`examples/kterm_console.c`) | **Dev location** — source and build target live here **for now** because the monorepo couples K-Term development to a Situation host frame. This is convenience, not a claim that the console is part of `situation.h`. |

**Rules:**

- Console features, bugs, and releases → **K-Term docs/plans**, not Situation `UPDATELOG.md`.
- Situation `UPDATELOG.md` only when **`situation.h` / core impl** changes to support the console (e.g. new `SituationGetProcessList` API).
- Keep `examples/kterm_console.c` in Situation examples **at least through current development**; a future split (e.g. `sit/k-term/example/`) is optional and not required for ownership clarity.

After this plan completes, there is **exactly one** canonical console source:

### Name & location

| Item | Canonical value | Deprecated |
|------|-----------------|------------|
| **Source file** | `examples/kterm_console.c` | ~~`examples/console.c`~~ (deleted) |
| **CMake target** | `kterm_console` | ~~`console_example`~~ (removed) |
| **Batch build name** | `kterm_console` | *(none)* |
| **Binary output** | `kterm_console.exe` (Windows) / `kterm_console` (Unix) | `console_example.exe` |
| **User-facing name** | **KaOS Terminal** (or "K-Term Console") | "console example" |

### What it is (and is not)

**It is:**

- A **K-Term reference application** (KaOS Terminal) — the product face of terminal + shell work.
- A **Situation-hosted example** during development (`examples/kterm_console.c`).
- The **host frame** for virtual-display terminal rendering (`SituationRenderVirtualDisplays`).
- A **built-in command shell** (`ProcessCommand`) plus optional **system shell pass-through** (`shell` → `KTShell`).
- The **regression vehicle** for K-Term rendering, input, ConPTY; uses Situation sysinfo APIs where useful.

**It is not:**

- Part of the Situation library API (`situation.h` does not expose console commands).
- A Situation core product or release milestone (do not log in `doc/UPDATELOG.md` unless core APIs change).
- A second library or header (`console.h` does not exist as a distributable module).

### Command namespace (post-merge)

Two layers, clearly separated in help text:

| Layer | Prefix / style | Examples |
|-------|----------------|----------|
| **Built-in CLI** | No prefix; implemented in `ProcessCommand()` | `help`, `demo`, `sysinfo`, `font`, `type`, `shell` |
| **Legacy aliases** | Kept for scripts/docs parity | `sys_info` → full device dump; `cls` → `clear` |
| **System shell** | Active only in `shell_mode` | `cmd.exe`, PowerShell, `/bin/sh` via ConPTY |

**Rule:** New commands go only into `kterm_console.c`. No new `console.c`-style forks.

---

## Diff summary (what to merge vs drop)

### Port from `console.c` → `kterm_console.c` (must-have)

Tracked in Phase 1; summary:

- [x] **P1** `ConsoleCopyString()` + replace `strcpy` in history/edit paths — safety fix in legacy; kterm regressed
- [x] **P2** `sys_info` → `SitHelperPrintDeviceInfo(SituationGetDeviceInfo())` — legacy has full dump; kterm stub only
- [x] **P3** `sys_displays` → `SituationFreeDisplays()` + `SituationGetCurrentMonitor()` — correct API + useful output
- [x] **P4** `KTERM_CAPTURE_SCREENSHOT` / `KTERM_CAPTURE_EXIT` env vars — headless screenshot / visual regression
- [x] **P5** Tab completion for `font`, `cd`, `ls`, `dir`, `pwd`, `sysinfo`, `ps`, `threads`, `workers`, `type`, `shell`
- [x] **P6** Extended `term_vtlevel` names (VT510–525, K95, etc.) — terminal QA parity

### Already in `kterm_console.c` (keep; do not revert)

| Feature | Notes |
|---------|-------|
| `shell`, `type`, `pwd`, `cd`, `ls`, `sysinfo`, `ps`, `threads`, `font` | Core differentiators |
| `KTShell` + ConPTY resize | See KTERM_CONSOLE_GOALS_PLAN |
| Virtual display render path | See KTERM_VD_MIGRATION_PLAN |
| `HandleKTermResponse` (evolved parser) | Supersedes legacy `HandleTerminalResponse` |
| Title callback (OSC 2) | Window title sync |
| Richer `demo` / `graphics` | Superset of legacy output |

### Drop from `console.c` (do not port)

| Item | Reason |
|------|--------|
| `PipelineWrite*` macro layer | kterm uses direct `KTerm_Write*` — cleaner |
| `MA_NO_MP3` / `MA_NO_FLAC` | Build hack; distorts library capability assessment |
| `KTERM_DISABLE_GATEWAY/NET/VOICE/VOIP` | Console needs full K-Term |
| `SITUATION_BEGIN_FRAME()` loop | kterm's explicit poll/update matches VD architecture |
| `ACTIVE_SESSION` macro | Cosmetic; `GET_SESSION(term)` is equivalent |
| `ProcessConsolePipeline()` | Dead code — **removed** from kterm |
| Duplicate/stale help for `mouse_on`/`mouse_off` | **Removed** from help (not implemented) |

### Resolve explicitly (pick one)

- [x] **`sys_info` vs `sysinfo`:** Keep **both** — `sysinfo` = formatted snapshot; `sys_info` = raw `SitHelperPrintDeviceInfo` dump. Document in help.
- [x] **Welcome banner encoding:** kterm uses UTF-8 banner (`ESC % G`) per rendering bugfix plan; legacy CP437 border not ported.
- [x] **CMake vs bat build flags:** Unify on `SITUATION_IMPLEMENTATION`, `SITUATION_USE_OPENGL`, `-Isit/k-term`, optional `SITUATION_ENABLE_THREADING`.

---

## Phase 0 — Baseline & gate *(skipped)*

**Objective:** Record pre-merge behavior. **Skipped** — merge proceeded; legacy file no longer exists. Phase 1 parity list was confirmed against source before deletion.

- [x] **0.4** Gaps filled in Phase 1 (P1–P6).
- [~] **0.1–0.3** Not run formally — superseded by post-merge verification (V2/V3/V7).

---

## Phase 1 — Merge code into `kterm_console.c`

**Objective:** kterm is a strict superset of legacy behavior.

### 1.1 — String safety

- [x] **1.1.1** Copy `ConsoleCopyString()` from `console.c` into `kterm_console.c` (static helper).
- [x] **1.1.2** Replace `strcpy` in `AddToHistory()`, `NavigateHistory()`, `HandleEnterKey()` (command_buffer ← edit_buffer).
- [x] **1.1.3** Grep kterm for remaining unbounded copies on `command_history[]`, `edit_buffer[]`, `command_buffer[]`.

### 1.2 — System commands parity

- [x] **1.2.1** **`sys_info`:** replace stub with `SitHelperPrintDeviceInfo(SituationGetDeviceInfo())`.
- [x] **1.2.2** **`sys_displays`:** use `SituationFreeDisplays(displays, display_count)`; restore `SituationGetCurrentMonitor()` line.
- [x] **1.2.3** Verify `sys_audio`, `sys_userdir` unchanged (already shared).

### 1.3 — Screenshot automation

- [x] **1.3.1** Port env-var block from `console.c` main loop (`KTERM_CAPTURE_SCREENSHOT`, `KTERM_CAPTURE_EXIT`).
- [x] **1.3.2** Adapt to kterm's frame loop (`SituationEndFrame` / VD path if needed).
- [x] **1.3.3** Document in help page 3 or `doc/COMPILATION_GUIDE.md` § testing.

### 1.4 — Tab completion

- [x] **1.4.1** Extend `CompleteCommand` string array with: `type`, `shell`, `font`, `pwd`, `cd`, `ls`, `dir`, `sysinfo`, `ps`, `processes`, `threads`, `workers`.
- [x] **1.4.2** Add argument completion for `font` (existing font table) and `cd`/`type` (paths) — **done** (Windows cwd glob; font name list)

### 1.5 — Terminal QA parity

- [x] **1.5.1** Merge extended `term_vtlevel` case labels from legacy.
- [x] **1.5.2** Help cleanup: remove or implement `mouse_on` / `mouse_off` (stale in both).

### 1.6 — Dead code removal

- [x] **1.6.1** Delete `ProcessConsolePipeline()` if still unused.
- [x] **1.6.2** Remove commented legacy blocks and duplicate handlers (e.g. double `pipeline_stats` paths if any).

**Gate Phase 1:**

- [x] `build_examples.bat opengl kterm_console` — SUCCESS *(maintainer, 2026-06-06)*
- [x] Manual: `sys_info`, `sys_displays`, `sysinfo`, `shell`, `type`, history with long lines, tab completion on new commands *(maintainer)*
- [ ] Optional: `set KTERM_CAPTURE_SCREENSHOT=out.png` + `KTERM_CAPTURE_EXIT=1` → PNG written, clean exit

---

## Phase 2 — Unify build & docs

**Objective:** One target everywhere; no references to legacy file.

### 2.1 — CMake

- [x] **2.1.1** Replace `console_example` / `console.c` with `kterm_console` / `kterm_console.c` in `examples/CMakeLists.txt` (see snippet below).
- [x] **2.1.2** Rename output property `console_example` → `kterm_console` in `set_target_properties`.
- [x] **2.1.3** Confirm no `console_example` target remains.

```cmake
# Before
add_executable(console_example console.c)

# After
add_executable(kterm_console kterm_console.c)
target_include_directories(kterm_console PRIVATE ${CMAKE_SOURCE_DIR}/sit/k-term)
target_compile_definitions(kterm_console PRIVATE
    SITUATION_IMPLEMENTATION
    SITUATION_USE_OPENGL
)
target_link_libraries(kterm_console PRIVATE situation)
```

### 2.2 — `build_examples.bat`

- [x] **2.2.1** Confirm `kterm_console` remains the documented example name.
- [x] **2.2.2** Remove any dead branches referencing `console.c` if present.
- [x] **2.2.3** Add comment: *"Canonical Situation terminal console — see doc/plan/CONSOLE_MERGE_DEPRECATION_PLAN.md"*.

### 2.3 — Documentation redirects

Update references from `console.c` / `console_example` → `kterm_console.c` / `kterm_console`:

- [x] **2.3.1** `doc/COMPILATION_GUIDE.md` — add single "Building the terminal console" section
- [x] **2.3.2** `sit/k-term/doc/updatelog.md` — console merge entry (not Situation `UPDATELOG.md`)
- [x] **2.3.3** `sit/k-term/doc/updatelog.md` — redirect "Console Example" sections to kterm only
- [x] **2.3.4** `sit/k-term/doc/kterm_2_7_6_upgrade_plan.md` — mark console.c audit superseded by kterm
- [x] **2.3.5** `sit/k-term/doc/kterm_bug_fix_plan.md` — note UTF-8 reset lives in kterm post-merge
- [x] **2.3.6** `doc/reports and notes/VERSION_2.4.0_RELEASE_NOTES.md` — fix stale `console_example.c` name
- [x] **2.3.7** `doc/reports and notes/KTERM_INTEGRATION_STATUS.md` — list only `kterm_console.c`
- [x] **2.3.8** [KTERM_CONSOLE_GOALS_PLAN.md](KTERM_CONSOLE_GOALS_PLAN.md) header — *"Canonical console: `examples/kterm_console.c` only."*

**Gate Phase 2:**

- [x] Doc redirects complete (see § 2.3). Residual `console.c` mentions only in this plan (historical), git history, and merge-context notes in K-Term updatelog.

---

## Phase 3 — Delete legacy file

**Objective:** Remove `console.c` and verify nothing breaks.

- [x] Delete `examples/console.c`
- [x] Bat build: `build_examples.bat opengl kterm_console` *(maintainer verified)*
- [ ] CMake build: target `kterm_console` *(optional — V3)*
- [ ] CI-style gates from ERROR_PROPAGATION_PLAN: threading + filesystem tests + kterm_console build
- [x] Update `sit/k-term/doc/updatelog.md` with merge note (not Situation `UPDATELOG.md`)

**Gate Phase 3:**

- [x] No source file named `console.c` under `examples/`
- [x] No CMake target `console_example`
- [x] kterm_console is the only terminal console example listed in COMPILATION_GUIDE

---

## Phase 4 — Follow-ups

K-Term console only. Situation core out of scope.

### 4.1 — Shared CLI helpers (on hold)

- [~] **4.1.1** Extract helpers into `examples/console_common.h` if a second terminal example is needed.
- [~] **4.1.2** No extraction until then.

### 4.2 — Mouse reporting commands

- [x] **4.2.1** `mouse_on` / `mouse_off` in `ProcessCommand()` (SGR reporting via `KTerm_SetMouseTracking`).
- [~] **4.2.2** Document unimplemented — not needed; commands implemented.

### 4.3 — Automated screenshot regression

- [x] **4.3.1** Harness test: `tests/harness/test_kterm_console.c` (`sit_test.exe --module kterm_console`).
- [x] **4.3.2** Documented in `doc/COMPILATION_GUIDE.md`.

### 4.4 — Build & harness hygiene (on hold)

- [~] **4.4.1** CMake install `kterm_console` to `bin/`?
- [~] **4.4.2** Keep `kterm_simple_test.c` or fold into harness?

### Out of scope

- [x] Binary rename — stays `kterm_console`.

---

## Verification checklist (final)

- [x] **V1** Single console source: `examples/kterm_console.c` only
- [x] **V2** Build (bat): `build_examples.bat opengl kterm_console` → SUCCESS *(maintainer, 2026-06-06)*
- [ ] **V3** Build (CMake): target `kterm_console` → SUCCESS *(optional)*
- [x] **V4** `sys_info`: full device enumeration (code merged)
- [x] **V5** `sysinfo`: formatted snapshot (unchanged)
- [x] **V6** `sys_displays`: frees with API; shows current monitor (code merged)
- [x] **V7** `shell` / `type` / `font`: work as today (manual smoke — maintainer)
- [x] **V8** History safety: long commands don't overflow buffers (ConsoleCopyString)
- [x] **V9** MP3/OGG/FLAC enabled (no `MA_NO_*` in console example)
- [x] **V10** Docs: no stale `console.c` references (except this plan history)

---

## Summary

Merge done. Phase 4.2–4.3 done; 4.1 and 4.4 on hold.
