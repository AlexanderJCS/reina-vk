#include "DescriptorSet.h"

#include <stdexcept>
#include <iostream>

VkDescriptorSetLayoutBinding Binding::toLayoutBinding() const {
    return VkDescriptorSetLayoutBinding{
        .binding = bindingPoint,
        .descriptorType = type,
        .descriptorCount = descriptorCount,
        .stageFlags = stageFlags,
        .pImmutableSamplers = nullptr
    };
}

DescriptorSet::DescriptorSet(VkDevice logicalDevice, const std::vector<Binding>& bindings)
        : bindings(bindings) {
    if (hasDuplicateBindingPoints(bindings)) {
        throw std::runtime_error("Cannot initialize descriptor set: duplicate binding points found");
    }

    // Create descriptor set layout
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

    // Create descriptor pool (account for all descriptor types in bindings)
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto& binding : bindings) {
        bool typeExists = false;
        for (auto& poolSize : poolSizes) {
            if (poolSize.type == binding.type) {
                poolSize.descriptorCount += 1;
                typeExists = true;
                break;
            }
        }
        if (!typeExists) {
            poolSizes.push_back(VkDescriptorPoolSize{
                    .type = binding.type,
                    .descriptorCount = 1
            });
        }
    }

    VkDescriptorPoolCreateInfo poolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1, // Allocate 1 descriptor set
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
    };

    if (vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool,
            .descriptorSetCount = 1, // Allocate 1 descriptor set
            .pSetLayouts = &layout   // Use the layout created earlier
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
            1,
            &descriptorSet,
            0,
            nullptr
    );
}

void DescriptorSet::writeBinding(
        VkDevice logicalDevice, int bindingPoint,
        VkDescriptorImageInfo* imageInfo,
        VkDescriptorBufferInfo* bufferInfo,
        VkBufferView* bufferView,
        void* next
        ) {

    for (const Binding& binding : bindings) {
        if (binding.bindingPoint != bindingPoint) {
            continue;
        }

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = binding.bindingPoint;
        descriptorWrite.dstArrayElement = 0;  // assuming we are not working with arrays of descriptors per binding
        descriptorWrite.descriptorType = binding.type;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = imageInfo;
        descriptorWrite.pBufferInfo = bufferInfo;
        descriptorWrite.pTexelBufferView = bufferView;
        descriptorWrite.pNext = next;

        vkUpdateDescriptorSets(logicalDevice, 1, &descriptorWrite, 0, nullptr);
        break;
    }
}
