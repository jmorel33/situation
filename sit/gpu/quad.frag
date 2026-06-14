/**
 * @internal
 * @shader SIT_QUAD_FRAGMENT_SHADER
 * @brief Fragment stage for internal textured or solid quads.
 * @details Optional texture sample tinted by object color. OpenGL supports bindless
 *          sampler at location 7 when u_use_texture == 2. Pairs with quad.vert.
 *
 * @var v_TexCoord (in) Interpolated UV from vertex stage.
 * @var u_QuadTexture / u_Texture (uniform) Albedo sampler (VK: set 1 binding 0).
 * @var pc / u_objectColor (uniform, VK / GL) Tint color and use_texture flag.
 *
 * @see situation_impl_renderer.h SituationCmdDrawQuad, SituationCmdDrawTexture
 */
#version 450 core
#if defined(SITUATION_USE_OPENGL)
/* bindless_sampler requires BOTH extensions; enabling only bindless_texture breaks compile on partial drivers. */
#if defined(GL_ARB_bindless_texture) && defined(GL_ARB_gpu_shader_int64)
#extension GL_ARB_bindless_texture : enable
#extension GL_ARB_gpu_shader_int64 : enable
#endif
#endif
layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 outColor;

#if defined(SITUATION_USE_VULKAN)
layout(set = 1, binding = 0) uniform sampler2D u_QuadTexture;
layout(push_constant) uniform QuadPushConstants { mat4 model; vec4 color; vec4 uv_rect; uint texture_id; int use_texture; } pc;
void main() {
    vec4 texColor = vec4(1.0);
    if (pc.use_texture == 1) {
        texColor = texture(u_QuadTexture, v_TexCoord);
    }
    outColor = texColor * pc.color;
}
#elif defined(SITUATION_USE_OPENGL)
layout(location = 1) uniform vec4 u_objectColor;
layout(location = 6) uniform int u_use_texture;
uniform sampler2D u_Texture;
#if defined(GL_ARB_bindless_texture) && defined(GL_ARB_gpu_shader_int64)
layout(bindless_sampler, location = 7) uniform sampler2D u_TextureHandle;
#endif

void main() {
    vec4 texColor = vec4(1.0);
    if (u_use_texture == 1) {
#if defined(GL_ARB_bindless_texture) && defined(GL_ARB_gpu_shader_int64)
        if (u_use_texture == 2) texColor = texture(u_TextureHandle, v_TexCoord);
        else texColor = texture(u_Texture, v_TexCoord);
#else
        texColor = texture(u_Texture, v_TexCoord);
#endif
    }
    outColor = texColor * u_objectColor;
}
#endif
