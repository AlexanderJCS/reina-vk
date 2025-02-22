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
#define BOUNCES_PER_SAMPLE 12

struct PushConstantsStruct {
    mat4 invView;
    mat4 invProjection;
    uint sampleBatch;
};

#endif // #ifndef RAYGUN_VK_POLYGLOT_COMMON_H