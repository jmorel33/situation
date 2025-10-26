#define SITUATION_IMPLEMENTATION
#define SITUATION_USE_OPENGL // Or SITUATION_USE_VULKAN
#define SITUATION_ENABLE_SHADER_COMPILER // Required for compute shaders
#include "../situation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// --- NOTE ---
// This example requires an image file named "input_image.png" in an "assets/textures" directory.
// Since this file is not provided, the loading will fail. A stub function is provided.
// You would typically use a library like stb_image.h to load the image.

// --- Global Handles ---
static SituationComputePipeline g_invert_pipeline = {0};
static SituationBuffer g_input_ssbo = {0};
static SituationBuffer g_output_ssbo = {0};
static uint32_t g_pixel_count = 0;

// --- GLSL Compute Shader Source ---
static const char* invert_compute_shader_source =
"#version 450\n"
"struct Pixel { uint rgba; };\n"
"layout(std430, binding = 0) restrict readonly buffer InputBuffer { Pixel pixels[]; } input_buffer;\n"
"layout(std430, binding = 1) restrict writeonly buffer OutputBuffer { Pixel pixels[]; } output_buffer;\n"
"layout(push_constant) uniform PushConstants { uint pixel_count; } pc;\n"
"uint packRGBA(float r, float g, float b, float a) {\n"
"    return (uint(a*255.0)<<24)|(uint(b*255.0)<<16)|(uint(g*255.0)<<8)|uint(r*255.0);\n"
"}\n"
"void unpackRGBA(uint p, out float r, out float g, out float b, out float a) {\n"
"    r = float(p & 0xFF)/255.0; g = float((p>>8)&0xFF)/255.0; b = float((p>>16)&0xFF)/255.0; a = float((p>>24)&0xFF)/255.0;\n"
"}\n"
"void main() {\n"
"    uint index = gl_GlobalInvocationID.x;\n"
"    if (index >= pc.pixel_count) return;\n"
"    uint input_packed = input_buffer.pixels[index].rgba;\n"
"    float r, g, b, a;\n"
"    unpackRGBA(input_packed, r, g, b, a);\n"
"    float inv_r = 1.0 - r; float inv_g = 1.0 - g; float inv_b = 1.0 - b;\n"
"    output_buffer.pixels[index].rgba = packRGBA(inv_r, inv_g, inv_b, a);\n"
"}\n";

// --- Helper Function to Load Image Data (Stub) ---
uint32_t load_image_data_into_host_buffer(const char* filename, uint8_t** out_data) {
    fprintf(stderr, "Warning: load_image_data_into_host_buffer is a stub and does not load a real image.\n");
    *out_data = NULL;
    return 0;
}

// --- Initialization ---
int init_compute_resources() {
    g_invert_pipeline = SituationCreateComputePipelineFromMemory(invert_compute_shader_source);
    if (g_invert_pipeline.id == 0) {
        fprintf(stderr, "Failed to create compute pipeline: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    uint8_t* image_data = NULL;
    g_pixel_count = load_image_data_into_host_buffer("assets/textures/input_image.png", &image_data);
    if (g_pixel_count == 0 || image_data == NULL) {
        return -1;
    }
    size_t image_data_size = g_pixel_count * sizeof(uint32_t);

    g_input_ssbo = SituationCreateBuffer(image_data_size, image_data, SITUATION_BUFFER_USAGE_STORAGE_BUFFER);
    if (g_input_ssbo.id == 0) {
        fprintf(stderr, "Failed to create input SSBO: %s\n", SituationGetLastErrorMsg());
        free(image_data);
        return -1;
    }

    g_output_ssbo = SituationCreateBuffer(image_data_size, NULL, SITUATION_BUFFER_USAGE_STORAGE_BUFFER);
    if (g_output_ssbo.id == 0) {
        fprintf(stderr, "Failed to create output SSBO: %s\n", SituationGetLastErrorMsg());
        free(image_data);
        return -1;
    }

    free(image_data);
    return 0;
}

// --- Cleanup ---
void cleanup_compute_resources() {
    if (g_output_ssbo.id != 0) SituationDestroyBuffer(&g_output_ssbo);
    if (g_input_ssbo.id != 0) SituationDestroyBuffer(&g_input_ssbo);
    if (g_invert_pipeline.id != 0) SituationDestroyComputePipeline(&g_invert_pipeline);
}

// --- Run Compute Shader ---
void run_compute_shader() {
    if (g_pixel_count == 0) return;

    if (!SituationAcquireFrameCommandBuffer()) return;
    SituationCommandBuffer cmd = SituationGetMainCommandBuffer();

    SituationCmdBindComputePipeline(cmd, g_invert_pipeline);
    SituationCmdBindComputeBuffer(cmd, 0, g_input_ssbo);
    SituationCmdBindComputeBuffer(cmd, 1, g_output_ssbo);
    SituationCmdSetPushConstant(cmd, 0, &g_pixel_count, sizeof(g_pixel_count));
    SituationCmdDispatch(cmd, g_pixel_count, 1, 1);
    SituationMemoryBarrier(SITUATION_BARRIER_COMPUTE_SHADER_STORAGE_WRITE);

    SituationEndFrame();
}

// --- Main Application Entry ---
int main(int argc, char* argv[]) {
    SituationInitInfo init_info = {0};
    init_info.window_title = "situation.h - Compute Shader SSBO Example";

    SituationError err = SituationInit(argc, argv, &init_info);
    if (err != SITUATION_SUCCESS) {
        fprintf(stderr, "Failed to initialize: %s\n", SituationGetLastErrorMsg());
        return -1;
    }

    if (init_compute_resources() != 0) {
        printf("Note: Compute resources failed to initialize as expected (stub image loader).\n");
    }

    run_compute_shader();

    printf("Compute shader run complete (no-op). Press Enter to exit.\n");
    getchar();

    cleanup_compute_resources();
    SituationShutdown();
    return 0;
}
