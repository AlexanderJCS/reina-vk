#include "DescriptorSet.h"

#include <stdexcept>

int DescriptorSet::idxCounter = 0;

VkDescriptorSetLayoutBinding Binding::toLayoutBinding() {
    return VkDescriptorSetLayoutBinding{
        .binding = bindingPoint,
        .descriptorType = type,
        .descriptorCount = descriptorCount,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        .pImmutableSamplers = nullptr
    };
}

DescriptorSet::DescriptorSet() : setIdx(idxCounter++) {

}

void DescriptorSet::addBinding(Binding binding) {
    for (Binding existingBinding : bindings) {
        if (existingBinding.bindingPoint == binding.bindingPoint) {
            throw std::runtime_error("Cannot add binding with binding point " + std::to_string(bindingPoint) + " since a binding with that point already exists");
        }
    }

    bindings.push_back(binding);
}

void DescriptorSet::removeBinding(uint32_t point) {
    for (int i = 0; i < bindings.size(); i++) {
        if (bindings[i].bindingPoint != point) {
            continue;
        }

        bindings.erase(bindings.begin() + i);
        break;
    }
}

void DescriptorSet::init(VkDevice logicalDevice) {
    std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());

    for (Binding binding : bindings) {
        vkBindings.push_back(binding.toLayoutBinding());
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
        .descriptorCount = 1
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
}

void DescriptorSet::destroy(VkDevice logicalDevice) {
    if (layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(logicalDevice, layout, nullptr);
    }

    if (pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(logicalDevice, pool, nullptr);
    }
}
