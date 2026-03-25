/*
 * midi_learn.h - MIDI Learn System for Situation
 * 
 * Allows dynamic assignment of MIDI CC messages to device parameters at runtime.
 * Users can click "Learn" and move a controller to assign that CC to a parameter.
 * 
 * Features:
 * - Auto-select first available MIDI input device
 * - 7-bit and 14-bit CC learning
 * - Per-device mapping tables
 * - Save/load mappings to JSON presets
 * - Thread-safe learning (can learn while audio is running)
 * - Conflict detection
 * 
 * Usage:
 *   1. Create learn state: SIT_MidiLearn_Create()
 *   2. Auto-select device: SIT_MidiLearn_AutoSelectInput()
 *   3. Start learning: SIT_MidiLearn_Start(state, control_index, ...)
 *   4. Process CC: SIT_MidiLearn_ProcessCC() in your callback
 *   5. Update timeout: SIT_MidiLearn_Update() periodically
 *   6. Save preset: SIT_MidiLearn_SavePreset()
 */

#ifndef MIDI_LEARN_H
#define MIDI_LEARN_H

#include "midi.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ================================================================================================
// DATA STRUCTURES
// ================================================================================================

/**
 * @brief Scaling types for parameter mapping
 */
typedef enum {
    SIT_MIDI_SCALING_LINEAR,    // Linear 0-127 → min-max
    SIT_MIDI_SCALING_LOG,       // Logarithmic (for frequency)
    SIT_MIDI_SCALING_DB,        // Decibel scaling (for gain)
    SIT_MIDI_SCALING_DISCRETE   // Discrete steps (for switches)
} SIT_MidiScaling;

/**
 * @brief MIDI Learn mapping entry
 * 
 * Stores the relationship between a MIDI CC and a device parameter.
 */
typedef struct {
    uint8_t cc_number;           // CC number (0-127)
    uint8_t cc_lsb;              // LSB for 14-bit (32-63), or 0xFF if 7-bit
    uint8_t channel;             // MIDI channel (0-15), or 0xFF for omni
    int control_index;           // Which control[] this maps to
    float min_value;             // Minimum parameter value
    float max_value;             // Maximum parameter value
    SIT_MidiScaling scaling;     // Linear, Log, or dB
    char param_name[64];         // Human-readable parameter name
} SIT_MidiLearnMapping;

/**
 * @brief MIDI device information for device selection
 */
typedef struct {
    PmDeviceID device_id;        // Device ID for use with SIT_MidiLearn_SetInputDevice
    char device_name[128];       // Human-readable device name
    int is_input;                // 1 if input device, 0 otherwise
    int is_output;               // 1 if output device, 0 otherwise
} SIT_MidiLearnDeviceInfo;

/**
 * @brief MIDI Learn state
 * 
 * Manages the learning process and stores learned mappings.
 */
typedef struct SIT_MidiLearnState {
    // Mappings
    SIT_MidiLearnMapping mappings[128];  // Up to 128 mappings per device
    int mapping_count;
    
    // Learn mode state
    int learning;                        // 1 if in learn mode
    int learn_control_index;             // Which control we're learning
    double learn_start_time;             // Timestamp when learn started
    double learn_timeout;                // Timeout in seconds (default: 5.0)
    char learn_param_name[64];           // Parameter name being learned
    float learn_min_value;               // Min value for parameter
    float learn_max_value;               // Max value for parameter
    SIT_MidiScaling learn_scaling;       // Scaling type for parameter
    uint8_t learn_channel_filter;        // Channel filter (0-15, or 0xFF for omni)
    
    // 14-bit CC detection
    uint8_t learn_14bit_msb;             // Detected MSB CC number (0xFF if none)
    double learn_14bit_msb_time;         // Time when MSB was received
    
    // MIDI device selection
    PmDeviceID input_device;             // Which MIDI device to learn from (PM_NO_DEVICE = any)
    PmStream *input_stream;              // Open stream for learning
    
    // Conflict detection
    int (*on_conflict)(void *user_data, int cc_number, int existing_control);
    void *conflict_user_data;
    
    // Callbacks
    void (*on_learn_complete)(void *user_data, const SIT_MidiLearnMapping *mapping);
    void (*on_learn_timeout)(void *user_data, int control_index);
    void *callback_user_data;
} SIT_MidiLearnState;

// ================================================================================================
// API FUNCTIONS
// ================================================================================================

/**
 * @brief Create a MIDI Learn state
 * 
 * @return Pointer to new learn state, or NULL on failure
 */
SIT_MidiLearnState* SIT_MidiLearn_Create(void);

/**
 * @brief Destroy a MIDI Learn state
 * 
 * @param state Learn state to destroy
 */
void SIT_MidiLearn_Destroy(SIT_MidiLearnState *state);

/**
 * @brief Auto-select the first available MIDI input device
 * 
 * @return Device ID of first input, or PM_NO_DEVICE if none found
 */
PmDeviceID SIT_MidiLearn_AutoSelectInput(void);

/**
 * @brief List all available MIDI input devices
 * 
 * @param devices Array to fill with device info
 * @param max_count Maximum number of devices to return
 * @return Number of devices found
 */
int SIT_MidiLearn_ListInputDevices(SIT_MidiLearnDeviceInfo* devices, int max_count);

/**
 * @brief Set the MIDI input device for learning
 * 
 * @param state Learn state
 * @param device_id Device ID to use (from SIT_MidiLearn_AutoSelectInput or SIT_MidiLearn_ListInputDevices)
 * @return 1 on success, 0 on failure
 */
int SIT_MidiLearn_SetInputDevice(SIT_MidiLearnState *state, PmDeviceID device_id);

/**
 * @brief Start learning a parameter
 * 
 * @param state Learn state
 * @param control_index Which control[] index to learn
 * @param param_name Human-readable parameter name
 * @param min_value Minimum parameter value
 * @param max_value Maximum parameter value
 * @param scaling Scaling type (linear, log, dB)
 */
void SIT_MidiLearn_Start(SIT_MidiLearnState *state, 
                          int control_index,
                          const char *param_name,
                          float min_value,
                          float max_value,
                          SIT_MidiScaling scaling);

/**
 * @brief Set MIDI channel filter for learning
 * 
 * @param state Learn state
 * @param channel MIDI channel (0-15), or 0xFF for omni (all channels)
 * 
 * @note Call this before SIT_MidiLearn_Start() to filter by channel during learning
 */
void SIT_MidiLearn_SetChannelFilter(SIT_MidiLearnState *state, uint8_t channel);

/**
 * @brief Cancel learning
 * 
 * @param state Learn state
 */
void SIT_MidiLearn_Cancel(SIT_MidiLearnState *state);

/**
 * @brief Process incoming MIDI CC (call from on_control_change callback)
 * 
 * @param state Learn state
 * @param channel MIDI channel (0-15, or 0xFF for omni)
 * @param cc_number CC number (0-127)
 * @param cc_value CC value (0-127)
 * @param output_value Pointer to receive normalized output value
 * @return 1 if learned (capture this CC), 0 if normal processing
 */
int SIT_MidiLearn_ProcessCC(SIT_MidiLearnState *state,
                             uint8_t channel,
                             uint8_t cc_number,
                             uint8_t cc_value,
                             float *output_value);

/**
 * @brief Check for timeout (call periodically)
 * 
 * @param state Learn state
 * @param current_time Current time in seconds
 */
void SIT_MidiLearn_Update(SIT_MidiLearnState *state, double current_time);

/**
 * @brief Clear a specific mapping
 * 
 * @param state Learn state
 * @param control_index Control index to clear
 */
void SIT_MidiLearn_ClearMapping(SIT_MidiLearnState *state, int control_index);

/**
 * @brief Clear all mappings
 * 
 * @param state Learn state
 */
void SIT_MidiLearn_ClearAll(SIT_MidiLearnState *state);

/**
 * @brief Get a mapping by control index
 * 
 * @param state Learn state
 * @param control_index Control index to query
 * @return Pointer to mapping, or NULL if not found
 */
const SIT_MidiLearnMapping* SIT_MidiLearn_GetMapping(SIT_MidiLearnState *state, 
                                                       int control_index);

/**
 * @brief Save mappings to JSON preset file
 * 
 * @param state Learn state
 * @param filename Path to JSON file
 * @return 1 on success, 0 on failure
 */
int SIT_MidiLearn_SavePreset(SIT_MidiLearnState *state, const char *filename);

/**
 * @brief Load mappings from JSON preset file
 * 
 * @param state Learn state
 * @param filename Path to JSON file
 * @return 1 on success, 0 on failure
 */
int SIT_MidiLearn_LoadPreset(SIT_MidiLearnState *state, const char *filename);

/**
 * @brief Set conflict callback
 * 
 * @param state Learn state
 * @param callback Callback function (return 1 to overwrite, 0 to cancel)
 * @param user_data User data to pass to callback
 */
void SIT_MidiLearn_SetConflictCallback(SIT_MidiLearnState *state,
                                        int (*callback)(void*, int, int),
                                        void *user_data);

/**
 * @brief Set learn complete callback
 * 
 * @param state Learn state
 * @param callback Callback function
 * @param user_data User data to pass to callback
 */
void SIT_MidiLearn_SetLearnCompleteCallback(SIT_MidiLearnState *state,
                                             void (*callback)(void*, const SIT_MidiLearnMapping*),
                                             void *user_data);

// ================================================================================================
// IMPLEMENTATION
// ================================================================================================

#ifdef MIDI_LEARN_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>

// ================================================================================================
// JSON SERIALIZATION HELPERS (Following node_graph_serialization pattern)
// ================================================================================================
static double _SIT_MidiLearn_GetTime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

// Helper: Normalize MIDI CC value (linear)
static float _SIT_MidiLearn_NormalizeLinear(uint8_t cc_value, float min_val, float max_val) {
    float normalized = cc_value / 127.0f;
    return min_val + normalized * (max_val - min_val);
}

// Helper: Normalize MIDI CC value (logarithmic)
static float _SIT_MidiLearn_NormalizeLog(uint8_t cc_value, float min_val, float max_val) {
    float normalized = cc_value / 127.0f;
    float log_min = logf(min_val);
    float log_max = logf(max_val);
    return expf(log_min + normalized * (log_max - log_min));
}

// Helper: Normalize MIDI CC value (dB)
static float _SIT_MidiLearn_NormalizeDb(uint8_t cc_value, float min_db, float max_db) {
    float normalized = cc_value / 127.0f;
    float db = min_db + normalized * (max_db - min_db);
    return powf(10.0f, db / 20.0f);  // dB to linear
}

// Create MIDI Learn state
SIT_MidiLearnState* SIT_MidiLearn_Create(void) {
    SIT_MidiLearnState *state = (SIT_MidiLearnState*)calloc(1, sizeof(SIT_MidiLearnState));
    if (!state) return NULL;
    
    state->learning = 0;
    state->mapping_count = 0;
    state->learn_timeout = 5.0;  // 5 second default timeout
    state->input_device = PM_NO_DEVICE;
    state->input_stream = NULL;
    state->learn_channel_filter = 0xFF;  // Omni by default
    state->learn_14bit_msb = 0xFF;  // No MSB detected
    
    return state;
}

// Destroy MIDI Learn state
void SIT_MidiLearn_Destroy(SIT_MidiLearnState *state) {
    if (!state) return;
    
    // Close MIDI stream if open
    if (state->input_stream) {
        Pm_Close(state->input_stream);
    }
    
    free(state);
}

// Auto-select first available MIDI input device
PmDeviceID SIT_MidiLearn_AutoSelectInput(void) {
    // Initialize PortMidi if needed
    static int pm_initialized = 0;
    if (!pm_initialized) {
        if (Pm_Initialize() != pmNoError) {
            return PM_NO_DEVICE;
        }
        pm_initialized = 1;
    }
    
    // Find first input device
    int device_count = Pm_CountDevices();
    for (int i = 0; i < device_count; i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if (info && info->input && !info->opened) {
            return i;
        }
    }
    
    return PM_NO_DEVICE;
}

// List all available MIDI input devices
int SIT_MidiLearn_ListInputDevices(SIT_MidiLearnDeviceInfo* devices, int max_count) {
    if (!devices || max_count <= 0) return 0;
    
    // Initialize PortMidi if needed
    static int pm_initialized = 0;
    if (!pm_initialized) {
        if (Pm_Initialize() != pmNoError) {
            return 0;
        }
        pm_initialized = 1;
    }
    
    int count = 0;
    int device_count = Pm_CountDevices();
    
    for (int i = 0; i < device_count && count < max_count; i++) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
        if (info) {
            devices[count].device_id = i;
            strncpy(devices[count].device_name, info->name, sizeof(devices[count].device_name) - 1);
            devices[count].device_name[sizeof(devices[count].device_name) - 1] = '\0';
            devices[count].is_input = info->input;
            devices[count].is_output = info->output;
            count++;
        }
    }
    
    return count;
}

// Set MIDI input device for learning
int SIT_MidiLearn_SetInputDevice(SIT_MidiLearnState *state, PmDeviceID device_id) {
    if (!state) return 0;
    
    // Close existing stream if open
    if (state->input_stream) {
        Pm_Close(state->input_stream);
        state->input_stream = NULL;
    }
    
    // Open new device
    if (device_id != PM_NO_DEVICE) {
        PmError err = Pm_OpenInput(&state->input_stream, device_id, NULL, 512, NULL, NULL);
        if (err != pmNoError) {
            state->input_stream = NULL;
            return 0;
        }
    }
    
    state->input_device = device_id;
    return 1;
}

// Set MIDI channel filter for learning
void SIT_MidiLearn_SetChannelFilter(SIT_MidiLearnState *state, uint8_t channel) {
    if (!state) return;
    state->learn_channel_filter = channel;
}

// Start learning a parameter
void SIT_MidiLearn_Start(SIT_MidiLearnState *state, 
                          int control_index,
                          const char *param_name,
                          float min_value,
                          float max_value,
                          SIT_MidiScaling scaling) {
    if (!state) return;
    
    state->learning = 1;
    state->learn_control_index = control_index;
    state->learn_start_time = _SIT_MidiLearn_GetTime();
    state->learn_min_value = min_value;
    state->learn_max_value = max_value;
    state->learn_scaling = scaling;
    state->learn_14bit_msb = 0xFF;  // Reset 14-bit detection
    
    if (param_name) {
        strncpy(state->learn_param_name, param_name, sizeof(state->learn_param_name) - 1);
        state->learn_param_name[sizeof(state->learn_param_name) - 1] = '\0';
    } else {
        state->learn_param_name[0] = '\0';
    }
}

// Cancel learning
void SIT_MidiLearn_Cancel(SIT_MidiLearnState *state) {
    if (!state) return;
    state->learning = 0;
}

// Process incoming MIDI CC
int SIT_MidiLearn_ProcessCC(SIT_MidiLearnState *state,
                             uint8_t channel,
                             uint8_t cc_number,
                             uint8_t cc_value,
                             float *output_value) {
    if (!state) return 0;
    
    // Check if we're in learn mode
    if (state->learning) {
        // Check channel filter
        if (state->learn_channel_filter != 0xFF && state->learn_channel_filter != channel) {
            return 0;  // Wrong channel, ignore
        }
        
        // 14-bit CC detection
        double current_time = _SIT_MidiLearn_GetTime();
        
        if (cc_number < 32) {
            // Potential MSB (CC 0-31)
            // Check if LSB follows within 100ms
            state->learn_14bit_msb = cc_number;
            state->learn_14bit_msb_time = current_time;
            
            // Don't learn yet, wait for potential LSB
            // But if timeout is approaching, learn as 7-bit
            double elapsed = current_time - state->learn_start_time;
            if (elapsed < state->learn_timeout - 0.2) {
                return 0;  // Wait for potential LSB
            }
            // Otherwise fall through and learn as 7-bit
        } else if (cc_number >= 32 && cc_number < 64) {
            // Potential LSB (CC 32-63)
            uint8_t expected_msb = cc_number - 32;
            
            // Check if we received corresponding MSB recently (within 100ms)
            if (state->learn_14bit_msb == expected_msb &&
                (current_time - state->learn_14bit_msb_time) < 0.1) {
                // 14-bit CC detected!
                cc_number = expected_msb;  // Use MSB as the CC number
                // LSB will be stored in cc_lsb field
            }
        }
        
        // Check for conflict
        int conflict_control = -1;
        for (int i = 0; i < state->mapping_count; i++) {
            if (state->mappings[i].cc_number == cc_number &&
                (state->mappings[i].channel == 0xFF || state->mappings[i].channel == channel)) {
                conflict_control = state->mappings[i].control_index;
                break;
            }
        }
        
        // Handle conflict
        if (conflict_control >= 0 && conflict_control != state->learn_control_index) {
            if (state->on_conflict) {
                if (!state->on_conflict(state->conflict_user_data, cc_number, conflict_control)) {
                    // User cancelled
                    state->learning = 0;
                    state->learn_14bit_msb = 0xFF;
                    return 1;  // Consume the CC
                }
            }
            // Remove old mapping
            for (int i = 0; i < state->mapping_count; i++) {
                if (state->mappings[i].control_index == conflict_control) {
                    // Shift remaining mappings
                    for (int j = i; j < state->mapping_count - 1; j++) {
                        state->mappings[j] = state->mappings[j + 1];
                    }
                    state->mapping_count--;
                    break;
                }
            }
        }
        
        // Create new mapping
        SIT_MidiLearnMapping mapping;
        mapping.cc_number = cc_number;
        
        // Check if this is 14-bit
        if (state->learn_14bit_msb == cc_number && cc_number < 32) {
            mapping.cc_lsb = cc_number + 32;  // LSB is MSB + 32
        } else {
            mapping.cc_lsb = 0xFF;  // 7-bit
        }
        
        mapping.channel = state->learn_channel_filter;  // Use filter setting
        mapping.control_index = state->learn_control_index;
        mapping.min_value = state->learn_min_value;
        mapping.max_value = state->learn_max_value;
        mapping.scaling = state->learn_scaling;
        strncpy(mapping.param_name, state->learn_param_name, sizeof(mapping.param_name) - 1);
        mapping.param_name[sizeof(mapping.param_name) - 1] = '\0';
        
        // Add to mappings (replace if exists for this control)
        int found = 0;
        for (int i = 0; i < state->mapping_count; i++) {
            if (state->mappings[i].control_index == state->learn_control_index) {
                state->mappings[i] = mapping;
                found = 1;
                break;
            }
        }
        
        if (!found && state->mapping_count < 128) {
            state->mappings[state->mapping_count++] = mapping;
        }
        
        // Calculate output value (7-bit for now, will be refined by 14-bit later)
        switch (mapping.scaling) {
            case SIT_MIDI_SCALING_LINEAR:
                *output_value = _SIT_MidiLearn_NormalizeLinear(cc_value, mapping.min_value, mapping.max_value);
                break;
            case SIT_MIDI_SCALING_LOG:
                *output_value = _SIT_MidiLearn_NormalizeLog(cc_value, mapping.min_value, mapping.max_value);
                break;
            case SIT_MIDI_SCALING_DB:
                *output_value = _SIT_MidiLearn_NormalizeDb(cc_value, mapping.min_value, mapping.max_value);
                break;
            case SIT_MIDI_SCALING_DISCRETE:
                *output_value = _SIT_MidiLearn_NormalizeLinear(cc_value, mapping.min_value, mapping.max_value);
                break;
        }
        
        // Exit learn mode
        state->learning = 0;
        state->learn_14bit_msb = 0xFF;
        
        // Call completion callback
        if (state->on_learn_complete) {
            state->on_learn_complete(state->callback_user_data, &mapping);
        }
        
        return 1;  // Learned, consume this CC
    }
    
    // Check learned mappings (including 14-bit)
    for (int i = 0; i < state->mapping_count; i++) {
        SIT_MidiLearnMapping *map = &state->mappings[i];
        
        // Check channel
        if (map->channel != 0xFF && map->channel != channel) {
            continue;  // Wrong channel
        }
        
        // Check if this is a 14-bit mapping
        if (map->cc_lsb != 0xFF) {
            // 14-bit mapping - need to track MSB/LSB
            // For now, just handle MSB (7-bit resolution)
            // Full 14-bit tracking would require state per mapping
            if (map->cc_number == cc_number) {
                // MSB received
                switch (map->scaling) {
                    case SIT_MIDI_SCALING_LINEAR:
                        *output_value = _SIT_MidiLearn_NormalizeLinear(cc_value, map->min_value, map->max_value);
                        break;
                    case SIT_MIDI_SCALING_LOG:
                        *output_value = _SIT_MidiLearn_NormalizeLog(cc_value, map->min_value, map->max_value);
                        break;
                    case SIT_MIDI_SCALING_DB:
                        *output_value = _SIT_MidiLearn_NormalizeDb(cc_value, map->min_value, map->max_value);
                        break;
                    case SIT_MIDI_SCALING_DISCRETE:
                        *output_value = _SIT_MidiLearn_NormalizeLinear(cc_value, map->min_value, map->max_value);
                        break;
                }
                return 1;  // Mapped, consume this CC
            }
        } else {
            // 7-bit mapping
            if (map->cc_number == cc_number) {
                // Apply learned mapping
                switch (map->scaling) {
                    case SIT_MIDI_SCALING_LINEAR:
                        *output_value = _SIT_MidiLearn_NormalizeLinear(cc_value, map->min_value, map->max_value);
                        break;
                    case SIT_MIDI_SCALING_LOG:
                        *output_value = _SIT_MidiLearn_NormalizeLog(cc_value, map->min_value, map->max_value);
                        break;
                    case SIT_MIDI_SCALING_DB:
                        *output_value = _SIT_MidiLearn_NormalizeDb(cc_value, map->min_value, map->max_value);
                        break;
                    case SIT_MIDI_SCALING_DISCRETE:
                        *output_value = _SIT_MidiLearn_NormalizeLinear(cc_value, map->min_value, map->max_value);
                        break;
                }
                return 1;  // Mapped, consume this CC
            }
        }
    }
    
    return 0;  // Not learned, normal processing
}

// Check for timeout
void SIT_MidiLearn_Update(SIT_MidiLearnState *state, double current_time) {
    if (!state || !state->learning) return;
    
    double elapsed = current_time - state->learn_start_time;
    if (elapsed >= state->learn_timeout) {
        // Timeout
        state->learning = 0;
        
        if (state->on_learn_timeout) {
            state->on_learn_timeout(state->callback_user_data, state->learn_control_index);
        }
    }
}

// Clear a specific mapping
void SIT_MidiLearn_ClearMapping(SIT_MidiLearnState *state, int control_index) {
    if (!state) return;
    
    for (int i = 0; i < state->mapping_count; i++) {
        if (state->mappings[i].control_index == control_index) {
            // Shift remaining mappings
            for (int j = i; j < state->mapping_count - 1; j++) {
                state->mappings[j] = state->mappings[j + 1];
            }
            state->mapping_count--;
            return;
        }
    }
}

// Clear all mappings
void SIT_MidiLearn_ClearAll(SIT_MidiLearnState *state) {
    if (!state) return;
    state->mapping_count = 0;
}

// Get mapping by control index
const SIT_MidiLearnMapping* SIT_MidiLearn_GetMapping(SIT_MidiLearnState *state, 
                                                       int control_index) {
    if (!state) return NULL;
    
    for (int i = 0; i < state->mapping_count; i++) {
        if (state->mappings[i].control_index == control_index) {
            return &state->mappings[i];
        }
    }
    
    return NULL;
}

// ================================================================================================
// JSON SERIALIZATION HELPERS (Following node_graph_serialization pattern)
// ================================================================================================

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} SIT_MidiLearnJSONBuffer;

static SIT_MidiLearnJSONBuffer* _SIT_MidiLearn_CreateJSONBuffer(void) {
    SIT_MidiLearnJSONBuffer* buf = (SIT_MidiLearnJSONBuffer*)calloc(1, sizeof(SIT_MidiLearnJSONBuffer));
    if (!buf) return NULL;
    
    buf->capacity = 4096;
    buf->data = (char*)malloc(buf->capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    
    buf->data[0] = '\0';
    buf->size = 0;
    return buf;
}

static void _SIT_MidiLearn_DestroyJSONBuffer(SIT_MidiLearnJSONBuffer* buf) {
    if (!buf) return;
    if (buf->data) free(buf->data);
    free(buf);
}

static int _SIT_MidiLearn_AppendToBuffer(SIT_MidiLearnJSONBuffer* buf, const char* str) {
    if (!buf || !str) return 0;
    
    size_t str_len = strlen(str);
    size_t needed = buf->size + str_len + 1;
    
    if (needed > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        while (new_capacity < needed) {
            new_capacity *= 2;
        }
        
        char* new_data = (char*)realloc(buf->data, new_capacity);
        if (!new_data) return 0;
        
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
    
    memcpy(buf->data + buf->size, str, str_len);
    buf->size += str_len;
    buf->data[buf->size] = '\0';
    
    return 1;
}

static int _SIT_MidiLearn_AppendFormatted(SIT_MidiLearnJSONBuffer* buf, const char* format, ...) {
    char temp[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    return _SIT_MidiLearn_AppendToBuffer(buf, temp);
}

static void _SIT_MidiLearn_EscapeJSONString(const char* src, char* dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        if (src[i] == '"' || src[i] == '\\') {
            if (j < dst_size - 2) {
                dst[j++] = '\\';
                dst[j++] = src[i];
            }
        } else if (src[i] == '\n') {
            if (j < dst_size - 2) {
                dst[j++] = '\\';
                dst[j++] = 'n';
            }
        } else if (src[i] == '\r') {
            if (j < dst_size - 2) {
                dst[j++] = '\\';
                dst[j++] = 'r';
            }
        } else if (src[i] == '\t') {
            if (j < dst_size - 2) {
                dst[j++] = '\\';
                dst[j++] = 't';
            }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static const char* _SIT_MidiLearn_ScalingToString(SIT_MidiScaling scaling) {
    switch (scaling) {
        case SIT_MIDI_SCALING_LINEAR: return "linear";
        case SIT_MIDI_SCALING_LOG: return "log";
        case SIT_MIDI_SCALING_DB: return "db";
        case SIT_MIDI_SCALING_DISCRETE: return "discrete";
        default: return "linear";
    }
}

static SIT_MidiScaling _SIT_MidiLearn_StringToScaling(const char* str) {
    if (strcmp(str, "log") == 0) return SIT_MIDI_SCALING_LOG;
    if (strcmp(str, "db") == 0) return SIT_MIDI_SCALING_DB;
    if (strcmp(str, "discrete") == 0) return SIT_MIDI_SCALING_DISCRETE;
    return SIT_MIDI_SCALING_LINEAR;
}

// ================================================================================================
// JSON PARSER (Simplified, following node_graph_serialization pattern)
// ================================================================================================

typedef enum {
    ML_JSON_TOKEN_OBJECT_START,
    ML_JSON_TOKEN_OBJECT_END,
    ML_JSON_TOKEN_ARRAY_START,
    ML_JSON_TOKEN_ARRAY_END,
    ML_JSON_TOKEN_STRING,
    ML_JSON_TOKEN_NUMBER,
    ML_JSON_TOKEN_TRUE,
    ML_JSON_TOKEN_FALSE,
    ML_JSON_TOKEN_NULL,
    ML_JSON_TOKEN_COLON,
    ML_JSON_TOKEN_COMMA,
    ML_JSON_TOKEN_EOF,
    ML_JSON_TOKEN_ERROR
} MLJSONTokenType;

typedef struct {
    MLJSONTokenType type;
    const char* start;
    size_t length;
    double number_value;
} MLJSONToken;

typedef struct {
    const char* json;
    size_t pos;
    size_t length;
    MLJSONToken current;
} MLJSONParser;

static void _ML_JSONInitParser(MLJSONParser* parser, const char* json) {
    parser->json = json;
    parser->pos = 0;
    parser->length = strlen(json);
    parser->current.type = ML_JSON_TOKEN_ERROR;
}

static int _ML_JSONIsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int _ML_JSONIsDigit(char c) {
    return c >= '0' && c <= '9';
}

static void _ML_JSONSkipWhitespace(MLJSONParser* parser) {
    while (parser->pos < parser->length && _ML_JSONIsWhitespace(parser->json[parser->pos])) {
        parser->pos++;
    }
}

static int _ML_JSONNextToken(MLJSONParser* parser) {
    _ML_JSONSkipWhitespace(parser);
    
    if (parser->pos >= parser->length) {
        parser->current.type = ML_JSON_TOKEN_EOF;
        return 1;
    }
    
    char c = parser->json[parser->pos];
    parser->current.start = &parser->json[parser->pos];
    parser->current.length = 1;
    
    switch (c) {
        case '{': parser->current.type = ML_JSON_TOKEN_OBJECT_START; parser->pos++; return 1;
        case '}': parser->current.type = ML_JSON_TOKEN_OBJECT_END; parser->pos++; return 1;
        case '[': parser->current.type = ML_JSON_TOKEN_ARRAY_START; parser->pos++; return 1;
        case ']': parser->current.type = ML_JSON_TOKEN_ARRAY_END; parser->pos++; return 1;
        case ':': parser->current.type = ML_JSON_TOKEN_COLON; parser->pos++; return 1;
        case ',': parser->current.type = ML_JSON_TOKEN_COMMA; parser->pos++; return 1;
        
        case '"': {
            parser->pos++;
            parser->current.start = &parser->json[parser->pos];
            parser->current.length = 0;
            
            while (parser->pos < parser->length && parser->json[parser->pos] != '"') {
                if (parser->json[parser->pos] == '\\') parser->pos++;
                parser->pos++;
                parser->current.length++;
            }
            
            if (parser->pos >= parser->length) return 0;
            parser->pos++;
            parser->current.type = ML_JSON_TOKEN_STRING;
            return 1;
        }
        
        case 't':
            if (parser->pos + 4 <= parser->length && strncmp(&parser->json[parser->pos], "true", 4) == 0) {
                parser->current.type = ML_JSON_TOKEN_TRUE;
                parser->pos += 4;
                return 1;
            }
            break;
        
        case 'f':
            if (parser->pos + 5 <= parser->length && strncmp(&parser->json[parser->pos], "false", 5) == 0) {
                parser->current.type = ML_JSON_TOKEN_FALSE;
                parser->pos += 5;
                return 1;
            }
            break;
        
        case 'n':
            if (parser->pos + 4 <= parser->length && strncmp(&parser->json[parser->pos], "null", 4) == 0) {
                parser->current.type = ML_JSON_TOKEN_NULL;
                parser->pos += 4;
                return 1;
            }
            break;
        
        default:
            if (_ML_JSONIsDigit(c) || c == '-') {
                size_t start = parser->pos;
                if (c == '-') parser->pos++;
                
                while (parser->pos < parser->length && _ML_JSONIsDigit(parser->json[parser->pos])) {
                    parser->pos++;
                }
                
                if (parser->pos < parser->length && parser->json[parser->pos] == '.') {
                    parser->pos++;
                    while (parser->pos < parser->length && _ML_JSONIsDigit(parser->json[parser->pos])) {
                        parser->pos++;
                    }
                }
                
                parser->current.type = ML_JSON_TOKEN_NUMBER;
                parser->current.start = &parser->json[start];
                parser->current.length = parser->pos - start;
                parser->current.number_value = atof(parser->current.start);
                return 1;
            }
            break;
    }
    
    parser->current.type = ML_JSON_TOKEN_ERROR;
    return 0;
}

static int _ML_JSONExpect(MLJSONParser* parser, MLJSONTokenType type) {
    if (parser->current.type != type) return 0;
    return _ML_JSONNextToken(parser);
}

static int _ML_JSONGetString(MLJSONParser* parser, char* buffer, size_t buffer_size) {
    if (parser->current.type != ML_JSON_TOKEN_STRING) return 0;
    
    size_t copy_len = parser->current.length;
    if (copy_len >= buffer_size) copy_len = buffer_size - 1;
    
    memcpy(buffer, parser->current.start, copy_len);
    buffer[copy_len] = '\0';
    
    return _ML_JSONNextToken(parser);
}

static int _ML_JSONGetNumber(MLJSONParser* parser, float* value) {
    if (parser->current.type != ML_JSON_TOKEN_NUMBER) return 0;
    *value = (float)parser->current.number_value;
    return _ML_JSONNextToken(parser);
}

static int _ML_JSONGetInt(MLJSONParser* parser, int* value) {
    if (parser->current.type != ML_JSON_TOKEN_NUMBER) return 0;
    *value = (int)parser->current.number_value;
    return _ML_JSONNextToken(parser);
}

static int _ML_JSONFindKey(MLJSONParser* parser, const char* key) {
    while (parser->current.type != ML_JSON_TOKEN_OBJECT_END && 
           parser->current.type != ML_JSON_TOKEN_EOF) {
        
        if (parser->current.type == ML_JSON_TOKEN_STRING) {
            if (parser->current.length == strlen(key) &&
                strncmp(parser->current.start, key, parser->current.length) == 0) {
                if (!_ML_JSONNextToken(parser)) return 0;
                if (!_ML_JSONExpect(parser, ML_JSON_TOKEN_COLON)) return 0;
                return 1;
            }
        }
        
        // Skip this key-value pair
        if (!_ML_JSONNextToken(parser)) return 0;
        if (parser->current.type == ML_JSON_TOKEN_COLON) {
            if (!_ML_JSONNextToken(parser)) return 0;
            // Skip value (simplified - assumes no nested objects/arrays)
            if (!_ML_JSONNextToken(parser)) return 0;
        }
        
        if (parser->current.type == ML_JSON_TOKEN_COMMA) {
            if (!_ML_JSONNextToken(parser)) return 0;
        }
    }
    
    return 0;
}

// ================================================================================================
// PRESET SAVE/LOAD IMPLEMENTATION
// ================================================================================================

// Save preset to JSON file
int SIT_MidiLearn_SavePreset(SIT_MidiLearnState *state, const char *filename) {
    if (!state || !filename) return 0;
    
    SIT_MidiLearnJSONBuffer* buf = _SIT_MidiLearn_CreateJSONBuffer();
    if (!buf) return 0;
    
    // Start JSON object
    if (!_SIT_MidiLearn_AppendToBuffer(buf, "{\n")) goto error;
    if (!_SIT_MidiLearn_AppendFormatted(buf, "  \"version\": \"1.0\",\n")) goto error;
    if (!_SIT_MidiLearn_AppendFormatted(buf, "  \"mapping_count\": %d,\n", state->mapping_count)) goto error;
    
    // Mappings array
    if (!_SIT_MidiLearn_AppendToBuffer(buf, "  \"mappings\": [\n")) goto error;
    
    for (int i = 0; i < state->mapping_count; i++) {
        SIT_MidiLearnMapping* map = &state->mappings[i];
        
        char escaped_name[128];
        _SIT_MidiLearn_EscapeJSONString(map->param_name, escaped_name, sizeof(escaped_name));
        
        if (!_SIT_MidiLearn_AppendToBuffer(buf, "    {\n")) goto error;
        if (!_SIT_MidiLearn_AppendFormatted(buf, "      \"cc_number\": %d,\n", map->cc_number)) goto error;
        if (!_SIT_MidiLearn_AppendFormatted(buf, "      \"cc_lsb\": %d,\n", map->cc_lsb)) goto error;
        if (!_SIT_MidiLearn_AppendFormatted(buf, "      \"channel\": %d,\n", map->channel)) goto error;
        if (!_SIT_MidiLearn_AppendFormatted(buf, "      \"control_index\": %d,\n", map->control_index)) goto error;
        if (!_SIT_MidiLearn_AppendFormatted(buf, "      \"param_name\": \"%s\",\n", escaped_name)) goto error;
        if (!_SIT_MidiLearn_AppendFormatted(buf, "      \"min_value\": %.6f,\n", map->min_value)) goto error;
        if (!_SIT_MidiLearn_AppendFormatted(buf, "      \"max_value\": %.6f,\n", map->max_value)) goto error;
        if (!_SIT_MidiLearn_AppendFormatted(buf, "      \"scaling\": \"%s\"\n", 
            _SIT_MidiLearn_ScalingToString(map->scaling))) goto error;
        
        if (i < state->mapping_count - 1) {
            if (!_SIT_MidiLearn_AppendToBuffer(buf, "    },\n")) goto error;
        } else {
            if (!_SIT_MidiLearn_AppendToBuffer(buf, "    }\n")) goto error;
        }
    }
    
    if (!_SIT_MidiLearn_AppendToBuffer(buf, "  ]\n")) goto error;
    if (!_SIT_MidiLearn_AppendToBuffer(buf, "}\n")) goto error;
    
    // Write to file
    FILE* file = fopen(filename, "w");
    if (!file) goto error;
    
    size_t written = fwrite(buf->data, 1, buf->size, file);
    fclose(file);
    
    _SIT_MidiLearn_DestroyJSONBuffer(buf);
    return (written == buf->size) ? 1 : 0;
    
error:
    _SIT_MidiLearn_DestroyJSONBuffer(buf);
    return 0;
}

// Load preset from JSON file
int SIT_MidiLearn_LoadPreset(SIT_MidiLearnState *state, const char *filename) {
    if (!state || !filename) return 0;
    
    // Read file
    FILE* file = fopen(filename, "r");
    if (!file) return 0;
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 1024 * 1024) {  // Max 1MB
        fclose(file);
        return 0;
    }
    
    char* json = (char*)malloc(file_size + 1);
    if (!json) {
        fclose(file);
        return 0;
    }
    
    size_t read = fread(json, 1, file_size, file);
    fclose(file);
    
    if (read != (size_t)file_size) {
        free(json);
        return 0;
    }
    
    json[file_size] = '\0';
    
    // Parse JSON
    MLJSONParser parser;
    _ML_JSONInitParser(&parser, json);
    
    if (!_ML_JSONNextToken(&parser)) goto error;
    if (!_ML_JSONExpect(&parser, ML_JSON_TOKEN_OBJECT_START)) goto error;
    
    // Parse version (optional)
    if (_ML_JSONFindKey(&parser, "version")) {
        char version[32];
        _ML_JSONGetString(&parser, version, sizeof(version));
        // TODO: Check version compatibility
    }
    
    // Reset parser to start of object
    _ML_JSONInitParser(&parser, json);
    _ML_JSONNextToken(&parser);
    _ML_JSONExpect(&parser, ML_JSON_TOKEN_OBJECT_START);
    
    // Find mappings array
    if (!_ML_JSONFindKey(&parser, "mappings")) goto error;
    if (!_ML_JSONExpect(&parser, ML_JSON_TOKEN_ARRAY_START)) goto error;
    
    // Clear existing mappings
    state->mapping_count = 0;
    
    // Parse each mapping
    while (parser.current.type != ML_JSON_TOKEN_ARRAY_END && 
           parser.current.type != ML_JSON_TOKEN_EOF) {
        
        if (state->mapping_count >= 128) break;
        
        if (!_ML_JSONExpect(&parser, ML_JSON_TOKEN_OBJECT_START)) goto error;
        
        SIT_MidiLearnMapping* map = &state->mappings[state->mapping_count];
        
        // Parse mapping fields
        if (_ML_JSONFindKey(&parser, "cc_number")) {
            int val;
            _ML_JSONGetInt(&parser, &val);
            map->cc_number = (uint8_t)val;
        }
        
        // Reset to start of object for next key
        size_t obj_start = parser.pos;
        while (parser.current.type != ML_JSON_TOKEN_OBJECT_START && parser.pos > 0) {
            parser.pos--;
        }
        _ML_JSONNextToken(&parser);
        
        if (_ML_JSONFindKey(&parser, "cc_lsb")) {
            int val;
            _ML_JSONGetInt(&parser, &val);
            map->cc_lsb = (uint8_t)val;
        }
        
        parser.pos = obj_start;
        _ML_JSONNextToken(&parser);
        
        if (_ML_JSONFindKey(&parser, "channel")) {
            int val;
            _ML_JSONGetInt(&parser, &val);
            map->channel = (uint8_t)val;
        }
        
        parser.pos = obj_start;
        _ML_JSONNextToken(&parser);
        
        if (_ML_JSONFindKey(&parser, "control_index")) {
            _ML_JSONGetInt(&parser, &map->control_index);
        }
        
        parser.pos = obj_start;
        _ML_JSONNextToken(&parser);
        
        if (_ML_JSONFindKey(&parser, "param_name")) {
            _ML_JSONGetString(&parser, map->param_name, sizeof(map->param_name));
        }
        
        parser.pos = obj_start;
        _ML_JSONNextToken(&parser);
        
        if (_ML_JSONFindKey(&parser, "min_value")) {
            _ML_JSONGetNumber(&parser, &map->min_value);
        }
        
        parser.pos = obj_start;
        _ML_JSONNextToken(&parser);
        
        if (_ML_JSONFindKey(&parser, "max_value")) {
            _ML_JSONGetNumber(&parser, &map->max_value);
        }
        
        parser.pos = obj_start;
        _ML_JSONNextToken(&parser);
        
        if (_ML_JSONFindKey(&parser, "scaling")) {
            char scaling_str[32];
            _ML_JSONGetString(&parser, scaling_str, sizeof(scaling_str));
            map->scaling = _SIT_MidiLearn_StringToScaling(scaling_str);
        }
        
        // Skip to end of object
        while (parser.current.type != ML_JSON_TOKEN_OBJECT_END && 
               parser.current.type != ML_JSON_TOKEN_EOF) {
            _ML_JSONNextToken(&parser);
        }
        
        if (!_ML_JSONExpect(&parser, ML_JSON_TOKEN_OBJECT_END)) goto error;
        
        state->mapping_count++;
        
        // Check for comma
        if (parser.current.type == ML_JSON_TOKEN_COMMA) {
            _ML_JSONNextToken(&parser);
        }
    }
    
    free(json);
    return 1;
    
error:
    free(json);
    return 0;
}

// Set conflict callback
void SIT_MidiLearn_SetConflictCallback(SIT_MidiLearnState *state,
                                        int (*callback)(void*, int, int),
                                        void *user_data) {
    if (!state) return;
    state->on_conflict = callback;
    state->conflict_user_data = user_data;
}

// Set learn complete callback
void SIT_MidiLearn_SetLearnCompleteCallback(SIT_MidiLearnState *state,
                                             void (*callback)(void*, const SIT_MidiLearnMapping*),
                                             void *user_data) {
    if (!state) return;
    state->on_learn_complete = callback;
    state->callback_user_data = user_data;
}

#endif /* MIDI_LEARN_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* MIDI_LEARN_H */
