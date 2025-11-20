# The "Situation" Advanced Platform Awareness, Control, and Timing

_Core API library v2.3.2 "Parity"_

_(c) 2025 Jacques Morel_

_MIT Licenced_

Welcome to "Situation", a public API engineered for high-performance, cross-platform development. "Situation" is a single-file, cross-platform C/C++ library providing unified, low-level access and control over essential application subsystems. Its purpose is to abstract away platform-specific complexities, offering a lean yet powerful API for building sophisticated, high-performance software. This library is designed as a foundational layer for professional applications, including but not limited to: real-time simulations, game engines, multimedia installations, and scientific visualization tools. We are actively seeking contributions from the community to help us build a truly exceptional and robust platform.

**Version 2.3.2 "Parity"** marks a major milestone, achieving feature parity between OpenGL and Vulkan backends (specifically regarding advanced blending), adding audio capture capabilities, and finalizing 3D model exporting tools.

Our immediate development roadmap is focused on several key areas:
*   **Full Thread-Safety**: We are working towards making the entire library thread-safe, allowing for even greater performance and scalability in multi-threaded applications.
*   **Vulkan Optimization**: Continuing to refine the persistent descriptor set model for maximum throughput.
*   **Robust Virtual Display and Windows Integration**: Continuously improving our virtual display system to ensure bullet-proof stability across diverse hardware configurations.

"Situation" is an ambitious project that aims to become a premier, go-to solution for developers seeking a reliable and powerful platform layer. We encourage you to explore the library, challenge its capabilities, and contribute to its evolution.

The library's philosophy is reflected in its name, granting developers complete situational "Awareness," precise "Control," and fine-grained "Timing."

It provides deep **Awareness** of the host system through APIs for querying hardware and multi-monitor display information, and by handling operating system events like window focus and file drops.

This foundation enables precise **Control** over the entire application stack, from window management (fullscreen, borderless) and input devices (keyboard, mouse, gamepad) to a comprehensive audio pipeline with playback, **capture (recording)**, and real-time effects. This control extends to the graphics and compute pipeline, abstracting modern OpenGL and Vulkan through a unified command-buffer model. It offers simplified management of GPU resources—such as shaders, meshes, and textures—and includes powerful utilities for high-quality text rendering, robust filesystem I/O, and **3D model exporting**.

Finally, its **Timing** capabilities range from high-resolution performance measurement and frame rate management to an advanced **Temporal Oscillator System** for creating complex, rhythmically synchronized events. By handling the foundational boilerplate of platform interaction, "Situation" empowers developers to focus on core application logic, enabling the creation of responsive and sophisticated software—from games and creative coding projects to data visualization tools—across all major desktop platforms.

---

## Table of Contents:
- [1.  Introduction & Overview](#1-introduction--overview)
    - [1.1 Purpose & Scope](#11-purpose--scope)
    - [1.2 Key Features & Capabilities](#12-key-features--capabilities)
    - [1.3 Target Audience & Use Cases](#13-target-audience--use-cases)
- [2.  Getting Started](#2-getting-started)
- [3.  Core Concepts & Architecture](#3-core-concepts--architecture)
- [4.  Building & Configuration](#4-building--configuration)
- [5.  Examples & Tutorials](#5-examples--tutorials)
- [6.  Frequently Asked Questions (FAQ) & Troubleshooting](#6-frequently-asked-questions-faq--troubleshooting)
- [7.  API Reference](#7-api-reference)

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

</details>

---
<details>
<summary><h2>2. Getting Started</h2></summary>

This section provides a high-level overview of how to integrate and use the "Situation" library. For a comprehensive guide, please refer to the [**Getting Started**](situation_api.md#getting-started) and [**Building the Library**](situation_api.md#building-the-library) sections in the full API documentation.

A minimal application involves:
1.  Defining `SITUATION_IMPLEMENTATION` and the desired backend (`SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`) in one C/C++ file.
2.  Including `situation.h`.
3.  Calling `SituationInit()` at the start of your application.
4.  Running a main loop that calls `SituationPollInputEvents()`, then `SituationUpdateTimers()`, and finally renders the frame.
5.  Calling `SituationShutdown()` before exiting.

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

This section addresses common issues and questions. For more detailed troubleshooting, please refer to the [**FAQ & Troubleshooting**](situation_api.md#frequently-asked-questions-faq--troubleshooting) section in the API guide.

-   **Initialization Failures:** Often caused by missing dependencies (Vulkan SDK, GLFW) or incorrect graphics drivers. Enable validation layers for more detailed error messages.
-   **Resource Invalid Errors:** Typically occur when using a resource handle that has not been created or has already been destroyed.
-   **Performance:** Batch similar draw calls and minimize state changes to improve performance. Use `SituationDrawTextStyled` for text-intensive applications.

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