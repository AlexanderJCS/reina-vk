#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadEXT vec3 payload;

void main() {
    // Example: simple background color (sky blue)
    payload = vec3(0.5, 0.7, 1.0);
}