#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "../situation.h"
#include <stdio.h>
#include <cglm/cglm.h> // This example requires the cglm library

// --- NOTE ---
// This example assumes the existence of a model loading function `SituationLoadModelFromObj`
// which is not part of the core situation.h library. You would need to implement this
// function using a library like tinyobjloader-c or similar.
// For demonstration, we will define a stub for it.

// --- Stub for Model Loading ---
// In a real application, this would load a .obj file and create meshes and textures.
SituationModel SituationLoadModelFromObj(const char* path) {
    fprintf(stderr, "Warning: SituationLoadModelFromObj is a stub and does not load a real model.\n");
    SituationModel model = {0};
    return model;
}

// --- Global Handles ---
static SituationShader g_model_shader = {0};
static SituationModel g_loaded_model = {0};
static SituationTexture g_diffuse_texture = {0};

// --- Simple Shaders for a Textured Model ---
static const char* model_vertex_shader =
"#version 450 core\n"
"layout(location = 0) in vec3 inPosition;\n"
"layout(location = 1) in vec3 inNormal;\n"
"layout(location = 2) in vec2 inTexCoord;\n"
"layout(location = 0) out vec2 fragTexCoord;\n"
"layout(location = 1) out vec3 fragNormal;\n"
"layout(location = 2) out vec3 fragWorldPos;\n"
"layout(std140, binding = 0) uniform ViewData {\n"
"    mat4 uView;\n"
"    mat4 uProjection;\n"
"};\n"
"layout(push_constant) uniform ModelData {\n"
"    mat4 uModel;\n"
"};\n"
"void main() {\n"
"    vec4 worldPos = uModel * vec4(inPosition, 1.0);\n"
"    gl_Position = uProjection * uView * worldPos;\n"
"    fragTexCoord = inTexCoord;\n"
"    fragNormal = mat3(transpose(inverse(uModel))) * inNormal;\n"
"    fragWorldPos = worldPos.xyz;\n"
"}\n";

static const char* model_fragment_shader =
"#version 450 core\n"
"layout(location = 0) in vec2 fragTexCoord;\n"
"layout(location = 1) in vec3 fragNormal;\n"
"layout(location = 2) in vec3 fragWorldPos;\n"
"layout(location = 0) out vec4 outColor;\n"
"layout(binding = 1) uniform sampler2D uDiffuseTexture;\n"
"void main() {\n"
"    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));\n"
"    vec3 norm = normalize(fragNormal);\n"
"    float diff = max(dot(norm, lightDir), 0.2);\n"
"    vec4 texColor = texture(uDiffuseTexture, fragTexCoord);\n"
"    outColor = vec4(diff * texColor.rgb, texColor.a);\n"
"}\n";

// --- Initialization ---
int init_model_resources() {
    g_model_shader = SituationLoadShaderFromMemory(model_vertex_shader, model_fragment_shader);
    if (g_model_shader.id == 0) {
        fprintf(stderr, "Failed to load model shader: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    const char* model_path = "assets/models/cube.obj";
    g_loaded_model = SituationLoadModelFromObj(model_path);
    if (g_loaded_model.id == 0 || g_loaded_model.mesh_count == 0) {
         fprintf(stderr, "Failed to load model from %s: %s\n", model_path, SituationGetLastErrorMsg());
         return -1;
    }
    if (g_loaded_model.texture_count > 0) {
        g_diffuse_texture = g_loaded_model.all_model_textures[0];
    }

    return 0;
}

// --- Cleanup ---
void cleanup_model_resources() {
     if (g_loaded_model.id != 0) {
         SituationUnloadModel(&g_loaded_model);
     }
     if (g_model_shader.id != 0) {
         SituationUnloadShader(&g_model_shader);
     }
}

// --- Rendering the Model ---
void render_model() {
     if (!SituationAcquireFrameCommandBuffer()) return;
     SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

     SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){ 20, 20, 30, 255 });

     SituationCmdBindPipeline(cmd, g_model_shader);

     mat4 view, projection, model;
     glm_mat4_identity(view);
     glm_translate(view, (vec3){0.0f, 0.0f, -3.0f});
     glm_perspective(glm_rad(45.0f), 1024.0f / 768.0f, 0.1f, 100.0f, projection);
     glm_mat4_identity(model);

     SituationCmdSetPushConstant(cmd, 0, &model, sizeof(mat4));

     if (g_diffuse_texture.id != 0) {
         SituationCmdBindTexture(cmd, g_diffuse_texture, 1);
     }

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

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize situation.h: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // Since the model loading is a stub, this will fail.
    // In a real application, this would proceed.
    if (init_model_resources() != 0) {
        printf("Note: Model loading failed as expected because it's a stub.\n");
    } else {
        printf("Model loaded. Running...\n");
    }


    while (!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        render_model(); // This will not draw anything as the model is empty.
    }

    cleanup_model_resources();
    SituationShutdown();
    return 0;
}
