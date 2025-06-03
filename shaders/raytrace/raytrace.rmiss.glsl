#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "shaderCommon.h.glsl"

// The payload:
layout(location = 0) rayPayloadInEXT HitPayload pld;

void main() {
    const float rayDirY = normalize(gl_WorldRayDirectionEXT).y;
    float t = 0.5 * (rayDirY + 1.0);
    pld.color = mix(vec3(0.1), vec3(0.4, 1.7, 2), t) * 0.07;
//    pld.color = vec3(0.5);
    pld.albedo = pld.color;

    pld.rayHitSky = true;
    pld.skip = false;
}