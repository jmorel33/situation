# Situation Library - Compilation Guide

**Version**: 2.4.0  
**Date**: 2026-03-03  
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

## Project Structure

```
situation/                         # Project root
├── situation.h                    # ← Public API entry point (include this)
│
├── sit/                          # ← Core implementation (internal)
│   ├── situation_api.h           # Public API declarations
│   ├── situation_impl.h          # Core implementation
│   ├── situation_impl_audio.h    # Audio subsystem
│   │
│   ├── aud/                      # Audio Subsystem
│   │   ├── fx/                   # Effects (15 files)
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
├── ext/                         # External dependencies
│   ├── glfw/                    # GLFW windowing library
│   ├── cglm/                    # Math library
│   ├── glad/                    # OpenGL loader (if using OpenGL)
│   └── stb/                     # STB libraries (image, truetype)
│
├── doc/                         # Documentation
└── shaders/                     # Shader files
```

## Quick Start

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

### Compile Command (Windows/GCC)

```bash
gcc -o myapp.exe main.c \
    -I. -Iext -Iext/glfw/include \
    -DSITUATION_USE_OPENGL \
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -lws2_32
```

## Dependencies

### Required Dependencies

1. **GLFW3** - Windowing and input
   - Headers: `GLFW/glfw3.h`
   - Library: `glfw3.lib` (Windows), `libglfw3.a` (Linux/Mac)
   - Website: https://www.glfw.org/

2. **Graphics Backend** (choose one):
   - **OpenGL 4.6**: System OpenGL library + GLAD loader
   - **Vulkan 1.2+**: Vulkan SDK from LunarG

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
#define SITUATION_USE_VULKAN    // Use Vulkan 1.2+ backend
```

#### Implementation (Required in ONE file)

```c
#define SITUATION_IMPLEMENTATION  // Include implementation code
```

#### Optional Features

```c
#define SITUATION_ENABLE_SHADER_COMPILER  // Enable runtime GLSL compilation (Vulkan)
#define SITUATION_ENABLE_THREADING        // Enable multi-threading support
#define SITUATION_ENABLE_DXGI            // Enable DXGI for GPU info (Windows)
```

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

```bash
# OpenGL Backend
cl /Fe:myapp.exe main.c ^
    /I. /Iext /Iext\glfw\include ^
    /DSITUATION_USE_OPENGL ^
    /DSITUATION_IMPLEMENTATION ^
    /link glfw3.lib opengl32.lib gdi32.lib winmm.lib ws2_32.lib ole32.lib shell32.lib user32.lib

# Vulkan Backend
cl /Fe:myapp.exe main.c ^
    /I. /Iext /Iext\glfw\include /I%VULKAN_SDK%\Include ^
    /DSITUATION_USE_VULKAN ^
    /DSITUATION_IMPLEMENTATION ^
    /DSITUATION_ENABLE_SHADER_COMPILER ^
    /link glfw3.lib vulkan-1.lib shaderc_shared.lib gdi32.lib winmm.lib ws2_32.lib ole32.lib shell32.lib user32.lib ^
    /LIBPATH:%VULKAN_SDK%\Lib
```

### Linux (GCC)

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

### macOS (Clang)

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

### Vulkan 1.2+ Backend

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
- Vulkan 1.2+ capable GPU
- Vulkan SDK installed
- shaderc for compute shaders

**Compilation:**
```c
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER  // For compute shaders
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

- **v2.4.0** (2026-03-03) - Folder reorganization, audio subsystem organization
- **v2.3.x** - Previous versions (see UPDATELOG.md)
