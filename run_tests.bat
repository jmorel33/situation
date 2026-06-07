@echo off
REM ========================================================================
REM run_tests.bat - Run the Situation test harness
REM
REM Handles both static and DLL-linked builds. Static builds can also be
REM run directly without this launcher.
REM
REM Usage:
REM   run_tests.bat opengl [args]     Run OpenGL harness
REM   run_tests.bat vulkan [args]     Run Vulkan harness
REM
REM   args: --module name, --filter substr, --verbose, --headless, etc.
REM
REM Examples:
REM   run_tests.bat opengl
REM   run_tests.bat vulkan --module graphics --filter spirv
REM   run_tests.bat vulkan --module audio --verbose
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal

if "%~1"=="" goto :usage

REM Add build\dll to PATH so DLL-linked builds also work
set "PATH=%~dp0build\dll;%PATH%"

if /i "%~1"=="opengl" (
    set EXE=build\tests\sit_test_opengl.exe
    shift
) else if /i "%~1"=="vulkan" (
    set EXE=build\tests\sit_test_vulkan.exe
    shift
) else (
    echo [ERROR] Unknown backend: %~1
    goto :usage
)

if not exist "%EXE%" (
    echo [ERROR] %EXE% not found.
    echo         Run: build_tests.bat static-opengl  ^(or static-vulkan^)
    exit /b 1
)

"%EXE%" %1 %2 %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%

:usage
echo.
echo Usage: run_tests.bat [opengl^|vulkan] [--module name] [--filter substr] [--verbose]
echo.
echo   run_tests.bat opengl
echo   run_tests.bat vulkan --module graphics
echo   run_tests.bat vulkan --filter spirv --verbose
echo.
exit /b 1
