#ifndef RAYGUN_VK_PUSHCONSTANTS_H
#define RAYGUN_VK_PUSHCONSTANTS_H

#include "../../polyglot/common.h"

#include <vulkan/vulkan.h>

namespace reina::core {
    class PushConstants {
    public:
        PushConstants(const PushConstantsStruct& defaultValues, VkShaderStageFlags stageFlags);

        [[nodiscard]] PushConstantsStruct& getPushConstants();
        [[nodiscard]] const VkPushConstantRange& getRange() const;

        void push(VkCommandBuffer cmdBuffer, VkPipelineLayout pipeLayout);

    private:
        PushConstantsStruct data;
        VkPushConstantRange pushConstantRange;
        VkShaderStageFlags stageFlags;
    };
}


#endif //RAYGUN_VK_PUSHCONSTANTS_H
