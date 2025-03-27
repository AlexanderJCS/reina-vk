#ifndef RAYGUN_VK_PUSHCONSTANTS_H
#define RAYGUN_VK_PUSHCONSTANTS_H

#include "../../polyglot/common.h"

#include <vulkan/vulkan.h>

namespace reina::core {
    template<typename T>
    class PushConstants {
    public:
        PushConstants() = default;
        PushConstants(const T& defaultValues, VkShaderStageFlags stageFlags)
                : stageFlags(stageFlags), data(defaultValues) {
            if (sizeof(T) % 4 != 0) {
                throw std::runtime_error("Could not create push constants since the Vulkan spec requires them to be aligned to 4 bytes");
            }

            pushConstantRange = {
                    .stageFlags = stageFlags,
                    .offset = 0,
                    .size = sizeof(T)
            };
        }

        [[nodiscard]] T& getPushConstants() {
            return data;
        }

        [[nodiscard]] const VkPushConstantRange& getRange() const {
            return pushConstantRange;
        }

        void push(VkCommandBuffer cmdBuffer, VkPipelineLayout pipeLayout) {
            vkCmdPushConstants(
                    cmdBuffer,
                    pipeLayout,
                    stageFlags,
                    0,
                    sizeof(T),
                    &data
            );
        }

    private:
        T data;
        VkPushConstantRange pushConstantRange{};
        VkShaderStageFlags stageFlags;
    };
}

#endif //RAYGUN_VK_PUSHCONSTANTS_H
