#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "shaderCommon.h.glsl"

// The payload:
layout(location = 0) rayPayloadInEXT PassableInfo pld;

void main() {
    const float rayDirY = normalize(gl_WorldRayDirectionEXT).y;
    float t = 0.5 * (rayDirY + 1.0);
//    pld.color = mix(vec3(1), vec3(0.5, 0.7, 1), t);
    pld.color = vec3(0);

    pld.rayHitSky = true;
    pld.skip = false;
}