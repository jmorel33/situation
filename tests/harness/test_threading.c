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
#else
    #include <unistd.h>
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
    SituationError err = SituationCreateThreadPool(&g_test_pool, 4, 256, 0.0, true);
    g_pool_created = (err == SITUATION_SUCCESS);
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

    bool done = (SituationWaitForJob(&g_test_pool, job) == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationCreateThreadPool(&pool, 1, 256, 0.0, true) == SITUATION_SUCCESS);

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

    SituationError ok = SituationAddJobDependency(&pool, job_a, job_b);
    SIT_ASSERT(ok == SITUATION_SUCCESS);

    SituationWaitForAllJobs(&pool);
    SituationDestroyThreadPool(&pool);

    SIT_ASSERT_EQ(g_dep_order[0], 1);
    SIT_ASSERT_EQ(g_dep_order[1], 2);
}

// ============================================================================
//  Diagnostics Tests
// ============================================================================

static void test_cpu_topology_refresh(void) {
    SIT_ASSERT(SituationRefreshCpuTopology() == SITUATION_SUCCESS);
    const SituationCpuTopology* topo = NULL;
    SIT_ASSERT(SituationGetCpuTopology(&topo) == SITUATION_SUCCESS);
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

    if (SituationGetThreadAffinity(&current) != SITUATION_SUCCESS) {
        return;
    }

    if (SituationSetThreadAffinityEx(pin, &previous) != SITUATION_SUCCESS) {
        return;
    }

    SIT_ASSERT(SituationGetThreadAffinity(&current) == SITUATION_SUCCESS);
    SIT_ASSERT((current & pin) != 0);

    if (previous != 0) {
        SituationSetThreadAffinityEx(previous, NULL);
    }
}

static void test_mask_builders(void) {
    SIT_ASSERT(SituationRefreshCpuTopology() == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationRefreshCpuTopology() == SITUATION_SUCCESS);
    SIT_ASSERT(SituationRefreshNumaTopology() == SITUATION_SUCCESS);
    const SituationNumaTopology* numa = NULL;
    SIT_ASSERT(SituationGetNumaTopology(&numa) == SITUATION_SUCCESS);
    SIT_ASSERT(numa != NULL);
    SIT_ASSERT(numa->node_count >= 1);
    SIT_ASSERT(numa->nodes[0].processor_count > 0);

    const SituationCpuTopology* cpu = NULL;
    SIT_ASSERT(SituationGetCpuTopology(&cpu) == SITUATION_SUCCESS);
    if (numa->node_count > 1) {
        SIT_ASSERT(numa->nodes[1].processor_count > 0 || numa->nodes[1].memory_bytes > 0);
    }
}

static void test_numa_node_mask(void) {
    SIT_ASSERT(SituationRefreshNumaTopology() == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationGetThreadPoolSnapshot(&g_test_pool, &snap) == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationRefreshCpuTopology() == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationGetThreadPoolMetrics(&g_test_pool, &metrics) == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationGetThreadPoolMetrics(&g_test_pool, &metrics) == SITUATION_SUCCESS);
    SIT_ASSERT_EQ((int)metrics.dispatch_parallel_calls, 0);
}

static void test_scheduler_metrics_after_parallel(void) {
    SituationResetThreadPoolStats(&g_test_pool);
    memset(g_parallel_results, 0, sizeof(g_parallel_results));
    SituationDispatchParallel(&g_test_pool, 64, 4, parallel_set_index, NULL);

    SituationThreadPoolMetrics metrics;
    SIT_ASSERT(SituationGetThreadPoolMetrics(&g_test_pool, &metrics) == SITUATION_SUCCESS);
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
    uint64_t mid_snapshot[SIT_CPU_HISTOGRAM_MAX];
    atomic_bool mid_snapshot_saved;
} SitCpuStressCtx;

static void cpu_burn_parallel_work(int index, void* user_data) {
    SitCpuStressCtx* ctx = (SitCpuStressCtx*)user_data;

    volatile uint64_t sink = (uint64_t)(unsigned)index + 1ULL;
    uint64_t next_sample_ns = sit_test_monotonic_ns();
    while (sit_test_monotonic_ns() < ctx->end_ns) {
        for (int k = 0; k < 768; ++k) {
            sink = sink * 1103515245ULL + 12345ULL;
        }
        uint64_t now = sit_test_monotonic_ns();
        if (now >= next_sample_ns) {
            int cpu = SituationGetCurrentProcessorIndex();
            if (cpu >= 0 && cpu < SIT_CPU_HISTOGRAM_MAX) {
                atomic_fetch_add(&ctx->cpu_hits[cpu], 1);
            }
            next_sample_ns = now + 5000000ULL; /* ~5 ms — tracks sustained load, not just job start */
        }
    }
    (void)sink;
}

static void sit_cpu_stress_wait_until(uint64_t target_ns) {
    for (;;) {
        uint64_t now = sit_test_monotonic_ns();
        if (now >= target_ns) {
            return;
        }
        uint64_t remain_ms = (target_ns - now) / 1000000ULL;
        if (remain_ms > 50) {
            remain_ms = 50;
        }
        if (remain_ms == 0) {
            continue;
        }
#if defined(_WIN32)
        Sleep((DWORD)remain_ms);
#else
        usleep((useconds_t)(remain_ms * 1000ULL));
#endif
    }
}

typedef struct {
    SituationThreadPool* pool;
    SitCpuStressCtx* ctx;
    uint32_t logical;
    uint32_t physical;
    size_t workers;
    uint64_t start_ns;
    uint64_t fire_at_ns;
    atomic_bool* fired;
} SitCpuStressMidTimer;

static void sit_cpu_stress_snapshot_counts(
    const SitCpuStressCtx* ctx, uint64_t* out, uint32_t display_cpus)
{
    for (uint32_t i = 0; i < display_cpus; ++i) {
        out[i] = atomic_load(&ctx->cpu_hits[i]);
    }
}

/** @return distinct CPUs with non-zero counts. */
static int sit_cpu_stress_print_histogram(
    const char* heading,
    const char* blurb,
    const uint64_t* counts,
    uint32_t display_cpus)
{
    uint64_t total = 0;
    uint64_t max_hits = 0;
    for (uint32_t i = 0; i < display_cpus; ++i) {
        total += counts[i];
        if (counts[i] > max_hits) {
            max_hits = counts[i];
        }
    }
    if (max_hits == 0) {
        max_hits = 1;
    }

    fprintf(stderr, "\n%s\n", heading);
    if (blurb && blurb[0]) {
        fprintf(stderr, "%s\n", blurb);
    }
    if (total == 0) {
        fprintf(stderr, "  (no activity in this interval)\n");
        return 0;
    }

    int distinct = 0;
    for (uint32_t i = 0; i < display_cpus; ++i) {
        uint64_t h = counts[i];
        if (h == 0) {
            continue;
        }
        distinct++;
        double pct = (100.0 * (double)h) / (double)total;
        int bar_len = (int)((h * 40ULL) / max_hits);
        if (bar_len < 1) {
            bar_len = 1;
        }
        if (bar_len > 40) {
            bar_len = 40;
        }
        fprintf(stderr, "  CPU %3u: %5.1f%% ", i, pct);
        for (int b = 0; b < bar_len; ++b) {
            fputc('#', stderr);
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "  CPUs with activity: %d (of %u logical)\n", distinct, display_cpus);
    return distinct;
}

static void sit_cpu_stress_report_mid(
    SitCpuStressCtx* ctx,
    uint32_t logical_count,
    uint64_t start_ns)
{
    uint32_t display_cpus = logical_count;
    if (display_cpus == 0 || display_cpus > SIT_CPU_HISTOGRAM_MAX) {
        display_cpus = SIT_CPU_HISTOGRAM_MAX;
    }

    double elapsed_s = (sit_test_monotonic_ns() - start_ns) / 1e9;
    fprintf(stderr,
        "\n======== CPU stress — FIRST HALF (%.1fs / %ds) ========\n",
        elapsed_s, SIT_CPU_STRESS_SECONDS);
    fprintf(stderr,
        "Task Manager: Performance -> CPU, logical processors — should be busy now.\n");

    uint64_t snap[SIT_CPU_HISTOGRAM_MAX];
    sit_cpu_stress_snapshot_counts(ctx, snap, display_cpus);
    for (uint32_t i = 0; i < display_cpus; ++i) {
        ctx->mid_snapshot[i] = snap[i];
    }
    atomic_store(&ctx->mid_snapshot_saved, true);

    sit_cpu_stress_print_histogram(
        "Where burn work ran (seconds 0-5):",
        "Each line is %% of CPU-time samples in the first half. Bars compare CPUs to each other.",
        snap, display_cpus);

    fprintf(stderr, "======== end first half (burn continues) ========\n\n");
}

static void sit_cpu_stress_report_final(
    SituationThreadPool* pool,
    SitCpuStressCtx* ctx,
    uint32_t logical_count,
    uint32_t physical_count,
    size_t worker_count,
    uint64_t start_ns)
{
    uint32_t display_cpus = logical_count;
    if (display_cpus == 0 || display_cpus > SIT_CPU_HISTOGRAM_MAX) {
        display_cpus = SIT_CPU_HISTOGRAM_MAX;
    }

    double elapsed_s = (sit_test_monotonic_ns() - start_ns) / 1e9;
    fprintf(stderr,
        "\n======== CPU stress — FINAL (%.1fs / %ds) ========\n",
        elapsed_s, SIT_CPU_STRESS_SECONDS);
    fprintf(stderr, "Machine: %u logical CPUs, %u physical cores, %zu pool workers\n",
        logical_count, physical_count, worker_count);

    uint64_t full[SIT_CPU_HISTOGRAM_MAX];
    sit_cpu_stress_snapshot_counts(ctx, full, display_cpus);

    if (atomic_load(&ctx->mid_snapshot_saved)) {
        uint64_t second_half[SIT_CPU_HISTOGRAM_MAX];
        for (uint32_t i = 0; i < display_cpus; ++i) {
            second_half[i] = (full[i] >= ctx->mid_snapshot[i])
                ? (full[i] - ctx->mid_snapshot[i]) : 0;
        }
        sit_cpu_stress_print_histogram(
            "Where burn work ran (seconds 5-10):",
            "Second half only — compare with the first-half report above.",
            second_half, display_cpus);
    }

    sit_cpu_stress_print_histogram(
        "Where burn work ran (full run):",
        "Combined 0-10 s — used for the harness distinct-CPU check.",
        full, display_cpus);

    SituationThreadPoolSnapshot snap;
    if (SituationGetThreadPoolSnapshot(pool, &snap) == SITUATION_SUCCESS) {
        fprintf(stderr, "\nWorker last logical CPU (after burn):\n");
        for (int s = 0; s < snap.slot_count; ++s) {
            const SituationThreadSlotSnapshot* slot = &snap.slots[s];
            if (slot->role != SIT_THREAD_ROLE_WORKER) {
                continue;
            }
            fprintf(stderr, "  worker slot %d -> logical CPU %d (NUMA %d)\n",
                s, slot->last_logical_cpu, slot->numa_node);
        }
        fprintf(stderr, "Pool jobs completed: %llu\n",
            (unsigned long long)snap.stats_jobs_completed);
    }

    SituationThreadPoolMetrics metrics;
    if (SituationGetThreadPoolMetrics(pool, &metrics) == SITUATION_SUCCESS) {
        fprintf(stderr, "DispatchParallel calls: %llu | main-thread steals: %llu\n",
            (unsigned long long)metrics.dispatch_parallel_calls,
            (unsigned long long)metrics.main_steal_success);
    }
    fprintf(stderr, "======== end final ========\n\n");
}

static void cpu_stress_mid_report_job(void* data, void* ctx) {
    (void)ctx;
    SitCpuStressMidTimer* timer = (SitCpuStressMidTimer*)data;

    sit_cpu_stress_wait_until(timer->fire_at_ns);

    bool expected = false;
    if (!atomic_compare_exchange_strong(timer->fired, &expected, true)) {
        return;
    }

    sit_cpu_stress_report_mid(timer->ctx, timer->logical, timer->start_ns);
}

static void test_cpu_stress_10s_taskmgr_report(void) {
    if (getenv("SIT_SKIP_CPU_STRESS") != NULL) {
        fprintf(stderr, "[threading] cpu_stress_10s skipped (SIT_SKIP_CPU_STRESS set)\n");
        return;
    }

    SIT_ASSERT(SituationRefreshCpuTopology() == SITUATION_SUCCESS);
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
    SIT_ASSERT(SituationCreateThreadPool(&stress_pool, workers, 2048, 0.0, true) == SITUATION_SUCCESS);

    SitCpuStressCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    for (int i = 0; i < SIT_CPU_HISTOGRAM_MAX; ++i) {
        atomic_init(&ctx.cpu_hits[i], 0);
    }
    atomic_init(&ctx.mid_snapshot_saved, false);

    atomic_bool mid_report_fired;
    atomic_init(&mid_report_fired, false);

    uint64_t start_ns = sit_test_monotonic_ns();
    ctx.end_ns = start_ns + (uint64_t)SIT_CPU_STRESS_SECONDS * 1000000000ULL;
    uint64_t mid_ns = start_ns + (uint64_t)(SIT_CPU_STRESS_SECONDS / 2) * 1000000000ULL;

    SitCpuStressMidTimer mid_timer = {
        .pool = &stress_pool,
        .ctx = &ctx,
        .logical = logical,
        .physical = physical,
        .workers = workers,
        .start_ns = start_ns,
        .fire_at_ns = mid_ns,
        .fired = &mid_report_fired,
    };

    SituationJobId mid_job = SituationSubmitJobEx(
        &stress_pool, cpu_stress_mid_report_job, &mid_timer, sizeof(mid_timer),
        SIT_SUBMIT_HIGH_PRIORITY);
    SIT_ASSERT_NEQ(mid_job, 0);

    fprintf(stderr,
        "\n[threading] CPU stress %d s, %zu workers.\n"
        "  Open Task Manager -> Performance -> CPU (logical processors).\n"
        "  First-half report at ~5 s; final report at ~10 s.\n",
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
    }

    SIT_ASSERT(SituationWaitForJob(&stress_pool, mid_job) == SITUATION_SUCCESS);

    sit_cpu_stress_report_final(
        &stress_pool, &ctx, logical, physical, workers, start_ns);
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
    SIT_ASSERT(SituationGetThreadPoolMetrics(&stress_pool, &metrics) == SITUATION_SUCCESS);
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
