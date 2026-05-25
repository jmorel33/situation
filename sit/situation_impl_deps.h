/***************************************************************************************************
*
*   situation_impl_deps.h - Third-Party Dependency Implementations
*   (c) 2025-2026 Jacques Morel
*   MIT Licensed
*
*   "Zero Friction" automatic dependency implementation.
*   This file includes and implements all third-party libraries used by Situation:
*   - STB libraries (image, image_write, truetype)
*   - MiniAudio
*   - GLAD (OpenGL loader)
*   - VMA wrapper (Vulkan Memory Allocator)
*
*   Define SITUATION_NO_STB (or specific SITUATION_NO_STB_*) to opt-out of STB.
*
*   This is an implementation-internal file. Do not include directly.
*
***************************************************************************************************/
#ifndef SITUATION_IMPL_DEPS_H
#define SITUATION_IMPL_DEPS_H

// ==================================================================================
// Platform Headers
// ==================================================================================

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
    #include <immintrin.h>
#endif
#include <time.h>

// ==================================================================================
// STB Libraries
// ==================================================================================

#if !defined(SITUATION_NO_STB)

    // --- stb_image.h (Loading) ---
    #if !defined(SITUATION_NO_STB_IMAGE)
        #ifndef STB_IMAGE_IMPLEMENTATION
            #define STB_IMAGE_IMPLEMENTATION
        #endif
        #include "stb_image.h"
    #endif

    // --- stb_image_write.h (Screenshots/Export) ---
    #if !defined(SITUATION_NO_STB_IMAGE_WRITE)
        #ifndef STB_IMAGE_WRITE_IMPLEMENTATION
            #define STB_IMAGE_WRITE_IMPLEMENTATION
        #endif
        #include "stb_image_write.h"
    #endif

    // --- stb_image_resize2.h (CPU resize) ---
    #if !defined(SITUATION_NO_STB_IMAGE_RESIZE)
        #ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
            #define STB_IMAGE_RESIZE_IMPLEMENTATION
        #endif
        #ifndef STBIR_MALLOC
            #define STBIR_MALLOC(size,user_data) ((void)(user_data), SIT_MALLOC(size))
        #endif
        #ifndef STBIR_FREE
            /* STBIR_FREE must be an expression (comma-op in stbir internals); SIT_FREE is a statement. */
            static inline void sit_stbir_free(void* ptr, void* user_data) {
                (void)user_data;
                if (ptr) {
                    free(ptr);
                }
            }
            #define STBIR_FREE(ptr,user_data) (sit_stbir_free((ptr), (user_data)), (void)0)
        #endif
        #include "stb_image_resize2.h"
    #endif

    // --- stb_truetype.h (Text Rendering) ---
    #if !defined(SITUATION_NO_STB_TRUETYPE)
        #ifndef STB_TRUETYPE_IMPLEMENTATION
            #define STB_TRUETYPE_IMPLEMENTATION
        #endif
        #include "stb_truetype.h"
    #endif

#endif

// ==================================================================================
// MiniAudio
// ==================================================================================

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

// ==================================================================================
// Platform & Backend
// ==================================================================================

#if defined(_WIN32)
    #pragma comment(lib, "xinput.lib")
#endif

#if defined(SITUATION_USE_OPENGL)
#include <glad.c>
#endif

#include <ctype.h>

#if defined(SITUATION_USE_VULKAN)
#include "vma_wrapper.h"
#endif

// ==================================================================================
// STB Fallback Prototypes (when user hasn't included implementations)
// ==================================================================================

#if defined(STB_IMAGE_WRITE_IMPLEMENTATION)
#else
    #ifdef __cplusplus
    extern "C" {
    #endif
    extern int stbi_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes);
    #ifdef __cplusplus
    }
    #endif
#endif

#if defined(STB_IMAGE_IMPLEMENTATION)
#else
    #ifdef __cplusplus
    extern "C" {
    #endif
    extern unsigned char *stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);
    extern unsigned char *stbi_load_from_memory(unsigned char const *buffer, int len, int *x, int *y, int *comp, int req_comp);
    extern void stbi_free(void *retval_from_stbi_load);
    #ifdef __cplusplus
    }
    #endif
#endif

#if defined(STB_IMAGE_RESIZE_IMPLEMENTATION)
#else
    #ifdef __cplusplus
    extern "C" {
    #endif
    typedef enum stbir_pixel_layout {
        STBIR_RGBA = 4
    } stbir_pixel_layout;

    extern unsigned char * stbir_resize_uint8_srgb( const unsigned char *input_pixels , int input_w , int input_h, int input_stride_in_bytes,
                                                    unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                                    stbir_pixel_layout pixel_type );
    #ifdef __cplusplus
    }
    #endif
#endif

#endif // SITUATION_IMPL_DEPS_H
