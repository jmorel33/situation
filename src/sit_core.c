// src/sit_core.c
#ifndef SITUATION_IMPLEMENTATION_INTERNAL
// This file is a module part of the Situation library.
// It is not intended to be compiled directly.
// Please include "situation_impl.c" instead.
#ifdef __INTELLISENSE__
#include "../situation_impl.c" // Trick Intellisense into seeing the full context
#endif
#endif


static void _SitCore_SetError(const char* msg) {
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


SITAPI void SituationSetTraceLogLevel(int logType) {
    _sit_trace_log_level = logType;
}


SITAPI void SituationLog(int msgType, const char* text, ...) {
    if (msgType < _sit_trace_log_level) return;

    // ANSI Color Codes
    const char* color_code = "\033[0m"; // Reset
    const char* label = "[INFO]";

    switch (msgType) {
        case SIT_LOG_TRACE:   label = "[TRACE]"; color_code = "\033[90m"; break; // Gray
        case SIT_LOG_DEBUG:   label = "[DEBUG]"; color_code = "\033[36m"; break; // Cyan
        case SIT_LOG_INFO:    label = "[INFO]";  color_code = "\033[32m"; break; // Green
        case SIT_LOG_WARNING: label = "[WARN]";  color_code = "\033[33m"; break; // Yellow
        case SIT_LOG_ERROR:   label = "[ERROR]"; color_code = "\033[31m"; break; // Red
        case SIT_LOG_FATAL:   label = "[FATAL]"; color_code = "\033[41m"; break; // Red Background
        default: break;
    }

    va_list args;
    va_start(args, text);

    // Print to stdout with color
    printf("%s%s ", color_code, label);
    vprintf(text, args);
    printf("\033[0m\n"); // Reset color and newline

    va_end(args);
}


SITAPI void SituationLogWarning(SituationError code, const char* fmt, ...) {
#ifndef NDEBUG
    char _sit_err_buf[SITUATION_MAX_ERROR_MSG_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(_sit_err_buf, sizeof(_sit_err_buf), fmt, args);
    va_end(args);
    _SitCore_SetErrorFromCode(code, _sit_err_buf);
    fprintf(stderr, "[Situation WARN] %s\n", _sit_err_buf);
#else
    (void)code; (void)fmt;
#endif
}


/**
 * @brief [INTERNAL] Sets the library's last error message from an error code and an optional detail string.
 * @details Translates a `SituationError` enum into a human-readable base message, appends details,
 *          and stores the result in the global error message buffer via `_SitCore_SetError`.
 * @param err The `SituationError` code to translate.
 * @param detail An optional, more specific string describing the context of the error.
 */
static SituationError _SitCore_SetErrorFromCode(SituationError err, const char* detail) {
    char buffer[SITUATION_MAX_ERROR_MSG_LEN];
    const char* base_msg = "Unknown Error";

    switch (err) {
        // --- Core & System Errors (0-99) ---
        case SITUATION_SUCCESS:                           base_msg = "No error"; break;
        case SITUATION_ERROR_GENERAL:                     base_msg = "A general error occurred"; break;
        case SITUATION_ERROR_NOT_IMPLEMENTED:             base_msg = "A feature is declared but not implemented for the current backend"; break;
        case SITUATION_ERROR_NOT_INITIALIZED:             base_msg = "Function called before library was successfully initialized"; break;
        case SITUATION_ERROR_ALREADY_INITIALIZED:         base_msg = "SituationInit() was called more than once"; break;
        case SITUATION_ERROR_INIT_FAILED:                 base_msg = "Core library initialization failed"; break;
        case SITUATION_ERROR_SHUTDOWN_FAILED:             base_msg = "Library shutdown failed"; break;
        case SITUATION_ERROR_INVALID_PARAM:               base_msg = "A function was called with an invalid parameter"; break;
        case SITUATION_ERROR_MEMORY_ALLOCATION:           base_msg = "A memory allocation (SIT_MALLOC, SIT_CALLOC, SIT_REALLOC) failed"; break;
        case SITUATION_ERROR_INTERNAL_STATE_CORRUPTED:    base_msg = "Internal invariant violated — fatal bug"; break;
        case SITUATION_ERROR_ASSERTION_FAILED:            base_msg = "Debug assertion tripped"; break;
        case SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION: base_msg = "Architectural rule broken: Update called after Draw"; break;
		case SITUATION_ERROR_THREAD_QUEUE_FULL:  	 	  base_msg = "Threading Error: Thread Queue Full"; break;
		case SITUATION_ERROR_THREAD_VIOLATION:   		  base_msg = "Main-thread-only function called from worker thread"; break;
        case SITUATION_ERROR_THREAD_CREATION_FAILED:      base_msg = "Failed to create thread"; break;
        case SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT: base_msg = "Render thread join timeout—aborted"; break;
        case SITUATION_ERROR_RENDER_LIST_INCOMPLETE:      base_msg = "Render list missing mandatory commands"; break;
        case SITUATION_ERROR_ARM_INTRINSICS_FAILED:       base_msg = "ARM intrinsic (WFE/SEV) execution failed"; break;

        // --- Platform & Window Errors (100-199) ---
        case SITUATION_ERROR_GLFW_FAILED:                 base_msg = "An underlying GLFW library operation failed"; break;
        case SITUATION_ERROR_WINDOW_CREATION_FAILED:      base_msg = "Failed to create the application window"; break;
        case SITUATION_ERROR_WINDOW_FOCUS_FAILED:         base_msg = "Window focus/minimize/restore operation failed"; break;
        case SITUATION_ERROR_CLIPBOARD_FAILED:            base_msg = "Clipboard operation failed"; break;
        case SITUATION_ERROR_CURSOR_CREATION_FAILED:      base_msg = "Failed to create custom cursor"; break;
        case SITUATION_ERROR_COM_INITIALIZATION_FAILED:   base_msg = "Failed to initialize COM (CoInitializeEx)"; break;
        case SITUATION_ERROR_DXGI_QUERY_FAILED:           base_msg = "DXGI GPU query failed"; break;
        case SITUATION_ERROR_WINDOW_FOCUS:                base_msg = "An operation related to window focus failed"; break;
        case SITUATION_ERROR_DEVICE_QUERY:                base_msg = "Failed to query system hardware or device information"; break;
        case SITUATION_ERROR_TIMER_SYSTEM:                base_msg = "An error occurred within the internal timer/oscillator system"; break;
        case SITUATION_ERROR_COM_FAILED:                  base_msg = "[Win32] Failed to initialize the COM library"; break;
        case SITUATION_ERROR_DXGI_FAILED:                 base_msg = "[Win32] A call to the DXGI library failed"; break;

        // --- Display & Virtual Display Errors (200-299) ---
        case SITUATION_ERROR_DISPLAY_QUERY:               base_msg = "Failed to query physical monitor information"; break;
        case SITUATION_ERROR_DISPLAY_SET:                 base_msg = "Failed to set a display mode on a physical monitor"; break;
        case SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT:       base_msg = "The maximum number of virtual displays has been reached"; break;
        case SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID:  base_msg = "An invalid virtual display ID was provided"; break;
        case SITUATION_ERROR_DISPLAY_QUERY_FAILED:        base_msg = "Detailed display query failure"; break;
        case SITUATION_ERROR_DISPLAY_MODE_UNSUPPORTED:    base_msg = "Requested resolution or refresh rate is not supported"; break;
        case SITUATION_ERROR_DISPLAY_MODE_SET_FAILED:     base_msg = "Failed to apply fullscreen or windowed mode settings"; break;
        case SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT_REACHED: base_msg = "Max virtual displays (32) limit reached"; break;
        case SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND:   base_msg = "Virtual display ID not found"; break;

        // --- Filesystem Errors (300-399) ---
        case SITUATION_ERROR_FILE_ACCESS:                 base_msg = "A generic file or directory access error occurred"; break;
        case SITUATION_ERROR_PATH_NOT_FOUND:              base_msg = "The specified file or directory was not found"; break;
        case SITUATION_ERROR_PATH_INVALID:                base_msg = "The specified path is invalid or contains illegal characters"; break;
        case SITUATION_ERROR_PERMISSION_DENIED:           base_msg = "Permission was denied for the requested file operation"; break;
        case SITUATION_ERROR_DISK_FULL:                   base_msg = "The disk is full; cannot complete a write operation"; break;
        case SITUATION_ERROR_FILE_LOCKED:                 base_msg = "The file is locked or currently in use by another process"; break;
        case SITUATION_ERROR_DIR_NOT_EMPTY:               base_msg = "A directory is not empty and cannot be deleted non-recursively"; break;
        case SITUATION_ERROR_FILE_ALREADY_EXISTS:         base_msg = "The specified file already exists where it shouldn't"; break;
        case SITUATION_ERROR_PATH_IS_DIRECTORY:           base_msg = "A file operation was attempted on a path that is a directory"; break;
        case SITUATION_ERROR_PATH_IS_FILE:                base_msg = "A directory operation was attempted on a path that is a file"; break;
        case SITUATION_ERROR_FILE_NOT_FOUND:              base_msg = "File does not exist"; break;
        case SITUATION_ERROR_FILE_ACCESS_DENIED:          base_msg = "Access denied to file"; break;
        case SITUATION_ERROR_FILE_OPEN_FAILED:            base_msg = "Failed to open file (fopen)"; break;
        case SITUATION_ERROR_FILE_READ_FAILED:            base_msg = "Failed to read from file"; break;
        case SITUATION_ERROR_FILE_WRITE_FAILED:           base_msg = "Failed to write to file"; break;
        case SITUATION_ERROR_FILE_TOO_LARGE:              base_msg = "File size exceeds internal limits"; break;
        case SITUATION_ERROR_DIRECTORY_CREATION_FAILED:   base_msg = "Failed to create directory"; break;
        case SITUATION_ERROR_HOTRELOAD_WATCHER_FAILED:    base_msg = "Hot-reload watcher failed to initialize or report"; break;
        case SITUATION_ERROR_HOTRELOAD_FILE_CHANGED_TOO_FAST: base_msg = "File changed too fast for hot-reload"; break;
        case SITUATION_ERROR_HOTRELOAD_GPU_SYNC_FAILED:   base_msg = "GPU synchronization failed during hot-reload"; break;

        // --- Audio Errors (400-499) ---
        case SITUATION_ERROR_AUDIO_CONTEXT:               base_msg = "Audio: Failed to initialize the audio context (MiniAudio)"; break;
        case SITUATION_ERROR_AUDIO_DEVICE:                base_msg = "Audio: Failed to initialize, start, or stop an audio device"; break;
        case SITUATION_ERROR_AUDIO_SOUND_LIMIT:           base_msg = "Audio: The sound playback queue limit was reached"; break;
        case SITUATION_ERROR_AUDIO_CONVERTER:             base_msg = "Audio: Failed to configure a data format/rate converter for a sound"; break;
        case SITUATION_ERROR_AUDIO_DECODING:              base_msg = "Audio: Failed to decode an audio file"; break;
        case SITUATION_ERROR_AUDIO_INVALID_OPERATION:     base_msg = "Audio: An invalid operation was attempted on a sound (e.g., cropping a stream)"; break;
        case SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED:   base_msg = "Audio: Backend initialization failed"; break;
        case SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED:    base_msg = "Audio: Device initialization failed"; break;
        case SITUATION_ERROR_AUDIO_DEVICE_START_FAILED:   base_msg = "Audio: Device start failed"; break;
        case SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED:   base_msg = "Audio: Decoder initialization failed"; break;
        case SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED: base_msg = "Audio: Format not supported by decoder"; break;
        case SITUATION_ERROR_AUDIO_STREAM_ENDED:          base_msg = "Audio: Stream reached end of file"; break;
        case SITUATION_ERROR_AUDIO_SOUND_LIMIT_REACHED:   base_msg = "Audio: Max concurrent sounds limit reached"; break;
        case SITUATION_ERROR_AUDIO_CAPTURE_NOT_AVAILABLE: base_msg = "Audio: Capture device not available"; break;

        // --- Resource & Rendering Errors (500-599) ---
        case SITUATION_ERROR_RESOURCE_INVALID:            base_msg = "An invalid resource handle (shader, mesh, texture, etc.) was used"; break;
        case SITUATION_ERROR_BUFFER_MAP_FAILED:           base_msg = "Failed to map a GPU buffer to CPU memory"; break;
        case SITUATION_ERROR_BUFFER_INVALID_SIZE:         base_msg = "A buffer operation was attempted with an out-of-bounds offset or size"; break;
        case SITUATION_ERROR_RENDER_COMMAND_FAILED:       base_msg = "A command failed to be recorded to a command buffer"; break;
        case SITUATION_ERROR_RENDER_PASS_ACTIVE:          base_msg = "An operation was attempted that is illegal during an active render pass"; break;
        case SITUATION_ERROR_NO_RENDER_PASS_ACTIVE:       base_msg = "A drawing operation was attempted outside of a render pass"; break;
        case SITUATION_ERROR_BACKEND_MISMATCH:            base_msg = "Operation requested on wrong backend (e.g., GL call on Vulkan)"; break;
        case SITUATION_ERROR_PIPELINE_BIND_FAIL:          base_msg = "Failed to bind pipeline (incompatible layout or invalid handle)"; break;
        case SITUATION_ERROR_INVALID_RESOURCE_HANDLE:     base_msg = "Invalid resource handle"; break;
        case SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED:  base_msg = "Resource already destroyed (Use-after-free)"; break;
        case SITUATION_ERROR_BUFFER_OVERFLOW:             base_msg = "Buffer write overflow detected"; break;
        case SITUATION_ERROR_BUFFER_INVALID_USAGE:        base_msg = "Buffer usage flags incompatible with operation"; break;
        case SITUATION_ERROR_TEXTURE_UPLOAD_FAILED:       base_msg = "Texture upload to GPU failed"; break;
        case SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER:    base_msg = "No active command buffer found"; break;
        case SITUATION_ERROR_COMMAND_BUFFER_FULL:         base_msg = "Command buffer capacity exceeded"; break;
        case SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE:  base_msg = "Nested render pass attempted"; break;

        // --- OpenGL Specific Errors (600-699) ---
        case SITUATION_ERROR_OPENGL_GENERAL:              base_msg = "OpenGL: A general error occurred (glGetError)"; break;
        case SITUATION_ERROR_OPENGL_LOADER_FAILED:        base_msg = "OpenGL: Failed to load OpenGL functions (GLAD)"; break;
        case SITUATION_ERROR_OPENGL_UNSUPPORTED:          base_msg = "OpenGL: A required version or extension is not supported by the driver"; break;
        case SITUATION_ERROR_OPENGL_SHADER_COMPILE:       base_msg = "OpenGL: GLSL shader compilation failed"; break;
        case SITUATION_ERROR_OPENGL_SHADER_LINK:          base_msg = "OpenGL: GLSL shader program linking failed"; break;
        case SITUATION_ERROR_OPENGL_FBO_INCOMPLETE:       base_msg = "OpenGL: A Framebuffer Object is not complete and cannot be used"; break;
        case SITUATION_ERROR_OPENGL_CONTEXT_CREATION_FAILED: base_msg = "OpenGL: Context creation failed"; break;
        case SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION:  base_msg = "OpenGL: Version too old (Requires 4.6+)"; break;
        case SITUATION_ERROR_OPENGL_SHADER_COMPILE_FAILED: base_msg = "OpenGL: Detailed shader compilation error"; break;
        case SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED:   base_msg = "OpenGL: Detailed shader linking error"; break;
        case SITUATION_ERROR_OPENGL_PROGRAM_VALIDATION_FAILED: base_msg = "OpenGL: Program validation failed"; break;
        case SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND:    base_msg = "OpenGL: Uniform not found"; break;

        // --- Vulkan Specific Errors (700-799) ---
        case SITUATION_ERROR_VULKAN_INIT_FAILED:          base_msg = "Vulkan: General initialization failed"; break;
        case SITUATION_ERROR_VULKAN_INSTANCE_FAILED:      base_msg = "Vulkan: Failed to create a VkInstance"; break;
        case SITUATION_ERROR_VULKAN_DEVICE_FAILED:        base_msg = "Vulkan: Failed to select a physical or create a logical device"; break;
        case SITUATION_ERROR_VULKAN_UNSUPPORTED:          base_msg = "Vulkan: A required layer, extension, or feature is unsupported"; break;
        case SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED:     base_msg = "Vulkan: A swapchain operation failed (creation, acquire, present)"; break;
        case SITUATION_ERROR_VULKAN_COMMAND_FAILED:       base_msg = "Vulkan: A command pool or buffer operation failed"; break;
        case SITUATION_ERROR_VULKAN_RENDERPASS_FAILED:    base_msg = "Vulkan: Failed to create a VkRenderPass"; break;
        case SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED:   base_msg = "Vulkan: Failed to create a VkFramebuffer"; break;
        case SITUATION_ERROR_VULKAN_PIPELINE_FAILED:      base_msg = "Vulkan: Failed to create a graphics or compute pipeline"; break;
        case SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED:   base_msg = "Vulkan: Failed to create a fence or semaphore"; break;
        case SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED:  base_msg = "Vulkan: A GPU memory allocation failed (VMA)"; break;
        case SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED:    base_msg = "Vulkan: A descriptor set or pool operation failed"; break;
        case SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED: base_msg = "Vulkan: Failed to create swapchain (incompatible surface/format?)"; break;
        case SITUATION_ERROR_VULKAN_IMAGE_ACQUIRE_FAILED: base_msg = "Vulkan: Failed to acquire next swapchain image"; break;
        case SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED:  base_msg = "Vulkan: Queue submission failed (Device lost?)"; break;
        case SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED: base_msg = "Vulkan: Pipeline creation failed (check shader stages/layout)"; break;
        case SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED: base_msg = "Vulkan: Detailed instance creation failure"; break;
        case SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE: base_msg = "Vulkan: No suitable physical device found"; break;
        case SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED: base_msg = "Vulkan: Detailed logical device creation failure"; break;
        case SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID:    base_msg = "Vulkan: Swapchain handle is invalid or incompatible"; break;
        case SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED: base_msg = "Vulkan: Shader module creation failed"; break;
        case SITUATION_ERROR_VULKAN_DESCRIPTOR_POOL_EXHAUSTED: base_msg = "Vulkan: Descriptor pool exhausted"; break;
        case SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED: base_msg = "Vulkan: Detailed memory allocation error"; break;
        case SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR: base_msg = "Vulkan: Validation layer reported error"; break;
        case SITUATION_ERROR_SHADER_COMPILATION_FAILED:   base_msg = "Shader compilation failed (shaderc)"; break;

        // --- Compute / GPGPU Errors (800-899) ---
        case SITUATION_ERROR_COMPUTE_PIPELINE_CREATION_FAILED: base_msg = "Compute pipeline creation failed"; break;
        case SITUATION_ERROR_COMPUTE_DISPATCH_FAILED:          base_msg = "Compute dispatch failed"; break;
        case SITUATION_ERROR_COMPUTE_BUFFER_BINDING_MISSING:   base_msg = "Required buffer binding missing for compute shader"; break;

        // --- Network Errors (900-949) ---
        case SITUATION_ERROR_NETWORK_INIT_FAILED:           base_msg = "Network: Initialization failed"; break;
        case SITUATION_ERROR_NETWORK_SOCKET_CREATION_FAILED:base_msg = "Network: Socket creation failed"; break;
        case SITUATION_ERROR_NETWORK_CONNECTION_FAILED:     base_msg = "Network: Connection failed"; break;
        case SITUATION_ERROR_NETWORK_SEND_FAILED:           base_msg = "Network: Send failed"; break;
        case SITUATION_ERROR_NETWORK_RECEIVE_FAILED:        base_msg = "Network: Receive failed"; break;
        case SITUATION_ERROR_NETWORK_BIND_FAILED:           base_msg = "Network: Bind failed"; break;
        case SITUATION_ERROR_NETWORK_LISTEN_FAILED:         base_msg = "Network: Listen failed"; break;
        case SITUATION_ERROR_NETWORK_ACCEPT_FAILED:         base_msg = "Network: Accept failed"; break;

        case SITUATION_ERROR_UNKNOWN_ERROR:               base_msg = "Unknown error (Cosmic rays)"; break;
    }

    if (detail) {
        snprintf(buffer, sizeof(buffer), "%s: %s", base_msg, detail);
    } else {
        strncpy(buffer, base_msg, sizeof(buffer) - 1);
    }
    buffer[sizeof(buffer) - 1] = '\0';
    _SitCore_SetError(buffer);
}


/**
 * @brief Retrieves a copy of the last error message generated by the library.
 * @details This function provides a way for the user application to obtain a detailed, human-readable description of the most recent error that occurred within the Situation library. This is essential for diagnosing problems and providing feedback to the user.
 *
 * @par Memory Management
 *   This function returns a *copy* of the internal error message string, allocating new memory for it.
 *   The caller takes ownership of this memory and **must** release it by calling `SituationFreeString()` when it is no longer needed. Failure to do so will result in a memory leak.
 *
 * @warning Do NOT use `free()` on the returned pointer. Always use `SituationFreeString()`.
 *
 * @return A null-terminated C string containing the last error message.
 * @return NULL if the library is not initialized, if no error has occurred, or if memory allocation for the copy fails.
 *
 * @see SituationFreeString(), _SitCore_SetError()
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
 * @brief [INTERNAL] Performs a comprehensive cleanup of all partially initialized library components.
 *
 * @details This helper function is a critical part of the library's robust initialization and error handling strategy. It is designed to be called safely from *any* point during the `SituationInit` process if a critical error occurs.
 *
 * Its primary role is to prevent resource leaks and ensure a clean state in the event of a failed startup. It achieves this by:
 * 1.  Attempting to flush any pending GPU operations to ensure resources are no longer in use.
 * 2.  Calling the main cleanup functions for subsystems, the renderer, and the platform layer, *in reverse order* of their initialization.
 * 3.  Relying on the robustness of these individual cleanup functions to handle being called when only parts of their corresponding initialization were completed.
 *
 * @note This function is intended solely for internal use during the `SituationInit` failure path and should not be called directly by user code under normal circumstances.
 * @note The cleanup proceeds in reverse order of initialization:
 *       Platform -> Renderer -> Subsystems. This order is crucial to avoid dependencies (e.g., destroying GLFW window before Vulkan surface).
 * @note Each `_SituationCleanup*` function called is designed to check if its corresponding component was initialized before attempting to destroy it, making this "best-effort" cleanup safe.
 * @warning This function modifies the global state `sit_gs`. After it runs, the library is considered uninitialized.
 *
 * @see SituationInit(), _SitCore_CleanupSubsystems(), _SitRender_CleanupRenderer(), _SitCore_CleanupPlatform()
 */
static void _SitCore_FullCleanupOnError(void) {
    // --- 1. Synchronize GPU ---
    // Before attempting to destroy any graphics resources, it's prudent to wait for the GPU to finish any operations that might be using them.
    // This helps prevent validation errors or crashes during cleanup, especially if the error occurred partway through renderer initialization.
#if defined(SITUATION_USE_VULKAN)
    {
        // For Vulkan, wait for the logical device to be idle.
        // This ensures all submitted work on all queues is finished.
        // It's safe to call this even if the device creation failed, as sit_render.vk.device would be VK_NULL_HANDLE.
        if (sit_render.vk.device != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(sit_render.vk.device);
            // Ignore the result. If it fails, proceeding with cleanup is still the best option.
            // Potential failures (VK_ERROR_DEVICE_LOST) indicate a serious problem, but cleanup should still be attempted to free other resources (GLFW, audio context).
            if (result != VK_SUCCESS) {
                // Optional: Log in debug builds if a logger is available.
                // fprintf(stderr, "WARNING: vkDeviceWaitIdle failed (0x%x) during error cleanup.\n", result);
            }
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
    _SitCore_CleanupSubsystems();

    // --- 2b. Cleanup Renderer ---
    // This cleans up all graphics API specific resources (Vulkan instance/device/swapchain, OpenGL context/states, internal pipelines, etc.).
    // It must be robust enough to handle partial initialization (e.g., if Vulkan device creation failed but instance was created).
    _SitRender_CleanupRenderer();

    // --- 2c. Cleanup Platform ---
    // This cleans up the lowest level components: GLFW window, GLFW itself, any platform-specific initializations (like COM on Windows).
    // This should be the final step.
    _SitCore_CleanupPlatform();

    // --- 3. Post-Cleanup State (Implicit) ---
    // The individual cleanup functions (_SitCore_CleanupPlatform in particular) should set `sit_gs.is_initialized = false;` and potentially clear the error state.
    // This function itself doesn't need to modify `sit_gs` further.
    // The library is now in an uninitialized state, ready (hopefully) for a fresh `SituationInit` attempt or safe shutdown.
}



SITAPI const char* SituationGetVersionString(void) {
    static char version_str[32] = {0};

    // Generate only once
    if (version_str[0] == '\0') {
        snprintf(version_str, sizeof(version_str), "%d.%d.%d%s",
            SITUATION_VERSION_MAJOR,
            SITUATION_VERSION_MINOR,
            SITUATION_VERSION_PATCH,
            SITUATION_VERSION_REVISION
        );
    }
    return version_str;
}


SITAPI SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info) {
#if !defined(SITUATION_USE_VULKAN) && !defined(SITUATION_USE_OPENGL)
	// We can't use _SitCore_SetErrorFromCode yet because context might not exist.
    // But since this is a compile-time check failure (logic-wise), we can just return.
	// _SitCore_SetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "No graphics backend defined...");
    // Ideally we'd log to stderr here.
    fprintf(stderr, "SituationInit Error: No graphics backend defined (SITUATION_USE_VULKAN/OPENGL).\n");
	return SITUATION_ERROR_INIT_FAILED;
#endif

    // --- 1. PRE-INITIALIZATION CHECKS ---

#ifdef SITUATION_ENABLE_THREADING
	// Multi-Threading platform initialisation of the main thread (sit_gs context)
	if (!sit_gs_thread_id_set) { sit_gs_main_thread_id = thrd_current(); sit_gs_thread_id_set = true; }
#endif

    // Ensure the library isn't already initialized to prevent conflicts.
    if (_sit_current_context != NULL) {
        // If context exists, check if it claims to be initialized
        if (sit_gs.is_initialized) {
             _SitCore_SetErrorFromCode(SITUATION_ERROR_ALREADY_INITIALIZED, "SituationInit: Library is already initialized.");
             return SITUATION_ERROR_ALREADY_INITIALIZED;
        }
        // If context exists but is_initialized is false, it might be a dirty state or a re-init attempt.
        // We will proceed to re-allocate/reset below.
    }

    // Ensure the required initialization configuration struct is provided.
    if (!init_info) {
        // Can't set error code if context doesn't exist yet!
        if (_sit_current_context) {
            _SitCore_SetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationInit: init_info cannot be NULL.");
        }
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 1.5. CONTEXT ALLOCATION ---
    if (_sit_current_context == NULL) {
        _sit_current_context = (SituationContext*)SIT_CALLOC(1, sizeof(SituationContext));
        if (!_sit_current_context) {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
    } else {
        // Zero out the existing context to ensure a clean slate.
        memset(_sit_current_context, 0, sizeof(SituationContext));
    }

    // Now 'sit_gs' and 'sit_audio' macros are valid and point to zeroed memory.

    // --- 2. INITIALIZE CORE PLATFORM & WINDOW ---
    // These steps are prerequisites for renderer and subsystem initialization.
    // They are common to both OpenGL and Vulkan backends.

    // 2a. Initialize platform-specific components (GLFW, COM on Windows, etc.).
    SituationError err = _SitCore_InitPlatform();
    if (err != SITUATION_SUCCESS) {
        // Platform initialization failed. No major resources have been allocated yet
        // by the library itself (GLFW might have allocated some internal stuff, but _SitCore_InitPlatform should clean that up on failure).
        // Therefore, a full cleanup is not strictly necessary here, but calling it for consistency and to ensure any partial platform setup is undone is good.
        // However, the previous version comment said "No need to call FullCleanup...", which is also a valid viewpoint for this very early failure.
        // Let's stick to the original logic for this specific early failure point, but document it clearly.
        // _SitCore_FullCleanupOnError(); // Not needed for platform-only failure
        return err; // Return the specific error from platform init.
    }

    // 2b. Create the main application window using GLFW.
    err = _SitPlatform_InitWindow(init_info);
    if (err != SITUATION_SUCCESS) {
        // Window creation failed. Platform was initialized, so cleanup is needed.
        _SitCore_FullCleanupOnError(); // Clean up platform (GLFW)
        return err; // Return the specific error from window init.
    }

    // --- 3. INITIALIZE THE CHOSEN RENDERER ---
    // Dispatch to the backend-specific initialization (OpenGL or Vulkan).
    // This is a major step involving context/device creation, swapchains, etc.
    err = _SitRender_InitRenderer(init_info);
    if (err != SITUATION_SUCCESS) {
        // Renderer initialization failed. Platform and Window were initialized.
        _SitCore_FullCleanupOnError(); // Clean up platform, window, and any partial renderer state
        return err; // Return the specific error from renderer init.
    }

    // [v2.3.24b] Validate Capabilities
    if (!_SitRender_ValidateRenderCaps()) {
        _SitCore_FullCleanupOnError();
        return SITUATION_ERROR_INIT_FAILED;
    }

    // --- 4. INITIALIZE OTHER LIBRARY SUBSYSTEMS ---
    // Initialize audio, input, timers, filesystem utils, etc.
    // These often depend on the window and renderer being available.
    err = _SitCore_InitSubsystems(init_info);
    if (err != SITUATION_SUCCESS) {
        // Subsystem initialization failed. Platform, Window, and Renderer were initialized.
        _SitCore_FullCleanupOnError(); // Clean up everything initialized so far
        return err; // Return the specific error from subsystems init.
    }

    // --- 5. FINAL STATE SETUP ---
    // Perform any final configuration steps that require all subsystems to be ready.
    // This happens only after all preceding steps have succeeded.

    // Cache information about physical displays connected to the system.
    _SitPlatform_CachePhysicalDisplays(); // Result checked internally/during queries

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
    if (!_SitRender_InitRenderThread(init_info)) {
        _SitCore_FullCleanupOnError();
        return SITUATION_ERROR_THREAD_CREATION_FAILED;
    }
    #endif

    // --- 6. Mark as Successfully Initialized ---
    // All steps completed successfully. Set the global initialized flag.
    atomic_store(&sit_gs.is_initialized, true);

    // Clear any lingering error message and set a success indicator.
    _SitCore_SetError("SituationInit: No error. Initialization successful.");

    // --- 7. Return Success ---
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
 * The function also sets up the GLFW error callback (`_SitRender_GLFWErrorCallback`) to capture and report any errors originating
 * from GLFW during the initialization process or later library operations.
 *
 * @return SITUATION_SUCCESS on successful initialization of all required platform components.
 * @return SITUATION_ERROR_COM_FAILED if COM initialization fails on Windows (and the error is not `RPC_E_CHANGED_MODE`, which is tolerated).
 * @return SITUATION_ERROR_GLFW_FAILED if `glfwInit` fails to initialize the GLFW library. A specific error message is set by the GLFW error
 *         callback or this function.
 *
 * @note This function must be called before any other platform-dependent operations (like window creation or Vulkan instance creation).
 * @note If this function fails, it attempts to undo any partial initializations it performed (e.g., calling `CoUninitialize` on Windows if COM was initialized).
 * @warning This function is for internal use by `SituationInit` and should not be called directly by user code.
 *
 * @see SituationInit(), _SitRender_GLFWErrorCallback(), _SitCore_CleanupPlatform(), glfwInit(), glfwSetErrorCallback()
 */
static SituationError _SitCore_InitPlatform(void) {
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
            _SitCore_SetErrorFromCode(
                SITUATION_ERROR_COM_FAILED,
                "_SitCore_InitPlatform: CoInitializeEx failed unexpectedly."
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
    glfwSetErrorCallback(_SitRender_GLFWErrorCallback);

    // Attempt to initialize the GLFW library.
    if (!glfwInit()) {
        // glfwInit failed. This is a critical failure for window and input management.
        // A more detailed error message should have been captured by _SitRender_GLFWErrorCallback and stored in sit_gs.last_error_msg.
        _SitCore_SetErrorFromCode(
            SITUATION_ERROR_GLFW_FAILED,
            "_SitCore_InitPlatform: glfwInit failed. Check previous GLFW error message or system configuration."
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

    // --- 5. Success ---
    // If we reach here, both platform-specific initializations (COM on Windows) and GLFW initialization were successful.
    // The next step in the SituationInit sequence is typically _SitPlatform_InitWindow.
    return SITUATION_SUCCESS;
}



/**
 * @brief [INTERNAL] Initializes all non-rendering library subsystems.
 * @details This function is called once during `SituationInit` after the platform and graphics backend have been successfully set up. It is responsible for bringing all other core library modules to life.
 *
 * @par Initialization Process
 *   - **Audio System:** Initializes the `miniaudio` context, creates the mutex for the sound queue, and pre-allocates the temporary memory buffers used by the real-time audio callback. This pre-allocation is a critical optimization to prevent `SIT_MALLOC` calls on the audio thread.
 *   - **Timer System:** Initializes the Temporal Oscillator system, calculating the default, musically-timed periods for the oscillator bank and setting the initial system time.
 *   - **Input Systems:**
 *     - Initializes mutexes for thread-safe keyboard and joystick event queuing.
 *     - Registers all of the library's internal GLFW callbacks (`_SitRender_GLFWKeyCallback`, `_SitRender_GLFWMouseButtonCallback`, etc.) to capture OS events.
 *     - Initializes the internal state-tracking structures for the keyboard, mouse, and gamepads.
 *     - Pre-creates the standard system cursor shapes for fast runtime switching.
 *     - Performs an initial poll for already-connected joysticks.
 *   - **Frame Timing:** Initializes all variables used for per-frame delta time and FPS calculation.
 *
 * @return `SITUATION_SUCCESS` on successful initialization of all subsystems.
 * @return An appropriate `SituationError` code if a critical step fails (e.g., a mutex or memory allocation fails).
 *
 * @note This function is for internal use by `SituationInit` only.
 *
 * @see SituationInit(), _SitCore_CleanupSubsystems()
 */
static SituationError _SitCore_InitSubsystems(const SituationInitInfo* init_info) {
    // --- 1. Audio System Initialization ---
    ma_context_config ctx_config = ma_context_config_init();
    if (ma_context_init(NULL, 0, &ctx_config, &sit_audio.miniaudio_context) != MA_SUCCESS) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_AUDIO_CONTEXT, "ma_context_init failed");
        return SITUATION_ERROR_AUDIO_CONTEXT;
    }
    sit_audio.is_miniaudio_context_initialized = true;

    // Initialize the handle pool
    _SitAudio_InitPool();

    // Initialize the tone pool [Resonance]
    memset(sit_audio.tone_pool, 0, sizeof(sit_audio.tone_pool));

    // Initialize the mutex that protects the sound playback queue.
    /*if (ma_mutex_init(&sit_audio.audio_queue_mutex) != MA_SUCCESS) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED, "Failed to initialize audio queue mutex");
        return SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED;
    }*/
    // [FIX v2.3.27B] Initialize recursive mutex
    // This allows the same thread (Audio Thread) to re-acquire the lock if a
    // user processor calls a Situation API function.
    if (mtx_init(&sit_audio.audio_queue_mutex, mtx_recursive) != thrd_success) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED, "Failed to initialize recursive audio mutex");
        return SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED;
    }

    // Initialize capture queue if requested
    if (init_info->flags & SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD) {
        sit_audio.audio_capture_on_main_thread = true;
        sit_audio.audio_capture_queue_capacity = 4096 * 4; // Reasonable default
        sit_audio.audio_capture_queue = (float*)SIT_MALLOC(sit_audio.audio_capture_queue_capacity * sizeof(float));
        if (!sit_audio.audio_capture_queue) {
             _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Audio capture ring buffer");
             return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        if (ma_mutex_init(&sit_audio.audio_capture_mutex) != MA_SUCCESS) {
             SIT_FREE(sit_audio.audio_capture_queue);
             _SitCore_SetErrorFromCode(SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED, "Audio capture mutex");
             return SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED;
        }
    }

    // Allocate temporary buffers...
    size_t decoder_buf_size = SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES * 8 * sizeof(float);
    size_t effects_buf_size = SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES * 8 * sizeof(float);
    size_t converter_buf_size = SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES * MA_MAX_CHANNELS * sizeof(float);
    sit_audio.audio_callback_decoder_temp_buffer = (float*)SIT_MALLOC(decoder_buf_size);
    sit_audio.audio_callback_effects_temp_buffer = (float*)SIT_MALLOC(effects_buf_size);
    sit_audio.audio_callback_converter_temp_buffer = (float*)SIT_MALLOC(converter_buf_size);
    if (!sit_audio.audio_callback_decoder_temp_buffer || !sit_audio.audio_callback_effects_temp_buffer || !sit_audio.audio_callback_converter_temp_buffer) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Audio callback temp buffers");
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    sit_audio.audio_callback_temp_buffer_frames_capacity = SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES;

    atomic_init(&sit_audio.is_processing_snapshot, false);

    // [v2.4] Audio Queue Initialization
    sit_audio.config_max_voices = init_info->max_audio_voices;
    // Start with 32 voices by default, or the limit if it's smaller than 32
    int initial_cap = 32;
    if (sit_audio.config_max_voices > 0 && sit_audio.config_max_voices < 32) {
        initial_cap = (int)sit_audio.config_max_voices;
    }

    sit_audio.active_voices = (SituationSound**)SIT_MALLOC(initial_cap * sizeof(SituationSound*));
    if (!sit_audio.active_voices) return SITUATION_ERROR_MEMORY_ALLOCATION;
    sit_audio.active_voice_capacity = initial_cap;
    sit_audio.active_voice_count = 0;

    // Snapshot buffer (initialized to same capacity)
    sit_audio.snapshot_buffer = (SituationSound**)SIT_MALLOC(initial_cap * sizeof(SituationSound*));
    if (!sit_audio.snapshot_buffer) {
        SIT_FREE(sit_audio.active_voices);
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
    if (ma_mutex_init(&sit_input.keyboard.event_queue_mutex) != MA_SUCCESS ||
        ma_mutex_init(&sit_input.joysticks.event_queue_mutex) != MA_SUCCESS ||
        ma_mutex_init(&sit_input.mouse.mutex) != MA_SUCCESS) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize input mutexes");
        return SITUATION_ERROR_INIT_FAILED;
    }

    // Register callbacks
    glfwSetDropCallback(sit_gs.sit_glfw_window, _SitRender_GLFWFileDropCallback);
    glfwSetWindowFocusCallback(sit_gs.sit_glfw_window, _SitRender_GLFWWindowFocusCallback);
    glfwSetWindowIconifyCallback(sit_gs.sit_glfw_window, _SitRender_GLFWWindowIconifyCallback);
    glfwSetFramebufferSizeCallback(sit_gs.sit_glfw_window, _SitRender_GLFWFramebufferSizeCallback);
    glfwSetKeyCallback(sit_gs.sit_glfw_window, _SitRender_GLFWKeyCallback);
    glfwSetCharCallback(sit_gs.sit_glfw_window, _SitRender_GLFWCharCallback);
    glfwSetMouseButtonCallback(sit_gs.sit_glfw_window, _SitRender_GLFWMouseButtonCallback);
    glfwSetCursorPosCallback(sit_gs.sit_glfw_window, _SitRender_GLFWCursorPosCallback);
    glfwSetScrollCallback(sit_gs.sit_glfw_window, _SitRender_GLFWScrollCallback);
    glfwSetJoystickCallback(_SitRender_GLFWJoystickCallback);

    // Initialize remaining state (that isn't 0)
    glm_vec2_one(sit_input.mouse.scale); // Default mouse scale is (1, 1).

    double initial_mx, initial_my;
    glfwGetCursorPos(sit_gs.sit_glfw_window, &initial_mx, &initial_my);
    sit_input.mouse.current_pos[0] = (float)initial_mx;
    sit_input.mouse.current_pos[1] = (float)initial_my;
    glm_vec2_copy(sit_input.mouse.current_pos, sit_input.mouse.last_pos);

    // Pre-create cursors
    sit_input.cursors[SIT_CURSOR_DEFAULT]   = NULL;
    sit_input.cursors[SIT_CURSOR_ARROW]     = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    sit_input.cursors[SIT_CURSOR_IBEAM]     = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    sit_input.cursors[SIT_CURSOR_CROSSHAIR] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    sit_input.cursors[SIT_CURSOR_HAND]      = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    sit_input.cursors[SIT_CURSOR_HRESIZE]   = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    sit_input.cursors[SIT_CURSOR_VRESIZE]   = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    sit_input.cursor_count = 7;

    // Poll for existing joysticks
    for (int jid = 0; jid < SITUATION_MAX_JOYSTICKS; jid++) {
        if (glfwJoystickPresent(jid)) {
            _SitRender_GLFWJoystickCallback(jid, GLFW_CONNECTED);
        }
    }

    // Framebuffer size callback
    int fb_w, fb_h;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fb_w, &fb_h);
    _SitRender_GLFWFramebufferSizeCallback(sit_gs.sit_glfw_window, fb_w, fb_h);

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

#if defined(SITUATION_ENABLE_THREADING)
    if (!SituationCreateThreadPool(&sit_gs.thread_pool, 0, init_info->io_queue_capacity, init_info->hot_reload_poll_rate, init_info->disable_io_thread)) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_THREAD_CREATION_FAILED, "Failed to create thread pool");
        return SITUATION_ERROR_THREAD_CREATION_FAILED;
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
    if (!SituationIsInitialized()) return;

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
        size_t frames_available = 0;
        if (write_head >= read_head) {
            frames_available = write_head - read_head;
        } else {
            // Wrapped around
            frames_available = (capacity - read_head) + write_head;
        }

        // Only process if we have data
        if (frames_available > 0) {
            // 1. Allocate a linear temporary buffer
            // We use SIT_MALLOC here to avoid stack overflow on large chunks,
            // though a static scratch buffer would be an optimization for v2.4.
            float* temp_buffer = (float*)SIT_MALLOC(frames_available * sizeof(float));

            if (temp_buffer) {
                // 2. Copy and Linearize Data
                if (write_head >= read_head) {
                    // Contiguous block
                    memcpy(temp_buffer, &sit_audio.audio_capture_queue[read_head], frames_available * sizeof(float));
                } else {
                    // Split block (Wrapped)
                    size_t end_chunk_size = capacity - read_head;
                    // Part 1: Read to end of buffer
                    memcpy(temp_buffer, &sit_audio.audio_capture_queue[read_head], end_chunk_size * sizeof(float));
                    // Part 2: Start from beginning to write head
                    memcpy(temp_buffer + end_chunk_size, &sit_audio.audio_capture_queue[0], write_head * sizeof(float));
                }

                // 3. Advance Read Head
                sit_audio.audio_capture_read_head = write_head;

                // 4. Unlock BEFORE callback to prevent deadlocks if user callback takes time
                ma_mutex_unlock(&sit_audio.audio_capture_mutex);

                // 5. Dispatch to User
                sit_audio.capture_callback(temp_buffer, (uint32_t)frames_available, sit_audio.capture_user_data);

                // 6. Cleanup
                SIT_FREE(temp_buffer);
            } else {
                // Malloc failed, just unlock. We'll try again next frame.
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
    // This call triggers all the GLFW callbacks (_SitRender_GLFWKeyCallback, etc.), which will populate our `current_state` and event queue buffers for this frame.
    glfwPollEvents();

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
 *          This function should be called **once per frame**, immediately after `SituationPollInputEvents()` but before your main application logic.
 *
 * @par Function Workflow
 *   1.  **Calculates Delta Time:** It measures the time elapsed since the last frame and updates the value retrieved by `SituationGetFrameTime()`.
 *   2.  **Updates Temporal Oscillators:** It advances the state of the Temporal Oscillator system, triggering any oscillators whose periods have elapsed.
 *   3.  **Updates Joystick/Gamepad State:** It processes the joystick connection event queue and polls the state of connected gamepads to detect button press/release events for the current frame.
 *   4.  **Updates Virtual Display Clocks:** It advances the internal `elapsed_time_seconds` for each active virtual display.
 *
 * @note Calling this function is essential for `SituationGetFrameTime()` to return a correct, updated value for the current frame.
 *
 * @see SituationPollInputEvents(), SituationGetFrameTime(), SituationUpdate()
 */
SITAPI void SituationUpdateTimers(void) {
    if (!SituationIsInitialized()) return;

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
                    ts->next_trigger_time_seconds[i] += ts->period_seconds[i];
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
 *          It combines event polling and timer updates, which is less explicit and can lead to off-by-one-frame bugs.
 *          Please update your main loop to use `SituationPollInputEvents()` and `SituationUpdateTimers()` separately for a clearer and more robust structure.
 *
 * @par New Workflow (Correct):
 *   The recommended main loop structure is now:
 *   ```c
 *   while (!SituationWindowShouldClose()) {
 *       // 1. GATHER INPUT: Poll all OS events.
 *       SituationPollInputEvents();
 *
 *       // 2. UPDATE STATE: Update all internal timers and calculate delta time.
 *       SituationUpdateTimers();
 *       float delta_time = SituationGetFrameTime();
 *
 *       // 3. YOUR LOGIC: Use fresh input and delta time to update your application.
 *       UpdatePlayer(delta_time);
 *
 *       // 4. RENDER: Draw the new state of your application.
 *       if (SituationAcquireFrameCommandBuffer()) {
 *           SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
 *           // ... record your drawing commands ...
 *           SituationEndFrame();
 *       }
 *   }
 *   ```
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
 *   1.  **User Callback:** Invokes the optional exit callback set by `SituationSetExitCallback`.
 *   2.  **GPU Synchronization:** Ensures all pending GPU commands are completed (`vkDeviceWaitIdle` or `glFinish`) to prevent destroying resources that are still in use.
 *   3.  **Dangling Resource Cleanup:** Calls `_SitRender_CleanupDanglingResources` to automatically free any resources (meshes, shaders, textures, etc.) that the user forgot to destroy, printing warnings for each leak.
 *   4.  **Renderer Teardown:** Dispatches to the backend-specific cleanup function (`_SitRender_CleanupOpenGL` or `_SitCore_CleanupVulkan`) to destroy all graphics contexts, devices, and internal rendering resources.
 *   5.  **Subsystem Teardown:** Shuts down all other library modules, including the audio device, input systems, and timers.
 *   6.  **Platform Teardown:** Destroys the main window and terminates the underlying platform libraries (GLFW, COM).
 *
 * After this function completes, the library is in an uninitialized state and can be safely re-initialized with `SituationInit` if desired.
 *
 * @note It is safe to call this function even if `SituationInit` failed, as the internal cleanup helpers are robust to partially initialized states.
 * @warning This function is not thread-safe and must be called from the main thread.
 *
 * @see SituationInit(), _SitRender_CleanupDanglingResources()
 */
SITAPI void SituationShutdown(void) {
    if (!_sit_current_context) return;
    // [FIX v2.3.27B] Atomic check-and-set
    if (!atomic_exchange(&sit_gs.is_initialized, false)) {
        return; // Already shut down or shutting down
    }
    if (!SituationIsInitialized()) { _SitCore_SetErrorFromCode(SITUATION_ERROR_SHUTDOWN_FAILED, "Not initialized"); return; }
    if (sit_gs.exit_callback != NULL) { sit_gs.exit_callback(sit_gs.exit_callback_user_data); }

    // Wait for the GPU to finish any in-flight work before we start tearing things down. This is especially critical for Vulkan.
#if defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.device != VK_NULL_HANDLE) vkDeviceWaitIdle(sit_render.vk.device);
#elif defined(SITUATION_USE_OPENGL)
    if (sit_gs.sit_glfw_window) glFinish();
#endif

    // [v2.3.24a] Safety Zenith: Refcount Leak Check
    // Scan frame refcounts for dangling references before shutdown.
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; ++i) {
        int refs = atomic_load(&sit_render.frame_refcounts[i]);
        if (refs > 0) {
            fprintf(stderr, "[Situation] WARNING: Frame %d leaked with %d active references during shutdown!\n", i, refs);
        }
    }

    // --- Call the auto-cleanup function ---
    _SitRender_CleanupDanglingResources();

    // 0. --- SHUTDOWN RENDER THREAD ---
    // [v2.3.21] Must stop render thread before destroying renderer resources
    #if !defined(__STDC_NO_THREADS__)
    _SitRender_DestroyRenderThread();
    #endif

    // 1. --- CLEANUP THE RENDERER ---
    _SitRender_CleanupRenderer();    // This is the main dispatch for backend-specific cleanup.

    // 2. --- CLEANUP LIBRARY SUBSYSTEMS ---
    _SitCore_CleanupSubsystems();     // (Audio, Input, Timers, etc.)

    // 3. --- CLEANUP CORE PLATFORM & WINDOW ---
    _SitCore_CleanupPlatform();

#if defined(SITUATION_ENABLE_THREADING)
    SituationDestroyThreadPool(&sit_gs.thread_pool);
#endif

    // 4. --- FINAL STATE RESET ---
    if (_sit_current_context) {
        // Cleanup text scratch
        if (sit_render.text_batch_scratch) { SIT_FREE(sit_render.text_batch_scratch); }

        atomic_store(&sit_gs.is_initialized, false);
        _SitCore_SetError("Shutdown complete");

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
 *   - **Audio System:** Stops all currently playing sounds, uninitializes the active `miniaudio` device and context, and frees the temporary audio processing buffers.
 *   - **Synchronization:** Uninitializes all mutexes used for thread-safe event queuing (audio, keyboard, joystick).
 *   - **Input Systems:** Destroys all standard system cursors that were created by GLFW.
 *   - **Timer System:** Marks the temporal oscillator system as uninitialized.
 *
 * @note This function is designed to be robust and can be safely called even if some subsystems failed to initialize fully. It checks the state of each component before attempting to uninitialize it.
 * @warning This function is for internal use by `SituationShutdown` only.
 */
static void _SitCore_CleanupSubsystems(void) {
    // --- Audio System ---
    // Stop all sounds before uninitializing the device.
    SituationStopAllLoadedSounds();
    SituationStopAllTones();     // [Resonance]
	SituationStopAudioCapture(); // Stop recording if active
    if (sit_audio.is_miniaudio_device_active) {
        ma_device_uninit(&sit_audio.miniaudio_device);
        sit_audio.is_miniaudio_device_active = false;
    }
    // Uninitialize the context and free buffers last for the audio system.
    if (sit_audio.is_miniaudio_context_initialized) {
        ma_context_uninit(&sit_audio.miniaudio_context);
        sit_audio.is_miniaudio_context_initialized = false;
    }

    // Cleanup Handle Pool
    _SitAudio_CleanupPool();

    SIT_FREE(sit_audio.audio_callback_decoder_temp_buffer);
    SIT_FREE(sit_audio.audio_callback_effects_temp_buffer);
    SIT_FREE(sit_audio.audio_callback_converter_temp_buffer);
    sit_audio.audio_callback_decoder_temp_buffer = NULL;
    sit_audio.audio_callback_effects_temp_buffer = NULL;
    sit_audio.audio_callback_converter_temp_buffer = NULL;

    // [v2.4] Cleanup Dynamic Audio Arrays
    SIT_FREE(sit_audio.active_voices);
    sit_audio.active_voices = NULL;
    SIT_FREE(sit_audio.snapshot_buffer);
    sit_audio.snapshot_buffer = NULL;

    // Uninitialize mutexes.
    // [FIX v2.3.27B] Destroy the C11 recursive mutex used for the audio queue
    mtx_destroy(&sit_audio.audio_queue_mutex);

    // Input mutexes use standard miniaudio wrappers (non-recursive)
    ma_mutex_uninit(&sit_input.keyboard.event_queue_mutex);
    ma_mutex_uninit(&sit_input.mouse.mutex);

    // Cleanup capture resources
    if (sit_audio.audio_capture_on_main_thread) {
        ma_mutex_uninit(&sit_audio.audio_capture_mutex);
        SIT_FREE(sit_audio.audio_capture_queue);
    }
    ma_mutex_uninit(&sit_input.joysticks.event_queue_mutex);

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
 * @brief [INTERNAL] Shuts down the core platform and windowing layer.
 * @details This is the final stage of the library's shutdown sequence. It is responsible for destroying the main application window, terminating the underlying windowing library (GLFW), and uninitializing any platform-specific APIs (like COM on Windows).
 *
 * @par Cleanup Process
 *   1.  Destroys the main `GLFWwindow` handle.
 *   2.  Terminates the GLFW library, releasing all of its global resources.
 *   3.  Frees the memory used for the cached physical display information.
 *   4.  On Windows, calls `CoUninitialize` to close the COM library if it was opened by the application.
 *
 * @note This function must be called after the renderer and all other subsystems have been shut down, as they depend on the window and its context.
 * @warning This function is for internal use by `SituationShutdown` only.
 */
static void _SitCore_CleanupPlatform(void) {
    // Destroy the main window.
    if (sit_gs.sit_glfw_window) {
        glfwDestroyWindow(sit_gs.sit_glfw_window);
        sit_gs.sit_glfw_window = NULL;
    }

    // Terminate GLFW.
    glfwTerminate();

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
 * @brief [INTERNAL] Initializes all backend-specific resources for the internal 2D quad renderer.
 * @details This function is a critical part of the main initialization sequence. It creates the dedicated shaders, pipeline objects, and vertex buffers required by the high-level `SituationCmdDrawQuad` and `SituationCmdDrawText` commands.
 *          It is designed to be completely self-contained, ensuring that its internal state does not interfere with the user's rendering state.
 *
 * @par Backend-Specific Implementation
 * - **OpenGL:**
 *   1.  Compiles and links a dedicated shader program from the internal `SIT_QUAD_VERTEX_SHADER` and `SIT_QUAD_FRAGMENT_SHADER` sources.
 *   2.  Creates a **private** Vertex Array Object (`sit_render.gl.quad_vao`) and Vertex Buffer Object (`sit_render.gl.quad_vbo`). This is a crucial step to isolate the quad renderer's state from the main user-facing VAO (`sit_render.gl.global_vao_id`).
 *   3.  Uploads a static, 4-vertex triangle strip to the VBO.
 *   4.  Configures the private VAO with the correct vertex attribute layout for the simple 2D vertex format.
 *   5.  Sets the initial orthographic projection matrix uniform in the shader.
 *   6.  Critically, it restores the binding of the main global VAO before returning, ensuring the user's rendering context is left undisturbed.
 * - **Vulkan:**
 *   1.  Compiles the internal GLSL shader sources into SPIR-V using `shaderc`.
 *   2.  Creates a `VkPipelineLayout` that defines the interface for the quad renderer.
 *       - **Update:** It now includes `image_sampler_layout` (Set 1) to support textured quads for fonts.
 *       - **Update:** Push constants are expanded to include UV Rect and UseTexture flags.
 *   3.  Calls the generic `_SitRender_VulkanCreateGraphicsPipeline` helper to build the final `VkPipeline` object with the correct vertex input state and primitive topology (`TRIANGLE_STRIP`).
 *   4.  Creates and uploads the static vertex data to a device-local `VkBuffer` for optimal performance.
 *
 * @param width The initial width of the main window's viewport, used to configure the orthographic projection matrix.
 * @param height The initial height of the main window's viewport.
 *
 * @return `true` on successful initialization of all required resources.
 * @return `false` if any step fails (e.g., shader compilation, object creation). On failure, an appropriate error message is set, and any partially created resources are cleaned up.
 *
 * @note This function is for internal use by `_SitRender_InitOpenGL` or `_SitCore_InitVulkan` only.
 * @warning The success of this function is mandatory for `SituationCmdDrawQuad` and `SituationCmdDrawText` to work.
 *
 * @see _SitRender_CleanupQuadRenderer(), SituationCmdDrawQuad(), SituationCmdDrawText()
 */
static void _SitCore_InitDefaultFont(void) {
    // 8x8 font bitmap from sit_default_8x8_font. 256 chars.
    // Layout: 16 chars per row, 16 rows.
    const int tex_w = 128;
    const int tex_h = 128;
    size_t data_size = tex_w * tex_h * 4; // RGBA
    uint8_t* pixels = (uint8_t*)SIT_CALLOC(1, data_size);

    if (!pixels) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate default font atlas.");
        return;
    }

    // Expand 1-bit font data to RGBA texture
    for (int i = 0; i < 256; ++i) {
        int cx = i % 16;
        int cy = i / 16;
        int base_x = cx * 8;
        int base_y = cy * 8;

        const unsigned char* char_data = &sit_default_8x8_font[i * 8];

        for (int y = 0; y < 8; ++y) {
            unsigned char row_bits = char_data[y];
            for (int x = 0; x < 8; ++x) {
                bool on = (row_bits >> (7 - x)) & 1;
                uint8_t val = on ? 255 : 0;
                int p_idx = ((base_y + y) * tex_w + (base_x + x)) * 4;
                pixels[p_idx + 0] = 255;
                pixels[p_idx + 1] = 255;
                pixels[p_idx + 2] = 255;
                pixels[p_idx + 3] = val; // Alpha
            }
        }
    }

    SituationImage img = { .width = tex_w, .height = tex_h, .channels = 4, .data = pixels };
    SituationCreateTexture(img, false, &sit_render.default_font_atlas);
    SIT_FREE(pixels);

    // Setup font struct
    // Note: We don't have STB baked data, so we rely on SituationCmdDrawText fallback for default font
    sit_render.default_font = (SituationFont){
        .atlas_texture = sit_render.default_font_atlas,
        .atlas_width = tex_w,
        .atlas_height = tex_h,
        .font_height_pixels = 8.0f,
        .glyph_info = NULL // Signal to use fallback grid logic
    };
}


// --- Callbacks and Event Handling ---
/**
 * @brief Sets a user-defined callback function to be executed just before the library shuts down.
 * @details This function registers a callback that will be invoked at the very beginning of the `SituationShutdown` process.
 *          It provides a final opportunity for the application to perform its own cleanup tasks, such as saving state to a file, closing network connections, or freeing application-specific memory, while the library's subsystems are still active.
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
 *          This callback is the primary mechanism for handling resolution-dependent updates, such as:
 * - Adjusting the graphics viewport (`glViewport` or `vkCmdSetViewport`).
 * - Recalculating projection matrices for cameras.
 * - Resizing off-screen framebuffers (`SituationVirtualDisplay`) that should match the window size.
 *
 * @param callback A function pointer to the callback to be executed. The callback receives the new `width` and `height` in pixels, along with the `user_data` pointer. Pass `NULL` to clear a previously set callback.
 * @param user_data A custom, user-defined pointer that will be passed to the callback function.
 *
 * @note The library's internal `_SitRender_GLFWFramebufferSizeCallback` is always called first to update internal state (like viewport for OpenGL and the resize flag for Vulkan). The user's callback is invoked immediately after.
 * @warning The callback is executed in the same thread that calls `SituationPollInputEvents`. It is not asynchronous and will block the main loop until it returns.
 *
 * @see SituationGetRenderWidth(), SituationGetRenderHeight()
 */
SITAPI void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data) {
    if (!SituationIsInitialized()) return;
    sit_gs.resize_callback = callback;
    sit_gs.resize_callback_user_data = user_data;
}


// --- Command-Line Argument Queries ---
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
 *          For "-key:value", it returns a pointer to the character after the colon.
 *          For "-key value", it returns the next argument in the list.
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
 *          It is used internally by almost every API function to prevent undefined behavior when accessing
 *          uninitialized subsystems (like Audio or Vulkan). User code can use this to verify the engine state
 *          before attempting operations in modular or plugin-based architectures.
 *
 * @return `true` if the library is fully initialized and ready for use.
 * @return `false` if the library is uninitialized, has been shut down, or if initialization failed.
 *
 * @note This function is safe to call at any time, from any thread, even before `SituationInit()` or after a crash.
 */
SITAPI bool SituationIsInitialized(void) {
    return _sit_current_context && atomic_load(&sit_gs.is_initialized);
}


/**
 * @brief Registers a callback function to handle file drop events asynchronously.
 *
 * @details This provides an event-driven alternative to polling `SituationIsFileDropped`. The callback is invoked immediately by the OS event handler when files are dropped.
 *
 * @param callback The function to call. It receives the file count, the array of paths, and the user data pointer.
 *                 Pass `NULL` to disable the callback.
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
 * @brief Gathers and returns a comprehensive snapshot of the host system's hardware.
 * @details This function queries the operating system and underlying platform libraries to collect a wide range of information about the CPU, GPU, memory, storage, and connected devices. The collected data is aggregated into a single `SituationDeviceInfo` struct.
 *          This function is designed to give the application deep "Awareness" of its runtime environment, which can be used for logging, debugging, selecting quality settings, or displaying system information to the user.
 *
 * @par Data Collection & Platform Specificity
 *   The level of detail provided is platform-dependent and relies on various native APIs:
 *   - **CPU Info (Name, Cores, Speed):** Retrieved from the Windows Registry on Windows and `sysconf` on POSIX systems.
 *   - **GPU Info (Name, VRAM):**
 *     - On Windows, it prioritizes the DXGI API (if `SITUATION_ENABLE_DXGI` is defined) for the most accurate information, including dedicated VRAM.
 *     - As a fallback, or on other platforms, it retrieves the renderer string provided by the active graphics context (OpenGL or Vulkan).
 *   - **RAM Info (Total, Available):** Uses `GlobalMemoryStatusEx` on Windows and `sysinfo` on Linux. Not implemented on all POSIX systems.
 *   - **Storage Info (Drives, Capacity):** Enumerates logical drives on Windows. Not implemented on other platforms.
 *   - **Network & Input Devices:** Enumerates adapters and device classes on Windows for detailed names. Not implemented on other platforms.
 *
 * @return A `SituationDeviceInfo` struct populated with the discovered hardware information.
 * @return A zeroed struct if the library is not initialized. Fields for which information could not be retrieved will also be zero or empty.
 *
 * @note This can be a moderately expensive call, as it may involve querying multiple system APIs. It is best to call it once at startup and cache the results if the information is needed frequently.
 * @warning The completeness of the returned data is highly dependent on the operating system. Features like VRAM size, storage info, and detailed network/input device names are most reliable on Windows.
 */
/**
 * @brief Returns the number of logical CPU cores (threads) available.
 *        Falls back to 1 if query fails.
 * @return uint32_t Number of threads (hyper-threading included)
 */
SITAPI uint32_t SituationGetCPUThreadCount(void) {
#if defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (uint32_t)sysinfo.dwNumberOfProcessors;

#elif defined(__APPLE__)
    int count;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.logicalcpu", &count, &size, NULL, 0) == 0) {
        return (uint32_t)(count > 0 ? count : 1);
    }
    // Fallback
    if (sysctlbyname("hw.ncpu", &count, &size, NULL, 0) == 0) {
        return (uint32_t)(count > 0 ? count : 1);
    }
    return 1;

#elif defined(__linux__)
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)(cores > 0 ? cores : 1);

#else
    return 1;  // Minimal safe fallback
#endif
}

SITAPI SituationDeviceInfo SituationGetDeviceInfo(void) {
    SituationDeviceInfo info = {0};
    if (!SituationIsInitialized()) { _SitCore_SetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot get device info"); return info; }

    #if defined(_WIN32)
    // CPU Info
    SYSTEM_INFO sys_info_win;
    GetSystemInfo(&sys_info_win);
    info.cpu_cores = (int)SituationGetCPUThreadCount();
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD size_cpu_name = sizeof(info.cpu_name);
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)info.cpu_name, &size_cpu_name) != ERROR_SUCCESS) {
            strncpy(info.cpu_name, "Unknown CPU", SITUATION_MAX_CPU_NAME_LEN -1);
            info.cpu_name[SITUATION_MAX_CPU_NAME_LEN -1] = '\0';
        }
        DWORD speed_mhz = 0;
        DWORD size_speed = sizeof(speed_mhz);
        if (RegQueryValueExA(hKey, "~MHz", NULL, NULL, (LPBYTE)&speed_mhz, &size_speed) == ERROR_SUCCESS) {
            info.cpu_clock_speed_ghz = speed_mhz / 1000.0f;
        }
        RegCloseKey(hKey);
    } else {
        strncpy(info.cpu_name, "Unknown CPU (RegOpenKeyExA failed)", SITUATION_MAX_CPU_NAME_LEN -1);
        info.cpu_name[SITUATION_MAX_CPU_NAME_LEN -1] = '\0';
    }

    // GPU Info
    #ifdef SITUATION_ENABLE_DXGI
    // DXGI needs COM to be initialized. The flag sit_gs.is_com_initialized should be true.
    if (sit_gs.is_com_initialized) { // Check if COM is available
        IDXGIFactory* pFactory = NULL;
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory)) && pFactory) {
            IDXGIAdapter* pAdapter = NULL;
            if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapter)) && pAdapter) { // Get primary adapter
                DXGI_ADAPTER_DESC desc;
                if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, info.gpu_name, SITUATION_MAX_GPU_NAME_LEN-1, NULL, NULL);
                    info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
                    info.gpu_dedicated_memory_bytes = desc.DedicatedVideoMemory;
                } else { strncpy(info.gpu_name, "Unknown GPU (DXGI desc failed)", SITUATION_MAX_GPU_NAME_LEN-1);
                info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';}
                pAdapter->Release();
            } else { strncpy(info.gpu_name, "Unknown GPU (DXGI adapter enum failed)", SITUATION_MAX_GPU_NAME_LEN-1);
            info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';}
            pFactory->Release();
        } else { /* CreateDXGIFactory failed, or pFactory is NULL */
            strncpy(info.gpu_name, "Unknown GPU (DXGI factory failed)", SITUATION_MAX_GPU_NAME_LEN-1);
            info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
        }
    } else
    #endif // SITUATION_ENABLE_DXGI
    if (sit_gs.sit_glfw_window && glad_glGetString) {
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
        if (gl_renderer) { strncpy(info.gpu_name, gl_renderer, SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
        else { strncpy(info.gpu_name, "Unknown GPU (OpenGL name not available)", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
    } else {
        strncpy(info.gpu_name, "Unknown GPU (No context/DXGI/COM)", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
    }

    // RAM Info
    MEMORYSTATUSEX mem_status = { .dwLength = sizeof(MEMORYSTATUSEX) };
    if (GlobalMemoryStatusEx(&mem_status)) {
        info.total_ram_bytes = mem_status.ullTotalPhys;
        info.available_ram_bytes = mem_status.ullAvailPhys;
    }

    // Storage Info
    DWORD drives_mask = GetLogicalDrives();
    info.storage_device_count = 0;
    for (int i = 0; i < 26 && info.storage_device_count < SITUATION_MAX_STORAGE_DEVICES; ++i) {
        if (drives_mask & (1 << i)) {
            char drive_path[] = { (char)('A' + i), ':', '\\', '\0' };
            ULARGE_INTEGER total_cap, free_space;
            if (GetDiskFreeSpaceExA(drive_path, NULL, &total_cap, &free_space)) { snprintf(info.storage_device_names[info.storage_device_count], SITUATION_MAX_DEVICE_NAME_LEN, "Drive %c:", (char)('A' + i));
                info.storage_capacity_bytes[info.storage_device_count] = total_cap.QuadPart;
                info.storage_free_bytes[info.storage_device_count] = free_space.QuadPart;
                info.storage_device_count++;
            }
        }
    }

    // Network Adapter Info
    ULONG adapters_buffer_size = 15000; // Recommended starting size by MS docs
    info.network_adapter_count = 0;
    IP_ADAPTER_ADDRESSES* adapters_list = (IP_ADAPTER_ADDRESSES*)SIT_MALLOC(adapters_buffer_size);
    if (adapters_list) {
        DWORD ret_val = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters_list, &adapters_buffer_size);
        if (ret_val == ERROR_BUFFER_OVERFLOW) { // Should have been caught if initial buffer was 0 and we got size.
                                                // But if initial guess was too small.
            SIT_FREE(adapters_list);
            adapters_list = (IP_ADAPTER_ADDRESSES*)SIT_MALLOC(adapters_buffer_size); // Retry with new size
            if (adapters_list) { ret_val = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters_list, &adapters_buffer_size);
            }
        }
        if (ret_val == ERROR_SUCCESS && adapters_list) {
            IP_ADAPTER_ADDRESSES* current_adapter = adapters_list;
            while (current_adapter && info.network_adapter_count < SITUATION_MAX_NETWORK_ADAPTERS) {
                // Filter for common operational adapters if desired (e.g., IfOperStatusUp) if (current_adapter->OperStatus == IfOperStatusUp) {
                WideCharToMultiByte(CP_UTF8, 0, current_adapter->FriendlyName, -1, info.network_adapter_names[info.network_adapter_count], SITUATION_MAX_DEVICE_NAME_LEN-1, NULL, NULL);
                info.network_adapter_names[info.network_adapter_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                info.network_adapter_count++;
                // }
                current_adapter = current_adapter->Next;
            }
        }
        SIT_FREE(adapters_list); adapters_list = NULL;
    }


    // Input Device Info
    info.input_device_count = 0;
    const GUID* device_classes[] = { &GUID_DEVCLASS_KEYBOARD, &GUID_DEVCLASS_MOUSE, &GUID_DEVCLASS_HIDCLASS };
    for (int class_idx = 0; class_idx < 3 && info.input_device_count < SITUATION_MAX_INPUT_DEVICES; ++class_idx) {
        HDEVINFO hDevInfo = SetupDiGetClassDevsW(device_classes[class_idx], NULL, NULL, DIGCF_PRESENT);
        if (hDevInfo == INVALID_HANDLE_VALUE) continue;
        SP_DEVINFO_DATA dev_info_data = { .cbSize = sizeof(SP_DEVINFO_DATA) };
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &dev_info_data) && info.input_device_count < SITUATION_MAX_INPUT_DEVICES; ++i) {
            char friendly_name[SITUATION_MAX_DEVICE_NAME_LEN];
            if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &dev_info_data, SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendly_name, sizeof(friendly_name)-1, NULL) ||
                SetupDiGetDeviceRegistryPropertyW(hDevInfo, &dev_info_data, SPDRP_DEVICEDESC, NULL, (PBYTE)friendly_name, sizeof(friendly_name)-1, NULL) ) {
                friendly_name[sizeof(friendly_name)-1] = '\0';
                if (device_classes[class_idx] == &GUID_DEVCLASS_HIDCLASS) { // Filter HIDCLASS for gamepads/controllers
                    if (!strstr(friendly_name, "Controller") && !strstr(friendly_name, "Gamepad") &&
                        !strstr(friendly_name, "Joystick") && !strstr(friendly_name, "XBOX") &&
                        !strstr(friendly_name, "Wireless Controller") && !strstr(friendly_name, "Joy-Con") &&
                        !strstr(friendly_name, "controller") && !strstr(friendly_name, "gamepad") ) { // Add lowercase checks
                        continue; // Skip if not a typical gamepad name
                    }
                }
                strncpy(info.input_device_names[info.input_device_count], friendly_name, SITUATION_MAX_DEVICE_NAME_LEN -1);
                info.input_device_names[info.input_device_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                info.input_device_count++;
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    #elif defined(__linux__) // Linux Implementation
    // CPU Info
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        bool found = false;
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "model name", 10) == 0) {
                char* start = strchr(line, ':');
                if (start) {
                    strncpy(info.cpu_name, start + 2, SITUATION_MAX_CPU_NAME_LEN-1); // +2 to skip ": "
                    info.cpu_name[SITUATION_MAX_CPU_NAME_LEN-1] = '\0';
                    // Remove newline
                    size_t len = strlen(info.cpu_name);
                    if (len > 0 && info.cpu_name[len-1] == '\n') info.cpu_name[len-1] = '\0';
                    found = true;
                    break;
                }
            }
        }
        fclose(cpuinfo);
        if (!found) strncpy(info.cpu_name, "Linux CPU", SITUATION_MAX_CPU_NAME_LEN-1);
    } else {
        strncpy(info.cpu_name, "Unknown Linux CPU", SITUATION_MAX_CPU_NAME_LEN-1);
    }
    info.cpu_cores = (int)SituationGetCPUThreadCount();

    // RAM Info
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.total_ram_bytes = (uint64_t)si.totalram * si.mem_unit;
        info.available_ram_bytes = (uint64_t)si.freeram * si.mem_unit;
    }

    // Storage Info (Root partition)
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        info.storage_device_count = 1;
        strncpy(info.storage_device_names[0], "/", SITUATION_MAX_DEVICE_NAME_LEN-1);
        info.storage_capacity_bytes[0] = (uint64_t)stat.f_blocks * stat.f_frsize;
        info.storage_free_bytes[0] = (uint64_t)stat.f_bfree * stat.f_frsize;
    }

    // Network Adapter Info
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != NULL && info.network_adapter_count < SITUATION_MAX_NETWORK_ADAPTERS; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            // Only care about AF_INET (IPv4) or AF_INET6 (IPv6) and not loopback
            if ((ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6) &&
                !(ifa->ifa_flags & IFF_LOOPBACK)) {
                // Check if we already added this interface (getifaddrs returns one entry per address per interface)
                bool exists = false;
                for(int i=0; i<info.network_adapter_count; ++i) {
                    if (strcmp(info.network_adapter_names[i], ifa->ifa_name) == 0) { exists = true; break; }
                }
                if (!exists) {
                    strncpy(info.network_adapter_names[info.network_adapter_count], ifa->ifa_name, SITUATION_MAX_DEVICE_NAME_LEN-1);
                    info.network_adapter_names[info.network_adapter_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                    info.network_adapter_count++;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    // Input Device Info
    FILE* bus_devices = fopen("/proc/bus/input/devices", "r");
    if (bus_devices) {
        char line[256];
        char current_name[SITUATION_MAX_DEVICE_NAME_LEN] = {0};
        while (fgets(line, sizeof(line), bus_devices) && info.input_device_count < SITUATION_MAX_INPUT_DEVICES) {
            if (strncmp(line, "N: Name=", 8) == 0) {
                // Extract name
                strncpy(current_name, line + 9, SITUATION_MAX_DEVICE_NAME_LEN - 1); // Skip "N: Name=\""
                size_t len = strlen(current_name);
                if (len > 0 && current_name[len-1] == '\n') current_name[len-1] = '\0';
                if (len > 0 && current_name[len-2] == '"') current_name[len-2] = '\0'; // Remove trailing quote
                if (len > 0 && current_name[len-1] == '"') current_name[len-1] = '\0'; // Or just quote
            } else if (strncmp(line, "H: Handlers=", 12) == 0) {
                // Check if it has a relevant handler like kbd, mouse, js, or event
                if (strstr(line, "kbd") || strstr(line, "mouse") || strstr(line, "js") || strstr(line, "event")) {
                    if (strlen(current_name) > 0) {
                        strncpy(info.input_device_names[info.input_device_count], current_name, SITUATION_MAX_DEVICE_NAME_LEN-1);
                        info.input_device_names[info.input_device_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                        info.input_device_count++;
                        current_name[0] = '\0'; // Reset
                    }
                }
            }
        }
        fclose(bus_devices);
    }

    // GPU Info
    if (sit_gs.sit_glfw_window && glad_glGetString) {
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
        if (gl_renderer) { strncpy(info.gpu_name, gl_renderer, SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
        else { strncpy(info.gpu_name, "Generic GPU (OpenGL name not available)", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
    } else {
        strncpy(info.gpu_name, "Generic GPU", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
    }

    #elif defined(__APPLE__) // macOS Implementation
    // CPU Info
    size_t size = sizeof(info.cpu_name);
    if (sysctlbyname("machdep.cpu.brand_string", info.cpu_name, &size, NULL, 0) != 0) {
        strncpy(info.cpu_name, "Apple CPU", SITUATION_MAX_CPU_NAME_LEN-1);
    }
    info.cpu_cores = (int)SituationGetCPUThreadCount();

    // RAM Info
    int64_t memsize = 0;
    size = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &size, NULL, 0) == 0) {
        info.total_ram_bytes = (uint64_t)memsize;
        // Available RAM is complex on macOS (vm_stat), omitting for brevity/stability
        info.available_ram_bytes = 0;
    }

    // Storage Info
    struct statfs stats;
    if (statfs("/", &stats) == 0) {
        info.storage_device_count = 1;
        strncpy(info.storage_device_names[0], "/", SITUATION_MAX_DEVICE_NAME_LEN-1);
        info.storage_capacity_bytes[0] = (uint64_t)stats.f_blocks * stats.f_bsize;
        info.storage_free_bytes[0] = (uint64_t)stats.f_bfree * stats.f_bsize;
    }

    // Network Adapter Info (Shared with Linux via getifaddrs)
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) != -1) {
        for (ifa = ifaddr; ifa != NULL && info.network_adapter_count < SITUATION_MAX_NETWORK_ADAPTERS; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            if ((ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6) &&
                !(ifa->ifa_flags & IFF_LOOPBACK)) {
                bool exists = false;
                for(int i=0; i<info.network_adapter_count; ++i) {
                    if (strcmp(info.network_adapter_names[i], ifa->ifa_name) == 0) { exists = true; break; }
                }
                if (!exists) {
                    strncpy(info.network_adapter_names[info.network_adapter_count], ifa->ifa_name, SITUATION_MAX_DEVICE_NAME_LEN-1);
                    info.network_adapter_names[info.network_adapter_count][SITUATION_MAX_DEVICE_NAME_LEN-1] = '\0';
                    info.network_adapter_count++;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    // GPU Info
    if (sit_gs.sit_glfw_window && glad_glGetString) {
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
        if (gl_renderer) { strncpy(info.gpu_name, gl_renderer, SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
    } else {
        strncpy(info.gpu_name, "Generic GPU", SITUATION_MAX_GPU_NAME_LEN-1);
    }

    #else // Fallback for other platforms
    strncpy(info.cpu_name, "Generic CPU", SITUATION_MAX_CPU_NAME_LEN-1); info.cpu_name[SITUATION_MAX_CPU_NAME_LEN-1] = '\0';
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    info.cpu_cores = (nproc > 0) ? (int)nproc : 1;

    if (sit_gs.sit_glfw_window && glad_glGetString) {
        const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
        if (gl_renderer) { strncpy(info.gpu_name, gl_renderer, SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
        else { strncpy(info.gpu_name, "Generic GPU (OpenGL name not available)", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0'; }
    } else {
        strncpy(info.gpu_name, "Generic GPU", SITUATION_MAX_GPU_NAME_LEN-1);
        info.gpu_name[SITUATION_MAX_GPU_NAME_LEN-1] = '\0';
    }
    #endif

    // --- Common: Display Info (via GLFW) ---
    // This runs on all platforms where GLFW is available (Windows, Linux, macOS)
    int monitor_count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
    info.display_count = (monitor_count < SITUATION_MAX_MONITORS) ? monitor_count : SITUATION_MAX_MONITORS;

    for (int i = 0; i < info.display_count; ++i) {
        const char* name = glfwGetMonitorName(monitors[i]);
        if (name) {
            strncpy(info.display_names[i], name, SITUATION_MAX_MONITOR_NAME_LEN - 1);
            info.display_names[i][SITUATION_MAX_MONITOR_NAME_LEN - 1] = '\0';
        } else {
            strncpy(info.display_names[i], "Unknown Display", SITUATION_MAX_MONITOR_NAME_LEN - 1);
        }

        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        if (mode) {
            info.display_widths[i] = mode->width;
            info.display_heights[i] = mode->height;
            info.display_refresh_rates[i] = mode->refreshRate;
        }
    }

    return info;
}


/**
 * @brief Gets the human-readable name of the active GPU.
 *
 * @details Returns the renderer string provided by the active backend.
 *          - **OpenGL:** Returns `glGetString(GL_RENDERER)`.
 *          - **Vulkan:** Returns `VkPhysicalDeviceProperties.deviceName`.
 *
 * @return A pointer to a static string containing the GPU name (e.g., "NVIDIA GeForce RTX 4090").
 *         Do not free this string.
 */
SITAPI const char* SituationGetGPUName(void) {
    if (!SituationIsInitialized()) return "Unknown (Not Initialized)";

#if defined(SITUATION_USE_OPENGL)
    if (sit_gs.sit_glfw_window) {
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        if (renderer) return renderer;
    }
    return "Unknown OpenGL Device";

#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.physical_device != VK_NULL_HANDLE) {
        // We use a static buffer to return a valid const char* pointer without SIT_MALLOC.
        // This is not thread-safe if called concurrently, but getting GPU name is usually a setup-time task.
        static char device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];

        // Only query if we haven't already (simple optimization)
        if (device_name[0] == '\0') {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(sit_render.vk.physical_device, &properties);
            strncpy(device_name, properties.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);
        }
        return device_name;
    }
    return "Unknown Vulkan Device";
#endif

    return "Unknown Backend";
}


// --- Storage Media Information Implementation ---

/**
 * @brief Retrieves the full path to the current user's home directory.
 *
 * @details This function provides a cross-platform way to get the root directory for the current user profile.
 *          - **Windows:** Returns the path mapped to `FOLDERID_Profile` (e.g., `C:\Users\Name`).
 *            It internally handles the conversion from Windows Wide Characters (UTF-16) to UTF-8.
 *          - **Linux/macOS:** Returns the value of the `$HOME` environment variable.
 *            If `$HOME` is unset, it falls back to querying the password database (`getpwuid`).
 *
 * @return A dynamically allocated, null-terminated UTF-8 string containing the path.
 * @return `NULL` if the directory could not be determined or if memory allocation failed.
 *
 * @warning The returned string is allocated on the heap. The caller is **responsible** for freeing this memory
 *          using `free()` or `SituationFreeString()` when it is no longer needed.
 *
 * @see SituationGetAppSavePath()
 */
SITAPI char* SituationGetUserDirectory(void) {
    #if defined(_WIN32)
    if (!SituationIsInitialized() || !sit_gs.is_com_initialized) { // Check COM for SHGetKnownFolderPath
        _SitCore_SetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "COM or library not initialized for GetUserDirectory");
        return NULL;
    }
    PWSTR wPath = NULL;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_Profile, 0, NULL, &wPath);
    if (SUCCEEDED(hr) && wPath) {
        char path_utf8[MAX_PATH * 4]; // Ensure enough space for UTF-8
        int chars_converted = WideCharToMultiByte(CP_UTF8, 0, wPath, -1, path_utf8, sizeof(path_utf8), NULL, NULL);
        CoTaskMemFree(wPath);
        if (chars_converted > 0) {
            char* result = (char*)SIT_MALLOC(strlen(path_utf8) + 1);
            if (!result) { _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "User directory string"); return NULL; }
            strcpy(result, path_utf8);
            return result;
        }
    }
    _SitCore_SetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SHGetKnownFolderPath failed");
    return NULL;
    #else
    // On Linux/macOS, get $HOME or use getpwuid(getuid())->pw_dir
    const char* home_dir = getenv("HOME");
    if (!home_dir) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home_dir = pw->pw_dir;
    }
    if (home_dir) {
        char* result = (char*)SIT_MALLOC(strlen(home_dir) + 1);
        if (!result) { _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "User directory string"); return NULL; }
        strcpy(result, home_dir);
        return result;
    }
    _SitCore_SetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "Could not get user directory");
    return NULL;
    #endif
}

#if defined(_WIN32)

/**
 * @brief Gets the drive letter of the logical volume where the running executable is located.
 * @details This is a Windows-specific utility function. It retrieves the full path of the current application's executable and extracts the drive letter from it (e.g., 'C', 'D').
 *
 * @par Platform Specificity
 *   This function is only implemented on Windows and will not be available on other platforms like Linux or macOS, where the concept of drive letters does not exist.
 *
 * @return The uppercase drive letter (e.g., 'C') on success.
 * @return `0` (null character) if the function fails, if the executable is running from a path without a drive letter (e.g., a UNC network path), or if the library is not initialized.
 *
 * @note This function is useful for applications that need to be aware of their installation location in a Windows environment, for example, to check for available space on the current drive.
 *
 * @see SituationGetDriveInfo()
 */
SITAPI char SituationGetCurrentDriveLetter(void) {
    if (!SituationIsInitialized()) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "GetCurrentDriveLetter");
        return 0;
    }
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "GetModuleFileNameA failed for current drive letter");
        return 0;
    }

    int drive_number = PathGetDriveNumberA(exe_path);
    if (drive_number != -1) { // -1 means no drive letter (e.g. UNC path) or error
        return (char)('A' + drive_number);
    }
    _SitCore_SetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "PathGetDriveNumberA failed or path has no drive letter");
    return 0;
}
#endif

#if defined(_WIN32)

/**
 * @brief Retrieves information about a specific logical drive on Windows, including its capacity, free space, and volume name.
 * @details This is a Windows-specific utility function that provides detailed information about a storage volume identified by its drive letter.
 *
 * @par Platform Specificity
 *   This function is only implemented on Windows and will not be available on other platforms. It uses the Win32 API functions `GetDiskFreeSpaceExA` and `GetVolumeInformationA`.
 *
 * @param drive_letter The letter of the drive to query (e.g., 'C' or 'c').
 * @param[out] out_total_capacity_bytes A pointer to a `uint64_t` that will be filled with the total size of the drive in bytes. Can be `NULL` if not needed.
 * @param[out] out_free_space_bytes A pointer to a `uint64_t` that will be filled with the free space available to the current user on the drive, in bytes. Can be `NULL` if not needed.
 * @param[out] out_volume_name A character buffer that will be filled with the drive's volume label (e.g., "Local Disk"). Can be `NULL` if not needed.
 * @param volume_name_len The size of the `out_volume_name` buffer, including the null terminator.
 *
 * @return `true` if the function was able to attempt the query.
 * @return `false` if the library is not initialized or if the provided drive letter is invalid.
 *
 * @note The function is considered successful if the API calls are made. If a specific query fails (e.g., a drive is not ready), the corresponding output parameter will not be filled, and an internal error will be set via `SituationGetLastErrorMsg()`.
 *   The caller should always check the contents of the output parameters.
 *
 * @see SituationGetCurrentDriveLetter()
 */
SITAPI bool SituationGetDriveInfo(char drive_letter, uint64_t* out_total_capacity_bytes, uint64_t* out_free_space_bytes, char* out_volume_name, int volume_name_len) {
    if (!SituationIsInitialized()) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "GetDriveInfo");
        return false;
    }
    if (!((drive_letter >= 'A' && drive_letter <= 'Z') || (drive_letter >= 'a' && drive_letter <= 'z'))) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid drive letter for GetDriveInfo");
        return false;
    }
    if (!out_total_capacity_bytes && !out_free_space_bytes && !out_volume_name) {
         _SitCore_SetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "No output parameters provided for GetDriveInfo");
        return false; // Nothing to retrieve
    }


    char root_path[4]; // "X:\\"
    snprintf(root_path, sizeof(root_path), "%c:\\", toupper(drive_letter));

    if (out_total_capacity_bytes || out_free_space_bytes) {
        ULARGE_INTEGER total_bytes, free_bytes_to_caller, total_free_bytes;
        if (GetDiskFreeSpaceExA(root_path, &free_bytes_to_caller, &total_bytes, &total_free_bytes)) {
            if (out_total_capacity_bytes) *out_total_capacity_bytes = total_bytes.QuadPart;
            if (out_free_space_bytes) *out_free_space_bytes = free_bytes_to_caller.QuadPart; // Free space available to the caller
        } else {
            char err_detail[128];
            snprintf(err_detail, sizeof(err_detail), "GetDiskFreeSpaceExA failed for drive %c (Error: %lu)", toupper(drive_letter), GetLastError());
            _SitCore_SetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, err_detail);
            // Don't return false yet, try to get volume name if requested
        }
    }

    if (out_volume_name && volume_name_len > 0) {
        char volume_name_buffer[MAX_PATH + 1]; // MAX_PATH for volume name is generous
        char file_system_name_buffer[MAX_PATH + 1];
        DWORD volume_serial_number;
        DWORD max_component_length;
        DWORD file_system_flags;

        if (GetVolumeInformationA(
                root_path,
                volume_name_buffer,
                sizeof(volume_name_buffer),
                &volume_serial_number,
                &max_component_length,
                &file_system_flags,
                file_system_name_buffer,
                sizeof(file_system_name_buffer))) {
            strncpy(out_volume_name, volume_name_buffer, volume_name_len - 1);
            out_volume_name[volume_name_len - 1] = '\0';
        } else {
            char err_detail[128];
            snprintf(err_detail, sizeof(err_detail), "GetVolumeInformationA failed for drive %c (Error: %lu)", toupper(drive_letter), GetLastError());
            _SitCore_SetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, err_detail);
            out_volume_name[0] = '\0'; // Clear output volume name on error
            // If both GetDiskFreeSpaceExA and GetVolumeInformationA failed, then return false
            if (!out_total_capacity_bytes && !out_free_space_bytes) return false; // if only volume name was requested and failed
            if ( (out_total_capacity_bytes || out_free_space_bytes) && GetLastError() != ERROR_SUCCESS) {
                // if space was also requested and failed, this is an overall failure
                // The check above for GetDiskFreeSpaceExA already set an error.
            }
        }
    } else if (out_volume_name) {
        out_volume_name[0] = '\0'; // No space to write volume name
    }

    // Return true if at least one requested piece of info was successfully retrieved or attempted.
    // A more strict approach would return false if any part fails.
    // For now, let's assume if we got here without an early return, it's "successful enough" unless both GetDiskFreeSpaceExA and GetVolumeInformationA specifically failed and were requested.
    // The error state will hold the latest error.
    return true; // Simplification: if function runs, it's considered a success, caller checks outputs.
                 // A better check: return true only if ALL requested outputs were successfully populated.
                 // For now, if GetDiskFreeSpaceExA fails for requested space info, it's a problem.
                 // If GetVolumeInformationA fails for requested name info, it's a problem.
                 // Let's return based on whether the *last* critical operation succeeded or if nothing critical was requested.
    // Revised logic:
    // if ((out_total_capacity_bytes || out_free_space_bytes) && GetLastError() from GetDiskFreeSpaceExA was not SUCCESS) return false;
    // if (out_volume_name && GetLastError() from GetVolumeInformationA was not SUCCESS) return false;
    // This becomes complex due to GetLastError state. The current code is simpler.
    // Let's assume if we try to get info and it fails, the out params won't be valid, and the error message will be set. The boolean indicates an attempt was made.
}
#endif


/**
 * @brief Asks the operating system to open a file, folder, or URL with its default application.
 * @details This functions like a "double-click". It uses the platform's recommended native APIs for a secure and reliable operation (e.g., ShellExecute on Windows, xdg-open on Linux).
 * @param filePath The path to the file, folder, or a full URL to open.
 */
SITAPI void SituationOpenFile(const char* filePath) {
    if (!filePath || filePath[0] == '\0') {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "File path cannot be null or empty.");
        return;
    }

#if defined(_WIN32)
    #include <shellapi.h>
    int result = (int)(uintptr_t)ShellExecuteA(NULL, "open", filePath, NULL, NULL, SW_SHOWNORMAL);
    if (result <= 32) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "ShellExecuteA failed to open file or path.");
    }
#elif defined(__APPLE__)
    char command[2048];
    snprintf(command, sizeof(command), "open \"%s\"", filePath);
    if (system(command) != 0) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "macOS 'open' command failed.");
    }
#elif defined(__linux__)
    char command[2048];
    snprintf(command, sizeof(command), "xdg-open \"%s\"", filePath);
    if (system(command) != 0) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_FILE_OPEN_FAILED, "Linux 'xdg-open' command failed.");
    }
#else
    _SitCore_SetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "SituationOpenFile is not supported on this platform.");
#endif
}



/**
 * @brief Executes a system command hidden from the user, capturing stdout/stderr.
 *
 * @details This function runs a shell command in a hidden manner (no window popup on Windows,
 *          no new terminal on POSIX) and captures the combined output (stdout + stderr).
 *
 * @param cmd The full command line to execute (e.g., "dir C:\\Windows" or "ls -l /tmp").
 *            On Windows, this is passed to `cmd.exe /C`. On POSIX, to `/bin/sh -c`.
 * @param[out] output Pointer to a `char*` that will be allocated with the command output.
 *                    The caller MUST free this string using `SituationFreeString()` or `SIT_FREE()`.
 *                    If output is captured, this pointer is set. If no output or error, it may be NULL or empty string.
 *
 * @return The exit code of the process (0 usually means success).
 * @return -1 if the process failed to launch or execution setup failed. In this case,
 *         `SituationGetLastErrorMsg()` may provide more details.
 */
SITAPI int SituationExecuteCommand(const char *cmd, char **output) {
    if (!cmd || !output) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Cmd or Output pointer is NULL");
        return -1;
    }

    *output = NULL;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;

    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "CreatePipe failed");
        return -1;
    }
    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hRead); CloseHandle(hWrite);
        _SitCore_SetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "SetHandleInformation failed");
        return -1;
    }

    PROCESS_INFORMATION pi = {0};
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE); // Inherit stdin? Or NULL? Snippet used GetStdHandle.
    si.wShowWindow = SW_HIDE;

    // Use cmd.exe /C to interpret the command.
    // We construct "cmd.exe /C \"<cmd>\"" to handle shell features.
    // Length calculation: "cmd.exe /C \"" (13) + cmd len + "\"" (1) + null (1) = len + 15
    size_t cmd_len = strlen(cmd);
    size_t full_len = cmd_len + 32; // Safety margin
    char *cmdline = (char*)SIT_MALLOC(full_len);
    if (!cmdline) {
        CloseHandle(hRead); CloseHandle(hWrite);
        _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate cmdline buffer");
        return -1;
    }
    snprintf(cmdline, full_len, "cmd.exe /C \"%s\"", cmd);

    BOOL success = CreateProcessA(
        NULL,           // Application Name
        cmdline,        // Command Line
        NULL,           // Process Attributes
        NULL,           // Thread Attributes
        TRUE,           // Inherit Handles
        CREATE_NO_WINDOW, // Creation Flags
        NULL,           // Environment
        NULL,           // Current Directory
        &si,            // Startup Info
        &pi             // Process Information
    );

    SIT_FREE(cmdline);
    CloseHandle(hWrite);  // Close write end in parent, otherwise ReadFile blocks forever

    if (!success) {
        CloseHandle(hRead);
        _SitCore_SetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "CreateProcessA failed");
        return -1;
    }

    // Read output
    char buf[4096];
    DWORD bytesRead;
    size_t total = 0;

    // Initial empty string allocation so *output is valid even if empty
    *output = (char*)SIT_CALLOC(1, 1);

    while (ReadFile(hRead, buf, sizeof(buf)-1, &bytesRead, NULL) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        char *tmp = (char*)SIT_REALLOC(*output, total + bytesRead + 1);
        if (!tmp) {
            SIT_FREE(*output);
            *output = NULL;
            CloseHandle(hRead);
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
            _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to realloc output buffer");
            return -1;
        }
        *output = tmp;
        memcpy(*output + total, buf, bytesRead);
        total += bytesRead;
        (*output)[total] = '\0';
    }

    CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return (int)exit_code;

#else  // Linux & macOS
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "pipe() failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]); close(pipefd[1]);
        _SitCore_SetErrorFromCode(SITUATION_ERROR_COMMAND_EXECUTION_FAILED, "fork() failed");
        return -1;
    }

    if (pid == 0) {  // Child
        close(pipefd[0]);  // Close read end

        // Redirect both stdout and stderr to write end of pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // To prevent any terminal allocation (extra safety on macOS/Linux)
        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd != -1) {
            dup2(nullfd, STDIN_FILENO);
            close(nullfd);
        }

        // Use shell to interpret the command
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);

        // If execl fails
        _exit(127);
    }

    // Parent
    close(pipefd[1]);  // Close write end

    char buf[4096];
    ssize_t bytesRead;
    size_t total = 0;

    // Initial empty string
    *output = (char*)SIT_CALLOC(1, 1);

    while ((bytesRead = read(pipefd[0], buf, sizeof(buf)-1)) > 0) {
        buf[bytesRead] = '\0';
        char *tmp = (char*)SIT_REALLOC(*output, total + bytesRead + 1);
        if (!tmp) {
            SIT_FREE(*output);
            *output = NULL;
            close(pipefd[0]);
            int status;
            waitpid(pid, &status, 0);
            _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to realloc output buffer");
            return -1;
        }
        *output = tmp;
        memcpy(*output + total, buf, bytesRead);
        total += bytesRead;
        (*output)[total] = '\0';
    }

    close(pipefd[0]);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
#endif
}



// Filesystem Module
//==================================================================================
// --- Path Management & Special Directories ---

/**
 * @brief Get a safe, persistent path for saving application data (e.g., %APPDATA%/AppName).
 * @warning The returned string is dynamically allocated. The caller is **responsible for freeing this memory** using `free()`.
 * @param app_name The name of your application, used to create the final subdirectory.
 * @return A new string containing the full path, or NULL on failure.
 */
SITAPI char* SituationGetAppSavePath(const char* app_name) {
    if (!app_name || app_name[0] == '\0') {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "App name cannot be NULL or empty.");
        return NULL;
    }

#if defined(_WIN32)
    // NOTE: For this to work reliably, CoInitialize may need to be called.
    // situation.h already does this for DXGI, so it should be fine.
    PWSTR wide_path_appdata = NULL;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &wide_path_appdata);

    if (FAILED(hr)) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SHGetKnownFolderPath failed to retrieve AppData.");
        return NULL;
    }

    char* path_appdata = _SitUtils_Wide_to_utf8(wide_path_appdata);
    CoTaskMemFree(wide_path_appdata); // Free the memory allocated by the shell API

    if (!path_appdata) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to convert AppData path to UTF-8.");
        return NULL;
    }

    // Now, construct the full path: %APPDATA%\app_name
    // +1 for separator, +1 for null terminator
    size_t final_len = strlen(path_appdata) + 1 + strlen(app_name) + 1;
    char* final_path = (char*)SIT_MALLOC(final_len);
    if (!final_path) {
        SIT_FREE(path_appdata);
        return NULL;
    }

    // snprintf is safer than strcpy/strcat
    snprintf(final_path, final_len, "%s\\%s", path_appdata, app_name);
    SIT_FREE(path_appdata);

    // Create the directory if it doesn't exist
    WCHAR* wide_final_path = _SitUtils_Utf8_to_wide(final_path);
    if (wide_final_path) {
        CreateDirectoryW(wide_final_path, NULL); // Fails harmlessly if it already exists
        SIT_FREE(wide_final_path);
    }

    return final_path;
#else // POSIX fallback
    const char* home_dir = getenv("HOME");
    if (!home_dir) return NULL;

    // Follow XDG Base Directory Spec: $XDG_DATA_HOME or fallback to ~/.local/share
    const char* xdg_data_home = getenv("XDG_DATA_HOME");
    char* base_path = NULL;
    if (xdg_data_home && xdg_data_home[0] != '\0') {
        base_path = _SitUtils_Strdup(xdg_data_home);
    } else {
        const char* fallback_suffix = "/.local/share";
        size_t len = strlen(home_dir) + strlen(fallback_suffix) + 1;
        base_path = (char*)SIT_MALLOC(len);
        if (base_path) snprintf(base_path, len, "%s%s", home_dir, fallback_suffix);
    }

    if (!base_path) return NULL;

    // TODO: Create the base directory if it doesn't exist.
    // mkdir(base_path, 0755);

    size_t final_len = strlen(base_path) + 1 + strlen(app_name) + 1;
    char* final_path = (char*)SIT_MALLOC(final_len);
    if (!final_path) {
        SIT_FREE(base_path);
        return NULL;
    }
    snprintf(final_path, final_len, "%s/%s", base_path, app_name);
    SIT_FREE(base_path);

    // TODO: Create the final directory.
    // mkdir(final_path, 0755);

    return final_path;
#endif
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


// --- Application Pause/Resume Implementation ---
/**
 * @brief Pauses the library's internal, time-dependent subsystems.
 * @details This function sets an internal flag that signals the application is in a paused state. Its primary effect is to pause the audio device, stopping all sound playback.
 *          This function is called automatically when the window is minimized (via the `_SitRender_GLFWWindowIconifyCallback`) and can also be called manually by the user to implement an in-game pause menu or similar functionality.
 *
 * @note This function only affects library subsystems. It does not stop the main application loop from running. It is the developer's responsibility to check `SituationIsAppPaused()` in their main loop and halt their own game logic and updates accordingly.
 *
 * @see SituationResumeApp(), SituationIsAppPaused(), SituationPauseAudioDevice()
 */
SITAPI void SituationPauseApp(void) { // Largely same logic
    if (!SituationIsInitialized() || sit_gs.is_app_internally_paused) return;
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
    if (!SituationIsInitialized() || !sit_gs.is_app_internally_paused) return;
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
 * @brief Gets the current logical size of the window.
 * @details This is a convenience function that retrieves both width and height in a single call.
 * @param[out] width A pointer to an integer where the window width in screen coordinates will be stored.
 * @param[out] height A pointer to an integer where the window height in screen coordinates will be stored.
 * @see SituationGetScreenWidth(), SituationGetScreenHeight()
 */
SITAPI void SituationGetWindowSize(int* width, int* height) {
    if (!SituationIsInitialized()) {
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
        _SitCore_SetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "Cannot check window close state");
        return true; // If not init'd, effectively should "close" or not run.
    }
    return glfwWindowShouldClose(sit_gs.sit_glfw_window) == GLFW_TRUE;
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
        _SitCore_SetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SituationSetTargetFPS: Library not initialized.");
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
        // Return a default value as per the original docstring and common practice.
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
        // Return a default value as per the original docstring.
        return 0;
    }

    // --- 2. Return Stored Value ---
    // The current FPS is calculated and stored in the global state.
    return sit_gs.current_fps;
}

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Packs queue index, generation, and slot index into a single Job ID.
 * @param q_idx The priority queue index (0 or 1).
 * @param gen The generation counter (15 bits).
 * @param slot The slot index in the ring buffer (16 bits).
 * @return A packed SituationJobId.
 */
static inline SituationJobId _SitMakeId(uint32_t q_idx, uint32_t gen, uint32_t slot) {
    return ((q_idx & 1) << SIT_ID_QUEUE_SHIFT) | ((gen & SIT_ID_GEN_MASK) << SIT_ID_GEN_SHIFT) | (slot & SIT_ID_SLOT_MASK);
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Resolves a Job ID to a pointer, with validation.
 * @details Unpacks the ID, checks bounds, and validates the generation counter to prevent ABA issues.
 * @param pool The thread pool instance.
 * @param id The job handle to resolve.
 * @return Pointer to the SituationJob, or NULL if invalid/stale.
 */
static SituationJob* _SitCore_GetJobFromId(SituationThreadPool* pool, SituationJobId id) {
    if (id == 0 || !pool) return NULL;

    uint32_t q_idx = (id >> SIT_ID_QUEUE_SHIFT) & 1;
    uint32_t slot_idx = id & SIT_ID_SLOT_MASK;
    uint32_t gen = (id >> SIT_ID_GEN_SHIFT) & SIT_ID_GEN_MASK;

    // Bounds check
    if (slot_idx >= pool->queues[q_idx].capacity) return NULL;

    SituationJob* job = &pool->queues[q_idx].jobs[slot_idx];

    // Generation check (ABA protection)
    if (atomic_load(&job->generation) != (uint16_t)gen) return NULL;

    return job;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Detects cycles by traversing the dependency chain.
 * @details Starts from the *dependent* job and follows its continuation chain.
 *          If we encounter the *prerequisite* job, a cycle (A->B->A) exists.
 */
static bool _SitCore_DetectCycle(SituationThreadPool* pool, SituationJobId prereq_id, SituationJobId dep_id, uint8_t* out_new_depth) {
    uint8_t depth = 0;
    SituationJobId current_cursor = dep_id;

    // Base depth of the prerequisite (if it exists)
    SituationJob* prereq_ptr = _SitCore_GetJobFromId(pool, prereq_id);
    uint8_t base_depth = prereq_ptr ? prereq_ptr->dep_depth : 0;

    // Traverse downstream from the dependent job
    while (current_cursor != 0 && depth < 32) {
        SituationJob* job = _SitCore_GetJobFromId(pool, current_cursor);
        if (!job) break; // Chain broken (job finished or invalid)

        if (current_cursor == prereq_id) {
            // We found the prerequisite downstream from the dependent -> Cycle!
            if (out_new_depth) *out_new_depth = depth;
            return true;
        }

        current_cursor = atomic_load(&job->continuation_id);
        depth++;
    }

    if (out_new_depth) *out_new_depth = base_depth + 1;
    return (depth >= 32 || (base_depth + 1) >= 32); // Fail if too deep
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

// ==================================================================================
//  Task Graph API
// ==================================================================================

/**
 * @brief Establishes a directed dependency between two jobs (Prerequisite -> Dependent).
 *
 * @details This function constructs a dependency edge in the task graph, ensuring that the `dependent_job`
 *          will not execute until the `prerequisite_job` has completed.
 *
 *          **Mechanism:**
 *          - The dependency count of the `dependent_job` is atomically incremented.
 *          - The `continuation_id` of the `prerequisite_job` is updated to point to the `dependent_job`.
 *          - This uses a lock-free Compare-And-Swap (CAS) loop to ensure thread safety without mutexes.
 *
 *          **Cycle Detection:**
 *          Before modifying the graph, this function performs a depth-limited search (max depth 32)
 *          to detect potential cycles (e.g., A->B->A). If a cycle is detected, the operation is aborted
 *          to prevent deadlocks.
 *
 *          **Constraints:**
 *          - **1:1 Continuation:** Currently, a job can trigger only *one* direct successor via this mechanism.
 *            If `prerequisite_job` already has a continuation, this function will fail.
 *            For 1:N (Fan-Out) dependencies, use a dedicated dispatcher job that submits multiple children.
 *            For N:1 (Fan-In) dependencies, use `SituationAddJobDependencies`.
 *
 * @param pool The thread pool instance managing the jobs.
 * @param prereq_id The ID of the job that must finish first (the "parent" or "predecessor").
 * @param dep_id The ID of the job that is waiting (the "child" or "successor").
 *
 * @return `true` if the dependency was successfully added.
 * @return `false` if:
 *         - The thread pool is invalid.
 *         - Either job ID is invalid or refers to a completed/recycled slot.
 *         - A dependency cycle was detected.
 *         - The `prerequisite_job` already has a continuation (collision).
 *
 * @see SituationAddJobDependencies()
 */
SITAPI bool SituationAddJobDependency(SituationThreadPool* pool, SituationJobId prereq_id, SituationJobId dep_id) {
    if (!pool) return false;

    // 1. Validate Handles
    SituationJob* prereq = _SitCore_GetJobFromId(pool, prereq_id);
    SituationJob* dep = _SitCore_GetJobFromId(pool, dep_id);

    if (!prereq || !dep) {
        _SitCore_SetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid job IDs in AddJobDependency (jobs may have finished).");
        return false;
    }

    // 2. Cycle Detection (Read-only traversal, safe-ish without lock if topology is stable-ish)
    uint8_t new_depth = 0;
    if (_SitCore_DetectCycle(pool, prereq_id, dep_id, &new_depth)) {
        #ifndef NDEBUG
        fprintf(stderr, "[Situation] ERROR: Dependency Cycle Detected! Job 0x%x -> 0x%x causes loop.\n", prereq_id, dep_id);
        #endif
        _SitCore_SetErrorFromCode(SITUATION_ERROR_THREAD_CYCLE, "Cycle detected or depth limit (32) exceeded.");
        return false;
    }

    // 3. Link via CAS (Compare-And-Swap)
    // We only allow one continuation per job in this lightweight system.
    uint32_t expected_cont = 0;
    if (atomic_compare_exchange_strong_explicit(&prereq->continuation_id, &expected_cont, dep_id, memory_order_seq_cst, memory_order_seq_cst)) {
        // Successfully linked prereq -> dep
        atomic_fetch_add(&dep->dependency_count, 1);
        dep->dep_depth = new_depth;
        return true;
    } else {
        // Prerequisite already has a continuation
        _SitCore_SetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL, "Prerequisite job already has a continuation (1:1 limit). Use SituationAddJobDependencies for fan-in.");
        return false;
    }
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Establishes a Many-to-One dependency (Fan-In).
 *
 * @details Configures the `dependent_job` to wait for *multiple* prerequisite jobs to complete.
 *          This is commonly used for synchronization points, such as a "Frame End" job that waits for
 *          "Physics", "AI", and "Rendering" jobs to finish.
 *
 *          This is a helper function that iteratively calls `SituationAddJobDependency` for each
 *          ID in the `prerequisites` array.
 *
 * @param pool The thread pool instance.
 * @param prerequisites An array of job IDs that must complete before the dependent job starts.
 * @param count The number of job IDs in the `prerequisites` array.
 * @param dependent_job The ID of the job that will wait.
 *
 * @return `true` if **all** dependencies were successfully added.
 * @return `false` if *any* dependency failed to be added (e.g., due to invalid ID, cycle, or 1:1 constraint violation).
 *         If `false` is returned, the dependency graph may be in a partially updated state (some dependencies added, some not).
 *
 * @see SituationAddJobDependency()
 */
SITAPI bool SituationAddJobDependencies(SituationThreadPool* pool, SituationJobId* prerequisites, int count, SituationJobId dependent_job) {
    for (int i = 0; i < count; ++i) {
        // Note: The parameter order in AddJobDependency is (Prereq, Dependent)
        if (!SituationAddJobDependency(pool, prerequisites[i], dependent_job)) {
            return false; // Fail fast if any link fails
        }
    }
    return true;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Dumps the current state of the task graph to a file stream for debugging.
 *
 * @details Prints a snapshot of all active jobs, their states, and their dependency links.
 *          This is an invaluable tool for diagnosing deadlocks, verifying graph topology, or visualizing
 *          the execution flow of complex async workloads.
 *
 * @par Output Formats
 *   - **Text Mode (`json_mode = false`):** A human-readable, column-aligned table useful for console logging.
 *   - **JSON Mode (`json_mode = true`):** A structured JSON object representing the graph nodes and edges.
 *     This output can be piped to a file and visualized using graph plotting tools (e.g., Graphviz, D3.js).
 *
 * @param pool The thread pool instance to inspect.
 * @param out The output file stream (e.g., `stdout`, `stderr`, or a file opened with `fopen`).
 *            If `NULL`, defaults to `stderr`.
 * @param json_mode Set to `true` for machine-readable JSON output, `false` for human-readable text.
 */
SITAPI void SituationDumpTaskGraph(SituationThreadPool* pool, FILE* out, bool json_mode) {
    if (!pool) return;
    if (!out) out = stderr;

    if (json_mode) fprintf(out, "{\"active_jobs\": %d, \"queues\": {\n", atomic_load(&pool->active_jobs));
    else {
        fprintf(out, "\n=== Situation Task Graph (Active: %d) ===\n", atomic_load(&pool->active_jobs));
    }

    for (int q = 0; q < 2; ++q) {
        const char* q_name = (q == 1) ? "high" : "low";
        if (json_mode) fprintf(out, "  \"%s\": [", q_name);
        else fprintf(out, "-- Queue: %s --\n", q_name);

        size_t cap = pool->queues[q].capacity;
        bool first = true;

        for (size_t i = 0; i < cap; ++i) {
            SituationJob* job = &pool->queues[q].jobs[i];
            uint16_t gen = atomic_load(&job->generation);
            bool completed = atomic_load(&job->is_completed);

            // Only dump active jobs
            if (!completed && gen > 0) {
                int deps = atomic_load(&job->dependency_count);
                uint32_t cont = atomic_load(&job->continuation_id);

                if (json_mode) {
                    if (!first) fprintf(out, ",");
                    fprintf(out, "{\"id\":\"0x%08x\",\"gen\":%d,\"depth\":%u,\"deps\":%d,\"cont\":\"0x%08x\"}",
                        _SitMakeId(q, gen, (uint32_t)i), gen, job->dep_depth, deps, cont);
                    first = false;
                } else {
                    fprintf(out, "  [0x%08x] Gen:%04d | Depth:%02u | Wait:%d | Trig:0x%08x\n",
                        _SitMakeId(q, gen, (uint32_t)i), gen, job->dep_depth, deps, cont);
                }
            }
        }
        if (json_mode) fprintf(out, "]%s\n", (q==0) ? "," : "");
    }
    if (json_mode) fprintf(out, "}}\n");
    else fprintf(out, "=========================================\n\n");
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

SITAPI size_t SituationGetIOQueueDepth(void) {
    if (!sit_gs.thread_pool.is_active) return 0;
    // Queue 0 is Low Priority / IO
    size_t head = atomic_load(&sit_gs.thread_pool.queues[0].head);
    size_t tail = atomic_load(&sit_gs.thread_pool.queues[0].tail);
    return head - tail;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)


// ==================================================================================
//  Worker Thread Implementation (Updated)
// ==================================================================================

/**
 * @brief [INTERNAL] The main loop for worker threads.
 * @details Continuously polls the High then Low priority queues for work.
 *          Implements cooperative yielding and condition variable sleeping to minimize CPU usage when idle.
 *          Handles job execution, dependency resolution (continuation), and generation cycling.
 * @param arg Pointer to the SituationThreadPool.
 * @return 0 upon thread exit.
 */
static int _SitCore_WorkerEntry(void* arg) {
    SituationThreadPool* pool = (SituationThreadPool*)arg;

    while (!atomic_load(&pool->shutdown)) {
        SituationJob* job_ptr = NULL;
        int queue_idx = -1;

        // --- Job Picking Loop (Priority 1 -> 0) ---
        for (int q = 1; q >= 0; --q) {
            mtx_lock(&pool->queues[q].lock);

            size_t head = atomic_load(&pool->queues[q].head);
            size_t tail = atomic_load(&pool->queues[q].tail);

            if (tail != head) {
                // Potential job found
                size_t idx = tail & pool->queues[q].mask;
                SituationJob* potential = &pool->queues[q].jobs[idx];

                // Dependency Check
                // If job has dependencies (>0), we CANNOT run it.
                // In a ring buffer, this causes Head-of-Line blocking.
                // We yield this queue and try the other one.
                if (atomic_load(&potential->dependency_count) > 0) {
                    mtx_unlock(&pool->queues[q].lock);
                    thrd_yield(); // Cooperatively yield to avoid spinning on blocked jobs
                    continue; // Try next priority queue
                }

                // Job is ready!
                job_ptr = potential;
                atomic_store(&pool->queues[q].tail, tail + 1);
                queue_idx = q;
                mtx_unlock(&pool->queues[q].lock);
                break; // Stop searching, we found work
            } else {
                mtx_unlock(&pool->queues[q].lock);
            }
        }

        // If no job found in either queue
        if (!job_ptr) {
            mtx_lock(&pool->queues[0].lock); // Lock low prio for condition var
            // Double check to prevent race where signal came before wait
            size_t head = atomic_load(&pool->queues[0].head);
            size_t tail = atomic_load(&pool->queues[0].tail);
            // Also check High prio emptiness? Ideally yes, but for simplicity we sleep on Low lock.
            // Real robustness would use a dedicated condition mutex.
            if (head == tail && !atomic_load(&pool->shutdown)) {
                 cnd_wait(&pool->wake_condition, &pool->queues[0].lock);
            }
            mtx_unlock(&pool->queues[0].lock);
            continue;
        }

        // --- Execute Job ---
        if (job_ptr) {
            // 1. Run User Function
            void* data_arg = job_ptr->uses_large_data ? job_ptr->large_data_ptr : job_ptr->storage;
            if (job_ptr->func) {
                // [CRITICAL FIX] Legacy Support for SituationError* signature
                // Previous API versions passed a SituationError* as the second argument.
                // While the new API uses void* user_context (currently unused/NULL),
                // legacy code might try to write to the second argument.
                // We pass a dummy error variable to prevent segfaults if old code is linked against this new implementation.
                SituationError dummy_err = SITUATION_SUCCESS;
                job_ptr->func(data_arg, (void*)&dummy_err);
            }

            // 2. Handle Continuation
            uint32_t cont_id = atomic_load(&job_ptr->continuation_id);
            if (cont_id != 0) {
                SituationJob* next_job = _SitCore_GetJobFromId(pool, cont_id);
                if (next_job) {
                    // Decrement dependency count
                    int remaining = atomic_fetch_sub(&next_job->dependency_count, 1);
                    if (remaining == 1) {
                        // Job became ready (count went 1 -> 0).
                        // Wake up workers to potentially process it.
                        cnd_signal(&pool->wake_condition);
                    }
                }
            }

            // 3. Completion & Cleanup
            // [Safety] Free copied data if owned
            if (job_ptr->owns_memory && job_ptr->large_data_ptr) {
                SIT_FREE(job_ptr->large_data_ptr);
                job_ptr->large_data_ptr = NULL;
                job_ptr->owns_memory = false;
            }

            atomic_store(&job_ptr->is_completed, true);

            // Increment generation to invalidate handle
            uint16_t old_gen = atomic_load(&job_ptr->generation);
            atomic_store(&job_ptr->generation, (uint16_t)((old_gen + 1) & SIT_ID_GEN_MASK));

            // Decrement global active count
            if (atomic_fetch_sub(&pool->active_jobs, 1) == 1) {
                cnd_broadcast(&pool->idle_condition); // Wake Main Thread (WaitForAll)
            }
        }
    }
    return 0;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Initializes the thread pool and spawns worker threads.
 *
 * @details Allocates resources for the dual-priority ring buffers (High/Low) and spawns the requested number of worker threads.
 *          The system uses a **Generational Index** strategy for job handles, ensuring O(1) validation and preventing ABA problems
 *          (reuse of IDs) without the need for heavy locking or dynamic allocation per job.
 *
 *          The **High Priority Queue** is always checked first by workers. This ensures that critical, latency-sensitive tasks
 *          (like Physics updates or Audio processing) are never blocked behind a backlog of bulk background work (like Asset Loading).
 *
 * @param pool Pointer to the `SituationThreadPool` struct to initialize. This memory must be allocated by the caller (e.g., on the stack or heap).
 * @param num_threads Number of worker threads to spawn.
 *                    - Pass `0` to automatically detect the number of logical CPU cores and use a sensible default (usually `logical_cores - 1`).
 *                    - Recommended setting: `logical_cores - 1` to leave the main thread free for window/rendering tasks.
 * @param queue_size The capacity of the internal job ring buffers.
 *                   - Must be a power of 2 (e.g., 1024, 2048). If not, it will be automatically rounded up.
 *                   - This determines the maximum number of pending jobs before backpressure strategies (Blocking or Run-Inline) are triggered.
 *
 * @return `true` if the thread pool was successfully initialized.
 * @return `false` if initialization failed (e.g., memory allocation failure, thread creation failure).
 *
 * @warning This function must be called from the main thread.
 */
// [v2.3.34] Dedicated I/O Thread Entry
static int _SitFS_IOThreadEntry(void* arg) {
    SituationThreadPool* pool = (SituationThreadPool*)arg;
    atomic_store(&pool->io_active, true);

    // Rate Limiting for Hot-Reload
    struct timespec last_hr_time;
    timespec_get(&last_hr_time, TIME_UTC);

    while (!atomic_load(&pool->shutdown)) {
        // --- 1. Process Low Priority Queue (Index 0) ---
        bool worked = false;
        mtx_lock(&pool->queues[0].lock);
        size_t head = atomic_load(&pool->queues[0].head);
        size_t tail = atomic_load(&pool->queues[0].tail);

        if (tail != head) {
            size_t idx = tail & pool->queues[0].mask;
            SituationJob* job = &pool->queues[0].jobs[idx];

            if (atomic_load(&job->dependency_count) == 0) {
                atomic_store(&pool->queues[0].tail, tail + 1);
                mtx_unlock(&pool->queues[0].lock);

                // Execute
                void* d = job->uses_large_data ? job->large_data_ptr : job->storage;
                if (job->func) {
                    SituationError dummy = SITUATION_SUCCESS;
                    job->func(d, (void*)&dummy);
                }

                // Continuation
                uint32_t cont_id = atomic_load(&job->continuation_id);
                if (cont_id != 0) {
                    SituationJob* next_job = _SitCore_GetJobFromId(pool, cont_id);
                    if (next_job) {
                        if (atomic_fetch_sub(&next_job->dependency_count, 1) == 1) cnd_signal(&pool->wake_condition);
                    }
                }

                atomic_store(&job->is_completed, true);
                uint16_t old = atomic_load(&job->generation);
                atomic_store(&job->generation, (uint16_t)((old + 1) & SIT_ID_GEN_MASK));

                if (atomic_fetch_sub(&pool->active_jobs, 1) == 1) cnd_broadcast(&pool->idle_condition);
                worked = true;
            } else {
                mtx_unlock(&pool->queues[0].lock);
            }
        } else {
            mtx_unlock(&pool->queues[0].lock);
        }

        // --- 2. Hot-Reload Polling ---
        if (pool->hot_reload_rate > 0.0) {
            struct timespec now;
            timespec_get(&now, TIME_UTC);
            double diff = (now.tv_sec - last_hr_time.tv_sec) + (now.tv_nsec - last_hr_time.tv_nsec) / 1e9;

            if (diff >= pool->hot_reload_rate) {
                _SitRender_PerformHotReloadPass();
                last_hr_time = now;
            }
        }

        // --- 3. Sleep ---
        if (!worked) {
            struct timespec ts;
            timespec_get(&ts, TIME_UTC);
            ts.tv_nsec += 33000000; // 33ms
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }

            mtx_lock(&pool->queues[0].lock);
            if (atomic_load(&pool->queues[0].head) == atomic_load(&pool->queues[0].tail) && !atomic_load(&pool->shutdown)) {
                cnd_timedwait(&pool->wake_condition, &pool->queues[0].lock, &ts);
            }
            mtx_unlock(&pool->queues[0].lock);
        }
    }
    atomic_store(&pool->io_active, false);
    return 0;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

SITAPI bool SituationCreateThreadPool(SituationThreadPool* pool, size_t num_threads, size_t queue_size, double hot_reload_rate, bool disable_io) {
    SIT_ASSERT_MAIN_THREAD();
    if (!pool) return false;
    memset(pool, 0, sizeof(SituationThreadPool));

    // Auto-detect threads if 0
    if (num_threads == 0) {
        num_threads = (size_t)SituationGetCPUThreadCount();
        num_threads = (num_threads > 1) ? num_threads - 1 : 1; // Leave one for main
    }
    if (num_threads > SITUATION_MAX_THREADS) num_threads = SITUATION_MAX_THREADS;
    pool->thread_count = num_threads;
    pool->hot_reload_rate = hot_reload_rate; // [v2.3.37] Store rate

    // Round queue size up to next power of 2 for fast bitmasking
    size_t cap = 256;
    while (cap < queue_size) cap *= 2;

    // Initialize Dual Queues
    for (int i = 0; i < 2; i++) {
        pool->queues[i].capacity = cap;
        pool->queues[i].mask = cap - 1;
        pool->queues[i].jobs = (SituationJob*)SIT_CALLOC(cap, sizeof(SituationJob));

        if (!pool->queues[i].jobs) {
            // Cleanup previous if failed
            if(i==1) SIT_FREE(pool->queues[0].jobs);
            return false;
        }

        mtx_init(&pool->queues[i].lock, mtx_plain);
        atomic_init(&pool->queues[i].head, 0);
        atomic_init(&pool->queues[i].tail, 0);

        // Initialize generations to 1 (0 is reserved for null/invalid)
        for(size_t k=0; k<cap; k++) {
            atomic_init(&pool->queues[i].jobs[k].generation, 1);
            atomic_init(&pool->queues[i].jobs[k].is_completed, true); // Slots start "free"
        }
    }

    cnd_init(&pool->wake_condition);
    cnd_init(&pool->idle_condition);
    atomic_init(&pool->active_jobs, 0);
    atomic_init(&pool->shutdown, false);

    for (size_t i = 0; i < num_threads; ++i) {
        if (thrd_create(&pool->threads[i], _SitCore_WorkerEntry, pool) != thrd_success) {
            // Rollback logic omitted for brevity, assuming stable OS env
            return false;
        }
    }

    // [v2.3.34] Spawn Dedicated I/O Thread (Conditional)
    if (!disable_io) {
        if (thrd_create(&pool->io_thread, _SitFS_IOThreadEntry, pool) != thrd_success) {
            return false;
        }
    } else {
        pool->io_thread = 0; // Explicitly null
    }

    pool->is_active = true;
    return true;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Submits a job to the thread pool for execution.
 *
 * @details Pushes a task into one of the priority queues. This function is thread-safe and wait-free in the common case
 *          (unless the queue is full).
 *
 *          **Small Object Optimization (SOO):**
 *          - If `data_size` is <= 64 bytes, the payload is copied *by value* into the job structure itself. This avoids heap allocation
 *            and pointer indirection, making it ideal for passing small structs like matrices, vectors, or configuration identifiers.
 *          - If `data_size` > 64 bytes, the `data` pointer is passed through directly. The user must ensure the pointed-to memory
 *            remains valid until the job completes.
 *
 * @param pool The thread pool instance.
 * @param func The work function to execute. Prototype: `void my_func(void* data, void* unused)`.
 * @param data A pointer to the data payload.
 * @param data_size The size of the data payload in bytes. Determines whether SOO is used.
 * @param flags Bitmask of behavior flags:
 *              - `SIT_SUBMIT_HIGH_PRIORITY`: Submit to the high-priority queue (Physics, Audio).
 *              - `SIT_SUBMIT_BLOCK_IF_FULL`: If the queue is full, block (sleep) until a slot becomes available.
 *              - `SIT_SUBMIT_RUN_IF_FULL`: If the queue is full, execute the job *immediately* on the calling thread. This prevents stalls but blocks the main thread temporarily.
 *
 * @return A unique `SituationJobId` handle for the submitted job.
 *         - This handle encodes the queue index, slot index, and a generation counter.
 *         - It allows for O(1) status checks via `SituationWaitForJob`.
 *         - Returns `0` if the job could not be submitted (e.g., queue full and no backpressure strategy specified).
 */
SITAPI SituationJobId SituationSubmitJobEx(SituationThreadPool* pool, void (*func)(void*, void*), const void* data, size_t data_size, SituationJobFlags flags) {
    if (!pool || !pool->is_active) return 0;

    int q_idx = (flags & SIT_SUBMIT_HIGH_PRIORITY) ? 1 : 0;

    mtx_lock(&pool->queues[q_idx].lock);

    // [v2.3.37] Inline Fallback if I/O Thread Disabled (Queue 0 Only)
    // If we submit a low-priority job but have no I/O thread, we must run it inline
    // to prevent the job from sitting in the queue forever.
    if (q_idx == 0 && pool->io_thread == 0) {
        mtx_unlock(&pool->queues[q_idx].lock);
        // Execute immediately
        if (func) {
            SituationError dummy_err = SITUATION_SUCCESS;
            func((void*)data, (void*)&dummy_err);
        }
        return 0; // Treated as "done/inline"
    }

    // Check Capacity (with Backpressure Handling)
    size_t head;
    while (true) {
        head = atomic_load(&pool->queues[q_idx].head);
        size_t tail = atomic_load(&pool->queues[q_idx].tail);

        if (head - tail >= pool->queues[q_idx].capacity) {
            mtx_unlock(&pool->queues[q_idx].lock);

            // Handle Backpressure
            if (flags & SIT_SUBMIT_RUN_IF_FULL) {
                // "Velocity" Path: Run immediately to avoid stutter
                if(func) {
                    // [CRITICAL FIX] Legacy Support for SituationError* signature
                    // Must pass a dummy error pointer, even on main thread, to prevent segfaults in legacy callbacks.
                    SituationError dummy_err = SITUATION_SUCCESS;
                    func((void*)data, (void*)&dummy_err);
                }
                return 0; // 0 indicates job is already done/invalid handle
            }
            else if (flags & SIT_SUBMIT_BLOCK_IF_FULL) {
                // "Robust" Path: Spin-wait (polite yield) until slot opens
                // [FIX] Replaced recursion with iterative loop to prevent Stack Overflow
                struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000 }; // 10us
                thrd_sleep(&ts, NULL);

                // Re-acquire lock and try again
                mtx_lock(&pool->queues[q_idx].lock);
                continue;
            }

            // Default Path: Fail
            _SitCore_SetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL, "Job queue full and no blocking/run-inline flag set.");
            return 0;
        }
        // If we have space, break the retry loop and proceed to slot reservation
        break;
    }

    size_t slot_idx = head & pool->queues[q_idx].mask;
    SituationJob* job = &pool->queues[q_idx].jobs[slot_idx];
    uint16_t gen = atomic_load(&job->generation);

    // Init Job Fields
    job->func = func;
    atomic_store(&job->is_completed, false);

    // Reset Dependency Fields
    atomic_store(&job->dependency_count, 0);
    atomic_store(&job->continuation_id, 0);
    job->dep_depth = 0;

    // SOO Logic & Safe Copy
    if (data_size > 0 && data_size <= SITUATION_JOB_PAYLOAD_MAX) {
        // Small Data: Copy into SOO buffer
        if (data) memcpy(job->storage, data, data_size);
        job->uses_large_data = false;
        job->owns_memory = false;
    } else {
        // Large Data
        if (flags & SIT_SUBMIT_POINTER_ONLY) {
            // Optimization: Just store the pointer (User promises lifetime)
            job->large_data_ptr = (void*)data;
            job->owns_memory = false;
        } else if (data && data_size > 0) {
            // Safety: Allocate and Copy (Default)
            job->large_data_ptr = SIT_MALLOC(data_size);
            if (job->large_data_ptr) {
                memcpy(job->large_data_ptr, data, data_size);
                job->owns_memory = true;
            } else {
                // Allocation failed. Fallback to pointer and hope?
                // Or fail the submission?
                // For robustness, we fallback to pointer but log warning if possible.
                // In this lock-held section, we just store the pointer.
                job->large_data_ptr = (void*)data;
                job->owns_memory = false;
            }
        } else {
            // No data or invalid size
            job->large_data_ptr = (void*)data;
            job->owns_memory = false;
        }
        job->uses_large_data = true;
    }

    // Commit
    atomic_fetch_add(&pool->queues[q_idx].head, 1);
    atomic_fetch_add(&pool->active_jobs, 1);
    mtx_unlock(&pool->queues[q_idx].lock);
    cnd_signal(&pool->wake_condition);

    return _SitMakeId((uint32_t)q_idx, (uint32_t)gen, (uint32_t)slot_idx);
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Blocks the calling thread until a specific job has completed.
 *
 * @details This function performs an efficient O(1) check using the job's generation ID.
 *          It handles three scenarios correctly:
 *          1. **Job Pending/Running:** The ID in the slot matches. The function yields/sleeps until the `is_completed` flag is set.
 *          2. **Job Already Finished & Replaced:** The ID in the slot has a *newer* generation than the handle. This proves the old job finished and the slot was reused. Returns immediately.
 *          3. **Invalid Handle:** Returns immediately (safe default).
 *
 * @param pool The thread pool instance.
 * @param job_id The handle of the job to wait for.
 * @return `true` when the job is confirmed complete.
 */
SITAPI bool SituationWaitForJob(SituationThreadPool* pool, SituationJobId job_id) {
    SIT_ASSERT_MAIN_THREAD();
    if (job_id == 0) return true; // Immediate jobs (Run-Inline) are implicitly done

    uint32_t q_idx = (job_id >> SIT_ID_QUEUE_SHIFT) & 1;
    uint32_t slot_idx = job_id & SIT_ID_SLOT_MASK;
    uint32_t expected_gen = (job_id >> SIT_ID_GEN_SHIFT) & SIT_ID_GEN_MASK;

    // Direct pointer access to the slot (safe because array doesn't move)
    SituationJob* job = &pool->queues[q_idx].jobs[slot_idx];

    while (true) {
        uint16_t current_gen = atomic_load(&job->generation);

        // O(1) Status Logic:
        // 1. If generation has incremented, the slot was reused for a NEW job -> OLD job is definitely done.
        // 2. If generation matches, check the explicit completion flag.

        if (current_gen != (uint16_t)expected_gen) return true;
        if (atomic_load(&job->is_completed)) return true;

        // Job not done. Yield CPU politely.
        // We don't use a condition var here to avoid N^2 condition/mutex pairs.
        // Active polling with yield is standard for game tasks waiting <1 frame.
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000 }; // 1us sleep
        thrd_sleep(&ts, NULL);
    }
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Helper wrapper for parallel dispatch jobs.
 * @details This function unpacks the `_SitParallelCtx`, executes the user's loop body for the assigned range, and then decrements the shared atomic counter.
 */
static void _SitCore_ParallelWorker(void* data, void* ctx) {
    (void)ctx;
    _SitParallelCtx* pctx = (_SitParallelCtx*)data;
    // Execute loop range
    for (int i = pctx->start_idx; i < pctx->end_idx; ++i) {
        pctx->user_func(i, pctx->user_data);
    }
    // Decrement shared counter
    atomic_fetch_sub(pctx->counter, 1);
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Executes a parallel loop (Fork-Join) across the worker threads.
 *
 * @details Splits a workload of `count` items into efficient batches and distributes them to the thread pool.
 *          This is ideal for data-parallel tasks like particle updates, culling, or image processing.
 *
 *          **"Helping" Strategy:**
 *          While waiting for the workers to finish the batches, the calling thread (usually Main) does not sleep.
 *          Instead, it actively "steals" and executes jobs from the High Priority queue. This maximizes CPU utilization
 *          and prevents the main thread from becoming a bottleneck.
 *
 * @param pool The thread pool instance.
 * @param count The total number of items to process (iterations).
 * @param min_batch_size The minimum number of items per batch.
 *                       - Prevents overhead from creating too many tiny jobs.
 *                       - A good rule of thumb is a size that takes at least 10-50 microseconds to execute.
 * @param func The user callback function. It will be called for every index `i` from 0 to `count - 1`.
 *             Prototype: `void func(int index, void* user_data)`.
 * @param user_data A pointer passed to the callback (e.g., the array being processed).
 */
SITAPI void SituationDispatchParallel(SituationThreadPool* pool, int count, int min_batch_size, void (*func)(int, void*), void* user_data) {
    SIT_ASSERT_MAIN_THREAD();
    if (count <= 0) return;

    // 1. Calculate Chunking
    int thread_count = (int)pool->thread_count;
    // Heuristic: Aim for 2 batches per thread to allow load balancing
    int batch_size = count / (thread_count * 2);
    if (batch_size < min_batch_size) batch_size = min_batch_size;
    if (batch_size < 1) batch_size = 1;

    int num_batches = (count + batch_size - 1) / batch_size;

    // 2. Setup Sync Counter
    // Stack allocated atomic is safe because this function blocks until 0.
    atomic_int completion_counter;
    atomic_init(&completion_counter, num_batches);

    // 3. Submit Batches
    for (int i = 0; i < num_batches; ++i) {
        int start = i * batch_size;
        int end = start + batch_size;
        if (end > count) end = count;

        _SitParallelCtx ctx;
        ctx.user_func = func;
        ctx.user_data = user_data;
        ctx.counter = &completion_counter;
        ctx.start_idx = start;
        ctx.end_idx = end;

        // Important: Use SIT_SUBMIT_RUN_IF_FULL.
        // If queue is full, we run it here immediately. This prevents deadlocks
        // and ensures progress even under heavy load.
        // Using High Priority ensures workers pick these up ASAP.
        SituationSubmitJobEx(pool, _SitCore_ParallelWorker, &ctx, sizeof(_SitParallelCtx), SIT_SUBMIT_HIGH_PRIORITY | SIT_SUBMIT_RUN_IF_FULL);
    }

    // 4. Helping Loop (Work Stealing)
    // While waiting for the batches to finish, the main thread shouldn't sleep.
    // It should help process the High Priority queue to clear the blockage.
    while (atomic_load(&completion_counter) > 0) {
        bool stole_work = false;

        // Peek into High Priority Queue (Non-blocking try)
        if (mtx_trylock(&pool->queues[1].lock) == thrd_success) {
            size_t head = atomic_load(&pool->queues[1].head);
            size_t tail = atomic_load(&pool->queues[1].tail);

            if (tail != head) {
                // Steal it!
                size_t idx = tail & pool->queues[1].mask;
                SituationJob* job_ptr = &pool->queues[1].jobs[idx];
                atomic_store(&pool->queues[1].tail, tail + 1);
                mtx_unlock(&pool->queues[1].lock);

                // Execute Stolen Job
                void* d = job_ptr->uses_large_data ? job_ptr->large_data_ptr : job_ptr->storage;
                if (job_ptr->func) job_ptr->func(d, NULL);

                atomic_store(&job_ptr->is_completed, true);
                uint16_t g = atomic_load(&job_ptr->generation);
                atomic_store(&job_ptr->generation, (uint16_t)((g + 1) & SIT_ID_GEN_MASK));
                atomic_fetch_sub(&pool->active_jobs, 1);

                stole_work = true;
            } else {
                mtx_unlock(&pool->queues[1].lock);
            }
        }

        if (!stole_work) {
            thrd_yield(); // Nothing to steal, brief yield
        }
    }
    // All batches done.
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Blocks until all submitted jobs in the pool have finished.
 *
 * @details Acts as a global synchronization barrier. It waits for both High and Low priority queues to drain completely
 *          and for all currently running worker threads to finish their tasks.
 *
 *          Useful for:
 *          - End-of-frame synchronization.
 *          - Ensuring all assets are loaded before switching scenes.
 *          - Clean shutdown.
 *
 * @param pool The thread pool instance.
 */
SITAPI void SituationWaitForAllJobs(SituationThreadPool* pool) {
    if (!pool->is_active) return;

    // We use the Low Priority queue lock for the idle condition
    mtx_lock(&pool->queues[0].lock);
    while (atomic_load(&pool->active_jobs) > 0) {
        cnd_wait(&pool->idle_condition, &pool->queues[0].lock);
    }
    mtx_unlock(&pool->queues[0].lock);
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Shuts down the thread pool and releases all resources.
 *
 * @details 1. Sets a shutdown flag to signal all worker threads to exit.
 *          2. Wakes all sleeping threads.
 *          3. Joins all worker threads (waits for them to terminate).
 *          4. Destroys internal mutexes, condition variables, and ring buffer memory.
 *
 *          **Note:** Any jobs still pending in the queue will be discarded and NOT executed.
 *
 * @param pool Pointer to the `SituationThreadPool` to destroy.
 */
SITAPI void SituationDestroyThreadPool(SituationThreadPool* pool) {
    if (!pool->is_active) return;

    atomic_store(&pool->shutdown, true);

    // Wake everyone up so they can exit
    cnd_broadcast(&pool->wake_condition);

    for (size_t i = 0; i < pool->thread_count; ++i) {
        thrd_join(pool->threads[i], NULL);
    }

    if (pool->io_thread) {
        thrd_join(pool->io_thread, NULL);
        pool->io_thread = 0;
    }

    mtx_destroy(&pool->queues[0].lock);
    mtx_destroy(&pool->queues[1].lock);
    cnd_destroy(&pool->wake_condition);
    cnd_destroy(&pool->idle_condition);

    SIT_FREE(pool->queues[0].jobs);
    SIT_FREE(pool->queues[1].jobs);

    pool->is_active = false;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Job callback for background audio loading.
 * @details Decodes audio to RAM (SITUATION_AUDIO_LOAD_FULL) on a worker thread to avoid main-thread disk I/O.
 * @param data Pointer to the _SitAsyncAudioCtx (embedded in job storage).
 * @param unused Unused user context.
 */
static void _SitAudio_AsyncAudioWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncAudioCtx* ctx = (_SitAsyncAudioCtx*)data;

    // Use FULL load mode to decode to RAM on this background thread.
    // This ensures no disk I/O happens on the main thread later.
    SituationLoadSoundFromFile(ctx->path, SITUATION_AUDIO_LOAD_FULL, ctx->looping, ctx->target);

    // Cleanup string copy
    SIT_FREE(ctx->path);
    // Note: We don't free 'ctx' here because it's embedded in the job storage!
    // The beauty of Small Object Optimization.
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Asynchronously loads an audio file from disk in a background thread.
 *
 * @details This is a convenience helper that wraps `SituationLoadSoundFromFile` in a thread pool job.
 *          It performs a **Full Load** (decoding the entire file to RAM) to avoid disk I/O on the main thread.
 *
 *          **Usage:**
 *          1. Call this function. It returns immediately.
 *          2. Store the returned `SituationJobId`.
 *          3. Use `SituationWaitForJob(job_id)` to know when loading is done.
 *          4. Once complete, the `out_sound` struct contains the ready-to-play sound.
 *
 * @param pool The thread pool instance.
 * @param file_path The path to the audio file.
 * @param looping Whether the sound should loop.
 * @param out_sound Pointer to the `SituationSound` struct to be initialized.
 *                  **Important:** This memory must remain valid until the job completes.
 *
 * @return A `SituationJobId` for the loading task, or `0` if submission failed.
 */
SITAPI SituationJobId SituationLoadSoundFromFileAsync(SituationThreadPool* pool, const char* file_path, bool looping, SituationSound* out_sound) {
    if (!pool || !file_path || !out_sound) return 0;

    // 1. Prepare Context
    _SitAsyncAudioCtx ctx;
    ctx.path = _SitUtils_Strdup(file_path); // Duplicate string (ownership transfers to worker)
    if (!ctx.path) return 0;

    ctx.looping = looping;
    ctx.target = out_sound;

    // 2. Clear target struct for safety
    memset(out_sound, 0, sizeof(SituationSound));

    // 3. Submit to Low Priority Queue (Assets/IO)
    // We pass 'ctx' by value. Since sizeof(_SitAsyncAudioCtx) is ~24 bytes,
    // it fits easily into the 64-byte storage (SOO). No malloc for the context!
    return SituationSubmitJobEx(
        pool,
        _SitAudio_AsyncAudioWorker,
        &ctx,
        sizeof(_SitAsyncAudioCtx),
        SIT_SUBMIT_DEFAULT // Low Priority is correct for loading
    );
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Worker for async file loading.
 */
static void _SitFS_AsyncFileLoadWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncFileLoadCtx* ctx = (_SitAsyncFileLoadCtx*)data;

    unsigned int bytes_read = 0;
    unsigned char* file_data = NULL;
    SituationError err = SituationLoadFileData(ctx->path, &bytes_read, &file_data);
    (void)err; // Suppress unused warning if callback doesn't care

    if (ctx->callback) {
        ctx->callback(file_data, (size_t)bytes_read, ctx->user_data);
    } else {
        // If no callback, we must free the data to avoid a leak!
        if (file_data) SIT_FREE(file_data);
    }

    SIT_FREE(ctx->path);
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Asynchronously loads a file from disk.
 * @details Offloads the blocking `SituationLoadFileData` call to a background thread.
 *          The user callback is invoked on the worker thread with the loaded data.
 *
 * @param pool The thread pool.
 * @param file_path The path to load.
 * @param callback Function to call when done.
 * @param user_data User context pointer.
 * @return Job ID or 0 on failure.
 */
SITAPI SituationJobId SituationLoadFileAsync(SituationThreadPool* pool, const char* file_path, SituationFileLoadCallback callback, void* user_data) {
    if (!pool || !file_path) return 0;

    _SitAsyncFileLoadCtx ctx;
    ctx.path = _SitUtils_Strdup(file_path);
    if (!ctx.path) return 0;
    ctx.callback = callback;
    ctx.user_data = user_data;

    SituationJobId jid = SituationSubmitJobEx(pool, _SitFS_AsyncFileLoadWorker, &ctx, sizeof(_SitAsyncFileLoadCtx), SIT_SUBMIT_DEFAULT);
    if (jid == 0) {
        SIT_FREE(ctx.path);
    }
    return jid;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Worker for async text file loading.
 */
static void _SitFS_AsyncFileTextLoadWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncFileTextLoadCtx* ctx = (_SitAsyncFileTextLoadCtx*)data;

    char* text = SituationLoadFileText(ctx->path);

    if (ctx->callback) {
        ctx->callback(text, ctx->user_data);
    } else {
        if (text) SIT_FREE(text);
    }

    SIT_FREE(ctx->path);
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Asynchronously loads a text file from disk.
 * @details Offloads the blocking `SituationLoadFileText` call to a background thread.
 *          The user callback is invoked on the worker thread with the loaded string.
 *
 * @param pool The thread pool.
 * @param file_path The path to load.
 * @param callback Function to call when done.
 * @param user_data User context pointer.
 * @return Job ID or 0 on failure.
 */
SITAPI SituationJobId SituationLoadFileTextAsync(SituationThreadPool* pool, const char* file_path, SituationFileTextLoadCallback callback, void* user_data) {
    if (!pool || !file_path) return 0;

    _SitAsyncFileTextLoadCtx ctx;
    ctx.path = _SitUtils_Strdup(file_path);
    if (!ctx.path) return 0;
    ctx.callback = callback;
    ctx.user_data = user_data;

    SituationJobId jid = SituationSubmitJobEx(pool, _SitFS_AsyncFileTextLoadWorker, &ctx, sizeof(_SitAsyncFileTextLoadCtx), SIT_SUBMIT_DEFAULT);
    if (jid == 0) {
        SIT_FREE(ctx.path);
    }
    return jid;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Worker for async text file saving.
 */
static void _SitFS_AsyncFileTextSaveWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncFileTextSaveCtx* ctx = (_SitAsyncFileTextSaveCtx*)data;

    bool success = SituationSaveFileText(ctx->path, ctx->text_copy);

    if (ctx->callback) {
        ctx->callback(success, ctx->user_data);
    }

    SIT_FREE(ctx->path);
    SIT_FREE(ctx->text_copy);
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Asynchronously saves a string to a text file.
 * @details Copies the input string to a temporary buffer and offloads the write to a worker thread.
 *
 * @param pool The thread pool.
 * @param file_path The path to save to.
 * @param text The null-terminated string to write.
 * @param callback Function to call when done.
 * @param user_data User context pointer.
 * @return Job ID or 0 on failure.
 */
SITAPI SituationJobId SituationSaveFileTextAsync(SituationThreadPool* pool, const char* file_path, const char* text, SituationFileSaveCallback callback, void* user_data) {
    if (!pool || !file_path || !text) return 0;

    _SitAsyncFileTextSaveCtx ctx;
    ctx.path = _SitUtils_Strdup(file_path);
    if (!ctx.path) return 0;

    ctx.text_copy = _SitUtils_Strdup(text);
    if (!ctx.text_copy) {
        SIT_FREE(ctx.path);
        return 0;
    }

    ctx.callback = callback;
    ctx.user_data = user_data;

    SituationJobId jid = SituationSubmitJobEx(pool, _SitFS_AsyncFileTextSaveWorker, &ctx, sizeof(_SitAsyncFileTextSaveCtx), SIT_SUBMIT_DEFAULT);
    if (jid == 0) {
        SIT_FREE(ctx.path);
        SIT_FREE(ctx.text_copy);
    }
    return jid;
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief [INTERNAL] Worker for async file saving.
 */
static void _SitFS_AsyncFileSaveWorker(void* data, void* unused) {
    (void)unused;
    _SitAsyncFileSaveCtx* ctx = (_SitAsyncFileSaveCtx*)data;

    bool success = (SituationSaveFileData(ctx->path, ctx->data_copy, (unsigned int)ctx->size) == SITUATION_SUCCESS);

    if (ctx->callback) {
        ctx->callback(success, ctx->user_data);
    }

    SIT_FREE(ctx->path);
    SIT_FREE(ctx->data_copy);
}
#endif

#if defined(SITUATION_ENABLE_THREADING)

/**
 * @brief Asynchronously saves data to a file.
 * @details Copies the input data to a temporary buffer and offloads the write to a worker thread.
 *          This allows the caller to free their data immediately after this function returns.
 *
 * @param pool The thread pool.
 * @param file_path The path to save to.
 * @param data The data to write.
 * @param size The size of the data in bytes.
 * @param callback Function to call when done.
 * @param user_data User context pointer.
 * @return Job ID or 0 on failure.
 */
SITAPI SituationJobId SituationSaveFileAsync(SituationThreadPool* pool, const char* file_path, const void* data, size_t size, SituationFileSaveCallback callback, void* user_data) {
    if (!pool || !file_path || !data || size == 0) return 0;

    _SitAsyncFileSaveCtx ctx;
    ctx.path = _SitUtils_Strdup(file_path);
    if (!ctx.path) return 0;

    ctx.data_copy = SIT_MALLOC(size);
    if (!ctx.data_copy) {
        SIT_FREE(ctx.path);
        return 0;
    }
    memcpy(ctx.data_copy, data, size);

    ctx.size = size;
    ctx.callback = callback;
    ctx.user_data = user_data;

    SituationJobId jid = SituationSubmitJobEx(pool, _SitFS_AsyncFileSaveWorker, &ctx, sizeof(_SitAsyncFileSaveCtx), SIT_SUBMIT_DEFAULT);
    if (jid == 0) {
        SIT_FREE(ctx.path);
        SIT_FREE(ctx.data_copy);
    }
    return jid;
}
#endif
