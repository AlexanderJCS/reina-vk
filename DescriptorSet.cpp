#include "DescriptorSet.h"

#include <stdexcept>

int DescriptorSet::idxCounter = 0;

VkDescriptorSetLayoutBinding Binding::toLayoutBinding() const {
    return VkDescriptorSetLayoutBinding{
        .binding = bindingPoint,
        .descriptorType = type,
        .descriptorCount = descriptorCount,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        .pImmutableSamplers = nullptr
    };
}

DescriptorSet::DescriptorSet(VkDevice logicalDevice, const std::vector<Binding>& bindings) : setIdx(idxCounter++), bindings(bindings) {
    if (hasDuplicateBindingPoints(bindings)) {
        throw std::runtime_error("Cannot initialize descriptor set: duplicate binding points found");
    }

    std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());

    for (size_t i = 0; i < bindings.size(); ++i) {
        vkBindings[i] = bindings[i].toLayoutBinding();
    }

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<uint32_t>(vkBindings.size()),
            .pBindings = vkBindings.data()
    };

    if (vkCreateDescriptorSetLayout(logicalDevice, &layoutCreateInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Cannot create descriptor set layout");
    }

    VkDescriptorPoolSize poolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = static_cast<uint32_t>(vkBindings.size())
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
    };

    if (vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool,
            .descriptorSetCount = static_cast<uint32_t>(vkBindings.size()),
            .pSetLayouts = &layout
    };

    if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }
}

void DescriptorSet::destroy(VkDevice logicalDevice) {
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(logicalDevice, layout, nullptr);
    }

    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(logicalDevice, pool, nullptr);
    }
}

bool DescriptorSet::hasDuplicateBindingPoints(const std::vector<Binding>& bindings) {
    for (int i = 0; i < bindings.size(); i++) {
        for (int j = i + 1; j < bindings.size(); j++) {
            if (bindings[i].bindingPoint == bindings[j].bindingPoint) {
                return true;
            }
        }
    }

    return false;
}

VkDescriptorSetLayout DescriptorSet::getLayout() const {
    return layout;
}

VkDescriptorPool DescriptorSet::getPool() const {
    return pool;
}

VkDescriptorSet DescriptorSet::getDescriptorSet() const {
    return descriptorSet;
}

void DescriptorSet::bind(VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout) {
    vkCmdBindDescriptorSets(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            pipelineLayout,
            0,
            bindings.size(), &descriptorSet,
            0, nullptr
    );
}
