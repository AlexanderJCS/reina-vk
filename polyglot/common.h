#ifndef RAYGUN_VK_POLYGLOT_COMMON_H
#define RAYGUN_VK_POLYGLOT_COMMON_H

#ifdef __cplusplus
#include <cstdint>
using uint = uint32_t;
#endif  // #ifdef __cplusplus

struct PushConstantsStruct {
    uint sampleBatch;
};

#endif // #ifndef RAYGUN_VK_POLYGLOT_COMMON_H