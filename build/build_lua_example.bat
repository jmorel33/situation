@echo off
REM ========================================================================
REM build_lua_example.bat - Build a self-contained Lua Situation example .exe
REM
REM Produces build\examples\lua\<example>.exe only (LuaJIT + scripts + Situation
REM DLL embedded — extracted to %%TEMP%% at runtime, nothing beside the exe).
REM
REM Usage:
REM   build_lua_example.bat opengl        [example_name] [--no-run]
REM   build_lua_example.bat vulkan        [example_name]
REM
REM Prerequisites:
REM   build\build_situation.bat opengl      (for opengl)
REM   build\build_situation.bat vulkan      (for vulkan)
REM Bindings: python tools\generate_lua_bindings.py
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

cd /d "%~dp0.."

if "%~1"=="" goto :usage

set BACKEND=%~1
set EXAMPLE_NAME=%~2
set NO_RUN=
if /i "%~2"=="--no-run" (
    set EXAMPLE_NAME=hello_situation
    set NO_RUN=1
) else if /i "%~3"=="--no-run" (
    set NO_RUN=1
)
if "%EXAMPLE_NAME%"=="" set EXAMPLE_NAME=hello_situation

set SIT_LUA_EXAMPLE=%EXAMPLE_NAME%
set SIT_LUA_BACKEND=%BACKEND%

call scripts\wrapper_link_config.bat %BACKEND%
if errorlevel 1 goto :usage

call scripts\wrapper_paths.bat lua %EXAMPLE_NAME% %BACKEND%
if errorlevel 1 exit /b 1

set OUT_DIR=%SIT_WRAPPER_OUT_DIR%
set OUT_EXE=%OUT_DIR%\%EXAMPLE_NAME%.exe

if not exist "%SIT_DLL_SRC%" (
    echo [ERROR] Situation DLL not found: %SIT_DLL_SRC%
    echo         Run: build\build_situation.bat %BACKEND%
    exit /b 1
)

call scripts\wrapper_mingw_setup.bat

echo.
echo [BUILD] Lua %EXAMPLE_NAME% (%BACKEND%)
echo         Output:  %OUT_EXE%
echo.

call scripts\wrapper_compile_lua.bat
if errorlevel 1 goto :failed

call scripts\wrapper_link_lua.bat
if errorlevel 1 goto :failed

if defined NO_RUN (
    echo [SUCCESS] %OUT_EXE%
    exit /b 0
)

"%OUT_EXE%"
set EXIT_CODE=%errorlevel%

echo.
if %EXIT_CODE%==0 (
    echo [SUCCESS] %OUT_EXE%
) else (
    echo [FAILED] Lua %EXAMPLE_NAME% exit code %EXIT_CODE%
)
exit /b %EXIT_CODE%

:failed
echo [FAILED] Lua build failed
exit /b 1

:usage
echo.
echo Usage: build_lua_example.bat [backend] [example_name] [--no-run]
echo.
echo Backends (self-contained embedded exe):
echo   opengl   Embeds build\dll\situation_opengl.dll
echo   vulkan   Embeds build\dll\situation_vulkan.dll
echo.
echo Prerequisites:
echo   build\build_situation.bat opengl   ^(for opengl^)
echo   build\build_situation.bat vulkan   ^(for vulkan^)
echo.
echo Output: build\examples\lua\^<example^>.exe  ^(only file in that folder^)
echo.
echo Examples:
echo   build_lua_example.bat opengl hello_situation
echo   build_lua_example.bat opengl hello_situation --no-run
echo.
exit /b 1