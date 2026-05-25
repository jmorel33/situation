# Phase H — Detailed Execution Plan: Remove miniaudio Mixer

**Date**: 2026-05-07  
**Risk Level**: HIGH — breaking API change, no VCS, irreversible  
**Strategy**: Deprecate → Backup → Isolate → Rewire → Remove → Validate  
**Prerequisite**: Phase G passed (all 119 audio tests green, DLL builds clean)

**Revision (2026-05-10)**: Steps 2–4 assumed **`active_graph`** could replace **all** non-tone audio by **`goto tone_mixing`**, which **skipped** **`active_voices`** (loaded/streamed sounds). **`SituationInit`** sets **`default_graph`** → **`active_graph`** is normally non-NULL, so that omission broke **`SituationPlayLoadedSound`** until the **conditional latent-voice mix** and **`default_graph`** rule were added in implementation. The **canonical callback pipeline**, **policy options (graph-only vs dual-path)**, shutdown, and **remaining thread-safety / harness** items are documented in **`doc/plan/AUDIO_NODE_COMPLETION_PLAN.md`** § *Canonical miniaudio callback pipeline*. Treat that section as the **target system behavior** going forward; historical Step 2 text below is kept for audit trail.

---

## Risk Mitigation Strategy

1. **Manual backups before each destructive step** — copy the target file to `_junk/phase_h_backup/` before modifying
2. **One file at a time** — never edit two files without a compile check between them
3. **Compile after every step** — catch errors immediately, fix forward from a known-good state
4. **Additive before subtractive** — add the new code path first, verify it works, THEN remove the old one
5. **Tests updated incrementally** — remove mixer tests only after the mixer code is gone
6. **Deprecation wrapper phase** — old API functions become stubs that return errors before full removal

---

## Pre-Flight Checklist

- [ ] Create backup directory: `_junk/phase_h_backup/`
- [ ] Copy these files into it:
  - `sit/situation_api.h`
  - `sit/situation_impl_audio.h`
  - `sit/situation_impl_decl.h`
  - `tests/harness/test_audio.c`
  - `tests/harness/sit_test_registry.c`
- [ ] Verify current state compiles: `build_situation.bat opengl` → SUCCESS
- [ ] Verify tests pass: `build\sit_test.exe --module audio` → 119 passed, 0 failed

---

## Step 1: Add `active_graph` Infrastructure (ADDITIVE ONLY)

**Files touched**: `sit/situation_impl_decl.h`, `sit/situation_api.h`  
**Risk**: None — purely additive, no existing code modified  
**Compile check**: YES

### 1A: Add `active_graph` field to `_SituationAudioState`

In `sit/situation_impl_decl.h`, find the `_SituationAudioState` struct and add:

```c
// Node graph integration (Phase H)
SituationAudioGraph*    active_graph;       // Currently active processing graph (NULL = legacy path)
SituationAudioGraph*    default_graph;      // Auto-created minimal graph (Sound Source + Tone Synth → Mixer)
```

### 1B: Add `SituationSetActiveGraph` API declaration

In `sit/situation_api.h`, add near the Node Graph section:

```c
SITAPI SituationError SituationSetActiveGraph(SituationAudioGraph* graph);  // Set the active audio processing graph (replaces default).
SITAPI SituationAudioGraph* SituationGetActiveGraph(void);                  // Get the currently active audio processing graph.
```

### 1C: Implement `SituationSetActiveGraph` / `SituationGetActiveGraph`

In `sit/situation_impl_audio.h`, add the implementation (at the end of the file, before the `#endif`):

```c
SITAPI SituationError SituationSetActiveGraph(SituationAudioGraph* graph) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    sit_audio.active_graph = graph;
    return SITUATION_SUCCESS;
}

SITAPI SituationAudioGraph* SituationGetActiveGraph(void) {
    if (!SituationIsInitialized()) return NULL;
    return sit_audio.active_graph;
}
```

### Compile check: `build_situation.bat opengl`

---

## Step 2: Wire `SituationProcessGraph` into Audio Callback (ADDITIVE)

**Files touched**: `sit/situation_impl_audio.h`  
**Risk**: Low — adds a NEW code path that only activates when `active_graph != NULL`  
**Compile check**: YES

### What to do:

In `sit_miniaudio_data_callback()`, add a new block **before** the existing mixer check:

```c
// --- [Phase H] Node Graph Processing ---
// If an active graph is set, process it first (replaces mixer path)
if (pGs->active_graph) {
    extern const SituationDeviceFunctions g_device_function_table[];
    extern const int g_device_function_table_count;
    SituationProcessGraph(pGs->active_graph, (float*)pOutput, frameCount,
                          g_device_function_table, g_device_function_table_count);
    // Skip the legacy mixer path — graph handles everything
    goto tone_mixing;
}
```

This goes BEFORE the `mtx_lock(&pGs->audio_queue_mutex)` / `active_mixer` check. The logic is:
- If `active_graph` is set → use node graph → skip mixer → still mix tones
- If `active_graph` is NULL → fall through to existing mixer/legacy path (unchanged)

### Why this is safe:

`active_graph` starts as NULL (calloc'd struct). Nothing sets it yet. So the new code path is dead code until Step 4 activates it. The existing mixer path is completely untouched.

### Compile check: `build_situation.bat opengl`
### Test check: `build_tests.bat` + `build\sit_test.exe --module audio` (should still pass — graph path not activated)

---

## Step 3: Create Default Graph During Init (ADDITIVE)

**Files touched**: `sit/situation_impl_audio.h` (or `sit/situation_impl_ctrl.h` if init is there)  
**Risk**: Low — creates a graph but doesn't activate it yet  
**Compile check**: YES

### What to do:

Find where audio initialization happens (after `ma_device_start`). Add a function:

```c
static void _SituationCreateDefaultGraph(void) {
    _SituationEnsureRegistryInit();
    
    SituationAudioGraph* graph = SituationCreateGraph();
    if (!graph) return;
    
    // Create minimal nodes: Tone Synth + Sound Source → Mixer
    SituationNodeHandle tone_synth = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle sound_src  = SITUATION_INVALID_NODE_HANDLE;
    SituationNodeHandle mixer      = SITUATION_INVALID_NODE_HANDLE;
    
    SituationCreateNode(graph, SITUATION_NODE_TONE_SYNTH, &tone_synth);
    SituationCreateNode(graph, SITUATION_NODE_SOUND_SOURCE, &sound_src);
    SituationCreateNode(graph, SITUATION_NODE_MIXER, &mixer);
    
    // Patch: Tone Synth → Mixer input 0, Sound Source → Mixer input 1
    if (tone_synth != SITUATION_INVALID_NODE_HANDLE && mixer != SITUATION_INVALID_NODE_HANDLE) {
        SituationCreatePatch(graph, tone_synth, 0, mixer, 0, false);
    }
    if (sound_src != SITUATION_INVALID_NODE_HANDLE && mixer != SITUATION_INVALID_NODE_HANDLE) {
        SituationCreatePatch(graph, sound_src, 0, mixer, 1, false);
    }
    
    sit_audio.default_graph = graph;
    // NOTE: Do NOT set active_graph yet — that happens in Step 4
}
```

Call this at the end of audio init (after device is started).

### Compile check: `build_situation.bat opengl`
### Test check: All audio tests still pass (graph created but not activated)

---

## Step 4: Activate Default Graph (THE SWITCH)

**Files touched**: `sit/situation_impl_audio.h`  
**Risk**: MEDIUM — this is where audio routing changes  
**Compile check**: YES  
**Audio verification**: Play a tone after init, confirm it still works

### What to do:

At the end of `_SituationCreateDefaultGraph()`, add:

```c
sit_audio.active_graph = graph;  // Activate the node graph path
```

### What this means:

- The audio callback now takes the `active_graph` path (Step 2)
- `SituationProcessGraph()` processes the default graph (Tone Synth + Sound Source → Mixer → output)
- The legacy mixer path is skipped (but still exists in code)
- Tone pool mixing still runs (after `tone_mixing:` label)

### CRITICAL: Tone pool routing

The tone pool (`SituationPlayTone`) currently generates audio directly in the callback's `tone_mixing:` section. This still works because that section runs regardless of which path (graph or mixer) was taken. So `SituationPlayTone(440, ...)` still produces audio.

However, the Tone Synth NODE in the default graph is separate from the tone pool. The tone pool is the legacy system. For now, both coexist:
- Tone pool → direct mixing in callback (always runs)
- Tone Synth node → part of graph (for users who build custom graphs)

This is intentional — backward compatibility. The tone pool can be deprecated later.

### Compile check: `build_situation.bat opengl`
### Test check: `build\sit_test.exe --module audio` — verify all tests still pass
### Manual check: If possible, run an example that plays a tone

---

## Step 5: Deprecate Mixer API Functions (STUBS)

**Files touched**: `sit/situation_impl_audio.h`  
**Risk**: Low — functions still exist, just return errors/no-ops  
**Compile check**: YES

### What to do:

Replace the mixer function BODIES (not declarations) with deprecation stubs. The API declarations in `situation_api.h` stay for now (so the DLL still exports them). But the implementations become:

```c
SITAPI SituationAudioMixer* SituationCreateMixer(void) {
    // DEPRECATED: Use SituationCreateGraph() + node graph API instead
    return NULL;
}

SITAPI void SituationDestroyMixer(SituationAudioMixer* mixer) {
    // DEPRECATED: Use SituationDestroyGraph() instead
    (void)mixer;
}

SITAPI SituationAudioTrack* SituationAddTrack(SituationAudioMixer* mixer, const char* name) {
    (void)mixer; (void)name;
    return NULL;  // DEPRECATED
}
// ... etc for all mixer functions
```

### Why stubs first:

- The DLL still exports the symbols (no link errors for existing users)
- Tests that call mixer functions will get NULL/error returns
- We can verify the DLL builds and links before removing the code
- If something goes wrong, we can restore the implementations from backup

### Compile check: `build_situation.bat opengl`

---

## Step 6: Update Test Harness (Remove Mixer Tests)

**Files touched**: `tests/harness/test_audio.c`, `tests/harness/sit_test_registry.c`  
**Risk**: Low — only test code  
**Compile check**: YES (build_tests.bat)

### What to do:

Remove or comment out the mixer test functions:
- `test_create_destroy_mixer`
- `test_mixer_add_remove_track`
- `test_mixer_track_volume_pan`
- `test_mixer_track_mute_solo`
- `test_mixer_master_volume`
- `test_mixer_get_aux_bus`
- `test_mixer_track_send_post_fader`
- `test_mixer_track_send_pre_fader`
- `test_mixer_set_track_output`
- `test_mixer_route_sound_to_track`
- `test_mixer_track_eq_enable`
- `test_mixer_track_eq_disable`
- `test_mixer_track_dynamics_compressor`
- `test_mixer_track_dynamics_limiter`
- `test_mixer_track_dynamics_gate`
- `test_mixer_track_dynamics_disable`
- `test_mixer_track_sidechain`
- `test_mixer_track_meter`
- `test_mixer_get_mixer_graph`
- `test_mixer_session_save`
- `test_mixer_session_load`
- `test_mixer_bind_to_device`
- `test_mixer_find_best_device`

Also remove their registrations from the test registry.

### Compile check: `build_tests.bat`
### Test check: `build\sit_test.exe --module audio` — should pass with fewer tests

---

## Step 7: Remove Mixer Implementation Code

**Files touched**: `sit/situation_impl_audio.h`  
**Risk**: HIGH — this is the big deletion (~800+ lines)  
**Compile check**: YES  
**BACKUP FIRST**: Verify `_junk/phase_h_backup/situation_impl_audio.h` exists

### What to remove:

1. **Struct definitions** (if in this file): `SituationAudioBus`, `SituationAudioTrack`, `SituationAudioMixer`
2. **miniaudio node wrappers**: `_situation_dynamics_process`, `SituationDynamicsNodeInit`, `SituationPannerNodeInit`, `SituationMeterNodeInit` and their configs
3. **Mixer creation/destruction**: `SituationCreateMixer` body (now a stub), `_SituationInitTrack_NoLock`, etc.
4. **Track operations**: All `SituationSetTrack*` implementations
5. **Bus operations**: `SituationGetAuxBus`, `SituationSetTrackSend`, `SituationSetTrackOutput`
6. **Session save/load**: `SituationSaveMixerSession`, `SituationLoadMixerSession`
7. **Insert chain processing**: `_SituationProcessInsertChain`, `_SituationProcessAuxFXChain`
8. **The old mixer path in the callback**: The `active_mixer` check block (already bypassed by Step 2/4)

### What to KEEP:

- `sit_miniaudio_data_callback` (with the new graph path + tone mixing)
- Sound loading/playback (`SituationLoadSoundFromFile`, `SituationPlayLoadedSound`, etc.)
- Tone pool (`SituationPlayTone`, `SituationPlayToneEx`, etc.)
- Audio device management (`SituationGetAudioDevices`, `SituationPauseAudioDevice`, etc.)
- Capture (`SituationStartAudioCapture`, `SituationStopAudioCapture`)
- Master volume (`SituationGetAudioMasterVolume`, `SituationSetAudioMasterVolume`)
- Audio processor callbacks (`SituationAttachAudioProcessor`, `SituationDetachAudioProcessor`)
- The `_SituationCreateDefaultGraph` function (added in Step 3)
- `SituationSetActiveGraph` / `SituationGetActiveGraph` (added in Step 1)

### Compile check: `build_situation.bat opengl`

---

## Step 8: Remove Mixer Struct Definitions from `situation_impl_decl.h`

**Files touched**: `sit/situation_impl_decl.h`  
**Risk**: Medium — if anything still references these types, compile will fail  
**Compile check**: YES

### What to remove:

- `struct SituationAudioBus` (if defined here)
- `struct SituationAudioTrack` (if defined here)
- `struct SituationAudioMixer` (if defined here)
- `SIT_MAX_TRACKS`, `SIT_MAX_AUX_BUSES` defines
- `active_mixer` field from `_SituationAudioState`

### Compile check: `build_situation.bat opengl`

---

## Step 9: Remove Mixer API Declarations from `situation_api.h`

**Files touched**: `sit/situation_api.h`  
**Risk**: Medium — this is the public-facing break  
**Compile check**: YES (DLL + tests)

### What to remove:

The entire `// --- Mixer API (Phase 1) ---` section and `// --- Mixer API (Phase 4) ---` section:
- `SituationCreateMixer` / `SituationDestroyMixer`
- `SituationAddTrack` / `SituationRemoveTrack` / `SituationSetTrackName`
- `SituationRouteSoundToTrack`
- `SituationSetTrackVolume` / `SituationSetTrackPan` / `SituationSetTrackMute` / `SituationSetTrackSolo`
- `SituationGetAuxBus` / `SituationSetTrackSend` / `SituationSetTrackOutput`
- `SituationSetTrackEQ` / `SituationSetTrackDynamics` / `SituationSetTrackSideChain`
- `SituationSetMasterVolume` / `SituationGetMasterVolume` (mixer-specific)
- `SituationSaveMixerSession` / `SituationLoadMixerSession`
- `SituationInsertEffect` / `SituationRemoveEffect`
- `SituationGetTrackMeter`
- `SituationGetMixerGraph`
- `SituationBindMixerToDevice` / `SituationBindCaptureDevice` / `SituationFindBestDevice`

Also remove the forward declarations for `SituationAudioMixer`, `SituationAudioTrack`, `SituationAudioBus` types.

### Compile check: `build_situation.bat opengl` + `build_tests.bat`
### Test check: `build\sit_test.exe --module audio`

---

## Step 10: Final Validation

- [ ] `build_situation.bat opengl` — DLL builds clean
- [ ] `build_tests.bat` — test harness builds clean
- [ ] `build\sit_test.exe --module audio` — all remaining audio tests pass
- [ ] `build\sit_test.exe --module filesystem` — no regressions
- [ ] `build\sit_test.exe --module threading` — no regressions
- [ ] `build\sit_test.exe --module core` — no regressions
- [ ] Verify `SituationPlayTone` still works (tone pool path)
- [ ] Verify `SituationLoadSoundFromFile` + `SituationPlayLoadedSound` still works

---

## Step 11: Version Bump & Changelog

- [ ] Bump `SITUATION_VERSION_MINOR` (breaking change) in `situation.h`
- [ ] Add changelog entry in `doc/UPDATELOG.md`
- [ ] Update `doc/plan/AUDIO_NODE_COMPLETION_PLAN.md` — mark Phase H complete

---

## Rollback Plan

If any step fails catastrophically:

1. Stop immediately
2. Copy the backup file from `_junk/phase_h_backup/` back to its original location
3. Rebuild: `build_situation.bat opengl`
4. Verify: `build\sit_test.exe --module audio` passes
5. Diagnose what went wrong before retrying

---

## Files Modified (Summary)

| File | Action | Step |
|------|--------|------|
| `sit/situation_impl_decl.h` | Add `active_graph`/`default_graph` fields, later remove mixer structs | 1A, 8 |
| `sit/situation_api.h` | Add `SetActiveGraph`/`GetActiveGraph`, later remove mixer API | 1B, 9 |
| `sit/situation_impl_audio.h` | Add graph path to callback, add default graph, stub mixer, remove mixer | 1C, 2, 3, 4, 5, 7 |
| `tests/harness/test_audio.c` | Remove mixer tests | 6 |
| `tests/harness/sit_test_registry.c` | Remove mixer test registrations | 6 |
| `situation.h` | Version bump | 11 |
| `doc/UPDATELOG.md` | Changelog | 11 |

---

## Estimated Time

| Step | Time | Cumulative |
|------|------|-----------|
| Pre-flight (backups) | 5 min | 5 min |
| Step 1 (add active_graph) | 15 min | 20 min |
| Step 2 (wire callback) | 15 min | 35 min |
| Step 3 (default graph) | 20 min | 55 min |
| Step 4 (activate) | 5 min | 60 min |
| Step 5 (stub mixer) | 30 min | 90 min |
| Step 6 (update tests) | 20 min | 110 min |
| Step 7 (remove impl) | 45 min | 155 min |
| Step 8 (remove structs) | 15 min | 170 min |
| Step 9 (remove API decls) | 10 min | 180 min |
| Step 10 (validation) | 15 min | 195 min |
| Step 11 (version bump) | 10 min | 205 min |

**Total**: ~3.5 hours

---

## Decision Points (Pause & Confirm)

These are points where I'll stop and ask for confirmation before proceeding:

1. **After Step 4** — "The graph path is active. Tone pool still works. Mixer is bypassed but code still exists. Ready to stub the mixer functions?"
2. **After Step 6** — "Tests updated, mixer tests removed. Ready to delete the mixer implementation (~800 lines)?"
3. **After Step 9** — "All mixer code removed. API declarations gone. Ready for final validation and version bump?"

---

## What This Does NOT Touch

- `ext/miniaudio.h` — stays completely untouched
- Sound loading/playback API — unchanged
- Tone synthesis API — unchanged
- Audio capture API — unchanged
- Audio device enumeration — unchanged
- Node graph API (Phases 1-5) — unchanged
- Any non-audio code — unchanged
