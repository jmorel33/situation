/***************************************************************************************************
*
*   -- The "Situation" Advanced Platform Awareness, Control, and Timing --
*   Core API library (see version in Version Macros)
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   A single-file, cross-platform C/C++ library providing unified, low-level access and control over essential application subsystems. Its purpose is to abstract away platform-specific complexities,
*   offering a lean yet powerful API for building sophisticated, high-performance software.
*
*   The library's philosophy is reflected in its name, granting developers complete situational "Awareness," precise "Control," and fine-grained "Timing."
*
*   **Velocity Module (Hot-Reloading):**
*   This release integrates the **Hot-Reloading Module**, a development-focused toolset that allows Shaders, Compute Pipelines, Textures, and 3D Models to be reloaded from disk at runtime.
*   This eliminates the need to restart the application to see asset changes, significantly increasing iteration speed for visual adjustments and shader programming.
*
*   It provides deep **Awareness** of the host system through APIs for querying hardware and multi-monitor display information, and by handling operating system events like window focus and file drops.
*
*   This foundation enables precise **Control** over the entire application stack, from window management (fullscreen, borderless) and input devices (keyboard, mouse, gamepad) to a comprehensive audio
*   pipeline with playback, capture, and real-time effects. This control extends to the graphics and compute pipeline, abstracting modern OpenGL and Vulkan through a unified command-buffer model.
*   It offers simplified management of GPU resources—such as shaders, meshes, and textures—and includes powerful utilities for high-quality text rendering and robust filesystem I/O.
*
*   Finally, its **Timing** capabilities range from high-resolution performance measurement and frame rate management to an advanced **Temporal Oscillator System** for creating complex, rhythmically
*   synchronized events. By handling the foundational boilerplate of platform interaction, "Situation" empowers developers to focus on core application logic, enabling the creation of responsive and
*   sophisticated software—from games and creative coding projects to data visualization tools—across all major desktop platforms.
*
***************************************************************************************************
*
*   License (MIT)
*   -------------
*   Copyright (c) 2025-2026 Jacques Morel
*
*   Permission is hereby granted, free of charge, to any person obtaining a copy
*   of this software and associated documentation files (the "Software"), to deal
*   in the Software without restriction, including without limitation the rights
*   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*   copies of the Software, and to permit persons to whom the Software is
*   furnished to do so, subject to the following conditions:
*
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*
 *   NOTE: This file is part of a split-header library.
 *   - Use `situation.h` as the primary entry point.
 *   - If you include this file directly, define SITUATION_IMPLEMENTATION to compile the implementation.
 *
***************************************************************************************************/

/*
 *  ---------------------------------------------------------------------------------------------------
 *  COMPILATION INSTRUCTIONS
 *  ---------------------------------------------------------------------------------------------------
 *
 *  GCC / Clang (Linux):
 *      gcc main.c -o situation_demo -std=c11 -I. -I./ext -D_POSIX_C_SOURCE=200809L \
 *      -lglfw -lGL -lm -ldl -lpthread
 *
 *  MinGW (Windows):
 *      gcc main.c -o situation_demo.exe -std=c11 -I. -I./ext \
 *      -lglfw3 -lgdi32 -lopengl32 -lwinmm -luser32 -lshell32 -lole32 -liphlpapi -lsetupapi \
 *      -ldxgi -lpropsys -lshlwapi -lm
 *
 *  Note: Ensure you link against GLFW3 and your system's OpenGL libraries.
 *        The 'ext' directory should contain the required dependencies (stb, glad, miniaudio).
 *
 *  Optional: define SITUATION_VERBOSE_DIAGNOSTICS (e.g. -DSITUATION_VERBOSE_DIAGNOSTICS) before including headers
 *  to enable extra renderer init/logging chatter on OpenGL and Vulkan (default builds stay quiet).
 */

#ifndef SITUATION_API_H
#define SITUATION_API_H

/*
 * Feature Test Macros (Strict C11 Support)
 * ----------------------------------------
 * When compiling with strict standard flags (e.g., -std=c11), compilers like GCC and Clang disable non-standard extensions by default. This hides common OS-level functions
 * that are part of POSIX but not ISO C.
 *
 * We define these macros to explicitly request POSIX.1-2008 and X/Open 7 support.
 * This exposes necessary APIs required by the implementation, specifically:
 * - Filesystem queries: stat(), S_ISREG, S_ISDIR
 * - System interaction: readlink(), nanosleep()
 *
 * NOTE: These must be defined before ANY system headers are included.
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

/**
 * @brief API Declaration Control (for Shared Library / DLL support)
 */
#if defined(_WIN32)
    #if defined(SITUATION_BUILD_SHARED)
        #define SITAPI __declspec(dllexport) // Exporting functions from the DLL
    #elif defined(SITUATION_USE_SHARED)
        #define SITAPI __declspec(dllimport) // Importing functions from the DLL
    #else
        #define SITAPI // Static library or header-only, no special declaration needed
    #endif
#else // On other platforms like Linux or macOS, attribute visibility is preferred
    #if defined(SITUATION_BUILD_SHARED)
        #define SITAPI __attribute__((visibility("default")))
    #else
        #define SITAPI // No special declaration needed for static or header-only
    #endif
#endif

// C++ linkage guard: Ensures C++ compilers don't mangle SITAPI function names
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The public-facing macro. It now ALWAYS calls the logging function.
 *      The logging function itself will decide whether to print to stderr based on the build type.
 */
#if defined(SITUATION_USE_OPENGL)
SITAPI void _SituationLogGLError(const char* file, int line);
#define SIT_CHECK_GL_ERROR() _SituationLogGLError(__FILE__, __LINE__)
#else
#define SIT_CHECK_GL_ERROR() do {} while(0)
#endif

// --- [IMPORTANT] User must define the backend to use ---
#if !defined(SITUATION_USE_VULKAN) && !defined(SITUATION_USE_OPENGL)
    #error "You must define either SITUATION_USE_VULKAN or SITUATION_USE_OPENGL before including situation.h"
#endif
#if defined(SITUATION_USE_VULKAN) && defined(SITUATION_USE_OPENGL)
    #error "You can only define one renderer backend (SITUATION_USE_VULKAN or SITUATION_USE_OPENGL)"
#endif

// --- [OPTIONAL] Define this to enable runtime GLSL -> SPIR-V compilation for both backends.
// #define SITUATION_ENABLE_SHADER_COMPILER

// --- Check for valid configurations ---
// FIX 2.3.2A: Relaxed requirement. Internal renderers will be disabled if compiler is missing.
#if defined(SITUATION_USE_VULKAN) && !defined(SITUATION_ENABLE_SHADER_COMPILER)
    // Warning: SituationCmdDrawQuad and Virtual Displays will be unavailable.
#endif

// ================================================================================================
// NOTE: External library includes have been moved to situation.h
// This file now contains only pure API declarations (types, enums, function prototypes)
// ================================================================================================

#include "situation_base_errno.h"

#include "situation_base_etc.h"

// --- Initialization State Management (v2.3.40) ---
typedef enum SituationInitState {
    SITUATION_STATE_UNINITIALIZED = 0,  // Library not initialized
    SITUATION_STATE_INITIALIZING = 1,    // Init in progress, render thread starting
    SITUATION_STATE_READY = 2,           // Fully initialized, safe to create resources
    SITUATION_STATE_SHUTTING_DOWN = 3    // Cleanup in progress
} SituationInitState;

/**
 * @brief Logs a warning message in debug builds.
 * @details This function is intended for internal library use. It formats a warning message
 *          and, in debug builds (when NDEBUG is not defined), prints it to stderr and sets the
 *          library's last error state. In release builds, this function is compiled out to nothing.
 * @param code The SituationError code associated with the warning.
 * @param fmt The printf-style format string for the message.
 * @param ... Variable arguments for the format string.
 */
typedef enum {
    SIT_LOG_ALL = 0,
    SIT_LOG_TRACE,
    SIT_LOG_DEBUG,
    SIT_LOG_INFO,
    SIT_LOG_WARNING,
    SIT_LOG_ERROR,
    SIT_LOG_FATAL,
    SIT_LOG_NONE
} SituationLogLevel;

SITAPI void SituationLog(int msgType, const char* text, ...);                           // Log a message at the specified level (SIT_LOG_*).
SITAPI void SituationSetTraceLogLevel(int logType);                                     // Set the minimum log level for output filtering.
SITAPI void SituationSetLogCallback(void (*callback)(SituationLogLevel level, const char* message, void* user), void* user); // Set a custom log callback.
SITAPI void SituationShowMessageBox(const char* title, const char* message);            // Blocking UI message box; for fatal init errors.

SITAPI void SituationLogWarning(SituationError code, const char* fmt, ...);             // Log a warning with an associated error code (debug builds only).
#define SITUATION_LOG_WARNING SituationLogWarning

// Enable runtime main-thread asserts (debug only)
#ifdef SITUATION_ENABLE_MT_ASSERTS
    #define SIT_ASSERT_MAIN_THREAD() _SituationAssertMainThread(__FILE__, __LINE__)
#else
    #define SIT_ASSERT_MAIN_THREAD() do {} while(0)
#endif

// --- Threading Support Configuration ---
// Use native C11 threads if available, otherwise fall back to tinycthread (Windows MinGW)
#ifdef __cplusplus
    // C++ mode: Use C++11 <atomic> instead of C11 <stdatomic.h>
    #include <atomic>
    #include <thread>
    #include <mutex>
    #include <condition_variable>

    // Map C11 atomic types to C++ std::atomic
    #define _Atomic(T) std::atomic<T>
    #define atomic_load(ptr) ((ptr)->load())
    #define atomic_store(ptr, val) ((ptr)->store(val))
    #define atomic_fetch_add(ptr, val) ((ptr)->fetch_add(val))
    #define atomic_fetch_sub(ptr, val) ((ptr)->fetch_sub(val))
    #define atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail) \
        ((ptr)->compare_exchange_strong(*(expected), desired, succ, fail))
    #define atomic_init(ptr, val) ((ptr)->store(val))
    #define memory_order_seq_cst std::memory_order_seq_cst
    #define memory_order_relaxed std::memory_order_relaxed

    // C++ atomic type aliases
    using atomic_int = std::atomic<int>;
    using atomic_bool = std::atomic<bool>;
    using atomic_uint = std::atomic<unsigned int>;
    using atomic_size_t = std::atomic<size_t>;
    using atomic_uint16_t = std::atomic<uint16_t>;
    using atomic_uint32_t = std::atomic<uint32_t>;
    using atomic_ushort = std::atomic<unsigned short>;
    using atomic_uint_least32_t = std::atomic<uint_least32_t>;
    using atomic_uint_least64_t = std::atomic<uint_least64_t>;
    using atomic_float = std::atomic<float>;

    // Note: C11 threads (mtx_*, cnd_*, thrd_*) are NOT available in C++ mode
    // The library will need to use tinycthread which provides these on Windows
    // For now, we'll use tinycthread even in C++ mode
    #define TINYCTHREAD_IMPLEMENTATION
    #include "ext/glfw/deps/tinycthread.h"

    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        #include <stdalign.h>
    #endif
#else
    // C mode: Use C11 atomics
    #if !defined(__STDC_NO_THREADS__)
        #include <threads.h>
        #include <stdatomic.h>
        #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
            #include <stdalign.h> // For alignas/_Alignas
        #endif
    #else
        // Fallback: Use tinycthread for platforms without native C11 threads (e.g., MinGW)
        #if defined(SITUATION_ENABLE_THREADING)
            #define TINYCTHREAD_IMPLEMENTATION
            #include "ext/glfw/deps/tinycthread.h"
            #include <stdatomic.h>
            #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
                #include <stdalign.h>
            #endif
        #endif
    #endif

    // Define atomic_float (not in C11 standard but needed for audio)
    typedef _Atomic(float) atomic_float;
#endif


// Shim for timespec_get on Windows (not available in MinGW)
#if defined(_WIN32) && !defined(timespec_get)
static inline int timespec_get(struct timespec *ts, int base) {
    if (base != TIME_UTC) return 0;
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    ts->tv_sec = (time_t)(counter.QuadPart / frequency.QuadPart);
    ts->tv_nsec = (long)(((counter.QuadPart % frequency.QuadPart) * 1000000000) / frequency.QuadPart);
    return base;
}
#endif

#ifdef SITUATION_ENABLE_THREADING
    #if defined(__STDC_NO_THREADS__) && !defined(TINYCTHREAD_IMPLEMENTATION)
        #error "SITUATION_ENABLE_THREADING requires C11 <threads.h> support or tinycthread fallback."
    #endif

// ==================================================================================
//  Thread Pool & Task System (Generational)
// ==================================================================================
//  Generational dual-queue job system (mutex per queue; atomics for heads/tails and job state).
//  Features:
//   - Dual Priority Queues: High (Physics/Logic) and Low (Assets/IO); each queue has its own mtx_t.
//   - O(1) Job Tracking: Generational slot IDs (ABA-safe completion checks).
//   - Small Object Optimization (SOO): Embeds 64 bytes of payload in the job struct.
//   - Fork-Join Parallelism: SituationDispatchParallel (main thread may help on the high queue).
//   - Backpressure: BLOCK_IF_FULL spin, RUN_IF_FULL inline execute, or fail (default).

// -- Constants --
#define SITUATION_MAX_THREADS 32
#define SITUATION_JOB_PAYLOAD_MAX 64

// -- Build-Flag Defaults --
// SITUATION_WORKER_NUMA_SPREAD_DEFAULT: When threading is enabled, workers are spread
// across NUMA nodes by default. Define as 0 before including this header to disable.
#ifndef SITUATION_WORKER_NUMA_SPREAD_DEFAULT
  #if defined(SITUATION_ENABLE_THREADING)
    #define SITUATION_WORKER_NUMA_SPREAD_DEFAULT 1
  #else
    #define SITUATION_WORKER_NUMA_SPREAD_DEFAULT 0
  #endif
#endif

// -- Types --
typedef uint32_t SituationJobId;

// -- Submission Flags --
typedef enum {
    SIT_SUBMIT_DEFAULT       = 0,       // Low Priority, Return 0 if full
    SIT_SUBMIT_HIGH_PRIORITY = 1 << 0,  // Use High Priority Queue (Physics, Audio)
    SIT_SUBMIT_BLOCK_IF_FULL = 1 << 1,  // Spin/Sleep until a slot opens
    SIT_SUBMIT_RUN_IF_FULL   = 1 << 2,  // Execute immediately on current thread if full
    SIT_SUBMIT_POINTER_ONLY  = 1 << 3   // Do not copy large data; user guarantees lifetime
} SituationJobFlags;

// -- Job Definition (Generational) --
// Aligned to cache line boundaries to prevent false sharing
typedef struct SituationJob {
    // Generation counter for O(1) validation (prevents ABA problems)
    atomic_ushort generation;

    // Callback function: func(payload_ptr, user_context_ptr)
    void (*func)(void*, void*);

    // Dependency Graph Support
    atomic_int dependency_count;        // Wait counter: Job runs when this hits 0
    atomic_uint_least32_t continuation_id; // ID of job to trigger when this finishes (CAS target)
    uint8_t dep_depth;                  // Cycle detection depth counter (max 32)
    bool uses_large_data;               // Flag for data location
    bool owns_memory;                   // [New] If true, large_data_ptr must be freed

    // Small Object Optimization (SOO)
    // 64 bytes avoids malloc for matrices, config structs, etc.
    #if defined(__stdalign_h)
    alignas(16) uint8_t storage[SITUATION_JOB_PAYLOAD_MAX];
    #else
    uint8_t storage[SITUATION_JOB_PAYLOAD_MAX]; // Fallback alignment
    #endif

    // Fallback for large data (>64 bytes)
    void* large_data_ptr;

    // Synchronization & Status
    atomic_bool is_completed;
} SituationJob;

// --- Threading observability (Epic B) ---
typedef enum {
    SITUATION_THREAD_CAP_NONE           = 0,
    SITUATION_THREAD_CAP_C11_THREADS    = (1 << 0),
    SITUATION_THREAD_CAP_C11_ATOMICS    = (1 << 1),
    SITUATION_THREAD_CAP_MUTEX          = (1 << 2),
    SITUATION_THREAD_CAP_SLEEP          = (1 << 3),
    SITUATION_THREAD_CAP_PLATFORM_SLEEP = (1 << 4),
} SituationThreadCapability;

typedef struct {
    bool available;
    int capabilities;
    const char* platform;
    const char* sleep_impl;
    bool sleep_reliable;
    int max_threads;
    bool platform_topology_ok;
    bool numa_available;
    int pool_thread_count;
    bool io_thread_enabled;
    const char* warnings[4];
    int warning_count;
} SituationThreadingStatus;

typedef enum {
    SIT_THREAD_ROLE_UNKNOWN = 0,
    SIT_THREAD_ROLE_MAIN,
    SIT_THREAD_ROLE_WORKER,
    SIT_THREAD_ROLE_IO,
    SIT_THREAD_ROLE_RENDER,
    SIT_THREAD_ROLE_AUDIO,
} SituationThreadRole;

typedef enum {
    SIT_JOB_QUEUE_LOW  = (1 << 0),
    SIT_JOB_QUEUE_HIGH = (1 << 1),
    SIT_JOB_QUEUE_BOTH = (SIT_JOB_QUEUE_LOW | SIT_JOB_QUEUE_HIGH),
} SituationJobQueueMask;

typedef struct {
    SituationThreadRole role;
    int index;                      // Worker index (0..N-1) or -1 for non-worker roles
    char name[24];                  // Thread name (e.g. "Sit Worker 0", "Sit I/O")
    int last_logical_cpu;
    int numa_node;
    uint64_t affinity_mask_applied;
    bool active;
} SituationThreadSlotSnapshot;

#define SITUATION_THREAD_SNAPSHOT_MAX_SLOTS (SITUATION_MAX_THREADS + 4)

typedef struct {
    bool pool_active;
    size_t worker_count;
    bool io_thread_enabled;
    int active_jobs;
    size_t low_queue_depth;
    size_t high_queue_depth;
    uint64_t stats_jobs_submitted;
    uint64_t stats_jobs_completed;
    uint64_t stats_main_steal_success;
    uint64_t stats_main_steal_fail;
    int slot_count;
    SituationThreadSlotSnapshot slots[SITUATION_THREAD_SNAPSHOT_MAX_SLOTS];
} SituationThreadPoolSnapshot;

/** Scheduler / contention counters (Epic D — Threading Bolstering). */
typedef struct {
    uint64_t jobs_submitted;
    uint64_t jobs_completed;
    uint64_t main_steal_success;
    uint64_t main_steal_fail;
    uint64_t main_steal_empty_queue;
    uint64_t high_queue_lock_ops;
    uint64_t high_queue_lock_ns;
    uint64_t scan_forward_swap;
    uint64_t scan_forward_exhausted;
    uint64_t io_idle_waits;
    uint64_t io_jobs_run;
    uint64_t submit_run_inline;
    uint64_t queue_full_spins;
    uint64_t dispatch_parallel_calls;
    double io_busy_ratio;
} SituationThreadPoolMetrics;

struct SituationThreadPool;

typedef struct SituationWorkerStartArg {
    struct SituationThreadPool* pool_handle;
    size_t worker_index;
} SituationWorkerStartArg;

// -- Thread Pool Handle --
struct SituationThreadPool {
    bool is_active;
    thrd_t threads[SITUATION_MAX_THREADS];
    size_t thread_count;
    SituationWorkerStartArg worker_args[SITUATION_MAX_THREADS];
    atomic_int worker_last_logical_cpu[SITUATION_MAX_THREADS];
    atomic_uint_least64_t stats_jobs_submitted;
    atomic_uint_least64_t stats_jobs_completed;
    atomic_uint_least64_t stats_main_steal_success;
    atomic_uint_least64_t stats_main_steal_fail;
    atomic_uint_least64_t stats_main_steal_empty_queue;
    atomic_uint_least64_t stats_high_queue_lock_ops;
    atomic_uint_least64_t stats_high_queue_lock_ns;
    atomic_uint_least64_t stats_scan_forward_swap;
    atomic_uint_least64_t stats_scan_forward_exhausted;
    atomic_uint_least64_t stats_io_idle_waits;
    atomic_uint_least64_t stats_io_jobs_run;
    atomic_uint_least64_t stats_submit_run_inline;
    atomic_uint_least64_t stats_queue_full_spins;
    atomic_uint_least64_t stats_dispatch_parallel_calls;
    atomic_int io_last_logical_cpu;

    // -- Dual Ring Buffers --
    // Index 0 = Low Priority (Assets/IO), Index 1 = High Priority (Physics/Logic)
    struct {
        SituationJob* jobs;
        size_t capacity;
        size_t mask;        // capacity - 1 (for fast bitwise wrapping)
        atomic_size_t head; // Write index
        atomic_size_t tail; // Read index
        mtx_t lock;         // Fine-grained lock per queue
    } queues[2];

    // Signaling
    cnd_t wake_condition;   // Wakes workers when work is added
    cnd_t idle_condition;   // Wakes main thread when all jobs complete

    // Dedicated I/O Thread
    thrd_t io_thread;
    atomic_bool io_active;
    double hot_reload_rate; // [v2.3.37] Store rate

    atomic_int active_jobs; // Total jobs currently running or pending
    atomic_bool shutdown;
    char _padding[64];      // Prevent false sharing on the shutdown flag
};

typedef struct SituationThreadPool SituationThreadPool;

// Note: API Prototypes are located in the main API section below (around line ~2240)
// to keep header structure clean and consistent with other modules.
// See: SituationCreateThreadPool, SituationSubmitJobEx, etc.

#endif // SITUATION_ENABLE_THREADING

// ---------------------------------------------------------------------------------
//  Buffer Usage Flags (Critical for backend memory optimisation)
//  These flags are translated directly to VkBufferUsageFlags / GL buffer usage hints.
//  Always specify the minimal set required - the backend will place the buffer in the fastest
//  memory type possible based on these hints.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BUFFER_USAGE_VERTEX_BUFFER     = 1 << 0,   // Source of vertex data
    SITUATION_BUFFER_USAGE_INDEX_BUFFER      = 1 << 1,   // Source of index data
    SITUATION_BUFFER_USAGE_UNIFORM_BUFFER    = 1 << 2,   // Uniform Buffer Object (constant data, frequently updated)
    SITUATION_BUFFER_USAGE_STORAGE_BUFFER    = 1 << 3,   // Shader Storage Buffer Object (read/write in shaders)
    SITUATION_BUFFER_USAGE_INDIRECT_BUFFER   = 1 << 4,   // Indirect draw/dispatch command buffer
    SITUATION_BUFFER_USAGE_TRANSFER_SRC      = 1 << 5,   // Source for copy operations (CPU → GPU staging)
    SITUATION_BUFFER_USAGE_TRANSFER_DST      = 1 << 6,   // Destination for copy operations (GPU → CPU readback)
    SITUATION_BUFFER_USAGE_DEVICE_ADDRESS    = 1 << 7,   // Buffer can be accessed via device address (for buffer references)

    // Common combination presets (use these for convenience and maximum performance)
    SITUATION_BUFFER_USAGE_VERTEX_AND_STORAGE = SITUATION_BUFFER_USAGE_VERTEX_BUFFER | SITUATION_BUFFER_USAGE_STORAGE_BUFFER,
    SITUATION_BUFFER_USAGE_DYNAMIC_VERTEX = SITUATION_BUFFER_USAGE_VERTEX_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,
    SITUATION_BUFFER_USAGE_DYNAMIC_UNIFORM = SITUATION_BUFFER_USAGE_UNIFORM_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST,
    SITUATION_BUFFER_USAGE_STORAGE_COMPUTE = SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC | SITUATION_BUFFER_USAGE_TRANSFER_DST | SITUATION_BUFFER_USAGE_DEVICE_ADDRESS,
} SituationBufferUsageFlags;
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
#define SITUATION_MAX_FRAMES_IN_FLIGHT          2    /* Max overlapping frames for VK/GL swapchains (2-3 typical for V-Sync). */
#ifndef SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS
#define SITUATION_VULKAN_ACQUIRE_TIMEOUT_NS     1000000000ULL /* Nanoseconds for vkAcquireNextImageKHR; avoids indefinite wait when the surface will not provide an image (minimized/occluded). Override before including this header if needed. */
#endif
#ifndef SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS
#define SITUATION_VULKAN_FENCE_WAIT_TIMEOUT_NS 15000000000ULL /* Total nanoseconds for chunked vkWaitForFences (+ glfwPollEvents) per fence; avoids infinite stall / Windows "Not responding". */
#endif
#ifndef SITUATION_VULKAN_SHUTDOWN_FENCE_WAIT_NS
#define SITUATION_VULKAN_SHUTDOWN_FENCE_WAIT_NS 3000000000ULL /* Per in-flight fence: chunked vkWaitForFences + glfwPollEvents anywhere we would have used vkDeviceWaitIdle (shutdown, swapchain teardown/recreate, VSync/full cleanup, init error path). vkDeviceWaitIdle blocks the main thread with no event pump and never returns if the GPU is wedged → frozen pale window. */
#endif
#ifndef SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS
#define SITUATION_VULKAN_LOG_SLOW_ACQUIRE_MIN_MS 100 /* Log vkAcquire timing to stderr if acquire >= this many ms, or on TIMEOUT. Use 0 to log every acquire (verbose). */
#endif
#define SITUATION_MAX_STORAGE_DEVICES           8    /* Max detected storage volumes (e.g., drives, mounts). */
#define SITUATION_MAX_NETWORK_ADAPTERS          8    /* Max network interfaces (e.g., Ethernet/Wi-Fi). */
#define SITUATION_MAX_DEVICE_NAME_LEN           128  /* Max length for device strings (e.g., GPU/CPU names). */
#define SITUATION_MAX_CPU_NAME_LEN              64   /* Max CPU model string length (e.g., "Intel i9-13900K"). */
#define SITUATION_MAX_LOGICAL_PROCESSORS        256  /* Max entries in SituationCpuTopology::processors */
#define SITUATION_AFFINITY_MASK_BITS            64   /* Bit width for SituationSetThreadAffinity / mask builders (low logical IDs) */
#define SITUATION_MAX_NUMA_NODES                64   /* Max NUMA nodes in SituationNumaTopology */
#define SITUATION_MAX_GPU_NAME_LEN              128  /* Max GPU model string length (e.g., "NVIDIA RTX 4090"). */
#define SITUATION_MAX_MONITORS                  8    /* Max physical displays to track in device snapshot. */
#define SITUATION_MAX_MONITOR_NAME_LEN          128  /* Max monitor EDID name length (e.g., "Dell UltraSharp"). */
#define SITUATION_MAX_ERROR_MSG_LEN             16384 /* Max length for error messages and logs. */
#define SITUATION_MAX_SHADER_LOG_LEN            16384 /* Max length for shader compilation logs. */

/* === Graphics & Rendering Limits === */
#define SITUATION_MAX_VIRTUAL_DISPLAYS          16   /* Max offscreen render targets (e.g., for UI/post-fx). */
#define SITUATION_MAX_TEXTURES                  4096
#define SITUATION_MAX_SHADERS                   1024
#define SITUATION_MAX_COMPUTE_PIPELINES         512
#define SITUATION_MAX_BUFFERS                   4096
#define SITUATION_MAX_MESHES                    4096
#define SITUATION_MAX_MODELS                    1024
#define SITUATION_MAX_RASTER_STACK_DEPTH        256  /* Max depth for SituationCmdPushRasterState / PopRasterState (GL + VK). */

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

/**
 * @brief Basic Math Types
 */
typedef struct ColorHSV { float h, s, v; } ColorHSV; // Hue = 0.0f to 360.0f degrees, Saturation = 0.0f grayscale to 1.0f color, Value/Brightness = 0.0f to 1.0f
typedef struct ColorYPQA { unsigned char y, p, q, a; } ColorYPQA; // NTSC-style luma/phase/chroma (see situation_impl_ypq.h internally)
typedef struct ColorYPQf { float y, p, q, a; } ColorYPQf;         // Normalized YPQ edit space: y/p/q/a in [0, 1]; p is phase (0..1 → full hue wheel)
typedef struct ColorRGBA { unsigned char r, g, b, a; } ColorRGBA;
typedef ColorRGBA Color;

typedef union Vector2 {
    struct { float x, y; };
    float raw[2];
} Vector2;

typedef union Vector3 {
    struct { float x, y, z; };
    struct { float r, g, b; };
    float raw[3];
} Vector3;

typedef union Vector4 {
    struct { float x, y, z, w; };
    struct { float r, g, b, a; };
    float raw[4];
} Vector4;

typedef struct SitRectangle { float x, y, width, height; } SitRectangle;

//==================================================================================
//  Callback Type Definitions - v2.3.4 "Velocity" Standard
//==================================================================================
//
//  All Situation callbacks are designed to be:
//   • Invoked exclusively from the main thread (except audio capture when the SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD flag is NOT set — then it runs on the audio thread)
//   • Real-time safe where applicable (especially audio-related callbacks)
//   • Zero-overhead (no virtual calls, no hidden allocations)
//   • Fully user-data driven (last parameter is always the pointer you supplied)
//
//  Never call other SITAPI functions from inside a callback unless the documentation for that specific callback explicitly declares it safe.
//
//  These signatures are frozen — they will never change in any future version.
//

// ── Window / OS Events ─────────────────────────────────────────────────────
typedef void (*SituationFileDropCallback)(
    int          count,       // Number of files dropped
    const char** paths,       // Array of UTF-8 file paths (valid only for duration of callback)
    void*        user_data
); // Files/folders dragged from OS onto the window

typedef void (*SituationFileLoadCallback)(
    void*        data,        // The loaded file data (or NULL on failure). OWNED BY CALLER (must free).
    size_t       size,        // Size of the loaded data in bytes.
    void*        user_data    // User data passed to the async function.
); // Callback for asynchronous file loading

typedef void (*SituationFileSaveCallback)(
    bool         success,     // true if the file was written successfully.
    void*        user_data    // User data passed to the async function.
); // Callback for asynchronous file saving

typedef void (*SituationFileTextLoadCallback)(
    char*        text,        // The loaded null-terminated string (or NULL on failure). OWNED BY CALLER (must free).
    void*        user_data    // User data passed to the async function.
); // Callback for asynchronous text file loading

typedef void (*SituationFocusCallback)(
    bool   gained_focus,      // true = window gained focus, false = lost focus
    void*  user_data
); // Window focus change (alt-tab, click, etc.)

typedef void (*SituationMaximizeCallback)(
    bool   maximized,         // true = window maximized, false = restored from maximized
    void*  user_data
); // Title-bar maximize / restore (requires SITUATION_FLAG_WINDOW_RESIZABLE at init)

typedef void (*SituationWindowCloseCallback)(
    void* user_data
); // User clicked the OS close button. Rarely used — most code just polls SituationWindowShouldClose()

// ── Input Events (Optional Event-Driven API — polling API is always available) ──
typedef void (*SituationKeyCallback)(
    int   key,       		// SIT_KEY_xxx code
    int   scancode,  		// Platform-specific scancode (useful for non-QWERTY layouts)
    int   action,    		// SIT_PRESS, SIT_RELEASE, or SIT_REPEAT
    int   mods,      		// Bitfield of SIT_MOD_xxx
    void* user_data
); // Exact GLFW key callback signature

typedef void (*SituationCharCallback)(
    unsigned int codepoint,   // UTF-32 codepoint (valid Unicode character)
    void*        user_data
); // Text input (separate from key events — handles IME, dead keys, etc.)

typedef void (*SituationMouseButtonCallback)(
    int   button,    // SIT_MOUSE_BUTTON_1 to 8
    int   action,    // SIT_PRESS or SIT_RELEASE
    int   mods,      // Modifier bitfield
    void* user_data
); // Mouse button events

typedef void (*SituationCursorPosCallback)(
    Vector2 position, // Cursor position in screen coordinates (HiDPI-aware, sub-pixel precision)
    void*   user_data
); // Called every time the mouse moves (can be very frequent)

typedef void (*SituationScrollCallback)(
    Vector2 offset,   // x/y scroll amount (y is usually ±1.0 per notch)
    void*   user_data
); // Mouse wheel / trackpad scroll

typedef void (*SituationJoystickCallback)(
    int  jid,        // Joystick ID (0 to SITUATION_MAX_JOYSTICKS-1)
    int  event,      // GLFW_CONNECTED or GLFW_DISCONNECTED
    void* user_data
); // Gamepad/controller hotplug events

// ── Custom Audio Streaming (Exact MiniAudio vtable signatures — required for perfect compatibility) ──
typedef ma_uint64 (*SituationStreamReadCallback)(
    void*      pUserData,    // Your custom stream context pointer
    void*      pBufferOut,   // Buffer to fill with PCM data
    ma_uint64  bytesToRead   // Maximum bytes to write
); // Return number of bytes actually written

typedef ma_result (*SituationStreamSeekCallback)(
    void*       pUserData,   // Your custom stream context pointer
    ma_int64    byteOffset,  // Offset in bytes
    ma_seek_origin origin    // MA_SEEK_WHENCE_SOF/COF/EOF
); // Return MA_SUCCESS on success

// ── Audio Capture (Microphone / Line-In) ─────────────────────────────────────
typedef void (*SituationAudioCaptureCallback)(
    const float* input_buffer,   // Interleaved 32-bit float samples (read-only!)
    uint32_t     frame_count,    // Number of frames in this block (typically 256–1024)
    void*        user_data
); // Called from audio thread unless SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD is set
   // Format is always engine native: 48 kHz (or custom, stereo (or mono), interleaved floats

// ── Custom DSP Processor Chain (Per-Sound Post-Effects) ─────────────────────
typedef void (*SituationAudioProcessorCallback)(
    float*       buffer,         // Interleaved float samples — read/write in-place
    uint32_t     frames,         // Number of frames in this block
    uint32_t     channels,       // 1 (mono) or 2 (stereo)
    uint32_t     sampleRate,     // Current engine sample rate in Hz
    void*        user_data 		 // Pointer supplied when the processor was added
); // Applied after built-in effects (filter → echo → reverb) but before final volume/pan
   // Must be real-time safe — no SIT_MALLOC, no locking, no system calls

// ── Internal GLFW Error Callback (Exposed only for extremely advanced users) ──
typedef void (*GLFWerrorfun)(int error_code, const char* description);

SITAPI void SituationFreeString(char* str);                                             // Free a string allocated by the library (e.g., from path helpers).

#if defined(__cplusplus)
extern "C++" {
/**
 * @brief RAII Wrapper for Situation Strings (C++ only).
 * @details Automatically calls SituationFreeString() when it goes out of scope.
 */
struct SituationScopedString {
    char* str;
    SituationScopedString(char* s) : str(s) {}
    ~SituationScopedString() { if(str) SituationFreeString(str); }
    operator const char*() const { return str; }
    const char* get() const { return str; }
    // Prevent copy
    SituationScopedString(const SituationScopedString&) = delete;
    SituationScopedString& operator=(const SituationScopedString&) = delete;
    // Allow move
    SituationScopedString(SituationScopedString&& other) noexcept : str(other.str) { other.str = NULL; }
    SituationScopedString& operator=(SituationScopedString&& other) noexcept {
        if (this != &other) {
            if (str) SituationFreeString(str);
            str = other.str;
            other.str = NULL;
        }
        return *this;
    }
};
}
#endif


/**
 * @brief Specifies the color encoding of image data.
 *
 * This enum describes whether pixel data is stored in linear or SRGB color space.
 * The encoding affects how the data should be interpreted when creating GPU textures.
 *
 * **SITUATION_COLOR_LINEAR:**
 * - Data is in linear color space with no gamma encoding
 * - Required for storage images (textures writable by compute shaders)
 * - No automatic gamma correction applied during sampling
 * - Maps to UNORM formats:
 *   - Vulkan: VK_FORMAT_R8G8B8A8_UNORM
 *   - OpenGL: GL_RGBA8
 *
 * **SITUATION_COLOR_SRGB:**
 * - Data is in SRGB color space with gamma 2.2 encoding
 * - Preferred for sampled-only textures (photos, UI elements, etc.)
 * - Automatic gamma correction applied when sampled in shaders
 * - Cannot be used with storage images on most GPUs
 * - Maps to SRGB formats:
 *   - Vulkan: VK_FORMAT_R8G8B8A8_SRGB
 *   - OpenGL: GL_SRGB8_ALPHA8
 *
 * @note When creating textures with SITUATION_TEXTURE_USAGE_STORAGE flag, LINEAR encoding
 *       must be used. SRGB formats typically don't support storage image operations.
 * @note For sampled-only textures, SRGB encoding is preferred for proper gamma correction
 *       and color accuracy on standard displays.
 *
 * @since v2.3.40
 */
typedef enum SituationColorEncoding {
    SITUATION_COLOR_LINEAR = 0,     // Linear color space - required for storage images (both OpenGL and Vulkan)
    SITUATION_COLOR_SRGB = 1        // SRGB color space with gamma encoding - for sampled textures (both OpenGL and Vulkan)
} SituationColorEncoding;
typedef struct SituationImage {
    void *data;                                     // Image raw data
    int width;                                      // Image width
    int height;                                     // Image height
    int channels;                                   // Number of channels (e.g., 4 for RGBA, 1 for Grayscale)
    SituationColorEncoding color_encoding;      	// Color space encoding (LINEAR or SRGB)
} SituationImage;

/**
 * @brief Specifies the type of flip operation to perform on an image.
 */
typedef enum SituationImageFlipMode {
    SIT_FLIP_VERTICAL,                              // Flips the image top-to-bottom.
    SIT_FLIP_HORIZONTAL,                            // Flips the image left-to-right.
    SIT_FLIP_BOTH                                   // Flips both vertically and horizontally (180-degree rotation).
} SituationImageFlipMode;


/**
 * @brief Renderer Abstraction
 */
typedef enum {
    SIT_RENDERER_OPENGL,
    SIT_RENDERER_VULKAN
} SituationRendererType;

/**
 * @brief Defines a set of common, pre-configured layouts for compute pipelines.
 * @details This enum is passed to SituationCreateComputePipeline* to select an appropriate
 *          VkPipelineLayout that matches the resources declared in the compute shader.
 */
typedef enum {
    SIT_COMPUTE_LAYOUT_ONE_SSBO,                    // A layout for shaders that use one Shader Storage Buffer Object (SSBO) at set 0.
    SIT_COMPUTE_LAYOUT_TWO_SSBOS,                   // A layout for shaders that use two SSBOs at sets 0 and 1.
    SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO,              // A layout for shaders that use one Storage Image at set 0 and one SSBO at set 1.
    SIT_COMPUTE_LAYOUT_PUSH_CONSTANT,               // A layout for shaders that use a 64-byte push constant for small data.
    SIT_COMPUTE_LAYOUT_EMPTY,                       // A layout for simple shaders that take no external resources.
    SIT_COMPUTE_LAYOUT_BUFFER_IMAGE,                // A layout for shaders that use one SSBO (Set 0) and one Storage Image (Set 1).
    SIT_COMPUTE_LAYOUT_TERMINAL,
    SIT_COMPUTE_LAYOUT_VECTOR,
} SituationComputeLayoutType;

/**
 * @brief Descriptor pipeline layout for `SituationLoadShaderFromSpirvMemoryEx` (Vulkan graphics).
 * @details **Vulkan:** selects a pre-defined `VkPipelineLayout` matching harness/custom SPIR-V descriptor sets.
 *          **OpenGL:** ignored; load path is unchanged from `SituationLoadShaderFromSpirvMemory`.
 */
typedef enum SituationSpirvLayoutProfile {
    SIT_SPIRV_LAYOUT_PROFILE_MESH = 0,       /**< Default: set 0 dynamic UBO, set 1 sampler (same as `SituationLoadShaderFromSpirvMemory`). */
    SIT_SPIRV_LAYOUT_PROFILE_DUAL_SSBO,      /**< Set 0 + set 1 storage buffers @ binding 0 each. */
    SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO,         /**< Set 0 uniform buffer + set 1 storage buffer @ binding 0. */
} SituationSpirvLayoutProfile;

/**
 * @brief Flags for texture creation (used in SituationCreateTextureEx)
 */
typedef enum {
    SITUATION_TEXTURE_USAGE_SAMPLED         = 1 << 0, // Standard texture
    SITUATION_TEXTURE_USAGE_STORAGE         = 1 << 1, // Can be used with imageStore/Compute
    SITUATION_TEXTURE_USAGE_TRANSFER_SRC    = 1 << 2, // Can be copied from
    SITUATION_TEXTURE_USAGE_TRANSFER_DST    = 1 << 3, // Can be copied to
    SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED = 1 << 4  // Will be sampled (read-only) in compute shaders
} SituationTextureUsageFlags;

/**
 * @brief Texture formats
 */
typedef enum {
    SIT_TEXTURE_FORMAT_UNKNOWN = 0,
    SIT_TEXTURE_FORMAT_RGBA8_UNORM,
    SIT_TEXTURE_FORMAT_RGBA8_SRGB,
} SituationTextureFormat;

/**
 * @brief Texture filters
 */
typedef enum {
    SIT_TEXTURE_FILTER_NEAREST = 0,
    SIT_TEXTURE_FILTER_LINEAR,
} SituationTextureFilter;

/**
 * @brief Texture wrap modes
 */
typedef enum {
    SIT_TEXTURE_WRAP_CLAMP_TO_EDGE = 0,
    SIT_TEXTURE_WRAP_REPEAT,
} SituationTextureWrap;

/**
 * @brief Texture Information
 */
typedef struct {
    int width;
    int height;
    int mip_levels;
    SituationTextureFormat format;
    SituationTextureUsageFlags usage_flags;
    SituationTextureFilter min_filter;
    SituationTextureFilter mag_filter;
    SituationTextureWrap wrap_s;
    SituationTextureWrap wrap_t;
} SituationTextureInfo;

/**
 * @brief Texture Readback Format
 */
typedef enum {
    SIT_TEXTURE_READ_RGBA8 = 0, /* normalized RGBA bytes, backend-independent */
} SituationTextureReadFormat;

/**
 * @brief Texture Region
 */
typedef struct {
    int x, y;
    int width, height;
    int mip_level; /* 0 for base */
} SituationTextureRegion;

/**
 * @brief Texture blit filter mode.
 */
typedef enum {
    SITUATION_BLIT_FILTER_NEAREST = 0,
    SITUATION_BLIT_FILTER_LINEAR = 1
} SituationBlitFilter;

/**
 * @brief 2D texture rectangle in Situation API space.
 *
 * @details Origin is top-left, `y` increases downward, and no backend-specific
 *          implicit flip is applied by blit commands.
 */
typedef struct {
    int x;
    int y;
    int width;
    int height;
} SituationTextureRect;

/**
 * @brief Texture-to-texture blit region.
 *
 * @details First implementation slice is color-only 2D textures, mip 0+,
 *          layer 0, with explicit caller-owned texture barriers before and
 *          after the blit.
 */
typedef struct {
    SituationTextureRect src_rect;
    SituationTextureRect dst_rect;
    uint32_t src_mip_level;
    uint32_t dst_mip_level;
    uint32_t src_array_layer;
    uint32_t dst_array_layer;
    SituationBlitFilter filter;
} SituationTextureBlitRegion;

/**
 * @brief Texture-to-texture copy region (exact-size transfer).
 *
 * @details Texture-to-texture: copies `src_rect` to `(dst_x, dst_y)` on the destination mip.
 *          Buffer-to-texture: `src_rect` supplies width/height only (`x`/`y` must be 0); data is read
 *          tightly packed RGBA8 rows from the source buffer offset.
 *          Texture-to-buffer: `src_rect` selects the texture subregion; `dst_x`/`dst_y` are unused.
 *          Copy does not scale (use blit for that). First slice: color-only 2D textures, layer 0,
 *          explicit caller-owned barriers.
 */
typedef struct {
    SituationTextureRect src_rect;
    int dst_x;
    int dst_y;
    uint32_t src_mip_level;
    uint32_t dst_mip_level;
    uint32_t src_array_layer;
    uint32_t dst_array_layer;
} SituationTextureCopyRegion;

/**
 * @brief Readback description for textures
 */
typedef struct {
    SituationTextureRegion region;       /* mip_level 0 unless explicitly supported */
    SituationTextureReadFormat format;   /* default SIT_TEXTURE_READ_RGBA8 */
    size_t dst_row_pitch_bytes;          /* 0 = tightly packed width * 4 */
} SituationTextureReadbackDesc;

/**
 * @brief Readback description for framebuffers
 */
typedef struct {
    int x;
    int y;
    int width;
    int height;
    SituationTextureReadFormat format; /* default SIT_TEXTURE_READ_RGBA8 */
    size_t dst_row_pitch_bytes;        /* 0 = tightly packed width * 4 */
} SituationReadPixelsDesc;

/**
 * @brief Opaque handle for a command buffer
 */
#ifdef SITUATION_USE_VULKAN
typedef VkCommandBuffer SituationCommandBuffer;
#else
typedef struct SituationCommandBuffer_t* SituationCommandBuffer;
#endif


/**
 * @brief Device Information Structures
 */
typedef struct {
    char cpu_name[SITUATION_MAX_CPU_NAME_LEN];
    int cpu_cores;
    float cpu_clock_speed_ghz;
    char gpu_name[SITUATION_MAX_GPU_NAME_LEN];
    uint64_t gpu_dedicated_memory_bytes;            // Primarily via DXGI on Windows
    uint64_t total_ram_bytes;
    uint64_t available_ram_bytes;
    int storage_device_count;
    char storage_device_names[SITUATION_MAX_STORAGE_DEVICES][SITUATION_MAX_DEVICE_NAME_LEN];
    uint64_t storage_capacity_bytes[SITUATION_MAX_STORAGE_DEVICES];
    uint64_t storage_free_bytes[SITUATION_MAX_STORAGE_DEVICES];
    int network_adapter_count;
    char network_adapter_names[SITUATION_MAX_NETWORK_ADAPTERS][SITUATION_MAX_DEVICE_NAME_LEN];
    int input_device_count;
    char input_device_names[SITUATION_MAX_INPUT_DEVICES][SITUATION_MAX_DEVICE_NAME_LEN];
    int display_count;
    char display_names[SITUATION_MAX_MONITORS][SITUATION_MAX_MONITOR_NAME_LEN];
    int display_widths[SITUATION_MAX_MONITORS];
    int display_heights[SITUATION_MAX_MONITORS];
    int display_refresh_rates[SITUATION_MAX_MONITORS];
} SituationDeviceInfo;

/**
 * @brief Physical Display Management Structures
 */
typedef struct {
    int width;
    int height;
    int refresh_rate;
    int color_depth;                                // Can be tricky to get reliably for all modes/APIs
} SituationDisplayMode;

typedef struct {
    char name[SITUATION_MAX_MONITOR_NAME_LEN];      // Win32 device name
    int situation_monitor_id;                       // Internal ID, corresponds to index in cached_physical_displays_array
    GLFWmonitor* glfw_monitor_handle;               // Corresponding GLFW monitor handle, if matched
    bool is_primary;
    SituationDisplayMode current_mode;
    SituationDisplayMode* available_modes;          // Caller must free
    int available_mode_count;
} SituationDisplayInfo;

/**
 * @brief Defines standard system cursor shapes.
 */
typedef enum {
    SIT_CURSOR_DEFAULT = 0,                         // The default, platform-specific arrow
    SIT_CURSOR_ARROW,                               // A standard arrow cursor
    SIT_CURSOR_IBEAM,                               // The text input I-beam
    SIT_CURSOR_CROSSHAIR,                           // A crosshair for targeting
    SIT_CURSOR_HAND,                                // A pointing hand, for links or buttons
    SIT_CURSOR_HRESIZE,                             // Horizontal resize arrow (e.g., <->)
    SIT_CURSOR_VRESIZE                              // Vertical resize arrow (e.g., ^ v)
} SituationCursor;

/**
 * @brief Defines the color blending mode for a virtual display during compositing.
 * @details These modes determine how a virtual display's texture is drawn onto the main framebuffer.
 */
typedef enum {
    // --- Standard & Simple Modes ---
    SITUATION_BLEND_ALPHA,                          // Default alpha blending. Final = Src * SrcAlpha + Dst * (1 - SrcAlpha). Ideal for UI.
    SITUATION_BLEND_ADDITIVE,                       // Brightening blend (Src + Dst). Black is transparent. Good for glows, sparks.
    SITUATION_BLEND_MULTIPLY,                       // Darkening blend (Src * Dst). White is transparent. Good for shadows, tinting.
    SITUATION_BLEND_SCREEN,                         // Brightening blend, less harsh than additive. Inverts, multiplies, and inverts again.
    SITUATION_BLEND_NONE,                           // Opaque blend (Final = Src). Ignores alpha and overwrites destination.

    // --- Photoshop-Style Blend Modes (require custom shader) ---
    SITUATION_BLEND_OVERLAY,                        // Combines Multiply and Screen. Preserves highlights and shadows of the destination.
    SITUATION_BLEND_SOFT_LIGHT,                     // Darkens or lightens, depending on source color. A softer version of Hard Light.
    SITUATION_BLEND_HARD_LIGHT,                     // Combines Multiply and Screen based on source color. A harsher version of Overlay.
    SITUATION_BLEND_COLOR_DODGE,                    // Brightens the destination color to reflect the source color.
    SITUATION_BLEND_COLOR_BURN,                     // Darkens the destination color to reflect the source color.
    SITUATION_BLEND_DARKEN,                         // Selects the darker of the source and destination pixels.
    SITUATION_BLEND_LIGHTEN,                        // Selects the lighter of the source and destination pixels.
    SITUATION_BLEND_DIFFERENCE,                     // Subtracts the darker color from the lighter color. Black shows no change.
    SITUATION_BLEND_EXCLUSION,                      // Similar to Difference but with lower contrast.
} SituationBlendMode;

/**
 * @brief Defines the scaling and filtering method for a virtual display.
 */
typedef enum {
    // @brief Smoothly stretches the VD to fill its defined rectangle (via offset/resolution).
    // Ignores aspect ratio. Uses GL_LINEAR filtering (blurry). Good for high-res UI.
    SITUATION_SCALING_STRETCH,

    // @brief Sharp, aspect-correct scaling that fills the screen as much as possible.
    // Uses GL_NEAREST filtering (sharp). This is your requested "sharp stretch" mode.
    // This will leave minimal black bars (letterbox/pillarbox).
    SITUATION_SCALING_FIT,

    // @brief Sharp, aspect-correct, integer-only scaling.
    // Guarantees all game pixels are perfect squares on screen, but may leave larger black bars.
    // Uses GL_NEAREST filtering (sharp). This is the "pixel perfect" purist mode.
    SITUATION_SCALING_INTEGER

} SituationScalingMode;

/**
 * @brief Specifies how an attachment's contents should be treated at the start of a render pass.
 *
 * Applied by `SituationCmdBeginRenderPass` through `SituationRenderPassInfo`:
 * - **OpenGL:** `SIT_LOAD_OP_CLEAR` issues `glClear` for the attachment aspect; `LOAD` preserves
 *   the bound framebuffer; `DONT_CARE` skips the clear (contents undefined).
 * - **Vulkan:** maps to `VkAttachmentLoadOp` / `VkAttachmentLoadOp` (stencil aspect on the depth
 *   attachment). Clear values come from the matching `SituationAttachmentInfo.clear` field.
 *
 * For mid-pass clears inside an already active pass, use `SituationCmdClear*` instead.
 */
typedef enum {
    SIT_LOAD_OP_LOAD,       // Preserve the existing contents of the attachment.
    SIT_LOAD_OP_CLEAR,      // Clear the attachment to a specified value.
    SIT_LOAD_OP_DONT_CARE   // The existing contents are undefined and can be discarded.
} SituationAttachmentLoadOp;

/**
 * @brief Specifies how an attachment's contents should be treated at the end of a render pass.
 *
 * `SIT_STORE_OP_STORE` keeps the attachment for sampling, present, or a later pass.
 * `SIT_STORE_OP_DONT_CARE` allows the backend to discard the attachment after the pass
 * (typical for transient depth on the main window).
 */
typedef enum {
    SIT_STORE_OP_STORE,     // The rendered contents will be stored in memory for later access.
    SIT_STORE_OP_DONT_CARE  // The rendered contents are not needed after the pass and can be discarded.
} SituationAttachmentStoreOp;

/**
 * @brief Clear values supplied when a begin-pass load op is `SIT_LOAD_OP_CLEAR`.
 *
 * Each attachment reads only the fields relevant to its aspect (`color`, `depth`, `stencil`).
 * Color components are 0–255 (`ColorRGBA`). Depth is normalized 0.0–1.0. Stencil is an integer mask
 * value passed to the backend when stencil aspects are supported on the active target.
 */
typedef struct {
    ColorRGBA color;
    float     depth;
    uint32_t  stencil;
} SituationClearValue;

/** @brief Attachment bits used by SituationCmdClear. */
typedef enum {
    SIT_CLEAR_COLOR_BIT   = 0x1,
    SIT_CLEAR_DEPTH_BIT   = 0x2,
    SIT_CLEAR_STENCIL_BIT = 0x4
} SituationClearFlags;

/** @brief Buffer layout consumed by SituationCmdDispatchIndirect. */
typedef struct {
    uint32_t group_count_x;
    uint32_t group_count_y;
    uint32_t group_count_z;
} SituationDispatchIndirectCommand;

/** @brief Buffer layout consumed by SituationCmdDrawIndirect (matches VkDrawIndirectCommand / GL draw arrays indirect). */
typedef struct {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
} SituationDrawIndirectCommand;

/** @brief Buffer layout consumed by SituationCmdDrawIndexedIndirect (matches VkDrawIndexedIndirectCommand / GL draw elements indirect). */
typedef struct {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
} SituationDrawIndexedIndirectCommand;

/** @brief Load/store/clear configuration for one render-pass attachment aspect. */
typedef struct {
    SituationAttachmentLoadOp  loadOp;
    SituationAttachmentStoreOp storeOp;
    SituationClearValue        clear;
} SituationAttachmentInfo;

/**
 * @brief Complete configuration for `SituationCmdBeginRenderPass`.
 *
 * **Target:** `display_id == -1` renders to the main window swapchain; `display_id >= 0` renders
 * to a Virtual Display FBO/texture. Future `SituationRenderTarget` handles (v2.5 Phase 6) will
 * extend this model without breaking single-target users.
 *
 * **Attachments:** Color, depth, and stencil are configured independently. On combined depth/stencil
 * surfaces, depth and stencil load/store ops map to the corresponding aspects of the same backend
 * attachment. Stencil begin-pass clears are only honored when the active target exposes stencil;
 * otherwise backends may ignore stencil load ops or return `SITUATION_ERROR_NOT_IMPLEMENTED` for
 * stencil-specific mid-pass clears.
 *
 * **Helpers:** `SituationRenderPassInfoDefault` (clear color+depth) and `SituationRenderPassInfoLoad`
 * (preserve all attachments) cover the two most common begin-pass patterns.
 */
typedef struct {
    int                     display_id;     // The render target (-1 for main window, >= 0 for a Virtual Display).
    SituationAttachmentInfo color_attachment;
    SituationAttachmentInfo depth_attachment;
    SituationAttachmentInfo stencil_attachment;
} SituationRenderPassInfo;

/** @brief Typical begin-pass: clear color and depth, store color, discard depth after the pass. */
static inline SituationRenderPassInfo SituationRenderPassInfoDefault(int display_id, ColorRGBA clear_color) {
    SituationRenderPassInfo info = {0};
    info.display_id = display_id;
    info.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.color_attachment.storeOp = SIT_STORE_OP_STORE;
    info.color_attachment.clear.color = clear_color;
    info.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    info.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    info.depth_attachment.clear.depth = 1.0f;
    info.stencil_attachment.loadOp = SIT_LOAD_OP_DONT_CARE;
    info.stencil_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    return info;
}

/** @brief Resume/composite begin-pass: preserve existing attachment contents (no begin-pass clear). */
static inline SituationRenderPassInfo SituationRenderPassInfoLoad(int display_id) {
    SituationRenderPassInfo info = {0};
    info.display_id = display_id;
    info.color_attachment.loadOp = SIT_LOAD_OP_LOAD;
    info.color_attachment.storeOp = SIT_STORE_OP_STORE;
    info.depth_attachment.loadOp = SIT_LOAD_OP_LOAD;
    info.depth_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    info.stencil_attachment.loadOp = SIT_LOAD_OP_LOAD;
    info.stencil_attachment.storeOp = SIT_STORE_OP_DONT_CARE;
    return info;
}

/**
 * @brief Deterministic configuration key from load/store ops and target class (main vs offscreen).
 *
 * Clear values, display index (among VDs), and MSAA sample count are **not** part of the key.
 * Used for Vulkan render-pass caching when dynamic offscreen passes ship (Phase 11+).
 */
static inline uint32_t SituationRenderPassConfigurationKey(const SituationRenderPassInfo* info) {
    if (!info) {
        return 0u;
    }
    uint32_t key = 0u;
    key |= (info->display_id == -1) ? 0u : 1u;
    key |= ((uint32_t)info->color_attachment.loadOp & 3u) << 1;
    key |= ((uint32_t)info->depth_attachment.loadOp & 3u) << 3;
    key |= ((uint32_t)info->stencil_attachment.loadOp & 3u) << 5;
    key |= ((uint32_t)info->color_attachment.storeOp & 3u) << 7;
    key |= ((uint32_t)info->depth_attachment.storeOp & 3u) << 9;
    key |= ((uint32_t)info->stencil_attachment.storeOp & 3u) << 11;
    return key;
}

/**
 * @brief Window State Management
 * @details These flags are now custom defines, their values are arbitrary but must be unique bits. Their functionality will be mapped to GLFW operations.
 */
#define SITUATION_FLAG_WINDOW_TOPMOST           0x00000001  // GLFW_FLOATING (default-on at SituationInit)
#define SITUATION_FLAG_WINDOW_HIDDEN            0x00000002  // glfwHideWindow/ShowWindow
#define SITUATION_FLAG_WINDOW_FROZEN            0x00000004  // Conceptual, app-defined
#define SITUATION_FLAG_FULLSCREEN_MODE          0x00000008  // glfwSetWindowMonitor
#define SITUATION_FLAG_WINDOW_UNDECORATED       0x00000010  // GLFW_DECORATED attribute
#define SITUATION_FLAG_WINDOW_ALWAYS_RUN        0x00000020  // No direct GLFW equivalent, typically always runs when visible
#define SITUATION_FLAG_WINDOW_MINIMIZED         0x00000040  // glfwIconifyWindow, GLFW_ICONIFIED attribute
#define SITUATION_FLAG_WINDOW_MAXIMIZED         0x00000080  // glfwMaximizeWindow, GLFW_MAXIMIZED attribute
#define SITUATION_FLAG_WINDOW_UNFOCUSED         0x00000100  // Queryable state (GLFW_FOCUSED), not settable
#define SITUATION_FLAG_WINDOW_RESIZABLE         0x00000200  // GLFW_RESIZABLE attribute
#define SITUATION_FLAG_BORDERLESS_WINDOWED_MODE 0x00000400  // Achieved via undecorated + specific size/pos
#define SITUATION_FLAG_MSAA_4X_HINT             0x00000800  // glfwWindowHint(GLFW_SAMPLES, 4)
#define SITUATION_FLAG_VSYNC_HINT               0x00001000  // glfwSwapInterval(1)

typedef enum { // Enum can still use the defines for clarity in API
    SITUATION_WINDOW_STATE_ON_TOP         = SITUATION_FLAG_WINDOW_TOPMOST,
    SITUATION_WINDOW_STATE_HIDDEN         = SITUATION_FLAG_WINDOW_HIDDEN,
    SITUATION_WINDOW_STATE_FROZEN         = SITUATION_FLAG_WINDOW_FROZEN,
    SITUATION_WINDOW_STATE_FULLSCREEN     = SITUATION_FLAG_FULLSCREEN_MODE,
    SITUATION_WINDOW_STATE_UNDECORATED    = SITUATION_FLAG_WINDOW_UNDECORATED,
    SITUATION_WINDOW_STATE_ALWAYS_RUN     = SITUATION_FLAG_WINDOW_ALWAYS_RUN,
    SITUATION_WINDOW_STATE_MINIMIZED      = SITUATION_FLAG_WINDOW_MINIMIZED,
    SITUATION_WINDOW_STATE_MAXIMIZED      = SITUATION_FLAG_WINDOW_MAXIMIZED,
    // SITUATION_WINDOW_STATE_UNFOCUSED   // Removed as it's not a settable state directly
    SITUATION_WINDOW_STATE_RESIZABLE      = SITUATION_FLAG_WINDOW_RESIZABLE,
    SITUATION_WINDOW_STATE_BORDERLESS     = SITUATION_FLAG_BORDERLESS_WINDOWED_MODE,
    SITUATION_WINDOW_STATE_MSAA_4X_HINT   = SITUATION_FLAG_MSAA_4X_HINT,
    SITUATION_WINDOW_STATE_VSYNC_HINT     = SITUATION_FLAG_VSYNC_HINT
} SituationWindowStateFlags;

// In your global state or a new rendering state struct
typedef struct {
    mat4 view;
    mat4 projection;
    // Add other per-view data here like camera position
} ViewDataUBO;

#ifdef SITUATION_IMPLEMENTATION
#if defined(SITUATION_USE_VULKAN)
// Forward declare VMA types used in public-facing structs
struct VmaAllocation_T;
typedef struct VmaAllocation_T* VmaAllocation;
#endif
#endif

/**
 * @brief Opaque handle for a compute pipeline.
 * @details In OpenGL, this represents a linked shader program containing only a compute shader.
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationComputePipeline;

/**
 * @brief Opaque handle for a generic GPU data buffer (e.g., an SSBO).
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
    size_t size_in_bytes; // Cached metadata
    SituationBufferUsageFlags usage_flags; // Cached metadata
} SituationBuffer;

// SAFETY: SituationBuffer handle identity is packed into a uint64_t (slot_index + generation = 8 bytes).
// If you add fields, they will NOT be transmitted through the command buffer. Only slot_index and
// generation are used for handle lookup. This assert ensures the packable portion stays at offset 0.
#define SIT_BUFFER_HANDLE_PACK_SIZE 8  // bytes: slot_index(4) + generation(4)
_Static_assert(
    offsetof(SituationBuffer, generation) + sizeof(uint32_t) == SIT_BUFFER_HANDLE_PACK_SIZE,
    "SituationBuffer handle fields (slot_index + generation) must be the first 8 bytes for command buffer packing"
);


// --- Cross-Platform Resource Validation Helpers ---


/**
 * @brief Represents a mesh of vertices and indices stored on the GPU.
 * @details This is an opaque handle to the underlying graphics resources (VBO/EBO/VAO for OpenGL, VkBuffers for Vulkan).
        The library manages the creation and destruction of these resources.
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
    int index_count;        // Cached metadata
    int vertex_count;       // Cached metadata
    size_t vertex_stride;   // Cached metadata
} SituationMesh;

// Forward-declaration for the internal uniform map implementation struct.
// The full definition is hidden inside the SITUATION_IMPLEMENTATION block.
struct _SituationUniformMap;

// Enums for the 'type' parameter
typedef enum {
    SIT_UNIFORM_FLOAT,
    SIT_UNIFORM_VEC2,
    SIT_UNIFORM_VEC3,
    SIT_UNIFORM_VEC4,
    SIT_UNIFORM_INT,
    SIT_UNIFORM_IVEC2,
    SIT_UNIFORM_IVEC3,
    SIT_UNIFORM_IVEC4,
    SIT_UNIFORM_MAT4
} SituationUniformType;

// --- Shader Handle ---
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationShader;

/**
 * @brief Opaque handle for a generic GPU texture resource.
 * @details Uses an Indirect Handle system (Index + Generation) to allow safe hot-reloading.
 */
typedef struct {
    uint32_t slot_index;    // Index into the internal texture registry
    uint32_t generation;    // Validation ID to detect use-after-free
    int width;              // Cached metadata
    int height;             // Cached metadata
} SituationTexture;

/**
 * @brief Represents a single drawable part of a larger 3D model.
 * @details A model can be composed of multiple sub-meshes, each with its own material properties and GPU mesh resource.
 */
typedef struct SituationModelMesh {
    char name[SITUATION_MAX_DEVICE_NAME_LEN]; // Name of the mesh from the model file
    SituationMesh gpu_mesh;                   // The handle to the GPU vertex/index data

    // --- PBR Material Properties ---
    // These are loaded directly from the GLTF material definition.
    Vector4 base_color_factor;                // The base color tint (RGBA)
    float metallic_factor;                    // How metallic the surface is [0-1]
    float roughness_factor;                   // How rough the surface is [0-1]
    Vector3 emissive_factor;                  // The color of light emitted by the surface

    // --- Texture Handles ---
    // These point to textures that are also part of the model.
    SituationTexture base_color_texture;      // Albedo/Diffuse map
    SituationTexture metallic_roughness_texture; // Packed Metal (R), Rough (G) map
    SituationTexture normal_texture;          // Normal map
    SituationTexture occlusion_texture;       // Ambient Occlusion map
    SituationTexture emissive_texture;        // Emissive/Glow map
} SituationModelMesh;

/**
 * @brief Represents a complete 3D model, loaded from a file.
 * @details This is a container for all the meshes and materials that make up a model.
 *          It is the result of a call to SituationLoadModel.
 */
typedef struct SituationModel {
    uint32_t slot_index;
    uint32_t generation;
    int mesh_count;             // Cached metadata
    SituationModelMesh* meshes; // Pointer to meshes (valid until unloaded)
} SituationModel;

// --- Virtual Display Structures ---
typedef struct {
    int      id;                     // Unique sequential ID assigned at creation (used internally for tracking)
    Vector2  resolution;             // Render resolution of this virtual display (width, height in pixels)
    Vector2  offset;                 // Top-left screen position when composited to the main window (in screen pixels)
    float    opacity;                // Global alpha multiplier for the entire display (0.0f = fully transparent, 1.0f = opaque)
    bool     visible;                // If false, the display is skipped entirely during compositing
    int      z_order;                // Sorting key for compositing order — lower values are drawn first (background → foreground)

    // ── Independent Timing & Animation System (allows retro slowdown, bullet-time, UI-independent speed, etc.) ──
    uint64_t frame_count;                // Number of frames this virtual display has advanced (independent of main window)
    double   frame_time_multiplier;      // Speed multiplier (1.0 = normal, 0.5 = half speed, 2.0 = double speed, etc.)
    double   elapsed_time_seconds;       // Total time this display has been running (affected by frame_time_multiplier)
    float    cycle_animation_value;      // Oscillating value 0.0..1.0..0.0 useful for cheap pulsing/shake effects
    double   last_update_time_seconds;   // Timestamp of the last frame advance (used for delta calculation)
    double   frame_delta_time_seconds;   // Delta time for this virtual display's last frame (affected by multiplier)

    // ── Optimization & Compositing Controls ──
    bool                    is_dirty;       // Set to true when content changed → forces re-render of the off-screen buffer
    SituationScalingMode    scaling_mode;   // How the VD is scaled when composited (Integer, Fit, Stretch, etc.)
    SituationBlendMode      blend_mode;     // Blending style when compositing (Alpha, Additive, Overlay, Soft Light, Screen Grab, etc.)

    // ── Backend-Specific GPU Resources (only compiled in the implementation file) ──
#if defined(SITUATION_IMPLEMENTATION)
#if defined(SITUATION_USE_VULKAN)
    struct {
        VkImage         image;               // Device-local color image
        VmaAllocation   image_memory;        // VMA allocation handle for the color image
        VkImageView     image_view;          // Color attachment view
        VkImage         depth_image;         // Depth-stencil image (if enabled)
        VmaAllocation   depth_image_memory;  // VMA allocation for depth image
        VkImageView     depth_image_view;    // Depth attachment view
        VkFramebuffer   framebuffer;         // Framebuffer that references the above images
        VkSampler       sampler;             // Sampler used when sampling this VD as a texture
        VkRenderPass    render_pass;         // Dedicated render pass (one per VD for maximum compatibility/flexibility)
        VkDescriptorSet descriptor_set;      // Pre-allocated descriptor set for ultra-fast compositing (Velocity era)
        VkDescriptorPool descriptor_pool;    // [FIX v2.3.27B]
    } vk;
#elif defined(SITUATION_USE_OPENGL)
    struct {
        GLuint fbo_id;          // Framebuffer Object ID
        GLuint texture_id;      // Color attachment texture (GL_TEXTURE_2D)
        GLuint depth_rbo_id;    // Renderbuffer Object for depth/stencil (optional but usually present)
    } gl;
#endif
#else
    // Opaque placeholder for backend data to ensure vd->gl.texture_id compiles
    // for code outside the implementation block, even if it can't be used.
    struct {
        uint64_t _internal_handle_1;
        uint64_t _internal_handle_2;
        uint64_t _internal_handle_3;
    } gl;
#endif
} SituationVirtualDisplay;

/**
 * @brief manage a loaded font
 */
typedef struct SituationFont {
    void *fontData;                                 // The raw data buffer of the .ttf file
    void *stbFontInfo;                              // A pointer to the stbtt_fontinfo struct

    // [NEW] GPU-side data for real-time rendering
    SituationTexture atlas_texture;
    void* glyph_info; // Pointer to stbtt_bakedchar array
    int atlas_width;
    int atlas_height;
    float font_height_pixels; // The size this atlas was baked at

    // [v2.3.38] Bitmap Font Support
    bool is_bitmap;
    const unsigned char* bitmap_data;
    int bitmap_width;   // Width of one character (e.g. 8)
    int bitmap_height;  // Height of one character (e.g. 8)
    int bitmap_count;   // Number of characters (e.g. 256)
} SituationFont;

// --- Audio Control Structures ---

// --- Audio Handle System ---
// --- Audio Handle System ---
// SituationSoundHandle is deprecated/aliased to the new handle struct
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationSound;

typedef SituationSound SituationSoundHandle;
#define SITUATION_NULL_HANDLE ((SituationSound){0, 0})
#define SITUATION_MAX_LOADED_SOUNDS 1024

typedef struct {
    int sample_rate;
    int channels;
    int bit_depth;
} SituationAudioFormat;

// --- Mixer & Device Types (Phase 0/1) ---
typedef enum {
    SIT_AUDIO_DEVICE_TYPE_UNKNOWN     = 0,
    SIT_AUDIO_DEVICE_TYPE_PLAYBACK    = 1,   // Output only (speakers, headphones)
    SIT_AUDIO_DEVICE_TYPE_CAPTURE     = 2,   // Input only (microphones, line-in)
    SIT_AUDIO_DEVICE_TYPE_DUPLEX      = 3,   // Both input and output (most sound cards)
    SIT_AUDIO_DEVICE_TYPE_LOOPBACK    = 4    // System loopback (for desktop audio capture)
} SituationAudioDeviceType;

typedef struct {
    char name[256];                    // Human-readable name
    char id[128];                      // Backend-specific unique identifier (stringified)
    ma_device_id native_id;            // [INTERNAL] Raw miniaudio ID

    SituationAudioDeviceType type;

    uint32_t min_channels_in;          // Minimum supported input channels
    uint32_t max_channels_in;          // Maximum supported input channels
    uint32_t min_channels_out;         // Minimum supported output channels
    uint32_t max_channels_out;         // Maximum supported output channels

    uint32_t preferred_sample_rate;    // Preferred / native sample rate
    uint32_t native_format;            // Preferred miniaudio format (f32, s16, etc.)

    bool is_default_playback;          // Is this the system's default output?
    bool is_default_capture;           // Is this the system's default input?

    uint32_t latency_us;               // Estimated latency in microseconds
} SituationAudioDeviceInfo;

// --- Mixer Data Structures ---
typedef struct SituationAudioMixer SituationAudioMixer;
typedef struct SituationAudioTrack SituationAudioTrack;
typedef struct SituationAudioBus SituationAudioBus;

// ================================================================================================
// NODE GRAPH & DEVICE REGISTRY TYPES (Phase 3-5)
// ================================================================================================

// --- Configuration Constants ---
#define SITUATION_MAX_DEVICES           	64      // Maximum number of registered device types
#define SITUATION_MAX_DEVICE_NAME       	64      // Maximum length of device name
#define SITUATION_MAX_CONTROL_NAME      	32      // Maximum length of control parameter name
#define SITUATION_MAX_CONTROLS_PER_DEVICE 	48    	// Maximum controls per device (Tone Synth uses 34)
#define SITUATION_MAX_NODES             	256     // Maximum nodes in a graph
#define SITUATION_MAX_PATCHES_PER_PORT  	16      // Maximum connections per port
#define SITUATION_MAX_AUDIO_BUFFER      	2048    // Maximum audio buffer size (frames)

// --- Device Categories ---
typedef enum {
    SITUATION_DEVICE_EFFECT = 0,    // Audio effects (reverb, delay, distortion, etc.)
    SITUATION_DEVICE_SOURCE,        // Audio generators (tone synth, sample playback)
    SITUATION_DEVICE_CAPTURE,       // Audio input devices (microphone, line-in)
    SITUATION_DEVICE_UTILITY,       // Routing/mixing utilities (panner, gain, mixer)
    SITUATION_DEVICE_MODULATOR,     // Control signal generators (LFO, envelope follower)
    SITUATION_DEVICE_ANALYZER,      // Analysis tools (spectrum, meter, oscilloscope)
    SITUATION_DEVICE_CUSTOM         // User-defined custom devices
} SituationDeviceCategory;

// --- Control Parameter Types ---
typedef enum {
    SITUATION_CONTROL_FLOAT = 0,    // Floating point value (e.g., 0.0 to 1.0)
    SITUATION_CONTROL_INT,          // Integer value (e.g., 0, 1, 2 for modes)
    SITUATION_CONTROL_BOOL,         // Boolean value (0 or 1)
    SITUATION_CONTROL_ENUM          // Enumerated choice (stored as int, with string labels)
} SituationControlType;

// --- Device Type Identifiers ---
typedef enum {
    // Effects
    SITUATION_NODE_REVERB = 0,
    SITUATION_NODE_ECHO,
    SITUATION_NODE_CHORUS,
    SITUATION_NODE_PHASER,
    SITUATION_NODE_OVERDRIVE,
    SITUATION_NODE_EXCITER,
    SITUATION_NODE_MAXIMIZER,
    SITUATION_NODE_SPRING_REVERB,
    SITUATION_NODE_STUDIO_REVERB,
    SITUATION_NODE_SST282,
    SITUATION_NODE_DYNAMICS,
    SITUATION_NODE_COMPANDER,
    SITUATION_NODE_EQ_4BAND,
    SITUATION_NODE_FILTER,
    SITUATION_NODE_MASTERING_AMP,
    SITUATION_NODE_DEAFMAX,
    // Utilities
    SITUATION_NODE_PANNER,
    SITUATION_NODE_GAIN,
    SITUATION_NODE_MIXER,
    // Sources
    SITUATION_NODE_SOUND_SOURCE,
    SITUATION_NODE_TONE_SYNTH,
    // Capture
    SITUATION_NODE_MIC_CAPTURE,
    // Modulators
    SITUATION_NODE_LFO,
    SITUATION_NODE_ENVELOPE_FOLLOWER,
    // Analyzers
    SITUATION_NODE_SPECTRUM_ANALYZER,
    SITUATION_NODE_PEAK_METER,
    // Custom devices start here
    SITUATION_NODE_CUSTOM = 1000
} SituationNodeType;

// --- Control Parameter Descriptor ---
typedef struct {
    uint32_t id;
    char name[SITUATION_MAX_CONTROL_NAME];
    SituationControlType type;
    float min_value;
    float max_value;
    float default_value;
    const char** enum_labels;
    int enum_count;
    const char* units;
    bool is_logarithmic;
} SituationControlDesc;

// --- Device Metadata ---
typedef struct {
    SituationNodeType type;
    char name[SITUATION_MAX_DEVICE_NAME];
    SituationDeviceCategory category;
    uint8_t num_audio_ins;
    uint8_t num_audio_outs;
    uint8_t audio_channels;
    uint8_t num_ctrl_ins;
    uint8_t num_ctrl_outs;
    SituationControlDesc controls[SITUATION_MAX_CONTROLS_PER_DEVICE];
    uint8_t num_controls;
    uint32_t latency_samples;
    const char* description;
    const char* author;
    uint32_t version;
    void* (*create_func)(void);
    void (*destroy_func)(void*);
    void (*process_func)(void*, float**, float**, uint32_t);
    void (*set_control_func)(void*, uint32_t, float);
    float (*get_control_func)(void*, uint32_t);
} SituationDeviceMetadata;

// --- Node Handle ---
typedef uint32_t SituationNodeHandle;
#define SITUATION_INVALID_NODE_HANDLE 0xFFFFFFFF

// --- Audio Port ---
typedef struct {
    float* buffer;
    int channels;
    int frames;
} SituationAudioPort;

// --- Control Port ---
typedef struct {
    float value;
    bool is_modulated;
} SituationControlPort;

// --- Patch Connection ---
typedef struct {
    SituationNodeHandle src_node;
    int src_port;
    SituationNodeHandle dst_node;
    int dst_port;
    bool is_control;
} SituationPatch;

// --- Forward Declarations ---
typedef struct SituationNode SituationNode;
typedef struct SituationAudioGraph SituationAudioGraph;

// --- Device Function Table (for processing) ---
typedef struct {
    SituationNodeType type;
    void* (*create)(const SituationDeviceMetadata*);
    void (*destroy)(void*);
    void (*process)(void*, SituationAudioPort*, SituationAudioPort*, float*, int);
} SituationDeviceFunctions;

/**
 * @brief Strategy for loading audio data from disk.
 * @details This enum allows the user to control the trade-off between RAM usage and CPU/Disk latency.
 *
 * - **SITUATION_AUDIO_LOAD_AUTO:** The recommended default. Automatically selects the strategy based on file duration.
 *   Files shorter than ~10 seconds are fully decoded to RAM (safest for SFX). Longer files are streamed.
 * - **SITUATION_AUDIO_LOAD_FULL:** Forces the entire audio file to be decoded into a raw PCM buffer in RAM upon load.
 *   - *Pros:* Zero disk I/O during playback; impossible to stutter during gameplay; perfectly thread-safe.
 *   - *Cons:* Higher RAM usage. High load times for long music tracks.
 * - **SITUATION_AUDIO_LOAD_STREAM:** Forces the audio engine to read from the file on disk during playback.
 *   - *Pros:* Minimal RAM usage; instant load times.
 *   - *Cons:* Risk of audio stuttering if the OS disk cache misses or if the drive is busy (e.g., loading textures).
 */
typedef enum {
    SITUATION_AUDIO_LOAD_AUTO,   // Library decides based on file size (<10 sec -> RAM)
    SITUATION_AUDIO_LOAD_FULL,   // Force full decode to RAM (Safest, best for SFX)
    SITUATION_AUDIO_LOAD_STREAM  // Force disk streaming (Best for long Music)
} SituationAudioLoadMode;

typedef enum {
    SIT_WAVE_SINE,      // Pure tone
    SIT_WAVE_SQUARE,    // Retro/8-bit sound (has harmonics)
    SIT_WAVE_TRIANGLE,  // Mellow, flute-like
    SIT_WAVE_SAW,       // Harsh, string-like
    SIT_WAVE_NOISE      // White noise (for percussion/explosions)
} SituationWaveType;

typedef enum {
    SITUATION_FILTER_NONE,
    SITUATION_FILTER_LOWPASS,
    SITUATION_FILTER_HIGHPASS
} SituationFilterType;

// SituationSound struct definition has moved to internal implementation (Generational Handle used in API)

// --- Resonance (Procedural Synthesis) ---
/**
 * @brief Handle for an actively playing procedural tone.
 *        Invalid/expired handle is 0.
 */
typedef uint32_t SituationToneHandle;  // 0 = invalid

/**
 * @brief Plays an extended procedural tone with full control.
 *
 * @param type          Waveform type (Sine, Square, Triangle, Saw, Noise)
 * @param frequency     Frequency in Hz (e.g., 440.0f). For noise: ignored (use 0.0f)
 * @param volume        Peak volume (0.0 to 1.0)
 * @param pan           Stereo panning (-1.0 left, 0.0 center, +1.0 right)
 * @param attack_sec    Attack time in seconds
 * @param decay_sec     Decay time in seconds
 * @param sustain_level Sustain volume level (0.0 to 1.0)
 * @param release_sec   Release time in seconds
 * @param hold_sec      Hold duration in seconds. Use -1.0f for infinite sustain (key down)
 *
 * @return Handle to the playing tone, or 0 if no voice available (polyphony limit)
 */
SITAPI SituationToneHandle SituationPlayToneEx(
    SituationWaveType type,
    float frequency,
    float volume,
    float pan,
    float attack_sec,
    float decay_sec,
    float sustain_level,
    float release_sec,
    float hold_sec
);

// --- Temporal Oscillator System (Global High-Precision Timing & Rhythm Engine) ---
// This subsystem powers the advanced "Temporal Oscillator" feature set — a deterministic,
// high-resolution metronome/beat-sync system capable of driving music-reactive events,
// gameplay rhythms, animation pulses, procedural sequencing, etc.
// All oscillators run on the same global timebase but can have independent periods and phases.

// --- Timer System Structures ---
#define SITUATION_MAX_OSCILLATORS 			256
#define SITUATION_TIMER_GRID_PERIOD_EDGES 	60.0
#define SITUATION_TIMER_GRIDILON 			1.182940076

typedef struct {
    // ── Oscillator Configuration ──
    double   period_seconds[SITUATION_MAX_OSCILLATORS];     // Period of each oscillator in seconds (e.g. 0.5 = 120 BPM, 1/4 note)

    // ── Deterministic Pseudo-Random State (xoshiro256** derived - 256-bit state) ──
    // Used for repeatable "random" pulses, shakes, or procedural events that must stay perfectly in sync across runs/recordings
    uint64_t state_current[4];      // Current 256-bit RNG state (xoshiro256** algorithm)
    uint64_t state_previous[4];     // Previous frame state — enables perfect reverse playback or rewind debugging

    // ── Per-Oscillator Runtime State ──
    uint64_t trigger_count[SITUATION_MAX_OSCILLATORS];          // How many times this oscillator has fired since init (rolls over safely)
    double   next_trigger_time_seconds[SITUATION_MAX_OSCILLATORS]; // Absolute time when the next trigger is scheduled
    double   last_ping_time_seconds[SITUATION_MAX_OSCILLATORS];    // Time of the most recent trigger (for phase/duty queries)
    double   anchor_time_seconds[SITUATION_MAX_OSCILLATORS];       // Absolute start time for drift-free trigger calculation

    // ── Global Timebase ──
    double   current_system_time_seconds;   // Monotonically increasing high-resolution time (updated every frame via SituationUpdateTimers())

    // ── Initialization Guard ──
    bool     is_initialized;                // True after first call to SituationUpdateTimers()
} SituationTimerSystem;

// --- Initialization Configuration Structure (Passed to SituationInit) ---
typedef struct {
    // ── Window Creation Parameters ──
    int          window_width;              // Initial window width in screen coordinates
    int          window_height;             // Initial window height in screen coordinates
    const char*  window_title;              // Window title bar text (UTF-8)

    // ── Window State Flags (Applied via GLFW window hints or direct state changes) ──
    uint32_t     initial_active_window_flags;    // Flags when window has focus (e.g. SIT_WINDOW_BORDERLESS | SIT_WINDOW_VSYNC)
    uint32_t     initial_inactive_window_flags;  // Flags when window is unfocused (e.g. pause rendering or reduce refresh rate)

    // ── Vulkan-Specific Options ──
    bool         enable_vulkan_validation;       // Enable VK_LAYER_KHRONOS_validation (debug builds only - auto-disabled in release)
    bool         force_single_queue;             // Force shared compute/graphics queue (debug/compatibility)
    uint32_t     max_frames_in_flight;           // Override SITUATION_MAX_FRAMES_IN_FLIGHT (usually 2 or 3)

    // Optional: Provide custom Vulkan instance extensions (e.g. for VR, ray tracing, etc.)
    const char** required_vulkan_extensions;     // Array of extension names (null or empty = use defaults)
    uint32_t     required_vulkan_extension_count;// Length of the above array

    // ── Engine Feature Flags ──
    uint32_t     flags;  // Bitfield:
                         //   SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD → route mic capture callbacks to main thread
                         //   (future-proof expansion slot)

    // ── Audio Configuration ──
    uint32_t     max_audio_voices; // Max concurrent audio voices. 0 = Unlimited (Dynamic).

#if defined(SITUATION_ENABLE_RENDER_THREAD)
    int          render_thread_count; // Number of render threads to spawn (0 = Single Threaded)
    // [v2.3.22] Backpressure Policy
    // Determines behavior when the render queue is full (Depth >= Max Frames)
    // 0: Spin (Low Latency, High CPU), 1: Yield (Balanced), 2: Sleep (Low CPU)
    int          backpressure_policy;
#endif

    // [v2.3.34] Async I/O
    uint32_t     io_queue_capacity; // Size of the IO queue (Low Priority). Default: 1024.

    // [v2.3.37] I/O Configuration
    bool disable_io_thread;         // If true, runs I/O tasks on main thread (fallback)
    double hot_reload_poll_rate;    // Seconds between checks (default 0.5). 0 = disable.
    uint64_t staging_buffer_size;   // Override default 128MB staging buffer size (Vulkan only). 0 = Default.

    // [Threading Bolstering] Optional thread affinity masks (logical CPU bit N = 1ULL << N).
    // thread_affinity_main: 0 = no pin. render/audio: 0 = default cores 1/2 (or NUMA-local when numa_prefer_local).
    // Masks wider than 64 bits are truncated on Windows. Affinity failures are fail-soft (warning, init continues).
    uint64_t thread_affinity_main;
    uint64_t thread_affinity_render;
    uint64_t thread_affinity_audio;

    // [Threading Bolstering — Epic C] NUMA placement (requires SituationRefreshCpuTopology at init).
    bool  numa_prefer_local;       /* If true and affinity mask is 0, pin render/audio to the NUMA node of default cores */
    bool  worker_numa_spread;      /* Pin pool worker i to NUMA node (i % node_count) at worker entry.
                                      Defaults to true when SITUATION_ENABLE_THREADING is defined
                                      (via SITUATION_WORKER_NUMA_SPREAD_DEFAULT build flag). */
    int32_t io_thread_numa_node;   /* Dedicated I/O thread NUMA node; < 0 = no pin */

    // [Threading Bolstering — Epic D] Pool sizing when SituationCreateThreadPool(..., num_threads=0, ...)
    bool     thread_pool_use_physical_cores; /* false = logical CPUs - reserved; true = physical cores - reserved */
    uint32_t thread_pool_reserved_threads;     /* Threads left for main/render/audio/IO (default 4 if 0) */
} SituationInitInfo;

// [v2.3.22] Render Queue Backpressure Policies
typedef enum {
    SIT_RENDER_BACKPRESSURE_SPIN  = 0, // Busy-wait loop (Highest responsiveness, uses CPU)
    SIT_RENDER_BACKPRESSURE_YIELD = 1, // Yield thread slice (OS decides, good balance)
    SIT_RENDER_BACKPRESSURE_SLEEP = 2  // Sleep 1ms (Low CPU usage, worst latency)
} SituationRenderBackpressurePolicy;

// [v2.3.22] Opaque Render List Handle (Momentum)
typedef struct SituationRenderList_t* SituationRenderList;

/**
 * @brief Flags representing optional GPU capabilities and advanced feature sets.
 * @details Used with SituationIsFeatureSupported() to check runtime availability. These flags cover core
 *          rasterization features, compute capabilities, and next-generation rendering techniques.
 */
typedef enum {
    // ── Core Rasterization ──
    SIT_FEATURE_GEOMETRY_SHADER        = 1 << 0,  // Geometry shader support
    SIT_FEATURE_TESSELLATION_SHADER    = 1 << 1,  // Tessellation control/eval shaders
    SIT_FEATURE_WIDE_LINES             = 1 << 2,  // Lines with width > 1.0
    SIT_FEATURE_FILL_MODE_NON_SOLID    = 1 << 3,  // Wireframe/Point rendering (PolygonMode)
    SIT_FEATURE_SAMPLER_ANISOTROPY     = 1 << 4,  // Anisotropic texture filtering
    SIT_FEATURE_MULTI_VIEWPORT         = 1 << 5,  // Multiple viewports/scissors (e.g. for VR/Split-screen without multiple passes)

    // ── Compute & Precision ──
    SIT_FEATURE_COMPUTE_SHADER         = 1 << 6,  // Compute shader support (Standard in Vulkan, GL 4.3+)
    SIT_FEATURE_INT64                  = 1 << 7,  // 64-bit integer support in shaders (int64_t)
    SIT_FEATURE_FLOAT64                = 1 << 8,  // 64-bit float (double) support in shaders
    SIT_FEATURE_FLOAT16                = 1 << 9,  // 16-bit float (half) support for storage/arithmetic (performance/bandwidth optimization)
    SIT_FEATURE_SUBGROUP_OPERATIONS    = 1 << 10, // Subgroup/Wave intrinsics (ballot, shuffle, arithmetic)

    // ── Modern Memory Model (Bindless) ──
    SIT_FEATURE_BINDLESS_BUFFERS       = 1 << 11, // Buffer Device Address / GL_EXT_buffer_reference (Pointers in shaders)
    SIT_FEATURE_BINDLESS_TEXTURES      = 1 << 12, // Descriptor Indexing / Bindless Textures (Arrays of unbounded textures)

    // ── GPU-Driven Rendering ──
    SIT_FEATURE_DRAW_INDIRECT_COUNT    = 1 << 13, // DrawIndirectCount / MultiDrawIndirect with count buffer (GPU culling)
    SIT_FEATURE_MULTI_DRAW_INDIRECT    = 1 << 14, // Standard MultiDrawIndirect support

    // ── Advanced Rendering ──
    SIT_FEATURE_MESH_SHADER            = 1 << 15, // Mesh Shaders (NV/EXT) - Replaces vertex/geometry pipeline
    SIT_FEATURE_RAY_TRACING            = 1 << 16, // Hardware Ray Tracing (KHR_ray_tracing_pipeline / queries)
    SIT_FEATURE_VARIABLE_RATE_SHADING  = 1 << 17, // Variable Rate Shading (VRS) for performance optimization
    SIT_FEATURE_ATOMIC_FLOAT           = 1 << 18, // Atomic operations on floating point images/buffers

    // ── Asset Support ──
    SIT_FEATURE_TEXTURE_COMPRESSION_BC = 1 << 19, // Block Compression (BC1-BC7 / S3TC) support
    SIT_FEATURE_TEXTURE_COMPRESSION_ASTC = 1 << 20, // ASTC Compression support (Mobile/High-end)
    SIT_FEATURE_HDR_OUTPUT             = 1 << 21, // High Dynamic Range display output support (10-bit/16-bit swapchain)

} SituationRenderFeature;

SITAPI bool SituationIsFeatureSupported(SituationRenderFeature feature);                 // Check if a graphics feature is supported on current hardware.

// --- Legacy OpenGL-style barrier bits (kept for low-level compatibility helpers) ---
#define SITUATION_BARRIER_VERTEX_ATTRIB_ARRAY_BIT   		0x00000001
#define SITUATION_BARRIER_ELEMENT_ARRAY_BIT         		0x00000002
#define SITUATION_BARRIER_UNIFORM_BARRIER_BIT       		0x00000004
#define SITUATION_BARRIER_TEXTURE_FETCH_BARRIER_BIT 		0x00000008
#define SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BARRIER_BIT 	0x00000020
#define SITUATION_BARRIER_COMMAND_BARRIER_BIT       		0x00000040
#define SITUATION_BARRIER_PIXEL_BUFFER_BARRIER_BIT  		0x00000080
#define SITUATION_BARRIER_TEXTURE_UPDATE_BARRIER_BIT 		0x00000100
#define SITUATION_BARRIER_BUFFER_UPDATE_BARRIER_BIT 		0x00000200
#define SITUATION_BARRIER_FRAMEBUFFER_BARRIER_BIT   		0x00000400
#define SITUATION_BARRIER_TRANSFORM_FEEDBACK_BARRIER_BIT 	0x00000800
#define SITUATION_BARRIER_ATOMIC_COUNTER_BARRIER_BIT 		0x00001000
#define SITUATION_BARRIER_SHADER_STORAGE_BARRIER_BIT 		0x00002000
#define SITUATION_BARRIER_ALL_BARRIER_BITS          		0xFFFFFFFF

// Aliases to match implementation usage
#define SITUATION_BARRIER_INDEX_BUFFER_BIT          SITUATION_BARRIER_ELEMENT_ARRAY_BIT
#define SITUATION_BARRIER_UNIFORM_BUFFER_BIT        SITUATION_BARRIER_UNIFORM_BARRIER_BIT
#define SITUATION_BARRIER_TEXTURE_FETCH_BIT         SITUATION_BARRIER_TEXTURE_FETCH_BARRIER_BIT
#define SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BIT   SITUATION_BARRIER_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define SITUATION_BARRIER_COMMAND_BIT               SITUATION_BARRIER_COMMAND_BARRIER_BIT
#define SITUATION_BARRIER_SHADER_STORAGE_BIT        SITUATION_BARRIER_SHADER_STORAGE_BARRIER_BIT

//==================================================================================
//  Core Data Types & GPU Resource Semantics - v2.3.4 "Velocity" Standard
//==================================================================================

// ---------------------------------------------------------------------------------
//  Vertex Attribute Data Types (used in SituationVertexAttribute layout descriptions)
// ---------------------------------------------------------------------------------
typedef enum {
    SIT_DATA_BYTE           = 0,  // 8-bit signed integer   (normalized possible)
    SIT_DATA_UNSIGNED_BYTE  = 1,  // 8-bit unsigned integer (normalized possible)
    SIT_DATA_SHORT          = 2,  // 16-bit signed integer
    SIT_DATA_UNSIGNED_SHORT = 3,  // 16-bit unsigned integer
    SIT_DATA_INT            = 4,  // 32-bit signed integer
    SIT_DATA_UNSIGNED_INT   = 5,  // 32-bit unsigned integer
    SIT_DATA_FLOAT          = 6,  // 32-bit IEEE floating point (default for most attributes)
    SIT_DATA_DOUBLE         = 7,  // 64-bit IEEE floating point (rare, only when explicitly needed)
} SituationDataType;

// ---------------------------------------------------------------------------------
//  Memory Allocation Macros (overridable for custom allocators)
// ---------------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------------
//  Compute Shader Source Format Specification
//  Used when creating compute pipelines on backends that support multiple input formats.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_GL_SHADER_SOURCE_TYPE_GLSL   = 0,  // Null-terminated GLSL source string (compiled at runtime via shaderc when enabled)
    SITUATION_GL_SHADER_SOURCE_TYPE_SPIRV  = 1,  // Raw SPIR-V bytecode blob (uint32_t array) - used when pre-compiling offline
} SituationGLShaderSourceType;

// ---------------------------------------------------------------------------------
//  Pipeline Barrier Source Access Flags
//  Describes which previous pipeline stages have written to memory that later stages need to read.
//  Combine with bitwise OR.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BARRIER_VERTEX_SHADER_WRITE   = 1 << 0,   // Vertex shader wrote to SSBO / image
    SITUATION_BARRIER_FRAGMENT_SHADER_WRITE  = 1 << 1,   // Fragment shader wrote to SSBO / image / color attachment
    SITUATION_BARRIER_COMPUTE_SHADER_WRITE   = 1 << 2,   // Compute shader wrote to storage buffer / image
    SITUATION_BARRIER_TRANSFER_WRITE         = 1 << 3,   // Copy/blit/fill operations wrote to buffer/image
} SituationBarrierSrcFlags;

// ---------------------------------------------------------------------------------
//  Pipeline Barrier Destination Access Flags
//  Describes which subsequent pipeline stages will read memory written by earlier stages.
//  Combine with bitwise OR.
// ---------------------------------------------------------------------------------
typedef enum {
    SITUATION_BARRIER_VERTEX_SHADER_READ     = 1 << 0,   // Vertex shader will read SSBO/image
    SITUATION_BARRIER_FRAGMENT_SHADER_READ    = 1 << 1,   // Fragment shader will read SSBO/image/color attachment
    SITUATION_BARRIER_COMPUTE_SHADER_READ     = 1 << 2,   // Compute shader will read storage buffer/image
    SITUATION_BARRIER_TRANSFER_READ           = 1 << 3,   // Copy/blit operations will read from buffer/image
    SITUATION_BARRIER_INDIRECT_COMMAND_READ   = 1 << 4,   // Indirect draw/dispatch buffer will be read by command processor
} SituationBarrierDstFlags;

typedef enum {
    SITUATION_PIPELINE_STAGE_TOP              = 1 << 0,
    SITUATION_PIPELINE_STAGE_INDIRECT_COMMAND = 1 << 1,
    SITUATION_PIPELINE_STAGE_VERTEX_INPUT     = 1 << 2,
    SITUATION_PIPELINE_STAGE_VERTEX_SHADER    = 1 << 3,
    SITUATION_PIPELINE_STAGE_FRAGMENT_SHADER  = 1 << 4,
    SITUATION_PIPELINE_STAGE_COLOR_ATTACHMENT = 1 << 5,
    SITUATION_PIPELINE_STAGE_DEPTH_STENCIL    = 1 << 6,
    SITUATION_PIPELINE_STAGE_COMPUTE_SHADER   = 1 << 7,
    SITUATION_PIPELINE_STAGE_TRANSFER         = 1 << 8,
    SITUATION_PIPELINE_STAGE_HOST             = 1 << 9,
    SITUATION_PIPELINE_STAGE_BOTTOM           = 1 << 10
} SituationPipelineStageFlags;

typedef enum {
    SITUATION_ACCESS_INDIRECT_COMMAND_READ   = 1 << 0,
    SITUATION_ACCESS_VERTEX_READ             = 1 << 1,
    SITUATION_ACCESS_INDEX_READ              = 1 << 2,
    SITUATION_ACCESS_UNIFORM_READ            = 1 << 3,
    SITUATION_ACCESS_SHADER_READ             = 1 << 4,
    SITUATION_ACCESS_SHADER_WRITE            = 1 << 5,
    SITUATION_ACCESS_COLOR_ATTACHMENT_READ   = 1 << 6,
    SITUATION_ACCESS_COLOR_ATTACHMENT_WRITE  = 1 << 7,
    SITUATION_ACCESS_DEPTH_STENCIL_READ      = 1 << 8,
    SITUATION_ACCESS_DEPTH_STENCIL_WRITE     = 1 << 9,
    SITUATION_ACCESS_TRANSFER_READ           = 1 << 10,
    SITUATION_ACCESS_TRANSFER_WRITE          = 1 << 11,
    SITUATION_ACCESS_HOST_READ               = 1 << 12,
    SITUATION_ACCESS_HOST_WRITE              = 1 << 13
} SituationAccessFlags;

typedef struct {
    uint32_t src_stages;
    uint32_t src_access;
    uint32_t dst_stages;
    uint32_t dst_access;
} SituationPipelineBarrierDesc;

typedef struct {
    SituationBuffer buffer;
    size_t offset;
    size_t size;
    uint32_t src_stages;
    uint32_t src_access;
    uint32_t dst_stages;
    uint32_t dst_access;
} SituationBufferBarrierDesc;

/**
 * @brief Backend-neutral texture layouts for explicit image barriers.
 *
 * @details This is a vocabulary for commands such as `SituationCmdTextureBarrier`.
 *          It does not imply automatic layout tracking. Callers must provide the
 *          actual old layout and intended new layout for the texture subresource.
 */
typedef enum {
    SITUATION_TEXTURE_LAYOUT_UNDEFINED = 0,
    SITUATION_TEXTURE_LAYOUT_GENERAL,
    SITUATION_TEXTURE_LAYOUT_SHADER_READ,
    SITUATION_TEXTURE_LAYOUT_TRANSFER_SRC,
    SITUATION_TEXTURE_LAYOUT_TRANSFER_DST,
    SITUATION_TEXTURE_LAYOUT_COLOR_ATTACHMENT,
    SITUATION_TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT,
    SITUATION_TEXTURE_LAYOUT_PRESENT
} SituationTextureLayout;

/**
 * @brief Explicit texture memory/layout barrier for a 2D texture subresource range.
 *
 * @details For the first slice, public textures are treated as color-only 2D images.
 *          `mip_level_count == 0` means one mip level. `array_layer_count == 0`
 *          means one layer. Array layers other than layer 0 are reserved until
 *          array/cube texture ownership is exposed. `old_layout` may be
 *          `SITUATION_TEXTURE_LAYOUT_UNDEFINED`; `new_layout` must be a real
 *          usage layout.
 */
typedef struct {
    SituationTextureLayout old_layout;
    SituationTextureLayout new_layout;
    uint32_t base_mip_level;
    uint32_t mip_level_count;
    uint32_t base_array_layer;
    uint32_t array_layer_count;
} SituationTextureBarrierDesc;


//==================================================================================================
//
//  SITUATION API USAGE GUIDE - v2.3.4 "Velocity"
//
//  This is the canonical reference for correct usage of the Situation library.
//  Every rule here is deliberate and enforced for maximum performance, identical cross-backend
//  behaviour, and long-term stability in production applications.
//
//  Read this once. Then read it again. Then keep it open while you code.
//
//==================================================================================================

/**
 * @section Core Principles (Non-Negotiable)
 *
 * 1. Single-Threaded API
 *    All SITAPI functions (windowing, input polling, rendering, resource creation)
 *    MUST be called from the main thread that called SituationInit().
 *    Background threads may perform pure CPU work or prepare data, but never call the API directly.
 *
 * 2. Update-Before-Draw Contract (CRITICAL FOR BACKEND PARITY)
 *    You MUST update buffers / push constants / textures
 *    THEN you record draw commands.
 *    Never the other way around.
 *    In debug builds the library actively detects violations and aborts with a clear error.
 *    This guarantees pixel-identical results between OpenGL (immediate) and Vulkan (deferred).
 *
 * 3. Explicit Resource Ownership
 *    SituationCreate*  → must be paired with SituationDestroy*
 *    SituationLoad*    → must be paired with SituationUnload*
 *    SituationTakeScreenshot(), SituationGetLastErrorMsg(), SituationGetBasePath(), etc.
 *      → return heap-allocated data → caller must SIT_FREE() or SituationFreeString().
 *    SituationShutdown() performs leak detection and prints warnings for any GPU resource still alive.
 *
 * 4. Handle Pattern (by value) vs Modification (by pointer)
 *    - Use:    SituationCmdDrawMesh(mesh_handle);          // pass by value
 *    - Destroy: SituationDestroyMesh(&mesh_handle);        // pass by pointer → handle is zeroed
 *    This pattern is used everywhere and prevents use-after-free bugs.
 */

/**
 * @section Recommended Main Loop (Velocity-Era Standard)
 *
 * while (!SituationWindowShouldClose()) {
 *     SITUATION_BEGIN_FRAME();           // Polls input + updates timers in correct order
 *
 *     // ── Your Update Logic Here (physics, gameplay, audio triggers, hot-reload checks) ──
 *     SituationCheckHotReloads();        // Optional but highly recommended in development
 *
 *     if (SituationAcquireFrameCommandBuffer()) {
 *         SituationCommandBuffer cmd = SituationGetMainCommandBuffer();
 *
 *         // ── Update GPU data first (buffers, push constants, descriptor binds) ──
 *
 *         // ── Then record draw commands ──
 *         SituationCmdBeginRenderPass(cmd, &main_pass_info);
 *         SituationCmdDrawMesh(cmd, my_mesh);
 *         SituationCmdDrawText(cmd, font, "Situation v2.3.4F", vec2(10,10), WHITE);
 *         SituationCmdEndRenderPass(cmd);
 *
 *         SituationEndFrame();               // Presents + GPU submit
 *     }
 * }
 *
 * This structure is now the official recommended pattern as of v2.3.4.
 */

/**
 * @section Hot-Reloading Workflow (The "Velocity" Killer Feature)
 *
 * In development builds:
 *   - Call SituationCheckHotReloads() once per frame (usually right after SITUATION_BEGIN_FRAME()).
 *   - Any shader, compute pipeline, texture, or GLTF model that was loaded with the normal
 *     SituationLoad* functions will automatically reload when the file on disk changes.
 *   - Original handles remain valid → no need to rebuild materials, UI, or scene graphs.
 *
 * This feature alone typically doubles or triples artist/programmer iteration speed.
 */

/**
 * @section Zero-Friction Features (They Just Work™)
 *
 *  SituationLoadSoundFromFile("music.mp3", SITUATION_AUDIO_LOAD_AUTO, true, &snd);
 *  SituationStartAudioCapture(cb, NULL);           // native mic input → callback (auto-format)
 *  SituationLoadTexture("tex.png", true, &tex);           // mips + hot-reload ready
 *  SituationTakeScreenshot("shot.png");            // always PNG, always works
 *  SituationLoadFont("font.ttf") → SituationBakeFontAtlas() → SituationCmdDrawText()
 *
 * No extra defines, no manual stb includes, no custom writers required.
 */

/**
 * @section Final Checklist Before Shipping
 *
 * [ ] Remove or #ifdef out SituationCheckHotReloads() in release builds
 * [ ] Disable validation layers (SituationInitInfo::enable_vulkan_validation = false)
 * [ ] Verify SituationShutdown() prints "No resource leaks detected"
 * [ ] Confirm no "update-after-draw" assertions in debug builds
 *
 * If you can tick all boxes, you have achieved Situation mastery.
 *
 * Welcome to the Titanium tier.
 */

//==================================================================================
// Core Module: Application Lifecycle and System
//==================================================================================

// --- Application Lifecycle & State ---
SITAPI const char* SituationGetVersionString(void); 									// Returns a read-only static string (e.g., "2.3.3A"). Do not free.
SITAPI SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info); // Initialize the library, create window and graphics context.
SITAPI void SituationPollInputEvents(void);                                             // Poll for all input events (keyboard, mouse, joystick). Call once per frame.
SITAPI void SituationUpdateTimers(void);                                                // Update all internal timers (frame timer, temporal system). Call after polling events.
SITAPI void SituationUpdate(void);                                                      // DEPRECATED: Use SituationPollInputEvents() and SituationUpdateTimers().
SITAPI void SituationShutdown(void);                                                    // Shut down the library and release all resources.
SITAPI bool SituationIsInitialized(void);                                               // Check if the library has been successfully initialized.

SITAPI SituationInitState SituationGetInitState(void);                                  // Query the current initialization state (thread-safe).

SITAPI bool SituationWindowShouldClose(void);                                           // Check if the application should close (e.g., user clicked X).
SITAPI void SituationPauseApp(void);                                                    // Pause the application's internal state (e.g., audio).
SITAPI void SituationResumeApp(void);                                                   // Resume a paused application.
SITAPI bool SituationIsAppPaused(void);                                                 // Check if the application is currently paused.

// --- Frame Timing & FPS Management ---
SITAPI void SituationSetTargetFPS(int fps);                                             // Set a desired frame rate cap (0 for uncapped).
SITAPI float SituationGetFrameTime(void);                                               // Get the time in seconds for the last frame to complete (deltaTime).
SITAPI int SituationGetFPS(void);                                                       // Get the current frames-per-second value.

// --- Callbacks and Event Handling ---
SITAPI SituationError SituationGetLastErrorMsg(char** out_msg);                         // Get the last error message as a string (caller must free).
SITAPI SituationError SituationGetLastErrorCode(void);                                  // Get the SituationError enum from the most recent _SituationSetErrorFromCode call.
SITAPI const char* SituationErrorToString(SituationError err);                          // Human-readable base label for an error code (from the errno table).
SITAPI void SituationSetExitCallback(void (*callback)(void* user_data), void* user_data); // Set a callback to run just before shutdown.
SITAPI void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data); // Set a callback for window framebuffer resize events.
SITAPI void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data); // Set a callback for window focus events.
SITAPI void SituationSetMaximizeCallback(SituationMaximizeCallback callback, void* user_data); // Set a callback for window maximize / restore events.
SITAPI void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data); // Set a callback for file drop events.

// --- Command-Line Argument Queries ---
SITAPI bool SituationIsArgumentPresent(const char* arg_name);                           // Check if a command-line argument (e.g., "-server") was provided.
SITAPI const char* SituationGetArgumentValue(const char* arg_name);                     // Get the value of an argument (e.g., "jungle" from "-level:jungle").

// --- System & Hardware Information ---
SITAPI SituationDeviceInfo SituationGetDeviceInfo(void);                                // Get detailed information about system hardware (CPU, GPU, RAM, etc.).
SITAPI uint32_t SituationGetCPUThreadCount(void);                                       // Get the number of logical CPU cores.
SITAPI const char* SituationGetGPUName(void);											// Get the name of the active GPU.

/** Active renderer backend for this Situation DLL build (OpenGL vs Vulkan). */
typedef enum SituationGraphicsBackend {
    SIT_GRAPHICS_BACKEND_UNKNOWN = 0,
    SIT_GRAPHICS_BACKEND_OPENGL  = 1,
    SIT_GRAPHICS_BACKEND_VULKAN  = 2,
} SituationGraphicsBackend;

/** Which graphics API this DLL was built for. Valid before SituationInit. */
SITAPI SituationGraphicsBackend SituationGetGraphicsBackend(void);
/** Read-only label for SituationGetGraphicsBackend() ("OpenGL", "Vulkan", "Unknown"). */
SITAPI const char* SituationGetGraphicsBackendName(void);

typedef struct SituationGraphicsCaps {
    uint32_t api_version_packed;        /* Situation backend target: (4<<16)|6 OpenGL, (1<<16)|4 Vulkan */
    int      max_msaa_samples;
    int      bindless_textures;
    int      shader_compiler_available;
    int      compute_supported;
    int      max_viewports;             /* GL_MAX_VIEWPORTS / VkPhysicalDeviceLimits::maxViewports (>=1 after init) */
    SituationGraphicsBackend backend;   /* Same as SituationGetGraphicsBackend() after init */
    uint32_t device_api_version_packed; /* Runtime GL context / VkPhysicalDevice version (major<<16|minor) */
} SituationGraphicsCaps;
SITAPI void SituationGetGraphicsCaps(SituationGraphicsCaps* out_caps);                  // Get backend capabilities for examples/frameworks.
SITAPI char* SituationGetUserDirectory(void);                                           // Get the full path to the current user's home directory (caller must free).
#if defined(_WIN32)
SITAPI char SituationGetCurrentDriveLetter(void);                                       // Get the drive letter of the running executable (Windows only).
SITAPI bool SituationGetDriveInfo(char drive_letter, uint64_t* out_total_capacity_bytes, uint64_t* out_free_space_bytes, char* out_volume_name, int volume_name_len); // Get info for a specific drive (Windows only).
#endif // _WIN32

SITAPI void SituationOpenFile(const char* filePath);                                    // Open a file or folder with its default application.
SITAPI int SituationExecuteCommand(const char *cmd, char **output);                     // Execute a shell command hidden, return exit code & combined output.

//==================================================================================
// Window and Display Module
//==================================================================================
// --- Window State Management ---
SITAPI void SituationSetWindowState(uint32_t flags);                                    // Set window configuration state using flags (additive).
SITAPI void SituationClearWindowState(uint32_t flags);                                  // Clear window configuration state flags.
SITAPI void SituationSetVSync(bool enable);                                             // Enable or disable VSync (vertical synchronization).
SITAPI void SituationToggleFullscreen(void);                                            // Toggle window between fullscreen and windowed mode.
SITAPI void SituationToggleBorderlessWindowed(void);                                    // Toggle window between borderless and decorated mode.
SITAPI void SituationMaximizeWindow(void);                                              // Maximize the window if it's resizable.
SITAPI void SituationMinimizeWindow(void);                                              // Minimize the window (iconify).
SITAPI void SituationRestoreWindow(void);                                               // Restore a minimized or maximized window.
SITAPI void SituationSetWindowFocused(void);                                            // Set the window to be focused.

// --- Window Property Management ---
SITAPI void SituationSetWindowTitle(const char *title);                                 // Set the title for the window.
SITAPI void SituationSetWindowIcon(SituationImage image);                               // Set the icon for the window (single image).
SITAPI void SituationSetWindowIcons(SituationImage *images, int count);                 // Set the icon for the window (multiple sizes).
SITAPI void SituationSetWindowPosition(int x, int y);                                   // Set the window position on the screen.
SITAPI void SituationSetWindowSize(int width, int height);                              // Set the window dimensions.
SITAPI void SituationSetWindowMinSize(int width, int height);                           // Set the window minimum dimensions.
SITAPI void SituationSetWindowMaxSize(int width, int height);                           // Set the window maximum dimensions.
SITAPI void SituationSetWindowOpacity(float opacity);                                   // Set window opacity [0.0f to 1.0f].

// --- Window State Queries ---
SITAPI bool SituationIsWindowState(uint32_t flag);                                      // Check if a specific window state flag is set.
SITAPI bool SituationIsWindowFullscreen(void);                                          // Check if the window is currently in fullscreen mode.
SITAPI bool SituationIsWindowHidden(void);                                              // Check if the window is currently hidden.
SITAPI bool SituationIsWindowMinimized(void);                                           // Check if the window is currently minimized.
SITAPI bool SituationIsWindowMaximized(void);                                           // Check if the window is currently maximized.
SITAPI bool SituationHasWindowFocus(void);                                              // Check if the window is currently focused.
SITAPI bool SituationIsWindowResized(void);                                             // Check if the window was resized in the last frame.

// --- Window & Screen Dimension Queries ---
SITAPI int SituationGetScreenWidth(void);                                               // Get the current logical width of the window.
SITAPI int SituationGetScreenHeight(void);                                              // Get the current logical height of the window.
SITAPI int SituationGetRenderWidth(void);                                               // Get the current render width (backbuffer size, considers HiDPI).
SITAPI int SituationGetRenderHeight(void);                                              // Get the current render height (backbuffer size, considers HiDPI).
SITAPI void SituationGetWindowSize(int* width, int* height);                            // Get the current logical window size.
SITAPI Vector2 SituationGetWindowPosition(void);                                        // Get the window's top-left position on the screen.
SITAPI Vector2 SituationGetWindowScaleDPI(void);                                        // Get the DPI scaling factor for the window.

// --- Physical Display (Monitor) Management ---
SITAPI int SituationGetMonitorCount(void);                                              // Get the number of connected monitors.
SITAPI int SituationGetCurrentMonitor(void);                                            // Get the index of the monitor the window is on.
SITAPI SituationError SituationGetDisplays(SituationDisplayInfo** out_displays, int* out_count); // Get information for all displays (caller must free).
SITAPI void SituationFreeDisplays(SituationDisplayInfo* displays, int count);            // Free a display info array returned by SituationGetDisplays.
SITAPI void SituationRefreshDisplays(void);                                             // Force a refresh of the cached display information.
SITAPI SituationError SituationSetDisplayMode(int monitor_id, const SituationDisplayMode* mode, bool fullscreen); // Set the display mode for a monitor.
SITAPI void SituationSetWindowMonitor(int monitor_id);                                  // Set the window to be fullscreen on a specific monitor.
SITAPI const char* SituationGetMonitorName(int monitor_id);                             // Get the human-readable name of a monitor.
SITAPI int SituationGetMonitorWidth(int monitor_id);                                    // Get the width of a monitor's current video mode.
SITAPI int SituationGetMonitorHeight(int monitor_id);                                   // Get the height of a monitor's current video mode.
SITAPI int SituationGetMonitorPhysicalWidth(int monitor_id);                            // Get the physical width of a monitor in millimeters.
SITAPI int SituationGetMonitorPhysicalHeight(int monitor_id);                           // Get the physical height of a monitor in millimeters.
SITAPI int SituationGetMonitorRefreshRate(int monitor_id);                              // Get the refresh rate of a monitor.
SITAPI Vector2 SituationGetMonitorPosition(int monitor_id);                             // Get the top-left position of a monitor on the desktop.

// --- Cursor, Clipboard and File Drops ---
SITAPI void SituationSetCursor(SituationCursor cursor);                                 // Set the mouse cursor to a standard shape.
SITAPI void SituationShowCursor(void);                                                  // Show the mouse cursor.
SITAPI void SituationHideCursor(void);                                                  // Hide the mouse cursor.
SITAPI void SituationDisableCursor(void);                                               // Hide and lock the cursor, providing raw mouse motion.
SITAPI SituationError SituationGetClipboardText(const char** out_text);                 // Get text from the system clipboard.
SITAPI SituationError SituationSetClipboardText(const char* text);                      // Set text in the system clipboard.
SITAPI bool SituationIsFileDropped(void);                                               // Check if a file was dropped into the window this frame.
SITAPI char** SituationLoadDroppedFiles(int* count);                                    // Get the paths of dropped files (returns a copy, caller must free).
SITAPI void SituationUnloadDroppedFiles(char** paths, int count);                       // Unload the file path list returned by SituationLoadDroppedFiles.

// --- Advanced Window Profile Management ---
SITAPI SituationError SituationSetWindowStateProfiles(uint32_t active_flags, uint32_t inactive_flags); // Set the flag profiles for when the window is focused vs. unfocused.
SITAPI SituationError SituationApplyCurrentProfileWindowState(void);                    // Manually apply the appropriate window state profile based on current focus.
SITAPI SituationError SituationToggleWindowStateFlags(SituationWindowStateFlags flags_to_toggle); // Toggle flags in the current profile and apply the result.
SITAPI uint32_t SituationGetCurrentActualWindowStateFlags(void);                        // Gets flags based on current GLFW window state

//==================================================================================
// Image Module: CPU-side Image and Font Loading and Manipulation
//==================================================================================
// --- Image Loading and Unloading ---
SITAPI SituationError SituationLoadImage(const char *fileName, SituationImage* out_image);                         // Load an image from a file into CPU memory (RAM).
SITAPI SituationError SituationLoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, SituationImage* out_image); // Load an image from a memory buffer.
SITAPI void SituationUnloadImage(SituationImage image);                                 // Unload an image's pixel data from memory.
SITAPI bool SituationIsImageValid(SituationImage image);                                // Check if an image has been loaded successfully.

// --- Image Exporting ---
SITAPI SituationError SituationExportImage(SituationImage image, const char *fileName);           // Export image data to a file (PNG, BMP supported).

// --- Image Generation & Copying ---
SITAPI SituationError SituationImageCopy(SituationImage image, SituationImage* out_image);                         // Create a new image by copying another.

SITAPI SituationError SituationCreateImage(int width, int height, int channels, SituationImage* out_image);        // Allocates a new SituationImage container with UNINITIALIZED data.
SITAPI void SituationSetPixelColor(SituationImage *img, int x, int y, ColorRGBA col);   // Helper to set a specific pixel color (CPU-side).
SITAPI void SituationBlitRawDataToImage(SituationImage *dst, const void* data, int x, int y, int width, int height, int src_channels); // Copies raw byte data into a specific region of an image.

SITAPI void SituationImageDraw(SituationImage *dst, SituationImage src, SitRectangle srcRect, Vector2 dstPos); // Copying portion of one image into another image at destination placement
SITAPI void SituationImageDrawAlpha(SituationImage *dst, SituationImage src, SitRectangle srcRect, Vector2 dstPos, ColorRGBA tint); // Draw a portion of a source image onto dst with alpha tinting.
SITAPI SituationError SituationGenImageColor(int width, int height, ColorRGBA color, SituationImage* out_image);   // Generate a new image of a solid color.
SITAPI SituationError SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br, SituationImage* out_image); // Generate a new image with a gradient.

// --- Image Manipulation (Modifies image in-place) ---
SITAPI void SituationImageCrop(SituationImage *image, SitRectangle crop);                  // Crop an image to a specific rectangle.
SITAPI void SituationImageResize(SituationImage *image, int newWidth, int newHeight);   // Resize an image using default bicubic scaling.
SITAPI void SituationImageFlip(SituationImage *image, SituationImageFlipMode mode);     // Flip an image.
SITAPI void SituationImageAdjustHSV(SituationImage *image, float hue_shift, float sat_factor, float val_factor, float mix);   // Control an image by Hue Saturation and Brightness.
SITAPI void SituationImageAdjustYPQ(SituationImage *image, float phase_shift_deg, float chroma_factor, float luma_factor, float mix); // Grade an image in YPQ (phase/chroma/luma).

// --- Font Management ---
SITAPI SituationError SituationLoadFont(const char *fileName, SituationFont* out_font);                         // Load a font from a TTF/OTF file for CPU rendering.
SITAPI SituationError SituationLoadFontFromMemory(const void* data, int dataSize, SituationFont* out_font);		// Loads a font directly from a memory buffer (e.g., embedded resource).
SITAPI SituationError SituationLoadBitmapFontFromMemory(const unsigned char* data, int char_width, int char_height, int num_chars, SituationFont* out_font); // Loads a raw bitmap font (e.g. 8x8 array).
SITAPI SituationError SituationBakeFontAtlas(SituationFont* font, float fontSizePixels); // Rasterize a font into a GPU-ready atlas at the given size.
SITAPI void SituationUnloadFont(SituationFont font);                                    // Unload a CPU-side font and free its memory.
SITAPI SitRectangle SituationMeasureText(SituationFont font, const char *text, float fontSize); // Measure the pixel dimensions of a string before drawing.
SITAPI void SituationImageDrawCodepoint(SituationImage *dst, SituationFont font, int codepoint, Vector2 position, float fontSize, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness); // Draw a single Unicode character with advanced styling onto an image.
SITAPI void SituationImageDrawText(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, ColorRGBA tint ); // Draw a simple, tinted text string onto an image.
SITAPI void SituationImageDrawTextEx(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness); // Draw a text string with advanced styling (rotation, outline) onto an image.
SITAPI void SituationImageDrawTextFormatted(SituationImage *dst, SituationFont font, Vector2 position, float fontSize, float spacing, ColorRGBA tint, const char* fmt, ...); // Draw printf-style formatted text onto an image.

//==================================================================================
// Graphics Module: Rendering, Shaders, and GPU Resources
//==================================================================================

// --- Profiling & Diagnostics ---
SITAPI uint32_t SituationGetDrawCallCount(void); 										// Number of draw commands this frame
SITAPI uint64_t SituationGetVRAMUsage(void);     										// Total GPU memory allocated (Bytes)
SITAPI void SituationExportRenderHistogram(char* buf, size_t buf_size);                  // Write a text-based frame time histogram into buf.
#if defined(SITUATION_ENABLE_RENDER_THREAD)
SITAPI size_t SituationGetRenderQueueDepth(void);                                       // Get the current depth of the render queue
SITAPI void SituationGetRenderLatencyStats(uint64_t* avg_ns, uint64_t* max_ns);         // Get render thread latency metrics
#endif

// [v2.3.37] I/O Metrics
SITAPI size_t SituationGetIOQueueDepth(void);                                           // [v2.3.37] Get the current depth of the IO/Low Priority queue

// [v2.3.23] Debug Overlay
SITAPI void SituationDrawMetricsOverlay(SituationCommandBuffer cmd, Vector2 position, ColorRGBA color); // Draws FPS, Latency, and Memory stats

// --- Frame Lifecycle & Command Buffer ---
SITAPI bool SituationAcquireFrameCommandBuffer(void);                                   // Prepare the backend for a new frame of rendering commands.
#if defined(SITUATION_ENABLE_THREADING)
SITAPI SituationJobId SituationSubmitRenderList(SituationThreadPool* pool, SituationRenderList list, void (*func)(void*, void*), void* user_data); // Submit a render list for async recording on a worker thread.
#else
SITAPI void SituationSubmitRenderList(SituationRenderList list, void (*func)(void*, void*), void* user_data); // Submit a render list for immediate recording (single-threaded fallback).
#endif
SITAPI void SituationReplayRenderList(SituationCommandBuffer cmd, SituationRenderList list); // Replay a previously recorded render list into a command buffer.
SITAPI void SituationResetRenderList(SituationRenderList list);                         // Reset a render list for reuse next frame.
SITAPI SituationCommandBuffer SituationGetMainCommandBuffer(void);                      // Get the primary command buffer for the current frame.
SITAPI SituationCommandBuffer SituationGetComputeCommandBuffer(void);                   // [v2.3.23] Get the compute-specific command buffer (Vulkan only).
SITAPI SituationError SituationEndFrame(void);                                          // Submit all commands for the frame and present the result.

// --- Raster & Fixed-Function State Enums (Phase 4) ---
typedef enum SituationCullMode {
    SIT_CULL_NONE = 0,
    SIT_CULL_BACK,
    SIT_CULL_FRONT
} SituationCullMode;

typedef enum SituationFrontFace {
    SIT_FRONT_FACE_CCW = 0,
    SIT_FRONT_FACE_CW
} SituationFrontFace;

typedef enum SituationPrimitiveTopology {
    SIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 0,
    SIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    SIT_PRIMITIVE_TOPOLOGY_LINE_LIST,
    SIT_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    SIT_PRIMITIVE_TOPOLOGY_POINT_LIST
} SituationPrimitiveTopology;

typedef enum SituationPolygonMode {
    SIT_POLYGON_MODE_FILL = 0,
    SIT_POLYGON_MODE_LINE,
    SIT_POLYGON_MODE_POINT
} SituationPolygonMode;

typedef enum SituationIndexType {
    SIT_INDEX_UINT32 = 0,
    SIT_INDEX_UINT16
} SituationIndexType;

typedef enum SituationDepthCompareOp {
    SIT_DEPTH_COMPARE_ALWAYS = 0,
    SIT_DEPTH_COMPARE_LESS,
    SIT_DEPTH_COMPARE_LEQUAL,
    SIT_DEPTH_COMPARE_GREATER,
    SIT_DEPTH_COMPARE_GEQUAL,
    SIT_DEPTH_COMPARE_EQUAL,
    SIT_DEPTH_COMPARE_NOTEQUAL,
    SIT_DEPTH_COMPARE_NEVER
} SituationDepthCompareOp;

typedef enum SituationStencilOp {
    SIT_STENCIL_OP_KEEP = 0,
    SIT_STENCIL_OP_ZERO,
    SIT_STENCIL_OP_REPLACE,
    SIT_STENCIL_OP_INCREMENT_CLAMP,
    SIT_STENCIL_OP_DECREMENT_CLAMP,
    SIT_STENCIL_OP_INVERT,
    SIT_STENCIL_OP_INCREMENT_WRAP,
    SIT_STENCIL_OP_DECREMENT_WRAP
} SituationStencilOp;

typedef struct SituationStencilState {
    SituationDepthCompareOp compare_op;
    SituationStencilOp fail_op;
    SituationStencilOp depth_fail_op;
    SituationStencilOp pass_op;
    uint32_t compare_mask;
    uint32_t write_mask;
    uint32_t reference;
} SituationStencilState;

typedef struct SituationMultisampleState {
    bool sample_shading_enable;
    float min_sample_shading;
    uint32_t sample_mask;
    bool alpha_to_coverage_enable;
} SituationMultisampleState;

typedef enum SituationBlendFactor {
    SIT_BLEND_ZERO = 0,
    SIT_BLEND_ONE,
    SIT_BLEND_SRC_COLOR,
    SIT_BLEND_ONE_MINUS_SRC_COLOR,
    SIT_BLEND_DST_COLOR,
    SIT_BLEND_ONE_MINUS_DST_COLOR,
    SIT_BLEND_SRC_ALPHA,
    SIT_BLEND_ONE_MINUS_SRC_ALPHA,
    SIT_BLEND_DST_ALPHA,
    SIT_BLEND_ONE_MINUS_DST_ALPHA
} SituationBlendFactor;

// --- Command Buffer Recording ---
SITAPI SituationError SituationCmdSetCullMode(SituationCommandBuffer cmd, SituationCullMode mode);
SITAPI SituationError SituationCmdSetFrontFace(SituationCommandBuffer cmd, SituationFrontFace front_face);
SITAPI SituationError SituationCmdSetPrimitiveTopology(SituationCommandBuffer cmd, SituationPrimitiveTopology topology);
SITAPI SituationError SituationCmdSetPolygonMode(SituationCommandBuffer cmd, SituationPolygonMode mode);
SITAPI SituationError SituationCmdSetDepthBias(SituationCommandBuffer cmd, bool enable, float constant_factor, float clamp, float slope_factor);
SITAPI SituationError SituationCmdSetLineWidth(SituationCommandBuffer cmd, float width);
SITAPI SituationError SituationCmdSetColorWriteMask(SituationCommandBuffer cmd, bool r, bool g, bool b, bool a);
SITAPI SituationError SituationCmdSetStencilTest(SituationCommandBuffer cmd, bool enable, const SituationStencilState* front, const SituationStencilState* back);
SITAPI SituationError SituationCmdSetMultisampleState(SituationCommandBuffer cmd, const SituationMultisampleState* state);
SITAPI SituationError SituationCmdSetDepthTest(SituationCommandBuffer cmd, bool enable, SituationDepthCompareOp depth_op);
SITAPI SituationError SituationCmdSetDepthWrite(SituationCommandBuffer cmd, bool enable);
SITAPI SituationError SituationCmdSetBlendEnable(SituationCommandBuffer cmd, bool enable);
SITAPI SituationError SituationCmdSetBlendFuncSeparate(SituationCommandBuffer cmd, SituationBlendFactor src_rgb, SituationBlendFactor dst_rgb, SituationBlendFactor src_a, SituationBlendFactor dst_a);
SITAPI SituationError SituationCmdPushRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
SITAPI SituationError SituationCmdPopRasterState(SituationCommandBuffer cmd, uint32_t scope_id);
SITAPI SituationError SituationCmdBeginDebugGroup(SituationCommandBuffer cmd, const char* name, ColorRGBA color);
SITAPI SituationError SituationCmdEndDebugGroup(SituationCommandBuffer cmd);
SITAPI SituationError SituationCmdSetPushConstantData(SituationCommandBuffer cmd, SituationShader shader, uint32_t offset, const void* data, size_t size);

// --- Abstracted Rendering Commands ---
SITAPI SituationError SituationCmdSetViewport(SituationCommandBuffer cmd, float x, float y, float width, float height);                           // Sets the dynamic viewport and scissor for the current render pass.
SITAPI SituationError SituationCmdSetScissor(SituationCommandBuffer cmd, int x, int y, int width, int height);                                    // Sets the dynamic scissor rectangle to clip rendering.
SITAPI SituationError SituationCmdSetViewportIndexed(SituationCommandBuffer cmd, uint32_t index, float x, float y, float width, float height);   // Sets viewport at index (0 = default viewport).
SITAPI SituationError SituationCmdSetScissorIndexed(SituationCommandBuffer cmd, uint32_t index, int x, int y, int width, int height);             // Sets scissor at index (0 = default scissor).
SITAPI SituationError SituationCmdBindPipeline(SituationCommandBuffer cmd, SituationShader shader);                                     // Binds a graphics pipeline (shader program) for subsequent draws.
SITAPI SituationError SituationCmdDrawMesh(SituationCommandBuffer cmd, SituationMesh mesh);                                             // [High-Level] Records a command to draw a complete, pre-configured mesh.
SITAPI SituationError SituationCmdDrawQuad(SituationCommandBuffer cmd, mat4 model, Vector4 color);                                                // [High-Level] Record a command to draw a simple, colored 2D quad.
SITAPI SituationError SituationCmdDrawTexture(SituationCommandBuffer cmd, SituationTexture texture, SitRectangle source, SitRectangle dest, Vector2 origin, float rotation, ColorRGBA tint); // [High-Level] Draw a part of a texture defined by a rectangle.
SITAPI SituationError SituationCmdDrawTextureYpqGrade(SituationCommandBuffer cmd, SituationTexture texture, SitRectangle source, SitRectangle dest, Vector2 origin, float rotation, float phase_shift_deg, float chroma_factor, float luma_factor, float mix); // [High-Level] Draw texture with YPQ grade (matches SituationImageAdjustYPQ).
SITAPI SituationError SituationCmdSetPushConstant(SituationCommandBuffer cmd, uint32_t contract_id, const void* data, size_t size);               // [Core] Set a small block of per-draw uniform data (push constant).
SITAPI SituationError SituationCmdBindDescriptorSet(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer);            // [Core] Binds a buffer's descriptor set (UBO/SSBO) to a set index.
SITAPI SituationError SituationCmdBindDescriptorSetDynamic(SituationCommandBuffer cmd, uint32_t set_index, SituationBuffer buffer, uint32_t dynamic_offset); // [Core] Binds a dynamic buffer descriptor set with an offset.
SITAPI SituationError SituationCmdBindTextureSet(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);             // [Core] Binds a texture's descriptor set (sampler/storage) to a set index.
SITAPI SituationError SituationCmdBindComputeTexture(SituationCommandBuffer cmd, uint32_t binding, SituationTexture texture);           // [Core] Binds a texture as a storage image for compute shaders.
SITAPI SituationError SituationCmdSetVertexAttribute(SituationCommandBuffer cmd, uint32_t location, uint32_t binding, int size, SituationDataType type, bool normalized, size_t offset); // [OpenGL Only] Attribute format + vertex buffer binding index (must match SituationCmdBindVertexBuffer).
SITAPI SituationError SituationCmdBindVertexBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer, size_t offset, size_t stride); // [Core] Bind a vertex buffer for subsequent SituationCmdDraw / SituationCmdDrawIndexed.
SITAPI SituationError SituationCmdBindIndexBufferEx(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset, SituationIndexType index_type); // [Core] Bind index buffer with 16- or 32-bit element type for subsequent SituationCmdDrawIndexed.
SITAPI SituationError SituationCmdBindIndexBuffer(SituationCommandBuffer cmd, SituationBuffer buffer, size_t offset); // [Core] Bind a 32-bit index buffer (SIT_INDEX_UINT32). Pass offset 0 when indices start at the beginning of the buffer.
SITAPI SituationError SituationCmdDraw(SituationCommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance); // [Core] Record a non-indexed draw call.
SITAPI SituationError SituationCmdDrawIndexed(SituationCommandBuffer cmd, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance); // [Core] Record an indexed draw call.
SITAPI SituationError SituationCmdDrawIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset); // [Core] Draw from a CPU/GPU-filled SituationDrawIndirectCommand in an indirect buffer (requires active render pass, bound pipeline, and vertex buffers).
SITAPI SituationError SituationCmdDrawIndexedIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset); // [Core] Indexed indirect draw (32-bit indices; requires bound index buffer). firstIndex is relative to SituationCmdBindIndexBuffer offset.
SITAPI SituationError SituationCmdBeginRenderPass(SituationCommandBuffer cmd, const SituationRenderPassInfo* info);                     // Begins a render pass with detailed configuration.
SITAPI SituationError SituationCmdClear(SituationCommandBuffer cmd, uint32_t clear_flags, const SituationClearValue* clear_value);       // Mid-pass clear of active render-pass attachments; begin-pass clears use SituationRenderPassInfo loadOp.
SITAPI SituationError SituationCmdClearColor(SituationCommandBuffer cmd, ColorRGBA color);                                               // Mid-pass clear of the active color attachment.
SITAPI SituationError SituationCmdClearDepth(SituationCommandBuffer cmd, float depth);                                                    // Mid-pass clear of the active depth attachment.
SITAPI SituationError SituationCmdClearStencil(SituationCommandBuffer cmd, uint32_t stencil);                                             // Mid-pass clear of the active stencil attachment when supported by backend/attachment state.
SITAPI SituationError SituationCmdClearDepthStencil(SituationCommandBuffer cmd, float depth, uint32_t stencil);                          // Mid-pass clear of active depth and stencil attachments.
SITAPI SituationError SituationCmdEndRenderPass(SituationCommandBuffer cmd);                                                                      // Ends the current render pass.
SITAPI SituationError SituationCmdDrawText(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, ColorRGBA color);		// Draws a text string using GPU-accelerated textured quads.
SITAPI SituationError SituationCmdDrawTextEx(SituationCommandBuffer cmd, SituationFont font, const char* text, Vector2 pos, float fontSize, float spacing, ColorRGBA color); // Advanced text drawing (scaling/spacing).
SITAPI SituationError SituationCmdPresent(SituationCommandBuffer cmd, SituationTexture texture);                                                  // Submits a command to copy a texture to the main window's swapchain (Compute-Only).
SITAPI SituationError SituationCmdBindSampledTexture(SituationCommandBuffer cmd, int binding, SituationTexture texture);                // Binds a texture as a sampled image (sampler2D) to a binding point.

// --- Graphics Resource Management ---
SITAPI SituationError SituationCreateMesh(const void* vertex_data, int vertex_count, size_t vertex_stride, const uint32_t* index_data, int index_count, SituationMesh* out_mesh); // Create a mesh from vertex and index data.
SITAPI void SituationDestroyMesh(SituationMesh* mesh);                                  // Unload a mesh from GPU memory.
SITAPI uint64_t SituationGetBufferDeviceAddress(SituationBuffer buffer);                // Retrieves the GPU device address of a buffer for bindless access.
SITAPI uint64_t SituationGetTextureHandle(SituationTexture texture);                    // Retrieves the bindless texture handle (OpenGL Only).

// --- Shader Management ---
SITAPI SituationError SituationLoadShader(const char* vs_path, const char* fs_path, SituationShader* out_shader);   // Load a graphics shader pipeline from vertex and fragment files.
SITAPI SituationError SituationLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader); // Create a graphics shader pipeline from in-memory GLSL source.
SITAPI SituationError SituationBeginLoadShaderFromMemory(const char* vs_code, const char* fs_code, SituationShader* out_shader); // Start non-blocking GLSL load: [OpenGL] async compile/link; [Vulkan] shaderc on worker thread, pipelines on next frames. Poll with SituationPollShaderLoad.
SITAPI SituationError SituationBeginLoadShaderFromSpirvMemory(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader); // [Vulkan] Non-blocking pipeline build from in-memory SPIR-V (bytecode copied). [OpenGL] blocking SPIR-V load (unchanged).
SITAPI SituationError SituationBeginLoadShaderFromSpirvMemoryEx(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader); // [Vulkan] Async SPIR-V with layout profile (e.g. UBO_SSBO for Demon Hunt). [OpenGL] profile ignored; same as SituationBeginLoadShaderFromSpirvMemory.
SITAPI SituationError SituationPollShaderLoad(SituationShader shader); // SITUATION_SUCCESS when ready, SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS while compiling/linking/building pipelines.
SITAPI SituationError SituationLoadShaderFromSpirv(const char* vs_spv_path, const char* fs_spv_path, SituationShader* out_shader); // Precompiled .spv: OpenGL via GL_ARB_gl_spirv; Vulkan same pipeline contract as SituationLoadShaderFromMemory (no shaderc required).
SITAPI SituationError SituationLoadShaderFromSpirvMemory(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationShader* out_shader); // Same as SituationLoadShaderFromSpirv but from in-memory SPIR-V (e.g. build-time embedded .spv). No hot-reload paths (vs/fs file paths left unset).
SITAPI SituationError SituationLoadShaderFromSpirvMemoryEx(const void* vs_spirv, size_t vs_len, const void* fs_spirv, size_t fs_len, SituationSpirvLayoutProfile layout_profile, SituationShader* out_shader); // [Vulkan] `layout_profile` selects user SSBO/UBO layouts; [OpenGL] profile ignored.
SITAPI void SituationUnloadShader(SituationShader* shader);                             // Unload a graphics shader pipeline and free its GPU resources.

// --- Shader Interaction & Synchronization ---
SITAPI SituationError SituationSetShaderUniform(SituationShader shader, const char* uniform_name, const void* data, SituationUniformType type); // [OpenGL] Set a standalone uniform by name (location cache). While a frame is active, defers to SIT_OP_SET_UNIFORM.
SITAPI SituationError SituationSetShaderUniformLocation(SituationShader shader, int location, const void* data, SituationUniformType type); // [OpenGL] Set uniform by explicit location (SPIR-V layout(location=)); defers during frames like SituationSetShaderUniform.
SITAPI SituationError SituationBindShaderStorageBlock(SituationShader shader, const char* block_name, uint32_t binding_point); // [OpenGL] glShaderStorageBlockBinding for SPIR-V when reflection reports binding 0 for layout(binding=N).
SITAPI SituationError SituationBindUniformBlock(SituationShader shader, const char* block_name, uint32_t binding_point); // [OpenGL] glUniformBlockBinding for std140 UBO blocks (layout(binding=N)).
SITAPI SituationError SituationSetShaderUniform1fv(SituationShader shader, const char* uniform_name, int count, const float* values); // [OpenGL] Set float uniform array.
SITAPI SituationError SituationSetShaderUniform1iv(SituationShader shader, const char* uniform_name, int count, const int* values); // [OpenGL] Set int uniform array in one call (e.g. name "uWallRows[0]", count=24). While a frame is active, records SIT_OP_SET_UNIFORM (same as render-thread mode).
SITAPI SituationError SituationSetShaderUniformMatrix4fv(SituationShader shader, const char* uniform_name, int count, const mat4* matrices); // [OpenGL] Set mat4 uniform array.

typedef struct SituationUniformExpectation {
    const char* name;
    SituationUniformType type;
    int array_length; /* 0 = scalar */
} SituationUniformExpectation;
SITAPI SituationError SituationValidateShaderUniforms(SituationShader shader, const SituationUniformExpectation* table, int table_count, char* error_buf, size_t error_buf_size); // Returns first missing/wrong-type uniform, or SUCCESS if all resolved.

SITAPI void SituationCmdPipelineBarrier(SituationCommandBuffer cmd, uint32_t src_flags, uint32_t dst_flags); // Legacy convenience barrier; prefer SituationCmdPipelineBarrierEx, SituationCmdBufferBarrier, or SituationCmdTextureBarrier for new synchronization code.

// --- Texture Management ---
SITAPI SituationError SituationLoadTexture(const char* file_path, bool generate_mipmaps, SituationTexture* out_texture);// Loads a texture from disk and registers the path for hot-reloading.
SITAPI SituationError SituationCreateTexture(SituationImage image, bool generate_mipmaps, SituationTexture* out_texture); // Create a texture from a CPU-side image.
SITAPI SituationError SituationCreateTextureEx(SituationImage image, bool generate_mipmaps, SituationTextureUsageFlags flags, SituationTexture* out_texture); // Create a texture with specific usage flags.
SITAPI void SituationDestroyTexture(SituationTexture* texture);                         // Unload a texture from GPU memory.
SITAPI SituationError SituationGetTextureInfo(SituationTexture texture, SituationTextureInfo* out_info); // [Phase 2] Query texture metadata.
SITAPI SituationError SituationSetTextureSamplerParams(SituationTexture texture, SituationTextureFilter min_filter, SituationTextureFilter mag_filter, SituationTextureWrap wrap_s, SituationTextureWrap wrap_t); // [Phase 2] Update sampler state.
SITAPI SituationError SituationCmdBlitTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureBlitRegion* region); // Blit between color 2D textures; caller owns explicit texture barriers.
SITAPI SituationError SituationCmdCopyTexture(SituationCommandBuffer cmd, SituationTexture src, SituationTexture dst, const SituationTextureCopyRegion* region); // Exact-size copy between color 2D textures; caller owns explicit texture barriers.
SITAPI SituationError SituationCmdCopyBufferToTexture(SituationCommandBuffer cmd, SituationBuffer src, size_t src_offset, SituationTexture dst, const SituationTextureCopyRegion* dst_region); // Upload tightly packed RGBA8 rows from a buffer into a texture subregion; caller owns texture barriers.
SITAPI SituationError SituationCmdCopyTextureToBuffer(SituationCommandBuffer cmd, SituationTexture src, const SituationTextureCopyRegion* src_region, SituationBuffer dst, size_t dst_offset, size_t dst_row_pitch); // Copy a texture subregion into a buffer (`dst_row_pitch` 0 = width * 4); caller owns texture barriers.
SITAPI SituationError SituationReadTexture(SituationTexture texture, const SituationTextureReadbackDesc* desc, void* dst_pixels, size_t dst_size_bytes); // [Phase 2] Blocking readback of texture pixels.
SITAPI SituationError SituationReadTextureAlloc(SituationTexture texture, const SituationTextureReadbackDesc* desc, SituationImage* out_image); // [Phase 2] Blocking readback into allocated SituationImage.
SITAPI SituationError SituationReadFramebuffer(const SituationReadPixelsDesc* desc, void* dst_pixels, size_t dst_size_bytes); // [Phase 2] Blocking readback of framebuffer pixels.

// --- Compute Shader Pipeline ---
SITAPI SituationError SituationCreateComputePipeline(const char* compute_shader_path, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline); // Create a compute pipeline from a shader file.
SITAPI SituationError SituationCreateComputePipelineFromMemory(const char* compute_shader_source, SituationComputeLayoutType layout_type, SituationComputePipeline* out_pipeline); // Create a compute pipeline from in-memory GLSL source.
SITAPI void SituationDestroyComputePipeline(SituationComputePipeline* pipeline);        // Destroy a compute pipeline and free its GPU resources.
SITAPI void SituationCmdBindComputePipeline(SituationCommandBuffer cmd, SituationComputePipeline pipeline); // Bind a compute pipeline for a subsequent dispatch.
SITAPI SituationError SituationCmdDispatchEx(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z); // Record a compute dispatch with validation and error reporting.
SITAPI void SituationCmdDispatch(SituationCommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z); // Record a command to dispatch compute shader work groups.
SITAPI SituationError SituationCmdDispatchIndirect(SituationCommandBuffer cmd, SituationBuffer indirect_buffer, size_t offset); // Record an indirect compute dispatch.
SITAPI void SituationGetMaxComputeWorkGroups(uint32_t* x, uint32_t* y, uint32_t* z); // Query maximum compute work group count per dispatch.
SITAPI SituationError SituationCmdPipelineBarrierEx(SituationCommandBuffer cmd, const SituationPipelineBarrierDesc* desc); // Record an explicit global memory barrier.
SITAPI SituationError SituationCmdBufferBarrier(SituationCommandBuffer cmd, const SituationBufferBarrierDesc* desc); // Record an explicit buffer-range memory barrier.
SITAPI SituationError SituationCmdTextureBarrier(SituationCommandBuffer cmd, SituationTexture texture, const SituationTextureBarrierDesc* desc); // Record an explicit texture layout/memory barrier.

// --- GPU Buffer Management ---
SITAPI SituationError SituationCreateBuffer(size_t size, const void* initial_data, SituationBufferUsageFlags usage_flags, SituationBuffer* out_buffer); // Create a generic GPU data buffer (e.g., SSBO).
SITAPI SituationError SituationCreateReadbackBuffer(size_t size, SituationBuffer* out_buffer); // [Phase 1] Create an async GPU->CPU staging buffer.
SITAPI void SituationDestroyBuffer(SituationBuffer* buffer);                            // Destroy a GPU buffer.
SITAPI SituationError SituationUpdateBuffer(SituationBuffer buffer, size_t offset, size_t size, const void* data); // Update data in a GPU buffer.
SITAPI SituationError SituationGetBufferData(SituationBuffer buffer, size_t offset, size_t size, void* out_data); // Read data from a GPU buffer (blocking).
SITAPI SituationError SituationCmdCopyBufferEx(SituationCommandBuffer cmd, SituationBuffer src, SituationBuffer dst, size_t src_offset, size_t dst_offset, size_t size); // Error-returning buffer-copy command with independent source/destination offsets.
SITAPI void SituationCmdCopyBuffer(SituationCommandBuffer cmd, SituationBuffer src, SituationBuffer dst, size_t offset, size_t size); // Legacy void buffer-copy command; Phase 4 tracks the error-returning copy/blit API.
SITAPI void SituationReadBuffer(SituationBuffer readback_buf, void* dst, size_t size); // [Phase 1] Read mapped buffer data safely.

// --- Virtual Displays (Render Targets) ---
SITAPI SituationError SituationCreateVirtualDisplay(Vector2 resolution, double frame_time_mult, int z_order, SituationScalingMode scaling_mode, SituationBlendMode blend_mode, int* out_id); // Create an off-screen render target.
SITAPI SituationError SituationDestroyVirtualDisplay(int display_id);                   // Destroy a virtual display.
SITAPI SituationError SituationRenderVirtualDisplays(SituationCommandBuffer cmd);       // Composite all visible virtual displays to the current target.
SITAPI SituationError SituationConfigureVirtualDisplay(int display_id, Vector2 offset, float opacity, int z_order, bool visible, double frame_time_mult, SituationBlendMode blend_mode); // Configure a virtual display's properties.
SITAPI SituationVirtualDisplay* SituationGetVirtualDisplay(int display_id);             // Get a pointer to a virtual display's state.
SITAPI SituationError SituationSetVirtualDisplayScalingMode(int display_id, SituationScalingMode scaling_mode); // Set the scaling/filtering mode for a virtual display.
SITAPI void SituationSetVirtualDisplayDirty(int display_id, bool is_dirty);             // Mark a virtual display as needing to be re-rendered.
SITAPI bool SituationIsVirtualDisplayDirty(int display_id);                             // Check if a virtual display is marked as dirty.
SITAPI double SituationGetLastVDCompositeTimeMS(void);                                  // Get the time taken for the last virtual display composite pass.
SITAPI void SituationGetVirtualDisplaySize(int display_id, int* width, int* height);    // Get the internal resolution of a virtual display.

// --- Camera & Projection Math ---
typedef enum SituationCameraFlags {
    SIT_CAMERA_FLAG_NONE                 = 0,
    SIT_CAMERA_FLAG_ORTHOGRAPHIC         = 1 << 0,  // Use orthographic instead of perspective
    SIT_CAMERA_FLAG_REVERSE_Z            = 1 << 1,  // Use infinite reverse-Z projection (1.0 near, 0.0 far)
    SIT_CAMERA_FLAG_INFINITE_PROJECTION  = 1 << 2   // Use infinite projection (normal Z)
} SituationCameraFlags;

typedef struct SituationCameraDesc {
    Vector3 eye;
    Vector3 target;
    Vector3 up;               // Default to {0,1,0} if {0,0,0}
    float   vertical_fov_deg; // Used if perspective
    float   ortho_height;     // Used if SIT_CAMERA_FLAG_ORTHOGRAPHIC is set
    float   aspect;           // 0.0f auto-uses current window/render target aspect
    float   z_near;
    float   z_far;
    uint32_t flags;           // Bitmask of SituationCameraFlags
} SituationCameraDesc;

SITAPI void SituationCameraBuildView(const SituationCameraDesc* desc, mat4 out_view);
SITAPI void SituationCameraBuildProj(const SituationCameraDesc* desc, mat4 out_proj);
SITAPI void SituationCameraBuildViewProj(const SituationCameraDesc* desc, mat4 out_vp);
SITAPI void SituationCameraBuildInvViewProj(const SituationCameraDesc* desc, mat4 out_inv_vp);
SITAPI void SituationCameraUnprojectPixel(const SituationCameraDesc* desc, const mat4 inv_vp, Vector2 pixel, Vector2 framebuffer_px, Vector3* out_ray_origin, Vector3* out_ray_dir);

// --- 3D Model Utilities ---
SITAPI SituationError SituationLoadModel(const char* file_path, SituationModel* out_model); // Loads a complete 3D model and its textures from a GLTF file.
SITAPI void SituationUnloadModel(SituationModel* model);                                // Frees all GPU and CPU resources associated with a loaded model.
SITAPI void SituationDrawModel(SituationCommandBuffer cmd, SituationModel model, mat4 transform); // Draws all sub-meshes of a model with a single root transformation.
SITAPI bool SituationSaveModelAsGltf(SituationModel model, const char* file_path);      // Exports a model to a human-readable .gltf and a .bin file for debugging.
SITAPI void SituationGetMeshData(SituationMesh mesh, void** vertex_data, int* vertex_count, int* vertex_stride, void** index_data, int* index_count); // Get raw vertex/index data pointers from a mesh (read-only).

// --- Image & Screenshot Utilities ---
SITAPI SituationError SituationLoadImageFromScreen(SituationImage* out_image);          // Get a copy of the current screen backbuffer as an image.
SITAPI SituationError SituationTakeScreenshot(const char *fileName);                    // Take a screenshot and save it to a file (PNG or BMP).

// --- Backend-Specific Accessors ---
SITAPI SituationRendererType SituationGetRendererType(void);                            // Get the current active renderer type (OpenGL or Vulkan).
SITAPI GLFWwindow* SituationGetGLFWwindow(void);                                        // Get the raw GLFW window handle.
#ifdef SITUATION_USE_VULKAN
SITAPI VkInstance SituationGetVulkanInstance(void);                                     // Get the raw Vulkan instance handle.
SITAPI VkDevice SituationGetVulkanDevice(void);                                         // Get the raw Vulkan logical device handle.
SITAPI VkPhysicalDevice SituationGetVulkanPhysicalDevice(void);                         // Get the raw Vulkan physical device handle.
SITAPI VkRenderPass SituationGetMainWindowRenderPass(void);                             // Get the render pass for the main window.
#endif

// --- [DEPRECATED] Use SituationCmdBindDescriptorSet() or SituationCmdBindTextureSet() instead. ---
SITAPI __attribute__((deprecated)) SituationError SituationCmdBeginRenderToDisplay(SituationCommandBuffer cmd, int display_id, ColorRGBA clear_color);              // [DEPRECATED] Begins a render pass on a target (-1 for main window), clearing it.
SITAPI __attribute__((deprecated)) SituationError SituationCmdEndRender(SituationCommandBuffer cmd);                                                                // [DEPRECATED] End the current render pass.
SITAPI SituationError SituationCmdBindUniformBuffer(SituationCommandBuffer cmd, uint32_t contract_id, SituationBuffer buffer);          // [DEPRECATED] [Core] Bind a Uniform Buffer Object (UBO) to a shader binding point.
SITAPI SituationError SituationCmdBindTexture(SituationCommandBuffer cmd, uint32_t set_index, SituationTexture texture);                // [DEPRECATED] [Core] Bind a texture and sampler to a shader binding point.
SITAPI SituationError SituationCmdBindComputeBuffer(SituationCommandBuffer cmd, uint32_t binding, SituationBuffer buffer);              // [DEPRECATED] Bind a buffer to a compute shader binding point.
SITAPI SituationError SituationLoadComputeShader(const char* cs_path, SituationShader* out_shader);                                     // [DEPRECATED] Load a compute shader from a file. Use SituationCreateComputePipeline instead.
SITAPI SituationError SituationLoadComputeShaderFromMemory(const char* cs_code, SituationShader* out_shader);                           // [DEPRECATED] Create a compute shader from memory. Use SituationCreateComputePipelineFromMemory instead.
SITAPI void SituationMemoryBarrier(SituationCommandBuffer cmd, uint32_t barrier_bits);                                                  // [DEPRECATED] Insert a coarse-grained memory barrier. Use SituationCmdPipelineBarrierEx, SituationCmdBufferBarrier, or SituationCmdTextureBarrier instead.

//==================================================================================
// Hot-Reloading Module (Development Tools)
//==================================================================================
// These functions allow you to reload assets from disk at runtime without restarting.
// They handle GPU synchronization, resource destruction, and re-loading.
// Returns true if the reload was successful. On failure, the old handle is usually invalid.
SITAPI SituationError SituationCheckHotReloads(void);                                   // Checks all tracked resources for file changes and reloads them if necessary.
SITAPI bool SituationReloadShader(SituationShader* shader);                             // Recompiles and links a shader from its original source files (Synchronous/Stalls GPU).
SITAPI bool SituationReloadComputePipeline(SituationComputePipeline* pipeline);         // Recompiles a compute pipeline from its original source file (Synchronous/Stalls GPU).
SITAPI bool SituationReloadTexture(SituationTexture* texture);                          // Re-reads image file and recreates the GPU texture resource (Synchronous/Stalls GPU).
SITAPI bool SituationReloadModel(SituationModel* model);                                // Re-parses GLTF/GLB file and rebuilds all meshes and textures (Synchronous/Stalls GPU).

//==================================================================================
// Input Module: Keyboard, Mouse, and Gamepad
//==================================================================================
// --- Keyboard Input ---
SITAPI int SituationGetCharFromScancode(int window, int scancode, int mods, uint32_t* out_char); // Maps a physical key scancode (plus modifiers) to a Unicode character, respecting the current OS keyboard layout.
SITAPI bool SituationIsKeyDown(int key);                                                // Check if a key is currently held down (a state).
SITAPI bool SituationIsKeyUp(int key);                                                  // Check if a key is currently up (a state).
SITAPI bool SituationIsKeyPressed(int key);                                             // Check if a key was pressed down this frame (an event).
SITAPI bool SituationIsKeyReleased(int key);                                            // Check if a key was released this frame (an event).
SITAPI bool SituationIsScancodeDown(int scancode);                                      // Check if a physical key (scancode) is currently held down.
SITAPI int SituationGetKeyScancode(int key);                                            // Get the platform-specific scancode for a logical key.
SITAPI int SituationGetKeyPressed(void);                                                // Get the next key from the press queue (no repeats).
SITAPI int SituationGetKeyPressedEx(int* out_scancode);                                 // Get the next key and its scancode from the queue.
SITAPI int SituationPeekKeyPressed(void);                                               // Peek at the next key in the press queue without consuming it.
SITAPI int SituationPeekKeyPressedEx(int* out_scancode);                                // Peek at the next key and its scancode.
SITAPI unsigned int SituationGetCharPressed(void);                                      // Get the next character from the text input queue.
SITAPI bool SituationIsLockKeyPressed(int lock_key_mod);                                // Check if a lock key (Caps, Num) is currently active.
SITAPI bool SituationIsScrollLockOn(void);                                              // Check if Scroll Lock is currently toggled on.
SITAPI bool SituationIsModifierPressed(int modifier);                                   // Check if a modifier key (Shift, Ctrl, Alt) is pressed.
SITAPI void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data);    // Set a callback for key events.

// --- Mouse Input ---
SITAPI Vector2 SituationGetMousePosition(void);                                         // Get the mouse position within the window.
SITAPI Vector2 SituationGetMouseDelta(void);                                            // Get the mouse movement since the last frame.
SITAPI float SituationGetMouseWheelMove(void);                                          // Get vertical mouse wheel movement.
SITAPI Vector2 SituationGetMouseWheelMoveV(void);                                       // Get vertical and horizontal mouse wheel movement.
SITAPI bool SituationIsMouseButtonDown(int button);                                     // Check if a mouse button is currently held down (a state).
SITAPI bool SituationIsMouseButtonPressed(int button);                                  // Check if a mouse button was pressed down this frame (an event).
SITAPI bool SituationIsMouseButtonReleased(int button);                                 // Check if a mouse button was released this frame.
SITAPI void SituationSetMousePosition(Vector2 pos);                                     // Set the mouse position within the window.
SITAPI void SituationSetMouseOffset(Vector2 offset);                                    // Set a software offset for the mouse position.
SITAPI void SituationSetMouseScale(Vector2 scale);                                      // Set a software scale for the mouse position and delta.
SITAPI void SituationSetMouseButtonCallback(SituationMouseButtonCallback callback, void* user_data); // Set a callback for mouse button events.
SITAPI void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data); // Set a callback for mouse movement events.
SITAPI void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data); // Set a callback for mouse scroll events.

// --- Gamepad Input ---
SITAPI bool SituationIsJoystickPresent(int jid);                                        // Check if a joystick/gamepad is connected.
SITAPI bool SituationIsGamepad(int jid);                                                // Check if a connected joystick has a standard gamepad mapping.
SITAPI const char* SituationGetJoystickName(int jid);                                   // Get the human-readable name of a joystick/gamepad.
SITAPI void SituationSetJoystickCallback(SituationJoystickCallback callback, void* user_data); // Set a callback for joystick connection events.
SITAPI int SituationSetGamepadMappings(const char *mappings);                           // Load a new set of gamepad mappings from a string.
SITAPI int SituationGetGamepadButtonPressed(void);                                      // Get the next gamepad button from the press queue.
SITAPI bool SituationIsGamepadButtonDown(int jid, int button);                          // Check if a gamepad button is currently held down (a state).
SITAPI bool SituationIsGamepadButtonPressed(int jid, int button);                       // Check if a gamepad button was pressed down this frame (an event).
SITAPI bool SituationIsGamepadButtonReleased(int jid, int button);                      // Check if a gamepad button was released this frame (an event).
SITAPI int SituationGetGamepadAxisCount(int jid);                                       // Get the number of axes for a gamepad.
SITAPI float SituationGetGamepadAxisValue(int jid, int axis);                           // Get the value of a gamepad axis (deadzone applied).
SITAPI bool SituationSetGamepadVibration(int jid, float left_motor, float right_motor); // Set gamepad vibration/rumble (Windows only).

//==================================================================================
// Audio Module
//==================================================================================

// --- Audio Device Management ---
SITAPI SituationAudioDeviceInfo* SituationGetAudioDevices(int* count);                  // Get a list of available audio playback devices (caller must free).
SITAPI SituationError SituationSetAudioDevice(int internal_id, const SituationAudioFormat* format); // Set the active audio device.
SITAPI int SituationGetAudioPlaybackSampleRate(void);                                   // Get the sample rate of the current audio device.
SITAPI SituationError SituationSetAudioPlaybackSampleRate(int sample_rate);             // Re-initialize the audio device with a new sample rate.
SITAPI float SituationGetAudioMasterVolume(void);                                       // Get the master volume for the audio device.
SITAPI SituationError SituationSetAudioMasterVolume(float volume);                      // Set the master volume for the audio device.
SITAPI bool SituationIsAudioDevicePlaying(void);                                        // Check if the audio device is currently playing.
SITAPI SituationError SituationPauseAudioDevice(void);                                  // Pause audio playback on the device.
SITAPI SituationError SituationResumeAudioDevice(void);                                 // Resume audio playback on the device.

// --- Audio Capture ---
SITAPI SituationError SituationStartAudioCapture(SituationAudioCaptureCallback callback, void* user_data);                                          // Start capturing audio input with default format.
SITAPI SituationError SituationStartAudioCaptureEx(SituationAudioCaptureCallback callback, void* user_data, uint32_t sample_rate, uint32_t channels); // Start capturing with explicit sample rate and channel count.
SITAPI void SituationStopAudioCapture(void);                                            // Stop audio capture and release the input device.

// --- Audio Output Monitoring (for visualization) ---
SITAPI void SituationSetAudioOutputMonitor(void (*callback)(const float* samples, uint32_t frame_count, void* user_data), void* user_data); // Set a callback to receive mixed output samples (for VU meters, FFT, etc.).
SITAPI void SituationGetMasterOutputMeter(float* out_peak, float* out_rms); // Last playback callback block: peak sample magnitude & RMS (optional pointers; safe from main/UI thread).

// --- Sound Loading and Management ---
// --- Audio Handle API ---
SITAPI SituationSoundHandle SituationLoadAudio(const char* file_path, SituationAudioLoadMode mode, bool looping); // Load audio and return a lightweight handle for playback control.
SITAPI SituationError SituationPlayAudio(SituationSoundHandle handle);                  // Play audio by handle (restarts if already playing).
SITAPI void SituationUnloadAudio(SituationSoundHandle handle);                          // Unload audio by handle and free resources.
SITAPI SituationError SituationSetAudioVolume(SituationSoundHandle handle, float volume); // Set volume for a handle-based sound [0.0 to 1.0+].
SITAPI SituationError SituationSetAudioPan(SituationSoundHandle handle, float pan);     // Set stereo pan for a handle-based sound [-1.0 to 1.0].
SITAPI SituationError SituationSetAudioPitch(SituationSoundHandle handle, float pitch); // Set pitch multiplier for a handle-based sound (1.0 = normal).

SITAPI SituationError SituationLoadSoundFromFile(const char* file_path, SituationAudioLoadMode mode, bool looping, SituationSound* out_sound); // Load a sound from a file.
SITAPI SituationError SituationLoadSoundFromStream(SituationStreamReadCallback on_read, SituationStreamSeekCallback on_seek, void* user_data, const SituationAudioFormat* format, bool looping, SituationSound* out_sound); // Load a sound from a custom stream.
SITAPI void SituationUnloadSound(SituationSound* sound);                                // Unload a sound and free its resources.
SITAPI SituationError SituationPlayLoadedSound(SituationSound* sound);                  // Play a loaded sound (restarts if already playing).
SITAPI SituationError SituationStopLoadedSound(SituationSound* sound);                  // Stop a specific sound from playing.
SITAPI SituationError SituationStopAllLoadedSounds(void);                               // Stop all currently playing sounds.

SITAPI void SituationStopTone(SituationToneHandle handle);                              // Gracefully stop a tone by triggering its release envelope. Invalid handles are ignored.

SITAPI void SituationPlayTone(SituationWaveType type, float frequency, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec); // Legacy: play a simple ADSR tone (backward compat / quick UI sounds).
SITAPI void SituationPlayMidiNote(int note, SituationWaveType type, float volume, float attack_sec, float decay_sec, float sustain_level, float release_sec, float hold_sec);   // Legacy: play a tone by MIDI note number (0-127).
SITAPI void SituationStopAllTones(void);                                                // Stop all active tones (triggers release on each).

// --- Sound Data Manipulation (Wave Utilities) ---
SITAPI SituationError SituationSoundCopy(const SituationSound* source, SituationSound* out_destination);    // Create a new sound by copying the raw PCM data from a source.
SITAPI SituationError SituationSoundCrop(SituationSound* sound, uint64_t initFrame, uint64_t finalFrame);   // Crop a sound's PCM data in-place to a new range.
SITAPI bool SituationSoundExportAsWav(const SituationSound* sound, const char* fileName);                   // Export the sound's raw PCM data to a WAV file.

// --- Sound Parameters and Effects ---
SITAPI SituationError SituationSetSoundVolume(SituationSound* sound, float volume);     // Set the volume for a specific sound.
SITAPI float SituationGetSoundVolume(SituationSound* sound);                            // Get the volume of a specific sound.
SITAPI SituationError SituationSetSoundPan(SituationSound* sound, float pan);           // Set the stereo pan for a sound [-1.0 to 1.0].
SITAPI float SituationGetSoundPan(SituationSound* sound);                               // Get the stereo pan of a sound.
SITAPI SituationError SituationSetSoundPitch(SituationSound* sound, float pitch);       // Set the pitch for a sound (resamples).
SITAPI float SituationGetSoundPitch(SituationSound* sound);                             // Get the pitch of a sound.
SITAPI SituationError SituationSetSoundFilter(SituationSound* sound, SituationFilterType type, float cutoff_hz, float q_factor);                    // Apply a low-pass or high-pass filter to a sound.
SITAPI SituationError SituationSetSoundEcho(SituationSound* sound, bool enabled, float delay_sec, float feedback, float wet_mix);                   // Apply an echo effect to a sound.
SITAPI SituationError SituationSetSoundReverb(SituationSound* sound, bool enabled, float room_size, float damping, float wet_mix, float dry_mix);   // Apply a reverb effect to a sound.

// --- Custom Audio Processing ---
SITAPI SituationError SituationAttachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* user_data); // Attach a custom DSP processor to a sound's effect chain.
SITAPI SituationError SituationDetachAudioProcessor(SituationSound* sound, SituationAudioProcessorCallback processor, void* user_data); // Detach a custom DSP processor from a sound.

// [Phase H] Removed: Legacy Mixer API (replaced by node graph system)
// Use SituationCreateGraph() + SituationCreateNode(SITUATION_NODE_MIXER) + SituationProcessGraph() instead.

// ================================================================================================
// NODE GRAPH & DEVICE REGISTRY API (Phase 3-5)
// ================================================================================================

// --- Device Registry Functions ---
SITAPI void SituationInitDeviceRegistry(void);                                          // Initialize the built-in device registry (call once at startup).
SITAPI int SituationGetRegisteredDeviceCount(void);                                     // Get the number of registered audio device types.
SITAPI SituationError SituationRegisterDeviceType(const SituationDeviceMetadata* meta); // Register a custom device type with the registry.
SITAPI SituationError SituationGetDeviceMetadata(SituationNodeType type, SituationDeviceMetadata* out_meta); // Get metadata for a registered device type.
SITAPI bool SituationIsDeviceRegistered(SituationNodeType type);                        // Check if a device type is registered.
SITAPI const char* SituationGetCategoryName(SituationDeviceCategory category);          // Get the display name for a device category.

// --- Active Graph (Audio Callback Integration) ---
SITAPI SituationError SituationSetActiveGraph(SituationAudioGraph* graph);              // Set the active audio processing graph (replaces default). NULL disables graph processing.
SITAPI SituationAudioGraph* SituationGetActiveGraph(void);                              // Get the currently active audio processing graph (NULL if none).

// --- Node Graph Functions ---
SITAPI SituationAudioGraph* SituationCreateGraph(void);                                 // Create a new audio processing graph.
SITAPI void SituationDestroyGraph(SituationAudioGraph* graph);                          // Destroy a graph and all its nodes/patches.
SITAPI SituationError SituationCreateNode(SituationAudioGraph* graph, SituationNodeType type, SituationNodeHandle* handle); // Create a node of the given type in the graph.
SITAPI SituationError SituationDestroyNode(SituationAudioGraph* graph, SituationNodeHandle handle); // Remove and destroy a node from the graph.
SITAPI SituationNode* SituationGetNode(SituationAudioGraph* graph, SituationNodeHandle handle); // Get a direct pointer to a node (for advanced use).
SITAPI SituationError SituationCreatePatch(SituationAudioGraph* graph, SituationNodeHandle src, int src_port, SituationNodeHandle dst, int dst_port, bool is_control); // Connect an output port to an input port.
SITAPI SituationError SituationRemovePatch(SituationAudioGraph* graph, SituationNodeHandle src, int src_port, SituationNodeHandle dst, int dst_port, bool is_control); // Disconnect a specific patch between two ports.
SITAPI SituationError SituationDestroyPatch(SituationAudioGraph* graph, SituationNodeHandle src, int src_port, SituationNodeHandle dst, int dst_port); // Disconnect a patch between two ports (legacy, no is_control param).
SITAPI SituationError SituationSetControl(SituationAudioGraph* graph, SituationNodeHandle handle, uint32_t control_id, float value); // Set a control parameter on a node.
SITAPI SituationError SituationGetControl(SituationAudioGraph* graph, SituationNodeHandle handle, uint32_t control_id, float* out_value); // Get the current value of a node's control parameter.

// ================================================================================================
// MIDI CONTROL INTEGRATION
// ================================================================================================

/** @brief MIDI device information for device selection. */
typedef struct {
    int device_id;                  // Device ID for use with SituationEnableMidiControl
    char device_name[128];          // Human-readable device name
    int is_input;                   // 1 if input device, 0 otherwise
    int is_output;                  // 1 if output device, 0 otherwise
} SituationMidiDeviceInfo;

// --- MIDI Device Control ---
SITAPI SituationError SituationEnableMidiControl(SituationAudioGraph* graph, SituationNodeHandle handle, int device_id);  // Enable MIDI CC control for a node. Pass device_id=-1 for auto-select.
SITAPI SituationError SituationDisableMidiControl(SituationAudioGraph* graph, SituationNodeHandle handle);                // Disable MIDI control for a node.
SITAPI SituationError SituationAutoConnectMidi(SituationAudioGraph* graph, SituationNodeHandle handle);                   // Convenience: auto-select first available MIDI input. Equivalent to EnableMidiControl(..., -1).
SITAPI int SituationListMidiDevices(SituationMidiDeviceInfo* devices, int max_count);                                     // List available MIDI input devices. Returns number found.
SITAPI SituationError SituationGetMidiDeviceName(int device_id, char* out_name, size_t out_name_size);                    // PortMidi device name for device_id (hardware or virtual).
SITAPI int SituationIsMidiEnabled(SituationAudioGraph* graph, SituationNodeHandle handle);                                // Check if a node has MIDI control enabled. Returns 1/0.
SITAPI SituationError SituationSetNodeMidiChannel(SituationAudioGraph* graph, SituationNodeHandle handle, int channel);   // Filter MIDI to channel 0-15, or -1 omni.

// --- Official names for harness virtual MIDI + graph tone synth target (PortMidi + SIT_MidiDevice) ---
#define SITUATION_TEST_MIDI_CHANNEL           0    /* 0-based; human-readable MIDI channel 1 */
#define SITUATION_VIRTUAL_MIDI_IN_NAME        "Situation Test MIDI In"
#define SITUATION_VIRTUAL_MIDI_OUT_NAME       "Situation Test MIDI Out"
#define SITUATION_TONE_SYNTH_MIDI_DEVICE_NAME "Tone Synth"

// --- Virtual MIDI loopback (integration testing; no hardware keyboard required) ---
SITAPI SituationError SituationSetupVirtualMidiLoopback(int* out_input_device_id);  // Create connected virtual out→in pair. Returns input device_id for SituationEnableMidiControl().
SITAPI SituationError SituationVirtualMidiNoteOnEx(uint8_t channel, uint8_t note, uint8_t velocity); // Channel-aware note-on (0-15).
SITAPI SituationError SituationVirtualMidiNoteOffEx(uint8_t channel, uint8_t note);                  // Channel-aware note-off (0-15).
SITAPI SituationError SituationVirtualMidiNoteOn(uint8_t note, uint8_t velocity);     // Inject note-on on channel 0 (legacy wrapper).
SITAPI SituationError SituationVirtualMidiNoteOff(uint8_t note);                      // Inject note-off on channel 0 (legacy wrapper).
SITAPI SituationError SituationVirtualMidiControlChange(uint8_t channel, uint8_t controller, uint8_t value); // CC (e.g. mod wheel, expression).
SITAPI SituationError SituationVirtualMidiPitchBend(uint8_t channel, int16_t bend);   // Pitch bend 0..16383 (center 8192).
SITAPI SituationError SituationVirtualMidiProgramChange(uint8_t channel, uint8_t program); // Program change on channel 0-15.
SITAPI void SituationTeardownVirtualMidiLoopback(void);                             // Close and destroy the virtual loopback devices.

// ================================================================================================
// MIDI LEARN INTEGRATION (v2.6.0)
// ================================================================================================
// Dynamic MIDI CC learning: map physical knobs/faders to node parameters at runtime.
// Requires MIDI to be enabled first via SituationEnableMidiControl().

// --- MIDI Learn Lifecycle ---
SITAPI SituationError SituationEnableMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);                   // Enable MIDI Learn capability for a node. MIDI must already be enabled.
SITAPI SituationError SituationDisableMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);                  // Disable MIDI Learn for a node.
SITAPI int SituationIsMidiLearnEnabled(SituationAudioGraph* graph, SituationNodeHandle handle);                           // Check if MIDI Learn is enabled. Returns 1/0.

// --- Learning Operations ---
SITAPI SituationError SituationStartMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle, int control_index, const char* param_name, float min_value, float max_value, int scaling); // Start learning: next CC received maps to this param. Scaling: 0=linear, 1=log, 2=dB, 3=discrete. Times out after 5s.
SITAPI SituationError SituationCancelMidiLearn(SituationAudioGraph* graph, SituationNodeHandle handle);                   // Cancel an active learn operation.
SITAPI int SituationIsLearning(SituationAudioGraph* graph, SituationNodeHandle handle);                                   // Check if currently in learn mode. Returns 1/0.

// --- Mapping Management ---
SITAPI SituationError SituationClearMidiMapping(SituationAudioGraph* graph, SituationNodeHandle handle, int control_index); // Clear a specific learned CC mapping.
SITAPI SituationError SituationClearAllMidiMappings(SituationAudioGraph* graph, SituationNodeHandle handle);               // Clear all learned mappings for a node.

// --- Preset Persistence ---
SITAPI SituationError SituationSaveMidiPreset(SituationAudioGraph* graph, SituationNodeHandle handle, const char* filename);  // Save MIDI Learn mappings to JSON file.
SITAPI SituationError SituationLoadMidiPreset(SituationAudioGraph* graph, SituationNodeHandle handle, const char* filename);  // Load MIDI Learn mappings from JSON file.

// --- Graph Serialization Functions ---
SITAPI SituationError SituationSaveGraphToFile(const SituationAudioGraph* graph, const char* filepath);   // Save a graph to a JSON file.
SITAPI SituationError SituationLoadGraphFromFile(SituationAudioGraph* graph, const char* filepath, const SituationDeviceFunctions* device_funcs, int num_device_funcs); // Load a graph from a JSON file, re-creating nodes via device_funcs.
SITAPI char* SituationSerializeGraphToJSON(const SituationAudioGraph* graph);           // Serialize a graph to a JSON string (caller must free with SituationFreeJSONString).
SITAPI SituationError SituationDeserializeGraphFromJSON(SituationAudioGraph* graph, const char* json_string, const SituationDeviceFunctions* device_funcs, int num_device_funcs); // Deserialize a graph from a JSON string.
SITAPI void SituationFreeJSONString(char* json_string);                                 // Free a JSON string returned by SituationSerializeGraphToJSON.
SITAPI const char* SituationGetSerializationVersion(void);                              // Get the current serialization format version string.
SITAPI bool SituationIsVersionCompatible(const char* json_version);                     // Check if a serialized version is compatible with this library.

// --- Device Enumeration (Phase 0) ---
SITAPI SituationAudioDeviceInfo* SituationEnumerateAudioDevices(int* out_count);         // Enumerate available audio devices. Caller must free with SituationFreeDeviceList.
SITAPI void SituationFreeDeviceList(SituationAudioDeviceInfo* devices, int count);       // Free a device list returned by SituationEnumerateAudioDevices.
SITAPI SituationAudioDeviceInfo* SituationFindBestDevice(SituationAudioDeviceType preferred_type, uint32_t min_channels_out, uint32_t min_channels_in); // Find the best matching device by type and channel requirements.


//==================================================================================
// Filesystem Module
//==================================================================================
// --- Path Management & Special Directories ---
SITAPI char* SituationGetAppSavePath(const char* app_name);                             // Get a safe, persistent path for saving application data (caller must free).
SITAPI char* SituationGetBasePath(void);                                                // Get the path to the directory containing the executable (caller must free).
static char* SituationGetBasePathFromFile(const char* file_path);                       // Internal helper: Extract directory path from file path (caller must free).
SITAPI char* SituationJoinPath(const char* base_path, const char* file_or_dir_name);    // Join two path components with the correct OS separator (caller must free).
SITAPI const char* SituationGetFileName(const char* full_path);                         // Extract the file name (including extension) from a full path.
SITAPI const char* SituationGetFileExtension(const char* file_path);                    // Extract the file extension from a path.

// --- File & Directory Queries ---
SITAPI bool SituationFileExists(const char* file_path);                                 // Check if a file exists at the given path.
SITAPI bool SituationDirectoryExists(const char* dir_path);                             // Check if a directory exists at the given path.
SITAPI long SituationGetFileModTime(const char* file_path);                             // Get the last modification time of a file (Unix timestamp).

// --- File Operations ---
SITAPI SituationError SituationLoadFileData(const char* file_path, unsigned int* out_bytes_read, unsigned char** out_data);   // Load an entire file into a memory buffer (caller must free).
SITAPI SituationError SituationSaveFileData(const char* file_path, const void* data, unsigned int bytes_to_write);    // Save a block of memory to a file.
#ifdef SITUATION_ENABLE_THREADING
SITAPI SituationJobId SituationLoadFileAsync(SituationThreadPool* pool, const char* file_path, SituationFileLoadCallback callback, void* user_data); // Asynchronously load a file.
SITAPI SituationJobId SituationSaveFileAsync(SituationThreadPool* pool, const char* file_path, const void* data, size_t size, SituationFileSaveCallback callback, void* user_data); // Asynchronously save a file.
SITAPI SituationJobId SituationLoadFileTextAsync(SituationThreadPool* pool, const char* file_path, SituationFileTextLoadCallback callback, void* user_data); // Asynchronously load a text file.
SITAPI SituationJobId SituationSaveFileTextAsync(SituationThreadPool* pool, const char* file_path, const char* text, SituationFileSaveCallback callback, void* user_data); // Asynchronously save a text file.
#endif
SITAPI char* SituationLoadFileText(const char* file_path);                              // Load a text file into a null-terminated string (caller must free).
SITAPI bool SituationSaveFileText(const char* file_path, const char* text);             // Save a null-terminated string to a text file.
SITAPI bool SituationCopyFile(const char* source_path, const char* dest_path);          // Copy a file.
SITAPI bool SituationDeleteFile(const char* file_path);                                 // Delete a file.
SITAPI bool SituationMoveFile(const char* old_path, const char* new_path);              // Move/rename a file, even across drives on Windows.
SITAPI bool SituationRenameFile(const char* old_path, const char* new_path);            // Alias for SituationMoveFile.

// --- Directory Operations ---
SITAPI bool SituationCreateDirectory(const char* dir_path, bool create_parents);        // Create a directory, optionally creating parent directories.
SITAPI bool SituationDeleteDirectory(const char* dir_path, bool recursive);             // Delete a directory, optionally deleting all its contents.
SITAPI char** SituationListDirectoryFiles(const char* dir_path, int* out_count);        // List files and subdirectories in a path (caller must free with SituationFreeDirectoryFileList).
SITAPI void SituationFreeDirectoryFileList(char** file_list, int count);                // Free the memory allocated by SituationListDirectoryFiles.

//==================================================================================
// Miscellaneous Module
//==================================================================================
// --- Temporal Oscillator System ---
SITAPI bool SituationTimerGetOscillatorState(int oscillator_id);                        // Get the current binary state (0 or 1) of an oscillator.
SITAPI bool SituationTimerGetPreviousOscillatorState(int oscillator_id);                // Get the previous frame's state of an oscillator.
SITAPI bool SituationTimerHasOscillatorUpdated(int oscillator_id);                      // Check if an oscillator's state has changed this frame.
SITAPI bool SituationTimerPingOscillator(int oscillator_id);                            // Check if an oscillator's period has elapsed since the last ping.
SITAPI uint64_t SituationTimerGetOscillatorTriggerCount(int oscillator_id);             // Get the total number of times an oscillator has triggered.
SITAPI double SituationTimerGetOscillatorPeriod(int oscillator_id);                     // Get the period of an oscillator in seconds.
SITAPI SituationError SituationSetTimerOscillatorPeriod(int oscillator_id, double period_seconds); // Set the period of an oscillator.
SITAPI double SituationTimerGetPingProgress(int oscillator_id);                         // Get progress [0.0 to 1.0+] of the interval since the last successful ping.
SITAPI double SituationTimerGetTime(void);                                              // Get the total time elapsed since initialization.

// --- Color Space Conversions ---
SITAPI void SituationConvertColorToVector4(ColorRGBA c, Vector4* out_normalized_color); // Convert an 8-bit ColorRGBA struct to a normalized Vector4.
SITAPI ColorHSV SituationRgbToHsv(ColorRGBA rgb);                                       // Converts a standard RGBA color to the Hue, Saturation, Value color space.
SITAPI ColorRGBA SituationHsvToRgb(ColorHSV hsv);                                       // Converts a Hue, Saturation, Value color back to the standard RGBA color space.
SITAPI ColorYPQA SituationColorToYPQ(ColorRGBA color);                                  // Converts a standard RGBA color to the YPQA (Luma, Phase, Quadrature) color space.
SITAPI ColorRGBA SituationColorFromYPQ(ColorYPQA ypq_color);                            // Converts a YPQA color back to the standard RGBA color space.
SITAPI ColorYPQA SituationYpqLerp(ColorYPQA a, ColorYPQA b, float t);                   // Interpolate YPQ; phase uses shortest arc on the hue wheel.
SITAPI ColorYPQA SituationYpqAdjustLuma(ColorYPQA color, float luma_factor);             // Scale Y (luma); preserve phase and chroma.
SITAPI ColorYPQA SituationYpqAdjustPhase(ColorYPQA color, int phase_shift);             // Rotate hue; P shifts by byte steps mod 256.
SITAPI ColorYPQA SituationYpqAdjustChroma(ColorYPQA color, float chroma_factor);          // Scale Q (chroma amplitude); preserve luma and phase.
SITAPI float SituationYpqGetLuma(ColorYPQA color);                                      // Normalized luma [0, 1].
SITAPI float SituationYpqGetHueDegrees(ColorYPQA color);                                // Hue in degrees [0, 360).
SITAPI float SituationYpqGetChroma(ColorYPQA color);                                    // Normalized chroma amplitude [0, 1].
SITAPI float SituationYpqDistance(ColorYPQA a, ColorYPQA b);                          // Weighted distance in YPQ space.
SITAPI bool SituationYpqEquals(ColorYPQA a, ColorYPQA b, unsigned char tolerance);    // Per-channel tolerance compare.
SITAPI ColorYPQf SituationColorToYPQf(ColorRGBA color);                                 // RGBA → normalized float YPQ (no 8-bit quantize).
SITAPI ColorRGBA SituationColorFromYPQf(ColorYPQf ypq);                                 // Float YPQ → RGBA (linear YIQ, clamped RGB).
SITAPI ColorYPQA SituationYpqQuantize(ColorYPQf ypq);                                   // Float YPQ → 8-bit ColorYPQA.
SITAPI ColorYPQf SituationYpqClampInGamut(ColorYPQf ypq);                               // Reduce chroma if linear RGB would clip.

//==================================================================================
// Threading Module
//==================================================================================

/**
 * @brief Per-logical-processor topology entry (Epic A — Threading Bolstering).
 * @note logical_id is the index used in affinity masks (bit `logical_id` of a uint64_t mask).
 */
typedef struct SituationLogicalProcessorInfo {
    uint32_t logical_id;
    uint32_t physical_core_id;
    uint16_t numa_node;
    bool     is_hyperthread_sibling;
} SituationLogicalProcessorInfo;

/**
 * @brief Cached CPU topology snapshot (read-only after refresh).
 */
typedef struct SituationCpuTopology {
    uint32_t logical_count;
    uint32_t physical_count;
    uint16_t numa_node_count;
    SituationLogicalProcessorInfo processors[SITUATION_MAX_LOGICAL_PROCESSORS];
} SituationCpuTopology;

typedef struct {
    uint16_t node_id;
    uint32_t processor_count;
    uint64_t memory_bytes;
    uint64_t processor_mask_low;
} SituationNumaNodeInfo;

typedef struct {
    uint16_t node_count;
    SituationNumaNodeInfo nodes[SITUATION_MAX_NUMA_NODES];
} SituationNumaTopology;

// --- CPU & Thread Management ---
SITAPI uint32_t SituationGetCPUThreadCount(void);           // Gets logical processors (Threads)
SITAPI uint32_t SituationGetCPUCoreCount(void);             // Gets physical processors (Cores) from cached topology
SITAPI bool SituationRefreshCpuTopology(void);              // Rebuilds the process-wide topology cache
SITAPI bool SituationGetCpuTopology(const SituationCpuTopology** out_topology); // Pointer to cached topology (NULL on failure)
SITAPI bool SituationSetThreadAffinity(uint64_t core_mask); // Pins the CURRENT thread (logical CPU bitmask, bits 0..63)
SITAPI bool SituationSetThreadAffinityEx(uint64_t core_mask, uint64_t* out_previous); // Set affinity; optional previous mask
SITAPI bool SituationGetThreadAffinity(uint64_t* out_mask); // Reads affinity mask for the CURRENT thread
SITAPI int  SituationGetCurrentProcessorIndex(void);        // Logical CPU index for current thread, or -1 if unknown
SITAPI int  SituationGetThreadNumaNode(void);               // NUMA node for current thread, or -1 if unknown
SITAPI uint64_t SituationBuildPhysicalCoreMask(int physical_core_index); // All logical CPUs on one physical core
SITAPI uint64_t SituationBuildUniqueCoreMask(int start_physical_core, int count, bool avoid_siblings); // One LP per core
SITAPI uint64_t SituationBuildNumaNodeMask(int numa_node_index); // All logical CPUs on a NUMA node
SITAPI uint64_t SituationGetConfiguredMainThreadAffinity(void);   // Init mask for main thread (0 = no pin)
SITAPI uint64_t SituationGetConfiguredRenderThreadAffinity(void); // Effective render mask (init or default)
SITAPI uint64_t SituationGetConfiguredAudioThreadAffinity(void);  // Effective audio mask (init or default)
SITAPI uint64_t SituationGetConfiguredIOThreadAffinity(void);     // Effective I/O mask (init or default CPU 3)
SITAPI bool SituationRefreshNumaTopology(void);                   // Rebuild NUMA summary from CPU topology + OS memory
SITAPI bool SituationGetNumaTopology(const SituationNumaTopology** out_topology); // Cached NUMA snapshot
SITAPI int SituationGetPreferredNumaNode(void);                   // TLS: node for current thread, or -1 if unset
#ifdef SITUATION_ENABLE_THREADING
SITAPI bool SituationCreateThreadPool(SituationThreadPool* pool, size_t num_threads, size_t queue_size, double hot_reload_rate, bool disable_io); // Initializes the thread pool with dual-priority queues and worker threads.
SITAPI void SituationDestroyThreadPool(SituationThreadPool* pool); 											// Shuts down the thread pool and releases resources.
SITAPI SituationJobId SituationSubmitJobEx(SituationThreadPool* pool, void (*func)(void*, void*), const void* data, size_t data_size, SituationJobFlags flags); // Submits a job with priority flags and optional data payload.
 // Legacy wrapper for simple pointer passing (Low priority, no copy).
#define SituationSubmitJob(pool, func, user_ptr) \
    SituationSubmitJobEx(pool, (void(*)(void*, void*))func, user_ptr, 0, SIT_SUBMIT_DEFAULT)
SITAPI void SituationDispatchParallel(SituationThreadPool* pool, int count, int min_batch_size, void (*func)(int index, void* user_data), void* user_data); // Executes a loop in parallel across worker threads (Fork-Join).
SITAPI bool SituationWaitForJob(SituationThreadPool* pool, SituationJobId job_id); 							// Waits for a specific job to complete (O(1) check).
SITAPI void SituationWaitForAllJobs(SituationThreadPool* pool); 											// Blocks until all queued jobs are finished.
SITAPI bool SituationAddJobDependency(SituationThreadPool* pool, SituationJobId prerequisite_job, SituationJobId dependent_job); // Adds a dependency between two jobs (prereq -> dependent).
SITAPI bool SituationAddJobDependencies(SituationThreadPool* pool, SituationJobId* prerequisites, int count, SituationJobId dependent_job); // Adds multiple dependencies for a single dependent job.
SITAPI void SituationDumpTaskGraph(SituationThreadPool* pool, FILE* out_stream, bool json_mode); 			// Prints the current task graph state to the stream.
SITAPI SituationThreadingStatus SituationGetThreadingStatus(void);                                          // Runtime threading capabilities + pool summary
SITAPI void SituationPrintThreadingStatus(FILE* out_stream);                                                // Human-readable threading status (stdout if NULL)
SITAPI size_t SituationGetQueueDepth(SituationThreadPool* pool, SituationJobQueueMask mask);                 // Pending jobs per queue mask
SITAPI size_t SituationGetHighQueueDepth(SituationThreadPool* pool);                                          // High-priority queue depth
SITAPI int SituationGetActiveJobCount(SituationThreadPool* pool);                                           // active_jobs counter
SITAPI bool SituationGetThreadPoolSnapshot(SituationThreadPool* pool, SituationThreadPoolSnapshot* out);    // Worker/I/O/render/audio placement snapshot
SITAPI void SituationDumpThreadPoolStatus(SituationThreadPool* pool, FILE* out_stream, bool json_mode);      // Pool metrics + per-role CPU snapshot
SITAPI void SituationDumpThreadingReport(SituationThreadPool* pool, FILE* out_stream, bool json_mode);       // Status + topology line + pool dump
SITAPI uint32_t SituationGetRecommendedWorkerCount(uint32_t reserved_threads, bool use_physical_cores);     // Sizing helper (no pool required)
SITAPI bool SituationGetThreadPoolMetrics(SituationThreadPool* pool, SituationThreadPoolMetrics* out_metrics); // Scheduler counters snapshot
SITAPI void SituationResetThreadPoolStats(SituationThreadPool* pool);                                         // Zero scheduler counters
SITAPI void SituationDumpThreadPoolMetrics(SituationThreadPool* pool, FILE* out_stream, bool json_mode);      // Metrics-only dump
SITAPI SituationThreadPool* SituationGetInternalThreadPool(void);                                            // Returns pointer to the library's internal thread pool (NULL if not initialized).

SITAPI SituationJobId SituationLoadSoundFromFileAsync(SituationThreadPool* pool, const char* file_path, bool looping, SituationSound* out_sound); // Asynchronously loads and decodes a sound file.
#endif // SITUATION_ENABLE_THREADING

// --- Node Graph SFX Routing (v2.6.5) ---
SITAPI SituationError SituationSetToneRouting(SituationToneHandle handle, bool route_to_graph);                   // Route a procedural tone to the active graph's SFX sound source.
SITAPI SituationError SituationSetGraphSFXSource(SituationNodeHandle handle);                                     // Designate the Sound Source node in the active graph to receive routed SFX tones.

// Close C++ linkage guard
#ifdef __cplusplus
}
#endif

#endif // SITUATION_API_H
