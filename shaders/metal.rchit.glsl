#version 460
#extension GL_GOOGLE_include_directive : require
#include "closestHitCommon.h.glsl"

void main() {
    HitInfo hitInfo = getObjectHitInfo();

    // poor man's version of backface culling
    if (!hitInfo.frontFace) {
        skip(hitInfo);
        return;
    }

    pld.color        = objectProperties[gl_InstanceCustomIndexEXT].albedo;
    pld.emission     = objectProperties[gl_InstanceCustomIndexEXT].emission;
    pld.rayOrigin    = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    pld.rayDirection = reflect(gl_WorldRayDirectionEXT, hitInfo.worldNormal) + objectProperties[gl_InstanceCustomIndexEXT].fuzzOrRefIdx * randomUnitVec(pld.rngState);
    pld.rayHitSky    = false;
    pld.skip         = false;
}