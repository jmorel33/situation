/***************************************************************************************************
*
*   situation_api_types_audio.h - Audio Graph, MIDI, and Tone Types
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Node graph, device registry, mixer metadata, MIDI port info, resonance/tone enums, and
*   audio format structs. ma_device_id comes from miniaudio.h (via situation.h) — not typedef'd here.
*
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_API_TYPES_AUDIO_H
#define SITUATION_API_TYPES_AUDIO_H

#include "situation_api_config.h"
#include "situation_base_types.h"

/* SituationAudioDeviceInfo::native_id is miniaudio's ma_device_id. That type is provided
 * by miniaudio.h (included from situation.h before this header) or by harness forward
 * declarations when parsing situation_api.h standalone — do not typedef it here. */

// --- Audio Control Structures ---

// --- Audio Handle System --- (SituationSound, SituationSoundHandle, SITUATION_NULL_HANDLE defined in situation_base_types.h)
// --- Tone Handle --- (SituationToneHandle defined in situation_base_types.h)
// --- Node Handle --- (SituationNodeHandle, SITUATION_INVALID_NODE_HANDLE defined in situation_base_types.h)

typedef struct {
    int sample_rate;
    int channels;
    int bit_depth;
} SituationAudioFormat;

// --- Mixer & Device Types (Phase 0/1) ---
typedef enum {
    SIT_AUDIO_DEVICE_TYPE_UNKNOWN     = 0,
    SIT_AUDIO_DEVICE_TYPE_PLAYBACK    = 1,   // Output only (speakers, headphones)
    SIT_AUDIO_DEVICE_TYPE_CAPTURE     = 2,   // Input only (microphones, line-in)
    SIT_AUDIO_DEVICE_TYPE_DUPLEX      = 3,   // Both input and output (most sound cards)
    SIT_AUDIO_DEVICE_TYPE_LOOPBACK    = 4    // System loopback (for desktop audio capture)
} SituationAudioDeviceType;

typedef struct {
    char name[256];                    // Human-readable name
    char id[128];                      // Backend-specific unique identifier (stringified)
    ma_device_id native_id;            // [INTERNAL] Raw miniaudio ID

    SituationAudioDeviceType type;

    uint32_t min_channels_in;          // Minimum supported input channels
    uint32_t max_channels_in;          // Maximum supported input channels
    uint32_t min_channels_out;         // Minimum supported output channels
    uint32_t max_channels_out;         // Maximum supported output channels

    uint32_t preferred_sample_rate;    // Preferred / native sample rate
    uint32_t native_format;            // Preferred miniaudio format (f32, s16, etc.)

    bool is_default_playback;          // Is this the system's default output?
    bool is_default_capture;           // Is this the system's default input?

    uint32_t latency_us;               // Estimated latency in microseconds
} SituationAudioDeviceInfo;

// --- Mixer Data Structures ---
typedef struct SituationAudioMixer SituationAudioMixer;
typedef struct SituationAudioTrack SituationAudioTrack;
typedef struct SituationAudioBus SituationAudioBus;

// ================================================================================================
// NODE GRAPH & DEVICE REGISTRY TYPES (Phase 3-5)
// ================================================================================================

// --- Configuration Constants ---
#define SITUATION_MAX_DEVICES           	64      // Maximum number of registered device types
#define SITUATION_MAX_DEVICE_NAME       	64      // Maximum length of device name
#define SITUATION_MAX_CONTROL_NAME      	32      // Maximum length of control parameter name
#define SITUATION_MAX_CONTROLS_PER_DEVICE 	48    	// Maximum controls per device (Tone Synth uses 34)
#define SITUATION_MAX_NODES             	256     // Maximum nodes in a graph
#define SITUATION_MAX_PATCHES_PER_PORT  	16      // Maximum connections per port
#define SITUATION_MAX_AUDIO_BUFFER      	2048    // Maximum audio buffer size (frames)

// --- Device Categories ---
typedef enum {
    SITUATION_DEVICE_EFFECT = 0,    // Audio effects (reverb, delay, distortion, etc.)
    SITUATION_DEVICE_SOURCE,        // Audio generators (tone synth, sample playback)
    SITUATION_DEVICE_CAPTURE,       // Audio input devices (microphone, line-in)
    SITUATION_DEVICE_UTILITY,       // Routing/mixing utilities (panner, gain, mixer)
    SITUATION_DEVICE_MODULATOR,     // Control signal generators (LFO, envelope follower)
    SITUATION_DEVICE_ANALYZER,      // Analysis tools (spectrum, meter, oscilloscope)
    SITUATION_DEVICE_CUSTOM         // User-defined custom devices
} SituationDeviceCategory;

// --- Control Parameter Types ---
typedef enum {
    SITUATION_CONTROL_FLOAT = 0,    // Floating point value (e.g., 0.0 to 1.0)
    SITUATION_CONTROL_INT,          // Integer value (e.g., 0, 1, 2 for modes)
    SITUATION_CONTROL_BOOL,         // Boolean value (0 or 1)
    SITUATION_CONTROL_ENUM          // Enumerated choice (stored as int, with string labels)
} SituationControlType;

// --- Device Type Identifiers ---
typedef enum {
    // Effects
    SITUATION_NODE_REVERB = 0,
    SITUATION_NODE_ECHO,
    SITUATION_NODE_CHORUS,
    SITUATION_NODE_PHASER,
    SITUATION_NODE_OVERDRIVE,
    SITUATION_NODE_EXCITER,
    SITUATION_NODE_MAXIMIZER,
    SITUATION_NODE_SPRING_REVERB,
    SITUATION_NODE_STUDIO_REVERB,
    SITUATION_NODE_SST282,
    SITUATION_NODE_DYNAMICS,
    SITUATION_NODE_COMPANDER,
    SITUATION_NODE_EQ_4BAND,
    SITUATION_NODE_FILTER,
    SITUATION_NODE_MASTERING_AMP,
    SITUATION_NODE_DEAFMAX,
    SITUATION_NODE_ISA110,          // Focusrite ISA 110 preamp + 4-band inductor EQ emulation
    // Utilities
    SITUATION_NODE_PANNER,
    SITUATION_NODE_GAIN,
    SITUATION_NODE_MIXER,
    // Sources
    SITUATION_NODE_SOUND_SOURCE,
    SITUATION_NODE_TONE_SYNTH,
    SITUATION_NODE_PCM_INPUT,
    // Capture
    SITUATION_NODE_MIC_CAPTURE,
    // Modulators
    SITUATION_NODE_LFO,
    SITUATION_NODE_ENVELOPE_FOLLOWER,
    // Analyzers
    SITUATION_NODE_SPECTRUM_ANALYZER,
    SITUATION_NODE_PEAK_METER,
    // Custom devices start here
    SITUATION_NODE_CUSTOM = 1000
} SituationNodeType;

// --- Control Parameter Descriptor ---
typedef struct {
    uint32_t id;
    char name[SITUATION_MAX_CONTROL_NAME];
    SituationControlType type;
    float min_value;
    float max_value;
    float default_value;
    const char** enum_labels;
    int enum_count;
    const char* units;
    bool is_logarithmic;
} SituationControlDesc;

// --- Device Metadata ---
typedef struct {
    SituationNodeType type;
    char name[SITUATION_MAX_DEVICE_NAME];
    SituationDeviceCategory category;
    uint8_t num_audio_ins;
    uint8_t num_audio_outs;
    uint8_t audio_channels;
    uint8_t num_ctrl_ins;
    uint8_t num_ctrl_outs;
    SituationControlDesc controls[SITUATION_MAX_CONTROLS_PER_DEVICE];
    uint8_t num_controls;
    uint32_t latency_samples;
    const char* description;
    const char* author;
    uint32_t version;
    void* (*create_func)(void);
    void (*destroy_func)(void*);
    void (*process_func)(void*, float**, float**, uint32_t);
    void (*set_control_func)(void*, uint32_t, float);
    float (*get_control_func)(void*, uint32_t);
} SituationDeviceMetadata;

// --- Node Handle --- (defined in situation_base_types.h)
// SituationNodeHandle = uint32_t, SITUATION_INVALID_NODE_HANDLE = 0xFFFFFFFF

// --- Audio Port ---
typedef struct {
    float* buffer;
    int channels;
    int frames;
} SituationAudioPort;

// --- Control Port ---
typedef struct {
    float value;
    bool is_modulated;
} SituationControlPort;

// --- Patch Connection ---
typedef struct {
    SituationNodeHandle src_node;
    int src_port;
    SituationNodeHandle dst_node;
    int dst_port;
    bool is_control;
} SituationPatch;

// --- Forward Declarations ---
typedef struct SituationNode SituationNode;
typedef struct SituationAudioGraph SituationAudioGraph;

// --- Device Function Table (for processing) ---
typedef struct {
    SituationNodeType type;
    void* (*create)(const SituationDeviceMetadata*);
    void (*destroy)(void*);
    void (*process)(void*, SituationAudioPort*, SituationAudioPort*, float*, int);
} SituationDeviceFunctions;

/**
 * @brief Strategy for loading audio data from disk.
 * @details This enum allows the user to control the trade-off between RAM usage and CPU/Disk latency.
 *
 * - **SITUATION_AUDIO_LOAD_AUTO:** The recommended default. Automatically selects the strategy based on file duration.
 *   Files shorter than ~10 seconds are fully decoded to RAM (safest for SFX). Longer files are streamed.
 * - **SITUATION_AUDIO_LOAD_FULL:** Forces the entire audio file to be decoded into a raw PCM buffer in RAM upon load.
 *   - *Pros:* Zero disk I/O during playback; impossible to stutter during gameplay; perfectly thread-safe.
 *   - *Cons:* Higher RAM usage. High load times for long music tracks.
 * - **SITUATION_AUDIO_LOAD_STREAM:** Forces the audio engine to read from the file on disk during playback.
 *   - *Pros:* Minimal RAM usage; instant load times.
 *   - *Cons:* Risk of audio stuttering if the OS disk cache misses or if the drive is busy (e.g., loading textures).
 */
typedef enum {
    SITUATION_AUDIO_LOAD_AUTO,   // Library decides based on file size (<10 sec -> RAM)
    SITUATION_AUDIO_LOAD_FULL,   // Force full decode to RAM (Safest, best for SFX)
    SITUATION_AUDIO_LOAD_STREAM  // Force disk streaming (Best for long Music)
} SituationAudioLoadMode;

typedef enum {
    SIT_WAVE_SINE,      // Pure tone
    SIT_WAVE_SQUARE,    // Retro/8-bit sound (has harmonics)
    SIT_WAVE_TRIANGLE,  // Mellow, flute-like
    SIT_WAVE_SAW,       // Harsh, string-like
    SIT_WAVE_NOISE      // White noise (for percussion/explosions)
} SituationWaveType;

typedef enum {
    SITUATION_FILTER_NONE,
    SITUATION_FILTER_LOWPASS,
    SITUATION_FILTER_HIGHPASS
} SituationFilterType;
//==================================================================================================
//  Additional public types (P2.1 — referenced by SITAPI declarations below)
//==================================================================================================



/** @brief MIDI device information for device selection. */
typedef struct {
    int device_id;                  // Device ID for use with SituationEnableMidiControl
    char device_name[128];          // Human-readable device name
    int is_input;                   // 1 if input device, 0 otherwise
    int is_output;                  // 1 if output device, 0 otherwise
} SituationMidiDeviceInfo;

#endif /* SITUATION_API_TYPES_AUDIO_H */
