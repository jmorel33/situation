@echo off

REM Regenerate API docs index and language bindings from sit/situation_api.h

cd /d "%~dp0.."

python tools\generate_api_index.py

if errorlevel 1 exit /b 1

python tools\generate_odin_bindings.py

if errorlevel 1 exit /b 1

python tools\generate_odin_bindings.py --jam

if errorlevel 1 exit /b 1

python tools\generate_zig_bindings.py

if errorlevel 1 exit /b 1

python tools\generate_zig_bindings.py --jam

if errorlevel 1 exit /b 1

python tools\generate_rust_bindings.py

if errorlevel 1 exit /b 1

python tools\generate_rust_bindings.py --jam

if errorlevel 1 exit /b 1

echo Done.

