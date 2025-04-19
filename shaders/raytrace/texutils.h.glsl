#ifndef REINA_TEXUTILS_H
#define REINA_TEXUTILS_H

vec2 bumpMapping(vec2 uv, vec3 rayIn, mat3 T, sampler2D heightMap) {
    const float heightScale = 0.5;
    const float minLayers   =  512.0;
    const float maxLayers   = 1024.0;

    // 1. world → tangent, view vector
    vec3 V = normalize(transpose(T) * -rayIn);
    if (V.z <= 0.0) return uv;               // avoid divide‐by‐zero / backfaces

    // 2. pick layers based on angle
    float numLayers = mix(maxLayers, minLayers, clamp(V.z, 0.0, 1.0));
    float layerDepth = 1.0 / numLayers;

    // 3. UV step
    vec2 P       = V.xy / V.z * heightScale;
    vec2 deltaUV = P / numLayers;

    // 4. layer‐by‐layer search
    vec2  currUV   = uv;
    float depthSum = 0.0;
    float h        = texture(heightMap, currUV).r * heightScale;

    while (depthSum < h) {
        currUV   -= deltaUV;
        depthSum += layerDepth;
        h         = texture(heightMap, currUV).r * heightScale;
    }

    // 5. linear interp between last two samples
    vec2 prevUV      = currUV + deltaUV;
    float hPrev      = texture(heightMap, prevUV).r * heightScale;
    float sumPrev    = depthSum - layerDepth;
    float after      =  h        - depthSum;
    float before     =  hPrev    - sumPrev;
    float weight     = after / (after - before);

    return mix(prevUV, currUV, weight);
}


#endif