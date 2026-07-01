#version 450
/* Vulkan: explicit sets match pipeline (set 0 = Frame UBO, set 1 = TagBlock SSBO). Verify: scripts/spirv_desc_spike.py */
layout(std140, set = 0, binding = 0) uniform Frame {
    vec4 color;
} frame;
layout(std430, set = 1, binding = 0) readonly buffer TagBlock {
    uint tagB;
} tags;
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(float(tags.tagB) / 255.0, frame.color.g, 0.0, 1.0);
}
