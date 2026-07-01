@echo off
REM Standard wrapper example output / object directories.
REM
REM Usage: call scripts\wrapper_paths.bat <language> [example_name] [backend]
REM   language: fortran | modula2 | rust | zig | odin | python | lua
REM
REM Sets:
REM   SIT_WRAPPER_LANG, SIT_WRAPPER_OUT_DIR
REM   SIT_WRAPPER_OBJ_BASE  (intermediate objects root under build\obj\)
REM   SIT_WRAPPER_EXAMPLE_OBJ_DIR  (per-example+backend objects, when name/backend given)

set "SIT_WRAPPER_LANG=%~1"
set "SIT_WRAPPER_EXAMPLE=%~2"
set "SIT_WRAPPER_BACKEND=%~3"

if /i "%SIT_WRAPPER_LANG%"=="fortran" (
    set "SIT_WRAPPER_OUT_DIR=build\examples\fortran"
    set "SIT_WRAPPER_OBJ_BASE=build\obj\fortran"
    goto :example_obj
)
if /i "%SIT_WRAPPER_LANG%"=="modula2" (
    set "SIT_WRAPPER_OUT_DIR=build\examples\modula2"
    set "SIT_WRAPPER_OBJ_BASE=build\obj\modula2"
    goto :example_obj
)
if /i "%SIT_WRAPPER_LANG%"=="rust" (
    set "SIT_WRAPPER_OUT_DIR=build\examples\rust"
    set "SIT_WRAPPER_OBJ_BASE=wrappers\Rust\target"
    goto :done
)
if /i "%SIT_WRAPPER_LANG%"=="zig" (
    set "SIT_WRAPPER_OUT_DIR=build\examples\zig"
    set "SIT_WRAPPER_OBJ_BASE=wrappers\Zig\.zig-cache"
    goto :done
)
if /i "%SIT_WRAPPER_LANG%"=="odin" (
    set "SIT_WRAPPER_OUT_DIR=build\examples\odin"
    set "SIT_WRAPPER_OBJ_BASE=wrappers\Odin"
    goto :done
)
if /i "%SIT_WRAPPER_LANG%"=="python" (
    set "SIT_WRAPPER_OUT_DIR=build\examples\python"
    set "SIT_WRAPPER_OBJ_BASE=build\obj\python"
    goto :example_obj
)
if /i "%SIT_WRAPPER_LANG%"=="lua" (
    set "SIT_WRAPPER_OUT_DIR=build\examples\lua"
    set "SIT_WRAPPER_OBJ_BASE=build\obj\lua"
    goto :example_obj
)

echo [ERROR] wrapper_paths.bat: unknown language "%SIT_WRAPPER_LANG%"
exit /b 1

:example_obj
if not "%SIT_WRAPPER_EXAMPLE%"=="" if not "%SIT_WRAPPER_BACKEND%"=="" (
    set "SIT_WRAPPER_EXAMPLE_OBJ_DIR=%SIT_WRAPPER_OBJ_BASE%\%SIT_WRAPPER_EXAMPLE%_%SIT_WRAPPER_BACKEND%"
)
goto :done

:done
exit /b 0