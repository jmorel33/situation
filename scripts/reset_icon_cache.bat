@echo off
REM ========================================================================
REM reset_icon_cache.bat
REM
REM Clears the Windows Explorer icon cache and restarts Explorer.
REM Useful after rebuilding DLLs with new embedded icons -- Windows caches
REM icons aggressively and won't pick up changes without a cache reset.
REM
REM Run this from anywhere. Does NOT require elevation on Windows 10/11.
REM
REM What it does:
REM   1. Kills Explorer.exe (desktop disappears briefly -- normal)
REM   2. Waits 1.5 seconds for Explorer to fully release cache file handles
REM   3. Deletes all iconcache_*.db files from %LOCALAPPDATA%
REM   4. Restarts Explorer
REM
REM After running: navigate to build\dll\ in Explorer with Large Icons view
REM to see the updated situation_opengl.dll / situation_vulkan.dll icons.
REM
REM (c) 2025-2026 Jacques Morel -- MIT Licensed
REM ========================================================================

echo Stopping Explorer...
taskkill /f /im explorer.exe >nul 2>&1

echo Waiting for file handles to release...
timeout /t 2 /nobreak >nul

echo Clearing icon cache...
del /f /q "%LOCALAPPDATA%\IconCache.db" >nul 2>&1
del /f /q "%LOCALAPPDATA%\Microsoft\Windows\Explorer\iconcache*" >nul 2>&1

echo Restarting Explorer...
start "" "C:\Windows\explorer.exe"

echo Done. Navigate to build\dll\ in Large Icons view to see DLL icons.
