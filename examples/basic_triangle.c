/***************************************************************************************************
*   Situation Library - Example: Interactive Basic Triangle
*   -----------------------------------------------------
*   This example demonstrates the "Low Level" API with interaction.
*   Unlike the Quad example, here WE write the shader and WE manage the data.
*
*   Key Concepts:
*   1. GLSL Uniforms: Adding variables (uOffset) to a custom shader.
*   2. SituationSetShaderUniform: Sending CPU data to the GPU.
*   3. Manual Geometry: Defining the triangle shape explicitly.
*
*   Controls:
*   - ARROW KEYS: Move the triangle.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL
#include "situation.h"

// --- 1. The Data ---
// We define the shape manually.
typedef struct {
    float position[3];
    float color[3];
} Vertex;

static const Vertex triangle_vertices[] = {
    {{ 0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // Top Red
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // Left Green
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}  // Right Blue
};
static const uint32_t triangle_indices[] = { 0, 1, 2 };

// --- 2. The Shaders (Modified for Interaction) ---
static const char* vertex_shader_src =
    "#version 450 core\n"
    "layout(location = 0) in vec3 inPos;\n"
    "layout(location = 1) in vec3 inColor;\n"
    
    "// [NEW] A Uniform is a global variable we set from C code\n"
    "uniform vec2 uOffset;\n" 

    "layout(location = 0) out vec3 fragColor;\n"
    
    "void main() {\n"
    "    // Add the offset to the position\n"
    "    vec3 finalPos = inPos + vec3(uOffset, 0.0);\n" 
    "    gl_Position = vec4(finalPos, 1.0);\n"
    "    fragColor = inColor;\n"
    "}\n";

static const char* fragment_shader_src =
    "#version 450 core\n"
    "layout(location = 0) in vec3 fragColor;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    outColor = vec4(fragColor, 1.0);\n"
    "}\n";

// --- Global State ---
static SituationShader g_pipeline = {0};
static SituationMesh g_mesh = {0};
static vec2 g_triangle_pos = {0.0f, 0.0f}; // CPU-side state

// --- Setup ---
int init_resources() {
    g_pipeline = SituationLoadShaderFromMemory(vertex_shader_src, fragment_shader_src);
    if (g_pipeline.id == 0) {
        char* err = SituationGetLastErrorMsg();
        fprintf(stderr, "%s\n", err);
        free(err);
        return -1;
    }

    g_mesh = SituationCreateMesh(triangle_vertices, 3, sizeof(Vertex), triangle_indices, 3);
    return (g_mesh.id != 0) ? 0 : -1;
}

// --- Render Loop ---
void render_frame() {
    if (!SituationAcquireFrameCommandBuffer()) return;
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationRenderPassInfo pass = {
        .display_id = -1,
        .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = { 20, 20, 30, 255 } } }
    };

    SituationCmdBeginRenderPass(cmd, &pass);
    
    // 1. Bind the pipeline (Load the shader program)
    SituationCmdBindPipeline(cmd, g_pipeline);

    // 2. [NEW] Update the Uniform
    // We send our C variable 'g_triangle_pos' to the GLSL variable 'uOffset'
    // Note: In OpenGL this happens immediately.
    SituationSetShaderUniform(g_pipeline, "uOffset", &g_triangle_pos, SIT_UNIFORM_VEC2);

    // 3. Draw the mesh
    SituationCmdDrawMesh(cmd, g_mesh);
    
    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}

int main(int argc, char** argv) {
    SituationInitInfo config = {0};
    config.window_title = "Situation - Interactive Triangle";
    config.window_width = 800;
    config.window_height = 600;

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;
    if (init_resources() != 0) return -1;

    printf("Controls: ARROW KEYS to move the triangle.\n");

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();

        // --- Input Logic ---
        float speed = 1.5f * SituationGetFrameTime();
        if (SituationIsKeyDown(SIT_KEY_LEFT))  g_triangle_pos[0] -= speed;
        if (SituationIsKeyDown(SIT_KEY_RIGHT)) g_triangle_pos[0] += speed;
        if (SituationIsKeyDown(SIT_KEY_UP))    g_triangle_pos[1] += speed;
        if (SituationIsKeyDown(SIT_KEY_DOWN))  g_triangle_pos[1] -= speed;

        render_frame();
    }

    if (g_mesh.id) SituationDestroyMesh(&g_mesh);
    if (g_pipeline.id) SituationUnloadShader(&g_pipeline);
    SituationShutdown();
    return 0;
}