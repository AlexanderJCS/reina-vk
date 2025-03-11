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
    pld.usedNEE = true;

    vec3 target = vec3(0, 1.5, 0);
    vec3 direction = normalize(target - pld.rayOrigin);
    float dist = length(target - pld.rayOrigin);
    float lightProb = 1;

    pld.directLight = vec3(0);
    if (!shadowRayOccluded(pld.rayOrigin, direction, dist)) {
        // Calculate cosine of the angle between the surface normal and the light direction.
        float cosTheta = max(dot(hitInfo.worldNormal, direction), 0.0);

        // Constants:
        // Define a light intensity (radiance) for the point light.
        vec3 lightIntensity = vec3(1.0); // Adjust as needed.

        // Compute direct lighting contribution:
        // For a diffuse surface, the BRDF is albedo/PI, and we include
        // the cosine term and inverse-square falloff.
        vec3 directLight = (objectProperties[gl_InstanceCustomIndexEXT].albedo / k_pi) *
            lightIntensity * cosTheta / (dist * dist);

        pld.directLight = directLight / lightProb;
    }

    if (pld.insideDielectric) {
        pld.accumulatedDistance += length(hitInfo.worldPosition - gl_WorldRayOriginEXT);
    } else {
        pld.accumulatedDistance = 0.0;
    }
}