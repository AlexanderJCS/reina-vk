#ifndef REINA_TEXUTILS_H
#define REINA_TEXUTILS_H

vec2 bumpMapping(vec2 uv, vec3 rayDir, mat3x3 tbn, sampler2D bumpMap) {
    // todo: this isn't really working...
    rayDir = inverse(tbn) * rayDir;

    float heightScale = 0.5f;
    const float minLayers = 8.0f;
    const float maxLayers = 64.0f;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0f, 0.0f, 1.0f), rayDir)));
    float layerDepth = 1.0f / numLayers;
    float currentLayerDepth = 0.0f;

    vec2 s = rayDir.xy / rayDir.z * heightScale;
    vec2 deltaUV = s / numLayers;

    vec2 currentUV = uv;
    float currentDepthMapValue = 1.0f - texture(bumpMap, currentUV).r;

    while (currentLayerDepth < currentDepthMapValue) {
        currentUV -= deltaUV;
        currentDepthMapValue = 1.0f - texture(bumpMap, currentUV).r;
        currentLayerDepth += layerDepth;
    }

    // Occlusion (interpolate with previous value)
    vec2 prevTexCoords = currentUV + deltaUV;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = 1.0f - texture(bumpMap, prevTexCoords).r - currentLayerDepth + layerDepth;
    float weight = afterDepth / (afterDepth - beforeDepth);

    // todo: discard outside of bounds
    return prevTexCoords * weight + currentUV * (1.0f - weight);  // todo: replace with mix();
}

#endif