#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

// The payload:
layout(location = 0) rayPayloadInEXT vec3 pld;

void main() {
//    float dir = normalize(gl_WorldRayDirectionEXT.y) / 2 + 0.5;
//    pld = mix(vec3(0.1, 0.1, 0.1), vec3(0.5, 0.5, 0.9), dir);
    pld = vec3(0.1, 0.1, 0.6);
}