#ifndef RAYGUN_VK_CONSTS_H
#define RAYGUN_VK_CONSTS_H

#include <vulkan/vulkan.h>
#include <array>

namespace consts {
    const std::array<const char*, 1> VALIDATION_LAYERS{
        "VK_LAYER_KHRONOS_validation"
    };

    // array size + 1 with RT validation enabled
    const std::array<const char*, 9> DEVICE_EXTENSIONS{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,  // for debug printf
//        "VK_NV_ray_tracing_validation"
    };

#ifdef NDEBUG
    const bool ENABLE_VALIDATION_LAYERS = false;
#else
    const bool ENABLE_VALIDATION_LAYERS = true;
#endif
}

#endif //RAYGUN_VK_CONSTS_H
