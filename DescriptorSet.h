#ifndef RAYGUN_VK_DESCRIPTORSET_H
#define RAYGUN_VK_DESCRIPTORSET_H

#include <vulkan/vulkan.h>

#include <vector>

struct Binding {
    uint32_t bindingPoint;
    VkDescriptorType type;
    uint32_t descriptorCount;
    VkShaderStageFlagBits stageFlags;

    [[nodiscard]] VkDescriptorSetLayoutBinding toLayoutBinding() const;
};

class DescriptorSet {
private:
    static int idxCounter;
    int setIdx;
    const std::vector<Binding>& bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    [[nodiscard]] static bool hasDuplicateBindingPoints(const std::vector<Binding>& bindings);

public:
    DescriptorSet(VkDevice logicalDevice, const std::vector<Binding>& bindings);

    void bind(VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout);

    void destroy(VkDevice device);

    [[nodiscard]] VkDescriptorSetLayout getLayout() const;
    [[nodiscard]] VkDescriptorPool getPool() const;
    [[nodiscard]] VkDescriptorSet getDescriptorSet() const;
};

#endif //RAYGUN_VK_DESCRIPTORSET_H