@echo off
REM Stage a Python Situation wrapper example for linking (PyInstaller).
REM
REM Caller sets (via build_python_example.bat):
REM   SIT_PYTHON_EXAMPLE  — script basename without .py (default: hello_situation)
REM   SIT_PYTHON_BACKEND  — opengl | vulkan
REM
REM Requires wrapper_link_config.bat + wrapper_paths.bat (sets SIT_WRAPPER_EXAMPLE_OBJ_DIR).
REM
REM On success sets:
REM   SIT_PYTHON_STAGE_DIR  — build\obj\python\<example>_<backend>\stage

if not defined SIT_PYTHON_EXAMPLE set "SIT_PYTHON_EXAMPLE=hello_situation"
if not defined SIT_PYTHON_BACKEND (
    echo [ERROR] wrapper_compile_python.bat: SIT_PYTHON_BACKEND not set
    exit /b 1
)
if not defined SIT_WRAPPER_EXAMPLE_OBJ_DIR (
    echo [ERROR] wrapper_compile_python.bat: call wrapper_paths.bat first
    exit /b 1
)

set "STAGE=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%\stage"
set "SCRIPT_SRC=wrappers\Python\examples\%SIT_PYTHON_EXAMPLE%.py"
set "PKG_SRC=wrappers\Python\situation"

if not exist "%SCRIPT_SRC%" (
    echo [ERROR] Example not found: %SCRIPT_SRC%
    exit /b 1
)
if not exist "%PKG_SRC%" (
    echo [ERROR] Package not found: %PKG_SRC%
    echo         Run: python tools\generate_python_bindings.py
    exit /b 1
)

where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] python not found on PATH. Install Python 3.10+.
    exit /b 1
)

if exist "%STAGE%" rmdir /S /Q "%STAGE%"
mkdir "%STAGE%"
mkdir "%STAGE%\situation" 2>nul

copy /Y "%SCRIPT_SRC%" "%STAGE%\%SIT_PYTHON_EXAMPLE%.py" >nul
if errorlevel 1 exit /b 1

xcopy /E /I /Y /Q "%PKG_SRC%" "%STAGE%\situation\" >nul
if errorlevel 1 exit /b 1

pushd "%STAGE%"
python -m compileall -q situation "%SIT_PYTHON_EXAMPLE%.py"
set COMPILE_EXIT=%errorlevel%
popd
if not %COMPILE_EXIT%==0 (
    echo [ERROR] python -m compileall failed
    exit /b 1
)

set "SIT_PYTHON_STAGE_DIR=%STAGE%"

echo [COMPILE] Python %SIT_PYTHON_EXAMPLE% (%SIT_PYTHON_BACKEND%)
echo           stage: %STAGE%
exit /b 0
