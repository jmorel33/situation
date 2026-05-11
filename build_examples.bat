@echo off
REM ========================================================================
REM build_examples.bat - Build Situation examples
REM
REM Usage:
REM   build_examples.bat opengl [example_name]    Build OpenGL example
REM   build_examples.bat vulkan [example_name]    Build Vulkan example
REM
REM Examples:
REM   build_examples.bat opengl basic_quad
REM   build_examples.bat opengl shader_lab_torus
REM   build_examples.bat opengl shader_lab_raytrace
REM   build_examples.bat opengl node_graph_piano_demo
REM   build_examples.bat vulkan diagnostic_render_vk
REM   build_examples.bat opengl quad_storm
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Configuration ---
set BUILD_DIR=build\examples
set GLFW_LIB=ext\glfw\build\src
set SHADERC_LIB=ext\shaderc\build\libshaderc

REM --- Parse Arguments ---
if "%~1"=="" goto :usage
if "%~2"=="" goto :usage
set BACKEND=%~1
set EXAMPLE=%~2

REM --- Resolve MinGW Path (same as build_situation.bat) ---
if defined MINGW_PATH (
    set "PATH=%MINGW_PATH%;%PATH%"
) else (
    if exist "C:\msys64\mingw64\bin\gcc.exe" (
        set "PATH=C:\msys64\mingw64\bin;%PATH%"
    ) else (
        echo [ERROR] MinGW not found. Set MINGW_PATH or install MSYS2.
        exit /b 1
    )
)

REM --- Verify GCC ---
gcc --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] gcc not found in PATH.
    exit /b 1
)
for /f "tokens=*" %%i in ('gcc -dumpversion') do set GCC_VER=%%i

REM --- Create output directory ---
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM --- Check source file exists ---
if not exist "examples\%EXAMPLE%.c" (
    echo [ERROR] Source file not found: examples\%EXAMPLE%.c
    exit /b 1
)

REM --- Dispatch ---
if /i "%BACKEND%"=="opengl" goto :build_opengl
if /i "%BACKEND%"=="vulkan" goto :build_vulkan
goto :usage

REM ========================================================================
REM BUILD: OpenGL Example
REM ========================================================================
:build_opengl
echo.
echo [BUILD] %EXAMPLE% (OpenGL) - GCC %GCC_VER%
echo.

REM node_graph_piano_demo: GUI subsystem so no extra console window behind the app.
set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"

gcc examples/%EXAMPLE%.c ext/glfw/deps/tinycthread.c ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -std=c11 -O2 ^
    -msse -msse2 -msse4.1 ^
    -I. -Iext -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -DSITUATION_ENABLE_THREADING ^
    -L%GLFW_LIB% ^
    -static-libgcc ^
    %EXTRA_LDFLAGS% ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lshlwapi -luuid -lxinput -lws2_32 -lm

if errorlevel 1 (
    echo [FAILED] Compilation failed!
    exit /b 1
)
echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe
exit /b 0

REM ========================================================================
REM BUILD: Vulkan Example
REM ========================================================================
:build_vulkan

REM --- Resolve Vulkan SDK ---
if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK (
    echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK.
    exit /b 1
)

echo.
echo [BUILD] %EXAMPLE% (Vulkan) - GCC %GCC_VER% - SDK: %VULKAN_SDK%
echo.

REM Step 1: Compile C source
echo   [1/3] Compiling %EXAMPLE%.c...
gcc -c examples/%EXAMPLE%.c ^
    -o %BUILD_DIR%/%EXAMPLE%.o ^
    -std=c11 -O2 ^
    -msse -msse2 -msse4.1 ^
    -I. -Iext -Iext/vulkan -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -I"%VULKAN_SDK%\Include" ^
    -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER

if errorlevel 1 (
    echo [FAILED] C compilation failed!
    exit /b 1
)

REM Step 2: Compile tinycthread
echo   [2/3] Compiling tinycthread...
gcc -c ext/glfw/deps/tinycthread.c ^
    -o %BUILD_DIR%/tinycthread_ex.o ^
    -std=c11 -Iext/glfw/deps

if errorlevel 1 (
    echo [FAILED] tinycthread compilation failed!
    exit /b 1
)

REM Step 3: Link with g++ (shaderc/VMA need C++ runtime)
echo   [3/3] Linking...
g++ %BUILD_DIR%/%EXAMPLE%.o %BUILD_DIR%/tinycthread_ex.o ext/vma_wrapper.o ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -L%GLFW_LIB% -L%SHADERC_LIB% -L"%VULKAN_SDK%\Lib" ^
    -static-libgcc -static-libstdc++ ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    -lglfw3 -lvulkan-1 -lshaderc_combined ^
    -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lshlwapi -luuid -lxinput -lws2_32 -lm

if errorlevel 1 (
    echo [FAILED] Linking failed!
    exit /b 1
)

REM Cleanup .o files
del %BUILD_DIR%\%EXAMPLE%.o 2>nul
del %BUILD_DIR%\tinycthread_ex.o 2>nul

echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe
exit /b 0

REM ========================================================================
:usage
echo.
echo Usage: build_examples.bat [backend] [example_name]
echo.
echo Backends:
echo   opengl   - Build with OpenGL backend
echo   vulkan   - Build with Vulkan backend
echo.
echo Example:
echo   build_examples.bat opengl quad_storm
echo   build_examples.bat vulkan diagnostic_render_vk
echo.
exit /b 1
