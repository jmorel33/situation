/***************************************************************************************************
 *  Situation — Vertex Pull Triangle (Phase B′ reference example)
 *
 *  Demonstrates rendering a mesh by fetching vertices from the mesh vertex buffer device
 *  address (BDA) in the vertex shader — no fixed-function attribute fetch on the pull path.
 *
 *  Vulkan only (requires SIT_FEATURE_BINDLESS_BUFFERS). On OpenGL the example falls back to
 *  the standard VAO passthrough path and prints a note once at startup.
 *
 *  Build:
 *    build\build_examples.bat static-vulkan  vertex_pull_triangle
 ***************************************************************************************************/

#if !defined(SITUATION_USE_OPENGL) && !defined(SITUATION_USE_VULKAN)
#define SITUATION_USE_VULKAN
#endif

#include "shared/sit_example.h"
#include <stdio.h>
#include <string.h>

#if defined(SITUATION_USE_VULKAN)
static const char* k_vs_vertex_pull =
    "#version 450\n"
    "#extension GL_EXT_buffer_reference : require\n"
    "#extension GL_EXT_buffer_reference2 : require\n"
    "#extension GL_EXT_scalar_block_layout : require\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(push_constant) uniform PC { uint64_t vertex_address; } pc;\n"
    "layout(buffer_reference, scalar) readonly buffer SitVertexBuffer_Simple { vec3 data[]; };\n"
    "void main() {\n"
    "    SitVertexBuffer_Simple verts = SitVertexBuffer_Simple(pc.vertex_address);\n"
    "    vec3 pos = verts.data[gl_VertexIndex];\n"
    "    gl_Position = vec4(pos + aPos * 0.0, 1.0);\n"
    "}\n";

static const char* k_fs_solid_red =
    "#version 450\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";
#else
static const char* k_vs_passthrough =
    "#version 460 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "void main() { gl_Position = vec4(aPos, 1.0); }\n";

static const char* k_fs_solid_red =
    "#version 460 core\n"
    "layout(location=0) out vec4 fragColor;\n"
    "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";
#endif

static SituationMesh s_mesh = {0};
static SituationShader s_shader = {0};
static bool s_use_vertex_pull = false;

static SituationError example_create_mesh(void) {
    float vertices[] = {
        -0.8f, -0.6f, 0.0f,
         0.8f, -0.6f, 0.0f,
         0.0f,  0.7f, 0.0f
    };
    uint32_t indices[] = {0, 1, 2};
    return SituationCreateMeshEx(
        vertices, 3, sizeof(float) * 3, indices, 3, SIT_MESH_LAYOUT_PULL, &s_mesh);
}

static SituationError example_load_shaders(void) {
#if defined(SITUATION_USE_VULKAN)
    if (SituationIsFeatureSupported(SIT_FEATURE_BINDLESS_BUFFERS)) {
        if (SituationGetMeshVertexBufferAddress(s_mesh) != 0) {
            s_use_vertex_pull = true;
            return SituationLoadShaderFromMemory(k_vs_vertex_pull, k_fs_solid_red, &s_shader);
        }
    }
    fprintf(stderr,
        "[vertex_pull_triangle] BDA unavailable — falling back to VAO passthrough.\n");
#endif
    s_use_vertex_pull = false;
#if defined(SITUATION_USE_VULKAN)
    return SituationLoadShaderFromMemory(
        "#version 450\n"
        "layout(location=0) in vec3 aPos;\n"
        "void main() { gl_Position = vec4(aPos, 1.0); }\n",
        k_fs_solid_red, &s_shader);
#else
    return SituationLoadShaderFromMemory(k_vs_passthrough, k_fs_solid_red, &s_shader);
#endif
}

int main(int argc, char** argv) {
    if (SitExample_Init(argc, argv, "Vertex Pull Triangle") != SITUATION_SUCCESS) {
        return -1;
    }

    if (example_create_mesh() != SITUATION_SUCCESS) {
        fprintf(stderr, "[vertex_pull_triangle] SituationCreateMesh failed\n");
        SituationShutdown();
        return -1;
    }
    if (example_load_shaders() != SITUATION_SUCCESS) {
        fprintf(stderr, "[vertex_pull_triangle] shader load failed\n");
        SituationDestroyMesh(&s_mesh);
        SituationShutdown();
        return -1;
    }

    printf("Backend: %s — draw path: %s\n",
           SituationGetGraphicsBackendName(),
           s_use_vertex_pull ? "vertex pull (mesh BDA)" : "VAO passthrough");

    const ColorRGBA bg = {8, 10, 18, 255};

    while (!SituationWindowShouldClose()) {
        if (SitExample_BeginFrame()) {
            break;
        }
        if (SituationIsAppPaused()) {
            continue;
        }

        if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
            continue;
        }
        SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

        SituationRenderPassInfo rp = {0};
        rp.display_id = -1;
        rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
        rp.color_attachment.clear.color = bg;
        rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
        rp.depth_attachment.clear.depth = 1.0f;

        SituationCmdBeginRenderPass(cmd, &rp);
        SituationCmdBindPipeline(cmd, s_shader);
        if (s_use_vertex_pull) {
            SituationCmdBindMeshPullBuffers(cmd, s_mesh);
        }
        SituationCmdDrawMesh(cmd, s_mesh);

        SitExample_DrawHUD(cmd,
            "Vertex Pull Triangle",
            s_use_vertex_pull
                ? "Pull path: BDA + buffer_reference (see sit/gpu/vertex_pull.glslh)"
                : "VAO fallback — Vulkan BDA not available on this device");

        SituationCmdEndRenderPass(cmd);
        SitExample_EndFrame();
    }

    SituationUnloadShader(&s_shader);
    SituationDestroyMesh(&s_mesh);
    SitExample_Shutdown();
    return 0;
}
