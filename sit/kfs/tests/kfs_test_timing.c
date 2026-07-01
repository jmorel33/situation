#include "kfs_test_timing.h"
#include "kfs_test_fixture.h"
#include "kfs_test_perf.h"
#include "kfs/kfs_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void kfs_test_timing_reset(KFS_TestCtx* ctx) {
    if (!ctx) {
        return;
    }
    ctx->timing_count = 0;
    memset(ctx->timings, 0, sizeof(ctx->timings));
}

void kfs_test_timing_record(KFS_TestCtx* ctx, const char* label, uint64_t elapsed_ns) {
    int i;
    if (!ctx || !label || label[0] == '\0') {
        return;
    }

    for (i = 0; i < ctx->timing_count; ++i) {
        if (strcmp(ctx->timings[i].label, label) == 0) {
            ctx->timings[i].total_ns += elapsed_ns;
            ctx->timings[i].count++;
            return;
        }
    }

    if (ctx->timing_count >= KFS_TEST_TIMING_MAX_OPS) {
        return;
    }

    ctx->timings[ctx->timing_count].label = label;
    ctx->timings[ctx->timing_count].total_ns = elapsed_ns;
    ctx->timings[ctx->timing_count].count = 1;
    ctx->timing_count++;
}

void kfs_test_format_duration(uint64_t ns, char* out, size_t out_sz) {
    double ms;
    double sec;
    if (!out || out_sz == 0) {
        return;
    }
    if (ns >= 1000000000ULL) {
        sec = (double)ns / 1000000000.0;
        snprintf(out, out_sz, "%.2f s", sec);
    } else if (ns >= 1000000ULL) {
        ms = (double)ns / 1000000.0;
        snprintf(out, out_sz, "%.3f ms", ms);
    } else if (ns >= 1000ULL) {
        snprintf(out, out_sz, "%.3f us", (double)ns / 1000.0);
    } else {
        snprintf(out, out_sz, "%llu ns", (unsigned long long)ns);
    }
}

int kfs_test_timing_is_fixture_label(const char* label) {
    return label != NULL &&
           (strcmp(label, "kfs_init") == 0 || strcmp(label, "kfs_close") == 0);
}

uint64_t kfs_test_timing_ops_total_ns(const KFS_TestCtx* ctx) {
    uint64_t total = 0;
    int i;
    if (!ctx) {
        return 0;
    }
    for (i = 0; i < ctx->timing_count; ++i) {
        if (!kfs_test_timing_is_fixture_label(ctx->timings[i].label)) {
            total += ctx->timings[i].total_ns;
        }
    }
    return total;
}

static void kfs_test_timing_print_entries(const KFS_TestTimingEntry* entries, int n,
                                          const char* prefix, int fixture_only) {
    char dur[32];
    int i;
    int printed = 0;

    if (!entries || n <= 0 || !prefix) {
        return;
    }

    for (i = 0; i < n; ++i) {
        int is_fixture = kfs_test_timing_is_fixture_label(entries[i].label);
        if (fixture_only ? !is_fixture : is_fixture) {
            continue;
        }
        if (printed == 0) {
            printf("         %s:", prefix);
        }
        kfs_test_format_duration(entries[i].total_ns, dur, sizeof(dur));
        if (entries[i].count > 1) {
            printf(" %s=%s(x%u)", entries[i].label, dur, entries[i].count);
        } else {
            printf(" %s=%s", entries[i].label, dur);
        }
        printed++;
    }
    if (printed > 0) {
        printf("\n");
    }
}

static int kfs_test_timing_entry_cmp(const void* a, const void* b) {
    const KFS_TestTimingEntry* lhs = (const KFS_TestTimingEntry*)a;
    const KFS_TestTimingEntry* rhs = (const KFS_TestTimingEntry*)b;
    if (lhs->total_ns < rhs->total_ns) return 1;
    if (lhs->total_ns > rhs->total_ns) return -1;
    return 0;
}

void kfs_test_mem_print_summary(int verbose) {
    if (!verbose) {
        return;
    }
    printf("         mem: in_use=%zu peak=%zu sqlite=%zu\n",
           kfs_mem_bytes_in_use(), kfs_mem_peak_bytes(), kfs_sqlite_bytes_in_use());
}

void kfs_test_timing_print_breakdown(const KFS_TestCtx* ctx) {
    KFS_TestTimingEntry sorted[KFS_TEST_TIMING_MAX_OPS];
    int n;

    if (!ctx || ctx->timing_count <= 0) {
        return;
    }

    n = ctx->timing_count;
    memcpy(sorted, ctx->timings, (size_t)n * sizeof(sorted[0]));
    qsort(sorted, (size_t)n, sizeof(sorted[0]), kfs_test_timing_entry_cmp);

    kfs_test_timing_print_entries(sorted, n, "ops", 0);
    kfs_test_timing_print_entries(sorted, n, "fixture", 1);
}