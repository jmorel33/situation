<div align="center">
  <img src="situation_blackMetal_logo.jpg" alt="Situation logo">
</div>

# The "Situation" Advanced Platform Awareness, Control, and Timing

_(c) 2025-2026 Jacques Morel — MIT Licensed_

Welcome to "Situation", a public API engineered for high-performance, cross-platform development. "Situation" is a single-file, cross-platform **[Strict C11 (ISO/IEC 9899:2011) Compliant](C11_Compliance_Report.md)** library providing unified, low-level access and control over essential application subsystems. Its purpose is to abstract away platform-specific complexities, offering a lean yet powerful API for building sophisticated, high-performance software. This library is designed as a foundational layer for professional applications, including but not limited to: real-time simulations, game engines, multimedia installations, and scientific visualization tools.

Our immediate development roadmap is focused on expanding the library's capability. See **[What's New](whatsnew.md)** for a full history of recent features, optimizations, and roadmap completions!

*   **Async Compute:** Exposing dedicated transfer and compute queues in Vulkan for non-blocking background operations.
*   **Built-in Debug Tools**: Leveraging internal profiling counters to render an immediate-mode performance overlay.
*   **Advanced Audio DSP**: Expanding the effects chain with user-definable graph routing.
*   **Cross-Platform Expansion**: Formalizing support for Android and WebAssembly targets.
*   **Web & Reach (Phase 4):** Full **Emscripten** (WASM) support and a **WebGPU (Dawn)** backend to bring Situation apps to the browser with near-native performance.

"Situation" is an ambitious project that aims to become a premier, go-to solution for developers seeking a reliable and powerful platform layer.

The library's philosophy is reflected in its name, granting developers complete situational **Awareness**, precise **Control**, and fine-grained **Timing**.

It provides deep **Awareness** of the host system through APIs for querying hardware **(GPU Name, VRAM)** and multi-monitor display information, and by handling operating system events like window focus and file drops.

This foundation enables precise **Control** over the entire application stack:

*   **Threading:** Generational dual-queue task system (mutex per queue, atomics for indices/job state): fork-join `SituationDispatchParallel`, high/low priority rings, backpressure (`RUN_IF_FULL` / `BLOCK_IF_FULL`), dedicated I/O thread, in-place job claim + full-queue HOL scan (v2.4.232–233), topology/affinity/NUMA placement, OS-visible thread names (`SituationSetCurrentThreadName`, v2.4.239), pool observability and scheduler metrics (v2.4.139+).
*   **Windowing:** Fullscreen, borderless, and HiDPI-aware window management with explicit **State Hardening** to prevent context poisoning from external middleware (e.g., ImGui).
*   **Input:** O(1) ring-buffered processing for Keyboard, Mouse, and Gamepad events ensures no input is ever lost during frame spikes.
*   **Audio:** **Node graph** output (**`SituationProcessGraph`**) plus **Snapshot-and-Unlock** mixing for loaded/streamed voices, zero-stall concurrency, safe RAM preloading via background threads (Async Load), disk streaming for music, and fused-loop real-time effects (Reverb, Delay, Filter).
*   **Graphics:** A unified command-buffer abstraction for **OpenGL 4.6** and **Vulkan 1.4**. It manages complex resources automatically, utilizing **Best-Fit Descriptor Recycling** and **Persistent Staging Rings** to eliminate fragmentation and allocation overhead. The OpenGL backend now features **Multi-Draw Indirect (MDI)** batching and **Bindless Textures** for console-like efficiency. It includes high-level utilities for **Compute Shaders**, **Virtual Display Compositing**, and high-quality text rendering powered by **Zero-Copy Ring Buffers**.
*   **Hot-Reloading:** A suite of tools for live-reloading assets (Shaders, Textures, Models) at runtime, safely handling GPU synchronization and resource rebuilding with **Debounced IO Polling** to prevent CPU storms.

Finally, its **Timing** capabilities range from high-resolution performance measurement **(FPS, Draw Calls, Latency Histograms)** and frame rate management to an advanced **Temporal Oscillator System** for creating complex, rhythmically synchronized events.

---

## Table of Contents

- [1. What is Situation?](#1-what-is-situation)
- [2. Getting Started](#2-getting-started)
- [3. Building & Configuration](#3-building--configuration)
    - [Language Wrappers (Odin, Zig, Rust)](#language-wrappers-odin-zig-rust)
- [4. Examples & Tutorials](#4-examples--tutorials)
- [5. FAQ & Troubleshooting](#5-frequently-asked-questions-faq--troubleshooting)
- [6. API Reference](#6-api-reference)
- [7. Version History](#7-version-history)

---

## 1. What is Situation?

`situation.h` is a single-header C/C++ library that acts as a high-performance kernel for interactive software. It abstracts the fragmented landscape of OS APIs (Windows/Linux/macOS) and Graphics Backends (OpenGL/Vulkan) into a unified, deterministic "Situation" that you control.

Unlike simple wrappers, Situation is an **opinionated micro-engine**. It enforces a strict separation of Update and Render phases to guarantee identical behavior across immediate-mode (OpenGL) and deferred-mode (Vulkan) drivers.

### Key Capabilities

*   **Unified Command Architecture:** Write your rendering code once using abstract `SituationCmd*` functions. The library compiles this into direct state changes for **OpenGL 4.6** or optimized command buffers for **Vulkan 1.4**.
*   **Generational Task System:** C11 thread pool with per-queue mutexes, generational job IDs, fork-join parallelism, priority queues, and optional CPU/NUMA pinning via `SituationInitInfo`.
*   **"Hardened" Audio Engine:** miniaudio drives the device; the callback mixes **active graph** processing, **voice snapshots**, and the **tone pool** into the output buffer. **Thread-safe asset loading** (decode SFX to RAM to avoid stalls), background music streaming, real-time DSP effects (Reverb/Delay), and low-latency microphone capture.
*   **Dynamic Resource Management:** No arbitrary limits. The Vulkan backend features a **Dynamic Descriptor Manager** with a linear allocation strategy that automatically grows resource pools as you load assets, supporting scenes with thousands of textures and buffers without fragmentation.
*   **O(1) Input System:** A lock-free, ring-buffered input architecture ensures that no keypress or mouse click is ever lost, even during frame-rate spikes.
*   **Virtual Display Compositor:** Render your game to low-resolution off-screen targets (e.g., 320x240) and composite them to the main screen with precise control over scaling algorithms (Integer, Fit, Stretch) and blend modes.
*   **First-Class Compute:** Compute Shaders are not an afterthought. The API treats Compute Pipelines and Storage Buffers (SSBOs) as primary citizens, enabling complex simulations and post-processing.
*   **Deep System Awareness:** Query precise hardware details (GPU Name, dedicated VRAM usage, Monitor topology) to auto-configure your application's quality settings.

> **CRITICAL ARCHITECTURAL NOTE:** To guarantee identical behavior between OpenGL (Immediate) and Vulkan (Deferred), developers must **update all buffer data before recording draw commands** within a frame. *The library actively enforces this rule in debug builds and will report a runtime error if violated.*

---

## 2. Getting Started

A minimal application requires **zero configuration** beyond selecting a backend.

1.  Download `situation.h` (and ensure stb headers are available if not using the bundled release).
2.  Create `main.c`. This example utilizes the new Task System to load music asynchronously while drawing text.

```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN            // Select Backend: VULKAN or OPENGL
#define SITUATION_ENABLE_THREADING      // Enable the new Task System
#define SITUATION_ENABLE_SHADER_COMPILER // Required for Vulkan Text/Quad rendering
#include "situation.h"

int main(int argc, char** argv) {
    // 1. Initialize with config
    SituationInitInfo config = { .window_width = 1280, .window_height = 720, .window_title = "Hello Situation" };
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;

    // 2. Create Generational Thread Pool (Auto-detect core count)
    SituationThreadPool pool;
    SituationCreateThreadPool(&pool, 0, 1024);

    // 3. Zero Friction Assets (Async)
    SituationSound music;
    // Decodes to RAM on background thread (Low Priority), zero main-thread stalls
    SituationLoadSoundFromFileAsync(&pool, "bgm.mp3", true, &music);

    SituationFont font;
    if (SituationLoadFont("font.ttf", &font) == SITUATION_SUCCESS) {
        SituationBakeFontAtlas(&font, 24.0f); // Create GPU texture for the font
    }

    // 4. Main Loop
    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME(); // Macro: Polls Input + Updates Timers

        // Example: Dispatch Physics in Parallel (High Priority)
        SituationDispatchParallel(&pool, 1000, 64, MyPhysicsCallback, NULL);

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

            // Clear screen to dark slate blue
            SituationRenderPassInfo pass = {
                .display_id = -1, // Main Window
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {20, 30, 40, 255} } }
            };

            SituationCmdBeginRenderPass(cmd, &pass);

            // Draw text directly using the internal batch renderer
            SituationCmdDrawText(cmd, font, "Situation Engine Running...", (Vector2){50, 50}, (ColorRGBA){255, 255, 255, 255});

            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    // 5. Cleanup (Automatic leak detection runs here)
    SituationDestroyThreadPool(&pool);
    SituationUnloadSound(&music);
    SituationUnloadFont(font);
    SituationShutdown();
    return 0;
}
```

---

## 3. Building & Configuration

"Situation" uses a **Header-Only + Implementation** pattern. Configuration is handled entirely via preprocessor macros, which must be defined **before** including `situation.h`.

See **[COMPILATION_GUIDE.md](COMPILATION_GUIDE.md)** for the full build system reference.

### Preprocessor Macros

| Macro | Type | Description |
| :--- | :--- | :--- |
| `SITUATION_IMPLEMENTATION` | **Required** | Define this in **exactly one** `.c` or `.cpp` file to compile the library's implementation code. |
| `SITUATION_USE_VULKAN` | Backend | Selects the **Vulkan 1.4** backend. Best for high-performance, multi-threaded asset loading, and modern GPU features. |
| `SITUATION_USE_OPENGL` | Backend | Selects the **OpenGL 4.6** backend using GLAD (included). Best for compatibility and smaller binary sizes. |
| `SITUATION_ENABLE_THREADING` | Feature | Enables the Generational Task System. Requires C11 support. |
| `SITUATION_ENABLE_SHADER_COMPILER` | Feature | Enables runtime GLSL → SPIR-V compilation. **Mandatory for Vulkan** if you wish to use the built-in Text or Virtual Display renderers. Requires linking `shaderc`. |
| `SITUATION_ENABLE_DXGI` | Feature | **(Windows Only)** Enables high-precision VRAM monitoring and GPU naming using the DXGI API. Requires linking `dxgi.lib` and `ole32.lib`. |
| `SITUATION_NO_STB` | Integration | Define this to disable embedded stb if your project already links these libraries to avoid symbol collisions. |

### Linker Requirements

| Platform | Standard Links | With `SITUATION_USE_VULKAN` | With `SITUATION_ENABLE_DXGI` |
| :--- | :--- | :--- | :--- |
| **Windows (MSVC/MinGW)** | `kernel32`, `user32`, `shell32`, `gdi32` | `vulkan-1.lib`, `shaderc_shared.lib` | `dxgi.lib`, `ole32.lib`, `shlwapi.lib` |
| **Linux (GCC/Clang)** | `-lm`, `-ldl`, `-lpthread`, `-lX11` | `-lvulkan`, `-lshaderc_shared` | N/A |
| **macOS (Clang)** | `-framework Cocoa`, `-framework IOKit` | `-lvulkan`, `-lshaderc_shared` | N/A |

> **Note:** If using `SITUATION_ENABLE_SHADER_COMPILER`, ensure the `shaderc` includes and libraries are in your compiler's search path.

---

### Language Wrappers (Odin, Zig, Rust)

Situation provides official FFI bindings and fully featured interactive examples for **Odin**, **Zig**, and **Rust** inside the `wrappers/` folder. The binding generators parse the C public headers automatically.

Wrapper example builders use the **same backends** as `build_examples.bat` and `build_tests.bat`:

| Backend | Prerequisite |
|---------|--------------|
| `opengl` | `build_situation.bat opengl` |
| `vulkan` | `build_situation.bat vulkan` |
| `static-opengl` | `build_situation.bat static-opengl` |
| `static-vulkan` | `build_situation.bat static-vulkan` |

```bat
build_odin_example.bat  [backend] [example_name]
build_zig_example.bat   [backend] [example_name]
build_rust_example.bat  [backend] [example_name]
```

`example_name` defaults to `hello_situation`. See `doc/COMPILATION_GUIDE.md` for per-language link details and shared `scripts/wrapper_*.bat` helpers.

#### Odin Wrapper
- **Odin Language**: [Odin Official Website](https://odin-lang.org/)
- **Source Files**: `wrappers/Odin/`
- **Binding Generator**: `python tools/generate_odin_bindings.py`
- **Build**:
  ```bat
  build_odin_example.bat opengl
  build_odin_example.bat static-opengl hello_situation
  ```
- **Output**: `build/examples/odin/`

#### Zig Wrapper
- **Zig Language**: [Zig Official Website](https://ziglang.org/)
- **Source Files**: `wrappers/Zig/`
- **Binding Generator**: `python tools/generate_zig_bindings.py`
- **Build**:
  ```bat
  build_zig_example.bat opengl
  build_zig_example.bat vulkan hello_situation
  ```
- **Output**: `build/examples/zig/`

#### Rust Wrapper
- **Rust Language**: [Rust Standalone Installers](https://forge.rust-lang.org/infra/other-installation-methods.html#standalone-installers) | [Rustup Installer](https://rustup.rs/)
- **Source Files**: `wrappers/Rust/`
- **Binding Generator**: `python tools/generate_rust_bindings.py`
- **Build**:
  ```bat
  build_rust_example.bat opengl
  build_rust_example.bat static-opengl hello_situation
  ```
- **Output**: `build/examples/rust/`

For `opengl` and `vulkan` backends, the matching `situation_*.dll` is copied next to the exe. Static backends produce self-contained exes with no DLL at runtime.

---

## 4. Examples & Tutorials

The repository includes a variety of examples demonstrating the library's features, from basic triangle rendering to more advanced topics like compute shaders and 3D model loading.

The full source code for all examples can be found in the `examples/` directory.

---

## 5. Frequently Asked Questions (FAQ) & Troubleshooting

### Configuration Settings (Preprocessor Macros)

"Situation" is configured via preprocessor definitions. You must define these **before** including `situation.h`.

| Macro | Description |
| :--- | :--- |
| `SITUATION_IMPLEMENTATION` | **Required** in exactly one source file to compile the library implementation. |
| `SITUATION_USE_VULKAN` | Selects the **Vulkan 1.4** backend. Requires the Vulkan SDK to be installed/linked. |
| `SITUATION_USE_OPENGL` | Selects the **OpenGL** backend. Uses GLAD (included) to load GL 4.6 Core functions. |
| `SITUATION_ENABLE_SHADER_COMPILER` | Enables runtime GLSL to SPIR-V compilation (requires `shaderc`). **Mandatory** for Vulkan if using internal renderers (Text, Virtual Displays). |
| `SITUATION_ENABLE_DXGI` | **(Windows Only)** Enables high-precision VRAM monitoring and GPU naming using the DXGI API. Requires linking `dxgi.lib` and `ole32.lib`. |
| `SITUATION_NO_STB` | Disables the automatic implementation of the STB libraries. Use this if your project already links `stb_image` or `stb_truetype` to avoid symbol collisions. |

### Architectural Safeguards

#### The "Update-Before-Draw" Rule
To maintain consistency between the **Immediate Mode** nature of OpenGL and the **Deferred** nature of Vulkan command buffers, strictly adhere to this order in your render loop:
1.  **Update Data:** Call `SituationUpdateBuffer`, `SituationCmdSetPushConstant`, or texture uploads.
2.  **Record Commands:** Call `SituationCmdDraw*`.

In Vulkan, commands recorded now are executed later. If you update a buffer after recording a draw call but before the frame ends, the GPU will read the new data for the old draw call. In Debug builds, the library actively monitors this and will report architectural violations.

#### Thread Safety
*   **Main Thread:** Windowing, Rendering commands, and Event Polling must occur here.
*   **Worker Threads:** File I/O, Data decompression, and Logic (Physics/AI) are safe via the **Generational Task System**.
*   **Safety:** The task system uses atomic operations for O(1) lock-free status checks. Input uses ring buffers. Audio loading uses full RAM decoding.

### Best Practices

#### Audio Loading
*   **Sound Effects:** Use `SITUATION_AUDIO_LOAD_AUTO` or `FULL`. This decodes the entire sound to RAM, ensuring instant playback with zero risk of disk-related stuttering.
*   **Music:** Use `SITUATION_AUDIO_LOAD_STREAM`. This keeps a small buffer in RAM and streams from disk. It saves memory but relies on the OS disk cache.

#### Resource Lifecycle
This library does not use garbage collection.
*   **Create/Destroy:** Every `SituationCreate*` must be paired with a `SituationDestroy*`.
*   **Load/Unload:** Every `SituationLoad*` must be paired with a `SituationUnload*`.
*   **Leak Detection:** Calling `SituationShutdown()` will scan the internal tracking lists and print warnings to `stderr` for any resources you forgot to free.

### Troubleshooting

**Q: `SituationInit` fails with `SITUATION_ERROR_VULKAN_PIPELINE_FAILED`?**
You defined `SITUATION_USE_VULKAN` but did not define `SITUATION_ENABLE_SHADER_COMPILER`. The internal 2D renderers (for Text and Virtual Displays) require `shaderc` to compile their GLSL source to SPIR-V at runtime.

**Q: Why does my game crash after loading ~500 textures in Vulkan?**
If you are on an older version (< v2.3.3C), you hit the fixed descriptor pool limit. Upgrade to v2.3.15+, which introduces the Dynamic Descriptor Manager to automatically grow the pool as needed.

**Q: `SituationTakeScreenshot` returns false?**
Screenshots require a supported file extension (`.png` or `.bmp`). Check that your filename ends in one of these and that you haven't disabled STB support without providing an alternative writer.

**Q: Audio crackles or pops when loading a level?**
You might be streaming too many sounds from disk at once. Switch your SFX loading mode to `SITUATION_AUDIO_LOAD_FULL` to decode them to RAM, removing the disk I/O bottleneck from the audio thread.

**Q: My 3D Model renders black?**
The model loader likely failed to find the texture files relative to the model. Check the console output; the library logs warnings if specific texture paths in a GLTF file could not be resolved.

---

## 6. API Reference

The documentation for "Situation" is split into two key documents:

1.  [**Core API Library Reference Manual (situation_sdk.md)**](situation_sdk.md): The primary SDK documentation and technical reference manual. This is the "Bible" for the library, covering architecture, concepts, and detailed component specifications.
2.  [**Situation API Programming Guide (situation_api.md)**](situation_api.md): A comprehensive list of all functions, structs, and enums with usage examples.

---

## 7. Version History

For a detailed history of changes, improvements, and fixes, please refer to the [**Update Log**](UPDATELOG.md).

---

## License (MIT)

"Situation" is licensed under the permissive MIT License. You are free to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software for both commercial and private projects. The only requirement is that you include the original copyright and license notice in any substantial portion of the software or derivative work you distribute.

Copyright (c) 2025-2026 Jacques Morel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
