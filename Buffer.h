#ifndef RAYGUN_VK_BUFFER_H
#define RAYGUN_VK_BUFFER_H

#include <vulkan/vulkan.h>
#include <vector>

namespace rt::core {
    class Buffer {
    public:
        Buffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkDeviceSize dataSize, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkMemoryPropertyFlags memFlags);

        template<typename T>
        Buffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<T>& data, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkMemoryPropertyFlags memFlags)
                : Buffer(logicalDevice, physicalDevice, data.empty() ? 0 : sizeof(data[0]) * data.size(), usage, allocFlags, memFlags)
        {
            void* bufferData;
            vkMapMemory(logicalDevice, deviceMemory, 0, data.size(), 0, &bufferData);
            memcpy(bufferData, data.data(), data.size() * sizeof(T));
            vkUnmapMemory(logicalDevice, deviceMemory);
        }

        [[nodiscard]] VkBuffer getBuffer() const;
        [[nodiscard]] VkDeviceMemory getDeviceMemory() const;

        void destroy(VkDevice logicalDevice);

    private:
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    };
}


#endif //RAYGUN_VK_BUFFER_H
