#ifndef RAYGUN_VK_POLYGLOT_COMMON_H
#define RAYGUN_VK_POLYGLOT_COMMON_H

// define this to show normals on non-dielectric surfaces
//#define DEBUG_SHOW_NORMALS

#ifdef __cplusplus
    #include <cstdint>
    #include <glm/vec3.hpp>
    #include <glm/mat4x4.hpp>
    using uint = uint32_t;
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;
    using mat4 = glm::mat4;
#endif  // #ifdef __cplusplus

struct InstanceProperties {
    uint indicesOffset;
    vec3 albedo;
    vec3 emission;
    uint tbnsIndicesOffset;
    uint texIndicesOffset;
    float roughness;
    float ior;
    bool interpNormals;
    float absorption;
    int textureID;
    int normalMapTexID;
    int bumpMapTexID;
    uint cullBackface;
    float anisotropic;
    float subsurface;
    float clearcoatGloss;
    vec3 sheenTint;
    vec3 specularTint;
    float metallic;
    float clearcoat;
    float specularTransmission;
    float sheen;
};

struct RtPushConsts {
    mat4 invView;
    mat4 invProjection;
    uint sampleBatch;
    float totalEmissiveWeight;
    float focusDist;
    float defocusMultiplier;
    float directClamp;
    float indirectClamp;
    uint samplesPerPixel;
    uint maxBounces;
};

#endif // #ifndef RAYGUN_VK_POLYGLOT_COMMON_H