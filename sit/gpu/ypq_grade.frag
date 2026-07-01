/**
 * @internal
 * @shader SIT_YPQ_GRADE_SHADER
 * @brief Internal fullscreen textured draw with NTSC YPQ grade (matches SituationImageAdjustYPQ).
 * @details Reuses quad.vert. Sample source RGB, convert YIQ→YPQ, apply luma/phase/chroma/mix, clamp RGB out.
 *          Matrix constants must stay in sync with sit/situation_impl_color.h (SIT_YIQ_NTSC_*).
 *
 * @var v_TexCoord (in) Interpolated UV from quad.vert.
 * @var u_QuadTexture / u_Texture (uniform) Source sampler (VK: set 1 binding 0).
 * @var pc / grade uniforms (uniform, VK / GL) YPQ grade parameters and tint color.
 *
 * @see situation_impl_renderer.h SituationCmdDrawTextureYpqGrade
 */
#version 450 core
layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 outColor;

#if defined(SITUATION_USE_VULKAN)
layout(set = 1, binding = 0) uniform sampler2D u_QuadTexture;
layout(push_constant) uniform YpqGradePushConstants {
    mat4 projection;
    mat4 model;
    vec4 color;
    vec4 uv_rect;
    uint texture_id;
    int use_texture;
    float phase_shift_deg;
    float chroma_factor;
    float luma_factor;
    float mix;
} pc;
#elif defined(SITUATION_USE_OPENGL)
layout(location = 1) uniform vec4 u_objectColor;
layout(location = 6) uniform int u_use_texture;
uniform sampler2D u_Texture;
uniform float u_phase_shift_deg;
uniform float u_chroma_factor;
uniform float u_luma_factor;
uniform float u_mix;
#endif

const float SIT_YIQ_MAX_I = 0.595715671472;
const float SIT_YIQ_MAX_Q = 0.522591049541;
const float SIT_YIQ_INV_MAX_I = 1.6783189601;
const float SIT_YIQ_INV_MAX_Q = 1.9135436530;
const float SIT_YIQ_TAU = 6.28318530718;
const float SIT_YIQ_INV_TAU = 0.1591549431;

vec3 sit_rgb_to_yiq(vec3 rgb) {
    return vec3(
        dot(rgb, vec3(0.299, 0.587, 0.114)),
        dot(rgb, vec3(0.596, -0.274, -0.322)),
        dot(rgb, vec3(0.211, -0.523, 0.312)));
}
vec3 sit_yiq_to_rgb_clamped(vec3 yiq) {
    vec3 rgb = vec3(
        yiq.x + 0.95568806036115671171 * yiq.y + 0.62082467141531188082 * yiq.z,
        yiq.x - 0.27178838506206335708 * yiq.y - 0.64860590248778682744 * yiq.z,
        yiq.x - 1.1081773266826619523 * yiq.y + 1.7025019884020956631 * yiq.z);
    return clamp(rgb, 0.0, 1.0);
}
vec3 sit_rgb_from_ypq(float y, float p, float q) {
    float ang = p * SIT_YIQ_TAU;
    vec3 yiq = vec3(y, q * cos(ang) * SIT_YIQ_MAX_I, q * sin(ang) * SIT_YIQ_MAX_Q);
    return sit_yiq_to_rgb_clamped(yiq);
}
vec3 sit_apply_ypq_grade(vec3 rgb, float phase_shift_deg, float chroma_factor, float luma_factor, float mix_amt) {
    vec3 yiq = sit_rgb_to_yiq(rgb);
    float i_norm = yiq.y * SIT_YIQ_INV_MAX_I;
    float q_norm = yiq.z * SIT_YIQ_INV_MAX_Q;
    float amp = min(length(vec2(i_norm, q_norm)), 1.0);
    float ang = atan(q_norm, i_norm);
    if (ang < 0.0) ang += SIT_YIQ_TAU;
    float y_pq = clamp(yiq.x * luma_factor, 0.0, 1.0);
    float p_pq = fract(ang * SIT_YIQ_INV_TAU + phase_shift_deg / 360.0);
    float q_pq = clamp(amp * chroma_factor, 0.0, 1.0);
    vec3 adjusted = sit_rgb_from_ypq(y_pq, p_pq, q_pq);
    return mix(rgb, adjusted, mix_amt);
}
void main() {
#if defined(SITUATION_USE_VULKAN)
    vec4 src = (pc.use_texture == 1) ? texture(u_QuadTexture, v_TexCoord) : vec4(1.0);
    vec3 graded = sit_apply_ypq_grade(src.rgb, pc.phase_shift_deg, pc.chroma_factor, pc.luma_factor, pc.mix);
    outColor = vec4(graded, src.a) * pc.color;
#elif defined(SITUATION_USE_OPENGL)
    vec4 src = (u_use_texture == 1) ? texture(u_Texture, v_TexCoord) : vec4(1.0);
    vec3 graded = sit_apply_ypq_grade(src.rgb, u_phase_shift_deg, u_chroma_factor, u_luma_factor, u_mix);
    outColor = vec4(graded, src.a) * u_objectColor;
#endif
}
