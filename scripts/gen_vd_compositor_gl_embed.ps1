# Emit C translation unit with embedded VD compositor SPIR-V (OpenGL).
param(
    [Parameter(Mandatory = $true)][string]$VsPathBSpv,
    [Parameter(Mandatory = $true)][string]$VsPathASpv,
    [Parameter(Mandatory = $true)][string]$FsVdSpv,
    [Parameter(Mandatory = $true)][string]$FsCompositeSpv,
    [Parameter(Mandatory = $true)][string]$OutC,
    [Parameter(Mandatory = $true)][string]$HeaderInclude,
    [Parameter(Mandatory = $true)][string]$OutH,
    [string]$Prefix = 'sit_vd_gl'
)
$ErrorActionPreference = 'Stop'

function Write-SpirvArray([System.Text.StringBuilder]$Sb, [string]$ArrayName, [byte[]]$Bytes) {
    [void]$Sb.AppendLine("const unsigned char ${ArrayName}[] = {")
    for ($i = 0; $i -lt $Bytes.Length; $i += 16) {
        $lim = [Math]::Min($i + 15, $Bytes.Length - 1)
        $parts = New-Object 'System.Collections.Generic.List[string]' $Bytes.Length
        for ($j = $i; $j -le $lim; $j++) {
            $parts.Add(('0x{0:x2}' -f $Bytes[$j]))
        }
        $line = '    ' + ($parts -join ', ')
        if ($lim -lt $Bytes.Length - 1) { [void]$Sb.AppendLine("$line,") }
        else { [void]$Sb.AppendLine($line) }
    }
    [void]$Sb.AppendLine('};')
    [void]$Sb.AppendLine("const size_t ${ArrayName}_len = sizeof(${ArrayName});")
}

function Read-SpirvOrStub([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        return [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
    }
    return [byte[]]@(0)
}

function Write-VdCompositorGlEmbedHeader([string]$Path) {
    $headerDir = Split-Path -Parent $Path
    if ($headerDir -and -not (Test-Path -LiteralPath $headerDir)) {
        New-Item -ItemType Directory -Path $headerDir -Force | Out-Null
    }
    $lines = @(
        '#ifndef SIT_VD_COMPOSITOR_GL_SPIRV_EMBED_H',
        '#define SIT_VD_COMPOSITOR_GL_SPIRV_EMBED_H',
        '',
        '#include <stddef.h>',
        '',
        'extern const unsigned char sit_vd_gl_compositor_vs_path_b_spv[];',
        'extern const size_t sit_vd_gl_compositor_vs_path_b_spv_len;',
        '',
        'extern const unsigned char sit_vd_gl_compositor_vs_path_a_spv[];',
        'extern const size_t sit_vd_gl_compositor_vs_path_a_spv_len;',
        '',
        'extern const unsigned char sit_vd_gl_vd_fs_spv[];',
        'extern const size_t sit_vd_gl_vd_fs_spv_len;',
        '',
        'extern const unsigned char sit_vd_gl_composite_fs_spv[];',
        'extern const size_t sit_vd_gl_composite_fs_spv_len;',
        '',
        '#endif /* SIT_VD_COMPOSITOR_GL_SPIRV_EMBED_H */',
        ''
    )
    [System.IO.File]::WriteAllLines($Path, $lines, [System.Text.UTF8Encoding]::new($false))
}

Write-VdCompositorGlEmbedHeader $OutH

$vsB = Read-SpirvOrStub $VsPathBSpv
$vsA = Read-SpirvOrStub $VsPathASpv
$fsVd = Read-SpirvOrStub $FsVdSpv
$fsComp = Read-SpirvOrStub $FsCompositeSpv

$outDir = Split-Path -Parent $OutC
if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$sb = New-Object System.Text.StringBuilder 1048576
[void]$sb.AppendLine('/* Auto-generated — do not edit; run build/compile_vd_compositor_gl.ps1 */')
[void]$sb.AppendLine("#include `"$HeaderInclude`"")
[void]$sb.AppendLine('')
Write-SpirvArray $sb "${Prefix}_compositor_vs_path_b_spv" $vsB
[void]$sb.AppendLine('')
Write-SpirvArray $sb "${Prefix}_compositor_vs_path_a_spv" $vsA
[void]$sb.AppendLine('')
Write-SpirvArray $sb "${Prefix}_vd_fs_spv" $fsVd
[void]$sb.AppendLine('')
Write-SpirvArray $sb "${Prefix}_composite_fs_spv" $fsComp
[System.IO.File]::WriteAllText($OutC, $sb.ToString(), [System.Text.UTF8Encoding]::new($false))
