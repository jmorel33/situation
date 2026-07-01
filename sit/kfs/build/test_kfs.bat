@echo off
setlocal
cd /d "%~dp0..\tests"

where mingw32-make >nul 2>&1
if %ERRORLEVEL%==0 (
    mingw32-make test
) else (
    make test
)
REM Runs correctness (35) + perf (11). Fast-only: mingw32-make test-correctness
exit /b %ERRORLEVEL%