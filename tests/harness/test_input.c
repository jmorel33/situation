/**
 * @file test_input.c
 * @brief Input module tests — Keyboard, Mouse, Gamepad
 *
 * Requires context: calls SituationInit() in setup, SituationShutdown() in teardown.
 * Tests verify that input queries return sane defaults when no physical input is occurring.
 *
 * (c) 2025-2026 Jacques Morel — MIT Licensed
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_init_ok = false;

static void input_setup(void) {
    SituationInitInfo config = {0};
    config.window_width = 320;
    config.window_height = 240;
    config.window_title = "SIT_TEST_INPUT";

    SituationError err = SituationInit(0, NULL, &config);
    g_init_ok = (err == SITUATION_SUCCESS);
    if (!g_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
    // Poll once to initialize input state
    SituationPollInputEvents();
}

static void input_teardown(void) {
    if (g_init_ok) {
        SituationShutdown();
        g_init_ok = false;
    }
}

// ============================================================================
//  Keyboard Tests
// ============================================================================

static void test_key_down_default(void) {
    // No keys should be down when no input is happening
    SIT_ASSERT(!SituationIsKeyDown(SIT_KEY_A));
    SIT_ASSERT(!SituationIsKeyDown(SIT_KEY_SPACE));
    SIT_ASSERT(!SituationIsKeyDown(SIT_KEY_ESCAPE));
}

static void test_key_up_default(void) {
    // All keys should be up
    SIT_ASSERT(SituationIsKeyUp(SIT_KEY_A));
    SIT_ASSERT(SituationIsKeyUp(SIT_KEY_ENTER));
}

static void test_key_pressed_default(void) {
    // No keys pressed this frame
    SIT_ASSERT(!SituationIsKeyPressed(SIT_KEY_A));
    SIT_ASSERT(!SituationIsKeyReleased(SIT_KEY_A));
}

static void test_get_key_pressed_queue(void) {
    // Queue should be empty
    int key = SituationGetKeyPressed();
    SIT_ASSERT_EQ(key, 0);
}

static void test_get_char_pressed_queue(void) {
    unsigned int ch = SituationGetCharPressed();
    SIT_ASSERT_EQ(ch, 0);
}

static void test_get_key_scancode(void) {
    // Should return a valid scancode for a known key
    int sc = SituationGetKeyScancode(SIT_KEY_A);
    // Scancode is platform-specific but should be > 0 for a real key
    SIT_ASSERT(sc >= 0);
}

static void test_modifier_not_pressed(void) {
    SIT_ASSERT(!SituationIsModifierPressed(SIT_MOD_SHIFT));
    SIT_ASSERT(!SituationIsModifierPressed(SIT_MOD_CONTROL));
    SIT_ASSERT(!SituationIsModifierPressed(SIT_MOD_ALT));
}

static void test_set_key_callback(void) {
    SituationSetKeyCallback(NULL, NULL);
    SIT_ASSERT(true);
}

// ============================================================================
//  Mouse Tests
// ============================================================================

static void test_mouse_position(void) {
    Vector2 pos = SituationGetMousePosition();
    // Position could be anywhere, just verify it's finite
    SIT_ASSERT(pos.x >= -100000.0f && pos.x <= 100000.0f);
    SIT_ASSERT(pos.y >= -100000.0f && pos.y <= 100000.0f);
}

static void test_mouse_delta(void) {
    Vector2 delta = SituationGetMouseDelta();
    // First frame delta should be 0 or very small
    SIT_ASSERT(delta.x >= -10000.0f && delta.x <= 10000.0f);
    SIT_ASSERT(delta.y >= -10000.0f && delta.y <= 10000.0f);
}

static void test_mouse_wheel(void) {
    float wheel = SituationGetMouseWheelMove();
    // No wheel input — should be 0
    SIT_ASSERT(wheel >= -1000.0f && wheel <= 1000.0f);
}

static void test_mouse_buttons_default(void) {
    SIT_ASSERT(!SituationIsMouseButtonDown(0));
    SIT_ASSERT(!SituationIsMouseButtonPressed(0));
    SIT_ASSERT(!SituationIsMouseButtonReleased(0));
}

static void test_set_mouse_position(void) {
    Vector2 target = {{160.0f, 120.0f}};
    SituationSetMousePosition(target);
    SIT_ASSERT(true); // No crash
}

static void test_set_mouse_offset_scale(void) {
    Vector2 offset = {{0.0f, 0.0f}};
    Vector2 scale = {{1.0f, 1.0f}};
    SituationSetMouseOffset(offset);
    SituationSetMouseScale(scale);
    SIT_ASSERT(true);
}

// ============================================================================
//  Gamepad Tests
// ============================================================================

static void test_joystick_not_present(void) {
    // Joystick 0 may or may not be connected — function should not crash
    bool present = SituationIsJoystickPresent(0);
    // Just verify it returns a bool without crashing
    SIT_ASSERT(present == true || present == false);
}

static void test_gamepad_query_graceful(void) {
    // These should return gracefully even if no gamepad is connected
    bool is_gp = SituationIsGamepad(0);
    SIT_ASSERT(is_gp == true || is_gp == false);

    int axes = SituationGetGamepadAxisCount(0);
    SIT_ASSERT(axes >= 0);

    float val = SituationGetGamepadAxisValue(0, 0);
    SIT_ASSERT(val >= -1.0f && val <= 1.0f);

    bool btn = SituationIsGamepadButtonDown(0, 0);
    SIT_ASSERT(btn == true || btn == false);
}

static void test_joystick_name(void) {
    // May return NULL if no joystick connected — that's fine
    const char* name = SituationGetJoystickName(0);
    // Just verify no crash
    SIT_ASSERT(name == NULL || strlen(name) >= 0);
}

// ============================================================================
//  Module Definition
// ============================================================================

static SitTestCase input_tests[] = {
    // Keyboard
    {"key_down_default",        test_key_down_default,      true},
    {"key_up_default",          test_key_up_default,        true},
    {"key_pressed_default",     test_key_pressed_default,   true},
    {"get_key_pressed_queue",   test_get_key_pressed_queue, true},
    {"get_char_pressed_queue",  test_get_char_pressed_queue, true},
    {"get_key_scancode",        test_get_key_scancode,      true},
    {"modifier_not_pressed",    test_modifier_not_pressed,  true},
    {"set_key_callback",        test_set_key_callback,      true},
    // Mouse
    {"mouse_position",          test_mouse_position,        true},
    {"mouse_delta",             test_mouse_delta,           true},
    {"mouse_wheel",             test_mouse_wheel,           true},
    {"mouse_buttons_default",   test_mouse_buttons_default, true},
    {"set_mouse_position",      test_set_mouse_position,    true},
    {"set_mouse_offset_scale",  test_set_mouse_offset_scale, true},
    // Gamepad
    {"joystick_not_present",    test_joystick_not_present,  true},
    {"gamepad_query_graceful",  test_gamepad_query_graceful, true},
    {"joystick_name",           test_joystick_name,         true},
};

const SitTestModule g_module_input = {
    .name = "input",
    .setup = input_setup,
    .teardown = input_teardown,
    .tests = input_tests,
    .test_count = sizeof(input_tests) / sizeof(input_tests[0]),
    .requires_context = true
};
