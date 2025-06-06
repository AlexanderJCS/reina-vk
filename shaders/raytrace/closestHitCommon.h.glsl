#ifndef REINA_CLOSEST_HIT_COMMON_H
#define REINA_CLOSEST_HIT_COMMON_H

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#include "shaderCommon.h.glsl"

// include this primarily for DEBUG_SHOW_NORMALS
#include "raytrace.h"

hitAttributeEXT vec2 attributes;

layout (binding = 5, set = 0, scalar) buffer NormalsBuffer {
    mat3 tbns[];
};

layout (binding = 6, set = 0, scalar) buffer NormalsIndicesBuffer {
    uint tbnsIndices[];
};

layout (binding = 10, set = 0, scalar) buffer TexCoordsBuffer {
    vec2 texCoords[];
};

layout (binding = 11, set = 0, scalar) buffer TexIndicesBuffer {
    uint texIndices[];
};

layout(location = 0) rayPayloadInEXT HitPayload pld;

layout(binding = 4, set = 0, scalar) buffer InstancePropertiesBuffer {
    InstanceProperties instanceProperties[];
};

//layout(binding = 12, set = 0, scalar) buffer ImagesBuffer {
//    sampler2D images[];
//};

layout(binding = 13, set = 0) uniform sampler2D textures[];

struct HitInfo {
    vec3 objectPosition;
    vec3 worldPosition;
    vec3 worldNormal;
    vec3 worldNormalGeometry;
    vec3 color;
    vec2 uv;
    bool frontFace;
    mat3 tbn;
};

HitInfo getObjectHitInfo() {
    HitInfo result;
    InstanceProperties props = instanceProperties[gl_InstanceCustomIndexEXT];
    // Get the ID of the triangle
    const uint primitiveID = gl_PrimitiveID;

    const uint indexOffset = props.indicesOffset;

    // Get the indices of the vertices of the triangle
    const uint i0 = indices[3 * primitiveID + indexOffset + 0];
    const uint i1 = indices[3 * primitiveID + indexOffset + 1];
    const uint i2 = indices[3 * primitiveID + indexOffset + 2];

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

    vec3 objectNormalGeometry = normalize(cross(v1 - v0, v2 - v0));
    vec3 objectNormal;
    if (!props.interpNormals) {
        objectNormal = objectNormalGeometry;
    } else {
        const uint tbnsIndexOffset = props.tbnsIndicesOffset;
        const uint tbn0Index = tbnsIndices[3 * primitiveID + tbnsIndexOffset + 0];
        const uint n1Index = tbnsIndices[3 * primitiveID + tbnsIndexOffset + 1];
        const uint tbn2Index = tbnsIndices[3 * primitiveID + tbnsIndexOffset + 2];

        const vec3 n0 = normalize(tbns[tbn0Index][2]);
        const vec3 n1 = normalize(tbns[n1Index][2]);
        const vec3 n2 = normalize(tbns[tbn2Index][2]);

        objectNormal = normalize(n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z);
    }

    if (props.texIndicesOffset == 0xFFFFFFFFu) {
        // no tex coords for this model
        result.uv = vec2(0);
    } else {
        const uint texIndexOffset = props.texIndicesOffset;
        const uint t0Index = texIndices[3 * primitiveID + texIndexOffset + 0];
        const uint t1Index = texIndices[3 * primitiveID + texIndexOffset + 1];
        const uint t2Index = texIndices[3 * primitiveID + texIndexOffset + 2];

        const vec2 t0 = texCoords[t0Index].xy;
        const vec2 t1 = texCoords[t1Index].xy;
        const vec2 t2 = texCoords[t2Index].xy;

        result.uv = t0 * barycentrics.x + t1 * barycentrics.y + t2 * barycentrics.z;
    }

    // Transform normals from object space to world space
    result.worldNormal = normalize(mat3(gl_ObjectToWorldEXT) * objectNormal);
    result.worldNormalGeometry = normalize(mat3(gl_ObjectToWorldEXT) * objectNormalGeometry);

    result.frontFace = dot(gl_WorldRayDirectionEXT, result.worldNormalGeometry) < 0;
    result.worldNormal = faceforward(result.worldNormal, gl_WorldRayDirectionEXT, result.worldNormalGeometry);
    result.worldNormalGeometry = faceforward(result.worldNormalGeometry, gl_WorldRayDirectionEXT, result.worldNormalGeometry);

    // TBN stuff
    result.tbn = mat3(1.0);
    const uint tbnsIndexOffset = props.tbnsIndicesOffset;
    mat3 vertex1 = tbns[tbnsIndices[3 * primitiveID + tbnsIndexOffset + 0]];
    mat3 vertex2 = tbns[tbnsIndices[3 * primitiveID + tbnsIndexOffset + 1]];
    mat3 vertex3 = tbns[tbnsIndices[3 * primitiveID + tbnsIndexOffset + 2]];

    vec3 tangent = normalize(vertex1[0] * barycentrics.x + vertex2[0] * barycentrics.y + vertex3[0] * barycentrics.z);
    vec3 bitangent = normalize(vertex1[1] * barycentrics.x + vertex2[1] * barycentrics.y + vertex3[1] * barycentrics.z);
    vec3 normal = normalize(vertex1[2] * barycentrics.x + vertex2[2] * barycentrics.y + vertex3[2] * barycentrics.z);

    mat3 M = mat3(gl_ObjectToWorldEXT);
    mat3 N = transpose(inverse(M));

    vec3 worldT = normalize(M * tangent);
    vec3 worldB = normalize(M * bitangent);
    vec3 worldN = normalize(N * normal);

    // Re-orthagonalization
    worldT = normalize(worldT - worldN * dot(worldN, worldT));
    worldB = normalize(worldB - worldN * dot(worldN, worldB));

    worldB *= -1.0;
    worldN *= result.frontFace ? 1.0 : -1.0;

    result.tbn = mat3(worldT, worldB, worldN);

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

/*
 * Keep the ray moving in the same direction and origin and enable the 'skip' flag, which tells the raygen shader to
 * not count this ray to the total color. This is useful for skipping rays that hit the back face of an object.
 * I can't just turn on back face culling because some materials (like dielectrics) require the back face to be
 * considered.
 */
void skip(HitInfo hitInfo) {
    // ignore back faces. this should ideally be done in the any hit shader but I don't feel like modifying the SBT
    // right now.
    // todo: do this in the any hit shader instead
    pld.rayOrigin = offsetPositionAlongNormal(hitInfo.worldPosition, -hitInfo.worldNormal);
    pld.rayDirection = gl_WorldRayDirectionEXT;
    pld.rayHitSky = false;
    pld.skip = true;
}

vec3 randomUnitVec(inout uint rngState) {
    // todo: see if this method or sampling a sphere is faster. profile it
    while (true) {
        vec3 vector = 2 * vec3(random(rngState), random(rngState), random(rngState)) - vec3(1);

        float lenSquared = dot(vector, vector);
        if (0.0001 < lenSquared && lenSquared < 1) {
            return normalize(vector);
        }
    }
}

vec3 addFuzz(vec3 ray, float fuzz, inout uint rngState) {
    return ray + fuzz * randomUnitVec(rngState);
}

vec3 fuzzyReflection(vec3 rayIn, vec3 normal, float fuzz, inout uint rngState) {
    return reflect(normalize(rayIn), normalize(normal)) + fuzz * randomUnitVec(pld.rngState);
}

vec3 diffuseReflection(vec3 normal, inout uint rngState) {
    const float theta = 2.0 * k_pi * random(rngState);  // Random in [0, 2pi]
    const float u = 2.0 * random(rngState) - 1.0;   // Random in [-1, 1]
    const float r = sqrt(1.0 - u * u);
    const vec3 direction = normal + vec3(r * cos(theta), r * sin(theta), u);

    return normalize(direction);
}

#endif  // #ifndef REINA_CLOSEST_HIT_COMMON_H