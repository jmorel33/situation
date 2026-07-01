# Emit C embed arrays for harness test-pattern SPIR-V blobs.
param(
    [Parameter(Mandatory = $true)][string]$FsPatternSpv,
    [Parameter(Mandatory = $true)][string]$FsSmpteVdSpv,
    [Parameter(Mandatory = $true)][string]$OutC,
    [Parameter(Mandatory = $true)][string]$HeaderInclude,
    [string]$Prefix = "sit_harness_tp_gl"
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

function Read-SpirvOrStub([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        return [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
    }
    return [byte[]]@()
}

$fsPattern = Read-SpirvOrStub $FsPatternSpv
$fsSmpteVd = Read-SpirvOrStub $FsSmpteVdSpv

$outDir = Split-Path -Parent $OutC
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$sw = New-Object System.IO.StreamWriter($OutC, $false, [System.Text.UTF8Encoding]::new($false))
$sw.WriteLine('/* Auto-generated — do not edit; run compile_harness_shaders.bat */')
$sw.WriteLine("#include `"$HeaderInclude`"")
$sw.WriteLine('')
Write-SpirvArray "${Prefix}_test_pattern_fs_spv" $fsPattern
$sw.WriteLine('')
Write-SpirvArray "${Prefix}_test_pattern_smpte_vd_fs_spv" $fsSmpteVd
$sw.Close()
