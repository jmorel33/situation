# The "Situation" Advanced Platform Awareness, Control, and Timing
## Core API Library Reference Manual

| Metadata | Details |
| :--- | :--- |
| **Version** | 2.3.4F "Velocity" (Hotfix F) |
| **Language** | Strict C11 (ISO/IEC 9899:2011) / C++ Compatible |
| **Backends** | OpenGL 4.6 Core / Vulkan 1.1+ |
| **License** | MIT License |
| **Author** | Jacques Morel |

### Executive Summary

"Situation" is a high-performance, single-file application kernel designed for cross-platform C and C++ development. It provides a unified, deterministic abstraction layer over the host operating system and graphics hardware.

Unlike simple windowing wrappers, Situation is an opinionated System Abstraction Layer (SAL). It isolates the developer from the fragmentation of OS APIs (Windows/Linux/macOS) and Graphics Drivers (OpenGL/Vulkan), providing a stable, "Titanium-grade" foundation for building sophisticated interactive software—from real-time simulations and game engines to scientific visualization tools and multimedia installations.

The library is engineered around three architectural pillars:

1. **Awareness**
   The library provides deep introspection of the host environment. It does not simply open a window; it queries the entire system topology.
   *   **Hardware:** Real-time monitoring of GPU memory (VRAM), CPU topology, and storage capacity.
   *   **Display:** Complete multi-monitor awareness, including physical dimensions, refresh rates, and HiDPI scaling factors.
   *   **Events:** Precise handling of OS signals, file drops, focus changes, and clipboard state.

2. **Control**
   The library hands the developer explicit, granular control over the application stack.
   *   **Unified Graphics:** A modern Command Buffer architecture that abstracts OpenGL 4.6 and Vulkan 1.1 behind a single API. It manages complex resources—Shaders, Compute Pipelines, and Descriptor Sets—automatically, while enforcing correct synchronization barriers.
   *   **Hardened Audio:** A professional-grade audio engine supporting thread-safe capture (microphone), low-latency SFX playback, disk streaming for music, and a programmable DSP chain (Reverb, Echo, Filters).
   *   **Input:** A lock-free, ring-buffered input system that guarantees $O(1)$ access time and zero event loss, even during frame-rate spikes.

3. **Timing**
   The library enforces a strict temporal cadence to ensure simulation stability.
   *   **High-Resolution Timing:** Sub-millisecond performance measurement and frame-rate limiting.
   *   **Oscillator System:** An integrated temporal oscillator bank for synchronizing logic and visuals to rhythmic, periodic events independent of the rendering frame rate.

### New in v2.3.4 "Velocity"

This release shifts focus from pure stability to Developer Efficiency. The "Velocity" module introduces a comprehensive suite of Hot-Reloading tools.

Developers can now modify Shaders, Compute Pipelines, Textures, and 3D Models on disk and reload them instantly into the running application via the API. The engine handles the complex task of stalling the GPU, destroying old resources, and seamlessly swapping in new assets while maintaining existing handle IDs. This drastically accelerates the iteration loop for visual programming and content creation.

## Table of Contents

- [1.0 Core System Architecture](#10-core-system-architecture)
  - [1.1 Lifecycle Management](#11-lifecycle-management)
    - [1.1.1 Initialization](#111-initialization)
    - [1.1.2 The Application Loop](#112-the-application-loop)
    - [1.1.3 Shutdown & Cleanup](#113-shutdown--cleanup)
    - [1.1.4 Lifecycle Example](#114-lifecycle-example)
  - [1.2 The Execution Model](#12-the-execution-model)
    - [1.2.1 The Three-Phase Frame](#121-the-three-phase-frame)
    - [1.2.2 Thread Safety Contracts](#122-thread-safety-contracts)
    - [1.2.3 The "Update-Before-Draw" Rule](#123-the-update-before-draw-rule)
    - [1.2.4 Error Handling](#124-error-handling)
  - [1.3 Timing & Synchronization](#13-timing--synchronization)
    - [1.3.1 High-Resolution Timing](#131-high-resolution-timing)
    - [1.3.2 Frame Pacing (FPS Limiting)](#132-frame-pacing-fps-limiting)
    - [1.3.3 The Temporal Oscillator System](#133-the-temporal-oscillator-system)
    - [1.3.4 Timing Example](#134-timing-example)
  - [1.4 Hardware Awareness](#14-hardware-awareness)
    - [1.4.1 System Snapshot](#141-system-snapshot)
    - [1.4.2 Specialized Queries](#142-specialized-queries)
    - [1.4.3 Drive Information (Windows Only)](#143-drive-information-windows-only)
    - [1.4.4 Usage Example: Auto-Config](#144-usage-example-auto-config)
- [2.0 Windowing & Display Subsystem](#20-windowing--display-subsystem)
  - [2.1 Window State Management](#21-window-state-management)
    - [Configuration Flags (SITUATION_FLAG_*)](#configuration-flags-situation_flag_)
    - [Fullscreen & Borderless Modes](#fullscreen--borderless-modes)
    - [Focus & Attention](#focus--attention)
  - [2.2 Multi-Monitor Topology](#22-multi-monitor-topology)
    - [Monitor Enumeration (SituationGetDisplays)](#monitor-enumeration-situationgetdisplays)
    - [Video Modes & Refresh Rates](#video-modes--refresh-rates)
    - [DPI Scaling & Coordinate Mapping](#dpi-scaling--coordinate-mapping)
  - [2.3 Cursor & Clipboard](#23-cursor--clipboard)
    - [Hardware Cursor Shapes](#hardware-cursor-shapes)
    - [Raw Motion (FPS Mode)](#raw-motion-fps-mode)
    - [System Clipboard Access](#system-clipboard-access)
- [3.0 The Graphics Pipeline](#30-the-graphics-pipeline)
  - [3.1 The Command Buffer Abstraction](#31-the-command-buffer-abstraction)
    - [Concept: Immediate (GL) vs Deferred (VK)](#concept-immediate-gl-vs-deferred-vk)
    - [Frame Acquisition (SituationAcquireFrameCommandBuffer)](#frame-acquisition-situationacquireframecommandbuffer)
    - [Submission (SituationEndFrame)](#submission-situationendframe)
  - [3.2 Render Passes](#32-render-passes)
    - [Pass Configuration (SituationRenderPassInfo)](#pass-configuration-situationrenderpassinfo)
    - [Load/Store Operations & Clearing](#loadstore-operations--clearing)
    - [Viewport & Scissor](#viewport--scissor)
  - [3.3 Shader Pipelines](#33-shader-pipelines)
    - [Graphics Pipelines (SituationLoadShader)](#graphics-pipelines-situationloadshader)
    - [Uniform Management](#uniform-management)
    - [Hot-Reloading Shaders](#hot-reloading-shaders)
  - [3.4 Geometry & Meshes](#34-geometry--meshes)
    - [Vertex Data Layouts](#vertex-data-layouts)
    - [Mesh Creation & Destruction](#mesh-creation--destruction)
    - [Model Loading (GLTF)](#model-loading-gltf)
  - [3.5 Textures & Images](#35-textures--images)
    - [Image Manipulation (CPU)](#image-manipulation-cpu)
    - [Texture Creation (GPU)](#texture-creation-gpu)
    - [Samplers & Mipmaps](#samplers--mipmaps)
  - [3.6 Virtual Display Compositor](#36-virtual-display-compositor)
    - [Concept: Offscreen Rendering](#concept-offscreen-rendering)
    - [Scaling Modes (Integer, Fit, Stretch)](#scaling-modes-integer-fit-stretch)
    - [Blend Modes (Overlay, Soft Light, etc.)](#blend-modes-overlay-soft-light-etc)
  - [3.7 Compute & GPGPU](#37-compute--gpgpu)
    - [Compute Pipelines](#compute-pipelines)
    - [Storage Buffers (SSBOs)](#storage-buffers-ssbos)
    - [Dispatch & Synchronization Barriers](#dispatch--synchronization-barriers)
- [4.0 Audio Engine](#40-audio-engine)
  - [4.1 Audio Context](#41-audio-context)
    - [Device Enumeration](#device-enumeration)
    - [Master Control](#master-control)
  - [4.2 Resource Management](#42-resource-management)
    - [Loading Strategies (Stream vs. RAM)](#loading-strategies-stream-vs-ram)
    - [Decoding](#decoding)
  - [4.3 Voice Management](#43-voice-management)
    - [Playback Control (Play/Stop/Loop)](#playback-control-playstoploop)
    - [Properties (Pitch, Pan, Volume)](#properties-pitch-pan-volume)
  - [4.4 DSP Chain](#44-dsp-chain)
    - [Filters (Low/High Pass)](#filters-lowhigh-pass)
    - [Environmental Effects (Reverb, Echo)](#environmental-effects-reverb-echo)
    - [Custom Processors](#custom-processors)
  - [4.5 Audio Capture](#45-audio-capture)
    - [Microphone Initialization](#microphone-initialization)
    - [Buffer Access](#buffer-access)
- [5.0 Input & Haptics](#50-input--haptics)
  - [5.1 Architecture: Ring Buffers & Polling](#51-architecture-ring-buffers--polling)
  - [5.2 Keyboard](#52-keyboard)
    - [Key States vs. Events](#key-states-vs-events)
    - [Text Input](#text-input)
  - [5.3 Mouse](#53-mouse)
    - [Position, Delta, and Wheel](#position-delta-and-wheel)
    - [Buttons](#buttons)
  - [5.4 Gamepad](#54-gamepad)
    - [Connection Handling](#connection-handling)
    - [Axis & Button Mapping](#axis--button-mapping)
    - [Haptic Feedback (Rumble)](#haptic-feedback-rumble)
- [6.0 Filesystem & I/O](#60-filesystem--io)
  - [6.1 Path Management](#61-path-management)
    - [Virtual Paths & Base Directories](#virtual-paths--base-directories)
    - [User Data / Save Paths](#user-data--save-paths)
  - [6.2 File Operations](#62-file-operations)
    - [Reading/Writing (Binary & Text)](#readingwriting-binary--text)
    - [File Drop Events](#file-drop-events)

<a id="10-core-system-architecture"></a>
<details>
<summary><b>1.0 Core System Architecture</b></summary>

The Core module acts as the central nervous system of the library. It is responsible for initializing the platform abstraction layer, managing the application's lifecycle, enforcing the execution model, and providing high-resolution timing services.

Unlike loosely coupled libraries, Situation enforces a specific initialization and shutdown sequence to ensure that subsystems (Audio, Input, Graphics) are brought online in the correct order and torn down safely.

<a id="11-lifecycle-management"></a>
## 1.1 Lifecycle Management

The application lifecycle is strictly defined. All interaction with the library must occur between a successful call to `SituationInit` and a final call to `SituationShutdown`.

<a id="111-initialization"></a>
### 1.1.1 Initialization

The entry point for the library is `SituationInit`. This function performs a monolithic initialization of all subsystems. It creates the OS window, initializes the chosen graphics backend (OpenGL context or Vulkan Instance/Device), enumerates audio devices, and allocates the internal memory arenas.

#### SituationInit

```C
SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info);
```

**Returns:** `SITUATION_SUCCESS` (0) on success, or a specific error code on failure.

**Thread Safety:** Must be called from the main thread.

**Re-entrancy:** Not re-entrant. Calling this twice without an intervening Shutdown will return `SITUATION_ERROR_ALREADY_INITIALIZED`.

#### SituationInitInfo

This structure configures the initial state of the application kernel. It is passed to `SituationInit`.

```C
typedef struct {
    // --- Window Configuration ---
    int window_width;                   // Client area width (e.g., 1280)
    int window_height;                  // Client area height (e.g., 720)
    const char* window_title;           // Title bar text (UTF-8)

    // --- State Flags ---
    // Bitmask of SituationWindowStateFlags to apply immediately upon creation.
    // Example: SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_VSYNC_HINT
    uint32_t initial_active_window_flags;
    uint32_t initial_inactive_window_flags;

    // --- Vulkan Specific Configuration ---
    // Ignored if SITUATION_USE_OPENGL is defined.
    bool enable_vulkan_validation;      // Enables standard validation layers (LunarG).
    uint32_t max_frames_in_flight;      // Default: 2 (Double buffering).
    const char** required_vulkan_extensions; // Additional instance extensions.
    uint32_t required_vulkan_extension_count;

    // --- Advanced Configuration ---
    // Bitmask of initialization flags.
    // SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD: Routes capture callbacks to main thread.
    uint32_t flags;
} SituationInitInfo;
```

<a id="112-the-application-loop"></a>
### 1.1.2 The Application Loop

The library does not seize control of your main loop. Instead, it provides a predicate function, `SituationWindowShouldClose`, which allows you to drive the loop yourself. This "Game Loop" pattern is preferred over callback-based architectures as it allows for simpler control flow.

#### SituationWindowShouldClose

```C
bool SituationWindowShouldClose(void);
```

**Returns:** `true` if the user has requested the application to close (e.g., clicked the Close button, pressed Alt+F4).

**Usage:** Use this as the condition for your `while()` loop.

<a id="113-shutdown--cleanup"></a>
### 1.1.3 Shutdown & Cleanup

The `SituationShutdown` function is the destruct sequence for the kernel. It is mandatory.

#### SituationShutdown

```C
void SituationShutdown(void);
```

**Behavior:**

*   **Graphics:** Waits for the GPU to become idle. Destroys all remaining textures, buffers, and shaders.
*   **Window:** Destroys the OS window and restores video modes.
*   **Audio:** Stops the audio engine and releases the device.
*   **Leak Detection:** Scans internal tracking lists. If any resources (Meshes, Shaders, etc.) were not manually destroyed by the user, it prints a detailed warning to `stderr` with the Resource ID and Type.

<a id="114-lifecycle-example"></a>
### 1.1.4 Lifecycle Example

A correct, minimal lifecycle implementation:

```C
int main(int argc, char** argv) {
    // 1. Configuration
    SituationInitInfo config = {0};
    config.window_width = 1280;
    config.window_height = 720;
    config.window_title = "Situation App";
    config.initial_active_window_flags = SITUATION_FLAG_WINDOW_RESIZABLE;

    // 2. Initialization
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) {
        // Retrieve the specific failure reason
        char* err = SituationGetLastErrorMsg();
        fprintf(stderr, "Critical Error: %s\n", err);
        free(err); // Caller must free error strings
        return -1;
    }

    // 3. The Loop
    while (!SituationWindowShouldClose()) {
        // ... Frame Logic ...
    }

    // 4. Shutdown
    SituationShutdown();
    return 0;
}
```
</details>

<a id="12-the-execution-model"></a>
<details>
<summary><b>1.2 The Execution Model</b></summary>

This section defines the "Contract of Execution." It explains how the code runs, the strict ordering requirements of the frame loop, and the threading guarantees provided by the library. Violating these rules is the primary cause of "it works on my machine but crashes on yours" bugs.

"Situation" is designed to be deterministic. Unlike event-driven UI frameworks (like Qt or WinForms) where code runs in response to unpredictable callbacks, Situation enforces a Three-Phase Frame architecture. This ensures that every system—Input, Physics, and Rendering—operates on a consistent snapshot of time and state.

<a id="121-the-three-phase-frame"></a>
### 1.2.1 The Three-Phase Frame

Every iteration of your main loop represents one discrete unit of time (a frame). To maintain stability and synchronization between the CPU and GPU, you must call specific core functions in a strict order.

#### Phase 1: Input & State Polling

**Function:** `SituationPollInputEvents()`
**When:** Must be the first call in your loop.
**What it does:**
*   Pumps the OS message queue (Windows/Cocoa/X11).
*   Processes hardware events (Keyboard, Mouse, Gamepad).
*   Updates the internal Input Ring Buffers.
*   Dispatches window events (Resize, Focus, File Drops).
**Why:** This ensures that all input queries (`SituationIsKeyDown`) return consistent results for the duration of the frame.

#### Phase 2: Time & Logic Update

**Function:** `SituationUpdateTimers()`
**When:** Immediately after polling input.
**What it does:**
*   Calculates the `deltaTime` (time elapsed since the last call).
*   Updates the Temporal Oscillator System.
*   Enforces the Frame Rate Cap (if `SituationSetTargetFPS` is active) by sleeping the thread.
**Why:** Your simulation logic (Physics, AI) needs a stable `deltaTime` to integrate movement correctly.

#### Phase 3: Rendering

**Functions:** `SituationAcquireFrameCommandBuffer()` → ... → `SituationEndFrame()`
**When:** After all logic is complete.
**What it does:**
*   Acquires a swapchain image from the GPU.
*   Resets the command buffer.
*   Submits the recorded commands to the graphics queue.
*   Presents the image to the screen.

#### The SITUATION_BEGIN_FRAME() Macro

To reduce boilerplate and enforce this ordering, the library provides a convenience macro:

```C
#define SITUATION_BEGIN_FRAME() \
    SituationPollInputEvents(); \
    SituationUpdateTimers();
```

<a id="122-thread-safety-contracts"></a>
### 1.2.2 Thread Safety Contracts

The library uses a Hybrid Threading Model.

| Subsystem | Threading Rule |
| :--- | :--- |
| **Core / Windowing** | **Main Thread Only.** OS window handles are thread-affinity bound. Calling `SituationSetWindowTitle` from a worker thread is undefined behavior. |
| **Graphics (Command Recording)** | **Main Thread Only.** The current version of the API exposes a single main command buffer. Recording commands is not thread-safe. |
| **Graphics (Resource Creation)** | **Main Thread Only.** Creating Textures/Meshes involves context-bound operations. |
| **Audio** | **Thread Safe.** The audio engine runs on a dedicated high-priority thread. You may call `SituationPlaySound` or modify volume from the Main Thread safely; the library handles the mutex locking. |
| **Input** | **Thread Safe.** Input state is guarded by mutexes. |

**Best Practice:** Perform file I/O, decompression, and physics calculations on worker threads. However, hand the final data back to the Main Thread to upload it to the GPU or update the Window.

<a id="123-the-update-before-draw-rule"></a>
### 1.2.3 The "Update-Before-Draw" Rule

To guarantee identical behavior between OpenGL (Immediate Execution) and Vulkan (Deferred Execution), you must adhere to the Update-Before-Draw contract.

**The Rule:** You must update all data buffers (UBOs, SSBOs, Push Constants) **before** you issue the Draw command that uses them.

**The Violation:** If you record `SituationCmdDrawMesh` and then call `SituationUpdateBuffer` later in the same frame, OpenGL might render correctly (because it drew immediately), but Vulkan will render with the new data (because it executes later).

**Enforcement:** In Debug builds, the library tracks state and will issue a warning if it detects a buffer update occurring after a draw call within the same render pass.

<a id="124-error-handling"></a>
### 1.2.4 Error Handling

The library uses return codes (`SituationError` enum) for critical failures and a logging system for warnings.

**Critical Failures:** Functions like `SituationInit` or `SituationLoadShader` return an error code or an invalid handle ID (0). You must check these.

**Error Retrieval:** If a function fails, call `SituationGetLastErrorMsg()` immediately to get a human-readable string describing the failure (e.g., "Shader compilation failed at line 40"). You must free() this string.

```C
SituationShader shader = SituationLoadShader(...);
if (shader.id == 0) {
    char* msg = SituationGetLastErrorMsg();
    printf("Error: %s\n", msg);
    free(msg); // Important!
}
```
</details>

<a id="13-timing--synchronization"></a>
<details>
<summary><b>1.3 Timing & Synchronization</b></summary>

Precise timing is the heartbeat of any interactive application. Situation provides a high-resolution monotonic clock, automatic frame pacing, and a specialized system for cyclic events.

This section covers how the library measures time, how it paces the application loop (FPS limiting), and the unique Temporal Oscillator System used for rhythmic logic.

<a id="131-high-resolution-timing"></a>
### 1.3.1 High-Resolution Timing

The library maintains an internal clock that starts at 0.0 upon initialization.

#### SituationGetTime

```C
double SituationGetTime(void);
```

**Returns:** The total time elapsed since `SituationInit` in seconds.
**Precision:** Microsecond precision (uses QueryPerformanceCounter on Windows, clock_gettime on POSIX).
**Usage:** Use this for driving continuous animations (e.g., `sin(SituationGetTime())`) or measuring long-duration events.

#### SituationGetFrameTime (Delta Time)

```C
float SituationGetFrameTime(void);
```

**Returns:** The time in seconds that the previous frame took to complete.
**Usage:** Multiply all movement and physics calculations by this value to ensure your simulation runs at the same speed regardless of the frame rate.

```C
position.x += speed * SituationGetFrameTime();
```

<a id="132-frame-pacing-fps-limiting"></a>
### 1.3.2 Frame Pacing (FPS Limiting)

The library includes a built-in frame limiter to reduce CPU/GPU usage and prevent coil whine on simple scenes.

#### SituationSetTargetFPS

```C
void SituationSetTargetFPS(int fps);
```

**Behavior:**
*   `fps > 0`: The `SituationUpdateTimers()` function will put the thread to sleep (`nanosleep`) if the frame finished faster than `1.0 / fps`.
*   `fps = 0`: Uncapped. The loop runs as fast as possible.

**VSync Interaction:** If VSync is enabled (via `SITUATION_FLAG_VSYNC_HINT`), the driver will cap the frame rate to the monitor's refresh rate, overriding this setting unless the target FPS is lower than the refresh rate.

#### SituationGetFPS

```C
int SituationGetFPS(void);
```

**Returns:** The current Frames Per Second, averaged over the last second.

<a id="133-the-temporal-oscillator-system"></a>
### 1.3.3 The Temporal Oscillator System

This is a unique feature of "Situation". Instead of tracking dozens of float timers manually (`timer += dt`), you can use the Oscillator System. This is a bank of synchronized, periodic timers ideal for rhythmic game logic, blinking lights, or musical synchronization.

**Concept:** An oscillator is a timer that loops endlessly. It has a Period (duration of one cycle) and a State (On/Off). The state flips automatically halfway through the period.

#### SituationTimerHasOscillatorUpdated

```C
bool SituationTimerHasOscillatorUpdated(int oscillator_id);
```

**Returns:** `true` only on the exact frame that the oscillator completes a cycle. This acts as a "trigger" event.

#### SituationSetTimerOscillatorPeriod

```C
SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds);
```

**Usage:** Set oscillator 0 to trigger every 0.5 seconds (120 BPM).

```C
SituationSetTimerOscillatorPeriod(0, 0.5);
```

#### SituationTimerGetOscillatorState

```C
bool SituationTimerGetOscillatorState(int oscillator_id);
```

**Returns:** The current binary state (true or false). Useful for blinking UI elements.

#### SituationTimerGetPingProgress

```C
double SituationTimerGetPingProgress(int oscillator_id);
```

**Returns:** A normalized value (0.0 to 1.0) representing progress through the current cycle. Useful for interpolating animations.

<a id="134-timing-example"></a>
### 1.3.4 Timing Example

```C
// Setup: Create a 1-second beat
SituationSetTimerOscillatorPeriod(0, 1.0);

while (!SituationWindowShouldClose()) {
    SITUATION_BEGIN_FRAME();

    // 1. Delta Time Movement
    float dt = SituationGetFrameTime();
    player.x += 5.0f * dt;

    // 2. Rhythmic Logic
    if (SituationTimerHasOscillatorUpdated(0)) {
        printf("Tick!\n"); // Prints exactly once per second
        SpawnEnemy();
    }

    // 3. Smooth Pulse Animation
    float alpha = (float)SituationTimerGetPingProgress(0);
    // alpha goes 0.0 -> 1.0 over 1 second
}
```
</details>

<a id="14-hardware-awareness"></a>
<details>
<summary><b>1.4 Hardware Awareness</b></summary>

This section details the library's ability to introspect the host machine. This is critical for auto-detecting performance tiers (e.g., "Low Spec" vs "Ultra") and debugging user issues.

"Situation" provides deep introspection of the host system. Instead of generic queries, it attempts to retrieve precise model names and capacity metrics for the CPU, GPU, RAM, and Storage.

<a id="141-system-snapshot"></a>
### 1.4.1 System Snapshot

The primary interface for hardware info is `SituationGetDeviceInfo`. This function gathers a snapshot of the entire system state.

#### SituationGetDeviceInfo

```C
SituationDeviceInfo SituationGetDeviceInfo(void);
```

**Returns:** A `SituationDeviceInfo` struct containing the snapshot.
**Performance:** This function can be expensive (it queries WMI/Registry on Windows or `/proc` on Linux). Call it once at startup, not every frame.

#### SituationDeviceInfo Struct

```C
typedef struct {
    // --- Processor ---
    char cpu_name[64];              // e.g., "Intel(R) Core(TM) i9-13900K"
    int cpu_cores;                  // Physical Cores
    float cpu_clock_speed_ghz;      // Base Clock

    // --- Memory ---
    uint64_t total_ram_bytes;       // Total System RAM
    uint64_t available_ram_bytes;   // Currently Free RAM

    // --- Graphics ---
    char gpu_name[128];             // e.g., "NVIDIA GeForce RTX 4090"
    uint64_t gpu_dedicated_memory_bytes; // VRAM (Accurate on Win32/DXGI, Estimated on others)

    // --- Storage ---
    int storage_device_count;
    char storage_device_names[8][128];
    uint64_t storage_capacity_bytes[8];
    uint64_t storage_free_bytes[8];

    // --- Peripherals ---
    int network_adapter_count;
    int input_device_count;
} SituationDeviceInfo;
```

<a id="142-specialized-queries"></a>
### 1.4.2 Specialized Queries

For specific runtime checks, lighter helper functions are available.

#### SituationGetGPUName

```C
const char* SituationGetGPUName(void);
```

**Returns:** The name of the currently active GPU.
**Usage:** Useful for logging or displaying in a "Settings" menu.

#### SituationGetVRAMUsage (Graphics Module)

```C
uint64_t SituationGetVRAMUsage(void);
```

**Returns:** The number of bytes currently allocated by the application on the GPU.

**Backend Details:**
*   **Vulkan:** Returns exact bytes allocated via VMA.
*   **OpenGL:** Returns estimate based on texture/buffer uploads (driver dependent).

<a id="143-drive-information-windows-only"></a>
### 1.4.3 Drive Information (Windows Only)

On Windows, you can query specific drive letters to manage save data or installation paths.

#### SituationGetCurrentDriveLetter

```C
char SituationGetCurrentDriveLetter(void);
```

**Returns:** The drive letter (e.g., 'C') where the executable is running.

#### SituationGetDriveInfo

```C
bool SituationGetDriveInfo(char drive_letter,
                           uint64_t* out_total,
                           uint64_t* out_free,
                           char* out_vol_name,
                           int vol_name_len);
```

**Usage:** Check for available disk space before saving a large file.

<a id="144-usage-example-auto-config"></a>
### 1.4.4 Usage Example: Auto-Config

```C
void ConfigureQualitySettings() {
    SituationDeviceInfo info = SituationGetDeviceInfo();

    printf("System: %s with %d cores.\n", info.cpu_name, info.cpu_cores);
    printf("GPU: %s (%llu MB VRAM)\n", info.gpu_name, info.gpu_dedicated_memory_bytes / 1024 / 1024);

    // Simple heuristic for quality settings
    if (info.gpu_dedicated_memory_bytes > 8ULL * 1024 * 1024 * 1024) {
        // > 8GB VRAM
        printf("Setting Quality: ULTRA\n");
        g_texture_quality = HIGH;
        g_shadow_resolution = 4096;
    } else {
        printf("Setting Quality: MEDIUM\n");
        g_texture_quality = MEDIUM;
        g_shadow_resolution = 1024;
    }
}
```
</details>
