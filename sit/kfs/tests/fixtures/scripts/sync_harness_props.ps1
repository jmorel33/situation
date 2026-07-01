# Sync curated Situation harness assets into sit/kfs/tests/fixtures/props/
# Run from sit/kfs/tests: powershell -File fixtures/scripts/sync_harness_props.ps1

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$FixturesDir = Resolve-Path (Join-Path $ScriptDir "..")
$PropsRoot = Join-Path $FixturesDir "props"
$TestsDir = Resolve-Path (Join-Path $FixturesDir "..")
if (-not (Test-Path $PropsRoot)) {
    New-Item -ItemType Directory -Path $PropsRoot -Force | Out-Null
}
$PropsRoot = Resolve-Path $PropsRoot
$HarnessAssets = Resolve-Path (Join-Path $TestsDir "../../../tests/harness/assets")

if (-not (Test-Path $HarnessAssets)) {
    Write-Error "Situation harness assets not found at: $HarnessAssets"
}

$Entries = @(
    @{ Id = "prairie_jpg";    Dest = "textures/prairie.jpg";           Source = "prairie.jpg" },
    @{ Id = "thoc_jpg";       Dest = "textures/thoc.jpg";              Source = "thoc.jpg" },
    @{ Id = "rosewood_png";   Dest = "textures/rosewood_veneer1.png";   Source = "rosewood_veneer1.png" },
    @{ Id = "bunny_obj";      Dest = "models/stanford-bunny.obj";      Source = "stanford-bunny.obj" },
    @{ Id = "teapot_obj";     Dest = "models/utah_teapot.obj";         Source = "utah_teapot.obj" },
    @{ Id = "teapot_stl";     Dest = "models/teapot.stl";              Source = "teapot.stl" },
    @{ Id = "boombox_glb";    Dest = "models/BoomBox.glb";             Source = "BoomBox.glb" },
    @{ Id = "sample_wav";     Dest = "audio/sample.wav";               Source = "sample.wav" },
    @{ Id = "demon_vs";       Dest = "shaders/demon_hunt_sky.vs";      Source = "demon_hunt_sky.vs" },
    @{ Id = "demon_fs";       Dest = "shaders/demon_hunt_sky.fs";      Source = "demon_hunt_sky.fs" },
    @{ Id = "roboto_ttf";     Dest = "fonts/Roboto-Regular.ttf";       Source = "static/Roboto-Regular.ttf" }
)

$Meta = @{
    prairie_jpg  = @{ kfs_type = "image";  format = "jpg";  topic = "perf_textures"; epic = "perf_graphics"; tier = "S" }
    thoc_jpg     = @{ kfs_type = "image";  format = "jpg";  topic = "perf_textures"; epic = "perf_graphics"; tier = "S" }
    rosewood_png = @{ kfs_type = "image";  format = "png";  topic = "perf_textures"; epic = "perf_graphics"; tier = "M" }
    bunny_obj    = @{ kfs_type = "model";  format = "obj";  topic = "perf_models";   epic = "perf_geometry"; tier = "M" }
    teapot_obj   = @{ kfs_type = "model";  format = "obj";  topic = "perf_models";   epic = "perf_geometry"; tier = "M" }
    teapot_stl   = @{ kfs_type = "model";  format = "stl";  topic = "perf_models";   epic = "perf_geometry"; tier = "L" }
    boombox_glb  = @{ kfs_type = "model";  format = "glb";  topic = "perf_models";   epic = "perf_geometry"; tier = "L" }
    sample_wav   = @{ kfs_type = "audio";  format = "wav";  topic = "perf_audio";    epic = "perf_media";    tier = "L" }
    demon_vs     = @{ kfs_type = "shader"; format = "glsl"; topic = "perf_shaders";  epic = "perf_graphics"; tier = "S" }
    demon_fs     = @{ kfs_type = "shader"; format = "glsl"; topic = "perf_shaders";  epic = "perf_graphics"; tier = "S" }
    roboto_ttf   = @{ kfs_type = "font";   format = "ttf";  topic = "perf_fonts";    epic = "perf_media";    tier = "M" }
}

$manifestProps = @()

foreach ($e in $Entries) {
    $srcPath = Join-Path $HarnessAssets $e.Source
    if (-not (Test-Path $srcPath)) {
        Write-Error "Missing harness source: $srcPath"
    }

    $destPath = Join-Path $PropsRoot $e.Dest
    $destDir = Split-Path -Parent $destPath
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }

    Copy-Item -Path $srcPath -Destination $destPath -Force
    $hash = Get-FileHash -Path $destPath -Algorithm SHA256
    $bytes = (Get-Item $destPath).Length
    $m = $Meta[$e.Id]

    $manifestProps += @{
        id       = $e.Id
        file     = $e.Dest
        source   = $e.Source
        bytes    = $bytes
        sha256   = $hash.Hash.ToLower()
        kfs_type = $m.kfs_type
        format   = $m.format
        topic    = $m.topic
        epic     = $m.epic
        tier     = $m.tier
    }

    Write-Host ("  synced {0} ({1:N0} bytes)" -f $e.Id, $bytes)
}

$manifest = @{
    version     = 1
    source_root = "tests/harness/assets/"
    synced_at   = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    props       = $manifestProps
}

$manifestPath = Join-Path $PropsRoot "PROPS_MANIFEST.json"
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Path $manifestPath -Encoding UTF8

Write-Host ""
Write-Host ("Props synced to {0}" -f $PropsRoot)
Write-Host ("Manifest: {0}" -f $manifestPath)