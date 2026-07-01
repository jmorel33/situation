#version 450 core
layout(location = 0) in vec3 inPos;
void main() {
    /* Near clip plane (NDC z=0): GL_LESS vs cleared depth 1.0 always passes; avoids
       far-plane / precision edge cases on some drivers with z ~= 1. */
    gl_Position = vec4(inPos.xy, 0.0, 1.0);
}
