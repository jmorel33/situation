#ifndef KFS_TEST_TIMING_H
#define KFS_TEST_TIMING_H

#include <stddef.h>
#include <stdint.h>

#include "kfs_test_perf.h"

#define KFS_TEST_TIMING_MAX_OPS 24

typedef struct KFS_TestTimingEntry {
    const char* label;
    uint64_t total_ns;
    uint32_t count;
} KFS_TestTimingEntry;

typedef struct KFS_TestCtx KFS_TestCtx;

void kfs_test_timing_reset(KFS_TestCtx* ctx);
void kfs_test_timing_record(KFS_TestCtx* ctx, const char* label, uint64_t elapsed_ns);
void kfs_test_format_duration(uint64_t ns, char* out, size_t out_sz);

int kfs_test_timing_is_fixture_label(const char* label);
uint64_t kfs_test_timing_ops_total_ns(const KFS_TestCtx* ctx);
void kfs_test_timing_print_breakdown(const KFS_TestCtx* ctx);
void kfs_test_mem_print_summary(int verbose);

/* Statement expression: `call` must run inside the macro, not as a pre-evaluated arg. */
#define KFS_TEST_TIMED_INT(ctx, label, call) \
    ({ \
        uint64_t _kfs_t0 = kfs_test_perf_now_ns(); \
        int _kfs_trc = (call); \
        kfs_test_timing_record((ctx), (label), kfs_test_perf_now_ns() - _kfs_t0); \
        _kfs_trc; \
    })

#endif /* KFS_TEST_TIMING_H */