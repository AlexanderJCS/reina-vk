#version 460
#extension GL_GOOGLE_include_directive : require

#include "closestHitCommon.h.glsl"
#include "nee.h.glsl"

void main() {
    HitInfo hitInfo = getObjectHitInfo();

    // poor man's version of backface culling
    if (!hitInfo.frontFace) {
        skip(hitInfo);
        return;
    }

    ObjectProperties props = objectProperties[gl_InstanceCustomIndexEXT];

    #ifdef DEBUG_SHOW_NORMALS
        pld.color = hitInfo.worldNormal * 0.5 + 0.5;
    #else
        pld.color = props.albedo;
    #endif

    pld.emission = props.emission;
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    pld.rayDirection = diffuseReflection(hitInfo.worldNormal, pld.rngState);
    pld.rayHitSky = false;
    pld.skip = false;
    pld.insideDielectric = false;
    pld.materialID = 0;
    pld.surfaceNormal = hitInfo.worldNormal;

    if (pld.insideDielectric) {
        pld.accumulatedDistance += length(hitInfo.worldPosition - gl_WorldRayOriginEXT);
    } else {
        pld.accumulatedDistance = 0.0;
    }
}