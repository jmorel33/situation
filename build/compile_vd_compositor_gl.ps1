# Precompile VD compositor stages to SPIR-V (OpenGL target) for situation_opengl.dll embed.

$ErrorActionPreference = 'Stop'



$ProjectRoot = Split-Path -Parent $PSScriptRoot

Set-Location $ProjectRoot



$glslc = Join-Path $ProjectRoot 'ext\shaderc\build\glslc\glslc.exe'

$outDir = Join-Path $ProjectRoot 'build\spirv_out'

$embedDir = Join-Path $ProjectRoot 'build\opengl'

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

New-Item -ItemType Directory -Force -Path $embedDir | Out-Null



$includeRoot = $ProjectRoot

$glFlags = @(

    '--target-env=opengl', '-fauto-map-locations',

    '-std=450', '-O', "-I$includeRoot",

    '-DSITUATION_USE_OPENGL', '-DSITUATION_GL_VD_COMPOSITOR_SPIRV=1'

)



function Invoke-GlslcCompile([string]$Stage, [string]$Src, [string]$OutSpv, [string[]]$ExtraFlags) {

    $args = @("-fshader-stage=$Stage", $Src, '-o', $OutSpv) + $glFlags + $ExtraFlags

    & $glslc @args

    if ($LASTEXITCODE -ne 0) { throw "glslc failed: $Src" }

}



$vsPathBSpv = Join-Path $outDir 'vd_compositor_path_b.vs.spv'

$vsPathASpv = Join-Path $outDir 'vd_compositor_path_a.vs.spv'

$fsVdSpv = Join-Path $outDir 'vd_compositor_vd.fs.spv'

$fsCompositeSpv = Join-Path $outDir 'vd_compositor_composite.fs.spv'



if (Test-Path -LiteralPath $glslc) {

    $vert = Join-Path $ProjectRoot 'sit\gpu\compositor.vert'

    $vdFrag = Join-Path $ProjectRoot 'sit\gpu\vd.frag'

    $compFrag = Join-Path $ProjectRoot 'sit\gpu\composite.frag'



    Write-Host '[SHADER] Precompiling VD compositor SPIR-V (OpenGL)...'

    Invoke-GlslcCompile 'vertex' $vert $vsPathBSpv @('-DSIT_COMPOSITOR_PATH_B=1')

    Invoke-GlslcCompile 'vertex' $vert $vsPathASpv @('-DSIT_COMPOSITOR_PATH_A=1')

    Invoke-GlslcCompile 'fragment' $vdFrag $fsVdSpv @()

    Invoke-GlslcCompile 'fragment' $compFrag $fsCompositeSpv @()

} else {

    Write-Host '[WARN] glslc not found — VD compositor GL SPIR-V embed stays stub (zero length).'

}



$genScript = Join-Path $ProjectRoot 'scripts\gen_vd_compositor_gl_embed.ps1'
$embedArgs = @{
    VsPathBSpv       = $vsPathBSpv
    VsPathASpv       = $vsPathASpv
    FsVdSpv          = $fsVdSpv
    FsCompositeSpv   = $fsCompositeSpv
    OutC             = (Join-Path $embedDir 'sit_vd_compositor_gl_spirv_embed.c')
    OutH             = (Join-Path $embedDir 'sit_vd_compositor_gl_spirv_embed.h')
    HeaderInclude    = 'sit_vd_compositor_gl_spirv_embed.h'
}
& $genScript @embedArgs


Write-Host '[SHADER] OK: VD compositor GL SPIR-V embed regenerated.'

exit 0

