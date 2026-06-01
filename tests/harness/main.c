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
//  Crash Recovery (POSIX signals + Windows SEH)
// ============================================================================

#ifdef _WIN32
static LONG WINAPI sit_win_exception_filter(EXCEPTION_POINTERS* info) {
    const char* name = "Windows exception";
    DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: name = "ACCESS_VIOLATION (0xC0000005)"; break;
        case EXCEPTION_STACK_OVERFLOW:   name = "STACK_OVERFLOW"; break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO: name = "INT_DIVIDE_BY_ZERO"; break;
        case EXCEPTION_ILLEGAL_INSTRUCTION: name = "ILLEGAL_INSTRUCTION"; break;
        default: break;
    }
    char detail[96];
    snprintf(detail, sizeof(detail), "%s (code 0x%08lx)", name, (unsigned long)code);
    sit_test_crash_recover(detail);
    return EXCEPTION_EXECUTE_HANDLER; /* unreachable */
}
#endif

#ifndef _WIN32
static void sit_signal_handler(int sig) {
    const char* sig_name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: sig_name = "SIGSEGV (Segmentation Fault)"; break;
        case SIGABRT: sig_name = "SIGABRT (Abort)"; break;
        case SIGALRM: sig_name = "SIGALRM (Timeout)"; break;
        default: break;
    }
    sit_test_crash_recover(sig_name);
}
#endif

static void sit_install_crash_handlers(void) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(sit_win_exception_filter);
#else
    signal(SIGSEGV, sit_signal_handler);
    signal(SIGABRT, sit_signal_handler);
    signal(SIGALRM, sit_signal_handler);
#endif
}

static void sit_restore_crash_handlers(void) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(NULL);
#else
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
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
    if (g_sit_config.headless) {
        fprintf(stderr, "%s[HARNESS]%s Mode: headless (hidden window, no listen overlay)\n",
                sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET));
    }

    // Register all modules
    sit_test_register_all();

    fprintf(stderr, "%s[HARNESS]%s Running %d modules\n",
            sit_color(SIT_TEST_COLOR_BOLD), sit_color(SIT_TEST_COLOR_RESET),
            g_sit_module_count);

    // Install crash handlers (Windows: SEH — signal() does not catch ACCESS_VIOLATION)
    sit_install_crash_handlers();

    // Run
    SitTestResults results = sit_test_run_all();

    // Restore handlers
    sit_restore_crash_handlers();

    // Print summary
    if (!g_sit_config.list_only) {
        sit_test_print_results(results);
    }

    return results.failed > 0 ? 1 : 0;
}
