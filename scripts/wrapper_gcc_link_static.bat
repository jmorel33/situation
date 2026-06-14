@echo off
REM Link wrapper object file(s) into a self-contained Situation exe.
REM Requires: SIT_LINK_BACKEND, SIT_STATIC_A, OUT_EXE
REM Provide either OBJ_FILE (single .o) or OBJ_GLOB (wildcard pattern).

setlocal enabledelayedexpansion

set GLFW_LIB=ext\glfw\build\src
set SHADERC_LIB=ext\shaderc\build\libshaderc
set LINK_ERR=0

set "OBJ_ARGS="
set "MERGED_OBJ="
if defined OBJ_GLOB (
    set "MERGED_OBJ=%TEMP%\sit_wrapper_objs_%RANDOM%.a"
    del "!MERGED_OBJ!" 2>nul
    for %%F in (%OBJ_GLOB%) do (
        ar q "!MERGED_OBJ!" "%%F"
        if errorlevel 1 set LINK_ERR=1
    )
    if not "!LINK_ERR!"=="0" (
        echo [ERROR] Failed to merge object files into static library
        goto :cleanup
    )
    set "OBJ_ARGS=!MERGED_OBJ!"
) else if defined OBJ_FILE (
    set "OBJ_ARGS=%OBJ_FILE%"
) else (
    echo [ERROR] wrapper_gcc_link_static.bat: OBJ_FILE or OBJ_GLOB required
    set LINK_ERR=1
    goto :cleanup
)

if /i "%SIT_LINK_BACKEND%"=="static-opengl" (
    gcc !OBJ_ARGS! ^
        -o "%OUT_EXE%" ^
        -static-libgcc ^
        -L%GLFW_LIB% ^
        -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
        "%SIT_STATIC_A%" ^
        -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
        -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
    set LINK_ERR=!ERRORLEVEL!
    goto :cleanup
)

if /i "%SIT_LINK_BACKEND%"=="static-vulkan" (
    g++ !OBJ_ARGS! ^
        -o "%OUT_EXE%" ^
        -static-libgcc -static-libstdc++ ^
        -L%GLFW_LIB% -L%SHADERC_LIB% -L"%VULKAN_SDK%\Lib" ^
        -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
        "%SIT_STATIC_A%" ^
        -lglfw3 -lvulkan-1 -lshaderc_combined ^
        -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
        -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
    set LINK_ERR=!ERRORLEVEL!
    goto :cleanup
)

echo [ERROR] wrapper_gcc_link_static.bat: unsupported backend %SIT_LINK_BACKEND%
set LINK_ERR=1

:cleanup
if defined MERGED_OBJ del "!MERGED_OBJ!" 2>nul
exit /b %LINK_ERR%
