#ifndef SIT_HARNESS_TEST_PATTERN_HELPERS_H
#define SIT_HARNESS_TEST_PATTERN_HELPERS_H

#include <stddef.h>
#include <stdint.h>

#include "sit_api_include.h"
#include "sit_harness_pattern_ubo.h"

/** Harness-only UBO/SSBO bind/draw helpers for test_graphics_patterns.c (not library API). */
SituationError sit_harness_test_pattern_bind_config_resources(SituationShader shader, bool smpte_vd_subset);
SituationError sit_harness_test_pattern_upload_config_ubo(
    SituationBuffer ubo,
    const uint8_t* std140_bytes,
    size_t std140_size);
SituationError sit_harness_test_pattern_upload_params_ssbo(
    SituationBuffer ssbo,
    const uint8_t* std430_bytes,
    size_t std430_size);
SituationError sit_harness_test_pattern_draw_fullscreen_ubo(
    SituationCommandBuffer cmd,
    SituationShader shader,
    SituationMesh mesh,
    SituationBuffer ubo,
    const uint8_t* std140_bytes,
    size_t std140_size);
SituationError sit_harness_test_pattern_draw_fullscreen_config(
    SituationCommandBuffer cmd,
    SituationShader shader,
    SituationMesh mesh,
    SituationBuffer ubo,
    SituationBuffer ssbo,
    const uint8_t* header_bytes,
    const uint8_t* params_bytes,
    size_t header_size,
    size_t params_size);

#endif /* SIT_HARNESS_TEST_PATTERN_HELPERS_H */
