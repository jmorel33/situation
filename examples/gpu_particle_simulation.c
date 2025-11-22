/***************************************************************************************************
*   Situation Library - Example: GPU Particle System
*   ------------------------------------------------
*   This example demonstrates a high-performance particle system where 
*   simulation AND rendering happen entirely on the GPU.
*
*   Key Concepts:
*   1. Shared Buffer: One buffer acts as an SSBO (for Compute) and a VBO (for Drawing).
*   2. Compute Shader: Updates physics (Gravity, Velocity, Bounds).
*   3. Pipeline Barrier: Ensures Physics is done before Drawing starts.
*   4. Instanced Rendering: Drawing 100,000 quads with a single draw call.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER
#include "situation.h"
#include <cglm/cglm.h>

// --- Configuration ---
#define PARTICLE_COUNT 100000 // 100k Particles

// --- Data Structures ---
// This matches the layout in both C and GLSL.
typedef struct {
    vec2 pos; // Position (x, y)
    vec2 vel; // Velocity (vx, vy)
    vec4 col; // Color (r, g, b, a)
} Particle;

// --- Global Handles ---
static SituationComputePipeline g_compute_pipeline = {0};
static SituationShader g_render_pipeline = {0};
static SituationBuffer g_particle_buffer = {0};
static SituationMesh g_quad_mesh = {0};

// --- Shaders ---

// 1. Compute Shader (Physics)
static const char* cs_src =
    "#version 450\n"
    "layout(local_size_x = 256) in;\n"
    
    "struct Particle { vec2 pos; vec2 vel; vec4 col; };\n"
    
    "layout(std430, set = 0, binding = 0) buffer PBuffer { Particle p[]; } particles;\n"
    
    "void main() {\n"
    "    uint idx = gl_GlobalInvocationID.x;\n"
    "    if (idx >= 100000) return;\n"
    
    "    // Apply Gravity\n"
    "    particles.p[idx].vel.y -= 0.0005;\n"
    
    "    // Apply Velocity\n"
    "    particles.p[idx].pos += particles.p[idx].vel;\n"
    
    "    // Bounce off floor (-1.0 is bottom of screen)\n"
    "    if (particles.p[idx].pos.y < -1.0) {\n"
    "        particles.p[idx].pos.y = -1.0;\n"
    "        particles.p[idx].vel.y *= -0.8; // Lose energy\n"
    "    }\n"
    "}\n";

// 2. Vertex Shader (Rendering)
static const char* vs_src =
    "#version 450\n"
    "layout(location = 0) in vec2 inQuadPos;\n" // From Mesh
    
    "struct Particle { vec2 pos; vec2 vel; vec4 col; };\n"
    
    // We bind the SAME buffer as a Storage Buffer here
    "layout(std430, binding = 0) readonly buffer PBuffer { Particle p[]; } particles;\n"
    
    "layout(location = 0) out vec4 fragColor;\n"
    
    "void main() {\n"
    "    // gl_InstanceID tells us which particle we are drawing\n"
    "    Particle p = particles.p[gl_InstanceID];\n"
    
    "    vec2 finalPos = p.pos + (inQuadPos * 0.005); // Scale quad down\n"
    "    gl_Position = vec4(finalPos, 0.0, 1.0);\n"
    "    fragColor = p.col;\n"
    "}\n";

// 3. Fragment Shader (Rendering)
static const char* fs_src =
    "#version 450\n"
    "layout(location = 0) in vec4 fragColor;\n"
    "out vec4 outColor;\n"
    "void main() { outColor = fragColor; }\n";


// --- Initialization ---
int init_resources() {
    // 1. Generate Initial Particles
    Particle* data = (Particle*)malloc(PARTICLE_COUNT * sizeof(Particle));
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        // Random position [-1, 1]
        data[i].pos[0] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        data[i].pos[1] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        // Random velocity
        data[i].vel[0] = (((float)rand() / RAND_MAX) - 0.5f) * 0.01f;
        data[i].vel[1] = (((float)rand() / RAND_MAX) - 0.5f) * 0.01f;
        // Color based on ID
        data[i].col[0] = (float)(i % 255) / 255.0f; // R
        data[i].col[1] = 0.5f;                      // G
        data[i].col[2] = 1.0f;                      // B
        data[i].col[3] = 1.0f;                      // A
    }

    // 2. Create Shared Buffer
    // This is the critical part. Usage flags allow it to be used for EVERYTHING.
    g_particle_buffer = SituationCreateBuffer(
        PARTICLE_COUNT * sizeof(Particle),
        data,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_VERTEX_BUFFER
    );
    free(data);

    // 3. Create Pipelines
    g_compute_pipeline = SituationCreateComputePipelineFromMemory(cs_src, SIT_COMPUTE_LAYOUT_ONE_SSBO);
    g_render_pipeline = SituationLoadShaderFromMemory(vs_src, fs_src);

    // 4. Create Quad Mesh (for instancing)
    float quad_verts[] = { -1,-1,  1,-1,  1,1,  -1,1 };
    uint32_t quad_inds[] = { 0,1,2, 0,2,3 };
    g_quad_mesh = SituationCreateMesh(quad_verts, 4, sizeof(float)*2, quad_inds, 6);

    if (g_particle_buffer.id == 0 || g_compute_pipeline.id == 0) return -1;
    return 0;
}

// --- Render Loop ---
void render_frame() {
    if (!SituationAcquireFrameCommandBuffer()) return;
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // --- STEP 1: PHYSICS (Compute) ---
    SituationCmdBindComputePipeline(cmd, g_compute_pipeline);
    SituationCmdBindDescriptorSet(cmd, 0, g_particle_buffer);
    
    // 100,000 particles / 256 threads per group = ~391 groups
    SituationCmdDispatch(cmd, (PARTICLE_COUNT + 255) / 256, 1, 1);

    // --- STEP 2: BARRIER ---
    // Wait for Physics (Compute Write) -> Before Drawing (Vertex Read)
    SituationCmdPipelineBarrier(cmd, 
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE, 
        SITUATION_BARRIER_VERTEX_SHADER_READ
    );

    // --- STEP 3: DRAWING (Graphics) ---
    SituationRenderPassInfo pass = {
        .display_id = -1,
        .color_attachment = { .loadOp = SIT_LOAD_OP_CLEAR, .clear = { .color = {10, 10, 20, 255} } }
    };

    SituationCmdBeginRenderPass(cmd, &pass);
    
    SituationCmdBindPipeline(cmd, g_render_pipeline);
    
    // Bind the same buffer so the Vertex Shader can read the positions
    SituationCmdBindDescriptorSet(cmd, 0, g_particle_buffer);

    // Draw the Quad Mesh 100,000 times
    // Arg 1: Index Count (6 for a quad)
    // Arg 2: Instance Count (100,000 particles)
    // Arg 3: First Index
    // Arg 4: Vertex Offset
    // Arg 5: First Instance
    SituationCmdDrawIndexed(cmd, 6, PARTICLE_COUNT, 0, 0, 0);
    
    SituationCmdEndRenderPass(cmd);
    SituationEndFrame();
}

// --- Cleanup ---
void cleanup_resources() {
    if (g_particle_buffer.id) SituationDestroyBuffer(&g_particle_buffer);
    if (g_quad_mesh.id) SituationDestroyMesh(&g_quad_mesh);
    if (g_render_pipeline.id) SituationUnloadShader(&g_render_pipeline);
    if (g_compute_pipeline.id) SituationDestroyComputePipeline(&g_compute_pipeline);
}

// --- Main ---
int main(int argc, char** argv) {
    SituationInitInfo config = { .window_title = "Situation - GPU Particles", .window_width = 1280, .window_height = 720 };
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;
    if (init_resources() != 0) return -1;

    printf("Simulating %d Particles on GPU.\n", PARTICLE_COUNT);

    while (!SituationWindowShouldClose()) {
        SITUATION_BEGIN_FRAME();
        render_frame();
    }

    cleanup_resources();
    SituationShutdown();
    return 0;
}