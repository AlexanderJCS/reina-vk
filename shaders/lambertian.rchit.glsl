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
    pld.usedNEE = true;
    pld.directLight = vec3(0);

    RandomEmissivePointOutput target = randomEmissivePoint(pld.rngState);
    vec3 direction = normalize(target.point - pld.rayOrigin);
    float dist = length(target.point - pld.rayOrigin);

//    if (!shadowRayOccluded(pld.rayOrigin, direction, dist)) {
        vec3 lambertBRDF = props.albedo / k_pi;
        float cosThetai = max(dot(hitInfo.worldNormal, direction), 0.0);
        float geometryTerm = max(dot(target.normal, -direction), 0.0) / (dist * dist);

        float probChoosingLight = 1;
        float probChoosingPoint = 1 / target.triArea;
        float lightPDF = probChoosingLight * probChoosingPoint;

        pld.directLight = target.emission * lambertBRDF * cosThetai * geometryTerm / lightPDF;
//    }

    if (pld.insideDielectric) {
        pld.accumulatedDistance += length(hitInfo.worldPosition - gl_WorldRayOriginEXT);
    } else {
        pld.accumulatedDistance = 0.0;
    }
}