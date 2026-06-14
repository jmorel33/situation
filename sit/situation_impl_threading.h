/*
 * situation_impl_threading.h - Thread Pool & Job System Implementation
 * (c) 2025-2026 Jacques Morel
 * MIT Licensed
 *
 * Extracted from situation_impl.h for modularity.
 * This file is included by situation_impl.h when SITUATION_ENABLE_THREADING is defined.
 */

#ifndef SITUATION_IMPL_THREADING_H
#define SITUATION_IMPL_THREADING_H

#ifdef SITUATION_ENABLE_THREADING

// Include threading diagnostics for SITUATION_SLEEP_MS and debug macros
#include "situation_impl_threading_diag.h"
#include "situation_impl_threading_observability.h"
#include "situation_impl_threading_scheduler.h"

// ==================================================================================
//  Threading Implementation (v2.3.15+patch)
// ==================================================================================


/* CPU topology / affinity: situation_impl_threading_topology.h (included after io.h). */

// ==================================================================================
//  Cycle Detection & ID Helpers
// ==================================================================================

// ID Layout: [1 bit Queue] [15 bits Gen] [16 bits Slot]
#define SIT_ID_QUEUE_SHIFT 31
#define SIT_ID_GEN_SHIFT   16
#define SIT_ID_GEN_MASK    0x7FFF
#define SIT_ID_SLOT_MASK   0xFFFF

/**
 * @brief [INTERNAL] Packs queue index, generation, and slot index into a single Job ID.
 * @param q_idx The priority queue index (0 or 1).
 * @param gen The generation counter (15 bits).
 * @param slot The slot index in the ring buffer (16 bits).
 * @return A packed SituationJobId.
 */
static inline SituationJobId _SitMakeId(uint32_t q_idx, uint32_t gen, uint32_t slot) {
    return ((q_idx & 1) << SIT_ID_QUEUE_SHIFT) | ((gen & SIT_ID_GEN_MASK) << SIT_ID_GEN_SHIFT) | (slot & SIT_ID_SLOT_MASK);
}

/**
 * @brief [INTERNAL] Resolves a Job ID to a pointer, with validation.
 * @details Unpacks the ID, checks bounds, and validates the generation counter to prevent ABA issues.
 * @param pool The thread pool instance.
 * @param id The job handle to resolve.
 * @return Pointer to the SituationJob, or NULL if invalid/stale.
 */
static SituationJob* _SitGetJobFromId(SituationThreadPool* pool, SituationJobId id) {
    if (id == 0 || !pool) return NULL;

    uint32_t q_idx = (id >> SIT_ID_QUEUE_SHIFT) & 1;
    uint32_t slot_idx = id & SIT_ID_SLOT_MASK;
    uint32_t gen = (id >> SIT_ID_GEN_SHIFT) & SIT_ID_GEN_MASK;

    // Bounds check
    if (slot_idx >= pool->queues[q_idx].capacity) return NULL;

    SituationJob* job = &pool->queues[q_idx].jobs[slot_idx];

    // Generation check (ABA protection)
    if (atomic_load(&job->generation) != (uint16_t)gen) return NULL;

    return job;
}

/* Main thread: retire a pool job that was never claimed/executed but whose side effects
 * were satisfied elsewhere (v2.4.234–235 inline compile pump bug). No-op if completed or
 * claimed/in-flight — those paths must finish via the worker completion protocol. */
static void _SitThreadPoolRetireOrphanedJobMain(SituationThreadPool* pool, SituationJobId job_id) {
    if (job_id == 0 || !pool || !pool->is_active) return;

    uint32_t q_idx = (job_id >> SIT_ID_QUEUE_SHIFT) & 1;
    uint32_t slot_idx = job_id & SIT_ID_SLOT_MASK;
    if (slot_idx >= pool->queues[q_idx].capacity) return;

    mtx_lock(&pool->queues[q_idx].lock);
    SituationJob* job = &pool->queues[q_idx].jobs[slot_idx & pool->queues[q_idx].mask];
    uint32_t expected_gen = (job_id >> SIT_ID_GEN_SHIFT) & SIT_ID_GEN_MASK;
    if (atomic_load(&job->generation) != (uint16_t)expected_gen ||
        atomic_load(&job->is_completed)) {
        mtx_unlock(&pool->queues[q_idx].lock);
        return;
    }
    if (atomic_load(&job->dependency_count) != 0) {
        mtx_unlock(&pool->queues[q_idx].lock);
        return;
    }

    job->large_data_ptr = NULL;
    job->owns_memory = false;
    uint16_t g = atomic_load(&job->generation);
    atomic_store(&job->generation, (uint16_t)((g + 1) & SIT_ID_GEN_MASK));
    atomic_store(&job->dependency_count, 0);
    atomic_store(&job->is_completed, true);
    if (atomic_fetch_sub(&pool->active_jobs, 1) == 1) {
        cnd_broadcast(&pool->idle_condition);
    }
    atomic_fetch_add(&pool->stats_jobs_completed, 1);
    mtx_unlock(&pool->queues[q_idx].lock);
    cnd_signal(&pool->wake_condition);
}

/**
 * @brief [INTERNAL] Detects dependency cycles by walking the continuation chain.
 *
 * @details Performs a linear traversal starting from `dep_id`, following each job's
 *          `continuation_id` link. If the traversal encounters `prereq_id`, a cycle
 *          exists and the dependency must be rejected.
 *
 *          This is a simplified cycle check that works correctly under the 1:1
 *          continuation constraint (each job has at most one successor). It does NOT
 *          perform a full graph DFS - complex multi-path topologies are prevented by
 *          the continuation limit itself.
 *
 *          Depth is capped at 32 to prevent runaway traversal on pathological chains.
 *
 * @param pool          The thread pool instance.
 * @param prereq_id     The proposed prerequisite job.
 * @param dep_id        The proposed dependent job (start of traversal).
 * @param out_new_depth [out] The computed depth for the new edge.
 *
 * @return true if a cycle was detected or depth limit exceeded (reject the edge),
 *         false if safe to add.
 *
 * @note Must be called while the caller holds appropriate synchronization (or during
 *       single-threaded graph construction). The traversal reads `continuation_id`
 *       atomically but does not acquire any locks.
 *
 * @see SituationAddJobDependency, SituationAddJobDependencies,
 *      SITUATION_ERROR_THREAD_CYCLE
 */
/* HARDENING: bool by design — true when cycle or depth limit detected (caller sets THREAD_CYCLE). */
static bool _SituationDetectCycle(SituationThreadPool* pool, SituationJobId prereq_id, SituationJobId dep_id, uint8_t* out_new_depth) {
    uint8_t depth = 0;
    SituationJobId current_cursor = dep_id;

    // Base depth of the prerequisite (if it exists)
    SituationJob* prereq_ptr = _SitGetJobFromId(pool, prereq_id);
    uint8_t base_depth = prereq_ptr ? prereq_ptr->dep_depth : 0;

    // Traverse downstream from the dependent job
    while (current_cursor != 0 && depth < 32) {
        SituationJob* job = _SitGetJobFromId(pool, current_cursor);
        if (!job) break; // Chain broken (job finished or invalid)

        if (current_cursor == prereq_id) {
            // We found the prerequisite downstream from the dependent -> Cycle!
            if (out_new_depth) *out_new_depth = depth;
            return true;
        }

        current_cursor = atomic_load(&job->continuation_id);
        depth++;
    }

    if (out_new_depth) *out_new_depth = base_depth + 1;
    return (depth >= 32 || (base_depth + 1) >= 32); // Fail if too deep
}

// ==================================================================================
//  Task Graph API
// ==================================================================================

/**
 * @brief Establishes a directed dependency between two jobs (Prerequisite -> Dependent).
 *
 * @details This function constructs a dependency edge in the task graph, ensuring that the `dependent_job`
 *          will not execute until the `prerequisite_job` has completed.
 *
 *          **Mechanism:**
 *          - The dependency count of the `dependent_job` is atomically incremented.
 *          - The `continuation_id` of the `prerequisite_job` is updated to point to the `dependent_job`.
 *          - This uses a lock-free Compare-And-Swap (CAS) loop to ensure thread safety without mutexes.
 *
 *          **Cycle Detection:**
 *          Before modifying the graph, this function performs a depth-limited search (max depth 32)
 *          to detect potential cycles (e.g., A->B->A). If a cycle is detected, the operation is aborted
 *          to prevent deadlocks.
 *
 *          **Constraints:**
 *          - **1:1 Continuation:** Currently, a job can trigger only *one* direct successor via this mechanism.
 *            If `prerequisite_job` already has a continuation, this function will fail.
 *            For 1:N (Fan-Out) dependencies, use a dedicated dispatcher job that submits multiple children.
 *            For N:1 (Fan-In) dependencies, use `SituationAddJobDependencies`.
 *
 * @param pool The thread pool instance managing the jobs.
 * @param prereq_id The ID of the job that must finish first (the "parent" or "predecessor").
 * @param dep_id The ID of the job that is waiting (the "child" or "successor").
 *
 * @return `true` if the dependency was successfully added.
 * @return `false` if:
 *         - The thread pool is invalid.
 *         - Either job ID is invalid or refers to a completed/recycled slot.
 *         - A dependency cycle was detected.
 *         - The `prerequisite_job` already has a continuation (collision).
 *
 * @see SituationAddJobDependencies()
 */
SITAPI SituationError SituationAddJobDependency(SituationThreadPool* pool, SituationJobId prereq_id, SituationJobId dep_id) {
    if (!pool) return SITUATION_ERROR_INVALID_PARAM;

    // 1. Validate Handles
    SituationJob* prereq = _SitGetJobFromId(pool, prereq_id);
    SituationJob* dep = _SitGetJobFromId(pool, dep_id);

    if (!prereq || !dep) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid job IDs in AddJobDependency (jobs may have finished).");
        return SITUATION_ERROR_INVALID_PARAM;
    }

    // 2. Cycle Detection (Read-only traversal, safe-ish without lock if topology is stable-ish)
    uint8_t new_depth = 0;
    if (_SituationDetectCycle(pool, prereq_id, dep_id, &new_depth)) {
        #ifndef NDEBUG
        fprintf(stderr, "[Situation] ERROR: Dependency Cycle Detected! Job 0x%x -> 0x%x causes loop.\n", prereq_id, dep_id);
        #endif
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CYCLE, "Cycle detected or depth limit (32) exceeded.");
        return SITUATION_ERROR_THREAD_CYCLE;
    }

    // 3. Link via CAS (Compare-And-Swap)
    // We only allow one continuation per job in this lightweight system.
    uint32_t expected_cont = 0;
    if (atomic_compare_exchange_strong_explicit(&prereq->continuation_id, &expected_cont, dep_id, memory_order_seq_cst, memory_order_seq_cst)) {
        // Successfully linked prereq -> dep
        atomic_fetch_add(&dep->dependency_count, 1);
        dep->dep_depth = new_depth;
        return SITUATION_SUCCESS;
    } else {
        // Prerequisite already has a continuation
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL, "Prerequisite job already has a continuation (1:1 limit). Use SituationAddJobDependencies for fan-in.");
        return SITUATION_ERROR_THREAD_QUEUE_FULL;
    }
    return SITUATION_ERROR_THREAD_QUEUE_FULL;
}

/**
 * @brief Establishes a Many-to-One dependency (Fan-In).
 *
 * @details Configures the `dependent_job` to wait for *multiple* prerequisite jobs to complete.
 *          This is commonly used for synchronization points, such as a "Frame End" job that waits for
 *          "Physics", "AI", and "Rendering" jobs to finish.
 *
 *          This is a helper function that iteratively calls `SituationAddJobDependency` for each
 *          ID in the `prerequisites` array.
 *
 * @param pool The thread pool instance.
 * @param prerequisites An array of job IDs that must complete before the dependent job starts.
 * @param count The number of job IDs in the `prerequisites` array.
 * @param dependent_job The ID of the job that will wait.
 *
 * @return `true` if **all** dependencies were successfully added.
 * @return `false` if *any* dependency failed to be added (e.g., due to invalid ID, cycle, or 1:1 constraint violation).
 *         If `false` is returned, the dependency graph may be in a partially updated state (some dependencies added, some not).
 *
 * @see SituationAddJobDependency()
 */
SITAPI SituationError SituationAddJobDependencies(SituationThreadPool* pool, SituationJobId* prerequisites, int count, SituationJobId dependent_job) {
    for (int i = 0; i < count; ++i) {
        // Note: The parameter order in AddJobDependency is (Prereq, Dependent)
        SituationError err = SituationAddJobDependency(pool, prerequisites[i], dependent_job);
        if (err != SITUATION_SUCCESS) {
            return err; // Fail fast if any link fails
        }
    }
    return SITUATION_SUCCESS;
}

/**
 * @brief Dumps the current state of the task graph to a file stream for debugging.
 *
 * @details Prints a snapshot of all active jobs, their states, and their dependency links.
 *          This is an invaluable tool for diagnosing deadlocks, verifying graph topology, or visualizing
 *          the execution flow of complex async workloads.
 *
 * @par Output Formats
 *   - **Text Mode (`json_mode = false`):** A human-readable, column-aligned table useful for console logging.
 *   - **JSON Mode (`json_mode = true`):** A structured JSON object representing the graph nodes and edges.
 *     This output can be piped to a file and visualized using graph plotting tools (e.g., Graphviz, D3.js).
 *
 * @param pool The thread pool instance to inspect.
 * @param out The output file stream (e.g., `stdout`, `stderr`, or a file opened with `fopen`).
 *            If `NULL`, defaults to `stderr`.
 * @param json_mode Set to `true` for machine-readable JSON output, `false` for human-readable text.
 */
SITAPI void SituationDumpTaskGraph(SituationThreadPool* pool, FILE* out, bool json_mode) {
    if (!pool) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationDumpTaskGraph: pool is NULL"); return; }
    if (!out) out = stderr;

    if (json_mode) fprintf(out, "{\"active_jobs\": %d, \"queues\": {\n", atomic_load(&pool->active_jobs));
    else {
        fprintf(out, "\n=== Situation Task Graph (Active: %d) ===\n", atomic_load(&pool->active_jobs));
    }

    for (int q = 0; q < 2; ++q) {
        const char* q_name = (q == 1) ? "high" : "low";
        if (json_mode) fprintf(out, "  \"%s\": [", q_name);
        else fprintf(out, "-- Queue: %s --\n", q_name);

        size_t cap = pool->queues[q].capacity;
        bool first = true;

        for (size_t i = 0; i < cap; ++i) {
            SituationJob* job = &pool->queues[q].jobs[i];
            uint16_t gen = atomic_load(&job->generation);
            bool completed = atomic_load(&job->is_completed);

            // Only dump active jobs
            if (!completed && gen > 0) {
                int deps = atomic_load(&job->dependency_count);
                uint32_t cont = atomic_load(&job->continuation_id);

                if (json_mode) {
                    if (!first) fprintf(out, ",");
                    fprintf(out, "{\"id\":\"0x%08x\",\"gen\":%d,\"depth\":%u,\"deps\":%d,\"cont\":\"0x%08x\"}",
                        _SitMakeId(q, gen, (uint32_t)i), gen, job->dep_depth, deps, cont);
                    first = false;
                } else {
                    fprintf(out, "  [0x%08x] Gen:%04d | Depth:%02u | Wait:%d | Trig:0x%08x\n",
                        _SitMakeId(q, gen, (uint32_t)i), gen, job->dep_depth, deps, cont);
                }
            }
        }
        if (json_mode) fprintf(out, "]%s\n", (q==0) ? "," : "");
    }
    if (json_mode) fprintf(out, "}}\n");
    else fprintf(out, "=========================================\n\n");
}

// ==================================================================================
//  Worker Thread Implementation (Updated)
// ==================================================================================

/**
 * @brief [INTERNAL] Worker thread main loop for `SituationCreateThreadPool`.
 *
 * @details Each worker waits on `wake_condition` when both relevant queues are empty,
 *          then dequeues under `queues[q].lock` (high queue first). Dequeue may scan
 *          forward past blocked tail jobs (dynamic depth, Epic D). Low queue is skipped
 *          when a dedicated I/O thread owns it (`pool->io_thread != 0`).
 *
 * @param arg `SituationWorkerStartArg` (pool handle + worker index for NUMA spread).
 * @return 0 after cooperative shutdown (`pool->shutdown`).
 */
static int _SituationWorkerEntry(void* arg) {
    SituationWorkerStartArg* start = (SituationWorkerStartArg*)arg;
    SituationThreadPool* pool = start->pool_handle;
    size_t worker_index = start->worker_index;
    int jobs_since_cpu_sample = 0;

    {
        char name[32];
        snprintf(name, sizeof(name), "Sit Worker %zu", worker_index);
        _SituationSetCurrentThreadName(name);
    }

    _SituationApplyWorkerNumaPlacement(pool, worker_index);

    while (!atomic_load(&pool->shutdown)) {
        SituationJob* job_ptr = NULL;
        int queue_idx = -1;

        // --- Job Picking Loop (High priority first; low only when no dedicated I/O thread) ---
        for (int q = 1; q >= 0; --q) {
            if (q == 0 && pool->io_thread != 0) {
                continue; /* _SituationIOThreadEntry owns the low-priority (I/O) queue */
            }
            uint64_t lock_start_ns = (q == 1) ? _SitGetMonotonicTimeNS() : 0;
            mtx_lock(&pool->queues[q].lock);
            if (q == 1) {
                atomic_fetch_add(&pool->stats_high_queue_lock_ops, 1);
            }

            size_t head = atomic_load(&pool->queues[q].head);
            size_t tail = atomic_load(&pool->queues[q].tail);

            if (tail != head) {
                /* [Epic D] Scan-forward past blocked tail jobs — claim in-place (never swap;
                 * handles encode slot_idx). Tail compacts lazily past completed front slots. */
                SituationJob* claimed = NULL;
                bool found_ready = _SitWorkerTryClaimReadyJob(pool, q, &claimed);

                if (found_ready) {
                    job_ptr = claimed;
                    queue_idx = q;
                }

                if (!found_ready) {
                    if (q == 1) {
                        atomic_fetch_add(&pool->stats_scan_forward_exhausted, 1);
                    }
                    if (q == 1 && lock_start_ns != 0) {
                        atomic_fetch_add(&pool->stats_high_queue_lock_ns,
                            _SitGetMonotonicTimeNS() - lock_start_ns);
                    }
                    mtx_unlock(&pool->queues[q].lock);
                    thrd_yield();
                    continue; // Try next priority queue
                }

                if (q == 1 && lock_start_ns != 0) {
                    atomic_fetch_add(&pool->stats_high_queue_lock_ns,
                        _SitGetMonotonicTimeNS() - lock_start_ns);
                }
                mtx_unlock(&pool->queues[q].lock);
                break; // Stop searching, we found work
            } else {
                if (q == 1 && lock_start_ns != 0) {
                    atomic_fetch_add(&pool->stats_high_queue_lock_ns,
                        _SitGetMonotonicTimeNS() - lock_start_ns);
                }
                mtx_unlock(&pool->queues[q].lock);
            }
        }

        // If no job found in either queue
        if (!job_ptr) {
            mtx_lock(&pool->queues[0].lock);
            size_t head_hi = atomic_load(&pool->queues[1].head);
            size_t tail_hi = atomic_load(&pool->queues[1].tail);
            size_t head_lo = atomic_load(&pool->queues[0].head);
            size_t tail_lo = atomic_load(&pool->queues[0].tail);
            int work_pending = (head_hi != tail_hi);
            if (!pool->io_thread) {
                work_pending = work_pending || (head_lo != tail_lo);
            }
            if (!work_pending && !atomic_load(&pool->shutdown)) {
                _SitWorkerSampleCpu(pool, worker_index);
                // [FIX] Use timed wait (1ms) instead of indefinite wait
                // This ensures workers wake up regularly to check for work
                // even if condition signals are missed
                struct timespec ts;
                timespec_get(&ts, TIME_UTC);
                ts.tv_nsec += 1000000; // Add 1ms
                if (ts.tv_nsec >= 1000000000) {
                    ts.tv_sec += 1;
                    ts.tv_nsec -= 1000000000;
                }
                cnd_timedwait(&pool->wake_condition, &pool->queues[0].lock, &ts);
            }
            mtx_unlock(&pool->queues[0].lock);
            continue;
        }

        // --- Execute Job ---
        if (job_ptr) {
            // Dependency edge may be added on the main thread after dequeue but before we run
            // (submit job B, submit A, SituationAddJobDependency(A,B)). Wait until prereqs fire.
            while (_SitJobDepCount(atomic_load(&job_ptr->dependency_count)) != 0) {
                if (atomic_load(&pool->shutdown)) break;
                thrd_yield();
            }
            // 1. Run User Function
            void* data_arg = job_ptr->uses_large_data ? job_ptr->large_data_ptr : job_ptr->storage;
            if (job_ptr->func) {
                // [CRITICAL FIX] Legacy Support for SituationError* signature
                // Previous API versions passed a SituationError* as the second argument.
                // While the new API uses void* user_context (currently unused/NULL),
                // legacy code might try to write to the second argument.
                // We pass a dummy error variable to prevent segfaults if old code is linked against this new implementation.
                SituationError dummy_err = SITUATION_SUCCESS;
                job_ptr->func(data_arg, (void*)&dummy_err);
            }

            // 2. Handle Continuation
            uint32_t cont_id = atomic_load(&job_ptr->continuation_id);
            if (cont_id != 0) {
                SituationJob* next_job = _SitGetJobFromId(pool, cont_id);
                if (next_job) {
                    if (_SitJobDecrementDependency(next_job)) {
                        // Job became ready (count went 1 -> 0).
                        // [Patch 6] Signal outside lock is acceptable here: the atomic
                        // store of dependency_count provides happens-before visibility.
                        // The signal is a hint -- woken workers re-check under their own lock.
                        cnd_signal(&pool->wake_condition);
                    }
                }
            }

            // 3. Completion & Cleanup
            // [Safety] Free copied data if owned
            if (job_ptr->owns_memory && job_ptr->large_data_ptr) {
                SIT_FREE(job_ptr->large_data_ptr);
                job_ptr->large_data_ptr = NULL;
                job_ptr->owns_memory = false;
            }

            // [FIX] Bump generation BEFORE is_completed. is_completed=true releases the
            // slot for reuse (submit checks it); if generation were bumped after, a
            // submitter could capture the stale generation and the new job's handle
            // would falsely report complete via the gen-mismatch path in WaitForJob.
            // After is_completed=true this thread must not touch the slot again.
            uint16_t old_gen = atomic_load(&job_ptr->generation);
            atomic_store(&job_ptr->generation, (uint16_t)((old_gen + 1) & SIT_ID_GEN_MASK));

            atomic_store(&job_ptr->is_completed, true);

            // Decrement global active count
            if (atomic_fetch_sub(&pool->active_jobs, 1) == 1) {
                cnd_broadcast(&pool->idle_condition); // Wake Main Thread (WaitForAll)
            }
            atomic_fetch_add(&pool->stats_jobs_completed, 1);
            if (++jobs_since_cpu_sample >= SIT_WORKER_CPU_SAMPLE_INTERVAL) {
                jobs_since_cpu_sample = 0;
                _SitWorkerSampleCpu(pool, worker_index);
            }
        }
    }
    return 0;
}

/**
 * @brief Initializes the thread pool and spawns worker threads.
 *
 * @details Allocates resources for the dual-priority ring buffers (High/Low) and spawns the requested number of worker threads.
 *          The system uses a **Generational Index** strategy for job handles, ensuring O(1) validation and preventing ABA problems
 *          (reuse of IDs) without the need for heavy locking or dynamic allocation per job.
 *
 *          The **High Priority Queue** is always checked first by workers. This ensures that critical, latency-sensitive tasks
 *          (like Physics updates or Audio processing) are never blocked behind a backlog of bulk background work (like Asset Loading).
 *
 * @param pool Pointer to the `SituationThreadPool` struct to initialize. This memory must be allocated by the caller (e.g., on the stack or heap).
 * @param num_threads Number of worker threads to spawn.
 *                    - Pass `0` to automatically detect the number of logical CPU cores and use a sensible default (usually `logical_cores - 1`).
 *                    - Recommended setting: `logical_cores - 1` to leave the main thread free for window/rendering tasks.
 * @param queue_size The capacity of the internal job ring buffers.
 *                   - Must be a power of 2 (e.g., 1024, 2048). If not, it will be automatically rounded up.
 *                   - This determines the maximum number of pending jobs before backpressure strategies (Blocking or Run-Inline) are triggered.
 *
 * @return `true` if the thread pool was successfully initialized.
 * @return `false` if initialization failed (e.g., memory allocation failure, thread creation failure).
 *
 * @warning This function must be called from the main thread.
 */

/**
 * @brief Creates and initializes a thread pool for parallel task execution.
 *
 * @details This function allocates and starts a configurable number of worker threads,
 *          each running an infinite loop that waits for and executes submitted jobs.
 *          The pool uses a dual-priority queue system (high/low) and supports job
 *          dependencies, asynchronous I/O, and optional hot-reload rate limiting.
 *
 *          Once created, the pool can be used with:
 *            - `SituationSubmitJobEx` / `SituationSubmitJob` for individual tasks
 *            - `SituationDispatchParallel` for fork-join style parallel loops
 *            - `SituationLoadFileAsync`, `SituationSaveFileAsync`, etc. for I/O offloading
 *
 * Key features:
 *   - Configurable thread count and queue capacity
 *   - Two priority levels (high for latency-critical, low for background/IO)
 *   - Job dependency graph support (prerequisites -> dependent jobs)
 *   - Optional hot-reload polling rate (to avoid excessive filesystem checks)
 *   - Graceful shutdown via `SituationDestroyThreadPool`
 *
 * Thread safety:
 *   - Safe to call from the main thread before any rendering or audio starts
 *   - All subsequent submissions and waits are thread-safe
 *   - Workers do not access main-thread-only resources (e.g. GL context)
 *
 * @param pool Pointer to an uninitialized `SituationThreadPool` structure.
 *             The function will fill in all fields (queues, threads, condvars, etc.).
 * @param num_threads Number of worker threads to create.
 *                    Recommended: physical cores - 1 or -2 to leave headroom for main/render/audio.
 *                    Pass 0 to auto-detect (uses hardware concurrency).
 * @param queue_size Maximum number of pending jobs before submissions block or fail.
 *                   Should be at least 2--4x num_threads for good throughput.
 * @param hot_reload_rate Seconds between filesystem checks for hot-reload watchers
 *                        (0.0f disables periodic checking; use callbacks instead).
 * @param disable_io If true, workers will not perform disk I/O operations
 *                   (useful when main thread handles all filesystem access).
 *
 * @return true on success (pool fully initialized and workers running),
 *         false on failure (allocation error, thread creation failed, etc.).
 *         On failure, sets an appropriate `SituationError` code internally.
 *
 * @note The pool must be destroyed with `SituationDestroyThreadPool` before the
 *       application exits to avoid resource leaks and ensure clean worker shutdown.
 *
 * @see SituationThreadPool, SituationDestroyThreadPool, SituationSubmitJobEx,
 *      SituationDispatchParallel, SituationWaitForJob, SituationWaitForAllJobs,
 *      SIT_SUBMIT_HIGH_PRIORITY, SIT_SUBMIT_LOW_PRIORITY
 */
SITAPI SituationError SituationCreateThreadPool(SituationThreadPool* pool, size_t num_threads, size_t queue_size, double hot_reload_rate, bool disable_io) {
#ifdef SITUATION_DEBUG_THREADING
    printf("[THREADING] SituationCreateThreadPool called\n");
    fflush(stdout);
#endif
    SIT_ASSERT_MAIN_THREAD();
    if (!pool) {
#ifdef SITUATION_DEBUG_THREADING
        printf("[THREADING] ERROR: pool is NULL\n");
        fflush(stdout);
#endif
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationCreateThreadPool: pool is NULL");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    memset(pool, 0, sizeof(SituationThreadPool));

    // Auto-detect threads if 0 (Epic D: logical or physical cores minus reserved)
    if (num_threads == 0) {
        num_threads = _SitResolveAutoWorkerCount();
#ifdef SITUATION_DEBUG_THREADING
        printf("[THREADING] Auto-detected %zu worker threads\n", num_threads);
        fflush(stdout);
#endif
    }
    if (num_threads > SITUATION_MAX_THREADS) num_threads = SITUATION_MAX_THREADS;
    pool->thread_count = num_threads;
    pool->hot_reload_rate = hot_reload_rate; // [v2.3.37] Store rate

    // Round queue size up to next power of 2 for fast bitmasking
    size_t cap = 256;
    while (cap < queue_size) cap *= 2;

    // Initialize Dual Queues
    for (int i = 0; i < 2; i++) {
        pool->queues[i].capacity = cap;
        pool->queues[i].mask = cap - 1;
        pool->queues[i].jobs = (SituationJob*)SIT_CALLOC(cap, sizeof(SituationJob));

        if (!pool->queues[i].jobs) {
            // Cleanup previous if failed
            if(i==1) SIT_FREE(pool->queues[0].jobs);
            _SituationSetErrorFromCode(SITUATION_ERROR_MEMORY_ALLOCATION, "SituationCreateThreadPool: failed to allocate job queue");
            return SITUATION_ERROR_MEMORY_ALLOCATION;
        }

        if (mtx_init(&pool->queues[i].lock, mtx_plain) != thrd_success) {
            // Cleanup allocated job arrays
            SIT_FREE(pool->queues[i].jobs);
            if (i == 1) SIT_FREE(pool->queues[0].jobs);
            _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED,
                "SituationCreateThreadPool: mtx_init failed for queue lock");
            return SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED;
        }
        atomic_init(&pool->queues[i].head, 0);
        atomic_init(&pool->queues[i].tail, 0);

        // Initialize generations to 1 (0 is reserved for null/invalid)
        for(size_t k=0; k<cap; k++) {
            atomic_init(&pool->queues[i].jobs[k].generation, 1);
            atomic_init(&pool->queues[i].jobs[k].is_completed, true); // Slots start "free"
        }
    }

    cnd_init(&pool->wake_condition);
    cnd_init(&pool->idle_condition);
    atomic_init(&pool->active_jobs, 0);
    atomic_init(&pool->shutdown, false);
    atomic_init(&pool->io_last_logical_cpu, -1);
    atomic_init(&pool->stats_jobs_submitted, 0);
    atomic_init(&pool->stats_jobs_completed, 0);
    atomic_init(&pool->stats_main_steal_success, 0);
    atomic_init(&pool->stats_main_steal_fail, 0);
    atomic_init(&pool->stats_main_steal_empty_queue, 0);
    atomic_init(&pool->stats_high_queue_lock_ops, 0);
    atomic_init(&pool->stats_high_queue_lock_ns, 0);
    atomic_init(&pool->stats_scan_forward_swap, 0);
    atomic_init(&pool->stats_scan_forward_exhausted, 0);
    atomic_init(&pool->stats_io_idle_waits, 0);
    atomic_init(&pool->stats_io_jobs_run, 0);
    atomic_init(&pool->stats_submit_run_inline, 0);
    atomic_init(&pool->stats_queue_full_spins, 0);
    atomic_init(&pool->stats_dispatch_parallel_calls, 0);
    for (size_t wi = 0; wi < SITUATION_MAX_THREADS; ++wi) {
        atomic_init(&pool->worker_last_logical_cpu[wi], -1);
        pool->worker_args[wi].pool_handle = pool;
        pool->worker_args[wi].worker_index = wi;
    }

#ifdef SITUATION_DEBUG_THREADING
    printf("[THREADING] About to create %zu worker threads...\n", num_threads);
    fflush(stdout);
    fprintf(stderr, "[THREADING] Creating %zu worker threads...\n", num_threads);
#endif
    for (size_t i = 0; i < num_threads; ++i) {
#ifdef SITUATION_DEBUG_THREADING
        printf("[THREADING] Creating worker thread %zu...\n", i);
        fflush(stdout);
#endif
        pool->worker_args[i].pool_handle = pool;
        pool->worker_args[i].worker_index = i;
        if (thrd_create(&pool->threads[i], _SituationWorkerEntry, &pool->worker_args[i]) != thrd_success) {
#ifdef SITUATION_DEBUG_THREADING
            printf("[THREADING] ERROR: Failed to create worker thread %zu\n", i);
            fflush(stdout);
            fprintf(stderr, "[THREADING] ERROR: Failed to create worker thread %zu\n", i);
#endif
            // Rollback: signal shutdown and join already-created threads
            atomic_store(&pool->shutdown, true);
            cnd_broadcast(&pool->wake_condition);
            for (size_t j = 0; j < i; ++j) {
                thrd_join(pool->threads[j], NULL);
            }
            mtx_destroy(&pool->queues[0].lock);
            mtx_destroy(&pool->queues[1].lock);
            cnd_destroy(&pool->wake_condition);
            cnd_destroy(&pool->idle_condition);
            SIT_FREE(pool->queues[0].jobs);
            SIT_FREE(pool->queues[1].jobs);
            memset(pool, 0, sizeof(SituationThreadPool));
            _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CREATION_FAILED, "SituationCreateThreadPool: thrd_create failed for worker thread");
            return SITUATION_ERROR_THREAD_CREATION_FAILED;
        } else {
#ifdef SITUATION_DEBUG_THREADING
            printf("[THREADING] Worker thread %zu created successfully\n", i);
            fflush(stdout);
#endif
        }
    }

    // [v2.3.34] Spawn Dedicated I/O Thread (Conditional)
    if (!disable_io) {
#ifdef SITUATION_DEBUG_THREADING
        fprintf(stderr, "[THREADING] Creating I/O thread...\n");
#endif
        if (thrd_create(&pool->io_thread, _SituationIOThreadEntry, pool) != thrd_success) {
#ifdef SITUATION_DEBUG_THREADING
            fprintf(stderr, "[THREADING] ERROR: Failed to create I/O thread\n");
#endif
            // Rollback: signal shutdown and join all worker threads
            atomic_store(&pool->shutdown, true);
            cnd_broadcast(&pool->wake_condition);
            for (size_t j = 0; j < num_threads; ++j) {
                thrd_join(pool->threads[j], NULL);
            }
            mtx_destroy(&pool->queues[0].lock);
            mtx_destroy(&pool->queues[1].lock);
            cnd_destroy(&pool->wake_condition);
            cnd_destroy(&pool->idle_condition);
            SIT_FREE(pool->queues[0].jobs);
            SIT_FREE(pool->queues[1].jobs);
            memset(pool, 0, sizeof(SituationThreadPool));
            _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CREATION_FAILED, "SituationCreateThreadPool: thrd_create failed for I/O thread");
            return SITUATION_ERROR_THREAD_CREATION_FAILED;
        }
    } else {
        pool->io_thread = 0; // Explicitly null
    }

#ifdef SITUATION_DEBUG_THREADING
    fprintf(stderr, "[THREADING] Thread pool created successfully!\n");
    fprintf(stderr, "[THREADING] Worker threads: %zu\n", num_threads);
    fprintf(stderr, "[THREADING] I/O thread: %s\n", disable_io ? "DISABLED" : "ENABLED");
#endif
    pool->is_active = true;
    return SITUATION_SUCCESS;
}

/**
 * @brief Submits a job to the thread pool for execution.
 *
 * @details Pushes a task into one of the priority queues. This function is thread-safe and wait-free in the common case
 *          (unless the queue is full).
 *
 *          **Small Object Optimization (SOO):**
 *          - If `data_size` is <= 64 bytes, the payload is copied *by value* into the job structure itself. This avoids heap allocation
 *            and pointer indirection, making it ideal for passing small structs like matrices, vectors, or configuration identifiers.
 *          - If `data_size` > 64 bytes, the `data` pointer is passed through directly. The user must ensure the pointed-to memory
 *            remains valid until the job completes.
 *
 * @param pool The thread pool instance.
 * @param func The work function to execute. Prototype: `void my_func(void* data, void* unused)`.
 * @param data A pointer to the data payload.
 * @param data_size The size of the data payload in bytes. Determines whether SOO is used.
 * @param flags Bitmask of behavior flags:
 *              - `SIT_SUBMIT_HIGH_PRIORITY`: Submit to the high-priority queue (Physics, Audio).
 *              - `SIT_SUBMIT_BLOCK_IF_FULL`: If the queue is full, block (sleep) until a slot becomes available.
 *              - `SIT_SUBMIT_RUN_IF_FULL`: If the queue is full, execute the job *immediately* on the calling thread. This prevents stalls but blocks the main thread temporarily.
 *
 * @return A unique `SituationJobId` handle for the submitted job.
 *         - This handle encodes the queue index, slot index, and a generation counter.
 *         - It allows for O(1) status checks via `SituationWaitForJob`.
 *         - Returns `0` if the job could not be submitted (e.g., queue full and no backpressure strategy specified).
 */
SITAPI SituationJobId SituationSubmitJobEx(SituationThreadPool* pool, void (*func)(void*, void*), const void* data, size_t data_size, SituationJobFlags flags) {
    if (!pool || !pool->is_active) {
        if (pool && !pool->is_active) {
            _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_STATE_INVALID,
                "SituationSubmitJobEx: pool is not active (destroyed or not initialized)");
        }
        return 0;
    }

    int q_idx = (flags & SIT_SUBMIT_HIGH_PRIORITY) ? 1 : 0;

    mtx_lock(&pool->queues[q_idx].lock);

    // [Patch 7] Inline Fallback if I/O Thread Disabled (Queue 0 Only)
    // When no I/O thread exists, low-priority jobs are executed immediately on the
    // calling thread. Returns 0 which is semantically "already complete" (same as
    // SIT_SUBMIT_RUN_IF_FULL behavior). Callers already handle 0 as "no handle needed."
    if (q_idx == 0 && pool->io_thread == 0) {
        mtx_unlock(&pool->queues[q_idx].lock);
        atomic_fetch_add(&pool->stats_submit_run_inline, 1);
        // Execute immediately
        if (func) {
            SituationError dummy_err = SITUATION_SUCCESS;
            func((void*)data, (void*)&dummy_err);
        }
        return 0; // Treated as "done/inline"
    }

    // Check Capacity (with Backpressure Handling)
    size_t head;
    while (true) {
        head = atomic_load(&pool->queues[q_idx].head);
        size_t tail = atomic_load(&pool->queues[q_idx].tail);

        /* [FIX] A slot may still be IN FLIGHT after tail passes it: workers/stealers
         * advance tail before executing out of the slot and stamp is_completed/generation
         * into it afterwards. Reusing such a slot loses the in-flight job's fields
         * (lost job / double-run) and lets the late completion stamp corrupt the new
         * job. Treat "target slot not completed" exactly like a full queue so the
         * existing backpressure paths (RUN_IF_FULL / BLOCK_IF_FULL / fail) apply. */
        bool slot_in_flight =
            !atomic_load(&pool->queues[q_idx].jobs[head & pool->queues[q_idx].mask].is_completed);

        if (head - tail >= pool->queues[q_idx].capacity || slot_in_flight) {
            mtx_unlock(&pool->queues[q_idx].lock);

            // Handle Backpressure
            if (flags & SIT_SUBMIT_RUN_IF_FULL) {
                atomic_fetch_add(&pool->stats_submit_run_inline, 1);
                // "Velocity" Path: Run immediately to avoid stutter
                if(func) {
                    // [CRITICAL FIX] Legacy Support for SituationError* signature
                    // Must pass a dummy error pointer, even on main thread, to prevent segfaults in legacy callbacks.
                    SituationError dummy_err = SITUATION_SUCCESS;
                    func((void*)data, (void*)&dummy_err);
                }
                return 0; // 0 indicates job is already done/invalid handle
            }
            else if (flags & SIT_SUBMIT_BLOCK_IF_FULL) {
                atomic_fetch_add(&pool->stats_queue_full_spins, 1);
                // "Robust" Path: Spin-wait (polite yield) until slot opens
                SITUATION_SLEEP_MS(0);
                mtx_lock(&pool->queues[q_idx].lock);
                continue;
            }

            // Default Path: Fail
            _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL, "Job queue full and no blocking/run-inline flag set.");
            return 0;
        }
        // If we have space, break the retry loop and proceed to slot reservation
        break;
    }

    size_t slot_idx = head & pool->queues[q_idx].mask;
    SituationJob* job = &pool->queues[q_idx].jobs[slot_idx];
    uint16_t gen = atomic_load(&job->generation);

    // Init Job Fields
    job->func = func;
    atomic_store(&job->is_completed, false);

    // Reset Dependency Fields
    atomic_store(&job->dependency_count, 0);
    atomic_store(&job->continuation_id, 0);
    job->dep_depth = 0;

    // SOO Logic & Safe Copy
    if (data_size > 0 && data_size <= SITUATION_JOB_PAYLOAD_MAX) {
        // Small Data: Copy into SOO buffer
        if (data) memcpy(job->storage, data, data_size);
        job->uses_large_data = false;
        job->owns_memory = false;
    } else {
        // Large Data
        if (flags & SIT_SUBMIT_POINTER_ONLY) {
            // Optimization: Just store the pointer (User promises lifetime)
            job->large_data_ptr = (void*)data;
            job->owns_memory = false;
        } else if (data && data_size > 0) {
            // Safety: Allocate and Copy (Default)
            job->large_data_ptr = SIT_MALLOC(data_size);
            if (job->large_data_ptr) {
                memcpy(job->large_data_ptr, data, data_size);
                job->owns_memory = true;
            } else {
                // [Patch 5] Allocation failed -- reject submission explicitly.
                // Returning 0 is already handled by callers as "job not submitted."
                // This prevents potential use-after-free if caller deallocates data
                // before the job runs.
                atomic_store(&job->is_completed, true);  // Mark slot as free
                mtx_unlock(&pool->queues[q_idx].lock);
                _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL,
                    "Failed to allocate job payload buffer (out of memory).");
                return 0;
            }
        } else {
            // No data or invalid size
            job->large_data_ptr = (void*)data;
            job->owns_memory = false;
        }
        job->uses_large_data = true;
    }

    // Commit
    atomic_fetch_add(&pool->queues[q_idx].head, 1);
    atomic_fetch_add(&pool->active_jobs, 1);
    atomic_fetch_add(&pool->stats_jobs_submitted, 1);

    // [FIX] Signal BEFORE unlock to prevent lost wakeups
    cnd_signal(&pool->wake_condition);
    mtx_unlock(&pool->queues[q_idx].lock);

    return _SitMakeId((uint32_t)q_idx, (uint32_t)gen, (uint32_t)slot_idx);
}

/**
 * @brief Blocks the calling thread until a specific job has completed.
 *
 * @details This function performs an efficient O(1) check using the job's generation ID.
 *          It handles three scenarios correctly:
 *          1. **Job Pending/Running:** The ID in the slot matches. The function yields/sleeps until the `is_completed` flag is set.
 *          2. **Job Already Finished & Replaced:** The ID in the slot has a *newer* generation than the handle. This proves the old job finished and the slot was reused. Returns immediately.
 *          3. **Invalid Handle:** Returns immediately (safe default).
 *
 * @param pool The thread pool instance.
 * @param job_id The handle of the job to wait for.
 * @return `true` when the job is confirmed complete.
 */
/**
 * @brief [INTERNAL] Non-blocking check: has a job handle settled (completed or slot moved on)?
 * @details Same O(1) generation/flag logic as SituationWaitForJob, without waiting.
 *          Used by async consumers (e.g. the Vulkan async shader poll) to distinguish
 *          "job still pending/running" from "the queue retired this handle". If this
 *          returns true but the job's side effects are absent, the job was lost —
 *          a scheduler defect that callers must surface as SITUATION_ERROR_THREAD_JOB_LOST
 *          instead of reporting in-progress forever.
 */
static bool _SitJobHandleSettled(SituationThreadPool* pool, SituationJobId job_id) {
    if (job_id == 0) return true; // Ran inline at submit time
    if (!pool || !pool->is_active) return true;

    uint32_t q_idx = (job_id >> SIT_ID_QUEUE_SHIFT) & 1;
    uint32_t slot_idx = job_id & SIT_ID_SLOT_MASK;
    uint32_t expected_gen = (job_id >> SIT_ID_GEN_SHIFT) & SIT_ID_GEN_MASK;

    SituationJob* job = &pool->queues[q_idx].jobs[slot_idx];
    if (atomic_load(&job->generation) != (uint16_t)expected_gen) return true;
    return atomic_load(&job->is_completed);
}

SITAPI SituationError SituationWaitForJob(SituationThreadPool* pool, SituationJobId job_id) {
    SIT_ASSERT_MAIN_THREAD();
    if (job_id == 0) return SITUATION_SUCCESS; // Immediate jobs (Run-Inline) are implicitly done

    uint32_t q_idx = (job_id >> SIT_ID_QUEUE_SHIFT) & 1;
    uint32_t slot_idx = job_id & SIT_ID_SLOT_MASK;
    uint32_t expected_gen = (job_id >> SIT_ID_GEN_SHIFT) & SIT_ID_GEN_MASK;

    // Direct pointer access to the slot (safe because array doesn't move)
    SituationJob* job = &pool->queues[q_idx].jobs[slot_idx];

    while (true) {
        uint16_t current_gen = atomic_load(&job->generation);

        // O(1) Status Logic:
        // 1. If generation has incremented, the slot was reused for a NEW job -> OLD job is definitely done.
        // 2. If generation matches, check the explicit completion flag.

        if (current_gen != (uint16_t)expected_gen) return SITUATION_SUCCESS;
        if (atomic_load(&job->is_completed)) return SITUATION_SUCCESS;

        // Job not done. Yield CPU politely.
        // We don't use a condition var here to avoid N^2 condition/mutex pairs.
        // Active polling with yield is standard for game tasks waiting <1 frame.
        // [Patch 1] Use thrd_yield() instead of thrd_sleep() -- avoids tinycthread
        // bug on Windows while providing the same cooperative yielding behavior.
        thrd_yield();
    }
}

// --- Parallel Dispatch Implementation ---

/**
 * @brief [INTERNAL] Context for parallel dispatch jobs.
 * @details Carries the loop range and synchronization counter for a batch of work.
 */
typedef struct {
    void (*user_func)(int, void*);
    void* user_data;
    atomic_int* counter; // Shared counter across batch
    int start_idx;
    int end_idx;
} _SitParallelCtx;

/**
 * @brief [INTERNAL] Worker function for parallel loop execution (fork-join style).
 *
 * @details This function is executed on worker threads from the thread pool when
 *          `SituationDispatchParallel` is called. It processes a contiguous range
 *          of indices from the total loop count, invoking the user-provided loop
 *          body function for each index in the assigned batch.
 *
 *          The function receives its work range and user data via the job payload
 *          (`_SitParallelDispatchCtx`). It processes items in a simple for-loop,
 *          minimizing overhead and ensuring good cache locality within each batch.
 *
 *          Designed as the leaf worker for `SituationDispatchParallel`, which
 *          automatically splits the iteration space into batches sized according
 *          to `min_batch_size` and the number of available workers.
 *
 * Key responsibilities:
 *   - Receives start/end index range and user callback from job context
 *   - Calls the user-provided function `func(index, user_data)` for each index
 *     in the assigned range [start, end)
 *   - No return value -- completion is signaled via the job system
 *
 * Thread safety invariants:
 *   - Each invocation processes a disjoint range of indices (no overlap)
 *   - User callback `func` must be thread-safe if it modifies shared data
 *   - No locks held inside the worker -- assumes user handles synchronization
 *   - Runs only on pool worker threads (never main or render thread)
 *
 * @param data Pointer to the `_SitParallelDispatchCtx` structure (embedded or
 *             heap-allocated in the job submission payload). Contains:
 *               - start index
 *               - end index (exclusive)
 *               - user callback function pointer
 *               - user_data pointer passed to each invocation
 * @param unused Unused second argument (conforms to `SituationSubmitJobEx` signature)
 *
 * @note This is a high-throughput worker intended for data-parallel workloads
 *       (e.g. image processing, particle updates, batch asset loading).
 *       Performance depends heavily on the granularity chosen in `SituationDispatchParallel`
 *       (via `min_batch_size`); too small -> overhead dominates, too large -> poor load balance.
 *
 * @see SituationDispatchParallel, _SitParallelDispatchCtx,
 *      SituationSubmitJobEx, SituationThreadPool
 */
/* HARDENING: void by design — thread-pool parallel dispatch ABI. */
static void _SitParallelWorker(void* data, void* ctx) {
    (void)ctx;
    _SitParallelCtx* pctx = (_SitParallelCtx*)data;
    // Execute loop range
    for (int i = pctx->start_idx; i < pctx->end_idx; ++i) {
        pctx->user_func(i, pctx->user_data);
    }
    // Decrement shared counter
    atomic_fetch_sub(pctx->counter, 1);
}

/**
 * @brief Executes a parallel loop (Fork-Join) across the worker threads.
 *
 * @details Splits a workload of `count` items into efficient batches and distributes them to the thread pool.
 *          This is ideal for data-parallel tasks like particle updates, culling, or image processing.
 *
 *          **"Helping" Strategy:**
 *          While waiting for the workers to finish the batches, the calling thread (usually Main) does not sleep.
 *          Instead, it actively "steals" and executes jobs from the High Priority queue. This maximizes CPU utilization
 *          and prevents the main thread from becoming a bottleneck.
 *
 * @param pool The thread pool instance.
 * @param count The total number of items to process (iterations).
 * @param min_batch_size The minimum number of items per batch.
 *                       - Prevents overhead from creating too many tiny jobs.
 *                       - A good rule of thumb is a size that takes at least 10-50 microseconds to execute.
 * @param func The user callback function. It will be called for every index `i` from 0 to `count - 1`.
 *             Prototype: `void func(int index, void* user_data)`.
 * @param user_data A pointer passed to the callback (e.g., the array being processed).
 */
SITAPI void SituationDispatchParallel(SituationThreadPool* pool, int count, int min_batch_size, void (*func)(int, void*), void* user_data) {
    SIT_ASSERT_MAIN_THREAD();
    if (!pool) { _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationDispatchParallel: pool is NULL"); return; }
    if (count <= 0) return;

    atomic_fetch_add(&pool->stats_dispatch_parallel_calls, 1);

    // 1. Calculate Chunking
    int thread_count = (int)pool->thread_count;
    // Heuristic: Aim for 2 batches per thread to allow load balancing
    int batch_size = count / (thread_count * 2);
    if (batch_size < min_batch_size) batch_size = min_batch_size;
    if (batch_size < 1) batch_size = 1;

    int num_batches = (count + batch_size - 1) / batch_size;

    // 2. Setup Sync Counter
    // Stack allocated atomic is safe because this function blocks until 0.
    atomic_int completion_counter;
    atomic_init(&completion_counter, num_batches);

    // 3. Submit Batches
    for (int i = 0; i < num_batches; ++i) {
        int start = i * batch_size;
        int end = start + batch_size;
        if (end > count) end = count;

        _SitParallelCtx ctx;
        ctx.user_func = func;
        ctx.user_data = user_data;
        ctx.counter = &completion_counter;
        ctx.start_idx = start;
        ctx.end_idx = end;

        // Important: Use SIT_SUBMIT_RUN_IF_FULL.
        // If queue is full, we run it here immediately. This prevents deadlocks
        // and ensures progress even under heavy load.
        // Using High Priority ensures workers pick these up ASAP.
        SituationSubmitJobEx(pool, _SitParallelWorker, &ctx, sizeof(_SitParallelCtx), (SituationJobFlags)(SIT_SUBMIT_HIGH_PRIORITY | SIT_SUBMIT_RUN_IF_FULL));
    }

    // 4. Helping Loop (Work Stealing)
    // While waiting for the batches to finish, the main thread shouldn't sleep.
    // It should help process the High Priority queue to clear the blockage.
    while (atomic_load(&completion_counter) > 0) {
        bool stole_work = false;

        // Peek into High Priority Queue (Non-blocking try)
        int try_result = mtx_trylock(&pool->queues[1].lock);
        if (try_result == thrd_success) {
            SituationJob* job_ptr = NULL;
            if (_SitWorkerTryClaimReadyJob(pool, 1, &job_ptr) && job_ptr) {
                mtx_unlock(&pool->queues[1].lock);

                while (_SitJobDepCount(atomic_load(&job_ptr->dependency_count)) != 0) {
                    if (atomic_load(&pool->shutdown)) break;
                    thrd_yield();
                }
                // Execute Stolen Job
                void* d = job_ptr->uses_large_data ? job_ptr->large_data_ptr : job_ptr->storage;
                if (job_ptr->func) job_ptr->func(d, NULL);

                uint32_t cont_id = atomic_load(&job_ptr->continuation_id);
                if (cont_id != 0) {
                    SituationJob* next_job = _SitGetJobFromId(pool, cont_id);
                    if (next_job) {
                        if (_SitJobDecrementDependency(next_job)) {
                            cnd_signal(&pool->wake_condition);
                        }
                    }
                }

                // [Safety] Free copied data if owned (parity with worker path)
                if (job_ptr->owns_memory && job_ptr->large_data_ptr) {
                    SIT_FREE(job_ptr->large_data_ptr);
                    job_ptr->large_data_ptr = NULL;
                    job_ptr->owns_memory = false;
                }

                // [FIX] Generation before is_completed — see worker completion comment.
                uint16_t g = atomic_load(&job_ptr->generation);
                atomic_store(&job_ptr->generation, (uint16_t)((g + 1) & SIT_ID_GEN_MASK));
                atomic_store(&job_ptr->is_completed, true);
                atomic_fetch_sub(&pool->active_jobs, 1);

                atomic_fetch_add(&pool->stats_jobs_completed, 1);
                atomic_fetch_add(&pool->stats_main_steal_success, 1);
                stole_work = true;
            } else {
                size_t head = atomic_load(&pool->queues[1].head);
                size_t tail = atomic_load(&pool->queues[1].tail);
                if (tail == head) {
                    atomic_fetch_add(&pool->stats_main_steal_empty_queue, 1);
                }
                mtx_unlock(&pool->queues[1].lock);
            }
        } else {
            atomic_fetch_add(&pool->stats_main_steal_fail, 1);
        }

        if (!stole_work) {
            thrd_yield(); // Nothing to steal, brief yield
        }
    }
    // All batches done.
}

/**
 * @brief Blocks until all submitted jobs in the pool have finished.
 *
 * @details Acts as a global synchronization barrier. It waits for both High and Low priority queues to drain completely
 *          and for all currently running worker threads to finish their tasks.
 *
 *          Useful for:
 *          - End-of-frame synchronization.
 *          - Ensuring all assets are loaded before switching scenes.
 *          - Clean shutdown.
 *
 * @param pool The thread pool instance.
 */
SITAPI void SituationWaitForAllJobs(SituationThreadPool* pool) {
    if (!pool || !pool->is_active) { if (!pool) _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationWaitForAllJobs: pool is NULL"); return; }

    // We use the Low Priority queue lock for the idle condition
    mtx_lock(&pool->queues[0].lock);
    while (atomic_load(&pool->active_jobs) > 0) {
        cnd_wait(&pool->idle_condition, &pool->queues[0].lock);
    }
    mtx_unlock(&pool->queues[0].lock);
}

/**
 * @brief Shuts down the thread pool and releases all resources.
 *
 * @details 1. Sets a shutdown flag to signal all worker threads to exit.
 *          2. Wakes all sleeping threads.
 *          3. Joins all worker threads (waits for them to terminate).
 *          4. Destroys internal mutexes, condition variables, and ring buffer memory.
 *
 *          **Note:** Any jobs still pending in the queue will be discarded and NOT executed.
 *
 * @param pool Pointer to the `SituationThreadPool` to destroy.
 */
SITAPI void SituationDestroyThreadPool(SituationThreadPool* pool) {
    if (!pool || !pool->is_active) { if (!pool) _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationDestroyThreadPool: pool is NULL"); return; }

    atomic_store(&pool->shutdown, true);

    // Wake everyone up so they can exit
    cnd_broadcast(&pool->wake_condition);

    for (size_t i = 0; i < pool->thread_count; ++i) {
        if (thrd_join(pool->threads[i], NULL) != thrd_success) {
            _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_JOIN_FAILED,
                "SituationDestroyThreadPool: thrd_join failed for worker thread");
            /* Non-fatal: continue cleanup regardless */
        }
    }

    if (pool->io_thread) {
        if (thrd_join(pool->io_thread, NULL) != thrd_success) {
            _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_JOIN_FAILED,
                "SituationDestroyThreadPool: thrd_join failed for I/O thread");
            /* Non-fatal: continue cleanup regardless */
        }
        pool->io_thread = 0;
    }

    mtx_destroy(&pool->queues[0].lock);
    mtx_destroy(&pool->queues[1].lock);
    cnd_destroy(&pool->wake_condition);
    cnd_destroy(&pool->idle_condition);

    SIT_FREE(pool->queues[0].jobs);
    SIT_FREE(pool->queues[1].jobs);

    pool->is_active = false;
}

/**
 * @brief Returns a pointer to the library's internal thread pool.
 * @return Pointer to the active pool, or NULL if threading is not initialized.
 */
SITAPI SituationThreadPool* SituationGetInternalThreadPool(void) {
    if (_sit_current_context == NULL) return NULL;
    if (!sit_gs.thread_pool.is_active) return NULL;
    return &sit_gs.thread_pool;
}

#endif // SITUATION_ENABLE_THREADING
#endif // SITUATION_IMPL_THREADING_H
