# concat_situation.ps1 - Concatenate the Situation library into a single C file.
#
# Recursively resolves all #include "..." directives within sit/ files,
# producing a self-contained output suitable for single-file distribution
# or feeding into tools that need the full source in one shot.
#
# Can be run from anywhere — the script resolves the repo root via $PSScriptRoot.
#
# Usage:
#     .\scripts\concat_situation.ps1 [output_file]   (from repo root)
#     .\concat_situation.ps1         [output_file]   (from scripts/)
#
#     Default output: <repo_root>\situation_full.c

param(
    [string]$OutputFile = ""
)

$ErrorActionPreference = "Stop"

# Resolve repo root: this script lives in <repo>/scripts/, so go one level up.
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

# Default output relative to repo root
if (-not $OutputFile) {
    $OutputFile = Join-Path $RepoRoot "situation_full.c"
} elseif (-not [System.IO.Path]::IsPathRooted($OutputFile)) {
    # Relative path: resolve against the caller's cwd, not the repo root
    $OutputFile = (Join-Path (Get-Location).Path $OutputFile)
}

# Switch to repo root so all sit/ relative paths work correctly
Push-Location $RepoRoot
try {

$script:seen = @{}
$script:result = [System.Text.StringBuilder]::new()

function Resolve-IncludePath {
    param([string]$IncludePath, [string]$CurrentFile)
    
    $baseDir = Split-Path -Parent $CurrentFile
    $candidate = Join-Path $baseDir $IncludePath
    if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
    
    # Try from repo root
    if (Test-Path $IncludePath) { return (Resolve-Path $IncludePath).Path }
    
    return $null
}

function Should-Inline {
    param([string]$IncludePath, [string]$ResolvedPath)
    
    if (-not $ResolvedPath) { return $false }
    
    $norm = $ResolvedPath.Replace('\', '/').ToLower()
    # Inline anything under sit/
    if ($norm -match '[\\/]sit[\\/]' -or $norm.StartsWith('sit/') -or $norm.StartsWith('sit\')) {
        return $true
    }
    # Also inline situation_impl* or situation_api* relative includes
    if ($IncludePath -match '^situation_(impl|api)') {
        return $true
    }
    return $false
}

function Concat-File {
    param([string]$FilePath)
    
    $normPath = (Resolve-Path $FilePath -ErrorAction SilentlyContinue)
    if (-not $normPath) {
        $script:result.AppendLine("/* [file not found: $FilePath] */") | Out-Null
        return
    }
    $normPath = $normPath.Path
    
    if ($script:seen.ContainsKey($normPath)) {
        $script:result.AppendLine("/* [already included: $FilePath] */") | Out-Null
        return
    }
    $script:seen[$normPath] = $true
    
    $separator = "=" * 70
    $script:result.AppendLine("") | Out-Null
    $script:result.AppendLine("/* $separator */") | Out-Null
    $script:result.AppendLine("/* FILE: $FilePath */") | Out-Null
    $script:result.AppendLine("/* $separator */") | Out-Null
    $script:result.AppendLine("") | Out-Null
    
    $lines = [System.IO.File]::ReadAllLines($normPath, [System.Text.Encoding]::UTF8)
    if (-not $lines) { return }
    
    foreach ($line in $lines) {
        if ($line -match '^\s*#include\s+"([^"]+)"') {
            $incPath = $Matches[1]
            $resolved = Resolve-IncludePath -IncludePath $incPath -CurrentFile $normPath
            if (Should-Inline -IncludePath $incPath -ResolvedPath $resolved) {
                Concat-File -FilePath $resolved
                continue
            }
        }
        $script:result.AppendLine($line) | Out-Null
    }
}

$outDir = Split-Path -Parent $OutputFile
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$script:result.AppendLine("/* Auto-generated single-file concatenation of the Situation library. */") | Out-Null
$script:result.AppendLine("/* Do not edit. Regenerate with: .\scripts\concat_situation.ps1 */") | Out-Null

Concat-File -FilePath "sit/situation.h"

[System.IO.File]::WriteAllText($OutputFile, $script:result.ToString(), [System.Text.UTF8Encoding]::new($false))

$lineCount = ($script:result.ToString() -split "`n").Count
$fileCount = $script:seen.Count
Write-Host "Concatenated $fileCount files -> $OutputFile ($lineCount lines)"

} finally {
    Pop-Location
}
