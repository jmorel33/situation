## Input Module

**Overview:** The Input module provides a flexible interface for handling user input from keyboards, mice, and gamepads. It supports both state polling (e.g., `SituationIsKeyDown()`) for continuous actions and event-driven callbacks (e.g., `SituationSetKeyCallback()`) for discrete events.

### Key Codes
The library defines a comprehensive set of key codes for use with keyboard input functions.

| Key Code | Value | Description |
|---|---|---|
| `SIT_KEY_SPACE` | 32 | Spacebar |
| `SIT_KEY_APOSTROPHE` | 39 | ' |
| `SIT_KEY_COMMA` | 44 | , |
| `SIT_KEY_MINUS` | 45 | - |
| `SIT_KEY_PERIOD` | 46 | . |
| `SIT_KEY_SLASH` | 47 | / |
| `SIT_KEY_0` | 48 | 0 |
| `SIT_KEY_1` | 49 | 1 |
| `SIT_KEY_2` | 50 | 2 |
| `SIT_KEY_3` | 51 | 3 |
| `SIT_KEY_4` | 52 | 4 |
| `SIT_KEY_5` | 53 | 5 |
| `SIT_KEY_6` | 54 | 6 |
| `SIT_KEY_7` | 55 | 7 |
| `SIT_KEY_8` | 56 | 8 |
| `SIT_KEY_9` | 57 | 9 |
| `SIT_KEY_SEMICOLON` | 59 | ; |
| `SIT_KEY_EQUAL` | 61 | = |
| `SIT_KEY_A` | 65 | A |
| `SIT_KEY_B` | 66 | B |
| `SIT_KEY_C` | 67 | C |
| `SIT_KEY_D` | 68 | D |
| `SIT_KEY_E` | 69 | E |
| `SIT_KEY_F` | 70 | F |
| `SIT_KEY_G` | 71 | G |
| `SIT_KEY_H` | 72 | H |
| `SIT_KEY_I` | 73 | I |
| `SIT_KEY_J` | 74 | J |
| `SIT_KEY_K` | 75 | K |
| `SIT_KEY_L` | 76 | L |
| `SIT_KEY_M` | 77 | M |
| `SIT_KEY_N` | 78 | N |
| `SIT_KEY_O` | 79 | O |
| `SIT_KEY_P` | 80 | P |
| `SIT_KEY_Q` | 81 | Q |
| `SIT_KEY_R` | 82 | R |
| `SIT_KEY_S` | 83 | S |
| `SIT_KEY_T` | 84 | T |
| `SIT_KEY_U` | 85 | U |
| `SIT_KEY_V` | 86 | V |
| `SIT_KEY_W` | 87 | W |
| `SIT_KEY_X` | 88 | X |
| `SIT_KEY_Y` | 89 | Y |
| `SIT_KEY_Z` | 90 | Z |
| `SIT_KEY_LEFT_BRACKET` | 91 | [ |
| `SIT_KEY_BACKSLASH` | 92 | \ |
| `SIT_KEY_RIGHT_BRACKET` | 93 | ] |
| `SIT_KEY_GRAVE_ACCENT` | 96 | ` |
| `SIT_KEY_WORLD_1` | 161 | non-US #1 |
| `SIT_KEY_WORLD_2` | 162 | non-US #2 |
| `SIT_KEY_ESCAPE` | 256 | Escape |
| `SIT_KEY_ENTER` | 257 | Enter |
| `SIT_KEY_TAB` | 258 | Tab |
| `SIT_KEY_BACKSPACE` | 259 | Backspace |
| `SIT_KEY_INSERT` | 260 | Insert |
| `SIT_KEY_DELETE` | 261 | Delete |
| `SIT_KEY_RIGHT` | 262 | Right Arrow |
| `SIT_KEY_LEFT` | 263 | Left Arrow |
| `SIT_KEY_DOWN` | 264 | Down Arrow |
| `SIT_KEY_UP` | 265 | Up Arrow |
| `SIT_KEY_PAGE_UP` | 266 | Page Up |
| `SIT_KEY_PAGE_DOWN` | 267 | Page Down |
| `SIT_KEY_HOME` | 268 | Home |
| `SIT_KEY_END` | 269 | End |
| `SIT_KEY_CAPS_LOCK` | 280 | Caps Lock |
| `SIT_KEY_SCROLL_LOCK` | 281 | Scroll Lock |
| `SIT_KEY_NUM_LOCK` | 282 | Num Lock |
| `SIT_KEY_PRINT_SCREEN` | 283 | Print Screen |
| `SIT_KEY_PAUSE` | 284 | Pause |
| `SIT_KEY_F1` | 290 | F1 |
| `SIT_KEY_F2` | 291 | F2 |
| `SIT_KEY_F3` | 292 | F3 |
| `SIT_KEY_F4` | 293 | F4 |
| `SIT_KEY_F5` | 294 | F5 |
| `SIT_KEY_F6` | 295 | F6 |
| `SIT_KEY_F7` | 296 | F7 |
| `SIT_KEY_F8` | 297 | F8 |
| `SIT_KEY_F9` | 298 | F9 |
| `SIT_KEY_F10` | 299 | F10 |
| `SIT_KEY_F11` | 300 | F11 |
| `SIT_KEY_F12` | 301 | F12 |
| `SIT_KEY_F13` | 302 | F13 |
| `SIT_KEY_F14` | 303 | F14 |
| `SIT_KEY_F15` | 304 | F15 |
| `SIT_KEY_F16` | 305 | F16 |
| `SIT_KEY_F17` | 306 | F17 |
| `SIT_KEY_F18` | 307 | F18 |
| `SIT_KEY_F19` | 308 | F19 |
| `SIT_KEY_F20` | 309 | F20 |
| `SIT_KEY_F21` | 310 | F21 |
| `SIT_KEY_F22` | 311 | F22 |
| `SIT_KEY_F23` | 312 | F23 |
| `SIT_KEY_F24` | 313 | F24 |
| `SIT_KEY_F25` | 314 | F25 |
| `SIT_KEY_KP_0` | 320 | Keypad 0 |
| `SIT_KEY_KP_1` | 321 | Keypad 1 |
| `SIT_KEY_KP_2` | 322 | Keypad 2 |
| `SIT_KEY_KP_3` | 323 | Keypad 3 |
| `SIT_KEY_KP_4` | 324 | Keypad 4 |
| `SIT_KEY_KP_5` | 325 | Keypad 5 |
| `SIT_KEY_KP_6` | 326 | Keypad 6 |
| `SIT_KEY_KP_7` | 327 | Keypad 7 |
| `SIT_KEY_KP_8` | 328 | Keypad 8 |
| `SIT_KEY_KP_9` | 329 | Keypad 9 |
| `SIT_KEY_KP_DECIMAL` | 330 | Keypad . |
| `SIT_KEY_KP_DIVIDE` | 331 | Keypad / |
| `SIT_KEY_KP_MULTIPLY` | 332 | Keypad * |
| `SIT_KEY_KP_SUBTRACT` | 333 | Keypad - |
| `SIT_KEY_KP_ADD` | 334 | Keypad + |
| `SIT_KEY_KP_ENTER` | 335 | Keypad Enter |
| `SIT_KEY_KP_EQUAL` | 336 | Keypad = |
| `SIT_KEY_LEFT_SHIFT` | 340 | Left Shift |
| `SIT_KEY_LEFT_CONTROL` | 341 | Left Control |
| `SIT_KEY_LEFT_ALT` | 342 | Left Alt |
| `SIT_KEY_LEFT_SUPER` | 343 | Left Super/Windows/Command |
| `SIT_KEY_RIGHT_SHIFT` | 344 | Right Shift |
| `SIT_KEY_RIGHT_CONTROL` | 345 | Right Control |
| `SIT_KEY_RIGHT_ALT` | 346 | Right Alt |
| `SIT_KEY_RIGHT_SUPER` | 347 | Right Super/Windows/Command |
| `SIT_KEY_MENU` | 348 | Menu |

### Modifier Bitmasks
These bitmasks are used in callback functions to check for modifier key states.

| Bitmask | Value |
|---|---|
| `SIT_MOD_SHIFT` | 0x0001 |
| `SIT_MOD_CONTROL` | 0x0002 |
| `SIT_MOD_ALT` | 0x0004 |
| `SIT_MOD_SUPER` | 0x0008 |
| `SIT_MOD_CAPS_LOCK` | 0x0010 |
| `SIT_MOD_NUM_LOCK` | 0x0020 |

### Callbacks
The input module allows you to register callback functions to be notified of input events as they happen, as an alternative to polling for state each frame.

#### `SituationKeyCallback`
`typedef void (*SituationKeyCallback)(int key, int scancode, int action, SituationModifiers mods, void* user_data);`
-   `key`: The keyboard key that was pressed or released (e.g., `SIT_KEY_A`).
-   `scancode`: The system-specific scancode of the key.
-   `action`: The key action (`SIT_PRESS`, `SIT_RELEASE`, or `SIT_REPEAT`).
-   `mods`: A `SituationModifiers` bitmask of modifier keys that were held down (`SIT_MOD_SHIFT`, `SIT_MOD_CONTROL`, etc.).
-   `user_data`: The custom user data pointer you provided when setting the callback.

---
#### `SituationMouseButtonCallback`
`typedef void (*SituationMouseButtonCallback)(int button, int action, SituationModifiers mods, void* user_data);`
-   `button`: The mouse button that was pressed or released (e.g., `SIT_MOUSE_BUTTON_LEFT`).
-   `action`: The button action (`SIT_PRESS` or `SIT_RELEASE`).
-   `mods`: A `SituationModifiers` bitmask of modifier keys.
-   `user_data`: Custom user data.

---
#### `SituationCursorPosCallback`
`typedef void (*SituationCursorPosCallback)(double xpos, double ypos, void* user_data);`
-   `xpos`, `ypos`: The new cursor position in screen coordinates.
-   `user_data`: Custom user data.

---
#### `SituationScrollCallback`
`typedef void (*SituationScrollCallback)(double xoffset, double yoffset, void* user_data);`
-   `xoffset`, `yoffset`: The scroll offset.
-   `user_data`: Custom user data.

#### Functions
### Functions

#### Keyboard Input
---
#### `SituationIsKeyDown` / `SituationIsKeyUp`
Checks if a key is currently being held down or is up. This checks the *state* of the key and will return `true` for every frame the key is held. It is ideal for continuous actions like character movement.
```c
bool SituationIsKeyDown(int key);
bool SituationIsKeyUp(int key);
```
**Usage Example:**
```c
// For continuous movement, check the key state every frame.
if (SituationIsKeyDown(SIT_KEY_W)) {
    player.y -= PLAYER_SPEED * SituationGetFrameTime();
}
if (SituationIsKeyDown(SIT_KEY_S)) {
    player.y += PLAYER_SPEED * SituationGetFrameTime();
}
```
---
#### `SituationIsKeyPressed` / `SituationIsKeyReleased`
Checks if a key was just pressed down or released in the current frame. This is a single-trigger *event* and will only return `true` for the exact frame the action occurred. It is ideal for discrete actions like jumping or opening a menu.
```c
bool SituationIsKeyPressed(int key);
bool SituationIsKeyReleased(int key);
```
**Usage Example:**
```c
// For a discrete action like jumping, use the key pressed event.
if (SituationIsKeyPressed(SIT_KEY_SPACE)) {
    player.velocity_y = JUMP_FORCE;
}

// Toggling a menu is another good use case for a single-trigger event.
if (SituationIsKeyPressed(SIT_KEY_ESCAPE)) {
    g_is_menu_open = !g_is_menu_open;
}
```

---
#### `SituationGetKeyPressed`
Gets the last key pressed.
```c
int SituationGetKeyPressed(void);
```
**Usage Example:**
```c
int last_key = SituationGetKeyPressed();
if (last_key > 0) {
    // A key was pressed this frame, you can use it for text input or debug commands.
    printf("Key pressed: %c\n", (char)last_key);
}
```

---
#### `SituationGetKeyPressedEx`
Gets the next key from the press queue along with its platform-specific scancode. This is useful when you need both the logical key and the physical key location.
```c
int SituationGetKeyPressedEx(int* out_scancode);
```
**Usage Example:**
```c
int scancode = 0;
int key = SituationGetKeyPressedEx(&scancode);
if (key > 0) {
    printf("Key %d pressed (scancode: %d)\n", key, scancode);
}
```

---
#### `SituationPeekKeyPressed`
Peeks at the next key in the press queue without consuming it. This allows you to check what key is next without removing it from the queue.
```c
int SituationPeekKeyPressed(void);
```
**Usage Example:**
```c
int next_key = SituationPeekKeyPressed();
if (next_key == SIT_KEY_ENTER) {
    // Prepare to handle Enter key, but don't consume it yet
    printf("Enter key is next in queue\n");
}
```

---
#### `SituationPeekKeyPressedEx`
Peeks at the next key and its scancode without consuming them from the queue.
```c
int SituationPeekKeyPressedEx(int* out_scancode);
```
**Usage Example:**
```c
int scancode = 0;
int next_key = SituationPeekKeyPressedEx(&scancode);
if (next_key > 0) {
    printf("Next key: %d (scancode: %d)\n", next_key, scancode);
}
```

---
#### `SituationGetCharPressed`
Gets the next Unicode character from the text input queue. This is ideal for text entry fields as it handles character composition and international keyboards correctly.
```c
unsigned int SituationGetCharPressed(void);
```
**Usage Example:**
```c
// In a text input handler
unsigned int character = SituationGetCharPressed();
if (character > 0) {
    // Append character to text buffer (handle UTF-8 encoding as needed)
    AppendCharToTextBuffer(character);
}
```

---
#### `SituationIsLockKeyPressed`
Checks if a lock key (Caps Lock, Num Lock) is currently active/toggled on.
```c
bool SituationIsLockKeyPressed(int lock_key_mod);
```
**Usage Example:**
```c
// Check if Caps Lock is on
if (SituationIsLockKeyPressed(SIT_MOD_CAPS_LOCK)) {
    printf("Caps Lock is ON\n");
}

// Check if Num Lock is on
if (SituationIsLockKeyPressed(SIT_MOD_NUM_LOCK)) {
    printf("Num Lock is ON\n");
}
```

---
#### `SituationIsScrollLockOn`
Checks if Scroll Lock is currently toggled on.
```c
bool SituationIsScrollLockOn(void);
```
**Usage Example:**
```c
if (SituationIsScrollLockOn()) {
    printf("Scroll Lock is active\n");
}
```

---
#### `SituationIsModifierPressed`
Checks if a modifier key (Shift, Ctrl, Alt, Super) is currently pressed.
```c
bool SituationIsModifierPressed(int modifier);
```
**Usage Example:**
```c
// Check for Ctrl+S (Save)
if (SituationIsModifierPressed(SIT_MOD_CONTROL) && SituationIsKeyPressed(SIT_KEY_S)) {
    SaveFile();
}

// Check for Shift+Click
if (SituationIsModifierPressed(SIT_MOD_SHIFT) && SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    SelectMultiple();
}
```

---
#### `SituationIsKeyUp`
Checks if a key is currently up (not pressed). This is the opposite of `SituationIsKeyDown`.
```c
bool SituationIsKeyUp(int key);
```
**Usage Example:**
```c
if (SituationIsKeyUp(SIT_KEY_SPACE)) {
    // Space bar is not being pressed
}
```

---
#### `SituationIsKeyReleased`
Checks if a key was just released this frame (single-frame event).
```c
bool SituationIsKeyReleased(int key);
```
**Usage Example:**
```c
if (SituationIsKeyReleased(SIT_KEY_SPACE)) {
    // Player just released the jump button
    EndCharging();
}
```

---
#### `SituationGetKeyScancode`
Gets the platform-specific scancode for a logical key code.
```c
int SituationGetKeyScancode(int key);
```
**Usage Example:**
```c
int scancode = SituationGetKeyScancode(SIT_KEY_W);
printf("W key scancode: %d\n", scancode);
```

---
#### `SituationGetCharFromScancode`
Converts a scancode to its character representation.
```c
int SituationGetCharFromScancode(int scancode);
```
**Usage Example:**
```c
int scancode = SituationGetKeyScancode(SIT_KEY_A);
int character = SituationGetCharFromScancode(scancode);
printf("Character: %c\n", character);
```

---
#### `SituationIsScancodeDown`
Checks if a key is pressed using its scancode instead of logical key code.
```c
bool SituationIsScancodeDown(int scancode);
```
**Usage Example:**
```c
// Useful for handling keyboard layouts that differ from QWERTY
int w_scancode = SituationGetKeyScancode(SIT_KEY_W);
if (SituationIsScancodeDown(w_scancode)) {
    MoveForward();
}
```

---
#### `SituationSetKeyCallback`
Sets a callback function for all keyboard key events.
```c
void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data);
```
**Usage Example:**
```c
void my_key_logger(int key, int scancode, int action, int mods, void* user_data) {
    if (action == SIT_PRESS) {
        printf("Key pressed: %d\n", key);
    }
}
SituationSetKeyCallback(my_key_logger, NULL);
```

---
#### `SituationSetMouseButtonCallback` / `SituationSetCursorPosCallback` / `SituationSetScrollCallback`
Sets callback functions for mouse button, cursor movement, and scroll wheel events.
```c
void SituationSetMouseButtonCallback(SituationMouseButtonCallback callback, void* user_data);
void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data);
void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data);
```

---
#### `SituationSetCharCallback`
Sets a callback for Unicode character input, which is useful for text entry fields. The callback receives individual Unicode codepoints as they're typed.

```c
void SituationSetCharCallback(SituationCharCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call when character is typed (or NULL to remove)
- `user_data` - User data passed to callback

**Callback Signature:**
```c
typedef void (*SituationCharCallback)(unsigned int codepoint, void* user_data);
```

**Usage Example:**
```c
// Text input field
typedef struct {
    char buffer[256];
    int length;
} TextInput;

void OnCharInput(unsigned int codepoint, void* user_data) {
    TextInput* input = (TextInput*)user_data;
    
    // Convert codepoint to UTF-8 and append
    if (input->length < 255) {
        // Simple ASCII handling
        if (codepoint < 128) {
            input->buffer[input->length++] = (char)codepoint;
            input->buffer[input->length] = '\0';
        }
    }
}

// Set up text input
TextInput input = {0};
SituationSetCharCallback(OnCharInput, &input);

// In main loop
while (!SituationShouldClose()) {
    SituationPollInputEvents();  // Triggers callback
    
    // Handle backspace separately
    if (SituationIsKeyPressed(SIT_KEY_BACKSPACE) && input.length > 0) {
        input.buffer[--input.length] = '\0';
    }
    
    DrawTextInput(input.buffer);
}

// Cleanup
SituationSetCharCallback(NULL, NULL);
```

**Notes:**
- Receives Unicode codepoints, not raw key codes
- Handles keyboard layout automatically
- Use for text input, not game controls
- Call with NULL to remove callback

---
#### `SituationSetDropCallback`
Sets a callback that is fired when files are dragged and dropped onto the window. Useful for loading assets or opening files.

```c
void SituationSetDropCallback(SituationDropCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call when files are dropped (or NULL to remove)
- `user_data` - User data passed to callback

**Callback Signature:**
```c
typedef void (*SituationDropCallback)(int count, const char** paths, void* user_data);
```

**Usage Example:**
```c
// Handle dropped files
void OnFileDrop(int count, const char** paths, void* user_data) {
    printf("Dropped %d file(s):\n", count);
    
    for (int i = 0; i < count; i++) {
        printf("  - %s\n", paths[i]);
        
        // Load based on extension
        if (strstr(paths[i], ".png") || strstr(paths[i], ".jpg")) {
            LoadTexture(paths[i]);
        } else if (strstr(paths[i], ".wav") || strstr(paths[i], ".ogg")) {
            LoadSound(paths[i]);
        } else if (strstr(paths[i], ".gltf")) {
            LoadModel(paths[i]);
        }
    }
}

// Set up drop handler
SituationSetDropCallback(OnFileDrop, NULL);

// Alternative: Use polling instead of callback
if (SituationIsFileDropped()) {
    int count;
    char** paths = SituationLoadDroppedFiles(&count);
    // Process files...
    SituationUnloadDroppedFiles(paths, count);
}
```

**Notes:**
- Paths are absolute file paths
- Multiple files can be dropped at once
- Paths are valid only during callback
- Alternative: use `SituationIsFileDropped()` polling

---
#### `SituationSetFileDropCallback`
Sets a callback for file drop events. This is an alias for `SituationSetDropCallback` with the same functionality.

```c
void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call when files are dropped
- `user_data` - User data passed to callback

**Usage Example:**
```c
// Same as SituationSetDropCallback
void OnFileDropped(int count, const char** paths, void* user_data) {
    for (int i = 0; i < count; i++) {
        printf("File dropped: %s\n", paths[i]);
    }
}

SituationSetFileDropCallback(OnFileDropped, NULL);
```

**Notes:**
- Identical to `SituationSetDropCallback`
- Use whichever name is clearer for your use case

---
#### `SituationSetFocusCallback`
Sets a callback for window focus events. Called when the window gains or loses focus.

```c
void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call on focus change (or NULL to remove)
- `user_data` - User data passed to callback

**Callback Signature:**
```c
typedef void (*SituationFocusCallback)(bool focused, void* user_data);
```

**Usage Example:**
```c
// Handle focus changes
void OnFocusChanged(bool focused, void* user_data) {
    if (focused) {
        printf("Window gained focus\n");
        ResumeGame();
        SituationSetAudioMasterVolume(1.0f);
    } else {
        printf("Window lost focus\n");
        PauseGame();
        SituationSetAudioMasterVolume(0.0f);
    }
}

// Set up focus callback
SituationSetFocusCallback(OnFocusChanged, NULL);

// Alternative: Poll focus state
if (!SituationHasWindowFocus()) {
    PauseGame();
}
```

**Notes:**
- Called when window gains/loses focus
- Use for auto-pause functionality
- Alternative: poll with `SituationHasWindowFocus()`

---
#### `SituationSetResizeCallback`
Sets a callback function for window framebuffer resize events. Called when the window is resized.

```c
void SituationSetResizeCallback(SituationResizeCallback callback, void* user_data);
```

**Parameters:**
- `callback` - Function to call on resize (or NULL to remove)
- `user_data` - User data passed to callback

**Callback Signature:**
```c
typedef void (*SituationResizeCallback)(int width, int height, void* user_data);
```

**Usage Example:**
```c
// Handle window resize
void OnWindowResize(int width, int height, void* user_data) {
    printf("Window resized to %dx%d\n", width, height);
    
    // Update camera aspect ratio
    Camera* camera = (Camera*)user_data;
    camera->aspect = (float)width / (float)height;
    UpdateProjectionMatrix(camera);
    
    // Recreate framebuffers
    RecreateFramebuffers(width, height);
}

// Set up resize callback
Camera camera = {0};
SituationSetResizeCallback(OnWindowResize, &camera);

// Alternative: Poll resize event
if (SituationIsWindowResized()) {
    int width = SituationGetRenderWidth();
    int height = SituationGetRenderHeight();
    RecreateFramebuffers(width, height);
}
```

**Notes:**
- Called when framebuffer size changes
- Width/height are in pixels, not screen coordinates
- Use for recreating resolution-dependent resources
- Alternative: poll with `SituationIsWindowResized()`

---
#### `SituationSetCursor`
Sets the mouse cursor to a standard shape (arrow, hand, crosshair, etc.).

```c
void SituationSetCursor(SituationCursorShape shape);
```

**Parameters:**
- `shape` - Cursor shape to display

**Cursor Shapes:**
- `SITUATION_CURSOR_ARROW` - Standard arrow pointer
- `SITUATION_CURSOR_IBEAM` - Text input I-beam
- `SITUATION_CURSOR_CROSSHAIR` - Crosshair for aiming
- `SITUATION_CURSOR_HAND` - Pointing hand for links
- `SITUATION_CURSOR_HRESIZE` - Horizontal resize arrows
- `SITUATION_CURSOR_VRESIZE` - Vertical resize arrows

**Usage Example:**
```c
// Change cursor based on UI element
if (IsHoveringButton()) {
    SituationSetCursor(SITUATION_CURSOR_HAND);
} else if (IsHoveringTextInput()) {
    SituationSetCursor(SITUATION_CURSOR_IBEAM);
} else if (IsHoveringResizeHandle()) {
    SituationSetCursor(SITUATION_CURSOR_HRESIZE);
} else {
    SituationSetCursor(SITUATION_CURSOR_ARROW);
}

// Crosshair for aiming mode
if (IsAiming()) {
    SituationSetCursor(SITUATION_CURSOR_CROSSHAIR);
}
```

**Notes:**
- Changes cursor appearance immediately
- Use for UI feedback
- Cursor is hidden if disabled with `SituationDisableCursor()`

---
#### Clipboard
---
#### `SituationGetClipboardText`
Gets UTF-8 encoded text from the system clipboard. The returned string is heap-allocated and must be freed by the caller using `SituationFreeString`.
```c
SituationError SituationGetClipboardText(const char** out_text);
```
**Usage Example:**
```c
// In an input handler for Ctrl+V
if (SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) && SituationIsKeyPressed(SIT_KEY_V)) {
    const char* clipboard_text = NULL;
    if (SituationGetClipboardText(&clipboard_text) == SITUATION_SUCCESS) {
        // Paste text into an input field.
        SituationFreeString((char*)clipboard_text);
    }
}
```
---
#### `SituationSetClipboardText`
Sets the system clipboard to the provided UTF-8 encoded text.
```c
SituationError SituationSetClipboardText(const char* text);
```
**Usage Example:**
```c
// In an input handler for Ctrl+C
if (SituationIsKeyDown(SIT_KEY_LEFT_CONTROL) && SituationIsKeyPressed(SIT_KEY_C)) {
    // Copy selected text to the clipboard.
    SituationSetClipboardText(selected_text);
}
```
---
#### Mouse Input
---
#### `SituationGetMousePosition` / `SituationGetMouseDelta`
Gets the mouse cursor position in screen coordinates, or the mouse movement since the last frame. `SituationGetMouseDelta` is particularly useful for implementing camera controls when the cursor is disabled.
```c
vec2 SituationGetMousePosition(void);
vec2 SituationGetMouseDelta(void);
```
**Usage Example:**
```c
// For a 3D camera, use the mouse delta to control pitch and yaw.
if (IsCursorDisabled()) { // Assuming you have a check for this state
    vec2 mouse_delta = SituationGetMouseDelta();
    camera.yaw   += mouse_delta[0] * MOUSE_SENSITIVITY;
    camera.pitch -= mouse_delta[1] * MOUSE_SENSITIVITY;
}
```
---
#### `SituationIsMouseButtonDown`
Checks if a mouse button is currently being held down. This is a *state* check and is suitable for continuous actions like dragging or aiming.
```c
bool SituationIsMouseButtonDown(int button);
```
**Usage Example:**
```c
// Useful for continuous actions like aiming down sights.
if (SituationIsMouseButtonDown(SIT_MOUSE_BUTTON_RIGHT)) {
    // Zoom in with weapon sights.
}
```

---
#### `SituationIsMouseButtonPressed`
Checks if a mouse button was just pressed down in the current frame. This is a single-trigger *event*, ideal for discrete actions like clicking a button or firing a weapon.
```c
bool SituationIsMouseButtonPressed(int button);
```
**Usage Example:**
```c
// Ideal for discrete actions like firing a weapon.
if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    FireProjectile();
}
```

---
#### `SituationGetMouseButtonPressed`
Gets the mouse button that was pressed this frame.
```c
int SituationGetMouseButtonPressed(void);
```
**Usage Example:**
```c
// Useful for UI interactions where you need to know which button was clicked.
int clicked_button = SituationGetMouseButtonPressed();
if (clicked_button == SIT_MOUSE_BUTTON_LEFT) {
    // Handle left click on a UI element.
} else if (clicked_button == SIT_MOUSE_BUTTON_RIGHT) {
    // Open a context menu.
}
```

---
#### `SituationIsMouseButtonReleased`
Checks if a mouse button was released this frame. This is a single-frame event, ideal for detecting button releases.

```c
bool SituationIsMouseButtonReleased(int button);
```

**Parameters:**
- `button` - Mouse button to check (e.g., `SIT_MOUSE_BUTTON_LEFT`, `SIT_MOUSE_BUTTON_RIGHT`)

**Returns:** `true` if button was released this frame, `false` otherwise

**Usage Example:**
```c
// Detect click release
if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    StartDragging();
}
if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_LEFT)) {
    StopDragging();
}

// Charge-up mechanic
static float charge_time = 0.0f;
if (SituationIsMouseButtonDown(SIT_MOUSE_BUTTON_LEFT)) {
    charge_time += SituationGetFrameTime();
    ShowChargeIndicator(charge_time);
}
if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_LEFT)) {
    FireWeapon(charge_time);
    charge_time = 0.0f;
}

// Context menu
if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_RIGHT)) {
    vec2 mouse_pos = SituationGetMousePosition();
    ShowContextMenu(mouse_pos[0], mouse_pos[1]);
}

// Drag and drop
static bool dragging = false;
static vec2 drag_start;

if (SituationIsMouseButtonPressed(SIT_MOUSE_BUTTON_LEFT)) {
    dragging = true;
    vec2 pos = SituationGetMousePosition();
    drag_start[0] = pos[0];
    drag_start[1] = pos[1];
}

if (SituationIsMouseButtonReleased(SIT_MOUSE_BUTTON_LEFT)) {
    if (dragging) {
        vec2 pos = SituationGetMousePosition();
        OnDragComplete(drag_start, pos);
        dragging = false;
    }
}
```

**Notes:**
- Single-frame event, not a continuous state
- Use for detecting button releases
- Pair with `SituationIsMouseButtonPressed()` for press/release logic
- Common buttons: `SIT_MOUSE_BUTTON_LEFT`, `SIT_MOUSE_BUTTON_RIGHT`, `SIT_MOUSE_BUTTON_MIDDLE`

---
#### `SituationSetMousePosition`
Sets the mouse cursor position within the window.
```c
void SituationSetMousePosition(Vector2 pos);
```
**Usage Example:**
```c
// Center the mouse cursor in the window
Vector2 center = {
    SituationGetScreenWidth() / 2.0f,
    SituationGetScreenHeight() / 2.0f
};
SituationSetMousePosition(center);
```

---
#### `SituationSetMouseOffset`
Sets a software offset for the mouse position. This is useful for implementing custom coordinate systems or UI scaling.
```c
void SituationSetMouseOffset(Vector2 offset);
```
**Usage Example:**
```c
// Offset mouse coordinates to account for a UI panel
Vector2 ui_offset = {200.0f, 50.0f};
SituationSetMouseOffset(ui_offset);
```

---
#### `SituationSetMouseScale`
Sets a software scale for the mouse position and delta. This is useful when rendering at a different resolution than the window size.
```c
void SituationSetMouseScale(Vector2 scale);
```
**Usage Example:**
```c
// Scale mouse coordinates when rendering at 1920x1080 in a 1280x720 window
Vector2 scale = {1920.0f / 1280.0f, 1080.0f / 720.0f};
SituationSetMouseScale(scale);
```

---
#### `SituationGetMouseWheelMove`
Gets the vertical mouse wheel movement for the current frame. Positive values indicate scrolling up/away from the user, negative values indicate scrolling down/towards the user.
```c
float SituationGetMouseWheelMove(void);
```
**Usage Example:**
```c
// Zoom camera based on mouse wheel
float wheel = SituationGetMouseWheelMove();
if (wheel != 0.0f) {
    camera_zoom += wheel * ZOOM_SPEED;
}
```

---
#### `SituationGetMouseWheelMoveV`
Gets both vertical and horizontal mouse wheel movement as a Vector2. The x component is horizontal scroll, y component is vertical scroll.
```c
Vector2 SituationGetMouseWheelMoveV(void);
```
**Usage Example:**
```c
// Handle both vertical and horizontal scrolling
Vector2 wheel = SituationGetMouseWheelMoveV();
scroll_offset_x += wheel.x * SCROLL_SPEED;
scroll_offset_y += wheel.y * SCROLL_SPEED;
```

---
#### `SituationGetMouseDelta`
Gets the mouse movement since the last frame. Useful for camera controls.
```c
Vector2 SituationGetMouseDelta(void);
```
**Usage Example:**
```c
// First-person camera control
Vector2 delta = SituationGetMouseDelta();
camera_yaw += delta.x * MOUSE_SENSITIVITY;
camera_pitch -= delta.y * MOUSE_SENSITIVITY;
```

---
#### Gamepad Input
---
#### `SituationIsJoystickPresent`
Checks if a joystick or gamepad is connected at the given joystick ID (0-15).
```c
bool SituationIsJoystickPresent(int jid);
```
**Usage Example:**
```c
// Check for a joystick at the first slot.
if (SituationIsJoystickPresent(0)) {
    printf("A joystick/gamepad is connected at JID 0.\n");
}
```

---
#### `SituationIsGamepad`
Checks if the joystick at the given ID has a standard gamepad mapping, making it compatible with the `SIT_GAMEPAD_*` enums.
```c
bool SituationIsGamepad(int jid);
```
**Usage Example:**
```c
// Before using gamepad-specific functions, check if the device has a standard mapping.
if (SituationIsJoystickPresent(0) && SituationIsGamepad(0)) {
    // Now it's safe to use functions like SituationIsGamepadButtonPressed.
}
```

---
#### `SituationGetJoystickName`
Gets the implementation-defined name of a joystick (e.g., "Xbox Controller").
```c
const char* SituationGetJoystickName(int jid);
```
**Usage Example:**
```c
#define GAMEPAD_ID 0
if (SituationIsJoystickPresent(GAMEPAD_ID) && SituationIsGamepad(GAMEPAD_ID)) {
    printf("Gamepad '%s' is ready.\n", SituationGetJoystickName(GAMEPAD_ID));
}
```
---
#### `SituationIsGamepadButtonDown` / `SituationIsGamepadButtonPressed`
Checks if a gamepad button is held down (state) or was just pressed (event).
```c
bool SituationIsGamepadButtonDown(int jid, int button);
bool SituationIsGamepadButtonPressed(int jid, int button);
```
**Usage Example:**
```c
if (SituationIsGamepadButtonPressed(GAMEPAD_ID, SIT_GAMEPAD_BUTTON_A)) {
    // Jump
}
```

---
#### `SituationGetGamepadButtonPressed`
Gets the next gamepad button from the press queue. This is useful for UI navigation where you don't care which specific gamepad was used.
```c
int SituationGetGamepadButtonPressed(void);
```
**Usage Example:**
```c
// In a menu system, accept input from any connected gamepad
int button = SituationGetGamepadButtonPressed();
if (button == SIT_GAMEPAD_BUTTON_A) {
    ConfirmSelection();
} else if (button == SIT_GAMEPAD_BUTTON_B) {
    CancelSelection();
}
```

---
#### `SituationIsGamepadButtonReleased`
Checks if a gamepad button was released this frame (a single-trigger event).
```c
bool SituationIsGamepadButtonReleased(int jid, int button);
```
**Usage Example:**
```c
// Detect when the player releases the trigger button
if (SituationIsGamepadButtonReleased(GAMEPAD_ID, SIT_GAMEPAD_BUTTON_RIGHT_TRIGGER)) {
    ReleaseCharge();
}
```

---
#### `SituationGetGamepadAxisValue`
Gets the value of a gamepad axis with deadzone applied. Returns a value between -1.0 and 1.0.
```c
float SituationGetGamepadAxisValue(int jid, int axis);
```
**Usage Example:**
```c
// Use left stick for character movement
float left_x = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_LEFT_X);
float left_y = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_LEFT_Y);

player_velocity.x = left_x * MOVE_SPEED;
player_velocity.y = left_y * MOVE_SPEED;

// Use right stick for camera control
float right_x = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_RIGHT_X);
float right_y = SituationGetGamepadAxisValue(GAMEPAD_ID, SIT_GAMEPAD_AXIS_RIGHT_Y);

camera_yaw += right_x * CAMERA_SENSITIVITY * deltaTime;
camera_pitch += right_y * CAMERA_SENSITIVITY * deltaTime;
```

---
#### `SituationGetGamepadAxisCount`
Gets the number of axes available on a gamepad.
```c
int SituationGetGamepadAxisCount(int jid);
```
**Usage Example:**
```c
int axis_count = SituationGetGamepadAxisCount(GAMEPAD_ID);
printf("Gamepad has %d axes\n", axis_count);
```

---
#### `SituationSetGamepadMappings`
Loads a new set of gamepad mappings from a string. This allows you to add support for custom or non-standard controllers.
```c
int SituationSetGamepadMappings(const char* mappings);
```
**Usage Example:**
```c
// Load custom gamepad mappings from a file
const char* custom_mappings = LoadTextFile("gamecontrollerdb.txt");
if (SituationSetGamepadMappings(custom_mappings)) {
    printf("Custom gamepad mappings loaded successfully\n");
}
```

---
#### `SituationSetGamepadVibration`
Sets gamepad vibration/rumble intensity. Note: This is currently Windows-only.
```c
bool SituationSetGamepadVibration(int jid, float left_motor, float right_motor);
```
**Usage Example:**
```c
// Trigger a short rumble when the player takes damage
SituationSetGamepadVibration(GAMEPAD_ID, 0.8f, 0.5f);

// Stop vibration after a delay
// (In your update loop after the delay)
SituationSetGamepadVibration(GAMEPAD_ID, 0.0f, 0.0f);
```

---
#### `SituationSetJoystickCallback`
Sets a callback for joystick connection and disconnection events.
```c
void SituationSetJoystickCallback(SituationJoystickCallback callback, void* user_data);
```
**Usage Example:**
```c
void on_joystick_event(int jid, int event, void* user_data) {
    if (event == GLFW_CONNECTED) {
        printf("Joystick %d connected: %s\n", jid, SituationGetJoystickName(jid));
    } else if (event == GLFW_DISCONNECTED) {
        printf("Joystick %d disconnected\n", jid);
    }
}

SituationSetJoystickCallback(on_joystick_event, NULL);
```

---

#### `SituationSetMousePosition`
Sets the mouse cursor position within the window.
```c
SITAPI void SituationSetMousePosition(Vector2 pos);
```
**Usage Example:**
```c
// Center the mouse cursor in a 1280x720 window
Vector2 center = { .x = 1280 / 2.0f, .y = 720 / 2.0f };
SituationSetMousePosition(center);
```

---

#### `SituationConsumeKeyPress`
Eat a key press this frame so later IsKeyPressed() returns false (e.g. for global hotkeys to take priority over app controls).
```c
void SituationConsumeKeyPress(int key);
```
---

#### `SituationSetLogCallback`
Set a custom log callback.
```c
void SituationSetLogCallback(void (*callback)(SituationLogLevel level, const char* message, void* user), void* user);
```

