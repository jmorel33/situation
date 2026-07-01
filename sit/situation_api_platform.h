/***************************************************************************************************
*
*   situation_api_platform.h - Lifecycle, Window, Input, Image, and Introspection API
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Core lifecycle (init/shutdown/frame), window and display management, keyboard/mouse/gamepad
*   input, CPU image and font utilities, logging levels, and hardware introspection queries.
*
*   Requires SITAPI from situation_api.h (included before this submodule in the umbrella chain).
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_PLATFORM_H
#define SITUATION_API_PLATFORM_H

#include <stdio.h>

#include "situation_api_config.h"
#include "situation_base_errno.h"
#include "situation_base_types.h"
#include "situation_base_callbacks.h"
#include "situation_api_types_system.h"
#include "situation_api_types_gpu.h"

typedef struct GLFWmonitor GLFWmonitor;

/**
 * @brief Logs a warning message in debug builds.
 * @details This function is intended for internal library use. It formats a warning message
 *          and, in debug builds (when NDEBUG is not defined), prints it to stderr and sets the
 *          library's last error state. In release builds, this function is compiled out to nothing.
 * @param code The SituationError code associated with the warning.
 * @param fmt The printf-style format string for the message.
 * @param ... Variable arguments for the format string.
 */
typedef enum {
    SIT_LOG_ALL = 0,
    SIT_LOG_TRACE,
    SIT_LOG_DEBUG,
    SIT_LOG_INFO,
    SIT_LOG_WARNING,
    SIT_LOG_ERROR,
    SIT_LOG_FATAL,
    SIT_LOG_NONE
} SituationLogLevel;



// Enable runtime main-thread asserts (debug only)
#ifdef SITUATION_ENABLE_MT_ASSERTS
    #define SIT_ASSERT_MAIN_THREAD() _SituationAssertMainThread(__FILE__, __LINE__)
#else
    #define SIT_ASSERT_MAIN_THREAD() do {} while(0)
#endif

// General debug assertion macro — fires SITUATION_ERROR_ASSERTION_FAILED and logs the failed expression.
// Active in debug builds (NDEBUG not defined); no-op in release.
// Named SITUATION_ASSERT (not SIT_ASSERT) to avoid collision with the test harness's SIT_ASSERT macro.
#ifndef NDEBUG
    #define SITUATION_ASSERT(cond) \
        do { \
            if (!(cond)) { \
                _SituationSetErrorFromCode(SITUATION_ERROR_ASSERTION_FAILED, \
                    "Assertion failed: " #cond " (" __FILE__ ")"); \
                fprintf(stderr, "[Situation] ASSERTION FAILED: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); \
            } \
        } while(0)
#else
    #define SITUATION_ASSERT(cond) do {} while(0)
#endif
SITAPI void SituationFreeString(char* str);                                             // Free a string allocated by the library (e.g., from path helpers).

#if defined(__cplusplus)
extern "C++" {
/**
 * @brief RAII Wrapper for Situation Strings (C++ only).
 * @details Automatically calls SituationFreeString() when it goes out of scope.
 */
struct SituationScopedString {
    char* str;
    SituationScopedString(char* s) : str(s) {}
    ~SituationScopedString() { if(str) SituationFreeString(str); }
    operator const char*() const { return str; }
    const char* get() const { return str; }
    // Prevent copy
    SituationScopedString(const SituationScopedString&) = delete;
    SituationScopedString& operator=(const SituationScopedString&) = delete;
    // Allow move
    SituationScopedString(SituationScopedString&& other) noexcept : str(other.str) { other.str = NULL; }
    SituationScopedString& operator=(SituationScopedString&& other) noexcept {
        if (this != &other) {
            if (str) SituationFreeString(str);
            str = other.str;
            other.str = NULL;
        }
        return *this;
    }
};
}
#endif
typedef struct SituationImage {
    void *data;                                     // Image raw data
    int width;                                      // Image width
    int height;                                     // Image height
    int channels;                                   // Number of channels (e.g., 4 for RGBA, 1 for Grayscale)
    SituationColorEncoding color_encoding;      	// Color space encoding (LINEAR or SRGB)
} SituationImage;

/**
 * @brief Specifies the type of flip operation to perform on an image.
 */
typedef enum SituationImageFlipMode {
    SIT_FLIP_VERTICAL,                              // Flips the image top-to-bottom.
    SIT_FLIP_HORIZONTAL,                            // Flips the image left-to-right.
    SIT_FLIP_BOTH                                   // Flips both vertically and horizontally (180-degree rotation).
} SituationImageFlipMode;

/**
 * @brief Screenshot output format.
 * @details Controls what file format SituationTakeScreenshot uses.
 *          Default is BMP (fastest, lossless, no compression overhead).
 *          Extension strings: sit_screenshot_format_ext[format]
 */
typedef enum SituationScreenshotFormat {
    SIT_SCREENSHOT_BMP = 0,     // Windows Bitmap (default, fastest write)
    SIT_SCREENSHOT_PNG,         // Portable Network Graphics (lossless, compressed)
    SIT_SCREENSHOT_JPG,         // JPEG (lossy, small files, quality 90)
    SIT_SCREENSHOT_TGA,         // Targa (lossless, RLE compressed)
    SIT_SCREENSHOT_FORMAT_COUNT
} SituationScreenshotFormat;

/** File extension for each SituationScreenshotFormat value (indexed by enum). */
static const char* const sit_screenshot_format_ext[] = {
    ".bmp",  // SIT_SCREENSHOT_BMP
    ".png",  // SIT_SCREENSHOT_PNG
    ".jpg",  // SIT_SCREENSHOT_JPG
    ".tga",  // SIT_SCREENSHOT_TGA
};
typedef struct SituationCPUInfo {
    char name[SITUATION_MAX_CPU_NAME_LEN];
    uint32_t thread_count;
    uint32_t core_count;
    float clock_speed_ghz;
} SituationCPUInfo;

typedef struct SituationGPUInfo {
    char name[SITUATION_MAX_GPU_NAME_LEN];
    uint64_t dedicated_memory_bytes;
} SituationGPUInfo;

typedef struct SituationMemoryInfo {
    uint64_t total_bytes;
    uint64_t available_bytes;
} SituationMemoryInfo;

typedef struct {
    char cpu_name[SITUATION_MAX_CPU_NAME_LEN];
    int cpu_cores;
    float cpu_clock_speed_ghz;
    char gpu_name[SITUATION_MAX_GPU_NAME_LEN];
    uint64_t gpu_dedicated_memory_bytes;            // Primarily via DXGI on Windows
    uint64_t total_ram_bytes;
    uint64_t available_ram_bytes;
    int storage_device_count;
    char storage_device_names[SITUATION_MAX_STORAGE_DEVICES][SITUATION_MAX_DEVICE_NAME_LEN];
    uint64_t storage_capacity_bytes[SITUATION_MAX_STORAGE_DEVICES];
    uint64_t storage_free_bytes[SITUATION_MAX_STORAGE_DEVICES];
    int network_adapter_count;
    char network_adapter_names[SITUATION_MAX_NETWORK_ADAPTERS][SITUATION_MAX_DEVICE_NAME_LEN];
    int input_device_count;
    char input_device_names[SITUATION_MAX_INPUT_DEVICES][SITUATION_MAX_DEVICE_NAME_LEN];
    int display_count;
    char display_names[SITUATION_MAX_MONITORS][SITUATION_MAX_MONITOR_NAME_LEN];
    int display_widths[SITUATION_MAX_MONITORS];
    int display_heights[SITUATION_MAX_MONITORS];
    int display_refresh_rates[SITUATION_MAX_MONITORS];
} SituationDeviceInfo;

/**
 * @brief Physical Display Management Structures
 */
typedef struct {
    int width;
    int height;
    int refresh_rate;
    int color_depth;                                // Can be tricky to get reliably for all modes/APIs
} SituationDisplayMode;

typedef struct {
    char name[SITUATION_MAX_MONITOR_NAME_LEN];      // Win32 device name
    int situation_monitor_id;                       // Internal ID, corresponds to index in cached_physical_displays_array
    GLFWmonitor* glfw_monitor_handle;               // Corresponding GLFW monitor handle, if matched
    bool is_primary;
    SituationDisplayMode current_mode;
    float current_refresh_hz;                       // Fractional nominal refresh (e.g. 59.94); 0 = use current_mode.refresh_rate
    SituationDisplayMode* available_modes;          // Caller must free
    int available_mode_count;
    /** Phase 5 — DXGI advanced color / HDR (Windows; false/0 when probe unavailable). */
    bool     hdr_supported;                         // Panel/path reports HDR capability (10 bpc + extended luminance)
    bool     hdr_enabled;                           // OS "Use HDR" active on this output (PQ2020 color space)
    uint8_t  bits_per_color;                        // DXGI BitsPerColor (8, 10, 12, 16); 0 if unknown
    float    max_luminance_nits;                    // DXGI MaxFullFrameLuminance; 0 if unknown
    uint32_t dxgi_color_space;                      // Raw DXGI_COLOR_SPACE_TYPE; 0 if unknown
    bool     dxgi_metadata_valid;                   // true when IDXGIOutput6::GetDesc1 succeeded for this monitor
} SituationDisplayInfo;

/**
 * @brief Defines standard system cursor shapes.
 */
typedef enum {
    SIT_CURSOR_DEFAULT = 0,                         // The default, platform-specific arrow
    SIT_CURSOR_ARROW,                               // A standard arrow cursor
    SIT_CURSOR_IBEAM,                               // The text input I-beam
    SIT_CURSOR_CROSSHAIR,                           // A crosshair for targeting
    SIT_CURSOR_HAND,                                // A pointing hand, for links or buttons
    SIT_CURSOR_HRESIZE,                             // Horizontal resize arrow (e.g., <->)
    SIT_CURSOR_VRESIZE                              // Vertical resize arrow (e.g., ^ v)
} SituationCursor;

typedef struct SituationPackedFont {
    int char_width;
    int char_height;
    int display_height;
    int char_count;
    int first_char;
    int chars_per_row;
    int bits_per_row;
    int data_bits;
    int data_bit_offset;
    bool bit_order_msb_first;
    int top_padding;
    int bottom_padding;
    int left_padding;
    int right_padding;
    int atlas_chars_per_row;
    int atlas_chars_per_col;
    bool enable_outline;
    int outline_thickness;
    unsigned char outline_r, outline_g, outline_b, outline_a;
    unsigned char font_r, font_g, font_b, font_a;
} SituationPackedFont;

typedef struct SituationFont {
    void *fontData;                                 // The raw data buffer of the .ttf file
    void *stbFontInfo;                              // A pointer to the stbtt_fontinfo struct

    // [NEW] GPU-side data for real-time rendering
    SituationTexture atlas_texture;
    void* glyph_info; // Pointer to stbtt_bakedchar array
    int atlas_width;
    int atlas_height;
    float font_height_pixels; // The size this atlas was baked at

    // [v2.3.38] Bitmap Font Support
    bool is_bitmap;
    const unsigned char* bitmap_data;
    int bitmap_width;   // Width of one character (e.g. 8)
    int bitmap_height;  // Height of one character (e.g. 8)
    int bitmap_count;   // Number of characters (e.g. 256)

    /* Grid atlas layout (bitmap / terminal fonts after GPU bake or builder) */
    int first_char;
    int chars_per_row;
    int chars_per_col;
    int display_cell_width;
    int display_cell_height;
    float char_spacing;
    float line_spacing;
} SituationFont;
//==================================================================================
// Core Module: Application Lifecycle and System  [Main thread] [GL+VK]
//==================================================================================

// --- Application Lifecycle & State ---
SITAPI const char* SituationGetVersionString(void); 									// [Main thread] Returns a read-only static string (e.g., "2.4.336"). Do not free.
SITAPI SituationError SituationInit(int argc, char** argv, const SituationInitInfo* init_info); // [Main thread] Initialize the library, create window and graphics context.
SITAPI void SituationPollInputEvents(void);                                             // [Main thread] Poll for all input events (keyboard, mouse, joystick). Call once per frame.
SITAPI void SituationUpdateTimers(void);                                                // [Main thread] Update all internal timers (frame timer, temporal system). Call after polling events.
SITAPI void SituationShutdown(void);                                                    // [Main thread] Shut down the library and release all resources.
SITAPI bool SituationIsInitialized(void);                                               // [Main thread] Check if the library has been successfully initialized.

SITAPI SituationInitState SituationGetInitState(void);                                  // Query the current initialization state (thread-safe).

SITAPI bool SituationWindowShouldClose(void);                                           // Check if the application should close (e.g., user clicked X).
SITAPI void SituationPauseApp(void);                                                    // Pause the application's internal state (e.g., audio).
SITAPI void SituationResumeApp(void);                                                   // Resume a paused application.
SITAPI bool SituationIsAppPaused(void);                                                 // Check if the application is currently paused.

// --- Frame Timing & FPS Management ---
SITAPI void SituationSetTargetFPS(int fps);                                             // Set a desired frame rate cap (0 for uncapped).
SITAPI float SituationGetFrameTime(void);                                               // Get the time in seconds for the last frame to complete (deltaTime).
SITAPI int SituationGetFPS(void);                                                       // Get the current frames-per-second value.

// --- Callbacks and Event Handling ---
SITAPI SituationError SituationGetLastErrorMsg(char** out_msg);                         // Get the last error message as a string (caller must free).
SITAPI SituationError SituationGetLastErrorCode(void);                                  // Get the SituationError enum from the most recent _SituationSetErrorFromCode call.
SITAPI const char* SituationErrorToString(SituationError err);                          // Human-readable base label for an error code (from the errno table).
SITAPI void SituationSetExitCallback(void (*callback)(void* user_data), void* user_data); // Set a callback to run just before shutdown.
SITAPI void SituationSetResizeCallback(void (*callback)(int width, int height, void* user_data), void* user_data); // Set a callback for window framebuffer resize events.
SITAPI void SituationSetFocusCallback(SituationFocusCallback callback, void* user_data); // Set a callback for window focus events.
SITAPI void SituationSetMaximizeCallback(SituationMaximizeCallback callback, void* user_data); // Set a callback for window maximize / restore events.
SITAPI void SituationSetFileDropCallback(SituationFileDropCallback callback, void* user_data); // Set a callback for file drop events.

// --- Command-Line Argument Queries ---
SITAPI bool SituationIsArgumentPresent(const char* arg_name);                           // Check if a command-line argument (e.g., "-server") was provided.
SITAPI const char* SituationGetArgumentValue(const char* arg_name);                     // Get the value of an argument (e.g., "jungle" from "-level:jungle").

// --- System & Hardware Information (split queries; v2.4.207) ---
SITAPI void SituationGetCPUInfo(SituationCPUInfo* out);                                 // CPU name, core/thread counts, and clock speed.
SITAPI void SituationGetGPUInfo(SituationGPUInfo* out);                                 // GPU name and dedicated VRAM (when available).
SITAPI void SituationGetMemoryInfo(SituationMemoryInfo* out);                           // Total and available physical RAM.
SITAPI int SituationGetStorageDeviceCount(void);                                        // Number of storage volumes reported by the OS.
SITAPI bool SituationGetStorageDevice(int index, char* out_name, int name_len, uint64_t* out_capacity_bytes, uint64_t* out_free_bytes);
SITAPI int SituationGetNetworkAdapterCount(void);                                       // Number of network adapters reported by the OS.
SITAPI bool SituationGetNetworkAdapterName(int index, char* out_name, int name_len);
SITAPI int SituationGetInputDeviceCount(void);                                          // Number of input devices (keyboard/mouse/gamepad).
SITAPI bool SituationGetInputDeviceName(int index, char* out_name, int name_len);
SITAPI const char* SituationGetGPUName(void);                                           // Get the name of the active GPU.

// --- OS Information (v2.4.199) ---
SITAPI SituationOSInfo SituationGetOSInfo(void);                                        // Get operating system name, version, and build number.

// --- Process Enumeration (v2.4.199) ---
SITAPI SituationProcessInfo* SituationGetProcessList(int* out_count);                   // Get snapshot of running OS processes. Caller must free with SituationFreeProcessList().
SITAPI void SituationFreeProcessList(SituationProcessInfo* list, int count);            // Free a process list returned by SituationGetProcessList().

// --- Active Audio Device Query (v2.4.199) ---
SITAPI const char* SituationGetActiveAudioDeviceName(void);                             // Get the name of the currently active playback device (static buffer, do not free).

/** Canonical backend query (valid before and after SituationInit). Pair with SituationGetGraphicsCaps(). */
SITAPI SituationGraphicsBackend SituationGetGraphicsBackend(void);
/** Read-only label for SituationGetGraphicsBackend() ("OpenGL", "Vulkan", "Unknown"). */
SITAPI const char* SituationGetGraphicsBackendName(void);

SITAPI void SituationGetGraphicsCaps(SituationGraphicsCaps* out_caps);                  // Get backend capabilities for examples/frameworks.
SITAPI char* SituationGetUserDirectory(void);                                           // [Caller frees] Get the full path to the current user's home directory.
#if defined(_WIN32)
SITAPI char SituationGetCurrentDriveLetter(void);                                       // Get the drive letter of the running executable (Windows only).
SITAPI bool SituationGetDriveInfo(char drive_letter, uint64_t* out_total_capacity_bytes, uint64_t* out_free_space_bytes, char* out_volume_name, int volume_name_len); // Get info for a specific drive (Windows only).
SITAPI SituationError SituationWin32SetAppUserModelId(const char* app_id_utf8);         // Set shell AppUserModelID before first window (UTF-8); idempotent per process.
#endif // _WIN32

SITAPI void SituationOpenFile(const char* filePath);                                    // Open a file or folder with its default application.
SITAPI int SituationExecuteCommand(const char *cmd, char **output);                     // Execute a shell command hidden, return exit code & combined output.

//==================================================================================
// Window and Display Module
//==================================================================================
// --- Window State Management ---
SITAPI void SituationSetWindowState(uint32_t flags);                                    // Set window configuration state using flags (additive).
SITAPI void SituationClearWindowState(uint32_t flags);                                  // Clear window configuration state flags.
SITAPI void SituationSetVSync(bool enable);                                             // Enable or disable VSync (vertical synchronization).
SITAPI void SituationToggleFullscreen(void);                                            // Toggle window between fullscreen and windowed mode.
SITAPI void SituationToggleBorderlessWindowed(void);                                    // Toggle window between borderless and decorated mode.
SITAPI void SituationMaximizeWindow(void);                                              // Maximize the window if it's resizable.
SITAPI void SituationMinimizeWindow(void);                                              // Minimize the window (iconify).
SITAPI void SituationRestoreWindow(void);                                               // Restore a minimized or maximized window.
SITAPI void SituationSetWindowFocused(void);                                            // Set the window to be focused.

// --- Window Property Management ---
SITAPI void SituationSetWindowTitle(const char *title);                                 // Set the title for the window.
SITAPI void SituationSetWindowIcon(SituationImage image);                               // Set the icon for the window (single image).
SITAPI void SituationSetWindowIcons(SituationImage *images, int count);                 // Set the icon for the window (multiple sizes).
SITAPI void SituationSetWindowPosition(int x, int y);                                   // Set the window position on the screen.
SITAPI void SituationSetWindowSize(int width, int height);                              // Set the window dimensions.
SITAPI void SituationSetWindowMinSize(int width, int height);                           // Set the window minimum dimensions.
SITAPI void SituationSetWindowMaxSize(int width, int height);                           // Set the window maximum dimensions.
SITAPI void SituationSetWindowOpacity(float opacity);                                   // Set window opacity [0.0f to 1.0f].

// --- Window State Queries ---
SITAPI bool SituationIsWindowState(uint32_t flag);                                      // Check if a specific window state flag is set.
SITAPI bool SituationIsWindowFullscreen(void);                                          // Check if the window is currently in fullscreen mode.
SITAPI bool SituationIsWindowHidden(void);                                              // Check if the window is currently hidden.
SITAPI bool SituationIsWindowMinimized(void);                                           // Check if the window is currently minimized.
SITAPI bool SituationIsWindowMaximized(void);                                           // Check if the window is currently maximized.
SITAPI bool SituationHasWindowFocus(void);                                              // Check if the window is currently focused.
SITAPI bool SituationIsWindowResized(void);                                             // Check if the window was resized in the last frame.

// --- Window & Screen Dimension Queries ---
SITAPI int SituationGetScreenWidth(void);                                               // Get the current logical width of the window.
SITAPI int SituationGetScreenHeight(void);                                              // Get the current logical height of the window.
SITAPI int SituationGetRenderWidth(void);                                               // Get the current render width (backbuffer size, considers HiDPI).
SITAPI int SituationGetRenderHeight(void);                                              // Get the current render height (backbuffer size, considers HiDPI).
SITAPI void SituationGetWindowSize(int* width, int* height);                            // Get the current logical window size.
SITAPI Vector2 SituationGetWindowPosition(void);                                        // Get the window's top-left position on the screen.
SITAPI Vector2 SituationGetWindowScaleDPI(void);                                        // Get the DPI scaling factor for the window.

// --- Physical Display (Monitor) Management ---
SITAPI int SituationGetMonitorCount(void);                                              // Get the number of connected monitors.
SITAPI int SituationGetCurrentMonitor(void);                                            // Get the index of the monitor the window is on.
SITAPI SituationError SituationGetDisplays(SituationDisplayInfo** out_displays, int* out_count); // Get information for all displays (caller must free).
SITAPI void SituationFreeDisplays(SituationDisplayInfo* displays, int count);            // Free a display info array returned by SituationGetDisplays.
SITAPI void SituationRefreshDisplays(void);                                             // Force a refresh of the cached display information.
SITAPI SituationError SituationSetDisplayMode(int monitor_id, const SituationDisplayMode* mode, bool fullscreen); // Set the display mode for a monitor.
SITAPI void SituationSetWindowMonitor(int monitor_id);                                  // Set the window to be fullscreen on a specific monitor.
SITAPI const char* SituationGetMonitorName(int monitor_id);                             // Get the human-readable name of a monitor.
SITAPI int SituationGetMonitorWidth(int monitor_id);                                    // Get the width of a monitor's current video mode.
SITAPI int SituationGetMonitorHeight(int monitor_id);                                   // Get the height of a monitor's current video mode.
SITAPI int SituationGetMonitorPhysicalWidth(int monitor_id);                            // Get the physical width of a monitor in millimeters.
SITAPI int SituationGetMonitorPhysicalHeight(int monitor_id);                           // Get the physical height of a monitor in millimeters.
SITAPI int SituationGetMonitorRefreshRate(int monitor_id);                              // Get the refresh rate of a monitor (integer Hz from OS/GLFW).
SITAPI int SituationGetDisplayRefreshRate(void);                                        // Primary display refresh rate (monitor 0, integer Hz).
SITAPI float SituationGetMonitorRefreshRateHz(int monitor_id);                          // Fractional nominal refresh (DXGI rational when available).
SITAPI float SituationGetDisplayRefreshRateHz(void);                                    // Primary display fractional nominal refresh (monitor 0).
SITAPI float SituationGetMeasuredPresentRateHz(void);                                   // Measured present rate from latest present interval (0 if unknown).
SITAPI Vector2 SituationGetMonitorPosition(int monitor_id);                             // Get the top-left position of a monitor on the desktop.

// --- Cursor, Clipboard and File Drops ---
SITAPI void SituationSetCursor(SituationCursor cursor);                                 // Set the mouse cursor to a standard shape.
SITAPI void SituationShowCursor(void);                                                  // Show the mouse cursor.
SITAPI void SituationHideCursor(void);                                                  // Hide the mouse cursor.
SITAPI void SituationDisableCursor(void);                                               // Hide and lock the cursor, providing raw mouse motion.
SITAPI SituationError SituationGetClipboardText(const char** out_text);                 // Get text from the system clipboard.
SITAPI SituationError SituationSetClipboardText(const char* text);                      // Set text in the system clipboard.
SITAPI bool SituationIsFileDropped(void);                                               // Check if a file was dropped into the window this frame.
SITAPI char** SituationLoadDroppedFiles(int* count);                                    // Get the paths of dropped files (returns a copy, caller must free).
SITAPI void SituationUnloadDroppedFiles(char** paths, int count);                       // Unload the file path list returned by SituationLoadDroppedFiles.

// --- Advanced Window Profile Management ---
SITAPI SituationError SituationSetWindowStateProfiles(uint32_t active_flags, uint32_t inactive_flags); // Set the flag profiles for when the window is focused vs. unfocused.
SITAPI SituationError SituationApplyCurrentProfileWindowState(void);                    // Manually apply the appropriate window state profile based on current focus.
SITAPI SituationError SituationToggleWindowStateFlags(SituationWindowStateFlags flags_to_toggle); // Toggle flags in the current profile and apply the result.
SITAPI uint32_t SituationGetCurrentActualWindowStateFlags(void);                        // Gets flags based on current GLFW window state

//==================================================================================
// Image Module: CPU-side Image and Font Loading and Manipulation
//==================================================================================
// --- Image Loading and Unloading ---
SITAPI SituationError SituationLoadImage(const char *fileName, SituationImage* out_image);                         // Load an image via stb_image (JPEG, PNG, BMP, TGA, PSD, GIF, HDR, PIC, PNM).
SITAPI SituationError SituationLoadImageFromMemory(const char *fileType, const unsigned char *fileData, int dataSize, SituationImage* out_image); // Load an image from a memory buffer.
SITAPI void SituationUnloadImage(SituationImage image);                                 // Unload an image's pixel data from memory.
SITAPI bool SituationIsImageValid(SituationImage image);                                // Check if an image has been loaded successfully.
SITAPI bool SituationIsStbImageLoadExtension(const char* extension);                     // True for stb_image decode extensions (.jpg, .png, .bmp, .tga, .psd, .gif, .hdr, .pic, .ppm, .pgm, .pnm).

// --- Image Exporting ---
SITAPI SituationError SituationExportImage(SituationImage image, const char *fileName);           // Export image data (.png, .bmp, .jpg, .tga, .hdr).

// --- Image Generation & Copying ---
SITAPI SituationError SituationImageCopy(SituationImage image, SituationImage* out_image);                         // Create a new image by copying another.

SITAPI SituationError SituationCreateImage(int width, int height, int channels, SituationImage* out_image);        // Allocates a new SituationImage container with UNINITIALIZED data.
SITAPI void SituationSetPixelColor(SituationImage *img, int x, int y, ColorRGBA col);   // Helper to set a specific pixel color (CPU-side).
SITAPI void SituationBlitRawDataToImage(SituationImage *dst, const void* data, int x, int y, int width, int height, int src_channels); // Copies raw byte data into a specific region of an image.

SITAPI void SituationImageDraw(SituationImage *dst, SituationImage src, SitRectangle srcRect, Vector2 dstPos); // Copying portion of one image into another image at destination placement
SITAPI void SituationImageDrawAlpha(SituationImage *dst, SituationImage src, SitRectangle srcRect, Vector2 dstPos, ColorRGBA tint); // Draw a portion of a source image onto dst with alpha tinting.
SITAPI SituationError SituationGenImageColor(int width, int height, ColorRGBA color, SituationImage* out_image);   // Generate a new image of a solid color.
SITAPI SituationError SituationGenImageGradient(int width, int height, ColorRGBA tl, ColorRGBA tr, ColorRGBA bl, ColorRGBA br, SituationImage* out_image); // Generate a new image with a gradient.

// --- Image Manipulation (Modifies image in-place) ---
SITAPI void SituationImageCrop(SituationImage *image, SitRectangle crop);                  // Crop an image to a specific rectangle.
SITAPI void SituationImageResize(SituationImage *image, int newWidth, int newHeight);   // Resize an image using default bicubic scaling.
SITAPI void SituationImageFlip(SituationImage *image, SituationImageFlipMode mode);     // Flip an image.
SITAPI void SituationImageAdjustHSV(SituationImage *image, float hue_shift, float sat_factor, float val_factor, float mix);   // Control an image by Hue Saturation and Brightness.
SITAPI void SituationImageAdjustYPQ(SituationImage *image, float phase_shift_deg, float chroma_factor, float luma_factor, float mix); // Grade an image in YPQ (phase/chroma/luma).

// --- Font Management ---
SITAPI SituationError SituationLoadFont(const char *fileName, SituationFont* out_font);                         // Load a font from a TTF/OTF file for CPU rendering.
SITAPI SituationError SituationLoadFontFromMemory(const void* data, int dataSize, SituationFont* out_font);		// Loads a font directly from a memory buffer (e.g., embedded resource).
SITAPI SituationError SituationLoadBitmapFontFromMemory(const unsigned char* data, int char_width, int char_height, int num_chars, SituationFont* out_font); // Loads a raw bitmap font (e.g. 8x8 array).
SITAPI SituationError SituationBakeFontAtlas(SituationFont* font, float fontSizePixels); // Rasterize a font into a GPU-ready atlas at the given size.
SITAPI SituationError SituationBakeBitmapFontAtlas(SituationFont* font);                  // Upload bitmap_data as a NEAREST-filtered grid atlas for GPU text.
SITAPI SituationError SituationLoadBitmapFontFromTexture(SituationTexture sheet, int char_width, int char_height, int first_char, SituationFont* out_font); // Grid atlas already on GPU — fills layout metadata.
SITAPI void SituationUnloadFont(SituationFont font);                                    // Frees CPU font data, glyph metrics, and owned GPU atlas (not the built-in default).
SITAPI SitRectangle SituationMeasureText(SituationFont font, const char *text, float fontSize); // Measure the pixel dimensions of a string before drawing.
SITAPI SitRectangle SituationMeasureTextEx(SituationFont font, const char *text, float fontSize, float spacing); // Measure with extra per-character spacing.
SITAPI int SituationGetTextLineCount(SituationFont font, const char *text, float max_width); // Lines required for width constraint (grid/TTF).
SITAPI SituationError SituationCreateTerminalFontFromMemory(const unsigned char* data, int char_width, int char_height, int char_count, int chars_per_row, int first_char, SituationFont* out_font);
SITAPI SituationError SituationCreateTerminalFontEx(const unsigned char* data, int char_width, int char_height, int char_count, int chars_per_row, int first_char, float char_spacing, float line_spacing, SituationFont* out_font);
SITAPI SituationError SituationCreateCP437Font(const unsigned char* font_data_8x16, SituationFont* out_font);
SITAPI SituationError SituationCreateASCIIFont(const unsigned char* data, int cw, int ch, SituationFont* out_font);
SITAPI SituationError SituationCreatePackedBitmapFont(const void* packed_data, const SituationPackedFont* config, SituationFont* out_font);
SITAPI SituationError SituationCreateOutlinedPackedBitmapFont(const void* packed_data, const SituationPackedFont* config, SituationFont* out_font);
SITAPI SituationError SituationCreateVCRFont(const uint16_t* font_data, SituationFont* out_font);
SITAPI SituationError SituationCreateVCRFontWithOutline(const uint16_t* data, int outline_thickness, SituationFont* out_font);
SITAPI SituationError SituationCreateVGA8x8Font(const unsigned char* data, SituationFont* out_font);
SITAPI SituationError SituationCreateVGA8x8FontWithOutline(const unsigned char* data, int outline_thickness, SituationFont* out_font);
SITAPI SituationError SituationImageStampText(SituationImage* dst, SituationFont font, const char* text, Vector2 pos, float fontSize, ColorRGBA text_color, ColorRGBA bg_color);
SITAPI SituationError SituationImageStampTextBoxed(SituationImage* dst, SituationFont font, const char* text, SitRectangle bounds, float fontSize, ColorRGBA text_color, ColorRGBA bg_color, bool word_wrap, int* out_width, int* out_height);
SITAPI void SituationImageDrawCodepoint(SituationImage *dst, SituationFont font, int codepoint, Vector2 position, float fontSize, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness); // Draw a single Unicode character with advanced styling onto an image.
SITAPI void SituationImageDrawText(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, ColorRGBA tint ); // Draw a simple, tinted text string onto an image.
SITAPI void SituationImageDrawTextEx(SituationImage *dst, SituationFont font, const char *text, Vector2 position, float fontSize, float spacing, float rotationDegrees, float skewFactor, ColorRGBA fillColor, ColorRGBA outlineColor, float outlineThickness); // Draw a text string with advanced styling (rotation, outline) onto an image.
SITAPI void SituationImageDrawTextFormatted(SituationImage *dst, SituationFont font, Vector2 position, float fontSize, float spacing, ColorRGBA tint, const char* fmt, ...); // Draw printf-style formatted text onto an image.

//==================================================================================
// Input Module: Keyboard, Mouse, and Gamepad
//==================================================================================
// --- Keyboard Input ---
SITAPI int SituationGetCharFromScancode(int window, int scancode, int mods, uint32_t* out_char); // Maps a physical key scancode (plus modifiers) to a Unicode character, respecting the current OS keyboard layout.
SITAPI bool SituationIsKeyDown(int key);                                                // Check if a key is currently held down (a state).
SITAPI bool SituationIsKeyUp(int key);                                                  // Check if a key is currently up (a state).
SITAPI bool SituationIsKeyPressed(int key);                                             // Check if a key was pressed down this frame (an event).
SITAPI bool SituationIsKeyReleased(int key);                                            // Check if a key was released this frame (an event).
SITAPI void SituationConsumeKeyPress(int key);                                          // Eat a key press this frame so later IsKeyPressed() returns false (e.g. for global hotkeys to take priority over app controls).
SITAPI bool SituationIsScancodeDown(int scancode);                                      // Check if a physical key (scancode) is currently held down.
SITAPI int SituationGetKeyScancode(int key);                                            // Get the platform-specific scancode for a logical key.
SITAPI int SituationGetKeyPressed(void);                                                // Get the next key from the press queue (no repeats).
SITAPI int SituationGetKeyPressedEx(int* out_scancode);                                 // Get the next key and its scancode from the queue.
SITAPI int SituationPeekKeyPressed(void);                                               // Peek at the next key in the press queue without consuming it.
SITAPI int SituationPeekKeyPressedEx(int* out_scancode);                                // Peek at the next key and its scancode.
SITAPI unsigned int SituationGetCharPressed(void);                                      // Get the next character from the text input queue.
SITAPI bool SituationIsLockKeyPressed(int lock_key_mod);                                // Check if a lock key (Caps, Num) is currently active.
SITAPI bool SituationIsScrollLockOn(void);                                              // Check if Scroll Lock is currently toggled on.
SITAPI bool SituationIsModifierPressed(int modifier);                                   // Check if a modifier key (Shift, Ctrl, Alt) is pressed.
SITAPI void SituationSetKeyCallback(SituationKeyCallback callback, void* user_data);    // Set a callback for key events.

// --- Mouse Input ---
SITAPI Vector2 SituationGetMousePosition(void);                                         // Get the mouse position within the window.
SITAPI Vector2 SituationGetMouseDelta(void);                                            // Get the mouse movement since the last frame.
SITAPI float SituationGetMouseWheelMove(void);                                          // Get vertical mouse wheel movement.
SITAPI Vector2 SituationGetMouseWheelMoveV(void);                                       // Get vertical and horizontal mouse wheel movement.
SITAPI bool SituationIsMouseButtonDown(int button);                                     // Check if a mouse button is currently held down (a state).
SITAPI bool SituationIsMouseButtonPressed(int button);                                  // Check if a mouse button was pressed down this frame (an event).
SITAPI bool SituationIsMouseButtonReleased(int button);                                 // Check if a mouse button was released this frame.
SITAPI void SituationSetMousePosition(Vector2 pos);                                     // Set the mouse position within the window.
SITAPI void SituationSetMouseOffset(Vector2 offset);                                    // Set a software offset for the mouse position.
SITAPI void SituationSetMouseScale(Vector2 scale);                                      // Set a software scale for the mouse position and delta.
SITAPI void SituationSetMouseButtonCallback(SituationMouseButtonCallback callback, void* user_data); // Set a callback for mouse button events.
SITAPI void SituationSetCursorPosCallback(SituationCursorPosCallback callback, void* user_data); // Set a callback for mouse movement events.
SITAPI void SituationSetScrollCallback(SituationScrollCallback callback, void* user_data); // Set a callback for mouse scroll events.

// --- Gamepad Input ---
SITAPI bool SituationIsJoystickPresent(int jid);                                        // Check if a joystick/gamepad is connected.
SITAPI bool SituationIsGamepad(int jid);                                                // Check if a connected joystick has a standard gamepad mapping.
SITAPI const char* SituationGetJoystickName(int jid);                                   // Get the human-readable name of a joystick/gamepad.
SITAPI void SituationSetJoystickCallback(SituationJoystickCallback callback, void* user_data); // Set a callback for joystick connection events.
SITAPI int SituationSetGamepadMappings(const char *mappings);                           // Load a new set of gamepad mappings from a string.
SITAPI int SituationGetGamepadButtonPressed(void);                                      // Get the next gamepad button from the press queue.
SITAPI bool SituationIsGamepadButtonDown(int jid, int button);                          // Check if a gamepad button is currently held down (a state).
SITAPI bool SituationIsGamepadButtonPressed(int jid, int button);                       // Check if a gamepad button was pressed down this frame (an event).
SITAPI bool SituationIsGamepadButtonReleased(int jid, int button);                      // Check if a gamepad button was released this frame (an event).
SITAPI int SituationGetGamepadAxisCount(int jid);                                       // Get the number of axes for a gamepad.
SITAPI float SituationGetGamepadAxisValue(int jid, int axis);                           // Get the value of a gamepad axis (deadzone applied).
SITAPI bool SituationSetGamepadVibration(int jid, float left_motor, float right_motor); // Set gamepad vibration/rumble (Windows only).

//==================================================================================
// Logging Module
//==================================================================================

SITAPI void SituationLog(int msgType, const char* text, ...);                           // Log a message at the specified level (SIT_LOG_*).
SITAPI void SituationSetTraceLogLevel(int logType);                                     // Set the minimum log level for output filtering.
SITAPI void SituationSetLogCallback(void (*callback)(SituationLogLevel level, const char* message, void* user), void* user); // Set a custom log callback.
SITAPI void SituationShowMessageBox(const char* title, const char* message);            // Blocking UI message box; for fatal init errors.
SITAPI void SituationLogWarning(SituationError code, const char* fmt, ...);             // Log a warning with an associated error code (debug builds only).
#define SITUATION_LOG_WARNING SituationLogWarning
#endif /* SITUATION_API_PLATFORM_H */
