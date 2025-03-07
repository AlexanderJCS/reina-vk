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
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    pld.rayDirection = diffuseReflection(hitInfo.worldNormal, pld.rngState);
    pld.rayHitSky = false;
    pld.skip = false;
    pld.insideDielectric = false;

    vec3 target = vec3(0, 3, 0);
    vec3 direction = normalize(target - pld.rayOrigin);
    float dist = length(target - pld.rayOrigin);

    pld.color = shadowRayOccluded(pld.rayOrigin, direction, dist) ? vec3(0, 0, 1) : vec3(1, 0, 0);

    if (pld.insideDielectric) {
        pld.accumulatedDistance += length(hitInfo.worldPosition - gl_WorldRayOriginEXT);
    } else {
        pld.accumulatedDistance = 0.0;
    }
}