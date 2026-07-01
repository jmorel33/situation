/*
 * sit_harness_test_pattern_helpers.c
 *
 * Test-harness draw helpers for graphics pattern readback tests.
 * Lives in tests/harness/ only — not linked into situation_*.dll / .a.
 */

#include "sit_harness_test_pattern_helpers.h"

SituationError sit_harness_test_pattern_bind_config_resources(SituationShader shader, bool smpte_vd_subset) {
#if defined(SITUATION_USE_OPENGL)
    const char* block_name = smpte_vd_subset ? "SitTpSmpteVdBlock" : "SitTpConfigBlock";
    SituationError err = SituationBindUniformBlock(shader, block_name, 0u);
    if (err != SITUATION_SUCCESS || smpte_vd_subset) {
        return err;
    }
    return SituationBindShaderStorageBlock(shader, "SitTpLayerParamsBlock", 1u);
#else
    (void)shader;
    (void)smpte_vd_subset;
    return SITUATION_SUCCESS;
#endif
}

SituationError sit_harness_test_pattern_upload_config_ubo(
    SituationBuffer ubo,
    const uint8_t* std140_bytes,
    size_t std140_size)
{
    if (ubo.generation == 0 || !std140_bytes || std140_size == 0u) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (std140_size != SIT_HARNESS_PATTERN_CONFIG_UBO_SIZE && std140_size != SIT_HARNESS_PATTERN_SMPTE_VD_UBO_SIZE) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    return SituationUpdateBuffer(ubo, 0u, std140_size, std140_bytes);
}

SituationError sit_harness_test_pattern_upload_params_ssbo(
    SituationBuffer ssbo,
    const uint8_t* std430_bytes,
    size_t std430_size)
{
    if (ssbo.generation == 0 || !std430_bytes || std430_size == 0u) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    if (std430_size != SIT_HARNESS_PATTERN_PARAMS_SSBO_SIZE) {
        return SITUATION_ERROR_INVALID_PARAM;
    }
    return SituationUpdateBuffer(ssbo, 0u, std430_size, std430_bytes);
}

SituationError sit_harness_test_pattern_draw_fullscreen_ubo(
    SituationCommandBuffer cmd,
    SituationShader shader,
    SituationMesh mesh,
    SituationBuffer ubo,
    const uint8_t* std140_bytes,
    size_t std140_size)
{
    return sit_harness_test_pattern_draw_fullscreen_config(
        cmd, shader, mesh, ubo, (SituationBuffer){0}, std140_bytes, NULL, std140_size, 0u);
}

SituationError sit_harness_test_pattern_draw_fullscreen_config(
    SituationCommandBuffer cmd,
    SituationShader shader,
    SituationMesh mesh,
    SituationBuffer ubo,
    SituationBuffer ssbo,
    const uint8_t* header_bytes,
    const uint8_t* params_bytes,
    size_t header_size,
    size_t params_size)
{
    if (!cmd || shader.generation == 0 || mesh.generation == 0) {
        return SITUATION_ERROR_INVALID_PARAM;
    }

    SituationError err = sit_harness_test_pattern_upload_config_ubo(ubo, header_bytes, header_size);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    if (params_bytes && params_size > 0u) {
        err = sit_harness_test_pattern_upload_params_ssbo(ssbo, params_bytes, params_size);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }

    SituationRenderPassInfo rp = {0};
    rp.display_id = -1;
    rp.color_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.color_attachment.storeOp = SIT_STORE_OP_STORE;
    rp.color_attachment.clear.color = (ColorRGBA){0, 0, 0, 255};
    rp.depth_attachment.loadOp = SIT_LOAD_OP_CLEAR;
    rp.depth_attachment.clear.depth = 1.0f;

    err = SituationCmdBeginRenderPass(cmd, &rp);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    SituationCmdBindPipeline(cmd, shader);

    err = SituationCmdBindDescriptorSet(cmd, 0u, ubo);
    if (err != SITUATION_SUCCESS) {
        return err;
    }
    if (params_bytes && params_size > 0u && ssbo.generation != 0) {
        err = SituationCmdBindDescriptorSet(cmd, 1u, ssbo);
        if (err != SITUATION_SUCCESS) {
            return err;
        }
    }

    SituationCmdDrawMesh(cmd, mesh);

    err = SituationCmdEndRenderPass(cmd);
    if (err != SITUATION_SUCCESS) {
        return err;
    }

    return SituationEndFrame();
}
