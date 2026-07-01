/**
 * @internal
 * @shader SIT_COMPOSITOR_VERTEX_SHADER
 * @brief Shared vertex stage for Virtual Display and advanced compositing.
 * @details Merges the former SIT_VD_VERTEX_SHADER_SRC and SIT_COMPOSITE_VERTEX_SHADER_SRC.
 *          Pairs with vd.frag (path B) and composite.frag (path A). Uses vd_quad_vao
 *          (vec2 pos + vec2 uv, triangle strip). Vertex stage only reads model from push block.
 *
 * @var aPos (in) Unit-quad corner position (0..1 scaled by model matrix).
 * @var aTexCoords (in, OpenGL) UV from VBO; ignored on Vulkan (UV = aPos).
 * @var v_texCoord (out) Interpolated UV passed to fragment stage.
 * @var ubo (uniform, Vulkan) View UBO at set 0 binding 1.
 * @var u_projection / u_model (uniform, OpenGL) Locations 4 and 8 (8 avoids SPIR-V loc-0 vs aPos).
 *
 * @note Compile with -DSIT_COMPOSITOR_PATH_B for vd.frag (VDPushConstants, 104 B).
 * @note Compile with -DSIT_COMPOSITOR_PATH_A for composite.frag (CompositePushConstants, 120 B).
 * @see situation_impl_vd.h _SitVDFillPathBPushConstants, _SitVDFillPathAPushConstants
 * @see SIT_VD_PATH_B_PUSH_CONSTANT_SIZE, SIT_VD_PATH_A_PUSH_CONSTANT_SIZE
 */
#version 450 core
layout(location = 0) in vec2 aPos;
#if defined(SITUATION_USE_OPENGL)
layout(location = 2) in vec2 aTexCoords;
#endif
layout(location = 0) out vec2 v_texCoord;

#if defined(SITUATION_USE_VULKAN)
layout(set = 0, binding = 1) uniform UboView { mat4 view; mat4 projection; } ubo;
#if defined(SIT_COMPOSITOR_PATH_A)
layout(push_constant) uniform CompositePushConstants { mat4 model; int blendMode; float opacity;
    int is_idle;
    int fallback_mode;
    float elapsed_idle;
    vec4 fallback_color;
    uint texture_id;
} pc;
#elif defined(SIT_COMPOSITOR_PATH_B)
layout(push_constant) uniform VDPushConstants { mat4 model; float opacity;
    int is_idle;
    int fallback_mode;
    float elapsed_idle;
    vec4 fallback_color;
    uint texture_id;
} pc;
#else
layout(push_constant) uniform VDPushConstants { mat4 model; float opacity;
    int is_idle;
    int fallback_mode;
    float elapsed_idle;
    vec4 fallback_color;
    uint texture_id;
} pc;
#endif
void main() {
    gl_Position = ubo.projection * pc.model * vec4(aPos, 0.0, 1.0);
    v_texCoord = aPos;
}
#elif defined(SITUATION_USE_OPENGL)
layout(location = 4) uniform mat4 u_projection;
layout(location = 8) uniform mat4 u_model;
void main() {
    gl_Position = u_projection * u_model * vec4(aPos, 0.0, 1.0);
    v_texCoord = aTexCoords;
}
#endif
