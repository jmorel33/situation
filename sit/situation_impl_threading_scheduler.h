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

/* Epic D in-place claim: SituationJobId encodes physical slot_idx. Never swap job structs
 * in the ring — swap breaks handles (WaitForJob, async compile_job, continuation_id).
 * Claim a ready slot with dependency_count 0 via CAS to SIT_JOB_DEP_CLAIM_BIT; real
 * dependency counts live in the low bits (SituationAddJobDependency fetch_add). */
#define SIT_JOB_DEP_CLAIM_BIT  0x40000000
#define SIT_JOB_DEP_COUNT_MASK 0x3FFFFFFF

static inline int _SitJobDepCount(int dep_raw) {
    return dep_raw & SIT_JOB_DEP_COUNT_MASK;
}

/* Continuation / fan-in must decrement only the low dependency bits and preserve
 * SIT_JOB_DEP_CLAIM_BIT. A blind fetch_sub(..., 1) on a claimed slot (0x40000000)
 * produces 0x3FFFFFFF in the low bits and wedges the worker in the pre-run dep wait
 * forever — compile_done never set, SituationPollShaderLoad hits -557 after 5s. */
static inline bool _SitJobDecrementDependency(SituationJob* job) {
    if (!job) return false;
    for (;;) {
        int old = atomic_load(&job->dependency_count);
        int claim = old & SIT_JOB_DEP_CLAIM_BIT;
        int low = _SitJobDepCount(old);
        if (low == 0) {
            return false;
        }
        int new_val = claim | (low - 1);
        if (atomic_compare_exchange_weak(&job->dependency_count, &old, new_val)) {
            return (low - 1) == 0 && claim == 0;
        }
    }
}

static inline void _SitQueueCompactTailLocked(SituationThreadPool* pool, int q_idx) {
    size_t head = atomic_load(&pool->queues[q_idx].head);
    size_t tail = atomic_load(&pool->queues[q_idx].tail);
    while (tail != head) {
        SituationJob* front = &pool->queues[q_idx].jobs[tail & pool->queues[q_idx].mask];
        if (!atomic_load(&front->is_completed)) {
            break;
        }
        tail++;
    }
    atomic_store(&pool->queues[q_idx].tail, tail);
}

/* Caller must hold queues[q_idx].lock. Returns true and sets *out_job when a job is claimed. */
static bool _SitWorkerTryClaimReadyJob(SituationThreadPool* pool, int q_idx, SituationJob** out_job) {
    *out_job = NULL;
    _SitQueueCompactTailLocked(pool, q_idx);

    size_t head = atomic_load(&pool->queues[q_idx].head);
    size_t tail = atomic_load(&pool->queues[q_idx].tail);
    if (tail == head) {
        return false;
    }

    /* Scan the entire pending window. An artificial scan cap (Epic D sizing) hid ready jobs
     * behind a long-running or blocked tail — classic HOL starvation: tail cannot compact
     * until the front slot completes, head keeps advancing, and anything beyond scan_limit
     * sat unclaimed until the blocker finished (async shader compile timeouts at 5s). */
    size_t pending = head - tail;

    for (size_t scan = 0; scan < pending; ++scan) {
        size_t idx = (tail + scan) & pool->queues[q_idx].mask;
        SituationJob* candidate = &pool->queues[q_idx].jobs[idx];

        if (atomic_load(&candidate->is_completed)) {
            continue;
        }
        int dep_raw = atomic_load(&candidate->dependency_count);
        if (dep_raw != 0) {
            continue; /* blocked or already claimed */
        }

        int expected = 0;
        if (!atomic_compare_exchange_strong(&candidate->dependency_count, &expected, SIT_JOB_DEP_CLAIM_BIT)) {
            continue;
        }

        if (scan > 0) {
            atomic_fetch_add(&pool->stats_scan_forward_swap, 1);
        }
        *out_job = candidate;
        return true;
    }
    return false;
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

SITAPI SituationError SituationGetThreadPoolMetrics(SituationThreadPool* pool, SituationThreadPoolMetrics* out_metrics) {
    if (!out_metrics) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetThreadPoolMetrics: out_metrics is NULL");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (!pool || !pool->is_active) {
        _SitPoolMetricsFromPool(NULL, out_metrics);
        return SITUATION_ERROR_INVALID_PARAM;
    }
    _SitPoolMetricsFromPool(pool, out_metrics);
    return SITUATION_SUCCESS;
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
