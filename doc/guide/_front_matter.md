# Situation — Advanced Platform Awareness, Control, and Timing

_Core API library v2.4.403 · (c) 2025-2026 Jacques Morel · MIT Licensed_

**Situation** is a **strict C11** single-file library providing unified access to windowing, graphics (OpenGL 4.6 / Vulkan 1.4), audio (23-effect node graph, 16-voice MIDI synth), input, filesystem, NUMA-aware threading, and high-resolution timing. One header, one DLL, one `SituationInit()` call — then build games, creative tools, simulations, or visualizations without fighting platform boilerplate.

Ships as header-only or pre-built DLL with auto-generated FFI bindings for **Odin**, **Zig**, and **Rust** (`wrappers/`).

> **581** public `SITAPI` functions · Windows 10+ · OpenGL 4.6 or Vulkan 1.4 hardware required
>
> **Documentation:**
> - **[situation_api.md](../situation_api.md)** — module map linking to all **`doc/guide/`** sections
> - **[situation_api_index.md](../situation_api_index.md)** — categorized function index (auto-generated)
> - **[situation_command_reference.md](../situation_command_reference.md)** — all `SituationCmd*` rendering commands
> - **[situation_sdk.md](../situation_sdk.md)** — SDK manual (architecture, workflows, examples)
> - **[whatsnew.md](../whatsnew.md)** / **[UPDATELOG.md](../UPDATELOG.md)** — release history

---

For release history and changelogs, see **[`doc/whatsnew.md`](../whatsnew.md)** and **[`doc/UPDATELOG.md`](../UPDATELOG.md)**.

---

# Situation v2.4.399 API Programming Guide

**Situation** is a single-file C11 library (MIT Licensed) that provides unified, low-level access to windowing, graphics, audio, input, filesystem, threading, and timing from a single `#include`. It targets modern hardware — OpenGL 4.6, Vulkan 1.4, Windows 10+ — and ships as a pre-built DLL or a header-only integration, with auto-generated FFI bindings for Odin, Zig, and Rust (see [§2.3.1 Language Wrappers](#231-language-wrappers-odin-zig-rust) and [COMPILATION_GUIDE.md](../COMPILATION_GUIDE.md)).

The library delivers a complete application foundation in one coherent API surface (540 public functions): GPU rendering through a unified command-buffer model, a 23-effect audio node graph with 16-voice polyphonic MIDI synthesis, a generational dual-queue job system with NUMA-aware scheduling, async file I/O, high-resolution temporal oscillators, and hot-reloadable assets — all orchestrated from a single `SituationInit()` call.

This guide is the primary technical reference for the public API. For architecture deep-dives, see the [SDK manual](../situation_sdk.md). For changelogs, see [`whatsnew.md`](../whatsnew.md).

---

---

## Introduction and Core Concepts

This section explains the fundamental concepts, design philosophy, and core architectural patterns of the "Situation" library. A solid understanding of these ideas is crucial for using the library effectively.

### 1. Core Philosophy and Design
#### Immediate Mode and Explicit Control
The library favors a mostly **"immediate mode"** style API. This means that for many operations, you call a function and it takes effect immediately within the current frame. For example, `SituationCmdDrawQuad()` directly records a draw command into the current frame's command buffer. This approach is designed to be simple, intuitive, and easy to debug, contrasting with "retained mode" systems where you would build a scene graph of objects that is then rendered by the engine.

Complementing this is a philosophy of **explicit resource management**. Any resource you create (a texture, a mesh, a sound) must be explicitly destroyed by you using its corresponding `SituationDestroy...()` or `SituationUnload...()` function. This design choice avoids the complexities and performance overhead of automatic garbage collection and puts you in full control of resource lifecycles. To aid in debugging, the library will warn you at shutdown if you've leaked any GPU resources.

#### C-Style, Single-File, and Backend-Agnostic Architecture
- **C-Style, Data-Oriented API:** The API is pure C, promoting maximum portability and interoperability. It uses handles (structs passed by value) to represent opaque resources and pointers for functions that need to modify or destroy those resources. This approach is data-oriented, focusing on transforming data (e.g., vertex data into a mesh, image data into a texture) rather than on object-oriented hierarchies.
- **Single-File, Header-Only Distribution:** "Situation" is distributed as a single header file (`situation.h`), making it incredibly easy to integrate into your projects. To use it, you simply `#include "situation.h"` in your source files. In exactly one C or C++ file, you must first define `SITUATION_IMPLEMENTATION` before the include to create the implementation.
- **Backend Abstraction:** The library provides a unified API that works over different graphics backends (currently OpenGL and Vulkan). You choose the backend at compile time by defining either `SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`.

#### Threading Model
The library internally manages multiple dedicated threads: **Main** (lifecycle, input, frame submission), **Render** (GL/VK context owner), **Audio** (miniaudio callback), and **I/O** (async file loading, hot-reload polling), plus a configurable **worker pool** for user-submitted parallel jobs. The worker pool features dual-priority queues (high for physics/logic, low for assets/I/O), generational O(1) job tracking, fork-join parallelism, NUMA-aware thread placement, and CPU topology-driven affinity. All library threads are named at the OS level for debugger visibility.

**All public SITAPI function calls must originate from the main thread** — the thread that called `SituationInit()`. This is a call-site discipline common in game engines (similar to Unity's game thread constraint), not a single-threaded limitation. The actual work is parallelized across the dedicated threads and worker pool internally.

#### Error Handling
As of v2.4.201+, all fallible public functions return `SituationError`. Check against `SITUATION_SUCCESS`, or use `SituationGetLastErrorMsg()` / `SituationGetLastErrorCode()` for diagnostics after any failure. The error enum lives in `sit/situation_base_errno.h`.

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

A **Virtual Display** is an off-screen render target with a built-in compositor (integer scaling, z-order, blend modes). See **[Virtual Display Module](virtual_display.md)** for the full guide — retro CRT, PiP, 3D layers, and compute targets. For **cell-based playfields** (tile maps, stacked scrolling layers), see **[2D Grid Module](grid.md)**.

### 5. Other Key Systems
#### Audio: Node Graph, Sounds, and Synthesis
The audio subsystem operates at three levels:
-   **Loaded Sounds (`SituationLoadSoundFromFile`):** Decodes the entire audio file into memory. Ideal for short, low-latency sound effects.
-   **Streamed Sounds (`SituationLoadSoundFromStream`):** Decodes audio in small chunks as it plays. Uses less memory, ideal for music.
-   **Audio Node Graph:** A modular, graph-based processing architecture for arbitrary signal routing. Create typed nodes (tone synth, reverb, echo, chorus, filter, EQ, compressor, etc.), patch them together with `SituationCreatePatch()`, and set parameters with `SituationSetControl()`. The graph runs on the audio thread automatically once activated with `SituationSetActiveGraph()`.
-   **MIDI Synthesis:** The `SITUATION_NODE_TONE_SYNTH` is a 16-voice polyphonic synthesizer controllable via MIDI. Use the virtual MIDI loopback (`SituationSetupVirtualMidiLoopback`) to send note-on/off and CC messages programmatically, or connect hardware controllers via PortMidi.

#### Filesystem: Cross-Platform and Special Paths
The filesystem module abstracts away OS-specific differences. All paths are UTF-8. To ensure your application is portable, use the provided helper functions instead of hardcoding paths:
-   `SituationGetBasePath()`: Returns the directory containing your executable. Use this for loading application assets.
-   `SituationGetAppSavePath()`: Returns a platform-appropriate, user-specific directory for saving configuration files and user data.

#### The Temporal Oscillator System
This is a high-level timing utility for creating rhythmic, periodic events. You can initialize oscillators with specific periods (e.g., 0.5 seconds for 120 BPM). The library updates these timers independent of the frame rate, allowing you to easily synchronize animations, game logic, or visual effects to a steady, musical beat using functions like `SituationTimerHasOscillatorUpdated()`.

---

## Building the Library

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

**C) Static library (recommended for portable exes):**
- Build once: `build_situation.bat static-opengl` or `static-vulkan` → `build/dll/situation_*.a`
- Link the `.a` with GLFW and system libs (no DLL at runtime). See **[COMPILATION_GUIDE.md](../COMPILATION_GUIDE.md)** and `build_examples.bat`.

### 2.2 Project Structure Recommendations

**Situation Library v2.4.0 Structure:**
```
situation/                         # Situation library root
├── situation.h                    # ← Public API entry point (include this)
│
├── sit/                          # ← Core implementation (internal)
│   ├── situation_api.h           # Public API declarations
│   ├── situation_impl.h          # Core implementation
│   ├── situation_impl_audio.h    # Audio subsystem
│   │
│   ├── aud/                      # Audio Subsystem
│   │   ├── fx/                   # Effects (16 files)
│   │   │   ├── reverb.h, echo.h, chorus_4stage.h
│   │   │   ├── filter.h, eq_4band.h, dynamics.h
│   │   │   └── ... (other effects)
│   │   │
│   │   ├── polysonix/            # Polyphonic synthesizer
│   │   │   ├── polysonix.h
│   │   │   ├── px_vm.h
│   │   │   └── ... (synth components)
│   │   │
│   │   ├── node_graph*.h         # Node graph system (5 files)
│   │   ├── device_*.h            # Device system (3 files)
│   │   ├── sound_source.h        # Audio file playback
│   │   ├── mic_capture.h         # Microphone capture
│   │   └── tone_synth.h          # Simple tone generator
│   │
│   └── k-term/                   # Terminal Subsystem
│       ├── kterm.h               # Main wrapper
│       ├── kterm_api.h           # Public API
│       └── ... (terminal components)
│
├── examples/                     # Example programs
├── wrappers/                     # Odin, Zig, Rust FFI + hello_situation demos
├── scripts/                      # Shared wrapper link helpers (wrapper_*.bat)
├── _languages/                   # Bundled Odin, Zig, Rust toolchains (Windows)
├── ext/                         # External dependencies
│   ├── glfw/                    # GLFW windowing library
│   ├── cglm/                    # Math library
│   ├── glad/                    # OpenGL loader (if using OpenGL)
│   └── stb/                     # STB libraries (image, truetype)
│
├── doc/                         # Documentation
└── shaders/                     # Shader files
```

**Your Application Structure:**
```
your_project/
├── src/
│   ├── main.c              // Your application entry point
│   └── (other .c files)    // Your application logic
├── situation/              // Situation library (as shown above)
│   ├── situation.h         // Include this file
│   ├── sit/                // Internal implementation
│   └── ext/                // Dependencies
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

**Include Pattern:**
```c
// In your main.c or one implementation file:
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL  // or SITUATION_USE_VULKAN
#include "situation/situation.h"

// In other files, just include normally:
#include "situation/situation.h"
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

**Compilation Example (Windows/GCC):**
```bash
# OpenGL Backend
gcc -o myapp.exe src/main.c \
    -Isituation -Isituation/ext -Isituation/ext/glfw/include \
    -DSITUATION_USE_OPENGL \
    -Lsituation/ext/glfw/lib-mingw-w64 \
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32

# Vulkan Backend
gcc -o myapp.exe src/main.c \
    -Isituation -Isituation/ext -Isituation/ext/glfw/include \
    -DSITUATION_USE_VULKAN \
    -DSITUATION_ENABLE_SHADER_COMPILER \
    -Lsituation/ext/glfw/lib-mingw-w64 -L%VULKAN_SDK%/Lib \
    -lglfw3 -lvulkan-1 -lshaderc_shared -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32
```

**See Also:** `doc/COMPILATION_GUIDE.md` for comprehensive platform-specific instructions.

#### 2.3.1 Language Wrappers (Odin, Zig, Rust)

Auto-generated bindings live under `wrappers/`. Regenerate after API changes with `python tools/generate_<lang>_bindings.py`.

Wrapper example builders use the same backends as `build_examples.bat`:

```bat
build_odin_example.bat  opengl [hello_situation]
build_zig_example.bat     vulkan [hello_situation]
build_rust_example.bat    static-opengl [hello_situation]
```

Backends: `opengl`, `vulkan`, `static-opengl`, `static-vulkan`. Full details: **[COMPILATION_GUIDE.md](../COMPILATION_GUIDE.md)** → *Language Wrapper Examples*.

### 2.4 Build & Feature Defines
This section details the preprocessor defines that control the library's features and build configuration.

#### Backend Selection
- **`SITUATION_USE_OPENGL`**: Enables the modern OpenGL backend. Must be defined before including `situation.h`.
- **`SITUATION_USE_VULKAN`**: Enables the explicit Vulkan backend. Must be defined before including `situation.h`.

#### Shared Library Support
- **`SITUATION_BUILD_SHARED`**: Must be defined when compiling the library as a shared object (DLL). This controls symbol visibility for export.
- **`SITUATION_USE_SHARED`**: Must be defined in the application code when linking against the shared library to control symbol import.

#### Feature Enablement
- **`SITUATION_ENABLE_SHADER_COMPILER`**: Mandatory for using compute shaders with the Vulkan backend as it enables runtime compilation of GLSL to SPIR-V.

#### Build-Flag Defaults
- **`SITUATION_WORKER_NUMA_SPREAD_DEFAULT`**: Controls the default value of `SituationInitInfo.worker_numa_spread`. When `SITUATION_ENABLE_THREADING` is defined, this defaults to `1` (workers are spread across NUMA nodes at pool startup). Define as `0` before including `situation.h` to disable NUMA spreading by default. When threading is not enabled, this is always `0`.

---

## Getting Started

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
        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
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

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
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

---

## Module reference

Detailed API documentation lives under **`doc/guide/`**. The umbrella index is **[situation_api.md](../situation_api.md)**.

| Category | Guides |
|----------|--------|
| **Core systems** | [Core](core.md) · [Window & display](window_display.md) · [Input](input.md) · [Image](image.md) · [Fonts](font.md) · [System introspection](system_introspection.md) |
| **Graphics** | [Graphics](graphics.md) · [Virtual Display](virtual_display.md) · [Advanced GPU commands](renderer_bolster.md) · [Compute](compute.md) · [2D Grid](grid.md) · [Fonts](font.md) · [Text rendering](text_rendering.md) · [2D drawing](drawing_2d.md) · [3D drawing](drawing_3d.md) |
| **Media & I/O** | [Audio](audio.md) · [Audio node graph](audio_graph.md) · [MIDI](midi.md) · [Filesystem](filesystem.md) |
| **Utilities** | [Threading](threading.md) · [YPQ color](ypq_color.md) · [HD color output](hd_color_output.md) · [Hot-reload](hot_reload.md) · [Logging](logging.md) · [Miscellaneous](miscellaneous.md) · [Deprecated APIs](deprecated.md) |
| **Learning** | [Examples & FAQ](examples_faq.md) |

---
