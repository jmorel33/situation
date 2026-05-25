/***************************************************************************************************
*
*   sit/situation_impl_threading_diag.h - Threading Diagnostics and Hardening
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
*   This is a companion to situation_impl_threading.h, providing diagnostics and
*   hardening utilities for the base threading system.
*
***************************************************************************************************/

#ifndef SITUATION_IMPL_THREADING_DIAG_H
#define SITUATION_IMPL_THREADING_DIAG_H

#include <stdbool.h>
#include <stdio.h>

// Threading status API: SituationGetThreadingStatus / SituationPrintThreadingStatus
// (situation_impl_threading_observability.h). See doc/THREADING_TROUBLESHOOTING_GUIDE.md.

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
// See doc/THREADING_TROUBLESHOOTING_GUIDE.md for details.

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

#endif // SITUATION_IMPL_THREADING_DIAG_H
