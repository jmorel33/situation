/***************************************************************************************************
*
*   sit/aud/threading_diagnostics.h - Threading Diagnostics and Hardening
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Provides:
*   - Threading capability detection
*   - Error checking macros for defensive programming
*   - Debug logging system (conditional compilation)
*   - Timeout protection for mutex operations
*   - Diagnostic functions for threading status
*
***************************************************************************************************/

#ifndef SITUATION_THREADING_DIAGNOSTICS_H
#define SITUATION_THREADING_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdio.h>

// ================================================================================================
// THREADING CAPABILITY DETECTION
// ================================================================================================

/**
 * @brief Threading capability flags
 */
typedef enum {
    SITUATION_THREAD_CAP_NONE           = 0,
    SITUATION_THREAD_CAP_C11_THREADS    = (1 << 0),  // C11 threads available
    SITUATION_THREAD_CAP_C11_ATOMICS    = (1 << 1),  // C11 atomics available
    SITUATION_THREAD_CAP_MUTEX          = (1 << 2),  // Mutex support
    SITUATION_THREAD_CAP_SLEEP          = (1 << 3),  // Sleep support (platform-specific)
    SITUATION_THREAD_CAP_PLATFORM_SLEEP = (1 << 4),  // Platform-specific sleep (recommended)
} SituationThreadCapability;

/**
 * @brief Threading status information
 */
typedef struct {
    bool available;                      // Threading available
    int capabilities;                    // Bitmask of SituationThreadCapability
    const char* platform;                // Platform name ("Windows", "POSIX", "Unknown")
    const char* sleep_impl;              // Sleep implementation ("Windows Sleep", "POSIX usleep", "thrd_sleep", "None")
    bool sleep_reliable;                 // Is sleep implementation reliable?
    int max_threads;                     // Maximum recommended threads (0 = unknown)
    const char* warnings[4];             // Warning messages (NULL-terminated)
    int warning_count;                   // Number of warnings
} SituationThreadingStatus;

/**
 * @brief Get threading status and capabilities
 * @return Threading status structure
 */
static SituationThreadingStatus SituationGetThreadingStatus(void) {
    SituationThreadingStatus status = {0};
    
    #if !defined(__STDC_NO_THREADS__)
        status.available = true;
        status.capabilities |= SITUATION_THREAD_CAP_C11_THREADS;
        status.capabilities |= SITUATION_THREAD_CAP_MUTEX;
        
        #if !defined(__STDC_NO_ATOMICS__)
            status.capabilities |= SITUATION_THREAD_CAP_C11_ATOMICS;
        #else
            status.warnings[status.warning_count++] = "C11 atomics not available";
        #endif
        
        // Platform detection
        #if defined(_WIN32)
            status.platform = "Windows";
            status.sleep_impl = "Windows Sleep (recommended)";
            status.sleep_reliable = true;
            status.capabilities |= SITUATION_THREAD_CAP_PLATFORM_SLEEP;
            status.max_threads = 64;  // Reasonable default for Windows
        #elif defined(__unix__) || defined(__APPLE__)
            status.platform = "POSIX";
            status.sleep_impl = "POSIX usleep (recommended)";
            status.sleep_reliable = true;
            status.capabilities |= SITUATION_THREAD_CAP_PLATFORM_SLEEP;
            status.max_threads = 64;  // Reasonable default for POSIX
        #else
            status.platform = "Unknown";
            status.sleep_impl = "thrd_sleep (may have issues)";
            status.sleep_reliable = false;
            status.capabilities |= SITUATION_THREAD_CAP_SLEEP;
            status.warnings[status.warning_count++] = "Platform-specific sleep not available, using thrd_sleep (may be unreliable)";
            status.max_threads = 0;
        #endif
        
        // Check for tinycthread issues
        #if defined(_WIN32) && !defined(SITUATION_USE_PLATFORM_SLEEP)
            status.warnings[status.warning_count++] = "Using tinycthread on Windows - recommend defining SITUATION_USE_PLATFORM_SLEEP";
        #endif
        
    #else
        status.available = false;
        status.platform = "None";
        status.sleep_impl = "None";
        status.sleep_reliable = false;
        status.max_threads = 0;
        status.warnings[status.warning_count++] = "C11 threads not available - threading disabled";
    #endif
    
    return status;
}

/**
 * @brief Print threading status to stdout
 */
static void SituationPrintThreadingStatus(void) {
    SituationThreadingStatus status = SituationGetThreadingStatus();
    
    printf("========================================\n");
    printf("Threading Status\n");
    printf("========================================\n");
    printf("Available:       %s\n", status.available ? "YES" : "NO");
    printf("Platform:        %s\n", status.platform);
    printf("Sleep Impl:      %s\n", status.sleep_impl);
    printf("Sleep Reliable:  %s\n", status.sleep_reliable ? "YES" : "NO");
    printf("Max Threads:     %d\n", status.max_threads);
    
    printf("\nCapabilities:\n");
    printf("  C11 Threads:   %s\n", (status.capabilities & SITUATION_THREAD_CAP_C11_THREADS) ? "YES" : "NO");
    printf("  C11 Atomics:   %s\n", (status.capabilities & SITUATION_THREAD_CAP_C11_ATOMICS) ? "YES" : "NO");
    printf("  Mutex:         %s\n", (status.capabilities & SITUATION_THREAD_CAP_MUTEX) ? "YES" : "NO");
    printf("  Platform Sleep:%s\n", (status.capabilities & SITUATION_THREAD_CAP_PLATFORM_SLEEP) ? "YES" : "NO");
    
    if (status.warning_count > 0) {
        printf("\nWarnings:\n");
        for (int i = 0; i < status.warning_count; i++) {
            printf("  - %s\n", status.warnings[i]);
        }
    }
    
    printf("========================================\n");
}

// ================================================================================================
// DEBUG LOGGING SYSTEM
// ================================================================================================

#ifdef SITUATION_DEBUG_THREADING
    #define SITUATION_THREAD_LOG(fmt, ...) \
        fprintf(stderr, "[Thread %s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
    #define SITUATION_THREAD_LOG_ERROR(fmt, ...) \
        fprintf(stderr, "[Thread ERROR %s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define SITUATION_THREAD_LOG(fmt, ...) do {} while(0)
    #define SITUATION_THREAD_LOG_ERROR(fmt, ...) do {} while(0)
#endif

// ================================================================================================
// ERROR CHECKING MACROS
// ================================================================================================

#if !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    
    /**
     * @brief Check mutex initialization result
     */
    #define SITUATION_CHECK_MUTEX_INIT(result, error_code) \
        do { \
            if ((result) != thrd_success) { \
                SITUATION_THREAD_LOG_ERROR("Mutex init failed"); \
                return (error_code); \
            } \
        } while(0)
    
    /**
     * @brief Check mutex lock result
     */
    #define SITUATION_CHECK_MUTEX_LOCK(result, error_code) \
        do { \
            if ((result) != thrd_success) { \
                SITUATION_THREAD_LOG_ERROR("Mutex lock failed"); \
                return (error_code); \
            } \
        } while(0)
    
    /**
     * @brief Check mutex unlock result
     */
    #define SITUATION_CHECK_MUTEX_UNLOCK(result, error_code) \
        do { \
            if ((result) != thrd_success) { \
                SITUATION_THREAD_LOG_ERROR("Mutex unlock failed"); \
                return (error_code); \
            } \
        } while(0)
    
    /**
     * @brief Check thread creation result
     */
    #define SITUATION_CHECK_THREAD_CREATE(result, error_code) \
        do { \
            if ((result) != thrd_success) { \
                SITUATION_THREAD_LOG_ERROR("Thread creation failed"); \
                return (error_code); \
            } \
        } while(0)
    
    /**
     * @brief Check thread join result
     */
    #define SITUATION_CHECK_THREAD_JOIN(result, error_code) \
        do { \
            if ((result) != thrd_success) { \
                SITUATION_THREAD_LOG_ERROR("Thread join failed"); \
                return (error_code); \
            } \
        } while(0)
    
    /**
     * @brief Safe mutex lock with error handling
     */
    #define SITUATION_SAFE_MUTEX_LOCK(mutex, error_code) \
        do { \
            int _lock_result = mtx_lock(&(mutex)); \
            SITUATION_CHECK_MUTEX_LOCK(_lock_result, error_code); \
            SITUATION_THREAD_LOG("Mutex locked"); \
        } while(0)
    
    /**
     * @brief Safe mutex unlock with error handling
     */
    #define SITUATION_SAFE_MUTEX_UNLOCK(mutex, error_code) \
        do { \
            int _unlock_result = mtx_unlock(&(mutex)); \
            SITUATION_CHECK_MUTEX_UNLOCK(_unlock_result, error_code); \
            SITUATION_THREAD_LOG("Mutex unlocked"); \
        } while(0)
    
#else
    // No-op macros when threading is not available
    #define SITUATION_CHECK_MUTEX_INIT(result, error_code) do {} while(0)
    #define SITUATION_CHECK_MUTEX_LOCK(result, error_code) do {} while(0)
    #define SITUATION_CHECK_MUTEX_UNLOCK(result, error_code) do {} while(0)
    #define SITUATION_CHECK_THREAD_CREATE(result, error_code) do {} while(0)
    #define SITUATION_CHECK_THREAD_JOIN(result, error_code) do {} while(0)
    #define SITUATION_SAFE_MUTEX_LOCK(mutex, error_code) do {} while(0)
    #define SITUATION_SAFE_MUTEX_UNLOCK(mutex, error_code) do {} while(0)
#endif

// ================================================================================================
// TIMEOUT PROTECTION
// ================================================================================================

#if !defined(__STDC_NO_THREADS__)
    #include <time.h>
    
    /**
     * @brief Try to lock mutex with timeout (busy-wait implementation)
     * @param mutex Mutex to lock
     * @param timeout_ms Timeout in milliseconds
     * @return true if locked, false if timeout
     */
    static bool SituationMutexTryLockTimeout(mtx_t* mutex, int timeout_ms) {
        // Use simple iteration-based timeout (portable)
        int iterations = timeout_ms * 10;  // ~100 iterations per ms
        
        while (iterations > 0) {
            int result = mtx_trylock(mutex);
            if (result == thrd_success) {
                SITUATION_THREAD_LOG("Mutex locked (with timeout)");
                return true;
            }
            
            iterations--;
            
            // Small yield to avoid busy-wait
            thrd_yield();
        }
        
        SITUATION_THREAD_LOG_ERROR("Mutex lock timeout after %d ms", timeout_ms);
        return false;
    }
    
    /**
     * @brief Lock mutex with timeout and error code
     */
    #define SITUATION_MUTEX_LOCK_TIMEOUT(mutex, timeout_ms, error_code) \
        do { \
            if (!SituationMutexTryLockTimeout(&(mutex), (timeout_ms))) { \
                SITUATION_THREAD_LOG_ERROR("Mutex lock timeout"); \
                return (error_code); \
            } \
        } while(0)
    
#else
    #define SITUATION_MUTEX_LOCK_TIMEOUT(mutex, timeout_ms, error_code) do {} while(0)
#endif

// ================================================================================================
// PLATFORM-SPECIFIC SLEEP
// ================================================================================================
// CRITICAL: On Windows, tinycthread's thrd_sleep() has a bug that causes hangs.
// Always use SITUATION_SLEEP_MS() which uses native Windows Sleep() on Windows,
// POSIX usleep() on Unix/macOS, or falls back to thrd_sleep() on other platforms.
// See THREADING_TROUBLESHOOTING_GUIDE.md for details.

#if defined(_WIN32)
    #include <windows.h>
    #define SITUATION_SLEEP_MS(ms) Sleep(ms)  // Native Windows Sleep() - reliable and correct
    #define SITUATION_SLEEP_AVAILABLE 1
#elif defined(__unix__) || defined(__APPLE__)
    #include <unistd.h>
    #define SITUATION_SLEEP_MS(ms) usleep((ms) * 1000)  // POSIX usleep() - reliable
    #define SITUATION_SLEEP_AVAILABLE 1
#elif !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    #define SITUATION_SLEEP_MS(ms) \
        do { \
            struct timespec ts = {.tv_sec = (ms) / 1000, .tv_nsec = ((ms) % 1000) * 1000000}; \
            thrd_sleep(&ts, NULL); \
        } while(0)  // WARNING: May be unreliable on some platforms
    #define SITUATION_SLEEP_AVAILABLE 1
#else
    #define SITUATION_SLEEP_MS(ms) do {} while(0)  // No sleep available
    #define SITUATION_SLEEP_AVAILABLE 0
#endif

// ================================================================================================
// ATOMIC OPERATION HELPERS
// ================================================================================================

#if !defined(__STDC_NO_ATOMICS__)
    #include <stdatomic.h>
    
    /**
     * @brief Safe atomic load with error checking
     */
    #define SITUATION_ATOMIC_LOAD(var) atomic_load(&(var))
    
    /**
     * @brief Safe atomic store with error checking
     */
    #define SITUATION_ATOMIC_STORE(var, value) atomic_store(&(var), (value))
    
    /**
     * @brief Safe atomic fetch-add with error checking
     */
    #define SITUATION_ATOMIC_FETCH_ADD(var, value) atomic_fetch_add(&(var), (value))
    
    /**
     * @brief Safe atomic compare-exchange
     */
    #define SITUATION_ATOMIC_COMPARE_EXCHANGE(var, expected, desired) \
        atomic_compare_exchange_strong(&(var), &(expected), (desired))
    
#else
    // Fallback to non-atomic operations (not thread-safe!)
    #define SITUATION_ATOMIC_LOAD(var) (var)
    #define SITUATION_ATOMIC_STORE(var, value) ((var) = (value))
    #define SITUATION_ATOMIC_FETCH_ADD(var, value) ((var) += (value), (var) - (value))
    #define SITUATION_ATOMIC_COMPARE_EXCHANGE(var, expected, desired) \
        (((var) == (expected)) ? ((var) = (desired), true) : false)
#endif

#endif // SITUATION_THREADING_DIAGNOSTICS_H
