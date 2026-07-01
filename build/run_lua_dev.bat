@echo off
cd /d "%~dp0.."
set SIT_LUA_BACKEND=opengl
set "SIT_LUA_DLL_PATH=%CD%\build\dll\situation_opengl.dll"
if not exist "%SIT_LUA_DLL_PATH%" (
    echo [ERROR] DLL not found: %SIT_LUA_DLL_PATH%
    echo         Run: build\build_situation.bat opengl
    exit /b 1
)
set "PATH=%CD%\build\dll;%PATH%"
_languages\lua\luajit\bin\luajit.exe -e "package.path='wrappers/lua/?.lua;wrappers/lua/?/init.lua;'..package.path" wrappers\lua\examples\hello_situation.lua