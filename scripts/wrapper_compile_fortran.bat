@echo off
REM Compile Situation Fortran bindings and one example.
REM
REM Caller sets (or accepts defaults via build_fortran_example.bat):
REM   SIT_FORTRAN_EXAMPLE   — example folder name (default: hello_situation)
REM   SIT_FORTRAN_BACKEND   — backend token (for per-backend obj dir)
REM
REM Uses wrapper_paths.bat layout under build\obj\fortran\.
REM On success sets:
REM   SIT_FORTRAN_OBJ_ARGS  — object list for linking
REM   SIT_FORTRAN_OBJ_GLOB  — glob for static link via wrapper_gcc_link_static.bat

if not defined SIT_FORTRAN_EXAMPLE set "SIT_FORTRAN_EXAMPLE=hello_situation"
if not defined SIT_FORTRAN_BACKEND (
    echo [ERROR] wrapper_compile_fortran.bat: SIT_FORTRAN_BACKEND not set
    exit /b 1
)

set "SRC_DIR=wrappers\Fortran\src"
set "EXAMPLE_DIR=wrappers\Fortran\examples\%SIT_FORTRAN_EXAMPLE%"
set "EXAMPLE_SRC=%EXAMPLE_DIR%\main.f90"
set "DEMO_HELPERS=%EXAMPLE_DIR%\demo_helpers.f90"

call scripts\wrapper_paths.bat fortran %SIT_FORTRAN_EXAMPLE% %SIT_FORTRAN_BACKEND%
if errorlevel 1 exit /b 1

set "MOD_DIR=%SIT_WRAPPER_OBJ_BASE%\mod"
set "BIND_OBJ_DIR=%SIT_WRAPPER_OBJ_BASE%\bindings"
set "EXAMPLE_OBJ_DIR=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%"

if not exist "%EXAMPLE_SRC%" (
    echo [ERROR] Example not found: %EXAMPLE_SRC%
    exit /b 1
)

where gfortran >nul 2>&1
if errorlevel 1 (
    echo [ERROR] gfortran not found. Install: pacman -S mingw-w64-x86_64-gcc-fortran
    exit /b 1
)

if not exist "%MOD_DIR%" mkdir "%MOD_DIR%"
if not exist "%BIND_OBJ_DIR%" mkdir "%BIND_OBJ_DIR%"
if not exist "%EXAMPLE_OBJ_DIR%" mkdir "%EXAMPLE_OBJ_DIR%"

set "FFLAGS=-Wall -J%MOD_DIR% -I%MOD_DIR%"

gfortran -c %FFLAGS% "%SRC_DIR%\situation_types.f90" -o "%BIND_OBJ_DIR%\situation_types.o"
if errorlevel 1 exit /b 1
gfortran -c %FFLAGS% "%SRC_DIR%\situation_callbacks.f90" -o "%BIND_OBJ_DIR%\situation_callbacks.o"
if errorlevel 1 exit /b 1
gfortran -c %FFLAGS% "%SRC_DIR%\situation_foreign.f90" -o "%BIND_OBJ_DIR%\situation_foreign.o"
if errorlevel 1 exit /b 1
gfortran -c %FFLAGS% "%SRC_DIR%\situation_constants.f90" -o "%BIND_OBJ_DIR%\situation_constants.o"
if errorlevel 1 exit /b 1
gfortran -c %FFLAGS% "%SRC_DIR%\situation_helpers.f90" -o "%BIND_OBJ_DIR%\situation_helpers.o"
if errorlevel 1 exit /b 1
gfortran -c %FFLAGS% "%SRC_DIR%\situation.f90" -o "%BIND_OBJ_DIR%\situation.o"
if errorlevel 1 exit /b 1

if exist "%DEMO_HELPERS%" (
    gfortran -c %FFLAGS% "%DEMO_HELPERS%" -o "%EXAMPLE_OBJ_DIR%\demo_helpers.o"
    if errorlevel 1 exit /b 1
)

gfortran -c %FFLAGS% "%EXAMPLE_SRC%" -o "%EXAMPLE_OBJ_DIR%\main.o"
if errorlevel 1 exit /b 1

set "SIT_FORTRAN_OBJ_ARGS=%EXAMPLE_OBJ_DIR%\main.o"
if exist "%EXAMPLE_OBJ_DIR%\demo_helpers.o" (
    set "SIT_FORTRAN_OBJ_ARGS=%EXAMPLE_OBJ_DIR%\demo_helpers.o %SIT_FORTRAN_OBJ_ARGS%"
)
set "SIT_FORTRAN_OBJ_ARGS=%SIT_FORTRAN_OBJ_ARGS% %BIND_OBJ_DIR%\situation.o %BIND_OBJ_DIR%\situation_helpers.o %BIND_OBJ_DIR%\situation_constants.o %BIND_OBJ_DIR%\situation_foreign.o %BIND_OBJ_DIR%\situation_callbacks.o %BIND_OBJ_DIR%\situation_types.o"

set "SIT_FORTRAN_OBJ_GLOB=%BIND_OBJ_DIR%\*.o %EXAMPLE_OBJ_DIR%\*.o"

exit /b 0