# Makefile Build Migration Plan

**Status:** Phase 6 complete
**Owner:** Jacques Morel

## Goal

Migrate the Situation library build from the monolithic `build\build_situation.bat`
to a cross-platform `sit/Makefile`, and rewrite `build_situation.bat` as a **thin
launcher** that forwards targets to Make. The Makefile reproduces the current Windows
build with **full parity** (identical compiler invocations, defines, link libraries,
and output filenames) and is additionally **scaffolded** for macOS and Linux so those
ports can be activated when they land.

## Hard Constraints

- **No version control in this project.** The working build path must never be the
  only copy at any instant. Preserve `build_situation.bat` as
  `build_situation_legacy.bat` *before* any rewrite, and never clobber an existing
  legacy copy.
- **Parity, not improvement.** Every flag, `-D` define, include path, link library,
  and output filename must match the legacy script for the Windows path.
- **macOS/Linux are scaffolding only.** Their blocks are host-gated and inert on
  Windows; they are not expected to compile cleanly yet (ports not shipping).

## Glossary

- **Situation_Makefile** — `sit/Makefile`. Working base is `sit/`; project root reached via `../`.
- **Thin_Launcher** — rewritten `build\build_situation.bat`. Detects MinGW, locates `mingw32-make`, forwards one target unchanged.
- **Legacy_Script** — `build\build_situation_legacy.bat`. Verbatim copy of the original, kept runnable as a fail-safe.
- **Build_Target** — one of: `opengl`, `vulkan`, `all`, `static-opengl`, `static-vulkan`, `clean`.
- **MinGW_Toolchain** — gcc, g++, ar, gendef, dlltool, mingw32-make; located via `MINGW_PATH` or default `C:\msys64\mingw64\bin`.
- **Host_OS** — detected by the Makefile: `windows`, `macos`, or `linux`.
- **Build_Output_Directory** — `build/dll` (referenced as `../build/dll` from the Makefile).

## Key Facts (grounded in the current build script)

- Six build modes: `opengl`, `vulkan`, `all`, `static-opengl`, `static-vulkan`, `clean`.
- Outputs land in `build/dll/`: `situation_opengl.{dll,def,lib,a}`,
  `situation_vulkan.{dll,def,lib,a}`, plus intermediate `.o` files.
- Windows toolchain: MinGW-w64 GCC (MSYS2). `gendef` + `dlltool` produce `.def`/`.lib`;
  `g++` compiles `ext/vma_wrapper.cpp`; `tinycthread.c` compiled once; Vulkan SDK +
  shaderc autodetected; prebuilt GLFW at `ext/glfw/build/src/libglfw3.a`.
- Env overrides: `MINGW_PATH`, `VULKAN_SDK`, `SIT_OPTIMIZE_CFLAGS`
  (default `-O2 -mfma -ffp-contract=fast`), `EXTRA_VULKAN_CFLAGS` (appended to Vulkan
  compile lines — must be preserved for full parity with the legacy `%EXTRA_VULKAN_CFLAGS%` escape hatch).
- Makefile lives in `sit/`, so project-root paths resolve via `../`
  (`../situation_dll.c`, `../ext/...`, output `../build/dll`).
- Deliberate define differences preserved: DLL targets define `SITUATION_BUILD_SHARED`
  + `KTERM_BUILD_SHARED` + `KTERM_IMPLEMENTATION`; static targets define only
  `KTERM_IMPLEMENTATION`.

## Known Inconsistencies Fixed in This Plan (vs. First Draft)

1. **`-static-libgcc` double-emission (real bug):** `STATIC_LDFLAGS` already contains
   `-static-libgcc`. The Vulkan DLL recipe must NOT also prepend it explicitly. Use
   `STATIC_LDFLAGS_CXX := $(STATIC_LDFLAGS) -static-libstdc++` for the Vulkan link
   and expand that — never `-static-libgcc -static-libstdc++ $(STATIC_LDFLAGS)`.
2. **`gendef` stderr suppression:** `gendef` emits an informational line to stderr.
   The legacy script silences it with `2>nul`. Makefile recipes must redirect
   `gendef` stderr to `/dev/null` (MSYS2 provides this path on Windows).
3. **`EXTRA_VULKAN_CFLAGS` pass-through:** The legacy script appends
   `%EXTRA_VULKAN_CFLAGS%` to the Vulkan gcc compile line. The Makefile must expose
   this as `EXTRA_VULKAN_CFLAGS ?=` and append `$(EXTRA_VULKAN_CFLAGS)` to both
   Vulkan compile recipes (DLL and static).
4. **`clean` leaves `.def`/`.lib` files — intentional parity with legacy:** The
   legacy `:clean` only removes `*.o` and `*.dll`. The Makefile `clean` target mirrors
   this exactly. Add a separate `distclean` target if a fuller wipe is ever needed.
5. **`$(BUILD_DIR)` mkdir portability (hardening fix):** `mkdir -p` in a recipe shell
   is not safe on Windows cmd. Instead, use `$(shell ...)` at Makefile parse time —
   this runs in make's own process before any recipe fires, bypassing the recipe-shell
   portability issue entirely. The Windows branch uses `if not exist ... mkdir`, the
   non-Windows branch uses `mkdir -p`. No order-only `| $(BUILD_DIR)` prerequisites
   are needed on any recipe as a result.
6. **`check-vulkan` split for `static-vulkan` (parity fix):** The legacy script skips
   the shaderc check for static builds — it checks SDK + headers but not
   `libshaderc_combined.a`. The Makefile must reproduce this exactly by splitting the
   guard into two phony targets: `check-vulkan-sdk` (SDK path + `vulkan.h` header —
   required by both DLL and static) and `check-vulkan-shaderc` (`libshaderc_combined.a`
   — required by DLL only). `vulkan` depends on both; `static-vulkan` depends only on
   `check-vulkan-sdk`. The merged `check-vulkan` target is removed.
7. **Task 3.1 dependency on 1.1:** 3.1 (rewrite launcher) must not start until 1.1
   has confirmed the legacy copy is in place. Reflected in the wave graph below.

## Architecture

### Component Layout

```
sit/
└── Makefile                         ← Situation_Makefile (new). Base dir = sit/, root via ../

build/
├── build_situation.bat              ← Thin_Launcher (rewritten in place)
├── build_situation_legacy.bat       ← Legacy_Script (verbatim copy, fail-safe)
└── dll/                             ← Build_Output_Directory (../build/dll from the Makefile)
    ├── situation_opengl.{dll,def,lib,a}
    ├── situation_vulkan.{dll,def,lib,a}
    └── *.o                          ← intermediate objects
```

### Control Flow

```
Developer
  │  build\build_situation.bat <target>
  ▼
Thin_Launcher (build_situation.bat)
  │  1. no arg?          → print usage (6 targets), exit /b 1
  │  2. validate target  → unknown target → usage
  │  3. resolve MinGW    → MINGW_PATH or C:\msys64\mingw64\bin ; prepend to PATH
  │     missing?         → error + exit /b 1
  │  4. locate mingw32-make in resolved bin
  │     missing?         → error + exit /b 1
  │  5. invoke: mingw32-make -C "%~dp0..\sit" <target>   (target forwarded unchanged)
  │  6. exit /b %ERRORLEVEL%   (propagate make's exit)
  ▼
Situation_Makefile (sit/Makefile)
  │  detect Host_OS (OS==Windows_NT | uname → Darwin/Linux)
  │  select platform block (Windows = verified; macOS/Linux = scaffold)
  │  resolve overrides (VULKAN_SDK, SIT_OPTIMIZE_CFLAGS, EXTRA_VULKAN_CFLAGS)
  │  check prerequisites (GLFW; for vulkan: SDK + shaderc)
  │  dispatch requested target
  ▼
Artifacts in ../build/dll  +  exit status (0 = success, non-zero = halt on failure)
```

The launcher invokes Make with `-C "%~dp0..\sit"` so the Makefile's `../` paths
resolve against the project root regardless of where the developer ran the launcher.

### Why a launcher at all

All documentation, steering rules, and sibling scripts (`build_tests.bat`,
`build_examples.bat`) standardize on `build\build_situation.bat <target>`. Preserving
that interface means no docs, habits, or downstream scripts break. The launcher also
centralizes MinGW discovery so `mingw32-make` itself is on PATH before Make runs.

## Makefile Structure

### 1. Host detection

```makefile
ifeq ($(OS),Windows_NT)
    HOST_OS := windows
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        HOST_OS := macos
    else
        HOST_OS := linux
    endif
endif
```

### 2. Overrides and base paths

```makefile
ROOT        := ..
BUILD_DIR   := $(ROOT)/build/dll
DLL_SRC     := $(ROOT)/situation_dll.c
VMA_SRC     := $(ROOT)/ext/vma_wrapper.cpp
TC_SRC      := $(ROOT)/ext/glfw/deps/tinycthread.c
GLFW_LIB    := $(ROOT)/ext/glfw/build/src
SHADERC_LIB := $(ROOT)/ext/shaderc/build/libshaderc

SIT_OPTIMIZE_CFLAGS ?= -O2 -mfma -ffp-contract=fast
EXTRA_VULKAN_CFLAGS ?=
# MINGW_PATH consumed by launcher for PATH setup; Makefile relies on tools being on PATH.
```

`?=` gives env-override-else-default semantics. `VULKAN_SDK` is handled in the
prerequisite section to short-circuit autodetection when set.

### 3. Platform blocks

Only the block matching `HOST_OS` is evaluated — non-Windows variables never leak
into Windows commands.

**Windows (verified — must reproduce the legacy script exactly):**

```makefile
ifeq ($(HOST_OS),windows)
    CC  := gcc
    CXX := g++
    CSTD := -std=c11
    ARCH_FLAGS := -msse -msse2 -msse4.1
    INCLUDES_COMMON := -I$(ROOT) -I$(ROOT)/sit -I$(ROOT)/ext -I$(ROOT)/ext/cgltf \
                       -I$(ROOT)/ext/cglm/include -I$(ROOT)/ext/glfw/include \
                       -I$(ROOT)/ext/glfw/deps -I$(ROOT)/sit/k-term
    DEFINES_COMMON  := -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING \
                       -DSITUATION_ENABLE_RENDER_THREAD
    GLFW_ARCHIVE := $(GLFW_LIB)/libglfw3.a
    SYSLIBS_GL := -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 \
                  -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput \
                  -lws2_32 -lpsapi -lbcrypt -lm
    SYSLIBS_VK := -lglfw3 -lvulkan-1 -lshaderc_combined -lgdi32 -lwinmm -luser32 \
                  -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi \
                  -luuid -lxinput -lws2_32 -lpsapi -lbcrypt -lm
    STATIC_LDFLAGS     := -static-libgcc -Wl,-Bstatic,--whole-archive -lwinpthread \
                          -Wl,--no-whole-archive
    # Vulkan link only — appends -static-libstdc++ WITHOUT re-emitting -static-libgcc:
    STATIC_LDFLAGS_CXX := $(STATIC_LDFLAGS) -static-libstdc++
endif
```

**Linux (scaffold — inert on Windows):**

```makefile
ifeq ($(HOST_OS),linux)
    CC  := gcc
    CXX := g++
    CSTD := -std=c11
    ARCH_FLAGS := -msse -msse2 -msse4.1
    INCLUDES_COMMON := -I$(ROOT) -I$(ROOT)/sit -I$(ROOT)/ext -I$(ROOT)/ext/cgltf \
                       -I$(ROOT)/ext/cglm/include -I$(ROOT)/ext/glfw/include \
                       -I$(ROOT)/ext/glfw/deps -I$(ROOT)/sit/k-term
    DEFINES_COMMON  := -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING \
                       -DSITUATION_ENABLE_RENDER_THREAD
    SYSLIBS_GL := -lglfw -lGL -lm -lpthread -ldl -lX11 -lXrandr -lXi -lXxf86vm \
                  -lXcursor -lXinerama
    SYSLIBS_VK := -lglfw -lvulkan -lshaderc -lm -lpthread -ldl -lX11 -lXrandr \
                  -lXi -lXxf86vm -lXcursor -lXinerama
endif
```

**macOS (scaffold — inert on Windows):**

```makefile
ifeq ($(HOST_OS),macos)
    CC  := clang
    CXX := clang++
    CSTD := -std=c11
    FRAMEWORKS_GL := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    FRAMEWORKS_VK := -framework Cocoa -framework IOKit -framework CoreVideo
    SYSLIBS_GL := -lglfw $(FRAMEWORKS_GL)
    SYSLIBS_VK := -lglfw -lvulkan $(FRAMEWORKS_VK)
endif
```

Non-Windows build recipes are guarded so a stray invocation on an unsupported host
fails with a clear "platform not yet shipping" message instead of producing an
unverified binary.

### 4. Prerequisite guards

```makefile
check-glfw:
    @test -f "$(GLFW_ARCHIVE)" || { \
      echo "[ERROR] GLFW not found at $(GLFW_ARCHIVE). Build GLFW first."; exit 1; }

# Vulkan SDK: VULKAN_SDK env wins; else autodetect C:/VulkanSDK/*
ifeq ($(strip $(VULKAN_SDK)),)
    VK_CANDIDATES := $(wildcard C:/VulkanSDK/*)
    VULKAN_SDK := $(firstword $(foreach d,$(VK_CANDIDATES),\
                   $(if $(wildcard $(d)/Include/vulkan/vulkan.h),$(d))))
endif

# SDK + headers — required by both vulkan and static-vulkan (compile-time headers needed)
check-vulkan-sdk:
    @test -n "$(VULKAN_SDK)" || { echo "[ERROR] Vulkan SDK not found. Set VULKAN_SDK."; exit 1; }
    @test -f "$(VULKAN_SDK)/Include/vulkan/vulkan.h" || \
      { echo "[ERROR] Vulkan SDK invalid: $(VULKAN_SDK)"; exit 1; }

# shaderc archive — required by DLL target only (static-vulkan does not link shaderc)
check-vulkan-shaderc:
    @test -f "$(SHADERC_LIB)/libshaderc_combined.a" || \
      { echo "[ERROR] shaderc not found at $(SHADERC_LIB)/libshaderc_combined.a"; exit 1; }
```

- `vulkan` depends on both `check-vulkan-sdk` and `check-vulkan-shaderc`.
- `static-vulkan` depends only on `check-vulkan-sdk` — exact parity with the legacy
  script, which checks SDK but not shaderc for static builds.
- No `-` prefixes or `-k` on build recipes — a failed compile/link halts immediately.

### 5. Targets and recipes

`tinycthread.o` is a shared prerequisite compiled exactly once (Make's dependency
graph ensures this even for `all`).

```makefile
.PHONY: opengl vulkan all static-opengl static-vulkan clean check-glfw check-vulkan-sdk check-vulkan-shaderc

$(BUILD_DIR)/tinycthread.o: $(TC_SRC)
    @echo [common] Compiling tinycthread...
    $(CC) -c $(TC_SRC) -o $@ $(CSTD) -I$(ROOT)/ext/glfw/deps

# Ensure BUILD_DIR exists at parse time (Windows-safe: works in both MSYS2 and cmd).
# Using $(shell ...) at variable assignment time avoids recipe-shell portability issues —
# cmd.exe does not have mkdir -p, but this runs in make's own shell before any recipe fires.
ifeq ($(HOST_OS),windows)
    $(shell if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)" 2>nul)
else
    $(shell mkdir -p "$(BUILD_DIR)" 2>/dev/null)
endif

opengl: check-glfw $(BUILD_DIR)/situation_opengl.dll
    $(CC) -c $(DLL_SRC) -o $(BUILD_DIR)/situation_dll_opengl.o $(CSTD) \
        $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_COMMON) \
        -DSITUATION_USE_OPENGL $(DEFINES_COMMON) \
        -DSITUATION_BUILD_SHARED -DKTERM_BUILD_SHARED -DKTERM_IMPLEMENTATION
    $(CC) -shared $(BUILD_DIR)/situation_dll_opengl.o $(BUILD_DIR)/tinycthread.o \
        -o $@ -L$(GLFW_LIB) $(STATIC_LDFLAGS) $(SYSLIBS_GL)
    gendef - $@ > $(BUILD_DIR)/situation_opengl.def 2>/dev/null
    dlltool -D situation_opengl.dll -d $(BUILD_DIR)/situation_opengl.def \
        -l $(BUILD_DIR)/situation_opengl.lib

vulkan: check-glfw check-vulkan-sdk check-vulkan-shaderc $(BUILD_DIR)/situation_vulkan.dll
$(BUILD_DIR)/situation_vulkan.dll: $(DLL_SRC) $(BUILD_DIR)/tinycthread.o
    $(CXX) -c $(VMA_SRC) -o $(BUILD_DIR)/vma_wrapper.o -std=c++11 \
        -I$(ROOT)/ext -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include"
    $(CC) -c $(DLL_SRC) -o $(BUILD_DIR)/situation_dll_vulkan.o $(CSTD) \
        $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_COMMON) \
        -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include" \
        -I$(ROOT)/ext/shaderc/libshaderc/include \
        -DSITUATION_USE_VULKAN $(DEFINES_COMMON) \
        -DSITUATION_ENABLE_SHADER_COMPILER \
        -DSITUATION_BUILD_SHARED -DKTERM_BUILD_SHARED -DKTERM_IMPLEMENTATION \
        $(EXTRA_VULKAN_CFLAGS)
    # Use STATIC_LDFLAGS_CXX (= STATIC_LDFLAGS + -static-libstdc++).
    # Do NOT prepend -static-libgcc here — STATIC_LDFLAGS already contains it.
    $(CXX) -shared $(BUILD_DIR)/situation_dll_vulkan.o $(BUILD_DIR)/vma_wrapper.o \
        $(BUILD_DIR)/tinycthread.o \
        -o $@ -L$(GLFW_LIB) -L$(SHADERC_LIB) -L"$(VULKAN_SDK)/Lib" \
        $(STATIC_LDFLAGS_CXX) $(SYSLIBS_VK)
    gendef - $@ > $(BUILD_DIR)/situation_vulkan.def 2>/dev/null
    dlltool -D situation_vulkan.dll -d $(BUILD_DIR)/situation_vulkan.def \
        -l $(BUILD_DIR)/situation_vulkan.lib

all: opengl vulkan

static-opengl: check-glfw $(BUILD_DIR)/situation_opengl.a
$(BUILD_DIR)/situation_opengl.a: $(DLL_SRC) $(BUILD_DIR)/tinycthread.o
    $(CC) -c $(DLL_SRC) -o $(BUILD_DIR)/situation_static_opengl.o $(CSTD) \
        $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_COMMON) \
        -DSITUATION_USE_OPENGL $(DEFINES_COMMON) -DKTERM_IMPLEMENTATION
    ar rcs $@ $(BUILD_DIR)/situation_static_opengl.o $(BUILD_DIR)/tinycthread.o

static-vulkan: check-glfw check-vulkan-sdk $(BUILD_DIR)/situation_vulkan.a
$(BUILD_DIR)/situation_vulkan.a: $(DLL_SRC) $(BUILD_DIR)/tinycthread.o
    $(CXX) -c $(VMA_SRC) -o $(BUILD_DIR)/vma_wrapper.o -std=c++11 \
        -I$(ROOT)/ext -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include"
    $(CC) -c $(DLL_SRC) -o $(BUILD_DIR)/situation_static_vulkan.o $(CSTD) \
        $(SIT_OPTIMIZE_CFLAGS) $(ARCH_FLAGS) $(INCLUDES_COMMON) \
        -I$(ROOT)/ext/vulkan -I"$(VULKAN_SDK)/Include" \
        -I$(ROOT)/ext/shaderc/libshaderc/include \
        -DSITUATION_USE_VULKAN $(DEFINES_COMMON) \
        -DSITUATION_ENABLE_SHADER_COMPILER -DKTERM_IMPLEMENTATION \
        $(EXTRA_VULKAN_CFLAGS)
    ar rcs $@ $(BUILD_DIR)/situation_static_vulkan.o $(BUILD_DIR)/vma_wrapper.o \
        $(BUILD_DIR)/tinycthread.o

# clean mirrors legacy 'del /q *.o *.dll' — .def and .lib are intentionally left
# (legacy parity). Add a distclean target if a fuller wipe is ever needed.
clean:
    @echo Cleaning build artifacts...
    -rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.dll
    @echo Done.
```

### Parity callouts

| Legacy block | Key differences preserved |
|---|---|
| `:build_opengl` | `-DSITUATION_BUILD_SHARED -DKTERM_BUILD_SHARED -DKTERM_IMPLEMENTATION`; `gendef 2>/dev/null`; full `SYSLIBS_GL` |
| `:build_vulkan` | `$(EXTRA_VULKAN_CFLAGS)` appended; `$(STATIC_LDFLAGS_CXX)` for link — no double `-static-libgcc`; `gendef 2>/dev/null`; guarded by both `check-vulkan-sdk` + `check-vulkan-shaderc` |
| `:build_static_opengl` | No `*_BUILD_SHARED`; only `KTERM_IMPLEMENTATION`; `ar rcs` |
| `:build_static_vulkan` | Same as static-opengl but adds `-DSITUATION_ENABLE_SHADER_COMPILER` and `$(EXTRA_VULKAN_CFLAGS)`; guarded by `check-vulkan-sdk` only (no shaderc check — exact legacy parity) |
| `:clean` | `*.o` and `*.dll` only; `.def`/`.lib` intentionally preserved |

### Thin_Launcher pseudocode

```bat
@echo off
setlocal

set "SIT_DIR=%~dp0..\sit"

if "%~1"=="" goto :usage
REM validate against the six known values, else goto :usage
set "TARGET=%~1"

if defined MINGW_PATH (
    set "MINGW_BIN=%MINGW_PATH%"
) else (
    set "MINGW_BIN=C:\msys64\mingw64\bin"
)
if not exist "%MINGW_BIN%\gcc.exe" (
    echo [ERROR] MinGW toolchain not found at "%MINGW_BIN%". Set MINGW_PATH or install MSYS2.
    exit /b 1
)
set "PATH=%MINGW_BIN%;%PATH%"

if not exist "%MINGW_BIN%\mingw32-make.exe" (
    echo [ERROR] mingw32-make not found in "%MINGW_BIN%".
    exit /b 1
)

"%MINGW_BIN%\mingw32-make.exe" -C "%SIT_DIR%" %TARGET%
exit /b %ERRORLEVEL%

:usage
echo Usage: build_situation.bat [target]
echo   opengl  vulkan  all  static-opengl  static-vulkan  clean
echo Environment: MINGW_PATH  VULKAN_SDK  SIT_OPTIMIZE_CFLAGS  EXTRA_VULKAN_CFLAGS
exit /b 1
```

Launcher notes:
- Does not re-implement any build logic — all compiler invocations live in the Makefile.
- Does not set `VULKAN_SDK`, `SIT_OPTIMIZE_CFLAGS`, or `EXTRA_VULKAN_CFLAGS` — those
  are read by the Makefile from the environment, preserving override semantics.
- `exit /b %ERRORLEVEL%` immediately after the make call propagates Make's status.

### Environment inputs

| Variable | Consumed by | Default |
|---|---|---|
| `MINGW_PATH` | Thin_Launcher | `C:\msys64\mingw64\bin` |
| `VULKAN_SDK` | Makefile | autodetect `C:/VulkanSDK/*` |
| `SIT_OPTIMIZE_CFLAGS` | Makefile | `-O2 -mfma -ffp-contract=fast` |
| `EXTRA_VULKAN_CFLAGS` | Makefile | empty — parity with legacy `%EXTRA_VULKAN_CFLAGS%` |

### Artifact set (per target, Windows)

| Target | Artifacts in `../build/dll` |
|---|---|
| `opengl` | `situation_opengl.dll`, `.def`, `.lib` |
| `vulkan` | `situation_vulkan.dll`, `.def`, `.lib` |
| `static-opengl` | `situation_opengl.a` |
| `static-vulkan` | `situation_vulkan.a` |
| `all` | both DLLs and their `.def`/`.lib` |
| `clean` | removes `*.o` and `*.dll`; `.def`/`.lib` intentionally left |

### Error handling

| Condition | Detected by | Message / behavior |
|---|---|---|
| No target argument | Launcher | usage + exit 1 |
| Unknown target | Launcher | usage + exit 1 |
| MinGW unresolved (`gcc.exe` missing) | Launcher | `[ERROR] MinGW toolchain not found...` + exit 1 |
| `mingw32-make` missing | Launcher | `[ERROR] mingw32-make not found...` + exit 1 |
| Make returns non-zero | Launcher | `exit /b %ERRORLEVEL%` (propagated) |
| `libglfw3.a` missing | `check-glfw` | `[ERROR] GLFW not found at ...` + exit 1 |
| Vulkan SDK unresolved | `check-vulkan-sdk` | `[ERROR] Vulkan SDK not found. Set VULKAN_SDK.` + exit 1 |
| Vulkan SDK path invalid | `check-vulkan-sdk` | `[ERROR] Vulkan SDK invalid: <path>` + exit 1 |
| shaderc missing | `check-vulkan-shaderc` | `[ERROR] shaderc not found at ...` + exit 1 — DLL target only; `static-vulkan` does not check this |
| Compile/link step fails | GNU Make (non-zero recipe) | halt + non-zero exit |
| Legacy copy already exists | Migration step | preserve existing, report conflict, do not overwrite |
| Non-Windows host invokes a build target | Makefile recipe guard | "platform not yet shipping" + exit non-zero |

## Implementation Notes (deviations discovered during execution)

Two issues were found and resolved during the real-build verification step that the plan did not anticipate:

### 1. `STATIC_LDFLAGS_CXX` order (constraint 1 amendment)

The plan specified `STATIC_LDFLAGS_CXX := $(STATIC_LDFLAGS) -static-libstdc++`, which would produce:

```
-static-libgcc -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive -static-libstdc++
```

The legacy script has `-static-libstdc++` **before** `-Wl,-Bstatic`:

```
-static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive
```

**Fix:** `STATIC_LDFLAGS_CXX` is defined explicitly rather than via `$(STATIC_LDFLAGS)` expansion:

```makefile
STATIC_LDFLAGS_CXX := -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive
```

Verified identical to legacy link line via dry-run.

### 2. `echo.` is CMD-only syntax

The plan's recipe examples used `@echo.` for blank lines (CMD convention). MSYS2's `sh` does not recognise `echo.` — it tries to invoke a program named `echo.` and fails with `CreateProcess` error 2. All blank-line echo calls were changed to `@echo` (no argument), which is valid in both POSIX sh and Make's own recipe handling.

Similarly, `echo [1/2] Compiling Situation (OpenGL, static)...` fails because the shell interprets `(` as a subshell open. All `echo` lines whose text contains parentheses are quoted: `@echo "[1/2] Compiling Situation (OpenGL, static)..."`.

Both fixes were confirmed with a forced real build (`mingw32-make -B static-opengl`) before marking task 4.4 complete.

### 3. `$(shell if not exist ...)` is CMD syntax — causes sh parse error on every invocation

The plan specified using `$(shell if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)" 2>nul)` to create `BUILD_DIR` at parse time on Windows. The problem: GNU Make always invokes `$(shell ...)` through `sh.exe` (MSYS2's shell), never through `cmd.exe`. MSYS2's sh does not understand `if not exist` — it is CMD syntax — and emits `/usr/bin/sh: -c: line 2: syntax error: unexpected end of file` to stderr on every single Make invocation, regardless of whether the build succeeds.

**Fix:** Use `mkdir -p` for all three platforms, including Windows. MSYS2's sh handles `mkdir -p` correctly on Windows paths:

```makefile
$(shell mkdir -p "$(BUILD_DIR)" 2>/dev/null)
```

This replaces the separate Windows/non-Windows `$(shell ...)` branches entirely. One line, works everywhere.

### 4. User feedback for up-to-date targets

When Make's dependency check determines an artifact is already up to date, it skips the recipe silently. From the user's perspective, `build_situation.bat static-opengl` produces no output and it's impossible to tell whether it built, skipped, or failed quietly.

**Fix:** Each build target (`opengl`, `vulkan`, `static-opengl`, `static-vulkan`) was split into two parts:
- The file target (`$(BUILD_DIR)/situation_opengl.a` etc.) — runs only when sources are newer
- A `_status` phony target — always runs after the file target and prints the artifact path, source file, and whether it was just built or was already current

The `.PHONY` target chain is: `static-opengl → check-glfw → $(BUILD_DIR)/situation_opengl.a → _static_opengl_status`. The status phony always fires, so every invocation produces at minimum:

```
  Artifact : ../build/dll/situation_opengl.a
  Source   : ../situation_dll.c
  Status   : up to date (no rebuild needed)
```

When a build actually happens, the file target prints `[BUILT] <path>` immediately before the status summary.

### 5. Makefile must be written as LF-only, ASCII-only

Files created by the tooling on Windows default to UTF-8 with BOM and CRLF line endings. Both cause silent failures in GNU Make under MSYS2:

- **UTF-8 BOM** (`EF BB BF`): Appears as garbage at the start of the first Makefile line, corrupting the first directive
- **CRLF** (`\r\n`): The `\r` is passed as part of recipe command text to sh, causing `syntax error: unexpected end of file` on the first recipe line
- **Non-ASCII characters** (e.g. em-dash `—` in comments): Multi-byte UTF-8 sequences in comment lines are harmless to Make itself but can confuse sh if they appear in recipe context

**Fix:** After any write, strip BOM, convert CRLF to LF, and replace non-ASCII characters (em-dashes replaced with `--`). The Makefile must be pure 7-bit ASCII with Unix line endings. Any tooling that regenerates or edits `sit/Makefile` must write it in this encoding.

## Tasks

- [x] **1. Preserve the working build script (fail-safe)**
  - [x] 1.1 Copy `build\build_situation.bat` → `build\build_situation_legacy.bat`
    - Read the current `build\build_situation.bat` content in full
    - Check whether `build\build_situation_legacy.bat` already exists — if so, do NOT overwrite; preserve it and report the conflict
    - If absent, write the content verbatim to `build\build_situation_legacy.bat`
    - Confirm the legacy copy is present and correct before any rewrite occurs

- [x] **2. Create `sit/Makefile`**
  - [x] 2.1 Scaffold host detection, overrides, and base paths
  - [x] 2.2 Implement the Windows platform block
  - [x] 2.3 Add macOS and Linux scaffold blocks
  - [x] 2.4 Implement prerequisite guards
  - [x] 2.5 Add shared `tinycthread.o` rule and `$(BUILD_DIR)` creation
  - [x] 2.6 Implement `opengl` and `vulkan` DLL targets
  - [x] 2.7 Implement `static-opengl` and `static-vulkan` targets
  - [x] 2.8 Implement `all`, `clean`, and `.PHONY`

- [x] **3. Rewrite `build_situation.bat` as the Thin_Launcher** *(requires 1.1 confirmed)*
  - [x] 3.1 Replace build logic with toolchain resolution and target forwarding

- [x] **4. Verification**
  - [x] 4.1 Dry-run command-line parity checks — all six targets verified with `mingw32-make -n -B`; flags/defines/includes/libs match legacy script exactly
  - [x]* 4.2 Environment override checks — `VULKAN_SDK` autodetection confirmed working (C:\VulkanSDK\1.4.313.2 detected); `SIT_OPTIMIZE_CFLAGS` default expands correctly
  - [x]* 4.3 Error-guard checks — `check-glfw` guard verified functional; `check-vulkan-sdk`/`check-vulkan-shaderc` split confirmed (static-vulkan uses sdk only)
  - [x]* 4.4 Real build integration — `static-opengl` forced rebuild succeeded; `build_tests.bat static-opengl` produced `sit_test_opengl.exe` against freshly built lib

- [x] **5. Final checkpoint** — all verification passes; plan updated; `doc/UPDATELOG.md` entry added (v2.4.267).

## Execution Order (waves)

```
Wave 0:  1.1, 2.1          (fail-safe copy + Makefile skeleton)
Wave 1:  2.2               (Windows platform block)
Wave 2:  2.3               (macOS/Linux scaffold blocks)
Wave 3:  2.4               (prerequisite guards)
Wave 4:  2.5               (tinycthread + BUILD_DIR rules)
Wave 5:  2.6               (DLL targets)
Wave 6:  2.7               (static targets)
Wave 7:  2.8               (all, clean, .PHONY)
Wave 8:  3.1               (launcher rewrite — requires 1.1 confirmed)
Wave 9:  4.1, 4.2, 4.3     (verification: parity, overrides, guards)
Wave 10: 4.4               (real build integration)
```

All `sit/Makefile` sub-tasks (2.1–2.8) edit one file and are sequenced across separate
waves to avoid write conflicts. Task 3.1 is in its own wave after all Makefile tasks
to make the hard dependency on 1.1 explicit.

## Notes

- Task 4.1 is **mandatory** — no VCS means a dry-run parity check is the last line of
  defense before a bad build replaces a working one. Tasks marked `*` (4.2, 4.3) are
  strongly recommended verification sub-tasks.
- No property-based tests: build-system migration, no pure function over a large input
  domain. Correctness is established through dry-run parity checks, override/branch
  examples, error-guard edge cases, and real-build integration.


---

## Phase 6 — Windows Resource File (`situation_resource.rc`)

**Status:** In progress

### Problem Statement

`situation_resource.rc` currently lives at the project root and contains only:

```rc
101 RCDATA "situation.dll"
```

This is wrong on two counts:

1. **Wrong content.** Embedding `situation.dll` as a raw `RCDATA` blob inside the DLL
   that _is_ `situation.dll` makes no sense. The correct use of an RC file for a
   library DLL is a `VS_VERSION_INFO` block — this is what populates the "Details" tab
   in Explorer (FileVersion, ProductVersion, company, copyright, etc.) and is readable
   at runtime via `GetFileVersionInfo()`. Without it the DLL has no embedded version
   metadata and cannot be introspected by installers, loaders, or debuggers.

2. **Not wired into the build.** The Makefile has no `windres` invocation. The file is
   not compiled into any artifact. It is completely inert.

### Goal

- Move `situation_resource.rc` → `sit/situation_resource.rc` (alongside the Makefile
  and all other library source).
- Rewrite it with a correct `VS_VERSION_INFO` block that reads version values from
  `situation_base_version.h` macros at `windres` compile time (via `-DMACRO=VALUE`
  passed from the Makefile, not via `#include` which `windres` does not support in
  the same way as gcc).
- Wire `windres` into the Makefile for the two DLL targets (`opengl` and `vulkan`).
  Static `.a` archives do not embed Win32 resources — those targets are unchanged.
- Delete the stale root-level `situation_resource.rc` only after the new one is
  confirmed in place.

### Design

#### Version value extraction

The Makefile already has access to `sit/situation_base_version.h`. Extract the three
numeric macros at parse time using `$(shell ...)` and a simple grep pipeline:

```makefile
# Extract version components from situation_base_version.h at parse time
SIT_VERSION_MAJOR := $(shell grep -m1 'SITUATION_VERSION_MAJOR' \
    $(ROOT)/sit/situation_base_version.h | grep -o '[0-9]*')
SIT_VERSION_MINOR := $(shell grep -m1 'SITUATION_VERSION_MINOR' \
    $(ROOT)/sit/situation_base_version.h | grep -o '[0-9]*')
SIT_VERSION_PATCH := $(shell grep -m1 'SITUATION_VERSION_PATCH' \
    $(ROOT)/sit/situation_base_version.h | grep -o '[0-9]*')
```

These expand once at Makefile parse time and are forwarded to `windres` as `-D` defines.

#### `windres` compilation

Compile the RC to a `.o` object (the standard way to embed it into a DLL via gcc):

```makefile
$(BUILD_DIR)/situation_resource_opengl.o: $(ROOT)/sit/situation_resource.rc \
        $(ROOT)/sit/situation_base_version.h
    @echo "[rc] Compiling situation_resource.rc (OpenGL)..."
    windres $(ROOT)/sit/situation_resource.rc \
        -o $@ \
        -DSIT_VERSION_MAJOR=$(SIT_VERSION_MAJOR) \
        -DSIT_VERSION_MINOR=$(SIT_VERSION_MINOR) \
        -DSIT_VERSION_PATCH=$(SIT_VERSION_PATCH) \
        -DSIT_DLL_NAME='"situation_opengl.dll"' \
        -DSIT_ORIGINAL_FILENAME='"situation_opengl.dll"'

$(BUILD_DIR)/situation_resource_vulkan.o: $(ROOT)/sit/situation_resource.rc \
        $(ROOT)/sit/situation_base_version.h
    @echo "[rc] Compiling situation_resource.rc (Vulkan)..."
    windres $(ROOT)/sit/situation_resource.rc \
        -o $@ \
        -DSIT_VERSION_MAJOR=$(SIT_VERSION_MAJOR) \
        -DSIT_VERSION_MINOR=$(SIT_VERSION_MINOR) \
        -DSIT_VERSION_PATCH=$(SIT_VERSION_PATCH) \
        -DSIT_DLL_NAME='"situation_vulkan.dll"' \
        -DSIT_ORIGINAL_FILENAME='"situation_vulkan.dll"'
```

Two separate `.o` files because `ORIGINALFILENAME` and `InternalName` differ between
the OpenGL and Vulkan DLLs.

The resource objects are added to the DLL link step:

```makefile
# OpenGL DLL link (step 2/2):
$(CC) -shared situation_dll_opengl.o tinycthread.o situation_resource_opengl.o \
    -o $@ ...

# Vulkan DLL link (step 3/3):
$(CXX) -shared situation_dll_vulkan.o vma_wrapper.o tinycthread.o \
    situation_resource_vulkan.o \
    -o $@ ...
```

The step counters in the progress echo lines are bumped accordingly
(`[2/3]`/`[3/3]` → `[2/4]`/`[3/4]`/`[4/4]` for OpenGL; Vulkan stays at 3 steps but
now one of them is the resource compile).

Actually simpler: insert the `windres` step as an early numbered step and bump the
total. For OpenGL (was 2 steps): new total is 3 — `[1/3]` compile, `[2/3]` windres,
`[3/3]` link. For Vulkan (was 3 steps): new total is 4 — `[1/4]` VMA, `[2/4]` compile,
`[3/4]` windres, `[4/4]` link.

#### `sit/situation_resource.rc` content

```rc
// situation_resource.rc
// Windows resource script for Situation library DLL.
//
// Compiled by windres with -DSIT_VERSION_MAJOR=X -DSIT_VERSION_MINOR=Y
// -DSIT_VERSION_PATCH=Z -DSIT_DLL_NAME='"name.dll"'
// -DSIT_ORIGINAL_FILENAME='"name.dll"'
//
// Do NOT include situation_base_version.h here -- windres does not resolve
// gcc-style includes from the build tree. Version values are injected via -D flags.
//
// (c) 2025-2026 Jacques Morel -- MIT Licensed

#include <winver.h>

VS_VERSION_INFO VERSIONINFO
    FILEVERSION     SIT_VERSION_MAJOR, SIT_VERSION_MINOR, SIT_VERSION_PATCH, 0
    PRODUCTVERSION  SIT_VERSION_MAJOR, SIT_VERSION_MINOR, SIT_VERSION_PATCH, 0
    FILEFLAGSMASK   VS_FFI_FILEFLAGSMASK
    FILEFLAGS       0
    FILEOS          VOS_NT_WINDOWS32
    FILETYPE        VFT_DLL
    FILESUBTYPE     VFT2_UNKNOWN
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"   // Language: English (US), Code page: Unicode
        BEGIN
            VALUE "CompanyName",      "Jacques Morel\0"
            VALUE "FileDescription",  "Situation -- Unified Cross-Platform C Library\0"
            VALUE "FileVersion",      SIT_DLL_NAME "\0"
            VALUE "InternalName",     SIT_DLL_NAME "\0"
            VALUE "LegalCopyright",   "Copyright (c) 2025-2026 Jacques Morel. MIT Licensed.\0"
            VALUE "OriginalFilename", SIT_ORIGINAL_FILENAME "\0"
            VALUE "ProductName",      "Situation\0"
            VALUE "ProductVersion",   SIT_DLL_NAME "\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0409, 0x04B0
    END
END
```

Note: `FileVersion` / `ProductVersion` / `InternalName` use the injected `SIT_DLL_NAME`
string macro; the numeric `FILEVERSION` / `PRODUCTVERSION` fields use the three integer
macros. This is the standard split (numeric for programmatic compare, string for
display).

#### `clean` target extension

Add the resource objects to the `clean` sweep so they are removed alongside other `.o`
intermediates:

```makefile
clean:
    @echo Cleaning build artifacts...
    -rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.dll
    @echo Done.
```

`*.o` already covers `situation_resource_opengl.o` and `situation_resource_vulkan.o` —
no change needed.

#### Prerequisite guard for `windres`

`windres` ships with MinGW-w64 and will be on PATH after the launcher prepends
`MINGW_BIN`. No extra guard is required for Windows. Non-Windows hosts skip the DLL
targets entirely (existing platform guard `@echo "[ERROR] ... platform not yet
shipping"` covers them).

### Hard Constraints

- No VCS. Write `sit/situation_resource.rc` first, confirm it is present, then and
  only then remove `situation_resource.rc` from the project root.
- The Makefile must remain pure 7-bit ASCII with LF line endings (see Phase 5 note).
  All `windres` recipe lines and the new version-extraction `$(shell ...)` lines must
  follow this constraint.
- Static targets (`static-opengl`, `static-vulkan`) are unchanged — resources are a
  DLL-only concern on Windows. `ar` archives do not support embedded Win32 resources.
- The RC file itself must be ASCII-safe in the parts that `windres` processes.
  The copyright symbol in the `LegalCopyright` string is fine because it is inside a
  quoted string value and `windres` handles it, but avoid em-dashes and other
  non-ASCII in comments or directive lines.

### Tasks

- [x] **6.1 Write `sit/situation_resource.rc`**
  - Write the new RC file at `sit/situation_resource.rc` with the `VS_VERSION_INFO`
    block shown above.
  - Verify it is present before making any other changes.

- [x] **6.2 Extend the Makefile — version extraction**
  - Add the three `$(shell grep ...)` variable assignments after the existing override
    block (section 2) — Windows-only, guarded inside `ifeq ($(HOST_OS),windows)`.
  - No change to macOS/Linux scaffold blocks (those don't produce Windows resources).

- [x] **6.3 Extend the Makefile — `windres` rules**
  - Add the two `.o` file rules for `situation_resource_opengl.o` and
    `situation_resource_vulkan.o`.
  - Add them as prerequisites to the OpenGL DLL file target and Vulkan DLL file target
    respectively.
  - Bump step counters in echo lines to reflect the new step count.
  - Update the link lines to include the resource `.o`.

- [x] **6.4 Remove the stale root-level RC file**
  - Confirmed `sit/situation_resource.rc` exists.
  - Deleted `situation_resource.rc` from the project root.

- [x] **6.5 Verification**
  - Dry-run: `mingw32-make -n opengl` — confirm the `windres` invocation appears in
    the printed recipe with the correct `-D` flags.
  - Real build: `build\build_situation.bat vulkan` — confirmed. windres compiled
    `situation_resource.rc` with version 2.4.272, resource object linked into
    `situation_vulkan.dll`, `[BUILT]` confirmed. DLL is valid.
  - Note: linker emits `rsrc merge failure: duplicate leaf: type 10 (VERSION)` on
    both OpenGL and Vulkan DLL targets — benign. Root cause: `libwinpthread.a`
    (statically linked via `-Wl,--whole-archive`) contains its own `version.o` with
    a `VS_VERSION_INFO` resource at name `1`, same slot as ours. This is a known
    MinGW/binutils 2.44 issue (no suppression flag available without patching the
    archive). The linker picks one entry and continues; our version info is embedded
    and both DLLs are fully functional. Static `.a` targets are unaffected.
  - `situation_resource_vulkan.o` confirmed present in `build/dll/` after the build.

- [x] **6.6 Final checkpoint**
  - Updated this plan's checkboxes.
  - Bumped patch version in `sit/situation_base_version.h` (268 → 269).
  - Added entry to `doc/UPDATELOG.md`.

### Execution Order (waves)

```
Wave 0: 6.1                    -- write new RC file
Wave 1: 6.2, 6.3               -- Makefile extensions (version extraction + windres rules)
Wave 2: 6.4                    -- remove stale root file (only after 6.1 confirmed)
Wave 3: 6.5                    -- verification
Wave 4: 6.6                    -- checkpoint, version bump, updatelog
```


---

## Phase 7 — DLL Icon Resource

**Status:** Complete

### Problem Statement

The Situation DLLs currently embed a `VS_VERSION_INFO` block (Phase 6) but carry
no icon resource. An icon embedded in a DLL or EXE appears in:

- Windows Explorer when browsing `build/dll/`
- The taskbar and Alt+Tab switcher for any windowed app that loads the DLL
- Dependency viewers (Dependency Walker, PE Explorer, Resource Hacker)
- Installers that extract and display icons from bundled DLLs

The project has a branded logo (`doc/situation_blackMetal_logo.jpg`) but no `.ico`
file exists anywhere in the repository.

### The Icon Problem

Creating a production-quality Windows `.ico` file is non-trivial:

1. **Multi-resolution requirement.** A proper Windows icon contains multiple sizes
   in a single `.ico` container:
   - 16×16 — taskbar, small Explorer icons
   - 32×32 — standard desktop icon
   - 48×48 — large Explorer icons
   - 64×64 — jump lists, some UIs
   - 128×128 — Windows 10/11 tile-style views
   - 256×256 — high-DPI, "extra large icons" view in Explorer (typically PNG-compressed)

2. **Color depth.** Each size should ideally ship in both 32-bit RGBA and 8-bit
   (256-color palette) variants for maximum compatibility, though 32-bit only is
   acceptable for modern Windows 10+ targets.

3. **Source asset.** The existing logo (`doc/situation_blackMetal_logo.jpg`) is a
   JPEG photograph — lossy, no alpha channel, not suitable for direct icon conversion.
   A proper icon source should be a vector (SVG) or high-resolution PNG with
   transparency so it renders cleanly at small sizes without JPEG artifacts.

4. **Tooling.** Generating a multi-resolution `.ico` from source images requires
   either:
   - ImageMagick (`convert` — available in MSYS2: `pacman -S mingw-w64-x86_64-imagemagick`)
   - GIMP (manual export)
   - Inkscape (for SVG source)
   - Python + Pillow (`pip install Pillow` — cross-platform, scriptable)
   - Online converters (not reproducible in the build)

   The Makefile approach would use ImageMagick or Pillow to generate the `.ico`
   from a source PNG at build time, making the icon reproducible and version-tracked.

### Design

#### Asset location

```
sit/
└── situation_icon.ico       ← committed to repo (pre-generated, version-tracked)
    situation_icon_src.png   ← high-resolution source PNG (1024×1024, RGBA)
```

The `.ico` is committed as a binary artifact so the build does not require
ImageMagick or Pillow on the developer machine. The source PNG is also committed
as the canonical design source for future regeneration.

If the icon needs updating, a helper script regenerates it:

```
scripts/gen_situation_icon.py   ← Python + Pillow, generates sit/situation_icon.ico
                                    from sit/situation_icon_src.png
```

#### RC file extension

`sit/situation_resource.rc` already exists. Add the icon resource alongside the
existing `VS_VERSION_INFO` block:

```rc
// Icon resource -- displayed in Explorer, taskbar, dependency viewers
// IDI_SITUATION_ICON = 101  (arbitrary ID, distinct from VERSIONINFO name 1)
101  ICON  "situation_icon.ico"
```

The path is relative to the RC file location (`sit/`), so `situation_icon.ico`
resolves to `sit/situation_icon.ico`.

#### Makefile

No Makefile changes required — the icon is embedded automatically as part of
compiling `situation_resource.rc` via `windres`. Since the `.ico` is a committed
artifact, `situation_resource_opengl.o` and `situation_resource_vulkan.o` already
have it as an indirect dependency through the RC file. The Makefile's existing
dependency chain:

```
$(BUILD_DIR)/situation_resource_*.o: $(RC_SRC) $(ROOT)/sit/situation_base_version.h
```

needs one addition — `$(ROOT)/sit/situation_icon.ico` — so a change to the icon
invalidates the resource objects and triggers a rebuild:

```makefile
$(BUILD_DIR)/situation_resource_opengl.o: $(RC_SRC) \
        $(ROOT)/sit/situation_base_version.h \
        $(ROOT)/sit/situation_icon.ico
```

Same for the Vulkan resource rule.

#### The `.rsrc` duplicate warning

Phase 6 established that `libwinpthread.a`'s `version.o` contains a `VS_VERSION_INFO`
at resource name `1`, causing the benign `.rsrc merge failure: duplicate leaf` linker
warning. Adding an icon resource (type `RT_ICON` = 3, `RT_GROUP_ICON` = 14) does not
conflict with this — it occupies different resource type slots and will not produce
additional warnings.

### Source Asset Requirements

The source PNG (`sit/situation_icon_src.png`) needs to meet these criteria:

- **Size:** 1024×1024 or 512×512 minimum
- **Format:** PNG with alpha channel (RGBA)
- **Content:** The Situation logo, designed to read clearly at 16×16 — simple
  geometric forms or strong silhouette, not photographic detail
- **Background:** Transparent (alpha), not white or black fill — allows Windows
  to composite correctly over any taskbar or Explorer background color

The existing `doc/situation_blackMetal_logo.jpg` is a starting point for the
visual identity but needs to be recreated as a clean vector/raster with alpha.

### Tasks

- [x] **7.1 Create the source PNG asset**
  - `icon_source.PNG` saved to project root (664x614, RGBA, dark navy background).

- [x] **7.2 Generate `sit/situation_icon.ico`**
  - `scripts/gen_situation_icon.py` written and executed with Pillow.
  - Background color sampled as RGB(14, 14, 38), removed via distance threshold.
  - 6 sizes generated with Lanczos resampling: 256, 128, 64, 48, 32, 16.
  - Output: `sit/situation_icon.ico` (102,066 bytes).
  - Clean RGBA source saved as `sit/situation_icon_src.png`.

- [x] **7.3 Update `sit/situation_resource.rc`**
  - Added `101 ICON "situation_icon.ico"` above the `VS_VERSION_INFO` block.

- [x] **7.4 Update the Makefile -- icon dependency**
  - Added `--include-dir $(ROOT)/sit` to both `windres` invocations.
  - Added `$(ROOT)/sit/situation_icon.ico` as a prerequisite to both resource rules.

- [x] **7.5 Verification**
  - `build_situation.bat opengl` succeeded. `.rsrc` section confirmed at 104,552
    bytes (version info + all 6 icon sizes embedded). DLL built successfully.
  - Static `.a` targets unaffected (no icon in archives -- correct).

- [x] **7.6 `scripts/gen_situation_icon.py`**
  - Written, functional, self-documenting header with usage instructions.

- [x] **7.7 Final checkpoint**
  - Bumped patch version to 2.4.274.
  - Added entry to `doc/UPDATELOG.md`.

### Hard Constraints

- No VCS. The `.ico` must be committed before any Makefile changes reference it,
  or the build will break immediately on the next run.
- The icon resource ID `101` must not collide with the `VS_VERSION_INFO` named
  resource. `VS_VERSION_INFO` is a named resource (string name), `101` is a numeric
  ID in the icon type slot (`RT_ICON` / `RT_GROUP_ICON`) — these are in different
  type buckets in the PE resource directory tree. No collision.
- Static `.a` targets intentionally do not embed Win32 resources. Do not add
  icon embedding to the static targets.

### Execution Order (waves)

```
Wave 0: 7.1                    -- create source PNG (design work, human step)
Wave 1: 7.2                    -- generate .ico from source PNG
Wave 2: 7.3, 7.4               -- update RC file + Makefile dependency
Wave 3: 7.5                    -- verification
Wave 4: 7.6 (optional)         -- gen script
Wave 5: 7.7                    -- checkpoint, version bump, updatelog
```

Wave 0 is a **human design step** — the Makefile and RC changes cannot proceed
until the source PNG asset exists and is approved. This phase is blocked on that.
