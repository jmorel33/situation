@echo off
REM ========================================================================
REM debug.bat - Build Situation library and tests in Debug mode, then run under GDB
REM
REM Usage:
REM   debug.bat [options] [target] [test_args...]
REM
REM Options:
REM   --no-build            Skip recompiling (run only)
REM   --rebuild, -B         Force a full clean rebuild of the library
REM   --break <function>    Set breakpoint (default: _SituationSetErrorFromCode)
REM
REM Targets:
REM   static-opengl   OpenGL static (self-contained exe)
REM   static-vulkan   Vulkan static (self-contained exe)
REM   opengl          OpenGL DLL-linked
REM   vulkan          Vulkan DLL-linked
REM   clean           Remove build products (*.o *.dll *.a)
REM   distclean       Remove all artifacts (including *.def *.lib)
REM
REM Examples:
REM   debug.bat static-vulkan --module virtual_display
REM   debug.bat --no-build vulkan --filter spirv --verbose
REM   debug.bat --break _SitVulkanEnsureGraphicsPipelineBound static-vulkan
REM ========================================================================

setlocal enabledelayedexpansion

REM --- Argument Parsing ---
set "NO_BUILD="
set "FORCE_REBUILD="
set "BREAKPOINT=_SituationSetErrorFromCode"
set "TARGET="
set "TEST_ARGS="

:parse_args
if "%~1"=="" goto :parse_done
if /i "%~1"=="--no-build" (
    set NO_BUILD=1
    shift
    goto :parse_args
)
if /i "%~1"=="--rebuild" (
    set FORCE_REBUILD=1
    shift
    goto :parse_args
)
if /i "%~1"=="-B" (
    set FORCE_REBUILD=1
    shift
    goto :parse_args
)
if /i "%~1"=="--break" (
    set "BREAKPOINT=%~2"
    shift
    shift
    goto :parse_args
)

if not defined TARGET (
    set "TARGET=%~1"
    shift
    goto :parse_args
)

REM Collect all remaining arguments for test harness
set "TEST_ARGS=!TEST_ARGS! %1"
shift
goto :parse_args

:parse_done

if "%TARGET%"=="" goto :usage

REM --- Clean Targets ---
if /i "%TARGET%"=="clean" goto :clean_action
if /i "%TARGET%"=="distclean" goto :clean_action
goto :resolve_target

:clean_action
call :resolve_toolchain
if errorlevel 1 exit /b 1
echo [DEBUG-BUILD] Forwarding clean target to Makefile...
"%MINGW_BIN%\mingw32-make.exe" -C "%~dp0sit" %TARGET%
exit /b %ERRORLEVEL%

:resolve_target
set BACKEND=
if /i "%TARGET%"=="static-opengl" set BACKEND=opengl
if /i "%TARGET%"=="opengl"        set BACKEND=opengl
if /i "%TARGET%"=="static-vulkan" set BACKEND=vulkan
if /i "%TARGET%"=="vulkan"        set BACKEND=vulkan

if "%BACKEND%"=="" (
    echo [ERROR] Unknown target: %TARGET%
    goto :usage
)

call :resolve_toolchain
if errorlevel 1 exit /b 1

if defined NO_BUILD (
    echo [DEBUG-BUILD] Skipping build phase - no-build flag active...
    goto :run_gdb
)

REM --- Resolve environment variable compiler flags ---
REM Default to -O0 -g if not already defined (to preserve user overrides)
if not defined SIT_OPTIMIZE_CFLAGS set "SIT_OPTIMIZE_CFLAGS=-O0 -g"
if not defined EXTRA_VULKAN_CFLAGS set "EXTRA_VULKAN_CFLAGS=-O0 -g"

REM --- Recompile Situation Library ---
echo [DEBUG-BUILD] Recompiling Situation Library (%TARGET%)...
echo               SIT_OPTIMIZE_CFLAGS: %SIT_OPTIMIZE_CFLAGS%
echo               EXTRA_VULKAN_CFLAGS: %EXTRA_VULKAN_CFLAGS%

set MAKE_FLAGS=
if defined FORCE_REBUILD set MAKE_FLAGS=-B

"%MINGW_BIN%\mingw32-make.exe" %MAKE_FLAGS% -C "%~dp0sit" %TARGET% "SIT_OPTIMIZE_CFLAGS=%SIT_OPTIMIZE_CFLAGS%" "EXTRA_VULKAN_CFLAGS=%EXTRA_VULKAN_CFLAGS%"
if errorlevel 1 (
    echo [ERROR] Situation library compilation failed!
    exit /b 1
)

REM --- Recompile Test Harness ---
echo [DEBUG-BUILD] Recompiling Situation Test Harness (%TARGET%)...
call "%~dp0build\build_tests.bat" %TARGET%
if errorlevel 1 (
    echo [ERROR] Test harness compilation failed!
    exit /b 1
)

:run_gdb
REM --- Setup Path for DLLs ---
set "PATH=%~dp0build\dll;%PATH%"
set EXE=build\tests\sit_test_%BACKEND%.exe

if not exist "%~dp0%EXE%" (
    echo [ERROR] Target executable %EXE% not found!
    exit /b 1
)

REM --- Run under GDB ---
echo.
echo [GDB] Launching %EXE% under GDB...
echo       Breakpoint: %BREAKPOINT%
echo       Arguments:  %TEST_ARGS%
echo.

gdb -ex "break %BREAKPOINT%" -ex run --args "%~dp0%EXE%" %TEST_ARGS%
exit /b %ERRORLEVEL%

REM --- Helper to resolve toolchain ---
:resolve_toolchain
if defined MINGW_PATH (
    set "MINGW_BIN=%MINGW_PATH%"
) else (
    set "MINGW_BIN=C:\msys64\mingw64\bin"
)
if not exist "%MINGW_BIN%\gcc.exe" (
    echo [ERROR] MinGW toolchain not found at "%MINGW_BIN%". Set MINGW_PATH or install MSYS2.
    exit /b 1
)
if not exist "%MINGW_BIN%\mingw32-make.exe" (
    echo [ERROR] mingw32-make not found in "%MINGW_BIN%".
    exit /b 1
)
set "PATH=%MINGW_BIN%;%PATH%"
exit /b 0

:usage
echo.
echo Usage: debug.bat [options] [target] [test_args...]
echo.
echo Options:
echo   --no-build            Skip compiling, run debugger immediately
echo   --rebuild, -B         Force a clean rebuild of the library
echo   --break ^<function^>    Override default breakpoint (default: _SituationSetErrorFromCode)
echo.
echo Targets:
echo   static-opengl   OpenGL static (self-contained exe)
echo   static-vulkan   Vulkan static (self-contained exe)
echo   opengl          OpenGL DLL-linked
echo   vulkan          Vulkan DLL-linked
echo   clean           Remove build products (*.o *.dll *.a)
echo   distclean       Remove all artifacts (including *.def *.lib)
echo.
echo Examples:
echo   debug.bat static-vulkan --module virtual_display
echo   debug.bat --no-build vulkan --filter spirv --verbose
echo   debug.bat --break _SitVulkanEnsureGraphicsPipelineBound static-vulkan
echo.
exit /b 1
