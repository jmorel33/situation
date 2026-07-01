@echo off
REM Link wrapper .o files against a Situation DLL import library.
REM
REM Requires (from wrapper_link_config.bat):
REM   SIT_DLL_BASENAME, SIT_DLL_SRC, SIT_IMPORT_LIB
REM Caller must set:
REM   OUT_EXE, OBJ_ARGS (space-separated object files)
REM   OUT_DIR (folder that receives the copied DLL)
REM Optional:
REM   SIT_LINK_DRIVER   — gcc (default), g++, gfortran
REM   SIT_LINK_FLAGS    — extra flags after -o OUT_EXE (e.g. -static-libgfortran)
REM   SIT_EXTRA_LDFLAGS — prepended flags (e.g. gm2 runtime -L... -lgm2)

setlocal enabledelayedexpansion

if not defined OUT_EXE (
    echo [ERROR] wrapper_link_dll.bat: OUT_EXE not set
    exit /b 1
)
if not defined OBJ_ARGS (
    echo [ERROR] wrapper_link_dll.bat: OBJ_ARGS not set
    exit /b 1
)
if not defined OUT_DIR (
    echo [ERROR] wrapper_link_dll.bat: OUT_DIR not set
    exit /b 1
)

set "DRIVER=%SIT_LINK_DRIVER%"
if not defined DRIVER set "DRIVER=gcc"

set "EXTRA_PRE="
if defined SIT_EXTRA_LDFLAGS set "EXTRA_PRE=%SIT_EXTRA_LDFLAGS%"

set "EXTRA_POST="
if defined SIT_LINK_FLAGS set "EXTRA_POST=%SIT_LINK_FLAGS%"

%DRIVER% %OBJ_ARGS% -o "%OUT_EXE%" %EXTRA_PRE% -Lbuild\dll -l:%SIT_DLL_BASENAME%.lib %EXTRA_POST%
if errorlevel 1 exit /b 1

if not exist "%SIT_DLL_SRC%" (
    echo [ERROR] Situation DLL not found: %SIT_DLL_SRC%
    exit /b 1
)

copy /y "%SIT_DLL_SRC%" "%OUT_DIR%\" >nul
exit /b 0