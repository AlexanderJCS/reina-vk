#ifndef REINA_VK_REINA_H
#define REINA_VK_REINA_H

#include <vector>
#include <optional>
#include <vulkan/vulkan_core.h>

#include "tools/vktools.h"
#include "core/Buffer.h"
#include "graphics/Instances.h"
#include "core/CmdBuffer.h"
#include "window/Window.h"
#include "graphics/Camera.h"

class Reina {
public:
    Reina();
    void renderLoop();
    ~Reina();

private:
    uint32_t renderWidth, renderHeight;
    int windowWidth, windowHeight;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    std::vector<reina::graphics::Shader> shaders;
    vktools::SbtSpacing sbtSpacing;
    reina::core::PushConstants pushConstants;
    reina::graphics::Camera camera;
    reina::window::Window renderWindow;
    VkInstance instance;
    VkDevice logicalDevice;
    std::vector<VkFramebuffer> framebuffers;
    reina::graphics::Blas light;
    reina::graphics::Blas box;
    reina::graphics::Blas subject;
    vktools::AccStructureInfo tlas;
    reina::core::Buffer sbtBuffer;
    reina::core::Buffer objectPropertiesBuffer;
    reina::graphics::Image postprocessingOutputImage;
    reina::graphics::Image rtImage;
    reina::graphics::Instances instances;
    reina::core::Buffer stagingBuffer;
    reina::core::CmdBuffer commandBuffer;
    reina::core::DescriptorSet rtDescriptorSet;
    reina::core::DescriptorSet postprocessingDescriptorSet;
    reina::core::DescriptorSet rasterizationDescriptorSet;
    reina::graphics::Shader postprocessingShader;
    reina::graphics::Models models;
    vktools::SyncObjects syncObjects;
    VkSampler fragmentImageSampler;
    VkRenderPass renderPass;
    vktools::PipelineInfo rtPipeline;
    vktools::PipelineInfo rasterPipeline;
    vktools::PipelineInfo postprocessingPipeline;
    reina::graphics::Image pingImage;
    reina::graphics::Image pongImage;

    reina::graphics::Shader blurXShader;
    reina::core::DescriptorSet blurXDescriptorSet;
    vktools::PipelineInfo blurXPipeline;

    reina::graphics::Shader blurYShader;
    reina::core::DescriptorSet blurYDescriptorSet;
    vktools::PipelineInfo blurYPipeline;

    reina::graphics::Shader combineShader;
    reina::core::DescriptorSet combineDescriptorSet;
    vktools::PipelineInfo combinePipeline;

    VkCommandPool commandPool;
    std::vector<VkImageView> swapchainImageViews;
    vktools::SwapchainObjects swapchainObjects;
    std::optional<VkDebugUtilsMessengerEXT> debugMessenger;
    VkSurfaceKHR surface;
};


#endif //REINA_VK_REINA_H
