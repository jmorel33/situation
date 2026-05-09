/*
 * hello_fancy_dll_wrapper.c
 * 
 * Wrapper to use hello_fancy with situation.dll
 * This file properly configures the includes for DLL usage
 */

#if defined(_WIN32)
    #define NOMINMAX
#endif

// DO NOT define SITUATION_IMPLEMENTATION when using the DLL!
// #define SITUATION_IMPLEMENTATION  <-- REMOVED

// Define that we're using the shared library
#define SITUATION_USE_SHARED
#define SITUATION_USE_VULKAN
#define SITUATION_ENABLE_THREADING
#define SITUATION_ENABLE_SHADER_COMPILER

#include "../situation.h"
#include <stdio.h>
#include <math.h>

// Now include the actual hello_fancy implementation
// We need to undef SITUATION_IMPLEMENTATION first if it was defined
#undef SITUATION_IMPLEMENTATION

// Include the main function from hello_fancy
#include "hello_fancy.c"
