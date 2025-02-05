#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 pld;

void main() {
    pld = vec4(0.5, 0.5, 0, 0);
}