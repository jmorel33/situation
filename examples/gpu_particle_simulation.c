/*
 * GPU Particle Simulation and Rendering (Conceptual Example)
 *
 * This file illustrates the concept of combining compute and graphics pipelines
 * for a particle simulation, as described in the situation.h README.
 * This is not a complete, runnable program, but a guide to the required structure and API calls.
 */

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER
#include "../situation.h"
#include <stdio.h>
#include <stdlib.h>
#include <cglm/cglm.h>

// --- Particle Data Structure ---
typedef struct {
    vec2 position;
    vec2 velocity;
    vec4 color;
} Particle;

// --- Constants ---
#define PARTICLE_COUNT 10000

// --- Global Handles ---
static SituationComputePipeline g_particle_update_pipeline = {0};
static SituationShader g_particle_render_pipeline = {0};
static SituationBuffer g_particle_buffer = {0}; // SSBO for particle data
static SituationMesh g_particle_mesh = {0};     // A simple quad mesh for rendering each particle

// --- GLSL Shaders (Conceptual) ---

// Compute Shader to update particle positions
const char* particle_compute_shader_source =
"#version 450\n"
"struct Particle { vec2 pos; vec2 vel; vec4 col; };\n"
"layout(std430, binding = 0) buffer ParticleBuffer { Particle particles[]; } particle_buffer;\n"
"layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;\n"
"void main() {\n"
"    uint index = gl_GlobalInvocationID.x;\n"
"    if (index >= 10000) return;\n"
"    // Simple physics: apply velocity and gravity\n"
"    particle_buffer.particles[index].vel.y -= 0.001;\n"
"    particle_buffer.particles[index].pos += particle_buffer.particles[index].vel;\n"
"    // Boundary check\n"
"    if (particle_buffer.particles[index].pos.y < -1.0) particle_buffer.particles[index].pos.y = 1.0;\n"
"}\n";

// Vertex Shader to render particles
const char* particle_vertex_shader_source =
"#version 450\n"
"layout(location = 0) in vec2 inPosition;\n" // Position from the quad mesh
"struct Particle { vec2 pos; vec2 vel; vec4 col; };\n"
"layout(std430, binding = 0) readonly buffer ParticleBuffer { Particle particles[]; } particle_buffer;\n"
"void main() {\n"
"    // Get particle position from the SSBO and offset the quad vertex\n"
"    vec2 particle_pos = particle_buffer.particles[gl_InstanceID].pos;\n"
"    gl_Position = vec4(particle_pos + inPosition * 0.01, 0.0, 1.0);\n"
"}\n";

// Fragment Shader
const char* particle_fragment_shader_source =
"#version 450\n"
"out vec4 outColor;\n"
"void main() { outColor = vec4(1.0, 1.0, 1.0, 1.0); }\n";


void init_particle_system() {
    // 1. Create compute and graphics pipelines
    g_particle_update_pipeline = SituationCreateComputePipelineFromMemory(particle_compute_shader_source);
    g_particle_render_pipeline = SituationLoadShaderFromMemory(particle_vertex_shader_source, particle_fragment_shader_source);

    // 2. Create a buffer for particle data
    Particle* initial_particles = malloc(PARTICLE_COUNT * sizeof(Particle));
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        initial_particles[i].position[0] = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        initial_particles[i].position[1] = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        initial_particles[i].velocity[0] = 0.0f;
        initial_particles[i].velocity[1] = 0.0f;
    }
    g_particle_buffer = SituationCreateBuffer(PARTICLE_COUNT * sizeof(Particle), initial_particles,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_VERTEX_BUFFER);
    free(initial_particles);

    // 3. Create a quad mesh for instanced rendering
    static const float quad_vertices[] = { -1.0, -1.0, 1.0, -1.0, 1.0, 1.0, -1.0, 1.0 };
    static const uint32_t quad_indices[] = { 0, 1, 2, 0, 2, 3 };
    g_particle_mesh = SituationCreateMesh(quad_vertices, 4, sizeof(float)*2, quad_indices, 6);
}

void run_particle_simulation_frame() {
    if (!SituationAcquireFrameCommandBuffer()) return;
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // --- COMPUTE PASS ---
    SituationCmdBindComputePipeline(cmd, g_particle_update_pipeline);
    SituationCmdBindComputeBuffer(cmd, 0, g_particle_buffer);
    SituationCmdDispatch(cmd, (PARTICLE_COUNT + 255) / 256, 1, 1);

    // --- SYNCHRONIZATION BARRIER ---
    // Ensure compute shader writes are visible to the vertex shader reads
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_VERTEX_SHADER_READ);

    // --- RENDER PASS ---
    SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){0,0,0,255});
    SituationCmdBindPipeline(cmd, g_particle_render_pipeline);
    // Bind the particle buffer as a storage buffer for the vertex shader to read from
    SituationCmdBindStorageBuffer(cmd, 0, g_particle_buffer);
    // Draw the particle mesh using instancing
    SituationCmdDrawMeshInstanced(cmd, g_particle_mesh, PARTICLE_COUNT);
    SituationCmdEndRender(cmd);

    SituationEndFrame();
}

void cleanup_particle_system() {
    SituationDestroyComputePipeline(&g_particle_update_pipeline);
    SituationUnloadShader(&g_particle_render_pipeline);
    SituationDestroyBuffer(&g_particle_buffer);
    SituationDestroyMesh(&g_particle_mesh);
}

int main(int argc, char* argv[]) {
    SituationInitInfo init_info = { .window_title = "GPU Particles" };
    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) return -1;

    init_particle_system();

    while(!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        run_particle_simulation_frame();
    }

    cleanup_particle_system();
    SituationShutdown();
    return 0;
}
