# Emit a C translation unit with embedded SPIR-V byte arrays for harness tests.
param(
    [Parameter(Mandatory = $true)][string]$VsSpv,
    [Parameter(Mandatory = $true)][string]$FsDualSpv,
    [Parameter(Mandatory = $true)][string]$FsUboSpv,
    [Parameter(Mandatory = $true)][string]$OutC,
    [Parameter(Mandatory = $true)][string]$HeaderInclude,
    [string]$Prefix = "sit_harness"
)
$ErrorActionPreference = 'Stop'

function Write-SpirvArray([string]$ArrayName, [byte[]]$Bytes) {
    $script:sw.WriteLine("const unsigned char ${ArrayName}[] = {")
    for ($i = 0; $i -lt $Bytes.Length; $i += 16) {
        $lim = [Math]::Min($i + 15, $Bytes.Length - 1)
        $parts = @()
        for ($j = $i; $j -le $lim; $j++) {
            $parts += ('0x{0:x2}' -f $Bytes[$j])
        }
        $line = '    ' + ($parts -join ', ')
        if ($lim -lt $Bytes.Length - 1) { $script:sw.WriteLine("$line,") }
        else { $script:sw.WriteLine($line) }
    }
    $script:sw.WriteLine('};')
    $script:sw.WriteLine("const size_t ${ArrayName}_len = sizeof(${ArrayName});")
}

$vs = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $VsSpv).Path)
$fsDual = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $FsDualSpv).Path)
$fsUbo = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $FsUboSpv).Path)

$outDir = Split-Path -Parent $OutC
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$sw = New-Object System.IO.StreamWriter($OutC, $false, [System.Text.UTF8Encoding]::new($false))
$sw.WriteLine('/* Auto-generated — do not edit; run compile_harness_shaders.bat */')
$sw.WriteLine("#include `"$HeaderInclude`"")
$sw.WriteLine('')
Write-SpirvArray "${Prefix}_passthrough_vs_spv" $vs
$sw.WriteLine('')
Write-SpirvArray "${Prefix}_dual_ssbo_fs_spv" $fsDual
$sw.WriteLine('')
Write-SpirvArray "${Prefix}_ubo_ssbo_fs_spv" $fsUbo
$sw.Close()
