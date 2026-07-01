@echo off
REM Link a staged Python wrapper into build\examples\python\<example>.exe (PyInstaller).
REM
REM Requires wrapper_compile_python.bat (SIT_PYTHON_STAGE_DIR) and wrapper_link_config.bat
REM (SIT_DLL_SRC, SIT_NEEDS_DLL_COPY, SIT_DLL_BASENAME).
REM
REM Toolchain: pip install pyinstaller  (python -m PyInstaller)
REM
REM On success sets:
REM   SIT_PYTHON_OUT_EXE — build\examples\python\<example>.exe

if not defined SIT_PYTHON_EXAMPLE set "SIT_PYTHON_EXAMPLE=hello_situation"
if not defined SIT_PYTHON_STAGE_DIR (
    echo [ERROR] wrapper_link_python.bat: run wrapper_compile_python.bat first
    exit /b 1
)
if not defined SIT_WRAPPER_OUT_DIR (
    echo [ERROR] wrapper_link_python.bat: SIT_WRAPPER_OUT_DIR not set
    exit /b 1
)

set "OUT_DIR=%SIT_WRAPPER_OUT_DIR%"
set "OUT_EXE=%OUT_DIR%\%SIT_PYTHON_EXAMPLE%.exe"
set "WORK=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%\work"
set "SPEC_DIR=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%"

where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] python not found on PATH.
    exit /b 1
)

python -m PyInstaller --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] PyInstaller not found. Install with:
    echo         pip install pyinstaller
    exit /b 1
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%WORK%" mkdir "%WORK%"

REM Remove prior output (exe and legacy staged .py tree)
if exist "%OUT_EXE%" del /F /Q "%OUT_EXE%"
if exist "%OUT_DIR%\%SIT_PYTHON_EXAMPLE%.py" del /F /Q "%OUT_DIR%\%SIT_PYTHON_EXAMPLE%.py"
if exist "%OUT_DIR%\situation" rmdir /S /Q "%OUT_DIR%\situation"
if exist "%OUT_DIR%\__pycache__" rmdir /S /Q "%OUT_DIR%\__pycache__"

python -m PyInstaller --noconfirm --clean --onefile ^
  --name "%SIT_PYTHON_EXAMPLE%" ^
  --distpath "%OUT_DIR%" ^
  --workpath "%WORK%" ^
  --specpath "%SPEC_DIR%" ^
  --paths "%SIT_PYTHON_STAGE_DIR%" ^
  --collect-submodules situation ^
  "%SIT_PYTHON_STAGE_DIR%\%SIT_PYTHON_EXAMPLE%.py"
if errorlevel 1 (
    echo [ERROR] PyInstaller failed
    exit /b 1
)

if not exist "%OUT_EXE%" (
    echo [ERROR] Expected output not found: %OUT_EXE%
    exit /b 1
)

if "%SIT_NEEDS_DLL_COPY%"=="1" (
    if not exist "%SIT_DLL_SRC%" (
        echo [ERROR] Situation DLL not found: %SIT_DLL_SRC%
        exit /b 1
    )
    copy /Y "%SIT_DLL_SRC%" "%OUT_DIR%\" >nul
    if errorlevel 1 exit /b 1
)

set "SIT_PYTHON_OUT_EXE=%OUT_EXE%"

echo [LINK] Python %SIT_PYTHON_EXAMPLE%.exe (%SIT_PYTHON_BACKEND%)
echo        %OUT_EXE%
if "%SIT_NEEDS_DLL_COPY%"=="1" (
    echo        %OUT_DIR%\%SIT_DLL_BASENAME%.dll
)
exit /b 0
