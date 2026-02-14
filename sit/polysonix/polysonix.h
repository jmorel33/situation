/**
 * @file polysonix.h
 * @brief A single-header polyphonic synthesizer engine.
 *
 * This file contains the entire implementation of the Polysonix synthesizer engine.
 * To use it, you must `#define POLYSONIX_IMPLEMENTATION` in one C/C++ file
 * before including this header.
 *
 * Polysonix provides a thread-safe API for real-time audio synthesis, featuring
 * multiple voices, ADSR envelopes, LFOs, a multi-mode filter, and a master limiter.
 * It uses a command queue to safely receive parameter changes from a UI/control thread
 * while the audio processing runs on a dedicated audio thread.
 *
 * @copyright Copyright (c) 2025, Jacques Morel
 * @license This software is licensed under the MIT License. See the LICENSE file for more information.
 */
// --- Version Macros ---
#define POLYSONIX_VERSION_MAJOR 1
#define POLYSONIX_VERSION_MINOR 8
#define POLYSONIX_VERSION_PATCH 5
#define POLYSONIX_VERSION_REVISION ""

#ifndef POLYSONIX_H
#define POLYSONIX_H

#include "px_vm.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

/**
 * @def PX_API
 * @brief A macro to control the visibility and linkage of public API functions.
 */
#define PX_API

// --- Forward Declarations ---
typedef struct PxSynth PxSynth;
typedef struct PxConfig PxConfig;
typedef struct PxPatch PxPatch;
typedef struct PxVoiceInfo PxVoiceInfo;
typedef struct PxLimiterInfo PxLimiterInfo;
typedef struct PxWaveInfo PxWaveInfo;
typedef struct PxLFOInfo PxLFOInfo;

// --- Public Enums and Structs ---

#define PX_MAX_OSC_PER_VOICE 3

/**
 * @struct PxOscillator
 * @brief Configuration for a single oscillator in a voice.
 */
typedef struct {
    bool  enabled;
    int   wave_idx;         /**< Bytecode waveform index. */
    float coarse_semitones; /**< Tuning offset in semitones (-24 to +24). */
    float fine_cents;       /**< Fine tuning offset in cents (-100 to +100). */
    float mix_level;        /**< Output mix level (0.0 to 1.0). */
    float pan;              /**< Stereo pan position (-1.0 to 1.0). */
    int   sequence_id;      /**< Wave Sequence ID (-1 = Disabled/Static, 0-127 = Active). */

    // v1.7 Features
    bool  cross_mod_enabled;    /**< Osc n modulates n-1 */
    float cross_mod_depth;      /**< 0.0–1.0 (matrix modulatable) */
    bool  phase_dist_enabled;   /**< Phase Distortion on/off */
    float phase_dist_amount;    /**< 0.0–1.0 (warp factor) */
    bool  osc_sync_enabled;     /**< Sync to previous osc */
    float osc_sync_softness;    /**< 0.0 (hard) to 1.0 (soft) */
    bool  ring_mod_enabled;     /**< Ring mod with previous osc */
    float ring_mod_depth;       /**< 0.0–1.0 */

    // v1.7.1 Features
    bool  bitcrush_enabled;
    float bitcrush_depth;       /**< 0.0–1.0 (matrix modulatable) */
} PxOscillator;

/**
 * @enum PxOscillatorType
 * @brief Defines the type of oscillator used for sound generation.
 */
typedef enum {
    PX_OSC_TYPE_BYTECODE,  /**< Oscillator waveform is generated from a `px_vm` bytecode script. */
    PX_OSC_TYPE_SAMPLE,    /**< Oscillator uses a pre-recorded sample (future implementation). */
    PX_OSC_TYPE_GENERATED, /**< Oscillator uses a classic generated waveform (e.g., sine, square) (future implementation). */
    PX_OSC_TYPE_FM4OP      /**< A 4-operator FM synthesis oscillator (future implementation). */
} PxOscillatorType;

/**
 * @enum PxOscillatorUpdateMode
 * @brief Controls the quality and performance of the main oscillator by defining how often its waveform is recalculated.
 */
typedef enum {
    PX_OSC_UPDATE_MODE_PER_SAMPLE,  /**< Highest quality: recalculates the oscillator waveform on every single audio sample. Most CPU intensive. */
    PX_OSC_UPDATE_MODE_FIXED_RATE,  /**< Performance: recalculates at a fixed rate (e.g., 35kHz) and interpolates between points. Good balance of quality and performance. */
    PX_OSC_UPDATE_MODE_NYQUIST      /**< Dynamic Performance: recalculates at a multiple of the note's frequency (related to the Nyquist rate) and interpolates. Efficient for lower notes. */
} PxOscillatorUpdateMode;

/**
 * @enum PxADSRState
 * @brief Represents the current state of an ADSR (Attack, Decay, Sustain, Release) envelope. Useful for UI display.
 */
typedef enum {
    PX_ADSR_STATE_IDLE,    /**< The envelope is inactive and its output is 0. */
    PX_ADSR_STATE_ATTACK,  /**< The envelope is in the Attack phase, rising towards its peak level. */
    PX_ADSR_STATE_DECAY,   /**< The envelope is in the Decay phase, falling from the peak to the sustain level. */
    PX_ADSR_STATE_SUSTAIN, /**< The envelope is in the Sustain phase, holding at the sustain level. */
    PX_ADSR_STATE_RELEASE  /**< The envelope is in the Release phase, falling from its current level back to 0. */
} PxADSRState;

/**
 * @enum PxFilterMode
 * @brief Defines the different modes for the synthesizer's global filter.
 */
typedef enum {
    PX_FILTER_MODE_OFF,     /**< Filter is disabled. */
    PX_FILTER_MODE_LP,      /**< Low-Pass Filter. */
    PX_FILTER_MODE_BP,      /**< Band-Pass Filter. (In 1-pole mode: approximates an All-Pass response). */
    PX_FILTER_MODE_HP,      /**< High-Pass Filter. */
    PX_FILTER_MODE_LP_BP,   /**< A mix of Low-Pass and Band-Pass outputs. */
    PX_FILTER_MODE_LP_HP,   /**< A mix of Low-Pass and High-Pass outputs (creates a notch-like effect). */
    PX_FILTER_MODE_BP_HP,   /**< A mix of Band-Pass and High-Pass outputs. */
    PX_FILTER_MODE_NOTCH,   /**< Notch Filter (Band-Reject). (In 1-pole mode: passes input signal). */
    PX_FILTER_MODE_ALLPASS, /**< All-Pass Filter (for phase shifting effects). (In 1-pole mode: approximate). */
    PX_FILTER_MODE_COUNT    /**< The total number of filter modes. */
} PxFilterMode;

/**
 * @enum PxGlideMode
 * @brief Defines the pitch glide behavior for static (non-sequenced) playback.
 */
typedef enum {
    PX_GLIDE_OFF = 0,        /**< Instant pitch changes. */
    PX_GLIDE_STEP_LINEAR,    /**< Hard jump to new note, then linear interpolation over time (Legacy Unilegato). */
    PX_GLIDE_SMOOTH_RC       /**< Natural exponential glide (RC-filter style) for smooth portamento. */
} PxGlideMode;

typedef enum {
    PX_CURVE_LINEAR,       // Default: straight 0-1 mapping
    PX_CURVE_EXP,          // Exponential: sensitive low, steep high
    PX_CURVE_LOG,          // Logarithmic: steep low, sensitive high
    PX_CURVE_S,            // S-curve: ease-in/out for smooth response
    PX_CURVE_COUNT
} PxCurveType;

/**
 * @enum PxADSRDestination
 * @brief Defines the possible modulation destinations for a voice ADSR envelope.
 */
typedef enum {
    PX_ADSR_DEST_NONE,                /**< No modulation target. */
    PX_ADSR_DEST_PARAM1,              /**< Modulates the `modA` parameter in the waveform bytecode. */
    PX_ADSR_DEST_PARAM2,              /**< Modulates the `modB` parameter in the waveform bytecode. */
    PX_ADSR_DEST_PARAM3,              /**< Modulates the `modC` parameter in the waveform bytecode. */
    PX_ADSR_DEST_AMP,                 /**< Modulates the voice's amplitude (the primary use for an ADSR). */
    PX_ADSR_DEST_FREQUENCY,           /**< Modulates the voice's pitch (in semitones). */
    PX_ADSR_DEST_LFO0_OUTPUT_LEVEL,   /**< Modulates the output level of LFO 0. */
    PX_ADSR_DEST_LFO1_OUTPUT_LEVEL,   /**< Modulates the output level of LFO 1. */
    PX_ADSR_DEST_LFO2_OUTPUT_LEVEL,   /**< Modulates the output level of LFO 2. */
    PX_ADSR_DEST_FILTER_CUTOFF,       /**< Modulates the filter's cutoff frequency (in Hz). */
    PX_ADSR_DEST_FILTER_ENV_INPUT,    /**< Modulates the dedicated filter envelope input amount. */
    PX_ADSR_DEST_FILTER_RESONANCE,    /**< Modulates the filter's resonance (Q factor). */
    PX_ADSR_DEST_COUNT                /**< The total number of ADSR destinations. */
} PxADSRDestination;

/**
 * @enum PxLFODestination
 * @brief Defines the possible modulation destinations for an LFO.
 */
typedef enum {
    PX_LFO_DEST_NONE,           /**< No modulation target. */
    PX_LFO_DEST_PARAM1,         /**< Modulates the `modA` parameter in the waveform bytecode. */
    PX_LFO_DEST_PARAM2,         /**< Modulates the `modB` parameter in the waveform bytecode. */
    PX_LFO_DEST_PARAM3,         /**< Modulates the `modC` parameter in the waveform bytecode. */
    PX_LFO_DEST_FILTER_CUTOFF,  /**< Modulates the filter's cutoff frequency (in Hz). */
    PX_LFO_DEST_AMP,            /**< Modulates the voice's amplitude (for tremolo effects). */
    PX_LFO_DEST_PITCH,          /**< Modulates the voice's pitch (in semitones, for vibrato effects). */
    PX_LFO_DEST_PAN,            /**< Modulates the voice's stereo pan position (for auto-pan effects). */
    PX_LFO_DEST_COUNT           /**< The total number of LFO destinations. */
} PxLFODestination;

/**
 * @enum PxADSRParamType
 * @brief Identifies a specific parameter within an ADSR envelope.
 */
typedef enum {
    PX_ADSR_PARAM_ATTACK,   /**< The attack time parameter. */
    PX_ADSR_PARAM_DECAY,    /**< The decay time parameter. */
    PX_ADSR_PARAM_SUSTAIN,  /**< The sustain level parameter (0.0 to 1.0). */
    PX_ADSR_PARAM_RELEASE   /**< The release time parameter. */
} PxADSRParamType;

/**
 * @enum PxFilterParamType
 * @brief Identifies a specific parameter of the global filter.
 */
typedef enum {
    PX_FILTER_PARAM_CUTOFF,     /**< The filter's cutoff frequency (in Hz). */
    PX_FILTER_PARAM_RESONANCE,  /**< The filter's resonance (Q factor). (Ignored in 1-pole mode). */
    PX_FILTER_PARAM_ENV_AMOUNT, /**< The amount of modulation applied from ADSRs to the cutoff frequency. */
    PX_FILTER_PARAM_DRIVE,      /**< The amount of saturation/drive applied at the filter's input. */
    PX_FILTER_PARAM_KEYTRACK,   /**< The amount the note's pitch affects the cutoff frequency (0.0 to 1.0). */
    PX_FILTER_PARAM_POLES       /**< The number of poles (filter slope). The float value is rounded and clamped to an integer of 1 (6dB/oct), 2 (12dB/oct), 3 (18dB/oct), or 4 (24dB/oct). */
} PxFilterParamType;

/**
 * @enum PxLFOParamType
 * @brief Identifies a specific parameter of an LFO.
 */
typedef enum {
    PX_LFO_PARAM_FREQUENCY /**< The LFO's frequency (rate) in Hz. */
} PxLFOParamType;

// --- v1.5 Wave Sequencing Definitions ---

/**
 * @enum PxWseqAmpModType
 * @brief Defines the shape of the amplitude envelope applied during a Wave Sequence step.
 */
typedef enum {
    PX_WSEQ_AMP_RAMP_DOWN = 0, /**< Linear fade out (Pluck). */
    PX_WSEQ_AMP_RAMP_UP,       /**< Linear fade in (Reverse). */
    PX_WSEQ_AMP_TRIANGLE,      /**< Triangle (Swell/Fade). */
    PX_WSEQ_AMP_SINE,          /**< Sine Hump. */
    PX_WSEQ_AMP_SQUARE,        /**< 50% Gate (Chopper). */
    PX_WSEQ_AMP_PULSE_25,      /**< 25% Gate (Short blip). */
    PX_WSEQ_AMP_EXP_DOWN,      /**< Exponential Pluck (Percussive). */
    PX_WSEQ_AMP_RANDOM         /**< Random Level (S&H), latched once per step. */
} PxWseqAmpModType;

/**
 * @enum PxWSeqEndAction
 * @brief Determines the behavior when a wave sequence reaches its end or an explicit END flag.
 */
typedef enum {
    PX_WSEQ_END_STOP = 0,    /**< Stop the voice (triggers release behavior). */
    PX_WSEQ_END_HOLD,        /**< Hold the last step indefinitely (keeps voice active). */
    PX_WSEQ_END_LOOP,        /**< Loop back to the start of the sequence. */
    PX_WSEQ_END_PINGPONG,    /**< Reverse the playback direction. */
    PX_WSEQ_END_REVERSE      /**< Play backwards (typically used as a starting mode). */
} PxWSeqEndAction;

/**
 * @enum PxWSeqGlideMode
 * @brief Determines how pitch changes are handled between steps in a wave sequence.
 */
typedef enum {
    PX_WSEQ_GLIDE_OFF = 0,   /**< Instant pitch changes (stepped). */
    PX_WSEQ_GLIDE_STEP,      /**< Glide to the new pitch over the duration of the step. */
    PX_WSEQ_GLIDE_SMOOTH     /**< Continuous glide that ignores step boundaries. */
} PxWSeqGlideMode;

// 16-Bit Logic Flags (Per Step)
// --- FLOW CONTROL (Bits 0-3) ---
#define PX_WSEQ_END             (1 << 0)  /**< Force Sequence End Action at this step. */
#define PX_WSEQ_LOOP_POINT      (1 << 1)  /**< Marker for Loop Start (if supported by loop logic). */
#define PX_WSEQ_JUMP_RANDOM     (1 << 2)  /**< Jump to a random step in the sequence. */
#define PX_WSEQ_AMP_MOD         (1 << 3)  /**< Enable per-step Amplitude Modulation (Type defined in global settings). */

// --- MODULATION / RESET (Bits 4-7) ---
#define PX_WSEQ_RESET_LFO       (1 << 4)  /**< Reset LFO phases to 0. */
#define PX_WSEQ_RETRIG_ADSR     (1 << 5)  /**< Retrigger ADSR envelopes to the phase specified in global settings. */
#define PX_WSEQ_LOCK_PHASE      (1 << 6)  /**< Hard Sync the oscillator phase to 0. */
#define PX_WSEQ_GLIDE           (1 << 7)  /**< Enable per-step exponential glide (v1.7.1). */

// --- GENERATIVE (Bits 8-11) ---
#define PX_WSEQ_USE_PROB_MUTE   (1 << 8)  /**< Use the global 'prob_mute_score' to randomly mute this step (rolled once per step load). */
#define PX_WSEQ_USE_PROB_SKIP   (1 << 9)  /**< Use the global 'prob_skip_score' to randomly skip this step (rolled once per step load). */
#define PX_WSEQ_USE_RND_OCTAVE  (1 << 10) /**< Use the global 'rnd_octave_range' to apply a random octave shift. */
#define PX_WSEQ_USE_RND_WAVE    (1 << 11) /**< Use the global Random Wave Range to pick a random waveform. */

// --- GLITCH / TIMBRE (Bits 12-15) ---
#define PX_WSEQ_REVERSE_PLAY    (1 << 12) /**< Play the waveform backwards (negative frequency). */
#define PX_WSEQ_BITCRUSH        (1 << 13) /**< Enable Bitcrush effect using global depth. */
#define PX_WSEQ_XMOD            (1 << 14) /**< Enable Cross-Modulation (FM) using global settings. */
#define PX_WSEQ_RING_MOD        (1 << 15) /**< Enable Ring Modulation using global settings. */

/**
 * @struct PxWaveSeqStep
 * @brief Represents a single step in a Wave Sequence.
 * @note Strictly 8-byte aligned for ROM efficiency.
 */
typedef struct {
    uint16_t wave_idx;        /**< The waveform index (0-65535). */
    uint16_t duration_cycles; /**< Duration of the step in oscillator cycles (1-65535). Values < 1 clamped to 1. */
    int16_t  pitch_offset;    /**< Pitch offset in cents (-32768 to +32767). 1200 = 1 Octave. */
    uint16_t flags;           /**< Bitwise logic flags (PX_WSEQ_*). */
} PxWaveSeqStep;

#define PX_MAX_WSEQ_STEPS 64
#define PX_NUM_WSEQ_BANKS 256
#define PX_OSC_MOD_PARAM_COUNT 11

/**
 * @struct PxWaveSequence
 * @brief Defines a complete Wave Sequence, including global settings and step data.
 */
typedef struct {
    // --- Global Settings Header ---
    const char* name;             /**< Descriptive name of the sequence. */
    uint8_t  end_action;          /**< PxWSeqEndAction: What to do when the sequence ends. (0-4) */
    uint8_t  glide_mode;          /**< PxWSeqGlideMode: Pitch glide behavior. (0-2) */
    uint8_t  adsr_retrig_phase;   /**< PxADSRState: Target phase on retrigger (1=ATTACK, etc). */

    uint8_t  prob_mute_score;     /**< 0-100%: Probability of muting step. 0 defaults to 50%. */
    uint8_t  prob_skip_score;     /**< 0-100%: Probability of skipping step. 0 defaults to 50%. */
    uint8_t  rnd_octave_range;    /**< 0-100%: Probability of octave shift. */
    uint8_t  reset_lfo_pos;       /**< Boolean (1=Yes, 0=No): Reset LFOs when sequence starts. */

    uint16_t rnd_wave_low;        /**< Low index for random wave selection (0-65535). */
    uint16_t rnd_wave_high;       /**< High index for random wave selection (0-65535). */

    // Modulation Sources (PxModSource cast to int8_t, -1 for None)
    int8_t   lock_phase_mod_src;  /**< Mod Source for Phase Lock (-1 to 6). */
    int8_t   xmod_mod_src;        /**< Mod Source for XMod (FM) (-1 to 6). */
    int8_t   ring_mod_mod_src;    /**< Mod Source for Ring Mod (-1 to 6). */
    int8_t   amp_mod_type;        /**< PxWseqAmpModType: Shape of amplitude envelope (0-7). */

    float    xmod_depth;          /**< Base amount for XMod (0.0 to 1.0). */
    float    ring_mod_depth;      /**< Base amount for Ring Mod (0.0 to 1.0). */

    // --- Steps ---
    PxWaveSeqStep steps[PX_MAX_WSEQ_STEPS];
} PxWaveSequence;

/**
 * @struct PxADSRParams
 * @brief Holds the configuration for an ADSR envelope.
 */
typedef struct {
    float attack_time;    /**< Attack time in seconds. */
    float decay_time;     /**< Decay time in seconds. */
    float sustain_level;  /**< Sustain level, clamped between 0.0 and 1.0. */
    float release_time;   /**< Release time in seconds. */
    bool enabled;         /**< Whether the ADSR is active. */
} PxADSRParams;

/**
 * @struct PxLFOParams
 * @brief Holds the configuration for a Low-Frequency Oscillator (LFO).
 */
typedef struct {
    int wave_idx;                           /**< The index of the waveform used by the LFO. */
    float frequency;                        /**< The LFO's rate in Hz. */
    bool enabled;                           /**< Whether the LFO is active. */
    bool reset_on_key_on;                   /**< If true, the LFO's phase resets to 0 when a new note is triggered. */
    PxADSRParams adsr;                      /**< An internal ADSR envelope that can shape the LFO's output level over time. */
    float mod_amounts[PX_LFO_DEST_COUNT];   /**< An array specifying the modulation amount for each possible destination. */
} PxLFOParams;

#ifndef NUM_WAVEFORMS
    #define NUM_WAVEFORMS 256
    #define NUM_WAVEFORMS 256
#endif

// --- Configuration and Patch Structures ---

/**
 * @struct PxConfig
 * @brief Configuration settings for creating a new synthesizer instance.
 * @details This structure is passed to `PX_Create` to define the synthesizer's
 * fundamental properties, such as the number of voices, LFOs, and the audio sample rate.
 */
typedef struct PxConfig {
    int num_voices;                     /**< The maximum number of simultaneous voices (e.g., 16). */
    int num_lfos;                       /**< The number of global Low-Frequency Oscillators (LFOs) (e.g., 3). */
    int num_voice_adsrs;                /**< The number of ADSR envelopes available per voice (e.g., 3). */
    float sample_rate;                  /**< The audio sample rate in Hz (e.g., 44100.0f). */
    int samples_per_lfo_update;         /**< The number of audio samples to process between each LFO update. */
    float lfo_update_interval_ms;       /**< The time in milliseconds between LFO value updates for performance (e.g., 1.0f). */
    PxOscillatorUpdateMode osc_update_mode; /**< The quality/performance mode for the main voice oscillators. */
    float osc_fixed_update_rate_hz;     /**< The update rate for oscillators in `PX_OSC_UPDATE_MODE_FIXED_RATE`. */
    float nyquist_precision_multiplier; /**< The multiplier for the dynamic update rate in `PX_OSC_UPDATE_MODE_NYQUIST`. */
    bool use_gpu;                       /**< If true, attempts to use GPU acceleration for waveform generation. Requires POLYSONIX_USE_GPU define. */
} PxConfig;

/**
 * @struct PxLimiterInfo
 * @brief A snapshot of the master limiter's real-time state.
 * @details This struct provides read-only information about the limiter's current
 * performance, primarily for UI display purposes.
 */
typedef struct PxLimiterInfo {
    bool initialized;           /**< `true` if the limiter has been successfully initialized. */
    float gain_reduction_db;    /**< The current amount of gain reduction being applied, in decibels (a non-negative value). */
} PxLimiterInfo;

/**
 * @struct PxWaveInfo
 * @brief Information about a specific waveform definition.
 * @details Provides metadata about a waveform, such as its name and compilation status.
 */
typedef struct PxWaveInfo {
    const char* name;   /**< The descriptive name of the waveform. */
    bool is_compiled;   /**< `true` if the waveform's script has been successfully compiled into bytecode. */
} PxWaveInfo;

/**
 * @struct PxLFOInfo
 * @brief A snapshot of a global LFO's real-time state.
 * @details This struct provides a read-only view of an LFO's current internal state,
 * intended for UI visualization (e.g., drawing an oscilloscope or monitoring values).
 */
typedef struct PxLFOInfo {
    bool enabled;           /**< `true` if this LFO is currently active. */
    int wave_idx;           /**< The index of the waveform this LFO is using. */
    float frequency;        /**< The current frequency (rate) of the LFO in Hz. */
    bool reset_on_key_on;   /**< `true` if the LFO's phase resets on a new note event. */
    bool adsr_enabled;      /**< `true` if the LFO's internal ADSR is active. */
    float adsr_level;       /**< The current output level of the LFO's internal ADSR (0.0 to 1.0). */
    float phase;            /**< The LFO's current phase, from 0.0 to 1.0. */
    float raw_output;       /**< The direct, unmodified output of the LFO's waveform (-1.0 to 1.0). */
    float final_output;     /**< The final output after being shaped by its internal ADSR. */
} PxLFOInfo;

/**
 * @struct PxVoiceInfo
 * @brief A snapshot of a single voice's real-time state.
 * @details This struct provides a read-only view of a voice's most important
 * parameters, primarily for UI display and debugging. All values represent the state
 * at the moment the info was requested.
 */
typedef struct PxVoiceInfo {
    bool active;                /**< `true` if the voice is currently playing or in its release phase. */
    int midi_note;              /**< The MIDI note number this voice is playing. */
    float frequency;            /**< The current, modulated frequency of the voice's oscillator in Hz. */
    float pan_position;         /**< The current stereo pan position (-1.0 for left, 1.0 for right). */
    float effective_amplitude;  /**< The final amplitude of the voice after all modulations. */
    PxADSRState adsr_states[3]; /**< The current state (e.g., ATTACK, SUSTAIN) of the first 3 voice ADSRs. */
    float adsr_levels[3];       /**< The current output level (0.0 to 1.0) of the first 3 voice ADSRs. */
    float lfo_outputs[3];       /**< The current final output value of the global LFOs. */
    int active_wave_indices[3]; /**< The current waveform index being played by each oscillator (monitoring Wave Sequencing). */
} PxVoiceInfo;

// --- Core API Functions ---

/**
 * @brief Creates and initializes a new synthesizer instance.
 * @details This function allocates all necessary memory for the synthesizer, including voices, LFOs, and internal buffers,
 * based on the provided configuration. It sets up a default patch and prepares the synth for processing.
 * @param config A pointer to a `PxConfig` struct containing the desired configuration (e.g., sample rate, number of voices).
 * @return A pointer to the newly created `PxSynth` instance, or `NULL` on failure (e.g., invalid config, memory allocation error).
 */
PX_API PxSynth*    PX_Create(const PxConfig* config);

/**
 * @brief Destroys a synthesizer instance and frees all associated memory.
 * @param s A pointer to the `PxSynth` instance to be destroyed.
 */
PX_API void        PX_Destroy(PxSynth* s);

/**
 * @brief Processes a block of audio.
 * @details This is the main audio processing function. It should be called from an audio callback. It generates audio samples
 * for all active voices, applies effects (filter, limiter), and fills the provided buffer. This function is real-time safe.
 * @param s A pointer to the `PxSynth` instance.
 * @param stereo_buffer A pointer to a stereo buffer to be filled with signed 16-bit audio samples (interleaved L/R).
 * @param num_frames The number of stereo frames (i.e., sample pairs) to process. For a buffer containing 512 total samples, this value would be 256.
 */
PX_API void        PX_Process(PxSynth* s, float* stereo_buffer, int num_frames);

/**
 * @brief Triggers a new note to be played.
 * @details This function is thread-safe and sends a command to the audio thread to start a new note.
 * @param s A pointer to the `PxSynth` instance.
 * @param midi_note The MIDI note number (0-127) of the note to play.
 * @param wave_idx The index of the waveform to be used for the note's oscillator.
 * @param key_id A unique integer identifier for this note event. This ID is used to track the note so it can be released later with `PX_NoteOff`.
 * @param velocity The velocity of the note (0.0 to 1.0).
 */
PX_API void        PX_NoteOn(PxSynth* s, int midi_note, int wave_idx, int key_id, float velocity); // v1.1: now with velocity
PX_API void        PX_NoteOnLegacy(PxSynth* s, int midi_note, int wave_idx, int key_id); // Optional: explicit old version

PX_API void        PX_PolyAftertouch(PxSynth* s, int key_id, float pressure); // 0.0 to 1.0

PX_API void        PX_ChannelAftertouch(PxSynth* s, float pressure); // 0.0 to 1.0

// --- Oscillator Parameters (v1.6) ---

/**
 * @brief Enables or disables a specific oscillator.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @param enabled True to enable, false to disable
 */
PX_API void        PX_SetOscEnabled(PxSynth* s, int osc_idx, bool enabled);

/**
 * @brief Checks if a specific oscillator is enabled.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @return True if enabled, false otherwise
 */
PX_API bool        PX_GetOscEnabled(PxSynth* s, int osc_idx);

/**
 * @brief Sets the waveform for a specific oscillator.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @param wave_idx Index of the waveform (0 to NUM_WAVEFORMS-1)
 */
PX_API void        PX_SetOscWave(PxSynth* s, int osc_idx, int wave_idx);

/**
 * @brief Gets the current waveform index for a specific oscillator.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @return Waveform index
 */
PX_API int         PX_GetOscWave(PxSynth* s, int osc_idx);

/**
 * @brief Sets coarse tuning for an oscillator in semitones.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @param semitones Tuning offset (-24.0 to +24.0)
 */
PX_API void        PX_SetOscCoarseTune(PxSynth* s, int osc_idx, float semitones);

/**
 * @brief Gets coarse tuning for an oscillator in semitones.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @return Tuning offset in semitones
 */
PX_API float       PX_GetOscCoarseTune(PxSynth* s, int osc_idx);

/**
 * @brief Sets fine tuning for an oscillator in cents.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @param cents Fine tuning offset (-100.0 to +100.0)
 */
PX_API void        PX_SetOscFineTune(PxSynth* s, int osc_idx, float cents);

/**
 * @brief Gets fine tuning for an oscillator in cents.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @return Fine tuning offset in cents
 */
PX_API float       PX_GetOscFineTune(PxSynth* s, int osc_idx);

/**
 * @brief Sets the output mix level for an oscillator.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @param level Mix level (0.0 to 1.0)
 */
PX_API void        PX_SetOscMix(PxSynth* s, int osc_idx, float level);

/**
 * @brief Gets the output mix level for an oscillator.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @return Mix level (0.0 to 1.0)
 */
PX_API float       PX_GetOscMix(PxSynth* s, int osc_idx);

/**
 * @brief Sets the stereo pan position for an oscillator.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @param pan Pan position (-1.0 Left to +1.0 Right)
 */
PX_API void        PX_SetOscPan(PxSynth* s, int osc_idx, float pan);

/**
 * @brief Gets the stereo pan position for an oscillator.
 * @param s Synth instance
 * @param osc_idx Oscillator index (0-2)
 * @return Pan position (-1.0 to +1.0)
 */
PX_API float       PX_GetOscPan(PxSynth* s, int osc_idx);

/**
 * @brief Sets the active Wave Sequence ID for a specific oscillator.
 * @param s A pointer to the `PxSynth` instance.
 * @param osc_idx The oscillator index (0-2).
 * @param seq_id The Wave Sequence ID (0-127), or -1 to disable sequencing.
 */
PX_API void        PX_SetOscSequence(PxSynth* s, int osc_idx, int seq_id);
PX_API int         PX_GetOscSequence(PxSynth* s, int osc_idx);

// --- v1.7 Oscillator Features ---
PX_API void        PX_SetOscCrossMod(PxSynth* s, int osc_idx, bool enabled, float depth);
PX_API float       PX_GetOscCrossMod(PxSynth* s, int osc_idx);
PX_API bool        PX_GetOscCrossModEnabled(PxSynth* s, int osc_idx);

PX_API void        PX_SetOscPhaseDist(PxSynth* s, int osc_idx, bool enabled, float amount);
PX_API float       PX_GetOscPhaseDist(PxSynth* s, int osc_idx);
PX_API bool        PX_GetOscPhaseDistEnabled(PxSynth* s, int osc_idx);

PX_API void        PX_SetOscSync(PxSynth* s, int osc_idx, bool enabled, float softness);
PX_API float       PX_GetOscSync(PxSynth* s, int osc_idx);
PX_API bool        PX_GetOscSyncEnabled(PxSynth* s, int osc_idx);

PX_API void        PX_SetOscRingMod(PxSynth* s, int osc_idx, bool enabled, float depth);
PX_API float       PX_GetOscRingMod(PxSynth* s, int osc_idx);
PX_API bool        PX_GetOscRingModEnabled(PxSynth* s, int osc_idx);

PX_API void        PX_SetVelocityToAmp(PxSynth* s, float amount);
PX_API void        PX_SetVelocityToFilterCutoff(PxSynth* s, float hz_amount);
PX_API void        PX_SetVelocityAttackScaling(PxSynth* s, float scale);
PX_API void        PX_SetVelocityToParam1(PxSynth* s, float amount);

PX_API void        PX_SetAftertouchToFilterCutoff(PxSynth* s, float hz_amount);
PX_API void        PX_SetAftertouchToVibrato(PxSynth* s, float semitones);

PX_API float       PX_GetVelocityToAmp(PxSynth* s);
PX_API float       PX_GetVelocityToFilterCutoff(PxSynth* s);
PX_API float       PX_GetVelocityAttackScaling(PxSynth* s);
PX_API float       PX_GetVelocityToParam1(PxSynth* s);
PX_API float       PX_GetAftertouchToFilterCutoff(PxSynth* s);
PX_API float       PX_GetAftertouchToVibrato(PxSynth* s);

// --- v1.8 Time-Locked Wave Sequencing ---
PX_API void        PX_SetWSeqFixedTime(PxSynth* s, bool enabled);
PX_API bool        PX_GetWSeqFixedTime(PxSynth* s);
PX_API void        PX_SetWSeqRefFreq(PxSynth* s, float freq);
PX_API float       PX_GetWSeqRefFreq(PxSynth* s);

// --- v1.8.3 Static Glide / Portamento ---
PX_API void        PX_SetGlideMode(PxSynth* s, PxGlideMode mode);
PX_API PxGlideMode PX_GetGlideMode(PxSynth* s);
PX_API void        PX_SetGlideTime(PxSynth* s, float time_s);
PX_API float       PX_GetGlideTime(PxSynth* s);
PX_API void        PX_SetGlideLegatoOnly(PxSynth* s, bool legato_only);
PX_API bool        PX_GetGlideLegatoOnly(PxSynth* s);
PX_API void        PX_SetGlideAlways(PxSynth* s, bool always);
PX_API bool        PX_GetGlideAlways(PxSynth* s);

PX_API void        PX_SetPatchName(PxSynth* s, const char* name);
PX_API const char* PX_GetPatchName(PxSynth* s);

/**
 * @brief Releases a note that was previously triggered.
 * @details This function is thread-safe and sends a command to the audio thread to begin the release phase of the note's envelopes.
 * @param s A pointer to the `PxSynth` instance.
 * @param key_id The unique integer identifier that was used to trigger the note in `PX_NoteOn`.
 */
PX_API void        PX_NoteOff(PxSynth* s, int key_id);


// --- Voice ADSR Parameters ---

/**
 * @brief Sets a core parameter for a specific voice ADSR template.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the voice ADSR template to modify.
 * @param p The type of parameter to set (e.g., `PX_ADSR_PARAM_ATTACK`).
 * @param v The new value for the parameter.
 */
PX_API void        PX_SetVoiceADSRParam(PxSynth* s, int idx, PxADSRParamType p, float v);

/**
 * @brief Gets a core parameter for a specific voice ADSR template.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the voice ADSR template.
 * @param p The type of parameter to get.
 * @return The current value of the parameter.
 */
PX_API float       PX_GetVoiceADSRParam(PxSynth* s, int idx, PxADSRParamType p);

/**
 * @brief Enables or disables a specific voice ADSR template.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the voice ADSR template.
 * @param enabled `true` to enable, `false` to disable.
 */
PX_API void        PX_SetVoiceADSREnabled(PxSynth* s, int idx, bool enabled);

/**
 * @brief Checks if a specific voice ADSR template is enabled.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the voice ADSR template.
 * @return `true` if enabled, `false` otherwise.
 */
PX_API bool        PX_GetVoiceADSREnabled(PxSynth* s, int idx);

/**
 * @brief Sets the modulation amount from a voice ADSR to a specific destination.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the source voice ADSR template.
 * @param d The modulation destination.
 * @param v The modulation amount (typically -1.0 to 1.0, but can vary by destination).
 */
PX_API void        PX_SetVoiceADSRModAmount(PxSynth* s, int idx, PxADSRDestination d, float v);

/**
 * @brief Gets the modulation amount from a voice ADSR to a specific destination.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the source voice ADSR template.
 * @param d The modulation destination.
 * @return The current modulation amount.
 */
PX_API float       PX_GetVoiceADSRModAmount(PxSynth* s, int idx, PxADSRDestination d);


// --- LFO Core Parameters ---

/**
 * @brief Sets a core parameter for a specific LFO.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO to modify.
 * @param p The type of parameter to set (e.g., `PX_LFO_PARAM_FREQUENCY`).
 * @param v The new value for the parameter.
 */
PX_API void        PX_SetLFOParam(PxSynth* s, int idx, PxLFOParamType p, float v);

/**
 * @brief Gets a core parameter for a specific LFO.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @param p The type of parameter to get.
 * @return The current value of the parameter.
 */
PX_API float       PX_GetLFOParam(PxSynth* s, int idx, PxLFOParamType p);

/**
 * @brief Sets the waveform for an LFO.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @param wave_idx The index of the waveform to use.
 */
PX_API void        PX_SetLFOWaveform(PxSynth* s, int idx, int wave_idx);

/**
 * @brief Gets the current waveform index for an LFO.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @return The waveform index.
 */
PX_API int         PX_GetLFOWaveform(PxSynth* s, int idx);

/**
 * @brief Enables or disables an LFO.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @param enabled `true` to enable, `false` to disable.
 */
PX_API void        PX_SetLFOEnabled(PxSynth* s, int idx, bool enabled);

/**
 * @brief Checks if an LFO is enabled.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @return `true` if enabled, `false` otherwise.
 */
PX_API bool        PX_GetLFOEnabled(PxSynth* s, int idx);

/**
 * @brief Sets whether an LFO's phase should reset when a new note is triggered.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @param reset `true` to enable phase reset, `false` otherwise.
 */
PX_API void        PX_SetLFOResetOnKeyOn(PxSynth* s, int idx, bool reset);

/**
 * @brief Checks if an LFO's phase is set to reset on note on.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @return `true` if phase reset is enabled, `false` otherwise.
 */
PX_API bool        PX_GetLFOResetOnKeyOn(PxSynth* s, int idx);


// --- LFO ADSR Parameters ---

/**
 * @brief Sets a core parameter for an LFO's internal ADSR envelope.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO whose ADSR will be modified.
 * @param p The type of ADSR parameter to set.
 * @param v The new value for the parameter.
 */
PX_API void        PX_SetLFOADSRParam(PxSynth* s, int idx, PxADSRParamType p, float v);

/**
 * @brief Gets a core parameter from an LFO's internal ADSR envelope.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @param p The type of ADSR parameter to get.
 * @return The current value of the parameter.
 */
PX_API float       PX_GetLFOADSRParam(PxSynth* s, int idx, PxADSRParamType p);

/**
 * @brief Enables or disables an LFO's internal ADSR envelope.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @param enabled `true` to enable, `false` to disable.
 */
PX_API void        PX_SetLFOADSREnabled(PxSynth* s, int idx, bool enabled);

/**
 * @brief Checks if an LFO's internal ADSR envelope is enabled.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the LFO.
 * @return `true` if the ADSR is enabled, `false` otherwise.
 */
PX_API bool        PX_GetLFOADSREnabled(PxSynth* s, int idx);

/**
 * @brief Gets a snapshot of an LFO's real-time state for UI display.
 * @param s A pointer to the `PxSynth` instance.
 * @param lfo_idx The index of the LFO.
 * @return A `PxLFOInfo` struct containing the LFO's current state.
 */
PX_API PxLFOInfo   PX_GetLFOInfo(PxSynth* s, int lfo_idx);


// --- LFO Routing ---

/**
 * @brief Sets the modulation amount from an LFO to a specific destination.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the source LFO.
 * @param d The modulation destination.
 * @param v The modulation amount.
 */
PX_API void        PX_SetLFOModAmount(PxSynth* s, int idx, PxLFODestination d, float v);

/**
 * @brief Gets the modulation amount from an LFO to a specific destination.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the source LFO.
 * @param d The modulation destination.
 * @return The current modulation amount.
 */
PX_API float       PX_GetLFOModAmount(PxSynth* s, int idx, PxLFODestination d);

/**
 * @brief Sets the update interval for all LFOs.
 * @details LFOs are updated at a lower rate than the audio sample rate for performance. This sets that rate.
 * @param s A pointer to the `PxSynth` instance.
 * @param interval_ms The update interval in milliseconds.
 */
PX_API void        PX_SetLFOUpdateInterval(PxSynth* s, float interval_ms);

/**
 * @brief Gets the update interval for all LFOs.
 * @param s A pointer to the `PxSynth` instance.
 * @return The update interval in milliseconds.
 */
PX_API float       PX_GetLFOUpdateInterval(PxSynth* s);


// --- Unilegato ---

/**
 * @brief Enables or disables the unilegato/portamento mode.
 * @param s A pointer to the `PxSynth` instance.
 * @param enabled `true` to enable unilegato, `false` to disable.
 */
PX_API void        PX_SetUnilegatoEnabled(PxSynth* s, bool enabled);

/**
 * @brief Checks if unilegato mode is enabled.
 * @param s A pointer to the `PxSynth` instance.
 * @return `true` if unilegato is enabled, `false` otherwise.
 */
PX_API bool        PX_GetUnilegatoEnabled(PxSynth* s);

/**
 * @brief Sets the slide time for unilegato/portamento.
 * @param s A pointer to the `PxSynth` instance.
 * @param duration_s The slide duration in seconds.
 */
PX_API void        PX_SetUnilegatoSlideTime(PxSynth* s, float duration_s);

/**
 * @brief Gets the slide time for unilegato/portamento.
 * @param s A pointer to the `PxSynth` instance.
 * @return The slide duration in seconds.
 */
PX_API float       PX_GetUnilegatoSlideTime(PxSynth* s);


// --- Filter Parameters ---

/**
 * @brief Sets a core parameter for the global filter.
 * @param s A pointer to the `PxSynth` instance.
 * @param p The type of filter parameter to set.
 * @param v The new value for the parameter.
 */
PX_API void        PX_SetFilterParam(PxSynth* s, PxFilterParamType p, float v);

/**
 * @brief Gets a core parameter for the global filter.
 * @param s A pointer to the `PxSynth` instance.
 * @param p The type of filter parameter to get.
 * @return The current value of the parameter.
 */
PX_API float       PX_GetFilterParam(PxSynth* s, PxFilterParamType p);

/**
 * @brief Sets the filter mode (e.g., Low-pass, Band-pass).
 * @param s A pointer to the `PxSynth` instance.
 * @param mode The desired filter mode.
 */
PX_API void        PX_SetFilterMode(PxSynth* s, PxFilterMode mode);

/**
 * @brief Gets the current filter mode.
 * @param s A pointer to the `PxSynth` instance.
 * @return The current `PxFilterMode`.
 */
PX_API PxFilterMode PX_GetFilterMode(PxSynth* s);

// --- Global Post-Filter API (v1.4.4) ---

PX_API void PX_SetGlobalFilterEnabled(PxSynth* s, bool enabled);
PX_API bool PX_GetGlobalFilterEnabled(PxSynth* s);

PX_API void PX_SetGlobalFilterParam(PxSynth* s, PxFilterParamType p, float v);
PX_API float PX_GetGlobalFilterParam(PxSynth* s, PxFilterParamType p);

PX_API void PX_SetGlobalFilterMode(PxSynth* s, PxFilterMode mode);
PX_API PxFilterMode PX_GetGlobalFilterMode(PxSynth* s);


// --- Global & Limiter Parameters ---

/**
 * @brief Sets the base stereo pan position for all new voices.
 * @param s A pointer to the `PxSynth` instance.
 * @param pan The pan position, from -1.0 (full left) to 1.0 (full right).
 */
PX_API void        PX_SetGlobalVoicePan(PxSynth* s, float pan);

/**
 * @brief Gets the base stereo pan position.
 * @param s A pointer to the `PxSynth` instance.
 * @return The current pan position.
 */
PX_API float       PX_GetGlobalVoicePan(PxSynth* s);

/**
 * @brief Sets the threshold for the master bus limiter.
 * @param s A pointer to the `PxSynth` instance.
 * @param threshold The threshold in linear amplitude (0.0 to 1.0).
 */
PX_API void        PX_SetLimiterThreshold(PxSynth* s, float threshold);

/**
 * @brief Gets the threshold for the master bus limiter.
 * @param s A pointer to the `PxSynth` instance.
 * @return The current limiter threshold.
 */
PX_API float       PX_GetLimiterThreshold(PxSynth* s);

/**
 * @brief Sets the release time for the master bus limiter.
 * @param s A pointer to the `PxSynth` instance.
 * @param release_ms The release time in milliseconds.
 */
PX_API void        PX_SetLimiterRelease(PxSynth* s, float release_ms);

/**
 * @brief Gets the release time for the master bus limiter.
 * @param s A pointer to the `PxSynth` instance.
 * @return The current limiter release time in milliseconds.
 */
PX_API float       PX_GetLimiterRelease(PxSynth* s);


// --- UI Helper / Info Functions ---

/**
 * @brief Gets a snapshot of a voice's real-time state for UI display.
 * @param s A pointer to the `PxSynth` instance.
 * @param idx The index of the voice to inspect.
 * @return A `PxVoiceInfo` struct containing the voice's current state.
 */
PX_API PxVoiceInfo PX_GetVoiceInfo(PxSynth* s, int idx);

/**
 * @brief Gets a snapshot of the limiter's real-time state.
 * @param s A pointer to the `PxSynth` instance.
 * @return A `PxLimiterInfo` struct containing the limiter's current state (e.g., gain reduction).
 */
PX_API PxLimiterInfo PX_GetLimiterInfo(PxSynth* s);

/**
 * @brief Gets the total number of available waveforms.
 * @return The number of waveforms.
 */
PX_API int         PX_GetNumWaveforms();

/**
 * @brief Gets information about a specific waveform.
 * @param idx The index of the waveform.
 * @return A `PxWaveInfo` struct containing the waveform's name and compiled status.
 */
PX_API PxWaveInfo  PX_GetWaveInfo(int idx);

/**
 * @brief Gets the string representation of a filter mode.
 * @param mode The filter mode enum.
 * @return A constant string with the name of the filter mode (e.g., "LP", "HP").
 */
PX_API const char* PX_GetFilterModeName(PxFilterMode mode);

/**
 * @brief Gets the string representation of an ADSR destination.
 * @param d The ADSR destination enum.
 * @return A constant string with the name of the destination.
 */
PX_API const char* PX_GetADSRDestinationName(PxADSRDestination d);

/**
 * @brief Gets the string representation of an LFO destination.
 * @param d The LFO destination enum.
 * @return A constant string with the name of the destination.
 */
PX_API const char* PX_GetLFODestinationName(PxLFODestination d);

/**
 * @brief Gets the string representation of an ADSR state.
 * @param state The ADSR state enum.
 * @return A constant string with the name of the state (e.g., "ATTACK", "IDLE").
 */
PX_API const char* PX_GetADSRStateName(PxADSRState state);

#endif // POLYSONIX_H

#ifdef POLYSONIX_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// --- Internal Constants ---
#define ADSR_STATE_IDLE     0
#define ADSR_STATE_ATTACK   1
#define ADSR_STATE_DECAY    2
#define ADSR_STATE_SUSTAIN  3
#define ADSR_STATE_RELEASE  4

// --- Internal Constants & Data ---
#define MAX_VOICES 16
#define MAX_LFOS 3 // For snapshot array sizing
#define CMD_QUEUE_SIZE 1024
#define MIN_ADSR_TIME 0.001f
#define EXP_DECAY_TARGET 0.001f
#define INAUDIBLE_AMPLITUDE_THRESHOLD 0.001f
#define INAUDIBLE_ADSR_LEVEL 0.0001f
#define DEFAULT_NYQUIST_MULTIPLIER 8.0f
#define DEFAULT_OSC_FIXED_UPDATE_RATE_HZ 35000.0f

#ifndef NUM_WAVEFORMS
    #define NUM_WAVEFORMS 212
#endif

#define SOFTCLIP_RATIO 0.8f

static const float HB[5] = {
    0.036681502163648017f,   /* h[-2] */
   -0.136728736584138680f,   /* h[-1] */
    0.613659362617572050f,   /* h[ 0] */
    0.613659362617572050f,   /* h[ 1] */
   -0.136728736584138680f    /* h[ 2] */
};

// --- v1.5 ROM (Populated) ---
#define PX_WSEQ_ROM_IMPLEMENTATION
#include "px_wseq_rom.h"

#define PX_WAVE_ROM_IMPLEMENTATION
#include "px_wave_rom.h"

static const char* PX_FILTER_MODE_NAMES[] = {"OFF", "LP", "BP", "HP", "LP+BP", "LP+HP", "BP+HP", "NOTCH", "ALLPASS"};
static const char* PX_ADSR_DEST_NAMES[] = { "NONE", "PARAM1", "PARAM2", "PARAM3", "AMP", "FREQ(ST)", "LFO0 LVL", "LFO1 LVL", "LFO2 LVL", "FREQ.CUT(HZ)", "FREQ.ENV IN", "FREQ.RES(Q)"};
static const char* PX_LFO_DEST_NAMES[] = {"NONE", "PARAM1", "PARAM2", "PARAM3", "FREQ CUT(HZ)", "AMP", "PITCH(ST)", "PAN"};
static const char* PX_ADSR_STATE_NAMES[] = {"IDLE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"};

// --- Internal Data Structures ---

typedef struct {
    float attack_time, decay_time, sustain_level, release_time;
    int   state;
    float level;
    float attack_rate, decay_multiplier, release_multiplier;
    bool enabled;
} ADSR;

typedef struct {
    float threshold;
    float ratio;
    float attack_coeff;
    float release_coeff;
    float makeup_gain;
    float envelope;
    float* delay_line_l;
    float* delay_line_r;
    int delay_write_pos;
    int delay_samples;
    int buffer_capacity;
    float smooth_gain;
    float target_gain;
    float peak_hold;
    int peak_hold_samples;
    bool initialized;
    float release_ms_cache;
} EnhancedLimiter;

typedef struct {
    float lp_state1, bp_state1;                             // State for the first 2-pole filter stage
    float lp_state2, bp_state2;                             // State for the second 2-pole filter stage (for 24dB)
    float pole3_lp_state, pole3_bp_state, pole3_hp_state;   // State for the additional 1-pole stage (for 18dB)

    float f_coeff;
    float q_inv_coeff;
    float pole3_coeff; // Coefficient for the 1-pole stage

    // v1.4.3: Parallel 1-pole states for combo modes at 6 dB
    float combo_lp_state;
    float combo_hp_state;
    PxFilterMode current_mode;
    int poles; // Will be 2, 3, or 4
    float drive;

    // DC blocker state
    float dc_block_x1;
    float dc_block_y1;
    float os_x1, os_x2;   /* 2-sample delay for half-band FIR */
} Filter;

typedef struct {
    int wave_idx;
    float frequency;
    bool enabled;
    bool reset_on_key_on;
    float phase;
    ADSR adsr;
    float current_output_value;
    float current_raw_output_value;
    VmParams lfo_vm_params;
} LFOInstance;

typedef struct {
    int  seq_id;
    int  step_idx;
    int  direction;
    uint32_t cycles_counter;
    uint32_t current_step_target_cycles; // v1.8: Cached target
    bool finished;
    int  loop_start_idx;
    const PxWaveSequence* current_sequence;

    uint16_t step_flags;
    float    step_pitch_ratio;
    float    target_pitch_ratio;
    float    prev_step_pitch_ratio;
    float    step_amp_mod_val;
    bool     step_mute_state;
    float    cycle_accumulator; /**< Fractional carry-over for timing precision */
} PxSeqState;

typedef struct {
    bool active;
    int midi_note;
    float frequency, original_frequency;

    // v1.6: Per-oscillator state
    float osc_phase[PX_MAX_OSC_PER_VOICE];
    int   osc_wave_indices[PX_MAX_OSC_PER_VOICE];

    // v1.7: Per-oscillator output cache & Sync
    float osc_output[PX_MAX_OSC_PER_VOICE];
    bool  osc_cycle_completed[PX_MAX_OSC_PER_VOICE]; // Track reset for sync

    // NOTE: frequency and original_frequency track the "Base Note" pitch.
    // Per-oscillator frequency is calculated on the fly: base * tuning_ratio.

    float pan_position;

    ADSR* adsrs;
    float* adsr_mod_amounts; // Points into s->patch.template_voice_adsr_mod_amounts

    Filter filter_instance;
    Filter filter_instance_r; // v1.6: Stereo Filter
    float filter_cutoff_hz;
    float filter_resonance_q;
    PxFilterMode filter_mode;
    float filter_env_amount_hz;

    int key_id;
    uint64_t trigger_sequence_number;

    LFOInstance* lfo_instances;
    float* lfo_mod_amounts_snapshot; // 2D array flattened

    VmParams osc_vm_params[PX_MAX_OSC_PER_VOICE];

    // --- Unilegato State Members
    bool is_sliding;              // True if this voice is currently pitch sliding.
    float slide_target_freq;      // The frequency (in Hz) this voice is sliding towards.
    float slide_start_freq;       // The frequency where the slide began.
    float slide_progress;         // A value from 0.0 to 1.0 tracking the slide's completion.

    float initial_velocity;                // Velocity stored at note-on (0.0 to 1.0)

    float poly_aftertouch_pressure;  // v1.4.1: 0.0 to 1.0, default 0.0

    // v1.6: Cached tuning for active oscillators
    float current_osc_coarse_semitones[PX_MAX_OSC_PER_VOICE];
    float current_osc_fine_cents[PX_MAX_OSC_PER_VOICE];

    // --- State for oscillator update throttling (Per Oscillator)
    float update_countdown;         // Samples remaining until the next real calculation (Shared timing for now?)
    float samples_per_update;       // How many samples to wait between calculations.

    // Per-oscillator interpolation state
    float osc_interp_samples[PX_MAX_OSC_PER_VOICE][4];
    float osc_phase_at_interp_start[PX_MAX_OSC_PER_VOICE];
    float osc_phase_at_interp_end[PX_MAX_OSC_PER_VOICE];

    // --- v1.6: Per-Oscillator Sequencer State ---
    PxSeqState seq_states[PX_MAX_OSC_PER_VOICE];

    uint32_t rng_state;         // Context-aware PRNG state (Shared)
} Voice;

/**
 * @section mod_matrix Modulation Matrix Usage
 *
 * Polysonix includes a unified 16-slot modulation matrix that allows routing various
 * control sources (Velocity, Aftertouch, Mod Wheel, Pitch Bend) to synthesis parameters.
 *
 * Example: Routing Mod Wheel to Filter Cutoff
 * @code
 * // Set slot 0: Source = Mod Wheel, Dest = Filter Cutoff, Amount = 0.5 (50%)
 * PX_SetModMatrixSlot(synth, 0, PX_MOD_SRC_MODWHEEL, PX_MOD_DEST_FILTER_CUTOFF, 0.5f);
 * PX_EnableModMatrixSlot(synth, 0, true);
 * @endcode
 *
 * Example: Routing Velocity to Oscillator ModA
 * @code
 * // Set slot 1: Source = Velocity, Dest = OSC ModA, Amount = 1.0 (100%)
 * PX_SetModMatrixSlot(synth, 1, PX_MOD_SRC_VELOCITY, PX_MOD_DEST_OSC_MODA, 1.0f);
 * PX_EnableModMatrixSlot(synth, 1, true);
 * @endcode
 *
 * Example: Routing Polyphonic Aftertouch to Filter Cutoff
 * @code
 * // Set slot 2: Source = Poly Aftertouch, Dest = Filter Cutoff, Amount = 0.8f
 * PX_SetModMatrixSlot(synth, 2, PX_MOD_SRC_POLY_AFTERTOUCH, PX_MOD_DEST_FILTER_CUTOFF, 0.8f);
 * PX_EnableModMatrixSlot(synth, 2, true);
 * @endcode
 *
 * Example: Routing Pitch Bend to Oscillator Frequency (Pitch)
 * @code
 * // Set slot 3: Source = Pitch Bend, Dest = Frequency, Amount = 1.0f (Full Range)
 * // Note: Pitch bend range is set globally via PX_SetPitchBendRange()
 * PX_SetModMatrixSlot(synth, 3, PX_MOD_SRC_PITCHBEND, PX_MOD_DEST_FREQUENCY, 1.0f);
 * PX_EnableModMatrixSlot(synth, 3, true);
 * @endcode
 */

/* ==================== v1.3 MODULATION MATRIX TYPES ==================== */
typedef enum {
    PX_MOD_SRC_VELOCITY,
    PX_MOD_SRC_AFTERTOUCH,
    PX_MOD_SRC_MODWHEEL,      // CC #1, 0.0 to 1.0
    PX_MOD_SRC_PITCHBEND,     // -1.0 to +1.0 (bipolar, full range)
    PX_MOD_SRC_POLY_AFTERTOUCH,   // v1.4.1: Per-note pressure
    PX_MOD_SRC_KEY_TRACK,  // v1.4.5: Normalized key position (-1.0 low to +1.0 high)
    PX_MOD_SRC_COUNT
} PxModSource;

typedef enum {
    // ADSR parameters (3 ADSRs × 4 params)
    PX_MOD_DEST_ADSR1_ATTACK, PX_MOD_DEST_ADSR1_DECAY, PX_MOD_DEST_ADSR1_SUSTAIN, PX_MOD_DEST_ADSR1_RELEASE,
    PX_MOD_DEST_ADSR2_ATTACK, PX_MOD_DEST_ADSR2_DECAY, PX_MOD_DEST_ADSR2_SUSTAIN, PX_MOD_DEST_ADSR2_RELEASE,
    PX_MOD_DEST_ADSR3_ATTACK, PX_MOD_DEST_ADSR3_DECAY, PX_MOD_DEST_ADSR3_SUSTAIN, PX_MOD_DEST_ADSR3_RELEASE,

    // LFO parameters (3 LFOs × frequency + depth)
    PX_MOD_DEST_LFO1_FREQ, PX_MOD_DEST_LFO1_DEPTH,
    PX_MOD_DEST_LFO2_FREQ, PX_MOD_DEST_LFO2_DEPTH,
    PX_MOD_DEST_LFO3_FREQ, PX_MOD_DEST_LFO3_DEPTH,

    // Oscillator bytecode params (Legacy mapped to Osc 1, or Global?)
    // v1.6: Specific Oscillator Targets
    PX_MOD_DEST_OSC1_PITCH, PX_MOD_DEST_OSC1_MIX, PX_MOD_DEST_OSC1_PAN,
    PX_MOD_DEST_OSC1_MODA,  PX_MOD_DEST_OSC1_MODB, PX_MOD_DEST_OSC1_MODC,
    PX_MOD_DEST_OSC1_CROSS_MOD_DEPTH, PX_MOD_DEST_OSC1_PHASE_DIST, PX_MOD_DEST_OSC1_SYNC_SOFTNESS, PX_MOD_DEST_OSC1_RING_MOD_DEPTH, PX_MOD_DEST_OSC1_BITCRUSH_DEPTH,

    PX_MOD_DEST_OSC2_PITCH, PX_MOD_DEST_OSC2_MIX, PX_MOD_DEST_OSC2_PAN,
    PX_MOD_DEST_OSC2_MODA,  PX_MOD_DEST_OSC2_MODB, PX_MOD_DEST_OSC2_MODC,
    PX_MOD_DEST_OSC2_CROSS_MOD_DEPTH, PX_MOD_DEST_OSC2_PHASE_DIST, PX_MOD_DEST_OSC2_SYNC_SOFTNESS, PX_MOD_DEST_OSC2_RING_MOD_DEPTH, PX_MOD_DEST_OSC2_BITCRUSH_DEPTH,

    PX_MOD_DEST_OSC3_PITCH, PX_MOD_DEST_OSC3_MIX, PX_MOD_DEST_OSC3_PAN,
    PX_MOD_DEST_OSC3_MODA,  PX_MOD_DEST_OSC3_MODB, PX_MOD_DEST_OSC3_MODC,
    PX_MOD_DEST_OSC3_CROSS_MOD_DEPTH, PX_MOD_DEST_OSC3_PHASE_DIST, PX_MOD_DEST_OSC3_SYNC_SOFTNESS, PX_MOD_DEST_OSC3_RING_MOD_DEPTH, PX_MOD_DEST_OSC3_BITCRUSH_DEPTH,

    // Legacy / Global mappings (mapped to Osc 1 internally for compatibility)
    PX_MOD_DEST_OSC_MODA,
    PX_MOD_DEST_OSC_MODB,
    PX_MOD_DEST_OSC_MODC,

    // Filter (added per review)
    PX_MOD_DEST_FILTER_CUTOFF,

    PX_MOD_DEST_COUNT
} PxModDestination;

typedef struct {
    PxModSource source;
    PxModDestination dest;
    float amount;      // -1.0 to +1.0
    bool enabled;
} PxModSlot;

#define PX_MOD_MATRIX_SLOTS 16
#define PX_PATCH_NAME_LEN 64

// PxPatch: The editable parameters of the synthesizer (the "sound").
typedef struct PxPatch {
    char name[PX_PATCH_NAME_LEN];
    PxADSRParams* template_voice_adsrs;
    float* template_voice_adsr_mod_amounts;
    PxLFOParams* template_lfos;
    float filter_cutoff_hz;
    float filter_resonance_q;
    float filter_env_amount_hz;
    float filter_drive;
    float filter_key_track;
    int   filter_poles;
    PxFilterMode filter_mode;
    float voice_pan_setting;
    float default_note_amp;
    float limiter_threshold;
    float limiter_release_ms;
    bool unilegato_enabled;
    float unilegato_slide_duration_s;

    /* ==================== v1.3 MODULATION MATRIX ==================== */
    // Note: Previous hard-wired velocity/aftertouch params are removed.
    PxModSlot mod_matrix[PX_MOD_MATRIX_SLOTS];

    // === v1.4: Pitch Bend Range (for classic feel when routed to pitch) ===
    float pitchbend_range_semitones;  // Default 2.0 — used only if matrix routes to pitch

    // v1.4.4: Global post-filter params (independent of per-voice)
    bool global_filter_enabled;         // Default false
    float global_filter_cutoff_hz;
    float global_filter_resonance_q;
    float global_filter_env_amount_hz;  // Ignored (global, no env)
    float global_filter_drive;
    float global_filter_key_track;      // Ignored (global, no key)
    int global_filter_poles;
    PxFilterMode global_filter_mode;

    // v1.4.5: Curves for velocity/aftertouch
    PxCurveType velocity_curve;     // Default PX_CURVE_LINEAR
    PxCurveType aftertouch_curve;   // Default PX_CURVE_LINEAR

    // v1.6: Triple Oscillator Architecture
    PxOscillator osc[PX_MAX_OSC_PER_VOICE];

    // v1.8: Time-Locked Wave Sequencing
    bool  wseq_fixed_time;      // Enable Time-Locked mode
    float wseq_ref_freq;        // Reference frequency (default 440.0 Hz)

    // v1.8.3: Static Glide / Portamento
    PxGlideMode glide_mode;
    float glide_time;           // Time constant (seconds)
    bool glide_legato_only;     // Only glide on overlapped notes (default true)
    bool glide_always;          // Force glide behavior (Monophonic/Polyphonic Portamento)
} PxPatch;

// --- THREAD-SAFE COMMUNICATION STRUCTURES ---
typedef enum {
    PX_CMD_NOTE_ON,
    PX_CMD_NOTE_OFF,
    PX_CMD_SET_VOICE_ADSR_PARAM,
    PX_CMD_SET_VOICE_ADSR_ENABLED,
    PX_CMD_SET_VOICE_ADSR_MOD_AMOUNT,
    PX_CMD_SET_LFO_PARAM,
    PX_CMD_SET_LFO_WAVEFORM,
    PX_CMD_SET_LFO_ENABLED,
    PX_CMD_SET_LFO_RESET_ON_KEY,
    PX_CMD_SET_LFO_ADSR_PARAM,
    PX_CMD_SET_LFO_ADSR_ENABLED,
    PX_CMD_SET_LFO_MOD_AMOUNT,
    PX_CMD_SET_LFO_UPDATE_INTERVAL,
    PX_CMD_SET_FILTER_PARAM,
    PX_CMD_SET_FILTER_MODE,
    PX_CMD_SET_GLOBAL_PAN,
    PX_CMD_SET_LIMITER_THRESHOLD,
    PX_CMD_SET_LIMITER_RELEASE,
    PX_CMD_SET_UNILEGATO_ENABLED,
    PX_CMD_SET_UNILEGATO_SLIDE_TIME,
    PX_CMD_NOTE_ON_VEL,                  // New: NoteOn with velocity
    PX_CMD_CHANNEL_AFTERTOUCH,           // New: Channel aftertouch (monophonic pressure)
    PX_CMD_SET_VELOCITY_TO_AMP,
    PX_CMD_SET_VELOCITY_TO_FILTER_CUTOFF,
    PX_CMD_SET_VELOCITY_ATTACK_SCALING,
    PX_CMD_SET_VELOCITY_TO_PARAM1,
    PX_CMD_SET_AFTERTOUCH_TO_FILTER_CUTOFF,
    PX_CMD_SET_AFTERTOUCH_TO_VIBRATO,
    // === v1.3 Modulation Matrix Commands ===
    PX_CMD_SET_MOD_MATRIX_SLOT,
    PX_CMD_ENABLE_MOD_MATRIX_SLOT,
    PX_CMD_CLEAR_MOD_MATRIX,
    PX_CMD_SET_GLOBAL_FILTER_ENABLED,
    PX_CMD_SET_GLOBAL_FILTER_PARAM,
    PX_CMD_SET_GLOBAL_FILTER_MODE,
    PX_CMD_SET_VELOCITY_CURVE,
    PX_CMD_SET_AFTERTOUCH_CURVE,
    PX_CMD_CONTROL_CHANGE,
    PX_CMD_PITCH_BEND,
    PX_CMD_SET_PITCHBEND_RANGE,
    PX_CMD_POLY_AFTERTOUCH,
    // v1.6: Triple Osc Commands
    PX_CMD_SET_OSC_ENABLED,
    PX_CMD_SET_OSC_WAVE,
    PX_CMD_SET_OSC_COARSE_TUNE,
    PX_CMD_SET_OSC_FINE_TUNE,
    PX_CMD_SET_OSC_MIX,
    PX_CMD_SET_OSC_PAN,
    PX_CMD_SET_SEQUENCE_ID, // v1.5 Legacy
    PX_CMD_SET_OSC_SEQUENCE, // v1.6
    // v1.7 Features
    PX_CMD_SET_OSC_CROSS_MOD,
    PX_CMD_SET_OSC_PHASE_DIST,
    PX_CMD_SET_OSC_SYNC,
    PX_CMD_SET_OSC_RING_MOD,
    PX_CMD_SET_OSC_BITCRUSH,
    // v1.8
    PX_CMD_SET_WSEQ_FIXED_TIME,
    PX_CMD_SET_WSEQ_REF_FREQ,
    // v1.8.3 Static Glide
    PX_CMD_SET_GLIDE_MODE,
    PX_CMD_SET_GLIDE_TIME,
    PX_CMD_SET_GLIDE_LEGATO_ONLY,
    PX_CMD_SET_GLIDE_ALWAYS,
    PX_CMD_SET_PATCH_NAME
} PxCommandType;

typedef union {
    struct { int midi_note; int wave_idx; int key_id; } note_on;
    struct { int key_id; } note_off;
    struct { int idx; int enum_val; float float_val; } param_idx_enum_float;
    struct { int idx; int enum_val; } param_idx_enum;
    struct { int idx; bool bool_val; } param_idx_bool;
    struct { int enum_val; float float_val; } param_enum_float;
    struct { int mode_val; } param_mode;
    struct { float float_val; } param_float;
    struct { bool bool_val; } param_bool;
    struct { int midi_note; int wave_idx; int key_id; float velocity; } note_on_vel;
    struct { float pressure; } aftertouch;
    struct { int slot; int src; int dest; float amount; } mod_slot;
    struct { int slot; bool enabled; } mod_enable;
    struct { bool enabled; } global_filter_enable;
    struct { int param_type; float value; } global_filter_param;
    struct { int mode; } global_filter_mode;
    struct { int curve_type; } curve;
    struct { int cc_num; float value; } cc;
    struct { float value; } bend;  // 0.0 to 1.0
    struct { int key_id; float pressure; } poly_at;
    struct { int osc_idx; float value; } osc_val; // For Tune, Mix, Pan
    struct { int osc_idx; int int_val; } osc_int; // For Wave
    struct { int osc_idx; bool enabled; } osc_bool; // For Enabled
    struct { int seq_id; } sequence;
    struct { int osc_idx; int seq_id; } osc_sequence;
    struct { int osc_idx; bool enabled; float value; } osc_feat; // v1.7: enabled + depth/amount
    struct { bool enabled; } wseq_fixed_time;
    struct { float freq; } wseq_ref_freq;
    struct { int mode; } glide_mode;
    struct { float time_s; } glide_time;
    struct { bool enabled; } glide_bool;
    struct { char name[PX_PATCH_NAME_LEN]; } patch_name;
} PxCommandData;

typedef struct {
    PxCommandType command_type;
    PxCommandData data;
} PxCommand;

typedef struct {
    PxCommand* buffer;
    int capacity;
    _Atomic int write;
    _Atomic int read;
    atomic_flag lock;
} CommandQueue;

typedef struct {
    PxPatch patch_copy;
    PxVoiceInfo voices[MAX_VOICES];
    PxLimiterInfo limiter;
    PxLFOInfo lfos[MAX_LFOS];
    float lfo_update_interval_ms;
} UISnapshot;

// The main synthesizer object, containing all state.
struct PxSynth {
    PxConfig config;                // Core configuration, set at creation time.
    PxPatch patch;                  // The current sound "patch" containing all editable parameters.
    Voice* voices;                  // Dynamically allocated array of voice structures.
    EnhancedLimiter limiter;        // The master bus limiter instance.

    // State for managing voice allocation and timing.
    uint64_t global_trigger_counter; // Used for voice stealing priority.
    int lfo_update_countdown;        // Countdown for the slower LFO update tick.
    float time_per_sample;           // Pre-calculated time delta for one audio sample.
    float glide_coeff;               // Pre-calculated coefficient for smooth glide.

    LFOInstance* template_lfo_instances;    // Internal state for the template LFOs (used for UI display).

    // --- Unilegato Tracking State
    int last_held_note_midi;        // The MIDI note number of the last key that was pressed. This determines the slide target.
    int num_keys_held;              // The total number of keys currently held down by the user.
    int held_notes[MAX_VOICES];     // An array acting as a "stack" to keep track of the MIDI notes of all currently held keys, in the order they were pressed.

    float channel_aftertouch_pressure;     // Current channel aftertouch value (0.0 to 1.0)

    // === v1.4 Unified Controllers ===
    float modwheel_value;             // 0.0 to 1.0
    float pitchbend_value;            // -1.0 to +1.0 (live global value)

    // No global poly aftertouch needed — stored per-voice

    // v1.4.4: Global post-filter instance
    Filter global_filter_l;
    Filter global_filter_r;

    // --- THREAD-SAFE MEMBERS ---
    CommandQueue cmd_queue;
    UISnapshot ui_snapshot;
};


// --- FORWARD DECLARATIONS for internal functions ---
static void PX_NoteOn_internal(PxSynth* s, int midi_note, int wave_idx, int key_id, float velocity);
static void PX_NoteOff_internal(PxSynth* s, int key_id);
static PxVoiceInfo PX_GetVoiceInfo_internal(PxSynth* s, int idx);
static PxLimiterInfo PX_GetLimiterInfo_internal(PxSynth* s);
static PxLFOInfo PX_GetLFOInfo_internal(PxSynth* s, int lfo_idx);
static void PX_UpdateUISnapshot(PxSynth* s);
static void ADSR_SetParams(ADSR* adsr, const PxADSRParams* params, float sample_rate);
static void ADSR_Init(ADSR* adsr, const PxADSRParams* params, float sample_rate);
static void ADSR_TriggerAttack(ADSR* adsr);
static void ADSR_TriggerRelease(ADSR* adsr);
static void ADSR_Update(ADSR* adsr, float time_delta, float sample_rate);
static void InitializeEnhancedLimiter(EnhancedLimiter* limiter, float sample_rate, float threshold, float release_ms);
static void ProcessEnhancedLimiter(EnhancedLimiter* limiter, float *input_l, float *input_r, float *output_l, float *output_r, float sample_rate);
static void Filter_Init(Filter* filter);
static void Filter_SetCoefficients(Filter* filter, float cutoff_hz, float resonance_q, PxFilterMode mode, int poles, float sample_rate);
static float Filter_Process_Internal(Filter* filter, float input_sample);
static float Filter_Process_Oversampled(Filter* filter, float input_sample);
static float GenerateLFOValue(LFOInstance* lfo_instance);
static void LFOInstance_Init(LFOInstance* lfo, float sample_rate);
static float get_midi_frequency(int midi_note);
static int find_inactive_voice(PxSynth* s);
static int find_voice_to_steal(PxSynth* s);
static float soft_clip(float x);
static float cubic_interpolate(float y0, float y1, float y2, float y3, float t);

// Simple LCG PRNG for audio thread safety
static inline uint32_t px_rand(uint32_t* state) {
    *state = *state * 1664525 + 1013904223;
    return *state;
}

// --- Internal Helper Functions ---
static void ADSR_SetParams(ADSR* adsr, const PxADSRParams* params, float sample_rate) {
    adsr->attack_time = fmaxf(MIN_ADSR_TIME, params->attack_time);
    adsr->decay_time = fmaxf(MIN_ADSR_TIME, params->decay_time);
    adsr->sustain_level = fmaxf(0.0f, fminf(1.0f, params->sustain_level));
    adsr->release_time = fmaxf(MIN_ADSR_TIME, params->release_time);
    adsr->enabled = params->enabled;

    if (adsr->attack_time > 0.0f) adsr->attack_rate = 1.0f / adsr->attack_time;
    else adsr->attack_rate = 1.0f / MIN_ADSR_TIME;

    double log_target = log((double)EXP_DECAY_TARGET);

    if (adsr->sustain_level < 0.999f && adsr->decay_time > 0.0f && sample_rate > 0) {
         double steps_for_decay = adsr->decay_time * sample_rate;
         if (steps_for_decay < 1.0) steps_for_decay = 1.0;
         adsr->decay_multiplier = (float)exp(log_target / steps_for_decay);
    } else {
        adsr->decay_multiplier = 0.0f;
    }

    if (adsr->release_time > 0.0f && sample_rate > 0) {
        double steps_for_release = adsr->release_time * sample_rate;
        if (steps_for_release < 1.0) steps_for_release = 1.0;
        adsr->release_multiplier = (float)exp(log_target / steps_for_release);
    } else {
        adsr->release_multiplier = 0.0f;
    }
}

static void ADSR_Init(ADSR* adsr, const PxADSRParams* params, float sample_rate) {
    adsr->state = ADSR_STATE_IDLE;
    adsr->level = 0.0f;
    ADSR_SetParams(adsr, params, sample_rate);
}

static void ADSR_TriggerAttack(ADSR* adsr) {
    if (!adsr->enabled) return;
    if (adsr->state == ADSR_STATE_ATTACK || adsr->state == ADSR_STATE_DECAY || adsr->state == ADSR_STATE_SUSTAIN) {
        return;
    }
    adsr->state = ADSR_STATE_ATTACK;
    if (adsr->attack_time <= MIN_ADSR_TIME) {
        adsr->level = 1.0f;
        adsr->state = ADSR_STATE_DECAY;
        if (adsr->decay_time <= MIN_ADSR_TIME || adsr->sustain_level >= 0.999f) {
            adsr->state = ADSR_STATE_SUSTAIN;
            adsr->level = adsr->sustain_level;
        }
    }
}

static void ADSR_TriggerRelease(ADSR* adsr) {
    if (!adsr->enabled || adsr->state == ADSR_STATE_IDLE) return;
    adsr->state = ADSR_STATE_RELEASE;
    if (adsr->release_time <= MIN_ADSR_TIME || adsr->release_multiplier == 0.0f) {
        adsr->level = 0.0f;
        adsr->state = ADSR_STATE_IDLE;
    }
}

static void ADSR_Update(ADSR* adsr, float time_delta, float sample_rate) {
    if (!adsr->enabled) {
        if (adsr->state != ADSR_STATE_IDLE) {
            adsr->level = 0.0f;
            adsr->state = ADSR_STATE_IDLE;
        }
        return;
    }
    if (time_delta <= 0.0f) return;

    switch (adsr->state) {
        case ADSR_STATE_ATTACK:
            adsr->level += adsr->attack_rate * time_delta;
            if (adsr->level >= 1.0f) {
                adsr->level = 1.0f;
                adsr->state = ADSR_STATE_DECAY;
            }
            break;

        case ADSR_STATE_DECAY:
            if (adsr->level > adsr->sustain_level) {
                // Calculate how many samples have passed and apply the multiplier exponentially.
                int num_steps = (int)(time_delta * sample_rate);
                if (num_steps > 0) {
                     adsr->level = (adsr->level - adsr->sustain_level) * powf(adsr->decay_multiplier, (float)num_steps) + adsr->sustain_level;
                }
            }
            if (adsr->level <= adsr->sustain_level) {
                adsr->level = adsr->sustain_level;
                adsr->state = ADSR_STATE_SUSTAIN;
            }
            break;

        case ADSR_STATE_SUSTAIN:
            // The level is already at sustain_level, do nothing.
            break;

        case ADSR_STATE_RELEASE:
            if (adsr->level > EXP_DECAY_TARGET) {
                // Calculate how many samples have passed and apply the multiplier exponentially.
                int num_steps = (int)(time_delta * sample_rate);
                if (num_steps > 0) {
                    adsr->level *= powf(adsr->release_multiplier, (float)num_steps);
                }
            } else {
                adsr->level = 0.0f;
                adsr->state = ADSR_STATE_IDLE;
            }
            break;

        default: // ADSR_STATE_IDLE
            adsr->level = 0.0f;
            break;
    }
}

static void InitializeEnhancedLimiter(EnhancedLimiter* limiter, float sample_rate, float threshold, float release_ms) {
    if (sample_rate <= 0) { limiter->initialized = false; return; }

    limiter->threshold = threshold;
    limiter->ratio = 20.0f;
    limiter->makeup_gain = 1.0f / limiter->threshold;
    float attack_time_ms = 0.1f;
    limiter->attack_coeff = expf(-1.0f / (attack_time_ms * 0.001f * sample_rate));
    limiter->release_coeff = expf(-1.0f / (release_ms * 0.001f * sample_rate));
    limiter->delay_samples = (int)(sample_rate * 0.001f);

    // Safety clamp against buffer size (should be allocated correctly in PX_Create)
    // We strictly need delay_samples < buffer_capacity to have any lookahead effect in a circular buffer?
    // Actually, capacity just needs to be >= delay_samples + 1.
    if (limiter->buffer_capacity > 0) {
        if (limiter->delay_samples >= limiter->buffer_capacity) limiter->delay_samples = limiter->buffer_capacity - 1;
    }
    if (limiter->delay_samples < 1) limiter->delay_samples = 1;

    // Zero out the buffer using the known capacity. Pointers must be valid (allocated in PX_Create).
    if (limiter->delay_line_l) memset(limiter->delay_line_l, 0, limiter->buffer_capacity * sizeof(float));
    if (limiter->delay_line_r) memset(limiter->delay_line_r, 0, limiter->buffer_capacity * sizeof(float));

    limiter->delay_write_pos = 0;
    limiter->envelope = 0.0f;
    limiter->smooth_gain = 1.0f;
    limiter->target_gain = 1.0f;
    limiter->peak_hold = 0.0f;
    limiter->peak_hold_samples = 0;
    limiter->release_ms_cache = release_ms;
    limiter->initialized = true;
}

static void ProcessEnhancedLimiter(EnhancedLimiter* limiter, float *input_l, float *input_r, float *output_l, float *output_r, float sample_rate) {
    if (!limiter->initialized) { *output_l = *input_l * 0.5f; *output_r = *input_r * 0.5f; return; }

    // Store input in delay line
    limiter->delay_line_l[limiter->delay_write_pos] = *input_l;
    limiter->delay_line_r[limiter->delay_write_pos] = *input_r;

    // Calculate read position for lookahead
    int cap = limiter->buffer_capacity;
    int read_pos = (limiter->delay_write_pos - limiter->delay_samples + cap) % cap;
    float delayed_l = limiter->delay_line_l[read_pos];
    float delayed_r = limiter->delay_line_r[read_pos];

    // Advance write position
    limiter->delay_write_pos = (limiter->delay_write_pos + 1) % cap;

    // Peak detection on current (non-delayed) input
    float input_peak = fmaxf(fabsf(*input_l), fabsf(*input_r));

    // Peak hold with decay for smoother response
    if (input_peak > limiter->peak_hold) {
        limiter->peak_hold = input_peak;
        limiter->peak_hold_samples = (int)(sample_rate * 0.002f);
    } else if (limiter->peak_hold_samples > 0) {
        limiter->peak_hold_samples--;
    } else {
        limiter->peak_hold *= 0.999f;
    }

    // Use peak hold for envelope detection
    float detection_level = limiter->peak_hold;

    // Envelope follower with attack/release
    if (detection_level > limiter->envelope) {
        limiter->envelope = detection_level + (limiter->envelope - detection_level) * limiter->attack_coeff;
    } else {
        limiter->envelope = detection_level + (limiter->envelope - detection_level) * limiter->release_coeff;
    }

    // Calculate gain reduction
    float gain_reduction = 1.0f;
    if (limiter->envelope > limiter->threshold) {
        float over_threshold = limiter->envelope - limiter->threshold;
        float compressed_over = over_threshold / limiter->ratio;
        float target_level = limiter->threshold + compressed_over;
        gain_reduction = target_level / limiter->envelope;
        if (gain_reduction * limiter->envelope > limiter->threshold) {
            gain_reduction = limiter->threshold / limiter->envelope;
        }
    }

    // Smooth gain changes to prevent clicks
    limiter->target_gain = gain_reduction;
    float gain_smooth_coeff = 0.99f;
    limiter->smooth_gain = limiter->target_gain + (limiter->smooth_gain - limiter->target_gain) * gain_smooth_coeff;

    // Apply limiting with makeup gain
    float final_gain = limiter->smooth_gain * limiter->makeup_gain;

    *output_l = fmaxf(-0.999f, fminf(0.999f, delayed_l * final_gain));
    *output_r = fmaxf(-0.999f, fminf(0.999f, delayed_r * final_gain));
}

static void Filter_Init(Filter* filter) {
    memset(filter, 0, sizeof(Filter));
    filter->current_mode = PX_FILTER_MODE_OFF;
    filter->dc_block_x1 = 0.0f;
    filter->dc_block_y1 = 0.0f;
    filter->os_x1 = 0.0f;
    filter->os_x2 = 0.0f;
    filter->combo_lp_state = 0.0f;
    filter->combo_hp_state = 0.0f;
}

static void Filter_SetCoefficients(Filter* filter, float cutoff_hz, float resonance_q, PxFilterMode mode, int poles, float sample_rate) {
    if (sample_rate <= 0) return;

    // Option B: Disable Compensation (Pure & Accurate) - Remove magic numbers.
    float max_safe_cutoff = sample_rate * 0.45f;
    cutoff_hz = fmaxf(20.0f, fminf(cutoff_hz, max_safe_cutoff));

    // Adjust resonance based on the number of poles to maintain a similar feel
    float q_for_poles = resonance_q;
    if (poles == 3) {
        q_for_poles = powf(resonance_q, 0.75f);
    } else if (poles == 4) {
        q_for_poles = sqrtf(resonance_q);
    }
    // For 1-pole (6dB), resonance has little effect — clamp gently
    else if (poles == 1) {
        q_for_poles = 0.707f;  // Neutral, no peaking
    }

    float freq_factor = cutoff_hz / sample_rate;
    float max_q_at_freq = 20.0f * (1.0f - freq_factor * freq_factor);
    resonance_q = fmaxf(0.5f, fminf(q_for_poles, max_q_at_freq));

    // Coefficients for the main 2-pole SVF stage
    float omega = 2.0f * PI * cutoff_hz / sample_rate;
    float sin_omega = sinf(omega);
    float alpha = sin_omega / (2.0f * resonance_q);
    filter->f_coeff = 2.0f * sin_omega / (1.0f + alpha);
    filter->q_inv_coeff = 2.0f * alpha;

    // Coefficient for the additional 1-pole stage
    float tan_val = tanf(PI * cutoff_hz / sample_rate);
    filter->pole3_coeff = tan_val / (1.0f + tan_val);

    filter->current_mode = mode;
    filter->poles = poles;
}

static float Filter_Process_Internal(Filter* filter, float input_sample) {
    if (filter->current_mode == PX_FILTER_MODE_OFF) return input_sample;

    // --- DC Blocker & Drive ---
    float dc_block_coeff = 0.999f;
    float dc_blocked = input_sample - filter->dc_block_x1 + dc_block_coeff * filter->dc_block_y1;
    filter->dc_block_x1 = input_sample;
    filter->dc_block_y1 = dc_blocked;
    float driven_input = tanhf(dc_blocked * filter->drive);

    // v1.4.3: Unified 6 dB/oct (1-pole) path with full combo support
    if (filter->poles == 1) {
        // Always compute both LP and HP 1-pole responses in parallel
        filter->combo_lp_state += filter->pole3_coeff * (driven_input - filter->combo_lp_state);
        filter->combo_hp_state += filter->pole3_coeff * (driven_input - filter->combo_hp_state);

        float lp = filter->combo_lp_state;
        float hp = driven_input - filter->combo_hp_state;
        float bp = driven_input - lp - hp;  // Derived band-pass

        float output;
        switch (filter->current_mode) {
            case PX_FILTER_MODE_LP:       output = lp; break;
            case PX_FILTER_MODE_HP:       output = hp; break;
            case PX_FILTER_MODE_BP:       output = bp; break;
            case PX_FILTER_MODE_NOTCH:    output = lp + hp; break;
            case PX_FILTER_MODE_ALLPASS:  output = driven_input - 2.0f * bp; break;
            case PX_FILTER_MODE_LP_BP:    output = (lp + bp) * 0.707f; break;
            case PX_FILTER_MODE_LP_HP:    output = (lp + hp) * 0.707f; break;
            case PX_FILTER_MODE_BP_HP:    output = (bp + hp) * 0.707f; break;
            default:                      output = driven_input; break;
        }
        return output;
    }

    // --- Multi-pole path (poles >= 2): Full SVF with combos ---
    // --- Stage 1: 12dB SVF (Always runs) ---
    // These are the raw 12dB outputs, calculated fresh each sample.
    float notch1 = driven_input - filter->q_inv_coeff * filter->bp_state1;
    float lp1 = filter->lp_state1 + filter->f_coeff * filter->bp_state1;
    float hp1 = notch1 - lp1;
    float bp1 = filter->bp_state1 + filter->f_coeff * hp1;

    // Update the state variables for the next sample.
    const float anti_denormal = 1e-25f;
    filter->lp_state1 = tanhf(lp1 + anti_denormal) - anti_denormal;
    filter->bp_state1 = tanhf(bp1 + anti_denormal) - anti_denormal;

    // --- Initialize final outputs with the 12dB results ---
    float final_lp = filter->lp_state1;
    float final_bp = filter->bp_state1;
    float final_hp = hp1;

    // --- Stage 2: Apply additional poles if needed ---
    if (filter->poles == 3) {
        // --- Correct 18dB Logic ---
        // 18dB LP = 12dB LP -> 6dB LP stage
        filter->pole3_lp_state += filter->pole3_coeff * (final_lp - filter->pole3_lp_state);
        final_lp = filter->pole3_lp_state;

        // 18dB HP = 12dB HP -> 6dB HP stage
        filter->pole3_hp_state += filter->pole3_coeff * (final_hp - filter->pole3_hp_state);
        final_hp = final_hp - filter->pole3_hp_state;

        // 18dB BP = 12dB BP -> 6dB LP stage (to smooth the peak)
        filter->pole3_bp_state += filter->pole3_coeff * (final_bp - filter->pole3_bp_state);
        final_bp = filter->pole3_bp_state;

    } else if (filter->poles == 4) {
        // --- Correct 24dB Logic ---
        float stage2_input = filter->lp_state1;
        float notch2 = stage2_input - filter->q_inv_coeff * filter->bp_state2;
        float lp2 = filter->lp_state2 + filter->f_coeff * filter->bp_state2;
        float hp2 = notch2 - lp2;
        float bp2 = filter->bp_state2 + filter->f_coeff * hp2;

        filter->lp_state2 = tanhf(lp2 + anti_denormal) - anti_denormal;
        filter->bp_state2 = tanhf(bp2 + anti_denormal) - anti_denormal;

        final_lp = filter->lp_state2;
        final_bp = filter->bp_state2;
        final_hp = hp2;
    }

    // --- Final Output Selection ---
    switch (filter->current_mode) {
        case PX_FILTER_MODE_LP:      return final_lp;
        case PX_FILTER_MODE_BP:      return final_bp;
        case PX_FILTER_MODE_HP:      return final_hp;
        case PX_FILTER_MODE_LP_BP:   return (final_lp + final_bp) * 0.707f;
        case PX_FILTER_MODE_LP_HP:   return (final_lp + final_hp) * 0.707f;
        case PX_FILTER_MODE_BP_HP:   return (final_bp + final_hp) * 0.707f;
        case PX_FILTER_MODE_NOTCH:   return final_lp + final_hp;
        case PX_FILTER_MODE_ALLPASS: return final_lp - final_bp + final_hp;
        default: return input_sample;
    }
}

static float Filter_Process_Oversampled(Filter *filter, float input_sample) {
    float y0 = Filter_Process_Internal(filter, input_sample);
    float y1 = Filter_Process_Internal(filter, input_sample);
    float out = (HB[0] * filter->os_x2) + (HB[1] * filter->os_x1) + (HB[2] * y0) + (HB[3] * y1);
    filter->os_x2 = filter->os_x1;
    filter->os_x1 = y0;
    return out;
}

static float GenerateLFOValue(LFOInstance* lfo_instance) {
    if (lfo_instance->wave_idx < 0 || lfo_instance->wave_idx >= NUM_WAVEFORMS) return 0.0f;
    BytecodeChunk* chunk = default_waves[lfo_instance->wave_idx].compiled_bytecode;
    if (!chunk) return 0.0f;
    lfo_instance->lfo_vm_params.x = lfo_instance->phase * 2.0f * PI;
    return execute_bytecode(chunk, &lfo_instance->lfo_vm_params);
}

static void LFOInstance_Init(LFOInstance* lfo, float sample_rate) {
    memset(lfo, 0, sizeof(LFOInstance));

    // Explicitly initialize the nested ADSR struct.
    // Use some safe, silent defaults. The real values will be set later.
    PxADSRParams default_adsr_params = {0.01f, 0.1f, 1.0f, 0.1f, false};
    ADSR_Init(&lfo->adsr, &default_adsr_params, sample_rate);

    // Initialize VmParams
    lfo->lfo_vm_params.rand_offset = (float)rand() / RAND_MAX;
    lfo->lfo_vm_params.lfsr_type = LFSR_8BIT;
    lfo->lfo_vm_params.lfsr_state = (uint32_t)rand() | 1;
    lfo->lfo_vm_params.lfsr_seed = lfo->lfo_vm_params.lfsr_state;
}

static float get_midi_frequency(int midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f);
}

static int find_inactive_voice(PxSynth* s) {
    for (int i = 0; i < s->config.num_voices; ++i) {
        if (!s->voices[i].active) return i;
        bool all_adsrs_idle = true;
        for (int j = 0; j < s->config.num_voice_adsrs; ++j) {
            if (s->voices[i].adsrs[j].enabled && s->voices[i].adsrs[j].state != ADSR_STATE_IDLE) {
                all_adsrs_idle = false; break;
            }
        }
        if (all_adsrs_idle) {
            bool lfos_idle = true;
            for (int k = 0; k < s->config.num_lfos; ++k) {
                if (s->voices[i].lfo_instances[k].enabled && s->voices[i].lfo_instances[k].adsr.enabled && s->voices[i].lfo_instances[k].adsr.state != ADSR_STATE_IDLE) {
                    lfos_idle = false; break;
                }
            }
            if (lfos_idle) return i;
        }
    }
    return -1;
}

static int find_voice_to_steal(PxSynth* s) {
    if (s->config.num_voices == 0) return -1;

    int oldest_unheld_idx = -1;
    uint64_t oldest_unheld_seq = UINT64_MAX;

    // --- Pass 1: Find the oldest voice that is NOT currently being held down ---
    for (int i = 0; i < s->config.num_voices; ++i) {
        // Skip voices that are currently sliding, they are high priority
        if (s->voices[i].is_sliding) continue;

        bool is_held = false;
        // Check if this voice's key_id is in our stack of held keys
        for (int j = 0; j < s->num_keys_held; j++) {
            if (s->voices[i].key_id == s->held_notes[j]) {
                is_held = true;
                break;
            }
        }

        // If the note is not held (i.e., it's in release phase), it's a candidate for stealing.
        if (!is_held) {
            if (s->voices[i].trigger_sequence_number < oldest_unheld_seq) {
                oldest_unheld_seq = s->voices[i].trigger_sequence_number;
                oldest_unheld_idx = i;
            }
        }
    }

    // If we found a good candidate in the release phase, use it.
    if (oldest_unheld_idx != -1) {
        return oldest_unheld_idx;
    }

    // --- Pass 2: If all voices are held, find the oldest held voice to steal (last resort) ---
    int oldest_held_idx = -1;
    uint64_t oldest_held_seq = UINT64_MAX;
    for (int i = 0; i < s->config.num_voices; ++i) {
        if (s->voices[i].is_sliding) continue;
        if (s->voices[i].trigger_sequence_number < oldest_held_seq) {
            oldest_held_seq = s->voices[i].trigger_sequence_number;
            oldest_held_idx = i;
        }
    }

    return oldest_held_idx;
}

static float soft_clip(float x) {
    return tanhf(x * SOFTCLIP_RATIO) / SOFTCLIP_RATIO;
}

static float cubic_interpolate(float y0, float y1, float y2, float y3, float t) {
    float a0 = y3 - y2 - y0 + y1;
    float a1 = y0 - y1 - a0;
    float a2 = y2 - y0;
    float a3 = y1;
    return a0 * t * t * t + a1 * t * t + a2 * t + a3;
}

// --- COMMAND QUEUE HELPERS ---

// Add CPU pause/yield for spinlocks to reduce power consumption and bus contention
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
    #include <emmintrin.h>
    #define PX_CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
    #define PX_CPU_PAUSE() __asm__ __volatile__("yield") // ARM yield
#else
    #define PX_CPU_PAUSE() ((void)0)
#endif

static bool cmd_push(CommandQueue* q, PxCommand cmd) {
    // 1. Spinlock Acquire
    // "memory_order_acquire" prevents memory reordering of the buffer write
    // to before the lock is taken.
    while (atomic_flag_test_and_set_explicit(&q->lock, memory_order_acquire)) {
        PX_CPU_PAUSE(); // Hint to CPU that we are in a spin-wait loop
    }

    // --- Critical Section Start ---

    // Load 'write' index (Relaxed is fine because we hold the lock)
    int w = atomic_load_explicit(&q->write, memory_order_relaxed);
    int next_w = (w + 1) % q->capacity;
    bool success = false;

    // Load 'read' index
    // Using 'acquire' ensures we see the most up-to-date 'read' value from the
    // audio thread (Consumer). If we used relaxed, we might think the queue
    // is full when it actually has space.
    int r = atomic_load_explicit(&q->read, memory_order_acquire);

    // Check full condition
    if (next_w != r) {
        q->buffer[w] = cmd;
        // Store 'write' (Release ensures the Consumer sees the data
        // before seeing the index update)
        atomic_store_explicit(&q->write, next_w, memory_order_release);
        success = true;
    }

    // --- Critical Section End ---

    // 2. Spinlock Release
    atomic_flag_clear_explicit(&q->lock, memory_order_release);

    return success;
}

static bool cmd_pop(CommandQueue* q, PxCommand* out_cmd) {
    int r = atomic_load_explicit(&q->read, memory_order_relaxed);
    if (r == atomic_load_explicit(&q->write, memory_order_acquire)) {
        return false; // Queue is empty.
    }
    *out_cmd = q->buffer[r];
    atomic_store_explicit(&q->read, (r + 1) % q->capacity, memory_order_release);
    return true;
}

// --- COMMAND PROCESSING (Audio Thread Only) ---
static void PX_ProcessCommands(PxSynth* s) {
    PxCommand cmd;
    while (cmd_pop(&s->cmd_queue, &cmd)) {
        switch (cmd.command_type) {
            case PX_CMD_NOTE_ON: PX_NoteOn_internal(s, cmd.data.note_on.midi_note, cmd.data.note_on.wave_idx, cmd.data.note_on.key_id, 1.0f); break;
            case PX_CMD_NOTE_OFF: PX_NoteOff_internal(s, cmd.data.note_off.key_id); break;
            case PX_CMD_SET_VOICE_ADSR_PARAM: {
                if (cmd.data.param_idx_enum_float.idx < s->config.num_voice_adsrs) {
                    PxADSRParams* p = &s->patch.template_voice_adsrs[cmd.data.param_idx_enum_float.idx];
                    switch ((PxADSRParamType)cmd.data.param_idx_enum_float.enum_val) {
                        case PX_ADSR_PARAM_ATTACK:  p->attack_time = fmaxf(MIN_ADSR_TIME, cmd.data.param_idx_enum_float.float_val); break;
                        case PX_ADSR_PARAM_DECAY:   p->decay_time = fmaxf(MIN_ADSR_TIME, cmd.data.param_idx_enum_float.float_val); break;
                        case PX_ADSR_PARAM_SUSTAIN: p->sustain_level = fmaxf(0.0f, fminf(1.0f, cmd.data.param_idx_enum_float.float_val)); break;
                        case PX_ADSR_PARAM_RELEASE: p->release_time = fmaxf(MIN_ADSR_TIME, cmd.data.param_idx_enum_float.float_val); break;
                    }
                }
                break;
            }
            case PX_CMD_SET_VOICE_ADSR_ENABLED: if (cmd.data.param_idx_bool.idx < s->config.num_voice_adsrs) s->patch.template_voice_adsrs[cmd.data.param_idx_bool.idx].enabled = cmd.data.param_idx_bool.bool_val; break;
            case PX_CMD_SET_VOICE_ADSR_MOD_AMOUNT: if (cmd.data.param_idx_enum_float.idx < s->config.num_voice_adsrs) s->patch.template_voice_adsr_mod_amounts[cmd.data.param_idx_enum_float.idx * PX_ADSR_DEST_COUNT + cmd.data.param_idx_enum_float.enum_val] = cmd.data.param_idx_enum_float.float_val; break;
            case PX_CMD_SET_LFO_PARAM: if (cmd.data.param_idx_enum_float.idx < s->config.num_lfos) s->patch.template_lfos[cmd.data.param_idx_enum_float.idx].frequency = fmaxf(0.01f, cmd.data.param_idx_enum_float.float_val); break;
            case PX_CMD_SET_LFO_WAVEFORM: if (cmd.data.param_idx_enum.idx < s->config.num_lfos) s->patch.template_lfos[cmd.data.param_idx_enum.idx].wave_idx = cmd.data.param_idx_enum.enum_val; break;
            case PX_CMD_SET_LFO_ENABLED: if (cmd.data.param_idx_bool.idx < s->config.num_lfos) s->patch.template_lfos[cmd.data.param_idx_bool.idx].enabled = cmd.data.param_idx_bool.bool_val; break;
            case PX_CMD_SET_LFO_RESET_ON_KEY: if (cmd.data.param_idx_bool.idx < s->config.num_lfos) s->patch.template_lfos[cmd.data.param_idx_bool.idx].reset_on_key_on = cmd.data.param_idx_bool.bool_val; break;
            case PX_CMD_SET_LFO_ADSR_PARAM: {
                if (cmd.data.param_idx_enum_float.idx < s->config.num_lfos) {
                    PxADSRParams* p = &s->patch.template_lfos[cmd.data.param_idx_enum_float.idx].adsr;
                    switch ((PxADSRParamType)cmd.data.param_idx_enum_float.enum_val) {
                        case PX_ADSR_PARAM_ATTACK:  p->attack_time = fmaxf(MIN_ADSR_TIME, cmd.data.param_idx_enum_float.float_val); break;
                        case PX_ADSR_PARAM_DECAY:   p->decay_time = fmaxf(MIN_ADSR_TIME, cmd.data.param_idx_enum_float.float_val); break;
                        case PX_ADSR_PARAM_SUSTAIN: p->sustain_level = fmaxf(0.0f, fminf(1.0f, cmd.data.param_idx_enum_float.float_val)); break;
                        case PX_ADSR_PARAM_RELEASE: p->release_time = fmaxf(MIN_ADSR_TIME, cmd.data.param_idx_enum_float.float_val); break;
                    }
                }
                break;
            }
            case PX_CMD_SET_LFO_ADSR_ENABLED: if (cmd.data.param_idx_bool.idx < s->config.num_lfos) s->patch.template_lfos[cmd.data.param_idx_bool.idx].adsr.enabled = cmd.data.param_idx_bool.bool_val; break;
            case PX_CMD_SET_LFO_MOD_AMOUNT: if (cmd.data.param_idx_enum_float.idx < s->config.num_lfos) s->patch.template_lfos[cmd.data.param_idx_enum_float.idx].mod_amounts[cmd.data.param_idx_enum_float.enum_val] = cmd.data.param_idx_enum_float.float_val; break;
            case PX_CMD_SET_LFO_UPDATE_INTERVAL:
                s->config.lfo_update_interval_ms = cmd.data.param_float.float_val;
                s->config.samples_per_lfo_update = (int)(s->config.sample_rate * (cmd.data.param_float.float_val / 1000.0f));
                if (s->config.samples_per_lfo_update < 1) s->config.samples_per_lfo_update = 1;
                break;
            case PX_CMD_SET_FILTER_PARAM:
                switch ((PxFilterParamType)cmd.data.param_enum_float.enum_val) {
                    case PX_FILTER_PARAM_CUTOFF: s->patch.filter_cutoff_hz = fmaxf(20.0f, cmd.data.param_enum_float.float_val); break;
                    case PX_FILTER_PARAM_RESONANCE: s->patch.filter_resonance_q = fmaxf(0.5f, cmd.data.param_enum_float.float_val); break;
                    case PX_FILTER_PARAM_ENV_AMOUNT: s->patch.filter_env_amount_hz = cmd.data.param_enum_float.float_val; break;
                    case PX_FILTER_PARAM_DRIVE: s->patch.filter_drive = fmaxf(0.1f, cmd.data.param_enum_float.float_val); break;
                    case PX_FILTER_PARAM_KEYTRACK: s->patch.filter_key_track = fmaxf(0.0f, fminf(1.0f, cmd.data.param_enum_float.float_val)); break;
                    case PX_FILTER_PARAM_POLES:
                        {
                            float p = cmd.data.param_enum_float.float_val;
                            if (p < 1.5f) s->patch.filter_poles = 1;        // 6 dB/oct
                            else if (p < 2.5f) s->patch.filter_poles = 2;   // 12 dB/oct
                            else if (p < 3.5f) s->patch.filter_poles = 3;   // 18 dB/oct
                            else s->patch.filter_poles = 4;                 // 24 dB/oct
                        }
                        break;
                }
                break;
            case PX_CMD_SET_FILTER_MODE: s->patch.filter_mode = (PxFilterMode)cmd.data.param_mode.mode_val; break;
            case PX_CMD_SET_GLOBAL_PAN: s->patch.voice_pan_setting = fmaxf(-1.0f, fminf(1.0f, cmd.data.param_float.float_val)); break;
            case PX_CMD_SET_LIMITER_THRESHOLD: s->patch.limiter_threshold = fmaxf(0.1f, fminf(1.0f, cmd.data.param_float.float_val)); break;
            case PX_CMD_SET_LIMITER_RELEASE: s->patch.limiter_release_ms = fmaxf(1.0f, cmd.data.param_float.float_val); break;
            case PX_CMD_SET_UNILEGATO_ENABLED: s->patch.unilegato_enabled = cmd.data.param_bool.bool_val; break;
            case PX_CMD_SET_UNILEGATO_SLIDE_TIME: s->patch.unilegato_slide_duration_s = fmaxf(0.01f, cmd.data.param_float.float_val); break;
            case PX_CMD_NOTE_ON_VEL:
                PX_NoteOn_internal(s, cmd.data.note_on_vel.midi_note, cmd.data.note_on_vel.wave_idx,
                                   cmd.data.note_on_vel.key_id, cmd.data.note_on_vel.velocity);
                break;
            case PX_CMD_CHANNEL_AFTERTOUCH:
                s->channel_aftertouch_pressure = fmaxf(0.0f, fminf(1.0f, cmd.data.aftertouch.pressure));
                break;
            case PX_CMD_SET_VELOCITY_TO_AMP:             break; // Removed in v1.3
            case PX_CMD_SET_VELOCITY_TO_FILTER_CUTOFF:   break; // Removed in v1.3
            case PX_CMD_SET_VELOCITY_ATTACK_SCALING:     break; // Removed in v1.3
            case PX_CMD_SET_VELOCITY_TO_PARAM1:          break; // Removed in v1.3
            case PX_CMD_SET_AFTERTOUCH_TO_FILTER_CUTOFF: break; // Removed in v1.3
            case PX_CMD_SET_AFTERTOUCH_TO_VIBRATO:       break; // Removed in v1.3
            case PX_CMD_SET_MOD_MATRIX_SLOT:
                if (cmd.data.mod_slot.slot >= 0 && cmd.data.mod_slot.slot < PX_MOD_MATRIX_SLOTS) {
                    int src = cmd.data.mod_slot.src;
                    int dest = cmd.data.mod_slot.dest;
                    if (src >= 0 && src < PX_MOD_SRC_COUNT && dest >= 0 && dest < PX_MOD_DEST_COUNT) {
                        PxModSlot* slot = &s->patch.mod_matrix[cmd.data.mod_slot.slot];
                        slot->source = (PxModSource)src;
                        slot->dest = (PxModDestination)dest;
                        slot->amount = fmaxf(-1.0f, fminf(1.0f, cmd.data.mod_slot.amount));
                    } else {
                        fprintf(stderr, "PX_CMD_SET_MOD_MATRIX_SLOT: Invalid source (%d) or destination (%d)\n", src, dest);
                    }
                } else {
                    fprintf(stderr, "PX_CMD_SET_MOD_MATRIX_SLOT: Invalid slot index (%d)\n", cmd.data.mod_slot.slot);
                }
                break;
            case PX_CMD_ENABLE_MOD_MATRIX_SLOT:
                if (cmd.data.mod_enable.slot >= 0 && cmd.data.mod_enable.slot < PX_MOD_MATRIX_SLOTS) {
                    s->patch.mod_matrix[cmd.data.mod_enable.slot].enabled = cmd.data.mod_enable.enabled;
                } else {
                    fprintf(stderr, "PX_CMD_ENABLE_MOD_MATRIX_SLOT: Invalid slot index (%d)\n", cmd.data.mod_enable.slot);
                }
                break;
            case PX_CMD_CLEAR_MOD_MATRIX:
                for (int i = 0; i < PX_MOD_MATRIX_SLOTS; i++) {
                    s->patch.mod_matrix[i].enabled = false;
                    s->patch.mod_matrix[i].amount = 0.0f;
                }
                break;
            case PX_CMD_SET_GLOBAL_FILTER_ENABLED:
                s->patch.global_filter_enabled = cmd.data.global_filter_enable.enabled;
                break;
            case PX_CMD_SET_GLOBAL_FILTER_PARAM:
                {
                    int p = cmd.data.global_filter_param.param_type;
                    float v = cmd.data.global_filter_param.value;
                    switch ((PxFilterParamType)p) {
                        case PX_FILTER_PARAM_CUTOFF: s->patch.global_filter_cutoff_hz = fmaxf(20.0f, v); break;
                        case PX_FILTER_PARAM_RESONANCE: s->patch.global_filter_resonance_q = fmaxf(0.5f, v); break;
                        case PX_FILTER_PARAM_DRIVE: s->patch.global_filter_drive = fmaxf(0.1f, v); break;
                        case PX_FILTER_PARAM_POLES:
                            if (v < 1.5f) s->patch.global_filter_poles = 1;
                            else if (v < 2.5f) s->patch.global_filter_poles = 2;
                            else if (v < 3.5f) s->patch.global_filter_poles = 3;
                            else s->patch.global_filter_poles = 4;
                            break;
                        default: break; // Env/keytrack ignored for global
                    }
                }
                break;
            case PX_CMD_SET_GLOBAL_FILTER_MODE:
                s->patch.global_filter_mode = (PxFilterMode)cmd.data.global_filter_mode.mode;
                break;
            case PX_CMD_SET_VELOCITY_CURVE:
                s->patch.velocity_curve = (PxCurveType)cmd.data.curve.curve_type;
                break;
            case PX_CMD_SET_AFTERTOUCH_CURVE:
                s->patch.aftertouch_curve = (PxCurveType)cmd.data.curve.curve_type;
                break;
            case PX_CMD_CONTROL_CHANGE:
                if (cmd.data.cc.cc_num == 1) {  // Mod Wheel only
                    s->modwheel_value = fmaxf(0.0f, fminf(1.0f, cmd.data.cc.value));
                }
                break;
            case PX_CMD_PITCH_BEND:
                {
                    float normalized = cmd.data.bend.value - 0.5f;  // 0.0–1.0 → -0.5 to +0.5
                    s->pitchbend_value = normalized * 2.0f;         // → -1.0 to +1.0
                }
                break;
            case PX_CMD_SET_PITCHBEND_RANGE:
                s->patch.pitchbend_range_semitones = fmaxf(0.1f, fminf(48.0f, cmd.data.param_float.float_val));
                break;
            case PX_CMD_POLY_AFTERTOUCH:
                {
                    int key_id = cmd.data.poly_at.key_id;
                    float pressure = fmaxf(0.0f, fminf(1.0f, cmd.data.poly_at.pressure));
                    for (int i = 0; i < s->config.num_voices; i++) {
                        if (s->voices[i].active && s->voices[i].key_id == key_id) {
                            s->voices[i].poly_aftertouch_pressure = pressure;
                        }
                    }
                }
                break;
            case PX_CMD_SET_OSC_ENABLED:
                if (cmd.data.osc_bool.osc_idx >= 0 && cmd.data.osc_bool.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_bool.osc_idx].enabled = cmd.data.osc_bool.enabled;
                }
                break;
            case PX_CMD_SET_OSC_WAVE:
                if (cmd.data.osc_int.osc_idx >= 0 && cmd.data.osc_int.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_int.osc_idx].wave_idx = cmd.data.osc_int.int_val;
                    for (int i = 0; i < s->config.num_voices; i++) {
                        if (s->voices[i].active) s->voices[i].osc_wave_indices[cmd.data.osc_int.osc_idx] = cmd.data.osc_int.int_val;
                    }
                }
                break;
            case PX_CMD_SET_OSC_COARSE_TUNE:
                if (cmd.data.osc_val.osc_idx >= 0 && cmd.data.osc_val.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_val.osc_idx].coarse_semitones = cmd.data.osc_val.value;
                    for (int i = 0; i < s->config.num_voices; i++) {
                        if (s->voices[i].active) s->voices[i].current_osc_coarse_semitones[cmd.data.osc_val.osc_idx] = cmd.data.osc_val.value;
                    }
                }
                break;
            case PX_CMD_SET_OSC_FINE_TUNE:
                if (cmd.data.osc_val.osc_idx >= 0 && cmd.data.osc_val.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_val.osc_idx].fine_cents = cmd.data.osc_val.value;
                    for (int i = 0; i < s->config.num_voices; i++) {
                        if (s->voices[i].active) s->voices[i].current_osc_fine_cents[cmd.data.osc_val.osc_idx] = cmd.data.osc_val.value;
                    }
                }
                break;
            case PX_CMD_SET_OSC_MIX:
                if (cmd.data.osc_val.osc_idx >= 0 && cmd.data.osc_val.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_val.osc_idx].mix_level = fmaxf(0.0f, fminf(1.0f, cmd.data.osc_val.value));
                }
                break;
            case PX_CMD_SET_OSC_PAN:
                if (cmd.data.osc_val.osc_idx >= 0 && cmd.data.osc_val.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_val.osc_idx].pan = fmaxf(-1.0f, fminf(1.0f, cmd.data.osc_val.value));
                }
                break;
            case PX_CMD_SET_SEQUENCE_ID:
                // Legacy: Map to Osc 1
                s->patch.osc[0].sequence_id = fmaxf(-1, fminf(PX_NUM_WSEQ_BANKS - 1, cmd.data.sequence.seq_id));
                break;
            case PX_CMD_SET_OSC_SEQUENCE:
                if (cmd.data.osc_sequence.osc_idx >= 0 && cmd.data.osc_sequence.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_sequence.osc_idx].sequence_id = fmaxf(-1, fminf(PX_NUM_WSEQ_BANKS - 1, cmd.data.osc_sequence.seq_id));
                }
                break;
            case PX_CMD_SET_OSC_CROSS_MOD:
                if (cmd.data.osc_feat.osc_idx >= 0 && cmd.data.osc_feat.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_feat.osc_idx].cross_mod_enabled = cmd.data.osc_feat.enabled;
                    s->patch.osc[cmd.data.osc_feat.osc_idx].cross_mod_depth = fmaxf(0.0f, fminf(1.0f, cmd.data.osc_feat.value));
                }
                break;
            case PX_CMD_SET_OSC_PHASE_DIST:
                if (cmd.data.osc_feat.osc_idx >= 0 && cmd.data.osc_feat.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_feat.osc_idx].phase_dist_enabled = cmd.data.osc_feat.enabled;
                    s->patch.osc[cmd.data.osc_feat.osc_idx].phase_dist_amount = fmaxf(0.0f, fminf(1.0f, cmd.data.osc_feat.value));
                }
                break;
            case PX_CMD_SET_OSC_SYNC:
                if (cmd.data.osc_feat.osc_idx >= 0 && cmd.data.osc_feat.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_feat.osc_idx].osc_sync_enabled = cmd.data.osc_feat.enabled;
                    s->patch.osc[cmd.data.osc_feat.osc_idx].osc_sync_softness = fmaxf(0.0f, fminf(1.0f, cmd.data.osc_feat.value));
                }
                break;
            case PX_CMD_SET_OSC_RING_MOD:
                if (cmd.data.osc_feat.osc_idx >= 0 && cmd.data.osc_feat.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_feat.osc_idx].ring_mod_enabled = cmd.data.osc_feat.enabled;
                    s->patch.osc[cmd.data.osc_feat.osc_idx].ring_mod_depth = fmaxf(0.0f, fminf(1.0f, cmd.data.osc_feat.value));
                }
                break;
            case PX_CMD_SET_OSC_BITCRUSH:
                if (cmd.data.osc_feat.osc_idx >= 0 && cmd.data.osc_feat.osc_idx < PX_MAX_OSC_PER_VOICE) {
                    s->patch.osc[cmd.data.osc_feat.osc_idx].bitcrush_enabled = cmd.data.osc_feat.enabled;
                    s->patch.osc[cmd.data.osc_feat.osc_idx].bitcrush_depth = fmaxf(0.0f, fminf(1.0f, cmd.data.osc_feat.value));
                }
                break;
            case PX_CMD_SET_WSEQ_FIXED_TIME:
                s->patch.wseq_fixed_time = cmd.data.wseq_fixed_time.enabled;
                break;
            case PX_CMD_SET_WSEQ_REF_FREQ:
                s->patch.wseq_ref_freq = fmaxf(1.0f, cmd.data.wseq_ref_freq.freq);
                break;
            case PX_CMD_SET_GLIDE_MODE:
                s->patch.glide_mode = (PxGlideMode)cmd.data.glide_mode.mode;
                break;
            case PX_CMD_SET_GLIDE_TIME:
                s->patch.glide_time = fmaxf(0.0f, cmd.data.glide_time.time_s);
                break;
            case PX_CMD_SET_GLIDE_LEGATO_ONLY:
                s->patch.glide_legato_only = cmd.data.glide_bool.enabled;
                break;
            case PX_CMD_SET_GLIDE_ALWAYS:
                s->patch.glide_always = cmd.data.glide_bool.enabled;
                break;
            case PX_CMD_SET_PATCH_NAME:
                // Ensure null termination safe copy
                memset(s->patch.name, 0, PX_PATCH_NAME_LEN);
                strncpy(s->patch.name, cmd.data.patch_name.name, PX_PATCH_NAME_LEN - 1);
                break;
        }
    }
}

// --- UI SNAPSHOT UPDATE (Audio Thread Only) ---
static void PX_UpdateUISnapshot(PxSynth* s) {
    memcpy(s->ui_snapshot.patch_copy.name, s->patch.name, PX_PATCH_NAME_LEN);
    memcpy(s->ui_snapshot.patch_copy.template_voice_adsrs, s->patch.template_voice_adsrs, s->config.num_voice_adsrs * sizeof(PxADSRParams));
    memcpy(s->ui_snapshot.patch_copy.template_voice_adsr_mod_amounts, s->patch.template_voice_adsr_mod_amounts, s->config.num_voice_adsrs * PX_ADSR_DEST_COUNT * sizeof(float));
    memcpy(s->ui_snapshot.patch_copy.template_lfos, s->patch.template_lfos, s->config.num_lfos * sizeof(PxLFOParams));

    s->ui_snapshot.patch_copy.filter_cutoff_hz = s->patch.filter_cutoff_hz;
    s->ui_snapshot.patch_copy.filter_resonance_q = s->patch.filter_resonance_q;
    s->ui_snapshot.patch_copy.filter_env_amount_hz = s->patch.filter_env_amount_hz;
    s->ui_snapshot.patch_copy.filter_drive = s->patch.filter_drive;
    s->ui_snapshot.patch_copy.filter_key_track = s->patch.filter_key_track;
    s->ui_snapshot.patch_copy.filter_poles = s->patch.filter_poles;
    s->ui_snapshot.patch_copy.filter_mode = s->patch.filter_mode;
    s->ui_snapshot.patch_copy.voice_pan_setting = s->patch.voice_pan_setting;
    s->ui_snapshot.patch_copy.default_note_amp = s->patch.default_note_amp;
    s->ui_snapshot.patch_copy.limiter_threshold = s->patch.limiter_threshold;
    s->ui_snapshot.patch_copy.limiter_release_ms = s->patch.limiter_release_ms;
    s->ui_snapshot.patch_copy.unilegato_enabled = s->patch.unilegato_enabled;
    s->ui_snapshot.patch_copy.unilegato_slide_duration_s = s->patch.unilegato_slide_duration_s;
    memcpy(s->ui_snapshot.patch_copy.mod_matrix, s->patch.mod_matrix, PX_MOD_MATRIX_SLOTS * sizeof(PxModSlot));
    s->ui_snapshot.patch_copy.global_filter_enabled = s->patch.global_filter_enabled;
    s->ui_snapshot.patch_copy.global_filter_cutoff_hz = s->patch.global_filter_cutoff_hz;
    s->ui_snapshot.patch_copy.global_filter_resonance_q = s->patch.global_filter_resonance_q;
    s->ui_snapshot.patch_copy.global_filter_env_amount_hz = s->patch.global_filter_env_amount_hz;
    s->ui_snapshot.patch_copy.global_filter_drive = s->patch.global_filter_drive;
    s->ui_snapshot.patch_copy.global_filter_key_track = s->patch.global_filter_key_track;
    s->ui_snapshot.patch_copy.global_filter_poles = s->patch.global_filter_poles;
    s->ui_snapshot.patch_copy.global_filter_mode = s->patch.global_filter_mode;

    s->ui_snapshot.patch_copy.velocity_curve = s->patch.velocity_curve;
    s->ui_snapshot.patch_copy.aftertouch_curve = s->patch.aftertouch_curve;

    // v1.6: Copy oscillator configs
    memcpy(s->ui_snapshot.patch_copy.osc, s->patch.osc, sizeof(PxOscillator) * PX_MAX_OSC_PER_VOICE);

    s->ui_snapshot.patch_copy.pitchbend_range_semitones = s->patch.pitchbend_range_semitones;
    s->ui_snapshot.lfo_update_interval_ms = s->config.lfo_update_interval_ms;

    s->ui_snapshot.patch_copy.wseq_fixed_time = s->patch.wseq_fixed_time;
    s->ui_snapshot.patch_copy.wseq_ref_freq = s->patch.wseq_ref_freq;

    s->ui_snapshot.patch_copy.glide_mode = s->patch.glide_mode;
    s->ui_snapshot.patch_copy.glide_time = s->patch.glide_time;
    s->ui_snapshot.patch_copy.glide_legato_only = s->patch.glide_legato_only;
    s->ui_snapshot.patch_copy.glide_always = s->patch.glide_always;

    for (int i = 0; i < s->config.num_voices; ++i) { s->ui_snapshot.voices[i] = PX_GetVoiceInfo_internal(s, i); }
    for (int i = 0; i < s->config.num_lfos; ++i) { s->ui_snapshot.lfos[i] = PX_GetLFOInfo_internal(s, i); }
    s->ui_snapshot.limiter = PX_GetLimiterInfo_internal(s);
}

// --- PUBLIC API IMPLEMENTATION ---

PX_API PxSynth* PX_Create(const PxConfig* config) {
    // 1. Validate the incoming configuration.
    if (!config || config->num_voices <= 0 || config->num_voices > MAX_VOICES || config->num_lfos < 0 || config->num_lfos > MAX_LFOS || config->num_voice_adsrs <= 0 || config->sample_rate <= 0) {
        return NULL;
    }

    // 2. Allocate the main synth struct. If this fails, we can't proceed.
    PxSynth* s = (PxSynth*)calloc(1, sizeof(PxSynth));
    if (!s) return NULL;

    // 3. Copy the configuration and calculate initial timing values.
    s->config = *config;
    // Seed global trigger counter with time for per-run variation
    s->global_trigger_counter = (uint64_t)time(NULL);
    s->time_per_sample = 1.0f / config->sample_rate;
    s->glide_coeff = 1.0f - expf(-1.0f / (0.05f * s->config.sample_rate));
    s->lfo_update_countdown = 1;

    s->cmd_queue.capacity = CMD_QUEUE_SIZE;
    s->cmd_queue.buffer = (PxCommand*)calloc(CMD_QUEUE_SIZE, sizeof(PxCommand));
    if (!s->cmd_queue.buffer) { free(s); return NULL; }
    atomic_init(&s->cmd_queue.write, 0);
    atomic_init(&s->cmd_queue.read, 0);
    atomic_flag_clear(&s->cmd_queue.lock);

    // 4. Allocate memory for all dynamic arrays within the patch.
    s->patch.template_voice_adsrs = (PxADSRParams*)calloc(config->num_voice_adsrs, sizeof(PxADSRParams));
    s->patch.template_voice_adsr_mod_amounts = (float*)calloc(config->num_voice_adsrs * PX_ADSR_DEST_COUNT, sizeof(float));
    s->patch.template_lfos = (PxLFOParams*)calloc(config->num_lfos, sizeof(PxLFOParams));
    s->ui_snapshot.patch_copy.template_voice_adsrs = (PxADSRParams*)calloc(config->num_voice_adsrs, sizeof(PxADSRParams));
    s->ui_snapshot.patch_copy.template_voice_adsr_mod_amounts = (float*)calloc(config->num_voice_adsrs * PX_ADSR_DEST_COUNT, sizeof(float));
    s->ui_snapshot.patch_copy.template_lfos = (PxLFOParams*)calloc(config->num_lfos, sizeof(PxLFOParams));

    // 5. Allocate memory for the main voice pool and the template LFO instances.
    s->voices = (Voice*)calloc(config->num_voices, sizeof(Voice));
    s->template_lfo_instances = (LFOInstance*)calloc(config->num_lfos, sizeof(LFOInstance));

    // 6. Check if any of the primary allocations failed.
    if (!s->patch.template_voice_adsrs ||
        !s->patch.template_voice_adsr_mod_amounts ||
        !s->patch.template_lfos ||
        !s->ui_snapshot.patch_copy.template_voice_adsrs ||
        !s->ui_snapshot.patch_copy.template_voice_adsr_mod_amounts ||
        !s->ui_snapshot.patch_copy.template_lfos ||
        !s->voices ||
        !s->template_lfo_instances) {
        PX_Destroy(s);
        return NULL;
    }

    // 6a. Initialize shared resources (CPU/General)
    // Always initialize LFSR tables and cache, as CPU execution might be needed even in GPU mode (fallback/validation)
    // or simply for the CPU mode itself.
    // The functions are safe to call multiple times if they have internal guards (checked below).
    // px_vm_init_lfsr_tables() usually re-initializes, so checking first is better if we want to avoid re-work.
    // However, the current implementation in px_vm.h re-calculates tables.
    // Since this is PX_Create (one time setup), it is acceptable.
    px_vm_init_lfsr_tables();
    initialize_bytecode_cache();

    // 7. Allocate and initialize all per-voice internal arrays and structs.
    for (int i = 0; i < config->num_voices; ++i) {
        s->voices[i].poly_aftertouch_pressure = 0.0f;
        s->voices[i].adsrs = (ADSR*)calloc(config->num_voice_adsrs, sizeof(ADSR));
        s->voices[i].lfo_instances = (LFOInstance*)calloc(config->num_lfos, sizeof(LFOInstance));
        s->voices[i].lfo_mod_amounts_snapshot = (float*)calloc(config->num_lfos * PX_LFO_DEST_COUNT, sizeof(float));
        s->voices[i].adsr_mod_amounts = (float*)calloc(config->num_voice_adsrs * PX_ADSR_DEST_COUNT, sizeof(float));
        if (!s->voices[i].adsrs || !s->voices[i].lfo_instances || !s->voices[i].lfo_mod_amounts_snapshot || !s->voices[i].adsr_mod_amounts) {
            PX_Destroy(s);
            return NULL;
        }
        for (int j = 0; j < config->num_voice_adsrs; ++j) {
            PxADSRParams default_params = {0.0f, 0.0f, 0.0f, 0.0f, false};
            ADSR_Init(&s->voices[i].adsrs[j], &default_params, config->sample_rate);
        }
        Filter_Init(&s->voices[i].filter_instance);
        Filter_Init(&s->voices[i].filter_instance_r);
        for (int j = 0; j < config->num_lfos; ++j) {
            LFOInstance_Init(&s->voices[i].lfo_instances[j], config->sample_rate);
        }
    }

    // 8. Initialize the template LFO instances (for UI display).
    for (int i = 0; i < config->num_lfos; ++i) { LFOInstance_Init(&s->template_lfo_instances[i], config->sample_rate); }

    // 9. Initialize GPU resources if requested and enabled
    if (s->config.use_gpu) {
#ifdef POLYSONIX_USE_GPU
        px_vm_init_gpu_resources();
#else
        fprintf(stderr, "Warning: use_gpu set to true in PxConfig, but POLYSONIX_USE_GPU not defined. Falling back to CPU.\n");
        s->config.use_gpu = false;
#endif
    }

    // 9.5 Allocate Limiter Buffers
    // Allocate 2ms worth of buffer to safely hold 1ms lookahead + margin, even at high sample rates.
    int limiter_buf_size = (int)(config->sample_rate * 0.002f);
    if (limiter_buf_size < 16) limiter_buf_size = 16; // Minimum safe size
    s->limiter.buffer_capacity = limiter_buf_size;
    s->limiter.delay_line_l = (float*)calloc(limiter_buf_size, sizeof(float));
    s->limiter.delay_line_r = (float*)calloc(limiter_buf_size, sizeof(float));

    if (!s->limiter.delay_line_l || !s->limiter.delay_line_r) {
        PX_Destroy(s);
        return NULL;
    }

    // 10. Set up the default patch parameters.
    snprintf(s->patch.name, PX_PATCH_NAME_LEN, "Init Patch");
    s->patch.default_note_amp = 0.5f;
    s->patch.voice_pan_setting = 0.0f;

    // Default Voice ADSR 0 (Amp Env)
    if (config->num_voice_adsrs > 0) {
        s->patch.template_voice_adsrs[0] = (PxADSRParams){
            .attack_time=0.01f,
            .decay_time=0.4f,
            .sustain_level=0.7f,
            .release_time=1.0f,
            .enabled=true
        };
        s->patch.template_voice_adsr_mod_amounts[0 * PX_ADSR_DEST_COUNT + PX_ADSR_DEST_AMP] = 1.0f;
    }
    // Default other ADSRs
    for (int i = 1; i < config->num_voice_adsrs; ++i) {
        s->patch.template_voice_adsrs[i] = (PxADSRParams){
            .attack_time=0.01f,
            .decay_time=0.1f,
            .sustain_level=1.0f,
            .release_time=0.1f,
            .enabled=false
        };
    }
    // Default LFOs
    for (int i = 0; i < config->num_lfos; ++i) {
        s->patch.template_lfos[i] = (PxLFOParams){
            .wave_idx = 0,
            .frequency = 0.5f + i * 0.7f,
            .enabled = (i==0),
            .reset_on_key_on = false,
            .adsr = {.attack_time=0.5f, .decay_time=1.0f, .sustain_level=0.3f, .release_time=0.3f, .enabled=false}
        };
        memset(s->patch.template_lfos[i].mod_amounts, 0, sizeof(float) * PX_LFO_DEST_COUNT);
    }
    // Default Filter
    s->patch.filter_mode = PX_FILTER_MODE_LP;
    s->patch.filter_cutoff_hz = 2000.0f;
    s->patch.filter_resonance_q = 0.707f;
    s->patch.filter_env_amount_hz = 1000.0f;
    s->patch.filter_drive = 1.0f;
    s->patch.filter_key_track = 0.0f;
    s->patch.filter_poles = 3;
    // Default Limiter
    s->patch.limiter_threshold = 0.95f;
    s->patch.limiter_release_ms = 50.0f;
    InitializeEnhancedLimiter(&s->limiter, config->sample_rate, s->patch.limiter_threshold, s->patch.limiter_release_ms);
    // Initialize Unilegato tracking members
    s->patch.unilegato_enabled = false;
    s->patch.unilegato_slide_duration_s = 0.1f;

    // === v1.3: Initialize modulation matrix (all slots disabled) ===
    for (int i = 0; i < PX_MOD_MATRIX_SLOTS; i++) {
        s->patch.mod_matrix[i].source = PX_MOD_SRC_VELOCITY;
        s->patch.mod_matrix[i].dest = PX_MOD_DEST_OSC_MODA;
        s->patch.mod_matrix[i].amount = 0.0f;
        s->patch.mod_matrix[i].enabled = false;
    }

    s->patch.pitchbend_range_semitones = 2.0f;  // Classic ±2 semitones when used

    // v1.4.4: Global filter defaults (disabled)
    s->patch.global_filter_enabled = false;
    s->patch.global_filter_cutoff_hz = 2000.0f;
    s->patch.global_filter_resonance_q = 0.707f;
    s->patch.global_filter_env_amount_hz = 0.0f; // Ignored
    s->patch.global_filter_drive = 1.0f;
    s->patch.global_filter_key_track = 0.0f; // Ignored
    s->patch.global_filter_poles = 3;
    s->patch.global_filter_mode = PX_FILTER_MODE_LP;

    s->patch.velocity_curve = PX_CURVE_LINEAR;
    s->patch.aftertouch_curve = PX_CURVE_LINEAR;

    // v1.6: Initialize Oscillators
    for (int i = 0; i < PX_MAX_OSC_PER_VOICE; i++) {
        s->patch.osc[i].enabled = (i == 0); // Only Osc 1 on by default
        s->patch.osc[i].wave_idx = 0;
        s->patch.osc[i].coarse_semitones = 0.0f;
        s->patch.osc[i].fine_cents = 0.0f;
        s->patch.osc[i].mix_level = (i == 0) ? 1.0f : 0.0f;
        s->patch.osc[i].pan = 0.0f;
        s->patch.osc[i].sequence_id = -1;
        s->patch.osc[i].cross_mod_enabled = false;
        s->patch.osc[i].cross_mod_depth = 0.0f;
        s->patch.osc[i].phase_dist_enabled = false;
        s->patch.osc[i].phase_dist_amount = 0.0f;
        s->patch.osc[i].osc_sync_enabled = false;
        s->patch.osc[i].osc_sync_softness = 0.0f;
        s->patch.osc[i].ring_mod_enabled = false;
        s->patch.osc[i].ring_mod_depth = 0.0f;
        s->patch.osc[i].bitcrush_enabled = false;
        s->patch.osc[i].bitcrush_depth = 0.8f; // Default to ~4 bits for legacy WSEQ compatibility
    }

    // v1.8 Defaults
    s->patch.wseq_fixed_time = false;
    s->patch.wseq_ref_freq = 440.0f;

    // v1.8.3 Defaults
    s->patch.glide_mode = PX_GLIDE_OFF;
    s->patch.glide_time = 0.1f;
    s->patch.glide_legato_only = true;
    s->patch.glide_always = false;

    PX_UpdateUISnapshot(s);
    return s;
}

PX_API void PX_Destroy(PxSynth* s) {
    if (!s) return;
    if (s->voices) {
        for (int i = 0; i < s->config.num_voices; ++i) {
            free(s->voices[i].adsrs);
            free(s->voices[i].lfo_instances);
            free(s->voices[i].lfo_mod_amounts_snapshot);
            free(s->voices[i].adsr_mod_amounts);
        }
        free(s->voices);
    }
    if (s->template_lfo_instances) free(s->template_lfo_instances);
    if (s->patch.template_voice_adsrs) free(s->patch.template_voice_adsrs);
    if (s->patch.template_voice_adsr_mod_amounts) free(s->patch.template_voice_adsr_mod_amounts);
    if (s->patch.template_lfos) free(s->patch.template_lfos);
    if (s->ui_snapshot.patch_copy.template_voice_adsrs) free(s->ui_snapshot.patch_copy.template_voice_adsrs);
    if (s->ui_snapshot.patch_copy.template_voice_adsr_mod_amounts) free(s->ui_snapshot.patch_copy.template_voice_adsr_mod_amounts);
    if (s->ui_snapshot.patch_copy.template_lfos) free(s->ui_snapshot.patch_copy.template_lfos);
    if (s->cmd_queue.buffer) free(s->cmd_queue.buffer);
    if (s->limiter.delay_line_l) free(s->limiter.delay_line_l);
    if (s->limiter.delay_line_r) free(s->limiter.delay_line_r);
    if (s->config.use_gpu) {
#ifdef POLYSONIX_USE_GPU
        px_vm_cleanup_gpu_resources();
#endif
    }
    free(s);
}

PX_API void PX_Process(PxSynth* s, float* stereo_buffer, int num_frames) {
    if (!s) { memset(stereo_buffer, 0, num_frames * 2 * sizeof(float)); return; }
    PX_ProcessCommands(s);
    if (s->limiter.threshold != s->patch.limiter_threshold || s->limiter.release_ms_cache != s->patch.limiter_release_ms) {
        InitializeEnhancedLimiter(&s->limiter, s->config.sample_rate, s->patch.limiter_threshold, s->patch.limiter_release_ms);
    }

    // v1.4.4: Pre-calculate global filter coefficients if enabled (Optimization)
    if (s->patch.global_filter_enabled) {
        Filter_SetCoefficients(&s->global_filter_l, s->patch.global_filter_cutoff_hz,
                               s->patch.global_filter_resonance_q,
                               s->patch.global_filter_mode,
                               s->patch.global_filter_poles,
                               s->config.sample_rate);
        s->global_filter_l.drive = s->patch.global_filter_drive;

        // Copy coefficients to right channel filter
        s->global_filter_r.f_coeff = s->global_filter_l.f_coeff;
        s->global_filter_r.q_inv_coeff = s->global_filter_l.q_inv_coeff;
        s->global_filter_r.pole3_coeff = s->global_filter_l.pole3_coeff;
        s->global_filter_r.current_mode = s->global_filter_l.current_mode;
        s->global_filter_r.poles = s->global_filter_l.poles;
        s->global_filter_r.drive = s->global_filter_l.drive;
    }

    memset(stereo_buffer, 0, num_frames * 2 * sizeof(float));
    // --- Main Sample Loop ---
    for (int i = 0; i < num_frames; i++) {
        // --- LFO Update Block (runs at a slower rate) ---
        s->lfo_update_countdown--;
        if (s->lfo_update_countdown <= 0) {
            s->lfo_update_countdown = s->config.samples_per_lfo_update;
            float lfo_update_delta_time = (float)s->config.samples_per_lfo_update * s->time_per_sample;

            // Update the Template LFOs for UI display
            for (int lfo_idx = 0; lfo_idx < s->config.num_lfos; ++lfo_idx) {
                LFOInstance* tlfo_inst = &s->template_lfo_instances[lfo_idx];
                PxLFOParams* tplfo_params = &s->patch.template_lfos[lfo_idx];

                // Keep the internal instance in sync with the public patch
                ADSR_SetParams(&tlfo_inst->adsr, &tplfo_params->adsr, s->config.sample_rate);
                tlfo_inst->wave_idx = tplfo_params->wave_idx;
                tlfo_inst->frequency = tplfo_params->frequency;
                tlfo_inst->enabled = tplfo_params->enabled;

                // Simulate a continuous running state for the template ADSR for display
                if (tplfo_params->adsr.enabled) {
                    if (tlfo_inst->adsr.state == ADSR_STATE_IDLE) {
                        ADSR_TriggerAttack(&tlfo_inst->adsr);
                    }
                    ADSR_Update(&tlfo_inst->adsr, lfo_update_delta_time, s->config.sample_rate);
                } else {
                    tlfo_inst->adsr.level = 0.0f;
                    tlfo_inst->adsr.state = ADSR_STATE_IDLE;
                }

                if (tplfo_params->enabled) {
                    tlfo_inst->phase = fmodf(tlfo_inst->phase + (tplfo_params->frequency * lfo_update_delta_time), 1.0f);
                    if (tlfo_inst->phase < 0.0f) tlfo_inst->phase += 1.0f;

                    // Generate and store both raw and final values
                    tlfo_inst->current_raw_output_value = GenerateLFOValue(tlfo_inst);
                    tlfo_inst->current_output_value = tlfo_inst->current_raw_output_value * (tplfo_params->adsr.enabled ? tlfo_inst->adsr.level : 1.0f);
                } else {
                    tlfo_inst->current_raw_output_value = 0.0f;
                    tlfo_inst->current_output_value = 0.0f;
                }
            }

            // Update Per-Voice LFOs
            for (int v_idx = 0; v_idx < s->config.num_voices; ++v_idx) {
                Voice* v = &s->voices[v_idx];
                if (!v->active) continue;

                // Matrix calculation for LFOs
                float mod_sources[PX_MOD_SRC_COUNT] = {0.0f};
                float raw_vel = v->initial_velocity;
                float raw_at = s->channel_aftertouch_pressure;
                float raw_poly_at = v->poly_aftertouch_pressure;

                // v1.4.5: Apply velocity curve
                switch (s->patch.velocity_curve) {
                    case PX_CURVE_EXP: raw_vel = powf(raw_vel, 2.0f); break; // Steep high
                    case PX_CURVE_LOG: raw_vel = logf(1.0f + raw_vel * (expf(1.0f) - 1.0f)); break; // Steep low
                    case PX_CURVE_S:   raw_vel = raw_vel * raw_vel * (3.0f - 2.0f * raw_vel); break; // Smooth S
                    default: break; // Linear
                }
                mod_sources[PX_MOD_SRC_VELOCITY] = raw_vel;

                // Apply aftertouch curve (same for channel/poly)
                float curve_at = raw_at;
                switch (s->patch.aftertouch_curve) {
                    case PX_CURVE_EXP: curve_at = powf(curve_at, 2.0f); break;
                    case PX_CURVE_LOG: curve_at = logf(1.0f + curve_at * (expf(1.0f) - 1.0f)); break;
                    case PX_CURVE_S:   curve_at = curve_at * curve_at * (3.0f - 2.0f * curve_at); break;
                    default: break;
                }
                mod_sources[PX_MOD_SRC_AFTERTOUCH] = curve_at;

                float curve_poly_at = raw_poly_at;
                switch (s->patch.aftertouch_curve) {
                    case PX_CURVE_EXP: curve_poly_at = powf(curve_poly_at, 2.0f); break;
                    case PX_CURVE_LOG: curve_poly_at = logf(1.0f + curve_poly_at * (expf(1.0f) - 1.0f)); break;
                    case PX_CURVE_S:   curve_poly_at = curve_poly_at * curve_poly_at * (3.0f - 2.0f * curve_poly_at); break;
                    default: break;
                }
                mod_sources[PX_MOD_SRC_POLY_AFTERTOUCH] = curve_poly_at;

                // v1.4.5: Keyboard tracking source
                mod_sources[PX_MOD_SRC_KEY_TRACK] = ((float)v->midi_note - 60.0f) / 60.0f; // Normalized -1.0 (C0) to +1.0 (C10)

                mod_sources[PX_MOD_SRC_MODWHEEL] = s->modwheel_value;
                mod_sources[PX_MOD_SRC_PITCHBEND] = s->pitchbend_value;  // bipolar!

                float dest_mod[PX_MOD_DEST_COUNT] = {0.0f};
                for (int m = 0; m < PX_MOD_MATRIX_SLOTS; m++) {
                    PxModSlot* slot = &s->patch.mod_matrix[m];
                    if (slot->enabled) dest_mod[slot->dest] += mod_sources[slot->source] * slot->amount;
                }

                for (int lfo_idx = 0; lfo_idx < s->config.num_lfos; ++lfo_idx) {
                    LFOInstance* vlfo = &v->lfo_instances[lfo_idx];
                    PxLFOParams* tplfo = &s->patch.template_lfos[lfo_idx];

                    if (tplfo->adsr.enabled) {
                        ADSR_Update(&vlfo->adsr, lfo_update_delta_time, s->config.sample_rate);
                    } else {
                        vlfo->adsr.level = 0.0f;
                        vlfo->adsr.state = ADSR_STATE_IDLE;
                    }

                    if (tplfo->enabled) {
                        // Matrix Mod: Frequency
                        float freq_mod = dest_mod[PX_MOD_DEST_LFO1_FREQ + lfo_idx * 2];
                        float effective_freq = tplfo->frequency * powf(2.0f, freq_mod * 4.0f);
                        effective_freq = fmaxf(0.01f, effective_freq);

                        vlfo->phase = fmodf(vlfo->phase + (effective_freq * lfo_update_delta_time), 1.0f);
                        if (vlfo->phase < 0.0f) vlfo->phase += 1.0f;
                        float raw_val = GenerateLFOValue(vlfo);

                        // Matrix Mod: Depth
                        float depth_mod = dest_mod[PX_MOD_DEST_LFO1_DEPTH + lfo_idx * 2];
                        float depth_scale = 1.0f + depth_mod;
                        depth_scale = fmaxf(0.0f, depth_scale);

                        vlfo->current_output_value = raw_val * (tplfo->adsr.enabled ? vlfo->adsr.level : 1.0f) * depth_scale;
                    } else {
                        vlfo->current_output_value = 0.0f;
                    }
                }
            }
        } // End LFO update block

        float mixed_sample_l_f = 0.0f;
        float mixed_sample_r_f = 0.0f;

        // --- Per-Voice Audio-Rate Processing Loop ---
        for (int v_idx = 0; v_idx < s->config.num_voices; ++v_idx) {
            Voice *v = &s->voices[v_idx];

            // Update unilegato slide / Static Glide
            if (v->is_sliding) {
                // v->is_sliding covers legacy unilegato (GLIDE_STEP_LINEAR)
                float duration = s->patch.glide_time;
                if (duration < 0.001f) duration = 0.001f;
                v->slide_progress += s->time_per_sample / duration;
                if (v->slide_progress >= 1.0f) {
                    v->slide_progress = 1.0f;
                    v->is_sliding = false;
                    v->frequency = v->slide_target_freq;
                } else {
                    float t = v->slide_progress;
                    t = t * t * (3.0f - 2.0f * t); // Cubic
                    v->frequency = v->slide_start_freq + (v->slide_target_freq - v->slide_start_freq) * t;
                }
            } else if (s->patch.glide_mode == PX_GLIDE_SMOOTH_RC && v->frequency != v->original_frequency) {
                // RC-style exponential approach
                float duration = s->patch.glide_time;
                if (duration < 0.001f) duration = 0.001f;
                float dt = s->time_per_sample;
                float coeff = 1.0f - expf(-dt / duration);

                v->frequency += (v->original_frequency - v->frequency) * coeff;

                // Snap if close enough (avoids denormals/endless processing)
                if (fabsf(v->frequency - v->original_frequency) < 0.01f) {
                    v->frequency = v->original_frequency;
                }
            } else {
                // Ensure frequency is snapped to target if glide is disabled or inactive
                v->frequency = v->original_frequency;
            }

            // === v1.3: Apply Modulation Matrix ===
            float mod_sources[PX_MOD_SRC_COUNT] = {0.0f};
            float raw_vel = v->initial_velocity;
            float raw_at = s->channel_aftertouch_pressure;
            float raw_poly_at = v->poly_aftertouch_pressure;

            // v1.4.5: Apply velocity curve
            switch (s->patch.velocity_curve) {
                case PX_CURVE_EXP: raw_vel = powf(raw_vel, 2.0f); break; // Steep high
                case PX_CURVE_LOG: raw_vel = logf(1.0f + raw_vel * (expf(1.0f) - 1.0f)); break; // Steep low
                case PX_CURVE_S:   raw_vel = raw_vel * raw_vel * (3.0f - 2.0f * raw_vel); break; // Smooth S
                default: break; // Linear
            }
            mod_sources[PX_MOD_SRC_VELOCITY] = raw_vel;

            // Apply aftertouch curve (same for channel/poly)
            float curve_at = raw_at;
            switch (s->patch.aftertouch_curve) {
                case PX_CURVE_EXP: curve_at = powf(curve_at, 2.0f); break;
                case PX_CURVE_LOG: curve_at = logf(1.0f + curve_at * (expf(1.0f) - 1.0f)); break;
                case PX_CURVE_S:   curve_at = curve_at * curve_at * (3.0f - 2.0f * curve_at); break;
                default: break;
            }
            mod_sources[PX_MOD_SRC_AFTERTOUCH] = curve_at;

            float curve_poly_at = raw_poly_at;
            switch (s->patch.aftertouch_curve) {
                case PX_CURVE_EXP: curve_poly_at = powf(curve_poly_at, 2.0f); break;
                case PX_CURVE_LOG: curve_poly_at = logf(1.0f + curve_poly_at * (expf(1.0f) - 1.0f)); break;
                case PX_CURVE_S:   curve_poly_at = curve_poly_at * curve_poly_at * (3.0f - 2.0f * curve_poly_at); break;
                default: break;
            }
            mod_sources[PX_MOD_SRC_POLY_AFTERTOUCH] = curve_poly_at;

            // v1.4.5: Keyboard tracking source
            mod_sources[PX_MOD_SRC_KEY_TRACK] = ((float)v->midi_note - 60.0f) / 60.0f; // Normalized -1.0 (C0) to +1.0 (C10)

            mod_sources[PX_MOD_SRC_MODWHEEL] = s->modwheel_value;
            mod_sources[PX_MOD_SRC_PITCHBEND] = s->pitchbend_value;  // bipolar!

            float dest_mod[PX_MOD_DEST_COUNT] = {0.0f};
            for (int m = 0; m < PX_MOD_MATRIX_SLOTS; m++) {
                PxModSlot* slot = &s->patch.mod_matrix[m];
                if (slot->enabled) dest_mod[slot->dest] += mod_sources[slot->source] * slot->amount;
            }

            // --- Step 1: Update Live Parameters & State (with Matrix Mod) ---
            for (int j = 0; j < s->config.num_voice_adsrs; ++j) {
                PxADSRParams* tpl = &s->patch.template_voice_adsrs[j];
                PxADSRParams mod_params = *tpl;

                if (j < 3) {
                    int base = PX_MOD_DEST_ADSR1_ATTACK + j * 4;
                    mod_params.attack_time  *= powf(0.05f, dest_mod[base + 0]);
                    mod_params.decay_time   *= powf(0.1f,  dest_mod[base + 1]);
                    mod_params.sustain_level += dest_mod[base + 2];
                    mod_params.release_time *= powf(0.1f,  dest_mod[base + 3]);
                }
                mod_params.sustain_level = fmaxf(0.0f, fminf(1.0f, mod_params.sustain_level));

                ADSR_SetParams(&v->adsrs[j], &mod_params, s->config.sample_rate);
                ADSR_Update(&v->adsrs[j], s->time_per_sample, s->config.sample_rate);
            }
            for (int k = 0; k < s->config.num_lfos; ++k) {
                ADSR_SetParams(&v->lfo_instances[k].adsr, &s->patch.template_lfos[k].adsr, s->config.sample_rate);
            }

            // --- Step 2: Calculate All Modulations LIVE from the Patch ---
            float adsr_total_param1_mod=0, adsr_total_param2_mod=0, adsr_total_param3_mod=0, adsr_total_pitch_mod_st=0, adsr_total_filter_cutoff_hz=0, adsr_filter_env_input=0, adsr_total_filter_res=0;

            // Removed v1.2 hard-wired velocity/aftertouch
            float vel_amp_scale = 1.0f;

            float adsr_lfo_level_scale[s->config.num_lfos];
            for(int k=0; k<s->config.num_lfos; ++k) adsr_lfo_level_scale[k] = 1.0f;
            for (int j = 0; j < s->config.num_voice_adsrs; ++j) {
                if (s->patch.template_voice_adsrs[j].enabled && v->adsrs[j].level > INAUDIBLE_ADSR_LEVEL) {
                    float lvl = v->adsrs[j].level;
                    float* mods = &s->patch.template_voice_adsr_mod_amounts[j * PX_ADSR_DEST_COUNT];
                    adsr_total_param1_mod += lvl * mods[PX_ADSR_DEST_PARAM1];
                    adsr_total_param2_mod += lvl * mods[PX_ADSR_DEST_PARAM2];
                    adsr_total_param3_mod += lvl * mods[PX_ADSR_DEST_PARAM3];
                    adsr_total_pitch_mod_st += lvl * mods[PX_ADSR_DEST_FREQUENCY];
                    if (s->config.num_lfos > 0) adsr_lfo_level_scale[0] += (lvl-0.5f)*2.f * mods[PX_ADSR_DEST_LFO0_OUTPUT_LEVEL];
                    if (s->config.num_lfos > 1) adsr_lfo_level_scale[1] += (lvl-0.5f)*2.f * mods[PX_ADSR_DEST_LFO1_OUTPUT_LEVEL];
                    if (s->config.num_lfos > 2) adsr_lfo_level_scale[2] += (lvl-0.5f)*2.f * mods[PX_ADSR_DEST_LFO2_OUTPUT_LEVEL];
                    adsr_total_filter_cutoff_hz += lvl * mods[PX_ADSR_DEST_FILTER_CUTOFF];
                    adsr_filter_env_input += lvl * mods[PX_ADSR_DEST_FILTER_ENV_INPUT];
                    adsr_total_filter_res += lvl * mods[PX_ADSR_DEST_FILTER_RESONANCE];
                }
            }
            for(int k=0; k<s->config.num_lfos; ++k) adsr_lfo_level_scale[k] = fmaxf(0.f, adsr_lfo_level_scale[k]);
            adsr_filter_env_input = fmaxf(0.f, fminf(1.f, adsr_filter_env_input));
            float lfo_pitch_env_input=0, lfo_filter_env_input=0, lfo_amp_env_input=0, lfo_bytecode_mod_a=0, lfo_bytecode_mod_b=0, lfo_bytecode_mod_c=0, lfo_pan_env_input=0;
            for (int lfo_idx = 0; lfo_idx < s->config.num_lfos; ++lfo_idx) {
                float lfo_final_output = v->lfo_instances[lfo_idx].current_output_value * adsr_lfo_level_scale[lfo_idx];
                float* mods = s->patch.template_lfos[lfo_idx].mod_amounts;
                lfo_pitch_env_input += lfo_final_output * mods[PX_LFO_DEST_PITCH];
                lfo_filter_env_input += lfo_final_output * mods[PX_LFO_DEST_FILTER_CUTOFF];
                lfo_amp_env_input += lfo_final_output * mods[PX_LFO_DEST_AMP];
                lfo_bytecode_mod_a += lfo_final_output * mods[PX_LFO_DEST_PARAM1];
                lfo_bytecode_mod_b += lfo_final_output * mods[PX_LFO_DEST_PARAM2];
                lfo_bytecode_mod_c += lfo_final_output * mods[PX_LFO_DEST_PARAM3];
                lfo_pan_env_input += lfo_final_output * mods[PX_LFO_DEST_PAN];
            }

            // --- Step 3: Amplitude Calculation ---
            float effective_amplitude = 0.0f; bool is_amp_targeted = false; float summed_amp_contributions = 0.0f;
            for (int j = 0; j < s->config.num_voice_adsrs; ++j) {
                if (s->patch.template_voice_adsrs[j].enabled && s->patch.template_voice_adsr_mod_amounts[j * PX_ADSR_DEST_COUNT + PX_ADSR_DEST_AMP] != 0.0f) {
                    is_amp_targeted = true;
                    if (s->patch.template_voice_adsrs[j].enabled) summed_amp_contributions += v->adsrs[j].level * s->patch.template_voice_adsr_mod_amounts[j * PX_ADSR_DEST_COUNT + PX_ADSR_DEST_AMP];
                }
            }
            if (is_amp_targeted) {
                effective_amplitude = fmaxf(0.0f, fminf(1.0f, summed_amp_contributions));
            } else if (v->active) {
                effective_amplitude = s->patch.default_note_amp;
            }

            effective_amplitude *= vel_amp_scale;
            float final_amp_with_lfo = effective_amplitude * fmaxf(0.f, 1.f + lfo_amp_env_input);

            // --- Step 4: Deactivation Check ---
            if (v->active) {
                bool all_relevant_adsrs_idle = true;
                for (int j = 0; j < s->config.num_voice_adsrs; ++j) {
                    if (s->patch.template_voice_adsrs[j].enabled && s->patch.template_voice_adsr_mod_amounts[j*PX_ADSR_DEST_COUNT+PX_ADSR_DEST_AMP] != 0.f) {
                        if (v->adsrs[j].state != ADSR_STATE_IDLE) {
                            all_relevant_adsrs_idle = false;
                            break;
                        }
                    }
                }
                bool is_held = false;
                for (int j = 0; j < s->num_keys_held; j++) if (v->key_id == s->held_notes[j]) { is_held = true; break; }
                if (all_relevant_adsrs_idle && !is_held) {
                    v->active = false;
                    v->is_sliding = false;
                    v->key_id = -1;
                }
            }
            if (!v->active) continue;

            // --- Step 5: Apply Modulations and Generate Audio ---
            // 5.1 Base Frequency (Note + Slide) - Keep v->frequency "clean" for Unilegato
            if (v->is_sliding) {
                 // v->frequency is already updated by the slide logic above
            } else {
                 v->frequency = v->original_frequency;
            }

            // Calculate Effective Frequency (with Global Pitch Mod)
            float global_pitch_mod = lfo_pitch_env_input + adsr_total_pitch_mod_st;
            float effective_voice_freq = v->frequency * powf(2.0f, global_pitch_mod / 12.0f);

            // 5.2 Filter Logic
            float key_track_factor = powf(2.0f, (v->midi_note - 60.0f) / 12.0f * s->patch.filter_key_track);
            float current_filter_cutoff = (s->patch.filter_cutoff_hz * key_track_factor) + (adsr_filter_env_input + lfo_filter_env_input) * s->patch.filter_env_amount_hz + adsr_total_filter_cutoff_hz;
            current_filter_cutoff += dest_mod[PX_MOD_DEST_FILTER_CUTOFF] * 8000.0f;
            current_filter_cutoff = fmaxf(20.f, fminf(current_filter_cutoff, s->config.sample_rate * .48f));
            float current_filter_res = s->patch.filter_resonance_q + adsr_total_filter_res; current_filter_res = fmaxf(0.5f, fminf(current_filter_res, 20.f));
            Filter_SetCoefficients(&v->filter_instance, current_filter_cutoff, current_filter_res, s->patch.filter_mode, s->patch.filter_poles, s->config.sample_rate); v->filter_instance.drive = s->patch.filter_drive;
            v->filter_instance_r.f_coeff = v->filter_instance.f_coeff;
            v->filter_instance_r.q_inv_coeff = v->filter_instance.q_inv_coeff;
            v->filter_instance_r.pole3_coeff = v->filter_instance.pole3_coeff;
            v->filter_instance_r.current_mode = v->filter_instance.current_mode;
            v->filter_instance_r.poles = v->filter_instance.poles;
            v->filter_instance_r.drive = v->filter_instance.drive;

            float voice_mixed_l = 0.0f;
            float voice_mixed_r = 0.0f;

            if (s->config.osc_update_mode != PX_OSC_UPDATE_MODE_PER_SAMPLE) {
                v->update_countdown -= 1.0f;
            }

            // 5.3 Oscillator Loop (Independent Sequencing)
            for (int o = 0; o < PX_MAX_OSC_PER_VOICE; ++o) {
                if (!s->patch.osc[o].enabled) {
                    v->osc_output[o] = 0.0f; // Cache silence
                    v->osc_cycle_completed[o] = false;
                    continue;
                }
                PxSeqState* sq = &v->seq_states[o];

                // A. Calculate Seq Pitch & Glide
                float seq_pitch_mult = 1.0f;
                if (sq->current_sequence) {
                    bool do_step_glide = (sq->current_sequence->glide_mode == PX_WSEQ_GLIDE_STEP) || (sq->step_flags & PX_WSEQ_GLIDE);

                    if (sq->current_sequence->glide_mode == PX_WSEQ_GLIDE_SMOOTH) {
                        sq->target_pitch_ratio += (sq->step_pitch_ratio - sq->target_pitch_ratio) * s->glide_coeff;
                        seq_pitch_mult = sq->target_pitch_ratio;
                    } else if (do_step_glide) {
                        const PxWaveSeqStep* current_step = &sq->current_sequence->steps[sq->step_idx];

                        // v1.8: Use Cached Target Cycles
                        float effective_duration = (float)sq->current_step_target_cycles;

                        // v1.7.1: Safety for zero/short steps (retained but simplified as target is already >= 1)
                        if (effective_duration < 1.0f) effective_duration = 1.0f;

                        // v1.7.1: Clamp glide time (min 10ms to avoid smearing, max 1000 cycles to avoid extreme glides)
                        // Note: For Time-Locked mode, clamping might interfere with intent, but we keep safety limits.
                        // However, dynamic target cycles already handles the "time" aspect.
                        // We will trust current_step_target_cycles for Time-Locked unless it's extremely short.
                        if (!s->patch.wseq_fixed_time) {
                             float est_freq = effective_voice_freq;
                             if (est_freq < 20.0f) est_freq = 20.0f;
                             float min_dur_cycles = 0.01f * est_freq; // 10ms * freq
                             effective_duration = fminf(fmaxf(effective_duration, min_dur_cycles), 1000.0f);
                        }

                        if (effective_duration > 0.5f) {
                            float t = ((float)sq->cycles_counter + v->osc_phase[o]) / effective_duration;
                            t = fmaxf(0.0f, fminf(1.0f, t));

                            // v1.7.1: Curve Selection
                            if (sq->step_flags & PX_WSEQ_GLIDE) {
                                // Exponential/RC-like interpolation (Optimized: 1 - (1-t)^3)
                                float one_minus_t = 1.0f - t;
                                t = 1.0f - (one_minus_t * one_minus_t * one_minus_t);
                            }
                            // Else: Linear (Legacy PX_WSEQ_GLIDE_STEP behavior)

                            float current_ratio = sq->prev_step_pitch_ratio + (sq->step_pitch_ratio - sq->prev_step_pitch_ratio) * t;
                            seq_pitch_mult = current_ratio;
                        } else {
                            seq_pitch_mult = sq->step_pitch_ratio;
                        }
                        sq->target_pitch_ratio = sq->step_pitch_ratio; // Sync for smooth glide switching
                    } else {
                        seq_pitch_mult = sq->step_pitch_ratio;
                        sq->target_pitch_ratio = sq->step_pitch_ratio;
                    }
                }

                // B. Final Freq

                // v1.7: Sync Check (Hard Sync)
                // If Sync enabled and o > 0, check if o-1 reset.
                bool do_sync = false;
                if (o > 0 && s->patch.osc[o].osc_sync_enabled) {
                    if (v->osc_cycle_completed[o-1]) do_sync = true;
                }
                if (do_sync) {
                    float softness = s->patch.osc[o].osc_sync_softness;
                    if (softness < 0.01f) {
                        v->osc_phase[o] = 0.0f; // Hard Sync
                    } else {
                        // Soft pull to master reset (Exponential pull)
                        // softness: 0.0 (Hard) -> 1.0 (Soft/No Sync)
                        // strength = 1.0 - softness
                        v->osc_phase[o] += (1.0f - softness) * (0.0f - v->osc_phase[o]);
                    }
                }

                // Standard Pitch Calc
                float osc_pitch_mod = dest_mod[PX_MOD_DEST_OSC1_PITCH + o * PX_OSC_MOD_PARAM_COUNT];
                float tuning_st = v->current_osc_coarse_semitones[o] + (v->current_osc_fine_cents[o] / 100.0f) + osc_pitch_mod * 12.0f;
                float osc_freq = effective_voice_freq * powf(2.0f, tuning_st / 12.0f) * seq_pitch_mult;
                v->osc_vm_params[o].frequency = osc_freq;

                int base_dest = PX_MOD_DEST_OSC1_MODA + o * PX_OSC_MOD_PARAM_COUNT;
                v->osc_vm_params[o].modA = lfo_bytecode_mod_a + adsr_total_param1_mod + dest_mod[PX_MOD_DEST_OSC_MODA] * 10.0f + dest_mod[base_dest] * 10.0f;
                v->osc_vm_params[o].modB = lfo_bytecode_mod_b + adsr_total_param2_mod + dest_mod[PX_MOD_DEST_OSC_MODB] * 10.0f + dest_mod[base_dest + 1] * 10.0f;
                v->osc_vm_params[o].modC = lfo_bytecode_mod_c + adsr_total_param3_mod + dest_mod[PX_MOD_DEST_OSC_MODC] * 10.0f + dest_mod[base_dest + 2] * 10.0f;

                // v1.7: Phase Distortion
                float pd_amount = s->patch.osc[o].phase_dist_amount;
                if (s->patch.osc[o].phase_dist_enabled) {
                    pd_amount += dest_mod[PX_MOD_DEST_OSC1_PHASE_DIST + o * PX_OSC_MOD_PARAM_COUNT];
                    pd_amount = fmaxf(0.0f, fminf(1.0f, pd_amount));
                }
                float effective_phase = v->osc_phase[o];
                if (pd_amount > 0.001f) {
                    // Simple PD: phase + sin(phase) * amount
                    // Casio-style often uses piecewise lines, but sin is smoother and "analog".
                    effective_phase += sinf(effective_phase * 2.0f * PI) * pd_amount * 0.5f;
                    // Wrap if needed, though sin is periodic so it might just warp safely.
                }
                v->osc_vm_params[o].x = effective_phase * 2.f * PI;

                BytecodeChunk* chunk = default_waves[v->osc_wave_indices[o]].compiled_bytecode;
                float raw_sample = 0.0f;

                // C. Generation (CPU)
                if (s->config.osc_update_mode == PX_OSC_UPDATE_MODE_PER_SAMPLE) {
                    raw_sample = execute_bytecode(chunk, &v->osc_vm_params[o]);
                } else {
                    if (v->update_countdown <= 0.0f) {
                        v->osc_interp_samples[o][0] = v->osc_interp_samples[o][1];
                        v->osc_interp_samples[o][1] = v->osc_interp_samples[o][2];
                        v->osc_interp_samples[o][2] = v->osc_interp_samples[o][3];
                        v->osc_interp_samples[o][3] = execute_bytecode(chunk, &v->osc_vm_params[o]);
                        v->osc_phase_at_interp_start[o] = v->osc_phase_at_interp_end[o];
                        v->osc_phase_at_interp_end[o] = v->osc_phase[o];
                    }
                    float total_phase = v->osc_phase_at_interp_end[o] - v->osc_phase_at_interp_start[o];
                    if (total_phase < 0.0f) total_phase += 1.0f;
                    if (total_phase < 1e-9f) total_phase = 1e-9f;
                    float progress = v->osc_phase[o] - v->osc_phase_at_interp_start[o];
                    if (progress < 0.0f) progress += 1.0f;
                    float t = fmaxf(0.0f, fminf(1.0f, progress / total_phase));
                    raw_sample = cubic_interpolate(v->osc_interp_samples[o][0], v->osc_interp_samples[o][1], v->osc_interp_samples[o][2], v->osc_interp_samples[o][3], t);
                }

                // D. FX
                if (sq->current_sequence) {
                    if (sq->step_mute_state) raw_sample = 0.0f;

                    // v1.7.10: Amplitude Modulation
                    if (sq->step_flags & PX_WSEQ_AMP_MOD) {
                        float t = 0.0f;
                        // v1.8: Use Cached Target Cycles
                        float dur = (float)sq->current_step_target_cycles;
                        if (dur < 1.0f) dur = 1.0f;
                        t = ((float)sq->cycles_counter + sq->cycle_accumulator) / dur;
                        t = fmaxf(0.0f, fminf(1.0f, t));

                        float amp_scalar = 1.0f;
                        switch (sq->current_sequence->amp_mod_type) {
                            case PX_WSEQ_AMP_RAMP_DOWN: amp_scalar = 1.0f - t; break;
                            case PX_WSEQ_AMP_RAMP_UP:   amp_scalar = t; break;
                            case PX_WSEQ_AMP_TRIANGLE:  amp_scalar = 1.0f - fabsf(2.0f * t - 1.0f); break;
                            case PX_WSEQ_AMP_SINE:      amp_scalar = sinf(t * PI); break;
                            case PX_WSEQ_AMP_SQUARE:    amp_scalar = (t < 0.5f) ? 1.0f : 0.0f; break;
                            case PX_WSEQ_AMP_PULSE_25:  amp_scalar = (t < 0.25f) ? 1.0f : 0.0f; break;
                            case PX_WSEQ_AMP_EXP_DOWN:  amp_scalar = powf(1.0f - t, 4.0f); break;
                            case PX_WSEQ_AMP_RANDOM:    amp_scalar = sq->step_amp_mod_val; break;
                        }
                        raw_sample *= amp_scalar;
                    }

                    if (sq->step_flags & PX_WSEQ_RING_MOD) {
                        float ring_mod_src = sinf(v->osc_phase[o] * 4.0f * PI);
                        float depth = sq->current_sequence->ring_mod_depth;
                        if (sq->current_sequence->ring_mod_mod_src >= 0) depth += mod_sources[sq->current_sequence->ring_mod_mod_src];
                        depth = fmaxf(0.0f, fminf(1.0f, depth));
                        if (depth > 0.001f) {
                            float wet = raw_sample * ring_mod_src;
                            raw_sample = raw_sample * (1.0f - depth) + wet * depth;
                        }
                    }
                    if (sq->step_flags & PX_WSEQ_XMOD) {
                        float xmod = sq->current_sequence->xmod_depth;
                        if (sq->current_sequence->xmod_mod_src >= 0) xmod += mod_sources[sq->current_sequence->xmod_mod_src];
                        v->osc_phase[o] += raw_sample * xmod * 0.1f;
                        if (v->osc_phase[o] >= 1.0f) v->osc_phase[o] -= 1.0f;
                        else if (v->osc_phase[o] < 0.0f) v->osc_phase[o] += 1.0f;
                    }
                }

                // v1.7: Apply Cross Mod (Modulate Previous Osc Phase)
                if (o > 0 && s->patch.osc[o].cross_mod_enabled) {
                    float xm_depth = s->patch.osc[o].cross_mod_depth + dest_mod[PX_MOD_DEST_OSC1_CROSS_MOD_DEPTH + o * PX_OSC_MOD_PARAM_COUNT];
                    xm_depth = fmaxf(0.0f, fminf(1.0f, xm_depth));
                    if (xm_depth > 0.001f) {
                        // Modulate previous oscillator's phase for NEXT cycle (Feedback FM)
                        // Scaling: raw_sample is +/-1.
                        v->osc_phase[o-1] += raw_sample * xm_depth * 0.5f;
                        // Wrap
                        if (v->osc_phase[o-1] >= 1.0f) v->osc_phase[o-1] -= 1.0f;
                        else if (v->osc_phase[o-1] < 0.0f) v->osc_phase[o-1] += 1.0f;
                    }
                }

                // v1.7: Apply Ring Mod (Modulate Previous Osc Output)
                if (o > 0 && s->patch.osc[o].ring_mod_enabled) {
                    float rm_depth = s->patch.osc[o].ring_mod_depth + dest_mod[PX_MOD_DEST_OSC1_RING_MOD_DEPTH + o * PX_OSC_MOD_PARAM_COUNT];
                    rm_depth = fmaxf(0.0f, fminf(1.0f, rm_depth));
                    if (rm_depth > 0.001f) {
                        float wet = raw_sample * v->osc_output[o-1];
                        raw_sample = raw_sample * (1.0f - rm_depth) + wet * rm_depth;
                    }
                }

                // v1.7.1 / v1.8.2: Bitcrush (Oscillator or WSEQ Flag)
                // Determine effective bit depth with Strict Precedence:
                // 1. If WSEQ is active (current_sequence is set), the WSEQ Flag EXCLUSIVELY determines enabling.
                // 2. Else, use the Oscillator's enable flag.
                // Note: Depth always comes from Oscillator/Modulation (with fallback for legacy sequences).
                float effective_bits = 0.0f;
                bool do_bitcrush = false;

                if (sq->current_sequence) {
                    if (sq->step_flags & PX_WSEQ_BITCRUSH) {
                        do_bitcrush = true;
                        // Legacy Fallback: If Osc depth is 0, default to 4-bits (0.8f) to support old ROM sequences.
                        float base_depth = (s->patch.osc[o].bitcrush_depth > 0.001f) ? s->patch.osc[o].bitcrush_depth : 0.8f;
                        float bc_depth = base_depth + dest_mod[PX_MOD_DEST_OSC1_BITCRUSH_DEPTH + o * PX_OSC_MOD_PARAM_COUNT];
                        bc_depth = fmaxf(0.0f, fminf(1.0f, bc_depth));
                        effective_bits = 16.0f - (bc_depth * 15.0f);
                    }
                } else if (s->patch.osc[o].bitcrush_enabled) {
                    do_bitcrush = true;
                    float bc_depth = s->patch.osc[o].bitcrush_depth + dest_mod[PX_MOD_DEST_OSC1_BITCRUSH_DEPTH + o * PX_OSC_MOD_PARAM_COUNT];
                    bc_depth = fmaxf(0.0f, fminf(1.0f, bc_depth));
                    effective_bits = 16.0f - (bc_depth * 15.0f);
                }

                if (do_bitcrush) {
                    if (effective_bits < 1.0f) effective_bits = 1.0f;
                    float levels = powf(2.0f, effective_bits);
                    // Optional: Dither for low bit depths (from WSEQ logic)
                    if (effective_bits < 8.0f) {
                        float dither = ((float)(px_rand(&v->rng_state) % 1000) / 1000.0f) * 2.0f - 1.0f;
                        dither *= (1.0f / levels);
                        raw_sample += dither;
                    }
                    raw_sample = roundf(raw_sample * levels) / levels;
                }

                // Cache Output for Next Osc
                v->osc_output[o] = raw_sample;

                // E. Mix
                float level = s->patch.osc[o].mix_level + dest_mod[PX_MOD_DEST_OSC1_MIX + o * PX_OSC_MOD_PARAM_COUNT];
                level = fmaxf(0.0f, fminf(1.0f, level));
                float pan = s->patch.osc[o].pan + dest_mod[PX_MOD_DEST_OSC1_PAN + o * PX_OSC_MOD_PARAM_COUNT];
                pan = fmaxf(-1.0f, fminf(1.0f, pan));
                float osc_l = raw_sample * level * (1.0f - fmaxf(0.0f, pan));
                float osc_r = raw_sample * level * (1.0f + fminf(0.0f, pan));
                voice_mixed_l += osc_l;
                voice_mixed_r += osc_r;

                // F. Phase Update & WSeq Advancement
                float phase_inc = osc_freq * s->time_per_sample;
                bool cycle_completed = false;

                if (sq->current_sequence && (sq->step_flags & PX_WSEQ_REVERSE_PLAY)) {
                    v->osc_phase[o] -= phase_inc;
                    if (v->osc_phase[o] < 0.0f) {
                        v->osc_phase[o] += 1.0f;
                        cycle_completed = true;
                    }
                } else {
                    v->osc_phase[o] += phase_inc;
                    if (v->osc_phase[o] >= 1.0f) {
                        v->osc_phase[o] -= 1.0f;
                        cycle_completed = true;
                    }
                }
                v->osc_cycle_completed[o] = cycle_completed; // Cache for next osc sync check

                if (sq->current_sequence && !sq->finished) {
                    sq->cycle_accumulator += phase_inc;

                    int whole_cycles = (int)sq->cycle_accumulator;
                    sq->cycle_accumulator -= (float)whole_cycles;

                    sq->cycles_counter += whole_cycles;

                    // Ensure target is safe before loop
                    if (sq->current_step_target_cycles < 1) sq->current_step_target_cycles = 1;

                    while (!sq->finished && sq->cycles_counter >= sq->current_step_target_cycles) {
                        uint32_t target_cycles = sq->current_step_target_cycles;
                        // Just in case it changed inside loop or something weird
                        if (target_cycles < 1) target_cycles = 1;

                        const PxWaveSeqStep* current_step = &sq->current_sequence->steps[sq->step_idx];
                        bool force_end = (current_step->flags & PX_WSEQ_END);
                        if (force_end) {
                            switch (sq->current_sequence->end_action) {
                                case PX_WSEQ_END_STOP:
                                    // Trigger Release (Affects Voice Global)
                                    for (int j = 0; j < s->config.num_voice_adsrs; j++) if (v->adsrs[j].enabled) ADSR_TriggerRelease(&v->adsrs[j]);
                                    for (int k = 0; k < s->config.num_lfos; k++) if (s->patch.template_lfos[k].adsr.enabled) ADSR_TriggerRelease(&v->lfo_instances[k].adsr);
                                    v->key_id = -1;
                                    sq->finished = true;
                                    break;
                                case PX_WSEQ_END_HOLD: sq->finished = true; break;
                                case PX_WSEQ_END_LOOP: sq->step_idx = sq->loop_start_idx; sq->cycles_counter -= target_cycles; break;
                                case PX_WSEQ_END_PINGPONG: sq->direction *= -1; sq->step_idx += sq->direction; sq->cycles_counter -= target_cycles; break;
                                default: sq->finished = true; break;
                            }
                        } else {
                            if (current_step->flags & PX_WSEQ_JUMP_RANDOM) {
                                // Scan for valid length
                                int valid_len = 0;
                                for (int k=0; k<PX_MAX_WSEQ_STEPS; k++) {
                                    if (sq->current_sequence->steps[k].flags & PX_WSEQ_END) {
                                        valid_len = k + 1;
                                        break;
                                    }
                                    if (sq->current_sequence->steps[k].duration_cycles == 0 && sq->current_sequence->steps[k].flags == 0 && sq->current_sequence->steps[k].wave_idx == 0) {
                                         // Assume implicit end of data
                                         valid_len = k;
                                         break;
                                    }
                                    valid_len = k + 1;
                                }
                                if (valid_len < 1) valid_len = 1;
                                sq->step_idx = px_rand(&v->rng_state) % valid_len;
                            } else {
                                sq->step_idx += sq->direction;
                            }
                            if (sq->step_idx >= PX_MAX_WSEQ_STEPS || sq->step_idx < 0) {
                                if (sq->current_sequence->end_action == PX_WSEQ_END_LOOP) {
                                    sq->step_idx = sq->loop_start_idx;
                                } else if (sq->current_sequence->end_action == PX_WSEQ_END_PINGPONG) {
                                    sq->direction *= -1;
                                    sq->step_idx += sq->direction * 2;
                                } else {
                                    sq->finished = true;
                                    sq->step_idx = (sq->step_idx < 0) ? 0 : PX_MAX_WSEQ_STEPS-1;
                                }
                            }
                            sq->cycles_counter -= target_cycles;
                        }

                        if (!sq->finished) {
                            const PxWaveSeqStep* next_step = &sq->current_sequence->steps[sq->step_idx];

                            // Load Wave
                            if (next_step->flags & PX_WSEQ_USE_RND_WAVE) {
                                uint16_t range = sq->current_sequence->rnd_wave_high - sq->current_sequence->rnd_wave_low;
                                if (range > 0) v->osc_wave_indices[o] = sq->current_sequence->rnd_wave_low + (px_rand(&v->rng_state) % (range + 1));
                            } else {
                                v->osc_wave_indices[o] = next_step->wave_idx;
                            }

                            // Load Pitch
                            // v1.7.1: Start next glide from CURRENT ratio to avoid jumps if previous glide was clamped/interrupted
                            sq->prev_step_pitch_ratio = seq_pitch_mult;
                            sq->step_pitch_ratio = powf(2.0f, next_step->pitch_offset / 1200.0f);

                            // v1.8: Calculate Cycle Target for Next Step
                            float step_freq = v->original_frequency * sq->step_pitch_ratio;
                            if (s->patch.wseq_fixed_time && s->patch.wseq_ref_freq > 0.0f) {
                                float time_scaling = step_freq / s->patch.wseq_ref_freq;
                                sq->current_step_target_cycles = (uint32_t)(next_step->duration_cycles * time_scaling);
                            } else {
                                sq->current_step_target_cycles = next_step->duration_cycles;
                            }
                            if (sq->current_step_target_cycles < 1) sq->current_step_target_cycles = 1;

                            if (next_step->flags & PX_WSEQ_USE_RND_OCTAVE) {
                                if ((px_rand(&v->rng_state) % 100) < sq->current_sequence->rnd_octave_range) {
                                    float shift = (px_rand(&v->rng_state) % 2 == 0) ? 2.0f : 0.5f;
                                    sq->step_pitch_ratio *= shift;
                                }
                            }

                            sq->step_flags = next_step->flags;
                            sq->step_mute_state = false;

                            // Latch Random Amp Mod Value
                            sq->step_amp_mod_val = 1.0f;
                            if (sq->step_flags & PX_WSEQ_AMP_MOD) {
                                if (sq->current_sequence->amp_mod_type == PX_WSEQ_AMP_RANDOM) {
                                    sq->step_amp_mod_val = (float)px_rand(&v->rng_state) / UINT32_MAX;
                                }
                            }

                            if (sq->step_flags & PX_WSEQ_USE_PROB_MUTE) {
                                uint8_t score = sq->current_sequence->prob_mute_score;
                                if ((px_rand(&v->rng_state) % 100) < (score==0?50:score)) sq->step_mute_state = true;
                            }
                            if (sq->step_flags & PX_WSEQ_USE_PROB_SKIP) {
                                uint8_t score = sq->current_sequence->prob_skip_score;
                                if ((px_rand(&v->rng_state) % 100) < (score==0?50:score)) sq->cycles_counter = sq->current_step_target_cycles;
                            }

                            if (sq->step_flags & PX_WSEQ_RETRIG_ADSR) {
                                PxADSRState target_phase = (PxADSRState)sq->current_sequence->adsr_retrig_phase;
                                float target_level = (target_phase == PX_ADSR_STATE_DECAY || target_phase == PX_ADSR_STATE_SUSTAIN) ? 1.0f : 0.0f;
                                for (int j=0; j<s->config.num_voice_adsrs; ++j) {
                                    if (v->adsrs[j].enabled) {
                                        v->adsrs[j].state = target_phase;
                                        v->adsrs[j].level = (target_phase == PX_ADSR_STATE_SUSTAIN) ? v->adsrs[j].sustain_level : target_level;
                                    }
                                }
                            }

                            if (sq->step_flags & PX_WSEQ_RESET_LFO) {
                                for (int k = 0; k < s->config.num_lfos; ++k) v->lfo_instances[k].phase = 0.0f;
                            }

                            if (sq->step_flags & PX_WSEQ_LOCK_PHASE) {
                                bool do_lock = true;
                                if (sq->current_sequence->lock_phase_mod_src >= 0) {
                                    float src_val = mod_sources[sq->current_sequence->lock_phase_mod_src];
                                    if (src_val <= 0.0f) do_lock = false;
                                }
                                if (do_lock) v->osc_phase[o] = 0.0f;
                            }
                        }
                    }
                }
            }

            if (v->update_countdown <= 0.0f) {
                if (s->config.osc_update_mode == PX_OSC_UPDATE_MODE_NYQUIST) {
                    float update_rate = effective_voice_freq * s->config.nyquist_precision_multiplier;
                    update_rate = fminf(update_rate, s->config.osc_fixed_update_rate_hz);
                    if (update_rate < 1.0f) update_rate = 1.0f;
                    v->samples_per_update = s->config.sample_rate / update_rate;
                } else {
                    v->samples_per_update = s->config.sample_rate / s->config.osc_fixed_update_rate_hz;
                }
                if (v->samples_per_update < 1.0f) v->samples_per_update = 1.0f;
                v->update_countdown += v->samples_per_update;
            }

            float filtered_sample_l = Filter_Process_Oversampled(&v->filter_instance, voice_mixed_l);
            float filtered_sample_r = Filter_Process_Oversampled(&v->filter_instance_r, voice_mixed_r);
            float final_sample_l = soft_clip(filtered_sample_l * final_amp_with_lfo);
            float final_sample_r = soft_clip(filtered_sample_r * final_amp_with_lfo);

            float current_pan = s->patch.voice_pan_setting + lfo_pan_env_input;
            current_pan = fmaxf(-1.f, fminf(1.f, current_pan));
            float pan_angle = (current_pan + 1.f) * .25f * PI;
            float gain_l = cosf(pan_angle);
            float gain_r = sinf(pan_angle);

            mixed_sample_l_f += final_sample_l * gain_l;
            mixed_sample_r_f += final_sample_r * gain_r;

        } // End Voice Loop

        // Global Filter
        if (s->patch.global_filter_enabled) {
             mixed_sample_l_f = Filter_Process_Internal(&s->global_filter_l, mixed_sample_l_f);
             mixed_sample_r_f = Filter_Process_Internal(&s->global_filter_r, mixed_sample_r_f);
        }

        // Limiter
        ProcessEnhancedLimiter(&s->limiter, &mixed_sample_l_f, &mixed_sample_r_f, &stereo_buffer[i*2], &stereo_buffer[i*2+1], s->config.sample_rate);

    } // End Sample Loop
    PX_UpdateUISnapshot(s);
}
// --- THREAD-SAFE CONTROL API ---
#define PUSH_CMD_VOID(type, ...) do { if (!s) return; PxCommand _c = {.command_type = type, .data = {__VA_ARGS__}}; cmd_push(&s->cmd_queue, _c); } while(0)
// v1.1: Primary NoteOn with velocity (0.0 to 1.0, or use 127/127.0f from MIDI)
PX_API void PX_NoteOn(PxSynth* s, int midi_note, int wave_idx, int key_id, float velocity) {
    velocity = fmaxf(0.0f, fminf(1.0f, velocity));
    PUSH_CMD_VOID(PX_CMD_NOTE_ON_VEL, .note_on_vel = {midi_note, wave_idx, key_id, velocity});
}

// Backward compatibility: old signature calls with full velocity
PX_API void PX_NoteOnLegacy(PxSynth* s, int midi_note, int wave_idx, int key_id) {
    PX_NoteOn(s, midi_note, wave_idx, key_id, 1.0f);
}

PX_API void PX_PolyAftertouch(PxSynth* s, int key_id, float pressure) {
    if (!s) return;
    pressure = fmaxf(0.0f, fminf(1.0f, pressure));
    PUSH_CMD_VOID(PX_CMD_POLY_AFTERTOUCH, .poly_at = {key_id, pressure});
}

PX_API void PX_ChannelAftertouch(PxSynth* s, float pressure) {
    pressure = fmaxf(0.0f, fminf(1.0f, pressure));
    PUSH_CMD_VOID(PX_CMD_CHANNEL_AFTERTOUCH, .aftertouch = {pressure});
}

PX_API void PX_SetVelocityToAmp(PxSynth* s, float v) { PUSH_CMD_VOID(PX_CMD_SET_VELOCITY_TO_AMP, .param_float = {v}); }
PX_API float PX_GetVelocityToAmp(PxSynth* s) { return 0.0f; }

PX_API void PX_SetVelocityToFilterCutoff(PxSynth* s, float v) { PUSH_CMD_VOID(PX_CMD_SET_VELOCITY_TO_FILTER_CUTOFF, .param_float = {v}); }
PX_API float PX_GetVelocityToFilterCutoff(PxSynth* s) { return 0.0f; }

PX_API void PX_SetVelocityAttackScaling(PxSynth* s, float v) { PUSH_CMD_VOID(PX_CMD_SET_VELOCITY_ATTACK_SCALING, .param_float = {v}); }
PX_API float PX_GetVelocityAttackScaling(PxSynth* s) { return 0.0f; }

PX_API void PX_SetVelocityToParam1(PxSynth* s, float v) { PUSH_CMD_VOID(PX_CMD_SET_VELOCITY_TO_PARAM1, .param_float = {v}); }
PX_API float PX_GetVelocityToParam1(PxSynth* s) { return 0.0f; }

PX_API void PX_SetAftertouchToFilterCutoff(PxSynth* s, float v) { PUSH_CMD_VOID(PX_CMD_SET_AFTERTOUCH_TO_FILTER_CUTOFF, .param_float = {v}); }
PX_API float PX_GetAftertouchToFilterCutoff(PxSynth* s) { return 0.0f; }

PX_API void PX_SetAftertouchToVibrato(PxSynth* s, float v) { PUSH_CMD_VOID(PX_CMD_SET_AFTERTOUCH_TO_VIBRATO, .param_float = {v}); }
PX_API float PX_GetAftertouchToVibrato(PxSynth* s) { return 0.0f; }

PX_API void PX_NoteOff(PxSynth* s, int key_id) { PUSH_CMD_VOID(PX_CMD_NOTE_OFF, .note_off = {key_id}); }
PX_API void PX_SetVoiceADSRParam(PxSynth* s, int idx, PxADSRParamType p, float v) { PUSH_CMD_VOID(PX_CMD_SET_VOICE_ADSR_PARAM, .param_idx_enum_float = {idx, (int)p, v}); }
PX_API void PX_SetVoiceADSREnabled(PxSynth* s, int idx, bool enabled) { PUSH_CMD_VOID(PX_CMD_SET_VOICE_ADSR_ENABLED, .param_idx_bool = {idx, enabled}); }
PX_API void PX_SetVoiceADSRModAmount(PxSynth* s, int idx, PxADSRDestination d, float v) { PUSH_CMD_VOID(PX_CMD_SET_VOICE_ADSR_MOD_AMOUNT, .param_idx_enum_float = {idx, (int)d, v}); }
PX_API void PX_SetLFOParam(PxSynth* s, int idx, PxLFOParamType p, float v) { PUSH_CMD_VOID(PX_CMD_SET_LFO_PARAM, .param_idx_enum_float = {idx, (int)p, v}); }
PX_API void PX_SetLFOWaveform(PxSynth* s, int idx, int wave_idx) { PUSH_CMD_VOID(PX_CMD_SET_LFO_WAVEFORM, .param_idx_enum = {idx, wave_idx}); }
PX_API void PX_SetLFOEnabled(PxSynth* s, int idx, bool enabled) { PUSH_CMD_VOID(PX_CMD_SET_LFO_ENABLED, .param_idx_bool = {idx, enabled}); }
PX_API void PX_SetLFOResetOnKeyOn(PxSynth* s, int idx, bool reset) { PUSH_CMD_VOID(PX_CMD_SET_LFO_RESET_ON_KEY, .param_idx_bool = {idx, reset}); }
PX_API void PX_SetLFOADSRParam(PxSynth* s, int idx, PxADSRParamType p, float v) { PUSH_CMD_VOID(PX_CMD_SET_LFO_ADSR_PARAM, .param_idx_enum_float = {idx, (int)p, v}); }
PX_API void PX_SetLFOADSREnabled(PxSynth* s, int idx, bool enabled) { PUSH_CMD_VOID(PX_CMD_SET_LFO_ADSR_ENABLED, .param_idx_bool = {idx, enabled}); }
PX_API void PX_SetLFOModAmount(PxSynth* s, int idx, PxLFODestination d, float v) { PUSH_CMD_VOID(PX_CMD_SET_LFO_MOD_AMOUNT, .param_idx_enum_float = {idx, (int)d, v}); }
PX_API void PX_SetLFOUpdateInterval(PxSynth* s, float interval_ms) { PUSH_CMD_VOID(PX_CMD_SET_LFO_UPDATE_INTERVAL, .param_float = {interval_ms}); }
PX_API void PX_SetUnilegatoEnabled(PxSynth* s, bool enabled) {
    PUSH_CMD_VOID(PX_CMD_SET_UNILEGATO_ENABLED, .param_bool = {enabled});
    // Automatically configure Glide parameters for legacy unilegato behavior
    if (enabled) {
        PUSH_CMD_VOID(PX_CMD_SET_GLIDE_MODE, .glide_mode = {(int)PX_GLIDE_STEP_LINEAR});
        PUSH_CMD_VOID(PX_CMD_SET_GLIDE_LEGATO_ONLY, .glide_bool = {true});
    } else {
        // If disabling unilegato, should we disable glide?
        // Probably yes, to maintain legacy "Unilegato Off" state.
        // But if user explicitly set Glide Mode, this might override it.
        // Legacy function implies legacy behavior control.
        PUSH_CMD_VOID(PX_CMD_SET_GLIDE_MODE, .glide_mode = {(int)PX_GLIDE_OFF});
    }
}
PX_API void PX_SetUnilegatoSlideTime(PxSynth* s, float duration_s) {
    PUSH_CMD_VOID(PX_CMD_SET_UNILEGATO_SLIDE_TIME, .param_float = {duration_s});
    // Also update generic glide time
    PUSH_CMD_VOID(PX_CMD_SET_GLIDE_TIME, .glide_time = {duration_s});
}
PX_API void PX_SetFilterParam(PxSynth* s, PxFilterParamType p, float v) { PUSH_CMD_VOID(PX_CMD_SET_FILTER_PARAM, .param_enum_float = {(int)p, v}); }
PX_API void PX_SetFilterMode(PxSynth* s, PxFilterMode mode) { PUSH_CMD_VOID(PX_CMD_SET_FILTER_MODE, .param_mode = {(int)mode}); }

PX_API void PX_SetGlobalFilterEnabled(PxSynth* s, bool enabled) {
    PUSH_CMD_VOID(PX_CMD_SET_GLOBAL_FILTER_ENABLED, .global_filter_enable = {enabled});
}
PX_API bool PX_GetGlobalFilterEnabled(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.global_filter_enabled : false;
}

PX_API void PX_SetGlobalFilterParam(PxSynth* s, PxFilterParamType p, float v) {
    PUSH_CMD_VOID(PX_CMD_SET_GLOBAL_FILTER_PARAM, .global_filter_param = {(int)p, v});
}
PX_API float PX_GetGlobalFilterParam(PxSynth* s, PxFilterParamType p) {
    if (!s) return 0.0f;
    switch (p) {
        case PX_FILTER_PARAM_CUTOFF: return s->ui_snapshot.patch_copy.global_filter_cutoff_hz;
        case PX_FILTER_PARAM_RESONANCE: return s->ui_snapshot.patch_copy.global_filter_resonance_q;
        case PX_FILTER_PARAM_ENV_AMOUNT: return s->ui_snapshot.patch_copy.global_filter_env_amount_hz;
        case PX_FILTER_PARAM_DRIVE: return s->ui_snapshot.patch_copy.global_filter_drive;
        case PX_FILTER_PARAM_KEYTRACK: return s->ui_snapshot.patch_copy.global_filter_key_track;
        case PX_FILTER_PARAM_POLES: return (float)s->ui_snapshot.patch_copy.global_filter_poles;
    }
    return 0.0f;
}

PX_API void PX_SetGlobalFilterMode(PxSynth* s, PxFilterMode mode) {
    PUSH_CMD_VOID(PX_CMD_SET_GLOBAL_FILTER_MODE, .global_filter_mode = {(int)mode});
}
PX_API PxFilterMode PX_GetGlobalFilterMode(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.global_filter_mode : PX_FILTER_MODE_OFF;
}

PX_API void PX_SetVelocityCurve(PxSynth* s, PxCurveType curve) {
    PUSH_CMD_VOID(PX_CMD_SET_VELOCITY_CURVE, .curve = {(int)curve});
}
PX_API PxCurveType PX_GetVelocityCurve(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.velocity_curve : PX_CURVE_LINEAR;
}

PX_API void PX_SetAftertouchCurve(PxSynth* s, PxCurveType curve) {
    PUSH_CMD_VOID(PX_CMD_SET_AFTERTOUCH_CURVE, .curve = {(int)curve});
}
PX_API PxCurveType PX_GetAftertouchCurve(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.aftertouch_curve : PX_CURVE_LINEAR;
}

PX_API void PX_SetGlobalVoicePan(PxSynth* s, float pan) { PUSH_CMD_VOID(PX_CMD_SET_GLOBAL_PAN, .param_float = {pan}); }
PX_API void PX_SetLimiterThreshold(PxSynth* s, float threshold) { PUSH_CMD_VOID(PX_CMD_SET_LIMITER_THRESHOLD, .param_float = {threshold}); }
PX_API void PX_SetLimiterRelease(PxSynth* s, float release_ms) { PUSH_CMD_VOID(PX_CMD_SET_LIMITER_RELEASE, .param_float = {release_ms}); }

PX_API void PX_ControlChange(PxSynth* s, int cc_num, float value_0_1) {
    if (!s) return;
    value_0_1 = fmaxf(0.0f, fminf(1.0f, value_0_1));
    PUSH_CMD_VOID(PX_CMD_CONTROL_CHANGE, .cc = {cc_num, value_0_1});
}

PX_API void PX_PitchBend(PxSynth* s, float bend_0_1) {
    if (!s) return;
    bend_0_1 = fmaxf(0.0f, fminf(1.0f, bend_0_1));
    PUSH_CMD_VOID(PX_CMD_PITCH_BEND, .bend = {bend_0_1});
}

PX_API void PX_SetPitchBendRange(PxSynth* s, float semitones) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_SET_PITCHBEND_RANGE, .param_float = {semitones});
}

// --- v1.6 Oscillator API ---
PX_API void PX_SetOscEnabled(PxSynth* s, int osc_idx, bool enabled) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    PUSH_CMD_VOID(PX_CMD_SET_OSC_ENABLED, .osc_bool = {osc_idx, enabled});
}
PX_API bool PX_GetOscEnabled(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return false;
    return s->ui_snapshot.patch_copy.osc[osc_idx].enabled;
}

PX_API void PX_SetOscWave(PxSynth* s, int osc_idx, int wave_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    PUSH_CMD_VOID(PX_CMD_SET_OSC_WAVE, .osc_int = {osc_idx, wave_idx});
}
PX_API int PX_GetOscWave(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0;
    return s->ui_snapshot.patch_copy.osc[osc_idx].wave_idx;
}

PX_API void PX_SetOscCoarseTune(PxSynth* s, int osc_idx, float semitones) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    semitones = fmaxf(-24.0f, fminf(24.0f, semitones));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_COARSE_TUNE, .osc_val = {osc_idx, semitones});
}
PX_API float PX_GetOscCoarseTune(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].coarse_semitones;
}

PX_API void PX_SetOscFineTune(PxSynth* s, int osc_idx, float cents) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    cents = fmaxf(-100.0f, fminf(100.0f, cents));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_FINE_TUNE, .osc_val = {osc_idx, cents});
}
PX_API float PX_GetOscFineTune(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].fine_cents;
}

PX_API void PX_SetOscMix(PxSynth* s, int osc_idx, float level) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    level = fmaxf(0.0f, fminf(1.0f, level));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_MIX, .osc_val = {osc_idx, level});
}
PX_API float PX_GetOscMix(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].mix_level;
}

PX_API void PX_SetOscPan(PxSynth* s, int osc_idx, float pan) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    pan = fmaxf(-1.0f, fminf(1.0f, pan));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_PAN, .osc_val = {osc_idx, pan});
}
PX_API float PX_GetOscPan(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].pan;
}

PX_API void PX_SetOscSequence(PxSynth* s, int osc_idx, int seq_id) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    PUSH_CMD_VOID(PX_CMD_SET_OSC_SEQUENCE, .osc_sequence = {osc_idx, seq_id});
}

PX_API int PX_GetOscSequence(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return -1;
    return s->ui_snapshot.patch_copy.osc[osc_idx].sequence_id;
}

PX_API void PX_SetSequenceID(PxSynth* s, int seq_id) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_SET_SEQUENCE_ID, .sequence = {seq_id});
}

PX_API int PX_GetSequenceID(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.osc[0].sequence_id : -1;
}

// --- v1.7 Oscillator Features Implementation ---

PX_API void PX_SetOscCrossMod(PxSynth* s, int osc_idx, bool enabled, float depth) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    depth = fmaxf(0.0f, fminf(1.0f, depth));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_CROSS_MOD, .osc_feat = {osc_idx, enabled, depth});
}
PX_API float PX_GetOscCrossMod(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].cross_mod_depth;
}
PX_API bool PX_GetOscCrossModEnabled(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return false;
    return s->ui_snapshot.patch_copy.osc[osc_idx].cross_mod_enabled;
}

PX_API void PX_SetOscPhaseDist(PxSynth* s, int osc_idx, bool enabled, float amount) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    amount = fmaxf(0.0f, fminf(1.0f, amount));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_PHASE_DIST, .osc_feat = {osc_idx, enabled, amount});
}
PX_API float PX_GetOscPhaseDist(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].phase_dist_amount;
}
PX_API bool PX_GetOscPhaseDistEnabled(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return false;
    return s->ui_snapshot.patch_copy.osc[osc_idx].phase_dist_enabled;
}

PX_API void PX_SetOscSync(PxSynth* s, int osc_idx, bool enabled, float softness) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    softness = fmaxf(0.0f, fminf(1.0f, softness));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_SYNC, .osc_feat = {osc_idx, enabled, softness});
}
PX_API float PX_GetOscSync(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].osc_sync_softness;
}
PX_API bool PX_GetOscSyncEnabled(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return false;
    return s->ui_snapshot.patch_copy.osc[osc_idx].osc_sync_enabled;
}

PX_API void PX_SetOscRingMod(PxSynth* s, int osc_idx, bool enabled, float depth) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    depth = fmaxf(0.0f, fminf(1.0f, depth));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_RING_MOD, .osc_feat = {osc_idx, enabled, depth});
}
PX_API float PX_GetOscRingMod(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].ring_mod_depth;
}
PX_API bool PX_GetOscRingModEnabled(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return false;
    return s->ui_snapshot.patch_copy.osc[osc_idx].ring_mod_enabled;
}

PX_API void PX_SetOscBitcrush(PxSynth* s, int osc_idx, bool enabled, float depth) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return;
    depth = fmaxf(0.0f, fminf(1.0f, depth));
    PUSH_CMD_VOID(PX_CMD_SET_OSC_BITCRUSH, .osc_feat = {osc_idx, enabled, depth});
}
PX_API float PX_GetOscBitcrush(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return 0.0f;
    return s->ui_snapshot.patch_copy.osc[osc_idx].bitcrush_depth;
}
PX_API bool PX_GetOscBitcrushEnabled(PxSynth* s, int osc_idx) {
    if (!s || osc_idx < 0 || osc_idx >= PX_MAX_OSC_PER_VOICE) return false;
    return s->ui_snapshot.patch_copy.osc[osc_idx].bitcrush_enabled;
}

PX_API void PX_SetWSeqFixedTime(PxSynth* s, bool enabled) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_SET_WSEQ_FIXED_TIME, .wseq_fixed_time = {enabled});
}

PX_API bool PX_GetWSeqFixedTime(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.wseq_fixed_time : false;
}

PX_API void PX_SetWSeqRefFreq(PxSynth* s, float freq) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_SET_WSEQ_REF_FREQ, .wseq_ref_freq = {freq});
}

PX_API float PX_GetWSeqRefFreq(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.wseq_ref_freq : 440.0f;
}

// --- v1.8.3 Static Glide / Portamento ---
PX_API void PX_SetGlideMode(PxSynth* s, PxGlideMode mode) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_SET_GLIDE_MODE, .glide_mode = {(int)mode});
}
PX_API PxGlideMode PX_GetGlideMode(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.glide_mode : PX_GLIDE_OFF;
}

PX_API void PX_SetGlideTime(PxSynth* s, float time_s) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_SET_GLIDE_TIME, .glide_time = {time_s});
}
PX_API float PX_GetGlideTime(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.glide_time : 0.0f;
}

PX_API void PX_SetGlideLegatoOnly(PxSynth* s, bool legato_only) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_SET_GLIDE_LEGATO_ONLY, .glide_bool = {legato_only});
}
PX_API bool PX_GetGlideLegatoOnly(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.glide_legato_only : true;
}

PX_API void PX_SetGlideAlways(PxSynth* s, bool always) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_SET_GLIDE_ALWAYS, .glide_bool = {always});
}
PX_API bool PX_GetGlideAlways(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.glide_always : false;
}

PX_API void PX_SetPatchName(PxSynth* s, const char* name) {
    if (!s || !name) return;
    PxCommand cmd = {0};
    cmd.command_type = PX_CMD_SET_PATCH_NAME;
    memset(cmd.data.patch_name.name, 0, PX_PATCH_NAME_LEN);
    strncpy(cmd.data.patch_name.name, name, PX_PATCH_NAME_LEN - 1);
    cmd_push(&s->cmd_queue, cmd);
}

PX_API const char* PX_GetPatchName(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.name : "";
}

// --- THREAD-SAFE GET API ---
PX_API float PX_GetVoiceADSRParam(PxSynth* s, int idx, PxADSRParamType p) {
    if (!s || idx < 0 || idx >= s->config.num_voice_adsrs) return 0.0f;
    const PxADSRParams* params = &s->ui_snapshot.patch_copy.template_voice_adsrs[idx];
    switch(p) {
        case PX_ADSR_PARAM_ATTACK: return params->attack_time;
        case PX_ADSR_PARAM_DECAY: return params->decay_time;
        case PX_ADSR_PARAM_SUSTAIN: return params->sustain_level;
        case PX_ADSR_PARAM_RELEASE: return params->release_time;
    }
    return 0.0f;
}

PX_API bool PX_GetVoiceADSREnabled(PxSynth* s, int idx) {
    return (s && idx >= 0 && idx < s->config.num_voice_adsrs) ? s->ui_snapshot.patch_copy.template_voice_adsrs[idx].enabled : false;
}

PX_API float PX_GetVoiceADSRModAmount(PxSynth* s, int idx, PxADSRDestination d) {
    return (s && idx >= 0 && idx < s->config.num_voice_adsrs && d >= 0 && d < PX_ADSR_DEST_COUNT) ? s->ui_snapshot.patch_copy.template_voice_adsr_mod_amounts[idx * PX_ADSR_DEST_COUNT + d] : 0.0f;
}

PX_API float PX_GetLFOParam(PxSynth* s, int idx, PxLFOParamType p) {
    return (s && idx >= 0 && idx < s->config.num_lfos) ? s->ui_snapshot.patch_copy.template_lfos[idx].frequency : 0.0f;
}

PX_API int PX_GetLFOWaveform(PxSynth* s, int idx) {
    return (s && idx >= 0 && idx < s->config.num_lfos) ? s->ui_snapshot.patch_copy.template_lfos[idx].wave_idx : 0;
}

PX_API bool PX_GetLFOEnabled(PxSynth* s, int idx) {
    return (s && idx >= 0 && idx < s->config.num_lfos) ? s->ui_snapshot.patch_copy.template_lfos[idx].enabled : false;
}

PX_API bool PX_GetLFOResetOnKeyOn(PxSynth* s, int idx) {
    return (s && idx >= 0 && idx < s->config.num_lfos) ? s->ui_snapshot.patch_copy.template_lfos[idx].reset_on_key_on : false;
}

PX_API float PX_GetLFOADSRParam(PxSynth* s, int idx, PxADSRParamType p) {
    if (!s || idx < 0 || idx >= s->config.num_lfos) return 0.0f;
    const PxADSRParams* params = &s->ui_snapshot.patch_copy.template_lfos[idx].adsr;
    switch(p) {
        case PX_ADSR_PARAM_ATTACK: return params->attack_time;
        case PX_ADSR_PARAM_DECAY: return params->decay_time;
        case PX_ADSR_PARAM_SUSTAIN: return params->sustain_level;
        case PX_ADSR_PARAM_RELEASE: return params->release_time;
    }
    return 0.0f;
}

PX_API bool PX_GetLFOADSREnabled(PxSynth* s, int idx) {
    return (s && idx >= 0 && idx < s->config.num_lfos) ? s->ui_snapshot.patch_copy.template_lfos[idx].adsr.enabled : false;
}

PX_API float PX_GetLFOModAmount(PxSynth* s, int idx, PxLFODestination d) {
    return (s && idx >= 0 && idx < s->config.num_lfos && d >= 0 && d < PX_LFO_DEST_COUNT) ? s->ui_snapshot.patch_copy.template_lfos[idx].mod_amounts[d] : 0.0f;
}

PX_API float PX_GetLFOUpdateInterval(PxSynth* s) {
    return s ? s->ui_snapshot.lfo_update_interval_ms : 0.0f;
}

PX_API bool PX_GetUnilegatoEnabled(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.unilegato_enabled : false;
}

PX_API float PX_GetUnilegatoSlideTime(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.unilegato_slide_duration_s : 0.0f;
}

PX_API float PX_GetFilterParam(PxSynth* s, PxFilterParamType p) {
    if (!s) return 0.0f;
    switch(p) {
        case PX_FILTER_PARAM_CUTOFF: return s->ui_snapshot.patch_copy.filter_cutoff_hz;
        case PX_FILTER_PARAM_RESONANCE: return s->ui_snapshot.patch_copy.filter_resonance_q;
        case PX_FILTER_PARAM_ENV_AMOUNT: return s->ui_snapshot.patch_copy.filter_env_amount_hz;
        case PX_FILTER_PARAM_DRIVE: return s->ui_snapshot.patch_copy.filter_drive;
        case PX_FILTER_PARAM_KEYTRACK: return s->ui_snapshot.patch_copy.filter_key_track;
        case PX_FILTER_PARAM_POLES: return (float)s->ui_snapshot.patch_copy.filter_poles;
    }
    return 0.0f;
}

PX_API PxFilterMode PX_GetFilterMode(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.filter_mode : PX_FILTER_MODE_OFF;
}

PX_API float PX_GetGlobalVoicePan(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.voice_pan_setting : 0.0f;
}

PX_API float PX_GetLimiterThreshold(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.limiter_threshold : 1.0f;
}

PX_API float PX_GetLimiterRelease(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.limiter_release_ms : 50.0f;
}

PX_API float PX_GetPitchBendRange(PxSynth* s) {
    return s ? s->ui_snapshot.patch_copy.pitchbend_range_semitones : 2.0f;
}

PX_API PxVoiceInfo PX_GetVoiceInfo(PxSynth* s, int idx) {
    if (!s || idx < 0 || idx >= s->config.num_voices) return (PxVoiceInfo){0};
    return s->ui_snapshot.voices[idx];
}

PX_API PxLFOInfo PX_GetLFOInfo(PxSynth* s, int lfo_idx) {
    if (!s || lfo_idx < 0 || lfo_idx >= s->config.num_lfos) return (PxLFOInfo){0};
    return s->ui_snapshot.lfos[lfo_idx];
}

PX_API PxLimiterInfo PX_GetLimiterInfo(PxSynth* s) {
    return s ? s->ui_snapshot.limiter : (PxLimiterInfo){0};
}

PX_API int PX_GetNumWaveforms() {
    return NUM_WAVEFORMS; // Assumes NUM_WAVEFORMS is a constant known at compile time
}

PX_API PxWaveInfo PX_GetWaveInfo(int idx) {
    PxWaveInfo info = {.name="INVALID", .is_compiled=false};
    if(idx >= 0 && idx < NUM_WAVEFORMS) {
        info.name = default_waves[idx].name;
        info.is_compiled = (default_waves[idx].compiled_bytecode != NULL);
    }
    return info;
}

PX_API const char* PX_GetFilterModeName(PxFilterMode m) {
    return (m >= 0 && m < PX_FILTER_MODE_COUNT) ? PX_FILTER_MODE_NAMES[m] : "INVALID";
}

PX_API const char* PX_GetADSRDestinationName(PxADSRDestination d) {
    return (d >= 0 && d < PX_ADSR_DEST_COUNT) ? PX_ADSR_DEST_NAMES[d] : "INVALID";
}

PX_API const char* PX_GetLFODestinationName(PxLFODestination d) {
    return (d >= 0 && d < PX_LFO_DEST_COUNT) ? PX_LFO_DEST_NAMES[d] : "INVALID";
}

PX_API const char* PX_GetADSRStateName(PxADSRState s) {
    return (s >= 0 && s <= PX_ADSR_STATE_RELEASE) ? PX_ADSR_STATE_NAMES[s] : "?";
 }

PX_API void PX_SetModMatrixSlot(PxSynth* s, int slot_idx, PxModSource src, PxModDestination dest, float amount) {
    if (!s || slot_idx < 0 || slot_idx >= PX_MOD_MATRIX_SLOTS) return;
    PUSH_CMD_VOID(PX_CMD_SET_MOD_MATRIX_SLOT, .mod_slot = {slot_idx, (int)src, (int)dest, amount});
}

PX_API void PX_EnableModMatrixSlot(PxSynth* s, int slot_idx, bool enabled) {
    if (!s || slot_idx < 0 || slot_idx >= PX_MOD_MATRIX_SLOTS) return;
    PUSH_CMD_VOID(PX_CMD_ENABLE_MOD_MATRIX_SLOT, .mod_enable = {slot_idx, enabled});
}

PX_API void PX_ClearModMatrix(PxSynth* s) {
    if (!s) return;
    PUSH_CMD_VOID(PX_CMD_CLEAR_MOD_MATRIX, .param_bool = {false}); // Data doesn't matter
}

// --- INTERNAL IMPLEMENTATIONS ---
static void PX_NoteOn_internal(PxSynth* s, int midi_note, int wave_idx, int key_id, float velocity) {
    // 1. Determine if we should perform Legato/Glide behavior
    bool do_glide = false;
    bool is_legato_overlap = (s->num_keys_held > 0);

    // Logic: Glide if mode enabled AND (Always OR (LegatoOnly AND Overlap))
    if (s->patch.glide_mode != PX_GLIDE_OFF) {
        if (!s->patch.glide_legato_only || s->patch.glide_always) do_glide = true; // Glide Always
        else if (is_legato_overlap) do_glide = true;      // Legato Overlap
    }

    // 2. Unilegato Logic (Mono Voice Reuse)
    // If enabled, we reuse the active voice instead of stealing/finding new one.
    // Legacy support: unilegato_enabled implies glide_legato_only=true, glide_mode=STEP.
    // But we map that in setters. Here we check the unified flags.
    // However, strict "Unilegato" means MONOPHONIC behavior on overlap.
    // Standard Poly Portamento means new voice glides from old pitch.

    // We assume "Legato Only" + "Glide Enabled" -> Reuse Voice (Mono Style)?
    // Not necessarily. Many polysynths have "Legato" glide which glides newly allocated voice from previous note pitch if overlapping.
    // BUT Polysonix's "Unilegato" feature was explicitly monophonic voice reuse.
    // We will keep the voice reuse logic tied to `glide_legato_only` to mimic Unilegato behavior,
    // OR we can rely on `unilegato_enabled` flag if we want to separate "Mono Voice Mode" from "Glide".
    // User requested: "Tie into existing Unilegato mode".

    bool reuse_voice = s->patch.unilegato_enabled && is_legato_overlap;
    // Note: If we move fully to new system, we might want a separate "Mono" flag.
    // For now, respect the legacy flag for voice reuse.

    if (reuse_voice) {
        int active_voice_idx = -1;
        for (int i = 0; i < s->config.num_voices; ++i) if (s->voices[i].active && s->voices[i].midi_note == s->last_held_note_midi) { active_voice_idx = i; break; }
        if (active_voice_idx != -1) {
            Voice* v = &s->voices[active_voice_idx];

            float start_freq = v->frequency;

            // Update voice state to new note
            v->midi_note = midi_note;
            v->key_id = key_id;

            // Update Oscillator Configs
            for(int o=0; o<PX_MAX_OSC_PER_VOICE; ++o) {
                 if (o == 0) v->osc_wave_indices[o] = wave_idx;
                 else v->osc_wave_indices[o] = s->patch.osc[o].wave_idx;

                 v->current_osc_coarse_semitones[o] = s->patch.osc[o].coarse_semitones;
                 v->current_osc_fine_cents[o] = s->patch.osc[o].fine_cents;
            }

            v->original_frequency = get_midi_frequency(midi_note); // Set new base

            // Apply Glide Logic
            if (s->patch.glide_mode == PX_GLIDE_STEP_LINEAR) {
                v->is_sliding = true;
                v->slide_target_freq = v->original_frequency;
                v->slide_start_freq = start_freq;
                v->slide_progress = 0.0f;
            } else if (s->patch.glide_mode == PX_GLIDE_SMOOTH_RC) {
                v->is_sliding = false;
                // Do nothing to v->frequency, let it slide from current value in PX_Process
            } else {
                v->is_sliding = false;
                v->frequency = v->original_frequency; // Snap
            }

            for (int j = 0; j < s->config.num_voice_adsrs; ++j) if (v->adsrs[j].enabled) ADSR_TriggerAttack(&v->adsrs[j]);
            for (int k = 0; k < s->config.num_lfos; ++k) if (s->patch.template_lfos[k].adsr.enabled) ADSR_TriggerAttack(&v->lfo_instances[k].adsr);
            s->last_held_note_midi = midi_note;
            if (s->num_keys_held < s->config.num_voices) s->held_notes[s->num_keys_held++] = key_id;
            return;
        }
    }
    int voice_idx = find_inactive_voice(s);
    if (voice_idx == -1) voice_idx = find_voice_to_steal(s);
    if (voice_idx == -1) return;
    Voice* v = &s->voices[voice_idx];
    v->initial_velocity = fmaxf(0.0f, fminf(1.0f, velocity));
    v->poly_aftertouch_pressure = 0.0f;
    v->active = true; v->midi_note = midi_note;

    // v1.6: Initialize Oscillators
    for (int o = 0; o < PX_MAX_OSC_PER_VOICE; ++o) {
        if (o == 0) v->osc_wave_indices[o] = wave_idx; // Override Osc 1 with NoteOn wave
        else v->osc_wave_indices[o] = s->patch.osc[o].wave_idx;

        v->current_osc_coarse_semitones[o] = s->patch.osc[o].coarse_semitones;
        v->current_osc_fine_cents[o] = s->patch.osc[o].fine_cents;
        v->osc_phase[o] = 0.0f; // Reset phase
    }

    v->original_frequency = get_midi_frequency(midi_note);
    // Don't snap v->frequency yet if we might glide.
    // v->frequency = v->original_frequency;

    v->key_id = key_id;
    v->trigger_sequence_number = s->global_trigger_counter++;
    v->pan_position = s->patch.voice_pan_setting;

    // Apply Polyphonic Glide Logic (if not in reuse_voice block)
    // If do_glide is true, we should glide from CURRENT voice freq (portamento).
    // We only glide if the previous frequency was valid (> 1Hz) to avoid swooping up from 0Hz.
    bool can_glide_from_prev = do_glide && (v->frequency > 1.0f);

    if (can_glide_from_prev) {
        if (s->patch.glide_mode == PX_GLIDE_STEP_LINEAR) {
            v->is_sliding = true;
            v->slide_start_freq = v->frequency;
            v->slide_target_freq = v->original_frequency;
            v->slide_progress = 0.0f;
        } else if (s->patch.glide_mode == PX_GLIDE_SMOOTH_RC) {
            v->is_sliding = false;
            // Let PX_Process handle RC approach (it will see v->freq != v->orig_freq)
        } else {
            // Should not happen if do_glide is true, but safe fallback
            v->is_sliding = false;
            v->frequency = v->original_frequency;
        }
    } else {
        v->is_sliding = false;
        v->frequency = v->original_frequency; // Snap
    }

    v->slide_progress = 0.0f;
    v->update_countdown = 1.0f;
    switch (s->config.osc_update_mode) {
        case PX_OSC_UPDATE_MODE_NYQUIST: {
            float update_rate = v->frequency * DEFAULT_NYQUIST_MULTIPLIER;
            update_rate = fminf(update_rate, s->config.osc_fixed_update_rate_hz);
            if (update_rate < 1.0f) update_rate = 1.0f;
            v->samples_per_update = s->config.sample_rate / update_rate;
            break;
        }
        case PX_OSC_UPDATE_MODE_FIXED_RATE: {
            v->samples_per_update = s->config.sample_rate / s->config.osc_fixed_update_rate_hz;
            break;
        }
        default: v->samples_per_update = 1.0f;
        break;
    }
    if (v->samples_per_update < 1.0f) v->samples_per_update = 1.0f;

    // Seed PRNG deterministically using the global trigger counter to avoid rand() in audio thread
    // v1.7.1: Mix midi_note for polyphonic independence
    // Global trigger counter is seeded with time(NULL) in PX_Create for per-run variation
    v->rng_state = (uint32_t)(s->global_trigger_counter * 1664525 + 1013904223 + midi_note * 2246822507U);

    for (int o = 0; o < PX_MAX_OSC_PER_VOICE; ++o) {
        v->osc_vm_params[o].x = 0.0f;
        v->osc_vm_params[o].frequency = v->frequency;
        v->osc_vm_params[o].rand_offset = (float)rand() / RAND_MAX;
        v->osc_vm_params[o].modA = 0.0f;
        v->osc_vm_params[o].modB = 0.0f;
        v->osc_vm_params[o].modC = 0.0f;
        v->osc_vm_params[o].lfsr_type = LFSR_16BIT;
        v->osc_vm_params[o].lfsr_state = px_rand(&v->rng_state) | 1;
        v->osc_vm_params[o].lfsr_position = 0;
        v->osc_vm_params[o].lfsr_seed = v->osc_vm_params[o].lfsr_state;
    }

    for (int i = 0; i < s->config.num_voice_adsrs; ++i) {
        ADSR_Init(&v->adsrs[i], &s->patch.template_voice_adsrs[i], s->config.sample_rate);
        if (v->adsrs[i].enabled) ADSR_TriggerAttack(&v->adsrs[i]);
    }
    Filter_Init(&v->filter_instance);
    Filter_Init(&v->filter_instance_r);
    v->filter_cutoff_hz = s->patch.filter_cutoff_hz;
    v->filter_resonance_q = s->patch.filter_resonance_q;
    v->filter_mode = s->patch.filter_mode;
    v->filter_env_amount_hz = s->patch.filter_env_amount_hz;
    v->filter_instance.drive = s->patch.filter_drive;
    v->filter_instance_r.drive = s->patch.filter_drive;
    for (int i = 0; i < s->config.num_lfos; ++i) {
        LFOInstance* vlfo = &v->lfo_instances[i];
        PxLFOParams* tlfo = &s->patch.template_lfos[i];
        vlfo->wave_idx = tlfo->wave_idx;
        vlfo->frequency = tlfo->frequency;
        vlfo->enabled = tlfo->enabled;
        vlfo->reset_on_key_on = tlfo->reset_on_key_on;
        if (vlfo->reset_on_key_on || vlfo->lfo_vm_params.lfsr_state == 0) {
            vlfo->phase = 0.0f;
            vlfo->lfo_vm_params.x = 0.0f;
            vlfo->lfo_vm_params.frequency = 0.0f;
            vlfo->lfo_vm_params.rand_offset = (float)px_rand(&v->rng_state) / UINT32_MAX;
            vlfo->lfo_vm_params.modA = 0.0f;
            vlfo->lfo_vm_params.modB = 0.0f;
            vlfo->lfo_vm_params.modC = 0.0f;
            vlfo->lfo_vm_params.lfsr_type = LFSR_8BIT;
            vlfo->lfo_vm_params.lfsr_state = px_rand(&v->rng_state) | 1;
            vlfo->lfo_vm_params.lfsr_position = 0;
            vlfo->lfo_vm_params.lfsr_seed = vlfo->lfo_vm_params.lfsr_state;
        } else {
            LFOInstance* tpl_inst = &s->template_lfo_instances[i];
            vlfo->phase = tpl_inst->phase;
            vlfo->lfo_vm_params = tpl_inst->lfo_vm_params;
        }
        ADSR_Init(&vlfo->adsr, &tlfo->adsr, s->config.sample_rate);
        if (vlfo->enabled && tlfo->adsr.enabled) ADSR_TriggerAttack(&vlfo->adsr); else { vlfo->adsr.level = 0.0f; vlfo->adsr.state = ADSR_STATE_IDLE; }
    }
    if (s->config.osc_update_mode != PX_OSC_UPDATE_MODE_PER_SAMPLE) {
        for (int o = 0; o < PX_MAX_OSC_PER_VOICE; ++o) {
            v->osc_phase_at_interp_start[o] = 0.0f;
            v->osc_phase_at_interp_end[o] = 0.0f;
            VmParams temp_params = v->osc_vm_params[o];
            temp_params.x = 0.0f;
            // Use wave index 0 for default init if not set, or whatever works
            // In PX_NoteOn we'll set it properly.
            BytecodeChunk* chunk = default_waves[0].compiled_bytecode;
            float initial_value = chunk ? execute_bytecode(chunk, &temp_params) : 0.0f;
            for (int j = 0; j < 4; j++) v->osc_interp_samples[o][j] = initial_value;
        }
    }

    // --- v1.6 Per-Oscillator Sequence Initialization ---
    for (int o = 0; o < PX_MAX_OSC_PER_VOICE; ++o) {
        int seq_id = s->patch.osc[o].sequence_id;
        PxSeqState* sq = &v->seq_states[o];

        if (seq_id >= 0 && seq_id < PX_NUM_WSEQ_BANKS) {
            sq->seq_id = seq_id;
            sq->current_sequence = &ROM_WAVE_SEQUENCES[seq_id];
            sq->step_idx = 0;
            sq->direction = 1;
            sq->cycles_counter = 0;
            sq->cycle_accumulator = 0.0f;
            sq->finished = false;
            sq->target_pitch_ratio = 1.0f;
            sq->prev_step_pitch_ratio = 1.0f;

            // Cache Loop Point
            sq->loop_start_idx = 0;
            for (int i = 0; i < PX_MAX_WSEQ_STEPS; i++) {
                if (sq->current_sequence->steps[i].flags & PX_WSEQ_LOOP_POINT) {
                    sq->loop_start_idx = i;
                    break;
                }
            }

            // Initialize Random Amp Mod
            sq->step_amp_mod_val = 1.0f;
            if (sq->current_sequence->amp_mod_type == PX_WSEQ_AMP_RANDOM) {
                sq->step_amp_mod_val = (float)px_rand(&v->rng_state) / UINT32_MAX;
            }

            // Reset LFOs
            if (sq->current_sequence->reset_lfo_pos) {
                for (int k = 0; k < s->config.num_lfos; ++k) v->lfo_instances[k].phase = 0.0f;
            }

            // Load Step 0
            const PxWaveSeqStep* step = &sq->current_sequence->steps[0];

            // Override Static Wave with Sequence Wave
            v->osc_wave_indices[o] = step->wave_idx;

            sq->step_pitch_ratio = powf(2.0f, step->pitch_offset / 1200.0f);
            sq->target_pitch_ratio = sq->step_pitch_ratio;
            sq->prev_step_pitch_ratio = sq->step_pitch_ratio;
            sq->step_flags = step->flags;
            sq->step_mute_state = false;

            // v1.8: Calculate Cycle Target
            float step_freq = v->original_frequency * sq->step_pitch_ratio;
            if (s->patch.wseq_fixed_time && s->patch.wseq_ref_freq > 0.0f) {
                float time_scaling = step_freq / s->patch.wseq_ref_freq;
                sq->current_step_target_cycles = (uint32_t)(step->duration_cycles * time_scaling);
            } else {
                sq->current_step_target_cycles = step->duration_cycles;
            }
            if (sq->current_step_target_cycles < 1) sq->current_step_target_cycles = 1;

            if (sq->step_flags & PX_WSEQ_USE_PROB_MUTE) {
                 uint8_t score = sq->current_sequence->prob_mute_score;
                 uint8_t threshold = (score == 0) ? 50 : score;
                 if ((px_rand(&v->rng_state) % 100) < threshold) sq->step_mute_state = true;
            }

            if (sq->step_flags & PX_WSEQ_USE_RND_WAVE) {
                 uint16_t range = sq->current_sequence->rnd_wave_high - sq->current_sequence->rnd_wave_low;
                 if (range > 0) v->osc_wave_indices[o] = sq->current_sequence->rnd_wave_low + (px_rand(&v->rng_state) % (range + 1));
            }

            if (sq->step_flags & PX_WSEQ_USE_RND_OCTAVE) {
                 if ((px_rand(&v->rng_state) % 100) < sq->current_sequence->rnd_octave_range) {
                     float shift = (px_rand(&v->rng_state) % 2 == 0) ? 2.0f : 0.5f;
                     sq->step_pitch_ratio *= shift;
                 }
            }

            if (sq->step_flags & PX_WSEQ_LOCK_PHASE) {
                bool do_lock = true;
                if (sq->current_sequence->lock_phase_mod_src >= 0 && sq->current_sequence->lock_phase_mod_src < PX_MOD_SRC_COUNT) {
                    float src_val = 0.0f;
                    switch (sq->current_sequence->lock_phase_mod_src) {
                        case PX_MOD_SRC_VELOCITY: src_val = v->initial_velocity; break;
                        case PX_MOD_SRC_MODWHEEL: src_val = s->modwheel_value; break;
                        case PX_MOD_SRC_AFTERTOUCH: src_val = s->channel_aftertouch_pressure; break;
                        case PX_MOD_SRC_PITCHBEND: src_val = s->pitchbend_value; break;
                        case PX_MOD_SRC_KEY_TRACK: src_val = ((float)v->midi_note - 60.0f) / 60.0f; break;
                        default: src_val = 0.0f; break;
                    }
                    if (src_val <= 0.0f) do_lock = false;
                }
                if (do_lock) v->osc_phase[o] = 0.0f;
            }

        } else {
            sq->current_sequence = NULL;
            sq->step_pitch_ratio = 1.0f;
            sq->step_flags = 0;
            sq->step_mute_state = false;
        }
    }

    s->last_held_note_midi = midi_note; if (s->num_keys_held < s->config.num_voices) s->held_notes[s->num_keys_held++] = key_id;
}

static void PX_NoteOff_internal(PxSynth* s, int key_id) {
    int key_found_at_index = -1;
    for (int i = 0; i < s->num_keys_held; i++) if (s->held_notes[i] == key_id) { key_found_at_index = i; break; }
    if (key_found_at_index != -1) {
        for (int i = key_found_at_index; i < s->num_keys_held - 1; i++) s->held_notes[i] = s->held_notes[i + 1];
        s->held_notes[s->num_keys_held - 1] = -1;
        s->num_keys_held--;
    }
    s->last_held_note_midi = -1;
    if (s->num_keys_held > 0) {
        int last_key_id = s->held_notes[s->num_keys_held - 1];
        for (int i = 0; i < s->config.num_voices; i++) if (s->voices[i].active && s->voices[i].key_id == last_key_id) { s->last_held_note_midi = s->voices[i].midi_note; break; }
    }
    for (int i = 0; i < s->config.num_voices; i++) {
        if (s->voices[i].active && s->voices[i].key_id == key_id) {
            s->voices[i].is_sliding = false;
            s->voices[i].frequency = s->voices[i].original_frequency;
            for(int o=0; o<PX_MAX_OSC_PER_VOICE; ++o) s->voices[i].osc_vm_params[o].frequency = s->voices[i].frequency;
            s->voices[i].slide_progress = 0.0f;
            for (int j = 0; j < s->config.num_voice_adsrs; j++) if (s->voices[i].adsrs[j].enabled) ADSR_TriggerRelease(&s->voices[i].adsrs[j]);
            for (int k = 0; k < s->config.num_lfos; k++) if (s->patch.template_lfos[k].adsr.enabled) ADSR_TriggerRelease(&s->voices[i].lfo_instances[k].adsr);
            s->voices[i].key_id = -1;
        }
    }
}

static PxVoiceInfo PX_GetVoiceInfo_internal(PxSynth* s, int idx) {
    PxVoiceInfo info = {0};
    if (!s || idx < 0 || idx >= s->config.num_voices) return info;

    Voice* v = &s->voices[idx];
    info.active = v->active;
    info.midi_note = v->midi_note;
    info.frequency = v->frequency;
    info.pan_position = v->pan_position;

    for (int i = 0; i < s->config.num_voice_adsrs && i < 3; ++i) {
        info.adsr_states[i] = (PxADSRState)v->adsrs[i].state;
        info.adsr_levels[i] = v->adsrs[i].level;
    }
    for (int i = 0; i < s->config.num_lfos && i < 3; ++i) {
        info.lfo_outputs[i] = v->lfo_instances[i].current_output_value;
    }
    for (int i = 0; i < PX_MAX_OSC_PER_VOICE; ++i) {
        info.active_wave_indices[i] = v->osc_wave_indices[i];
    }
    float amp = 0.0f;
    bool amp_targeted = false;
    for(int j=0; j<s->config.num_voice_adsrs; ++j) {
        if (s->patch.template_voice_adsr_mod_amounts[j * PX_ADSR_DEST_COUNT + PX_ADSR_DEST_AMP] != 0.0f) {
            amp_targeted = true;
            if(v->adsrs[j].enabled) amp += v->adsrs[j].level * s->patch.template_voice_adsr_mod_amounts[j * PX_ADSR_DEST_COUNT + PX_ADSR_DEST_AMP];
        }
    }
    if (!amp_targeted && v->active) amp = s->patch.default_note_amp;
    info.effective_amplitude = fmaxf(0.0f, fminf(1.0f, amp));
    return info;
}

static PxLimiterInfo PX_GetLimiterInfo_internal(PxSynth* s) {
    PxLimiterInfo info = {
        .initialized = s->limiter.initialized,
        .gain_reduction_db = 0.0f
    };
    if (info.initialized) info.gain_reduction_db = -20.0f * log10f(fmaxf(0.001f, s->limiter.smooth_gain));
    return info;
}

static PxLFOInfo PX_GetLFOInfo_internal(PxSynth* s, int lfo_idx) {
    PxLFOInfo info = {0};
    if (!s || lfo_idx < 0 || lfo_idx >= s->config.num_lfos) return info;
    PxLFOParams* tpl_lfo_params = &s->patch.template_lfos[lfo_idx];
    info.enabled = tpl_lfo_params->enabled;
    info.wave_idx = tpl_lfo_params->wave_idx;
    info.frequency = tpl_lfo_params->frequency;
    info.reset_on_key_on = tpl_lfo_params->reset_on_key_on;
    info.adsr_enabled = tpl_lfo_params->adsr.enabled;
    LFOInstance* tpl_lfo_instance = &s->template_lfo_instances[lfo_idx];
    info.adsr_level = tpl_lfo_instance->adsr.level;
    info.phase = tpl_lfo_instance->phase;
    info.raw_output = tpl_lfo_instance->current_raw_output_value;
    info.final_output = tpl_lfo_instance->current_output_value;
    return info;
}

#endif // POLYSONIX_IMPLEMENTATION
