#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "closestHitCommon.h.glsl"
#include "nee.h.glsl"
#include "brdfDisney.h.glsl"
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
    // Before doing bump mapping modulus the UV by 1 to effectively wrap the UV coordinates. I can't change the image
    //  sampler configuration since then the UV check to discard out of bounds UV coordinates would not work.
    uv = mod(uv, 1.0);

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
        vec4 texColor = texture(textures[props.textureID], uv);
        if (texColor.a < 0.999 && random(pld.rngState) > texColor.a) {
            // transparency
            skip(hitInfo);
            return;
        }

        pld.color *= texColor.rgb;
    }
    #endif

    vec3 rayDir = vec3(0);
    float pdf = 0.0;

    // Diffuse
//    rayDir = sampleDiffuse(worldNormal, pld.rngState);
//    pdf = pdfDiffuse(worldNormal, rayDir);

    // Metal
//    rayDir = sampleMetal(hitInfo.tbn, props.albedo, props.anisotropic, props.roughness, worldNormal, -gl_WorldRayDirectionEXT, pld.rngState);
//    pdf = pdfMetal(hitInfo.tbn, -gl_WorldRayDirectionEXT, rayDir, props.anisotropic, props.roughness);

    rayDir = sampleClearcoat(hitInfo.tbn, props.clearcoatGloss, -gl_WorldRayDirectionEXT, pld.rngState);
    vec3 h = normalize(rayDir + -gl_WorldRayDirectionEXT);

    pdf = pdfClearcoat(hitInfo.tbn, -gl_WorldRayDirectionEXT, rayDir, h, props.clearcoatGloss);

    pld.pdf = pdf;
    pld.emission = props.emission;
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, hitInfo.worldNormalGeometry);
    pld.rayDirection = rayDir;
    pld.rayHitSky = false;
    pld.skip = false;
    pld.insideDielectric = false;
    pld.materialID = 3;
    pld.surfaceNormal = worldNormal;
    pld.tbn = hitInfo.tbn;
    pld.props = props;

    if (pld.insideDielectric) {
        pld.accumulatedDistance += length(hitInfo.worldPosition - gl_WorldRayOriginEXT);
    } else {
        pld.accumulatedDistance = 0.0;
    }
}