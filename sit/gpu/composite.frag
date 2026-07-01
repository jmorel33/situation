/**
 * @internal
 * @shader SIT_COMPOSITE_FRAGMENT_SHADER
 * @brief Advanced Virtual Display compositing with Photoshop-style blend modes.
 */
#version 450 core
layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 outColor;

#if defined(SITUATION_USE_VULKAN)
layout(set = 1, binding = 0) uniform sampler2D u_sourceTexture;
layout(set = 2, binding = 5) uniform sampler2D u_destinationTexture;
#elif defined(SITUATION_USE_OPENGL)
layout(location = 1, binding = 4) uniform sampler2D u_sourceTexture;
layout(location = 7, binding = 5) uniform sampler2D u_destinationTexture;
#endif

float overlay(float b, float l) { return (b < 0.5) ? (2.0*b*l) : (1.0 - 2.0*(1.0-b)*(1.0-l)); }
float softlight(float b, float l) { return (l < 0.5) ? (b - (1.0 - 2.0 * l) * b * (1.0 - b)) : (b + (2.0 * l - 1.0) * (((b <= 0.25) ? (((16.0 * b - 12.0) * b + 4.0) * b) : sqrt(b)) - b)); }

#include "sit/gpu/vd_colorburst_subset.glslh"
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_GL_VD_COMPOSITOR_SPIRV)
#if defined(SITUATION_USE_VULKAN)
#define SIT_TP_CONFIG_UBO_SET 3
#define SIT_TP_CONFIG_UBO_BINDING 0
#define SIT_TP_LAYER_PARAMS_SSBO_SET 3
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
layout(push_constant) uniform CompositePushConstants { mat4 model; int blendMode; float opacity;
    int is_idle;
    int fallback_mode;
    float elapsed_idle;
    vec4 fallback_color;
    uint texture_id;
} pc;
#elif defined(SITUATION_USE_OPENGL)
layout(location = 3) uniform int u_blendMode;
layout(location = 2) uniform float u_opacity;
layout(location = 9) uniform int u_isIdle;
layout(location = 10) uniform int u_fallbackMode;
layout(location = 11) uniform float u_elapsedIdle;
layout(location = 12) uniform vec4 u_fallbackColor;
#endif

void main() {
    int blendMode;
    float opacity;
    int is_idle;
    int fallback_mode;
    vec4 fallback_color;
#if defined(SITUATION_USE_VULKAN)
    blendMode = pc.blendMode;
    opacity = pc.opacity;
    is_idle = pc.is_idle;
    fallback_mode = pc.fallback_mode;
    fallback_color = pc.fallback_color;
#else
    blendMode = u_blendMode;
    opacity = u_opacity;
    is_idle = u_isIdle;
    fallback_mode = u_fallbackMode;
    fallback_color = u_fallbackColor;
#endif

    vec3 srcRgb;
    float srcAlpha;
    if (is_idle != 0) {
        if (fallback_mode == 1) {
            srcRgb = _sit_smpte_color_bars(v_texCoord);
        } else if (fallback_mode == 2) {
#if defined(SITUATION_USE_VULKAN) || defined(SITUATION_GL_VD_COMPOSITOR_SPIRV)
            srcRgb = _sit_vd_idle_pattern_rgb(fallback_mode, v_texCoord, fallback_color.rgb);
#else
            srcRgb = fallback_color.rgb;
#endif
        } else {
            srcRgb = fallback_color.rgb;
        }
        srcAlpha = opacity;
    } else {
        vec4 tex = texture(u_sourceTexture, v_texCoord);
        srcRgb = tex.rgb;
        srcAlpha = tex.a * opacity;
    }

    vec3 dst = texture(u_destinationTexture, gl_FragCoord.xy / textureSize(u_destinationTexture, 0)).rgb;
    vec3 res;

    switch (blendMode) {
        case 0:  res = srcRgb; break;
        case 1:  res = srcRgb + dst; break;
        case 2:  res = srcRgb * dst; break;
        case 3:  res = 1.0 - (1.0 - srcRgb) * (1.0 - dst); break;
        case 5:  res = vec3(overlay(dst.r, srcRgb.r), overlay(dst.g, srcRgb.g), overlay(dst.b, srcRgb.b)); break;
        case 6:  res = vec3(softlight(dst.r, srcRgb.r), softlight(dst.g, srcRgb.g), softlight(dst.b, srcRgb.b)); break;
        case 7:  res = vec3(overlay(srcRgb.r, dst.r), overlay(srcRgb.g, dst.g), overlay(srcRgb.b, dst.b)); break;
        case 8:  res = dst / (1.0 - min(vec3(0.9999), srcRgb)); break;
        case 9:  res = 1.0 - (1.0 - dst) / max(vec3(0.0001), srcRgb); break;
        case 10: res = min(dst, srcRgb); break;
        case 11: res = max(dst, srcRgb); break;
        case 12: res = abs(dst - srcRgb); break;
        case 13: res = dst + srcRgb - 2.0 * dst * srcRgb; break;
        default: res = srcRgb; break;
    }

    outColor = vec4(mix(dst.rgb, res, srcAlpha), 1.0);
}
