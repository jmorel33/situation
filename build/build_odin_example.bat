@echo off
REM ========================================================================
REM build_odin_example.bat - Build an Odin Situation wrapper example
REM
REM Usage:
REM   build_odin_example.bat opengl        [example_name]
REM   build_odin_example.bat vulkan        [example_name]
REM   build_odin_example.bat static-opengl [example_name]
REM   build_odin_example.bat static-vulkan [example_name]
REM
REM Prerequisites (DLL):    build\build_situation.bat opengl / vulkan
REM Prerequisites (static): build\build_situation.bat static-opengl / static-vulkan
REM Bindings: python tools\generate_odin_bindings.py
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

REM --- Static builds are not supported for Odin on Windows.
REM     Odin uses MSVC link.exe / lld-link (MSVC mode) which cannot consume
REM     MinGW .a archives (the situation static libs are built with GCC).
REM     Symbols like sincosf, __imp__aligned_free, __gxx_personality_seh0 are
REM     GCC/MinGW-only and have no MSVC equivalents in the same ABI.
REM     Use the DLL-linked modes (opengl / vulkan) instead.
if /i "%BACKEND%"=="static-opengl" (
    echo.
    echo [ERROR] static-opengl is not supported for Odin on Windows.
    echo.
    echo         Odin uses MSVC link.exe / lld-link ^(MSVC mode^), which cannot
    echo         consume MinGW .a archives built with GCC. The situation static
    echo         lib depends on GCC-only symbols ^(sincosf, __imp__aligned_free,
    echo         libmingwex, etc.^) that are unavailable in the MSVC CRT.
    echo.
    echo         Use the DLL-linked build instead:
    echo           build\build_odin_example.bat opengl %EXAMPLE_NAME%
    echo.
    exit /b 1
)
if /i "%BACKEND%"=="static-vulkan" (
    echo.
    echo [ERROR] static-vulkan is not supported for Odin on Windows.
    echo.
    echo         Odin uses MSVC link.exe / lld-link ^(MSVC mode^), which cannot
    echo         consume MinGW .a archives built with GCC. The situation static
    echo         lib + shaderc + VMA all use the GCC C++ ABI ^(__cxx11 STL,
    echo         __gxx_personality_seh0, __cxa_begin_catch, sincosf, etc.^)
    echo         which are incompatible with MSVC lld-link.
    echo.
    echo         Use the DLL-linked build instead:
    echo           build\build_odin_example.bat vulkan %EXAMPLE_NAME%
    echo.
    exit /b 1
)

set ODIN_EXE=_languages\odin\dist\odin.exe
set ODIN_ROOT=_languages\odin\dist
set OUT_DIR=build\examples\odin
set EXAMPLE_DIR=wrappers\Odin\examples\%EXAMPLE_NAME%

if defined MINGW_PATH (
    set "PATH=%MINGW_PATH%;%PATH%"
) else if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "PATH=C:\msys64\mingw64\bin;%PATH%"
)

call scripts\wrapper_link_config.bat %BACKEND%
if errorlevel 1 goto :usage
if not exist "%ODIN_EXE%" (
    echo [ERROR] Odin compiler not found at %ODIN_EXE%
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

REM Clean stale intermediates and old stray exes from a previous run.
REM Odin drops *.o files alongside the -out: path; remove them so the
REM output folder only ever contains the final exe + DLL.
if exist "%OUT_DIR%\*.o"   del /q "%OUT_DIR%\*.o"   >nul 2>&1
if exist "%OUT_DIR%\*.obj" del /q "%OUT_DIR%\*.obj" >nul 2>&1

call scripts\wrapper_patch_odin_foreign.bat
if errorlevel 1 exit /b 1
echo.
echo [BUILD] Odin %EXAMPLE_NAME% (%BACKEND%)
echo         Source: %EXAMPLE_DIR%
echo         Output: %OUT_DIR%\%EXAMPLE_NAME%.exe
echo.

set ODIN_ROOT=%ODIN_ROOT%
set BUILD_ERR=0

if "%SIT_NEEDS_DLL_COPY%"=="1" (
    "%ODIN_EXE%" build "%EXAMPLE_DIR%" -out:"%OUT_DIR%\%EXAMPLE_NAME%.exe" -o:none
    set BUILD_ERR=!ERRORLEVEL!
) else (
    "%ODIN_EXE%" build "%EXAMPLE_DIR%" -extra-linker-flags:"%SIT_ODIN_EXTRA_LDFLAGS%" -out:"%OUT_DIR%\%EXAMPLE_NAME%.exe" -o:none
    set BUILD_ERR=!ERRORLEVEL!
)
call scripts\wrapper_restore_odin_foreign.bat
if not "%BUILD_ERR%"=="0" (
    echo [FAILED] Odin build failed
    exit /b 1
)

REM Remove any intermediate object files Odin dropped during this build.
if exist "%OUT_DIR%\*.o"   del /q "%OUT_DIR%\*.o"   >nul 2>&1
if exist "%OUT_DIR%\*.obj" del /q "%OUT_DIR%\*.obj" >nul 2>&1

if "%SIT_NEEDS_DLL_COPY%"=="1" (
    copy /y "%SIT_DLL_SRC%" "%OUT_DIR%\" >nul
    echo [SUCCESS] %OUT_DIR%\%EXAMPLE_NAME%.exe - requires %SIT_DLL_BASENAME%.dll alongside
) else (
    echo [SUCCESS] %OUT_DIR%\%EXAMPLE_NAME%.exe - self-contained, no DLL needed
)
exit /b 0

:usage
echo.
echo Usage: build_odin_example.bat [backend] [example_name]
echo.
echo Backends (supported):
echo   opengl          DLL-linked OpenGL  ^(requires situation_opengl.dll at runtime^)
echo   vulkan          DLL-linked Vulkan  ^(requires situation_vulkan.dll at runtime^)
echo.
echo Unsupported on Windows (Odin uses MSVC linker; situation is built with GCC):
echo   static-opengl   ^(will error with a clear message^)
echo   static-vulkan   ^(will error with a clear message^)
echo.
echo Prerequisites:
echo   build\build_situation.bat opengl   ^(for opengl backend^)
echo   build\build_situation.bat vulkan   ^(for vulkan backend^)
echo.
echo Examples:
echo   build_odin_example.bat opengl hello_situation
echo   build_odin_example.bat vulkan hello_situation
echo.
exit /b 1
