#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "closestHitCommon.h.glsl"
#include "nee.h.glsl"
#include "texutils.h.glsl"

void main() {
    HitInfo hitInfo = getObjectHitInfo();
    ObjectProperties props = objectProperties[gl_InstanceCustomIndexEXT];

    // poor man's version of backface culling
    if (props.cullBackface != 0u && !hitInfo.frontFace) {
        skip(hitInfo);
        return;
    }

    vec2 uv = hitInfo.uv;
    if (props.bumpMapTexID >= 0) {
        uv = bumpMapping(uv, normalize(gl_WorldRayDirectionEXT), hitInfo.tbn, textures[props.bumpMapTexID]);
    }

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        skip(hitInfo);  // skip if uv is out of bounds
        return;
    }

    vec3 worldNormal = hitInfo.worldNormal;
    if (props.normalMapTexID >= 0) {
        vec3 tangentNormal = texture(textures[props.normalMapTexID], uv).rgb * 2 - 1;
        tangentNormal.y *= -1;
        worldNormal = normalize(hitInfo.tbn * tangentNormal);
    }

    #ifdef DEBUG_SHOW_NORMALS
        pld.color = worldNormal * 0.5 + 0.5;
    #else
        pld.color = props.albedo;
        if (props.textureID >= 0) {
            pld.color *= texture(textures[props.textureID], uv).rgb;
        }
    #endif

    pld.emission = props.emission;
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);  // use hitInfo.worldNormal since using tangent-space calculations may not prevent self-intersection
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