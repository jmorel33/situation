/**
 * @internal
 * @shader SIT_QUAD_VERTEX_SHADER
 * @brief Vertex stage for internal textured/colored quads.
 * @details Unit quad corners (aPos 0..1). UV computed from uv_rect push/uniform.
 *          Pairs with quad.frag and ypq_grade.frag. Driven by SituationCmdDrawQuad,
 *          SituationCmdDrawTexture, SituationCmdDrawTextureYpqGrade.
 *
 * @var aPos (in) Quad corner position at location 0.
 * @var v_TexCoord (out) UV = uv_rect.xy + aPos * uv_rect.zw.
 * @var ubo (uniform, Vulkan) View UBO set 0 binding 1.
 * @var u_projection / u_model (uniform, OpenGL) Locations 4 and 0.
 * @var pc / u_uv_rect (uniform, VK / GL) UV rect and model via push block.
 *
 * @see situation_impl_renderer.h SituationCmdDrawQuad, _SituationInitQuadRenderer
 */
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 0) out vec2 v_TexCoord;

#if defined(SITUATION_USE_VULKAN)
layout(set = 0, binding = 1) uniform UboView { mat4 view; mat4 projection; } ubo;
layout(push_constant) uniform QuadPushConstants { mat4 model; vec4 color; vec4 uv_rect; uint texture_id; int use_texture; } pc;

void main() {
    gl_Position = ubo.projection * pc.model * vec4(aPos, 0.0, 1.0);
    v_TexCoord = pc.uv_rect.xy + (aPos * pc.uv_rect.zw);
}
#elif defined(SITUATION_USE_OPENGL)
layout(location = 4) uniform mat4 u_projection;
layout(location = 0) uniform mat4 u_model;
layout(location = 5) uniform vec4 u_uv_rect;

void main() {
    gl_Position = u_projection * u_model * vec4(aPos, 0.0, 1.0);
    v_TexCoord = u_uv_rect.xy + (aPos * u_uv_rect.zw);
}
#endif
