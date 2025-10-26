# Situation API Programming Guide

"Situation" is a single-file, cross-platform C/C++ library designed for advanced platform awareness, control, and timing. It provides a comprehensive, immediate-mode API that abstracts the complexities of windowing, graphics (OpenGL/Vulkan), audio, and input. This guide serves as the primary technical manual for the library, detailing its architecture, usage patterns, and the complete Application Programming Interface (API).

## Table of Contents
- [Introduction and Core Concepts](#introduction-and-core-concepts)
- [Getting Started](#getting-started)
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

<details open>
<summary><h2>Introduction and Core Concepts</h2></summary>

This section explains the fundamental concepts, design philosophy, and core architectural patterns of the "Situation" library. A solid understanding of these ideas is crucial for using the library effectively.

### 1. Core Philosophy and Design
#### Immediate Mode and Explicit Control
The library favors a mostly **"immediate mode"** style API. This means that for many operations, you call a function and it takes effect immediately within the current frame. For example, `SituationCmdDrawQuad()` directly records a draw command into the current frame's command buffer. This approach is designed to be simple, intuitive, and easy to debug, contrasting with "retained mode" systems where you would build a scene graph of objects that is then rendered by the engine.

Complementing this is a philosophy of **explicit resource management**. Any resource you create (a texture, a mesh, a sound) must be explicitly destroyed by you using its corresponding `SituationDestroy...()` or `SituationUnload...()` function. This design choice avoids the complexities and performance overhead of automatic garbage collection and puts you in full control of resource lifecycles. To aid in debugging, the library will warn you at shutdown if you've leaked any GPU resources.

#### C-Style, Single-File, and Backend-Agnostic Architecture
- **C-Style, Data-Oriented API:** The API is pure C, promoting maximum portability and interoperability. It uses handles (structs passed by value) to represent opaque resources and pointers for functions that need to modify or destroy those resources. This approach is data-oriented, focusing on transforming data (e.g., vertex data into a mesh, image data into a texture) rather than on object-oriented hierarchies.
- **Single-File, Header-Only Distribution:** "Situation" is distributed as a single header file (`situation.h`), making it incredibly easy to integrate into your projects. To use it, you simply `#include "situation.h"` in your source files. In exactly one C or C++ file, you must first define `SITUATION_IMPLEMENTATION` before the include to create the implementation.
- **Backend Abstraction:** The library provides a unified API that works over different graphics backends (currently OpenGL and Vulkan). You choose the backend at compile time by defining either `SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`.

#### Strictly Single-Threaded Model
The library is **strictly single-threaded**. All API functions must be called from the same thread that called `SituationInit()`. Asynchronous operations, such as asset loading on a worker thread, must be handled by the user with care, ensuring that no `SITAPI` calls are made from outside the main thread.

### 2. Application Structure
#### The Application Lifecycle
The library enforces a strict and predictable lifecycle:
1.  **Initialization:** Call `SituationInit()` exactly once at the beginning. This sets up all subsystems. No other library functions should be called before this.
2.  **Main Loop:** After initialization, your application enters a loop that continues as long as `SituationWindowShouldClose()` returns `false`. This is where all real-time processing occurs.
3.  **Shutdown:** When the main loop terminates, you must call `SituationShutdown()` exactly once to gracefully tear down all subsystems and free resources.

#### The Three-Phase Frame
To ensure stable and predictable behavior, every iteration of the main loop must be divided into three distinct phases, executed in a specific order:
1.  **Input Phase:** At the very beginning of the frame, call `SituationPollInputEvents()`. This gathers all pending input from the operating system, ensuring that all logic in the frame operates on a consistent snapshot of input.
2.  **Update Phase:** Next, call `SituationUpdateTimers()` to calculate `deltaTime`. Immediately after, execute all of your application's logic (physics, AI, state changes). Using `deltaTime` is crucial for creating frame-rate-independent behavior.
3.  **Render Phase:** Finally, perform all rendering. This phase begins with `SituationAcquireFrameCommandBuffer()`, followed by recording all drawing commands, and concludes with `SituationEndFrame()`, which submits the work to the GPU.

### 3. Core API Patterns
#### Handles vs. Pointers
The API uses two patterns for interacting with objects:
- **Handles (by value):** Opaque structs like `SituationMesh` or `SituationShader` are typically passed by value to drawing or binding functions (e.g., `SituationCmdDrawMesh(my_mesh)`). These are lightweight identifiers for GPU resources.
- **Pointers (for modification):** When a function needs to modify or destroy a resource, you must pass a pointer to its handle (e.g., `SituationDestroyMesh(&my_mesh)`). This allows the function to invalidate the handle by setting its internal ID to 0, preventing accidental use after destruction.

#### Input Handling: Polling vs. Callbacks
The library offers two complementary models for handling input:
1.  **State Polling (`SituationIs...Down`, `SituationIs...Pressed`)**: This is the most common approach for real-time applications. Within your main `Update` phase, you can query the current state of any key or button. This is ideal for continuous actions (character movement) or single-trigger game events (jumping, shooting).
2.  **Event-Driven Callbacks (`SituationSet...Callback`)**: This model allows you to register callback functions that are invoked the moment an input event occurs. This is more efficient for UI interactions, text input, or other event-driven logic, as it avoids the need to check for input every single frame.

### 4. Rendering and Graphics
#### The Command Buffer
At the core of the rendering system is the **command buffer**. Rather than telling the GPU to "draw this now," you record a series of commands (prefixed with `SituationCmd...`) into a buffer. Once all commands for a frame are recorded, `SituationEndFrame()` submits the entire buffer to the GPU for execution. This batching approach is far more efficient and is central to how modern graphics APIs operate.

#### CPU-Side vs. GPU-Side Resources
The library makes a clear distinction between resources in system memory (CPU) and video memory (GPU).
- **`SituationImage` (CPU):** A block of pixel data in RAM. The Image module functions operate on this data, allowing for flexible manipulation (resizing, drawing text, etc.) without GPU overhead.
- **`SituationTexture` (GPU):** A GPU resource created by uploading a `SituationImage`. This is the object used by shaders for rendering.
The typical workflow is to load/generate a `SituationImage`, perform all desired manipulations, and then upload it once to a `SituationTexture` for efficient rendering.

#### Logical vs. Physical Coordinates (High-DPI)
Modern displays often have a high pixel density (High-DPI). The library abstracts this complexity:
-   **Logical Size (Screen Coordinates):** Dimensions used by the OS for window sizing and positioning. Functions like `SituationGetScreenWidth()` and `SituationGetMousePosition()` operate in this space. Use this for UI layout and logic.
-   **Physical Size (Render Pixels):** The actual number of pixels in the framebuffer (`SituationGetRenderWidth()`). This is the resolution the GPU renders to.
The library automatically handles this scaling. You can query the scaling factor using `SituationGetWindowScaleDPI()`.

#### The Virtual Display System
A "Virtual Display" is an **off-screen render target**. Instead of drawing directly to the main window, you can render a scene into a virtual display. This is incredibly powerful for post-processing effects (bloom, blur), UI layering (rendering UI at a fixed resolution), and caching parts of a scene that don't change frequently.

### 5. Other Key Systems
#### Audio: Sounds vs. Streams
The audio module can handle audio in two ways:
-   **Loaded Sounds (`SituationLoadSoundFromFile`):** Decodes the entire audio file into memory. Ideal for short, low-latency sound effects.
-   **Streamed Sounds (`SituationLoadSoundFromStream`):** Decodes the audio in small chunks as it's playing. Uses significantly less memory, making it perfect for long background music tracks.

#### Filesystem: Cross-Platform and Special Paths
The filesystem module abstracts away OS-specific differences. All paths are UTF-8. To ensure your application is portable, use the provided helper functions instead of hardcoding paths:
-   `SituationGetBasePath()`: Returns the directory containing your executable. Use this for loading application assets.
-   `SituationGetAppSavePath()`: Returns a platform-appropriate, user-specific directory for saving configuration files and user data.

#### The Temporal Oscillator System
This is a high-level timing utility for creating rhythmic, periodic events. You can initialize oscillators with specific periods (e.g., 0.5 seconds for 120 BPM). The library updates these timers independent of the frame rate, allowing you to easily synchronize animations, game logic, or visual effects to a steady, musical beat using functions like `SituationTimerHasOscillatorUpdated()`.

</details>

---

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
            SituationCmdBeginRenderPass(SituationGetMainCommandBuffer(), &pass_info);
            // ... Drawing commands go here ...
            SituationCmdEndRenderPass(SituationGetMainCommandBuffer());
            SituationEndFrame();
        }
    }
```

### Step 4: Shutdown
After the main loop finishes, it is critical to call `SituationShutdown()` to clean up all resources.

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
    };
    if (SituationInit(argc, argv, &init_info) != SIT_SUCCESS) {
        printf("Failed to initialize Situation: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // 2. Main Loop
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) break;

        if (SituationAcquireFrameCommandBuffer()) {
            SituationRenderPassInfo pass_info = {
                .color_load_action = SIT_LOAD_ACTION_CLEAR,
                .clear_color = {0, 12, 24, 255},
                .color_store_action = SIT_STORE_ACTION_STORE,
            };
            SituationCmdBeginRenderPass(SituationGetMainCommandBuffer(), &pass_info);
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

## Detailed API Reference

This section provides a complete list of all functions available in the "Situation" library, organized by module.

<details>
<summary><h3>Core Module</h3></summary>

**Overview:** The Core module is the heart of the "Situation" library, providing the essential functions for application lifecycle management. It handles initialization (`SituationInit`) and shutdown (`SituationShutdown`), processes the main event loop, and manages frame timing and rate control. This module also serves as a gateway to crucial system information, offering functions to query hardware details, manage command-line arguments, and set up critical application-wide callbacks.

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
-   `app_name`, `app_version`: The name and version of your application.
-   `initial_width`, `initial_height`: The desired initial dimensions for the main window.
-   `window_flags`: A bitmask of `SituationWindowStateFlags` to set the initial state of the window.
-   `target_fps`: The desired target frame rate. Use `0` for uncapped FPS.
-   `oscillator_count`, `oscillator_periods`: The number and initial periods (in seconds) for the Temporal Oscillator System.
-   `headless`: If `true`, the library initializes without a window or graphics context.

---
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

### Functions

#### `SituationInit`
Initializes the library, creates the main window, and sets up the graphics context. This must be the first function called.
```c
SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info);
```
**Usage Example:**
```c
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {
        .app_name = "Core Example",
        .initial_width = 1280,
        .initial_height = 720,
        .window_flags = SITUATION_FLAG_WINDOW_RESIZABLE
    };

    if (SituationInit(argc, argv, &init_info) != SIT_SUCCESS) {
        // Handle initialization failure
        return -1;
    }
    // ... main loop ...
    SituationShutdown();
    return 0;
}
```

---
#### `SituationPollInputEvents`
Polls for all pending input and window events from the operating system. Call this once at the beginning of every frame.
```c
void SituationPollInputEvents(void);
```
**Usage Example:**
```c
while (!SituationWindowShouldClose()) {
    SituationPollInputEvents(); // First call in the loop
    // ... update and render ...
}
```

---
#### `SituationUpdateTimers`
Updates all internal timers, including the main frame timer (`deltaTime`) and the Temporal Oscillator System. Call this once per frame, after polling events.
```c
void SituationUpdateTimers(void);
```
**Usage Example:**
```c
while (!SituationWindowShouldClose()) {
    SituationPollInputEvents();
    SituationUpdateTimers(); // Second call in the loop
    // ... application logic using deltaTime ...
    // ... render ...
}
```

---
#### `SituationShutdown`
Shuts down all library subsystems, releases all resources, and closes the application window. This should be the last function called.
```c
void SituationShutdown(void);
```
**Usage Example:**
```c
int main(int argc, char* argv[]) {
    SituationInit(argc, argv, NULL);
    while (!SituationWindowShouldClose()) {
        // ...
    }
    SituationShutdown(); // Last call before exit
    return 0;
}
```

---
#### `SituationIsInitialized`
Checks if the library has been successfully initialized.
```c
bool SituationIsInitialized(void);
```
**Usage Example:**
```c
if (!SituationIsInitialized()) {
    printf("Library was not initialized, or initialization failed.\n");
}
```

---
#### `SituationWindowShouldClose`
Returns `true` if the user has attempted to close the window (e.g., by clicking the 'X' button or pressing Alt+F4).
```c
bool SituationWindowShouldClose(void);
```
**Usage Example:**
```c
// The canonical main loop condition.
while (!SituationWindowShouldClose()) {
    // ...
}
```

---
#### `SituationSetTargetFPS`
Sets a target frame rate for the application. The main loop will sleep to avoid exceeding this rate. Pass `0` to uncap the frame rate.
```c
void SituationSetTargetFPS(int fps);
```
**Usage Example:**
```c
SituationInit(argc, argv, NULL);
SituationSetTargetFPS(60); // Cap the application at 60 FPS.
```

---
#### `SituationGetFrameTime`
Gets the time in seconds that the previous frame took to complete (also known as `deltaTime`). Essential for frame-rate-independent logic.
```c
float SituationGetFrameTime(void);
```
**Usage Example:**
```c
// Inside the main loop, after SituationUpdateTimers()
float dt = SituationGetFrameTime();
player_position.x += player_speed * dt; // Move player based on time, not frames.
```

---
#### `SituationGetFPS`
Gets the current frames-per-second, calculated periodically by the library.
```c
int SituationGetFPS(void);
```
**Usage Example:**
```c
// Inside the main loop
int current_fps = SituationGetFPS();
char window_title[128];
sprintf(window_title, "My App | FPS: %d", current_fps);
SituationSetWindowTitle(window_title);
```

---
#### `SituationGetLastErrorMsg`
Retrieves a copy of the last error message generated by the library. The caller is responsible for freeing this memory with `SituationFreeString()`.
```c
char* SituationGetLastErrorMsg(void);
```
**Usage Example:**
```c
if (SituationInit(argc, argv, &init_info) != SIT_SUCCESS) {
    char* error_msg = SituationGetLastErrorMsg();
    fprintf(stderr, "Initialization failed: %s\n", error_msg);
    SituationFreeString(error_msg); // IMPORTANT: Free the memory
    return -1;
}
```

---
#### `SituationSetExitCallback`
Registers a callback function to be executed just before the library shuts down. Useful for final cleanup.
```c
void on_exit_cleanup(void* user_data) {
    printf("Shutting down. Custom data: %s\n", (const char*)user_data);
}

// In main, after Init
const char* my_data = "Saved settings.";
SituationSetExitCallback(on_exit_cleanup, (void*)my_data);
```

---
#### `SituationIsArgumentPresent`
Checks if a specific command-line argument flag (e.g., `"-server"`) was provided when the application was launched.
```c
bool SituationIsArgumentPresent(const char* arg_name);
```
**Usage Example:**
```c
// Run as: ./my_app -fullscreen -debug
int main(int argc, char* argv[]) {
    SituationInit(argc, argv, NULL);
    if (SituationIsArgumentPresent("-fullscreen")) {
        SituationToggleFullscreen();
    }
}
```

---
#### `SituationGetArgumentValue`
Gets the value of a command-line argument (e.g., gets `"jungle"` from `"-level:jungle"` or `"-level jungle"`). Returns `NULL` if not found.
```c
const char* SituationGetArgumentValue(const char* arg_name);
```
**Usage Example:**
```c
// Run as: ./my_app -level:forest
int main(int argc, char* argv[]) {
    SituationInit(argc, argv, NULL);
    const char* level_name = SituationGetArgumentValue("-level");
    if (level_name) {
        printf("Loading level: %s\n", level_name);
    } else {
        printf("Loading default level.\n");
    }
}
```

---
#### `SituationGetDeviceInfo`
Gathers and returns a comprehensive snapshot of the host system's hardware, including CPU, GPU, RAM, and storage.
```c
SituationDeviceInfo SituationGetDeviceInfo(void);
```
**Usage Example:**
```c
SituationInit(argc, argv, NULL);
SituationDeviceInfo device = SituationGetDeviceInfo();
printf("GPU: %s\n", device.gpu_brand);
printf("CPU Cores: %d\n", device.cpu_core_count);
printf("System RAM: %llu GB\n", device.system_ram_bytes / (1024*1024*1024));
```

---
#### `SituationGetTime`
Gets the total elapsed time in seconds since the application was initialized.
```c
double SituationGetTime(void);
```
**Usage Example:**
```c
// This can be used for animations or calculating durations.
double current_time = SituationGetTime();
float brightness = (sin(current_time * 2.0) + 1.0) * 0.5; // Pulsating effect
```

---
#### `SituationGetFrameCount`
Gets the total number of frames that have been rendered since the application started.
```c
uint64_t SituationGetFrameCount(void);
```
**Usage Example:**
```c
// You can use the frame count for seeding random numbers or simple, periodic logic.
if (SituationGetFrameCount() % 120 == 0) {
    printf("Two seconds have passed (at 60 FPS).\n");
}
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

</details>
<details>
<summary><h3>Window and Display Module</h3></summary>

**Overview:** This module provides an exhaustive suite of tools for managing the application's window and querying the properties of physical display devices. It handles everything from basic window state changes (fullscreen, minimized, borderless) to more advanced features like DPI scaling, opacity, and clipboard access.

### Structs and Flags

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
-   `modes`: A pointer to an array of `SituationDisplayMode` structs.
-   `position`: The physical position of the monitor's top-left corner on the virtual desktop.
-   `physical_size`: The physical size of the display in millimeters.

---
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
-   `red_bits`, `green_bits`, `blue_bits`: The color depth for each channel.

---
#### `SituationWindowStateFlags`
These flags are used with `SituationSetWindowState()` and `SituationClearWindowState()` to control the window's appearance and behavior. They can be combined using the bitwise `|` operator.
| Flag | Description |
|---|---|
| `SITUATION_FLAG_VSYNC_HINT` | Suggests that the graphics backend should wait for vertical sync, reducing screen tearing. |
| `SITUATION_FLAG_FULLSCREEN_MODE` | Enables exclusive fullscreen mode. |
| `SITUATION_FLAG_WINDOW_RESIZABLE` | Allows the user to resize the window. |
| `SITUATION_FLAG_WINDOW_UNDECORATED` | Removes the window's border, title bar, and other decorations. |
| `SITUATION_FLAG_WINDOW_HIDDEN` | Hides the window from view. |
| `SITUATION_FLAG_WINDOW_MINIMIZED` | Minimizes the window to the taskbar. |
| `SITUATION_FLAG_WINDOW_MAXIMIZED` | Maximizes the window to fill the work area. |
| `SITUATION_FLAG_WINDOW_UNFOCUSED` | Prevents the window from gaining focus when created. |
| `SITUATION_FLAG_WINDOW_TOPMOST` | Keeps the window on top of all other windows. |
| `SITUATION_FLAG_WINDOW_ALWAYS_RUN` | Allows the application to continue running even when the window is minimized. |
| `SITUATION_FLAG_WINDOW_TRANSPARENT` | Enables a transparent framebuffer for non-rectangular window shapes. |
| `SITUATION_FLAG_HIGHDPI_HINT` | Requests a high-DPI framebuffer on platforms that support it (e.g., macOS Retina). |
| `SITUATION_FLAG_MSAA_4X_HINT` | Suggests that the graphics backend should use 4x multisample anti-aliasing. |

#### Functions
### Functions

#### `SituationSetWindowState` / `SituationClearWindowState`
Sets or clears one or more window state flags.
```c
void SituationSetWindowState(uint32_t flags);
void SituationClearWindowState(uint32_t flags);
```
**Usage Example:**
```c
// Make the window resizable and always on top.
SituationSetWindowState(SITUATION_FLAG_WINDOW_RESIZABLE | SITUATION_FLAG_WINDOW_TOPMOST);

// Later, remove the "always on top" state.
SituationClearWindowState(SITUATION_FLAG_WINDOW_TOPMOST);
```

---
#### `SituationToggleFullscreen`
Toggles the window between exclusive fullscreen and windowed mode.
```c
void SituationToggleFullscreen(void);
```
**Usage Example:**
```c
// In the update loop, toggle fullscreen when F11 is pressed.
if (SituationIsKeyPressed(SIT_KEY_F11)) {
    SituationToggleFullscreen();
}
```

---
#### `SituationSetWindowTitle`
Sets the text that appears in the window's title bar.
```c
void SituationSetWindowTitle(const char *title);
```
**Usage Example:**
```c
char title[100];
sprintf(title, "My Game | Score: %d", playerScore);
SituationSetWindowTitle(title);
```

---
#### `SituationSetWindowPosition` / `SituationGetWindowPosition`
Sets or gets the screen-space position of the top-left corner of the window's client area.
```c
void SituationSetWindowPosition(int x, int y);
vec2 SituationGetWindowPosition(void);
```
**Usage Example:**
```c
// Get the current position and move it 10 pixels to the right.
vec2 current_pos;
glm_vec2_copy(SituationGetWindowPosition(), current_pos);
SituationSetWindowPosition(current_pos[0] + 10, current_pos[1]);
```

---
#### `SituationSetWindowSize` / `SituationGetWindowSize`
Sets or gets the dimensions of the window's client area in screen coordinates.
```c
void SituationSetWindowSize(int width, int height);
vec2 SituationGetWindowSize(void);
```
**Usage Example:**
```c
// Set the window to a fixed 800x600 size
SituationSetWindowSize(800, 600);
// Verify the size
vec2 size;
glm_vec2_copy(SituationGetWindowSize(), size);
printf("Window size is now: %.0fx%.0f\n", size[0], size[1]);
```

---
#### `SituationSetWindowOpacity`
Sets the opacity of the entire window.
```c
void SituationSetWindowOpacity(float opacity);
```
**Usage Example:**
```c
// Make the window semi-transparent
SituationSetWindowOpacity(0.5f);
```

---
#### `SituationSetWindowIcon`
Sets a custom icon for the window from a `SituationImage`.
```c
void SituationSetWindowIcon(SituationImage image);
```
**Usage Example:**
```c
// During initialization
SituationImage icon = SituationLoadImage("assets/icon.png");
if (icon.data) {
    SituationSetWindowIcon(icon);
    SituationUnloadImage(icon);
}
```

---
#### `SituationRequestWindowAttention`
Requests the user's attention by making the window's icon flash or bounce in the taskbar.
```c
void SituationRequestWindowAttention(void);
```
**Usage Example:**
```c
// When a long background task is complete
SituationRequestWindowAttention();
```

---
#### `SituationIsWindowState`
Checks if a specific window state (e.g., `SITUATION_FLAG_WINDOW_MAXIMIZED`) is currently active.
```c
bool SituationIsWindowState(uint32_t flag);
```
**Usage Example:**
```c
if (SituationIsWindowState(SITUATION_FLAG_FULLSCREEN_MODE)) {
    printf("Application is in fullscreen mode.\n");
}
```

---
#### `SituationGetScreenWidth` / `SituationGetScreenHeight`
Gets the current width/height of the window in screen coordinates (logical size).
```c
int SituationGetScreenWidth(void);
int SituationGetScreenHeight(void);
```
**Usage Example:**
```c
int screenWidth = SituationGetScreenWidth();
int screenHeight = SituationGetScreenHeight();
printf("Window logical size: %dx%d\n", screenWidth, screenHeight);
```

---
#### `SituationGetRenderWidth` / `SituationGetRenderHeight`
Gets the current width/height of the rendering framebuffer in pixels (physical size).
```c
int SituationGetRenderWidth(void);
int SituationGetRenderHeight(void);
```
**Usage Example:**
```c
// On a high-DPI display, this might be larger than the screen size.
int framebufferWidth = SituationGetRenderWidth();
int framebufferHeight = SituationGetRenderHeight();
// Set the graphics viewport to match the framebuffer size.
SituationCmdSetViewport(SituationGetMainCommandBuffer(), 0, 0, framebufferWidth, framebufferHeight);
```

---
#### `SituationGetWindowScaleDPI`
Gets the ratio of physical pixels to logical screen coordinates.
```c
vec2 SituationGetWindowScaleDPI(void);
```
**Usage Example:**
```c
// On a Retina display, this might return (2.0, 2.0). On standard displays, (1.0, 1.0).
vec2 dpi_scale;
glm_vec2_copy(SituationGetWindowScaleDPI(), dpi_scale);
printf("DPI Scale: %.1fx, %.1fy\n", dpi_scale[0], dpi_scale[1]);
```

---
#### `SituationGetCurrentMonitor`
Gets the index of the monitor that the window is currently on.
```c
int SituationGetCurrentMonitor(void);
```
**Usage Example:**
```c
int current_monitor_id = SituationGetCurrentMonitor();
printf("Window is on monitor %d.\n", current_monitor_id);
```

---
#### `SituationGetMonitorName`
Gets the human-readable name of a monitor by its index.
```c
const char* SituationGetMonitorName(int monitor);
```
**Usage Example:**
```c
int monitor_count;
SituationGetDisplays(&monitor_count);
for (int i = 0; i < monitor_count; i++) {
    printf("Monitor %d is called: %s\n", i, SituationGetMonitorName(i));
}
```

---
#### `SituationGetMonitorWidth` / `SituationGetMonitorHeight`
Gets the current resolution of a monitor by its index.
```c
int SituationGetMonitorWidth(int monitor);
int SituationGetMonitorHeight(int monitor);
```
**Usage Example:**
```c
int primary_monitor_width = SituationGetMonitorWidth(0);
int primary_monitor_height = SituationGetMonitorHeight(0);
printf("Primary monitor resolution: %dx%d\n", primary_monitor_width, primary_monitor_height);
```

---
#### `SituationGetDisplays`
Retrieves detailed information for all connected displays. The caller is responsible for freeing the returned array using `SituationFreeDisplays`.
```c
SituationDisplayInfo* SituationGetDisplays(int* count);
```
**Usage Example:**
```c
int display_count;
SituationDisplayInfo* displays = SituationGetDisplays(&display_count);
for (int i = 0; i < display_count; i++) {
    printf("Display %d: %s\n", i, displays[i].name);
}
SituationFreeDisplays(displays, display_count); // Don't forget to free!
```

---
#### `SituationShowCursor` / `SituationHideCursor` / `SituationDisableCursor`
Controls the visibility and behavior of the mouse cursor.
```c
void SituationShowCursor(void);   // Normal visibility and behavior.
void SituationHideCursor(void);   // Invisible when over the window.
void SituationDisableCursor(void); // Hidden and locked for unbounded movement (camera controls).
```
**Usage Example:**
```c
// For a 3D game, disable the cursor.
SituationDisableCursor();
// For a menu, show it.
SituationShowCursor();
```

---
#### `SituationGetClipboardText` / `SituationSetClipboardText`
Gets or sets text in the system clipboard.
```c
const char* SituationGetClipboardText(void);
void SituationSetClipboardText(const char* text);
```
**Usage Example:**
```c
// Copy "Hello" to clipboard
SituationSetClipboardText("Hello, World!");

// Paste from clipboard
const char* clipboard = SituationGetClipboardText();
if (clipboard) {
    printf("Clipboard contains: %s\n", clipboard);
}
```

---
#### `SituationLoadDroppedFiles`
Get the paths of files dragged and dropped onto the window. The caller must free the returned list with `SituationUnloadDroppedFiles`.
```c
char** SituationLoadDroppedFiles(int* count);
```
**Usage Example:**
```c
// In the update loop
if (SituationIsFileDropped()) {
    int file_count;
    char** files = SituationLoadDroppedFiles(&file_count);
    for (int i = 0; i < file_count; i++) {
        printf("Dropped file: %s\n", files[i]);
    }
    SituationUnloadDroppedFiles(files, file_count); // Free the list
}
```

</details>
<details>
<summary><h3>Image Module</h3></summary>

**Overview:** The Image module is a comprehensive, CPU-side toolkit for all forms of image and font manipulation. It allows you to load images, generate new images programmatically, perform transformations, and render text. The `SituationImage` objects produced by this module are the primary source for creating GPU-side `SituationTexture`s.

### Structs

#### `SituationImage`
A handle representing a CPU-side image. All pixel data is stored in uncompressed 32-bit RGBA format.
```c
typedef struct SituationImage {
    void *data;
    int width;
    int height;
} SituationImage;
```
---
#### `SituationFont`
A handle representing a CPU-side font, loaded from a TTF or OTF file.
```c
typedef struct SituationFont {
    void *fontData;
    void *stbFontInfo;
} SituationFont;
```

### Functions

#### Image Loading and Unloading
---
#### `SituationLoadImage` / `SituationUnloadImage`
Loads an image from a file into CPU memory (RAM), and later unloads it.
```c
SituationImage SituationLoadImage(const char *fileName);
void SituationUnloadImage(SituationImage image);
```
**Usage Example:**
```c
SituationImage player_avatar = SituationLoadImage("avatars/player1.png");
if (player_avatar.data) {
    // ... use the image data ...
    SituationUnloadImage(player_avatar);
}
```
---
#### `SituationLoadImageFromMemory`
Loads an image from a buffer in memory.
```c
SituationImage SituationLoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize);
```
**Usage Example:**
```c
// Assume 'g_embedded_player_png' is a byte array with an embedded PNG file.
// Assume 'g_embedded_player_png_len' is its size.
SituationImage player_img = SituationLoadImageFromMemory(".png", g_embedded_player_png, g_embedded_player_png_len);
// ... use player_img ...
SituationUnloadImage(player_img);
```

---
#### `SituationLoadImageFromTexture`
Creates a CPU-side `SituationImage` by reading back data from a GPU `SituationTexture`.
```c
SituationImage SituationLoadImageFromTexture(SituationTexture texture);
```

---
#### `SituationExportImage`
Exports image data to a file (PNG or BMP supported).
```c
bool SituationExportImage(SituationImage image, const char *fileName);
```

---
#### Image Generation and Manipulation
---
#### `SituationImageFromImage`
Creates a new `SituationImage` from a sub-rectangle of another image.
```c
SituationImage SituationImageFromImage(SituationImage image, Rectangle rect);
```
**Usage Example:**
```c
// Extract a 16x16 sprite from a larger spritesheet.
SituationImage spritesheet = SituationLoadImage("assets/sprites.png");
Rectangle sprite_rect = { .x = 32, .y = 16, .width = 16, .height = 16 };
SituationImage single_sprite = SituationImageFromImage(spritesheet, sprite_rect);
// ... use single_sprite ...
SituationUnloadImage(single_sprite);
SituationUnloadImage(spritesheet);
```

---
#### `SituationImageCopy`
Creates a new image by making a deep copy of another.
```c
SituationImage SituationImageCopy(SituationImage image);
```
---
#### `SituationGenImageColor`
Generates a new image filled with a single, solid color.
```c
SituationImage SituationGenImageColor(int width, int height, ColorRGBA color);
```
**Usage Example:**
```c
SituationImage blue_square = SituationGenImageColor(128, 128, (ColorRGBA){0, 0, 255, 255});
// ... use the blue square image ...
SituationUnloadImage(blue_square);
```

---
#### `SituationGenImageGradient`
Generates an image with a linear, radial, or square gradient.
```c
SituationImage SituationGenImageGradient(int width, int height, int type, ColorRGBA start, ColorRGBA end);
```
**Usage Example:**
```c
// Create a vertical gradient from red to black
SituationImage background = SituationGenImageGradient(1280, 720, 1, (ColorRGBA){255,0,0,255}, (ColorRGBA){0,0,0,255});
// ... use background ...
SituationUnloadImage(background);
```

---
#### `SituationImageClearBackground`
Clears an image to a specific color.
```c
void SituationImageClearBackground(SituationImage *dst, ColorRGBA color);
```

---
#### `SituationImageDraw`
Draws a source image onto a destination image.
```c
void SituationImageDraw(SituationImage *dst, SituationImage src, Rectangle srcRect, Rectangle dstRect, ColorRGBA tint);
```

---
#### `SituationImageDrawRectangle` / `SituationImageDrawLine`
Draws a colored rectangle or line onto an image.
```c
void SituationImageDrawRectangle(SituationImage *dst, Rectangle rect, ColorRGBA color);
void SituationImageDrawLine(SituationImage *dst, Vector2 start, Vector2 end, ColorRGBA color);
```
**Usage Example:**
```c
SituationImage canvas = SituationGenImageColor(256, 256, (ColorRGBA){255,255,255,255});
Rectangle red_box = { .x = 50, .y = 50, .width = 156, .height = 156 };
SituationImageDrawRectangle(&canvas, red_box, (ColorRGBA){255,0,0,255});
```

---
#### `SituationImageCrop` / `SituationImageResize`
Crops or resizes an image in-place.
```c
void SituationImageCrop(SituationImage *image, Rectangle crop);
void SituationImageResize(SituationImage *image, int newWidth, int newHeight);
```
**Usage Example:**
```c
SituationImage atlas = SituationLoadImage("sprite_atlas.png");
// Crop the atlas to get the first sprite (e.g., a 32x32 sprite at top-left)
SituationImageCrop(&atlas, (Rectangle){0, 0, 32, 32});
// Now 'atlas' contains only the cropped sprite data.
SituationUnloadImage(atlas);
```

---
#### `SituationImageFlipVertical` / `SituationImageFlipHorizontal`
Flips an image vertically or horizontally in-place.
```c
void SituationImageFlipVertical(SituationImage *image);
void SituationImageFlipHorizontal(SituationImage *image);
```

---
#### `SituationImageRotate`
Rotates an image by a multiple of 90 degrees clockwise in-place.
```c
void SituationImageRotate(SituationImage *image, int rotations);
```

---
#### `SituationImageColorTint` / `SituationImageColorInvert`
Applies a color tint or inverts the colors of an image in-place.
```c
void SituationImageColorTint(SituationImage *image, ColorRGBA color);
void SituationImageColorInvert(SituationImage *image);
```

---
#### `SituationImageColorGrayscale` / `SituationImageColorContrast` / `SituationImageColorBrightness`
Adjusts the grayscale, contrast, or brightness of an image in-place.
```c
void SituationImageColorGrayscale(SituationImage *image);
void SituationImageColorContrast(SituationImage *image, float contrast);
void SituationImageColorBrightness(SituationImage *image, int brightness);
```

---
#### `SituationImageAlphaMask` / `SituationImagePremultiplyAlpha`
Applies an alpha mask to an image or premultiplies the color channels by the alpha channel.
```c
void SituationImageAlphaMask(SituationImage *image, SituationImage alphaMask);
void SituationImagePremultiplyAlpha(SituationImage *image);
```

---
#### Font and Text Rendering
---
#### `SituationLoadFont` / `SituationUnloadFont`
Loads a font from a TTF/OTF file for CPU-side rendering, and later unloads it.
```c
SituationFont SituationLoadFont(const char *fileName);
void SituationUnloadFont(SituationFont font);
```

---
#### `SituationLoadFontFromMemory`
Loads a font from a data buffer in memory.
```c
SituationFont SituationLoadFontFromMemory(const unsigned char *fileData, int dataSize);
```

---
#### `SituationGenImageFontAtlas`
Generates a texture atlas (a single image containing all characters) from a font. This is a more advanced and efficient way to handle font rendering.
```c
SituationImage SituationGenImageFontAtlas(SituationFont font, int fontSize, int padding, int packMethod);
```

---
#### `SituationMeasureText`
Measures the dimensions of a string of text if it were to be rendered with a specific font, size, and spacing.
```c
vec2 SituationMeasureText(SituationFont font, const char *text, float fontSize, float spacing);
```
**Usage Example:**
```c
const char* button_text = "Click Me!";
vec2 text_size;
glm_vec2_copy(SituationMeasureText(my_font, button_text, 20, 1), text_size);
// Now you can create a button rectangle that perfectly fits the text.
Rectangle button_rect = { .x = 100, .y = 100, .width = text_size[0] + 20, .height = text_size[1] + 10 };
```

---
#### `SituationImageDrawText`
Draws a simple, tinted text string onto an image.
```c
void SituationImageDrawText(SituationImage *dst, SituationFont font, const char *text, vec2 position, float fontSize, float spacing, ColorRGBA tint);
```
**Usage Example:**
```c
SituationImage canvas = SituationGenImageColor(800, 600, (ColorRGBA){20, 20, 20, 255});
SituationFont my_font = SituationLoadFont("fonts/my_font.ttf");

SituationImageDrawText(&canvas, my_font, "Hello, World!", (vec2){50, 50}, 40, 1, (ColorRGBA){255, 255, 255, 255});

// ... you can now upload 'canvas' to a GPU texture ...

SituationUnloadFont(my_font);
SituationUnloadImage(canvas);
```
</details>
<details>
<summary><h3>Graphics Module</h3></summary>

**Overview:** The Graphics module forms the core of the rendering pipeline, offering a powerful, backend-agnostic API for interacting with the GPU. It is responsible for all GPU resource management (meshes, shaders, textures) and its command-buffer-centric design (`SituationCmd...`) allows you to precisely sequence rendering operations.

### Structs, Enums, and Handles

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
-   `color_load_action`, `depth_load_action`: What to do with the buffer at the start of the pass (`SIT_LOAD_ACTION_LOAD`, `_CLEAR`, or `_DONT_CARE`).
-   `color_store_action`, `depth_store_action`: What to do with the buffer at the end of the pass (`SIT_STORE_ACTION_STORE` or `_DONT_CARE`).
-   `clear_color`, `clear_depth`: The values to use if the load action is `_CLEAR`.
-   `virtual_display_id`: The ID of a virtual display to render to. Use `-1` to target the main window.

---
#### Resource Handles
`SituationMesh`, `SituationShader`, `SituationTexture`, `SituationBuffer`, `SituationModel`, `SituationComputePipeline`: These are opaque handles to GPU resources. Their internal structure is not exposed to the user. You create them with `SituationCreate...` or `SituationLoad...` functions and free them with their corresponding `SituationDestroy...` or `SituationUnload...` functions.

---
#### `SituationBufferUsageFlags`
Specifies how a `SituationBuffer` will be used. This helps the driver place the buffer in the most optimal memory.
| Flag | Description |
|---|---|
| `SIT_BUFFER_USAGE_VERTEX` | The buffer will be used as a vertex buffer. |
| `SIT_BUFFER_USAGE_INDEX` | The buffer will be used as an index buffer. |
| `SIT_BUFFER_USAGE_UNIFORM` | The buffer will be used as a Uniform Buffer Object (UBO). |
| `SIT_BUFFER_USAGE_STORAGE` | The buffer will be used as a Shader Storage Buffer Object (SSBO). |
| `SIT_BUFFER_USAGE_TRANSFER_SRC`| The buffer can be used as a source for a copy operation. |
| `SIT_BUFFER_USAGE_TRANSFER_DST`| The buffer can be used as a destination for a copy operation. |

---
#### `SituationComputeLayoutType`
Defines the descriptor set layout for a compute pipeline, telling the GPU what kind of resources the shader expects.
| Type | Description |
|---|---|
| `SIT_COMPUTE_LAYOUT_EMPTY`| The compute shader does not use any resources. |
| `SIT_COMPUTE_LAYOUT_IMAGE`| The pipeline expects a single storage image to be bound at binding 0. |
| `SIT_COMPUTE_LAYOUT_BUFFER`| The pipeline expects a single storage buffer to be bound at binding 0. |
| `SIT_COMPUTE_LAYOUT_BUFFER_X2`| The pipeline expects two storage buffers to be bound at bindings 0 and 1. |


#### Functions
### Functions

#### Frame Lifecycle & Command Buffer
These functions control the overall rendering loop.

---
#### `SituationAcquireFrameCommandBuffer`
Prepares the backend for a new frame of rendering, acquiring the next available render target. Must be called before any drawing commands. Returns `false` if the frame cannot be acquired (e.g., window is minimized).
```c
bool SituationAcquireFrameCommandBuffer(void);
```
**Usage Example:**
```c
// At the start of the rendering phase
if (SituationAcquireFrameCommandBuffer()) {
    // ... record rendering commands ...
    SituationEndFrame();
}
```

---
#### `SituationEndFrame`
Submits all recorded commands for the frame and presents the result to the screen.
```c
SituationError SituationEndFrame(void);
```
**Usage Example:**
```c
// At the very end of the rendering phase
if (SituationAcquireFrameCommandBuffer()) {
    // ... record rendering commands ...
    SituationEndFrame();
}
```

---
#### `SituationGetMainCommandBuffer`
Gets a handle to the main command buffer used for rendering to the window.
```c
SituationCommandBuffer SituationGetMainCommandBuffer(void);
```

---
#### `SituationCmdBeginRenderPass` / `SituationCmdEndRenderPass`
Begins and ends a render pass. All drawing commands must be recorded between these two calls.
```c
SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info);
void SituationCmdEndRenderPass(SituationCommandBuffer cmd);
```
**Usage Example:**
```c
SituationRenderPassInfo pass_info = {
    .color_load_action = SIT_LOAD_ACTION_CLEAR,
    .clear_color = {20, 30, 40, 255}, // Dark blue
    .color_store_action = SIT_STORE_ACTION_STORE,
};
SituationCmdBeginRenderPass(SituationGetMainCommandBuffer(), &pass_info);
// ... draw calls ...
SituationCmdEndRenderPass(SituationGetMainCommandBuffer());
```

---
#### Rendering Commands
These functions record drawing and state-setting operations into the command buffer.

---
#### `SituationCmdSetViewport` / `SituationCmdSetScissor`
Sets the dynamic viewport or scissor rectangle for the current render pass.
```c
void SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height);
void SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);
```
**Usage Example:**
```c
// Render to the left half of the screen
int w = SituationGetRenderWidth();
int h = SituationGetRenderHeight();
SituationCmdSetViewport(SituationGetMainCommandBuffer(), 0, 0, w / 2.0f, h);
```

---
#### `SituationCmdBindPipeline`
Binds a graphics pipeline (shader program) for subsequent draws.
```c
SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader);
```
**Usage Example:**
```c
SituationCmdBindPipeline(SituationGetMainCommandBuffer(), my_shader);
SituationCmdDrawMesh(SituationGetMainCommandBuffer(), my_mesh);
```

---
#### `SituationCmdBindVertexBuffer` / `SituationCmdBindIndexBuffer`
Binds a vertex or index buffer for subsequent indexed draws.
```c
void SituationCmdBindVertexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer);
void SituationCmdBindIndexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer);
```

---
#### `SituationCmdBindShaderBuffer` / `SituationCmdBindShaderTexture`
Binds a uniform/storage buffer or texture to a specific binding point for use in a shader.
```c
void SituationCmdBindShaderBuffer(SituationCommandBuffer cmd, int binding, SituationBuffer buffer);
void SituationCmdBindShaderTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);
```
**Usage Example:**
```c
// In GLSL: layout(binding = 0) uniform sampler2D u_albedo;
// In C:
SituationCmdBindShaderTexture(cmd, 0, my_albedo_texture);

// In GLSL: layout(binding = 1) uniform SceneData { mat4 view; mat4 proj; }; u_scene;
// In C:
SituationCmdBindShaderBuffer(cmd, 1, my_scene_ubo);
```

---
#### `SituationCmdDraw` / `SituationCmdDrawIndexed`
Records a non-indexed or indexed drawing command.
```c
void SituationCmdDraw(SituationCommandBuffer cmd, int first_vertex, int vertex_count);
void SituationCmdDrawIndexed(SituationCommandBuffer cmd, int first_index, int index_count, int vertex_offset);
```
**Usage Example:**
```c
// Draw a mesh using previously bound vertex and index buffers.
SituationCmdBindVertexBuffer(cmd, my_vbo);
SituationCmdBindIndexBuffer(cmd, my_ibo);
SituationCmdDrawIndexed(cmd, 0, 36, 0); // Draw 36 indices
```

---
#### `SituationCmdDrawMesh`
Records a command to draw a complete, pre-configured mesh.
```c
SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh);
```
**Usage Example:**
```c
SituationCmdBindPipeline(SituationGetMainCommandBuffer(), my_shader);
// This mesh has its own vertex and index buffers.
SituationCmdDrawMesh(SituationGetMainCommandBuffer(), my_complex_model_mesh);
```

---
#### `SituationCmdDrawQuad`
Records a command to draw a simple, colored, and transformed 2D quad using an internally managed mesh.
```c
void SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, vec4 color);
```
**Usage Example:**
```c
mat4 transform;
glm_translate_make(transform, (vec3){100.0f, 200.0f, 0.0f});
vec4 quad_color = {1.0f, 0.0f, 1.0f, 1.0f}; // Magenta
SituationCmdDrawQuad(SituationGetMainCommandBuffer(), transform, quad_color);
```

---
#### Resource Management
These functions create and destroy GPU resources.

---
#### `SituationCreateMesh` / `SituationDestroyMesh`
Creates a self-contained GPU mesh from vertex and index data, and later destroys it.
```c
SituationMesh SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count);
void SituationDestroyMesh(SituationMesh* mesh);
```
**Usage Example:**
```c
// At init
MyVertex vertices[] = { ... };
uint32_t indices[] = { ... };
g_my_mesh = SituationCreateMesh(vertices, 4, sizeof(MyVertex), indices, 6);

// At shutdown
SituationDestroyMesh(&g_my_mesh);
```

---
#### `SituationLoadShader` / `SituationUnloadShader`
Loads, compiles, and links a graphics shader pipeline from GLSL files, and later unloads it.
```c
SituationShader SituationLoadShader(const char* vs_path, const char* fs_path);
void SituationUnloadShader(SituationShader* shader);
```
**Usage Example:**
```c
// At init
g_my_shader = SituationLoadShader("shaders/simple.vert", "shaders/simple.frag");

// At shutdown
SituationUnloadShader(&g_my_shader);
```

---
#### `SituationCreateTexture` / `SituationDestroyTexture`
Creates a GPU texture from a CPU-side `SituationImage`, and later destroys it.
```c
SituationTexture SituationCreateTexture(SituationImage image, bool generate_mipmaps);
void SituationDestroyTexture(SituationTexture* texture);
```
**Usage Example:**
```c
// At init
SituationImage cpu_image = SituationLoadImage("textures/player.png");
g_my_texture = SituationCreateTexture(cpu_image, true);
SituationUnloadImage(cpu_image); // CPU copy is no longer needed

// At shutdown
SituationDestroyTexture(&g_my_texture);
```

---
#### `SituationLoadModel` / `SituationUnloadModel`
Loads a 3D model (including meshes and materials) from a file (GLTF, OBJ), and later unloads it.
```c
SituationModel SituationLoadModel(const char* file_path);
void SituationUnloadModel(SituationModel* model);
```

---
#### `SituationCreateBuffer` / `SituationDestroyBuffer`
Creates a generic GPU buffer (for vertices, indices, uniforms, etc.), and later destroys it.
```c
SituationBuffer SituationCreateBuffer(uint32_t usage_flags, const void* data, size_t size);
void SituationDestroyBuffer(SituationBuffer* buffer);
```
**Usage Example:**
```c
// Create a uniform buffer for camera matrices
mat4 proj, view;
// ... calculate matrices ...
CameraMatrices ubo_data = { .projection = proj, .view = view };
g_camera_ubo = SituationCreateBuffer(SIT_BUFFER_USAGE_UNIFORM, &ubo_data, sizeof(ubo_data));

// At shutdown
SituationDestroyBuffer(&g_camera_ubo);
```

---
#### `SituationUpdateBuffer`
Updates the contents of a GPU buffer with new data from the CPU.
```c
SituationError SituationUpdateBuffer(SituationBuffer buffer, const void* data, size_t size);
```

---
#### Compute Shaders

---
#### `SituationCreateComputePipeline` / `SituationDestroyComputePipeline`
Creates a compute pipeline from a GLSL shader file.
```c
SituationComputePipeline SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type);
void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);
```

---
#### `SituationCmdBindComputePipeline`
Binds a compute pipeline for a subsequent dispatch.
```c
void SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline);
```

---
#### `SituationCmdBindComputeBuffer` / `SituationCmdBindComputeTexture`
Binds a storage buffer or storage image to a specific binding point for use in a compute shader.
```c
void SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, int binding, SituationBuffer buffer);
void SituationCmdBindComputeTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);
```

---
#### `SituationCmdDispatch`
Records a command to execute a compute shader.
```c
void SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
```
**Usage Example:**
```c
// In render loop, before the main render pass
SituationCmdBindComputePipeline(SituationGetMainCommandBuffer(), my_compute_pipeline);
SituationCmdBindComputeTexture(SituationGetMainCommandBuffer(), 0, my_storage_image);
// Dispatch a 16x16 grid of thread groups.
SituationCmdDispatch(SituationGetMainCommandBuffer(), 16, 16, 1);
// A pipeline barrier is needed here to sync with the graphics pass
```

---
#### `SituationCmdPipelineBarrier`
Inserts a barrier into the command buffer, synchronizing memory access between different pipeline stages (e.g., between a compute pass and a graphics pass).
```c
void SituationCmdPipelineBarrier(SituationCommandBuffer cmd);
```

---
#### Virtual Displays

---
#### `SituationCreateVirtualDisplay`
Creates an off-screen render target (framebuffer object).
```c
int SituationCreateVirtualDisplay(vec2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode);
```

---
#### `SituationDestroyVirtualDisplay`
Destroys a virtual display and its associated resources.
```c
SituationError SituationDestroyVirtualDisplay(int* display_id);
```

---
#### `SituationSetVirtualDisplayVisible`
Sets whether a virtual display should be rendered during the compositing pass.
```c
void SituationSetVirtualDisplayVisible(int display_id, bool visible);
```

---
#### `SituationGetVirtualDisplayTexture`
Gets a handle to the underlying `SituationTexture` for a virtual display's color buffer. This allows you to use the output of one render pass as an input texture for another (e.g., for post-processing).
```c
SituationTexture SituationGetVirtualDisplayTexture(int display_id);
```

---
#### `SituationRenderVirtualDisplays`
Composites all visible virtual displays onto the current render target.
```c
void SituationRenderVirtualDisplays(SituationCommandBuffer cmd);
```
**Usage Example:**
```c
// At init: Create a display for the 3D scene
int scene_vd = SituationCreateVirtualDisplay((vec2){640, 360}, ...);

// In render loop:
// 1. Render scene to the virtual display
SituationRenderPassInfo scene_pass = { .virtual_display_id = scene_vd, ... };
SituationCmdBeginRenderPass(cmd, &scene_pass);
// ... draw 3D models ...
SituationCmdEndRenderPass(cmd);

// 2. Render to the main window
SituationRenderPassInfo final_pass = { .virtual_display_id = -1, ... };
SituationCmdBeginRenderPass(cmd, &final_pass);
// This composites the 3D scene from its virtual display onto the main window
SituationRenderVirtualDisplays(cmd);
// ... draw UI on top ...
SituationCmdEndRenderPass(cmd);
```

---
#### Miscellaneous

---
#### `SituationLoadImageFromScreen`
Captures the current contents of the main window's backbuffer into a CPU-side image.
```c
SituationImage SituationLoadImageFromScreen(void);
```
**Usage Example:**
```c
if (SituationIsKeyPressed(SIT_KEY_F12)) {
    SituationImage screenshot = SituationLoadImageFromScreen();
    SituationExportImage(screenshot, "screenshot.png");
    SituationUnloadImage(screenshot);
}
```

</details>
<details>
<summary><h3>Input Module</h3></summary>

**Overview:** The Input module provides a flexible interface for handling user input from keyboards, mice, and gamepads. It supports both state polling (e.g., `SituationIsKeyDown()`) for continuous actions and event-driven callbacks (e.g., `SituationSetKeyCallback()`) for discrete events.

### Callbacks
The input module allows you to register callback functions to be notified of input events as they happen, as an alternative to polling for state each frame.

#### `SituationKeyCallback`
`typedef void (*SituationKeyCallback)(int key, int scancode, int action, int mods, void* user_data);`
-   `key`: The keyboard key that was pressed or released (e.g., `SIT_KEY_A`).
-   `scancode`: The system-specific scancode of the key.
-   `action`: The key action (`SIT_PRESS`, `SIT_RELEASE`, or `SIT_REPEAT`).
-   `mods`: A bitmask of modifier keys that were held down (`SIT_MOD_SHIFT`, `SIT_MOD_CONTROL`, etc.).
-   `user_data`: The custom user data pointer you provided when setting the callback.

---
#### `SituationMouseButtonCallback`
`typedef void (*SituationMouseButtonCallback)(int button, int action, int mods, void* user_data);`
-   `button`: The mouse button that was pressed or released (e.g., `SIT_MOUSE_BUTTON_LEFT`).
-   `action`: The button action (`SIT_PRESS` or `SIT_RELEASE`).
-   `mods`: A bitmask of modifier keys.
-   `user_data`: Custom user data.

---
#### `SituationCursorPosCallback`
`typedef void (*SituationCursorPosCallback)(double xpos, double ypos, void* user_data);`
-   `xpos`, `ypos`: The new cursor position in screen coordinates.
-   `user_data`: Custom user data.

---
#### `SituationScrollCallback`
`typedef void (*SituationScrollCallback)(double xoffset, double yoffset, void* user_data);`
-   `xoffset`, `yoffset`: The scroll offset.
-   `user_data`: Custom user data.

#### Functions
### Functions

#### Keyboard Input
---
#### `SituationIsKeyDown` / `SituationIsKeyUp`
Checks if a key is currently held down or up (a continuous state).
```c
bool SituationIsKeyDown(int key);
bool SituationIsKeyUp(int key);
```
**Usage Example:**
```c
// For continuous movement
if (SituationIsKeyDown(SIT_KEY_W)) {
    player.y -= PLAYER_SPEED * SituationGetFrameTime();
}
```
---
#### `SituationIsKeyPressed` / `SituationIsKeyReleased`
Checks if a key was pressed down or released this frame (a single-trigger event).
```c
bool SituationIsKeyPressed(int key);
bool SituationIsKeyReleased(int key);
```
**Usage Example:**
```c
// For discrete actions like jumping
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    player.velocity_y = JUMP_FORCE;
}
```
---
#### `SituationSetKeyCallback`
Sets a callback function for all keyboard key events.
```c
void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data);
```
**Usage Example:**
```c
void my_key_logger(int key, int scancode, int action, int mods, void* user_data) {
    if (action == SIT_PRESS) {
        printf("Key pressed: %d\n", key);
    }
}
SituationSetKeyCallback(my_key_logger, NULL);
```

---
#### `SituationSetMouseButtonCallback` / `SituationSetCursorPosCallback` / `SituationSetScrollCallback`
Sets callback functions for mouse button, cursor movement, and scroll wheel events.
```c
void SituationSetMouseButtonCallback(SituationMouseButtonCallback callback, void* user_data);
void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data);
void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data);
```

---
#### `SituationSetCharCallback`
Sets a callback for Unicode character input, which is useful for text entry fields.
```c
void SituationSetCharCallback(SituationCharCallback callback, void* user_data);
```

---
#### `SituationSetDropCallback`
Sets a callback that is fired when files are dragged and dropped onto the window.
```c
void SituationSetDropCallback(SituationDropCallback callback, void* user_data);
```

---
#### Mouse Input
---
#### `SituationGetMousePosition` / `SituationGetMouseDelta`
Gets the mouse position within the window, or the mouse movement since the last frame.
```c
vec2 SituationGetMousePosition(void);
vec2 SituationGetMouseDelta(void);
```
**Usage Example:**
```c
vec2 mouse_pos = SituationGetMousePosition();
vec2 mouse_delta = SituationGetMouseDelta();
printf("Mouse at (%.f, %.f), moved by (%.f, %.f)\n", mouse_pos[0], mouse_pos[1], mouse_delta[0], mouse_delta[1]);
```
---
#### `SituationIsMouseButtonDown` / `SituationIsMouseButtonPressed`
Checks if a mouse button is currently held down (a state) or was just pressed (an event).
```c
bool SituationIsMouseButtonDown(int button);
bool SituationIsMouseButtonPressed(int button);
```
**Usage Example:**
```c
if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    // Fire weapon
}
if (SituationIsMouseButtonDown(SIT_MOUSE_BUTTON_RIGHT)) {
    // Aim down sights
}
```

---
#### `SituationIsMouseButtonReleased`
Checks if a mouse button was released this frame (a single-trigger event).
```c
bool SituationIsMouseButtonReleased(int button);
```

---
#### `SituationSetMousePosition`
Sets the mouse cursor position within the window.
```c
void SituationSetMousePosition(int x, int y);
```
---
#### Gamepad Input
---
#### `SituationIsJoystickPresent` / `SituationIsGamepad`
Checks if a joystick/gamepad is connected, and if it has a standard gamepad mapping.
```c
bool SituationIsJoystickPresent(int jid);
bool SituationIsGamepad(int jid);
```

---
#### `SituationGetJoystickName`
Gets the implementation-defined name of a joystick (e.g., "Xbox Controller").
```c
const char* SituationGetJoystickName(int jid);
```
**Usage Example:**
```c
#define GAMEPAD_ID 0
if (SituationIsJoystickPresent(GAMEPAD_ID) && SituationIsGamepad(GAMEPAD_ID)) {
    printf("Gamepad '%s' is ready.\n", SituationGetJoystickName(GAMEPAD_ID));
}
```
---
#### `SituationIsGamepadButtonDown` / `SituationIsGamepadButtonPressed`
Checks if a gamepad button is held down (state) or was just pressed (event).
```c
bool SituationIsGamepadButtonDown(int jid, int button);
bool SituationIsGamepadButtonPressed(int jid, int button);
```
**Usage Example:**
```c
if (SituationIsGamepadButtonPressed(GAMEPAD_ID, SIT_GAMEPAD_BUTTON_A)) {
    // Jump
}
```
---
#### `SituationGetGamepadAxisValue`
Gets the value of a gamepad axis, between -1.0 and 1.0, with deadzone applied.
```c
float SituationGetGamepadAxisValue(int jid, int axis);
```
**Usage Example:**
```c
float move_x = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_LEFT_X);
float move_y = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_LEFT_Y);
player.x += move_x * PLAYER_SPEED * SituationGetFrameTime();
player.y += move_y * PLAYER_SPEED * SituationGetFrameTime();
```

</details>
<details>
<summary><h3>Audio Module</h3></summary>

**Overview:** The Audio module offers a full-featured audio engine capable of loading sounds (`SituationLoadSoundFromFile`) for low-latency playback and streaming longer tracks (`SituationLoadSoundFromStream`) to conserve memory. It supports device management, playback control (volume, pan, pitch), a built-in effects chain (filters, reverb), and custom real-time audio processors.

### Structs and Enums

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

---
#### `SituationAudioFormat`
Describes the format of audio data.
```c
typedef struct SituationAudioFormat {
    int channels;
    int sample_rate;
    int bit_depth;
} SituationAudioFormat;
```
-   `channels`: Number of audio channels (e.g., 1 for mono, 2 for stereo).
-   `sample_rate`: Number of samples per second (e.g., 44100).
-   `bit_depth`: Number of bits per sample (e.g., 16).

---
#### `SituationSound`
An opaque handle to a loaded sound, either fully in memory or streamed.

---
#### `SituationFilterType`
Specifies the type of filter to apply to a sound.
| Type | Description |
|---|---|
| `SIT_FILTER_NONE` | No filter is applied. |
| `SIT_FILTER_LOW_PASS` | Allows low frequencies to pass through. |
| `SIT_FILTER_HIGH_PASS` | Allows high frequencies to pass through. |

#### Functions
### Functions

#### Audio Device Management
---
#### `SituationIsAudioDeviceReady`
Checks if the audio device has been successfully initialized.
```c
bool SituationIsAudioDeviceReady(void);
```

---
#### `SituationGetAudioDevices`
Enumerates all available audio playback devices on the system.
```c
SituationAudioDeviceInfo* SituationGetAudioDevices(int* count);
```
**Usage Example:**
```c
int device_count;
SituationAudioDeviceInfo* devices = SituationGetAudioDevices(&device_count);
for (int i = 0; i < device_count; i++) {
    printf("Device %d: %s %s\n", i, devices[i].name, devices[i].is_default ? "(Default)" : "");
}
// Note: The returned array's memory is managed by the library.
```

---
#### `SituationSetAudioDevice`
Sets the active audio playback device by its ID.
```c
SituationError SituationSetAudioDevice(int device_id);
```

---
#### `SituationSetAudioMasterVolume`
Sets the master volume for the entire audio device, from `0.0` (silent) to `1.0` (full volume).
```c
SituationError SituationSetAudioMasterVolume(float volume);
```

---
#### `SituationSuspendAudioContext` / `SituationResumeAudioContext`
Suspends or resumes the entire audio context, stopping or restarting all sounds.
```c
SituationError SituationSuspendAudioContext(void);
SituationError SituationResumeAudioContext(void);
```
---
#### Sound Loading and Management
---
#### `SituationLoadSoundFromFile` / `SituationUnloadSound`
Loads a sound from a file (WAV, MP3, OGG, FLAC) into memory, and later unloads it.
```c
SituationError SituationLoadSoundFromFile(const char* file_path, bool looping, SituationSound* out_sound);
void SituationUnloadSound(SituationSound* sound);
```
**Usage Example:**
```c
SituationSound jump_sound;
// At init:
SituationLoadSoundFromFile("sounds/jump.wav", false, &jump_sound);
// At shutdown:
SituationUnloadSound(&jump_sound);
```
---
#### `SituationLoadSoundFromStream`
Initializes a sound for playback from a custom, user-defined data stream. Ideal for large music files.
```c
SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound);
```

---
#### `SituationLoadSoundFromMemory`
Loads a sound from a data buffer in memory.
```c
SituationError SituationLoadSoundFromMemory(const char* file_type, const unsigned char* data, int data_size, bool looping, SituationSound* out_sound);
```
---
#### Playback Control
---
#### `SituationPlayLoadedSound` / `SituationStopLoadedSound`
Begins or stops playback of a specific loaded sound.
```c
SituationError SituationPlayLoadedSound(SituationSound* sound);
SituationError SituationStopLoadedSound(SituationSound* sound);
```
**Usage Example:**
```c
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    SituationPlayLoadedSound(&jump_sound);
}
```

---
#### `SituationIsSoundPlaying`
Checks if a specific sound is currently playing.
```c
bool SituationIsSoundPlaying(SituationSound* sound);
```

---
#### `SituationSetSoundVolume`
Sets the volume for a specific, individual sound (`0.0` to `1.0`).
```c
SituationError SituationSetSoundVolume(SituationSound* sound, float volume);
```

---
#### `SituationSetSoundPan`
Sets the stereo panning for a sound (`-1.0` is full left, `0.0` is center, `1.0` is full right).
```c
SituationError SituationSetSoundPan(SituationSound* sound, float pan);
```

---
#### `SituationSetSoundPitch`
Sets the playback pitch for a sound by resampling (`1.0` is normal pitch, `0.5` is one octave lower, `2.0` is one octave higher).
```c
SituationError SituationSetSoundPitch(SituationSound* sound, float pitch);
```
**Usage Example:**
```c
// Make the sound effect's pitch slightly random
float random_pitch = 1.0f + ((rand() % 200) - 100) / 1000.0f; // Range 0.9 to 1.1
SituationSetSoundPitch(&jump_sound, random_pitch);
SituationPlayLoadedSound(&jump_sound);
```
---
#### Effects and Custom Processing
---
#### `SituationSetSoundFilter`
Applies a low-pass or high-pass filter to a sound's effects chain.
```c
SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor);
```
**Usage Example:**
```c
// To simulate sound coming from another room, apply a low-pass filter.
SituationSetSoundFilter(&music, SIT_FILTER_LOW_PASS, 800.0f, 1.0f); // Cut off frequencies above 800 Hz
```

---
#### `SituationSetSoundReverb`
Applies a reverb effect to a sound.
```c
SituationError SituationSetSoundReverb(SituationSound* sound, bool active, float room_size, float damping, float width, float wet_level, float dry_level);
```

---
#### `SituationAttachAudioProcessor`
Attaches a custom DSP processor to a sound's effect chain for real-time processing, like visualization or custom effects.
```c
SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* userData);
```

---
#### `SituationDetachAudioProcessor`
Detaches a custom DSP processor from a sound.
```c
SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor);
```

</details>
<details>
<summary><h3>Filesystem Module</h3></summary>

**Overview:** The Filesystem module provides a robust, cross-platform, and UTF-8 aware API for interacting with the host's file system. It includes functions for checking file/directory existence, reading/writing files, and path manipulation. Use helpers like `SituationGetBasePath()` (for assets) and `SituationGetAppSavePath()` (for user data) for maximum portability.

### Functions

#### Path Management & Special Directories
---
#### `SituationGetBasePath`
Gets the path to the directory containing the executable. Ideal for locating asset files.
```c
char* SituationGetBasePath(void);
```
---
#### `SituationGetAppSavePath`
Gets a safe, persistent path for saving application data (e.g., `%APPDATA%/AppName`).
```c
char* SituationGetAppSavePath(const char* app_name);
```
**Usage Example:**
```c
char* save_path = SituationGetAppSavePath("MyCoolGame");
if (save_path) {
    printf("Saving config to: %s\n", save_path);
    SituationFreeString(save_path);
}
```
---
#### `SituationJoinPath`
Joins two path components with the correct OS separator.
```c
char* SituationJoinPath(const char* base_path, const char* file_or_dir_name);
```
**Usage Example:**
```c
char* base = SituationGetBasePath();
char* texture_path = SituationJoinPath(base, "textures/player.png");
// ... use texture_path ...
SituationFreeString(base);
SituationFreeString(texture_path);
```
---
#### File and Directory Queries
---
#### `SituationGetFileName` / `SituationGetFileExtension`
Extracts the file name or extension from a path.
```c
const char* SituationGetFileName(const char* file_path);
const char* SituationGetFileExtension(const char* file_path);
```
**Usage Example:**
```c
const char* path = "C:/assets/textures/player.png";
printf("File name: %s\n", SituationGetFileName(path)); // -> "player.png"
printf("Extension: %s\n", SituationGetFileExtension(path)); // -> ".png"
```

---
#### `SituationFileExists` / `SituationDirectoryExists`
Checks if a file or directory exists at the given path.
```c
bool SituationFileExists(const char* file_path);
bool SituationDirectoryExists(const char* dir_path);
```

---
#### `SituationGetFileModTime`
Gets the last modification time of a file.
```c
long SituationGetFileModTime(const char* file_path);
```
---
#### File I/O
---
#### `SituationLoadFileText` / `SituationSaveFileText`
Loads or saves a text file as a null-terminated string.
```c
char* SituationLoadFileText(const char* file_path);
bool SituationSaveFileText(const char* file_path, const char* text);
```
**Usage Example:**
```c
const char* settings = "[Graphics]\nwidth=1920\nheight=1080";
SituationSaveFileText("settings.ini", settings);

char* loaded_settings = SituationLoadFileText("settings.ini");
if (loaded_settings) {
    puts(loaded_settings);
    SituationFreeString(loaded_settings);
}
```
---
#### `SituationLoadFileData` / `SituationSaveFileData`
Loads an entire file into a memory buffer or saves a buffer to a file.
```c
unsigned char* SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read);
bool SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write);
```
---
#### Directory Operations
---
#### `SituationCreateDirectory`
Creates a directory, optionally creating all parent directories in the path.
```c
bool SituationCreateDirectory(const char* dir_path, bool create_parents);
```
**Usage Example:**
```c
// Creates "assets", "assets/models", and "assets/models/player" if they don't exist.
SituationCreateDirectory("assets/models/player", true);
```
---
#### `SituationListDirectoryFiles`
Lists files and subdirectories in a path. The returned list must be freed with `SituationFreeDirectoryFileList`.
```c
char** SituationListDirectoryFiles(const char* dir_path, int* out_count);
```
**Usage Example:**
```c
int file_count = 0;
char** files = SituationListDirectoryFiles("assets", &file_count);
for (int i = 0; i < file_count; ++i) {
    printf("Found file: %s\n", files[i]);
}
SituationFreeDirectoryFileList(files, file_count);
```

---
#### `SituationChangeDirectory` / `SituationGetWorkingDirectory`
Changes the application's current working directory or gets the current one.
```c
bool SituationChangeDirectory(const char* dir_path);
char* SituationGetWorkingDirectory(void);
```
**Usage Example:**
```c
char* initial_dir = SituationGetWorkingDirectory();
printf("Started in: %s\n", initial_dir);
if (SituationChangeDirectory("assets")) {
    printf("Changed dir to 'assets'\n");
}
SituationChangeDirectory(initial_dir); // Change back
SituationFreeString(initial_dir);
```
</details>
<details>
<summary><h3>Miscellaneous Module</h3></summary>

**Overview:** This module includes powerful utilities like the Temporal Oscillator System for rhythmic timing, a suite of color space conversion functions (RGBA, HSV, YPQA), and essential memory management helpers for data allocated by the library.

### Functions

#### Temporal Oscillator System
---
#### `SituationTimerHasOscillatorUpdated`
Checks if an oscillator's state has changed this frame. This is a single-frame trigger, ideal for synchronizing events to a beat.
```c
bool SituationTimerHasOscillatorUpdated(int oscillator_id);
```
**Usage Example:**
```c
// In Init, create an oscillator with a 0.5s period (120 BPM)
double period = 0.5;
init_info.oscillator_count = 1;
init_info.oscillator_periods = &period;

// In Update loop
if (SituationTimerHasOscillatorUpdated(0)) {
    printf("Beat!\n");
    // Play a drum sound, flash a light, etc.
}
```
---
#### `SituationSetTimerOscillatorPeriod`
Sets a new period for an oscillator at runtime.
```c
SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds);
```
---
#### `SituationTimerGetOscillatorValue` / `SituationTimerGetOscillatorPhase`
Gets the current value (typically 0.0 to 1.0) or phase (0.0 to 2*PI) of an oscillator.
```c
double SituationTimerGetOscillatorValue(int oscillator_id);
double SituationTimerGetOscillatorPhase(int oscillator_id);
```
**Usage Example:**
```c
// Use an oscillator to create a smooth pulsing animation
double pulse = SituationTimerGetOscillatorValue(0); // This will smoothly go 0 -> 1 -> 0
float scale = 1.0f + (float)pulse * 0.2f;
// Apply 'scale' to a transform
```

---
#### `SituationTimerGetTime`
Gets the total time elapsed in seconds since `SituationInit()` was called.
```c
double SituationTimerGetTime(void);
```
---
#### Color Space Conversions
---
#### `SituationRgbToHsv` / `SituationHsvToRgb`
Converts a color between RGBA and HSV (Hue, Saturation, Value) color spaces.
```c
ColorHSV SituationRgbToHsv(ColorRGBA rgb);
ColorRGBA SituationHsvToRgb(ColorHSV hsv);
```
**Usage Example:**
```c
// Create a rainbow effect by cycling the hue
ColorHSV hsv_color = {.h = fmodf(SituationTimerGetTime() * 50.0f, 360.0f), .s = 1.0, .v = 1.0};
ColorRGBA final_color = SituationHsvToRgb(hsv_color);
```

---
#### `SituationRgbToYpqa` / `SituationYpqaToRgb`
Converts a color between RGBA and the YPQA color space (a custom space for signal processing-like effects).
```c
ColorYPQA SituationRgbToYpqa(ColorRGBA rgb);
ColorRGBA SituationYpqaToRgb(ColorYPQA ypqa);
```
---
#### Memory Management Helpers
---
#### `SituationFreeString`
Frees the memory for a string allocated and returned by the library (e.g., from `SituationGetLastErrorMsg`).
```c
void SituationFreeString(char* str);
```
---
#### `SituationFreeDisplays`
Frees the memory for the array of display information returned by `SituationGetDisplays`.
```c
void SituationFreeDisplays(SituationDisplayInfo* displays, int count);
```
---
#### `SituationFreeDirectoryFileList`
Frees the memory for the list of file paths returned by `SituationListDirectoryFiles`.
```c
void SituationFreeDirectoryFileList(char** files, int count);
```
</details>
