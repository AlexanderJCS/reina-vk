#include "Image.h"

#include <stdexcept>
#include "../tools/vktools.h"

reina::graphics::Image::Image(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
        : width(width), height(height), image(VK_NULL_HANDLE), imageView(VK_NULL_HANDLE), imageMemory(VK_NULL_HANDLE) {
    createImage(logicalDevice, physicalDevice, width, height, format, VK_IMAGE_TILING_OPTIMAL, usage, properties);
    createImageView(logicalDevice, format);
}

void reina::graphics::Image::createImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
                        VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                        VkMemoryPropertyFlags properties) {

    VkImageCreateInfo imageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {
                    .width = width,
                    .height = height,
                    .depth = 1
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = vktools::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };

    if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory)) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

void reina::graphics::Image::createImageView(VkDevice logicalDevice, VkFormat imageFormat) {
    VkImageViewCreateInfo imageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = imageFormat,
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            }
    };

    if (vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }
}

VkImage reina::graphics::Image::getImage() {
    return image;
}

VkImageView reina::graphics::Image::getImageView() {
    return imageView;
}

void reina::graphics::Image::transition(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, VkAccessFlags newAccessMask, VkPipelineStageFlags newPipelineStages) {
    VkImageMemoryBarrier rayTracingToGeneralBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = accessMask,
            .dstAccessMask = newAccessMask,
            .oldLayout = layout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
            }
    };

    vkCmdPipelineBarrier(
            cmdBuffer,
            pipelineStages,
            newPipelineStages,
            0,
            0, nullptr,
            0, nullptr,
            1, &rayTracingToGeneralBarrier
    );
}

void reina::graphics::Image::destroy(VkDevice logicalDevice) {
    vkDestroyImage(logicalDevice, image, nullptr);
    vkFreeMemory(logicalDevice, imageMemory, nullptr);
    vkDestroyImageView(logicalDevice, imageView, nullptr);
}

void reina::graphics::Image::copyToBuffer(VkCommandBuffer cmdBuffer, VkBuffer dstBuffer) {
    VkBufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1}
    };

    vkCmdCopyImageToBuffer(
            cmdBuffer, getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuffer,
            1, &region
    );
}
