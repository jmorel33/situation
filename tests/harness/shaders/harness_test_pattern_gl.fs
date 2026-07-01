#version 450
#define SIT_TP_CONFIG_UBO_SET 0
#define SIT_TP_CONFIG_UBO_BINDING 0
#define SIT_TP_LAYER_PARAMS_SSBO_SET 1
#define SIT_TP_LAYER_PARAMS_SSBO_BINDING 1
#include "sit/gpu/test_patterns/sit_tp_config_ubo.glslh"
#include "sit/gpu/test_patterns/sit_test_patterns.glslh"

layout(location = 0) out vec4 fragColor;

void main() {
    SitTpConfig cfg = sit_tp_config_from_ubo();
    vec2 uv = gl_FragCoord.xy / vec2(max(cfg.width, 1.0), max(cfg.height, 1.0));
    SitTpPalette pal = sit_tp_palette_from_ssbo();
    fragColor = vec4(sit_tp_sample(uv, cfg, pal), 1.0);
}
