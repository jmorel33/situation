@echo off
REM Verify KaOS Terminal (kterm_console) build — see doc/plan/CONSOLE_MERGE_DEPRECATION_PLAN.md
setlocal
cd /d "%~dp0.."

echo === Build (OpenGL) ===
call build_examples.bat opengl kterm_console
set BUILD_RC=%ERRORLEVEL%
echo Build exit code: %BUILD_RC%

if not exist "build\examples\kterm_console.exe" (
    echo [FAIL] build\examples\kterm_console.exe not found
    exit /b 1
)
echo [OK] build\examples\kterm_console.exe exists

echo.
echo === Optional: headless screenshot capture ===
echo Set KTERM_CAPTURE_SCREENSHOT=build\examples\kterm_console_shot.png
echo Set KTERM_CAPTURE_EXIT=1
echo then run build\examples\kterm_console.exe manually for visual/smoke test.

exit /b %BUILD_RC%
