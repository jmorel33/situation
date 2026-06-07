/***************************************************************************************************
*
*   situation_impl_wdm.h - Window & Display Module Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Extracted from situation_impl.h for modularity.
*   This file is included by situation_impl.h after situation_impl_input.h.
*
*   Contains:
*     - Physical display enumeration and caching
*     - Monitor queries (count, size, refresh rate, position)
*     - Window state queries and manipulation (fullscreen, size, position, etc.)
*     - Display mode setting
*     - Application lifecycle (pause/resume, target FPS, frame time)
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_WDM_H
#define SITUATION_IMPL_WDM_H
// --- Monitor Enum Proc for Physical Displays (Windows-specific) ---
#if defined(_WIN32)
typedef struct {
    SituationDisplayInfo* displays_array;
    int current_display_idx;
    int max_displays;
    GLFWmonitor** glfw_monitors;
    int glfw_monitor_count;
} _SituationMonitorEnumData;

/**
 * @brief [INTERNAL] Win32 callback function for enumerating and collecting physical display information.
 * @details This function is the core of the display enumeration logic on the Windows platform. It is designed to be passed as a callback to the Win32 API function `EnumDisplayMonitors`.
 *          For each monitor detected by the operating system, Windows invokes this function, providing a handle to the monitor.
 *
 * @par Function Workflow
 *   For each monitor passed to it, this function performs the following steps:
 *   1.  **Get Detailed Info:** Uses `GetMonitorInfoExA` to retrieve the monitor's unique device name (e.g., `\\.\DISPLAY1`) and determine if it is the primary display.
 *   2.  **Match with GLFW:** It attempts to correlate the Win32 monitor handle with a `GLFWmonitor` handle by comparing their screen coordinates. This is crucial for bridging the gap between the low-level OS information and the GLFW context used for windowing.
 *   3.  **Query Current Mode:** It uses `EnumDisplaySettingsA` to get the monitor's current resolution, refresh rate, and color depth.
 *   4.  **Enumerate All Modes:** It repeatedly calls `EnumDisplaySettingsA` in a loop to build a comprehensive, de-duplicated list of all video modes supported by the display.
 *   5.  **Store Data:** All of this collected information is stored in the next available `SituationDisplayInfo` struct within the array provided via the `dwData` parameter.
 *
 * @param hMonitor A handle to the display monitor, provided by the Win32 API.
 * @param hdcMonitor A handle to a device context for the monitor (unused).
 * @param lprcMonitor A pointer to a rectangle specifying the monitor's display area (unused).
 * @param dwData A user-defined value passed from the `EnumDisplayMonitors` call. In this library, it is a pointer to a `_SituationMonitorEnumData` struct, which contains the destination array and state for the enumeration process.
 *
 * @return `TRUE` to continue the enumeration to the next monitor. `FALSE` would halt the process.
 *
 * @note This function is platform-specific to Windows and is only compiled on `_WIN32`.
 * @warning This is a low-level callback and is not part of the public API. It should never be called directly.
 *
 * @see _SituationCachePhysicalDisplays(), EnumDisplayMonitors()
 */
static BOOL CALLBACK _SituationMonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    (void)hdcMonitor; (void)lprcMonitor;
    _SituationMonitorEnumData* enum_data = (_SituationMonitorEnumData*)dwData;
    if (enum_data->current_display_idx >= enum_data->max_displays) return TRUE;

    MONITORINFOEXA monitor_info_ex = {};
    monitor_info_ex.cbSize = sizeof(MONITORINFOEXA);
    if (GetMonitorInfoA(hMonitor, (LPMONITORINFO)&monitor_info_ex)) {
        SituationDisplayInfo* display = &enum_data->displays_array[enum_data->current_display_idx];
        strncpy(display->name, monitor_info_ex.szDevice, SITUATION_MAX_MONITOR_NAME_LEN-1);
        display->name[SITUATION_MAX_MONITOR_NAME_LEN-1] = '\0';
        display->is_primary = (monitor_info_ex.dwFlags & MONITORINFOF_PRIMARY) != 0;
        display->situation_monitor_id = enum_data->current_display_idx;
        display->glfw_monitor_handle = NULL;

        for (int i = 0; i < enum_data->glfw_monitor_count; ++i) {
            int gx, gy;
            glfwGetMonitorPos(enum_data->glfw_monitors[i], &gx, &gy);
            if (monitor_info_ex.rcMonitor.left == gx && monitor_info_ex.rcMonitor.top == gy) {
                display->glfw_monitor_handle = enum_data->glfw_monitors[i];
                break;
            }
        }
        if (!display->glfw_monitor_handle && display->is_primary) {
             display->glfw_monitor_handle = glfwGetPrimaryMonitor();
        }

        DEVMODEA current_dev_mode = { .dmSize = sizeof(DEVMODEA), .dmDriverExtra = 0 };
        if (EnumDisplaySettingsA(monitor_info_ex.szDevice, ENUM_CURRENT_SETTINGS, &current_dev_mode)) {
            display->current_mode.width = current_dev_mode.dmPelsWidth;
            display->current_mode.height = current_dev_mode.dmPelsHeight;
            display->current_mode.refresh_rate = current_dev_mode.dmDisplayFrequency;
            display->current_mode.color_depth = current_dev_mode.dmBitsPerPel;
        } else {memset(&display->current_mode, 0, sizeof(SituationDisplayMode));}

        #define TEMP_MAX_MODES_ENUM 256
        SituationDisplayMode temp_modes_buffer[TEMP_MAX_MODES_ENUM];
        int unique_modes_count = 0;
        DEVMODEA available_dev_mode = { .dmSize = sizeof(DEVMODEA), .dmDriverExtra = 0 };
        int mode_idx_counter = 0;

        while (EnumDisplaySettingsA(monitor_info_ex.szDevice, mode_idx_counter++, &available_dev_mode)) {
            if (available_dev_mode.dmBitsPerPel < 16) continue;
            bool is_duplicate = false;
            for (int k = 0; k < unique_modes_count; ++k) {
                if (temp_modes_buffer[k].width == (int)available_dev_mode.dmPelsWidth &&
                    temp_modes_buffer[k].height == (int)available_dev_mode.dmPelsHeight &&
                    temp_modes_buffer[k].refresh_rate == (int)available_dev_mode.dmDisplayFrequency &&
                    temp_modes_buffer[k].color_depth == (int)available_dev_mode.dmBitsPerPel) {
                    is_duplicate = true;
                    break;
                }
            }
            if (!is_duplicate) {
                if (unique_modes_count < TEMP_MAX_MODES_ENUM) {
                    temp_modes_buffer[unique_modes_count].width = available_dev_mode.dmPelsWidth;
                    temp_modes_buffer[unique_modes_count].height = available_dev_mode.dmPelsHeight;
                    temp_modes_buffer[unique_modes_count].refresh_rate = available_dev_mode.dmDisplayFrequency;
                    temp_modes_buffer[unique_modes_count].color_depth = available_dev_mode.dmBitsPerPel;
                    unique_modes_count++;
                } else { break; }
            }
            memset(&available_dev_mode, 0, sizeof(DEVMODEA));
            available_dev_mode.dmSize = sizeof(DEVMODEA);
        }

        if (unique_modes_count > 0) {
            display->available_modes = (SituationDisplayMode*)SIT_MALLOC(unique_modes_count * sizeof(SituationDisplayMode));
            if (display->available_modes) {
                memcpy(display->available_modes, temp_modes_buffer, unique_modes_count * sizeof(SituationDisplayMode));
                display->available_mode_count = unique_modes_count;
            } else {
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Available display modes array");
                display->available_mode_count = 0;
            }
        } else {
            display->available_modes = NULL;
            display->available_mode_count = 0;
        }
        enum_data->current_display_idx++;
    }
    return TRUE;
}
#endif

/**
 * @brief [INTERNAL] Queries and caches all available physical monitors and their properties.
 *
 * @details This function is the central point for discovering and storing information about
 *          connected physical displays/monitors at runtime. It is called during library
 *          initialization (SituationInit) and whenever a monitor configuration change is
 *          detected (e.g. via GLFW callbacks or hot-plug events).
 *
 *          What it does:
 *            - Calls `glfwGetMonitors` to retrieve the current list of monitors
 *            - For each monitor:
 *              - Retrieves primary/secondary status
 *              - Queries physical bounds (x,y,width,height in virtual screen space)
 *              - Gets current video mode (resolution, refresh rate, bit depth)
 *              - Queries all supported video modes (via `glfwGetVideoModes`)
 *              - Retrieves monitor name (human-readable string)
 *              - Caches DPI scaling / content scale if available
 *            - Stores the aggregated data in a global/internal cache (e.g. `sit_display.monitors` array)
 *            - Updates primary monitor index and total count
 *            - Invalidates any stale virtual display associations if monitors changed
 *
 *          The cache is used by all public display query functions:
 *            - `SituationGetMonitorCount`
 *            - `SituationGetMonitorName`
 *            - `SituationGetMonitorBounds`
 *            - `SituationGetMonitorCurrentMode`
 *            - `SituationGetMonitorModes`
 *            - `SituationGetPrimaryMonitor`
 *
 *          This avoids repeated expensive GLFW calls during frame loops or user queries.
 *
 * Thread safety invariants:
 *   - Should be called only from the **main thread** during init or on monitor change events
 *   - GLFW monitor functions are not thread-safe ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â must not be called concurrently
 *   - Cache write is protected by internal mutex if multi-thread access is possible
 *     (though in typical usage, queries are read-only after init)
 *   - Safe to call multiple times ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â refreshes the cache atomically
 *
 * @note Called automatically on:
 *       - `SituationInit`
 *       - GLFW monitor callback (connection/disconnection events)
 *       - Manual refresh requests (if you expose one)
 *
 *       On failure (e.g. GLFW returns no monitors), logs a warning and sets
 *       an internal error state (e.g. SITUATION_ERROR_DISPLAY_QUERY_FAILED).
 *       Cache is left in a safe but empty state.
 *
 * @see SituationInit, SituationGetMonitorCount, SituationGetMonitorName,
 *      SituationGetMonitorBounds, SituationGetPrimaryMonitor,
 *      SITUATION_ERROR_DISPLAY_QUERY, SITUATION_ERROR_DISPLAY_QUERY_FAILED
 */
static SituationError _SituationCachePhysicalDisplays(void) {
    if (!SituationIsInitialized()) return SITUATION_SUCCESS;
    if (sit_gs.cached_physical_displays_array) {
        for (int i = 0; i < sit_gs.cached_physical_display_count; ++i) {
            SIT_FREE(sit_gs.cached_physical_displays_array[i].available_modes);
        }
        SIT_FREE(sit_gs.cached_physical_displays_array);
        sit_gs.cached_physical_displays_array = NULL;
        sit_gs.cached_physical_display_count = 0;
    }
    #if defined(_WIN32)
    int win32_monitor_count = GetSystemMetrics(SM_CMONITORS);
    if (win32_monitor_count <= 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY_FAILED, "No physical monitors reported by GetSystemMetrics(SM_CMONITORS).");
    }
    sit_gs.cached_physical_displays_array = (SituationDisplayInfo*)SIT_CALLOC(win32_monitor_count, sizeof(SituationDisplayInfo));
    if (!sit_gs.cached_physical_displays_array) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Physical displays cache");
    }
    int glfw_monitor_count;
    GLFWmonitor** glfw_monitors = glfwGetMonitors(&glfw_monitor_count);
    _SituationMonitorEnumData enum_data = {};
    enum_data.displays_array = sit_gs.cached_physical_displays_array;
    enum_data.max_displays = win32_monitor_count;
    enum_data.current_display_idx = 0;
    enum_data.glfw_monitors = glfw_monitors;
    enum_data.glfw_monitor_count = glfw_monitor_count;

    if (!EnumDisplayMonitors(NULL, NULL, _SituationMonitorEnumProc, (LPARAM)&enum_data)) {
        for (int i = 0; i < enum_data.current_display_idx; ++i) SIT_FREE(sit_gs.cached_physical_displays_array[i].available_modes);
        SIT_FREE(sit_gs.cached_physical_displays_array); sit_gs.cached_physical_displays_array = NULL;
        return _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY_FAILED, "EnumDisplayMonitors failed.");
    }
    sit_gs.cached_physical_display_count = enum_data.current_display_idx;
    #else
    int glfw_count;
    GLFWmonitor** monitors = glfwGetMonitors(&glfw_count);
    if (glfw_count > 0) {
        sit_gs.cached_physical_displays_array = (SituationDisplayInfo*)SIT_CALLOC(glfw_count, sizeof(SituationDisplayInfo));
        if (!sit_gs.cached_physical_displays_array) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Physical displays cache (non-Win32)");
        }
        sit_gs.cached_physical_display_count = glfw_count;
        for (int i = 0; i < glfw_count; ++i) {
            SituationDisplayInfo* disp = &sit_gs.cached_physical_displays_array[i];
            disp->situation_monitor_id = i;
            disp->glfw_monitor_handle = monitors[i];
            const char* name = glfwGetMonitorName(monitors[i]);
            if (name) { strncpy(disp->name, name, SITUATION_MAX_MONITOR_NAME_LEN-1); disp->name[SITUATION_MAX_MONITOR_NAME_LEN-1] = '\0'; }
            else { snprintf(disp->name, SITUATION_MAX_MONITOR_NAME_LEN, "GLFW Monitor %d", i); }
            disp->is_primary = (monitors[i] == glfwGetPrimaryMonitor());
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            if (mode) {
                disp->current_mode.width = mode->width;
                disp->current_mode.height = mode->height;
                disp->current_mode.refresh_rate = mode->refreshRate;
                disp->current_mode.color_depth = mode->redBits + mode->greenBits + mode->blueBits;
            }
            int vid_mode_count;
            const GLFWvidmode* vid_modes = glfwGetVideoModes(monitors[i], &vid_mode_count);
            if (vid_mode_count > 0) {
                disp->available_modes = (SituationDisplayMode*)SIT_MALLOC(vid_mode_count * sizeof(SituationDisplayMode));
                if (disp->available_modes) {
                    disp->available_mode_count = vid_mode_count;
                    for (int j = 0; j < vid_mode_count; ++j) {
                        disp->available_modes[j].width = vid_modes[j].width;
                        disp->available_modes[j].height = vid_modes[j].height;
                        disp->available_modes[j].refresh_rate = vid_modes[j].refreshRate;
                        disp->available_modes[j].color_depth = vid_modes[j].redBits + vid_modes[j].greenBits + vid_modes[j].blueBits;
                    }
                } else {
                    disp->available_mode_count = 0;
                    _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Available modes for GLFW monitor");
                }
            } else { disp->available_mode_count = 0; }
        }
    } else {
        return _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY_FAILED, "No monitors reported by GLFW.");
    }
    #endif
    return SITUATION_SUCCESS;
}

/**
 * @brief Retrieves information about all connected physical displays.
 * @warning This function returns a dynamically allocated array of structs. The caller is RESPONSIBLE FOR FREEING THIS MEMORY. This requires a nested deallocation:first, loop through the array and free the `available_modes` pointer for each display, then free the top-level array itself.
 *
 * @example
 *   int display_count = 0;
 *   SituationDisplayInfo* displays = SituationGetDisplays(&display_count);
 *   if (displays) {
 *       // ... use the display info ...
 *
 *       // --- CRITICAL: Correct cleanup ---
 *       for (int i = 0; i < display_count; ++i) {
 *           SIT_FREE(displays[i].available_modes);
 *       }
 *       SIT_FREE(displays);
 *   }
 *
 * @param count Pointer to an integer that will be filled with the number of displays found.
 * @return A pointer to a newly allocated array of SituationDisplayInfo structs, or NULL on failure.
 */
SITAPI SituationError SituationGetDisplays(SituationDisplayInfo** out_displays, int* out_count) {
    if (out_count) *out_count = 0;
    if (out_displays) *out_displays = NULL;
    else return SITUATION_ERROR_INVALID_PARAM;

    if (!SituationIsInitialized()) return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "GetDisplays");
    if (!sit_gs.cached_physical_displays_array) _SituationCachePhysicalDisplays();
    if (!sit_gs.cached_physical_displays_array || sit_gs.cached_physical_display_count == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY, "No cached displays or count is zero");
    }
    SituationDisplayInfo* displays_copy = (SituationDisplayInfo*)SIT_MALLOC(sit_gs.cached_physical_display_count * sizeof(SituationDisplayInfo));
    if (!displays_copy) return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Copy of display infos");

    for (int i = 0; i < sit_gs.cached_physical_display_count; ++i) {
        memcpy(&displays_copy[i], &sit_gs.cached_physical_displays_array[i], sizeof(SituationDisplayInfo));
        if (sit_gs.cached_physical_displays_array[i].available_mode_count > 0 && sit_gs.cached_physical_displays_array[i].available_modes) {
            displays_copy[i].available_modes = (SituationDisplayMode*)SIT_MALLOC(displays_copy[i].available_mode_count * sizeof(SituationDisplayMode));
            if (displays_copy[i].available_modes) {
                memcpy(displays_copy[i].available_modes, sit_gs.cached_physical_displays_array[i].available_modes, displays_copy[i].available_mode_count * sizeof(SituationDisplayMode));
            } else {
                displays_copy[i].available_modes = NULL;
                displays_copy[i].available_mode_count = 0;
                // Allocation failed for modes, but we continue with partial data or log warning?
                // For robustness, just skip modes for this display.
            }
        } else {
            displays_copy[i].available_modes = NULL;
            displays_copy[i].available_mode_count = 0;
        }
    }
    if (out_count) *out_count = sit_gs.cached_physical_display_count;
    *out_displays = displays_copy;
    return SITUATION_SUCCESS;
}

/**
 * @brief Forces a re-scan of all connected physical displays and their supported video modes.
 *
 * @details Clears the internal display cache and queries the operating system for the current hardware configuration.
 *          This is useful if the user plugs in or unplugs a monitor while the application is running.
 *
 * @note This function will invalidate any pointers previously returned by `SituationGetDisplays()`.
 *       If you are holding a pointer to the display list, you must call `SituationGetDisplays()` again after calling this.
 *
 * @see _SituationCachePhysicalDisplays()
 */
SITAPI void SituationRefreshDisplays(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "RefreshDisplays"); return; }
    _SituationCachePhysicalDisplays();
}

/**
 * @brief [INTERNAL] Identifies which physical monitor the window is currently occupying.
 *
 * @details Uses a geometric heuristic to determine the "current" monitor:
 *          1. If the window is in exclusive fullscreen, returns that monitor immediately.
 *          2. If windowed, calculates the intersection area (overlap) between the window's rectangle and every connected monitor's viewport.
 *          3. Returns the ID of the monitor with the largest overlap area.
 *
 * @return The `situation_monitor_id` (index) of the current display.
 * @return `-1` if the library is not initialized or if the window is not overlapping any known display (e.g., off-screen).
 *
 * @note This function is used internally by `SituationApplyCurrentProfileWindowState` to determine where to place the window when toggling fullscreen.
 */
SITAPI int _SituationGetCurrentDisplayIdentifier(void) {
    if (!SituationIsInitialized() || !sit_gs.sit_glfw_window) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "GetCurrentDisplayIdentifier"); return -1; }
    GLFWmonitor* current_glfw_monitor = glfwGetWindowMonitor(sit_gs.sit_glfw_window);
    if (!current_glfw_monitor) {
        int win_x, win_y, win_w, win_h;
        glfwGetWindowPos(sit_gs.sit_glfw_window, &win_x, &win_y);
        glfwGetWindowSize(sit_gs.sit_glfw_window, &win_w, &win_h);
        if (win_w <=0 || win_h <=0) {
            win_w = sit_gs.main_window_width; win_h = sit_gs.main_window_height;
        }
        SitRectangle window_rect = {(float)win_x, (float)win_y, (float)win_w, (float)win_h};
        current_glfw_monitor = glfwGetPrimaryMonitor();
        int max_overlap = 0;
        if (sit_gs.cached_physical_display_count == 0) _SituationCachePhysicalDisplays();
        for (int i = 0; i < sit_gs.cached_physical_display_count; ++i) {
            if (sit_gs.cached_physical_displays_array[i].glfw_monitor_handle) {
                GLFWmonitor* mon_handle = sit_gs.cached_physical_displays_array[i].glfw_monitor_handle;
                const GLFWvidmode* mode = glfwGetVideoMode(mon_handle);
                int mon_x, mon_y;
                glfwGetMonitorPos(mon_handle, &mon_x, &mon_y);
                if (mode) {
                    SitRectangle monitor_rect = {(float)mon_x, (float)mon_y, (float)mode->width, (float)mode->height};
                    float overlap_x_dim = fmaxf(0.0f, fminf(window_rect.x + window_rect.width, monitor_rect.x + monitor_rect.width) - fmaxf(window_rect.x, monitor_rect.x));
                    float overlap_y_dim = fmaxf(0.0f, fminf(window_rect.y + window_rect.height, monitor_rect.y + monitor_rect.height) - fmaxf(window_rect.y, monitor_rect.y));
                    int current_overlap_area = (int)(overlap_x_dim * overlap_y_dim);
                    if (current_overlap_area > max_overlap) {
                        max_overlap = current_overlap_area;
                        current_glfw_monitor = mon_handle;
                    }
                }
            }
        }
    }
    for (int i = 0; i < sit_gs.cached_physical_display_count; ++i) {
        if (sit_gs.cached_physical_displays_array[i].glfw_monitor_handle == current_glfw_monitor) {
            return sit_gs.cached_physical_displays_array[i].situation_monitor_id;
        }
    }
    if (current_glfw_monitor == glfwGetPrimaryMonitor() && sit_gs.cached_physical_display_count > 0) {
        for (int i = 0; i < sit_gs.cached_physical_display_count; ++i) {
            if (sit_gs.cached_physical_displays_array[i].is_primary) return sit_gs.cached_physical_displays_array[i].situation_monitor_id;
        }
    }
    return -1;
}

static GLFWmonitor* _SituationGetWindowGLFWMonitor(void) {
    GLFWmonitor* attached = glfwGetWindowMonitor(sit_gs.sit_glfw_window);
    if (attached) {
        return attached;
    }
    if (sit_gs.cached_physical_display_count == 0) {
        _SituationCachePhysicalDisplays();
    }
    int monitor_id = _SituationGetCurrentDisplayIdentifier();
    if (monitor_id != -1 && sit_gs.cached_physical_displays_array) {
        GLFWmonitor* on_display = sit_gs.cached_physical_displays_array[monitor_id].glfw_monitor_handle;
        if (on_display) {
            return on_display;
        }
    }
    return glfwGetPrimaryMonitor();
}

/**
 * @brief Sets the display mode (resolution, refresh rate, etc.) and fullscreen state for a specific monitor.
 *
 * @details Changes the video mode of the specified physical monitor and optionally toggles
 *          the application window between windowed and fullscreen (or borderless fullscreen)
 *          on that monitor.
 *
 *          This is the primary function for:
 *            - Switching resolutions and refresh rates at runtime
 *            - Entering or exiting fullscreen mode on a chosen monitor
 *            - Supporting multi-monitor exclusive fullscreen applications
 *            - Implementing user-configurable display settings
 *
 *          Behavior:
 *            - If `fullscreen` is true:
 *              - Attempts exclusive fullscreen mode on the target monitor using the requested mode
 *              - Falls back to borderless fullscreen if exclusive mode is unavailable or denied
 *            - If `fullscreen` is false:
 *              - Restores the window to windowed mode (usually centered on the target monitor)
 *              - The `mode` parameter is ignored in windowed mode
 *            - The window is automatically moved to/resized for the target monitor if not already there
 *
 *          The requested mode must be one of the supported modes for the monitor
 *          (query via `SituationGetMonitorModes` or `SituationGetMonitorCurrentMode`).
 *          Passing NULL for `mode` when entering fullscreen uses the monitor's preferred/current mode.
 *
 * @param situation_monitor_id Zero-based monitor index (as returned by `SituationGetMonitorCount` and related queries).
 *                             Use 0 for primary monitor. Invalid IDs return an error.
 * @param mode Pointer to a `SituationDisplayMode` structure specifying the desired resolution,
 *             refresh rate, bit depth, etc. (see `SituationDisplayMode` definition).
 *             May be NULL when `fullscreen` is false or when using the monitor's preferred mode.
 * @param fullscreen true to enter fullscreen (exclusive or borderless), false to return to windowed mode.
 *
 * @return SITUATION_SUCCESS on successful mode change,
 *         SITUATION_ERROR_DISPLAY_MODE_UNSUPPORTED if the requested mode is not available on the monitor,
 *         SITUATION_ERROR_DISPLAY_MODE_SET_FAILED if GLFW/video driver rejected the change,
 *         SITUATION_ERROR_DISPLAY_QUERY_FAILED if monitor state could not be queried,
 *         SITUATION_ERROR_INVALID_PARAM if monitor_id is out of range or mode is malformed,
 *         or other appropriate error codes.
 *
 * @note This function may cause a brief screen flash, resolution change, or window reposition.
 *       - Exclusive fullscreen may fail on some platforms/drivers (e.g. macOS prefers borderless).
 *       - DPI scaling and window position are preserved or adjusted automatically where possible.
 *       - Safe to call before window creation (sets initial fullscreen preference), but most effective after.
 *       - Calling with the current mode/fullscreen state is a no-op (no error).
 *
 * @see SituationGetMonitorModes, SituationGetMonitorCurrentMode, SituationGetMonitorCount,
 *      SituationSetWindowMonitor, SituationGetPrimaryMonitor,
 *      SituationDisplayMode, SITUATION_ERROR_DISPLAY_MODE_xxx codes
 */
SITAPI SituationError SituationSetDisplayMode(int situation_monitor_id, const SituationDisplayMode* mode, bool fullscreen) {
    if (!SituationIsInitialized() || !sit_gs.sit_glfw_window) return SITUATION_ERROR_NOT_INITIALIZED;
    if (!mode) return SITUATION_ERROR_INVALID_PARAM;
    if (situation_monitor_id < 0 || situation_monitor_id >= sit_gs.cached_physical_display_count || !sit_gs.cached_physical_displays_array) {
         _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid situation_monitor_id or display cache empty");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    SituationDisplayInfo* target_display_info = &sit_gs.cached_physical_displays_array[situation_monitor_id];
    GLFWmonitor* glfw_mon = target_display_info->glfw_monitor_handle;
    if (!glfw_mon) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY, "No GLFW monitor handle for specified Situation ID. Cannot set mode via GLFW.");
        #if !defined(_WIN32)
        return SITUATION_ERROR_DISPLAY_QUERY;
        #endif
    }
#if defined(_WIN32)
    if (target_display_info->name[0] != '\0') {
        DEVMODEA dev_mode = {0};
        dev_mode.dmSize = sizeof(DEVMODEA);
        strncpy((char*)dev_mode.dmDeviceName, target_display_info->name, CCHDEVICENAME -1);
        dev_mode.dmDeviceName[CCHDEVICENAME-1] = '\0';
        dev_mode.dmPelsWidth = mode->width;
        dev_mode.dmPelsHeight = mode->height;
        dev_mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
        if (mode->refresh_rate > 0) { dev_mode.dmDisplayFrequency = mode->refresh_rate; dev_mode.dmFields |= DM_DISPLAYFREQUENCY; }
        if (mode->color_depth > 0) { dev_mode.dmBitsPerPel = mode->color_depth; dev_mode.dmFields |= DM_BITSPERPEL; }
        LONG cds_result;
        DWORD flags = fullscreen ? CDS_FULLSCREEN : 0;
        if (fullscreen) {
             cds_result = ChangeDisplaySettingsExA(target_display_info->name, &dev_mode, NULL, flags, NULL);
        } else {
             cds_result = ChangeDisplaySettingsExA(target_display_info->name, NULL, NULL, 0, NULL);
        }
        if (cds_result != DISP_CHANGE_SUCCESSFUL) {
            char err_detail[128];
            snprintf(err_detail, sizeof(err_detail), "Win32 ChangeDisplaySettingsExA for '%s' failed with code %ld.", target_display_info->name, cds_result);
            if (cds_result == DISP_CHANGE_BADMODE) {
                _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_MODE_UNSUPPORTED, err_detail);
                if (!glfw_mon) return SITUATION_ERROR_DISPLAY_MODE_UNSUPPORTED;
            } else {
                _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_MODE_SET_FAILED, err_detail);
                if (!glfw_mon) return SITUATION_ERROR_DISPLAY_MODE_SET_FAILED;
            }
        } else {
             SituationRefreshDisplays();
        }
    } else if (!glfw_mon) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY, "No Win32 device name and no GLFW monitor for SetDisplayMode.");
        return SITUATION_ERROR_DISPLAY_QUERY;
    }
#endif
    if (glfw_mon) {
        if (fullscreen) {
            sit_gs.fullscreen_w = mode->width;
            sit_gs.fullscreen_h = mode->height;
            glfwSetWindowMonitor(sit_gs.sit_glfw_window, glfw_mon, 0, 0, mode->width, mode->height, mode->refresh_rate > 0 ? mode->refresh_rate : GLFW_DONT_CARE);
        } else {
            sit_gs.fullscreen_w = 0;
            sit_gs.fullscreen_h = 0;
            const GLFWvidmode* current_mon_mode = glfwGetVideoMode(glfw_mon);
            int win_x = 0, win_y = 0;
            if(current_mon_mode) {
                int mon_x_pos, mon_y_pos;
                glfwGetMonitorPos(glfw_mon, &mon_x_pos, &mon_y_pos);
                win_x = mon_x_pos + (current_mon_mode->width - mode->width) / 2;
                win_y = mon_y_pos + (current_mon_mode->height - mode->height) / 2;
            } else {
                GLFWmonitor* primary = glfwGetPrimaryMonitor();
                const GLFWvidmode* primary_mode = glfwGetVideoMode(primary);
                if(primary_mode) {
                     win_x = (primary_mode->width - mode->width) / 2;
                     win_y = (primary_mode->height - mode->height) / 2;
                } else { win_x = 100; win_y = 100; }
            }
            glfwSetWindowMonitor(sit_gs.sit_glfw_window, NULL, win_x, win_y, mode->width, mode->height, GLFW_DONT_CARE);
        }
    } else {
        #if defined(_WIN32)
        // Error handling already done if WinAPI failed.
        #else
        return SITUATION_ERROR_DISPLAY_QUERY;
        #endif
    }
    int fb_w, fb_h;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fb_w, &fb_h);
    _SituationGLFWFramebufferSizeCallback(sit_gs.sit_glfw_window, fb_w, fb_h);
    SituationRefreshDisplays();
    return SITUATION_SUCCESS;
}

//==================================================================================
// Window and Display Module
//==================================================================================

/**
 * @brief Checks if the window is currently in exclusive fullscreen mode.
 * @return `true` if the window is in fullscreen mode, `false` otherwise.
 * @see SituationToggleFullscreen()
 */
SITAPI bool SituationIsWindowFullscreen(void) {
    if (!SituationIsInitialized()) return false;
    return (glfwGetWindowMonitor(sit_gs.sit_glfw_window) != NULL);
}

/**
 * @brief Checks if the window is currently hidden (not visible).
 * @return `true` if the window is hidden, `false` otherwise.
 */
SITAPI bool SituationIsWindowHidden(void) {
    if (!SituationIsInitialized()) return false;
    return (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_VISIBLE) == GLFW_FALSE);
}

/**
 * @brief Checks if the window is currently minimized (iconified).
 * @return `true` if the window is minimized, `false` otherwise.
 * @see SituationMinimizeWindow()
 */
SITAPI bool SituationIsWindowMinimized(void) {
    if (!SituationIsInitialized()) return false;
    return (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_ICONIFIED) == GLFW_TRUE);
}

/**
 * @brief Checks if the window is currently maximized.
 * @return `true` if the window is maximized, `false` otherwise.
 * @see SituationMaximizeWindow()
 */
SITAPI bool SituationIsWindowMaximized(void) {
    if (!SituationIsInitialized()) return false;
    return (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_MAXIMIZED) == GLFW_TRUE);
}

/**
 * @brief Checks if the window was resized during the last frame's event polling.
 * @details This is a single-frame "event" flag, ideal for triggering resolution-dependent updates in your main loop. It is reset to `false` at the beginning of each frame by `SituationPollInputEvents()`.
 * @return `true` if a resize event occurred, `false` otherwise.
 */
SITAPI bool SituationIsWindowResized(void) {
    if (!SituationIsInitialized()) return false;
    return sit_gs.was_window_resized_last_frame;
}

/**
 * @brief Checks if a specific window state is currently active.
 * @details This function queries the underlying windowing system for the *actual*, current state of the window, which may differ from the target state set in the profiles.
 * @param flag The `SITUATION_FLAG_*` define to check.
 * @return `true` if the flag corresponds to an active window state, `false` otherwise.
 * @see SituationGetCurrentActualWindowStateFlags()
 */
SITAPI bool SituationIsWindowState(uint32_t flag) {
    if (!SituationIsInitialized()) return false;
    return (SituationGetCurrentActualWindowStateFlags() & flag) != 0;
}

/**
 * @brief Sets one or more window state flags for the current focus profile.
 * @details This function modifies the window's target state by adding the specified flags to the active profile (either the "focused" or "unfocused" profile). It then immediately applies this new profile, causing the window's state to change.
 *
 * @param flags A bitmask of `SITUATION_FLAG_*` defines (e.g., `SITUATION_FLAG_WINDOW_TOPMOST | SITUATION_FLAG_WINDOW_UNDECORATED`).
 * @see SituationClearWindowState(), SituationApplyCurrentProfileWindowState()
 */
/* Forward declaration — defined later in this file alongside SituationGetCurrentActualWindowStateFlags. */
static uint32_t _SituationComputeWindowStateFlags(void);

SITAPI void SituationSetWindowState(uint32_t flags) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowState"); return; }
    uint32_t* profile = sit_gs.current_window_focus_state ? &sit_gs.active_profile_window_flags : &sit_gs.inactive_profile_window_flags;
    *profile |= flags;
    SituationApplyCurrentProfileWindowState();
    sit_gs.cached_window_state_flags = _SituationComputeWindowStateFlags();
}

/**
 * @brief Clears one or more window state flags from the current focus profile.
 * @details This function modifies the window's target state by removing the specified flags from the active profile (either the "focused" or "unfocused" profile). It then immediately applies this new profile.
 *
 * @param flags A bitmask of `SITUATION_FLAG_*` defines to remove.
 * @see SituationSetWindowState(), SituationApplyCurrentProfileWindowState()
 */
SITAPI void SituationClearWindowState(uint32_t flags) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationClearWindowState"); return; }
    uint32_t* profile = sit_gs.current_window_focus_state ? &sit_gs.active_profile_window_flags : &sit_gs.inactive_profile_window_flags;
    *profile &= ~flags;
    SituationApplyCurrentProfileWindowState();
    sit_gs.cached_window_state_flags = _SituationComputeWindowStateFlags();
}

/**
 * @brief Enables or disables VSync (vertical synchronization).
 *
 * @details VSync synchronizes frame rendering with the monitor's refresh rate,
 *          preventing screen tearing but capping FPS at the monitor's refresh rate
 *          (typically 60 Hz). Disabling VSync allows unlimited FPS but may cause tearing.
 *
 * @param enable True to enable VSync, false to disable it.
 *
 * @note This is a convenience wrapper around SituationSetWindowState() and
 *       SituationClearWindowState() with SITUATION_FLAG_VSYNC_HINT.
 *
 * @see SituationSetWindowState(), SituationClearWindowState()
 */
SITAPI void SituationSetVSync(bool enable) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetVSync"); return; }

    // Check if VSync state is actually changing
    bool current_vsync = (sit_gs.active_profile_window_flags & SITUATION_FLAG_VSYNC_HINT) != 0;
    if (current_vsync == enable) {
        return;  // No change needed
    }

    // Update the flag
    if (enable) {
        SituationSetWindowState(SITUATION_FLAG_VSYNC_HINT);
    } else {
        SituationClearWindowState(SITUATION_FLAG_VSYNC_HINT);
    }

    // For Vulkan, we need to recreate the swapchain with the new present mode
    #ifdef SITUATION_USE_VULKAN
    _SituationVulkanWaitInFlightFencesPump("SituationSetVSync");
    SituationError recreate_err = _SituationVulkanRecreateSwapchain();
    if (recreate_err != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(recreate_err, "SituationSetVSync failed to recreate swapchain.");
    }
    #endif

    // For OpenGL, glfwSwapInterval is already called by SituationSetWindowState
}

/**
 * @brief Toggles the window between exclusive fullscreen and windowed mode.
 * @details This is a high-level convenience function that toggles the `SITUATION_FLAG_FULLSCREEN_MODE` in the current window profile and applies the change.
 *          When entering fullscreen, it uses the current monitor's native resolution. When returning to windowed mode, it restores the window's previous size and position.
 * @see SituationToggleBorderlessWindowed(), SituationIsWindowFullscreen()
 */
SITAPI void SituationToggleFullscreen(void) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationToggleFullscreen");
        return;
    }

    bool is_fullscreen = SituationIsWindowFullscreen();
    if (is_fullscreen) {
        sit_gs.active_profile_window_flags &= ~SITUATION_FLAG_FULLSCREEN_MODE;
        sit_gs.inactive_profile_window_flags &= ~SITUATION_FLAG_FULLSCREEN_MODE;
    } else {
        sit_gs.active_profile_window_flags |= SITUATION_FLAG_FULLSCREEN_MODE;
        sit_gs.inactive_profile_window_flags |= SITUATION_FLAG_FULLSCREEN_MODE;
        sit_gs.active_profile_window_flags &= ~(SITUATION_FLAG_WINDOW_MAXIMIZED | SITUATION_FLAG_WINDOW_MINIMIZED);
        sit_gs.inactive_profile_window_flags &= ~(SITUATION_FLAG_WINDOW_MAXIMIZED | SITUATION_FLAG_WINDOW_MINIMIZED);
    }

    SituationApplyCurrentProfileWindowState();
}

/**
 * @brief Toggles the window between a standard decorated style and a borderless, fullscreen-windowed style.
 * @details This is ideal for creating an immersive, "fake fullscreen" experience. When entering borderless mode, it saves the current window position, removes decorations, and resizes the window to fill the current monitor. When exiting, it restores decorations and the original size/position.
 * @see SituationToggleFullscreen()
 */
SITAPI void SituationToggleBorderlessWindowed(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationToggleBorderlessWindowed"); return; }

    if (!sit_gs.is_borderless_active) {
        // --- ENTERING BORDERLESS ---
        // Save current windowed size and position
        glfwGetWindowPos(sit_gs.sit_glfw_window, &sit_gs.windowed_x, &sit_gs.windowed_y);
        glfwGetWindowSize(sit_gs.sit_glfw_window, &sit_gs.windowed_w, &sit_gs.windowed_h);

        GLFWmonitor* monitor = _SituationGetWindowGLFWMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) { _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY, "SituationToggleBorderlessWindowed: glfwGetVideoMode returned NULL"); return; }

        int mx = 0;
        int my = 0;
        glfwGetMonitorPos(monitor, &mx, &my);
        glfwSetWindowAttrib(sit_gs.sit_glfw_window, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(sit_gs.sit_glfw_window, NULL, mx, my, mode->width, mode->height, 0);
        sit_gs.is_borderless_active = true;
        glfwPollEvents();
    } else {
        // --- LEAVING BORDERLESS ---
        // Restore decoration and previous size/pos
        glfwSetWindowAttrib(sit_gs.sit_glfw_window, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(sit_gs.sit_glfw_window, NULL, sit_gs.windowed_x, sit_gs.windowed_y, sit_gs.windowed_w, sit_gs.windowed_h, 0);
        sit_gs.is_borderless_active = false;
    }
}

/**
 * @brief Maximizes the application window to fill the available work area of the monitor.
 * @details This function will have no effect if the window is not resizable.
 * @see SituationMinimizeWindow(), SituationRestoreWindow(), SituationIsWindowMaximized()
 */
SITAPI void SituationMaximizeWindow(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationMaximizeWindow"); return; }
    glfwMaximizeWindow(sit_gs.sit_glfw_window);
}

/**
 * @brief Minimizes (iconifies) the application window, hiding it from the screen.
 * @details When minimized, the application will typically be paused automatically to conserve resources.
 * @see SituationMaximizeWindow(), SituationRestoreWindow(), SituationIsWindowMinimized()
 */
SITAPI void SituationMinimizeWindow(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationMinimizeWindow"); return; }
    glfwIconifyWindow(sit_gs.sit_glfw_window);
}

/**
 * @brief Restores a minimized or maximized window to its normal, windowed state.
 * @details If the window is minimized, it will be un-hidden. If it is maximized, it will return to its previous non-maximized size and position.
 * @see SituationMaximizeWindow(), SituationMinimizeWindow()
 */
SITAPI void SituationRestoreWindow(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationRestoreWindow"); return; }
    glfwRestoreWindow(sit_gs.sit_glfw_window);
}

/**
 * @brief Sets multiple images of different sizes as the application's icon.
 * @details The operating system will automatically select the most appropriate image size for display in various contexts (e.g., title bar, taskbar, Alt-Tab switcher).
 *          Common sizes to provide are 16x16, 32x32, and 48x48.
 * @param images A pointer to an array of `SituationImage` structs.
 * @param count The number of images in the array.
 * @see SituationSetWindowIcon()
 */
SITAPI void SituationSetWindowIcons(SituationImage *images, int count) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowIcons"); return; }
    if (!images || count <= 0) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationSetWindowIcons: images is NULL or count <= 0"); return; }

    GLFWimage* glfw_images = (GLFWimage*)SIT_MALLOC(count * sizeof(GLFWimage));
    if (!glfw_images) { _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationSetWindowIcons: failed to allocate GLFWimage array"); return; }

    for (int i = 0; i < count; i++) {
        glfw_images[i].width = images[i].width;
        glfw_images[i].height = images[i].height;
        glfw_images[i].pixels = (unsigned char*)images[i].data;
    }

    glfwSetWindowIcon(sit_gs.sit_glfw_window, count, glfw_images);
    SIT_FREE(glfw_images);
}

/**
 * @brief Sets a single image as the application's icon in the title bar and taskbar.
 * @details For best results across different operating systems and display scales, it is recommended to use `SituationSetWindowIcons` with multiple sizes.
 * @param image A `SituationImage` containing the icon's pixel data.
 * @see SituationSetWindowIcons()
 */
SITAPI void SituationSetWindowIcon(SituationImage image) {
    SituationSetWindowIcons(&image, 1);
}

/**
 * @brief Sets the text that appears in the window's title bar.
 * @param title A null-terminated, UTF-8 encoded string for the new window title.
 */
SITAPI void SituationSetWindowTitle(const char *title) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowTitle"); return; }
    glfwGetError(NULL);
    glfwSetWindowTitle(sit_gs.sit_glfw_window, title);
    const char* desc = NULL;
    if (glfwGetError(&desc) != GLFW_NO_ERROR) {
        _SituationSetErrorFromCode(SITUATION_ERROR_WINDOW_PROPERTY_FAILED, desc ? desc : "Failed to set window title");
    }
}

/**
 * @brief Sets the position of the top-left corner of the window on the desktop.
 * @param x The new x-coordinate on the virtual screen.
 * @param y The new y-coordinate on the virtual screen.
 * @see SituationGetWindowPosition()
 */
SITAPI void SituationSetWindowPosition(int x, int y) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowPosition"); return; }
    glfwGetError(NULL);
    glfwSetWindowPos(sit_gs.sit_glfw_window, x, y);
    const char* desc = NULL;
    if (glfwGetError(&desc) != GLFW_NO_ERROR) {
        _SituationSetErrorFromCode(SITUATION_ERROR_WINDOW_PROPERTY_FAILED, desc ? desc : "Failed to set window position");
    }
}

/**
 * @brief Sets the dimensions of the window's client area (the drawable region).
 * @param width The new width of the client area in screen coordinates.
 * @param height The new height of the client area in screen coordinates.
 * @see SituationGetWindowSize()
 */
SITAPI void SituationSetWindowSize(int width, int height) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowSize"); return; }
    glfwGetError(NULL);
    glfwSetWindowSize(sit_gs.sit_glfw_window, width, height);
    const char* desc = NULL;
    if (glfwGetError(&desc) != GLFW_NO_ERROR) {
        _SituationSetErrorFromCode(SITUATION_ERROR_WINDOW_PROPERTY_FAILED, desc ? desc : "Failed to set window size");
    }
}

/**
 * @brief Moves or sets the window to run on a specific physical monitor.
 *
 * @details Changes the monitor that the application window is assigned to. This function
 *          is typically used to support multi-monitor setups, allowing the user or application
 *          to move the window to a secondary display, full-screen it on a specific monitor,
 *          or restore windowed mode on a chosen display.
 *
 *          If the window is currently in fullscreen or borderless fullscreen mode, this call
 *          may trigger a mode switch or repositioning on the target monitor. In windowed mode,
 *          the window is moved to the target monitor (centered or at its current relative position,
 *          depending on internal policy).
 *
 *          The monitor ID is a zero-based index returned by `SituationGetMonitorCount()` and
 *          queried via functions such as `SituationGetMonitorName()`, `SituationGetMonitorBounds()`,
 *          etc.
 *
 *          Passing an invalid monitor ID (negative or ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â°Ãƒâ€šÃ‚Â¥ monitor count) is a no-op ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â the window
 *          remains on its current monitor.
 *
 * @param monitor_id Zero-based index of the target monitor (0 = primary, 1 = first secondary, etc.).
 *                   Use -1 to explicitly keep the current monitor (no change).
 *
 * @note This function is synchronous and may cause a brief window reposition or mode change.
 *       - In fullscreen mode, the display mode (resolution/refresh rate) is reapplied on the new monitor
 *         if the current mode is supported; otherwise, it falls back to the monitor's preferred mode.
 *       - No automatic DPI scaling or scaling policy change is applied ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â use `SituationSetWindowScale`
 *         or related functions if needed after moving monitors.
 *       - Safe to call before window creation (sets initial monitor preference), but most useful after
 *         `SituationCreateWindow`.
 *
 * @return None (void). Errors (invalid monitor, GLFW failure, etc.) are logged internally via
 *         SITUATION_LOG_WARNING and may set the global error state, but do not abort execution.
 *
 * @see SituationGetMonitorCount, SituationGetMonitorName, SituationGetMonitorBounds,
 *      SituationGetPrimaryMonitor, SituationSetWindowFullscreen, SituationCreateWindow
 */
SITAPI void SituationSetWindowMonitor(int monitor_id) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowMonitor"); return; }

    int display_count = 0;
    SituationDisplayInfo* displays = NULL;
    if (SituationGetDisplays(&displays, &display_count) != SITUATION_SUCCESS || !displays) { _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY, "SituationSetWindowMonitor: failed to query displays"); return; }

    if (monitor_id >= 0 && monitor_id < display_count) {
        SituationDisplayInfo* target_display = &displays[monitor_id];
        if (target_display->glfw_monitor_handle) {
            // Go fullscreen on the target monitor using its current mode
            glfwSetWindowMonitor(sit_gs.sit_glfw_window,
                                 target_display->glfw_monitor_handle,
                                 0, 0, // Position is relative to monitor in fullscreen
                                 target_display->current_mode.width,
                                 target_display->current_mode.height,
                                 target_display->current_mode.refresh_rate);
        }
    }

    // CRITICAL: Free the memory allocated by SituationGetDisplays
    for (int i = 0; i < display_count; ++i) {
        SIT_FREE(displays[i].available_modes);
    }
    SIT_FREE(displays);
}

/**
 * @brief Sets the minimum allowed size for the window's client area.
 * @details The user will not be able to resize the window smaller than these dimensions.
 * @param width The minimum width in screen coordinates.
 * @param height The minimum height in screen coordinates.
 * @see SituationSetWindowMaxSize()
 */
SITAPI void SituationSetWindowMinSize(int width, int height) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowMinSize"); return; }
    // Use GLFW_DONT_CARE to preserve existing max limits
    glfwSetWindowSizeLimits(sit_gs.sit_glfw_window, width, height, GLFW_DONT_CARE, GLFW_DONT_CARE);
}

/**
 * @brief Sets the maximum allowed size for the window's client area.
 * @details The user will not be able to resize the window larger than these dimensions.
 * @param width The maximum width in screen coordinates.
 * @param height The maximum height in screen coordinates.
 * @see SituationSetWindowMinSize()
 */
SITAPI void SituationSetWindowMaxSize(int width, int height) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowMaxSize"); return; }
    // Use GLFW_DONT_CARE to preserve existing min limits
    glfwSetWindowSizeLimits(sit_gs.sit_glfw_window, GLFW_DONT_CARE, GLFW_DONT_CARE, width, height);
}


/**
 * @brief Sets the opacity of the entire window.
 * @details This allows for translucent or "ghosted" window effects.
 * @param opacity A value from `0.0f` (fully transparent) to `1.0f` (fully opaque). Values outside this range will be clamped.
 */
SITAPI void SituationSetWindowOpacity(float opacity) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowOpacity"); return; }
    // Clamp opacity between 0.0 and 1.0
    float clamped_opacity = (opacity < 0.0f) ? 0.0f : (opacity > 1.0f) ? 1.0f : opacity;
    glfwGetError(NULL);
    glfwSetWindowOpacity(sit_gs.sit_glfw_window, clamped_opacity);
    const char* desc = NULL;
    if (glfwGetError(&desc) != GLFW_NO_ERROR) {
        _SituationSetErrorFromCode(SITUATION_ERROR_WINDOW_PROPERTY_FAILED, desc ? desc : "Failed to set window opacity");
    }
}

/**
 * @brief Attempts to bring the application window to the foreground and give it input focus.
 * @details The operating system's window manager ultimately decides whether to grant focus, but this function signals the application's intent to become the active window.
 * @see SituationHasWindowFocus()
 */
SITAPI void SituationSetWindowFocused(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetWindowFocused"); return; }
    glfwGetError(NULL);
    glfwFocusWindow(sit_gs.sit_glfw_window);
    const char* desc = NULL;
    if (glfwGetError(&desc) != GLFW_NO_ERROR) {
        _SituationSetErrorFromCode(SITUATION_ERROR_WINDOW_FOCUS_FAILED, desc ? desc : "Failed to focus window");
    }
}

/**
 * @brief Checks if the window currently has input focus.
 * @details The application's internal "paused" state may change based on focus.
 * @return `true` if the window is the active, focused window on the desktop, `false` otherwise.
 * @see SituationSetWindowFocused()
 */
SITAPI bool SituationHasWindowFocus(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationHasWindowFocus"); return false; }
    return sit_gs.current_window_focus_state;
}

/**
 * @brief Gets the current width of the window in screen coordinates (logical size).
 * @details This value represents the size of the window as perceived by the OS's window manager.
 *          On HiDPI displays, this may be different from the actual pixel width of the framebuffer.
 * @return The current window width in screen coordinates.
 * @see SituationGetRenderWidth()
 */
SITAPI int SituationGetScreenWidth(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetScreenWidth"); return 0; }
    // We already cache this in our global state, but glfwGetWindowSize is the source of truth.
    int width, height;
    glfwGetWindowSize(sit_gs.sit_glfw_window, &width, &height);
    return width;
}

/**
 * @brief Gets the current height of the window in screen coordinates (logical size).
 * @details This value represents the size of the window as perceived by the OS's window manager.
 *          On HiDPI displays, this may be different from the actual pixel height of the framebuffer.
 * @return The current window height in screen coordinates.
 * @see SituationGetRenderHeight()
 */
SITAPI int SituationGetScreenHeight(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetScreenHeight"); return 0; }
    int width, height;
    glfwGetWindowSize(sit_gs.sit_glfw_window, &width, &height);
    return height;
}

/**
 * @brief Gets the current width of the rendering framebuffer in pixels (HiDPI-aware).
 * @details This is the actual pixel dimension you should use for setting viewports, creating render targets, and calculating projection matrices. On a 200% scaled display, this value may be twice `SituationGetScreenWidth()`.
 * @return The current backbuffer width in pixels.
 * @see SituationGetScreenWidth()
 */
SITAPI int SituationGetRenderWidth(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetRenderWidth"); return 0; }
    // This is the framebuffer size, which correctly handles DPI scaling.
    int width, height;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &width, &height);
    return width;
}

/**
 * @brief Gets the current height of the rendering framebuffer in pixels (HiDPI-aware).
 * @details This is the actual pixel dimension you should use for setting viewports, creating
 *          render targets, and calculating projection matrices.
 * @return The current backbuffer height in pixels.
 * @see SituationGetScreenHeight()
 */
SITAPI int SituationGetRenderHeight(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetRenderHeight"); return 0; }
    int width, height;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &width, &height);
    return height;
}

// --- Monitor Information ---

/**
 * @brief Gets the number of connected monitors.
 * @return The number of detected physical displays.
 */
SITAPI int SituationGetMonitorCount(void) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetMonitorCount"); return 0; }
    // Ensure cache is populated if it hasn't been already
    if (sit_gs.cached_physical_display_count == 0) {
        _SituationCachePhysicalDisplays();
    }
    return sit_gs.cached_physical_display_count;
}

/**
 * @brief Gets the index of the monitor the window is currently on.
 * @return The index of the current monitor.
 */
SITAPI int SituationGetCurrentMonitor(void) {
    // This is a direct 1:1 mapping to your existing, more descriptively named function.
    return _SituationGetCurrentDisplayIdentifier();
}

/**
 * @brief Gets the position of the specified monitor on the desktop.
 * @param monitor_id The index of the monitor (from 0 to GetMonitorCount()-1).
 * @return A Vector2 with the monitor's top-left XY coordinates.
 */
SITAPI Vector2 SituationGetMonitorPosition(int monitor_id) {
    Vector2 pos = {0.0f, 0.0f};
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetMonitorPosition"); return pos; }

    if (sit_gs.cached_physical_display_count == 0) _SituationCachePhysicalDisplays();
    if (monitor_id < 0 || monitor_id >= sit_gs.cached_physical_display_count) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetMonitorPosition: monitor_id out of range"); return pos; }

    SituationDisplayInfo* disp = &sit_gs.cached_physical_displays_array[monitor_id];
    if (disp->glfw_monitor_handle) {
        int x, y;
        glfwGetMonitorPos(disp->glfw_monitor_handle, &x, &y);
        pos.x = (float)x;
        pos.y = (float)y;
    }
    return pos;
}

/**
 * @brief Gets the width of the monitor's current video mode.
 * @param monitor_id The index of the monitor.
 * @return The width of the monitor in pixels.
 */
SITAPI int SituationGetMonitorWidth(int monitor_id) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetMonitorWidth"); return 0; }
    if (sit_gs.cached_physical_display_count == 0) _SituationCachePhysicalDisplays();
    if (monitor_id < 0 || monitor_id >= sit_gs.cached_physical_display_count) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetMonitorWidth: monitor_id out of range"); return 0; }
    // This data is already in our excellent cache.
    return sit_gs.cached_physical_displays_array[monitor_id].current_mode.width;
}

/**
 * @brief Gets the height of the monitor's current video mode.
 * @param monitor_id The index of the monitor.
 * @return The height of the monitor in pixels.
 */
SITAPI int SituationGetMonitorHeight(int monitor_id) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetMonitorHeight"); return 0; }
    if (sit_gs.cached_physical_display_count == 0) _SituationCachePhysicalDisplays();
    if (monitor_id < 0 || monitor_id >= sit_gs.cached_physical_display_count) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetMonitorHeight: monitor_id out of range"); return 0; }
    return sit_gs.cached_physical_displays_array[monitor_id].current_mode.height;
}

/**
 * @brief Gets the physical width of the monitor in millimeters.
 * @param monitor_id The index of the monitor.
 * @return The physical width of the monitor.
 */
SITAPI int SituationGetMonitorPhysicalWidth(int monitor_id) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetMonitorPhysicalWidth"); return 0; }
    if (sit_gs.cached_physical_display_count == 0) _SituationCachePhysicalDisplays();
    if (monitor_id < 0 || monitor_id >= sit_gs.cached_physical_display_count) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetMonitorPhysicalWidth: monitor_id out of range"); return 0; }

    SituationDisplayInfo* disp = &sit_gs.cached_physical_displays_array[monitor_id];
    if (disp->glfw_monitor_handle) {
        int width_mm, height_mm;
        glfwGetMonitorPhysicalSize(disp->glfw_monitor_handle, &width_mm, &height_mm);
        return width_mm;
    }
    return 0;
}

/**
 * @brief Gets the physical height of the monitor in millimeters.
 * @param monitor_id The index of the monitor.
 * @return The physical height of the monitor.
 */
SITAPI int SituationGetMonitorPhysicalHeight(int monitor_id) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetMonitorPhysicalHeight"); return 0; }
    if (sit_gs.cached_physical_display_count == 0) _SituationCachePhysicalDisplays();
    if (monitor_id < 0 || monitor_id >= sit_gs.cached_physical_display_count) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetMonitorPhysicalHeight: monitor_id out of range"); return 0; }

    SituationDisplayInfo* disp = &sit_gs.cached_physical_displays_array[monitor_id];
    if (disp->glfw_monitor_handle) {
        int width_mm, height_mm;
        glfwGetMonitorPhysicalSize(disp->glfw_monitor_handle, &width_mm, &height_mm);
        return height_mm;
    }
    return 0;
}

/**
 * @brief Gets the refresh rate of the monitor's current video mode.
 * @param monitor_id The index of the monitor.
 * @return The refresh rate in Hz.
 */
SITAPI int SituationGetMonitorRefreshRate(int monitor_id) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetMonitorRefreshRate"); return 0; }
    if (sit_gs.cached_physical_display_count == 0) _SituationCachePhysicalDisplays();
    if (monitor_id < 0 || monitor_id >= sit_gs.cached_physical_display_count) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetMonitorRefreshRate: monitor_id out of range"); return 0; }
    return sit_gs.cached_physical_displays_array[monitor_id].current_mode.refresh_rate;
}

/**
 * @brief Gets the human-readable UTF-8 name of the monitor.
 * @param monitor_id The index of the monitor.
 * @return A const string with the monitor's name.
 */
SITAPI const char* SituationGetMonitorName(int monitor_id) {
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetMonitorName"); return "N/A"; }
    if (sit_gs.cached_physical_display_count == 0) _SituationCachePhysicalDisplays();
    if (monitor_id < 0 || monitor_id >= sit_gs.cached_physical_display_count) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetMonitorName: monitor_id out of range"); return "Invalid ID"; }
    return sit_gs.cached_physical_displays_array[monitor_id].name;
}

// --- Window Properties ---

/**
 * @brief Gets the window's top-left position on the virtual desktop.
 * @return A `Vector2` containing the window's top-left (x, y) coordinates in screen space.
 * @see SituationSetWindowPosition()
 */
SITAPI Vector2 SituationGetWindowPosition(void) {
    Vector2 pos = {0.0f, 0.0f};
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetWindowPosition"); return pos; }
    int x, y;
    glfwGetWindowPos(sit_gs.sit_glfw_window, &x, &y);
    pos.x = (float)x;
    pos.y = (float)y;
    return pos;
}

/**
 * @brief Gets the DPI scaling factor for the window's content area.
 * @details On a standard 96 DPI display, this will be (1.0, 1.0). On a "Retina" or other HiDPI display scaled to 200%, this will be (2.0, 2.0). This factor represents the ratio
 *          between screen coordinates and framebuffer pixels.
 * @return A `Vector2` containing the horizontal and vertical content scale factors.
 */
SITAPI Vector2 SituationGetWindowScaleDPI(void) {
    Vector2 scale = {1.0f, 1.0f};
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetWindowScaleDPI"); return scale; }
    glfwGetWindowContentScale(sit_gs.sit_glfw_window, &scale.x, &scale.y);
    return scale;
}

/**
 * @brief Sets a callback function to be executed when the window gains or loses input focus.
 * @details The operating system's window manager ultimately decides whether to grant focus, but this function signals the application's intent to become the active window.
 * @param callback The function pointer to your callback. Pass `NULL` to clear the callback.
 * @param user_data A custom pointer that will be passed to your callback function.
 * @see SituationHasWindowFocus()
 */
SITAPI void SituationSetFocusCallback(SituationFocusCallback gained_focus, void* user_data) {
    sit_gs.focus_callback_fn = gained_focus;
    sit_gs.focus_callback_user_ptr = user_data;
}

/**
 * @brief Sets a callback when the window is maximized or restored from the title bar.
 * @details Requires `SITUATION_FLAG_WINDOW_RESIZABLE` at init so the OS maximize control is enabled.
 *          Also fires when `SituationMaximizeWindow()` / `SituationRestoreWindow()` change maximize state.
 * @param callback Function to invoke, or `NULL` to clear.
 * @param user_data Opaque pointer passed to the callback.
 * @see SituationIsWindowMaximized(), SituationMaximizeWindow(), SituationRestoreWindow()
 */
SITAPI void SituationSetMaximizeCallback(SituationMaximizeCallback callback, void* user_data) {
    sit_gs.maximize_callback_fn = callback;
    sit_gs.maximize_callback_user_ptr = user_data;
}

/**
 * @brief Sets a set of window state profiles for specific window behavior during different states.
 * @details This sets profiles for an active (focused) and inactive (unfocused) window, letting the application dynamically change its state based on focus to conserve power or avoid unwanted behavior.
 * @param active_flags A bitmask of `SITUATION_FLAG_*` defines to set when the window has focus.
 * @param inactive_flags A bitmask of `SITUATION_FLAG_*` defines to set when the window does not have focus.
 * @see SituationClearWindowState(), SituationApplyCurrentProfileWindowState()
 */
SITAPI SituationError SituationSetWindowStateProfiles(uint32_t active_flags, uint32_t inactive_flags) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    sit_gs.active_profile_window_flags = active_flags;
    sit_gs.inactive_profile_window_flags = inactive_flags;
    return SituationApplyCurrentProfileWindowState();
}

/**
 * @brief Manually applies the appropriate window state profile based on the current focus.
 * @details This function is the core of the profile management system. It checks if the window currently has focus and then applies the full set of flags defined in either the "active" or "inactive" profile, which were previously set by `SituationSetWindowStateProfiles`.
 *          This function is called automatically when focus changes, but can also be called manually if you need to force a re-application of the current profile's state. It orchestrates all the underlying calls to change window attributes like fullscreen, decoration, topmost, etc.
 *
 * @par Execution Order
 *   To prevent conflicts, state changes are applied in a specific order:
 *   1. Fullscreen/windowed mode is handled first, as it is the most significant state change.
 *   2. Attributes like decoration, topmost, and resizability are applied next.
 *   3. Finally, states like minimized or maximized are applied.
 *
 * @return SITUATION_SUCCESS on success, or an error code if setting any of the flags fails.
 * @see SituationSetWindowStateProfiles(), SituationToggleWindowStateFlags()
 */
SITAPI SituationError SituationApplyCurrentProfileWindowState(void) {
    if (!SituationIsInitialized() || !sit_gs.sit_glfw_window) return SITUATION_ERROR_NOT_INITIALIZED;
    // Clear any pending GLFW error before applying state changes.
    glfwGetError(NULL);

    uint32_t target_flags = sit_gs.current_window_focus_state ? sit_gs.active_profile_window_flags : sit_gs.inactive_profile_window_flags;
    bool is_currently_fullscreen = (glfwGetWindowMonitor(sit_gs.sit_glfw_window) != NULL);

    // Handle fullscreen/windowed transition first, as it can affect other attributes or require specific order.
    if (target_flags & SITUATION_FLAG_FULLSCREEN_MODE) {
        if (!is_currently_fullscreen) {
            // Save logical window coordinates before exclusive fullscreen replaces
            // the window size with the monitor/video-mode resolution.
            glfwGetWindowPos(sit_gs.sit_glfw_window, &sit_gs.windowed_x, &sit_gs.windowed_y);
            glfwGetWindowSize(sit_gs.sit_glfw_window, &sit_gs.windowed_w, &sit_gs.windowed_h);
            if (sit_gs.windowed_w <= 0 || sit_gs.windowed_h <= 0) {
                sit_gs.windowed_w = sit_gs.main_window_width;
                sit_gs.windowed_h = sit_gs.main_window_height;
            }
            int monitor_id = _SituationGetCurrentDisplayIdentifier();
            if (monitor_id == -1) { // If no specific monitor, try primary
                if (sit_gs.cached_physical_display_count > 0) {
                    for(int i=0; i < sit_gs.cached_physical_display_count; ++i) {
                        if(sit_gs.cached_physical_displays_array[i].is_primary) {
                            monitor_id = sit_gs.cached_physical_displays_array[i].situation_monitor_id;
                            break;
                        }
                    }
                    if (monitor_id == -1) monitor_id = sit_gs.cached_physical_displays_array[0].situation_monitor_id; // Fallback to first
                }
            }

            if (monitor_id != -1 && sit_gs.cached_physical_displays_array) {
                SituationDisplayInfo* disp = &sit_gs.cached_physical_displays_array[monitor_id];
                GLFWmonitor* monitor = disp->glfw_monitor_handle;
                if (monitor) {
                    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                    if (mode) {
                        sit_gs.fullscreen_w = mode->width;
                        sit_gs.fullscreen_h = mode->height;
                        glfwSetWindowMonitor(sit_gs.sit_glfw_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                    }
                }
            } else { // Absolute fallback: use GLFW primary monitor with its current mode
                GLFWmonitor* primary = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(primary);
                if (primary && mode) {
                    sit_gs.fullscreen_w = mode->width;
                    sit_gs.fullscreen_h = mode->height;
                    glfwSetWindowMonitor(sit_gs.sit_glfw_window, primary, 0, 0, mode->width, mode->height, mode->refreshRate);
                } else {
                     _SituationSetErrorFromCode(SITUATION_ERROR_DISPLAY_QUERY, "Cannot enter fullscreen, primary monitor info unavailable.");
                     // Potentially skip fullscreen attempt or return error
                }
            }
            // [FIX] Pump events after fullscreen mode change to let the OS complete the transition.
            // Without this, subsequent GL calls or event queries can block on Windows.
            glfwPollEvents();
        }
    } else { // Not fullscreen mode - ensure windowed
        if (is_currently_fullscreen) {
            // Revert to the saved logical window size, not the fullscreen
            // framebuffer/display resolution tracked by main_window_*.
            // And try to center on primary monitor (or current monitor if identifiable)
            int current_mon_id = _SituationGetCurrentDisplayIdentifier();
            GLFWmonitor* target_mon_for_windowed = NULL;
            if (current_mon_id != -1 && sit_gs.cached_physical_displays_array) {
                target_mon_for_windowed = sit_gs.cached_physical_displays_array[current_mon_id].glfw_monitor_handle;
            }
            if (!target_mon_for_windowed) target_mon_for_windowed = glfwGetPrimaryMonitor();

            const GLFWvidmode* mode = glfwGetVideoMode(target_mon_for_windowed);
            int win_x = 100, win_y = 100; // Default position
            int restore_w = sit_gs.windowed_w > 0 ? sit_gs.windowed_w : sit_gs.main_window_width;
            int restore_h = sit_gs.windowed_h > 0 ? sit_gs.windowed_h : sit_gs.main_window_height;
            int have_saved_window_rect = sit_gs.windowed_w > 0 && sit_gs.windowed_h > 0;
            if (mode) {
                int mon_x_pos, mon_y_pos;
                glfwGetMonitorPos(target_mon_for_windowed, &mon_x_pos, &mon_y_pos);
                win_x = have_saved_window_rect ? sit_gs.windowed_x : mon_x_pos + (mode->width - restore_w) / 2;
                win_y = have_saved_window_rect ? sit_gs.windowed_y : mon_y_pos + (mode->height - restore_h) / 2;
            }
            glfwSetWindowMonitor(sit_gs.sit_glfw_window, NULL, win_x, win_y, restore_w, restore_h, 0);
            sit_gs.fullscreen_w = 0;
            sit_gs.fullscreen_h = 0;
            // [FIX] Pump events after windowed mode restore to let the OS complete the transition.
            glfwPollEvents();
        }
    }

    // Apply other attributes AFTER fullscreen is handled.
    // Some attributes might be hints and only work at creation, or might be ignored in fullscreen.
    // GLFW_FLOATING, GLFW_DECORATED, GLFW_RESIZABLE are settable post-creation via glfwSetWindowAttrib.
    glfwSetWindowAttrib(sit_gs.sit_glfw_window, GLFW_FLOATING, (target_flags & SITUATION_FLAG_WINDOW_TOPMOST) ? GLFW_TRUE : GLFW_FALSE);
    glfwSetWindowAttrib(sit_gs.sit_glfw_window, GLFW_DECORATED, (target_flags & SITUATION_FLAG_WINDOW_UNDECORATED) ? GLFW_FALSE : GLFW_TRUE);
    glfwSetWindowAttrib(sit_gs.sit_glfw_window, GLFW_RESIZABLE, (target_flags & SITUATION_FLAG_WINDOW_RESIZABLE) ? GLFW_TRUE : GLFW_FALSE);

    // Hidden state
    if (target_flags & SITUATION_FLAG_WINDOW_HIDDEN) {
        if(glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_VISIBLE)) glfwHideWindow(sit_gs.sit_glfw_window);
    } else {
        if(!glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_VISIBLE)) glfwShowWindow(sit_gs.sit_glfw_window);
    }

    // Minimized/Maximized state (mutually exclusive from user's perspective of "restored")
    if (target_flags & SITUATION_FLAG_WINDOW_MINIMIZED) {
        if (!glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_ICONIFIED)) glfwIconifyWindow(sit_gs.sit_glfw_window);
    } else if (target_flags & SITUATION_FLAG_WINDOW_MAXIMIZED) { // Only consider maximize if not minimizing
        if (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_ICONIFIED)) glfwRestoreWindow(sit_gs.sit_glfw_window); // Must restore if iconified
        if (!glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_MAXIMIZED)) glfwMaximizeWindow(sit_gs.sit_glfw_window);
    } else { // Not minimized and not maximized -> should be "normal" restored state
        if (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_ICONIFIED) || glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_MAXIMIZED)) {
            glfwRestoreWindow(sit_gs.sit_glfw_window);
        }
    }

    // VSync hint (OpenGL only - Vulkan uses present mode in swapchain)
    #ifndef SITUATION_USE_VULKAN
    if (target_flags & SITUATION_FLAG_VSYNC_HINT) glfwSwapInterval(1); else glfwSwapInterval(0);
    #endif

    // Borderless windowed requires undecorated and specific size/pos, often set at creation or with fullscreen toggle logic.
    // If SITUATION_FLAG_BORDERLESS_WINDOWED_MODE is set:
    // Ensure undecorated, ensure not fullscreen, set window to monitor size/pos.
    // This is complex and might conflict with simple SITUATION_FLAG_WINDOW_UNDECORATED.
    // For now, assume SITUATION_FLAG_WINDOW_UNDECORATED covers the "no border" part.
    // True borderless windowed fullscreen usually means:
    // Undecorated, same size as monitor, positioned at monitor's 0,0.
    // This is not handled explicitly here beyond the UNDECORATED flag.

    // MSAA is a creation hint, cannot be changed on the fly easily.

    // Call framebuffer size callback manually to update viewport and projection immediately after potential size/monitor changes
    int fb_w, fb_h;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fb_w, &fb_h);
    _SituationGLFWFramebufferSizeCallback(sit_gs.sit_glfw_window, fb_w, fb_h);

    // Check if any GLFW error occurred during the state application.
    const char* desc = NULL;
    if (glfwGetError(&desc) != GLFW_NO_ERROR) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_WINDOW_STATE_FAILED,
            desc ? desc : "Window state change failed (GLFW rejected operation)");
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Toggles window state using flags in the current focus profile
 * @details Allows you to toggle individual flags in the current profile (focused or unfocused) and apply the changes immediately.
 *
 * @param flags_to_toggle A bitmask of `SITUATION_FLAG_*` defines to toggle (flip) in the profile.
 */
SITAPI SituationError SituationToggleWindowStateFlags(SituationWindowStateFlags flags_to_toggle) {
    if (!SituationIsInitialized()) return SITUATION_ERROR_NOT_INITIALIZED;
    uint32_t* profile_to_modify = sit_gs.current_window_focus_state ? &sit_gs.active_profile_window_flags : &sit_gs.inactive_profile_window_flags;

    // Special handling for mutually exclusive flags like MINIMIZED/MAXIMIZED/FULLSCREEN
    if (flags_to_toggle & SITUATION_FLAG_FULLSCREEN_MODE) {
        // If toggling fullscreen ON, turn off maximized/minimized in the profile
        if (!(*profile_to_modify & SITUATION_FLAG_FULLSCREEN_MODE)) { // Means we are turning it ON
            *profile_to_modify &= ~(SITUATION_FLAG_WINDOW_MAXIMIZED | SITUATION_FLAG_WINDOW_MINIMIZED);
        }
    } else if (flags_to_toggle & SITUATION_FLAG_WINDOW_MAXIMIZED) {
        if (!(*profile_to_modify & SITUATION_FLAG_WINDOW_MAXIMIZED)) { // Turning Maximized ON
            *profile_to_modify &= ~(SITUATION_FLAG_FULLSCREEN_MODE | SITUATION_FLAG_WINDOW_MINIMIZED);
        }
    } else if (flags_to_toggle & SITUATION_FLAG_WINDOW_MINIMIZED) {
         if (!(*profile_to_modify & SITUATION_FLAG_WINDOW_MINIMIZED)) { // Turning Minimized ON
            *profile_to_modify &= ~(SITUATION_FLAG_FULLSCREEN_MODE | SITUATION_FLAG_WINDOW_MAXIMIZED);
        }
    }
    // Regular toggle for the requested flag
    *profile_to_modify ^= (uint32_t)flags_to_toggle;
    return SituationApplyCurrentProfileWindowState();
}

/**
 * @brief Gets a bitmask representing the window's *actual*, current state.
 * @details This function queries the underlying windowing system (GLFW) for the real-time status of various window attributes (e.g., is it *really* maximized, is it *really* fullscreen) and combines them into a single bitmask of `SITUATION_FLAG_*` flags.
 *          This is useful for debugging or for comparing the window's actual state against the *target* state defined in the window profiles.
 *
 * @return A `uint32_t` bitmask composed of `SITUATION_FLAG_*` defines that reflect the current window state. Returns 0 if the library is not initialized.
 * @see SituationIsWindowState()
 */
/* Internal: compute the live window state by querying GLFW attributes.
 * Called by SituationPollInputEvents to refresh the cache, and on-demand
 * when the cache hasn't been populated yet (e.g. before the first Poll). */
static uint32_t _SituationComputeWindowStateFlags(void) {
    if (!SituationIsInitialized() || !sit_gs.sit_glfw_window) return 0;
    uint32_t flags = 0;
    if (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_FLOATING)) flags |= SITUATION_FLAG_WINDOW_TOPMOST;
    if (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_VISIBLE) == GLFW_FALSE) flags |= SITUATION_FLAG_WINDOW_HIDDEN;
    if (glfwGetWindowMonitor(sit_gs.sit_glfw_window) != NULL) flags |= SITUATION_FLAG_FULLSCREEN_MODE;
    if (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_DECORATED) == GLFW_FALSE) flags |= SITUATION_FLAG_WINDOW_UNDECORATED;
    // SITUATION_FLAG_WINDOW_ALWAYS_RUN - Not directly queryable
    if (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_ICONIFIED)) flags |= SITUATION_FLAG_WINDOW_MINIMIZED;
    if (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_MAXIMIZED)) flags |= SITUATION_FLAG_WINDOW_MAXIMIZED;
    if (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_RESIZABLE)) flags |= SITUATION_FLAG_WINDOW_RESIZABLE;

    // SITUATION_FLAG_MSAA_4X_HINT: Can't query samples from default FBO easily after creation.
    // VSync: GLFW cannot query swap interval; reflect the profile last applied by
    // SituationApplyCurrentProfileWindowState (same focus rule as glfwSwapInterval there).
    {
        uint32_t applied_profile = sit_gs.current_window_focus_state
            ? sit_gs.active_profile_window_flags
            : sit_gs.inactive_profile_window_flags;
        if (applied_profile & SITUATION_FLAG_VSYNC_HINT) {
            flags |= SITUATION_FLAG_VSYNC_HINT;
        }
    }

    // Borderless check: if undecorated, not fullscreen, and window size matches monitor size.
    // This is a heuristic and can be complex.
    if ((flags & SITUATION_FLAG_WINDOW_UNDECORATED) && !(flags & SITUATION_FLAG_FULLSCREEN_MODE)) {
        int win_w, win_h, mon_w = 0, mon_h = 0;
        glfwGetWindowSize(sit_gs.sit_glfw_window, &win_w, &win_h);
        int mon_id = _SituationGetCurrentDisplayIdentifier();
        if (mon_id != -1 && sit_gs.cached_physical_displays_array) {
            mon_w = sit_gs.cached_physical_displays_array[mon_id].current_mode.width;
            mon_h = sit_gs.cached_physical_displays_array[mon_id].current_mode.height;
        }
        if (win_w == mon_w && win_h == mon_h) {
            flags |= SITUATION_FLAG_BORDERLESS_WINDOWED_MODE;
        }
    }
    return flags;
}

/* Public API: returns the cached window state flags refreshed by SituationPollInputEvents.
 * O(1) in the normal frame loop. Falls back to a live query only before the first poll
 * (cache is zero) so early calls (e.g. right after SituationInit) still work correctly. */
SITAPI uint32_t SituationGetCurrentActualWindowStateFlags(void) {
    if (!SituationIsInitialized() || !sit_gs.sit_glfw_window) return 0;
    if (sit_gs.cached_window_state_flags == 0) {
        /* Cache not yet populated (before first SituationPollInputEvents call).
         * Compute and store so subsequent calls in the same pre-poll window are also fast. */
        sit_gs.cached_window_state_flags = _SituationComputeWindowStateFlags();
    }
    return sit_gs.cached_window_state_flags;
}

// --- Application Pause/Resume Implementation ---
/**
 * @brief Pauses the library's internal, time-dependent subsystems.
 * @details This function sets an internal flag that signals the application is in a paused state. Its primary effect is to pause the audio device, stopping all sound playback.
 *          This function is called automatically when the window is minimized (via the `_SituationGLFWWindowIconifyCallback`) and can also be called manually by the user to implement an in-game pause menu or similar functionality.
 *
 * @note This function only affects library subsystems. It does not stop the main application loop from running. It is the developer's responsibility to check `SituationIsAppPaused()` in their main loop and halt their own game logic and updates accordingly.
 *
 * @see SituationResumeApp(), SituationIsAppPaused(), SituationPauseAudioDevice()
 */
SITAPI void SituationPauseApp(void) { // Largely same logic
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationPauseApp"); return; }
    if (sit_gs.is_app_internally_paused) return;
    sit_gs.is_app_internally_paused = true;
    SituationPauseAudioDevice();
    // Minimizing window on pause is now handled by profile flags in ApplyCurrentProfileWindowState,
    // triggered by focus loss or explicit call.
    // Or if iconify callback calls this, it might re-apply inactive profile which could include minimize.
}

/**
 * @brief Resumes the library's internal, time-dependent subsystems from a paused state.
 * @details This function clears the internal paused-state flag and resumes any subsystems that were halted by `SituationPauseApp()`, most notably by resuming audio playback on the main audio device.
 *          This function is called automatically when a minimized window is restored and can also be called manually by the user to un-pause the application.
 *
 * @see SituationPauseApp(), SituationIsAppPaused(), SituationResumeAudioDevice()
 */
SITAPI void SituationResumeApp(void) { // Largely same logic
    if (!SituationIsInitialized()) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationResumeApp"); return; }
    if (!sit_gs.is_app_internally_paused) return;
    sit_gs.is_app_internally_paused = false;
    SituationResumeAudioDevice();
    // Window restoration handled by profile or iconify callback.
}

/**
 * @brief Checks if the application is currently in an internally-paused state.
 * @details This function queries the flag set by `SituationPauseApp()` and `SituationResumeApp()`. It is intended to be used in the main application loop to control whether game logic, physics, or animations should be updated.
 *
 * @example
 *   // In your main loop, after updating timers:
 *   if (!SituationIsAppPaused()) {
 *       UpdatePhysics(delta_time);
 *       UpdateAnimations(delta_time);
 *   }
 *
 * @return `true` if the application is paused, `false` otherwise.
 * @return `true` if the library has not yet been initialized.
 *
 * @see SituationPauseApp(), SituationResumeApp()
 */
SITAPI bool SituationIsAppPaused(void) {
    if (!SituationIsInitialized()) return true;
    return sit_gs.is_app_internally_paused;
}

/**
 * @brief Gets the raw, underlying GLFW window handle.
 * @details This function provides direct access to the `GLFWwindow*` managed by the library. This is an escape hatch for advanced users who need to call a GLFW function that is not wrapped by the Situation API.
 *
 * @return The `GLFWwindow*` handle on success.
 * @return `NULL` if the library is not initialized or the window has not been created.
 *
 * @warning Use with caution. Directly manipulating the GLFW window or its context can interfere with the library's internal state management and may lead to unexpected behavior.
 */
SITAPI GLFWwindow* SituationGetGLFWwindow(void) {
    if (!SituationIsInitialized() || !sit_gs.sit_glfw_window) { _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "GLFW window not initialized"); return NULL; }
    return sit_gs.sit_glfw_window;
}

/**
 * @brief Gets the current logical size of the window.
 * @details This is a convenience function that retrieves both width and height in a single call.
 * @param[out] width A pointer to an integer where the window width in screen coordinates will be stored.
 * @param[out] height A pointer to an integer where the window height in screen coordinates will be stored.
 * @see SituationGetScreenWidth(), SituationGetScreenHeight()
 */
SITAPI void SituationGetWindowSize(int* width, int* height) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetWindowSize");
        // If the engine isn't running, return 0 to avoid errors.
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    // Use the underlying windowing library's function to get the size.
    // For GLFW, this is glfwGetWindowSize().
    glfwGetWindowSize(sit_gs.sit_glfw_window, width, height);
}

// Checks if the GLFW window should close (e.g., user clicked close button).
// Returns true if not initialized or window is null, setting an error.
// Use in the main loop to control application exit.
SITAPI bool SituationWindowShouldClose(void) {
    if (!SituationIsInitialized() || !sit_gs.sit_glfw_window) {
        SIT_DEBUG_LOG("[WindowShouldClose] Not initialized or no window");
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot check window close state");
        return true; // If not init'd, effectively should "close" or not run.
    }
    int should_close = glfwWindowShouldClose(sit_gs.sit_glfw_window);
    SIT_DEBUG_LOG("[WindowShouldClose] glfwWindowShouldClose returned: %d", should_close);
    return should_close == GLFW_TRUE;
}

/**
 * @brief Sets a desired frame rate cap.
 * @details Pass 0 to disable the cap. This is independent of VSync.
 * @param fps The target frames per second.
 * @note This function requires the library to be initialized (`SituationInit` must have been called successfully).
 * @see SituationGetFrameTime(), SituationGetFPS()
 */
SITAPI void SituationSetTargetFPS(int fps) {
    // --- 1. Pre-condition Check ---
    if (!SituationIsInitialized()) {
        // As per library convention, silently return or set a general error if called incorrectly.
        // The provided text shows similar functions returning early.
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetTargetFPS: Library not initialized.");
        return;
    }

    // --- 2. Set Target Frame Time ---
    if (fps <= 0) {
        // Disable frame rate limiting by setting target time to 0.0.
        sit_gs.target_frame_time = 0.0;
    } else {
        // Calculate the target time per frame in seconds.
        sit_gs.target_frame_time = 1.0 / (double)fps;
    }
}

/**
 * @brief Gets the time in seconds for the last frame to complete (deltaTime).
 * @details Use this for frame-rate-independent movement and physics.
 *          This value is updated once per frame in the main loop.
 * @return The last frame time (deltaTime) in seconds. Returns 0.0f if the library is not initialized.
 * @note This function requires the library to be initialized.
 * @see SituationSetTargetFPS(), SituationGetFPS()
 */
SITAPI float SituationGetFrameTime(void) {
    // --- 1. Pre-condition Check ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetFrameTime");
        return 0.0f;
    }

    // --- 2. Return Stored Value ---
    // The frame time is updated in the main loop (e.g., within SituationEndFrame or similar timing update function).
    return (float)sit_gs.frame_time;
}

/**
 * @brief Gets the current frames-per-second value.
 * @details This value is updated periodically (e.g., once per second) within the library's main loop.
 * @return The current calculated FPS. Returns 0 if the library is not initialized.
 * @note This function requires the library to be initialized.
 * @see SituationSetTargetFPS(), SituationGetFrameTime()
 */
SITAPI int SituationGetFPS(void) {
    // --- 1. Pre-condition Check ---
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationGetFPS");
        return 0;
    }

    // --- 2. Return Stored Value ---
    // The current FPS is calculated and stored in the global state.
    return sit_gs.current_fps;
}

/**
 * @brief Frees the memory for the array of display information returned by `SituationGetDisplays`.
 * @details This function correctly handles the complex nested deallocation required for the `SituationDisplayInfo` array, freeing both the inner `available_modes` and the top-level array. This is the only safe way to clean up this resource.
 * @param displays The array of `SituationDisplayInfo` structs to be freed.
 * @param count The number of elements in the array, as returned by `SituationGetDisplays`.
 */
SITAPI void SituationFreeDisplays(SituationDisplayInfo* displays, int count) {
    if (!displays || count == 0) return;
    for (int i = 0; i < count; ++i) {
        if (displays[i].available_modes) {
            SIT_FREE(displays[i].available_modes);
        }
    }
    SIT_FREE(displays);
}

#endif // SITUATION_IMPL_WDM_H