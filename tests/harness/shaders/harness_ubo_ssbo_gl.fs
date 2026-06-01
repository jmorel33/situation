#version 450 core
layout(std140, binding = 0) uniform Frame {
    vec4 color;
} frame;
layout(std430, binding = 1) readonly buffer TagBlock {
    uint tagB;
} tags;
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(float(tags.tagB) / 255.0, frame.color.g, 0.0, 1.0);
}
