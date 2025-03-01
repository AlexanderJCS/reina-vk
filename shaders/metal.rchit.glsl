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

    #ifdef DEBUG_SHOW_NORMALS
        pld.color = hitInfo.worldNormal * 0.5 + 0.5;
    #else
        pld.color = objectProperties[gl_InstanceCustomIndexEXT].albedo;
    #endif

    pld.emission = objectProperties[gl_InstanceCustomIndexEXT].emission;
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, normalize(hitInfo.worldNormal));
    pld.rayDirection = reflect(normalize(gl_WorldRayDirectionEXT), normalize(hitInfo.worldNormal)) + objectProperties[gl_InstanceCustomIndexEXT].fuzzOrRefIdx * randomUnitVec(pld.rngState);
    pld.rayHitSky = false;
    pld.skip = false;
    pld.insideDielectric = false;
    pld.accumulatedDistance = 0.0;
}