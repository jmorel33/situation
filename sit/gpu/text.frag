/**
 * @internal
 * @shader SIT_TEXT_FRAGMENT_SHADER
 * @brief Fragment stage for batched glyph rendering from atlas texture.
 * @details Samples glyph atlas; alpha from max(tex.a, tex.r). Discards near-transparent pixels.
 *          OpenGL supports bindless sampler at location 7 when u_use_bindless == 1.
 *
 * @var v_TexCoord (in) Interpolated atlas UV from text.vert.
 * @var u_Texture (uniform) Glyph atlas sampler (VK: set 1 binding 0).
 * @var pc.color / u_color (uniform, VK / GL) Per-draw text color.
 *
 * @see situation_impl_renderer.h text draw path
 */
#version 450 core
#if defined(SITUATION_USE_OPENGL)
#if defined(GL_ARB_bindless_texture) && defined(GL_ARB_gpu_shader_int64)
#extension GL_ARB_bindless_texture : enable
#extension GL_ARB_gpu_shader_int64 : enable
#endif
#endif
layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 outColor;

#if defined(SITUATION_USE_VULKAN)
layout(set = 1, binding = 0) uniform sampler2D u_Texture;
layout(push_constant) uniform TextPushConstants { vec4 color; } pc;
void main() {
    vec4 texColor = texture(u_Texture, v_TexCoord);
    float glyphAlpha = max(texColor.a, texColor.r);
    if (glyphAlpha < 0.01) discard;
    outColor = vec4(pc.color.rgb, pc.color.a * glyphAlpha);
}
#elif defined(SITUATION_USE_OPENGL)
layout(binding = 0) uniform sampler2D u_Texture;

layout(location = 1) uniform vec4 u_color;
layout(location = 6) uniform int u_use_bindless;
#if defined(GL_ARB_bindless_texture) && defined(GL_ARB_gpu_shader_int64)
layout(bindless_sampler, location = 7) uniform sampler2D u_TextureHandle;
#endif

void main() {
    vec4 texColor;
#if defined(GL_ARB_bindless_texture) && defined(GL_ARB_gpu_shader_int64)
    if (u_use_bindless == 1) texColor = texture(u_TextureHandle, v_TexCoord);
    else texColor = texture(u_Texture, v_TexCoord);
#else
    texColor = texture(u_Texture, v_TexCoord);
#endif
    float glyphAlpha = max(texColor.a, texColor.r);
    if (glyphAlpha < 0.01) discard;
    outColor = vec4(u_color.rgb, u_color.a * glyphAlpha);
}
#endif
