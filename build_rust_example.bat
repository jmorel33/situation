@echo off
REM Build a Rust example that uses the Situation wrapper
REM Usage: build_rust_example.bat [example_name]
REM   Example: build_rust_example.bat hello_situation
REM
REM Prerequisites:
REM   - Pre-built DLL: build\dll\situation_opengl.dll (run: build_situation.bat opengl)
REM   - Rust toolchain: _languages\Rust\
REM

setlocal enabledelayedexpansion

set RUSTC_EXE=_languages\Rust\rustc\bin\rustc.exe
set CARGO_EXE=_languages\Rust\cargo\bin\cargo.exe
set EXAMPLE_NAME=%~1
set DLL_SRC=build\dll\situation_opengl.dll
set OUT_DIR=build\examples\rust

if "%EXAMPLE_NAME%"=="" (
    set EXAMPLE_NAME=hello_situation
)

REM --- Check prerequisites ---
if not exist "%RUSTC_EXE%" (
    echo ERROR: Rust compiler not found at %RUSTC_EXE%
    echo   Place the Rust standalone distribution in _languages\Rust\
    exit /b 1
)
if not exist "%CARGO_EXE%" (
    echo ERROR: Cargo not found at %CARGO_EXE%
    exit /b 1
)

if not exist "%DLL_SRC%" (
    echo ERROR: Situation DLL not found at %DLL_SRC%
    echo   Run: build_situation.bat opengl
    exit /b 1
)

REM --- Automate copying of target standard library and mingw tools if needed ---
set TARGET_DIR=_languages\Rust\rustc\lib\rustlib\x86_64-pc-windows-gnu
if not exist "%TARGET_DIR%\lib\self-contained\libkernel32.a" (
    echo Merging Rust std and MinGW components into compiler sysroot...
    if not exist "%TARGET_DIR%\lib" mkdir "%TARGET_DIR%\lib"
    if not exist "%TARGET_DIR%\lib\self-contained" mkdir "%TARGET_DIR%\lib\self-contained"
    
    if exist "_languages\Rust\rust-std-x86_64-pc-windows-gnu\lib\rustlib\x86_64-pc-windows-gnu\lib" (
        xcopy /E /I /Y "_languages\Rust\rust-std-x86_64-pc-windows-gnu\lib\rustlib\x86_64-pc-windows-gnu\lib" "%TARGET_DIR%\lib" >nul
    )
    if exist "_languages\Rust\rust-mingw\lib\rustlib\x86_64-pc-windows-gnu\lib\self-contained" (
        xcopy /E /I /Y "_languages\Rust\rust-mingw\lib\rustlib\x86_64-pc-windows-gnu\lib\self-contained" "%TARGET_DIR%\lib\self-contained" >nul
    )
)

REM --- Create output directory ---
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Building Rust example: %EXAMPLE_NAME%
echo   Output:  %OUT_DIR%\%EXAMPLE_NAME%.exe

REM --- Configure Environment for Cargo/rustc ---
for %%i in ("_languages\Rust\rustc\bin") do set "RUSTC_BIN_DIR=%%~fi"
for %%i in ("_languages\Rust\cargo\bin") do set "CARGO_BIN_DIR=%%~fi"
for %%i in ("%TARGET_DIR%\bin\rust-lld.exe") do set "RUST_LLD_PATH=%%~fi"

set "PATH=%RUSTC_BIN_DIR%;%CARGO_BIN_DIR%;%PATH%"
set "CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=%RUST_LLD_PATH%"

REM --- Build with Cargo ---
"%CARGO_EXE%" build --example %EXAMPLE_NAME% --manifest-path wrappers\Rust\Cargo.toml

if %ERRORLEVEL% neq 0 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

REM --- Copy built artifact and DLL to the output directory ---
copy /y "wrappers\Rust\target\debug\examples\%EXAMPLE_NAME%.exe" "%OUT_DIR%\" >nul
copy /y "wrappers\Rust\target\debug\examples\%EXAMPLE_NAME%.pdb" "%OUT_DIR%\" >nul 2>&1
copy /y "%DLL_SRC%" "%OUT_DIR%\" >nul

echo.
echo BUILD OK: %OUT_DIR%\%EXAMPLE_NAME%.exe
echo Run with: %OUT_DIR%\%EXAMPLE_NAME%.exe
