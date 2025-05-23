#ifndef RAYGUN_VK_BLAS_H
#define RAYGUN_VK_BLAS_H

#include <vulkan/vulkan.h>
#include <optional>
#include <glm/mat4x4.hpp>

#include "../scene/Models.h"
#include "../core/Buffer.h"

// todo: accumulate all BLASes then build them all at once for faster performance
namespace reina::graphics {
    class Blas {
    public:
        Blas() = default;
        Blas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const reina::scene::Models& models, const reina::scene::ModelRange& modelRange, bool shouldCompact);

        [[nodiscard]] VkAccelerationStructureKHR getHandle() const;
        [[nodiscard]] const reina::core::Buffer& getBuffer() const;


        void destroy(VkDevice logicalDevice);

    private:
        /**
         * Compacts the BLAS.
         * @param logicalDevice The Vulkan logical device
         * @param physicalDevice The Vulkan physical device
         * @param cmdPool A command pool
         * @param queue A queue
         * @return The size of the compacted BLAS.
         */
        VkDeviceSize compact(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue);

        reina::core::Buffer blasBuffer;
        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    };
}

#endif //RAYGUN_VK_BLAS_H
