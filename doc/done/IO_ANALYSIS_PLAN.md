# Implementation Plan: Situation Library v2.3.37 ("Trinity Polish & Hardening")
**Target Version:** 2.3.37
**Base Version:** 2.3.36
**Objective:** Fix a critical shutdown race condition in the I/O thread, expose runtime configuration for I/O polling, and add missing debug metrics.

## Phase 1: Critical Bug Fix (Shutdown Safety)
**Context:** In v2.3.36, `SituationDestroyThreadPool` joins `pool->threads[]` but ignores `pool->io_thread`. This causes the application to terminate while the I/O thread is potentially still writing to memory or checking files.

### 1.1. Fix Thread Join
*   **Location:** `SituationDestroyThreadPool` (Implementation)
*   **Action:** Add explicit join logic for the I/O thread after the worker loop.
*   **Code Requirement:**
    ```c
    // Existing loop joins workers...
    for (size_t i = 0; i < pool->thread_count; ++i) {
        thrd_join(pool->threads[i], NULL);
    }

    // [New] Join I/O Thread
    if (pool->io_thread) {
        thrd_join(pool->io_thread, NULL);
        pool->io_thread = 0; // Clear handle
    }
    ```
- [x] Implement I/O thread join logic in `SituationDestroyThreadPool`.

## Phase 2: Runtime Configuration
**Context:** v2.3.36 hardcodes the I/O thread spawn and the hot-reload poll rate (0.5s). Users need control over this via `SituationInit`.

### 2.1. Update Init Structure
*   **Location:** `SituationInitInfo` (Header)
*   **Action:** Add configuration fields.
    ```c
    typedef struct {
        // ... existing fields ...
        // [v2.3.37] I/O Configuration
        bool disable_io_thread;         // If true, runs I/O tasks on main thread (fallback)
        double hot_reload_poll_rate;    // Seconds between checks (default 0.5). 0 = disable.
    } SituationInitInfo;
    ```
- [x] Add `disable_io_thread` and `hot_reload_poll_rate` to `SituationInitInfo`.

### 2.2. Pass Config to Thread Pool
*   **Location:** `SituationThreadPool` (Struct)
*   **Action:** Add storage for the poll rate.
    ```c
    typedef struct SituationThreadPool {
        // ...
        double hot_reload_rate;
    } SituationThreadPool;
    ```
- [x] Add `hot_reload_rate` to `SituationThreadPool`.

### 2.3. Update Initialization Logic
*   **Location:** `SituationInit` and `SituationCreateThreadPool`
*   **Action:** Pass the `init_info` settings down to `SituationCreateThreadPool`. In `SituationCreateThreadPool`: If `disable_io_thread` is true, do not `thrd_create` the `io_thread`. Store `hot_reload_poll_rate` in the pool struct.
- [x] Pass I/O config from `SituationInit` to `SituationCreateThreadPool`.
- [x] Implement conditional I/O thread creation in `SituationCreateThreadPool`.

### 2.4. Update I/O Thread Entry
*   **Location:** `_SituationIOThreadEntry`
*   **Action:** Replace the hardcoded `0.5` constant with `pool->hot_reload_rate`.
- [x] Use `pool->hot_reload_rate` instead of hardcoded constant in `_SituationIOThreadEntry`.

## Phase 3: Fallback & Metrics
**Context:** If the I/O thread is disabled (or fails to spawn), async loaders must still work (synchronously) to prevent the app from hanging.

### 3.1. Implement Fallback Execution
*   **Location:** `SituationSubmitJobEx`
*   **Action:** Check if `pool->io_thread` exists. If not (and the job is low priority/IO), execute the function immediately inline (Synchronous Fallback).
*   **Reasoning:** Ensures `SituationLoadFileAsync` still returns valid data even if threading is off.
- [x] Implement synchronous fallback in `SituationSubmitJobEx` when I/O thread is missing.

### 3.2. Add Queue Metric
*   **Location:** Public API
*   **Action:** Implement `SituationGetIOQueueDepth`.
    ```c
    SITAPI size_t SituationGetIOQueueDepth(void) {
        if (!sit_render.enabled) return 0;
        // Calculate depth of queue[0] (Low Priority/IO)
        // atomic_load(head) - atomic_load(tail)
    }
    ```
- [x] Implement `SituationGetIOQueueDepth` function.
