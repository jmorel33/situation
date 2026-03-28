import re

with open("sit/situation_impl.h", "r") as f:
    data = f.read()

affinity_func = """
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

static void _SituationSetThreadAffinity(bool high_perf) {
#if defined(_WIN32)
    HANDLE thread = GetCurrentThread();
    DWORD_PTR mask = high_perf ? 1 : 2; // Extremely simplified, ideally you query cores. Let's just set to 1 for high perf, and 2 for low perf.
    SetThreadAffinityMask(thread, mask);
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (high_perf) {
        CPU_SET(0, &cpuset); // Assume core 0 is P-core
    } else {
        CPU_SET(1, &cpuset); // Assume core 1 is E-core
    }
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(__APPLE__)
    #include <mach/mach_init.h>
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
    thread_port_t mach_thread = mach_thread_self();
    thread_affinity_policy_data_t policyData = { high_perf ? 1 : 2 };
    thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policyData, THREAD_AFFINITY_POLICY_COUNT);
#endif
}
"""

if "_SituationSetThreadAffinity" not in data:
    data = re.sub(r'SITAPI SituationError SituationInit\(', affinity_func + '\nSITAPI SituationError SituationInit(', data)

data = re.sub(r'_SituationSetError\("SituationInit: No error. Initialization successful."\);\n\n    // --- 7. Return Success ---\n    return SITUATION_SUCCESS;', '_SituationSetError("SituationInit: No error. Initialization successful.");\n    _SituationSetThreadAffinity(true);\n\n    // --- 7. Return Success ---\n    return SITUATION_SUCCESS;', data)

data = re.sub(r'static int _SituationRenderThreadEntry\(void\* arg\) \{\n', r'static int _SituationRenderThreadEntry(void* arg) {\n    _SituationSetThreadAffinity(true);\n', data)

data = re.sub(r'static int _SituationWorkerEntry\(void\* arg\) \{\n', r'static int _SituationWorkerEntry(void* arg) {\n    _SituationSetThreadAffinity(false);\n', data)

with open("sit/situation_impl.h", "w") as f:
    f.write(data)
