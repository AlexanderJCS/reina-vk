#ifndef RAYGUN_VK_BLAS_H
#define RAYGUN_VK_BLAS_H

#include <vulkan/vulkan.h>
#include <optional>
#include <glm/mat4x4.hpp>

#include "Models.h"
#include "../core/Buffer.h"

// todo: accumulate all BLASes then build them all at once for faster performance
namespace reina::graphics {
    class Blas {
    public:
        Blas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const Models& models, const reina::graphics::ModelRange& modelRange, bool compact);

        [[nodiscard]] VkAccelerationStructureKHR getHandle() const;
        [[nodiscard]] const reina::core::Buffer& getBuffer() const;


        void destroy(VkDevice logicalDevice);

    private:
        std::optional<reina::core::Buffer> blasBuffer;
        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    };
}

#endif //RAYGUN_VK_BLAS_H
