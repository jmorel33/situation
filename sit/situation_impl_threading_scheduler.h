/*
 * situation_impl_threading_scheduler.h - Scheduler metrics & sizing (Epic D)
 * (c) 2025-2026 Jacques Morel
 * MIT Licensed
 *
 * Included from situation_impl_threading.h (after observability).
 */

#ifndef SITUATION_IMPL_THREADING_SCHEDULER_H
#define SITUATION_IMPL_THREADING_SCHEDULER_H

#include <stdio.h>
#include <string.h>

#define SIT_WORKER_SCAN_DEPTH_MIN 4
#define SIT_WORKER_SCAN_DEPTH_MAX 32

static size_t _SitWorkerScanDepthForPending(size_t pending) {
    if (pending <= SIT_WORKER_SCAN_DEPTH_MIN) {
        return pending > 0 ? pending : 1;
    }
    if (pending >= 64) {
        return SIT_WORKER_SCAN_DEPTH_MAX;
    }
    return pending / 2;
}

static size_t _SitResolveAutoWorkerCount(void) {
    uint32_t reserved = 4; // Main + Render + Audio + I/O
    bool use_physical = false;

    if (_sit_current_context != NULL) {
        reserved = sit_gs.thread_pool_reserved_threads;
        if (reserved == 0) {
            reserved = 4; // Default: reserve for main, render, audio, I/O
        }
        use_physical = sit_gs.thread_pool_use_physical_cores;
    }

    uint32_t cores = use_physical ? SituationGetCPUCoreCount() : SituationGetCPUThreadCount();
    if (cores <= reserved) {
        return 1;
    }
    return (size_t)(cores - reserved);
}

SITAPI uint32_t SituationGetRecommendedWorkerCount(uint32_t reserved_threads, bool use_physical_cores) {
    if (reserved_threads == 0) {
        reserved_threads = 4;
    }
    uint32_t cores = use_physical_cores ? SituationGetCPUCoreCount() : SituationGetCPUThreadCount();
    if (cores <= reserved_threads) {
        return 1;
    }
    return cores - reserved_threads;
}

static void _SitPoolMetricsFromPool(SituationThreadPool* pool, SituationThreadPoolMetrics* out) {
    memset(out, 0, sizeof(*out));
    if (!pool) {
        return;
    }

    out->jobs_submitted = atomic_load(&pool->stats_jobs_submitted);
    out->jobs_completed = atomic_load(&pool->stats_jobs_completed);
    out->main_steal_success = atomic_load(&pool->stats_main_steal_success);
    out->main_steal_fail = atomic_load(&pool->stats_main_steal_fail);
    out->main_steal_empty_queue = atomic_load(&pool->stats_main_steal_empty_queue);
    out->high_queue_lock_ops = atomic_load(&pool->stats_high_queue_lock_ops);
    out->high_queue_lock_ns = atomic_load(&pool->stats_high_queue_lock_ns);
    out->scan_forward_swap = atomic_load(&pool->stats_scan_forward_swap);
    out->scan_forward_exhausted = atomic_load(&pool->stats_scan_forward_exhausted);
    out->io_idle_waits = atomic_load(&pool->stats_io_idle_waits);
    out->io_jobs_run = atomic_load(&pool->stats_io_jobs_run);
    out->submit_run_inline = atomic_load(&pool->stats_submit_run_inline);
    out->queue_full_spins = atomic_load(&pool->stats_queue_full_spins);
    out->dispatch_parallel_calls = atomic_load(&pool->stats_dispatch_parallel_calls);

    uint64_t io_total = out->io_idle_waits + out->io_jobs_run;
    if (io_total > 0) {
        out->io_busy_ratio = (double)out->io_jobs_run / (double)io_total;
    }
}

SITAPI bool SituationGetThreadPoolMetrics(SituationThreadPool* pool, SituationThreadPoolMetrics* out_metrics) {
    if (!out_metrics) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetThreadPoolMetrics: out_metrics is NULL");
        return false;
    }
    if (!pool || !pool->is_active) {
        _SitPoolMetricsFromPool(NULL, out_metrics);
        return false;
    }
    _SitPoolMetricsFromPool(pool, out_metrics);
    return true;
}

SITAPI void SituationResetThreadPoolStats(SituationThreadPool* pool) {
    if (!pool) {
        return;
    }
    atomic_store(&pool->stats_jobs_submitted, 0);
    atomic_store(&pool->stats_jobs_completed, 0);
    atomic_store(&pool->stats_main_steal_success, 0);
    atomic_store(&pool->stats_main_steal_fail, 0);
    atomic_store(&pool->stats_main_steal_empty_queue, 0);
    atomic_store(&pool->stats_high_queue_lock_ops, 0);
    atomic_store(&pool->stats_high_queue_lock_ns, 0);
    atomic_store(&pool->stats_scan_forward_swap, 0);
    atomic_store(&pool->stats_scan_forward_exhausted, 0);
    atomic_store(&pool->stats_io_idle_waits, 0);
    atomic_store(&pool->stats_io_jobs_run, 0);
    atomic_store(&pool->stats_submit_run_inline, 0);
    atomic_store(&pool->stats_queue_full_spins, 0);
    atomic_store(&pool->stats_dispatch_parallel_calls, 0);
}

SITAPI void SituationDumpThreadPoolMetrics(SituationThreadPool* pool, FILE* out, bool json_mode) {
    if (!out) {
        out = stderr;
    }

    SituationThreadPoolMetrics m;
    SituationGetThreadPoolMetrics(pool, &m);

    if (json_mode) {
        fprintf(out,
            "{\"submitted\":%llu,\"completed\":%llu,"
            "\"steal_ok\":%llu,\"steal_fail\":%llu,\"steal_empty\":%llu,"
            "\"high_lock_ops\":%llu,\"high_lock_ns\":%llu,"
            "\"scan_swap\":%llu,\"scan_exhausted\":%llu,"
            "\"io_idle\":%llu,\"io_jobs\":%llu,\"io_busy_ratio\":%.4f,"
            "\"run_inline\":%llu,\"queue_full_spins\":%llu,\"dispatch_parallel\":%llu}\n",
            (unsigned long long)m.jobs_submitted,
            (unsigned long long)m.jobs_completed,
            (unsigned long long)m.main_steal_success,
            (unsigned long long)m.main_steal_fail,
            (unsigned long long)m.main_steal_empty_queue,
            (unsigned long long)m.high_queue_lock_ops,
            (unsigned long long)m.high_queue_lock_ns,
            (unsigned long long)m.scan_forward_swap,
            (unsigned long long)m.scan_forward_exhausted,
            (unsigned long long)m.io_idle_waits,
            (unsigned long long)m.io_jobs_run,
            m.io_busy_ratio,
            (unsigned long long)m.submit_run_inline,
            (unsigned long long)m.queue_full_spins,
            (unsigned long long)m.dispatch_parallel_calls);
    } else {
        fprintf(out, "\n=== Thread Pool Scheduler Metrics ===\n");
        fprintf(out, "Jobs: submitted=%llu completed=%llu\n",
            (unsigned long long)m.jobs_submitted, (unsigned long long)m.jobs_completed);
        fprintf(out, "Main steal: ok=%llu fail=%llu empty=%llu\n",
            (unsigned long long)m.main_steal_success,
            (unsigned long long)m.main_steal_fail,
            (unsigned long long)m.main_steal_empty_queue);
        fprintf(out, "High queue lock: ops=%llu total_ns=%llu\n",
            (unsigned long long)m.high_queue_lock_ops,
            (unsigned long long)m.high_queue_lock_ns);
        fprintf(out, "Scan-forward: swap=%llu exhausted=%llu\n",
            (unsigned long long)m.scan_forward_swap,
            (unsigned long long)m.scan_forward_exhausted);
        fprintf(out, "I/O thread: jobs=%llu idle_waits=%llu busy_ratio=%.2f%%\n",
            (unsigned long long)m.io_jobs_run,
            (unsigned long long)m.io_idle_waits,
            m.io_busy_ratio * 100.0);
        fprintf(out, "Backpressure: run_inline=%llu queue_full_spins=%llu\n",
            (unsigned long long)m.submit_run_inline,
            (unsigned long long)m.queue_full_spins);
        fprintf(out, "DispatchParallel calls: %llu\n",
            (unsigned long long)m.dispatch_parallel_calls);
        fprintf(out, "=========================================\n\n");
    }
}

#endif /* SITUATION_IMPL_THREADING_SCHEDULER_H */
