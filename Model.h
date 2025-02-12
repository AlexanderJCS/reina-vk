#ifndef RAYGUN_VK_MODEL_H
#define RAYGUN_VK_MODEL_H

#include <vulkan/vulkan.h>
#include <optional>

#include "vktools.h"
#include "Buffer.h"

class Model {
public:
    Model(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::string& objFilepath);

    [[nodiscard]] VkBuffer getVerticesBuffer() const;
    [[nodiscard]] VkBuffer getIndicesBuffer() const;
    [[nodiscard]] size_t getVerticesBufferSize() const;
    [[nodiscard]] size_t getIndicesBufferSize() const;

    void destroy(VkDevice logicalDevice);

private:
    std::optional<Buffer> verticesBuffer;
    std::optional<Buffer> indicesBuffer;

    size_t verticesBufferSize;
    size_t indicesBufferSize;
};


#endif //RAYGUN_VK_MODEL_H
