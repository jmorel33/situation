@echo off
REM Compile Situation Modula-2 bindings + one example.
REM
REM Requires:
REM   GM2_EXE, GM2_LINK  (set by build_modula2_example.bat)
REM   SIT_M2_EXAMPLE, SIT_M2_BACKEND
REM
REM On success sets SIT_M2_OBJ_ARGS for linking scripts.

setlocal enabledelayedexpansion

if not defined GM2_EXE (
    echo [ERROR] wrapper_compile_modula2.bat: GM2_EXE not set
    exit /b 1
)
if not defined SIT_M2_EXAMPLE set "SIT_M2_EXAMPLE=hello_situation"
if not defined SIT_M2_BACKEND (
    echo [ERROR] wrapper_compile_modula2.bat: SIT_M2_BACKEND not set
    exit /b 1
)

set "M2_SRC=wrappers\Modula2\src"
set "EXAMPLE_DIR=wrappers\Modula2\examples\%SIT_M2_EXAMPLE%"

call scripts\wrapper_paths.bat modula2 %SIT_M2_EXAMPLE% %SIT_M2_BACKEND%
if errorlevel 1 exit /b 1

set "OBJ_DIR=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%"

if not exist "%EXAMPLE_DIR%\Main.mod" (
    echo [ERROR] Example not found: %EXAMPLE_DIR%\Main.mod
    exit /b 1
)

if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

call scripts\wrapper_mingw_setup.bat

REM GNU Modula-2 needs cc1gm2 on -B path and standard-library .def search dirs.
if not defined GM2_CC1_DIR (
    for %%i in ("%GM2_EXE%") do set "GM2_BIN=%%~dpi"
    if exist "%GM2_BIN%..\libexec\gcc\x86_64-w64-mingw32\15.1.0\cc1gm2.exe" (
        set "GM2_CC1_DIR=%GM2_BIN%..\libexec\gcc\x86_64-w64-mingw32\15.1.0"
    ) else if exist "_languages\gm2\libexec\gcc\x86_64-w64-mingw32\15.1.0\cc1gm2.exe" (
        set "GM2_CC1_DIR=_languages\gm2\libexec\gcc\x86_64-w64-mingw32\15.1.0"
    )
)

if not defined GM2_LIBS_BUILD set "GM2_LIBS_BUILD=_languages\gm2-build\build\gcc\m2\gm2-libs"
if not defined GM2_LIBS_SRC set "GM2_LIBS_SRC=_languages\gm2-build\gcc-15.1.0\gcc\m2\gm2-libs"
if not defined GM2_LIBS_ISO set "GM2_LIBS_ISO=_languages\gm2-build\gcc-15.1.0\gcc\m2\gm2-libs-iso"
set "GM2_FLAGS=-fpim"
if defined GM2_CC1_DIR set "GM2_FLAGS=%GM2_FLAGS% -B %GM2_CC1_DIR%"
if exist "%GM2_LIBS_BUILD%" set "GM2_FLAGS=%GM2_FLAGS% -I %GM2_LIBS_BUILD%"
if exist "%GM2_LIBS_SRC%" set "GM2_FLAGS=%GM2_FLAGS% -I %GM2_LIBS_SRC%"
if exist "%GM2_LIBS_ISO%" set "GM2_FLAGS=%GM2_FLAGS% -I %GM2_LIBS_ISO%"

"%GM2_EXE%" %GM2_FLAGS% -c -I"%M2_SRC%" "%M2_SRC%\SituationTypes.mod" -o "%OBJ_DIR%\SituationTypes.o"
if errorlevel 1 exit /b 1

"%GM2_EXE%" %GM2_FLAGS% -c -I"%M2_SRC%" "%M2_SRC%\SituationConstants.mod" -o "%OBJ_DIR%\SituationConstants.o"
if errorlevel 1 exit /b 1

"%GM2_EXE%" %GM2_FLAGS% -c -I"%M2_SRC%" "%M2_SRC%\SituationHelpers.mod" -o "%OBJ_DIR%\SituationHelpers.o"
if errorlevel 1 exit /b 1

"%GM2_EXE%" %GM2_FLAGS% -c -fscaffold-main -I"%M2_SRC%" "%EXAMPLE_DIR%\Main.mod" -o "%OBJ_DIR%\Main.o"
if errorlevel 1 exit /b 1

set "M2_GLUE_SRC=wrappers\Modula2\glue\situation_m2_glue.c"

gcc -c "%M2_GLUE_SRC%" -o "%OBJ_DIR%\situation_m2_glue.o"
if errorlevel 1 exit /b 1

set "SIT_M2_OBJ_ARGS=%OBJ_DIR%\Main.o %OBJ_DIR%\SituationHelpers.o %OBJ_DIR%\SituationTypes.o %OBJ_DIR%\SituationConstants.o %OBJ_DIR%\situation_m2_glue.o"

endlocal & set "SIT_M2_OBJ_ARGS=%SIT_M2_OBJ_ARGS%"
exit /b 0