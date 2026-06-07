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
extern const SitTestModule g_module_proj;

// Phase 4 — Graphics / GPU (run before audio listen tests — scope overlay depends on this)
extern const SitTestModule g_module_graphics;
extern const SitTestModule g_module_virtual_display;
extern const SitTestModule g_module_compute;
extern const SitTestModule g_module_transfer;

// Phase 5 — Audio (after graphics; listen tests pump scope/spectrum on the main swapchain)
extern const SitTestModule g_module_audio;
extern const SitTestModule g_module_tone_synth;
extern const SitTestModule g_module_audio_effects_heard;

// Phase 5.5 — Model Loader (after graphics, real GLB assets)
extern const SitTestModule g_module_model_loader;

// Phase 6 — Miscellaneous
extern const SitTestModule g_module_misc;
extern const SitTestModule g_module_system_info;
extern const SitTestModule g_module_kterm_console;

// Phase 7 — Advanced (manual/visual stress; runs last)
extern const SitTestModule g_module_advanced;

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
    sit_test_register_module(&g_module_proj);
    sit_test_register_module(&g_module_graphics);
    sit_test_register_module(&g_module_virtual_display);
    sit_test_register_module(&g_module_compute);
    sit_test_register_module(&g_module_transfer);
    sit_test_register_module(&g_module_model_loader);
    sit_test_register_module(&g_module_audio);
    sit_test_register_module(&g_module_tone_synth);
    sit_test_register_module(&g_module_audio_effects_heard);
    sit_test_register_module(&g_module_misc);
    sit_test_register_module(&g_module_system_info);
    sit_test_register_module(&g_module_kterm_console);
    sit_test_register_module(&g_module_advanced);
}
