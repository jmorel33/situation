/*
 * sit_config.h - Situation Library Configuration
 *
 * This file allows you to override internal macros and configuration options.
 * It is included at the very beginning of situation.h.
 */

#ifndef SIT_CONFIG_H
#define SIT_CONFIG_H

// Example: Override memory allocators
// #define SIT_MALLOC(sz)       my_malloc(sz)
// #define SIT_CALLOC(n, sz)    my_calloc(n, sz)
// #define SIT_REALLOC(ptr, sz) my_realloc(ptr, sz)
// #define SIT_FREE(ptr)        my_free(ptr)

// Example: Disable specific modules
// #define SITUATION_DISABLE_AUDIO
// #define SITUATION_DISABLE_GRAPHICS

#endif // SIT_CONFIG_H
