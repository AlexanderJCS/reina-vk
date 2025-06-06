#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#include "closestHitCommon.h.glsl"
#include "texutils.h.glsl"

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
    InstanceProperties props = instanceProperties[gl_InstanceCustomIndexEXT];

    float ri = hitInfo.frontFace ? 1.0 / props.ior : props.ior;
    vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
    float cosTheta = min(dot(-unitDir, hitInfo.worldNormal), 1.0);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    bool cannotRefract = bool(ri * sinTheta > 1.0);  // must wrap in bool() to prevent glsl linter from throwing false-positive error
    float reflectivity = reflectance(cosTheta, ri);

    bool previouslyInsideDielectric = pld.insideDielectric;

    vec2 uv = hitInfo.uv;
    // Before doing bump mapping modulus the UV by 1 to effectively wrap the UV coordinates. I can't change the image
    //  sampler configuration since then the UV check to discard out of bounds UV coordinates would not work.
    uv = mod(uv, 1.0);

    if (props.bumpMapTexID >= 0) {
        uv = bumpMapping(uv, normalize(gl_WorldRayDirectionEXT), hitInfo.tbn, textures[props.bumpMapTexID]);
    }

    vec3 albedo = props.albedo;
    if (props.textureID >= 0) {
        albedo *= texture(textures[props.textureID], uv).rgb;
    }

    vec3 worldNormal = hitInfo.worldNormal;
    if (props.normalMapTexID >= 0) {
        vec3 tangentNormal = texture(textures[props.normalMapTexID], uv).rgb * 2 - 1;
        tangentNormal.y *= -1;
        worldNormal = normalize(hitInfo.tbn * tangentNormal);
    }

    if (cannotRefract || reflectivity > random(pld.rngState)) {
        // specular reflection
        pld.rayDirection = fuzzyReflection(unitDir, worldNormal, props.roughness, pld.rngState);
        pld.color = vec3(1);
        pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormalGeometry);
    } else {
        pld.insideDielectric = hitInfo.frontFace;

        // refract
        pld.rayDirection = addFuzz(refract(unitDir, worldNormal, ri), props.roughness, pld.rngState);
        pld.color = albedo;
        pld.rayOrigin = offsetPositionForDielectric(hitInfo.worldPosition, hitInfo.worldNormalGeometry, unitDir);
    }

    pld.accumulatedDistance += mix(0, length(hitInfo.worldPosition - gl_WorldRayOriginEXT), previouslyInsideDielectric);

    bool exitingDielectric = previouslyInsideDielectric && !pld.insideDielectric;
    if (exitingDielectric) {
        float dist = pld.accumulatedDistance;
        dist *= 100.0;  // convert meters to cm

        pld.color = vec3(1) * exp(-props.absorption * pld.accumulatedDistance);
        pld.color *= albedo;

        pld.accumulatedDistance = 0.0;
    }

    pld.albedo = pld.color;
    pld.emission = props.emission;
    pld.rayHitSky = false;
    pld.skip = false;
    pld.materialID = 2;
    pld.surfaceNormal = worldNormal;
    pld.pdf = 0.0;  // Not used in this shader
    pld.tbn = hitInfo.tbn;
    pld.props = props;
    pld.didRefract = false;  // only used for disney bsdf
    pld.eta = 0.0;  // only used for disney bsdf
}