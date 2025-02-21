#ifndef RAYGUN_VK_POLYGLOT_COMMON_H
#define RAYGUN_VK_POLYGLOT_COMMON_H

#ifdef __cplusplus
    #include <cstdint>
    #include <glm/vec3.hpp>
    using uint = uint32_t;
    using vec3 = glm::vec3;
#endif  // #ifdef __cplusplus

#define SAMPLES_PER_PIXEL 64
#define BOUNCES_PER_SAMPLE 16

struct PushConstantsStruct {
    vec3 cameraPos;
    uint sampleBatch;
};

#endif // #ifndef RAYGUN_VK_POLYGLOT_COMMON_H