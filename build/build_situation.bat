@echo off
REM ========================================================================
REM build_situation.bat - Thin Launcher for Situation Build System
REM
REM Forwards targets to sit/Makefile via mingw32-make.
REM All build logic lives in sit/Makefile.
REM
REM Usage:
REM   build_situation.bat opengl         Build OpenGL DLL
REM   build_situation.bat vulkan         Build Vulkan DLL
REM   build_situation.bat all            Build both DLLs
REM   build_situation.bat static-opengl  Build OpenGL static lib
REM   build_situation.bat static-vulkan  Build Vulkan static lib
REM   build_situation.bat clean          Remove build products (*.o *.dll *.a) - forces recompile on next build
REM
REM Environment Variables (optional overrides):
REM   MINGW_PATH          - Path to MinGW bin (default: C:\msys64\mingw64\bin)
REM   VULKAN_SDK          - Path to Vulkan SDK root (Makefile autodetects if unset)
REM   SIT_OPTIMIZE_CFLAGS - Default: -O2 -mfma -ffp-contract=fast
REM   EXTRA_VULKAN_CFLAGS - Extra flags appended to Vulkan compile lines (default: empty)
REM
REM Fail-safe: the original build logic is preserved in build_situation_legacy.bat
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal

REM --- Compute the Makefile directory (sit/ relative to this script) ---
set "SIT_DIR=%~dp0..\sit"

REM --- Validate argument ---
if "%~1"=="" goto :usage
if /i "%~1"=="opengl"        goto :resolve_toolchain
if /i "%~1"=="vulkan"        goto :resolve_toolchain
if /i "%~1"=="all"           goto :resolve_toolchain
if /i "%~1"=="all-static"    goto :resolve_toolchain
if /i "%~1"=="static-opengl" goto :resolve_toolchain
if /i "%~1"=="static-vulkan" goto :resolve_toolchain
if /i "%~1"=="clean"         goto :resolve_toolchain
if /i "%~1"=="distclean"     goto :resolve_toolchain
if /i "%~1"=="help"          goto :resolve_toolchain
goto :usage

:resolve_toolchain
set "TARGET=%~1"

REM --- Resolve MinGW bin directory ---
if defined MINGW_PATH (
    set "MINGW_BIN=%MINGW_PATH%"
) else (
    set "MINGW_BIN=C:\msys64\mingw64\bin"
)

if not exist "%MINGW_BIN%\gcc.exe" (
    echo [ERROR] MinGW toolchain not found at "%MINGW_BIN%". Set MINGW_PATH or install MSYS2.
    exit /b 1
)

if not exist "%MINGW_BIN%\mingw32-make.exe" (
    echo [ERROR] mingw32-make not found in "%MINGW_BIN%".
    exit /b 1
)

REM --- Prepend MinGW to PATH so make can find gcc, g++, ar, gendef, dlltool ---
set "PATH=%MINGW_BIN%;%PATH%"

REM --- OpenGL: precompile VD compositor SPIR-V embed (same headers as Vulkan shaderc path) ---
if /i "%TARGET%"=="opengl" (
    powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0compile_vd_compositor_gl.ps1"
    if errorlevel 1 exit /b 1
)
if /i "%TARGET%"=="static-opengl" (
    powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0compile_vd_compositor_gl.ps1"
    if errorlevel 1 exit /b 1
)
if /i "%TARGET%"=="all" (
    powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0compile_vd_compositor_gl.ps1"
    if errorlevel 1 exit /b 1
)
if /i "%TARGET%"=="all-static" (
    powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0compile_vd_compositor_gl.ps1"
    if errorlevel 1 exit /b 1
)

REM --- Optional Tracy profiling (second arg or env SIT_TRACY=1) ---
if /i "%~2"=="tracy" set SIT_TRACY=1

REM --- Forward target to Makefile ---
REM  -C changes make's working directory to sit/ so all ../ paths resolve correctly.
REM  VULKAN_SDK, SIT_OPTIMIZE_CFLAGS, and EXTRA_VULKAN_CFLAGS are intentionally NOT
REM  set here — the Makefile reads them from the environment, preserving override semantics.
"%MINGW_BIN%\mingw32-make.exe" -C "%SIT_DIR%" %TARGET%
exit /b %ERRORLEVEL%

:usage
echo.
echo Usage: build_situation.bat [target]
echo.
echo   opengl          Build OpenGL DLL       (situation_opengl.dll)
echo   vulkan          Build Vulkan DLL        (situation_vulkan.dll)
echo   all             Build both DLLs
echo   static-opengl   Build OpenGL static lib (situation_opengl.a)
echo   static-vulkan   Build Vulkan static lib  (situation_vulkan.a)
echo   all-static      Build both static libs
echo   clean           Remove build products (*.o *.dll *.a) - forces recompile on next build
echo   distclean       Remove all artifacts (including *.def *.lib)
echo.
echo Environment variables:
echo   MINGW_PATH          Path to MinGW bin directory
echo   VULKAN_SDK          Path to Vulkan SDK root
echo   SIT_OPTIMIZE_CFLAGS Compiler optimization flags
echo   EXTRA_VULKAN_CFLAGS Extra flags for Vulkan compile lines
echo   SIT_TRACY=1         Enable Tracy CPU profiling (opt-in)
echo.
echo Optional second argument: tracy  (same as SIT_TRACY=1)
echo   Example: build_situation.bat opengl tracy
echo.
exit /b 1
