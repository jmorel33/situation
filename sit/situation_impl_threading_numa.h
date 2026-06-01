/*
 * situation_impl_threading_numa.h - NUMA topology & placement (Epic C)
 * (c) 2025-2026 Jacques Morel
 * MIT Licensed
 *
 * Included from situation_impl.h after situation_impl_threading_topology.h.
 */

#ifndef SITUATION_IMPL_THREADING_NUMA_H
#define SITUATION_IMPL_THREADING_NUMA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <dirent.h>
#endif

#define SIT_NUMA_NODE_UNSET (-1)

static SituationNumaTopology g_sit_numa_topology;
static bool g_sit_numa_topology_valid = false;

#if defined(_MSC_VER)
    static __declspec(thread) int s_sit_tls_preferred_numa_node = SIT_NUMA_NODE_UNSET;
#else
    static _Thread_local int s_sit_tls_preferred_numa_node = SIT_NUMA_NODE_UNSET;
#endif

static void _SitSetPreferredNumaNode(int node) {
    s_sit_tls_preferred_numa_node = node;
}

static int _SitNodeForLogicalCpu(int logical_cpu) {
    if (logical_cpu < 0) return 0;
    const SituationCpuTopology* topo = NULL;
    if (!SituationGetCpuTopology(&topo) || !topo) return 0;
    if ((uint32_t)logical_cpu >= topo->logical_count) return 0;
    return (int)topo->processors[logical_cpu].numa_node;
}

#if defined(_WIN32)
static void _SitFetchNumaMemoryWindows(SituationNumaTopology* numa) {
    typedef BOOL (WINAPI *SitGetNumaAvailableMemoryNodeFn)(USHORT, PULONGLONG);
    static SitGetNumaAvailableMemoryNodeFn pfn = NULL;
    static bool resolved = false;
    if (!resolved) {
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        if (k32) {
            pfn = (SitGetNumaAvailableMemoryNodeFn)GetProcAddress(k32, "GetNumaAvailableMemoryNode");
        }
        resolved = true;
    }
    if (!pfn) return;

    for (uint16_t n = 0; n < numa->node_count; ++n) {
        ULONGLONG bytes = 0;
        if (pfn((USHORT)n, &bytes)) {
            numa->nodes[n].memory_bytes = (uint64_t)bytes;
        }
    }
}
#elif defined(__linux__)
static bool _SitReadNumaMemTotalLinux(unsigned int node, uint64_t* out_bytes) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/devices/system/node/node%u/meminfo", node);
    FILE* f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned long long kb = 0;
        if (sscanf(line, "MemTotal: %llu kB", &kb) == 1) {
            *out_bytes = (uint64_t)kb * 1024ULL;
            fclose(f);
            return true;
        }
    }
    fclose(f);
    return false;
}

static uint16_t _SitCountLinuxNumaNodes(void) {
    DIR* dir = opendir("/sys/devices/system/node");
    if (!dir) return 1;

    uint16_t max_node = 0;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "node", 4) != 0) continue;
        unsigned int id = 0;
        if (sscanf(ent->d_name, "node%u", &id) == 1 && id > max_node) {
            max_node = (uint16_t)id;
        }
    }
    closedir(dir);
    return (uint16_t)(max_node + 1);
}
#endif

static bool _SitRebuildNumaFromCpuTopology(SituationNumaTopology* numa) {
    const SituationCpuTopology* cpu = NULL;
    if (!SituationGetCpuTopology(&cpu) || !cpu || cpu->logical_count == 0) {
        return false;
    }

    memset(numa, 0, sizeof(*numa));
    uint16_t max_node = 0;
    for (uint32_t i = 0; i < cpu->logical_count; ++i) {
        uint16_t node = cpu->processors[i].numa_node;
        if (node >= SITUATION_MAX_NUMA_NODES) continue;
        if (node > max_node) max_node = node;
        numa->nodes[node].node_id = node;
        numa->nodes[node].processor_count++;
        if (i < SITUATION_AFFINITY_MASK_BITS) {
            numa->nodes[node].processor_mask_low |= (1ULL << i);
        }
    }

    numa->node_count = (uint16_t)(max_node + 1);
    if (numa->node_count == 0) {
        numa->node_count = 1;
        numa->nodes[0].node_id = 0;
        numa->nodes[0].processor_count = cpu->logical_count;
    }

#if defined(_WIN32)
    ULONG highest = 0;
    if (GetNumaHighestNodeNumber(&highest)) {
        uint16_t win_nodes = (uint16_t)(highest + 1);
        if (win_nodes > numa->node_count) {
            numa->node_count = win_nodes;
        }
    }
    _SitFetchNumaMemoryWindows(numa);
#elif defined(__linux__)
    uint16_t linux_nodes = _SitCountLinuxNumaNodes();
    if (linux_nodes > numa->node_count) {
        numa->node_count = linux_nodes;
    }
    for (uint16_t n = 0; n < numa->node_count; ++n) {
        numa->nodes[n].node_id = n;
        uint64_t mem = 0;
        if (_SitReadNumaMemTotalLinux(n, &mem)) {
            numa->nodes[n].memory_bytes = mem;
        }
    }
#endif

    return true;
}

// ==================================================================================
// Public API — NUMA topology (C1)
// ==================================================================================

SITAPI bool SituationRefreshNumaTopology(void) {
    if (!g_sit_cpu_topology_valid) {
        SituationRefreshCpuTopology();
    }
    g_sit_numa_topology_valid = _SitRebuildNumaFromCpuTopology(&g_sit_numa_topology);
    return g_sit_numa_topology_valid;
}

SITAPI bool SituationGetNumaTopology(const SituationNumaTopology** out_topology) {
    if (!out_topology) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetNumaTopology: out_topology is NULL");
        return false;
    }
    if (!g_sit_numa_topology_valid) {
        SituationRefreshNumaTopology();
    }
    *out_topology = &g_sit_numa_topology;
    return g_sit_numa_topology_valid;
}

SITAPI int SituationGetPreferredNumaNode(void) {
    return s_sit_tls_preferred_numa_node;
}

// ==================================================================================
// Placement policy (C2 / C3)
// ==================================================================================

static bool _SituationSetThreadAffinityForRole(SituationThreadRole role, uint64_t explicit_mask) {
    uint64_t mask = explicit_mask;

    if (mask == 0 && _sit_current_context != NULL) {
        switch (role) {
            case SIT_THREAD_ROLE_MAIN:
                mask = SituationGetConfiguredMainThreadAffinity();
                break;
            case SIT_THREAD_ROLE_RENDER:
                mask = SituationGetConfiguredRenderThreadAffinity();
                break;
            case SIT_THREAD_ROLE_AUDIO:
                mask = SituationGetConfiguredAudioThreadAffinity();
                break;
            default:
                break;
        }
    }

    if (mask == 0) {
        return false;
    }

    if (SituationSetThreadAffinityEx(mask, NULL)) {
        int cpu = SituationGetCurrentProcessorIndex();
        _SitSetPreferredNumaNode(_SitNodeForLogicalCpu(cpu));
        return true;
    }

    /* Fail-soft: affinity is best-effort; never abort SituationInit for a pin failure. */
#if defined(SITUATION_DEBUG) || defined(SITUATION_DEBUG_THREADING)
    fprintf(stderr,
        "[Situation] Warning: thread affinity failed for role %d (mask=0x%llX)\n",
        (int)role, (unsigned long long)mask);
#endif
    return false;
}

static void _SituationApplyWorkerNumaPlacement(SituationThreadPool* pool, size_t worker_index) {
    (void)pool;
    if (_sit_current_context == NULL || !sit_gs.worker_numa_spread) {
        return;
    }

    const SituationNumaTopology* numa = NULL;
    if (!SituationGetNumaTopology(&numa) || !numa || numa->node_count == 0) {
        return;
    }

    if (numa->node_count > 1) {
        // Multi-NUMA: spread workers across NUMA nodes (original behavior)
        int node = (int)(worker_index % (size_t)numa->node_count);
        uint64_t mask = SituationBuildNumaNodeMask(node);
        if (mask != 0) {
            SituationSetThreadAffinityEx(mask, NULL);
            _SitSetPreferredNumaNode(node);
            if (worker_index < SITUATION_MAX_THREADS) {
                atomic_store(&pool->worker_last_logical_cpu[worker_index], SituationGetCurrentProcessorIndex());
            }
        }
    } else {
        // Single-NUMA: pin each worker to a distinct physical core for true spread.
        // Use SituationBuildUniqueCoreMask to get one logical CPU per physical core,
        // then assign worker i to core (i % physical_count).
        const SituationCpuTopology* topo = NULL;
        if (!SituationGetCpuTopology(&topo) || !topo || topo->physical_count == 0) {
            return;
        }
        int core_idx = (int)(worker_index % (size_t)topo->physical_count);
        uint64_t mask = SituationBuildPhysicalCoreMask(core_idx);
        if (mask != 0) {
            SituationSetThreadAffinityEx(mask, NULL);
            _SitSetPreferredNumaNode(0);
            if (worker_index < SITUATION_MAX_THREADS) {
                atomic_store(&pool->worker_last_logical_cpu[worker_index], SituationGetCurrentProcessorIndex());
            }
        }
    }
}

static void _SituationApplyIoThreadNumaPlacement(SituationThreadPool* pool) {
    (void)pool;
    if (_sit_current_context == NULL) {
        return;
    }

    int node = sit_gs.io_thread_numa_node;
    if (node < 0) {
        return;
    }

    uint64_t mask = SituationBuildNumaNodeMask(node);
    if (mask != 0) {
        SituationSetThreadAffinityEx(mask, NULL);
        _SitSetPreferredNumaNode(node);
        atomic_store(&pool->io_last_logical_cpu, SituationGetCurrentProcessorIndex());
    }
}

#endif /* SITUATION_IMPL_THREADING_NUMA_H */
