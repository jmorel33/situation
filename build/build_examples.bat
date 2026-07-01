@echo off
REM ========================================================================
REM build_examples.bat - Build Situation examples
REM
REM Usage:
REM   build_examples.bat opengl        [example]   DLL-linked OpenGL
REM   build_examples.bat vulkan        [example]   DLL-linked Vulkan
REM   build_examples.bat static-opengl [example]   Static OpenGL (self-contained exe)
REM   build_examples.bat static-vulkan [example]   Static Vulkan (self-contained exe)
REM
REM   Use "all" as the example name to build every shipped digestible example:
REM     build_examples.bat opengl all
REM     build_examples.bat static-opengl all
REM     etc.
REM   (01-10, 18-21, 25, 27, kterm_console — see :build_all_numbered)
REM
REM Prerequisites (DLL modes):    build_situation.bat opengl / vulkan
REM Prerequisites (static modes): build_situation.bat static-opengl / static-vulkan
REM
REM NOTE: demon_hunt is Vulkan-only. Use vulkan or static-vulkan for it.
REM
REM Examples:
REM   build_examples.bat opengl        kterm_console
REM   build_examples.bat opengl        quad_storm
REM   build_examples.bat vulkan        demon_hunt
REM   build_examples.bat static-vulkan demon_hunt
REM   build_examples.bat static-opengl all     (builds 01-10, 18-21, 25, 27)
REM
REM (c) 2025-2026 Jacques Morel - MIT Licensed
REM ========================================================================

setlocal enabledelayedexpansion

REM K-Term grid client path (Phase F): default ON; set KTERM_USE_SIT_GRID=0 for legacy terminal.comp baseline.
set "KTERM_GRID_CFLAGS=-DKTERM_USE_SIT_GRID=1"
if /i "%KTERM_USE_SIT_GRID%"=="0" set "KTERM_GRID_CFLAGS=-DKTERM_USE_SIT_GRID=0"

REM --- Change to project root (one level up from build/) ---
cd /d "%~dp0.."

REM --- Configuration ---
set BUILD_DIR=build\examples
set DLL_DIR=build\dll
set GLFW_LIB=ext\glfw\build\src
set SHADERC_LIB=ext\shaderc\build\libshaderc

REM --- Parse Arguments ---
if "%~1"=="" goto :usage
if "%~2"=="" goto :usage
set BACKEND=%~1
set EXAMPLE=%~2

REM --- "all" builds every shipped digestible example for the given backend ---
if /i "%EXAMPLE%"=="all" (
    call :build_all_numbered %BACKEND%
    exit /b %errorlevel%
)

REM --- demon_hunt is Vulkan-only ---
if /i "%EXAMPLE%"=="demon_hunt" (
    if /i "%BACKEND%"=="opengl" (
        echo [ERROR] demon_hunt is Vulkan-only. The sky shader exceeds OpenGL SPIR-V instruction limits.
        echo         Use: build_examples.bat vulkan demon_hunt
        echo          or: build_examples.bat static-vulkan demon_hunt
        exit /b 1
    )
    if /i "%BACKEND%"=="static-opengl" (
        echo [ERROR] demon_hunt is Vulkan-only.
        echo         Use: build_examples.bat static-vulkan demon_hunt
        exit /b 1
    )
)

REM --- Resolve MinGW Path ---
if defined MINGW_PATH (
    set "PATH=%MINGW_PATH%;%PATH%"
) else (
    if exist "C:\msys64\mingw64\bin\gcc.exe" (
        set "PATH=C:\msys64\mingw64\bin;%PATH%"
    ) else (
        echo [ERROR] MinGW not found. Set MINGW_PATH or install MSYS2.
        exit /b 1
    )
)

REM --- Verify GCC ---
gcc --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] gcc not found in PATH.
    exit /b 1
)
for /f "tokens=*" %%i in ('gcc -dumpversion') do set GCC_VER=%%i

REM --- Create output directory ---
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM --- Compile application icon resource (version from situation_base_version.h) ---
set "APP_ICON_OBJ=%BUILD_DIR%\sit_app_icon.o"
if defined SIT_APP_RC (
    set "APP_RC=%SIT_APP_RC%"
) else (
    set "APP_RC=sit\platform\windows\sit_app.rc"
)
set "SIT_WINDRES_VER="
for /f "usebackq delims=" %%V in (`python scripts\read_situation_version.py --windres 2^>nul`) do set "SIT_WINDRES_VER=%%V"
if not defined SIT_WINDRES_VER (
    for /f "usebackq delims=" %%V in (`C:\msys64\mingw64\bin\python3.exe scripts\read_situation_version.py --windres 2^>nul`) do set "SIT_WINDRES_VER=%%V"
)
if not defined SIT_WINDRES_VER (
    echo [ERROR] Could not read version from sit\situation_base_version.h
    exit /b 1
)
windres "%APP_RC%" -o "%APP_ICON_OBJ%" --include-dir sit\platform\windows !SIT_WINDRES_VER! 2>nul
if errorlevel 1 (
    echo [WARN] windres failed -- exe will be built without embedded icon.
    set "APP_ICON_OBJ="
)

REM --- Resolve source file path ---
set "EXAMPLE_SRC="

REM Special named subdirectories
if /i "%EXAMPLE%"=="demon_hunt"    set "EXAMPLE_SRC=examples\demon_hunt\demon_hunt.c"
if /i "%EXAMPLE%"=="kterm_console" set "EXAMPLE_SRC=examples\console\console_host_app.c"

REM Numbered digestible examples — full folder name OR short name (without NN_ prefix)
if not defined EXAMPLE_SRC (
    if exist "examples\01_open_a_window\main.c" (
        if /i "%EXAMPLE%"=="01_open_a_window" set "EXAMPLE_SRC=examples\01_open_a_window\main.c"
        if /i "%EXAMPLE%"=="open_a_window"    set "EXAMPLE_SRC=examples\01_open_a_window\main.c"
    )
    if exist "examples\02_draw_shapes\main.c" (
        if /i "%EXAMPLE%"=="02_draw_shapes" set "EXAMPLE_SRC=examples\02_draw_shapes\main.c"
        if /i "%EXAMPLE%"=="draw_shapes"    set "EXAMPLE_SRC=examples\02_draw_shapes\main.c"
    )
    if exist "examples\03_keyboard_and_mouse\main.c" (
        if /i "%EXAMPLE%"=="03_keyboard_and_mouse" set "EXAMPLE_SRC=examples\03_keyboard_and_mouse\main.c"
        if /i "%EXAMPLE%"=="keyboard_and_mouse"    set "EXAMPLE_SRC=examples\03_keyboard_and_mouse\main.c"
    )
    if exist "examples\04_play_a_sound\main.c" (
        if /i "%EXAMPLE%"=="04_play_a_sound" set "EXAMPLE_SRC=examples\04_play_a_sound\main.c"
        if /i "%EXAMPLE%"=="play_a_sound"    set "EXAMPLE_SRC=examples\04_play_a_sound\main.c"
    )
    if exist "examples\05_virtual_display_retro\main.c" (
        if /i "%EXAMPLE%"=="05_virtual_display_retro" set "EXAMPLE_SRC=examples\05_virtual_display_retro\main.c"
        if /i "%EXAMPLE%"=="virtual_display_retro"    set "EXAMPLE_SRC=examples\05_virtual_display_retro\main.c"
    )
    if exist "examples\06_audio_node_graph\main.c" (
        if /i "%EXAMPLE%"=="06_audio_node_graph" set "EXAMPLE_SRC=examples\06_audio_node_graph\main.c"
        if /i "%EXAMPLE%"=="audio_node_graph"    set "EXAMPLE_SRC=examples\06_audio_node_graph\main.c"
    )
    if exist "examples\07_ypq_color_grading\main.c" (
        if /i "%EXAMPLE%"=="07_ypq_color_grading" set "EXAMPLE_SRC=examples\07_ypq_color_grading\main.c"
        if /i "%EXAMPLE%"=="ypq_color_grading"    set "EXAMPLE_SRC=examples\07_ypq_color_grading\main.c"
    )
    if exist "examples\08_temporal_oscillators\main.c" (
        if /i "%EXAMPLE%"=="08_temporal_oscillators" set "EXAMPLE_SRC=examples\08_temporal_oscillators\main.c"
        if /i "%EXAMPLE%"=="temporal_oscillators"    set "EXAMPLE_SRC=examples\08_temporal_oscillators\main.c"
    )
    if exist "examples\09_midi_control\main.c" (
        if /i "%EXAMPLE%"=="09_midi_control" set "EXAMPLE_SRC=examples\09_midi_control\main.c"
        if /i "%EXAMPLE%"=="midi_control"    set "EXAMPLE_SRC=examples\09_midi_control\main.c"
    )
    if exist "examples\10_thread_pool\main.c" (
        if /i "%EXAMPLE%"=="10_thread_pool" set "EXAMPLE_SRC=examples\10_thread_pool\main.c"
        if /i "%EXAMPLE%"=="thread_pool"    set "EXAMPLE_SRC=examples\10_thread_pool\main.c"
    )
    if exist "examples\11_music_visualizer\main.c" (
        if /i "%EXAMPLE%"=="11_music_visualizer" set "EXAMPLE_SRC=examples\11_music_visualizer\main.c"
        if /i "%EXAMPLE%"=="music_visualizer"    set "EXAMPLE_SRC=examples\11_music_visualizer\main.c"
    )
    if exist "examples\12_procedural_world\main.c" (
        if /i "%EXAMPLE%"=="12_procedural_world" set "EXAMPLE_SRC=examples\12_procedural_world\main.c"
        if /i "%EXAMPLE%"=="procedural_world"    set "EXAMPLE_SRC=examples\12_procedural_world\main.c"
    )
    if exist "examples\18_text_showcase\main.c" (
        if /i "%EXAMPLE%"=="18_text_showcase" set "EXAMPLE_SRC=examples\18_text_showcase\main.c"
        if /i "%EXAMPLE%"=="text_showcase"    set "EXAMPLE_SRC=examples\18_text_showcase\main.c"
    )
    if exist "examples\19_node_graph_piano\main.c" (
        if /i "%EXAMPLE%"=="19_node_graph_piano" set "EXAMPLE_SRC=examples\19_node_graph_piano\main.c"
        if /i "%EXAMPLE%"=="node_graph_piano"    set "EXAMPLE_SRC=examples\19_node_graph_piano\main.c"
    )
    if exist "examples\20_load_and_draw_model\main.c" (
        if /i "%EXAMPLE%"=="20_load_and_draw_model" set "EXAMPLE_SRC=examples\20_load_and_draw_model\main.c"
        if /i "%EXAMPLE%"=="load_and_draw_model"    set "EXAMPLE_SRC=examples\20_load_and_draw_model\main.c"
    )
    if exist "examples\21_ocean_realistic\main.c" (
        if /i "%EXAMPLE%"=="21_ocean_realistic" set "EXAMPLE_SRC=examples\21_ocean_realistic\main.c"
        if /i "%EXAMPLE%"=="ocean_realistic"    set "EXAMPLE_SRC=examples\21_ocean_realistic\main.c"
    )
    if exist "examples\25_vd_standby\main.c" (
        if /i "%EXAMPLE%"=="25_vd_standby" set "EXAMPLE_SRC=examples\25_vd_standby\main.c"
        if /i "%EXAMPLE%"=="vd_standby"    set "EXAMPLE_SRC=examples\25_vd_standby\main.c"
    )
    if exist "examples\27_grid_playfield\main.c" (
        if /i "%EXAMPLE%"=="27_grid_playfield" set "EXAMPLE_SRC=examples\27_grid_playfield\main.c"
        if /i "%EXAMPLE%"=="grid_playfield"    set "EXAMPLE_SRC=examples\27_grid_playfield\main.c"
    )
)

REM Legacy flat locations (keep backward-compatibility)
if not defined EXAMPLE_SRC (
    if exist "examples\other\%EXAMPLE%.c" set "EXAMPLE_SRC=examples\other\%EXAMPLE%.c"
)
if not defined EXAMPLE_SRC (
    if exist "examples\%EXAMPLE%.c" set "EXAMPLE_SRC=examples\%EXAMPLE%.c"
)

REM --- Unknown example ---
if not defined EXAMPLE_SRC (
    echo [ERROR] Unknown example: %EXAMPLE%
    goto :usage
)

REM --- Check source file exists ---
if not exist "%EXAMPLE_SRC%" (
    echo [ERROR] Source file not found: %EXAMPLE_SRC%
    exit /b 1
)

REM --- Dispatch ---
if /i "%BACKEND%"=="opengl" (
    if /i "%EXAMPLE%"=="kterm_console" (
        echo [NOTE] kterm_console is monolithic ^(SITUATION_IMPLEMENTATION^) — using static-opengl link.
        goto :build_static_opengl
    )
    goto :build_opengl
)
if /i "%BACKEND%"=="vulkan" (
    if /i "%EXAMPLE%"=="kterm_console" (
        echo [NOTE] kterm_console is monolithic ^(SITUATION_IMPLEMENTATION^) — using static-vulkan link.
        goto :build_static_vulkan
    )
    goto :build_vulkan
)
if /i "%BACKEND%"=="static-opengl" goto :build_static_opengl
if /i "%BACKEND%"=="static-vulkan" goto :build_static_vulkan
goto :usage

REM ========================================================================
REM BUILD: OpenGL DLL-linked
REM ========================================================================
:build_opengl

if not exist "%DLL_DIR%\situation_opengl.dll" (
    echo [ERROR] situation_opengl.dll not found in %DLL_DIR%
    echo         Run: build\build_situation.bat opengl
    exit /b 1
)

echo.
echo [BUILD] %EXAMPLE% (OpenGL, DLL) - GCC %GCC_VER%
echo         Linking against: %DLL_DIR%\situation_opengl.dll
echo.

set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="kterm_console"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="platformer_plumber"    set "EXTRA_LDFLAGS=-mwindows"
REM Numbered digestible examples are windowed apps
if /i "%EXAMPLE%"=="11_music_visualizer"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="12_procedural_world"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="music_visualizer"      set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="procedural_world"      set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="18_text_showcase"       set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="19_node_graph_piano"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="20_load_and_draw_model" set "EXTRA_LDFLAGS=-mwindows"
REM 21_ocean_realistic: keep console subsystem for shader init errors
if /i "%EXAMPLE%"=="text_showcase"          set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="node_graph_piano"       set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="load_and_draw_model"    set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="25_vd_standby"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="vd_standby"            set "EXTRA_LDFLAGS=-mwindows"


gcc %EXAMPLE_SRC% %APP_ICON_OBJ% ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Isit -Iexamples -Iexamples/console -Iexamples/shared -Iexamples/other ^
    -Iext -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Isit/k-term ^
    -Ibuild/opengl ^
    -DSITUATION_USE_OPENGL -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING ^
    %KTERM_GRID_CFLAGS% ^
    -L%DLL_DIR% -lsituation_opengl ^
    -static-libgcc %EXTRA_LDFLAGS% -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

copy /Y "%DLL_DIR%\situation_opengl.dll" "%BUILD_DIR%\situation_opengl.dll" >nul
echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe
exit /b 0

REM ========================================================================
REM BUILD: Vulkan DLL-linked
REM ========================================================================
:build_vulkan

if not exist "%DLL_DIR%\situation_vulkan.dll" (
    echo [ERROR] situation_vulkan.dll not found in %DLL_DIR%
    echo         Run: build\build_situation.bat vulkan
    exit /b 1
)

if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK ( echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK. & exit /b 1 )

echo.
echo [BUILD] %EXAMPLE% (Vulkan, DLL) - GCC %GCC_VER%
echo         Linking against: %DLL_DIR%\situation_vulkan.dll
echo.

set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="demon_hunt"            set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="kterm_console"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="platformer_plumber"    set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="11_music_visualizer"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="12_procedural_world"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="music_visualizer"      set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="procedural_world"      set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="18_text_showcase"       set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="19_node_graph_piano"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="20_load_and_draw_model" set "EXTRA_LDFLAGS=-mwindows"
REM 21_ocean_realistic: keep console subsystem for shader init errors
if /i "%EXAMPLE%"=="text_showcase"          set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="node_graph_piano"       set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="load_and_draw_model"    set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="25_vd_standby"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="vd_standby"            set "EXTRA_LDFLAGS=-mwindows"


REM demon_hunt: precompile shaders and include SPIR-V embed
set "DH_EMBED_SRC="
if /i "%EXAMPLE%"=="demon_hunt" (
    call "%~dp0compile_demon_hunt_shaders.bat"
    if errorlevel 1 echo [WARN] Shader precompile failed — game needs .spv at launch.
    set "DH_EMBED_SRC=examples/demon_hunt/demon_hunt_sky_spirv_embed.c"
)

gcc %EXAMPLE_SRC% %DH_EMBED_SRC% %APP_ICON_OBJ% ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Isit -Iexamples -Iexamples/console -Iexamples/shared -Iexamples/other ^
    -Iext -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Isit/k-term ^
    -I"%VULKAN_SDK%\Include" ^
    -DSITUATION_USE_VULKAN -DSITUATION_USE_SHARED -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER ^
    %KTERM_GRID_CFLAGS% ^
    -L%DLL_DIR% -lsituation_vulkan ^
    -static-libgcc %EXTRA_LDFLAGS% -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

copy /Y "%DLL_DIR%\situation_vulkan.dll" "%BUILD_DIR%\situation_vulkan.dll" >nul
echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe
exit /b 0

REM ========================================================================
REM BUILD: OpenGL static (self-contained exe)
REM ========================================================================
:build_static_opengl

set STATIC_LIB=build\dll\situation_opengl.a
if not exist "%STATIC_LIB%" (
    echo [ERROR] situation_opengl.a not found in build\dll
    echo         Run: build\build_situation.bat static-opengl
    exit /b 1
)

echo.
echo [BUILD] %EXAMPLE% (OpenGL, static) - GCC %GCC_VER%
echo         Linking against: %STATIC_LIB%
echo.

set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="kterm_console"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="platformer_plumber"    set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="11_music_visualizer"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="12_procedural_world"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="music_visualizer"      set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="procedural_world"      set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="18_text_showcase"       set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="19_node_graph_piano"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="20_load_and_draw_model" set "EXTRA_LDFLAGS=-mwindows"
REM 21_ocean_realistic: keep console subsystem for shader init errors
if /i "%EXAMPLE%"=="text_showcase"          set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="node_graph_piano"       set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="load_and_draw_model"    set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="25_vd_standby"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="vd_standby"            set "EXTRA_LDFLAGS=-mwindows"


gcc %EXAMPLE_SRC% %APP_ICON_OBJ% ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Isit -Iexamples -Iexamples/console -Iexamples/shared -Iexamples/other ^
    -Iext -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Isit/k-term ^
    -Ibuild/opengl ^
    -DSITUATION_USE_OPENGL -DSITUATION_ENABLE_THREADING ^
    %KTERM_GRID_CFLAGS% ^
    -L%GLFW_LIB% -static-libgcc %EXTRA_LDFLAGS% ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    "%STATIC_LIB%" ^
    -lglfw3 -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm

if errorlevel 1 ( echo [FAILED] Compilation failed! & exit /b 1 )

echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe  (self-contained, no DLL needed)
exit /b 0

REM ========================================================================
REM BUILD: Vulkan static (self-contained exe)
REM ========================================================================
:build_static_vulkan

set STATIC_LIB=build\dll\situation_vulkan.a
if not exist "%STATIC_LIB%" (
    echo [ERROR] situation_vulkan.a not found in build\dll
    echo         Run: build\build_situation.bat static-vulkan
    exit /b 1
)

if not defined VULKAN_SDK (
    for /d %%d in ("C:\VulkanSDK\*") do (
        if exist "%%d\Include\vulkan\vulkan.h" set "VULKAN_SDK=%%d"
    )
)
if not defined VULKAN_SDK ( echo [ERROR] Vulkan SDK not found. Set VULKAN_SDK. & exit /b 1 )

echo.
echo [BUILD] %EXAMPLE% (Vulkan, static) - GCC %GCC_VER%
echo         Linking against: %STATIC_LIB%
echo.

set "EXTRA_LDFLAGS="
if /i "%EXAMPLE%"=="node_graph_piano_demo" set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="demon_hunt"            set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="kterm_console"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="platformer_plumber"    set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="11_music_visualizer"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="12_procedural_world"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="music_visualizer"      set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="procedural_world"      set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="18_text_showcase"       set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="19_node_graph_piano"   set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="20_load_and_draw_model" set "EXTRA_LDFLAGS=-mwindows"
REM 21_ocean_realistic: keep console subsystem for shader init errors
if /i "%EXAMPLE%"=="text_showcase"          set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="node_graph_piano"       set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="load_and_draw_model"    set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="25_vd_standby"         set "EXTRA_LDFLAGS=-mwindows"
if /i "%EXAMPLE%"=="vd_standby"            set "EXTRA_LDFLAGS=-mwindows"


set "DH_EMBED_SRC="
if /i "%EXAMPLE%"=="demon_hunt" (
    call "%~dp0compile_demon_hunt_shaders.bat"
    if errorlevel 1 echo [WARN] Shader precompile failed.
    set "DH_EMBED_SRC=examples/demon_hunt/demon_hunt_sky_spirv_embed.c"
)

REM g++ needed for shaderc/VMA C++ runtime in the static archive
REM Step 1: compile the C source with gcc
gcc -c %EXAMPLE_SRC% ^
    -o %BUILD_DIR%\%EXAMPLE%_main.o ^
    -std=c11 -O2 -msse -msse2 -msse4.1 ^
    -I. -Isit -Iexamples -Iexamples/console -Iexamples/shared -Iexamples/other ^
    -Iext -Iext/cgltf -Iext/cglm/include -Iext/glfw/include -Isit/k-term ^
    -I"%VULKAN_SDK%\Include" ^
    -DSITUATION_USE_VULKAN -DSITUATION_ENABLE_THREADING -DSITUATION_ENABLE_SHADER_COMPILER

if errorlevel 1 ( echo [FAILED] C compilation failed! & exit /b 1 )

REM Step 2: compile SPIR-V embed (if present) with gcc
set "DH_EMBED_OBJ="
if defined DH_EMBED_SRC (
    gcc -c %DH_EMBED_SRC% ^
        -o %BUILD_DIR%\%EXAMPLE%_embed.o ^
        -std=c11 -O2 ^
        -I. -Isit -Iexamples/console -Iexamples/shared -Iexamples/other ^
        -Iext -Iext/cgltf -Iext/cglm/include -Isit/k-term ^
        -DSITUATION_USE_VULKAN
    if errorlevel 1 ( echo [FAILED] SPIR-V embed compilation failed! & exit /b 1 )
    set "DH_EMBED_OBJ=%BUILD_DIR%\%EXAMPLE%_embed.o"
)

REM Step 3: link with g++ to pull in the C++ runtime needed by the static archive
g++ %BUILD_DIR%\%EXAMPLE%_main.o %DH_EMBED_OBJ% %APP_ICON_OBJ% ^
    -o %BUILD_DIR%/%EXAMPLE%.exe ^
    -L%GLFW_LIB% -L%SHADERC_LIB% -L"%VULKAN_SDK%\Lib" ^
    -static-libgcc -static-libstdc++ %EXTRA_LDFLAGS% ^
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive ^
    "%STATIC_LIB%" ^
    -lglfw3 -lvulkan-1 -lshaderc_combined ^
    -lgdi32 -lwinmm -luser32 -lshell32 -lole32 ^
    -liphlpapi -lsetupapi -ldxgi -lpropsys -lshlwapi -luuid -lxinput -lws2_32 -lpsapi -lm

if errorlevel 1 ( echo [FAILED] Link failed! & exit /b 1 )

REM Cleanup intermediates
del "%BUILD_DIR%\%EXAMPLE%_main.o" 2>nul
del "%BUILD_DIR%\%EXAMPLE%_embed.o" 2>nul

echo [SUCCESS] %BUILD_DIR%\%EXAMPLE%.exe  (self-contained, no DLL needed)
exit /b 0

REM ========================================================================
:build_all_numbered
set "ALL_BACKEND=%~1"
if "%ALL_BACKEND%"=="" goto :usage

echo.
echo [ALL] Building all digestible examples for backend: %ALL_BACKEND%
echo.

for %%E in (
    01_open_a_window
    02_draw_shapes
    03_keyboard_and_mouse
    04_play_a_sound
    05_virtual_display_retro
    06_audio_node_graph
    07_ypq_color_grading
    08_temporal_oscillators
    09_midi_control
    10_thread_pool
    18_text_showcase
    19_node_graph_piano
    20_load_and_draw_model
    21_ocean_realistic
    25_vd_standby
    27_grid_playfield
    kterm_console
) do (
    echo.
    echo === [ALL] %%E ===
    call "%~f0" %ALL_BACKEND% %%E
    if errorlevel 1 (
        echo [ALL] Build failed for %%E
        exit /b 1
    )
)

echo.
echo [ALL] Successfully built all digestible examples for %ALL_BACKEND%
exit /b 0

REM ========================================================================
:usage
echo.
echo Usage: build_examples.bat [backend] [example]
echo.
echo Backends:
echo   opengl          DLL-linked    (needs situation_opengl.dll next to exe)
echo   vulkan          DLL-linked    (needs situation_vulkan.dll next to exe)
echo   static-opengl   Self-contained exe, no DLL required
echo   static-vulkan   Self-contained exe, no DLL required
echo.
echo Numbered digestible examples (OpenGL or Vulkan):
echo   build_examples.bat static-opengl  01_open_a_window         (or: open_a_window)
echo   build_examples.bat static-opengl  02_draw_shapes           (or: draw_shapes)
echo   build_examples.bat static-opengl  03_keyboard_and_mouse    (or: keyboard_and_mouse)
echo   build_examples.bat static-opengl  04_play_a_sound          (or: play_a_sound)
echo   build_examples.bat static-opengl  05_virtual_display_retro (or: virtual_display_retro)
echo   build_examples.bat static-opengl  06_audio_node_graph      (or: audio_node_graph)
echo   build_examples.bat static-opengl  07_ypq_color_grading     (or: ypq_color_grading)
echo   build_examples.bat static-opengl  08_temporal_oscillators  (or: temporal_oscillators)
echo   build_examples.bat static-opengl  09_midi_control          (or: midi_control)
echo   build_examples.bat static-opengl  10_thread_pool           (or: thread_pool)
echo   build_examples.bat static-opengl  18_text_showcase         (or: text_showcase)
echo   build_examples.bat static-opengl  19_node_graph_piano      (or: node_graph_piano)
echo   build_examples.bat static-opengl  20_load_and_draw_model   (or: load_and_draw_model)
echo   build_examples.bat static-opengl  21_ocean_realistic       (or: ocean_realistic)
echo   build_examples.bat static-opengl  25_vd_standby            (or: vd_standby)
echo.
echo All digestible examples at once:
echo   build_examples.bat opengl all
echo   build_examples.bat vulkan all
echo   build_examples.bat static-opengl all
echo   build_examples.bat static-vulkan all
echo.
echo Named examples:
echo   build_examples.bat static-opengl  node_graph_piano_demo
echo   build_examples.bat opengl         kterm_console
echo   build_examples.bat opengl         platformer_plumber
echo   build_examples.bat opengl         quad_storm
echo   build_examples.bat static-vulkan  demon_hunt               ^(Vulkan-only^)
echo.
echo Prerequisites:
echo   opengl / vulkan          ^> build\build_situation.bat opengl ^(or vulkan^)
echo   static-opengl            ^> build\build_situation.bat static-opengl
echo   static-vulkan            ^> build\build_situation.bat static-vulkan
echo.
exit /b 1
