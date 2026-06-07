# audit_errno_report.ps1 - Full unused-errno report with candidate function homes
# Usage: powershell -ExecutionPolicy Bypass -File scripts\audit_errno_report.ps1

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ErrnoFile = Join-Path $Root "sit\situation_base_errno.h"
$OutFile   = Join-Path $Root "doc\ERRNO_USAGE_REPORT.md"
$raw = Get-Content $ErrnoFile -Raw

# Parse X-macro entries: X(NAME, VALUE, "MESSAGE")
$defined = [regex]::Matches($raw, 'X\((SITUATION_ERROR_[A-Z0-9_]+),\s*(-?\d+),\s*"([^"]+)"') | ForEach-Object {
    [PSCustomObject]@{ Name = $_.Groups[1].Value; Value = [int]$_.Groups[2].Value; Msg = $_.Groups[3].Value }
}

# Parse compat aliases: #define NAME SITUATION_ERROR_...
$aliases = [regex]::Matches($raw, '#define\s+(SITUATION_ERROR_[A-Z0-9_]+)\s+(SITUATION_ERROR_[A-Z0-9_]+)') | ForEach-Object {
    [PSCustomObject]@{ Alias = $_.Groups[1].Value; Target = $_.Groups[2].Value }
}

# Build full list of defined names
$definedNames = ($defined.Name + $aliases.Alias) | Sort-Object -Unique

# --- Domain-to-file mapping ---
# Maps errno section keywords to the impl files that would contain candidate functions
$domainFileMap = @{
    "Core & System"        = @("situation_impl_ctrl.h", "situation_impl_etc.h")
    "Threading"            = @("situation_impl_threading.h", "situation_impl_threading_topology.h", "situation_impl_threading_numa.h", "situation_impl_threading_scheduler.h", "situation_impl_threading_observability.h", "situation_impl_threading_diag.h")
    "Platform & Windowing" = @("situation_impl_wdm.h", "situation_impl_ctrl.h")
    "Input & HID"          = @("situation_impl_input.h")
    "Display"              = @("situation_impl_wdm.h", "situation_impl_vd.h")
    "Filesystem"           = @("situation_impl_io.h")
    "Asset & Serialization"= @("situation_impl_io.h", "situation_impl_renderer.h")
    "Plugins & Scripting"  = @("situation_impl_io.h")
    "Audio"                = @("situation_impl_audio.h")
    "Mixer"                = @("situation_impl_audio.h")
    "Node Graph"           = @("situation_impl_audio.h")
    "Device Registry"      = @("situation_impl_audio.h")
    "MIDI"                 = @("situation_impl_audio.h")
    "Rendering Core"       = @("situation_impl_renderer.h")
    "Fonts"                = @("situation_impl_image.h")
    "Image"                = @("situation_impl_image.h")
    "OpenGL"               = @("situation_impl_renderer.h")
    "Vulkan"               = @("situation_impl_renderer.h")
    "Compute"              = @("situation_impl_renderer.h")
    "Network"              = @("situation_impl_io.h")
}

# --- Keyword extraction from error name ---
# Turns SITUATION_ERROR_CLIPBOARD_FAILED into search keywords like "clipboard"
function Get-ErrorKeywords($errName) {
    $stripped = $errName -replace '^SITUATION_ERROR_', ''
    # Remove generic suffixes that don't help identify functions
    $stripped = $stripped -replace '_(FAILED|INVALID|LIMIT|REACHED|MISMATCH|CORRUPTED|MISSING|OVERFLOW|TIMEOUT|UNSUPPORTED|DENIED|LOCKED|NOT_FOUND|NOT_INITIALIZED|NOT_AVAILABLE|ALREADY_EXISTS|ALREADY_ACTIVE|NOT_ATTACHED|ALREADY_ATTACHED)$', ''
    # Split on underscore, lowercase
    $words = ($stripped -split '_') | Where-Object { $_.Length -gt 2 } | ForEach-Object { $_.ToLower() }
    return $words
}

# --- Scan impl files for SITAPI function declarations and their error-producing lines ---
# We find functions that use SITUATION_ERROR_GENERAL or _SituationSetErrorFromCode
# and associate them with the keywords from the error message / context

Write-Host "Scanning impl files for candidate function homes..." -ForegroundColor Cyan

# Cache: file -> array of { FuncName, Line, ErrorUsed, Context }
$funcErrorMap = @{}

$sitDir = Join-Path $Root "sit"
$audDir = Join-Path $Root "sit\aud"

# Also scan aud/ subdirectory
$implFiles = @()
$implFiles += Get-ChildItem -Path $sitDir -Filter "situation_impl*.h" -File
if (Test-Path $audDir) {
    $implFiles += Get-ChildItem -Path $audDir -Filter "*.h" -File -Recurse
}

foreach ($file in $implFiles) {
    $lines = Get-Content $file.FullName
    $currentFunc = ""
    $funcErrors = @{}

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]

        # Detect function definitions: SITAPI return_type FunctionName(...) or static ... functionName(
        # Also catch: SituationError _SituationInternalFunc(
        if ($line -match '(?:SITAPI|static\s+(?:inline\s+)?SituationError|^SituationError)\s+\w*\s*(Situation\w+|_Situation\w+)\s*\(') {
            $currentFunc = $matches[1]
        }

        # Track error usage in current function
        if ($currentFunc -and ($line -match 'SITUATION_ERROR_GENERAL|_SituationSetErrorFromCode')) {
            # Extract which error is used
            $errMatch = [regex]::Match($line, '(SITUATION_ERROR_[A-Z0-9_]+)')
            $errUsed = if ($errMatch.Success) { $errMatch.Groups[1].Value } else { "GENERAL" }
            # Get context (the message string if present)
            $msgMatch = [regex]::Match($line, '"([^"]+)"')
            $ctx = if ($msgMatch.Success) { $msgMatch.Groups[1].Value } else { "" }

            $relPath = $file.FullName.Substring($Root.Length + 1) -replace '\\', '/'

            if (-not $funcErrorMap.ContainsKey($relPath)) {
                $funcErrorMap[$relPath] = [System.Collections.ArrayList]::new()
            }
            [void]$funcErrorMap[$relPath].Add([PSCustomObject]@{
                FuncName = $currentFunc
                LineNum  = $i + 1
                ErrorUsed = $errUsed
                Context  = $ctx
            })
        }
    }
}

# --- Also scan situation_api.h for SITAPI function declarations by keyword ---
$apiFile = Join-Path $Root "sit\situation_api.h"
$apiLines = Get-Content $apiFile
$apiDecls = @{}  # keyword -> list of function names

foreach ($line in $apiLines) {
    if ($line -match 'SITAPI\s+\w+\s+(Situation\w+)\s*\(') {
        $funcName = $matches[1]
        # Extract keywords from function name
        $funcWords = [regex]::Matches($funcName, '[A-Z][a-z]+') | ForEach-Object { $_.Value.ToLower() }
        foreach ($w in $funcWords) {
            if ($w.Length -gt 2) {
                if (-not $apiDecls.ContainsKey($w)) { $apiDecls[$w] = [System.Collections.ArrayList]::new() }
                if ($funcName -notin $apiDecls[$w]) { [void]$apiDecls[$w].Add($funcName) }
            }
        }
    }
}

# --- For each never-produced error, find candidate homes ---
# Strategy:
# 1. Match error keywords to SITAPI function names (keyword overlap)
# 2. Find functions in domain impl files using SITUATION_ERROR_GENERAL where this specific error would be better
# 3. Find functions in domain impl files whose context message matches the error description

function Find-CandidateHomes($errName, $errMsg, $section) {
    $candidates = [System.Collections.ArrayList]::new()
    $keywords = Get-ErrorKeywords $errName

    # Strategy 1: find SITAPI functions with matching keywords
    $matchedFuncs = @{}
    foreach ($kw in $keywords) {
        if ($apiDecls.ContainsKey($kw)) {
            foreach ($fn in $apiDecls[$kw]) {
                if (-not $matchedFuncs.ContainsKey($fn)) { $matchedFuncs[$fn] = 0 }
                $matchedFuncs[$fn]++
            }
        }
    }
    # Keep functions that match 2+ keywords, or all if only 1 keyword
    $threshold = if ($keywords.Count -gt 1) { 2 } else { 1 }
    $apiMatches = $matchedFuncs.GetEnumerator() | Where-Object { $_.Value -ge $threshold } |
        Sort-Object { -$_.Value } | Select-Object -First 5 | ForEach-Object { $_.Key }

    if ($apiMatches) {
        foreach ($fn in $apiMatches) {
            [void]$candidates.Add([PSCustomObject]@{ Function = $fn; Reason = "API name matches keywords: $($keywords -join ', ')" })
        }
    }

    # Strategy 2: find SITUATION_ERROR_GENERAL usages in domain files whose context matches
    $domainFiles = $domainFileMap[$section]
    if ($domainFiles) {
        foreach ($df in $domainFiles) {
            $relKey = "sit/$df"
            if ($funcErrorMap.ContainsKey($relKey)) {
                foreach ($entry in $funcErrorMap[$relKey]) {
                    if ($entry.ErrorUsed -eq 'SITUATION_ERROR_GENERAL') {
                        # Check if context message relates to this error
                        $ctxLower = $entry.Context.ToLower()
                        $msgLower = $errMsg.ToLower()
                        $nameKeywords = $keywords | ForEach-Object { $_.ToLower() }

                        $contextMatch = $false
                        foreach ($kw in $nameKeywords) {
                            if ($ctxLower -match $kw) { $contextMatch = $true; break }
                        }
                        # Also check if error message words appear in context
                        $msgWords = ($msgLower -split '\s+') | Where-Object { $_.Length -gt 3 }
                        $msgOverlap = ($msgWords | Where-Object { $ctxLower -match [regex]::Escape($_) }).Count
                        if ($msgOverlap -ge 2) { $contextMatch = $true }

                        if ($contextMatch) {
                            $reason = "Uses GENERAL at L$($entry.LineNum)"
                            if ($entry.Context) { $reason += ": ``$($entry.Context)``" }
                            [void]$candidates.Add([PSCustomObject]@{ Function = "$($entry.FuncName) ($relKey)"; Reason = $reason })
                        }
                    }
                }
            }

            # Also check aud/ subdirectory files
            $audRelKey = "sit/aud/$df"
            if ($funcErrorMap.ContainsKey($audRelKey)) {
                foreach ($entry in $funcErrorMap[$audRelKey]) {
                    if ($entry.ErrorUsed -eq 'SITUATION_ERROR_GENERAL') {
                        $ctxLower = $entry.Context.ToLower()
                        $nameKeywords = $keywords | ForEach-Object { $_.ToLower() }
                        $contextMatch = $false
                        foreach ($kw in $nameKeywords) {
                            if ($ctxLower -match $kw) { $contextMatch = $true; break }
                        }
                        if ($contextMatch) {
                            $reason = "Uses GENERAL at L$($entry.LineNum)"
                            if ($entry.Context) { $reason += ": ``$($entry.Context)``" }
                            [void]$candidates.Add([PSCustomObject]@{ Function = "$($entry.FuncName) ($audRelKey)"; Reason = $reason })
                        }
                    }
                }
            }
        }
    }

    return $candidates
}

# Scan sit/ and tests/ for strict usage (error-producing lines only)
$scanRoots = @(
    (Join-Path $Root "sit"),
    (Join-Path $Root "tests")
)
$excludePath = 'k-term\\example\\situation_api\.h'

$usedStrict = @{}
$usedAny    = @{}

foreach ($root in $scanRoots) {
    Get-ChildItem -Recurse -Include *.c,*.h -Path $root | Where-Object {
        $_.FullName -notmatch $excludePath
    } | ForEach-Object {
        $allLines = Get-Content $_.FullName
        foreach ($line in $allLines) {
            # Strip C comments to avoid phantom matches from EOL annotations
            $codeLine = $line -replace '/\*.*?\*/', '' -replace '//.*$', ''
            [regex]::Matches($codeLine, 'SITUATION_ERROR_[A-Z0-9_]+[A-Z0-9]') | ForEach-Object {
                if ($_.Value -ne 'SITUATION_ERROR_TABLE') { $usedAny[$_.Value] = $true }
            }
        }
        $strictLines = $allLines | Where-Object {
            $_ -match '_SituationSetErrorFromCode|_SituationSetGLErrorFromSpirvStage|_SituationSetFilesystemError|_SituationGLSpecializeSpirvShader|return\s+SITUATION_ERROR|return\s+_Situation|SIT_RETURN_IF_ERR|error_code\s*=\s*SITUATION_ERROR|\*error_code\)\s*=|err_code\s*=\s*SITUATION_ERROR|spirv_err\s*=|specific_error_code\s*=|\?\s*SITUATION_ERROR'
        }
        foreach ($line in $strictLines) {
            $codeLine = $line -replace '/\*.*?\*/', '' -replace '//.*$', ''
            [regex]::Matches($codeLine, 'SITUATION_ERROR_[A-Z0-9_]+[A-Z0-9]') | ForEach-Object {
                if ($_.Value -ne 'SITUATION_ERROR_TABLE') { $usedStrict[$_.Value] = $true }
            }
        }
    }
}

# Identify EOL-tagged entries
$eolEntries = [regex]::Matches($raw, 'X\((SITUATION_ERROR_[A-Z0-9_]+),[^)]+\)\s*/\*\s*EOL') | ForEach-Object {
    $_.Groups[1].Value
}

# Categorise
$neverProduced = $definedNames | Where-Object { -not $usedStrict.ContainsKey($_) } | Sort-Object
$neverReferenced = $definedNames | Where-Object { -not $usedAny.ContainsKey($_) } | Sort-Object
$phantoms = $usedAny.Keys | Where-Object {
    $name = $_
    if ($name -in $definedNames) { return $false }
    # Exclude names that are proper prefixes of a defined name (comment artifacts like SITUATION_ERROR_VIRTUAL_DISPLAY from _xxx)
    $isPrefix = $definedNames | Where-Object { $_.StartsWith($name + '_') }
    if ($isPrefix) { return $false }
    return $true
} | Sort-Object

# Determine section for each error code
function Get-Section($name) {
    $entry = $defined | Where-Object { $_.Name -eq $name }
    if (-not $entry) {
        $alias = $aliases | Where-Object { $_.Alias -eq $name }
        if ($alias) { return "Compat Alias -> $($alias.Target)" }
        return "Unknown"
    }
    $v = $entry.Value
    if ($v -eq 0) { return "Core (Success)" }
    if ($v -ge -99  -and $v -le -1)   { return "Core & System" }
    if ($v -ge -98  -and $v -le -80)  { return "Threading" }
    if ($v -ge -149 -and $v -le -130) { return "Input & HID" }
    if ($v -ge -199 -and $v -le -100) { return "Platform & Windowing" }
    if ($v -ge -299 -and $v -le -200) { return "Display" }
    if ($v -ge -329 -and $v -le -300) { return "Filesystem" }
    if ($v -ge -349 -and $v -le -330) { return "Asset & Serialization" }
    if ($v -ge -379 -and $v -le -360) { return "Plugins & Scripting" }
    if ($v -ge -439 -and $v -le -400) { return "Audio" }
    if ($v -ge -459 -and $v -le -440) { return "Mixer" }
    if ($v -ge -479 -and $v -le -460) { return "Node Graph" }
    if ($v -ge -493 -and $v -le -480) { return "Device Registry" }
    if ($v -ge -499 -and $v -le -494) { return "MIDI" }
    if ($v -ge -559 -and $v -le -500) { return "Rendering Core" }
    if ($v -ge -579 -and $v -le -560) { return "Fonts" }
    if ($v -ge -589 -and $v -le -580) { return "Image" }
    if ($v -ge -699 -and $v -le -600) { return "OpenGL" }
    if ($v -ge -799 -and $v -le -700) { return "Vulkan" }
    if ($v -ge -899 -and $v -le -800) { return "Compute" }
    if ($v -ge -949 -and $v -le -900) { return "Network" }
    if ($v -eq -999) { return "Unknown/Catch-All" }
    return "Other ($v)"
}

# Build markdown report
$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine("# Errno Usage Report")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm')")
[void]$sb.AppendLine("Script: ``scripts/audit_errno_report.ps1``")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Summary")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| Metric | Count |")
[void]$sb.AppendLine("|--------|-------|")
[void]$sb.AppendLine("| Defined in X-macro table | $($defined.Count) |")
[void]$sb.AppendLine("| Compat aliases (#define) | $($aliases.Count) |")
[void]$sb.AppendLine("| Total unique names | $($definedNames.Count) |")
[void]$sb.AppendLine("| Used (any reference in sit/ + tests/) | $($usedAny.Count) |")
[void]$sb.AppendLine("| Used strictly (error-producing lines) | $($usedStrict.Count) |")
[void]$sb.AppendLine("| **Never produced (strict)** | **$($neverProduced.Count)** |")
[void]$sb.AppendLine("| Never referenced at all | $($neverReferenced.Count) |")
[void]$sb.AppendLine("| EOL-tagged (deprecated) | $($eolEntries.Count) |")
[void]$sb.AppendLine("| Phantom (used but undefined) | $($phantoms.Count) |")
[void]$sb.AppendLine("")

if ($phantoms.Count -gt 0) {
    [void]$sb.AppendLine("## Phantom Errors (used but NOT in table)")
    [void]$sb.AppendLine("")
    foreach ($p in $phantoms) { [void]$sb.AppendLine("- ``$p``") }
    [void]$sb.AppendLine("")
}

[void]$sb.AppendLine("## Never Produced - With Candidate Homes")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("These error codes are defined but never appear on a ``return`` or")
[void]$sb.AppendLine("``_SituationSetErrorFromCode`` line. For each, candidate functions are")
[void]$sb.AppendLine("identified where they could logically be used (based on keyword matching")
[void]$sb.AppendLine("and detection of ``SITUATION_ERROR_GENERAL`` in domain-relevant code).")
[void]$sb.AppendLine("")

# Group by section
$grouped = $neverProduced | ForEach-Object {
    [PSCustomObject]@{ Name = $_; Section = (Get-Section $_) }
} | Group-Object Section | Sort-Object Name

$withHomes = 0
$withoutHomes = 0

foreach ($g in $grouped) {
    [void]$sb.AppendLine("### $($g.Name) ($($g.Count))")
    [void]$sb.AppendLine("")

    foreach ($item in ($g.Group | Sort-Object Name)) {
        $errEntry = $defined | Where-Object { $_.Name -eq $item.Name }
        $isEol = if ($item.Name -in $eolEntries) { " [EOL]" } else { "" }
        $anyRef = if ($usedAny.ContainsKey($item.Name)) { "" } else { " [UNREFERENCED]" }
        $msg = if ($errEntry) { $errEntry.Msg } else { "" }

        [void]$sb.AppendLine("#### ``$($item.Name)``$isEol$anyRef")
        if ($msg) { [void]$sb.AppendLine("") ; [void]$sb.AppendLine("> $msg") }
        [void]$sb.AppendLine("")

        # Skip compat aliases for candidate search
        $isAlias = $aliases | Where-Object { $_.Alias -eq $item.Name }
        if ($isAlias) {
            [void]$sb.AppendLine("Compat alias for ``$($isAlias.Target)`` - use the target name instead.")
            [void]$sb.AppendLine("")
            continue
        }

        $candidates = Find-CandidateHomes $item.Name $msg $item.Section

        if ($candidates.Count -gt 0) {
            $withHomes++
            [void]$sb.AppendLine("**Candidate homes:**")
            [void]$sb.AppendLine("")
            foreach ($c in $candidates | Select-Object -First 8) {
                [void]$sb.AppendLine("- ``$($c.Function)`` - $($c.Reason)")
            }
        } else {
            $withoutHomes++
            # Provide the API keyword matches as a fallback hint
            $keywords = Get-ErrorKeywords $item.Name
            $hintFuncs = @()
            foreach ($kw in $keywords) {
                if ($apiDecls.ContainsKey($kw)) {
                    $hintFuncs += $apiDecls[$kw] | Select-Object -First 3
                }
            }
            $hintFuncs = $hintFuncs | Sort-Object -Unique | Select-Object -First 5
            if ($hintFuncs.Count -gt 0) {
                $withHomes++; $withoutHomes--
                [void]$sb.AppendLine("**Related API functions** (keyword match: ``$($keywords -join '``, ``')``)**:**")
                [void]$sb.AppendLine("")
                foreach ($fn in $hintFuncs) {
                    [void]$sb.AppendLine("- ``$fn``")
                }
            } else {
                [void]$sb.AppendLine("*No candidate homes found - likely reserved for future subsystem.*")
            }
        }
        [void]$sb.AppendLine("")
    }
}

[void]$sb.AppendLine("---")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Actionability Summary")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| Category | Count |")
[void]$sb.AppendLine("|----------|-------|")
[void]$sb.AppendLine("| Unused errors with candidate homes | $withHomes |")
[void]$sb.AppendLine("| Unused errors without candidates (reserved/future) | $withoutHomes |")
[void]$sb.AppendLine("| Phantom errors (need table entry or rename) | $($phantoms.Count) |")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Recommendations")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("1. **Phantom errors** - add to the table or rename the usage to a valid code.")
[void]$sb.AppendLine("2. **Errors with candidate homes** - replace ``SITUATION_ERROR_GENERAL`` with the specific code in identified functions.")
[void]$sb.AppendLine("3. **EOL + never produced** - safe candidates for removal in a future cleanup pass.")
[void]$sb.AppendLine("4. **No candidates (reserved)** - keep in table; wire up when subsystem ships.")
[void]$sb.AppendLine("")

$sb.ToString() | Out-File -Encoding utf8 $OutFile
Write-Host "Report written to: $OutFile" -ForegroundColor Green
Write-Host "Never-produced: $($neverProduced.Count) / $($definedNames.Count)"
Write-Host "  With candidate homes: $withHomes"
Write-Host "  Reserved (no candidates): $withoutHomes"
