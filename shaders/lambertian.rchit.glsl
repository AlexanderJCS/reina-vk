#version 460
#extension GL_GOOGLE_include_directive : require
#include "closestHitCommon.h.glsl"

void main() {
    HitInfo hitInfo = getObjectHitInfo();

    #ifdef DEBUG_SHOW_NORMALS
        pld.color = hitInfo.worldNormal * 0.5 + 0.5;
    #else
        pld.color = objectProperties[gl_InstanceCustomIndexEXT].albedo;
    #endif

    pld.emission     = objectProperties[gl_InstanceCustomIndexEXT].emission;
    pld.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    pld.rayDirection = diffuseReflection(hitInfo.worldNormal, pld.rngState);
    pld.rayHitSky    = false;
}