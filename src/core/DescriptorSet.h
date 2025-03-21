#ifndef RAYGUN_VK_DESCRIPTORSET_H
#define RAYGUN_VK_DESCRIPTORSET_H

#include <vulkan/vulkan.h>

#include <vector>
#include <optional>

namespace reina::core {
    struct Binding {
        uint32_t bindingPoint;
        VkDescriptorType type;
        uint32_t descriptorCount;
        VkShaderStageFlagBits stageFlags;

        [[nodiscard]] VkDescriptorSetLayoutBinding toLayoutBinding() const;
    };

    class DescriptorSet {
    private:
        std::vector<Binding> bindings;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        [[nodiscard]] static bool hasDuplicateBindingPoints(const std::vector<Binding>& bindings);

        void writeBinding(VkDevice logicalDevice, int bindingPoint, VkDescriptorImageInfo *imageInfo,
                          VkDescriptorBufferInfo *bufferInfo, void *next);

    public:
        DescriptorSet(VkDevice logicalDevice, const std::vector<Binding>& bindings);

        void bind(VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout);

        void writeBinding(VkDevice logicalDevice, int bindingPoint, VkDescriptorBufferInfo* bufferInfo);
        void writeBinding(VkDevice logicalDevice, int bindingPoint, VkDescriptorImageInfo* imageInfo);
        void writeBinding(VkDevice logicalDevice, int bindingPoint, VkWriteDescriptorSetAccelerationStructureKHR* accStructureInfo);

        void destroy(VkDevice device);

        [[nodiscard]] VkDescriptorSetLayout getLayout() const;
        [[nodiscard]] VkDescriptorPool getPool() const;
        [[nodiscard]] VkDescriptorSet getDescriptorSet() const;
    };
}

#endif //RAYGUN_VK_DESCRIPTORSET_H