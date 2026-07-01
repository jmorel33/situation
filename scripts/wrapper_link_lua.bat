@echo off
REM Link embedded LuaJIT host into a single self-contained .exe.
REM Situation + lua51.dll ship as embedded blobs extracted to %%TEMP%% at runtime.
REM
REM Output: build\examples\lua\<example>.exe only (no loose files beside it).

if not defined SIT_LUA_EXAMPLE set "SIT_LUA_EXAMPLE=hello_situation"
if not defined SIT_LUA_EMBED_DIR (
    echo [ERROR] wrapper_link_lua.bat: run wrapper_compile_lua.bat first
    exit /b 1
)
if not defined SIT_LUA_DLL_EMBED_DIR (
    echo [ERROR] wrapper_link_lua.bat: SIT_LUA_DLL_EMBED_DIR not set
    exit /b 1
)
if not defined SIT_WRAPPER_OUT_DIR (
    echo [ERROR] wrapper_link_lua.bat: SIT_WRAPPER_OUT_DIR not set
    exit /b 1
)
if not defined SIT_WRAPPER_EXAMPLE_OBJ_DIR (
    echo [ERROR] wrapper_link_lua.bat: SIT_WRAPPER_EXAMPLE_OBJ_DIR not set
    exit /b 1
)

set "OUT_DIR=%SIT_WRAPPER_OUT_DIR%"
set "OUT_EXE=%OUT_DIR%\%SIT_LUA_EXAMPLE%.exe"
set "OBJ_DIR=%SIT_WRAPPER_EXAMPLE_OBJ_DIR%"
set "HOST_SRC=wrappers\lua\launcher\sit_lua_host.c"
set "RUNTIME_SRC=wrappers\lua\launcher\sit_lua_runtime.c"
set "DRAW_SHIM_SRC=wrappers\lua\launcher\sit_lua_draw_shim.c"
set "LUAJIT_INC=_languages\lua\luajit\include\luajit-2.1"

if not exist "%HOST_SRC%" (
    echo [ERROR] Launcher not found: %HOST_SRC%
    exit /b 1
)

call scripts\wrapper_mingw_setup.bat
where gcc >nul 2>&1
if errorlevel 1 (
    echo [ERROR] gcc not found. Add MSYS2 mingw64\bin to PATH or set MINGW_PATH.
    exit /b 1
)

if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

set "BACKEND_DEF=%SIT_LUA_BACKEND%"
if /i "%BACKEND_DEF%"=="static-opengl" set "BACKEND_DEF=opengl"
if /i "%BACKEND_DEF%"=="static-vulkan" set "BACKEND_DEF=vulkan"

gcc -c "%HOST_SRC%" -o "%OBJ_DIR%\sit_lua_host.o" -std=c11 -O2 ^
    -I"%LUAJIT_INC%" -I"wrappers\lua\launcher" -I"%SIT_LUA_EMBED_DIR%" -I"%SIT_LUA_DLL_EMBED_DIR%" ^
    -DSIT_LUA_BACKEND=\"%BACKEND_DEF%\"
if errorlevel 1 exit /b 1

gcc -c "%RUNTIME_SRC%" -o "%OBJ_DIR%\sit_lua_runtime.o" -std=c11 -O2 ^
    -I"%LUAJIT_INC%" -I"wrappers\lua\launcher"
if errorlevel 1 exit /b 1

gcc -c "%DRAW_SHIM_SRC%" -o "%OBJ_DIR%\sit_lua_draw_shim.o" -std=c11 -O2 ^
    -I"wrappers\lua\launcher"
if errorlevel 1 exit /b 1

gcc -c "%SIT_LUA_EMBED_DIR%\sit_lua_embed.c" -o "%OBJ_DIR%\sit_lua_embed.o" -std=c11 -O2 ^
    -I"%LUAJIT_INC%" -I"wrappers\lua\launcher" -I"%SIT_LUA_EMBED_DIR%"
if errorlevel 1 exit /b 1

gcc -c "%SIT_LUA_DLL_EMBED_DIR%\sit_lua_dll_embed.c" -o "%OBJ_DIR%\sit_lua_dll_embed.o" -std=c11 -O2 ^
    -I"%SIT_LUA_DLL_EMBED_DIR%"
if errorlevel 1 exit /b 1

REM Clean output folder — deliver only the .exe
if exist "%OUT_DIR%" rmdir /S /Q "%OUT_DIR%"
mkdir "%OUT_DIR%"

set "OUT_EXE=%OUT_DIR%\%SIT_LUA_EXAMPLE%.exe"
g++ "%OBJ_DIR%\sit_lua_host.o" "%OBJ_DIR%\sit_lua_runtime.o" "%OBJ_DIR%\sit_lua_draw_shim.o" "%OBJ_DIR%\sit_lua_embed.o" "%OBJ_DIR%\sit_lua_dll_embed.o" ^
    -o "%OUT_EXE%" -static-libgcc -static-libstdc++ -Wl,--export-all-symbols ^
    -lwinmm -luser32 -lshell32 -lole32 -lws2_32
if errorlevel 1 exit /b 1

if not exist "%OUT_EXE%" (
    echo [ERROR] Expected output not found: %OUT_EXE%
    exit /b 1
)

set "SIT_LUA_OUT_EXE=%OUT_EXE%"

echo [LINK] Lua %SIT_LUA_EXAMPLE%.exe (%SIT_LUA_BACKEND%)
echo        %OUT_EXE%
exit /b 0