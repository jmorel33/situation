<div align="center">
  <img src="situation_blackMetal_logo.jpg" alt="Situation logo">
</div>

# The "Situation" Advanced Platform Awareness, Control, and Timing

_(c) 2025-2026 Jacques Morel — MIT Licensed_

Welcome to **Situation** — a public API engineered for high-performance, cross-platform development. Situation is a **[Strict C11 (ISO/IEC 9899:2011) Compliant](C11_Compliance_Report.md)** header-only library (split API/impl modules under `sit/`, user entry point `sit/situation.h`) providing unified, low-level access to windowing, graphics, audio, input, filesystem, threading, and timing. It ships as pre-built static libraries or DLLs on Windows, with **581** public `SITAPI` functions and auto-generated FFI bindings for seven languages.

**Current release:** v2.4.403 — see **[What's New](whatsnew.md)** and **[UPDATELOG](UPDATELOG.md)** for patch notes. Deep architecture: **[architecture.md](architecture.md)** (includes [application identity](architecture.md#application-identity-architecture-v24399)). **Application identity Phase I (Win32) complete** — see **[SIT_IDENTITY_PLAN](plan/SIT_IDENTITY_PLAN.md)** for WI-6+, Linux, macOS tracks.

**Recently shipped (v2.4.384–393):**

*   **VSync-sharp game loop** — present-anchored `frame_time` and FPS; paced queue depth (2 frames under VSync); on-demand OpenGL screen capture (no per-frame readback).
*   **Fractional refresh rates** — `SituationGetDisplayRefreshRateHz()` for 59.94 Hz panels; HUD FPS rounds to 60 when appropriate.
*   **Virtual Display bolster** — sRGB FBOs, explicit sampler config, aniso/mip controls, static update mode, unified `SitVdStandbyConfig` idle patterns.
*   **Renderer behavior policy** — `SituationCmdSet/Push/PopRendererBehavior`; render-target readback (`SituationRenderTarget`).
*   **Tone synth & graph audio** — polyBLEP graph engine, 42-parameter travel editor in example 04; registry-driven node graph with 23 FX types.
*   **Seven language wrappers** — Odin, Zig, Rust, Fortran, Modula-2, Python, Lua (`tools/run_all.bat`).

**On the roadmap:**

*   **Async compute queues** — dedicated transfer/compute queues in Vulkan for non-blocking background work.
*   **Full Tracy integration** — opt-in CPU/GPU timeline profiling ([AAA_ARCHITECTURE_PLAN.md](plan/AAA_ARCHITECTURE_PLAN.md) §6).
*   **Cross-platform expansion** — Android, WebAssembly, WebGPU (Dawn) backends.

"Situation" is an ambitious project that aims to become a premier, go-to solution for developers seeking a reliable and powerful platform layer.

The library's philosophy is reflected in its name, granting developers complete situational **Awareness**, precise **Control**, and fine-grained **Timing**.

It provides deep **Awareness** of the host system through APIs for querying hardware **(GPU Name, VRAM)** and multi-monitor display information, and by handling operating system events like window focus and file drops.

This foundation enables precise **Control** over the entire application stack:

*   **Threading:** Generational dual-queue task system (mutex per queue, atomics for indices/job state): fork-join `SituationDispatchParallel`, high/low priority rings, backpressure (`RUN_IF_FULL` / `BLOCK_IF_FULL`), dedicated I/O thread, in-place job claim + full-queue HOL scan (v2.4.232–233), topology/affinity/NUMA placement, OS-visible thread names (`SituationSetCurrentThreadName`, v2.4.239), pool observability and scheduler metrics (v2.4.139+).
*   **Windowing:** Fullscreen, borderless, and HiDPI-aware window management with explicit **State Hardening** to prevent context poisoning from external middleware (e.g., ImGui).
*   **Input:** O(1) ring-buffered processing for Keyboard, Mouse, and Gamepad events ensures no input is ever lost during frame spikes.
*   **Audio:** Registry-driven **node graph** (`SituationProcessGraph`), **16-voice graph tone synth** with MIDI CC mapping, snapshot-and-unlock voice mixing, async RAM decode for SFX, disk streaming for music, and 23 real-time FX nodes (reverb, delay, filter, and more).
*   **Graphics:** Unified `SituationCmd*` command buffers for **OpenGL 4.6** and **Vulkan 1.4** (dual-backend parity — same binding model where possible). Features include MDI batching, bindless textures (where supported), compute shaders, **2D grid** cell playfields (`SituationGrid*` → compute [Virtual Display](guide/virtual_display.md)), **Virtual Display** compositing with calibration test patterns, GPU text with retro font builders, dynamic descriptor pools, and a dedicated **render thread** (optional, default in examples) that owns present while the main thread records commands.
*   **Hot-Reloading:** Debounced I/O polling on the I/O thread; shaders, textures, and models reload safely with GPU fence/graveyard synchronization.

Finally, its **Timing** capabilities include **present-anchored** `SituationGetFrameTime()` and refresh-aware `SituationGetFPS()`, built-in spike/phase diagnostics (`SituationGetLastFramePhases`, metrics overlay), software frame caps, VSync control, and a **Temporal Oscillator System** for rhythmically synchronized events.

---

## Table of Contents

- [1. What is Situation?](#1-what-is-situation)
- [2. Getting Started](#2-getting-started)
- [3. Building & Configuration](#3-building--configuration)
    - [Language Wrappers](#language-wrappers)
- [4. Examples & Tutorials](#4-examples--tutorials)
- [5. FAQ & Troubleshooting](#5-frequently-asked-questions-faq--troubleshooting)
- [6. API Reference](#6-api-reference)
- [7. Version History](#7-version-history)

**See also:** [architecture.md](architecture.md) (internals), [situation_sdk.md](situation_sdk.md) (SDK manual), [whatsnew.md](whatsnew.md) (release highlights).

### Build scripts (Windows — run from project root)

Thin `.bat` launchers in `build\` wrap the Makefile; no manual gcc lines. Full list: **[build/README.md](../build/README.md)**.

| Script | What it does |
|--------|----------------|
| `build\build_situation.bat` | Library — `static-opengl`, `static-vulkan`, `opengl`, `vulkan`, `all`, `clean` |
| `build\build_examples.bat` | C examples — `static-opengl 01_open_a_window`, etc. |
| `build\build_tests.bat` | Test harness — same backend names as above |
| `build\run_tests.bat` | Run harness with filters — `opengl --module graphics` |
| `build\build_<lang>_example.bat` | Wrapper hello demos — see table below |

Invoke from PowerShell as `& ".\build\build_situation.bat" static-opengl` (do not use `cmd /c` on this machine).

### Seven language bindings

Auto-generated from `sit/situation_api.h` via `tools\run_all.bat`. Output lives in `wrappers/`. Each ships a full **`hello_situation`** demo (raster bars + ambient synth).

| Language | Build script | Output |
|----------|--------------|--------|
| **Odin** | `build\build_odin_example.bat` | `build/examples/odin/` |
| **Zig** | `build\build_zig_example.bat` | `build/examples/zig/` |
| **Rust** | `build\build_rust_example.bat` | `build/examples/rust/` |
| **Fortran** | `build\build_fortran_example.bat` | `build/examples/fortran/` |
| **Modula-2** | `build\build_modula2_example.bat` | `build/examples/modula2/` |
| **Python** | `build\build_python_example.bat` | `build/examples/python/` |
| **Lua** | `build\build_lua_example.bat` | `build/examples/lua/hello_situation.exe` |

Prerequisite for any row: matching `build\build_situation.bat <backend>` first. Per-language toolchain notes and the static/DLL matrix: **[COMPILATION_GUIDE.md](COMPILATION_GUIDE.md)** · detailed wrapper prose: [§Language Wrappers](#language-wrappers) below.

---

## 1. What is Situation?

`sit/situation.h` is the user-facing entry point for a C/C++ library that acts as a high-performance kernel for interactive software. The implementation lives in modular headers under `sit/` and compiles into a single translation unit via `situation_dll.c` (or links as `situation_opengl.a` / `situation_vulkan.a`). Situation abstracts OS APIs and graphics backends (OpenGL/Vulkan) into one deterministic platform layer.

Unlike thin wrappers, Situation is an **opinionated system abstraction layer**. It enforces a strict **three-phase frame contract** and an **update-before-draw** rule so OpenGL (deferred soft-buffer replay) and Vulkan (hardware command buffers) behave identically.

**Platform:** Windows 10+ is the primary shipping target. Linux and macOS builds are in progress. Requires OpenGL 4.6+ or Vulkan 1.4+ hardware — no fallback to older GL/VK versions.

### Key Capabilities

*   **Unified Command Architecture:** Record once with `SituationCmd*`; OpenGL replays on the render thread, Vulkan submits `VkCommandBuffer`s. Dual-backend shaders share binding contracts (e.g. test-pattern std140 UBO at `set=0, binding=0`).
*   **Generational Task System:** C11 thread pool — dual priority queues, fork-join `SituationDispatchParallel`, generational job IDs, NUMA/topology-aware affinity, observability metrics.
*   **Hardened Audio Engine:** miniaudio callback drives the active node graph, voice snapshots, and graph tone synth; async decode, streaming, MIDI, and 23 FX nodes.
*   **Dynamic Resource Management:** Generational slot registries; Vulkan descriptor pools grow on demand; fence-guarded graveyards on both backends.
*   **O(1) Input System:** Ring-buffered keyboard, mouse, and gamepad — no events lost during frame spikes.
*   **Virtual Display Compositor:** Off-screen layers (e.g. 320×240) composited to the main swapchain with scaling, blend modes, sRGB/HDR-aware clears, and SMPTE/calibration idle patterns.
*   **First-Class Compute:** `SituationCmdDispatch`, SSBOs, indirect draw, barriers, and render-target readback — documented in [renderer_bolster.md](guide/renderer_bolster.md).
*   **2D Grid subsystem:** Dense **`SituationGrid*`** cell surfaces, stacked playfields, and collision probes — rendered into compute-target [Virtual Displays](guide/virtual_display.md). K-Term is the reference VT client ([grid guide](guide/grid.md)).
*   **Deep System Awareness:** GPU name/VRAM, fractional monitor refresh (`GetDisplayRefreshRateHz`), multi-monitor topology, thread-pool snapshots.

### Frame loop contract (v2.4.384+)

Every frame on the **main thread**, in order:

```text
SituationPollInputEvents()
SituationUpdateTimers()
  → your logic (physics, AI, audio triggers)
SituationAcquireFrameCommandBuffer()
  → record SituationCmd*
SituationEndFrame()
```

With the **render thread** enabled (recommended), `EndFrame` enqueues work; present runs on the render thread. `SituationGetFrameTime()` reflects **display pace** (interval between presents), not main-thread spin rate. Under VSync or a software FPS cap, the queue depth is **2** frames (not 6). See **[architecture.md — Frame Loop Contract](architecture.md#frame-loop-contract-v2484)**.

> **CRITICAL:** Update buffers and uniforms **before** recording draw commands that use them. Debug builds report violations. Do not call `UpdateTimers` between `AcquireFrameCommandBuffer` and `EndFrame`.

---

## 2. Getting Started

### Quick path (recommended)

From the project root on Windows (MSYS2 MinGW):

```bat
build\build_situation.bat static-opengl
build\build_examples.bat static-opengl 01_open_a_window
build\examples\01_open_a_window.exe
```

Examples link the **pre-built static library** — they do **not** define `SITUATION_IMPLEMENTATION`. See **[BUILD_SITUATION_GUIDE.md](BUILD_SITUATION_GUIDE.md)** and **[COMPILATION_GUIDE.md](COMPILATION_GUIDE.md)**.

The numbered examples under `examples/` use **`shared/sit_example.h`** (VSync on, render thread on, HUD with FPS/metrics hotkeys). Start with `01_open_a_window`, then `02_draw_shapes`.

### Minimal application skeleton

Select a backend, include `sit/situation.h`, link `situation_opengl.a` or `situation_vulkan.a` (or the matching DLL with `SITUATION_USE_SHARED`).

```c
#define SITUATION_USE_OPENGL
#define SITUATION_ENABLE_THREADING
#include "sit/situation.h"

int main(int argc, char** argv) {
    SituationInitInfo cfg = {
        .window_width = 1280,
        .window_height = 720,
        .window_title = "Hello Situation",
        .initial_active_window_flags = SITUATION_FLAG_VSYNC_HINT | SITUATION_FLAG_WINDOW_RESIZABLE,
#if defined(SITUATION_ENABLE_RENDER_THREAD)
        .render_thread_count = 1,
#endif
    };
    if (SituationInit(argc, argv, &cfg) != SITUATION_SUCCESS) return -1;

    SituationSetTargetFPS(0);  /* VSync paces; no software double-wait */

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        /* your logic here */

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
            SituationRenderPassInfo pass = {
                .display_id = -1,
                .color_attachment = {
                    .loadOp = SIT_LOAD_OP_CLEAR,
                    .clear = { .color = {20, 30, 40, 255} }
                }
            };
            SituationCmdBeginRenderPass(cmd, &pass);
            SituationFont font = {0};  /* built-in 8×8 default when generation == 0 */
            SituationCmdDrawText(cmd, font, "Situation",
                (Vector2){{50, 50}}, (ColorRGBA){255, 255, 255, 255});
            SituationCmdEndRenderPass(cmd);
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
```

**Library build only:** define `SITUATION_IMPLEMENTATION` in exactly one `.c` file (typically `situation_dll.c`) when compiling the library itself — not in application or test code.

**Vulkan text/VD:** define `SITUATION_ENABLE_SHADER_COMPILER` and link shaderc when using the Vulkan backend with built-in text or virtual-display compositors.

---

## 3. Building & Configuration

Situation uses a **header-only implementation** compiled once into a static library or DLL (`sit/Makefile`, invoked via `build\build_situation.bat`). Application code includes `sit/situation.h` and **links** the pre-built artifact — it does not recompile the library.

| Doc | Purpose |
|-----|---------|
| **[BUILD_SITUATION_GUIDE.md](BUILD_SITUATION_GUIDE.md)** | Makefile targets, Vulkan SDK, shaderc |
| **[COMPILATION_GUIDE.md](COMPILATION_GUIDE.md)** | Flags, defines, examples, wrappers |
| **[build/README.md](../build/README.md)** | Quick launcher reference |

Configuration macros must be defined **before** including `sit/situation.h` (or passed on the compiler command line).

### Preprocessor Macros

| Macro | Type | Description |
| :--- | :--- | :--- |
| `SITUATION_IMPLEMENTATION` | Library build | **Only** in `situation_dll.c` (or your single library TU) — never in apps, examples, or tests. |
| `SITUATION_USE_VULKAN` | Backend | Selects the **Vulkan 1.4** backend. |
| `SITUATION_USE_OPENGL` | Backend | Selects the **OpenGL 4.6** backend (GLAD included). |
| `SITUATION_USE_SHARED` | Link mode | Define when linking against the DLL (`__declspec(dllimport)`). |
| `SITUATION_ENABLE_THREADING` | Feature | Enables the generational task system and render thread. |
| `SITUATION_ENABLE_SHADER_COMPILER` | Feature | Runtime GLSL → SPIR-V via shaderc. **Required for Vulkan** built-in text/VD paths. |
| `SITUATION_ENABLE_DXGI` | Feature | **(Windows)** High-precision VRAM and GPU naming. |
| `SITUATION_NO_STB` | Integration | Disable embedded stb if your project already provides them. |

### Linker Requirements

| Platform | Standard Links | With `SITUATION_USE_VULKAN` | With `SITUATION_ENABLE_DXGI` |
| :--- | :--- | :--- | :--- |
| **Windows (MSVC/MinGW)** | `kernel32`, `user32`, `shell32`, `gdi32` | `vulkan-1.lib`, `shaderc_shared.lib` | `dxgi.lib`, `ole32.lib`, `shlwapi.lib` |
| **Linux (GCC/Clang)** | `-lm`, `-ldl`, `-lpthread`, `-lX11` | `-lvulkan`, `-lshaderc_shared` | N/A |
| **macOS (Clang)** | `-framework Cocoa`, `-framework IOKit` | `-lvulkan`, `-lshaderc_shared` | N/A |

> **Note:** If using `SITUATION_ENABLE_SHADER_COMPILER`, ensure the `shaderc` includes and libraries are in your compiler's search path.

---

### Language Wrappers

Situation provides official FFI bindings and fully featured interactive examples for **Odin**, **Zig**, **Rust**, **Fortran**, **Modula-2**, **Python**, and **Lua** inside the `wrappers/` folder. Binding generators in `tools/` parse the C public headers automatically (`tools/run_all.bat` regenerates everything).

Wrapper example builders use the **same backends** as `build_examples.bat` and `build_tests.bat` (where supported):

| Backend | Prerequisite |
|---------|--------------|
| `opengl` | `build_situation.bat opengl` |
| `vulkan` | `build_situation.bat vulkan` |
| `static-opengl` | `build_situation.bat static-opengl` |
| `static-vulkan` | `build_situation.bat static-vulkan` |

```bat
build\build_odin_example.bat     [backend] [example_name]
build\build_zig_example.bat      [backend] [example_name]
build\build_rust_example.bat     [backend] [example_name]
build\build_fortran_example.bat  [backend] [example_name]
build\build_modula2_example.bat  [backend] [example_name]
build\build_python_example.bat   [backend] [example_name]
build\build_lua_example.bat      [backend] [example_name]
```

`example_name` defaults to `hello_situation`. See `doc/COMPILATION_GUIDE.md` for per-language link details, the static-mode matrix, and shared `scripts/wrapper_*.bat` helpers.

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

#### Fortran Wrapper
- **Fortran**: MSYS2 MinGW `gfortran` (`pacman -S mingw-w64-x86_64-gcc-fortran`)
- **Source Files**: `wrappers/Fortran/`
- **Binding Generator**: `python tools/generate_fortran_bindings.py`
- **Build**:
  ```bat
  build_fortran_example.bat opengl
  build_fortran_example.bat static-opengl hello_situation
  ```
- **Output**: `build/examples/fortran/` (intermediate `.o`/`.mod` under `build/obj/fortran/`)

#### Modula-2 Wrapper
- **GNU Modula-2**: `gm2` — bundle in `_languages/gm2/` or build from GCC source (no MSYS2 package)
- **Source Files**: `wrappers/Modula2/`
- **Binding Generator**: `python tools/generate_modula2_bindings.py`
- **Build**:
  ```bat
  build_modula2_example.bat opengl
  build_modula2_example.bat static-opengl hello_situation
  ```
- **Output**: `build/examples/modula2/` (intermediate `.o` under `build/obj/modula2/`)

#### Python Wrapper
- **Python 3.10+** with **PyInstaller** (`pip install pyinstaller`)
- **Source Files**: `wrappers/Python/`
- **Binding Generator**: `python tools/generate_python_bindings.py`
- **Build**:
  ```bat
  build\build_python_example.bat opengl hello_situation
  ```
- **Output**: `build/examples/python/` — `.exe` + `situation_*.dll` (DLL modes only; no static link)

#### Lua Wrapper
- **LuaJIT 2.1** — bundled at `_languages/lua/` (`populate_toolchain.bat`)
- **Source Files**: `wrappers/lua/`
- **Binding Generator**: `python tools/generate_lua_bindings.py`
- **Build**:
  ```bat
  build\build_lua_example.bat opengl hello_situation
  ```
- **Output**: `build/examples/lua/hello_situation.exe` only — Situation + lua51 DLLs embedded; extracted to `%TEMP%` at runtime
- **Dev mode**: `build\run_lua_dev.bat` — run staged `.lua` sources with external DLL (text overlay requires embedded exe)

For `opengl` and `vulkan` backends, the matching `situation_*.dll` is copied next to the exe (Python) or embedded inside it (Lua). Static backends produce self-contained exes with no DLL at runtime (Odin/Zig/Rust/Fortran/Modula-2 where supported).

---

## 4. Examples & Tutorials

Numbered **digestible examples** in `examples/` (`01_open_a_window` through `25_vd_standby`) share `shared/sit_example.h` — VSync, render thread, HUD, and universal hotkeys (F9 VSync, F12 screenshot, M metrics). Build with:

```bat
build\build_examples.bat static-opengl <name>
```

Legacy demos (`platformer_plumber.c`, `demon_hunt`, etc.) remain for specific feature showcases. See [DIGESTIBLE_EXAMPLES_PLAN.md](plan/DIGESTIBLE_EXAMPLES_PLAN.md) for the curriculum map.

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
Within a frame, after `SituationAcquireFrameCommandBuffer()`:
1.  **Update data** — `SituationUpdateBuffer`, push constants, texture uploads.
2.  **Record commands** — `SituationCmdDraw*`.

Vulkan records now and executes later; late updates corrupt draws. Debug builds report violations.

#### Frame loop sequencing
Follow the canonical order in §1. Never call `SituationUpdateTimers()` while a frame is open (`in_frame`). Press **M** in digestible examples to view phase timing (poll, backpressure, fence, execute, present).

#### Screen capture
`SituationLoadImageFromScreen()` reads the last presented framebuffer. Preferred flow: `SituationRequestScreenCapture()` → draw → `EndFrame()` → `LoadImageFromScreen()`. Implicit `EndFrame` → `Load` without a prior request uses an on-demand urgent capture path on the render thread (OpenGL) — avoid every-frame readback in hot loops.

#### Thread Safety
*   **Main thread:** `SituationInit`, polling, timer updates, command recording, `SituationEndFrame`.
*   **Render thread:** GPU execute and present (when `render_thread_count > 0`).
*   **Worker / I/O threads:** Parallel jobs, async loads, hot-reload polling via the task system.
*   **Rule:** All public `SITAPI` calls from the thread that called `SituationInit()` (main thread discipline).

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

**Q: VSync ON but HUD shows 59 FPS?**
As of v2.4.386, `SituationGetFPS()` rounds using the fractional nominal refresh (`SituationGetDisplayRefreshRateHz`) so 59.94 Hz panels report **60**. If you still see 59, check `GetMeasuredPresentRateHz()` — sustained drops indicate pacing stalls (see metrics overlay, **M** key).

**Q: `SituationTakeScreenshot` or `LoadImageFromScreen` fails?**
Arm capture with `SituationRequestScreenCapture()` before `EndFrame`, or call `Load` promptly after `EndFrame` on the render-thread path. Screenshots to disk need `.png` or `.bmp` extensions and STB write support.

**Q: Audio crackles or pops when loading a level?**
You might be streaming too many sounds from disk at once. Switch your SFX loading mode to `SITUATION_AUDIO_LOAD_FULL` to decode them to RAM, removing the disk I/O bottleneck from the audio thread.

**Q: My 3D Model renders black?**
The model loader likely failed to find the texture files relative to the model. Check the console output; the library logs warnings if specific texture paths in a GLTF file could not be resolved.

---

## 6. API Reference

| Layer | Document |
|-------|----------|
| **Architecture** | [architecture.md](architecture.md) — threading, frame loop, audio graph, GL/VK lifecycles, [application identity](architecture.md#application-identity-architecture-v24399) |
| **SDK manual** | [situation_sdk.md](situation_sdk.md) — workflows, subsystem guides, examples |
| **API map** | [situation_api.md](situation_api.md) — links to every **`doc/guide/`** module (581 symbols) |
| **Function index** | [situation_api_index.md](situation_api_index.md) — auto-generated categorized listing |
| **GPU commands** | [situation_command_reference.md](situation_command_reference.md) — all `SituationCmd*` |

**Module guides** (`doc/guide/`):

| Category | Guides |
|----------|--------|
| Intro | [_front_matter](guide/_front_matter.md) |
| Core | [core](guide/core.md) · [window](guide/window_display.md) · [Windows app identity](guide/windows_app_identity.md) · [input](guide/input.md) · [image](guide/image.md) · [system](guide/system_introspection.md) |
| Graphics | [graphics](guide/graphics.md) · [2D grid](guide/grid.md) · [virtual display](guide/virtual_display.md) · [test patterns](guide/test_patterns.md) · [renderer bolster](guide/renderer_bolster.md) · [compute](guide/compute.md) · [fonts](guide/font.md) · [text](guide/text_rendering.md) · [2D](guide/drawing_2d.md) · [3D](guide/drawing_3d.md) |
| Media | [audio](guide/audio.md) · [audio graph](guide/audio_graph.md) · [MIDI](guide/midi.md) · [filesystem](guide/filesystem.md) |
| Utilities | [threading](guide/threading.md) · [YPQ](guide/ypq_color.md) · [HD color](guide/hd_color_output.md) · [hot-reload](guide/hot_reload.md) · [logging](guide/logging.md) · [misc](guide/miscellaneous.md) · [deprecated](guide/deprecated.md) |
| Learning | [examples & FAQ](guide/examples_faq.md) |

Supplementary: [tone_synth.md](tone_synth.md) (graph tone engine), [plan/GAME_LOOP_PERFORMANCE_PLAN.md](plan/GAME_LOOP_PERFORMANCE_PLAN.md) (frame pacing design).

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
