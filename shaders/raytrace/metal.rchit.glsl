#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#include "closestHitCommon.h.glsl"
#include "texutils.h.glsl"

void main() {
    HitInfo hitInfo = getObjectHitInfo();

    InstanceProperties props = instanceProperties[gl_InstanceCustomIndexEXT];

    // poor man's version of backface culling
    if (props.cullBackface != 0u && !hitInfo.frontFace) {
        skip(hitInfo);
        return;
    }

    vec2 uv = hitInfo.uv;
    if (props.bumpMapTexID >= 0) {
        uv = bumpMapping(uv, normalize(gl_WorldRayDirectionEXT), hitInfo.tbn, textures[props.bumpMapTexID]);
    }

    vec3 worldNormal = hitInfo.worldNormal;
    if (props.normalMapTexID >= 0) {
        vec3 tangentNormal = texture(textures[props.normalMapTexID], uv).rgb * 2 - 1;
        tangentNormal.y *= -1;
        worldNormal = normalize(hitInfo.tbn * tangentNormal);
    }

    #ifdef DEBUG_SHOW_NORMALS
        pld.color = hitInfo.worldNormal * 0.5 + 0.5;
    #else
        pld.color = props.albedo;
        if (props.textureID >= 0) {
            vec4 texColor = texture(textures[props.textureID], uv);
            if (texColor.a < 0.999 && random(pld.rngState) > texColor.a) {
                // transparency
                skip(hitInfo);
                return;
            }

            pld.color *= texColor.rgb;
        }
    #endif

    pld.emission = props.emission;
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormal);  // use hitInfo.worldNormal since using tangent-space calculations may not prevent self-intersection
    pld.rayDirection = reflect(normalize(gl_WorldRayDirectionEXT), normalize(worldNormal)) + props.fuzzOrRefIdx * randomUnitVec(pld.rngState);
    pld.rayHitSky = false;
    pld.skip = false;
    pld.insideDielectric = false;
    pld.materialID = 1;
    pld.surfaceNormal = worldNormal;

    if (pld.insideDielectric) {
        pld.accumulatedDistance += length(hitInfo.worldPosition - gl_WorldRayOriginEXT);
    } else {
        pld.accumulatedDistance = 0.0;
    }
}