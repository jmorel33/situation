#ifndef SIT_HARNESS_PATTERN_UBO_H
#define SIT_HARNESS_PATTERN_UBO_H

/**
 * Harness wrappers — pack/init via SituationVdStandby* (situation_api_graphics.h).
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* SitVdStandbyConfig / SIT_VD_STANDBY_* — include sit_api_include.h before this header. */

#define SIT_HARNESS_PATTERN_CONFIG_UBO_SIZE SIT_VD_STANDBY_CONFIG_UBO_SIZE
#define SIT_HARNESS_PATTERN_PARAMS_SSBO_SIZE SIT_VD_STANDBY_PARAMS_SSBO_SIZE
#define SIT_HARNESS_PATTERN_SMPTE_VD_UBO_SIZE 16u

#define SIT_HARNESS_PATTERN_UBO_OFF_PATTERN_LAYERS SIT_VD_STANDBY_CONFIG_UBO_OFF_PATTERN_LAYERS
#define SIT_HARNESS_PATTERN_UBO_OFF_WIDTH SIT_VD_STANDBY_CONFIG_UBO_OFF_WIDTH
#define SIT_HARNESS_PATTERN_UBO_OFF_HEIGHT SIT_VD_STANDBY_CONFIG_UBO_OFF_HEIGHT
#define SIT_HARNESS_PATTERN_UBO_OFF_SHOW_OVERLAY SIT_VD_STANDBY_CONFIG_UBO_OFF_SHOW_OVERLAY
#define SIT_HARNESS_PATTERN_UBO_OFF_CHECKER_SIZE_X SIT_VD_STANDBY_CONFIG_UBO_OFF_CHECKER_SIZE_X
#define SIT_HARNESS_PATTERN_UBO_OFF_CHECKER_SIZE_Y SIT_VD_STANDBY_CONFIG_UBO_OFF_CHECKER_SIZE_Y
#define SIT_HARNESS_PATTERN_UBO_OFF_STRIPE_WIDTH SIT_VD_STANDBY_CONFIG_UBO_OFF_STRIPE_WIDTH
#define SIT_HARNESS_PATTERN_UBO_OFF_FREQUENCIES SIT_VD_STANDBY_CONFIG_UBO_OFF_FREQUENCIES
#define SIT_HARNESS_PATTERN_UBO_OFF_NUM_FREQUENCIES SIT_VD_STANDBY_CONFIG_UBO_OFF_NUM_FREQUENCIES
#define SIT_HARNESS_PATTERN_UBO_OFF_GRID_SIZE SIT_VD_STANDBY_CONFIG_UBO_OFF_GRID_SIZE
#define SIT_HARNESS_PATTERN_UBO_OFF_NOISE_FRAME_SEED SIT_VD_STANDBY_CONFIG_UBO_OFF_NOISE_FRAME_SEED

typedef struct SitHarnessPatternSmpteVdConfig {
    float width;
    float height;
} SitHarnessPatternSmpteVdConfig;

static inline uint32_t sit_harness_pattern_layer_bit(SitVdStandbyLayer layer) {
    return SituationVdStandbyLayerBit((int)layer);
}

static inline void sit_harness_pattern_config_init_defaults(
    SitVdStandbyConfig* cfg, int layer_index, float w, float h)
{
    SituationVdStandbyConfigInitDefaults(cfg, layer_index, w, h);
}

static inline void sit_harness_pattern_toggle_layer(
    SitVdStandbyConfig* cfg, SitVdStandbyLayer layer, bool enabled)
{
    SituationVdStandbyToggleLayer(cfg, (int)layer, enabled);
}

static inline void sit_harness_pattern_pack_config_std140(
    uint8_t out[SIT_HARNESS_PATTERN_CONFIG_UBO_SIZE], const SitVdStandbyConfig* cfg)
{
    SituationVdStandbyPackStd140(out, cfg);
}

static inline void sit_harness_pattern_pack_params_std430(
    uint8_t out[SIT_HARNESS_PATTERN_PARAMS_SSBO_SIZE], const SitVdStandbyConfig* cfg)
{
    SituationVdStandbyPackParamsStd430(out, cfg);
}

static inline void sit_harness_pattern_pack_smpte_vd_std140(
    uint8_t out[SIT_HARNESS_PATTERN_SMPTE_VD_UBO_SIZE],
    const SitHarnessPatternSmpteVdConfig* cfg)
{
    memset(out, 0, SIT_HARNESS_PATTERN_SMPTE_VD_UBO_SIZE);
    memcpy(out, &cfg->width, sizeof(cfg->width));
    memcpy(out + 4u, &cfg->height, sizeof(cfg->height));
}

#endif /* SIT_HARNESS_PATTERN_UBO_H */
