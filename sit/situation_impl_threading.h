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

// ==================================================================================
//  Threading Implementation (v2.3.15+patch)
// ==================================================================================

// [Patch 3] Maximum slots to scan past a blocked tail job before giving up
#define SIT_WORKER_SCAN_DEPTH 8


/**
 * @brief Returns the number of logical CPU cores (threads) available.
 * @note Full implementation lives in `situation_impl_io.h` (included after this file).
 */
SITAPI uint32_t SituationGetCPUThreadCount(void);

/**
 * @brief Returns the number of physical CPU cores (ignoring Hyper-Threading).
 *        Uses GetLogicalProcessorInformation on Windows.
 */
SITAPI uint32_t SituationGetCPUCoreCount(void) {
#if defined(_WIN32)
    DWORD returnLength = 0;
    // First call gets the required buffer size
    GetLogicalProcessorInformation(NULL, &returnLength);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return SituationGetCPUThreadCount();

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)SIT_MALLOC(returnLength);
    if (!buffer) return SituationGetCPUThreadCount();

    // Second call actually fills the buffer
    if (!GetLogicalProcessorInformation(buffer, &returnLength)) {
        SIT_FREE(buffer);
        return SituationGetCPUThreadCount();
    }

    uint32_t physical_cores = 0;
    DWORD ptrOffset = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* ptr = buffer;
    
    while (ptrOffset < returnLength) {
        // RelationProcessorCore represents a physical core
        if (ptr->Relationship == RelationProcessorCore) {
            physical_cores++;
        }
        ptrOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    
    SIT_FREE(buffer);
    return physical_cores > 0 ? physical_cores : SituationGetCPUThreadCount();

#elif defined(__APPLE__)
    int count;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.physicalcpu", &count, &size, NULL, 0) == 0) return (uint32_t)(count > 0 ? count : 1);
    return SituationGetCPUThreadCount();

#elif defined(__linux__)
    // Parsing /proc/cpuinfo for "core id" uniqueness is the standard Linux way,
    // but as a fast fallback we just return threads / 2 if hyperthreading is assumed.
    // For a robust implementation, you'd parse /proc/cpuinfo.
    long threads = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)(threads > 1 ? threads / 2 : 1); 
#else
    return SituationGetCPUThreadCount();
#endif
}

/**
 * @brief Pins the calling thread to specific logical cores.
 * @param core_mask A bitmask where bit 0 is core 0, bit 1 is core 1, etc.
 *                  (e.g., 0x01 pins to core 0, 0x03 pins to cores 0 and 1).
 * @return true on success, false on failure.
 */
SITAPI bool SituationSetThreadAffinity(uint64_t core_mask) {
    if (core_mask == 0) return false;

#if defined(_WIN32)
    // GetCurrentThread() returns a pseudo-handle for the calling thread
    HANDLE thread = GetCurrentThread();
    
    // SetThreadAffinityMask returns 0 on failure, or the previous mask on success
    DWORD_PTR result = SetThreadAffinityMask(thread, (DWORD_PTR)core_mask);
    
    if (result == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SetThreadAffinityMask failed.");
        return false;
    }
    return true;

#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i < 64; i++) {
        if ((core_mask & (1ULL << i)) != 0) {
            CPU_SET(i, &cpuset);
        }
    }
    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset) == 0;

#elif defined(__APPLE__)
    // macOS does not support strict thread affinity (it uses Mach thread policy hints instead).
    // Returning false or true is a design choice. True allows the code to proceed gracefully.
    return true; 
#else
    return false;
#endif
}

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
SITAPI bool SituationAddJobDependency(SituationThreadPool* pool, SituationJobId prereq_id, SituationJobId dep_id) {
    if (!pool) return false;

    // 1. Validate Handles
    SituationJob* prereq = _SitGetJobFromId(pool, prereq_id);
    SituationJob* dep = _SitGetJobFromId(pool, dep_id);

    if (!prereq || !dep) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "Invalid job IDs in AddJobDependency (jobs may have finished).");
        return false;
    }

    // 2. Cycle Detection (Read-only traversal, safe-ish without lock if topology is stable-ish)
    uint8_t new_depth = 0;
    if (_SituationDetectCycle(pool, prereq_id, dep_id, &new_depth)) {
        #ifndef NDEBUG
        fprintf(stderr, "[Situation] ERROR: Dependency Cycle Detected! Job 0x%x -> 0x%x causes loop.\n", prereq_id, dep_id);
        #endif
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_CYCLE, "Cycle detected or depth limit (32) exceeded.");
        return false;
    }

    // 3. Link via CAS (Compare-And-Swap)
    // We only allow one continuation per job in this lightweight system.
    uint32_t expected_cont = 0;
    if (atomic_compare_exchange_strong_explicit(&prereq->continuation_id, &expected_cont, dep_id, memory_order_seq_cst, memory_order_seq_cst)) {
        // Successfully linked prereq -> dep
        atomic_fetch_add(&dep->dependency_count, 1);
        dep->dep_depth = new_depth;
        return true;
    } else {
        // Prerequisite already has a continuation
        _SituationSetErrorFromCode(SITUATION_ERROR_THREAD_QUEUE_FULL, "Prerequisite job already has a continuation (1:1 limit). Use SituationAddJobDependencies for fan-in.");
        return false;
    }
    return false;
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
SITAPI bool SituationAddJobDependencies(SituationThreadPool* pool, SituationJobId* prerequisites, int count, SituationJobId dependent_job) {
    for (int i = 0; i < count; ++i) {
        // Note: The parameter order in AddJobDependency is (Prereq, Dependent)
        if (!SituationAddJobDependency(pool, prerequisites[i], dependent_job)) {
            return false; // Fail fast if any link fails
        }
    }
    return true;
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

/**
 * @brief Returns the current number of pending asynchronous I/O jobs in the thread pool.
 *
 * @details Queries the combined depth of the high-priority and low-priority queues
 *          in the given thread pool, giving an indication of how many async file load/save
 *          or other I/O-bound jobs are waiting to be processed by worker threads.
 *
 *          This is a lightweight, atomic read -- it does not block or lock.
 *          Useful for:
 *            - Monitoring system load / backpressure
 *            - Deciding whether to submit more jobs or throttle
 *            - Debugging or displaying queue health in tools/profilers
 *
 * @param pool Pointer to an initialized `SituationThreadPool` (must have been created
 *             with `SituationCreateThreadPool` and not yet destroyed).
 *             Passing NULL or an invalid pool results in 0 being returned.
 *
 * @return The total number of jobs currently enqueued (high + low priority).
 *         This count includes jobs that are:
 *           - Waiting to be picked up by a worker
 *           - Being processed (popped but not yet completed)
 *         It does **not** include completed jobs or jobs that failed to submit.
 *
 * @note This is a snapshot-in-time value -- the actual queue may change immediately
 *       after the call returns. For precise synchronization, combine with
 *       `SituationWaitForAllJobs` or `SituationWaitForJob`.
 *
 * @see SituationThreadPool, SituationCreateThreadPool, SituationSubmitJobEx,
 *      SituationWaitForAllJobs, SituationWaitForJob
 */
// ==================================================================================
//  Worker Thread Implementation (Updated)
// ==================================================================================

/**
 * @brief [INTERNAL] Main entry point / infinite loop for each worker thread in the thread pool.
 *
 * @details This function is the body of every worker thread created by `SituationCreateThreadPool`.
 *          Each worker runs this loop indefinitely until shutdown is requested, waiting for
 *          available jobs in either the high- or low-priority queue.
 *
 *          The worker uses a condition variable to sleep efficiently when both queues are empty.
 *          When woken, it atomically pops the highest-priority pending job (high first, then low),
 *          executes the user-provided function with its payload, and notifies any dependent jobs
 *          or waiting threads upon completion.
 *
 * Key responsibilities:
 *   - Waits on `worker_cv` when no work is available
 *   - Prioritizes high-priority jobs over low-priority ones
 *   - Executes the job callback (`func(data, user_data)`)
 *   - Handles job completion signaling (increments done count, wakes dependents or waiters)
 *   - Checks shutdown flag periodically to exit cleanly
 *   - Maintains thread-local scratch space / state if needed
 *
 * Thread safety invariants:
 *   - Queue access is protected by `pool->queue_mutex`
 *   - Atomic operations used for refcounts, done flags, and shutdown detection
 *   - Job payload is owned by the submitter until popped -- worker does not free it
 *   - Multiple workers can run concurrently without interfering (disjoint jobs)
 *   - Safe to call from any thread context (but only pool workers invoke it)
 *
 * @param arg Pointer to the owning `SituationThreadPool*` structure (passed via thrd_create).
 *            The worker uses this to access shared queues, mutexes, condvars, and shutdown state.
 * @return 0 on clean exit (when shutdown is complete and queues are drained)
 *
 * @note This function never returns until the pool is being destroyed.
 *       Shutdown is cooperative: workers check `atomic_load(&pool->shutdown_requested)`
 *       after each job and during wait wakeups.
 *       See also: `SituationCreateThreadPool`, `SituationDestroyThreadPool`,
 *                 `pool->high_priority_queue`, `pool->low_priority_queue`,
 *                 `pool->worker_cv`, `pool->queue_mutex`
 */
static int _SituationWorkerEntry(void* arg) {
    SituationThreadPool* pool = (SituationThreadPool*)arg;

    while (!atomic_load(&pool->shutdown)) {
        SituationJob* job_ptr = NULL;
        int queue_idx = -1;

        // --- Job Picking Loop (Priority 1 -> 0) ---
        for (int q = 1; q >= 0; --q) {
            mtx_lock(&pool->queues[q].lock);

            size_t head = atomic_load(&pool->queues[q].head);
            size_t tail = atomic_load(&pool->queues[q].tail);

            if (tail != head) {
                // [Patch 3] Scan-forward: check up to SIT_WORKER_SCAN_DEPTH slots
                // past tail to find a ready job, mitigating head-of-line blocking
                // when the tail job has unmet dependencies.
                size_t pending = head - tail;
                size_t scan_limit = (pending < SIT_WORKER_SCAN_DEPTH) ? pending : SIT_WORKER_SCAN_DEPTH;
                bool found_ready = false;

                for (size_t scan = 0; scan < scan_limit; ++scan) {
                    size_t idx = (tail + scan) & pool->queues[q].mask;
                    SituationJob* candidate = &pool->queues[q].jobs[idx];

                    if (atomic_load(&candidate->dependency_count) == 0 &&
                        !atomic_load(&candidate->is_completed)) {
                        // Found a ready job. If it's not at the tail, swap it there.
                        if (scan > 0) {
                            size_t tail_idx = tail & pool->queues[q].mask;
                            SituationJob tmp = pool->queues[q].jobs[tail_idx];
                            pool->queues[q].jobs[tail_idx] = *candidate;
                            *candidate = tmp;
                        }
                        job_ptr = &pool->queues[q].jobs[tail & pool->queues[q].mask];
                        atomic_store(&pool->queues[q].tail, tail + 1);
                        queue_idx = q;
                        found_ready = true;
                        break;
                    }
                }

                if (!found_ready) {
                    mtx_unlock(&pool->queues[q].lock);
                    thrd_yield();
                    continue; // Try next priority queue
                }

                mtx_unlock(&pool->queues[q].lock);
                break; // Stop searching, we found work
            } else {
                mtx_unlock(&pool->queues[q].lock);
            }
        }

        // If no job found in either queue
        if (!job_ptr) {
            mtx_lock(&pool->queues[0].lock); // Lock low prio for condition var
            // Double check to prevent race where signal came before wait
            size_t head = atomic_load(&pool->queues[0].head);
            size_t tail = atomic_load(&pool->queues[0].tail);
            // Also check High prio emptiness? Ideally yes, but for simplicity we sleep on Low lock.
            // Real robustness would use a dedicated condition mutex.
            if (head == tail && !atomic_load(&pool->shutdown)) {
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
            while (atomic_load(&job_ptr->dependency_count) != 0) {
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
                    // Decrement dependency count
                    int remaining = atomic_fetch_sub(&next_job->dependency_count, 1);
                    if (remaining == 1) {
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

            atomic_store(&job_ptr->is_completed, true);

            // Increment generation to invalidate handle
            uint16_t old_gen = atomic_load(&job_ptr->generation);
            atomic_store(&job_ptr->generation, (uint16_t)((old_gen + 1) & SIT_ID_GEN_MASK));

            // Decrement global active count
            if (atomic_fetch_sub(&pool->active_jobs, 1) == 1) {
                cnd_broadcast(&pool->idle_condition); // Wake Main Thread (WaitForAll)
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
SITAPI bool SituationCreateThreadPool(SituationThreadPool* pool, size_t num_threads, size_t queue_size, double hot_reload_rate, bool disable_io) {
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
        return false;
    }
    memset(pool, 0, sizeof(SituationThreadPool));

    // Auto-detect threads if 0
    if (num_threads == 0) {
        num_threads = (size_t)SituationGetCPUThreadCount();
        num_threads = (num_threads > 1) ? num_threads - 1 : 1; // Leave one for main
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
            return false;
        }

        mtx_init(&pool->queues[i].lock, mtx_plain);
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
        if (thrd_create(&pool->threads[i], _SituationWorkerEntry, pool) != thrd_success) {
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
            return false;
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
            return false;
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
    return true;
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
    if (!pool || !pool->is_active) return 0;

    int q_idx = (flags & SIT_SUBMIT_HIGH_PRIORITY) ? 1 : 0;

    mtx_lock(&pool->queues[q_idx].lock);

    // [Patch 7] Inline Fallback if I/O Thread Disabled (Queue 0 Only)
    // When no I/O thread exists, low-priority jobs are executed immediately on the
    // calling thread. Returns 0 which is semantically "already complete" (same as
    // SIT_SUBMIT_RUN_IF_FULL behavior). Callers already handle 0 as "no handle needed."
    if (q_idx == 0 && pool->io_thread == 0) {
        mtx_unlock(&pool->queues[q_idx].lock);
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

        if (head - tail >= pool->queues[q_idx].capacity) {
            mtx_unlock(&pool->queues[q_idx].lock);

            // Handle Backpressure
            if (flags & SIT_SUBMIT_RUN_IF_FULL) {
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
                // "Robust" Path: Spin-wait (polite yield) until slot opens
                // [Patch 1] Use SITUATION_SLEEP_MS instead of thrd_sleep to avoid
                // tinycthread hang on Windows. Sleep(0) yields the timeslice.
                SITUATION_SLEEP_MS(0);

                // Re-acquire lock and try again
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
SITAPI bool SituationWaitForJob(SituationThreadPool* pool, SituationJobId job_id) {
    SIT_ASSERT_MAIN_THREAD();
    if (job_id == 0) return true; // Immediate jobs (Run-Inline) are implicitly done

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

        if (current_gen != (uint16_t)expected_gen) return true;
        if (atomic_load(&job->is_completed)) return true;

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
        if (mtx_trylock(&pool->queues[1].lock) == thrd_success) {
            size_t head = atomic_load(&pool->queues[1].head);
            size_t tail = atomic_load(&pool->queues[1].tail);

            if (tail != head) {
                size_t idx = tail & pool->queues[1].mask;
                SituationJob* job_ptr = &pool->queues[1].jobs[idx];

                // [Patch 2] Check dependencies before stealing -- prevents premature
                // execution of jobs that have unmet prerequisites in the high queue.
                if (atomic_load(&job_ptr->dependency_count) > 0) {
                    mtx_unlock(&pool->queues[1].lock);
                    thrd_yield();
                    continue;
                }

                // Steal it!
                atomic_store(&pool->queues[1].tail, tail + 1);
                mtx_unlock(&pool->queues[1].lock);

                while (atomic_load(&job_ptr->dependency_count) != 0) {
                    if (atomic_load(&pool->shutdown)) break;
                    thrd_yield();
                }
                // Execute Stolen Job
                void* d = job_ptr->uses_large_data ? job_ptr->large_data_ptr : job_ptr->storage;
                if (job_ptr->func) job_ptr->func(d, NULL);

                atomic_store(&job_ptr->is_completed, true);
                uint16_t g = atomic_load(&job_ptr->generation);
                atomic_store(&job_ptr->generation, (uint16_t)((g + 1) & SIT_ID_GEN_MASK));
                atomic_fetch_sub(&pool->active_jobs, 1);

                stole_work = true;
            } else {
                mtx_unlock(&pool->queues[1].lock);
            }
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
        thrd_join(pool->threads[i], NULL);
    }

    if (pool->io_thread) {
        thrd_join(pool->io_thread, NULL);
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


#endif // SITUATION_ENABLE_THREADING
#endif // SITUATION_IMPL_THREADING_H
