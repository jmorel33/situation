#include "kfs_test_perf.h"
#include "kfs_test_timing.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif

static KFS_TestPerfPublished g_kfs_test_perf_published;

static KFS_TestPerfOptions g_kfs_test_perf_options = {
    100,  /* warmup_iters */
    1000, /* measure_iters */
    0,    /* tier_filter: all */
    0     /* json_output */
};

void kfs_test_perf_set_options(const KFS_TestPerfOptions* opts) {
    if (!opts) {
        return;
    }
    g_kfs_test_perf_options = *opts;
}

const KFS_TestPerfOptions* kfs_test_perf_get_options(void) {
    return &g_kfs_test_perf_options;
}

uint64_t kfs_test_perf_now_ns(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    /* Use double: counter*1e9 overflows uint64 for large QPC values. */
    return (uint64_t)(((double)counter.QuadPart * 1000000000.0) / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

int kfs_test_perf_tier_allowed(char tier) {
    if (g_kfs_test_perf_options.tier_filter == 0) {
        return 1;
    }
    return tier == g_kfs_test_perf_options.tier_filter;
}

static int kfs_test_perf_compare_u64(const void* a, const void* b) {
    const uint64_t* lhs = (const uint64_t*)a;
    const uint64_t* rhs = (const uint64_t*)b;
    if (*lhs < *rhs) return -1;
    if (*lhs > *rhs) return 1;
    return 0;
}

void kfs_test_perf_stats_init(KFS_TestPerfStats* stats) {
    if (!stats) {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    stats->min_ns = UINT64_MAX;
}

void kfs_test_perf_stats_add(KFS_TestPerfStats* stats, uint64_t elapsed_ns, size_t bytes) {
    if (!stats) {
        return;
    }
    stats->samples++;
    stats->total_ns += elapsed_ns;
    if (elapsed_ns < stats->min_ns) {
        stats->min_ns = elapsed_ns;
    }
    if (elapsed_ns > stats->max_ns) {
        stats->max_ns = elapsed_ns;
    }
    if (bytes > stats->bytes_per_op) {
        stats->bytes_per_op = bytes;
    }
}

void kfs_test_perf_stats_finalize(KFS_TestPerfStats* stats) {
    if (!stats || stats->samples == 0) {
        return;
    }
    if (stats->min_ns == UINT64_MAX) {
        stats->min_ns = 0;
    }
    stats->mean_ns = (double)stats->total_ns / (double)stats->samples;
    if (stats->mean_ns > 0.0) {
        stats->ops_per_sec = 1000000000.0 / stats->mean_ns;
        if (stats->bytes_per_op > 0) {
            stats->bytes_per_sec = (stats->bytes_per_op * 1000000000.0) / stats->mean_ns;
        }
    }
}

int kfs_test_perf_run(const char* name, char tier,
                      int (*fn)(void* user_data, uint64_t* elapsed_ns, size_t* bytes),
                      void* user_data, KFS_TestPerfStats* out_stats) {
    uint64_t* samples = NULL;
    int warmup;
    int measure;
    int i;

    (void)name;

    if (!fn || !out_stats) {
        return 1;
    }
    if (!kfs_test_perf_tier_allowed(tier)) {
        return KFS_TEST_PERF_SKIP;
    }

    kfs_test_perf_stats_init(out_stats);
    measure = g_kfs_test_perf_options.measure_iters;
    if (measure <= 0) {
        return 1;
    }

    samples = (uint64_t*)malloc((size_t)measure * sizeof(uint64_t));
    if (!samples) {
        return 1;
    }

    for (warmup = 0; warmup < g_kfs_test_perf_options.warmup_iters; ++warmup) {
        uint64_t elapsed_ns = 0;
        size_t bytes = 0;
        if (fn(user_data, &elapsed_ns, &bytes) != 0) {
            free(samples);
            return 1;
        }
    }

    for (i = 0; i < measure; ++i) {
        uint64_t elapsed_ns = 0;
        size_t bytes = 0;
        if (fn(user_data, &elapsed_ns, &bytes) != 0) {
            free(samples);
            return 1;
        }
        samples[i] = elapsed_ns;
        kfs_test_perf_stats_add(out_stats, elapsed_ns, bytes);
    }

    qsort(samples, (size_t)measure, sizeof(uint64_t), kfs_test_perf_compare_u64);
    out_stats->p50_ns = samples[measure / 2];
    out_stats->p95_ns = samples[(measure * 95) / 100];
    kfs_test_perf_stats_finalize(out_stats);

    free(samples);
    return 0;
}

void kfs_test_perf_publish(const char* test_name, char tier, const KFS_TestPerfStats* stats) {
    if (!test_name || !stats) {
        g_kfs_test_perf_published.valid = 0;
        return;
    }
    g_kfs_test_perf_published.valid = 1;
    strncpy(g_kfs_test_perf_published.test_name, test_name,
            sizeof(g_kfs_test_perf_published.test_name) - 1);
    g_kfs_test_perf_published.test_name[sizeof(g_kfs_test_perf_published.test_name) - 1] = '\0';
    g_kfs_test_perf_published.tier = tier;
    g_kfs_test_perf_published.stats = *stats;
}

int kfs_test_perf_take_published(KFS_TestPerfPublished* out) {
    if (!out || !g_kfs_test_perf_published.valid) {
        return 0;
    }
    *out = g_kfs_test_perf_published;
    g_kfs_test_perf_published.valid = 0;
    return 1;
}

void kfs_test_perf_print_published(const KFS_TestPerfPublished* published) {
    if (!published) {
        return;
    }
    kfs_test_perf_print_result(published->test_name, published->tier, &published->stats);
}

void kfs_test_perf_print_result(const char* test_name, char tier, const KFS_TestPerfStats* stats) {
    const KFS_TestPerfOptions* opts = kfs_test_perf_get_options();
    if (!test_name || !stats) {
        return;
    }

    if (opts->json_output) {
        printf("{\"test\":\"%s\",\"tier\":\"%c\",\"p50_ns\":%llu,\"p95_ns\":%llu,"
               "\"mean_ns\":%.1f,\"ops_per_sec\":%.1f,\"bytes_per_op\":%zu,"
               "\"bytes_per_sec\":%.1f}\n",
               test_name, tier,
               (unsigned long long)stats->p50_ns,
               (unsigned long long)stats->p95_ns,
               stats->mean_ns, stats->ops_per_sec,
               stats->bytes_per_op, stats->bytes_per_sec);
        return;
    }

    {
        char p50_buf[32];
        char p95_buf[32];
        char mean_buf[32];
        kfs_test_format_duration((uint64_t)stats->p50_ns, p50_buf, sizeof(p50_buf));
        kfs_test_format_duration((uint64_t)stats->p95_ns, p95_buf, sizeof(p95_buf));
        kfs_test_format_duration((uint64_t)stats->mean_ns, mean_buf, sizeof(mean_buf));

        if (stats->bytes_per_op > 0) {
            printf("         perf: p50=%s p95=%s mean=%s %.1f MB/s (%zu bytes/op)\n",
                   p50_buf, p95_buf, mean_buf,
                   stats->bytes_per_sec / (1024.0 * 1024.0),
                   stats->bytes_per_op);
        } else if (stats->samples > 0) {
            printf("         perf: p50=%s p95=%s mean=%s %.0f ops/s (%llu samples)\n",
                   p50_buf, p95_buf, mean_buf,
                   stats->ops_per_sec,
                   (unsigned long long)stats->samples);
        } else {
            printf("         perf: %s (single-shot)\n", mean_buf);
        }
    }
    (void)test_name;
    (void)tier;
}

int kfs_test_perf_check_baseline(const char* test_name, const KFS_TestPerfStats* stats) {
    (void)test_name;
    (void)stats;
    /* Baselines committed after first green run on a reference machine (H7.0c). */
    return 0;
}