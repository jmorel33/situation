@echo off
REM ========================================================================
REM build_rust_example.bat - Build a Rust Situation wrapper example
REM
REM Usage:
REM   build_rust_example.bat opengl        [example_name]
REM   build_rust_example.bat vulkan        [example_name]
REM   build_rust_example.bat static-opengl [example_name]
REM   build_rust_example.bat static-vulkan [example_name]
REM
REM Prerequisites (DLL):    build\build_situation.bat opengl / vulkan
REM Prerequisites (static): build\build_situation.bat static-opengl / static-vulkan
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Change to project root (one level up from build/) ---
cd /d "%~dp0.."

if "%~1"=="" goto :usage

set BACKEND=%~1
set EXAMPLE_NAME=%~2
if "%EXAMPLE_NAME%"=="" set EXAMPLE_NAME=hello_situation

REM --- Rust toolchain paths ---
set RUSTC_EXE=_languages\Rust\rustc\bin\rustc.exe
set CARGO_EXE=_languages\Rust\cargo\bin\cargo.exe
set OUT_DIR=build\examples\rust
set TARGET_TRIPLE=x86_64-pc-windows-gnu
set TARGET_DIR=_languages\Rust\rustc\lib\rustlib\%TARGET_TRIPLE%

REM --- Resolve Situation link settings ---
call scripts\wrapper_link_config.bat %BACKEND%
if errorlevel 1 goto :usage

REM --- Resolve MinGW ---
if defined MINGW_PATH (
    set "PATH=%MINGW_PATH%;%PATH%"
) else if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "PATH=C:\msys64\mingw64\bin;%PATH%"
)

REM --- Verify toolchain ---
if not exist "%RUSTC_EXE%" (
    echo [ERROR] Rust compiler not found at %RUSTC_EXE%
    exit /b 1
)
if not exist "%CARGO_EXE%" (
    echo [ERROR] Cargo not found at %CARGO_EXE%
    exit /b 1
)

REM --- Verify prerequisites ---
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

REM --- Merge Rust std + MinGW into sysroot (one-time setup) ---
if not exist "%TARGET_DIR%\lib\self-contained\libkernel32.a" (
    echo Merging Rust std and MinGW components into compiler sysroot...
    if not exist "%TARGET_DIR%\lib" mkdir "%TARGET_DIR%\lib"
    if not exist "%TARGET_DIR%\lib\self-contained" mkdir "%TARGET_DIR%\lib\self-contained"
    if exist "_languages\Rust\rust-std-x86_64-pc-windows-gnu\lib\rustlib\%TARGET_TRIPLE%\lib" (
        xcopy /E /I /Y "_languages\Rust\rust-std-x86_64-pc-windows-gnu\lib\rustlib\%TARGET_TRIPLE%\lib" "%TARGET_DIR%\lib" >nul
    )
    if exist "_languages\Rust\rust-mingw\lib\rustlib\%TARGET_TRIPLE%\lib\self-contained" (
        xcopy /E /I /Y "_languages\Rust\rust-mingw\lib\rustlib\%TARGET_TRIPLE%\lib\self-contained" "%TARGET_DIR%\lib\self-contained" >nul
    )
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM --- Put Cargo and rustc on PATH ---
for %%i in ("%RUSTC_EXE%") do set "PATH=%%~dpi;%PATH%"
for %%i in ("%CARGO_EXE%") do set "PATH=%%~dpi;%PATH%"

REM --- Resolve MinGW lib paths and export for build.rs ---
for /f "delims=" %%G in ('where gcc 2^>nul') do (
    set "SIT_MINGW_LIB=%%~dpG..\lib"
    for /f "delims=" %%V in ('gcc -dumpversion 2^>nul') do set "SIT_MINGW_GCC_LIB=%%~dpG..\lib\gcc\x86_64-w64-mingw32\%%V"
    goto :mingw_done
)
:mingw_done

REM --- Env vars consumed by build.rs ---
set "SITUATION_LINK=%SIT_RUST_LINK_ENV%"
set "CARGO_TARGET_DIR=%CD%\wrappers\Rust\target"

REM --- Per-backend linker selection (target-specific env var) ---
REM   static-opengl: pure GCC (C-only archive)
REM   static-vulkan: g++ to pull in libstdc++ from the static archive
REM   DLL modes:     lld (bundled Rust linker, smallest overhead)
if /i "%BACKEND%"=="static-opengl" (
    set "CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=gcc"
    set "RUSTFLAGS=-C linker=gcc"
) else if /i "%BACKEND%"=="static-vulkan" (
    set "CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=g++"
    set "RUSTFLAGS=-C linker=g++"
) else (
    REM DLL modes: use rust-lld (avoids needing g++ for the import lib case)
    for %%i in ("%TARGET_DIR%\bin\rust-lld.exe") do set "RUST_LLD_PATH=%%~fi"
    if defined RUST_LLD_PATH (
        set "CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=!RUST_LLD_PATH!"
        set "RUSTFLAGS="
    ) else (
        REM Fallback: gcc linker works for DLL-linked builds too
        set "CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=gcc"
        set "RUSTFLAGS="
    )
)

echo.
echo [BUILD] Rust %EXAMPLE_NAME% (%BACKEND%)
echo         Target:  %TARGET_TRIPLE%
echo         Linker:  %CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER%
echo         Output:  %OUT_DIR%\%EXAMPLE_NAME%.exe
echo.

"%CARGO_EXE%" build --example %EXAMPLE_NAME% --target %TARGET_TRIPLE% --manifest-path wrappers\Rust\Cargo.toml
if errorlevel 1 (
    echo [FAILED] Cargo build failed
    exit /b 1
)

copy /y "wrappers\Rust\target\%TARGET_TRIPLE%\debug\examples\%EXAMPLE_NAME%.exe" "%OUT_DIR%\" >nul
copy /y "wrappers\Rust\target\%TARGET_TRIPLE%\debug\examples\%EXAMPLE_NAME%.pdb" "%OUT_DIR%\" >nul 2>&1

if "%SIT_NEEDS_DLL_COPY%"=="1" (
    copy /y "%SIT_DLL_SRC%" "%OUT_DIR%\" >nul
    echo [SUCCESS] %OUT_DIR%\%EXAMPLE_NAME%.exe - requires %SIT_DLL_BASENAME%.dll alongside
) else (
    echo [SUCCESS] %OUT_DIR%\%EXAMPLE_NAME%.exe - self-contained, no DLL needed
)
exit /b 0

:usage
echo.
echo Usage: build_rust_example.bat [backend] [example_name]
echo.
echo Backends:
echo   opengl          DLL-linked OpenGL
echo   vulkan          DLL-linked Vulkan
echo   static-opengl   Self-contained OpenGL exe
echo   static-vulkan   Self-contained Vulkan exe
echo.
echo Examples:
echo   build_rust_example.bat opengl hello_situation
echo   build_rust_example.bat static-opengl hello_situation
echo.
exit /b 1
