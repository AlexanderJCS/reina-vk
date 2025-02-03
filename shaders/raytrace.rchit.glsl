#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

layout(location = 0) rayPayloadEXT vec3 pld;

void main() {
    pld = vec3(0.9, 0, 0);
    // todo: change ray origin, direction
}