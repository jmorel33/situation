# -- The "Situation" Advanced Platform Awareness, Control, and Timing --
_Core API library v2.3.1_
_(c) 2025 Jacques Morel_
_MIT Licenced_

A single-file, cross-platform C/C++ library providing unified, low-level access and control over essential application subsystems. Its purpose is to abstract away platform-specific complexities, offering a lean yet powerful API for building sophisticated, high-performance software.

The library's philosophy is reflected in its name, granting developers complete situational "Awareness," precise "Control," and fine-grained "Timing."

It provides deep **Awareness** of the host system through APIs for querying hardware and multi-monitor display information, and by handling operating system events like window focus and file drops.

This foundation enables precise **Control** over the entire application stack, from window management (fullscreen, borderless) and input devices (keyboard, mouse, gamepad) to a comprehensive audio pipeline with playback, capture, and real-time effects. This control extends to the graphics and compute pipeline, abstracting modern OpenGL and Vulkan through a unified command-buffer model. It offers simplified management of GPU resources—such as shaders, meshes, and textures—and includes powerful utilities for high-quality text rendering and robust filesystem I/O.

Finally, its **Timing** capabilities range from high-resolution performance measurement and frame rate management to an advanced **Temporal Oscillator System** for creating complex, rhythmically synchronized events. By handling the foundational boilerplate of platform interaction, "Situation" empowers developers to focus on core application logic, enabling the creation of responsive and sophisticated software—from games and creative coding projects to data visualization tools—across all major desktop platforms.

---

## Table of Contents:
- [1.  Introduction & Overview](#1-introduction--overview)
    - [1.1 Purpose & Scope](#11-purpose--scope)
    - [1.2 Key Features & Capabilities](#12-key-features--capabilities)
    - [1.3 Target Audience & Use Cases](#13-target-audience--use-cases)
- [2.  Getting Started](#2-getting-started)
    - [2.1 Integration Models (Header-Only vs. Shared Library)](#21-integration-models-header-only-vs-shared-library)
    - [2.2 Project Structure Recommendations](#22-project-structure-recommendations)
    - [2.3 Compilation Requirements & Dependencies](#23-compilation-requirements--dependencies)
    - [2.4 Basic Initialization & Shutdown Sequence](#24-basic-initialization--shutdown-sequence)
    - [2.5 Core Application Loop Structure](#25-core-application-loop-structure)
- [3.  Core Concepts & Architecture](#3-core-concepts--architecture)
    - [3.1 Single-Threaded Design & Thread Safety](#31-single-threaded-design--thread-safety)
    - [3.2 Backend Abstraction (OpenGL / Vulkan)](#32-backend-abstraction-opengl--vulkan)
    - [3.3 Resource Management Philosophy](#33-resource-management-philosophy)
    - [3.4 Error Handling Strategy](#34-error-handling-strategy)
    - [3.5 Command Buffer Model](#35-command-buffer-model)
    - [3.6 Virtual Displays System](#36-virtual-displays-system)
- [4.  Detailed API Reference](#4-detailed-api-reference)
    - [4.1  Core Lifecycle Management](#41-core-lifecycle-management)
        - [4.1.1  SituationInit](#411-situationinit)
        - [4.1.2  SituationShutdown](#412-situationshutdown)
        - [4.1.3  SituationIsInitialized](#413-situationisinitialized)
        - [4.1.4  Error Handling Functions (SituationGetLastErrorMsg, etc.)](#414-error-handling-functions-situationgetlasterrormsg-etc)
    - [4.2  Window & Display Management](#42-window--display-management)
        - [4.2.1  Configuration (SituationInitInfo)](#421-configuration-situationinitinfo)
        - [4.2.2  State Control (Fullscreen, Borderless, Position, etc.)](#422-state-control-fullscreen-borderless-position-etc)
        - [4.2.3  Display Queries & Multi-Monitor Support](#423-display-queries--multi-monitor-support)
        - [4.2.4  Window Events & Callbacks](#424-window-events--callbacks)
    - [4.3  Input Handling](#43-input-handling)
        - [4.3.1  Keyboard State & Events (SituationGetKey, SituationIsKeyPressed)](#431-keyboard-state--events-situationgetkey-situationiskeypressed)
        - [4.3.2  Mouse State & Events (SituationGetMousePosition, SituationIsMouseButtonPressed)](#432-mouse-state--events-situationgetmouseposition-situationismousebuttonpressed)
        - [4.3.3  Gamepad Support (Joystick API)](#433-gamepad-support-joystick-api)
        - [4.3.4  Character Input & Text Entry](#434-character-input--text-entry)
        - [4.3.5  File Drop Events](#435-file-drop-events)
    - [4.4  Timing & Performance](#44-timing--performance)
        - [4.4.1  Core Frame Timing](#441-core-frame-timing)
        - [4.4.2  The Temporal Oscillator System](#442-the-temporal-oscillator-system)
        - [4.4.3  Frame Delta Time](#443-frame-delta-time)
    - [4.5  Filesystem Utilities](#45-filesystem-utilities)
        - [4.5.1  Path Handling & Standard Directories](#451-path-handling--standard-directories)
        - [4.5.2  File Operations (Load, Exists, Modification Time)](#452-file-operations-load-exists-modification-time)
    - [4.6  Graphics API Abstraction Layer](#46-graphics-api-abstraction-layer)
        - [4.6.1  Shaders](#461-shaders)
        - [4.6.2  Meshes](#462-meshes)
        - [4.6.3  Textures](#463-textures)
        - [4.6.4  Buffers (Vertex, Index, Uniform, Storage)](#464-buffers-vertex-index-uniform-storage)
        - [4.6.5  Virtual Displays (Off-Screen Rendering Layers)](#465-virtual-displays-off-screen-rendering-layers)
        - [4.6.6  Command Buffers](#466-command-buffers)
        - [4.6.7  Render Passes](#467-render-passes)
        - [4.6.8  Dynamic Pipeline State](#468-dynamic-pipeline-state)
        - [4.6.9  Instanced & Indirect Drawing](#469-instanced--indirect-drawing)
    - [4.7  Compute Shaders](#47-compute-shaders)
        - [4.7.1  Overview & Capabilities](#471-overview--capabilities)
        - [4.7.2  Initialization Prerequisites](#472-initialization-prerequisites)
        - [4.7.3  Creating Compute Pipelines](#473-creating-compute-pipelines)
        - [4.7.4  Using Compute Pipelines](#474-using-compute-pipelines)
        - [4.7.5  Synchronization & Memory Barriers](#475-synchronization--memory-barriers)
        - [4.7.6  Destroying Compute Pipelines (SituationDestroyComputePipeline)](#476-destroying-compute-pipelines-situationdestroycomputepipeline)
    - [4.8  Audio System](#48-audio-system)
        - [4.8.1  Device Enumeration (SituationGetAudioDevices)](#481-device-enumeration-situationgetaudiodevices)
        - [4.8.2  Audio Playback](#482-audio-playback)
        - [4.8.3  Audio Capture (Recording)](#483-audio-capture-recording)
        - [4.8.4  Destroying Sounds (SituationDestroySound)](#484-destroying-sounds-situationdestroysound)
        - [4.8.5  Audio Effects & Custom Processing](#485-audio-effects--custom-processing)
    - [4.9  Text Rendering](#49-text-rendering)
        - [4.9.1  Simple Text Drawing (SituationDrawTextSimple)](#491-simple-text-drawing-situationdrawtextsimple)
        - [4.9.2  Styled Text Rendering (SituationDrawTextStyled)](#492-styled-text-rendering-situationdrawtextstyled)
        - [4.9.3  Font Loading & Management](#493-font-loading--management)
    - [4.10 2D Rendering & Drawing](#410-2d-rendering--drawing)
        - [4.10.1  2D Coordinate System & Camera](#4101-2d-coordinate-system--camera)
        - [4.10.2  Drawing Basic Shapes](#4102-drawing-basic-shapes)
        - [4.10.3  Drawing Textures (Sprites)](#4103-drawing-textures-sprites)
        - [4.10.4  Text Rendering](#4104-text-rendering)
        - [4.10.5  UI & Layer Management](#4105-ui--layer-management)
- [5.  Building & Configuration](#5-building--configuration)
    - [5.1  Backend Selection Defines (SITUATION_USE_OPENGL, SITUATION_USE_VULKAN)](#51-backend-selection-defines-situation_use_opengl-situation_use_vulkan)
    - [5.2  Feature Enablement Defines (SITUATION_ENABLE_SHADER_COMPILER, etc.)](#52-feature-enablement-defines-situation_enable_shader_compiler-etc)
    - [5.3  Shared Library Support (SITUATION_BUILD_SHARED, SITUATION_USE_SHARED)](#53-shared-library-support-situation_build_shared-situation_use_shared)
    - [5.4  Compiler & Linker Flags](#54-compiler--linker-flags)
- [6.  Examples & Tutorials](#6-examples--tutorials)
    - [6.1  Basic Triangle Rendering](#61-basic-triangle-rendering)
    - [6.2  Loading and Rendering a 3D Model](#62-loading-and-rendering-a-3d-model)
    - [6.3  Playing Background Music](#63-playing-background-music)
    - [6.4  Handling Keyboard and Mouse Input](#64-handling-keyboard-and-mouse-input)
    - [6.5  Compute Shader Example: Image Processing](#65-compute-shader-example-image-processing)
        - [6.5.1 Problem Definition (e.g., Grayscale Filter)](#651-problem-definition-eg-grayscale-filter)
        - [6.5.2 Shader Code (GLSL Compute)](#652-shader-code-glsl-compute)
        - [6.5.3 Host Code Walkthrough (Init, Create, Bind, Dispatch, Sync, Destroy)](#653-host-code-walkthrough-init-create-bind-dispatch-sync-destroy)
    - [6.6  GPU Particle Simulation and Rendering (Concept)](#66-gpu-particle-simulation-and-rendering-concept)
        - [6.6.1 Scenario](#661-scenario)
        - [6.6.2 Key APIs Demonstrated](#662-key-apis-demonstrated)
        - [6.6.3 Purpose](#663-purpose)
- [7.  Frequently Asked Questions (FAQ) & Troubleshooting](#7-frequently-asked-questions-faq--troubleshooting)
    - [7.1  Common Initialization Failures](#71-common-initialization-failures)
    - [7.2  "Resource Invalid" Errors](#72-resource-invalid-errors)
    - [7.3  Performance Considerations](#73-performance-considerations)
    - [7.4  Backend-Specific Issues (OpenGL vs. Vulkan)](#74-backend-specific-issues-opengl-vs-vulkan)
    - [7.5  Debugging Tips (Validation Layers, Error Messages)](#75-debugging-tips-validation-layers-error-messages)

---
<details>
<summary><h2>1. Introduction & Overview</h2></summary>

### 1.1 Purpose & Scope
`situation.h` is a single-header C library designed to provide a robust, cross-platform foundation for developing graphical applications and games. It abstracts the complexities of underlying system APIs like windowing (GLFW), graphics (OpenGL/Vulkan), and audio (miniaudio) into a cohesive and simplified interface. Its primary goal is to offer developers "situational awareness" of the platform and precise control over core application subsystems.

### 1.2 Key Features & Capabilities
- **Cross-Platform Windowing & Input (via GLFW):** Create and manage windows, handle keyboard, mouse, and gamepad input, process file drops.
- **Dual Graphics Backend Support:** Seamless abstraction over modern OpenGL (3.3+ Core) and Vulkan (1.0+) for rendering.
- **Compute Shader Support:** Unified API for leveraging GPU compute power (OpenGL Compute Shaders, Vulkan Compute Pipelines).
- **Comprehensive Resource Management:** Simplified loading, usage, and destruction of Shaders, Meshes, Textures, Buffers, and Compute Pipelines.
- **Advanced Rendering Features:**
    - **Virtual Display System:** Off-screen rendering layers for UI, post-processing, or scene composition.
    - **Text Rendering:** Both simple character-by-character and high-performance styled text using Signed Distance Fields (SDF).
    - **Command Buffer Abstraction:** Records rendering and compute commands for submission.
- **Audio System:** Playback and recording capabilities using the miniaudio library.
- **Utility Functions:** High-resolution timers, FPS calculation, filesystem path utilities, argument parsing.
- **Single-Threaded Architecture:** Ensures simplicity and avoids complex multi-threading pitfalls within the library itself.

### 1.3 Target Audience & Use Cases
This library is intended for C/C++ developers building cross-platform graphical applications, tools, demos, or lightweight games. It is particularly useful for those who want to avoid the boilerplate of setting up windowing, graphics contexts, and basic I/O but still need control and performance. It serves as an excellent foundation for custom engines or rapid prototyping.
</details>

---
<details>
<summary><h2>2. Getting Started</h2></summary>

### 2.1 Integration Models (Header-Only vs. Shared Library)
**A) Header-Only:**
- Add `situation.h` to your project.
- In *one* C/C++ source file (e.g., `sit_lib.c`), define `SITUATION_IMPLEMENTATION` *before* including `situation.h`.
```c
#define SITUATION_IMPLEMENTATION
#include "situation.h"
```
- Compile this source file with your project.

**B) Shared Library (DLL):**
- Create a separate source file (e.g., `sit_dll.c`).
- Define `SITUATION_IMPLEMENTATION` and `SITUATION_BUILD_SHARED`.
```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_BUILD_SHARED
#include "situation.h"
```
- Compile this into a shared library (DLL/.so).
- In your main application, define `SITUATION_USE_SHARED` and include `situation.h`.
```c
#define SITUATION_USE_SHARED
#include "situation.h"
```
- Link your application against the generated library.

### 2.2 Project Structure Recommendations
```
your_project/
├── src/
│   ├── main.c              // Your application entry point
│   └── (other .c files)    // Your application logic
├── lib/
│   └── situation.h         // This library header
├── ext/                    // External dependencies (if not system-installed)
│   ├── glad/               // For OpenGL (if SITUATION_USE_OPENGL)
│   │   ├── glad.c
│   │   └── glad.h
│   ├── cglm/               // For math (if used)
│   │   └── ...             // cglm headers
│   ├── stb/                // For image loading (stb_image.h, etc.)
│   │   └── ...             // stb headers (define STB_IMAGE_IMPLEMENTATION in one .c file)
│   └── miniaudio/          // Audio library (miniaudio.h)
│       └── miniaudio.h     // (define MINIAUDIO_IMPLEMENTATION in one .c file)
├── assets/                 // Your application's assets
│   ├── models/
│   │   └── cube.obj
│   ├── textures/
│   │   └── diffuse.png
│   ├── shaders/
│   │   ├── basic.vert
│   │   ├── basic.frag
│   │   └── compute_filter.comp
│   └── audio/
│       └── background_music.wav
└── build/                  // Build output directory
```

### 2.3 Compilation Requirements & Dependencies
- A C99 or C++ compiler.
- **Required Dependencies (provided or system-installed):**
    - **GLFW3:** For windowing and input. Headers and library linking required.
    - **OpenGL Context Loader (e.g., GLAD):** If using `SITUATION_USE_OPENGL`. `glad.c` must be compiled.
    - **Vulkan SDK:** If using `SITUATION_USE_VULKAN`. Headers and linking required. Includes shaderc, VMA.
    - **cglm:** For math types and functions (vec3, mat4, etc.). Headers needed.
- **Optional Dependencies (for extra features):**
    - **stb_image.h, stb_image_write.h, stb_image_resize.h:** For image loading/saving/resizing. Define `STB_IMAGE_IMPLEMENTATION` etc. in one .c file.
    - **stb_truetype.h:** For styled text rendering (SDF generation). Define `STB_TRUETYPE_IMPLEMENTATION`.
    - **miniaudio.h:** For audio. Define `MINIAUDIO_IMPLEMENTATION` in one .c file.

### 2.4 Basic Initialization & Shutdown Sequence
```c
#include "situation.h"
#include <stdio.h>
#include <stdbool.h> // For bool

int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0}; // Zero-initialize for defaults
    init_info.window_title = "My Application v1.0";
    init_info.window_width = 1280;
    init_info.window_height = 720;
    // init_info.flags = ...; // Set optional window flags if needed
    // init_info.enable_vulkan_validation = true; // Enable for Vulkan debugging

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s (Error Code: %d)\n",
                SituationGetLastErrorMsg(), err);
        return -1; // Or handle error appropriately
    }
    printf("situation.h initialized successfully.\n");

    // --- Application Main Loop ---
    // (See Section 2.5)

    // --- Cleanup ---
    SituationShutdown(); // Cleanly shuts down all subsystems
    printf("situation.h shutdown complete.\n");
    return 0;
}
```

### 2.5 Core Application Loop Structure
The core of a "Situation" application is a `while` loop that continues as long as the main window is open. Each iteration of this loop represents a single frame and should follow a clear, ordered sequence of operations: Input, Update, and Render.
```c
while (!SituationWindowShouldClose()) {
    // 1. GATHER INPUT: Poll for OS events.
    // This is the first and most critical step. It processes all pending events from the
    // operating system (keyboard, mouse, window resize, etc.) and updates the library's
    // internal state for the current frame. All input queries (`SituationIsKeyPressed`, etc.)
    // depend on this being called.
    SituationPollInputEvents();

    // 2. UPDATE TIMERS & LOGIC: Calculate delta time and run application logic.
    // This step advances the library's internal clocks.
    SituationUpdateTimers();

    // Get the time elapsed since the last frame. This is essential for frame-rate
    // independent movement and physics.
    float delta_time = SituationGetFrameTime();

    // Now, run your application's logic using the fresh input state and delta time.
    // UpdatePlayer(delta_time);
    // UpdatePhysics(delta_time);

    // 3. RENDER: Record and submit all drawing commands for the frame.
    // This entire block handles the interaction with the GPU.

    // a. Prepare for rendering. This returns false on Vulkan if the window was
    //    resized, in which case we should skip rendering for this frame.
    if (SituationAcquireFrameCommandBuffer()) {
        // b. Get the main command buffer for recording. (Returns NULL for OpenGL).
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

        // c. Begin a render pass on the main window, clearing it to a color.
        SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });

        // --- Record all your rendering commands here ---
        // Example: Draw a mesh with a specific pipeline.
        // SituationCmdBindPipeline(cmd, my_graphics_pipeline);
        // SituationCmdDrawMesh(cmd, my_mesh);
        //
        // Example: Draw a 2D quad.
        // SituationCmdDrawQuad(cmd, model_matrix, color);
        // --- End of rendering commands ---

        // d. End the render pass.
        SituationCmdEndRender(cmd);

        // e. Submit all recorded commands to the GPU and present the frame.
        SituationEndFrame();
    }
}
```
</details>

---
<details>
<summary><h2>3. Core Concepts & Architecture</h2></summary>

### 3.1 Single-Threaded Design & Thread Safety
`situation.h` is explicitly designed as a **single-threaded library**. This architectural choice is fundamental to its API design and usage.

- **The Golden Rule:** All public `SITAPI` functions **must** be called from the same thread that executed `SituationInit()`. The only exceptions are internal, real-time callbacks (e.g., the audio data callback), which are handled in a thread-safe manner by the library itself.
- **Consequences of Violation:** Calling API functions from other threads will lead to race conditions, internal state corruption, and almost certainly result in crashes or other undefined behavior. The library does not contain internal synchronization primitives (like mutexes) to protect its global state from concurrent access.
- **Developer Responsibility:** This design simplifies the library's implementation and eliminates the performance overhead of locking. However, it places the full responsibility of thread management on the developer. If your application requires multi-threading (e.g., for background asset loading or physics simulation), you must design a thread-safe queuing system to communicate with the main thread. The worker threads must never call `SITAPI` functions directly. For example, a worker thread can load a texture file into a CPU memory buffer, but only the main thread can call `SituationCreateTexture()` to upload that data to the GPU.

### 3.2 Backend Abstraction (OpenGL / Vulkan)
The library provides a unified API that works seamlessly over two distinct graphics backends. The choice is made at **compile time** by defining one of the following preprocessor macros before including `situation.h`:

- **`SITUATION_USE_OPENGL`:** Enables the modern OpenGL backend (Core Profile 4.6+). This backend is generally easier to set up but relies on the quality of the system's graphics drivers to manage resources and execute commands. The command buffer API is an *emulation* on top of OpenGL's immediate-mode nature (see Section 3.5).
- **`SITUATION_USE_VULKAN`:** Enables the explicit Vulkan backend (1.1+). This backend offers more direct control over the GPU, potentially leading to higher and more consistent performance. It requires the Vulkan SDK to be installed. The library's command buffer API is a direct and natural mapping to Vulkan's core architecture.

This compile-time approach eliminates any runtime branching or overhead associated with backend selection. The abstraction layer is responsible for translating the unified `SITAPI` calls into the correct, backend-specific function calls and resource management strategies.

### 3.3 Resource Management Philosophy
The library adopts a C-style, **explicit manual resource management** philosophy. This gives the developer precise control over the lifetime of all GPU and CPU resources.

- **Creation and Destruction:** Every resource created with a `SituationCreate*` or `SituationLoad*` function (e.g., `SituationCreateMesh`, `SituationLoadShader`) returns a handle. It is the **user's absolute responsibility** to destroy that resource by calling its corresponding `SituationDestroy*` or `SituationUnload*` function when it is no longer needed.
- **Handles:** Resources are represented by handles (e.g., `SituationMesh`, `SituationShader`). A valid, live resource will have a non-zero `id` member. After destruction, the handle's `id` is set to 0 to invalidate it and prevent accidental reuse.
- **Leak Detection:** To aid in debugging, the library performs automatic leak detection during the `SituationShutdown()` process. If any resources have not been manually destroyed by the user, a warning message will be printed to `stderr` for each leaked resource. This is a safety net, not a substitute for proper resource management.
- **Memory Ownership:** Functions that return pointers to dynamically allocated memory (e.g., `SituationGetLastErrorMsg()`, `SituationLoadDroppedFiles()`) explicitly state that the **caller owns the memory** and is responsible for freeing it with `free()` or a designated `Unload` function. Failure to do so will result in memory leaks.

### 3.4 Error Handling Strategy
Functions that can fail typically return a `SituationError` enum or a handle struct where the `id` field is 0 to indicate failure. A detailed error message can always be retrieved by calling `SituationGetLastErrorMsg()`. This message is updated whenever an error occurs within the library. Always check return values, especially for resource creation and initialization functions.

### 3.5 Command Buffer Model
Rendering and compute operations are recorded into `SituationCommandBuffer` objects. This model, inspired by modern APIs like Vulkan, allows for batching commands to improve performance and provides explicit control over the rendering process.

- Obtain a command buffer using `SituationAcquireFrameCommandBuffer()` (for the main window) or within a virtual display render pass.
- Record a sequence of commands (draw calls, dispatches, state changes, barriers) using `SituationCmd*` functions.
- Submit the recorded commands using `SituationEndFrame()`. This triggers execution by the GPU.

**[CRITICAL] Backend Execution Differences:**
While the API is unified, the underlying execution timing differs significantly between backends. This is a crucial detail to understand:

- **Vulkan (Deferred Execution):** On the Vulkan backend, `SituationCmd*` functions operate as expected. They are lightweight calls that simply record commands into a buffer. No significant GPU work happens until `SituationEndFrame()` is called, which submits the entire batch to the GPU for execution.
- **OpenGL (Immediate Emulation):** On the OpenGL backend, the command buffer model is an *emulation*. Each `SituationCmd*` call (e.g., `SituationCmdDrawMesh`) typically translates directly to an immediate OpenGL call (e.g., `glDrawElements`). The GPU may begin executing these commands as soon as they are issued. The `SituationCommandBuffer` parameter is often ignored.

**Implication:** You must not write code that depends on the deferred execution of commands when using the OpenGL backend. For example, do not expect to be able to read back the results of a compute shader immediately after a `SituationCmdDispatch` call within the same frame without manual, backend-specific synchronization (like `glFinish()`).

### 3.6 Virtual Displays System
Virtual Displays (VDs) are off-screen rendering targets. They allow rendering scenes or UI elements to a texture, which can then be composited onto the main window or other VDs. This is useful for UI layers, post-processing effects, or rendering multiple views. VDs have properties like resolution, position, opacity, and z-order for compositing.
</details>

---
<details>
<summary><h2>4. Detailed API Reference</h2></summary>

This section provides a comprehensive reference for every major function and concept in the "Situation" library. Each entry includes the function's purpose, signature, parameter descriptions, return values, and important usage notes.

### 4.1 Core Lifecycle Management
These functions control the fundamental state of the entire library, from initialization to shutdown.

#### 4.1.1 SituationInit
Initializes all library subsystems, creates the main application window, and sets up the chosen graphics backend. This is the first function that must be called.
- **Signature:** `SITAPI SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info);`
- **Parameters:**
    - `argc`: The argument count from your `main` function.
    - `argv`: The argument vector from your `main` function.
    - `init_info`: A pointer to a `SituationInitInfo` struct containing configuration for the window and backends.
- **Return Value:** Returns `SITUATION_SUCCESS` on successful initialization, or an appropriate `SituationError` code on failure.
- **Notes:** Must be called from the main thread before any other library functions. On failure, the library performs a full cleanup.

#### 4.1.2 SituationShutdown
Shuts down all subsystems, releases all tracked GPU and CPU resources, closes the window, and terminates the graphics context.
- **Signature:** `SITAPI void SituationShutdown(void);`
- **Notes:** This function should be the last library call your application makes. It will print warnings to `stderr` for any resources (meshes, shaders, etc.) that were not manually destroyed by the user.

#### 4.1.3 SituationIsInitialized
Checks if the library has been successfully initialized.
- **Signature:** `SITAPI bool SituationIsInitialized(void);`
- **Return Value:** Returns `true` if `SituationInit` completed successfully, `false` otherwise. Safe to call at any time.

#### 4.1.4 Error Handling Functions (SituationGetLastErrorMsg, etc.)
Retrieves detailed information about the last error that occurred within the library.
- **Signature:** `SITAPI char* SituationGetLastErrorMsg(void);`
- **Return Value:** A dynamically allocated, null-terminated string containing a human-readable error message. Returns `NULL` if no error has occurred or if memory allocation fails.
- **Warning:** The returned string is a *copy*. The caller is **responsible for freeing this memory** using `free()` to prevent memory leaks.

---
### 4.2  Window & Display Management
Functions for creating, configuring, and querying the state of the application window and physical display monitors.

#### 4.2.1 Configuration (SituationInitInfo)
The `SituationInitInfo` struct, passed to `SituationInit`, configures the initial state of the window and backend.
- **Key Members:**
    - `window_width`, `window_height`: The initial dimensions of the window's client area.
    - `window_title`: The text that will appear in the window's title bar.
    - `initial_active_window_flags`: A bitmask of `SITUATION_FLAG_*` defines (e.g., `SITUATION_FLAG_VSYNC_HINT`, `SITUATION_FLAG_WINDOW_RESIZABLE`) to apply when the window is created.
    - `enable_vulkan_validation`: (Vulkan only) Set to `true` to enable Vulkan's validation layers for detailed debugging.

#### 4.2.2 State Control (Fullscreen, Borderless, Position, etc.)
These functions query and modify the window's state at runtime.
- `SITAPI void SituationToggleFullscreen(void);`: Toggles the window between windowed and exclusive fullscreen mode.
- `SITAPI void SituationToggleBorderlessWindowed(void);`: Toggles the window between a standard decorated and a borderless style.
- `SITAPI void SituationMaximizeWindow(void);`: Maximizes the window.
- `SITAPI void SituationMinimizeWindow(void);`: Minimizes (iconifies) the window.
- `SITAPI void SituationRestoreWindow(void);`: Restores a minimized or maximized window.
- `SITAPI void SituationSetWindowPosition(int x, int y);`: Sets the top-left corner of the window on the desktop.
- `SITAPI void SituationSetWindowSize(int width, int height);`: Sets the dimensions of the window's client area.
- `SITAPI bool SituationIsWindowFullscreen(void);`: Returns `true` if the window is in exclusive fullscreen mode.
- `SITAPI bool SituationIsWindowResized(void);`: Returns `true` if the window was resized in the last frame.

#### 4.2.3 Display Queries & Multi-Monitor Support
Functions for getting information about connected physical displays.
- `SITAPI int SituationGetMonitorCount(void);`: Returns the number of connected monitors.
- `SITAPI const char* SituationGetMonitorName(int monitor_id);`: Returns the human-readable name of the specified monitor.
- `SITAPI SituationDisplayInfo* SituationGetDisplays(int* count);`: Returns a dynamically allocated array of `SituationDisplayInfo` structs for all connected monitors.
  - **Warning:** The caller is **responsible for freeing the returned memory**. You must first loop through the array and `free()` the `available_modes` pointer for each display, and then `free()` the top-level array itself.

#### 4.2.4 Window Events & Callbacks
Functions for setting callbacks to respond to window events asynchronously.
- `SITAPI void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data);`: Sets a function to be called whenever the window's framebuffer is resized.
- `SITAPI void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data);`: Sets a function to be called when the window gains or loses input focus.

---
### 4.3  Input Handling
A unified API for polling and receiving events from keyboards, mice, and gamepads.

#### 4.3.1 Keyboard State & Events
- `SITAPI bool SituationIsKeyDown(int key);`: Checks if a key is *currently* held down (state).
- `SITAPI bool SituationIsKeyUp(int key);`: Checks if a key is currently up (not pressed).
- `SITAPI bool SituationIsKeyPressed(int key);`: Checks if a key was pressed *this frame* (event).
- `SITAPI bool SituationIsKeyReleased(int key);`: Checks if a key was released *this frame* (event).
- `SITAPI void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data);`: Sets a callback for all key events.

#### 4.3.2 Mouse State & Events
- `SITAPI Vector2 SituationGetMousePosition(void);`: Gets the mouse cursor position relative to the top-left corner of the window.
- `SITAPI Vector2 SituationGetMouseDelta(void);`: Gets the mouse movement since the last frame.
- `SITAPI bool SituationIsMouseButtonDown(int button);`: Checks if a mouse button is *currently* held down (state).
- `SITAPI bool SituationIsMouseButtonPressed(int button);`: Checks if a mouse button was pressed *this frame* (event).

#### 4.3.3 Gamepad Support (Joystick API)
- `SITAPI bool SituationIsJoystickPresent(int jid);`: Checks if a joystick/gamepad is connected at the given index (0-15).
- `SITAPI bool SituationIsGamepad(int jid);`: Checks if the connected joystick has a standard gamepad mapping.
- `SITAPI bool SituationIsGamepadButtonPressed(int jid, int button);`: Checks if a gamepad button was pressed *this frame*.
- `SITAPI float SituationGetGamepadAxisValue(int jid, int axis);`: Gets the value of a gamepad axis (e.g., `SITUATION_GAMEPAD_AXIS_LEFT_X`), with deadzone applied.

#### 4.3.4 Character Input & Text Entry
- `SITAPI unsigned int SituationGetCharPressed(void);`: Gets the next Unicode character from the input queue. Use this for text entry fields. Returns `0` if the queue is empty.

#### 4.3.5 File Drop Events
- `SITAPI bool SituationIsFileDropped(void);`: Returns `true` if files were dropped onto the window during the last frame.
- `SITAPI char** SituationLoadDroppedFiles(int* count);`: Retrieves the paths of dropped files.
  - **Warning:** Returns a dynamically allocated array. The caller is **responsible for freeing this memory** by calling `SituationUnloadDroppedFiles()`.
- `SITAPI void SituationUnloadDroppedFiles(char** paths, int count);`: Frees the memory allocated by `SituationLoadDroppedFiles`.

---
### 4.4  Timing & Performance
"Situation" provides a multi-faceted timing system designed for everything from simple frame-rate independent logic to complex, musically-timed event sequencing. It is built on a high-resolution monotonic clock, ensuring accuracy and stability.

#### 4.4.1  Core Frame Timing
These functions provide the essential timing information needed for the main application loop. The library updates these values once per frame during the `SituationUpdateTimers()` call.

##### 4.4.1.1  High-Resolution Timer (SituationTimerGetTime)
- **Signature:** `SITAPI double SituationTimerGetTime(void);`
- Returns the total time elapsed in seconds since `SituationInit` was called, as a high-precision `double`. This is the master clock for the entire application.

##### 4.4.1.2  Frame Delta Time (SituationGetFrameTime)
- **Signature:** `SITAPI float SituationGetFrameTime(void);`
- Returns the time in seconds that the previous frame took to complete. This is the "delta time" (`dt`) value and is crucial for creating frame-rate independent logic.
- **Example Usage:** `player.position.x += player.speed * SituationGetFrameTime();`

##### 4.4.1.3  Frame Rate Management
- `SITAPI int SituationGetFPS(void);`: Returns the current calculated frames per second. This value is an average and is typically updated once per second.
- `SITAPI void SituationSetTargetFPS(int fps);`: Sets a desired frame rate cap. The library will attempt to sleep for the appropriate amount of time in `SituationEndFrame()` to avoid exceeding this rate. Pass `0` to uncap the frame rate. This is independent of VSync.

#### 4.4.2  The Temporal Oscillator System
This is an advanced, built-in system designed to give your application a heartbeat. It consists of 256 internal, synchronized timers, or "oscillators," that function like a complex set of natural biorhythms.

Instead of just providing one-shot timers, this system creates a cascade of perfectly synchronized, repeating signals at different frequencies. Think of it less like a stopwatch and more like a collection of pendulums of different lengths, all swinging in perfect harmony. By querying the state and rhythm of these oscillators, you can create procedural animations, drive on-beat musical events, or orchestrate complex gameplay mechanics that feel organic and alive. This system is the key to unlocking intricate and musically-timed event sequencing.

##### 4.4.2.1  Concept: Oscillators as Clocks
Each of the 256 oscillators has a defined `period` (in seconds). The system tracks the master time and flips the binary state (0 or 1) of each oscillator every time its period elapses. This creates a cascade of perfectly synchronized square-wave signals at different frequencies, which can be queried to drive events. The initial periods of the first 64 oscillators are pre-configured in a musically relevant, logarithmic-like grid, providing a wide range of useful frequencies out-of-the-box.

##### 4.4.2.2  Querying Oscillator State
You can query an oscillator's state in several ways to trigger logic.
- `SITAPI bool SituationTimerGetOscillatorState(int oscillator_id);`: Returns the current binary state (`true` for 1, `false` for 0) of the specified oscillator (0-255).
- `SITAPI bool SituationTimerGetPreviousOscillatorState(int oscillator_id);`: Returns the state of the oscillator from the *previous* frame.
- `SITAPI bool SituationTimerHasOscillatorUpdated(int oscillator_id);`: This is often the most useful function. It returns `true` only on the exact frame that the oscillator's state flipped (e.g., from 0 to 1 or 1 to 0). This is ideal for triggering "on-beat" events.
- `SITAPI uint64_t SituationTimerGetOscillatorTriggerCount(int oscillator_id);`: Returns the total number of times the oscillator has flipped its state since the application started.

##### 4.4.2.3  "Pinging" an Oscillator
Sometimes, you need to know if a period has elapsed since the *last time you checked*, rather than relying on the fixed state flips. This is useful for cooldowns or event polling.
- `SITAPI bool SituationTimerPingOscillator(int oscillator_id);`: Returns `true` if the oscillator's period has elapsed since the last time this *specific function* was called for this oscillator ID. It then resets its internal "ping" timer for that oscillator.

##### 4.4.2.4  Customizing Oscillator Periods
You can override the default period of any oscillator at runtime.
- `SITAPI double SituationTimerGetOscillatorPeriod(int oscillator_id);`: Returns the current period of an oscillator in seconds.
- `SITAPI SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds);`: Sets a new period for an oscillator. The change takes effect immediately, rescheduling the next trigger event.

##### 4.4.2.5  Example Use Case: On-Beat Animation
```c
// In your update loop:
// Use a fast oscillator for a pulsing effect.
if (SituationTimerHasOscillatorUpdated(10)) {
    // This block executes precisely on the beat.
    // Check the new state to alternate colors.
    if (SituationTimerGetOscillatorState(10)) {
        my_object.color = COLOR_PULSE_A;
    } else {
        my_object.color = COLOR_PULSE_B;
    }
}

// Use a slower oscillator for a major event.
if (SituationTimerHasOscillatorUpdated(32)) {
    // Trigger a larger animation or sound effect every few seconds.
    PlayMajorSoundEffect();
}
```

#### 4.4.3 Frame Delta Time
- **Signature:** `SITAPI float SituationGetFrameTime(void);`
- **Return Value:** The time in seconds that the previous frame took to complete ("delta time"). Essential for frame-rate independent logic.

---
### 4.5  Filesystem Utilities
"Situation" provides a robust, cross-platform API for interacting with the host filesystem. It handles platform-specific details like path separators and standard directory locations, offering a unified interface for file and directory manipulation.

#### 4.5.1  Path Handling & Standard Directories
These functions help construct and deconstruct file paths in a way that is safe across Windows, macOS, and Linux.

##### 4.5.1.1  Getting the Executable Path
- `SITAPI char* SituationGetBasePath(void);`: Retrieves the absolute path to the directory containing the running executable. This is ideal for locating application assets that are bundled with the executable.

##### 4.5.1.2  Getting the Application Save Path
- `SITAPI char* SituationGetAppSavePath(const char* app_name);`: Gets a safe, persistent path for saving user data like settings or save files.
- **Platform Behavior:**
    - **Windows:** Returns a path inside `%APPDATA%\Roaming\<app_name>\`.
    - **Linux:** Follows the XDG Base Directory Spec, returning `$XDG_DATA_HOME/<app_name>/` or `~/.local/share/<app_name>/`.
    - **macOS:** Returns `~/Library/Application Support/<app_name>/`.
- **Note:** The function will attempt to create the directory if it does not already exist.

##### 4.5.1.3  Path Manipulation
- `SITAPI char* SituationJoinPath(const char* base_path, const char* file_or_dir_name);`: Safely joins two path components with the correct OS-specific separator (`\` or `/`).
- `SITAPI const char* SituationGetFileName(const char* full_path);`: Extracts just the file name (including extension) from a full path string.
- `SITAPI const char* SituationGetFileExtension(const char* file_path);`: Extracts the file extension (including the '.') from a path string. Returns `NULL` if no extension is found.

##### 4.5.1.4  Memory Management
- **Warning:** Functions returning `char*` (like `GetBasePath`, `GetAppSavePath`, `JoinPath`) allocate new memory. The caller is **responsible for freeing this memory** using `free()`. Functions returning `const char*` return a pointer into the original string and do not need to be freed.

#### 4.5.2  File Operations
These functions handle querying file metadata and reading/writing file contents.

##### 4.5.2.1  File & Directory Queries
- `SITAPI bool SituationFileExists(const char* file_path);`: Checks if a path exists and points to a regular file.
- `SITAPI bool SituationDirectoryExists(const char* dir_path);`: Checks if a path exists and points to a directory.
- `SITAPI long SituationGetFileModTime(const char* file_path);`: Gets the last modification time of a file as a Unix timestamp (seconds since epoch). Returns `0` on failure.

##### 4.5.2.2  Loading & Saving File Data
- `SITAPI unsigned char* SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read);`: Loads an entire binary file into a memory buffer.
- `SITAPI bool SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write);`: Saves a block of memory to a binary file, overwriting it if it exists.
- `SITAPI char* SituationLoadFileText(const char* file_path);`: A convenience helper that loads a file and ensures the resulting buffer is null-terminated, making it safe to use as a C string.
- `SITAPI bool SituationSaveFileText(const char* file_path, const char* text);`: Saves a null-terminated string to a text file.
- **Warning:** `SituationLoadFileData` and `SituationLoadFileText` return allocated memory that the caller **must `free()`**.

##### 4.5.2.3  File & Directory Manipulation
- `SITAPI bool SituationCopyFile(const char* source_path, const char* dest_path);`: Copies a file. Overwrites the destination if it exists.
- `SITAPI bool SituationDeleteFile(const char* file_path);`: Deletes a file.
- `SITAPI bool SituationMoveFile(const char* old_path, const char* new_path);`: Moves or renames a file. Can move across different drives on Windows.
- `SITAPI bool SituationCreateDirectory(const char* dir_path, bool create_parents);`: Creates a new directory. If `create_parents` is `true`, it will create all necessary parent directories in the path.
- `SITAPI bool SituationDeleteDirectory(const char* dir_path, bool recursive);`: Deletes a directory. If `recursive` is `true`, it will delete all files and subdirectories within it first. **Use with caution.**

##### 4.5.2.4  Listing Directory Contents
- `SITAPI char** SituationListDirectoryFiles(const char* dir_path, int* out_count);`: Returns a list of all file and subdirectory names within a given directory.
- `SITAPI void SituationFreeDirectoryFileList(char** file_list, int count);`: Frees the memory allocated by `SituationListDirectoryFiles`.
- **Warning:** The array returned by `SituationListDirectoryFiles` is dynamically allocated. The caller is **responsible for freeing this memory** by passing the returned pointer and count to `SituationFreeDirectoryFileList`.

---
### 4.6  Graphics API Abstraction Layer
This section details the core of the rendering engine. It provides an abstracted API for creating and managing all necessary GPU resources and for recording commands to be executed by the graphics hardware.

#### 4.6.1  Shaders
Shaders are user-written programs that run on the GPU to control the rendering process. "Situation" distinguishes between two types of pipelines:
- **Graphics Pipeline:** Consists of a vertex shader (processes vertices) and a fragment shader (processes pixels). Used for all visual rendering.
- **Compute Pipeline:** Consists of a single compute shader for general-purpose GPU programming.

##### 4.6.1.1  Creating Graphics Pipelines
- `SITAPI SituationShader SituationLoadShader(const char* vs_path, const char* fs_path);`: A high-level helper that reads GLSL source code from two files and passes them to `SituationLoadShaderFromMemory`. This is the recommended way to load shaders from disk.
- `SITAPI SituationShader SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code);`: The core creation function. It compiles the provided in-memory GLSL strings and creates a complete GPU pipeline object.
- **Return Value:** Both functions return a `SituationShader` handle. On success, `handle.id` will be non-zero. On failure (e.g., file not found, compilation error), `handle.id` will be 0. Use `SituationGetLastErrorMsg()` for details.

##### 4.6.1.2  Creating Compute Pipelines
- `SITAPI SituationComputePipeline SituationCreateComputePipeline(const char* compute_shader_path);`: A high-level helper that reads a GLSL compute shader from a file and creates a pipeline.
- `SITAPI SituationComputePipeline SituationCreateComputePipelineFromMemory(const char* compute_shader_source);`: The core creation function for compute pipelines. On the Vulkan backend, this requires `SITUATION_ENABLE_SHADER_COMPILER` to be defined.
- **Return Value:** Both functions return a `SituationComputePipeline` handle. On success, `handle.id` will be non-zero. On failure, `handle.id` will be 0.

##### 4.6.1.3  Destroying Pipelines
- `SITAPI void SituationUnloadShader(SituationShader* shader);`: Destroys a graphics pipeline and frees its associated GPU resources.
- `SITAPI void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);`: Destroys a compute pipeline and frees its associated GPU resources.
- **Note:** The pointer passed to these functions is zeroed out to invalidate the handle after destruction.

#### 4.6.2  Meshes
Meshes represent renderable geometric objects, containing vertex and index data stored on the GPU.

##### 4.6.2.1  Creating Meshes
- `SITAPI SituationMesh SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count);`: The primary function for creating a mesh.
- **Parameters:**
    - `vertex_data`: A pointer to the raw, interleaved vertex data on the CPU.
    - `vertex_count`: The total number of vertices.
    - `vertex_stride`: The size of a single vertex struct in bytes (e.g., `sizeof(MyVertex)`).
    - `index_data`: A pointer to the array of indices (must be `uint32_t`).
    - `index_count`: The total number of indices.
- **Backend Behavior:**
    - **OpenGL**: Creates a self-contained Vertex Array Object (VAO) that encapsulates the Vertex Buffer Object (VBO), Element Buffer Object (EBO), and vertex attribute layout.
    - **Vulkan**: Creates two device-local `VkBuffer` objects and uses a staging process to upload the data for optimal performance.

##### 4.6.2.2  Destroying Meshes
- `SITAPI void SituationDestroyMesh(SituationMesh* mesh);`: Destroys a mesh and frees all its associated GPU memory (VBO, EBO, VAO, etc.).

#### 4.6.3  Textures
Textures are images stored on the GPU, used for applying color, detail, and other visual effects in shaders.

##### 4.6.3.1  Creating Textures
- `SITAPI SituationTexture SituationCreateTexture(SituationImage image, bool generate_mipmaps);`: Creates a GPU texture by uploading a CPU-side `SituationImage`.
- **Parameters:**
    - `image`: A valid `SituationImage` struct containing the pixel data.
    - `generate_mipmaps`: If `true`, a full mipmap chain will be generated, which improves rendering quality for textures viewed at a distance.

##### 4.6.3.2  Destroying Textures
- `SITAPI void SituationDestroyTexture(SituationTexture* texture);`: Destroys a texture and frees its associated GPU memory.

#### 4.6.4  Buffers (Vertex, Index, Uniform, Storage)
Buffers are generic blocks of GPU memory used to store various types of data.

##### 4.6.4.1  Creating Buffers
- `SITAPI SituationBuffer SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags);`: The primary function for allocating a generic GPU buffer.
- **Parameters:**
    - `size`: The total size of the buffer to allocate in bytes.
    - `initial_data`: A pointer to CPU data to upload. If `NULL`, the buffer is allocated but uninitialized.
    - `usage_flags`: A critical performance hint. A bitmask of `SituationBufferUsageFlags` specifying how the buffer will be used (e.g., `SITUATION_BUFFER_USAGE_VERTEX_BUFFER`, `SITUATION_BUFFER_USAGE_UNIFORM_BUFFER`).

##### 4.6.4.2  Updating Buffers
- `SITAPI SituationError SituationUpdateBuffer(SituationBuffer buffer, size_t offset, size_t size, const void* data);`: Updates a region of an existing buffer with new data from the CPU. This is the primary way to update dynamic data like UBOs.

##### 4.6.4.3  Destroying Buffers
- `SITAPI void SituationDestroyBuffer(SituationBuffer* buffer);`: Destroys a buffer and frees its GPU memory.

#### 4.6.5  Virtual Displays (Off-Screen Rendering Layers)
Virtual Displays are off-screen render targets, essential for UI layers, post-processing, and scene composition. All 2D and 3D drawing commands can be directed to a Virtual Display.

##### 4.6.5.1  Creating Virtual Displays
- `SITAPI int SituationCreateVirtualDisplay(Vector2 resolution, ...);`: Creates a new off-screen render target of a specified resolution and returns its integer ID.

##### 4.6.5.2  Rendering to Virtual Displays
- To draw content *into* a Virtual Display, begin a render pass targeting its ID: `SituationCmdBeginRenderToDisplay(cmd, my_vd_id, clear_color);`.

##### 4.6.5.3  Compositing Virtual Displays
- `SITAPI void SituationRenderVirtualDisplays(SituationCommandBuffer cmd);`: After rendering content to your VDs, this function composites them all onto the current render target (typically the main window), respecting their Z-order, opacity, and blend modes.

##### 4.6.5.4  Accessing the Texture
- `SITAPI SituationTexture SituationGetVirtualDisplayTexture(int display_id);`: Retrieves the underlying `SituationTexture` of a Virtual Display, allowing it to be used as an input for post-processing shaders.

##### 4.6.5.5  Destroying Virtual Displays
- `SITAPI SituationError SituationDestroyVirtualDisplay(int display_id);`: Destroys a virtual display and all its associated GPU resources.

#### 4.6.6  Command Buffers
The command buffer is the core of the rendering workflow. All drawing and compute commands are recorded into it before being submitted to the GPU for execution. This model is explicit in Vulkan and emulated for OpenGL.

##### 4.6.6.1  Frame Lifecycle
The main render loop follows a strict sequence:
1.  `SituationAcquireFrameCommandBuffer()`: Prepares for a new frame.
2.  `SituationGetMainCommandBuffer()`: Gets the command buffer handle for Vulkan.
3.  Record all rendering and compute commands for the frame.
4.  `SituationEndFrame()`: Submits the commands and presents the result.

##### 4.6.6.2  Key Lifecycle Functions
- `SITAPI bool SituationAcquireFrameCommandBuffer(void);`: Prepares the backend for a new frame. For Vulkan, this acquires the next swapchain image. Returns `false` on Vulkan if the window was resized and the frame should be skipped.
- `SITAPI SituationCommandBuffer SituationGetMainCommandBuffer(void);`: Gets the primary command buffer for the current frame. This handle is only required for the Vulkan backend. For OpenGL, this returns `NULL`.
- `SITAPI SituationError SituationEndFrame(void);`: Submits all recorded commands, presents the result to the window, and handles frame timing and VSync.

#### 4.6.7  Render Passes
A Render Pass defines the context for drawing, specifying the render target and how it should be cleared.
- `SITAPI SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color);`: Begins a render pass. `display_id` is -1 for the main window, or an ID from a Virtual Display.
- `SITAPI SituationError SituationCmdEndRender(SituationCommandBuffer cmd);`: Ends the current render pass. All drawing commands must occur between a begin/end pair.

##### 4.6.7.1  Concept & Purpose
A "Render Pass" defines the context for a sequence of drawing operations. It specifies the target attachments (what you are drawing *to*, like a color buffer or depth buffer), how those attachments should be treated at the beginning of the pass (e.g., cleared to a specific color), and how they should be treated at the end. In "Situation", all drawing commands must occur within an active render pass.

##### 4.6.7.2  Render Targets
A render pass operates on a render target. This can be the main application window (the "backbuffer" or "swapchain image") or an off-screen Virtual Display. The library provides handles to refer to these targets.

##### 4.6.7.3  Beginning a Render Pass (SituationCmdBeginRenderPass)
- **Signature:** `SITAPI void SituationCmdBeginRenderPass(SituationCommandBuffer cmd, SituationRenderTarget target, const SituationClearValue* clear_values, uint32_t clear_value_count);`
- This command marks the beginning of a render pass on the specified command buffer.
- `target`: A handle specifying the destination, e.g., `SITUATION_MAIN_WINDOW_TARGET` or a handle from a Virtual Display.
- `clear_values`: An array specifying the clear color for the color attachment and clear value for the depth/stencil attachment.
- **Process (Backend):**
    - **OpenGL**: Binds the appropriate Framebuffer Object (FBO), sets the viewport, and calls `glClear`.
    - **Vulkan**: Records a `vkCmdBeginRenderPass` command with the appropriate `VkRenderPass` and `VkFramebuffer` for the target.

##### 4.6.7.4  Ending a Render Pass (SituationCmdEndRenderPass)
- **Signature:** `SITAPI void SituationCmdEndRenderPass(SituationCommandBuffer cmd);`
- This command finalizes the render pass, performing any necessary layout transitions on the attachments to make them ready for their next use (e.g., presenting to the screen or being sampled as a texture).
- **Process (Backend):**
    - **OpenGL**: Unbinds the FBO, returning rendering to the default framebuffer if necessary.
    - **Vulkan**: Records a `vkCmdEndRenderPass` command.

#### 4.6.8  Dynamic Pipeline State
These commands modify aspects of the rendering pipeline at runtime, within a command buffer.
- `SITAPI void SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height);`: Sets the renderable area of the target.
- `SITAPI void SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);`: Defines a clipping rectangle. Fragments outside this rectangle are discarded.

##### 4.6.8.1  Overview
While the core shaders of a pipeline are fixed upon creation, certain states of the graphics pipeline can be modified dynamically within a command buffer. This allows for greater flexibility and reduces the number of pipeline objects you need to create. These states include blending, depth testing, and face culling. **Note for Vulkan:** For these commands to work, the pipeline must have been created with the corresponding `VK_DYNAMIC_STATE_*` flags enabled. "Situation" handles this for its internal pipelines.

##### 4.6.8.2  Blending Control (SituationCmdSetBlendMode)
- **Signature:** `SITAPI void SituationCmdSetBlendMode(SituationCommandBuffer cmd, SituationBlendMode mode);`
- Sets the color blending equation for subsequent draw calls.
- `mode`: An enum specifying the blend mode (e.g., `SITUATION_BLEND_ALPHA`, `SITUATION_BLEND_ADDITIVE`).
- **Process (Backend):**
    - **OpenGL**: Calls `glEnable(GL_BLEND)` and `glBlendFunc()` with the appropriate parameters.
    - **Vulkan**: If the pipeline supports dynamic blend state, this function could be used, but typically blending is part of the `VkPipeline` state. This would likely be an abstraction that binds a pre-created pipeline with the desired blend state.

##### 4.6.8.3  Depth & Stencil Control (SituationCmdSetDepthState)
- **Signature:** `SITAPI void SituationCmdSetDepthState(SituationCommandBuffer cmd, bool depth_test_enabled, bool depth_write_enabled, SituationCompareOp compare_op);`
- Configures the depth buffer behavior.
- `depth_test_enabled`: Toggles the depth test on or off.
- `depth_write_enabled`: Toggles writing to the depth buffer (`glDepthMask`).
- `compare_op`: The comparison function (e.g., `SITUATION_COMPARE_OP_LESS`, `SITUATION_COMPARE_OP_GREATER_OR_EQUAL`).

##### 4.6.8.4  Face Culling (SituationCmdSetCullMode)
- **Signature:** `SITAPI void SituationCmdSetCullMode(SituationCommandBuffer cmd, SituationCullMode mode);`
- Sets the face culling mode to avoid rendering back-faces or front-faces.
- `mode`: An enum specifying which faces to cull (e.g., `SITUATION_CULL_MODE_NONE`, `SITUATION_CULL_MODE_BACK`, `SITUATION_CULL_MODE_FRONT`).

##### 4.6.8.5  Scissor Test (SituationCmdSetScissor)
- **Signature:** `SITAPI void SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);`
- Defines a rectangle outside of which all fragments are discarded. Useful for UI clipping and optimization.

#### 4.6.9  Instanced & Indirect Drawing
Advanced techniques for rendering a large number of objects with a single API call.
- `SITAPI void SituationCmdDrawIndexed(SituationCommandBuffer cmd, ...);`: The standard indexed draw call.
- `SITAPI void SituationCmdDrawIndexedInstanced(...);` (Conceptual): Renders many copies of the same mesh, each with unique properties read from an instance buffer.
- `SITAPI void SituationCmdDrawIndexedIndirect(...);` (Conceptual): Executes a draw call whose parameters (index count, instance count) are read directly from a GPU buffer, enabling GPU-driven rendering.

##### 4.6.9.1  Overview & Purpose
To render scenes with thousands of similar objects (like trees, bullets, or particles), issuing a separate draw call for each object is highly inefficient due to CPU overhead. Instanced and Indirect Drawing are advanced techniques that solve this by allowing you to render many objects with a single API call, offloading work from the CPU to the GPU.
- **Instanced Drawing:** Renders many copies of the *same mesh* in one draw call, with each copy (instance) having unique properties like position, rotation, or color, which are read from a separate buffer.
- **Indirect Drawing:** Takes this a step further by allowing the GPU itself to determine the parameters of the draw call (e.g., how many vertices or instances to draw). This is extremely powerful when combined with compute shaders for tasks like GPU-driven culling.

##### 4.6.9.2  Instanced Drawing
**Core Concept**
Instancing requires two main components: a base mesh (what to draw) and a per-instance data buffer (the properties for each copy). The vertex shader receives a unique `gl_InstanceID` for each instance, which it uses to look up the correct data from the per-instance buffer.

**Binding Per-Instance Data (SituationCmdBindInstanceBuffer)**
- **Signature:** `SITAPI void SituationCmdBindInstanceBuffer(SituationCommandBuffer cmd, SituationBuffer buffer, uint32_t binding_point);`
- Binds a `SituationBuffer` containing per-instance data (e.g., an array of model matrices or colors).
- `binding_point`: A unique vertex buffer binding point, separate from the main mesh's vertex buffer binding. The vertex shader will use this binding point for its per-instance attributes.

**Recording an Instanced Draw Call (SituationCmdDrawIndexedInstanced)**
- **Signature:** `SITAPI void SituationCmdDrawIndexedInstanced(SituationCommandBuffer *md, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);`
- This command functions like `SituationCmdDrawIndexed` but draws `instance_count` copies of the mesh.
- The vertex shader will be invoked `index_count * instance_count` times in total.

##### 4.6.9.3  Indirect Drawing
**Core Concept**
Indirect drawing uses a `SituationBuffer` (an "indirect buffer") that contains the *parameters* for one or more draw calls. The GPU reads these parameters directly from the buffer to execute the draw, eliminating the need for the CPU to specify them. This is the key to GPU-driven rendering pipelines.

**The Indirect Draw Command Structure**
The indirect buffer must be filled with one or more `SituationDrawIndexedIndirectCommand` structs:
```c
typedef struct SituationDrawIndexedIndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
} SituationDrawIndexedIndirectCommand;
```

**Creating an Indirect Buffer**
An indirect buffer is created like any other buffer, but with the `SITUATION_BUFFER_USAGE_INDIRECT_BUFFER` flag. It is typically written to by a compute shader.

**Recording an Indirect Draw Call (SituationCmdDrawIndexedIndirect)**
- **Signature:** `SITAPI void SituationCmdDrawIndexedIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset, uint32_t draw_count, uint32_t stride);`
- `indirect_buffer`: The `SituationBuffer` containing the draw command structures.
- `offset`: The byte offset into the buffer where the first draw command is located.
- `draw_count`: The number of separate draw commands to execute from the buffer.
- `stride`: The byte stride between consecutive draw commands in the buffer. Usually `sizeof(SituationDrawIndexedIndirectCommand)`.

**Common Use Case: GPU Culling**
1. A compute shader runs, determines which objects are visible, and writes their data to a buffer.
2. The same compute shader writes a single `SituationDrawIndexedIndirectCommand` to an indirect buffer, setting `instanceCount` to the number of visible objects it found.
3. A `SituationCmdPipelineBarrier` synchronizes the compute shader's writes with the graphics pipeline's reads.
4. `SituationCmdDrawIndexedIndirect` is called with `draw_count = 1`. The GPU reads the command and draws the exact number of visible instances, with zero CPU involvement in the culling decision.

---
### 4.7 Compute Shaders
#### 4.7.1 Overview & Capabilities
Compute shaders enable developers to harness the parallel processing power of the GPU for general-purpose computations that are independent of the traditional graphics rendering pipeline. This includes tasks like physics simulations, AI calculations, image/video processing, procedural generation, and more. `situation.h` provides a unified, backend-agnostic API to create, manage, and execute compute shaders using either OpenGL Compute Shaders or Vulkan Compute Pipelines.

#### 4.7.2 Initialization Prerequisites
- The core `situation.h` library must be successfully initialized using `SituationInit`.
- Define `SITUATION_ENABLE_SHADER_COMPILER` in your build. This is **mandatory** for the Vulkan backend and highly recommended for OpenGL if you are providing GLSL source code. It enables the inclusion and use of the `shaderc` library for runtime compilation of GLSL to SPIR-V bytecode.
- For Vulkan: Ensure that the selected physical device (GPU) supports compute capabilities. This is checked during `SituationInit` if a Vulkan backend is chosen.

#### 4.7.3 Creating Compute Pipelines
##### 4.7.3.1 From GLSL Source Code (SituationCreateComputePipelineFromMemory)
This is the primary function for creating a compute pipeline.
- **Signature:** `SITAPI SituationComputePipeline SituationCreateComputePipelineFromMemory(const char* compute_shader_source);`
- **Parameters:**
    - `compute_shader_source`: A null-terminated string containing the GLSL compute shader source code.
- **Process:**
    1.  Validates that the library is initialized and the source is not NULL.
    2.  If `SITUATION_ENABLE_SHADER_COMPILER` is defined:
        a.  Invokes `shaderc` to compile the provided GLSL source into SPIR-V bytecode.
        b.  Handles compilation errors and reports them via the error system.
    3.  Backend-Specific Creation:
        - **OpenGL**: Uses the SPIR-V (if compiled) or directly the GLSL source (if `ARB_gl_spirv` is not used/available) to create and link an OpenGL Compute Program (`glCreateProgram`, `glCreateShader(GL_COMPUTE_SHADER)`, `glLinkProgram`).
        - **Vulkan**: Uses the compiled SPIR-V bytecode to create a `VkShaderModule`, then a `VkPipelineLayout` (handling push constants), and finally the `VkComputePipeline` object.
    4.  Generates a unique `id` for the `SituationComputePipeline` handle.
    5.  Stores backend-specific handles internally (e.g., `gl_program_id`, `vk_pipeline`, `vk_pipeline_layout`).
    6.  Adds the pipeline to an internal tracking list for resource management and leak detection.
- **Return Value:**
    - Returns a `SituationComputePipeline` struct.
    - On success, `pipeline.id` will be a non-zero value.
    - On failure, `pipeline.id` will be 0. Use `SituationGetLastErrorMsg()` to get a detailed error description.

##### 4.7.3.2 Backend Compilation (OpenGL SPIR-V, Vulkan Runtime)
- The use of `shaderc` via `SITUATION_ENABLE_SHADER_COMPILER` standardizes the input (GLSL) and the intermediate representation (SPIR-V) for both backends, making the API consistent.
- OpenGL traditionally uses GLSL directly, but `ARB_gl_spirv` allows using SPIR-V. The library abstracts this choice.
- Vulkan *requires* SPIR-V, making runtime compilation with `shaderc` essential unless pre-compiled SPIR-V is used (which this function doesn't directly support, but the underlying Vulkan creation could be adapted).

#### 4.7.4 Using Compute Pipelines
Once a `SituationComputePipeline` is created, it can be used within a command buffer to perform computations.

##### 4.7.4.1 Binding a Compute Pipeline (SituationCmdBindComputePipeline)
- **Signature:** `SITAPI void SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline);`
- **Parameters:**
    - `cmd`: The command buffer obtained from `SituationAcquireFrameCommandBuffer` or `SituationBeginVirtualDisplayFrame`.
    - `pipeline`: The `SituationComputePipeline` handle returned by `SituationCreateComputePipelineFromMemory`.
- **Process:**
    1.  Validates the command buffer and pipeline handle.
    2.  Records the command to bind the pipeline state (program/pipeline object) to the command buffer for subsequent compute operations.
    3.  Backend-Specific:
        - **OpenGL**: Calls `glUseProgram(pipeline.gl_program_id)`.
        - **Vulkan**: Calls `vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.vk_pipeline)`.

##### 4.7.4.2 Binding Resources (Buffers, Images)
Compute shaders often read from and write to GPU resources like Shader Storage Buffer Objects (SSBOs) or Images.
- `SITAPI void SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, SituationBuffer buffer, uint32_t binding);`
    - Binds a `SituationBuffer` (created with appropriate usage flags like `SITUATION_BUFFER_USAGE_STORAGE_BUFFER`) to a specific binding point in the currently bound compute shader.
    - **Backend-Specific:**
        - **OpenGL**: Calls `glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer.gl_buffer_id)`.
        - **Vulkan**: Allocates a temporary descriptor set (or uses a pre-allocated one) that describes the buffer binding, then records `vkCmdBindDescriptorSets` for that set.

##### 4.7.4.3 Dispatching Work (SituationCmdDispatch)
- **Signature:** `SITAPI void SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);`
- **Parameters:**
    - `cmd`: The command buffer.
    - `group_count_x/y/z`: The number of local work groups to dispatch in each dimension. The total number of invocations is `group_count_x * group_count_y * group_count_z * local_size_x * local_size_y * local_size_z` (where local size is defined in the shader).
- **Process:**
    1.  Validates the command buffer.
    2.  Records the command to dispatch the compute work.
    3.  Backend-Specific:
        - **OpenGL**: Calls `glDispatchCompute(group_count_x, group_count_y, group_count_z)`.
        - **Vulkan**: Calls `vkCmdDispatch(cmd, group_count_x, group_count_y, group_count_z)`.

#### 4.7.5 Synchronization & Memory Barriers
##### 4.7.5.1 Importance of Synchronization in Compute
GPU operations, including compute shaders, can execute asynchronously and out-of-order relative to CPU commands and other GPU operations. Memory barriers are crucial to ensure that reads happen after writes, and that dependencies between operations are correctly observed.

##### 4.7.5.2 Using SituationCmdPipelineBarrier
The primary tool for synchronization is `SituationCmdPipelineBarrier`. It provides fine-grained control by explicitly defining the source of a memory dependency (the producer stage) and the destination (the consumer stage). This allows the driver to create a much more efficient barrier than a coarse, "sledgehammer" approach.

- **Signature:** `SITAPI void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags);`
- **Parameters:**
    - `cmd`: The command buffer to record the barrier into.
    - `src_flags`: A bitmask of `SituationBarrierSrcFlags` indicating the pipeline stage(s) that **WROTE** the data. For compute, this is typically `SITUATION_BARRIER_COMPUTE_SHADER_WRITE`.
    - `dst_flags`: A bitmask of `SituationBarrierDstFlags` indicating the pipeline stage(s) that will **READ** the data. If a vertex shader will read the results (e.g., from an SSBO used as a vertex buffer), this would be `SITUATION_BARRIER_VERTEX_SHADER_READ`.
- **Process:** This function maps the abstract source and destination flags to the precise stage and access masks required by the underlying API (`vkCmdPipelineBarrier` in Vulkan or a combination of flags for `glMemoryBarrier` in OpenGL).
- **Example: GPU Particle Simulation**
A common use case is updating particle positions in a compute shader and then immediately rendering them. A barrier is required between the dispatch and the draw call.
```c
// 1. Dispatch compute shader to update particle data in an SSBO
SituationCmdBindComputePipeline(cmd, particle_update_pipeline);
SituationCmdBindComputeBuffer(cmd, 0, particle_data_ssbo);
SituationCmdDispatch(cmd, PARTICLE_GROUPS, 1, 1);

// 2. *** CRITICAL BARRIER ***
//    Ensure the compute shader's writes to the SSBO are finished and visible
//    before the vertex shader stage attempts to read that data as vertex attributes.
SituationCmdPipelineBarrier(cmd,
                          SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
                          SITUATION_BARRIER_VERTEX_SHADER_READ);

// 3. Draw the particles using a graphics pipeline
SituationCmdBindPipeline(cmd, particle_render_pipeline);
// The same SSBO is now used as the source for vertex data
SituationCmdBindVertexBuffer(cmd, particle_data_ssbo);
SituationCmdDraw(cmd, PARTICLE_COUNT, 1, 0, 0);
```
- **Note on `SituationMemoryBarrier`:**
The library also provides a simpler, deprecated function `SituationMemoryBarrier(cmd, barrier_bits)`. This function is less optimal as it creates a very coarse barrier. It is provided for backward compatibility or extremely simple cases. For all new development, **`SituationCmdPipelineBarrier` is strongly recommended.**

#### 4.7.6 Destroying Compute Pipelines (SituationDestroyComputePipeline)
- **Signature:** `SITAPI void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);`
- **Parameters:**
    - `pipeline`: A pointer to the `SituationComputePipeline` handle to be destroyed. The handle's `id` field will be set to 0 upon successful destruction.
- **Process:**
    1.  Validates the input pointer and that the pipeline has a non-zero `id`.
    2.  Removes the pipeline from the internal tracking list.
    3.  Backend-Specific Cleanup:
        - **OpenGL**: Calls `glDeleteProgram(pipeline->gl_program_id)`.
        - **Vulkan**:
            a. Waits for the device to be idle (`vkDeviceWaitIdle`) to ensure no commands using the pipeline are pending.
            b. Destroys the Vulkan objects: `vkDestroyPipeline`, `vkDestroyPipelineLayout`.
    4.  Invalidates the handle by setting `pipeline->id = 0`.


---
### 4.8 Audio System
#### 4.8.1 Device Enumeration (SituationGetAudioDevices)
- **Signature:** `SITAPI SituationError SituationGetAudioDevices(SituationAudioDeviceInfo** out_devices, int* out_count);`
- **Parameters:**
    - `out_devices`: A pointer to a variable that will receive a pointer to an array of `SituationAudioDeviceInfo` structs. The caller is responsible for freeing this array using `SituationFreeAudioDevices`.
    - `out_count`: A pointer to an integer that will receive the number of devices in the `out_devices` array.
- **Process:**
    1.  Validates the library is initialized and that `out_devices` and `out_count` are not NULL.
    2.  Uses the underlying `miniaudio` library to enumerate available audio playback and capture devices.
    3.  Allocates an array to hold the device information.
    4.  Populates the array with details like device name, ID (as a `ma_device_id` internally), and type (Playback/Capture).
    5.  Sets `*out_devices` to the allocated array and `*out_count` to the number of devices found.
- **Return Value:**
    - `SITUATION_SUCCESS` on success.
    - An appropriate error code (e.g., `SITUATION_ERROR_AUDIO_DEVICE_FAILED`) on failure. The array `*out_devices` will be NULL in this case.

#### 4.8.2 Audio Playback
##### 4.8.2.1 Loading Sounds (SituationLoadSoundFromFile, SituationLoadSoundFromMemory)
- `SITAPI SituationSound SituationLoadSoundFromFile(const char* filepath);`
    - Loads an audio file (WAV, MP3, OGG, FLAC supported by miniaudio) from disk.
    - Decodes the audio data into memory (PCM).
    - Returns a `SituationSound` handle. Check `sound.id != 0`.
- `SITAPI SituationSound SituationLoadSoundFromMemory(const void* data, size_t data_size);`
    - Loads audio data from a memory buffer.
    - Decodes the audio data into memory (PCM).
    - Returns a `SituationSound` handle. Check `sound.id != 0`.

##### 4.8.2.2 Playing, Pausing, Stopping Sounds
- `SITAPI void SituationPlaySound(SituationSound sound);`
    - Starts playing the sound from the beginning (or resumes if paused).
- `SITAPI void SituationPauseSound(SituationSound sound);`
    - Pauses the currently playing sound.
- `SITAPI void SituationStopSound(SituationSound sound);`
    - Stops the sound and resets its playback position to the beginning.

##### 4.8.2.3 Controlling Volume & Panning
- `SITAPI void SituationSetSoundVolume(SituationSound sound, float volume);`
    - Sets the volume for the sound (0.0 = silent, 1.0 = original volume, >1.0 = amplification if supported).
- `SITAPI void SituationSetSoundPan(SituationSound sound, float pan);`
    - Sets the stereo panning (-1.0 = full left, 0.0 = center, 1.0 = full right). Note: Implementation detail depends on `miniaudio`'s spatialization capabilities or custom mixing.

##### 4.8.2.4 Looping & Streaming
- Looping is typically controlled via flags during sound loading or dedicated functions (implementation detail).
- For large files, streaming (loading in chunks during playback) might be used internally by `miniaudio` or require specific API calls (implementation detail).

#### 4.8.3 Audio Capture (Recording)
- The library provides functions to enumerate capture devices (`SituationGetAudioDevices` filtering for capture).
- `SITAPI SituationError SituationStartRecording(const SituationAudioDeviceInfo* device_info, uint32_t sample_rate, ma_format format, uint32_t channels);`
    - Starts recording audio from the specified `device_info` (obtained via `SituationGetAudioDevices`).
    - Configures the recording with the desired `sample_rate`, `format` (e.g., `ma_format_f32`), and `channels` (e.g., 1 for mono, 2 for stereo).
    - Recorded data is made available through a callback mechanism or an internal buffer that the application polls (implementation detail based on `miniaudio`).
- `SITAPI void SituationStopRecording(void);`
    - Stops the active recording session.

#### 4.8.4 Destroying Sounds (SituationDestroySound)
- **Signature:** `SITAPI void SituationDestroySound(SituationSound* sound);`
- **Parameters:**
    - `sound`: A pointer to the `SituationSound` handle to be destroyed. The handle's `id` field will be set to 0 upon successful destruction.
- **Process:**
    1.  Validates the input pointer and that the sound has a non-zero `id`.
    2.  Frees the decoded PCM audio data associated with the sound.
    3.  Removes the sound from the internal tracking list.
    4.  Invalidates the handle by setting `sound->id = 0`.

#### 4.8.5  Audio Effects & Custom Processing
##### 4.8.5.1  Overview & Effects Chain
"Situation" provides a real-time, per-sound effects processing chain. When a sound is played, its raw audio data is passed through a series of effects before being mixed for output. This allows for dynamic modification of sounds for environmental effects, creative expression, or audio analysis. The chain consists of two main stages: built-in effects and custom user-attached processors. Effects are always processed in a fixed order to ensure predictable results.

##### 4.8.5.2  Built-in Effects
The library includes several common, high-quality audio effects that can be enabled and configured on any sound.

**Filters (Low-pass & High-pass)**
Filters alter the frequency content of a sound. A low-pass filter removes high frequencies (making a sound muffled, as if through a wall), while a high-pass filter removes low frequencies (making a sound tinny).
- **Signature:** `SITAPI SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor);`
- `sound`: The sound to apply the filter to.
- `type`: `SITUATION_FILTER_LOWPASS`, `SITUATION_FILTER_HIGHPASS`, or `SITUATION_FILTER_NONE` to disable.
- `cutoff_hz`: The frequency (in Hz) at which the filter starts to take effect.
- `q_factor`: The resonance or "quality" of the filter. A value around 0.707 is neutral. Higher values create a resonant peak at the cutoff frequency.

**Echo (Delay)**
Creates repeating, decaying echoes of the original sound.
- **Signature:** `SITAPI SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix);`
- `enabled`: Set to `true` to activate the echo effect.
- `delay_sec`: The time in seconds between each echo.
- `feedback`: How much of the echo is fed back into the delay line. Values are typically [0.0 - 1.0], where 1.0 would be an infinite echo.
- `wet_mix`: The volume of the echo signal. 1.0 means the echo is at full volume.

**Reverb**
Simulates the acoustic reflections of a room or space, adding depth and atmosphere.
- **Signature:** `SITAPI SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix);`
- `enabled`: Set to `true` to activate the reverb effect.
- `room_size`: A value [0.0 - 1.0] representing the perceived size of the simulated space.
- `damping`: A value [0.0 - 1.0] representing how much high frequencies are absorbed by the room's surfaces.
- `wet_mix`: The volume of the reverberated ("wet") signal.
- `dry_mix`: The volume of the original ("dry") signal.

##### 4.8.5.3  Custom Audio Processing
For advanced effects not provided by the library (e.g., bitcrushing, flangers, custom equalization), you can attach your own real-time Digital Signal Processing (DSP) functions directly to a sound's effects chain.

**The Processor Callback**
A custom processor is a C function with a specific signature that you provide. This function will be called by the audio thread with a buffer of audio data to modify in-place.
- **Signature:** `typedef void (*SituationAudioProcessorCallback)(void* buffer, unsigned int frames, int channels, int sampleRate, void* userData);`
- `buffer`: A pointer to the floating-point PCM audio data to be processed.
- `frames`: The number of audio frames in the buffer.
- `channels`: The number of audio channels.
- `sampleRate`: The sample rate of the audio data.
- `userData`: The custom user data pointer you provided when attaching the processor.

**Attaching & Detaching Processors**
You can attach multiple custom processors to a single sound. They will be executed in the order they were attached.
- **Attach:** `SITAPI SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* userData);`
- **Detach:** `SITAPI SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* userData);`

##### 4.8.5.4  Processing Order
To ensure consistent and predictable audio output, effects are always applied in the following order for each block of audio data:
1.  **Built-in Filter** (if enabled)
2.  **Built-in Echo** (if enabled)
3.  **Built-in Reverb** (if enabled)
4.  **Custom Processors** (executed sequentially in the order they were attached)
5.  Final Pitch/Pan/Volume adjustments before mixing.
---
### 4.9 Text Rendering
#### 4.9.1 Simple Text Drawing (SituationDrawTextSimple)
- **Signature:** `SITAPI void SituationDrawTextSimple(const char* text, float x, float y, float scale, ColorRGBA color);`
- Draws text character by character using a simple, built-in font (often 8x8 or similar bitmap).
- Parameters define position (`x`, `y`), size (`scale`), and color.
- **Note:** As indicated in the library code comments, this method is intentionally slow and intended for debugging or simple UI elements where performance is not critical.

#### 4.9.2 Styled Text Rendering (SituationDrawTextStyled)
- **Signature:** `SITAPI void SituationDrawTextStyled(SituationFont font, const char* text, float x, float y, float font_size, ColorRGBA color);`
- Draws high-quality text using pre-rendered font atlases (textures) and Signed Distance Fields (SDF).
- Requires a `SituationFont` handle, obtained via font loading functions.
- Offers superior performance and visual quality (smooth scaling, sharp edges) compared to `SituationDrawTextSimple`.
- Parameters define the font, text string, position, size (`font_size`), and color.

#### 4.9.3 Font Loading & Management
- `SITAPI SituationFont SituationLoadFontFromFile(const char* filepath);`
    - Loads a TrueType Font (TTF) file.
    - Internally uses `stb_truetype` to parse the font and generate SDF data for an atlas texture.
    - Returns a `SituationFont` handle. Check `font.id != 0`.
- `SITAPI void SituationDestroyFont(SituationFont* font);`
    - Destroys a loaded font, freeing the associated atlas texture and `stbtt_fontinfo` data.
    - Invalidates the handle.

---
### 4.10 2D Rendering & Drawing
While "Situation" is a powerful 3D rendering library, it also provides a comprehensive and high-performance suite of tools specifically for classic 2D drawing. This is ideal for building user interfaces, debugging overlays, data visualizations, or complete 2D games. All 2D drawing functions operate within the Command Buffer model.

#### 4.10.1  2D Coordinate System & Camera
For all 2D drawing commands, "Situation" automatically establishes a 2D orthographic coordinate system. The origin (0, 0) is at the **top-left** corner of the current render target (either the main window or a Virtual Display). The X-axis increases to the right, and the Y-axis increases downwards. You do not need to set up a 3D camera or projection matrix; the library manages this internally for all `SituationCmdDraw*` 2D functions.

#### 4.10.2  Drawing Basic Shapes
The library provides commands for rendering primitive geometric shapes, which form the building blocks of any 2D scene.

##### 4.10.2.1 Rectangles (SituationCmdDrawQuad)
This is the primary function for drawing solid-colored rectangles. It uses the library's internal, optimized quad renderer.
- **Signature:** `SITAPI void SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, vec4 color);`
- `model`: A `mat4` transformation matrix used to set the rectangle's position, size, and rotation. Use `cglm` helpers (`glm_translate`, `glm_scale`, `glm_rotate`) to build this matrix.
- `color`: A normalized `vec4` representing the RGBA color of the quad.

##### 4.10.2.2 Lines & Circles (Concept)
While not yet implemented, the API is designed to easily accommodate high-level commands for drawing other primitives like lines (`SituationCmdDrawLine`), circles (`SituationCmdDrawCircle`), and polygons.

#### 4.10.3  Drawing Textures (Sprites)
This is the core of 2D rendering, allowing you to draw images and sprite sheets to the screen.

##### 4.10.3.1 Loading Textures
First, load your image file into a `SituationTexture` handle using the functions described in section `4.6.3`.
- `SituationTexture my_sprite = SituationCreateTexture(SituationLoadImage("assets/player.png"), true);`

##### 4.10.3.2 Drawing a Full Texture (SituationCmdDrawTexture)
This high-level command draws a texture with transformations and a color tint.
- **Signature:** `SITAPI void SituationCmdDrawTexture(SituationCommandBuffer cmd, SituationTexture texture, vec2 position, vec4 tint);`
- `texture`: The `SituationTexture` to draw.
- `position`: The top-left destination coordinate `(x, y)` on the render target.
- `tint`: A `vec4` color multiplier. White `{1,1,1,1}` draws the texture with its original colors.

##### 4.10.3.3 Advanced Sprite Drawing (SituationCmdDrawTextureEx)
For sprite sheets, rotation, and scaling, an extended version provides more control.
- **Signature:** `SITAPI void SituationCmdDrawTextureEx(SituationCommandBuffer cmd, SituationTexture texture, Rectangle source_rect, Rectangle dest_rect, vec2 origin, float rotation, vec4 tint);`
- `source_rect`: The rectangular region of the texture to draw (for sprite sheets).
- `dest_rect`: The destination rectangle on the screen, defining position and size.
- `origin`: The rotation pivot point, relative to the top-left of the destination rectangle. `(0,0)` pivots from the top-left corner.
- `rotation`: The rotation in degrees.
- `tint`: The color tint.

#### 4.10.4  Text Rendering
The library includes a powerful text rendering system suitable for UI, HUDs, and any in-game text. For a full API reference, see section `4.9`.
- **High-Quality Styled Text:** Use `SituationDrawTextStyled` for crisp, anti-aliased text with support for TrueType fonts (.ttf). This is the recommended function for all user-facing text.
- **Simple Debug Text:** Use `SituationDrawTextSimple` for quick, unstyled text output, ideal for debugging information where performance and visual quality are not critical.

#### 4.10.5  UI & Layer Management
"Situation" provides two key features that are essential for building complex 2D UIs and managing render layers.

##### 4.10.5.1 Scissor/Clipping (SituationCmdSetScissor)
The scissor command restricts all subsequent drawing to a specific rectangular area on the screen. This is indispensable for creating UI elements like scrollable lists, text boxes, or windows where content must be clipped to a boundary.
- See section `4.6.8.5` for the full API reference.
- **Example workflow:**
  1. Call `SituationCmdSetScissor(cmd, panel_x, panel_y, panel_width, panel_height);`
  2. Draw all content that should appear inside the panel.
  3. Disable the scissor by setting it to the full screen size.

##### 4.10.5.2 Virtual Displays as UI Layers
The Virtual Display system (see `4.6.5`) is a perfect tool for 2D layer management. You can render an entire UI screen or game layer to an off-screen Virtual Display first. This allows you to:
- Apply shaders and post-processing effects to the entire UI layer at once.
- Scale a low-resolution UI to a high-resolution screen with pixel-perfect filtering (`SITUATION_SCALING_INTEGER`).
- Easily manage render order using the `z_order` property when compositing the layers back to the main window.
</details>

---
<details>
<summary><h2>5. Building & Configuration</h2></summary>

### 5.1 Backend Selection Defines (SITUATION_USE_OPENGL, SITUATION_USE_VULKAN)
- Define **exactly one** of these before including `situation.h` in the implementation file.
- `SITUATION_USE_OPENGL`: Enables the OpenGL rendering backend. Requires `GLAD` for OpenGL function loading.
- `SITUATION_USE_VULKAN`: Enables the Vulkan rendering backend. Requires Vulkan SDK headers, `shaderc` (if `SITUATION_ENABLE_SHADER_COMPILER`), and `Vulkan Memory Allocator (VMA)`.

### 5.2 Feature Enablement Defines (SITUATION_ENABLE_SHADER_COMPILER, etc.)
- `SITUATION_ENABLE_SHADER_COMPILER`: Mandatory for Vulkan compute shaders and recommended for OpenGL compute or when loading GLSL shaders from source strings. Enables `shaderc` for runtime GLSL compilation.
- Other potential defines might control optional features or integrations (e.g., specific image format support via stb, profiling hooks).

### 5.3 Shared Library Support (SITUATION_BUILD_SHARED, SITUATION_USE_SHARED)
- `SITUATION_BUILD_SHARED`: Define this in the source file used to compile the shared library (DLL). It modifies symbol visibility.
- `SITUATION_USE_SHARED`: Define this in application source files that link against the pre-compiled `situation.h` shared library.

### 5.4 Compiler & Linker Flags
- **General:** C99 or C++ compiler. Link against system libraries for threading (e.g., `-lpthread` on Linux/macOS).
- **GLFW:** Link against the GLFW3 library (e.g., `-lglfw`).
- **OpenGL:** Link against system OpenGL library (e.g., `-lGL` on Linux, `-framework OpenGL` on macOS). Include GLAD source.
- **Vulkan:** Link against the Vulkan loader library (e.g., `-lvulkan`). Include Vulkan SDK headers, shaderc, VMA source.
- **miniaudio:** Include `miniaudio.h` and define `MINIAUDIO_IMPLEMENTATION` in one source file. May require linking against system audio APIs (e.g., `-ldsound`/`-lole32`/`-lksuser` on Windows, `-framework CoreAudio`/`-framework CoreFoundation` on macOS).
</details>

---
<details>
<summary><h2>6. Examples & Tutorials</h2></summary>

### 6.1 Basic Triangle Rendering
This example demonstrates the minimal steps required to render a single, colored triangle using `situation.h`. It covers window setup, shader creation, mesh definition, and the core rendering loop.
```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "situation.h"
#include <stdio.h>
#include <stdlib.h> // For malloc/free if needed
#include <string.h> // For memset

// --- Vertex Data for a Simple Triangle ---
// Define a simple vertex structure (position + color)
typedef struct {
    float position[3]; // x, y, z
    float color[3];    // r, g, b
} Vertex;

// Vertex data for a triangle (using an index buffer now for best practice)
static const Vertex triangle_vertices[] = {
    {{ 0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // Top, Red
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // Bottom Left, Green
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}  // Bottom Right, Blue
};
static const uint32_t triangle_indices[] = { 0, 1, 2 };

// --- Simple Vertex and Fragment Shaders ---
// Note: Using GLSL 450+ features like `#version 450` and `#extension GL_ARB_separate_shader_objects`
// might be necessary depending on the backend and driver.
static const char* vertex_shader_source = R"(
#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 fragColor;
// NOTE: This example is simple and does not use the ViewData UBO.
// A real application would use it for camera transformations.
void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragColor = inColor;
}
)";

static const char* fragment_shader_source = R"(
#version 450 core
layout(location = 0) in vec3 fragColor;
out vec4 outColor;
void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

// --- Global Handles ---
static SituationShader g_shader_pipeline = {0};
static SituationMesh g_triangle_mesh = {0};

// --- Initialization ---
int init_resources() {
    // 1. Create Shader Pipeline
    g_shader_pipeline = SituationLoadShaderFromMemory(vertex_shader_source, fragment_shader_source);
    if (g_shader_pipeline.id == 0) {
        fprintf(stderr, "Failed to load shader: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // 2. Create Mesh
    // Best practice is to always use an index buffer, even for simple geometry.
    g_triangle_mesh = SituationCreateMesh(
        triangle_vertices,
        3, // vertex_count
        sizeof(Vertex), // vertex_stride
        triangle_indices,
        3 // index_count
    );
    if (g_triangle_mesh.id == 0) {
        fprintf(stderr, "Failed to create mesh: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    return 0; // Success
}

// --- Cleanup ---
void cleanup_resources() {
    if (g_triangle_mesh.id != 0) {
        SituationDestroyMesh(&g_triangle_mesh);
    }
    if (g_shader_pipeline.id != 0) {
        SituationUnloadShader(&g_shader_pipeline);
    }
}

// --- Main Rendering Function ---
void render_frame() {
    // 1. Prepare for a new frame. Returns false on Vulkan if the window was resized.
    if (!SituationAcquireFrameCommandBuffer()) {
        return; // Skip rendering this frame
    }

    // 2. Get the main command buffer for recording. (Returns NULL for OpenGL).
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // 3. Begin a render pass, clearing the main window to a dark color.
    SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });

    // 4. Bind the graphics pipeline (shader).
    SituationCmdBindPipeline(cmd, g_shader_pipeline);

    // 5. Draw the mesh. This is a high-level command that handles binding the
    //    correct vertex/index buffers for the given mesh.
    SituationCmdDrawMesh(cmd, g_triangle_mesh);

    // 6. End the render pass.
    SituationCmdEndRender(cmd);

    // 7. Submit all recorded commands and present the frame.
    SituationEndFrame();
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Basic Triangle";
    init_info.window_width = 800;
    init_info.window_height = 600;

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    if (init_resources() != 0) {
        SituationShutdown();
        return -1;
    }

    printf("Running... Close the window to quit.\n");

    // --- Main Loop ---
    while (!SituationWindowShouldClose()) {
        // Poll for OS events (keyboard, mouse, window resize, etc.)
        SituationPollInputEvents();

        // Update internal timers (good practice, though not used in this simple example)
        SituationUpdateTimers();

        // Render the frame using our dedicated function
        render_frame();
    }

    cleanup_resources();
    SituationShutdown();
    return 0;
}
```

### 6.2 Loading and Rendering a 3D Model
This example shows how to load a 3D model from a file (e.g., Wavefront .OBJ) and render it using `situation.h`. It assumes the existence of a function like `SituationLoadModelFromObj` (based on library snippets) or a similar model loading mechanism.
```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "situation.h"
#include <stdio.h>
#include <cglm/cglm.h> // Assuming cglm is used for math

// --- Global Handles ---
static SituationShader g_model_shader = {0};
static SituationModel g_loaded_model = {0}; // Assuming SituationModel exists as per snippets
static SituationTexture g_diffuse_texture = {0};

// --- Simple Shaders for a Textured Model ---
static const char* model_vertex_shader = R"(
#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragWorldPos;

layout(std140, binding = 0) uniform ViewData {
    mat4 uView;
    mat4 uProjection;
};

layout(push_constant) uniform ModelData {
    mat4 uModel;
};

void main() {
    vec4 worldPos = uModel * vec4(inPosition, 1.0);
    gl_Position = uProjection * uView * worldPos;
    fragTexCoord = inTexCoord;
    fragNormal = mat3(transpose(inverse(uModel))) * inNormal; // Normal matrix
    fragWorldPos = worldPos.xyz;
}
)";

static const char* model_fragment_shader = R"(
#version 450 core
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D uDiffuseTexture;

void main() {
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Simple directional light
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, lightDir), 0.2); // Ambient + Diffuse

    vec4 texColor = texture(uDiffuseTexture, fragTexCoord);
    outColor = vec4(diff * texColor.rgb, texColor.a);
}
)";

// --- Initialization ---
int init_model_resources() {
    // 1. Load Shader
    g_model_shader = SituationLoadShaderFromMemory(model_vertex_shader, model_fragment_shader);
    if (g_model_shader.id == 0) {
        fprintf(stderr, "Failed to load model shader: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // 2. Load Model (from .obj file)
    // This function is implied by the library structure. Implementation details may vary.
    // It likely parses the .obj, creates SituationMeshes, loads associated textures.
    const char* model_path = "assets/models/cube.obj"; // Example path
    g_loaded_model = SituationLoadModelFromObj(model_path); // Hypothetical function
    if (g_loaded_model.id == 0 || g_loaded_model.mesh_count == 0) {
         fprintf(stderr, "Failed to load model from %s: %s\n", model_path, SituationGetLastErrorMsg());
         return -1;
    }
    // Assume the model loader also loads textures into g_loaded_model.all_model_textures
    // For simplicity, bind the first one if it exists.
    if (g_loaded_model.texture_count > 0) {
        g_diffuse_texture = g_loaded_model.all_model_textures[0];
    }

    return 0;
}

// --- Cleanup ---
void cleanup_model_resources() {
     // The SituationUnloadModel function handles the destruction of all associated resources,
     // including all sub-meshes and textures, in a single, safe call.
     if (g_loaded_model.id != 0) {
         SituationUnloadModel(&g_loaded_model);
     }

     // The shader is a separate resource and must be unloaded independently.
     if (g_model_shader.id != 0) {
         SituationUnloadShader(&g_model_shader);
     }
}


// --- Rendering the Model ---
void render_model() {
     SituationCommandBuffer cmd = SituationAcquireFrameCommandBuffer();
     if (cmd == SITUATION_CMD_BUFFER_INVALID) {
         fprintf(stderr, "Failed to begin frame: %s\n", SituationGetLastErrorMsg());
         return;
     }

     SituationCmdBindPipeline(cmd, g_model_shader);

     // Simple camera/view setup (could be more complex)
     mat4 view, projection, model;
     glm_mat4_identity(view);
     glm_translate(view, (vec3){0.0f, 0.0f, -3.0f}); // Move camera back
     glm_perspective(glm_rad(45.0f), (float)sit_gs.main_window_width / (float)sit_gs.main_window_height, 0.1f, 100.0f, projection);
     glm_mat4_identity(model);
     // glm_rotate(model, (float)SituationTimerGetTime(), (vec3){0.0f, 1.0f, 0.0f}); // Rotate over time

     // Update view/projection (library likely handles this internally per frame, but push constants need model)
     // Bind model matrix via push constant
     SituationCmdSetPushConstant(cmd, 0, model, sizeof(mat4)); // Assuming binding 0 in shader

     // Bind texture (assuming slot 1 in shader)
     if (g_diffuse_texture.id != 0) {
         SituationCmdBindTexture(cmd, g_diffuse_texture, 1);
     }

     // Draw each mesh in the model
     for (int i = 0; i < g_loaded_model.mesh_count; ++i) {
         SituationCmdBindVertexBuffer(cmd, g_loaded_model.meshes[i].vertex_buffer);
         if (g_loaded_model.meshes[i].index_buffer.id != 0) {
             SituationCmdBindIndexBuffer(cmd, g_loaded_model.meshes[i].index_buffer);
             SituationCmdDrawIndexed(cmd, g_loaded_model.meshes[i].index_count, 1, 0, 0, 0);
         } else {
             SituationCmdDraw(cmd, g_loaded_model.meshes[i].vertex_count, 1, 0, 0);
         }
     }

     SituationEndFrame(cmd);
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - 3D Model";
    init_info.window_width = 1024;
    init_info.window_height = 768;

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    if (init_model_resources() != 0) {
        SituationShutdown();
        return -1;
    }

    printf("Model loaded. Running...\n");

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        render_model();
    }

    cleanup_model_resources();
    SituationShutdown();
    return 0;
}
```

### 6.3 Playing Background Music
This example demonstrates how to load and play a sound file (e.g., WAV, OGG) in a continuous loop using the audio capabilities of `situation.h`.
```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Backend choice doesn't affect audio directly, but needed for init
#include "situation.h"
#include <stdio.h>

// --- Global Sound Handle ---
static SituationSound g_background_music = {0};

// --- Initialization ---
int init_audio() {
    // Audio system is initialized within SituationInit.
    // Just need to load and play the sound.

    const char* music_path = "assets/audio/background.ogg"; // Example path

    // Correct API: Pass a pointer to the sound struct and specify looping.
    SituationError err = SituationLoadSoundFromFile(music_path, true, &g_background_music);

    if (err != SITUATION_SUCCESS) {
        // It's often acceptable for audio to fail without crashing the whole app.
        fprintf(stderr, "Warning: Failed to load background music (%s): %s\n", music_path, SituationGetLastErrorMsg());
        // Return 0 to continue, as music might not be critical.
    } else {
        printf("Background music loaded successfully.\n");
        // Play the sound. Since we loaded it with looping=true, it will repeat automatically.
        SituationPlayLoadedSound(&g_background_music);
    }
    return 0; // Continue even if music fails
}

// --- Cleanup ---
void cleanup_audio() {
    if (g_background_music.is_initialized) {
        // Ensure it's stopped before destroying
        SituationStopLoadedSound(&g_background_music);
        SituationUnloadSound(&g_background_music);
    }
}

// --- Main Application Entry (Simple Loop with Audio) ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Audio Test";
    init_info.window_width = 800;
    init_info.window_height = 600;

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    if (init_audio() != 0) {
         // Handle critical audio init failure if applicable
    }

    printf("Audio playing (if loaded). Running... Close window to stop.\n");

    // --- Main Loop ---
    while (!SituationWindowShouldClose()) {
        // 1. Poll for OS events (keyboard, mouse, window resize, etc.)
        SituationPollInputEvents();

        // 2. Update internal timers (important for audio stream processing)
        SituationUpdateTimers();

        // --- Render an empty frame to keep the window responsive ---
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            // Begin a render pass and clear the screen to a pleasant color
            SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 40, 40, 60, 255 });
            SituationCmdEndRender(cmd);

            // Submit the frame to the GPU
            SituationEndFrame();
        }
    }

    cleanup_audio();
    SituationShutdown();
    return 0;
}
```

### 6.4 Handling Keyboard and Mouse Input
This example shows how to query the state of keyboard keys and the mouse position within the main application loop, and how to use this input to control simple application behavior (e.g., moving an on-screen element or closing the window).
```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or VULKAN
#include "situation.h"
#include <stdio.h>
#include <cglm/cglm.h> // For vec2, vec3, mat4

// --- Simple State for an On-Screen Element ---
static vec2 g_element_position = {400.0f, 300.0f}; // Start in the middle
static float g_element_speed = 200.0f; // Pixels per second

// --- Input Handling Logic ---
void process_input(float delta_time) {
    // --- Keyboard Input for Movement (State Polling) ---
    // Using SituationIsKeyDown checks if a key is *currently* held down,
    // which is perfect for continuous movement.
    const float velocity = g_element_speed * delta_time;

    if (SituationIsKeyDown(SIT_KEY_W) || SituationIsKeyDown(SIT_KEY_UP))    { g_element_position[1] -= velocity; }
    if (SituationIsKeyDown(SIT_KEY_S) || SituationIsKeyDown(SIT_KEY_DOWN))  { g_element_position[1] += velocity; }
    if (SituationIsKeyDown(SIT_KEY_A) || SituationIsKeyDown(SIT_KEY_LEFT))  { g_element_position[0] -= velocity; }
    if (SituationIsKeyDown(SIT_KEY_D) || SituationIsKeyDown(SIT_KEY_RIGHT)) { g_element_position[0] += velocity; }

    // --- Mouse Input (Event Polling) ---
    // Using SituationIsMouseButtonPressed checks if a button was pressed *this frame*,
    // which is ideal for single-trigger actions.
    if (SituationIsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        // On click, instantly move the element to the mouse cursor's position.
        vec2 mouse_pos = SituationGetMousePosition();
        glm_vec2_copy(mouse_pos, g_element_position);
    }
}

// --- Rendering the Element (Simple Quad) ---
// This function now uses the high-level SituationCmdDrawQuad helper.
void render_element(SituationCommandBuffer cmd) {
    // Create a model matrix for the quad based on its current position.
    mat4 model;
    glm_mat4_identity(model);
    // The quad's origin is its top-left, so we translate to center it on the position.
    glm_translate(model, (vec3){g_element_position[0] - 25.0f, g_element_position[1] - 25.0f, 0.0f});
    glm_scale(model, (vec3){50.0f, 50.0f, 1.0f}); // A 50x50 pixel quad

    // Define the color for the quad (white).
    vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

    // Issue the high-level draw command.
    SituationCmdDrawQuad(cmd, model, color);
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Input Handling";
    init_info.window_width = 800;
    init_info.window_height = 600;

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    printf("Use WASD/Arrow Keys to move. Click to teleport. Close window to quit.\n");

    while (!SituationWindowShouldClose()) {
        // 1. Poll for OS events (updates keyboard and mouse state)
        SituationPollInputEvents();

        // 2. Update internal timers to calculate delta time for this frame
        SituationUpdateTimers();
        float delta_time = SituationGetFrameTime(); // Use the official API getter

        // 3. Update application logic based on input
        process_input(delta_time);

        // 4. Render the frame
        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });

            // Call our rendering function for the element
            render_element(cmd);

            SituationCmdEndRender(cmd);
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
```
### 6.5 Compute Shader Example: Image Processing (SSBO Version - Updated for Persistent Descriptor Sets)
This example demonstrates using compute shaders with `situation.h` to perform a simple image processing task (inverting colors) by reading from and writing to Shader Storage Buffer Objects (SSBOs). This approach uses the confirmed API function `SituationCmdBindComputeBuffer`, which now correctly implements the high-performance, persistent descriptor set model for Vulkan.

#### 6.5.1 Problem Definition (Updated)
We want to take the pixel data of an image (already loaded into a `SituationBuffer` configured as an SSBO) and invert its colors using the GPU compute shader. The result will be written to another `SituationBuffer`.
**Crucial Note on Performance:** The library now ensures that binding these buffers for the compute shader is highly efficient. When a `SituationBuffer` is created using `SituationCreateBuffer`, the Vulkan backend internally:
1.  Allocates a `VkBuffer` and `VmaAllocation`.
2.  **Crucially:** Allocates a *persistent* `VkDescriptorSet` from a dedicated pool (`sit_gs.vk.persistent_descriptor_pool`).
3.  Immediately populates this descriptor set with the buffer's `VkBuffer` handle.
4.  Stores this `VkDescriptorSet` within the `SituationBuffer` struct (`buffer.descriptor_set`).
This means that subsequent binding operations are extremely fast, as they do not involve any runtime allocation or update of descriptor sets.

#### 6.5.2 Compute Shader Code (GLSL using SSBOs)
The shader reads RGBA values from an input SSBO, inverts them, and writes the result to an output SSBO. Each invocation processes one pixel.
```glsl
#version 450

// Define the structure for a pixel (RGBA)
struct Pixel {
    uint rgba; // Pack R, G, B, A into a single uint for efficient storage/bandwidth
};

// Input SSBO (binding point 0)
layout(std430, binding = 0) restrict readonly buffer InputBuffer {
    Pixel pixels[];
} input_buffer;

// Output SSBO (binding point 1)
layout(std430, binding = 1) restrict writeonly buffer OutputBuffer {
    Pixel pixels[];
} output_buffer;

// Total number of pixels (passed via push constant)
layout(push_constant) uniform PushConstants {
    uint pixel_count;
} pc;

// Helper function to pack float components (0.0-1.0) into a uint RGBA
uint packRGBA(float r, float g, float b, float a) {
    uint ir = uint(r * 255.0);
    uint ig = uint(g * 255.0);
    uint ib = uint(b * 255.0);
    uint ia = uint(a * 255.0);
    return (ia << 24) | (ib << 16) | (ig << 8) | ir;
}

// Helper function to unpack a uint RGBA into float components (0.0-1.0)
void unpackRGBA(uint packed, out float r, out float g, out float b, out float a) {
    r = float(packed & 0xFF) / 255.0;
    g = float((packed >> 8) & 0xFF) / 255.0;
    b = float((packed >> 16) & 0xFF) / 255.0;
    a = float((packed >> 24) & 0xFF) / 255.0;
}

void main() {
    uint index = gl_GlobalInvocationID.x;

    // Boundary check
    if (index >= pc.pixel_count) {
        return;
    }

    // Read packed pixel data
    uint input_packed = input_buffer.pixels[index].rgba;

    // Unpack
    float r, g, b, a;
    unpackRGBA(input_packed, r, g, b, a);

    // Invert colors (keep alpha)
    float inv_r = 1.0 - r;
    float inv_g = 1.0 - g;
    float inv_b = 1.0 - b;
    // a = a; // Alpha unchanged

    // Pack inverted colors
    uint output_packed = packRGBA(inv_r, inv_g, inv_b, a);

    // Write to output buffer
    output_buffer.pixels[index].rgba = output_packed;
}
```

#### 6.5.3 Host Code Walkthrough (Init, Create, Bind Buffers, Dispatch, Sync, Destroy)
This C code shows how to prepare data, create buffers, load the shader, bind resources, dispatch the compute job, synchronize, and clean up using the *existing* `situation.h` API.
```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER // Required for compute shaders
#include "situation.h"
#include <stdio.h>
#include <stdlib.h> // for malloc/free
#include <string.h> // for memset

// --- Global Handles ---
static SituationComputePipeline g_invert_pipeline = {0};
static SituationBuffer g_input_ssbo = {0};
static SituationBuffer g_output_ssbo = {0};
static uint32_t g_pixel_count = 0; // Store the number of pixels for push constant and dispatch

// --- GLSL Compute Shader Source (from 6.5.2) ---
static const char* invert_compute_shader_source = "/* ... (Insert the GLSL code from 6.5.2 here) ... */";

// --- Helper Function to Load Image Data into a Buffer (Pseudo-code) ---
// This function is application-specific. It should load image data (e.g., RGBA8)
// into a host memory buffer. Returns the number of pixels loaded.
// You might use stb_image.h or another library for this.
uint32_t load_image_data_into_host_buffer(const char* filename, uint8_t** out_data) {
    // Example using stb_image (conceptual)
    /*
    int width, height, channels;
    *out_data = stbi_load(filename, &width, &height, &channels, 4); // Force RGBA
    if (*out_data == NULL) {
        fprintf(stderr, "Failed to load image: %s\n", filename);
        return 0;
    }
    return width * height; // Return pixel count
    */
    // Placeholder implementation
    fprintf(stderr, "load_image_data_into_host_buffer is not implemented.\n");
    *out_data = NULL;
    return 0;
}

// --- Initialization ---
int init_compute_resources() {
    // 1. Load Compute Shader
    g_invert_pipeline = SituationCreateComputePipelineFromMemory(invert_compute_shader_source);
    if (g_invert_pipeline.id == 0) {
        fprintf(stderr, "Failed to create compute pipeline: %s\n", SituationGetLastErrorMsg());
        return -1;
    }
    printf("Compute pipeline created successfully.\n");

    // 2. Prepare Image Data (Host Side)
    uint8_t* image_data = NULL;
    g_pixel_count = load_image_data_into_host_buffer("assets/textures/input_image.png", &image_data);
    if (g_pixel_count == 0 || image_data == NULL) {
        fprintf(stderr, "Failed to load image data.\n");
        return -1; // Or handle gracefully
    }
    size_t image_data_size = g_pixel_count * sizeof(uint32_t); // 4 bytes per pixel (RGBA8 packed)

    // 3. Create Input SSBO
    // Usage flags indicate it will be used as a storage buffer by the compute shader for reading.
    g_input_ssbo = SituationCreateBuffer(image_data_size, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, image_data);
    if (g_input_ssbo.id == 0) {
        fprintf(stderr, "Failed to create input SSBO: %s\n", SituationGetLastErrorMsg());
        free(image_data); // Clean up host data
        return -1;
    }
    printf("Input SSBO created successfully.\n");

    // 4. Create Output SSBO
    // Usage flags indicate it will be used as a storage buffer by the compute shader for writing.
    // Initial data is NULL as it will be filled by the compute shader.
    g_output_ssbo = SituationCreateBuffer(image_data_size, SITUATION_BUFFER_USAGE_STORAGE_BUFFER, NULL);
    if (g_output_ssbo.id == 0) {
        fprintf(stderr, "Failed to create output SSBO: %s\n", SituationGetLastErrorMsg());
        free(image_data); // Clean up host data
        return -1;
    }
    printf("Output SSBO created successfully.\n");

    free(image_data); // Clean up host data, it's now on the GPU
    return 0; // Success
}

// --- Cleanup ---
void cleanup_compute_resources() {
    if (g_output_ssbo.id != 0) {
        SituationDestroyBuffer(&g_output_ssbo);
    }
    if (g_input_ssbo.id != 0) {
        SituationDestroyBuffer(&g_input_ssbo);
    }
    if (g_invert_pipeline.id != 0) {
        SituationDestroyComputePipeline(&g_invert_pipeline);
    }
    printf("Compute resources cleaned up.\n");
}

// --- Run Compute Shader (Updated Section) ---
void run_compute_shader() {
    if (g_pixel_count == 0) {
        fprintf(stderr, "Pixel count is zero, cannot dispatch compute shader.\n");
        return;
    }

    // 1. Begin Frame/Obtain Command Buffer
    // (Assuming standard library flow)
    SituationCommandBuffer cmd = SituationAcquireFrameCommandBuffer();
    if (cmd == SITUATION_CMD_BUFFER_INVALID) {
         fprintf(stderr, "Failed to begin frame for compute: %s\n", SituationGetLastErrorMsg());
         return;
    }

    // 2. Bind the Compute Pipeline
    // This makes the compute shader active for subsequent dispatches.
    SituationCmdBindComputePipeline(cmd, g_invert_pipeline);

    // 3. Bind Input SSBO to binding point 0 (HIGH-PERFORMANCE using Persistent Descriptor Set)
    // --------------------------------------------------------------------------------------------
    // OLD (Inefficient) WAY (DO NOT USE - This is what the old docs implied):
    //   - Dynamically allocate a new VkDescriptorSet.
    //   - Update it with g_input_ssbo's VkBuffer.
    //   - Bind the temporary set.
    //   - (Repeat for every frame/bind call - SLOW!)
    //
    // NEW (Efficient) WAY (This is how it actually works now):
    //   - The `g_input_ssbo.descriptor_set` was created and populated ONCE during `SituationCreateBuffer`.
    //   - `SituationCmdBindComputeBuffer` simply records a FAST `vkCmdBindDescriptorSets` command
    //     using the pre-cached `g_input_ssbo.descriptor_set`.
    // --------------------------------------------------------------------------------------------
    SituationCmdBindComputeBuffer(cmd, 0, g_input_ssbo); // Binding point 0

    // 4. Bind Output SSBO to binding point 1 (HIGH-PERFORMANCE)
    SituationCmdBindComputeBuffer(cmd, 1, g_output_ssbo); // Binding point 1

    // 5. Set Push Constant (pixel count)
    SituationCmdSetPushConstant(cmd, 0, &g_pixel_count, sizeof(g_pixel_count));

    // 6. Dispatch Compute Work
    // We dispatch one invocation per pixel.
    SituationCmdDispatch(cmd, g_pixel_count, 1, 1);

    // 7. Synchronize
    // Ensure compute shader finishes writing to g_output_ssbo before anything tries to read it.
    SituationMemoryBarrier(SITUATION_BARRIER_COMPUTE_SHADER_STORAGE_WRITE);

    // 8. End Frame/Submit Commands
    SituationEndFrame();
    printf("Compute shader dispatched and synchronized.\n");
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Compute Shader SSBO Example";
    init_info.window_width = 800;
    init_info.window_height = 600;
    // Enable Vulkan validation if using Vulkan for better debugging
    // init_info.enable_vulkan_validation = true;

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    if (init_compute_resources() != 0) {
        SituationShutdown();
        return -1;
    }

    printf("Compute resources initialized. Running compute shader once...\n");

    // Run the compute shader
    run_compute_shader();

    printf("Compute shader run complete. Check output buffer (implementation-dependent).\n");
    printf("Press Enter to exit...\n");
    getchar(); // Wait for user input

    cleanup_compute_resources();
    SituationShutdown();
    printf("Application shutdown complete.\n");
    return 0;
}
```

### 6.6 Example: GPU Particle Simulation and Rendering (Concept)
This example concept demonstrates a fundamental and powerful technique: combining compute and graphics pipelines within a single frame. It illustrates how to use a compute shader to update simulation data (like particle positions and velocities) stored in GPU buffers, and then immediately use a standard graphics pipeline to render the results in the same frame.

#### 6.6.1 Scenario
The core idea is to perform calculations on the GPU (using a compute shader) and then visualize the results (using a graphics shader) without stalling the pipeline or introducing race conditions.

1. **Compute Shader:** A compute shader operates on a buffer of particle data (e.g., struct { vec2 position; vec2 velocity; }). It reads the current state,
applies simulation logic (e.g., physics updates like velocity integration, applying forces), and writes the new state back to the same or a different buffer.
2. **Graphics Shader:** A vertex shader (potentially using instancing) reads the updated particle data from the buffer and uses it to position geometry
(e.g., a quad or sprite) for each particle on the screen.
3. **Synchronization:** The critical aspect is ensuring the compute shader's writes are globally visible and finished before the vertex shader attempts
to read that data. This requires explicit synchronization.

#### 6.6.2 Key APIs Demonstrated
This example concept highlights the interaction between several situation.h APIs:

- **SituationCreateBuffer / SituationDestroyBuffer:** Used to create the GPU buffer(s) that will store the particle simulation data (positions, velocities).
These buffers must be created with appropriate usage flags (e.g., SITUATION_BUFFER_USAGE_STORAGE_BUFFER for compute read/write, potentially
SITUATION_BUFFER_USAGE_VERTEX_BUFFER if used as such in the graphics pipeline, or bound via SituationCmdBindUniformBuffer if accessed as an SSBO).
- **SituationCreateComputePipelineFromMemory / SituationDestroyComputePipeline:**
Used to create the compute pipeline that will execute the particle update logic.
- **SituationCmdBindComputePipeline:** Binds the compute pipeline for subsequent dispatch commands.
- **SituationCmdBindComputeBuffer:** Binds the particle data buffer to a specific binding point within the compute shader's descriptor set.
- **SituationCmdDispatch:** Launches the compute shader work groups to perform the particle simulation update.
- **SituationMemoryBarrier:** Crucially, this function is used after the compute dispatch and before the graphics draw call. It inserts a memory and execution
barrier to ensure all compute shader invocations have completed their writes (SITUATION_BARRIER_COMPUTE_SHADER_STORAGE_WRITE) and that these writes are
visible to the subsequent graphics pipeline stages that will read the data (SITUATION_BARRIER_VERTEX_SHADER_STORAGE_READ or similar). Without this
barrier, the graphics pipeline might read stale or partially updated data.
- **SituationCmdBindPipeline (Graphics):** Binds the graphics pipeline used for rendering the particles.
- **SituationCmdBindVertexBuffer / SituationCmdBindIndexBuffer:** Binds the mesh data (e.g., a simple quad) used for instanced rendering of particles.
- **SituationCmdBindUniformBuffer / SituationCmdBindTexture:** Binds resources needed by the graphics shaders (e.g., the particle data buffer if accessed as an SSBO, textures for particle appearance).
- **SituationCmdDrawIndexedInstanced / SituationCmdDrawInstanced:** Renders the particle geometry, typically using instancing where the instance count equals
the number of particles, and the instance ID is used in the vertex shader to fetch data from the particle buffer.

#### 6.6.3 Purpose
This conceptual example should clarify the intended workflow for integrating compute-generated data into subsequent graphics rendering passes.
It emphasizes the essential role of SituationMemoryBarrier for correctness when sharing data between different pipeline types within the same command stream.
This bridges the gap between the existing separate compute and graphics examples, showing how they can be combined effectively.
</details>

---
<details>
<summary><h2>7. Frequently Asked Questions (FAQ) & Troubleshooting</h2></summary>

### 7.1 Common Initialization Failures
- **GLFW Init Failed:** Check GLFW installation, system libraries (X11 on Linux).
- **OpenGL Loader Failed:** Ensure `GLAD` is compiled and linked correctly when using `SITUATION_USE_OPENGL`.
- **Vulkan Instance/Device Failed:** Verify Vulkan SDK installation, compatible driver. Enable validation layers (`init_info.enable_vulkan_validation = true;`) for detailed errors.
- **Audio Device Failed:** Check system audio settings, permissions.

### 7.2 "Resource Invalid" Errors
- Occur when trying to use a resource handle (Shader, Mesh, Texture, Buffer, ComputePipeline) that hasn't been created successfully (`id == 0`) or has already been destroyed.

### 7.3 Performance Considerations
- Minimize state changes (binding different shaders, textures) within a single command buffer recording.
- Batch similar draw calls.
- Use `SituationDrawTextStyled` instead of `SituationDrawTextSimple` for significant text rendering.
- Profile your application to identify bottlenecks.

### 7.4 Backend-Specific Issues (OpenGL vs. Vulkan)
- OpenGL might be easier to set up initially but can have driver-specific quirks.
- Vulkan offers more explicit control and potentially better performance but has a steeper learning curve and more verbose setup.

### 7.5 Debugging Tips (Validation Layers, Error Messages)
- Always check the return value of `SituationInit` and resource creation functions.
- Use `SituationGetLastErrorMsg()` to get detailed error descriptions.
- For Vulkan, enable validation layers during development (`init_info.enable_vulkan_validation = true;`) to catch API misuse.
</details>

---
## License (MIT)
Copyright (c) 2025 Jacques Morel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.