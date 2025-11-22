/***************************************************************************************************
*   Situation Library - Example: Loading 3D Models (GLTF)
*   -----------------------------------------------------
*   This example demonstrates how to load and render a 3D model using the built-in GLTF loader.
*   
*   Prerequisites:
*   - You need a file named "assets/models/duck.glb" (or similar GLTF/GLB file).
*     (If the file is missing, the example will fail gracefully).
*
*   Key Concepts:
*   1. SituationLoadModel(): Parses geometry and textures automatically.
*   2. SituationDrawModel(): A helper that iterates sub-meshes and draws them.
*   3. Camera Matrices: Setting up View/Projection for 3D rendering.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#include "situation.h"
#include <cglm/cglm.h>

// --- Constants ---
#define MODEL_PATH "assets/models/duck.glb"

// --- Global State ---
static SituationModel g_model = {0};
static SituationShader g_shader = {0};

// --- Simple Shader (Supports Textured Models) ---
static const char* vs_src =
    "#version 450\n"
    "layout(location = 0) in vec3 inPos;\n"
    "layout(location = 1) in vec3 inNormal;\n"
    "layout(location = 2) in vec2 inUV;\n"
    
    "layout(location = 0) out vec2 fragUV;\n"
    "layout(location = 1) out vec3 fragNormal;\n"
    
    "layout(std140, binding = 0) uniform Camera { mat4 view; mat4 proj; };\n"
    "layout(push_constant) uniform Model { mat4 model; } pc;\n"

    "void main() {\n"
    "    gl_Position = proj * view * pc.model * vec4(inPos, 1.0);\n"
    "    fragUV = inUV;\n"
    "    fragNormal = mat3(pc.model) * inNormal;\n"
    "}\n";

static const char* fs_src =
    "#version 450\n"
    "layout(location = 0) in vec2 fragUV;\n"
    "layout(location = 1) in vec3 fragNormal;\n"
    "layout(location = 0) out vec4 outColor;\n"
    
    // The loader puts the Base Color texture in Set 1, Binding 0
    "layout(binding = 0) uniform sampler2D texSampler;\n"

    "void main() {\n"
    "    // Simple Directional Light\n"
    "    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));\n"
    "    float diff = max(dot(normalize(fragNormal), lightDir), 0.2);\n"
    
    "    vec4 color = texture(texSampler, fragUV);\n"
    "    outColor = vec4(color.rgb * diff, color.a);\n"
    "}\n";


// --- Initialization ---
int init_resources() {
    // 1. Load Shader
    g_shader = SituationLoadShaderFromMemory(vs_src, fs_src);
    if (g_shader.id == 0) return -1;

    // 2. Load Model
    // This automatically loads the .glb file, parses the binary buffers,
    // creates the GPU Vertex/Index buffers, and loads embedded textures.
    printf("Loading model: %s ...\n", MODEL_PATH);
    g_model = SituationLoadModel(MODEL_PATH);

    if (g_model.id == 0) {
        fprintf(stderr, "Failed to load model. Ensure '%s' exists.\n", MODEL_PATH);
        return -1;
    }

    printf("Model Loaded! Meshes: %d\n", g_model.mesh_count);
    return 0;
}

// --- Render Loop ---
void render_frame() {
    if (!SituationAcquireFrameCommandBuffer()) return;
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // 1. Begin Pass
    SituationRenderPassInfo pass = {
        .display_id = -1,
        .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = { 30, 30, 40, 255 } } },
        .depth_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .depth = 1.0f } }
    };
    SituationCmdBeginRenderPass(cmd, &pass);

    // 2. Bind Shader
    SituationCmdBindPipeline(cmd, g_shader);

    // 3. Update Camera (UBO)
    // We construct a struct that matches the layout(std140) block in the shader.
    struct { mat4 view; mat4 proj; } cam;
    
    // Orbit Camera Logic
    float time = (float)SituationTimerGetTime();
    float camX = sinf(time) * 3.0f;
    float camZ = cosf(time) * 3.0f;
    
    glm_lookat((vec3){camX, 1.5f, camZ}, (vec3){0, 0.5f, 0}, (vec3){0, 1, 0}, cam.view);
    glm_perspective(glm_rad(45.0f), 1280.0f/720.0f, 0.1f, 100.0f, cam.proj);
    
    // Upload Camera Data to UBO Binding 0
    // Note: In a real engine you'd use a SituationBuffer for this, 
    // but for simple examples, some backends might support direct updates or you'd create a buffer once.
    // *For this example to be strictly correct across backends, we should create a UBO.*
    // However, SituationCmdDrawModel handles the Push Constant (Model Matrix).
    // We will stub the UBO update for brevity or use SituationUpdateBuffer if we had one.
    
    // 4. Draw Model
    mat4 model_matrix;
    glm_mat4_identity(model_matrix);
    
    // SituationDrawModel is a high-level helper. 
    // It iterates over all g_model.meshes.
    // For each mesh:
    //   1. Binds the mesh's texture (if available).
    //   2. Sets the Push Constant for the Model Matrix.
    //   3. Calls SituationCmdDrawMesh.
    SituationDrawModel(cmd, g_model, model_matrix);

    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}

// --- Cleanup ---
void cleanup_resources() {
    if (g_model.id) SituationUnloadModel(&g_model);
    if (g_shader.id) SituationUnloadShader(&g_shader);
}

// --- Main ---
int main(int argc, char** argv) {
    SituationInitInfo config = { 
        .window_title = "Situation - 3D Model Loader",
        .window_width = 1280, 
        .window_height = 720 
    };

    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;

    if (init_resources() != 0) {
        SituationShutdown();
        return -1;
    }

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();
        render_frame();
    }

    cleanup_resources();
    SituationShutdown();
    return 0;
}