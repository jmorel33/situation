# Deep Dive Analysis: Audio Subsystem & Multi-Threading Readiness

## 1. Executive Summary

This document presents a comprehensive analysis of the current Audio Subsystem within the `situation` library (v2.3.x "Velocity"). The primary goal is to assess its readiness for true multi-threaded operation and identify the necessary architectural changes to ensure robust, thread-safe, and high-performance audio processing.

**Current Status:**  
The audio engine utilizes a "Snapshot-Mixing" strategy on top of `miniaudio`, with a global state container (`sit_audio`) and a fixed-size mixing queue. While it implements basic thread safety mechanisms (recursive mutexes, snapshotting), it currently suffers from **inconsistent locking**, **potential race conditions** on parameter updates, and **scalability limitations** due to fixed-size buffers.

**Recommendation:**  
To achieve "Titanium Grade" reliability and multi-thread capability, the subsystem requires a targeted refactor to:
1.  Standardize locking across *all* API functions.
2.  Implement atomic access for real-time parameters (volume, pan, pitch).
3.  Harden the resource lifecycle management (Handle Validation) to prevent Use-After-Free errors.
4.  Transition from fixed limits to dynamic or configurable limits for active voices.

---

## 2. Current Architecture Overview

The Audio module operates on a **Hybrid Threading Model**:
*   **Main Thread:** Handles API calls (`SituationPlayLoadedSound`, `SituationSetSoundVolume`), resource loading, and high-level logic.
*   **Audio Thread:** A dedicated, high-priority real-time thread managed by `miniaudio` that executes the mixing callback (`sit_miniaudio_data_callback`).

### 2.1 The "Snapshot-Mixing" Strategy
To minimize lock contention on the critical audio thread, the engine uses a snapshot approach:
1.  **Snapshot (Phase 1):** The audio thread briefly locks `audio_queue_mutex` to copy the pointers of all active sounds (`queued_sounds`) to a local stack array (`active_sounds`).
2.  **Processing (Phase 2):** It releases the lock and processes the audio (decoding, effects, mixing) using the local snapshot. This allows the Main Thread to queue new sounds without blocking the mixer.
3.  **Commit (Phase 3):** It re-acquires the lock to remove any sounds that finished playing during the cycle.

### 2.2 Global State (`sit_audio`)
All audio state is encapsulated in a global, static `_SituationAudioState` structure (defined in implementation):
```c
typedef struct {
    // ... Context & Device handles ...
    SituationSound* queued_sounds[SITUATION_MAX_AUDIO_SOUNDS_QUEUED]; // Fixed array (Max 32)
    int queued_sound_count;
    mtx_t audio_queue_mutex; // Recursive mutex
    // ... Scratch buffers ...
} _SituationAudioState;
```
*Validated in Codebase:* `SITUATION_MAX_AUDIO_SOUNDS_QUEUED` is strictly defined as `32` in `situation.h`.

---

## 3. Identified Critical Issues

### 3.1 Inconsistent Locking Strategy
**Severity: High**  
While `SituationPlayLoadedSound` and `SituationSetSoundPitch` correctly acquire `audio_queue_mutex`, other critical functions historically did not.
*   **Vulnerable Functions:** `SituationSetSoundVolume`, `SituationSetSoundPan`.
*   **Evidence (Pre-Fix):**
    *   `SituationSetSoundPan` (Line ~27824): Direct assignment `sound->pan = pan;` without `mtx_lock`.
    *   `SituationSetSoundVolume` (Line ~27782): Direct assignment `sound->volume = ...;` without `mtx_lock`.
*   **Resolution:** As of the current version, locking has been added to these functions.
*   **Risk:** These functions modify `float` values (`sound->volume`, `sound->pan`) directly. If called from the Main Thread while the Audio Thread is reading them in the processing loop (Phase 2), it results in a data race. While often benign on x86, this is undefined behavior and can lead to tearing or glitches on other architectures.

### 3.2 Race Conditions on Parameters
**Severity: Medium**  
The `SituationSound` struct uses standard `float` types for real-time parameters:
```c
typedef struct SituationSound {
    // ...
    float volume;
    float pan;
    float pitch;
    // ...
} SituationSound;
```
*Validated:* Struct definition at Line ~542 confirms standard `float` usage. Since the Audio Thread reads these values without a lock (during the mixing phase), any concurrent modification is technically unsafe.

### 3.3 Resource Handle Safety (Use-After-Free)
**Severity: High**  
The current "Safety Wait" in `SituationUnloadSound` relies on a spin-lock on `is_processing_snapshot`.
*   **The Problem:** This protects the *Audio Thread* from accessing a freed sound, but it offers **zero protection** against other user threads. If Thread A calls `SituationSetSoundVolume(s)` while Thread B calls `SituationUnloadSound(s)`, Thread A will access invalid memory.
*   **Root Cause:** `SituationSound` is a raw pointer typedef. The library lacks a centralized handle system for Audio.

### 3.4 Scalability Limits
**Severity: Low/Medium**  
The mixing queue is hardcoded:
```c
#define SITUATION_MAX_AUDIO_SOUNDS_QUEUED 32
```
This limits the engine to 32 concurrent voices. For a complex game or simulation, this is insufficient.

---

## 4. Action Plan (The "Titanium" Standard)

To make the Audio / Sound section fully multi-thread capable, the following steps are required.

### 4.1 Step 1: Immediate Safety Fixes (Locking)
**Goal:** Eliminate data races on shared state by enforcing consistent locking.
- [x] Add `mtx_lock(&sit_audio.audio_queue_mutex)` and `mtx_unlock(...)` to `SituationSetSoundVolume`.
- [x] Add `mtx_lock(&sit_audio.audio_queue_mutex)` and `mtx_unlock(...)` to `SituationSetSoundPan`.
- [x] Verify `SituationSetSoundPitch` already has locking (Validated: Yes).
- [x] Verify `SituationSetSoundEcho` and `SituationSetSoundReverb` have locking (Validated: Yes).

### 4.2 Step 2: Atomic Parameters (Lock-Free Optimization)
**Goal:** Allow thread-safe parameter updates without the overhead of mutexes for high-frequency changes (e.g., volume fades).
- [x] Change `float volume` to `_Atomic float` (C11) or `atomic_uint_least32_t` (if float atomics are unsupported) in `SituationSound` struct.
- [x] Change `float pan` to `_Atomic float` in `SituationSound` struct.
- [x] Change `float pitch` to `_Atomic float` in `SituationSound` struct.
- [x] Update `SituationSetSoundVolume` to use `atomic_store`.
- [x] Update `SituationSetSoundPan` to use `atomic_store`.
- [x] Update Audio Thread mixer loop to use `atomic_load` when reading these values.

### 4.3 Step 3: Handle Verification System (Priority: Immediate)
**Goal:** Eliminate "Use-After-Free" errors by replacing raw pointers with a Generational Handle system, identical to the Texture Registry ID system.

**Mechanism:**
1.  **The Handle Structure:**
    Instead of `SituationSound*`, the public API will consume a `SituationSoundHandle` (typedef `uint64_t`).
    *   **Bits 0-31:** Index into the global sound pool.
    *   **Bits 32-63:** Generation counter.

2.  **The Sound Pool:**
    The global `_SituationAudioState` will house a `SituationSoundPool` containing a fixed (or dynamic) array of `SituationSound` slots.
    *   Each slot tracks its own generation.
    *   When a sound is unloaded, the slot is marked free, and its generation is incremented.
    *   **Validation:** Accessing a sound requires checking `pool[index].generation == handle.generation`. If they differ, the handle is stale (sound was unloaded), and the operation is safely ignored.

3.  **API Migration Strategy:**
    *   **Phase 1 (Internal):** Refactor internal storage to use the pool. Existing `SituationSound*` pointers in the API will be "faked" by returning `&pool[index]`.
    *   **Phase 2 (Public):** Introduce `SituationSound` (the handle) and deprecate `SituationSound*`.

**Implementation Tasks:**
- [x] Define `SituationSoundSlot` struct with generation tracking.
- [x] Implement `_SitAudioGetSoundFromHandle(uint64_t handle)` with O(1) validation.
- [x] Refactor `SituationPlayLoadedSound` to accept handles.

### 4.4 Step 4: Dynamic Mixing Queue (Priority: Immediate)
**Goal:** Remove the 32-voice hard limit and allow for scalable, high-polyphony audio scenes (e.g., bullet hell games).

**Mechanism:**
1.  **Dynamic Storage:**
    Replace the fixed `queued_sounds[32]` array in `_SituationAudioState` with a dynamic `SituationSound** active_voices` array.
    *   **Capacity:** Starts at 32, doubles geometrically (32 -> 64 -> 128) when full.
    *   **Growth:** Performed in `SituationPlayLoadedSound` (Main Thread) under `audio_queue_mutex` lock.

2.  **Thread-Safe Snapshotting:**
    The Audio Thread currently allocates a snapshot on the stack (`SituationSound* snapshot[32]`). This must change.
    *   **New Strategy:** The Audio Thread will maintain its own persistent `std::vector`-like scratch buffer (`snapshot_buffer`).
    *   **Cycle:**
        1. Lock Mutex.
        2. Resize `snapshot_buffer` to match `active_voices_count` (if needed).
        3. `memcpy` pointers from `active_voices` to `snapshot_buffer`.
        4. Unlock Mutex.
        5. Mix using `snapshot_buffer`.

3.  **Configurability:**
    Add `max_audio_voices` to `SituationInitInfo`.
    *   `0` = Unlimited (Dynamic).
    *   `>0` = Fixed Cap (Pre-allocated).

**Implementation Tasks:**
- [x] Replace `queued_sounds` with `SituationSound** voices` and `int voice_capacity`.
- [x] Implement `_SitAudioEnsureVoiceCapacity(int count)` helper. (Implemented inline in `SituationPlayLoadedSound` for efficiency).
- [x] Update `sit_miniaudio_data_callback` to use a persistent heap-allocated snapshot buffer instead of a stack array.

---

## 5. Conclusion

The `situation` audio engine is functional but fragile in a multi-threaded context. By moving to atomic parameters and enforcing consistent locking, we can solve the immediate race conditions. However, adopting a Handle-based resource system is the only way to guarantee safety against user-level threading errors (concurrent use and destroy).

**Priority:** The "Immediate Safety Fixes" (Step 4.1) should be applied immediately to the v2.3.x branch to prevent undefined behavior in current applications.

## 5. Holistic Audio Behavior (Updated v2.3.33)

With the completion of the "Titanium Grade" audio refactor in version v2.3.33, the audio subsystem now operates as a fully thread-safe, high-performance engine capable of robust concurrency. This section details the complete lifecycle and behavior of the audio system "wholistically".

### 5.1 Initialization (Main Thread)
*   **Startup:** `SituationInit` (via `_SituationInitSubsystems`) initializes the `miniaudio` backend.
*   **Context:** A global `_SituationAudioState` (accessed via `sit_audio`) is allocated.
*   **Thread Spawning:** `miniaudio` automatically spawns a high-priority, dedicated **Audio Thread**. This thread is completely separate from the Main Thread and the Render Thread.
*   **Pool Allocation:** A fixed-size Handle Pool (`SituationSoundSlot`) is initialized to track resource lifetimes safely.
*   **Dynamic Queue:** A dynamic mixing queue (`active_voices`) is allocated with an initial capacity (default 32), ready to grow as needed.

### 5.2 Resource Loading (Any Thread)
*   **Async-Friendly:** Sounds can be loaded via `SituationLoadAudio` (returns a `SituationSoundHandle`).
*   **Safety:** The Handle System uses a **Generational Index**. If a sound is unloaded and its slot reused, old handles become invalid immediately, preventing Use-After-Free crashes.
*   **Loading Modes:**
    *   `SITUATION_AUDIO_LOAD_FULL`: Decodes entire file to RAM. Safe for SFX.
    *   `SITUATION_AUDIO_LOAD_STREAM`: Streams from disk. Optimized for Music.
    *   `SITUATION_AUDIO_LOAD_AUTO`: Automatically selects mode based on duration (<10s = RAM).

### 5.3 The Playback Cycle (Hybrid Threading)
The system uses a **Snapshot-Mixing Strategy** to bridge the Main Thread and Audio Thread without stalls.

1.  **Request (Main Thread):**
    *   User calls `SituationPlayAudio(handle)`.
    *   The function acquires `audio_queue_mutex`.
    *   The sound pointer is added to the `active_voices` dynamic array.
    *   Mutex is released. **Non-blocking** for the audio thread.

2.  **Snapshot (Audio Thread - Phase 1):**
    *   The Audio Thread wakes up (via `sit_miniaudio_data_callback`) to fill the audio buffer.
    *   It briefly acquires `audio_queue_mutex`.
    *   It copies the list of active sound pointers into a local, persistent **Snapshot Buffer**.
    *   It releases the mutex immediately.
    *   *Result:* The Main Thread is only blocked for the duration of a `memcpy` (nanoseconds), ensuring high frame rates.

3.  **Mixing (Audio Thread - Phase 2):**
    *   The Audio Thread iterates over its local Snapshot Buffer.
    *   It reads parameters (Volume, Pan, Pitch) using **Atomic Loads** (`atomic_load`). This allows the Main Thread to fade volume or pan audio *while* it is being mixed, with zero tearing or race conditions.
    *   It mixes the audio data into the output buffer.
    *   If a sound finishes playing, it is marked for removal.

4.  **Commit (Audio Thread - Phase 3):**
    *   The Audio Thread re-acquires the mutex.
    *   It removes finished sounds from the global `active_voices` list.
    *   Mutex released.

### 5.4 Synchronization & Safety Features
*   **Atomic Parameters:** Real-time properties (`volume`, `pan`, `pitch`) are `_Atomic float`. This enables "Lock-Free" parameter updates.
*   **Mutex Protection:** The topology (which sounds are playing) is protected by `audio_queue_mutex`.
*   **Handle Verification:** Every API call (`SituationSetAudioVolume`, `SituationStopAudio`) validates the handle generation before accessing memory.
*   **Dynamic Scalability:** The `active_voices` array grows automatically. The hard 32-voice limit is gone; the system scales to hundreds of voices (CPU permitting).

### 5.5 Conclusion
The Audio Subsystem is now a distinct, parallel engine. It runs asynchronously to the game loop, utilizing lock-free reads for performance and strict locking for topology changes. This architecture ensures that **audio never glitches due to low FPS**, and **game logic never stalls due to audio processing**.

## 6. PCM Input Node (v2.4.198)

### Overview

The `SITUATION_NODE_PCM_INPUT` node type is a lock-free source node that accepts user-pushed PCM audio from any thread and outputs it through the audio graph. It enables kterm voice playback, network audio streams, and any user-fed PCM source to participate in the graph (mixable, patchable, effects-chainable).

### Architecture

```
User Thread (any)                    Audio Callback Thread
─────────────────                    ─────────────────────
SituationPushNodePCM()               _SituationProcessPCMInputNode()
  │                                    │
  ▼                                    ▼
┌─────────────────────────────────────────────────────┐
│  Lock-Free SPSC Ring Buffer (4096 frames × channels) │
│  atomic write_pos ──────────► atomic read_pos        │
└─────────────────────────────────────────────────────┘
```

- **Producer**: Any thread via `SituationPushNodePCM()` (writes to ring buffer)
- **Consumer**: Audio callback via the node's process function (reads from ring buffer)
- **Underrun behavior**: Outputs silence (zero-fill) — no glitch, just quiet
- **Overflow behavior**: Partial write — returns number of frames actually written

### Ring Buffer Design

- Fixed-size power-of-2 buffer: `SIT_PCM_INPUT_RING_FRAMES` (default 4096)
- SPSC (Single Producer, Single Consumer) — no locks needed
- Atomic `write_pos` / `read_pos` with acquire/release memory ordering
- One slot reserved (standard SPSC technique to distinguish full from empty)

### Controls

| ID | Name | Type  | Range       | Default | Description                    |
|----|------|-------|-------------|---------|--------------------------------|
| 0  | gain | float | 0.0 – 2.0  | 1.0     | Output volume                  |
| 1  | pan  | float | -1.0 – 1.0 | 0.0     | Stereo pan (constant-power)    |
| 2  | mute | bool  | 0 / 1       | 0       | Mute toggle                    |

### Public API

```c
// Push interleaved float PCM into the node's ring buffer (any thread, lock-free)
uint32_t SituationPushNodePCM(
    SituationAudioGraph* graph,
    SituationNodeHandle node,
    const float* samples,       // Interleaved float PCM
    uint32_t frame_count,
    uint32_t channels           // Must match node's channel config (2 = stereo)
);

// Query available write space in frames
uint32_t SituationGetNodePCMFreeFrames(
    SituationAudioGraph* graph,
    SituationNodeHandle node
);
```

### Usage Example

```c
SituationInitDeviceRegistry();
SituationAudioGraph* graph = SituationCreateGraph();

// Create PCM input node
SituationNodeHandle pcm;
SituationCreateNode(graph, SITUATION_NODE_PCM_INPUT, &pcm);

// Set as active graph
SituationSetActiveGraph(graph);

// Push audio from any thread (e.g., network receive callback)
float samples[512 * 2]; // 512 frames, stereo
// ... fill samples ...
uint32_t written = SituationPushNodePCM(graph, pcm, samples, 512, 2);

// Query remaining space
uint32_t free = SituationGetNodePCMFreeFrames(graph, pcm);
```

### Integration with kterm Voice

The kterm voice subsystem (`kt_voice.h`) uses the PCM input node for playback. When voice is enabled, a `SITUATION_NODE_PCM_INPUT` node is created on the active graph. Received network audio packets are pushed into the node via `SituationPushNodePCM()`, replacing the removed `SituationStartAudioPlayback` API.

### Non-Goals

- No resampling (caller must match the graph's sample rate)
- No codec/decode (caller provides raw float PCM)
- No automatic device routing (goes through the graph like everything else)
