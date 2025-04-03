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

struct ObjectProperties {
    uint indicesBytesOffset;
    vec3 albedo;
    vec4 emission;
    uint normalsIndicesBytesOffset;
    uint texIndicesBytesOffset;
    float fuzzOrRefIdx;
    bool interpNormals;
    float absorption;
    int textureID;
    vec2 padding;
};

#ifdef __cplusplus
    inline glm::vec3 emissionAsVec3(const ObjectProperties& props) {
        return glm::vec3(props.emission.x, props.emission.y, props.emission.z) * props.emission.w;
    }
#endif  // #ifdef __cplusplus

struct RtPushConsts {
    mat4 invView;
    mat4 invProjection;
    uint sampleBatch;
    float totalEmissiveArea;
    float focusDist;
    float defocusMultiplier;
    float directClamp;
    float indirectClamp;
    uint samplesPerPixel;
    uint maxBounces;
};

#endif // #ifndef RAYGUN_VK_POLYGLOT_COMMON_H