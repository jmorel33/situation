@echo off
REM ========================================================================
REM build_python_example.bat - Build a Python Situation wrapper example
REM
REM Thin entry point — compile/link logic lives in scripts\wrapper_*.bat
REM
REM Produces build\examples\python\<example>.exe via PyInstaller (onefile),
REM with situation_*.dll copied alongside (same as Rust/Fortran DLL mode).
REM
REM Usage:
REM   build_python_example.bat opengl        [example_name] [--no-run]
REM   build_python_example.bat vulkan        [example_name]
REM
REM Prerequisites (DLL): build\build_situation.bat opengl / vulkan
REM Bindings:           python tools\generate_python_bindings.py
REM Link toolchain:     pip install pyinstaller
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

cd /d "%~dp0.."

if "%~1"=="" goto :usage

set BACKEND=%~1
set EXAMPLE_NAME=%~2
set NO_RUN=
if /i "%~2"=="--no-run" (
    set EXAMPLE_NAME=hello_situation
    set NO_RUN=1
) else if /i "%~3"=="--no-run" (
    set NO_RUN=1
)
if "%EXAMPLE_NAME%"=="" set EXAMPLE_NAME=hello_situation

set SIT_PYTHON_EXAMPLE=%EXAMPLE_NAME%
set SIT_PYTHON_BACKEND=%BACKEND%

call scripts\wrapper_link_config.bat %BACKEND%
if errorlevel 1 goto :usage

call scripts\wrapper_paths.bat python %EXAMPLE_NAME% %BACKEND%
if errorlevel 1 exit /b 1

set OUT_DIR=%SIT_WRAPPER_OUT_DIR%
set OUT_EXE=%OUT_DIR%\%EXAMPLE_NAME%.exe

if "%SIT_NEEDS_DLL_COPY%"=="1" (
    if not exist "%SIT_DLL_SRC%" (
        echo [ERROR] DLL not found: %SIT_DLL_SRC%
        echo         Run: build\build_situation.bat %BACKEND%
        exit /b 1
    )
)

echo.
echo [BUILD] Python %EXAMPLE_NAME% (%BACKEND%)
echo         Output:  %OUT_EXE%
echo.

call scripts\wrapper_compile_python.bat
if errorlevel 1 goto :failed

call scripts\wrapper_link_python.bat
if errorlevel 1 goto :failed

if defined NO_RUN (
    if "%SIT_NEEDS_DLL_COPY%"=="1" (
        echo [SUCCESS] %OUT_EXE% - requires %SIT_DLL_BASENAME%.dll alongside
    ) else (
        echo [SUCCESS] %OUT_EXE%
    )
    exit /b 0
)

set "SIT_PYTHON_BACKEND=%BACKEND%"
"%OUT_EXE%"
set EXIT_CODE=%errorlevel%

echo.
if %EXIT_CODE%==0 (
    if "%SIT_NEEDS_DLL_COPY%"=="1" (
        echo [SUCCESS] %OUT_EXE% - requires %SIT_DLL_BASENAME%.dll alongside
    ) else (
        echo [SUCCESS] %OUT_EXE%
    )
) else (
    echo [FAILED] Python %EXAMPLE_NAME% exit code %EXIT_CODE%
)
exit /b %EXIT_CODE%

:failed
echo [FAILED] Python build failed
exit /b 1

:usage
echo.
echo Usage: build_python_example.bat [backend] [example_name] [--no-run]
echo.
echo Backends:
echo   opengl   DLL-linked OpenGL
echo   vulkan   DLL-linked Vulkan
echo.
echo Toolchain:
echo   python 3.10+
echo   pip install pyinstaller
echo.
echo Shared scripts: wrapper_compile_python.bat, wrapper_link_python.bat
echo.
echo Examples:
echo   build_python_example.bat opengl hello_situation
echo   build_python_example.bat opengl hello_situation --no-run
echo.
exit /b 1
