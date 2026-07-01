/**
 * @file sit_test_framework.h
 * @brief Minimal C11 Test Framework for the Situation Library
 *
 * Header-only. Zero external dependencies beyond C11 stdlib.
 * Provides assertion macros, setjmp/longjmp recovery, result tracking,
 * and ANSI colored output.
 *
 * Usage:
 *   #define SIT_TEST_IMPLEMENTATION   (in exactly one .c file)
 *   #include "sit_test_framework.h"
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#ifndef SIT_TEST_FRAMEWORK_H
#define SIT_TEST_FRAMEWORK_H

#include "sit_test_headless.h"
#include "sit_test_scratch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <signal.h>
    #include <unistd.h>
#endif

// ============================================================================
//  Types
// ============================================================================

typedef void (*SitTestFunc)(void);

typedef struct SitTestCase {
    const char* name;
    SitTestFunc func;
    bool requires_context;
    bool listen_test; /* human listen / on-window overlay; use --listen-only */
} SitTestCase;

typedef struct SitTestModule {
    const char* name;
    void (*setup)(void);
    void (*teardown)(void);
    SitTestCase* tests;
    int test_count;
    bool requires_context;
    bool harness_visual; /* scope + spectrum overlay; same path on OpenGL and Vulkan */
    bool requires_visible_window; /* skip entire module when --headless / SIT_TEST_HEADLESS */
} SitTestModule;

typedef struct SitTestResults {
    int total;
    int passed;
    int failed;
    int skipped;
    int visual_warnings;
    double elapsed_seconds;
} SitTestResults;

typedef struct SitTestConfig {
    const char* filter_module;
    const char* filter_test;
    bool verbose;
    bool stop_on_fail;
    bool list_only;
    bool listen_only;
    bool no_color;
    bool strict_visual;
    bool headless; /* hidden GLFW window; no listen overlay — OpenGL and Vulkan */
} SitTestConfig;

#define SIT_MAX_MODULES 32
#define SIT_MAX_FAILURES_PER_TEST 16
#define SIT_MAX_FAILED_TESTS 128
#define SIT_MAX_FAILED_TEST_NAME 160
#define SIT_TEST_TIMEOUT_SECONDS 10

/** Optional per-run cleanup after SIGSEGV/abort or Windows AV (e.g. release MIDI graph fixture). */
typedef void (*SitTestCrashCleanupFn)(void);

#ifdef SIT_TEST_IMPLEMENTATION
const SitTestModule* g_sit_modules[SIT_MAX_MODULES];
int g_sit_module_count = 0;
SitTestConfig g_sit_config = {0};
jmp_buf g_sit_test_jmp_buf;
bool g_sit_current_test_failed = false;
bool g_sit_current_test_skipped = false;
char g_sit_test_skip_reason[256];
const char* g_sit_current_test_name = NULL;
int g_sit_assertion_count = 0;
int g_sit_visual_warning_count = 0;
bool g_sit_shared_context_active = false;
char g_sit_failed_tests[SIT_MAX_FAILED_TESTS][SIT_MAX_FAILED_TEST_NAME];
int g_sit_failed_test_count = 0;
SitTestCrashCleanupFn g_sit_test_crash_cleanup_fn = NULL;
#else
extern const SitTestModule* g_sit_modules[SIT_MAX_MODULES];
extern int g_sit_module_count;
extern SitTestConfig g_sit_config;
extern jmp_buf g_sit_test_jmp_buf;
extern bool g_sit_current_test_failed;
extern bool g_sit_current_test_skipped;
extern char g_sit_test_skip_reason[256];
extern const char* g_sit_current_test_name;
extern int g_sit_assertion_count;
extern int g_sit_visual_warning_count;
extern bool g_sit_shared_context_active;
extern char g_sit_failed_tests[SIT_MAX_FAILED_TESTS][SIT_MAX_FAILED_TEST_NAME];
extern int g_sit_failed_test_count;
extern SitTestCrashCleanupFn g_sit_test_crash_cleanup_fn;
#endif

/** Register module-specific cleanup invoked before longjmp on crash (NULL to clear). */
static inline void sit_test_set_crash_cleanup(SitTestCrashCleanupFn fn) {
#ifdef SIT_TEST_IMPLEMENTATION
    g_sit_test_crash_cleanup_fn = fn;
#else
    (void)fn;
#endif
}

// ============================================================================
//  Color Output
// ============================================================================

/* Prefix with SIT_TEST_ — situation_base_etc.h already uses SIT_COLOR_* for ColorRGBA. */
#define SIT_TEST_COLOR_RESET   "\033[0m"
#define SIT_TEST_COLOR_GREEN   "\033[32m"
#define SIT_TEST_COLOR_RED     "\033[31m"
#define SIT_TEST_COLOR_YELLOW  "\033[33m"
#define SIT_TEST_COLOR_CYAN    "\033[36m"
#define SIT_TEST_COLOR_BOLD    "\033[1m"
#define SIT_TEST_COLOR_DIM     "\033[2m"

static inline const char* sit_color(const char* code) {
    return g_sit_config.no_color ? "" : code;
}

/** Shared crash path for POSIX signals and Windows SEH — marks test failed and longjmps. */
static inline void sit_test_crash_recover(const char* kind) {
    if (g_sit_test_crash_cleanup_fn) {
        g_sit_test_crash_cleanup_fn();
    }
    fprintf(stderr, "\n    %sCRASH%s: %s in test '%s'\n",
            sit_color(SIT_TEST_COLOR_RED), sit_color(SIT_TEST_COLOR_RESET),
            kind ? kind : "fatal error",
            g_sit_current_test_name ? g_sit_current_test_name : "(unknown)");
    g_sit_current_test_failed = true;
    longjmp(g_sit_test_jmp_buf, 1);
}

// ============================================================================
//  Timing Helpers
// ============================================================================

static inline double sit_get_time_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

// ============================================================================
//  Assertion Implementation
// ============================================================================

static inline void sit_test_fail_impl(const char* file, int line, const char* expr, const char* msg) {
    if (g_sit_test_crash_cleanup_fn) {
        g_sit_test_crash_cleanup_fn();
    }
    fprintf(stderr, "    %sFAIL%s: [%s:%d] %s",
            sit_color(SIT_TEST_COLOR_RED), sit_color(SIT_TEST_COLOR_RESET),
            file, line, expr);
    if (msg && msg[0]) {
        fprintf(stderr, " — %s", msg);
    }
    fprintf(stderr, "\n");
    g_sit_current_test_failed = true;
    longjmp(g_sit_test_jmp_buf, 1);
}

static inline void sit_test_pass_impl(const char* file, int line, const char* expr) {
    g_sit_assertion_count++;
    if (g_sit_config.verbose) {
        fprintf(stderr, "    %sPASS%s: [%s:%d] %s\n",
                sit_color(SIT_TEST_COLOR_GREEN), sit_color(SIT_TEST_COLOR_RESET),
                file, line, expr);
    }
}

/** Skip the current test (not a pass). Runner prints a single [SKIP] line with the reason. */
static inline void sit_test_skip_impl(const char* reason) {
    g_sit_test_skip_reason[0] = '\0';
    if (reason && reason[0]) {
        snprintf(g_sit_test_skip_reason, sizeof(g_sit_test_skip_reason), "%s", reason);
    }
    g_sit_current_test_skipped = true;
    longjmp(g_sit_test_jmp_buf, 1);
}

// ============================================================================
//  Assertion Macros
// ============================================================================

#define SIT_TEST_SKIP(reason) sit_test_skip_impl(reason)

#define SIT_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            sit_test_fail_impl(__FILE__, __LINE__, #expr, NULL); \
        } else { \
            sit_test_pass_impl(__FILE__, __LINE__, #expr); \
        } \
    } while(0)

#define SIT_ASSERT_EQ(actual, expected) \
    do { \
        long long _a = (long long)(actual); \
        long long _e = (long long)(expected); \
        if (_a != _e) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "expected %lld, got %lld", _e, _a); \
            sit_test_fail_impl(__FILE__, __LINE__, #actual " == " #expected, _msg); \
        } else { \
            sit_test_pass_impl(__FILE__, __LINE__, #actual " == " #expected); \
        } \
    } while(0)

#define SIT_ASSERT_NEQ(actual, expected) \
    do { \
        long long _a = (long long)(actual); \
        long long _e = (long long)(expected); \
        if (_a == _e) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "both are %lld", _a); \
            sit_test_fail_impl(__FILE__, __LINE__, #actual " != " #expected, _msg); \
        } else { \
            sit_test_pass_impl(__FILE__, __LINE__, #actual " != " #expected); \
        } \
    } while(0)

#define SIT_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            sit_test_fail_impl(__FILE__, __LINE__, #ptr " == NULL", "pointer is not NULL"); \
        } else { \
            sit_test_pass_impl(__FILE__, __LINE__, #ptr " == NULL"); \
        } \
    } while(0)

#define SIT_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            sit_test_fail_impl(__FILE__, __LINE__, #ptr " != NULL", "pointer is NULL"); \
        } else { \
            sit_test_pass_impl(__FILE__, __LINE__, #ptr " != NULL"); \
        } \
    } while(0)

#define SIT_ASSERT_STR_EQ(a, b) \
    do { \
        const char* _sa = (a); \
        const char* _sb = (b); \
        if (_sa == NULL || _sb == NULL || strcmp(_sa, _sb) != 0) { \
            char _msg[512]; \
            snprintf(_msg, sizeof(_msg), "\"%s\" vs \"%s\"", \
                     _sa ? _sa : "(null)", _sb ? _sb : "(null)"); \
            sit_test_fail_impl(__FILE__, __LINE__, #a " streq " #b, _msg); \
        } else { \
            sit_test_pass_impl(__FILE__, __LINE__, #a " streq " #b); \
        } \
    } while(0)

#define SIT_ASSERT_MEM_EQ(a, b, size) \
    do { \
        if (memcmp((a), (b), (size)) != 0) { \
            char _msg[128]; \
            snprintf(_msg, sizeof(_msg), "%d bytes differ", (int)(size)); \
            sit_test_fail_impl(__FILE__, __LINE__, #a " memeq " #b, _msg); \
        } else { \
            sit_test_pass_impl(__FILE__, __LINE__, #a " memeq " #b); \
        } \
    } while(0)

// ============================================================================
//  Visual Assertion Macro
//  In normal mode: logs a warning but does NOT fail the test.
//  With --strict-visual: behaves like SIT_ASSERT (hard fail).
// ============================================================================

#define SIT_ASSERT_VISUAL(expr) \
    do { \
        if (!(expr)) { \
            if (g_sit_config.strict_visual) { \
                sit_test_fail_impl(__FILE__, __LINE__, #expr, "[VISUAL]"); \
            } else { \
                g_sit_visual_warning_count++; \
                if (g_sit_config.verbose) { \
                    fprintf(stderr, "    %sVISUAL%s: [%s:%d] %s\n", \
                            sit_color(SIT_TEST_COLOR_YELLOW), sit_color(SIT_TEST_COLOR_RESET), \
                            __FILE__, __LINE__, #expr); \
                } \
            } \
        } else { \
            sit_test_pass_impl(__FILE__, __LINE__, #expr); \
        } \
    } while(0)

// ============================================================================
//  Framework API (declarations)
// ============================================================================

void sit_test_register_module(const SitTestModule* module);
void sit_test_init(int argc, char** argv);
SitTestResults sit_test_run_all(void);
void sit_test_print_results(SitTestResults results);

// ============================================================================
//  Implementation
// ============================================================================

#ifdef SIT_TEST_IMPLEMENTATION

#include "sit_test_stereo_scope.h"

/* Serialize harness stderr vs other threads (audio callback, drivers) writing to stderr. */
#ifdef _WIN32
static CRITICAL_SECTION g_sit_harness_stderr_cs;
static void sit_harness_stderr_enter(void) { EnterCriticalSection(&g_sit_harness_stderr_cs); }
static void sit_harness_stderr_leave(void) {
    fflush(stderr);
    LeaveCriticalSection(&g_sit_harness_stderr_cs);
}
#else
static void sit_harness_stderr_enter(void) { (void)0; }
static void sit_harness_stderr_leave(void) { fflush(stderr); }
#endif

void sit_test_register_module(const SitTestModule* module) {
    if (g_sit_module_count < SIT_MAX_MODULES) {
        g_sit_modules[g_sit_module_count++] = module;
    } else {
        fprintf(stderr, "[HARNESS] ERROR: Max modules (%d) exceeded!\n", SIT_MAX_MODULES);
    }
}

static bool sit_strcasecmp_match(const char* a, const char* b) {
    if (!a || !b) return false;
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcasecmp(a, b) == 0;
#endif
}

/** Match test name against filter; `|` separates OR substrings (each uses strstr). */
static bool sit_test_matches_filter(const char* name, const char* filter) {
    if (!filter || !filter[0]) return true;
    if (!name) return false;
    const char* p = filter;
    for (;;) {
        const char* bar = strchr(p, '|');
        if (bar) {
            size_t n = (size_t)(bar - p);
            if (n > 0) {
                char token[256];
                if (n >= sizeof(token)) return false;
                memcpy(token, p, n);
                token[n] = '\0';
                if (strstr(name, token)) return true;
            }
            p = bar + 1;
            continue;
        }
        return p[0] != '\0' && strstr(name, p) != NULL;
    }
}

void sit_test_init(int argc, char** argv) {
    memset(&g_sit_config, 0, sizeof(g_sit_config));
    g_sit_failed_test_count = 0;
    memset(g_sit_failed_tests, 0, sizeof(g_sit_failed_tests));
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--module") == 0 && i + 1 < argc) {
            g_sit_config.filter_module = argv[++i];
        } else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            g_sit_config.filter_test = argv[++i];
        } else if (strcmp(argv[i], "--list") == 0) {
            g_sit_config.list_only = true;
        } else if (strcmp(argv[i], "--stop-on-fail") == 0) {
            g_sit_config.stop_on_fail = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            g_sit_config.verbose = true;
        } else if (strcmp(argv[i], "--listen-only") == 0) {
            g_sit_config.listen_only = true;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            g_sit_config.no_color = true;
        } else if (strcmp(argv[i], "--strict-visual") == 0) {
            g_sit_config.strict_visual = true;
        } else if (strcmp(argv[i], "--headless") == 0) {
            g_sit_config.headless = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Situation Test Harness\n");
            printf("Usage: sit_test [options]\n\n");
            printf("Options:\n");
            printf("  --module <name>    Run only the specified module\n");
            printf("  --filter <substr>  Run only tests matching substring\n");
            printf("  --listen-only      Run only tests marked for human listening\n");
            printf("  --list             List all tests without running\n");
            printf("  --stop-on-fail     Stop after first failure\n");
            printf("  --verbose          Print all assertion results\n");
            printf("  --no-color         Disable ANSI color output\n");
            printf("  --strict-visual    Treat visual assertions as hard failures\n");
            printf("  --headless         Hidden window; skip listen overlay and visible-window modules\n");
            printf("  --help, -h         Show this help\n");
            printf("\nEnvironment:\n");
            printf("  SIT_TEST_HEADLESS=1   Same as --headless (CLI wins if both set)\n");
            exit(0);
        }
    }

    if (!g_sit_config.headless) {
        const char* env_headless = getenv("SIT_TEST_HEADLESS");
        if (env_headless && env_headless[0]) {
#ifdef _WIN32
            if (_stricmp(env_headless, "0") != 0 && _stricmp(env_headless, "false") != 0
                && _stricmp(env_headless, "no") != 0) {
                g_sit_config.headless = true;
            }
#else
            if (strcasecmp(env_headless, "0") != 0 && strcasecmp(env_headless, "false") != 0
                && strcasecmp(env_headless, "no") != 0) {
                g_sit_config.headless = true;
            }
#endif
        }
    }

    g_sit_test_headless = g_sit_config.headless;

    // Enable ANSI on Windows
#ifdef _WIN32
    InitializeCriticalSection(&g_sit_harness_stderr_cs);
    if (!g_sit_config.no_color) {
        HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}

static void sit_test_list_all(void) {
    for (int m = 0; m < g_sit_module_count; m++) {
        const SitTestModule* mod = g_sit_modules[m];
        for (int t = 0; t < mod->test_count; t++) {
            fprintf(stderr, "[%s] %s (context: %s, listen: %s)\n",
                    mod->name, mod->tests[t].name,
                    mod->tests[t].requires_context ? "yes" : "no",
                    mod->tests[t].listen_test ? "yes" : "no");
        }
    }
}

SitTestResults sit_test_run_all(void) {
    SitTestResults results = {0};
    double start_time = sit_get_time_seconds();

    if (g_sit_config.list_only) {
        sit_test_list_all();
        results.elapsed_seconds = sit_get_time_seconds() - start_time;
        return results;
    }

    if (!sit_test_scratch_ensure()) {
        fprintf(stderr, "%sWARN%s: Could not create %s — temp-file tests may fail\n",
                sit_color(SIT_TEST_COLOR_YELLOW), sit_color(SIT_TEST_COLOR_RESET),
                SIT_TEST_SCRATCH_DIR);
    }

    // Validate module filter
    if (g_sit_config.filter_module) {
        bool found = false;
        for (int m = 0; m < g_sit_module_count; m++) {
            if (sit_strcasecmp_match(g_sit_modules[m]->name, g_sit_config.filter_module)) {
                found = true;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "%sERROR%s: Module '%s' not found. Available modules:\n",
                    sit_color(SIT_TEST_COLOR_RED), sit_color(SIT_TEST_COLOR_RESET),
                    g_sit_config.filter_module);
            for (int m = 0; m < g_sit_module_count; m++) {
                fprintf(stderr, "  - %s\n", g_sit_modules[m]->name);
            }
            results.failed = 1;
            results.total = 1;
            return results;
        }
    }

    // Pre-compute first and last context-requiring module indices (for shared context)
    // NOTE: Shared context is DISABLED. Each module does its own init/shutdown.
    // A small delay between context-requiring modules prevents resource contention.
    int first_ctx_module = -1;
    int last_ctx_module = -1;
    (void)first_ctx_module;
    (void)last_ctx_module;

    for (int m = 0; m < g_sit_module_count; m++) {
        const SitTestModule* mod = g_sit_modules[m];

        // Module filter
        if (g_sit_config.filter_module && !sit_strcasecmp_match(mod->name, g_sit_config.filter_module)) {
            results.skipped += mod->test_count;
            results.total += mod->test_count;
            continue;
        }

        if (g_sit_config.headless && mod->requires_visible_window) {
            sit_harness_stderr_enter();
            fprintf(stderr, "\n%s[%s]%s (%d tests) — skipped (headless)\n",
                    sit_color(SIT_TEST_COLOR_CYAN), mod->name, sit_color(SIT_TEST_COLOR_RESET),
                    mod->test_count);
            sit_harness_stderr_leave();
            results.skipped += mod->test_count;
            results.total += mod->test_count;
            continue;
        }

        sit_harness_stderr_enter();
        fprintf(stderr, "\n%s[%s]%s (%d tests)\n",
                sit_color(SIT_TEST_COLOR_CYAN), mod->name, sit_color(SIT_TEST_COLOR_RESET),
                mod->test_count);
        sit_harness_stderr_leave();

        // Module setup
        bool setup_ok = true;
        if (mod->setup) {
            /* Set name for crash reporting (atexit handler) */
            static char _setup_label[128];
            snprintf(_setup_label, sizeof(_setup_label), "%s.(setup)", mod->name);
            g_sit_current_test_name = _setup_label;
            g_sit_current_test_failed = false;
            if (setjmp(g_sit_test_jmp_buf) == 0) {
                mod->setup();
            } else {
                setup_ok = false;
            }
            if (g_sit_current_test_failed) setup_ok = false;
            g_sit_current_test_name = NULL;
        }

        if (setup_ok && mod->harness_visual && sit_test_harness_visual_enabled()) {
            sit_test_harness_visual_begin(mod->name);
        }

        if (!setup_ok) {
            fprintf(stderr, "  %sSETUP FAILED%s — skipping all tests in module\n",
                    sit_color(SIT_TEST_COLOR_YELLOW), sit_color(SIT_TEST_COLOR_RESET));
            results.skipped += mod->test_count;
            results.total += mod->test_count;
            if (mod->teardown) mod->teardown();
            continue;
        }

        // Run tests
        for (int t = 0; t < mod->test_count; t++) {
            SitTestCase* tc = &mod->tests[t];
            results.total++;

            // Test name filter
            if (g_sit_config.filter_test && !sit_test_matches_filter(tc->name, g_sit_config.filter_test)) {
                results.skipped++;
                continue;
            }

            if (g_sit_config.listen_only && !tc->listen_test) {
                results.skipped++;
                continue;
            }

            g_sit_current_test_failed = false;
            g_sit_current_test_skipped = false;
            g_sit_current_test_name = tc->name;
            g_sit_assertion_count = 0;

            if (mod->harness_visual && sit_test_harness_visual_enabled()) {
                sit_test_harness_visual_on_test_start(tc->name);
            }

            double test_start = sit_get_time_seconds();

            if (setjmp(g_sit_test_jmp_buf) == 0) {
                tc->func();
            }

            double test_elapsed = (sit_get_time_seconds() - test_start) * 1000.0;

            if (mod->harness_visual && sit_test_harness_visual_enabled()) {
                sit_test_harness_visual_on_test_end();
            }

            if (g_sit_current_test_skipped) {
                results.skipped++;
                sit_harness_stderr_enter();
                if (g_sit_test_skip_reason[0]) {
                    fprintf(stderr, "  %s[SKIP] %s%s — %s %s(%.1fms)%s\n",
                            sit_color(SIT_TEST_COLOR_YELLOW), tc->name, sit_color(SIT_TEST_COLOR_RESET),
                            g_sit_test_skip_reason,
                            sit_color(SIT_TEST_COLOR_DIM), test_elapsed, sit_color(SIT_TEST_COLOR_RESET));
                } else {
                    fprintf(stderr, "  %s[SKIP] %s%s %s(%.1fms)%s\n",
                            sit_color(SIT_TEST_COLOR_YELLOW), tc->name, sit_color(SIT_TEST_COLOR_RESET),
                            sit_color(SIT_TEST_COLOR_DIM), test_elapsed, sit_color(SIT_TEST_COLOR_RESET));
                }
                sit_harness_stderr_leave();
            } else if (g_sit_current_test_failed) {
                results.failed++;
                if (g_sit_failed_test_count < SIT_MAX_FAILED_TESTS) {
                    snprintf(g_sit_failed_tests[g_sit_failed_test_count],
                             SIT_MAX_FAILED_TEST_NAME,
                             "%s.%s",
                             mod->name,
                             tc->name);
                    g_sit_failed_test_count++;
                }
                fprintf(stderr, "  %s[FAIL] %s%s %s(%.1fms)%s\n",
                        sit_color(SIT_TEST_COLOR_RED), tc->name, sit_color(SIT_TEST_COLOR_RESET),
                        sit_color(SIT_TEST_COLOR_DIM), test_elapsed, sit_color(SIT_TEST_COLOR_RESET));

                if (g_sit_config.stop_on_fail) {
                    // Skip remaining tests
                    for (int rt = t + 1; rt < mod->test_count; rt++) {
                        results.skipped++;
                        results.total++;
                    }
                    for (int rm = m + 1; rm < g_sit_module_count; rm++) {
                        results.skipped += g_sit_modules[rm]->test_count;
                        results.total += g_sit_modules[rm]->test_count;
                    }
                    if (mod->harness_visual && sit_test_harness_visual_enabled()) {
                        sit_test_harness_visual_end();
                    }
                    if (mod->teardown) mod->teardown();
                    goto done;
                }
            } else {
                results.passed++;
                sit_harness_stderr_enter();
                fprintf(stderr, "  %s[ OK ] %s%s %s(%.1fms)%s\n",
                        sit_color(SIT_TEST_COLOR_GREEN), tc->name, sit_color(SIT_TEST_COLOR_RESET),
                        sit_color(SIT_TEST_COLOR_DIM), test_elapsed, sit_color(SIT_TEST_COLOR_RESET));
                sit_harness_stderr_leave();
            }
        }

        if (mod->harness_visual && sit_test_harness_visual_enabled()) {
            sit_test_harness_visual_end();
        }

        // Module teardown
        if (mod->teardown) {
            mod->teardown();
        }
    }

done:
    results.elapsed_seconds = sit_get_time_seconds() - start_time;
    results.visual_warnings = g_sit_visual_warning_count;
    return results;
}

void sit_test_print_results(SitTestResults results) {
    sit_harness_stderr_enter();
    fprintf(stderr, "\n%s============================================%s\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET));

    if (results.failed == 0) {
        fprintf(stderr, "%sRESULTS%s: %d total, %s%d passed%s, %d failed, %d skipped\n",
                sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET),
                results.total,
                sit_color(SIT_TEST_COLOR_GREEN), results.passed, sit_color(SIT_TEST_COLOR_RESET),
                results.failed, results.skipped);
    } else {
        fprintf(stderr, "%sRESULTS%s: %d total, %d passed, %s%d failed%s, %d skipped\n",
                sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET),
                results.total, results.passed,
                sit_color(SIT_TEST_COLOR_RED), results.failed, sit_color(SIT_TEST_COLOR_RESET),
                results.skipped);
    }

    if (results.visual_warnings > 0) {
        fprintf(stderr, "%sVISUAL%s: %s%d visual assertions skipped%s (use --strict-visual to enforce)\n",
                sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET),
                sit_color(SIT_TEST_COLOR_YELLOW), results.visual_warnings, sit_color(SIT_TEST_COLOR_RESET));
    }

    if (results.failed > 0 && g_sit_failed_test_count > 0) {
        fprintf(stderr, "%sFAILED TESTS%s:\n",
                sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET));
        for (int i = 0; i < g_sit_failed_test_count; i++) {
            fprintf(stderr, "  - %s\n", g_sit_failed_tests[i]);
        }
    }

    fprintf(stderr, "%sTIME%s: %.2fs\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET),
            results.elapsed_seconds);

    fprintf(stderr, "%s============================================%s\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET));
    sit_harness_stderr_leave();
}

#endif // SIT_TEST_IMPLEMENTATION

#endif // SIT_TEST_FRAMEWORK_H
