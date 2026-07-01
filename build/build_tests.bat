@echo off
REM ========================================================================
REM build_tests.bat - Thin Launcher for Situation Test Harness
REM
REM Forwards targets to tests/harness/Makefile via mingw32-make.
REM All harness build logic lives in tests/harness/Makefile.
REM
REM Usage:
REM   build_tests.bat static-opengl   Self-contained OpenGL exe (no DLL)
REM   build_tests.bat static-vulkan   Self-contained Vulkan exe (no DLL)
REM   build_tests.bat all-static      Both static harness exes
REM   build_tests.bat opengl          OpenGL DLL-linked (fast build)
REM   build_tests.bat vulkan          Vulkan DLL-linked (fast build)
REM   build_tests.bat all             Both DLL-linked harness exes
REM
REM Prerequisites:
REM   static-opengl / all-static   build\build_situation.bat static-opengl (and static-vulkan for all-static)
REM   static-vulkan / all-static   build\build_situation.bat static-vulkan
REM   opengl / all                 build\build_situation.bat opengl (and vulkan for all)
REM   vulkan / all                 build\build_situation.bat vulkan
REM
REM Output: build\tests\sit_test_opengl.exe or sit_test_vulkan.exe
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal

set "HARNESS_DIR=%~dp0..\tests\harness"
set "TARGET=%~1"

if "%TARGET%"=="" goto :usage
if /i "%TARGET%"=="static-opengl" goto :resolve_toolchain
if /i "%TARGET%"=="static-vulkan" goto :resolve_toolchain
if /i "%TARGET%"=="all-static"    goto :resolve_toolchain
if /i "%TARGET%"=="opengl"        goto :resolve_toolchain
if /i "%TARGET%"=="vulkan"        goto :resolve_toolchain
if /i "%TARGET%"=="all"           goto :resolve_toolchain
if /i "%TARGET%"=="clean"         goto :resolve_toolchain
if /i "%TARGET%"=="shaders"       goto :resolve_toolchain
if /i "%TARGET%"=="help"          goto :resolve_toolchain
goto :usage

:resolve_toolchain
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

set "PATH=%MINGW_BIN%;%PATH%"

"%MINGW_BIN%\mingw32-make.exe" -s -C "%HARNESS_DIR%" %TARGET%
exit /b %ERRORLEVEL%

:usage
echo.
echo Usage: build_tests.bat [target]
echo.
echo   static-opengl   Self-contained OpenGL exe (no DLL needed)
echo   static-vulkan   Self-contained Vulkan exe (no DLL needed)
echo   all-static      Build both static harness exes
echo   opengl          DLL-linked OpenGL (use build\run_tests.bat to run)
echo   vulkan          DLL-linked Vulkan (use build\run_tests.bat to run)
echo   all             Build both DLL-linked harness exes
echo   shaders         Regenerate harness SPIR-V embeds only
echo   clean           Remove harness build products
echo.
echo Prerequisites:
echo   static-opengl / all-static   ^> build\build_situation.bat static-opengl (+ static-vulkan)
echo   static-vulkan / all-static ^> build\build_situation.bat static-vulkan
echo   opengl / all                 ^> build\build_situation.bat opengl (+ vulkan)
echo   vulkan / all                 ^> build\build_situation.bat vulkan
echo.
exit /b 1