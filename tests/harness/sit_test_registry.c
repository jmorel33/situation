/**
 * @file sit_test_registry.c
 * @brief Test module registration for the Situation Test Harness
 *
 * Registers all test modules in execution order.
 * Context-free modules first (filesystem, threading), then context-dependent.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_test_framework.h"

// ============================================================================
//  External Module Declarations
//  (Each test_*.c file exposes a const SitTestModule)
// ============================================================================

// Phase 2 — Context-free modules
extern const SitTestModule g_module_filesystem;
extern const SitTestModule g_module_threading;

// Phase 3 — Core context modules
extern const SitTestModule g_module_core;
extern const SitTestModule g_module_window;
extern const SitTestModule g_module_input;
extern const SitTestModule g_module_timer;

// Phase 4 — Graphics
extern const SitTestModule g_module_graphics;

// Phase 5 — Audio
extern const SitTestModule g_module_audio;

// Phase 6 — Miscellaneous
extern const SitTestModule g_module_misc;

// ============================================================================
//  Registration
// ============================================================================

void sit_test_register_all(void) {
    sit_test_register_module(&g_module_filesystem);
    sit_test_register_module(&g_module_threading);
    sit_test_register_module(&g_module_core);
    sit_test_register_module(&g_module_window);
    sit_test_register_module(&g_module_input);
    sit_test_register_module(&g_module_timer);
    sit_test_register_module(&g_module_audio);
    sit_test_register_module(&g_module_graphics);
    sit_test_register_module(&g_module_misc);
}
