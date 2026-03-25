# Threading Troubleshooting Guide

**Date**: 2026-03-02  
**Purpose**: Quick reference for diagnosing threading issues

## The Problem We Had Before

**Symptom**: Threading test hung after first iteration with no error messages or diagnostic information.

**What We Didn't Know**:
- Was threading even available?
- Was the sleep function reliable?
- Which mutex operation failed (if any)?
- Was it a platform-specific issue?
- Were atomics working correctly?

**Result**: Hours of debugging with printf statements and trial-and-error.

---

## The Solution We Have Now

### 1. Immediate Capability Detection

**Before any threading code runs**, call the diagnostic function:

```c
#include "sit/aud/threading_diagnostics.h"

int main(void) {
    // Print full threading status
    SituationPrintThreadingStatus();
    
    // Or check programmatically
    SituationThreadingStatus status = SituationGetThreadingStatus();
    if (!status.available) {
        printf("ERROR: Threading not available on this platform\n");
        return 1;
    }
    
    if (!status.sleep_reliable) {
        printf("WARNING: Sleep implementation may be unreliable\n");
        printf("Recommendation: %s\n", status.sleep_impl);
    }
    
    // Continue with threading code...
}
```

**Output Example**:
```
========================================
Threading Status
========================================
Available:       YES
Platform:        Windows
Sleep Impl:      Windows Sleep (recommended)
Sleep Reliable:  YES
Max Threads:     64

Capabilities:
  C11 Threads:   YES
  C11 Atomics:   YES
  Mutex:         YES
  Platform Sleep:YES

Warnings:
  - Using tinycthread on Windows - recommend defining SITUATION_USE_PLATFORM_SLEEP
========================================
```

**What This Tells You Immediately**:
- ✅ Threading is available
- ✅ Platform is Windows
- ⚠️ Sleep implementation warning (this is what caused our bug!)
- ✅ All capabilities present
- ✅ Specific recommendation provided

### 2. Automatic Error Detection

**Every threading operation now has error checking**:

```c
// Old way (no error checking)
mtx_lock(&mutex);
// If this fails, you have no idea why

// New way (automatic error checking with debug logging)
#define SITUATION_DEBUG_THREADING  // Enable debug logging
#include "sit/aud/threading_diagnostics.h"

int result = mtx_lock(&mutex);
SITUATION_CHECK_MUTEX_LOCK(result, SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED);
// If this fails, you get:
// [Thread ERROR file.c:123] Mutex lock failed
// And the function returns with a specific error code
```

### 3. Specific Error Codes

**Before**: Generic errors or silent failures

**Now**: 11 specific thread error codes:

| Code | Error | Meaning |
|------|-------|---------|
| -80 | `SITUATION_ERROR_THREAD_QUEUE_FULL` | Thread queue exhausted |
| -81 | `SITUATION_ERROR_THREAD_VIOLATION` | Wrong thread called function |
| -82 | `SITUATION_ERROR_THREAD_CYCLE` | Dependency cycle detected |
| -83 | `SITUATION_ERROR_THREAD_CREATION_FAILED` | Thread spawn failed |
| -84 | `SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED` | Mutex init failed |
| -85 | `SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED` | Mutex lock failed |
| -86 | `SITUATION_ERROR_THREAD_MUTEX_UNLOCK_FAILED` | Mutex unlock failed |
| -87 | `SITUATION_ERROR_THREAD_MUTEX_TIMEOUT` | Deadlock prevention timeout |
| -88 | `SITUATION_ERROR_THREAD_JOIN_FAILED` | Thread join failed |
| -89 | `SITUATION_ERROR_THREAD_DETACH_FAILED` | Thread detach failed |
| -90 | `SITUATION_ERROR_THREAD_NOT_AVAILABLE` | Threading not supported |
| -91 | `SITUATION_ERROR_THREAD_ATOMIC_FAILED` | Atomic operation failed |
| -92 | `SITUATION_ERROR_THREAD_STATE_INVALID` | Invalid state for operation |
| -93 | `SITUATION_ERROR_THREAD_BUFFER_OVERFLOW` | Thread buffer overflow |
| -94 | `SITUATION_ERROR_THREAD_DEADLOCK_DETECTED` | Deadlock detected |

### 4. Debug Logging

**Enable detailed logging during development**:

```c
#define SITUATION_DEBUG_THREADING
#include "sit/aud/threading_diagnostics.h"

// Now every threading operation logs:
// [Thread file.c:123] Creating thread-safe graph
// [Thread file.c:145] Mutex locked
// [Thread file.c:167] Node created successfully (handle=0x00010000)
// [Thread ERROR file.c:189] Mutex lock failed
```

### 5. Timeout Protection

**Prevent deadlocks automatically**:

```c
// Try to lock with 1 second timeout
bool locked = SituationMutexTryLockTimeout(&mutex, 1000);
if (!locked) {
    printf("ERROR: Mutex lock timeout - possible deadlock\n");
    // Handle gracefully instead of hanging forever
}
```

### 6. Platform-Specific Sleep

**Use the right sleep function automatically**:

```c
// Old way (caused our bug)
struct timespec ts = {.tv_sec = 0, .tv_nsec = 5000000};
thrd_sleep(&ts, NULL);  // BUG: Hangs on Windows with tinycthread

// New way (automatic platform detection)
SITUATION_SLEEP_MS(5);  // Uses Windows Sleep() or POSIX usleep()
```

---

## Quick Diagnostic Checklist

When you encounter a threading issue, run through this checklist:

### Step 1: Check Capabilities
```bash
# Run the diagnostic test
./build/threading_diagnostic_test.exe
```

**Look for**:
- ❌ "Threading not available" → Platform doesn't support C11 threads
- ⚠️ "Sleep implementation may be unreliable" → Use platform-specific sleep
- ⚠️ Warnings about tinycthread → Known issues on this platform

### Step 2: Enable Debug Logging
```c
#define SITUATION_DEBUG_THREADING
```

**Recompile and run** - you'll see exactly where it fails:
- Last successful operation before hang/crash
- Which mutex/thread operation failed
- Specific error codes

### Step 3: Check Error Codes
```c
SituationNodeError err = SituationCreateNodeThreadSafe(...);
if (err != SITUATION_NODE_SUCCESS) {
    printf("Error code: %d\n", err);
    // Look up error code in table above
}
```

### Step 4: Test Mutex Timeout
```c
// If you suspect deadlock
bool locked = SituationMutexTryLockTimeout(&mutex, 1000);
if (!locked) {
    printf("DEADLOCK: Mutex held for >1 second\n");
    // Investigate what's holding the mutex
}
```

---

## Common Issues and Solutions

### Issue 1: Thread Hangs After First Iteration

**Symptoms**:
- Thread starts successfully
- Processes one iteration
- Hangs indefinitely

**Diagnosis**:
```bash
./build/threading_diagnostic_test.exe
```
Look for: "Sleep implementation may be unreliable"

**Solution**:
```c
// Use platform-specific sleep
SITUATION_SLEEP_MS(5);  // Instead of thrd_sleep()
```

**Root Cause**: tinycthread's `thrd_sleep()` has a bug on Windows

---

### Issue 2: Mutex Lock Fails Silently

**Symptoms**:
- No error messages
- Unexpected behavior
- Data corruption

**Diagnosis**:
```c
#define SITUATION_DEBUG_THREADING
```
Recompile - you'll see: `[Thread ERROR] Mutex lock failed`

**Solution**:
```c
// Use error checking macro
SITUATION_SAFE_MUTEX_LOCK(mutex, SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED);
```

---

### Issue 3: Threading Not Available

**Symptoms**:
- Compilation errors
- `__STDC_NO_THREADS__` defined

**Diagnosis**:
```bash
./build/threading_diagnostic_test.exe
```
Output: "Threading not available"

**Solution**:
- Update compiler (need C11 support)
- Or use fallback non-threaded mode (automatic)

---

### Issue 4: Suspected Deadlock

**Symptoms**:
- Application hangs
- No error messages
- CPU usage low

**Diagnosis**:
```c
// Add timeout to all mutex locks
bool locked = SituationMutexTryLockTimeout(&mutex, 5000);
if (!locked) {
    printf("DEADLOCK DETECTED\n");
}
```

**Solution**:
- Check lock ordering
- Ensure all locks are released
- Use timeout protection everywhere

---

## Best Practices

### 1. Always Check Capabilities First
```c
int main(void) {
    SituationThreadingStatus status = SituationGetThreadingStatus();
    if (!status.available) {
        fprintf(stderr, "Threading not available\n");
        return 1;
    }
    // Continue...
}
```

### 2. Enable Debug Logging During Development
```c
#ifdef DEBUG
    #define SITUATION_DEBUG_THREADING
#endif
```

### 3. Use Error Checking Macros
```c
// Instead of raw calls
SITUATION_SAFE_MUTEX_LOCK(mutex, error_code);
SITUATION_SAFE_MUTEX_UNLOCK(mutex, error_code);
```

### 4. Use Platform-Specific Sleep
```c
// Always use this
SITUATION_SLEEP_MS(milliseconds);

// Never use this on Windows
thrd_sleep(&ts, NULL);  // BUG!
```

### 5. Add Timeout Protection
```c
// For critical sections
if (!SituationMutexTryLockTimeout(&mutex, 1000)) {
    // Handle timeout
}
```

---

## Testing Your Threading Code

### Minimal Test Template
```c
#define SITUATION_DEBUG_THREADING
#include "sit/aud/threading_diagnostics.h"

int main(void) {
    // 1. Check capabilities
    SituationPrintThreadingStatus();
    
    SituationThreadingStatus status = SituationGetThreadingStatus();
    if (!status.available) {
        printf("ERROR: Threading not available\n");
        return 1;
    }
    
    // 2. Your threading code here
    // ...
    
    // 3. Check for errors
    if (error_occurred) {
        printf("Error code: %d\n", error_code);
    }
    
    return 0;
}
```

### Run Diagnostic Test First
```bash
# Always run this before debugging threading issues
./build/threading_diagnostic_test.exe
```

This will tell you immediately:
- ✅ What's available
- ⚠️ What might be problematic
- ❌ What's not working

---

## Summary: Before vs After

| Aspect | Before | After |
|--------|--------|-------|
| **Capability Detection** | None | Automatic at startup |
| **Error Messages** | Silent failures | Specific error codes + messages |
| **Debug Info** | Manual printf | Automatic logging system |
| **Deadlock Protection** | None | Timeout protection |
| **Platform Issues** | Trial and error | Detected and warned |
| **Sleep Reliability** | Unknown | Detected and reported |
| **Diagnosis Time** | Hours | Minutes |

**Bottom Line**: You'll never be dumbfounded again. The system tells you exactly what's wrong, where it failed, and what to do about it.

---

**Maintained By**: Kiro AI Assistant  
**Last Updated**: 2026-03-02  
**Related Docs**: `THREADING_COMPLETE.md`, `THREADING_DEBUG_STATUS.md`
