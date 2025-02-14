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
        Blas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const Model& model, int objectPropertyID, glm::mat4x4 transform = glm::mat4x4{1.0f});

        [[nodiscard]] VkAccelerationStructureKHR getHandle() const;
        [[nodiscard]] rt::core::Buffer getBuffer() const;  // todo: return a const reference
        [[nodiscard]] int getObjectPropertyID() const;
        [[nodiscard]] const glm::mat4x4& getTransform() const;


        void destroy(VkDevice logicalDevice);

    private:
        int objectPropertyID;
        std::optional<rt::core::Buffer> blasBuffer;
        VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
        glm::mat4x4 transform;
    };
}

#endif //RAYGUN_VK_BLAS_H
