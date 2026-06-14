# Audio Bugfix Plan — SituationLoadSoundFromFile, SituationAddTrack & SituationRemoveTrack

**Date**: 2026-05-06  
**Severity**: High — blocks 13 of 33 audio tests + critical crash in public API  
**Risk Level**: Low — fixes are localized to `sit/situation_impl_audio.h`  
**Discovered by**: Test harness Phase 5 (`tests/harness/test_audio.c`)

---

## Bug #1: SituationLoadSoundFromFile Returns Error — ☑ FIXED

### Symptoms

- `SituationLoadSoundFromFile("_sit_test_sine.wav", SITUATION_AUDIO_LOAD_FULL, false, &sound)` returns a non-SUCCESS error code.
- The WAV file is valid (16-bit PCM, mono, 44100Hz, generated programmatically).
- Tone synthesis works fine (proves audio device is functional).
- `SituationGetAudioPlaybackSampleRate()` returns a valid value (proves `is_miniaudio_device_active` is true).
- All 13 tests that depend on a loaded sound fail.

### Root Cause

The preloaded code path never set `sound->is_initialized = true`:

```c
if (should_preload) {
    sound->total_frames = framesRead;
    sound->is_preloaded = true;
    // BUG: sound->is_initialized was never set!
} else {
    sound->is_streamed = true;
    sound->is_initialized = true;  // Only set here
}
```

### Fix Applied

```c
sound->total_frames = framesRead;
sound->is_preloaded = true;
sound->is_initialized = true;  // ADDED
```

---

## Bug #2: SituationAddTrack SIGSEGV (Null Device Pointer) — ☑ FIXED

### Symptoms

- `SituationCreateMixer()` returns a valid non-NULL pointer.
- `SituationAddTrack(mixer, "TestTrack")` crashes with SIGSEGV.

### Root Cause

`SituationCreateMixer()` never initialized `mixer->device`. Track init dereferences `mixer->device->sampleRate` → NULL dereference.

### Fix Applied

```c
// In SituationCreateMixer():
mixer->is_initialized = true;
mixer->device = &sit_audio.miniaudio_device;  // ADDED

// In _SituationInitTrack_NoLock():
uint32_t sr = mixer->device ? mixer->device->sampleRate : 48000;  // Defensive guard
```

---

## Bug #3: SituationRemoveTrack SIGSEGV (Use-After-Nullify) — ☐ TO FIX

### Symptoms

- `SituationCreateMixer()` succeeds.
- `SituationAddTrack(mixer, "name")` succeeds, returns valid track pointer.
- `SituationRemoveTrack(track)` crashes with SIGSEGV.
- `SituationDestroyMixer(mixer)` works fine (handles track cleanup internally without crash).

### Root Cause

Classic use-after-nullify bug in `SituationRemoveTrack`:

```c
SITAPI void SituationRemoveTrack(SituationAudioTrack* track) {
    if (!track || !track->is_active || !track->owner) return;

    mtx_lock(&track->owner->topology_mutex);   // (1) Lock via track->owner
    _SituationRemoveTrack_NoLock(track);        // (2) Sets track->owner = NULL !!!
    mtx_unlock(&track->owner->topology_mutex);  // (3) CRASH: track->owner is now NULL
}
```

Step (2) calls `_SituationRemoveTrack_NoLock` which ends with:
```c
track->is_active = false;
track->owner = NULL;  // <-- This nullifies the pointer used in step (3)
```

Step (3) then dereferences `track->owner` to unlock the mutex → SIGSEGV.

### Why SituationDestroyMixer Works

`SituationDestroyMixer` locks `mixer->topology_mutex` directly (not via `track->owner`), so it doesn't matter that `_SituationRemoveTrack_NoLock` nullifies `track->owner`:

```c
mtx_lock(&mixer->topology_mutex);              // Lock via mixer directly
for (int i=0; i<SIT_MAX_TRACKS; ++i) {
    if (mixer->tracks[i].is_active) {
        _SituationRemoveTrack_NoLock(&mixer->tracks[i]);  // Nullifies track->owner — doesn't matter
    }
}
mtx_unlock(&mixer->topology_mutex);            // Unlock via mixer directly — safe
```

### Proposed Fix

Save the mixer pointer before calling the inner function:

```c
SITAPI void SituationRemoveTrack(SituationAudioTrack* track) {
    if (!track || !track->is_active || !track->owner) return;

    SituationAudioMixer* mixer = track->owner;  // Save before it gets nullified
    mtx_lock(&mixer->topology_mutex);
    _SituationRemoveTrack_NoLock(track);
    _SituationUpdateSoloState(mixer);           // Recalculate solo/mute after removal
    mtx_unlock(&mixer->topology_mutex);
}
```

This is a one-line fix. The mutex is the same object — we just access it through a local variable instead of through the track's back-pointer.

### Verification

After fix:
1. Rebuild DLL: `build_situation.bat opengl`
2. Rebuild tests: `build_tests.bat opengl`  
3. Restore `SituationRemoveTrack` calls in `test_audio.c` mixer tests
4. Run: `build\sit_test.exe --module audio --verbose`
5. Expected: 33/33 passing, including `mixer_add_remove_track` with explicit `RemoveTrack` call

---

## Implementation Order

| # | Task | Risk | Effort | Status |
|---|------|------|--------|--------|
| 1 | Fix `is_initialized` flag for preloaded sounds | Low | 2 min | ☑ Done |
| 2 | Fix mixer NULL device pointer in `SituationCreateMixer()` | Low | 5 min | ☑ Done |
| 3 | Add null guard in `_SituationInitTrack_NoLock()` | None | 2 min | ☑ Done |
| 4 | Fix `SituationRemoveTrack` use-after-nullify | Low | 2 min | ☑ Done |
| 5 | Rebuild DLL | None | 1 min | ☑ Done |
| 6 | Restore `SituationRemoveTrack` in test_audio.c mixer tests | None | 2 min | ☑ Done |
| 7 | Re-run test harness — verify all 33 audio tests pass | None | 1 min | ☑ 33/33 passing |

---

## Verification

After all fixes:
```
build_situation.bat opengl
build_tests.bat opengl
build\sit_test.exe --module audio --verbose
```

Expected: 33/33 tests passing, 0 failures, 0 crashes — including explicit `SituationRemoveTrack` calls.

---

## Files to Modify

- `sit/situation_impl_audio.h` — All three fixes
- `tests/harness/test_audio.c` — Restore `SituationRemoveTrack` calls in mixer tests
- No API changes, no new public functions, no breaking changes.

---

**Author**: Kiro  
**Status**: ☑ All bugs fixed — plan complete
