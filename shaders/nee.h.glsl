#ifndef REINA_NEE_H
#define REINA_NEE_H

#include "shaderCommon.h.glsl"
#include "closestHitCommon.h.glsl"

struct InstanceData {
    mat4x4 transform;
    uint materialOffset;
    uint cdfRangeStart;
    uint cdfRangeEnd;
    uint indexOffset;
};

layout (binding = 7, set = 0, scalar) buffer EmissiveInstancesDataBuffer{
    InstanceData emissiveInstancesData[];
};

layout (binding = 8, set = 0) buffer AllInstancesCDFsBuffer{
    float allInstancesCDFs[];
};

struct RandomEmissivePointOutput {
    vec3 point;
    vec3 normal;
    vec3 emission;
    float triArea;
};


uint pickEmissiveInstance(inout uint rngState) {
    return 0;  // do this later
}


uint pickEmissiveTriangle(inout uint rngState, uint instanceIdx) {
    float u = stepAndOutputRNGFloat(rngState);
    int lowerBound = int(emissiveInstancesData[instanceIdx].cdfRangeStart);
    int upperBound = int(emissiveInstancesData[instanceIdx].cdfRangeEnd);

    while (lowerBound <= upperBound) {
        int mid = (lowerBound + upperBound) / 2;
        if (allInstancesCDFs[mid] < u) {
            lowerBound = mid + 1;
        } else {
            upperBound = mid - 1;
        }
    }

    return lowerBound;
}

vec3 randomPointOnTriangle(inout uint rngState, vec3 v0, vec3 v1, vec3 v2) {
    // source: chapter 16 of Ray Tracing Gems (Shirley 2019)
    float beta = 1 - sqrt(stepAndOutputRNGFloat(rngState));
    float gamma = (1 - beta) * stepAndOutputRNGFloat(rngState);
    float alpha = 1 - beta - gamma;

    return alpha * v0 + beta * v1 + gamma * v2;
}

RandomEmissivePointOutput randomEmissivePoint(inout uint rngState) {
    // todo: make this work when there is a matrix transform for the instance

    uint instanceIdx = 0;
    uint emissiveTriangleIndex = 0;

    InstanceData emissiveInstance = emissiveInstancesData[instanceIdx];

    // divide by 4 since 4 bytes for an int32
    const uint indexOffset = emissiveInstance.indexOffset / 4;

    // get the indices of the vertices of the triangle
    const uint i0 = indices[3 * emissiveTriangleIndex + indexOffset + 0];
    const uint i1 = indices[3 * emissiveTriangleIndex + indexOffset + 1];
    const uint i2 = indices[3 * emissiveTriangleIndex + indexOffset + 2];

    // get the vertices of the triangle
    const vec3 v0 = vertices[i0].xyz;
    const vec3 v1 = vertices[i1].xyz;
    const vec3 v2 = vertices[i2].xyz;

    vec3 point = randomPointOnTriangle(rngState, v0, v1, v2);
    vec3 normal = normalize(cross(v1 - v0, v2 - v0));  // todo: add normal interpolation
    vec3 emission = vec3(12.25);  // hard-coded for now

    float area = 0.5 * length(cross(v1 - v0, v2 - v0));

    return RandomEmissivePointOutput(point, normal, emission, area);
}

#endif  // REINA_NEE_H