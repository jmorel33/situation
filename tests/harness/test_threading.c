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

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

#include <stdatomic.h>
#include <time.h>

#define SIT_CPU_STRESS_SECONDS 10
#define SIT_CPU_HISTOGRAM_MAX  256

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
static volatile int g_dep_order_idx = 0;

static void dep_job_a(void* payload, void* ctx) {
    (void)payload; (void)ctx;
    for (volatile int i = 0; i < 100000; i++) {}
    g_dep_order[g_dep_order_idx++] = 1;
}

static void dep_job_b(void* payload, void* ctx) {
    (void)payload; (void)ctx;
    g_dep_order[g_dep_order_idx++] = 2;
}

static void test_job_dependency(void) {
    /* One worker: strict FIFO execution so we can wire dependencies after submit without races
       (see situation_impl_threading.h — graph edges must exist before dependents run). */
    SituationThreadPool pool;
    memset(&pool, 0, sizeof(pool));
    SIT_ASSERT(SituationCreateThreadPool(&pool, 1, 256, 0.0, true));

    g_dep_order_idx = 0;
    g_dep_order[0] = 0;
    g_dep_order[1] = 0;

    SituationJobId job_a = SituationSubmitJobEx(
        &pool, dep_job_a, NULL, 0,
        SIT_SUBMIT_HIGH_PRIORITY
    );
    SIT_ASSERT_NEQ(job_a, 0);

    SituationJobId job_b = SituationSubmitJobEx(
        &pool, dep_job_b, NULL, 0,
        SIT_SUBMIT_HIGH_PRIORITY
    );
    SIT_ASSERT_NEQ(job_b, 0);

    bool ok = SituationAddJobDependency(&pool, job_a, job_b);
    SIT_ASSERT(ok);

    SituationWaitForAllJobs(&pool);
    SituationDestroyThreadPool(&pool);

    SIT_ASSERT_EQ(g_dep_order[0], 1);
    SIT_ASSERT_EQ(g_dep_order[1], 2);
}

// ============================================================================
//  Diagnostics Tests
// ============================================================================

static void test_cpu_topology_refresh(void) {
    SIT_ASSERT(SituationRefreshCpuTopology());
    const SituationCpuTopology* topo = NULL;
    SIT_ASSERT(SituationGetCpuTopology(&topo));
    SIT_ASSERT(topo != NULL);
    SIT_ASSERT(topo->logical_count > 0);
    SIT_ASSERT(topo->physical_count > 0);
    SIT_ASSERT(topo->logical_count == SituationGetCPUThreadCount());
    SIT_ASSERT(topo->physical_count <= topo->logical_count);
    SIT_ASSERT(topo->numa_node_count >= 1);
}

static void test_affinity_roundtrip(void) {
    uint64_t previous = 0;
    uint64_t current = 0;
    uint64_t pin = 1ULL;

    if (!SituationGetThreadAffinity(&current)) {
        return;
    }

    if (!SituationSetThreadAffinityEx(pin, &previous)) {
        return;
    }

    SIT_ASSERT(SituationGetThreadAffinity(&current));
    SIT_ASSERT((current & pin) != 0);

    if (previous != 0) {
        SituationSetThreadAffinityEx(previous, NULL);
    }
}

static void test_mask_builders(void) {
    SIT_ASSERT(SituationRefreshCpuTopology());
    uint64_t core0 = SituationBuildPhysicalCoreMask(0);
    SIT_ASSERT(core0 != 0);

    uint64_t unique = SituationBuildUniqueCoreMask(0, 2, true);
    SIT_ASSERT(unique != 0);

    uint64_t numa0 = SituationBuildNumaNodeMask(0);
    SIT_ASSERT(numa0 != 0);
}

static void test_configured_affinity_defaults(void) {
    SIT_ASSERT_EQ(SituationGetConfiguredRenderThreadAffinity(), (uint64_t)(1ULL << 1));
    SIT_ASSERT_EQ(SituationGetConfiguredAudioThreadAffinity(), (uint64_t)(1ULL << 2));
}

static void test_threading_status_export(void) {
    SituationThreadingStatus st = SituationGetThreadingStatus();
    SIT_ASSERT(st.available);
    SIT_ASSERT(st.platform_topology_ok);
    SIT_ASSERT(st.max_threads > 0);
}

static void test_queue_depth_metrics(void) {
    g_simple_counter = 0;
    for (int i = 0; i < 5; ++i) {
        SituationSubmitJobEx(&g_test_pool, simple_increment, NULL, 0, SIT_SUBMIT_HIGH_PRIORITY);
    }
    SIT_ASSERT(SituationGetActiveJobCount(&g_test_pool) >= 0);
    SituationWaitForAllJobs(&g_test_pool);
    SIT_ASSERT_EQ(SituationGetHighQueueDepth(&g_test_pool), 0);
    SIT_ASSERT_EQ(SituationGetQueueDepth(&g_test_pool, SIT_JOB_QUEUE_BOTH), 0);
}

static void test_numa_topology_refresh(void) {
    SIT_ASSERT(SituationRefreshCpuTopology());
    SIT_ASSERT(SituationRefreshNumaTopology());
    const SituationNumaTopology* numa = NULL;
    SIT_ASSERT(SituationGetNumaTopology(&numa));
    SIT_ASSERT(numa != NULL);
    SIT_ASSERT(numa->node_count >= 1);
    SIT_ASSERT(numa->nodes[0].processor_count > 0);

    const SituationCpuTopology* cpu = NULL;
    SIT_ASSERT(SituationGetCpuTopology(&cpu));
    if (numa->node_count > 1) {
        SIT_ASSERT(numa->nodes[1].processor_count > 0 || numa->nodes[1].memory_bytes > 0);
    }
}

static void test_numa_node_mask(void) {
    SIT_ASSERT(SituationRefreshNumaTopology());
    uint64_t mask0 = SituationBuildNumaNodeMask(0);
    SIT_ASSERT(mask0 != 0);

    const SituationNumaTopology* numa = NULL;
    SituationGetNumaTopology(&numa);
    if (numa && numa->node_count > 0) {
        SIT_ASSERT((mask0 & numa->nodes[0].processor_mask_low) == numa->nodes[0].processor_mask_low);
    }
}

static void test_pool_snapshot_after_parallel(void) {
    memset(g_parallel_results, 0, sizeof(g_parallel_results));
    SituationDispatchParallel(&g_test_pool, 64, 4, parallel_set_index, NULL);

    SituationThreadPoolSnapshot snap;
    SIT_ASSERT(SituationGetThreadPoolSnapshot(&g_test_pool, &snap));
    SIT_ASSERT(snap.pool_active);
    SIT_ASSERT_EQ((int)snap.worker_count, 4);
    SIT_ASSERT(snap.stats_jobs_completed > 0);

    bool saw_worker = false;
    for (int i = 0; i < snap.slot_count; ++i) {
        if (snap.slots[i].role == SIT_THREAD_ROLE_WORKER) {
            saw_worker = true;
        }
    }
    SIT_ASSERT(saw_worker);
}

static void test_recommended_worker_count(void) {
    SIT_ASSERT(SituationRefreshCpuTopology());
    uint32_t logical = SituationGetCPUThreadCount();
    uint32_t physical = SituationGetCPUCoreCount();
    SIT_ASSERT(logical > 0);
    SIT_ASSERT(physical > 0);
    SIT_ASSERT(physical <= logical);

    uint32_t rec_logical = SituationGetRecommendedWorkerCount(1, false);
    uint32_t rec_physical = SituationGetRecommendedWorkerCount(1, true);
    SIT_ASSERT(rec_logical >= 1);
    SIT_ASSERT(rec_physical >= 1);
    if (logical > 1) {
        SIT_ASSERT(rec_logical == logical - 1);
    }
    if (physical > 1) {
        SIT_ASSERT(rec_physical == physical - 1);
    }
}

static void test_configured_main_affinity_default(void) {
    SIT_ASSERT_EQ(SituationGetConfiguredMainThreadAffinity(), (uint64_t)0);
}

static void test_metrics_reset_and_dump(void) {
    SituationResetThreadPoolStats(&g_test_pool);
    SituationDispatchParallel(&g_test_pool, 32, 4, parallel_set_index, NULL);

    SituationThreadPoolMetrics metrics;
    SIT_ASSERT(SituationGetThreadPoolMetrics(&g_test_pool, &metrics));
    SIT_ASSERT(metrics.jobs_completed > 0);

#if defined(_WIN32)
    FILE* sink = fopen("nul", "wb");
#else
    FILE* sink = fopen("/dev/null", "wb");
#endif
    if (sink) {
        SituationDumpThreadPoolMetrics(&g_test_pool, sink, false);
        SituationDumpThreadingReport(&g_test_pool, sink, false);
        fclose(sink);
    }

    SituationResetThreadPoolStats(&g_test_pool);
    SIT_ASSERT(SituationGetThreadPoolMetrics(&g_test_pool, &metrics));
    SIT_ASSERT_EQ((int)metrics.dispatch_parallel_calls, 0);
}

static void test_scheduler_metrics_after_parallel(void) {
    SituationResetThreadPoolStats(&g_test_pool);
    memset(g_parallel_results, 0, sizeof(g_parallel_results));
    SituationDispatchParallel(&g_test_pool, 64, 4, parallel_set_index, NULL);

    SituationThreadPoolMetrics metrics;
    SIT_ASSERT(SituationGetThreadPoolMetrics(&g_test_pool, &metrics));
    SIT_ASSERT(metrics.dispatch_parallel_calls >= 1);
    SIT_ASSERT(metrics.jobs_completed > 0);
    SIT_ASSERT(metrics.jobs_submitted >= metrics.jobs_completed);
}

// ============================================================================
//  10s all-core CPU stress (Task Manager correlation)
// ============================================================================

static uint64_t sit_test_monotonic_ns(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / (uint64_t)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

typedef struct {
    uint64_t end_ns;
    atomic_uint_least64_t cpu_hits[SIT_CPU_HISTOGRAM_MAX];
    bool mid_report_done;
} SitCpuStressCtx;

static void cpu_burn_parallel_work(int index, void* user_data) {
    SitCpuStressCtx* ctx = (SitCpuStressCtx*)user_data;
    int cpu = SituationGetCurrentProcessorIndex();
    if (cpu >= 0 && cpu < SIT_CPU_HISTOGRAM_MAX) {
        atomic_fetch_add(&ctx->cpu_hits[cpu], 1);
    }

    volatile uint64_t sink = (uint64_t)(unsigned)index + 1ULL;
    while (sit_test_monotonic_ns() < ctx->end_ns) {
        for (int k = 0; k < 768; ++k) {
            sink = sink * 1103515245ULL + 12345ULL;
        }
    }
    (void)sink;
}

static void sit_print_cpu_stress_report(
    SituationThreadPool* pool,
    const SitCpuStressCtx* ctx,
    uint32_t logical_count,
    uint32_t physical_count,
    size_t worker_count,
    const char* phase_label)
{
    fprintf(stderr, "\n--- CPU stress report (%s) ---\n", phase_label);
    fprintf(stderr, "Machine: %u logical CPUs, %u physical cores, %zu pool workers\n",
        logical_count, physical_count, worker_count);
    fprintf(stderr, "Task Manager: expect high %% on many logical CPUs during the %d s window.\n",
        SIT_CPU_STRESS_SECONDS);

    uint32_t display_cpus = logical_count;
    if (display_cpus == 0 || display_cpus > SIT_CPU_HISTOGRAM_MAX) {
        display_cpus = SIT_CPU_HISTOGRAM_MAX;
    }

    uint64_t max_hits = 1;
    for (uint32_t i = 0; i < display_cpus; ++i) {
        uint64_t h = atomic_load(&ctx->cpu_hits[i]);
        if (h > max_hits) {
            max_hits = h;
        }
    }

    int distinct = 0;
    fprintf(stderr, "\nLogical CPU histogram (work-item samples; bar ~ relative share):\n");
    for (uint32_t i = 0; i < display_cpus; ++i) {
        uint64_t h = atomic_load(&ctx->cpu_hits[i]);
        if (h == 0) {
            continue;
        }
        distinct++;
        int bar_len = (int)((h * 40ULL) / max_hits);
        if (bar_len < 1) {
            bar_len = 1;
        }
        fprintf(stderr, "  CPU %3u: ", i);
        for (int b = 0; b < bar_len; ++b) {
            fputc('#', stderr);
        }
        fprintf(stderr, " %llu\n", (unsigned long long)h);
    }
    fprintf(stderr, "Distinct logical CPUs with load samples: %d\n", distinct);

    SituationThreadPoolSnapshot snap;
    if (SituationGetThreadPoolSnapshot(pool, &snap)) {
        fprintf(stderr, "\nWorker last-seen logical CPU (library snapshot):\n");
        for (int s = 0; s < snap.slot_count; ++s) {
            const SituationThreadSlotSnapshot* slot = &snap.slots[s];
            if (slot->role != SIT_THREAD_ROLE_WORKER) {
                continue;
            }
            fprintf(stderr, "  worker slot %d -> logical CPU %d (NUMA %d)\n",
                s, slot->last_logical_cpu, slot->numa_node);
        }
        fprintf(stderr, "Jobs completed so far: %llu\n",
            (unsigned long long)snap.stats_jobs_completed);
    }

    SituationThreadPoolMetrics metrics;
    if (SituationGetThreadPoolMetrics(pool, &metrics)) {
        fprintf(stderr, "DispatchParallel calls: %llu | main steal ok: %llu\n",
            (unsigned long long)metrics.dispatch_parallel_calls,
            (unsigned long long)metrics.main_steal_success);
    }
    fprintf(stderr, "--- end report ---\n\n");
}

static void test_cpu_stress_10s_taskmgr_report(void) {
    if (getenv("SIT_SKIP_CPU_STRESS") != NULL) {
        fprintf(stderr, "[threading] cpu_stress_10s skipped (SIT_SKIP_CPU_STRESS set)\n");
        return;
    }

    SIT_ASSERT(SituationRefreshCpuTopology());
    uint32_t logical = SituationGetCPUThreadCount();
    uint32_t physical = SituationGetCPUCoreCount();
    SIT_ASSERT(logical > 0);
    SIT_ASSERT(physical > 0);

    size_t workers = (size_t)SituationGetRecommendedWorkerCount(1, false);
    if (workers < 2) {
        workers = 2;
    }
    if (workers > SITUATION_MAX_THREADS) {
        workers = SITUATION_MAX_THREADS;
    }

    SituationThreadPool stress_pool;
    memset(&stress_pool, 0, sizeof(stress_pool));
    SIT_ASSERT(SituationCreateThreadPool(&stress_pool, workers, 2048, 0.0, true));

    SitCpuStressCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    for (int i = 0; i < SIT_CPU_HISTOGRAM_MAX; ++i) {
        atomic_init(&ctx.cpu_hits[i], 0);
    }
    ctx.mid_report_done = false;

    uint64_t start_ns = sit_test_monotonic_ns();
    ctx.end_ns = start_ns + (uint64_t)SIT_CPU_STRESS_SECONDS * 1000000000ULL;
    uint64_t mid_ns = start_ns + (uint64_t)(SIT_CPU_STRESS_SECONDS / 2) * 1000000000ULL;

    fprintf(stderr,
        "\n[threading] CPU stress: %d s, %zu workers — open Task Manager (logical CPUs) now.\n",
        SIT_CPU_STRESS_SECONDS, workers);

    int parallel_width = (int)workers * 8;
    if (parallel_width < 16) {
        parallel_width = 16;
    }
    if (parallel_width > 512) {
        parallel_width = 512;
    }

    while (sit_test_monotonic_ns() < ctx.end_ns) {
        SituationDispatchParallel(&stress_pool, parallel_width, 1, cpu_burn_parallel_work, &ctx);
        if (!ctx.mid_report_done && sit_test_monotonic_ns() >= mid_ns) {
            sit_print_cpu_stress_report(&stress_pool, &ctx, logical, physical, workers, "mid (~5s)");
            ctx.mid_report_done = true;
        }
    }

    sit_print_cpu_stress_report(&stress_pool, &ctx, logical, physical, workers, "final");
    SituationDumpThreadPoolStatus(&stress_pool, stderr, false);

    int distinct = 0;
    uint32_t display_cpus = logical;
    if (display_cpus > SIT_CPU_HISTOGRAM_MAX) {
        display_cpus = SIT_CPU_HISTOGRAM_MAX;
    }
    for (uint32_t i = 0; i < display_cpus; ++i) {
        if (atomic_load(&ctx.cpu_hits[i]) > 0) {
            distinct++;
        }
    }

    uint32_t min_distinct = (uint32_t)workers;
    if (min_distinct > physical) {
        min_distinct = physical;
    }
    if (min_distinct > 8) {
        min_distinct = min_distinct / 2;
    }
    if (min_distinct < 2 && logical >= 2) {
        min_distinct = 2;
    }
    if (min_distinct < 1) {
        min_distinct = 1;
    }

    fprintf(stderr,
        "[threading] Distinct logical CPUs sampled: %d (expect >= %u for Task Manager correlation)\n",
        distinct, min_distinct);

    SIT_ASSERT(distinct >= (int)min_distinct);

    SituationThreadPoolMetrics metrics;
    SIT_ASSERT(SituationGetThreadPoolMetrics(&stress_pool, &metrics));
    SIT_ASSERT(metrics.jobs_completed > (uint64_t)workers);
    SIT_ASSERT(metrics.dispatch_parallel_calls >= 1);

    SituationDestroyThreadPool(&stress_pool);
}

static void test_dump_task_graph(void) {
    /* Exercise the dump path without cluttering harness stderr (Windows cp1252 consoles
       and CI logs). Open the platform null device; fall back to stderr if it fails. */
#if defined(_WIN32)
    FILE* sink = fopen("nul", "wb");
#else
    FILE* sink = fopen("/dev/null", "wb");
#endif
    if (sink) {
        SituationDumpTaskGraph(&g_test_pool, sink, false);
        fclose(sink);
    } else {
        SituationDumpTaskGraph(&g_test_pool, stderr, false);
    }
    SIT_ASSERT(true);
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
    {"cpu_topology_refresh", test_cpu_topology_refresh, false},
    {"affinity_roundtrip",   test_affinity_roundtrip,   false},
    {"mask_builders",        test_mask_builders,        false},
    {"configured_affinity",  test_configured_affinity_defaults, false},
    {"threading_status_export", test_threading_status_export, false},
    {"queue_depth_metrics",  test_queue_depth_metrics,      false},
    {"pool_snapshot_parallel", test_pool_snapshot_after_parallel, false},
    {"numa_topology_refresh",  test_numa_topology_refresh,      false},
    {"numa_node_mask",         test_numa_node_mask,             false},
    {"recommended_worker_count", test_recommended_worker_count, false},
    {"scheduler_metrics_parallel", test_scheduler_metrics_after_parallel, false},
    {"configured_main_affinity", test_configured_main_affinity_default, false},
    {"metrics_reset_and_dump", test_metrics_reset_and_dump, false},
    {"cpu_stress_10s_taskmgr", test_cpu_stress_10s_taskmgr_report, false},
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
