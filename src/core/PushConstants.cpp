#include "PushConstants.h"

#include <stdexcept>

reina::core::PushConstants::PushConstants(const PushConstantsStruct& defaultValues, VkShaderStageFlags stageFlags)
        : stageFlags(stageFlags), data(defaultValues) {
    if (sizeof(PushConstantsStruct) % 4 != 0) {
        throw std::runtime_error("Could not create push constants since the Vulkan spec requires them to be aligned to 4 bytes");
    }

    pushConstantRange = {
        .stageFlags = stageFlags,
        .offset = 0,
        .size = sizeof(PushConstantsStruct)
    };
}

PushConstantsStruct& reina::core::PushConstants::getPushConstants() {
    return data;
}

void reina::core::PushConstants::push(VkCommandBuffer cmdBuffer, VkPipelineLayout pipeLayout) {
    vkCmdPushConstants(
            cmdBuffer,
            pipeLayout,
            stageFlags,
            0,
            sizeof(PushConstantsStruct),
            &data
            );
}

const VkPushConstantRange& reina::core::PushConstants::getRange() const {
    return pushConstantRange;
}
