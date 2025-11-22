/***************************************************************************************************
*   Situation Library - Example: Basic Compute (SSBO)
*   -------------------------------------------------
*   This example demonstrates General Purpose GPU (GPGPU) computing.
*   We will create an array of numbers on the CPU, upload them to the GPU,
*   multiply them by a factor in parallel using a Compute Shader, and read the results back.
*
*   Key Concepts:
*   1. SSBOs (Shader Storage Buffer Objects) for generic data.
*   2. Compute Pipelines & Layouts.
*   3. Dispatching Workgroups.
*   4. GPU-to-CPU Readback.
*
***************************************************************************************************/

#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER // Mandatory for runtime GLSL compilation
#include "situation.h"

// --- Configuration ---
#define DATA_SIZE 1024 // Number of floats to process

// --- Global Handles ---
static SituationComputePipeline g_compute_pipeline = {0};
static SituationBuffer g_input_buffer = {0};
static SituationBuffer g_output_buffer = {0};

// --- GLSL Compute Shader Source ---
// This shader takes an input array, multiplies every element by a constant,
// and writes it to the output array.
static const char* compute_shader_src =
    "#version 450\n"
    // We use a local workgroup size of 64 threads.
    "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
    
    // Input Buffer (Set 0)
    "layout(std430, set = 0, binding = 0) readonly buffer InBuffer {\n"
    "    float values[];\n"
    "} input_data;\n"

    // Output Buffer (Set 1)
    "layout(std430, set = 1, binding = 0) writeonly buffer OutBuffer {\n"
    "    float values[];\n"
    "} output_data;\n"

    // Push Constant: Simple data sent directly in the command buffer
    "layout(push_constant) uniform PushConsts {\n"
    "    float multiplier;\n"
    "    uint count;\n"
    "} pc;\n"

    "void main() {\n"
    "    // Get the unique global index of this thread\n"
    "    uint idx = gl_GlobalInvocationID.x;\n"
    
    "    // Boundary check\n"
    "    if (idx >= pc.count) return;\n"
    
    "    // Perform the math\n"
    "    output_data.values[idx] = input_data.values[idx] * pc.multiplier;\n"
    "}\n";


// --- Initialization ---
int init_compute_resources() {
    // 1. Generate CPU Data
    // We create a simple sequence: 0.0, 1.0, 2.0 ... 1023.0
    float host_data[DATA_SIZE];
    for (int i = 0; i < DATA_SIZE; i++) {
        host_data[i] = (float)i;
    }

    // 2. Create Input Buffer (SSBO)
    // Usage: STORAGE (for shader) + TRANSFER_DST (to upload data)
    // Note: We pass 'host_data' here, so the library uploads it immediately.
    g_input_buffer = SituationCreateBuffer(
        sizeof(host_data), 
        host_data, 
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_DST
    );

    // 3. Create Output Buffer (SSBO)
    // Usage: STORAGE (for shader) + TRANSFER_SRC (to read back to CPU)
    // We pass NULL because we only care about allocating the space on GPU.
    g_output_buffer = SituationCreateBuffer(
        sizeof(host_data), 
        NULL, 
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER | SITUATION_BUFFER_USAGE_TRANSFER_SRC
    );

    if (g_input_buffer.id == 0 || g_output_buffer.id == 0) {
        fprintf(stderr, "Buffer Error: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // 4. Create Compute Pipeline
    // We specify SIT_COMPUTE_LAYOUT_TWO_SSBOS because our shader uses:
    // Set 0 (Input) and Set 1 (Output).
    g_compute_pipeline = SituationCreateComputePipelineFromMemory(
        compute_shader_src, 
        SIT_COMPUTE_LAYOUT_TWO_SSBOS
    );

    if (g_compute_pipeline.id == 0) {
        char* err = SituationGetLastErrorMsg();
        fprintf(stderr, "Pipeline Error: %s\n", err);
        free(err);
        return -1;
    }

    return 0;
}

// --- Execution ---
void run_compute_pass() {
    // Even though we aren't drawing to the screen, we need a command buffer context.
    if (!SituationAcquireFrameCommandBuffer()) return;
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // 1. Bind Pipeline
    SituationCmdBindComputePipeline(cmd, g_compute_pipeline);

    // 2. Bind Buffers
    // Map SituationBuffers to the Descriptor Sets defined in the shader.
    SituationCmdBindDescriptorSet(cmd, 0, g_input_buffer);  // set = 0
    SituationCmdBindDescriptorSet(cmd, 1, g_output_buffer); // set = 1

    // 3. Push Constants
    // Send the multiplier (10.0x) and the size.
    struct { float mult; uint32_t count; } constants = { 10.0f, DATA_SIZE };
    SituationCmdSetPushConstant(cmd, 0, &constants, sizeof(constants));

    // 4. Dispatch
    // We calculate how many workgroups we need.
    // Layout local_size_x is 64. Total items is 1024.
    // 1024 / 64 = 16 Workgroups.
    SituationCmdDispatch(cmd, DATA_SIZE / 64, 1, 1);

    // 5. Pipeline Barrier
    // Ensure the Compute Shader finishes writing (COMPUTE_WRITE)
    // before the Host tries to read the memory (HOST_READ).
    SituationCmdPipelineBarrier(cmd, 
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE, 
        SITUATION_BARRIER_TRANSFER_READ
    );

    // Submit execution to GPU
    SituationEndFrame();
}

// --- Cleanup ---
void cleanup_resources() {
    if (g_input_buffer.id) SituationDestroyBuffer(&g_input_buffer);
    if (g_output_buffer.id) SituationDestroyBuffer(&g_output_buffer);
    if (g_compute_pipeline.id) SituationDestroyComputePipeline(&g_compute_pipeline);
}

// --- Main Entry ---
int main(int argc, char** argv) {
    // Init (Window is required for Context, even if hidden)
    SituationInitInfo config = { .window_title = "Situation - Compute", .window_width = 100, .window_height = 100 };
    if (SituationInit(argc, argv, &config) != SITUATION_SUCCESS) return -1;

    if (init_compute_resources() != 0) return -1;

    printf("Dispatching Compute Shader...\n");
    printf("Task: Multiply array [0..1023] by 10.0 on GPU.\n");

    // Run the GPU task
    run_compute_pass();

    // --- Verify Results ---
    // This function blocks the CPU until the GPU is finished and the data is copied back.
    float results[DATA_SIZE];
    SituationGetBufferData(g_output_buffer, 0, sizeof(results), results);

    printf("Results Readback:\n");
    printf("  Input[0] = 0.0  -> Output[0] = %.1f\n", results[0]);
    printf("  Input[1] = 1.0  -> Output[1] = %.1f\n", results[1]);
    printf("  Input[50] = 50.0 -> Output[50] = %.1f\n", results[50]);
    printf("  Input[1023] = 1023.0 -> Output[1023] = %.1f\n", results[1023]);

    if (results[50] == 500.0f) {
        printf("\nSUCCESS: GPU calculation verified.\n");
    } else {
        printf("\nFAILURE: Calculation mismatch.\n");
    }

    cleanup_resources();
    SituationShutdown();
    return 0;
}