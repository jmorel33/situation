#version 450 core
layout(std430, binding = 0) readonly buffer BlockA {
    uint tagA;
};
layout(std430, binding = 1) readonly buffer BlockB {
    uint tagB;
};
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(float(tagA) / 255.0, float(tagB) / 255.0, 0.0, 1.0);
}
