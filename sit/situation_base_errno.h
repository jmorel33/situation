/**
 * @brief SituationError — X-Macro errno table (single source of truth).
 *
 * FORMAT: X(NAME, VALUE, MESSAGE)
 *
 * Values are permanent (never renumber). New codes use gaps in the section range.
 *
 * EOL: Entries marked `EOL:` are retained for ABI/docs; prefer the named successor
 *      when adding call sites. A future sanitisation pass may redirect callers and
 *      remove duplicate names (same value aliases in SITUATION_ERRORS_COMPAT).
 *
 * @see scripts/audit_errno.ps1 — table vs codebase usage report
 */

#ifndef SITUATION_BASE_ERRNO_H
#define SITUATION_BASE_ERRNO_H

//==================================================================================
//  SituationError — X-Macro Error Table (Single Source of Truth)
//==================================================================================
//
//  Ranges:
//    0        → Success
//   -1 to -99 → Core, System & Threading
// -100 to -199→ Platform, Windowing & Input
// -200 to -299→ Display System
// -300 to -399→ Filesystem, Assets & Plugins
// -400 to -499→ Audio Subsystem
// -500 to -599→ Resource Management, Rendering Core, Fonts & Image
// -600 to -699→ OpenGL Backend
// -700 to -799→ Vulkan Backend
// -800 to -899→ Compute / GPGPU
// -900 to -949→ Network
// -999        → Unknown / Catch-All
//

// ── Core & System Errors (0 to -99) ─────────────────────────────────────────────
#define SITUATION_ERRORS_CORE(X) \
    X(SITUATION_SUCCESS,                             0, "No error") \
    X(SITUATION_ERROR_GENERAL,                      -1, "A general error occurred") \
    X(SITUATION_ERROR_NOT_IMPLEMENTED,              -2, "Feature not implemented on current backend") \
    X(SITUATION_ERROR_NOT_INITIALIZED,              -3, "API called before SituationInit()") \
    X(SITUATION_ERROR_ALREADY_INITIALIZED,          -4, "SituationInit() called more than once") \
    X(SITUATION_ERROR_INIT_FAILED,                  -5, "Core initialization sequence failed") \
    X(SITUATION_ERROR_SHUTDOWN_FAILED,              -6, "Resources still alive or backend refused cleanup") \
    X(SITUATION_ERROR_INVALID_PARAM,                -7, "Invalid parameter (NULL, out-of-range, bad enum)") \
    X(SITUATION_ERROR_MEMORY_ALLOCATION,            -8, "Memory allocation failed") \
    X(SITUATION_ERROR_INTERNAL_STATE_CORRUPTED,     -9, "Internal invariant violated -- fatal bug") \
    X(SITUATION_ERROR_ASSERTION_FAILED,            -10, "Debug assertion tripped") \
    X(SITUATION_ERROR_UPDATE_AFTER_DRAW_VIOLATION, -11, "Architectural rule broken: Update called after Draw") \
    X(SITUATION_ERROR_MEMORY_ACCESS,               -12, "Invalid or unmapped memory access") \
    X(SITUATION_ERROR_TIMER_SYSTEM,                -20, "An error occurred within the internal timer/oscillator system")

// ── Threading Errors (-80 to -99) ───────────────────────────────────────────────
#define SITUATION_ERRORS_THREADING(X) \
    X(SITUATION_ERROR_THREAD_QUEUE_FULL,            -80, "Thread queue full") \
    X(SITUATION_ERROR_THREAD_VIOLATION,             -81, "Main-thread-only function called from worker thread") \
    X(SITUATION_ERROR_THREAD_CYCLE,                 -82, "Dependency cycle or depth limit exceeded") \
    X(SITUATION_ERROR_THREAD_CREATION_FAILED,       -83, "Failed to spawn a new thread (thrd_create)") \
    X(SITUATION_ERROR_THREAD_MUTEX_INIT_FAILED,     -84, "Mutex initialization failed (mtx_init)") \
    X(SITUATION_ERROR_THREAD_MUTEX_LOCK_FAILED,     -85, "Mutex lock operation failed (mtx_lock)") \
    X(SITUATION_ERROR_THREAD_MUTEX_UNLOCK_FAILED,   -86, "Mutex unlock operation failed (mtx_unlock)") \
    X(SITUATION_ERROR_THREAD_MUTEX_TIMEOUT,         -87, "Mutex lock timeout (deadlock prevention)") \
    X(SITUATION_ERROR_THREAD_JOIN_FAILED,           -88, "Thread join operation failed (thrd_join)") \
    X(SITUATION_ERROR_THREAD_DETACH_FAILED,         -89, "Thread detach operation failed (thrd_detach)") \
    X(SITUATION_ERROR_THREAD_NOT_AVAILABLE,         -90, "Threading not available on this platform") \
    X(SITUATION_ERROR_THREAD_ATOMIC_FAILED,         -91, "Atomic operation failed or not supported") \
    X(SITUATION_ERROR_THREAD_STATE_INVALID,         -92, "Invalid thread state for requested operation") \
    X(SITUATION_ERROR_THREAD_BUFFER_OVERFLOW,       -93, "Thread-local buffer overflow") \
    X(SITUATION_ERROR_THREAD_DEADLOCK_DETECTED,     -94, "Potential deadlock detected") \
    X(SITUATION_ERROR_RENDER_BACKPRESSURE_TIMEOUT,  -95, "Render thread join timeout") \
    X(SITUATION_ERROR_RENDER_LIST_INCOMPLETE,       -96, "Render list missing mandatory commands") \
    X(SITUATION_ERROR_ARM_INTRINSICS_FAILED,        -97, "ARM-specific WFE/SEV intrinsic failure") \
    X(SITUATION_ERROR_COMMAND_EXECUTION_FAILED,     -98, "External system command execution failed") \
    X(SITUATION_ERROR_THREAD_JOB_LOST,              -99, "Job slot settled in queue but the job function never ran (scheduler defect)")

// ── Platform & Windowing (-100 to -199) ─────────────────────────────────────────
#define SITUATION_ERRORS_PLATFORM(X) \
    X(SITUATION_ERROR_GLFW_FAILED,                 -100, "GLFW operation failed") \
    X(SITUATION_ERROR_WINDOW_CREATION_FAILED,      -101, "Failed to create the application window") \
    X(SITUATION_ERROR_WINDOW_FOCUS_FAILED,         -102, "Window focus/minimize/restore operation failed") \
    X(SITUATION_ERROR_CLIPBOARD_FAILED,            -103, "Clipboard operation failed") \
    X(SITUATION_ERROR_CURSOR_CREATION_FAILED,      -104, "Custom cursor creation failed") \
    X(SITUATION_ERROR_WINDOW_STATE_FAILED,         -105, "Window state change failed (GLFW rejected the operation)") \
    X(SITUATION_ERROR_WINDOW_PROPERTY_FAILED,      -106, "Window property set failed (title, size, position, opacity, icon)") \
    X(SITUATION_ERROR_APP_STATE_FAILED,            -107, "Application state transition failed (pause/resume/target FPS)") \
    X(SITUATION_ERROR_COM_INITIALIZATION_FAILED,   -110, "COM initialization failed (Windows)") /* EOL: prefer SITUATION_ERROR_COM_FAILED */ \
    X(SITUATION_ERROR_DXGI_QUERY_FAILED,           -111, "DXGI GPU query failed (Windows)") /* EOL: prefer SITUATION_ERROR_DXGI_FAILED */ \
    X(SITUATION_ERROR_WINDOW_FOCUS,                -120, "An operation related to window focus failed") /* EOL: prefer SITUATION_ERROR_WINDOW_FOCUS_FAILED */ \
    X(SITUATION_ERROR_DEVICE_QUERY,                -121, "Failed to query system hardware or device information") \
    X(SITUATION_ERROR_COM_FAILED,                  -123, "[Win32] Failed to initialize the COM library") \
    X(SITUATION_ERROR_DXGI_FAILED,                 -124, "[Win32] A call to the DXGI library failed")

// ── Input & HID (-130 to -149) ──────────────────────────────────────────────────
#define SITUATION_ERRORS_INPUT(X) \
    X(SITUATION_ERROR_INPUT_DEVICE_DISCONNECTED,    -130, "Input device disconnected during operation") \
    X(SITUATION_ERROR_INPUT_MAPPING_INVALID,        -131, "Invalid input mapping or layout definition") \
    X(SITUATION_ERROR_INPUT_HAPTIC_FAILED,          -132, "Failed to initialize or play haptic feedback")

// ── Display & Virtual Display (-200 to -299) ────────────────────────────────────
#define SITUATION_ERRORS_DISPLAY(X) \
    X(SITUATION_ERROR_DISPLAY_QUERY,                    -200, "Failed to query physical monitor information") /* EOL: prefer SITUATION_ERROR_DISPLAY_QUERY_FAILED */ \
    X(SITUATION_ERROR_DISPLAY_SET,                      -201, "Failed to set a display mode on a physical monitor") /* EOL: prefer SITUATION_ERROR_DISPLAY_MODE_SET_FAILED */ \
    X(SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT,            -202, "The maximum number of virtual displays has been reached") /* EOL: prefer SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT_REACHED */ \
    X(SITUATION_ERROR_VIRTUAL_DISPLAY_INVALID_ID,       -203, "Invalid virtual display ID supplied") \
    X(SITUATION_ERROR_DISPLAY_QUERY_FAILED,             -210, "Display query failed (glfwGetMonitors)") \
    X(SITUATION_ERROR_DISPLAY_MODE_UNSUPPORTED,         -211, "Requested resolution/refresh rate not available") \
    X(SITUATION_ERROR_DISPLAY_MODE_SET_FAILED,          -212, "Failed to apply fullscreen mode") \
    X(SITUATION_ERROR_VIRTUAL_DISPLAY_LIMIT_REACHED,    -213, "Max virtual displays (32) already created") \
    X(SITUATION_ERROR_VIRTUAL_DISPLAY_NOT_FOUND,        -214, "Virtual display ID not found in active list")

// ── Filesystem & Hot-Reloading (-300 to -329) ───────────────────────────────────
#define SITUATION_ERRORS_FILESYSTEM(X) \
    X(SITUATION_ERROR_FILE_ACCESS,                      -300, "A generic file or directory access error occurred") /* EOL: prefer specific SITUATION_ERROR_FILE_* / PATH_* */ \
    X(SITUATION_ERROR_PATH_NOT_FOUND,                   -301, "The specified file or directory was not found") \
    X(SITUATION_ERROR_PATH_INVALID,                     -302, "The specified path is invalid or contains illegal characters") \
    X(SITUATION_ERROR_PERMISSION_DENIED,                -303, "Permission was denied for the requested file operation") /* EOL: prefer SITUATION_ERROR_FILE_ACCESS_DENIED */ \
    X(SITUATION_ERROR_DISK_FULL,                        -304, "The disk is full; cannot complete a write operation") \
    X(SITUATION_ERROR_FILE_LOCKED,                      -305, "The file is locked or currently in use by another process") \
    X(SITUATION_ERROR_DIR_NOT_EMPTY,                    -306, "A directory is not empty and cannot be deleted non-recursively") \
    X(SITUATION_ERROR_FILE_ALREADY_EXISTS,              -307, "The specified file already exists where it shouldn't") \
    X(SITUATION_ERROR_PATH_IS_DIRECTORY,                -308, "A file operation was attempted on a path that is a directory") \
    X(SITUATION_ERROR_PATH_IS_FILE,                     -309, "A directory operation was attempted on a path that is a file") \
    X(SITUATION_ERROR_FILE_NOT_FOUND,                   -310, "File does not exist") \
    X(SITUATION_ERROR_FILE_ACCESS_DENIED,               -311, "Permission denied") \
    X(SITUATION_ERROR_FILE_OPEN_FAILED,                 -312, "fopen() or equivalent failed") \
    X(SITUATION_ERROR_FILE_READ_FAILED,                 -313, "Read operation failed") \
    X(SITUATION_ERROR_FILE_WRITE_FAILED,                -314, "Write operation failed") \
    X(SITUATION_ERROR_FILE_TOO_LARGE,                   -315, "File exceeds internal limits") \
    X(SITUATION_ERROR_DIRECTORY_CREATION_FAILED,        -316, "Failed to create directory") \
    X(SITUATION_ERROR_FILE_MODIFIED,                    -317, "File changed on disk during operation (hot-reload / race)") \
    X(SITUATION_ERROR_HOTRELOAD_WATCHER_FAILED,         -320, "Hot-reload watcher failed (inotify/ReadDirectoryChangesW)") \
    X(SITUATION_ERROR_HOTRELOAD_FILE_CHANGED_TOO_FAST,  -321, "File changed faster than debounce window") \
    X(SITUATION_ERROR_HOTRELOAD_GPU_SYNC_FAILED,        -322, "GPU sync failed during hot-reload (vkDeviceWaitIdle/glFinish)")

// ── Asset & Serialization (-330 to -349) ────────────────────────────────────────
#define SITUATION_ERRORS_ASSET(X) \
    X(SITUATION_ERROR_ASSET_PARSE_FAILED,               -330, "Failed to parse asset file (malformed JSON/XML/Binary)") \
    X(SITUATION_ERROR_ASSET_CORRUPTED,                  -331, "Asset data is corrupted or failed checksum validation") \
    X(SITUATION_ERROR_ASSET_VERSION_MISMATCH,           -332, "Asset was built for an incompatible engine version") \
    X(SITUATION_ERROR_ASSET_DECOMPRESSION_FAILED,       -333, "Failed to decompress asset payload")

// ── Plugins & Scripting (-360 to -379) ──────────────────────────────────────────
#define SITUATION_ERRORS_PLUGIN(X) \
    X(SITUATION_ERROR_PLUGIN_LOAD_FAILED,               -360, "Failed to load dynamic library / shared object") \
    X(SITUATION_ERROR_PLUGIN_SYMBOL_NOT_FOUND,          -361, "Required symbol or function not found in plugin") \
    X(SITUATION_ERROR_PLUGIN_ABI_MISMATCH,              -362, "Plugin ABI version does not match the host engine")

// ── Audio Subsystem (-400 to -439) ──────────────────────────────────────────────
#define SITUATION_ERRORS_AUDIO(X) \
    X(SITUATION_ERROR_AUDIO_CONTEXT,                    -400, "Failed to initialize the audio context (MiniAudio)") /* EOL: prefer SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED */ \
    X(SITUATION_ERROR_AUDIO_DEVICE,                     -401, "Failed to initialize, start, or stop an audio device") /* EOL: prefer SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED */ \
    X(SITUATION_ERROR_AUDIO_SOUND_LIMIT,                -402, "The sound playback queue limit was reached") /* EOL: prefer SITUATION_ERROR_AUDIO_SOUND_LIMIT_REACHED */ \
    X(SITUATION_ERROR_AUDIO_CONVERTER,                  -403, "Failed to configure a data format/rate converter") \
    X(SITUATION_ERROR_AUDIO_DECODING,                   -404, "Failed to decode an audio file") \
    X(SITUATION_ERROR_AUDIO_INVALID_OPERATION,          -405, "Invalid operation on a sound (e.g., cropping a stream)") \
    X(SITUATION_ERROR_AUDIO_BACKEND_INIT_FAILED,        -410, "MiniAudio context initialization failed") \
    X(SITUATION_ERROR_AUDIO_DEVICE_INIT_FAILED,         -411, "Audio device startup failed") \
    X(SITUATION_ERROR_AUDIO_DEVICE_START_FAILED,        -412, "ma_device_start() failed") \
    X(SITUATION_ERROR_AUDIO_DECODER_INIT_FAILED,        -413, "ma_decoder_init failed") \
    X(SITUATION_ERROR_AUDIO_DECODER_FORMAT_UNSUPPORTED, -414, "Codec/container not supported") \
    X(SITUATION_ERROR_AUDIO_STREAM_ENDED,               -415, "Stream reached EOF (not fatal)") \
    X(SITUATION_ERROR_AUDIO_SOUND_LIMIT_REACHED,        -420, "Max concurrent sounds exceeded") \
    X(SITUATION_ERROR_AUDIO_CAPTURE_NOT_AVAILABLE,      -430, "No microphone or capture device found")

// ── Audio Mixer (-440 to -459) ──────────────────────────────────────────────────
#define SITUATION_ERRORS_MIXER(X) \
    X(SITUATION_ERROR_MIXER_NOT_INITIALIZED,            -440, "Mixer not initialized") \
    X(SITUATION_ERROR_MIXER_TRACK_LIMIT,                -441, "Maximum number of tracks reached") \
    X(SITUATION_ERROR_MIXER_TRACK_INVALID,              -442, "Invalid track ID or track not active") \
    X(SITUATION_ERROR_MIXER_BUS_LIMIT,                  -443, "Maximum number of aux buses reached") \
    X(SITUATION_ERROR_MIXER_BUS_INVALID,                -444, "Invalid bus ID or bus not active") \
    X(SITUATION_ERROR_MIXER_INSERT_INVALID,             -445, "Invalid insert position") \
    X(SITUATION_ERROR_MIXER_INSERT_ALREADY_ATTACHED,    -446, "Insert chain already attached at this position") \
    X(SITUATION_ERROR_MIXER_INSERT_NOT_ATTACHED,        -447, "No insert chain at this position") \
    X(SITUATION_ERROR_MIXER_ROUTING_CYCLE,              -448, "Routing would create a cycle (feedback loop)") \
    X(SITUATION_ERROR_MIXER_ROUTING_INVALID,            -449, "Invalid routing configuration") \
    X(SITUATION_ERROR_MIXER_SEND_INVALID,               -450, "Invalid aux send configuration") \
    X(SITUATION_ERROR_MIXER_TOPOLOGY_LOCKED,            -451, "Cannot modify topology while processing") \
    X(SITUATION_ERROR_MIXER_SCENE_LOAD_FAILED,          -452, "Failed to load mixer scene") \
    X(SITUATION_ERROR_MIXER_SCENE_SAVE_FAILED,          -453, "Failed to save mixer scene") \
    X(SITUATION_ERROR_MIXER_SCENE_VERSION_MISMATCH,     -454, "Scene file version incompatible")

// ── Audio Node Graph (-460 to -479) ─────────────────────────────────────────────
#define SITUATION_ERRORS_NODE_GRAPH(X) \
    X(SITUATION_ERROR_NODE_GRAPH_NOT_INITIALIZED,       -460, "Node graph not initialized") \
    X(SITUATION_ERROR_NODE_LIMIT_REACHED,               -461, "Maximum number of nodes reached") \
    X(SITUATION_ERROR_NODE_INVALID_HANDLE,              -462, "Invalid node handle (generation mismatch or out of range)") \
    X(SITUATION_ERROR_NODE_TYPE_INVALID,                -463, "Invalid or unregistered node type") \
    X(SITUATION_ERROR_NODE_ALREADY_EXISTS,              -464, "Node with this ID already exists") \
    X(SITUATION_ERROR_NODE_NOT_FOUND,                   -465, "Node not found in graph") \
    X(SITUATION_ERROR_NODE_PORT_INVALID,                -466, "Invalid port index (out of range)") \
    X(SITUATION_ERROR_NODE_PORT_TYPE_MISMATCH,          -467, "Port type mismatch (audio vs control)") \
    X(SITUATION_ERROR_NODE_CHANNEL_MISMATCH,            -468, "Channel count mismatch (mono vs stereo)") \
    X(SITUATION_ERROR_NODE_PATCH_ALREADY_EXISTS,        -469, "Patch already exists between these ports") \
    X(SITUATION_ERROR_NODE_PATCH_NOT_FOUND,             -470, "Patch not found") \
    X(SITUATION_ERROR_NODE_PATCH_CYCLE_DETECTED,        -471, "Patch would create a cycle") \
    X(SITUATION_ERROR_NODE_CONTROL_INVALID,             -472, "Invalid control ID or control not found") \
    X(SITUATION_ERROR_NODE_CONTROL_OUT_OF_RANGE,        -473, "Control value out of valid range") \
    X(SITUATION_ERROR_NODE_CONTROL_TYPE_MISMATCH,       -474, "Control type mismatch (float vs int vs bool)") \
    X(SITUATION_ERROR_NODE_PROCESSING_FAILED,           -475, "Node processing failed (device error)") \
    X(SITUATION_ERROR_NODE_SERIALIZATION_FAILED,        -476, "Failed to serialize node graph") \
    X(SITUATION_ERROR_NODE_DESERIALIZATION_FAILED,      -477, "Failed to deserialize node graph") \
    X(SITUATION_ERROR_NODE_TOPOLOGY_INVALID,            -478, "Invalid graph topology (disconnected, no output, etc.)") \
    X(SITUATION_ERROR_NODE_ALLOCATION_FAILED,           -479, "Memory allocation failed during node operation")

// ── Audio Device Registry (-480 to -493) ────────────────────────────────────────
#define SITUATION_ERRORS_DEVICE_REGISTRY(X) \
    X(SITUATION_ERROR_DEVICE_REGISTRY_NOT_INITIALIZED,  -480, "Device registry not initialized") \
    X(SITUATION_ERROR_DEVICE_TYPE_INVALID,              -481, "Invalid device type ID") \
    X(SITUATION_ERROR_DEVICE_TYPE_NOT_REGISTERED,       -482, "Device type not found in registry") \
    X(SITUATION_ERROR_DEVICE_TYPE_ALREADY_REGISTERED,   -483, "Device type already registered (duplicate)") \
    X(SITUATION_ERROR_DEVICE_REGISTRY_FULL,             -484, "Registry capacity reached (max 64 devices)") \
    X(SITUATION_ERROR_DEVICE_METADATA_INVALID,          -485, "Invalid device metadata (missing name, ports, etc.)") \
    X(SITUATION_ERROR_DEVICE_CONTROL_INVALID,           -486, "Invalid control definition") \
    X(SITUATION_ERROR_DEVICE_PORT_INVALID,              -487, "Invalid port definition") \
    X(SITUATION_ERROR_DEVICE_CATEGORY_INVALID,          -488, "Invalid device category") \
    X(SITUATION_ERROR_DEVICE_QUERY_FAILED,              -489, "Device query operation failed") \
    X(SITUATION_ERROR_DEVICE_FUNCTION_TABLE_INVALID,    -490, "Invalid device function table") \
    X(SITUATION_ERROR_DEVICE_CREATE_FAILED,             -491, "Device creation failed") \
    X(SITUATION_ERROR_DEVICE_DESTROY_FAILED,            -492, "Device destruction failed") \
    X(SITUATION_ERROR_DEVICE_PROCESS_FAILED,            -493, "Device processing failed")

// ── MIDI Integration (-494 to -499) ─────────────────────────────────────────────
#define SITUATION_ERRORS_MIDI(X) \
    X(SITUATION_ERROR_MIDI_INIT_FAILED,                 -494, "Failed to initialize MIDI system") \
    X(SITUATION_ERROR_MIDI_NO_DEVICES,                  -495, "No MIDI devices available") \
    X(SITUATION_ERROR_MIDI_DEVICE_OPEN_FAILED,          -496, "Failed to open MIDI device") \
    X(SITUATION_ERROR_MIDI_NOT_SUPPORTED,               -497, "Device type doesn't support MIDI") \
    X(SITUATION_ERROR_MIDI_LEARN_NOT_ENABLED,           -498, "MIDI Learn not enabled for node") \
    X(SITUATION_ERROR_MIDI_LEARN_ALREADY_ENABLED,       -499, "MIDI Learn already enabled")

// ── Resource Management & Rendering Core (-500 to -599) ─────────────────────────
#define SITUATION_ERRORS_RENDERING(X) \
    X(SITUATION_ERROR_RESOURCE_INVALID,                 -500, "Invalid resource handle (shader, mesh, texture, buffer)") /* EOL: prefer SITUATION_ERROR_INVALID_RESOURCE_HANDLE */ \
    X(SITUATION_ERROR_BUFFER_INVALID_SIZE,              -501, "Buffer operation with out-of-bounds offset or size") \
    X(SITUATION_ERROR_RENDER_COMMAND_FAILED,            -502, "Command failed to be recorded to command buffer") \
    X(SITUATION_ERROR_RENDER_PASS_ACTIVE,               -503, "Operation illegal during an active render pass") \
    X(SITUATION_ERROR_INVALID_RESOURCE_HANDLE,          -510, "Null or corrupted handle passed") \
    X(SITUATION_ERROR_RESOURCE_ALREADY_DESTROYED,       -511, "Use-after-free attempt") \
    X(SITUATION_ERROR_BUFFER_MAP_FAILED,                -512, "vkMapMemory / glMapBuffer failed") \
    X(SITUATION_ERROR_BUFFER_OVERFLOW,                  -513, "Write beyond buffer bounds") \
    X(SITUATION_ERROR_BUFFER_INVALID_USAGE,             -514, "Wrong usage flags for operation") \
    X(SITUATION_ERROR_MESH_DEVICE_ADDRESS_UNSUPPORTED,  -515, "Mesh vertex/index buffer device address unavailable (no SHADER_DEVICE_ADDRESS support on this backend/hardware)") \
    X(SITUATION_ERROR_TEXTURE_UPLOAD_FAILED,            -520, "Texture upload to GPU failed") \
    X(SITUATION_ERROR_TEXTURE_INVALID_USAGE,            -521, "Texture missing required usage flags for operation") \
    X(SITUATION_ERROR_TEXTURE_REGION_INVALID,           -522, "Texture region, mip, layer, extent, or row pitch is invalid or out of bounds") \
    X(SITUATION_ERROR_TEXTURE_FORMAT_UNSUPPORTED,       -523, "Texture format cannot be used for requested operation") \
    X(SITUATION_ERROR_NO_ACTIVE_COMMAND_BUFFER,         -530, "No frame acquired") \
    X(SITUATION_ERROR_COMMAND_BUFFER_FULL,              -531, "Command limit reached") \
    X(SITUATION_ERROR_NO_RENDER_PASS_ACTIVE,            -540, "Draw call outside render pass") \
    X(SITUATION_ERROR_RENDER_PASS_ALREADY_ACTIVE,       -541, "Nested render pass attempted") \
    X(SITUATION_ERROR_BACKEND_MISMATCH,                 -550, "Operation requested on wrong backend") \
    X(SITUATION_ERROR_BACKEND_SPECIFIC,                 -551, "Backend-specific GPU operation failed (see detail)") \
    X(SITUATION_ERROR_PIPELINE_BIND_FAIL,               -552, "Failed to bind pipeline (incompatible layout or invalid handle)") \
    X(SITUATION_ERROR_SHADER_LOAD_IN_PROGRESS,          -553, "Shader compile or link still in progress (poll again next frame)") \
    X(SITUATION_ERROR_SPIRV_FILE_READ_FAILED,           -554, "SPIR-V file read failed (.spv missing or unreadable)") \
    X(SITUATION_ERROR_SPIRV_INVALID_BINARY,             -555, "SPIR-V binary invalid (null, empty, or misaligned size)") \
    X(SITUATION_ERROR_INDIRECT_COMMAND_INVALID,         -556, "Indirect command buffer range, alignment, or payload is invalid") \
    X(SITUATION_ERROR_SHADER_COMPILE_TIMEOUT,           -557, "Async shader compile exceeded wall-clock deadline (compile worker wedged or starved)")

// ── Fonts & Typography (-560 to -579) ───────────────────────────────────────────
#define SITUATION_ERRORS_FONT(X) \
    X(SITUATION_ERROR_FONT_LOAD_FAILED,                 -560, "Failed to load font face (e.g., FreeType/stb_truetype error)") \
    X(SITUATION_ERROR_FONT_GLYPH_MISSING,               -561, "Requested glyph is not present in the font") \
    X(SITUATION_ERROR_FONT_ATLAS_FULL,                  -562, "Font texture atlas is full; cannot pack more glyphs")

// ── Image Operations (-580 to -589) ─────────────────────────────────────────────
#define SITUATION_ERRORS_IMAGE(X) \
    X(SITUATION_ERROR_IMAGE_OPERATION_FAILED,           -580, "Image operation failed (crop, resize, flip, or save)")

// ── OpenGL Backend (-600 to -699) ───────────────────────────────────────────────
#define SITUATION_ERRORS_OPENGL(X) \
    X(SITUATION_ERROR_OPENGL_GENERAL,                   -600, "OpenGL: A general error occurred (glGetError)") \
    X(SITUATION_ERROR_OPENGL_LOADER_FAILED,             -601, "OpenGL: Failed to load functions (GLAD)") \
    X(SITUATION_ERROR_OPENGL_UNSUPPORTED,               -602, "OpenGL: Required version or extension not supported") \
    X(SITUATION_ERROR_OPENGL_SHADER_COMPILE,            -610, "OpenGL: GLSL shader compilation failed") /* EOL: prefer SITUATION_ERROR_OPENGL_SHADER_COMPILE_FAILED */ \
    X(SITUATION_ERROR_OPENGL_SHADER_LINK,               -611, "OpenGL: GLSL shader program linking failed") /* EOL: prefer SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED */ \
    X(SITUATION_ERROR_OPENGL_FBO_INCOMPLETE,            -620, "OpenGL: Framebuffer Object is not complete") \
    X(SITUATION_ERROR_OPENGL_CONTEXT_CREATION_FAILED,   -630, "OpenGL: Context creation failed") \
    X(SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION,       -631, "OpenGL: Version too old (requires 4.6+)") \
    X(SITUATION_ERROR_OPENGL_SHADER_COMPILE_FAILED,     -632, "OpenGL: Detailed shader compilation error") \
    X(SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED,        -633, "OpenGL: Detailed shader linking error") \
    X(SITUATION_ERROR_OPENGL_PROGRAM_VALIDATION_FAILED, -634, "OpenGL: Program validation failed") \
    X(SITUATION_ERROR_OPENGL_UNIFORM_NOT_FOUND,         -635, "OpenGL: Uniform location query failed") \
    X(SITUATION_ERROR_OPENGL_SPIRV_UNAVAILABLE,         -636, "OpenGL: GL_ARB_gl_spirv not available (cannot load SPIR-V)") \
    X(SITUATION_ERROR_OPENGL_SPIRV_INVALID_BINARY,      -637, "OpenGL: SPIR-V blob invalid (null, empty, or misaligned size)") \
    X(SITUATION_ERROR_OPENGL_SPIRV_VS_SPECIALIZE_FAILED,-638, "OpenGL: SPIR-V vertex shader specialization failed") \
    X(SITUATION_ERROR_OPENGL_SPIRV_FS_SPECIALIZE_FAILED,-639, "OpenGL: SPIR-V fragment shader specialization failed") \
    X(SITUATION_ERROR_OPENGL_SPIRV_CS_SPECIALIZE_FAILED,-640, "OpenGL: SPIR-V compute shader specialization failed") \
    X(SITUATION_ERROR_OPENGL_SPIRV_PROGRAM_LINK_FAILED, -641, "OpenGL: SPIR-V graphics program link failed")

// ── Vulkan Backend (-700 to -799) ───────────────────────────────────────────────
#define SITUATION_ERRORS_VULKAN(X) \
    X(SITUATION_ERROR_VULKAN_INIT_FAILED,               -700, "Vulkan: General initialization failed") /* EOL: prefer detailed SITUATION_ERROR_VULKAN_*_FAILED codes */ \
    X(SITUATION_ERROR_VULKAN_INSTANCE_FAILED,           -701, "Vulkan: Failed to create VkInstance") /* EOL: prefer SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED */ \
    X(SITUATION_ERROR_VULKAN_DEVICE_FAILED,             -702, "Vulkan: Failed to select physical or create logical device") /* EOL: prefer SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED */ \
    X(SITUATION_ERROR_VULKAN_UNSUPPORTED,               -703, "Vulkan: Required layer, extension, or feature unsupported") \
    X(SITUATION_ERROR_VULKAN_SWAPCHAIN_FAILED,          -710, "Vulkan: Swapchain operation failed") /* EOL: prefer SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED */ \
    X(SITUATION_ERROR_VULKAN_COMMAND_FAILED,            -720, "Vulkan: Command pool or buffer operation failed") \
    X(SITUATION_ERROR_VULKAN_COMMAND_BUFFER_FAILED,     -721, "Vulkan: Command buffer record, submit, or sync failed") \
    X(SITUATION_ERROR_VULKAN_RENDERPASS_FAILED,         -730, "Vulkan: Failed to create VkRenderPass") \
    X(SITUATION_ERROR_VULKAN_FRAMEBUFFER_FAILED,        -731, "Vulkan: Failed to create VkFramebuffer") \
    X(SITUATION_ERROR_VULKAN_PIPELINE_FAILED,           -732, "Vulkan: Failed to create graphics or compute pipeline") /* EOL: prefer SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED */ \
    X(SITUATION_ERROR_VULKAN_SYNC_OBJECT_FAILED,        -733, "Vulkan: Failed to create fence or semaphore") \
    X(SITUATION_ERROR_VULKAN_MEMORY_ALLOC_FAILED,       -734, "Vulkan: GPU memory allocation failed (VMA)") /* EOL: prefer SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED */ \
    X(SITUATION_ERROR_VULKAN_DESCRIPTOR_FAILED,         -735, "Vulkan: Descriptor set or pool operation failed") \
    X(SITUATION_ERROR_VULKAN_INSTANCE_CREATION_FAILED,  -740, "Vulkan: Detailed instance creation failure") \
    X(SITUATION_ERROR_VULKAN_PHYSICAL_DEVICE_UNSUITABLE,-741, "Vulkan: No suitable physical device found") \
    X(SITUATION_ERROR_VULKAN_DEVICE_CREATION_FAILED,    -742, "Vulkan: Logical device creation failed") \
    X(SITUATION_ERROR_VULKAN_SWAPCHAIN_CREATION_FAILED, -743, "Vulkan: Swapchain creation failed") \
    X(SITUATION_ERROR_VULKAN_SWAPCHAIN_INVALID,         -744, "Vulkan: Invalid swapchain state") \
    X(SITUATION_ERROR_VULKAN_IMAGE_ACQUIRE_FAILED,      -745, "Vulkan: Image acquire failed") \
    X(SITUATION_ERROR_VULKAN_QUEUE_SUBMIT_FAILED,       -746, "Vulkan: Queue submit failed") \
    X(SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED,  -747, "Vulkan: Pipeline creation failed") \
    X(SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED,      -748, "Vulkan: Shader module creation failed") \
    X(SITUATION_ERROR_VULKAN_DESCRIPTOR_POOL_EXHAUSTED, -749, "Vulkan: Descriptor pool exhausted") \
    X(SITUATION_ERROR_VULKAN_MEMORY_ALLOCATION_FAILED,  -750, "Vulkan: Detailed memory allocation error") \
    X(SITUATION_ERROR_VULKAN_VALIDATION_LAYER_ERROR,    -751, "Vulkan: Validation layer reported error") \
    X(SITUATION_ERROR_SHADER_COMPILATION_FAILED,        -752, "Shader compilation failed (shaderc)") \
    X(SITUATION_ERROR_VULKAN_SPIRV_INVALID,             -753, "Vulkan: SPIR-V blob invalid (null, empty, or misaligned size)") \
    X(SITUATION_ERROR_VULKAN_SPIRV_VS_MODULE_FAILED,    -754, "Vulkan: SPIR-V vertex shader module creation failed") \
    X(SITUATION_ERROR_VULKAN_SPIRV_FS_MODULE_FAILED,    -755, "Vulkan: SPIR-V fragment shader module creation failed") \
    X(SITUATION_ERROR_VULKAN_SPIRV_CS_MODULE_FAILED,    -756, "Vulkan: SPIR-V compute shader module creation failed") \
    X(SITUATION_ERROR_SHADER_COMPILER_REQUIRED,         -757, "Vulkan: internal pipelines require SITUATION_ENABLE_SHADER_COMPILER; recompile the library with shaderc support")

// ── Compute / GPGPU (-800 to -899) ──────────────────────────────────────────────
#define SITUATION_ERRORS_COMPUTE(X) \
    X(SITUATION_ERROR_COMPUTE_PIPELINE_CREATION_FAILED, -800, "Compute pipeline creation failed") \
    X(SITUATION_ERROR_COMPUTE_DISPATCH_FAILED,          -801, "Compute dispatch failed") \
    X(SITUATION_ERROR_COMPUTE_BUFFER_BINDING_MISSING,   -802, "Missing storage buffer binding")

// ── Network (-900 to -949) ──────────────────────────────────────────────────────
#define SITUATION_ERRORS_NETWORK(X) \
    X(SITUATION_ERROR_NETWORK_INIT_FAILED,              -900, "Network: Initialization failed") \
    X(SITUATION_ERROR_NETWORK_SOCKET_CREATION_FAILED,   -901, "Network: Socket creation failed") \
    X(SITUATION_ERROR_NETWORK_CONNECTION_FAILED,        -902, "Network: Connection failed") \
    X(SITUATION_ERROR_NETWORK_SEND_FAILED,              -903, "Network: Send failed") \
    X(SITUATION_ERROR_NETWORK_RECEIVE_FAILED,           -904, "Network: Receive failed") \
    X(SITUATION_ERROR_NETWORK_BIND_FAILED,              -905, "Network: Bind failed") \
    X(SITUATION_ERROR_NETWORK_LISTEN_FAILED,            -906, "Network: Listen failed") \
    X(SITUATION_ERROR_NETWORK_ACCEPT_FAILED,            -907, "Network: Accept failed") \
    X(SITUATION_ERROR_NETWORK_TIMEOUT,                  -908, "Network: Operation timed out") \
    X(SITUATION_ERROR_NETWORK_DNS_RESOLUTION_FAILED,    -909, "Network: DNS resolution failed") \
    X(SITUATION_ERROR_NETWORK_TLS_HANDSHAKE_FAILED,     -910, "Network: TLS/SSL handshake failed")

//==================================================================================
// Master Table — expands all sections in order
//==================================================================================
#define SITUATION_ERROR_TABLE(X) \
    SITUATION_ERRORS_CORE(X) \
    SITUATION_ERRORS_THREADING(X) \
    SITUATION_ERRORS_PLATFORM(X) \
    SITUATION_ERRORS_INPUT(X) \
    SITUATION_ERRORS_DISPLAY(X) \
    SITUATION_ERRORS_FILESYSTEM(X) \
    SITUATION_ERRORS_ASSET(X) \
    SITUATION_ERRORS_PLUGIN(X) \
    SITUATION_ERRORS_AUDIO(X) \
    SITUATION_ERRORS_MIXER(X) \
    SITUATION_ERRORS_NODE_GRAPH(X) \
    SITUATION_ERRORS_DEVICE_REGISTRY(X) \
    SITUATION_ERRORS_MIDI(X) \
    SITUATION_ERRORS_RENDERING(X) \
    SITUATION_ERRORS_FONT(X) \
    SITUATION_ERRORS_IMAGE(X) \
    SITUATION_ERRORS_OPENGL(X) \
    SITUATION_ERRORS_VULKAN(X) \
    SITUATION_ERRORS_COMPUTE(X) \
    SITUATION_ERRORS_NETWORK(X) \
    X(SITUATION_ERROR_UNKNOWN_ERROR, -999, "Unknown error")

//==================================================================================
// Generate the Enum
//==================================================================================
typedef enum {
    #define _SIT_ERRNO_ENUM(name, value, msg) name = value,
    SITUATION_ERROR_TABLE(_SIT_ERRNO_ENUM)
    #undef _SIT_ERRNO_ENUM
} SituationError;

/* Historical / doc names — #define aliases only (not in switch table; duplicate values). */
#define SITUATION_ERROR_GLAD_LOAD_FAILED              SITUATION_ERROR_OPENGL_LOADER_FAILED
#define SITUATION_ERROR_GL_VERSION_TOO_LOW              SITUATION_ERROR_OPENGL_UNSUPPORTED_VERSION
#define SITUATION_ERROR_GL_ERROR                        SITUATION_ERROR_OPENGL_GENERAL
#define SITUATION_ERROR_GL_EXTENSION_MISSING            SITUATION_ERROR_OPENGL_UNSUPPORTED
#define SITUATION_ERROR_GL_UPLOAD_FAILED                SITUATION_ERROR_TEXTURE_UPLOAD_FAILED
#define SITUATION_ERROR_VULKAN_UPLOAD_FAILED            SITUATION_ERROR_TEXTURE_UPLOAD_FAILED
#define SITUATION_ERROR_VULKAN_PIPELINE_CREATE_FAILED   SITUATION_ERROR_VULKAN_PIPELINE_CREATION_FAILED
#define SITUATION_ERROR_SHADER_LINK_FAILED              SITUATION_ERROR_OPENGL_SHADER_LINK_FAILED
#define SITUATION_ERROR_SHADER_MODULE_CREATE_FAILED     SITUATION_ERROR_VULKAN_SHADER_MODULE_FAILED
#define SITUATION_ERROR_ACCESS_DENIED                   SITUATION_ERROR_FILE_ACCESS_DENIED

#endif // SITUATION_BASE_ERRNO_H
