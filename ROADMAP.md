## Roadmap & Future Goals

The Situation SDK is an evolving project. While the current v2.3.x series focuses on "Velocity" (Developer Experience & Stability), the next major milestones target web capability and expanded platform reach.

### Phase 3: "Momentum" (v2.4)
*   **Async I/O:** Dedicated I/O queues for non-blocking asset streaming.
*   **Virtual Mounts:** Support for loading assets from packed archives (.zip, .pak).
*   **Audio Job System:** Lock-free, massive concurrency for audio processing.

### Phase 4: "Web & Reach" (v2.5+)
We aim to bring the Titanium-grade experience to the browser.
*   **Emscripten Support:** Full compilation target for WebAssembly (WASM), allowing Situation apps to run in the browser with near-native performance.
*   **WebGPU (Dawn):** A new backend targeting WebGPU via Google's Dawn implementation. This will sit alongside OpenGL and Vulkan, unlocking high-performance compute and rendering on the web.

### Phase 5: "Ecosystem" (v3.0)
*   **UI Toolkit:** A lightweight, immediate-mode UI library built on Situation.
*   **Visual Profiler:** A standalone tool to visualize `SituationDumpTaskGraph` and performance metrics.
