/***************************************************************************************************
*
*   sit/aud/midi_device_callbacks.h - MIDI Device Callback Implementations
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Centralized MIDI callback implementations for all MIDI-enabled devices.
*   
*   This file bridges the gap between MIDI input (midi_device.h) and device control arrays
*   (device_wrappers.h). Each device gets callback functions that map MIDI messages to control
*   parameter indices.
*   
*   ARCHITECTURE:
*   
*       midi.h                    - Low-level MIDI (streams, routing, buffers)
*           ↓
*       midi_device.h             - Callback infrastructure (SIT_MidiDevice, SIT_MidiCallbacks)
*           ↓
*       midi_device_callbacks.h   - Device-specific MIDI→control mappings (THIS FILE)
*           ↓
*       device_wrappers.h         - DSP processing with control arrays
*   
*   BENEFITS:
*   - Single source of truth for all MIDI mappings
*   - Easy to see which CCs control which parameters
*   - Device implementations stay focused on DSP
*   - MIDI mappings can be changed without touching device code
*   - Clear documentation of MIDI learn capabilities
*   
*   USAGE PATTERN:
*   
*       // In node graph creation code:
*       SIT_MidiDevice *midi = SIT_MidiDevice_Create("Compander", SIT_MIDI_DEVICE_EFFECT, 
*                                                     SIT_MIDI_CAP_INPUT, node);
*       SIT_MidiCallbacks callbacks = {0};
*       callbacks.on_control_change = _SituationCompanderOnControlChange;
*       callbacks.device_ptr = node->controls;  // Pass control array
*       SIT_MidiDevice_SetCallbacks(midi, &callbacks);
*   
*   CONTROL ARRAY CONVENTION:
*   - Each device wrapper in device_wrappers.h receives a float* controls array
*   - MIDI callbacks write directly to this array (thread-safe if on audio thread)
*   - CC values (0-127) are normalized to parameter ranges
*   
***************************************************************************************************/

#ifndef SITUATION_MIDI_DEVICE_CALLBACKS_H
#define SITUATION_MIDI_DEVICE_CALLBACKS_H

#include "midi_device.h"
#include "../situation_base_etc.h"
#include "tone_synth_graph.h"
#include <math.h>
#include <stdbool.h>

extern int SituationGetAudioPlaybackSampleRate(void);

// ================================================================================================
// DEVICE IDENTITY HELPERS
// ================================================================================================

/**
 * @brief Create device identity for Situation Audio devices.
 * 
 * Manufacturer ID: 0x00 0x53 0x49 ("SI" for Situation)
 * Family: 0x00 0x01 (Audio FX)
 * Model: Device-specific
 * Version: 0x01 0x00 0x00 0x00 (v1.0.0.0)
 */
static inline SIT_MidiDeviceIdentity _SituationCreateDeviceIdentity(uint8_t model_lsb, const char* name) {
    SIT_MidiDeviceIdentity identity = {0};
    
    // Manufacturer ID: "SI" for Situation (0x00 0x53 0x49)
    identity.manufacturer_id[0] = 0x00;
    identity.manufacturer_id[1] = 0x53;  // 'S'
    identity.manufacturer_id[2] = 0x49;  // 'I'
    
    // Family: Audio FX
    identity.family[0] = 0x00;
    identity.family[1] = 0x01;
    
    // Model: Device-specific
    identity.model[0] = 0x00;
    identity.model[1] = model_lsb;
    
    // Version: 1.0.0.0
    identity.version[0] = 0x01;
    identity.version[1] = 0x00;
    identity.version[2] = 0x00;
    identity.version[3] = 0x00;
    
    // Device name (not sent in SysEx, just for reference)
    strncpy(identity.device_name, name, sizeof(identity.device_name) - 1);
    
    return identity;
}

/**
 * @brief Device model IDs for Situation Audio devices.
 */
enum {
    SIT_MODEL_COMPANDER      = 0x01,
    SIT_MODEL_DYNAMICS       = 0x02,
    SIT_MODEL_FILTER         = 0x03,
    SIT_MODEL_EQ_4BAND       = 0x04,
    SIT_MODEL_REVERB         = 0x05,
    SIT_MODEL_CHORUS         = 0x06,
    SIT_MODEL_OVERDRIVE      = 0x07,
    SIT_MODEL_PANNER         = 0x08,
    SIT_MODEL_LFO            = 0x09,
    SIT_MODEL_ECHO           = 0x0A,
    SIT_MODEL_PHASER         = 0x0B,
    SIT_MODEL_EXCITER        = 0x0C,
    SIT_MODEL_STUDIO_REVERB  = 0x0D,
    SIT_MODEL_SPRING_REVERB  = 0x0E,
    SIT_MODEL_SST282         = 0x0F,
    SIT_MODEL_MASTERING_AMP  = 0x10,
    SIT_MODEL_MAXIMIZER      = 0x11,
    SIT_MODEL_TONE_SYNTH     = 0x12,
};

// ================================================================================================
// HELPER FUNCTIONS
// ================================================================================================

/**
 * @brief Normalize MIDI CC value (0-127) to float range.
 */
static inline float _SituationNormalizeMidiCC(uint8_t value, float min, float max) {
    float normalized = (float)value / 127.0f;
    return min + normalized * (max - min);
}

/**
 * @brief Normalize MIDI CC value to logarithmic range (for frequency, etc.).
 */
static inline float _SituationNormalizeMidiCCLog(uint8_t value, float min, float max) {
    float normalized = (float)value / 127.0f;
    float log_min = logf(min);
    float log_max = logf(max);
    return expf(log_min + normalized * (log_max - log_min));
}

/**
 * @brief Normalize MIDI CC value to decibel range.
 */
static inline float _SituationNormalizeMidiCCDb(uint8_t value, float min_db, float max_db) {
    return _SituationNormalizeMidiCC(value, min_db, max_db);
}

// ================================================================================================
// 14-BIT MIDI CC SUPPORT (MSB/LSB PAIRS)
// ================================================================================================

/**
 * @brief 14-bit CC state tracker.
 * @details Tracks MSB/LSB pairs for high-resolution MIDI control.
 * 
 * MIDI 14-bit CC uses two CC messages:
 * - MSB (Most Significant Byte): CC 0-31 (coarse control, 7 bits)
 * - LSB (Least Significant Byte): CC 32-63 (fine control, 7 bits)
 * 
 * Combined value: (MSB << 7) | LSB = 0-16383 (14 bits)
 * 
 * Example:
 *   CC 1 (MSB) = 64 → 8192 (64 << 7)
 *   CC 33 (LSB) = 32 → 32
 *   Combined = 8224 (14-bit value)
 */
typedef struct {
    uint8_t msb;           // Most significant 7 bits (CC 0-31)
    uint8_t lsb;           // Least significant 7 bits (CC 32-63)
    uint8_t has_msb;       // 1 if MSB received
    uint8_t has_lsb;       // 1 if LSB received
} SIT_MidiCC14Bit;

/**
 * @brief Update 14-bit CC state.
 * @param state 14-bit CC state tracker.
 * @param controller CC number (0-63).
 * @param value CC value (0-127).
 * @return 1 if complete 14-bit value ready, 0 if waiting for pair.
 */
static inline int _SituationUpdate14BitCC(SIT_MidiCC14Bit* state, uint8_t cc_number, uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    if (cc_number < 32) {
        // MSB (CC 0-31)
        state->msb = value;
        state->has_msb = 1;
        // Return immediately with 7-bit value (for responsiveness)
        // LSB will refine it later
        return 1;
    } else if (cc_number >= 32 && cc_number < 64) {
        // LSB (CC 32-63)
        state->lsb = value;
        state->has_lsb = 1;
        // Only return complete value if we have MSB
        return state->has_msb;
    }
    return 0;
}

/**
 * @brief Get 14-bit value from MSB/LSB pair.
 * @param state 14-bit CC state tracker.
 * @return 14-bit value (0-16383).
 */
static inline uint16_t _SituationGet14BitValue(const SIT_MidiCC14Bit* state) {
    if (state->has_msb && state->has_lsb) {
        // Full 14-bit resolution
        return ((uint16_t)state->msb << 7) | (uint16_t)state->lsb;
    } else if (state->has_msb) {
        // Only MSB, use 7-bit resolution (shift to 14-bit range)
        return (uint16_t)state->msb << 7;
    }
    return 0;
}

/**
 * @brief Normalize 14-bit CC value (0-16383) to float range.
 */
static inline float _SituationNormalize14BitCC(uint16_t value_14bit, float min, float max) {
    float normalized = (float)value_14bit / 16383.0f;
    return min + normalized * (max - min);
}

/**
 * @brief Normalize 14-bit CC value to logarithmic range.
 */
static inline float _SituationNormalize14BitCCLog(uint16_t value_14bit, float min, float max) {
    float normalized = (float)value_14bit / 16383.0f;
    float log_min = logf(min);
    float log_max = logf(max);
    return expf(log_min + normalized * (log_max - log_min));
}

/**
 * @brief Normalize 14-bit CC value to decibel range.
 */
static inline float _SituationNormalize14BitCCDb(uint16_t value_14bit, float min_db, float max_db) {
    return _SituationNormalize14BitCC(value_14bit, min_db, max_db);
}

// ================================================================================================
// COMPANDER CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Compander (3-band multiband compander with EQ).
 * 
 * DEVICE: Compander (SITUATION_NODE_COMPANDER)
 * CONTROLS: 24 total (3 bands × 8 params)
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 * 
 * Band 0 (Low, <200Hz):
 *   CC 16 → Control 0  (comp_thresh: 0.0-1.0)
 *   CC 17 → Control 1  (exp_thresh: 0.0-1.0)
 *   CC 18 → Control 2  (comp_slope: 1.0-10.0)
 *   CC 19 → Control 3  (exp_slope: 1.0-10.0)
 *   CC 20 → Control 4  (noise_gate: -96dB to 0dB)
 *   CC 21 → Control 5  (bell_freq: 20Hz-200Hz, log)
 *   CC 22 → Control 6  (bell_gain: -24dB to +24dB)
 *   CC 23 → Control 7  (bell_Q: 0.1-10.0)
 * 
 * Band 1 (Mid, 200-4000Hz):
 *   CC 24 → Control 8  (comp_thresh: 0.0-1.0)
 *   CC 25 → Control 9  (exp_thresh: 0.0-1.0)
 *   CC 26 → Control 10 (comp_slope: 1.0-10.0)
 *   CC 27 → Control 11 (exp_slope: 1.0-10.0)
 *   CC 28 → Control 12 (noise_gate: -96dB to 0dB)
 *   CC 29 → Control 13 (bell_freq: 200Hz-4000Hz, log)
 *   CC 30 → Control 14 (bell_gain: -24dB to +24dB)
 *   CC 31 → Control 15 (bell_Q: 0.1-10.0)
 * 
 * Band 2 (High, >4000Hz):
 *   CC 80 → Control 16 (comp_thresh: 0.0-1.0)
 *   CC 81 → Control 17 (exp_thresh: 0.0-1.0)
 *   CC 82 → Control 18 (comp_slope: 1.0-10.0)
 *   CC 83 → Control 19 (exp_slope: 1.0-10.0)
 *   CC 84 → Control 20 (noise_gate: -96dB to 0dB)
 *   CC 85 → Control 21 (bell_freq: 4000Hz-20000Hz, log)
 *   CC 86 → Control 22 (bell_gain: -24dB to +24dB)
 *   CC 87 → Control 23 (bell_Q: 0.1-10.0)
 */
static void _SituationCompanderOnControlChange(void* device_ptr, uint8_t controller, 
                                                uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;  // Compander responds to all channels
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    // Band 0 (Low): CC 16-23 → Controls 0-7
    if (controller >= 16 && controller <= 23) {
        int param = controller - 16;
        int ctrl_idx = param;
        
        switch (param) {
            case 0: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;      // comp_thresh
            case 1: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;      // exp_thresh
            case 2: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;     // comp_slope
            case 3: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;     // exp_slope
            case 4: controls[ctrl_idx] = _SituationNormalizeMidiCCDb(value, -96.0f, 0.0f); break;  // noise_gate
            case 5: controls[ctrl_idx] = _SituationNormalizeMidiCCLog(value, 20.0f, 200.0f); break;// bell_freq
            case 6: controls[ctrl_idx] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break; // bell_gain
            case 7: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;     // bell_Q
        }
    }
    // Band 1 (Mid): CC 24-31 → Controls 8-15
    else if (controller >= 24 && controller <= 31) {
        int param = controller - 24;
        int ctrl_idx = 8 + param;
        
        switch (param) {
            case 0: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;
            case 1: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;
            case 2: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
            case 3: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
            case 4: controls[ctrl_idx] = _SituationNormalizeMidiCCDb(value, -96.0f, 0.0f); break;
            case 5: controls[ctrl_idx] = _SituationNormalizeMidiCCLog(value, 200.0f, 4000.0f); break;
            case 6: controls[ctrl_idx] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;
            case 7: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;
        }
    }
    // Band 2 (High): CC 80-87 → Controls 16-23
    else if (controller >= 80 && controller <= 87) {
        int param = controller - 80;
        int ctrl_idx = 16 + param;
        
        switch (param) {
            case 0: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;
            case 1: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;
            case 2: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
            case 3: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
            case 4: controls[ctrl_idx] = _SituationNormalizeMidiCCDb(value, -96.0f, 0.0f); break;
            case 5: controls[ctrl_idx] = _SituationNormalizeMidiCCLog(value, 4000.0f, 20000.0f); break;
            case 6: controls[ctrl_idx] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;
            case 7: controls[ctrl_idx] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;
        }
    }
}

// ================================================================================================
// DYNAMICS CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Dynamics (compressor/limiter/gate/expander).
 * 
 * DEVICE: Dynamics (SITUATION_NODE_DYNAMICS)
 * CONTROLS: 7 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (mode: 0=comp, 1=limit, 2=gate, 3=expand)
 *   CC 17 → Control 1 (threshold_db: -60dB to 0dB)
 *   CC 18 → Control 2 (ratio: 1.0-20.0)
 *   CC 73 → Control 3 (attack_ms: 0.1-100ms, log) [Standard MIDI Attack]
 *   CC 72 → Control 4 (release_ms: 10-1000ms, log) [Standard MIDI Release]
 *   CC 19 → Control 5 (knee_db: 0-12dB)
 *   CC 7  → Control 6 (makeup_gain_db: 0-24dB) [Standard MIDI Volume]
 */
static void _SituationDynamicsOnControlChange(void* device_ptr, uint8_t controller,
                                               uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = floorf(_SituationNormalizeMidiCC(value, 0.0f, 3.99f)); break;  // mode (0-3)
        case 17: controls[1] = _SituationNormalizeMidiCCDb(value, -60.0f, 0.0f); break;       // threshold
        case 18: controls[2] = _SituationNormalizeMidiCC(value, 1.0f, 20.0f); break;          // ratio
        case 73: controls[3] = _SituationNormalizeMidiCCLog(value, 0.1f, 100.0f); break;      // attack
        case 72: controls[4] = _SituationNormalizeMidiCCLog(value, 10.0f, 1000.0f); break;    // release
        case 19: controls[5] = _SituationNormalizeMidiCC(value, 0.0f, 12.0f); break;          // knee
        case 7:  controls[6] = _SituationNormalizeMidiCC(value, 0.0f, 24.0f); break;          // makeup gain
    }
}

// ================================================================================================
// FILTER CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Filter (biquad filter).
 * 
 * DEVICE: Filter (SITUATION_NODE_FILTER)
 * CONTROLS: 5 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (type: 0=LP, 1=HP, 2=BP, 3=notch, 4=peak, 5=LS, 6=HS)
 *   CC 74 → Control 1 (cutoff: 20Hz-20000Hz, log) [Standard MIDI Brightness]
 *   CC 71 → Control 2 (resonance/Q: 0.1-10.0) [Standard MIDI Resonance]
 *   CC 7  → Control 3 (gain_db: -24dB to +24dB) [Standard MIDI Volume]
 *   CC 17 → Control 4 (drive: 1.0-10.0)
 *   CC 18 → Control 5 (oversampling: 0=off, 1=2x, 2=4x)
 */
static void _SituationFilterOnControlChange(void* device_ptr, uint8_t controller,
                                             uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = floorf(_SituationNormalizeMidiCC(value, 0.0f, 6.99f)); break;  // type (0-6)
        case 74: controls[1] = _SituationNormalizeMidiCCLog(value, 20.0f, 20000.0f); break;   // cutoff
        case 71: controls[2] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;          // Q
        case 7:  controls[3] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;      // gain
        case 17: controls[4] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;          // drive
        case 18: controls[5] = floorf(_SituationNormalizeMidiCC(value, 0.0f, 2.99f)); break;  // oversampling
    }
}

// ================================================================================================
// EQ 4-BAND CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for EQ 4-Band (parametric equalizer).
 * 
 * DEVICE: EQ 4-Band (SITUATION_NODE_EQ_4BAND)
 * CONTROLS: 12 total (4 bands × 3 params)
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 * 
 * Band 0 (Low):
 *   CC 16 → Control 0 (freq: 20Hz-500Hz, log)
 *   CC 17 → Control 1 (Q: 0.1-10.0)
 *   CC 18 → Control 2 (gain: -24dB to +24dB)
 * 
 * Band 1 (Low-Mid):
 *   CC 19 → Control 3 (freq: 200Hz-2000Hz, log)
 *   CC 20 → Control 4 (Q: 0.1-10.0)
 *   CC 21 → Control 5 (gain: -24dB to +24dB)
 * 
 * Band 2 (High-Mid):
 *   CC 22 → Control 6 (freq: 1000Hz-8000Hz, log)
 *   CC 23 → Control 7 (Q: 0.1-10.0)
 *   CC 24 → Control 8 (gain: -24dB to +24dB)
 * 
 * Band 3 (High):
 *   CC 25 → Control 9  (freq: 4000Hz-20000Hz, log)
 *   CC 26 → Control 10 (Q: 0.1-10.0)
 *   CC 27 → Control 11 (gain: -24dB to +24dB)
 */
static void _SituationEQ4BandOnControlChange(void* device_ptr, uint8_t controller,
                                              uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    // Band 0: CC 16-18 → Controls 0-2
    if (controller >= 16 && controller <= 18) {
        int param = controller - 16;
        switch (param) {
            case 0: controls[0] = _SituationNormalizeMidiCCLog(value, 20.0f, 500.0f); break;
            case 1: controls[1] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;
            case 2: controls[2] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;
        }
    }
    // Band 1: CC 19-21 → Controls 3-5
    else if (controller >= 19 && controller <= 21) {
        int param = controller - 19;
        switch (param) {
            case 0: controls[3] = _SituationNormalizeMidiCCLog(value, 200.0f, 2000.0f); break;
            case 1: controls[4] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;
            case 2: controls[5] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;
        }
    }
    // Band 2: CC 22-24 → Controls 6-8
    else if (controller >= 22 && controller <= 24) {
        int param = controller - 22;
        switch (param) {
            case 0: controls[6] = _SituationNormalizeMidiCCLog(value, 1000.0f, 8000.0f); break;
            case 1: controls[7] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;
            case 2: controls[8] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;
        }
    }
    // Band 3: CC 25-27 → Controls 9-11
    else if (controller >= 25 && controller <= 27) {
        int param = controller - 25;
        switch (param) {
            case 0: controls[9] = _SituationNormalizeMidiCCLog(value, 4000.0f, 20000.0f); break;
            case 1: controls[10] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;
            case 2: controls[11] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;
        }
    }
}


// ================================================================================================
// REVERB CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Reverb (Freeverb algorithm).
 * 
 * DEVICE: Reverb (SITUATION_NODE_REVERB)
 * CONTROLS: 5 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (room_size: 0.0-1.0)
 *   CC 17 → Control 1 (damp: 0.0-1.0)
 *   CC 18 → Control 2 (wet: 0.0-1.0)
 *   CC 19 → Control 3 (dry: 0.0-1.0)
 *   CC 20 → Control 4 (width: 0.0-1.0)
 */
static void _SituationReverbOnControlChange(void* device_ptr, uint8_t controller,
                                             uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;  // room_size
        case 17: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;  // damp
        case 18: controls[2] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;  // wet
        case 19: controls[3] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;  // dry
        case 20: controls[4] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;  // width
    }
}

// ================================================================================================
// CHORUS CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Chorus (4-stage chorus).
 * 
 * DEVICE: Chorus (SITUATION_NODE_CHORUS)
 * CONTROLS: 4 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 76 → Control 0 (rate: 0.1Hz-10Hz, log) [Standard MIDI Vibrato Rate]
 *   CC 77 → Control 1 (depth: 0.0-1.0) [Standard MIDI Vibrato Depth]
 *   CC 16 → Control 2 (feedback: 0.0-0.9)
 *   CC 17 → Control 3 (mix: 0.0-1.0)
 */
static void _SituationChorusOnControlChange(void* device_ptr, uint8_t controller,
                                             uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 76: controls[0] = _SituationNormalizeMidiCCLog(value, 0.1f, 10.0f); break;  // rate
        case 77: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;      // depth
        case 16: controls[2] = _SituationNormalizeMidiCC(value, 0.0f, 0.9f); break;      // feedback
        case 17: controls[3] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;      // mix
    }
}

// ================================================================================================
// OVERDRIVE CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Overdrive (multi-mode distortion).
 * 
 * DEVICE: Overdrive (SITUATION_NODE_OVERDRIVE)
 * CONTROLS: 4 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (mode: 0=soft, 1=hard, 2=fuzz, 3=tube)
 *   CC 17 → Control 1 (drive: 1.0-100.0, log)
 *   CC 18 → Control 2 (tone: 0.0-1.0)
 *   CC 7  → Control 3 (level: 0.0-2.0) [Standard MIDI Volume]
 */
static void _SituationOverdriveOnControlChange(void* device_ptr, uint8_t controller,
                                                uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = floorf(_SituationNormalizeMidiCC(value, 0.0f, 3.99f)); break;  // mode
        case 17: controls[1] = _SituationNormalizeMidiCCLog(value, 1.0f, 100.0f); break;      // drive
        case 18: controls[2] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // tone
        case 7:  controls[3] = _SituationNormalizeMidiCC(value, 0.0f, 2.0f); break;           // level
    }
}

// ================================================================================================
// PANNER CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Panner (stereo panner).
 * 
 * DEVICE: Panner (SITUATION_NODE_PANNER)
 * CONTROLS: 1 total
 * 
 * MIDI CC MAPPING:
 *   CC 10 → Control 0 (pan: -1.0 to +1.0) [Standard MIDI CC for pan]
 */
static void _SituationPannerOnControlChange(void* device_ptr, uint8_t controller,
                                             uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    if (controller == 10) {
        controls[0] = _SituationNormalizeMidiCC(value, -1.0f, 1.0f);  // pan
    }
}

// ================================================================================================
// LFO CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for LFO (low frequency oscillator).
 * 
 * DEVICE: LFO (SITUATION_NODE_LFO)
 * CONTROLS: 2 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (waveform: 0=sine, 1=tri, 2=saw, 3=square, 4=random)
 *   CC 17 → Control 1 (frequency: 0.01Hz-20Hz, log)
 */
static void _SituationLFOOnControlChange(void* device_ptr, uint8_t controller,
                                          uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = floorf(_SituationNormalizeMidiCC(value, 0.0f, 4.99f)); break;  // waveform
        case 17: controls[1] = _SituationNormalizeMidiCCLog(value, 0.01f, 20.0f); break;      // frequency
    }
}

// ================================================================================================
// ECHO CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Echo (delay effect).
 * 
 * DEVICE: Echo (SITUATION_NODE_ECHO)
 * CONTROLS: 4 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (delay_time: 10ms-2000ms, log)
 *   CC 17 → Control 1 (feedback: 0.0-0.95)
 *   CC 18 → Control 2 (wet: 0.0-1.0)
 *   CC 19 → Control 3 (dry: 0.0-1.0)
 */
static void _SituationEchoOnControlChange(void* device_ptr, uint8_t controller,
                                           uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = _SituationNormalizeMidiCCLog(value, 10.0f, 2000.0f); break;  // delay_time
        case 17: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 0.95f); break;        // feedback
        case 18: controls[2] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;         // wet
        case 19: controls[3] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;         // dry
    }
}

// ================================================================================================
// PHASER CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Phaser.
 * 
 * DEVICE: Phaser (SITUATION_NODE_PHASER)
 * CONTROLS: 5 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 76 → Control 0 (rate: 0.1Hz-10Hz, log) [Standard MIDI Vibrato Rate]
 *   CC 77 → Control 1 (depth: 0.0-1.0) [Standard MIDI Vibrato Depth]
 *   CC 16 → Control 2 (feedback: 0.0-0.95)
 *   CC 17 → Control 3 (stages: 2-12)
 *   CC 18 → Control 4 (mix: 0.0-1.0)
 */
static void _SituationPhaserOnControlChange(void* device_ptr, uint8_t controller,
                                             uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 76: controls[0] = _SituationNormalizeMidiCCLog(value, 0.1f, 10.0f); break;  // rate
        case 77: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;      // depth
        case 16: controls[2] = _SituationNormalizeMidiCC(value, 0.0f, 0.95f); break;     // feedback
        case 17: controls[3] = _SituationNormalizeMidiCC(value, 2.0f, 12.0f); break;     // stages
        case 18: controls[4] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;      // mix
    }
}

// ================================================================================================
// EXCITER CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Exciter (harmonic enhancer).
 * 
 * DEVICE: Exciter (SITUATION_NODE_EXCITER)
 * CONTROLS: 4 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (frequency: 1000Hz-10000Hz, log)
 *   CC 17 → Control 1 (harmonics: 0.0-1.0)
 *   CC 18 → Control 2 (blend: 0.0-1.0)
 *   CC 19 → Control 3 (drive: 1.0-10.0)
 */
static void _SituationExciterOnControlChange(void* device_ptr, uint8_t controller,
                                              uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = _SituationNormalizeMidiCCLog(value, 1000.0f, 10000.0f); break;  // frequency
        case 17: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;            // harmonics
        case 18: controls[2] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;            // blend
        case 19: controls[3] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;           // drive
    }
}

// ================================================================================================
// STUDIO REVERB CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Studio Reverb (professional algorithmic reverb).
 * 
 * DEVICE: Studio Reverb (SITUATION_NODE_STUDIO_REVERB)
 * CONTROLS: 8 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (pre_delay: 0ms-100ms)
 *   CC 17 → Control 1 (room_size: 0.0-1.0)
 *   CC 18 → Control 2 (damping: 0.0-1.0)
 *   CC 19 → Control 3 (diffusion: 0.0-1.0)
 *   CC 20 → Control 4 (decay_time: 0.1s-10s, log)
 *   CC 21 → Control 5 (early_reflections: 0.0-1.0)
 *   CC 22 → Control 6 (wet: 0.0-1.0)
 *   CC 23 → Control 7 (dry: 0.0-1.0)
 */
static void _SituationStudioReverbOnControlChange(void* device_ptr, uint8_t controller,
                                                   uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = _SituationNormalizeMidiCC(value, 0.0f, 100.0f); break;         // pre_delay
        case 17: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // room_size
        case 18: controls[2] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // damping
        case 19: controls[3] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // diffusion
        case 20: controls[4] = _SituationNormalizeMidiCCLog(value, 0.1f, 10.0f); break;       // decay_time
        case 21: controls[5] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // early_reflections
        case 22: controls[6] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // wet
        case 23: controls[7] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // dry
    }
}

// ================================================================================================
// SPRING REVERB CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Spring Reverb (physical modeling).
 * 
 * DEVICE: Spring Reverb (SITUATION_NODE_SPRING_REVERB)
 * CONTROLS: 6 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0 (spring_tension: 0.0-1.0)
 *   CC 17 → Control 1 (spring_damping: 0.0-1.0)
 *   CC 18 → Control 2 (spring_length: 0.0-1.0)
 *   CC 19 → Control 3 (drive: 1.0-10.0)
 *   CC 20 → Control 4 (wet: 0.0-1.0)
 *   CC 21 → Control 5 (dry: 0.0-1.0)
 */
static void _SituationSpringReverbOnControlChange(void* device_ptr, uint8_t controller,
                                                   uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;   // spring_tension
        case 17: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;   // spring_damping
        case 18: controls[2] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;   // spring_length
        case 19: controls[3] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;  // drive
        case 20: controls[4] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;   // wet
        case 21: controls[5] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;   // dry
    }
}

// ================================================================================================
// SST-282 CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for SST-282 (hardware emulation).
 * 
 * DEVICE: SST-282 (SITUATION_NODE_SST282)
 * CONTROLS: 13 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0  (input_gain: 0.0-2.0)
 *   CC 17 → Control 1  (lf_cut_db: -12dB to +12dB)
 *   CC 18 → Control 2  (hf_cut_db: -12dB to +12dB)
 *   CC 19 → Control 3  (tap_level_0: 0.0-1.0)
 *   CC 20 → Control 4  (tap_level_1: 0.0-1.0)
 *   CC 21 → Control 5  (tap_level_2: 0.0-1.0)
 *   CC 22 → Control 6  (tap_level_3: 0.0-1.0)
 *   CC 23 → Control 7  (feedback: 0.0-0.95)
 *   CC 24 → Control 8  (dry_db: -60dB to 0dB)
 *   CC 25 → Control 9  (echo_db: -60dB to 0dB)
 *   CC 26 → Control 10 (direct_db: -60dB to 0dB)
 *   CC 27 → Control 11 (mode: 0=normal, 1=reverse)
 *   CC 28 → Control 12 (echo_delay: 0ms-500ms)
 */
static void _SituationSST282OnControlChange(void* device_ptr, uint8_t controller,
                                             uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = _SituationNormalizeMidiCC(value, 0.0f, 2.0f); break;           // input_gain
        case 17: controls[1] = _SituationNormalizeMidiCCDb(value, -12.0f, 12.0f); break;      // lf_cut_db
        case 18: controls[2] = _SituationNormalizeMidiCCDb(value, -12.0f, 12.0f); break;      // hf_cut_db
        case 19: controls[3] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // tap_level_0
        case 20: controls[4] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // tap_level_1
        case 21: controls[5] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // tap_level_2
        case 22: controls[6] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;           // tap_level_3
        case 23: controls[7] = _SituationNormalizeMidiCC(value, 0.0f, 0.95f); break;          // feedback
        case 24: controls[8] = _SituationNormalizeMidiCCDb(value, -60.0f, 0.0f); break;       // dry_db
        case 25: controls[9] = _SituationNormalizeMidiCCDb(value, -60.0f, 0.0f); break;       // echo_db
        case 26: controls[10] = _SituationNormalizeMidiCCDb(value, -60.0f, 0.0f); break;      // direct_db
        case 27: controls[11] = floorf(_SituationNormalizeMidiCC(value, 0.0f, 1.99f)); break; // mode
        case 28: controls[12] = _SituationNormalizeMidiCC(value, 0.0f, 500.0f); break;        // echo_delay
    }
}

// ================================================================================================
// MASTERING AMP CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Mastering Amp (SSE-optimized console processor).
 * 
 * DEVICE: Mastering Amp (SITUATION_NODE_MASTERING_AMP)
 * CONTROLS: 15 total
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 *   CC 16 → Control 0  (amp_type: 0=clean, 1=warm, 2=vintage, 3=modern)
 *   CC 17 → Control 1  (drive: 0.0-10.0)
 *   CC 18 → Control 2  (low_freq: 20Hz-500Hz, log)
 *   CC 19 → Control 3  (low_gain: -24dB to +24dB)
 *   CC 20 → Control 4  (mid_freq: 200Hz-5kHz, log)
 *   CC 21 → Control 5  (mid_gain: -24dB to +24dB)
 *   CC 22 → Control 6  (mid_q: 0.1-10.0)
 *   CC 23 → Control 7  (high_freq: 2kHz-20kHz, log)
 *   CC 24 → Control 8  (high_gain: -24dB to +24dB)
 *   CC 25 → Control 9  (air_freq: 8kHz-20kHz, log)
 *   CC 26 → Control 10 (air_gain: -12dB to +12dB)
 *   CC 27 → Control 11 (tight_cutoff: 20Hz-200Hz, log)
 *   CC 28 → Control 12 (aspect_ratio: 0.0-1.0)
 *   CC 64 → Control 13 (vintage: 0=off, 1=on) [Standard MIDI Sustain]
 *   CC 65 → Control 14 (circuit_bending: 0=off, 1=on)
 */
static void _SituationMasteringAmpOnControlChange(void* device_ptr, uint8_t controller,
                                                   uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    switch (controller) {
        case 16: controls[0] = floorf(_SituationNormalizeMidiCC(value, 0.0f, 3.99f)); break;  // amp_type
        case 17: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 10.0f); break;          // drive
        case 18: controls[2] = _SituationNormalizeMidiCCLog(value, 20.0f, 500.0f); break;     // low_freq
        case 19: controls[3] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;      // low_gain
        case 20: controls[4] = _SituationNormalizeMidiCCLog(value, 200.0f, 5000.0f); break;   // mid_freq
        case 21: controls[5] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;      // mid_gain
        case 22: controls[6] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;          // mid_q
        case 23: controls[7] = _SituationNormalizeMidiCCLog(value, 2000.0f, 20000.0f); break; // high_freq
        case 24: controls[8] = _SituationNormalizeMidiCCDb(value, -24.0f, 24.0f); break;      // high_gain
        case 25: controls[9] = _SituationNormalizeMidiCCLog(value, 8000.0f, 20000.0f); break; // air_freq
        case 26: controls[10] = _SituationNormalizeMidiCCDb(value, -12.0f, 12.0f); break;     // air_gain
        case 27: controls[11] = _SituationNormalizeMidiCCLog(value, 20.0f, 200.0f); break;    // tight_cutoff
        case 28: controls[12] = _SituationNormalizeMidiCC(value, 0.0f, 1.0f); break;          // aspect_ratio
        case 64: controls[13] = (value > 63) ? 1.0f : 0.0f; break;                            // vintage
        case 65: controls[14] = (value > 63) ? 1.0f : 0.0f; break;                            // circuit_bending
    }
}

// ================================================================================================
// MAXIMIZER CALLBACKS
// ================================================================================================

/**
 * @brief MIDI CC mapping for Maximizer (FFTW3-based spectral enhancer).
 * 
 * DEVICE: Maximizer (SITUATION_NODE_MAXIMIZER)
 * CONTROLS: 18 total (4 bands × 4 params + 2 filters)
 * 
 * MIDI CC MAPPING (STANDARDIZED v2.5.0):
 * 
 * Band 0 (Low):
 *   CC 16 → Control 0  (freq: 20Hz-500Hz, log)
 *   CC 17 → Control 1  (threshold: 0.0-2.0)
 *   CC 18 → Control 2  (ratio: 1.0-10.0)
 *   CC 19 → Control 3  (attack: 1-10)
 * 
 * Band 1 (Low-Mid):
 *   CC 20 → Control 4  (freq: 200Hz-2kHz, log)
 *   CC 21 → Control 5  (threshold: 0.0-2.0)
 *   CC 22 → Control 6  (ratio: 1.0-10.0)
 *   CC 23 → Control 7  (attack: 1-10)
 * 
 * Band 2 (High-Mid):
 *   CC 24 → Control 8  (freq: 1kHz-8kHz, log)
 *   CC 25 → Control 9  (threshold: 0.0-2.0)
 *   CC 26 → Control 10 (ratio: 1.0-10.0)
 *   CC 27 → Control 11 (attack: 1-10)
 * 
 * Band 3 (High):
 *   CC 28 → Control 12 (freq: 4kHz-20kHz, log)
 *   CC 29 → Control 13 (threshold: 0.0-2.0)
 *   CC 30 → Control 14 (ratio: 1.0-10.0)
 *   CC 31 → Control 15 (attack: 1-10)
 * 
 * Filters:
 *   CC 80 → Control 16 (hpf_cutoff: 20Hz-500Hz, log)
 *   CC 81 → Control 17 (lpf_cutoff: 5kHz-20kHz, log)
 */
static void _SituationMaximizerOnControlChange(void* device_ptr, uint8_t controller,
                                                uint8_t value, uint32_t sample_offset) {
    (void)sample_offset;
    
    float* controls = (float*)device_ptr;
    if (!controls) return;
    
    // Band 0 (CC 16-19)
    if (controller >= 16 && controller <= 19) {
        int param = controller - 16;
        switch (param) {
            case 0: controls[0] = _SituationNormalizeMidiCCLog(value, 20.0f, 500.0f); break;
            case 1: controls[1] = _SituationNormalizeMidiCC(value, 0.0f, 2.0f); break;
            case 2: controls[2] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
            case 3: controls[3] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
        }
    }
    // Band 1 (CC 20-23)
    else if (controller >= 20 && controller <= 23) {
        int param = controller - 20;
        switch (param) {
            case 0: controls[4] = _SituationNormalizeMidiCCLog(value, 200.0f, 2000.0f); break;
            case 1: controls[5] = _SituationNormalizeMidiCC(value, 0.0f, 2.0f); break;
            case 2: controls[6] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
            case 3: controls[7] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
        }
    }
    // Band 2 (CC 24-27)
    else if (controller >= 24 && controller <= 27) {
        int param = controller - 24;
        switch (param) {
            case 0: controls[8] = _SituationNormalizeMidiCCLog(value, 1000.0f, 8000.0f); break;
            case 1: controls[9] = _SituationNormalizeMidiCC(value, 0.0f, 2.0f); break;
            case 2: controls[10] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
            case 3: controls[11] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
        }
    }
    // Band 3 (CC 28-31)
    else if (controller >= 28 && controller <= 31) {
        int param = controller - 28;
        switch (param) {
            case 0: controls[12] = _SituationNormalizeMidiCCLog(value, 4000.0f, 20000.0f); break;
            case 1: controls[13] = _SituationNormalizeMidiCC(value, 0.0f, 2.0f); break;
            case 2: controls[14] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
            case 3: controls[15] = _SituationNormalizeMidiCC(value, 1.0f, 10.0f); break;
        }
    }
    // Filters (CC 80-81)
    else if (controller == 80) {
        controls[16] = _SituationNormalizeMidiCCLog(value, 20.0f, 500.0f);  // hpf_cutoff
    }
    else if (controller == 81) {
        controls[17] = _SituationNormalizeMidiCCLog(value, 5000.0f, 20000.0f);  // lpf_cutoff
    }
}

// ================================================================================================
// TONE SYNTH CALLBACKS (graph node — SituationToneSynthMidiCtx in device_ptr)
// ================================================================================================

static SituationToneSynthMidiCtx* _SituationToneSynthMidiCtx(void* device_ptr) {
    return (SituationToneSynthMidiCtx*)device_ptr;
}

/**
 * @brief MIDI mapping for graph tone synth (16-voice poly per node, ADSR, pan, waveforms).
 *
 * Note on/off (poly), pitch bend, CC1 vibrato, CC7/CC11 volume, CC10 pan,
 * CC64 sustain, CC70 waveform, CC72–77 ADSR, CC92 tremolo, CC106 pulse width, CC107–110 sub-osc,
 * CC24–31 mod LFO (rate/wave + pitch/PWM/filter depth/range),
 * CC5 portamento time, CC20 portamento speed, CC126 mono / CC127 poly, CC123 all-notes-off, program → waveform.
 */
static void _SituationToneSynthOnNoteOn(void* device_ptr, uint8_t note, uint8_t velocity,
                                        uint32_t sample_offset) {
    (void)sample_offset;
    SituationToneSynthMidiCtx* ctx = _SituationToneSynthMidiCtx(device_ptr);
    if (!ctx || !ctx->controls || !ctx->synth || note > 127) return;

    int sr = SituationGetAudioPlaybackSampleRate();
    if (sr <= 0) sr = 48000;

    _SituationToneSynthTriggerNoteOn(ctx, note, velocity, sr);
    _SituationToneSynthMidiSyncControls(ctx);
}

static void _SituationToneSynthOnNoteOff(void* device_ptr, uint8_t note, uint8_t velocity,
                                         uint32_t sample_offset) {
    (void)velocity;
    (void)sample_offset;
    SituationToneSynthMidiCtx* ctx = _SituationToneSynthMidiCtx(device_ptr);
    if (!ctx || !ctx->synth) return;

    _SituationToneSynthReleaseNote(ctx->synth, note);
    _SituationToneSynthMidiSyncControls(ctx);
}

static void _SituationToneSynthOnControlChange(void* device_ptr, uint8_t controller, uint8_t value,
                                               uint32_t sample_offset) {
    (void)sample_offset;
    SituationToneSynthMidiCtx* ctx = _SituationToneSynthMidiCtx(device_ptr);
    if (!ctx || !ctx->controls || !ctx->synth) return;

    int sr = SituationGetAudioPlaybackSampleRate();
    if (sr <= 0) sr = 48000;

    _SituationToneSynthApplyControlChange(ctx, controller, value, sr);
    _SituationToneSynthMidiSyncControls(ctx);
}

void _SituationToneSynthOnPitchBend(SituationToneSynthMidiCtx* ctx, int16_t bend) {
    if (!ctx || !ctx->synth) return;
    ctx->synth->bend_semitones = (((float)bend / 8192.0f) - 1.0f) * 2.0f;
    _SituationToneSynthMidiSyncControls(ctx);
}

void _SituationToneSynthOnProgramChange(SituationToneSynthMidiCtx* ctx, uint8_t program) {
    if (!ctx || !ctx->controls || !ctx->synth) return;
    _SituationToneSynthSetWaveform(ctx, (int)(program % 5));
    _SituationToneSynthMidiSyncControls(ctx);
}

// ================================================================================================
// CALLBACK LOOKUP TABLE
// ================================================================================================

/**
 * @brief Callback lookup entry.
 */
typedef struct {
    SituationNodeType device_type;
    void (*on_control_change)(void*, uint8_t, uint8_t, uint32_t);  // Fixed: uint32_t sample_offset
    const char* device_name;
    void (*on_note_on)(void*, uint8_t, uint8_t, uint32_t);
    void (*on_note_off)(void*, uint8_t, uint8_t, uint32_t);
} SIT_MidiCallbackEntry;

/**
 * @brief Global callback lookup table.
 * @details Maps device types to their MIDI callback functions.
 * 
 * USAGE:
 *   const SIT_MidiCallbackEntry* entry = SIT_GetMidiCallbackForDevice(SITUATION_NODE_COMPANDER);
 *   if (entry) {
 *       callbacks.on_control_change = entry->on_control_change;
 *   }
 */
static const SIT_MidiCallbackEntry g_midi_callback_table[] = {
    { SITUATION_NODE_COMPANDER,      _SituationCompanderOnControlChange,      "Compander" },
    { SITUATION_NODE_DYNAMICS,       _SituationDynamicsOnControlChange,       "Dynamics" },
    { SITUATION_NODE_FILTER,         _SituationFilterOnControlChange,         "Filter" },
    { SITUATION_NODE_EQ_4BAND,       _SituationEQ4BandOnControlChange,        "EQ 4-Band" },
    { SITUATION_NODE_REVERB,         _SituationReverbOnControlChange,         "Reverb" },
    { SITUATION_NODE_CHORUS,         _SituationChorusOnControlChange,         "Chorus" },
    { SITUATION_NODE_OVERDRIVE,      _SituationOverdriveOnControlChange,      "Overdrive" },
    { SITUATION_NODE_PANNER,         _SituationPannerOnControlChange,         "Panner" },
    { SITUATION_NODE_LFO,            _SituationLFOOnControlChange,            "LFO" },
    { SITUATION_NODE_ECHO,           _SituationEchoOnControlChange,           "Echo" },
    { SITUATION_NODE_PHASER,         _SituationPhaserOnControlChange,         "Phaser" },
    { SITUATION_NODE_EXCITER,        _SituationExciterOnControlChange,        "Exciter" },
    { SITUATION_NODE_STUDIO_REVERB,  _SituationStudioReverbOnControlChange,   "Studio Reverb" },
    { SITUATION_NODE_SPRING_REVERB,  _SituationSpringReverbOnControlChange,   "Spring Reverb" },
    { SITUATION_NODE_SST282,         _SituationSST282OnControlChange,         "SST-282" },
    { SITUATION_NODE_MASTERING_AMP,  _SituationMasteringAmpOnControlChange,   "Mastering Amp" },
    { SITUATION_NODE_MAXIMIZER,      _SituationMaximizerOnControlChange,      "Maximizer" },
    { SITUATION_NODE_TONE_SYNTH,     _SituationToneSynthOnControlChange,      "Tone Synth",
      _SituationToneSynthOnNoteOn,   _SituationToneSynthOnNoteOff },
};

static const int g_midi_callback_table_count = sizeof(g_midi_callback_table) / sizeof(g_midi_callback_table[0]);

/**
 * @brief Get MIDI callback for device type.
 * @param device_type Device type to look up.
 * @return Callback entry or NULL if not found.
 */
static inline const SIT_MidiCallbackEntry* SIT_GetMidiCallbackForDevice(SituationNodeType device_type) {
    for (int i = 0; i < g_midi_callback_table_count; i++) {
        if (g_midi_callback_table[i].device_type == device_type) {
            return &g_midi_callback_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Check if device supports MIDI control.
 * @param device_type Device type to check.
 * @return true if device has MIDI callbacks, false otherwise.
 */
static inline bool SIT_DeviceSupportsMidi(SituationNodeType device_type) {
    return SIT_GetMidiCallbackForDevice(device_type) != NULL;
}

/**
 * @brief Get device identity for a device type.
 * @param device_type Device type.
 * @return Device identity structure.
 */
static inline SIT_MidiDeviceIdentity SIT_GetDeviceIdentity(SituationNodeType device_type) {
    switch (device_type) {
        case SITUATION_NODE_COMPANDER:      return _SituationCreateDeviceIdentity(SIT_MODEL_COMPANDER, "Compander");
        case SITUATION_NODE_DYNAMICS:       return _SituationCreateDeviceIdentity(SIT_MODEL_DYNAMICS, "Dynamics");
        case SITUATION_NODE_FILTER:         return _SituationCreateDeviceIdentity(SIT_MODEL_FILTER, "Filter");
        case SITUATION_NODE_EQ_4BAND:       return _SituationCreateDeviceIdentity(SIT_MODEL_EQ_4BAND, "EQ 4-Band");
        case SITUATION_NODE_REVERB:         return _SituationCreateDeviceIdentity(SIT_MODEL_REVERB, "Reverb");
        case SITUATION_NODE_CHORUS:         return _SituationCreateDeviceIdentity(SIT_MODEL_CHORUS, "Chorus");
        case SITUATION_NODE_OVERDRIVE:      return _SituationCreateDeviceIdentity(SIT_MODEL_OVERDRIVE, "Overdrive");
        case SITUATION_NODE_PANNER:         return _SituationCreateDeviceIdentity(SIT_MODEL_PANNER, "Panner");
        case SITUATION_NODE_LFO:            return _SituationCreateDeviceIdentity(SIT_MODEL_LFO, "LFO");
        case SITUATION_NODE_ECHO:           return _SituationCreateDeviceIdentity(SIT_MODEL_ECHO, "Echo");
        case SITUATION_NODE_PHASER:         return _SituationCreateDeviceIdentity(SIT_MODEL_PHASER, "Phaser");
        case SITUATION_NODE_EXCITER:        return _SituationCreateDeviceIdentity(SIT_MODEL_EXCITER, "Exciter");
        case SITUATION_NODE_STUDIO_REVERB:  return _SituationCreateDeviceIdentity(SIT_MODEL_STUDIO_REVERB, "Studio Reverb");
        case SITUATION_NODE_SPRING_REVERB:  return _SituationCreateDeviceIdentity(SIT_MODEL_SPRING_REVERB, "Spring Reverb");
        case SITUATION_NODE_SST282:         return _SituationCreateDeviceIdentity(SIT_MODEL_SST282, "SST-282");
        case SITUATION_NODE_MASTERING_AMP:  return _SituationCreateDeviceIdentity(SIT_MODEL_MASTERING_AMP, "Mastering Amp");
        case SITUATION_NODE_MAXIMIZER:      return _SituationCreateDeviceIdentity(SIT_MODEL_MAXIMIZER, "Maximizer");
        case SITUATION_NODE_TONE_SYNTH:     return _SituationCreateDeviceIdentity(SIT_MODEL_TONE_SYNTH, "Tone Synth");
        default: {
            // Unknown device - return generic identity
            SIT_MidiDeviceIdentity identity = _SituationCreateDeviceIdentity(0xFF, "Unknown");
            return identity;
        }
    }
}

// ================================================================================================
// USAGE EXAMPLE
// ================================================================================================

/**
 * @example
 * 
 * // In node graph creation code:
 * SituationNode* compander_node = SituationCreateNode(graph, SITUATION_NODE_COMPANDER);
 * 
 * // Setup MIDI control for the node
 * const SIT_MidiCallbackEntry* entry = SIT_GetMidiCallbackForDevice(SITUATION_NODE_COMPANDER);
 * if (entry) {
 *     SIT_MidiDevice* midi = SIT_MidiDevice_Create(entry->device_name, 
 *                                                   SIT_MIDI_DEVICE_EFFECT,
 *                                                   SIT_MIDI_CAP_INPUT,
 *                                                   compander_node);
 *     
 *     SIT_MidiCallbacks callbacks = {0};
 *     callbacks.on_control_change = entry->on_control_change;
 *     callbacks.device_ptr = compander_node->controls;  // Pass control array
 *     
 *     SIT_MidiDevice_SetCallbacks(midi, &callbacks);
 *     
 *     // Store midi device in node for cleanup
 *     compander_node->midi_device = midi;
 * }
 * 
 * // Now MIDI CC messages will automatically update the control array
 * // which is read by _SituationProcessCompanderNode() in device_wrappers.h
 */

// ================================================================================================
// 14-BIT MIDI CC USAGE EXAMPLE
// ================================================================================================

/**
 * @example 14-bit MIDI CC for high-resolution control
 * 
 * // Example: High-resolution filter cutoff control
 * // Uses CC 1 (MSB) and CC 33 (LSB) for 14-bit resolution
 * 
 * typedef struct {
 *     float* controls;
 *     SIT_MidiCC14Bit cutoff_14bit;  // Track MSB/LSB pair
 * } FilterMidiState;
 * 
 * static void FilterOnControlChange_14Bit(void* device_ptr, uint8_t controller,
 *                                          uint8_t value, uint32_t sample_offset) {
 *     (void)sample_offset;
 *     FilterMidiState* state = (FilterMidiState*)device_ptr;
 *     
 *     // Handle 14-bit cutoff (CC 1 MSB + CC 33 LSB)
 *     if (controller == 1 || controller == 33) {
 *         if (_SituationUpdate14BitCC(&state->cutoff_14bit, controller, value)) {
 *             // Get 14-bit value (0-16383)
 *             uint16_t value_14bit = _SituationGet14BitValue(&state->cutoff_14bit);
 *             
 *             // Normalize to frequency range with 14-bit precision
 *             state->controls[1] = _SituationNormalize14BitCCLog(value_14bit, 20.0f, 20000.0f);
 *             
 *             // 14-bit gives 16384 steps vs 128 steps (7-bit)
 *             // At 20Hz-20kHz: ~0.5Hz resolution vs ~157Hz resolution
 *         }
 *     }
 *     
 *     // Handle regular 7-bit CCs
 *     switch (controller) {
 *         case 40: state->controls[0] = floorf(_SituationNormalizeMidiCC(value, 0.0f, 6.99f)); break;  // type
 *         case 42: state->controls[2] = _SituationNormalizeMidiCC(value, 0.1f, 10.0f); break;          // Q
 *         // ... etc
 *     }
 * }
 * 
 * // MIDI CC MAPPING (14-bit):
 * //   CC 1 (MSB) + CC 33 (LSB) → Cutoff (20Hz-20kHz, 14-bit, ~0.5Hz resolution)
 * //   CC 40 → Type (7-bit)
 * //   CC 42 → Q (7-bit)
 * 
 * // BENEFITS:
 * // - Smooth parameter sweeps (no zipper noise)
 * // - Precise control for critical parameters
 * // - Standard MIDI 14-bit protocol
 * // - Backward compatible (MSB works alone as 7-bit)
 */

#endif // SITUATION_MIDI_DEVICE_CALLBACKS_H
