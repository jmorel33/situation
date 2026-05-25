/*
 * situation_impl_threading_observability.h - Thread pool observability (Epic B)
 * (c) 2025-2026 Jacques Morel
 * MIT Licensed
 *
 * Included from situation_impl_threading.h when SITUATION_ENABLE_THREADING is defined.
 */

#ifndef SITUATION_IMPL_THREADING_OBSERVABILITY_H
#define SITUATION_IMPL_THREADING_OBSERVABILITY_H

#include <stdio.h>
#include <string.h>

#define SIT_WORKER_CPU_SAMPLE_INTERVAL 8

typedef struct _SitSpecialThreadObs {
    atomic_int last_logical_cpu;
    atomic_uint_least64_t affinity_mask_requested;
    atomic_bool started;
} _SitSpecialThreadObs;

static _SitSpecialThreadObs g_sit_obs_render;
static _SitSpecialThreadObs g_sit_obs_audio;

static void _SitObsInitSpecialThreads(void) {
    static bool once = false;
    if (once) return;
    atomic_init(&g_sit_obs_render.last_logical_cpu, -1);
    atomic_init(&g_sit_obs_render.affinity_mask_requested, 0);
    atomic_init(&g_sit_obs_render.started, false);
    atomic_init(&g_sit_obs_audio.last_logical_cpu, -1);
    atomic_init(&g_sit_obs_audio.affinity_mask_requested, 0);
    atomic_init(&g_sit_obs_audio.started, false);
    once = true;
}

static void _SituationObservabilityRecordRenderThread(uint64_t affinity_mask) {
    _SitObsInitSpecialThreads();
    atomic_store(&g_sit_obs_render.affinity_mask_requested, affinity_mask);
    atomic_store(&g_sit_obs_render.started, true);
    int cpu = SituationGetCurrentProcessorIndex();
    atomic_store(&g_sit_obs_render.last_logical_cpu, cpu);
}

static void _SituationObservabilityRecordAudioThread(uint64_t affinity_mask) {
    _SitObsInitSpecialThreads();
    atomic_store(&g_sit_obs_audio.affinity_mask_requested, affinity_mask);
    atomic_store(&g_sit_obs_audio.started, true);
    int cpu = SituationGetCurrentProcessorIndex();
    atomic_store(&g_sit_obs_audio.last_logical_cpu, cpu);
}

static void _SitWorkerSampleCpu(SituationThreadPool* pool, size_t worker_index) {
    if (!pool || worker_index >= SITUATION_MAX_THREADS) return;
    int cpu = SituationGetCurrentProcessorIndex();
    atomic_store(&pool->worker_last_logical_cpu[worker_index], cpu);
}

static int _SitNumaForLogicalCpu(int logical_cpu) {
    if (logical_cpu < 0) return -1;
    const SituationCpuTopology* topo = NULL;
    if (!SituationGetCpuTopology(&topo) || !topo) return -1;
    if ((uint32_t)logical_cpu >= topo->logical_count) return -1;
    return (int)topo->processors[logical_cpu].numa_node;
}

static size_t _SitQueueDepthUnlocked(SituationThreadPool* pool, int q_idx) {
    size_t head = atomic_load(&pool->queues[q_idx].head);
    size_t tail = atomic_load(&pool->queues[q_idx].tail);
    return head - tail;
}

// ==================================================================================
// B1 — Threading status (public)
// ==================================================================================

SITAPI SituationThreadingStatus SituationGetThreadingStatus(void) {
    SituationThreadingStatus status = {0};

#if !defined(__STDC_NO_THREADS__)
    status.available = true;
    status.capabilities |= SITUATION_THREAD_CAP_C11_THREADS;
    status.capabilities |= SITUATION_THREAD_CAP_MUTEX;

#if !defined(__STDC_NO_ATOMICS__)
    status.capabilities |= SITUATION_THREAD_CAP_C11_ATOMICS;
#else
    if (status.warning_count < 4) {
        status.warnings[status.warning_count++] = "C11 atomics not available";
    }
#endif

#if defined(_WIN32)
    status.platform = "Windows";
    status.sleep_impl = "Windows Sleep (recommended)";
    status.sleep_reliable = true;
    status.capabilities |= SITUATION_THREAD_CAP_PLATFORM_SLEEP;
    status.max_threads = (int)SITUATION_MAX_THREADS;
#elif defined(__unix__) || defined(__APPLE__)
    status.platform = "POSIX";
    status.sleep_impl = "POSIX usleep (recommended)";
    status.sleep_reliable = true;
    status.capabilities |= SITUATION_THREAD_CAP_PLATFORM_SLEEP;
    status.max_threads = (int)SITUATION_MAX_THREADS;
#else
    status.platform = "Unknown";
    status.sleep_impl = "thrd_sleep (may have issues)";
    status.sleep_reliable = false;
    status.capabilities |= SITUATION_THREAD_CAP_SLEEP;
#endif

#if defined(_WIN32) && !defined(SITUATION_USE_PLATFORM_SLEEP)
    if (status.warning_count < 4) {
        status.warnings[status.warning_count++] = "Using tinycthread on Windows - recommend SITUATION_USE_PLATFORM_SLEEP";
    }
#endif

#else
    status.available = false;
    status.platform = "None";
    status.sleep_impl = "None";
    if (status.warning_count < 4) {
        status.warnings[status.warning_count++] = "C11 threads not available - threading disabled";
    }
#endif

    {
        const SituationCpuTopology* topo = NULL;
        status.platform_topology_ok = SituationGetCpuTopology(&topo) && topo && topo->logical_count > 0;
        status.numa_available = status.platform_topology_ok && topo->numa_node_count > 1;
    }

#if defined(SITUATION_ENABLE_THREADING)
    if (_sit_current_context != NULL && sit_gs.thread_pool.is_active) {
        status.pool_thread_count = (int)sit_gs.thread_pool.thread_count;
        status.io_thread_enabled = sit_gs.thread_pool.io_thread != 0;
    }
#endif

    return status;
}

SITAPI void SituationPrintThreadingStatus(FILE* out) {
    if (!out) out = stdout;
    SituationThreadingStatus status = SituationGetThreadingStatus();

    fprintf(out, "========================================\n");
    fprintf(out, "Threading Status\n");
    fprintf(out, "========================================\n");
    fprintf(out, "Available:            %s\n", status.available ? "YES" : "NO");
    fprintf(out, "Platform:             %s\n", status.platform ? status.platform : "?");
    fprintf(out, "Sleep Impl:           %s\n", status.sleep_impl ? status.sleep_impl : "?");
    fprintf(out, "Sleep Reliable:       %s\n", status.sleep_reliable ? "YES" : "NO");
    fprintf(out, "Max Threads:          %d\n", status.max_threads);
    fprintf(out, "Topology OK:          %s\n", status.platform_topology_ok ? "YES" : "NO");
    fprintf(out, "NUMA Detected:        %s\n", status.numa_available ? "YES" : "NO");
    fprintf(out, "Pool Workers:         %d\n", status.pool_thread_count);
    fprintf(out, "Dedicated I/O Thread: %s\n", status.io_thread_enabled ? "YES" : "NO");

    fprintf(out, "\nCapabilities:\n");
    fprintf(out, "  C11 Threads:    %s\n", (status.capabilities & SITUATION_THREAD_CAP_C11_THREADS) ? "YES" : "NO");
    fprintf(out, "  C11 Atomics:    %s\n", (status.capabilities & SITUATION_THREAD_CAP_C11_ATOMICS) ? "YES" : "NO");
    fprintf(out, "  Mutex:          %s\n", (status.capabilities & SITUATION_THREAD_CAP_MUTEX) ? "YES" : "NO");
    fprintf(out, "  Platform Sleep: %s\n", (status.capabilities & SITUATION_THREAD_CAP_PLATFORM_SLEEP) ? "YES" : "NO");

    if (status.warning_count > 0) {
        fprintf(out, "\nWarnings:\n");
        for (int i = 0; i < status.warning_count; ++i) {
            fprintf(out, "  - %s\n", status.warnings[i]);
        }
    }
    fprintf(out, "========================================\n");
}

// ==================================================================================
// B3 — Queue depth & active jobs
// ==================================================================================

SITAPI size_t SituationGetQueueDepth(SituationThreadPool* pool, SituationJobQueueMask mask) {
    if (!pool || !pool->is_active || mask == 0) return 0;
    size_t depth = 0;
    if (mask & SIT_JOB_QUEUE_LOW) depth += _SitQueueDepthUnlocked(pool, 0);
    if (mask & SIT_JOB_QUEUE_HIGH) depth += _SitQueueDepthUnlocked(pool, 1);
    return depth;
}

SITAPI size_t SituationGetHighQueueDepth(SituationThreadPool* pool) {
    return SituationGetQueueDepth(pool, SIT_JOB_QUEUE_HIGH);
}

SITAPI int SituationGetActiveJobCount(SituationThreadPool* pool) {
    if (!pool || !pool->is_active) return 0;
    return atomic_load(&pool->active_jobs);
}

// ==================================================================================
// B2 — Pool snapshot & dumps
// ==================================================================================

SITAPI bool SituationGetThreadPoolSnapshot(SituationThreadPool* pool, SituationThreadPoolSnapshot* out) {
    if (!out) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetThreadPoolSnapshot: out is NULL");
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!pool || !pool->is_active) {
        return false;
    }

    out->pool_active = true;
    out->worker_count = pool->thread_count;
    out->io_thread_enabled = (pool->io_thread != 0);
    out->active_jobs = atomic_load(&pool->active_jobs);
    out->low_queue_depth = _SitQueueDepthUnlocked(pool, 0);
    out->high_queue_depth = _SitQueueDepthUnlocked(pool, 1);
    out->stats_jobs_submitted = atomic_load(&pool->stats_jobs_submitted);
    out->stats_jobs_completed = atomic_load(&pool->stats_jobs_completed);
    out->stats_main_steal_success = atomic_load(&pool->stats_main_steal_success);
    out->stats_main_steal_fail = atomic_load(&pool->stats_main_steal_fail);

    int slot = 0;
    for (size_t i = 0; i < pool->thread_count && slot < SITUATION_THREAD_SNAPSHOT_MAX_SLOTS; ++i) {
        SituationThreadSlotSnapshot* s = &out->slots[slot++];
        s->role = SIT_THREAD_ROLE_WORKER;
        s->active = true;
        s->last_logical_cpu = atomic_load(&pool->worker_last_logical_cpu[i]);
        s->affinity_mask_applied = 0;
        s->numa_node = _SitNumaForLogicalCpu(s->last_logical_cpu);
    }

    if (pool->io_thread != 0 && slot < SITUATION_THREAD_SNAPSHOT_MAX_SLOTS) {
        SituationThreadSlotSnapshot* s = &out->slots[slot++];
        s->role = SIT_THREAD_ROLE_IO;
        s->active = atomic_load(&pool->io_active);
        s->last_logical_cpu = atomic_load(&pool->io_last_logical_cpu);
        s->numa_node = _SitNumaForLogicalCpu(s->last_logical_cpu);
    }

    if (atomic_load(&g_sit_obs_render.started) && slot < SITUATION_THREAD_SNAPSHOT_MAX_SLOTS) {
        SituationThreadSlotSnapshot* s = &out->slots[slot++];
        s->role = SIT_THREAD_ROLE_RENDER;
        s->active = true;
        s->last_logical_cpu = atomic_load(&g_sit_obs_render.last_logical_cpu);
        s->affinity_mask_applied = atomic_load(&g_sit_obs_render.affinity_mask_requested);
        s->numa_node = _SitNumaForLogicalCpu(s->last_logical_cpu);
    }

    if (atomic_load(&g_sit_obs_audio.started) && slot < SITUATION_THREAD_SNAPSHOT_MAX_SLOTS) {
        SituationThreadSlotSnapshot* s = &out->slots[slot++];
        s->role = SIT_THREAD_ROLE_AUDIO;
        s->active = true;
        s->last_logical_cpu = atomic_load(&g_sit_obs_audio.last_logical_cpu);
        s->affinity_mask_applied = atomic_load(&g_sit_obs_audio.affinity_mask_requested);
        s->numa_node = _SitNumaForLogicalCpu(s->last_logical_cpu);
    }

    out->slot_count = slot;
    return true;
}

static const char* _SitRoleName(SituationThreadRole role) {
    switch (role) {
        case SIT_THREAD_ROLE_MAIN: return "main";
        case SIT_THREAD_ROLE_WORKER: return "worker";
        case SIT_THREAD_ROLE_IO: return "io";
        case SIT_THREAD_ROLE_RENDER: return "render";
        case SIT_THREAD_ROLE_AUDIO: return "audio";
        default: return "unknown";
    }
}

SITAPI void SituationDumpThreadPoolStatus(SituationThreadPool* pool, FILE* out, bool json_mode) {
    if (!out) out = stderr;
    SituationThreadPoolSnapshot snap;
    if (!SituationGetThreadPoolSnapshot(pool, &snap)) {
        if (json_mode) fprintf(out, "{\"error\":\"pool inactive or null\"}\n");
        else fprintf(out, "(thread pool snapshot unavailable)\n");
        return;
    }

    if (json_mode) {
        fprintf(out, "{\"pool_active\":true,\"workers\":%zu,\"io_thread\":%s,\"active_jobs\":%d,"
                "\"low_queue\":%zu,\"high_queue\":%zu,"
                "\"stats\":{\"submitted\":%llu,\"completed\":%llu,\"steal_ok\":%llu,\"steal_fail\":%llu},"
                "\"slots\":[",
            snap.worker_count, snap.io_thread_enabled ? "true" : "false", snap.active_jobs,
            snap.low_queue_depth, snap.high_queue_depth,
            (unsigned long long)snap.stats_jobs_submitted,
            (unsigned long long)snap.stats_jobs_completed,
            (unsigned long long)snap.stats_main_steal_success,
            (unsigned long long)snap.stats_main_steal_fail);
        for (int i = 0; i < snap.slot_count; ++i) {
            const SituationThreadSlotSnapshot* s = &snap.slots[i];
            fprintf(out, "%s{\"role\":\"%s\",\"cpu\":%d,\"numa\":%d,\"affinity\":\"0x%llx\",\"active\":%s}",
                (i > 0) ? "," : "",
                _SitRoleName(s->role), s->last_logical_cpu, s->numa_node,
                (unsigned long long)s->affinity_mask_applied,
                s->active ? "true" : "false");
        }
        fprintf(out, "]}\n");
    } else {
        fprintf(out, "\n=== Situation Thread Pool ===\n");
        fprintf(out, "Workers: %zu | I/O thread: %s | Active jobs: %d\n",
            snap.worker_count, snap.io_thread_enabled ? "yes" : "no", snap.active_jobs);
        fprintf(out, "Queues: low=%zu high=%zu\n", snap.low_queue_depth, snap.high_queue_depth);
        fprintf(out, "Stats: submitted=%llu completed=%llu steal_ok=%llu steal_fail=%llu\n",
            (unsigned long long)snap.stats_jobs_submitted,
            (unsigned long long)snap.stats_jobs_completed,
            (unsigned long long)snap.stats_main_steal_success,
            (unsigned long long)snap.stats_main_steal_fail);
        for (int i = 0; i < snap.slot_count; ++i) {
            const SituationThreadSlotSnapshot* s = &snap.slots[i];
            fprintf(out, "  [%s] cpu=%d numa=%d affinity=0x%llx active=%s\n",
                _SitRoleName(s->role), s->last_logical_cpu, s->numa_node,
                (unsigned long long)s->affinity_mask_applied, s->active ? "yes" : "no");
        }
        fprintf(out, "=========================================\n\n");
    }
}

SITAPI void SituationDumpThreadingReport(SituationThreadPool* pool, FILE* out, bool json_mode) {
    if (!out) out = stderr;
    if (!json_mode) {
        SituationPrintThreadingStatus(out);
        const SituationCpuTopology* topo = NULL;
        if (SituationGetCpuTopology(&topo) && topo) {
            fprintf(out, "Topology: logical=%u physical=%u numa_nodes=%u\n",
                topo->logical_count, topo->physical_count, topo->numa_node_count);
        }
    } else {
        fprintf(out, "{\"threading_status\":");
        SituationThreadingStatus st = SituationGetThreadingStatus();
        fprintf(out, "{\"topology_ok\":%s,\"numa\":%s,\"pool_workers\":%d,\"io_thread\":%s},",
            st.platform_topology_ok ? "true" : "false",
            st.numa_available ? "true" : "false",
            st.pool_thread_count,
            st.io_thread_enabled ? "true" : "false");
        fprintf(out, "\"pool\":");
    }
    SituationDumpThreadPoolStatus(pool, out, json_mode);
}

#endif /* SITUATION_IMPL_THREADING_OBSERVABILITY_H */
