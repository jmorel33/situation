@echo off
REM Add MinGW-w64 bin directory to PATH for wrapper builds.
REM Optional: set MINGW_PATH before calling to override the default MSYS2 location.

if defined MINGW_PATH (
    set "PATH=%MINGW_PATH%;%PATH%"
    exit /b 0
)

if exist "C:\msys64\mingw64\bin\gcc.exe" (
    set "PATH=C:\msys64\mingw64\bin;%PATH%"
)

exit /b 0