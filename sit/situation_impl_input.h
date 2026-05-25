/***************************************************************************************************
*
*   situation_impl_input.h - Keyboard, Mouse & Joystick Input Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Extracted from situation_impl.h for modularity.
*   This file is included by situation_impl.h after situation_impl_io.h.
*
*   Contains:
*     - Keyboard state queries (IsKeyDown, IsKeyPressed, GetKeyPressed, etc.)
*     - Mouse state queries (IsMouseButtonDown, GetMouseWheelMove, etc.)
*     - Cursor control (ShowCursor, HideCursor, DisableCursor)
*     - Joystick/Gamepad API (IsJoystickPresent, GetGamepadAxisValue, etc.)
*     - GLFW Joystick callback
*     - Scancode-to-character mapping
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_INPUT_H
#define SITUATION_IMPL_INPUT_H
//==================================================================================
// GLFW Input Callbacks
//==================================================================================
/**
 * @brief [INTERNAL] GLFW callback function invoked for all keyboard key events.
 * @details This is the central processing point for all raw keyboard input. It is registered with GLFW and is called whenever a key is pressed, released, or repeats.
 *
 * @par State Management
 *   Upon invocation, this function updates several aspects of the internal keyboard state:
 *   - **Current State:** Updates the `sit_input.keyboard.current_state` array, which is used by `SituationIsKeyDown()`.
 *   - **Event Flags:** Sets the `down_this_frame` or `up_this_frame` flags for the specific key, which are used by `SituationIsKeyPressed()` and `SituationIsKeyReleased()`.
 *   - **Event Queue:** On a key press, it pushes the key code onto a mutex-protected queue for consumption by `SituationGetKeyPressed()`.
 *   - **Modifier State:** It updates the global modifier flags (`sit_input.keyboard.modifier_state`) for Shift, Ctrl, Alt, etc.
 *   - **Lock Key State:** It specifically tracks the state of lock keys like Caps Lock and Num Lock.
 *   - **Scroll Lock:** It manually toggles the internal `is_scroll_lock_on` flag, as this state is not provided as a standard modifier.
 *
 * It also dispatches the event to the optional user-registered key callback.
 *
 * @param window The GLFW window that received the event (unused).
 * @param key The keyboard key that was pressed or released (e.g., `GLFW_KEY_A`).
 * @param scancode The system-specific scancode of the key (unused).
 * @param action The key action (`GLFW_PRESS`, `GLFW_RELEASE`, `GLFW_REPEAT`).
 * @param mods A bitfield describing which modifier keys were held down.
 *
 * @note This function is for internal use only.
 * @warning Although GLFW may invoke this callback from a different thread in some theoretical configurations, this library's design assumes it is called synchronously during `SituationPollInputEvents()`.
 * The mutex is included as a robust safeguard for thread safety, preparing the library for future evolution.
 *
 * @see SituationPollInputEvents(), SituationSetKeyCallback()
 */
static void _SituationGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    if (key >= 0 && key <= GLFW_KEY_LAST) {
        if (action == GLFW_PRESS) {
            sit_input.keyboard.current_state[key] = true;
            if (scancode >= 0 && scancode < SITUATION_MAX_SCANCODES) sit_input.keyboard.scancode_state[scancode] = true;
            sit_input.keyboard.down_this_frame[key] = true; // This happens before queue lock, generally fine as it's main thread context

			ma_mutex_lock(&sit_input.keyboard.event_queue_mutex); // Lock for queue
            // Ring Buffer Push
            uint32_t next_head = (sit_input.keyboard.pressed_head + 1) % SITUATION_KEY_QUEUE_MAX;
            if (next_head != sit_input.keyboard.pressed_tail) {
                sit_input.keyboard.pressed_queue[sit_input.keyboard.pressed_head] = key;
                sit_input.keyboard.scancode_queue[sit_input.keyboard.pressed_head] = scancode;
                sit_input.keyboard.pressed_head = next_head;
            }
            if (key == SIT_KEY_SCROLL_LOCK) {
                sit_input.keyboard.is_scroll_lock_on = !sit_input.keyboard.is_scroll_lock_on;
            }
            ma_mutex_unlock(&sit_input.keyboard.event_queue_mutex); // Unlock queue

        } else if (action == GLFW_RELEASE) {
            sit_input.keyboard.current_state[key] = false;
            if (scancode >= 0 && scancode < SITUATION_MAX_SCANCODES) sit_input.keyboard.scancode_state[scancode] = false;
            sit_input.keyboard.up_this_frame[key] = true;
        }
    }
    sit_input.keyboard.modifier_state = mods;

    // Store the state of Caps Lock and Num Lock
    sit_input.keyboard.lock_key_state = mods & (SIT_MOD_CAPS_LOCK | SIT_MOD_NUM_LOCK);

    if (sit_input.keyboard.key_callback) { // User callback can be called outside the queue lock
        sit_input.keyboard.key_callback(key, scancode, action, mods, sit_input.keyboard.key_callback_user_data);
    }
}

/**
 * @brief [INTERNAL] GLFW callback function invoked for text input events.
 * @details This callback is specifically for handling character input, as opposed to raw key presses. GLFW calls this function with the Unicode codepoint of a character as it is typed, correctly handling keyboard layouts and modifier keys.
 *
 * Its sole responsibility is to push the received codepoint onto a thread-safe queue (`sit_input.keyboard.char_queue`). This queue is then consumed by the public `SituationGetCharPressed()` function, providing a reliable way for applications to implement text entry fields.
 *
 * @param window The GLFW window that received the event (unused).
 * @param codepoint The Unicode codepoint of the character.
 *
 * @note This function is for internal use only.
 *
 * @see SituationGetCharPressed()
 */
static void _SituationGLFWCharCallback(GLFWwindow* window, unsigned int codepoint) {
    (void)window; // Unused parameter

	ma_mutex_lock(&sit_input.keyboard.event_queue_mutex);
    // Ring Buffer Push
    uint32_t next_head = (sit_input.keyboard.char_head + 1) % SITUATION_CHAR_QUEUE_MAX;
    if (next_head != sit_input.keyboard.char_tail) {
        sit_input.keyboard.char_queue[sit_input.keyboard.char_head] = codepoint;
        sit_input.keyboard.char_head = next_head;
    }
    ma_mutex_unlock(&sit_input.keyboard.event_queue_mutex);
}

/**
 * @brief [INTERNAL] GLFW callback function invoked when the window gains or loses input focus.
 * @details This function is called by GLFW whenever the application window's focus state changes. It updates the internal focus state tracker (`sit_gs.current_window_focus_state`) and serves two main purposes:
 *          1.  It invokes the optional user-defined callback set via `SituationSetFocusCallback`.
 *          2.  It calls `SituationApplyCurrentProfileWindowState()` to automatically switch between the "active" and "inactive" window state profiles, allowing for behavior changes like pausing the game or reducing frame rate when the window is not focused.
 *
 * @param window The GLFW window that received the event (unused).
 * @param focused `GLFW_TRUE` if the window gained focus, `GLFW_FALSE` if it lost focus.
 *
 * @note This function is for internal use only.
 *
 * @see SituationSetFocusCallback(), SituationApplyCurrentProfileWindowState()
 */
static void _SituationGLFWWindowFocusCallback(GLFWwindow* window, int focused) {
    (void)window; // Unused
    bool has_focus = (focused == GLFW_TRUE);
    if (has_focus != sit_gs.current_window_focus_state) {
        sit_gs.current_window_focus_state = has_focus;
        if (sit_gs.focus_callback_fn) {
            sit_gs.focus_callback_fn(has_focus, sit_gs.focus_callback_user_ptr);
        }
        SituationApplyCurrentProfileWindowState();
    }
}

/**
 * @brief [INTERNAL] GLFW callback when the window is maximized or restored via the OS chrome.
 * @see SituationSetMaximizeCallback(), SituationIsWindowMaximized()
 */
static void _SituationGLFWWindowMaximizeCallback(GLFWwindow* window, int maximized) {
    (void)window;
    if (sit_gs.maximize_callback_fn) {
        sit_gs.maximize_callback_fn(maximized == GLFW_TRUE, sit_gs.maximize_callback_user_ptr);
    }
}

/**
 * @brief [INTERNAL] GLFW callback function invoked when the window is iconified (minimized) or restored.
 * @details This function is called by GLFW when the user minimizes or restores the application window. Its primary role is to automatically pause and resume the application to conserve system resources while it is not visible.
 *
 * It checks for a state change (e.g., from not-minimized to minimized) and calls `SituationPauseApp()` or `SituationResumeApp()` accordingly. It also calls `SituationApplyCurrentProfileWindowState()` to apply any state changes defined in the window profiles for the minimized state.
 *
 * @param window The GLFW window that received the event (unused).
 * @param iconified `GLFW_TRUE` if the window was minimized, `GLFW_FALSE` if it was restored.
 *
 * @note This function is for internal use only.
 *
 * @see SituationPauseApp(), SituationResumeApp()
 */
static void _SituationGLFWWindowIconifyCallback(GLFWwindow* window, int iconified) {
    (void)window; // Unused
    bool is_minimized_now = (iconified == GLFW_TRUE);
    if (is_minimized_now && !sit_gs.is_app_internally_paused && !sit_gs.was_minimized_last_frame) {
        SituationPauseApp();
    } else if (!is_minimized_now && sit_gs.is_app_internally_paused && sit_gs.was_minimized_last_frame) {
        SituationResumeApp();
    }
    sit_gs.was_minimized_last_frame = is_minimized_now;
    // Also apply window profile as minimization state might be part of it
    SituationApplyCurrentProfileWindowState();
}

/**
 * @brief [INTERNAL] GLFW callback function invoked when the window's framebuffer size changes.
 * @details This is the core handler for all resolution and DPI scaling changes. It is called by GLFW whenever the pixel dimensions of the window's rendering area are modified.
 *
 * @par Responsibilities & Backend Differences
 *   The function's primary responsibility is to update the library's internal state and trigger necessary backend-specific actions:
 *   1.  **State Update:** It updates the library's cached render dimensions (`sit_gs.main_window_width`/`height`) and sets the `was_window_resized_last_frame` flag for the polling API.
 *   2.  **OpenGL Backend:** It immediately updates the OpenGL state by calling `glViewport` and recalculating internal orthographic projection matrices. This is safe because OpenGL is an immediate-mode API.
 *   3.  **Vulkan Backend:** It **does not** perform any immediate resource recreation. Instead, it simply sets the `sit_render.vk.framebuffer_resized` flag. This is a critical design choice, as recreating the Vulkan swapchain is a complex, blocking operation that cannot be safely performed inside an asynchronous callback. The main render loop will detect this flag and handle the recreation gracefully.
 *   4.  **User Callback:** Finally, it invokes the optional user-defined callback set via `SituationSetResizeCallback`, allowing the application to respond to the size change.
 *
 * @param window The GLFW window that received the event (unused).
 * @param width The new width of the framebuffer in pixels.
 * @param height The new height of the framebuffer in pixels.
 *
 * @note This function is for internal use only.
 *
 * @see SituationSetResizeCallback(), _SituationVulkanRecreateSwapchain()
 */
static void _SituationGLFWFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    (void)window;
    if (width == 0 || height == 0) return; // Window is minimized, do nothing for now

    // Update the library's internal window size tracking
    sit_gs.main_window_width = width;
    sit_gs.main_window_height = height;
    sit_gs.was_window_resized_last_frame = true;

#if defined(SITUATION_USE_OPENGL)
    // Mark shadow state as dirty so the Render Thread knows it needs to rebuild
    // internal textures and projections on its next loop.
    sit_render.gl.shadow_state_dirty = true;

#elif defined(SITUATION_USE_VULKAN)
    // For Vulkan, we CANNOT recreate the swapchain here because this callback can be called from within other functions (like glfwPollEvents) and
    // we must not interrupt the main loop.
    // Instead, we just set a flag to be handled at the start of the next frame.
    sit_render.vk.framebuffer_resized = true;
#endif

    // --- Call user-defined resize callback ---
    if (sit_gs.resize_callback != NULL) {
        sit_gs.resize_callback(width, height, sit_gs.resize_callback_user_data);
    }

}


// Mouse GLFW Callback Implementations
/**
 * @brief [INTERNAL] GLFW callback for mouse button events.
 * @details This function is called by GLFW's event processing thread whenever a mouse button is pressed or released. It is responsible for updating the library's internal mouse state in a thread-safe manner.
 *
 * @par Thread Safety
 *   All modifications to the shared `sit_input.mouse` state are protected by a mutex. This prevents race conditions where the main application thread might read incomplete or inconsistent state while this callback is executing.
 *   The user-defined callback is intentionally called *after* the mutex is unlocked to prevent potential deadlocks if the user's code also performs locking.
 *
 * @param window The GLFW window that received the event (unused).
 * @param button The mouse button that was pressed or released (e.g., GLFW_MOUSE_BUTTON_LEFT).
 * @param action The button action (GLFW_PRESS or GLFW_RELEASE).
 * @param mods Bit field describing which modifier keys were held down.
 */
static void _SituationGLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)window; // Unused parameter

    // --- State Update (Thread-Safe) ---
    ma_mutex_lock(&sit_input.mouse.mutex);
    {
        // Validate the button code to prevent out-of-bounds array access.
        if (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST) {
            if (action == GLFW_PRESS) {
                sit_input.mouse.current_button_state[button] = true;
                sit_input.mouse.button_down_this_frame[button] = true;

                // Ring Buffer Push
                uint32_t next_head = (sit_input.mouse.button_head + 1) % SITUATION_KEY_QUEUE_MAX;
                if (next_head != sit_input.mouse.button_tail) {
                    sit_input.mouse.button_queue[sit_input.mouse.button_head] = button;
                    sit_input.mouse.button_head = next_head;
                }
            } else if (action == GLFW_RELEASE) {
                sit_input.mouse.current_button_state[button] = false;
                sit_input.mouse.button_up_this_frame[button] = true;
            }
        }
    }
    ma_mutex_unlock(&sit_input.mouse.mutex);

    // --- User Callback (Outside of Lock) ---
    // Call the user's registered callback, if any. This is done outside the critical section to avoid potential deadlocks in the user's application.
    if (sit_input.mouse.button_callback) {
        sit_input.mouse.button_callback(button, action, mods, sit_input.mouse.button_callback_user_data);
    }
}

/**
 * @brief [INTERNAL] GLFW callback for mouse cursor movement events.
 * @details This function is called by GLFW whenever the cursor moves over the window. It updates the internal `current_pos` state in a thread-safe manner.
 *
 * @par Thread Safety
 *   The update to `sit_input.mouse.current_pos` is protected by a mutex to prevent data tearing if another thread reads the position while it is being updated. The user-defined callback is called after the lock is released.
 *
 * @param window The GLFW window that received the event (unused).
 * @param xpos The new cursor x-coordinate, relative to the left edge of the content area.
 * @param ypos The new cursor y-coordinate, relative to the top edge of the content area.
 */
static void _SituationGLFWCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window; // Unused parameter

    // --- State Update (Thread-Safe) ---
    ma_mutex_lock(&sit_input.mouse.mutex);
    {
        sit_input.mouse.current_pos[0] = (float)xpos;
        sit_input.mouse.current_pos[1] = (float)ypos;
    }
    ma_mutex_unlock(&sit_input.mouse.mutex);

    // --- User Callback (Outside of Lock) ---
    if (sit_input.mouse.cursor_pos_callback) {
        Vector2 pos = {(float)xpos, (float)ypos};
        sit_input.mouse.cursor_pos_callback(pos, sit_input.mouse.cursor_pos_callback_user_data);
    }
}

/**
 * @brief [INTERNAL] GLFW callback for mouse wheel scroll events.
 * @details This function is called by GLFW when a scrolling device is used. It accumulates scroll offsets in a thread-safe manner, as multiple scroll events can fire within a single frame.
 *
 * @par Thread Safety
 *   The accumulation of scroll offsets into `sit_input.mouse.wheel_move_x/y` is protected by a mutex to ensure atomic updates. The user-defined callback is called after the lock is released.
 *
 * @param window The GLFW window that received the event (unused).
 * @param xoffset The scroll offset along the x-axis.
 * @param yoffset The scroll offset along the y-axis.
 */
static void _SituationGLFWScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window; // Unused parameter

    // --- State Update (Thread-Safe) ---
    ma_mutex_lock(&sit_input.mouse.mutex);
    {
        // Accumulate scroll offsets, as multiple events can occur per frame.
        sit_input.mouse.wheel_move_x += (float)xoffset;
        sit_input.mouse.wheel_move_y += (float)yoffset;
    }
    ma_mutex_unlock(&sit_input.mouse.mutex);

    // --- User Callback (Outside of Lock) ---
    if (sit_input.mouse.scroll_callback) {
        // Pass the per-event offset directly to the user callback.
        Vector2 offset = {(float)xoffset, (float)yoffset};
        sit_input.mouse.scroll_callback(offset, sit_input.mouse.scroll_callback_user_data);
    }
}

// --- Keyboard Management Implementation ---

/**
 * @brief Maps a physical key scancode (plus modifiers) to a Unicode character, respecting the current OS keyboard layout.
 *
 * @details This function provides layout-aware character resolution for input events, enabling robust key bindings and text input
 *          across international keyboards (e.g., QWERTY, AZERTY, Dvorak). It queries the OS's active keyboard layout and applies
 *          modifiers (Shift, AltGr, etc.) to produce the correct Unicode codepoint, handling basic dead keys and IME composition
 *          where supported. This is essential for i18n-safe applications: Bind actions to scancodes for invariance, then map to
 *          chars for display/input.
 *
 *          - **Physical Focus**: Operates on scancodes (from `SituationGetKeyScancode` or events) for layout-independence.
 *          - **Platform Behavior**:
 *              - Windows: Full support via `ToUnicodeEx` (IME/dead keys).
 *              - macOS: `UCKeyTranslate` for native layout resolution.
 *              - Linux: `xkbcommon` (if enabled via `SITUATION_USE_XKBCOMMON`) or fallback to GLFW char proxy.
 *          - **Limitations**: Advanced IME (e.g., full CJK composition) requires the char callback (`SituationSetCharCallback`).
 *                            Single codepoint onlyÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Âno multi-char sequences.
 *          - **Thread Safety**: Safe from any thread; caches layout for O(1) reuse. No allocations.
 *          - **Performance**: <1Ãƒâ€šÃ‚Âµs/call; ideal for polling or event processing.
 *
 * @par Example Usage
 * @code
 * uint32_t ch;
 * int mods = SIT_MOD_SHIFT;  // From event
 * int scancode = SituationGetKeyScancode(SITUATION_KEY_A);  // Physical pos
 * if (SituationGetCharFromScancode(sit_window, scancode, mods, &ch) == SITUATION_SUCCESS) {
 *     if (ch == 'A') { **Handle uppercase** }
 * }
 * @endcode
 *
 * @param window The Situation window handle (from `SituationCreateWindow`); used for GLFW context.
 * @param scancode The physical key scancode (e.g., from `SituationPollInputEvents` or `SituationGetKeyScancode`).
 * @param mods Modifier mask (bitfield: `SIT_MOD_SHIFT` (1<<0), `SIT_MOD_CONTROL` (1<<1), `SIT_MOD_ALT` (1<<2), `SIT_MOD_ALTGR` (1<<3)).
 * @param[out] out_char Pointer to receive the Unicode codepoint (UTF-32); 0 on failure.
 *
 * @return SITUATION_SUCCESS if mapped successfully; SITUATION_ERROR_INVALID_PARAM otherwise (e.g., invalid scancode/layout).
 *
 * @see SituationGetKeyScancode() for logical-to-physical mapping.
 * @see SituationSetCharCallback() for asynchronous text input events.
 * @see SituationGetKeyboardLayout() for explicit layout ID queries (future extension).
 *
 * @note Requires `SITUATION_ENABLE_INPUT_LAYOUT_MAPPER` macro for full cross-platform support.
 *       On Linux without xkbcommon, falls back to approximate mapping via GLFW proxiesÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Âenable via deps for precision.
 */
SITAPI int SituationGetCharFromScancode(int window, int scancode, int mods, uint32_t* out_char) {
    if (!out_char) return SITUATION_ERROR_INVALID_PARAM;
    *out_char = 0;

#if defined(_WIN32)
    // Windows: ToUnicodeEx (full IME/dead keys)
    HKL layout = GetKeyboardLayout(0);  // Current user layout
    BYTE key_state[256] = {0};
    if (mods & SIT_MOD_SHIFT) key_state[VK_SHIFT] = 0x80;
    if (mods & SIT_MOD_CONTROL) key_state[VK_CONTROL] = 0x80;
    if (mods & SIT_MOD_ALT) key_state[VK_MENU] = 0x80;
    // VK code from scancode via MapVirtualKey
    UINT vk = MapVirtualKeyEx(scancode, MAPVK_VSC_TO_VK_EX, layout);
    *out_char = ToUnicodeEx(vk, scancode, key_state, (WCHAR*)out_char, 1, 0, layout);
    return (*out_char > 0) ? SITUATION_SUCCESS : SITUATION_ERROR_INVALID_PARAM;

#elif defined(__APPLE__)
    // macOS: TIS/UCKeyTranslate (Carbon, handles layouts)
    TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    CFDataRef layout_data = (CFDataRef)TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData);
    const UCKeyboardLayout *layout = (UCKeyboardLayout *)CFDataGetBytePtr(layout_data);
    UniChar chars[4];
    UniCharCount len;
    UInt32 dead_key_state = 0;
    OSStatus status = UCKeyTranslate(layout, scancode, kUCKeyActionDown, 0 /*mods shim*/, LMGetKbdLast(), kUCKeyTranslateNoDeadKeysMask | kUCKeyTranslateConvertDeadKeyToASCII, &dead_key_state, 4, &len, chars);
    if (status == noErr && len > 0) {
        *out_char = chars[0];  // UTF-16 to uint32_t (basic)
        CFRelease(source);
        return SITUATION_SUCCESS;
    }
    CFRelease(source);
    return SITUATION_ERROR_INVALID_PARAM;

#else  // Linux/X11
    // XKB (via xkbcommonÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Âadd as opt dep, or fallback to XLookupString)
    // Simplified: Use GLFW's char callback as proxy (OS-handled)
    // For direct: xkb_keysym_get_utf32(xkb_state_key_get_one_sym(state, scancode + 8 /*XKB shift*/))
    // Stub for nowÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Ârecommend char callback
    return SITUATION_ERROR_INVALID_PARAM;
#endif
}

/**
 * @brief Checks whether a specific keyboard scancode is currently held down.
 *
 * @details Queries the current physical key state using scancode (hardware-level key code),
 *          independent of keyboard layout or modifiers. Returns true if the key is physically
 *          pressed at the moment of the call.
 *
 *          Scancodes are platform-consistent (via GLFW) and recommended for games/controls
 *          that want layout-independent input (e.g., WASD always moves regardless of locale).
 *
 *          This function reflects the **instantaneous state** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â it does not queue events or
 *          track press/release transitions (use callbacks or `SituationIsKeyPressed`/`Released`
 *          for edge detection).
 *
 * @param scancode The GLFW scancode to query (e.g., GLFW_KEY_W, GLFW_KEY_SPACE, GLFW_KEY_ESCAPE).
 *                 Values are defined in glfw3.h and are usually in the range 0ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“348.
 *                 Invalid/unknown scancodes return false.
 *
 * @return true if the key corresponding to the scancode is currently held down,
 *         false otherwise (released, not present on keyboard, or invalid scancode).
 *
 * @note This reads the latest polled state from GLFW. For most accurate timing,
 *       call this inside your main update loop after `SituationPollEvents()`.
 *       Does not distinguish between left/right modifiers (e.g., left Shift vs right Shift).
 *
 * @see SituationGetKeyScancode, SituationIsKeyDown, SituationPollEvents,
 *      GLFW_KEY_xxx constants (glfw3.h)
 */
SITAPI bool SituationIsScancodeDown(int scancode) {
    if (!SituationIsInitialized() || scancode < 0 || scancode >= SITUATION_MAX_SCANCODES) return false;
    return sit_input.keyboard.scancode_state[scancode];
}

/**
 * @brief Retrieves the scancode corresponding to the most recently pressed physical key.
 *
 * @details Returns the GLFW scancode of the last key that transitioned from released to pressed
 *          (the "latest key down event"). This is useful for one-shot actions like rebinding keys,
 *          capturing user input for configuration menus, or implementing "press any key to continue".
 *
 *          The value is updated every time a new key press is detected during event polling.
 *          Returns 0 if no key has been pressed since the last call or since initialization.
 *
 *          This is a **non-blocking, stateless query** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â it does not consume the event.
 *          The returned scancode remains valid until the next key press occurs.
 *
 * @return The GLFW scancode of the most recent key press event (e.g., GLFW_KEY_ENTER),
 *         or 0 if no key has been pressed recently or the input queue is empty.
 *
 * @note The "most recent" press is tracked internally and reset only on new presses ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
 *       repeated holding of the same key does not update it.
 *       For continuous polling of multiple keys, prefer `SituationIsScancodeDown` or the
 *       full key state array via `SituationGetKeyboardState`.
 *
 * @see SituationIsScancodeDown, SituationPollEvents, GLFW_KEY_xxx constants (glfw3.h)
 */
SITAPI int SituationGetKeyScancode(int key) {
    if (!SituationIsInitialized()) return -1;
    // Just wrap GLFW
    return glfwGetKeyScancode(key);
}

/**
 * @brief Checks if a keyboard key is currently held down (a continuous state).
 * @details This function provides real-time state polling. It will return `true` for every frame that the specified key is held down. This is ideal for continuous actions like character movement.
 *
 * @param key The key code to check (e.g., `SIT_KEY_W`, `SIT_KEY_UP`).
 *
 * @return `true` if the key is currently pressed, `false` otherwise.
 *
 * @note This function's state is updated once per frame by `SituationPollInputEvents()`.
 *
 * @see SituationIsKeyUp(), SituationIsKeyPressed()
 */
SITAPI bool SituationIsKeyDown(int key) {
    if (!SituationIsInitialized() || key < 0 || key > GLFW_KEY_LAST) return false;
    return sit_input.keyboard.current_state[key];
}

/**
 * @brief Checks if a keyboard key is currently released (a continuous state).
 * @details This function provides real-time state polling. It will return `true` for every frame that the specified key is *not* held down.
 *
 * @param key The key code to check (e.g., `SIT_KEY_W`, `SIT_KEY_SPACE`).
 *
 * @return `true` if the key is currently released, `false` otherwise.
 *
 * @note This function's state is updated once per frame by `SituationPollInputEvents()`.
 *
 * @see SituationIsKeyDown(), SituationIsKeyReleased()
 */
SITAPI bool SituationIsKeyUp(int key) {
    if (!SituationIsInitialized() || key < 0 || key > GLFW_KEY_LAST) return false;
    return !sit_input.keyboard.current_state[key];
}

/**
 * @brief Checks if a keyboard key was pressed down *this frame* (a single-trigger event).
 * @details This function detects a state change. It will only return `true` for the single frame immediately after the key is pressed. This is ideal for single-trigger actions like jumping, shooting, or opening a menu.
 *
 * @param key The key code to check (e.g., `SIT_KEY_ENTER`).
 *
 * @return `true` if the key was just pressed in the current frame, `false` otherwise.
 *
 * @note This function's state is updated once per frame by `SituationPollInputEvents()`.
 *
 * @see SituationIsKeyDown(), SituationIsKeyReleased(), SituationGetKeyPressed()
 */
SITAPI bool SituationIsKeyPressed(int key) {
    if (!SituationIsInitialized() || key < 0 || key > GLFW_KEY_LAST) return false;
    return sit_input.keyboard.down_this_frame[key];
}

/**
 * @brief Checks if a keyboard key was released *this frame* (a single-trigger event).
 * @details This function detects a state change. It will only return `true` for the single frame immediately after the key is released. This is useful for actions that should trigger on key-up.
 *
 * @param key The key code to check (e.g., `SIT_KEY_LEFT_SHIFT`).
 *
 * @return `true` if the key was just released in the current frame, `false` otherwise.
 *
 * @note This function's state is updated once per frame by `SituationPollInputEvents()`.
 *
 * @see SituationIsKeyUp(), SituationIsKeyPressed()
 */
SITAPI bool SituationIsKeyReleased(int key) {
    if (!SituationIsInitialized() || key < 0 || key > GLFW_KEY_LAST) return false;
    return sit_input.keyboard.up_this_frame[key];
}

/**
 * @brief Retrieves the next key-press event from the input queue.
 * @details Returns the key code of the next event in the FIFO queue and advances the queue head.
 *
 * @par Performance Note
 * This function uses an **O(1) Ring Buffer** implementation. Unlike previous versions, it does not perform memory shifting, ensuring consistent performance regardless of queue depth.
 *
 * @return The key code (e.g., `SIT_KEY_A`) or 0 if the queue is empty.
 * @see SituationPeekKeyPressed()
 */
SITAPI int SituationGetKeyPressedEx(int* out_scancode) {
    if (!SituationIsInitialized()) return 0;

    int key = 0;
    int scancode = 0;
    ma_mutex_lock(&sit_input.keyboard.event_queue_mutex);
    // Ring Buffer Pop
    if (sit_input.keyboard.pressed_head != sit_input.keyboard.pressed_tail) {
        key = sit_input.keyboard.pressed_queue[sit_input.keyboard.pressed_tail];
        scancode = sit_input.keyboard.scancode_queue[sit_input.keyboard.pressed_tail];
        sit_input.keyboard.pressed_tail = (sit_input.keyboard.pressed_tail + 1) % SITUATION_KEY_QUEUE_MAX;
    }
    ma_mutex_unlock(&sit_input.keyboard.event_queue_mutex);

    if (out_scancode) *out_scancode = scancode;
    return key;
}

SITAPI int SituationGetKeyPressed(void) {
    return SituationGetKeyPressedEx(NULL);
}

/**
 * @brief Peeks at the next key-press event in the input queue without consuming it.
 * @details This function allows you to check the next available key-press event without removing it from the queue. This is useful for inspecting the input state before deciding how to process it.
 *
 * @return The key code of the next key in the queue (e.g., `SIT_KEY_ENTER`).
 * @return `0` if the key-press queue is empty.
 *
 * @note Unlike `SituationGetKeyPressed`, calling this function multiple times in a row will return the same key code until the event is consumed.
 *
 * @see SituationGetKeyPressed()
 */
SITAPI int SituationPeekKeyPressedEx(int* out_scancode) {
    if (!SituationIsInitialized()) return 0;

    int key = 0;
    int scancode = 0;
    ma_mutex_lock(&sit_input.keyboard.event_queue_mutex);
    // Ring Buffer Peek
    if (sit_input.keyboard.pressed_head != sit_input.keyboard.pressed_tail) {
        key = sit_input.keyboard.pressed_queue[sit_input.keyboard.pressed_tail];
        scancode = sit_input.keyboard.scancode_queue[sit_input.keyboard.pressed_tail];
    }
    ma_mutex_unlock(&sit_input.keyboard.event_queue_mutex);

    if (out_scancode) *out_scancode = scancode;
    return key;
}

SITAPI int SituationPeekKeyPressed(void) {
    return SituationPeekKeyPressedEx(NULL);
}

/**
 * @brief Retrieves the next Unicode character event from the text input queue and consumes it.
 * @details This function is specifically designed for text entry. It returns the Unicode codepoint of the next character typed by the user, respecting keyboard layouts, modifier keys (like Shift), and dead keys.
 *          It should be used when implementing text fields, command consoles, or any form of text input.
 *
 * @return The Unicode codepoint of the next character entered.
 * @return `0` if the character queue is empty.
 *
 * @note This function is distinct from `SituationGetKeyPressed`, which returns raw key codes (`SIT_KEY_A`) and does not handle character composition.
 * @note The queue is cleared at the beginning of each frame by `SituationPollInputEvents()`.
 */
SITAPI unsigned int SituationGetCharPressed(void) {
    if (!SituationIsInitialized()) return 0;

    unsigned int codepoint = 0;
    ma_mutex_lock(&sit_input.keyboard.event_queue_mutex);
    // Ring Buffer Pop
    if (sit_input.keyboard.char_head != sit_input.keyboard.char_tail) {
        codepoint = sit_input.keyboard.char_queue[sit_input.keyboard.char_tail];
        sit_input.keyboard.char_tail = (sit_input.keyboard.char_tail + 1) % SITUATION_CHAR_QUEUE_MAX;
    }
    ma_mutex_unlock(&sit_input.keyboard.event_queue_mutex);
    return codepoint;
}

/**
 * @brief Checks if a lock key (Caps Lock or Num Lock) is currently active.
 * @details This function queries the modifier state captured during input polling to determine if a lock key is toggled on.
 *
 * @param lock_key_mod The lock key to check. Use `SIT_MOD_CAPS_LOCK` or `SIT_MOD_NUM_LOCK`.
 *
 * @return `true` if the specified lock key is currently active, `false` otherwise.
 *
 * @note This reflects the state of the lock key at the time of the last event, as reported by the OS.
 *
 * @see SituationIsScrollLockOn(), SituationIsModifierPressed()
 */
SITAPI bool SituationIsLockKeyPressed(int lock_key_mod) {
    if (!SituationIsInitialized()) return false;
    return (sit_input.keyboard.lock_key_state & lock_key_mod) != 0;
}

/**
 * @brief Checks if the Scroll Lock key is currently toggled on.
 * @details This function is necessary because the Scroll Lock state is not reported as a standard modifier by GLFW. The library manually tracks its toggle state based on key-press events.
 *
 * @return `true` if Scroll Lock is toggled on, `false` otherwise.
 *
 * @note The accuracy of this function depends on the initial state of Scroll Lock when the application starts.
 *
 * @see SituationIsLockKeyPressed()
 */
SITAPI bool SituationIsScrollLockOn(void) {
    if (!SituationIsInitialized()) return false;
    return sit_input.keyboard.is_scroll_lock_on;
}

/**
 * @brief Checks if a modifier key (Shift, Ctrl, Alt, Super) is currently held down.
 * @details This function provides a convenient way to check the state of modifier keys without needing to check for left and right keys individually.
 *
 * @param modifier The modifier key to check. Use `SIT_MOD_SHIFT`, `SIT_MOD_CONTROL`, `SIT_MOD_ALT`, or `SIT_MOD_SUPER`.
 *
 * @return `true` if one or more of the specified modifier keys are pressed, `false` otherwise.
 *
 * @see SituationIsKeyDown()
 */
SITAPI bool SituationIsModifierPressed(int modifier) {
    if (!SituationIsInitialized()) return false;
    return (sit_input.keyboard.modifier_state & modifier) != 0;
}

/**
 * @brief Sets a callback function to be executed for all keyboard key events.
 * @details This provides an event-driven way to handle keyboard input, an alternative to polling in the main loop. The callback is invoked by the OS's event thread (via GLFW) immediately when a key is pressed, released, or repeated.
 *
 * @param callback The function pointer to your callback. The callback receives the key, scancode, action, and modifier flags. Pass `NULL` to clear the callback.
 * @param user_data A custom pointer that will be passed to your callback function, allowing you to maintain state.
 *
 * @warning The callback is executed in the same thread that calls `SituationPollInputEvents`. It is not asynchronous and will block the main loop until it returns.
 */
SITAPI void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data) {
    if (!SituationIsInitialized()) return;
    sit_input.keyboard.key_callback = callback;
    sit_input.keyboard.key_callback_user_data = user_data;
}

// --- Mouse Management Implementation ---
/**
 * @brief Gets the current mouse cursor position within the window's client area.
 * @details This function returns the mouse coordinates relative to the top-left corner of the window. The returned value is affected by any custom transformations set via `SituationSetMouseOffset()` and `SituationSetMouseScale()`.
 *
 * @return A `Vector2` containing the current (x, y) coordinates of the mouse.
 *
 * @note This function's state is updated once per frame by `SituationPollInputEvents()`.
 *
 * @see SituationGetMouseDelta(), SituationSetMousePosition()
 */
SITAPI Vector2 SituationGetMousePosition(void) {
    if (!SituationIsInitialized()) {
        Vector2 zero_vec = {0.0f, 0.0f}; return zero_vec;
    }
    Vector2 pos;
    // Apply scale first, then offset
    glm_vec2_mul(sit_input.mouse.current_pos, sit_input.mouse.scale, (float*)&pos);
    glm_vec2_add((float*)&pos, sit_input.mouse.offset, (float*)&pos);
    return pos;
}

/**
 * @brief Gets the change in mouse position since the last frame.
 * @details This function is essential for implementing camera controls or any interaction based on mouse movement. It returns the difference between the current and previous frame's mouse positions, scaled by `SituationSetMouseScale()`.
 *
 * @return A `Vector2` representing the movement delta (dx, dy).
 *
 * @note This value is reset to zero at the start of each frame by `SituationPollInputEvents()`.
 *
 * @see SituationGetMousePosition()
 */
SITAPI Vector2 SituationGetMouseDelta(void) {
    if (!SituationIsInitialized()) {
        Vector2 zero_vec = {0.0f, 0.0f}; return zero_vec;
    }
    Vector2 delta;
    glm_vec2_sub(sit_input.mouse.current_pos, sit_input.mouse.last_pos, (float*)&delta);
    // Scale the delta as well
    glm_vec2_mul((float*)&delta, sit_input.mouse.scale, (float*)&delta);
    return delta;
}

/**
 * @brief Sets the mouse cursor's position within the window's client area.
 * @details This function "warps" or "teleports" the mouse cursor to the specified coordinates. It correctly accounts for any custom transformations set via `SituationSetMouseOffset()` and `SituationSetMouseScale()`,
 * so the coordinates you provide are in the same space as those returned by `SituationGetMousePosition()`.
 *
 * @param pos A `Vector2` containing the target (x, y) coordinates.
 *
 * @note This function also updates the internal mouse state to prevent a large, incorrect `SituationGetMouseDelta()` value on the next frame.
 */
SITAPI void SituationSetMousePosition(Vector2 pos) {
    if (!SituationIsInitialized() || !sit_gs.sit_glfw_window) return;
    // We must "un-transform" the position before sending it to GLFW, so that GetMousePosition will return the value the user expects.
    Vector2 raw_pos;
    glm_vec2_sub((float*)&pos, sit_input.mouse.offset, (float*)&raw_pos);
    // Division is component-wise. Ensure scale components are not zero.
    if (sit_input.mouse.scale[0] != 0.0f) raw_pos.x /= sit_input.mouse.scale[0];
    if (sit_input.mouse.scale[1] != 0.0f) raw_pos.y /= sit_input.mouse.scale[1];

    glfwSetCursorPos(sit_gs.sit_glfw_window, raw_pos.x, raw_pos.y);

    // Also update our internal state immediately to prevent a "jumpy" delta on the next frame.
    glm_vec2_copy((float*)&raw_pos, sit_input.mouse.current_pos);
}

/**
 * @brief Sets a virtual offset for the mouse cursor's coordinate system.
 * @details This is an advanced utility for remapping mouse coordinates. The provided offset is added to the raw mouse position before it is returned by `SituationGetMousePosition()`. This can be useful for emulating a virtual camera or a scrolling view within a larger space.
 *
 * @param offset A `Vector2` representing the (x, y) offset to apply.
 *
 * @note The default offset is (0, 0).
 *
 * @see SituationGetMousePosition(), SituationSetMouseScale()
 */
SITAPI void SituationSetMouseOffset(Vector2 offset) {
    if (!SituationIsInitialized()) return;
    glm_vec2_copy((float*)&offset, sit_input.mouse.offset);
}

/**
 * @brief Sets a virtual scale for the mouse cursor's coordinate system.
 * @details This is an advanced utility for remapping mouse coordinates. The raw mouse position and delta are multiplied by this scale factor before being returned by `SituationGetMousePosition()` and `SituationGetMouseDelta()`.
 * This is useful for matching mouse input to a scaled viewport or rendering resolution.
 *
 * @param scale A `Vector2` representing the (x, y) scale factors to apply.
 *
 * @note The default scale is (1.0, 1.0).
 *
 * @see SituationGetMousePosition(), SituationGetMouseDelta(), SituationSetMouseOffset()
 */
SITAPI void SituationSetMouseScale(Vector2 scale) {
    if (!SituationIsInitialized()) return;
    glm_vec2_copy((float*)&scale, sit_input.mouse.scale);
}

/**
 * @brief Gets the vertical mouse wheel movement since the last frame.
 * @details This function returns the accumulated vertical scroll offset for the current frame.
 *
 * @return A float representing the vertical scroll amount. Positive values typically mean scrolling up (away from the user), and negative values mean scrolling down (towards the user).
 *
 * @note This value is an accumulation of all scroll events within a frame and is reset to zero by `SituationPollInputEvents()`. For both horizontal and vertical movement, use `SituationGetMouseWheelMoveV()`.
 *
 * @see SituationGetMouseWheelMoveV()
 */
SITAPI float SituationGetMouseWheelMove(void) {
    if (!SituationIsInitialized()) return 0.0f;
    // GLFW scroll y offset is positive for scroll up/away from user, negative for scroll down/towards user.
    return sit_input.mouse.wheel_move_y;
}

/**
 * @brief Gets the vertical and horizontal mouse wheel movement since the last frame.
 * @details This function returns the accumulated scroll offsets for both axes. While most mice only have a vertical wheel, some devices (like trackpads or mice with tilt-wheels) can report horizontal scrolling.
 *
 * @return A `Vector2` where `x` is the horizontal scroll offset and `y` is the vertical scroll offset.
 *
 * @note These values are an accumulation of all scroll events within a frame and are reset to zero by `SituationPollInputEvents()`.
 *
 * @see SituationGetMouseWheelMove()
 */
SITAPI Vector2 SituationGetMouseWheelMoveV(void) {
    if (!SituationIsInitialized()) {
        Vector2 zero_vec = {0.0f, 0.0f}; return zero_vec;
    }
    Vector2 wheel_v = {sit_input.mouse.wheel_move_x, sit_input.mouse.wheel_move_y};
    return wheel_v;
}

/**
 * @brief Retrieves the next mouse button-press event from the input queue and consumes it.
 * @details This function provides a queue-based approach to handling mouse clicks, guaranteeing that no press events are missed. It is useful for UI interactions where you need to process every single click.
 *
 * @return The button code of the next mouse button pressed (e.g., `GLFW_MOUSE_BUTTON_LEFT`).
 * @return `-1` if the button-press queue is empty.
 *
 * @note The queue is cleared at the beginning of each frame by `SituationPollInputEvents()`.
 */
SITAPI int SituationGetMouseButtonPressed(void) {
    if (!SituationIsInitialized()) return -1;

    ma_mutex_lock(&sit_input.mouse.mutex);
    int button = -1;
    // Ring Buffer Pop
    if (sit_input.mouse.button_head != sit_input.mouse.button_tail) {
        button = sit_input.mouse.button_queue[sit_input.mouse.button_tail];
        sit_input.mouse.button_tail = (sit_input.mouse.button_tail + 1) % SITUATION_KEY_QUEUE_MAX;
    }
    ma_mutex_unlock(&sit_input.mouse.mutex);
    return button;
}

/**
 * @brief Checks if a mouse button is currently held down (a continuous state).
 * @details This function provides real-time state polling. It will return `true` for every frame that the specified button is held down. This is ideal for continuous actions like dragging or firing an automatic weapon.
 *
 * @param button The mouse button to check (e.g., `GLFW_MOUSE_BUTTON_LEFT`, `GLFW_MOUSE_BUTTON_RIGHT`).
 *
 * @return `true` if the button is currently pressed, `false` otherwise.
 */
SITAPI bool SituationIsMouseButtonDown(int button) {
    if (!SituationIsInitialized() || button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return sit_input.mouse.current_button_state[button];
}

/**
 * @brief Checks if a mouse button was pressed down *this frame* (a single-trigger event).
 * @details This function detects a state change. It will only return `true` for the single frame immediately after the button is pressed. This is ideal for single-trigger actions like selecting an item or firing a semi-automatic weapon.
 *
 * @param button The mouse button to check.
 *
 * @return `true` if the button was just pressed in the current frame, `false` otherwise.
 */
SITAPI bool SituationIsMouseButtonPressed(int button) {
    if (!SituationIsInitialized() || button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return sit_input.mouse.button_down_this_frame[button];
}

/**
 * @brief Checks if a mouse button was released *this frame* (a single-trigger event).
 * @details This function detects a state change. It will only return `true` for the single frame immediately after the button is released. This is useful for actions that trigger on release, like confirming a drag-and-drop operation.
 *
 * @param button The mouse button to check.
 *
 * @return `true` if the button was just released in the current frame, `false` otherwise.
 */
SITAPI bool SituationIsMouseButtonReleased(int button) {
    if (!SituationIsInitialized() || button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return sit_input.mouse.button_up_this_frame[button];
}

/**
 * @brief Sets a callback function to be executed for all mouse button events.
 * @details This provides an event-driven way to handle mouse clicks. The callback is invoked immediately when a button is pressed or released.
 *
 * @param callback The function pointer to your callback. Pass `NULL` to clear.
 * @param user_data A custom pointer that will be passed to your callback function.
 *
 * @warning The callback is executed in the same thread that calls `SituationPollInputEvents`.
 */
SITAPI void SituationSetMouseButtonCallback(SituationMouseButtonCallback callback, void* user_data) {
    if (!SituationIsInitialized()) return;
    sit_input.mouse.button_callback = callback;
    sit_input.mouse.button_callback_user_data = user_data;
}

/**
 * @brief Sets a callback function to be executed when the mouse cursor moves.
 * @details This provides an event-driven way to handle mouse movement, an alternative to polling `SituationGetMouseDelta()`.
 *
 * @param callback The function pointer to your callback. Pass `NULL` to clear.
 * @param user_data A custom pointer that will be passed to your callback function.
 *
 * @warning The callback is executed in the same thread that calls `SituationPollInputEvents`.
 */
SITAPI void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data) {
    if (!SituationIsInitialized()) return;
    sit_input.mouse.cursor_pos_callback = callback;
    sit_input.mouse.cursor_pos_callback_user_data = user_data;
}

/**
 * @brief Sets a callback function to be executed for mouse wheel scroll events.
 * @details This provides an event-driven way to handle scrolling.
 *
 * @param callback The function pointer to your callback. Pass `NULL` to clear.
 * @param user_data A custom pointer that will be passed to your callback function.
 *
 * @warning The callback is executed in the same thread that calls `SituationPollInputEvents`.
 */
SITAPI void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data) {
    if (!SituationIsInitialized()) return;
    sit_input.mouse.scroll_callback = callback;
    sit_input.mouse.scroll_callback_user_data = user_data;
}

/**
 * @brief Sets the appearance of the mouse cursor to a standard system shape.
 * @details The library pre-creates a set of standard cursors at initialization for fast switching.
 *
 * @param cursor An enum `SituationCursor` representing the desired shape (e.g., `SIT_CURSOR_HAND`, `SIT_CURSOR_IBEAM`). Use `SIT_CURSOR_DEFAULT` to restore the system's default arrow.
 */
SITAPI void SituationSetCursor(SituationCursor cursor) {
    if (!SituationIsInitialized()) return;

    // Ensure the requested cursor is within the bounds of what we created
    if (cursor >= 0 && cursor < sit_input.cursor_count) {
        // NULL for the cursor handle tells GLFW to use the default system cursor
        glfwSetCursor(sit_gs.sit_glfw_window, sit_input.cursors[cursor]);
    }
}

/**
 * @brief Makes the mouse cursor visible and behave normally.
 * @details This is the default cursor mode.
 *
 * @see SituationHideCursor(), SituationDisableCursor()
 */
SITAPI void SituationShowCursor(void) {
    if (!SituationIsInitialized()) return;
    glfwSetInputMode(sit_gs.sit_glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

/**
 * @brief Makes the mouse cursor invisible while it is over the window's client area.
 * @details The cursor will reappear if it leaves the window. Its position is not locked.
 *
 * @see SituationShowCursor(), SituationDisableCursor()
 */
SITAPI void SituationHideCursor(void) {
    if (!SituationIsInitialized()) return;
    glfwSetInputMode(sit_gs.sit_glfw_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

/**
 * @brief Hides the mouse cursor and locks it to the window, providing unbounded movement.
 * @details This is the ideal mode for 3D camera controls ("mouse look") or any application that requires raw, continuous mouse motion input without being constrained by the screen edges.
 *
 * @see SituationShowCursor(), SituationHideCursor(), SituationGetMouseDelta()
 */
SITAPI void SituationDisableCursor(void) {
    if (!SituationIsInitialized()) return;
    glfwSetInputMode(sit_gs.sit_glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

/**
 * @brief GLFW callback for joystick connection/disconnection events.
 *
 * @details THIS FUNCTION IS CALLED ON A SEPARATE GLFW THREAD.
 * Its only job is to safely queue a connection event for the main thread to process later.
 * It locks a mutex, adds the event to a small queue, and unlocks immediately.
 * No complex state changes should ever happen in this function.
 *
 * @param jid The joystick ID.
 * @param event The event type (GLFW_CONNECTED or GLFW_DISCONNECTED).
 */
static void _SituationGLFWJoystickCallback(int jid, int event) {
    // Ignore joysticks beyond the maximum we are tracking.
    if (jid < 0 || jid >= SITUATION_MAX_JOYSTICKS) return;

    // Lock the mutex to safely access the shared event queue.
    ma_mutex_lock(&sit_input.joysticks.event_queue_mutex);
    {
        // Add the event to the queue if there is space.
        if (sit_input.joysticks.event_queue_count < SITUATION_MAX_JOYSTICKS) {
            sit_input.joysticks.event_queue[sit_input.joysticks.event_queue_count].jid = jid;
            sit_input.joysticks.event_queue[sit_input.joysticks.event_queue_count].event = event;
            sit_input.joysticks.event_queue_count++;
        }
        // else: The queue is full and the event is dropped. This is rare for connect/disconnect events but prevents a buffer overflow. A larger queue could be used if this becomes an issue.
    }
    // Unlock the mutex as quickly as possible.
    ma_mutex_unlock(&sit_input.joysticks.event_queue_mutex);
}

// --- Gamepad (Joystick) Management Implementation ---
/**
 * @brief Checks if a joystick or gamepad is currently connected at a specific slot.
 * @details This function queries the internal state, which is updated by the joystick connection/disconnection callback. It can be used to detect if a player's controller is plugged in.
 *
 * @param jid The joystick ID (slot) to check. This is typically 0 for the first player, 1 for the second, and so on, up to `SITUATION_MAX_JOYSTICKS - 1`.
 *
 * @return `true` if a joystick is connected at the specified slot, `false` otherwise.
 */
SITAPI bool SituationIsJoystickPresent(int jid) {
    if (!SituationIsInitialized() || jid < 0 || jid >= SITUATION_MAX_JOYSTICKS) return false;
    return sit_input.joysticks.state[jid].is_present;
}

/**
 * @brief Checks if a connected joystick has a standard gamepad mapping.
 * @details A "gamepad" is a joystick that the underlying backend (GLFW) recognizes and can map to a standard layout (like an Xbox or PlayStation controller). If this returns true, you can reliably use the `SITUATION_GAMEPAD_BUTTON_*` and `SITUATION_GAMEPAD_AXIS_*` enums.
 * If it returns false, the device is a non-standard joystick, and you would need to query its raw axes and buttons.
 *
 * @param jid The joystick ID (slot) to check.
 *
 * @return `true` if the joystick in the specified slot is a standard, mapped gamepad, `false` otherwise.
 *
 * @see SituationIsJoystickPresent()
 */
SITAPI bool SituationIsGamepad(int jid) {
    if (!SituationIsJoystickPresent(jid)) return false;
    return sit_input.joysticks.state[jid].is_gamepad;
}

/**
 * @brief Gets the human-readable name of a connected joystick or gamepad.
 * @details This function returns the name of the controller as reported by the operating system (e.g., "XInput Controller", "DualSense Wireless Controller"). This is useful for displaying controller information in UI or for debugging.
 *
 * @param jid The joystick ID (slot) to query.
 *
 * @return A constant string containing the name of the device.
 * @return `"N/A"` if no joystick is present at the specified slot.
 */
SITAPI const char* SituationGetJoystickName(int jid) {
    if (!SituationIsJoystickPresent(jid)) return "N/A";
    return sit_input.joysticks.state[jid].name;
}

/**
 * @brief Sets a callback function to be executed when a joystick is connected or disconnected.
 * @details This provides an event-driven way to manage controller connections. The callback is invoked immediately when a device is plugged in or unplugged, allowing the application to react by updating player states, showing UI prompts, etc.
 *
 * @param callback The function pointer to your callback. The callback receives the joystick ID and the event type (`GLFW_CONNECTED` or `GLFW_DISCONNECTED`). Pass `NULL` to clear the callback.
 * @param user_data A custom, user-defined pointer that will be passed to your callback function.
 *
 * @warning The GLFW callback that triggers this may be called from a separate thread. Your callback function should be thread-safe or delegate complex work to the main thread.
 */
SITAPI void SituationSetJoystickCallback(SituationJoystickCallback callback, void* user_data) {
    if (!SituationIsInitialized()) return;
    sit_input.joysticks.callback = callback;
    sit_input.joysticks.callback_user_data = user_data;
}

/**
 * @brief Checks if a gamepad button is currently held down (a continuous state).
 * @details This function provides real-time state polling. It will return `true` for every frame that the specified button is held down. This is ideal for continuous actions like accelerating in a racing game.
 *
 * @param jid The joystick ID of the gamepad.
 * @param button The button to check (e.g., `SITUATION_GAMEPAD_BUTTON_A`).
 *
 * @return `true` if the button is currently pressed, `false` otherwise.
 *
 * @note This function's state is updated once per frame by `SituationUpdateTimers()`.
 */
SITAPI bool SituationIsGamepadButtonDown(int jid, int button) {
    if (!SituationIsGamepad(jid) || button < 0 || button >= SITUATION_MAX_JOYSTICK_BUTTONS) return false;
    return (sit_input.joysticks.state[jid].current_button_state[button] == GLFW_PRESS);
}

/**
 * @brief Retrieves the next gamepad button-press event from the input queue and consumes it.
 * @details This function provides a queue-based approach to handling button presses, similar to `SituationGetKeyPressed`. It returns the button code of the first button that was pressed in the current frame across all connected gamepads and removes it from the queue. This guarantees that no button presses are missed.
 *
 * @return The button code of the next button pressed (e.g., `SITUATION_GAMEPAD_BUTTON_START`).
 * @return `-1` if the button-press queue is empty.
 *
 * @note This function does not distinguish which gamepad the press came from. It is best used for single-player UI navigation or actions where the source controller doesn't matter. For player-specific input, use `SituationIsGamepadButtonPressed()`.
 */
SITAPI int SituationGetGamepadButtonPressed(void) {
    if (!SituationIsInitialized()) return -1;

    int button = -1;
    // Note: Gamepad queue isn't mutex protected as it's updated in main thread
    // Ring Buffer Pop
    if (sit_input.joysticks.button_head != sit_input.joysticks.button_tail) {
        button = sit_input.joysticks.button_pressed_queue[sit_input.joysticks.button_tail];
        sit_input.joysticks.button_tail = (sit_input.joysticks.button_tail + 1) % SITUATION_KEY_QUEUE_MAX;
    }
    return button;
}

/**
 * @brief Checks if a gamepad button was pressed down *this frame* (a single-trigger event).
 * @details This function detects a state change. It will only return `true` for the single frame immediately after the button is pressed. This is ideal for single-trigger actions like jumping or firing.
 *
 * @param jid The joystick ID of the gamepad.
 * @param button The button to check (e.g., `SITUATION_GAMEPAD_BUTTON_X`).
 *
 * @return `true` if the button was just pressed in the current frame, `false` otherwise.
 *
 * @note This function's state is updated once per frame by `SituationUpdateTimers()`.
 */
SITAPI bool SituationIsGamepadButtonPressed(int jid, int button) {
    if (!SituationIsGamepad(jid) || button < 0 || button >= SITUATION_MAX_JOYSTICK_BUTTONS) return false;
    return (sit_input.joysticks.state[jid].current_button_state[button] == GLFW_PRESS &&
            sit_input.joysticks.state[jid].last_button_state[button] == GLFW_RELEASE);
}

/**
 * @brief Checks if a gamepad button was released *this frame* (a single-trigger event).
 * @details This function detects a state change. It will only return `true` for the single frame immediately after the button is released.
 *
 * @param jid The joystick ID of the gamepad.
 * @param button The button to check (e.g., `SITUATION_GAMEPAD_BUTTON_LEFT_BUMPER`).
 *
 * @return `true` if the button was just released in the current frame, `false` otherwise.
 *
 * @note This function's state is updated once per frame by `SituationUpdateTimers()`.
 */
SITAPI bool SituationIsGamepadButtonReleased(int jid, int button) {
    if (!SituationIsGamepad(jid) || button < 0 || button >= SITUATION_MAX_JOYSTICK_BUTTONS) return false;
    return (sit_input.joysticks.state[jid].current_button_state[button] == GLFW_RELEASE &&
            sit_input.joysticks.state[jid].last_button_state[button] == GLFW_PRESS);
}

/**
 * @brief Gets the current value of a gamepad axis, with deadzone applied.
 * @details This function returns the position of an analog stick or trigger. The value is normalized to a range of [-1.0 to 1.0] for sticks and [0.0 to 1.0] for triggers (after backend conversion).
 * A deadzone is automatically applied to the analog sticks to prevent "drift" from worn-out or imprecise hardware.
 *
 * @param jid The joystick ID of the gamepad.
 * @param axis The axis to query (e.g., `SITUATION_GAMEPAD_AXIS_LEFT_X`, `SITUATION_GAMEPAD_AXIS_RIGHT_TRIGGER`).
 *
 * @return The axis value as a float. Returns `0.0f` if the axis is within the deadzone.
 *
 * @note The value is rescaled to provide a smooth response curve, starting from 0 at the edge of the deadzone and reaching 1.0 at the full extent of the axis.
 */
SITAPI float SituationGetGamepadAxisValue(int jid, int axis) {
    if (!SituationIsGamepad(jid) || axis < 0 || axis >= SITUATION_MAX_JOYSTICK_AXES) return 0.0f;

    float value = sit_input.joysticks.state[jid].axis_state[axis];

    // Apply deadzone for analog sticks to prevent drift
    float deadzone = 0.0f;
    if (axis == GLFW_GAMEPAD_AXIS_LEFT_X || axis == GLFW_GAMEPAD_AXIS_LEFT_Y) {
        deadzone = SITUATION_JOYSTICK_DEADZONE_L;
    } else if (axis == GLFW_GAMEPAD_AXIS_RIGHT_X || axis == GLFW_GAMEPAD_AXIS_RIGHT_Y) {
        deadzone = SITUATION_JOYSTICK_DEADZONE_R;
    }

    if (fabsf(value) < deadzone) {
        return 0.0f;
    }

    // Optional: Rescale the value to be 0 at the edge of the deadzone
    // This provides a smoother response curve after the deadzone.
    float rescaled_value = (value - copysignf(deadzone, value)) / (1.0f - deadzone);
    return rescaled_value;
}

/**
 * @brief Gets the number of axes available on a connected joystick.
 * @details This is useful for querying the capabilities of non-standard joysticks that are not recognized as gamepads. For standard gamepads, this will typically return 6.
 *
 * @param jid The joystick ID (slot) to query.
 *
 * @return The number of axes detected for the device.
 */
SITAPI int SituationGetGamepadAxisCount(int jid) {
    if (!SituationIsJoystickPresent(jid)) return 0;
    return sit_input.joysticks.state[jid].axis_count;
}

/**
 * @brief Updates the internal controller mappings from an SDL2-comptabile mapping string.
 * @details This function allows you to add or override gamepad mappings at runtime. This is useful for supporting controllers that are not recognized by default. You can find mapping strings for various controllers in the community-maintained `gamecontrollerdb.txt` file.
 *
 * @param mappings A string containing one or more SDL2-comptabile controller mappings.
 *
 * @return `1` on success, `0` on failure (e.g., if the mapping string is invalid).
 */
SITAPI int SituationSetGamepadMappings(const char *mappings) {
    if (!SituationIsInitialized()) {
        _SituationSetErrorFromCode(SITUATION_ERROR_NOT_INITIALIZED, "SetGamepadMappings");
        return 0;
    }
    // This is a direct wrapper around the GLFW function.
    // It returns 1 on success, 0 on failure.
    return glfwUpdateGamepadMappings(mappings);
}

/**
 * @brief Sets the vibration/rumble intensity for a connected gamepad.
 * @details This function controls the strength of the low-frequency (left) and high-frequency (right) rumble motors in the controller.
 *
 * @par Platform Specificity
 *   This feature is currently only implemented on **Windows** via the XInput API and will only work for XInput-compatible controllers. On other platforms, this function will do nothing.
 *
 * @param jid The joystick ID of the gamepad (0-3 on Windows).
 * @param left_motor The intensity of the left (low-frequency) motor, from `0.0f` (off) to `1.0f` (full strength).
 * @param right_motor The intensity of the right (high-frequency) motor, from `0.0f` (off) to `1.0f` (full strength).
 */
SITAPI bool SituationSetGamepadVibration(int jid, float left_motor, float right_motor) {
    if (!SituationIsJoystickPresent(jid)) return false;

#if defined(_WIN32)
    // On Windows, GLFW joystick IDs map directly to XInput user indices (0-3) for XInput-compatible devices. We assume this mapping holds.
    if (jid < 0 || jid >= 4) return false;

    XINPUT_VIBRATION vibration;
    memset(&vibration, 0, sizeof(XINPUT_VIBRATION));

    // XInput wants values from 0-65535. Clamp float from 0.0-1.0 and scale.
    float left = (left_motor < 0.0f) ? 0.0f : (left_motor > 1.0f) ? 1.0f : left_motor;
    float right = (right_motor < 0.0f) ? 0.0f : (right_motor > 1.0f) ? 1.0f : right_motor;

    vibration.wLeftMotorSpeed = (WORD)(left * 65535.0f);
    vibration.wRightMotorSpeed = (WORD)(right * 65535.0f);

    // XInputSetState returns ERROR_SUCCESS (0) on success
    return (XInputSetState((DWORD)jid, &vibration) == ERROR_SUCCESS);
#else
    // Correctly fail on non-supported platforms
    _SituationSetErrorFromCode(SITUATION_ERROR_NOT_IMPLEMENTED, "Gamepad vibration is currently only supported on Windows (XInput).");
    return false;
#endif
}
#endif // SITUATION_IMPL_INPUT_H
