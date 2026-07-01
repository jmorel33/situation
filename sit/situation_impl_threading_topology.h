/*
 * situation_impl_threading_topology.h - CPU topology, affinity query/set, mask builders
 * (c) 2025-2026 Jacques Morel
 * MIT Licensed
 *
 * Epic A (Threading Bolstering): read-only topology cache, affinity feedback, HT/NUMA masks.
 * Included from situation_impl.h after situation_impl_io.h (needs SituationGetCPUThreadCount).
 */

#ifndef SITUATION_IMPL_THREADING_TOPOLOGY_H
#define SITUATION_IMPL_THREADING_TOPOLOGY_H

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__linux__)
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#endif

// ==================================================================================
// Cached topology (process-wide)
// ==================================================================================

static SituationCpuTopology g_sit_cpu_topology;
static bool g_sit_cpu_topology_valid = false;

static int _SitCountBits64(uint64_t mask) {
    int n = 0;
    while (mask) {
        n += (int)(mask & 1ULL);
        mask >>= 1;
    }
    return n;
}

#if defined(_WIN32)
static bool _SitRefreshTopologyWindows(SituationCpuTopology* topo) {
    DWORD returnLength = 0;
    GetLogicalProcessorInformation(NULL, &returnLength);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || returnLength == 0) {
        return false;
    }

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer =
        (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)SIT_MALLOC(returnLength);
    if (!buffer) {
        return false;
    }
    if (!GetLogicalProcessorInformation(buffer, &returnLength)) {
        SIT_FREE(buffer);
        return false;
    }

    uint32_t logical_count = SituationGetCPUThreadCount();
    if (logical_count > SITUATION_MAX_LOGICAL_PROCESSORS) {
        logical_count = SITUATION_MAX_LOGICAL_PROCESSORS;
    }

    memset(topo->processors, 0, sizeof(topo->processors));
    for (uint32_t i = 0; i < logical_count; ++i) {
        topo->processors[i].logical_id = i;
        topo->processors[i].numa_node = 0;
        topo->processors[i].physical_core_id = i;
        topo->processors[i].is_hyperthread_sibling = false;
    }

    uint32_t next_physical_id = 0;
    DWORD ptrOffset = 0;
    while (ptrOffset < returnLength) {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION* ptr =
            (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)((char*)buffer + ptrOffset);

        if (ptr->Relationship == RelationProcessorCore) {
            ULONG_PTR mask = ptr->ProcessorMask;
            int bits = _SitCountBits64((uint64_t)mask);
            bool ht = bits > 1;
            for (uint32_t i = 0; i < logical_count; ++i) {
                if (mask & ((ULONG_PTR)1 << i)) {
                    topo->processors[i].physical_core_id = next_physical_id;
                    topo->processors[i].is_hyperthread_sibling = ht;
                }
            }
            next_physical_id++;
        }

        if (ptr->Relationship == RelationNumaNode) {
            ULONG_PTR mask = ptr->ProcessorMask;
            uint16_t node = (uint16_t)ptr->NumaNode.NodeNumber;
            for (uint32_t i = 0; i < logical_count; ++i) {
                if (mask & ((ULONG_PTR)1 << i)) {
                    topo->processors[i].numa_node = node;
                }
            }
        }

        ptrOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    }

    SIT_FREE(buffer);

    topo->logical_count = logical_count;
    topo->physical_count = next_physical_id > 0 ? next_physical_id : logical_count;
    uint16_t max_node = 0;
    for (uint32_t i = 0; i < logical_count; ++i) {
        if (topo->processors[i].numa_node > max_node) {
            max_node = topo->processors[i].numa_node;
        }
    }
    topo->numa_node_count = (uint16_t)(max_node + 1);
    return true;
}
#elif defined(__linux__)
static bool _SitReadSysfsUint(const char* path, unsigned long* out) {
    FILE* f = fopen(path, "r");
    if (!f) {
        return false;
    }
    unsigned long v = 0;
    if (fscanf(f, "%lu", &v) != 1) {
        fclose(f);
        return false;
    }
    fclose(f);
    *out = v;
    return true;
}

static bool _SitRefreshTopologyLinux(SituationCpuTopology* topo) {
    long n_online = sysconf(_SC_NPROCESSORS_ONLN);
    if (n_online < 1) {
        return false;
    }

    uint32_t logical_count = (uint32_t)n_online;
    if (logical_count > SITUATION_MAX_LOGICAL_PROCESSORS) {
        logical_count = SITUATION_MAX_LOGICAL_PROCESSORS;
    }

    memset(topo->processors, 0, sizeof(topo->processors));

    typedef struct {
        unsigned long package;
        unsigned long core;
        int first_logical;
        int count;
    } _SitCoreKey;

    _SitCoreKey keys[SITUATION_MAX_LOGICAL_PROCESSORS];
    int key_count = 0;

    for (uint32_t cpu = 0; cpu < logical_count; ++cpu) {
        char path[256];
        unsigned long core_id = 0;
        unsigned long package_id = 0;
        unsigned long node_id = 0;

        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/topology/core_id", cpu);
        if (!_SitReadSysfsUint(path, &core_id)) {
            core_id = cpu;
        }
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/topology/physical_package_id", cpu);
        if (!_SitReadSysfsUint(path, &package_id)) {
            package_id = 0;
        }
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u", cpu);
        {
            char node_link[320];
            snprintf(node_link, sizeof(node_link), "/sys/devices/system/cpu/cpu%u/node0", cpu);
            char target[64];
            ssize_t rl = readlink(node_link, target, sizeof(target) - 1);
            if (rl > 0) {
                target[rl] = '\0';
                const char* node_tag = strstr(target, "node");
                if (node_tag) {
                    node_id = (unsigned long)strtoul(node_tag + 4, NULL, 10);
                }
            } else {
                node_id = package_id;
            }
        }

        topo->processors[cpu].logical_id = cpu;
        topo->processors[cpu].numa_node = (uint16_t)node_id;

        int found = -1;
        for (int k = 0; k < key_count; ++k) {
            if (keys[k].package == package_id && keys[k].core == core_id) {
                found = k;
                break;
            }
        }
        if (found < 0 && key_count < SITUATION_MAX_LOGICAL_PROCESSORS) {
            found = key_count++;
            keys[found].package = package_id;
            keys[found].core = core_id;
            keys[found].first_logical = (int)cpu;
            keys[found].count = 0;
        }
        if (found >= 0) {
            keys[found].count++;
            topo->processors[cpu].physical_core_id = (uint32_t)found;
        } else {
            topo->processors[cpu].physical_core_id = cpu;
        }
    }

    for (uint32_t cpu = 0; cpu < logical_count; ++cpu) {
        uint32_t pid = topo->processors[cpu].physical_core_id;
        if (pid < (uint32_t)key_count && keys[pid].count > 1) {
            topo->processors[cpu].is_hyperthread_sibling = true;
        }
    }

    topo->logical_count = logical_count;
    topo->physical_count = (uint32_t)key_count;
    if (topo->physical_count == 0) {
        topo->physical_count = logical_count;
    }

    uint16_t max_node = 0;
    for (uint32_t i = 0; i < logical_count; ++i) {
        if (topo->processors[i].numa_node > max_node) {
            max_node = topo->processors[i].numa_node;
        }
    }
    topo->numa_node_count = (uint16_t)(max_node + 1);
    return true;
}
#elif defined(__APPLE__)
static bool _SitRefreshTopologyMacOS(SituationCpuTopology* topo) {
    int logical = 0;
    int physical = 0;
    size_t sz = sizeof(logical);
    if (sysctlbyname("hw.logicalcpu", &logical, &sz, NULL, 0) != 0 || logical < 1) {
        logical = (int)SituationGetCPUThreadCount();
    }
    sz = sizeof(physical);
    if (sysctlbyname("hw.physicalcpu", &physical, &sz, NULL, 0) != 0 || physical < 1) {
        physical = logical > 1 ? logical / 2 : 1;
    }

    uint32_t logical_count = (uint32_t)logical;
    if (logical_count > SITUATION_MAX_LOGICAL_PROCESSORS) {
        logical_count = SITUATION_MAX_LOGICAL_PROCESSORS;
    }

    memset(topo->processors, 0, sizeof(topo->processors));
    bool ht = (uint32_t)physical < logical_count;
    for (uint32_t i = 0; i < logical_count; ++i) {
        topo->processors[i].logical_id = i;
        topo->processors[i].physical_core_id = ht ? (i % (uint32_t)physical) : i;
        topo->processors[i].numa_node = 0;
        topo->processors[i].is_hyperthread_sibling = ht;
    }

    topo->logical_count = logical_count;
    topo->physical_count = (uint32_t)physical;
    topo->numa_node_count = 1;
    return true;
}
#endif

static bool _SitRefreshTopologyInternal(SituationCpuTopology* topo) {
    memset(topo, 0, sizeof(*topo));

#if defined(_WIN32)
    if (_SitRefreshTopologyWindows(topo)) {
        return true;
    }
#elif defined(__linux__)
    if (_SitRefreshTopologyLinux(topo)) {
        return true;
    }
#elif defined(__APPLE__)
    if (_SitRefreshTopologyMacOS(topo)) {
        return true;
    }
#endif

    uint32_t logical_count = SituationGetCPUThreadCount();
    if (logical_count == 0) {
        logical_count = 1;
    }
    if (logical_count > SITUATION_MAX_LOGICAL_PROCESSORS) {
        logical_count = SITUATION_MAX_LOGICAL_PROCESSORS;
    }

    for (uint32_t i = 0; i < logical_count; ++i) {
        topo->processors[i].logical_id = i;
        topo->processors[i].physical_core_id = i;
        topo->processors[i].numa_node = 0;
        topo->processors[i].is_hyperthread_sibling = false;
    }
    topo->logical_count = logical_count;
    topo->physical_count = logical_count;
    topo->numa_node_count = 1;
    return true;
}

// ==================================================================================
// Public API — topology
// ==================================================================================

SITAPI SituationError SituationRefreshCpuTopology(void) {
    g_sit_cpu_topology_valid = _SitRefreshTopologyInternal(&g_sit_cpu_topology);
    if (!g_sit_cpu_topology_valid) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SituationRefreshCpuTopology: failed to query CPU topology");
        return SITUATION_ERROR_DEVICE_QUERY;
    }
    return SITUATION_SUCCESS;
}

SITAPI SituationError SituationGetCpuTopology(const SituationCpuTopology** out_topology) {
    if (!out_topology) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetCpuTopology: out_topology is NULL");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (!g_sit_cpu_topology_valid) {
        SituationRefreshCpuTopology();
    }
    *out_topology = &g_sit_cpu_topology;
    if (!g_sit_cpu_topology_valid) {
        return SITUATION_ERROR_DEVICE_QUERY;
    }
    return SITUATION_SUCCESS;
}

SITAPI uint32_t SituationGetCPUCoreCount(void) {
    if (!g_sit_cpu_topology_valid) {
        SituationRefreshCpuTopology();
    }
    if (g_sit_cpu_topology_valid && g_sit_cpu_topology.physical_count > 0) {
        return g_sit_cpu_topology.physical_count;
    }
    return SituationGetCPUThreadCount();
}

// ==================================================================================
// Affinity — query / set with optional previous mask
// ==================================================================================

SITAPI SituationError SituationGetThreadAffinity(uint64_t* out_mask) {
    if (!out_mask) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationGetThreadAffinity: out_mask is NULL");
        return SITUATION_ERROR_INVALID_PARAM;
    }
    *out_mask = 0;

#if defined(_WIN32)
    typedef BOOL (WINAPI *SitGetThreadGroupAffinityFn)(HANDLE, PGROUP_AFFINITY);
    static SitGetThreadGroupAffinityFn pfn_get_group = NULL;
    static bool pfn_resolved = false;
    if (!pfn_resolved) {
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        if (k32) {
            pfn_get_group = (SitGetThreadGroupAffinityFn)GetProcAddress(k32, "GetThreadGroupAffinity");
        }
        pfn_resolved = true;
    }
    if (pfn_get_group) {
        GROUP_AFFINITY group;
        memset(&group, 0, sizeof(group));
        if (pfn_get_group(GetCurrentThread(), &group)) {
            *out_mask = (uint64_t)group.Mask;
            return SITUATION_SUCCESS;
        }
    }
    DWORD_PTR process_mask = 0;
    DWORD_PTR system_mask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
        *out_mask = (uint64_t)process_mask;
        return SITUATION_SUCCESS;
    }
    _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SituationGetThreadAffinity failed.");
    return SITUATION_ERROR_DEVICE_QUERY;

#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    pthread_t self = pthread_self();
    if (pthread_getaffinity_np(self, sizeof(cpuset), &cpuset) != 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "pthread_getaffinity_np failed.");
        return SITUATION_ERROR_DEVICE_QUERY;
    }
    for (int i = 0; i < SITUATION_AFFINITY_MASK_BITS; ++i) {
        if (CPU_ISSET(i, &cpuset)) {
            *out_mask |= (1ULL << i);
        }
    }
    return SITUATION_SUCCESS;

#elif defined(__APPLE__)
    (void)out_mask;
    return SITUATION_SUCCESS;

#else
    return SITUATION_ERROR_DEVICE_QUERY;
#endif
}

SITAPI SituationError SituationSetThreadAffinityEx(uint64_t core_mask, uint64_t* out_previous) {
    if (core_mask == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_INVALID_PARAM, "SituationSetThreadAffinity: core_mask is zero");
        return SITUATION_ERROR_INVALID_PARAM;
    }

    if (out_previous) {
        if (SituationGetThreadAffinity(out_previous) != SITUATION_SUCCESS) {
            *out_previous = 0;
        }
    }

#if defined(_WIN32)
    HANDLE thread = GetCurrentThread();
    DWORD_PTR result = SetThreadAffinityMask(thread, (DWORD_PTR)core_mask);
    if (result == 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "SetThreadAffinityMask failed.");
        return SITUATION_ERROR_DEVICE_QUERY;
    }
    if (out_previous) {
        *out_previous = (uint64_t)result;
    }
    return SITUATION_SUCCESS;

#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i < SITUATION_AFFINITY_MASK_BITS; ++i) {
        if ((core_mask & (1ULL << i)) != 0) {
            CPU_SET(i, &cpuset);
        }
    }
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) != 0) {
        _SituationSetErrorFromCode(SITUATION_ERROR_DEVICE_QUERY, "pthread_setaffinity_np failed.");
        return SITUATION_ERROR_DEVICE_QUERY;
    }
    return SITUATION_SUCCESS;

#elif defined(__APPLE__)
    (void)core_mask;
    return SITUATION_SUCCESS;

#else
    return SITUATION_ERROR_DEVICE_QUERY;
#endif
}

SITAPI SituationError SituationSetThreadAffinity(uint64_t core_mask) {
    return SituationSetThreadAffinityEx(core_mask, NULL);
}

SITAPI int SituationGetCurrentProcessorIndex(void) {
#if defined(_WIN32)
    return (int)GetCurrentProcessorNumber();

#elif defined(__linux__)
    int cpu = sched_getcpu();
    return cpu >= 0 ? cpu : -1;

#elif defined(__APPLE__)
    return -1;

#else
    return -1;
#endif
}

SITAPI int SituationGetThreadNumaNode(void) {
    int cpu = SituationGetCurrentProcessorIndex();
    if (cpu < 0) {
        return -1;
    }
    const SituationCpuTopology* topo = NULL;
    if (SituationGetCpuTopology(&topo) != SITUATION_SUCCESS || !topo) {
        return -1;
    }
    if ((uint32_t)cpu >= topo->logical_count) {
        return -1;
    }
    return (int)topo->processors[cpu].numa_node;
}

// ==================================================================================
// Mask builders (require valid topology cache)
// ==================================================================================

static bool _SitEnsureTopology(void) {
    if (!g_sit_cpu_topology_valid) {
        return SituationRefreshCpuTopology() == SITUATION_SUCCESS;
    }
    return true;
}

SITAPI uint64_t SituationBuildPhysicalCoreMask(int physical_core_index) {
    if (!_SitEnsureTopology() || physical_core_index < 0) {
        return 0;
    }
    const SituationCpuTopology* topo = &g_sit_cpu_topology;
    if ((uint32_t)physical_core_index >= topo->physical_count) {
        return 0;
    }

    uint64_t mask = 0;
    for (uint32_t i = 0; i < topo->logical_count && i < SITUATION_AFFINITY_MASK_BITS; ++i) {
        if ((int)topo->processors[i].physical_core_id == physical_core_index) {
            mask |= (1ULL << i);
        }
    }
    return mask;
}

SITAPI uint64_t SituationBuildUniqueCoreMask(int start_physical_core, int count, bool avoid_siblings) {
    if (!_SitEnsureTopology() || count <= 0 || start_physical_core < 0) {
        return 0;
    }

    const SituationCpuTopology* topo = &g_sit_cpu_topology;
    uint64_t mask = 0;
    int picked = 0;

    for (int p = start_physical_core; picked < count && (uint32_t)p < topo->physical_count; ++p) {
        uint32_t best_lp = UINT32_MAX;
        for (uint32_t i = 0; i < topo->logical_count && i < SITUATION_AFFINITY_MASK_BITS; ++i) {
            if ((int)topo->processors[i].physical_core_id != p) {
                continue;
            }
            if (!avoid_siblings) {
                best_lp = i;
                break;
            }
            if (!topo->processors[i].is_hyperthread_sibling) {
                best_lp = i;
                break;
            }
            if (best_lp == (uint32_t)UINT32_MAX) {
                best_lp = i;
            }
        }
        if (best_lp != (uint32_t)UINT32_MAX) {
            mask |= (1ULL << best_lp);
            picked++;
        }
    }
    return mask;
}

SITAPI uint64_t SituationBuildNumaNodeMask(int numa_node_index) {
    if (!_SitEnsureTopology() || numa_node_index < 0) {
        return 0;
    }

    const SituationCpuTopology* topo = &g_sit_cpu_topology;
    uint64_t mask = 0;
    for (uint32_t i = 0; i < topo->logical_count && i < SITUATION_AFFINITY_MASK_BITS; ++i) {
        if ((int)topo->processors[i].numa_node == numa_node_index) {
            mask |= (1ULL << i);
        }
    }
    return mask;
}

// ==================================================================================
// Configured affinity (SituationInitInfo → sit_gs), Epic A3 / E3
// ==================================================================================

SITAPI uint64_t SituationGetConfiguredMainThreadAffinity(void) {
    if (_sit_current_context != NULL) {
        return sit_gs.thread_affinity_main;
    }
    return 0;
}

SITAPI uint64_t SituationGetConfiguredRenderThreadAffinity(void) {
    if (_sit_current_context != NULL && sit_gs.thread_affinity_render != 0) {
        return sit_gs.thread_affinity_render;
    }
    if (_sit_current_context != NULL && sit_gs.numa_prefer_local) {
        int node = 0;
        const SituationCpuTopology* topo = NULL;
        if (SituationGetCpuTopology(&topo) && topo && topo->logical_count > 1) {
            node = (int)topo->processors[1].numa_node;
        }
        uint64_t mask = SituationBuildNumaNodeMask(node);
        if (mask != 0) return mask;
    }
    return (1ULL << 1);
}

SITAPI uint64_t SituationGetConfiguredAudioThreadAffinity(void) {
    if (_sit_current_context != NULL && sit_gs.thread_affinity_audio != 0) {
        return sit_gs.thread_affinity_audio;
    }
    if (_sit_current_context != NULL && sit_gs.numa_prefer_local) {
        int node = 0;
        const SituationCpuTopology* topo = NULL;
        if (SituationGetCpuTopology(&topo) && topo && topo->logical_count > 2) {
            node = (int)topo->processors[2].numa_node;
        }
        uint64_t mask = SituationBuildNumaNodeMask(node);
        if (mask != 0) return mask;
    }
    return (1ULL << 2);
}

SITAPI uint64_t SituationGetConfiguredIOThreadAffinity(void) {
    // io_thread_numa_node > 0 means user explicitly chose a NUMA node (0 is ambiguous with zero-init)
    if (_sit_current_context != NULL && sit_gs.io_thread_numa_node > 0) {
        uint64_t mask = SituationBuildNumaNodeMask(sit_gs.io_thread_numa_node);
        if (mask != 0) return mask;
    }
    // Default: logical CPU 3
    return (1ULL << 3);
}

#endif /* SITUATION_IMPL_THREADING_TOPOLOGY_H */
