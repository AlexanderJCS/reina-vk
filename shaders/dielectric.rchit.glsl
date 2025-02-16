#version 460
#extension GL_GOOGLE_include_directive : require
#include "closestHitCommon.h.glsl"

float reflectance(float cosine, float ref_idx) {
    // Use Schlick's approximation for reflectance
    float r0 = (1 - ref_idx) / (1 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}

void main() {
    HitInfo hitInfo = getObjectHitInfo();

    // todo: extract a bunch of this into a function
    float ri = hitInfo.frontFace ? 1.0 / objectProperties[gl_InstanceCustomIndexEXT].fuzzOrRefIdx : objectProperties[gl_InstanceCustomIndexEXT].fuzzOrRefIdx;
    vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
    float cosTheta = min(dot(-unitDir, hitInfo.worldNormal), 1.0);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    bool cannot_refract = bool(ri * sinTheta > 1.0);  // must wrap in bool() to prevent glsl linter from throwing false-positive error
    float reflectance = reflectance(cosTheta, ri);

    if (cannot_refract || reflectance > stepAndOutputRNGFloat(pld.rngState)) {
        // specular reflection
        pld.rayDirection = reflect(unitDir, hitInfo.worldNormal);
        pld.color = vec3(1);
    } else {
        // refract
        pld.rayDirection = refract(unitDir, hitInfo.worldNormal, ri);
        pld.color = objectProperties[gl_InstanceCustomIndexEXT].albedo;
    }

    pld.emission  = objectProperties[gl_InstanceCustomIndexEXT].emission;
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    pld.rayHitSky = false;
}