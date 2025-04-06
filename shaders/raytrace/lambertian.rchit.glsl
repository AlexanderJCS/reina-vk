#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

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

    vec3 worldNormal = hitInfo.worldNormal;
    if (props.normalMapTexID != -1) {
        vec3 tangentNormal = texture(textures[props.normalMapTexID], hitInfo.uv).rgb * 2 - 1;
        worldNormal = hitInfo.tbn * tangentNormal;
    }

    #ifdef DEBUG_SHOW_NORMALS
        pld.color = worldNormal * 0.5 + 0.5;
    #else
        pld.color = props.albedo;

        if (props.textureID >= 0) {
            pld.color *= texture(textures[props.textureID], hitInfo.uv).rgb;
        }
    #endif

    pld.emission = props.emission;
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, worldNormal);
    pld.rayDirection = diffuseReflection(worldNormal, pld.rngState);
    pld.rayHitSky = false;
    pld.skip = false;
    pld.insideDielectric = false;
    pld.materialID = 0;
    pld.surfaceNormal = worldNormal;

    if (pld.insideDielectric) {
        pld.accumulatedDistance += length(hitInfo.worldPosition - gl_WorldRayOriginEXT);
    } else {
        pld.accumulatedDistance = 0.0;
    }
}