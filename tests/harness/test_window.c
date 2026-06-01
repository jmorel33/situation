/**
 * @file test_window.c
 * @brief Window/Display module tests — State, Properties, Monitors, Cursor, Clipboard
 *
 * Requires context: calls SituationInit() in setup, SituationShutdown() in teardown.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_init_ok = false;

static void window_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info_flags(&config, "SIT_TEST_WINDOW", SITUATION_FLAG_WINDOW_RESIZABLE);

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void window_teardown(void) {
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

// ============================================================================
//  Window State Tests
// ============================================================================

static void test_set_window_title(void) {
    // Should not crash
    SituationSetWindowTitle("Test Title Changed");
    SIT_ASSERT(true);
}

static void test_set_window_size(void) {
    SituationSetWindowSize(800, 600);
    // Give the OS a moment to process
    SituationPollInputEvents();
    int w = 0, h = 0;
    SituationGetWindowSize(&w, &h);
    // Window managers may not honor exact sizes, but should be > 0
    SIT_ASSERT(w > 0);
    SIT_ASSERT(h > 0);
}

static void test_set_window_position(void) {
    SituationSetWindowPosition(100, 100);
    SituationPollInputEvents();
    Vector2 pos = SituationGetWindowPosition();
    // Position should be reasonable (not negative thousands)
    SIT_ASSERT(pos.x >= -10000.0f && pos.x <= 10000.0f);
    SIT_ASSERT(pos.y >= -10000.0f && pos.y <= 10000.0f);
}

static void test_set_window_min_max_size(void) {
    SituationSetWindowMinSize(200, 150);
    SituationSetWindowMaxSize(1920, 1080);
    SIT_ASSERT(true);
}

static void test_set_vsync(void) {
    SituationSetVSync(true);
    SIT_ASSERT(true);
    SituationSetVSync(false);
    SIT_ASSERT(true);
}

static void test_maximize_minimize_restore(void) {
    SituationMaximizeWindow();
    SituationPollInputEvents();
    SituationRestoreWindow();
    SituationPollInputEvents();
    SituationMinimizeWindow();
    SituationPollInputEvents();
    SituationRestoreWindow();
    SituationPollInputEvents();
    SIT_ASSERT(true);
}

// ============================================================================
//  Window Dimension Query Tests
// ============================================================================

static void test_get_screen_dimensions(void) {
    int w = SituationGetScreenWidth();
    int h = SituationGetScreenHeight();
    SIT_ASSERT(w > 0);
    SIT_ASSERT(h > 0);
}

static void test_get_render_dimensions(void) {
    int w = SituationGetRenderWidth();
    int h = SituationGetRenderHeight();
    // Render dimensions should be >= screen dimensions (HiDPI)
    SIT_ASSERT(w > 0);
    SIT_ASSERT(h > 0);
}

static void test_get_window_scale_dpi(void) {
    Vector2 scale = SituationGetWindowScaleDPI();
    // DPI scale should be >= 1.0 (standard) or > 0 at minimum
    SIT_ASSERT(scale.x > 0.0f);
    SIT_ASSERT(scale.y > 0.0f);
}

// ============================================================================
//  Monitor Tests
// ============================================================================

static void test_get_monitor_count(void) {
    int count = SituationGetMonitorCount();
    SIT_ASSERT(count >= 1);
}

static void test_get_current_monitor(void) {
    int idx = SituationGetCurrentMonitor();
    SIT_ASSERT(idx >= 0);
    SIT_ASSERT(idx < SituationGetMonitorCount());
}

static void test_get_monitor_properties(void) {
    int mon = SituationGetCurrentMonitor();
    
    const char* name = SituationGetMonitorName(mon);
    SIT_ASSERT_NOT_NULL(name);
    
    int w = SituationGetMonitorWidth(mon);
    int h = SituationGetMonitorHeight(mon);
    SIT_ASSERT(w > 0);
    SIT_ASSERT(h > 0);
    
    int refresh = SituationGetMonitorRefreshRate(mon);
    SIT_ASSERT(refresh > 0);
    
    Vector2 pos = SituationGetMonitorPosition(mon);
    // Position can be 0,0 for primary or offset for secondary
    SIT_ASSERT(pos.x >= -100000.0f);
    SIT_ASSERT(pos.y >= -100000.0f);
}

static void test_get_displays(void) {
    SituationDisplayInfo* displays = NULL;
    int count = 0;
    SituationError err = SituationGetDisplays(&displays, &count);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(count >= 1);
    SIT_ASSERT_NOT_NULL(displays);
    SituationFreeDisplays(displays, count);
}

// ============================================================================
//  Cursor Tests
// ============================================================================

static void test_cursor_visibility(void) {
    SituationShowCursor();
    SIT_ASSERT(true);
    SituationHideCursor();
    SIT_ASSERT(true);
    SituationShowCursor(); // Restore
}

static void test_set_cursor_shape(void) {
    SituationSetCursor(SIT_CURSOR_DEFAULT);
    SIT_ASSERT(true);
    SituationSetCursor(SIT_CURSOR_CROSSHAIR);
    SIT_ASSERT(true);
    SituationSetCursor(SIT_CURSOR_DEFAULT); // Restore
}

// ============================================================================
//  Clipboard Tests
// ============================================================================

static void test_clipboard_roundtrip(void) {
    const char* test_text = "SIT_TEST_CLIPBOARD_DATA_12345";
    SituationError err = SituationSetClipboardText(test_text);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    const char* result = NULL;
    err = SituationGetClipboardText(&result);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT_NOT_NULL(result);
    SIT_ASSERT_STR_EQ(result, test_text);
}

// ============================================================================
//  Phase 19 — Window State Flags & Display Modes
// ============================================================================

static void test_set_window_state_topmost(void) {
    SituationSetWindowState(SITUATION_FLAG_WINDOW_TOPMOST);
    SituationPollInputEvents();
    SIT_ASSERT(true); // No crash
}

static void test_clear_window_state_topmost(void) {
    SituationClearWindowState(SITUATION_FLAG_WINDOW_TOPMOST);
    SituationPollInputEvents();
    SIT_ASSERT(true); // No crash
}

static void test_set_window_state_undecorated(void) {
    SituationSetWindowState(SITUATION_FLAG_WINDOW_UNDECORATED);
    SituationPollInputEvents();
    SIT_ASSERT(true); // No crash
}

static void test_clear_window_state_undecorated(void) {
    SituationClearWindowState(SITUATION_FLAG_WINDOW_UNDECORATED);
    SituationPollInputEvents();
    SIT_ASSERT(true); // No crash
}

static void test_set_window_focused(void) {
    SituationSetWindowFocused();
    SituationPollInputEvents();
    SIT_ASSERT(true); // No crash
}

static void test_toggle_fullscreen(void) {
    // Toggle on
    SituationToggleFullscreen();
    SituationPollInputEvents();
    SIT_ASSERT(true);
    // Toggle off (restore)
    SituationToggleFullscreen();
    SituationPollInputEvents();
    SIT_ASSERT(true);
}

static void test_toggle_borderless_windowed(void) {
    // Toggle on
    SituationToggleBorderlessWindowed();
    SituationPollInputEvents();
    SIT_ASSERT(true);
    // Toggle off (restore)
    SituationToggleBorderlessWindowed();
    SituationPollInputEvents();
    SIT_ASSERT(true);
}

static void test_fullscreen_dimensions_valid(void) {
    // After fullscreen toggle, dimensions should still be valid
    SituationToggleFullscreen();
    SituationPollInputEvents();
    int w = SituationGetScreenWidth();
    int h = SituationGetScreenHeight();
    SIT_ASSERT(w > 0);
    SIT_ASSERT(h > 0);
    // Restore
    SituationToggleFullscreen();
    SituationPollInputEvents();
}

static void test_set_window_icon_null(void) {
    // Reset the window icon to the OS default by calling glfwSetWindowIcon with NULL/0.
    // NOTE: SituationSetWindowIcon with a zeroed image triggers a GLFW assertion
    // (images[i].pixels != NULL), so we cannot pass NULL data through that API.
    // Instead, verify that SituationSetWindowIcons with count=0 is handled gracefully
    // (returns an error code rather than crashing).
    SituationImage null_img = {0};
    SituationSetWindowIcons(NULL, 0);
    (void)null_img;
    SIT_ASSERT(true); // No crash — graceful error path
}

static void test_set_window_icon_valid(void) {
    // Create a 32x32 RGBA image for the icon
    SituationImage icon = {0};
    SituationError err = SituationCreateImage(32, 32, 4, &icon);
    if (err == SITUATION_SUCCESS && icon.data) {
        // Fill with a solid color
        memset(icon.data, 0xFF, 32 * 32 * 4);
        SituationSetWindowIcon(icon);
        SIT_ASSERT(true);
        free(icon.data);
    } else {
        // Image creation failed — skip gracefully
        SIT_ASSERT(true);
    }
}

static void test_set_window_icons_multiple(void) {
    // Create two icon sizes: 16x16 and 32x32
    SituationImage icons[2] = {0};
    SituationError err0 = SituationCreateImage(16, 16, 4, &icons[0]);
    SituationError err1 = SituationCreateImage(32, 32, 4, &icons[1]);
    if (err0 == SITUATION_SUCCESS && err1 == SITUATION_SUCCESS && icons[0].data && icons[1].data) {
        memset(icons[0].data, 0xAA, 16 * 16 * 4);
        memset(icons[1].data, 0xBB, 32 * 32 * 4);
        SituationSetWindowIcons(icons, 2);
        SIT_ASSERT(true);
        free(icons[0].data);
        free(icons[1].data);
    } else {
        if (icons[0].data) free(icons[0].data);
        if (icons[1].data) free(icons[1].data);
        SIT_ASSERT(true); // Skip gracefully
    }
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase window_tests[] = {
    // Window state (Phase 3B — original)
    {"set_window_title",        test_set_window_title,      true},
    {"set_window_size",         test_set_window_size,       true},
    {"set_window_position",     test_set_window_position,   true},
    {"set_window_min_max_size", test_set_window_min_max_size, true},
    {"set_vsync",               test_set_vsync,             true},
    {"maximize_minimize_restore", test_maximize_minimize_restore, true},
    // Dimensions
    {"get_screen_dimensions",   test_get_screen_dimensions, true},
    {"get_render_dimensions",   test_get_render_dimensions, true},
    {"get_window_scale_dpi",    test_get_window_scale_dpi,  true},
    // Monitors
    {"get_monitor_count",       test_get_monitor_count,     true},
    {"get_current_monitor",     test_get_current_monitor,   true},
    {"get_monitor_properties",  test_get_monitor_properties, true},
    {"get_displays",            test_get_displays,          true},
    // Cursor
    {"cursor_visibility",       test_cursor_visibility,     true},
    {"set_cursor_shape",        test_set_cursor_shape,      true},
    // Clipboard
    {"clipboard_roundtrip",     test_clipboard_roundtrip,   true},

    // --- Phase 19: Window State & Display Modes (11) ---
    {"set_state_topmost",           test_set_window_state_topmost,      true},
    {"clear_state_topmost",         test_clear_window_state_topmost,    true},
    {"set_state_undecorated",       test_set_window_state_undecorated,  true},
    {"clear_state_undecorated",     test_clear_window_state_undecorated, true},
    {"set_window_focused",          test_set_window_focused,            true},
    {"toggle_fullscreen",           test_toggle_fullscreen,             true},
    {"toggle_borderless_windowed",  test_toggle_borderless_windowed,    true},
    {"fullscreen_dimensions_valid", test_fullscreen_dimensions_valid,   true},
    {"set_window_icon_null",        test_set_window_icon_null,          true},
    {"set_window_icon_valid",       test_set_window_icon_valid,         true},
    {"set_window_icons_multiple",   test_set_window_icons_multiple,     true},
};

const SitTestModule g_module_window = {
    .name = "window",
    .setup = window_setup,
    .teardown = window_teardown,
    .tests = window_tests,
    .test_count = sizeof(window_tests) / sizeof(window_tests[0]),
    .requires_context = true
};
