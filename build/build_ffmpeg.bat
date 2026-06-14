@echo off
REM ========================================================================
REM build_ffmpeg.bat - Build minimal LGPL FFmpeg static libs for Situation
REM
REM Usage:
REM   build_ffmpeg.bat          Configure + build + install to ext\ffmpeg\build
REM   build_ffmpeg.bat clean    Remove ext\ffmpeg\build
REM
REM Prerequisites (MSYS2 MinGW64):
REM   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-make
REM              mingw-w64-x86_64-nasm mingw-w64-x86_64-yasm
REM              mingw-w64-x86_64-pkgconf mingw-w64-x86_64-diffutils
REM
REM Environment:
REM   MSYS2_ROOT   Default C:\msys64
REM   FFMPEG_JOBS  Parallel make jobs (default: nproc)
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Change to project root (one level up from build/) ---
cd /d "%~dp0.."

if /i "%~1"=="clean" goto :clean

set "MSYS2_ROOT=C:\msys64"
if defined MSYS2_ROOT_USER set "MSYS2_ROOT=%MSYS2_ROOT_USER%"

if not exist "%MSYS2_ROOT%\usr\bin\bash.exe" (
    echo [ERROR] MSYS2 bash not found at %MSYS2_ROOT%\usr\bin\bash.exe
    echo         Install MSYS2 from https://www.msys2.org/
    echo         Or set MSYS2_ROOT to your installation path.
    exit /b 1
)

if not exist "ext\ffmpeg\configure" (
    echo [ERROR] FFmpeg source not found at ext\ffmpeg\configure
    exit /b 1
)

set "PATH=%MSYS2_ROOT%\mingw64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

where gcc >nul 2>&1
if errorlevel 1 (
    echo [ERROR] mingw-w64 gcc not found. Install: pacman -S mingw-w64-x86_64-gcc
    exit /b 1
)

where nasm >nul 2>&1
if errorlevel 1 (
    echo [WARN] nasm not found — FFmpeg x86 assembly may be disabled.
    echo        Install: pacman -S mingw-w64-x86_64-nasm
)

echo.
echo Running build_ffmpeg.sh via MSYS2 bash ...
echo.

set "REPO=%CD:\=/%"
"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "cd '%REPO%' && bash build_ffmpeg.sh"
if errorlevel 1 (
    echo.
    echo [FAILED] FFmpeg build failed.
    exit /b 1
)

echo.
echo [DONE] FFmpeg static libs are in ext\ffmpeg\build\lib\
echo       Situation is unchanged — video wiring lands in sit/situation_impl_video.h (Phase 1+).
exit /b 0

:clean
echo Cleaning ext\ffmpeg\build ...
if exist "ext\ffmpeg\build" (
    rmdir /s /q "ext\ffmpeg\build"
    echo Done.
) else (
    echo Nothing to clean.
)
exit /b 0
