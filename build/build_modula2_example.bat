@echo off
REM ========================================================================
REM build_modula2_example.bat - Build a Modula-2 Situation wrapper example
REM
REM Thin entry point — compile/link logic lives in scripts\wrapper_*.bat
REM
REM Usage:
REM   build_modula2_example.bat opengl        [example_name]
REM   build_modula2_example.bat vulkan        [example_name]
REM   build_modula2_example.bat static-opengl [example_name]
REM   build_modula2_example.bat static-vulkan [example_name]
REM
REM Prerequisites (DLL):    build\build_situation.bat opengl / vulkan
REM Prerequisites (static): build\build_situation.bat static-opengl / static-vulkan
REM Bindings: python tools\generate_modula2_bindings.py
REM Toolchain: GNU Modula-2 (gm2) — see doc/plan/FORTRAN_MODULA2_BINDINGS_PLAN.md
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

cd /d "%~dp0.."

if "%~1"=="" goto :usage

set BACKEND=%~1
set EXAMPLE_NAME=%~2
if "%EXAMPLE_NAME%"=="" set EXAMPLE_NAME=hello_situation

set SIT_M2_EXAMPLE=%EXAMPLE_NAME%
set SIT_M2_BACKEND=%BACKEND%

call scripts\wrapper_link_config.bat %BACKEND%
if errorlevel 1 goto :usage

call scripts\wrapper_paths.bat modula2 %EXAMPLE_NAME% %BACKEND%
if errorlevel 1 exit /b 1

set OUT_DIR=%SIT_WRAPPER_OUT_DIR%
set OUT_EXE=%OUT_DIR%\%EXAMPLE_NAME%.exe
set OBJ_DIR=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%

REM --- Resolve gm2: bundled, then PATH, then MSYS2 default ---
set GM2_EXE=
if exist "_languages\gm2\bin\gm2.exe" (
    set "GM2_EXE=_languages\gm2\bin\gm2.exe"
) else (
    where gm2 >nul 2>&1
    if not errorlevel 1 (
        for /f "delims=" %%G in ('where gm2 2^>nul') do (
            set "GM2_EXE=%%G"
            goto :gm2_found
        )
    )
    if exist "C:\msys64\mingw64\bin\gm2.exe" (
        set "GM2_EXE=C:\msys64\mingw64\bin\gm2.exe"
    )
)
:gm2_found

if not defined GM2_EXE (
    echo.
    echo [ERROR] GNU Modula-2 compiler ^(gm2^) not found.
    echo         See doc\plan\FORTRAN_MODULA2_BINDINGS_PLAN.md
    exit /b 1
)

call scripts\wrapper_mingw_setup.bat

where gcc >nul 2>&1
if errorlevel 1 (
    echo [ERROR] gcc not found. Add MSYS2 mingw64\bin to PATH or set MINGW_PATH.
    exit /b 1
)

for %%i in ("%GM2_EXE%") do set "GM2_BIN=%%~dpi"
set "GM2_RT_LIB=%GM2_BIN%..\lib\gcc\x86_64-w64-mingw32\15.1.0"
if not exist "%GM2_RT_LIB%\libgm2.a" (
    set "GM2_RT_LIB=_languages\gm2-build\build\gcc\m2\gm2-libs"
)
set "GM2_LINK=-L%GM2_RT_LIB% -lgm2 -lstdc++ -lgcc"

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

set "SIT_LINK_DRIVER=g++"
if /i "%BACKEND%"=="static-vulkan" set "SIT_LINK_DRIVER=g++"

echo.
echo [BUILD] Modula-2 %EXAMPLE_NAME% (%BACKEND%)
echo         Compiler: %GM2_EXE%
echo         Linker:   %SIT_LINK_DRIVER%
echo         Output:   %OUT_EXE%
echo.

call scripts\wrapper_compile_modula2.bat
if errorlevel 1 goto :failed

set OBJ_ARGS=%SIT_M2_OBJ_ARGS%
set SIT_EXTRA_LDFLAGS=%GM2_LINK%

if "%SIT_NEEDS_DLL_COPY%"=="1" (
    call scripts\wrapper_link_dll.bat
    if errorlevel 1 goto :failed
    echo [SUCCESS] %OUT_EXE% - requires %SIT_DLL_BASENAME%.dll alongside
    exit /b 0
)

set OBJ_GLOB=%OBJ_DIR%\*.o
call scripts\wrapper_gcc_link_static.bat
if errorlevel 1 goto :failed

echo [SUCCESS] %OUT_EXE% - self-contained, no DLL needed
exit /b 0

:failed
echo [FAILED] Modula-2 build failed
exit /b 1

:usage
echo.
echo Usage: build_modula2_example.bat [backend] [example_name]
echo.
echo Backends:
echo   opengl          DLL-linked OpenGL
echo   vulkan          DLL-linked Vulkan
echo   static-opengl   Self-contained OpenGL exe
echo   static-vulkan   Self-contained Vulkan exe
echo.
echo Shared scripts: scripts\wrapper_link_config.bat, wrapper_compile_modula2.bat,
echo                  wrapper_link_dll.bat, wrapper_gcc_link_static.bat
echo.
echo Examples:
echo   build_modula2_example.bat opengl hello_situation
echo   build_modula2_example.bat static-opengl hello_situation
echo.
exit /b 1