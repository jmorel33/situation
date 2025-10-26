// Define implementation, backend, and enable the shader compiler for compute shaders.
#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER // Required for compute shaders
#include "../situation.h"

// Standard C libraries.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// --- NOTE ---
// This example demonstrates a compute shader that processes an image.
// It is designed to fail gracefully because it depends on an external image file
// ("assets/textures/input_image.png") which is not provided in the repository.
// To make this example fully functional, you would need to:
// 1. Create the "assets/textures" directory.
// 2. Place a PNG image named "input_image.png" inside it.
// 3. Implement the `load_image_data_into_host_buffer` function using a library
//    like stb_image.h to load the pixel data.

// --- Global Handles ---
// Handles for the compute shader pipeline and the input/output buffers on the GPU.
static SituationComputePipeline g_invert_pipeline = {0};
static SituationBuffer g_input_ssbo = {0};  // Shader Storage Buffer Object for input pixels
static SituationBuffer g_output_ssbo = {0}; // Shader Storage Buffer Object for output pixels
static uint32_t g_pixel_count = 0;          // Total number of pixels in the image

// --- GLSL Compute Shader Source ---
// This shader inverts the RGB colors of each pixel in an image.
static const char* invert_compute_shader_source =
"#version 450\n"
// Define a simple struct for a pixel, holding its 32-bit RGBA value.
"struct Pixel { uint rgba; };\n"
// Input buffer (SSBO) at binding 0, marked as read-only.
"layout(std430, binding = 0) restrict readonly buffer InputBuffer { Pixel pixels[]; } input_buffer;\n"
// Output buffer (SSBO) at binding 1, marked as write-only.
"layout(std430, binding = 1) restrict writeonly buffer OutputBuffer { Pixel pixels[]; } output_buffer;\n"
// Push constants for small, dynamic data (in this case, the total pixel count).
"layout(push_constant) uniform PushConstants { uint pixel_count; } pc;\n"
// Helper function to pack four float color components (0.0-1.0) into a single 32-bit integer.
"uint packRGBA(float r, float g, float b, float a) {\n"
"    return (uint(a*255.0)<<24)|(uint(b*255.0)<<16)|(uint(g*255.0)<<8)|uint(r*255.0);\n"
"}\n"
// Helper function to unpack a 32-bit integer into four float color components.
"void unpackRGBA(uint p, out float r, out float g, out float b, out float a) {\n"
"    r = float(p & 0xFF)/255.0; g = float((p>>8)&0xFF)/255.0; b = float((p>>16)&0xFF)/255.0; a = float((p>>24)&0xFF)/255.0;\n"
"}\n"
// The main function for the compute shader.
"void main() {\n"
"    uint index = gl_GlobalInvocationID.x; // Get the unique ID for this shader invocation.
"
"    if (index >= pc.pixel_count) return; // Boundary check: do not process beyond the image size.
"
"    uint input_packed = input_buffer.pixels[index].rgba; // Read the pixel data from the input buffer.
"
"    float r, g, b, a;\n"
"    unpackRGBA(input_packed, r, g, b, a); // Unpack the integer color into floats.
"
"    float inv_r = 1.0 - r; float inv_g = 1.0 - g; float inv_b = 1.0 - b; // Invert the RGB components.
"
"    output_buffer.pixels[index].rgba = packRGBA(inv_r, inv_g, inv_b, a); // Pack and write the result to the output buffer.
"
"}\n";

// --- Helper Function to Load Image Data (Stub) ---
// This is a placeholder function. In a real application, you would use a library
// like stb_image.h to load the image file from disk into a byte array.
uint32_t load_image_data_into_host_buffer(const char* filename, uint8_t** out_data) {
    fprintf(stderr, "Warning: `load_image_data_into_host_buffer` is a stub and does not load a real image.\n");
    *out_data = NULL;
    return 0; // Returning 0 ensures the compute shader is not dispatched.
}

// --- Initialization ---
// Creates the compute pipeline and associated GPU buffers.
int init_compute_resources() {
    // Create the compute pipeline from the GLSL source code.
    g_invert_pipeline = SituationCreateComputePipelineFromMemory(invert_compute_shader_source);
    if (g_invert_pipeline.id == 0) {
        fprintf(stderr, "Failed to create compute pipeline: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // Load image data from disk into a host-side buffer.
    uint8_t* image_data = NULL;
    g_pixel_count = load_image_data_into_host_buffer("assets/textures/input_image.png", &image_data);
    if (g_pixel_count == 0 || image_data == NULL) {
        return -1; // Fail initialization if image loading fails.
    }
    size_t image_data_size = g_pixel_count * sizeof(uint32_t); // 4 bytes per pixel (RGBA)

    // Create the input SSBO on the GPU and upload the image data to it.
    g_input_ssbo = SituationCreateBuffer(image_data_size, image_data, SITUATION_BUFFER_USAGE_STORAGE_BUFFER);
    if (g_input_ssbo.id == 0) {
        fprintf(stderr, "Failed to create input SSBO: %s\n", SituationGetLastErrorMsg());
        free(image_data);
        return -1;
    }

    // Create the output SSBO on the GPU. It is not initialized with any data.
    g_output_ssbo = SituationCreateBuffer(image_data_size, NULL, SITUATION_BUFFER_USAGE_STORAGE_BUFFER);
    if (g_output_ssbo.id == 0) {
        fprintf(stderr, "Failed to create output SSBO: %s\n", SituationGetLastErrorMsg());
        free(image_data);
        return -1;
    }

    // The host-side image data can be freed now that it's on the GPU.
    free(image_data);
    return 0;
}

// --- Cleanup ---
// Releases all allocated GPU resources.
void cleanup_compute_resources() {
    if (g_output_ssbo.id != 0) SituationDestroyBuffer(&g_output_ssbo);
    if (g_input_ssbo.id != 0) SituationDestroyBuffer(&g_input_ssbo);
    if (g_invert_pipeline.id != 0) SituationDestroyComputePipeline(&g_invert_pipeline);
}

// --- Run Compute Shader ---
// Dispatches the compute shader to perform the image processing.
void run_compute_shader() {
    if (g_pixel_count == 0) return; // Don't run if there's no data.

    if (!SituationAcquireFrameCommandBuffer()) return; // Prepare for a new frame/command submission.
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    // Bind the compute pipeline and the input/output buffers.
    SituationCmdBindComputePipeline(cmd, g_invert_pipeline);
    SituationCmdBindComputeBuffer(cmd, 0, g_input_ssbo);  // Bind input to binding point 0
    SituationCmdBindComputeBuffer(cmd, 1, g_output_ssbo); // Bind output to binding point 1

    // Send the pixel count to the shader via push constants.
    SituationCmdSetPushConstant(cmd, 0, &g_pixel_count, sizeof(g_pixel_count));

    // Dispatch the compute shader. We launch one invocation per pixel.
    SituationCmdDispatch(cmd, g_pixel_count, 1, 1);

    // Insert a memory barrier to ensure that the compute shader's writes to the
    // output buffer are visible to subsequent operations that might read from it.
    SituationMemoryBarrier(SITUATION_BARRIER_COMPUTE_SHADER_STORAGE_WRITE);

    // End the frame/command submission.
    SituationEndFrame();

    // At this point, g_output_ssbo contains the processed image data.
    // You could now, for example, read this data back to the CPU or use it
    // as a texture in a rendering pass.
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Compute Shader SSBO Example";

    // Initialize the library. A window is created but not used for rendering in this example.
    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    // Initialize compute resources. This is expected to fail due to the stub image loader.
    if (init_compute_resources() != 0) {
        printf("Note: Compute resources failed to initialize as expected (stub image loader).\n");
    }

    // Run the compute shader (will be a no-op if initialization failed).
    run_compute_shader();

    printf("Compute shader run complete (no-op). Press Enter to exit.\n");
    getchar();

    // Clean up all resources.
    cleanup_compute_resources();
    SituationShutdown();
    return 0;
}
