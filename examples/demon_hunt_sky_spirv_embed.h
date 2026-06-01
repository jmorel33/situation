/**
 * Build-time embedded Demon Hunt skydome SPIR-V (OpenGL + Vulkan targets).
 * Lengths are zero until `compile_demon_hunt_shaders.bat` runs with glslc and regenerates `demon_hunt_sky_spirv_embed.c`.
 */
#ifndef EXAMPLES_DEMON_HUNT_SKY_SPIRV_EMBED_H
#define EXAMPLES_DEMON_HUNT_SKY_SPIRV_EMBED_H

#include <stddef.h>

/* OpenGL-target (--target-env=opengl) */
extern const unsigned char demon_hunt_sky_vs_spv[];
extern const size_t demon_hunt_sky_vs_spv_len;
extern const unsigned char demon_hunt_sky_fs_spv[];
extern const size_t demon_hunt_sky_fs_spv_len;

/* Vulkan-target (--target-env=vulkan, VULKAN=1 in GLSL) */
extern const unsigned char demon_hunt_sky_vs_spv_vk[];
extern const size_t demon_hunt_sky_vs_spv_vk_len;
extern const unsigned char demon_hunt_sky_fs_spv_vk[];
extern const size_t demon_hunt_sky_fs_spv_vk_len;

#endif
