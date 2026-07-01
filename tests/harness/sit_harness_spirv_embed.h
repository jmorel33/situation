/**
 * Build-time embedded harness SPIR-V (OpenGL or Vulkan target).
 * Lengths are zero until compile_harness_shaders.bat runs with glslc.
 */
#ifndef SIT_HARNESS_SPIRV_EMBED_H
#define SIT_HARNESS_SPIRV_EMBED_H

#include <stddef.h>

#if defined(SITUATION_USE_OPENGL)
#define SIT_HARNESS_SPIRV_PREFIX sit_harness_gl
#elif defined(SITUATION_USE_VULKAN)
#define SIT_HARNESS_SPIRV_PREFIX sit_harness_vk
#endif

#define SIT_HARNESS_SPIRV_JOIN2(a, b) a##b
#define SIT_HARNESS_SPIRV_JOIN(a, b) SIT_HARNESS_SPIRV_JOIN2(a, b)

#define sit_harness_passthrough_vs_spv SIT_HARNESS_SPIRV_JOIN(SIT_HARNESS_SPIRV_PREFIX, _passthrough_vs_spv)
#define sit_harness_passthrough_vs_spv_len SIT_HARNESS_SPIRV_JOIN(SIT_HARNESS_SPIRV_PREFIX, _passthrough_vs_spv_len)
#define sit_harness_dual_ssbo_fs_spv SIT_HARNESS_SPIRV_JOIN(SIT_HARNESS_SPIRV_PREFIX, _dual_ssbo_fs_spv)
#define sit_harness_dual_ssbo_fs_spv_len SIT_HARNESS_SPIRV_JOIN(SIT_HARNESS_SPIRV_PREFIX, _dual_ssbo_fs_spv_len)
#define sit_harness_ubo_ssbo_fs_spv SIT_HARNESS_SPIRV_JOIN(SIT_HARNESS_SPIRV_PREFIX, _ubo_ssbo_fs_spv)
#define sit_harness_ubo_ssbo_fs_spv_len SIT_HARNESS_SPIRV_JOIN(SIT_HARNESS_SPIRV_PREFIX, _ubo_ssbo_fs_spv_len)

extern const unsigned char sit_harness_passthrough_vs_spv[];
extern const size_t sit_harness_passthrough_vs_spv_len;
extern const unsigned char sit_harness_dual_ssbo_fs_spv[];
extern const size_t sit_harness_dual_ssbo_fs_spv_len;
extern const unsigned char sit_harness_ubo_ssbo_fs_spv[];
extern const size_t sit_harness_ubo_ssbo_fs_spv_len;

/* UBO_SSBO_SAMPLER FS — Vulkan only; zero-length stub emitted for OpenGL builds
 * by gen_spirv_embed.ps1 (compile_harness_shaders.bat does not compile this shader for GL).
 * The test is #if SITUATION_USE_VULKAN gated and will never dereference these on GL builds. */
#define sit_harness_ubo_ssbo_sampler_fs_spv \
    SIT_HARNESS_SPIRV_JOIN(SIT_HARNESS_SPIRV_PREFIX, _ubo_ssbo_sampler_fs_spv)
#define sit_harness_ubo_ssbo_sampler_fs_spv_len \
    SIT_HARNESS_SPIRV_JOIN(SIT_HARNESS_SPIRV_PREFIX, _ubo_ssbo_sampler_fs_spv_len)
extern const unsigned char sit_harness_ubo_ssbo_sampler_fs_spv[];
extern const size_t sit_harness_ubo_ssbo_sampler_fs_spv_len;

#endif /* SIT_HARNESS_SPIRV_EMBED_H */
