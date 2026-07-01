@echo off
set "FOREIGN_FILE=wrappers\Odin\situation_foreign.odin"
set "FOREIGN_BACKUP=wrappers\Odin\situation_foreign.odin.linkbak"
if exist "%FOREIGN_BACKUP%" (
    copy /y "%FOREIGN_BACKUP%" "%FOREIGN_FILE%" >nul
)
exit /b 0
