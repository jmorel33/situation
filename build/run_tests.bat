@echo off
REM ========================================================================
REM run_tests.bat - Run the Situation test harness
REM
REM Handles both static and DLL-linked builds. Static builds can also be
REM run directly without this launcher.
REM
REM Output is printed to the console AND saved to a timestamped results file
REM in build\tests\results\ for later consultation.
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

setlocal enabledelayedexpansion

REM --- Change to project root (one level up from build/) ---
cd /d "%~dp0.."

if "%~1"=="" goto :usage

REM Add build\dll to PATH so DLL-linked builds also work
set "PATH=%CD%\build\dll;%PATH%"

REM Determine backend and exe
if /i "%~1"=="opengl" (
    set EXE=build\tests\sit_test_opengl.exe
    set BACKEND=opengl
    shift
) else if /i "%~1"=="vulkan" (
    set EXE=build\tests\sit_test_vulkan.exe
    set BACKEND=vulkan
    shift
) else (
    echo [ERROR] Unknown backend: %~1
    goto :usage
)

if not exist "%EXE%" (
    echo [ERROR] %EXE% not found.
    echo         Run: build\build_tests.bat static-opengl  ^(or static-vulkan^)
    exit /b 1
)

REM Create results folder if needed
if not exist "build\tests\results" mkdir "build\tests\results"

REM Build timestamped filename: YYYYMMDD_HHMMSS_backend
for /f "tokens=1-3 delims=/ " %%a in ('date /t') do (
    set DATE_STR=%%c%%a%%b
)
for /f "tokens=1-2 delims=: " %%a in ('time /t') do (
    set TIME_STR=%%a%%b
)
REM Normalize: remove spaces, pad if needed
set DATE_STR=%DATE_STR: =%
set TIME_STR=%TIME_STR: =%

set RESULTS_FILE=build\tests\results\%DATE_STR%_%TIME_STR%_%BACKEND%.txt

echo.
echo [RUN] %EXE% %1 %2 %3 %4 %5 %6 %7 %8 %9
echo       Results: %RESULTS_FILE%
echo.

REM Run and tee to file
REM PowerShell Tee-Object writes to file while passing through to stdout
powershell -NoProfile -Command ^
    "& '%CD%\%EXE%' %1 %2 %3 %4 %5 %6 %7 %8 %9 2>&1 | Tee-Object -FilePath '%CD%\%RESULTS_FILE%'"

REM Preserve the harness exit code
set HARNESS_EXIT=%ERRORLEVEL%

echo.
echo [SAVED] %RESULTS_FILE%
exit /b %HARNESS_EXIT%

:usage
echo.
echo Usage: run_tests.bat [opengl^|vulkan] [--module name] [--filter substr] [--verbose]
echo.
echo   run_tests.bat opengl
echo   run_tests.bat vulkan --module graphics
echo   run_tests.bat vulkan --filter spirv --verbose
echo.
exit /b 1
