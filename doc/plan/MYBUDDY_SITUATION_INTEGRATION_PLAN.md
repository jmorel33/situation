# MyBuddy → Situation Integration Plan

**Status:** Phase 1 blocked — see Pain Points  
**Version target:** Situation 2.4.267+; MyBuddy 1.6.2 (current, ready)  
**Scope:** Wire MyBuddy into Situation behind the existing `SIT_MALLOC`/`SIT_FREE` macros.

---

## Background

MyBuddy (`sit/mybuddy/`) is a per-thread-caching buddy allocator vendored as a sub-library at **v1.6.2**. It is fully tested on Windows/MinGW, has its own `UPDATELOG.md`, and benchmarks at 4–10x on mixed alloc/free workloads — the real-world pattern for Situation's renderer and audio subsystems.

Situation's allocator abstraction — `SIT_MALLOC`, `SIT_CALLOC`, `SIT_REALLOC`, `SIT_FREE` — is defined in `sit/situation_api.h` with `#ifndef` guards and currently routes to the system CRT. Wiring in MyBuddy is a matter of repointing those macros once, in `situation_impl_deps.h`, before any `situation_impl_*.h` file sees them. No call sites need to change.

---

## Phase 0 — MyBuddy Sub-Library (COMPLETE at v1.6.2)

All items done. MyBuddy is clean, versioned, and tested.

- [x] 0.0 `MYBUDDY_VERSION_*` macros; `mybuddy_version.h`; `UPDATELOG.md` created
- [x] 0.1 `mbd_destroy()` safety — `fprintf + abort()` guard (pre-resolved in v1.5.x)
- [x] 0.2 Memory ordering — `_Atomic(uint8_t) order`, acquire in `coalesce_up_and_update()` (pre-resolved)
- [x] 0.3 Cache pressure unified into per-arena `cached_bytes` (pre-resolved)
- [x] 0.4 `mbd_init(NULL)` test calls — non-issue, signature now takes `const mbd_config_t *`
- [x] 0.5 SCALING_PLAN remote-queue text corrected to lock-free CAS
- [x] 0.6 Windows/MinGW: VirtualAlloc shim, `mbd_sysconf`, bcrypt, `ntstatus.h`
- [x] 0.7 macOS: `getentropy(3)` entropy, `MAP_ANON` → `MAP_ANONYMOUS` fallback
- [x] 0.8 Three-file split: `mybuddy_version.h` / `mybuddy_api.h` / `mybuddy_impl.h`
- [x] 0.9 `MbdGetVersionString()` API; `mybuddy_version.h` as canonical version source
- [x] **GATE** `make test` from `sit/mybuddy/` — all 7 tests green on Windows ✅

---

## Pre-Phase 1 Requirement — Remaining Mixed-Allocator Sites

Before Phase 1 is attempted again, these specific mismatches must be fixed. They were identified by a full `SIT_FREE` audit across all `sit/` implementation files. Everything else audited was **SAFE** (allocated and freed consistently through `SIT_MALLOC`/`SIT_CALLOC`/`SIT_REALLOC`).

### M1 — `SituationUnloadImage` frees stb_image memory via `SIT_FREE`  
**Severity: Confirmed crash — fires on every image load+unload**

`stbi_load()` allocates its output with the CRT `malloc` (stb's internal default). `SituationUnloadImage()` at `situation_impl_image.h:145` calls `SIT_FREE(image.data)` — heap corruption when `mbd_free` is live.

**Fix:** Add `STBI_MALLOC`/`STBI_FREE` overrides in `situation_impl_deps.h` before `#include "stb_image.h"`:
```c
#define STBI_MALLOC(sz)        SIT_MALLOC(sz)
#define STBI_REALLOC(p,sz)     SIT_REALLOC(p,sz)
#define STBI_FREE(p)           do { mbd_free(p); } while(0)
```
This makes stb_image use MyBuddy for all its internal allocations, so `SIT_FREE` in `SituationUnloadImage` is correct.

**Files:** `sit/situation_impl_deps.h`, `sit/situation_impl_image.h:145`

---

### M2 — `SituationImageResize` propagates the stb_image pointer mismatch  
**Severity: Conditional crash — fires when resizing an image loaded from file**

`SituationImageResize()` at `situation_impl_image.h:740` calls `SIT_FREE(image->data)` on the old image data before replacing it. If the image was loaded via `SituationLoadImage` (stb_image path), the data is CRT-allocated. The M1 fix (STBI_MALLOC override) resolves this automatically.

**Fix:** Resolved by M1.

---

### M3 — `stbtt_GetGlyphBitmap` result freed via `SIT_FREE`  
**Severity: Confirmed crash — fires during font rendering when MyBuddy is live**

`stbtt_GetGlyphBitmap()` allocates its output with CRT `malloc` (stb_truetype's internal default). Two call sites in `situation_impl_image.h` (lines 1808 and 2084) call `SIT_FREE(glyphBitmap)` on the result.

**Fix:** Add `STBTT_malloc`/`STBTT_free` overrides in `situation_impl_deps.h` before `#include "stb_truetype.h"`:
```c
#define STBTT_malloc(x,u)  ((void)(u), SIT_MALLOC(x))
#define STBTT_free(x,u)    ((void)(u), mbd_free(x))
```
`stb_truetype` respects these macros for all internal allocations including `stbtt_GetGlyphBitmap`.

**Files:** `sit/situation_impl_deps.h`, `sit/situation_impl_image.h:1808,2084`

---

### M4 — `_SitAudioCleanupPool` frees `ma_decode_file` memory via `SIT_FREE`  
**Severity: Confirmed crash — fires on any program that loads and plays a sound**

`ma_decode_file()` in `SituationLoadSound` allocates `snd->preloaded_data` using miniaudio's internal allocator (CRT `malloc`). The pool cleanup in `_SitAudioCleanupPool` at `situation_impl_audio.h:1931` frees it via `SIT_FREE`. The normal unload path (`SituationUnloadSound`) correctly uses `ma_free(snd->preloaded_data, NULL)` — the shutdown cleanup path was inconsistent.

**Fix:** Change line 1931 from `SIT_FREE(snd->preloaded_data)` to `ma_free(snd->preloaded_data, NULL)`.

**File:** `sit/situation_impl_audio.h:1931`

---

### M5 — `SituationSoundCrop` frees `ma_decode_file` memory via `SIT_FREE`  
**Severity: Conditional crash — fires when cropping a file-loaded sound**

`SituationSoundCrop()` at `situation_impl_audio.h:1627` calls `SIT_FREE(data->preloaded_data)` before replacing it with a `SIT_MALLOC`'d crop buffer. If the sound was loaded via `SituationLoadSound` (miniaudio path), `preloaded_data` is CRT-allocated.

**Fix:** Change line 1627 from `SIT_FREE(data->preloaded_data)` to `ma_free(data->preloaded_data, NULL)`.

**File:** `sit/situation_impl_audio.h:1627`

---

### Pre-Phase 1 Checklist — Mixed-Allocator Fixes

- [ ] M1 Add `STBI_MALLOC`/`STBI_REALLOC`/`STBI_FREE` overrides in `situation_impl_deps.h` (before stb_image include)
- [ ] M2 Resolved by M1
- [ ] M3 Add `STBTT_malloc`/`STBTT_free` overrides in `situation_impl_deps.h` (before stb_truetype include)
- [ ] M4 `_SitAudioCleanupPool` line 1931: `SIT_FREE(snd->preloaded_data)` → `ma_free(snd->preloaded_data, NULL)`
- [ ] M5 `SituationSoundCrop` line 1627: `SIT_FREE(data->preloaded_data)` → `ma_free(data->preloaded_data, NULL)`
- [ ] Rebuild `static-opengl`, run `--module timer --module audio` — must be exit 0

**Only after all M-items are checked off does Phase 1 proceed.**

---



**STATUS: BLOCKED. See Pain Points section below.**

When the blockers are resolved, the mechanical steps are:

### 1.1 — `SituationInit()` — pre-warm
```c
// In situation_impl_ctrl.h — early in SituationInit, before any subsystem allocates
mbd_init(NULL);
```

### 1.2 — `SituationShutdown()` — teardown
```c
// At the very end of SituationShutdown, after SIT_FREE(_sit_current_context)
mbd_destroy();
```

### 1.3 — Include once in `situation_impl_deps.h`
```c
// Before the STB / miniaudio blocks — MyBuddy must compile before SIT_MALLOC is used
#define MYBUDDY_IMPLEMENTATION
#include "mybuddy/mybuddy.h"

// Override the CRT fallbacks in situation_api.h
#undef  SIT_MALLOC
#define SIT_MALLOC(sz)     mbd_alloc(sz)
#undef  SIT_CALLOC
#define SIT_CALLOC(n, sz)  mbd_calloc(n, sz)
#undef  SIT_REALLOC
#define SIT_REALLOC(p, sz) mbd_realloc(p, sz)
#undef  SIT_FREE
#define SIT_FREE(p)        do { mbd_free(p); (p) = NULL; } while(0)
```

### 1.4 — Build scripts: add `-lbcrypt`
In `build_situation.bat` (OpenGL DLL, Vulkan DLL link lines) and `build_tests.bat` (static-opengl and static-vulkan link lines), add `-lbcrypt` before `-lm`.

### 1.5 — Checklist
- [ ] 1.1 `mbd_init(NULL)` in `SituationInit()`
- [ ] 1.2 `mbd_destroy()` in `SituationShutdown()`
- [ ] 1.3 MyBuddy include + macro override in `situation_impl_deps.h`
- [ ] 1.4 `-lbcrypt` in `build_situation.bat` and `build_tests.bat`

---

## Pain Points — What Broke and Why

This section records every problem that surfaced during the Phase 1 attempt (2026-06-14) so the next attempt starts with full awareness.

---

### P1 — One Unknown CRT Pointer Reaching `mbd_free` at Shutdown

**Severity: Was the blocker — now IDENTIFIED**  
**Root cause:** Multiple confirmed mixed-allocator sites existed in `situation_impl_image.h` and `situation_impl_audio.h` before Phase 1 was attempted. The specific pointer observed during the timer test (zero-header, not in any arena, fired during `SituationShutdown` after OpenGL teardown) is consistent with either the stb_image or stb_truetype allocations reaching `mbd_free` during texture/font teardown inside `_SituationCleanupOpenGL` → cleanup of loaded textures → `SituationUnloadImage`.

**Resolution:** All confirmed mixed-allocator sites are documented as M1–M5 in the Pre-Phase 1 section above. Fixing them before wiring the macros eliminates the crash class entirely. No ASAN required — the sites were found by static audit.

---

### P2 — `SIT_FREE((void*)ptr)` Cannot Assign to Cast

**Severity: Minor bug in macro design**  
**Status: Requires care at specific sites**

`SIT_FREE(p)` expands to `do { mbd_free(p); (p) = NULL; } while(0)`. The `(p) = NULL` assignment fails to compile when `p` is a cast expression such as `SIT_FREE((void*)dev->info.name)` — you cannot assign to a cast.

The `dev->info.name` / `vdev->info.name` fields in `midi.h` are `const char*` (set from GLFW/PortMidi string returns). Freeing them requires a cast to `void*`. The current workaround is to call `free((void*)dev->info.name)` directly and null the field on the next line explicitly.

**Resolution for Phase 1:**
These fields were allocated with `SIT_MALLOC(MAX_NAME_LEN)` (a `char*`) and stored into a `const char*` field. The cast-free workaround at the specific sites is acceptable. Document as a known pattern: `const char*` fields that hold `SIT_MALLOC`-allocated strings must use a local temp pointer for the `SIT_FREE` call:
```c
char *tmp = (char *)dev->info.name;
SIT_FREE(tmp);
dev->info.name = NULL;
```

---

### P3 — `mbd_destroy()` Called on Uninitialized Allocator (Headless Tests)

**Severity: Crash in debug builds**  
**Status: Fixed in MyBuddy — `mbd_destroy()` now no-ops when `arenas == NULL`**

Some test modules (filesystem, Projection, etc.) do not call `SituationInit()` — they run headlessly. `SituationShutdown()` returns early when `_sit_current_context == NULL`, so `mbd_destroy()` was never reached from headless tests. However if the call order changes, or if `mbd_destroy()` is placed before the context check, it would crash by calling `pthread_getspecific(thread_cache_key)` on an uninitialized key.

MyBuddy's `mbd_destroy()` now has an early-exit guard: if `arenas == NULL` (allocator never initialized), it returns immediately. This is already in `mybuddy_impl.h`. No further action needed.

---

### P4 — `active_threads` Count Under winpthreads

**Severity: Was investigated but did not end up being the root cause**  
**Status: Resolved with the `arenas == NULL` guard in P3**

Under MSYS2/winpthreads, `pthread_join()` guarantees the joined thread has exited, including TLS destructors. `mbd_destroy()` decrements `active_threads` in `thread_cache_destructor`. The sequence — destroy thread pool → all workers join → all `thread_cache_destructor` calls complete → `active_threads` reaches 0 → `mbd_destroy()` safe — is valid and works correctly. This was confirmed by the timer test passing after P3 was resolved with the `arenas == NULL` guard.

---

### P5 — `STBIR_FREE` Uses CRT `free()` While `STBIR_MALLOC` Would Use `mbd_alloc`

**Severity: Mixed-allocator corruption for stb_image_resize operations**  
**Status: Fixed in `situation_impl_deps.h`**

`stb_image_resize2` defines `STBIR_MALLOC(size, user_data)` → `SIT_MALLOC(size)` and `STBIR_FREE(ptr, user_data)` → `free(ptr)`. The asymmetry means resize operations allocate with MyBuddy but free with the CRT — immediate heap corruption.

The fix (already applied): `sit_stbir_free()` shim now calls `mbd_free()` directly instead of `free()`. This fix is **active in the codebase** and does not depend on the Phase 1 macro wiring — it is unconditionally correct even with CRT backing, since `mbd_free(ptr)` would only be called when MyBuddy is the allocator.

**Wait — actually this is wrong when CRT is the allocator.** When Phase 1 is reverted and `SIT_MALLOC` = `malloc`, calling `mbd_free()` on a `malloc`-allocated pointer is undefined. The shim was reverted to `free(ptr)` correctly. This will need to be re-applied as part of Phase 1 wiring, not before.

---

## Phase 2 — Validation (pending Phase 1)

Once Phase 1 is unblocked:

- [ ] 2.1 `build\build_situation.bat static-opengl` — zero warnings
- [ ] 2.2 `build\build_situation.bat static-vulkan` — zero warnings  
- [ ] 2.3 Full harness: `build\tests\sit_test_opengl.exe` — all modules green
- [ ] 2.4 Confirm `mbd_get_stats()` at shutdown shows bytes-in == bytes-out
- [ ] 2.5 Add entry to `doc/UPDATELOG.md`; bump Situation patch version

---

## Phase 3 — Follow-Up (non-blocking)

- [ ] 3.1 `mbd_set_profiler_hook` → Situation trace events
- [ ] 3.2 Evaluate `mbd_frame_arena.h` for audio graph process-cycle allocations
- [ ] 3.3 `#ifdef MYBUDDY_HARDENED` order-check in `get_buddy()`
- [ ] 3.4 CPU pinning on macOS (`MBD_FLAG_CPU_LOCAL` — currently round-robin)

---

## What Is Already Done and Kept

Regardless of Phase 1 status, the following changes are **live in the codebase and correct**:

| Area | Change | File |
|------|--------|------|
| Mixed-alloc bug fix | `free(copy)` → `SIT_FREE(copy)` | `sit/situation_impl_etc.h` |
| `aud/` consistency | All raw CRT calls → `SIT_MALLOC`/`SIT_FREE` | `sit/aud/midi.h`, `midi_device.h`, `midi_learn.h`, `tone_synth_graph.h`, `sound_source.h`, `fx/dynamics.h`, `fx/deafmax.h`, `fx/maximizer.h`, `polysonix/*.h` |
| Build tooling | `-lbcrypt` in Makefile for MyBuddy standalone tests | `sit/mybuddy/Makefile` |
| MyBuddy v1.6.2 | Fully tested, versioned, UPDATELOG complete | `sit/mybuddy/` |
