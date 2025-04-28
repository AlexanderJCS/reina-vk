#ifndef REINA_VK_IMAGE_H
#define REINA_VK_IMAGE_H

#include <vulkan/vulkan.h>
#include <string>
#include <span>

namespace reina::graphics {
    class Image {
    public:
        Image() = default;
        Image(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::string& filepath);
        Image(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, std::span<const std::byte> imageData);
        Image(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps);

        [[nodiscard]] VkImage getImage() const;
        [[nodiscard]] VkImageView getImageView() const;

        void transition(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, VkAccessFlags newAccessMask, VkPipelineStageFlags newPipelineStages);
        void copyToBuffer(VkCommandBuffer cmdBuffer, VkBuffer dstBuffer);

        void destroy(VkDevice logicalDevice);
    private:
        void load(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, uint8_t* imgData, int imageWidth, int imageHeight);

        void createImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
        void createImageView(VkDevice logicalDevice, VkFormat imageFormat);

        uint32_t width = 0, height = 0;

        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;

        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags accessMask = static_cast<VkAccessFlags>(0);
        VkPipelineStageFlags pipelineStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    };
}

#endif //REINA_VK_IMAGE_H
