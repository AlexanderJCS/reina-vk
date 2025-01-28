#ifndef RAYGUN_VK_DESCRIPTORSET_H
#define RAYGUN_VK_DESCRIPTORSET_H

#include <vulkan/vulkan.h>

#include <vector>

struct Binding {
    uint32_t bindingPoint;
    VkDescriptorType type;
    uint32_t descriptorCount;
    VkShaderStageFlagBits stageFlags;

    VkDescriptorSetLayoutBinding toLayoutBinding();
};

// todo: have DescriptorSet take in a std::vector of Binding, then init inside the constructor
class DescriptorSet {
private:
    static int idxCounter;
    int setIdx;
    std::vector<Binding> bindings;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;

public:
    DescriptorSet();

    void addBinding(Binding binding);
    void removeBinding(uint32_t point);

    void init(VkDevice logicalDevice);

    void destroy(VkDevice device);
};

#endif //RAYGUN_VK_DESCRIPTORSET_H