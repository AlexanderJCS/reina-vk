#ifndef RAYGUN_VK_BLAS_H
#define RAYGUN_VK_BLAS_H

#include <vulkan/vulkan.h>
#include <optional>
#include <glm/mat4x4.hpp>

#include "Model.h"
#include "../core/Buffer.h"

namespace rt::graphics {
    class Blas {
    public:
        Blas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const Model& model);

        [[nodiscard]] VkAccelerationStructureKHR getHandle() const;
        [[nodiscard]] const rt::core::Buffer& getBuffer() const;


        void destroy(VkDevice logicalDevice);

    private:
        std::optional<rt::core::Buffer> blasBuffer;
        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    };
}

#endif //RAYGUN_VK_BLAS_H
