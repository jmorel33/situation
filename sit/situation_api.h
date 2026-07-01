/***************************************************************************************************
*
*   situation_api.h - Public API Umbrella
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   Single supported entry point for all Situation API declarations (types, enums, SITAPI).
*   Fans out to situation_api_*.h submodules; implementations live in situation_impl*.h.
*
*   Prefer #include "sit/situation.h" in application code. Bindings and doc tools may include
*   this file directly. Manual: doc/situation_sdk.md | Index: doc/situation_api_index.md
*
***************************************************************************************************/
#ifndef SITUATION_API_H
#define SITUATION_API_H

#if defined(SITUATION_ENABLE_THREADING) && !defined(SITUATION_ENABLE_RENDER_THREAD)
    #define SITUATION_ENABLE_RENDER_THREAD
#endif

/* POSIX feature macros — before any system headers (stat, nanosleep, readlink). */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#if defined(_WIN32)
    #if defined(SITUATION_BUILD_SHARED)
        #define SITAPI __declspec(dllexport)
    #elif defined(SITUATION_USE_SHARED)
        #define SITAPI __declspec(dllimport)
    #else
        #define SITAPI
    #endif
#else
    #if defined(SITUATION_BUILD_SHARED)
        #define SITAPI __attribute__((visibility("default")))
    #else
        #define SITAPI
    #endif
#endif

#if defined(_MSC_VER)
    #define SIT_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
    #define SIT_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
    #define SIT_DEPRECATED(msg)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SITUATION_USE_OPENGL)
SITAPI void _SituationLogGLError(const char* file, int line);
#define SIT_CHECK_GL_ERROR() _SituationLogGLError(__FILE__, __LINE__)
#else
#define SIT_CHECK_GL_ERROR() do {} while(0)
#endif

#if !defined(SITUATION_USE_VULKAN) && !defined(SITUATION_USE_OPENGL)
    #error "Define SITUATION_USE_VULKAN or SITUATION_USE_OPENGL before including situation.h"
#endif
#if defined(SITUATION_USE_VULKAN) && defined(SITUATION_USE_OPENGL)
    #error "Only one renderer backend (SITUATION_USE_VULKAN or SITUATION_USE_OPENGL)"
#endif

/* P2.2 submodule chain: base → config → types → platform → graphics → audio → system → [deprecated]
 * Module catalog: doc/situation_api_index.md
 * Tracy CPU zone macros: situation_profiling.h via situation.h (not part of this umbrella). */

#include "situation_base_errno.h"
#include "situation_base_types.h"
#include "situation_base_callbacks.h"
#include "situation_base_etc.h"
#include "situation_api_config.h"
#include "situation_api_types_system.h"
#include "situation_api_types_gpu.h"
#include "situation_api_types_audio.h"
#include "situation_api_platform.h"
#include "situation_api_graphics.h"
#include "situation_api_grid.h"
#include "situation_api_audio.h"
#include "situation_api_system.h"
#ifndef SITUATION_INCLUDE_DEPRECATED_API
    #define SITUATION_INCLUDE_DEPRECATED_API 1
#endif
#if SITUATION_INCLUDE_DEPRECATED_API
#include "situation_api_deprecated.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* SITUATION_API_H */
