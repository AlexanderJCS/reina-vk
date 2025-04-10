#ifndef REINA_NEE_H
#define REINA_NEE_H

#include "shaderCommon.h.glsl"
#include "raytrace.h"

layout (push_constant) uniform PushConsts {
    RtPushConsts pushConstants;
};

struct InstanceData {
    mat4x4 transform;
    uint materialOffset;
    uint cdfRangeStart;
    uint cdfRangeEnd;
    uint indexOffset;
    vec3 emission;
    float weight;
};

layout (binding = 7, set = 0, scalar) buffer EmissiveMetadataBuffer {
    InstanceData emissiveMetadata[];
};

layout (binding = 8, set = 0) buffer CDFTrianglesBuffer {
    float cdfTriangles[];
};

layout (binding = 9, set = 0) buffer CDFInstancesBuffer {
    uint numInstances;
    float allInstancesArea;
    float cdfInstances[];
};

struct RandomEmissivePointOutput {
    vec3 point;
    vec3 normal;
    vec3 emission;
    float pdf;
};

struct ShadowPayload {
    bool occluded;
};

layout(location = 1) rayPayloadEXT ShadowPayload shadowPld;

uint pickEmissiveInstance(inout uint rngState) {
    float u = random(rngState);
    int lowerBound = 0;
    int upperBound = int(numInstances) - 1;

    while (lowerBound < upperBound) {
        int mid = (lowerBound + upperBound) / 2;
        if (u <= cdfInstances[mid]) {
            upperBound = mid;
        } else {
            lowerBound = mid + 1;
        }
    }

    return lowerBound;
}

uint pickEmissiveTriangle(inout uint rngState, uint instanceIdx) {
    float u = random(rngState);
    int lowerBound = int(emissiveMetadata[instanceIdx].cdfRangeStart);
    int upperBound = int(emissiveMetadata[instanceIdx].cdfRangeEnd);

    while (lowerBound < upperBound) {
        int mid = (lowerBound + upperBound) / 2;
        if (u <= cdfTriangles[mid]) {
            upperBound = mid;
        } else {
            lowerBound = mid + 1;
        }
    }

    return lowerBound;
}

vec3 randomPointOnTriangle(inout uint rngState, vec3 v0, vec3 v1, vec3 v2) {
    // source: chapter 16 of Ray Tracing Gems (Shirley 2019)
    float beta = 1 - sqrt(random(rngState));
    float gamma = (1 - beta) * random(rngState);
    float alpha = 1 - beta - gamma;

    return alpha * v0 + beta * v1 + gamma * v2;
}

RandomEmissivePointOutput randomEmissivePoint(inout uint rngState) {
    uint instanceIdx = pickEmissiveInstance(rngState);
    uint emissiveTriangleIndex = pickEmissiveTriangle(rngState, instanceIdx);

    InstanceData instanceMetadata = emissiveMetadata[instanceIdx];

    // divide by 4 since 4 bytes for an int32
    const uint indexOffset = instanceMetadata.indexOffset / 4;

    // get the indices of the vertices of the triangle
    const uint i0 = indices[3 * emissiveTriangleIndex + indexOffset + 0];
    const uint i1 = indices[3 * emissiveTriangleIndex + indexOffset + 1];
    const uint i2 = indices[3 * emissiveTriangleIndex + indexOffset + 2];

    // get the vertices of the triangle
    vec3 v0 = vertices[i0].xyz;
    vec3 v1 = vertices[i1].xyz;
    vec3 v2 = vertices[i2].xyz;

    v0 = (instanceMetadata.transform * vec4(v0, 1.0)).xyz;
    v1 = (instanceMetadata.transform * vec4(v1, 1.0)).xyz;
    v2 = (instanceMetadata.transform * vec4(v2, 1.0)).xyz;

    vec3 point = randomPointOnTriangle(rngState, v0, v1, v2);
    vec3 normal = normalize(cross(v1 - v0, v2 - v0));

    float luminosity = 0.2126 * instanceMetadata.emission.r + 0.7152 * instanceMetadata.emission.g + 0.0722 * instanceMetadata.emission.b;

    float probability = instanceMetadata.weight / pushConstants.totalEmissiveArea;
    return RandomEmissivePointOutput(point, normal, instanceMetadata.emission, probability);
}

bool shadowRayOccluded(vec3 origin, vec3 direction, float dist) {
    shadowPld.occluded = true;

    traceRayEXT(
        tlas,                  // Top-level acceleration structure
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
        0xFF,                  // 8-bit instance mask, here saying "trace against all instances"
        0,                     // SBT record offset
        0,                     // SBT record stride for offset
        1,                     // Miss index
        origin,                // Ray origin
        0,                     // Minimum t-value
        direction,             // Ray direction
        dist - 0.001,          // Maximum t-value
        1                      // Location of payload
    );

    return shadowPld.occluded;
}

#endif  // REINA_NEE_H