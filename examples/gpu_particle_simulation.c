/*
 * GPU Particle Simulation and Rendering
 *
 * This example demonstrates a powerful technique where a compute shader is used
 * to simulate particle physics, and a traditional graphics pipeline is used to
 * render the results. The key is that both pipelines operate on the same
 * buffer of particle data on the GPU, avoiding costly CPU-GPU data transfers
 * each frame.
 */

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER // Required for the compute shader
#include "../situation.h"

// Standard C libraries and cglm for math.
#include <stdio.h>
#include <stdlib.h>
#include <cglm/cglm.h>

// --- Particle Data Structure ---
// This struct defines the data for a single particle.
// It will be mirrored in the GLSL shaders.
typedef struct {
    vec2 position; // Current position of the particle
    vec2 velocity; // Current velocity of the particle
    vec4 color;    // Color of the particle (not used in this simple example)
} Particle;

// --- Constants ---
#define PARTICLE_COUNT 10000

// --- Global Handles ---
static SituationComputePipeline g_particle_update_pipeline = {0}; // The compute shader for physics
static SituationShader g_particle_render_pipeline = {0};           // The graphics shader for drawing
static SituationBuffer g_particle_buffer = {0};                    // The GPU buffer holding all particle data
static SituationMesh g_particle_mesh = {0};                        // A simple quad mesh to represent each particle

// --- GLSL Shaders ---

// Compute Shader: Updates particle positions and velocities based on simple physics.
const char* particle_compute_shader_source =
"#version 450\n"
// The Particle struct must match the C host code for memory layout compatibility.
"struct Particle { vec2 pos; vec2 vel; vec4 col; };\n"
// The buffer of particles, bound at binding point 0.
"layout(std430, binding = 0) buffer ParticleBuffer { Particle particles[]; } particle_buffer;\n"
// Define the local workgroup size. 256 is a common choice.
"layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;\n"
"void main() {\n"
"    uint index = gl_GlobalInvocationID.x; // Get the unique index for this particle.
"
"    if (index >= 10000) return; // Boundary check.
"
"    // Simple physics simulation: apply velocity and a constant downward force (gravity).
"
"    particle_buffer.particles[index].vel.y -= 0.0001; // Apply gravity
"
"    particle_buffer.particles[index].pos += particle_buffer.particles[index].vel * 0.1; // Apply velocity
"
"    // Simple boundary check to wrap particles around the screen.
"
"    if (particle_buffer.particles[index].pos.y < -1.0) particle_buffer.particles[index].pos.y = 1.0;
"
"}\n";

// Vertex Shader: Renders the particles using instancing.
const char* particle_vertex_shader_source =
"#version 450\n"
// The input is a single vertex from our quad mesh.
"layout(location = 0) in vec2 inPosition;\n"
// The Particle struct, matching the compute shader and C code.
"struct Particle { vec2 pos; vec2 vel; vec4 col; };\n"
// The same particle buffer, bound at binding point 0, but marked as read-only.
"layout(std430, binding = 0) readonly buffer ParticleBuffer { Particle particles[]; } particle_buffer;\n"
"void main() {\n"
"    // `gl_InstanceID` gives us the index of the current particle we are rendering.
"
"    // We fetch its position from the buffer.
"
"    vec2 particle_pos = particle_buffer.particles[gl_InstanceID].pos;\n"
"    // We construct the final vertex position by taking the particle's center position
"
"    // and adding the offset of the quad's vertex, scaled down to create a small point.
"
"    gl_Position = vec4(particle_pos + inPosition * 0.01, 0.0, 1.0);\n"
"}\n";

// Fragment Shader: Outputs a solid color for each particle.
const char* particle_fragment_shader_source =
"#version 450\n"
"out vec4 outColor;\n"
"void main() { outColor = vec4(1.0, 0.8, 0.4, 1.0); }\n"; // A nice orange color


// --- Initialization ---
// Sets up all GPU resources needed for the simulation.
void init_particle_system() {
    // 1. Create the compute and graphics shader pipelines.
    g_particle_update_pipeline = SituationCreateComputePipelineFromMemory(particle_compute_shader_source);
    g_particle_render_pipeline = SituationLoadShaderFromMemory(particle_vertex_shader_source, particle_fragment_shader_source);

    // 2. Create the main particle buffer on the GPU.
    // First, we create initial data on the CPU.
    Particle* initial_particles = malloc(PARTICLE_COUNT * sizeof(Particle));
    for (int i = 0; i < PARTICLE_COUNT; ++i) {
        initial_particles[i].position[0] = (rand() / (float)RAND_MAX) * 2.0f - 1.0f; // Random X
        initial_particles[i].position[1] = (rand() / (float)RAND_MAX) * 2.0f - 1.0f; // Random Y
        initial_particles[i].velocity[0] = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f) * 0.01; // Random initial velocity
        initial_particles[i].velocity[1] = ((rand() / (float)RAND_MAX) * 2.0f - 1.0f) * 0.01;
    }
    // Now, create the GPU buffer and upload the initial data.
    // The buffer is marked as both a storage buffer (for compute) and a vertex buffer (for graphics).
    g_particle_buffer = SituationCreateBuffer(PARTICLE_COUNT * sizeof(Particle), initial_particles,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_VERTEX_BUFFER);
    free(initial_particles); // Free the CPU-side copy.

    // 3. Create a simple quad mesh. We will draw this mesh once for each particle (instancing).
    static const float quad_vertices[] = { -1.0, -1.0, 1.0, -1.0, 1.0, 1.0, -1.0, 1.0 };
    static const uint32_t quad_indices[] = { 0, 1, 2, 0, 2, 3 };
    g_particle_mesh = SituationCreateMesh(quad_vertices, 4, sizeof(float)*2, quad_indices, 6);
}

// --- Per-Frame Simulation and Rendering ---
// This function is called every frame to update and draw the particles.
void run_particle_simulation_frame() {
    if (!SituationAcquireFrameCommandBuffer()) return;
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // --- 1. COMPUTE PASS ---
    // This pass updates the particle data.
    SituationCmdBindComputePipeline(cmd, g_particle_update_pipeline);
    SituationCmdBindComputeBuffer(cmd, 0, g_particle_buffer);
    // Dispatch the compute shader. We calculate the number of workgroups needed.
    SituationCmdDispatch(cmd, (PARTICLE_COUNT + 255) / 256, 1, 1);

    // --- 2. SYNCHRONIZATION BARRIER ---
    // This is a CRITICAL step. It ensures that the compute shader's writes to the
    // particle buffer are finished and visible before the vertex shader tries to
    // read from that same buffer in the rendering pass.
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE, // Source: The compute shader wrote to the buffer.
        SITUATION_BARRIER_VERTEX_SHADER_READ);  // Destination: The vertex shader will read from it.

    // --- 3. RENDER PASS ---
    // This pass draws the updated particles to the screen.
    SituationCmdBeginRenderToDisplay(cmd, -1, (ColorRGBA){10, 20, 30, 255}); // Dark blue background
    SituationCmdBindPipeline(cmd, g_particle_render_pipeline);
    // Bind the particle buffer again, this time for the graphics pipeline.
    SituationCmdBindStorageBuffer(cmd, 0, g_particle_buffer);
    // Draw the quad mesh `PARTICLE_COUNT` times. The vertex shader uses `gl_InstanceID`
    // to fetch the correct data for each instance.
    SituationCmdDrawMeshInstanced(cmd, g_particle_mesh, PARTICLE_COUNT);
    SituationCmdEndRender(cmd);

    // Finalize the frame and present it.
    SituationEndFrame();
}

// --- Cleanup ---
// Releases all GPU resources.
void cleanup_particle_system() {
    SituationDestroyComputePipeline(&g_particle_update_pipeline);
    SituationUnloadShader(&g_particle_render_pipeline);
    SituationDestroyBuffer(&g_particle_buffer);
    SituationDestroyMesh(&g_particle_mesh);
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = { .window_title = "GPU Particles" };
    if (SituationInit(argc, argv, &init_info) != SITUATION_SUCCESS) return -1;

    init_particle_system();

    // Main application loop.
    while(!SituationWindowShouldClose()) {
        SituationPollInputEvents();
        SituationUpdateTimers();
        run_particle_simulation_frame(); // Update and render particles each frame.
    }

    cleanup_particle_system();
    SituationShutdown();
    return 0;
}
