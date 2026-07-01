/**
 * @file tests/kfs_test_main.c
 * @brief KFS test harness entry point (Phase H0+).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#ifndef dup
#define dup _dup
#endif
#ifndef dup2
#define dup2 _dup2
#endif
#ifndef close
#define close _close
#endif
#define KFS_TEST_DEV_NULL "NUL"
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#else
#include <unistd.h>
#define KFS_TEST_DEV_NULL "/dev/null"
#endif

#include "kfs_test_registry.h"
#include "kfs_test_fixture.h"
#include "kfs_test_perf.h"
#include "kfs_test_timing.h"

typedef struct KFS_TestOptions {
    int list_only;
    int verbose;
    int perf_only;
    int json_output;
    int perf_iters;
    char perf_tier;
    const char* module_filter;
    const char* test_filter;
} KFS_TestOptions;

typedef struct KFS_TestModuleMeta {
    const char* id;
    const char* phase;
    const char* title;
} KFS_TestModuleMeta;

static const KFS_TestModuleMeta kfs_test_module_meta[] = {
    { "harness",         "H0", "Harness foundation" },
    { "lifecycle",       "H1", "Bootstrap & lifecycle" },
    { "actors",          "H2", "Actors & groups" },
    { "domains",         "H3", "Domains & firewall" },
    { "security_schemes","H4", "Security schemes" },
    { "content",         "H5", "Content model" },
    { "permissions",     "H6", "Permissions matrix" },
    { "perf",            "H7", "Performance baselines" },
};

static const size_t kfs_test_module_meta_count =
    sizeof(kfs_test_module_meta) / sizeof(kfs_test_module_meta[0]);

static int g_kfs_test_stdout_saved = -1;
static int g_kfs_test_stderr_saved = -1;

static void kfs_test_print_usage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [--list] [--module NAME] [--test NAME] [--verbose]\n"
            "       %s [--perf] [--perf-iters N] [--perf-tier S|M|L] [--json]\n"
            "  --list           Print registered tests grouped by module\n"
            "  --module NAME    Run only tests in module NAME\n"
            "  --test NAME      Run only test NAME (within module if set)\n"
            "  --verbose        Show KFS library INFO logs during tests\n"
            "  --perf           Run H7 performance module only (excluded by default)\n"
            "  --perf-iters N   Measured iterations for perf tests (default: 1000)\n"
            "  --perf-tier X    Run only perf tests for tier S, M, or L\n"
            "  --json           Emit perf results as JSON lines\n",
            argv0, argv0);
}

static int kfs_test_parse_args(int argc, char** argv, KFS_TestOptions* opts) {
    int i;
    if (!opts) return 1;
    opts->list_only = 0;
    opts->verbose = 0;
    opts->perf_only = 0;
    opts->json_output = 0;
    opts->perf_iters = 0;
    opts->perf_tier = 0;
    opts->module_filter = NULL;
    opts->test_filter = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) {
            opts->list_only = 1;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            opts->verbose = 1;
        } else if (strcmp(argv[i], "--perf") == 0) {
            opts->perf_only = 1;
        } else if (strcmp(argv[i], "--json") == 0) {
            opts->json_output = 1;
        } else if (strcmp(argv[i], "--perf-iters") == 0) {
            if (i + 1 >= argc) return 1;
            opts->perf_iters = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--perf-tier") == 0) {
            if (i + 1 >= argc) return 1;
            opts->perf_tier = argv[++i][0];
        } else if (strcmp(argv[i], "--module") == 0) {
            if (i + 1 >= argc) return 1;
            opts->module_filter = argv[++i];
        } else if (strcmp(argv[i], "--test") == 0) {
            if (i + 1 >= argc) return 1;
            opts->test_filter = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            kfs_test_print_usage(argv[0]);
            return 2;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }
    return 0;
}

static int kfs_test_should_run(const KFS_TestCase* tc, const KFS_TestOptions* opts) {
    int is_perf = (strcmp(tc->module, "perf") == 0);
    if (opts->perf_only) {
        if (!is_perf) {
            return 0;
        }
    } else if (is_perf) {
        return 0;
    }
    if (opts->module_filter && strcmp(tc->module, opts->module_filter) != 0) {
        return 0;
    }
    if (opts->test_filter && strcmp(tc->name, opts->test_filter) != 0) {
        return 0;
    }
    return 1;
}

static const KFS_TestModuleMeta* kfs_test_lookup_module_meta(const char* module_id) {
    size_t i;
    for (i = 0; i < kfs_test_module_meta_count; i++) {
        if (strcmp(kfs_test_module_meta[i].id, module_id) == 0) {
            return &kfs_test_module_meta[i];
        }
    }
    return NULL;
}

static void kfs_test_print_module_banner(const char* module_id) {
    const KFS_TestModuleMeta* meta = kfs_test_lookup_module_meta(module_id);
    if (meta) {
        printf("== %s %s: %s ==\n", meta->phase, meta->id, meta->title);
    } else {
        printf("== %s ==\n", module_id);
    }
}

static void kfs_test_quiet_begin(int verbose) {
    if (verbose || g_kfs_test_stdout_saved >= 0) {
        return;
    }
    fflush(stdout);
    fflush(stderr);
    g_kfs_test_stdout_saved = dup(STDOUT_FILENO);
    g_kfs_test_stderr_saved = dup(STDERR_FILENO);
    if (g_kfs_test_stdout_saved < 0 || g_kfs_test_stderr_saved < 0) {
        if (g_kfs_test_stdout_saved >= 0) {
            close(g_kfs_test_stdout_saved);
            g_kfs_test_stdout_saved = -1;
        }
        if (g_kfs_test_stderr_saved >= 0) {
            close(g_kfs_test_stderr_saved);
            g_kfs_test_stderr_saved = -1;
        }
        return;
    }
    if (freopen(KFS_TEST_DEV_NULL, "w", stdout) == NULL) {
        dup2(g_kfs_test_stdout_saved, STDOUT_FILENO);
        close(g_kfs_test_stdout_saved);
        g_kfs_test_stdout_saved = -1;
    }
    if (freopen(KFS_TEST_DEV_NULL, "w", stderr) == NULL) {
        dup2(g_kfs_test_stderr_saved, STDERR_FILENO);
        close(g_kfs_test_stderr_saved);
        g_kfs_test_stderr_saved = -1;
    }
}

static void kfs_test_quiet_end(void) {
    if (g_kfs_test_stdout_saved < 0) {
        return;
    }
    fflush(stdout);
    fflush(stderr);
    dup2(g_kfs_test_stdout_saved, STDOUT_FILENO);
    dup2(g_kfs_test_stderr_saved, STDERR_FILENO);
    close(g_kfs_test_stdout_saved);
    close(g_kfs_test_stderr_saved);
    g_kfs_test_stdout_saved = -1;
    g_kfs_test_stderr_saved = -1;
}

static void kfs_test_list_all(void) {
    size_t i;
    const char* current_module = NULL;

    for (i = 0; i < kfs_test_case_count; i++) {
        const KFS_TestCase* tc = &kfs_test_cases[i];
        if (!current_module || strcmp(current_module, tc->module) != 0) {
            if (current_module) {
                printf("\n");
            }
            kfs_test_print_module_banner(tc->module);
            current_module = tc->module;
        }
        printf("  %s.%s\n", tc->module, tc->name);
    }
}

int main(int argc, char** argv) {
    KFS_TestOptions opts;
    int parse_rc = kfs_test_parse_args(argc, argv, &opts);
    if (parse_rc == 2) return 0;
    if (parse_rc != 0) {
        kfs_test_print_usage(argv[0]);
        return 2;
    }

    if (opts.list_only) {
        kfs_test_list_all();
        return 0;
    }

    {
        KFS_TestPerfOptions perf_opts;
        perf_opts.warmup_iters = 100;
        perf_opts.measure_iters = (opts.perf_iters > 0) ? opts.perf_iters : 1000;
        perf_opts.tier_filter = opts.perf_tier;
        perf_opts.json_output = opts.json_output;
        kfs_test_perf_set_options(&perf_opts);
    }

    size_t ran = 0;
    size_t passed = 0;
    size_t failed = 0;
    size_t skipped = 0;
    size_t module_ran = 0;
    size_t module_passed = 0;
    size_t module_failed = 0;
    size_t module_skipped = 0;
    const char* current_module = NULL;
    size_t i;
    int exit_code = 0;

    if (kfs_test_fixture_suite_begin() != 0) {
        fprintf(stderr, "[HARNESS] failed to initialize suite temp directory\n");
        return 2;
    }

    for (i = 0; i < kfs_test_case_count; i++) {
        const KFS_TestCase* tc = &kfs_test_cases[i];
        KFS_TestCtx ctx;
        int test_rc;

        if (!kfs_test_should_run(tc, &opts)) {
            continue;
        }

        if (!current_module || strcmp(current_module, tc->module) != 0) {
            if (current_module && module_ran > 0) {
                printf("  (%zu/%zu passed)\n\n", module_passed, module_ran);
            }
            kfs_test_print_module_banner(tc->module);
            current_module = tc->module;
            module_ran = 0;
            module_passed = 0;
            module_failed = 0;
            module_skipped = 0;
        }

        uint64_t test_start_ns;
        uint64_t test_ns;
        char test_buf[32];
        KFS_TestPerfPublished perf_pub;

        kfs_test_quiet_begin(opts.verbose);
        if (kfs_test_ctx_create(&ctx) != 0) {
            kfs_test_quiet_end();
            fprintf(stderr, "[HARNESS] setup failed for %s.%s\n", tc->module, tc->name);
            kfs_test_fixture_suite_end();
            return 2;
        }

        test_start_ns = kfs_test_perf_now_ns();
        test_rc = tc->fn(&ctx);
        test_ns = kfs_test_perf_now_ns() - test_start_ns;
        kfs_test_quiet_end();

        kfs_test_format_duration(test_ns, test_buf, sizeof(test_buf));

        ran++;
        module_ran++;
        if (test_rc == KFS_TEST_PERF_SKIP) {
            printf("  [SKIP] %s (%s)\n", tc->name, test_buf);
            skipped++;
            module_skipped++;
        } else if (test_rc == 0) {
            printf("  [PASS] %s (%s)\n", tc->name, test_buf);
            passed++;
            module_passed++;
        } else {
            printf("  [FAIL] %s (%s)", tc->name, test_buf);
            if (!opts.verbose) {
                printf(" (re-run with --verbose for library trace)");
            }
            printf("\n");
            failed++;
            module_failed++;
        }

        kfs_test_ctx_destroy(&ctx);

        if (kfs_test_perf_take_published(&perf_pub)) {
            kfs_test_perf_print_result(perf_pub.test_name, perf_pub.tier, &perf_pub.stats);
        }
        kfs_test_timing_print_breakdown(&ctx);
        kfs_test_mem_print_summary(opts.verbose);

    }

    if (current_module && module_ran > 0) {
        printf("  (%zu/%zu passed", module_passed, module_ran);
        if (module_failed > 0) {
            printf(", %zu failed", module_failed);
        }
        if (module_skipped > 0) {
            printf(", %zu skipped", module_skipped);
        }
        printf(")\n\n");
    }

    if (ran == 0) {
        fprintf(stderr, "[HARNESS] no tests matched filters\n");
        kfs_test_fixture_suite_end();
        return 2;
    }

    printf("[SUMMARY] %zu passed, %zu failed", passed, failed);
    if (skipped > 0) {
        printf(", %zu skipped", skipped);
    }
    printf(" (%zu total)\n", ran);

    exit_code = (failed == 0) ? 0 : 1;
    kfs_test_fixture_suite_end();
    return exit_code;
}