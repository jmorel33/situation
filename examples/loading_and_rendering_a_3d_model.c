/*
 * Loading and Rendering a 3D Model
 *
 * This example demonstrates the structure for loading and rendering a 3D model.
 * It relies on an external model loading function, which is provided here as a stub.
 * In a real application, you would implement this using a library like tinyobjloader-c.
 */

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "../situation.h"

// Standard C libraries and cglm for math.
#include <stdio.h>
#include <cglm/cglm.h>

// --- NOTE ---
// The `SituationModel` and related functions (`SituationLoadModelFromObj`, `SituationUnloadModel`)
// are conceptual. They are not part of the core situation.h API but represent how one might
// structure a model loading system that integrates with it.
// To make this example work, you would need to:
// 1. Choose a model loading library (e.g., tinyobjloader-c, assimp).
// 2. Implement `SituationLoadModelFromObj` to parse the model file, create `SituationMesh`
//    and `SituationTexture` objects, and populate a `SituationModel` struct.
// 3. Implement `SituationUnloadModel` to clean up these resources.

// --- Stub for Model Loading ---
// This placeholder function demonstrates the expected signature. It returns an empty model.
SituationModel SituationLoadModelFromObj(const char* path) {
    fprintf(stderr, "Warning: `SituationLoadModelFromObj` is a stub and does not load a real model.\n");
    SituationModel model = {0}; // Zero-initialize the struct.
    return model;
}

// --- Global Handles ---
static SituationShader g_model_shader = {0};   // Shader for rendering the model
static SituationModel g_loaded_model = {0};   // The loaded model data (meshes, textures)
static SituationTexture g_diffuse_texture = {0}; // The model's primary texture

// --- GLSL Shaders for a Textured Model ---
// This vertex shader handles Model-View-Projection (MVP) transformation and passes
// normals and texture coordinates to the fragment shader.
static const char* model_vertex_shader =
"#version 450 core\n"
"// Vertex attributes\n"
"layout(location = 0) in vec3 inPosition;\n"
"layout(location = 1) in vec3 inNormal;\n"
"layout(location = 2) in vec2 inTexCoord;\n"
"// Outputs to fragment shader\n"
"layout(location = 0) out vec2 fragTexCoord;\n"
"layout(location = 1) out vec3 fragNormal;\n"
"layout(location = 2) out vec3 fragWorldPos;\n"
"// Uniform Buffer Object for view and projection matrices\n"
"layout(std140, binding = 0) uniform ViewData {\n"
"    mat4 uView;\n"
"    mat4 uProjection;\n"
"};\n"
"// Push constants for the per-object model matrix\n"
"layout(push_constant) uniform ModelData {\n"
"    mat4 uModel;\n"
"};\n"
"void main() {\n"
"    vec4 worldPos = uModel * vec4(inPosition, 1.0);\n"
"    gl_Position = uProjection * uView * worldPos;\n"
"    // Pass data to the fragment shader\n"
"    fragTexCoord = inTexCoord;\n"
"    fragNormal = mat3(transpose(inverse(uModel))) * inNormal; // For correct lighting on non-uniformly scaled models\n"
"    fragWorldPos = worldPos.xyz;\n"
"}\n";

// This fragment shader applies a diffuse texture and simple directional lighting.
static const char* model_fragment_shader =
"#version 450 core\n"
"// Inputs from vertex shader\n"
"layout(location = 0) in vec2 fragTexCoord;\n"
"layout(location = 1) in vec3 fragNormal;\n"
"layout(location = 2) in vec3 fragWorldPos;\n"
"// Output color\n"
"layout(location = 0) out vec4 outColor;\n"
"// The diffuse texture sampler\n"
"layout(binding = 1) uniform sampler2D uDiffuseTexture;\n"
"void main() {\n"
"    // Simple directional light from a fixed direction\n"
"    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));\n"
"    vec3 norm = normalize(fragNormal);\n"
"    // Calculate diffuse lighting factor (with a minimum ambient term)\n"
"    float diff = max(dot(norm, lightDir), 0.2);\n"
"    // Sample the texture\n"
"    vec4 texColor = texture(uDiffuseTexture, fragTexCoord);\n"
"    // Combine lighting and texture color\n"
"    outColor = vec4(diff * texColor.rgb, texColor.a);\n"
"}\n";

// --- Initialization ---
// Loads the shader and the 3D model.
int init_model_resources() {
    // Load the GLSL shaders into a pipeline.
    g_model_shader = SituationLoadShaderFromMemory(model_vertex_shader, model_fragment_shader);
    if (g_model_shader.id == 0) {
        fprintf(stderr, "Failed to load model shader: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // Load the model from a file (using our stub function).
    const char* model_path = "assets/models/cube.obj";
    g_loaded_model = SituationLoadModelFromObj(model_path);
    if (g_loaded_model.id == 0 || g_loaded_model.mesh_count == 0) {
         fprintf(stderr, "Failed to load model from %s: %s\n", model_path, SituationGetLastErrorMsg());
         return -1;
    }
    // If the model has textures, grab the first one to use for rendering.
    if (g_loaded_model.texture_count > 0) {
        g_diffuse_texture = g_loaded_model.all_model_textures[0];
    }

    return 0; // Success
}

// --- Cleanup ---
// Releases all allocated resources.
void cleanup_model_resources() {
     if (g_loaded_model.id != 0) {
         SituationUnloadModel(&g_loaded_model); // This would free meshes and textures.
     }
     if (g_model_shader.id != 0) {
         SituationUnloadShader(&g_model_shader);
     }
}

// --- Rendering the Model ---
// This function is called every frame to draw the model.
void render_model() {
     if (!SituationAcquireFrameCommandBuffer()) return;
     SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

     SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });

     // Bind the graphics pipeline for the model.
     SituationCmdBindPipeline(cmd, g_model_shader);

     // Set up camera matrices (View and Projection).
     mat4 view, projection, model;
     glm_mat4_identity(view);
     glm_translate(view, (vec3){0.0f, 0.0f, -3.0f}); // Move camera back
     glm_perspective(glm_rad(45.0f), 1024.0f / 768.0f, 0.1f, 100.0f, projection);

     // Set up the model matrix (position, rotation, scale of the object).
     glm_mat4_identity(model);
     // TODO: Here you could rotate the model over time.
     // glm_rotate(model, (float)SituationGetTime(), (vec3){0.0f, 1.0f, 0.0f});

     // Send the model matrix to the shader via push constants.
     SituationCmdSetPushConstant(cmd, 0, &model, sizeof(mat4));
     // Note: The View/Projection matrices would typically be sent via a UBO.

     // Bind the diffuse texture to texture slot 1 (matching the fragment shader layout).
     if (g_diffuse_texture.id != 0) {
         SituationCmdBindTexture(cmd, g_diffuse_texture, 1);
     }

     // Draw each mesh that is part of the model.
     for (int i = 0; i < g_loaded_model.mesh_count; ++i) {
         SituationCmdDrawMesh(cmd, g_loaded_model.meshes[i]);
     }

     SituationCmdEndRender(cmd);
     SituationEndFrame();
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - 3D Model";
    init_info.window_width = 1024;
    init_info.window_height = 768;

    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // Since the model loading is a stub, resource initialization will fail.
    // In a real, working application, this would proceed to the main loop.
    if (init_model_resources() != 0) {
        printf("Note: Model loading failed as expected because `SituationLoadModelFromObj` is a stub.\n");
    } else {
        printf("Model loaded successfully. Running...\n");
    }

    // Main application loop.
    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        // This will not draw anything because the stub function returns an empty model.
        render_model();
    }

    cleanup_model_resources();
    SituationShutdown();
    return 0;
}
