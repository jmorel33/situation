/***************************************************************************************************
*
*   examples/threading_diagnostic_test.c - Threading Diagnostics and Capability Test
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Demonstrates:
*   - Threading capability detection
*   - Error handling and recovery
*   - Debug logging (when enabled)
*   - Timeout protection
*   - Platform-specific sleep
*   
***************************************************************************************************/

#define SITUATION_USE_VULKAN
// Enable debug logging for this test
#define SITUATION_DEBUG_THREADING
#include "situation.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Device function table (required by node graph processing)
extern const SituationDeviceFunctions g_device_function_table[];
extern const int g_device_function_table_count;

#if !defined(__STDC_NO_THREADS__)
    #include <threads.h>
    #include <stdatomic.h>
    #define THREADS_AVAILABLE 1
#else
    #define THREADS_AVAILABLE 0
#endif

// ================================================================================================
// TEST 1: CAPABILITY DETECTION
// ================================================================================================

static bool test_capability_detection(void) {
    printf("\n========================================\n");
    printf("TEST 1: Capability Detection\n");
    printf("========================================\n\n");
    
    SituationThreadingStatus status = SituationGetThreadingStatus();
    SituationPrintThreadingStatus();
    
    if (!status.available) {
        printf("\n❌ FAIL: Threading not available\n");
        return false;
    }
    
    if (!status.sleep_reliable) {
        printf("\n⚠️  WARNING: Sleep implementation may be unreliable\n");
    }
    
    printf("\n✅ PASS: Threading capabilities detected\n");
    return true;
}

// ================================================================================================
// TEST 2: ERROR HANDLING
// ================================================================================================

#if THREADS_AVAILABLE

static bool test_error_handling(void) {
    printf("\n========================================\n");
    printf("TEST 2: Error Handling\n");
    printf("========================================\n\n");
    
    // Test 1: NULL graph
    printf("Test 2.1: NULL graph handling...\n");
    SituationNodeHandle handle;
    SituationError err = SituationCreateNodeThreadSafe(NULL, SITUATION_NODE_TONE_SYNTH, &handle);
    if (err != SITUATION_ERROR_NODE_INVALID_HANDLE) {
        printf("❌ FAIL: Expected INVALID_HANDLE error\n");
        return false;
    }
    printf("✅ PASS: NULL graph rejected\n\n");
    
    // Test 2: NULL handle
    printf("Test 2.2: NULL handle handling...\n");
    SituationInitDeviceRegistry();
    SituationThreadSafeGraph* graph = SituationCreateThreadSafeGraph();
    if (!graph) {
        printf("❌ FAIL: Failed to create graph\n");
        return false;
    }
    
    err = SituationCreateNodeThreadSafe(graph, SITUATION_NODE_TONE_SYNTH, NULL);
    if (err != SITUATION_NODE_ERR_INVALID_HANDLE) {
        printf("❌ FAIL: Expected INVALID_HANDLE error\n");
        SituationDestroyThreadSafeGraph(graph);
        return false;
    }
    printf("✅ PASS: NULL handle rejected\n\n");
    
    // Test 3: Valid node creation
    printf("Test 2.3: Valid node creation...\n");
    err = SituationCreateNodeThreadSafe(graph, SITUATION_NODE_TONE_SYNTH, &handle);
    if (err != SITUATION_SUCCESS) {
        printf("❌ FAIL: Node creation failed (error=%d)\n", err);
        SituationDestroyThreadSafeGraph(graph);
        return false;
    }
    printf("✅ PASS: Node created successfully (handle=0x%08X)\n\n", handle);
    
    SituationDestroyThreadSafeGraph(graph);
    
    printf("✅ PASS: All error handling tests passed\n");
    return true;
}

// ================================================================================================
// TEST 3: MUTEX TIMEOUT PROTECTION
// ================================================================================================

static mtx_t g_test_mutex;
static atomic_bool g_mutex_holder_running;

static int mutex_holder_thread(void* arg) {
    (void)arg;
    
    printf("  [Holder] Acquiring mutex...\n");
    mtx_lock(&g_test_mutex);
    printf("  [Holder] Mutex acquired, holding for 2 seconds...\n");
    
    SITUATION_SLEEP_MS(2000);  // Platform-specific sleep: Windows Sleep() avoids tinycthread hang bug
    
    printf("  [Holder] Releasing mutex...\n");
    mtx_unlock(&g_test_mutex);
    
    atomic_store(&g_mutex_holder_running, false);
    return 0;
}

static bool test_mutex_timeout(void) {
    printf("\n========================================\n");
    printf("TEST 3: Mutex Timeout Protection\n");
    printf("========================================\n\n");
    
    // Initialize mutex
    if (mtx_init(&g_test_mutex, mtx_plain) != thrd_success) {
        printf("❌ FAIL: Mutex initialization failed\n");
        return false;
    }
    
    // Start thread that holds mutex
    atomic_init(&g_mutex_holder_running, true);
    thrd_t holder;
    if (thrd_create(&holder, mutex_holder_thread, NULL) != thrd_success) {
        printf("❌ FAIL: Thread creation failed\n");
        mtx_destroy(&g_test_mutex);
        return false;
    }
    
    // Wait a bit for holder to acquire mutex
    SITUATION_SLEEP_MS(100);  // Platform-specific sleep (Windows Sleep(), not thrd_sleep())
    
    // Try to acquire with short timeout (should fail)
    printf("Test 3.1: Attempting lock with 500ms timeout (should fail)...\n");
    bool locked = SituationMutexTryLockTimeout(&g_test_mutex, 500);
    if (locked) {
        printf("❌ FAIL: Lock succeeded when it should have timed out\n");
        mtx_unlock(&g_test_mutex);
        thrd_join(holder, NULL);
        mtx_destroy(&g_test_mutex);
        return false;
    }
    printf("✅ PASS: Lock timed out as expected\n\n");
    
    // Wait for holder to release
    printf("Test 3.2: Waiting for holder to release...\n");
    while (atomic_load(&g_mutex_holder_running)) {
        SITUATION_SLEEP_MS(100);  // Platform-specific sleep (Windows Sleep(), not thrd_sleep())
    }
    
    // Try to acquire with timeout (should succeed)
    printf("Test 3.3: Attempting lock with 1000ms timeout (should succeed)...\n");
    locked = SituationMutexTryLockTimeout(&g_test_mutex, 1000);
    if (!locked) {
        printf("❌ FAIL: Lock failed when it should have succeeded\n");
        thrd_join(holder, NULL);
        mtx_destroy(&g_test_mutex);
        return false;
    }
    printf("✅ PASS: Lock succeeded\n\n");
    
    mtx_unlock(&g_test_mutex);
    thrd_join(holder, NULL);
    mtx_destroy(&g_test_mutex);
    
    printf("✅ PASS: Mutex timeout protection working\n");
    return true;
}

// ================================================================================================
// TEST 4: PLATFORM SLEEP
// ================================================================================================

static bool test_platform_sleep(void) {
    printf("\n========================================\n");
    printf("TEST 4: Platform-Specific Sleep\n");
    printf("========================================\n\n");
    
    #if SITUATION_SLEEP_AVAILABLE
        printf("Sleep implementation: ");
        #if defined(_WIN32)
            printf("Windows Sleep()\n");
        #elif defined(__unix__) || defined(__APPLE__)
            printf("POSIX usleep()\n");
        #else
            printf("C11 thrd_sleep()\n");
        #endif
        
        printf("Testing 100ms sleep...\n");
        SITUATION_SLEEP_MS(100);  // CRITICAL: Uses native Windows Sleep() on Windows to avoid tinycthread hang bug
        printf("✅ PASS: Sleep completed\n");
        return true;
    #else
        printf("❌ FAIL: Sleep not available\n");
        return false;
    #endif
}

// ================================================================================================
// TEST 5: ATOMIC OPERATIONS
// ================================================================================================

static bool test_atomic_operations(void) {
    printf("\n========================================\n");
    printf("TEST 5: Atomic Operations\n");
    printf("========================================\n\n");
    
    #if !defined(__STDC_NO_ATOMICS__)
        atomic_int counter;
        atomic_init(&counter, 0);
        
        printf("Test 5.1: Atomic load/store...\n");
        SITUATION_ATOMIC_STORE(counter, 42);
        int value = SITUATION_ATOMIC_LOAD(counter);
        if (value != 42) {
            printf("❌ FAIL: Expected 42, got %d\n", value);
            return false;
        }
        printf("✅ PASS: Load/store working\n\n");
        
        printf("Test 5.2: Atomic fetch-add...\n");
        SITUATION_ATOMIC_STORE(counter, 10);
        int old_value = SITUATION_ATOMIC_FETCH_ADD(counter, 5);
        int new_value = SITUATION_ATOMIC_LOAD(counter);
        if (old_value != 10 || new_value != 15) {
            printf("❌ FAIL: Expected old=10, new=15, got old=%d, new=%d\n", old_value, new_value);
            return false;
        }
        printf("✅ PASS: Fetch-add working\n\n");
        
        printf("Test 5.3: Atomic compare-exchange...\n");
        SITUATION_ATOMIC_STORE(counter, 100);
        int expected = 100;
        bool success = SITUATION_ATOMIC_COMPARE_EXCHANGE(counter, expected, 200);
        if (!success || SITUATION_ATOMIC_LOAD(counter) != 200) {
            printf("❌ FAIL: Compare-exchange failed\n");
            return false;
        }
        printf("✅ PASS: Compare-exchange working\n\n");
        
        printf("✅ PASS: All atomic operations working\n");
        return true;
    #else
        printf("⚠️  WARNING: Atomics not available (fallback mode)\n");
        return true;  // Not a failure, just a warning
    #endif
}

#endif // THREADS_AVAILABLE

// ================================================================================================
// MAIN
// ================================================================================================

int main(void) {
    printf("========================================\n");
    printf("Threading Diagnostics Test Suite\n");
    printf("========================================\n");
    
    int passed = 0;
    int total = 0;
    
    // Test 1: Capability Detection
    total++;
    if (test_capability_detection()) passed++;
    
#if THREADS_AVAILABLE
    // Test 2: Error Handling
    total++;
    if (test_error_handling()) passed++;
    
    // Test 3: Mutex Timeout
    total++;
    if (test_mutex_timeout()) passed++;
    
    // Test 4: Platform Sleep
    total++;
    if (test_platform_sleep()) passed++;
    
    // Test 5: Atomic Operations
    total++;
    if (test_atomic_operations()) passed++;
#else
    printf("\n⚠️  Threading not available - skipping tests 2-5\n");
#endif
    
    // Final Summary
    printf("\n========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Passed: %d / %d\n", passed, total);
    
    if (passed == total) {
        printf("\n✅ ALL TESTS PASSED\n");
        printf("========================================\n");
        return 0;
    } else {
        printf("\n❌ SOME TESTS FAILED\n");
        printf("========================================\n");
        return 1;
    }
}
