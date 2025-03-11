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


uint pickEmissiveInstance(inout uint rngState) {
    return 0;  // do this later
}


uint pickEmissiveTriangle(inout uint rngState) {
    uint emissiveInstanceIndex = pickEmissiveInstance(rngState);

    float u = stepAndOutputRNGFloat(rngState);
    uint lowerBound = emissiveInstancesData[emissiveInstanceIndex].cdfRangeStart;
    uint upperBound = emissiveInstancesData[emissiveInstanceIndex].cdfRangeEnd;

    while (lowerBound <= upperBound) {
        uint mid = (lowerBound + upperBound) / 2;
        if (allInstancesCDFs[mid] < u) {
            lowerBound = mid + 1;
        } else {
            upperBound = mid - 1;
        }
    }

    return lowerBound;
}

vec3 randomPointOnTriangle(vec3 v0, vec3 v1, vec3 v2, inout uint rngState) {
    // source: chapter 16 of Ray Tracing Gems (Shirley 2019)
    float beta = 1 - sqrt(stepAndOutputRNGFloat(rngState));
    float gamma = (1 - beta) * stepAndOutputRNGFloat(rngState);
    float alpha = 1 - beta - gamma;

    return alpha * v0 + beta * v1 + gamma * v2;
}

vec3 randomEmissivePoint(inout uint rngState) {
    uint emissiveTriangleIndex = pickEmissiveTriangle(rngState);

    InstanceData emissiveInstance = emissiveInstancesData[emissiveTriangleIndex];

    // todo: somewhere in the code below this line there is a bug that cause a crash

    return vec3(0);  // temp
//    // divide by 4 since 4 bytes for an int32
//    const uint indexOffset = emissiveInstance.indexOffset / 4;
//
//    // get the indices of the vertices of the triangle
//    const uint i0 = indices[3 * emissiveTriangleIndex + indexOffset + 0];
//    const uint i1 = indices[3 * emissiveTriangleIndex + indexOffset + 1];
//    const uint i2 = indices[3 * emissiveTriangleIndex + indexOffset + 2];
//
//    // get the vertices of the triangle
//    const vec3 v0 = vertices[i0].xyz;
//    const vec3 v1 = vertices[i1].xyz;
//    const vec3 v2 = vertices[i2].xyz;
//
//    return randomPointOnTriangle(v0, v1, v2, rngState);
}

#endif  // REINA_NEE_H