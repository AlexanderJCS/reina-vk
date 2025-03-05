#ifndef REINA_VK_IMAGE_H
#define REINA_VK_IMAGE_H

#include <vulkan/vulkan.h>

namespace reina::graphics {
    class Image {
    public:
        Image(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps);

        [[nodiscard]] VkImage getImage();
        [[nodiscard]] VkImageView getImageView();

        void transition(VkCommandBuffer cmdBuffer, VkImageLayout newLayout, VkAccessFlags newAccessMask, VkPipelineStageFlags newPipelineStages);
        void copyToBuffer(VkCommandBuffer cmdBuffer, VkBuffer dstBuffer);

        void destroy(VkDevice logicalDevice);
    private:
        void createImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
        void createImageView(VkDevice logicalDevice, VkFormat imageFormat);

        uint32_t width, height;

        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;

        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags accessMask = static_cast<VkAccessFlags>(0);
        VkPipelineStageFlags pipelineStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    };
}

#endif //REINA_VK_IMAGE_H
