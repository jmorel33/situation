#version 450
/* UBO_SSBO_SAMPLER harness fragment shader — Vulkan target only (no GL variant).
 * Pipeline profile: SIT_SPIRV_LAYOUT_PROFILE_UBO_SSBO_SAMPLER
 *   set 0, binding 0 — Frame UBO      (SituationCmdBindDescriptorSet)
 *   set 1, binding 0 — TagBlock SSBO  (SituationCmdBindDescriptorSet)
 *   set 2, binding 0 — feedback tex   (SituationCmdBindTextureSet)
 *
 * Output encoding:
 *   R = float(tags.tagB) / 255.0      — verifies SSBO read
 *   G = frame.color.g                  — verifies UBO read (expect 1.0 → 255)
 *   B = texture(feedback, 0.5).r       — verifies sampler bind (carries texture R channel)
 *   A = 1.0
 *
 * Compile (no -fauto-map-locations):
 *   glslc --target-env=vulkan -std=450 -O -fshader-stage=fragment \
 *         harness_ubo_ssbo_sampler_vk.fs -o spirv_out/harness_ubo_ssbo_sampler_vk.fs.spv
 * Verify: python scripts/spirv_desc_spike.py (expect set/binding: (0,0),(1,0),(2,0))
 */

layout(std140, set = 0, binding = 0) uniform Frame {
    vec4 color;
} frame;

layout(std430, set = 1, binding = 0) readonly buffer TagBlock {
    uint tagB;
} tags;

layout(set = 2, binding = 0) uniform sampler2D feedback;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 samp = texture(feedback, vec2(0.5, 0.5));
    fragColor = vec4(
        float(tags.tagB) / 255.0,
        frame.color.g,
        samp.r,
        1.0
    );
}
