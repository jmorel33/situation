@echo off
REM ========================================================================
REM build_situation.bat - Official Situation DLL Build Script
REM
REM Usage:
REM   build_situation.bat vulkan     Build Vulkan backend DLL
REM   build_situation.bat opengl     Build OpenGL backend DLL
REM   build_situation.bat all        Build both
REM   build_situation.bat clean      Remove build artifacts
REM
REM Environment Variables (optional overrides):
REM   MINGW_PATH    - Path to MinGW bin (default: C:\msys64\mingw64\bin)
REM   VULKAN_SDK    - Path to Vulkan SDK (auto-detected if not set)
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Configuration ---
set BUILD_DIR=build\dll
set GLFW_LIB=ext\glfw\build\src
set SHADERC_LIB=ext\shaderc\build\libshaderc
set VMA_SRC=ext\vma_wrapper.cpp
set TINYCTHREAD_SRC=ext\glfw\deps\tinycthread.c
set DLL_SRC=situation_dll.c

REM --- Parse Arguments ---
if "%~1"=="" goto :usage
if /i "%~1"=="vulkan" goto :setup
if /i "%~1"=="opengl" goto :setup
if /i "%~1"=="all" goto :setup
if /i "%~1"=="clean" goto :clean
goto :usage

:setup
set TARGET=%~1

REM --- Resolve MinGW Path ---
if defined MINGW_PATH (
    set "PATH=%MINGW_PATH%;%PATH%"
) else (
    REM Default: MSYS2 MinGW64
    if exist "C:\msys64\mingw64\bin\gcc.exe" (
        set "PATH=C:\msys64\mingw64\bin;%PATH%"
    ) else (
        echo [ERROR] MinGW not found at default path C:\msys64\mingw64\bin
        echo         Set MINGW_PATH environment variable to your MinGW bin directory.
        exit /b 1
    )
)

REM --- Verify GCC is available ---
gcc --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] gcc not found in PATH. Install MinGW-w64 or set MINGW_PATH.
    exit /b 1
)

REM --- Show GCC version ---
for /f "tokens=*" %%i in ('gcc -dumpversion') do set GCC_VER=%%i

REM --- Create build directory ---
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM --- Check GLFW ---
if not exist "%GLFW_LIB%\libglfw3.a" (
    echo [ERROR] GLFW static library not found at %GLFW_LIB%\libglfw3.a
    echo.
    echo         GLFW needs to be built first. Run:
    echo           cd ext\glfw
    echo           mkdir build ^&^& cd build
    echo           cmake .. -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc
    echo           mingw32-make
    echo.
    exit /b 1
)

REM --- Dispatch ---
if /i "%TARGET%"=="all" (
    call :build_opengl
    if errorlevel 1 exit /b 1
    call :build_vulkan
    if errorlevel 1 exit /b 1
    goto :done_all
)
if /i "%TARGET%"=="opengl" (
    call :build_opengl
    if errorlevel 1 exit /b 1
    goto :done
)
if /i "%TARGET%"=="vulkan" (
    call :build_vulkan
    if errorlevel 1 exit /b 1
    goto :done
)

REM ========================================================================
REM BUILD: OpenGL
REM ========================================================================
:build_opengl
echo.
echo ========================================
echo  Situation DLL Build - OpenGL
echo  Compiler: GCC %GCC_VER% (C11)
echo  Backend:  OpenGL
echo ========================================
echo.

REM Step 1: Compile tinycthread
echo [1/3] Compiling tinycthread...
gcc -c "%TINYCTHREAD_SRC%" ^
    -o "%BUILD_DIR%\tinycthread.o" ^
    -std=c11 ^
    -Iext\glfw\deps

if errorlevel 1 (
    echo [FAILED] tinycthread compilation failed!
    exit /b 1
)
echo       OK

REM Step 2: Compile Situation DLL (OpenGL)
echo [2/3] Compiling Situation DLL (C11 - OpenGL)...
gcc -c "%DLL_SRC%" ^
    -o "%BUILD_DIR%\situation_dll_opengl.o" ^
    -std=c11 ^
    -msse -msse2 -msse4.1 ^
    -I. ^
    -Iext ^
    -Iext\cglm\include ^
    -Iext\glfw\include ^
    -Iext\glfw\deps ^
    -Isit\k-term ^
    -DSITUATION_USE_OPENGL ^
    -DSITUATION_ENABLE_THREADING ^
    -DSITUATION_BUILD_SHARED ^
    -DKTERM_BUILD_SHARED ^
    -DKTERM_IMPLEMENTATION

if errorlevel 1 (
    echo [FAILED] Situation DLL compilation failed!
    exit /b 1
)
echo       OK

REM Step 3: Link OpenGL DLL
echo [3/3] Linking situation_opengl.dll...
gcc -shared ^
    "%BUILD_DIR%\situation_dll_opengl.o" ^
    "%BUILD_DIR%\tinycthread.o" ^
    -o "%BUILD_DIR%\situation_opengl.dll" ^
    -L"%GLFW_LIB%" ^
    -static-libgcc ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    -lglfw3 ^
    -lopengl32 ^
    -lgdi32 ^
    -lwinmm ^
    -luser32 ^
    -lshell32 ^
    -lole32 ^
    -liphlpapi ^
    -lsetupapi ^
    -ldxgi ^
    -lpropsys ^
    -lshlwapi ^
    -luuid ^
    -lxinput ^
    -lws2_32 ^
    -lm

if errorlevel 1 (
    echo [FAILED] OpenGL DLL linking failed!
    exit /b 1
)
echo       OK
echo.
echo [SUCCESS] %BUILD_DIR%\situation_opengl.dll
exit /b 0

REM ========================================================================
REM BUILD: Vulkan
REM ========================================================================
:build_vulkan

REM --- Resolve Vulkan SDK ---
if not defined VULKAN_SDK (
    REM Try to auto-detect from common install locations
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" (
            set "VULKAN_SDK=%%d"
        )
    )
)
if not defined VULKAN_SDK (
    echo [ERROR] Vulkan SDK not found.
    echo         Install from https://vulkan.lunarg.com/sdk/home
    echo         Or set VULKAN_SDK environment variable.
    exit /b 1
)
if not exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" (
    echo [ERROR] Vulkan SDK path invalid: %VULKAN_SDK%
    echo         vulkan.h not found at %VULKAN_SDK%\Include\vulkan\vulkan.h
    exit /b 1
)

REM --- Check shaderc ---
if not exist "%SHADERC_LIB%\libshaderc_combined.a" (
    echo [ERROR] shaderc library not found at %SHADERC_LIB%\libshaderc_combined.a
    echo.
    echo         shaderc needs to be built. See ext\vulkan\shaderc\README.md
    echo         or use the pre-built library from the Vulkan SDK.
    echo.
    exit /b 1
)

echo.
echo ========================================
echo  Situation DLL Build - Vulkan
echo  Compiler:   GCC %GCC_VER% (C11)
echo  Vulkan SDK: %VULKAN_SDK%
echo  Backend:    Vulkan
echo ========================================
echo.

REM Step 1: Compile VMA wrapper (C++ - Vulkan only)
echo [1/4] Compiling VMA wrapper (C++)...
g++ -c "%VMA_SRC%" ^
    -o "%BUILD_DIR%\vma_wrapper.o" ^
    -std=c++11 ^
    -Iext ^
    -Iext\vulkan ^
    -I"%VULKAN_SDK%\Include"

if errorlevel 1 (
    echo [FAILED] VMA wrapper compilation failed!
    exit /b 1
)
echo       OK

REM Step 2: Compile tinycthread
echo [2/4] Compiling tinycthread...
gcc -c "%TINYCTHREAD_SRC%" ^
    -o "%BUILD_DIR%\tinycthread.o" ^
    -std=c11 ^
    -Iext\glfw\deps

if errorlevel 1 (
    echo [FAILED] tinycthread compilation failed!
    exit /b 1
)
echo       OK

REM Step 3: Compile Situation DLL (Vulkan)
echo [3/4] Compiling Situation DLL (C11 - Vulkan)...
gcc -c "%DLL_SRC%" ^
    -o "%BUILD_DIR%\situation_dll_vulkan.o" ^
    -std=c11 ^
    -msse -msse2 -msse4.1 ^
    -I. ^
    -Iext ^
    -Iext\vulkan ^
    -Iext\cgltf ^
    -Iext\cglm\include ^
    -Iext\glfw\include ^
    -Iext\glfw\deps ^
    -Isit\k-term ^
    -I"%VULKAN_SDK%\Include" ^
    -DSITUATION_USE_VULKAN ^
    -DSITUATION_ENABLE_THREADING ^
    -DSITUATION_ENABLE_SHADER_COMPILER ^
    -DSITUATION_BUILD_SHARED ^
    -DKTERM_BUILD_SHARED ^
    -DKTERM_IMPLEMENTATION

if errorlevel 1 (
    echo [FAILED] Situation DLL compilation failed!
    exit /b 1
)
echo       OK

REM Step 4: Link Vulkan DLL
echo [4/4] Linking situation_vulkan.dll...
g++ -shared ^
    "%BUILD_DIR%\situation_dll_vulkan.o" ^
    "%BUILD_DIR%\vma_wrapper.o" ^
    "%BUILD_DIR%\tinycthread.o" ^
    -o "%BUILD_DIR%\situation_vulkan.dll" ^
    -L"%GLFW_LIB%" ^
    -L"%SHADERC_LIB%" ^
    -L"%VULKAN_SDK%\Lib" ^
    -static-libgcc ^
    -static-libstdc++ ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    -lglfw3 ^
    -lvulkan-1 ^
    -lshaderc_combined ^
    -lgdi32 ^
    -lwinmm ^
    -luser32 ^
    -lshell32 ^
    -lole32 ^
    -liphlpapi ^
    -lsetupapi ^
    -ldxgi ^
    -lpropsys ^
    -lshlwapi ^
    -luuid ^
    -lxinput ^
    -lws2_32 ^
    -lm

if errorlevel 1 (
    echo [FAILED] Vulkan DLL linking failed!
    exit /b 1
)
echo       OK
echo.
echo [SUCCESS] %BUILD_DIR%\situation_vulkan.dll
exit /b 0

REM ========================================================================
REM CLEAN
REM ========================================================================
:clean
echo Cleaning build artifacts...
if exist "%BUILD_DIR%" (
    del /q "%BUILD_DIR%\*.o" 2>nul
    del /q "%BUILD_DIR%\*.dll" 2>nul
    echo Done.
) else (
    echo Nothing to clean.
)
exit /b 0

REM ========================================================================
REM DONE
REM ========================================================================
:done
echo.
echo ========================================
echo  Build complete.
echo  Output: %BUILD_DIR%\situation_%TARGET%.dll
echo ========================================
exit /b 0

:done_all
echo.
echo ========================================
echo  Both builds complete.
echo  Output: %BUILD_DIR%\situation_opengl.dll
echo          %BUILD_DIR%\situation_vulkan.dll
echo ========================================
exit /b 0

:usage
echo.
echo Usage: build_situation.bat [target]
echo.
echo Targets:
echo   vulkan   - Build Vulkan backend DLL
echo   opengl   - Build OpenGL backend DLL
echo   all      - Build both backends
echo   clean    - Remove build artifacts
echo.
echo Environment Variables:
echo   MINGW_PATH  - Path to MinGW bin directory
echo   VULKAN_SDK  - Path to Vulkan SDK root
echo.
exit /b 1
