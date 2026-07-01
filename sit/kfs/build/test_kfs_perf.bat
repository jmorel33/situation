@echo off
setlocal
cd /d "%~dp0..\tests"

where mingw32-make >nul 2>&1
if %ERRORLEVEL%==0 (
    mingw32-make test-perf
) else (
    make test-perf
)
exit /b %ERRORLEVEL%