#ifndef RAYGUN_VK_BUFFER_H
#define RAYGUN_VK_BUFFER_H

#include <vulkan/vulkan.h>
#include <vector>

#include "CmdBuffer.h"

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
        Buffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::vector<T>& data, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags)
                : Buffer(logicalDevice, physicalDevice, data.empty() ? 0 : sizeof(data[0]) * data.size(), usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, allocFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT){
            VkBufferUsageFlags usageFlagsStaging = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            VkMemoryPropertyFlags memFlagsStaging = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            Buffer stagingBuffer{logicalDevice, physicalDevice, data, usageFlagsStaging, allocFlags, memFlagsStaging};

            reina::core::CmdBuffer oneTime{logicalDevice, cmdPool, true};
            copyFrom(oneTime, stagingBuffer);
            oneTime.endWaitSubmit(logicalDevice, queue);
            oneTime.destroy(logicalDevice);

            stagingBuffer.destroy(logicalDevice);
        }

        void copyFrom(const reina::core::CmdBuffer& cmdBuffer, const Buffer& src);

        template<typename T>
        std::vector<T> copyToHost(VkDevice logicalDevice) {
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
        [[nodiscard]] VkDeviceSize getSize() const;

        void destroy(VkDevice logicalDevice);

    private:
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
    };
}


#endif //RAYGUN_VK_BUFFER_H
