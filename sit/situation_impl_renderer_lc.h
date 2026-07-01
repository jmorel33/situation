/***************************************************************************************************
*
*   situation_impl_renderer_lc.h - Renderer Lifecycle (Init, Backends, Thread, Hot-Reload)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Renderer init/shutdown, OpenGL/Vulkan bootstrap, soft command-buffer execute,
*   internal 2D renderers, render thread, hot-reload pass (GL + VK inline).
*
*   Do not include directly — included only from situation_impl_renderer.h.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_RENDERER_LC_H
#define SITUATION_IMPL_RENDERER_LC_H

#if !defined(__STDC_NO_THREADS__)
/**
 * @brief [INTERNAL] Starts the dedicated render thread and performs context handoff.
 *
 * @details This function is called during library initialization (from `_SituationInitSubsystems`)
 *          when `SITUATION_ENABLE_RENDER_THREAD` is defined and enabled in `init_info`.
 *          It is responsible for creating and launching the render thread, which takes over
 *          all GPU command execution, submission, presentation, and resource cleanup.
 *
 *          Critical sequence (do not reorder):
 *            1. Validates that the main thread currently owns the GL context (if OpenGL)
 *            2. Releases the context from the main thread
 *               (`glfwMakeContextCurrent(NULL)`) to allow handoff
 *            3. Creates the render thread via `thrd_create(_SituationRenderThreadEntry, NULL)`
 *            4. Waits briefly (spin/yield) until the render thread acquires the context
 *               (via atomic flag `sit_render.gl_context_released` or similar sync)
 *            5. Sets up per-frame resources (graveyards, fences, command buffers)
 *            6. Initializes render metrics (if enabled)
 *            7. Marks render thread as active (`sit_render.thread_active = true`)
 *            8. Returns success or failure based on thread creation and handoff
 *
 *          On success:
 *            - Main thread no longer has GL context
 *            - Render thread owns context and is running its loop
 *            - All subsequent GL/VK calls must go through command buffers
 *
 *          On failure:
 *            - Logs error (e.g. thread creation fail)
 *            - Returns false init aborts or falls back to main-thread rendering
 *
 * @param info Pointer to `SituationInitInfo` containing render thread preferences
 *             (e.g. enable/disable flag, thread priority hints if supported).
 *             May influence whether the thread starts or falls back to synchronous mode.
 *
 * @return true if render thread was successfully created, context handed off,
 *         and thread is running,
 *         false on failure (thread creation error, context handoff timeout,
 *         allocation failure, etc.).
 *         Failures are logged internally and may set global `SituationError`.
 *
 * @note **Critical thread safety point**:
 *       - Must be called **only from the main thread** with GL context current
 *       - Context release must succeed before thread start
 *       - Render thread acquires context immediately after creation
 *       - No GL calls allowed on main thread after this function succeeds
 *
 *       If render thread is disabled (`init_info->enable_render_thread = false`
 *       or compile-time define absent), this function returns true immediately
 *       (no-op) main thread retains context and does synchronous rendering.
 *
 *       Dependencies:
 *         - GLFW window must exist (`sit_gs.sit_glfw_window != NULL`)
 *         - GL context must be current on calling thread
 *         - Vulkan path may skip context handoff (uses queues instead)
 *
 * @see _SituationInitSubsystems (caller), _SituationRenderThreadEntry,
 *      SituationInitInfo.enable_render_thread,
 *      SITUATION_ENABLE_RENDER_THREAD (compile-time toggle),
 *      SITUATION_ERROR_THREAD_CREATION_FAILED,
 *      SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT (related)
 */
static SituationError _SituationInitRenderThread(const SituationInitInfo* info) {
    // Note: resource_registry_mutex is now initialized earlier in SituationInit, before renderer init
    
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (info->render_thread_count == 0) return SITUATION_SUCCESS;

    sit_render.enabled = true;
    atomic_init(&sit_render.thread_active, true);
    atomic_init(&sit_render.thread_shutdown_req, false);
    // Note: frames_pending already initialized to 0 in _SituationInitRenderer
    atomic_init(&sit_render.render_queue_depth, 0);
    sit_render.render_queue_head = 0;
    sit_render.render_queue_tail = 0;
    sit_render.render_queue_count = 0;

    // Note: render_queue_mutex, main_wait_cv, and render_queue_cv already initialized in _SituationInitRenderer

    // [Polish 1] GL Handover: Release from main before spawn
    #if defined(SITUATION_USE_OPENGL)
    if (sit_gs.sit_glfw_window) {
        glfwMakeContextCurrent(NULL); // Release context for render thread
        atomic_store(&sit_render.gl_context_released, true); // Render thread reads after thrd_create
    }
    #endif

    fprintf(stderr, "[Situation] [MAIN] About to create render thread...\n"); fflush(stderr);
    if (thrd_create(&sit_render.render_thread, _SituationRenderThreadEntry, NULL) != thrd_success) {
        fprintf(stderr, "[Situation] [MAIN] Render thread creation FAILED\n"); fflush(stderr);
        #if defined(SITUATION_USE_OPENGL)
        if (sit_gs.sit_glfw_window) glfwMakeContextCurrent(sit_gs.sit_glfw_window); // Reacquire on fail
        #endif
        return _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CREATION_FAILED, "Failed to spawn render thread");
    }
    fprintf(stderr, "[Situation] [MAIN] Render thread created successfully\n"); fflush(stderr);

    /* Re-apply main-thread OS name after winpthreads spawns the render worker. */
    if (sit_gs.main_thread_name[0]) {
        _SituationSetCurrentThreadName(sit_gs.main_thread_name);
    }

    // Note: Main thread must NOT call GL/VK cmds post-handover. Use render queue for all GPU work.

    // For OpenGL, Main thread typically needs a shared context for asset loading.
    // _SituationInitOpenGL created 'loader_window' for this.
    // We should make THAT current now if it exists.
    #if defined(SITUATION_USE_OPENGL)
    if (sit_render.gl.loader_window) {
        _SituationMakeGLContextCurrentForHostThread();
    }
    #endif

    return SITUATION_SUCCESS;
    #else
    return SITUATION_SUCCESS;
    #endif
}

static SituationError _SituationDestroyRenderThread(void) {
    #if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (!sit_render.enabled || !atomic_load(&sit_render.thread_active)) {
        return SITUATION_SUCCESS;
    }

    if (atomic_load(&sit_render.thread_shutdown_req)) {
        return SITUATION_SUCCESS;
    }

    SituationError result = SITUATION_SUCCESS;

    // If the render thread never processed a frame, it's sitting in cnd_wait.
    // Set shutdown flag first, then wake it — the timed wait ensures it checks
    // the flag within 50ms even if the signal is missed.
    atomic_store(&sit_render.thread_shutdown_req, true);

    // Wake the render thread repeatedly until it acknowledges shutdown
    for (int wake = 0; wake < 3; ++wake) {
        mtx_lock(&sit_render.render_queue_mutex);
        cnd_broadcast(&sit_render.render_queue_cv);
        mtx_unlock(&sit_render.render_queue_mutex);
        if (!atomic_load(&sit_render.thread_active)) break;
        SITUATION_SLEEP_MS(20);
    }

    cnd_broadcast(&sit_render.main_wait_cv);

    // [v2.3.22] Timed Join (Polling thread_active for 1s before join)
    // C11 thrd_join is blocking, so we poll for the thread to mark itself inactive first.
    // If it doesn't deactivate within the timeout, we log an error but proceed to block-join.
    int ticks = 10;
    bool timed_out = true;

    for (int i = 0; i < ticks; ++i) {
        if (!atomic_load(&sit_render.thread_active)) {
            timed_out = false;
            break;
        }
        // Re-broadcast each tick in case the signal was missed
        mtx_lock(&sit_render.render_queue_mutex);
        cnd_broadcast(&sit_render.render_queue_cv);
        mtx_unlock(&sit_render.render_queue_mutex);

        if (i % 5 == 0 && i > 0) {
            fprintf(stderr, "[WARN] Render join tick %d/10...\n", i);
        }
        SITUATION_SLEEP_MS(100);
    }

    if (timed_out) {
        result = _SituationSetErrorFromCode(
            SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT,
            "Render thread join timeout—aborted.");
        // Proceed to join anyway to avoid leaking a running thread, but the error is logged.
    }

    if (thrd_join(sit_render.render_thread, NULL) != thrd_success) {
        SituationError join_err = _SituationSetErrorFromCode(
            SITUATION_ERROR_THREAD_JOIN_FAILED, "Render thread join failed.");
        if (result == SITUATION_SUCCESS) {
            result = join_err;
        }
    }

    // Fence for cleanup vis
    atomic_thread_fence(memory_order_release);

    mtx_destroy(&sit_render.render_queue_mutex);
    cnd_destroy(&sit_render.render_queue_cv);
    cnd_destroy(&sit_render.main_wait_cv);

    atomic_store(&sit_render.thread_active, false);
    sit_render.enabled = false;

    // [GL] Release context
    #if defined(SITUATION_USE_OPENGL)
    glfwMakeContextCurrent(NULL);
    #endif

    return result;
    #else
    return SITUATION_SUCCESS;
    #endif
}
#endif

//----------------------------------------------------------------------------------------------------------
// --- Core Lifecycle Implementation ---
//----------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes the entire Situation library.
 *
 * @details This is the main entry point and the first function a user of the library must call. It orchestrates the complete initialization process by setting up all necessary subsystems in a specific, dependency-respecting order:
 * 1.  **Platform:** Initializes low-level libraries like GLFW.
 * 2.  **Window:** Creates the main application window.
 * 3.  **Renderer:** Initializes the selected graphics backend (OpenGL or Vulkan), including contexts/devices, swapchains, internal pipelines, etc.
 * 4.  **Subsystems:** Initializes audio, input handling, timer system, filesystem utilities, and other core functionalities.
 *
 * If any step in this sequence fails, the function triggers a comprehensive cleanup process (`_SituationFullCleanupOnError`) to ensure that no resources
 * are leaked and the library is left in a clean, uninitialized state.
 *
 * @param argc The number of command-line arguments, including the program name.
 *             This is typically the `argc` parameter from the `main` function.
 * @param argv An array of strings representing the command-line arguments.
 *             This is typically the `argv` parameter from the `main` function.
 *             The library stores these for later querying via argument functions.
 * @param init_info A pointer to a `SituationInitInfo` struct containing all necessary configuration options for the library's initialization (e.g., window title, dimensions, initial flags).
 *                  This pointer must not be NULL.
 *
 * @return SITUATION_SUCCESS on successful initialization of all subsystems.
 * @return SITUATION_ERROR_INVALID_PARAM if `init_info` is NULL.
 * @return SITUATION_ERROR_ALREADY_INITIALIZED if `SituationInit` is called more than once without an intervening `SituationShutdown`.
 * @return SITUATION_ERROR_INIT_FAILED if any part of the initialization sequence fails (e.g., GLFW failure, graphics context creation failure,
 *         audio device failure). A specific error code and message will be set by the failing subsystem's initialization function. Cleanup is attempted.
 * @return SITUATION_ERROR_SHUTDOWN_FAILED if a previous call to `SituationShutdown` failed and left the library in an inconsistent state, preventing re-initialization.
 *         This is a safeguard to avoid attempting initialization from a bad state.
 *
 * @note This function must be called before any other `SITAPI` functions (except potentially other init/shutdown functions).
 * @note The library is designed to be initialized and shut down once per application run. While re-initialization after a successful shutdown
 *       is intended to work, it's generally recommended to structure the application's main lifecycle around a single init/shutdown pair.
 * @warning This function is not thread-safe. It must be called from the main thread of the application.
 *
 * @see SituationShutdown(), SituationInitInfo, SituationGetLastErrorMsg()
 */

// [v2.3.24b] Integration Zenith: Initialization Validation
static SituationError _SituationValidateRenderCaps(void) {
#if defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.device) {
        // Validate Semaphore Creation (Critical for Queue Sync)
        VkSemaphoreCreateInfo sema_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore sema = VK_NULL_HANDLE;
        if (vkCreateSemaphore(sit_render.vk.device, &sema_info, NULL, &sema) != VK_SUCCESS) {
            return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Validation: Semaphore creation failed.");
        }
        vkDestroySemaphore(sit_render.vk.device, sema, NULL);

        // Check Queue Topology
        if (sit_render.vk.compute_family_index == sit_render.vk.graphics_family_index) {
            // Not fatal, but good to know for tuning
            #ifndef NDEBUG
            fprintf(stderr, "[Situation] Note: Shared Graphics/Compute Queue (Family %u). Async overlap limited.\n", sit_render.vk.graphics_family_index);
            #endif
        }
    }
#endif
    return SITUATION_SUCCESS;
}


/**
 * @brief [INTERNAL] Dispatches renderer initialization to the selected backend.
 *
 * @details This helper function acts as a simple dispatcher or gateway. Based on the compile-time definitions (`SITUATION_USE_VULKAN` or `SITUATION_USE_OPENGL`), it calls the corresponding backend-specific initialization function:
 *          `_SituationInitVulkan` or `_SituationInitOpenGL`.
 *
 * This abstraction allows the main `SituationInit` function to remain clean and unaware of the specific steps required for each graphics API.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct provided during `SituationInit`. This contains configuration options that might be relevant to the renderer initialization.
 *                  This pointer must not be NULL (though the backend functions should also validate this).
 *
 * @return The `SituationError` code returned by the chosen backend's initialization function (e.g., `_SituationInitVulkan`, `_SituationInitOpenGL`).
 *         - SITUATION_SUCCESS indicates successful renderer setup.
 *         - Any other error code indicates a failure within the backend-specific initialization process.
 *
 * @note This function should only be called from `SituationInit` after the platform and window have been successfully initialized.
 * @note The choice of backend is determined at compile time by defining either `SITUATION_USE_VULKAN` or `SITUATION_USE_OPENGL`.
 * @warning This function is for internal use by `SituationInit` and should not be called directly by user code.
 *
 * @see SituationInit(), _SituationInitVulkan(), _SituationInitOpenGL()
 */
static SituationError _SituationInitRenderer(const SituationInitInfo* init_info) {
    // 1. Initialize Momentum Queue (Common)
    atomic_init(&sit_render.momentum_head, 0);
    atomic_init(&sit_render.momentum_tail, 0);

    if (mtx_init(&sit_render.momentum_mutex, mtx_recursive) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize render queue mutex.");
        return SITUATION_ERROR_INIT_FAILED;
    }
    sit_render.momentum_mutex_initialized = true;

    // 2. Initialize render queue mutex (needed for Vulkan backpressure even before render thread starts)
    #if !defined(__STDC_NO_THREADS__)
    if (mtx_init(&sit_render.render_queue_mutex, mtx_plain) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize render queue mutex.");
        return SITUATION_ERROR_INIT_FAILED;
    }
    if (cnd_init(&sit_render.main_wait_cv) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize main wait condition variable.");
        return SITUATION_ERROR_INIT_FAILED;
    }
    if (cnd_init(&sit_render.render_queue_cv) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to initialize render queue condition variable.");
        return SITUATION_ERROR_INIT_FAILED;
    }
    sit_render.frames_pending = 0;
    atomic_init(&sit_render.gl_context_released, false);

    // [FIX v2.4.251] Apply user-configured backpressure policy from SituationInitInfo.
    // Previously, sit_render_policy_state always started at SPIN regardless of what the
    // user specified — making SituationInitInfo.backpressure_policy dead code.
    if (init_info->backpressure_policy >= SIT_RENDER_BACKPRESSURE_SPIN &&
        init_info->backpressure_policy <= SIT_RENDER_BACKPRESSURE_SLEEP) {
        atomic_store(&sit_render_policy_state, init_info->backpressure_policy);
    }
    #endif

    // Dispatch to the appropriate backend initialization function based on the compile-time flag.
#if defined(SITUATION_USE_VULKAN)
    {
        SituationError err = _SituationInitVulkan(init_info);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }
#elif defined(SITUATION_USE_OPENGL)
    {
        SituationError err = _SituationInitOpenGL(init_info);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }
#else
    _SituationSetErrorFromCode(
        SITUATION_ERROR_NOT_IMPLEMENTED,
        "_SituationInitRenderer: No graphics renderer backend defined (SITUATION_USE_VULKAN or SITUATION_USE_OPENGL). This should be caught at compile time."
    );
    return SITUATION_ERROR_NOT_IMPLEMENTED;
#endif
    _SituationRecomputePacedFramesInFlight();
    return SITUATION_SUCCESS;
}



/**
 * @brief Loads a bitmap font directly from an in-memory data buffer.
 *
 * @details Creates a `SituationFont` handle from a raw, pre-rasterized bitmap font stored
 *          in memory (e.g. embedded font data, loaded asset, or procedurally generated atlas).
 *          This is the low-level entry point for using custom or non-TTF bitmap fonts with
 *          the built-in text renderer.
 *
 *          Expected format of the input data:
 *            - Monochrome or grayscale bitmap (1 byte per pixel recommended)
 *            - Fixed-size grid layout: characters arranged in rows/columns
 *            - Left-to-right, top-to-bottom order (ASCII or custom range)
 *            - No padding between characters (tightly packed)
 *            - Data is row-major: stride = char_width per row
 *
 *          The function:
 *            - Validates input parameters (dimensions, char count, non-null data)
 *            - Allocates internal glyph atlas texture (usually GL_R8 or GL_RGBA8)
 *            - Uploads the bitmap data to GPU
 *            - Computes per-character UV coordinates and metrics
 *              (x/y offset, width/height, advance)
 *            - Stores the glyph table (num_chars entries)
 *            - Sets up default font properties (line height, baseline, etc.)
 *            - Returns a usable `SituationFont` handle
 *
 *          After success, the font can be used immediately with `SituationCmdDrawText`,
 *          `SituationCmdDrawTextEx`, or any text rendering path.
 *
 * @param data Pointer to the raw bitmap data buffer.
 *             Must remain valid for the duration of the call (ownership not transferred).
 *             Size must be exactly `char_width * char_height * num_chars` bytes.
 * @param char_width Width of each glyph in pixels (fixed for all characters).
 *                   Typical values: 8, 16, 32.
 * @param char_height Height of each glyph in pixels (fixed).
 *                    Typical values: 8, 16, 32.
 * @param num_chars Total number of glyphs in the font (e.g. 128 for ASCII, 256 for extended).
 *                  Must be > 0 and reasonable (avoid thousands without reason).
 * @param out_font Pointer to a `SituationFont` variable that receives the new font handle
 *                 on success. On failure, set to `SITUATION_NULL_FONT`.
 *
 * @return SITUATION_SUCCESS on successful load and GPU upload,
 *         SITUATION_ERROR_INVALID_PARAM if data is NULL, dimensions invalid,
 *         num_chars 0, or out_font is NULL,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if internal texture or glyph table allocation failed,
 *         SITUATION_ERROR_GL_UPLOAD_FAILED if texture upload failed (OpenGL),
 *         SITUATION_ERROR_VULKAN_UPLOAD_FAILED if texture creation/upload failed (Vulkan),
 *         or other backend-specific errors.
 *
 * @note The input data is **not** copied the function assumes it remains valid.
 *       For dynamic fonts or TTF loading, use `SituationLoadFontFromFile` or `SituationLoadFontFromMemory` instead.
 *       Texture is created with default sampler state (nearest filtering, clamp-to-edge).
 *       Caller is responsible for destroying the font with `SituationDestroyFont` when done.
 *       Thread safety: Must be called with active GL/VK context (typically render thread or init).
 *
 *       Recommended usage for embedded VGA font:
 *       ```c
 *       SituationFont font;
 *       SituationLoadBitmapFontFromMemory(sit_default_8x8_font, 8, 8, 256, &font);
 *       ```
 *
 * @see SituationDestroyFont, SituationCmdDrawText, SituationCmdDrawTextEx,
 *      sit_default_8x8_font (embedded VGA font), SITUATION_NULL_FONT,
 *      SITUATION_ERROR_MEMORY_ALLOCATION, SITUATION_ERROR_GL_UPLOAD_FAILED
 */
SITAPI SituationError SituationLoadBitmapFontFromMemory(const unsigned char* data, int char_width, int char_height, int num_chars, SituationFont* out_font) {
    if (!data || char_width <= 0 || char_height <= 0 || num_chars <= 0 || !out_font) return SITUATION_ERROR_INVALID_PARAM;

    memset(out_font, 0, sizeof(SituationFont));

    out_font->is_bitmap = true;
    out_font->bitmap_data = data; // Note: We do NOT copy the data for bitmap fonts, assuming it's static/embedded.
    out_font->bitmap_width = char_width;
    out_font->bitmap_height = char_height;
    out_font->bitmap_count = num_chars;

    return SITUATION_SUCCESS;
}


#if defined(SITUATION_USE_OPENGL)
/**
 * @brief [INTERNAL] Maps library-agnostic data types to OpenGL constants.
 *
 * @details Converts `SituationDataType` enums into their corresponding GLenum values (e.g., `SIT_DATA_FLOAT` -> `GL_FLOAT`). This is a utility helper used for vertex attribute configuration.
 *
 * @param type The generic data type enum.
 * @return The corresponding GLenum value.
 * @return `0` if the input type is unknown or invalid.
 *
 * @see SituationCmdSetVertexAttribute()
 */
static GLenum _SituationMapDataTypeToGL(SituationDataType type) {
    switch (type) {
        case SIT_DATA_BYTE: return GL_BYTE;
        case SIT_DATA_UNSIGNED_BYTE: return GL_UNSIGNED_BYTE;
        case SIT_DATA_SHORT: return GL_SHORT;
        case SIT_DATA_UNSIGNED_SHORT: return GL_UNSIGNED_SHORT;
        case SIT_DATA_INT: return GL_INT;
        case SIT_DATA_UNSIGNED_INT: return GL_UNSIGNED_INT;
        case SIT_DATA_FLOAT: return GL_FLOAT;
        case SIT_DATA_DOUBLE: return GL_DOUBLE;
        default: return 0;
    }
}


/**
 * @brief [INTERNAL] Initializes the OpenGL rendering backend and all internal OpenGL resources.
 * @details This is the master function for setting up the OpenGL environment. It is called once during `SituationInit` after the GLFW window and an OpenGL context have been successfully created.
 *
 * @par Initialization Sequence
 *   1.  **Context & Function Loading:** It makes the GLFW window's OpenGL context current for the calling thread and then uses GLAD to load all necessary modern OpenGL function pointers.
 *   2.  **Version & Extension Validation:** It verifies that the available OpenGL version meets the library's minimum requirement (e.g., OpenGL 4.6) and that critical extensions (like `GL_ARB_direct_state_access`) are supported.
 *   3.  **Global VAO Abstraction:** It creates and binds a single, global Vertex Array Object (`sit_render.gl.global_vao_id`).
 *           This VAO remains active for all user rendering commands, providing a crucial abstraction layer that simplifies vertex attribute management and is essential for the `SituationCreateMesh` and `SituationCmd*` API to function correctly.
 *   4.  **Internal Renderers:** It initializes the library's private rendering modules, such as the 2D quad renderer and the virtual display compositors. These modules create their own private VAOs and shaders to ensure their state does not interfere with the user's global VAO.
 *   5.  **Global UBO:** It creates and binds the global Uniform Buffer Object for per-view data (e.g., camera matrices) to its standard binding point (`SIT_UBO_BINDING_VIEW_DATA`).
 *   6.  **Initial State:** It sets the initial VSync state (`glfwSwapInterval`) and default clear color based on the user's `init_info`.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct, containing user-defined configuration like the VSync hint.
 *
 * @return `SITUATION_SUCCESS` on successful initialization of all OpenGL components.
 * @return An appropriate `SituationError` code if any phase fails (e.g., GLAD fails to load, version is too old, an internal renderer fails to initialize).
 *
 * @note This function is for internal use by `_SituationInitRenderer` only.
 * @warning The creation and binding of the `global_vao_id` is a cornerstone of the OpenGL backend's design. All user-facing mesh and drawing functions rely on this VAO being active.
 *
 * @see _SituationInitRenderer(), _SituationInitQuadRenderer(), _SituationCleanupOpenGL()
 */
// --- Soft Command Buffer Implementation ---

/**
 * @brief [INTERNAL] Allocates a new command packet in the soft buffer.
 * @details Checks for capacity and grows the buffer if necessary using `SIT_REALLOC`.
 *          The growth strategy is geometric (doubling) to minimize allocation frequency.
 *
 * @param buf The soft command buffer to append to.
 * @param opcode The operation code for the new command.
 * @param out_packet Optional output; set to the new packet on success.
 * @return `SITUATION_SUCCESS` or `SITUATION_ERROR_COMMAND_BUFFER_FULL` / `SITUATION_ERROR_MEMORY_ALLOCATION`.
 */
static SituationError _SitGLSoftCmdPush(SituationGLSoftCommandBuffer* buf, SitOpCode opcode, SitCommandPacket** out_packet) {
    if (out_packet) {
        *out_packet = NULL;
    }
    if (!buf) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SitGLSoftCmdPush: buf is NULL.");
    }
    if (buf->is_broken) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_BUFFER_FULL,
            "_SitGLSoftCmdPush: soft buffer broken (prior record failure).");
    }

    if (buf->packet_count >= buf->packet_capacity) {
        size_t new_cap = (buf->packet_capacity == 0) ? 64 : buf->packet_capacity * 2;
#if defined(SITUATION_DEBUG_GL_SOFT_CMD_MAX_PACKETS)
        if (new_cap > (size_t)SITUATION_DEBUG_GL_SOFT_CMD_MAX_PACKETS) {
            if (buf->packet_capacity >= (size_t)SITUATION_DEBUG_GL_SOFT_CMD_MAX_PACKETS) {
                buf->is_broken = true;
                return _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_BUFFER_FULL,
                    "_SitGLSoftCmdPush: debug soft-buffer packet cap reached.");
            }
            new_cap = (size_t)SITUATION_DEBUG_GL_SOFT_CMD_MAX_PACKETS;
        }
#endif
        SitCommandPacket* new_ptr = (SitCommandPacket*)SIT_REALLOC(buf->packets, new_cap * sizeof(SitCommandPacket));
        if (!new_ptr) {
            buf->is_broken = true;
            return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
                "Soft command buffer packets realloc failed.");
        }
        buf->packets = new_ptr;
        buf->packet_capacity = new_cap;
    }

    SitCommandPacket* packet = &buf->packets[buf->packet_count++];
    packet->opcode = opcode;
    if (out_packet) {
        *out_packet = packet;
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Pushes raw data (payload) into the soft buffer's data stream.
 * @details Used for variable-length data like push constants or text strings that don't fit in the fixed-size packet union.
 *          Ensures 8-byte alignment could be added here if needed, but currently packs tightly.
 *
 * @param buf The soft command buffer.
 * @param data Pointer to the source data to copy. If NULL, space is reserved but not written.
 * @param size Size in bytes to allocate/copy.
 * @param out_ptr Optional output pointer to data inside the buffer.
 * @return `SITUATION_SUCCESS` or `SITUATION_ERROR_COMMAND_BUFFER_FULL` / `SITUATION_ERROR_MEMORY_ALLOCATION`.
 */
static SituationError _SitGLSoftDataPush(SituationGLSoftCommandBuffer* buf, const void* data, size_t size, void** out_ptr) {
    if (out_ptr) {
        *out_ptr = NULL;
    }
    if (!buf) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SitGLSoftDataPush: buf is NULL.");
    }
    if (buf->is_broken) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_COMMAND_BUFFER_FULL,
            "_SitGLSoftDataPush: soft buffer broken (prior record failure).");
    }

    if (buf->data_cursor + size > buf->data_capacity) {
        size_t new_cap = (buf->data_capacity == 0) ? 4096 : buf->data_capacity * 2;
        while (buf->data_cursor + size > new_cap) {
            new_cap *= 2;
        }

        uint8_t* new_ptr = (uint8_t*)SIT_REALLOC(buf->data_buffer, new_cap);
        if (!new_ptr) {
            buf->is_broken = true;
            return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION,
                "Soft command buffer data realloc failed.");
        }
        buf->data_buffer = new_ptr;
        buf->data_capacity = new_cap;
    }

    void* dest = buf->data_buffer + buf->data_cursor;
    if (data) {
        memcpy(dest, data, size);
    }
    buf->data_cursor += size;
    if (out_ptr) {
        *out_ptr = dest;
    }
    return SITUATION_SUCCESS;
}

#if defined(SITUATION_USE_OPENGL)
#define SIT_GL_SOFT_CMD_PUSH(buf, opcode, pvar) do { \
    SituationError _sit_gl_push_err_ = _SitGLSoftCmdPush((buf), (opcode), &(pvar)); \
    if (_sit_gl_push_err_ != SITUATION_SUCCESS) return _sit_gl_push_err_; \
} while (0)
#define SIT_GL_SOFT_CMD_PUSH_VOID(buf, opcode, pvar) do { \
    if (_SitGLSoftCmdPush((buf), (opcode), &(pvar)) != SITUATION_SUCCESS) return; \
} while (0)
#define SIT_GL_SOFT_DATA_PUSH(buf, data, size, pvar) do { \
    SituationError _sit_gl_data_err_ = _SitGLSoftDataPush((buf), (data), (size), &(pvar)); \
    if (_sit_gl_data_err_ != SITUATION_SUCCESS) return _sit_gl_data_err_; \
} while (0)
#define SIT_GL_ASYNC_STAGE_IDLE    0u
#define SIT_GL_ASYNC_STAGE_COMPILE 1u
#define SIT_GL_ASYNC_STAGE_SPIRV   2u
#endif

#if defined(SITUATION_USE_OPENGL)
static void _SituationGLApplyBaselineRasterState(void);
static _SituationTextureSlot* _SituationGLFindTextureSlotByGlId(GLuint gl_tex_id);
static SituationError _SituationGLPrepareStorageTextureForSampling(_SituationTextureSlot* slot);

static void _SituationGLDestroyCanvasResources(void) {
    if (sit_render.gl.canvas_fbo != 0) {
        glDeleteFramebuffers(1, &sit_render.gl.canvas_fbo);
        sit_render.gl.canvas_fbo = 0;
    }
    if (sit_render.gl.canvas_color_tex != 0) {
        glDeleteTextures(1, &sit_render.gl.canvas_color_tex);
        sit_render.gl.canvas_color_tex = 0;
    }
    if (sit_render.gl.canvas_depth_rbo != 0) {
        glDeleteRenderbuffers(1, &sit_render.gl.canvas_depth_rbo);
        sit_render.gl.canvas_depth_rbo = 0;
    }
    sit_render.gl.canvas_resource_width = 0;
    sit_render.gl.canvas_resource_height = 0;
}

static SituationError _SituationGLEnsureCanvasResources(void) {
    if (!_SituationRenderCanvasStretchActive()) {
        _SituationGLDestroyCanvasResources();
        return SITUATION_SUCCESS;
    }

    /* Drain stale GL errors; do not early-out — canvas creation retries after mode changes. */
    while (glGetError() != GL_NO_ERROR) { /* drain stale errors */ }

    int cw = sit_gs.render_canvas_width;
    int ch = sit_gs.render_canvas_height;
    static bool _canvas_fbo_fail_logged = false;
    if (cw < 1 || ch < 1) {
        /* [Phase A1 v2.4.320] Diagnostic: dimensions invalid at canvas ensure time. */
        int disp_w = 0, disp_h = 0;
        _SituationGetDisplayPresentSize(&disp_w, &disp_h);
        SituationLog(SIT_LOG_WARNING,
            "_SituationGLEnsureCanvasResources: canvas dimensions invalid (%dx%d), "
            "display=%dx%d main_window=%dx%d monitor=%s",
            cw, ch, disp_w, disp_h,
            sit_gs.main_window_width, sit_gs.main_window_height,
            glfwGetWindowMonitor(sit_gs.sit_glfw_window) ? "exclusive" : "windowed");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (sit_render.gl.canvas_fbo != 0 &&
        sit_render.gl.canvas_resource_width == cw &&
        sit_render.gl.canvas_resource_height == ch) {
        return SITUATION_SUCCESS;
    }

    _SituationGLDestroyCanvasResources();

    glGenTextures(1, &sit_render.gl.canvas_color_tex);
    if (sit_render.gl.canvas_color_tex == 0) {
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }
    glBindTexture(GL_TEXTURE_2D, sit_render.gl.canvas_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cw, ch, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &sit_render.gl.canvas_depth_rbo);
    if (sit_render.gl.canvas_depth_rbo == 0) {
        _SituationGLDestroyCanvasResources();
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }
    glBindRenderbuffer(GL_RENDERBUFFER, sit_render.gl.canvas_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, cw, ch);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &sit_render.gl.canvas_fbo);
    if (sit_render.gl.canvas_fbo == 0) {
        _SituationGLDestroyCanvasResources();
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, sit_render.gl.canvas_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sit_render.gl.canvas_color_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sit_render.gl.canvas_depth_rbo);
    GLenum fbo_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (fbo_status != GL_FRAMEBUFFER_COMPLETE) {
        /* Log once per failure burst; caller retries (Vulkan parity). */
        static bool _canvas_fbo_fail_logged = false;
        if (!_canvas_fbo_fail_logged) {
            int disp_w = 0, disp_h = 0;
            _SituationGetDisplayPresentSize(&disp_w, &disp_h);
            GLenum gl_err = glGetError();
            SituationLog(SIT_LOG_DEBUG,
                "_SituationGLEnsureCanvasResources FAILED: fbo_status=0x%04X glError=0x%04X "
                "canvas=%dx%d display=%dx%d render_canvas=%dx%d monitor=%s",
                (unsigned)fbo_status, (unsigned)gl_err,
                cw, ch, disp_w, disp_h,
                sit_gs.render_canvas_width, sit_gs.render_canvas_height,
                glfwGetWindowMonitor(sit_gs.sit_glfw_window) ? "exclusive" : "windowed");
            _canvas_fbo_fail_logged = true;
        }
        _SituationGLDestroyCanvasResources();
        return SITUATION_ERROR_RENDER_COMMAND_FAILED;
    }

    sit_render.gl.canvas_resource_width = cw;
    sit_render.gl.canvas_resource_height = ch;
    _canvas_fbo_fail_logged = false; /* Reset: next failure sequence will log once. */
    return SITUATION_SUCCESS;
}

static bool _SituationGLPrepareCanvasStretchTarget(void) {
    if (!_SituationRenderCanvasStretchActive()) {
        return true;
    }
    for (int attempt = 0; attempt < 8; attempt++) {
        if (_SituationGLEnsureCanvasResources() == SITUATION_SUCCESS &&
            sit_render.gl.canvas_fbo != 0) {
            return true;
        }
#if !defined(__STDC_NO_THREADS__)
        thrd_yield();
#endif
    }
    return false;
}

static void _SituationGLBlitCanvasToDisplay(void) {
    if (!_SituationRenderCanvasStretchActive() || sit_render.gl.canvas_fbo == 0) {
        return;
    }
    int dw = 0;
    int dh = 0;
    _SituationGetDisplayPresentSize(&dw, &dh);
    int cw = sit_render.gl.canvas_resource_width;
    int ch = sit_render.gl.canvas_resource_height;
    if (dw < 1 || dh < 1 || cw < 1 || ch < 1) {
        return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sit_render.gl.canvas_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, cw, ch, 0, 0, dw, dh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

 static void _SituationGLFlipScreenshotRowsTopLeft(uint8_t* rgba, int width, int height) {
    if (!rgba || width < 1 || height < 1) {
        return;
    }
    const size_t row_bytes = (size_t)width * 4u;
    for (int y = 0; y < height / 2; ++y) {
        uint8_t* row_top = rgba + (size_t)y * row_bytes;
        uint8_t* row_bot = rgba + (size_t)(height - 1 - y) * row_bytes;
        for (size_t i = 0; i < row_bytes; ++i) {
            uint8_t tmp = row_top[i];
            row_top[i] = row_bot[i];
            row_bot[i] = tmp;
        }
    }
}
#endif

/**
 * @brief [INTERNAL] Replays the soft command buffer to the OpenGL driver.
 * @details This is the "Consumer" phase of the deferred rendering model. It iterates through the recorded packets
 *          and issues the corresponding `gl*` calls.
 *
 *          **Key Responsibilities:**
 *          - State Translation: Maps abstract opcodes to specific OpenGL functions.
 *          - Resource Binding: Handles VAO/VBO/UBO binding.
 *          - Draw Calls: Issues `glDrawArrays` / `glDrawElements`.
 *          - **State Restoration:** Critically, after operations that modify global state (like `SIT_OP_DRAW_MESH` changing the VAO),
 *            it restores the "Global VAO" (`sit_render.gl.global_vao_id`) to ensure subsequent commands work as expected.
 *
 * @param buf The soft command buffer to execute.
 */
static SituationError _SituationGLExecuteCommands(SituationGLSoftCommandBuffer* buf, int frame_index) {
    if (!buf) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationGLExecuteCommands: buf is NULL.");
    }
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: ENTRY, buf=%p, frame_index=%d, packet_count=%d\n", 
           (void*)buf, frame_index, buf ? buf->packet_count : -1);
    fflush(stdout);
    #endif
    SIT_DEBUG_LOG("[GLExecute] START: packet_count=%d\n", buf->packet_count);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: After SIT_DEBUG_LOG\n");
    fflush(stdout);
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Checking packet_count=%d\n", buf->packet_count);
    fflush(stdout);
    printf("[OpenGL Debug] _SituationGLExecuteCommands: About to check if packet_count == 0\n");
    fflush(stdout);
    #endif
    if (buf->packet_count == 0) return SITUATION_SUCCESS;
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: packet_count > 0, continuing\n");
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Checking is_broken=%d\n", buf->is_broken);
    fflush(stdout);
    #endif
    // [FIX v2.3.27B] Do not execute incomplete/corrupted buffers
    if (buf->is_broken) {
        SituationLog(SIT_LOG_WARNING, "_SituationGLExecuteCommands: skipped broken soft buffer (packet_count=%d)",
            (int)buf->packet_count);
        buf->packet_count = 0;
        buf->data_cursor = 0;
        buf->is_broken = false;
        buf->recording_render_pass_active = false;
        _SitVDResetGLRecordingState(buf);
        return _SituationSetErrorFromCode(SITUATION_ERROR_RENDER_COMMAND_FAILED,
            "_SituationGLExecuteCommands: skipped broken soft buffer.");
    }

    sit_render.gl.bound_ibo_byte_offset = 0;
    sit_render.gl.current_index_type = GL_UNSIGNED_INT;
    sit_render.gl.bound_ibo_index_element_size = sizeof(uint32_t);

    // [v2.3.31] Optimization: Track bound texture locally to avoid glGetIntegerv stalls in draw calls
    // GLuint current_bound_texture_id = 0; // REPLACED by sit_render.gl.current_bound_texture_id

    static int cached_w = 0;
    static int cached_h = 0;

    // If the window resized, rebuild the Render Thread's target FBO resources safely
    {
        if (sit_render.gl.shadow_state_dirty && _SituationRenderCanvasStretchActive()) {
            _SituationGLDestroyCanvasResources();
        }

        int ortho_w = SituationGetRenderWidth();
        int ortho_h = SituationGetRenderHeight();
        if (ortho_w < 1) ortho_w = sit_gs.main_window_width;
        if (ortho_h < 1) ortho_h = sit_gs.main_window_height;
        if (cached_w != ortho_w || cached_h != ortho_h || sit_render.gl.shadow_state_dirty) {
            cached_w = ortho_w;
            cached_h = ortho_h;

            glm_ortho(0.0f, (float)cached_w, (float)cached_h, 0.0f, -1.0f, 1.0f, sit_render.gl.vd_ortho_projection);

            if (sit_render.gl.composite_copy_texture_id != 0) {
                glBindTexture(GL_TEXTURE_2D, sit_render.gl.composite_copy_texture_id);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cached_w, cached_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            if (sit_render.gl.quad_shader_program) {
                glProgramUniformMatrix4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)sit_render.gl.vd_ortho_projection);
            }

            sit_render.gl.shadow_state_dirty = false;
        }
    }

    // --- [v2.3.27] State Hardening: Reset critical state ---
    // We cannot assume the state from the previous frame persists,
    // because external code (ImGui, etc.) might have run in between.

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: About to reset GL state\n");
    fflush(stdout);
    #endif

    // 1. Reset Capabilities
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glEnable(GL_DEPTH_TEST)\n");
    fflush(stdout);
    #endif
    glEnable(GL_DEPTH_TEST);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glDepthFunc(GL_LESS)\n");
    fflush(stdout);
    #endif
    glDepthFunc(GL_LESS);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glEnable(GL_CULL_FACE)\n");
    fflush(stdout);
    #endif
    glEnable(GL_CULL_FACE);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glCullFace(GL_BACK)\n");
    fflush(stdout);
    #endif
    glCullFace(GL_BACK);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glDisable(GL_BLEND)\n");
    fflush(stdout);
    #endif
    glDisable(GL_BLEND); // Default to opaque
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glDisable(GL_SCISSOR_TEST)\n");
    fflush(stdout);
    #endif
    glDisable(GL_SCISSOR_TEST);

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: GL capability reset complete\n");
    fflush(stdout);
    #endif

    // 2. Reset shadow cache (must precede baseline so tracked state matches GL)
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Resetting shadow cache\n");
    fflush(stdout);
    #endif
    sit_render.gl.current_program_id = 0;
    sit_render.gl.current_vao_id = 0;
    sit_render.gl.current_fbo_id = 0;
    sit_render.gl.current_bound_texture_id = 0;
    sit_render.gl.blend_enabled = -1; // Force re-application if command requests it

    _SituationGLApplyBaselineRasterState();

    /* Drain stale errors from prior-frame present, screenshot readback, or canvas blit so the
     * per-packet glGetError check is not attributed to the first opcode this frame (Track D). */
    while (glGetError() != GL_NO_ERROR) { /* drain */ }

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: GL baseline + shadow sync complete\n");
    fflush(stdout);
    #endif

    // 4. Set MDI Offset based on frame index (Double/Triple Buffering)
    // Each frame gets a dedicated 1MB slice of the MDI ring buffer.
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Setting MDI offset, frame_index=%d\n", frame_index);
    fflush(stdout);
    #endif
    size_t mdi_frame_offset = (frame_index % SITUATION_MAX_FRAMES_IN_FLIGHT) * (1024 * 1024);
    atomic_store(&sit_render.gl.mdi_ring_head, mdi_frame_offset);

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: MDI offset set, entering execution loop\n");
    fflush(stdout);
    #endif

    // Deferred GL: packet order defines pass boundaries at execute time (not at record time).
    bool exec_inside_render_pass = false;
    int exec_pass_display_id = -1;
    bool exec_pass_had_draw = false;
    _SitGLRasterStackEntry exec_raster_stack[SITUATION_MAX_RASTER_STACK_DEPTH];
    int exec_raster_stack_depth = 0;
    SituationRendererBehaviorPolicy exec_behavior = {0};
    SituationRendererBehaviorPolicy exec_behavior_stack[SITUATION_MAX_BEHAVIOR_STACK_DEPTH];
    int exec_behavior_stack_depth = 0;

    // --- Execution Loop ---
    for (size_t i = 0; i < buf->packet_count; ++i) {
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: Processing packet %zu\n", i);
        fflush(stdout);
        #endif
        SitCommandPacket* p = &buf->packets[i];
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: Got packet pointer, opcode=%d\n", p->opcode);
        fflush(stdout);
        #endif
        SIT_DEBUG_LOG("[GLExecute] Processing packet %zu, opcode=%d\n", i, p->opcode);
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: Entering switch statement\n");
        fflush(stdout);
        #endif
        switch (p->opcode) {
            case SIT_OP_BEGIN_RENDER_PASS:
                {
                    exec_pass_display_id = p->args.begin_pass.display_id;
                    exec_pass_had_draw = false;
                    exec_inside_render_pass = false;

                    const SituationRenderPassInfo* pass_info = &p->args.begin_pass.info;
                    if (_SitRenderPassInfoUsesRenderTarget(pass_info)) {
                        _SituationRenderTargetSlot* rts = _SitGetRenderTargetSlot(pass_info->render_target);
                        if (rts) {
                            glBindFramebuffer(GL_FRAMEBUFFER, rts->fbo_id);
                            sit_render.gl.current_fbo_id = rts->fbo_id;
                            glViewport(0, 0, rts->width, rts->height);
                            sit_render.gl.current_target_width = rts->width;
                            sit_render.gl.current_target_height = rts->height;
                        }
                    } else if (p->args.begin_pass.display_id < 0) {
                        if (_SituationRenderCanvasStretchActive()) {
                            if (!_SituationGLPrepareCanvasStretchTarget()) {
                                _SituationSetErrorFromCode(
                                    SITUATION_ERROR_DISPLAY_MODE_SETTLING,
                                    "Exclusive fullscreen canvas unavailable; retry next frame");
                                break;
                            }
                            glBindFramebuffer(GL_FRAMEBUFFER, sit_render.gl.canvas_fbo);
                            sit_render.gl.current_fbo_id = sit_render.gl.canvas_fbo;
                            ma_mutex_lock(&sit_gs.error_mutex);
                            if (sit_gs.last_error_code == SITUATION_ERROR_DISPLAY_MODE_SETTLING) {
                                sit_gs.last_error_code = SITUATION_SUCCESS;
                            }
                            ma_mutex_unlock(&sit_gs.error_mutex);
                        } else {
                            glBindFramebuffer(GL_FRAMEBUFFER, 0);
                            sit_render.gl.current_fbo_id = 0;
                        }
                    } else {
                        int did = p->args.begin_pass.display_id;
                        if (did < SITUATION_MAX_VIRTUAL_DISPLAYS && sit_render.virtual_display_slots_used[did]) {
                            SituationVirtualDisplay* vd = &sit_render.virtual_display_slots[did];
                            glBindFramebuffer(GL_FRAMEBUFFER, SitVDGl(vd)->fbo_id);
                            sit_render.gl.current_fbo_id = SitVDGl(vd)->fbo_id;
                            glViewport(0, 0, (GLsizei)vd->resolution.x, (GLsizei)vd->resolution.y);
                            sit_render.gl.current_target_width = (int)vd->resolution.x;
                            sit_render.gl.current_target_height = (int)vd->resolution.y;
                        }
                    }

                    if (_SitRenderPassInfoUsesRenderTarget(pass_info)) {
                        _SituationRenderTargetSlot* rts = _SitGetRenderTargetSlot(pass_info->render_target);
                        if (!rts) {
                            break;
                        }
                    } else if (p->args.begin_pass.display_id >= 0 &&
                        (p->args.begin_pass.display_id >= SITUATION_MAX_VIRTUAL_DISPLAYS ||
                         !sit_render.virtual_display_slots_used[p->args.begin_pass.display_id])) {
                        break;
                    }
                    if (!_SitRenderPassInfoUsesRenderTarget(pass_info) &&
                        p->args.begin_pass.display_id < 0 &&
                        _SituationRenderCanvasStretchActive() &&
                        sit_render.gl.canvas_fbo == 0) {
                        break;
                    }

                    exec_inside_render_pass = true;

                    if (p->args.begin_pass.display_id < 0 && !_SitRenderPassInfoUsesRenderTarget(pass_info)) {
                        glViewport(0, 0, p->args.begin_pass.target_w, p->args.begin_pass.target_h);
                        sit_render.gl.current_target_width = p->args.begin_pass.target_w;
                        sit_render.gl.current_target_height = p->args.begin_pass.target_h;
                    }

                    if (sit_render.gl.current_target_width < 1) sit_render.gl.current_target_width = sit_gs.main_window_width;
                    if (sit_render.gl.current_target_height < 1) sit_render.gl.current_target_height = sit_gs.main_window_height;
                    if (sit_render.gl.quad_shader_program) {
                        mat4 pass_proj;
                        glm_ortho(0.0f, (float)sit_render.gl.current_target_width, (float)sit_render.gl.current_target_height, 0.0f, -1.0f, 1.0f, pass_proj);
                        glProgramUniformMatrix4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)pass_proj);
                        /* [Bug B2] Reset leaked tint/uniform state before textured draws in this pass. */
                        glProgramUniform4f(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_OBJECT_COLOR, 1.0f, 1.0f, 1.0f, 1.0f);
                        glProgramUniform4f(sit_render.gl.quad_shader_program, 5, 0.0f, 0.0f, 1.0f, 1.0f);
                    }

                    /* Depth clears are ignored if GL_DEPTH_WRITEMASK is false; previous passes may leave it off. */
                    glDepthMask(GL_TRUE);
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glDisable(GL_SCISSOR_TEST);
                    sit_render.gl.scissor_test_enabled = 0;

                    /* [Phase B3] Release leaked image bindings before graphics sampling (order-dependent after compute tests). */
                    if (p->args.begin_pass.display_id < 0 && !_SitRenderPassInfoUsesRenderTarget(pass_info)) {
                        for (int image_unit = 0; image_unit < 8; ++image_unit) {
                            glBindImageTexture((GLuint)image_unit, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
                        }
                        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
                    }

                    GLbitfield clear_mask = 0;
                    bool vd_pass_srgb = false;
                    if (!_SitRenderPassInfoUsesRenderTarget(pass_info) && p->args.begin_pass.display_id >= 0) {
                        int did = p->args.begin_pass.display_id;
                        if (did < SITUATION_MAX_VIRTUAL_DISPLAYS && sit_render.virtual_display_slots_used[did]) {
                            vd_pass_srgb = (sit_render.virtual_display_slots[did].color_format == SIT_VD_FORMAT_RGBA8_SRGB);
                        }
                    }
                    if (vd_pass_srgb) {
                        glEnable(GL_FRAMEBUFFER_SRGB);
                    } else {
                        glDisable(GL_FRAMEBUFFER_SRGB);
                    }
                    if (p->args.begin_pass.info.color_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                        ColorRGBA c = p->args.begin_pass.info.color_attachment.clear.color;
                        float clear_rgba[4];
                        const bool hdr_clear = (p->args.begin_pass.display_id < 0) && (sit_render.output_hdr_active != 0);
                        _SituationColorRgbaToClearFloats(c, hdr_clear, clear_rgba);
                        glClearColor(clear_rgba[0], clear_rgba[1], clear_rgba[2], clear_rgba[3]);
                        clear_mask |= GL_COLOR_BUFFER_BIT;
                    }
                    if (p->args.begin_pass.info.stencil_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                        /* Deferred until the renderer exposes/creates stencil attachments consistently. */
                    }
                    bool target_has_depth = (p->args.begin_pass.display_id < 0 && !_SitRenderPassInfoUsesRenderTarget(pass_info));
                    if (_SitRenderPassInfoUsesRenderTarget(pass_info)) {
                        _SituationRenderTargetSlot* rts = _SitGetRenderTargetSlot(pass_info->render_target);
                        if (rts) {
                            target_has_depth = rts->has_depth;
                        }
                    } else if (p->args.begin_pass.display_id >= 0) {
                        int did = p->args.begin_pass.display_id;
                        if (did < SITUATION_MAX_VIRTUAL_DISPLAYS && sit_render.virtual_display_slots_used[did]) {
                            target_has_depth = _SitVDHasDepthAttachment(&sit_render.virtual_display_slots[did]);
                        }
                    }
                    if (target_has_depth &&
                        p->args.begin_pass.info.depth_attachment.loadOp == SIT_LOAD_OP_CLEAR) {
                        glClearDepth(p->args.begin_pass.info.depth_attachment.clear.depth);
                        clear_mask |= GL_DEPTH_BUFFER_BIT;
                    }
                    if (clear_mask) glClear(clear_mask);

                    if (target_has_depth) {
                        glEnable(GL_DEPTH_TEST);
                        sit_render.gl.depth_test_enabled = true;
                    } else {
                        glDisable(GL_DEPTH_TEST);
                        sit_render.gl.depth_test_enabled = false;
                    }
                }
                break;

            case SIT_OP_CLEAR:
                {
                    const uint32_t flags = p->args.clear.flags;
                    const SituationClearValue* value = &p->args.clear.value;

                    if (flags & SIT_CLEAR_COLOR_BIT) {
                        const GLfloat c[4] = {
                            value->color.r / 255.0f,
                            value->color.g / 255.0f,
                            value->color.b / 255.0f,
                            value->color.a / 255.0f
                        };
                        glClearBufferfv(GL_COLOR, 0, c);
                    }

                    if ((flags & SIT_CLEAR_DEPTH_BIT) && (flags & SIT_CLEAR_STENCIL_BIT)) {
                        glClearBufferfv(GL_DEPTH, 0, &value->depth);
                        glStencilMask(0xFFFFFFFFu);
                        glClearStencil((GLint)value->stencil);
                        glClear(GL_STENCIL_BUFFER_BIT);
                    } else if (flags & SIT_CLEAR_DEPTH_BIT) {
                        glClearBufferfv(GL_DEPTH, 0, &value->depth);
                    } else if (flags & SIT_CLEAR_STENCIL_BIT) {
                        glStencilMask(0xFFFFFFFFu);
                        glClearStencil((GLint)value->stencil);
                        glClear(GL_STENCIL_BUFFER_BIT);
                    }
                }
                break;

            case SIT_OP_END_RENDER_PASS:
                glDisable(GL_FRAMEBUFFER_SRGB);
                _SitVDEndRenderPassCheck(exec_pass_display_id, exec_pass_had_draw);
                exec_pass_display_id = -1;
                exec_pass_had_draw = false;
                exec_inside_render_pass = false;
                #ifdef SITUATION_OPENGL_DEBUG
                printf("[OpenGL Debug] _SituationGLExecuteCommands: In END_RENDER_PASS case\n");
                fflush(stdout);
                #endif
                SIT_DEBUG_LOG("[GLExecute] END_RENDER_PASS: About to unbind framebuffer\n");
                #ifdef SITUATION_OPENGL_DEBUG
                printf("[OpenGL Debug] _SituationGLExecuteCommands: Calling glBindFramebuffer\n");
                fflush(stdout);
                #endif
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                #ifdef SITUATION_OPENGL_DEBUG
                printf("[OpenGL Debug] _SituationGLExecuteCommands: glBindFramebuffer returned\n");
                fflush(stdout);
                #endif
                SIT_DEBUG_LOG("[GLExecute] END_RENDER_PASS: Framebuffer unbound\n");
                #ifdef SITUATION_OPENGL_DEBUG
                printf("[OpenGL Debug] _SituationGLExecuteCommands: About to break from END_RENDER_PASS\n");
                fflush(stdout);
                #endif
                break;

            case SIT_OP_SET_CULL_MODE:
                if (p->args.set_cull_mode.mode == SIT_CULL_NONE) {
                    glDisable(GL_CULL_FACE);
                    sit_render.gl.cull_face_enabled = 0;
                } else {
                    glEnable(GL_CULL_FACE);
                    sit_render.gl.cull_face_enabled = 1;
                    if (p->args.set_cull_mode.mode == SIT_CULL_BACK) glCullFace(GL_BACK);
                    else if (p->args.set_cull_mode.mode == SIT_CULL_FRONT) glCullFace(GL_FRONT);
                }
                break;

            case SIT_OP_SET_FRONT_FACE:
                if (p->args.set_front_face.front_face == SIT_FRONT_FACE_CW) glFrontFace(GL_CW);
                else glFrontFace(GL_CCW);
                break;

            case SIT_OP_SET_PRIMITIVE_TOPOLOGY:
                sit_render.gl.current_primitive_mode_set = true;
                switch (p->args.set_primitive_topology.topology) {
                    case SIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:  sit_render.gl.current_primitive_mode = GL_TRIANGLES; break;
                    case SIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: sit_render.gl.current_primitive_mode = GL_TRIANGLE_STRIP; break;
                    case SIT_PRIMITIVE_TOPOLOGY_LINE_LIST:      sit_render.gl.current_primitive_mode = GL_LINES; break;
                    case SIT_PRIMITIVE_TOPOLOGY_LINE_STRIP:     sit_render.gl.current_primitive_mode = GL_LINE_STRIP; break;
                    case SIT_PRIMITIVE_TOPOLOGY_POINT_LIST:
                        sit_render.gl.current_primitive_mode = GL_POINTS;
                        glEnable(GL_PROGRAM_POINT_SIZE);
                        break;
                    default:                                     sit_render.gl.current_primitive_mode = GL_TRIANGLES; break;
                }
                break;

            case SIT_OP_SET_POLYGON_MODE:
                switch (p->args.set_polygon_mode.mode) {
                    case SIT_POLYGON_MODE_LINE:
                        sit_render.gl.current_polygon_mode = GL_LINE;
                        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                        break;
                    case SIT_POLYGON_MODE_POINT:
                        sit_render.gl.current_polygon_mode = GL_POINT;
                        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
                        break;
                    default:
                        sit_render.gl.current_polygon_mode = GL_FILL;
                        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                        break;
                }
                break;

            case SIT_OP_SET_DEPTH_BIAS:
                if (p->args.set_depth_bias.enable) {
                    glEnable(GL_POLYGON_OFFSET_FILL);
                    sit_render.gl.polygon_offset_enabled = 1;
                    glPolygonOffset(p->args.set_depth_bias.slope_factor, p->args.set_depth_bias.constant_factor);
                } else {
                    glDisable(GL_POLYGON_OFFSET_FILL);
                    sit_render.gl.polygon_offset_enabled = 0;
                }
                break;

            case SIT_OP_SET_LINE_WIDTH:
                glLineWidth(p->args.set_line_width.width);
                break;

            case SIT_OP_SET_COLOR_WRITE_MASK:
                glColorMask(
                    p->args.set_color_write_mask.r ? GL_TRUE : GL_FALSE,
                    p->args.set_color_write_mask.g ? GL_TRUE : GL_FALSE,
                    p->args.set_color_write_mask.b ? GL_TRUE : GL_FALSE,
                    p->args.set_color_write_mask.a ? GL_TRUE : GL_FALSE);
                break;

            case SIT_OP_SET_STENCIL_TEST:
                if (p->args.set_stencil_test.enable) {
                    glEnable(GL_STENCIL_TEST);
                    _SitGLApplyStencilFace(GL_FRONT, &p->args.set_stencil_test.front);
                    _SitGLApplyStencilFace(GL_BACK, &p->args.set_stencil_test.back);
                } else {
                    glDisable(GL_STENCIL_TEST);
                }
                break;

            case SIT_OP_SET_MULTISAMPLE_STATE:
                {
                    const SituationMultisampleState* ms = &p->args.set_multisample_state.ms;
                    if (ms->sample_shading_enable) {
                        glEnable(GL_SAMPLE_SHADING);
                        glMinSampleShading(ms->min_sample_shading);
                    } else {
                        glDisable(GL_SAMPLE_SHADING);
                    }
                    // sample_mask == 0 means "all samples" — map to ~0u for the GL call
                    GLuint mask = ms->sample_mask ? (GLuint)ms->sample_mask : 0xFFFFFFFFu;
                    glSampleMaski(0, mask);
                    if (ms->alpha_to_coverage_enable) {
                        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
                    } else {
                        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
                    }
                }
                break;

            case SIT_OP_SET_DEPTH_TEST:
                {
                    if (p->args.set_depth_test.enable) {
                        glEnable(GL_DEPTH_TEST);
                        sit_render.gl.depth_test_enabled = 1;
                    } else {
                        glDisable(GL_DEPTH_TEST);
                        sit_render.gl.depth_test_enabled = 0;
                    }
                    GLenum gl_op = GL_LESS;
                    switch (p->args.set_depth_test.depth_op) {
                        case SIT_DEPTH_COMPARE_ALWAYS:   gl_op = GL_ALWAYS; break;
                        case SIT_DEPTH_COMPARE_LESS:     gl_op = GL_LESS; break;
                        case SIT_DEPTH_COMPARE_LEQUAL:   gl_op = GL_LEQUAL; break;
                        case SIT_DEPTH_COMPARE_GREATER:  gl_op = GL_GREATER; break;
                        case SIT_DEPTH_COMPARE_GEQUAL:   gl_op = GL_GEQUAL; break;
                        case SIT_DEPTH_COMPARE_EQUAL:    gl_op = GL_EQUAL; break;
                        case SIT_DEPTH_COMPARE_NOTEQUAL: gl_op = GL_NOTEQUAL; break;
                        case SIT_DEPTH_COMPARE_NEVER:    gl_op = GL_NEVER; break;
                    }
                    glDepthFunc(gl_op);
                }
                break;

            case SIT_OP_SET_DEPTH_WRITE:
                glDepthMask(p->args.set_depth_write.enable ? GL_TRUE : GL_FALSE);
                break;

            case SIT_OP_SET_BLEND_ENABLE:
                if (p->args.set_blend_enable.enable) {
                    glEnable(GL_BLEND);
                    sit_render.gl.blend_enabled = 1;
                } else {
                    glDisable(GL_BLEND);
                    sit_render.gl.blend_enabled = 0;
                }
                break;

            case SIT_OP_SET_BLEND_FUNC_SEPARATE:
                {
                    GLenum srgb = GL_ZERO, drgb = GL_ZERO, sa = GL_ZERO, da = GL_ZERO;
#define SIT_MAP_BLEND(bf, out_gl) \
    switch(bf) { \
        case SIT_BLEND_ZERO: out_gl = GL_ZERO; break; \
        case SIT_BLEND_ONE: out_gl = GL_ONE; break; \
        case SIT_BLEND_SRC_COLOR: out_gl = GL_SRC_COLOR; break; \
        case SIT_BLEND_ONE_MINUS_SRC_COLOR: out_gl = GL_ONE_MINUS_SRC_COLOR; break; \
        case SIT_BLEND_DST_COLOR: out_gl = GL_DST_COLOR; break; \
        case SIT_BLEND_ONE_MINUS_DST_COLOR: out_gl = GL_ONE_MINUS_DST_COLOR; break; \
        case SIT_BLEND_SRC_ALPHA: out_gl = GL_SRC_ALPHA; break; \
        case SIT_BLEND_ONE_MINUS_SRC_ALPHA: out_gl = GL_ONE_MINUS_SRC_ALPHA; break; \
        case SIT_BLEND_DST_ALPHA: out_gl = GL_DST_ALPHA; break; \
        case SIT_BLEND_ONE_MINUS_DST_ALPHA: out_gl = GL_ONE_MINUS_DST_ALPHA; break; \
        default: out_gl = GL_ZERO; break; \
    }
                    SIT_MAP_BLEND(p->args.set_blend_func.src_rgb, srgb);
                    SIT_MAP_BLEND(p->args.set_blend_func.dst_rgb, drgb);
                    SIT_MAP_BLEND(p->args.set_blend_func.src_a, sa);
                    SIT_MAP_BLEND(p->args.set_blend_func.dst_a, da);
#undef SIT_MAP_BLEND
                    glBlendFuncSeparate(srgb, drgb, sa, da);
                    
                    sit_render.gl.blend_src_rgb = srgb;
                    sit_render.gl.blend_dst_rgb = drgb;
                    sit_render.gl.blend_src_alpha = sa;
                    sit_render.gl.blend_dst_alpha = da;
                }
                break;

            case SIT_OP_PUSH_RASTER_STATE:
                if (exec_raster_stack_depth < SITUATION_MAX_RASTER_STACK_DEPTH) {
                    _SitGLCaptureRasterState(&exec_raster_stack[exec_raster_stack_depth++]);
                }
                break;

            case SIT_OP_POP_RASTER_STATE:
                if (exec_raster_stack_depth > 0) {
                    _SitGLApplyRasterState(&exec_raster_stack[--exec_raster_stack_depth]);
                }
                break;

            case SIT_OP_SET_RENDERER_BEHAVIOR:
                exec_behavior = p->args.set_renderer_behavior.policy;
                break;

            case SIT_OP_PUSH_RENDERER_BEHAVIOR:
                if (exec_behavior_stack_depth < SITUATION_MAX_BEHAVIOR_STACK_DEPTH) {
                    exec_behavior_stack[exec_behavior_stack_depth++] = exec_behavior;
                }
                break;

            case SIT_OP_POP_RENDERER_BEHAVIOR:
                if (exec_behavior_stack_depth > 0) {
                    exec_behavior = exec_behavior_stack[--exec_behavior_stack_depth];
                }
                break;
                
            case SIT_OP_BEGIN_DEBUG_GROUP:
                if (glPushDebugGroup) {
                    const char* name = (const char*)(buf->data_buffer + p->args.begin_debug_group.name_offset);
                    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
                }
                break;

            case SIT_OP_END_DEBUG_GROUP:
                if (glPopDebugGroup) {
                    glPopDebugGroup();
                }
                break;

            case SIT_OP_SET_PUSH_CONSTANT_DATA:
                // Not implemented for OpenGL yet.
                break;

            case SIT_OP_SET_VIEWPORT:
                if (p->args.viewport.index == 0) {
                    glViewport((GLint)p->args.viewport.x, (GLint)p->args.viewport.y,
                               (GLsizei)p->args.viewport.w, (GLsizei)p->args.viewport.h);
                } else {
                    glViewportIndexedf(p->args.viewport.index, p->args.viewport.x, p->args.viewport.y,
                                       p->args.viewport.w, p->args.viewport.h);
                }
                break;

            case SIT_OP_SET_SCISSOR:
                {
                    /* API uses top-left (same as ortho/text); GL scissor origin is bottom-left. */
                    glEnable(GL_SCISSOR_TEST);
                    int th = sit_render.gl.current_target_height;
                    if (th < 1) {
                        th = sit_gs.main_window_height;
                    }
                    if (th < 1) {
                        th = 1;
                    }
                    GLint gl_y = (GLint)th - p->args.scissor.y - p->args.scissor.h;
                    if (p->args.scissor.index == 0) {
                        glScissor(p->args.scissor.x, gl_y, p->args.scissor.w, p->args.scissor.h);
                    } else {
                        glScissorIndexed(p->args.scissor.index, p->args.scissor.x, gl_y,
                                         p->args.scissor.w, p->args.scissor.h);
                    }
                }
                break;

            case SIT_OP_BIND_PIPELINE:
                glUseProgram((GLuint)p->args.bind_pipeline.shader_id);
                sit_render.gl.current_program_id = (GLuint)p->args.bind_pipeline.shader_id;

                // [Phase 5] Update Virtual Bindless Cache
                if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
                    sit_render.gl.current_virtual_loc = glGetUniformLocation(sit_render.gl.current_program_id, "_sit_texture_slot_id");
                }
                break;

            case SIT_OP_DRAW_MESH:
                {
                    if (exec_pass_display_id >= 0) {
                        exec_pass_had_draw = true;
                    }
                    GLuint vao = _SitGLGetCachedVAO(p->args.draw_mesh.mesh);
                    if (vao == 0) break;

                    glBindVertexArray(vao);
                    sit_render.gl.current_vao_id = vao;

                    // --- Batch detection (VAO must match) ---
                    // Note: Pipeline changes are implicitly handled because they are distinct opcodes (SIT_OP_BIND_PIPELINE),
                    // which will cause the lookahead loop to break immediately.
                    size_t batch_start = i;
                    GLuint first_vao = vao;

                    size_t lookahead = i + 1;
                    while (lookahead < buf->packet_count) {
                        SitCommandPacket* next = &buf->packets[lookahead];
                        if (next->opcode != SIT_OP_DRAW_MESH) break;

                        if (_SitGLGetCachedVAO(next->args.draw_mesh.mesh) != first_vao) break;
                        if (next->args.draw_mesh.shader_id != sit_render.gl.current_program_id) break;

                        lookahead++;
                    }

                    size_t batch_size = lookahead - i;

                    // Tune threshold: Batching has overhead. Start with >= 8.
                    if (batch_size >= 8 && sit_render.gl.mdi_data_ptr) {
                        size_t cmd_size = sizeof(_SituationGLDrawElementsIndirectCommand);
                        size_t total_size = batch_size * cmd_size;

                        // Atomic reservation in ring buffer
                        size_t offset = atomic_fetch_add(&sit_render.gl.mdi_ring_head, total_size);

                        // Safety check: Ensure we stay within the CURRENT FRAME's slice (1MB per frame)
                        if (offset >= mdi_frame_offset && (offset + total_size <= mdi_frame_offset + (1024 * 1024))) {
                            _SituationGLDrawElementsIndirectCommand* cmds = (_SituationGLDrawElementsIndirectCommand*)((uint8_t*)sit_render.gl.mdi_data_ptr + offset);

                            for (size_t k = 0; k < batch_size; ++k) {
                                SitCommandPacket* bp = &buf->packets[i + k];
                                struct _SituationMeshSlot* slot = _SitGetMeshSlot(bp->args.draw_mesh.mesh);
                                if (slot) {
                                    cmds[k].count         = (GLuint)slot->index_count;
                                    cmds[k].instanceCount = 1;          // change if real instancing is added
                                    cmds[k].firstIndex    = 0;
                                    cmds[k].baseVertex    = 0;
                                    cmds[k].baseInstance  = 0;
                                } else {
                                    cmds[k].count = 0;
                                    cmds[k].instanceCount = 0;
                                }
                            }

                            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, sit_render.gl.mdi_buffer_id);
                            glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (const void*)((uintptr_t)offset), (GLsizei)batch_size, 0);
                            SIT_CHECK_GL_ERROR();

                            // Skip the batched commands
                            i += (batch_size - 1);
                        } else {
                            // Fallback single draw (buffer full)
                            struct _SituationMeshSlot* slot = _SitGetMeshSlot(p->args.draw_mesh.mesh);
                            if (slot) {
                                int single_tri = (slot->index_count == 3 && slot->vertex_count == 3);
                                if (single_tri) {
                                    /* Fullscreen-style passes (e.g. fps_ray_demo skydome): ignore depth so stale
                                     * or driver-quirky depth buffer cannot reject the whole triangle. */
                                    glDisable(GL_CULL_FACE);
                                    glDisable(GL_DEPTH_TEST);
                                }
                                glDrawElements(GL_TRIANGLES, slot->index_count, GL_UNSIGNED_INT, NULL);
                                if (single_tri) {
                                    glEnable(GL_DEPTH_TEST);
                                    glEnable(GL_CULL_FACE);
                                }
                            }
                        }
                    } else {
                        // Single draw fallback
                        struct _SituationMeshSlot* slot = _SitGetMeshSlot(p->args.draw_mesh.mesh);
                        if (slot) {
                            int single_tri = (slot->index_count == 3 && slot->vertex_count == 3);
                            if (single_tri) {
                                glDisable(GL_CULL_FACE);
                                glDisable(GL_DEPTH_TEST);
                            }
                            glDrawElements(GL_TRIANGLES, slot->index_count, GL_UNSIGNED_INT, NULL);
                            if (single_tri) {
                                glEnable(GL_DEPTH_TEST);
                                glEnable(GL_CULL_FACE);
                            }
                        }
                    }

                    // [CRITICAL] Restore global VAO state for subsequent generic draw calls
                    if (sit_render.gl.global_vao_id != 0) {
                        glBindVertexArray(sit_render.gl.global_vao_id);
                        sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                    }
                }
                break;

            case SIT_OP_DRAW_QUAD:
                {
                    if (!exec_inside_render_pass) {
                        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE,
                            "SIT_OP_DRAW_QUAD: no active render pass at execute time.");
                    }
                    if (exec_pass_display_id >= 0) {
                        exec_pass_had_draw = true;
                    }

                    /* [Phase B1 v2.4.320] Diagnostic: log GL_COLOR_WRITEMASK at textured quad draw
                     * when SIT_TEST_DEBUG_GL=1 env var is set. Helps confirm whether the
                     * storage-readback red-channel-zero bug is a color mask leak. */
                    if (p->args.draw_quad.use_texture) {
                        static int _sit_debug_gl_checked = 0;
                        static int _sit_debug_gl_active = 0;
                        if (!_sit_debug_gl_checked) {
                            _sit_debug_gl_active = (getenv("SIT_TEST_DEBUG_GL") != NULL);
                            _sit_debug_gl_checked = 1;
                        }
                        if (_sit_debug_gl_active) {
                            GLboolean mask[4];
                            glGetBooleanv(GL_COLOR_WRITEMASK, mask);
                            if (!mask[0] || !mask[1] || !mask[2] || !mask[3]) {
                                SituationLog(SIT_LOG_WARNING,
                                    "[B1 DIAG] SIT_OP_DRAW_QUAD (textured): GL_COLOR_WRITEMASK = (%d,%d,%d,%d) — expected all TRUE",
                                    (int)mask[0], (int)mask[1], (int)mask[2], (int)mask[3]);
                            }
                        }
                    }

                    {
                        SituationError quad_err = _SituationGLValidateInternalQuadDrawReady(buf, "SIT_OP_DRAW_QUAD", false);
                        if (quad_err != SITUATION_SUCCESS) {
                            return quad_err;
                        }
                    }

                    // Disable culling and depth for 2D quads, but restore them afterwards to avoid clobbering user state (Phase 4)
                    bool was_cull = sit_render.gl.cull_face_enabled > 0;
                    bool was_depth = sit_render.gl.depth_test_enabled > 0;
                    bool was_blend = sit_render.gl.blend_enabled > 0;
                    
                    glDisable(GL_CULL_FACE);
                    glDisable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                    // Only bind program if not already bound (avoids driver overhead on repeated quads)
                    glUseProgram(sit_render.gl.quad_shader_program);
                    sit_render.gl.current_program_id = sit_render.gl.quad_shader_program;

                    if (sit_render.gl.quad_vao) {
                        glBindVertexArray(sit_render.gl.quad_vao);
                        sit_render.gl.current_vao_id = sit_render.gl.quad_vao;
                    }

                    if (sit_render.gl.quad_shader_program) {
                        int tw = sit_render.gl.current_target_width;
                        int th = sit_render.gl.current_target_height;
                        if (tw < 1) {
                            tw = sit_gs.main_window_width;
                        }
                        if (th < 1) {
                            th = sit_gs.main_window_height;
                        }
                        if (tw < 1) {
                            tw = 1;
                        }
                        if (th < 1) {
                            th = 1;
                        }
                        mat4 quad_proj;
                        glm_ortho(0.0f, (float)tw, (float)th, 0.0f, -1.0f, 1.0f, quad_proj);
                        glProgramUniformMatrix4fv(sit_render.gl.quad_shader_program,
                            SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)quad_proj);
                    }

                    // --- Batch: process consecutive DRAW_QUAD opcodes ---
                    {
                        size_t batch_start = i;
						while (i < buf->packet_count && buf->packets[i].opcode == SIT_OP_DRAW_QUAD) {
                            SitCommandPacket* qp = &buf->packets[i];
                            
                            // [FIX] Bind texture directly from the quad packet to allow batching
                            if (qp->args.draw_quad.use_texture && qp->args.draw_quad.texture_id != 0) {
                                _SituationTextureSlot* tex_slot = NULL;
                                int slot_idx = qp->args.draw_quad.texture_slot_index;
                                if (slot_idx >= 0 && slot_idx < SITUATION_MAX_TEXTURES) {
                                    tex_slot = &sit_render.texture_registry[slot_idx];
                                    if (!tex_slot->is_active ||
                                        tex_slot->gl_texture_id != (GLuint)qp->args.draw_quad.texture_id) {
                                        tex_slot = NULL;
                                    }
                                }
                                if (!tex_slot) {
                                    tex_slot = _SituationGLFindTextureSlotByGlId((GLuint)qp->args.draw_quad.texture_id);
                                }
                                if (tex_slot) {
                                    SituationError prep_err = _SituationGLPrepareStorageTextureForSampling(tex_slot);
                                    if (prep_err != SITUATION_SUCCESS) {
                                        return prep_err;
                                    }
                                }
                                if (sit_render.gl.current_bound_texture_id != qp->args.draw_quad.texture_id) {
                                    glBindTextureUnit(0, (GLuint)qp->args.draw_quad.texture_id);
                                    sit_render.gl.current_bound_texture_id = (GLuint)qp->args.draw_quad.texture_id;
                                }
                            }
                            glProgramUniform1i(sit_render.gl.quad_shader_program, 6, qp->args.draw_quad.use_texture ? 1 : 0);
                            glProgramUniformMatrix4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_MODEL_MATRIX, 1, GL_FALSE, (const GLfloat*)qp->args.draw_quad.model);
                            {
                                Vector4 col = qp->args.draw_quad.color;
                                /* [Bug B2] Order-dependent leak: u_objectColor.r stuck at 0 while G/B/A stay ~1. */
                                if (qp->args.draw_quad.use_texture && col.x == 0.0f &&
                                    col.y >= 0.99f && col.z >= 0.99f && col.w >= 0.99f) {
                                    col.x = 1.0f;
                                }
                                glProgramUniform4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_OBJECT_COLOR, 1, (const GLfloat*)col.raw);
                            }
                            /* [Bug B3] Defense: leaked raster/uniform state can zero uv_rect scale after long runs. */
                            {
                                Vector4 uv = qp->args.draw_quad.uv_rect;
                                if (qp->args.draw_quad.use_texture) {
                                    if (uv.z == 0.0f) uv.z = 1.0f;
                                    if (uv.w == 0.0f) uv.w = 1.0f;
                                }
                                glProgramUniform4fv(sit_render.gl.quad_shader_program, 5, 1, (const GLfloat*)uv.raw);
                            }
                            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                            i++;
                        }
                        i--; // The for loop will increment
                    }

                    // Restore state to avoid clobbering user settings (Phase 4)
                    if (was_cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
                    if (was_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
                    if (!was_blend) glDisable(GL_BLEND);
                    glBlendFuncSeparate(sit_render.gl.blend_src_rgb, sit_render.gl.blend_dst_rgb, sit_render.gl.blend_src_alpha, sit_render.gl.blend_dst_alpha);
                    
                    // Restore global VAO
                    if (sit_render.gl.global_vao_id != 0) {
                        glBindVertexArray(sit_render.gl.global_vao_id);
                        sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                    }
                }
                break;

            case SIT_OP_DRAW_TEXTURE_YPQ:
                {
                    if (!exec_inside_render_pass) {
                        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE,
                            "SIT_OP_DRAW_TEXTURE_YPQ: no active render pass at execute time.");
                    }
                    if (exec_pass_display_id >= 0) {
                        exec_pass_had_draw = true;
                    }
                    {
                        SituationError quad_err = _SituationGLValidateInternalQuadDrawReady(buf, "SIT_OP_DRAW_TEXTURE_YPQ", false);
                        if (quad_err != SITUATION_SUCCESS) {
                            return quad_err;
                        }
                    }
                    if (sit_render.gl.ypq_grade_shader_program == 0) {
                        return _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED,
                            "SIT_OP_DRAW_TEXTURE_YPQ: YPQ grade shader is not initialized.");
                    }

                    bool was_cull = sit_render.gl.cull_face_enabled > 0;
                    bool was_depth = sit_render.gl.depth_test_enabled > 0;
                    bool was_blend = sit_render.gl.blend_enabled > 0;

                    glDisable(GL_CULL_FACE);
                    glDisable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                    if (sit_render.gl.current_program_id != sit_render.gl.ypq_grade_shader_program) {
                        glUseProgram(sit_render.gl.ypq_grade_shader_program);
                        sit_render.gl.current_program_id = sit_render.gl.ypq_grade_shader_program;
                    }

                    if (sit_render.gl.current_vao_id != sit_render.gl.quad_vao) {
                        glBindVertexArray(sit_render.gl.quad_vao);
                        sit_render.gl.current_vao_id = sit_render.gl.quad_vao;
                    }

                    {
                        int tw = sit_render.gl.current_target_width;
                        int th = sit_render.gl.current_target_height;
                        if (tw < 1) tw = sit_gs.main_window_width;
                        if (th < 1) th = sit_gs.main_window_height;
                        if (tw < 1) tw = 1;
                        if (th < 1) th = 1;
                        mat4 quad_proj;
                        glm_ortho(0.0f, (float)tw, (float)th, 0.0f, -1.0f, 1.0f, quad_proj);
                        glProgramUniformMatrix4fv(sit_render.gl.ypq_grade_shader_program,
                            SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)quad_proj);
                    }

                    SitCommandPacket* ypq_p = &buf->packets[i];
                    if (sit_render.gl.current_bound_texture_id != 0) {
                        glBindTextureUnit(0, sit_render.gl.current_bound_texture_id);
                    }
                    glProgramUniform1i(sit_render.gl.ypq_grade_shader_program, 6, 1);
                    glProgramUniformMatrix4fv(sit_render.gl.ypq_grade_shader_program, SIT_UNIFORM_LOC_MODEL_MATRIX, 1, GL_FALSE, (const GLfloat*)ypq_p->args.draw_texture_ypq.model);
                    glProgramUniform4fv(sit_render.gl.ypq_grade_shader_program, SIT_UNIFORM_LOC_OBJECT_COLOR, 1, (const GLfloat[]){1.0f, 1.0f, 1.0f, 1.0f});
                    glProgramUniform4fv(sit_render.gl.ypq_grade_shader_program, 5, 1, (const GLfloat*)ypq_p->args.draw_texture_ypq.uv_rect.raw);
                    glProgramUniform1f(sit_render.gl.ypq_grade_shader_program,
                        glGetUniformLocation(sit_render.gl.ypq_grade_shader_program, "u_phase_shift_deg"),
                        ypq_p->args.draw_texture_ypq.phase_shift_deg);
                    glProgramUniform1f(sit_render.gl.ypq_grade_shader_program,
                        glGetUniformLocation(sit_render.gl.ypq_grade_shader_program, "u_chroma_factor"),
                        ypq_p->args.draw_texture_ypq.chroma_factor);
                    glProgramUniform1f(sit_render.gl.ypq_grade_shader_program,
                        glGetUniformLocation(sit_render.gl.ypq_grade_shader_program, "u_luma_factor"),
                        ypq_p->args.draw_texture_ypq.luma_factor);
                    glProgramUniform1f(sit_render.gl.ypq_grade_shader_program,
                        glGetUniformLocation(sit_render.gl.ypq_grade_shader_program, "u_mix"),
                        ypq_p->args.draw_texture_ypq.mix);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    if (was_cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
                    if (was_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
                    if (!was_blend) glDisable(GL_BLEND);
                    glBlendFuncSeparate(sit_render.gl.blend_src_rgb, sit_render.gl.blend_dst_rgb, sit_render.gl.blend_src_alpha, sit_render.gl.blend_dst_alpha);

                    if (sit_render.gl.global_vao_id != 0) {
                        glBindVertexArray(sit_render.gl.global_vao_id);
                        sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                    }
                }
                break;

            case SIT_OP_SET_PUSH_CONSTANT:
                {
                    // Optimization: Use tracked state to avoid glGetIntegerv stall
                    GLuint prog = sit_render.gl.current_program_id;
                    if (prog) {
                        void* data = buf->data_buffer + p->args.push_constant.data_offset;
                        size_t sz = p->args.push_constant.size;
                        uint32_t loc = p->args.push_constant.offset;

                        /* SituationDrawModel pushes a 96-byte PBR block (mat4 + 2x vec4). OpenGL shaders
                         * use layout(location=0) uniform mat4 for the model matrix (see SIT_UNIFORM_LOC_*).
                         * Mirror the leading mat4 to uniform loc 0 even when the full payload uses SSBO. */
                        if (sz >= sizeof(mat4) && loc == 0u) {
                            glProgramUniformMatrix4fv(prog, SIT_UNIFORM_LOC_MODEL_MATRIX, 1, GL_FALSE, (const GLfloat*)data);
                        }

                        if (sz == sizeof(mat4)) {
                            if (loc != 0u) {
                                glProgramUniformMatrix4fv(prog, loc, 1, GL_FALSE, (const GLfloat*)data);
                            }
                        } else if (sz == sizeof(vec4)) glProgramUniform4fv(prog, loc, 1, (const GLfloat*)data);
                        else if (sz == sizeof(vec3)) glProgramUniform3fv(prog, loc, 1, (const GLfloat*)data);
                        else if (sz == sizeof(vec2)) glProgramUniform2fv(prog, loc, 1, (const GLfloat*)data);
                        else if (sz == sizeof(float)) glProgramUniform1fv(prog, loc, 1, (const GLfloat*)data);
                        else if (sz == sizeof(int)) glProgramUniform1iv(prog, loc, 1, (const GLint*)data);
                        else {
                            static GLuint push_constant_ssbo = 0;
                            static GLsizeiptr push_constant_capacity = 0;
                            if (push_constant_ssbo == 0 || push_constant_capacity < (GLsizeiptr)sz) {
                                if (push_constant_ssbo != 0) glDeleteBuffers(1, &push_constant_ssbo);
                                glCreateBuffers(1, &push_constant_ssbo);
                                push_constant_capacity = (GLsizeiptr)sz;
                                glNamedBufferStorage(push_constant_ssbo, push_constant_capacity, NULL, GL_DYNAMIC_STORAGE_BIT);
                            }
                            glNamedBufferSubData(push_constant_ssbo, 0, (GLsizeiptr)sz, data);
                            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, loc, push_constant_ssbo);
                        }
                    }
                }
                break;

                        case SIT_OP_BIND_DESCRIPTOR_SET:
                {
                    uint32_t idx = p->args.bind_desc.set_index;
                    size_t offset = p->args.bind_desc.offset;
                    size_t size = p->args.bind_desc.size;
                    uint32_t usage = p->args.bind_desc.usage_flags;

                    // Unpack Handle (slot_index in low 32 bits, generation in high 32 bits)
                    SituationBuffer handle = {0};
                    handle.slot_index = (uint32_t)(p->args.bind_desc.resource_id & 0xFFFFFFFF);
                    handle.generation = (uint32_t)(p->args.bind_desc.resource_id >> 32);

                    _SituationBufferSlot* slot = _SitGetBufferSlot(handle);
                    if (!slot) break;

                    GLuint id = slot->gl_buffer_id;

                    GLenum target = GL_UNIFORM_BUFFER;
                    if (usage & SITUATION_BUFFER_USAGE_STORAGE_BUFFER) target = GL_SHADER_STORAGE_BUFFER;

                    if (size > 0) {
                        glBindBufferRange(target, idx, id, (GLintptr)offset, (GLsizeiptr)size);
                    } else {
                        glBindBufferBase(target, idx, id);
                    }
                }
                break;


            /*
            // [Phase 2 Cleanup] Legacy Texture Binding via Descriptor Set Opcode removed.
            // Textures should use SIT_OP_BIND_TEXTURE_SET (if implemented) or handle this differently.
            // Wait, previous code handled type==1 as glBindTextureUnit.
            // SIT_OP_BIND_DESCRIPTOR_SET is documented as [Core] Binds a buffer's descriptor set.
            // SituationCmdBindTextureSet uses SIT_OP_BIND_TEXTURE_SET? Let's check.
            // SituationCmdBindTextureSet uses SIT_OP_BIND_DESCRIPTOR_SET with type=1 in previous versions?
            // Let's verify SituationCmdBindTextureSet implementation.
            */
            case SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING:
                {
                    // [Phase 2] Legacy Texture/Image binding logic.
                    // This handles SIT_OP_BIND_DESCRIPTOR_SET_LEGACY_TEXTURE_HANDLING, which is used by
                    // SituationCmdBindTextureSet to bind textures (type 1) and storage images (type 3).

                    uint32_t idx = p->args.bind_desc.set_index;
                    uint64_t id = p->args.bind_desc.resource_id;
                    int type = p->args.bind_desc.resource_type;

                    if (type == 1) { // 1 = Sampled Texture
                        if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
                            // [Phase 5] Virtual Bindless Fallback
                            // Only use virtual system if the shader supports it (has the injected uniform)
                            if (sit_render.gl.current_virtual_loc >= 0 &&
                                sit_render.gl.current_program_id != 0) {
                                int v_slot = _SituationVirtualBindlessBind((GLuint)id);
                                glProgramUniform1i(sit_render.gl.current_program_id,
                                    sit_render.gl.current_virtual_loc, v_slot);
                                // Note: We do NOT bind to 'idx' here because the shader uses the virtual array.
                            } else {
                                // Standard Bind (Fallback for non-bindless shaders)
                                glBindTextureUnit(idx, (GLuint)id);
                            }
                            // Always track for legacy/internal purposes
                            sit_render.gl.current_bound_texture_id = (GLuint)id;
                        } else {
                            // Native GL 4.6 Bindless or standard bind
                            glBindTextureUnit(idx, (GLuint)id);
                            // [v2.3.31] Track texture state for subsequent internal draw calls (Quad/Text)
                            sit_render.gl.current_bound_texture_id = (GLuint)id;
                        }
                    }
                    else if (type == 3) { // 3 = Storage Image
                         glBindImageTexture(idx, (GLuint)id, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
                         {
                             _SituationTextureSlot* slot = _SituationGLFindTextureSlotByGlId((GLuint)id);
                             if (slot) {
                                 slot->gl_image_binding_unit = (int)idx;
                             }
                         }
                    }
                }
                break;

            case SIT_OP_BIND_VERTEX_BUFFER:
                if (sit_render.gl.global_vao_id != 0 &&
                    sit_render.gl.current_vao_id != sit_render.gl.global_vao_id) {
                    glBindVertexArray(sit_render.gl.global_vao_id);
                    sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                }
                glVertexArrayVertexBuffer(sit_render.gl.global_vao_id, p->args.bind_vbo.binding,
                                          (GLuint)p->args.bind_vbo.buffer_id,
                                          (GLintptr)p->args.bind_vbo.offset,
                                          (GLsizei)p->args.bind_vbo.stride);
                break;

            case SIT_OP_BIND_INDEX_BUFFER:
                if (sit_render.gl.global_vao_id != 0) {
                    glVertexArrayElementBuffer(sit_render.gl.global_vao_id, (GLuint)p->args.bind_ibo.buffer_id);
                    sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                } else {
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)p->args.bind_ibo.buffer_id);
                }
                sit_render.gl.bound_ibo_byte_offset = p->args.bind_ibo.offset;
                if (p->args.bind_ibo.index_type == SIT_INDEX_UINT16) {
                    sit_render.gl.current_index_type = GL_UNSIGNED_SHORT;
                    sit_render.gl.bound_ibo_index_element_size = sizeof(uint16_t);
                } else {
                    sit_render.gl.current_index_type = GL_UNSIGNED_INT;
                    sit_render.gl.bound_ibo_index_element_size = sizeof(uint32_t);
                }
                break;

            case SIT_OP_DRAW:
                {
                    if (exec_pass_display_id >= 0) {
                        exec_pass_had_draw = true;
                    }
                    GLenum draw_mode = sit_render.gl.current_primitive_mode_set
                        ? sit_render.gl.current_primitive_mode : GL_TRIANGLES;
                    if (sit_render.gl.global_vao_id != 0 &&
                        sit_render.gl.current_vao_id != sit_render.gl.global_vao_id) {
                        glBindVertexArray(sit_render.gl.global_vao_id);
                        sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                    }
                    // [Phase 4] Multi-Draw Indirect Optimization
                    // Check for subsequent draw commands with the same opcode
                    size_t batch_count = 1;
                    size_t lookahead = i + 1;
                    while (lookahead < buf->packet_count) {
                        if (buf->packets[lookahead].opcode == SIT_OP_DRAW) {
                            batch_count++;
                            lookahead++;
                        } else {
                            break;
                        }
                    }

                    if (batch_count > 1 && sit_render.gl.mdi_data_ptr) {
                        // Batch detected!
                        // 1. Allocate space in MDI ring
                        // Note: Simple linear allocator for now. Assuming 1MB is enough per frame.
                        // Ideally check wrap-around/overflow.
                        size_t cmd_size = sizeof(_SituationGLDrawArraysIndirectCommand);
                        size_t total_size = batch_count * cmd_size;
                        size_t offset = atomic_fetch_add(&sit_render.gl.mdi_ring_head, total_size);

                        // Safety check: Ensure we stay within the CURRENT FRAME's slice
                        if (offset >= mdi_frame_offset && offset + total_size <= mdi_frame_offset + (1024 * 1024)) {
                            _SituationGLDrawArraysIndirectCommand* cmds = (_SituationGLDrawArraysIndirectCommand*)((uint8_t*)sit_render.gl.mdi_data_ptr + offset);

                            // 2. Fill commands
                            for (size_t k = 0; k < batch_count; ++k) {
                                SitCommandPacket* next_p = &buf->packets[i + k];
                                cmds[k].count = next_p->args.draw.v_count;
                                cmds[k].instanceCount = next_p->args.draw.i_count;
                                cmds[k].first = next_p->args.draw.first_v;
                                cmds[k].baseInstance = next_p->args.draw.first_i;
                            }

                            // 3. Bind & Draw
                            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, sit_render.gl.mdi_buffer_id);
                            glMultiDrawArraysIndirect(draw_mode, (const void*)((uintptr_t)offset), (GLsizei)batch_count, 0);
                            SIT_CHECK_GL_ERROR();

                            // 4. Advance
                            i += (batch_count - 1); // Loop increments i one more time
                        } else {
                            // Overflow fallback: Draw individually
                            glDrawArraysInstanced(draw_mode, p->args.draw.first_v, p->args.draw.v_count, p->args.draw.i_count);
                        }
                    } else {
                        // Single draw fallback
                        glDrawArraysInstanced(draw_mode, p->args.draw.first_v, p->args.draw.v_count, p->args.draw.i_count);
                    }
                }
                break;

            case SIT_OP_DRAW_INDEXED:
                {
                    if (exec_pass_display_id >= 0) {
                        exec_pass_had_draw = true;
                    }
                    if (sit_render.gl.global_vao_id != 0 &&
                        sit_render.gl.current_vao_id != sit_render.gl.global_vao_id) {
                        glBindVertexArray(sit_render.gl.global_vao_id);
                        sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                    }
                    GLenum draw_mode = sit_render.gl.current_primitive_mode_set
                        ? sit_render.gl.current_primitive_mode : GL_TRIANGLES;
                    GLenum index_type = sit_render.gl.current_index_type ? sit_render.gl.current_index_type : GL_UNSIGNED_INT;
                    size_t index_elem_size = sit_render.gl.bound_ibo_index_element_size ? sit_render.gl.bound_ibo_index_element_size : sizeof(uint32_t);
                    // [Phase 4] Multi-Draw Indirect Optimization
                    size_t batch_count = 1;
                    size_t lookahead = i + 1;
                    while (lookahead < buf->packet_count) {
                        if (buf->packets[lookahead].opcode == SIT_OP_DRAW_INDEXED) {
                            batch_count++;
                            lookahead++;
                        } else {
                            break;
                        }
                    }

                    if (batch_count > 1 && sit_render.gl.mdi_data_ptr) {
                        size_t cmd_size = sizeof(_SituationGLDrawElementsIndirectCommand);
                        size_t total_size = batch_count * cmd_size;
                        size_t offset = atomic_fetch_add(&sit_render.gl.mdi_ring_head, total_size);

                        // Safety check: Ensure we stay within the CURRENT FRAME's slice
                        if (offset >= mdi_frame_offset && offset + total_size <= mdi_frame_offset + (1024 * 1024)) {
                            _SituationGLDrawElementsIndirectCommand* cmds = (_SituationGLDrawElementsIndirectCommand*)((uint8_t*)sit_render.gl.mdi_data_ptr + offset);

                            {
                                const uint32_t ibo_first_index_bias = (uint32_t)(sit_render.gl.bound_ibo_byte_offset / index_elem_size);
                                for (size_t k = 0; k < batch_count; ++k) {
                                    SitCommandPacket* next_p = &buf->packets[i + k];
                                    cmds[k].count = next_p->args.draw_indexed.idx_count;
                                    cmds[k].instanceCount = next_p->args.draw_indexed.inst_count;
                                    cmds[k].firstIndex = ibo_first_index_bias + next_p->args.draw_indexed.first_idx;
                                    cmds[k].baseVertex = next_p->args.draw_indexed.v_offset;
                                    cmds[k].baseInstance = next_p->args.draw_indexed.first_inst;
                                }
                            }

                            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, sit_render.gl.mdi_buffer_id);
                            glMultiDrawElementsIndirect(draw_mode, index_type, (const void*)((uintptr_t)offset), (GLsizei)batch_count, 0);
                            SIT_CHECK_GL_ERROR();

                            i += (batch_count - 1);
                        } else {
                            // Overflow
                            glDrawElementsInstancedBaseVertexBaseInstance(draw_mode, p->args.draw_indexed.idx_count, index_type,
                                (void*)((uintptr_t)(sit_render.gl.bound_ibo_byte_offset + (size_t)p->args.draw_indexed.first_idx * index_elem_size)),
                                p->args.draw_indexed.inst_count, p->args.draw_indexed.v_offset, p->args.draw_indexed.first_inst);
                        }
                    } else {
                        glDrawElementsInstancedBaseVertexBaseInstance(draw_mode, p->args.draw_indexed.idx_count, index_type,
                            (void*)((uintptr_t)(sit_render.gl.bound_ibo_byte_offset + (size_t)p->args.draw_indexed.first_idx * index_elem_size)),
                            p->args.draw_indexed.inst_count, p->args.draw_indexed.v_offset, p->args.draw_indexed.first_inst);
                    }
                }
                break;

            case SIT_OP_PIPELINE_BARRIER:
                {
                    GLbitfield barriers = 0;
                    if (p->args.barrier.src & SITUATION_BARRIER_COMPUTE_SHADER_WRITE)
                        barriers |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT;
                    if (p->args.barrier.src & SITUATION_BARRIER_FRAGMENT_SHADER_WRITE)
                        barriers |= GL_FRAMEBUFFER_BARRIER_BIT;
                    if (p->args.barrier.dst & SITUATION_BARRIER_VERTEX_SHADER_READ)
                        barriers |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT;
                    if (p->args.barrier.dst & SITUATION_BARRIER_FRAGMENT_SHADER_READ)
                        barriers |= GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT;
                    if (p->args.barrier.dst & SITUATION_BARRIER_COMPUTE_SHADER_READ)
                        barriers |= GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
					if (p->args.barrier.dst & SITUATION_BARRIER_TRANSFER_READ)
                        barriers |= GL_BUFFER_UPDATE_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT;
                    if (p->args.barrier.dst & SITUATION_BARRIER_INDIRECT_COMMAND_READ)
                        barriers |= GL_COMMAND_BARRIER_BIT;

                    if (barriers == 0) barriers = GL_ALL_BARRIER_BITS;
                    glMemoryBarrier(barriers);
                }
                break;

            case SIT_OP_DISPATCH:
                glDispatchCompute(p->args.dispatch.x, p->args.dispatch.y, p->args.dispatch.z);
                break;

            case SIT_OP_DISPATCH_INDIRECT:
                glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, (GLuint)p->args.dispatch_indirect.buffer_id);
                glDispatchComputeIndirect((GLintptr)p->args.dispatch_indirect.offset);
                glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
                break;

            case SIT_OP_DRAW_INDIRECT:
                {
                GLenum draw_mode = sit_render.gl.current_primitive_mode_set
                    ? sit_render.gl.current_primitive_mode : GL_TRIANGLES;
                if (sit_render.gl.global_vao_id != 0 &&
                    sit_render.gl.current_vao_id != sit_render.gl.global_vao_id) {
                    glBindVertexArray(sit_render.gl.global_vao_id);
                    sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                }
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, (GLuint)p->args.draw_indirect.buffer_id);
                glDrawArraysIndirect(draw_mode, (const void*)(uintptr_t)p->args.draw_indirect.offset);
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
                SIT_CHECK_GL_ERROR();
                break;
                }

            case SIT_OP_DRAW_INDEXED_INDIRECT:
                {
                GLenum draw_mode = sit_render.gl.current_primitive_mode_set
                    ? sit_render.gl.current_primitive_mode : GL_TRIANGLES;
                if (sit_render.gl.global_vao_id != 0 &&
                    sit_render.gl.current_vao_id != sit_render.gl.global_vao_id) {
                    glBindVertexArray(sit_render.gl.global_vao_id);
                    sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                }
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, (GLuint)p->args.draw_indexed_indirect.buffer_id);
                glDrawElementsIndirect(draw_mode, GL_UNSIGNED_INT,
                    (const void*)(uintptr_t)p->args.draw_indexed_indirect.offset);
                glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
                SIT_CHECK_GL_ERROR();
                break;
                }

            // [Bug 10 Fix] Handle compute pipeline binding — was missing, causing compute
            // dispatches to use whatever program was previously bound (state leak from prior tests)
            case SIT_OP_BIND_COMPUTE_PIPELINE:
                glUseProgram((GLuint)p->args.bind_pipeline.shader_id);
                sit_render.gl.current_program_id = (GLuint)p->args.bind_pipeline.shader_id;
                if (!SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES)) {
                    sit_render.gl.current_virtual_loc = glGetUniformLocation(sit_render.gl.current_program_id, "_sit_texture_slot_id");
                } else {
                    sit_render.gl.current_virtual_loc = -1;
                }
                break;

            case SIT_OP_PRESENT:
                {
                    _SituationTextureSlot* slot = _SitGetTextureSlot(p->args.present.texture);
                    if (!slot) break;

                    GLuint tex = slot->gl_texture_id;
                    GLuint fbo;
                    glCreateFramebuffers(1, &fbo);
                    if (fbo == 0) break; // Context lost or invalid — skip present
                    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
                    GLenum fb_status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
                    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
                        glDeleteFramebuffers(1, &fbo);
                        break;
                    }
                    // Use captured resolution to avoid race with main thread resize
                    glBlitNamedFramebuffer(fbo, 0,
                        0, 0, p->args.present.texture.width, p->args.present.texture.height,
                        0, 0, p->args.present.target_w, p->args.present.target_h,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);
                    glDeleteFramebuffers(1, &fbo);
                }
                break;

            case SIT_OP_RENDER_VIRTUAL_DISPLAYS:
                {
                    SituationError vd_exec_err = _SitGLExecRenderVirtualDisplays(frame_index);
                    if (vd_exec_err != SITUATION_SUCCESS) return vd_exec_err;
                }
                break;

            case SIT_OP_GPU_ZONE_BEGIN:
                if (sit_render.gpu_timestamps_supported) {
                    _SitGpuProfZoneBeginGL(p->args.gpu_zone.zone_id, (uint32_t)frame_index);
                }
                break;

            case SIT_OP_GPU_ZONE_END:
                if (sit_render.gpu_timestamps_supported) {
                    _SitGpuProfZoneEndGL(p->args.gpu_zone.zone_id, (uint32_t)frame_index);
                }
                break;

            case SIT_OP_RESET_QUERY_POOL:
                {
                    _SituationQueryPoolSlot* qps = _SitGetQueryPoolSlot((SituationQueryPool){
                        p->args.reset_query_pool.pool_slot, p->args.reset_query_pool.pool_generation});
                    if (qps) {
                        _SitQueryPoolResetGL(qps, p->args.reset_query_pool.first_query, p->args.reset_query_pool.query_count);
                    }
                }
                break;

            case SIT_OP_WRITE_TIMESTAMP:
                {
                    _SituationQueryPoolSlot* qps = _SitGetQueryPoolSlot((SituationQueryPool){
                        p->args.write_timestamp.pool_slot, p->args.write_timestamp.pool_generation});
                    if (qps) {
                        _SitQueryPoolWriteTimestampGL(qps, p->args.write_timestamp.query_index);
                    }
                }
                break;

            case SIT_OP_BEGIN_OCCLUSION_QUERY:
                {
                    _SituationQueryPoolSlot* qps = _SitGetQueryPoolSlot((SituationQueryPool){
                        p->args.occlusion_query.pool_slot, p->args.occlusion_query.pool_generation});
                    if (qps) {
                        _SitQueryPoolBeginOcclusionGL(qps, p->args.occlusion_query.query_index);
                    }
                }
                break;

            case SIT_OP_END_OCCLUSION_QUERY:
                _SitQueryPoolEndOcclusionGL();
                break;

            case SIT_OP_DRAW_TEXT:
            case SIT_OP_DRAW_TEXT_EX:
                #if !defined(SITUATION_NO_STB) && !defined(SITUATION_NO_STB_TRUETYPE)
                {
                    if (!exec_inside_render_pass) {
                        return _SituationSetErrorFromCode(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE,
                            "SIT_OP_DRAW_TEXT: no active render pass at execute time.");
                    }
                    if (exec_pass_display_id >= 0) {
                        exec_pass_had_draw = true;
                    }
                    {
                        SituationError text_err = _SituationGLValidateInternalTextDrawReady(buf, "SIT_OP_DRAW_TEXT", false);
                        if (text_err != SITUATION_SUCCESS) {
                            return text_err;
                        }
                    }

                    const char* text;
                    SituationFont font;
                    Vector2 pos;
                    ColorRGBA color;
                    float fontSize = 0.0f;
                    float spacing = 0.0f;

                    if (p->opcode == SIT_OP_DRAW_TEXT) {
                         text = (const char*)(buf->data_buffer + p->args.draw_text.text_offset);
                         font = p->args.draw_text.font;
                         pos = p->args.draw_text.pos;
                         color = p->args.draw_text.color;
                    } else {
                         text = (const char*)(buf->data_buffer + p->args.draw_text_ex.text_offset);
                         font = p->args.draw_text_ex.font;
                         pos = p->args.draw_text_ex.pos;
                         color = p->args.draw_text_ex.color;
                         fontSize = p->args.draw_text_ex.fontSize;
                         spacing = p->args.draw_text_ex.spacing;
                    }

                    size_t len = strlen(text);
                    if (len == 0) break;
                    if (len > 2048) len = 2048;

                    size_t vert_count = len * 6;
                    size_t data_size = vert_count * 4 * sizeof(float);

                    if (sit_render.text_batch_capacity < data_size) {
                        sit_render.text_batch_scratch = (float*)SIT_REALLOC(sit_render.text_batch_scratch, data_size * 2);
                        sit_render.text_batch_capacity = data_size * 2;
                    }
                    float* vertices = sit_render.text_batch_scratch;

                    float x = pos.x;
                    float y = pos.y;
                    stbtt_bakedchar* cdata = (stbtt_bakedchar*)font.glyph_info;
                    int v_idx = 0;

                    bool is_grid_font = _SituationFontIsGridAtlas(&font);

                    // Use provided font size or default to font's native size
                    float target_size = (fontSize > 0.0f) ? fontSize : font.font_height_pixels;
                    // For grid font, scale ratio. For STB, it's baked, so we can't easily rescale without artifacts unless signed distance field.
                    // But for simple scaling (like pixel art), scaling the quad is fine.
                    float scale_factor = (font.font_height_pixels > 0.0f) ? (target_size / font.font_height_pixels) : 1.0f;
                    float line_start_x = x;

                    for (size_t k = 0; k < len; k++) {
                        if (is_grid_font) {
                            _SituationFontEmitGridGlyph(
                                vertices, &v_idx, &font, (unsigned char)text[k],
                                &x, &y, line_start_x, scale_factor, spacing);
                        }
                        else if (text[k] >= 32 && text[k] < 128) {
                            float x_before = x;
                            stbtt_aligned_quad q;
                            stbtt_GetBakedQuad(cdata, font.atlas_width, font.atlas_height, text[k] - 32, &x, &y, &q, 1);

                            if (scale_factor != 1.0f || spacing != 0.0f) {
                                float w = q.x1 - q.x0;
                                float h = q.y1 - q.y0;
                                float y_off = q.y0 - y;

                                float x0 = x_before + (q.x0 - x_before) * scale_factor;
                                float y0 = y + y_off * scale_factor;
                                float x1 = x0 + w * scale_factor;
                                float y1 = y0 + h * scale_factor;

                                q.x0 = x0; q.y0 = y0;
                                q.x1 = x1; q.y1 = y1;

                                float advance = x - x_before;
                                x = x_before + (advance * scale_factor) + spacing;
                            }

                            vertices[v_idx++] = q.x0; vertices[v_idx++] = q.y0; vertices[v_idx++] = q.s0; vertices[v_idx++] = q.t0;
                            vertices[v_idx++] = q.x0; vertices[v_idx++] = q.y1; vertices[v_idx++] = q.s0; vertices[v_idx++] = q.t1;
                            vertices[v_idx++] = q.x1; vertices[v_idx++] = q.y0; vertices[v_idx++] = q.s1; vertices[v_idx++] = q.t0;

                            vertices[v_idx++] = q.x1; vertices[v_idx++] = q.y0; vertices[v_idx++] = q.s1; vertices[v_idx++] = q.t0;
                            vertices[v_idx++] = q.x0; vertices[v_idx++] = q.y1; vertices[v_idx++] = q.s0; vertices[v_idx++] = q.t1;
                            vertices[v_idx++] = q.x1; vertices[v_idx++] = q.y1; vertices[v_idx++] = q.s1; vertices[v_idx++] = q.t1;
                        }
                    }
                    int final_vert_count = v_idx / 4;

                    // Text path: bindless only if API exists — glad_glProgramUniformHandleui64ARB can be NULL even when
                    // Situation reports bindless/handles; calling it crashes instantly (often first DrawText).
                    {
                        bool did_bindless = false;
#if defined(GLAD_GL_ARB_bindless_texture)
                        if (!is_grid_font && SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_TEXTURES) && glad_glProgramUniformHandleui64ARB) {
                            uint64_t handle = SituationGetTextureHandle(font.atlas_texture);
                            if (handle) {
                                glProgramUniform1i(sit_render.gl.text_shader_program, 6, 1);
                                glad_glProgramUniformHandleui64ARB(sit_render.gl.text_shader_program, 7, handle);
                                did_bindless = true;
                            }
                        }
#endif
                        if (!did_bindless) {
                            glProgramUniform1i(sit_render.gl.text_shader_program, 6, 0);
                            _SituationTextureSlot* slot = _SitGetTextureSlot(font.atlas_texture);
                            if (slot) glBindTextureUnit(SIT_SAMPLER_BINDING_ALBEDO, slot->gl_texture_id);
                        }
                    }

                    if (data_size > 524288) data_size = 524288;
                    glNamedBufferSubData(sit_render.gl.text_vbo, 0, data_size, vertices);

                    Vector4 color_vec;
                    SituationConvertColorToVector4(color, &color_vec);
                    glProgramUniform4fv(sit_render.gl.text_shader_program, SIT_UNIFORM_LOC_OBJECT_COLOR, 1, (const GLfloat*)color_vec.raw);

                    glUseProgram(sit_render.gl.text_shader_program);
                    sit_render.gl.current_program_id = sit_render.gl.text_shader_program;

                    // Set projection matrix for text shader (ortho, top-left origin)
                    mat4 text_proj;
                    int text_target_w = sit_render.gl.current_target_width > 0 ? sit_render.gl.current_target_width : sit_gs.main_window_width;
                    int text_target_h = sit_render.gl.current_target_height > 0 ? sit_render.gl.current_target_height : sit_gs.main_window_height;
                    glm_ortho(0.0f, (float)text_target_w, (float)text_target_h, 0.0f, -1.0f, 1.0f, text_proj);
                    glProgramUniformMatrix4fv(sit_render.gl.text_shader_program, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)text_proj);

                    if (sit_render.gl.text_shader_program && final_vert_count > 0) {
                        /* Same as DRAW_QUAD: 2D overlays must not lose to the depth buffer — quads wrote ~same Z first. */
                        bool was_cull = sit_render.gl.cull_face_enabled > 0;
                        bool was_depth = sit_render.gl.depth_test_enabled > 0;
                        bool was_blend = sit_render.gl.blend_enabled > 0;

                        glDisable(GL_CULL_FACE);
                        glDisable(GL_DEPTH_TEST);
                        glEnable(GL_BLEND);
                        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                        glBindVertexArray(sit_render.gl.text_vao);
                        _SitGpuProfInternalZoneBeginGL((uint32_t)SITUATION_GPU_ZONE_TEXT_BATCH, frame_index);
                        glDrawArrays(GL_TRIANGLES, 0, final_vert_count);
                        _SitGpuProfInternalZoneEndGL((uint32_t)SITUATION_GPU_ZONE_TEXT_BATCH, frame_index);
                        
                        // Restore state (Phase 4 fix)
                        if (was_cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
                        if (was_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
                        if (!was_blend) glDisable(GL_BLEND);
                        glBlendFuncSeparate(sit_render.gl.blend_src_rgb, sit_render.gl.blend_dst_rgb, sit_render.gl.blend_src_alpha, sit_render.gl.blend_dst_alpha);
                    }

                    // [CRITICAL] Restore global VAO state
                    glBindVertexArray(sit_render.gl.global_vao_id);
                    sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
                }
                #endif
                break;

            case SIT_OP_UPDATE_BUFFER:
                {
                    void* data = buf->data_buffer + p->args.update_buffer.data_offset;
                    glNamedBufferSubData((GLuint)p->args.update_buffer.buffer_id,
                                         (GLintptr)p->args.update_buffer.offset,
                                         (GLsizeiptr)p->args.update_buffer.size,
                                         data);
                }
                break;

            case SIT_OP_SET_VERTEX_ATTRIBUTE:
                {
                    GLenum gl_type = _SituationMapDataTypeToGL((SituationDataType)p->args.set_vertex_attr.type);
                    if (gl_type != 0) {
                        uint32_t loc = p->args.set_vertex_attr.location;
                        glVertexArrayAttribFormat(sit_render.gl.global_vao_id, loc,
                                                  p->args.set_vertex_attr.size,
                                                  gl_type,
                                                  p->args.set_vertex_attr.normalized ? GL_TRUE : GL_FALSE,
                                                  (GLuint)p->args.set_vertex_attr.offset);
                        glVertexArrayAttribBinding(sit_render.gl.global_vao_id, loc,
                                                    p->args.set_vertex_attr.binding);
                        glEnableVertexArrayAttrib(sit_render.gl.global_vao_id, loc);
                    }
                }
                break;

            case SIT_OP_SET_UNIFORM:
                {
                    void* data = buf->data_buffer + p->args.set_uniform.data_offset;
                    GLint loc = p->args.set_uniform.location;
                    GLuint prog = (GLuint)p->args.set_uniform.shader_id;
                    int type = p->args.set_uniform.type;
                    int n = p->args.set_uniform.elem_count > 0 ? p->args.set_uniform.elem_count : 1;

                    switch (type) {
                        case SIT_UNIFORM_FLOAT: glProgramUniform1fv(prog, loc, n, (const GLfloat*)data); break;
                        case SIT_UNIFORM_VEC2:  glProgramUniform2fv(prog, loc, n, (const GLfloat*)data); break;
                        case SIT_UNIFORM_VEC3:  glProgramUniform3fv(prog, loc, n, (const GLfloat*)data); break;
                        case SIT_UNIFORM_VEC4:  glProgramUniform4fv(prog, loc, n, (const GLfloat*)data); break;
                        case SIT_UNIFORM_INT:   glProgramUniform1iv(prog, loc, n, (const GLint*)data); break;
                        case SIT_UNIFORM_IVEC2: glProgramUniform2iv(prog, loc, n, (const GLint*)data); break;
                        case SIT_UNIFORM_IVEC3: glProgramUniform3iv(prog, loc, n, (const GLint*)data); break;
                        case SIT_UNIFORM_IVEC4: glProgramUniform4iv(prog, loc, n, (const GLint*)data); break;
                        case SIT_UNIFORM_MAT4:  glProgramUniformMatrix4fv(prog, loc, n, GL_FALSE, (const GLfloat*)data); break;
                    }
                }
                break;
            case SIT_OP_COPY_BUFFER:
                {
                    GLuint src_gl = (GLuint)p->args.copy_buffer.src_id;
                    GLuint dst_gl = (GLuint)p->args.copy_buffer.dst_id;
                    GLintptr src_off = (GLintptr)p->args.copy_buffer.src_offset;
                    GLintptr dst_off = (GLintptr)p->args.copy_buffer.dst_offset;
                    GLsizeiptr copy_size = (GLsizeiptr)p->args.copy_buffer.size;
                    while (glGetError() != GL_NO_ERROR) {}
                    glCopyNamedBufferSubData(
                        src_gl,
                        dst_gl,
                        src_off,
                        dst_off,
                        copy_size);
                    if (glGetError() != GL_NO_ERROR && copy_size > 0) {
                        /* Some drivers reject CopyNamedBufferSubData for loader-context buffers
                         * or readback storage layouts; CPU staging preserves semantics. */
                        void* staging = SIT_MALLOC((size_t)copy_size);
                        if (staging) {
                            while (glGetError() != GL_NO_ERROR) {}
                            glGetNamedBufferSubData(src_gl, src_off, copy_size, staging);
                            if (glGetError() == GL_NO_ERROR) {
                                glNamedBufferSubData(dst_gl, dst_off, copy_size, staging);
                            }
                            SIT_FREE(staging);
                        }
                    }
                }
                break;
            case SIT_OP_BLIT_TEXTURE:
                {
                    _SituationTextureSlot* src_slot = _SitGetTextureSlot(p->args.blit_texture.src);
                    _SituationTextureSlot* dst_slot = _SitGetTextureSlot(p->args.blit_texture.dst);
                    if (!src_slot || !dst_slot) break;

                    const SituationTextureBlitRegion* region = &p->args.blit_texture.region;
                    GLuint src_fbo = 0;
                    GLuint dst_fbo = 0;
                    glCreateFramebuffers(1, &src_fbo);
                    glCreateFramebuffers(1, &dst_fbo);
                    if (src_fbo == 0 || dst_fbo == 0) {
                        if (src_fbo) glDeleteFramebuffers(1, &src_fbo);
                        if (dst_fbo) glDeleteFramebuffers(1, &dst_fbo);
                        break;
                    }

                    glNamedFramebufferTexture(src_fbo, GL_COLOR_ATTACHMENT0, src_slot->gl_texture_id, (GLint)region->src_mip_level);
                    glNamedFramebufferTexture(dst_fbo, GL_COLOR_ATTACHMENT0, dst_slot->gl_texture_id, (GLint)region->dst_mip_level);
                    glNamedFramebufferReadBuffer(src_fbo, GL_COLOR_ATTACHMENT0);
                    glNamedFramebufferDrawBuffer(dst_fbo, GL_COLOR_ATTACHMENT0);

                    GLenum src_status = glCheckNamedFramebufferStatus(src_fbo, GL_READ_FRAMEBUFFER);
                    GLenum dst_status = glCheckNamedFramebufferStatus(dst_fbo, GL_DRAW_FRAMEBUFFER);
                    if (src_status == GL_FRAMEBUFFER_COMPLETE && dst_status == GL_FRAMEBUFFER_COMPLETE) {
                        GLenum filter = (region->filter == SITUATION_BLIT_FILTER_LINEAR) ? GL_LINEAR : GL_NEAREST;
                        glBlitNamedFramebuffer(
                            src_fbo,
                            dst_fbo,
                            region->src_rect.x,
                            region->src_rect.y,
                            region->src_rect.x + region->src_rect.width,
                            region->src_rect.y + region->src_rect.height,
                            region->dst_rect.x,
                            region->dst_rect.y,
                            region->dst_rect.x + region->dst_rect.width,
                            region->dst_rect.y + region->dst_rect.height,
                            GL_COLOR_BUFFER_BIT,
                            filter);
                    }

                    glDeleteFramebuffers(1, &src_fbo);
                    glDeleteFramebuffers(1, &dst_fbo);
                }
                break;
            case SIT_OP_COPY_TEXTURE:
                {
                    _SituationTextureSlot* src_slot = _SitGetTextureSlot(p->args.copy_texture.src);
                    _SituationTextureSlot* dst_slot = _SitGetTextureSlot(p->args.copy_texture.dst);
                    if (!src_slot || !dst_slot) break;

                    const SituationTextureCopyRegion* region = &p->args.copy_texture.region;
                    glCopyImageSubData(
                        src_slot->gl_texture_id,
                        GL_TEXTURE_2D,
                        (GLint)region->src_mip_level,
                        region->src_rect.x,
                        region->src_rect.y,
                        (GLint)region->src_array_layer,
                        dst_slot->gl_texture_id,
                        GL_TEXTURE_2D,
                        (GLint)region->dst_mip_level,
                        region->dst_x,
                        region->dst_y,
                        (GLint)region->dst_array_layer,
                        region->src_rect.width,
                        region->src_rect.height,
                        1);
                }
                break;
            case SIT_OP_COPY_BUFFER_TO_TEXTURE:
                {
                    _SituationTextureSlot* dst_slot = _SitGetTextureSlot(p->args.copy_buffer_to_texture.dst);
                    if (!dst_slot) break;

                    const SituationTextureCopyRegion* region = &p->args.copy_buffer_to_texture.region;
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, (GLuint)p->args.copy_buffer_to_texture.buffer_id);
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                    glTextureSubImage2D(
                        dst_slot->gl_texture_id,
                        (GLint)region->dst_mip_level,
                        region->dst_x,
                        region->dst_y,
                        region->src_rect.width,
                        region->src_rect.height,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        (const void*)p->args.copy_buffer_to_texture.buffer_offset);
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                }
                break;
            case SIT_OP_COPY_TEXTURE_TO_BUFFER:
                {
                    _SituationTextureSlot* src_slot = _SitGetTextureSlot(p->args.copy_texture_to_buffer.src);
                    if (!src_slot) break;

                    const SituationTextureCopyRegion* region = &p->args.copy_texture_to_buffer.region;
                    GLint pack_row_length = 0;
                    if (p->args.copy_texture_to_buffer.buffer_row_pitch != 0) {
                        pack_row_length = (GLint)(p->args.copy_texture_to_buffer.buffer_row_pitch / 4u);
                    }
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)p->args.copy_texture_to_buffer.buffer_id);
                    glPixelStorei(GL_PACK_ROW_LENGTH, pack_row_length);
                    glGetTextureSubImage(
                        src_slot->gl_texture_id,
                        (GLint)region->src_mip_level,
                        region->src_rect.x,
                        region->src_rect.y,
                        0,
                        region->src_rect.width,
                        region->src_rect.height,
                        1,
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        (GLsizei)(region->src_rect.width * region->src_rect.height * 4),
                        (void*)p->args.copy_texture_to_buffer.buffer_offset);
                    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
                    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                }
                break;
        }
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: Exited switch, about to check GL error\n");
        fflush(stdout);
        #endif
        {
            GLenum gl_err = glGetError();
            if (gl_err != GL_NO_ERROR) {
                char gl_detail[128];
                snprintf(gl_detail, sizeof(gl_detail),
                    "_SituationGLExecuteCommands: GL 0x%X after opcode %d",
                    (unsigned)gl_err, (int)p->opcode);
                buf->packet_count = 0;
                buf->data_cursor = 0;
                buf->recording_render_pass_active = false;
                return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, gl_detail);
            }
        }
        #ifdef SITUATION_OPENGL_DEBUG
        printf("[OpenGL Debug] _SituationGLExecuteCommands: GL error check complete, continuing loop\n");
        fflush(stdout);
        #endif
    }

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] _SituationGLExecuteCommands: Exited execution loop\n");
    fflush(stdout);
    printf("[OpenGL Debug] About to call first SIT_DEBUG_LOG\n");
    fflush(stdout);
    #endif

    SIT_DEBUG_LOG("[GLExecute] All packets processed, resetting buffer\n");
    // Reset buffer after execution
    buf->packet_count = 0;
    buf->data_cursor = 0;
    buf->recording_render_pass_active = false;

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] Buffer reset complete, about to call second SIT_DEBUG_LOG\n");
    fflush(stdout);
    #endif

    SIT_DEBUG_LOG("[GLExecute] Cleaning up MDI state\n");
    // [Phase 4] Clean up MDI state
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] About to call glBindBuffer\n");
    fflush(stdout);
    #endif
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    _SituationGLApplyBaselineRasterState();

    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] glBindBuffer complete, about to call third SIT_DEBUG_LOG\n");
    fflush(stdout);
    #endif

    SIT_DEBUG_LOG("[GLExecute] Inserting fence\n");
    // [Phase 1.5] Insert Fence for Ring Buffer Synchronization
    // We infer the frame index from the buffer pointer
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] About to calculate frame_idx\n");
    fflush(stdout);
    #endif
    int frame_idx = (int)(buf - sit_render.gl.soft_buffers);
    #ifdef SITUATION_OPENGL_DEBUG
    printf("[OpenGL Debug] frame_idx calculated: %d\n", frame_idx);
    fflush(stdout);
    #endif
    if (frame_idx >= 0 && frame_idx < SITUATION_MAX_FRAMES_IN_FLIGHT) {
        if (sit_render.gl.ring_fences[frame_idx]) {
            glDeleteSync(sit_render.gl.ring_fences[frame_idx]);
        }
        sit_render.gl.ring_fences[frame_idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Initializes the OpenGL resources for Virtual Display compositing.
 * @details Creates a dedicated VAO/VBO containing a full-screen quad used by
 *          `SituationRenderVirtualDisplays` to composite off-screen framebuffers.
 *          Restores the user's global VAO before returning.
 * @return `true` on success, `false` if GL resource creation fails.
 * @note Called once during `_SituationInitOpenGL`.
 */
static SituationError _SituationInitGLVirtualDisplayRenderer(void) {
    glCreateVertexArrays(1, &sit_render.gl.vd_quad_vao);
    glCreateBuffers(1, &sit_render.gl.vd_quad_vbo);
    if (sit_render.gl.vd_quad_vao == 0 || sit_render.gl.vd_quad_vbo == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitGLVirtualDisplayRenderer: Failed to create VD quad VAO/VBO.");
    }

    // Unit quad [0,1] — the model matrix scales to pixel size and the ortho projection
    // maps pixel coords to NDC. Using [0,1] means: scale by resolution = correct pixel rect.
    float quad_vertices[] = {
        // pos.x  pos.y   uv.x  uv.y
         0.0f,  0.0f,   0.0f, 1.0f,   // top-left
         1.0f,  0.0f,   1.0f, 1.0f,   // top-right
         0.0f,  1.0f,   0.0f, 0.0f,   // bottom-left
         1.0f,  1.0f,   1.0f, 0.0f    // bottom-right
    };
    glNamedBufferStorage(sit_render.gl.vd_quad_vbo, sizeof(quad_vertices), quad_vertices, 0);

    glBindVertexArray(sit_render.gl.vd_quad_vao);
    glVertexArrayVertexBuffer(sit_render.gl.vd_quad_vao, 0, sit_render.gl.vd_quad_vbo, 0, 4 * sizeof(float));
    glEnableVertexArrayAttrib(sit_render.gl.vd_quad_vao, SIT_ATTR_POSITION);
    glVertexArrayAttribFormat(sit_render.gl.vd_quad_vao, SIT_ATTR_POSITION, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(sit_render.gl.vd_quad_vao, SIT_ATTR_POSITION, 0);
    glEnableVertexArrayAttrib(sit_render.gl.vd_quad_vao, SIT_ATTR_TEXCOORD_0);
    glVertexArrayAttribFormat(sit_render.gl.vd_quad_vao, SIT_ATTR_TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(sit_render.gl.vd_quad_vao, SIT_ATTR_TEXCOORD_0, 0);
    glBindVertexArray(0);
    glBindVertexArray(sit_render.gl.global_vao_id);
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Performs one-time OpenGL backend initialization and context setup.
 *
 * @details This function is called exactly once during library startup (typically from
 *          `SituationInit` or the main initialization sequence) when the `SITUATION_USE_OPENGL`
 *          macro is defined. It is responsible for establishing a fully functional OpenGL
 *          rendering environment that the rest of the library can rely on.
 *
 *          Execution order (critical sequence do not reorder without care):
 *            1. Makes the GLFW window context current on the calling thread
 *               (`glfwMakeContextCurrent(sit_gs.sit_glfw_window)`)
 *            2. Loads OpenGL function pointers via GLAD
 *               (`gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)`)
 *            3. Checks minimum required OpenGL version (4.3+ core profile recommended)
 *               and logs error if not met
 *            4. Queries and caches key capabilities/extensions:
 *               - GL version string & GLSL version
 *               - `GL_ARB_bindless_texture` / `GL_EXT_bindless_texture` support
 *               - `GL_ARB_gl_spirv` for SPIR-V binary shaders
 *               - `GL_ARB_multi_bind` / `GL_ARB_direct_state_access` (if used)
 *               - Max texture units, max texture size, max compute workgroup sizes, etc.
 *            5. Sets global GL state defaults used by the library:
 *               - `glEnable(GL_BLEND)`, `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`
 *               - `glEnable(GL_DEPTH_TEST)` (optional, depending on init flags)
 *               - `glEnable(GL_CULL_FACE)`, `glCullFace(GL_BACK)`
 *               - `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` / `GL_PACK_ALIGNMENT`
 *            6. Initializes internal OpenGL resources:
 *               - Default white 1x1 texture for missing bindings
 *               - Dummy VAO for core-profile compatibility
 *               - Text renderer (calls `_SituationInitTextRenderer`)
 *               - Bindless texture support (calls `_SituationVirtualBindlessInit`)
 *               - Any other backend-specific singletons (shader cache, quad pipeline, etc.)
 *            7. Verifies no GL errors occurred during init (`SIT_CHECK_GL_ERROR()` macro)
 *            8. Sets `sit_render.gl_initialized = true` and other state flags
 *
 *          On failure (GLAD load fail, version too low, critical extension missing),
 *          logs detailed error messages and sets appropriate `SituationError` code.
 *          The library may continue in degraded mode (e.g. no bindless, no SPIR-V) or abort init.
 *
 * @param init_info Pointer to `SituationInitInfo` structure containing backend preferences,
 *                  window hints, feature toggles, etc. (may be NULL for defaults).
 *
 * @return SITUATION_SUCCESS if OpenGL context and required state initialized successfully,
 *         SITUATION_ERROR_GLAD_LOAD_FAILED if GLAD failed to load functions,
 *         SITUATION_ERROR_GL_VERSION_TOO_LOW if OpenGL version < required (e.g. 4.3),
 *         SITUATION_ERROR_GL_EXTENSION_MISSING for critical missing extensions,
 *         SITUATION_ERROR_MEMORY_ALLOCATION if internal resource creation failed,
 *         or other backend-specific errors propagated from sub-init calls.
 *
 * @note Must be called **with a valid GLFW window context current** on the calling thread.
 *       Thread safety: Only safe from the main thread or thread that owns the context
 *       during library initialization not reentrant or thread-safe afterward.
 *       If render thread is enabled, context is released after this function so render
 *       thread can acquire it.
 *
 *       Critical dependency chain:
 *         - GLFW window must exist (`sit_gs.sit_glfw_window != NULL`)
 *         - `glfwMakeContextCurrent` must succeed before GLAD load
 *         - All subsequent GL calls assume context is current
 *
 * @see SituationInit (primary caller), _SituationInitTextRenderer,
 *      _SituationVirtualBindlessInit, SIT_CHECK_GL_ERROR macro,
 *      SITUATION_ERROR_GLAD_LOAD_FAILED, SITUATION_ERROR_GL_VERSION_TOO_LOW
 */
#if defined(SITUATION_USE_OPENGL)
/** Probe default framebuffer RGB bit depth after GLFW 10-bit hints (Phase 3). */
static void _SituationOpenGLSetOutputColorDepthFromFramebuffer(void) {
    bool active_10bit = false;
    if (_SituationWants10BitOutput(sit_gs.output_color_depth_policy) && sit_gs.sit_glfw_window) {
        /* Query via GL after GLAD init (GLFW framebuffer attribs are not window attribs). */
        GLint red_bits = 0, green_bits = 0, blue_bits = 0;
        glGetIntegerv(0x0D52, &red_bits);   /* GL_RED_BITS */
        glGetIntegerv(0x0D53, &green_bits);  /* GL_GREEN_BITS */
        glGetIntegerv(0x0D54, &blue_bits);   /* GL_BLUE_BITS */
        if (red_bits >= 10 && green_bits >= 10 && blue_bits >= 10) {
            active_10bit = true;
            #ifdef SITUATION_OPENGL_DEBUG
            printf("Situation [OpenGL Debug]: default framebuffer %d/%d/%d RGB bits (10-bit SDR active)\n",
                red_bits, green_bits, blue_bits);
            fflush(stdout);
            #endif
        } else if (sit_gs.output_color_depth_policy == SIT_OUTPUT_COLOR_10BIT) {
            printf("Situation [OpenGL]: 10-bit output requested but default FB reports %d/%d/%d RGB bits — using 8-bit\n",
                red_bits, green_bits, blue_bits);
            fflush(stdout);
        }
    }
    _SituationSetOutputColorDepthState(active_10bit, false);
}
#endif

static SituationError _SituationInitOpenGL(const SituationInitInfo* init_info) {
    // --- 1. Context and Function Loading ---
    glfwMakeContextCurrent(sit_gs.sit_glfw_window); // Ensure context is current for GLAD

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_LOADER_FAILED, "_SituationInitOpenGL: GLAD failed to load function pointers.");
        return SITUATION_ERROR_OPENGL_LOADER_FAILED;
    }

    // --- 2. OpenGL Version and Extension Checks ---
    _SituationVirtualBindlessInit();

    if (GLVersion.major < 4 || (GLVersion.major == 4 && GLVersion.minor < 6)) {
        char detail[128];
        snprintf(detail, sizeof(detail), "_SituationInitOpenGL: OpenGL 4.6 not supported by the driver. Found version %d.%d", GLVersion.major, GLVersion.minor);
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION, detail);
        return SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION;
    }

    // Validate required core features/extensions for our abstraction.
    if (!GLAD_GL_ARB_direct_state_access) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_UNSUPPORTED, "_SituationInitOpenGL: Required extension GL_ARB_direct_state_access is not available.");
        return SITUATION_ERROR_OPENGL_UNSUPPORTED;
    }

    // Check optional extension for SPIR-V if compiler is enabled.
#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    // Note: Absence of ARB_gl_spirv is NOT a fatal error. It just means we must fallback to GLSL.
    // The refactored _SituationCreateGLComputeProgram handles this logic.
    sit_render.gl.arb_spirv_available = GLAD_GL_ARB_gl_spirv;
#endif // SITUATION_ENABLE_SHADER_COMPILER
    sit_render.gl.parallel_shader_compile_available =
        (GLAD_GL_KHR_parallel_shader_compile != 0) || (GLAD_GL_ARB_parallel_shader_compile != 0);

    sit_render.gl.screenshot_resolved_frame_index = -1;
    sit_render.gl.screenshot_capture_epoch = 0u;
    sit_render.gl.screenshot_buffer_epoch = 0u;
    sit_render.gl.screenshot_mutex_initialized = false;
    if (mtx_init(&sit_render.gl.screenshot_mutex, mtx_plain) == thrd_success) {
        sit_render.gl.screenshot_mutex_initialized = true;
    }
    for (int _si = 0; _si < SITUATION_MAX_FRAMES_IN_FLIGHT; ++_si) {
        atomic_init(&sit_render.gl.screenshot_urgent[_si], 0);
        sit_render.gl.screenshot_request_pending[_si] = false;
    }

    // [v2.4.206] Cache GL limits for main-thread access (GL context belongs to render thread after init)
    {
        GLint max_vp = 1;
        glGetIntegerv(GL_MAX_VIEWPORTS, &max_vp);
        sit_render.cached_max_viewports = (max_vp >= 1) ? (int)max_vp : 1;
    }

    // --- 3. VAO Abstraction Initialization ---
    // Create and bind the SINGLE, GLOBAL VAO for all USER rendering (Dynamic/Custom).
    glCreateVertexArrays(1, &sit_render.gl.global_vao_id);

    // [2.3.19] Create the Shared Mesh VAO (PBR Standard Layout)
    // This enables context sharing for meshes, as VAOs are not shared but VBOs are.
    // We configure this VAO once on the render thread and bind shared VBOs to it at draw time.
    glCreateVertexArrays(1, &sit_render.gl.mesh_vao_id);

    // Configure Mesh VAO Layout (Interleaved: Pos3, Norm3, Tan4, UV2)
    // Stride = 12 floats (48 bytes)
    // Binding Index 0
    GLuint mvao = sit_render.gl.mesh_vao_id;

    // Pos (0)
    glEnableVertexArrayAttrib(mvao, SIT_ATTR_POSITION);
    glVertexArrayAttribFormat(mvao, SIT_ATTR_POSITION, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(mvao, SIT_ATTR_POSITION, 0);

    // Norm (1)
    glEnableVertexArrayAttrib(mvao, SIT_ATTR_NORMAL);
    glVertexArrayAttribFormat(mvao, SIT_ATTR_NORMAL, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
    glVertexArrayAttribBinding(mvao, SIT_ATTR_NORMAL, 0);

    // Tan (4)
    glEnableVertexArrayAttrib(mvao, SIT_ATTR_TANGENT);
    glVertexArrayAttribFormat(mvao, SIT_ATTR_TANGENT, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float));
    glVertexArrayAttribBinding(mvao, SIT_ATTR_TANGENT, 0);

    // UV (2)
    glEnableVertexArrayAttrib(mvao, SIT_ATTR_TEXCOORD_0);
    glVertexArrayAttribFormat(mvao, SIT_ATTR_TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float));
    glVertexArrayAttribBinding(mvao, SIT_ATTR_TEXCOORD_0, 0);

    if (sit_render.gl.global_vao_id == 0 || sit_render.gl.mesh_vao_id == 0) {
         _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitOpenGL: Failed to create global VAOs.");
         return SITUATION_ERROR_OPENGL_GENERAL;
    }
    glBindVertexArray(sit_render.gl.global_vao_id);
    SIT_CHECK_GL_ERROR(); // Check for errors after binding

    // --- 4. Internal Renderer Initialization ---
    // Initialize internal renderers (Quad Renderer, Virtual Display Renderer).
    // These functions MUST create, configure, and then unbind their own PRIVATE VAOs/VBOs.
    // They MUST leave sit_render.gl.global_vao_id bound upon successful return.
    {
        SituationError quad_err = _SituationInitQuadRenderer(sit_gs.main_window_width, sit_gs.main_window_height);
        if (quad_err != SITUATION_SUCCESS) {
            glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
            glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
            sit_render.gl.global_vao_id = 0;
            sit_render.gl.mesh_vao_id = 0;
            return quad_err;
        }
        SituationError ypq_err = _SituationInitYpqGradeRenderer(sit_gs.main_window_width, sit_gs.main_window_height);
        if (ypq_err != SITUATION_SUCCESS) {
            glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
            glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
            sit_render.gl.global_vao_id = 0;
            sit_render.gl.mesh_vao_id = 0;
            return ypq_err;
        }
    }

#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [OpenGL]: Initializing default font...\n"); fflush(stdout);
#endif

    {
        SituationError font_err = _SituationInitDefaultFont();
        if (font_err != SITUATION_SUCCESS) {
            glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
            glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
            sit_render.gl.global_vao_id = 0;
            sit_render.gl.mesh_vao_id = 0;
            return font_err;
        }
    }

#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [OpenGL]: Default font initialized\n"); fflush(stdout);
    printf("Situation [OpenGL]: About to initialize text renderer...\n"); fflush(stdout);
#endif

    {
        SituationError text_err = _SituationInitTextRenderer();
        if (text_err != SITUATION_SUCCESS) {
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        printf("Situation [OpenGL]: Text renderer init FAILED\n"); fflush(stdout);
#endif
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
        sit_render.gl.global_vao_id = 0;
        sit_render.gl.mesh_vao_id = 0;
        return text_err;
        }
    }
    
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [OpenGL]: Text renderer initialized\n"); fflush(stdout);
    printf("Situation [OpenGL]: Creating virtual display shaders...\n"); fflush(stdout);
#endif

    // --- Initialize Virtual Display System ---
    // This involves creating shaders, setting up the VD quad renderer (with its own VAO/VBO), and initializing UBOs used for compositing.
    SituationError shader_err_code = SITUATION_SUCCESS;

    // a. Create Shaders for Virtual Display Compositing (precompiled SPIR-V embed — same headers as Vulkan)
    sit_render.gl.vd_shader_program_id = _SituationCreateGLVdCompositorShaderProgram(true, &shader_err_code);
    if (shader_err_code != SITUATION_SUCCESS) {
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        printf("Situation [OpenGL]: VD shader creation FAILED: %d\n", shader_err_code); fflush(stdout);
#endif
        /* Error detail (incl. driver link log) already set by _SituationCreateGLShaderProgramFromSpirv. */
        // Cleanup VAOs
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
        sit_render.gl.global_vao_id = 0;
        sit_render.gl.mesh_vao_id = 0;
        return shader_err_code;
    }

    sit_render.gl.composite_shader_program_id = _SituationCreateGLVdCompositorShaderProgram(false, &shader_err_code);
    if (shader_err_code != SITUATION_SUCCESS) {
        /* Error detail already set by _SituationCreateGLShaderProgramFromSpirv. */
        // Cleanup VAOs and first shader
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id);
        sit_render.gl.global_vao_id = 0;
        sit_render.gl.mesh_vao_id = 0;
        glDeleteProgram(sit_render.gl.vd_shader_program_id);
        sit_render.gl.vd_shader_program_id = 0;
        return shader_err_code;
    }

    // --- Bind sampler uniforms to correct texture units (Bug 7 fix) ---
    // VD compositor GL path uses precompiled SPIR-V (names stripped); use explicit layout(location=).
    // Textures bind to units SIT_SAMPLER_BINDING_SOURCE_0/1 (4/5); tell samplers which units to use.
    {
        glProgramUniform1i(sit_render.gl.vd_shader_program_id, SIT_UNIFORM_LOC_VD_SCREEN_TEXTURE, SIT_SAMPLER_BINDING_SOURCE_0);
        glProgramUniform1i(sit_render.gl.composite_shader_program_id, SIT_UNIFORM_LOC_COMPOSITE_SOURCE, SIT_SAMPLER_BINDING_SOURCE_0);
        glProgramUniform1i(sit_render.gl.composite_shader_program_id, SIT_UNIFORM_LOC_COMPOSITE_DEST, SIT_SAMPLER_BINDING_SOURCE_1);
    }

    {
        GLuint block = glGetUniformBlockIndex(sit_render.gl.vd_shader_program_id, "SitTpConfigBlock");
        if (block != GL_INVALID_INDEX) {
            glUniformBlockBinding(sit_render.gl.vd_shader_program_id, block, SIT_UBO_BINDING_VD_PATTERN);
        }
        block = glGetUniformBlockIndex(sit_render.gl.composite_shader_program_id, "SitTpConfigBlock");
        if (block != GL_INVALID_INDEX) {
            glUniformBlockBinding(sit_render.gl.composite_shader_program_id, block, SIT_UBO_BINDING_VD_PATTERN);
        }
    }

    glCreateBuffers(1, &sit_render.gl.vd_pattern_config_ubo_id);
    if (sit_render.gl.vd_pattern_config_ubo_id != 0) {
        glNamedBufferStorage(sit_render.gl.vd_pattern_config_ubo_id, (GLsizeiptr)SIT_VD_STANDBY_HEADER_UBO_SIZE, NULL, GL_DYNAMIC_STORAGE_BIT);
        glBindBufferBase(GL_UNIFORM_BUFFER, SIT_UBO_BINDING_VD_PATTERN, sit_render.gl.vd_pattern_config_ubo_id);
    }
    glCreateBuffers(1, &sit_render.gl.vd_pattern_config_ssbo_id);
    if (sit_render.gl.vd_pattern_config_ssbo_id != 0) {
        glNamedBufferStorage(sit_render.gl.vd_pattern_config_ssbo_id, (GLsizeiptr)SIT_VD_STANDBY_PARAMS_SSBO_SIZE, NULL, GL_DYNAMIC_STORAGE_BIT);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SIT_SSBO_BINDING_VD_PATTERN, sit_render.gl.vd_pattern_config_ssbo_id);
    }

    // b. Initialize the Virtual Display Quad Renderer
    // This function is responsible for creating sit_render.gl.vd_quad_vao/vbo, configuring them for a simple textured quad, and unbinding them, ensuring sit_render.gl.global_vao_id is bound again at the end.
    // You need to implement this function, similar to _SituationInitQuadRenderer.
    {
        SituationError vd_err = _SituationInitGLVirtualDisplayRenderer();
        if (vd_err != SITUATION_SUCCESS) {
         // Cleanup global VAO and shaders
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        sit_render.gl.global_vao_id = 0;
        glDeleteProgram(sit_render.gl.vd_shader_program_id);
        sit_render.gl.vd_shader_program_id = 0;
        glDeleteProgram(sit_render.gl.composite_shader_program_id);
        sit_render.gl.composite_shader_program_id = 0;
        return vd_err;
        }
    }

    // c. Create UBO for View/Projection data (used by user shaders, potentially internal ones too)
    glCreateBuffers(1, &sit_render.gl.view_data_ubo_id);
    if (sit_render.gl.view_data_ubo_id == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitOpenGL: Failed to create View UBO.");
        // Cleanup global VAO, shaders, and VD renderer resources
        glDeleteVertexArrays(1, &sit_render.gl.global_vao_id);
        sit_render.gl.global_vao_id = 0;
        glDeleteProgram(sit_render.gl.vd_shader_program_id);
        sit_render.gl.vd_shader_program_id = 0;
        glDeleteProgram(sit_render.gl.composite_shader_program_id);
        sit_render.gl.composite_shader_program_id = 0;
        // Assume _SituationInitVirtualDisplayRenderer cleaned up after itself on failure
        // Assume _SituationInitQuadRenderer cleaned up after itself on failure
        return SITUATION_ERROR_OPENGL_GENERAL;
    }
    // Allocate storage. Initial data can be set later or here if needed.
    glNamedBufferStorage(sit_render.gl.view_data_ubo_id, sizeof(ViewDataUBO), NULL, GL_DYNAMIC_STORAGE_BIT);
    // Bind it to the standard binding point. This binding is persistent.
    glBindBufferBase(GL_UNIFORM_BUFFER, SIT_UBO_BINDING_VIEW_DATA, sit_render.gl.view_data_ubo_id);
    SIT_CHECK_GL_ERROR();

    // --- [Phase 1] Initialize Persistent Ring Buffer ---
    SIT_RETURN_IF_ERR(_SituationInitGLRingBuffer());
    SIT_CHECK_GL_ERROR();

    // --- [Phase 1.5] Initialize Ring Buffer Fences ---
    SIT_RETURN_IF_ERR(_SituationInitGLRingFences());

    // --- [Phase 4] Initialize Multi-Draw Indirect Buffer ---
    SIT_RETURN_IF_ERR(_SituationInitGLMDIBuffer());
    SIT_CHECK_GL_ERROR();

#if SIT_GL_SHADER_CACHE_ENABLE
    _SitGLProgramCacheInit(&sit_render.gl.program_cache);
#endif

    // d. Initialize Virtual Display Slots (Data structures)
    for (int i = 0; i < SITUATION_MAX_VIRTUAL_DISPLAYS; ++i) {
        sit_render.virtual_display_slots_used[i] = false;
        // Ensure other members of sit_render.virtual_display_slots[i] are initialized if needed
    }
    sit_render.active_virtual_display_count = 0;
    sit_render.active_occlusion_pool_slot = -1;
    // Note: Virtual Display *textures/framebuffers* are created on-demand when VDs are created by the user.

    // --- 5. Initial GL State Configuration ---
    if (init_info->initial_active_window_flags & SITUATION_FLAG_VSYNC_HINT) {
        glfwSwapInterval(1);
        sit_render.gl.last_applied_swap_interval = 1;
    } else {
        glfwSwapInterval(0);
        sit_render.gl.last_applied_swap_interval = 0;
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Default clear color

    // --- 6. Finalize ---
    // CRITICAL: Ensure the global_vao_id is the active VAO at the end of initialization.
    // While it should already be bound from step 3, re-binding explicitly after potentially complex internal renderer setups is a good defensive practice.
    glBindVertexArray(sit_render.gl.global_vao_id);
    SIT_CHECK_GL_ERROR();

    // Set the renderer type
    sit_render.renderer_type = SIT_RENDERER_OPENGL;

    // --- Populate Enabled Features Mask for OpenGL ---
    sit_render.enabled_features_mask = 0; // Clear first
    // Basic features implied by GL 4.6
    sit_render.enabled_features_mask |= SIT_FEATURE_GEOMETRY_SHADER;
    sit_render.enabled_features_mask |= SIT_FEATURE_TESSELLATION_SHADER;
    sit_render.enabled_features_mask |= SIT_FEATURE_WIDE_LINES;
    sit_render.enabled_features_mask |= SIT_FEATURE_FILL_MODE_NON_SOLID;
    sit_render.enabled_features_mask |= SIT_FEATURE_SAMPLER_ANISOTROPY;
    sit_render.enabled_features_mask |= SIT_FEATURE_COMPUTE_SHADER;
    sit_render.enabled_features_mask |= SIT_FEATURE_INT64;
    sit_render.enabled_features_mask |= SIT_FEATURE_FLOAT64;
    sit_render.enabled_features_mask |= SIT_FEATURE_DRAW_INDIRECT_COUNT; // Core in 4.6 (GL_ARB_indirect_parameters)
    sit_render.enabled_features_mask |= SIT_FEATURE_MULTI_DRAW_INDIRECT; // Core in 4.3
    sit_render.enabled_features_mask |= SIT_FEATURE_MULTI_VIEWPORT;      // Core in 4.1 (GL_ARB_viewport_array)

    // Extension-based features
#if defined(GLAD_GL_NV_shader_buffer_load) && defined(GLAD_GL_EXT_buffer_reference)
    if (GLAD_GL_NV_shader_buffer_load || GLAD_GL_EXT_buffer_reference) {
        sit_render.enabled_features_mask |= SIT_FEATURE_BINDLESS_BUFFERS;
    }
#endif
#if defined(GLAD_GL_ARB_bindless_texture) && defined(GLAD_GL_ARB_gpu_shader_int64)
    if (GLAD_GL_ARB_bindless_texture && GLAD_GL_ARB_gpu_shader_int64) {
        sit_render.enabled_features_mask |= SIT_FEATURE_BINDLESS_TEXTURES;
    }
#endif
#if defined(GLAD_GL_NV_mesh_shader) && defined(GLAD_GL_EXT_mesh_shader)
    if (GLAD_GL_NV_mesh_shader || GLAD_GL_EXT_mesh_shader) {
        sit_render.enabled_features_mask |= SIT_FEATURE_MESH_SHADER;
    }
#endif
#if defined(GLAD_GL_KHR_shader_subgroup)
    if (GLAD_GL_KHR_shader_subgroup) {
        sit_render.enabled_features_mask |= SIT_FEATURE_SUBGROUP_OPERATIONS;
    }
#endif
#if defined(GLAD_GL_AMD_gpu_shader_half_float) || defined(GLAD_GL_NV_gpu_shader5)
    if (GLAD_GL_AMD_gpu_shader_half_float || GLAD_GL_NV_gpu_shader5) {
        sit_render.enabled_features_mask |= SIT_FEATURE_FLOAT16;
    }
#endif
#if defined(GLAD_GL_NV_shader_atomic_float)
    if (GLAD_GL_NV_shader_atomic_float) {
        sit_render.enabled_features_mask |= SIT_FEATURE_ATOMIC_FLOAT;
    }
#endif
#if defined(GLAD_GL_EXT_texture_compression_s3tc)
    if (GLAD_GL_EXT_texture_compression_s3tc) {
        sit_render.enabled_features_mask |= SIT_FEATURE_TEXTURE_COMPRESSION_BC;
    }
#endif
#if defined(GLAD_GL_KHR_texture_compression_astc_ldr)
    if (GLAD_GL_KHR_texture_compression_astc_ldr) {
        sit_render.enabled_features_mask |= SIT_FEATURE_TEXTURE_COMPRESSION_ASTC;
    }
#endif
    // 10-bit output is enabled only when the active swapchain/FB confirms it (Phase 1/3).
    _SituationOpenGLSetOutputColorDepthFromFramebuffer();

    // [Phase 2.5] Initialize VAO Cache & Graveyard
    // Zero cache is handled by SIT_CALLOC of context.
    // Initialize Graveyards
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; ++i) {
        if (ma_mutex_init(&sit_render.gl.graveyards[i].lock) != MA_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_INIT_FAILED, "Failed to init GL graveyard mutex.");
            return SITUATION_ERROR_INIT_FAILED;
        }
        // Pre-allocate arrays
        sit_render.gl.graveyards[i].mesh_capacity = 32;
        sit_render.gl.graveyards[i].mesh_ids_to_clean = (uint64_t*)SIT_MALLOC(32 * sizeof(uint64_t));
        sit_render.gl.graveyards[i].buffer_capacity = 32;
        sit_render.gl.graveyards[i].buffers_to_delete = (GLuint*)SIT_MALLOC(32 * sizeof(GLuint));
        sit_render.gl.graveyards[i].texture_capacity = 32;
        sit_render.gl.graveyards[i].textures_to_delete = (GLuint*)SIT_MALLOC(32 * sizeof(GLuint));
        if (!sit_render.gl.graveyards[i].mesh_ids_to_clean || !sit_render.gl.graveyards[i].buffers_to_delete || !sit_render.gl.graveyards[i].textures_to_delete) {
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        sit_render.gl.frame_fences[i] = 0;
    }

    // [Phase 2] Initialize Threading Support (Loader Window Only)
    #if !defined(__STDC_NO_THREADS__)
    // 1. Create Loader Window (Hidden, Shares Context with Main Window)
    // This window is used by the main thread for async asset loading while the render thread uses the main window.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    sit_render.gl.loader_window = glfwCreateWindow(640, 480, "Situation Loader", NULL, sit_gs.sit_glfw_window);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    // [v2.3.21] Thread spawning logic moved to _SituationInitRenderThread in SituationInit
    // Note: Context handover logic is also moved there.
    #endif

    {
        SituationError gpu_prof_err = _SitGpuProfInit();
        if (gpu_prof_err != SITUATION_SUCCESS && gpu_prof_err != SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED) {
            return gpu_prof_err;
        }
    }

    return SITUATION_SUCCESS;
}

#if defined(SITUATION_USE_OPENGL)
/** Main/loader thread GL context for shader compile, SPIR-V link, resource upload, and uniform reflection (not the render thread).
 *  No-op when the target window is already current (avoids redundant shadow-state invalidation). */
static void _SituationMakeGLContextCurrentForHostThread(void) {
    GLFWwindow* win = sit_gs.sit_glfw_window;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled && sit_render.gl.loader_window) {
        win = sit_render.gl.loader_window;
    }
#endif
    if (!win) {
        return;
    }
    if (glfwGetCurrentContext() == win) {
        return;
    }
    glfwMakeContextCurrent(win);
    _SitGLInvalidateShadowState();
    /* Host context binds (loader window) can disturb WGL swap-interval state for the
     * present window; force a refresh before the next SwapBuffers on the render thread. */
    sit_render.gl.last_applied_swap_interval = -1;
}

/** Release host compile context so the render thread can use the main GLFW context. */
static void _SituationReleaseHostGLContextForRenderThread(void) {
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled && glfwGetCurrentContext() != NULL) {
        glfwMakeContextCurrent(NULL);
        sit_render.gl.last_applied_swap_interval = -1;
    }
#endif
}

/** Drop loader context after mid-frame host GL (texture upload, uniform reflection, etc.). */
static void _SituationReleaseHostGLContextIfInFrame(void) {
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled && sit_render.in_frame) {
        _SituationReleaseHostGLContextForRenderThread();
    }
#endif
}

/** Apply glfwSwapInterval at present. Uncapped (interval 0) is re-applied every swap so
 * host-thread loader binds cannot leave vsync effectively on; vsync-on uses change-detection. */
static void _SitGLApplySwapIntervalBeforePresent(void) {
#ifndef SITUATION_USE_VULKAN
    int desired_interval = (sit_gs.active_profile_window_flags & SITUATION_FLAG_VSYNC_HINT) ? 1 : 0;
    if (desired_interval == 0 ||
        desired_interval != sit_render.gl.last_applied_swap_interval) {
        glfwSwapInterval(desired_interval);
        sit_render.gl.last_applied_swap_interval = desired_interval;
    }
#endif
}

/** Fill uniform location cache after link (SPIR-V / GLSL). SPIR-V: use glGetUniformLocation (GL_LOCATION from resource query is often -1). */
static SituationError _SituationPopulateGLShaderUniformMap(GLuint program, _SituationUniformMap* map) {
    if (!map || program == 0 || !glIsProgram(program)) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "_SituationPopulateGLShaderUniformMap: invalid program or map.");
    }

    GLint uniform_count = 0;
    glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &uniform_count);
    for (GLint j = 0; j < uniform_count; j++) {
        GLenum size_prop = GL_ARRAY_SIZE;
        GLint array_size = 1;
        glGetProgramResourceiv(program, GL_UNIFORM, (GLuint)j, 1, &size_prop, 1, NULL, &array_size);
        if (array_size < 1) {
            array_size = 1;
        }

        GLenum block_prop = GL_BLOCK_INDEX;
        GLint block_index = -1;
        glGetProgramResourceiv(program, GL_UNIFORM, (GLuint)j, 1, &block_prop, 1, NULL, &block_index);
        if (block_index >= 0) {
            continue; /* std140 UBO member — not a standalone glProgramUniform location */
        }

        char name[256];
        GLsizei name_len = 0;
        glGetProgramResourceName(program, GL_UNIFORM, (GLuint)j, (GLsizei)sizeof(name), &name_len, name);
        if (name_len <= 0) {
            continue;
        }
        if (name_len >= (GLsizei)sizeof(name)) {
            name_len = (GLsizei)sizeof(name) - 1;
        }
        name[name_len] = '\0';

        GLint location = -1;
        GLenum loc_prop = GL_LOCATION;
        glGetProgramResourceiv(program, GL_UNIFORM, (GLuint)j, 1, &loc_prop, 1, NULL, &location);
        if (location < 0) {
            location = glGetUniformLocation(program, name);
        }
        if (location >= 0) {
            SituationError set_err = _sit_uniform_map_set(map, name, location);
            if (set_err != SITUATION_SUCCESS) {
                return set_err;
            }
        }

        char base[256];
        strncpy(base, name, sizeof(base) - 1u);
        base[sizeof(base) - 1u] = '\0';
        char* bracket = strchr(base, '[');
        if (bracket) {
            *bracket = '\0';
        }
        for (GLint k = 0; k < array_size; k++) {
            char indexed[256];
            snprintf(indexed, sizeof(indexed), "%s[%d]", base, (int)k);
            GLint loc_k = glGetUniformLocation(program, indexed);
            if (loc_k >= 0) {
                SituationError set_err = _sit_uniform_map_set(map, indexed, loc_k);
                if (set_err != SITUATION_SUCCESS) {
                    return set_err;
                }
            }
        }
    }
    return SITUATION_SUCCESS;
}

/** Find SSBO/UBO block index by name (glGetProgramResourceIndex + active-resource name scan). */
static GLuint _SituationGLFindProgramResourceIndex(GLuint program, GLenum resource_interface, const char* name) {
    if (!program || !name) {
        return GL_INVALID_INDEX;
    }

    GLuint idx = glGetProgramResourceIndex(program, resource_interface, name);
    if (idx != GL_INVALID_INDEX) {
        return idx;
    }

    GLint count = 0;
    glGetProgramInterfaceiv(program, resource_interface, GL_ACTIVE_RESOURCES, &count);
    for (GLint i = 0; i < count; i++) {
        char res_name[256];
        GLsizei name_len = 0;
        glGetProgramResourceName(program, resource_interface, (GLuint)i, (GLsizei)sizeof(res_name), &name_len, res_name);
        if (name_len <= 0) {
            continue;
        }
        if (name_len >= (GLsizei)sizeof(res_name)) {
            name_len = (GLsizei)sizeof(res_name) - 1;
        }
        res_name[name_len] = '\0';
        if (strcmp(res_name, name) == 0) {
            return (GLuint)i;
        }
    }
    return GL_INVALID_INDEX;
}

/** Match active block whose GL_BUFFER_BINDING equals binding_point (SPIR-V name fallback). */
static GLuint _SituationGLFindBlockIndexByBinding(GLuint program, GLenum resource_interface, uint32_t binding_point) {
    GLint count = 0;
    glGetProgramInterfaceiv(program, resource_interface, GL_ACTIVE_RESOURCES, &count);
    for (GLint i = 0; i < count; i++) {
        GLenum binding_prop = GL_BUFFER_BINDING;
        GLint reported = -1;
        glGetProgramResourceiv(
            program, resource_interface, (GLuint)i, 1, &binding_prop, 1, NULL, &reported);
        if (reported >= 0 && (uint32_t)reported == binding_point) {
            return (GLuint)i;
        }
    }
    if (binding_point < (uint32_t)count) {
        return binding_point;
    }
    return GL_INVALID_INDEX;
}


/**
 * @brief [INTERNAL] One-time initialization for OpenGL bindless texture support in virtual display paths.
 *
 * @details Called once during library startup (usually from `SituationInit` or first virtual display creation)
 *          to check for and prepare bindless texture functionality when using OpenGL backend.
 *
 *          What it does:
 *            - Checks for `GL_ARB_bindless_texture` extension availability
 *              (via `glfwExtensionSupported` or `glGetStringi`)
 *            - Logs warning if missing (virtual displays may fall back to legacy binding)
 *            - Caches extension function pointers if using GLAD or manual loading
 *              (`glGetTextureHandleARB`, `glMakeTextureHandleResidentARB`, etc.)
 *            - Pre-allocates or resets any bindless handle cache / resident table
 *            - Sets internal flag `sit_render.gl_supports_bindless` for later queries
 *
 *          After success, bindless operations become available for virtual display textures,
 *          improving performance by reducing texture unit pressure in multi-pass rendering.
 *
 * @return true if bindless support is available and initialized successfully,
 *         false if extension missing or initialization failed (logs internally)
 *
 * @note This function is idempotent safe to call multiple times.
 *       Thread safety: Must be called with GL context current (typically main/render thread during init).
 *       No runtime cost after init only queried via flag.
 *
 *       If bindless is unavailable, virtual display rendering falls back to traditional
 *       `glActiveTexture` + `glBindTexture` per draw call (slower in complex scenes).
 *
 * @see _SituationVirtualBindlessBind, SituationCreateVirtualDisplay,
 *      SITUATION_ERROR_GL_EXTENSION_MISSING, glGetTextureHandleARB
 */
static void _SituationVirtualBindlessInit(void) {
    for (int i = 0; i < SITUATION_GL_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
        _SituationGLVirtualTextureSlot* slot = &sit_render.gl.virtual_texture_slots[i];
        slot->texture_slot_index = i;
        slot->gl_texture_id = 0;
        slot->last_used_counter = 0;
        slot->is_active = false;
    }
    sit_render.gl.virtual_stats.hits = 0;
    sit_render.gl.virtual_stats.misses = 0;
    sit_render.gl.virtual_stats.evictions = 0;
    sit_render.gl.virtual_lru_counter = 0;
}

/**
 * @brief [INTERNAL] Binds a GL texture to a bindless texture unit for virtual display / offscreen use.
 *
 * @details This low-level helper records the necessary OpenGL state changes to make a texture
 *          accessible via bindless texture handles in shaders used by virtual displays.
 *
 *          It is called internally when:
 *            - A virtual display needs to sample a texture in its render pass
 *            - A texture is attached as a color attachment or input to a virtual framebuffer
 *            - Dynamic texture updates occur (e.g. after blitting or CPU upload)
 *
 *          Typical sequence:
 *            - Generates or retrieves a bindless texture handle via `glGetTextureHandleARB`
 *            - Makes the handle resident if not already (`glMakeTextureHandleResidentARB`)
 *            - Binds the handle to a uniform location or shader storage block
 *              (via `glProgramUniformHandleui64ARB` or equivalent)
 *
 *          This function exists to abstract away ARB_bindless_texture / EXT_bindless_texture
 *          boilerplate while ensuring compatibility with virtual display rendering paths.
 *
 * @param gl_texture_id OpenGL texture name (GLuint) that should be made bindless.
 *                      Must be a valid, complete texture object (has storage allocated).
 *
 * @return The 64-bit bindless handle (`GLuint64`) that was made resident and can be
 *         passed to shaders, or 0 on failure (invalid texture, extension missing, etc.).
 *
 * @note Requires `GL_ARB_bindless_texture` (or EXT equivalent) to be supported and loaded.
 *       If the extension is missing, logs a warning and returns 0.
 *       Handles are made resident once and stay resident until texture destruction
 *       or explicit `glMakeTextureHandleNonResidentARB`.
 *       Thread safety: Must be called with an active OpenGL context (typically render thread).
 *
 * @see _SituationVirtualBindlessInit, glGetTextureHandleARB, glMakeTextureHandleResidentARB,
 *      glProgramUniformHandleui64ARB, SITUATION_ERROR_GL_EXTENSION_MISSING
 */
static int _SituationVirtualBindlessBind(GLuint gl_texture_id) {
    sit_render.gl.virtual_lru_counter++;
    uint64_t current_counter = sit_render.gl.virtual_lru_counter;

    // 1. Check if the texture is already bound (Hit?)
    for (int i = 0; i < SITUATION_GL_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
        _SituationGLVirtualTextureSlot* slot = &sit_render.gl.virtual_texture_slots[i];
        if (slot->is_active && slot->gl_texture_id == gl_texture_id) {

            // Hit! Update LRU
            slot->last_used_counter = current_counter;
            sit_render.gl.virtual_stats.hits++;
            return i;
        }
    }

    // 2. Miss! Find a slot to evict
    sit_render.gl.virtual_stats.misses++;

    int best_slot = -1;
    uint64_t oldest_counter = UINT64_MAX;

    // First pass: look for empty slot
    for (int i = 0; i < SITUATION_GL_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
        if (!sit_render.gl.virtual_texture_slots[i].is_active) {
            best_slot = i;
            break;
        }
    }

    // Second pass: if full, find LRU
    if (best_slot == -1) {
        sit_render.gl.virtual_stats.evictions++;
        for (int i = 0; i < SITUATION_GL_MAX_VIRTUAL_TEXTURE_UNITS; i++) {
            if (sit_render.gl.virtual_texture_slots[i].last_used_counter < oldest_counter) {
                oldest_counter = sit_render.gl.virtual_texture_slots[i].last_used_counter;
                best_slot = i;
            }
        }
    }

    // Safety fallback
    if (best_slot == -1) best_slot = 0;

    // 3. Bind the texture to the chosen slot
    _SituationGLVirtualTextureSlot* slot = &sit_render.gl.virtual_texture_slots[best_slot];

    // Bind the actual texture to the texture unit using DSA
    glBindTextureUnit(best_slot, gl_texture_id);

    // Update slot metadata
    slot->is_active = true;
    slot->gl_texture_id = gl_texture_id;
    slot->last_used_counter = current_counter;

    return best_slot;
}

#endif /* SITUATION_USE_OPENGL (host GL helpers opened ~line 3408) */

#endif /* SITUATION_USE_OPENGL (renderer GL backend opened at line 1613) */

#if defined(SITUATION_USE_VULKAN)

/**
 * @brief [INTERNAL] Initializes Vulkan pipelines for the Virtual Display Compositing system.
 * @details This function is the second stage of internal renderer setup. It compiles and creates the specific graphics pipelines used by `SituationRenderVirtualDisplays` to draw off-screen framebuffers onto the main screen.
 *
 * @par Scope
 * This function creates two distinct pipelines:
 * 1. **Simple Compositor:** For standard blending (Alpha, Additive, Multiply). Uses a lightweight shader and single texture sampling.
 * 2. **Advanced Compositor:** For complex "Photoshop-style" blend modes (Overlay, Soft Light, etc.). Uses a specialized shader that samples both the source Virtual Display and a copy of the destination framebuffer.
 *
 * @note The standard 2D Quad Renderer (`SituationCmdDrawQuad`) is **not** initialized here; it is handled by the shared `_SituationInitQuadRenderer` function before this one is called.
 *
 * @par Initialization Sequence
 *   1. **Simple Compositor:**
 *      - Loads `sit/gpu/compositor.vert` (with `SIT_COMPOSITOR_PATH_B` define) and `sit/gpu/vd.frag` via `_SituationVulkanCompileCoreShaderFile`.
 *      - Creates a pipeline layout with: Set 0 (View UBO), Set 1 (Source Sampler), and Push Constants.
 *      - Creates the `vd_compositing_pipeline`.
 *   2. **Advanced Compositor:**
 *      - Loads `sit/gpu/compositor.vert` (with `SIT_COMPOSITOR_PATH_A` define) and `sit/gpu/composite.frag`.
 *      - Creates a pipeline layout with: Set 0 (View UBO), Set 1 (Source Sampler), Set 2 (Dest Sampler), and Push Constants.
 *      - Creates the `advanced_compositing_pipeline`.
 *
 * @return SITUATION_SUCCESS on successful initialization of both pipelines.
 * @return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED if shader compilation or pipeline creation fails. Cleanup is performed automatically on failure.
 * @return SITUATION_ERROR_SHADER_COMPILER_REQUIRED if `SITUATION_ENABLE_SHADER_COMPILER` is not defined at compile time. No pipelines are initialised; the caller must treat this as a fatal init error.
 *
 * @warning Requires `SITUATION_ENABLE_SHADER_COMPILER` to be defined. Without it, this function returns
 *          `SITUATION_ERROR_SHADER_COMPILER_REQUIRED` — it does **not** silently succeed with NULL pipelines.
 *          See `doc/plan/EXTERNALIZE_GPU_COMPUTE_PLAN.md` Phase 2 for the future precompiled SPIR-V path.
 *
 * @see _SituationInitVulkan(), _SituationInitQuadRenderer(), SituationRenderVirtualDisplays()
 */
static SituationError _SituationVulkanInitInternalRenderers(void) {
    // --- Initialize all local handles to NULL for robust cleanup ---
    // NOTE: Quad renderer is initialized separately by _SituationInitQuadRenderer()

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanInitInternalRenderers starting...\n"); fflush(stdout);
    #endif

    VkPipelineLayout text_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline       text_pipeline = VK_NULL_HANDLE;

    VkPipelineLayout vd_compositing_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline       vd_compositing_pipeline = VK_NULL_HANDLE;

    VkPipelineLayout advanced_compositing_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline       advanced_compositing_pipeline = VK_NULL_HANDLE;

    _SituationSpirvBlob vs_spirv = {};
    _SituationSpirvBlob fs_spirv = {};

    // ---------------------------------------------------------------------------------
    // CRITICAL CHECK: Only proceed if Shader Compiler is enabled.
    // Internal renderers require runtime GLSL→SPIR-V compilation via shaderc.
    // Phase 2 (precompiled SPIR-V embed) would lift this restriction — see
    // doc/plan/EXTERNALIZE_GPU_COMPUTE_PLAN.md Phase 2.
    // ---------------------------------------------------------------------------------
#if !defined(SITUATION_ENABLE_SHADER_COMPILER)
    return _SituationSetErrorFromCode(
        SITUATION_ERROR_SHADER_COMPILER_REQUIRED,
        "_SituationVulkanInitInternalRenderers: SITUATION_ENABLE_SHADER_COMPILER is not defined. "
        "Vulkan internal pipelines (VD compositing, quad, text, YPQ) cannot be initialized. "
        "Rebuild the library with shaderc support, or see doc/plan/EXTERNALIZE_GPU_COMPUTE_PLAN.md Phase 2 "
        "for the precompiled SPIR-V embed path (not yet implemented)."
    );
#else

    // ======================================================================================
    // --- 1. Initialize the Simple VD Compositing Renderer ---
    // ======================================================================================
    {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Compiling VD vertex shader...\n"); fflush(stdout);
        #endif
        {
            const _SituationShadercMacro vd_compositor_macros[] = { { "SIT_COMPOSITOR_PATH_B", "1" } };
            vs_spirv = _SituationVulkanCompileCoreShaderFile(
                SIT_GPU_PATH_COMPOSITOR_VERT, "internal_vd.vert", shaderc_vertex_shader,
                vd_compositor_macros, sizeof(vd_compositor_macros) / sizeof(vd_compositor_macros[0]));
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD vertex shader compiled, data=%p\n", vs_spirv.data); fflush(stdout);
        printf("Situation [Vulkan Debug]: Compiling VD fragment shader...\n"); fflush(stdout);
        #endif
        fs_spirv = _SituationVulkanCompileCoreShaderFile(
            SIT_GPU_PATH_VD_FRAG, "internal_vd.frag", shaderc_fragment_shader, NULL, 0);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD fragment shader compiled, data=%p\n", fs_spirv.data); fflush(stdout);
        #endif
        if (!vs_spirv.data || !fs_spirv.data) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: SHADER COMPILATION FAILED! vs_spirv.data=%p, fs_spirv.data=%p\n", vs_spirv.data, fs_spirv.data); fflush(stdout);
            #endif
            goto cleanup;
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD shaders compiled successfully\n"); fflush(stdout);
        #endif

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Creating VD pipeline layout...\n"); fflush(stdout);
        printf("Situation [Vulkan Debug]: Device=%p, view_data_ubo_layout=%p, image_sampler_layout=%p\n",
               (void*)sit_render.vk.device, (void*)sit_render.vk.view_data_ubo_layout, (void*)sit_render.vk.image_sampler_layout);
        fflush(stdout);
        #endif

        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = SIT_VD_PATH_B_PUSH_CONSTANT_SIZE
        };
        VkDescriptorSetLayout vd_set_layouts[3];
        uint32_t vd_set_layout_count = 1;
        vd_set_layouts[0] = sit_render.vk.view_data_ubo_layout;
        if (sit_render.vk.text_sampler_layout != VK_NULL_HANDLE) {
            vd_set_layouts[1] = sit_render.vk.text_sampler_layout;
        } else {
            vd_set_layouts[1] = sit_render.vk.image_sampler_layout;
        }
        vd_set_layouts[2] = sit_render.vk.vd_pattern_config_ubo_layout;
        vd_set_layout_count = 3;
        VkPipelineLayoutCreateInfo pipeline_layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = vd_set_layout_count,
            .pSetLayouts = vd_set_layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        };

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Calling vkCreatePipelineLayout...\n"); fflush(stdout);
        #endif
        VkResult layout_result = vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &vd_compositing_pipeline_layout);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: vkCreatePipelineLayout result=%d (VK_SUCCESS=0)\n", layout_result); fflush(stdout);
        #endif
        if (layout_result != VK_SUCCESS) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: PIPELINE LAYOUT CREATION FAILED!\n"); fflush(stdout);
            #endif
            goto cleanup;
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD pipeline layout created successfully, handle=%p\n", (void*)vd_compositing_pipeline_layout); fflush(stdout);
        #endif

        VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 2 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attr_desc = {};
        attr_desc.binding = 0;
        attr_desc.location = SIT_ATTR_POSITION;
        attr_desc.format = VK_FORMAT_R32G32_SFLOAT;
        attr_desc.offset = 0;

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Creating VD graphics pipeline...\n"); fflush(stdout);
        #endif
        static const uint32_t vd_simple_blend_flags[5] = {
            SIT_VK_PIPELINE_NO_DEPTH,
            SIT_VK_PIPELINE_BLEND_ADDITIVE | SIT_VK_PIPELINE_NO_DEPTH,
            SIT_VK_PIPELINE_BLEND_MULTIPLY | SIT_VK_PIPELINE_NO_DEPTH,
            SIT_VK_PIPELINE_BLEND_SCREEN | SIT_VK_PIPELINE_NO_DEPTH,
            SIT_VK_PIPELINE_BLEND_OPAQUE | SIT_VK_PIPELINE_NO_DEPTH,
        };
        for (int _vd_blend = 0; _vd_blend < 5; ++_vd_blend) {
            vd_compositing_pipeline = _SituationVulkanCreateGraphicsPipeline(
                vs_spirv.data, vs_spirv.size,
                fs_spirv.data, fs_spirv.size,
                vd_compositing_pipeline_layout,
                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                1, &binding_desc,
                1, &attr_desc,
                vd_simple_blend_flags[_vd_blend],
                VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL
            );
            if (vd_compositing_pipeline == VK_NULL_HANDLE) {
                goto cleanup;
            }
            sit_render.vk.vd_compositing_blend_pipelines[_vd_blend] = vd_compositing_pipeline;
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD graphics pipelines created (5 blend variants)\n"); fflush(stdout);
        #endif
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        if (sit_render.vk.vd_compositing_blend_pipelines[SITUATION_BLEND_ALPHA] == VK_NULL_HANDLE) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: VD GRAPHICS PIPELINE CREATION FAILED!\n"); fflush(stdout);
            #endif
            goto cleanup;
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: VD pipeline initialization complete\n"); fflush(stdout);
        #endif
    }

    // ======================================================================================
    // --- 2. Initialize the Advanced VD Compositing Renderer ---
    // ======================================================================================
    {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Starting Advanced Compositing initialization...\n"); fflush(stdout);
        #endif
        {
            const _SituationShadercMacro composite_compositor_macros[] = { { "SIT_COMPOSITOR_PATH_A", "1" } };
            vs_spirv = _SituationVulkanCompileCoreShaderFile(
                SIT_GPU_PATH_COMPOSITOR_VERT, "internal_composite.vert", shaderc_vertex_shader,
                composite_compositor_macros, sizeof(composite_compositor_macros) / sizeof(composite_compositor_macros[0]));
        }
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Advanced vertex shader compiled, data=%p\n", vs_spirv.data); fflush(stdout);
        #endif
        fs_spirv = _SituationVulkanCompileCoreShaderFile(
            SIT_GPU_PATH_COMPOSITE_FRAG, "internal_composite.frag", shaderc_fragment_shader, NULL, 0);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Advanced fragment shader compiled, data=%p\n", fs_spirv.data); fflush(stdout);
        #endif
        if (!vs_spirv.data || !fs_spirv.data) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: ADVANCED COMPOSITING SHADER COMPILATION FAILED!\n"); fflush(stdout);
            #endif
            goto cleanup;
        }

        VkDescriptorSetLayout layouts_adv[4];
        uint32_t adv_set_layout_count = 1;
        layouts_adv[0] = sit_render.vk.view_data_ubo_layout;
        if (sit_render.vk.text_sampler_layout != VK_NULL_HANDLE) {
            layouts_adv[1] = sit_render.vk.text_sampler_layout;
        } else {
            layouts_adv[1] = sit_render.vk.image_sampler_layout;
        }
        layouts_adv[2] = sit_render.vk.composite_dest_sampler_layout;
        layouts_adv[3] = sit_render.vk.vd_pattern_config_ubo_layout;
        adv_set_layout_count = 4;

        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset = 0,
            .size = SIT_VD_PATH_A_PUSH_CONSTANT_SIZE
        };

        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = adv_set_layout_count,
            .pSetLayouts = layouts_adv,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        };

        if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &advanced_compositing_pipeline_layout) != VK_SUCCESS) goto cleanup;

        VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 2 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attr_desc;
        attr_desc.binding = 0;
        attr_desc.location = SIT_ATTR_POSITION;
        attr_desc.format = VK_FORMAT_R32G32_SFLOAT;
        attr_desc.offset = 0;

        advanced_compositing_pipeline = _SituationVulkanCreateGraphicsPipeline(
            vs_spirv.data, vs_spirv.size,
            fs_spirv.data, fs_spirv.size,
            advanced_compositing_pipeline_layout,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            1, &binding_desc,
            1, &attr_desc,
            SIT_VK_PIPELINE_NO_DEPTH,
            VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL
        );

        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        if (advanced_compositing_pipeline == VK_NULL_HANDLE) goto cleanup;
    }

    // ======================================================================================
    // --- 3. Initialize the Batched Text Renderer ---
    // ======================================================================================
    {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Starting Text Renderer initialization...\n"); fflush(stdout);
        #endif
        vs_spirv = _SituationVulkanCompileCoreShaderFile(
            SIT_GPU_PATH_TEXT_VERT, "internal_text.vert", shaderc_vertex_shader, NULL, 0);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Text vertex shader compiled, data=%p\n", vs_spirv.data); fflush(stdout);
        #endif
        fs_spirv = _SituationVulkanCompileCoreShaderFile(
            SIT_GPU_PATH_TEXT_FRAG, "internal_text.frag", shaderc_fragment_shader, NULL, 0);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Text fragment shader compiled, data=%p\n", fs_spirv.data); fflush(stdout);
        #endif
        if (!vs_spirv.data || !fs_spirv.data) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: TEXT RENDERER SHADER COMPILATION FAILED!\n"); fflush(stdout);
            #endif
            goto cleanup;
        }

        VkDescriptorSetLayout layouts[2];
        uint32_t set_layout_count = 1;
        layouts[0] = sit_render.vk.view_data_ubo_layout;
        if (sit_render.vk.text_sampler_layout != VK_NULL_HANDLE) {
            layouts[1] = sit_render.vk.text_sampler_layout;
            set_layout_count = 2;
        }
        VkPushConstantRange push_constant_range = {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset = 0,
            .size = sizeof(vec4) + sizeof(uint32_t)
        };

        VkPipelineLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = set_layout_count,
            .pSetLayouts = layouts,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &push_constant_range
        };

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Creating text pipeline layout with 2 sets:\n");
        printf("Situation [Vulkan Debug]:   Set 0 (UBO): %p\n", (void*)layouts[0]);
        printf("Situation [Vulkan Debug]:   Set 1 (Sampler): %p\n", (void*)layouts[1]);
        fflush(stdout);
        #endif

        if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &text_pipeline_layout) != VK_SUCCESS) goto cleanup;

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Text pipeline layout created: %p\n", (void*)text_pipeline_layout);
        fflush(stdout);
        #endif

        VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 4 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attr_descs[2];
        attr_descs[0].binding = 0;
        attr_descs[0].location = SIT_ATTR_POSITION;
        attr_descs[0].format = VK_FORMAT_R32G32_SFLOAT;
        attr_descs[0].offset = 0;
        attr_descs[1].binding = 0;
        attr_descs[1].location = SIT_ATTR_TEXCOORD_0;
        attr_descs[1].format = VK_FORMAT_R32G32_SFLOAT;
        attr_descs[1].offset = 2 * sizeof(float);  // CRITICAL: UV comes after XY (8 bytes offset)

        text_pipeline = _SituationVulkanCreateGraphicsPipeline(
            vs_spirv.data, vs_spirv.size,
            fs_spirv.data, fs_spirv.size,
            text_pipeline_layout,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            1, &binding_desc,
            2, attr_descs,
            SIT_VK_PIPELINE_NO_DEPTH,
            VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL
        );
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        if (text_pipeline == VK_NULL_HANDLE) goto cleanup;
    }

    // --- Success ---
    // NOTE: quad_pipeline_layout, quad_pipeline, quad_vertex_buffer, and quad_vertex_buffer_memory
    // are initialized by _SituationInitQuadRenderer(), NOT here. Do not overwrite them!
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: All internal renderers initialized successfully!\n"); fflush(stdout);
    #endif
    sit_render.vk.text_pipeline = text_pipeline;
    sit_render.vk.text_pipeline_layout = text_pipeline_layout;
    sit_render.vk.vd_compositing_pipeline_layout = vd_compositing_pipeline_layout;
    sit_render.vk.vd_compositing_pipeline = sit_render.vk.vd_compositing_blend_pipelines[SITUATION_BLEND_ALPHA];
    sit_render.vk.advanced_compositing_pipeline_layout = advanced_compositing_pipeline_layout;
    sit_render.vk.advanced_compositing_pipeline = advanced_compositing_pipeline;

    return SITUATION_SUCCESS;

cleanup:
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanInitInternalRenderers FAILED - entering cleanup\n"); fflush(stdout);
    #endif

    _SituationFreeSpirvBlob(&vs_spirv);
    _SituationFreeSpirvBlob(&fs_spirv);
    // NOTE: Quad renderer resources are NOT cleaned up here - they're managed by _SituationCleanupQuadRenderer()
    if (text_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, text_pipeline_layout, NULL);
    if (text_pipeline) vkDestroyPipeline(sit_render.vk.device, text_pipeline, NULL);
    if (vd_compositing_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, vd_compositing_pipeline_layout, NULL);
    for (int _vd_blend = 0; _vd_blend < 5; ++_vd_blend) {
        if (sit_render.vk.vd_compositing_blend_pipelines[_vd_blend]) {
            vkDestroyPipeline(sit_render.vk.device, sit_render.vk.vd_compositing_blend_pipelines[_vd_blend], NULL);
            sit_render.vk.vd_compositing_blend_pipelines[_vd_blend] = VK_NULL_HANDLE;
        }
    }
    if (advanced_compositing_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, advanced_compositing_pipeline_layout, NULL);
    if (advanced_compositing_pipeline) vkDestroyPipeline(sit_render.vk.device, advanced_compositing_pipeline, NULL);
    return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

#endif // SITUATION_ENABLE_SHADER_COMPILER
}

/**
 * @brief [INTERNAL] Allocates resources for the screen copy operation.
 *
 * @details Creates a `VkImage`, `VkImageView`, and a persistent `VkDescriptorSet` specifically designed to hold a copy of the swapchain's backbuffer.
 *          This resource is used by the Advanced Compositing pipeline to read the destination color for blend modes like Overlay and Soft Light.
 *
 * @note This function is called automatically during swapchain creation/recreation to ensure the image dimensions match the window size.
 */
static SituationError _SituationVulkanCreateScreenCopyResource(void) {
    /* Caller must have composite_dest_sampler_layout and swapchain extent/format valid. */
    if (sit_render.vk.swapchain_extent.width == 0 || sit_render.vk.swapchain_extent.height == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED, "Screen copy: zero swapchain extent.");
    }

    // 1. Create the Image (Device Local, Usage: Transfer Dst + Sampled)
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (_SituationVulkanCreateImage(
        sit_render.vk.swapchain_extent.width,
        sit_render.vk.swapchain_extent.height,
        1,
        sit_render.vk.swapchain_image_format,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VMA_MEMORY_USAGE_GPU_ONLY,
        VK_SAMPLE_COUNT_1_BIT,
        &sit_render.vk.screen_copy_image,
        &sit_render.vk.screen_copy_memory
    ) != SITUATION_SUCCESS) {
        sit_render.vk.screen_copy_image = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_memory = VK_NULL_HANDLE;
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Screen copy: vkCreateImage failed.");
    }

    sit_render.vk.screen_copy_view = _SituationVulkanCreateImageView(
        sit_render.vk.screen_copy_image,
        sit_render.vk.swapchain_image_format,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    if (sit_render.vk.screen_copy_view == VK_NULL_HANDLE) {
        vmaDestroyImage(sit_render.vk.vma_allocator, sit_render.vk.screen_copy_image, sit_render.vk.screen_copy_memory);
        sit_render.vk.screen_copy_image = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_memory = VK_NULL_HANDLE;
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Screen copy: vkCreateImageView failed.");
    }

    sit_render.vk.screen_copy_descriptor_set = _SituationVulkanAllocateDescriptorSet(
        sit_render.vk.composite_dest_sampler_layout,
        &sit_render.vk.screen_copy_descriptor_pool
    );

    if (sit_render.vk.screen_copy_descriptor_set == VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.screen_copy_view, NULL);
        sit_render.vk.screen_copy_view = VK_NULL_HANDLE;
        vmaDestroyImage(sit_render.vk.vma_allocator, sit_render.vk.screen_copy_image, sit_render.vk.screen_copy_memory);
        sit_render.vk.screen_copy_image = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_memory = VK_NULL_HANDLE;
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate descriptor set for Screen Copy.");
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Frees the screen copy resources.
 *
 * @details Destroys the image, view, and descriptor set created by `_SituationVulkanCreateScreenCopyResource`.
 *          Called during swapchain cleanup.
 */
static void _SituationVulkanDestroyScreenCopyResource(void) {
    if (sit_render.vk.screen_copy_descriptor_set != VK_NULL_HANDLE) {
        // [FIX v2.3.27B] Explicitly free the set to prevent memory leaks during resize
        if (sit_render.vk.screen_copy_descriptor_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(
                sit_render.vk.device,
                sit_render.vk.screen_copy_descriptor_pool,
                1,
                &sit_render.vk.screen_copy_descriptor_set
            );
        }
        sit_render.vk.screen_copy_descriptor_set = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_descriptor_pool = VK_NULL_HANDLE;
    }

    if (sit_render.vk.screen_copy_view != VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.screen_copy_view, NULL);
        sit_render.vk.screen_copy_view = VK_NULL_HANDLE;
    }

    if (sit_render.vk.screen_copy_image != VK_NULL_HANDLE) {
        vmaDestroyImage(sit_render.vk.vma_allocator, sit_render.vk.screen_copy_image, sit_render.vk.screen_copy_memory);
        sit_render.vk.screen_copy_image = VK_NULL_HANDLE;
        sit_render.vk.screen_copy_memory = VK_NULL_HANDLE;
    }
}

/**
 * @brief [INTERNAL] Allocates and begins recording a temporary, primary-level Vulkan command buffer.
 *
 * @details This helper function is a standard and convenient way to execute short, one-off sequences of Vulkan commands (e.g., image layout transitions, buffer copies, setting image data). It simplifies the process by:
 * 1.  Allocating a single primary command buffer from the library's main command pool (`sit_render.vk.command_pool`).
 * 2.  Beginning recording on that buffer with the `VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT` flag, indicating it will be submitted once and then discarded.
 *
 * @par Typical Usage Pattern
 * The caller uses this function to get a command buffer, records commands into it using `vkCmd*` functions, and then calls
 * `_SituationVulkanEndSingleTimeCommands` to end recording, submit the commands to the graphics queue, wait for completion, and clean up the buffer.
 *
 * @code{.c}
 * VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
 * if (cmd == VK_NULL_HANDLE) { // Handle allocation/error } // Check added for robustness
 * // Record commands, e.g., vkCmdPipelineBarrier, vkCmdCopyBuffer...
 * _SituationVulkanEndSingleTimeCommands(cmd); // Handles submission, wait, and cleanup
 * @endcode
 *
 * @return A valid `VkCommandBuffer` handle ready for command recording.
 * @return `VK_NULL_HANDLE` if the library is not initialized, if the Vulkan device or command pool is invalid, or if allocation/beginning fails.
 *         A specific error message is set via `_SituationSetErrorFromCode`.
 *
 * @note This function is for internal library use and is not part of the public API.
 * @note It is the caller's sole responsibility to pass the returned `VkCommandBuffer` handle to `_SituationVulkanEndSingleTimeCommands` to ensure proper submission, synchronization, and cleanup.
 *       Failing to do so will result in resource leaks.
 * @warning This function allocates a command buffer. Not calling `_SituationVulkanEndSingleTimeCommands` will leak this resource.
 * @warning This function is synchronous; `_SituationVulkanEndSingleTimeCommands` calls `vkQueueWaitIdle`, blocking the CPU until the commands complete.
 *
 * @see _SituationVulkanEndSingleTimeCommands(), vkAllocateCommandBuffers(), vkBeginCommandBuffer()
 */
static VkCommandBuffer _SituationVulkanBeginSingleTimeCommands(void) {
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanBeginSingleTimeCommands called\n"); fflush(stdout);
    #endif
    // --- 1. Input/State Validation ---
    // NOTE: We do NOT check SituationIsInitialized() here because this function is called
    // DURING initialization (e.g., when creating quad renderer buffers). The is_initialized
    // flag is only set at the END of SituationInit(), so checking it here would cause
    // a false negative and prevent initialization from completing.
    // Instead, we only check if the device and command pool are valid, which is sufficient.
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Checking device/pool...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Device: %p, Pool: %p\n", (void*)sit_render.vk.device, (void*)sit_render.vk.command_pool); fflush(stdout);
    #endif
    if (sit_render.vk.device == VK_NULL_HANDLE || sit_render.vk.command_pool == VK_NULL_HANDLE) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: ERROR: Device or command pool is NULL!\n"); fflush(stdout);
        #endif
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanBeginSingleTimeCommands: Vulkan device or command pool is NULL.");
        return VK_NULL_HANDLE;
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Device/pool OK, allocating command buffer...\n"); fflush(stdout);
    #endif

    // --- 2. Allocate Command Buffer ---
    VkCommandBufferAllocateInfo alloc_info = {}; // Explicitly zero-initialize
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; // Mandatory sType
    alloc_info.pNext = NULL; // No extension structures
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Primary command buffer
    alloc_info.commandPool = sit_render.vk.command_pool; // Use the library's main command pool
    alloc_info.commandBufferCount = 1; // Allocate one command buffer

#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: About to call vkAllocateCommandBuffers...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   device=%p, pool=%p\n", (void*)sit_render.vk.device, (void*)sit_render.vk.command_pool); fflush(stdout);
    #endif

    VkCommandBuffer command_buffer = VK_NULL_HANDLE; // Initialize handle
    VkResult result = vkAllocateCommandBuffers(sit_render.vk.device, &alloc_info, &command_buffer);
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: vkAllocateCommandBuffers result: %d, buffer: %p\n", result, (void*)command_buffer); fflush(stdout);
    #endif
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanBeginSingleTimeCommands: vkAllocateCommandBuffers failed (VkResult: 0x%x).", result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        return VK_NULL_HANDLE; // Return invalid handle on allocation failure
    }

    // --- 3. Begin Recording Command Buffer ---
    VkCommandBufferBeginInfo begin_info = {}; // Explicitly zero-initialize
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; // Mandatory sType
    begin_info.pNext = NULL; // No extension structures
    // --- CRITICAL FLAG ---
    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT tells the driver this buffer
    // will be submitted once and then not used again. This can enable optimizations.
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    // --- END CRITICAL FLAG ---
    begin_info.pInheritanceInfo = NULL; // Not used for primary command buffers

    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanBeginSingleTimeCommands: vkBeginCommandBuffer failed (VkResult: 0x%x).", result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        // --- CRITICAL CLEANUP ---
        // If vkBeginCommandBuffer fails, we must free the allocated command buffer to prevent a resource leak.
        vkFreeCommandBuffers(sit_render.vk.device, sit_render.vk.command_pool, 1, &command_buffer);
        // --- END CRITICAL CLEANUP ---
        return VK_NULL_HANDLE; // Return invalid handle on begin failure
    }

    // --- 4. Success ---
    // If we reach here, the command buffer is valid and in the recording state.
    // It is the caller's responsibility to end and submit it using _SituationVulkanEndSingleTimeCommands.
    return command_buffer;
}


// --- Updated/Added Documentation Block for _SituationVulkanEndSingleTimeCommands ---
/**
 * @brief [INTERNAL] Ends recording, submits, waits for completion, and cleans up a one-time-use command buffer.
 *
 * @details This helper function completes the lifecycle of a temporary command buffer created by `_SituationVulkanBeginSingleTimeCommands`. It performs the following essential steps:
 * 1.  Ends the recording of the command buffer.
 * 2.  Submits the command buffer to the graphics queue (`sit_render.vk.graphics_queue`) for execution.
 * 3.  Waits for the graphics queue to become idle (`vkQueueWaitIdle`), ensuring that all commands recorded in the buffer have finished executing on the GPU.
 *     This makes the function synchronous.
 * 4.  Frees the command buffer back to the pool (`sit_render.vk.command_pool`) from which it was allocated.
 *
 * @par Typical Usage Pattern
 * This function is always paired with `_SituationVulkanBeginSingleTimeCommands`.
 *
 * @code{.c}
 * VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();
 * if (cmd == VK_NULL_HANDLE) { // Handle allocation/error }
 * // Record commands, e.g., vkCmdPipelineBarrier, vkCmdCopyBuffer...
 * _SituationVulkanEndSingleTimeCommands(cmd); // Handles submission, wait, and cleanup
 * @endcode
 *
 * @param command_buffer The `VkCommandBuffer` handle returned by `_SituationVulkanBeginSingleTimeCommands`. This handle must be valid.
 *
 * @note This function is for internal library use and is not part of the public API.
 * @note This function is synchronous due to the `vkQueueWaitIdle` call. It will block the calling thread until the GPU has finished executing the commands.
 * @note It is crucial that the `command_buffer` parameter is the handle returned by `_SituationVulkanBeginSingleTimeCommands` and has not been previously ended or submitted.
 * @warning Failing to call this function after obtaining a command buffer from `_SituationVulkanBeginSingleTimeCommands` will result in a resource leak.
 * @warning Calling this function with an invalid or already-ended `command_buffer` handle can lead to undefined behavior or validation errors.
 *
 * @see _SituationVulkanBeginSingleTimeCommands(), vkEndCommandBuffer(), vkQueueSubmit(), vkQueueWaitIdle(), vkFreeCommandBuffers()
 */
static SituationError _SituationVulkanEndSingleTimeCommands(VkCommandBuffer command_buffer) {
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanEndSingleTimeCommands called\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   command_buffer=%p\n", (void*)command_buffer); fflush(stdout);
    printf("Situation [Vulkan Debug]:   device=%p, queue=%p\n", (void*)sit_render.vk.device, (void*)sit_render.vk.graphics_queue); fflush(stdout);
    #endif
    // --- 1. Input Validation ---
    // While internal, checking for VK_NULL_HANDLE prevents potential crashes.
    if (command_buffer == VK_NULL_HANDLE) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanEndSingleTimeCommands: command_buffer is VK_NULL_HANDLE.");
    }
    // Note: We don't check sit_render.vk.device/queue/pool here as they should be valid if this function is called correctly after Begin. A check could be added if paranoia dictates.

    // --- 2. End Recording the Command Buffer ---
    VkResult result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanEndSingleTimeCommands: vkEndCommandBuffer failed (VkResult: 0x%x).", result);
        vkFreeCommandBuffers(sit_render.vk.device, sit_render.vk.command_pool, 1, &command_buffer);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
    }

    // --- 3. Submit the Command Buffer to the Graphics Queue ---
    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; // Explicitly zero-initialize
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; // Mandatory sType
    submit_info.pNext = NULL; // No extension structures
    submit_info.waitSemaphoreCount = 0; // No semaphores to wait on for one-time cmds
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1; // Submit one command buffer
    submit_info.pCommandBuffers = &command_buffer; // The buffer to submit
    submit_info.signalSemaphoreCount = 0; // No semaphores to signal upon completion
    submit_info.pSignalSemaphores = NULL;

    result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanEndSingleTimeCommands: vkQueueSubmit failed (VkResult: 0x%x).", result);
        vkFreeCommandBuffers(sit_render.vk.device, sit_render.vk.command_pool, 1, &command_buffer);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, error_detail);
    }

    // --- 4. Wait for the Submitted Commands to Finish ---
    // This is the synchronous part. It blocks the CPU thread until the GPU is completely done executing the commands in `command_buffer`.
    // This ensures resources used by those commands are no longer in use.
    result = vkQueueWaitIdle(sit_render.vk.graphics_queue);
    if (result != VK_SUCCESS) {
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "_SituationVulkanEndSingleTimeCommands: vkQueueWaitIdle failed (VkResult: 0x%x).", result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        // Note: Even if waiting fails (indicating a serious problem like device loss), we still attempt to free the command buffer. The device might be in a bad state, but cleanup is still the right intention.
    }
    // Note: No specific error handling for vkQueueWaitIdle failure beyond logging.
    // The device might be lost, but proceeding with freeing the buffer is still necessary.

    // --- 5. Free the Command Buffer ---
    // Regardless of whether the wait succeeded (in terms of device health), we must free the command buffer to prevent leaks.
    vkFreeCommandBuffers(sit_render.vk.device, sit_render.vk.command_pool, 1, &command_buffer);
    // After this call, `command_buffer` is an invalid handle and must not be used.

    return (result == VK_SUCCESS) ? SITUATION_SUCCESS : SITUATION_ERROR_VULKAN_COMMAND_FAILED;
}


/**
 * @brief [INTERNAL] Creates a Vulkan Shader Module from SPIR-V bytecode.
 *
 * @details This helper function encapsulates the process of creating a `VkShaderModule` object from a block of compiled SPIR-V code. A `VkShaderModule` is a container object that holds the compiled shader code and makes it available for use in pipeline shader stages.
 *
 * @param code A pointer to the raw SPIR-V bytecode data. This data must be valid and correctly compiled SPIR-V. The function does not validate the SPIR-V itself, only that the pointer is not NULL and `code_size` is non-zero.
 * @param code_size The size of the SPIR-V bytecode data in bytes. This must be a non-zero, positive value and should be a multiple of 4, as SPIR-V is a 32-bit word-based format.
 *
 * @return A valid `VkShaderModule` handle on success.
 * @return `VK_NULL_HANDLE` if the function fails. This can occur if:
 *         - The input `code` pointer is `NULL`.
 *         - The `code_size` is 0.
 *         - The call to `vkCreateShaderModule` fails (e.g., due to invalid SPIR-V, driver issues, or device loss). A specific error message is set in the library's global error state via `_SituationSetErrorFromCode`.
 *
 * @note This function requires that `sit_render.vk.device` is a valid and initialized `VkDevice` handle. This is guaranteed by the library's Vulkan initialization sequence if this function is called correctly.
 * @note The caller is responsible for destroying the returned `VkShaderModule` using `vkDestroyShaderModule` when it is no longer needed, typically after the pipeline using it has been created.
 * @warning The SPIR-V data pointed to by `code` is not validated by this function for correctness beyond basic size and pointer checks. Passing invalid SPIR-V can lead to errors during pipeline creation or runtime.
 *
 * @see _SituationVulkanCreateComputePipeline(), _SituationVulkanCreateGraphicsPipeline(), vkCreateShaderModule(), vkDestroyShaderModule()
 */

/**
 * @brief [INTERNAL] Allocates a descriptor set using a recycling pool strategy.
 *
 * @details [Optimized v2.3.27C] This function manages the "Dynamic Descriptor Manager".
 *          Unlike the previous version which only grew linearly, this version attempts to
 *          recycle space in existing pools before allocating new memory.
 *
 *          Strategy:
 *          1. Try to allocate from the 'current' pool (fast path).
 *          2. If full, iterate through ALL existing pools to find free space (recycling).
 *          3. If all pools are full/fragmented, create a new pool and add it to the list.
 *
 * @param layout The descriptor set layout to allocate.
 * @param[out] out_pool The pool that the set was allocated from (needed for freeing).
 * @return A valid VkDescriptorSet, or VK_NULL_HANDLE on critical failure.
 */
static VkDescriptorSet _SituationVulkanAllocateDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool* out_pool) {
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationVulkanAllocateDescriptorSet called\n"); fflush(stdout);
    #endif
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Layout handle: %p\n", (void*)layout); fflush(stdout);
    #endif
    if (layout == VK_NULL_HANDLE) {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   ERROR: Layout is NULL!\n"); fflush(stdout);
        #endif
        return VK_NULL_HANDLE;
    }
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkResult res = VK_ERROR_OUT_OF_POOL_MEMORY;
    VkDescriptorSet out_set = VK_NULL_HANDLE;

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Phase 1: Trying current pool...\n"); fflush(stdout);
    #endif
    // --- Phase 1: Try Current Active Pool (Fast Path) ---
    // We check the last used pool first to maintain cache locality and speed.
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:     Manager count: %d\n", sit_render.vk.descriptor_manager.count); fflush(stdout);
    #endif
    if (sit_render.vk.descriptor_manager.count > 0) {
        int idx = sit_render.vk.descriptor_manager.current_index;
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Current index: %d\n", idx); fflush(stdout);
        #endif
        alloc_info.descriptorPool = sit_render.vk.descriptor_manager.pools[idx];
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Pool handle: %p\n", (void*)alloc_info.descriptorPool); fflush(stdout);
        #endif
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Device handle: %p\n", (void*)sit_render.vk.device); fflush(stdout);
        #endif
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Calling vkAllocateDescriptorSets...\n"); fflush(stdout);
        #endif
        res = vkAllocateDescriptorSets(sit_render.vk.device, &alloc_info, &out_set);
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     vkAllocateDescriptorSets result: %d\n", res); fflush(stdout);
        #endif
        if (res == VK_SUCCESS) {
#ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]:     Allocation SUCCESS! Set handle: %p\n", (void*)out_set); fflush(stdout);
            #endif
            if (out_pool) *out_pool = sit_render.vk.descriptor_manager.pools[idx];
#ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]:     Returning from _SituationVulkanAllocateDescriptorSet\n"); fflush(stdout);
            #endif
            return out_set;
        }
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Phase 2: Searching existing pools...\n"); fflush(stdout);
    #endif
    // --- Phase 2: Search Existing Pools (Recycling Path) ---
    // If the current pool is full/fragmented, check if older pools have freed up space.
    // This prevents infinite growth during long-running sessions (Load/Unload cycles).
    for (int i = 0; i < sit_render.vk.descriptor_manager.count; ++i) {
        // Skip the one we just checked
        if (i == sit_render.vk.descriptor_manager.current_index) continue;

        alloc_info.descriptorPool = sit_render.vk.descriptor_manager.pools[i];
        res = vkAllocateDescriptorSets(sit_render.vk.device, &alloc_info, &out_set);

        if (res == VK_SUCCESS) {
            // Found a pool with space! Make it the new 'current' to speed up subsequent allocs.
            sit_render.vk.descriptor_manager.current_index = i;
            if (out_pool) *out_pool = sit_render.vk.descriptor_manager.pools[i];
            return out_set;
        }
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Phase 3: Creating new pool...\n"); fflush(stdout);
    #endif
    // --- Phase 3: Create New Pool (Growth Path) ---
    // If we reach here, all existing pools are full or fragmented. We must grow.

    // Define pool sizes (Balanced for typical engine usage)
    // Increased counts to reduce allocation frequency.
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 500 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 200 }
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        // CRITICAL: FREE_DESCRIPTOR_SET_BIT allows individual sets to be freed.
        // This is required for our recycling strategy to work.
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 4000, // Sum of poolSizes approx
        .poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes
    };

    VkDescriptorPool new_pool;
    if (vkCreateDescriptorPool(sit_render.vk.device, &pool_info, NULL, &new_pool) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_POOL_EXHAUSTED, "Critical: Failed to create new descriptor pool.");
        return VK_NULL_HANDLE;
    }

    // Add to manager list (Dynamic array logic)
    if (sit_render.vk.descriptor_manager.count >= sit_render.vk.descriptor_manager.capacity) {
        int new_cap = (sit_render.vk.descriptor_manager.capacity == 0) ? 4 : sit_render.vk.descriptor_manager.capacity * 2;
        void* new_pools = SIT_REALLOC(sit_render.vk.descriptor_manager.pools, new_cap * sizeof(VkDescriptorPool));
        if (!new_pools) {
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to resize descriptor pool list.");
            vkDestroyDescriptorPool(sit_render.vk.device, new_pool, NULL);
            return VK_NULL_HANDLE;
        }
        sit_render.vk.descriptor_manager.pools = (VkDescriptorPool*)new_pools;
        sit_render.vk.descriptor_manager.capacity = new_cap;
    }

    // Register new pool
    int new_index = sit_render.vk.descriptor_manager.count;
    sit_render.vk.descriptor_manager.pools[new_index] = new_pool;
    sit_render.vk.descriptor_manager.count++;
    sit_render.vk.descriptor_manager.current_index = new_index; // Set as active

    // Allocate from the fresh pool (Should always succeed)
    alloc_info.descriptorPool = new_pool;
    if (vkAllocateDescriptorSets(sit_render.vk.device, &alloc_info, &out_set) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Allocation failed on fresh pool (Driver Error?)");
        return VK_NULL_HANDLE;
    }

    if (out_pool) *out_pool = new_pool;
    return out_set;
}

// --- Main Vulkan Initializer ---

/**
 * @brief [INTERNAL] Orchestrates the complete initialization of the Vulkan rendering backend.
 * @details This is the master function for setting up the Vulkan environment. It is called once during `SituationInit` and executes a multi-phase sequence to create all necessary Vulkan objects, from the instance and device to the swapchain and internal renderers.
 *          This function is responsible for establishing the library's high-performance resource management models, including the persistent descriptor set infrastructure.
 *
 * @par Initialization Sequence
 *   The function proceeds through several distinct phases:
 *   1.  **Core API Setup:** Creates the `VkInstance`, validation layers (if enabled), `VkSurfaceKHR`, selects a suitable `VkPhysicalDevice`, and creates the `VkDevice`. It also initializes the Vulkan Memory Allocator (VMA).
 *   2.  **Framing Setup:** Determines the optimal number of in-flight frames based on swapchain capabilities and allocates the per-frame arrays for command buffers, semaphores, and fences.
 *   3.  **Frame-Independent Resources:** Creates resources that are not tied to a specific frame, including the swapchain, main render pass, and depth buffer.
 *   4.  **Descriptor Infrastructure:** Critically, it initializes the **Dynamic Descriptor Manager**. It creates an initial "seed" `VkDescriptorPool` (`persistent_descriptor_pool`) and registers it with the manager. This allows the engine to automatically grow its descriptor capacity at runtime if the initial pool becomes full, preventing crashes during heavy asset streaming.
 *   5.  **Per-Frame Resources:** Creates the per-frame command buffers, synchronization objects (semaphores/fences), and the UBOs used for global view/projection data.
 *   6.  **Internal Renderers:** Initializes the pipelines and vertex buffers required for the library's internal rendering helpers, such as the 2D quad renderer.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct, containing user-defined configuration like enabling validation layers and the window title.
 *
 * @return SITUATION_SUCCESS on successful initialization of all Vulkan components.
 * @return An appropriate `SituationError` code if any phase of the initialization fails. The function will halt on the first error and return immediately.
 *
 * @note This is a complex orchestrator function. Each sub-step (e.g., `_SituationVulkanCreateInstance`) is handled by a dedicated helper function for clarity and modularity.
 * @warning This function is for internal use by `SituationInit` only and must not be called directly.
 *          It assumes that `_SituationInitPlatform` and `_SituationInitWindow` have already been called successfully.
 */
static SituationError _SituationInitVulkan(const SituationInitInfo* init_info) {
    // --- Phase 1: Establish Core Vulkan API Handles ---
    if (_SituationVulkanCreateInstance(init_info) != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED; }
    if (_SituationVulkanSetupDebugMessenger(init_info) != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED; }
    if (_SituationVulkanCreateSurface() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_INIT_FAILED; }
    if (_SituationVulkanPickPhysicalDevice() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE; }

    if (init_info && init_info->force_single_queue) {
        sit_render.vk.compute_family_index = sit_render.vk.graphics_family_index;
    }

    if (_SituationVulkanCreateLogicalDevice(init_info) != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED; }

    // [Bindless] Verify Feature Support (Required for V2.4+)
    if (!(sit_render.enabled_features_mask & SIT_FEATURE_BINDLESS_TEXTURES)) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE, "Bindless Textures not supported by device.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE;
    }

    if (_SituationVulkanCreateAllocator() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED; }
    if (_SituationVulkanCreateCommandPool() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_COMMAND_FAILED; }

    // --- Phase 2: Determine Dynamic Frame Count & Allocate Per-Frame State ---
    uint32_t desired_frames = (init_info->max_frames_in_flight > 1) ? init_info->max_frames_in_flight : 2;
    _SituationVulkanSwapchainSupportDetails support_details;
    _SituationVulkanQuerySwapchainSupport(sit_render.vk.physical_device, &support_details);

    uint32_t image_count = support_details.capabilities.minImageCount + 1;
    if (support_details.capabilities.maxImageCount > 0 && image_count > support_details.capabilities.maxImageCount) {
        image_count = support_details.capabilities.maxImageCount;
    }
    _SituationVulkanFreeSwapchainSupportDetails(&support_details);

    /* max_frames_in_flight must be LESS than swapchain image count so that
     * vkAcquireNextImageKHR always has a free image even when all in-flight
     * slots are busy. This enables fence-wait-free present (submit + present
     * immediately; fence waited only on acquire when the slot is reused). */
    uint32_t max_fif = (desired_frames < image_count) ? desired_frames : image_count;
    if (max_fif >= image_count && image_count > 1) {
        max_fif = image_count - 1;
    }
    sit_render.vk.max_frames_in_flight = max_fif;
    if (sit_render.vk.max_frames_in_flight > (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT) {
        sit_render.vk.max_frames_in_flight = (uint32_t)SITUATION_MAX_FRAMES_IN_FLIGHT;
    }
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [Vulkan]: Using %u frames in flight.\n", sit_render.vk.max_frames_in_flight);
#endif

    uint32_t frame_count = sit_render.vk.max_frames_in_flight;
    // Use SIT_CALLOC to zero-initialize all handles to NULL
    sit_render.vk.command_buffers = (VkCommandBuffer*)SIT_CALLOC(frame_count, sizeof(VkCommandBuffer));
    sit_render.vk.compute_command_buffers = (VkCommandBuffer*)SIT_CALLOC(frame_count, sizeof(VkCommandBuffer));
    sit_render.vk.image_available_semaphores = (VkSemaphore*)SIT_CALLOC(frame_count, sizeof(VkSemaphore));
    sit_render.vk.render_finished_semaphores = (VkSemaphore*)SIT_CALLOC(frame_count, sizeof(VkSemaphore));
    sit_render.vk.compute_finished_semaphores = (VkSemaphore*)SIT_CALLOC(frame_count, sizeof(VkSemaphore));
    sit_render.vk.in_flight_fences = (VkFence*)SIT_CALLOC(frame_count, sizeof(VkFence));
    sit_render.vk.view_proj_ubo_buffer = (VkBuffer*)SIT_CALLOC(frame_count, sizeof(VkBuffer));
    sit_render.vk.view_proj_ubo_memory = (VmaAllocation*)SIT_CALLOC(frame_count, sizeof(VmaAllocation));
    sit_render.vk.view_proj_ubo_mapped = (void**)SIT_CALLOC(frame_count, sizeof(void*));
    sit_render.vk.view_proj_ubo_descriptor_set = (VkDescriptorSet*)SIT_CALLOC(frame_count, sizeof(VkDescriptorSet));
    sit_render.vk.graveyards = (_SituationVKGraveyard*)SIT_CALLOC(frame_count, sizeof(_SituationVKGraveyard));

    if (!sit_render.vk.command_buffers || !sit_render.vk.compute_command_buffers || !sit_render.vk.image_available_semaphores || !sit_render.vk.render_finished_semaphores || !sit_render.vk.compute_finished_semaphores || !sit_render.vk.in_flight_fences || !sit_render.vk.view_proj_ubo_buffer || !sit_render.vk.view_proj_ubo_memory || !sit_render.vk.view_proj_ubo_mapped || !sit_render.vk.view_proj_ubo_descriptor_set || !sit_render.vk.graveyards) {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Per-frame Vulkan resource arrays");
        _SituationCleanupVulkan(); // The main cleanup function will free any non-NULL arrays
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    for (uint32_t i = 0; i < frame_count; i++) {
        _SituationInitGraveyard(&sit_render.vk.graveyards[i]);
    }

    // --- Phase 3 & 4: Frame-Independent and Descriptor Infrastructure ---
    if (_SituationVulkanCreateSwapchain(VK_NULL_HANDLE) != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED; }
    if (_SituationVulkanCreateImageViews() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED; }
    if (_SituationVulkanCreateRenderPass() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED; }
    if (_SituationVulkanCreateDepthResources() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED; }
    if (_SituationVulkanCreateFramebuffers() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED; }

    // --- Descriptor Pool & Manager Setup ---
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SITUATION_VULKAN_UNIFORM_BUFFER_SIZE + frame_count },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SITUATION_VULKAN_STORAGE_BUFFER_SIZE },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SITUATION_VULKAN_COMBINED_IMAGE_SAMPLER_SIZE },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, SITUATION_VULKAN_DEFAULT_USER_STORAGE_IMAGES }
    };

    const uint32_t total_max_sets = SITUATION_VULKAN_UNIFORM_BUFFER_SIZE +
                                    SITUATION_VULKAN_STORAGE_BUFFER_SIZE +
                                    SITUATION_VULKAN_COMBINED_IMAGE_SAMPLER_SIZE +
                                    SITUATION_VULKAN_DEFAULT_USER_STORAGE_IMAGES +
                                    frame_count;

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        // [FIX v2.3.27B] Re-enable FREE_BIT to allow reclaiming memory for individual sets.
        // This is critical for preventing OOM during asset streaming.
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = total_max_sets,
        .poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]),
        .pPoolSizes = pool_sizes
    };

    // 1. Create the initial persistent pool
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating descriptor pool...\n"); fflush(stdout);
    #endif
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Calling vkCreateDescriptorPool...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorPool(sit_render.vk.device, &pool_info, NULL, &sit_render.vk.persistent_descriptor_pool) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create unified descriptor pool.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Descriptor pool created successfully\n"); fflush(stdout);
    #endif

    VkDescriptorSetLayoutBinding dynamic_ubo_binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo dynamic_ubo_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &dynamic_ubo_binding };
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating dynamic UBO layout...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &dynamic_ubo_layout_info, NULL, &sit_render.vk.dynamic_ubo_layout) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create dynamic UBO layout.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Set as the active pool for the dynamic manager to start with
    sit_render.vk.descriptor_pool = sit_render.vk.persistent_descriptor_pool;

    // 2. Seed the Dynamic Manager with this pool
    // This ensures subsequent allocations use this pool instead of creating a new one immediately.
    sit_render.vk.descriptor_manager.capacity = 4;
    sit_render.vk.descriptor_manager.pools = (VkDescriptorPool*)SIT_MALLOC(sizeof(VkDescriptorPool) * 4);
    if (!sit_render.vk.descriptor_manager.pools) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    sit_render.vk.descriptor_manager.pools[0] = sit_render.vk.persistent_descriptor_pool;
    sit_render.vk.descriptor_manager.count = 1;
    sit_render.vk.descriptor_manager.current_index = 0;

    // Create Descriptor Set Layouts... (Rest of function continues below)
    VkDescriptorSetLayoutBinding ubo_binding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo ubo_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &ubo_binding };
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating UBO layout...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &ubo_layout_info, NULL, &sit_render.vk.ubo_layout) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create UBO layout.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    VkDescriptorSetLayoutBinding ssbo_binding = { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo ssbo_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &ssbo_binding };
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating SSBO layout...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &ssbo_layout_info, NULL, &sit_render.vk.ssbo_layout) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create SSBO layout.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Create layouts for internal renderers
    VkDescriptorSetLayoutBinding ubo_layout_binding_internal = { SIT_UBO_BINDING_VIEW_DATA, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo ubo_layout_info_internal = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &ubo_layout_binding_internal };
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating UBO layout...\n"); fflush(stdout);
    #endif
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &ubo_layout_info_internal, NULL, &sit_render.vk.view_data_ubo_layout) != VK_SUCCESS) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // --- 1. Restore Standard Sampler Layout (For VDs and Compute) ---
    // Uses Binding 4 (SIT_SAMPLER_BINDING_VD_SOURCE)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating standard sampler layout...\n"); fflush(stdout);
    #endif
    VkDescriptorSetLayoutBinding standard_sampler_binding = { SIT_SAMPLER_BINDING_VD_SOURCE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo standard_sampler_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &standard_sampler_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &standard_sampler_info, NULL, &sit_render.vk.image_sampler_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    /* Advanced VD composite FS: layout(set=2, binding=5) u_destinationTexture — must match screen_copy_descriptor_set */
    VkDescriptorSetLayoutBinding composite_dest_binding = { SIT_SAMPLER_BINDING_VD_DEST, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo composite_dest_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &composite_dest_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &composite_dest_info, NULL, &sit_render.vk.composite_dest_sampler_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    {
        VkDescriptorSetLayoutBinding pat_bindings[2] = {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
            { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL }
        };
        VkDescriptorSetLayoutCreateInfo pat_ubo_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 2, pat_bindings };
        if (vkCreateDescriptorSetLayout(sit_render.vk.device, &pat_ubo_layout_info, NULL, &sit_render.vk.vd_pattern_config_ubo_layout) != VK_SUCCESS) {
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
        }

        VkBufferCreateInfo pat_buf_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        pat_buf_info.size = (VkDeviceSize)SIT_VD_STANDBY_HEADER_UBO_SIZE;
        pat_buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VmaAllocationCreateInfo pat_alloc_info = {0};
        pat_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        if (vmaCreateBuffer(sit_render.vk.vma_allocator, &pat_buf_info, &pat_alloc_info,
                &sit_render.vk.vd_pattern_config_ubo, &sit_render.vk.vd_pattern_config_ubo_memory, NULL) != VK_SUCCESS) {
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
        }

        VkBufferCreateInfo pat_ssbo_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        pat_ssbo_info.size = (VkDeviceSize)SIT_VD_STANDBY_PARAMS_SSBO_SIZE;
        pat_ssbo_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (vmaCreateBuffer(sit_render.vk.vma_allocator, &pat_ssbo_info, &pat_alloc_info,
                &sit_render.vk.vd_pattern_config_ssbo, &sit_render.vk.vd_pattern_config_ssbo_memory, NULL) != VK_SUCCESS) {
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
        }

        VkDescriptorPoolSize pat_pool_sizes[2] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
        };
        VkDescriptorPoolCreateInfo pat_pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        pat_pool_info.maxSets = 1;
        pat_pool_info.poolSizeCount = 2;
        pat_pool_info.pPoolSizes = pat_pool_sizes;
        if (vkCreateDescriptorPool(sit_render.vk.device, &pat_pool_info, NULL, &sit_render.vk.vd_pattern_config_descriptor_pool) != VK_SUCCESS) {
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
        }

        sit_render.vk.vd_pattern_config_descriptor_set = _SituationVulkanAllocateDescriptorSet(
            sit_render.vk.vd_pattern_config_ubo_layout, &sit_render.vk.vd_pattern_config_descriptor_pool);
        if (sit_render.vk.vd_pattern_config_descriptor_set == VK_NULL_HANDLE) {
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
        }

        VkDescriptorBufferInfo pat_ubo_info = { sit_render.vk.vd_pattern_config_ubo, 0, (VkDeviceSize)SIT_VD_STANDBY_HEADER_UBO_SIZE };
        VkDescriptorBufferInfo pat_ssbo_info_res = { sit_render.vk.vd_pattern_config_ssbo, 0, (VkDeviceSize)SIT_VD_STANDBY_PARAMS_SSBO_SIZE };
        VkWriteDescriptorSet pat_writes[2] = {
            { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, sit_render.vk.vd_pattern_config_descriptor_set, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &pat_ubo_info, NULL },
            { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, sit_render.vk.vd_pattern_config_descriptor_set, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, NULL, &pat_ssbo_info_res, NULL }
        };
        vkUpdateDescriptorSets(sit_render.vk.device, 2, pat_writes, 0, NULL);
    }

    if (_SituationVulkanCreateScreenCopyResource() != SITUATION_SUCCESS) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }

    // --- 2. Create Bindless Layout (For Global Texture Array) ---
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating bindless sampler layout...\n"); fflush(stdout);
    #endif

    // [Bindless] Setup Global Descriptor Layout
    VkDescriptorSetLayoutBinding bindless_binding = {};
    bindless_binding.binding = 0; // Use binding 0 for the array (GLSL: binding = 0)
    bindless_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindless_binding.descriptorCount = SITUATION_MAX_TEXTURES;
    bindless_binding.stageFlags = VK_SHADER_STAGE_ALL;
    bindless_binding.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    flagsInfo.bindingCount = 1;
    flagsInfo.pBindingFlags = &flags;

    VkDescriptorSetLayoutCreateInfo bindless_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    bindless_layout_info.pNext = &flagsInfo;
    bindless_layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    bindless_layout_info.bindingCount = 1;
    bindless_layout_info.pBindings = &bindless_binding;

    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &bindless_layout_info, NULL, &sit_render.vk.bindless_descriptor_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // [Bindless] Create Dedicated Pool for the Global Set
    VkDescriptorPoolSize bindless_pool_size = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SITUATION_MAX_TEXTURES };
    VkDescriptorPoolCreateInfo bindless_pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    bindless_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    bindless_pool_info.maxSets = 1;
    bindless_pool_info.poolSizeCount = 1;
    bindless_pool_info.pPoolSizes = &bindless_pool_size;

    if (vkCreateDescriptorPool(sit_render.vk.device, &bindless_pool_info, NULL, &sit_render.vk.global_bindless_pool) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to create bindless descriptor pool.");
        _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // [Bindless] Allocate Global Set
    uint32_t bindless_descriptor_count = SITUATION_MAX_TEXTURES;
    VkDescriptorSetVariableDescriptorCountAllocateInfo bindless_var_count_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        NULL,
        1,
        &bindless_descriptor_count
    };
    VkDescriptorSetAllocateInfo bindless_alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    bindless_alloc_info.pNext = &bindless_var_count_info;
    bindless_alloc_info.descriptorPool = sit_render.vk.global_bindless_pool;
    bindless_alloc_info.descriptorSetCount = 1;
    bindless_alloc_info.pSetLayouts = &sit_render.vk.bindless_descriptor_layout;

    if (vkAllocateDescriptorSets(sit_render.vk.device, &bindless_alloc_info, &sit_render.vk.global_bindless_set) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED, "Failed to allocate global bindless descriptor set.");
        _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Create text sampler layout (binding 0 for ALBEDO texture)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating text sampler layout (binding 0)...\n"); fflush(stdout);
    #endif
    VkDescriptorSetLayoutBinding text_sampler_binding = { SIT_SAMPLER_BINDING_ALBEDO, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo text_sampler_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &text_sampler_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &text_sampler_info, NULL, &sit_render.vk.text_sampler_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Create compute sampler layout (binding 0 for compute shaders)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating compute sampler layout (binding 0, compute stage)...\n"); fflush(stdout);
    #endif
    VkDescriptorSetLayoutBinding compute_sampler_binding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo compute_sampler_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &compute_sampler_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &compute_sampler_info, NULL, &sit_render.vk.compute_sampler_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    // Bindings for storage images usually happen in Compute or Fragment stages
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating storage image layout...\n"); fflush(stdout);
    #endif
    VkDescriptorSetLayoutBinding storage_img_binding = { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL };
    VkDescriptorSetLayoutCreateInfo storage_img_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &storage_img_binding };
    if (vkCreateDescriptorSetLayout(sit_render.vk.device, &storage_img_info, NULL, &sit_render.vk.storage_image_layout) != VK_SUCCESS) {
         _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating dynamic vertex buffers...\n"); fflush(stdout);
    #endif
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Device handle = %p\n", (void*)sit_render.vk.device); fflush(stdout);
    #endif
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: VMA allocator = %p\n", (void*)sit_render.vk.vma_allocator); fflush(stdout);
    #endif
    // --- Dynamic Vertex Buffer Initialization ---
    // Allocate 512KB per frame for dynamic text/UI geometry.
    // usage = VERTEX_BUFFER, memory = CPU_TO_GPU (Host Visible, Coherent)
    sit_render.vk.dynamic_vbo_capacity = 524288;
    VkBufferCreateInfo dyn_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    dyn_info.size = sit_render.vk.dynamic_vbo_capacity;
    dyn_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo dyn_alloc_info = {0};
    dyn_alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // Direct write
    dyn_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // Map immediately

    for (uint32_t i = 0; i < frame_count; i++) {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Creating dynamic VBO %u/%u...\n", i+1, frame_count); fflush(stdout);
        #endif
        VkResult vbo_result = vmaCreateBuffer(sit_render.vk.vma_allocator, &dyn_info, &dyn_alloc_info,
            &sit_render.vk.dynamic_vbo[i],
            &sit_render.vk.dynamic_vbo_alloc[i],
            NULL);
        if (vbo_result != VK_SUCCESS) {
            #ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]: vmaCreateBuffer failed with result: 0x%x\n", vbo_result); fflush(stdout);
            #endif
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
        }
        // Get the mapped pointer
        VmaAllocationInfo alloc_result;
        vmaGetAllocationInfo(sit_render.vk.vma_allocator, sit_render.vk.dynamic_vbo_alloc[i], &alloc_result);
        sit_render.vk.dynamic_vbo_mapped[i] = alloc_result.pMappedData;
    }

    // --- Phase 5 & 6: Per-Frame Objects and Internal Renderers ---
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating command buffers...\n"); fflush(stdout);
    #endif
    if (_SituationVulkanCreateCommandBuffers() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_COMMAND_FAILED; }
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating sync objects...\n"); fflush(stdout);
    #endif
    if (_SituationVulkanCreateSyncObjects() != SITUATION_SUCCESS) { _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED; }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating per-frame UBOs...\n"); fflush(stdout);
    #endif
    for (uint32_t i = 0; i < frame_count; i++) {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   UBO %u/%u...\n", i+1, frame_count); fflush(stdout);
        #endif
        VkDeviceSize buffer_size = sizeof(ViewDataUBO);

        // Create UBO with Persistent Mapping (CPU to GPU)
        VkBufferCreateInfo buf_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buf_info.size = sizeof(ViewDataUBO);
        buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo alloc_info = {0};
        alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT; // Keeps it mapped forever

        VmaAllocationInfo alloc_result;
        if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buf_info, &alloc_info,
            &sit_render.vk.view_proj_ubo_buffer[i],
            &sit_render.vk.view_proj_ubo_memory[i],
            &alloc_result) != VK_SUCCESS) {
            _SituationCleanupVulkan(); return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
        }

        // Save the mapped pointer
        sit_render.vk.view_proj_ubo_mapped[i] = alloc_result.pMappedData;

        // [FIX v2.3.27B] Updated to pass NULL for pool tracking (View UBOs persist until shutdown)
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:     Allocating descriptor set...\n"); fflush(stdout);
        #endif
        sit_render.vk.view_proj_ubo_descriptor_set[i] = _SituationVulkanAllocateDescriptorSet(sit_render.vk.view_data_ubo_layout, NULL);

        if (sit_render.vk.view_proj_ubo_descriptor_set[i] == VK_NULL_HANDLE) {
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED;
        }

        VkDescriptorBufferInfo buffer_info = { sit_render.vk.view_proj_ubo_buffer[i], 0, buffer_size };
        VkWriteDescriptorSet write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = sit_render.vk.view_proj_ubo_descriptor_set[i];
            write.dstBinding = SIT_UBO_BINDING_VIEW_DATA;  // Binding 1 - must match layout
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &buffer_info;
        vkUpdateDescriptorSets(sit_render.vk.device, 1, &write, 0, NULL);
    }

#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Initializing compute layouts...\n"); fflush(stdout);
    #endif
    if (_SituationVulkanInitComputeLayouts() != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create pre-defined compute pipeline layouts.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }
    if (_SituationVulkanInitGraphicsSpirvLayouts() != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create graphics SPIR-V pipeline layouts.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

#if defined(SITUATION_ENABLE_SHADER_COMPILER)
    // 1. Initialize the Quad Renderer (Shared Function)
    // Note: Width/Height are ignored by Vulkan path, passing 0 is safe.
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Initializing quad renderer...\n"); fflush(stdout);
    #endif
    {
        SituationError quad_err = _SituationInitQuadRenderer(0, 0);
        if (quad_err != SITUATION_SUCCESS) {
        _SituationCleanupVulkan();
        return quad_err;
        }
        SituationError ypq_err = _SituationInitYpqGradeRenderer(0, 0);
        if (ypq_err != SITUATION_SUCCESS) {
            _SituationCleanupVulkan();
            return ypq_err;
        }
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: About to call _SituationInitDefaultFont...\n"); fflush(stdout);
    #endif
    {
        SituationError font_err = _SituationInitDefaultFont();
        if (font_err != SITUATION_SUCCESS) {
        _SituationCleanupVulkan();
        return font_err;
        }
    }
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: _SituationInitDefaultFont succeeded!\n"); fflush(stdout);
    #endif

    // 2. Initialize Virtual Display Renderers (includes text pipeline)
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Initializing internal renderers (text pipeline + VD)...\n"); fflush(stdout);
    #endif
    if (_SituationVulkanInitInternalRenderers() != SITUATION_SUCCESS) {
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Internal renderers initialized successfully\n"); fflush(stdout);
    #endif
#else
    printf("Situation [Vulkan]: Shader compiler disabled. Internal renderers (Quad, VD) are unavailable.\n");
    // Zero out handles to be safe
    sit_render.vk.quad_pipeline = VK_NULL_HANDLE;
    for (int _vd_blend = 0; _vd_blend < 5; ++_vd_blend) {
        sit_render.vk.vd_compositing_blend_pipelines[_vd_blend] = VK_NULL_HANDLE;
    }
    sit_render.vk.vd_compositing_pipeline = VK_NULL_HANDLE;
    sit_render.vk.advanced_compositing_pipeline = VK_NULL_HANDLE;
#endif

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Initializing staging buffers...\n"); fflush(stdout);
    #endif

    // [v2.4] Configure Staging Buffer Size
    if (init_info && init_info->staging_buffer_size > 0) {
        sit_render.vk.staging_buffer_size = init_info->staging_buffer_size;
    } else {
        sit_render.vk.staging_buffer_size = SITUATION_VK_STAGING_BUFFER_SIZE;
    }

    SituationError staging_result = _SituationInitStagingBuffers();
    if (staging_result != SITUATION_SUCCESS) {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Staging buffers initialization FAILED with error code: %d\n", staging_result); fflush(stdout);
        #endif
        _SituationCleanupVulkan();
        return staging_result;
    }
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Staging buffers initialized successfully!\n"); fflush(stdout);
    #endif

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Setting renderer type and marking as initialized...\n"); fflush(stdout);
    #endif
    sit_render.renderer_type = SIT_RENDERER_VULKAN;

    sit_render.vk.screenshot_staging_buffer = VK_NULL_HANDLE;
    sit_render.vk.screenshot_staging_allocation = VK_NULL_HANDLE;
    sit_render.vk.screenshot_buffer = NULL;
    sit_render.vk.screenshot_width = 0;
    sit_render.vk.screenshot_height = 0;
    sit_render.vk.screenshot_valid = false;
    sit_render.vk.screenshot_resolved_frame_index = UINT32_MAX;
    for (int _si = 0; _si < SITUATION_MAX_FRAMES_IN_FLIGHT; _si++) {
        sit_render.vk.screenshot_copy_pending[_si] = false;
    }
    sit_render.vk.screenshot_mutex_initialized = false;
    if (mtx_init(&sit_render.vk.screenshot_mutex, mtx_plain) != thrd_success) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INIT_FAILED, "_SituationInitVulkan: screenshot mutex init failed.");
        _SituationCleanupVulkan();
        return SITUATION_ERROR_VULKAN_INIT_FAILED;
    }
    sit_render.vk.screenshot_mutex_initialized = true;
    sit_render.active_occlusion_pool_slot = -1;

#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
#if SIT_VK_SHADER_CACHE_PHASE2
    {
        VkPipelineCacheCreateInfo pci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        };
        if (vkCreatePipelineCache(sit_render.vk.device, &pci, NULL, &sit_render.vk.pipeline_cache) != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CACHE_INIT_FAILED,
                "vkCreatePipelineCache failed during Vulkan init.");
            _SituationCleanupVulkan();
            return SITUATION_ERROR_VULKAN_PIPELINE_CACHE_INIT_FAILED;
        }
    }
#endif
    _SitVkShaderCacheInit(&sit_render.vk.shader_cache);
#endif

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Vulkan initialization COMPLETE!\n"); fflush(stdout);
    #endif

    {
        SituationError gpu_prof_err = _SitGpuProfInit();
        if (gpu_prof_err != SITUATION_SUCCESS && gpu_prof_err != SITUATION_ERROR_PROFILING_GPU_UNSUPPORTED) {
            _SituationCleanupVulkan();
            return gpu_prof_err;
        }
    }

    return SITUATION_SUCCESS;
}


// --- Helper Implementations ---

/**
 * @brief [INTERNAL] Builds a list of required Vulkan instance extensions.
 *
 * @details This helper function consolidates the list of Vulkan instance extensions required for the application to function correctly. This includes:
 * - Extensions mandated by GLFW for window surface creation (e.g., `VK_KHR_surface`).
 * - The `VK_EXT_debug_utils` extension if runtime validation is enabled.
 * - Platform-specific extensions required for compatibility (e.g., `VK_KHR_portability_enumeration` on macOS with MoltenVK).
 *
 * @param out_extension_count A pointer to a `uint32_t` where the number of extensions in the returned list will be stored.
 *                            This pointer must not be NULL.
 * @param enable_validation   A boolean flag indicating whether Vulkan validation layers are enabled. If true, the debug utils extension will be included in the list.
 *
 * @return A pointer to a statically allocated array of `const char*` strings, each representing a required Vulkan instance extension name.
 *         The array's length is given by the value written to `out_extension_count`.
 *         The returned pointer is valid only until the next call to this function.
 * @return NULL If GLFW reports no required instance extensions, or if `out_extension_count` is NULL.
 *
 * @note This function uses a statically allocated internal buffer to hold the
 *       list of extension names. The maximum number of extensions it can handle is defined by `SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS` (currently 16).
 *       If the total required extensions exceed this limit, an error message is logged, and the function's behavior is undefined (likely truncation or crash).
 *       This limit is considered sufficient for standard use cases.
 *
 * @see _SituationVulkanCreateInstance()
 */
static const char** _SituationVulkanGetRequiredExtensions(uint32_t* out_extension_count, bool enable_validation) {
    // --- 1. Input Validation ---
    if (!out_extension_count) {
        // Cannot output the count, so the result would be unusable.
        // This is a logic error in the caller.
        // fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: out_extension_count is NULL.\n");
        // Using _SituationSetErrorFromCode might be overkill for an internal helper,
        // but could be considered if the library does this for internal helpers.
        return NULL;
    }
    *out_extension_count = 0; // Initialize output count to zero in case of early return.

    // --- 2. Get GLFW Required Extensions ---
    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    // If GLFW reports no extensions are needed, return an empty list.
    if (glfw_extensions == NULL || glfw_extension_count == 0) {
        // This is unusual but not necessarily an error depending on the platform/context.
        // fprintf(stderr, "WARNING: GLFW reported no required Vulkan instance extensions.\n");
        return NULL; // *out_extension_count is already 0.
    }

    // --- 3. Aggregate Extensions into Static Array ---
    // Define a reasonable limit for the number of extensions.
    // This should cover GLFW extensions + debug utils + platform specifics.
    static const char* extensions[SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS];
    uint32_t count = 0;

    // --- 4. Add GLFW Extensions ---
    for (uint32_t i = 0; i < glfw_extension_count; ++i) {
        if (count >= SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
            // This should not happen with standard setups, but protect against overflow.
            fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d). Truncating list.\n", SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Exceeded maximum Vulkan instance extension limit (GLFW extensions)");
            break; // Stop adding extensions to prevent buffer overrun.
        }
        extensions[count++] = glfw_extensions[i];
    }

    // --- 5. Add Validation Extension (if enabled) ---
    if (enable_validation) {
        if (count >= SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
            fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d) when adding VK_EXT_DEBUG_UTILS_EXTENSION_NAME.\n", SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Exceeded maximum Vulkan instance extension limit (debug utils)");
            // Cannot add it, list is full.
        } else {
            extensions[count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
    }

    // --- 6. Add Platform-Specific Extensions ---
#if defined(__APPLE__)
    {
        // --- macOS / MoltenVK Specific Extensions ---
        // VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME is required on macOS
        // to allow enumerating portability-compliant devices.
        if (count >= SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
            fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d) when adding VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME.\n", SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Exceeded maximum Vulkan instance extension limit (portability enumeration)");
        } else {
            extensions[count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        }

        // VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME is core in Vulkan 1.1
        // but explicitly enabling it can be good for portability layers.
        // Uncomment the lines below if this extension is deemed necessary.
        /*
        if (count >= SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
            fprintf(stderr, "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d) when adding VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME.\n", SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
        } else {
            extensions[count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
        }
        */
    }
#endif // __APPLE__

    /* Phase 6: extended surface color spaces (HDR10 ST2084, etc.). */
    if (count < SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
        extensions[count++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
    } else {
        fprintf(stderr,
            "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d) when adding VK_KHR_get_surface_capabilities2.\n",
            SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED,
            "Exceeded maximum Vulkan instance extension limit (get_surface_capabilities2)");
    }

    if (count < SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
        extensions[count++] = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;
    } else {
        fprintf(stderr,
            "ERROR: _SituationVulkanGetRequiredExtensions: Exceeded maximum extension limit (%d) when adding VK_EXT_swapchain_colorspace.\n",
            SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED,
            "Exceeded maximum Vulkan instance extension limit (swapchain colorspace)");
    }

    // --- 7. Finalize and Return ---
    // Ensure the final count doesn't exceed the logical limit, though checks above should prevent it.
    if (count > SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS) {
        count = SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS; // Defensive truncation
    }

    *out_extension_count = count;
    return extensions;
#undef SITUATION_VULKAN_MAX_INSTANCE_EXTENSIONS // Undefine local macro
}

/**
 * @brief [INTERNAL] Creates the core Vulkan instance.
 *
 * @details This helper function is responsible for initializing the Vulkan runtime environment by creating the `VkInstance`. This is the first major step in the Vulkan initialization process. It involves:
 * - Specifying the application and engine information.
 * - Enumerating and requesting the necessary instance extensions (provided by GLFW and potentially for debugging/validation).
 * - Optionally enabling the standard Khronos validation layer for development/debugging.
 * - Creating the `VkInstance` handle itself.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct provided during library initialization. This contains settings like the window title and whether Vulkan validation should be enabled.
 *                  This pointer must not be NULL.
 *
 * @return SITUATION_SUCCESS on successful creation of the Vulkan instance.
 * @return SITUATION_ERROR_INVALID_PARAM if `init_info` is NULL.
 * @return SITUATION_ERROR_VULKAN_UNSUPPORTED if Vulkan validation is requested but the required `VK_LAYER_KHRONOS_validation` layer is not found on the system, or if GLFW cannot provide the necessary instance extensions.
 * @return SITUATION_ERROR_VULKAN_INSTANCE_FAILED if `vkCreateInstance` fails for any reason (e.g., driver issues, unsupported API version, missing extensions).
 *
 * @note This function relies on `_SituationVulkanGetRequiredExtensions` to determine the list of necessary instance extensions.
 * @note The created `VkInstance` handle is stored in `sit_render.vk.instance`.
 *
 * @see _SituationInitVulkan(), _SituationVulkanGetRequiredExtensions()
 */
static SituationError _SituationVulkanCreateInstance(const SituationInitInfo* init_info) {
    // --- 1. Input Validation ---
    if (!init_info) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateInstance: init_info is NULL.");
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Handle Vulkan Validation Layers ---
    const char* validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
    const uint32_t validation_layer_count = 1; // Number of layers in the array above

    if (init_info->enable_vulkan_validation) {
        uint32_t layer_count = 0;
        // Query the number of available instance layer properties.
        VkResult enumerate_result = vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        if (enumerate_result != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Failed to enumerate Vulkan instance layer properties.");
            return SITUATION_ERROR_VULKAN_UNSUPPORTED;
        }

        // If no layers are available at all, validation cannot be enabled.
        if (layer_count == 0) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "No Vulkan validation layers found on the system.");
             return SITUATION_ERROR_VULKAN_UNSUPPORTED;
        }

        // Allocate memory to hold the list of available layers.
        VkLayerProperties* available_layers = (VkLayerProperties*)SIT_MALLOC(sizeof(VkLayerProperties) * layer_count);
        if (!available_layers) {
             _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for Vulkan instance layer properties.");
             return SITUATION_ERROR_MEMORY_ALLOCATION;
        }

        // Query the actual layer properties.
        enumerate_result = vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
        if (enumerate_result != VK_SUCCESS) {
            SIT_FREE(available_layers);
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Failed to enumerate Vulkan instance layer properties (second query).");
            return SITUATION_ERROR_VULKAN_UNSUPPORTED;
        }

        // Check if the required validation layer is present in the list.
        bool layer_found = false;
        for (uint32_t i = 0; i < layer_count; i++) {
            // Compare the name of the current available layer with the one we need.
            if (strcmp(validation_layers[0], available_layers[i].layerName) == 0) {
                layer_found = true;
                break; // Found it, no need to check further
            }
        }

        // Clean up the allocated list of layer properties.
        SIT_FREE(available_layers);

        // If the required validation layer was not found, report an error.
        if (!layer_found) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Validation layer 'VK_LAYER_KHRONOS_validation' requested but not available!");
            return SITUATION_ERROR_VULKAN_UNSUPPORTED;
        }
        // If layer_found is true, we can proceed with enabling the layer.
    }
    // If validation is not enabled, no layer checks are needed.

    // --- 3. Specify Application and Engine Information ---
    VkApplicationInfo app_info = {}; // Explicitly zero-initialize
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = init_info->window_title; // Use the title from init info
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // App version 1.0.0
    app_info.pEngineName = "Situation Engine"; // Identify the engine
    app_info.engineVersion = VK_MAKE_VERSION(SITUATION_VERSION_MAJOR, SITUATION_VERSION_MINOR, SITUATION_VERSION_PATCH);
    // Specify the target Vulkan API version. Ensure consistency with VMA and device requirements.
    app_info.apiVersion = VK_API_VERSION_1_4; // Target Vulkan 1.4

    // --- 4. Specify Instance Creation Parameters ---
    VkInstanceCreateInfo create_info = {}; // Explicitly zero-initialize
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info; // Link the application info

    // --- 5. Get Required Instance Extensions ---
    uint32_t extension_count = 0;
    // Use the helper function to get the list of required extensions,
    // including platform-specific ones and the debug extension if validation is enabled.
    const char** required_extensions = _SituationVulkanGetRequiredExtensions(&extension_count, init_info->enable_vulkan_validation);
    if (required_extensions == NULL) {
        // The helper function should have set an error message if it failed critically.
        // If it returns NULL with extension_count=0, it might be okay (no extensions needed),
        // but GLFW needing none is unusual. Treat as an error condition.
        if (extension_count == 0) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "GLFW reported no required Vulkan instance extensions, or failed to query them.");
        } // Else, error set by _SituationVulkanGetRequiredExtensions
        // If _SituationVulkanGetRequiredExtensions sets its own error, we could just return its code.
        // Assuming it sets SITUATION_ERROR_VULKAN_UNSUPPORTED on its critical failures.
        return SITUATION_ERROR_VULKAN_UNSUPPORTED;
    }
    // Set the extensions in the create info structure.
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = required_extensions;

    // --- 6. Configure Enabled Layers (if Validation is On) ---
    if (init_info->enable_vulkan_validation) {
        // Enable the validation layer(s) by setting the count and pointer to the array.
        create_info.enabledLayerCount = validation_layer_count;
        create_info.ppEnabledLayerNames = validation_layers;
    } else {
        // No layers are enabled.
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = NULL; // Explicitly set to NULL for clarity
    }

    // --- 7. Platform-Specific Instance Creation Flags ---
#if defined(__APPLE__)
    {
        // On macOS (when using MoltenVK), the VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
        // flag is required to allow enumeration of portability-compliant devices.
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif // __APPLE__

    // --- 8. Create the Vulkan Instance ---
    // This is the actual API call that creates the VkInstance handle.
    VkResult create_result = vkCreateInstance(&create_info, NULL, &sit_render.vk.instance);
    if (create_result != VK_SUCCESS) {
        // vkCreateInstance failed. This could be due to various reasons:
        // - Unsupported API version (app_info.apiVersion)
        // - Missing or unsupported extensions
        // - Missing or unsupported layers (if enabled)
        // - Driver issues
        // - Problems with pApplicationInfo
        char error_detail[256];
        snprintf(error_detail, sizeof(error_detail),
                 "vkCreateInstance failed with VkResult 0x%x. Possible causes: unsupported API version, missing extensions/layers, driver issues.", create_result);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED, error_detail);
        return SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED;
    }

    // --- 9. Success ---
    // If we reach here, the VkInstance was created successfully.
    // The handle is stored in sit_render.vk.instance for use by subsequent Vulkan functions.
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Callback function for Vulkan Validation Layer messages.
 *
 * @details This function is registered with the Vulkan instance (via `VkDebugUtilsMessengerCreateInfoEXT`) to receive debug, warning, and error messages from the Vulkan validation layers and the driver.
 *          It serves as the primary mechanism for diagnosing issues during Vulkan application development.
 *          The function receives detailed information about each message, including its severity (verbose, info, warning, error), type (general, validation, performance), and a descriptive text message. Based on the severity, it formats and prints the message to `stderr` for immediate visibility.
 *
 * @param messageSeverity The severity level of the message (e.g., `VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT`).
 * @param messageType The type of the message (e.g., `VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT`).
 * @param pCallbackData A pointer to a `VkDebugUtilsMessengerCallbackDataEXT` struct containing the detailed message information, including `pMessage`.
 * @param pUserData User-defined data pointer passed during messenger creation.
 *                  This implementation does not use it and sets it to NULL.
 *
 * @return VK_FALSE. This indicates that the Vulkan call that triggered the callback should *not* be aborted. The application should handle
 *         errors programmatically based on VkResult codes. Returning VK_TRUE would force the call to return `VK_ERROR_VALIDATION_FAILED_EXT`.
 *
 * @note This function is only used if Vulkan validation is enabled (`init_info->enable_vulkan_validation` is true) and the necessary extensions (`VK_EXT_debug_utils`) are supported and loaded.
 * @warning This function is called asynchronously from internal Vulkan threads.
 *          Therefore, it must be thread-safe. Using `fprintf` to `stderr` is generally acceptable for this purpose.
 *
 * @see _SituationVulkanSetupDebugMessenger(), VkDebugUtilsMessengerCallbackDataEXT
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL _SituationVulkanDebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    // --- 1. Input Validation (Defensive for callback) ---
    // While Vulkan should provide valid data, checking is good practice.
    if (!pCallbackData || !pCallbackData->pMessage) {
        // Received invalid callback data. This is unusual but possible.
        // Log a basic message to indicate the problem.
        fprintf(stderr, "[Vulkan Debug Callback] ERROR: Received invalid callback data (NULL pCallbackData or pMessage).\n");
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR, "Vulkan debug callback received NULL data");
        return VK_FALSE; // Still return VK_FALSE
    }

    // --- 2. Silence Verbose Messages (Optional) ---
    // The callback is set up to receive VERBOSE, WARNING, and ERROR messages.
    // VERBOSE messages can be very noisy. Uncomment the lines below to filter them out.
    /*
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        // Optionally, filter out verbose messages based on type or content.
        // For example, silence specific verbose performance messages:
        // if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        //     return VK_FALSE;
        // }
        // Or just return early for all verbose messages:
        return VK_FALSE;
    }
    */

    // --- 3. Format and Print Message ---
    // Determine a prefix for the message based on its severity for easier scanning.
    const char* severity_prefix = "INFO"; // Default, though VERBOSE/INFO might be filtered above
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity_prefix = "ERROR";
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR, pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity_prefix = "WARN";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severity_prefix = "VERBOSE";
    }
    // Note: INFO level is VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT

    // Determine a prefix for the message based on its type.
    const char* type_prefix = "";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type_prefix = "[Validation] ";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type_prefix = "[Performance] ";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type_prefix = "[General] ";
    }

    // Print the formatted message to stderr.
    // Include severity and type for quick identification.
    fprintf(
        stderr,
        "[Vulkan %s] %s%s\n",
        severity_prefix,
        type_prefix, // Includes brackets and space if applicable
        pCallbackData->pMessage
    );
    // fflush(stderr); // Optional: Force immediate output, useful if stderr is buffered.

    // --- 4. Return Value ---
    // Always return VK_FALSE to indicate that the Vulkan call should continue.
    // The application should check VkResult codes for actual errors.
    return VK_FALSE;
}

// --- Updated/Added Documentation Block for _SituationVulkanSetupDebugMessenger ---
/**
 * @brief [INTERNAL] Sets up the Vulkan Debug Utils Messenger for validation layer output.
 *
 * @details This helper function is responsible for creating and registering the `VkDebugUtilsMessengerEXT` object if Vulkan validation is enabled.
 *          This messenger routes messages from the validation layers to the `_SituationVulkanDebugCallback` function, providing essential feedback for debugging Vulkan applications.
 *
 * The process involves:
 * 1.  Checking if validation is enabled in `init_info`.
 * 2.  Preparing a `VkDebugUtilsMessengerCreateInfoEXT` struct with the desired message severity levels, message types, and the callback function pointer.
 * 3.  Dynamically loading the `vkCreateDebugUtilsMessengerEXT` function pointer using `vkGetInstanceProcAddr`, as it's an extension function.
 * 4.  Calling the loaded function to create the messenger object.
 * 5.  Storing the created messenger handle in `sit_render.vk.debug_messenger` for later destruction.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct provided during `SituationInit`. This is used to check if validation is enabled.
 *                  This pointer must not be NULL.
 *
 * @return SITUATION_SUCCESS if validation is disabled, or if the messenger is successfully created.
 * @return SITUATION_ERROR_INVALID_PARAM if `init_info` is NULL.
 * @return SITUATION_ERROR_VULKAN_INSTANCE_FAILED if the required `vkCreateDebugUtilsMessengerEXT` function pointer cannot be loaded, or if the call to create the messenger fails. A specific error message is set.
 *
 * @note This function must be called after the Vulkan instance (`sit_render.vk.instance`) has been successfully created.
 * @note The created messenger (`sit_render.vk.debug_messenger`) is destroyed by `_SituationCleanupVulkan`.
 * @warning This function should only be called when using the Vulkan backend (`SITUATION_USE_VULKAN` is defined).
 *
 * @see _SituationVulkanDebugCallback(), _SituationInitVulkan(), _SituationVulkanCreateInstance(), _SituationCleanupVulkan(), vkCreateDebugUtilsMessengerEXT()
 */
static SituationError _SituationVulkanSetupDebugMessenger(const SituationInitInfo* init_info) {
    // --- 1. Input Validation ---
    if (!init_info) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanSetupDebugMessenger: init_info cannot be NULL." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Check if Validation is Enabled ---
    // If the user has not requested Vulkan validation, there's nothing to set up.
    // This is a normal and common path.
    if (!init_info->enable_vulkan_validation) {
        // Ensure the debug messenger handle is clean/invalid if not used.
        sit_render.vk.debug_messenger = VK_NULL_HANDLE;
        return SITUATION_SUCCESS;
    }

    // --- 3. Configure Debug Messenger Creation Info ---
    // This struct defines what kinds of messages we want to receive and how to handle them.
    VkDebugUtilsMessengerCreateInfoEXT create_info = {}; // Explicitly zero-initialize
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; // Mandatory sType
    create_info.pNext = NULL; // No extension structures
    create_info.flags = 0; // No special flags for messenger creation

    // Specify the message severity levels we are interested in receiving.
    // VERBOSE can be very noisy, but is useful for detailed analysis.
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | // Include INFO messages
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    // Specify the message types we are interested in receiving.
    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | // General events
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | // Violation of valid usage
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // Potential performance issues

    // Set the callback function that will be invoked when a message is received.
    create_info.pfnUserCallback = _SituationVulkanDebugCallback;

    // pUserData allows passing custom data to the callback. We don't need it.
    create_info.pUserData = NULL;

    // --- 4. Load the Extension Function Pointer ---
    // vkCreateDebugUtilsMessengerEXT is part of the VK_EXT_debug_utils extension,
    // so it's not automatically loaded with the standard Vulkan loader.
    // We must retrieve its function pointer manually using vkGetInstanceProcAddr.
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT_func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(sit_render.vk.instance, "vkCreateDebugUtilsMessengerEXT");

    // Check if the function pointer was successfully loaded.
    if (vkCreateDebugUtilsMessengerEXT_func == NULL) {
        // The function pointer could not be loaded. This usually means the
        // VK_EXT_debug_utils extension is not available or not properly loaded.
        _SituationSetErrorFromCode( SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED, "_SituationVulkanSetupDebugMessenger: Failed to load vkCreateDebugUtilsMessengerEXT function pointer. Check if VK_EXT_debug_utils is supported." );
        // Ensure the handle is explicitly invalid.
        sit_render.vk.debug_messenger = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED;
    }

    // --- 5. Create the Debug Messenger ---
    // Call the loaded function to create the VkDebugUtilsMessengerEXT object.
    VkResult result = vkCreateDebugUtilsMessengerEXT_func(
        sit_render.vk.instance, // The Vulkan instance
        &create_info,       // Creation parameters
        NULL,               // Optional allocation callbacks (use default)
        &sit_render.vk.debug_messenger // Output: the created messenger handle
    );

    // --- 6. Handle Creation Result ---
    if (result != VK_SUCCESS) {
        // vkCreateDebugUtilsMessengerEXT failed. This is unexpected but possible.
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "_SituationVulkanSetupDebugMessenger: vkCreateDebugUtilsMessengerEXT failed (VkResult: 0x%x).",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED, error_detail);
        // Ensure the global handle is explicitly invalid on failure.
        sit_render.vk.debug_messenger = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED;
    }

    // --- 7. Success ---
    // If we reach here, the VkDebugUtilsMessengerEXT was created successfully.
    // The handle is stored in sit_render.vk.debug_messenger.
    // It will receive messages from the validation layers until it is destroyed by _SituationCleanupVulkan (which should use vkDestroyDebugUtilsMessengerEXT).
    // The next step in Vulkan initialization is typically picking a physical device.
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Helper to create a VkImage and allocate memory via VMA.
 *
 * @details Wraps the complex setup of `VkImageCreateInfo` and `VmaAllocationCreateInfo`.
 *          It ensures images are created with `VK_SHARING_MODE_EXCLUSIVE` and the specified usage flags.
 *
 * @param width Width of the image in pixels.
 * @param height Height of the image in pixels.
 * @param mipLevels Total number of mip levels (1 for base level only).
 * @param format The Vulkan format (e.g., `VK_FORMAT_R8G8B8A8_SRGB`).
 * @param tiling Usually `VK_IMAGE_TILING_OPTIMAL`.
 * @param usage Bitmask of usage flags (Sampled, Storage, Transfer Dst, etc.).
 * @param memory_usage VMA hint (e.g., `VMA_MEMORY_USAGE_GPU_ONLY`).
 * @param[out] out_image Pointer to store the resulting VkImage handle.
 * @param[out] out_allocation Pointer to store the resulting VMA allocation handle.
 *
 * @return `SITUATION_SUCCESS` on success, or `SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED`.
 */
static SituationError _SituationVulkanCreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VmaMemoryUsage memory_usage, VkSampleCountFlagBits samples, VkImage* out_image, VmaAllocation* out_allocation) {
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mipLevels; // Use parameter
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
        fprintf(stderr, "[Vulkan] Creating texture with usage flags: 0x%x (storage=%d)\n",
                usage, (usage & VK_IMAGE_USAGE_STORAGE_BIT) ? 1 : 0);
#endif
    image_info.samples = (samples != 0) ? samples : VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = memory_usage;

    if (vmaCreateImage(sit_render.vk.vma_allocator, &image_info, &alloc_info, out_image, out_allocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create/allocate image.");
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates a Vulkan surface for the GLFW window.
 *
 * @details This helper function is a crucial step in the Vulkan initialization process.
 *          It instructs GLFW to create a `VkSurfaceKHR` object that represents the abstract surface of the `sit_gs.sit_glfw_window` within the Vulkan instance.
 *          This surface is essential for presenting rendered images to the screen, as it is used later to create the swapchain.
 *
 * @return SITUATION_SUCCESS on successful creation of the Vulkan surface.
 * @return SITUATION_ERROR_INVALID_PARAM if required prerequisites are not met:
 *         - `sit_render.vk.instance` is `VK_NULL_HANDLE`.
 *         - `sit_gs.sit_glfw_window` is `NULL`.
 * @return SITUATION_ERROR_VULKAN_INIT_FAILED if `glfwCreateWindowSurface` fails to create the surface. This can happen due to incompatibilities between the Vulkan instance and the GLFW window, or platform-specific issues. A specific error message is set.
 *
 * @note This function must be called after the Vulkan instance (`sit_render.vk.instance`) and the GLFW window (`sit_gs.sit_glfw_window`) have been successfully created.
 * @note The created `VkSurfaceKHR` handle is stored in `sit_render.vk.surface`.
 * @note This function relies on the `VK_KHR_surface` extension being enabled (which is typically done automatically by GLFW when `glfwCreateWindowSurface` is called) and the appropriate platform-specific surface extension (e.g., `VK_KHR_win32_surface`, `VK_KHR_xcb_surface`).
 * @warning This function should only be called when using the Vulkan backend (`SITUATION_USE_VULKAN` is defined).
 *
 * @see _SituationInitVulkan(), _SituationVulkanCreateInstance(), _SituationInitWindow(), glfwCreateWindowSurface(), vkDestroySurfaceKHR()
 */
static SituationError _SituationVulkanCreateSurface(void) {
    // --- 1. Input Validation (Defensive for internal helper) ---
    // Check if the prerequisite Vulkan instance and GLFW window handles are valid.
    // While the library's init sequence should guarantee this, checking adds robustness.
    if (sit_render.vk.instance == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateSurface: Vulkan instance is NULL. Call _SituationVulkanCreateInstance first." );
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (sit_gs.sit_glfw_window == NULL) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateSurface: GLFW window is NULL. Call _SituationInitWindow first." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Create the Vulkan Surface using GLFW ---
    // This is the core API call that bridges GLFW and Vulkan.
    // It creates a VkSurfaceKHR object associated with the GLFW window.
    // The VkAllocationCallbacks parameter is NULL, using default allocation.
    VkResult result = glfwCreateWindowSurface(
        sit_render.vk.instance,         // The Vulkan instance
        sit_gs.sit_glfw_window,     // The GLFW window
        NULL,                       // Optional allocation callbacks
        &sit_render.vk.surface          // Output: the created VkSurfaceKHR handle
    );

    // --- 3. Handle Result ---
    if (result != VK_SUCCESS) {
        // glfwCreateWindowSurface failed. This is a critical error for Vulkan setup.
        // Common reasons include:
        // - Incompatibility between the Vulkan instance extensions and GLFW.
        // - The GLFW window was created with GLFW_NO_API (correct for Vulkan)
        //   but there's still an issue.
        // - Platform-specific problems (e.g., missing/wrong display server libraries).
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "_SituationVulkanCreateSurface failed: glfwCreateWindowSurface returned VkResult 0x%x. Check Vulkan/Window compatibility or platform setup.",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_INIT_FAILED, error_detail);
        // Ensure the global surface handle is explicitly invalid on failure.
        sit_render.vk.surface = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_INIT_FAILED;
    }

    // --- 4. Success ---
    // If we reach here, the VkSurfaceKHR was created successfully by GLFW.
    // The handle is stored in sit_render.vk.surface and will be used subsequently
    // for swapchain creation and eventually presentation.
    // The next step in Vulkan init is typically picking a physical device
    // that supports this surface.
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Enumerates available GPUs and selects the most suitable one for the application.
 * @details This function is a critical step in the Vulkan initialization process. It queries the system for all Vulkan-capable physical devices and evaluates each one against a set of criteria to find the best fit.
 *          The selection is performed using a scoring system implemented in the `_SituationIsDeviceSuitable` helper function.
 *
 * @par Selection Logic
 *   1.  Enumerates all `VkPhysicalDevice`s present on the system.
 *   2.  For each device, it calls `_SituationIsDeviceSuitable` which performs pass/fail checks for essential features:
 *       - Support for a graphics queue family.
 *       - Support for a presentation queue family compatible with the window surface.
 *       - Availability of the `VK_KHR_swapchain` device extension.
 *       - Adequate swapchain support (at least one format and present mode).
 *   3.  Devices that pass the essential checks are then scored based on desirable properties, with a strong preference given to discrete GPUs over integrated ones.
 *   4.  The device with the highest score is selected as the primary GPU for the application.
 *
 * Upon successful selection, this function stores the chosen `VkPhysicalDevice` handle in `sit_render.vk.physical_device` and caches its graphics and present queue family indices for later use in logical device creation.
 *
 * @return SITUATION_SUCCESS if a suitable physical device is found and selected.
 * @return SITUATION_ERROR_VULKAN_DEVICE_FAILED if no Vulkan-capable GPUs are found, or if none of the found GPUs meet the minimum suitability requirements.
 *
 * @note This function must be called after the `VkInstance` and `VkSurfaceKHR` have been successfully created.
 * @warning This function is for internal use by `_SituationInitVulkan` only and should not be called directly.
 *
 * @see _SituationInitVulkan(), _SituationIsDeviceSuitable(), _SituationVulkanFindQueueFamilies()
 */
static SituationError _SituationVulkanPickPhysicalDevice(void) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(sit_render.vk.instance, &device_count, NULL);
    if (device_count == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE, "Failed to find GPUs with Vulkan support.");
        return SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE;
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)SIT_MALLOC(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(sit_render.vk.instance, &device_count, devices);

    int max_score = 0;
    VkPhysicalDevice best_device = VK_NULL_HANDLE;

    // Iterate over all devices and find the one with the highest score
    for (uint32_t i = 0; i < device_count; i++) {
        int score = _SituationIsDeviceSuitable(devices[i]);
        if (score > max_score) {
            max_score = score;
            best_device = devices[i];
        }
    }

    SIT_FREE(devices);

    if (best_device == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE, "Failed to find any suitable GPU.");
        return SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE;
    }

    // Store the best device and its queue family indices
    sit_render.vk.physical_device = best_device;
    _SituationQueueFamilyIndices indices = _SituationVulkanFindQueueFamilies(best_device, sit_render.vk.surface);
    sit_render.vk.graphics_family_index = indices.graphics_family;
    sit_render.vk.present_family_index = indices.present_family;
    sit_render.vk.compute_family_index = indices.compute_family_has_value ? indices.compute_family : indices.graphics_family;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(best_device, &properties);
    sit_render.vk.physical_device_api_version = properties.apiVersion;
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    printf("Situation [Vulkan]: Picked device '%s' with score %d\n", properties.deviceName, max_score);
#endif

    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates the Vulkan logical device and retrieves queue handles.
 * @details This function creates the `VkDevice` (the logical device), which is the primary interface for interacting with the selected physical device. It specifies which features, extensions, and queue families the application will use.
 *
 * @par Creation Process
 *   1.  **Queue Configuration:** It prepares one or two `VkDeviceQueueCreateInfo` structs based on the graphics and present queue family indices found by `_SituationVulkanPickPhysicalDevice`. If the indices are the same, only one queue is requested; otherwise, two are requested.
 *   2.  **Feature Enablement:** It specifies the set of `VkPhysicalDeviceFeatures` to enable. Currently, this is an empty set, but it can be expanded to request features like geometry shaders or anisotropic filtering.
 *   3.  **Extension Enablement:** It builds a list of required device-level extensions. This always includes `VK_KHR_swapchain` for rendering to a window and may include platform-specific extensions like `"VK_KHR_portability_subset"` on macOS.
 *   4.  **Device Creation:** It calls `vkCreateDevice` with the configured queues, features, and extensions to create the logical device handle.
 *   5.  **Queue Handle Retrieval:** After the device is created, it calls `vkGetDeviceQueue` to retrieve the handles for the graphics and present queues, storing them in the global state for command submission and presentation.
 *
 * @param init_info A pointer to the `SituationInitInfo` struct, used to determine if validation layers should be enabled at the device level.
 *
 * @return SITUATION_SUCCESS on successful creation of the logical device and retrieval of queue handles.
 * @return SITUATION_ERROR_VULKAN_DEVICE_FAILED if `vkCreateDevice` fails. This can happen if requested features or extensions are not supported by the physical device.
 *
 * @note This function must be called after a `VkPhysicalDevice` has been successfully selected by `_SituationVulkanPickPhysicalDevice`.
 * @warning This function is for internal use by `_SituationInitVulkan` only and should not be called directly.
 *
 * @see _SituationInitVulkan(), _SituationVulkanPickPhysicalDevice(), vkCreateDevice(), vkGetDeviceQueue()
 */
static SituationError _SituationVulkanCreateLogicalDevice(const SituationInitInfo* init_info) {
    // --- Queue Create Info ---
    // [v2.3.23] Updated to support up to 3 distinct queues (Graphics, Present, Compute)
    VkDeviceQueueCreateInfo queue_create_infos[3] = {};
    float queue_priority = 1.0f;
    uint32_t unique_queue_families[3];
    uint32_t unique_queue_family_count = 0;

    // Helper to add unique family
    // Always add Graphics first
    unique_queue_families[unique_queue_family_count++] = sit_render.vk.graphics_family_index;

    // Add Present if distinct
    bool present_unique = true;
    for(uint32_t i=0; i<unique_queue_family_count; i++) if(unique_queue_families[i] == sit_render.vk.present_family_index) present_unique = false;
    if(present_unique) unique_queue_families[unique_queue_family_count++] = sit_render.vk.present_family_index;

    // Add Compute if distinct
    bool compute_unique = true;
    for(uint32_t i=0; i<unique_queue_family_count; i++) if(unique_queue_families[i] == sit_render.vk.compute_family_index) compute_unique = false;
    if(compute_unique) unique_queue_families[unique_queue_family_count++] = sit_render.vk.compute_family_index;

    for (uint32_t i = 0; i < unique_queue_family_count; i++) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = unique_queue_families[i];
        queue_create_infos[i].queueCount = 1;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
    }

    // --- Device Features (Good as is) ---
    VkPhysicalDeviceFeatures device_features = {}; // Enable features as needed later

    // --- Device Extensions ---
    // Use a manageable array to build the list of required extensions.
    const char* device_extensions[16]; // Optional feature extensions can stack up quickly
    uint32_t extension_count = 0;

    // The swapchain extension is always required for rendering to a window.
    device_extensions[extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    // For macOS compatibility via MoltenVK, the portability subset extension is required.
    #if defined(__APPLE__)
        device_extensions[extension_count++] = "VK_KHR_portability_subset";
    #endif

    // Check for optional extensions
    uint32_t available_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(sit_render.vk.physical_device, NULL, &available_ext_count, NULL);
    VkExtensionProperties* available_exts = (VkExtensionProperties*)SIT_MALLOC(sizeof(VkExtensionProperties) * available_ext_count);
    vkEnumerateDeviceExtensionProperties(sit_render.vk.physical_device, NULL, &available_ext_count, available_exts);

    bool mesh_shader_supported = false;
    bool ray_tracing_supported = false;
    bool ext_dynamic_state_supported = false;
    bool ext_dynamic_state2_supported = false;
    bool ext_dynamic_state3_supported = false;

    for (uint32_t i = 0; i < available_ext_count; i++) {
        if (strcmp(available_exts[i].extensionName, "VK_EXT_mesh_shader") == 0) {
            mesh_shader_supported = true;
        }
        if (strcmp(available_exts[i].extensionName, "VK_KHR_ray_tracing_pipeline") == 0) {
            ray_tracing_supported = true;
        }
        if (strcmp(available_exts[i].extensionName, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) == 0) {
            ext_dynamic_state_supported = true;
        }
        if (strcmp(available_exts[i].extensionName, VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME) == 0) {
            ext_dynamic_state2_supported = true;
        }
        if (strcmp(available_exts[i].extensionName, VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME) == 0) {
            ext_dynamic_state3_supported = true;
        }
    }
    SIT_FREE(available_exts);

    // Define feature structures for extensions
    VkPhysicalDeviceMeshShaderFeaturesEXT mesh_features = {};
    mesh_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_features = {};
    ray_tracing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_features = {};
    accel_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    if (mesh_shader_supported) {
        device_extensions[extension_count++] = "VK_EXT_mesh_shader";
        sit_render.enabled_features_mask |= SIT_FEATURE_MESH_SHADER;
        mesh_features.meshShader = VK_TRUE;
    }

    if (ray_tracing_supported) {
        device_extensions[extension_count++] = "VK_KHR_ray_tracing_pipeline";
        device_extensions[extension_count++] = "VK_KHR_acceleration_structure"; // Prerequisite
        device_extensions[extension_count++] = "VK_KHR_deferred_host_operations"; // Prerequisite
        sit_render.enabled_features_mask |= SIT_FEATURE_RAY_TRACING;

        ray_tracing_features.rayTracingPipeline = VK_TRUE;
        accel_features.accelerationStructure = VK_TRUE;
    }

    if (ext_dynamic_state_supported) {
        device_extensions[extension_count++] = VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME;
    }
    if (ext_dynamic_state2_supported) {
        device_extensions[extension_count++] = VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME;
    }
    if (ext_dynamic_state3_supported) {
        device_extensions[extension_count++] = VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME;
    }

    // --- Feature Query & Enablement ---
    // We need to query what is supported before blindly enabling it.
    // This uses the modern VkPhysicalDeviceFeatures2 structure chain.

    // 1. Prepare the structures to query support
    VkPhysicalDeviceVulkan12Features supported_vk12 = {};
    supported_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceVulkan13Features supported_vk13 = {};
    supported_vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT supported_ext_dyn = {};
    supported_ext_dyn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;

    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT supported_ext_dyn2 = {};
    supported_ext_dyn2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;

    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT supported_ext_dyn3 = {};
    supported_ext_dyn3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;

    VkPhysicalDeviceFeatures2 supported_features2 = {};
    supported_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    supported_features2.pNext = &supported_vk12;
    supported_vk12.pNext = &supported_vk13;
    supported_vk13.pNext = &supported_ext_dyn;
    supported_ext_dyn.pNext = &supported_ext_dyn2;
    supported_ext_dyn2.pNext = &supported_ext_dyn3;

    vkGetPhysicalDeviceFeatures2(sit_render.vk.physical_device, &supported_features2);

    // 2. Prepare the structures for creation (enable what we found)
    VkPhysicalDeviceVulkan12Features enable_vk12 = {};
    enable_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceVulkan13Features enable_vk13 = {};
    enable_vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT enable_ext_dyn = {};
    enable_ext_dyn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;

    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT enable_ext_dyn2 = {};
    enable_ext_dyn2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT;

    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT enable_ext_dyn3 = {};
    enable_ext_dyn3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;

    // Chain extension features if enabled
    void** next_ptr = &enable_vk12.pNext;
    if (supported_vk13.dynamicRendering) {
        enable_vk13.dynamicRendering = VK_TRUE;
        sit_render.vk.dynamic_rendering_enabled = true;
        *next_ptr = &enable_vk13;
        next_ptr = &enable_vk13.pNext;
    }
    if (mesh_shader_supported) {
        *next_ptr = &mesh_features;
        next_ptr = &mesh_features.pNext;
    }
    if (ray_tracing_supported) {
        *next_ptr = &ray_tracing_features;
        next_ptr = &ray_tracing_features.pNext;
        *next_ptr = &accel_features;
        next_ptr = &accel_features.pNext;
    }
    if (supported_ext_dyn.extendedDynamicState) {
        enable_ext_dyn.extendedDynamicState = VK_TRUE;
        sit_render.vk.extended_dynamic_state_enabled = true;
        *next_ptr = &enable_ext_dyn;
        next_ptr = &enable_ext_dyn.pNext;
    }
    if (supported_ext_dyn2.extendedDynamicState2) {
        enable_ext_dyn2.extendedDynamicState2 = VK_TRUE;
        sit_render.vk.depth_bias_dynamic_enabled = true;
        *next_ptr = &enable_ext_dyn2;
        next_ptr = &enable_ext_dyn2.pNext;
    }
    /* Wireframe uses static vk_pipeline_*_line variants (POLYGON_MODE_LINE baked in).
     * Do not enable extendedDynamicState3PolygonMode: VK_DYNAMIC_STATE_POLYGON_MODE_EXT
     * in pipelines overrides static line mode and requires vkCmdSetPolygonModeEXT every draw. */
    (void)supported_ext_dyn3.extendedDynamicState3PolygonMode;
    bool enable_ext_dyn3_chain = false;
    if (supported_ext_dyn3.extendedDynamicState3ColorWriteMask) {
        enable_ext_dyn3.extendedDynamicState3ColorWriteMask = VK_TRUE;
        sit_render.vk.extended_dynamic_state3_color_write_enabled = true;
        enable_ext_dyn3_chain = true;
    }
    if (enable_ext_dyn3_chain) {
        *next_ptr = &enable_ext_dyn3;
        next_ptr = &enable_ext_dyn3.pNext;
    }

    // Enable Buffer Device Address (Critical for Bindless)
    if (supported_vk12.bufferDeviceAddress) {
        enable_vk12.bufferDeviceAddress = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_BINDLESS_BUFFERS;
    } else {
        printf("Situation [Vulkan]: Warning - bufferDeviceAddress not supported. Bindless features disabled.\n");
    }

    // Enable Descriptor Indexing (Critical for Bindless Textures)
    if (supported_vk12.descriptorIndexing) {
        enable_vk12.descriptorIndexing = VK_TRUE;
        // We also need specific sub-features for full bindless texture support
        if (supported_vk12.shaderSampledImageArrayNonUniformIndexing &&
            supported_vk12.runtimeDescriptorArray &&
            supported_vk12.descriptorBindingPartiallyBound &&
            supported_vk12.descriptorBindingVariableDescriptorCount) {

             enable_vk12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
             enable_vk12.runtimeDescriptorArray = VK_TRUE;
             enable_vk12.descriptorBindingPartiallyBound = VK_TRUE;
             enable_vk12.descriptorBindingVariableDescriptorCount = VK_TRUE;
             if (supported_vk12.descriptorBindingSampledImageUpdateAfterBind) {
                 enable_vk12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
             }

             sit_render.enabled_features_mask |= SIT_FEATURE_BINDLESS_TEXTURES;
        }
    }

    // Enable Float16 (Half-float)
    if (supported_vk12.shaderFloat16) {
        enable_vk12.shaderFloat16 = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_FLOAT16;
    }

    // Enable Draw Indirect Count
    if (supported_vk12.drawIndirectCount) {
        enable_vk12.drawIndirectCount = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_DRAW_INDIRECT_COUNT;
    }

    // Enable Standard Features (Compute, Geometry, etc.)
    // Vulkan 1.0 features are in supported_features2.features
    if (supported_features2.features.geometryShader) {
        device_features.geometryShader = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_GEOMETRY_SHADER;
    }
    if (supported_features2.features.tessellationShader) {
        device_features.tessellationShader = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_TESSELLATION_SHADER;
    }
    if (supported_features2.features.wideLines) {
        device_features.wideLines = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_WIDE_LINES;
    }
    if (supported_features2.features.fillModeNonSolid) {
        device_features.fillModeNonSolid = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_FILL_MODE_NON_SOLID;
    }
    if (supported_features2.features.samplerAnisotropy) {
        device_features.samplerAnisotropy = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_SAMPLER_ANISOTROPY;
    }
    if (supported_features2.features.shaderInt64) {
        device_features.shaderInt64 = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_INT64;
    }
    if (supported_features2.features.shaderFloat64) {
        device_features.shaderFloat64 = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_FLOAT64;
    }
    if (supported_features2.features.multiViewport) {
        device_features.multiViewport = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_MULTI_VIEWPORT;
    }
    if (supported_features2.features.multiDrawIndirect) {
        device_features.multiDrawIndirect = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_MULTI_DRAW_INDIRECT;
    }
    if (supported_features2.features.textureCompressionBC) {
        device_features.textureCompressionBC = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_TEXTURE_COMPRESSION_BC;
    }
    if (supported_features2.features.textureCompressionASTC_LDR) {
        device_features.textureCompressionASTC_LDR = VK_TRUE;
        sit_render.enabled_features_mask |= SIT_FEATURE_TEXTURE_COMPRESSION_ASTC;
    }

    // Compute is mandatory in Vulkan, but good to track
    sit_render.enabled_features_mask |= SIT_FEATURE_COMPUTE_SHADER;

    // Subgroup Operations (Core in 1.1)
    // We can assume basic subgroup support if we are on Vulkan 1.2, but let's check properties if we were being pedantic.
    // For now, enable the flag as it's standard.
    sit_render.enabled_features_mask |= SIT_FEATURE_SUBGROUP_OPERATIONS;


    // --- Device Create Info ---
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &enable_vk12; // Chain the 1.2 features
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.queueCreateInfoCount = unique_queue_family_count;
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = extension_count; // Use the dynamic count
    create_info.ppEnabledExtensionNames = device_extensions; // Use the new array

    // Validation layers (Good as is)
    const char* validation_layers[] = { "VK_LAYER_KHRONOS_validation" };
    if (init_info->enable_vulkan_validation) {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = validation_layers;
    } else {
        create_info.enabledLayerCount = 0;
    }

    // --- Create the Device ---
    if (vkCreateDevice(sit_render.vk.physical_device, &create_info, NULL, &sit_render.vk.device) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED, "Failed to create logical device");
        return SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED;
    }

    // --- Get Queue Handles ---
    vkGetDeviceQueue(sit_render.vk.device, sit_render.vk.graphics_family_index, 0, &sit_render.vk.graphics_queue);
    vkGetDeviceQueue(sit_render.vk.device, sit_render.vk.present_family_index, 0, &sit_render.vk.present_queue);
    vkGetDeviceQueue(sit_render.vk.device, sit_render.vk.compute_family_index, 0, &sit_render.vk.compute_queue);

    if (sit_render.vk.extended_dynamic_state3_polygon_mode_enabled) {
        sit_render.vk.pfn_cmd_set_polygon_mode_ext = (PFN_vkCmdSetPolygonModeEXT)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetPolygonModeEXT");
        if (!sit_render.vk.pfn_cmd_set_polygon_mode_ext) {
            sit_render.vk.pfn_cmd_set_polygon_mode_ext = (PFN_vkCmdSetPolygonModeEXT)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetPolygonModeEXT");
        }
        if (!sit_render.vk.pfn_cmd_set_polygon_mode_ext) {
            sit_render.vk.extended_dynamic_state3_polygon_mode_enabled = false;
            fprintf(stderr, "Situation [Vulkan]: vkCmdSetPolygonModeEXT unavailable; wireframe uses static line pipelines.\n");
            fflush(stderr);
        }
    }
    if (sit_render.vk.extended_dynamic_state_enabled) {
        sit_render.vk.pfn_cmd_set_primitive_topology = (PFN_vkCmdSetPrimitiveTopology)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetPrimitiveTopology");
        if (!sit_render.vk.pfn_cmd_set_primitive_topology) {
            sit_render.vk.pfn_cmd_set_primitive_topology = (PFN_vkCmdSetPrimitiveTopology)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetPrimitiveTopology");
        }
        sit_render.vk.pfn_cmd_set_depth_test_enable = (PFN_vkCmdSetDepthTestEnable)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetDepthTestEnable");
        if (!sit_render.vk.pfn_cmd_set_depth_test_enable) {
            sit_render.vk.pfn_cmd_set_depth_test_enable = (PFN_vkCmdSetDepthTestEnable)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetDepthTestEnable");
        }
        sit_render.vk.pfn_cmd_set_depth_write_enable = (PFN_vkCmdSetDepthWriteEnable)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetDepthWriteEnable");
        if (!sit_render.vk.pfn_cmd_set_depth_write_enable) {
            sit_render.vk.pfn_cmd_set_depth_write_enable = (PFN_vkCmdSetDepthWriteEnable)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetDepthWriteEnable");
        }
        sit_render.vk.pfn_cmd_set_depth_compare_op = (PFN_vkCmdSetDepthCompareOp)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetDepthCompareOp");
        if (!sit_render.vk.pfn_cmd_set_depth_compare_op) {
            sit_render.vk.pfn_cmd_set_depth_compare_op = (PFN_vkCmdSetDepthCompareOp)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetDepthCompareOp");
        }
        sit_render.vk.pfn_cmd_set_stencil_test_enable = (PFN_vkCmdSetStencilTestEnable)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetStencilTestEnable");
        if (!sit_render.vk.pfn_cmd_set_stencil_test_enable) {
            sit_render.vk.pfn_cmd_set_stencil_test_enable = (PFN_vkCmdSetStencilTestEnable)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetStencilTestEnable");
        }
        sit_render.vk.pfn_cmd_set_stencil_op = (PFN_vkCmdSetStencilOp)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetStencilOp");
        if (!sit_render.vk.pfn_cmd_set_stencil_op) {
            sit_render.vk.pfn_cmd_set_stencil_op = (PFN_vkCmdSetStencilOp)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetStencilOp");
        }
    }
    if (sit_render.vk.depth_bias_dynamic_enabled) {
        sit_render.vk.pfn_cmd_set_depth_bias_enable = (PFN_vkCmdSetDepthBiasEnable)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetDepthBiasEnable");
        if (!sit_render.vk.pfn_cmd_set_depth_bias_enable) {
            sit_render.vk.pfn_cmd_set_depth_bias_enable = (PFN_vkCmdSetDepthBiasEnable)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetDepthBiasEnable");
        }
        sit_render.vk.pfn_cmd_set_depth_bias = (PFN_vkCmdSetDepthBias)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetDepthBias");
        if (!sit_render.vk.pfn_cmd_set_depth_bias) {
        sit_render.vk.pfn_cmd_set_depth_bias = (PFN_vkCmdSetDepthBias)
            vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetDepthBias");
        }
    }
    if (sit_render.vk.extended_dynamic_state3_color_write_enabled) {
        sit_render.vk.pfn_cmd_set_color_write_mask_ext = (PFN_vkCmdSetColorWriteMaskEXT)
            vkGetDeviceProcAddr(sit_render.vk.device, "vkCmdSetColorWriteMaskEXT");
        if (!sit_render.vk.pfn_cmd_set_color_write_mask_ext) {
            sit_render.vk.pfn_cmd_set_color_write_mask_ext = (PFN_vkCmdSetColorWriteMaskEXT)
                vkGetInstanceProcAddr(sit_render.vk.instance, "vkCmdSetColorWriteMaskEXT");
        }
        if (!sit_render.vk.pfn_cmd_set_color_write_mask_ext) {
            sit_render.vk.extended_dynamic_state3_color_write_enabled = false;
            fprintf(stderr, "Situation [Vulkan]: vkCmdSetColorWriteMaskEXT unavailable; color write mask API disabled.\n");
            fflush(stderr);
        }
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates the Vulkan Memory Allocator (VMA) instance.
 *
 * @details This helper function initializes the Vulkan Memory Allocator (VMA) library, which provides a higher-level, more efficient interface for allocating and managing GPU memory (VkDeviceMemory) and associating it with Vulkan objects like VkBuffer and VkImage.
 *          VMA handles memory type selection, sub-allocation, and defragmentation internally.
 *
 * @return SITUATION_SUCCESS on successful creation of the VMA allocator.
 * @return SITUATION_ERROR_INVALID_PARAM if required Vulkan handles (instance, physicalDevice, device) in `sit_render.vk` are invalid.
 * @return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED if `vmaCreateAllocator` fails for any reason (e.g., incompatible Vulkan version, driver issues, internal VMA error). A specific error message is set.
 *
 * @note This function must be called after the Vulkan instance, physical device, and logical device have been successfully created and their handles
 *       stored in `sit_render.vk.instance`, `sit_render.vk.physical_device`, and `sit_render.vk.device` respectively.
 * @note The created `VmaAllocator` handle is stored in `sit_render.vk.vma_allocator`.
 * @note The target Vulkan API version is specified as `VK_API_VERSION_1_4`.
 *       Ensure this aligns with the version used in `VkApplicationInfo` and is supported by the chosen physical device and driver.
 *
 * @see _SituationInitVulkan(), _SituationVulkanCreateInstance(), _SituationVulkanPickPhysicalDevice(), _SituationVulkanCreateLogicalDevice() _SituationCleanupVulkan() (for destruction)
 */
static SituationError _SituationVulkanCreateAllocator(void) {
    // --- 1. Input Validation (Defensive for internal helper) ---
    // Check if the prerequisite Vulkan handles are valid before passing them to VMA.
    // While the library's init sequence should guarantee this, a check adds robustness.
    if (sit_render.vk.instance == VK_NULL_HANDLE ||
        sit_render.vk.physical_device == VK_NULL_HANDLE ||
        sit_render.vk.device == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateAllocator: Vulkan instance, physical device, or logical device is NULL." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Configure VMA Creation Info ---
    VmaAllocatorCreateInfo allocator_info = {0}; // Explicitly zero-initialize
    // VmaAllocatorCreateInfo does not have sType field
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_4; // Specify target Vulkan API version
    allocator_info.instance = sit_render.vk.instance; // Link to Vulkan instance
    allocator_info.physicalDevice = sit_render.vk.physical_device; // Link to physical device
    allocator_info.device = sit_render.vk.device; // Link to logical device
    // allocator_info.pAllocationCallbacks = NULL; // Use default allocation callbacks
    // allocator_info.pDeviceMemoryCallbacks = NULL; // No custom memory callbacks
    // allocator_info.pHeapSizeLimit = NULL; // No heap size limits

    // Enable buffer device address support (required for shader device address)
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    // Set up Vulkan function pointers for VMA (required for VMA_DYNAMIC_VULKAN_FUNCTIONS)
    VmaVulkanFunctions vulkan_functions = {0};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    allocator_info.pVulkanFunctions = &vulkan_functions;

    // --- 3. Create the VMA Allocator ---
    // This is the actual call to the VMA library to create the allocator instance.
    VkResult result = vmaCreateAllocator(&allocator_info, &sit_render.vk.vma_allocator);
    if (result != VK_SUCCESS) {
        // vmaCreateAllocator failed. This usually indicates a problem with
        // the provided Vulkan handles, an unsupported API version, or an internal VMA issue.
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "Failed to create Vulkan Memory Allocator (VMA) (VkResult: 0x%x). Check Vulkan handles, API version (1.1), or driver compatibility.",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, error_detail);
        // Ensure the handle is explicitly invalid on failure.
        sit_render.vk.vma_allocator = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }

    // --- 4. Success ---
    // If we reach here, the VmaAllocator was created successfully.
    // The handle is stored in sit_render.vk.vma_allocator for use by subsequent
    // buffer/image creation functions that rely on VMA.
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Frees memory allocated by `_SituationVulkanQuerySwapchainSupport`.
 * @details This is a mandatory cleanup utility function. It frees the dynamically allocated arrays for surface formats and presentation modes that are stored within a `_SituationVulkanSwapchainSupportDetails` struct.
 *          It must be called after the details gathered by the query function are no longer needed to prevent memory leaks.
 *
 * @param details A pointer to the `_SituationVulkanSwapchainSupportDetails` struct whose internal arrays should be freed. It is safe to pass NULL to this function.
 *
 * @note This function is for internal use only and is a critical part of the resource management for Vulkan initialization helpers.
 *
 * @see _SituationVulkanQuerySwapchainSupport()
 */
static void _SituationVulkanFreeSwapchainSupportDetails(_SituationVulkanSwapchainSupportDetails* details) {
    if (details == NULL) return;
    // SIT_FREE() is safe to call on a NULL pointer.
    SIT_FREE(details->formats);
    SIT_FREE(details->present_modes);
    // No need to zero out the struct, as it's typically a stack-allocated variable
    // that will go out of scope.
}

/**
 * @brief [INTERNAL] Queries a physical device for its swapchain support details for the active surface.
 * @details This function populates a `_SituationVulkanSwapchainSupportDetails` struct with all the necessary information required to create a valid swapchain. This includes:
 *          1.  Surface capabilities (min/max image count, current extent, supported transforms, etc.).
 *          2.  A list of available surface formats (`VkSurfaceFormatKHR`).
 *          3.  A list of available presentation modes (`VkPresentModeKHR`).
 *
 * @warning This function allocates new memory for the `formats` and `present_modes` arrays within the `out_details` struct. The caller is **responsible** for freeing this memory by calling `_SituationVulkanFreeSwapchainSupportDetails()` on the struct once the data is no longer needed.
 *          Failure to do so will result in a memory leak.
 *
 * @param device The `VkPhysicalDevice` to query.
 * @param[out] out_details A pointer to the struct that will be filled with the queried support details. This pointer must not be NULL.
 *
 * @note This function must be called after the `VkInstance` and `VkSurfaceKHR` have been created. It is a key prerequisite for both device suitability checks and swapchain creation.
 * @note This function is for internal use only.
 *
 * @see _SituationVulkanFreeSwapchainSupportDetails(), _SituationIsDeviceSuitable(), _SituationVulkanCreateSwapchain()
 */
static void _SituationVulkanQuerySwapchainSupport(VkPhysicalDevice device, _SituationVulkanSwapchainSupportDetails* out_details) {
    // Get the basic surface capabilities (min/max image count, extent, etc.)
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, sit_render.vk.surface, &out_details->capabilities);

    out_details->format_count = 0;
    out_details->formats = NULL;

    /* Prefer SurfaceFormats2 — extended color spaces (HDR10 ST2084) may be omitted from v1 query. */
    PFN_vkGetPhysicalDeviceSurfaceFormats2KHR pfn_get_surface_formats2 =
        (PFN_vkGetPhysicalDeviceSurfaceFormats2KHR)vkGetInstanceProcAddr(
            sit_render.vk.instance, "vkGetPhysicalDeviceSurfaceFormats2KHR");
    if (pfn_get_surface_formats2) {
        VkPhysicalDeviceSurfaceInfo2KHR surface_info = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
            .surface = sit_render.vk.surface,
        };
        uint32_t fmt2_count = 0;
        pfn_get_surface_formats2(device, &surface_info, &fmt2_count, NULL);
        if (fmt2_count > 0) {
            VkSurfaceFormat2KHR* fmt2 =
                (VkSurfaceFormat2KHR*)SIT_MALLOC(sizeof(VkSurfaceFormat2KHR) * fmt2_count);
            if (fmt2) {
                for (uint32_t i = 0; i < fmt2_count; ++i) {
                    fmt2[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
                    fmt2[i].pNext = NULL;
                }
                pfn_get_surface_formats2(device, &surface_info, &fmt2_count, fmt2);
                out_details->formats = (VkSurfaceFormatKHR*)SIT_MALLOC(sizeof(VkSurfaceFormatKHR) * fmt2_count);
                if (out_details->formats) {
                    for (uint32_t i = 0; i < fmt2_count; ++i) {
                        out_details->formats[i] = fmt2[i].surfaceFormat;
                    }
                    out_details->format_count = fmt2_count;
                }
                SIT_FREE(fmt2);
            }
        }
    }

    if (out_details->format_count == 0) {
        // Get the supported surface formats (legacy fallback)
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, sit_render.vk.surface, &out_details->format_count, NULL);
        if (out_details->format_count != 0) {
            out_details->formats = (VkSurfaceFormatKHR*)SIT_MALLOC(sizeof(VkSurfaceFormatKHR) * out_details->format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, sit_render.vk.surface, &out_details->format_count, out_details->formats);
        }
    }

    // Get the supported presentation modes
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, sit_render.vk.surface, &out_details->present_mode_count, NULL);
    if (out_details->present_mode_count != 0) {
        out_details->present_modes = (VkPresentModeKHR*)SIT_MALLOC(sizeof(VkPresentModeKHR) * out_details->present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, sit_render.vk.surface, &out_details->present_mode_count, out_details->present_modes);
    } else {
        out_details->present_modes = NULL;
    }
}

/**
 * @brief [INTERNAL] Evaluates a physical device to determine its suitability and assigns it a score.
 * @details This helper function is the core of the GPU selection logic. It performs a series of checks to determine if a `VkPhysicalDevice` meets the minimum requirements for the application and then scores it based on desirable properties. A score of 0 indicates the device is unsuitable.
 *
 * @par Suitability Criteria
 *   The function first performs several pass/fail checks. A device is considered unsuitable (score 0) if it fails any of these:
 *   - It does not have a queue family that supports both graphics and presentation operations.
 *   - It does not support the mandatory `VK_KHR_swapchain` device extension.
 *   - It does not offer at least one supported surface format and one presentation mode for the active window surface.
 *
 * @par Scoring System
 *   If a device passes all suitability checks, it is assigned a score based on the following preferences:
 *   - **Device Type:** Discrete GPUs are heavily favored and receive a high score (+1000), while integrated GPUs receive a smaller bonus (+100).
 *   - **Capabilities:** Additional points are awarded for features like a larger maximum 2D texture dimension, indicating a more powerful GPU.
 *
 * @param device The `VkPhysicalDevice` handle to evaluate.
 *
 * @return An integer score representing the suitability of the device. A higher score is better. Returns `0` if the device does not meet the minimum requirements.
 *
 * @note This function is for internal use by `_SituationVulkanPickPhysicalDevice` only.
 *
 * @see _SituationVulkanPickPhysicalDevice(), _SituationVulkanFindQueueFamilies(), _SituationVulkanQuerySwapchainSupport()
 */
static int _SituationIsDeviceSuitable(VkPhysicalDevice device) {
    // --- 1. Essential Feature Checks (Pass/Fail) ---

    // Check if the device supports required queue families
    _SituationQueueFamilyIndices indices = _SituationVulkanFindQueueFamilies(device, sit_render.vk.surface);
    if (!indices.graphics_family_has_value || !indices.present_family_has_value) {
        return 0; // Not suitable
    }

    // Check for required device extension support (e.g., swapchain)
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
    VkExtensionProperties* available_extensions = (VkExtensionProperties*)SIT_MALLOC(sizeof(VkExtensionProperties) * extension_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, available_extensions);

    bool swapchain_supported = false;
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(available_extensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            swapchain_supported = true;
            break;
        }
    }
    SIT_FREE(available_extensions);
    if (!swapchain_supported) return 0; // Not suitable

    // Check if the swapchain is adequate (has at least one format and one present mode)
    // You already have a helper for this from a previous step, let's assume it's called _SituationVulkanQuerySwapchainSupport
    _SituationVulkanSwapchainSupportDetails swapchain_support;
    _SituationVulkanQuerySwapchainSupport(device, &swapchain_support);
    bool swapchain_adequate = (swapchain_support.format_count > 0 && swapchain_support.present_mode_count > 0);
    _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support); // Helper to free the format/present mode arrays
    if (!swapchain_adequate) return 0; // Not suitable

    // --- 2. Scoring Based on Desirable Properties ---
    int score = 0;
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(device, &device_properties);
    vkGetPhysicalDeviceFeatures(device, &device_features);

    // Strongly prefer discrete GPUs
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    } else if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 100;
    }

    // Add points for larger max texture size
    score += device_properties.limits.maxImageDimension2D / 1024;

    // (Future) Add points for other features
    // if (device_features.geometryShader) { score += 100; }

    return score;
}

/**
 * @brief [INTERNAL] Finds the indices of queue families that support graphics and presentation.
 * @details This function iterates through all available queue families of a given physical device to find indices for two essential capabilities:
 *          1.  A queue family that supports graphics commands (`VK_QUEUE_GRAPHICS_BIT`).
 *          2.  A queue family that supports presenting to the application's window surface (`vkGetPhysicalDeviceSurfaceSupportKHR`).
 *
 * The function returns a struct containing the found indices and boolean flags indicating whether each was found. A device is only suitable for the application if both a graphics and a present family are found.
 *
 * @param device The `VkPhysicalDevice` to inspect.
 * @param surface The `VkSurfaceKHR` to check for presentation support against.
 *
 * @return A `_SituationQueueFamilyIndices` struct. The `graphics_family_has_value` and `present_family_has_value` members will be `true` if suitable families were found, and the corresponding `_family` members will hold their indices.
 *
 * @note The graphics and present queue families may or may not be the same index. This function correctly handles both cases.
 * @note This function is for internal use only, primarily called by `_SituationIsDeviceSuitable` and `_SituationVulkanCreateSwapchain`.
 */
static _SituationQueueFamilyIndices _SituationVulkanFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    _SituationQueueFamilyIndices indices;
    memset(&indices, 0, sizeof(indices));

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)SIT_MALLOC(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family = i;
            indices.graphics_family_has_value = true;
        }

        if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (!indices.compute_family_has_value) {
                indices.compute_family = i;
                indices.compute_family_has_value = true;
            } else {
                // Prefer distinct compute queue (no graphics bit)
                bool current_distinct = !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT);
                bool old_distinct = !(queue_families[indices.compute_family].queueFlags & VK_QUEUE_GRAPHICS_BIT);
                if (current_distinct && !old_distinct) {
                    indices.compute_family = i;
                }
            }
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support) {
            indices.present_family = i;
            indices.present_family_has_value = true;
        }
    }

    SIT_FREE(queue_families);
    return indices;
}

/**
 * @brief [INTERNAL] Creates the Vulkan swapchain for presenting rendered images to the window.
 * @details This function creates the `VkSwapchainKHR`, which is a collection of renderable images that are queued for presentation to the screen. It is a central component of the rendering pipeline.
 *
 * @par Creation Logic
 *   1.  **Query Support:** It first calls `_SituationVulkanQuerySwapchainSupport` to get the capabilities, formats, and present modes of the selected physical device.
 *   2.  **Select Best Format:** Prefers **`VK_FORMAT_B8G8R8A8_UNORM`** (with `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`) when offered so swapchain staging bytes match typical **`glReadPixels(..., GL_RGBA)`** / harness expectations. Falls back to **`B8G8R8A8_SRGB`**, then first listed format.
 *   3.  **Select Best Present Mode:** It iterates through available modes, preferring `VK_PRESENT_MODE_MAILBOX_KHR` (for low-latency, tear-free rendering) and falling back to the guaranteed `VK_PRESENT_MODE_FIFO_KHR` (standard V-Sync).
 *   4.  **Determine Extent & Image Count:** It determines the resolution of the swapchain images and the number of images in the chain based on the surface capabilities and current window size.
 *   5.  **Create Swapchain:** It populates the `VkSwapchainCreateInfoKHR` struct with the chosen settings and creates the `VkSwapchainKHR` object.
 *   6.  **Retrieve Images:** It retrieves the handles to the created `VkImage`s within the swapchain.
 *
 * Upon success, it stores the swapchain handle, image format, extent, and image handles in the global state (`sit_render.vk`).
 *
 * @return SITUATION_SUCCESS on successful creation of the swapchain and retrieval of its images.
 * @return SITUATION_ERROR_VULKAN_UNSUPPORTED if the physical device does not offer any compatible formats or present modes.
 * @return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED if the `vkCreateSwapchainKHR` call fails for any other reason.
 *
 * @note This function must be called after the logical device and surface have been created.
 * @warning This function is for internal use by `_SituationInitVulkan` and `_SituationVulkanRecreateSwapchain`.
 *
 * @see _SituationVulkanQuerySwapchainSupport(), _SituationVulkanCreateImageViews(), _SituationVulkanRecreateSwapchain()
 */

static SituationError _SituationVulkanRecreateSwapchain(void);

/** Keep sit_gs.main_window_* aligned with GLFW (OpenGL parity for GetRenderWidth/Height). */
static void _SituationVulkanSyncMainWindowFromGLFW(void) {
    if (!sit_gs.sit_glfw_window) {
        return;
    }
    int fb_w = 0;
    int fb_h = 0;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fb_w, &fb_h);
    if (fb_w > 0 && fb_h > 0) {
        sit_gs.main_window_width = fb_w;
        sit_gs.main_window_height = fb_h;
    }
}

static void _SituationVulkanDestroyImage(VkImage image, VmaAllocation allocation);
static VkImageView _SituationVulkanCreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
static void _SituationVulkanTransitionImageLayout(VkCommandBuffer cmd, VkImage image, uint32_t mip_levels, VkImageLayout old_layout, VkImageLayout new_layout);

static void _SituationVulkanDestroyCanvasResources(void) {
#if defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.canvas_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.canvas_framebuffer, NULL);
        sit_render.vk.canvas_framebuffer = VK_NULL_HANDLE;
    }
    if (sit_render.vk.canvas_color_view != VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.canvas_color_view, NULL);
        sit_render.vk.canvas_color_view = VK_NULL_HANDLE;
    }
    if (sit_render.vk.canvas_color_image != VK_NULL_HANDLE) {
        _SituationVulkanDestroyImage(sit_render.vk.canvas_color_image, sit_render.vk.canvas_color_memory);
        sit_render.vk.canvas_color_image = VK_NULL_HANDLE;
        sit_render.vk.canvas_color_memory = VK_NULL_HANDLE;
    }
    if (sit_render.vk.canvas_depth_view != VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.canvas_depth_view, NULL);
        sit_render.vk.canvas_depth_view = VK_NULL_HANDLE;
    }
    if (sit_render.vk.canvas_depth_image != VK_NULL_HANDLE) {
        _SituationVulkanDestroyImage(sit_render.vk.canvas_depth_image, sit_render.vk.canvas_depth_memory);
        sit_render.vk.canvas_depth_image = VK_NULL_HANDLE;
        sit_render.vk.canvas_depth_memory = VK_NULL_HANDLE;
    }
    sit_render.vk.canvas_resource_width = 0;
    sit_render.vk.canvas_resource_height = 0;
#endif
}

static SituationError _SituationVulkanEnsureCanvasResources(void) {
#if !defined(SITUATION_USE_VULKAN)
    return SITUATION_SUCCESS;
#else
    if (!_SituationRenderCanvasStretchActive()) {
        _SituationVulkanDestroyCanvasResources();
        return SITUATION_SUCCESS;
    }
    if (sit_render.vk.main_window_render_pass == VK_NULL_HANDLE || sit_render.vk.depth_format == VK_FORMAT_UNDEFINED) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }

    uint32_t cw = (uint32_t)sit_gs.render_canvas_width;
    uint32_t ch = (uint32_t)sit_gs.render_canvas_height;
    if (cw < 1 || ch < 1) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (sit_render.vk.canvas_framebuffer != VK_NULL_HANDLE &&
        sit_render.vk.canvas_resource_width == cw &&
        sit_render.vk.canvas_resource_height == ch) {
        return SITUATION_SUCCESS;
    }

    _SituationVulkanDestroyCanvasResources();

    if (_SituationVulkanCreateImage(
            cw, ch, 1, sit_render.vk.swapchain_image_format, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            VK_SAMPLE_COUNT_1_BIT,
            &sit_render.vk.canvas_color_image, &sit_render.vk.canvas_color_memory) != SITUATION_SUCCESS) {
        _SituationVulkanDestroyCanvasResources();
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }
    sit_render.vk.canvas_color_view = _SituationVulkanCreateImageView(
        sit_render.vk.canvas_color_image, sit_render.vk.swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
    if (sit_render.vk.canvas_color_view == VK_NULL_HANDLE) {
        _SituationVulkanDestroyCanvasResources();
        return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
    }

    if (_SituationVulkanCreateImage(
            cw, ch, 1, sit_render.vk.depth_format, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            VK_SAMPLE_COUNT_1_BIT,
            &sit_render.vk.canvas_depth_image, &sit_render.vk.canvas_depth_memory) != SITUATION_SUCCESS) {
        _SituationVulkanDestroyCanvasResources();
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }
    sit_render.vk.canvas_depth_view = _SituationVulkanCreateImageView(
        sit_render.vk.canvas_depth_image, sit_render.vk.depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
    if (sit_render.vk.canvas_depth_view == VK_NULL_HANDLE) {
        _SituationVulkanDestroyCanvasResources();
        return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
    }

    VkImageView attachments[] = {
        sit_render.vk.canvas_color_view,
        sit_render.vk.canvas_depth_view
    };
    VkFramebufferCreateInfo fb_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb_info.renderPass = sit_render.vk.main_window_render_pass;
    fb_info.attachmentCount = 2;
    fb_info.pAttachments = attachments;
    fb_info.width = cw;
    fb_info.height = ch;
    fb_info.layers = 1;
    if (vkCreateFramebuffer(sit_render.vk.device, &fb_info, NULL, &sit_render.vk.canvas_framebuffer) != VK_SUCCESS) {
        _SituationVulkanDestroyCanvasResources();
        return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
    }

    sit_render.vk.canvas_resource_width = cw;
    sit_render.vk.canvas_resource_height = ch;
    return SITUATION_SUCCESS;
#endif
}

static void _SituationVulkanRecordCanvasStretchBlit(VkCommandBuffer cmd) {
#if defined(SITUATION_USE_VULKAN)
    if (!_SituationRenderCanvasStretchActive() ||
        cmd == VK_NULL_HANDLE ||
        sit_render.vk.canvas_color_image == VK_NULL_HANDLE ||
        !sit_render.vk.swapchain_valid ||
        !sit_render.vk.swapchain_images ||
        sit_render.vk.current_image_index >= sit_render.vk.swapchain_image_count) {
        return;
    }

    VkImage dst = sit_render.vk.swapchain_images[sit_render.vk.current_image_index];
    uint32_t cw = sit_render.vk.canvas_resource_width;
    uint32_t ch = sit_render.vk.canvas_resource_height;
    uint32_t dw = sit_render.vk.swapchain_extent.width;
    uint32_t dh = sit_render.vk.swapchain_extent.height;
    if (cw < 1 || ch < 1 || dw < 1 || dh < 1) {
        return;
    }

    _SituationVulkanTransitionImageLayout(cmd, sit_render.vk.canvas_color_image, 1,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    _SituationVulkanTransitionImageLayout(cmd, dst, 1,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageBlit blit = {0};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1].x = (int32_t)cw;
    blit.srcOffsets[1].y = (int32_t)ch;
    blit.srcOffsets[1].z = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[1].x = (int32_t)dw;
    blit.dstOffsets[1].y = (int32_t)dh;
    blit.dstOffsets[1].z = 1;
    vkCmdBlitImage(cmd,
        sit_render.vk.canvas_color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit, VK_FILTER_NEAREST);

    _SituationVulkanTransitionImageLayout(cmd, sit_render.vk.canvas_color_image, 1,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    _SituationVulkanTransitionImageLayout(cmd, dst, 1,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
#endif
}

static SituationError _SituationVulkanRecreateSurfaceKHR(void) {
    if (sit_render.vk.instance == VK_NULL_HANDLE || !sit_gs.sit_glfw_window) {
        return SITUATION_ERROR_NOT_INITIALIZED;
    }
    if (sit_render.vk.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(sit_render.vk.instance, sit_render.vk.surface, NULL);
        sit_render.vk.surface = VK_NULL_HANDLE;
    }
    return _SituationVulkanCreateSurface();
}

static VkExtent2D _SituationVulkanPickSwapchainExtent(
    const VkSurfaceCapabilitiesKHR* caps, int fb_w, int fb_h)
{
    VkExtent2D extent;
    if (caps->currentExtent.width != UINT32_MAX) {
        extent = caps->currentExtent;
    } else if (fb_w > 0 && fb_h > 0) {
        extent.width = (uint32_t)fmaxf((float)caps->minImageExtent.width,
            fminf((float)caps->maxImageExtent.width, (float)fb_w));
        extent.height = (uint32_t)fmaxf((float)caps->minImageExtent.height,
            fminf((float)caps->maxImageExtent.height, (float)fb_h));
    } else {
        extent = caps->currentExtent;
    }
    return extent;
}

static bool _SituationVulkanSurfaceSupports10BitSdr(const _SituationVulkanSwapchainSupportDetails* support) {
    if (!support || !support->formats) {
        return false;
    }
    for (uint32_t i = 0; i < support->format_count; i++) {
        const VkFormat fmt = support->formats[i].format;
        if (support->formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
            (fmt == VK_FORMAT_A2R10G10B10_UNORM_PACK32 || fmt == VK_FORMAT_A2B10G10R10_UNORM_PACK32)) {
            return true;
        }
    }
    return false;
}

static bool _SituationVulkanSurfaceSupportsHdr10(const _SituationVulkanSwapchainSupportDetails* support) {
    if (!support || !support->formats) {
        return false;
    }
    for (uint32_t i = 0; i < support->format_count; i++) {
        const VkFormat fmt = support->formats[i].format;
        if (support->formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT &&
            (fmt == VK_FORMAT_A2R10G10B10_UNORM_PACK32 || fmt == VK_FORMAT_A2B10G10R10_UNORM_PACK32)) {
            return true;
        }
    }
    return false;
}

static VkSurfaceFormatKHR _SituationVulkanPick8BitSurfaceFormat(const _SituationVulkanSwapchainSupportDetails* support) {
    VkSurfaceFormatKHR surface_format = support->formats[0];
    bool picked = false;
    /* UNORM first: readback + SituationLoadImageFromScreen parity with GL RGBA8 expectations (harness pixel asserts). */
    for (uint32_t i = 0; i < support->format_count && !picked; i++) {
        if (support->formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            support->formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = support->formats[i];
            picked = true;
        }
    }
    for (uint32_t i = 0; i < support->format_count && !picked; i++) {
        if (support->formats[i].format == VK_FORMAT_R8G8B8A8_UNORM &&
            support->formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = support->formats[i];
            picked = true;
        }
    }
    for (uint32_t i = 0; i < support->format_count && !picked; i++) {
        if (support->formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            support->formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = support->formats[i];
            picked = true;
        }
    }
    return surface_format;
}

static VkSurfaceFormatKHR _SituationVulkanPickSurfaceFormat(
    const _SituationVulkanSwapchainSupportDetails* support,
    SituationOutputColorDepth policy,
    bool* out_using_10bit)
{
    sit_render.vk.surface_supports_10bit_sdr = _SituationVulkanSurfaceSupports10BitSdr(support);
    sit_render.vk.surface_supports_hdr10 = _SituationVulkanSurfaceSupportsHdr10(support);

    const bool os_hdr_enabled = _SituationWindowMonitorDxgiHdrEnabled();
    const bool wants_hdr = _SituationWantsHdr10Output(policy, os_hdr_enabled);
    const bool try_hdr = wants_hdr && sit_render.vk.surface_supports_hdr10 && os_hdr_enabled;

    if (try_hdr) {
        static const VkFormat k_hdr10_formats[] = {
            VK_FORMAT_A2R10G10B10_UNORM_PACK32,
            VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        };
        for (uint32_t f = 0; f < sizeof(k_hdr10_formats) / sizeof(k_hdr10_formats[0]); f++) {
            for (uint32_t i = 0; i < support->format_count; i++) {
                if (support->formats[i].format == k_hdr10_formats[f] &&
                    support->formats[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                    if (out_using_10bit) {
                        *out_using_10bit = true;
                    }
                    _SituationSetOutputColorDepthState(true, true);
                    sit_render.vk.swapchain_color_space = (uint32_t)support->formats[i].colorSpace;
                    #ifdef SITUATION_VULKAN_DEBUG
                    printf("Situation [Vulkan Debug]: swapchain fmt=%u + HDR10_ST2084 (HDR active)\n",
                        (unsigned)support->formats[i].format);
                    fflush(stdout);
                    #endif
                    return support->formats[i];
                }
            }
        }
    }

    if (policy == SIT_OUTPUT_COLOR_HDR10 && !os_hdr_enabled) {
        fprintf(stderr,
            "[Vulkan] SIT_OUTPUT_COLOR_HDR10 requested but OS HDR is off on the window monitor — falling back to 10-bit SDR / 8-bit\n");
        fflush(stderr);
    } else if (wants_hdr && os_hdr_enabled && !sit_render.vk.surface_supports_hdr10) {
        fprintf(stderr,
            "[Vulkan] HDR10 requested but WSI has no A2R10G10B10+HDR10_ST2084 — falling back to 10-bit SDR / 8-bit "
            "(surface_format_count=%u)\n",
            support->format_count);
        for (uint32_t i = 0; i < support->format_count && i < 24u; i++) {
            fprintf(stderr,
                "[Vulkan]   surface_format[%u]: fmt=%u colorSpace=%u\n",
                i,
                (unsigned)support->formats[i].format,
                (unsigned)support->formats[i].colorSpace);
        }
        fflush(stderr);
    }

    if (_SituationWants10BitOutput(policy) && sit_render.vk.surface_supports_10bit_sdr) {
        static const VkFormat k_sdr10_formats[] = {
            VK_FORMAT_A2R10G10B10_UNORM_PACK32,
            VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        };
        for (uint32_t f = 0; f < sizeof(k_sdr10_formats) / sizeof(k_sdr10_formats[0]); f++) {
            for (uint32_t i = 0; i < support->format_count; i++) {
                if (support->formats[i].format == k_sdr10_formats[f] &&
                    support->formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                if (out_using_10bit) {
                    *out_using_10bit = true;
                }
                _SituationSetOutputColorDepthState(true, false);
                sit_render.vk.swapchain_color_space = (uint32_t)support->formats[i].colorSpace;
                #ifdef SITUATION_VULKAN_DEBUG
                printf("Situation [Vulkan Debug]: swapchain format A2R10G10B10_UNORM_PACK32 + SRGB_NONLINEAR (10-bit SDR)\n");
                fflush(stdout);
                #endif
                return support->formats[i];
                }
            }
        }
    }

    if (out_using_10bit) {
        *out_using_10bit = false;
    }
    _SituationSetOutputColorDepthState(false, false);
    if (policy == SIT_OUTPUT_COLOR_10BIT && !sit_render.vk.surface_supports_10bit_sdr) {
        fprintf(stderr,
            "[Vulkan] 10-bit output requested but WSI has no A2R10G10B10+SRGB_NONLINEAR — using 8-bit swapchain\n");
        fflush(stderr);
    }
    VkSurfaceFormatKHR surface_format = _SituationVulkanPick8BitSurfaceFormat(support);
    sit_render.vk.swapchain_color_space = (uint32_t)surface_format.colorSpace;
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: swapchain format %u + colorSpace %u (8-bit path)\n",
        (unsigned)surface_format.format, (unsigned)surface_format.colorSpace);
    fflush(stdout);
    #endif
    return surface_format;
}

/** Recreate swapchain when GLFW backbuffer size diverges (resize/fullscreen/HDR). */
static SituationError _SituationVulkanEnsureSwapchainMatchesFramebuffer(void) {
    if (!sit_render.vk.swapchain_valid || !sit_gs.sit_glfw_window) {
        return SITUATION_SUCCESS;
    }
    int fb_w = 0;
    int fb_h = 0;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fb_w, &fb_h);
    if (fb_w < 1 || fb_h < 1) {
        return SITUATION_SUCCESS;
    }
    if ((uint32_t)fb_w == sit_render.vk.swapchain_extent.width &&
        (uint32_t)fb_h == sit_render.vk.swapchain_extent.height) {
        return SITUATION_SUCCESS;
    }
    sit_render.vk.framebuffer_resized = true;
    if (_SituationVulkanRecreateSwapchain() != SITUATION_SUCCESS) {
        return SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED;
    }
    return SITUATION_SUCCESS;
}

static SituationError _SituationVulkanCreateSwapchain(VkSwapchainKHR old_swapchain) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating swapchain...\n"); fflush(stdout);
    #endif

    _SituationPumpWindowEventsGuarded();
    int fb_w = 0;
    int fb_h = 0;
    if (sit_gs.sit_glfw_window) {
        glfwGetFramebufferSize(sit_gs.sit_glfw_window, &fb_w, &fb_h);
    }

    /* Re-probe DXGI HDR state before format pick (monitor migration / OS HDR toggle). */
    _SituationCachePhysicalDisplays();

_SituationVulkanSwapchainSupportDetails swapchain_support = {0};
    _SituationVulkanQuerySwapchainSupport(sit_render.vk.physical_device, &swapchain_support);

    if (swapchain_support.format_count == 0 || swapchain_support.present_mode_count == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "GPU does not support any suitable swapchain formats or present modes.");
        _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support);
        return SITUATION_ERROR_VULKAN_UNSUPPORTED;
    }

    VkSurfaceFormatKHR surface_format = _SituationVulkanPickSurfaceFormat(
        &swapchain_support, sit_gs.output_color_depth_policy, NULL);

    // Select present mode based on VSync flag
    // VK_PRESENT_MODE_FIFO_KHR = VSync ON (guaranteed available, caps at refresh rate)
    // VK_PRESENT_MODE_MAILBOX_KHR = VSync OFF (triple buffering, no tearing, unlimited FPS)
    // VK_PRESENT_MODE_IMMEDIATE_KHR = VSync OFF (no buffering, may tear, unlimited FPS)
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;  // Default to VSync ON

    bool vsync_enabled = (sit_gs.active_profile_window_flags & SITUATION_FLAG_VSYNC_HINT) != 0;

    if (!vsync_enabled) {
        // VSync OFF - prefer IMMEDIATE (truly unlimited FPS) over MAILBOX (may cap at 2x refresh)
        for (uint32_t i = 0; i < swapchain_support.present_mode_count; i++) {
            if (swapchain_support.present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                break;  // IMMEDIATE gives truly unlimited FPS
            }
            if (swapchain_support.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                // Keep looking for IMMEDIATE which is preferred for max FPS
            }
        }
    }
    // else: keep VK_PRESENT_MODE_FIFO_KHR (VSync ON)

    /* Phase 6: Windows HDR compositor expects FIFO; avoid IMMEDIATE on HDR10 swapchains. */
    if (sit_render.output_hdr_active) {
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D extent = _SituationVulkanPickSwapchainExtent(
        &swapchain_support.capabilities, fb_w, fb_h);

    /* WSI currentExtent can lag after glfwSetWindowMonitor / DPI changes. Refresh the
     * surface once so swapchain pixels match glfwGetFramebufferSize (OpenGL parity). */
    if (fb_w > 0 && fb_h > 0 &&
        (extent.width != (uint32_t)fb_w || extent.height != (uint32_t)fb_h)) {
        if (_SituationVulkanRecreateSurfaceKHR() == SITUATION_SUCCESS) {
            _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support);
            memset(&swapchain_support, 0, sizeof(swapchain_support));
            _SituationVulkanQuerySwapchainSupport(sit_render.vk.physical_device, &swapchain_support);
            if (swapchain_support.format_count == 0 || swapchain_support.present_mode_count == 0) {
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED,
                    "GPU swapchain support missing after surface recreate.");
                _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support);
                return SITUATION_ERROR_VULKAN_UNSUPPORTED;
            }
            extent = _SituationVulkanPickSwapchainExtent(&swapchain_support.capabilities, fb_w, fb_h);
            surface_format = _SituationVulkanPickSurfaceFormat(
                &swapchain_support, sit_gs.output_color_depth_policy, NULL);
        }
        if (extent.width != (uint32_t)fb_w || extent.height != (uint32_t)fb_h) {
            fprintf(stderr,
                "[Vulkan] swapchain extent %ux%u != GLFW framebuffer %dx%d (text/VD scale will differ from OpenGL until they match)\n",
                extent.width, extent.height, fb_w, fb_h);
            fflush(stderr);
        }
    }

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = sit_render.vk.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    _SituationQueueFamilyIndices indices = _SituationVulkanFindQueueFamilies(sit_render.vk.physical_device, sit_render.vk.surface);
    uint32_t queueFamilyIndices[] = {indices.graphics_family, indices.present_family};
    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    create_info.preTransform = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = old_swapchain;

    if (vkCreateSwapchainKHR(sit_render.vk.device, &create_info, NULL, &sit_render.vk.swapchain) != VK_SUCCESS) {
        sit_render.vk.swapchain_valid = false;
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED, "Failed to create swap chain");
        _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support);
        return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED;
    }

    vkGetSwapchainImagesKHR(sit_render.vk.device, sit_render.vk.swapchain, &image_count, NULL);
    sit_render.vk.swapchain_images = (VkImage*)SIT_MALLOC(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(sit_render.vk.device, sit_render.vk.swapchain, &image_count, sit_render.vk.swapchain_images);
    sit_render.vk.swapchain_image_format = surface_format.format;
    sit_render.vk.swapchain_extent = extent;
    sit_render.vk.swapchain_image_count = image_count;

    if (sit_render.output_hdr_active) {
        fprintf(stderr,
            "[Vulkan] HDR10 swapchain active: fmt=%u + HDR10_ST2084 (color_space=%u, present=FIFO)\n",
            (unsigned)sit_render.vk.swapchain_image_format,
            (unsigned)sit_render.vk.swapchain_color_space);
        fflush(stderr);
    }

    _SituationVulkanSyncMainWindowFromGLFW();

    _SituationVulkanFreeSwapchainSupportDetails(&swapchain_support);
    sit_render.vk.swapchain_valid = true;
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates a VkImageView for each image in the swapchain.
 * @details An image view is a mandatory component that describes how to access a `VkImage` and which parts of it are accessible. This function creates a view for each swapchain image, specifying that they will be used as 2D color textures.
 *          The created image views are essential for binding the swapchain images as render targets in a framebuffer.
 *
 * @return SITUATION_SUCCESS if all image views are created successfully.
 * @return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED if any of the `vkCreateImageView` calls fail.
 *
 * @note This function must be called after `_SituationVulkanCreateSwapchain` has successfully retrieved the swapchain images. The created handles are stored in the `sit_render.vk.swapchain_image_views` array.
 * @warning This function is for internal use by `_SituationInitVulkan` and `_SituationVulkanRecreateSwapchain`.
 *
 * @see _SituationVulkanCreateSwapchain(), _SituationVulkanCreateImageView()
 */
static SituationError _SituationVulkanCreateImageViews(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating image views...\n"); fflush(stdout);
    #endif
sit_render.vk.swapchain_image_views = (VkImageView*)SIT_MALLOC(sizeof(VkImageView) * sit_render.vk.swapchain_image_count);
    for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
        sit_render.vk.swapchain_image_views[i] = _SituationVulkanCreateImageView(sit_render.vk.swapchain_images[i], sit_render.vk.swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
        if(sit_render.vk.swapchain_image_views[i] == VK_NULL_HANDLE) {
             _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED, "Failed to create image views");
             return SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED;
        }
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Creates the main render pass for the application window.
 * @details This function defines the structure of a render pass, specifying the attachments (color and depth), their properties, and the dependencies between subpasses. The main render pass created here is configured for standard forward rendering:
 *          - It uses one color attachment, which will be cleared at the start of the pass and stored for presentation at the end. Its layout is transitioned from `UNDEFINED` to `PRESENT_SRC_KHR`.
 *          - It uses one depth/stencil attachment, which will be cleared at the start and its contents discarded at the end.
 *          - It contains a single subpass that uses these attachments for graphics operations.
 *          - It includes a subpass dependency to ensure that color attachment operations in one frame are complete before the next frame's rendering begins.
 *
 * The resulting `VkRenderPass` is compatible with the framebuffers created for the swapchain.
 *
 * @return SITUATION_SUCCESS on successful creation of the render pass.
 * @return SITUATION_ERROR_VULKAN_UNSUPPORTED if no suitable depth format can be found on the physical device.
 * @return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED if the `vkCreateRenderPass` call fails.
 *
 * @note This function must be called after the logical device has been created and the swapchain format has been determined.
 * @warning This function is for internal use by `_SituationInitVulkan` only.
 *
 * @see _SituationVulkanFindSupportedFormat(), _SituationVulkanCreateFramebuffers()
 */
static SituationError _SituationVulkanCreateRenderPass(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating render pass...\n"); fflush(stdout);
    #endif
sit_render.vk.depth_format = _SituationVulkanFindSupportedFormat(
        (VkFormat[]){VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 3,
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
    if (sit_render.vk.depth_format == VK_FORMAT_UNDEFINED) {
         _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_UNSUPPORTED, "Failed to find a supported depth format");
         return SITUATION_ERROR_VULKAN_UNSUPPORTED;
    }

    VkAttachmentDescription color_attachment = {};
    color_attachment.format = sit_render.vk.swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment = {};
    depth_attachment.format = sit_render.vk.depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_attachment_ref = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};
    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(sit_render.vk.device, &render_pass_info, NULL, &sit_render.vk.main_window_render_pass) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED, "Failed to create render pass");
        return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED;
    }

#if SIT_VK_SHADER_CACHE_PHASE2
    _SitVkShaderCacheOnMainRenderPassCreated();
#endif

    sit_render.vk.main_window_render_pass_resume = VK_NULL_HANDLE;
    {
        /* Resume pass: color LOAD from PRESENT_SRC (after a prior EndRenderPass left the swapchain
         * ready for present). The default pass uses CLEAR on every Begin — SituationRenderVirtualDisplays
         * restarts the main-window pass after compositing; CLEAR would erase the VD draws (harness vd_*). */
        VkAttachmentDescription color_resume = color_attachment;
        color_resume.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color_resume.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depth_resume = depth_attachment;
        depth_resume.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_resume.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        VkAttachmentDescription attachments_resume[] = { color_resume, depth_resume };
        VkRenderPassCreateInfo rp_resume_info = render_pass_info;
        rp_resume_info.pAttachments = attachments_resume;

        if (vkCreateRenderPass(sit_render.vk.device, &rp_resume_info, NULL, &sit_render.vk.main_window_render_pass_resume) != VK_SUCCESS) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.main_window_render_pass, NULL);
            sit_render.vk.main_window_render_pass = VK_NULL_HANDLE;
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED, "Failed to create main-window resume render pass (LOAD color)");
            return SITUATION_ERROR_VULKAN_RENDERPASS_FAILED;
        }
    }
    return SITUATION_SUCCESS;
}

static void _SituationVulkanDestroyMainWindowRenderPass(void) {
    if (sit_render.vk.device == VK_NULL_HANDLE) {
        return;
    }
    if (sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE) {
        vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.main_window_render_pass_resume, NULL);
        sit_render.vk.main_window_render_pass_resume = VK_NULL_HANDLE;
    }
    if (sit_render.vk.main_window_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.main_window_render_pass, NULL);
        sit_render.vk.main_window_render_pass = VK_NULL_HANDLE;
    }
}

static SituationError _SituationVulkanRecreateMainWindowRenderPass(void) {
    _SituationVulkanDestroyMainWindowRenderPass();
    return _SituationVulkanCreateRenderPass();
}

/**
 * @brief [INTERNAL] Creates the depth buffer image and its view for the main render pass.
 * @details This function allocates a `VkImage` and its corresponding `VkImageView` to be used as the depth/stencil attachment for the main window's framebuffers. The image's dimensions are matched to the swapchain extent.
 *          The image is created in optimal device-local memory (`VMA_MEMORY_USAGE_GPU_ONLY`) for maximum performance.
 *
 * @return SITUATION_SUCCESS on successful creation of the depth image and its view.
 * @return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED if the `vmaCreateImage` call fails.
 * @return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED if the `vkCreateImageView` call fails.
 *
 * @note This function must be called after the swapchain extent and a suitable depth format have been determined.
 * @warning This function is for internal use by `_SituationInitVulkan` and `_SituationVulkanRecreateSwapchain`.
 *
 * @see _SituationVulkanCreateImage(), _SituationVulkanCreateImageView(), _SituationVulkanCreateFramebuffers()
 */
static SituationError _SituationVulkanCreateDepthResources(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating depth resources...\n"); fflush(stdout);
    #endif
if (_SituationVulkanCreateImage(sit_render.vk.swapchain_extent.width, sit_render.vk.swapchain_extent.height, 1, sit_render.vk.depth_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_SAMPLE_COUNT_1_BIT,
                                  &sit_render.vk.depth_image, &sit_render.vk.depth_image_memory) != SITUATION_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED, "Failed to create depth image");
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED;
    }
    sit_render.vk.depth_image_view = _SituationVulkanCreateImageView(sit_render.vk.depth_image, sit_render.vk.depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);
    if(sit_render.vk.depth_image_view == VK_NULL_HANDLE){
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, "Failed to create depth image view");
        return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
    }
    return SITUATION_SUCCESS;
}

// --- Framebuffer Creation ---
/**
 * @brief [INTERNAL] Creates Vulkan framebuffers for the main window swapchain.
 *
 * @details This helper function is responsible for creating the `VkFramebuffer` objects that connect the swapchain images (via their image views) and the depth buffer image view to the main window's render pass.
 *          A framebuffer defines the attachments (color, depth, stencil) that will be used in a render pass instance.
 *          This function is typically called during Vulkan initialization (`_SituationInitVulkan`) after the swapchain, image views, depth resources, and main render pass have been successfully created.
 *          It allocates an array to hold the framebuffer handles, then iterates through each swapchain image view, creating a corresponding framebuffer that uses that image view as the color attachment and the shared depth image view as the depth attachment.
 *
 * @return SITUATION_SUCCESS on successful creation of all framebuffers.
 * @return SITUATION_ERROR_MEMORY_ALLOCATION if memory allocation fails for the internal array of `VkFramebuffer` handles.
 * @return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED if `vkCreateFramebuffer`fails for any of the framebuffers. A specific error message is set.
 *         Any successfully created framebuffers *before* the failure point will be left in the `sit_render.vk.main_window_framebuffers` array and must be cleaned up by the caller (e.g., `_SituationVulkanCleanupSwapchain` or `_SituationCleanupVulkan`) to prevent leaks.
 *
 * @note This function requires that the following Vulkan resources are already created and valid:
 *       - `sit_render.vk.device`
 *       - `sit_render.vk.swapchain_image_views` (array of image views)
 *       - `sit_render.vk.depth_image_view`
 *       - `sit_render.vk.main_window_render_pass`
 *       - `sit_render.vk.swapchain_extent`
 *       - `sit_render.vk.swapchain_image_count`
 * @note The created array of `VkFramebuffer` handles is stored in `sit_render.vk.main_window_framebuffers`. This array must be freed later by the cleanup process.
 * @warning This function is for internal use by the Vulkan initialization and swapchain recreation processes and should not be called directly by user code.
 *
 * @see _SituationInitVulkan(), _SituationVulkanRecreateSwapchain(), _SituationVulkanCleanupSwapchain(), vkCreateFramebuffer()
 */

static VkRenderPass _SituationVulkanGetOrCreateRenderPass(_SituationVulkanState* vk_state, const SituationRenderPassInfo* info) {
    bool is_main_window = (info->display_id == -1);
    uint32_t key = _SituationHashRenderPassKey(info, is_main_window);

    // 1. Check cache
    for (uint32_t i = 0; i < vk_state->render_pass_cache_count; ++i) {
        if (vk_state->render_pass_cache[i].key == key) {
            return vk_state->render_pass_cache[i].handle;
        }
    }

    // 2. Cache miss, create new RenderPass
    VkAttachmentDescription attachments[3] = {0};
    uint32_t attachment_count = 0;

    // --- Color Attachment ---
    attachments[0].format = is_main_window ? vk_state->swapchain_image_format : VK_FORMAT_R8G8B8A8_UNORM;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT; // MSAA not explicitly handled in this snippet yet

    // Color Load Op
    if (info->color_attachment.loadOp == SIT_LOAD_OP_CLEAR || info->color_attachment.loadOp == SIT_LOAD_OP_DONT_CARE) {
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    } else { // SIT_LOAD_OP_LOAD
        attachments[0].initialLayout = is_main_window ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // Mapping our simple load ops to Vulkan
    attachments[0].loadOp = (info->color_attachment.loadOp == SIT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                            (info->color_attachment.loadOp == SIT_LOAD_OP_LOAD)  ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                                                   VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    attachments[0].storeOp = (info->color_attachment.storeOp == SIT_STORE_OP_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                                                                                      VK_ATTACHMENT_STORE_OP_DONT_CARE;

    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Final layout
    attachments[0].finalLayout = is_main_window ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference color_attachment_ref = {0};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment_count++;

    // --- Depth/Stencil Attachment ---
    VkAttachmentReference depth_attachment_ref = {0};
    bool has_depth = true; // Typically we assume depth exists, or we could check if display_id has depth. For now, we assume all render passes have a depth attachment.

    if (has_depth) {
        attachments[1].format = vk_state->depth_format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;

        // Depth Load Op
        if (info->depth_attachment.loadOp == SIT_LOAD_OP_CLEAR || info->depth_attachment.loadOp == SIT_LOAD_OP_DONT_CARE) {
            attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        } else { // SIT_LOAD_OP_LOAD
            attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        attachments[1].loadOp = (info->depth_attachment.loadOp == SIT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                (info->depth_attachment.loadOp == SIT_LOAD_OP_LOAD)  ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                                                       VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        attachments[1].storeOp = (info->depth_attachment.storeOp == SIT_STORE_OP_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                                                                                          VK_ATTACHMENT_STORE_OP_DONT_CARE;

        // Stencil Load Op
        attachments[1].stencilLoadOp = (info->stencil_attachment.loadOp == SIT_LOAD_OP_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                       (info->stencil_attachment.loadOp == SIT_LOAD_OP_LOAD)  ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE;

        attachments[1].stencilStoreOp = (info->stencil_attachment.storeOp == SIT_STORE_OP_STORE) ? VK_ATTACHMENT_STORE_OP_STORE :
                                                                                                   VK_ATTACHMENT_STORE_OP_DONT_CARE;

        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachment_count++;
    }

    // --- Subpass ---
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    if (has_depth) {
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
    }

    // --- Subpass Dependencies ---
    VkSubpassDependency dependencies[2] = {0};
    uint32_t dependency_count = 0;

    // External to Subpass 0 (Color)
    dependencies[dependency_count].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[dependency_count].dstSubpass = 0;
    dependencies[dependency_count].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[dependency_count].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[dependency_count].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[dependency_count].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency_count++;

    // External to Subpass 0 (Depth/Stencil)
    if (has_depth) {
        dependencies[dependency_count].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[dependency_count].dstSubpass = 0;
        dependencies[dependency_count].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[dependency_count].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[dependency_count].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[dependency_count].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency_count++;
    }

    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = attachment_count;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = dependency_count;
    render_pass_info.pDependencies = dependencies;

    VkRenderPass new_render_pass = VK_NULL_HANDLE;
    VkResult res = vkCreateRenderPass(vk_state->device, &render_pass_info, NULL, &new_render_pass);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateRenderPass failed for dynamic cache! (Result: %d)\n", res);
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED, "vkCreateRenderPass failed for dynamic render pass cache");
        return VK_NULL_HANDLE;
    }

    // 3. Store in cache
    if (vk_state->render_pass_cache_count < 32) {
        vk_state->render_pass_cache[vk_state->render_pass_cache_count].key = key;
        vk_state->render_pass_cache[vk_state->render_pass_cache_count].handle = new_render_pass;
        vk_state->render_pass_cache_count++;
    } else {
        fprintf(stderr, "WARNING: Render Pass Cache full! (32 max). Returning un-cached pass, likely leaking.\n");
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_RENDER_PASS_CACHE_FULL,
            "Render pass cache full (32 max); un-cached render pass returned. Likely leaking.");
    }

    return new_render_pass;
}

static SituationError _SituationVulkanCreateFramebuffers(void) {

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating framebuffers...\n"); fflush(stdout);
    #endif
// --- 1. Allocate Array for Framebuffer Handles ---
    // Allocate memory for the array that will hold the VkFramebuffer handles.
    // The number of framebuffers needed equals the number of swapchain images.
    sit_render.vk.main_window_framebuffers = (VkFramebuffer*)SIT_MALLOC(sizeof(VkFramebuffer) * sit_render.vk.swapchain_image_count);

    // Check if the memory allocation for the framebuffer array was successful.
    if (!sit_render.vk.main_window_framebuffers) {
        // Allocation failed. This is a critical error for this step.
        _SituationSetErrorFromCode(
            SITUATION_ERROR_MEMORY_ALLOCATION,
            "_SituationVulkanCreateFramebuffers: Failed to allocate memory for framebuffer handle array."
        );
        // No Vulkan objects have been created yet in this function, so no cleanup is needed here.
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }

    // --- 2. Create Framebuffers for Each Swapchain Image ---
    // Iterate through each swapchain image view to create its corresponding framebuffer.
    for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
        // --- 2a. Define Framebuffer Attachments ---
        // Specify the attachments for this framebuffer:
        // 1. The swapchain image view (color attachment)
        // 2. The shared depth image view (depth attachment)
        VkImageView attachments[] = {
            sit_render.vk.swapchain_image_views[i], // Color attachment (index 0)
            sit_render.vk.depth_image_view          // Depth attachment (index 1)
        };

        // --- 2b. Configure Framebuffer Creation Info ---
        // Set up the VkFramebufferCreateInfo struct with the necessary parameters.
        VkFramebufferCreateInfo framebuffer_info = {}; // Explicitly zero-initialize
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; // Mandatory sType
        framebuffer_info.pNext = NULL; // No extension structures
        framebuffer_info.flags = 0; // No special flags for framebuffer creation
        framebuffer_info.renderPass = sit_render.vk.main_window_render_pass; // Link to the render pass
        framebuffer_info.attachmentCount = 2; // We have two attachments (color and depth)
        framebuffer_info.pAttachments = attachments; // Pointer to the attachments array
        // Set the dimensions of the framebuffer to match the swapchain extent.
        framebuffer_info.width = sit_render.vk.swapchain_extent.width;
        framebuffer_info.height = sit_render.vk.swapchain_extent.height;
        framebuffer_info.layers = 1; // Number of layers (for array textures or VR, usually 1)

        // --- 2c. Create the VkFramebuffer Object ---
        // Call the Vulkan API to create the framebuffer object.
        VkResult result = vkCreateFramebuffer(
            sit_render.vk.device,           // The logical device
            &framebuffer_info,          // Creation parameters
            NULL,                       // Optional allocation callbacks (use default)
            &sit_render.vk.main_window_framebuffers[i] // Output: the created VkFramebuffer handle
        );

        // --- 2d. Handle Creation Result ---
        if (result != VK_SUCCESS) {
            // vkCreateFramebuffer failed for the framebuffer at index `i`.
            // This is a critical failure for the initialization process.
            char error_detail[256];
            snprintf(
                error_detail,
                sizeof(error_detail),
                "_SituationVulkanCreateFramebuffers: vkCreateFramebuffer failed for swapchain image %u (VkResult: 0x%x).",
                i,
                result
            );
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, error_detail);

            // --- 2e. Critical: Handle Partial Success and Cleanup ---
            // If framebuffer creation fails at index `i`, it means framebuffers from index 0 to `i-1` *might* have been successfully created and stored in `sit_render.vk.main_window_framebuffers[0]` to `[i-1]`.
            //
            // It is the responsibility of the *caller* of this function (e.g., _SituationInitVulkan, _SituationVulkanRecreateSwapchain) to perform a full cleanup (e.g., by calling _SituationVulkanCleanupSwapchain) when any error is returned.
            // That cleanup process will iterate through the `sit_render.vk.main_window_framebuffers` array and destroy any non-VK_NULL_HANDLE entries, then free the array itself.
            //
            // This function does *not* attempt to destroy the potentially successfully created framebuffers here. It simply reports the error and returns. This simplifies error handling in this function and relies on the robustness of the overall Vulkan cleanup sequence.
            //
            // Note: The `sit_render.vk.main_window_framebuffers` array itself is left allocated but partially populated. The cleanup function must handle this state correctly.

            // Return the specific error code to signal failure to the caller.
            return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
        }

        // If we reach here, the framebuffer at index `i` was created successfully.
        // Its handle is stored in `sit_render.vk.main_window_framebuffers[i]`.
        // The loop will continue to create the remaining framebuffers.
    }

    // --- 3. Success ---
        // If the loop completes without returning an error, all framebuffers have been successfully created and their handles are stored in the `sit_render.vk.main_window_framebuffers` array.
    // The next step in Vulkan initialization is typically creating command buffers or synchronization objects.

    sit_render.vk.main_window_framebuffers_resume = NULL;
    if (sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE) {
        sit_render.vk.main_window_framebuffers_resume = (VkFramebuffer*)SIT_CALLOC(sit_render.vk.swapchain_image_count, sizeof(VkFramebuffer));
        if (!sit_render.vk.main_window_framebuffers_resume) {
            for (uint32_t k = 0; k < sit_render.vk.swapchain_image_count; k++) {
                if (sit_render.vk.main_window_framebuffers[k] != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers[k], NULL);
                    sit_render.vk.main_window_framebuffers[k] = VK_NULL_HANDLE;
                }
            }
            SIT_FREE(sit_render.vk.main_window_framebuffers);
            sit_render.vk.main_window_framebuffers = NULL;
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "_SituationVulkanCreateFramebuffers: resume framebuffer array alloc failed.");
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }
        for (uint32_t j = 0; j < sit_render.vk.swapchain_image_count; j++) {
            VkImageView attachments_r[] = {
                sit_render.vk.swapchain_image_views[j],
                sit_render.vk.depth_image_view
            };
            VkFramebufferCreateInfo fb_resume = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fb_resume.renderPass = sit_render.vk.main_window_render_pass_resume;
            fb_resume.attachmentCount = 2;
            fb_resume.pAttachments = attachments_r;
            fb_resume.width = sit_render.vk.swapchain_extent.width;
            fb_resume.height = sit_render.vk.swapchain_extent.height;
            fb_resume.layers = 1;
            VkResult rr = vkCreateFramebuffer(sit_render.vk.device, &fb_resume, NULL, &sit_render.vk.main_window_framebuffers_resume[j]);
            if (rr != VK_SUCCESS) {
                for (uint32_t k = 0; k < j; k++) {
                    if (sit_render.vk.main_window_framebuffers_resume[k] != VK_NULL_HANDLE) {
                        vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers_resume[k], NULL);
                    }
                }
                SIT_FREE(sit_render.vk.main_window_framebuffers_resume);
                sit_render.vk.main_window_framebuffers_resume = NULL;
                for (uint32_t k = 0; k < sit_render.vk.swapchain_image_count; k++) {
                    if (sit_render.vk.main_window_framebuffers[k] != VK_NULL_HANDLE) {
                        vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers[k], NULL);
                        sit_render.vk.main_window_framebuffers[k] = VK_NULL_HANDLE;
                    }
                }
                SIT_FREE(sit_render.vk.main_window_framebuffers);
                sit_render.vk.main_window_framebuffers = NULL;
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED, "_SituationVulkanCreateFramebuffers: vkCreateFramebuffer (resume) failed.");
                return SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED;
            }
        }
    }

    return SITUATION_SUCCESS;
}

// --- Command Pool Creation ---
/**
 * @brief [INTERNAL] Creates the primary Vulkan command pool.
 *
 * @details This helper function is responsible for creating the main `VkCommandPool` used by the Situation library for allocating command buffers.
 *          This pool is specifically created for the graphics queue family, as all recorded commands (graphics, compute, transfer) in `situation.h` are submitted to the graphics queue.
 *          The pool is created with the `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` flag. This flag is essential because it allows individual command buffers allocated from this pool to be reset using `vkResetCommandBuffer`.
 *          This is necessary for the library's command buffer model, where a single command buffer (e.g., `sit_render.vk.command_buffers[frame_index]`) is reset and re-recorded every frame.
 *
 * @return SITUATION_SUCCESS on successful creation of the command pool.
 * @return SITUATION_ERROR_INVALID_PARAM if the Vulkan device (`sit_render.vk.device`) is `VK_NULL_HANDLE` or if the graphics queue family index (`sit_render.vk.graphics_family_index`) is invalid (e.g., `UINT32_MAX`).
 * @return SITUATION_ERROR_VULKAN_COMMAND_FAILED if `vkCreateCommandPool` fails to create the pool. This can happen due to invalid device handle, invalid queue family index, or driver issues. A specific error message is set.
 *
 * @note This function must be called after the Vulkan logical device (`sit_render.vk.device`) and the graphics queue family index (`sit_render.vk.graphics_family_index`) have been successfully determined (e.g., in `_SituationVulkanCreateLogicalDevice`).
 * @note The created `VkCommandPool` handle is stored in `sit_render.vk.command_pool`.
 * @note This command pool is used by `_SituationVulkanCreateCommandBuffers` to allocate the per-frame command buffers.
 * @warning This function is for internal use by the Vulkan initialization process (`_SituationInitVulkan`) and should not be called directly by user code.
 *
 * @see _SituationInitVulkan(), _SituationVulkanCreateLogicalDevice(), _SituationVulkanCreateCommandBuffers(), vkCreateCommandPool()
 */
static SituationError _SituationVulkanCreateCommandPool(void) {
    // --- 1. Input Validation (Defensive for internal helper) ---
    // Check if the prerequisite Vulkan device handle is valid.
    if (sit_render.vk.device == VK_NULL_HANDLE) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateCommandPool: Vulkan device is NULL. Call _SituationVulkanCreateLogicalDevice first." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // Check if the graphics queue family index is valid.
    // UINT32_MAX is often used as an "unset" value.
    if (sit_render.vk.graphics_family_index == UINT32_MAX) {
        _SituationSetErrorFromCode( SITUATION_ERROR_INVALID_PARAM, "_SituationVulkanCreateCommandPool: Graphics queue family index is invalid (UINT32_MAX). Ensure _SituationVulkanPickPhysicalDevice/_SituationVulkanCreateLogicalDevice ran successfully." );
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // --- 2. Configure Command Pool Creation Info ---
    // Set up the VkCommandPoolCreateInfo struct with the necessary parameters.
    VkCommandPoolCreateInfo pool_info = {}; // Explicitly zero-initialize
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; // Mandatory sType
    pool_info.pNext = NULL; // No extension structures
    // --- CRITICAL FLAG ---
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT allows command buffers allocated from this pool to be individually reset using vkResetCommandBuffer.
    // This is essential for the library's per-frame command buffer recording model.
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    // --- END CRITICAL FLAG ---
    // Specify the queue family that command buffers from this pool will be submitted to.
    // All library commands go to the graphics queue.
    pool_info.queueFamilyIndex = sit_render.vk.graphics_family_index;

    // --- 3. Create the VkCommandPool ---
    // This is the actual Vulkan API call that creates the command pool object.
    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Calling vkCreateCommandPool...\n"); fflush(stdout);
    #endif
    VkResult result = vkCreateCommandPool(
        sit_render.vk.device,       // The logical device the pool is associated with
        &pool_info,             // Creation parameters
        NULL,                   // Optional allocation callbacks (use default)
        &sit_render.vk.command_pool // Output: the created VkCommandPool handle
    );

    // --- 4. Handle Creation Result ---
    if (result != VK_SUCCESS) {
        // vkCreateCommandPool failed. This is a critical error for Vulkan setup.
        // Common reasons include:
        // - Invalid device handle (sit_render.vk.device)
        // - Invalid queue family index (sit_render.vk.graphics_family_index)
        // - Driver issues or resource exhaustion.
        char error_detail[256];
        snprintf(
            error_detail,
            sizeof(error_detail),
            "_SituationVulkanCreateCommandPool failed: vkCreateCommandPool returned VkResult 0x%x. Check device/queue family validity or driver state.",
            result
        );
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, error_detail);
        // Ensure the global command pool handle is explicitly invalid on failure.
        sit_render.vk.command_pool = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Command pool created successfully\n"); fflush(stdout);
    #endif
    // [v2.3.23] Create separate pool for Compute
    pool_info.queueFamilyIndex = sit_render.vk.compute_family_index;
    if (vkCreateCommandPool(sit_render.vk.device, &pool_info, NULL, &sit_render.vk.compute_command_pool) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to create compute command pool.");
        sit_render.vk.compute_command_pool = VK_NULL_HANDLE;
        return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
    }

    // --- 5. Success ---
    // If we reach here, the VkCommandPool was created successfully.
    // The handle is stored in sit_render.vk.command_pool.
    // The next step in Vulkan initialization is typically allocating command buffers from this pool using _SituationVulkanCreateCommandBuffers.
    return SITUATION_SUCCESS;
}

// --- Command Buffer Creation ---
/**
 * @brief [INTERNAL] Allocates the primary command buffers for each in-flight frame.
 * @details This function allocates a dedicated, primary-level `VkCommandBuffer` for each frame that can be processed concurrently (determined by `sit_render.vk.max_frames_in_flight`).
 *          These command buffers are long-lived; one is used for each frame in a round-robin fashion. At the beginning of a frame, the corresponding command buffer is reset and then used to record all rendering and compute commands for that frame.
 *          They are allocated from the library's main command pool, which is created with the `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` flag to allow this per-frame reset behavior.
 *
 * @return `SITUATION_SUCCESS` if all command buffers are allocated successfully.
 * @return `SITUATION_ERROR_VULKAN_COMMAND_FAILED` if the `vkAllocateCommandBuffers` call fails.
 *
 * @note This function must be called after the logical device and the main command pool have been created. The allocated handles are stored in the `sit_render.vk.command_buffers` array.
 * @warning This function is for internal use by `_SituationInitVulkan` only.
 *
 * @see _SituationInitVulkan(), _SituationVulkanCreateCommandPool(), SituationGetMainCommandBuffer()
 */
static SituationError _SituationVulkanCreateCommandBuffers(void) {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = sit_render.vk.command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    // --- Use the dynamic value from the global state ---
    alloc_info.commandBufferCount = sit_render.vk.max_frames_in_flight;

    if (vkAllocateCommandBuffers(sit_render.vk.device, &alloc_info, sit_render.vk.command_buffers) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to allocate command buffers");
        return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
    }

    // Allocate Compute Buffers (from separate pool)
    alloc_info.commandPool = sit_render.vk.compute_command_pool;
    if (vkAllocateCommandBuffers(sit_render.vk.device, &alloc_info, sit_render.vk.compute_command_buffers) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Failed to allocate compute command buffers");
        return SITUATION_ERROR_VULKAN_COMMAND_FAILED;
    }

    return SITUATION_SUCCESS;
}

// --- Sync Object Creation ---
/**
 * @brief [INTERNAL] Creates the synchronization objects (semaphores and fences) for each in-flight frame.
 * @details This function creates the Vulkan synchronization primitives required to manage the render loop and ensure correct ordering between the CPU and GPU, as well as between different GPU operations.
 *          For each frame that can be "in-flight" simultaneously (determined by `sit_render.vk.max_frames_in_flight`), this function creates:
 *   - **An `image_available_semaphore`:** This semaphore is signaled by `vkAcquireNextImageKHR` when a swapchain image is ready to be rendered to. The command buffer submission will wait on this semaphore.
 *   - **A `render_finished_semaphore`:** This semaphore is signaled by the `vkQueueSubmit` call when the command buffer has finished execution. The presentation engine will wait on this semaphore before showing the image on screen.
 *   - **An `in_flight_fence`:** This fence is signaled by `vkQueueSubmit` and is used by the CPU (`vkWaitForFences`) to wait until the frame has completely finished rendering.
 *       This prevents the CPU from starting to record commands for a new frame `N` before frame `N-max_frames_in_flight` has finished.
 *
 * The fences are created in the **signaled state** (`VK_FENCE_CREATE_SIGNALED_BIT`) to ensure that the very first frame doesn't block indefinitely waiting for a fence that has never been submitted.
 *
 * @return `SITUATION_SUCCESS` if all semaphores and fences for all in-flight frames are created successfully.
 * @return `SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED` if any `vkCreateSemaphore` or `vkCreateFence` call fails.
 *
 * @note This function must be called after the logical device has been created and `max_frames_in_flight` has been determined.
 * @warning This function is for internal use by `_SituationInitVulkan` only.
 *
 * @see _SituationInitVulkan(), SituationAcquireFrameCommandBuffer(), SituationEndFrame()
 */
static SituationError _SituationVulkanCreateSyncObjects(void) {
    VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Creating sync objects for %u frames...\n", sit_render.vk.max_frames_in_flight);
    printf("Situation [Vulkan Debug]:   Fence create flags: 0x%x (SIGNALED_BIT=0x%x)\n",
           fence_info.flags, VK_FENCE_CREATE_SIGNALED_BIT);
    fflush(stdout);
    #endif

    // --- Loop using the dynamic value from the global state ---
    for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
        if (vkCreateSemaphore(sit_render.vk.device, &semaphore_info, NULL, &sit_render.vk.image_available_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(sit_render.vk.device, &semaphore_info, NULL, &sit_render.vk.render_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(sit_render.vk.device, &semaphore_info, NULL, &sit_render.vk.compute_finished_semaphores[i]) != VK_SUCCESS ||
            vkCreateFence(sit_render.vk.device, &fence_info, NULL, &sit_render.vk.in_flight_fences[i]) != VK_SUCCESS) {
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED, "Failed to create synchronization objects for a frame");
            return SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED;
        }

        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   Frame %u: fence=%p\n", i, (void*)sit_render.vk.in_flight_fences[i]);
        fflush(stdout);
        #endif
    }

    #ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]: Sync objects created successfully\n");
    fflush(stdout);
    #endif

    return SITUATION_SUCCESS;
}

// --- Utility Helpers ---

/**
 * @brief [INTERNAL] Finds the first supported Vulkan format from a list that supports specific tiling and usage features.
 *
 * @details This helper function is used during Vulkan initialization to find suitable formats for images, such as depth buffers, that meet the required criteria.
 *          It queries the Vulkan physical device for the properties of each candidate format and returns the first one that supports the specified tiling mode and feature flags.
 *
 * @param candidates An array of `VkFormat` enums to check for support.
 * @param candidate_count The number of elements in the `candidates` array.
 * @param tiling The desired image tiling mode (`VK_IMAGE_TILING_LINEAR` or `VK_IMAGE_TILING_OPTIMAL`).
 * @param features A bitmask of `VkFormatFeatureFlags` that the format must support (e.g., `VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT`).
 * @return The first `VkFormat` from the `candidates` array that is supported with the specified `tiling` and `features`.
 * @return `VK_FORMAT_UNDEFINED` if none of the `candidates` support the requested `tiling` and `features`.
 *
 * @note This function relies on `sit_render.vk.physical_device` being a valid handle to an enumerated physical device. This is guaranteed by the library's initialization sequence if this function is called.
 * @warning The order of formats in the `candidates` array is important.
 *          The function returns the *first* supported format found. Place preferred formats (e.g., higher precision) earlier in the list.
 *
 * @see _SituationVulkanCreateDepthResources()
 */
static VkFormat _SituationVulkanFindSupportedFormat(
    const VkFormat* candidates,
    uint32_t candidate_count,
    VkImageTiling tiling,
    VkFormatFeatureFlags features)
{
    // --- 1. Input Validation (Defensive for internal helper) ---
    // While internal, checking for null pointer or zero count prevents crashes if called incorrectly from within the library.
    if (!candidates || candidate_count == 0) {
        // Cannot find a format from an empty list.
        return VK_FORMAT_UNDEFINED;
    }

    // --- 2. Iterate Through Candidates ---
    for (uint32_t i = 0; i < candidate_count; i++) {
        VkFormat format = candidates[i];

        // --- 3. Query Format Properties ---
        // Get the properties supported by the physical device for this format.
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(sit_render.vk.physical_device, format, &props);

        // --- 4. Check for Required Features based on Tiling ---
        // Determine if the format supports the needed features for the requested tiling.
        VkFormatFeatureFlags supported_features = 0;
        if (tiling == VK_IMAGE_TILING_LINEAR) {
            supported_features = props.linearTilingFeatures;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL) {
            supported_features = props.optimalTilingFeatures;
        }
        // Note: Other tiling modes (e.g., VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) are not handled here and would result in supported_features = 0.

        // --- 5. Match Found? ---
        // Check if ALL requested features are present in the supported features.
        if ((supported_features & features) == features) {
            // Found a suitable format. Return it immediately.
            return format;
        }
        // If not, continue checking the next candidate.
    }

    // --- 6. No Suitable Format Found ---
    // If the loop completes without returning, no candidate format met the criteria.
    return VK_FORMAT_UNDEFINED;
}

/**
 * @brief [INTERNAL] Cleans up all Vulkan resources that are directly dependent on the current swapchain.
 *
 * @details This helper function is a crucial part of the swapchain recreation process. It ensures that all objects derived from the old swapchain are destroyed before a new swapchain can be created. This prevents resource leaks and potential validation errors.
 *
 * @details This function performs the following cleanup steps:
 *          1.  Waits for the device to be idle (`vkDeviceWaitIdle`) to ensure no commands are currently using the resources to be destroyed.
 *          2.  Destroys the main window's depth image view and the depth image itself (along with its VMA allocation).
 *          3.  Iterates through and destroys all main window framebuffers stored in `sit_render.vk.main_window_framebuffers`.
 *          4.  Frees the `sit_render.vk.main_window_framebuffers` array itself.
 *          5.  Iterates through and destroys all swapchain image views stored in `sit_render.vk.swapchain_image_views`.
 *          6.  Frees the `sit_render.vk.swapchain_image_views` array itself.
 *          7.  Destroys the `VkSwapchainKHR` handle (`sit_render.vk.swapchain`).
 *
 *          Crucially, it leaves core, swapchain-independent resources intact, such as:
 *          - The `VkDevice` (`sit_render.vk.device`)
 *          - The `VkPhysicalDevice` (`sit_render.vk.physical_device`)
 *          - The `VkRenderPass` (`sit_render.vk.main_window_render_pass`)
 *          - The `VkCommandPool` and command buffers
 *          - Descriptor sets, pools, and layouts
 *          - The `VkInstance` and `VkSurfaceKHR`
 *
 * @note This function should only be called when it's safe to destroy these resources, typically just before `_SituationVulkanCreateSwapchain` is called.
 * @note It is the caller's responsibility to ensure that:
 *       1. The Vulkan device (`sit_render.vk.device`) is valid.
 *       2. Any command buffers recording commands that use these resources have finished.
 *       3. This function is part of a coordinated swapchain recreation sequence.
 *
 * @see _SituationVulkanRecreateSwapchain(), _SituationVulkanCreateSwapchain(),
 *      _SituationVulkanCreateImageViews(), _SituationVulkanCreateDepthResources(),
 *      _SituationVulkanCreateFramebuffers()
 */
#if defined(SITUATION_USE_VULKAN)
static void _SituationVulkanDestroyScreenshotResources(void);
static SituationError _SituationVulkanEnsureScreenshotResources(uint32_t width, uint32_t height);
static void _SituationVulkanRecordScreenshotCopy(VkCommandBuffer cmd, VkImage swapchain_image, uint32_t width, uint32_t height);
static void _SituationVulkanResolveScreenshotAfterSubmit(uint32_t frame_index);
#endif
static SituationError _SituationVulkanCleanupSwapchain(void) {
    // --- 1. Validate Device Handle (Robustness) ---
    if (sit_render.vk.device == VK_NULL_HANDLE) {
        // Nothing to clean up if the device isn't created.
        // This can happen during partial init/cleanup.
        return SITUATION_SUCCESS;
    }

    // --- 2. Ensure GPU is Finished Using Resources ---
    // Bounded wait + message pump (vkDeviceWaitIdle wedges forever if the GPU hangs — frozen pale window).
    _SituationVulkanWaitInFlightFencesPump("_SituationVulkanCleanupSwapchain");
    sit_render.vk.swapchain_valid = false;

    _SituationVulkanDestroyScreenCopyResource();

    /* Do not tear down pre-present screenshot buffers here: vkQueuePresentKHR may trigger
     * swapchain recreate (OUT_OF_DATE/SUBOPTIMAL) in the same EndFrame after CPU screenshot
     * resolve — destroying here clears screenshot_valid before SituationLoadImageFromScreen.
     * Extent/format changes are handled by _SituationVulkanEnsureScreenshotResources on next use. */

    // --- Render Pass Cache Cleanup ---
    for (uint32_t i = 0; i < sit_render.vk.render_pass_cache_count; ++i) {
        if (sit_render.vk.render_pass_cache[i].handle != VK_NULL_HANDLE) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.render_pass_cache[i].handle, NULL);
        }
    }
    sit_render.vk.render_pass_cache_count = 0;

    // --- 3. Destroy Depth Resources ---
    // These are specific to the swapchain's extent/format.
    _SituationVulkanDestroyCanvasResources();
    if (sit_render.vk.depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.depth_image_view, NULL);
        sit_render.vk.depth_image_view = VK_NULL_HANDLE;
    }
    // Use the internal helper if one exists, otherwise call VMA directly.
    // Assuming _SituationVulkanDestroyImage helper exists and handles VMA destruction:
    if (sit_render.vk.depth_image != VK_NULL_HANDLE) {
        _SituationVulkanDestroyImage(sit_render.vk.depth_image, sit_render.vk.depth_image_memory);
        // Or directly: vmaDestroyImage(sit_render.vk.vma_allocator, sit_render.vk.depth_image, sit_render.vk.depth_image_memory);
        sit_render.vk.depth_image = VK_NULL_HANDLE;
        sit_render.vk.depth_image_memory = VK_NULL_HANDLE;
    }

    // --- 4. Destroy Main Window Framebuffers ---
    if (sit_render.vk.main_window_framebuffers_resume) {
        for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
            if (sit_render.vk.main_window_framebuffers_resume[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers_resume[i], NULL);
            }
        }
        SIT_FREE(sit_render.vk.main_window_framebuffers_resume);
        sit_render.vk.main_window_framebuffers_resume = NULL;
    }
    if (sit_render.vk.main_window_framebuffers) { // Check if array was allocated
        for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
            if (sit_render.vk.main_window_framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers[i], NULL);
                // Optional: Set to NULL for extra safety in debug builds if array might be reused
                // sit_render.vk.main_window_framebuffers[i] = VK_NULL_HANDLE;
            }
        }
        // Free the array holding the framebuffer handles.
        SIT_FREE(sit_render.vk.main_window_framebuffers);
        sit_render.vk.main_window_framebuffers = NULL; // Important: Nullify the pointer after freeing.
    }
    // Note: sit_render.vk.swapchain_image_count retains its value, as it's needed by _SituationVulkanCreateFramebuffers
    // which will be called next in the recreation sequence.

    // --- 5. Destroy Swapchain Image Views & Images ---
    if (sit_render.vk.swapchain_image_views) { // Check if array was allocated
        for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
            if (sit_render.vk.swapchain_image_views[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(sit_render.vk.device, sit_render.vk.swapchain_image_views[i], NULL);
                // Optional: Set to NULL for extra safety in debug builds
                // sit_render.vk.swapchain_image_views[i] = VK_NULL_HANDLE;
            }
        }
        // Free the array holding the image view handles.
        SIT_FREE(sit_render.vk.swapchain_image_views);
        sit_render.vk.swapchain_image_views = NULL; // Important: Nullify the pointer after freeing.
    }

    // Note: We do NOT destroy the VkImages themselves here, as they are owned
    // by the swapchain extension, but we must free our C array holding the handles.
    if (sit_render.vk.swapchain_images) {
        SIT_FREE(sit_render.vk.swapchain_images);
        sit_render.vk.swapchain_images = NULL;
    }

    // --- 6. Destroy the Swapchain Object ---
    if (sit_render.vk.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(sit_render.vk.device, sit_render.vk.swapchain, NULL);
        sit_render.vk.swapchain = VK_NULL_HANDLE;
    }
    // sit_render.vk.swapchain_image_count could be reset here, but it's often left
    // for the recreation process to potentially reuse if the new swapchain has the same count.
    // It's set correctly by _SituationVulkanCreateSwapchain.

    // HARDENING: best-effort teardown — vkDestroy* does not surface failures; always succeeds.
    return SITUATION_SUCCESS;
}


/**
 * @brief [INTERNAL] Recreates the Vulkan swapchain and all resources dependent on it.
 *
 * @details This function handles the full cycle of destroying the old swapchain and its associated resources, then creating a new swapchain and rebuilding the necessary dependent objects (image views, depth buffer, framebuffers).
 *          It is typically triggered by window resize events or when Vulkan reports that the swapchain is out of date (`VK_ERROR_OUT_OF_DATE_KHR`).
 *
 * @details The recreation process involves the following steps:
 *          1.  Waits for the window to have a non-zero size (handles minimization).
 *          2.  Calls `_SituationVulkanCleanupSwapchain` to destroy old resources.
 *          3.  Calls `_SituationVulkanCreateSwapchain` to create the new swapchain.
 *          4.  Calls `_SituationVulkanCreateImageViews` to create views for the new swapchain images.
 *          5.  Calls `_SituationVulkanCreateDepthResources` to create the depth buffer for the new extent.
 *          6.  Calls `_SituationVulkanCreateFramebuffers` to create framebuffers linking the new image views and depth buffer to the render pass.
 *
 * @note This function is designed to be called when the application detects a need for swapchain recreation (e.g., in `SituationEndFrame` or a resize callback).
 *       It internally handles the waiting and cleanup.
 * @warning If any step in the recreation process fails, the Vulkan backend may be left in an inconsistent state. The application should be prepared to handle such failures, potentially by shutting down or attempting recovery.
 *
 * @see _SituationVulkanCleanupSwapchain(), SituationEndFrame()
 */
static SituationError _SituationVulkanRecreateSwapchain(void) {
    // --- 1. Handle Window Minimization ---
    // If the window is minimized, width/height can be 0. We must wait for a valid size.
    int width = 0, height = 0;
    glfwGetFramebufferSize(sit_gs.sit_glfw_window, &width, &height);
    while (width == 0 || height == 0) {
        /* Timeout wake keeps the loop from blocking indefinitely on some drivers if events stall */
        _SituationWaitWindowEventsTimeoutGuarded(0.05);
        glfwGetFramebufferSize(sit_gs.sit_glfw_window, &width, &height);
    }

    // --- 2. Drain render thread queue ---
    // The render thread may have frames in-flight that reference the current swapchain.
    // Wait for it to finish all pending submissions/presents before touching state.
    #if !defined(__STDC_NO_THREADS__)
    if (sit_render.enabled) {
        mtx_lock(&sit_render.render_queue_mutex);
        while (sit_render.frames_pending > 0) {
            mtx_unlock(&sit_render.render_queue_mutex);
            _SituationPumpWindowEventsGuarded();
            thrd_yield();
            mtx_lock(&sit_render.render_queue_mutex);
        }
        mtx_unlock(&sit_render.render_queue_mutex);
    }
    #endif

    // --- 3. Wait for in-flight GPU work to complete ---
    // Batched: single vkWaitForFences on all fences simultaneously. Returns in one VSync
    // period (~16ms) rather than the old sequential approach (N * VSync periods).
    _SituationVulkanWaitInFlightFencesBatched();
    sit_render.vk.swapchain_valid = false;

    // --- 4. Destroy old swapchain-dependent resources ---
    _SituationVulkanDestroyScreenCopyResource();

    for (uint32_t i = 0; i < sit_render.vk.render_pass_cache_count; ++i) {
        if (sit_render.vk.render_pass_cache[i].handle != VK_NULL_HANDLE) {
            vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.render_pass_cache[i].handle, NULL);
        }
    }
    sit_render.vk.render_pass_cache_count = 0;

    _SituationVulkanDestroyCanvasResources();
    if (sit_render.vk.depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(sit_render.vk.device, sit_render.vk.depth_image_view, NULL);
        sit_render.vk.depth_image_view = VK_NULL_HANDLE;
    }
    if (sit_render.vk.depth_image != VK_NULL_HANDLE) {
        _SituationVulkanDestroyImage(sit_render.vk.depth_image, sit_render.vk.depth_image_memory);
        sit_render.vk.depth_image = VK_NULL_HANDLE;
        sit_render.vk.depth_image_memory = VK_NULL_HANDLE;
    }

    if (sit_render.vk.main_window_framebuffers_resume) {
        for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
            if (sit_render.vk.main_window_framebuffers_resume[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers_resume[i], NULL);
            }
        }
        SIT_FREE(sit_render.vk.main_window_framebuffers_resume);
        sit_render.vk.main_window_framebuffers_resume = NULL;
    }
    if (sit_render.vk.main_window_framebuffers) {
        for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
            if (sit_render.vk.main_window_framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(sit_render.vk.device, sit_render.vk.main_window_framebuffers[i], NULL);
            }
        }
        SIT_FREE(sit_render.vk.main_window_framebuffers);
        sit_render.vk.main_window_framebuffers = NULL;
    }

    if (sit_render.vk.swapchain_image_views) {
        for (uint32_t i = 0; i < sit_render.vk.swapchain_image_count; i++) {
            if (sit_render.vk.swapchain_image_views[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(sit_render.vk.device, sit_render.vk.swapchain_image_views[i], NULL);
            }
        }
        SIT_FREE(sit_render.vk.swapchain_image_views);
        sit_render.vk.swapchain_image_views = NULL;
    }
    if (sit_render.vk.swapchain_images) {
        SIT_FREE(sit_render.vk.swapchain_images);
        sit_render.vk.swapchain_images = NULL;
    }

    // --- 5. Destroy old swapchain ---
    if (sit_render.vk.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(sit_render.vk.device, sit_render.vk.swapchain, NULL);
        sit_render.vk.swapchain = VK_NULL_HANDLE;
    }

    // --- 6. Create new swapchain (clean, no oldSwapchain dependency) ---
    SituationError create_swapchain_result = _SituationVulkanCreateSwapchain(VK_NULL_HANDLE);
    if (create_swapchain_result != SITUATION_SUCCESS) {
        return _SituationSetErrorFromCode(create_swapchain_result, "_SituationVulkanRecreateSwapchain failed in _SituationVulkanCreateSwapchain.");
    }

    // --- 7. Recreate dependent resources ---
    SituationError recreate_rp_result = _SituationVulkanRecreateMainWindowRenderPass();
    if (recreate_rp_result != SITUATION_SUCCESS) {
        if (sit_render.vk.swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(sit_render.vk.device, sit_render.vk.swapchain, NULL);
            sit_render.vk.swapchain = VK_NULL_HANDLE;
        }
        return _SituationSetErrorFromCode(recreate_rp_result,
            "_SituationVulkanRecreateSwapchain failed in _SituationVulkanRecreateMainWindowRenderPass.");
    }

    SituationError create_views_result = _SituationVulkanCreateImageViews();
    if (create_views_result != SITUATION_SUCCESS) {
        if (sit_render.vk.swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(sit_render.vk.device, sit_render.vk.swapchain, NULL);
            sit_render.vk.swapchain = VK_NULL_HANDLE;
        }
        return _SituationSetErrorFromCode(create_views_result, "_SituationVulkanRecreateSwapchain failed in _SituationVulkanCreateImageViews.");
    }

    SituationError create_depth_result = _SituationVulkanCreateDepthResources();
    if (create_depth_result != SITUATION_SUCCESS) {
        _SituationVulkanCleanupSwapchain();
        return _SituationSetErrorFromCode(create_depth_result, "_SituationVulkanRecreateSwapchain failed in _SituationVulkanCreateDepthResources.");
    }

    SituationError create_framebuffers_result = _SituationVulkanCreateFramebuffers();
    if (create_framebuffers_result != SITUATION_SUCCESS) {
        _SituationVulkanCleanupSwapchain();
        return _SituationSetErrorFromCode(create_framebuffers_result, "_SituationVulkanRecreateSwapchain failed in _SituationVulkanCreateFramebuffers.");
    }

    SituationError screen_copy_result = _SituationVulkanCreateScreenCopyResource();
    if (screen_copy_result != SITUATION_SUCCESS) {
        _SituationVulkanCleanupSwapchain();
        return _SituationSetErrorFromCode(screen_copy_result,
            "_SituationVulkanRecreateSwapchain: screen copy recreation failed after swapchain rebuild.");
    }

    // --- 8. Success ---
    _SituationVulkanSyncMainWindowFromGLFW();

    // Reset frame timing so the recreation stall doesn't pollute dt/FPS measurements.
    sit_gs.previous_time = glfwGetTime();

    return SITUATION_SUCCESS;
}

#endif // SITUATION_USE_VULKAN

/**
 * @brief [INTERNAL] Initializes all backend-specific resources for the internal 2D quad renderer.
 * @details This function is a critical part of the main initialization sequence. It creates the dedicated shaders, pipeline objects, and vertex buffers required by the high-level `SituationCmdDrawQuad` and `SituationCmdDrawText` commands.
 *          It is designed to be completely self-contained, ensuring that its internal state does not interfere with the user's rendering state.
 *
 * @par Backend-Specific Implementation
 * - **OpenGL:**
 *   1.  Loads `sit/gpu/quad.vert` and `sit/gpu/quad.frag` from disk via `_SituationCreateGLCoreShaderProgram` and links the shader program.
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
 *   3.  Calls the generic `_SituationVulkanCreateGraphicsPipeline` helper to build the final `VkPipeline` object with the correct vertex input state and primitive topology (`TRIANGLE_STRIP`).
 *   4.  Creates and uploads the static vertex data to a device-local `VkBuffer` for optimal performance.
 *
 * @param width The initial width of the main window's viewport, used to configure the orthographic projection matrix.
 * @param height The initial height of the main window's viewport.
 *
 * @return `true` on successful initialization of all required resources.
 * @return `false` if any step fails (e.g., shader compilation, object creation). On failure, an appropriate error message is set, and any partially created resources are cleaned up.
 *
 * @note This function is for internal use by `_SituationInitOpenGL` or `_SituationInitVulkan` only.
 * @warning The success of this function is mandatory for `SituationCmdDrawQuad` and `SituationCmdDrawText` to work.
 *
 * @see _SituationCleanupQuadRenderer(), SituationCmdDrawQuad(), SituationCmdDrawText()
 */
static SituationError _SituationInitDefaultFont(void) {
    // 8x8 font bitmap from sit_default_8x8_font. 256 chars.
    // Layout: 16 chars per row, 16 rows.
    const int tex_w = 128;
    const int tex_h = 128;
    size_t data_size = tex_w * tex_h * 4; // RGBA
    uint8_t* pixels = (uint8_t*)SIT_CALLOC(1, data_size);

    if (!pixels) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Failed to allocate default font atlas.");
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
                pixels[p_idx + 0] = val;  // R: white for glyph, black for background
                pixels[p_idx + 1] = val;  // G: white for glyph, black for background
                pixels[p_idx + 2] = val;  // B: white for glyph, black for background
                pixels[p_idx + 3] = val;  // A: opaque for glyph, transparent for background
            }
        }
    }

    SituationImage img = {};
    img.width = tex_w;
    img.height = tex_h;
    img.channels = 4;
    img.data = pixels;
    SituationError tex_result = SituationCreateTexture(img, false, &sit_render.default_font_atlas);

    if (tex_result != SITUATION_SUCCESS) {
        SIT_FREE(pixels);
        return _SituationSetErrorFromCode(tex_result, "_SituationInitDefaultFont: Failed to create font atlas texture");
    }

    // Override filtering to NEAREST for pixel-perfect bitmap font rendering
    #if defined(SITUATION_USE_OPENGL)
    {
        _SituationTextureSlot* slot = _SitGetTextureSlot(sit_render.default_font_atlas);
        if (slot && slot->gl_texture_id) {
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_BASE_LEVEL, 0);
            glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_MAX_LEVEL, 0);
            slot->min_filter = SIT_TEXTURE_FILTER_NEAREST;
            slot->mag_filter = SIT_TEXTURE_FILTER_NEAREST;
        }
    }
    #endif

    // CRITICAL FIX: Font atlas needs text_sampler_layout (binding 0), not image_sampler_layout (binding 4)
    // Recreate the descriptor set with the correct layout
    #if defined(SITUATION_USE_VULKAN)
    _SituationTextureSlot* font_slot = _SitGetTextureSlot(sit_render.default_font_atlas);
    if (font_slot) {
        if (font_slot->sampler != VK_NULL_HANDLE) {
            _SituationDeferDestroyImage(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, font_slot->sampler);
            font_slot->sampler = VK_NULL_HANDLE;
        }

        VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;
        if (vkCreateSampler(sit_render.vk.device, &sampler_info, NULL, &font_slot->sampler) == VK_SUCCESS) {
            font_slot->min_filter = SIT_TEXTURE_FILTER_NEAREST;
            font_slot->mag_filter = SIT_TEXTURE_FILTER_NEAREST;
            font_slot->wrap_s = SIT_TEXTURE_WRAP_CLAMP_TO_EDGE;
            font_slot->wrap_t = SIT_TEXTURE_WRAP_CLAMP_TO_EDGE;

            if (sit_render.vk.global_bindless_set != VK_NULL_HANDLE) {
                VkDescriptorImageInfo bindless_image_info = {};
                bindless_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                bindless_image_info.imageView = font_slot->image_view;
                bindless_image_info.sampler = font_slot->sampler;

                VkWriteDescriptorSet bindless_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                bindless_write.dstSet = sit_render.vk.global_bindless_set;
                bindless_write.dstBinding = 0;
                bindless_write.dstArrayElement = (uint32_t)sit_render.default_font_atlas.slot_index;
                bindless_write.descriptorCount = 1;
                bindless_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindless_write.pImageInfo = &bindless_image_info;
                vkUpdateDescriptorSets(sit_render.vk.device, 1, &bindless_write, 0, NULL);
            }
            if (font_slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
                VkDescriptorImageInfo single_image_info = {};
                single_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                single_image_info.imageView = font_slot->image_view;
                single_image_info.sampler = font_slot->sampler;

                VkWriteDescriptorSet single_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                single_write.dstSet = font_slot->single_sampler_descriptor_set;
                single_write.dstBinding = SIT_SAMPLER_BINDING_ALBEDO;
                single_write.dstArrayElement = 0;
                single_write.descriptorCount = 1;
                single_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                single_write.pImageInfo = &single_image_info;
                vkUpdateDescriptorSets(sit_render.vk.device, 1, &single_write, 0, NULL);
            } else if (sit_render.vk.text_sampler_layout != VK_NULL_HANDLE) {
                VkDescriptorPool used_pool = VK_NULL_HANDLE;
                font_slot->single_sampler_descriptor_set = _SituationVulkanAllocateDescriptorSet(
                    sit_render.vk.text_sampler_layout, &used_pool);
                font_slot->single_sampler_descriptor_pool = used_pool;
                if (font_slot->single_sampler_descriptor_set != VK_NULL_HANDLE) {
                    VkDescriptorImageInfo single_image_info = {};
                    single_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    single_image_info.imageView = font_slot->image_view;
                    single_image_info.sampler = font_slot->sampler;

                    VkWriteDescriptorSet single_write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
                    single_write.dstSet = font_slot->single_sampler_descriptor_set;
                    single_write.dstBinding = SIT_SAMPLER_BINDING_ALBEDO;
                    single_write.dstArrayElement = 0;
                    single_write.descriptorCount = 1;
                    single_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    single_write.pImageInfo = &single_image_info;
                    vkUpdateDescriptorSets(sit_render.vk.device, 1, &single_write, 0, NULL);
                }
            }
        }
    }
    if (font_slot && font_slot->descriptor_set != VK_NULL_HANDLE) {
        // Allocate new descriptor set with text_sampler_layout
        VkDescriptorPool used_pool = VK_NULL_HANDLE;
        VkDescriptorSet new_desc_set = _SituationVulkanAllocateDescriptorSet(sit_render.vk.text_sampler_layout, &used_pool);

        if (new_desc_set != VK_NULL_HANDLE) {
            // Update the descriptor set to point to the font atlas texture
            VkDescriptorImageInfo image_info = {
                .sampler = font_slot->sampler,
                .imageView = font_slot->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = new_desc_set,
                .dstBinding = SIT_SAMPLER_BINDING_ALBEDO,  // Binding 0
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &image_info
            };

            vkUpdateDescriptorSets(sit_render.vk.device, 1, &write, 0, NULL);
            // Replace the old descriptor set
            font_slot->descriptor_set = new_desc_set;
        }
    }
    #endif
    SIT_FREE(pixels);

    // Setup font struct
    // Note: We don't have STB baked data, so we rely on SituationCmdDrawText fallback for default font
    sit_render.default_font.fontData = NULL;
    sit_render.default_font.stbFontInfo = NULL;
    sit_render.default_font.atlas_texture = sit_render.default_font_atlas;
    sit_render.default_font.glyph_info = NULL;
    sit_render.default_font.atlas_width = tex_w;
    sit_render.default_font.atlas_height = tex_h;
    sit_render.default_font.font_height_pixels = 8.0f;
    sit_render.default_font.is_bitmap = false;
    sit_render.default_font.bitmap_data = NULL;
    sit_render.default_font.bitmap_width = 0;
    sit_render.default_font.bitmap_height = 0;
    sit_render.default_font.bitmap_count = 0;
    sit_render.default_font.first_char = 0;
    sit_render.default_font.chars_per_row = 16;
    sit_render.default_font.chars_per_col = 16;
    sit_render.default_font.display_cell_width = 8;
    sit_render.default_font.display_cell_height = 8;
    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] One-time setup of the built-in quad renderer subsystem.
 *
 * @details This function initializes the fast-path quad drawing primitive used throughout
 *          the library for full-screen effects, solid-color rectangles, debug overlays,
 *          virtual display compositing, and any simple 2D drawing that doesn't require
 *          a full mesh or model.
 *
 *          It is called exactly once during initialization (typically from `_SituationInitSubsystems`
 *          or after backend context is ready) and prepares:
 *
 *          OpenGL path:
 *            - Vertex/fragment shader pair (simple transform + flat color or texture)
 *            - Links them into a program object
 *            - Creates VAO + VBO for a static full-screen quad (-1..1 NDC coords)
 *            - Sets up uniform locations (model matrix, color, optional texture)
 *            - Caches the program ID for fast binding
 *
 *          Vulkan path:
 *            - Creates VkShaderModule(s) from embedded SPIR-V (or compiles GLSL if enabled)
 *            - Builds VkPipelineLayout (with push constants or small descriptor set)
 *            - Creates VkPipeline for graphics (compute if used for blits)
 *            - Prepares static vertex buffer or push-constant vertex data
 *            - Caches pipeline and layout handles
 *
 *          Common setup:
 *            - Defines quad vertices (two triangles: positions + optional UVs)
 *            - Sets default blend state (alpha blending enabled)
 *            - Validates no GL/VK errors during creation (`SIT_CHECK_GL_ERROR()`)
 *
 *          On success, `SituationCmdDrawQuad` and related calls become fully functional.
 *          On failure, logs error and disables quad rendering (draw calls become no-ops).
 *
 * @param width  Expected render target width (used for aspect ratio or viewport defaults).
 *               Usually matches primary window/backbuffer width.
 * @param height Expected render target height.
 *               Usually matches primary window/backbuffer height.
 *
 * @return true if quad renderer initialized successfully (shaders/pipeline/VAO ready),
 *         false on failure (shader compile/link fail, allocation error, invalid dimensions).
 *         Failures are logged internally and may set global `SituationError`.
 *
 * @note Must be called **after** backend context is current (GL context or Vulkan device ready).
 *       Thread safety: Only safe from the thread owning the context (usually main thread during init
 *       or render thread if deferred).
 *       Dimensions are used for initial viewport/scissor setup can be updated later via resize events.
 *       The quad is static (NDC coords) transformations are applied via model matrix in draw calls.
 *
 *       Critical dependencies:
 *         - OpenGL context current (GL path) or Vulkan device/queue ready
 *         - `_SituationInitOpenGL` / `_SituationInitVulkan` already completed
 *         - No prior quad init (idempotent but wasteful if called twice)
 *
 * @see SituationCmdDrawQuad, _SituationInitSubsystems (caller),
 *      _SituationInitOpenGL, _SituationInitVulkan,
 *      SITUATION_ERROR_SHADER_COMPILATION_FAILED,
 *      SITUATION_ERROR_VULKAN_PIPELINE_CREATE_FAILED
 */
#if defined(SITUATION_USE_VULKAN)
static const uint32_t SIT_QUAD_VK_PUSH_BYTES =
    (uint32_t)(sizeof(mat4) + sizeof(mat4) + sizeof(vec4) + sizeof(vec4) + sizeof(uint32_t) + sizeof(int));

static const uint32_t SIT_YPQ_GRADE_PUSH_BYTES =
    SIT_QUAD_VK_PUSH_BYTES + (uint32_t)(sizeof(float) * 4u);

static void _SitVulkanDestroyQuadVDDynamicPipelines(void);
#endif

static SituationError _SituationInitQuadRenderer(int width, int height) {
#if defined(SITUATION_USE_OPENGL)
    // --- OpenGL Quad Renderer Initialization ---
    SIT_DEBUG_LOG("[QUAD] Starting quad renderer initialization");
    SituationError shader_err_code = SITUATION_SUCCESS;

    // 1. Compile and link the internal quad shader program.
    SIT_DEBUG_LOG("[QUAD] Compiling quad shader program");
    sit_render.gl.quad_shader_program = _SituationCreateGLCoreShaderProgram(SIT_GPU_PATH_QUAD_VERT, SIT_GPU_PATH_QUAD_FRAG, &shader_err_code);

    if (shader_err_code != SITUATION_SUCCESS || sit_render.gl.quad_shader_program == 0) {
        SIT_DEBUG_LOG("[QUAD] FAILED: Shader compilation failed, error code: %d", shader_err_code);
        // Error message should already be set by _SituationCreateGLShaderProgram
        return (shader_err_code != SITUATION_SUCCESS) ? shader_err_code : SITUATION_ERROR_OPENGL_GENERAL;
    }
    SIT_DEBUG_LOG("[QUAD] Shader program created successfully, ID: %u", sit_render.gl.quad_shader_program);

    // 2. Define vertex data for a simple 2D quad (TRIANGLE_STRIP order).
    // Format: [X, Y] (assuming Z=0, W=1 in shader or handled by model matrix)
    float quad_vertices[] = {
        0.0f, 0.0f, // Bottom-left
        1.0f, 0.0f, // Bottom-right
        0.0f, 1.0f, // Top-left
        1.0f, 1.0f  // Top-right
    };

    // --- [PRIVATE VAO/VBO SETUP for Quad Renderer] ---

    // 3. Create the PRIVATE VAO and VBO for the quad renderer.
    SIT_DEBUG_LOG("[QUAD] Creating private VAO");
    glCreateVertexArrays(1, &sit_render.gl.quad_vao);
    if (sit_render.gl.quad_vao == 0) {
        SIT_DEBUG_LOG("[QUAD] FAILED: VAO creation failed");
        glDeleteProgram(sit_render.gl.quad_shader_program);
        sit_render.gl.quad_shader_program = 0;
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitQuadRenderer: Failed to create private quad VAO.");
    }
    SIT_DEBUG_LOG("[QUAD] VAO created successfully, ID: %u", sit_render.gl.quad_vao);
    SIT_CHECK_GL_ERROR(); // Check for errors during VAO creation

    SIT_DEBUG_LOG("[QUAD] Creating private VBO");
    glCreateBuffers(1, &sit_render.gl.quad_vbo);
    if (sit_render.gl.quad_vbo == 0) {
        SIT_DEBUG_LOG("[QUAD] FAILED: VBO creation failed");
        glDeleteProgram(sit_render.gl.quad_shader_program);
        sit_render.gl.quad_shader_program = 0;
        glDeleteVertexArrays(1, &sit_render.gl.quad_vao);
        sit_render.gl.quad_vao = 0;
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitQuadRenderer: Failed to create private quad VBO.");
    }
    SIT_DEBUG_LOG("[QUAD] VBO created successfully, ID: %u", sit_render.gl.quad_vbo);
    SIT_CHECK_GL_ERROR(); // Check for errors during VBO creation

    // 4. Allocate and populate the VBO's storage with the quad vertex data.
    // Using glNamedBufferStorage for DSA (Direct State Access).
    SIT_DEBUG_LOG("[QUAD] Allocating VBO storage");
    glNamedBufferStorage(sit_render.gl.quad_vbo, sizeof(quad_vertices), quad_vertices, 0); // Static data
    SIT_CHECK_GL_ERROR(); // Check for errors during buffer storage
    SIT_DEBUG_LOG("[QUAD] VBO storage allocated");

    // 5. Temporarily bind OUR private VAO to configure it.
    SIT_DEBUG_LOG("[QUAD] Binding private VAO");
    glBindVertexArray(sit_render.gl.quad_vao);
    // sit_render.gl.current_vao_id = sit_render.gl.quad_vao; // Don't track internal temporary binds as they are restored immediately
    SIT_CHECK_GL_ERROR(); // Check for errors during VAO binding

    // 6. Configure the VAO state: Bind VBO, set vertex attributes.
    // Bind the VBO to the VAO's binding index 0.
    SIT_DEBUG_LOG("[QUAD] Configuring VAO attributes");
    glVertexArrayVertexBuffer(sit_render.gl.quad_vao, 0, sit_render.gl.quad_vbo, 0, 2 * sizeof(float)); // Binding index 0, stride 2 floats
    SIT_CHECK_GL_ERROR();

    // Set up vertex attribute format for position (Location 0)
    glVertexArrayAttribFormat(sit_render.gl.quad_vao, SIT_ATTR_POSITION, 2, GL_FLOAT, GL_FALSE, 0);
    SIT_CHECK_GL_ERROR();
    glVertexArrayAttribBinding(sit_render.gl.quad_vao, SIT_ATTR_POSITION, 0);
    SIT_CHECK_GL_ERROR();
    glEnableVertexArrayAttrib(sit_render.gl.quad_vao, SIT_ATTR_POSITION);
    SIT_CHECK_GL_ERROR();
    SIT_DEBUG_LOG("[QUAD] VAO attributes configured");

    // 7. *** CRITICAL *** Unbind our private VAO.
    SIT_DEBUG_LOG("[QUAD] Unbinding private VAO");
    glBindVertexArray(0); // Explicit unbind for safety and clarity
    // sit_render.gl.current_vao_id = 0;
    SIT_CHECK_GL_ERROR();

    // --- End of Private VAO/VBO Setup ---

    // 8. Set the initial projection matrix uniform in the shader program.
    // This matrix maps from screen pixel coordinates (0,0 top-left) to clip space.
    SIT_DEBUG_LOG("[QUAD] Setting projection matrix uniform");
    mat4 proj_quad;
    glm_ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f, proj_quad); // Top-left is (0,0)
    glProgramUniformMatrix4fv(sit_render.gl.quad_shader_program, SIT_UNIFORM_LOC_PROJECTION_MATRIX, 1, GL_FALSE, (const GLfloat*)proj_quad);
    SIT_CHECK_GL_ERROR(); // Check for errors setting the uniform
    {
        GLint tex_loc = glGetUniformLocation(sit_render.gl.quad_shader_program, "u_Texture");
        if (tex_loc >= 0) {
            glProgramUniform1i(sit_render.gl.quad_shader_program, tex_loc, 0);
        }
    }
    SIT_CHECK_GL_ERROR();
    SIT_DEBUG_LOG("[QUAD] Projection matrix set");

    // 9. CRITICAL: Ensure the global_vao_id is bound again before returning.
    // This reinforces that the user's rendering state is ready.
    SIT_DEBUG_LOG("[QUAD] Restoring global VAO binding");
    glBindVertexArray(sit_render.gl.global_vao_id);
    sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
    SIT_CHECK_GL_ERROR();

    SIT_DEBUG_LOG("[QUAD] Quad renderer initialization complete");
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // --- Vulkan Quad Renderer Initialization ---

    // 1. Compile the unified GLSL source into SPIR-V.
    //    The compiler is mandatory for Vulkan internal renderers.
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Compiling quad vertex shader...\n"); fflush(stdout);
    #endif
    _SituationSpirvBlob vs_spirv = _SituationVulkanCompileCoreShaderFile(
        SIT_GPU_PATH_QUAD_VERT, "internal_quad.vert", shaderc_vertex_shader, NULL, 0);
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Compiling quad fragment shader...\n"); fflush(stdout);
    #endif
    _SituationSpirvBlob fs_spirv = _SituationVulkanCompileCoreShaderFile(
        SIT_GPU_PATH_QUAD_FRAG, "internal_quad.frag", shaderc_fragment_shader, NULL, 0);

    if (!vs_spirv.data || !fs_spirv.data) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   ERROR: Shader compilation failed!\n"); fflush(stdout);
        char* err_msg = NULL;
        SituationGetLastErrorMsg(&err_msg);
        printf("Situation [Vulkan Debug]:   Error: %s\n", err_msg ? err_msg : "Unknown"); fflush(stdout);
        if (err_msg) SituationFreeString(err_msg);
        #endif
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        return SituationGetLastErrorCode();
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Shaders compiled successfully\n"); fflush(stdout);
    #endif

    // 2. Create the Pipeline Layout.
    // This defines the "shape" of the uniforms (Descriptor Sets and Push Constants).
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // Accessible by both shaders
    push_constant_range.offset = 0;
    /* Projection(64) + Model(64) + Color(16) + UVRect(16) + TextureID(4) + UseTex(4) = 168 bytes */
    push_constant_range.size = SIT_QUAD_VK_PUSH_BYTES;

    // Set 0 = view/proj UBO; set 1 = per-draw sampler (text_sampler_layout).
    VkDescriptorSetLayout set_layouts[2];
    uint32_t set_layout_count = 1;
    set_layouts[0] = sit_render.vk.view_data_ubo_layout;
    if (sit_render.vk.text_sampler_layout != VK_NULL_HANDLE) {
        set_layouts[1] = sit_render.vk.text_sampler_layout;
        set_layout_count = 2;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = set_layout_count;
    pipeline_layout_info.pSetLayouts = set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Creating quad pipeline layout...\n"); fflush(stdout);
    #endif
    if (vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &sit_render.vk.quad_pipeline_layout) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create quad pipeline layout.");
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

    // 3. Define the quad's specific vertex input layout.
    VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 2 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attr_desc = {};
    attr_desc.binding = 0;
    attr_desc.location = SIT_ATTR_POSITION;
    attr_desc.format = VK_FORMAT_R32G32_SFLOAT;
    attr_desc.offset = 0;

    // 4. Call the generic pipeline creator with the quad's specific configuration.
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Creating graphics pipeline...\n"); fflush(stdout);
    #endif
    sit_render.vk.quad_pipeline = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv.data, vs_spirv.size,
        fs_spirv.data, fs_spirv.size,
        sit_render.vk.quad_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // Quads are drawn as a strip
        1, &binding_desc,
        1, &attr_desc,
        0,  /* depth controlled dynamically per draw (VD+depth vs swapchain UI) */
        VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL
    );

    if (sit_render.vk.quad_owned_vs_spirv) {
        SIT_FREE(sit_render.vk.quad_owned_vs_spirv);
        sit_render.vk.quad_owned_vs_spirv = NULL;
    }
    if (sit_render.vk.quad_owned_fs_spirv) {
        SIT_FREE(sit_render.vk.quad_owned_fs_spirv);
        sit_render.vk.quad_owned_fs_spirv = NULL;
    }
    sit_render.vk.quad_vs_spirv_size = 0;
    sit_render.vk.quad_fs_spirv_size = 0;
    if (vs_spirv.data && vs_spirv.size > 0) {
        sit_render.vk.quad_owned_vs_spirv = (uint8_t*)SIT_MALLOC(vs_spirv.size);
        if (sit_render.vk.quad_owned_vs_spirv) {
            memcpy(sit_render.vk.quad_owned_vs_spirv, vs_spirv.data, vs_spirv.size);
            sit_render.vk.quad_vs_spirv_size = vs_spirv.size;
        }
    }
    if (fs_spirv.data && fs_spirv.size > 0) {
        sit_render.vk.quad_owned_fs_spirv = (uint8_t*)SIT_MALLOC(fs_spirv.size);
        if (sit_render.vk.quad_owned_fs_spirv) {
            memcpy(sit_render.vk.quad_owned_fs_spirv, fs_spirv.data, fs_spirv.size);
            sit_render.vk.quad_fs_spirv_size = fs_spirv.size;
        }
    }

    _SituationFreeSpirvBlob(&vs_spirv);
    _SituationFreeSpirvBlob(&fs_spirv);

    if(sit_render.vk.quad_pipeline == VK_NULL_HANDLE) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]:   ERROR: Pipeline creation returned NULL!\n"); fflush(stdout);
        #endif
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Graphics pipeline created successfully\n"); fflush(stdout);
    #endif

    // 5. Create and upload the vertex buffer for the quad.
    // Unit Quad: (0,0) to (1,1)
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Creating quad vertex buffer...\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Device handle before buffer creation: %p\n", (void*)sit_render.vk.device); fflush(stdout);
    printf("Situation [Vulkan Debug]:   VMA allocator: %p\n", (void*)sit_render.vk.vma_allocator); fflush(stdout);
    #endif
    float quad_vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    if (_SituationVulkanCreateAndUploadBuffer(VK_NULL_HANDLE, quad_vertices, sizeof(quad_vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &sit_render.vk.quad_vertex_buffer, &sit_render.vk.quad_vertex_buffer_memory) != SITUATION_SUCCESS) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Failed to create quad vertex buffer.");
    }

    /* Solid DrawQuad still declares set-1 sampler in quad.frag — bind a white 1x1 texture. */
    {
        static const uint8_t quad_white_rgba[4] = {255, 255, 255, 255};
        SituationImage white_img = {
            .width = 1,
            .height = 1,
            .channels = 4,
            .data = (uint8_t*)quad_white_rgba
        };
        SituationError solid_tex_err = SituationCreateTexture(white_img, false, &sit_render.vk.quad_solid_texture);
        if (solid_tex_err != SITUATION_SUCCESS) {
            return solid_tex_err;
        }
    }
#ifdef SITUATION_VULKAN_DEBUG
    printf("Situation [Vulkan Debug]:   Quad vertex buffer created successfully\n"); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Buffer handle: %p, Memory: %p\n", (void*)sit_render.vk.quad_vertex_buffer, (void*)sit_render.vk.quad_vertex_buffer_memory); fflush(stdout);
    printf("Situation [Vulkan Debug]:   Quad renderer initialization complete!\n"); fflush(stdout);
    #endif

    return SITUATION_SUCCESS;
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief [INTERNAL] Initializes the built-in text renderer subsystem (glyph atlas, shaders, uniforms).
 *
 * @details This function is called once during library startup (typically from `SituationInit`)
 *          to set up the low-level text rendering infrastructure. It prepares everything needed
 *          for high-quality, efficient 2D text drawing via `SituationCmdDrawText` and related APIs.
 *
 *          What it initializes (in rough order):
 *            - Loads the default font bitmap (e.g. embedded 8x8 VGA font or stb_truetype atlas)
 *            - Creates GPU texture for the glyph atlas (RGBA8 or similar)
 *            - Uploads glyph metrics and UV coordinates (either pre-baked or computed)
 *            - Creates vertex/index buffers for a dynamic quad pool (for glyph instances)
 *            - Compiles/linkes the text-specific vertex + fragment shaders
 *              (simple transform + texture sampling + color tint + optional outline/drop shadow)
 *            - Sets up uniform buffer or push constants for per-draw text state
 *              (color, transform matrix, scale, font size, etc.)
 *            - Prepares descriptor sets / texture bindings (Vulkan) or texture units (OpenGL)
 *            - Allocates scratch buffers for string processing and glyph batching
 *            - Caches common ASCII/Unicode ranges if using runtime glyph rasterization
 *
 *          On success, the text renderer is ready for immediate use no further init required.
 *          On failure (e.g. texture allocation fail, shader compile error), logs warnings/errors
 *          and disables text rendering gracefully (future draw calls become no-ops).
 *
 * @return true if initialization completed successfully,
 *         false on any critical failure (shader compile fail, out of memory, invalid font data)
 *
 * @note This function is **called only once** during library lifetime.
 *       Thread safety: Must be called from the thread that owns the GL/VK context
 *       (typically main thread during init, or render thread if deferred).
 *       No locking assumes exclusive access during startup.
 *
 *       If `SITUATION_ENABLE_TEXT_RENDERER` is not defined (or disabled at runtime),
 *       this function returns true immediately (no-op).
 *
 *       Dependencies:
 *         - stb_truetype (for font loading/rasterization if dynamic)
 *         - Embedded font data (e.g. sit_default_8x8_font array)
 *         - Vulkan/OpenGL context already current
 *
 * @see SituationInit (caller), SituationCmdDrawText, SituationCmdDrawTextEx,
 *      SITUATION_ENABLE_TEXT_RENDERER (compile-time toggle),
 *      SITUATION_ERROR_SHADER_COMPILATION_FAILED, SITUATION_ERROR_MEMORY_ALLOCATION
 */
static SituationError _SituationInitTextRenderer(void) {
#if defined(SITUATION_USE_OPENGL)
    SituationError shader_err;
    sit_render.gl.text_shader_program = _SituationCreateGLCoreShaderProgram(SIT_GPU_PATH_TEXT_VERT, SIT_GPU_PATH_TEXT_FRAG, &shader_err);
    if (shader_err != SITUATION_SUCCESS || sit_render.gl.text_shader_program == 0) {
        return (shader_err != SITUATION_SUCCESS) ? shader_err : SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED;
    }

    glCreateVertexArrays(1, &sit_render.gl.text_vao);
    glCreateBuffers(1, &sit_render.gl.text_vbo);

    if (sit_render.gl.text_vao == 0 || sit_render.gl.text_vbo == 0) {
        if (sit_render.gl.text_vao) { glDeleteVertexArrays(1, &sit_render.gl.text_vao); sit_render.gl.text_vao = 0; }
        if (sit_render.gl.text_vbo) { glDeleteBuffers(1, &sit_render.gl.text_vbo); sit_render.gl.text_vbo = 0; }
        glDeleteProgram(sit_render.gl.text_shader_program);
        sit_render.gl.text_shader_program = 0;
        return _SituationSetErrorFromCode(SITUATION_ERROR_OPENGL_GENERAL, "_SituationInitTextRenderer: Failed to create text VAO/VBO");
    }

    // Pre-allocate a dynamic buffer (512KB = ~5400 characters)
    glNamedBufferData(sit_render.gl.text_vbo, 524288, NULL, GL_DYNAMIC_DRAW);

    glBindVertexArray(sit_render.gl.text_vao);
    // sit_render.gl.current_vao_id = sit_render.gl.text_vao;
    glVertexArrayVertexBuffer(sit_render.gl.text_vao, 0, sit_render.gl.text_vbo, 0, 4 * sizeof(float)); // Stride: x,y,u,v

    // Pos: 2 floats, offset 0
    glEnableVertexArrayAttrib(sit_render.gl.text_vao, SIT_ATTR_POSITION);
    glVertexArrayAttribFormat(sit_render.gl.text_vao, SIT_ATTR_POSITION, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(sit_render.gl.text_vao, SIT_ATTR_POSITION, 0);

    // UV: 2 floats, offset 8
    glEnableVertexArrayAttrib(sit_render.gl.text_vao, SIT_ATTR_TEXCOORD_0);
    glVertexArrayAttribFormat(sit_render.gl.text_vao, SIT_ATTR_TEXCOORD_0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(sit_render.gl.text_vao, SIT_ATTR_TEXCOORD_0, 0);

    glBindVertexArray(0);
    // sit_render.gl.current_vao_id = 0;
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    // Vulkan initialization is handled in _SituationVulkanInitInternalRenderers due to complex dependency chains
    // (Pipeline Layouts, SPIR-V compilation, etc.)
    return SITUATION_SUCCESS;
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief [INTERNAL] Initializes the YPQ grade textured-quad pipeline (reuses quad geometry).
 * @see SIT_YPQ_GRADE_FRAGMENT_SHADER, SituationCmdDrawTextureYpqGrade()
 */
static SituationError _SituationInitYpqGradeRenderer(int width, int height) {
#if defined(SITUATION_USE_OPENGL)
    SituationError shader_err_code = SITUATION_SUCCESS;
    sit_render.gl.ypq_grade_shader_program = _SituationCreateGLCoreShaderProgram(
        SIT_GPU_PATH_QUAD_VERT,
        SIT_GPU_PATH_YPQ_GRADE_FRAG,
        &shader_err_code);
    if (shader_err_code != SITUATION_SUCCESS || sit_render.gl.ypq_grade_shader_program == 0) {
        return (shader_err_code != SITUATION_SUCCESS) ? shader_err_code : SITUATION_ERROR_OPENGL_GENERAL;
    }

    mat4 proj_quad;
    glm_ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f, proj_quad);
    glProgramUniformMatrix4fv(
        sit_render.gl.ypq_grade_shader_program,
        SIT_UNIFORM_LOC_PROJECTION_MATRIX,
        1,
        GL_FALSE,
        (const GLfloat*)proj_quad);
    SIT_CHECK_GL_ERROR();
    {
        GLint tex_loc = glGetUniformLocation(sit_render.gl.ypq_grade_shader_program, "u_Texture");
        if (tex_loc >= 0) {
            glProgramUniform1i(sit_render.gl.ypq_grade_shader_program, tex_loc, 0);
        }
    }
    SIT_CHECK_GL_ERROR();
    return SITUATION_SUCCESS;

#elif defined(SITUATION_USE_VULKAN)
    _SituationSpirvBlob vs_spirv = _SituationVulkanCompileCoreShaderFile(
        SIT_GPU_PATH_QUAD_VERT, "internal_ypq_grade.vert", shaderc_vertex_shader, NULL, 0);
    _SituationSpirvBlob fs_spirv = _SituationVulkanCompileCoreShaderFile(
        SIT_GPU_PATH_YPQ_GRADE_FRAG, "internal_ypq_grade.frag", shaderc_fragment_shader, NULL, 0);
    if (!vs_spirv.data || !fs_spirv.data) {
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        return SituationGetLastErrorCode();
    }

    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = SIT_YPQ_GRADE_PUSH_BYTES;

    VkDescriptorSetLayout set_layouts[2];
    uint32_t set_layout_count = 1;
    set_layouts[0] = sit_render.vk.view_data_ubo_layout;
    if (sit_render.vk.text_sampler_layout != VK_NULL_HANDLE) {
        set_layouts[1] = sit_render.vk.text_sampler_layout;
        set_layout_count = 2;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = set_layout_count;
    pipeline_layout_info.pSetLayouts = set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    if (vkCreatePipelineLayout(sit_render.vk.device, &pipeline_layout_info, NULL, &sit_render.vk.ypq_grade_pipeline_layout) != VK_SUCCESS) {
        _SituationFreeSpirvBlob(&vs_spirv);
        _SituationFreeSpirvBlob(&fs_spirv);
        return _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED, "Failed to create YPQ grade pipeline layout.");
    }

    VkVertexInputBindingDescription binding_desc = { .binding = 0, .stride = 2 * sizeof(float), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attr_desc = {};
    attr_desc.binding = 0;
    attr_desc.location = SIT_ATTR_POSITION;
    attr_desc.format = VK_FORMAT_R32G32_SFLOAT;
    attr_desc.offset = 0;

    sit_render.vk.ypq_grade_pipeline = _SituationVulkanCreateGraphicsPipeline(
        vs_spirv.data, vs_spirv.size,
        fs_spirv.data, fs_spirv.size,
        sit_render.vk.ypq_grade_pipeline_layout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        1, &binding_desc,
        1, &attr_desc,
        SIT_VK_PIPELINE_BLEND_OPAQUE,
        VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL);

    _SituationFreeSpirvBlob(&vs_spirv);
    _SituationFreeSpirvBlob(&fs_spirv);

    if (sit_render.vk.ypq_grade_pipeline == VK_NULL_HANDLE) {
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }
    return SITUATION_SUCCESS;
#endif
    return SITUATION_ERROR_NOT_IMPLEMENTED;
}

/**
 * @brief [INTERNAL] Destroys all backend-specific resources used by the internal quad renderer.
 * @details This helper function is called during the main shutdown sequence to clean up the dedicated resources created by `_SituationInitQuadRenderer`. It ensures that the internal shaders, pipelines, and vertex buffers used for drawing simple quads are properly released.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:** Deletes the quad renderer's shader program, its private Vertex Array Object (VAO), and its Vertex Buffer Object (VBO).
 * - **Vulkan:** Destroys the `VkPipeline`, `VkPipelineLayout`, and the vertex `VkBuffer` (along with its `VmaAllocation`) associated with the quad renderer. It assumes the device is idle before being called.
 *
 * @note This function is designed to be robust and will safely handle being called on a partially initialized state by checking if resource handles are valid before attempting destruction.
 * @warning This function is for internal use by the main cleanup routines (`_SituationCleanupOpenGL` or `_SituationCleanupVulkan`) only.
 *
 * @see _SituationInitQuadRenderer()
 */
static void _SituationCleanupQuadRenderer(void) {
#if defined(SITUATION_USE_OPENGL)
    if (sit_render.gl.quad_shader_program) { glDeleteProgram(sit_render.gl.quad_shader_program); sit_render.gl.quad_shader_program = 0; }
    if (sit_render.gl.ypq_grade_shader_program) { glDeleteProgram(sit_render.gl.ypq_grade_shader_program); sit_render.gl.ypq_grade_shader_program = 0; }
    if (sit_render.gl.quad_vao) { glDeleteVertexArrays(1, &sit_render.gl.quad_vao); sit_render.gl.quad_vao = 0; }
    if (sit_render.gl.quad_vbo) { glDeleteBuffers(1, &sit_render.gl.quad_vbo); sit_render.gl.quad_vbo = 0; }

    // Cleanup Text Renderer
    if (sit_render.gl.text_shader_program) { glDeleteProgram(sit_render.gl.text_shader_program); sit_render.gl.text_shader_program = 0; }
    if (sit_render.gl.text_vao) { glDeleteVertexArrays(1, &sit_render.gl.text_vao); sit_render.gl.text_vao = 0; }
    if (sit_render.gl.text_vbo) { glDeleteBuffers(1, &sit_render.gl.text_vbo); sit_render.gl.text_vbo = 0; }

#elif defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.device) {
#ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: Cleaning up quad renderer...\n"); fflush(stdout);
        printf("Situation [Vulkan Debug]:   quad_pipeline=%p\n", (void*)sit_render.vk.quad_pipeline); fflush(stdout);
        printf("Situation [Vulkan Debug]:   quad_vertex_buffer=%p\n", (void*)sit_render.vk.quad_vertex_buffer); fflush(stdout);
        #endif
        _SitVulkanDestroyQuadVDDynamicPipelines();
        if (sit_render.vk.quad_pipeline) vkDestroyPipeline(sit_render.vk.device, sit_render.vk.quad_pipeline, NULL);
        if (sit_render.vk.quad_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.quad_pipeline_layout, NULL);
        if (sit_render.vk.quad_owned_vs_spirv) {
            SIT_FREE(sit_render.vk.quad_owned_vs_spirv);
            sit_render.vk.quad_owned_vs_spirv = NULL;
        }
        if (sit_render.vk.quad_owned_fs_spirv) {
            SIT_FREE(sit_render.vk.quad_owned_fs_spirv);
            sit_render.vk.quad_owned_fs_spirv = NULL;
        }
        sit_render.vk.quad_vs_spirv_size = 0;
        sit_render.vk.quad_fs_spirv_size = 0;
        if (sit_render.vk.ypq_grade_pipeline) vkDestroyPipeline(sit_render.vk.device, sit_render.vk.ypq_grade_pipeline, NULL);
        if (sit_render.vk.ypq_grade_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.ypq_grade_pipeline_layout, NULL);
        if (sit_render.vk.quad_vertex_buffer) {
#ifdef SITUATION_VULKAN_DEBUG
            printf("Situation [Vulkan Debug]:   Destroying quad vertex buffer...\n"); fflush(stdout);
            #endif
            vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.quad_vertex_buffer, sit_render.vk.quad_vertex_buffer_memory);
        }
        if (sit_render.vk.quad_solid_texture.generation) {
            SituationDestroyTexture(&sit_render.vk.quad_solid_texture);
        }

        // Text Renderer Cleanup
        if (sit_render.vk.text_pipeline) vkDestroyPipeline(sit_render.vk.device, sit_render.vk.text_pipeline, NULL);
        if (sit_render.vk.text_pipeline_layout) vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.text_pipeline_layout, NULL);
    }
#endif
}

/**
 * @brief [INTERNAL] Destroys all OpenGL-specific resources created by the library.
 * @details This function is the backend-specific cleanup handler for OpenGL. It is responsible for deleting all globally managed OpenGL objects, such as internal shader programs, VAOs, VBOs, and UBOs.
 *          It assumes the OpenGL context is still active when it is called.
 *
 * @par Cleanup Process
 *   - Destroys the internal quad renderer's shader, VAO, and VBO.
 *   - Destroys the shader programs used for virtual display compositing.
 *   - Destroys the global VAO used for all user rendering.
 *   - Destroys the private VAO/VBO used for drawing virtual display quads.
 *   - Destroys any other global resources like the view data UBO.
 *
 * @note This function is designed to be robust and will not cause errors if called on a partially initialized state (i.e., it checks if object IDs are non-zero before attempting deletion).
 * @warning This function is for internal use by `_SituationCleanupRenderer` only.
 */
#if defined(SITUATION_USE_OPENGL)
static void _SituationCleanupOpenGL(void) {
    // [Phase 2] Loader Window Cleanup
    #if !defined(__STDC_NO_THREADS__)
    if (sit_render.gl.loader_window) {
        glfwDestroyWindow(sit_render.gl.loader_window);
        sit_render.gl.loader_window = NULL;
    }
    // [v2.3.21] Render thread shutdown logic moved to _SituationDestroyRenderThread in SituationShutdown

    // Ensure we have a context for cleanup (re-acquire if needed)
    if (sit_gs.sit_glfw_window && glfwGetCurrentContext() == NULL) {
        glfwMakeContextCurrent(sit_gs.sit_glfw_window);
    }
    #endif

    _SitGpuProfShutdown();
    _SitQueryPoolShutdownAll();

    // The OpenGL context is still active here.
    // Clean up all library-managed GL objects.
    _SituationCleanupQuadRenderer();
    if (sit_render.gl.vd_shader_program_id != 0) glDeleteProgram(sit_render.gl.vd_shader_program_id);
    if (sit_render.gl.composite_shader_program_id != 0) glDeleteProgram(sit_render.gl.composite_shader_program_id);
    if (sit_render.gl.global_vao_id != 0) { glDeleteVertexArrays(1, &sit_render.gl.global_vao_id); sit_render.gl.global_vao_id = 0; }
    if (sit_render.gl.mesh_vao_id != 0) { glDeleteVertexArrays(1, &sit_render.gl.mesh_vao_id); sit_render.gl.mesh_vao_id = 0; }
    if (sit_render.gl.vd_quad_vao != 0) glDeleteVertexArrays(1, &sit_render.gl.vd_quad_vao);
    if (sit_render.gl.vd_quad_vbo != 0) glDeleteBuffers(1, &sit_render.gl.vd_quad_vbo);
    if (sit_render.gl.composite_copy_texture_id != 0) glDeleteTextures(1, &sit_render.gl.composite_copy_texture_id);
    if (sit_render.gl.view_data_ubo_id != 0) glDeleteBuffers(1, &sit_render.gl.view_data_ubo_id);
    if (sit_render.gl.vd_pattern_config_ubo_id != 0) glDeleteBuffers(1, &sit_render.gl.vd_pattern_config_ubo_id);
    if (sit_render.gl.vd_pattern_config_ssbo_id != 0) glDeleteBuffers(1, &sit_render.gl.vd_pattern_config_ssbo_id);

    // [Phase 2.5] Cleanup VAO Cache
    for (int i = 0; i < 256; i++) {
        _SitGLVaoCacheEntry* entry = sit_render.gl.vao_cache[i];
        while (entry) {
            _SitGLVaoCacheEntry* next = entry->next;
            if (entry->vao_id) glDeleteVertexArrays(1, &entry->vao_id);
            SIT_FREE(entry);
            entry = next;
        }
        sit_render.gl.vao_cache[i] = NULL;
    }

    // Cleanup Graveyards & Fences
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; i++) {
        // Wait for any pending GPU work for this frame before destroying
        if (sit_render.gl.frame_fences[i]) {
            glClientWaitSync(sit_render.gl.frame_fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, 100000000); // 100ms
            glDeleteSync(sit_render.gl.frame_fences[i]);
            sit_render.gl.frame_fences[i] = 0;
        }

        // Force flush now that we know the GPU is idle (or shutting down)
        _SitGLFlushGraveyard(i);

        if (sit_render.gl.graveyards[i].mesh_ids_to_clean) SIT_FREE(sit_render.gl.graveyards[i].mesh_ids_to_clean);
        if (sit_render.gl.graveyards[i].buffers_to_delete) SIT_FREE(sit_render.gl.graveyards[i].buffers_to_delete);
        if (sit_render.gl.graveyards[i].textures_to_delete) SIT_FREE(sit_render.gl.graveyards[i].textures_to_delete);
        if (sit_render.gl.graveyards[i].programs_to_delete) SIT_FREE(sit_render.gl.graveyards[i].programs_to_delete);
        ma_mutex_uninit(&sit_render.gl.graveyards[i].lock);
    }
    memset(sit_render.gl.graveyards, 0, sizeof(sit_render.gl.graveyards));

#if SIT_GL_SHADER_CACHE_ENABLE
    _SitGLProgramCacheShutdown(&sit_render.gl.program_cache);
#endif

    // Cleanup Soft Command Buffers
    for (int i = 0; i < SITUATION_MAX_FRAMES_IN_FLIGHT; i++) {
        if (sit_render.gl.soft_buffers[i].packets) SIT_FREE(sit_render.gl.soft_buffers[i].packets);
        if (sit_render.gl.soft_buffers[i].data_buffer) SIT_FREE(sit_render.gl.soft_buffers[i].data_buffer);
    }
    memset(sit_render.gl.soft_buffers, 0, sizeof(sit_render.gl.soft_buffers));

    // [Bug 6 Fix] Cleanup Ring Buffer and MDI Buffer (persistent mapped buffers)
    if (sit_render.gl.ring_buffer_id != 0) {
        // Unmap is implicit when buffer is deleted (persistent mapping)
        glDeleteBuffers(1, &sit_render.gl.ring_buffer_id);
    }
    if (sit_render.gl.mdi_buffer_id != 0) {
        glDeleteBuffers(1, &sit_render.gl.mdi_buffer_id);
    }
    if (sit_render.gl.ring_fences) {
        for (size_t i = 0; i < sit_render.gl.ring_fence_count; i++) {
            if (sit_render.gl.ring_fences[i]) glDeleteSync(sit_render.gl.ring_fences[i]);
        }
        SIT_FREE(sit_render.gl.ring_fences);
    }

    // [Bug 6 Fix] Zero out ALL OpenGL state to allow clean re-initialization.
    // Without this, _SituationInitOpenGL's guard checks (e.g., ring_buffer_id != 0)
    // would skip re-creation, leaving stale/deleted IDs that crash on use.
    if (sit_render.gl.screenshot_pbo != 0u) {
        glDeleteBuffers(1, &sit_render.gl.screenshot_pbo);
    }
    if (sit_render.gl.screenshot_buffer) {
        SIT_FREE(sit_render.gl.screenshot_buffer);
    }
    if (sit_render.gl.screenshot_mutex_initialized) {
        mtx_destroy(&sit_render.gl.screenshot_mutex);
    }
    memset(&sit_render.gl, 0, sizeof(sit_render.gl));
    sit_render.gl.screenshot_resolved_frame_index = -1;
    sit_render.gl.screenshot_requested = false;
}
#endif // SITUATION_USE_OPENGL

/**
 * @brief [INTERNAL] Destroys all Vulkan-specific resources created by the library.
 * @details This is the comprehensive backend-specific cleanup handler for Vulkan. It is responsible for destroying all Vulkan objects in the precise reverse order of their creation to ensure compliance with the API's strict object lifetime rules.
 *
 * @par Cleanup Process
 *   The function systematically destroys all resources, from high-level objects down to the `VkInstance` itself. This includes:
 *   - Internal renderers (quad renderer).
 *   - The swapchain and all its dependent resources (`_SituationVulkanCleanupSwapchain`).
 *   - All per-frame synchronization objects (semaphores, fences) and UBOs.
 *   - The main command pool, render pass, and VMA allocator.
 *   - All descriptor set layouts and descriptor pools.
 *   - The `VkDevice` (logical device).
 *   - The debug messenger, `VkSurfaceKHR`, and finally the `VkInstance`.
 *
 * @note This function is designed to be robust. It checks if each handle is non-NULL before attempting to destroy it, making it safe to call even if the initialization process failed partway through.
 * @warning This function is for internal use by `_SituationCleanupRenderer` only.
 */
#if defined(SITUATION_USE_VULKAN)
static void _SituationCleanupVulkan(void) {
    enum { SIT_VK_POOL_SEEN_MAX = 32 };
    VkDescriptorPool seen_pools[SIT_VK_POOL_SEEN_MAX];
    uint32_t seen_pool_count = 0;

    /* Bounded idle + event pump — vkDeviceWaitIdle can wedge forever (frozen window). */
    if (sit_render.vk.device != VK_NULL_HANDLE) {
        _SituationVulkanWaitInFlightFencesPump("_SituationCleanupVulkan");
    }

    _SitGpuProfShutdown();
    _SitQueryPoolShutdownAll();

    /* Drain graveyards before swapchain / internal teardown so deferred vmaDestroy* runs
       before any path that might invalidate allocator state; pairs with immediate destroys
       during SHUTTING_DOWN in SituationDestroy*. */
    if (sit_render.vk.graveyards) {
        for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
            _SituationFlushGraveyard(i);
        }
    }

#if defined(SIT_VK_SHADER_CACHE_ENABLE) && SIT_VK_SHADER_CACHE_ENABLE
    /* Drain shader cache before graveyard teardown — immediate destroy while device is up. */
    _SitVkShaderCacheShutdown(&sit_render.vk.shader_cache);
#if SIT_VK_SHADER_CACHE_PHASE2
    if (sit_render.vk.pipeline_cache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(sit_render.vk.device, sit_render.vk.pipeline_cache, NULL);
        sit_render.vk.pipeline_cache = VK_NULL_HANDLE;
    }
#endif
#endif

#if !defined(NDEBUG)
    if (sit_render.vk.raster_pipeline_resolve_count > 0u) {
        fprintf(stderr,
            "Situation [Vulkan Debug]: raster variant stats — resolve=%llu polygon_hit=%llu cull_front_hit=%llu rebind=%llu\n",
            (unsigned long long)sit_render.vk.raster_pipeline_resolve_count,
            (unsigned long long)sit_render.vk.raster_polygon_variant_hits,
            (unsigned long long)sit_render.vk.raster_cull_front_variant_hits,
            (unsigned long long)sit_render.vk.raster_pipeline_rebind_count);
        fflush(stderr);
    }
#endif

    _SituationCleanupQuadRenderer();
    _SituationVulkanCleanupSwapchain();
    for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
        vkDestroySemaphore(sit_render.vk.device, sit_render.vk.render_finished_semaphores[i], NULL);
        vkDestroySemaphore(sit_render.vk.device, sit_render.vk.image_available_semaphores[i], NULL);
        vkDestroySemaphore(sit_render.vk.device, sit_render.vk.compute_finished_semaphores[i], NULL);
        vkDestroyFence(sit_render.vk.device, sit_render.vk.in_flight_fences[i], NULL);
        vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.view_proj_ubo_buffer[i], sit_render.vk.view_proj_ubo_memory[i]);
        // Destroy dynamic VBOs
        if (sit_render.vk.dynamic_vbo[i]) {
            // No need to Unmap if VMA_ALLOCATION_CREATE_MAPPED_BIT was used
            vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.dynamic_vbo[i], sit_render.vk.dynamic_vbo_alloc[i]);
        }
    }
    // --- Free the arrays themselves ---
    SIT_FREE(sit_render.vk.command_buffers);
    SIT_FREE(sit_render.vk.compute_command_buffers);
    SIT_FREE(sit_render.vk.image_available_semaphores);
    SIT_FREE(sit_render.vk.render_finished_semaphores);
    SIT_FREE(sit_render.vk.compute_finished_semaphores);
    SIT_FREE(sit_render.vk.in_flight_fences);
    SIT_FREE(sit_render.vk.view_proj_ubo_buffer);
    SIT_FREE(sit_render.vk.view_proj_ubo_memory);
    SIT_FREE(sit_render.vk.view_proj_ubo_mapped);
    SIT_FREE(sit_render.vk.view_proj_ubo_descriptor_set);

    // Clean up graveyards
    if (sit_render.vk.graveyards) {
        for (uint32_t i = 0; i < sit_render.vk.max_frames_in_flight; i++) {
            _SituationFlushGraveyard(i); // Important: Flush resources first!
            _SituationCleanupGraveyard(&sit_render.vk.graveyards[i]);
        }
        SIT_FREE(sit_render.vk.graveyards);
    }

    _SituationCleanupStagingBuffers();

    for (int i = 0; i < sizeof(sit_render.vk.compute_layouts) / sizeof(sit_render.vk.compute_layouts[0]); ++i) {
        if (sit_render.vk.compute_layouts[i] != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.compute_layouts[i], NULL);
        }
    }
    if (sit_render.vk.graphics_spirv_layout_ubo_ssbo != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.graphics_spirv_layout_ubo_ssbo, NULL);
        sit_render.vk.graphics_spirv_layout_ubo_ssbo = VK_NULL_HANDLE;
    }
    if (sit_render.vk.graphics_spirv_layout_ubo_ssbo_sampler != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(sit_render.vk.device, sit_render.vk.graphics_spirv_layout_ubo_ssbo_sampler, NULL);
        sit_render.vk.graphics_spirv_layout_ubo_ssbo_sampler = VK_NULL_HANDLE;
    }
    vkDestroyCommandPool(sit_render.vk.device, sit_render.vk.command_pool, NULL);
    vkDestroyCommandPool(sit_render.vk.device, sit_render.vk.compute_command_pool, NULL);
    vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.main_window_render_pass, NULL);
    if (sit_render.vk.main_window_render_pass_resume != VK_NULL_HANDLE) {
        vkDestroyRenderPass(sit_render.vk.device, sit_render.vk.main_window_render_pass_resume, NULL);
        sit_render.vk.main_window_render_pass_resume = VK_NULL_HANDLE;
    }
    if (sit_render.vk.screenshot_mutex_initialized) {
        mtx_destroy(&sit_render.vk.screenshot_mutex);
        sit_render.vk.screenshot_mutex_initialized = false;
    }
    _SituationVulkanDestroyScreenshotResources();
    if (sit_render.vk.vd_pattern_config_descriptor_set != VK_NULL_HANDLE && sit_render.vk.vd_pattern_config_descriptor_pool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(sit_render.vk.device, sit_render.vk.vd_pattern_config_descriptor_pool, 1, &sit_render.vk.vd_pattern_config_descriptor_set);
        sit_render.vk.vd_pattern_config_descriptor_set = VK_NULL_HANDLE;
    }
    if (sit_render.vk.vd_pattern_config_descriptor_pool != VK_NULL_HANDLE) {
        VkDescriptorPool pool = sit_render.vk.vd_pattern_config_descriptor_pool;
        bool already = false;
        for (uint32_t si = 0; si < seen_pool_count; ++si) {
            if (seen_pools[si] == pool) { already = true; break; }
        }
        if (!already) {
            if (seen_pool_count < SIT_VK_POOL_SEEN_MAX) seen_pools[seen_pool_count++] = pool;
            vkDestroyDescriptorPool(sit_render.vk.device, pool, NULL);
        }
        sit_render.vk.vd_pattern_config_descriptor_pool = VK_NULL_HANDLE;
    }
    if (sit_render.vk.vd_pattern_config_ubo != VK_NULL_HANDLE) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.vd_pattern_config_ubo, sit_render.vk.vd_pattern_config_ubo_memory);
        sit_render.vk.vd_pattern_config_ubo = VK_NULL_HANDLE;
        sit_render.vk.vd_pattern_config_ubo_memory = VK_NULL_HANDLE;
    }
    if (sit_render.vk.vd_pattern_config_ssbo != VK_NULL_HANDLE) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.vd_pattern_config_ssbo, sit_render.vk.vd_pattern_config_ssbo_memory);
        sit_render.vk.vd_pattern_config_ssbo = VK_NULL_HANDLE;
        sit_render.vk.vd_pattern_config_ssbo_memory = VK_NULL_HANDLE;
    }
    if (sit_render.vk.vd_pattern_config_ubo_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.vd_pattern_config_ubo_layout, NULL);
        sit_render.vk.vd_pattern_config_ubo_layout = VK_NULL_HANDLE;
    }
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.ssbo_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.ubo_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.dynamic_ubo_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.storage_buffer_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.bindless_descriptor_layout, NULL); // [Bindless]
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.image_sampler_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.text_sampler_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.storage_image_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.compute_sampler_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.composite_dest_sampler_layout, NULL);
    vkDestroyDescriptorSetLayout(sit_render.vk.device, sit_render.vk.view_data_ubo_layout, NULL);

    // --- Safe Descriptor Pool Cleanup ---

    // 1. Destroy any pools created dynamically by the Manager
    if (sit_render.vk.descriptor_manager.pools) {
        for (int i = 0; i < sit_render.vk.descriptor_manager.count; ++i) {
            VkDescriptorPool pool = sit_render.vk.descriptor_manager.pools[i];
            if (pool == VK_NULL_HANDLE) continue;
            if (pool == sit_render.vk.persistent_descriptor_pool
                    || pool == sit_render.vk.global_bindless_pool
                    || pool == sit_render.vk.vd_pattern_config_descriptor_pool) {
                continue;
            }
            bool already = false;
            for (uint32_t si = 0; si < seen_pool_count; ++si) {
                if (seen_pools[si] == pool) { already = true; break; }
            }
            if (already) continue;
            if (seen_pool_count < SIT_VK_POOL_SEEN_MAX) seen_pools[seen_pool_count++] = pool;
            vkDestroyDescriptorPool(sit_render.vk.device, pool, NULL);
        }
        SIT_FREE(sit_render.vk.descriptor_manager.pools);
        sit_render.vk.descriptor_manager.pools = NULL;
    }

    // 2. Destroy the initial Persistent Pool (created in Init)
    if (sit_render.vk.persistent_descriptor_pool != VK_NULL_HANDLE) {
        VkDescriptorPool pool = sit_render.vk.persistent_descriptor_pool;
        bool already = false;
        for (uint32_t si = 0; si < seen_pool_count; ++si) {
            if (seen_pools[si] == pool) { already = true; break; }
        }
        if (!already) {
            if (seen_pool_count < SIT_VK_POOL_SEEN_MAX) seen_pools[seen_pool_count++] = pool;
            vkDestroyDescriptorPool(sit_render.vk.device, pool, NULL);
        }
        sit_render.vk.persistent_descriptor_pool = VK_NULL_HANDLE;
    }

    // 3. Destroy Global Bindless Pool
    if (sit_render.vk.global_bindless_pool != VK_NULL_HANDLE) {
        VkDescriptorPool pool = sit_render.vk.global_bindless_pool;
        bool already = false;
        for (uint32_t si = 0; si < seen_pool_count; ++si) {
            if (seen_pools[si] == pool) { already = true; break; }
        }
        if (!already) {
            if (seen_pool_count < SIT_VK_POOL_SEEN_MAX) seen_pools[seen_pool_count++] = pool;
            vkDestroyDescriptorPool(sit_render.vk.device, pool, NULL);
        }
        sit_render.vk.global_bindless_pool = VK_NULL_HANDLE;
    }
    // ------------------------------------------

    vmaDestroyAllocator(sit_render.vk.vma_allocator);
    sit_render.vk.vma_allocator = VK_NULL_HANDLE;

    vkDestroyDevice(sit_render.vk.device, NULL);
    if (sit_render.vk.debug_messenger != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(sit_render.vk.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != NULL) {
            func(sit_render.vk.instance, sit_render.vk.debug_messenger, NULL);
        }
    }
    vkDestroySurfaceKHR(sit_render.vk.instance, sit_render.vk.surface, NULL);
    vkDestroyInstance(sit_render.vk.instance, NULL);
    // Match OpenGL teardown: after destroying objects, clear handles so a later SituationInit
    // cannot see stale non-NULL VkDevice/VkInstance pointers (same class of re-init bug as
    // memset(&sit_render.gl, ...) in _SituationCleanupOpenGL).
    memset(&sit_render.vk, 0, sizeof(sit_render.vk));
    sit_render.vk.screenshot_requested = false;
}

/**
 * @brief [INTERNAL] Creates all pre-defined VkPipelineLayouts for compute shaders.
 * @details This function is called once during Vulkan initialization. It builds a set of common pipeline layouts that users can select via the SituationComputeLayoutType enum, abstracting away the complexity of Vulkan layout creation.
 * @return SITUATION_SUCCESS on success, or an error code if any layout fails to create.
 */
 static SituationError _SituationVulkanInitComputeLayouts(void) {
    VkPipelineLayoutCreateInfo layout_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkDescriptorSetLayout set_layouts[4]; // Max needed for our most complex layout
    VkPushConstantRange push_constant = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 128 };

    // Layout 1: SIT_COMPUTE_LAYOUT_ONE_SSBO
    set_layouts[0] = sit_render.vk.ssbo_layout;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 0;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_ONE_SSBO]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 2: SIT_COMPUTE_LAYOUT_TWO_SSBOS
    set_layouts[0] = sit_render.vk.ssbo_layout;
    set_layouts[1] = sit_render.vk.ssbo_layout; // Same layout used for two different sets
    layout_info.setLayoutCount = 2;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_TWO_SSBOS]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 3: SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO
    set_layouts[0] = sit_render.vk.storage_image_layout;
    set_layouts[1] = sit_render.vk.ssbo_layout;
    layout_info.setLayoutCount = 2;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 4: SIT_COMPUTE_LAYOUT_PUSH_CONSTANT
    layout_info.setLayoutCount = 0;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_PUSH_CONSTANT]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 5: SIT_COMPUTE_LAYOUT_EMPTY
    layout_info.setLayoutCount = 0;
    layout_info.pushConstantRangeCount = 0;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_EMPTY]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 6: SIT_COMPUTE_LAYOUT_BUFFER_IMAGE
    // Set 0: SSBO (Buffer), Set 1: Storage Image
    set_layouts[0] = sit_render.vk.ssbo_layout;
    set_layouts[1] = sit_render.vk.storage_image_layout;
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 0;
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_BUFFER_IMAGE]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;


    // Layout 7: SIT_COMPUTE_LAYOUT_GRID (terminal / grid — 4 sets)
#if defined(SITUATION_VERBOSE_DIAGNOSTICS)
    fprintf(stderr, "[Vulkan] Creating SIT_COMPUTE_LAYOUT_GRID with 4 sets...\n");
    fprintf(stderr, "[Vulkan]   Set 0: SSBO layout %p\n", (void*)sit_render.vk.ssbo_layout);
    fprintf(stderr, "[Vulkan]   Set 1: Storage Image layout %p\n", (void*)sit_render.vk.storage_image_layout);
    fprintf(stderr, "[Vulkan]   Set 2: Image Sampler layout %p\n", (void*)sit_render.vk.image_sampler_layout);
    fprintf(stderr, "[Vulkan]   Set 3: Image Sampler layout %p\n", (void*)sit_render.vk.image_sampler_layout);
#endif
    set_layouts[0] = sit_render.vk.ssbo_layout;
    set_layouts[1] = sit_render.vk.storage_image_layout;
    set_layouts[2] = sit_render.vk.compute_sampler_layout;  // Use compute-specific layout
    set_layouts[3] = sit_render.vk.compute_sampler_layout;  // Use compute-specific layout
    layout_info.setLayoutCount = 4;
    layout_info.pSetLayouts = set_layouts;

    // We reuse the push_constant range defined at top of function (64 bytes)
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;

    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_GRID]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    // Layout 8: SIT_COMPUTE_LAYOUT_VECTOR
    // Set 0: SSBO (Lines), Set 1: Storage Image (Output)
    // Uses Push Constants
    set_layouts[0] = sit_render.vk.ssbo_layout;
    set_layouts[1] = sit_render.vk.storage_image_layout;
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant;

    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.compute_layouts[SIT_COMPUTE_LAYOUT_VECTOR]) != VK_SUCCESS) return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;

    return SITUATION_SUCCESS;
}

/**
 * @brief [INTERNAL] Cached graphics pipeline layouts for user SPIR-V (harness / custom descriptors).
 */
static SituationError _SituationVulkanInitGraphicsSpirvLayouts(void) {
    VkDescriptorSetLayout set_layouts[2] = {
        sit_render.vk.ubo_layout,
        sit_render.vk.ssbo_layout
    };
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = set_layouts,
        /* pushConstantRangeCount = 0: deliberate — UBO_SSBO shaders have no push constants.
         * If a future UBO_SSBO shader needs push constants, use SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER
         * (which carries the 128-byte all-stages range) or promote this profile. */
        .pushConstantRangeCount = 0
    };
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info, NULL, &sit_render.vk.graphics_spirv_layout_ubo_ssbo) != VK_SUCCESS) {
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

    /* UBO_SSBO_SAMPLER: set 0 UBO, set 1 SSBO, set 2 combined image sampler (fragment stage).
     * Reuses text_sampler_layout (binding 0, fragment, combined image sampler) for set 2.
     * Push constants: 128 bytes, all graphics stages (for future push-constant users). */
    VkDescriptorSetLayout set_layouts_sampler[3] = {
        sit_render.vk.ubo_layout,
        sit_render.vk.ssbo_layout,
        sit_render.vk.text_sampler_layout
    };
    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset = 0,
        .size = 128
    };
    VkPipelineLayoutCreateInfo layout_info_sampler = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 3,
        .pSetLayouts = set_layouts_sampler,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range
    };
    if (vkCreatePipelineLayout(sit_render.vk.device, &layout_info_sampler, NULL, &sit_render.vk.graphics_spirv_layout_ubo_ssbo_sampler) != VK_SUCCESS) {
        return SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED;
    }

    return SITUATION_SUCCESS;
}

/**
 * @brief [Internal] Creates a Vulkan compute pipeline from SPIR-V bytecode.
 *
 * @details This function takes pre-compiled SPIR-V bytecode for a compute shader, creates the necessary Vulkan objects (VkShaderModule, VkPipelineLayout, VkPipeline), and returns a `SituationComputePipeline` struct containing them.
 *          The caller is responsible for assigning a public `id` to the returned struct and for adding it to any resource tracking systems.
 *          The caller is also responsible for eventually destroying the pipeline using `_SituationVulkanDestroyComputePipeline` or `SituationDestroyComputePipeline`.;
 *          Error handling is performed internally. If this function fails, it returns an invalid `SituationComputePipeline` (zero-initialized) and sets the library's last error state via `_SituationSetErrorFromCode`.
 *
 * @param cs_spirv_data Pointer to the compiled SPIR-V compute shader bytecode.
 *                      This data must be valid and correctly formatted SPIR-V.
 *                      Must not be NULL.
 * @param cs_spirv_size Size of the SPIR-V bytecode in bytes.
 *                      Must be greater than 0 and typically a multiple of 4.
 *                      Must not be 0.
 * @return A `SituationComputePipeline` struct.
 *         - On **success**: The struct contains valid Vulkan handles (`.vk_pipeline`, `.vk_pipeline_layout`) and should be used with Vulkan binding/execution functions. The caller must assign a public `.id`.
 *         - On **failure**: The struct is zero-initialized (`{0}`), indicating an invalid pipeline. The specific error can be retrieved using `SituationGetLastErrorMsg()`.
 *
 * @note This function requires the library to be initialized (`SituationInit` must have been called successfully).
 * @note This function creates a pipeline layout with **no descriptor set layouts** and **no push constant ranges**.
 *       If the compute shader requires descriptors or push constants, this function (or the logic calling it) must be modified to provide the appropriate `VkDescriptorSetLayout` objects and `VkPushConstantRange` definitions when creating the `VkPipelineLayout`.
 * @note The `VkShaderModule` created internally is destroyed immediately after the `VkPipeline` is successfully created, as per Vulkan specification.
 *       The `VkShaderModule` handle is **not** stored in the returned struct.
 * @warning The SPIR-V data pointed to by `cs_spirv_data` is not validated by this function for semantic correctness beyond basic Vulkan object creation. Passing invalid SPIR-V can lead to errors during pipeline creation or undefined behavior at runtime.
 * @see _SituationVulkanCreateShaderModule(), SituationCreateComputePipelineFromMemory(), SituationDestroyComputePipeline();
 */
// _SituationVulkanCreateComputePipelineFromSpirv ***** Function got nuked for SituationCreateComputePipeline()

/**
 * @brief [INTERNAL] Records a command to transition the layout of a VkImage, inserting a memory barrier.
 * @details This is a critical Vulkan synchronization helper that wraps `vkCmdPipelineBarrier` specifically for image layout transitions.
 *          Changing an image's layout is the primary way in Vulkan to signal a change in how the image will be used, ensuring that writes from one pipeline stage are visible to reads in a subsequent stage.
 *
 * @par Synchronization Logic
 *   The function automatically determines the correct `srcStageMask`, `dstStageMask`, `srcAccessMask`, and `dstAccessMask` for a set of common, essential transitions:
 *   - `UNDEFINED` -> `TRANSFER_DST_OPTIMAL`: Prepares an image to be a destination for a copy operation.
 *   - `TRANSFER_DST_OPTIMAL` -> `SHADER_READ_ONLY_OPTIMAL`: Makes an image that has been written to available for sampling in a shader.
 *   - `PRESENT_SRC_KHR` -> `TRANSFER_SRC_OPTIMAL`: After `vkCmdEndRenderPass` (finalLayout present), prepares the swapchain for `vkCmdCopyImageToBuffer`. Source stage must be **COLOR_ATTACHMENT_OUTPUT** so the copy waits on fragment writes, not `TRANSFER` (which would not synchronize with the draw that filled the image).
 *   - `TRANSFER_SRC_OPTIMAL` -> `PRESENT_SRC_KHR`: Transitions a swapchain image back to a presentable state after a copy.
 *
 * If an unsupported transition is requested, an error is set.
 *
 * @param cmd The `VkCommandBuffer` (which must be in the recording state) into which the pipeline barrier command will be recorded.
 * @param image The `VkImage` whose layout is to be transitioned.
 * @param mip_levels The number of mip levels in the image's subresource range to be transitioned.
 * @param old_layout The current `VkImageLayout` of the image.
 * @param new_layout The target `VkImageLayout` to transition the image to.
 *
 * @note This function is a fundamental building block for managing resource lifetimes and dependencies in the Vulkan backend.
 * @warning This is a low-level helper for internal use only. Incorrectly specifying `old_layout` can lead to validation errors or race conditions.
 *
 * @see _SituationVulkanCopyBufferToImage(), _SituationVulkanGenerateMipmaps(), vkCmdPipelineBarrier()
 */
static void _SituationVulkanTransitionImageLayout(VkCommandBuffer cmd, VkImage image, uint32_t mip_levels, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    // Determine pipeline stages and access masks based on the layouts
    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        /* Post-render-pass screenshot: last writer was the color attachment (EndRenderPass -> PRESENT).
         * MEMORY_READ/BOTTOM_OF_PIPE was too weak and read stale texels on some drivers (wireframe /
         * text overlay looked correct on present but readback still showed prior frame or black). */
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
        /* Prepare for vkCmdBeginRenderPass with attachment initialLayout UNDEFINED (discard + clear). */
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = 0;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        // This is not an exhaustive list. Add other transitions as needed.
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_COMMAND_FAILED, "Unsupported image layout transition specified in helper.");
        return;
    }

    vkCmdPipelineBarrier(cmd, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

/**
 * @brief [INTERNAL] Records a command to copy data from a VkBuffer to a VkImage.
 * @details This is a fundamental Vulkan utility function that wraps `vkCmdCopyBufferToImage`. It is used to transfer raw pixel data from a staging buffer in CPU-accessible memory to a final, device-local image on the GPU.
 *          It configures a single `VkBufferImageCopy` region to copy the entire buffer to the base mip level (level 0) and base array layer (layer 0) of the destination image.
 *
 * @param cmd The `VkCommandBuffer` (which must be in the recording state) into which the copy command will be recorded.
 * @param buffer The source `VkBuffer` containing the pixel data to be copied.
 * @param image The destination `VkImage` that will receive the pixel data.
 * @param width The width of the image region to copy, in pixels.
 * @param height The height of the image region to copy, in pixels.
 *
 * @note This function assumes that the destination `image` has been previously transitioned to the `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` layout, making it ready to receive data.
 * @warning This is a low-level helper for internal use by functions like `SituationCreateTexture`. It does not perform any synchronization; the caller is responsible for ensuring the source buffer is ready and for transitioning the image layout after the copy is complete.
 *
 * @see SituationCreateTexture(), _SituationVulkanTransitionImageLayout(), vkCmdCopyBufferToImage()
 */
static void _SituationVulkanCopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

/**
 * Copy raw mapped swapchain/staging texels into RGBA8 order (SituationImage / OpenGL parity).
 * Vulkan B8G8R8A8 layouts store B,G,R,A in memory; A2R10G10B10 is unpacked and quantized to 8-bit.
 * @param pixel_count Number of 32-bit texels (not bytes).
 */
static void _SituationVulkanCopyMappedColorToRGBA(uint8_t* dst, const void* mapped, size_t pixel_count, VkFormat fmt) {
    const uint8_t* s = (const uint8_t*)mapped;
    switch (fmt) {
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32: {
        const uint32_t* s32 = (const uint32_t*)mapped;
        for (size_t p = 0; p < pixel_count; p++) {
            _SitUnpackA2R10G10B10ToRgba8(dst + p * 4, s32[p]);
        }
        break;
    }
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: {
        const uint32_t* s32 = (const uint32_t*)mapped;
        for (size_t p = 0; p < pixel_count; p++) {
            const uint32_t px = s32[p];
            dst[p * 4 + 0] = (uint8_t)(((px >> 0) & 0x3FFu) * 255u / 1023u);
            dst[p * 4 + 1] = (uint8_t)(((px >> 10) & 0x3FFu) * 255u / 1023u);
            dst[p * 4 + 2] = (uint8_t)(((px >> 20) & 0x3FFu) * 255u / 1023u);
            dst[p * 4 + 3] = (uint8_t)(((px >> 30) & 0x3u) * 255u / 3u);
        }
        break;
    }
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        for (size_t p = 0; p < pixel_count; p++) {
            size_t i = p * 4;
            dst[i + 0] = s[i + 2];
            dst[i + 1] = s[i + 1];
            dst[i + 2] = s[i + 0];
            dst[i + 3] = s[i + 3];
        }
        break;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        memcpy(dst, s, pixel_count * 4);
        break;
    default:
        memcpy(dst, s, pixel_count * 4);
        break;
    }
}

/**
 * @brief [INTERNAL] Synchronously copies a device-local VkImage to CPU-visible memory.
 *
 * @details This is a heavy-weight helper used for screenshots. It performs a complex sequence:
 *          1. Allocates a temporary host-visible buffer (`VK_BUFFER_USAGE_TRANSFER_DST_BIT`).
 *          2. Records a one-time command buffer to:
 *             - Transition source image layout to `TRANSFER_SRC_OPTIMAL`.
 *             - Execute `vkCmdCopyImageToBuffer`.
 *             - Transition source image layout back to its original state.
 *          3. Submits and waits for the GPU to finish (`vkQueueWaitIdle`).
 *          4. Maps the temporary buffer memory.
 *          5. `memcpy`s the data to a new `SIT_MALLOC`'d pointer.
 *          6. Destroys the temporary buffer.
 *
 * @param srcImage The source image handle (must have `TRANSFER_SRC` usage).
 * @param srcImageLayout The current layout of the source image (restored after copy).
 * @param width Image width.
 * @param height Image height.
 *
 * @return A pointer to raw pixel data (RGBA8), or NULL on failure. Caller must `free()`.
 */
static void* _SituationVulkanBlitImageToHostVisibleBuffer(VkImage srcImage, VkImageLayout srcImageLayout, uint32_t width, uint32_t height) {
    VkBuffer dstBuffer;
    VmaAllocation dstAllocation;
    VkDeviceSize bufferSize = (VkDeviceSize)width * height * 4; // Assuming 4 bytes per pixel (RGBA)
    void* finalImageData = NULL; // The final buffer we will return to the user

    // --- Step 1: Create the destination buffer in host-visible memory ---
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = bufferSize, .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT };
    VmaAllocationCreateInfo allocInfo = { .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, .usage = VMA_MEMORY_USAGE_GPU_TO_CPU };

    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &bufferInfo, &allocInfo, &dstBuffer, &dstAllocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Failed to create host-visible buffer for screenshot.");
        return NULL;
    }

    // --- Step 2: Record and submit commands for the copy ---
    VkCommandBuffer cmd = _SituationVulkanBeginSingleTimeCommands();

    // a. Transition source image to be ready for copy
    //    We need a new, more generic transition helper for this.
    _SituationVulkanTransitionImageLayout(cmd, srcImage, 1, srcImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // b. Record the copy command
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){width, height, 1};
    vkCmdCopyImageToBuffer(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuffer, 1, &region);

    // c. Transition source image back to its original layout so it can be presented
    _SituationVulkanTransitionImageLayout(cmd, srcImage, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcImageLayout);

    // Submit and wait for completion
    if (_SituationVulkanEndSingleTimeCommands(cmd) != SITUATION_SUCCESS) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, dstBuffer, dstAllocation);
        return NULL;
    }

    // --- Step 3: Map the memory, copy it, and clean up ---
    void* mappedData;
    if (vmaMapMemory(sit_render.vk.vma_allocator, dstAllocation, &mappedData) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_BUFFER_MAP_FAILED, "Failed to map screenshot buffer.");
        vmaDestroyBuffer(sit_render.vk.vma_allocator, dstBuffer, dstAllocation);
        return NULL;
    }

    // Allocate the final buffer for the user and copy the data
    finalImageData = SIT_MALLOC(bufferSize);
    if (finalImageData) {
        _SituationVulkanCopyMappedColorToRGBA((uint8_t*)finalImageData, mappedData,
            (size_t)width * (size_t)height, sit_render.vk.swapchain_image_format);
    } else {
        _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "Final screenshot image buffer.");
    }

    vmaUnmapMemory(sit_render.vk.vma_allocator, dstAllocation);
    vmaDestroyBuffer(sit_render.vk.vma_allocator, dstBuffer, dstAllocation);

    return finalImageData;
}

static void _SituationVulkanDestroyScreenshotResources(void) {
    if (sit_render.vk.screenshot_staging_buffer != VK_NULL_HANDLE && sit_render.vk.vma_allocator) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_buffer, sit_render.vk.screenshot_staging_allocation);
        sit_render.vk.screenshot_staging_buffer = VK_NULL_HANDLE;
        sit_render.vk.screenshot_staging_allocation = VK_NULL_HANDLE;
    }
    if (sit_render.vk.screenshot_buffer) {
        SIT_FREE(sit_render.vk.screenshot_buffer);
        sit_render.vk.screenshot_buffer = NULL;
    }
    sit_render.vk.screenshot_width = 0;
    sit_render.vk.screenshot_height = 0;
    sit_render.vk.screenshot_valid = false;
    sit_render.vk.screenshot_resolved_frame_index = UINT32_MAX;
    for (int _si = 0; _si < SITUATION_MAX_FRAMES_IN_FLIGHT; _si++) {
        sit_render.vk.screenshot_copy_pending[_si] = false;
    }
}

static SituationError _SituationVulkanEnsureScreenshotResources(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (sit_render.vk.screenshot_staging_buffer != VK_NULL_HANDLE &&
        (uint32_t)sit_render.vk.screenshot_width == width &&
        (uint32_t)sit_render.vk.screenshot_height == height) {
        return SITUATION_SUCCESS;
    }
    _SituationVulkanDestroyScreenshotResources();
    /* Same allocation pattern as _SituationVulkanBlitImageToHostVisibleBuffer (proven readback path). */
    VkDeviceSize buffer_size = (VkDeviceSize)width * (VkDeviceSize)height * 4u;
    VkBufferCreateInfo buf_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buf_info.size = buffer_size;
    buf_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo alloc_info = {0};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    if (vmaCreateBuffer(sit_render.vk.vma_allocator, &buf_info, &alloc_info,
            &sit_render.vk.screenshot_staging_buffer,
            &sit_render.vk.screenshot_staging_allocation, NULL) != VK_SUCCESS) {
        _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED, "Vulkan screenshot staging buffer creation failed.");
        return SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED;
    }
    sit_render.vk.screenshot_buffer = (uint8_t*)SIT_MALLOC((size_t)width * (size_t)height * 4u);
    if (!sit_render.vk.screenshot_buffer) {
        vmaDestroyBuffer(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_buffer, sit_render.vk.screenshot_staging_allocation);
        sit_render.vk.screenshot_staging_buffer = VK_NULL_HANDLE;
        sit_render.vk.screenshot_staging_allocation = VK_NULL_HANDLE;
        return SITUATION_ERROR_MEMORY_ALLOCATION;
    }
    sit_render.vk.screenshot_width = (int)width;
    sit_render.vk.screenshot_height = (int)height;
    return SITUATION_SUCCESS;
}

static void _SituationVulkanRecordScreenshotCopy(VkCommandBuffer cmd, VkImage swapchain_image, uint32_t width, uint32_t height) {
    /* Ensure color attachment writes from the render pass just ended are visible to the transfer copy. */
    VkMemoryBarrier mem_barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    mem_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    mem_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 1, &mem_barrier, 0, NULL, 0, NULL);

    _SituationVulkanTransitionImageLayout(cmd, swapchain_image, 1, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = (VkOffset3D){0, 0, 0};
    region.imageExtent = (VkExtent3D){width, height, 1};
    vkCmdCopyImageToBuffer(cmd, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        sit_render.vk.screenshot_staging_buffer, 1, &region);
    _SituationVulkanTransitionImageLayout(cmd, swapchain_image, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    if (sit_render.vk.current_frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
        sit_render.vk.screenshot_copy_pending[sit_render.vk.current_frame_index] = true;
    }
}

static void _SituationVulkanResolveScreenshotAfterSubmit(uint32_t frame_index) {
    bool do_copy = (frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) && sit_render.vk.screenshot_copy_pending[frame_index];
    if (frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
        sit_render.vk.screenshot_copy_pending[frame_index] = false;
    }
    if (!do_copy || sit_render.vk.screenshot_staging_buffer == VK_NULL_HANDLE || !sit_render.vk.screenshot_buffer) {
        sit_render.vk.screenshot_valid = false;
        sit_render.vk.screenshot_resolved_frame_index = UINT32_MAX;
        return;
    }

    VkResult w = _SituationVulkanWaitFencePumpWindow(sit_render.vk.device, sit_render.vk.in_flight_fences[frame_index]);
    if (w != VK_SUCCESS) {
        if (w == VK_TIMEOUT) {
            fprintf(stderr, "[Vulkan] Screenshot fence wait timed out (frame_index=%u)\n", frame_index);
            fflush(stderr);
        }
        sit_render.vk.screenshot_valid = false;
        sit_render.vk.screenshot_resolved_frame_index = UINT32_MAX;
        return;
    }

    void* mapped = NULL;
    if (vmaMapMemory(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_allocation, &mapped) != VK_SUCCESS) {
        sit_render.vk.screenshot_valid = false;
        sit_render.vk.screenshot_resolved_frame_index = UINT32_MAX;
        return;
    }

    VmaAllocationInfo alloc_inf = {};
    vmaGetAllocationInfo(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_allocation, &alloc_inf);
    VkMappedMemoryRange flush_range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
    flush_range.memory = alloc_inf.deviceMemory;
    flush_range.offset = alloc_inf.offset;
    flush_range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(sit_render.vk.device, 1, &flush_range);

    if (sit_render.vk.screenshot_mutex_initialized) {
        mtx_lock(&sit_render.vk.screenshot_mutex);
    }
    _SituationVulkanCopyMappedColorToRGBA(sit_render.vk.screenshot_buffer, mapped,
        (size_t)sit_render.vk.screenshot_width * (size_t)sit_render.vk.screenshot_height,
        sit_render.vk.swapchain_image_format);
    sit_render.vk.screenshot_valid = true;
    sit_render.vk.screenshot_resolved_frame_index = frame_index;
    if (sit_render.vk.screenshot_mutex_initialized) {
        mtx_unlock(&sit_render.vk.screenshot_mutex);
    }

    vmaUnmapMemory(sit_render.vk.vma_allocator, sit_render.vk.screenshot_staging_allocation);
}

/** Resolve a pending pre-present screenshot for frame_index (render-thread race safe for readback). */
static void _SituationVulkanEnsureScreenshotResolvedForFrame(uint32_t frame_index) {
    if (frame_index >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return;
    }
    if (!sit_render.vk.screenshot_copy_pending[frame_index]) {
        return;
    }
    if (sit_render.vk.screenshot_valid &&
        sit_render.vk.screenshot_resolved_frame_index == frame_index) {
        return;
    }
    _SituationVulkanResolveScreenshotAfterSubmit(frame_index);
}

/**
 * @brief [INTERNAL] Generates a complete mipmap chain for a Vulkan image using sequential blits.
 * @details This helper function is responsible for creating all mipmap levels for a given texture, from the base level (mip 0) down to the final 1x1 level. It performs this by iteratively blitting from each mip level `i` to the next level `i+1`, which has half the dimensions.
 *          This process is essential for high-quality texture rendering, as it provides pre-filtered, lower-resolution versions of the texture for the GPU to sample from when the object is far from the camera, significantly reducing aliasing and shimmering artifacts.
 *
 * @par Synchronization and Workflow
 *   The function executes a precise, looped sequence of commands for each new mip level:
 *   1.  **Barrier:** It first records a `VkImageMemoryBarrier` to transition the layout of the *source* mip level (e.g., mip `i-1`) from `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
 *       This ensures that the previous write operation (either the initial data copy or the previous blit) is complete and the memory is visible for reading.
 *   2.  **Blit:** It records a `vkCmdBlitImage` command. This command performs the downscaling operation, copying from the source mip level to the destination mip level (e.g., from mip `i-1` to mip `i`). Linear filtering is used to ensure a smooth, high-quality downsample.
 *   3.  **Barrier:** Immediately after the blit command, it records another barrier to transition the layout of the *source* mip level (`i-1`) from `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
 *       This makes the now-finalized mip level available for sampling by shaders.
 *
 * After the loop finishes, a final barrier is issued to transition the very last mip level to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
 *
 * @param cmd The `VkCommandBuffer` (which must be in the recording state) into which the barrier and blit commands will be recorded.
 * @param image The `VkImage` for which to generate mipmaps. This image must have been created with `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` and `VK_IMAGE_USAGE_TRANSFER_DST_BIT` usage flags.
 * @param width The width of the base mip level (level 0).
 * @param height The height of the base mip level (level 0).
 * @param mip_levels The total number of mip levels in the image, including the base level.
 *
 * @note This function assumes that the base mip level (level 0) has already been populated with data and that all mip levels are currently in the `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` layout.
 * @warning This function is for internal use by `SituationCreateTexture` only and should not be called directly.
 *
 * @see SituationCreateTexture(), vkCmdBlitImage(), vkCmdPipelineBarrier()
 */
static void _SituationVulkanGenerateMipmaps(VkCommandBuffer cmd, VkImage image, int32_t width, int32_t height, uint32_t mip_levels) {
    // This barrier will be reused to transition each mip level
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = width;
    int32_t mip_height = height;

    for (uint32_t i = 1; i < mip_levels; i++) {
        // 1. Transition the previous mip level (i-1) to be a transfer source.
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

        // 2. Perform the blit from the previous level to the current level.
        VkImageBlit blit = {};
        blit.srcOffsets[0] = (VkOffset3D){0, 0, 0};
        blit.srcOffsets[1] = (VkOffset3D){mip_width, mip_height, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = (VkOffset3D){0, 0, 0};
        blit.dstOffsets[1] = (VkOffset3D){ mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        // 3. Transition the previous mip level (i-1) to be shader-readable.
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

        // Update dimensions for the next iteration
        if (mip_width > 1) mip_width /= 2;
        if (mip_height > 1) mip_height /= 2;
    }

    // Finally, transition the very last mip level to be shader-readable.
    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
}

#endif // SITUATION_USE_VULKAN

/**
 * @brief [INTERNAL] Performs a comprehensive, robust cleanup of all library components in response to an initialization failure.
 * @details This is the primary error handling routine for the `SituationInit` function. It is designed to be called from any point during the initialization sequence if a critical failure occurs.
 *          Its purpose is to safely unwind the initialization process, releasing any resources that were successfully allocated before the point of failure to prevent leaks.
 *
 * @par Cleanup Strategy
 *   The function executes the main cleanup routines in the **exact reverse order of initialization** to respect dependencies (e.g., the Vulkan surface must be destroyed before the GLFW window).
 *   1.  **GPU Synchronization:** It first attempts to wait for the GPU to go idle (`vkDeviceWaitIdle` or `glFinish`). This is a critical step to ensure that no resources are in use by the GPU when destruction begins, preventing validation errors or crashes.
 *   2.  **Subsystem Teardown:** Calls `_SituationCleanupSubsystems` to release audio, input, and timer resources.
 *   3.  **Renderer Teardown:** Calls `_SituationCleanupRenderer` to dispatch to the backend-specific cleanup (`_SituationCleanupOpenGL` or `_SituationCleanupVulkan`).
 *   4.  **Platform Teardown:** Calls `_SituationCleanupPlatform` to destroy the window and terminate GLFW.
 *
 * The robustness of this function relies on the fact that each individual cleanup helper is idempotent and safely handles being called on a partially initialized state (i.e., by checking if resource handles are `NULL` before attempting to destroy them).
 *
 * @note This function is for internal use by `SituationInit` only and should never be called directly.
 * @warning After this function completes, the library is in a fully uninitialized state.
 *
 * @see SituationInit(), _SituationCleanupSubsystems(), _SituationCleanupRenderer(), _SituationCleanupPlatform()
 */


// --- Callbacks and Event Handling ---

// --- Command-Line Argument Queries ---

/**
 * @brief Prepares the rendering context for a new frame.
 *
 * @details This function must be called at the beginning of each application framebefore any rendering commands are recorded or executed. It performs backend-specific setup necessary to acquire the next rendering target (e.g., the next swapchain image in Vulkan) and prepare command buffers.
 *
 * @par Backend-Specific Behavior
 * - **OpenGL:**
 *   - Makes the main GLFW window's OpenGL context current for the calling thread.
 *   - Binds the default framebuffer (the main window's backbuffer).
 *   - Sets the viewport to cover the entire window area.
 *   - This function typically always succeeds if the library is initialized and the OpenGL context is valid, returning `true`.
 * - **Vulkan:**
 *   - Waits for the GPU to finish processing the commands associated with the frame identified by `sit_render.vk.current_frame_index`.
 *   - Attempts to acquire the next image from the swapchain. This image will be the target for rendering this frame.
 *   - If the swapchain is out of date (e.g., due to a window resize), this function internally calls `_SituationVulkanRecreateSwapchain` to handle the recreation process. In this specific case, it returns `false` to signal that the frame setup was interrupted and should be retried.
 *   - Resets the fence associated with the current frame index to the unsignaled state.
 *   - Resets the primary command buffer for the current frame.
 *   - Begins recording commands into the primary command buffer.
 *
 * @return `true` if the frame was successfully prepared and rendering can proceed.
 *         This is the standard return value for both OpenGL and Vulkan under normal conditions.
 * @return `false` (Vulkan only) if the swapchain was out of date and was automatically recreated. The caller should typically call `SituationAcquireFrameCommandBuffer()` again in the next iteration of their main loop to proceed with the new swapchain.
 * @return `false` if the library is not initialized.
 * @return `false` (Vulkan) if acquiring the swapchain image fails for reasons other than `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR`.
 * @return `false` (Vulkan) if resetting or beginning the command buffer fails.
 *
 * @note It is the caller's responsibility to ensure that:
 *       1. The library is initialized before calling this function.
 *       2. This function is called once per frame, before any rendering commands.
 *       3. (Vulkan) The application loop handles the `false` return value correctly, especially when it indicates swapchain recreation.
 *
 * @warning This function is not thread-safe and must be called from the thread that initialized the library.
 */

#if defined(SITUATION_USE_OPENGL)
/** Lookup texture registry slot by GL texture name (linear scan; used at execute time). */
static _SituationTextureSlot* _SituationGLFindTextureSlotByGlId(GLuint gl_tex_id) {
    if (gl_tex_id == 0) {
        return NULL;
    }
    for (int i = 0; i < SITUATION_MAX_TEXTURES; ++i) {
        _SituationTextureSlot* slot = &sit_render.texture_registry[i];
        if (slot->is_active && slot->gl_texture_id == gl_tex_id) {
            return slot;
        }
    }
    return NULL;
}

/** [Phase B3] Transition storage image to fragment sampling (barrier, unbind image unit, swizzle). */
static SituationError _SituationGLPrepareStorageTextureForSampling(_SituationTextureSlot* slot) {
    if (!slot || slot->gl_texture_id == 0) {
        return _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM,
            "_SituationGLPrepareStorageTextureForSampling: invalid texture slot.");
    }
    if ((slot->usage_flags & SITUATION_TEXTURE_USAGE_STORAGE) == 0u) {
        return SITUATION_SUCCESS;
    }

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);

    if (slot->gl_image_binding_unit >= 0) {
        glBindImageTexture((GLuint)slot->gl_image_binding_unit, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        slot->gl_image_binding_unit = -1;
    }

    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
    glTextureParameteri(slot->gl_texture_id, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

    slot->usage_flags |= SITUATION_TEXTURE_USAGE_SAMPLED;
    return SITUATION_SUCCESS;
}

/** Apply deterministic GL raster defaults so harness/tests cannot leak state across frames. */
static void _SituationGLApplyBaselineRasterState(void) {
    glFrontFace(GL_CCW);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    sit_render.gl.scissor_test_enabled = 0;
    glLineWidth(1.0f);
    sit_render.gl.cull_face_enabled = 0;
    sit_render.gl.current_polygon_mode = GL_FILL;
    sit_render.gl.polygon_offset_enabled = false;
    sit_render.gl.current_primitive_mode_set = false;
    sit_render.gl.current_primitive_mode = GL_TRIANGLES;
    sit_render.gl.current_index_type = GL_UNSIGNED_INT;
    sit_render.gl.bound_ibo_index_element_size = sizeof(uint32_t);
    sit_render.gl.bound_ibo_byte_offset = 0;
    sit_render.gl.current_virtual_loc = -1;
    if (sit_render.gl.global_vao_id != 0) {
        glBindVertexArray(sit_render.gl.global_vao_id);
        sit_render.gl.current_vao_id = sit_render.gl.global_vao_id;
    }
}
#endif

/** Reset tracked raster state so harness/tests cannot leak topology/polygon/cull across frames. */
static void _SituationResetTrackedRasterStateForNewFrame(void) {
#if defined(SITUATION_USE_VULKAN)
    sit_render.vk.dynamic_cull_mode = VK_CULL_MODE_NONE;
    sit_render.vk.dynamic_front_face = VK_FRONT_FACE_CLOCKWISE;
    sit_render.vk.dynamic_primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    sit_render.vk.dynamic_primitive_topology_initialized = true;
    sit_render.vk.dynamic_polygon_mode = VK_POLYGON_MODE_FILL;
    sit_render.vk.dynamic_depth_bias_enable = VK_FALSE;
    sit_render.vk.dynamic_depth_bias_constant = 0.0f;
    sit_render.vk.dynamic_depth_bias_clamp = 0.0f;
    sit_render.vk.dynamic_depth_bias_slope = 0.0f;
    sit_render.vk.dynamic_color_write_mask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    sit_render.vk.dynamic_depth_test_enable = VK_TRUE;
    sit_render.vk.dynamic_depth_write_enable = VK_FALSE;
    sit_render.vk.dynamic_depth_compare_op = VK_COMPARE_OP_LESS;
    sit_render.vk.dynamic_stencil_test_enable = VK_FALSE;
    memset(&sit_render.vk.dynamic_stencil_front, 0, sizeof(sit_render.vk.dynamic_stencil_front));
    memset(&sit_render.vk.dynamic_stencil_back, 0, sizeof(sit_render.vk.dynamic_stencil_back));
    sit_render.vk.dynamic_line_width = 1.0f;
    sit_render.vk.raster_stack_depth = 0;
    sit_render.vk.behavior = (SituationRendererBehaviorPolicy){0};
    sit_render.vk.behavior_stack_depth = 0;
    sit_render.vk.current_pbr_pipeline = VK_NULL_HANDLE;
    sit_render.vk.global_bindless_graphics_bound = false;
    sit_render.vk.global_bindless_graphics_layout = VK_NULL_HANDLE;
#endif
#if defined(SITUATION_USE_OPENGL)
    _SituationGLApplyBaselineRasterState();
#endif
}

/**
 * Number of render-thread frames between periodic resets of metric_max_latency_ns.
 * The adaptive backpressure policy uses this metric to decide SPIN vs SLEEP.  Without
 * periodic resets, a single transient spike (startup, alt-tab, DWM stall) permanently
 * ratchets the max upward, locking the policy into SLEEP with no path to recover.
 * 120 frames ≈ 2 seconds at 60 Hz — long enough for statistical relevance, short enough
 * that recovery happens within a few seconds of the spike passing.
 */
#ifndef SIT_LATENCY_METRIC_WINDOW_FRAMES
#define SIT_LATENCY_METRIC_WINDOW_FRAMES 120u
#endif

#if defined(SITUATION_USE_VULKAN)
static SituationError _SitVulkanBindGlobalBindlessSet(
    VkCommandBuffer vk_cmd, VkPipelineLayout layout, uint32_t set_index, const char* caller);
#endif

/**
 * @brief [INTERNAL] Executes one complete pass of the hot-reloading system (Velocity Module).
 *
 * @details This function is the core heartbeat of the hot-reload mechanism.
 *          It is called periodically (based on the configured hot-reload rate) or on-demand
 *          to detect file changes in watched directories/assets and trigger reloads where needed.
 *
 *          During a pass, the function:
 *            - Scans all registered watch paths for modification time changes
 *            - Compares current file mod-times against cached values
 *            - Identifies changed files (shaders, textures, models, audio, etc.)
 *            - Invokes the appropriate reload handler for each changed asset type
 *            - Re-compiles shaders to new SPIR-V blobs (if applicable)
 *            - Recreates GPU resources (shader modules, textures, pipelines, etc.)
 *            - Updates internal caches and invalidates stale references
 *            - Logs reload events and errors (in debug builds)
 *
 *          The pass is designed to be fast and non-blocking in most cases:
 *            - Only changed assets are processed
 *            - Resource recreation is deferred to the render thread when possible
 *            - Failures during reload (e.g. shader compilation error) are graceful  - 
 *              the old resource remains active and an error is logged/set
 *
 * Thread safety invariants:
 *   - Typically called from the main thread or a dedicated hot-reload timer thread
 *   - Filesystem access is protected where necessary (e.g. stat() calls)
 *   - GPU resource updates are queued to the render thread to avoid context conflicts
 *   - No long-blocking operations  -  filesystem checks are lightweight
 *
 * @note This function is usually invoked automatically by the thread pool's hot-reload timer
 *       (if enabled via `SituationCreateThreadPool` hot_reload_rate parameter).
 *       Manual calls are safe but redundant unless forcing an immediate reload.
 *       Reload rate should be tuned to balance responsiveness vs CPU usage
 *       (typical values: 0.1-1.0 seconds).
 *
 * @see SituationCreateThreadPool (hot_reload_rate parameter),
 *      _SituationCompileGLSLtoSPIRV, _SituationFreeSpirvBlob,
 *      Velocity Module documentation, SITUATION_ERROR_FILE_MODIFIED
 */
// [v2.3.34] Hot-Reload Logic (Running on I/O Thread)
static SituationError _SituationPerformHotReloadPass(void) {
#if defined(NDEBUG) && !defined(SITUATION_FORCE_HOTRELOAD)
    return SITUATION_SUCCESS;
#else
    if (!SituationIsInitialized()) {
        return SITUATION_SUCCESS;
    }
    SituationError first_err = SITUATION_SUCCESS;

    // [Optimized] The polling frequency is now controlled by the caller (IO Thread)
    // using pool->hot_reload_rate.

    // 1. Shaders
    for (int i = 0; i < SITUATION_MAX_SHADERS; i++) {
        _SituationShaderSlot* slot = &sit_render.shader_registry[i];
        if (slot->is_active && slot->vs_path && slot->fs_path) {
            long vs = SituationGetFileModTime(slot->vs_path);
            long fs = SituationGetFileModTime(slot->fs_path);
            if (vs != slot->vs_mod_time || fs != slot->fs_mod_time) {
                printf("[Situation] Hot-Reloading Shader %d...\n", i);
                SituationShader handle = { (uint32_t)i, slot->generation };
#if defined(SITUATION_USE_OPENGL)
                char* vs_src = SituationLoadFileText(slot->vs_path);
                char* fs_src = SituationLoadFileText(slot->fs_path);
                if (vs_src && fs_src) {
                    slot->vs_mod_time = vs;
                    slot->fs_mod_time = fs;
                    if (!slot->gl_is_linking) {
                        SituationError err = SITUATION_SUCCESS;
                        GLuint pending = _SituationCreateGLShaderProgramAsync(vs_src, fs_src, &err);
                        if (pending) {
                            slot->gl_pending_program_id = pending;
                            slot->gl_is_linking = true;
                        } else {
                            printf("[Situation] Hot-Reload Compile Failed for shader %d (GL)\n", i);
                            if (first_err == SITUATION_SUCCESS) {
                                first_err = (err != SITUATION_SUCCESS) ? err : SituationGetLastErrorCode();
                            }
                        }
                    }
                    SIT_FREE(vs_src); SIT_FREE(fs_src);
                }
#else
                /* Vulkan: Phase 3 in-place bundle swap (content_hash no-op when SPIR-V unchanged). */
                SituationError err = SituationReloadShader(&handle);
                if (err != SITUATION_SUCCESS) {
                    fprintf(stderr, "[Situation] Hot-reload failed for shader %d ('%s' / '%s')\n",
                        i, slot->vs_path, slot->fs_path);
                    if (first_err == SITUATION_SUCCESS) {
                        first_err = err;
                    }
                }
#endif
            }
        }
    }

    // 2. Textures
    for (int i = 0; i < SITUATION_MAX_TEXTURES; i++) {
        _SituationTextureSlot* slot = &sit_render.texture_registry[i];
        if (slot->is_active && slot->source_path) {
            long mod = SituationGetFileModTime(slot->source_path);
            if (mod != slot->mod_time) {
                printf("[Situation] Hot-Reloading Texture %d...\n", i);
                slot->mod_time = mod;

                SituationImage img;
                SituationError img_err = SituationLoadImage(slot->source_path, &img);
                if (img_err == SITUATION_SUCCESS) {
                    SituationTexture temp;
                    SituationError tex_err = SituationCreateTexture(img, true, &temp);
                    if (tex_err == SITUATION_SUCCESS) {
                        _SituationTextureSlot* new_slot = _SitGetTextureSlot(temp);
                        if (new_slot) {
                            // Swap internals
                            #if defined(SITUATION_USE_OPENGL)
                            _SitGLDeferDestroyTexture(slot->gl_texture_id);
                            slot->gl_texture_id = new_slot->gl_texture_id;
                            #elif defined(SITUATION_USE_VULKAN)
                            _SituationDeferDestroyImage(slot->image, slot->allocation, slot->image_view, slot->sampler);
                            slot->image = new_slot->image;
                            slot->allocation = new_slot->allocation;
                            slot->image_view = new_slot->image_view;
                            slot->sampler = new_slot->sampler;
                            #endif
                            slot->width = new_slot->width;
                            slot->height = new_slot->height;

                            new_slot->is_active = false;
                        }
                    } else if (first_err == SITUATION_SUCCESS) {
                        first_err = tex_err;
                    }
                    SituationUnloadImage(img);
                } else if (first_err == SITUATION_SUCCESS) {
                    first_err = img_err;
                }
            }
        }
    }

    // 3. Audio
    mtx_lock(&sit_audio.pool_mutex);
    for (int i = 0; i < SITUATION_MAX_LOADED_SOUNDS; i++) {
        _SituationSoundSlot* slot = &sit_audio.sound_pool[i];
        if (slot->is_active && slot->source_path) {
            long mod = SituationGetFileModTime(slot->source_path);
            if (mod != slot->mod_time) {
                printf("[Situation] Hot-Reloading Audio %d...\n", i);
                slot->mod_time = mod;

                // Stop playback
                SituationSound handle = { (uint32_t)i, slot->generation };
                SituationStopLoadedSound(&handle);

                // Reload
                _SituationSound* sound = &slot->sound_data;
                if (sound->is_preloaded && sound->preloaded_data) ma_free(sound->preloaded_data, NULL);
                if (sound->is_initialized) ma_decoder_uninit(&sound->decoder);

                // Re-load logic (inline simplified)
                // Assuming same mode (AUTO logic inside LoadSoundFromFile)
                // We reuse SituationLoadSoundFromFile but targeting a temp slot first?
                // Or just re-run load logic on this slot.
                // Re-running logic is safer.

                // Actually, SituationLoadSoundFromFile allocates a NEW slot.
                // So we should do:
                SituationSound temp_handle;
                SituationError snd_err = SituationLoadSoundFromFile(
                    slot->source_path, SITUATION_AUDIO_LOAD_AUTO, sound->is_looping, &temp_handle);
                if (snd_err == SITUATION_SUCCESS) {
                    _SituationSoundSlot* new_slot = _SitGetSoundSlot(temp_handle);
                    if (new_slot) {
                        // Move data from new_slot to current slot
                        // We must preserve existing volume/pan/effects?
                        // Or just reset?
                        // Preserve:
                        float vol = atomic_load(&sound->volume);
                        float pan = atomic_load(&sound->pan);

                        // Overwrite sound data
                        slot->sound_data = new_slot->sound_data;

                        // Restore state
                        atomic_store(&slot->sound_data.volume, vol);
                        atomic_store(&slot->sound_data.pan, pan);

                        // Free new slot
                        new_slot->is_active = false;
                    }
                } else if (first_err == SITUATION_SUCCESS) {
                    first_err = snd_err;
                }
            }
        }
    }
    mtx_unlock(&sit_audio.pool_mutex);

    return first_err;
#endif
}


SITAPI SituationError SituationCheckHotReloads(void) {
    // Logic moved to I/O thread.
    return SITUATION_SUCCESS;
}

#if !defined(__STDC_NO_THREADS__)

// Helper to flush resources for a specific frame index (or global for GL)
static SituationError _SitFlushFrameResources(int frame_index) {
    if (frame_index < 0 || frame_index >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return _SituationSetErrorFromCode(
            SITUATION_ERROR_INVALID_PARAM, "frame_index out of range for graveyard flush");
    }
    #if defined(SITUATION_USE_OPENGL)
        _SitGLFlushGraveyard(frame_index);
    #elif defined(SITUATION_USE_VULKAN)
        _SituationFlushGraveyard((uint32_t)frame_index);
    #endif
    return SITUATION_SUCCESS;
}

static void _SituationFlushRenderThread(void) {
#if defined(SITUATION_ENABLE_RENDER_THREAD)
    if (sit_render.enabled) {
        mtx_lock(&sit_render.render_queue_mutex);
        while (sit_render.frames_pending > 0) {
            cnd_wait(&sit_render.main_wait_cv, &sit_render.render_queue_mutex);
        }
        mtx_unlock(&sit_render.render_queue_mutex);
    }
#endif

    // [FIX] Ensure GPU is completely idle before CPU readbacks to prevent race conditions.
#if defined(SITUATION_USE_VULKAN)
    if (sit_render.vk.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(sit_render.vk.device);
    }
#elif defined(SITUATION_USE_OPENGL)
    // For OpenGL, wait on the most recently submitted frame fence
    int last_frame = (sit_render.current_frame_index + SITUATION_MAX_FRAMES_IN_FLIGHT - 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;
    if (sit_render.gl.frame_fences[last_frame]) {
        glClientWaitSync(sit_render.gl.frame_fences[last_frame], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
    }
    glFlush();
#endif
}

#if defined(SITUATION_USE_OPENGL)
/** Last frame slot handed to the render thread (after EndFrame index advance). */
static inline int _SituationGLLastSubmittedFrameIndex(void) {
    return (sit_render.current_frame_index + SITUATION_MAX_FRAMES_IN_FLIGHT - 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;
}

static bool _SituationGLShouldCaptureFrame(int frame_index) {
    if (frame_index < 0 || frame_index >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return false;
    }
    if (sit_render.gl.screenshot_request_pending[frame_index]) {
        return true;
    }
    if (atomic_load_explicit(&sit_render.gl.screenshot_urgent[frame_index], memory_order_acquire) != 0) {
        return true;
    }
    return false;
}

static void _SituationGLHandoffScreenshotRequestForSlot(int frame_index) {
    if (frame_index < 0 || frame_index >= SITUATION_MAX_FRAMES_IN_FLIGHT) {
        return;
    }
    if (sit_render.gl.screenshot_requested) {
        sit_render.gl.screenshot_request_pending[frame_index] = true;
        sit_render.gl.screenshot_requested = false;
    }
}

static void _SituationGLClearScreenshotCaptureFlags(int frame_index) {
    if (frame_index >= 0 && frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
        sit_render.gl.screenshot_request_pending[frame_index] = false;
        atomic_store_explicit(&sit_render.gl.screenshot_urgent[frame_index], 0, memory_order_release);
    }
}

static bool _SituationGLEnsureScreenshotResources(int sw, int sh) {
    if (sw < 1 || sh < 1) {
        return false;
    }
    size_t needed = (size_t)sw * (size_t)sh * 4u;
    if (sit_render.gl.screenshot_width != sw || sit_render.gl.screenshot_height != sh || !sit_render.gl.screenshot_buffer) {
        if (sit_render.gl.screenshot_buffer) {
            SIT_FREE(sit_render.gl.screenshot_buffer);
        }
        sit_render.gl.screenshot_buffer = (uint8_t*)SIT_MALLOC(needed);
        sit_render.gl.screenshot_width = sw;
        sit_render.gl.screenshot_height = sh;
    }
    if (!sit_render.gl.screenshot_buffer) {
        return false;
    }
    if (sit_render.gl.screenshot_pbo == 0u) {
        glGenBuffers(1, &sit_render.gl.screenshot_pbo);
    }
    if (sit_render.gl.screenshot_pbo != 0u) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, sit_render.gl.screenshot_pbo);
        glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)needed, NULL, GL_STREAM_READ);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
    return sit_render.gl.screenshot_pbo != 0u;
}

static void _SituationGLReadPixelsToPackBuffer(int sw, int sh, bool from_front_buffer) {
    if (sw < 1 || sh < 1 || sit_render.gl.screenshot_pbo == 0u) {
        return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(from_front_buffer ? GL_FRONT : GL_BACK);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, sit_render.gl.screenshot_pbo);
    glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glFlush();
}

static bool _SituationGLMapPackBufferToScreenshot(int sw, int sh, int frame_index) {
    if (sw < 1 || sh < 1 || !sit_render.gl.screenshot_buffer || sit_render.gl.screenshot_pbo == 0u) {
        sit_render.gl.screenshot_valid = false;
        return false;
    }
    size_t needed = (size_t)sw * (size_t)sh * 4u;
    glFinish();
    glBindBuffer(GL_PIXEL_PACK_BUFFER, sit_render.gl.screenshot_pbo);
    void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)needed, GL_MAP_READ_BIT);
    if (mapped) {
        memcpy(sit_render.gl.screenshot_buffer, mapped, needed);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        const bool ok = (glGetError() == GL_NO_ERROR);
        while (glGetError() != GL_NO_ERROR) { /* drain — Track D */ }
        if (ok) {
            sit_render.gl.screenshot_resolved_frame_index = frame_index;
            sit_render.gl.screenshot_buffer_epoch = sit_render.gl.screenshot_capture_epoch;
            if (sit_render.gl.screenshot_mutex_initialized) {
                mtx_lock(&sit_render.gl.screenshot_mutex);
            }
            atomic_thread_fence(memory_order_release);
            sit_render.gl.screenshot_valid = true;
            if (sit_render.gl.screenshot_mutex_initialized) {
                mtx_unlock(&sit_render.gl.screenshot_mutex);
            }
        } else {
            sit_render.gl.screenshot_valid = false;
        }
        return ok;
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    _SituationGLSyncReadFramebufferToCPU(sw, sh, true, frame_index);
    return sit_render.gl.screenshot_valid;
}

static void _SituationGLSyncReadFramebufferToCPU(int sw, int sh, bool from_front_buffer, int frame_index) {
    if (!_SituationGLEnsureScreenshotResources(sw, sh)) {
        sit_render.gl.screenshot_valid = false;
        return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(from_front_buffer ? GL_FRONT : GL_BACK);
    glFinish();
    glReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, sit_render.gl.screenshot_buffer);
    const bool ok = (glGetError() == GL_NO_ERROR);
    while (glGetError() != GL_NO_ERROR) { /* drain — Track D */ }
    if (ok) {
        if (frame_index >= 0) {
            sit_render.gl.screenshot_resolved_frame_index = frame_index;
        }
        sit_render.gl.screenshot_buffer_epoch = sit_render.gl.screenshot_capture_epoch;
        if (sit_render.gl.screenshot_mutex_initialized) {
            mtx_lock(&sit_render.gl.screenshot_mutex);
        }
        atomic_thread_fence(memory_order_release);
        sit_render.gl.screenshot_valid = true;
        if (sit_render.gl.screenshot_mutex_initialized) {
            mtx_unlock(&sit_render.gl.screenshot_mutex);
        }
    } else {
        sit_render.gl.screenshot_valid = false;
    }
}

/** Legacy sync capture (TakeScreenshot fallback / explicit sync read). */
static void _SituationGLCaptureDisplayedFramebuffer(int sw, int sh, bool from_front_buffer) {
    if (sw < 1 || sh < 1) {
        sit_render.gl.screenshot_valid = false;
        return;
    }
    _SituationGLSyncReadFramebufferToCPU(sw, sh, from_front_buffer, -1);
}
#endif

/**
 * @brief [INTERNAL] Entry point for the dedicated render thread.
 *
 * @details This function runs in a separate thread and is responsible for consuming
 *          queued frame indices from the main thread, executing the corresponding
 *          soft command buffers, submitting work to the GPU (Vulkan) or swapping
 *          buffers (OpenGL), handling presentation, and managing per-frame resource
 *          cleanup via graveyards/fences.
 *
 *          The thread uses a condition variable + mutex to wait efficiently when no
 *          frames are pending. It exits cleanly when `thread_shutdown_req` is set
 *          and the queue is empty.
 *
 *          Key responsibilities:
 *            - Acquires the OpenGL context (if applicable) after main thread release
 *            - Dequeues frame indices from the circular render queue
 *            - Executes backend-specific rendering (command buffer playback, submit, present)
 *            - Tracks and flushes deferred resource destruction (graveyards)
 *            - Updates frame metrics/latency when metrics are enabled
 *            - Signals the main thread when a frame slot becomes free
 *
 *          Thread safety invariants:
 *            - Only this thread accesses the backend command buffers / swapchain / context
 *            - Queue access is protected by `render_queue_mutex`
 *            - Frame refcounts are managed atomically
 *            - Graveyard flushes happen after GPU work completion (fences/sync objects)
 *
 * @param arg Unused thread argument (always NULL in current usage)
 * @return 0 on clean exit (standard thread return value)
 *
 * @note Called via thrd_create() during SituationInit().
 *       Main thread must release the OpenGL context before the render thread starts.
 *       See also: render_queue_cv, render_queue_mutex, thread_shutdown_req,
 *                 frame_refcounts[], _SitFlushFrameResources()
 */
 static int _SituationRenderThreadEntry(void* arg) {
    (void)arg;
    #ifdef _WIN32
    DWORD tid = GetCurrentThreadId();
    #else
    pthread_t tid = pthread_self();
    #endif
    fprintf(stderr, "[Situation] [Thread %lu] RENDER THREAD STARTED\n", (unsigned long)tid); fflush(stderr);

    _SituationSetCurrentThreadName("Sit Render");

    // Pin render thread (default: logical core 1; override via SituationInitInfo::thread_affinity_render)
    {
        uint64_t render_aff = SituationGetConfiguredRenderThreadAffinity();
        _SituationSetThreadAffinityForRole(SIT_THREAD_ROLE_RENDER, render_aff);
        _SituationObservabilityRecordRenderThread(render_aff);
    }
	
    // [OpenGL] We must acquire the context here.
    // Note: Initialization (SituationInit) happens on Main.
    // The Main thread must release the context (glfwMakeContextCurrent(NULL)) before triggering this thread.
    #if defined(SITUATION_USE_OPENGL)
    // Wait for Main thread to release context (prevents race condition)
    // This explicit synchronization ensures the context handoff is safe.
    while (!atomic_load(&sit_render.gl_context_released)) {
        thrd_yield(); // Yield CPU while waiting
    }
    if (sit_gs.sit_glfw_window) {
        glfwMakeContextCurrent(sit_gs.sit_glfw_window);
    }
    #ifndef SITUATION_USE_VULKAN
    // Set initial swap interval on the thread that owns the context
    if (sit_gs.active_profile_window_flags & SITUATION_FLAG_VSYNC_HINT) {
        glfwSwapInterval(1);
        sit_render.gl.last_applied_swap_interval = 1;
    } else {
        glfwSwapInterval(0);
        sit_render.gl.last_applied_swap_interval = 0;
    }
    #endif
    #endif

    fprintf(stderr, "[Situation] [Thread %lu] RENDER THREAD entering main loop\n", (unsigned long)tid); fflush(stderr);
    while (true) {
        mtx_lock(&sit_render.render_queue_mutex);

        // Wait for work or shutdown (timed wait to guarantee shutdown responsiveness)
        // [FIX] Test occupancy via render_queue_count, NOT head == tail: when MAX_FRAMES_IN_FLIGHT
        // frames are queued back-to-back before we dequeue, head wraps onto tail and a FULL queue
        // looks empty — the thread would sleep forever while the main thread's fences never signal
        // (15s acquire timeouts). Count is guarded by render_queue_mutex, so this stays race-free.
        while (sit_render.render_queue_count == 0 && !sit_render.thread_shutdown_req) {
            struct timespec ts;
            timespec_get(&ts, TIME_UTC);
            ts.tv_nsec += 1000000; // 1ms timeout (smaller to avoid occasional long sleeps causing stutters)
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }
            cnd_timedwait(&sit_render.render_queue_cv, &sit_render.render_queue_mutex, &ts);
        }

        // Shutdown Check
        if (sit_render.thread_shutdown_req && sit_render.render_queue_count == 0) {
            mtx_unlock(&sit_render.render_queue_mutex);
            break;
        }

        // Dequeue Frame Index
        int frame_index = sit_render.render_queue[sit_render.render_queue_tail];
        sit_render.render_queue_tail = (sit_render.render_queue_tail + 1) % SITUATION_MAX_FRAMES_IN_FLIGHT;
        sit_render.render_queue_count--;

        // [Metrics]
        #if defined(SITUATION_ENABLE_RENDER_THREAD)
        atomic_fetch_sub(&sit_render.render_queue_depth, 1);
        #endif

        // Note: We do NOT decrement frames_pending here. We are still "working" on this frame.
        // We decrement it only after we are fully done rendering.

        mtx_unlock(&sit_render.render_queue_mutex);

        // --- EXECUTE FRAME ---
        SIT_PROF_ZONE_SCOPED("RenderThread/Frame") {
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [RENDER THREAD] Processing frame_index=%d\n", frame_index);
        fflush(stdout);
        #endif

        #if defined(SITUATION_USE_OPENGL)

        // 1. Wait for OLD frame to finish and flush its graveyard (Vulkan Parity)
        // This ensures the GPU isn't still reading buffers we are about to overwrite/delete.
        if (sit_render.gl.frame_fences[frame_index]) {
            GLenum wait_result = glClientWaitSync(
                sit_render.gl.frame_fences[frame_index], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
            if (wait_result == GL_TIMEOUT_EXPIRED) {
                _SituationSetErrorFromCode(
                    SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT,
                    "Render thread: GL frame fence wait timed out.");
            } else if (wait_result == GL_WAIT_FAILED) {
                _SituationSetErrorFromCode(
                    SITUATION_ERROR_OPENGL_GENERAL, "Render thread: GL frame fence wait failed.");
            }

            (void)_SitFlushFrameResources(frame_index);

            glDeleteSync(sit_render.gl.frame_fences[frame_index]);
            sit_render.gl.frame_fences[frame_index] = 0;
        }

        _SitGpuProfReadbackFrame(frame_index);
        _SitGpuProfClearFrameSlot((uint32_t)frame_index);

        // 2. Execute Soft Command Buffer
        {
            SituationError exec_err = _SituationGLExecuteCommands(
                &sit_render.gl.soft_buffers[frame_index], frame_index);
            if (exec_err != SITUATION_SUCCESS) {
                char* err_msg = NULL;
                (void)SituationGetLastErrorMsg(&err_msg);
                fprintf(stderr, "[Situation] Render thread GL execute failed: %d%s%s\n",
                    (int)exec_err,
                    (err_msg && err_msg[0]) ? " — " : "",
                    (err_msg && err_msg[0]) ? err_msg : "");
                if (err_msg) {
                    free(err_msg);
                }
                fflush(stderr);
            }
        }
        _SitGLEnsureDefaultFramebufferOpaqueAlpha();

        _SituationGLBlitCanvasToDisplay();

        // [Phase 1] Pre-swap screenshot capture on demand (requested / urgent latch)
        {
            int sw = SituationGetRenderWidth();
            int sh = SituationGetRenderHeight();
            bool do_capture = _SituationGLShouldCaptureFrame(frame_index);
            if (!do_capture) {
                /* Main may set screenshot_urgent immediately after EndFrame returns (EndFrame→Load). */
                for (int urgent_wait = 0; urgent_wait < 512; ++urgent_wait) {
                    if (_SituationGLShouldCaptureFrame(frame_index)) {
                        do_capture = true;
                        break;
                    }
                    thrd_yield();
                }
            }
            if (do_capture && sw > 0 && sh > 0 && _SituationGLEnsureScreenshotResources(sw, sh)) {
                _SituationGLReadPixelsToPackBuffer(sw, sh, false);
            }

            // 3. Present
            uint64_t pres_t0 = _SitGetMonotonicTimeNS();
            _SitGLApplySwapIntervalBeforePresent();
            glfwSwapBuffers(sit_gs.sit_glfw_window);
            sit_gs.last_present_ns = _SitGetMonotonicTimeNS() - pres_t0;
#if defined(SITUATION_ENABLE_RENDER_THREAD)
            _SituationPublishPresentTimingFromRenderThread();
#endif

            if (do_capture && sw > 0 && sh > 0) {
                (void)_SituationGLMapPackBufferToScreenshot(sw, sh, frame_index);
                _SituationGLClearScreenshotCaptureFlags(frame_index);
            } else if (sw > 0 && sh > 0) {
                /* EndFrame→Load: urgent may arrive after pre-swap; brief post-present poll. */
                for (int late_wait = 0; late_wait < 256; ++late_wait) {
                    if (atomic_load_explicit(&sit_render.gl.screenshot_urgent[frame_index], memory_order_acquire) != 0) {
                        _SituationGLSyncReadFramebufferToCPU(sw, sh, true, frame_index);
                        _SituationGLClearScreenshotCaptureFlags(frame_index);
                        break;
                    }
                    thrd_yield();
                }
            }
        }

        // 4. Create NEW fence to track the commands we just submitted
        sit_render.gl.frame_fences[frame_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush(); // Ensure the fence is pushed to the GPU queue

        #elif defined(SITUATION_USE_VULKAN)
        VkCommandBuffer cmd = sit_render.vk.command_buffers[frame_index];

        // 1. Submit
        uint64_t ex_t0 = _SitGetMonotonicTimeNS();
        VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        // [v2.3.24b] Sync Logic
        VkSemaphore wait_semaphores[2];
        VkPipelineStageFlags wait_stages[2];
        uint32_t wait_count = 0;

        // Always wait for image available (Color Output)
        wait_semaphores[wait_count] = sit_render.vk.image_available_semaphores[frame_index];
        wait_stages[wait_count] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        wait_count++;

        // Conditionally wait for compute (Draw Indirect / Vertex Input)
        if (frame_index >= 0 && frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT &&
            sit_render.vk.needs_compute_wait[frame_index]) {
            wait_semaphores[wait_count] = sit_render.vk.compute_finished_semaphores[frame_index];
            wait_stages[wait_count] = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
            wait_count++;
            sit_render.vk.needs_compute_wait[frame_index] = false;
        }

        submit_info.waitSemaphoreCount = wait_count;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        VkSemaphore signal_semaphores[] = { sit_render.vk.render_finished_semaphores[frame_index] };
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        // Use the fence associated with this frame index
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [RENDER THREAD] About to submit frame_index=%d to GPU\n", frame_index);
        printf("Situation [Vulkan Debug]: [RENDER THREAD]   Command buffer: %p\n", (void*)cmd);
        printf("Situation [Vulkan Debug]: [RENDER THREAD]   Queue: %p\n", (void*)sit_render.vk.graphics_queue);
        fflush(stdout);
        #endif
        VkResult submit_result = vkQueueSubmit(sit_render.vk.graphics_queue, 1, &submit_info, sit_render.vk.in_flight_fences[frame_index]);
        #ifdef SITUATION_VULKAN_DEBUG
        printf("Situation [Vulkan Debug]: [RENDER THREAD] vkQueueSubmit result: %d (VK_SUCCESS=0)\n", submit_result);
        fflush(stdout);
        #endif
        sit_gs.last_execute_ns = _SitGetMonotonicTimeNS() - ex_t0;
        bool ready_to_present = (submit_result == VK_SUCCESS);
        if (!ready_to_present) {
            if (frame_index < SITUATION_MAX_FRAMES_IN_FLIGHT) {
                sit_render.vk.screenshot_copy_pending[frame_index] = false;
            }
            sit_render.vk.screenshot_valid = false;
            _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED, "RenderThread: Failed to submit draw command buffer!");
            _SituationVulkanResignalFrameFence((uint32_t)frame_index);
        }
        /* No fence wait before present. With max_frames_in_flight < swapchain_image_count,
         * acquire always has a free image. The fence is waited on in AcquireFrameCommandBuffer
         * when the slot is reused — that's the only place CPU needs to know GPU is done.
         * Present synchronizes on the GPU side via render_finished_semaphores. */

        // 2. Present (skip when submit/GPU-wait failed — present would block on unsignaled semaphore)
        if (ready_to_present && sit_render.vk.swapchain_valid && sit_render.vk.swapchain != VK_NULL_HANDLE) {
            uint64_t pres_t0 = _SitGetMonotonicTimeNS();
            VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores = signal_semaphores;
            VkSwapchainKHR swapchains[] = { sit_render.vk.swapchain };
            present_info.swapchainCount = 1;
            present_info.pSwapchains = swapchains;
            present_info.pImageIndices = &sit_render.vk.acquired_image_indices[frame_index];

            VkResult result = vkQueuePresentKHR(sit_render.vk.present_queue, &present_info);
            sit_gs.last_present_ns = _SitGetMonotonicTimeNS() - pres_t0;

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                atomic_store(&sit_render.vk.recreate_swapchain_request, true);
            } else if (result != VK_SUCCESS) {
                _SituationSetErrorFromCode(SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED, "RenderThread: Failed to present swap chain image!");
            } else {
                sit_render.vk.last_presented_image_index = sit_render.vk.acquired_image_indices[frame_index];
#if defined(SITUATION_ENABLE_RENDER_THREAD)
                _SituationPublishPresentTimingFromRenderThread();
#endif
            }
        } else {
            sit_gs.last_present_ns = 0;
        }
        #endif

        // [v2.3.22] Metrics: Record Latency
        #if defined(SITUATION_ENABLE_RENDER_THREAD)
        uint64_t submit_ts = atomic_load(&sit_render.submit_timestamps[frame_index]);
        if (submit_ts > 0) {
            uint64_t now = _SitGetMonotonicTimeNS();

            // [v2.3.25] Drift Check with Once-Warn
            uint64_t latency = 0;
            if (now < submit_ts) {
                #ifndef NDEBUG
                if (!atomic_exchange(&sit_render.drift_warned, true)) {
                    fprintf(stderr, "[METRIC] First drift; clamped 0.\n");
                }
                #endif
                latency = 0;
            } else {
                latency = now - submit_ts;
            }

            // [v2.4.319] Windowed max latency metric — periodic reset prevents permanent
            // SLEEP lock-in from transient spikes (startup, alt-tab, DWM stalls).
            uint64_t global_max = atomic_load(&sit_render.metric_max_latency_ns);
            int retries = 0;
            while (latency > global_max && !atomic_compare_exchange_weak(&sit_render.metric_max_latency_ns, &global_max, latency)) {
                 retries++;
                 if (retries > 20) { break; }
            }

            // Periodic reset: after SIT_LATENCY_METRIC_WINDOW_FRAMES samples, zero the max
            // so the adaptive backpressure policy can recover from one-off spikes.
            uint32_t wcount = (uint32_t)atomic_fetch_add(&sit_render.metric_window_frame_count, 1) + 1;
            if (wcount >= SIT_LATENCY_METRIC_WINDOW_FRAMES) {
                atomic_store(&sit_render.metric_max_latency_ns, 0);
                atomic_store(&sit_render.metric_window_frame_count, 0);
            }

            atomic_fetch_add(&sit_render.metric_latency_sum_ns, latency);
            atomic_fetch_add(&sit_render.metric_latency_count, 1);
        }
        #endif

        // Refcount (Unify)
        if (atomic_fetch_sub(&sit_render.frame_refcounts[frame_index], 1) == 1) {
            // Refcount reached 0 (fetch_sub returned 1). Safe to recycle.
            SituationError flush_err = _SitFlushFrameResources(frame_index);
            if (flush_err != SITUATION_SUCCESS) {
                fprintf(stderr, "[Situation] Render thread graveyard flush failed: %d\n", (int)flush_err);
                fflush(stderr);
            }
        }

        // --- FRAME COMPLETE ---
        mtx_lock(&sit_render.render_queue_mutex);

        sit_render.frames_pending--; // Slot `frame_index` is now free.

        // [CRITICAL FIX] Signal the Main Thread!
        // This wakes up SituationEndFrame if it is waiting in the cnd_wait loop above.
        cnd_signal(&sit_render.main_wait_cv);

        // Wake Main Thread if it was blocked on full queue (Redundant but safe signal)
        // cnd_signal(&sit_render.main_wait_cv); // Already done above

        mtx_unlock(&sit_render.render_queue_mutex);

        } /* SIT_PROF_ZONE_SCOPED RenderThread/Frame */
    }

    #if defined(SITUATION_USE_OPENGL)
    // Release context before exiting, just to be clean.
    glfwMakeContextCurrent(NULL);
    #endif

    // [v2.3.22] Mark thread as inactive for timeout join polling
    atomic_store(&sit_render.thread_active, false);

    return 0;
}
#endif

#endif // SITUATION_IMPL_RENDERER_LC_H
