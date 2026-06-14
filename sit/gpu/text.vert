/**
 * @internal
 * @shader SIT_TEXT_VERTEX_SHADER
 * @brief Vertex stage for batched on-screen text rendering.
 * @details Screen-space glyph quads: six vertices per glyph (two triangles), one batched draw per string.
 *          Positions are pre-transformed; no model matrix. Pairs with text.frag.
 *
 * @var aPos (in) Screen-space vertex position at location 0.
 * @var aTexCoord (in) Glyph atlas UV at location 2.
 * @var v_TexCoord (out) Interpolated atlas coordinates.
 * @var ubo (uniform, Vulkan) View UBO set 0 binding 1 (projection only).
 * @var u_projection (uniform, OpenGL) Orthographic projection at location 4.
 *
 * @see situation_impl_renderer.h text draw path
 */
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 2) in vec2 aTexCoord;
layout(location = 0) out vec2 v_TexCoord;

#if defined(SITUATION_USE_VULKAN)
layout(set = 0, binding = 1) uniform UboView { mat4 view; mat4 projection; } ubo;
layout(push_constant) uniform TextPushConstants { vec4 color; } pc;
void main() {
    gl_Position = ubo.projection * vec4(aPos, 0.0, 1.0);
    v_TexCoord = aTexCoord;
}
#elif defined(SITUATION_USE_OPENGL)
layout(location = 4) uniform mat4 u_projection;
void main() {
    gl_Position = u_projection * vec4(aPos, 0.0, 1.0);
    v_TexCoord = aTexCoord;
}
#endif
