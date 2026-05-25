@echo off
REM Precompile harness test shaders to SPIR-V (OpenGL + Vulkan targets).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0compile_harness_shaders.ps1"
exit /b %ERRORLEVEL%
