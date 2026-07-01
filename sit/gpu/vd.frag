/**
 * @internal
 * @shader SIT_VD_FRAGMENT_SHADER
 * @brief Fragment stage for simple Virtual Display compositing.
 */
#version 450 core
layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 outColor;

#if defined(SITUATION_USE_VULKAN)
layout(set = 1, binding = 0) uniform sampler2D u_screenTexture;
#elif defined(SITUATION_USE_OPENGL)
layout(location = 1, binding = 4) uniform sampler2D u_screenTexture;
#endif

#include "sit/gpu/vd_colorburst_subset.glslh"
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_GL_VD_COMPOSITOR_SPIRV)
#if defined(SITUATION_USE_VULKAN)
#define SIT_TP_CONFIG_UBO_SET 2
#define SIT_TP_CONFIG_UBO_BINDING 0
#define SIT_TP_LAYER_PARAMS_SSBO_SET 2
#define SIT_TP_LAYER_PARAMS_SSBO_BINDING 1
#else
#define SIT_TP_CONFIG_UBO_SET 0
#define SIT_TP_CONFIG_UBO_BINDING 6
#define SIT_TP_LAYER_PARAMS_SSBO_SET 0
#define SIT_TP_LAYER_PARAMS_SSBO_BINDING 7
#endif
#include "sit/gpu/test_patterns/sit_tp_config_ubo.glslh"
#include "sit/gpu/vd_idle_pattern.glslh"
#endif

#if defined(SITUATION_USE_VULKAN)
layout(push_constant) uniform VDPushConstants { mat4 model; float opacity;
    int is_idle;
    int fallback_mode;
    float elapsed_idle;
    vec4 fallback_color;
    uint texture_id;
} pc;
void main() {
    if (pc.is_idle != 0) {
        vec3 rgb;
        if (pc.fallback_mode == 1) {
            rgb = _sit_smpte_color_bars(v_texCoord);
        } else if (pc.fallback_mode == 2) {
            rgb = _sit_vd_idle_pattern_rgb(pc.fallback_mode, v_texCoord, pc.fallback_color.rgb);
        } else {
            rgb = pc.fallback_color.rgb;
        }
        outColor = vec4(rgb, pc.opacity);
    } else {
        vec4 texColor = texture(u_screenTexture, v_texCoord);
        outColor = vec4(texColor.rgb, texColor.a * pc.opacity);
    }
}
#elif defined(SITUATION_USE_OPENGL)
layout(location = 2) uniform float u_opacity;
layout(location = 9) uniform int u_isIdle;
layout(location = 10) uniform int u_fallbackMode;
layout(location = 11) uniform float u_elapsedIdle;
layout(location = 12) uniform vec4 u_fallbackColor;
void main() {
    if (u_isIdle != 0) {
        vec3 rgb;
        if (u_fallbackMode == 1) {
            rgb = _sit_smpte_color_bars(v_texCoord);
        } else if (u_fallbackMode == 2) {
#if defined(SITUATION_GL_VD_COMPOSITOR_SPIRV)
            rgb = _sit_vd_idle_pattern_rgb(u_fallbackMode, v_texCoord, u_fallbackColor.rgb);
#else
            rgb = u_fallbackColor.rgb;
#endif
        } else {
            rgb = u_fallbackColor.rgb;
        }
        outColor = vec4(rgb, u_opacity);
    } else {
        vec4 texColor = texture(u_screenTexture, v_texCoord);
        outColor = vec4(texColor.rgb, texColor.a * u_opacity);
    }
}
#endif
