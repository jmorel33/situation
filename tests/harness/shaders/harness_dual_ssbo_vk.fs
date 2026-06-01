#version 450
layout(std430, set = 0, binding = 0) readonly buffer BlockA {
    uint tagA;
};
layout(std430, set = 1, binding = 0) readonly buffer BlockB {
    uint tagB;
};
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(float(tagA) / 255.0, float(tagB) / 255.0, 0.0, 1.0);
}
