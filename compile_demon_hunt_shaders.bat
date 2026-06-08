@echo off
REM Precompile Demon Hunt skydome GLSL to SPIR-V at build time (avoids ~20s runtime compile).
setlocal

set "GLSLC=ext\shaderc\build\glslc\glslc.exe"
set "SRC_DIR=examples\demon_hunt"
set "VS=%SRC_DIR%\demon_hunt_sky.vs"
set "FS=%SRC_DIR%\demon_hunt_sky.fs"
set "VS_SPV=%SRC_DIR%\demon_hunt_sky.vs.spv"
set "FS_SPV=%SRC_DIR%\demon_hunt_sky.fs.spv"
set "VS_SPV_VK=%SRC_DIR%\demon_hunt_sky.vk.vs.spv"
set "FS_SPV_VK=%SRC_DIR%\demon_hunt_sky.vk.fs.spv"
set "OUT_DIR=build\examples"
set "GLSLC_FLAGS_GL=--target-env=opengl -fauto-map-locations -fauto-bind-uniforms -std=450 -O"
set "GLSLC_FLAGS_VK=--target-env=vulkan -std=450 -O"

if not exist "%GLSLC%" (
    echo [WARN] glslc not found at %GLSLC% — skip SPIR-V precompile.
    echo        Build shaderc in ext\shaderc or run from a machine with glslc on PATH.
    exit /b 0
)

if not exist "%VS%" (
    echo [ERROR] Missing %VS%
    exit /b 1
)
if not exist "%FS%" (
    echo [ERROR] Missing %FS%
    exit /b 1
)

echo [SHADER] Precompiling Demon Hunt skydome to SPIR-V...

echo [SHADER] OpenGL-target SPIR-V...
"%GLSLC%" -fshader-stage=vertex "%VS%" -o "%VS_SPV%" %GLSLC_FLAGS_GL%
if errorlevel 1 (
    echo [FAILED] OpenGL vertex SPIR-V compile failed.
    exit /b 1
)

"%GLSLC%" -fshader-stage=fragment "%FS%" -o "%FS_SPV%" %GLSLC_FLAGS_GL%
if errorlevel 1 (
    echo [FAILED] OpenGL fragment SPIR-V compile failed.
    exit /b 1
)

echo [SHADER] Vulkan-target SPIR-V...
"%GLSLC%" -fshader-stage=vertex "%VS%" -o "%VS_SPV_VK%" %GLSLC_FLAGS_VK%
if errorlevel 1 (
    echo [FAILED] Vulkan vertex SPIR-V compile failed.
    exit /b 1
)

REM --target-env=vulkan defines VULKAN; do not pass -DVULKAN (macro redefinition).
"%GLSLC%" -fshader-stage=fragment "%FS%" -o "%FS_SPV_VK%" %GLSLC_FLAGS_VK%
if errorlevel 1 (
    echo [FAILED] Vulkan fragment SPIR-V compile failed.
    exit /b 1
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
copy /Y "%VS_SPV%" "%OUT_DIR%\" >nul
copy /Y "%FS_SPV%" "%OUT_DIR%\" >nul
copy /Y "%VS_SPV_VK%" "%OUT_DIR%\" >nul
copy /Y "%FS_SPV_VK%" "%OUT_DIR%\" >nul
copy /Y "%VS%" "%OUT_DIR%\" >nul
copy /Y "%FS%" "%OUT_DIR%\" >nul

REM Devel FS (no -O): per-function SPIR-V for GPU link tests; production -O FS can hit driver insn limits.
set "FS_DEVEL=build\examples\demon_hunt_sky.fs.devel.spv"
set "GLSLC_DEVEL_FLAGS=--target-env=opengl -fauto-map-locations -fauto-bind-uniforms -std=450"
"%GLSLC%" -fshader-stage=fragment "%FS%" -o "%FS_DEVEL%" %GLSLC_DEVEL_FLAGS%
if errorlevel 1 (
    echo [WARN] Devel fragment SPIR-V compile failed — demon_hunt_sky_spirv_begin_poll may skip.
) else (
    copy /Y "%FS_DEVEL%" "%SRC_DIR%\demon_hunt_sky.fs.devel.spv" >nul
    copy /Y "%FS_DEVEL%" "%OUT_DIR%\demon_hunt_sky.fs.devel.spv" >nul
    echo [SHADER] OK: %FS_DEVEL% (devel, no -O)
)

python scripts\spirv_shader_debug.py demon_hunt --devel
if errorlevel 1 (
    echo [WARN] spirv_shader_debug.py reported issues — see output above.
)

echo [SHADER] OK: %VS_SPV% (OpenGL)
echo [SHADER] OK: %FS_SPV% (OpenGL)
echo [SHADER] OK: %VS_SPV_VK% (Vulkan)
echo [SHADER] OK: %FS_SPV_VK% (Vulkan)
echo [SHADER] Copied to %OUT_DIR%\

REM Sync compiled SPV files to test harness assets — tests load from here independently of example folder layout.
set "ASSETS_DIR=tests\harness\assets"
if exist "%ASSETS_DIR%" (
    copy /Y "%VS_SPV%"     "%ASSETS_DIR%\" >nul
    copy /Y "%FS_SPV%"     "%ASSETS_DIR%\" >nul
    copy /Y "%VS_SPV_VK%"  "%ASSETS_DIR%\" >nul
    copy /Y "%FS_SPV_VK%"  "%ASSETS_DIR%\" >nul
    copy /Y "%VS%"         "%ASSETS_DIR%\" >nul
    copy /Y "%FS%"         "%ASSETS_DIR%\" >nul
    if exist "%FS_DEVEL%" copy /Y "%FS_DEVEL%" "%ASSETS_DIR%\" >nul
    echo [SHADER] Synced to %ASSETS_DIR%\
)

powershell -NoProfile -ExecutionPolicy Bypass -File "scripts\gen_demon_hunt_spirv_embed.ps1" -VsSpvGl "%VS_SPV%" -FsSpvGl "%FS_SPV%" -VsSpvVk "%VS_SPV_VK%" -FsSpvVk "%FS_SPV_VK%" -OutC "%SRC_DIR%\demon_hunt_sky_spirv_embed.c"
if errorlevel 1 (
    echo [WARN] Could not regenerate %SRC_DIR%\demon_hunt_sky_spirv_embed.c - embed may be stale or stub.
)
exit /b 0
