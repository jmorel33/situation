/***************************************************************************************************
*
*   situation_api_config.h - Compile Limits, Init Flags, and Frame Macros
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Compile-time constants (SITUATION_MAX_*), init/window flags, SITUATION_BEGIN_FRAME,
*   Vulkan timeout budgets, and overridable SIT_MALLOC / SIT_FREE allocators.
*
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_CONFIG_H
#define SITUATION_API_CONFIG_H

// --- New Convenience Macro for Safe Main Loop ---
// Ensures inputs are polled and timers updated at the exact start of the frame.
#define SITUATION_BEGIN_FRAME() \
    SituationPollInputEvents(); \
    SituationUpdateTimers();

// --- Config Flags ---
#define SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD 0x00000001 // Process audio capture callbacks on main thread


/**
  * @brief Configuration Defines
  *
  * These macros define compile-time constants and limits for the library's internal structures and behaviors.
  * They control resource capacities, buffer sizes, and default thresholds across subsystems. Adjust via
  * preprocessor overrides before including `situation.h` if needed (e.g., for embedded targets).
  * Defaults are tuned for desktop performance; lower for mobile/constrained environments.
  *
  * @note Increasing limits may raise memory usage; decreasing may cap features (e.g., concurrent sounds).
  * @see SituationInitInfo for runtime tunables.
  */

/* === General System Limits === */
#define SITUATION_VK_STAGING_BUFFER_SIZE 		(128 * 1024 * 1024) /* 128 MB — great for large texture/model uploads in Vulkan */
#define SITUATION_GL_RING_SIZE                  (64 * 1024 * 1024) /* 64MB Persistent Ring Buffer for OpenGL Zero-Copy updates */
#define SITUATION_MAX_FRAMES_IN_FLIGHT          6    /* Max overlapping frames for VK/GL swapchains. 3 for low latency VSync; higher for uncapped high-FPS (more memory per-frame resources). */
#ifndef SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS
#define SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS     1000000000ULL /* Overall budget for pumped vkAcquireNextImageKHR (stepped + polls). Avoids indefinite wait. Timeout results in frame skip (normal under VSync or load). */
#endif
#ifndef SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS
#define SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS 15000000000ULL /* Total nanoseconds for chunked vkWaitForFences (+ glfwPollEvents) per fence; avoids infinite stall / Windows "Not responding". */
#endif
#ifndef SITUATION_VULKAN_SHUTDOWN_FENCE_WAIT_NS
#define SITUATION_VULKAN_SHUTDOWN_FENCE_WAIT_NS 3000000000ULL /* Per in-flight fence: chunked vkWaitForFences + glfwPollEvents anywhere we would have used vkDeviceWaitIdle (shutdown, swapchain teardown/recreate, VSync/full cleanup, init error path). vkDeviceWaitIdle blocks the main thread with no event pump and never returns if the GPU is wedged → frozen pale window. */
#endif
#ifndef SITUATION_VULKAN_ASYNC_COMPILE_DEADLINE_NS
#define SITUATION_VULKAN_ASYNC_COMPILE_DEADLINE_NS 5000000000ULL /* Wall-clock deadline for an async shaderc compile, enforced by SituationPollShaderLoad. A compile that exceeds it is abandoned and reported as SITUATION_ERROR_SHADER_COMPILE_TIMEOUT instead of returning SHADER_LOAD_IN_PROGRESS forever. Typical compiles are ~300ms; 5s is a wide margin for slow machines. */
#endif
#ifndef SITUATION_VULKAN_ASYNC_UNLOAD_COOPERATIVE_NS
#define SITUATION_VULKAN_ASYNC_UNLOAD_COOPERATIVE_NS 500000000ULL /* Unload: cooperative wait for a normal in-flight shaderc compile (~300ms typical) before escalating to abandon. */
#endif
#ifndef SITUATION_VULKAN_ASYNC_UNLOAD_ABANDON_NS
#define SITUATION_VULKAN_ASYNC_UNLOAD_ABANDON_NS 2000000000ULL /* Unload: abandon wedged {0,-3} compiles and retire the pool job slot. Normal unload must not reach UNLOAD_WAIT_NS. */
#endif
#ifndef SITUATION_VULKAN_ASYNC_UNCLAIMED_FAST_NS
#define SITUATION_VULKAN_ASYNC_UNCLAIMED_FAST_NS 100000000ULL /* Job submitted but never claimed: fast-path threshold for retire+inline (Phase B). */
#endif
#ifndef SITUATION_VULKAN_ASYNC_SHUTDOWN_NS
#define SITUATION_VULKAN_ASYNC_SHUTDOWN_NS 500000000ULL /* Destroy/shutdown: short budget before abandoning in-flight async compiles. */
#endif
#ifndef SITUATION_VULKAN_ASYNC_UNLOAD_WAIT_NS
#define SITUATION_VULKAN_ASYNC_UNLOAD_WAIT_NS 10000000000ULL /* Last-resort shutdown-only wall-clock cap. Normal SituationUnloadShader abandon uses UNLOAD_ABANDON_NS (2s). */
#endif
#ifndef SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS
#define SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS 100 /* Log vkAcquire timing to stderr if acquire >= this many ms, or on TIMEOUT. Use 0 to log every acquire (verbose). */
#endif
#ifndef SITUATION_VULKAN_ACQUIRE_SWAPCHAIN_RETRIES
#define SITUATION_VULKAN_ACQUIRE_SWAPCHAIN_RETRIES 4 /* Internal retries after successful swapchain recreation (resize/sync/out-of-date). */
#endif
#define SITUATION_MAX_STORAGE_DEVICES           8    /* Max detected storage volumes (e.g., drives, mounts). */
#define SITUATION_MAX_NETWORK_ADAPTERS          8    /* Max network interfaces (e.g., Ethernet/Wi-Fi). */
#define SITUATION_MAX_DEVICE_NAME_LEN           128  /* Max length for device strings (e.g., GPU/CPU names). */
#define SITUATION_MAX_CPU_NAME_LEN              64   /* Max CPU model string length (e.g., "Intel i9-13900K"). */
#define SITUATION_MAX_LOGICAL_PROCESSORS        256  /* Max entries in SituationCpuTopology::processors */
#define SITUATION_AFFINITY_MASK_BITS            64   /* Bit width for SituationSetThreadAffinity / mask builders (low logical IDs) */
#ifndef SITUATION_MAX_THREAD_NAME_LEN
#define SITUATION_MAX_THREAD_NAME_LEN           24   /* Max UTF-8 chars stored for OS-visible thread names (Linux truncates to 15 at the kernel). */
#endif
#ifndef SITUATION_MAIN_THREAD_NAME_DEFAULT
#define SITUATION_MAIN_THREAD_NAME_DEFAULT      "Sit Main" /* Default OS-visible name for the main thread when SituationInitInfo::main_thread_name is NULL. */
#endif
#ifndef SITUATION_DEFAULT_APP_USER_MODEL_ID
#define SITUATION_DEFAULT_APP_USER_MODEL_ID     "Situation.Application" /* Win32 shell AppUserModelID when SituationInitInfo::app_user_model_id is NULL. */
#endif
#define SITUATION_MAX_NUMA_NODES                64   /* Max NUMA nodes in SituationNumaTopology */
#define SITUATION_MAX_GPU_NAME_LEN              128  /* Max GPU model string length (e.g., "NVIDIA RTX 4090"). */
#define SITUATION_MAX_MONITORS                  8    /* Max physical displays to track in device snapshot. */
#define SITUATION_MAX_MONITOR_NAME_LEN          128  /* Max monitor EDID name length (e.g., "Dell UltraSharp"). */
#define SITUATION_MAX_ERROR_MSG_LEN             16384 /* Max length for error messages and logs. */
#define SITUATION_MAX_SHADER_LOG_LEN            16384 /* Max length for shader compilation logs. */

/* === Graphics & Rendering Limits === */
#define SITUATION_MAX_VIRTUAL_DISPLAYS          16   /* Max offscreen render targets (e.g., for UI/post-fx). */
#define SITUATION_MAX_RENDER_TARGETS             64   /* Max user SituationRenderTarget offscreen FBOs (Phase 3c). */
#define SITUATION_MAX_QUERY_POOLS                32   /* Max user SituationQueryPool objects (P10.4). */
#define SITUATION_MAX_QUERIES_PER_POOL           64   /* Max queries per pool (P10.4). */
#define SIT_VD_MAX_COMPUTE_TEXTURE_BINDS         8    /* Tracked compute storage-image bindings for VD content hooks. */
#define SITUATION_MAX_TEXTURES                  4096
#define SITUATION_MAX_SHADERS                   1024
#define SITUATION_MAX_COMPUTE_PIPELINES         512
#define SITUATION_MAX_BUFFERS                   4096
#define SITUATION_MAX_MESHES                    4096
#define SITUATION_MAX_MODELS                    1024
#define SITUATION_MAX_RASTER_STACK_DEPTH        256  /* Max depth for SituationCmdPushRasterState / PopRasterState (GL + VK). */
#define SITUATION_MAX_BEHAVIOR_STACK_DEPTH       32  /* Max depth for SituationCmdPushRendererBehavior / PopRendererBehavior (GL + VK). */

/* === Audio Subsystem Limits === */
#define SITUATION_MAX_AUDIO_SOUNDS_QUEUED       32   /* Max concurrent sounds in mixing queue (e.g., SFX layers). */
#define SITUATION_MAX_LOADED_SOUNDS             1024 /* Max loaded sound assets */
#define SITUATION_MAX_TONES                     64   /* 64-voice polyphony for procedural synthesis. */
#define SITUATION_AUDIO_CALLBACK_TEMP_BUFFER_FRAMES 2048 /* Scratch frames for decode/effects/conversion (48kHz ~40ms). */

/* === Input Subsystem Limits === */
#define SITUATION_MAX_INPUT_DEVICES             16   /* Max tracked input devices (keyboards, mice, gamepads, etc.). */
#define SITUATION_KEY_QUEUE_MAX                 64   /* Max buffered keyboard events per frame (anti-loss ring). */
#define SITUATION_CHAR_QUEUE_MAX                64   /* Max buffered char inputs per frame (IME/text entry). */
#define SITUATION_MAX_SCANCODES                 512  /* Max number of scancodes to track (safe for most OS). */
#define SITUATION_MAX_JOYSTICKS                 2    /* Max tracked gamepads/joysticks (local co-op). */
#define SITUATION_MAX_JOYSTICK_BUTTONS          15   /* Standard gamepad buttons (A/B/X/Y, D-pad, bumpers, etc.). */
#define SITUATION_MAX_JOYSTICK_AXES             6    /* Standard gamepad axes (left/right sticks + triggers). */
#define SITUATION_JOYSTICK_DEADZONE_L           0.10f /* Default deadzone for left analog (anti-drift). */
#define SITUATION_JOYSTICK_DEADZONE_R           0.10f /* Default deadzone for right analog (anti-drift). */

/* Memory allocation macros (override before include for custom allocators). */
#include <stdlib.h>
#ifndef SIT_MALLOC
    #define SIT_MALLOC(sz) malloc(sz)
#endif
#ifndef SIT_CALLOC
    #define SIT_CALLOC(n, sz) calloc(n, sz)
#endif
#ifndef SIT_REALLOC
#ifdef __cplusplus
    #define SIT_REALLOC(p, sz) reinterpret_cast<std::remove_reference<decltype(p)>::type>(realloc(p, sz))
#else
    #define SIT_REALLOC(p, sz) realloc(p, sz)
#endif
#endif
#ifndef SIT_FREE
    #define SIT_FREE(p) do { if (p) free(p); p = NULL; } while(0)
#endif

#endif /* SITUATION_API_CONFIG_H */
