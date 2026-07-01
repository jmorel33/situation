/***************************************************************************************************
*
*   situation_base_types.h - Primitive Types and Resource Handles
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Foundational types shared across all of Situation's public API, callbacks, and bindings.
*   This file has zero library dependencies — it requires only standard C headers.
*
*   Include order: situation_base_types.h → situation_base_callbacks.h → situation_api.h
*
*   Do not include this file directly — include situation.h or situation_api.h.
*
***************************************************************************************************/
#ifndef SITUATION_BASE_TYPES_H
#define SITUATION_BASE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ================================================================================================
// MODIFIER BITMASK
// ================================================================================================

/**
 * @brief Keyboard modifier bitmask type.
 * @details Bitfield — values may be OR'd together. typedef int keeps C bitwise ops natural
 *          while giving language bindings a typed surface.
 */
typedef int SituationModifiers;
#define SIT_MOD_SHIFT              ((SituationModifiers)0x0001)
#define SIT_MOD_CONTROL            ((SituationModifiers)0x0002)
#define SIT_MOD_ALT                ((SituationModifiers)0x0004)
#define SIT_MOD_SUPER              ((SituationModifiers)0x0008)
#define SIT_MOD_CAPS_LOCK          ((SituationModifiers)0x0010)
#define SIT_MOD_NUM_LOCK           ((SituationModifiers)0x0020)

// ================================================================================================
// MATH TYPES
// ================================================================================================

typedef struct ColorHSV  { float h, s, v; }           ColorHSV;
typedef struct ColorYPQA { unsigned char y, p, q, a; } ColorYPQA;
typedef struct ColorYPQf { float y, p, q, a; }         ColorYPQf;
typedef struct ColorRGBA { unsigned char r, g, b, a; } ColorRGBA;
/** 10-bit RGBA components (0–1023). Alpha uses full 10 bits; A2R10G10B10 packing uses the top 2 bits. */
typedef struct ColorRGBA10 {
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t a;
} ColorRGBA10;
typedef ColorRGBA Color;

typedef union Vector2 {
    struct { float x, y; };
    float raw[2];
} Vector2;

typedef union Vector3 {
    struct { float x, y, z; };
    struct { float r, g, b; };
    float raw[3];
} Vector3;

typedef union Vector4 {
    struct { float x, y, z, w; };
    struct { float r, g, b, a; };
    float raw[4];
} Vector4;

typedef struct SitRectangle { float x, y, width, height; } SitRectangle;

// ================================================================================================
// GPU RESOURCE HANDLES
// ================================================================================================

/**
 * @brief Opaque handle for a GPU texture. Uses Index + Generation for safe hot-reload.
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
    int width;
    int height;
} SituationTexture;

/**
 * @brief Opaque handle for a GPU mesh (vertex + index buffers).
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
    int index_count;
    int vertex_count;
    size_t vertex_stride;
} SituationMesh;

/**
 * @brief Opaque handle for a compiled shader / pipeline.
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationShader;

/**
 * @brief Opaque handle for a GPU data buffer (VBO, SSBO, UBO, etc.).
 * @note slot_index + generation must remain the first 8 bytes (command buffer packing contract).
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
    size_t   size_in_bytes;
    uint32_t usage_flags;   /* SituationBufferUsageFlags — typed in situation_api.h */
} SituationBuffer;

/**
 * @brief Opaque handle for a compute pipeline.
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationComputePipeline;

/**
 * @brief Opaque handle for a user offscreen render target (color + optional depth).
 * @details Distinct from Virtual Displays — no compositor, z-order, or idle hooks.
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationRenderTarget;

#define SITUATION_NULL_RENDER_TARGET ((SituationRenderTarget){0, 0})

/**
 * @brief Opaque handle for a user GPU query pool (timestamps or occlusion).
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationQueryPool;

#define SITUATION_NULL_QUERY_POOL ((SituationQueryPool){0, 0})

/**
 * @brief Generational handle for a loaded audio sound asset.
 */
typedef struct {
    uint32_t slot_index;
    uint32_t generation;
} SituationSound;

/** Deprecated alias — prefer SituationSound. */
typedef SituationSound SituationSoundHandle;

#define SITUATION_NULL_HANDLE ((SituationSound){0, 0})

/**
 * @brief Handle for an actively playing procedural tone. 0 = invalid/expired.
 */
typedef uint32_t SituationToneHandle;

/**
 * @brief Handle for a node in the audio graph.
 */
typedef uint32_t SituationNodeHandle;
#define SITUATION_INVALID_NODE_HANDLE 0xFFFFFFFF

// ================================================================================================
// AUDIO STREAM ABSTRACTION
// ================================================================================================
// These types wrap the miniaudio streaming primitives so that situation_base_callbacks.h
// (and language bindings) remain free of miniaudio includes.

/**
 * @brief Byte count / frame count type for audio streaming operations.
 *        Maps 1:1 to miniaudio's ma_uint64 at the implementation boundary.
 */
typedef uint64_t SituationStreamSize;

/**
 * @brief Seek origin for custom audio stream callbacks.
 *        Maps 1:1 to ma_seek_origin at the implementation boundary.
 */
typedef enum {
    SIT_SEEK_FROM_START   = 0,   /* ma_seek_origin_start   */
    SIT_SEEK_FROM_CURRENT = 1,   /* ma_seek_origin_current */
    SIT_SEEK_FROM_END     = 2    /* ma_seek_origin_end     */
} SituationSeekOrigin;

/**
 * @brief Result type for custom audio stream seek/read callbacks.
 *        SIT_STREAM_SUCCESS maps to MA_SUCCESS (0); any non-zero value is an error.
 */
typedef int SituationStreamResult;
#define SIT_STREAM_SUCCESS 0

#endif // SITUATION_BASE_TYPES_H
