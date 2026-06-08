@echo off
REM ========================================================================
REM build_examples.bat - Build Situation examples
REM
REM Usage:
REM   build_examples.bat opengl        [example]   DLL-linked OpenGL
REM   build_examples.bat vulkan        [example]   DLL-linked Vulkan
REM   build_examples.bat static-opengl [example]   Static OpenGL (self-contained exe)
REM   build_examples.bat static-vulkan [example]   Static Vulkan (self-contained exe)
REM
REM Prerequisites (DLL modes):    build_situation.bat opengl / vulkan
REM Prerequisites (static modes): build_situation.bat static-opengl / static-vulkan
REM
REM NOTE: demon_hunt is Vulkan-only. Use vulkan or static-vulkan for it.
REM
REM Examples:
REM   build_examples.bat opengl        kterm_console
REM   build_examples.bat opengl        quad_storm
REM   build_examples.bat vulkan        demon_hunt
REM   build_examples.bat static-vulkan demon_hunt
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Configuration ---
set BUILD_DIR=build\examples
set DLL_DIR=build\dll
set GLFW_LIB=ext\glfw\build\src
set SHADERC_LIB=ext\shaderc\build\libshaderc

REM --- Parse Arguments ---
if "%~1"=="" goto :usage
if "%~2"=="" goto :usage
set BACKEND=%~1
set EXAMPLE=%~2

REM --- demon_hunt is Vulkan-only ---
if /i "%EXAMPLE%"=="demon_hunt" (
    if /i "%BACKEND%"=="opengl" (
        echo [ERROR] demon_hunt is Vulkan-only. The sky shader exceeds OpenGL SPIR-V instruction limits.
        echo         Use: build_examples.bat vulkan demon_hunt
        echo          or: build_examples.bat static-vulkan demon_hunt
        exit /b 1
    )
    if /i "%BACKEND%"=="static-opengl" (
        echo [ERROR] demon_hunt is Vulkan-only.
        echo         Use: build_examples.bat static-vulkan demon_hunt
        exit /b 1
    )
)

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

REM --- Resolve source file path (examples with their own subfolder use subfolder/name.c) ---
set "EXAMPLE_SRC=examples\%EXAMPLE%.c"
if /i "%EXAMPLE%"=="demon_hunt" set "EXAMPLE_SRC=examples\demon_hunt\demon_hunt.c"
if /i "%EXAMPLE%"=="kterm_console" set "EXAMPLE_SRC=examples\console\console_host_app.c"

REM --- Check source file exists ---
if not exist "%EXAMPLE_SRC%" (
    echo [ERROR] Source file not found: %EXAMPLE_SRC%
    exit /b 1
)

REM --- Dispatch ---
if /i "%BACKEND%"=="opengl"        goto :build_opengl
if /i "%BACKEND%"=="vulkan"        goto :build_vulkan
if /i "%BACKEND%"=="static-opengl" goto :build_static_opengl
if /i "%BACKEND%"=="static-vulkan" goto :build_static_vulkan
goto :usage

REM ========================================================================
REM BUILD: OpenGL DLL-linked
REM ========================================================================
:build_opengl

if not exist "%DLL_DIR%\situation_opengl.dll" (
    echo [ERROR] situation_opengl.dll not found in %DLL_DIR%
    echo         Run: build_situation.bat opengl
    exit /b 1
)

echo.
echo [BUILD] %EXAMPLE% (OpenGL, DLL) - GCC %GCC_VER%
echo         Linking against: %DLL_DIR%\situation_opengl.dll
echo.

set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="kterm_console"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="platformer_plumber"    set "EXTRA_LDFLAGS=-mwindows"

gcc %EXAMPLE_SRC% ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Iexamples/console -Iext -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -DSITUATION_USE_OPENGL -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING ^
    -L%DLL_DIR% -lsituation_opengl ^
    -static-libgcc %EXTRA_LDFLAGS% -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

copy /Y "%DLL_DIR%\situation_opengl.dll" "%BUILD_DIR%\situation_opengl.dll" >nul
echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe
exit /b 0

REM ========================================================================
REM BUILD: Vulkan DLL-linked
REM ========================================================================
:build_vulkan

if not exist "%DLL_DIR%\situation_vulkan.dll" (
    echo [ERROR] situation_vulkan.dll not found in %DLL_DIR%
    echo         Run: build_situation.bat vulkan
    exit /b 1
)

if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK ( echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK. & exit /b 1 )

echo.
echo [BUILD] %EXAMPLE% (Vulkan, DLL) - GCC %GCC_VER%
echo         Linking against: %DLL_DIR%\situation_vulkan.dll
echo.

set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="demon_hunt"            set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="kterm_console"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="platformer_plumber"    set "EXTRA_LDFLAGS=-mwindows"

REM demon_hunt: precompile shaders and include SPIR-V embed
set "DH_EMBED_SRC="
if /i "%EXAMPLE%"=="demon_hunt" (
    call compile_demon_hunt_shaders.bat
    if errorlevel 1 echo [WARN] Shader precompile failed — game needs .spv at launch.
    set "DH_EMBED_SRC=examples/demon_hunt/demon_hunt_sky_spirv_embed.c"
)

gcc %EXAMPLE_SRC% %DH_EMBED_SRC% ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Iexamples/console -Iext -Iext/vulkan -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -I"%VULKAN_SDK%\Include" ^
    -DSITUATION_USE_VULKAN -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER ^
    -L%DLL_DIR% -lsituation_vulkan ^
    -static-libgcc %EXTRA_LDFLAGS% -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

copy /Y "%DLL_DIR%\situation_vulkan.dll" "%BUILD_DIR%\situation_vulkan.dll" >nul
echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe
exit /b 0

REM ========================================================================
REM BUILD: OpenGL static (self-contained exe)
REM ========================================================================
:build_static_opengl

set STATIC_LIB=build\dll\situation_opengl.a
if not exist "%STATIC_LIB%" (
    echo [ERROR] situation_opengl.a not found in build\dll
    echo         Run: build_situation.bat static-opengl
    exit /b 1
)

echo.
echo [BUILD] %EXAMPLE% (OpenGL, static) - GCC %GCC_VER%
echo         Linking against: %STATIC_LIB%
echo.

set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="kterm_console"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="platformer_plumber"    set "EXTRA_LDFLAGS=-mwindows"

gcc %EXAMPLE_SRC% ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Iexamples/console -Iext -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -DSITUATION_USE_OPENGL -DSITUATION_ENABLE_THREADING ^
    -L%GLFW_LIB% -static-libgcc %EXTRA_LDFLAGS% ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    "%STATIC_LIB%" ^
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe  (self-contained, no DLL needed)
exit /b 0

REM ========================================================================
REM BUILD: Vulkan static (self-contained exe)
REM ========================================================================
:build_static_vulkan

set STATIC_LIB=build\dll\situation_vulkan.a
if not exist "%STATIC_LIB%" (
    echo [ERROR] situation_vulkan.a not found in build\dll
    echo         Run: build_situation.bat static-vulkan
    exit /b 1
)

if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK ( echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK. & exit /b 1 )

echo.
echo [BUILD] %EXAMPLE% (Vulkan, static) - GCC %GCC_VER%
echo         Linking against: %STATIC_LIB%
echo.

set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="demon_hunt"            set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="kterm_console"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="platformer_plumber"    set "EXTRA_LDFLAGS=-mwindows"

set "DH_EMBED_SRC="
if /i "%EXAMPLE%"=="demon_hunt" (
    call compile_demon_hunt_shaders.bat
    if errorlevel 1 echo [WARN] Shader precompile failed.
    set "DH_EMBED_SRC=examples/demon_hunt/demon_hunt_sky_spirv_embed.c"
)

REM g++ needed for shaderc/VMA C++ runtime in the static archive
REM Step 1: compile the C source with gcc
gcc -c %EXAMPLE_SRC% ^
    -o %BUILD_DIR%\%EXAMPLE%_main.o ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Iexamples/console -Iext -Iext/vulkan -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -I"%VULKAN_SDK%\Include" ^
    -DSITUATION_USE_VULKAN -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER

if errorlevel 1 ( echo [FAILED] C compilation failed! & exit /b 1 )

REM Step 2: compile SPIR-V embed (if present) with gcc
set "DH_EMBED_OBJ="
if defined DH_EMBED_SRC (
    gcc -c %DH_EMBED_SRC% ^
        -o %BUILD_DIR%\%EXAMPLE%_embed.o ^
        -std=c11 -O2 ^
        -I. -Iexamples/console -Iext -Iext/cglm/include -Isit/k-term ^
        -DSITUATION_USE_VULKAN
    if errorlevel 1 ( echo [FAILED] SPIR-V embed compilation failed! & exit /b 1 )
    set "DH_EMBED_OBJ=%BUILD_DIR%\%EXAMPLE%_embed.o"
)

REM Step 3: link with g++ to pull in the C++ runtime needed by the static archive
g++ %BUILD_DIR%\%EXAMPLE%_main.o %DH_EMBED_OBJ% ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -L%GLFW_LIB% -L%SHADERC_LIB% -L"%VULKAN_SDK%\Lib" ^
    -static-libgcc -static-libstdc++ %EXTRA_LDFLAGS% ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    "%STATIC_LIB%" ^
    -lglfw3 -lvulkan-1 -lshaderc_combined ^
    -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm

if errorlevel 1 ( echo [FAILED] Link failed! & exit /b 1 )

REM Cleanup intermediates
del "%BUILD_DIR%\%EXAMPLE%_main.o" 2>nul
del "%BUILD_DIR%\%EXAMPLE%_embed.o" 2>nul

echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe  (self-contained, no DLL needed)
exit /b 0

REM ========================================================================
:usage
echo.
echo Usage: build_examples.bat [backend] [example]
echo.
echo Backends:
echo   opengl          DLL-linked    (needs situation_opengl.dll next to exe)
echo   vulkan          DLL-linked    (needs situation_vulkan.dll next to exe)
echo   static-opengl   Self-contained exe, no DLL required
echo   static-vulkan   Self-contained exe, no DLL required
echo.
echo Examples:
echo   build_examples.bat opengl        quad_storm
echo   build_examples.bat opengl        kterm_console
echo   build_examples.bat vulkan        demon_hunt        ^(Vulkan-only^)
echo   build_examples.bat static-vulkan demon_hunt        ^(Vulkan-only, portable exe^)
echo.
echo Prerequisites:
echo   opengl / vulkan          ^> build_situation.bat opengl ^(or vulkan^)
echo   static-opengl            ^> build_situation.bat static-opengl
echo   static-vulkan            ^> build_situation.bat static-vulkan
echo.
exit /b 1
