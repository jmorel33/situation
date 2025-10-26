# Situation API Programming Guide

This document provides a comprehensive guide to the "Situation" library API, organized by functional modules. Each section offers an overview of the module's purpose and a detailed reference for all its functions.

## Table of Contents

- [Grand Concepts](#grand-concepts)
- [Getting Started](#getting-started)
- [API Usage Guide](#api-usage-guide)
- [Detailed API Reference](#detailed-api-reference)
  - [Core Module](#core-module)
  - [Window and Display Module](#window-and-display-module)
  - [Image Module](#image-module)
  - [Graphics Module](#graphics-module)
  - [Input Module](#input-module)
  - [Audio Module](#audio-module)
  - [Filesystem Module](#filesystem-module)
  - [Miscellaneous Module](#miscellaneous-module)

---

<details>
<summary><h2>Grand Concepts</h2></summary>

The "Situation" library is built on a few core principles that inform its design and API. Understanding these concepts will help you use the library effectively and avoid common pitfalls.

### 1. Immediate Mode API Philosophy
The library favors a mostly "immediate mode" style API. This means that, for many operations, you call a function and it takes effect immediately within the current frame. For example, `SituationCmdDrawQuad()` directly records a draw command into the current frame's command buffer. This approach is designed to be simple, intuitive, and easy to debug. It contrasts with "retained mode" systems where you would create a scene graph of objects that is then rendered by the engine.

### 2. Explicit Resource Management
"Situation" puts you in full control of resource lifecycles. Any resource you create (a texture, a mesh, a sound) must be explicitly destroyed by you.
- **Creation:** `SituationCreate...()`, `SituationLoad...()`
- **Destruction:** `SituationDestroy...()`, `SituationUnload...()`

This design choice avoids the complexities and performance overhead of automatic garbage collection. The library will warn you at shutdown if you've leaked any GPU resources, helping you catch memory management bugs early.

### 3. C-Style, Data-Oriented API
The API is pure C, promoting portability and interoperability. It uses handles (structs passed by value) to represent opaque resources and pointers for functions that need to modify or destroy those resources. This approach is data-oriented, focusing on transforming data (e.g., vertex data into a mesh, image data into a texture) rather than on object-oriented hierarchies.

### 4. Single-File, Header-Only Philosophy
"Situation" is distributed as a single header file (`situation.h`), which makes it incredibly easy to integrate into your projects. To use it, you simply `#include "situation.h"` in your source files. In exactly one C or C++ file, you must first define `SITUATION_IMPLEMENTATION` before the include to create the implementation.

```c
// In one C/C++ file
#define SITUATION_IMPLEMENTATION
#include "situation.h"
```

### 5. Backend Abstraction
The library provides a unified API that works over different graphics backends (OpenGL and Vulkan). You choose the backend at compile time by defining either `SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`. This allows you to write your application code once and have it run on a wide range of hardware and platforms, from high-end desktops to older, legacy systems.

</details>

<details>
<summary><h2>Getting Started</h2></summary>

Here is a minimal, complete example of a "Situation" application that opens a window, clears it to a blue color, and runs until the user closes it.

### Step 1: Include the Library
First, make sure `situation.h` is in your project's include path. In your main C file, define `SITUATION_IMPLEMENTATION` and include the header.

```c
#define SITUATION_IMPLEMENTATION
// Define a graphics backend before including the library
#define SITUATION_USE_OPENGL // or SITUATION_USE_VULKAN
#include "situation.h"

#include <stdio.h> // For printf
```

### Step 2: Initialize the Library
In your `main` function, you need to initialize the library. Create a `SituationInitInfo` struct to configure your application's startup properties, such as the window title and initial dimensions. Then, call `SituationInit()`.

```c
int main(int argc, char** argv) {
    SituationInitInfo init_info = {
        .app_name = "My First Situation App",
        .app_version = "1.0",
        .initial_width = 1280,
        .initial_height = 720,
        .window_flags = SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_VSYNC_HINT,
        .target_fps = 60,
        .headless = false
    };

    if (SituationInit(argc, argv, &init_info) != SIT_SUCCESS) {
        printf("Failed to initialize Situation: %s\n", SituationGetLastErrorMsg());
        return -1;
    }
```

### Step 3: The Main Loop
The heart of your application is the main loop. This loop continues as long as the user has not tried to close the window (`!SituationWindowShouldClose()`). Inside the loop, you follow a strict three-phase structure: Input, Update, and Render.

```c
    while (!SituationWindowShouldClose()) {
        // --- 1. Input ---
        SituationPollInputEvents();

        // --- 2. Update ---
        SituationUpdateTimers();
        // Your application logic, physics, etc. would go here.
        // For this example, we'll just check for the ESC key to close.
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break; // Exit the loop
        }

        // --- 3. Render ---
        if (SituationAcquireFrameCommandBuffer()) {
            SituationRenderPassInfo pass_info = {
                .color_load_action = SIT_LOAD_ACTION_CLEAR,
                .clear_color = { .r = 0, .g = 12, .b = 24, .a = 255 }, // A dark blue
                .color_store_action = SIT_STORE_ACTION_STORE,
            };

            // Begin the render pass for the main window
            SituationCmdBeginRenderPass(SituationGetMainCommandBuffer(), &pass_info);

            // You would record all your drawing commands here
            // e.g., SituationCmdDrawMesh(...)

            // End the render pass
            SituationCmdEndRenderPass(SituationGetMainCommandBuffer());

            // Submit the frame to be presented
            SituationEndFrame();
        }
    }
```

### Step 4: Shutdown
After the main loop finishes, it is critical to call `SituationShutdown()` to clean up all resources, close the window, and terminate all subsystems gracefully.

```c
    SituationShutdown();
    return 0;
}
```

### Full Example Code

```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "situation.h"

#include <stdio.h>

int main(int argc, char** argv) {
    // 1. Configure and Initialize
    SituationInitInfo init_info = {
        .app_name = "My First Situation App",
        .app_version = "1.0",
        .initial_width = 1280,
        .initial_height = 720,
        .window_flags = SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_VSYNC_HINT,
        .target_fps = 60,
        .headless = false
    };

    if (SituationInit(argc, argv, &init_info) != SIT_SUCCESS) {
        printf("Failed to initialize Situation: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // 2. Main Loop
    while (!SituationWindowShouldClose()) {
        // --- Input Phase ---
        SituationPollInputEvents();

        // --- Update Phase ---
        SituationUpdateTimers();
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
            break;
        }

        // --- Render Phase ---
        if (SituationAcquireFrameCommandBuffer()) {
            SituationRenderPassInfo pass_info = {
                .color_load_action = SIT_LOAD_ACTION_CLEAR,
                .clear_color = { .r = 0, .g = 12, .b = 24, .a = 255 },
                .color_store_action = SIT_STORE_ACTION_STORE,
            };

            SituationCmdBeginRenderPass(SituationGetMainCommandBuffer(), &pass_info);
            // Drawing commands go here
            SituationCmdEndRenderPass(SituationGetMainCommandBuffer());

            SituationEndFrame();
        }
    }

    // 3. Shutdown
    SituationShutdown();
    return 0;
}
```

</details>

---

<details>
<summary><h2>API Usage Guide</h2></summary>

**1. Lifecycle**
The library follows a strict lifecycle:
- Call `SituationInit()` once at the start of your application.
- Enter a main loop that runs until `SituationWindowShouldClose()` returns true.
- Call `SituationShutdown()` once before your application exits.
All API functions (except where noted) must be called between `SituationInit()` and `SituationShutdown()`.

**2. Main Loop Structure**
A correct "Situation" main loop has three distinct phases per frame:
1.  **Input:** Call `SituationPollInputEvents()` to gather all OS events.
2.  **Update:** Call `SituationUpdateTimers()` to calculate delta time, then run your application logic.
3.  **Render:** Call `SituationAcquireFrameCommandBuffer()`, record all your drawing commands using `SituationCmd*` functions, and finish with `SituationEndFrame()`.

**3. Resource Management (CRITICAL)**
The library uses explicit, manual resource management. This is a core design principle.
- Any resource created with a `SituationCreate*` or `SituationLoad*` function (e.g., `SituationCreateMesh`, `SituationLoadTexture`) **MUST** be explicitly freed with its corresponding `SituationDestroy*` or `SituationUnload*` function.
- Any function that returns a `char*` (e.g., `SituationGetLastErrorMsg`, `SituationGetBasePath`) returns a new block of memory. The **caller is responsible for freeing this memory** using `free()`.
- Failure to follow these rules will result in GPU and CPU memory leaks. The library will print warnings for leaked GPU resources at shutdown.

**4. Handles vs. Pointers**
The API uses two patterns for interacting with objects:
- **Handles (by value):** Opaque structs like `SituationMesh` or `SituationShader` are typically passed by value to drawing or binding functions (e.g., `SituationCmdDrawMesh(my_mesh)`).
- **Pointers (for modification):** When a function needs to modify or destroy a resource, you must pass a pointer to its handle (e.g., `SituationDestroyMesh(&my_mesh)`). This allows the function to invalidate the handle by setting its internal ID to 0.

**5. Thread Safety**
The library is **strictly single-threaded**. All `SITAPI` functions must be called from the same thread that called `SituationInit()`. Asynchronous operations (like asset loading) must be handled by the user, ensuring that no `SITAPI` calls are made from worker threads.

</details>

---
*   [Core Module: Application Lifecycle and System](#core-module)
*   [Window and Display Module](#window-and-display-module)
*   [Image Module: CPU-side Image and Font Loading and Manipulation](#image-module)
*   [Graphics Module: Rendering, Shaders, and GPU Resources](#graphics-module)
*   [Input Module: Keyboard, Mouse, and Gamepad](#input-module)
*   [Audio Module](#audio-module)
*   [Filesystem Module](#filesystem-module)
*   [Miscellaneous Module](#miscellaneous-module)

---

## Detailed API Reference

This section provides a complete list of all functions available in the "Situation" library, organized by module.

<details>
<summary>Core Module</summary>

**Overview:** The Core module is the heart of the "Situation" library, providing the essential functions for application lifecycle management. It handles initialization (`SituationInit`) and shutdown (`SituationShutdown`), processes the main event loop, and manages frame timing and rate control. This module also serves as a gateway to crucial system information, offering functions to query hardware details like CPU and GPU specifications, manage command-line arguments, and set up critical application-wide callbacks for events like window resizing or exit requests. Mastering the Core module is the first step to building any application with the library.

### Core Structs

#### `SituationInitInfo`
This struct is passed to `SituationInit()` to configure the application at startup.

```c
typedef struct SituationInitInfo {
    const char* app_name;
    const char* app_version;
    int initial_width;
    int initial_height;
    uint32_t window_flags;
    int target_fps;
    int oscillator_count;
    const double* oscillator_periods;
    bool headless;
} SituationInitInfo;
```
-   `app_name`: The name of your application, used for window titles and save paths.
-   `app_version`: The version of your application.
-   `initial_width`, `initial_height`: The desired dimensions for the main window when it is first created.
-   `window_flags`: A bitmask of `SituationWindowStateFlags` to set the initial state of the window (e.g., `SITUATION_FLAG_WINDOW_RESIZABLE`).
-   `target_fps`: The desired target frame rate. The library will sleep to avoid exceeding this. Use `0` for uncapped FPS.
-   `oscillator_count`: The number of temporal oscillators to create for rhythmic timing.
-   `oscillator_periods`: An array of `double`s specifying the initial period (in seconds) for each oscillator.
-   `headless`: If `true`, the library will initialize without creating a window or graphics context. Useful for server-side applications or command-line tools.

#### `SituationDeviceInfo`
This struct, returned by `SituationGetDeviceInfo()`, provides a snapshot of the host system's hardware.

```c
typedef struct SituationDeviceInfo {
    char cpu_brand[49];
    int cpu_core_count;
    int cpu_thread_count;
    uint64_t system_ram_bytes;
    char gpu_brand[128];
    uint64_t gpu_vram_bytes;
    int display_count;
    char os_name[32];
    char os_version[32];
    uint64_t total_storage_bytes;
    uint64_t free_storage_bytes;
} SituationDeviceInfo;
```
-   `cpu_brand`: The full brand string of the CPU (e.g., "Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz").
-   `cpu_core_count`: The number of physical CPU cores.
-   `cpu_thread_count`: The number of logical CPU threads.
-   `system_ram_bytes`: The total amount of physical system RAM in bytes.
-   `gpu_brand`: The brand string of the primary GPU.
-   `gpu_vram_bytes`: The total amount of dedicated video memory (VRAM) in bytes.
-   `display_count`: The number of connected displays (monitors).
-   `os_name`: The name of the operating system (e.g., "Windows").
-   `os_version`: The version of the operating system (e.g., "10.0.19042").
-   `total_storage_bytes`, `free_storage_bytes`: The total and free space on the logical drive where the executable is located.

### Functions

#### Application Lifecycle & State

*   `SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info)`
    *   Initializes the library, creates the main window, and sets up the graphics context. This must be the first function called.
*   `void SituationPollInputEvents(void)`
    *   Polls for all pending input and window events from the operating system. Call this once at the beginning of every frame.
*   `void SituationUpdateTimers(void)`
    *   Updates all internal timers, including the main frame timer (`deltaTime`) and the Temporal Oscillator System. Call this once per frame, after polling events.
*   `void SituationShutdown(void)`
    *   Shuts down all library subsystems, releases all resources, and closes the application window. This should be the last function called.
*   `bool SituationIsInitialized(void)`
    *   Checks if the library has been successfully initialized.
*   `bool SituationWindowShouldClose(void)`
    *   Returns `true` if the user has attempted to close the window (e.g., by clicking the 'X' button or pressing Alt+F4).
*   `void SituationPauseApp(void)`
    *   Pauses the library's internal, time-dependent subsystems, primarily the audio device.
*   `void SituationResumeApp(void)`
    *   Resumes the library's subsystems from a paused state.
*   `bool SituationIsAppPaused(void)`
    *   Checks if the application is currently in an internally-paused state.

#### Frame Timing & FPS Management

*   `void SituationSetTargetFPS(int fps)`
    *   Sets a target frame rate for the application. The main loop will sleep to avoid exceeding this rate. Pass `0` to uncap the frame rate.
*   `float SituationGetFrameTime(void)`
    *   Gets the time in seconds that the previous frame took to complete (also known as `deltaTime`). Essential for frame-rate-independent logic.
*   `int SituationGetFPS(void)`
    *   Gets the current frames-per-second, calculated periodically by the library.

#### Callbacks and Event Handling

*   `char* SituationGetLastErrorMsg(void)`
    *   Retrieves a copy of the last error message generated by the library. The caller is responsible for freeing this memory with `SituationFreeString()`.
*   `void SituationSetExitCallback(void (*callback)(void* user_data), void* user_data)`
    *   Registers a callback function to be executed just before the library shuts down.
*   `void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data)`
    *   Registers a callback function to be executed whenever the window's framebuffer is resized.
*   `void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data)`
    *   Registers a callback function to be executed when the window gains or loses input focus.
*   `void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data)`
    *   Registers a callback function to be executed when files are dragged and dropped onto the window.

#### Command-Line Argument Queries

*   `bool SituationIsArgumentPresent(const char* arg_name)`
    *   Checks if a specific command-line argument flag (e.g., `"-server"`) was provided when the application was launched.
*   `const char* SituationGetArgumentValue(const char* arg_name)`
    *   Gets the value of a command-line argument (e.g., gets `"jungle"` from `"-level:jungle"` or `"-level jungle"`).

#### System & Hardware Information

*   `SituationDeviceInfo SituationGetDeviceInfo(void)`
    *   Gathers and returns a comprehensive snapshot of the host system's hardware, including CPU, GPU, RAM, and storage.
*   `char* SituationGetUserDirectory(void)`
    *   Gets the full path to the current user's home or profile directory (e.g., `C:\Users\YourUser`). The caller must free the returned string.
*   `char SituationGetCurrentDriveLetter(void)`
    *   (Windows only) Gets the drive letter of the logical volume where the running executable is located.
*   `bool SituationGetDriveInfo(char drive_letter, uint64_t* out_total_capacity_bytes, uint64_t* out_free_space_bytes, char* out_volume_name, int volume_name_len)`
    *   (Windows only) Retrieves information about a specific logical drive, including its capacity, free space, and volume name.
*   `void SituationOpenFile(const char* filePath)`
    *   Asks the operating system to open a file, folder, or URL with its default application.
</details>
<details>
<summary>Window and Display Module</summary>

**Overview:** This module provides an exhaustive suite of tools for managing the application's window and querying the properties of physical display devices. It handles everything from basic window creation and state changes (fullscreen, minimized, borderless) to more advanced features like DPI scaling, opacity, and clipboard access. Furthermore, it allows you to enumerate all connected monitors, query their resolutions, refresh rates, and physical dimensions, and even change their display modes. This powerful combination of window and display control enables sophisticated, multi-monitor applications and fine-tuned user experiences.

### Window and Display Structs and Flags

#### `SituationDisplayInfo`
Returned by `SituationGetDisplays()`, this struct contains detailed information about a connected monitor.

```c
typedef struct SituationDisplayInfo {
    int id;
    char name[128];
    int current_mode;
    int mode_count;
    SituationDisplayMode* modes;
    vec2 position;
    vec2 physical_size;
} SituationDisplayInfo;
```
-   `id`: The internal ID of the monitor.
-   `name`: The human-readable name of the monitor.
-   `current_mode`: The index of the display's current mode in the `modes` array.
-   `mode_count`: The number of available display modes.
-   `modes`: A pointer to an array of `SituationDisplayMode` structs, detailing all supported resolutions and refresh rates.
-   `position`: The physical position of the monitor's top-left corner on the virtual desktop.
-   `physical_size`: The physical size of the display in millimeters.

#### `SituationDisplayMode`
Represents a single supported display mode (resolution, refresh rate, etc.) for a monitor.

```c
typedef struct SituationDisplayMode {
    int width;
    int height;
    int refresh_rate;
    int red_bits;
    int green_bits;
    int blue_bits;
} SituationDisplayMode;
```
-   `width`, `height`: The resolution of the display mode in pixels.
-   `refresh_rate`: The refresh rate in Hertz (Hz).
-   `red_bits`, `green_bits`, `blue_bits`: The color depth (bit depth) for each color channel.

#### `SituationWindowStateFlags`
These flags are used with `SituationSetWindowState()` and `SituationClearWindowState()` to control the window's appearance and behavior.

| Flag                                | Description                                                                                                   |
| ----------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| `SITUATION_FLAG_VSYNC_HINT`         | Suggests that the graphics backend should wait for vertical sync, reducing screen tearing.                      |
| `SITUATION_FLAG_FULLSCREEN_MODE`    | Enables exclusive fullscreen mode.                                                                            |
| `SITUATION_FLAG_WINDOW_RESIZABLE`   | Allows the user to resize the window.                                                                         |
| `SITUATION_FLAG_WINDOW_UNDECORATED` | Removes the window's border, title bar, and other decorations.                                                |
| `SITUATION_FLAG_WINDOW_HIDDEN`      | Hides the window from view.                                                                                   |
| `SITUATION_FLAG_WINDOW_MINIMIZED`   | Minimizes the window to the taskbar.                                                                          |
| `SITUATION_FLAG_WINDOW_MAXIMIZED`   | Maximizes the window to fill the work area.                                                                   |
| `SITUATION_FLAG_WINDOW_UNFOCUSED`   | Prevents the window from gaining focus when created.                                                          |
| `SITUATION_FLAG_WINDOW_TOPMOST`     | Keeps the window on top of all other windows.                                                                 |
| `SITUATION_FLAG_WINDOW_ALWAYS_RUN`  | Allows the application to continue running even when the window is minimized or out of focus.                 |
| `SITUATION_FLAG_WINDOW_TRANSPARENT` | Enables a transparent framebuffer, allowing for non-rectangular window shapes (requires OS compositor support). |
| `SITUATION_FLAG_HIGHDPI_HINT`       | Requests a high-DPI framebuffer on platforms that support it (e.g., macOS Retina displays).                 |
| `SITUATION_FLAG_MSAA_4X_HINT`       | Suggests that the graphics backend should use 4x multisample anti-aliasing.                                     |

### API Reference

#### Window State Management

*   `void SituationSetWindowState(uint32_t flags)`
    *   Sets one or more window state flags (e.g., `SITUATION_FLAG_WINDOW_TOPMOST`) for the current focus profile and applies the change.
*   `void SituationClearWindowState(uint32_t flags)`
    *   Clears one or more window state flags from the current focus profile and applies the change.
*   `void SituationToggleFullscreen(void)`
    *   Toggles the window between exclusive fullscreen and windowed mode.
*   `void SituationToggleBorderlessWindowed(void)`
    *   Toggles the window between a standard decorated style and a borderless, fullscreen-windowed style.
*   `void SituationMaximizeWindow(void)`
    *   Maximizes the application window to fill the available work area.
*   `void SituationMinimizeWindow(void)`
    *   Minimizes (iconifies) the application window.
*   `void SituationRestoreWindow(void)`
    *   Restores a minimized or maximized window to its normal, windowed state.
*   `void SituationSetWindowFocused(void)`
    *   Attempts to bring the application window to the foreground and give it input focus.

#### Window Property Management

*   `void SituationSetWindowTitle(const char *title)`
    *   Sets the text that appears in the window's title bar.
*   `void SituationSetWindowIcon(SituationImage image)`
    *   Sets a single image as the application's icon.
*   `void SituationSetWindowIcons(SituationImage *images, int count)`
    *   Sets multiple images of different sizes as the application's icon.
*   `void SituationSetWindowPosition(int x, int y)`
    *   Sets the position of the top-left corner of the window on the desktop.
*   `void SituationSetWindowSize(int width, int height)`
    *   Sets the dimensions of the window's client area (the drawable region).
*   `void SituationSetWindowMinSize(int width, int height)`
    *   Sets the minimum allowed size for the window's client area.
*   `void SituationSetWindowMaxSize(int width, int height)`
    *   Sets the maximum allowed size for the window's client area.
*   `void SituationSetWindowOpacity(float opacity)`
    *   Sets the opacity of the entire window, from `0.0` (transparent) to `1.0` (opaque).

#### Window State Queries

*   `bool SituationIsWindowState(uint32_t flag)`
    *   Checks if a specific window state (e.g., `SITUATION_FLAG_WINDOW_MAXIMIZED`) is currently active.
*   `bool SituationIsWindowFullscreen(void)`
    *   Checks if the window is currently in exclusive fullscreen mode.
*   `bool SituationIsWindowHidden(void)`
    *   Checks if the window is currently hidden (not visible).
*   `bool SituationIsWindowMinimized(void)`
    *   Checks if the window is currently minimized (iconified).
*   `bool SituationIsWindowMaximized(void)`
    *   Checks if the window is currently maximized.
*   `bool SituationHasWindowFocus(void)`
    *   Checks if the window currently has input focus.
*   `bool SituationIsWindowResized(void)`
    *   Checks if the window was resized during the last frame's event polling (a single-frame event flag).

#### Window & Screen Dimension Queries

*   `int SituationGetScreenWidth(void)`
    *   Gets the current width of the window in screen coordinates (logical size).
*   `int SituationGetScreenHeight(void)`
    *   Gets the current height of the window in screen coordinates (logical size).
*   `int SituationGetRenderWidth(void)`
    *   Gets the current width of the rendering framebuffer in pixels (HiDPI-aware).
*   `int SituationGetRenderHeight(void)`
    *   Gets the current height of the rendering framebuffer in pixels (HiDPI-aware).
*   `void SituationGetWindowSize(int* width, int* height)`
    *   Gets both the width and height of the window in screen coordinates in a single call.
*   `Vector2 SituationGetWindowPosition(void)`
    *   Gets the window's top-left position on the virtual desktop.
*   `Vector2 SituationGetWindowScaleDPI(void)`
    *   Gets the DPI scaling factor for the window (e.g., `(2.0, 2.0)` on a 200% scaled display).

#### Physical Display (Monitor) Management

*   `int SituationGetMonitorCount(void)`
    *   Gets the number of connected monitors.
*   `int SituationGetCurrentMonitor(void)`
    *   Gets the index of the monitor the window is currently on.
*   `SituationDisplayInfo* SituationGetDisplays(int* count)`
    *   Retrieves detailed information for all connected displays. The caller is responsible for freeing the returned array.
*   `void SituationRefreshDisplays(void)`
    *   Forces a refresh of the cached display information.
*   `SituationError SituationSetDisplayMode(int monitor_id, const SituationDisplayMode* mode, bool fullscreen)`
    *   Sets the display mode for a specific monitor.
*   `void SituationSetWindowMonitor(int monitor_id)`
    *   Sets the window to be fullscreen on a specific monitor.
*   `const char* SituationGetMonitorName(int monitor_id)`
    *   Gets the human-readable name of a monitor (e.g., "Generic PnP Monitor").
*   `int SituationGetMonitorWidth(int monitor_id)`
    *   Gets the width of the monitor's current video mode in pixels.
*   `int SituationGetMonitorHeight(int monitor_id)`
    *   Gets the height of the monitor's current video mode in pixels.
*   `int SituationGetMonitorPhysicalWidth(int monitor_id)`
    *   Gets the physical width of the monitor in millimeters.
*   `int SituationGetMonitorPhysicalHeight(int monitor_id)`
    *   Gets the physical height of the monitor in millimeters.
*   `int SituationGetMonitorRefreshRate(int monitor_id)`
    *   Gets the current refresh rate of a monitor in Hz.
*   `Vector2 SituationGetMonitorPosition(int monitor_id)`
    *   Gets the position of the specified monitor on the desktop.

#### Cursor, Clipboard and File Drops

*   `void SituationSetCursor(SituationCursor cursor)`
    *   Sets the appearance of the mouse cursor to a standard system shape (e.g., arrow, hand, I-beam).
*   `void SituationShowCursor(void)`
    *   Makes the mouse cursor visible and behave normally.
*   `void SituationHideCursor(void)`
    *   Makes the mouse cursor invisible while it is over the window.
*   `void SituationDisableCursor(void)`
    *   Hides and locks the cursor to the window, providing unbounded movement for 3D camera controls.
*   `const char* SituationGetClipboardText(void)`
    *   Retrieves text from the system clipboard. The memory is managed by the library.
*   `void SituationSetClipboardText(const char* text)`
    *   Sets the system clipboard to the specified text.
*   `bool SituationIsFileDropped(void)`
    *   Checks if a file was dropped into the window this frame.
*   `char** SituationLoadDroppedFiles(int* count)`
    *   Get the paths of dropped files. The caller is responsible for freeing this memory with `SituationUnloadDroppedFiles()`.
*   `void SituationUnloadDroppedFiles(char** paths, int count)`
    *   Frees the memory for the file path list returned by `SituationLoadDroppedFiles`.

#### Advanced Window Profile Management

*   `SituationError SituationSetWindowStateProfiles(uint32_t active_flags, uint32_t inactive_flags)`
    *   Sets profiles for window behavior when it is focused (active) versus unfocused (inactive).
*   `SituationError SituationApplyCurrentProfileWindowState(void)`
    *   Manually applies the appropriate window state profile based on the current focus.
*   `SituationError SituationToggleWindowStateFlags(SituationWindowStateFlags flags_to_toggle)`
    *   Toggles one or more flags in the current profile and applies the result.
*   `uint32_t SituationGetCurrentActualWindowStateFlags(void)`
    *   Gets a bitmask representing the window's actual, current state from the OS.
</details>
<details>
<summary>Image Module</summary>

**Overview:** The Image module is a comprehensive, CPU-side toolkit for all forms of image and font manipulation. It allows you to load images from various formats, generate new images programmatically (e.g., with solid colors or gradients), and perform a wide range of transformations like resizing, cropping, flipping, and color adjustment (HSV). Crucially, this module also provides a powerful text rendering engine, enabling you to load TTF/OTF fonts and draw styled text directly onto your images. The `SituationImage` and `SituationFont` objects produced by this module are the primary source for creating GPU-side textures and font atlases used by the Graphics module.

### Image Structs

#### `SituationImage`
A handle representing a CPU-side image. All pixel data is stored in uncompressed 32-bit RGBA format.

```c
typedef struct SituationImage {
    void *data;
    int width;
    int height;
} SituationImage;
```
-   `data`: A pointer to the raw pixel data.
-   `width`, `height`: The dimensions of the image in pixels.

#### `SituationFont`
A handle representing a CPU-side font, loaded from a TTF or OTF file. This is used for rendering text onto `SituationImage` objects.

```c
typedef struct SituationFont {
    void *fontData;
    void *stbFontInfo;
} SituationFont;
```
-   `fontData`: A pointer to the raw data of the font file.
-   `stbFontInfo`: A pointer to the internal `stbtt_fontinfo` struct used by the font rendering backend.

### API Reference

#### Image Loading and Unloading
*   `SituationImage SituationLoadImage(const char *fileName)`
    *   Loads an image from a file into CPU memory (RAM).
*   `SituationImage SituationLoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize)`
    *   Loads an image from a memory buffer.
*   `void SituationUnloadImage(SituationImage image)`
    *   Unloads an image's pixel data from memory.
*   `bool SituationIsImageValid(SituationImage image)`
    *   Checks if an image has been loaded successfully and has valid data.

#### Image Exporting
*   `bool SituationExportImage(SituationImage image, const char *fileName)`
    *   Exports image data to a file (PNG or BMP supported).

#### Image Generation & Copying
*   `SituationImage SituationImageCopy(SituationImage image)`
    *   Creates a new image by making a deep copy of another.
*   `void SituationImageDraw(SituationImage *dst, SituationImage src, Rectangle srcRect, Vector2 dstPos)`
    *   Draws a portion of a source image onto a destination image (opaque blit).
*   `void SituationImageDrawAlpha(SituationImage *dst, SituationImage src, Rectangle srcRect, Vector2 dstPos, ColorRGBA tint)`
    *   Draws a portion of a source image onto a destination with alpha blending and tinting.
*   `SituationImage SituationGenImageColor(int width, int height, ColorRGBA color)`
    *   Generates a new image filled with a single, solid color.
*   `SituationImage SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br)`
    *   Generates a new image with a 4-corner color gradient.

#### Image Manipulation
*   `void SituationImageCrop(SituationImage *image, Rectangle crop)`
    *   Crops an image in-place to a specific rectangle.
*   `void SituationImageResize(SituationImage *image, int newWidth, int newHeight)`
    *   Resizes an image in-place using sRGB-correct scaling.
*   `void SituationImageFlip(SituationImage *image, SituationImageFlipMode mode)`
    *   Flips an image in-place either vertically, horizontally, or both.
*   `void SituationImageAdjustHSV(SituationImage *image, float hue_shift, float sat_factor, float val_factor, float mix)`
    *   Adjusts the Hue, Saturation, and Value (Brightness) of an image.

#### Font Management
*   `SituationFont SituationLoadFont(const char *fileName)`
    *   Loads a font from a TTF/OTF file for CPU-side rendering.
*   `void SituationUnloadFont(SituationFont font)`
    *   Unloads a CPU-side font and frees its memory.
*   `Rectangle SituationMeasureText(SituationFont font, const char *text, float fontSize)`
    *   Calculates the bounding box of a text string without rendering it.
*   `void SituationImageDrawCodepoint(SituationImage *dst, SituationFont font, int codepoint, Vector2 position, float fontSize, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness)`
    *   Draws a single character with advanced styling (rotation, skew, outline).
*   `void SituationImageDrawText(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, ColorRGBA tint )`
    *   Draws a simple, tinted text string onto an image.
*   `void SituationImageDrawTextEx(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness)`
    *   Draws a text string with advanced styling and transformations.
</details>
<details>
<summary>Graphics Module</summary>

**Overview:** The Graphics module forms the core of the rendering pipeline, offering a powerful, backend-agnostic API for interacting with the GPU. It abstracts the complexities of OpenGL and Vulkan into a single, cohesive set of commands. This module is responsible for all GPU resource management, including the creation and destruction of meshes, shaders, textures, and data buffers. Its command-buffer-centric design (`SituationCmd...`) allows you to precisely record and sequence drawing operations, manage rendering passes, and dispatch compute shaders. It also features a "Virtual Display" system, a powerful tool for creating and compositing off-screen render targets, enabling sophisticated post-processing effects and UI layering.

### Graphics Structs and Enums

#### `SituationRenderPassInfo`
Configures a rendering pass. Used with `SituationCmdBeginRenderPass()`.

```c
typedef struct SituationRenderPassInfo {
    SituationLoadAction color_load_action;
    SituationStoreAction color_store_action;
    ColorRGBA clear_color;
    SituationLoadAction depth_load_action;
    SituationStoreAction depth_store_action;
    float clear_depth;
    int virtual_display_id;
} SituationRenderPassInfo;
```
-   `color_load_action`, `depth_load_action`: What to do with the color/depth buffer at the start of the pass (`SIT_LOAD_ACTION_LOAD`, `_CLEAR`, or `_DONT_CARE`).
-   `color_store_action`, `depth_store_action`: What to do with the buffer at the end of the pass (`SIT_STORE_ACTION_STORE` or `_DONT_CARE`).
-   `clear_color`, `clear_depth`: The values to use if the load action is `_CLEAR`.
-   `virtual_display_id`: The ID of a virtual display to render to. Use `-1` to target the main window.

#### `SituationMesh`, `SituationShader`, `SituationTexture`, `SituationBuffer`, `SituationModel`
These are opaque handles to GPU resources. Their internal structure is not exposed to the user.

#### `SituationBufferUsageFlags`
Specifies how a `SituationBuffer` will be used. This helps the driver place the buffer in the most optimal memory.

| Flag                          | Description                                                                 |
| ----------------------------- | --------------------------------------------------------------------------- |
| `SIT_BUFFER_USAGE_VERTEX`     | The buffer will be used as a vertex buffer.                                 |
| `SIT_BUFFER_USAGE_INDEX`      | The buffer will be used as an index buffer.                                 |
| `SIT_BUFFER_USAGE_UNIFORM`    | The buffer will be used as a Uniform Buffer Object (UBO).                   |
| `SIT_BUFFER_USAGE_STORAGE`    | The buffer will be used as a Shader Storage Buffer Object (SSBO).           |
| `SIT_BUFFER_USAGE_INDIRECT`   | The buffer will be used for indirect drawing commands.                      |
| `SIT_BUFFER_USAGE_TRANSFER_SRC`| The buffer can be used as a source for a copy operation.                  |
| `SIT_BUFFER_USAGE_TRANSFER_DST`| The buffer can be used as a destination for a copy operation.             |

#### `SituationComputeLayoutType`
Defines the descriptor set layout for a compute pipeline.

| Type                      | Description                                                                                               |
| ------------------------- | --------------------------------------------------------------------------------------------------------- |
| `SIT_COMPUTE_LAYOUT_EMPTY`| The compute shader does not use any resources.                                                            |
| `SIT_COMPUTE_LAYOUT_IMAGE`| The pipeline expects a single storage image to be bound at binding 0.                                       |
| `SIT_COMPUTE_LAYOUT_BUFFER`| The pipeline expects a single storage buffer to be bound at binding 0.                                    |
| `SIT_COMPUTE_LAYOUT_BUFFER_X2`| The pipeline expects two storage buffers to be bound at bindings 0 and 1.                                 |

### API Reference

#### Frame Lifecycle & Command Buffer
*   `bool SituationAcquireFrameCommandBuffer(void)`
    *   Prepares the backend for a new frame of rendering, acquiring the next available render target. Must be called before any drawing commands.
*   `SituationCommandBuffer SituationGetMainCommandBuffer(void)`
    *   Gets the primary command buffer for the current frame (Vulkan only; returns `NULL` on OpenGL).
*   `SituationError SituationEndFrame(void)`
    *   Submits all recorded commands for the frame and presents the result to the screen.

#### Abstracted Rendering Commands
*   `SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info)`
    *   Begins a render pass on a target with detailed configuration for attachments (color/depth), load/store operations, and clear values.
*   `void SituationCmdEndRenderPass(SituationCommandBuffer cmd)`
    *   Ends the current render pass.
*   `void SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height)`
    *   Sets the dynamic viewport and scissor for the current render pass.
*   `void SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height)`
    *   Sets the dynamic scissor rectangle to clip rendering.
*   `SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader)`
    *   Binds a graphics pipeline (shader program) for subsequent draws.
*   `SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh)`
    *   Records a command to draw a complete, pre-configured mesh.
*   `void SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, vec4 color)`
    *   Records a command to draw a simple, colored, and transformed 2D quad.
*   `void SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id, const void* data, size_t size)`
    *   Sets a small block of per-draw uniform data (push constant).
*   `SituationError SituationCmdBindDescriptorSet(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer)`
    *   Binds a buffer's pre-packaged descriptor set (UBO/SSBO) to a set index.
*   `SituationError SituationCmdBindTextureSet(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture)`
    *   Binds a texture's pre-packaged descriptor set to a set index.
*   `SituationError SituationCmdBindComputeTexture(SituationCommandBuffer cmd, uint32_t binding, SituationTexture texture)`
    *   Binds a texture as a storage image for compute shaders.
*   `void SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, int size, SituationDataType type, bool normalized, size_t offset)`
    *   Defines the format of a vertex attribute for the active vertex buffer.
*   `void SituationCmdDraw(SituationCommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)`
    *   Records a non-indexed draw call.
*   `void SituationCmdDrawIndexed(SituationCommandBuffer cmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)`
    *   Records an indexed draw call.

#### Graphics Resource Management
*   `SituationMesh SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count)`
    *   Creates a self-contained GPU mesh from vertex and index data.
*   `void SituationDestroyMesh(SituationMesh* mesh)`
    *   Unloads a mesh from GPU memory.

#### Shader Management
*   `SituationShader SituationLoadShader(const char* vs_path, const char* fs_path)`
    *   Loads, compiles, and links a graphics shader pipeline from vertex and fragment shader files.
*   `SituationShader SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code)`
    *   Creates a graphics shader pipeline from in-memory GLSL source code.
*   `void SituationUnloadShader(SituationShader* shader)`
    *   Unloads a graphics shader pipeline and frees its GPU resources.

#### Shader Interaction & Synchronization
*   `SituationError SituationSetShaderUniform(SituationShader shader, const char* uniform_name, const void* data, SituationUniformType type)`
    *   (OpenGL only) Sets a standalone uniform value by name, using an internal cache for performance.
*   `void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags)`
    *   Inserts a fine-grained pipeline memory barrier for synchronization between pipeline stages.

#### Texture Management
*   `SituationTexture SituationCreateTexture(SituationImage image, bool generate_mipmaps)`
    *   Creates a GPU texture from a CPU-side `SituationImage`.
*   `void SituationDestroyTexture(SituationTexture* texture)`
    *   Unloads a texture from GPU memory.

#### Compute Shader Pipeline
*   `SituationComputePipeline SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type)`
    *   Creates a compute pipeline from a GLSL shader file.
*   `SituationComputePipeline SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type)`
    *   Creates a compute pipeline from in-memory GLSL source code.
*   `void SituationDestroyComputePipeline(SituationComputePipeline* pipeline)`
    *   Destroys a compute pipeline and frees its GPU resources.
*   `void SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline)`
    *   Binds a compute pipeline for a subsequent dispatch.
*   `void SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)`
    *   Records a command to execute a compute shader.

#### GPU Buffer Management
*   `SituationBuffer SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags)`
    *   Creates a general-purpose GPU data buffer (e.g., for vertices, indices, UBOs, or SSBOs).
*   `void SituationDestroyBuffer(SituationBuffer* buffer)`
    *   Destroys a GPU buffer and frees its memory.
*   `SituationError SituationUpdateBuffer(SituationBuffer buffer, size_t offset, size_t size, const void* data)`
    *   Updates a region of data within an existing GPU buffer.
*   `SituationError SituationGetBufferData(SituationBuffer buffer, size_t offset, size_t size, void* out_data)`
    *   Reads data back from a GPU buffer to host (CPU) memory.

#### Virtual Displays (Render Targets)
*   `int SituationCreateVirtualDisplay(vec2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode)`
    *   Creates an off-screen render target (framebuffer object).
*   `SituationError SituationDestroyVirtualDisplay(int display_id)`
    *   Destroys a virtual display and its associated resources.
*   `void SituationRenderVirtualDisplays(SituationCommandBuffer cmd)`
    *   Composites all visible virtual displays onto the current render target.
*   `SituationError SituationConfigureVirtualDisplay(int display_id, vec2 offset, float opacity, int z_order, bool visible, double frame_time_mult, SituationBlendMode blend_mode)`
    *   Configures a virtual display's properties for compositing (position, opacity, etc.).
*   `SituationVirtualDisplay* SituationGetVirtualDisplay(int display_id)`
    *   Retrieves a pointer to the internal state structure of a virtual display.
*   `SituationError SituationSetVirtualDisplayScalingMode(int display_id, SituationScalingMode scaling_mode)`
    *   Sets the scaling and filtering mode for a virtual display.
*   `void SituationSetVirtualDisplayDirty(int display_id, bool is_dirty)`
    *   Manually marks a virtual display as needing to be re-rendered.
*   `bool SituationIsVirtualDisplayDirty(int display_id)`
    *   Checks if a virtual display is marked as dirty.
*   `double SituationGetLastVDCompositeTimeMS(void)`
    *   Gets the time taken (in milliseconds) for the last virtual display composite pass.
*   `void SituationGetVirtualDisplaySize(int display_id, int* width, int* height)`
    *   Gets the internal resolution of a virtual display.

#### 3D Model Utilities
*   `SituationModel SituationLoadModel(const char* file_path)`
    *   Loads a complete 3D model and its materials/textures from a GLTF file.
*   `void SituationUnloadModel(SituationModel* model)`
    *   Frees all GPU and CPU resources associated with a loaded model.
*   `void SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform)`
    *   Draws all sub-meshes of a model with a single root transformation, binding materials automatically.
*   `bool SituationSaveModelAsGltf(SituationModel model, const char* file_path)`
    *   Exports a model to a human-readable .gltf and a .bin file.

#### Image & Screenshot Utilities
*   `SituationImage SituationLoadImageFromScreen(void)`
    *   Captures the current contents of the main window's backbuffer into a CPU-side image.
*   `bool SituationTakeScreenshot(const char *fileName)`
    *   Takes a screenshot and saves it to a PNG or BMP file.

#### Backend-Specific Accessors
*   `SituationRendererType SituationGetRendererType(void)`
    *   Gets the graphics backend renderer type that the library was compiled with (`SIT_RENDERER_OPENGL` or `SIT_RENDERER_VULKAN`).
*   `GLFWwindow* SituationGetGLFWwindow(void)`
    *   Gets the raw, underlying GLFW window handle for advanced, direct use of the GLFW API.
*   `VkInstance SituationGetVulkanInstance(void)`
    *   (Vulkan only) Gets the raw Vulkan instance handle.
*   `VkDevice SituationGetVulkanDevice(void)`
    *   (Vulkan only) Gets the raw Vulkan logical device handle.
*   `VkPhysicalDevice SituationGetVulkanPhysicalDevice(void)`
    *   (Vulkan only) Gets the raw Vulkan physical device handle.
*   `VkRenderPass SituationGetMainWindowRenderPass(void)`
    *   (Vulkan only) Gets the render pass for the main window.
</details>
<details>
<summary>Input Module</summary>

**Overview:** The Input module provides a flexible and comprehensive interface for handling user input from various devices. It supports keyboard, mouse, and gamepads, offering two distinct interaction models: state polling and event-driven callbacks. You can use polling functions (e.g., `SituationIsKeyDown()`) to check the current state of a button or axis within your main update loop, which is ideal for continuous actions like player movement. Alternatively, you can register callback functions (e.g., `SituationSetKeyCallback()`) to be notified of input events the moment they occur, which is perfect for handling discrete events like UI clicks or weapon firing. This dual approach allows you to choose the best input handling strategy for each part of your application.

### Input Callbacks

The input module allows you to register callback functions to be notified of input events as they happen, as an alternative to polling for state each frame.

#### `SituationKeyCallback`
`typedef void (*SituationKeyCallback)(int key, int scancode, int action, int mods, void* user_data);`
-   `key`: The keyboard key that was pressed or released (e.g., `SIT_KEY_A`).
-   `scancode`: The system-specific scancode of the key.
-   `action`: The key action (`SIT_PRESS`, `SIT_RELEASE`, or `SIT_REPEAT`).
-   `mods`: A bitmask of modifier keys that were held down (`SIT_MOD_SHIFT`, `SIT_MOD_CONTROL`, etc.).
-   `user_data`: The custom user data pointer you provided when setting the callback.

#### `SituationMouseButtonCallback`
`typedef void (*SituationMouseButtonCallback)(int button, int action, int mods, void* user_data);`
-   `button`: The mouse button that was pressed or released (e.g., `SIT_MOUSE_BUTTON_LEFT`).
-   `action`: The button action (`SIT_PRESS` or `SIT_RELEASE`).
-   `mods`: A bitmask of modifier keys.
-   `user_data`: Custom user data.

#### `SituationCursorPosCallback`
`typedef void (*SituationCursorPosCallback)(double xpos, double ypos, void* user_data);`
-   `xpos`, `ypos`: The new cursor position in screen coordinates.
-   `user_data`: Custom user data.

#### `SituationScrollCallback`
`typedef void (*SituationScrollCallback)(double xoffset, double yoffset, void* user_data);`
-   `xoffset`, `yoffset`: The scroll offset.
-   `user_data`: Custom user data.

### API Reference

#### Keyboard Input
*   `bool SituationIsKeyDown(int key)`
    *   Checks if a key is currently held down (a continuous state).
*   `bool SituationIsKeyUp(int key)`
    *   Checks if a key is currently up (a continuous state).
*   `bool SituationIsKeyPressed(int key)`
    *   Checks if a key was pressed down this frame (a single-trigger event).
*   `bool SituationIsKeyReleased(int key)`
    *   Checks if a key was released this frame (a single-trigger event).
*   `int SituationGetKeyPressed(void)`
    *   Gets the next key from the press queue, consuming it. Returns 0 if empty.
*   `int SituationPeekKeyPressed(void)`
    *   Peeks at the next key in the press queue without consuming it.
*   `unsigned int SituationGetCharPressed(void)`
    *   Gets the next Unicode character from the text input queue.
*   `bool SituationIsLockKeyPressed(int lock_key_mod)`
    *   Checks if a lock key (Caps Lock or Num Lock) is currently active.
*   `bool SituationIsScrollLockOn(void)`
    *   Checks if Scroll Lock is currently toggled on.
*   `bool SituationIsModifierPressed(int modifier)`
    *   Checks if a modifier key (Shift, Ctrl, Alt) is pressed.
*   `void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data)`
    *   Sets a callback function for all keyboard key events.

#### Mouse Input
*   `vec2 SituationGetMousePosition(void)`
    *   Gets the mouse position within the window.
*   `vec2 SituationGetMouseDelta(void)`
    *   Gets the mouse movement since the last frame.
*   `float SituationGetMouseWheelMove(void)`
    *   Gets vertical mouse wheel movement.
*   `vec2 SituationGetMouseWheelMoveV(void)`
    *   Gets both vertical and horizontal mouse wheel movement.
*   `bool SituationIsMouseButtonDown(int button)`
    *   Checks if a mouse button is currently held down (a state).
*   `bool SituationIsMouseButtonPressed(int button)`
    *   Checks if a mouse button was pressed down this frame (an event).
*   `bool SituationIsMouseButtonReleased(int button)`
    *   Checks if a mouse button was released this frame (an event).
*   `void SituationSetMousePosition(vec2 pos)`
    *   Sets the mouse position within the window.
*   `void SituationSetMouseOffset(vec2 offset)`
    *   Sets a virtual offset for the mouse coordinate system.
*   `void SituationSetMouseScale(vec2 scale)`
    *   Sets a virtual scale for the mouse coordinate system.
*   `void SituationSetMouseButtonCallback(SituationMouseButtonCallback callback, void* user_data)`
    *   Sets a callback for mouse button events.
*   `void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data)`
    *   Sets a callback for mouse movement events.
*   `void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data)`
    *   Sets a callback for mouse scroll events.

#### Gamepad Input
*   `bool SituationIsJoystickPresent(int jid)`
    *   Checks if a joystick or gamepad is connected at a specific slot.
*   `bool SituationIsGamepad(int jid)`
    *   Checks if a connected joystick has a standard gamepad mapping.
*   `const char* SituationGetJoystickName(int jid)`
    *   Gets the human-readable name of a joystick or gamepad.
*   `void SituationSetJoystickCallback(SituationJoystickCallback callback, void* user_data)`
    *   Sets a callback for joystick connection and disconnection events.
*   `int SituationSetGamepadMappings(const char *mappings)`
    *   Loads a new set of gamepad mappings from an SDL2-compatible string.
*   `int SituationGetGamepadButtonPressed(void)`
    *   Gets the next gamepad button from the global press queue.
*   `bool SituationIsGamepadButtonDown(int jid, int button)`
    *   Checks if a gamepad button is currently held down (a state).
*   `bool SituationIsGamepadButtonPressed(int jid, int button)`
    *   Checks if a gamepad button was pressed down this frame (an event).
*   `bool SituationIsGamepadButtonReleased(int jid, int button)`
    *   Checks if a gamepad button was released this frame (an event).
*   `int SituationGetGamepadAxisCount(int jid)`
    *   Gets the number of axes for a gamepad.
*   `float SituationGetGamepadAxisValue(int jid, int axis)`
    *   Gets the value of a gamepad axis, with deadzone applied.
*   `void SituationSetGamepadVibration(int jid, float left_motor, float right_motor)`
    *   (Windows only) Sets gamepad vibration/rumble intensity.
</details>
<details>
<summary>Audio Module</summary>

**Overview:** The Audio module offers a full-featured audio engine capable of handling everything from simple sound playback to complex, real-time digital signal processing (DSP). It begins with robust device management, allowing you to enumerate and select audio output devices. The module supports loading and streaming common audio formats (WAV, MP3, OGG, FLAC) and provides fine-grained control over individual sounds, including volume, panning, and pitch shifting. Beyond basic playback, it includes a built-in effects chain with filters (low-pass, high-pass), echo, and reverb. For advanced users, the module allows you to attach custom callback-based processors to any sound, enabling the implementation of unique, real-time audio effects and analysis.

### Audio Structs and Enums

#### `SituationAudioDeviceInfo`
Contains information about a single audio playback device available on the system.

```c
typedef struct SituationAudioDeviceInfo {
    int internal_id;
    char name[SITUATION_MAX_DEVICE_NAME_LEN];
    bool is_default;
    int min_channels, max_channels;
    int min_sample_rate, max_sample_rate;
} SituationAudioDeviceInfo;
```
-   `internal_id`: The ID used to select this device with `SituationSetAudioDevice()`.
-   `name`: The human-readable name of the device.
-   `is_default`: `true` if this is the operating system's default audio device.
-   `min_channels`, `max_channels`: The supported range of audio channels.
-   `min_sample_rate`, `max_sample_rate`: The supported range of sample rates.

#### `SituationAudioFormat`
Describes the format of audio data.

```c
typedef struct SituationAudioFormat {
    int channels;
    int sample_rate;
    int bit_depth;
} SituationAudioFormat;
```
-   `channels`: The number of audio channels (e.g., 1 for mono, 2 for stereo).
-   `sample_rate`: The number of samples per second (e.g., 44100).
-   `bit_depth`: The number of bits per sample (e.g., 16).

#### `SituationSound`
An opaque handle to a loaded sound, either fully in memory or streamed.

#### `SituationFilterType`
Specifies the type of filter to apply to a sound.

| Type                      | Description                               |
| ------------------------- | ----------------------------------------- |
| `SIT_FILTER_NONE`         | No filter is applied.                     |
| `SIT_FILTER_LOW_PASS`     | Allows low frequencies to pass through.   |
| `SIT_FILTER_HIGH_PASS`    | Allows high frequencies to pass through.  |

### API Reference

#### Audio Device Management
*   `SituationAudioDeviceInfo* SituationGetAudioDevices(int* count)`
    *   Enumerates all available audio playback devices on the system.
*   `SituationError SituationSetAudioDevice(int internal_id, const SituationAudioFormat* format)`
    *   Switches the active audio output to a specific device.
*   `int SituationGetAudioPlaybackSampleRate(void)`
    *   Gets the sample rate of the currently active audio playback device.
*   `SituationError SituationSetAudioPlaybackSampleRate(int sample_rate)`
    *   Re-initializes the active audio device with a new sample rate.
*   `float SituationGetAudioMasterVolume(void)`
    *   Gets the current master volume of the audio device.
*   `SituationError SituationSetAudioMasterVolume(float volume)`
    *   Sets the master volume for the entire audio device.
*   `bool SituationIsAudioDevicePlaying(void)`
    *   Checks if the audio device is currently active and playing sound.
*   `SituationError SituationPauseAudioDevice(void)`
    *   Pauses all audio output by stopping the audio device.
*   `SituationError SituationResumeAudioDevice(void)`
    *   Resumes audio output by restarting a paused audio device.

#### Sound Loading and Management
*   `SituationError SituationLoadSoundFromFile(const char* file_path, bool looping, SituationSound* out_sound)`
    *   Loads and decodes an audio file (WAV, MP3, OGG, FLAC) into memory for playback.
*   `SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound)`
    *   Initializes a sound for playback from a custom, user-defined data stream.
*   `void SituationUnloadSound(SituationSound* sound)`
    *   Unloads a sound and frees all of its associated memory and resources.
*   `SituationError SituationPlayLoadedSound(SituationSound* sound)`
    *   Begins playback of a loaded sound, restarting it if already playing.
*   `SituationError SituationStopLoadedSound(SituationSound* sound)`
    *   Stops a specific sound from playing and removes it from the mixing queue.
*   `SituationError SituationStopAllLoadedSounds(void)`
    *   Stops all currently playing sounds and clears the mixing queue.

#### Sound Data Manipulation
*   `SituationError SituationSoundCopy(const SituationSound* source, SituationSound* out_destination)`
    *   Creates a new sound by making a deep copy of a source sound's decoded PCM data.
*   `SituationError SituationSoundCrop(SituationSound* sound, uint64_t initFrame, uint64_t finalFrame)`
    *   Modifies a sound in-place to contain only a specific range of its audio data.
*   `bool SituationSoundExportAsWav(const SituationSound* sound, const char* fileName)`
    *   Exports the raw PCM data of a sound to a new WAV file.

#### Sound Parameters and Effects
*   `SituationError SituationSetSoundVolume(SituationSound* sound, float volume)`
    *   Sets the volume for a specific, individual sound.
*   `float SituationGetSoundVolume(SituationSound* sound)`
    *   Gets the current volume of a specific sound.
*   `SituationError SituationSetSoundPan(SituationSound* sound, float pan)`
    *   Sets the stereo panning for a sound (`-1.0` left, `1.0` right).
*   `float SituationGetSoundPan(SituationSound* sound)`
    *   Gets the current stereo panning of a sound.
*   `SituationError SituationSetSoundPitch(SituationSound* sound, float pitch)`
    *   Sets the playback pitch for a sound by resampling (`1.0` is normal pitch).
*   `float SituationGetSoundPitch(SituationSound* sound)`
    *   Gets the current pitch multiplier of a sound.
*   `SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor)`
    *   Applies a low-pass or high-pass filter to a sound's effects chain.
*   `SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix)`
    *   Applies a simple echo (delay) effect to a sound.
*   `SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix)`
    *   Applies a reverb effect to a sound.

#### Custom Audio Processing
*   `SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* userData)`
    *   Attaches a custom DSP processor to a sound's effect chain for real-time processing.
*   `SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* userData)`
    *   Detaches a custom DSP processor from a sound.
</details>
<details>
<summary>Filesystem Module</summary>

**Overview:** The Filesystem module provides a robust, cross-platform, and UTF-8 aware API for interacting with the host's file system. It abstracts away OS-specific differences, offering a unified set of functions for common file and directory operations. This includes essentials like checking for file/directory existence, reading and writing entire files to/from memory, and directory traversal. The module also includes convenient path manipulation utilities for joining paths, extracting filenames, and locating special directories like the application's base path or a safe location for user data. These features simplify the process of asset loading, data saving, and other file-related tasks.

#### Path Management & Special Directories
*   `char* SituationGetAppSavePath(const char* app_name)`
    *   Gets a safe, persistent path for saving application data (e.g., `%APPDATA%/AppName`).
*   `char* SituationGetBasePath(void)`
    *   Gets the path to the directory containing the executable.
*   `char* SituationJoinPath(const char* base_path, const char* file_or_dir_name)`
    *   Joins two path components with the correct OS separator.
*   `const char* SituationGetFileName(const char* full_path)`
    *   Extracts the file name (including extension) from a full path.
*   `const char* SituationGetFileExtension(const char* file_path)`
    *   Extracts the file extension from a path.

#### File & Directory Queries
*   `bool SituationFileExists(const char* file_path)`
    *   Checks if a file exists at the given path.
*   `bool SituationDirectoryExists(const char* dir_path)`
    *   Checks if a directory exists at the given path.
*   `long SituationGetFileModTime(const char* file_path)`
    *   Gets the last modification time of a file as a Unix timestamp.

#### File Operations
*   `unsigned char* SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read)`
    *   Loads an entire file into a memory buffer.
*   `bool SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write)`
    *   Saves a block of memory to a file.
*   `char* SituationLoadFileText(const char* file_path)`
    *   Loads a text file into a null-terminated string.
*   `bool SituationSaveFileText(const char* file_path, const char* text)`
    *   Saves a null-terminated string to a text file.
*   `bool SituationCopyFile(const char* source_path, const char* dest_path)`
    *   Copies a file, overwriting the destination if it exists.
*   `bool SituationDeleteFile(const char* file_path)`
    *   Deletes a file from the file system.
*   `bool SituationMoveFile(const char* old_path, const char* new_path)`
    *   Renames or moves a file or directory, even across drives on Windows.
*   `bool SituationRenameFile(const char* old_path, const char* new_path)`
    *   An alias for `SituationMoveFile`.

#### Directory Operations
*   `bool SituationCreateDirectory(const char* dir_path, bool create_parents)`
    *   Creates a directory, optionally creating all parent directories in the path.
*   `bool SituationDeleteDirectory(const char* dir_path, bool recursive)`
    *   Deletes a directory. If `recursive` is true, it deletes all contents first.
*   `char** SituationListDirectoryFiles(const char* dir_path, int* out_count)`
    *   Lists files and subdirectories in a path.
*   `void SituationFreeDirectoryFileList(char** file_list, int count)`
    *   Frees the memory for the list returned by `SituationListDirectoryFiles`.
</details>
<details>
<summary>Miscellaneous Module</summary>

**Overview:** The Miscellaneous module is a collection of powerful utility systems that don't fit into the other main categories but provide significant value. Its flagship feature is the Temporal Oscillator System, a unique and powerful tool for creating rhythmic, periodic events and synchronizing application logic to a musical beat. This module also provides a suite of robust color space conversion functions, allowing for easy translation between RGBA, HSV, and the broadcast-safe YPQA color spaces. Finally, it includes essential memory management helpers for freeing strings and other data structures allocated by the library, ensuring proper resource cleanup.

### Miscellaneous Concepts & Structs

#### The Temporal Oscillator System
This is a high-level timing utility designed to create rhythmic, periodic events in your application. You create a number of oscillators during `SituationInit()`, each with a specific period (e.g., 0.5 seconds). The library then updates these timers every frame.

You can query an oscillator's state (`0` or `1`), which flips every time its period elapses. This is useful for creating blinking effects, triggering animations on a beat, or synchronizing game logic to a fixed time step.

-   `SituationTimerGetOscillatorState()`: Gets the current binary state.
-   `SituationTimerHasOscillatorUpdated()`: A single-frame trigger that returns `true` only on the frame the state flips.
-   `SituationTimerPingOscillator()`: A "metronome" function that returns `true` once per period.

#### Color-Space Structs

`ColorRGBA`: The standard 8-bit per channel color representation.
```c
typedef struct ColorRGBA { unsigned char r, g, b, a; } ColorRGBA;
```

`ColorHSV`: Represents a color in Hue-Saturation-Value format.
```c
typedef struct ColorHSV { float h, s, v; } ColorHSV;
```
-   `h`: Hue, in degrees (0-360).
-   `s`: Saturation (0.0 for grayscale, 1.0 for full color).
-   `v`: Value/Brightness (0.0 for black, 1.0 for full brightness).

`ColorYPQA`: A broadcast-safe color format separating luma from chroma.
```c
typedef struct ColorYPQA { unsigned char y, p, q, a; } ColorYPQA;
```
-   `y`: Luminance (brightness).
-   `p`, `q`: Phase and Quadrature (chroma components).
-   `a`: Alpha.

### API Reference

#### Temporal Oscillator System
*   `bool SituationTimerGetOscillatorState(int oscillator_id)`
    *   Gets the current binary state (`0` or `1`) of a temporal oscillator.
*   `bool SituationTimerGetPreviousOscillatorState(int oscillator_id)`
    *   Gets the binary state of an oscillator from the previous frame.
*   `bool SituationTimerHasOscillatorUpdated(int oscillator_id)`
    *   Checks if an oscillator's state has changed this frame (a single-trigger event).
*   `bool SituationTimerPingOscillator(int oscillator_id)`
    *   Checks if an oscillator's period has elapsed since the last successful "ping" of this function.
*   `uint64_t SituationTimerGetOscillatorTriggerCount(int oscillator_id)`
    *   Gets the total number of times an oscillator has flipped its state.
*   `double SituationTimerGetOscillatorPeriod(int oscillator_id)`
    *   Gets the current period of an oscillator in seconds.
*   `SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds)`
    *   Sets a new period for an oscillator at runtime.
*   `double SituationTimerGetPingProgress(int oscillator_id)`
    *   Gets the progress [0.0 to 1.0+] of the interval since the last successful ping.
*   `double SituationTimerGetTime(void)`
    *   Gets the total time elapsed since the library was initialized.

#### Color Space Conversions
*   `void SituationConvertColorToVec4(ColorRGBA c, vec4 out_normalized_color)`
    *   Converts an 8-bit RGBA color struct to a normalized floating-point `vec4`.
*   `ColorHSV SituationRgbToHsv(ColorRGBA rgb)`
    *   Converts a color from RGB to HSV color space.
*   `ColorRGBA SituationHsvToRgb(ColorHSV hsv)`
    *   Converts a color from HSV to RGB color space.
*   `ColorYPQA SituationColorToYPQ(ColorRGBA color)`
    *   Converts a color from RGBA to the YPQA (Luma, Phase, Quadrature) color space.
*   `ColorRGBA SituationColorFromYPQ(ColorYPQA ypq_color)`
    *   Converts a color from YPQA back to the RGBA color space.

#### Memory Management Helpers
*   `void SituationFreeString(char* str)`
    *   Frees the memory for a string allocated and returned by the library.
*   `void SituationFreeDisplays(SituationDisplayInfo* displays, int count)`
    *   Frees the memory for the array of display information returned by `SituationGetDisplays`.
</details>