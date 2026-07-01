@echo off
REM Stage Lua sources and generate embedded bytecode for the launcher exe.
REM
REM On success sets:
REM   SIT_LUA_STAGE_DIR
REM   SIT_LUA_EMBED_DIR  — build\obj\lua\<example>_<backend>\embed

if not defined SIT_LUA_EXAMPLE set "SIT_LUA_EXAMPLE=hello_situation"
if not defined SIT_LUA_BACKEND (
    echo [ERROR] wrapper_compile_lua.bat: SIT_LUA_BACKEND not set
    exit /b 1
)
if not defined SIT_WRAPPER_EXAMPLE_OBJ_DIR (
    echo [ERROR] wrapper_compile_lua.bat: call wrapper_paths.bat first
    exit /b 1
)

set "STAGE=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%\stage"
set "EMBED_DIR=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%\embed"
set "SCRIPT_SRC=wrappers\lua\examples\%SIT_LUA_EXAMPLE%.lua"
set "PKG_SRC=wrappers\lua\situation"

if not exist "%SCRIPT_SRC%" (
    echo [ERROR] Example not found: %SCRIPT_SRC%
    exit /b 1
)
if not exist "%PKG_SRC%\init.lua" (
    echo [ERROR] Package not found: %PKG_SRC%
    echo         Run: python tools\generate_lua_bindings.py
    exit /b 1
)

where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] python not found on PATH
    exit /b 1
)

if exist "%STAGE%" rmdir /S /Q "%STAGE%"
mkdir "%STAGE%"
mkdir "%STAGE%\situation" 2>nul

copy /Y "%SCRIPT_SRC%" "%STAGE%\%SIT_LUA_EXAMPLE%.lua" >nul
if errorlevel 1 exit /b 1

xcopy /E /I /Y /Q "%PKG_SRC%" "%STAGE%\situation\" >nul
if errorlevel 1 exit /b 1

python tools\gen_lua_embed.py "%STAGE%" "%EMBED_DIR%"
if errorlevel 1 exit /b 1

set "DLL_EMBED_DIR=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%\dll_embed"
if not defined SIT_DLL_SRC (
    echo [ERROR] wrapper_compile_lua.bat: SIT_DLL_SRC not set
    echo         Call wrapper_link_config.bat before compile
    exit /b 1
)

REM Lua embed: prefer SIT_LUA_EMBED_DLL override, then known-good backup if present.
REM Fresh build\dll\situation_opengl.dll may hit render-thread GL -600; Jun 25 backup works.
set "LUA_EMBED_DLL=%SIT_DLL_SRC%"
if defined SIT_LUA_EMBED_DLL (
    set "LUA_EMBED_DLL=%SIT_LUA_EMBED_DLL%"
) else if exist "build\dll\situation_opengl_lua_embed.dll" (
    if /i "%SIT_LUA_BACKEND%"=="opengl" (
        set "LUA_EMBED_DLL=build\dll\situation_opengl_lua_embed.dll"
        echo [WARN] Using build\dll\situation_opengl_lua_embed.dll for Lua embed ^(override: set SIT_LUA_EMBED_DLL^)
    )
) else if exist "build\dll\situation_opengl_new.dll.bak" (
    if /i "%SIT_LUA_BACKEND%"=="opengl" (
        set "LUA_EMBED_DLL=build\dll\situation_opengl_new.dll.bak"
        echo [WARN] Using build\dll\situation_opengl_new.dll.bak for Lua embed ^(override: set SIT_LUA_EMBED_DLL^)
    )
)

if not exist "%LUA_EMBED_DLL%" (
    echo [ERROR] Situation DLL not found: %LUA_EMBED_DLL%
    echo         Run: build\build_situation.bat %SIT_LUA_BACKEND%
    exit /b 1
)
set "LUAJIT_DLL=_languages\lua\luajit\bin\lua51.dll"
if not exist "%LUAJIT_DLL%" (
    echo [ERROR] LuaJIT runtime DLL not found: %LUAJIT_DLL%
    echo         Run: _languages\lua\populate_toolchain.bat install
    exit /b 1
)
python tools\gen_lua_dll_embed.py "%DLL_EMBED_DIR%" "%LUA_EMBED_DLL%" "%LUAJIT_DLL%"
if errorlevel 1 exit /b 1

set "SIT_LUA_STAGE_DIR=%STAGE%"
set "SIT_LUA_EMBED_DIR=%EMBED_DIR%"
set "SIT_LUA_DLL_EMBED_DIR=%DLL_EMBED_DIR%"

echo [COMPILE] Lua %SIT_LUA_EXAMPLE% (%SIT_LUA_BACKEND%)
echo           embed: %EMBED_DIR%
exit /b 0