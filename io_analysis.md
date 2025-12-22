# Realization Analysis Plan: io_analysis
**Goal:** Decouple all I/O (File Access and User Input) from the Main Thread.

## 1. Executive Summary
The goal is to prevent I/O operations from blocking the Main Thread, which is responsible for the core game loop (Windowing, Rendering commands, Logic). Blocking the Main Thread causes frame rate stutters.
We will analyze the feasibility and strategy for decoupling:
1.  **File Access**: High Feasibility. Can be moved to the existing Thread Pool.
2.  **User Input**: Low Feasibility for polling (OS constraint), High Feasibility for processing.

## 2. Analysis of Current State

### 2.1 File Access
*   **Current Implementation**:
    *   `SituationLoadFileData` (Line 20976 in `situation.h`) uses standard synchronous `fopen`, `fseek`, `ftell`, `fread`, `fclose`.
    *   `SituationSaveFileData` (implied, likely similar) uses synchronous `fopen`, `fwrite`.
    *   Functions like `SituationLoadImage`, `SituationLoadModel` call these blocking functions directly.
*   **Bottleneck**: Large asset loads (textures, models, audio) freeze the application until the disk operation completes.
*   **Existing Async Primitives**:
    *   `SituationLoadSoundFromFileAsync` (Line 31015) already demonstrates using `SituationSubmitJobEx` to offload `SituationLoadSoundFromFile` (which does I/O) to a background thread.
    *   The `SituationThreadPool` system is robust and capable of handling generic jobs.

### 2.2 User Input
*   **Current Implementation**:
    *   `SituationPollInputEvents` calls `glfwPollEvents`.
    *   State is updated directly into global structs (`sit_input`) within the polling function or callbacks (which run on the Main Thread during `glfwPollEvents`).
*   **Constraints**:
    *   **OS/GLFW Requirement**: `glfwPollEvents` **MUST** be called on the thread that created the window (Main Thread). This is a hard OS limitation on Windows (Win32 message pump) and macOS (Cocoa event loop).
    *   Moving `glfwPollEvents` to another thread is not architecturally viable without rewriting the entire Windowing system to live on that separate thread (which complicates Rendering significantly).
*   **Bottleneck Analysis**:
    *   `glfwPollEvents` itself is usually fast unless the window is being dragged/resized (which blocks the thread on Windows anyway, though Situation has mitigations for this).
    *   The "I/O" part of User Input is the polling.
*   **Decoupling Strategy**:
    *   Since we cannot move the *source* (polling), we must ensure the *sink* (consumption) is thread-safe.
    *   Current Input API (`SituationIsKeyDown`, etc.) reads from `sit_input`.
    *   If Game Logic is moved to a worker thread (implied by "decouple logic"), it needs safe access to Input.
    *   **Atomic/Snapshotting**: The Input system already resets per-frame state (`down_this_frame`) at the start of `SituationPollInputEvents`.
    *   **Conclusion**: Strictly speaking, "Decoupling User Input IO from Main Thread" is impossible for the *polling* phase. We will interpret the goal as "Ensure Input *Processing* does not block Main Thread and can be accessed safely by Workers."

## 3. Detailed Realization Plan

### 3.1 Strategy: Asynchronous File I/O
We will introduce a new set of Async I/O API functions that leverage the Thread Pool.

**New API Functions:**
1.  `SituationLoadFileAsync(const char* path, SituationFileCallback callback, void* user_data)`
2.  `SituationSaveFileAsync(const char* path, const void* data, size_t size, SituationFileCallback callback, void* user_data)`

**Implementation Details:**
*   Create an internal job wrapper struct (similar to `_SitAsyncAudioCtx`).
*   **Job Function**:
    *   Performs the blocking `SituationLoadFileData` / `SituationSaveFileData`.
    *   Invokes the user-provided callback upon completion.
*   **Callback Safety**: The callback will run on the **Worker Thread**. Users must be warned or we must provide a mechanism to marshal results back to the Main Thread if they touch non-thread-safe resources (like OpenGL).
    *   *Decision*: For raw data loading, worker thread callbacks are fine. The user usually parses the data (e.g., decodes image) on the worker, then schedules a Main Thread task for GPU upload.

### 3.2 Strategy: Input System Hardening (Pseudo-Decoupling)
Since `glfwPollEvents` stays on Main, we focus on:
1.  **Thread-Safe Access**: Verify `sit_input` accessors (`IsKeyDown`) are safe for worker threads.
    *   Current: Arrays are `bool`. Reads are atomic-enough on modern archs.
    *   Improvement: Add `_Atomic` or explicit memory barriers if strict correctness is needed, but for now, raw reads are likely acceptable provided logic runs *after* Polling is complete for the frame.
2.  **Input Processing Jobs**:
    *   If "User Input" implies heavy processing (e.g., gesture recognition), provide a `SituationProcessInputAsync` helper? *Verdict: Over-engineering. Users can just submit a job.*

## 4. Work Items (Implementation Steps)

1.  **Define Async I/O Types in `situation.h`**:
    *   `typedef void (*SituationFileLoadCallback)(void* data, size_t size, void* user_data);`
    *   `typedef void (*SituationFileSaveCallback)(bool success, void* user_data);`
2.  **Implement `SituationLoadFileAsync`**:
    *   Allocates context (Path, Callback, UserData).
    *   Submits `SIT_SUBMIT_DEFAULT` job to Thread Pool.
    *   Worker calls `SituationLoadFileData`.
    *   Worker calls Callback.
3.  **Implement `SituationSaveFileAsync`**:
    *   Allocates context (Path, Data Copy?, Callback, UserData).
    *   *Note*: Saving requires copying data if the user frees it immediately. For Async Save, we probably need to `malloc` a copy of the data to ensure it persists until the thread writes it.
    *   *Constraint*: Limit Async Save to reasonable sizes or require user to keep memory valid?
    *   *Decision*: **Copy** the data for safety, or document strict lifetime requirements. Given "Situation" philosophy of safety, we should copy (or take ownership). Let's Copy.
4.  **Documentation**:
    *   Update `situation_api.md` (or header docs) to explain Async I/O usage.
    *   Add note about `glfwPollEvents` remaining on Main Thread.

## 5. Verification Plan
*   Create a test case `test_async_io.c`.
*   Trigger a file load.
*   Verify Main Thread continues running (measure delta time, ensure no spike).
*   Verify Callback fires with correct data.
