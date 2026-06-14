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
#include <string.h>

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
                _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED, \
                    "Mutex lock failed (mtx_lock returned non-success)"); \
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
                _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_UNLOCK_FAILED, \
                    "Mutex unlock failed (mtx_unlock returned non-success)"); \
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
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_TIMEOUT,
            "Mutex lock timeout -- possible deadlock or contention");
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

// ================================================================================================
// THREAD NAMING (debugger / Task Manager visibility)
// ================================================================================================

#if defined(_WIN32)
#if !defined(__STDC_NO_THREADS__)
#include <pthread.h>
#endif

/**
 * @brief OS-level thread description (Task Manager / debugger) via SetThreadDescription.
 * @details Pseudo-handles (GetCurrentThread) often lack THREAD_SET_LIMITED_INFORMATION and
 *          fail silently. Prefer OpenThread on the current TID; fall back to DuplicateHandle
 *          (same approach winpthreads uses for the main thread).
 */
static void _SituationWin32SetThreadDescriptionUtf8(const char* name) {
    if (!name || !name[0]) return;

    typedef HRESULT (WINAPI *PFN_SetThreadDescription)(HANDLE, PCWSTR);
    static PFN_SetThreadDescription pfn = NULL;
    static bool resolved = false;
    if (!resolved) {
        HMODULE mod = GetModuleHandleA("kernel32.dll");
        if (mod) pfn = (PFN_SetThreadDescription)GetProcAddress(mod, "SetThreadDescription");
        if (!pfn) {
            mod = GetModuleHandleA("kernelbase.dll");
            if (mod) pfn = (PFN_SetThreadDescription)GetProcAddress(mod, "SetThreadDescription");
        }
        resolved = true;
    }
    if (!pfn) return;

    int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
    if (len <= 0 || len > 128) return;
    wchar_t wname[128];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 128) == 0) return;

    HANDLE hThread = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, GetCurrentThreadId());
    if (hThread) {
        pfn(hThread, wname);
        CloseHandle(hThread);
        return;
    }

    hThread = NULL;
    if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                          GetCurrentProcess(), &hThread, 0, FALSE,
                          DUPLICATE_SAME_ACCESS) && hThread) {
        pfn(hThread, wname);
        CloseHandle(hThread);
    }
}

/**
 * @brief Sets the name of the current thread (visible in debuggers and Task Manager).
 * @details MinGW winpthreads names threads "POSIX WinThreads for Windows" at CRT startup.
 *          Order: pthread cache → OS description → pthread again (winpthreads may touch both).
 */
static void _SituationSetCurrentThreadName(const char* name) {
    if (!name || !name[0]) return;

#if !defined(__STDC_NO_THREADS__)
    pthread_setname_np(pthread_self(), name);
#endif

    _SituationWin32SetThreadDescriptionUtf8(name);

#if !defined(__STDC_NO_THREADS__)
    pthread_setname_np(pthread_self(), name);
#endif
}
#elif defined(__linux__)
#include <pthread.h>
static void _SituationSetCurrentThreadName(const char* name) {
    if (!name || !name[0]) return;
    pthread_setname_np(pthread_self(), name);
}
#elif defined(__APPLE__)
#include <pthread.h>
static void _SituationSetCurrentThreadName(const char* name) {
    if (!name || !name[0]) return;
    pthread_setname_np(name);
}
#else
static void _SituationSetCurrentThreadName(const char* name) { (void)name; }
#endif

/* main_thread_name → window_title → SITUATION_MAIN_THREAD_NAME_DEFAULT */
static const char* _SituationResolveMainThreadOsName(const char* main_thread_name, const char* window_title) {
    if (main_thread_name && main_thread_name[0]) return main_thread_name;
    if (window_title && window_title[0]) return window_title;
    return SITUATION_MAIN_THREAD_NAME_DEFAULT;
}

/* Copy name into dest; NULL/empty uses SITUATION_MAIN_THREAD_NAME_DEFAULT. */
static void _SituationCopyThreadName(char* dest, size_t dest_sz, const char* name) {
    if (!dest || dest_sz == 0) return;
    const char* src = (name && name[0]) ? name : SITUATION_MAIN_THREAD_NAME_DEFAULT;
    strncpy(dest, src, dest_sz - 1);
    dest[dest_sz - 1] = '\0';
}

SITAPI void SituationSetCurrentThreadName(const char* name) {
    if (!name || !name[0]) return;
    _SituationSetCurrentThreadName(name);
    if (_sit_current_context != NULL) {
#if defined(SITUATION_ENABLE_THREADING)
        if (sit_gs_thread_id_set && thrd_equal(thrd_current(), sit_gs_main_thread_id)) {
            _SituationCopyThreadName(sit_gs.main_thread_name, sizeof(sit_gs.main_thread_name), name);
        }
#endif
    }
}

#endif // SITUATION_IMPL_THREADING_DIAG_H
