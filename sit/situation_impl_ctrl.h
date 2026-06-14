/***************************************************************************************************
*
* situation_impl_ctrl.h - Control Module (Lifecycle, Error Handling, Init/Shutdown)
* (c) 2025-2026 Jacques Morel
* MIT Licensed
*
* This file contains the control plane of the Situation library:
* - Error handling and logging
* - Library initialization and shutdown
* - Platform and window initialization
* - Update loop (poll events, update timers)
* - Callbacks, arguments, clipboard, file drop
* - State queries (IsInitialized, GetRendererType, etc.)
*
* This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_CTRL_H
#define SITUATION_IMPL_CTRL_H

static void _SituationSetError(const char* msg) {
    // Safety check: Cannot record error if context doesn't exist
    if (!_sit_current_context) return;

    // A single, consistent string to use for NULL input.
    const char* default_error_msg = "Unknown error";
    const char* message_to_copy = msg ? msg : default_error_msg;

    // Lock the mutex to ensure exclusive access to the shared error message buffer.
    ma_mutex_lock(&sit_gs.error_mutex);
    {
        // Use strncpy to safely copy the message, preventing buffer overflows if the source string is longer than the destination buffer. It will copy at most (SITUATION_MAX_ERROR_MSG_LEN - 1) characters.
        strncpy(sit_gs.last_error_msg, message_to_copy, SITUATION_MAX_ERROR_MSG_LEN - 1);

        // CRITICAL: strncpy does not guarantee null-termination if the source string is exactly as long as or longer than the destination buffer size. We must manually ensure the string is always null-terminated for safety.
        sit_gs.last_error_msg[SITUATION_MAX_ERROR_MSG_LEN - 1] = '\0';
    }
    // Unlock the mutex as soon as the critical section is finished.
    ma_mutex_unlock(&sit_gs.error_mutex);
}

static int _sit_trace_log_level = SIT_LOG_INFO;

SITAPI void SituationSetTraceLogLevel(int logType) {
    _sit_trace_log_level = logType;
}

SITAPI void SituationSetLogCallback(void (*callback)(SituationLogLevel level, const char* message, void* user), void* user) {
    if (_sit_current_context) {
        sit_gs.log_callback = callback;
        sit_gs.log_user_data = user;
    }
}

SITAPI void SituationShowMessageBox(const char* title, const char* message) {
#if defined(_WIN32)
    MessageBoxA(NULL, message ? message : "", title ? title : "Situation Message", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
#else
    fprintf(stderr, "\n=== %s ===\n%s\n================\n", title ? title : "Message", message ? message : "");
#endif
}

SITAPI void SituationLog(int msgType, const char* text, ...) {
    if (msgType < _sit_trace_log_level) return;

    char buffer[4096];
    va_list args;
    va_start(args, text);
    vsnprintf(buffer, sizeof(buffer), text, args);
    va_end(args);

    if (_sit_current_context && sit_gs.log_callback) {
        sit_gs.log_callback((SituationLogLevel)msgType, buffer, sit_gs.log_user_data);
        return;
    }

    // ANSI Color Codes
    const char* color_code = "\033[0m"; // Reset
    const char* label = "[INFO]";

    switch (msgType) {
    case SIT_LOG_TRACE: label = "[TRACE]"; color_code = "\033[90m"; break; // Gray
    case SIT_LOG_DEBUG: label = "[DEBUG]"; color_code = "\033[36m"; break; // Cyan
    case SIT_LOG_INFO: label = "[INFO]"; color_code = "\033[32m"; break; // Green
    case SIT_LOG_WARNING: label = "[WARN]"; color_code = "\033[33m"; break; // Yellow
    case SIT_LOG_ERROR: label = "[ERROR]"; color_code = "\033[31m"; break; // Red
    case SIT_LOG_FATAL: label = "[FATAL]"; color_code = "\033[41m"; break; // Red Background
    default: break;
    }

    // Print to stdout with color
    printf("%s%s %s\033[0m\n", color_code, label, buffer);
}

SITAPI void SituationLogWarning(SituationError code, const char* fmt, ...) {
#ifndef NDEBUG
    char _sit_err_buf[SITUATION_MAX_ERROR_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(_sit_err_buf, sizeof(_sit_err_buf), fmt, args);
    va_end(args);
    _SituationSetErrorFromCode(code, _sit_err_buf);
    fprintf(stderr, "[Situation WARN] %s\n", _sit_err_buf);
#else
    (void)code; (void)fmt;
#endif
}

/**
* @brief [INTERNAL] Sets the library's last error message from an error code and an optional detail string.
* @details Translates a `SituationError` enum into a human-readable base message, appends details,
* and stores the result in the global error message buffer via `_SituationSetError`.
* @param err The `SituationError` code to translate.
* @param detail An optional, more specific string describing the context of the error.
*/
static SituationError _SituationSetErrorFromCode(SituationError err, const char* detail) {
    if (!_sit_current_context) {
        return err;
    }

    char buffer[SITUATION_MAX_ERROR_MSG_LEN];
    const char* base_msg = "Unknown Error";

    switch (err) {
#define _SIT_ERRNO_MSG(name, value, msg) case name: base_msg = msg; break;
        SITUATION_ERROR_TABLE(_SIT_ERRNO_MSG)
#undef _SIT_ERRNO_MSG
    default: break;
    }

    if (detail) {
        snprintf(buffer, sizeof(buffer), "%s: %s", base_msg, detail);
    } else {
        strncpy(buffer, base_msg, sizeof(buffer) - 1);
    }
    buffer[sizeof(buffer) - 1] = '\0';
    ma_mutex_lock(&sit_gs.error_mutex);
    sit_gs.last_error_code = err;
    ma_mutex_unlock(&sit_gs.error_mutex);
    _SituationSetError(buffer);
    return err;
}

SITAPI const char* SituationErrorToString(SituationError err) {
    switch (err) {
#define _SIT_ERRNO_STR(name, value, msg) case name: return msg;
        SITUATION_ERROR_TABLE(_SIT_ERRNO_STR)
#undef _SIT_ERRNO_STR
    default: return "Unknown error";
    }
}

SITAPI SituationError SituationGetLastErrorCode(void) {
    if (!_sit_current_context) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    ma_mutex_lock(&sit_gs.error_mutex);
    SituationError code = sit_gs.last_error_code;
    ma_mutex_unlock(&sit_gs.error_mutex);
    return code;
}

/**
* @brief Retrieves a copy of the last error message generated by the library.
* @details This function provides a way for the user application to obtain a detailed, human-readable description of the most recent error that occurred within the Situation library. This is essential for diagnosing problems and providing feedback to the user.
*
* @par Memory Management
* This function returns a *copy* of the internal error message string, allocating new memory for it.
* The caller takes ownership of this memory and **must** release it by calling `SituationFreeString()` when it is no longer needed. Failure to do so will result in a memory leak.
*
* @warning Do NOT use `free()` on the returned pointer. Always use `SituationFreeString()`.
*
* @return A null-terminated C string containing the last error message.
* @return NULL if the library is not initialized, if no error has occurred, or if memory allocation for the copy fails.
*
* @see SituationFreeString(), _SituationSetError()
*/
SITAPI SituationError SituationGetLastErrorMsg(char** out_msg) {
    if (out_msg) *out_msg = NULL;
    else return SITUATION_ERROR_INVALID_PARAM;

    // --- 1. Input/State Validation ---
    // Check if the context exists.
    if (!_sit_current_context) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }

    // A mutex lock/unlock could be added here for perfect thread-safety if another thread could be setting an error while this one is reading.
    // For now, we assume this is called on the main thread shortly after an error.
    ma_mutex_lock(&sit_gs.error_mutex);

    // Check if the internal error message is empty.
    if (sit_gs.last_error_msg[0] == '\0') {
        ma_mutex_unlock(&sit_gs.error_mutex);
        return SITUATION_SUCCESS; // No error set
    }

    // --- 2. Allocate Memory for the Copy ---
    // Determine the length of the internal error message.
    size_t msg_len = strlen(sit_gs.last_error_msg);

    // Allocate memory for the copy, including space for the null terminator.
    char* msg_copy = (char*)SIT_MALLOC(msg_len + 1);
    if (!msg_copy) {
        ma_mutex_unlock(&sit_gs.error_mutex);
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // --- 4. Copy the Error Message ---
    // Copy the string while the mutex is still held to prevent a data race.
    strcpy(msg_copy, sit_gs.last_error_msg);

    ma_mutex_unlock(&sit_gs.error_mutex);

    // --- 5. Return the Copy ---
    // The caller now owns this memory and is responsible for calling SituationFreeString().
    *out_msg = msg_copy;
    return SITUATION_SUCCESS;
}

/**
* @brief [INTERNAL] GLFW error callback function.
*
* @details This function is registered with GLFW using `glfwSetErrorCallback`. It is called by GLFW whenever an internal error occurs within the GLFW library.
* This allows the Situation library to capture these errors, record them in its own error state, and potentially log them for debugging purposes.
*
* @param error An integer error code defined by GLFW (e.g., GLFW_NOT_INITIALIZED, GLFW_INVALID_VALUE, GLFW_PLATFORM_ERROR).
* @param description A null-terminated string providing a human-readable description of the error.
*
* @note This function is for internal use by the Situation library and should not be called directly by user code.
* @note This function modifies the global state `sit_gs.last_error_msg`.
* @warning This function might be called asynchronously from within other GLFW functions (e.g., during `glfwPollEvents`). Therefore, it should be thread-safe. Using `_SituationSetError` (which likely just writes to `sit_gs.last_error_msg`) and `fprintf` to `stderr` is generally acceptable for this.
*
* @see SituationInit(), glfwSetErrorCallback(), _SituationSetError()
*/
static void _SituationGLFWErrorCallback(int error, const char* description) {
    SIT_DEBUG_LOG("[GLFW_ERROR] Code: %d, Description: %s", error, description);

    // --- 1. Input Validation ---
    // While GLFW should provide valid inputs, checking is good practice.
    if (!description) {
        // Description is NULL. This is unusual but possible.
        // Use a default string to prevent crashes.
        description = "<no description provided by GLFW>";
    }

    // --- 2. Format the Error Message ---
    // Create a formatted message string that combines the GLFW error code
    // and its description for clarity and consistency with the library's error format.
    char err_buf[SITUATION_MAX_ERROR_MSG_LEN]; // Use the library's defined max length

    // Use snprintf for safer string formatting, preventing potential buffer overflows.
    // Cast `error` to unsigned int for consistent formatting across platforms.
    int written = snprintf(
    err_buf,
    sizeof(err_buf),
    "_SituationGLFWErrorCallback: GLFW Error (%d): %s",
    error,
    description
    );

    // --- 3. Handle Formatting Errors ---
    // Check if snprintf truncated the output or failed.
    if (written < 0 || (size_t)written >= sizeof(err_buf)) {
        // If snprintf failed or truncated, provide a fallback message.
        // This ensures *some* error is recorded, even if details are lost.
        snprintf(
        err_buf,
        sizeof(err_buf),
        "_SituationGLFWErrorCallback: GLFW Error (%d) - Error formatting failed or message too long. Original description started with: %.50s...",
        error,
        description // Use first 50 chars of description as a hint
        );
    }

    // --- 4. Store the Error in Global State ---
    // Update the library's global error state with the formatted message.
    // This makes the error retrievable via `SituationGetLastErrorMsg()`.
    // _SituationSetError simply copies the string into sit_gs.last_error_msg.
    _SituationSetError(err_buf);

    // --- 5. Debug Output ---
    // Print the error message immediately to stderr for visibility during
    // development and debugging. This provides instant feedback.
    // It's generally acceptable for an error callback to log to stderr.
    fprintf(stderr, "[SITUATION] [GLFW ERROR] %s\n", err_buf);
    // fflush(stderr); // Optional: Force immediate output if stderr is buffered.
}

/**
* @brief [INTERNAL] GLFW callback function invoked when files are dropped onto the window.
* @details This function is the primary handler for file drop events. It is registered with GLFW and called automatically when the user drags one or more files from their operating system and releases them over the application window.
*
* @par Dual-Mode Handling
* This callback serves two purposes, supporting both event-driven and polling-based APIs:
* 1. **Event-Driven:** If a user callback has been registered via `SituationSetFileDropCallback`, this function immediately invokes it, passing along the file count, paths, and user data.
* 2. **Polling-Based:** It then creates a deep copy of the file path list and stores it in the global state (`sit_gs.dropped_file_paths`). It also sets the `sit_gs.file_was_dropped_this_frame` flag. This allows the polling functions `SituationIsFileDropped()` and `SituationLoadDroppedFiles()` to work correctly.
*
* @param window The GLFW window that received the event (unused).
* @param count The number of files that were dropped.
* @param paths An array of null-terminated, UTF-8 encoded strings, where each string is the absolute path to a dropped file. The memory for this array is managed by GLFW and is only valid for the duration of the callback.
*
* @note This function is for internal use only and is registered during library initialization.
*
* @see SituationSetFileDropCallback(), SituationIsFileDropped(), SituationLoadDroppedFiles()
*/
static void _SituationGLFWFileDropCallback(GLFWwindow* window, int count, const char** paths) {
    (void)window;

    // --- First, handle the user-defined callback, if it exists ---
    if (sit_gs.file_drop_callback != NULL) {
        sit_gs.file_drop_callback(count, paths, sit_gs.file_drop_user_data);
    }

    // --- Now, handle the internal state for the polling API ---

    // Clear any previous list of dropped files
    if (sit_gs.dropped_file_paths != NULL) {
        for (int i = 0; i < sit_gs.dropped_file_count; i++) {
            SIT_FREE(sit_gs.dropped_file_paths[i]);
        }
        SIT_FREE(sit_gs.dropped_file_paths);
        sit_gs.dropped_file_paths = NULL;
        sit_gs.dropped_file_count = 0;
    }

    if (count > 0) {
        sit_gs.dropped_file_paths = (char**)SIT_MALLOC(count * sizeof(char*));
        if (sit_gs.dropped_file_paths == NULL) {
            sit_gs.dropped_file_count = 0;
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_SituationGLFWFileDropCallback: failed to allocate file path array");
            return; // Allocation failed
        }

        for (int i = 0; i < count; i++) {
            sit_gs.dropped_file_paths[i] = _sit_strdup(paths[i]);
            if (sit_gs.dropped_file_paths[i] == NULL) {
                // Allocation failed for one string, clean up what we have
                for (int j = 0; j < i; j++) SIT_FREE(sit_gs.dropped_file_paths[j]);
                SIT_FREE(sit_gs.dropped_file_paths);
                sit_gs.dropped_file_paths = NULL;
                sit_gs.dropped_file_count = 0;
                _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_SituationGLFWFileDropCallback: failed to duplicate file path string");
                return;
            }
        }
        sit_gs.dropped_file_count = count;
        sit_gs.file_was_dropped_this_frame = true;
    }
}

/**
* @brief [INTERNAL] Performs a comprehensive cleanup of all partially initialized library components.
*
* @details This helper function is a critical part of the library's robust initialization and error handling strategy. It is designed to be called safely from *any* point during the `SituationInit` process if a critical error occurs.
*
* Its primary role is to prevent resource leaks and ensure a clean state in the event of a failed startup. It achieves this by:
* 1. Attempting to flush any pending GPU operations to ensure resources are no longer in use.
* 2. Calling the main cleanup functions for subsystems, the renderer, and the platform layer, *in reverse order* of their initialization.
* 3. Relying on the robustness of these individual cleanup functions to handle being called when only parts of their corresponding initialization were completed.
*
* @note This function is intended solely for internal use during the `SituationInit` failure path and should not be called directly by user code under normal circumstances.
* @note The cleanup proceeds in reverse order of initialization:
* Platform -> Renderer -> Subsystems. This order is crucial to avoid dependencies (e.g., destroying GLFW window before Vulkan surface).
* @note Each `_SituationCleanup*` function called is designed to check if its corresponding component was initialized before attempting to destroy it, making this "best-effort" cleanup safe.
* @warning This function modifies the global state `sit_gs`. After it runs, the library is considered uninitialized.
*
* @see SituationInit(), _SituationCleanupSubsystems(), _SituationCleanupRenderer(), _SituationCleanupPlatform()
*/
static void _SituationFullCleanupOnError(void) {
    // --- 1. Synchronize GPU ---
    // Before attempting to destroy any graphics resources, it's prudent to wait for the GPU to finish any operations that might be using them.
    // This helps prevent validation errors or crashes during cleanup, especially if the error occurred partway through renderer initialization.
#if defined(SITUATION_USE_VULKAN)
    {
        // For Vulkan, wait for the logical device to be idle.
        // This ensures all submitted work on all queues is finished.
        // It's safe to call this even if the device creation failed, as sit_render.vk.device would be VK_NULL_HANDLE.
        if (sit_render.vk.device != VK_NULL_HANDLE) {
            _SituationVulkanWaitInFlightFencesPump("SituationInit error cleanup");
        }
    }
#elif defined(SITUATION_USE_OPENGL)
    {
        // For OpenGL, if a context was made current (indicated by a valid window and the context being current on this thread), force the GPU to finish.
        // This is a coarse synchronization but can help ensure buffer/object deletion is safe. It stalls the GPU pipeline.
        // Note: glfwGetCurrentContext() could also be used for a more precise check.
        if (sit_gs.sit_glfw_window != NULL) {
            // Ensure the context is current before calling glFinish.
            // This is generally safe as SituationInit sets up the context.
            glfwMakeContextCurrent(sit_gs.sit_glfw_window);
            glFinish(); // This is a blocking call. Consider glFlush() for less blocking?
            // glFinish ensures all previously issued GL commands are complete.
            // Note: Errors during init might mean the context is in a bad state, but calling glFinish is still generally safe.
        }
    }
#endif // SITUATION_USE_OPENGL

    // --- 2. Cleanup in Reverse Order of Initialization ---
    // It's crucial to destroy components in the reverse order they were initialized to respect dependencies (e.g., don't destroy the window before the Vulkan surface).

    // --- 2a. Cleanup Subsystems ---
    // This cleans up audio, input, timer systems, etc.
    // It should be safe to call even if subsystem init failed partway through.
    _SituationCleanupSubsystems();

    // --- 2b. Cleanup Renderer ---
    // This cleans up all graphics API specific resources (Vulkan instance/device/swapchain, OpenGL context/states, internal pipelines, etc.).
    // It must be robust enough to handle partial initialization (e.g., if Vulkan device creation failed but instance was created).
    _SituationCleanupRenderer();

    // --- 2c. Cleanup Platform ---
    // This cleans up the lowest level components: GLFW window, GLFW itself, any platform-specific initializations (like COM on Windows).
    // This should be the final step.
    _SituationCleanupPlatform();

    // --- 3. Release init-time context so a later SituationInit can calloc fresh (harness misc GPU tests, etc.) ---
    if (_sit_current_context) {
        ma_mutex_uninit(&sit_gs.error_mutex);
        SIT_FREE(_sit_current_context);
        _sit_current_context = NULL;
    }
}

/**
* @brief Returns a human-readable version string for the Situation library.
*
* @details Provides a null-terminated string containing the full version information
* in the format:
* "Major.Minor.Patch [optional suffix/revision]"
*
* Examples of possible returns:
* "2.3.54"
* "2.3.54-dev"
* "2.3.54 [Velocity Hot-Reload]"
*
* The string is statically allocated and constant '' -- safe to use indefinitely
* without freeing or copying. The pointer remains valid for the entire program
* lifetime.
*
* Intended for:
* - Logging at startup ("Situation v2.3.54 initialized")
* - Debug overlays / about dialogs
* - Version checks in tools/plugins
* - Fun Easter eggs ("Powered by Situation 2.3.54 '' -- " -- ")
*
* @return A constant, null-terminated string representing the current library version.
* Never returns NULL.
*
* @note Thread-safe, allocation-free, zero-cost call '' -- can be used anywhere, anytime,
* even before full initialization (though most useful after `SituationInit`).
* The exact format may include build date, git hash, or feature flags in future
* releases '' -- do not parse programmatically (use version macros instead).
*
* @see SITUATION_VERSION_MAJOR, SITUATION_VERSION_MINOR,
* SITUATION_VERSION_PATCH, SITUATION_VERSION_REVISION
*/
#define _SIT_STR_HELPER(x) #x
#define _SIT_STR(x) _SIT_STR_HELPER(x)

SITAPI const char* SituationGetVersionString(void) {
    return _SIT_STR(SITUATION_VERSION_MAJOR) "."
    _SIT_STR(SITUATION_VERSION_MINOR) "."
    _SIT_STR(SITUATION_VERSION_PATCH)
    SITUATION_VERSION_REVISION " (" SITUATION_VERSION_DESCRIPTION ")";
}

/**
* @brief Primary entry point to initialize the entire Situation library.
*
* @details This is the **single most important public function** in the library.
* It must be called exactly once at the very beginning of the application,
* before any other Situation API function is used.
*
* `SituationInit` orchestrates the complete bootstrap sequence:
* 1. Parses command-line arguments (if provided) for overrides/debug flags
* 2. Validates and merges `init_info` with defaults/command-line
* 3. Initializes platform/OS abstractions (CoInitialize on Win32, etc.)
* 4. Sets up global error/logging state
* 5. Calls `_SituationInitSubsystems(init_info)` '' -- the real workhorse:
* - GLFW init + window creation (or reuse external window)
* - Backend-specific context setup:
* - OpenGL: GLAD load, version check, state defaults
* - Vulkan: instance/device/queues/swapchain creation
* - Render thread startup (if enabled)
* - Internal resources: default texture, dummy VAO, quad pipeline
* - Text renderer, bindless support, audio device
* - Thread pool (if threading enabled)
* - Filesystem watchers / hot-reload (if Velocity enabled)
* 6. Performs final validation (GL/VK errors, extension checks)
* 7. Sets `sit_gs.initialized = true` and other global state flags
*
* If initialization fails at any critical point, the function returns early
* with an appropriate error code. Non-critical failures (e.g. missing optional
* extension) log warnings and continue in degraded mode where possible.
*
* @param argc Number of command-line arguments (from main())
* @param argv Command-line argument array (from main())
* Recognized flags (implementation-defined, e.g. --debug, --no-render-thread,
* --vulkan, --opengl, --hot-reload-rate=0.5, etc.)
* @param init_info Pointer to `SituationInitInfo` struct containing user preferences:
* - window title, size, resizable, borderless, etc.
* - preferred backend (Vulkan/OpenGL override)
* - feature toggles (render thread, threading, audio, hot-reload)
* - custom window hints (GLFW or Vulkan)
* - May be NULL for strict defaults
*
* @return SITUATION_SUCCESS if the library is fully initialized and ready for use,
* SITUATION_ERROR_ALREADY_INITIALIZED if called more than once,
* SITUATION_ERROR_INIT_FAILED on critical early failure (GLFW init fail, etc.),
* SITUATION_ERROR_GLAD_LOAD_FAILED / SITUATION_ERROR_VULKAN_INSTANCE_FAILED
* for backend-specific context creation failures,
* SITUATION_ERROR_THREAD_CREATION_FAILED if render thread failed to start,
* SITUATION_ERROR_MEMORY_ALLOCATION for internal resource failures,
* or any other propagated subsystem error code.
*
* @note **Must be called from the main thread** before any rendering, input, or
* multi-threaded use of the library.
* Thread safety: Not thread-safe '' -- only safe from the main thread during startup.
* If the function returns failure, the library is in an undefined state.
* Caller should **not** attempt to use any Situation API and should call
* `SituationShutdown` (if partial init occurred) before exiting.
*
* Typical usage in main():
* ```c
* int main(int argc, char** argv) {
    * SituationInitInfo info = {0};
    * info.window_width = 1280;
    * info.window_height = 720;
    * info.title = "My App";
    * info.enable_render_thread = true;
    *
    * if (SituationInit(argc, argv, &info) != SITUATION_SUCCESS) {
        * fprintf(stderr, "Init failed: %s\n", SituationGetLastErrorMsg());
        * return 1;
        * }
    *
    * // Main loop...
    *
    * SituationShutdown();
    * return 0;
    * }
* ```
*
* @see SituationShutdown, SituationInitInfo,
* _SituationInitSubsystems (core worker), SITUATION_ERROR_INIT_FAILED,
* SITUATION_ERROR_ALREADY_INITIALIZED
*/
SITAPI SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info) {
    static int call_count = 0;
    call_count++;
    SIT_DEBUG_LOG("=== SituationInit START (Call #%d) ===", call_count);

    if (call_count > 1) {
        SIT_DEBUG_LOG("[WARNING] Recursive call detected! Call #%d", call_count);
    }

    // Explicitly initialize the context pointer if this is the first call
    // This handles cases where static initialization might not work properly
    static bool first_call = true;
    if (first_call) {
        SIT_DEBUG_LOG("[INIT] First call, ensuring context is NULL");
        _sit_current_context = NULL;
        first_call = false;
    }

#if !defined(SITUATION_USE_VULKAN) && !defined(SITUATION_USE_OPENGL)
    SIT_DEBUG_LOG("[FATAL] No graphics backend defined");
    fprintf(stderr, "SituationInit Error: No graphics backend defined (SITUATION_USE_VULKAN/OPENGL).\n");
    return SITUATION_ERROR_INIT_FAILED;
#endif

    SIT_DEBUG_LOG("[STEP 1] Pre-initialization checks");

    // --- 1. PRE-INITIALIZATION CHECKS ---

#ifdef SITUATION_ENABLE_THREADING
    SIT_DEBUG_LOG("[CHECK] Threading enabled, setting main thread ID");
    // Multi-Threading platform initialisation of the main thread (sit_gs context)
    if (!sit_gs_thread_id_set) {
        sit_gs_main_thread_id = thrd_current();
        sit_gs_thread_id_set = true;
        SIT_DEBUG_LOG("[OK] Main thread ID set");
    }
#else
    SIT_DEBUG_LOG("[INFO] Threading disabled");
#endif

    SIT_DEBUG_LOG("[CHECK] Checking if already initialized");

    SIT_DEBUG_LOG("[DEBUG] About to check _sit_current_context");
    // Ensure the library isn't already initialized to prevent conflicts.
    if (_sit_current_context != NULL) {
        SIT_DEBUG_LOG("[INFO] Context exists");
        // If context exists, check if it claims to be initialized
        if (sit_gs.is_initialized) {
            SIT_DEBUG_LOG("[FATAL] Already initialized");
            _SituationSetErrorFromCode(SITUATION_ERROR_ALREADY_INITIALIZED, "SituationInit: Library is already initialized.");
            return SITUATION_ERROR_ALREADY_INITIALIZED;
        }
        SIT_DEBUG_LOG("[INFO] Context not initialized, will reset");
        // If context exists but is_initialized is false, it might be a dirty state or a re-init attempt.
        // We will proceed to re-allocate/reset below.
    } else {
        SIT_DEBUG_LOG("[INFO] Context is NULL");
    }

    SIT_DEBUG_LOG("[CHECK] Validating init_info");

    SIT_DEBUG_LOG("[DEBUG] init_info pointer: %p", (void*)init_info);

    // Ensure the required initialization configuration struct is provided.
    if (!init_info) {
        SIT_DEBUG_LOG("[FATAL] init_info is NULL");
        // Can't set error code if context doesn't exist yet!
        if (_sit_current_context) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationInit: init_info cannot be NULL.");
        }
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SIT_DEBUG_LOG("[OK] init_info is valid");

    /* Name main thread before pthread/thrd bookkeeping or context alloc. */
    _SituationSetCurrentThreadName(
        _SituationResolveMainThreadOsName(init_info->main_thread_name, init_info->window_title));

    SIT_DEBUG_LOG("[STEP 1.5] Allocating context");

    // --- 1.5. CONTEXT ALLOCATION ---
    if (_sit_current_context == NULL) {
        SIT_DEBUG_LOG("[INFO] Allocating new context");
        _sit_current_context = (SituationContext*)SIT_CALLOC(1, sizeof(SituationContext));
        if (!_sit_current_context) {
            SIT_DEBUG_LOG("[FATAL] Context allocation failed");
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        SIT_DEBUG_LOG("[OK] Context allocated");
    } else {
        SIT_DEBUG_LOG("[INFO] Resetting existing context");
        // Zero out the existing context to ensure a clean slate.
        memset(_sit_current_context, 0, sizeof(SituationContext));
    }

    // Now 'sit_gs' and 'sit_audio' macros are valid and point to zeroed memory.

    if (ma_mutex_init(&sit_gs.error_mutex) != MA_SUCCESS) {
        SIT_DEBUG_LOG("[FATAL] error_mutex init failed");
        SIT_FREE(_sit_current_context);
        _sit_current_context = NULL;
        return SITUATION_ERROR_INIT_FAILED;
    }

    /* Name the main thread early so Task Manager shows the resolved label before GLFW/platform work. */
    _SituationCopyThreadName(sit_gs.main_thread_name, sizeof(sit_gs.main_thread_name),
        _SituationResolveMainThreadOsName(init_info->main_thread_name, init_info->window_title));
    _SituationSetCurrentThreadName(sit_gs.main_thread_name);

    // --- 2. INITIALIZE CORE PLATFORM & WINDOW ---
    // These steps are prerequisites for renderer and subsystem initialization.
    // They are common to both OpenGL and Vulkan backends.

    // 2a. Initialize platform-specific components (GLFW, COM on Windows, etc.).
    SIT_DEBUG_LOG("[STEP 2a] Calling _SituationInitPlatform");
    SituationError err = _SituationInitPlatform();
    SIT_DEBUG_LOG("[DEBUG] _SituationInitPlatform returned: %d", err);
    if (err != SITUATION_SUCCESS) {
        SIT_DEBUG_LOG("[FATAL] Platform init failed");
        // Platform initialization failed. No major resources have been allocated yet
        // by the library itself (GLFW might have allocated some internal stuff, but _SituationInitPlatform should clean that up on failure).
        // Therefore, a full cleanup is not strictly necessary here, but calling it for consistency and to ensure any partial platform setup is undone is good.
        // However, the previous version comment said "No need to call FullCleanup...", which is also a valid viewpoint for this very early failure.
        // Let's stick to the original logic for this specific early failure point, but document it clearly.
        // _SituationFullCleanupOnError(); // Not needed for platform-only failure
        return err; // Return the specific error from platform init.
    }

    // 2b. Create the main application window using GLFW.
    SIT_DEBUG_LOG("[STEP 2b] Calling _SituationInitWindow");
    err = _SituationInitWindow(init_info);
    SIT_DEBUG_LOG("[DEBUG] _SituationInitWindow returned: %d", err);
    if (err != SITUATION_SUCCESS) {
        SIT_DEBUG_LOG("[FATAL] Window init failed");
        // Window creation failed. Platform was initialized, so cleanup is needed.
        _SituationFullCleanupOnError(); // Clean up platform (GLFW)
        return err; // Return the specific error from window init.
    }

    // --- 3. INITIALIZE THE CHOSEN RENDERER ---
    // Dispatch to the backend-specific initialization (OpenGL or Vulkan).
    // This is a major step involving context/device creation, swapchains, etc.

    // [CRITICAL] Initialize resource_registry_mutex BEFORE renderer init
    // The renderer initialization (font loading, texture creation) needs this mutex
    SIT_DEBUG_LOG("[STEP 2.5] Initializing resource registry mutex");
    if (mtx_init(&sit_render.resource_registry_mutex, mtx_plain) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize resource registry mutex.");
        _SituationFullCleanupOnError();
        return SITUATION_ERROR_INIT_FAILED;
    }
    sit_render.resource_registry_mutex_initialized = true;

    SIT_DEBUG_LOG("[STEP 3] Calling _SituationInitRenderer");
    err = _SituationInitRenderer(init_info);
    SIT_DEBUG_LOG("[DEBUG] _SituationInitRenderer returned: %d", err);
    if (err != SITUATION_SUCCESS) {
        SIT_DEBUG_LOG("[FATAL] Renderer init failed with error: %d", err);
        char* error_msg = NULL;
        if (SituationGetLastErrorMsg(&error_msg) == SITUATION_SUCCESS && error_msg && error_msg[0]) {
            SIT_DEBUG_LOG("[ERROR_MSG] %s", error_msg);
            fprintf(stderr, "[Situation] Init failed (%d): %s\n", (int)err, error_msg);
            SituationFreeString(error_msg);
        } else {
            fprintf(stderr, "[Situation] Init failed (%d)\n", (int)err);
        }
        // Renderer initialization failed. Platform and Window were initialized.
        _SituationFullCleanupOnError(); // Clean up platform, window, and any partial renderer state
        return err; // Return the specific error from renderer init.
    }

    // [v2.3.24b] Validate Capabilities
    {
        SituationError cap_err = _SituationValidateRenderCaps();
        if (cap_err != SITUATION_SUCCESS) {
            _SituationFullCleanupOnError();
            return cap_err;
        }
    }

    // --- 4. INITIALIZE OTHER LIBRARY SUBSYSTEMS ---
    // Initialize audio, input, timers, filesystem utils, etc.
    // These often depend on the window and renderer being available.
    err = _SituationInitSubsystems(init_info);
    if (err != SITUATION_SUCCESS) {
        // Subsystem initialization failed. Platform, Window, and Renderer were initialized.
        _SituationFullCleanupOnError(); // Clean up everything initialized so far
        return err; // Return the specific error from subsystems init.
    }

    // --- 5. FINAL STATE SETUP ---
    // Perform any final configuration steps that require all subsystems to be ready.
    // This happens only after all preceding steps have succeeded.

    // Cache information about physical displays connected to the system.
    _SituationCachePhysicalDisplays(); // Result checked internally/during queries

    // Determine the initial focus and minimization state of the created window.
    // This sets up the initial window state profiles.
    if (sit_gs.sit_glfw_window) { // Defensive check, should be valid here
        sit_gs.current_window_focus_state = (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_FOCUSED) == GLFW_TRUE);
        sit_gs.was_minimized_last_frame = (glfwGetWindowAttrib(sit_gs.sit_glfw_window, GLFW_ICONIFIED) == GLFW_TRUE);
        // Apply the initial window state profile based on focus.
        SituationApplyCurrentProfileWindowState();
    } // else? This would be an unexpected state if window init succeeded.

    // --- Store Command-Line Arguments ---
    // Save the argc/argv for later access via SituationGetArgument* functions.
    sit_gs.argc = argc;
    sit_gs.argv = argv; // Store the pointer. The application must keep argv alive.

    // --- 5.5 Render Thread Initialization ---
    // [v2.3.21] Initialize render thread if requested. This MUST happen after renderer init.
#if !defined(__STDC_NO_THREADS__)
    {
        SituationError rt_err = _SituationInitRenderThread(init_info);
        if (rt_err != SITUATION_SUCCESS) {
            _SituationFullCleanupOnError();
            return rt_err;
        }
    }

    // [v2.3.40] Render thread initialized, now safe to create resources
    atomic_store(&sit_render.init_state, SITUATION_STATE_READY);
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Situation] Initialization complete - state: READY\n");
#endif
    fflush(stderr);

#endif

    // --- 6. Mark as Successfully Initialized ---
    // All steps completed successfully. Set the global initialized flag.
    if (sit_gs.main_thread_name[0]) {
        _SituationSetCurrentThreadName(sit_gs.main_thread_name);
    }
    atomic_store(&sit_gs.is_initialized, true);

    // Clear any lingering error message and set a success indicator.
    _SituationSetError("SituationInit: No error. Initialization successful.");

    // --- 7. Start Default Audio Device ---
    // Open the system default playback device so tones and sounds work out of the box.
    // Windows: always use SHARED for auto-start — exclusive mode hijacks the default endpoint
    // and mutes other apps (browser, Spotify, system sounds). The test harness runs many
    // SituationInit/SituationShutdown cycles per process; exclusive on session 1 was leaving
    // WASAPI in a bad state after teardown. Call SituationSetAudioDevice() for exclusive/low-latency.
    if (sit_audio.is_miniaudio_context_initialized) {
#if defined(_WIN32)
        SituationError audio_dev_err = _SituationSetAudioDeviceInternal(0, NULL, ma_share_mode_shared);
#else
        SituationError audio_dev_err = SituationSetAudioDevice(0, NULL);
#endif
        if (audio_dev_err != SITUATION_SUCCESS) {
            fprintf(stderr, "[Situation Audio] Warning: Could not start default audio device (error %d). Call SituationSetAudioDevice() manually.\n", audio_dev_err);
        }
    }

    // --- 8. Return Success ---
    return SITUATION_SUCCESS;
}

/**
* @brief [INTERNAL] Initializes platform-specific libraries and components.
*
* This helper function is the very first step in the `SituationInit` process.
* It is responsible for setting up the foundational, low-level libraries that the rest of the library depends on, primarily GLFW for windowing and input.
* It also handles any platform-specific initializations required.
*
* On Windows, this includes initializing the Component Object Model (COM) library, which is necessary for certain APIs (like file system operations
* using `SHGetKnownFolderPath`).
*
* The function also sets up the GLFW error callback (`_SituationGLFWErrorCallback`) to capture and report any errors originating
* from GLFW during the initialization process or later library operations.
*
* @return SITUATION_SUCCESS on successful initialization of all required platform components.
* @return SITUATION_ERROR_COM_FAILED if COM initialization fails on Windows (and the error is not `RPC_E_CHANGED_MODE`, which is tolerated).
* @return SITUATION_ERROR_GLFW_FAILED if `glfwInit` fails to initialize the GLFW library. A specific error message is set by the GLFW error
* callback or this function.
*
* @note This function must be called before any other platform-dependent operations (like window creation or Vulkan instance creation).
* @note If this function fails, it attempts to undo any partial initializations it performed (e.g., calling `CoUninitialize` on Windows if COM was initialized).
* @warning This function is for internal use by `SituationInit` and should not be called directly by user code.
*
* @see SituationInit(), _SituationGLFWErrorCallback(), _SituationCleanupPlatform(), glfwInit(), glfwSetErrorCallback()
*/
static SituationError _SituationInitPlatform(void) {
    // --- 1. Platform-Specific Initializations ---

#if defined(_WIN32)
    {
        // --- 1a. Initialize Console Virtual Terminal Processing (VT) ---
        // Enables ANSI color codes in the Windows console (cmd.exe / PowerShell).
        // This makes logs readable instead of full of junk characters.
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        if (hErr != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hErr, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hErr, dwMode);
            }
        }

        // --- 1b. Initialize COM (Windows only) ---
        // COM is needed for various Windows APIs, particularly for filesystem operations like getting user directories (SHGetKnownFolderPath).
        // We initialize it in Apartment-Threaded mode, which is suitable for most single-threaded applications like this library.
        // COINIT_DISABLE_OLE1DDE disables legacy OLE1 DDE, which is recommended.
        HRESULT com_hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

        if (SUCCEEDED(com_hr)) {
            // COM was successfully initialized for this thread.
            sit_gs.is_com_initialized = true;
        } else if (com_hr == RPC_E_CHANGED_MODE) {
            // COM was already initialized for this thread, but in a different mode (e.g., Multi-Threaded). This is not ideal but often workable.
            // We do not set sit_gs.is_com_initialized to true in this case, as we didn't perform the initialization ourselves.
            // The library will proceed, but the user should be aware of potential threading model conflicts if they also use COM directly.
            // Log a warning in debug builds if a logger is available.
            // fprintf(stderr, "WARNING: COM already initialized in a different mode (RPC_E_CHANGED_MODE).\n");
            // It's generally safer not to call CoUninitialize() if we didn't CoInitializeEx().
            sit_gs.is_com_initialized = false;
        } else {
            // COM initialization failed with an unexpected error code.
            // This is a critical failure for Windows platform setup.
            _SituationSetErrorFromCode(
            SITUATION_ERROR_COM_FAILED,
            "_SituationInitPlatform: CoInitializeEx failed unexpectedly."
            );
            // No cleanup needed for COM itself, as it failed to initialize.
            // Proceed to return the error.
            return SITUATION_ERROR_COM_FAILED;
        }
    }
#endif // defined(_WIN32)

    // --- 2. Initialize GLFW ---
    // Set the error callback *before* calling glfwInit.
    // This ensures that any errors occurring during glfwInit (or later GLFW calls) are captured and reported via our custom callback function.
    glfwSetErrorCallback(_SituationGLFWErrorCallback);

    // [FIX v2.4.x] GLFW persists for the process lifetime. Only init once.
    static bool s_glfw_initialized = false;
    if (!s_glfw_initialized) {
        // Attempt to initialize the GLFW library.
        if (!glfwInit()) {
            // glfwInit failed. This is a critical failure for window and input management.
            // A more detailed error message should have been captured by _SituationGLFWErrorCallback and stored in sit_gs.last_error_msg.
            _SituationSetErrorFromCode(
            SITUATION_ERROR_GLFW_FAILED,
            "_SituationInitPlatform: glfwInit failed. Check previous GLFW error message or system configuration."
            );

            // --- 3. Cleanup Partial Initializations ---
            // Since glfwInit failed, we need to undo any platform-specific initializations that were successfully performed *before* this point.
#if defined(_WIN32)
            // If COM was successfully initialized by us in step 1, uninitialize it.
            if (sit_gs.is_com_initialized) {
                CoUninitialize();
                sit_gs.is_com_initialized = false; // Reset the flag
            }
#endif // defined(_WIN32)
            // Note: There are no other initializations before glfwInit in this function that require cleanup on its failure.

            // --- 4. Return Error ---
            return SITUATION_ERROR_GLFW_FAILED;
        }
        s_glfw_initialized = true;
    }

    // --- 5. Success ---
    // If we reach here, both platform-specific initializations (COM on Windows) and GLFW initialization were successful.
    // The next step in the SituationInit sequence is typically _SituationInitWindow.
    return SITUATION_SUCCESS;
}

/**
* @brief [INTERNAL] Creates the main application window using GLFW.
*
* @details This helper function is responsible for the final step of the platform initialization phase. It configures GLFW window hints based on the selected graphics backend (OpenGL or Vulkan) and the user-provided `SituationInitInfo`, then attempts to create the GLFW window.
*
* @param init_info A pointer to the `SituationInitInfo` struct provided during `SituationInit`. This contains the desired initial window dimensions, title, and state flags.
* This pointer must not be NULL.
*
* @return SITUATION_SUCCESS on successful creation of the GLFW window.
* @return SITUATION_ERROR_INVALID_PARAM if `init_info` is NULL.
* @return SITUATION_ERROR_GLFW_FAILED if `glfwCreateWindow` fails to create the window. This can happen due to invalid dimensions, inability
* to find a suitable framebuffer configuration, or OS-level window creation failures. A specific error message is set.
*
* @note This function must be called after `_SituationInitPlatform` (which initializes GLFW) but before renderer-specific initialization.
* @note The created `GLFWwindow` handle is stored in `sit_gs.sit_glfw_window`.
* @note Window state flags from `init_info` are stored in `sit_gs.active_profile_window_flags` and
* `sit_gs.inactive_profile_window_flags` for later use by the window state management system.
* @warning This function relies on GLFW being successfully initialized.
* It also assumes that the graphics backend has been chosen (via `SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN`) so that the correct window hints can be set.
*
* @see _SituationInitPlatform(), _SituationInitRenderer(), SituationInitInfo, SituationInit()
*/
static SituationError _SituationInitWindow(const SituationInitInfo* init_info) {
    // --- 1. Input Validation ---
    // Check if the required initialization info struct is provided.
    if (!init_info) {
        _SituationSetErrorFromCode(
        SITUATION_ERROR_INVALID_PARAM,
        "_SituationInitWindow: init_info cannot be NULL."
        );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Determine Initial Window Dimensions ---
    // Set default window dimensions if invalid values are provided in init_info.
    // This prevents glfwCreateWindow from failing due to zero or negative sizes.
    sit_gs.main_window_width = (init_info->window_width > 0) ? init_info->window_width : 1280;
    sit_gs.main_window_height = (init_info->window_height > 0) ? init_info->window_height : 720;

#if defined(SITUATION_USE_OPENGL)
    // [HARDENING] Enforce 4.6 Core. Fail if unsupported.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    // --- 3. Configure GLFW Window Hints ---
    // Window hints must be set *before* calling glfwCreateWindow.

#if defined(SITUATION_USE_VULKAN)
    {
        // --- Vulkan-Specific Hints ---
        // For Vulkan, we explicitly tell GLFW *not* to create an OpenGL context.
        // The Vulkan application will create its own VkSurface and handle presentation.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // Note: Other OpenGL-specific hints (version, profile, MSAA) are irrelevant here.
    }
#elif defined(SITUATION_USE_OPENGL)
    {
        // --- OpenGL-Specific Hints ---
        // Request a specific OpenGL version and profile.
        // Ensure this version is supported by the target hardware/drivers.
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        // Enable forward compatibility (important for macOS and core profile).
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

        // Configure Multi-Sample Anti-Aliasing (MSAA) if requested.
        // This hint asks GLFW to create a framebuffer with 4x MSAA support.
        // The actual effectiveness depends on driver/hardware support.
        if (init_info->initial_active_window_flags & SITUATION_FLAG_MSAA_4X_HINT) {
            glfwWindowHint(GLFW_SAMPLES, 4);
        }
        // Note: Other OpenGL context flags (debug, robustness) could be set here if needed.
    }
#endif // SITUATION_USE_OPENGL

    // --- 4. Configure Common Window Hints ---
    // These hints apply regardless of the chosen graphics backend.
    // They control the initial appearance and behavior of the window.

    // New apps should appear above existing windows at startup.
    const uint32_t active_window_flags =
        init_info->initial_active_window_flags | SITUATION_FLAG_WINDOW_TOPMOST;

    // Resizable: Can the user resize the window?
    glfwWindowHint( GLFW_RESIZABLE, (active_window_flags & SITUATION_FLAG_WINDOW_RESIZABLE) ? GLFW_TRUE : GLFW_FALSE );

    // Decorated: Does the window have a title bar and borders?
    glfwWindowHint( GLFW_DECORATED, (active_window_flags & SITUATION_FLAG_WINDOW_UNDECORATED) ? GLFW_FALSE : GLFW_TRUE );

    // Floating/Topmost: Should the window stay on top of others?
    glfwWindowHint( GLFW_FLOATING, (active_window_flags & SITUATION_FLAG_WINDOW_TOPMOST) ? GLFW_TRUE : GLFW_FALSE );

#if defined(SITUATION_USE_OPENGL)
    // Compositor overlays should treat Situation windows as fully opaque unless an app
    // explicitly adds its own transparency path.
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
    glfwWindowHint(GLFW_ALPHA_BITS, 0);
#endif

    // Visible: honor SITUATION_FLAG_WINDOW_HIDDEN and reset any stale GLFW hint
    // (OpenGL loader window sets GLFW_VISIBLE FALSE; hints persist across Init/Shutdown cycles).
    glfwWindowHint(GLFW_VISIBLE,
        (active_window_flags & SITUATION_FLAG_WINDOW_HIDDEN) ? GLFW_FALSE : GLFW_TRUE);

    // Focused: Should the window be given input focus? (Default is GLFW_TRUE)
    // glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE); // Example of not focusing initially

    // --- 5. Create the GLFW Window ---
    // This is the actual call to GLFW to create the native window and, potentially, the OpenGL context (if not using GLFW_NO_API).
    sit_gs.sit_glfw_window = glfwCreateWindow(
    sit_gs.main_window_width, // Initial width
    sit_gs.main_window_height, // Initial height
    init_info->window_title, // Window title (can be NULL)
    NULL, // Monitor (NULL for windowed mode)
    NULL // Share (NULL for no context sharing)
    );

    // --- 6. Handle Window Creation Result ---
    if (!sit_gs.sit_glfw_window) {
        // glfwCreateWindow failed. This is a critical error.
        // Common reasons include invalid dimensions, unsupported pixel formats,
        // or OS-level failures (e.g., too many windows, permission denied).
        // GLFW's error callback (_SituationGLFWErrorCallback) should have captured a more detailed error message from GLFW itself.
        _SituationSetErrorFromCode(
        SITUATION_ERROR_WINDOW_CREATION_FAILED,
        "_SituationInitWindow: glfwCreateWindow failed. Check previous GLFW error message or window parameters (size, title)."
        );
        // Ensure the global window handle is explicitly NULL on failure.
        sit_gs.sit_glfw_window = NULL;
        return SITUATION_ERROR_WINDOW_CREATION_FAILED;
    }

    // --- 7. Store Window State Profiles ---
    // Save the initial window state flags provided in init_info.
    // These are used later by the window state management functions (e.g., SituationApplyCurrentProfileWindowState) to define the behavior when the window is active or inactive.
    sit_gs.active_profile_window_flags = active_window_flags;
    sit_gs.inactive_profile_window_flags = init_info->initial_inactive_window_flags;
    // Note: The actual GLFW window state (resizable, decorated, etc.) is set by the hints above and the creation process. These flags are for the library's higher-level state management system.

    // --- 8. Success ---
    // If we reach here, the GLFW window was created successfully.
    // The handle is stored in sit_gs.sit_glfw_window.
    // Framebuffer pixels can differ from the requested client size (Hi-DPI). Keep
    // sit_gs.main_window_* aligned with glfwGetFramebufferSize so OpenGL viewports,
    // SituationGetRenderWidth/Height, and user shaders using the same dimensions stay in sync.
    {
        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fbw, &fbh);
        if (fbw > 0 && fbh > 0) {
            sit_gs.main_window_width = fbw;
            sit_gs.main_window_height = fbh;
            sit_gs.render_canvas_width = fbw;
            sit_gs.render_canvas_height = fbh;
        }
    }
    _SituationCopyThreadName(sit_gs.main_thread_name, sizeof(sit_gs.main_thread_name),
        _SituationResolveMainThreadOsName(init_info->main_thread_name, init_info->window_title));
    _SituationSetCurrentThreadName(sit_gs.main_thread_name);
#ifdef SITUATION_ENABLE_THREADING
    (void)_SituationSetThreadAffinityForRole(SIT_THREAD_ROLE_MAIN, 0);
#endif
    // The next step in initialization will typically be renderer setup (_SituationInitRenderer) followed by subsystem initialization (_SituationInitSubsystems).
    return SITUATION_SUCCESS;
}

/**
* @brief [INTERNAL] Orchestrates the sequential initialization of all major subsystems.
*
* @details This is the **master initialization function** called exactly once during library startup
* (typically from `SituationInit`). It coordinates the creation and setup of every major
* subsystem in a carefully ordered sequence to ensure proper dependency satisfaction.
*
* Initialization order (critical '' -- changes can cause subtle failures):
* 1. Platform / OS-specific early setup (CoInitialize on Win32, thread locals, etc.)
* 2. GLFW initialization and error callback registration
* 3. Window creation (if not provided externally) with requested hints
* 4. **Backend-specific context init**:
* - OpenGL: `_SituationInitOpenGL` (GLAD load, version check, state defaults)
* - Vulkan: `_SituationInitVulkan` (instance, device, queues, swapchain, etc.)
* 5. Render thread startup (if `SITUATION_ENABLE_RENDER_THREAD`)
* - Context handoff (GL) or queue family setup (Vulkan)
* - Thread creation + launch `_SituationRenderThreadEntry`
* 6. Internal resource creation:
* - Default white 1'''' -- 1 texture
* - Dummy VAO (GL core profile)
* - Built-in quad shader/pipeline
* - Text renderer (`_SituationInitTextRenderer`)
* - Bindless texture support (`_SituationVirtualBindlessInit`)
* 7. Audio subsystem init (miniaudio device, mixer setup)
* 8. Filesystem / hot-reload watchers (if enabled)
* 9. Thread pool creation (if `SITUATION_ENABLE_THREADING`)
* 10. Final validation (check for GL/VK errors, log capabilities)
*
* Each step is guarded:
* - If a subsystem fails critically, init aborts early and returns the error
* - Non-critical failures (e.g. missing extension) log warnings and continue in degraded mode
* - All errors are propagated via return value and internal `_SituationSetErrorFromCode`
*
* @param init_info Pointer to `SituationInitInfo` containing user preferences:
* - window hints (size, title, resizable, etc.)
* - backend selection override (if any)
* - feature toggles (render thread, threading, hot-reload rate, etc.)
* - May be NULL for defaults
*
* @return SITUATION_SUCCESS if **all** subsystems initialized successfully,
* SITUATION_ERROR_INIT_FAILED on early critical failure (e.g. GLFW init fail, context creation fail),
* SITUATION_ERROR_GLAD_LOAD_FAILED / SITUATION_ERROR_VULKAN_INSTANCE_FAILED / etc. for backend-specific issues,
* SITUATION_ERROR_THREAD_CREATION_FAILED if render thread failed to start,
* SITUATION_ERROR_MEMORY_ALLOCATION for internal resource failures,
* or other propagated subsystem errors.
*
* @note This function is **called only once** and is **not reentrant**.
* Thread safety: Must be called from the main thread before any rendering or multi-threaded use.
* Assumes GLFW has not been initialized elsewhere (library owns GLFW lifecycle).
* On failure, partial subsystems may remain initialized '' -- caller should call `SituationShutdown`
* to clean up safely.
*
* Critical invariants:
* - OpenGL/Vulkan context must be created **before** render thread starts
* - Render thread must be running **before** any command recording or submission
* - All GL/VK calls in this function assume context is current
*
* @see SituationInit (public entry point), SituationShutdown,
* _SituationInitOpenGL, _SituationInitVulkan,
* _SituationRenderThreadEntry, _SituationInitTextRenderer,
* _SituationVirtualBindlessInit, SITUATION_ERROR_INIT_FAILED
*/
static SituationError _SituationInitSubsystems(const SituationInitInfo* init_info) {
    // --- 1. Audio System Initialization ---
    ma_context_config ctx_config = ma_context_config_init();
    // Try DirectSound first to avoid WASAPI driver crashes in virtualized environments; fallback to WASAPI if needed
    ma_backend backends[] = { ma_backend_dsound, ma_backend_wasapi };
    if (ma_context_init(backends, 2, &ctx_config, &sit_audio.miniaudio_context) != MA_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_CONTEXT, "ma_context_init failed");
        return SITUATION_ERROR_AUDIO_CONTEXT;
    }
    sit_audio.is_miniaudio_context_initialized = true;

    // Initialize the handle pool
    {
        SituationError pool_err = _SitAudioInitPool();
        if (pool_err != SITUATION_SUCCESS) return pool_err;
    }

    // Initialize the tone pool [Resonance]
    memset(sit_audio.tone_pool, 0, sizeof(sit_audio.tone_pool));

    // Initialize the mutex that protects the sound playback queue.
    /*if (ma_mutex_init(&sit_audio.audio_queue_mutex) != MA_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED, "Failed to initialize audio queue mutex");
        return SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED;
    }*/
    // [FIX v2.3.27B] Initialize recursive mutex
    // This allows the same thread (Audio Thread) to re-acquire the lock if a
    // user processor calls a Situation API function.
    if (mtx_init(&sit_audio.audio_queue_mutex, mtx_recursive) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED, "Failed to initialize recursive audio mutex");
        return SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED;
    }

    // Initialize capture queue if requested
    if (init_info->flags & SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD) {
        sit_audio.audio_capture_on_main_thread = true;
        sit_audio.audio_capture_queue_capacity = 4096 * 4; // Reasonable default
        sit_audio.audio_capture_queue = (float*)SIT_CALLOC(sit_audio.audio_capture_queue_capacity, sizeof(float));
        if (!sit_audio.audio_capture_queue) {
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Audio capture ring buffer");
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        if (ma_mutex_init(&sit_audio.audio_capture_mutex) != MA_SUCCESS) {
            SIT_FREE(sit_audio.audio_capture_queue);
            _SituationSetErrorFromCode(SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED, "Audio capture mutex");
            return SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED;
        }
    }

    // Allocate temporary buffers (zero-initialized to prevent clicks and pops in the left ear from garbage memory)
    size_t decoder_buf_size = SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES * 8 * sizeof(float);
    size_t effects_buf_size = SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES * 8 * sizeof(float);
    size_t converter_buf_size = SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES * MA_MAX_CHANNELS * sizeof(float);
    sit_audio.audio_callback_decoder_temp_buffer = (float*)SIT_CALLOC(1, decoder_buf_size);
    sit_audio.audio_callback_effects_temp_buffer = (float*)SIT_CALLOC(1, effects_buf_size);
    sit_audio.audio_callback_converter_temp_buffer = (float*)SIT_CALLOC(1, converter_buf_size);

    sit_audio.audio_capture_temp_buffer = NULL;
    sit_audio.audio_capture_temp_buffer_capacity = 0;
    if (!sit_audio.audio_callback_decoder_temp_buffer || !sit_audio.audio_callback_effects_temp_buffer || !sit_audio.audio_callback_converter_temp_buffer) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Audio callback temp buffers");
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    sit_audio.audio_callback_temp_buffer_frames_capacity = SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES;

    atomic_init(&sit_audio.is_processing_snapshot, false);
    atomic_init(&sit_audio.is_in_audio_callback, false);
    atomic_init(&sit_audio.audio_meter_peak, 0.f);
    atomic_init(&sit_audio.audio_meter_rms, 0.f);

    // [v2.4] Audio Queue Initialization
    sit_audio.config_max_voices = init_info->max_audio_voices;
    // Start with 32 voices by default, or the limit if it's smaller than 32
    int initial_cap = 32;
    if (sit_audio.config_max_voices > 0 && sit_audio.config_max_voices < 32) {
        initial_cap = (int)sit_audio.config_max_voices;
    }

    sit_audio.active_voices = (_SituationSound**)SIT_CALLOC(initial_cap, sizeof(_SituationSound*));
    if (!sit_audio.active_voices) { _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate active_voices array"); return SITUATION_ERROR_MEMORY_ALLOCATION; }
    sit_audio.active_voice_capacity = initial_cap;
    sit_audio.active_voice_count = 0;

    // Snapshot buffer (initialized to same capacity)
    sit_audio.snapshot_buffer = (_SituationSound**)SIT_CALLOC(initial_cap, sizeof(_SituationSound*));
    if (!sit_audio.snapshot_buffer) {
        SIT_FREE(sit_audio.active_voices);
        sit_audio.active_voices = NULL;
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate snapshot_buffer array");
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    sit_audio.snapshot_buffer_capacity = initial_cap;

    // --- 2. Timer System Initialization ---
    SituationTimerSystem* ts = &sit_gs.timer_system_instance;
    ts->current_system_time_seconds = glfwGetTime();

    double range_high = SITUATION_TIMER_GRID_PERIOD_EDGES;
    double range_low = 1.0 / SITUATION_TIMER_GRID_PERIOD_EDGES;
    double current_period_val = range_low;
    double increment_ratio = range_low;

    for (int i = 0; i < SITUATION_MAX_OSCILLATORS; i++) {
        if (i < 64) {
            ts->period_seconds[i] = current_period_val;
            if (ts->period_seconds[i] > range_high) ts->period_seconds[i] = range_high;
            if (ts->period_seconds[i] < range_low && i > 0) ts->period_seconds[i] = range_low > 0.000001 ? range_low : 0.000001;
            current_period_val += (range_low * increment_ratio);
            increment_ratio *= SITUATION_TIMER_GRIDILON;
        } else {
            ts->period_seconds[i] = 1.0;
        }
        ts->next_trigger_time_seconds[i] = ts->current_system_time_seconds + ts->period_seconds[i];
        ts->last_ping_time_seconds[i] = ts->current_system_time_seconds;
        ts->trigger_count[i] = 0;
    }
    memset(ts->state_current, 0, sizeof(ts->state_current));
    memset(ts->state_previous, 0, sizeof(ts->state_previous));
    ts->is_initialized = true;


    // --- 3. Input Systems Initialization ---

    // STEP 1: Zero out the memory structures FIRST
    memset(&sit_input.keyboard, 0, sizeof(sit_input.keyboard));
    memset(&sit_input.mouse, 0, sizeof(sit_input.mouse));
    memset(&sit_input.joysticks, 0, sizeof(sit_input.joysticks));

    // STEP 2: Initialize mutexes AFTER memset
    // [FIX v2.4.15] Initialize one at a time with rollback on failure
    if (ma_mutex_init(&sit_input.keyboard.event_queue_mutex) != MA_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize keyboard event queue mutex");
        return SITUATION_ERROR_INIT_FAILED;
    }
    if (ma_mutex_init(&sit_input.joysticks.event_queue_mutex) != MA_SUCCESS) {
        ma_mutex_uninit(&sit_input.keyboard.event_queue_mutex);
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize joystick event queue mutex");
        return SITUATION_ERROR_INIT_FAILED;
    }
    if (ma_mutex_init(&sit_input.mouse.mutex) != MA_SUCCESS) {
        ma_mutex_uninit(&sit_input.keyboard.event_queue_mutex);
        ma_mutex_uninit(&sit_input.joysticks.event_queue_mutex);
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize mouse mutex");
        return SITUATION_ERROR_INIT_FAILED;
    }
    sit_gs.input_mutexes_initialized = true;

    // Register callbacks
    glfwSetDropCallback(sit_gs.sit_glfw_window, _SituationGLFWFileDropCallback);
    glfwSetWindowFocusCallback(sit_gs.sit_glfw_window, _SituationGLFWWindowFocusCallback);
    glfwSetWindowMaximizeCallback(sit_gs.sit_glfw_window, _SituationGLFWWindowMaximizeCallback);
    glfwSetWindowIconifyCallback(sit_gs.sit_glfw_window, _SituationGLFWWindowIconifyCallback);
    glfwSetFramebufferSizeCallback(sit_gs.sit_glfw_window, _SituationGLFWFramebufferSizeCallback);
    glfwSetKeyCallback(sit_gs.sit_glfw_window, _SituationGLFWKeyCallback);
    glfwSetCharCallback(sit_gs.sit_glfw_window, _SituationGLFWCharCallback);
    glfwSetMouseButtonCallback(sit_gs.sit_glfw_window, _SituationGLFWMouseButtonCallback);
    glfwSetCursorPosCallback(sit_gs.sit_glfw_window, _SituationGLFWCursorPosCallback);
    glfwSetScrollCallback(sit_gs.sit_glfw_window, _SituationGLFWScrollCallback);
    glfwSetJoystickCallback(_SituationGLFWJoystickCallback);

    // Initialize remaining state (that isn't 0)
    glm_vec2_one(sit_input.mouse.scale); // Default mouse scale is (1, 1).

    double initial_mx, initial_my;
    glfwGetCursorPos(sit_gs.sit_glfw_window, &initial_mx, &initial_my);
    sit_input.mouse.current_pos[0] = (float)initial_mx;
    sit_input.mouse.current_pos[1] = (float)initial_my;
    glm_vec2_copy(sit_input.mouse.current_pos, sit_input.mouse.last_pos);

    // Pre-create cursors
    sit_input.cursors[SIT_CURSOR_DEFAULT] = NULL;
    sit_input.cursors[SIT_CURSOR_ARROW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    sit_input.cursors[SIT_CURSOR_IBEAM] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    sit_input.cursors[SIT_CURSOR_CROSSHAIR] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    sit_input.cursors[SIT_CURSOR_HAND] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    sit_input.cursors[SIT_CURSOR_HRESIZE] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    sit_input.cursors[SIT_CURSOR_VRESIZE] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    sit_input.cursor_count = 7;
    // Check if any cursor creation failed
    for (int ci = SIT_CURSOR_ARROW; ci < sit_input.cursor_count; ci++) {
        if (!sit_input.cursors[ci]) {
            _SituationSetErrorFromCode(SITUATION_ERROR_CURSOR_CREATION_FAILED, "One or more standard cursors failed to create");
            break;
        }
    }

    // Poll for existing joysticks
    for (int jid = 0; jid < SITUATION_MAX_JOYSTICKS; jid++) {
        if (glfwJoystickPresent(jid)) {
            _SituationGLFWJoystickCallback(jid, GLFW_CONNECTED);
        }
    }

    // Framebuffer size callback
    int fb_w, fb_h;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fb_w, &fb_h);
    _SituationGLFWFramebufferSizeCallback(sit_gs.sit_glfw_window, fb_w, fb_h);

    // --- Initialize Frame Timing State ---
    sit_gs.previous_time = glfwGetTime();
    sit_gs.current_time = sit_gs.previous_time;
    sit_gs.frame_time = 0.0;
    sit_gs.target_frame_time = 0.0;
    sit_gs.fps_frame_counter = 0;
    sit_gs.fps_last_update_time = sit_gs.previous_time;
    sit_gs.current_fps = 0;

    // --- Initialize Callback Pointers ---
    sit_gs.exit_callback = NULL;
    sit_gs.exit_callback_user_data = NULL;
    sit_gs.resize_callback = NULL;
    sit_gs.resize_callback_user_data = NULL;

    sit_gs.thread_affinity_main = init_info->thread_affinity_main;
    sit_gs.thread_affinity_render = init_info->thread_affinity_render;
    sit_gs.thread_affinity_audio = init_info->thread_affinity_audio;
    sit_gs.numa_prefer_local = init_info->numa_prefer_local;
    sit_gs.worker_numa_spread = init_info->worker_numa_spread || SITUATION_WORKER_NUMA_SPREAD_DEFAULT;
    sit_gs.io_thread_numa_node = init_info->io_thread_numa_node;
    sit_gs.thread_pool_use_physical_cores = init_info->thread_pool_use_physical_cores;
    sit_gs.thread_pool_reserved_threads = init_info->thread_pool_reserved_threads;
    SituationRefreshCpuTopology();
    SituationRefreshNumaTopology();

#if defined(SITUATION_ENABLE_THREADING)
    {
        SituationError pool_err = SituationCreateThreadPool(&sit_gs.thread_pool, 0, init_info->io_queue_capacity, init_info->hot_reload_poll_rate, init_info->disable_io_thread);
        if (pool_err != SITUATION_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CREATION_FAILED, "Failed to create thread pool");
            return SITUATION_ERROR_THREAD_CREATION_FAILED;
        }
    }
#endif

    return SITUATION_SUCCESS;
}

/**
* @brief Polls for all pending window and input events.
* @details Must be called once per frame.
*
* @par Audio Capture Integration
* If `SituationStartAudioCapture` has been called with the `SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD` flag, this function also acts as the dispatcher.
* It safely locks the audio capture ring buffer, linearizes the available audio frames into a temporary contiguous array, and invokes the user's capture callback on the main thread.
*
* @see SituationUpdateTimers()
*/
SITAPI void SituationPollInputEvents(void) {
    SIT_DEBUG_LOG("[PollInputEvents] START");
    if (!SituationIsInitialized()) {
        SIT_DEBUG_LOG("[PollInputEvents] Not initialized, returning");
        return;
    }

    // Detect UPDATE_AFTER_DRAW violation: poll called while a frame is still open
    if (sit_render.in_frame) {
        _SituationSetErrorFromCode(SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION,
            "SituationPollInputEvents called while a frame is active (missing SituationEndFrame before next poll)");
#ifndef NDEBUG
        fprintf(stderr, "[Situation] UPDATE_AFTER_DRAW_VIOLATION: SituationPollInputEvents called with in_frame=true\n");
#endif
    }

    // [NEW] Reset Profiler Counters
    sit_render.frame_draw_calls = 0;
    sit_render.frame_triangle_count = 0;

    // Reset consistency check flag
    sit_render.debug_draw_command_issued_this_frame = false;

    // Process Main Thread Audio Capture
    if (sit_audio.audio_capture_on_main_thread && sit_audio.capture_callback) {
        ma_mutex_lock(&sit_audio.audio_capture_mutex);

        size_t write_head = sit_audio.audio_capture_write_head;
        size_t read_head = sit_audio.audio_capture_read_head;
        size_t capacity = sit_audio.audio_capture_queue_capacity;

        // Calculate frames available to read
        size_t samples_available = 0;
        if (write_head >= read_head) {
            samples_available = write_head - read_head;
        } else {
            // Wrapped around
            samples_available = (capacity - read_head) + write_head;
        }

        // Only process if we have data
        uint32_t channels = sit_audio.capture_device.capture.channels ? sit_audio.capture_device.capture.channels : 1;
        size_t frames_available = samples_available / channels;
        size_t samples_to_read = frames_available * channels;

        if (frames_available > 0) {
            // 1. Ensure temporary scratch buffer is large enough
            if (samples_to_read > sit_audio.audio_capture_temp_buffer_capacity) {
                size_t new_capacity = samples_to_read * 2; // Grow with some headroom
                void* new_buf = SIT_REALLOC(sit_audio.audio_capture_temp_buffer, new_capacity * sizeof(float));
                if (new_buf) {
                    sit_audio.audio_capture_temp_buffer = (float*)new_buf;
                    sit_audio.audio_capture_temp_buffer_capacity = new_capacity;
                }
            }

            float* temp_buffer = sit_audio.audio_capture_temp_buffer;

            if (temp_buffer && sit_audio.audio_capture_temp_buffer_capacity >= samples_to_read) {
                // 2. Copy and Linearize Data
                if (write_head >= read_head) {
                    // Contiguous block
                    memcpy(temp_buffer, &sit_audio.audio_capture_queue[read_head], samples_to_read * sizeof(float));
                } else {
                    // Split block (Wrapped)
                    size_t end_chunk_size = capacity - read_head;
                    // Part 1: Read to end of buffer
                    memcpy(temp_buffer, &sit_audio.audio_capture_queue[read_head], end_chunk_size * sizeof(float));
                    // Part 2: Start from beginning to write head
                    memcpy(temp_buffer + end_chunk_size, &sit_audio.audio_capture_queue[0], (samples_to_read - end_chunk_size) * sizeof(float));
                }

                // 3. Advance Read Head
                sit_audio.audio_capture_read_head = (read_head + samples_to_read) % capacity;

                // 4. Unlock BEFORE callback to prevent deadlocks if user callback takes time
                ma_mutex_unlock(&sit_audio.audio_capture_mutex);

                // 5. Dispatch to User
                sit_audio.capture_callback(temp_buffer, (uint32_t)frames_available, sit_audio.capture_user_data);
            } else {
                // Realloc failed or buffer too small, just unlock. We'll try again next frame.
                // Data remains in buffer (potentially overflowing eventually, but safe crash-wise).
                ma_mutex_unlock(&sit_audio.audio_capture_mutex);
            }
        } else {
            // No data, just unlock
            ma_mutex_unlock(&sit_audio.audio_capture_mutex);
        }
    }

    // --- RESET PER-FRAME EVENT FLAGS AND BUFFERS ---
    sit_gs.was_window_resized_last_frame = false;
    sit_gs.file_was_dropped_this_frame = false;

    // Reset keyboard event state.
    // Copy the now-old state to the "last" buffer for comparison.
    memcpy(sit_input.keyboard.last_state, sit_input.keyboard.current_state, sizeof(sit_input.keyboard.last_state));
    // Clear the single-frame press/release event trackers.
    memset(sit_input.keyboard.down_this_frame, 0, sizeof(sit_input.keyboard.down_this_frame));
    memset(sit_input.keyboard.up_this_frame, 0, sizeof(sit_input.keyboard.up_this_frame));

    // Reset mouse event state.
    glm_vec2_copy(sit_input.mouse.current_pos, sit_input.mouse.last_pos);
    memcpy(sit_input.mouse.last_button_state, sit_input.mouse.current_button_state, sizeof(sit_input.mouse.last_button_state));
    memset(sit_input.mouse.button_down_this_frame, 0, sizeof(sit_input.mouse.button_down_this_frame));
    memset(sit_input.mouse.button_up_this_frame, 0, sizeof(sit_input.mouse.button_up_this_frame));
    sit_input.mouse.wheel_move_x = 0.0f;
    sit_input.mouse.wheel_move_y = 0.0f;

    // --- [POLL] GATHER NEW EVENTS FROM THE OPERATING SYSTEM ---
    // This call triggers all the GLFW callbacks (_SituationGLFWKeyCallback, etc.), which will populate our `current_state` and event queue buffers for this frame.
    glfwPollEvents();

    // Refresh the cached window state flags now that GLFW has processed all events.
    // SituationGetCurrentActualWindowStateFlags() queries multiple GLFW attributes and
    // is called by user code every frame (e.g. for VSync HUD display). Caching here
    // means callers get an O(1) read instead of N GLFW attribute queries per call.
    sit_gs.cached_window_state_flags = SituationGetCurrentActualWindowStateFlags();

    // ========================================================================
    // [FIX v2.3.27B] MOVED JOYSTICK LOGIC HERE FOR ATOMIC INPUT UPDATES
    // ========================================================================

    // --- Process Joystick Connection Events (Thread-Safe) ---
    ma_mutex_lock(&sit_input.joysticks.event_queue_mutex);
    for (int i = 0; i < sit_input.joysticks.event_queue_count; i++) {
        _SituationJoystickEvent ev = sit_input.joysticks.event_queue[i];

        if (ev.event == GLFW_CONNECTED) {
            sit_input.joysticks.state[ev.jid].is_present = true;
            sit_input.joysticks.state[ev.jid].is_gamepad = glfwJoystickIsGamepad(ev.jid);
            int axis_count = 0;
            glfwGetJoystickAxes(ev.jid, &axis_count);
            sit_input.joysticks.state[ev.jid].axis_count = axis_count;
            const char* name = glfwGetJoystickName(ev.jid);
            if (name) {
                strncpy(sit_input.joysticks.state[ev.jid].name, name, SITUATION_MAX_DEVICE_NAME_LEN - 1);
            } else {
                snprintf(sit_input.joysticks.state[ev.jid].name, SITUATION_MAX_DEVICE_NAME_LEN, "Joystick %d", ev.jid);
            }
        } else if (ev.event == GLFW_DISCONNECTED) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INPUT_DEVICE_DISCONNECTED,
                sit_input.joysticks.state[ev.jid].name[0] ? sit_input.joysticks.state[ev.jid].name : "Input device disconnected");
            memset(&sit_input.joysticks.state[ev.jid], 0, sizeof(_SituationJoystickState));
        }

        if (sit_input.joysticks.callback) {
            sit_input.joysticks.callback(ev.jid, ev.event, sit_input.joysticks.callback_user_data);
        }
    }
    sit_input.joysticks.event_queue_count = 0;
    ma_mutex_unlock(&sit_input.joysticks.event_queue_mutex);

    // --- Poll Gamepad State & Detect Press Events ---
    for (int jid = 0; jid < SITUATION_MAX_JOYSTICKS; jid++) {
        if (sit_input.joysticks.state[jid].is_present && sit_input.joysticks.state[jid].is_gamepad) {
            // Copy current state to last state BEFORE polling new state.
            memcpy(sit_input.joysticks.state[jid].last_button_state, sit_input.joysticks.state[jid].current_button_state, sizeof(sit_input.joysticks.state[jid].current_button_state));

            GLFWgamepadstate glfw_state;
            if (glfwGetGamepadState(jid, &glfw_state)) {
                // Update the current state buffers.
                memcpy(sit_input.joysticks.state[jid].current_button_state, glfw_state.buttons, sizeof(glfw_state.buttons));
                memcpy(sit_input.joysticks.state[jid].axis_state, glfw_state.axes, sizeof(glfw_state.axes));

                // Compare current vs. last to detect press events.
                for (int button = 0; button < SITUATION_MAX_JOYSTICK_BUTTONS; ++button) {
                    bool was_down = (sit_input.joysticks.state[jid].last_button_state[button] == GLFW_PRESS);
                    bool is_down = (sit_input.joysticks.state[jid].current_button_state[button] == GLFW_PRESS);

                    if (is_down && !was_down) {
                        // Joystick Ring Buffer Push
                        uint32_t next_head = (sit_input.joysticks.button_head + 1) % SITUATION_KEY_QUEUE_MAX;
                        if (next_head != sit_input.joysticks.button_tail) {
                            sit_input.joysticks.button_pressed_queue[sit_input.joysticks.button_head] = button;
                            sit_input.joysticks.button_head = next_head;
                        }
                    }
                }
            }
        }
    }
}

/**
* @brief Updates all internal timers and calculates the delta time for the current frame.
* @details This is the second of the two core functions that form the new main loop. Its sole responsibility is to advance the library's internal clocks.
* This function should be called **once per frame**, immediately after `SituationPollInputEvents()` but before your main application logic.
*
* @par Function Workflow
* 1. **Calculates Delta Time:** It measures the time elapsed since the last frame and updates the value retrieved by `SituationGetFrameTime()`.
* 2. **Updates Temporal Oscillators:** It advances the state of the Temporal Oscillator system, triggering any oscillators whose periods have elapsed.
* 3. **Updates Joystick/Gamepad State:** It processes the joystick connection event queue and polls the state of connected gamepads to detect button press/release events for the current frame.
* 4. **Updates Virtual Display Clocks:** It advances the internal `elapsed_time_seconds` for each active virtual display.
*
* @note Calling this function is essential for `SituationGetFrameTime()` to return a correct, updated value for the current frame.
*
* @see SituationPollInputEvents(), SituationGetFrameTime(), SituationUpdate()
*/
SITAPI void SituationUpdateTimers(void) {
    if (!SituationIsInitialized()) return;

    // Detect UPDATE_AFTER_DRAW violation: timers updated while a frame is still open
    if (sit_render.in_frame) {
        _SituationSetErrorFromCode(SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION,
            "SituationUpdateTimers called while a frame is active (missing SituationEndFrame before next update)");
#ifndef NDEBUG
        fprintf(stderr, "[Situation] UPDATE_AFTER_DRAW_VIOLATION: SituationUpdateTimers called with in_frame=true\n");
#endif
    }

    // --- 1. Global Frame Time Calculation ---
    sit_gs.current_time = glfwGetTime();
    sit_gs.frame_time = sit_gs.current_time - sit_gs.previous_time;
    sit_gs.previous_time = sit_gs.current_time;

    // --- 2. Update Temporal Oscillator System ---
    if (sit_gs.timer_system_instance.is_initialized) {
        SituationTimerSystem* ts = &sit_gs.timer_system_instance;
        memcpy(ts->state_previous, ts->state_current, sizeof(ts->state_current));
        ts->current_system_time_seconds = sit_gs.current_time;
        for (int i = 0; i < SITUATION_MAX_OSCILLATORS; i++) {
            if (ts->current_system_time_seconds >= ts->next_trigger_time_seconds[i] && ts->period_seconds[i] > 0.0) {
                int bank = i / 64;
                int bit_pos = i % 64;
                uint64_t mask = (uint64_t)1 << bit_pos;
                while (ts->current_system_time_seconds >= ts->next_trigger_time_seconds[i]) {
                    ts->state_current[bank] ^= mask;
                    ts->trigger_count[i]++;
                    // Fix: Calculate next trigger from anchor + count*period to prevent float drift
                    ts->next_trigger_time_seconds[i] = ts->anchor_time_seconds[i] + ts->trigger_count[i] * ts->period_seconds[i];
                }
            }
        }
    }

    // --- 3. Update Virtual Display Timers ---
    double current_time_for_vdisplays = sit_gs.timer_system_instance.is_initialized ? sit_gs.timer_system_instance.current_system_time_seconds : sit_gs.current_time;
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_DISPLAYS; ++i) {
        if (sit_render.virtual_display_slots_used[i]) {
            SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[i];
            vd->frame_delta_time_seconds = (current_time_for_vdisplays - vd->last_update_time_seconds);
            vd->elapsed_time_seconds += vd->frame_delta_time_seconds * vd->frame_time_multiplier;
            vd->frame_count++;
            vd->last_update_time_seconds = current_time_for_vdisplays;
            vd->cycle_animation_value = sinf(cosf(sinf((float)vd->elapsed_time_seconds) * sinf((float)vd->elapsed_time_seconds * 0.1f) * 0.1f) * cosf((float)vd->elapsed_time_seconds * 0.015f) * 0.1f) * 0.05f + 0.001f;
        }
    }
}

/**
* @brief [DEPRECATED] Polls for input events and updates all internal timers.
* @details This function is deprecated and will be removed in a future version.
* It combines event polling and timer updates, which is less explicit and can lead to off-by-one-frame bugs.
* Please update your main loop to use `SituationPollInputEvents()` and `SituationUpdateTimers()` separately for a clearer and more robust structure.
*
* @par New Workflow (Correct):
* The recommended main loop structure is now:
* ```c
* while (!SituationWindowShouldClose()) {
    * // 1. GATHER INPUT: Poll all OS events.
    * SituationPollInputEvents();
    *
    * // 2. UPDATE STATE: Update all internal timers and calculate delta time.
    * SituationUpdateTimers();
    * float delta_time = SituationGetFrameTime();
    *
    * // 3. YOUR LOGIC: Use fresh input and delta time to update your application.
    * UpdatePlayer(delta_time);
    *
    * // 4. RENDER: Draw the new state of your application.
    * if (SituationAcquireFrameCommandBuffer()) {
        * SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
        * // ... record your drawing commands ...
        * SituationEndFrame();
        * }
    * }
* ```
* @deprecated Use `SituationPollInputEvents()` followed by `SituationUpdateTimers()`.
* @see SituationPollInputEvents(), SituationUpdateTimers()
*/
SITAPI void SituationUpdate(void) {
    SituationPollInputEvents();
    SituationUpdateTimers();
}

/**
* @brief Shuts down the entire Situation library and releases all resources.
* @details This is the main exit point for the library and must be the last `SITAPI` function called. It orchestrates a graceful shutdown of all subsystems in the precise reverse order of their initialization, ensuring a clean exit with no resource leaks.
*
* @par Shutdown Sequence
* 1. **User Callback:** Invokes the optional exit callback set by `SituationSetExitCallback`.
* 2. **GPU Synchronization:** Ensures all pending GPU commands are completed (`vkDeviceWaitIdle` or `glFinish`) to prevent destroying resources that are still in use.
* 3. **Internal Resource Cleanup:** Frees library-owned default resources before leak detection.
* 4. **Dangling Resource Cleanup:** Calls `_SituationCleanupDanglingResources` to automatically free any resources (meshes, shaders, textures, etc.) that the user forgot to destroy, printing warnings for each leak.
* 5. **Renderer Teardown:** Dispatches to the backend-specific cleanup function (`_SituationCleanupOpenGL` or `_SituationCleanupVulkan`) to destroy all graphics contexts, devices, and internal rendering resources.
* 6. **Subsystem Teardown:** Shuts down all other library modules, including the audio device, input systems, and timers.
* 7. **Platform Teardown:** Destroys the main window and terminates the underlying platform libraries (GLFW, COM).
*
* After this function completes, the library is in an uninitialized state and can be safely re-initialized with `SituationInit` if desired.
*
* @note It is safe to call this function even if `SituationInit` failed, as the internal cleanup helpers are robust to partially initialized states.
* @warning This function is not thread-safe and must be called from the main thread.
*
* @see SituationInit(), _SituationCleanupDanglingResources()
*/
SITAPI void SituationShutdown(void) {
    // [v2.3.40] Set shutting down state
    atomic_store(&sit_render.init_state, SITUATION_STATE_SHUTTING_DOWN);

    if (!_sit_current_context) return;
    // [FIX v2.3.27B] Atomic check-and-set
    /* Exchange returns the *previous* value. After this, SituationIsInitialized() is false
    until the next successful SituationInit — do NOT call SituationIsInitialized() here. */
    if (!atomic_exchange(&sit_gs.is_initialized, false)) {
        return; // Already shut down or shutting down
    }
    if (sit_gs.exit_callback != NULL) { sit_gs.exit_callback(sit_gs.exit_callback_user_data); }

    // 1. Kill Thread Pool (stops I/O thread hot-reload polling)
#if defined(SITUATION_ENABLE_THREADING)
    SituationDestroyThreadPool(&sit_gs.thread_pool);
#endif

    // [v2.4.52] Stop miniaudio before blocking GPU sync. Exclusive-mode output + a long or
    // stuck glFinish / vkDeviceWaitIdle can present as "sound faded, then the UI locked" on shutdown.
    atomic_store(&sit_audio.audio_ready, false);
    (void)SituationStopAllLoadedSounds();
    (void)SituationStopAllTones();
    (void)SituationStopAudioCapture();
    if (sit_audio.is_miniaudio_device_active) {
        ma_device_stop(&sit_audio.miniaudio_device);
        ma_device_uninit(&sit_audio.miniaudio_device);
        sit_audio.is_miniaudio_device_active = false;
    }
    sit_audio.is_miniaudio_device_internally_paused = false;

    // Wait for the GPU to finish any in-flight work before we start tearing things down. This is especially critical for Vulkan.
#if defined(SITUATION_USE_VULKAN)
    _SituationVulkanShutdownWaitGpuPump();
#elif defined(SITUATION_USE_OPENGL)
    // [FIX] When render thread is active, it owns the GL context — glFinish here is a no-op.
    // Destroy the render thread first (it will glFinish internally before releasing context).
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled && atomic_load(&sit_render.thread_active)) {
        _SituationDestroyRenderThread();
        // Now main thread can reclaim context for cleanup
        if (sit_gs.sit_glfw_window) {
            glfwMakeContextCurrent(sit_gs.sit_glfw_window);
            glFinish();
        }
    } else
    #endif
    {
        if (sit_gs.sit_glfw_window) glFinish();
    }
#endif

    // [v2.3.24a] Safety Zenith: Refcount Leak Check
    // Scan frame refcounts for dangling references before shutdown.
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; ++i) {
        int refs = atomic_load(&sit_render.frame_refcounts[i]);
        if (refs > 0) {
            fprintf(stderr, "[Situation] WARNING: Frame %d leaked with %d active references during shutdown!\n", i, refs);
        }
    }

    // Release library-owned resources before user leak reporting.
    _SituationCleanupInternalDefaultResources();

    // --- Call the auto-cleanup function ---
    _SituationCleanupDanglingResources();

    // 0. --- SHUTDOWN RENDER THREAD ---
    // [v2.3.21] Must stop render thread before destroying renderer resources
    SituationError render_thread_err = SITUATION_SUCCESS;
#if !defined(__STDC_NO_THREADS__)
    render_thread_err = _SituationDestroyRenderThread();
#endif

    // 1. --- CLEANUP THE RENDERER ---
    _SituationCleanupRenderer(); // This is the main dispatch for backend-specific cleanup.

    // 2. --- CLEANUP LIBRARY SUBSYSTEMS ---
    _SituationCleanupSubsystems(); // (Audio, Input, Timers, etc.)

    // 3. --- CLEANUP CORE PLATFORM & WINDOW ---
    _SituationCleanupPlatform();

    // 4. --- FINAL STATE RESET ---
    if (_sit_current_context) {
        // Cleanup text scratch
        if (sit_render.text_batch_scratch) { SIT_FREE(sit_render.text_batch_scratch); }

        atomic_store(&sit_gs.is_initialized, false);
        if (render_thread_err == SITUATION_SUCCESS) {
            _SituationSetError("Shutdown complete");
        }

        ma_mutex_uninit(&sit_gs.error_mutex);

        // Free the context
        SIT_FREE(_sit_current_context);
        _sit_current_context = NULL;
    }
}

/**
* @brief [INTERNAL] Shuts down all non-rendering library subsystems.
* @details This helper function is responsible for the orderly teardown of the library's core modules, excluding the graphics backend. It is called by `SituationShutdown` as part of the main cleanup sequence.
*
* @par Cleanup Process
* - **Audio System:** Stops all currently playing sounds, uninitializes the active `miniaudio` device and context, and frees the temporary audio processing buffers.
* - **Synchronization:** Uninitializes all mutexes used for thread-safe event queuing (audio, keyboard, joystick).
* - **Input Systems:** Destroys all standard system cursors that were created by GLFW.
* - **Timer System:** Marks the temporal oscillator system as uninitialized.
*
* @note This function is designed to be robust and can be safely called even if some subsystems failed to initialize fully. It checks the state of each component before attempting to uninitialize it.
* @warning This function is for internal use by `SituationShutdown` only.
*/
static void _SituationCleanupSubsystems(void) {
    // --- Audio System ---
    // [FIX v2.4.38] Signal audio callback to stop processing BEFORE we tear down state.
    atomic_store(&sit_audio.audio_ready, false);

    // Stop all sounds before uninitializing the device.
    SituationStopAllLoadedSounds();
    SituationStopAllTones(); // [Resonance]
    SituationStopAudioCapture(); // Stop recording if active
    if (sit_audio.is_miniaudio_device_active) {
        ma_device_stop(&sit_audio.miniaudio_device);
        ma_device_uninit(&sit_audio.miniaudio_device);
        sit_audio.is_miniaudio_device_active = false;
    }
    sit_audio.is_miniaudio_device_internally_paused = false;

    // [Phase H] Destroy auto-created graph after device stopped so the audio thread cannot touch it.
    // If active_graph pointed at default_graph, clear it before free.
    if (sit_audio.active_graph == sit_audio.default_graph) {
        sit_audio.active_graph = NULL;
    }
    sit_audio.default_graph_voice_source = NULL;
    if (sit_audio.default_graph) {
        SituationDestroyGraph(sit_audio.default_graph);
        sit_audio.default_graph = NULL;
    }

    // [FIX v2.4.15] Destroy audio queue mutex BEFORE context uninit, while we can
    // still verify it was initialized (context init is the prerequisite for mutex init)
    if (sit_audio.is_miniaudio_context_initialized) {
        mtx_destroy(&sit_audio.audio_queue_mutex);
    }

    // Uninitialize the context and free buffers last for the audio system.
    if (sit_audio.is_miniaudio_context_initialized) {
        ma_context_uninit(&sit_audio.miniaudio_context);
        sit_audio.is_miniaudio_context_initialized = false;
    }

    // Cleanup Handle Pool
    _SitAudioCleanupPool();

    SIT_FREE(sit_audio.audio_callback_decoder_temp_buffer);
    SIT_FREE(sit_audio.audio_callback_effects_temp_buffer);
    SIT_FREE(sit_audio.audio_callback_converter_temp_buffer);
    sit_audio.audio_callback_decoder_temp_buffer = NULL;
    sit_audio.audio_callback_effects_temp_buffer = NULL;
    sit_audio.audio_callback_converter_temp_buffer = NULL;

    SIT_FREE(sit_audio.audio_capture_temp_buffer);
    sit_audio.audio_capture_temp_buffer = NULL;

    // [v2.4] Cleanup Dynamic Audio Arrays
    SIT_FREE(sit_audio.active_voices);
    sit_audio.active_voices = NULL;
    SIT_FREE(sit_audio.snapshot_buffer);
    sit_audio.snapshot_buffer = NULL;

    // Input mutexes use standard miniaudio wrappers (non-recursive)
    // [FIX v2.4.15] Only uninit if they were successfully initialized
    if (sit_gs.input_mutexes_initialized) {
        ma_mutex_uninit(&sit_input.keyboard.event_queue_mutex);
        ma_mutex_uninit(&sit_input.mouse.mutex);
        ma_mutex_uninit(&sit_input.joysticks.event_queue_mutex);
        sit_gs.input_mutexes_initialized = false;
    }

    // Cleanup capture resources
    if (sit_audio.audio_capture_on_main_thread) {
        ma_mutex_uninit(&sit_audio.audio_capture_mutex);
        SIT_FREE(sit_audio.audio_capture_queue);
    }

    // --- Input Systems ---
    // Destroy created cursors.
    for (int i = 0; i < sit_input.cursor_count; i++) {
        if (sit_input.cursors[i] != NULL) {
            glfwDestroyCursor(sit_input.cursors[i]);
        }
    }

    // --- Timer System ---
    // Nothing to free, just mark as uninitialized.
    sit_gs.timer_system_instance.is_initialized = false;
}

/**
* @brief [INTERNAL] Dispatches the cleanup process to the active graphics backend.
* @details This helper function serves as a central dispatcher for renderer-specific teardown. It first ensures all user-created virtual displays are destroyed, then calls the appropriate cleanup function (`_SituationCleanupOpenGL` or `_SituationCleanupVulkan`) based on the backend selected at compile time.
*
* @note This function is called by `SituationShutdown` before `_SituationCleanupSubsystems` to ensure that graphics resources are released while the underlying context/device is still valid.
* @warning This function is for internal use by `SituationShutdown` only.
*
* @see _SituationCleanupOpenGL(), _SituationCleanupVulkan()
*/
static void _SituationCleanupRenderer(void) {
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_DISPLAYS; ++i) {
        if (sit_render.virtual_display_slots_used[i]) {
            SituationDestroyVirtualDisplay(i);
        }
    }
#if defined(SITUATION_USE_VULKAN)
    _SituationCleanupVulkan();
#elif defined(SITUATION_USE_OPENGL)
    _SituationCleanupOpenGL();
#endif
    if (sit_render.resource_registry_mutex_initialized) {
        mtx_destroy(&sit_render.resource_registry_mutex);
        sit_render.resource_registry_mutex_initialized = false;
    }
    if (sit_render.momentum_mutex_initialized) {
        mtx_destroy(&sit_render.momentum_mutex);
        sit_render.momentum_mutex_initialized = false;
    }
}

/**
* @brief [INTERNAL] Shuts down the core platform and windowing layer.
* @details This is the final stage of the library's shutdown sequence. It is responsible for destroying the main application window, terminating the underlying windowing library (GLFW), and uninitializing any platform-specific APIs (like COM on Windows).
*
* @par Cleanup Process
* 1. Destroys the main `GLFWwindow` handle.
* 2. Terminates the GLFW library, releasing all of its global resources.
* 3. Frees the memory used for the cached physical display information.
* 4. On Windows, calls `CoUninitialize` to close the COM library if it was opened by the application.
*
* @note This function must be called after the renderer and all other subsystems have been shut down, as they depend on the window and its context.
* @warning This function is for internal use by `SituationShutdown` only.
*/
static void _SituationCleanupPlatform(void) {
    // Destroy the main window (and its GL context).
    if (sit_gs.sit_glfw_window) {
        glfwDestroyWindow(sit_gs.sit_glfw_window);
        sit_gs.sit_glfw_window = NULL;
    }

    // [FIX v2.4.x] Do NOT call glfwTerminate() here.
    // On Windows, glfwTerminate() followed by glfwInit() in the same process
    // leaves the OpenGL ICD in a broken state where GL calls block indefinitely.
    // GLFW stays initialized for the lifetime of the process. This is safe because:
    // - GLFW has no per-process resource leaks when windows are destroyed
    // - The OS reclaims all resources on process exit
    // - This matches how most game engines handle GLFW lifecycle

    // Free display cache.
    if (sit_gs.cached_physical_displays_array) {
        for (int i = 0; i < sit_gs.cached_physical_display_count; ++i) {
            SIT_FREE(sit_gs.cached_physical_displays_array[i].available_modes);
        }
        SIT_FREE(sit_gs.cached_physical_displays_array);
        sit_gs.cached_physical_displays_array = NULL;
    }

    // Uninitialize COM on Windows.
#if defined(_WIN32)
    if (sit_gs.is_com_initialized) {
        CoUninitialize();
        sit_gs.is_com_initialized = false;
    }
#endif
}

/**
* @brief Gets the graphics backend renderer type that the library was compiled with.
* @details This function allows the application to query which rendering backend (OpenGL or Vulkan) is currently active.
* This is useful for writing backend-specific code paths, such as loading pre-compiled SPIR-V shaders for Vulkan or providing raw GLSL for OpenGL, or for displaying renderer information to the user.
*
* The active renderer is determined at compile-time by the `SITUATION_USE_OPENGL` or `SITUATION_USE_VULKAN` preprocessor defines.
*
* @return An enum `SituationRendererType` indicating the active backend (`SIT_RENDERER_OPENGL` or `SIT_RENDERER_VULKAN`).
* @return An undefined or default value if the library is not initialized.
*
* @note This function requires the library to be initialized to return a meaningful value.
*
* @see SituationGetVulkanInstance(), SituationGetGLFWwindow()
*/
SITAPI SituationRendererType SituationGetRendererType(void) {
    return sit_render.renderer_type;
}

/**
* @brief Sets a user-defined callback function to be executed just before the library shuts down.
* @details This function registers a callback that will be invoked at the very beginning of the `SituationShutdown` process.
* It provides a final opportunity for the application to perform its own cleanup tasks, such as saving state to a file, closing network connections, or freeing application-specific memory, while the library's subsystems are still active.
*
* @param callback A function pointer to the callback to be executed. The callback receives the `user_data` pointer as its only argument. Pass `NULL` to clear a previously set callback.
* @param user_data A custom, user-defined pointer that will be passed to the callback function. This can be used to provide context or state to the callback without using global variables.
*
* @note Only one exit callback can be registered at a time. A new call to this function will overwrite any previously set callback.
*
* @see SituationShutdown()
*/
SITAPI void SituationSetExitCallback(void (*callback)(void* user_data), void* user_data) {
    if (!SituationIsInitialized()) return;
    sit_gs.exit_callback = callback;
    sit_gs.exit_callback_user_data = user_data;
}

/**
* @brief Sets a user-defined callback function to be executed when the window's framebuffer is resized.
* @details This function registers a callback that is invoked whenever the pixel dimensions of the main window's rendering area change. This can happen when the user manually resizes the window or when the window is moved between displays with different DPI scaling factors.
* This callback is the primary mechanism for handling resolution-dependent updates, such as:
* - Adjusting the graphics viewport (`glViewport` or `vkCmdSetViewport`).
* - Recalculating projection matrices for cameras.
* - Resizing off-screen framebuffers (`SituationVirtualDisplay`) that should match the window size.
*
* @param callback A function pointer to the callback to be executed. The callback receives the new `width` and `height` in pixels, along with the `user_data` pointer. Pass `NULL` to clear a previously set callback.
* @param user_data A custom, user-defined pointer that will be passed to the callback function.
*
* @note The library's internal `_SituationGLFWFramebufferSizeCallback` is always called first to update internal state (like viewport for OpenGL and the resize flag for Vulkan). The user's callback is invoked immediately after.
* @warning The callback is executed in the same thread that calls `SituationPollInputEvents`. It is not asynchronous and will block the main loop until it returns.
*
* @see SituationGetRenderWidth(), SituationGetRenderHeight()
*/
SITAPI void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data) {
    if (!SituationIsInitialized()) return;
    sit_gs.resize_callback = callback;
    sit_gs.resize_callback_user_data = user_data;
}

/**
* @brief Checks if a command-line argument flag exists.
* @details Searches for an exact match (e.g., "-server", "--fullscreen"). Case-sensitive.
* @param arg_name The argument to search for.
* @return True if the argument was present at launch.
*/
SITAPI bool SituationIsArgumentPresent(const char* arg_name) {
    if (!SituationIsInitialized() || !arg_name) return false;
    for (int i = 1; i < sit_gs.argc; i++) { // Start at 1 to skip the program name
        if (strcmp(sit_gs.argv[i], arg_name) == 0) {
            return true;
        }
    }
    return false;
}

/**
* @brief Gets the value of a command-line argument.
* @details Supports two formats: "-key:value" or "-key value".
* For "-key:value", it returns a pointer to the character after the colon.
* For "-key value", it returns the next argument in the list.
* @param arg_name The key of the argument to look for (e.g., "-level").
* @return A const string with the value, or NULL if the argument is not found.
*/
SITAPI const char* SituationGetArgumentValue(const char* arg_name) {
    if (!SituationIsInitialized() || !arg_name) return NULL;
    size_t arg_len = strlen(arg_name);

    for (int i = 1; i < sit_gs.argc; i++) {
        // Check for "-key:value" format
        if (strncmp(sit_gs.argv[i], arg_name, arg_len) == 0 && sit_gs.argv[i][arg_len] == ':') {
            return sit_gs.argv[i] + arg_len + 1;
        }
        // Check for "-key value" format
        if (strcmp(sit_gs.argv[i], arg_name) == 0) {
            // Ensure there is a next argument to be the value
            if (i + 1 < sit_gs.argc) {
                return sit_gs.argv[i + 1];
            }
        }
    }
    return NULL;
}

/**
* @brief Checks the initialization state of the Situation library.
*
* @details This function serves as a global safety check. It returns `true` only if `SituationInit()` has been called successfully and `SituationShutdown()` has not yet been called.
*
* It is used internally by almost every API function to prevent undefined behavior when accessing
* uninitialized subsystems (like Audio or Vulkan). User code can use this to verify the engine state
* before attempting operations in modular or plugin-based architectures.
*
* @return `true` if the library is fully initialized and ready for use.
* @return `false` if the library is uninitialized, has been shut down, or if initialization failed.
*
* @note This function is safe to call at any time, from any thread, even before `SituationInit()` or after a crash.
*/
SITAPI bool SituationIsInitialized(void) {
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: SituationIsInitialized: Checking _sit_current_context...\n"); fflush(stdout);
#endif
    if (!_sit_current_context) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: SituationIsInitialized: _sit_current_context is NULL!\n"); fflush(stdout);
#endif
        return false;
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: SituationIsInitialized: _sit_current_context OK, checking atomic...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]: SituationIsInitialized: &sit_gs.is_initialized = %p\n", (void*)&sit_gs.is_initialized); fflush(stdout);
#endif
    bool result = atomic_load(&sit_gs.is_initialized);
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: SituationIsInitialized: atomic_load returned %d\n", result); fflush(stdout);
#endif
    return result;
}

/**
* @brief Query the current initialization state of the library.
*
* @return Current SituationInitState value (thread-safe atomic read)
*
* @note This function is thread-safe and can be called from any thread.
* Use this to check if Situation is ready before creating resources
* from external libraries or threads.
*
* @par Example Usage:
* @code
* SituationInit(...);
*
* // Wait for Situation to be fully ready
* while (SituationGetInitState() != SITUATION_STATE_READY) {
    * // Spin or yield
    * }
*
* // Now safe to create pipelines, textures, etc.
* MyLibrary_Init();
* @endcode
*/
SITAPI SituationInitState SituationGetInitState(void) {
    return (SituationInitState)atomic_load(&sit_render.init_state);
}

/**
* @brief Retrieves the current text content from the system clipboard.
*
* @details Returns a pointer to a string managed by the underlying windowing library (GLFW).
* The returned string is valid only until the next call to `SituationGetClipboardText` or `SituationSetClipboardText`,
* or until input events are polled again.
*
* @return A const pointer to a null-terminated UTF-8 string.
* @return `NULL` if the clipboard is empty, contains non-text data, or if an error occurred.
*
* @warning **Do not free** the returned pointer. If you need to persist the text, make a copy immediately (e.g., using `strdup`).
*/
SITAPI SituationError SituationGetClipboardText(const char** out_text) {
    if (out_text) *out_text = NULL;
    else return SITUATION_ERROR_INVALID_PARAM;

    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot get clipboard text");
    }
    // Clear any pending GLFW error before the call.
    glfwGetError(NULL);
    // GLFW handles all the complexity and memory management.
    // Note: The string returned by glfwGetClipboardString is valid until the next call to it or when the window is closed.
    // We return a const pointer directly.
    *out_text = glfwGetClipboardString(sit_gs.sit_glfw_window);
    if (!*out_text) {
        const char* glfw_desc = NULL;
        int glfw_err = glfwGetError(&glfw_desc);
        if (glfw_err != GLFW_NO_ERROR) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_CLIPBOARD_FAILED,
                glfw_desc ? glfw_desc : "Clipboard read failed (GLFW error)");
        }
        // NULL with no GLFW error means clipboard is empty or non-text — not an error
    }
    return SITUATION_SUCCESS;
}

/**
* @brief Copies a string of text to the system clipboard.
*
* @details This function interacts with the operating system's clipboard mechanism (e.g., Ctrl+C / Cmd+C logic).
* It copies the contents of the provided string, making it available to other applications.
*
* @param text A null-terminated, UTF-8 encoded string to place in the clipboard.
* Passing `NULL` or an empty string will clear the clipboard.
*
* @note This operation is generally fast but involves OS interaction.
*/
SITAPI SituationError SituationSetClipboardText(const char* text) {
    if (!SituationIsInitialized()) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot set clipboard text");
    }
    if (!text) {
        // Setting an empty string is the correct way to "clear" the clipboard.
        text = "";
    }
    // Clear any pending GLFW error before the call.
    glfwGetError(NULL);
    glfwSetClipboardString(sit_gs.sit_glfw_window, text);
    const char* glfw_desc = NULL;
    int glfw_err = glfwGetError(&glfw_desc);
    if (glfw_err != GLFW_NO_ERROR) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_CLIPBOARD_FAILED,
            glfw_desc ? glfw_desc : "Clipboard write failed (GLFW error)");
    }
    return SITUATION_SUCCESS;
}

/**
* @brief Registers a callback function to handle file drop events asynchronously.
*
* @details This provides an event-driven alternative to polling `SituationIsFileDropped`. The callback is invoked immediately by the OS event handler when files are dropped.
*
* @param callback The function to call. It receives the file count, the array of paths, and the user data pointer.
* Pass `NULL` to disable the callback.
* @param user_data A custom pointer that will be passed to your callback function (e.g., for context).
*
* @note The callback is executed within the `SituationPollInputEvents()` call stack on the main thread.
*/
SITAPI void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data) {
    // Simply store the user's function pointer and data pointer in our global state.
    // The internal GLFW callback will then use these.
    sit_gs.file_drop_callback = callback;
    sit_gs.file_drop_user_data = user_data;
}

/**
* @brief Checks if any files were dragged and dropped onto the window during the current frame.
*
* @details This is a polling-based state check. It returns `true` for exactly one frame after the drop event occurs.
* Use this function in your main loop to trigger the retrieval of file paths via `SituationLoadDroppedFiles`.
*
* @return `true` if a file drop event was detected this frame, `false` otherwise.
*
* @see SituationLoadDroppedFiles()
*/
SITAPI bool SituationIsFileDropped(void) {
    if (!SituationIsInitialized()) return false;
    return sit_gs.file_was_dropped_this_frame;
}

/**
* @brief Retrieves a list of files dropped onto the application window during the current frame.
*
* @details This function provides access to the file paths captured by the internal drag-and-drop event handler.
* It returns a **copy** of the internal file list, ensuring that the user can safely modify or store the paths without affecting the library's internal state.
*
* @par Usage Workflow
* 1. Check `SituationIsFileDropped()` in your main loop.
* 2. If true, call `SituationLoadDroppedFiles(&count)`.
* 3. Process the files.
* 4. Call `SituationUnloadDroppedFiles(paths, count)` to free the memory.
*
* @param[out] count A pointer to an integer that will be filled with the number of dropped files.
*
* @return A pointer to a dynamically allocated array of dynamically allocated strings (char**).
* @return `NULL` if no files were dropped this frame, or if the library is not initialized. In this case, `*count` is set to 0.
*
* @warning The caller owns the returned memory and is **responsible** for freeing it using `SituationUnloadDroppedFiles()`.
* Do not use standard `free()` on the array pointer alone, as this will leak the individual string buffers.
*
* @see SituationIsFileDropped(), SituationUnloadDroppedFiles()
*/
SITAPI char** SituationLoadDroppedFiles(int* count) {
    if (!SituationIsInitialized() || sit_gs.dropped_file_count == 0) {
        *count = 0;
        return NULL;
    }

    // Return a COPY of the internal list, so the user can own it and free it.
    char** user_list = (char**)SIT_MALLOC(sit_gs.dropped_file_count * sizeof(char*));
    if (user_list == NULL) {
        *count = 0;
        return NULL;
    }

    for (int i = 0; i < sit_gs.dropped_file_count; i++) {
        user_list[i] = _sit_strdup(sit_gs.dropped_file_paths[i]);
        if (user_list[i] == NULL) {
            // Allocation failed, clean up this partial list
            for (int j = 0; j < i; j++) SIT_FREE(user_list[j]);
            SIT_FREE(user_list);
            *count = 0;
            return NULL;
        }
    }

    *count = sit_gs.dropped_file_count;

    // We have now given the user their own copy. We can clear the internal one.
    // This also prevents calling LoadDroppedFiles multiple times for the same drop event.
    for (int i = 0; i < sit_gs.dropped_file_count; i++) {
        SIT_FREE(sit_gs.dropped_file_paths[i]);
    }
    SIT_FREE(sit_gs.dropped_file_paths);
    sit_gs.dropped_file_paths = NULL;
    sit_gs.dropped_file_count = 0;

    return user_list;
}

/**
* @brief Frees the memory allocated by `SituationLoadDroppedFiles`.
*
* @details This helper function correctly iterates through the array of file paths, freeing each string individually, and then frees the array pointer itself.
* This is the mandatory cleanup function for the file drop API.
*
* @param paths The array of strings returned by `SituationLoadDroppedFiles`.
* @param count The number of strings in the array (returned by `SituationLoadDroppedFiles`).
*
* @note It is safe to call this function with `NULL` or a count of 0; it will simply do nothing.
*/
SITAPI void SituationUnloadDroppedFiles(char** paths, int count) {
    if (paths == NULL || count == 0) return;
    for (int i = 0; i < count; i++) {
        SIT_FREE(paths[i]);
    }
    SIT_FREE(paths);
}

SITAPI SituationGraphicsBackend SituationGetGraphicsBackend(void) {
#if defined(SITUATION_USE_VULKAN)
    return SIT_GRAPHICS_BACKEND_VULKAN;
#elif defined(SITUATION_USE_OPENGL)
    return SIT_GRAPHICS_BACKEND_OPENGL;
#else
    return SIT_GRAPHICS_BACKEND_UNKNOWN;
#endif
}

SITAPI const char* SituationGetGraphicsBackendName(void) {
    switch (SituationGetGraphicsBackend()) {
        case SIT_GRAPHICS_BACKEND_OPENGL: return "OpenGL";
        case SIT_GRAPHICS_BACKEND_VULKAN: return "Vulkan";
        default: return "Unknown";
    }
}

static uint32_t _SituationPackApiVersionMajorMinor(uint32_t major, uint32_t minor) {
    return (major << 16) | (minor & 0xFFFFu);
}

SITAPI void SituationGetGraphicsCaps(SituationGraphicsCaps* out_caps) {
    if (!out_caps) return;
    memset(out_caps, 0, sizeof(SituationGraphicsCaps));
    out_caps->backend = SituationGetGraphicsBackend();
    if (!SituationIsInitialized()) return;

#if defined(SITUATION_USE_VULKAN)
    out_caps->api_version_packed = _SituationPackApiVersionMajorMinor(1u, 4u); /* Situation Vulkan backend target */
    if (sit_render.vk.physical_device_api_version != 0u) {
        out_caps->device_api_version_packed = _SituationPackApiVersionMajorMinor(
            VK_API_VERSION_MAJOR(sit_render.vk.physical_device_api_version),
            VK_API_VERSION_MINOR(sit_render.vk.physical_device_api_version));
    }
    out_caps->max_msaa_samples = 1; // MSAA querying not yet implemented for Vulkan backend
    out_caps->bindless_textures = (sit_render.enabled_features_mask & SIT_FEATURE_BINDLESS_TEXTURES) ? 1 : 0;
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    out_caps->shader_compiler_available = 1;
#else
    out_caps->shader_compiler_available = 0;
#endif
    out_caps->compute_supported = 1; // Always true for Vulkan
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(sit_render.vk.physical_device, &props);
        // Derive max MSAA sample count from the intersection of color + depth framebuffer limits
        VkSampleCountFlags color_depth = props.limits.framebufferColorSampleCounts
                                       & props.limits.framebufferDepthSampleCounts;
        int max_s = 1;
        if (color_depth & VK_SAMPLE_COUNT_64_BIT) max_s = 64;
        else if (color_depth & VK_SAMPLE_COUNT_32_BIT) max_s = 32;
        else if (color_depth & VK_SAMPLE_COUNT_16_BIT) max_s = 16;
        else if (color_depth & VK_SAMPLE_COUNT_8_BIT)  max_s = 8;
        else if (color_depth & VK_SAMPLE_COUNT_4_BIT)  max_s = 4;
        else if (color_depth & VK_SAMPLE_COUNT_2_BIT)  max_s = 2;
        out_caps->max_msaa_samples = max_s;
        int max_vp = (int)props.limits.maxViewports;
        out_caps->max_viewports = (max_vp >= 1) ? max_vp : 1;
    }
#elif defined(SITUATION_USE_OPENGL)
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    out_caps->api_version_packed = _SituationPackApiVersionMajorMinor(4u, 6u); /* Situation OpenGL backend target */
    out_caps->device_api_version_packed = _SituationPackApiVersionMajorMinor((uint32_t)major, (uint32_t)minor);

    GLint max_samples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
    out_caps->max_msaa_samples = max_samples;

    out_caps->bindless_textures = (sit_render.enabled_features_mask & SIT_FEATURE_BINDLESS_TEXTURES) ? 1 : 0;
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    out_caps->shader_compiler_available = 1;
#else
    out_caps->shader_compiler_available = 0;
#endif
    out_caps->compute_supported = (major > 4 || (major == 4 && minor >= 3)) ? 1 : 0;
    {
        // Use cached value — GL context may not be current on main thread
        out_caps->max_viewports = (sit_render.cached_max_viewports >= 1) ? sit_render.cached_max_viewports : 1;
    }
#endif
}

#endif // SITUATION_IMPL_CTRL_H
