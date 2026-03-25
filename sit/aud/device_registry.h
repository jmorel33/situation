/***************************************************************************************************
*
*   sit/aud/device_registry.h - Audio Device Registry Implementation
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   ================================================================================================
*   DESCRIPTION
*   ================================================================================================
*   Internal implementation file for the device registry system.
*   
*   PUBLIC TYPES AND API: All public types, enums, and function declarations are in situation_api.h
*   This file contains ONLY the implementation code (static functions and global storage).
*   
*   Users should never include this file directly - it's included automatically via situation_impl.h
*   
***************************************************************************************************/

#ifndef SITUATION_DEVICE_REGISTRY_H
#define SITUATION_DEVICE_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// NOTE: All public types (SituationDeviceCategory, SituationNodeType, SituationControlDesc, etc.)
// are defined in situation_api.h. This file contains only implementation code.

// ================================================================================================
// IMPLEMENTATION - GLOBAL REGISTRY STORAGE
// ================================================================================================

// Global registry storage
static SituationDeviceMetadata g_device_registry[SITUATION_MAX_DEVICES];
static int g_device_registry_count = 0;
static bool g_registry_initialized = false;

// Initialize registry (called once)
static void _SituationInitRegistry(void) {
    if (g_registry_initialized) return;
    memset(g_device_registry, 0, sizeof(g_device_registry));
    g_device_registry_count = 0;
    g_registry_initialized = true;
}

// Validate device metadata
static SituationError SituationValidateDeviceMetadata(const SituationDeviceMetadata* meta) {
    if (!meta) return SITUATION_ERROR_DEVICE_METADATA_INVALID;
    if (meta->name[0] == '\0') return SITUATION_ERROR_DEVICE_METADATA_INVALID;
    if (meta->num_controls > SITUATION_MAX_CONTROLS_PER_DEVICE) return SITUATION_ERROR_DEVICE_METADATA_INVALID;
    
    // Validate controls
    for (int i = 0; i < meta->num_controls; i++) {
        const SituationControlDesc* ctrl = &meta->controls[i];
        if (ctrl->name[0] == '\0') return SITUATION_ERROR_DEVICE_METADATA_INVALID;
        if (ctrl->type == SITUATION_CONTROL_FLOAT || ctrl->type == SITUATION_CONTROL_INT) {
            if (ctrl->min_value > ctrl->max_value) return SITUATION_ERROR_DEVICE_METADATA_INVALID;
            if (ctrl->default_value < ctrl->min_value || ctrl->default_value > ctrl->max_value) {
                return SITUATION_ERROR_DEVICE_METADATA_INVALID;
            }
        }
    }
    
    return SITUATION_SUCCESS;
}

// Register device type
SituationError SituationRegisterDeviceType(const SituationDeviceMetadata* meta) {
    _SituationInitRegistry();
    
    // Validate metadata
    SituationError err = SituationValidateDeviceMetadata(meta);
    if (err != SITUATION_SUCCESS) return err;
    
    // Check for duplicates
    for (int i = 0; i < g_device_registry_count; i++) {
        if (g_device_registry[i].type == meta->type) {
            return SITUATION_ERROR_DEVICE_TYPE_ALREADY_REGISTERED;
        }
    }
    
    // Check capacity
    if (g_device_registry_count >= SITUATION_MAX_DEVICES) {
        return SITUATION_ERROR_DEVICE_REGISTRY_FULL;
    }
    
    // Copy metadata to registry
    g_device_registry[g_device_registry_count] = *meta;
    g_device_registry_count++;
    
    return SITUATION_SUCCESS;
}

// Query device metadata
SituationError SituationGetDeviceMetadata(SituationNodeType type, SituationDeviceMetadata* out_meta) {
    if (!out_meta) return SITUATION_ERROR_DEVICE_METADATA_INVALID;
    
    for (int i = 0; i < g_device_registry_count; i++) {
        if (g_device_registry[i].type == type) {
            *out_meta = g_device_registry[i];
            return SITUATION_SUCCESS;
        }
    }
    
    return SITUATION_ERROR_DEVICE_TYPE_NOT_REGISTERED;
}

// Query device metadata pointer (for node graph use)
static const SituationDeviceMetadata* SituationGetDeviceMetadataPtr(SituationNodeType type) {
    for (int i = 0; i < g_device_registry_count; i++) {
        if (g_device_registry[i].type == type) {
            return &g_device_registry[i];
        }
    }
    return NULL;
}

// Check if device is registered
bool SituationIsDeviceRegistered(SituationNodeType type) {
    for (int i = 0; i < g_device_registry_count; i++) {
        if (g_device_registry[i].type == type) {
            return true;
        }
    }
    return false;
}

// Get registered device count
int SituationGetRegisteredDeviceCount(void) {
    return g_device_registry_count;
}

// Iterate registry
static void SituationIterateRegistry(void (*callback)(const SituationDeviceMetadata* meta, void* user_data), void* user_data) {
    if (!callback) return;
    for (int i = 0; i < g_device_registry_count; i++) {
        callback(&g_device_registry[i], user_data);
    }
}

// Get metadata by index
static SituationError SituationGetDeviceMetadataByIndex(int index, SituationDeviceMetadata* out_meta) {
    if (!out_meta) return SITUATION_ERROR_DEVICE_METADATA_INVALID;
    if (index < 0 || index >= g_device_registry_count) return SITUATION_ERROR_DEVICE_TYPE_NOT_REGISTERED;
    
    *out_meta = g_device_registry[index];
    return SITUATION_SUCCESS;
}

// Clear registry
static void SituationClearRegistry(void) {
    memset(g_device_registry, 0, sizeof(g_device_registry));
    g_device_registry_count = 0;
}

// Get category name
const char* SituationGetCategoryName(SituationDeviceCategory category) {
    switch (category) {
        case SITUATION_DEVICE_EFFECT: return "Effect";
        case SITUATION_DEVICE_SOURCE: return "Source";
        case SITUATION_DEVICE_CAPTURE: return "Capture";
        case SITUATION_DEVICE_UTILITY: return "Utility";
        case SITUATION_DEVICE_MODULATOR: return "Modulator";
        case SITUATION_DEVICE_ANALYZER: return "Analyzer";
        case SITUATION_DEVICE_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

// Get control type name
static const char* SituationGetControlTypeName(SituationControlType type) {
    switch (type) {
        case SITUATION_CONTROL_FLOAT: return "Float";
        case SITUATION_CONTROL_INT: return "Int";
        case SITUATION_CONTROL_BOOL: return "Bool";
        case SITUATION_CONTROL_ENUM: return "Enum";
        default: return "Unknown";
    }
}

#endif // SITUATION_DEVICE_REGISTRY_H
