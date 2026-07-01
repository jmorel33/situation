/***************************************************************************************************
*
*   situation_api_types_system.h - System, Init, Timer, and Topology Types
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   SituationInitInfo, init-state enum, temporal oscillators, color/YPQ structs, CPU/NUMA
*   topology, and thread-pool configuration types shared across platform and system modules.
*
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_TYPES_SYSTEM_H
#define SITUATION_API_TYPES_SYSTEM_H

#include "situation_api_config.h"
#include "situation_base_types.h"

// --- Initialization State Management (v2.3.40) ---
typedef enum SituationInitState {
    SITUATION_STATE_UNINITIALIZED = 0,  // Library not initialized
    SITUATION_STATE_INITIALIZING = 1,    // Init in progress, render thread starting
    SITUATION_STATE_READY = 2,           // Fully initialized, safe to create resources
    SITUATION_STATE_SHUTTING_DOWN = 3    // Cleanup in progress
} SituationInitState;
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

/** Main-window swapchain / default framebuffer color policy.
 *  Vulkan: 8-bit SDR, 10-bit SDR (A2R10 + SRGB_NONLINEAR), or HDR10 (A2R10 + HDR10_ST2084 when OS HDR on).
 *  OpenGL: 8-bit / 10-bit SDR hints only (no HDR10 on Windows v1). */
typedef enum SituationOutputColorDepth {
    SIT_OUTPUT_COLOR_AUTO   = 0, /**< Prefer HDR10 when OS+WSI confirm; else 10-bit SDR; else 8-bit */
    SIT_OUTPUT_COLOR_8BIT   = 1, /**< Force 8-bit SDR (default harness / CI when zero-init) */
    SIT_OUTPUT_COLOR_10BIT  = 2, /**< Request 10-bit SDR; fail-soft to 8-bit when unavailable */
    SIT_OUTPUT_COLOR_HDR10  = 3, /**< Request HDR10/PQ swapchain; fail-soft to 10-bit SDR then 8-bit */
} SituationOutputColorDepth;

/** Active main-window output color space (SituationGetGraphicsCaps().output_color_space). */
typedef enum SituationOutputColorSpace {
    SIT_OUTPUT_COLOR_SPACE_UNKNOWN        = 0,
    SIT_OUTPUT_COLOR_SPACE_SDR_SRGB       = 1, /**< 8- or 10-bit SDR (SRGB_NONLINEAR or UNORM) */
    SIT_OUTPUT_COLOR_SPACE_HDR10_ST2084   = 2, /**< HDR10 ST.2084 PQ swapchain color space */
} SituationOutputColorSpace;

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

    /** Main-window output bit depth (8-bit default when struct is zero-initialized).
     *  Query `SituationGetGraphicsCaps().output_bits_per_channel` after init for the active depth. */
    SituationOutputColorDepth output_color_depth;

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

    // OS-visible main thread name (debugger / Task Manager). NULL = SITUATION_MAIN_THREAD_NAME_DEFAULT.
    const char* main_thread_name;

    // Win32 shell AppUserModelID (taskbar pin / jump lists). NULL = SITUATION_DEFAULT_APP_USER_MODEL_ID.
    // Ignored on non-Windows platforms. Override with SituationWin32SetAppUserModelId() before init if needed.
    const char* app_user_model_id;

    // Optional runtime window icon (PNG or .ico on Windows). NULL = use embedded PE icon only.
    // Applied once after the main window is created during SituationInit (fail-soft on load error).
    const char* default_window_icon_path;

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

/** Sensible defaults for `SituationInit`: size, title, `SIT_OUTPUT_COLOR_AUTO`; other fields zero (same as `{0}`). */
static inline SituationInitInfo SituationInitInfoDefault(int width, int height, const char* title) {
    SituationInitInfo info = {0};
    info.window_width = width;
    info.window_height = height;
    info.window_title = title;
    info.output_color_depth = SIT_OUTPUT_COLOR_AUTO;
    return info;
}

// [v2.3.22] Render Queue Backpressure Policies
typedef enum {
    SIT_RENDER_BACKPRESSURE_SPIN  = 0, // Busy-wait loop (Highest responsiveness, uses CPU)
    SIT_RENDER_BACKPRESSURE_YIELD = 1, // Yield thread slice (OS decides, good balance)
    SIT_RENDER_BACKPRESSURE_SLEEP = 2  // Sleep 1ms (Low CPU usage, worst latency)
} SituationRenderBackpressurePolicy;
// --- System & backend (Core module) ---
#ifndef SITUATION_MAX_PROCESS_NAME_LEN
#define SITUATION_MAX_PROCESS_NAME_LEN 260
#endif

typedef struct SituationOSInfo {
    char name[64];                  // OS product name (e.g., "Windows 11", "Ubuntu 24.04", "macOS Sequoia")
    char version[64];               // Full version string (e.g., "10.0.22631", "6.8.0-45-generic")
    uint32_t build_number;          // Build number (Windows) or kernel patch level (Linux); 0 if unavailable
} SituationOSInfo;

typedef struct SituationProcessInfo {
    uint32_t pid;                                       // Process ID
    char name[SITUATION_MAX_PROCESS_NAME_LEN];          // Executable name (e.g., "explorer.exe")
    uint64_t memory_bytes;                              // Working set / RSS in bytes
} SituationProcessInfo;
/**
 * @brief Mapping-quality statistics for the 256³ YPQ → 8-bit RGB function.
 *
 * Populated by SituationYpqAnalyzeRgbMapping(). All fields are filled on
 * SITUATION_SUCCESS; callers should treat partial results as invalid on error.
 *
 * The full scan iterates all 16 777 216 (Y,P,Q) byte triples and records how
 * many map to distinct 8-bit RGB values. Typical results:
 *   unique_rgb        ≈ 5 600 000   (about 33% of the 2²⁴ RGB cube)
 *   duplicate_mappings ≈ 11 177 216
 *   rgb_holes          ≈ 10 576 000
 */
typedef struct SituationYpqRgbMappingStats {
    int64_t ypq_mappings;         /* always 256³ = 16 777 216 */
    int64_t unique_rgb;           /* distinct 8-bit RGB triples reachable from YPQ */
    int64_t duplicate_mappings;   /* ypq_mappings - unique_rgb */
    int64_t rgb_holes;            /* 2²⁴ - unique_rgb — RGB triples never produced */
    int     worst_axis_dup;       /* max duplicates in any single fixed-Q slice */
    int     worst_axis_at;        /* Q value where worst_axis_dup occurs */
} SituationYpqRgbMappingStats;

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

#endif /* SITUATION_API_TYPES_SYSTEM_H */
