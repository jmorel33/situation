@echo off
REM Patch wrappers\Odin\situation_foreign.odin foreign import line for the active link mode.
REM Requires: SIT_ODIN_FOREIGN_IMPORT

set "FOREIGN_FILE=wrappers\Odin\situation_foreign.odin"
set "FOREIGN_BACKUP=wrappers\Odin\situation_foreign.odin.linkbak"

if not exist "%FOREIGN_FILE%" (
    echo [ERROR] Missing %FOREIGN_FILE%
    exit /b 1
)

if not exist "%FOREIGN_BACKUP%" (
    copy /y "%FOREIGN_FILE%" "%FOREIGN_BACKUP%" >nul
)

powershell -NoProfile -Command ^
  "$path = '%FOREIGN_FILE%';" ^
  "$import = 'foreign import situation \"%SIT_ODIN_FOREIGN_IMPORT%\"';" ^
  "$lines = Get-Content -LiteralPath $path;" ^
  "$idx = ($lines | Select-String -Pattern '^foreign import situation ' | Select-Object -First 1).LineNumber;" ^
  "if (-not $idx) { Write-Error 'foreign import line not found'; exit 1 };" ^
  "$lines[$idx - 1] = $import;" ^
  "Set-Content -LiteralPath $path -Value $lines -Encoding UTF8"

if errorlevel 1 (
    echo [ERROR] Failed to patch Odin foreign import in %FOREIGN_FILE%
    exit /b 1
)
exit /b 0
