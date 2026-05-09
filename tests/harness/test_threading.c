/**
 * @file test_threading.c
 * @brief Threading module tests — Thread pool, Jobs, Dependencies, Parallel dispatch
 *
 * Context-free: does NOT require SituationInit() (only the thread pool subsystem).
 * Requires SITUATION_ENABLE_THREADING to be defined at compile time.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"

#ifdef SITUATION_ENABLE_THREADING

// ============================================================================
//  Shared state for job tests
// ============================================================================

static SituationThreadPool g_test_pool;
static bool g_pool_created = false;

static void threading_setup(void) {
    memset(&g_test_pool, 0, sizeof(g_test_pool));
    // Create pool with 4 threads, queue size 256, no hot-reload, no IO thread
    bool ok = SituationCreateThreadPool(&g_test_pool, 4, 256, 0.0, true);
    g_pool_created = ok;
}

static void threading_teardown(void) {
    if (g_pool_created) {
        SituationDestroyThreadPool(&g_test_pool);
        g_pool_created = false;
    }
}

// ============================================================================
//  Pool Lifecycle Tests
// ============================================================================

static void test_pool_create(void) {
    SIT_ASSERT(g_pool_created);
    SIT_ASSERT(g_test_pool.is_active);
    SIT_ASSERT_EQ(g_test_pool.thread_count, 4);
}

// ============================================================================
//  Job Submission Tests
// ============================================================================

static volatile int g_counter = 0;

static void increment_job(void* data, void* ctx) {
    (void)ctx;
    int* counter = (int*)data;
    (*counter)++;
}

static void test_submit_and_wait(void) {
    int counter = 0;
    SituationJobId job = SituationSubmitJobEx(
        &g_test_pool, increment_job, &counter, sizeof(int*),
        SIT_SUBMIT_HIGH_PRIORITY
    );
    SIT_ASSERT_NEQ(job, 0);

    bool done = SituationWaitForJob(&g_test_pool, job);
    SIT_ASSERT(done);
    // Note: The job receives a COPY of the pointer data via SOO,
    // so we need to use a different approach for verification.
    // Let's use a pointer-only submission instead.
}

static volatile int g_simple_counter = 0;

static void simple_increment(void* payload, void* ctx) {
    (void)payload;
    (void)ctx;
    g_simple_counter++;
}

static void test_submit_pointer_only(void) {
    g_simple_counter = 0;

    SituationJobId job = SituationSubmitJobEx(
        &g_test_pool, simple_increment, NULL, 0,
        SIT_SUBMIT_HIGH_PRIORITY
    );
    SIT_ASSERT_NEQ(job, 0);

    SituationWaitForJob(&g_test_pool, job);
    SIT_ASSERT_EQ(g_simple_counter, 1);
}

static void test_wait_for_all(void) {
    g_simple_counter = 0;

    for (int i = 0; i < 10; i++) {
        SituationSubmitJobEx(
            &g_test_pool, simple_increment, NULL, 0,
            SIT_SUBMIT_DEFAULT
        );
    }

    SituationWaitForAllJobs(&g_test_pool);
    SIT_ASSERT_EQ(g_simple_counter, 10);
}

// ============================================================================
//  Parallel Dispatch Tests
// ============================================================================

static int g_parallel_results[100];

static void parallel_set_index(int index, void* user_data) {
    (void)user_data;
    g_parallel_results[index] = index * 2;
}

static void test_dispatch_parallel(void) {
    memset(g_parallel_results, 0, sizeof(g_parallel_results));

    SituationDispatchParallel(&g_test_pool, 100, 10, parallel_set_index, NULL);

    // Verify all items were processed
    for (int i = 0; i < 100; i++) {
        SIT_ASSERT_EQ(g_parallel_results[i], i * 2);
    }
}

// ============================================================================
//  Dependency Tests
// ============================================================================

static volatile int g_dep_order[2];
static volatile int g_dep_index = 0;

static void dep_job_a(void* payload, void* ctx) {
    (void)payload; (void)ctx;
    // Small delay to make ordering visible
    for (volatile int i = 0; i < 100000; i++) {}
    g_dep_order[g_dep_index++] = 1; // A = 1
}

static void dep_job_b(void* payload, void* ctx) {
    (void)payload; (void)ctx;
    g_dep_order[g_dep_index++] = 2; // B = 2
}

static void test_job_dependency(void) {
    g_dep_index = 0;
    g_dep_order[0] = 0;
    g_dep_order[1] = 0;

    // Submit A first
    SituationJobId job_a = SituationSubmitJobEx(
        &g_test_pool, dep_job_a, NULL, 0,
        SIT_SUBMIT_HIGH_PRIORITY
    );
    SIT_ASSERT_NEQ(job_a, 0);

    // Submit B
    SituationJobId job_b = SituationSubmitJobEx(
        &g_test_pool, dep_job_b, NULL, 0,
        SIT_SUBMIT_HIGH_PRIORITY
    );
    SIT_ASSERT_NEQ(job_b, 0);

    // Add dependency: B depends on A
    bool ok = SituationAddJobDependency(&g_test_pool, job_a, job_b);
    SIT_ASSERT(ok);

    // Wait for both
    SituationWaitForAllJobs(&g_test_pool);

    // A should have run before B
    SIT_ASSERT_EQ(g_dep_order[0], 1); // A first
    SIT_ASSERT_EQ(g_dep_order[1], 2); // B second
}

// ============================================================================
//  Diagnostics Tests
// ============================================================================

static void test_dump_task_graph(void) {
    // Just verify it doesn't crash — output goes to stderr
    SituationDumpTaskGraph(&g_test_pool, stderr, false);
    SIT_ASSERT(true); // If we got here, no crash
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase threading_tests[] = {
    {"pool_create",          test_pool_create,          false},
    {"submit_and_wait",      test_submit_and_wait,      false},
    {"submit_pointer_only",  test_submit_pointer_only,  false},
    {"wait_for_all",         test_wait_for_all,         false},
    {"dispatch_parallel",    test_dispatch_parallel,    false},
    {"job_dependency",       test_job_dependency,       false},
    {"dump_task_graph",      test_dump_task_graph,      false},
};

const SitTestModule g_module_threading = {
    .name = "threading",
    .setup = threading_setup,
    .teardown = threading_teardown,
    .tests = threading_tests,
    .test_count = sizeof(threading_tests) / sizeof(threading_tests[0]),
    .requires_context = false
};

#else // !SITUATION_ENABLE_THREADING

// Stub module when threading is disabled
static SitTestCase threading_tests[] = {0};

const SitTestModule g_module_threading = {
    .name = "threading",
    .setup = NULL,
    .teardown = NULL,
    .tests = threading_tests,
    .test_count = 0,
    .requires_context = false
};

#endif // SITUATION_ENABLE_THREADING
