/**
 * @internal
 * @shader SIT_COMPOSITE_FRAGMENT_SHADER
 * @brief Advanced Virtual Display compositing with Photoshop-style blend modes.
 * @details Reads VD source and destination (framebuffer copy) textures. Path A push layout (112 B).
 *          Pairs with compositor.vert (compile vert with -DSIT_COMPOSITOR_PATH_A).
 *
 * @var u_sourceTexture (uniform) VD source (VK: set 1 binding 4).
 * @var u_destinationTexture (uniform) Screen copy behind VD (VK: set 2 binding 5).
 * @var pc.blendMode / u_blendMode (uniform, VK / GL) Blend mode selector.
 *
 * @see situation_impl_vd.h _SitVDFillPathAPushConstants
 * @see SIT_VD_PATH_A_PUSH_CONSTANT_SIZE
 */
#version 450 core
layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 outColor;

#if defined(SITUATION_USE_VULKAN)
layout(set = 1, binding = 4) uniform sampler2D u_sourceTexture;
layout(set = 2, binding = 5) uniform sampler2D u_destinationTexture;
#elif defined(SITUATION_USE_OPENGL)
uniform sampler2D u_sourceTexture;
uniform sampler2D u_destinationTexture;
#endif

float overlay(float b, float l) { return (b < 0.5) ? (2.0*b*l) : (1.0 - 2.0*(1.0-b)*(1.0-l)); }
float softlight(float b, float l) { return (l < 0.5) ? (b - (1.0 - 2.0 * l) * b * (1.0 - b)) : (b + (2.0 * l - 1.0) * (((b <= 0.25) ? (((16.0 * b - 12.0) * b + 4.0) * b) : sqrt(b)) - b)); }

vec3 _sit_smpte75(int bar) {
    if (bar == 0) return vec3(0.706);
    if (bar == 1) return vec3(0.706, 0.706, 0.0);
    if (bar == 2) return vec3(0.0, 0.706, 0.706);
    if (bar == 3) return vec3(0.0, 0.706, 0.0);
    if (bar == 4) return vec3(0.706, 0.0, 0.706);
    if (bar == 5) return vec3(0.706, 0.0, 0.0);
    return vec3(0.0, 0.0, 0.706);
}
vec3 _sit_smpte_castellation(int col) {
    if (col == 0) return vec3(0.0, 0.0, 0.706);
    if (col == 1) return vec3(0.063);
    if (col == 2) return vec3(0.706, 0.0, 0.706);
    if (col == 3) return vec3(0.063);
    if (col == 4) return vec3(0.0, 0.706, 0.706);
    if (col == 5) return vec3(0.063);
    return vec3(0.706);
}
vec3 _sit_smpte_bottom(vec2 uv) {
    float u = clamp(uv.x, 0.0, 1.0);
    const float pw = 5.0 / 28.0;
    const float plw = 1.0 / 21.0;
    if (u < pw) return vec3(0.063, 0.275, 0.416);
    if (u < 2.0 * pw) return vec3(0.922);
    if (u < 3.0 * pw) return vec3(0.282, 0.063, 0.463);
    if (u < 5.0 / 7.0) return vec3(0.063);
    if (u < 5.0 / 7.0 + plw) return vec3(0.027);
    if (u < 5.0 / 7.0 + 2.0 * plw) return vec3(0.063);
    if (u < 5.0 / 7.0 + 3.0 * plw) return vec3(0.094);
    return vec3(0.063);
}
vec3 _sit_smpte_color_bars(vec2 uv) {
    float u = clamp(uv.x, 0.0, 0.999999);
#if defined(SITUATION_USE_VULKAN)
    float y = clamp(uv.y, 0.0, 1.0);
#else
    float y = clamp(1.0 - uv.y, 0.0, 1.0);
#endif
    int col = clamp(int(floor(u * 7.0)), 0, 6);
    if (y < (2.0 / 3.0)) {
        return _sit_smpte75(col);
    }
    if (y < 0.75) {
        return _sit_smpte_castellation(col);
    }
    return _sit_smpte_bottom(uv);
}

#if defined(SITUATION_USE_VULKAN)
layout(push_constant) uniform CompositePushConstants { mat4 model; int blendMode; float opacity;
    int is_idle;
    int fallback_mode;
    float elapsed_idle;
    vec4 fallback_color;
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
    float elapsed_idle;
    vec4 fallback_color;
#if defined(SITUATION_USE_VULKAN)
    blendMode = pc.blendMode;
    opacity = pc.opacity;
    is_idle = pc.is_idle;
    fallback_mode = pc.fallback_mode;
    elapsed_idle = pc.elapsed_idle;
    fallback_color = pc.fallback_color;
#else
    blendMode = u_blendMode;
    opacity = u_opacity;
    is_idle = u_isIdle;
    fallback_mode = u_fallbackMode;
    elapsed_idle = u_elapsedIdle;
    fallback_color = u_fallbackColor;
#endif

    vec3 srcRgb;
    float srcAlpha;
    if (is_idle != 0) {
        if (fallback_mode == 1) {
            srcRgb = _sit_smpte_color_bars(v_texCoord);
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
