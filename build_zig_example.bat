@echo off
REM Build a Zig example that uses the Situation wrapper
REM Usage: build_zig_example.bat [example_name]
REM   Example: build_zig_example.bat hello_situation
REM
REM Prerequisites:
REM   - Pre-built DLL: build\dll\situation_opengl.dll (run: build_situation.bat opengl)
REM   - Zig compiler: _languages\Zig\zig.exe
REM   - MinGW tools: gendef, dlltool (for import lib generation)

setlocal enabledelayedexpansion

set ZIG_EXE=_languages\Zig\zig.exe
set EXAMPLE_NAME=%~1
set DLL_SRC=build\dll\situation_opengl.dll
set LIB_SRC=build\dll\situation_opengl.lib
set OUT_DIR=build\examples\zig

if "%EXAMPLE_NAME%"=="" (
    set EXAMPLE_NAME=hello_situation
)

set EXAMPLE_DIR=wrappers\Zig\examples\%EXAMPLE_NAME%

REM --- Check prerequisites ---
if not exist "%ZIG_EXE%" (
    echo ERROR: Zig compiler not found at %ZIG_EXE%
    echo   Place the Zig distribution in _languages\Zig\
    exit /b 1
)

if not exist "%DLL_SRC%" (
    echo ERROR: Situation DLL not found at %DLL_SRC%
    echo   Run: build_situation.bat opengl
    exit /b 1
)

if not exist "%EXAMPLE_DIR%" (
    echo ERROR: Example not found at %EXAMPLE_DIR%
    exit /b 1
)

REM --- Generate import library if missing or older than DLL ---
if not exist "%LIB_SRC%" (
    echo Generating import library from DLL...
    where gendef >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        echo ERROR: gendef not found. Install MinGW-w64 or add MSYS2 bin to PATH.
        exit /b 1
    )
    where dlltool >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        echo ERROR: dlltool not found. Install MinGW-w64 or add MSYS2 bin to PATH.
        exit /b 1
    )
    gendef "%DLL_SRC%"
    dlltool -d situation_opengl.def -l "%LIB_SRC%" -D situation_opengl.dll
    move /y situation_opengl.def build\dll\ >nul
    if not exist "%LIB_SRC%" (
        echo ERROR: Failed to generate import library.
        exit /b 1
    )
    echo   Created: %LIB_SRC%
)

REM --- Create output directory ---
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Building Zig example: %EXAMPLE_NAME%
echo   Source:  %EXAMPLE_DIR%
echo   Output:  %OUT_DIR%\%EXAMPLE_NAME%.exe

REM --- Build with Zig ---
REM We build using build.zig, outputting to a prefix matching our OUT_DIR.
REM Zig installs binaries into a 'bin' subdirectory under the prefix.
"%ZIG_EXE%" build --build-file wrappers\Zig\build.zig --cache-dir wrappers\Zig\.zig-cache -p "%OUT_DIR%"

if %ERRORLEVEL% neq 0 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

REM --- Move built artifacts to the root of OUT_DIR to match other wrappers ---
if exist "%OUT_DIR%\bin" (
    copy /y "%OUT_DIR%\bin\*.exe" "%OUT_DIR%\" >nul
    copy /y "%OUT_DIR%\bin\*.pdb" "%OUT_DIR%\" >nul 2>&1
    rd /s /q "%OUT_DIR%\bin"
)

REM --- Copy DLL next to exe ---
copy /y "%DLL_SRC%" "%OUT_DIR%\" >nul

echo.
echo BUILD OK: %OUT_DIR%\%EXAMPLE_NAME%.exe
echo Run with: %OUT_DIR%\%EXAMPLE_NAME%.exe
