#ifndef REINA_VK_CMDBUFFER_H
#define REINA_VK_CMDBUFFER_H

#include <vulkan/vulkan.h>
#include <optional>

namespace reina::core {
    class CmdBuffer {
    public:
        CmdBuffer() = default;
        CmdBuffer(VkDevice logicalDevice, VkCommandPool cmdPool, bool oneTime, bool fenceCreateSignaled = false);

        [[nodiscard]] VkCommandBuffer getHandle() const;

        void begin();
        void endSubmit(VkDevice logicalDevice, VkQueue queue, const std::optional<VkSubmitInfo>& submitInfo = std::nullopt);
        void wait(VkDevice logicalDevice);
        void endWaitSubmit(VkDevice logicalDevice, VkQueue queue, const std::optional<VkSubmitInfo>& submitInfo = std::nullopt);
        void destroy(VkDevice logicalDevice);
    private:
        VkCommandPool createdCmdPool = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        bool oneTime = true;
    };
}


#endif //REINA_VK_CMDBUFFER_H
