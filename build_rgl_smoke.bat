@echo off
REM build_rgl_smoke.bat — compile examples/rgl_smoke_test.c (OpenGL)
REM Usage: build_rgl_smoke.bat

setlocal enabledelayedexpansion

set BUILD_DIR=build\examples
set GLFW_LIB=ext\glfw\build\src

if defined MINGW_PATH (
    set "PATH=%MINGW_PATH%;%PATH%"
) else (
    if exist "C:\msys64\mingw64\bin\gcc.exe" (
        set "PATH=C:\msys64\mingw64\bin;%PATH%"
    ) else (
        echo [ERROR] MinGW not found. Set MINGW_PATH or install MSYS2.
        exit /b 1
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo [BUILD] rgl_smoke_test (OpenGL)
echo.

gcc examples/rgl_smoke_test.c ext/glfw/deps/tinycthread.c ^
    -o %BUILD_DIR%/rgl_smoke_test.exe ^
    -std=c11 -O2 ^
    -msse -msse2 -msse4.1 ^
    -I. -Isit -Iexamples -Iext -Iext/cglm/include -Iext/glfw/include -Iext/glfw/deps -Isit/k-term ^
    -DSITUATION_ENABLE_THREADING ^
    -L%GLFW_LIB% ^
    -static-libgcc ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lshlwapi -luuid -lxinput -lws2_32 -lm

if errorlevel 1 (
    echo [FAILED] Compilation failed!
    exit /b 1
)

echo [SUCCESS] %BUILD_DIR%\rgl_smoke_test.exe
exit /b 0
