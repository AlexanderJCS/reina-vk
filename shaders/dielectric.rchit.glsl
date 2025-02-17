#version 460
#extension GL_GOOGLE_include_directive : require
#include "closestHitCommon.h.glsl"

float reflectance(float cosine, float ref_idx) {
    // Use Schlick's approximation for reflectance
    float r0 = (1 - ref_idx) / (1 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
}

vec3 offsetPositionForDielectric(vec3 worldPosition, vec3 normal, vec3 rayDir) {
    // If the ray is going inside (e.g. entering a dielectric), flip the normal.
    vec3 offsetNormal = (dot(normal, rayDir) < 0.0) ? -normal : normal;

    // Convert the normal to an integer offset.
    const float int_scale = 256.0;
    const ivec3 of_i = ivec3(int_scale * offsetNormal);

    // Offset each component of worldPosition using its bit representation.
    // The sign check on worldPosition components helps to handle negative values.
    const vec3 p_i = vec3(
    intBitsToFloat(floatBitsToInt(worldPosition.x) + ((worldPosition.x < 0.0) ? -of_i.x : of_i.x)),
    intBitsToFloat(floatBitsToInt(worldPosition.y) + ((worldPosition.y < 0.0) ? -of_i.y : of_i.y)),
    intBitsToFloat(floatBitsToInt(worldPosition.z) + ((worldPosition.z < 0.0) ? -of_i.z : of_i.z))
    );

    // For points near the origin, use a smaller floating-point offset.
    const float origin = 1.0 / 32.0;
    const float floatScale = 1.0 / 65536.0;
    return vec3(
    abs(worldPosition.x) < origin ? worldPosition.x + floatScale * offsetNormal.x : p_i.x,
    abs(worldPosition.y) < origin ? worldPosition.y + floatScale * offsetNormal.y : p_i.y,
    abs(worldPosition.z) < origin ? worldPosition.z + floatScale * offsetNormal.z : p_i.z
    );
}

void main() {
    HitInfo hitInfo = getObjectHitInfo();

    // todo: extract a bunch of this into a function
    float ri = hitInfo.frontFace ? 1.0 / objectProperties[gl_InstanceCustomIndexEXT].fuzzOrRefIdx : objectProperties[gl_InstanceCustomIndexEXT].fuzzOrRefIdx;
    vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
    float cosTheta = min(dot(-unitDir, hitInfo.worldNormal), 1.0);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    bool cannot_refract = bool(ri * sinTheta > 1.0);  // must wrap in bool() to prevent glsl linter from throwing false-positive error
    float reflectivity = reflectance(cosTheta, ri);

    if (cannot_refract || reflectivity > stepAndOutputRNGFloat(pld.rngState)) {
        // specular reflection
        pld.rayDirection = reflect(unitDir, hitInfo.worldNormal);
        pld.color = vec3(1);
        pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);
    } else {
        // refract
        pld.rayDirection = refract(unitDir, hitInfo.worldNormal, ri);
        pld.color = objectProperties[gl_InstanceCustomIndexEXT].albedo;
        pld.rayOrigin = offsetPositionForDielectric(hitInfo.worldPosition, hitInfo.worldNormal, unitDir);
    }

    pld.emission  = objectProperties[gl_InstanceCustomIndexEXT].emission;
    pld.rayHitSky = false;
}