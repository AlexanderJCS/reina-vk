#ifndef REINA_SHADER_COMMON_H
#define REINA_SHADER_COMMON_H

#extension GL_EXT_scalar_block_layout : require

layout(binding = 1, set = 0) uniform accelerationStructureEXT tlas;

layout(binding = 2, set = 0, scalar) buffer Vertices {
    vec4 vertices[];
};

layout(binding = 3, set = 0, scalar) buffer Indices {
    uint indices[];
};

struct HitPayload {
    vec3 color;         // The reflectivity of the surface.
    vec3 rayOrigin;     // The new ray origin in world-space.
    vec3 rayDirection;  // The new ray direction in world-space.
    uint rngState;      // State of the random number generator.
    bool rayHitSky;     // True if the ray hit the sky.
    vec4 emission;      // xyz: emission color, w: emission strength
    vec3 surfaceNormal; // The normal of the surface hit by the ray.
    uint materialID;    // 0 for lambertian, 1 for metal, 2 for dielectric
    bool skip;          // If true, the raygen shader knows to skip this ray
    float accumulatedDistance;  // Used for Beer's law. The distance the ray has traveled inside any media.
    bool insideDielectric;  // Apply color for this iteration
};

// Steps the RNG and returns a floating-point value between 0 and 1 inclusive.
float stepAndOutputRNGFloat(inout uint rngState) {
    // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
    rngState  = rngState * 747796405 + 1;
    uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
    word = (word >> 22) ^ word;
    return float(word) / 4294967295.0f;
}

const float k_pi = 3.14159265;

#endif  // #ifndef REINA_SHADER_COMMON_H