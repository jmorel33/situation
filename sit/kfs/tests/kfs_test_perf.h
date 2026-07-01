#ifndef KFS_TEST_PERF_H
#define KFS_TEST_PERF_H

#include <stddef.h>
#include <stdint.h>

typedef struct KFS_TestPerfOptions {
    int warmup_iters;
    int measure_iters;
    char tier_filter;
    int json_output;
} KFS_TestPerfOptions;

typedef struct KFS_TestPerfStats {
    uint64_t samples;
    uint64_t total_ns;
    uint64_t min_ns;
    uint64_t max_ns;
    uint64_t p50_ns;
    uint64_t p95_ns;
    double mean_ns;
    double ops_per_sec;
    double bytes_per_sec;
    size_t bytes_per_op;
} KFS_TestPerfStats;

#define KFS_TEST_PERF_SKIP 77

void kfs_test_perf_set_options(const KFS_TestPerfOptions* opts);
const KFS_TestPerfOptions* kfs_test_perf_get_options(void);

uint64_t kfs_test_perf_now_ns(void);

void kfs_test_perf_stats_init(KFS_TestPerfStats* stats);
void kfs_test_perf_stats_add(KFS_TestPerfStats* stats, uint64_t elapsed_ns, size_t bytes);
void kfs_test_perf_stats_finalize(KFS_TestPerfStats* stats);

/*
 * Run warmup then measured iterations of fn(user_data).
 * fn returns 0 on success; non-zero aborts the benchmark.
 */
int kfs_test_perf_run(const char* name, char tier,
                      int (*fn)(void* user_data, uint64_t* elapsed_ns, size_t* bytes),
                      void* user_data, KFS_TestPerfStats* out_stats);

typedef struct KFS_TestPerfPublished {
    int valid;
    char test_name[64];
    char tier;
    KFS_TestPerfStats stats;
} KFS_TestPerfPublished;

/* Store perf stats for harness to print after quiet mode ends. */
void kfs_test_perf_publish(const char* test_name, char tier, const KFS_TestPerfStats* stats);

/* Returns 1 and clears slot if a perf result was published for this test run. */
int kfs_test_perf_take_published(KFS_TestPerfPublished* out);

void kfs_test_perf_print_result(const char* test_name, char tier, const KFS_TestPerfStats* stats);
void kfs_test_perf_print_published(const KFS_TestPerfPublished* published);

int kfs_test_perf_check_baseline(const char* test_name, const KFS_TestPerfStats* stats);

int kfs_test_perf_tier_allowed(char tier);

#endif /* KFS_TEST_PERF_H */