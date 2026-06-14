# Emit examples/demon_hunt_sky_spirv_embed.c from OpenGL + Vulkan .spv files.
param(
    [Parameter(Mandatory = $true)][string]$VsSpvGl,
    [Parameter(Mandatory = $true)][string]$FsSpvGl,
    [Parameter(Mandatory = $true)][string]$VsSpvVk,
    [Parameter(Mandatory = $true)][string]$FsSpvVk,
    [Parameter(Mandatory = $true)][string]$OutC
)
$ErrorActionPreference = 'Stop'

function Read-SpirvBytes([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return [byte[]]@()
    }
    return [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
}

$vsGl = Read-SpirvBytes $VsSpvGl
$fsGl = Read-SpirvBytes $FsSpvGl
$vsVk = Read-SpirvBytes $VsSpvVk
$fsVk = Read-SpirvBytes $FsSpvVk

$outDir = Split-Path -Parent $OutC
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$sw = New-Object System.IO.StreamWriter($OutC, $false, [System.Text.UTF8Encoding]::new($false))

function Write-SpirvArray {
    param([string]$ArrayName, [byte[]]$Bytes)
    $sw.WriteLine("const unsigned char $ArrayName[] = {")
    if ($Bytes.Length -eq 0) {
        $sw.WriteLine('    0x00')
    } else {
        for ($i = 0; $i -lt $Bytes.Length; $i += 16) {
            $lim = [Math]::Min($i + 15, $Bytes.Length - 1)
            $parts = @()
            for ($j = $i; $j -le $lim; $j++) {
                $parts += ('0x{0:x2}' -f $Bytes[$j])
            }
            $line = '    ' + ($parts -join ', ')
            if ($lim -lt $Bytes.Length - 1) {
                $sw.WriteLine("$line,")
            } else {
                $sw.WriteLine($line)
            }
        }
    }
    $sw.WriteLine('};')
    $sw.WriteLine("const size_t ${ArrayName}_len = sizeof($ArrayName);")
}

$sw.WriteLine('/* Auto-generated from demon_hunt_sky SPIR-V - do not edit; regenerate via compile_demon_hunt_shaders.bat */')
$sw.WriteLine('#include "examples/demon_hunt/demon_hunt_sky_spirv_embed.h"')
$sw.WriteLine('')
Write-SpirvArray 'demon_hunt_sky_vs_spv' $vsGl
$sw.WriteLine('')
Write-SpirvArray 'demon_hunt_sky_fs_spv' $fsGl
$sw.WriteLine('')
Write-SpirvArray 'demon_hunt_sky_vs_spv_vk' $vsVk
$sw.WriteLine('')
Write-SpirvArray 'demon_hunt_sky_fs_spv_vk' $fsVk
$sw.Close()
