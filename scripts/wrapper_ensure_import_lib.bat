@echo off
REM Ensure MinGW import library exists for DLL-linked wrapper builds.
REM Requires: SIT_DLL_SRC, SIT_IMPORT_LIB, SIT_DLL_BASENAME

if not exist "%SIT_DLL_SRC%" (
    echo [ERROR] Situation DLL not found: %SIT_DLL_SRC%
    echo         Run: build_situation.bat %SIT_LINK_BACKEND%
    exit /b 1
)

if exist "%SIT_IMPORT_LIB%" exit /b 0

echo Generating import library from DLL...
where gendef >nul 2>&1
if errorlevel 1 (
    echo [ERROR] gendef not found. Add MSYS2 mingw64\bin to PATH.
    exit /b 1
)
where dlltool >nul 2>&1
if errorlevel 1 (
    echo [ERROR] dlltool not found. Add MSYS2 mingw64\bin to PATH.
    exit /b 1
)

pushd build\dll
gendef "%SIT_DLL_BASENAME%.dll"
if errorlevel 1 ( popd & exit /b 1 )
dlltool -d %SIT_DLL_BASENAME%.def -l %SIT_DLL_BASENAME%.lib -D %SIT_DLL_BASENAME%.dll
if errorlevel 1 ( popd & exit /b 1 )
popd

if not exist "%SIT_IMPORT_LIB%" (
    echo [ERROR] Failed to generate %SIT_IMPORT_LIB%
    exit /b 1
)
echo   Created: %SIT_IMPORT_LIB%
exit /b 0
