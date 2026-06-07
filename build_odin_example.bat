@echo off
REM Build an Odin example that uses the Situation wrapper
REM Usage: build_odin_example.bat [example_name]
REM   Example: build_odin_example.bat hello_situation
REM
REM Prerequisites:
REM   - Pre-built DLL: build\dll\situation_opengl.dll (run: build_situation.bat opengl)
REM   - Generated bindings: wrappers\odin\situation_foreign.odin (run: python tools\generate_odin_bindings.py)
REM   - Odin compiler: wrappers\odin\dist\odin.exe
REM   - MinGW tools: gendef, dlltool (for import lib generation)
REM   - VS Build Tools with MSVC C++ x64 (for linking)

setlocal enabledelayedexpansion

set ODIN_EXE=wrappers\odin\dist\odin.exe
set ODIN_ROOT=wrappers\odin\dist
set EXAMPLE_NAME=%~1
set DLL_SRC=build\dll\situation_opengl.dll
set LIB_SRC=build\dll\situation_opengl.lib
set OUT_DIR=build\examples\odin

if "%EXAMPLE_NAME%"=="" (
    set EXAMPLE_NAME=hello_situation
)

set EXAMPLE_DIR=wrappers\odin\examples\%EXAMPLE_NAME%

REM --- Check prerequisites ---
if not exist "%ODIN_EXE%" (
    echo ERROR: Odin compiler not found at %ODIN_EXE%
    echo   Place the Odin distribution in wrappers\odin\dist\
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

echo Building Odin example: %EXAMPLE_NAME%
echo   Source:  %EXAMPLE_DIR%
echo   Output:  %OUT_DIR%\%EXAMPLE_NAME%.exe

REM --- Build with Odin ---
set ODIN_ROOT=%ODIN_ROOT%
"%ODIN_EXE%" build "%EXAMPLE_DIR%" -out:"%OUT_DIR%\%EXAMPLE_NAME%.exe" -o:none

if %ERRORLEVEL% neq 0 (
    echo.
    echo BUILD FAILED
    exit /b 1
)

REM --- Copy DLL next to exe ---
copy /y "%DLL_SRC%" "%OUT_DIR%\" >nul

echo.
echo BUILD OK: %OUT_DIR%\%EXAMPLE_NAME%.exe
echo Run with: %OUT_DIR%\%EXAMPLE_NAME%.exe
