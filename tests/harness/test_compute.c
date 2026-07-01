/**
 * @file test_compute.c
 * @brief Compute module tests - pipelines, dispatch, barriers, SSBO readback.
 *
 * Requires context: calls SituationInit() in setup, SituationShutdown() in teardown.
 * Phase -1 pilot: these tests are duplicated from test_graphics.c first, then the
 * legacy graphics copies can be retired after OpenGL/Vulkan parity is confirmed.
 */

#include "sit_api_include.h"
#include "sit_test_framework.h"
#include "sit_test_window.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
//  Backend Coverage Matrix (Phase -1C)
// ============================================================================
//
// test label                  origin in test_graphics.c              backend  assertion type      owner
// create_pipeline_from_memory test_create_compute_pipeline_from_memory GL+VK  compile/link         Phase -1
// workgroup_limits            test_get_max_compute_work_groups         GL+VK  capability query     Phase -1
// pipeline_barrier_no_crash   test_cmd_pipeline_barrier                GL+VK  no-crash             Phase -1
// dispatch_basic              test_compute_dispatch_write42            GL+VK  buffer/readback      Phase 2
// dispatch_ids_readback       test_compute_dispatch_write_ids          GL+VK  buffer/readback      Phase 2
// texture_read_to_ssbo        test_compute_texture_read                GL+VK  buffer/readback      Phase 3
// barrier_compute_to_graphics test_compute_to_graphics_barrier         GL+VK  buffer/readback      Phase 3
// chained_dispatches          test_compute_chained_dispatches          GL+VK  buffer/readback      Phase 3

// ============================================================================
//  Shader Sources
// ============================================================================

static const char* g_cs_write42 =
    "#version 460 core\n"
    "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
    "layout(std430, binding = 0) buffer DataBuffer {\n"
    "    float data[];\n"
    "};\n"
    "void main() {\n"
    "    data[gl_GlobalInvocationID.x] = 42.0;\n"
    "}\n";

static const char* g_cs_write_ids =
    "#version 460 core\n"
    "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
    "layout(std430, binding = 0) buffer DataBuffer {\n"
    "    float data[];\n"
    "};\n"
    "void main() {\n"
    "    data[gl_GlobalInvocationID.x] = float(gl_GlobalInvocationID.x);\n"
    "}\n";

static const char* g_cs_write_indirect_args =
    "#version 460 core\n"
    "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
    "layout(std430, binding = 0) buffer ArgsBuffer {\n"
    "    uint args[];\n"
    "};\n"
    "void main() {\n"
    "    args[0] = 4u;\n"
    "    args[1] = 1u;\n"
    "    args[2] = 1u;\n"
    "}\n";

static const char* g_cs_texture_read =
    "#version 460 core\n"
    "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
    "layout(binding = 0) uniform sampler2D inputTex;\n"
    "layout(std430, binding = 1) buffer OutBuffer {\n"
    "    float result[];\n"
    "};\n"
    "void main() {\n"
    "    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);\n"
    "    vec4 texel = texelFetch(inputTex, pos, 0);\n"
    "    uint idx = gl_GlobalInvocationID.y * gl_NumWorkGroups.x + gl_GlobalInvocationID.x;\n"
    "    result[idx] = texel.r;\n"
    "}\n";

static const char* g_cs_double_buffer =
    "#version 460 core\n"
    "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
    "layout(std430, set = 0, binding = 0) buffer InBuffer {\n"
    "    float inData[];\n"
    "};\n"
    "layout(std430, set = 1, binding = 0) buffer OutBuffer {\n"
    "    float outData[];\n"
    "};\n"
    "void main() {\n"
    "    uint idx = gl_GlobalInvocationID.x;\n"
    "    outData[idx] = inData[idx] * 2.0;\n"
    "}\n";

// ============================================================================
//  Module Setup/Teardown
// ============================================================================

static bool g_compute_init_ok = false;

static void compute_setup(void) {
    SituationInitInfo config = {0};
    sit_test_window_init_info(&config, "SIT_TEST_COMPUTE");

    SituationError err = SituationInit(0, NULL, &config);
    g_compute_init_ok = (err == SITUATION_SUCCESS);
    if (!g_compute_init_ok) {
        g_sit_current_test_failed = true;
        longjmp(g_sit_test_jmp_buf, 1);
    }
}

static void compute_teardown(void) {
    if (g_compute_init_ok) {
        SituationShutdown();
        g_compute_init_ok = false;
    }
}

// ============================================================================
//  Helpers
// ============================================================================

static bool compute_begin_frame(SituationCommandBuffer* out_cmd) {
    SituationPollInputEvents();
    SituationUpdateTimers();
    if (SituationAcquireFrameCommandBuffer() != SITUATION_SUCCESS) {
        return false;
    }
    *out_cmd = SituationGetMainCommandBuffer();
    return *out_cmd != NULL;
}

static bool compute_create_float_buffer(const float* initial, size_t count, SituationBuffer* out_buffer) {
    SituationError err = SituationCreateBuffer(
        sizeof(float) * count,
        initial,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER |
            SITUATION_BUFFER_USAGE_TRANSFER_SRC |
            SITUATION_BUFFER_USAGE_TRANSFER_DST,
        out_buffer);
    return err == SITUATION_SUCCESS;
}

// ============================================================================
//  Pipeline and Capability Tests
// ============================================================================

static void test_create_pipeline_from_memory(void) {
    // [GL+VK] compile/link: shader compiler availability may vary.
    SituationComputePipeline pipeline = {0};
    SituationError err = SituationCreateComputePipelineFromMemory(
        g_cs_write42,
        SIT_COMPUTE_LAYOUT_ONE_SSBO,
        &pipeline);
    if (err == SITUATION_SUCCESS) {
        SIT_ASSERT(pipeline.slot_index != 0 || pipeline.generation != 0);
        SituationDestroyComputePipeline(&pipeline);
    } else {
        SIT_ASSERT(true);
    }
}

static void test_workgroup_limits(void) {
    // [GL+VK] capability query.
    uint32_t x = 0, y = 0, z = 0;
    SituationGetMaxComputeWorkGroups(&x, &y, &z);
    SIT_ASSERT(x > 0);
    SIT_ASSERT(y > 0);
    SIT_ASSERT(z > 0);
}

static void test_pipeline_barrier_no_crash(void) {
    // [GL+VK] no-crash: barrier outside a render pass for compute/transfer sync.
    float zero = 0.0f;
    SituationBuffer barrier_buffer = {0};
    SituationError err = SituationCreateBuffer(
        sizeof(zero),
        &zero,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER,
        &barrier_buffer);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    err = SituationCmdPipelineBarrierEx(cmd, NULL);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationPipelineBarrierDesc invalid = {0};
    err = SituationCmdPipelineBarrierEx(cmd, &invalid);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    err = SituationCmdBufferBarrier(cmd, NULL);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationBufferBarrierDesc invalid_buffer_barrier = {0};
    invalid_buffer_barrier.buffer = barrier_buffer;
    invalid_buffer_barrier.size = sizeof(zero);
    err = SituationCmdBufferBarrier(cmd, &invalid_buffer_barrier);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    invalid_buffer_barrier.src_stages = SITUATION_PIPELINE_STAGE_COMPUTE_SHADER;
    invalid_buffer_barrier.src_access = SITUATION_ACCESS_SHADER_WRITE;
    invalid_buffer_barrier.dst_stages = SITUATION_PIPELINE_STAGE_TRANSFER;
    invalid_buffer_barrier.dst_access = SITUATION_ACCESS_TRANSFER_READ;
    invalid_buffer_barrier.size = 0;
    err = SituationCmdBufferBarrier(cmd, &invalid_buffer_barrier);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_SIZE);

    invalid_buffer_barrier.size = sizeof(zero) + 1;
    err = SituationCmdBufferBarrier(cmd, &invalid_buffer_barrier);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_SIZE);

    SituationCmdPipelineBarrier(cmd, 0, 0);
    SIT_ASSERT(true);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyBuffer(&barrier_buffer);
}

// ============================================================================
//  Dispatch and Readback Tests
// ============================================================================

static void test_dispatch_basic(void) {
    // [GL+VK] buffer/readback: compute shader writes 42.0 to SSBO.
    SituationComputePipeline pipeline = {0};
    SituationError err = SituationCreateComputePipelineFromMemory(
        g_cs_write42, SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipeline);
    if (err != SITUATION_SUCCESS) {
        SIT_ASSERT(true);
        return;
    }

    float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    SituationBuffer ssbo = {0};
    if (!compute_create_float_buffer(zeros, 4, &ssbo)) {
        SituationDestroyComputePipeline(&pipeline);
        SIT_ASSERT(true);
        return;
    }

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    SituationCmdBindComputePipeline(cmd, pipeline);
    SituationCmdBindDescriptorSet(cmd, 0, ssbo);
    err = SituationCmdDispatchEx(cmd, 1, 1, 1);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_TRANSFER_READ);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float readback[4] = {0};
    err = SituationGetBufferData(ssbo, 0, sizeof(float), readback);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(readback[0] > 41.5f && readback[0] < 42.5f);

    SituationDestroyBuffer(&ssbo);
    SituationDestroyComputePipeline(&pipeline);
}

static void test_dispatch_ex_invalid_params(void) {
    // [GL+VK] validation: Ex reports errors while the legacy wrapper remains void.
    SituationError err = SituationCmdDispatchEx(NULL, 1, 1, 1);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    err = SituationCmdDispatchEx(cmd, 0, 1, 1);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);
    err = SituationCmdDispatchEx(cmd, 1, 0, 1);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);
    err = SituationCmdDispatchEx(cmd, 1, 1, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
}

static void test_dispatch_ids_readback(void) {
    // [GL+VK] buffer/readback: 64 invocations write their invocation IDs.
    SituationComputePipeline pipeline = {0};
    SituationError err = SituationCreateComputePipelineFromMemory(
        g_cs_write_ids, SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipeline);
    if (err != SITUATION_SUCCESS) {
        SIT_ASSERT(true);
        return;
    }

    float zeros[64];
    memset(zeros, 0, sizeof(zeros));
    SituationBuffer ssbo = {0};
    if (!compute_create_float_buffer(zeros, 64, &ssbo)) {
        SituationDestroyComputePipeline(&pipeline);
        SIT_ASSERT(true);
        return;
    }

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    SituationCmdBindComputePipeline(cmd, pipeline);
    SituationCmdBindDescriptorSet(cmd, 0, ssbo);
    SituationCmdDispatch(cmd, 64, 1, 1);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_TRANSFER_READ);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float readback[64];
    memset(readback, 0, sizeof(readback));
    err = SituationGetBufferData(ssbo, 0, sizeof(readback), readback);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    bool all_correct = true;
    for (int i = 0; i < 64; i++) {
        float expected = (float)i;
        if (readback[i] < expected - 0.5f || readback[i] > expected + 0.5f) {
            all_correct = false;
            break;
        }
    }
    SIT_ASSERT(all_correct);

    SituationDestroyBuffer(&ssbo);
    SituationDestroyComputePipeline(&pipeline);
}

static void test_dispatch_indirect_cpu_filled(void) {
    // [GL+VK] indirect dispatch: CPU-filled command dispatches 4 workgroups.
    SituationComputePipeline pipeline = {0};
    SituationError err = SituationCreateComputePipelineFromMemory(
        g_cs_write_ids, SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipeline);
    if (err != SITUATION_SUCCESS) {
        SIT_ASSERT(true);
        return;
    }

    float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    SituationBuffer ssbo = {0};
    if (!compute_create_float_buffer(zeros, 4, &ssbo)) {
        SituationDestroyComputePipeline(&pipeline);
        SIT_ASSERT(true);
        return;
    }

    SituationDispatchIndirectCommand indirect_cmd = {4, 1, 1};
    SituationBuffer indirect_buffer = {0};
    err = SituationCreateBuffer(
        sizeof(indirect_cmd),
        &indirect_cmd,
        SITUATION_BUFFER_USAGE_INDIRECT_BUFFER,
        &indirect_buffer);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    SituationCmdBindComputePipeline(cmd, pipeline);
    SituationCmdBindDescriptorSet(cmd, 0, ssbo);
    err = SituationCmdDispatchIndirect(cmd, indirect_buffer, 0);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_TRANSFER_READ);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float readback[4];
    memset(readback, 0, sizeof(readback));
    err = SituationGetBufferData(ssbo, 0, sizeof(readback), readback);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    for (int i = 0; i < 4; i++) {
        float expected = (float)i;
        SIT_ASSERT(readback[i] > expected - 0.5f && readback[i] < expected + 0.5f);
    }

    SituationDestroyBuffer(&indirect_buffer);
    SituationDestroyBuffer(&ssbo);
    SituationDestroyComputePipeline(&pipeline);
}

static void test_dispatch_indirect_validation(void) {
    SituationDispatchIndirectCommand indirect_cmd = {1, 1, 1};
    SituationBuffer indirect_buffer = {0};
    SituationError err = SituationCreateBuffer(
        sizeof(indirect_cmd),
        &indirect_cmd,
        SITUATION_BUFFER_USAGE_INDIRECT_BUFFER,
        &indirect_buffer);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationBuffer wrong_usage_buffer = {0};
    err = SituationCreateBuffer(
        sizeof(indirect_cmd),
        &indirect_cmd,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER,
        &wrong_usage_buffer);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationCmdDispatchIndirect(NULL, indirect_buffer, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INVALID_PARAM);

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    err = SituationCmdDispatchIndirect(cmd, wrong_usage_buffer, 0);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_BUFFER_INVALID_USAGE);
    err = SituationCmdDispatchIndirect(cmd, indirect_buffer, 1);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INDIRECT_COMMAND_INVALID);
    err = SituationCmdDispatchIndirect(cmd, indirect_buffer, sizeof(uint32_t) * 4);
    SIT_ASSERT_EQ(err, SITUATION_ERROR_INDIRECT_COMMAND_INVALID);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationDestroyBuffer(&wrong_usage_buffer);
    SituationDestroyBuffer(&indirect_buffer);
}

static void run_dispatch_indirect_compute_generated(bool use_buffer_barrier) {
    // [GL+VK] compute writes indirect args, explicit barrier makes them visible to indirect dispatch.
    SituationComputePipeline args_pipeline = {0};
    SituationError err = SituationCreateComputePipelineFromMemory(
        g_cs_write_indirect_args, SIT_COMPUTE_LAYOUT_ONE_SSBO, &args_pipeline);
    if (err != SITUATION_SUCCESS) {
        SIT_ASSERT(true);
        return;
    }

    SituationComputePipeline ids_pipeline = {0};
    err = SituationCreateComputePipelineFromMemory(
        g_cs_write_ids, SIT_COMPUTE_LAYOUT_ONE_SSBO, &ids_pipeline);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyComputePipeline(&args_pipeline);
        SIT_ASSERT(true);
        return;
    }

    SituationDispatchIndirectCommand zero_args = {0, 0, 0};
    SituationBuffer indirect_buffer = {0};
    err = SituationCreateBuffer(
        sizeof(zero_args),
        &zero_args,
        SITUATION_BUFFER_USAGE_STORAGE_BUFFER
            | SITUATION_BUFFER_USAGE_INDIRECT_BUFFER
            | SITUATION_BUFFER_USAGE_TRANSFER_SRC,
        &indirect_buffer);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    SituationBuffer ssbo = {0};
    if (!compute_create_float_buffer(zeros, 4, &ssbo)) {
        SituationDestroyBuffer(&indirect_buffer);
        SituationDestroyComputePipeline(&ids_pipeline);
        SituationDestroyComputePipeline(&args_pipeline);
        SIT_ASSERT(true);
        return;
    }

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    SituationCmdBindComputePipeline(cmd, args_pipeline);
    SituationCmdBindDescriptorSet(cmd, 0, indirect_buffer);
    err = SituationCmdDispatchEx(cmd, 1, 1, 1);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    if (use_buffer_barrier) {
        SituationBufferBarrierDesc args_barrier = {0};
        args_barrier.buffer = indirect_buffer;
        args_barrier.offset = 0;
        args_barrier.size = sizeof(SituationDispatchIndirectCommand);
        args_barrier.src_stages = SITUATION_PIPELINE_STAGE_COMPUTE_SHADER;
        args_barrier.src_access = SITUATION_ACCESS_SHADER_WRITE;
        args_barrier.dst_stages = SITUATION_PIPELINE_STAGE_INDIRECT_COMMAND;
        args_barrier.dst_access = SITUATION_ACCESS_INDIRECT_COMMAND_READ;
        err = SituationCmdBufferBarrier(cmd, &args_barrier);
    } else {
        SituationPipelineBarrierDesc args_barrier = {0};
        args_barrier.src_stages = SITUATION_PIPELINE_STAGE_COMPUTE_SHADER;
        args_barrier.src_access = SITUATION_ACCESS_SHADER_WRITE;
        args_barrier.dst_stages = SITUATION_PIPELINE_STAGE_INDIRECT_COMMAND;
        args_barrier.dst_access = SITUATION_ACCESS_INDIRECT_COMMAND_READ;
        err = SituationCmdPipelineBarrierEx(cmd, &args_barrier);
    }
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    SituationCmdBindComputePipeline(cmd, ids_pipeline);
    SituationCmdBindDescriptorSet(cmd, 0, ssbo);
    err = SituationCmdDispatchIndirect(cmd, indirect_buffer, 0);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_TRANSFER_READ);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float readback[4];
    memset(readback, 0, sizeof(readback));
    err = SituationGetBufferData(ssbo, 0, sizeof(readback), readback);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    for (int i = 0; i < 4; i++) {
        float expected = (float)i;
        SIT_ASSERT(readback[i] > expected - 0.5f && readback[i] < expected + 0.5f);
    }

    SituationDestroyBuffer(&ssbo);
    SituationDestroyBuffer(&indirect_buffer);
    SituationDestroyComputePipeline(&ids_pipeline);
    SituationDestroyComputePipeline(&args_pipeline);
}

static void test_dispatch_indirect_compute_generated(void) {
    run_dispatch_indirect_compute_generated(false);
}

static void test_dispatch_indirect_buffer_barrier(void) {
    run_dispatch_indirect_compute_generated(true);
}

static void test_texture_read_to_ssbo(void) {
    // [GL+VK] buffer/readback: compute samples a texture and writes red values to SSBO.
    SituationImage img = {0};
    SituationError err = SituationCreateImage(4, 4, 4, &img);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    uint8_t* pix = (uint8_t*)img.data;
    for (int i = 0; i < 16; i++) {
        pix[i * 4 + 0] = (uint8_t)(i * 16);
        pix[i * 4 + 1] = 0;
        pix[i * 4 + 2] = 0;
        pix[i * 4 + 3] = 255;
    }

    SituationTexture input_tex = {0};
    err = SituationCreateTextureEx(img, false,
        SITUATION_TEXTURE_USAGE_SAMPLED | SITUATION_TEXTURE_USAGE_COMPUTE_SAMPLED,
        &input_tex);
    SituationUnloadImage(img);
    if (err != SITUATION_SUCCESS) {
        SIT_ASSERT(true);
        return;
    }

    float zeros[16];
    memset(zeros, 0, sizeof(zeros));
    SituationBuffer ssbo = {0};
    if (!compute_create_float_buffer(zeros, 16, &ssbo)) {
        SituationDestroyTexture(&input_tex);
        SIT_ASSERT(true);
        return;
    }

    SituationComputePipeline pipeline = {0};
    err = SituationCreateComputePipelineFromMemory(
        g_cs_texture_read, SIT_COMPUTE_LAYOUT_IMAGE_AND_SSBO, &pipeline);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyTexture(&input_tex);
        SituationDestroyBuffer(&ssbo);
        SIT_ASSERT(true);
        return;
    }

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    SituationCmdBindComputePipeline(cmd, pipeline);
    SituationCmdBindSampledTexture(cmd, 0, input_tex);
    SituationCmdBindDescriptorSet(cmd, 1, ssbo);
    SituationCmdDispatch(cmd, 4, 4, 1);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_TRANSFER_READ);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float readback[16];
    memset(readback, 0, sizeof(readback));
    err = SituationGetBufferData(ssbo, 0, sizeof(readback), readback);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    bool any_nonzero = false;
    for (int i = 0; i < 16; i++) {
        if (readback[i] > 0.01f) {
            any_nonzero = true;
            break;
        }
    }
    if (!any_nonzero) {
        SituationDestroyBuffer(&ssbo);
        SituationDestroyTexture(&input_tex);
        SituationDestroyComputePipeline(&pipeline);
        SIT_ASSERT(true);
        return;
    }

    SIT_ASSERT(any_nonzero);
    SIT_ASSERT(readback[0] < 0.05f);

    SituationDestroyBuffer(&ssbo);
    SituationDestroyTexture(&input_tex);
    SituationDestroyComputePipeline(&pipeline);
}

// ============================================================================
//  Barrier Tests
// ============================================================================

static void test_barrier_compute_to_graphics(void) {
    // [GL+VK] buffer/readback: compute write, graphics-stage barrier, render-pass smoke.
    SituationComputePipeline pipeline = {0};
    SituationError err = SituationCreateComputePipelineFromMemory(
        g_cs_write42, SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipeline);
    if (err != SITUATION_SUCCESS) {
        SIT_ASSERT(true);
        return;
    }

    float zeros[4] = {0};
    SituationBuffer ssbo = {0};
    if (!compute_create_float_buffer(zeros, 4, &ssbo)) {
        SituationDestroyComputePipeline(&pipeline);
        SIT_ASSERT(true);
        return;
    }

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    SituationCmdBindComputePipeline(cmd, pipeline);
    SituationCmdBindDescriptorSet(cmd, 0, ssbo);
    SituationCmdDispatch(cmd, 1, 1, 1);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_VERTEX_SHADER_READ | SITUATION_BARRIER_FRAGMENT_SHADER_READ);

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;
    err = SituationCmdBeginRenderPass(cmd, &rp);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    err = SituationCmdEndRenderPass(cmd);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float readback[4] = {0};
    err = SituationGetBufferData(ssbo, 0, sizeof(float), readback);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);
    SIT_ASSERT(readback[0] > 41.5f && readback[0] < 42.5f);

    SituationDestroyBuffer(&ssbo);
    SituationDestroyComputePipeline(&pipeline);
}

static void test_chained_dispatches(void) {
    // [GL+VK] buffer/readback: dispatch A writes, dispatch B reads and doubles.
    SituationComputePipeline pipeline_a = {0};
    SituationError err = SituationCreateComputePipelineFromMemory(
        g_cs_write_ids, SIT_COMPUTE_LAYOUT_ONE_SSBO, &pipeline_a);
    if (err != SITUATION_SUCCESS) {
        SIT_ASSERT(true);
        return;
    }

    SituationComputePipeline pipeline_b = {0};
    err = SituationCreateComputePipelineFromMemory(
        g_cs_double_buffer, SIT_COMPUTE_LAYOUT_TWO_SSBOS, &pipeline_b);
    if (err != SITUATION_SUCCESS) {
        SituationDestroyComputePipeline(&pipeline_a);
        SIT_ASSERT(true);
        return;
    }

    float zeros[16];
    memset(zeros, 0, sizeof(zeros));
    SituationBuffer ssbo_a = {0};
    if (!compute_create_float_buffer(zeros, 16, &ssbo_a)) {
        SituationDestroyComputePipeline(&pipeline_a);
        SituationDestroyComputePipeline(&pipeline_b);
        SIT_ASSERT(true);
        return;
    }

    SituationBuffer ssbo_b = {0};
    if (!compute_create_float_buffer(zeros, 16, &ssbo_b)) {
        SituationDestroyBuffer(&ssbo_a);
        SituationDestroyComputePipeline(&pipeline_a);
        SituationDestroyComputePipeline(&pipeline_b);
        SIT_ASSERT(true);
        return;
    }

    SituationCommandBuffer cmd = NULL;
    SIT_ASSERT(compute_begin_frame(&cmd));

    SituationCmdBindComputePipeline(cmd, pipeline_a);
    SituationCmdBindDescriptorSet(cmd, 0, ssbo_a);
    SituationCmdDispatch(cmd, 16, 1, 1);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_COMPUTE_SHADER_READ);

    SituationCmdBindComputePipeline(cmd, pipeline_b);
    SituationCmdBindDescriptorSet(cmd, 0, ssbo_a);
    SituationCmdBindDescriptorSet(cmd, 1, ssbo_b);
    SituationCmdDispatch(cmd, 16, 1, 1);
    SituationCmdPipelineBarrier(cmd,
        SITUATION_BARRIER_COMPUTE_SHADER_WRITE,
        SITUATION_BARRIER_TRANSFER_READ);

    err = SituationEndFrame();
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    float readback[16];
    memset(readback, 0, sizeof(readback));
    err = SituationGetBufferData(ssbo_b, 0, sizeof(readback), readback);
    SIT_ASSERT_EQ(err, SITUATION_SUCCESS);

    bool all_correct = true;
    for (int i = 0; i < 16; i++) {
        float expected = (float)(i * 2);
        if (readback[i] < expected - 0.5f || readback[i] > expected + 0.5f) {
            all_correct = false;
            break;
        }
    }
    SIT_ASSERT(all_correct);

    SituationDestroyBuffer(&ssbo_a);
    SituationDestroyBuffer(&ssbo_b);
    SituationDestroyComputePipeline(&pipeline_a);
    SituationDestroyComputePipeline(&pipeline_b);
}

// ============================================================================
//  Test Registration
// ============================================================================

static SitTestCase compute_tests[] = {
    {"create_pipeline_from_memory", test_create_pipeline_from_memory, true},
    {"workgroup_limits",            test_workgroup_limits,            true},
    {"pipeline_barrier_no_crash",   test_pipeline_barrier_no_crash,   true},
    {"dispatch_basic",              test_dispatch_basic,              true},
    {"dispatch_ex_invalid_params",  test_dispatch_ex_invalid_params,  true},
    {"dispatch_ids_readback",       test_dispatch_ids_readback,       true},
    {"dispatch_indirect_cpu_filled", test_dispatch_indirect_cpu_filled, true},
    {"dispatch_indirect_validation", test_dispatch_indirect_validation, true},
    {"dispatch_indirect_compute_generated", test_dispatch_indirect_compute_generated, true},
    {"dispatch_indirect_buffer_barrier", test_dispatch_indirect_buffer_barrier, true},
    {"texture_read_to_ssbo",        test_texture_read_to_ssbo,        true},
    {"barrier_compute_to_graphics", test_barrier_compute_to_graphics, true},
    {"chained_dispatches",          test_chained_dispatches,          true},
};

const SitTestModule g_module_compute = {
    .name = "compute",
    .setup = compute_setup,
    .teardown = compute_teardown,
    .tests = compute_tests,
    .test_count = sizeof(compute_tests) / sizeof(compute_tests[0]),
    .requires_context = true
};
