#ifndef RAYGUN_VK_POLYGLOT_COMMON_H
#define RAYGUN_VK_POLYGLOT_COMMON_H

#ifdef __cplusplus
    #include <cstdint>
    #include <glm/vec3.hpp>
    #include <glm/mat4x4.hpp>
    using uint = uint32_t;
    using vec3 = glm::vec3;
    using mat4 = glm::mat4;
#endif  // #ifdef __cplusplus

#define SAMPLES_PER_PIXEL 32
#define BOUNCES_PER_SAMPLE 10

// define this to show normals on non-dielectric surfaces
#define DEBUG_SHOW_NORMALS

struct PushConstantsStruct {
    mat4 invView;
    mat4 invProjection;
    uint sampleBatch;
    uint directClamp;
    uint indirectClamp;
};

#endif // #ifndef RAYGUN_VK_POLYGLOT_COMMON_H