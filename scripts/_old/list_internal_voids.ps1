# Lists static internal helpers in sit/situation_impl*.h (void / bool / SituationError).
# Usage: .\scripts\list_internal_voids.ps1 > doc\plan\internal_void_inventory.csv

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$files = Get-ChildItem -Path (Join-Path $root "sit") -Filter "situation_impl*.h" -File | Sort-Object Name

Write-Output "file,line,return_type,name"
foreach ($file in $files) {
    $i = 0
    Get-Content -LiteralPath $file.FullName | ForEach-Object {
        $i++
        if ($_ -match '^static void (_\w+)') {
            Write-Output "$($file.Name),$i,void,$($Matches[1])"
        } elseif ($_ -match '^static bool (_\w+)') {
            Write-Output "$($file.Name),$i,bool,$($Matches[1])"
        } elseif ($_ -match '^static SituationError (_\w+)') {
            Write-Output "$($file.Name),$i,SituationError,$($Matches[1])"
        }
    }
}
