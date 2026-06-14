/**
 * @internal
 * @shader SIT_VD_FRAGMENT_SHADER
 * @brief Fragment stage for simple Virtual Display compositing.
 * @details Samples the VD offscreen texture or renders idle SMPTE/solid fallback.
 *          Driven by SituationRenderVirtualDisplays path B. Pairs with compositor.vert
 *          (compile vert with -DSIT_COMPOSITOR_PATH_B).
 *
 * @var v_texCoord (in) Interpolated UV; VK uses position-as-UV from vert, GL uses atlas coords.
 * @var u_screenTexture (uniform) VD source sampler (VK: set 1 binding 4).
 * @var pc / u_opacity (uniform, VK / GL) Per-draw opacity and idle fallback fields.
 *
 * @see situation_impl_vd.h _SitVDFillPathBPushConstants
 * @see SIT_VD_PATH_B_PUSH_CONSTANT_SIZE
 */
#version 450 core
layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 outColor;

#if defined(SITUATION_USE_VULKAN)
layout(set = 1, binding = 4) uniform sampler2D u_screenTexture;
#elif defined(SITUATION_USE_OPENGL)
uniform sampler2D u_screenTexture;
#endif

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
layout(push_constant) uniform VDPushConstants { mat4 model; float opacity;
    int is_idle;
    int fallback_mode;
    float elapsed_idle;
    vec4 fallback_color;
} pc;
void main() {
    if (pc.is_idle != 0) {
        vec3 rgb;
        int fallback_mode = pc.fallback_mode;
        float elapsed_idle = pc.elapsed_idle;
        vec4 fallback_color = pc.fallback_color;
        if (fallback_mode == 1) {
            rgb = _sit_smpte_color_bars(v_texCoord);
        } else {
            rgb = fallback_color.rgb;
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
        int fallback_mode = u_fallbackMode;
        float elapsed_idle = u_elapsedIdle;
        vec4 fallback_color = u_fallbackColor;
        if (fallback_mode == 1) {
            rgb = _sit_smpte_color_bars(v_texCoord);
        } else {
            rgb = fallback_color.rgb;
        }
        outColor = vec4(rgb, u_opacity);
    } else {
        vec4 texColor = texture(u_screenTexture, v_texCoord);
        outColor = vec4(texColor.rgb, texColor.a * u_opacity);
    }
}
#endif
