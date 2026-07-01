# Situation Library - Compilation Guide

**Version**: 2.4.403
**Date**: 2026-06-29
**Status**: Current

## Overview

This guide covers everything you need to compile, link, and ship applications using the
Situation library — from a quick one-liner to a full cross-language static build.

## Table of Contents

1. [Project Structure](#project-structure)
2. [Quick Start](#quick-start)
3. [Building the Library](#building-the-library) → [BUILD_SITUATION_GUIDE.md](BUILD_SITUATION_GUIDE.md)
4. [Building Examples](#building-examples)
5. [Building and Running Tests](#building-and-running-tests)
6. [Debug Builds](#debug-builds)
7. [Language Wrappers](#language-wrappers)
8. [Dependencies](#dependencies)
9. [Compilation Options](#compilation-options)
10. [Platform-Specific Instructions](#platform-specific-instructions)
11. [Backend Selection](#backend-selection)
12. [Common Issues](#common-issues)
13. [Application Identity (Windows PE resources)](#application-identity-windows-pe-resources)
14. [K-Term Console](#k-term-console)

---

## Project Structure

```
situation/                           # Project root
├── situation.h                      # Public API entry point (include this)
├── situation_dll.c                  # DLL compilation unit
│
├── sit/                             # Core implementation
│   ├── Makefile                     # Library build system (invoked by build_situation.bat)
│   ├── platform/                    # Platform-specific build resources
│   │   ├── windows/                 # Windows: .rc files, .ico
│   │   │   ├── situation_resource.rc  # DLL version info + icon (windres)
│   │   │   ├── sit_app.rc             # Default EXE icon + VS_VERSION_INFO
│   │   │   ├── sit_app_template.rc    # Author override template (-DAPP_*)
│   │   │   └── situation_icon.ico     # Multi-resolution icon (6 sizes)
│   │   ├── linux/                   # (future) .desktop, XDG icons
│   │   └── macos/                   # (future) Info.plist, .icns
│   ├── situation_api.h              # Public API umbrella (includes situation_api_*.h only)
│   ├── situation_profiling.h        # Tracy CPU zone macros (P10.2 — included via situation.h, not API)
│   ├── situation_api_config.h       # Init, lifecycle config
│   ├── situation_api_types_*.h      # System / GPU / audio types
│   ├── situation_api_platform.h     # Window, input, display
│   ├── situation_api_graphics.h     # Command buffer, shaders, compute
│   ├── situation_api_audio.h        # Audio graph, MIDI, devices
│   ├── situation_api_system.h       # Threading, filesystem, introspection
│   ├── situation_api_deprecated.h     # Legacy symbols (optional gate)
│   ├── situation_base_version.h     # Canonical version macros
│   ├── situation_base_errno.h       # SituationError enum
│   ├── situation_base_types.h       # Primitive types, handles, audio stream types
│   ├── situation_base_callbacks.h   # Callback typedef signatures
│   ├── situation_base_etc.h         # Key codes, color constants, MIDI table
│   ├── situation_base_font.h        # Embedded VGA bitmap font
│   ├── situation_impl_image.h       # Image, font, color (grid/bitmap font builders)
│   ├── situation_base_trace.h       # Trace hooks (generated — do not edit)
│   ├── situation_impl*.h            # Implementation modules (ctrl, wdm, vd, …)
│   ├── situation_impl_renderer.h    # Renderer orchestrator (includes slices below)
│   ├── situation_impl_renderer_*.h  # Renderer slices: core, lc, shader, resources, frame_cmd
│   ├── situation_impl_renderer_fwd.h
│   ├── aud/                         # Audio subsystem (node graph, FX, MIDI, synth)
│   │   ├── fx/                      # 23 DSP effect nodes
│   │   ├── polysonix/               # Polyphonic VM synthesizer
│   │   └── ...
│   ├── kfs/                         # Kaizen Filing System — see sit/kfs/doc/COMPILATION_GUIDE.md
│   ├── mybuddy/                     # NUMA-aware buddy allocator
│   ├── vid/                         # Video subsystem (planned)
│   └── k-term/                      # Terminal emulation library
│
├── build/                           # Build scripts and output
│   ├── build_situation.bat          # Thin launcher → sit/Makefile
│   ├── tracy_client.cpp             # Tracy client single-TU (SIT_TRACY=1 only)
│   ├── build_shaderc.bat            # Build ext/shaderc (Vulkan DLL prerequisite)
│   ├── build_situation_legacy.bat   # Verbatim original script (fail-safe)
│   ├── build_examples.bat           # Build example programs
│   ├── build_tests.bat              # Build the test harness
│   ├── run_tests.bat                # Run harness with result logging
│   ├── build_odin_example.bat       # Odin wrapper examples
│   ├── build_zig_example.bat        # Zig wrapper examples
│   ├── build_rust_example.bat       # Rust wrapper examples
│   ├── build_fortran_example.bat    # Fortran wrapper examples
│   ├── build_modula2_example.bat    # Modula-2 wrapper examples
│   ├── build_python_example.bat     # Python wrapper examples (PyInstaller .exe)
│   ├── build_lua_example.bat        # Lua wrapper examples (embedded self-contained .exe)
│   ├── run_lua_dev.bat              # Dev: run staged Lua sources with external DLL
│   ├── compile_harness_shaders.bat  # Precompile harness SPIR-V
│   ├── compile_demon_hunt_shaders.bat
│   └── dll/                         # Build output
│       ├── situation_opengl.dll / .a / .def / .lib
│       └── situation_vulkan.dll / .a / .def / .lib
│
├── debug.bat                        # Debug rebuild (-O0 -g) + GDB test harness launcher
│
├── ext/                             # External dependencies (bundled)
│   ├── glfw/                        # GLFW 3 — windowing and input
│   ├── cglm/                        # Math library (header-only)
│   ├── glad/                        # OpenGL 4.6 loader
│   ├── stb/                         # stb_image, stb_truetype, stb_image_write
│   ├── miniaudio.h                  # Audio backend (single header)
│   ├── shaderc/                     # GLSL→SPIR-V compiler (Vulkan)
│   ├── cgltf/                       # glTF 2.0 loader
│   ├── tracy/public/                # Tracy profiler client (optional; sparse clone)
│   └── vma_wrapper.cpp              # Vulkan Memory Allocator
│
├── examples/                        # Example programs
│   ├── 01_open_a_window/main.c
│   ├── 02_draw_shapes/main.c
│   ├── 03_keyboard_and_mouse/main.c
│   ├── 04_play_a_sound/main.c
│   ├── 05_virtual_display_retro/main.c
│   ├── 06_audio_node_graph/main.c
│   ├── 07_ypq_color_grading/main.c
│   ├── 08_temporal_oscillators/main.c
│   ├── 09_midi_control/main.c
│   ├── 10_thread_pool/main.c
│   ├── 11_music_visualizer/main.c
│   ├── 12_procedural_world/main.c
│   ├── console/console_host_app.c   # K-Term console host
│   ├── demon_hunt/                  # Vulkan-only showcase
│   └── other/                       # Misc dev examples
│
├── tests/harness/                   # Test harness (links against static lib or DLL)
├── tools/                           # Binding generators + API index (see tools/README.md)
├── wrappers/                        # Generated language bindings
│   ├── Odin/
│   ├── Zig/
│   ├── Rust/
│   ├── Fortran/
│   ├── Modula2/
│   ├── Python/                      # ctypes package (situation/)
│   └── lua/                         # LuaJIT FFI package (situation/) + embedded host
├── scripts/                         # Shared wrapper build helpers (wrapper_*.bat); renderer gates: verify_renderer_fwd.py, inventory_renderer_module.py
├── _languages/                      # Bundled compilers (Odin, Zig, Rust, gm2, LuaJIT)
└── doc/                             # Documentation (guide/, UPDATELOG.md, this file)
```

---

## Quick Start

### Two Integration Models

**Model 1 — Static library (recommended)**

A self-contained exe. No DLL needed at runtime.

```bat
build\build_situation.bat static-opengl
```

Then link your app:

```bat
gcc -o myapp.exe main.c ^
    -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include ^
    -DSITUATION_USE_OPENGL -DSITUATION_ENABLE_THREADING ^
    -Lext/glfw/build/src build/dll/situation_opengl.a ^
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lbcrypt -lm ^
    -static-libgcc
```

**Model 2 — DLL (faster iteration)**

The DLL must live next to the exe at runtime. Build time is ~5× faster than static.

```bat
build\build_situation.bat opengl
```

Then link your app:

```bat
gcc -o myapp.exe main.c ^
    -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include ^
    -DSITUATION_USE_OPENGL -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING ^
    -Lbuild/dll -lsituation_opengl ^
    -static-libgcc -lm
```

> The DLL exports a standard C ABI. Once built with GCC, it can be consumed from MSVC, Clang, Odin, Zig, or Rust.

### Minimal Example Program

```c
// main.c
#define SITUATION_USE_OPENGL   // or SITUATION_USE_VULKAN
#include "situation.h"

int main(int argc, char** argv) {
    SituationInitInfo info = {
        .window_width  = 1280,
        .window_height = 720,
        .window_title  = "My App"
    };

    if (SituationInit(argc, argv, &info) != SITUATION_SUCCESS)
        return -1;

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();

        if (SituationAcquireFrameCommandBuffer() == SITUATION_SUCCESS) {
            // record render commands
            SituationEndFrame();
        }
    }

    SituationShutdown();
    return 0;
}
```

---

## Building the Library

> **Full reference:** [BUILD_SITUATION_GUIDE.md](BUILD_SITUATION_GUIDE.md)

`build\build_situation.bat` is a thin launcher that forwards targets to `sit/Makefile`
via `mingw32-make`. Six targets are available:

| Target | Output |
|--------|--------|
| `static-opengl` | `build/dll/situation_opengl.a` — recommended |
| `static-vulkan` | `build/dll/situation_vulkan.a` — recommended |
| `opengl` | `build/dll/situation_opengl.dll` + `.def` + `.lib` |
| `vulkan` | `build/dll/situation_vulkan.dll` + `.def` + `.lib` |
| `all` | both DLLs |
| `all-static` | both static archives |
| `clean` | removes `*.o` and `*.dll` from `build/dll/` |
| `distclean` | removes everything (`*.o` `*.dll` `*.a` `*.def` `*.lib`) |

```bat
build\build_situation.bat static-opengl
build\build_situation.bat static-vulkan
```

**OpenGL targets** require only MSYS2 MinGW-w64 and cmake. GLFW is built
automatically on first use if `libglfw3.a` is missing.

**Vulkan targets** additionally require the Vulkan SDK (system installer,
autodetected from `C:\VulkanSDK\*`). The `vulkan` DLL target also requires
shaderc — run `build\build_shaderc.bat` once before building the DLL.
`static-vulkan` does not link shaderc and skips that check.

If any prerequisite is missing the Makefile prints a full diagnostic with exact
remediation steps. See [BUILD_SITUATION_GUIDE.md](BUILD_SITUATION_GUIDE.md) for
the complete reference including environment overrides, what gets compiled, and
the version stamping mechanism.

---

## Building shaderc

Required for the **`vulkan` DLL** target only. `static-vulkan` does not need it.

From the project root:

```bat
build\build_shaderc.bat
```

| Command | Effect |
|---------|--------|
| `build\build_shaderc.bat` | Build if `libshaderc_combined.a` is missing (skips if already present) |
| `build\build_shaderc.bat rebuild` | Remove `ext\shaderc\build` and rebuild from scratch |
| `build\build_shaderc.bat sync` | Run `git-sync-deps` only (fetch `third_party` sources) |
| `build\build_shaderc.bat clean` | Remove `ext\shaderc\build` |

**Prerequisites:** MSYS2 MinGW-w64 (`gcc`, `g++`, `cmake`, `mingw32-make`, `python`), plus `git` on PATH for dependency sync.

**Outputs:**

| Artifact | Used by |
|----------|---------|
| `ext\shaderc\build\libshaderc\libshaderc_combined.a` | `build\build_situation.bat vulkan` (runtime GLSL→SPIR-V) |
| `ext\shaderc\build\glslc\glslc.exe` | `compile_harness_shaders`, `compile_demon_hunt_shaders`, `compile_vd_compositor_gl.ps1` |

First build typically takes **10–30 minutes**. Set `MINGW_PATH` if MinGW is not at `C:\msys64\mingw64\bin`.

After shaderc is built:

```bat
build\build_situation.bat vulkan
```

Or skip shaderc entirely with the self-contained static archive:

```bat
build\build_situation.bat static-vulkan
```

---

## Building Examples

### Script

```bat
build\build_examples.bat [backend] [example]
```

### Backends

| Backend | Link model | Prerequisite |
|---------|------------|--------------|
| `opengl` | DLL-linked OpenGL | `build\build_situation.bat opengl` |
| `vulkan` | DLL-linked Vulkan | `build\build_situation.bat vulkan` |
| `static-opengl` | Self-contained OpenGL exe | `build\build_situation.bat static-opengl` |
| `static-vulkan` | Self-contained Vulkan exe | `build\build_situation.bat static-vulkan` |

DLL modes copy the matching `situation_*.dll` next to the exe automatically.

### Available Examples

**Numbered digestible examples** — use either the full folder name or the short name:

```bat
build\build_examples.bat static-opengl  01_open_a_window        (or: open_a_window)
build\build_examples.bat static-opengl  02_draw_shapes          (or: draw_shapes)
build\build_examples.bat static-opengl  03_keyboard_and_mouse   (or: keyboard_and_mouse)
build\build_examples.bat static-opengl  04_play_a_sound         (or: play_a_sound)
build\build_examples.bat static-opengl  05_virtual_display_retro
build\build_examples.bat static-opengl  06_audio_node_graph
build\build_examples.bat static-opengl  07_ypq_color_grading
build\build_examples.bat static-opengl  08_temporal_oscillators
build\build_examples.bat static-opengl  09_midi_control
build\build_examples.bat static-opengl  10_thread_pool
build\build_examples.bat static-opengl  11_music_visualizer
build\build_examples.bat static-opengl  12_procedural_world
```

**Named examples:**

```bat
build\build_examples.bat opengl         kterm_console
build\build_examples.bat opengl         node_graph_piano_demo
build\build_examples.bat opengl         quad_storm
build\build_examples.bat static-vulkan  demon_hunt          (Vulkan-only)
```

> `demon_hunt` is Vulkan-only. Its sky shader exceeds OpenGL SPIR-V instruction limits.

### Output

All built examples land in `build\examples\`. DLL-linked examples have the matching
`situation_*.dll` copied there automatically.

---

## Building and Running Tests

### Build

```bat
build\build_tests.bat [backend]
```

| Backend | Output | Notes |
|---------|--------|-------|
| `static-opengl` | `build\tests\sit_test_opengl.exe` | Self-contained, recommended |
| `static-vulkan` | `build\tests\sit_test_vulkan.exe` | Self-contained, recommended |
| `opengl` | `build\tests\sit_test_opengl.exe` | DLL-linked, use `run_tests.bat` |
| `vulkan` | `build\tests\sit_test_vulkan.exe` | DLL-linked, use `run_tests.bat` |

Static builds are self-contained (no DLL at runtime) and are the recommended default.
DLL builds link faster but require `build\dll\` to be on PATH, which `run_tests.bat`
handles automatically.

**Prerequisite:** build the matching library artifact first:

```bat
build\build_situation.bat static-opengl
build\build_tests.bat static-opengl
```

### Run — Direct (static builds)

```bat
build\tests\sit_test_opengl.exe                          run all modules
build\tests\sit_test_opengl.exe --module filesystem      run one module
build\tests\sit_test_opengl.exe --filter buffer          run tests matching substring
build\tests\sit_test_opengl.exe --verbose                show all assertions
build\tests\sit_test_opengl.exe --list                   list tests without running
build\tests\sit_test_opengl.exe --stop-on-fail           abort on first failure
```

### Run — Via Launcher (saves timestamped results)

`run_tests.bat` prepends `build\dll\` to PATH (covers DLL-linked builds) and saves
a timestamped result file to `build\tests\results\`:

```bat
build\run_tests.bat opengl
build\run_tests.bat vulkan --module graphics --filter spirv
build\run_tests.bat vulkan --module audio --verbose
```

### Test Modules

`filesystem`, `threading`, `core`, `window`, `input`, `timer`, `graphics`,
`graphics_spirv`, `text_rendering`, `virtual_display`, `compute`, `transfer`,
`model_loader`, `stl_loader`, `obj_loader`, `projection_3d`, `audio`,
`tone_synth`, `audio_effects_heard`, `misc`, `system_info`, `kterm_console`,
`advanced`, `proj`

---

## Debug Builds

Situation ships two complementary debug workflows:

### 1. `debug.bat` — library + test harness under GDB

Root-level **`debug.bat`** rebuilds the library and test harness with debug symbols, then launches **`build\tests\sit_test_<backend>.exe`** under GDB.

```bat
debug.bat static-vulkan --module virtual_display
debug.bat opengl --filter spirv --verbose
debug.bat --no-build vulkan --module graphics
debug.bat --rebuild --break _SitVulkanEnsureGraphicsPipelineBound static-vulkan
debug.bat clean
debug.bat distclean
```

| Option | Effect |
|--------|--------|
| `--no-build` | Skip compile; run GDB on existing `sit_test_*.exe` |
| `--rebuild`, `-B` | Force clean library rebuild via `mingw32-make -B` |
| `--break <fn>` | GDB breakpoint (default: `_SituationSetErrorFromCode`) |
| `clean` / `distclean` | Forward to `sit/Makefile` (no GDB) |

Targets: `opengl`, `vulkan`, `static-opengl`, `static-vulkan` — same vocabulary as `build_situation.bat` / `build_tests.bat`.

By default the script sets **`SIT_OPTIMIZE_CFLAGS=-O0 -g`** and **`EXTRA_VULKAN_CFLAGS=-O0 -g`** before invoking the Makefile (unless you already exported overrides).

### 2. Manual debug flags — any target

For C examples, wrappers, or a one-off library rebuild without GDB:

```bat
set SIT_OPTIMIZE_CFLAGS=-O0 -g
set EXTRA_VULKAN_CFLAGS=-O0 -g
build\build_situation.bat static-opengl
build\build_tests.bat static-opengl
build\build_examples.bat static-opengl 01_open_a_window
```

Release defaults (`-O2 -mfma -ffp-contract=fast`) and all Makefile env overrides are documented in **[BUILD_SITUATION_GUIDE.md](BUILD_SITUATION_GUIDE.md)** → *Environment Overrides*.

> **Note:** `debug.bat` does **not** rebuild C examples or language-wrapper demos — only the library + harness. Use the manual flags above for those, or run `debug.bat --no-build` after building tests yourself.

### Runtime debug env vars (harness / diagnostics)

| Variable | Effect |
|----------|--------|
| `SIT_TEST_DEBUG_GL=1` | Extra OpenGL state checks / logging in harness paths |
| `NDEBUG` undefined | Enables `SITUATION_ASSERT` and additional stderr diagnostics in debug library builds |

See **`doc/guide/logging.md`** and harness docs for overlay keys (e.g. M-key metrics in shared examples).

---

## Language Wrappers

Generated bindings live in `wrappers/` (Odin, Zig, Rust, Fortran, Modula-2, **Python**, **Lua**). Generators and
the shared parser live in `tools/` — see `tools/README.md`. Dedicated build scripts in
`build\` compile (or run) the wrapper demos using the same backend vocabulary as the C scripts.

### Canonical example: `hello_situation`

Every language uses the same demo — **Raster Bars + Ambient Synth** (~400 lines): raymarched
torus shader (VK push constants / GL uniforms), `ToneSynth → Echo → Reverb` audio graph,
virtual MIDI, interactive FX keys, and HUD text. Reference implementations:

- `wrappers/Rust/examples/hello_situation.rs`
- `wrappers/Zig/examples/hello_situation/main.zig`
- `wrappers/Odin/examples/hello_situation/hello.odin`
- `wrappers/Fortran/examples/hello_situation/main.f90`
- `wrappers/Modula2/examples/hello_situation/Main.mod`
- `wrappers/Python/examples/hello_situation.py`
- `wrappers/lua/examples/hello_situation.lua`

New language wrappers must port this demo in full, not a minimal init/shutdown stub.

### Build Scripts

```bat
build\build_odin_example.bat     [backend] [example_name]
build\build_zig_example.bat      [backend] [example_name]
build\build_rust_example.bat     [backend] [example_name]
build\build_fortran_example.bat  [backend] [example_name]
build\build_modula2_example.bat  [backend] [example_name]
build\build_python_example.bat   [backend] [example_name]
build\build_lua_example.bat      [backend] [example_name]
```

`example_name` defaults to `hello_situation`. Backend tokens match
`build_examples.bat`: `opengl`, `vulkan`, `static-opengl`, `static-vulkan`.

**Lua** uses a **self-contained embedded exe** (not loose `luajit.exe` + DLLs). The build embeds
Lua bytecode, `situation_*.dll`, and `lua51.dll`; at runtime they extract to `%TEMP%`. Only
`opengl` / `vulkan` backends are supported. See **`wrappers/lua/README.md`**.

**Python** uses **stdlib ctypes** for bindings (no C compiler for the package itself). The build pipeline mirrors other wrappers:

1. **`scripts/wrapper_compile_python.bat`** — stage example + `situation/` package under `build\obj\python\<example>_<backend>\stage\`, then **`python -m compileall`** (syntax check).
2. **`scripts/wrapper_link_python.bat`** — **`PyInstaller --onefile`** → `build\examples\python\<example>.exe`, copy `situation_*.dll` alongside (DLL mode).

Requires **`pip install pyinstaller`** (or MSYS2: `pacman -S mingw-w64-x86_64-pyinstaller`). Only **`opengl` / `vulkan` DLL** modes (no static link).

```bat
build\build_python_example.bat opengl hello_situation --no-run   REM compile + link .exe only
build\build_python_example.bat opengl hello_situation            REM build .exe + run
```

```bat
build\build_odin_example.bat     opengl
build\build_zig_example.bat      static-opengl
build\build_rust_example.bat     vulkan hello_situation
build\build_fortran_example.bat  opengl hello_situation
build\build_modula2_example.bat  opengl hello_situation
build\build_python_example.bat   opengl hello_situation
build\build_lua_example.bat      opengl hello_situation
```

### Output

Wrapper executables/scripts — never mix with C examples in `build\examples\` root:

| Language | Output directory | Intermediate objects |
|----------|-----------------|----------------------|
| Odin | `build\examples\odin\` | `wrappers\Odin\` build cache |
| Zig | `build\examples\zig\` | `wrappers\Zig\.zig-cache\` |
| Rust | `build\examples\rust\` | `wrappers\Rust\target\` |
| Fortran | `build\examples\fortran\` | `build\obj\fortran\` |
| Modula-2 | `build\examples\modula2\` | `build\obj\modula2\` |
| Python | `build\examples\python\` | `build\obj\python\` (PyInstaller work + stage) |
| Lua | `build\examples\lua\` | `build\obj\lua\` (embed + dll_embed + `.o`) |

DLL modes copy the matching `situation_*.dll` into the language output folder automatically.
**Lua** embeds the DLL inside the exe instead — the output folder contains only `<example>.exe`.

### Static Mode Support Matrix

| Language | `static-opengl` | `static-vulkan` | Notes |
|----------|:-:|:-:|-------|
| C (`build_examples.bat`) | ✅ | ✅ | Reference path |
| Rust | ✅ | ✅ | GCC linker; `build.rs` handles `libgcc_eh` for emulated TLS |
| Fortran | ✅ | ✅ | `gfortran` + `wrapper_gcc_link_static.bat`; Vulkan final link via `g++` |
| Modula-2 | ✅ | ✅ | `gm2 -c` then `gcc`/`g++` link |
| Python | ❌ | ❌ | **DLL only v1** — use `opengl` / `vulkan` |
| Lua | ❌ | ❌ | **Embedded DLL exe** — `opengl` / `vulkan` only; Situation + lua51 DLLs ship inside the exe |
| Zig | ✅ | ❌ | `lld-link` cannot resolve MinGW C++ runtime archives that shaderc/VMA require |
| Odin | ❌ | ❌ | `lld-link` (MSVC CRT) incompatible with MinGW `.a` archives — use DLL mode |

> For Odin and Zig Vulkan: use DLL mode (`vulkan`). For portable C binaries: `build_examples.bat static-opengl`.

### Regenerating Bindings

```bat
python tools\generate_odin_bindings.py
python tools\generate_zig_bindings.py
python tools\generate_rust_bindings.py
python tools\generate_fortran_bindings.py
python tools\generate_modula2_bindings.py
python tools\generate_python_bindings.py
python tools\generate_lua_bindings.py

REM Or all at once:
tools\run_all.bat
```

Python package layout and usage: **`wrappers/Python/README.md`**. Lua package and embedded host: **`wrappers/lua/README.md`**. Use `helpers.init_info_window()` (Python/Lua) for ABI-safe `SituationInitInfo` — do not hand-fill struct fields in demos.

### Shared Helper Scripts

All wrapper `build\*_example.bat` entry points are thin dispatchers; shared logic lives in `scripts\`:

| Script | Role |
|--------|------|
| `wrapper_link_config.bat` | Backend → `SIT_DLL_*`, `SIT_STATIC_A`, language-specific link env |
| `wrapper_mingw_setup.bat` | Add MinGW-w64 `bin` to `PATH` |
| `wrapper_paths.bat` | Standard `build\examples\<lang>\` output + `build\obj\<lang>\` intermediates |
| `wrapper_ensure_import_lib.bat` | Generate MinGW `.lib` from Situation DLL |
| `wrapper_link_dll.bat` | Link `.o` files against `build\dll\situation_*.lib`, copy DLL |
| `wrapper_gcc_link_static.bat` | Self-contained static OpenGL/Vulkan link (gcc/g++/gfortran/gm2) |
| `wrapper_compile_fortran.bat` | Compile `wrappers/Fortran/src/*.f90` + example |
| `wrapper_compile_modula2.bat` | Compile `wrappers/Modula2` bindings + example |
| `wrapper_compile_python.bat` | Stage `build/obj/python/.../stage/` + `python -m compileall` |
| `wrapper_link_python.bat` | PyInstaller `--onefile` → `build/examples/python/*.exe` + copy DLL |
| `wrapper_compile_lua.bat` | Stage Lua sources; `gen_lua_embed.py` + `gen_lua_dll_embed.py` |
| `wrapper_link_lua.bat` | Link embedded host (`sit_lua_host.c`, runtime, draw shim) → single `.exe` |
| `wrapper_patch_odin_foreign.bat` / `wrapper_restore_odin_foreign.bat` | Patch Odin `foreign import` for active backend |

### Fortran & Modula-2 toolchains

Fortran uses MSYS2 `gfortran` (`pacman -S mingw-w64-x86_64-gcc-fortran`).

Modula-2 (`gm2`) has no MSYS2 binary package — bundle under `_languages\gm2\` or build GCC
with `--enable-languages=m2`. See **`doc/plan/FORTRAN_MODULA2_BINDINGS_PLAN.md`** for the full
implementation plan and `hello_situation` port requirements.

---

## Dependencies

### Toolchain (Windows)

| Tool | Source | Notes |
|------|--------|-------|
| `gcc` / `g++` | MSYS2 MinGW-w64 | Required to build the library |
| `ar` | MinGW-w64 | Static archive creation |
| `windres` | MinGW-w64 | Windows resource compiler (DLL version stamping) |
| `gendef` / `dlltool` | MinGW-w64 | Generate `.def` and `.lib` from DLL |
| `mingw32-make` | MinGW-w64 | Drives `sit/Makefile` |
| `gfortran` | MSYS2 (optional) | Fortran wrappers — `mingw-w64-x86_64-gcc-fortran`; not in default toolchain |
| `gm2` | GCC build / `_languages\gm2\` bundle | Modula-2 wrappers — no MSYS2 package; bindings generate without it |
| LuaJIT (`lua51.dll`) | `_languages\lua\` bundle | Lua wrappers — `populate_toolchain.bat`; headers used at link time, runtime DLL embedded in exe |

Install via MSYS2: `pacman -S mingw-w64-x86_64-toolchain`

Default location assumed by `build_situation.bat`: `C:\msys64\mingw64\bin`. Override with `MINGW_PATH`.

### Bundled (no installation needed)

All in `ext/`:

- **GLFW 3** — windowing, input (auto-built by the Makefile on first use if `libglfw3.a` is missing; requires cmake on PATH)
- **cglm** — math library (header-only, no build)
- **glad** — OpenGL 4.6 loader (header-only)
- **miniaudio** — audio backend (single header)
- **stb_image / stb_truetype / stb_image_write** — image and font loading
- **cgltf** — glTF 2.0 model loading
- **VMA** (`vma_wrapper.cpp`) — Vulkan Memory Allocator

### Vulkan SDK

Required only for Vulkan targets. Download from [vulkan.lunarg.com](https://vulkan.lunarg.com/sdk/home).

Latest version as of this guide: **1.4.350.0** (May 2026).

The Makefile autodetects the SDK from `C:\VulkanSDK\*`. Set `VULKAN_SDK` explicitly to
override, e.g. `set VULKAN_SDK=C:\VulkanSDK\1.4.350.0`.

---

## Compilation Options

### Backend (Required — Choose One)

```c
#define SITUATION_USE_OPENGL    // OpenGL 4.6 backend
#define SITUATION_USE_VULKAN    // Vulkan 1.4 backend
```

### Optional Features

```c
#define SITUATION_ENABLE_THREADING        // Thread pool + dedicated threads
#define SITUATION_ENABLE_RENDER_THREAD    // Dedicated render thread (auto-set when THREADING is on)
#define SITUATION_ENABLE_SHADER_COMPILER  // Runtime GLSL→SPIR-V via shaderc (Vulkan)
#define SITUATION_ENABLE_DXGI             // DXGI for GPU VRAM queries (Windows)
#define SITUATION_ENABLE_TRACY            // Tracy CPU zones (opt-in; set by SIT_TRACY=1 build only)
```

**Tracy profiling (P10.2, v2.4.395+):** default builds do **not** define `SITUATION_ENABLE_TRACY`.
Enable when building the library:

```powershell
$env:SIT_TRACY = "1"
& ".\build\build_situation.bat" opengl
# or: & ".\build\build_situation.bat" opengl tracy
```

Requires `ext/tracy/public/` (clone upstream Tracy: `git clone --depth 1 --filter=blob:none --sparse https://github.com/wolfpld/tracy.git ext/tracy` then `git sparse-checkout set public`). The library links **`build/tracy_client.cpp`** (not a `sit/` source). OpenGL Tracy links use **g++** + `-ldbghelp -lsecur32`. Attach the [Tracy profiler](https://github.com/wolfpld/tracy) while the app runs. **`SIT_PROFILE_*` macros** live in **`sit/situation_profiling.h`**, pulled in by **`situation.h`** — not by `situation_api.h`. App code must `#include "situation.h"` and use the **same** define on app compile lines (`-DSITUATION_ENABLE_TRACY -Iext/tracy/public`) for user zones to appear in captures.

> `SITUATION_ENABLE_RENDER_THREAD` is implied by `SITUATION_ENABLE_THREADING`. You do not
> need to define both. The render thread is enabled/disabled at runtime via
> `SituationInitInfo.render_thread_count`.

### Link Mode

```c
#define SITUATION_BUILD_SHARED   // When compiling the DLL itself (set by build scripts only)
#define SITUATION_USE_SHARED     // When your app links against the DLL
```

Do not define `SITUATION_BUILD_SHARED` in application code — it is set by `build_situation.bat`
internally.

### SITUATION_IMPLEMENTATION — Deprecated

```c
// ⚠️ Do not use in new code.
// This still works for legacy single-file builds but recompiles ~200K lines
// on every build. Use the static lib or DLL instead.
#define SITUATION_IMPLEMENTATION
```

### Include Paths

```
-I.                      Project root (for situation.h)
-Isit                    sit/ folder (for direct sit/*.h includes in tests)
-Iext                    External dependencies
-Iext/cglm/include       cglm math
-Iext/glfw/include       GLFW
-Isit/k-term             K-Term (if building kterm_console or K-Term-enabled apps)
```

For Vulkan additionally:
```
-Iext/vulkan
-I%VULKAN_SDK%\Include
-Iext/shaderc/libshaderc/include   (if SITUATION_ENABLE_SHADER_COMPILER)
```

### Required Compiler Flag

```
-DCGLM_FORCE_DEPTH_ZERO_TO_ONE
```

This sets cglm's projection matrices to [0,1] depth range (required). It is set
automatically by `build_situation.bat` and all `build_*.bat` scripts.

---

## Platform-Specific Instructions

### Minimum Requirements

| Platform | Minimum | Notes |
|----------|---------|-------|
| Windows | Windows 10 (build 1607+) | Uses `SetThreadDescription`, `RtlGetVersion`, WASAPI, DXGI. No Win7/Win8. |
| GPU (OpenGL) | OpenGL 4.6 | Requires DSA, compute shaders, SPIR-V, MDI. No fallback. |
| GPU (Vulkan) | Vulkan 1.4 | Device selection rejects < 1.4. No fallback. |
| Linux | In progress | Not shipping. Planned: X11, PulseAudio/PipeWire. |
| macOS | In progress | Not shipping. Planned: Metal via MoltenVK. |

### Windows — MinGW-w64 (GCC)

**Static OpenGL** (self-contained exe):

```bat
gcc -o myapp.exe main.c ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include ^
    -DSITUATION_USE_OPENGL -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING ^
    -Lext/glfw/build/src ^
    -static-libgcc ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    build/dll/situation_opengl.a ^
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lbcrypt -lm
```

**Static Vulkan** (self-contained exe — two-step because of C++ runtime in shaderc/VMA):

```bat
REM Step 1: compile
gcc -c main.c -o main.o ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include -Iext/vulkan ^
    -I%VULKAN_SDK%\Include -Iext/shaderc/libshaderc/include ^
    -DSITUATION_USE_VULKAN -DCGLM_FORCE_DEPTH_ZERO_TO_ONE ^
    -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER

REM Step 2: link with g++ (pulls in C++ runtime for shaderc + VMA)
g++ main.o -o myapp.exe ^
    -Lext/glfw/build/src -Lext/shaderc/build/libshaderc -L%VULKAN_SDK%\Lib ^
    -static-libgcc -static-libstdc++ ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    build/dll/situation_vulkan.a ^
    -lglfw3 -lvulkan-1 -lshaderc_combined ^
    -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lbcrypt -lm
```

**DLL-linked OpenGL:**

```bat
gcc -o myapp.exe main.c ^
    -std=c11 -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include ^
    -DSITUATION_USE_OPENGL -DSITUATION_USE_SHARED ^
    -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING ^
    -Lbuild/dll -lsituation_opengl ^
    -static-libgcc -lm
```

### Windows — MSVC

MSVC cannot compile the library core (GCC-only internals). Build the DLL with
`build\build_situation.bat` first, then link from MSVC:

```bat
cl /Fe:myapp.exe main.c ^
    /I. /Isit /Iext /Iext\cglm\include /Iext\glfw\include ^
    /DSITUATION_USE_OPENGL /DSITUATION_USE_SHARED ^
    /DCGLM_FORCE_DEPTH_ZERO_TO_ONE /DSITUATION_ENABLE_THREADING ^
    /link build\dll\situation_opengl.lib
```

### Application Identity (Windows PE resources)

Situation EXEs link default **`sit/platform/windows/sit_app.rc`** (icon + `VS_VERSION_INFO`). Version triple is **never hardcoded in the RC** — build scripts pass `-DSIT_VERSION_*` from **`sit/situation_base_version.h`** via `build/sit_version.mk` (Make) or `scripts/read_situation_version.py --windres` (batch). Authors replace the entire RC — no merge with defaults.

| Mechanism | Default | Override |
|-----------|---------|----------|
| Repo examples / harness | `sit_app.rc` | `SIT_APP_RC` (`build_examples.bat`) or `APP_RC=` (`tests/harness/Makefile`) |
| Author template | — | `sit_app_template.rc` + windres `-DAPP_*` |
| Shell AppUserModelID | `Situation.Application` | `SituationInitInfo::app_user_model_id` or `SituationWin32SetAppUserModelId()` |

**Compile RC object (manual link):**

```powershell
$ver = & python scripts\read_situation_version.py --windres
windres sit/platform/windows/sit_app.rc -o app_res.o --include-dir sit/platform/windows $ver.Split()
# Link app_res.o with your exe (examples/harness Makefile does this automatically)
```

**Architecture (cross-platform):** [architecture.md — Application Identity](architecture.md#application-identity-architecture-v24399). **Windows how-to:** [guide/windows_app_identity.md](guide/windows_app_identity.md). **Plan:** [plan/SIT_IDENTITY_PLAN.md](plan/SIT_IDENTITY_PLAN.md).

### Linux — GCC _(not yet shipping)_

```bash
# OpenGL
gcc -o myapp main.c \
    -std=c11 -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include \
    -DSITUATION_USE_OPENGL -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING \
    build/dll/situation_opengl.a \
    -lglfw -lGL -lm -lpthread -ldl \
    -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama

# Vulkan
gcc -o myapp main.c \
    -std=c11 -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include \
    -DSITUATION_USE_VULKAN -DCGLM_FORCE_DEPTH_ZERO_TO_ONE \
    -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER \
    build/dll/situation_vulkan.a \
    -lglfw -lvulkan -lshaderc -lm -lpthread -ldl \
    -lX11 -lXrandr -lXi -lXxf86vm -lXcursor -lXinerama
```

### macOS — Clang _(not yet shipping)_

```bash
# OpenGL
clang -o myapp main.c \
    -std=c11 -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include \
    -DSITUATION_USE_OPENGL -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING \
    build/dll/situation_opengl.a -lglfw \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

# Vulkan (via MoltenVK)
clang -o myapp main.c \
    -std=c11 -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include \
    -I$VULKAN_SDK/include \
    -DSITUATION_USE_VULKAN -DCGLM_FORCE_DEPTH_ZERO_TO_ONE \
    -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER \
    -L$VULKAN_SDK/lib build/dll/situation_vulkan.a -lglfw -lvulkan \
    -framework Cocoa -framework IOKit -framework CoreVideo
```

---

## Backend Selection

### OpenGL 4.6

- Wider hardware and driver support
- Simpler debugging (RenderDoc, apitrace)
- Good choice for desktop apps targeting broad hardware

Requires OpenGL 4.6 — DSA, compute shaders, SPIR-V ingestion, and Multi-Draw Indirect are
used internally. Older drivers that expose 4.5 or below will be rejected at init.

```c
#define SITUATION_USE_OPENGL
#include "situation.h"
```

### Vulkan 1.4

- Explicit GPU control with lower CPU overhead
- Better threading model
- Compute shader support (with `SITUATION_ENABLE_SHADER_COMPILER`)
- Required for `demon_hunt` and other GPU-intensive showcases

Requires Vulkan 1.4 — device selection hard-rejects below 1.4.

```c
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER   // for runtime GLSL→SPIR-V
#include "situation.h"
```

---

## Common Issues

### "situation.h: No such file or directory"

Add the project root to your include path:

```bat
-I.
```

Or use `-I/path/to/situation` when building from elsewhere.

### "Multiple definition of SituationInit"

Do not define `SITUATION_IMPLEMENTATION`. Link against `build/dll/situation_opengl.a`
or the DLL — see [Quick Start](#quick-start).

### "undefined reference to glfwInit"

GLFW not linked. Add:

```bat
-Lext/glfw/build/src -lglfw3
```

If `libglfw3.a` is missing, build GLFW first (see [Prerequisites](#prerequisites)).

### "vulkan-1.dll not found" at runtime

Install the Vulkan SDK or add `%VULKAN_SDK%\Bin` to PATH.
The static build (`static-vulkan`) bundles everything — no DLL needed.

### "shaderc not found" / `libshaderc_combined.a` missing

```bat
build\build_shaderc.bat
build\build_situation.bat vulkan
```

Or use `static-vulkan`, which does not require shaderc. See [Building shaderc](#building-shaderc).

### "shaderc_shared.dll not found" at runtime

Use the static build (`static-vulkan`) which links `libshaderc_combined.a` internally.
DLL-linked builds do not depend on a shaderc DLL.

### "situation_opengl.dll not found" at runtime

Either:
- Use the static build (no DLL at runtime), or
- Copy `build\dll\situation_opengl.dll` next to your exe, or
- Add `build\dll\` to PATH

`build_examples.bat` and `build_tests.bat` in DLL mode copy the DLL automatically.

### Vulkan build fails with "Vulkan SDK not found"

```bat
set VULKAN_SDK=C:\VulkanSDK\1.4.350.0
build\build_situation.bat vulkan
```

Or install to the default path (`C:\VulkanSDK\`) and the Makefile will autodetect it.

### MinGW not found

```bat
set MINGW_PATH=C:\msys64\mingw64\bin
build\build_situation.bat static-opengl
```

Or install MSYS2 and run `pacman -S mingw-w64-x86_64-toolchain`.

---

## CMake Integration

The library does not ship a CMakeLists.txt, but it is straightforward to consume the
pre-built static lib from CMake:

```cmake
cmake_minimum_required(VERSION 3.16)
project(SituationApp C)
set(CMAKE_C_STANDARD 11)

add_executable(myapp main.c)

target_include_directories(myapp PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sit
    ${CMAKE_SOURCE_DIR}/ext
    ${CMAKE_SOURCE_DIR}/ext/glfw/include
    ${CMAKE_SOURCE_DIR}/ext/cglm/include
)

target_compile_definitions(myapp PRIVATE
    SITUATION_USE_OPENGL
    CGLM_FORCE_DEPTH_ZERO_TO_ONE
    SITUATION_ENABLE_THREADING
)

target_link_libraries(myapp PRIVATE
    ${CMAKE_SOURCE_DIR}/build/dll/situation_opengl.a
)

if(WIN32)
    target_link_libraries(myapp PRIVATE
        ${CMAKE_SOURCE_DIR}/ext/glfw/build/src/libglfw3.a
        opengl32 gdi32 winmm ws2_32 ole32 shell32 user32
        iphlpapi setupapi dxgi propsys shlwapi uuid xinput psapi bcrypt m
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(myapp PRIVATE
        glfw GL m pthread dl X11 Xrandr Xi Xxf86vm Xcursor Xinerama
    )
elseif(APPLE)
    target_link_libraries(myapp PRIVATE glfw
        "-framework OpenGL" "-framework Cocoa"
        "-framework IOKit" "-framework CoreVideo"
    )
endif()
```

---

## K-Term Console

`examples/console/console_host_app.c` is the canonical K-Term terminal + shell reference
app. It uses Situation as its rendering and platform backend (window, input, virtual display
compositor, sysinfo). It is not part of Situation's core API or version numbering.

- **K-Term version / changelog**: `sit/k-term/doc/updatelog.md`
- **K-Term API**: `sit/k-term/kterm_api.h`

### Build

```bat
build\build_examples.bat opengl  kterm_console
build\examples\kterm_console.exe
```

### Headless Screenshot (CI / testing)

```bat
set KTERM_CAPTURE_SCREENSHOT=shot.png
set KTERM_CAPTURE_EXIT=1
build\examples\kterm_console.exe
```

### Test Module

```bat
build\build_tests.bat static-opengl
build\tests\sit_test_opengl.exe --module kterm_console
```

---

## Additional Resources

| Document | Purpose |
|----------|---------|
| `doc/BUILD_SITUATION_GUIDE.md` | Full reference for building the library itself |
| `doc/situation_sdk.md` | Full SDK reference manual |
| `doc/situation_api.md` | Module map — links to all **`doc/guide/*.md`** sections |
| `doc/guide/` | 23 module guides (core, graphics, audio, logging, examples, …) |
| `doc/situation_api_index.md` | Auto-generated complete API index (581 symbols) |
| `doc/situation_command_reference.md` | All `SituationCmd*` rendering commands |
| `doc/architecture.md` | Internal design, threading model, GL/VK lifecycle |
| `doc/UPDATELOG.md` | Full version history |
| `doc/misc/SITUATION_QUICK_REFERENCE.md` | One-page cheat sheet |

Regenerate the API index and docs after API changes:

```bat
python scripts\generate_situation_api_docs.py
python scripts\verify_doc_links.py
```

---

## Version History (this document)

- **v2.4.399** (2026-06-29) — Application identity: default `sit_app.rc` PE resources, `SIT_APP_RC` / harness `APP_RC=`, AppUserModelID defaults; see [guide/windows_app_identity.md](guide/windows_app_identity.md) and [architecture.md](architecture.md#application-identity-architecture-v24399).
- **v2.4.363** (2026-06-26) — Lua wrapper documented: self-contained embedded exe (`build_lua_example.bat`), dynamic `lua51.dll` loading, draw shim for `SituationCmdDrawTextEx`, `gen_lua_embed.py` / `gen_lua_dll_embed.py`, dev mode via `run_lua_dev.bat`; binding regen includes `generate_lua_bindings.py`.
- **v2.4.271** (2026-06-14) — "Building the Library" section extracted to `doc/BUILD_SITUATION_GUIDE.md`; compilation guide now links to it with a concise summary. Vulkan SDK diagnostic improvements. GLFW auto-build guard improved.
- **v2.4.269** (2026-06-14) — Full refresh: Makefile build system, `windres` resource stamping, updated build script reference, `-lbcrypt` and `-lpropsys` added to link lines, static Vulkan two-step build documented, Vulkan SDK 1.4.350.0 noted, MSVC section updated, CMake section cleaned up, language wrapper static matrix updated
- **v2.4.265** (2026-06-14) — Wrapper example builders, static support matrix
- **v2.4.214** (2026-06-07) — Static build system, self-contained exes
- **v2.4.0** (2026-03-03) — Folder reorganization
