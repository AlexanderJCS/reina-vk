#include <stdexcept>
#include "Buffer.h"

#include "vktools.h"

Buffer::Buffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkDeviceSize dataSize, VkBufferUsageFlags usage,
               VkMemoryAllocateFlags allocFlags, VkMemoryPropertyFlags memFlags) {

    VkBufferCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = dataSize,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    if (vkCreateBuffer(logicalDevice, &createInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = allocFlags
    };

    VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &allocFlagsInfo,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = vktools::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, memFlags)
    };

    if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &deviceMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(logicalDevice, buffer, deviceMemory, 0);
}

VkBuffer Buffer::getBuffer() const {
    return buffer;
}

VkDeviceMemory Buffer::getDeviceMemory() const {
    return deviceMemory;
}

void Buffer::destroy(VkDevice logicalDevice) {
    vkDestroyBuffer(logicalDevice, buffer, nullptr);
    vkFreeMemory(logicalDevice, deviceMemory, nullptr);
}
