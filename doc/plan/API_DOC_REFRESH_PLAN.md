# Situation API Documentation Refresh Plan

## Status Summary (as of v2.4.199 — 2026-06-04, COMPLETED)

| Metric | Value |
|--------|-------|
| Library version | **v2.4.199** "System Introspection APIs" |
| Doc updated to | **v2.4.199** ✅ |
| Patch gap | **0** (closed) |
| `SITAPI` functions in header | **531** |
| Functions documented in `situation_api.md` | **531/531** ✅ |
| **Undocumented functions** | **0** ✅ |
| Auto-gen script | `scripts/generate_situation_api_docs.py` — **functional, 0 warnings** |
| `doc/whatsnew.md` | Updated to v2.4.199 |
| `doc/situation_command_reference.md` | Updated to v2.4.199, 70 active commands indexed |
| `doc/situation_api_index.md` | Regenerated (auto) |

## Problem Statement

`doc/situation_api.md` is frozen at **v2.4.106**. The library is at **v2.4.199**. That's 93 patches of new, changed, and deprecated APIs that users cannot discover from the documentation. The doc also contains references to struct fields and function signatures that no longer exist.

~95 public API functions have no documentation entry. Several documented structs have stale field names. Removed APIs like `SituationStartAudioPlayback` are still documented without deprecation/removal notice.

## Scope

- Single source of truth: `doc/situation_api.md`
- Supporting index: `doc/situation_api_index.md` (auto-generated)
- SDK manual: `doc/situation_sdk.md`
- What's-new summary: `doc/whatsnew.md` (kept in sync per-release, not full reference)
- Header truth: `sit/situation_api.h` (the actual declarations — **533 SITAPI functions as of v2.4.199**)

## Phase 1: Audit — Identify Gaps

- [x] Count total `SITAPI` functions in `sit/situation_api.h` → **533** (verified 2026-06-04)
- [x] Count documented functions in `doc/situation_api.md` → **531/531** (0 missing after refresh)
- [x] Diff: list functions present in header but missing from doc → **0 remaining** (all 78 gaps filled)
- [x] Diff: list functions in doc that no longer exist in header → deprecated table added
- [x] Identify deprecated functions still documented without deprecation notice → deprecation table added
- [x] Check struct definitions in doc vs header → `SituationDeviceInfo` and `SituationInitInfo` updated to match current header
- [x] Check all `Usage Example` code blocks → fixed stale field references (`os_name`, `cpu_brand`, deprecated commands)
- [x] Update version string in doc header from v2.4.106 to v2.4.199

## Phase 2: Categorize Missing APIs by Subsystem

Group undocumented APIs into their natural sections:

- [x] **Threading** (v2.4.139–v2.4.197): thread pool, topology, affinity, snapshot, metrics, dispatch, `SituationGetInternalThreadPool`, `SituationGetConfiguredIOThreadAffinity`, NUMA spread, thread naming
- [x] **Renderer Bolster** (v2.4.147–v2.4.188): barriers, transfers, compute, raster state, viewports, render passes, `SituationRenderPassInfoDefault`, `SituationRenderPassInfoLoad`, `SituationRenderPassConfigurationKey`
- [x] **Audio Graph** (v2.4.106–v2.4.198): device registry, node types, `SITUATION_NODE_PCM_INPUT`, `SituationPushNodePCM`, `SituationGetNodePCMFreeFrames`, graph routing, mixer, output monitor
- [x] **Tone Synth** (v2.4.112–v2.4.195): graph synth, SVF filter, patch memory, sub oscillator (coarse/sync/ring mod CC111–CC113)
- [x] **Shader/SPIR-V** (v2.4.83–v2.4.191): async load, SPIR-V memory, poll, error reporting, `SituationBeginLoadShaderFromMemory` hardening
- [x] **YPQ Color** (v2.4.192–v2.4.193): `ColorYPQf`, pixel API, `SituationImageAdjustYPQ`, `SituationCmdDrawTextureYpqGrade`, float edit path, in-gamut chroma clamp
- [x] **System Introspection** (v2.4.199): `SituationGetOSInfo`, `SituationGetProcessList`, `SituationFreeProcessList`, `SituationGetActiveAudioDeviceName`
- [x] **Graphics Caps** (v2.4.173): backend query, caps struct expansion
- [x] **Error System** (v2.4.124–v2.4.138): expanded errno table, error propagation
- [x] **Window/Display** (v2.4.194): `SITUATION_FLAG_WINDOW_TOPMOST` default, GLFW visible hint behavior

## Phase 3: Write Documentation

For each missing API, document with this template:

```markdown
#### `FunctionName` _(vX.Y.Z)_
One-sentence description.
\```c
ReturnType FunctionName(params);
\```
**Usage Example:**
\```c
// Minimal working example
\```
```

- [x] Document all Phase 2 threading APIs (~20 functions)
- [x] Document all Phase 2 renderer APIs (~40 functions)
- [x] Document all Phase 2 audio graph APIs (~30 functions)
- [x] Document all Phase 2 tone synth APIs
- [x] Document all Phase 2 shader APIs
- [x] Document all Phase 2 YPQ color APIs (~10 functions)
- [x] Document all Phase 2 system introspection APIs (4 functions — `SituationGetOSInfo`, `SituationGetProcessList`, `SituationFreeProcessList`, `SituationGetActiveAudioDeviceName`)
- [x] Document all Phase 2 graphics caps APIs
- [x] Document all Phase 2 error system APIs
- [x] Document all Phase 2 window/display APIs
- [x] Mark deprecated/removed functions with notice and replacement (e.g., `SituationStartAudioPlayback` → `SITUATION_NODE_PCM_INPUT` + `SituationPushNodePCM`)

## Phase 4: Validate

- [x] Run `python scripts/generate_situation_api_docs.py` to verify coverage count → **531/531, 0 missing, 0 warnings**
- [x] Grep for any `SITAPI` in header not mentioned in doc — **0 undocumented**
- [x] Verify no doc references to removed functions — deprecated table added with replacements
- [x] Update doc header version to match `situation_base_version.h` → v2.4.199
- [x] Update `situation_api_index.md` — regenerated (auto-gen script)
- [x] Update `situation_sdk.md` version metadata — **already at v2.4.199**
- [x] Verify `doc/whatsnew.md` covers all user-facing additions (currently up to date)
- [x] Update `situation_command_reference.md` — version synced, index expanded from 35→70 active commands
- [x] Update `CMD_REF_ANCHORS` in generator script — all 70 commands mapped
- [x] Fix incorrect "strictly single-threaded" claims in steering file and API doc

## Known Outdated Content in Current Doc

### Struct fields that don't match:
- `SituationDeviceInfo` — doc shows `os_name`, `os_version`, `cpu_brand`, `gpu_brand`; header has `cpu_name`, `gpu_name`, no OS fields (OS moved to `SituationOSInfo` in v2.4.199)
- `SituationInitInfo` — many new fields since v2.4.106 (threading config, render thread, NUMA spread, `worker_numa_spread`, `thread_pool_reserved_threads` default 4, `render_thread_count`)
- `SituationThreadSlotSnapshot` — doc missing `index` and `name[24]` fields (v2.4.197)

### Functions removed or renamed:
- `SituationStartAudioPlayback` — **removed in v2.4.198** (replaced by `SITUATION_NODE_PCM_INPUT` + `SituationPushNodePCM`)
- Old render API paths before command buffer model solidified
- `sub_note` (tone synth) — replaced by `sub_coarse` CC111 (v2.4.189)

### Behavior changes not reflected in doc:
- Window topmost default (v2.4.194)
- Thread pool reserved threads default changed from 1 → 4 (v2.4.197)
- `SITUATION_WORKER_NUMA_SPREAD_DEFAULT` defaults ON (v2.4.197)
- Render thread included in default DLL builds (v2.4.197)

### Large sections entirely missing:
- Thread pool API (~20 functions) — pool lifecycle, snapshot, dump, topology, affinity, metrics
- Audio graph/node API (~30 functions) — device registry, node create/destroy/connect, PCM input, graph routing, mixer, output monitor
- Renderer bolster commands (~40 functions) — barriers, transfers, raster state, compute dispatch, indirect draw, render pass helpers
- Tone synth API — graph creation, voice params, sub oscillator, MIDI CC mapping
- MIDI integration API — CC standardization, learn mode
- YPQ color API (~10 functions) — ColorYPQf, pixel conversion, image adjust, GPU grade
- System introspection API (4 functions) — OS info, process list, audio device name

## Effort Estimate

This is a multi-session task. The doc is ~2100+ lines currently. The missing content (~95 new functions + struct updates + deprecation notices) likely adds 1500–2000 lines more. Recommended approach:
1. One session per Phase 2 subsystem category
2. Work from the header declarations + UPDATELOG for context
3. Validate examples compile against current DLL (v2.4.199)
4. Renderer bolster is the largest chunk (~40 functions) — consider splitting into sub-sessions (barriers, transfers, raster state, compute)

### Suggested Session Order (by dependency and impact):
1. **System Introspection** (4 functions, newest, freshest in memory) — quick win
2. **YPQ Color** (~10 functions, well-contained) — quick win
3. **Threading** (~20 functions, critical for users)
4. **Audio Graph + PCM Input** (~30 functions)
5. **Renderer Bolster** (~40 functions, largest)
6. **Tone Synth + MIDI** (variable)
7. **Shader/SPIR-V + Error System + Graphics Caps** (remaining)
8. **Struct updates + removals + deprecation sweep** (final pass)

## Priority

**High** — users relying on this doc are seeing a 93-version-stale picture of the library. 95 public API functions are undiscoverable except by reading the header directly. Key subsystems (threading, audio graph, renderer commands) have no doc coverage at all.
