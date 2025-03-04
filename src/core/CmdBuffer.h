#ifndef REINA_VK_CMDBUFFER_H
#define REINA_VK_CMDBUFFER_H

#include <vulkan/vulkan.h>
#include <optional>

namespace reina::core {
    class CmdBuffer {
    public:
        CmdBuffer(VkDevice logicalDevice, VkCommandPool cmdPool, bool oneTime, bool fenceCreateSignaled = false);

        [[nodiscard]] VkCommandBuffer getHandle() const;

        void begin();
        void endSubmit(VkDevice logicalDevice, VkQueue queue, const std::optional<VkSubmitInfo>& submitInfo = std::nullopt);
        void wait(VkDevice logicalDevice);
        void endWaitSubmit(VkDevice logicalDevice, VkQueue queue, const std::optional<VkSubmitInfo>& submitInfo = std::nullopt);
        void destroy(VkDevice logicalDevice);
    private:
        VkCommandPool createdCmdPool;
        VkCommandBuffer cmdBuffer;
        VkFence fence;
        bool oneTime;
    };
}


#endif //REINA_VK_CMDBUFFER_H
