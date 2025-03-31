#include "Image.h"

#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "../tools/vktools.h"
#include "../core/CmdBuffer.h"
#include "../core/Buffer.h"

reina::graphics::Image::Image(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::string& filepath) {
    // read image with stb: https://solarianprogrammer.com/2019/06/10/c-programming-reading-writing-images-stb_image-libraries/
    int imageWidth, imageHeight, channels;

    stbi_set_flip_vertically_on_load(true);
    uint8_t* imgData = stbi_load(filepath.c_str(), &imageWidth, &imageHeight, &channels, 4);

    if (imgData == nullptr) {
        throw std::runtime_error("Could not load image at path: " + filepath);
    }

    width = static_cast<uint32_t>(imageWidth);
    height = static_cast<uint32_t>(imageHeight);

    std::vector<uint8_t> imgDataVec = std::vector<uint8_t>(imgData, imgData + width * height * 4);

    reina::core::Buffer stagingBuffer = reina::core::Buffer{
        logicalDevice, physicalDevice, imgDataVec,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    createImage(logicalDevice, physicalDevice, width, height, format, VK_IMAGE_TILING_OPTIMAL, usage, properties);
    createImageView(logicalDevice, format);

    reina::core::CmdBuffer cmdBuffer{logicalDevice, cmdPool, true};

    transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(cmdBuffer.getHandle(), stagingBuffer.getHandle(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    cmdBuffer.endWaitSubmit(logicalDevice, queue);

    cmdBuffer.destroy(logicalDevice);
    stagingBuffer.destroy(logicalDevice);
    stbi_image_free(imgData);
}

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

VkImage reina::graphics::Image::getImage() const {
    return image;
}

VkImageView reina::graphics::Image::getImageView() const {
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
