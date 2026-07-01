@echo off
REM ========================================================================
REM build_fortran_example.bat - Build a Fortran Situation wrapper example
REM
REM Thin entry point — compile/link logic lives in scripts\wrapper_*.bat
REM
REM Usage:
REM   build_fortran_example.bat opengl        [example_name]
REM   build_fortran_example.bat vulkan        [example_name]
REM   build_fortran_example.bat static-opengl [example_name]
REM   build_fortran_example.bat static-vulkan [example_name]
REM
REM Prerequisites (DLL):    build\build_situation.bat opengl / vulkan
REM Prerequisites (static): build\build_situation.bat static-opengl / static-vulkan
REM Fortran compiler: MSYS2 mingw-w64-x86_64-gcc-fortran (gfortran)
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

cd /d "%~dp0.."

if "%~1"=="" goto :usage

set BACKEND=%~1
set EXAMPLE_NAME=%~2
if "%EXAMPLE_NAME%"=="" set EXAMPLE_NAME=hello_situation

set SIT_FORTRAN_EXAMPLE=%EXAMPLE_NAME%
set SIT_FORTRAN_BACKEND=%BACKEND%

call scripts\wrapper_link_config.bat %BACKEND%
if errorlevel 1 goto :usage

call scripts\wrapper_paths.bat fortran %EXAMPLE_NAME% %BACKEND%
if errorlevel 1 exit /b 1

set OUT_DIR=%SIT_WRAPPER_OUT_DIR%
set OUT_EXE=%OUT_DIR%\%EXAMPLE_NAME%.exe

call scripts\wrapper_mingw_setup.bat

where gfortran >nul 2>&1
if errorlevel 1 (
    echo [ERROR] gfortran not found. Install MSYS2 package:
    echo         pacman -S mingw-w64-x86_64-gcc-fortran
    exit /b 1
)

if "%SIT_NEEDS_DLL_COPY%"=="1" (
    call scripts\wrapper_ensure_import_lib.bat
    if errorlevel 1 exit /b 1
) else (
    if not exist "%SIT_STATIC_A%" (
        echo [ERROR] Static archive not found: %SIT_STATIC_A%
        echo         Run: build\build_situation.bat %BACKEND%
        exit /b 1
    )
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo.
echo [BUILD] Fortran %EXAMPLE_NAME% (%BACKEND%)
echo         Output:  %OUT_EXE%
echo.

call scripts\wrapper_compile_fortran.bat
if errorlevel 1 goto :failed

if /i "%BACKEND%"=="static-opengl" goto :link_static
if /i "%BACKEND%"=="static-vulkan" goto :link_static

set OBJ_ARGS=!SIT_FORTRAN_OBJ_ARGS!
set SIT_LINK_DRIVER=gfortran
set SIT_LINK_FLAGS=-static-libgcc -static-libgfortran
call scripts\wrapper_link_dll.bat
if errorlevel 1 goto :failed

echo [SUCCESS] %OUT_EXE% - requires %SIT_DLL_BASENAME%.dll alongside
exit /b 0

:link_static
set OBJ_LIST=!SIT_FORTRAN_OBJ_ARGS!
set SIT_FORTRAN_LINK=1
call scripts\wrapper_gcc_link_static.bat
if errorlevel 1 goto :failed
set SIT_FORTRAN_LINK=
set OBJ_LIST=
echo [SUCCESS] %OUT_EXE% - self-contained, no DLL needed
exit /b 0

:failed
echo [FAILED] Fortran build failed
exit /b 1

:usage
echo.
echo Usage: build_fortran_example.bat [backend] [example_name]
echo.
echo Backends:
echo   opengl          DLL-linked OpenGL
echo   vulkan          DLL-linked Vulkan
echo   static-opengl   Self-contained OpenGL exe
echo   static-vulkan   Self-contained Vulkan exe
echo.
echo Shared scripts: scripts\wrapper_link_config.bat, wrapper_compile_fortran.bat,
echo                  wrapper_link_dll.bat, wrapper_gcc_link_static.bat
echo.
echo Examples:
echo   build_fortran_example.bat opengl hello_situation
echo   build_fortran_example.bat static-opengl hello_situation
echo.
exit /b 1