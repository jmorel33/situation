## Core Module

**Overview:** The Core module is the heart of the "Situation" library, providing the essential functions for application lifecycle management. It handles initialization (`SituationInit`) and shutdown (`SituationShutdown`), processes the main event loop, and manages frame timing and rate control. This module also serves as a gateway to crucial system information, offering functions to query hardware details, manage command-line arguments, and set up critical application-wide callbacks.

### Core Structs

#### `SituationInitInfo`
This struct is passed to `SituationInit()` to configure the application at startup. It allows for detailed control over the initial state of the window, rendering backend, and timing systems.

> **Titanium Tip:** Field names use strictly `snake_case`. Ensure you are not using legacy `camelCase` names (e.g. `windowWidth`) from older versions.

```c
typedef struct {
    // ── Window Creation Parameters ──
    int          window_width;              // Initial window width in screen coordinates
    int          window_height;             // Initial window height in screen coordinates
    const char*  window_title;              // Window title bar text (UTF-8)

    // ── Window State Flags (Applied via GLFW window hints or direct state changes) ──
    uint32_t     initial_active_window_flags;    // Flags when window has focus (e.g. SIT_WINDOW_BORDERLESS | SIT_WINDOW_VSYNC)
    uint32_t     initial_inactive_window_flags;  // Flags when window is unfocused (e.g. pause rendering or reduce refresh rate)

    // ── Vulkan-Specific Options ──
    bool         enable_vulkan_validation;       // Enable VK_LAYER_KHRONOS_validation (debug builds only - auto-disabled in release)
    bool         force_single_queue;             // Force shared compute/graphics queue (debug/compatibility)
    uint32_t     max_frames_in_flight;           // Override SITUATION_MAX_FRAMES_IN_FLIGHT (usually 2 or 3)

    // Optional: Provide custom Vulkan instance extensions (e.g. for VR, ray tracing, etc.)
    const char** required_vulkan_extensions;     // Array of extension names (null or empty = use defaults)
    uint32_t     required_vulkan_extension_count;// Length of the above array

    // ── Engine Feature Flags ──
    uint32_t     flags;  // Bitfield: SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD

    /** Main-window output: 8-bit SDR, 10-bit SDR, or HDR10 — see [HD Color Output](hd_color_output.md). */
    SituationOutputColorDepth output_color_depth;

    // ── Audio Configuration ──
    uint32_t     max_audio_voices; // Max concurrent audio voices. 0 = Unlimited (Dynamic).

    // ── Render Thread (auto-enabled when SITUATION_ENABLE_THREADING is defined) ──
    int          render_thread_count; // Number of render threads (0 = main-thread rendering)
    int          backpressure_policy; // 0: Spin (low latency), 1: Yield (balanced), 2: Sleep (low CPU)

    // ── Async I/O ──
    uint32_t     io_queue_capacity;  // Size of the IO queue. Default: 1024.
    bool         disable_io_thread;  // If true, runs I/O tasks on main thread (fallback)
    double       hot_reload_poll_rate; // Seconds between hot-reload checks (0 = disable; zero-init default — opt in with e.g. 0.5)
    uint64_t     staging_buffer_size;  // Override 128MB Vulkan staging buffer (0 = default)

    // ── Thread naming & affinity ──
    const char*  main_thread_name;       // OS-visible main thread name (NULL = SITUATION_MAIN_THREAD_NAME_DEFAULT "Sit Main")
    const char*  app_user_model_id;      // Win32 shell AppUserModelID (NULL = SITUATION_DEFAULT_APP_USER_MODEL_ID); ignored off Windows — see [Application identity](../architecture.md#application-identity-architecture-v24399)
    const char*  default_window_icon_path; // Optional PNG (all platforms) or .ico (Win32); applied at end of SituationInit — see [Windows app identity](windows_app_identity.md)
    uint64_t     thread_affinity_main;   // Main thread affinity
    uint64_t     thread_affinity_render; // Render thread affinity (0 = default core 1)
    uint64_t     thread_affinity_audio;  // Audio thread affinity (0 = default core 2)

    // ── NUMA Placement ──
    bool         numa_prefer_local;      // Pin render/audio to NUMA node of default cores when affinity is 0
    bool         worker_numa_spread;     // Pin workers across NUMA nodes (default true when threading enabled)
    int32_t      io_thread_numa_node;    // Dedicated I/O thread NUMA node; < 0 = no pin

    // ── Thread Pool Sizing (when num_threads=0 in SituationCreateThreadPool) ──
    bool     thread_pool_use_physical_cores; // false = logical CPUs - reserved; true = physical cores - reserved
    uint32_t thread_pool_reserved_threads;   // Threads left for main/render/audio/IO (default 4 if 0)
} SituationInitInfo;
```
-   `window_width`, `window_height`: The desired initial dimensions for the main window's client area.
-   `window_title`: The text to display in the window title bar.
-   `initial_active_window_flags`: A bitmask of `SituationWindowStateFlags` to set the initial state of the window when focused.
    -   **VSync Control:** To enable VSync, include the `SITUATION_FLAG_VSYNC_HINT` (or `SITUATION_WINDOW_STATE_VSYNC_HINT`) flag here. There is no separate boolean for VSync.
-   `initial_inactive_window_flags`: Flags to apply when the window loses focus (e.g., lower framerate).
-   `output_color_depth`: Swapchain bit depth / HDR10 policy (`SIT_OUTPUT_COLOR_AUTO` default) — **[HD Color Output](hd_color_output.md)**.
-   `enable_vulkan_validation`: Enables Vulkan validation layers for debugging.
-   `io_queue_capacity`: The size of the low-priority IO queue for the dedicated IO thread.
-   `app_user_model_id` _(Win32, v2.4.399+)_: Shell identity for taskbar pinning and jump lists. **`NULL`** → library default **`Situation.Application`**. Set before the first window, or call **`SituationWin32SetAppUserModelId()`** pre-init. Ignored on non-Windows platforms until Linux/macOS hooks land. See **[Application identity architecture](../architecture.md#application-identity-architecture-v24399)** and **[Windows app identity](windows_app_identity.md)**.
-   `default_window_icon_path` _(v2.4.400+)_: Optional path to a **PNG** (stb formats) or **`.ico`** (Win32). Applied once at the end of **`SituationInit`** via **`SituationSetWindowIcons`**. Load errors are **fail-soft** (init still succeeds). **`NULL`** → embedded PE icon only.

**Note on Backend Selection:**
You do **not** select the graphics backend (OpenGL vs Vulkan) inside this struct. Instead, you must define either `SITUATION_USE_VULKAN` or `SITUATION_USE_OPENGL` in your code *before* including `situation.h`.

---
#### `SituationInitInfoDefault` _(v2.4.336)_
Inline helper returning a zero-initialized `SituationInitInfo` with window size, title, and `SIT_OUTPUT_COLOR_AUTO`.

```c
static inline SituationInitInfo SituationInitInfoDefault(int width, int height, const char* title);
```

---
#### `SituationClearValueColor` / `SituationClearValueDepth` _(v2.4.336)_
Inline helpers for `SituationClearValue` used with mid-pass clears and render-pass load ops.

```c
static inline SituationClearValue SituationClearValueColor(ColorRGBA color);
static inline SituationClearValue SituationClearValueDepth(float depth);
```

---
#### `SituationTimerSystem`
Manages all timing-related functionality for the application, including frame time (`deltaTime`), total elapsed time, and the Temporal Oscillator System. This struct is managed internally by the library; you interact with it through functions like `SituationGetFrameTime()` and `SituationTimerHasOscillatorUpdated()`.
```c
typedef struct SituationTimerSystem {
    double current_time;
    double previous_time;
    float frame_time;
    int target_fps;
    int oscillator_count;
    SituationTimerOscillator* oscillators;
} SituationTimerSystem;
```
-   `current_time`, `previous_time`: Internal timestamps used to calculate `frame_time`.
-   `frame_time`: The duration of the last frame in seconds (`deltaTime`).
-   `target_fps`: The current target frame rate.
-   `oscillator_count`: The number of active oscillators.
-   `oscillators`: A pointer to the internal array of oscillator states.

---
#### `SituationTimerOscillator`
Represents the internal state of a single temporal oscillator. This struct is managed by the library as part of the `SituationTimerSystem` and is not typically interacted with directly. Its properties are exposed through functions like `SituationTimerHasOscillatorUpdated()` and `SituationTimerGetOscillatorValue()`.
```c
typedef struct SituationTimerOscillator {
    double period;
    bool state;
    bool previous_state;
    double last_ping_time;
    double anchor_time;
    uint64_t trigger_count;
} SituationTimerOscillator;
```
-   `period`: The duration of one full cycle of the oscillator in seconds.
-   `state`: The current binary state of the oscillator (`true` or `false`). This flips each time half of the `period` elapses.
-   `previous_state`: The state of the oscillator in the previous frame. Used to detect when the state has changed.
-   `last_ping_time`: An internal timestamp used by `SituationTimerPingOscillator()` to track time since the last successful "ping".
-   `anchor_time`: The absolute start time used as a reference point for drift-free trigger calculation. When a period is set or changed, the anchor is reset to the current system time. Subsequent triggers are computed as `anchor + count * period`, preventing cumulative floating-point drift that would occur with repeated `next += period` additions.
-   `trigger_count`: The total number of times the oscillator has flipped its state since initialization.

---
### Functions

#### `SituationInit`
Initializes all library subsystems. This is the entry point of the "Situation" library and **must be the first function you call**. It sets up the window, initializes the selected graphics backend (OpenGL or Vulkan), prepares the audio device, and processes any command-line arguments.

The function takes a pointer to a `SituationInitInfo` struct, which allows you to configure initial properties like window size, title, and desired frame rate. Passing `NULL` will initialize the library with default settings.
```c
SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info);
```
**Usage Example:**
```c
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = SituationInitInfoDefault(1280, 720, "Core Example");
    init_info.initial_active_window_flags = SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_VSYNC_HINT;

    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        char* error_msg = SituationGetLastErrorMsg();
        fprintf(stderr, "Failed to initialize Situation: %s\n", error_msg ? error_msg : "(unknown)");
        SituationFreeString(error_msg);
        return -1;
    }

    // ... main application loop ...

    SituationShutdown();
    return 0;
}
```

---
#### `SituationPollInputEvents`
Polls for all pending input and window events from the operating system. This function is the first part of the mandatory three-phase frame structure and **must be called exactly once at the beginning of every frame**. It gathers all keyboard, mouse, gamepad, and window events (like resizing or closing) and stores them in an internal state buffer. All other input and window functions in the frame will operate on this consistent snapshot of the state.
```c
void SituationPollInputEvents(void);
```
**Usage Example:**
```c
while (!SituationWindowShouldClose()) {
    // --- 1. Input Phase ---
    SituationPollInputEvents(); // First call in the loop

    // --- 2. Update Phase ---
    SituationUpdateTimers();
    // ... game logic that relies on input ...

    // --- 3. Render Phase ---
    // ... rendering code ...
}
```

---
#### `SituationUpdateTimers`
Updates all internal timers. This is the second part of the mandatory three-phase frame structure and **must be called exactly once per frame**, immediately after `SituationPollInputEvents()`. This function advances the Temporal Oscillator System and publishes **present-anchored** timing when the render thread (or synchronous present) has completed a swap.

**Frame time (v2.4.385+):** `SituationGetFrameTime()` returns the **display delta** (time between successive presents), not main-thread wall time between `UpdateTimers` calls when the render queue is pipelined. See [architecture.md — Frame Loop Contract](../architecture.md#frame-loop-contract-v2484).

**Virtual displays:** all active VD slots have frame clocks ticked here. **`SIT_VD_UPDATE_STATIC`** VDs skip clock advance (v2.4.387) — use **`SituationSetVirtualDisplayDirty`** when static content must refresh.

```c
void SituationUpdateTimers(void);
```
**Usage Example:**
```c
while (!SituationWindowShouldClose()) {
    // --- 1. Input Phase ---
    SituationPollInputEvents();

    // --- 2. Update Phase ---
    SituationUpdateTimers(); // Second call in the loop

    // Now it's safe to get the frame time for physics and logic.
    float dt = SituationGetFrameTime();
    player_position.x += player_velocity.x * dt;

    // --- 3. Render Phase ---
    // ... rendering code ...
}
```

---
#### `SituationShutdown`
Shuts down all library subsystems, releases all GPU and CPU resources, and closes the application window. This **must be the last function called** before your application exits. It ensures a graceful cleanup and will report any leaked GPU resources (in debug mode) if you forgot to destroy them.
```c
void SituationShutdown(void);
```
**Usage Example:**
```c
int main(int argc, char* argv[]) {
    // It's good practice to pair Init and Shutdown in the same scope.
    if (SituationInit(argc, argv, NULL) != SIT_SUCCESS) {
        return -1;
    }

    while (!SituationWindowShouldClose()) {
        // Main application loop
    }

    SituationShutdown(); // The very last call.
    return 0;
}
```

---
#### `SituationIsInitialized`
Checks if the library has been successfully initialized. Returns `true` if `SituationInit()` has been called and completed without errors, `false` otherwise.
```c
bool SituationIsInitialized(void);
```
**Usage Example:**
```c
// You might use this in a helper function to ensure it's safe to call library functions.
void UpdatePlayer() {
    if (!SituationIsInitialized()) {
        printf("Error: Cannot update player before the library is initialized.\n");
        return;
    }
    // ... proceed with player update logic ...
}
```

---
#### `SituationWindowShouldClose`
Returns `true` if the user has attempted to close the window (e.g., by clicking the 'X' button, pressing Alt+F4, or sending a quit signal). This is the canonical way to control your main application loop.
```c
bool SituationWindowShouldClose(void);
```
**Usage Example:**
```c
// The main loop should continue as long as this function returns false.
while (!SituationWindowShouldClose()) {
    // Poll events, update logic, render frame
}
// Loop terminates, and the application proceeds to shutdown.
```

---
#### `SituationPauseApp`
Pauses the application's internal state, including audio playback. This is useful when the application loses focus or when implementing a pause menu.
```c
void SituationPauseApp(void);
```
**Usage Example:**
```c
// Pause the application when the window loses focus
void on_focus_changed(bool focused, void* user_data) {
    if (!focused) {
        SituationPauseApp();
    } else {
        SituationResumeApp();
    }
}
```

---
#### `SituationResumeApp`
Resumes a paused application, restoring audio playback and other internal systems.
```c
void SituationResumeApp(void);
```
**Usage Example:**
```c
// Resume the application when the player unpauses
if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
    if (SituationIsAppPaused()) {
        SituationResumeApp();
    } else {
        SituationPauseApp();
    }
}
```

---
#### `SituationIsAppPaused`
Checks if the application is currently paused.
```c
bool SituationIsAppPaused(void);
```
**Usage Example:**
```c
// Skip game logic updates when paused
if (!SituationIsAppPaused()) {
    UpdateGameLogic(deltaTime);
}
```

---
#### `SituationSetTargetFPS`
Sets a target frame rate for the application. The main loop will sleep to avoid exceeding this rate, reducing CPU usage.
```c
void SituationSetTargetFPS(int fps);
```
**Usage Example:**
```c
// Cap the application at 60 FPS.
SituationSetTargetFPS(60);
// To uncap the frame rate, use 0.
// SituationSetTargetFPS(0);
```

---
#### `SituationGetFrameTime`
Gets the time in seconds for the last **display interval** (present-to-present), commonly used as `deltaTime`. Updated by `SituationUpdateTimers()` from render-thread present timing when pipelined, or from synchronous present when the render thread is off. Suitable for camera motion and frame-rate-independent logic — **not** guaranteed to match main-thread jitter between acquire and end.

```c
float SituationGetFrameTime(void);
```
**Usage Example:**
```c
// Inside the main loop, after SituationUpdateTimers()
float dt = SituationGetFrameTime();

// Update player position based on display pace, not queue jitter
player_position.x += player_speed * dt;
```

**See also:** [architecture.md — Present-anchored timing](../architecture.md#present-anchored-timing)

---
#### `SituationGetFPS`
Gets the current frames-per-second (integer), calculated periodically. With VSync ON, refresh-aware rounding uses fractional nominal Hz (v2.4.386+) so 59.94 Hz panels report **60** instead of **59**.

```c
int SituationGetFPS(void);
```
**Usage Example:**
```c
int current_fps = SituationGetFPS();
char window_title[128];
sprintf(window_title, "My App | FPS: %d", current_fps);
SituationSetWindowTitle(window_title);
```

**Fractional refresh (v2.4.386+):** for HUD or simulation that needs exact panel rate, use **`SituationGetDisplayRefreshRateHz()`** — see [Window & Display](window_display.md#display-refresh-rate).

---
#### `SituationGetLastErrorMsg`
Retrieves a copy of the last error message generated by the library. This is useful for debugging initialization failures or other runtime errors. The caller is responsible for freeing the returned string with `SituationFreeString()`.
```c
SituationError SituationGetLastErrorMsg(char** out_msg);
```
**Usage Example:**
```c
if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
    char* error_msg = NULL;
    if (SituationGetLastErrorMsg(&error_msg) == SITUATION_SUCCESS) {
        fprintf(stderr, "Initialization failed: %s\n", error_msg);
        SituationFreeString(error_msg); // IMPORTANT: Free the memory
    }
    return -1;
}
```

---
#### `SituationGetLastErrorCode`
Returns the **`SituationError`** enum value from the most recent call to `_SituationSetErrorFromCode` inside the library. Use this when you need a stable, programmatic error category (logging, telemetry, switch statements) without parsing the detail string from **`SituationGetLastErrorMsg`**.

```c
SituationError SituationGetLastErrorCode(void);
```

**Returns:** The last error code, or **`SITUATION_SUCCESS`** (0) if no error was recorded on the current context.

**Thread safety:** Safe from the main thread after **`SituationInit`**. After **`SituationShutdown`**, the error buffer is not updated (see v2.4.104 mutex guard).

**Usage Example:**
```c
if (SituationLoadShader("a.vert", "a.frag", &shader) != SITUATION_SUCCESS) {
    SituationError code = SituationGetLastErrorCode();
    fprintf(stderr, "Load failed: %s (%d)\n", SituationErrorToString(code), (int)code);
}
```

**See also:** **`SituationGetLastErrorMsg`**, **`SituationErrorToString`**, **`sit/situation_base_errno.h`** (full enum).

---
#### `SituationErrorToString`
Maps any **`SituationError`** value to a short, human-readable label from the errno table (e.g. **`"Shader compile or link still in progress (poll again next frame)"`** for **`SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS`**). The returned pointer is static string storage — **do not free**.

```c
const char* SituationErrorToString(SituationError err);
```

**Parameters:**
- `err` — Any **`SituationError`** value, including codes not produced by the last API call.

**Returns:** Non-null C string label. Unknown codes fall back to a generic `"Unknown SituationError"` label.

**Usage Example:**
```c
SituationError poll = SituationPollShaderLoad(shader);
if (poll == SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS) {
    /* Expected while async compile runs — not a hard failure */
} else if (poll != SITUATION_SUCCESS) {
    fprintf(stderr, "Shader load failed: %s\n", SituationErrorToString(poll));
}
```

**Notes:**
- Prefer **`SituationGetLastErrorMsg`** when you need compiler/driver detail (GLSL line numbers, Vulkan validation text).
- Pair with **`SituationGetLastErrorCode`** after failures for structured handling.
- For log levels, filtering, and custom sinks, see **[Logging Module](logging.md)**.

---
#### `SituationSetExitCallback`
Registers a callback function to be executed just before the library shuts down. This is useful for performing final cleanup tasks, such as saving application state.
```c
typedef void (*SituationExitCallback)(void* user_data);
void SituationSetExitCallback(SituationExitCallback callback, void* user_data);
```
**Usage Example:**
```c
void on_exit_cleanup(void* user_data) {
    const char* message = (const char*)user_data;
    printf("Shutting down. Custom data: %s\n", message);
    // You could save game settings or user progress here.
}

// In main, after Init
const char* my_data = "Saving settings...";
SituationSetExitCallback(on_exit_cleanup, (void*)my_data);
```

---
#### `SituationIsArgumentPresent`
Checks if a specific command-line argument flag (e.g., `"-server"`) was provided when the application was launched. The library automatically parses `argv` during `SituationInit`.
```c
bool SituationIsArgumentPresent(const char* arg_name);
```
**Usage Example:**
```c
// Run as: ./my_app -fullscreen -debug
int main(int argc, char* argv[]) {
    SituationInit(argc, argv, NULL);

    // Check for a simple flag to enable fullscreen mode at startup.
    if (SituationIsArgumentPresent("-fullscreen")) {
        SituationToggleFullscreen();
    }
    // Check for a debug flag.
    if (SituationIsArgumentPresent("-debug")) {
        g_enable_debug_mode = true;
    }
}
```

---
#### `SituationGetArgumentValue`
Gets the value associated with a command-line argument. It supports both colon-separated (`-level:jungle`) and space-separated (`-level jungle`) formats. Returns `NULL` if the argument is not found.
```c
const char* SituationGetArgumentValue(const char* arg_name);
```
**Usage Example:**
```c
// Run as: ./my_app -level:forest -player "Jules"
int main(int argc, char* argv[]) {
    SituationInit(argc, argv, NULL);

    // Get the level name to load.
    const char* level_name = SituationGetArgumentValue("-level");
    if (level_name) {
        printf("Loading level: %s\n", level_name);
    } else {
        printf("Loading default level.\n");
    }

    // Get the player's name.
    const char* player_name = SituationGetArgumentValue("-player");
    if (player_name) {
        printf("Welcome, %s!\n", player_name);
    }
}
```

---
#### `SituationGetVersionString`
Gets the version of the Situation library as a string.
```c
SITAPI const char* SituationGetVersionString(void);
```
**Usage Example:**
```c
const char* version = SituationGetVersionString();
printf("Situation library version: %s\n", version);
```

---
#### `SituationGetGPUName`
Gets the human-readable name of the active GPU.
```c
SITAPI const char* SituationGetGPUName(void);
```
**Usage Example:**
```c
const char* gpu_name = SituationGetGPUName();
printf("GPU: %s\n", gpu_name);
```

---
#### `SituationGetOSInfo`
Returns a struct containing the operating system's product name, version string, and build number. Added in v2.4.199.
```c
SITAPI SituationOSInfo SituationGetOSInfo(void);
```

**Returns:** A `SituationOSInfo` struct with the following fields:
```c
typedef struct SituationOSInfo {
    char name[64];          // OS product name (e.g., "Windows 11", "Ubuntu 24.04", "macOS Sequoia")
    char version[64];       // Full version string (e.g., "10.0.22631", "6.8.0-45-generic")
    uint32_t build_number;  // Build number (Windows) or kernel patch level (Linux); 0 if unavailable
} SituationOSInfo;
```

**Usage Example:**
```c
SituationOSInfo os = SituationGetOSInfo();
printf("OS: %s (version %s, build %u)\n", os.name, os.version, os.build_number);
```

**Notes:**
- On Windows, uses `RtlGetVersion` for accurate build info (avoids compatibility shim issues with `GetVersionEx`)
- On Linux/macOS, uses `uname()` for kernel version
- Safe to call at any point after `SituationInit()`

---
#### `SituationGetProcessList`
Returns a heap-allocated array of `SituationProcessInfo` structs representing all currently running OS-level processes. Added in v2.4.199.
```c
SITAPI SituationProcessInfo* SituationGetProcessList(int* out_count);
```

**Parameters:**
- `out_count` - Pointer to an int that receives the number of processes in the returned array

**Returns:** A dynamically allocated array of `SituationProcessInfo` structs. The caller must free this with `SituationFreeProcessList()`. Returns `NULL` on failure.

```c
typedef struct SituationProcessInfo {
    uint32_t pid;                                   // Process ID
    char name[SITUATION_MAX_PROCESS_NAME_LEN];      // Executable name (e.g., "explorer.exe")
    uint64_t memory_bytes;                          // Working set / RSS in bytes
} SituationProcessInfo;
```

**Usage Example:**
```c
int count = 0;
SituationProcessInfo* procs = SituationGetProcessList(&count);
if (procs) {
    printf("Running processes: %d\n", count);
    for (int i = 0; i < count; i++) {
        printf("  PID %u: %s (%.1f MB)\n",
            procs[i].pid, procs[i].name,
            procs[i].memory_bytes / (1024.0 * 1024.0));
    }
    SituationFreeProcessList(procs, count);
}
```

**Notes:**
- On Windows, uses `CreateToolhelp32Snapshot` + `Process32First/Next`
- On Linux, reads `/proc` filesystem
- Memory field reports working set (Windows) or RSS (Linux)

---
#### `SituationFreeProcessList`
Frees a process list previously returned by `SituationGetProcessList()`. Added in v2.4.199.
```c
SITAPI void SituationFreeProcessList(SituationProcessInfo* list, int count);
```

**Parameters:**
- `list` - The array returned by `SituationGetProcessList()`
- `count` - The count returned by `SituationGetProcessList()`

---
#### `SituationGetActiveAudioDeviceName`
Returns the name of the currently active (bound) audio playback device. Added in v2.4.199.
```c
SITAPI const char* SituationGetActiveAudioDeviceName(void);
```

**Returns:** A pointer to a static internal buffer containing the device name. Do not free this pointer. Returns an empty string if no device is active.

**Usage Example:**
```c
const char* audio_device = SituationGetActiveAudioDeviceName();
printf("Active audio device: %s\n", audio_device);
printf("Sample rate: %d Hz\n", SituationGetAudioPlaybackSampleRate());
```

**Notes:**
- Returns the device name as reported by the audio backend (miniaudio/WASAPI)
- The returned pointer is valid until the next call to this function or until `SituationShutdown()`
- Useful for diagnostics and system information displays

---
#### `SituationGetVRAMUsage`
Gets the estimated total Video RAM (VRAM) usage in bytes. This is a best-effort query and may not be perfectly accurate on all platforms.
```c
SITAPI uint64_t SituationGetVRAMUsage(void);
```
**Usage Example:**
```c
uint64_t vram_usage = SituationGetVRAMUsage();
printf("VRAM Usage: %.2f MB\n", (double)vram_usage / (1024.0 * 1024.0));
```

---
#### `SituationGetDrawCallCount`
Gets the number of draw calls submitted in the last completed frame. This is a key performance metric for identifying rendering bottlenecks.
```c
SITAPI uint32_t SituationGetDrawCallCount(void);
```
**Usage Example:**
```c
// In the update loop, display the draw call count in the window title.
char title[256];
sprintf(title, "My App | FPS: %d | Draw Calls: %u",
        SituationGetFPS(), SituationGetDrawCallCount());
SituationSetWindowTitle(title);
```

---
#### `SituationExecuteCommand`
Executes a system shell command in a hidden process and captures the output (stdout/stderr).
```c
SITAPI int SituationExecuteCommand(const char *cmd, char **output);
```
**Usage Example:**
```c
char* output = NULL;
int exit_code = SituationExecuteCommand("ls -la", &output);
if (exit_code == 0 && output) {
    printf("Command output:\n%s\n", output);
    SituationFreeString(output);
}
```

---
#### `SituationGetCPUThreadCount`
Gets the number of logical CPU cores available on the system. This is useful for determining optimal thread pool sizes or parallelization strategies.

```c
uint32_t SituationGetCPUThreadCount(void);
```

**Returns:** Number of logical CPU cores (includes hyperthreading/SMT cores)

**Usage Example:**
```c
// Create a thread pool sized for the system
uint32_t core_count = SituationGetCPUThreadCount();
printf("System has %u logical cores\n", core_count);

// Use 75% of cores for worker threads, leaving some for OS/other apps
uint32_t worker_threads = (core_count * 3) / 4;
if (worker_threads < 1) worker_threads = 1;

SituationThreadPool* pool = SituationCreateThreadPool(worker_threads);

// Or use for parallel processing decisions
if (core_count >= 8) {
    // Use multi-threaded asset loading
    LoadAssetsParallel();
} else {
    // Use single-threaded loading on low-core systems
    LoadAssetsSequential();
}
```

**Notes:**
- Returns logical cores, not physical cores (includes hyperthreading)
- On systems with heterogeneous cores (e.g., ARM big.LITTLE), returns total count
- Useful for sizing thread pools and making parallelization decisions

---
#### `SituationGetMaxComputeWorkGroups`
Queries the maximum supported compute shader work group count for each dimension (X, Y, Z). This tells you the hardware limits for `SituationCmdDispatch()` calls.

```c
void SituationGetMaxComputeWorkGroups(uint32_t* x, uint32_t* y, uint32_t* z);
```

**Parameters:**
- `x` - Pointer to receive maximum work groups in X dimension
- `y` - Pointer to receive maximum work groups in Y dimension
- `z` - Pointer to receive maximum work groups in Z dimension

**Usage Example:**
```c
// Query hardware limits
uint32_t max_x, max_y, max_z;
SituationGetMaxComputeWorkGroups(&max_x, &max_y, &max_z);
printf("Max compute work groups: %u x %u x %u\n", max_x, max_y, max_z);

// Ensure dispatch doesn't exceed limits
uint32_t required_groups_x = (particle_count + 255) / 256;
if (required_groups_x > max_x) {
    printf("Error: Too many particles for single dispatch\n");
    // Split into multiple dispatches or use 2D/3D layout
}

// Safe dispatch
SituationCmdDispatch(cmd,
    min(required_groups_x, max_x),
    1,
    1);
```

**Advanced Example (2D Image Processing):**
```c
// Process a large image with compute shader
int image_width = 4096, image_height = 4096;
int local_size_x = 16, local_size_y = 16;

// Calculate required work groups
uint32_t groups_x = (image_width + local_size_x - 1) / local_size_x;
uint32_t groups_y = (image_height + local_size_y - 1) / local_size_y;

// Check against hardware limits
uint32_t max_x, max_y, max_z;
SituationGetMaxComputeWorkGroups(&max_x, &max_y, &max_z);

if (groups_x <= max_x && groups_y <= max_y) {
    // Single dispatch covers entire image
    SituationCmdDispatch(cmd, groups_x, groups_y, 1);
} else {
    // Need to tile the dispatch
    printf("Image too large, using tiled dispatch\n");
    // Implement tiling logic...
}
```

**Notes:**
- Typical values: 65535 x 65535 x 65535 (Vulkan minimum guarantee)
- OpenGL may have lower limits on older hardware
- Total invocations = groups_x × groups_y × groups_z × local_size_x × local_size_y × local_size_z
- Query once at initialization and cache the values
- Use 2D or 3D dispatches for large datasets that exceed 1D limits

---
#### `SituationLogWarning`

Debug-build helper that stores an error and prints to stderr. **Release builds (`NDEBUG`):** no-op. Full documentation: **[Logging Module](logging.md)**.

```c
SITAPI void SituationLogWarning(SituationError code, const char* fmt, ...);
```
**Usage Example:**
```c
if (score > 9000) {
    SituationLogWarning(SITUATION_ERROR_GENERAL, "Score is over 9000!");
}
```

---
#### `SituationGetDeviceInfo` _(deprecated — v2.4.207)_
Returns a comprehensive hardware snapshot in one struct. **Deprecated:** prefer the split queries below (`SituationGetCPUInfo`, `SituationGetGPUInfo`, `SituationGetMemoryInfo`, and the storage/network/input enumeration helpers). This function still works — it composes the aggregate from those helpers plus a GLFW monitor summary.
```c
SituationDeviceInfo SituationGetDeviceInfo(void);
```
**Usage Example:**
```c
// Prefer split queries (no deprecation warnings). Aggregate still valid for legacy code:
SituationInit(argc, argv, NULL);
SituationCPUInfo cpu;
SituationGPUInfo gpu;
SituationMemoryInfo mem;
SituationGetCPUInfo(&cpu);
SituationGetGPUInfo(&gpu);
SituationGetMemoryInfo(&mem);
SituationOSInfo os = SituationGetOSInfo();
printf("--- System Information ---\n");
printf("OS: %s %s (build %u)\n", os.name, os.version, os.build_number);
printf("CPU: %s (%u threads, %.1f GHz)\n", cpu.name, cpu.thread_count, cpu.clock_speed_ghz);
printf("RAM: %.2f GB free / %.2f GB total\n",
    (double)mem.available_bytes / (1024.0*1024.0*1024.0),
    (double)mem.total_bytes / (1024.0*1024.0*1024.0));
printf("GPU: %s\n", gpu.name);
printf("VRAM: %.2f GB\n", (double)gpu.dedicated_memory_bytes / (1024.0*1024.0*1024.0));
printf("--------------------------\n");
```

---
#### `SituationGetCPUInfo` _(v2.4.207)_
Returns CPU brand string, logical thread count, physical core count, and base clock (when available). Platform logic is shared with the deprecated aggregate.
```c
void SituationGetCPUInfo(SituationCPUInfo* out);
```
**Struct:**
```c
typedef struct SituationCPUInfo {
    char name[SITUATION_MAX_CPU_NAME_LEN];
    uint32_t thread_count;
    uint32_t core_count;
    float clock_speed_ghz;
} SituationCPUInfo;
```
**Usage Example:**
```c
SituationCPUInfo cpu;
SituationGetCPUInfo(&cpu);
printf("CPU: %s — %u threads / %u cores @ %.2f GHz\n",
    cpu.name, cpu.thread_count, cpu.core_count, cpu.clock_speed_ghz);
```

---
#### `SituationGetGPUInfo` _(v2.4.207)_
Returns the primary GPU name and dedicated VRAM (accurate via DXGI on Windows when available; otherwise from the active graphics backend).
```c
void SituationGetGPUInfo(SituationGPUInfo* out);
```
**Struct:**
```c
typedef struct SituationGPUInfo {
    char name[SITUATION_MAX_GPU_NAME_LEN];
    uint64_t dedicated_memory_bytes;
} SituationGPUInfo;
```
**Usage Example:**
```c
SituationGPUInfo gpu;
SituationGetGPUInfo(&gpu);
printf("GPU: %s (%.1f GB VRAM)\n", gpu.name,
    (double)gpu.dedicated_memory_bytes / (1024.0 * 1024.0 * 1024.0));
```

---
#### `SituationGetMemoryInfo` _(v2.4.207)_
Returns total and available physical RAM in bytes.
```c
void SituationGetMemoryInfo(SituationMemoryInfo* out);
```
**Struct:**
```c
typedef struct SituationMemoryInfo {
    uint64_t total_bytes;
    uint64_t available_bytes;
} SituationMemoryInfo;
```
**Usage Example:**
```c
SituationMemoryInfo mem;
SituationGetMemoryInfo(&mem);
printf("RAM: %.1f GB free / %.1f GB total\n",
    mem.available_bytes / (1024.0 * 1024.0 * 1024.0),
    mem.total_bytes / (1024.0 * 1024.0 * 1024.0));
```

---
#### `SituationGetStorageDeviceCount` / `SituationGetStorageDevice` _(v2.4.207)_
Enumerates storage volumes reported by the OS. Use count + index loop (same pattern as monitor queries). On Windows, enumerates logical drives; on Linux/macOS, root filesystem.
```c
int SituationGetStorageDeviceCount(void);
bool SituationGetStorageDevice(int index, char* out_name, int name_len,
    uint64_t* out_capacity_bytes, uint64_t* out_free_bytes);
```
**Usage Example:**
```c
int n = SituationGetStorageDeviceCount();
for (int i = 0; i < n; i++) {
    char name[SITUATION_MAX_DEVICE_NAME_LEN];
    uint64_t cap = 0, free = 0;
    if (SituationGetStorageDevice(i, name, sizeof(name), &cap, &free)) {
        printf("Storage[%d]: %s — %.1f GB (%.1f GB free)\n", i, name,
            cap / (1024.0 * 1024.0 * 1024.0), free / (1024.0 * 1024.0 * 1024.0));
    }
}
```

---
#### `SituationGetNetworkAdapterCount` / `SituationGetNetworkAdapterName` _(v2.4.207)_
Enumerates network adapters (friendly name on Windows; interface name on Linux/macOS).
```c
int SituationGetNetworkAdapterCount(void);
bool SituationGetNetworkAdapterName(int index, char* out_name, int name_len);
```
**Usage Example:**
```c
for (int i = 0; i < SituationGetNetworkAdapterCount(); i++) {
    char name[SITUATION_MAX_DEVICE_NAME_LEN];
    if (SituationGetNetworkAdapterName(i, name, sizeof(name)))
        printf("Net[%d]: %s\n", i, name);
}
```

---
#### `SituationGetInputDeviceCount` / `SituationGetInputDeviceName` _(v2.4.207)_
Enumerates keyboards, mice, and gamepad-like HID devices (platform-dependent).
```c
int SituationGetInputDeviceCount(void);
bool SituationGetInputDeviceName(int index, char* out_name, int name_len);
```
**Usage Example:**
```c
for (int i = 0; i < SituationGetInputDeviceCount(); i++) {
    char name[SITUATION_MAX_DEVICE_NAME_LEN];
    if (SituationGetInputDeviceName(i, name, sizeof(name)))
        printf("Input[%d]: %s\n", i, name);
}
```

---
#### `SituationGetOSInfo` _(v2.4.199)_
Returns operating system name, version string, and build number. Cross-platform: Windows (RtlGetVersion), Linux (/etc/os-release + uname), macOS (sysctlbyname).
```c
SituationOSInfo SituationGetOSInfo(void);
```
**Struct:**
```c
typedef struct SituationOSInfo {
    char name[64];           // e.g., "Windows 11", "Ubuntu 24.04"
    char version[64];        // e.g., "10.0.22631", "6.8.0-45-generic"
    uint32_t build_number;   // Windows build number; 0 on other platforms
} SituationOSInfo;
```
**Usage Example:**
```c
SituationOSInfo os = SituationGetOSInfo();
printf("Running on: %s (%s, build %u)\n", os.name, os.version, os.build_number);
```

---
#### `SituationGetProcessList` _(v2.4.199)_
Returns a snapshot of all running OS processes. Each entry contains the process ID, executable name, and working set memory. Caller must free the returned array with `SituationFreeProcessList()`.
```c
SituationProcessInfo* SituationGetProcessList(int* out_count);
void SituationFreeProcessList(SituationProcessInfo* list, int count);
```
**Struct:**
```c
typedef struct SituationProcessInfo {
    uint32_t pid;                                   // Process ID
    char name[SITUATION_MAX_PROCESS_NAME_LEN];      // Executable name
    uint64_t memory_bytes;                          // Working set / RSS
} SituationProcessInfo;
```
**Usage Example:**
```c
int count = 0;
SituationProcessInfo* procs = SituationGetProcessList(&count);
for (int i = 0; i < count; i++) {
    printf("PID %u: %s (%.1f MB)\n", procs[i].pid, procs[i].name,
        procs[i].memory_bytes / (1024.0 * 1024.0));
}
SituationFreeProcessList(procs, count);
```

---
#### `SituationGetActiveAudioDeviceName` _(v2.4.199)_
Returns the name of the currently active audio playback device. Returns a pointer to a static buffer — do not free.
```c
const char* SituationGetActiveAudioDeviceName(void);
```
**Usage Example:**
```c
printf("Playing through: %s\n", SituationGetActiveAudioDeviceName());
```

---
#### `SituationGetTime`
Gets the total elapsed time in seconds since `SituationInit()` was called. This is a high-precision monotonic timer.
```c
double SituationGetTime(void);
```
**Usage Example:**
```c
// Use the total elapsed time to drive a continuous animation, like a rotation.
double current_time = SituationGetTime();
mat4 rotation_matrix;
glm_rotate_y(model_matrix, (float)current_time * 0.5f, rotation_matrix); // Rotate over time
```

---
#### `SituationGetFrameCount`
Gets the total number of frames that have been rendered since the application started.
```c
uint64_t SituationGetFrameCount(void);
```
**Usage Example:**
```c
// Use the frame count for simple, periodic logic that doesn't need to be tied to real time.
if (SituationGetFrameCount() % 120 == 0) {
    printf("120 frames have passed.\n");
}
```

---
#### `SituationWaitTime`
Pauses the application thread for a specified duration in seconds. This is a simple wrapper over the system's sleep function and can be useful for debugging or simple timing.
```c
void SituationWaitTime(double seconds);
```
**Usage Example:**
```c
printf("Preparing to load assets...\n");
// Wait for 500 milliseconds before proceeding to give the user time to read the message.
SituationWaitTime(0.5);
printf("Loading...\n");
```

---
#### `SituationEnableEventWaiting`
Enables event waiting. When enabled, `SituationPollInputEvents()` will wait for new events instead of immediately returning, putting the application to sleep and saving CPU cycles when idle. This is ideal for tools and non-game applications.
```c
void SituationEnableEventWaiting(void);
```
**Usage Example:**
```c
// In an editor or tool, enable event waiting to reduce resource usage.
SituationEnableEventWaiting();
while (!SituationWindowShouldClose()) {
    SituationPollInputEvents(); // This will now block until an event occurs.
    // ... update UI or process data only when there are new events ...
    // ... render ...
}
```

---
#### `SituationDisableEventWaiting`
Disables event waiting, restoring the default behavior where `SituationPollInputEvents()` returns immediately. This is necessary for real-time applications like games that need to run the update loop continuously.
```c
void SituationDisableEventWaiting(void);
```
**Usage Example:**
```c
// When switching from an editor mode to a real-time game simulation.
SituationDisableEventWaiting();
```

---
#### `SituationOpenFile`
Asks the operating system to open a file or folder with its default application.
```c
void SituationOpenFile(const char* filePath);
```
**Usage Example:**
```c
// This will open the specified file in its default application (e.g., Notepad).
SituationOpenFile("C:/path/to/your/log.txt");

// This will open the specified directory in the file explorer.
SituationOpenFile("C:/Users/Default/Documents");
```

---
#### `SituationOpenURL`
Asks the operating system to open a URL in the default web browser.
```c
void SituationOpenURL(const char* url);
```
**Usage Example:**
```c
// This will open the user's web browser to the specified URL.
SituationOpenURL("https://www.github.com");
```

---
#### `SituationSetErrorCallback`
Sets a callback for handling library errors.
```c
void SituationSetErrorCallback(SituationErrorCallback callback);
```
**Usage Example:**
```c
void my_error_logger(int error_code, const char* message) {
    fprintf(stderr, "Situation Error [%d]: %s\n", error_code, message);
}

// In main, after Init
SituationSetErrorCallback(my_error_logger);
```

---
#### `SituationSetVSync`
Enables or disables VSync.
```c
void SituationSetVSync(bool enabled);
```
**Usage Example:**
```c
// Disable VSync for performance testing
SituationSetVSync(false);
```

---
#### `SituationGetPlatform`
Gets the current platform.
```c
int SituationGetPlatform(void);
```
**Usage Example:**
```c
int platform = SituationGetPlatform();
#if defined(PLATFORM_DESKTOP)
    if (platform == PLATFORM_DESKTOP) printf("Running on a desktop platform.\n");
#endif
```

---
#### `SituationUpdate`
**[DEPRECATED]** This function has been split into separate calls for better control. Use `SituationPollInputEvents()` and `SituationUpdateTimers()` instead.

```c
void SituationUpdate(void);  // DEPRECATED
```

**Migration Guide:**

**Old Code:**
```c
while (!SituationShouldClose()) {
    SituationUpdate();  // DEPRECATED

    // Game logic
    UpdateGame();
    RenderGame();
}
```

**New Code:**
```c
while (!SituationShouldClose()) {
    SituationPollInputEvents();  // Process input events
    SituationUpdateTimers();     // Update oscillators and timers

    // Game logic
    UpdateGame();
    RenderGame();
}
```

**Why the change?**
- Better control over update order
- Can skip timer updates if needed
- More explicit about what's being updated
- Allows custom timing logic

**Notes:**
- `SituationUpdate()` still works but is deprecated
- Internally calls `SituationPollInputEvents()` and `SituationUpdateTimers()`
- Will be removed in a future version

---
#### `SituationSetResizeCallback`
Sets a callback function for window framebuffer resize events.
```c
void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data);
```

---

#### `SituationSetFocusCallback`

Sets a callback function to be called when the window gains or loses focus.

```c
SITAPI void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data);
```

**Usage Example:**
```c
void OnFocusChanged(bool focused, void* user_data) {
    printf("Window focus changed: %d\n", focused);
}

SituationSetFocusCallback(OnFocusChanged, NULL);
```

---
#### `SituationSetFocusCallback`
Sets a callback for window focus events.
```c
void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data);
```

---
#### `SituationSetFileDropCallback`
Sets a callback for file drop events.
```c
void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data);
```

---
#### `SituationGetUserDirectory`
Gets the full path to the current user's home directory. The returned string must be freed with `SituationFreeString()`.

```c
char* SituationGetUserDirectory(void);
```

**Returns:** Dynamically allocated string containing the user's home directory path (must be freed)

**Usage Example:**
```c
// Get user's home directory for storing user-specific files
char* home_dir = SituationGetUserDirectory();
printf("User home directory: %s\n", home_dir);

// Build a path to a user-specific config file
char config_path[512];
snprintf(config_path, sizeof(config_path), "%s/.mygame/config.ini", home_dir);

// Load user config
if (SituationFileExists(config_path)) {
    // Load existing config
    char* config_data = SituationLoadFileText(config_path);
    ParseConfig(config_data);
    SituationFreeString(config_data);
}

// Always free the returned string
SituationFreeString(home_dir);
```

**Notes:**
- Returns platform-specific home directory:
  - Windows: `C:\Users\Username`
  - Linux: `/home/username`
  - macOS: `/Users/username`
- The returned string must be freed with `SituationFreeString()`
- For application-specific save data, consider using `SituationGetAppSavePath()` instead

---
#### `SituationGetCurrentDriveLetter`
Gets the drive letter of the running executable (Windows only). Returns '\0' on non-Windows platforms.

```c
char SituationGetCurrentDriveLetter(void);
```

**Returns:** Drive letter (e.g., 'C', 'D') or '\0' on error/non-Windows

**Usage Example:**
```c
// Get current drive
char drive = SituationGetCurrentDriveLetter();
if (drive != '\0') {
    printf("Running on drive: %c:\n", drive);
}

// Check if running on specific drive
char current_drive = SituationGetCurrentDriveLetter();
if (current_drive == 'C') {
    printf("Running on system drive\n");
} else {
    printf("Running on external drive: %c:\n", current_drive);
}

// Build path on current drive
char drive = SituationGetCurrentDriveLetter();
char save_path[256];
snprintf(save_path, sizeof(save_path), "%c:/GameSaves/save.dat", drive);

// Cross-platform check
#ifdef _WIN32
    char drive = SituationGetCurrentDriveLetter();
    printf("Windows drive: %c:\n", drive);
#else
    printf("Not on Windows, no drive letters\n");
#endif
```

**Notes:**
- **Windows only** - returns '\0' on Linux/macOS
- Returns uppercase letter (A-Z)
- Useful for portable app installations
- Use `SituationGetBasePath()` for cross-platform paths

---
#### `SituationGetDriveInfo`
**[Windows Only]** Gets information about a specific drive including total capacity, free space, and volume name.

```c
bool SituationGetDriveInfo(char drive_letter, uint64_t* out_total_capacity_bytes, uint64_t* out_free_space_bytes, char* out_volume_name, int volume_name_len);
```

**Parameters:**
- `drive_letter` - Drive letter to query (e.g., 'C', 'D')
- `out_total_capacity_bytes` - Pointer to receive total drive capacity
- `out_free_space_bytes` - Pointer to receive free space available
- `out_volume_name` - Buffer to receive volume name (can be NULL)
- `volume_name_len` - Size of volume name buffer

**Returns:** `true` if drive exists and info retrieved, `false` otherwise

**Usage Example:**
```c
// Check C: drive space
uint64_t total_bytes, free_bytes;
char volume_name[256];

if (SituationGetDriveInfo('C', &total_bytes, &free_bytes, volume_name, sizeof(volume_name))) {
    float total_gb = total_bytes / (1024.0f * 1024.0f * 1024.0f);
    float free_gb = free_bytes / (1024.0f * 1024.0f * 1024.0f);
    float used_percent = ((total_bytes - free_bytes) / (float)total_bytes) * 100.0f;

    printf("Drive C: [%s]\n", volume_name);
    printf("Total: %.2f GB\n", total_gb);
    printf("Free: %.2f GB\n", free_gb);
    printf("Used: %.1f%%\n", used_percent);

    // Warn if low on space
    if (free_gb < 10.0f) {
        printf("WARNING: Low disk space!\n");
    }
}

// Check if save location has enough space
uint64_t required_space = 500 * 1024 * 1024;  // 500 MB
if (SituationGetDriveInfo('D', NULL, &free_bytes, NULL, 0)) {
    if (free_bytes < required_space) {
        printf("Not enough space to save game\n");
    }
}
```

**Notes:**
- Windows only - returns `false` on other platforms
- Drive letter is case-insensitive
- Volume name can be NULL if not needed
- Useful for save game space checks and installation validation

---
#### `SituationFreeDisplays`
Frees the memory allocated for the display list returned by `SituationGetDisplays()`. Always call this after you're done using the display information.

```c
void SituationFreeDisplays(SituationDisplayInfo* displays, int count);
```

**Parameters:**
- `displays` - Pointer to the display array to free
- `count` - Number of displays in the array

**Usage Example:**
```c
// Get display information
int display_count;
SituationDisplayInfo* displays = SituationGetDisplays(&display_count);

// Use the display info
for (int i = 0; i < display_count; i++) {
    printf("Display %d: %dx%d @ %dHz\n",
        i,
        displays[i].width,
        displays[i].height,
        displays[i].refresh_rate);
}

// Always free when done
SituationFreeDisplays(displays, display_count);
```

**Notes:**
- Must be called to avoid memory leaks
- Pass the same count returned by `SituationGetDisplays()`
- Safe to call with NULL pointer (does nothing)

---
#### `SituationGetWindowSize`
Gets the current logical window size in screen coordinates. This is the size of the window's client area, excluding decorations.

```c
void SituationGetWindowSize(int* width, int* height);
```

**Parameters:**
- `width` - Pointer to receive window width
- `height` - Pointer to receive window height

**Usage Example:**
```c
// Get current window size
int window_width, window_height;
SituationGetWindowSize(&window_width, &window_height);
printf("Window size: %dx%d\n", window_width, window_height);

// Calculate aspect ratio
float aspect_ratio = (float)window_width / (float)window_height;
printf("Aspect ratio: %.2f:1\n", aspect_ratio);

// Adjust viewport to match window
SituationCmdSetViewport(cmd, 0, 0, window_width, window_height);

// Center UI element
int ui_width = 400, ui_height = 300;
int ui_x = (window_width - ui_width) / 2;
int ui_y = (window_height - ui_height) / 2;
DrawUIPanel(ui_x, ui_y, ui_width, ui_height);

// Check if window was resized
static int last_width = 0, last_height = 0;
if (window_width != last_width || window_height != last_height) {
    printf("Window resized from %dx%d to %dx%d\n",
        last_width, last_height, window_width, window_height);
    OnWindowResize(window_width, window_height);
    last_width = window_width;
    last_height = window_height;
}
```

**Notes:**
- Returns logical size, not physical pixels (important for high-DPI displays)
- For render target size, use `SituationGetRenderWidth()` and `SituationGetRenderHeight()`
- Window size may differ from render size due to DPI scaling
- Excludes window decorations (title bar, borders)

---
#### `SituationGetRendererType`
Gets the current active graphics renderer backend (OpenGL or Vulkan). This is determined at initialization time based on system capabilities and user preferences.

```c
SituationRendererType SituationGetRendererType(void);
```

**Returns:** `SITUATION_RENDERER_OPENGL` or `SITUATION_RENDERER_VULKAN`

**Usage Example:**
```c
// Check which renderer is active
SituationRendererType renderer = SituationGetRendererType();

if (renderer == SITUATION_RENDERER_VULKAN) {
    printf("Running on Vulkan backend\n");
    // Enable Vulkan-specific optimizations
    EnableBindlessTextures();
    EnableAsyncCompute();
} else if (renderer == SITUATION_RENDERER_OPENGL) {
    printf("Running on OpenGL backend\n");
    // Use OpenGL-compatible code paths
    DisableBindlessTextures();
}

// Adjust quality settings based on renderer
if (renderer == SITUATION_RENDERER_VULKAN) {
    // Vulkan can handle more complex effects
    SetShadowQuality(QUALITY_ULTRA);
    SetParticleCount(10000);
} else {
    // OpenGL may need lower settings
    SetShadowQuality(QUALITY_HIGH);
    SetParticleCount(5000);
}
```

**Notes:**
- The renderer is selected at `SituationInit()` time and cannot be changed at runtime
- Vulkan is preferred when available for better performance
- OpenGL is used as a fallback on older systems or when Vulkan is unavailable
- Some features (like bindless textures) may only be available on Vulkan

---
#### `SituationGetGLFWwindow`
Gets the raw GLFW window handle.
```c
GLFWwindow* SituationGetGLFWwindow(void);
```

---
#### `SituationGetVulkanInstance`
Gets the raw Vulkan instance handle for advanced Vulkan interop. This allows direct access to Vulkan API for custom extensions or advanced features.

```c
VkInstance SituationGetVulkanInstance(void);
```

**Returns:** Vulkan instance handle, or NULL if using OpenGL backend

**Usage Example:**
```c
// Query Vulkan extension support
VkInstance instance = SituationGetVulkanInstance();
if (instance != NULL) {
    // Use Vulkan API directly
    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    printf("Vulkan extensions available: %u\n", extension_count);
}
```

**Notes:**
- Returns NULL on OpenGL backend
- Use for advanced Vulkan features not exposed by Situation API
- Be careful not to interfere with Situation's internal state

---
#### `SituationGetVulkanDevice`
Gets the raw Vulkan logical device handle for advanced Vulkan interop.

```c
VkDevice SituationGetVulkanDevice(void);
```

**Returns:** Vulkan device handle, or NULL if using OpenGL backend

**Usage Example:**
```c
// Create custom Vulkan resources
VkDevice device = SituationGetVulkanDevice();
if (device != NULL) {
    // Use Vulkan API for custom operations
    VkFence custom_fence;
    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(device, &fence_info, NULL, &custom_fence);
}
```

**Notes:**
- Returns NULL on OpenGL backend
- Useful for creating custom Vulkan objects
- Ensure compatibility with Situation's resource management

---
#### `SituationGetVulkanPhysicalDevice`
Gets the raw Vulkan physical device handle for querying hardware capabilities.

```c
VkPhysicalDevice SituationGetVulkanPhysicalDevice(void);
```

**Returns:** Vulkan physical device handle, or NULL if using OpenGL backend

**Usage Example:**
```c
// Query hardware properties
VkPhysicalDevice physical_device = SituationGetVulkanPhysicalDevice();
if (physical_device != NULL) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    printf("GPU: %s\n", props.deviceName);
    printf("Vulkan API: %u.%u.%u\n",
        VK_VERSION_MAJOR(props.apiVersion),
        VK_VERSION_MINOR(props.apiVersion),
        VK_VERSION_PATCH(props.apiVersion));
}
```

**Notes:**
- Returns NULL on OpenGL backend
- Use for querying hardware limits and features
- Read-only queries are safe

---
#### `SituationGetMainWindowRenderPass`
**[Vulkan Only]** Gets the Vulkan render pass handle for the main window's swapchain. This is useful for advanced Vulkan interop and custom rendering.

```c
VkRenderPass SituationGetMainWindowRenderPass(void);
```

**Returns:** Vulkan render pass handle, or NULL if using OpenGL backend

**Usage Example:**
```c
// Get main window render pass for custom Vulkan operations
VkRenderPass main_render_pass = SituationGetMainWindowRenderPass();
if (main_render_pass != VK_NULL_HANDLE) {
    // Use for creating compatible framebuffers or pipelines
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .renderPass = main_render_pass,
        // ... other pipeline settings ...
    };
}
```

**Notes:**
- Returns VK_NULL_HANDLE on OpenGL backend
- The render pass is managed by Situation - do not destroy it
- Useful for creating custom Vulkan pipelines compatible with the main window
- Render pass format matches the swapchain format

---
#### `SituationGetInitState`
Returns the library initialization state as a thread-safe **`SituationInitState`** enum. Use this to detect partial init, shutdown in progress, or re-entrancy during tooling that calls **`SituationInit`** / **`SituationShutdown`** in one process.

```c
SituationInitState SituationGetInitState(void);
```

**Returns:** One of **`SITUATION_INIT_STATE_UNINITIALIZED`**, **`INITIALIZING`**, **`INITIALIZED`**, **`SHUTTING_DOWN`**, etc. (see header).

**Usage Example:**
```c
if (SituationGetInitState() != SITUATION_INIT_STATE_INITIALIZED) {
    fprintf(stderr, "Situation not ready for this call\n");
    return;
}
```

---
#### `SituationGetGraphicsBackend`
Returns which renderer backend this Situation DLL was built for (`SIT_GRAPHICS_BACKEND_OPENGL` or `SIT_GRAPHICS_BACKEND_VULKAN`). Valid **before** `SituationInit` — use to branch app logic without compile-time `#ifdef`.

```c
SituationGraphicsBackend SituationGetGraphicsBackend(void);
```

---
#### `SituationGetGraphicsBackendName`
Read-only label for `SituationGetGraphicsBackend()` (`"OpenGL"`, `"Vulkan"`, or `"Unknown"`).

```c
const char* SituationGetGraphicsBackendName(void);
```

---
#### `SituationGetGraphicsCaps`
Fills **`SituationGraphicsCaps`** with backend feature flags and device limits. Examples and the test harness use this to skip or classify driver-specific tests.

```c
typedef struct SituationGraphicsCaps {
    uint32_t api_version_packed;        /* Target API: (4<<16)|6 = OpenGL 4.6, (1<<16)|4 = Vulkan 1.4 */
    uint32_t device_api_version_packed; /* Actual device API version reported by driver */
    int      max_msaa_samples;          /* Highest MSAA count supported for color+depth FBOs (e.g. 8, 16) */
    int      max_viewports;             /* Maximum simultaneous viewports */
    int      bindless_textures;         /* Non-zero if bindless texture handles are available */
    int      shader_compiler_available; /* Non-zero if runtime GLSL→SPIR-V (shaderc) is present */
    int      compute_supported;         /* Non-zero if compute shaders are available */
    int      backend;                   /* SituationGraphicsBackend enum value */
} SituationGraphicsCaps;

void SituationGetGraphicsCaps(SituationGraphicsCaps* out_caps);
```

**Parameters:**
- `out_caps` — Output struct; must not be NULL. Zeroed before population; safe to inspect any field after the call.

**`max_msaa_samples`:** On OpenGL, queried via `GL_MAX_SAMPLES` (must be called from the GL context thread — returns 0 if called from the main thread before the render thread has established the context). On Vulkan, derived from `VkPhysicalDeviceLimits::framebufferColorSampleCounts & framebufferDepthSampleCounts` — always correct regardless of calling thread.

**Notes:** **`backend`** and **`SituationGetGraphicsBackend()`** match the active DLL. **`api_version_packed`** is the Situation **target** API (`4.6` OpenGL, `1.4` Vulkan). **`device_api_version_packed`** is the runtime GL context or `VkPhysicalDevice` version (`major<<16|minor`). Safe after successful **`SituationInit`** for device fields.

---
#### `SituationMultisampleQuality` (v2.4.398)

Unified **attachment MSAA quality tier** for Virtual Displays and render targets (VD-4b). In **`situation_api_types_gpu.h`**:

```c
typedef enum SituationMultisampleQuality {
    SITUATION_MULTISAMPLE_OFF = 0,
    SITUATION_MULTISAMPLE_2X,
    SITUATION_MULTISAMPLE_4X,   /* SITUATION_MULTISAMPLE_DEFAULT — aligns with MSAA_4X_HINT */
    SITUATION_MULTISAMPLE_8X,
    SITUATION_MULTISAMPLE_16X,
} SituationMultisampleQuality;

int SituationMultisampleQualitySampleCount(SituationMultisampleQuality q);
SituationMultisampleQuality SituationMultisampleQualityFromSampleCount(int samples);
SituationMultisampleQuality SituationMultisampleQualityClamp(SituationMultisampleQuality q, int max_samples);
```

**v2.4.398:** types and helpers ship; VD stores **`msaa_quality`** from **`SituationVirtualDisplayDesc.msaa_samples`** at create. Values **`> 1`** are still **rejected** until **VD-4b** (MSAA FBO + resolve). Use **`SituationMultisampleQualityClamp(q, caps.max_msaa_samples)`** when planning quality UI.

**Separate from** **`SituationMultisampleState`** / **`SituationCmdSetMultisampleState`** — those are **in-pass raster flags** (sample shading, mask, alpha-to-coverage) on an already multisampled attachment.

---
#### `SituationShowMessageBox`
Displays a blocking native message box (Win32 **`MessageBox`**, platform equivalent elsewhere). Intended for **fatal init errors** when stderr is not visible (GUI apps without a console).

```c
void SituationShowMessageBox(const char* title, const char* message);
```

**Parameters:**
- `title` — Dialog title (UTF-8).
- `message` — Body text (UTF-8).

**Notes:** Blocks until the user dismisses the dialog. Do not call from the audio callback thread.


---

#### `SituationGetFrameSpikeCount`
Count of detected frame spikes (general debugging aid)
```c
uint32_t SituationGetFrameSpikeCount(void);
```

---

#### `SituationGetLastFramePhases`
_See `sit/situation_api.h` for the authoritative declaration._
```c
void SituationGetLastFramePhases(uint64_t* backpressure_ns, uint64_t* fence_wait_ns, uint64_t* execute_ns, uint64_t* present_ns);
```

---

#### `SituationGetMaxFrameTime`
Highest observed frame delta (general spike debugging)
```c
double SituationGetMaxFrameTime(void);
```
