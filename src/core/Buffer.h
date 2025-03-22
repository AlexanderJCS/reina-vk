#ifndef RAYGUN_VK_BUFFER_H
#define RAYGUN_VK_BUFFER_H

#include <vulkan/vulkan.h>
#include <vector>

namespace reina::core {
    class Buffer {
    public:
        Buffer() = default;
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

        template<typename T>
        std::vector<T> copyData(VkDevice logicalDevice) {
            void* data;
            vkMapMemory(logicalDevice, getDeviceMemory(), 0, size, 0, &data);

            std::vector<T> result(size / sizeof(T));
            memcpy(result.data(), data, static_cast<size_t>(size));

            vkUnmapMemory(logicalDevice, getDeviceMemory());
            return result;
        }

        [[nodiscard]] VkBuffer getHandle() const;
        [[nodiscard]] VkDeviceMemory getDeviceMemory() const;
        [[nodiscard]] VkDeviceAddress getDeviceAddress(VkDevice logicalDevice) const;

        void destroy(VkDevice logicalDevice);

    private:
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
    };
}


#endif //RAYGUN_VK_BUFFER_H
