# The "Situation" Advanced Platform Awareness, Control, and Timing

_Core API library v2.3.3B "Refinement"_

_(c) 2025 Jacques Morel_

_MIT Licenced_

Welcome to "Situation", a public API engineered for high-performance, cross-platform development. "Situation" is a single-file, cross-platform C/C++ library providing unified, low-level access and control over essential application subsystems. Its purpose is to abstract away platform-specific complexities, offering a lean yet powerful API for building sophisticated, high-performance software. This library is designed as a foundational layer for professional applications, including but not limited to: real-time simulations, game engines, multimedia installations, and scientific visualization tools. We are actively seeking contributions from the community to help us build a truly exceptional and robust platform.

**Version 2.3.3B "Refinement"** is a critical stabilization update. It resolves compilation issues in the Vulkan backend and solidifies the **Unified Resource Architecture**. This release ensures that all textures created via the standard API are "Compute-Ready" by default across both OpenGL and Vulkan, simplifying the workflow for high-performance compute shaders without requiring complex flag management.

Our immediate development roadmap is focused on several key areas:
*   **Hot Reloading:** Implementing live reloading for Shaders, Textures, and Models to drastically reduce iteration times during development.
*   **Built-in Debug Tools**: Leveraging the profiling data to create an immediate-mode debug UI overlay.
*   **Full Thread-Safety**: We are working towards making the entire library thread-safe, allowing for even greater performance and scalability.
*   **Vulkan Optimization**: Continuing to refine the persistent descriptor set model for maximum throughput.

"Situation" is an ambitious project that aims to become a premier, go-to solution for developers seeking a reliable and powerful platform layer. We encourage you to explore the library, challenge its capabilities, and contribute to its evolution.

The library's philosophy is reflected in its name, granting developers complete situational "Awareness," precise "Control," and fine-grained "Timing."

It provides deep **Awareness** of the host system through APIs for querying hardware **(GPU Name, VRAM)** and multi-monitor display information, and by handling operating system events like window focus and file drops.

This foundation enables precise **Control** over the entire application stack, from window management (fullscreen, borderless) and input devices (keyboard, mouse, gamepad) to a comprehensive audio pipeline with playback, **capture (recording)**, and real-time effects. This control extends to the graphics and compute pipeline, abstracting modern OpenGL and Vulkan through a unified command-buffer model **(designed for explicit sequential execution)**. It offers simplified management of GPU resources—such as shaders, meshes, and **compute-ready textures**—and includes powerful utilities for high-quality text rendering **(now with printf-style formatting)**, robust filesystem I/O, **memory-resident asset loading**, and **3D model exporting**.

Finally, its **Timing** capabilities range from high-resolution performance measurement **(FPS, Draw Calls)** and frame rate management to an advanced **Temporal Oscillator System** for creating complex, rhythmically synchronized events. By handling the foundational boilerplate of platform interaction, "Situation" empowers developers to focus on core application logic, enabling the creation of responsive and sophisticated software—from games and creative coding projects to data visualization tools—across all major desktop platforms.

> **CRITICAL ARCHITECTURAL NOTE:** To guarantee identical behavior between OpenGL (Immediate) and Vulkan (Deferred), developers must **update all buffer data before recording draw commands** within a frame. *As of v2.3.2B, the library actively enforces this rule in debug builds and will report a runtime error if violated.*

---

## Table of Contents
- [1. Introduction & Overview](#1-introduction--overview)
- [2. Getting Started](#2-getting-started)
- [3. Core Concepts & Architecture](#3-core-concepts--architecture)
- [4. Building & Configuration](#4-building--configuration)
- [5. Examples & Tutorials](#5-examples--tutorials)
- [6. Frequently Asked Questions (FAQ) & Troubleshooting](#6-frequently-asked-questions-faq--troubleshooting)
- [7. API Reference](#7-api-reference)

---

<details>
<summary><h2>1. Introduction & Overview</h2></summary>

`situation.h` is a single-header C library designed to provide a robust, cross-platform foundation for developing graphical applications and games. It abstracts the complexities of underlying system APIs like windowing (GLFW), graphics (OpenGL/Vulkan), and audio (miniaudio) into a cohesive and simplified interface. Its primary goal is to offer developers "situational awareness" of the platform and precise control over core application subsystems.

Key features include:
- **Cross-Platform Windowing & Input:** Create and manage windows, handle keyboard, mouse, and gamepad input.
- **Dual Graphics Backend Support:** Seamless abstraction over modern OpenGL (4.6+ Core) and Vulkan (1.1+).
- **Compute Shader Support:** A unified API for leveraging GPU compute power.
- **Comprehensive Resource Management:** Simplified loading and management of shaders, meshes, textures, and buffers.
- **Advanced Rendering Features:** Includes a Virtual Display System for off-screen rendering, high-performance text rendering, and a command buffer abstraction.
- **Audio System:** Full playback and **recording/capture** capabilities via the miniaudio library.
- **Utility Functions:** High-resolution timers, FPS calculation, filesystem path utilities, and model exporting (.gltf).
- **Resource Embedding:** Load fonts directly from memory buffers for single-file executables.
- **Profiling & Diagnostics:** Built-in counters for Draw Calls and VRAM usage (Vulkan).
- **Hardware Awareness:** APIs to query specific GPU names and capabilities.

</details>

---
<details>
<summary><h2>2. Getting Started</h2></summary>

A minimal application requires **zero configuration** beyond selecting a backend.

1.  Download `situation.h` (and `stb_*.h` if not using the bundled single-file release).
2.  Create `main.c`:

```c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_VULKAN // or SITUATION_USE_OPENGL
#include "situation.h"

int main(int argc, char** argv) {
    // 1. Initialize
    if (SituationInit(argc, argv, NULL) != SITUATION_SUCCESS) return -1;

    // 2. Zero Friction Assets (No extra libs needed!)
    SituationSound music;
    SituationLoadSoundFromFile("bgm.mp3", true, &music);
    SituationPlayLoadedSound(&music);
    
    SituationFont font = SituationLoadFont("font.ttf");

    // 3. Main Loop
    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME(); // Macro: Polls Input + Updates Timers

        if (SituationAcquireFrameCommandBuffer()) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            
            // Define a simple render pass (Clear screen to dark gray)
            SituationRenderPassInfo pass = { 
                .display_id = -1, // Main Window
                .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {20, 20, 20, 255} } } 
            };
            
            SituationCmdBeginRenderPass(cmd, &pass); 
            
            // ... Record draw commands here ...
            
            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }
    
    // 4. Cleanup
    SituationShutdown();
    return 0;
}```

</details>

---
<details>
<summary><h2>3. Core Concepts & Architecture</h2></summary>

The library is built on several core principles to ensure a simple and predictable development experience. For in-depth explanations of these topics, please see the [**Introduction and Core Concepts**](situation_api.md#introduction-and-core-concepts) section of the API guide.

-   **Single-Threaded Design:** All `SITAPI` functions must be called from the main thread.
-   **Explicit Resource Management:** Resources created with `SituationCreate...` or `SituationLoad...` must be manually destroyed.
-   **Three-Phase Frame:** Each frame in the main loop should follow a strict `Input (Poll) -> Update (Timers/Logic) -> Render` sequence.
-   **Command Buffer Model:** Rendering and compute operations are recorded into a command buffer and submitted to the GPU, a model that aligns with modern graphics APIs like Vulkan.

</details>

---
<details>
<summary><h2>4. Building & Configuration</h2></summary>

Configuration is managed through preprocessor defines. For detailed instructions on dependencies, compiler flags, and build configurations, refer to the [**Building the Library**](situation_api.md#building-the-library) section in the API documentation.

-   **Backend Selection:** Define `SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`.
-   **Shared Library:** Use `SITUATION_BUILD_SHARED` when compiling the library and `SITUATION_USE_SHARED` when linking against it.
-   **Feature Toggles:** Defines like `SITUATION_ENABLE_SHADER_COMPILER` enable optional features.

</details>

---
<details>
<summary><h2>5. Examples & Tutorials</h2></summary>

The repository includes a variety of examples demonstrating the library's features, from basic triangle rendering to more advanced topics like compute shaders and 3D model loading.

The full source code for all examples can be found in the `/examples` directory.

</details>

---
<details>
<summary><h2>6. Frequently Asked Questions (FAQ) & Troubleshooting</h2></summary>

### **Configuration Settings (Preprocessor Macros)**

"Situation" is configured via preprocessor definitions. You must define these **before** including `situation.h`.

| Macro | Description |
| :--- | :--- |
| `SITUATION_IMPLEMENTATION` | **Required** in exactly one source file to compile the library implementation. |
| `SITUATION_USE_VULKAN` | Selects the **Vulkan** backend. Requires the Vulkan SDK to be installed/linked. |
| `SITUATION_USE_OPENGL` | Selects the **OpenGL** backend. Uses GLAD (included) to load GL 4.6 Core functions. |
| `SITUATION_ENABLE_SHADER_COMPILER` | Enables runtime GLSL to SPIR-V compilation (requires `shaderc`). **Mandatory** for Vulkan if using internal renderers (Text, Virtual Displays). |
| `SITUATION_ENABLE_DXGI` | **(Windows Only)** Enables high-precision VRAM monitoring and GPU naming using the DXGI API. Requires linking `dxgi.lib` and `ole32.lib`. |
| `SITUATION_NO_STB` | Disables the automatic implementation of the STB libraries. Use this if your project already links `stb_image` or `stb_truetype` to avoid symbol collisions. |
| `SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD` | Flag for `SituationInitInfo`. Routes audio microphone callbacks to the main thread (via `SituationPollInputEvents`) to ensure thread safety for user logic. |

---

### **Architectural Safeguards**

#### 1. The "Update-Before-Draw" Rule
To maintain consistency between the **Immediate Mode** nature of OpenGL and the **Deferred** nature of Vulkan command buffers, you must strictly adhere to this order in your render loop:
1.  **Update Data:** Call `SituationUpdateBuffer`, `SituationCmdSetPushConstant`, or texture uploads.
2.  **Record Commands:** Call `SituationCmdDraw*`.
**Why?** In Vulkan, commands recorded now are executed later. If you update a buffer *after* recording a draw call but *before* the frame ends, the GPU will read the *new* data for the *old* draw call. In Debug builds, the library actively monitors this and will panic if you violate this order.

#### 2. Thread Safety
The "Situation" API is **Single-Threaded** by design. All `SITAPI` functions (Windowing, Rendering, Input) must be called from the main thread.
*   **Exception:** Audio Stream callbacks (`on_read`, `on_seek`) run on a high-priority background thread. Do not perform memory allocation or file I/O in these callbacks.
*   **Safeguard:** The Input and Audio subsystems use internal mutexes to safely queue events for the main thread to consume.

---

### **Best Practices**

#### Text Rendering Strategies
*   **Real-Time UI / HUD:** Use `SituationBakeFontAtlas()` once at startup, then use `SituationCmdDrawText()` every frame. This uses the GPU and is extremely fast.
*   **Static Assets / Textures:** Use `SituationImageDrawTextEx()` to draw high-quality text onto a `SituationImage` in CPU memory, then upload it as a texture. This is slow and should not be done per-frame.

#### Resource Lifecycle
This library does not use garbage collection.
*   **Create/Destroy:** Every `SituationCreate*` must be paired with a `SituationDestroy*`.
*   **Load/Unload:** Every `SituationLoad*` must be paired with a `SituationUnload*`.
*   **Leak Detection:** Calling `SituationShutdown()` will scan the internal tracking lists and print warnings to `stderr` for any resources you forgot to free.

---

### **Troubleshooting**

**Q: `SituationInit` fails with `SITUATION_ERROR_VULKAN_PIPELINE_FAILED`?**
*   **Cause:** You likely defined `SITUATION_USE_VULKAN` but did not define `SITUATION_ENABLE_SHADER_COMPILER`, or the `shaderc` library is not linked. The internal 2D renderers require runtime GLSL compilation.

**Q: `SituationTakeScreenshot` returns false?**
*   **Cause:** As of v2.3.3A, screenshots **must** use the `.png` extension. Check that your filename ends in `.png` and that you haven't disabled STB support without providing an alternative.

**Q: My 3D Model renders black?**
*   **Cause:** The model loader failed to find the texture files. Check the console output; v2.3.3+ logs warnings if a specific texture path in a GLTF file could not be resolved relative to the model file.

**Q: Why does `SituationGetVRAMUsage()` return 0?**
*   **Cause:** You are likely using OpenGL on a non-NVIDIA GPU, or on Windows without `SITUATION_ENABLE_DXGI`. Standard OpenGL does not support memory queries. Switch to Vulkan or enable DXGI for accurate tracking.

</details>

---
<details>
<summary><h2>7. API Reference</h2></summary>

The complete API reference, including detailed descriptions of all modules, functions, structs, and enums, is available in the [**Situation API Programming Guide (situation_api.md)**](situation_api.md).

</details>

---
## License (MIT)

"Situation" is licensed under the permissive MIT License. In simple terms, this means you are free to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software for both commercial and private projects. The only requirement is that you include the original copyright and license notice in any substantial portion of the software or derivative work you distribute. This library is provided "as is", without any warranty.

---

Copyright (c) 2025 Jacques Morel

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.