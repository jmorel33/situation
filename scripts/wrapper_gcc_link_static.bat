@echo off
REM Link wrapper object file(s) into a self-contained Situation exe.
REM
REM Requires: SIT_LINK_BACKEND, SIT_STATIC_A, OUT_EXE
REM Provide OBJ_FILE (single .o), OBJ_LIST (space-separated .o files), or OBJ_GLOB (wildcard pattern(s)).

setlocal enabledelayedexpansion

set GLFW_LIB=ext\glfw\build\src
set SHADERC_LIB=ext\shaderc\build\libshaderc
set LINK_ERR=0

set "OBJ_ARGS="
set "MERGED_OBJ="

if defined OBJ_GLOB (
    set "MERGED_OBJ=%TEMP%\sit_wrapper_objs_%RANDOM%.a"
    del "!MERGED_OBJ!" 2>nul
    for %%P in (%OBJ_GLOB%) do (
        for %%F in (%%P) do (
            ar q "!MERGED_OBJ!" "%%F"
            if errorlevel 1 set LINK_ERR=1
        )
    )
    if not "!LINK_ERR!"=="0" (
        echo [ERROR] Failed to merge object files into static library
        goto :cleanup
    )
    set "OBJ_ARGS=!MERGED_OBJ!"
) else if defined OBJ_LIST (
    set "OBJ_ARGS=!OBJ_LIST!"
) else if defined OBJ_FILE (
    set "OBJ_ARGS=%OBJ_FILE%"
) else (
    echo [ERROR] wrapper_gcc_link_static.bat: OBJ_FILE, OBJ_LIST, or OBJ_GLOB required
    set LINK_ERR=1
    goto :cleanup
)

set "EXTRA_PRE="
if defined SIT_EXTRA_LDFLAGS set "EXTRA_PRE=!SIT_EXTRA_LDFLAGS! "

set "EXPORT_ARG="
if defined SIT_EXPORT_DEF set "EXPORT_ARG=-Wl,%SIT_EXPORT_DEF%"

if defined SIT_WHOLE_ARCHIVE_STATIC (
    set "STATIC_LIB=-Wl,-Bstatic,--whole-archive "%SIT_STATIC_A%" -Wl,--no-whole-archive"
) else (
    set "STATIC_LIB="%SIT_STATIC_A%""
)

if /i "%SIT_LINK_BACKEND%"=="static-opengl" (
    if defined SIT_FORTRAN_LINK (
        gfortran !OBJ_ARGS! -o "%OUT_EXE%" !EXTRA_PRE!!EXPORT_ARG! -static-libgcc -static-libgfortran -L%GLFW_LIB% -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive !STATIC_LIB! -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
    ) else if defined SIT_LINK_DRIVER (
        %SIT_LINK_DRIVER% !OBJ_ARGS! -o "%OUT_EXE%" !EXTRA_PRE!!EXPORT_ARG! -static-libgcc -L%GLFW_LIB% -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive !STATIC_LIB! -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
    ) else (
        gcc !OBJ_ARGS! -o "%OUT_EXE%" !EXTRA_PRE!!EXPORT_ARG! -static-libgcc -L%GLFW_LIB% -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive !STATIC_LIB! -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
    )
    set LINK_ERR=!ERRORLEVEL!
    goto :cleanup
)

if /i "%SIT_LINK_BACKEND%"=="static-vulkan" (
    if defined SIT_FORTRAN_LINK (
        g++ !OBJ_ARGS! -o "%OUT_EXE%" !EXTRA_PRE!-static-libgcc -static-libgfortran -static-libstdc++ -lgfortran -L%GLFW_LIB% -L%SHADERC_LIB% -L"%VULKAN_SDK%\Lib" -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive !STATIC_LIB! -lglfw3 -lvulkan-1 -lshaderc_combined -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
    ) else if defined SIT_LINK_DRIVER (
        %SIT_LINK_DRIVER% !OBJ_ARGS! -o "%OUT_EXE%" !EXTRA_PRE!-static-libgcc -static-libstdc++ -L%GLFW_LIB% -L%SHADERC_LIB% -L"%VULKAN_SDK%\Lib" -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive !STATIC_LIB! -lglfw3 -lvulkan-1 -lshaderc_combined -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
    ) else (
        g++ !OBJ_ARGS! -o "%OUT_EXE%" !EXTRA_PRE!-static-libgcc -static-libstdc++ -L%GLFW_LIB% -L%SHADERC_LIB% -L"%VULKAN_SDK%\Lib" -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive !STATIC_LIB! -lglfw3 -lvulkan-1 -lshaderc_combined -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm
    )
    set LINK_ERR=!ERRORLEVEL!
    goto :cleanup
)

echo [ERROR] wrapper_gcc_link_static.bat: unsupported backend %SIT_LINK_BACKEND%
set LINK_ERR=1

:cleanup
if defined MERGED_OBJ del "!MERGED_OBJ!" 2>nul
exit /b %LINK_ERR%