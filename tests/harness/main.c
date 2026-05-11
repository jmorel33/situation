/**
 * @file main.c
 * @brief Situation Test Harness — Entry Point
 *
 * Parses CLI arguments, installs signal handlers for crash recovery,
 * registers all test modules, and runs the suite.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#define SIT_TEST_IMPLEMENTATION
#include "sit_test_framework.h"

#include <signal.h>

// ============================================================================
//  Signal Handling (Crash Recovery)
// ============================================================================

static volatile sig_atomic_t g_signal_caught = 0;

static void sit_signal_handler(int sig) {
    g_signal_caught = sig;
    const char* sig_name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: sig_name = "SIGSEGV (Segmentation Fault)"; break;
        case SIGABRT: sig_name = "SIGABRT (Abort)"; break;
#ifndef _WIN32
        case SIGALRM: sig_name = "SIGALRM (Timeout)"; break;
#endif
        default: break;
    }

    fprintf(stderr, "\n    %sCRASH%s: Signal %s in test '%s'\n",
            sit_color(SIT_TEST_COLOR_RED), sit_color(SIT_TEST_COLOR_RESET),
            sig_name, g_sit_current_test_name ? g_sit_current_test_name : "(unknown)");

    // Mark test as failed and jump back to runner
    g_sit_current_test_failed = true;
    longjmp(g_sit_test_jmp_buf, 1);
}

static void sit_install_signal_handlers(void) {
    signal(SIGSEGV, sit_signal_handler);
    signal(SIGABRT, sit_signal_handler);
#ifndef _WIN32
    signal(SIGALRM, sit_signal_handler);
#endif
}

static void sit_restore_signal_handlers(void) {
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
#ifndef _WIN32
    signal(SIGALRM, SIG_DFL);
#endif
}

// ============================================================================
//  External: Module Registration
// ============================================================================

extern void sit_test_register_all(void);

// ============================================================================
//  Main
// ============================================================================

int main(int argc, char** argv) {
    // Initialize framework (parse CLI)
    sit_test_init(argc, argv);

    // Print banner
    fprintf(stderr, "%s[HARNESS]%s Situation Test Harness v1.0\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET));
#if defined(SITUATION_USE_VULKAN)
    fprintf(stderr, "%s[HARNESS]%s Backend: Vulkan\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET));
#elif defined(SITUATION_USE_OPENGL)
    fprintf(stderr, "%s[HARNESS]%s Backend: OpenGL\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET));
#else
    fprintf(stderr, "%s[HARNESS]%s Backend: None (context-free tests only)\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET));
#endif

    // Register all modules
    sit_test_register_all();

    fprintf(stderr, "%s[HARNESS]%s Running %d modules\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET),
            g_sit_module_count);

    // Install crash handlers
    sit_install_signal_handlers();

    // Run
    SitTestResults results = sit_test_run_all();

    // Restore handlers
    sit_restore_signal_handlers();

    // Print summary
    if (!g_sit_config.list_only) {
        sit_test_print_results(results);
    }

    return results.failed > 0 ? 1 : 0;
}
