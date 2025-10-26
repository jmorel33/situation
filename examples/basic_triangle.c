// Include situation.h, defining the implementation and graphics backend.
// This must be done in exactly one C or C++ file.
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "../situation.h"

// Standard C libraries for I/O, memory allocation, and types.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// --- Vertex Data for a Simple Triangle ---
// This struct defines the layout of our vertex data in memory.
// Each vertex has a 3D position (x, y, z) and a 3-component color (r, g, b).
typedef struct {
    float position[3];
    float color[3];
} Vertex;

// An array of vertices for our triangle.
// Using an index buffer is a best practice as it reduces data duplication
// for more complex models.
static const Vertex triangle_vertices[] = {
    {{ 0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // Top vertex, red
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // Bottom-left vertex, green
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}  // Bottom-right vertex, blue
};
// The index buffer tells the GPU the order in which to draw the vertices.
static const uint32_t triangle_indices[] = { 0, 1, 2 };

// --- Simple Vertex and Fragment Shaders ---
// GLSL shader source code, embedded as strings.
// The vertex shader is responsible for transforming vertex positions.
static const char* vertex_shader_source =
"#version 450 core\n"
"layout(location = 0) in vec3 inPosition;\n" // Corresponds to Vertex.position
"layout(location = 1) in vec3 inColor;\n"    // Corresponds to Vertex.color
"layout(location = 0) out vec3 fragColor;\n" // Pass color to the fragment shader
"void main() {\n"
"    gl_Position = vec4(inPosition, 1.0);\n" // Output the final vertex position
"    fragColor = inColor;\n"                   // Pass the color through
"}\n";

// The fragment shader determines the final color of each pixel.
static const char* fragment_shader_source =
"#version 450 core\n"
"layout(location = 0) in vec3 fragColor;\n" // Receive color from the vertex shader
"out vec4 outColor;\n"                      // The final output color of the pixel
"void main() {\n"
"    outColor = vec4(fragColor, 1.0);\n"    // Output the color with full alpha
"}\n";

// --- Global Handles ---
// These global variables will hold the handles to our GPU resources.
static SituationShader g_shader_pipeline = {0};
static SituationMesh g_triangle_mesh = {0};

// --- Initialization ---
// This function sets up all the necessary GPU resources.
int init_resources() {
    // 1. Create the shader pipeline from our GLSL source code.
    g_shader_pipeline = SituationLoadShaderFromMemory(vertex_shader_source, fragment_shader_source);
    if (g_shader_pipeline.id == 0) {
        fprintf(stderr, "Failed to load shader: %s\n", SituationGetLastErrorMsg());
        return -1; // Return -1 to indicate failure
    }

    // 2. Create the triangle mesh on the GPU.
    g_triangle_mesh = SituationCreateMesh(
        triangle_vertices, // Pointer to the vertex data
        3,                 // Number of vertices
        sizeof(Vertex),    // The size of a single vertex struct
        triangle_indices,  // Pointer to the index data
        3                  // Number of indices
    );
    if (g_triangle_mesh.id == 0) {
        fprintf(stderr, "Failed to create mesh: %s\n", SituationGetLastErrorMsg());
        return -1; // Return -1 to indicate failure
    }

    return 0; // Success
}

// --- Cleanup ---
// This function releases all the GPU resources that were allocated.
void cleanup_resources() {
    // It's good practice to check if the resource ID is non-zero before destroying.
    if (g_triangle_mesh.id != 0) {
        SituationDestroyMesh(&g_triangle_mesh);
    }
    if (g_shader_pipeline.id != 0) {
        SituationUnloadShader(&g_shader_pipeline);
    }
}

// --- Main Rendering Function ---
// This function is called every frame to draw the scene.
void render_frame() {
    // Prepare for a new frame. This is essential for synchronization.
    if (!SituationAcquireFrameCommandBuffer()) {
        return; // Skip rendering if we can't acquire the command buffer
    }

    // Get the main command buffer for recording rendering commands.
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // Begin a rendering pass, clearing the screen to a dark blue color.
    SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });

    // Bind our shader pipeline. All subsequent draw calls will use this shader.
    SituationCmdBindPipeline(cmd, g_shader_pipeline);

    // Issue the command to draw our triangle mesh.
    SituationCmdDrawMesh(cmd, g_triangle_mesh);

    // End the rendering pass.
    SituationCmdEndRender(cmd);

    // Finalize the frame and present it to the screen.
    SituationEndFrame();
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    // Initialize the situation.h library with desired window properties.
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Basic Triangle";
    init_info.window_width = 800;
    init_info.window_height = 600;

    // SituationInit creates the window and initializes the graphics context.
    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // Create our shaders and meshes.
    if (init_resources() != 0) {
        SituationShutdown(); // Clean up the library if resource creation fails
        return -1;
    }

    printf("Running... Close the window to quit.\n");

    // The main application loop.
    while (!SituationWindowShouldClose()) {
        // Process all pending window and input events.
        SituationPollInputEvents();

        // Update internal timers.
        SituationUpdateTimers();

        // Render the current frame.
        render_frame();
    }

    // Clean up all resources before exiting.
    cleanup_resources();
    SituationShutdown();

    return 0;
}
