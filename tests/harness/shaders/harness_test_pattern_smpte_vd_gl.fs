#version 450
#define SIT_TP_SMPTE_VD_SUBSET 1
#include "sit/gpu/test_patterns/sit_tp_smpte.glslh"
#include "sit/gpu/test_patterns/sit_tp_smpte_vd_ubo.glslh"

layout(location = 0) out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(max(u_sit_tp_smpte_vd.width, 1.0), max(u_sit_tp_smpte_vd.height, 1.0));
    fragColor = vec4(_sit_smpte_color_bars(uv), 1.0);
}
