@echo off
ECHO ==================================================================
ECHO  SITUATION SHADERC BUILD SCRIPT
ECHO ==================================================================
ECHO.

ECHO [1] Setting a clean, temporary PATH for this build...
REM This is the most important step. It puts the new compiler FIRST.
SET "PATH=C:\msys64\mingw64\bin;C:\Windows\system32;C:\Windows"

ECHO.
ECHO [2] Verifying compiler version...
g++ --version
ECHO.

ECHO [3] Preparing build directory...
IF EXIST build (
    ECHO    - Deleting old build directory...
    rmdir /s /q build
)
mkdir build
cd build
ECHO.

ECHO [4] Running CMake to configure the project...
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON ..
IF %ERRORLEVEL% NEQ 0 (
    ECHO.
    ECHO !!!!! CMAKE FAILED! See errors above. !!!!!
    PAUSE
    EXIT /B 1
)
ECHO.

ECHO [5] Compiling the library with mingw32-make...
mingw32-make
IF %ERRORLEVEL% NEQ 0 (
    ECHO.
    ECHO !!!!! BUILD FAILED! See errors above. !!!!!
    PAUSE
    EXIT /B 1
)
ECHO.

ECHO ==================================================================
ECHO  BUILD SUCCEEDED!
ECHO ==================================================================
ECHO Your library is located at:
ECHO C:\raylib\jj\console\ext\vulkan\shaderc\build\libshaderc\libshaderc_combined.a
ECHO.
PAUSE