@echo off
REM ========================================================================
REM build_tests.bat - Build the Situation Test Harness
REM
REM Usage:
REM   build_tests.bat                        OpenGL static (default, self-contained)
REM   build_tests.bat static-opengl          OpenGL static (self-contained exe, no DLL)
REM   build_tests.bat static-vulkan          Vulkan static (self-contained exe, no DLL)
REM   build_tests.bat opengl                 OpenGL DLL-linked (fast build)
REM   build_tests.bat vulkan                 Vulkan DLL-linked (fast build)
REM
REM Static modes produce a single exe that runs without any DLL next to it.
REM DLL modes are faster to build but require build\dll\ on PATH or a DLL copy.
REM
REM Prerequisites:
REM   Static: build\build_situation.bat static-opengl (or static-vulkan)
REM   DLL:    build\build_situation.bat opengl (or vulkan)
REM
REM Output: build\tests\sit_test_opengl.exe or sit_test_vulkan.exe
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Change to project root (one level up from build/) ---
cd /d "%~dp0.."

REM --- Configuration ---
set BUILD_DIR=build\tests
set DLL_DIR=build\dll
set GLFW_LIB=ext\glfw\build\src
set SHADERC_LIB=ext\shaderc\build\libshaderc
set BACKEND=%~1
if "%BACKEND%"=="" goto :usage

REM --- Resolve MinGW Path ---
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
if errorlevel 1 ( echo [ERROR] gcc not found in PATH. & exit /b 1 )
for /f "tokens=*" %%i in ('gcc -dumpversion') do set GCC_VER=%%i

REM --- Create output directory ---
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM --- Precompile harness SPIR-V (optional if glslc missing) ---
call "%~dp0compile_harness_shaders.bat"

REM --- Source files ---
set TEST_SOURCES=tests/harness/main.c tests/harness/sit_test_registry.c tests/harness/test_filesystem.c tests/harness/test_threading.c tests/harness/test_core.c tests/harness/test_window.c tests/harness/test_input.c tests/harness/test_timer.c tests/harness/test_proj.c tests/harness/test_graphics.c tests/harness/test_text_rendering.c tests/harness/test_virtual_display.c tests/harness/test_compute.c tests/harness/test_transfer.c tests/harness/test_graphics_spirv.c tests/harness/test_model_loader.c tests/harness/test_stl_loader.c tests/harness/test_obj_loader.c tests/harness/test_projection_3d.c tests/harness/test_audio.c tests/harness/test_tone_synth.c tests/harness/test_audio_effects_heard.c tests/harness/audio_freq_detect.c tests/harness/sit_test_audio_levels.c tests/harness/sit_test_stereo_scope.c tests/harness/test_misc.c tests/harness/test_system_info.c tests/harness/test_kterm_console.c tests/harness/test_advanced.c

REM --- Dispatch ---
if /i "%BACKEND%"=="static-opengl" goto :build_static_opengl
if /i "%BACKEND%"=="static-vulkan" goto :build_static_vulkan
if /i "%BACKEND%"=="opengl"        goto :build_opengl
if /i "%BACKEND%"=="vulkan"        goto :build_vulkan
echo [ERROR] Unknown backend: %BACKEND%
goto :usage

REM ========================================================================
REM BUILD: Static OpenGL (self-contained exe, no DLL)
REM ========================================================================
:build_static_opengl

set STATIC_LIB=%DLL_DIR%\situation_opengl.a
if not exist "%STATIC_LIB%" (
    echo [ERROR] situation_opengl.a not found in %DLL_DIR%
    echo         Run: build\build_situation.bat static-opengl
    exit /b 1
)

set "TEST_EXE=sit_test_opengl.exe"

echo.
echo [BUILD] Situation Test Harness (OpenGL, static) - GCC %GCC_VER%
echo         Linking against: %STATIC_LIB%
echo.

gcc %TEST_SOURCES% tests/harness/sit_harness_spirv_gl_embed.c ^
    -o %BUILD_DIR%\%TEST_EXE% ^
    -std=c11 -O0 -g ^
    -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include -Isit/k-term ^
    -Itests/harness ^
    -DSITUATION_USE_OPENGL -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_RENDER_THREAD ^
    -L%GLFW_LIB% ^
    -static-libgcc ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    "%STATIC_LIB%" ^
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

echo [SUCCESS] %BUILD_DIR%\%TEST_EXE% (OpenGL, self-contained)
goto :done

REM ========================================================================
REM BUILD: Static Vulkan (self-contained exe, no DLL)
REM ========================================================================
:build_static_vulkan

set STATIC_LIB=%DLL_DIR%\situation_vulkan.a
if not exist "%STATIC_LIB%" (
    echo [ERROR] situation_vulkan.a not found in %DLL_DIR%
    echo         Run: build\build_situation.bat static-vulkan
    exit /b 1
)

REM --- Resolve Vulkan SDK ---
if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK ( echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK. & exit /b 1 )

set "TEST_EXE=sit_test_vulkan.exe"

echo.
echo [BUILD] Situation Test Harness (Vulkan, static) - GCC %GCC_VER%
echo         Linking against: %STATIC_LIB%
echo.

REM Step 1: compile all C test sources — each file gets its own .o in BUILD_DIR
for %%f in (%TEST_SOURCES% tests/harness/sit_harness_spirv_vk_embed.c) do (
    gcc -c %%f ^
        -o %BUILD_DIR%\%%~nf.o ^
        -std=c11 -O0 -g ^
        -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include -Isit/k-term ^
        -Itests/harness ^
        -I"%VULKAN_SDK%\Include" ^
        -DSITUATION_USE_VULKAN -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_RENDER_THREAD -DSITUATION_ENABLE_SHADER_COMPILER
    if errorlevel 1 ( echo [FAILED] Compiling %%f & del %BUILD_DIR%\*.o 2>nul & exit /b 1 )
)

REM Step 2: link with g++ for shaderc/VMA C++ runtime in the static archive
g++ %BUILD_DIR%\*.o ^
    -o %BUILD_DIR%\%TEST_EXE% ^
    -L%GLFW_LIB% -L%SHADERC_LIB% -L"%VULKAN_SDK%\Lib" ^
    -static-libgcc -static-libstdc++ ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    "%STATIC_LIB%" ^
    -lglfw3 -lvulkan-1 -lshaderc_combined ^
    -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm

if errorlevel 1 ( echo [FAILED] Link failed! & del %BUILD_DIR%\*.o 2>nul & exit /b 1 )

del %BUILD_DIR%\*.o 2>nul

echo [SUCCESS] %BUILD_DIR%\%TEST_EXE% (Vulkan, self-contained)
goto :done

REM ========================================================================
REM BUILD: OpenGL DLL-linked (fast, needs DLL on PATH or run via run_tests.bat)
REM ========================================================================
:build_opengl

if not exist "%DLL_DIR%\situation_opengl.dll" (
    echo [ERROR] situation_opengl.dll not found in %DLL_DIR%
    echo         Run: build\build_situation.bat opengl
    exit /b 1
)

set "TEST_EXE=sit_test_opengl.exe"

echo.
echo [BUILD] Situation Test Harness (OpenGL, DLL) - GCC %GCC_VER%
echo         Linking against: %DLL_DIR%\situation_opengl.dll
echo         Note: run via build\run_tests.bat or add build\dll to PATH
echo.

gcc %TEST_SOURCES% tests/harness/sit_harness_spirv_gl_embed.c ^
    -o %BUILD_DIR%\%TEST_EXE% ^
    -std=c11 -O0 -g ^
    -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include -Isit/k-term ^
    -Itests/harness ^
    -DSITUATION_USE_OPENGL -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_RENDER_THREAD ^
    -L%DLL_DIR% -lsituation_opengl ^
    -static-libgcc -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

echo [SUCCESS] %BUILD_DIR%\%TEST_EXE% (OpenGL, DLL-linked)
goto :done

REM ========================================================================
REM BUILD: Vulkan DLL-linked (fast, needs DLL on PATH or run via run_tests.bat)
REM ========================================================================
:build_vulkan

if not exist "%DLL_DIR%\situation_vulkan.dll" (
    echo [ERROR] situation_vulkan.dll not found in %DLL_DIR%
    echo         Run: build\build_situation.bat vulkan
    exit /b 1
)

if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK ( echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK. & exit /b 1 )

set "TEST_EXE=sit_test_vulkan.exe"

echo.
echo [BUILD] Situation Test Harness (Vulkan, DLL) - GCC %GCC_VER%
echo         Linking against: %DLL_DIR%\situation_vulkan.dll
echo         Note: run via build\run_tests.bat or add build\dll to PATH
echo.

gcc %TEST_SOURCES% tests/harness/sit_harness_spirv_vk_embed.c ^
    -o %BUILD_DIR%\%TEST_EXE% ^
    -std=c11 -O0 -g ^
    -I. -Isit -Iext -Iext/cglm/include -Iext/glfw/include -Isit/k-term ^
    -Itests/harness ^
    -I"%VULKAN_SDK%\Include" ^
    -DSITUATION_USE_VULKAN -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_RENDER_THREAD -DSITUATION_ENABLE_SHADER_COMPILER ^
    -L%DLL_DIR% -lsituation_vulkan ^
    -static-libgcc -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

echo [SUCCESS] %BUILD_DIR%\%TEST_EXE% (Vulkan, DLL-linked — use build\run_tests.bat to run)
goto :done

REM ========================================================================
:done
echo.
echo Run: build\tests\%TEST_EXE% [--module name] [--filter substr] [--verbose]
exit /b 0

:usage
echo.
echo Usage: build_tests.bat [backend]
echo.
echo   static-opengl   Self-contained OpenGL exe (no DLL needed)
echo   static-vulkan   Self-contained Vulkan exe (no DLL needed)
echo   opengl          DLL-linked OpenGL (faster build, use build\run_tests.bat to run)
echo   vulkan          DLL-linked Vulkan (faster build, use build\run_tests.bat to run)
echo.
echo Output:
echo   build\tests\sit_test_opengl.exe
echo   build\tests\sit_test_vulkan.exe
echo.
echo Prerequisites:
echo   static-opengl   ^> build\build_situation.bat static-opengl
echo   static-vulkan   ^> build\build_situation.bat static-vulkan
echo   opengl          ^> build\build_situation.bat opengl
echo   vulkan          ^> build\build_situation.bat vulkan
echo.
exit /b 1
