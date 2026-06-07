# Situation Library - Compilation Guide

**Version**: 2.4.203  
**Date**: 2026-06-05  
**Status**: Complete

## Overview

This guide provides comprehensive instructions for compiling applications using the Situation library across different platforms and configurations.

## Table of Contents

1. [Project Structure](#project-structure)
2. [Quick Start](#quick-start)
3. [Dependencies](#dependencies)
4. [Compilation Options](#compilation-options)
5. [Platform-Specific Instructions](#platform-specific-instructions)
6. [Backend Selection](#backend-selection)
7. [Common Issues](#common-issues)
8. [Example Build Scripts](#example-build-scripts)
9. [KaOS Terminal Console](#kaos-terminal-console)

## Project Structure

```
situation/                         # Project root
├── situation.h                    # ← Public API entry point (include this)
├── situation_dll.c                # DLL entry point
├── build/dll/                     # Pre-built DLLs (situation_opengl.dll, situation_vulkan.dll)
│
├── sit/                          # ← Core implementation (internal)
│   ├── situation_api.h           # Public API declarations (531 functions)
│   ├── situation_base_version.h  # Canonical version macros
│   ├── situation_base_errno.h    # Error enum (SituationError)
│   ├── situation_base_etc.h      # Base utilities
│   ├── situation_base_font.h     # Embedded VGA bitmap font
│   ├── situation_base_trace.h    # Trace/profiling hooks
│   ├── situation_impl.h          # Orchestrator (includes all impl files)
│   ├── situation_impl_decl.h     # Internal types & structs
│   ├── situation_impl_forward.h  # Forward declarations
│   ├── situation_impl_renderer_fwd.h # Renderer forward declarations
│   ├── situation_impl_deps.h     # Third-party lib includes
│   ├── situation_impl_etc.h      # Utilities (math, strings, hashing)
│   ├── situation_impl_timer.h    # Oscillators, high-res time
│   ├── situation_impl_threading.h         # Thread pool, job system
│   ├── situation_impl_threading_topology.h     # CPU topology, affinity masks
│   ├── situation_impl_threading_numa.h         # NUMA placement
│   ├── situation_impl_threading_scheduler.h    # Metrics, sizing
│   ├── situation_impl_threading_observability.h # Snapshot, dump
│   ├── situation_impl_threading_diag.h         # Thread naming, hardening
│   ├── situation_impl_io.h       # File I/O, async, system info
│   ├── situation_impl_input.h    # Keyboard, mouse, gamepad
│   ├── situation_impl_wdm.h      # Window, display, monitor
│   ├── situation_impl_image.h    # Image, font, color
│   ├── situation_impl_ypq.h      # YPQ perceptual color math
│   ├── situation_impl_proj.h     # Camera & projection helpers
│   ├── situation_impl_renderer.h # GL + VK backends
│   ├── situation_impl_vd.h       # Virtual display compositing
│   ├── situation_impl_ctrl.h     # Lifecycle, init/shutdown
│   ├── situation_impl_audio.h    # Audio subsystem
│   │
│   ├── aud/                      # Audio Subsystem
│   │   ├── fx/                   # Effects & DSP nodes (23 files)
│   │   │   ├── reverb.h, studio_reverb.h, spring_reverb.h, sst282.h
│   │   │   ├── echo.h, chorus_4stage.h, phaseshifter.h, lfo.h
│   │   │   ├── overdrive.h, exciter.h, compander.h
│   │   │   ├── dynamics.h, filter.h, eq_4band.h, isa110.h
│   │   │   ├── mastering_amp.h, maximizer.h, deafmax.h
│   │   │   ├── gain.h, mixer_node.h
│   │   │   └── envelope_follower.h, peak_meter.h, spectrum_analyzer.h
│   │   ├── polysonix/            # Polyphonic VM synthesizer
│   │   ├── tone_synth.h, tone_synth_graph.h  # Tone generation
│   │   ├── sound_source.h       # Sample playback source
│   │   ├── pcm_input.h          # Lock-free ring buffer PCM source
│   │   ├── mic_capture.h        # Microphone capture
│   │   ├── node_graph*.h        # Node graph system (6 files)
│   │   ├── device_*.h           # Device registry & wrappers (3 files)
│   │   ├── registry_init.h      # Built-in device registration
│   │   ├── midi*.h              # MIDI integration (4 files)
│   │   └── midi_learn.h         # MIDI Learn (dynamic CC mapping)
│   │
│   ├── kfs/                      # Filesystem sublibrary
│   ├── mybuddy/                  # NUMA-aware buddy allocator
│   ├── vid/                      # Video subsystem (planned)
│   └── k-term/                   # Terminal emulation library
│       ├── kterm_api.h           # Public terminal API
│       └── ...                   # VT100/VT220, ReGIS, Sixel, voice
│
├── examples/                     # Example programs (header-only model)
├── tests/harness/                # Test harness (links against DLL)
├── ext/                         # External dependencies
│   ├── glfw/                    # GLFW 3 windowing library
│   ├── cglm/                    # Math library (header-only)
│   ├── glad/                    # OpenGL 4.6 loader
│   ├── stb/                     # STB libraries (image, truetype)
│   ├── shaderc/                 # GLSL→SPIR-V compiler (Vulkan)
│   ├── cgltf/                   # glTF 2.0 model loading
│   └── miniaudio.h             # Audio backend (single header)
│
├── doc/                         # Documentation
└── shaders/                     # Shader files (Vulkan SPIR-V, GLSL)
```

## Quick Start

### Two Integration Models

**Model 1: DLL (Recommended for applications and tests)**

Build the library once, link against the DLL from any compiler:

```bash
# Build the DLL (GCC/MinGW required for this step only)
build_situation.bat opengl       # → build/dll/situation_opengl.dll
build_situation.bat vulkan       # → build/dll/situation_vulkan.dll

# Consume the DLL (any compiler — GCC, MSVC, Clang)
gcc -o myapp.exe main.c -I. -Iext -Iext/cglm/include -Iext/glfw/include \
    -DSITUATION_USE_OPENGL -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING \
    -Lbuild/dll -lsituation_opengl
```

**Model 2: Header-only (for self-contained single-file programs)**

Compile everything into one translation unit — no DLL needed:

```bash
gcc -o myapp.exe main.c -I. -Iext -Iext/cglm/include -Iext/glfw/include \
    -DSITUATION_USE_OPENGL -DSITUATION_ENABLE_THREADING \
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32
```

> **Note:** The DLL is built with GCC (MinGW-w64). Once built, the DLL exports a standard C ABI — consumers can link against it from MSVC, Clang, or any C/C++ compiler. The GCC requirement is only for building the library itself.

### Minimal Example

```c
// main.c
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL  // or SITUATION_USE_VULKAN
#include "situation.h"

int main(int argc, char** argv) {
    SituationInitInfo info = {
        .window_width = 1280,
        .window_height = 720,
        .window_title = "My App"
    };
    
    if (SituationInit(argc, argv, &info) != SITUATION_SUCCESS) {
        return -1;
    }
    
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        
        SituationCommandBuffer cmd = SituationAcquireFrameCommandBuffer();
        // ... rendering commands ...
        SituationEndFrame();
    }
    
    SituationShutdown();
    return 0;
}
```

### Compile Command (Windows/GCC — Header-Only Model)

```bash
gcc -o myapp.exe main.c \
    -std=c11 -I. -Iext -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps \
    -DSITUATION_USE_OPENGL -DSITUATION_IMPLEMENTATION -DSITUATION_ENABLE_THREADING \
    -Lext/glfw/lib-mingw-w64 \
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32 -lm
```

## Dependencies

### Required Dependencies

1. **GLFW3** - Windowing and input
   - Headers: `GLFW/glfw3.h`
   - Library: `glfw3.lib` (Windows), `libglfw3.a` (Linux/Mac)
   - Website: https://www.glfw.org/

2. **Graphics Backend** (choose one):
   - **OpenGL 4.6**: System OpenGL library + GLAD loader
   - **Vulkan 1.4+**: Vulkan SDK from LunarG

3. **cglm** - Math library
   - Headers only (no linking required)
   - Website: https://github.com/recp/cglm

### Embedded Dependencies (No Installation Required)

These are included in the `ext/` folder:

- **miniaudio** - Audio engine (single header)
- **stb_image** - Image loading (single header)
- **stb_image_write** - Image saving (single header)
- **stb_truetype** - Font rendering (single header)
- **VMA** - Vulkan Memory Allocator (Vulkan only)

### Optional Dependencies

- **shaderc** - GLSL to SPIR-V compiler (Vulkan compute shaders)

## Compilation Options

### Preprocessor Defines

#### Backend Selection (Required - Choose One)

```c
#define SITUATION_USE_OPENGL    // Use OpenGL 4.6 backend
#define SITUATION_USE_VULKAN    // Use Vulkan 1.4 backend
```

#### Implementation (Required in ONE file)

```c
#define SITUATION_IMPLEMENTATION  // Include implementation code
```

#### Optional Features

```c
#define SITUATION_ENABLE_SHADER_COMPILER  // Enable runtime GLSL compilation (Vulkan)
#define SITUATION_ENABLE_THREADING        // Enable thread pool API + dedicated threads (Main/Render/Audio/I/O)
#define SITUATION_ENABLE_RENDER_THREAD    // Enable dedicated render thread (included in default DLL builds)
#define SITUATION_ENABLE_DXGI            // Enable DXGI for GPU memory query (Windows)
```

> **Note:** `SITUATION_ENABLE_RENDER_THREAD` is automatically defined when `SITUATION_ENABLE_THREADING` is defined. You do not need to define it separately. The render thread is controlled at runtime via `init_info.render_thread_count` (0 = disabled, 1 = enabled).

#### Shared Library Build

```c
#define SITUATION_BUILD_SHARED   // When building as DLL
#define SITUATION_USE_SHARED     // When using DLL in application
```

### Include Paths

Always include these directories:

```bash
-I.                      # Project root (for situation.h)
-Iext                    # External dependencies
-Iext/glfw/include       # GLFW headers
-Iext/cglm/include       # cglm headers (if separate)
```

### Library Paths

#### Windows (MinGW/GCC)

```bash
-Lext/glfw/lib-mingw-w64  # GLFW library path
-L%VULKAN_SDK%/Lib        # Vulkan SDK (if using Vulkan)
```

#### Linux

```bash
-L/usr/local/lib          # System libraries
```

#### macOS

```bash
-L/usr/local/lib          # Homebrew libraries
-framework Cocoa          # macOS frameworks
-framework IOKit
-framework CoreVideo
```

### Linker Flags

#### Windows (OpenGL)

```bash
-lglfw3 -lopengl32 -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32
```

#### Windows (Vulkan)

```bash
-lglfw3 -lvulkan-1 -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32
```

#### Linux (OpenGL)

```bash
-lglfw -lGL -lm -lpthread -ldl -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama
```

#### Linux (Vulkan)

```bash
-lglfw -lvulkan -lm -lpthread -ldl -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama
```

#### macOS (OpenGL)

```bash
-lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
```

#### macOS (Vulkan)

```bash
-lglfw -lvulkan -framework Cocoa -framework IOKit -framework CoreVideo
```

## Platform-Specific Instructions

### Minimum Platform Requirements

| Platform | Minimum Version | Notes |
|----------|----------------|-------|
| **Windows** | **Windows 10** (build 1607+) | Uses `SetThreadDescription`, `RtlGetVersion`, WASAPI shared mode, DXGI 1.1. No Win7/8 compatibility. |
| **GPU (OpenGL)** | **OpenGL 4.6** | No fallback to older GL versions. Requires DSA, compute shaders, SPIR-V, MDI. |
| **GPU (Vulkan)** | **Vulkan 1.4** | No fallback to older VK versions. Device selection rejects < 1.4. |
| **Linux** | In progress | Not shipping yet. Target: X11 + PulseAudio/PipeWire. |
| **macOS** | In progress | Not shipping yet. Target: Metal via MoltenVK. |

### Windows (MinGW-w64)

```bash
# OpenGL Backend
gcc -o myapp.exe main.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_OPENGL \
    -DSITUATION_IMPLEMENTATION \
    -Lext/glfw/lib-mingw-w64 \
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32 \
    -static-libgcc -static-libstdc++

# Vulkan Backend
gcc -o myapp.exe main.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_VULKAN \
    -DSITUATION_IMPLEMENTATION \
    -DSITUATION_ENABLE_SHADER_COMPILER \
    -Lext/glfw/lib-mingw-w64 -L%VULKAN_SDK%/Lib \
    -lglfw3 -lvulkan-1 -lshaderc_shared -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32 \
    -static-libgcc -static-libstdc++
```

### Windows (MSVC)

> **Note:** MSVC can consume the pre-built DLL but cannot compile the library core (GCC-only internals). Build the DLL with `build_situation.bat` first, then link from MSVC.

```bash
# Link against pre-built DLL (consumer side)
cl /Fe:myapp.exe main.c ^
    /I. /Iext /Iext\cglm\include /Iext\glfw\include ^
    /DSITUATION_USE_OPENGL /DSITUATION_USE_SHARED /DSITUATION_ENABLE_THREADING ^
    /link situation_opengl.lib
```

### Linux (GCC) — _not yet shipping; planned target_

```bash
# OpenGL Backend
gcc -o myapp main.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_OPENGL \
    -DSITUATION_IMPLEMENTATION \
    -lglfw -lGL -lm -lpthread -ldl \
    -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama

# Vulkan Backend
gcc -o myapp main.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_VULKAN \
    -DSITUATION_IMPLEMENTATION \
    -DSITUATION_ENABLE_SHADER_COMPILER \
    -lglfw -lvulkan -lshaderc_shared -lm -lpthread -ldl \
    -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama
```

### macOS (Clang) — _not yet shipping; planned target_

```bash
# OpenGL Backend
clang -o myapp main.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_OPENGL \
    -DSITUATION_IMPLEMENTATION \
    -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

# Vulkan Backend (via MoltenVK)
clang -o myapp main.c \
    -I. -Iext -Iext/glfw/include -I$VULKAN_SDK/include \
    -DSITUATION_USE_VULKAN \
    -DSITUATION_IMPLEMENTATION \
    -DSITUATION_ENABLE_SHADER_COMPILER \
    -L$VULKAN_SDK/lib -lglfw -lvulkan -lshaderc_shared \
    -framework Cocoa -framework IOKit -framework CoreVideo
```

## Backend Selection

### OpenGL 4.6 Backend

**Pros:**
- Wider hardware support
- Simpler driver requirements
- Easier debugging with tools like RenderDoc

**Cons:**
- Less explicit control
- Older API design

**Requirements:**
- OpenGL 4.6 capable GPU
- GLAD loader (included in ext/)

**Compilation:**
```c
#define SITUATION_USE_OPENGL
#include "situation.h"
```

### Vulkan 1.4 Backend

**Pros:**
- Explicit control over GPU
- Better multi-threading support
- Modern API design
- Compute shader support

**Cons:**
- More complex
- Requires newer drivers
- Larger binary size

**Requirements:**
- Vulkan 1.4 capable GPU
- Vulkan SDK installed
- shaderc for runtime shader compilation

**Compilation:**
```c
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER  // For runtime GLSL→SPIR-V
#include "situation.h"
```

## Common Issues

### Issue: "situation.h: No such file or directory"

**Solution:** Make sure you're including the project root in your include path:
```bash
-I.  # or -I/path/to/situation/
```

### Issue: "undefined reference to `glfwInit`"

**Solution:** Link against GLFW library:
```bash
-lglfw3  # Windows
-lglfw   # Linux/macOS
```

### Issue: "vulkan-1.dll not found"

**Solution:** 
1. Install Vulkan SDK from LunarG
2. Add `%VULKAN_SDK%\Bin` to PATH
3. Or copy `vulkan-1.dll` to your exe directory

### Issue: "Multiple definition of `SituationInit`"

**Solution:** Only define `SITUATION_IMPLEMENTATION` in ONE source file.

### Issue: Linker errors about OpenGL functions

**Solution:** Make sure you're linking the OpenGL library:
```bash
-lopengl32  # Windows
-lGL        # Linux
-framework OpenGL  # macOS
```

### Issue: "shaderc_shared.dll not found"

**Solution:** For Vulkan compute shaders, copy `shaderc_shared.dll` from Vulkan SDK to your exe directory, or add SDK bin folder to PATH.

## Example Build Scripts

See also [KaOS Terminal Console](#kaos-terminal-console) for the canonical K-Term reference app.

### Windows Batch Script (compile_example.bat)

```batch
@echo off
echo Compiling with OpenGL backend...

gcc -o example.exe examples/example.c ^
    -I. -Iext -Iext/glfw/include ^
    -DSITUATION_USE_OPENGL ^
    -DSITUATION_IMPLEMENTATION ^
    -Lext/glfw/lib-mingw-w64 ^
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -lws2_32 -lole32 -lshell32 -luser32 ^
    -static-libgcc -static-libstdc++

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed
    exit /b 1
)

echo [SUCCESS] example.exe built!
echo Running...
example.exe
```

### Linux Shell Script (compile_example.sh)

```bash
#!/bin/bash
echo "Compiling with OpenGL backend..."

gcc -o example examples/example.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_OPENGL \
    -DSITUATION_IMPLEMENTATION \
    -lglfw -lGL -lm -lpthread -ldl \
    -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama

if [ $? -ne 0 ]; then
    echo "[ERROR] Build failed"
    exit 1
fi

echo "[SUCCESS] example built!"
echo "Running..."
./example
```

### CMakeLists.txt Example

```cmake
cmake_minimum_required(VERSION 3.10)
project(SituationApp)

set(CMAKE_C_STANDARD 11)

# Add source files
add_executable(myapp main.c)

# Include directories
target_include_directories(myapp PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ext
    ${CMAKE_SOURCE_DIR}/ext/glfw/include
)

# Compile definitions
target_compile_definitions(myapp PRIVATE
    SITUATION_USE_OPENGL
    SITUATION_IMPLEMENTATION
)

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Link libraries
target_link_libraries(myapp PRIVATE
    OpenGL::GL
    glfw
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(myapp PRIVATE
        gdi32 winmm ws2_32 ole32 shell32 user32
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(myapp PRIVATE
        m pthread dl
        X11 Xrandr Xi Xxf86vm Xcursor Xinerama
    )
elseif(APPLE)
    target_link_libraries(myapp PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()
```

## KaOS Terminal Console

**KaOS Terminal** (`examples/kterm_console.c`) is a **K-Term core product** — the canonical terminal + shell reference app. It lives under **Situation examples** during development because K-Term is exercised through a Situation host frame (window, input, virtual-display compositor). It is **not** part of `situation.h` or Situation core releases.

- **Product / changelog:** `sit/k-term/doc/updatelog.md`, `doc/plan/KTERM_CONSOLE_*`
- **Build location (dev):** `examples/kterm_console.c` in this repo
- **Situation role:** host platform only (`SituationInit`, VD compositing, sysinfo APIs)

Legacy `console.c` was merged into `kterm_console` (see `doc/plan/CONSOLE_MERGE_DEPRECATION_PLAN.md`).

### Build (Windows)

```batch
build_examples.bat opengl kterm_console
build\examples\kterm_console.exe
```

CMake (from project root, when using the examples target):

```batch
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target kterm_console
```

Quick verify script:

```batch
scripts\verify_kterm_console.bat
```

### Headless screenshot capture (testing)

Set environment variables before launch:

| Variable | Effect |
|----------|--------|
| `KTERM_CAPTURE_SCREENSHOT` | Path to PNG written on frame 30 |
| `KTERM_CAPTURE_EXIT` | Exit after capture (optional) |

```batch
set KTERM_CAPTURE_SCREENSHOT=shot.png
set KTERM_CAPTURE_EXIT=1
build\examples\kterm_console.exe
```

Harness (after building the example):

```batch
build_tests.bat
build\sit_test.exe --module kterm_console
```

## Additional Resources

- **Main Documentation**: `doc/situation_api.md`
- **Complete API Index** (auto-generated): `doc/situation_api_index.md`
- **API supplement** (header-sync gaps): `doc/situation_api_generated.md` — regenerate with `python scripts/generate_situation_api_docs.py`
- **Quick Reference**: `doc/SITUATION_QUICK_REFERENCE.md`
- **SDK Manual**: `doc/situation_sdk.md`
- **Examples**: `examples/` directory
- **Folder Structure**: `doc/V2_4_0_FOLDER_REORGANIZATION_COMPLETE.md`

## Support

For issues and questions:
1. Check the FAQ in `doc/situation_api.md`
2. Review example programs in `examples/`
3. Check compilation scripts in project root (`compile_*.bat`)

## Version History

- **v2.4.203** (2026-06-05) - Error propagation Phase 3 (bool/void → SituationError migration)
- **v2.4.200** (2026-06-04) - API documentation refresh, full 531-function coverage
- **v2.4.199** (2026-06-03) - System introspection APIs
- **v2.4.0** (2026-03-03) - Folder reorganization, audio subsystem organization
- **v2.3.x** - Previous versions (see UPDATELOG.md)
