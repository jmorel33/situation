@echo off
REM ========================================================================
REM wrapper_link_config.bat - Shared Situation link settings for language wrappers
REM
REM Usage: call scripts\wrapper_link_config.bat [backend]
REM   opengl | vulkan | static-opengl | static-vulkan
REM
REM Sets (when called with valid backend):
REM   SIT_LINK_BACKEND, SIT_DLL_BASENAME, SIT_DLL_SRC, SIT_STATIC_A, SIT_IMPORT_LIB
REM   SIT_NEEDS_DLL_COPY (0|1), SIT_ODIN_FOREIGN_IMPORT, SIT_ODIN_EXTRA_LDFLAGS
REM   SIT_ZIG_LINK_FLAG, SIT_RUST_LINK_ENV
REM Returns errorlevel 1 on unknown backend.
REM ========================================================================

set "SIT_LINK_BACKEND=%~1"
set "SIT_NEEDS_DLL_COPY=0"
set "SIT_ODIN_EXTRA_LDFLAGS="
set "SIT_GLFW_LIB=ext\glfw\build\src"
set "SIT_SHADERC_LIB=ext\shaderc\build\libshaderc"

if /i "%SIT_LINK_BACKEND%"=="opengl" goto :dll_opengl
if /i "%SIT_LINK_BACKEND%"=="vulkan" goto :dll_vulkan
if /i "%SIT_LINK_BACKEND%"=="static-opengl" goto :static_opengl
if /i "%SIT_LINK_BACKEND%"=="static-vulkan" goto :static_vulkan
exit /b 1

:dll_opengl
set "SIT_DLL_BASENAME=situation_opengl"
set "SIT_DLL_SRC=build\dll\%SIT_DLL_BASENAME%.dll"
set "SIT_STATIC_A=build\dll\%SIT_DLL_BASENAME%.a"
set "SIT_IMPORT_LIB=build\dll\%SIT_DLL_BASENAME%.lib"
set "SIT_NEEDS_DLL_COPY=1"
set "SIT_ODIN_FOREIGN_IMPORT=../../build/dll/%SIT_DLL_BASENAME%.lib"
set "SIT_ZIG_LINK_FLAG=opengl"
set "SIT_RUST_LINK_ENV=opengl"
exit /b 0

:dll_vulkan
set "SIT_DLL_BASENAME=situation_vulkan"
set "SIT_DLL_SRC=build\dll\%SIT_DLL_BASENAME%.dll"
set "SIT_STATIC_A=build\dll\%SIT_DLL_BASENAME%.a"
set "SIT_IMPORT_LIB=build\dll\%SIT_DLL_BASENAME%.lib"
set "SIT_NEEDS_DLL_COPY=1"
set "SIT_ODIN_FOREIGN_IMPORT=../../build/dll/%SIT_DLL_BASENAME%.lib"
set "SIT_ZIG_LINK_FLAG=vulkan"
set "SIT_RUST_LINK_ENV=vulkan"
exit /b 0

:static_opengl
set "SIT_DLL_BASENAME=situation_opengl"
set "SIT_DLL_SRC=build\dll\%SIT_DLL_BASENAME%.dll"
set "SIT_STATIC_A=build\dll\%SIT_DLL_BASENAME%.a"
set "SIT_IMPORT_LIB=build\dll\%SIT_DLL_BASENAME%.lib"
set "SIT_ODIN_FOREIGN_IMPORT=../../build/dll/%SIT_DLL_BASENAME%.a"
call :odin_static_ldflags_opengl
set "SIT_ZIG_LINK_FLAG=static-opengl"
set "SIT_RUST_LINK_ENV=static-opengl"
exit /b 0

:static_vulkan
if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK (
    echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK.
    exit /b 1
)
set "SIT_DLL_BASENAME=situation_vulkan"
set "SIT_DLL_SRC=build\dll\%SIT_DLL_BASENAME%.dll"
set "SIT_STATIC_A=build\dll\%SIT_DLL_BASENAME%.a"
set "SIT_IMPORT_LIB=build\dll\%SIT_DLL_BASENAME%.lib"
set "SIT_ODIN_FOREIGN_IMPORT=../../build/dll/%SIT_DLL_BASENAME%.a"
call :odin_static_ldflags_vulkan
set "SIT_ZIG_LINK_FLAG=static-vulkan"
set "SIT_RUST_LINK_ENV=static-vulkan"
exit /b 0

:odin_mingw_paths
set "SIT_MINGW_BIN=C:\msys64\mingw64\bin"
set "SIT_MINGW_LIB=C:\msys64\mingw64\lib"
set "SIT_MINGW_GCC_LIB=C:\msys64\mingw64\lib\gcc\x86_64-w64-mingw32"
for /f "delims=" %%G in ('where gcc 2^>nul') do (
    set "SIT_MINGW_BIN=%%~dpG"
    set "SIT_MINGW_LIB=%%~dpG..\lib"
    for /f "delims=" %%V in ('gcc -dumpversion 2^>nul') do set "SIT_MINGW_GCC_LIB=%%~dpG..\lib\gcc\x86_64-w64-mingw32\%%V"
    goto :odin_mingw_done
)
:odin_mingw_done
goto :eof

:odin_static_ldflags_opengl
call :odin_mingw_paths
REM lld-link format (MSVC-style flags) — kept for reference but not used for static
set "SIT_ODIN_EXTRA_LDFLAGS=/LIBPATH:%SIT_GLFW_LIB% /LIBPATH:%SIT_MINGW_LIB% /LIBPATH:%SIT_MINGW_GCC_LIB% libglfw3.a libmingwex.a libgcc.a libgcc_eh.a libwinpthread.a libm.a opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib ole32.lib iphlpapi.lib setupapi.lib dxgi.lib propsys.lib shlwapi.lib uuid.lib xinput.lib ws2_32.lib psapi.lib advapi32.lib"
REM GNU ld format — used by Odin static builds via -linker:<ld.exe path>
set "SIT_ODIN_GNU_LD=%SIT_MINGW_BIN%ld.exe"
set "SIT_ODIN_EXTRA_LDFLAGS_GNU=-L%SIT_GLFW_LIB% -L%SIT_MINGW_LIB% -L%SIT_MINGW_GCC_LIB% -lglfw3 -lmingwex -lgcc -lgcc_eh -lwinpthread -lm -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -ladvapi32"
goto :eof

:odin_static_ldflags_vulkan
call :odin_mingw_paths
set "SIT_ODIN_EXTRA_LDFLAGS=/LIBPATH:%SIT_GLFW_LIB% /LIBPATH:%SIT_SHADERC_LIB% /LIBPATH:%VULKAN_SDK%\Lib /LIBPATH:%SIT_MINGW_LIB% /LIBPATH:%SIT_MINGW_GCC_LIB% libglfw3.a libshaderc_combined.a vulkan-1.lib libmingwex.a libgcc.a libgcc_eh.a libwinpthread.a libm.a libstdc++.a gdi32.lib winmm.lib user32.lib shell32.lib ole32.lib iphlpapi.lib setupapi.lib dxgi.lib propsys.lib shlwapi.lib uuid.lib xinput.lib ws2_32.lib psapi.lib advapi32.lib"
set "SIT_ODIN_GNU_LD=%SIT_MINGW_BIN%g++.exe"
set "SIT_ODIN_EXTRA_LDFLAGS_GNU=-L%SIT_GLFW_LIB% -L%SIT_SHADERC_LIB% -L%VULKAN_SDK%\Lib -L%SIT_MINGW_LIB% -L%SIT_MINGW_GCC_LIB% -lglfw3 -lshaderc_combined -lvulkan-1 -lmingwex -lgcc -lgcc_eh -lwinpthread -lm -lstdc++ -lgdi32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -ladvapi32"
goto :eof
