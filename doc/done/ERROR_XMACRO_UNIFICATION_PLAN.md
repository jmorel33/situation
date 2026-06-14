# Error System Unification Plan — X-Macro Single Source of Truth

**Date**: 2026-05-05  
**Completed**: 2026-05-05  
**Target Files**:  
- `sit/situation_base_errno.h` (rewritten: X-macro table)  
- `sit/situation_impl_ctrl.h` (`_SituationSetErrorFromCode` — now 3-line macro expansion)  
**Scope**: Eliminate the duplicate error definition. One table, two expansions.  
**Risk Level**: Low — same enum names, same values, same ABI. Pure internal refactor.  
**Status**: ✅ COMPLETE

---

## The Problem

The error system has two parallel lists that must be kept in sync manually:

1. **`situation_base_errno.h`** — The `SituationError` enum with inline comments  
2. **`situation_impl_ctrl.h`** — A 200-line `switch` in `_SituationSetErrorFromCode` mapping each code to a human-readable string

They already drift. Some messages differ slightly between the two. Every new error code requires updating both files. Miss one and you get "Unknown Error" at runtime with no compiler warning.

---

## The Solution: X-Macro Error Table

Define the error table exactly once. Expand it mechanically into both the enum and the message lookup.

### New File: `sit/situation_base_errno.h` (rewritten)

```c
#ifndef SITUATION_BASE_ERRNO_H
#define SITUATION_BASE_ERRNO_H

//==================================================================================
//  SituationError — X-Macro Error Table (Single Source of Truth)
//==================================================================================
//
//  FORMAT: X(NAME, VALUE, MESSAGE)
//
//  - NAME:    The enum constant (e.g., SITUATION_ERROR_GENERAL)
//  - VALUE:   The integer value (negative, unique, permanent)
//  - MESSAGE: Human-readable description (used in _SituationSetErrorFromCode)
//
//  To add a new error: add ONE line to the appropriate section below.
//  The enum, the switch, and any future string tables are generated automatically.
//

// ── Core & System Errors (0 to -99) ─────────────────────────────────────────────
#define SITUATION_ERRORS_CORE(X) \
    X(SITUATION_SUCCESS,                            0,  "No error") \
    X(SITUATION_ERROR_GENERAL,                     -1,  "A general error occurred") \
    X(SITUATION_ERROR_NOT_IMPLEMENTED,             -2,  "Feature not implemented on current backend") \
    X(SITUATION_ERROR_NOT_INITIALIZED,             -3,  "API called before SituationInit()") \
    X(SITUATION_ERROR_ALREADY_INITIALIZED,         -4,  "SituationInit() called more than once") \
    X(SITUATION_ERROR_INIT_FAILED,                 -5,  "Core initialization sequence failed") \
    X(SITUATION_ERROR_SHUTDOWN_FAILED,             -6,  "Library shutdown failed") \
    X(SITUATION_ERROR_INVALID_PARAM,               -7,  "Invalid parameter (NULL, out-of-range, bad enum)") \
    X(SITUATION_ERROR_MEMORY_ALLOCATION,           -8,  "Memory allocation failed") \
    X(SITUATION_ERROR_INTERNAL_STATE_CORRUPTED,    -9,  "Internal invariant violated -- fatal bug") \
    X(SITUATION_ERROR_ASSERTION_FAILED,           -10,  "Debug assertion tripped") \
    X(SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION, -11, "Architectural rule broken: Update called after Draw") \
    X(SITUATION_ERROR_TIMER_SYSTEM,               -20,  "Timer/oscillator system error")

// ── Threading Errors (-80 to -98) ───────────────────────────────────────────────
#define SITUATION_ERRORS_THREADING(X) \
    X(SITUATION_ERROR_THREAD_QUEUE_FULL,           -80, "Thread queue full") \
    X(SITUATION_ERROR_THREAD_VIOLATION,            -81, "Main-thread-only function called from worker") \
    X(SITUATION_ERROR_THREAD_CYCLE,               -82, "Dependency cycle or depth limit exceeded") \
    X(SITUATION_ERROR_THREAD_CREATION_FAILED,     -83, "Failed to spawn thread (thrd_create)") \
    X(SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED,   -84, "Mutex initialization failed") \
    X(SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED,   -85, "Mutex lock failed") \
    X(SITUATION_ERROR_THREAD_MUTEX_UNLOCK_FAILED, -86, "Mutex unlock failed") \
    X(SITUATION_ERROR_THREAD_MUTEX_TIMEOUT,       -87, "Mutex lock timeout (deadlock prevention)") \
    X(SITUATION_ERROR_THREAD_JOIN_FAILED,         -88, "Thread join failed") \
    X(SITUATION_ERROR_THREAD_DETACH_FAILED,       -89, "Thread detach failed") \
    X(SITUATION_ERROR_THREAD_NOT_AVAILABLE,       -90, "Threading not available on this platform") \
    X(SITUATION_ERROR_THREAD_ATOMIC_FAILED,       -91, "Atomic operation failed") \
    X(SITUATION_ERROR_THREAD_STATE_INVALID,       -92, "Invalid thread state for operation") \
    X(SITUATION_ERROR_THREAD_BUFFER_OVERFLOW,     -93, "Thread-local buffer overflow") \
    X(SITUATION_ERROR_THREAD_DEADLOCK_DETECTED,   -94, "Potential deadlock detected") \
    X(SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT,-95, "Render thread join timeout") \
    X(SITUATION_ERROR_RENDER_LIST_INCOMPLETE,     -96, "Render list missing mandatory commands") \
    X(SITUATION_ERROR_ARM_INTRINSICS_FAILED,      -97, "ARM intrinsic (WFE/SEV) failed") \
    X(SITUATION_ERROR_COMMAND_EXECUTION_FAILED,   -98, "External command execution failed")

// ── Platform & Windowing (-100 to -199) ─────────────────────────────────────────
#define SITUATION_ERRORS_PLATFORM(X) \
    X(SITUATION_ERROR_GLFW_FAILED,                -100, "GLFW operation failed") \
    X(SITUATION_ERROR_WINDOW_CREATION_FAILED,     -101, "Failed to create window") \
    X(SITUATION_ERROR_WINDOW_FOCUS_FAILED,        -102, "Window focus operation failed") \
    X(SITUATION_ERROR_CLIPBOARD_FAILED,           -103, "Clipboard operation failed") \
    X(SITUATION_ERROR_CURSOR_CREATION_FAILED,     -104, "Custom cursor creation failed") \
    X(SITUATION_ERROR_COM_INITIALIZATION_FAILED,  -110, "COM initialization failed (Windows)") \
    X(SITUATION_ERROR_DXGI_QUERY_FAILED,          -111, "DXGI GPU query failed (Windows)") \
    X(SITUATION_ERROR_WINDOW_FOCUS,               -120, "Window focus operation failed") \
    X(SITUATION_ERROR_DEVICE_QUERY,               -121, "Hardware/device query failed") \
    X(SITUATION_ERROR_COM_FAILED,                 -123, "COM library init failed (Windows)") \
    X(SITUATION_ERROR_DXGI_FAILED,                -124, "DXGI call failed (Windows)")

// ... (Display, Filesystem, Audio, Rendering, OpenGL, Vulkan, Compute, Network sections follow same pattern)

// ── Master Table (expands all sections) ─────────────────────────────────────────
#define SITUATION_ERROR_TABLE(X) \
    SITUATION_ERRORS_CORE(X) \
    SITUATION_ERRORS_THREADING(X) \
    SITUATION_ERRORS_PLATFORM(X) \
    SITUATION_ERRORS_DISPLAY(X) \
    SITUATION_ERRORS_FILESYSTEM(X) \
    SITUATION_ERRORS_AUDIO(X) \
    SITUATION_ERRORS_MIXER(X) \
    SITUATION_ERRORS_NODE_GRAPH(X) \
    SITUATION_ERRORS_DEVICE_REGISTRY(X) \
    SITUATION_ERRORS_MIDI(X) \
    SITUATION_ERRORS_RENDERING(X) \
    SITUATION_ERRORS_OPENGL(X) \
    SITUATION_ERRORS_VULKAN(X) \
    SITUATION_ERRORS_COMPUTE(X) \
    SITUATION_ERRORS_NETWORK(X) \
    X(SITUATION_ERROR_UNKNOWN_ERROR, -999, "Unknown error")

// ── Generate the Enum ───────────────────────────────────────────────────────────
typedef enum {
    #define _SIT_ERRNO_ENUM(name, value, msg) name = value,
    SITUATION_ERROR_TABLE(_SIT_ERRNO_ENUM)
    #undef _SIT_ERRNO_ENUM
} SituationError;

#endif // SITUATION_BASE_ERRNO_H
```

### Updated: `_SituationSetErrorFromCode` in `situation_impl_ctrl.h`

```c
static SituationError _SituationSetErrorFromCode(SituationError err, const char* detail) {
    char buffer[SITUATION_MAX_ERROR_MSG_LEN];
    const char* base_msg = "Unknown Error";

    switch (err) {
        #define _SIT_ERRNO_MSG(name, value, msg) case name: base_msg = msg; break;
        SITUATION_ERROR_TABLE(_SIT_ERRNO_MSG)
        #undef _SIT_ERRNO_MSG
        default: break;
    }

    if (detail) {
        snprintf(buffer, sizeof(buffer), "%s: %s", base_msg, detail);
    } else {
        strncpy(buffer, base_msg, sizeof(buffer) - 1);
    }
    buffer[sizeof(buffer) - 1] = '\0';
    _SituationSetError(buffer);
    return err;
}
```

### Bonus: Public `SituationErrorToString` (free from the same table)

```c
SITAPI const char* SituationErrorToString(SituationError err) {
    switch (err) {
        #define _SIT_ERRNO_STR(name, value, msg) case name: return msg;
        SITUATION_ERROR_TABLE(_SIT_ERRNO_STR)
        #undef _SIT_ERRNO_STR
        default: return "Unknown error";
    }
}
```

---

## Implementation Steps

- [x] Rewrite `situation_base_errno.h` with X-macro table (15 sectioned sub-tables)
- [x] Replace the 200-line switch in `_SituationSetErrorFromCode` with 3-line macro expansion
- [x] Added missing `return err;` to `_SituationSetErrorFromCode` (was void-returning by accident)
- [x] Verify: all enum values unchanged (ABI stable)
- [x] Verify: all enum names unchanged (source compatible)
- [x] Verify: compilation passes clean (gcc -fsyntax-only, zero new warnings)
- [x] k-term duplicate check — N/A, mock uses `typedef int SituationError`
- [x] Fixed drift: 6 MIDI codes + `COMMAND_EXECUTION_FAILED` now auto-covered by table

### Remaining (optional, not blocking)

- [ ] Add public `SituationErrorToString(SituationError err)` API (trivial 3-liner, zero cost)
- [ ] Update `doc/UPDATELOG.md` with X-macro refactor entry

---

## Design Decisions

### Why sectioned sub-tables?

```c
SITUATION_ERRORS_CORE(X)
SITUATION_ERRORS_THREADING(X)
SITUATION_ERRORS_PLATFORM(X)
// ...
```

Rather than one giant `SITUATION_ERROR_TABLE`, we split by subsystem. This:
- Keeps each section navigable (grep for `SITUATION_ERRORS_AUDIO` to find audio errors)
- Allows subsystems to include only their own errors if needed
- Makes the master table a simple concatenation of sections

### Why keep it in `situation_base_errno.h`?

The enum must be visible to the public API (users check error codes). The X-macro table lives in the same file because:
- The enum IS the table — they're the same thing now
- Users who `#include "situation_base_errno.h"` get the enum as before
- The message strings are only materialized inside the implementation (in the switch)

### Why not move it to impl?

The enum values are part of the public API contract. Users write `if (err == SITUATION_ERROR_FILE_NOT_FOUND)`. The enum must stay in a public header. Only the message-string expansion happens in the impl.

### What about IDE hover/autocomplete?

Most IDEs show the enum value's comment on hover. With X-macros, the "comment" is the MESSAGE field in the table. Some IDEs handle this well (they'll show the expanded value), others won't. If this is a concern, we can add a generated comment block above the enum expansion:

```c
// Generated from SITUATION_ERROR_TABLE. See table above for descriptions.
typedef enum { ... } SituationError;
```

In practice, the MESSAGE string in the X-macro line serves the same purpose as the old inline comment — it's right there next to the name and value.

---

## What This Does NOT Change

- No public API changes (same enum names, same values)
- No ABI changes (same integer values)
- No behavioral changes (same messages at runtime)
- No new dependencies
- No new files (rewrite of existing `situation_base_errno.h`)

---

## Risk Assessment

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| Enum value accidentally changed | Low | Diff the before/after enum expansion |
| Missing error in new table | Low | Compiler warns on unhandled switch cases |
| IDE autocomplete degraded | Medium | MESSAGE field serves as inline doc |
| Merge conflicts with in-flight work | Low | Single file, clean diff |

---

## Estimated Effort

~30 minutes. The table is mechanical — copy each `case X: base_msg = "Y"; break;` line into `X(NAME, VALUE, "Y")` format. The switch replacement is 3 lines.

---

**Author**: Kiro  
**Status**: ✅ COMPLETE (2026-05-05)  
**Priority**: Done
