#ifndef TEST_GRAPHICS_SPIRV_H
#define TEST_GRAPHICS_SPIRV_H

void test_async_shader_spirv_memory_vulkan(void);
void test_spirv_memory_invalid_params(void);
void test_spirv_error_code_reporting(void);
void test_spirv_memory_dual_ssbo_readback(void);
void test_spirv_memory_ubo_ssbo_readback(void);
void test_spirv_memory_post_link_resources(void);
void test_spirv_disk_roundtrip(void);

#if defined(SITUATION_USE_VULKAN)
void test_demon_hunt_sky_spirv_vk_begin_poll(void);
#endif

#if defined(SITUATION_USE_OPENGL)
void test_spirv_memory_dual_ssbo_explicit_bind(void);
void test_spirv_bind_api_invalid_shader(void);
#endif

#endif /* TEST_GRAPHICS_SPIRV_H */
