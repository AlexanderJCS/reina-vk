#ifndef REINA_VK_REINA_H
#define REINA_VK_REINA_H

#include <vector>
#include <optional>
#include <vulkan/vulkan_core.h>

#include "tools/vktools.h"
#include "core/Buffer.h"
#include "core/CmdBuffer.h"
#include "window/Window.h"
#include "graphics/Camera.h"
#include "tools/SaveManager.h"
#include "scene/Scene.h"

#include "../polyglot/raytrace.h"
#include "../polyglot/bloom.h"
#include "../polyglot/tonemapping.h"

class Reina {
public:
    Reina();
    void renderLoop();
    ~Reina();

private:
    void writeDescriptorSets();
    void traceRays();
    void applyBloom();
    void applyTonemapping();
    void draw(uint32_t& imageIndex);
    void present(uint32_t imageIndex);

    uint32_t renderWidth, renderHeight;
    int windowWidth, windowHeight;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    std::vector<reina::graphics::Shader> shaders;
    vktools::SbtSpacing sbtSpacing;
    reina::core::PushConstants<RtPushConsts> rtPushConsts;
    reina::core::PushConstants<BloomPushConsts> bloomPushConsts;
    reina::core::PushConstants<TonemappingPushConsts> tonemapPushConsts;
    reina::graphics::Camera camera;
    reina::window::Window renderWindow;
    VkInstance instance;
    VkDevice logicalDevice;
    reina::scene::Scene scene;
    std::vector<VkFramebuffer> framebuffers;
    reina::core::Buffer sbtBuffer;
    reina::graphics::Image tonemapOutputImage;
    reina::graphics::Image rtImage;
    reina::core::Buffer stagingBuffer;
    reina::core::CmdBuffer cmdBuffer;
    reina::core::DescriptorSet rtDescriptorSet;
    reina::core::DescriptorSet tonemapDescriptorSet;
    reina::core::DescriptorSet rasterDescriptorSet;
    reina::graphics::Shader tonemapShader;
    vktools::SyncObjects syncObjects;
    VkSampler fragmentImageSampler;
    VkRenderPass renderPass;
    vktools::PipelineInfo rtPipeline;
    vktools::PipelineInfo rasterPipeline;
    vktools::PipelineInfo tonemapPipeline;
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

    reina::tools::SaveManager saveManager;

    uint32_t originalSamplesPerPixel = 0;
};


#endif //REINA_VK_REINA_H
