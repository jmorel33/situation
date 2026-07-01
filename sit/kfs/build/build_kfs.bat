@echo off
setlocal
cd /d "%~dp0..\kfs"

where mingw32-make >nul 2>&1
if %ERRORLEVEL%==0 (
    mingw32-make build
) else (
    make build
)
exit /b %ERRORLEVEL%