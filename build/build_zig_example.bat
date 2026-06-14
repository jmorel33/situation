@echo off
REM ========================================================================
REM build_zig_example.bat - Build a Zig Situation wrapper example
REM
REM Usage:
REM   build_zig_example.bat opengl        [example_name]
REM   build_zig_example.bat vulkan        [example_name]
REM   build_zig_example.bat static-opengl [example_name]
REM   build_zig_example.bat static-vulkan [example_name]
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

REM --- static-vulkan is not supported for Zig on Windows.
REM     Zig uses lld-link (MSVC-mode linker) which cannot consume the MinGW C++ runtime
REM     archives that shaderc/VMA pull in (libmsvcprt.a, libOLDNAMES.a, etc.).
REM     static-opengl works because the OpenGL lib has no C++ runtime dependency.
if /i "%BACKEND%"=="static-vulkan" (
    echo.
    echo [ERROR] static-vulkan is not supported for Zig on Windows.
    echo.
    echo         Zig uses lld-link ^(MSVC-mode linker^), which cannot consume the
    echo         MinGW C++ runtime archives that shaderc and VMA require
    echo         ^(libmsvcprt.a, libOLDNAMES.a, libstdc++.a, etc.^).
    echo.
    echo         Use the DLL-linked build instead:
    echo           build\build_zig_example.bat vulkan %EXAMPLE_NAME%
    echo.
    exit /b 1
)

set ZIG_EXE=_languages\Zig\zig.exe
set OUT_DIR=build\examples\zig
set EXAMPLE_DIR=wrappers\Zig\examples\%EXAMPLE_NAME%

call scripts\wrapper_link_config.bat %BACKEND%
if errorlevel 1 goto :usage

if defined MINGW_PATH (
    set "PATH=%MINGW_PATH%;%PATH%"
) else if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "PATH=C:\msys64\mingw64\bin;%PATH%"
)

if not exist "%ZIG_EXE%" (
    echo [ERROR] Zig compiler not found at %ZIG_EXE%
    exit /b 1
)

if not exist "%EXAMPLE_DIR%" (
    echo [ERROR] Example not found at %EXAMPLE_DIR%
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
echo [BUILD] Zig %EXAMPLE_NAME% (%BACKEND%)
echo         Source: %EXAMPLE_DIR%
echo         Output: %OUT_DIR%\%EXAMPLE_NAME%.exe
echo.

set "ZIG_BUILD_FLAGS=-Dlink=%SIT_ZIG_LINK_FLAG% -Dexample=%EXAMPLE_NAME%"
if /i "%BACKEND%"=="static-vulkan" set "ZIG_BUILD_FLAGS=!ZIG_BUILD_FLAGS! -Dvk_sdk=%VULKAN_SDK%"
if /i "%BACKEND%"=="static-opengl" goto :zig_mingw_lib
if /i "%BACKEND%"=="static-vulkan" goto :zig_mingw_lib
goto :zig_build
:zig_mingw_lib
for /f "delims=" %%G in ('where gcc 2^>nul') do (
    set "MINGW_LIB=%%~dpG..\lib"
    for /f "delims=" %%V in ('gcc -dumpversion 2^>nul') do set "MINGW_GCC_VER=%%V"
    set "MINGW_GCC_LIB=%%~dpG..\lib\gcc\x86_64-w64-mingw32\!MINGW_GCC_VER!"
    goto :zig_build
)
:zig_build
if defined MINGW_LIB set "ZIG_BUILD_FLAGS=!ZIG_BUILD_FLAGS! -Dmingw_lib=!MINGW_LIB!"
if defined MINGW_GCC_LIB set "ZIG_BUILD_FLAGS=!ZIG_BUILD_FLAGS! -Dmingw_gcc_lib=!MINGW_GCC_LIB!"

"%ZIG_EXE%" build --build-file wrappers\Zig\build.zig --cache-dir wrappers\Zig\.zig-cache -p "%OUT_DIR%" !ZIG_BUILD_FLAGS!
if errorlevel 1 (
    echo [FAILED] Zig build failed
    exit /b 1
)

if exist "%OUT_DIR%\bin" (
    copy /y "%OUT_DIR%\bin\*.exe" "%OUT_DIR%\" >nul
    copy /y "%OUT_DIR%\bin\*.pdb" "%OUT_DIR%\" >nul 2>&1
    rd /s /q "%OUT_DIR%\bin"
)

if "%SIT_NEEDS_DLL_COPY%"=="1" (
    copy /y "%SIT_DLL_SRC%" "%OUT_DIR%\" >nul
    echo [SUCCESS] %OUT_DIR%\%EXAMPLE_NAME%.exe - requires %SIT_DLL_BASENAME%.dll alongside
) else (
    echo [SUCCESS] %OUT_DIR%\%EXAMPLE_NAME%.exe - self-contained, no DLL needed
)
exit /b 0

:usage
echo.
echo Usage: build_zig_example.bat [backend] [example_name]
echo.
echo Backends (supported):
echo   opengl          DLL-linked OpenGL  ^(requires situation_opengl.dll at runtime^)
echo   vulkan          DLL-linked Vulkan  ^(requires situation_vulkan.dll at runtime^)
echo   static-opengl   Self-contained OpenGL exe ^(no DLL needed^)
echo.
echo Unsupported on Windows (shaderc/VMA C++ runtime incompatible with lld-link):
echo   static-vulkan   ^(will error with a clear message^)
echo.
echo Examples:
echo   build_zig_example.bat opengl hello_situation
echo   build_zig_example.bat static-opengl hello_situation
echo   build_zig_example.bat vulkan hello_situation
echo.
exit /b 1
