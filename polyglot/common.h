#ifndef RAYGUN_VK_POLYGLOT_COMMON_H
#define RAYGUN_VK_POLYGLOT_COMMON_H

#ifdef __cplusplus
    #include <cstdint>
    using uint = uint32_t;
#endif  // #ifdef __cplusplus

#define SAMPLES_PER_PIXEL 64
#define BOUNCES_PER_SAMPLE 16

struct PushConstantsStruct {
    uint sampleBatch;
};

#endif // #ifndef RAYGUN_VK_POLYGLOT_COMMON_H