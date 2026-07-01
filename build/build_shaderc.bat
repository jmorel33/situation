@echo off
REM ========================================================================
REM build_shaderc.bat - Build shaderc for Situation (Vulkan DLL + glslc)
REM
REM Usage (from project root):
REM   build\build_shaderc.bat            Build if libshaderc_combined.a is missing
REM   build\build_shaderc.bat rebuild    Remove build dir and rebuild from scratch
REM   build\build_shaderc.bat sync       Run git-sync-deps only (fetch third_party)
REM   build\build_shaderc.bat clean      Remove ext\shaderc\build
REM
REM Output:
REM   ext\shaderc\build\libshaderc\libshaderc_combined.a  (vulkan DLL link)
REM   ext\shaderc\build\glslc\glslc.exe                   (SPIR-V precompile scripts)
REM
REM Prerequisites (MSYS2 MinGW64):
REM   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
REM              mingw-w64-x86_64-make mingw-w64-x86_64-python
REM   git on PATH (for utils/git-sync-deps)
REM
REM Environment:
REM   MINGW_PATH   MinGW bin directory (default: C:\msys64\mingw64\bin)
REM
REM Note: static-vulkan does NOT require shaderc. Only the vulkan DLL target
REM       links libshaderc_combined.a for runtime GLSL->SPIR-V compilation.
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Change to project root (one level up from build/) ---
cd /d "%~dp0.."

set "ACTION=build"
if /i "%~1"=="clean"   set "ACTION=clean"
if /i "%~1"=="rebuild" set "ACTION=rebuild"
if /i "%~1"=="sync"    set "ACTION=sync"
if /i "%~1"=="help"    goto :usage
if /i "%~1"=="-h"      goto :usage
if /i "%~1"=="--help"  goto :usage
if not "%~1"=="" (
    if /i not "%ACTION%"=="build" goto :after_args
    echo [ERROR] Unknown argument: %~1
    goto :usage
)
:after_args

set "SHADERC_ROOT=ext\shaderc"
set "SHADERC_BUILD=%SHADERC_ROOT%\build"
set "SHADERC_LIB=%SHADERC_BUILD%\libshaderc\libshaderc_combined.a"
set "GLSLC_EXE=%SHADERC_BUILD%\glslc\glslc.exe"

if /i "%ACTION%"=="clean" goto :clean
if /i "%ACTION%"=="sync"  goto :sync_only

if /i "%ACTION%"=="rebuild" (
    echo [shaderc] Rebuild requested — removing %SHADERC_BUILD% ...
    if exist "%SHADERC_BUILD%" rmdir /s /q "%SHADERC_BUILD%"
)

if exist "%SHADERC_LIB%" if /i not "%ACTION%"=="rebuild" (
    echo [shaderc] Already built: %SHADERC_LIB%
    if exist "%GLSLC_EXE%" (
        echo [shaderc] glslc: %GLSLC_EXE%
    ) else (
        echo [shaderc] glslc missing — rebuilding glslc_exe only ...
        goto :configure_and_build_glslc_only
    )
    echo         Use "build\build_shaderc.bat rebuild" for a full rebuild.
    exit /b 0
)

goto :full_build

:sync_only
call :resolve_toolchain
if errorlevel 1 exit /b 1
call :check_shaderc_source
if errorlevel 1 exit /b 1
call :sync_deps
exit /b %ERRORLEVEL%

:full_build
call :resolve_toolchain
if errorlevel 1 exit /b 1
call :check_shaderc_source
if errorlevel 1 exit /b 1
call :sync_deps
if errorlevel 1 exit /b 1
call :configure_cmake
if errorlevel 1 exit /b 1
call :build_shaderc
exit /b %ERRORLEVEL%

:configure_and_build_glslc_only
call :resolve_toolchain
if errorlevel 1 exit /b 1
if not exist "%SHADERC_BUILD%\CMakeCache.txt" (
    call :configure_cmake
    if errorlevel 1 exit /b 1
)
pushd "%SHADERC_BUILD%"
echo.
echo [shaderc] Building glslc_exe ...
mingw32-make glslc_exe
set "RC=!ERRORLEVEL!"
popd
if not "!RC!"=="0" (
    echo.
    echo [FAILED] glslc build failed.
    exit /b 1
)
echo.
echo [DONE] %GLSLC_EXE%
exit /b 0

:resolve_toolchain
if defined MINGW_PATH (
    set "MINGW_BIN=%MINGW_PATH%"
) else (
    set "MINGW_BIN=C:\msys64\mingw64\bin"
)

if not exist "%MINGW_BIN%\gcc.exe" (
    echo [ERROR] MinGW gcc not found at "%MINGW_BIN%".
    echo         Install MSYS2 or set MINGW_PATH to your mingw64\bin directory.
    exit /b 1
)
if not exist "%MINGW_BIN%\g++.exe" (
    echo [ERROR] MinGW g++ not found in "%MINGW_BIN%".
    exit /b 1
)
if not exist "%MINGW_BIN%\mingw32-make.exe" (
    echo [ERROR] mingw32-make not found in "%MINGW_BIN%".
    exit /b 1
)
if not exist "%MINGW_BIN%\cmake.exe" (
    echo [ERROR] cmake not found in "%MINGW_BIN%".
    echo         Install: pacman -S mingw-w64-x86_64-cmake
    exit /b 1
)

set "PATH=%MINGW_BIN%;%PATH%"
exit /b 0

:check_shaderc_source
if not exist "%SHADERC_ROOT%\CMakeLists.txt" (
    echo [ERROR] shaderc source not found at %SHADERC_ROOT%\CMakeLists.txt
    exit /b 1
)
if not exist "%SHADERC_ROOT%\utils\git-sync-deps" (
    echo [ERROR] Missing %SHADERC_ROOT%\utils\git-sync-deps
    exit /b 1
)
exit /b 0

:sync_deps
where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] python not found on PATH (required for git-sync-deps).
    echo         Install: pacman -S mingw-w64-x86_64-python
    exit /b 1
)
where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] git not found on PATH (required for git-sync-deps).
    exit /b 1
)

echo.
echo [shaderc] Syncing third_party dependencies (git-sync-deps) ...
echo           This may take a few minutes on first run.
pushd "%SHADERC_ROOT%"
python utils\git-sync-deps
set "RC=!ERRORLEVEL!"
popd
if not "!RC!"=="0" (
    echo.
    echo [FAILED] git-sync-deps failed.
    exit /b 1
)
exit /b 0

:configure_cmake
if not exist "%SHADERC_BUILD%" mkdir "%SHADERC_BUILD%"

echo.
echo [shaderc] Configuring with CMake ...
pushd "%SHADERC_BUILD%"
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ^
    -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON ^
    -DSHADERC_ENABLE_SHARED_CRT=OFF
set "RC=!ERRORLEVEL!"
popd
if not "!RC!"=="0" (
    echo.
    echo [FAILED] CMake configure failed.
    exit /b 1
)
exit /b 0

:build_shaderc
echo.
echo [shaderc] Building shaderc_combined + glslc_exe ...
echo           Expect 10-30 minutes on first build.
pushd "%SHADERC_BUILD%"
mingw32-make shaderc_combined glslc_exe
set "RC=!ERRORLEVEL!"
popd
if not "!RC!"=="0" (
    echo.
    echo [FAILED] shaderc build failed.
    exit /b 1
)

if not exist "%SHADERC_LIB%" (
    echo [ERROR] Build finished but library missing: %SHADERC_LIB%
    exit /b 1
)

echo.
echo ========================================================================
echo  shaderc build succeeded
echo ========================================================================
echo   Library : %SHADERC_LIB%
if exist "%GLSLC_EXE%" (
    echo   glslc   : %GLSLC_EXE%
) else (
    echo   glslc   : [WARN] not found at %GLSLC_EXE%
)
echo.
echo Next: build\build_situation.bat vulkan
exit /b 0

:clean
echo Cleaning %SHADERC_BUILD% ...
if exist "%SHADERC_BUILD%" (
    rmdir /s /q "%SHADERC_BUILD%"
    echo Done.
) else (
    echo Nothing to clean.
)
exit /b 0

:usage
echo.
echo Usage:
echo   build\build_shaderc.bat            Build if missing
echo   build\build_shaderc.bat rebuild    Full rebuild
echo   build\build_shaderc.bat sync       git-sync-deps only
echo   build\build_shaderc.bat clean      Remove ext\shaderc\build
echo.
exit /b 1
