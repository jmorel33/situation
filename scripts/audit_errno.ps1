# audit_errno.ps1 — Compare situation_base_errno.h table vs sit/ + tests/ usage.
# Usage: .\scripts\audit_errno.ps1 [-StrictErrorPaths]
param([switch]$StrictErrorPaths)

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ErrnoFile = Join-Path $Root "sit\situation_base_errno.h"
$raw = Get-Content $ErrnoFile -Raw

$defined = [regex]::Matches($raw, 'X\((SITUATION_ERROR_[A-Z0-9_]+),\s*(-?\d+)') | ForEach-Object {
    [PSCustomObject]@{ Name = $_.Groups[1].Value; Value = [int]$_.Groups[2].Value }
}

$dupVals = $defined | Group-Object Value | Where-Object { $_.Count -gt 1 }
if ($dupVals.Count -gt 0) {
    Write-Host "WARN: duplicate numeric values (intentional compat aliases):" -ForegroundColor Yellow
    foreach ($g in $dupVals) {
        Write-Host "  value $($g.Name): $($g.Group.Name -join ', ')"
    }
}

$scanRoots = @(
    (Join-Path $Root "sit"),
    (Join-Path $Root "tests")
)
$excludePath = 'k-term\\example\\situation_api\.h'

$used = @{}
foreach ($root in $scanRoots) {
    Get-ChildItem -Recurse -Include *.c,*.h -Path $root | Where-Object {
        $_.FullName -notmatch $excludePath
    } | ForEach-Object {
        $lines = Get-Content $_.FullName
        if ($StrictErrorPaths) {
            $lines = $lines | Where-Object {
                $_ -match '_SituationSetErrorFromCode|_SituationSetGLErrorFromSpirvStage|_SituationSetFilesystemError|_SituationGLSpecializeSpirvShader|return\s+SITUATION_ERROR|return\s+_Situation|SIT_RETURN_IF_ERR|error_code\s*=\s*SITUATION_ERROR|\*error_code\)\s*=|err_code\s*=\s*SITUATION_ERROR|spirv_err\s*=|specific_error_code\s*=|\?\s*SITUATION_ERROR'
            }
        }
        foreach ($line in $lines) {
            $codeLine = $line -replace '/\*.*?\*/', '' -replace '//.*$', ''
            [regex]::Matches($codeLine, 'SITUATION_ERROR_[A-Z0-9_]+[A-Z0-9]') | ForEach-Object {
                if ($_.Value -ne 'SITUATION_ERROR_TABLE') { $used[$_.Value] = $true }
            }
        }
    }
}

$definedNames = $defined.Name | Sort-Object -Unique
$aliasMatches = [regex]::Matches($raw, '#define\s+(SITUATION_ERROR_[A-Z0-9_]+)\s+SITUATION_ERROR_')
foreach ($m in $aliasMatches) { $definedNames += $m.Groups[1].Value }
$definedNames = $definedNames | Sort-Object -Unique
$neverUsed = $definedNames | Where-Object { -not $used.ContainsKey($_) }
$phantom = $used.Keys | Where-Object {
    $name = $_
    if ($name -in $definedNames) { return $false }
    if ($definedNames | Where-Object { $_.StartsWith($name) -and $_.Length -gt $name.Length }) { return $false }
    return $true
} | Sort-Object

Write-Host "Defined in table: $($definedNames.Count) (rows: $($defined.Count))"
Write-Host "Referenced in scan: $($used.Count)"
if ($StrictErrorPaths) { Write-Host "(Strict: error-setting lines only)" }
Write-Host ""

if ($phantom.Count -gt 0) {
    Write-Host "FAIL: used but NOT in table ($($phantom.Count)):" -ForegroundColor Red
    $phantom
} else {
    Write-Host "OK: no phantom error names outside table." -ForegroundColor Green
}

$eol = [regex]::Matches($raw, 'EOL[^\\]*') | Measure-Object
Write-Host ""
Write-Host "EOL-tagged rows: $($eol.Count)"
Write-Host "Never referenced ($($neverUsed.Count)):"
if ($neverUsed.Count -gt 0 -and $neverUsed.Count -le 40) { $neverUsed }
elseif ($neverUsed.Count -gt 40) { $neverUsed | Select-Object -First 40; Write-Host "  ... and $($neverUsed.Count - 40) more (reserved / future)" }

if ($phantom.Count -gt 0) { exit 1 }
exit 0
