/***************************************************************************************************
*
*   situation_base_callbacks.h - Public Callback Type Definitions
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   All typedef'd function-pointer signatures for callbacks the user may register with Situation.
*
*   Design contract (frozen — these signatures will never change in any future version):
*     • Invoked exclusively from the main thread, EXCEPT SituationAudioCaptureCallback which runs
*       on the audio thread unless SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD is set.
*     • Real-time safe where documented (audio callbacks).
*     • Zero overhead — no virtual dispatch, no hidden allocations.
*     • Always user-data driven — last parameter is always the void* you supplied.
*     • Never call other SITAPI functions from inside a callback unless the documentation for
*       that specific callback explicitly declares it safe.
*
*   Dependencies: situation_base_types.h only (no miniaudio, no GL/VK, no platform headers).
*
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_BASE_CALLBACKS_H
#define SITUATION_BASE_CALLBACKS_H

#include "situation_base_types.h"

// ================================================================================================
// WINDOW / OS EVENT CALLBACKS
// ================================================================================================

/**
 * @brief Files or folders dragged from the OS onto the window.
 * @param count     Number of items dropped.
 * @param paths     Array of UTF-8 paths — valid only for the duration of this callback.
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationFileDropCallback)(
    int          count,
    const char** paths,
    void*        user_data
);

/**
 * @brief Result of an asynchronous file load.
 * @param data      Loaded bytes, or NULL on failure. CALLER MUST FREE with SIT_FREE / SituationFreeString.
 * @param size      Byte count of data.
 * @param user_data Pointer supplied at submission.
 */
typedef void (*SituationFileLoadCallback)(
    void*   data,
    size_t  size,
    void*   user_data
);

/**
 * @brief Result of an asynchronous file save.
 * @param success   true if the write succeeded.
 * @param user_data Pointer supplied at submission.
 */
typedef void (*SituationFileSaveCallback)(
    bool   success,
    void*  user_data
);

/**
 * @brief Result of an asynchronous text file load.
 * @param text      Null-terminated string, or NULL on failure. CALLER MUST FREE.
 * @param user_data Pointer supplied at submission.
 */
typedef void (*SituationFileTextLoadCallback)(
    char*  text,
    void*  user_data
);

/**
 * @brief Window focus change (alt-tab, click away, etc.).
 * @param gained_focus true = window gained focus, false = lost.
 * @param user_data    Pointer supplied at registration.
 */
typedef void (*SituationFocusCallback)(
    bool   gained_focus,
    void*  user_data
);

/**
 * @brief Title-bar maximize / restore.
 * @details Requires SITUATION_FLAG_WINDOW_RESIZABLE at init.
 * @param maximized true = maximized, false = restored.
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationMaximizeCallback)(
    bool   maximized,
    void*  user_data
);

/**
 * @brief User clicked the OS close button.
 * @details Most code just polls SituationWindowShouldClose(); this callback is rarely needed.
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationWindowCloseCallback)(
    void* user_data
);

// ================================================================================================
// INPUT EVENT CALLBACKS (optional event-driven API — polling is always available)
// ================================================================================================

/**
 * @brief Key press, release, or repeat event.
 * @param key       SIT_KEY_xxx constant.
 * @param scancode  Platform-specific scancode (useful for non-QWERTY layouts).
 * @param action    SIT_PRESS, SIT_RELEASE, or SIT_REPEAT.
 * @param mods      Bitfield of SIT_MOD_xxx modifier keys.
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationKeyCallback)(
    int                key,
    int                scancode,
    int                action,
    SituationModifiers mods,
    void*              user_data
);

/**
 * @brief Text input — one Unicode codepoint per event.
 * @details Handles IME, dead keys, etc. Separate from raw key events.
 * @param codepoint UTF-32 codepoint.
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationCharCallback)(
    unsigned int codepoint,
    void*        user_data
);

/**
 * @brief Mouse button press or release.
 * @param button    SIT_MOUSE_BUTTON_1 … SIT_MOUSE_BUTTON_8.
 * @param action    SIT_PRESS or SIT_RELEASE.
 * @param mods      Modifier bitfield.
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationMouseButtonCallback)(
    int                button,
    int                action,
    SituationModifiers mods,
    void*              user_data
);

/**
 * @brief Cursor position change (fires on every mouse move — can be very frequent).
 * @param position  Screen-space cursor position (HiDPI-aware, sub-pixel precision).
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationCursorPosCallback)(
    Vector2 position,
    void*   user_data
);

/**
 * @brief Mouse wheel or trackpad scroll.
 * @param offset    x/y scroll amount (y is typically ±1.0 per notch).
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationScrollCallback)(
    Vector2 offset,
    void*   user_data
);

/**
 * @brief Gamepad / controller hotplug event.
 * @param jid       Joystick ID (0 … SITUATION_MAX_JOYSTICKS-1).
 * @param event     GLFW_CONNECTED or GLFW_DISCONNECTED.
 * @param user_data Pointer supplied at registration.
 */
typedef void (*SituationJoystickCallback)(
    int   jid,
    int   event,
    void* user_data
);

// ================================================================================================
// CUSTOM AUDIO STREAMING CALLBACKS
// ================================================================================================
// SituationStreamSize / SituationSeekOrigin / SituationStreamResult are defined in
// situation_base_types.h and map 1:1 to miniaudio's ma_uint64 / ma_seek_origin / ma_result
// at the implementation boundary — keeping this header miniaudio-free.

/**
 * @brief Read callback for a custom audio stream.
 * @param pUserData  Your custom stream context pointer.
 * @param pBufferOut Buffer to fill with interleaved PCM data.
 * @param bytesToRead Maximum bytes to write into pBufferOut.
 * @return Actual number of bytes written.
 */
typedef SituationStreamSize (*SituationStreamReadCallback)(
    void*              pUserData,
    void*              pBufferOut,
    SituationStreamSize bytesToRead
);

/**
 * @brief Seek callback for a custom audio stream.
 * @param pUserData  Your custom stream context pointer.
 * @param byteOffset Byte offset relative to origin.
 * @param origin     SIT_SEEK_FROM_START, SIT_SEEK_FROM_CURRENT, or SIT_SEEK_FROM_END.
 * @return SIT_STREAM_SUCCESS (0) on success, non-zero on failure.
 */
typedef SituationStreamResult (*SituationStreamSeekCallback)(
    void*               pUserData,
    int64_t             byteOffset,
    SituationSeekOrigin origin
);

// ================================================================================================
// AUDIO CAPTURE CALLBACK (Microphone / Line-In)
// ================================================================================================

/**
 * @brief Called with a block of captured audio input.
 * @details Runs on the audio thread unless SITUATION_INIT_AUDIO_CAPTURE_MAIN_THREAD is set.
 *          Format: engine-native sample rate (default 48 kHz), stereo or mono, interleaved f32.
 *          Must be real-time safe — no SIT_MALLOC, no locking, no system calls.
 * @param input_buffer Interleaved 32-bit float samples (read-only).
 * @param frame_count  Number of frames in this block (typically 256–1024).
 * @param user_data    Pointer supplied at registration.
 */
typedef void (*SituationAudioCaptureCallback)(
    const float* input_buffer,
    uint32_t     frame_count,
    void*        user_data
);

// ================================================================================================
// CUSTOM DSP PROCESSOR CALLBACK
// ================================================================================================

/**
 * @brief Per-sound post-effects processor. Applied after built-in effects chain, before final
 *        volume/pan. Must be real-time safe — no SIT_MALLOC, no locking, no system calls.
 * @param buffer     Interleaved float samples — read/write in-place.
 * @param frames     Number of frames in this block.
 * @param channels   1 (mono) or 2 (stereo).
 * @param sampleRate Current engine sample rate in Hz.
 * @param user_data  Pointer supplied when the processor was added.
 */
typedef void (*SituationAudioProcessorCallback)(
    float*   buffer,
    uint32_t frames,
    uint32_t channels,
    uint32_t sampleRate,
    void*    user_data
);

// ================================================================================================
// INTERNAL / ADVANCED
// ================================================================================================

/**
 * @brief Raw GLFW error callback type. Exposed only for extremely advanced users who need to
 *        install a custom GLFW error handler alongside Situation.
 */
typedef void (*GLFWerrorfun)(int error_code, const char* description);

#endif // SITUATION_BASE_CALLBACKS_H
