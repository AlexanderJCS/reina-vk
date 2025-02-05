#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_debug_printf : require
#extension GL_GOOGLE_include_directive : require

layout(location = 0) rayPayloadEXT vec4 pld;

void main() {
    pld = vec4(1);
    // todo: change ray origin, direction
}