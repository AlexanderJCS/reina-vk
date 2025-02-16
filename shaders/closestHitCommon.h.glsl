#ifndef VK_MINI_PATH_TRACER_CLOSEST_HIT_COMMON_H
#define VK_MINI_PATH_TRACER_CLOSEST_HIT_COMMON_H

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#include "shaderCommon.h.glsl"

hitAttributeEXT vec2 attributes;

layout(binding = 2, set = 0, scalar) buffer Vertices {
    vec4 vertices[];
};

layout(binding = 3, set = 0, scalar) buffer Indices {
    uint indices[];
};

layout(location = 0) rayPayloadInEXT PassableInfo pld;

struct ObjectProperties {
    vec3 albedo;
    float padding;
    vec4 emission;
    float fuzzOrRefIdx;
    vec3 padding2;
};

layout(binding = 4, set = 0, scalar) buffer ObjectPropertiesBuffer {
    ObjectProperties objectProperties[];
};

struct HitInfo {
    vec3 objectPosition;
    vec3 worldPosition;
    vec3 worldNormal;
    vec3 color;
};

HitInfo getObjectHitInfo() {
    HitInfo result;
    // Get the ID of the triangle
    const int primitiveID = gl_PrimitiveID;

    // Get the indices of the vertices of the triangle
    const uint i0 = indices[3 * primitiveID + 0];
    const uint i1 = indices[3 * primitiveID + 1];
    const uint i2 = indices[3 * primitiveID + 2];

    // Get the vertices of the triangle
    const vec3 v0 = vertices[i0].xyz;
    const vec3 v1 = vertices[i1].xyz;
    const vec3 v2 = vertices[i2].xyz;

    // Get the barycentric coordinates of the intersection
    vec3 barycentrics = vec3(0.0, attributes.x, attributes.y);
    barycentrics.x    = 1.0 - barycentrics.y - barycentrics.z;

    // Compute the coordinates of the intersection
    result.objectPosition = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    // Transform from object space to world space:
    result.worldPosition = gl_ObjectToWorldEXT * vec4(result.objectPosition, 1.0f);

    const vec3 objectNormal = cross(v1 - v0, v2 - v0);
    // Transform normals from object space to world space. These use the transpose of the inverse matrix,
    // because they're directions of normals, not positions:
    result.worldNormal = normalize((objectNormal * gl_WorldToObjectEXT).xyz);

    // Flip the normal so it points against the ray direction:
    const vec3 rayDirection = gl_WorldRayDirectionEXT;
    result.worldNormal      = faceforward(result.worldNormal, rayDirection, result.worldNormal);

    return result;
}

/*
 * Credit: Carsten Wächter and Nikolaus Binder from "A Fast and Robust Method for Avoiding Self-Intersection"
 * from Ray Tracing Gems (version 1.7, 2020)
 *
 * You can negate the normal to pass through the surface
 */
vec3 offsetPositionAlongNormal(vec3 worldPosition, vec3 normal) {
    // Convert the normal to an integer offset.
    const float int_scale = 256.0f;
    const ivec3 of_i = ivec3(int_scale * normal);

    // Offset each component of worldPosition using its binary representation.
    // Handle the sign bits correctly.
    const vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(worldPosition.x) + ((worldPosition.x < 0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(worldPosition.y) + ((worldPosition.y < 0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(worldPosition.z) + ((worldPosition.z < 0) ? -of_i.z : of_i.z))
    );

    // Use a floating-point offset instead for points near (0,0,0), the origin.
    const float origin = 1.0f / 32.0f;
    const float floatScale = 1.0f / 65536.0f;
    return vec3(
        abs(worldPosition.x) < origin ? worldPosition.x + floatScale * normal.x : p_i.x,
        abs(worldPosition.y) < origin ? worldPosition.y + floatScale * normal.y : p_i.y,
        abs(worldPosition.z) < origin ? worldPosition.z + floatScale * normal.z : p_i.z
    );
}

vec3 randomUnitVec(inout uint rngState) {
    // todo: see if this method or sampling a sphere is faster. profile it
    while (true) {
        vec3 vector = vec3(stepAndOutputRNGFloat(rngState), stepAndOutputRNGFloat(rngState), stepAndOutputRNGFloat(rngState));

        float lenSquared = dot(vector, vector);
        if (0.0001 < lenSquared && lenSquared < 1) {
            return normalize(vector);
        }
    }
}

vec3 diffuseReflection(vec3 normal, inout uint rngState) {
    const float theta = 2.0 * k_pi * stepAndOutputRNGFloat(rngState);  // Random in [0, 2pi]
    const float u = 2.0 * stepAndOutputRNGFloat(rngState) - 1.0;   // Random in [-1, 1]
    const float r = sqrt(1.0 - u * u);
    const vec3 direction = normal + vec3(r * cos(theta), r * sin(theta), u);

    return normalize(direction);
}

#endif  // #ifndef VK_MINI_PATH_TRACER_CLOSEST_HIT_COMMON_H