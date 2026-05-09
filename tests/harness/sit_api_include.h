/**
 * @file sit_api_include.h
 * @brief Wrapper that includes situation_api.h with all required prerequisites.
 *
 * situation_api.h expects certain types (mat4, GLFWwindow, ma_* types) to be
 * available. This header provides the minimal forward declarations and includes
 * needed to parse the API declarations without compiling the full library.
 */

#ifndef SIT_API_INCLUDE_H
#define SIT_API_INCLUDE_H

// Standard C headers needed by situation_api.h
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

// Forward declarations for external library types used in the API
// (We don't need the full headers — just enough for the compiler to parse pointers/typedefs)

// GLFW types (used as opaque pointers in the API)
typedef struct GLFWwindow GLFWwindow;
typedef struct GLFWmonitor GLFWmonitor;

// cglm mat4 type (used by value in a few functions)
#include <cglm/cglm.h>

// miniaudio types used in the public API
typedef uint64_t ma_uint64;
typedef int64_t ma_int64;
typedef int ma_result;
typedef int ma_seek_origin;
typedef struct ma_node ma_node;
typedef struct ma_node_graph ma_node_graph;
typedef struct { unsigned char data[256]; } ma_device_id; // Opaque, size doesn't matter for declarations

// Vulkan types (opaque handles used in the API when SITUATION_USE_VULKAN is defined)
#ifdef SITUATION_USE_VULKAN
    #define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
    #define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
    VK_DEFINE_HANDLE(VkInstance)
    VK_DEFINE_HANDLE(VkDevice)
    VK_DEFINE_HANDLE(VkPhysicalDevice)
    VK_DEFINE_HANDLE(VkCommandBuffer)
    VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkRenderPass)
    #undef VK_DEFINE_HANDLE
    #undef VK_DEFINE_NON_DISPATCHABLE_HANDLE
#endif

// Now include the actual API
#include "sit/situation_api.h"

#endif // SIT_API_INCLUDE_H
