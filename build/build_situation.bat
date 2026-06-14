@echo off
REM ========================================================================
REM build_situation.bat - Official Situation DLL Build Script
REM
REM Usage:
REM   build_situation.bat vulkan         Build Vulkan backend DLL
REM   build_situation.bat opengl         Build OpenGL backend DLL
REM   build_situation.bat all            Build both
REM   build_situation.bat static-opengl  Build OpenGL static lib
REM   build_situation.bat static-vulkan  Build Vulkan static lib
REM   build_situation.bat clean          Remove build artifacts
REM
REM Environment Variables (optional overrides):
REM   MINGW_PATH          - Path to MinGW bin (default: C:\msys64\mingw64\bin)
REM   VULKAN_SDK          - Path to Vulkan SDK (auto-detected if not set)
REM   SIT_OPTIMIZE_CFLAGS - Default: -O2 -mfma -ffp-contract=fast
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal

REM --- Change to project root (one level up from build/) ---
cd /d "%~dp0.."

REM --- Configuration ---
set BUILD_DIR=build\dll
set GLFW_LIB=ext\glfw\build\src
set SHADERC_LIB=ext\shaderc\build\libshaderc
set VMA_SRC=ext\vma_wrapper.cpp
set TINYCTHREAD_SRC=ext\glfw\deps\tinycthread.c
set DLL_SRC=situation_dll.c

if not defined SIT_OPTIMIZE_CFLAGS set "SIT_OPTIMIZE_CFLAGS=-O2 -mfma -ffp-contract=fast"

REM --- Parse Arguments ---
if "%~1"=="" goto :usage
if /i "%~1"=="vulkan"        goto :setup
if /i "%~1"=="opengl"        goto :setup
if /i "%~1"=="all"           goto :setup
if /i "%~1"=="static-opengl" goto :setup
if /i "%~1"=="static-vulkan" goto :setup
if /i "%~1"=="clean"         goto :clean
goto :usage

:setup
set TARGET=%~1

REM --- Resolve MinGW ---
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
gcc --version >nul 2>&1
if errorlevel 1 ( echo [ERROR] gcc not found in PATH. & exit /b 1 )
for /f "tokens=*" %%i in ('gcc -dumpversion') do set GCC_VER=%%i

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

if not exist "%GLFW_LIB%\libglfw3.a" (
    echo [ERROR] GLFW not found at %GLFW_LIB%\libglfw3.a
    echo         Build GLFW first: cd ext\glfw ^& mkdir build ^& cd build
    echo           cmake .. -G "MinGW Makefiles" ... ^& mingw32-make
    exit /b 1
)

REM --- Compile tinycthread once (shared by all targets) ---
echo [common] Compiling tinycthread...
gcc -c "%TINYCTHREAD_SRC%" -o "%BUILD_DIR%\tinycthread.o" -std=c11 -Iext\glfw\deps
if errorlevel 1 ( echo [FAILED] tinycthread & exit /b 1 )

if /i "%TARGET%"=="all"           goto :dispatch_all
if /i "%TARGET%"=="opengl"        goto :dispatch_opengl
if /i "%TARGET%"=="vulkan"        goto :dispatch_vulkan
if /i "%TARGET%"=="static-opengl" goto :dispatch_static_opengl
if /i "%TARGET%"=="static-vulkan" goto :dispatch_static_vulkan
goto :usage

:dispatch_all
call :build_opengl
if errorlevel 1 exit /b 1
call :build_vulkan
if errorlevel 1 exit /b 1
goto :done_all

:dispatch_opengl
call :build_opengl
if errorlevel 1 exit /b 1
goto :done

:dispatch_vulkan
call :build_vulkan
if errorlevel 1 exit /b 1
goto :done

:dispatch_static_opengl
call :build_static_opengl
if errorlevel 1 exit /b 1
goto :done_static_opengl

:dispatch_static_vulkan
call :build_static_vulkan
if errorlevel 1 exit /b 1
goto :done_static_vulkan

REM ========================================================================
REM BUILD: OpenGL DLL
REM ========================================================================
:build_opengl
echo.
echo ========================================
echo  Situation DLL Build - OpenGL
echo  Compiler: GCC %GCC_VER% (C11)
echo ========================================
echo.
echo [1/2] Compiling Situation (OpenGL)...
gcc -c "%DLL_SRC%" -o "%BUILD_DIR%\situation_dll_opengl.o" -std=c11 %SIT_OPTIMIZE_CFLAGS% -msse -msse2 -msse4.1 -I. -Isit -Iext -Iext\cgltf -Iext\cglm\include -Iext\glfw\include -Iext\glfw\deps -Isit\k-term -DSITUATION_USE_OPENGL -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_RENDER_THREAD -DSITUATION_BUILD_SHARED -DKTERM_BUILD_SHARED -DKTERM_IMPLEMENTATION
if errorlevel 1 ( echo [FAILED] Situation compile & exit /b 1 )
echo [2/2] Linking situation_opengl.dll...
gcc -shared "%BUILD_DIR%\situation_dll_opengl.o" "%BUILD_DIR%\tinycthread.o" -o "%BUILD_DIR%\situation_opengl.dll" -L"%GLFW_LIB%" -static-libgcc -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
if errorlevel 1 ( echo [FAILED] Link & exit /b 1 )
gendef - "%BUILD_DIR%\situation_opengl.dll" > "%BUILD_DIR%\situation_opengl.def" 2>nul
if errorlevel 1 ( echo [FAILED] gendef (export list) & exit /b 1 )
dlltool -D situation_opengl.dll -d "%BUILD_DIR%\situation_opengl.def" -l "%BUILD_DIR%\situation_opengl.lib"
if errorlevel 1 ( echo [FAILED] dlltool (import lib) & exit /b 1 )
echo [SUCCESS] %BUILD_DIR%\situation_opengl.dll
exit /b 0

REM ========================================================================
REM BUILD: Vulkan DLL
REM ========================================================================
:build_vulkan
if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK ( echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK. & exit /b 1 )
if not exist "%VULKAN_SDK%\Include\vulkan\vulkan.h" ( echo [ERROR] Vulkan SDK invalid: %VULKAN_SDK% & exit /b 1 )
if not exist "%SHADERC_LIB%\libshaderc_combined.a" (
    echo [ERROR] shaderc not found at %SHADERC_LIB%\libshaderc_combined.a
    exit /b 1
)
echo.
echo ========================================
echo  Situation DLL Build - Vulkan
echo  Compiler:   GCC %GCC_VER% (C11)
echo  Vulkan SDK: %VULKAN_SDK%
echo ========================================
echo.
echo [1/3] Compiling VMA wrapper...
g++ -c "%VMA_SRC%" -o "%BUILD_DIR%\vma_wrapper.o" -std=c++11 -Iext -Iext\vulkan -I"%VULKAN_SDK%\Include"
if errorlevel 1 ( echo [FAILED] VMA & exit /b 1 )
echo [2/3] Compiling Situation (Vulkan)...
gcc -c "%DLL_SRC%" -o "%BUILD_DIR%\situation_dll_vulkan.o" -std=c11 %SIT_OPTIMIZE_CFLAGS% -msse -msse2 -msse4.1 -I. -Isit -Iext -Iext\vulkan -Iext\cgltf -Iext\cglm\include -Iext\glfw\include -Iext\glfw\deps -Isit\k-term -I"%VULKAN_SDK%\Include" -Iext\shaderc\libshaderc\include -DSITUATION_USE_VULKAN -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_RENDER_THREAD -DSITUATION_ENABLE_SHADER_COMPILER -DSITUATION_BUILD_SHARED -DKTERM_BUILD_SHARED -DKTERM_IMPLEMENTATION %EXTRA_VULKAN_CFLAGS%
if errorlevel 1 ( echo [FAILED] Situation compile & exit /b 1 )
echo [3/3] Linking situation_vulkan.dll...
g++ -shared "%BUILD_DIR%\situation_dll_vulkan.o" "%BUILD_DIR%\vma_wrapper.o" "%BUILD_DIR%\tinycthread.o" -o "%BUILD_DIR%\situation_vulkan.dll" -L"%GLFW_LIB%" -L"%SHADERC_LIB%" -L"%VULKAN_SDK%\Lib" -static-libgcc -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive -lglfw3 -lvulkan-1 -lshaderc_combined -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
if errorlevel 1 ( echo [FAILED] Link & exit /b 1 )
gendef - "%BUILD_DIR%\situation_vulkan.dll" > "%BUILD_DIR%\situation_vulkan.def" 2>nul
if errorlevel 1 ( echo [FAILED] gendef (export list) & exit /b 1 )
dlltool -D situation_vulkan.dll -d "%BUILD_DIR%\situation_vulkan.def" -l "%BUILD_DIR%\situation_vulkan.lib"
if errorlevel 1 ( echo [FAILED] dlltool (import lib) & exit /b 1 )
echo [SUCCESS] %BUILD_DIR%\situation_vulkan.dll
exit /b 0

REM ========================================================================
REM BUILD: Static OpenGL
REM ========================================================================
:build_static_opengl
echo.
echo ========================================
echo  Situation Static Lib - OpenGL
echo  Compiler: GCC %GCC_VER% (C11)
echo ========================================
echo.
echo [1/2] Compiling Situation (OpenGL, static)...
gcc -c "%DLL_SRC%" -o "%BUILD_DIR%\situation_static_opengl.o" -std=c11 %SIT_OPTIMIZE_CFLAGS% -msse -msse2 -msse4.1 -I. -Isit -Iext -Iext\cgltf -Iext\cglm\include -Iext\glfw\include -Iext\glfw\deps -Isit\k-term -DSITUATION_USE_OPENGL -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_RENDER_THREAD -DKTERM_IMPLEMENTATION
if errorlevel 1 ( echo [FAILED] Situation compile & exit /b 1 )
echo [2/2] Archiving situation_opengl.a...
ar rcs "%BUILD_DIR%\situation_opengl.a" "%BUILD_DIR%\situation_static_opengl.o" "%BUILD_DIR%\tinycthread.o"
if errorlevel 1 ( echo [FAILED] Archive & exit /b 1 )
echo [SUCCESS] %BUILD_DIR%\situation_opengl.a
exit /b 0

REM ========================================================================
REM BUILD: Static Vulkan
REM ========================================================================
:build_static_vulkan
if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK ( echo [ERROR] Vulkan SDK not found. & exit /b 1 )
echo.
echo ========================================
echo  Situation Static Lib - Vulkan
echo  Compiler:   GCC %GCC_VER% (C11)
echo  Vulkan SDK: %VULKAN_SDK%
echo ========================================
echo.
echo [1/3] Compiling VMA wrapper...
g++ -c "%VMA_SRC%" -o "%BUILD_DIR%\vma_wrapper.o" -std=c++11 -Iext -Iext\vulkan -I"%VULKAN_SDK%\Include"
if errorlevel 1 ( echo [FAILED] VMA & exit /b 1 )
echo [2/3] Compiling Situation (Vulkan, static)...
gcc -c "%DLL_SRC%" -o "%BUILD_DIR%\situation_static_vulkan.o" -std=c11 %SIT_OPTIMIZE_CFLAGS% -msse -msse2 -msse4.1 -I. -Isit -Iext -Iext\vulkan -Iext\cgltf -Iext\cglm\include -Iext\glfw\include -Iext\glfw\deps -Isit\k-term -I"%VULKAN_SDK%\Include" -Iext\shaderc\libshaderc\include -DSITUATION_USE_VULKAN -DCGLM_FORCE_DEPTH_ZERO_TO_ONE -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_RENDER_THREAD -DSITUATION_ENABLE_SHADER_COMPILER -DKTERM_IMPLEMENTATION
if errorlevel 1 ( echo [FAILED] Situation compile & exit /b 1 )
echo [3/3] Archiving situation_vulkan.a...
ar rcs "%BUILD_DIR%\situation_vulkan.a" "%BUILD_DIR%\situation_static_vulkan.o" "%BUILD_DIR%\vma_wrapper.o" "%BUILD_DIR%\tinycthread.o"
if errorlevel 1 ( echo [FAILED] Archive & exit /b 1 )
echo [SUCCESS] %BUILD_DIR%\situation_vulkan.a
exit /b 0

REM ========================================================================
REM CLEAN
REM ========================================================================
:clean
echo Cleaning build artifacts...
if exist "build\dll" (
    del /q "build\dll\*.o" 2>nul
    del /q "build\dll\*.dll" 2>nul
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
echo Build complete. Output: %BUILD_DIR%\situation_%TARGET%.dll
exit /b 0

:done_all
echo.
echo Both builds complete.
echo   %BUILD_DIR%\situation_opengl.dll
echo   %BUILD_DIR%\situation_vulkan.dll
exit /b 0

:done_static_opengl
echo.
echo Static build complete. Output: %BUILD_DIR%\situation_opengl.a
exit /b 0

:done_static_vulkan
echo.
echo Static build complete. Output: %BUILD_DIR%\situation_vulkan.a
exit /b 0

:usage
echo.
echo Usage: build_situation.bat [target]
echo.
echo   opengl          Build OpenGL DLL      (situation_opengl.dll)
echo   vulkan          Build Vulkan DLL       (situation_vulkan.dll)
echo   all             Build both DLLs
echo   static-opengl   Build OpenGL static lib (situation_opengl.a)
echo   static-vulkan   Build Vulkan static lib  (situation_vulkan.a)
echo   clean           Remove build artifacts
echo.
echo Environment variables:
echo   MINGW_PATH  - Path to MinGW bin directory
echo   VULKAN_SDK  - Path to Vulkan SDK root
echo.
exit /b 1
