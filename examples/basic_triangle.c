#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "../situation.h"
#include <stdio.h>
#include <stdlib.h> // For malloc/free if needed
#include <string.h> // For memset
#include <stdint.h> // For uint32_t

// --- Vertex Data for a Simple Triangle ---
// Define a simple vertex structure (position + color)
typedef struct {
    float position[3]; // x, y, z
    float color[3];    // r, g, b
} Vertex;

// Vertex data for a triangle (using an index buffer now for best practice)
static const Vertex triangle_vertices[] = {
    {{ 0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // Top, Red
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // Bottom Left, Green
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}  // Bottom Right, Blue
};
static const uint32_t triangle_indices[] = { 0, 1, 2 };

// --- Simple Vertex and Fragment Shaders ---
static const char* vertex_shader_source =
"#version 450 core\n"
"layout(location = 0) in vec3 inPosition;\n"
"layout(location = 1) in vec3 inColor;\n"
"layout(location = 0) out vec3 fragColor;\n"
"void main() {\n"
"    gl_Position = vec4(inPosition, 1.0);\n"
"    fragColor = inColor;\n"
"}\n";

static const char* fragment_shader_source =
"#version 450 core\n"
"layout(location = 0) in vec3 fragColor;\n"
"out vec4 outColor;\n"
"void main() {\n"
"    outColor = vec4(fragColor, 1.0);\n"
"}\n";

// --- Global Handles ---
static SituationShader g_shader_pipeline = {0};
static SituationMesh g_triangle_mesh = {0};

// --- Initialization ---
int init_resources() {
    // 1. Create Shader Pipeline
    g_shader_pipeline = SituationLoadShaderFromMemory(vertex_shader_source, fragment_shader_source);
    if (g_shader_pipeline.id == 0) {
        fprintf(stderr, "Failed to load shader: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // 2. Create Mesh
    g_triangle_mesh = SituationCreateMesh(
        triangle_vertices,
        3, // vertex_count
        sizeof(Vertex), // vertex_stride
        triangle_indices,
        3 // index_count
    );
    if (g_triangle_mesh.id == 0) {
        fprintf(stderr, "Failed to create mesh: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    return 0; // Success
}

// --- Cleanup ---
void cleanup_resources() {
    if (g_triangle_mesh.id != 0) {
        SituationDestroyMesh(&g_triangle_mesh);
    }
    if (g_shader_pipeline.id != 0) {
        SituationUnloadShader(&g_shader_pipeline);
    }
}

// --- Main Rendering Function ---
void render_frame() {
    if (!SituationAcquireFrameCommandBuffer()) {
        return;
    }

    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });

    SituationCmdBindPipeline(cmd, g_shader_pipeline);

    SituationCmdDrawMesh(cmd, g_triangle_mesh);

    SituationCmdEndRender(cmd);

    SituationEndFrame();
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Basic Triangle";
    init_info.window_width = 800;
    init_info.window_height = 600;

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    if (init_resources() != 0) {
        SituationShutdown();
        return -1;
    }

    printf("Running... Close the window to quit.\n");

    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        render_frame();
    }

    cleanup_resources();
    SituationShutdown();
    return 0;
}
