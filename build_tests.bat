@echo off
REM ========================================================================
REM build_tests.bat - Build the Situation Test Harness
REM
REM Usage:
REM   build_tests.bat              Build with OpenGL backend (default)
REM   build_tests.bat opengl       Build with OpenGL backend
REM   build_tests.bat vulkan       Build with Vulkan backend
REM
REM Prerequisites:
REM   Run build_situation.bat first to produce the DLL.
REM
REM Output: build\sit_test.exe (links against build\dll\situation_*.dll)
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Configuration ---
set BUILD_DIR=build
set DLL_DIR=build\dll
set BACKEND=%~1
if "%BACKEND%"=="" set BACKEND=opengl

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
if errorlevel 1 (
    echo [ERROR] gcc not found in PATH.
    exit /b 1
)
for /f "tokens=*" %%i in ('gcc -dumpversion') do set GCC_VER=%%i

REM --- Create output directory ---
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM --- Source files ---
set TEST_SOURCES=tests/harness/main.c tests/harness/sit_test_registry.c tests/harness/test_filesystem.c tests/harness/test_threading.c tests/harness/test_core.c tests/harness/test_window.c tests/harness/test_input.c tests/harness/test_timer.c tests/harness/test_graphics.c tests/harness/test_audio.c tests/harness/test_misc.c

REM --- Dispatch by backend ---
if /i "%BACKEND%"=="opengl" goto :build_opengl
if /i "%BACKEND%"=="vulkan" goto :build_vulkan
goto :build_opengl

REM ========================================================================
REM BUILD: OpenGL (links against situation_opengl.dll)
REM ========================================================================
:build_opengl

REM Check DLL exists
if not exist "%DLL_DIR%\situation_opengl.dll" (
    echo [ERROR] situation_opengl.dll not found in %DLL_DIR%
    echo         Run: build_situation.bat opengl
    exit /b 1
)

echo.
echo [BUILD] Situation Test Harness (OpenGL) - GCC %GCC_VER%
echo         Linking against: %DLL_DIR%\situation_opengl.dll
echo.

gcc %TEST_SOURCES% ^
    -o %BUILD_DIR%/sit_test.exe ^
    -std=c11 -O0 -g ^
    -I. -Iext -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -Itests/harness ^
    -DSITUATION_USE_OPENGL -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING ^
    -L%DLL_DIR% ^
    -lsituation_opengl ^
    -static-libgcc ^
    -lm

if errorlevel 1 (
    echo [FAILED] Compilation failed!
    exit /b 1
)

REM Copy DLL next to exe for runtime
copy /Y "%DLL_DIR%\situation_opengl.dll" "%BUILD_DIR%\situation_opengl.dll" >nul

echo [SUCCESS] %BUILD_DIR%\sit_test.exe (OpenGL)
goto :done

REM ========================================================================
REM BUILD: Vulkan (links against situation_vulkan.dll)
REM ========================================================================
:build_vulkan

REM Check DLL exists
if not exist "%DLL_DIR%\situation_vulkan.dll" (
    echo [ERROR] situation_vulkan.dll not found in %DLL_DIR%
    echo         Run: build_situation.bat vulkan
    exit /b 1
)

REM --- Resolve Vulkan SDK (for headers) ---
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
echo [BUILD] Situation Test Harness (Vulkan) - GCC %GCC_VER%
echo         Linking against: %DLL_DIR%\situation_vulkan.dll
echo.

gcc %TEST_SOURCES% ^
    -o %BUILD_DIR%/sit_test.exe ^
    -std=c11 -O0 -g ^
    -I. -Iext -Iext/vulkan -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -Itests/harness ^
    -I"%VULKAN_SDK%\Include" ^
    -DSITUATION_USE_VULKAN -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER ^
    -L%DLL_DIR% ^
    -lsituation_vulkan ^
    -static-libgcc ^
    -lm

if errorlevel 1 (
    echo [FAILED] Compilation failed!
    exit /b 1
)

REM Copy DLL next to exe for runtime
copy /Y "%DLL_DIR%\situation_vulkan.dll" "%BUILD_DIR%\situation_vulkan.dll" >nul

echo [SUCCESS] %BUILD_DIR%\sit_test.exe (Vulkan)
goto :done

REM ========================================================================
:done
echo.
echo Run with: %BUILD_DIR%\sit_test.exe [--module name] [--filter substr] [--verbose]
exit /b 0
