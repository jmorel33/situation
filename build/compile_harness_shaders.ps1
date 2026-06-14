# Precompile harness test shaders to SPIR-V (OpenGL + Vulkan targets).
$ErrorActionPreference = 'Stop'

# Resolve project root (one level up from build/)
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

$glslc = Join-Path $ProjectRoot 'ext\shaderc\build\glslc\glslc.exe'
if (-not (Test-Path -LiteralPath $glslc)) {
    Write-Host '[WARN] glslc not found - harness SPIR-V embed stays stub (zero length).'
    exit 0
}

$shDir = Join-Path $ProjectRoot 'tests\harness\shaders'
$outDir = Join-Path $ProjectRoot 'tests\harness\spirv_out'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$vs = Join-Path $shDir 'harness_passthrough.vs'
$glFlags = @('--target-env=opengl', '-fauto-map-locations', '-fauto-bind-uniforms', '-std=450', '-O')
# Phase 0.1: no -fauto-map-locations — explicit layout(set,binding) must match GLSL (see doc/plan/VULKAN_SPIRV_USER_DESCRIPTOR_PARITY.md).
$vkFlags = @('--target-env=vulkan', '-std=450', '-O')

function Invoke-GlslcCompile([string]$Stage, [string]$Src, [string]$OutSpv, [string[]]$Flags) {
    $args = @("-fshader-stage=$Stage", $Src, '-o', $OutSpv) + $Flags
    & $glslc @args
    if ($LASTEXITCODE -ne 0) { throw "glslc failed: $Src" }
}

Write-Host '[SHADER] Precompiling harness SPIR-V (OpenGL)...'
Invoke-GlslcCompile 'vertex' $vs (Join-Path $outDir 'harness_passthrough.vs.spv') $glFlags
Invoke-GlslcCompile 'fragment' (Join-Path $shDir 'harness_dual_ssbo_gl.fs') (Join-Path $outDir 'harness_dual_ssbo_gl.fs.spv') $glFlags
Invoke-GlslcCompile 'fragment' (Join-Path $shDir 'harness_ubo_ssbo_gl.fs') (Join-Path $outDir 'harness_ubo_ssbo_gl.fs.spv') $glFlags
Invoke-GlslcCompile 'fragment' (Join-Path $shDir 'harness_solid_red_gl.fs') (Join-Path $outDir 'harness_solid_red_gl.fs.spv') $glFlags

Write-Host '[SHADER] Precompiling harness SPIR-V (Vulkan)...'
Invoke-GlslcCompile 'vertex' $vs (Join-Path $outDir 'harness_passthrough_vk.vs.spv') $vkFlags
Invoke-GlslcCompile 'fragment' (Join-Path $shDir 'harness_dual_ssbo_vk.fs') (Join-Path $outDir 'harness_dual_ssbo_vk.fs.spv') $vkFlags
Invoke-GlslcCompile 'fragment' (Join-Path $shDir 'harness_ubo_ssbo_vk.fs') (Join-Path $outDir 'harness_ubo_ssbo_vk.fs.spv') $vkFlags
Invoke-GlslcCompile 'fragment' (Join-Path $shDir 'harness_solid_red_vk.fs') (Join-Path $outDir 'harness_solid_red_vk.fs.spv') $vkFlags

& (Join-Path $ProjectRoot 'scripts\gen_spirv_embed.ps1') `
    -VsSpv (Join-Path $outDir 'harness_passthrough.vs.spv') `
    -FsDualSpv (Join-Path $outDir 'harness_dual_ssbo_gl.fs.spv') `
    -FsUboSpv (Join-Path $outDir 'harness_ubo_ssbo_gl.fs.spv') `
    -OutC (Join-Path $ProjectRoot 'tests\harness\sit_harness_spirv_gl_embed.c') `
    -HeaderInclude 'sit_harness_spirv_embed.h' `
    -Prefix 'sit_harness_gl'

& (Join-Path $ProjectRoot 'scripts\gen_spirv_embed.ps1') `
    -VsSpv (Join-Path $outDir 'harness_passthrough_vk.vs.spv') `
    -FsDualSpv (Join-Path $outDir 'harness_dual_ssbo_vk.fs.spv') `
    -FsUboSpv (Join-Path $outDir 'harness_ubo_ssbo_vk.fs.spv') `
    -OutC (Join-Path $ProjectRoot 'tests\harness\sit_harness_spirv_vk_embed.c') `
    -HeaderInclude 'sit_harness_spirv_embed.h' `
    -Prefix 'sit_harness_vk'

Write-Host '[SHADER] OK: harness SPIR-V embed regenerated.'
exit 0
