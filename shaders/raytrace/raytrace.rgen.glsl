#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require

#include "shaderCommon.h.glsl"
#include "nee.h.glsl"
#include "pdf.h.glsl"

#include "brdfDisney.h.glsl"

#include "raytrace.h"

// Binding BINDING_IMAGEDATA in set 0 is a storage image with four 32-bit floating-point channels,
// defined using a uniform image2D variable.
layout(binding = 0, set = 0, rgba32f) uniform image2D storageImage;

// Ray payloads are used to send information between shaders.
layout(location = 0) rayPayloadEXT HitPayload pld;

// already defined in nee.h.glsl - probably not best practice, but it works
//layout (push_constant) uniform PushConsts {
//    RtPushConsts pushConstants;
//};

struct Ray {
    vec3 origin;
    vec3 direction;
};

// Uses the Box-Muller transform to return a normally distributed (centered
// at 0, standard deviation 1) 2D point.
vec2 randomGaussian(inout uint rngState) {
    // Almost uniform in (0, 1] - make sure the value is never 0:
    const float u1 = max(1e-5, random(rngState));
    const float u2 = random(rngState);  // In [0, 1]
    const float r = sqrt(-2.0 * log(u1));
    const float theta = 2 * k_pi * u2;  // Random in [0, 2pi]
    return r * vec2(cos(theta), sin(theta));
}

vec4 directLight(InstanceProperties props, mat3 tbn, uint materialID, vec3 rayIn, vec3 rayOrigin, vec3 surfaceNormal, vec3 albedo, float eta, bool didRefract, inout uint rngState) {
    RandomEmissivePointOutput target = randomEmissivePoint(rngState);
    vec3 direction = normalize(target.point - rayOrigin);
    float dist = length(target.point - rayOrigin);

    float pdf = target.pdf * dist * dist / max(dot(target.normal, -direction), 0.0001);

    if (shadowRayOccluded(rayOrigin, direction, dist)) {
        return vec4(0, 0, 0, pdf);
    }

    vec3 brdf;
    if (materialID == 0) {
        brdf = albedo / k_pi;
    } else if (materialID == 3) {
        // vec3 diffuse(vec3 baseColor, vec3 n, vec3 wi, vec3 wo, vec3 h)
        vec3 h = normalize(direction + -rayIn);

        // hard-coded for now
        brdf = evalDiffuse(props.roughness, props.subsurface, props.albedo, surfaceNormal, direction, -rayIn, h);
//        brdf = metal(tbn, albedo, props.anisotropic, props.roughness, surfaceNormal, -rayIn, direction, h);

//        float ignorePdf;  // for BSDF sampling only
//        brdf = glass(tbn, props.albedo, props.anisotropic, props.roughness, eta, surfaceNormal, -rayIn, direction, didRefract, ignorePdf);

//        brdf = sheen(props.albedo, -rayIn, h, surfaceNormal, props.sheenTint);
    }

    float cosThetai = dot(surfaceNormal, direction);
    cosThetai = target.cullBackface ? max(cosThetai, 0.0) : abs(cosThetai);

    float geometryTermNumerator = dot(target.normal, -direction);
    geometryTermNumerator = target.cullBackface ? max(geometryTermNumerator, 0.0) : abs(geometryTermNumerator);
    float geometryTerm = geometryTermNumerator / (dist * dist);

    vec3 light = target.emission * brdf * cosThetai * geometryTerm / target.pdf;

    return vec4(light, pdf);
}

vec3 traceSegments(Ray ray) {
    vec3 accumulatedRayColor = vec3(1.0);
    vec3 incomingLight = vec3(0.0);

    bool firstBounce = true;
    bool prevSkip = false;
    bool leftDielectric = false;

    for (int tracedSegments = 0; tracedSegments < pushConstants.maxBounces; tracedSegments++) {
        vec3 rayIn = ray.direction;  // wi is the old wo

        bool prevInsideDielectric = pld.insideDielectric;
        traceRayEXT(
            tlas,                  // Top-level acceleration structure
            gl_RayFlagsOpaqueEXT,  // Ray flags, here saying "treat all geometry as opaque"
            0xFF,                  // 8-bit instance mask, here saying "trace against all instances"
            0,                     // SBT record offset
            0,                     // SBT record stride for offset
            0,                     // Miss index
            ray.origin,            // Ray origin
            0.0,                   // Minimum t-value
            ray.direction,         // Ray direction
            10000.0,               // Maximum t-value
            0                      // Location of payload
        );

        bool leftDielectric = !pld.insideDielectric && prevInsideDielectric;

        ray.origin = pld.rayOrigin;
        ray.direction = pld.rayDirection;

        if (pld.skip) {
            continue;
        }

        #ifdef DEBUG_SHOW_NORMALS
            incomingLight += pld.color;
            break;
        #endif

        if (pld.rayHitSky) {
            incomingLight += pld.color * accumulatedRayColor;
            break;
        }

        if (!pld.insideDielectric) {
            vec3 indirect = pld.emission.xyz;
            bool skipNEE = bool(pld.materialID != 0);

            // vec4 directLight(int materialID, vec3 rayIn, vec3 rayOrigin, vec3 surfaceNormal, vec3 albedo, inout uint rngState)
            vec4 direct = !skipNEE
                ? directLight(pld.props, pld.tbn, pld.materialID, rayIn, pld.rayOrigin, pld.surfaceNormal, pld.color, pld.eta, pld.didRefract, pld.rngState)
                : vec4(0.0, 0.0, 0.0, 0.0);

            float pdfNEE = direct.w;
            float pdfBRDF = pld.pdf;

            float weightNEE = 0.0;
            float weightBRDF = 1.0;

            if (!skipNEE) {
                if (firstBounce || prevSkip || leftDielectric) {
                    weightNEE = 1.0;
                    weightBRDF = 1.0;
                } else if (tracedSegments + 1 == pushConstants.maxBounces) {  // last bounce
                    // todo: you can increase performance by not computing the direct lighting contribution when this case occurs
                    weightNEE = 0.0;
                    weightBRDF = balanceHeuristic(pdfBRDF, pdfNEE);
                } else {
                    weightNEE = balanceHeuristic(pdfNEE, pdfBRDF);
                    weightBRDF = balanceHeuristic(pdfBRDF, pdfNEE);
                }
            }

            prevSkip = skipNEE;
            vec3 combinedContribution = direct.rgb * weightNEE + indirect * weightBRDF;

            incomingLight += combinedContribution * accumulatedRayColor;
            accumulatedRayColor *= pld.color;
        }

        firstBounce = false;
    }

    return incomingLight;
}

vec2 randomInUnitDisk(uint rngState) {
    vec2 p;
    do {
        p = 2.0 * vec2(random(rngState), random(rngState)) - 1.0;
    } while (dot(p, p) >= 1.0);
    return p;
}

vec2 randomInUnitHexagon(uint rngState) {
    vec2 p;
    const float sqrt3 = 1.73205080757;  // approximate value of sqrt(3)
    do {
        // Generate x in [-1, 1]
        p.x = 2.0 * random(rngState) - 1.0;
        // Generate y in [-sqrt3/2, sqrt3/2]
        p.y = (random(rngState) - 0.5) * sqrt3;
    } while (abs(p.y) > (sqrt3 * 0.5) || (sqrt3 * abs(p.x) + abs(p.y)) > sqrt3);
    return p;
}

Ray getStartingRay(
    vec2 pixel,
    vec2 resolution,
    mat4 invView,
    mat4 invProjection
) {
    vec2 randomPixelCenter = pixel + vec2(0.5) + 0.375 * randomGaussian(pld.rngState);  // For antialiasing

    vec2 ndc = vec2(
        (randomPixelCenter.x / resolution.x) * 2.0 - 1.0,
        -((randomPixelCenter.y / resolution.y) * 2.0 - 1.0)  // Flip y-coordinate so image isn't upside down.
    );

    vec4 clipPos = vec4(ndc, -1.0, 1.0);

    // Unproject from clip space to view space using the inverse projection matrix
    vec4 viewPos = vec4(invProjection * clipPos);
    viewPos /= viewPos.w;  // Perspective divide.

    vec3 viewDir = normalize(viewPos.xyz);

    // Transform the view-space direction to world space using the inverse view matrix.
    // Use a w component of 0.0 to indicate that we're transforming a direction.
    vec4 worldDir4 = vec4(invView * vec4(viewDir, 0.0));
    vec3 rayDirection = normalize(worldDir4.xyz);

    vec3 origin = invView[3].xyz;
    vec3 focalPoint = origin + rayDirection * pushConstants.focusDist;
    vec2 lensOffset = randomInUnitHexagon(pld.rngState) * pushConstants.defocusMultiplier;

    vec3 right = normalize(invView[0].xyz);
    vec3 up = normalize(invView[1].xyz);

    // Offset the origin by the lens offset.
    vec3 offset = right * lensOffset.x + up * lensOffset.y;
    vec3 newOrigin = origin + offset;

    // Recompute the ray direction so that the ray goes through the focal point.
    vec3 newDirection = normalize(focalPoint - newOrigin);

    return Ray(newOrigin, newDirection);
}


void main() {
    const ivec2 resolution = imageSize(storageImage);
    const ivec2 pixel = ivec2(gl_LaunchIDEXT.xy);

    if ((pixel.x >= resolution.x) || (pixel.y >= resolution.y)) {
        return;
    }

    // State of the random number generator with an initial seed
    pld.rngState = uint((pushConstants.sampleBatch * resolution.y + pixel.y) * resolution.x + pixel.x);

    int actualSamples = 0;
    vec3 summedPixelColor = vec3(0.0);

    for (int sampleIdx = 0; sampleIdx < pushConstants.samplesPerPixel; sampleIdx++) {
        Ray startingRay = getStartingRay(vec2(pixel), vec2(resolution), pushConstants.invView, pushConstants.invProjection);
        vec3 color = clamp(traceSegments(startingRay), vec3(0), vec3(pushConstants.directClamp));

        // this is a hack. for some reason, some rays are returning NaN. no clue why.
        if (any(isnan(color))) {
            continue;
        }

        actualSamples++;
        summedPixelColor += color;
    }

    vec3 finalColor = summedPixelColor / float(actualSamples);

    if (pushConstants.sampleBatch > 0) {
        vec3 prevColor = imageLoad(storageImage, pixel).rgb;
        finalColor = (prevColor * pushConstants.sampleBatch + finalColor) / float(pushConstants.sampleBatch + 1);
    }

    imageStore(storageImage, pixel, vec4(finalColor, 1));
}