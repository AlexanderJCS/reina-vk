#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vulkan/vulkan.h>

#include "tools/vktools.h"
#include "tools/consts.h"
#include "window/Window.h"
#include "core/DescriptorSet.h"
#include "core/PushConstants.h"
#include "graphics/Models.h"
#include "graphics/ObjectProperties.h"
#include "graphics/Blas.h"
#include "graphics/Instance.h"

VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo addressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer };
    return vkGetBufferDeviceAddress(device, &addressInfo);
}

void transitionImage(
        VkCommandBuffer cmdBuffer,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkAccessFlagBits srcAccessMask,
        VkAccessFlagBits dstAccessMask,
        VkPipelineStageFlagBits srcStageMask,
        VkPipelineStageFlagBits dstStageMask
) {
    VkImageMemoryBarrier rayTracingToGeneralBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vkCmdPipelineBarrier(
            cmdBuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &rayTracingToGeneralBarrier
    );
}


void run() {
    // init
    rt::window::Window renderWindow{800, 800};
    VkInstance instance = vktools::createInstance();
    std::optional<VkDebugUtilsMessengerEXT> debugMessenger = vktools::createDebugMessenger(instance);
    VkSurfaceKHR surface = vktools::createSurface(instance, renderWindow.getGlfwWindow());
    VkPhysicalDevice physicalDevice = vktools::pickPhysicalDevice(instance, surface);
    VkDevice logicalDevice = vktools::createLogicalDevice(surface, physicalDevice);

    vktools::QueueFamilyIndices indices = vktools::findQueueFamilies(surface, physicalDevice);
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);

    vktools::SwapchainObjects swapchainObjects = vktools::createSwapchain(surface, physicalDevice, logicalDevice, renderWindow.getWidth(), renderWindow.getHeight());
    std::vector<VkImageView> swapchainImageViews = vktools::createSwapchainImageViews(logicalDevice, swapchainObjects.swapchainImageFormat, swapchainObjects.swapchainImages);

    vktools::ImageObjects rtImageObjects = vktools::createRtImage(logicalDevice, physicalDevice, swapchainObjects.swapchainExtent.width, swapchainObjects.swapchainExtent.height);
    VkImageView rtImageView = vktools::createRtImageView(logicalDevice, rtImageObjects.image);

    rt::core::DescriptorSet rtDescriptorSet{
        logicalDevice,
            {
                rt::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                rt::core::Binding{1,VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                rt::core::Binding{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                rt::core::Binding{3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                rt::core::Binding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}
        }
    };

    std::vector<rt::graphics::ObjectProperties> objectProperties{
            {glm::vec3{0.9}, 0, glm::vec4(0), 0},
            {glm::vec3{0.9}, 0, glm::vec4(1, 1, 1, 20), 0},
            {glm::vec3(0.9, 0.3, 0.5), 0, glm::vec4(0), 0}
    };
    rt::core::Buffer objectPropertiesBuffer{
        logicalDevice, physicalDevice, objectProperties,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        static_cast<VkMemoryAllocateFlags>(0),
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
    };

    rt::core::PushConstants pushConstants{PushConstantsStruct{0}, VK_SHADER_STAGE_RAYGEN_BIT_KHR};

    vktools::SbtSpacing sbtSpacing = vktools::calculateSbtSpacing(physicalDevice);
    std::vector<rt::graphics::Shader> shaders = {
            rt::graphics::Shader(logicalDevice, "../shaders/raytrace.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR),
            rt::graphics::Shader(logicalDevice, "../shaders/raytrace.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR),
            rt::graphics::Shader(logicalDevice, "../shaders/lambertian.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            rt::graphics::Shader(logicalDevice, "../shaders/metal.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            rt::graphics::Shader(logicalDevice, "../shaders/dielectric.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
    };

    vktools::PipelineInfo rtPipelineInfo = vktools::createRtPipeline(logicalDevice, rtDescriptorSet, shaders, pushConstants);
    rt::core::Buffer sbtBuffer = vktools::createSbt(logicalDevice, physicalDevice, rtPipelineInfo.pipeline, sbtSpacing, shaders.size());

    for (rt::graphics::Shader& shader : shaders) {
        shader.destroy(logicalDevice);
    }

    rt::core::DescriptorSet rasterizationDescriptorSet{
        logicalDevice,
        {
                rt::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
        }
    };

    rt::graphics::Shader vertexShader = rt::graphics::Shader(logicalDevice, "../shaders/display.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    rt::graphics::Shader fragmentShader = rt::graphics::Shader(logicalDevice, "../shaders/display.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkRenderPass renderPass = vktools::createRenderPass(logicalDevice, swapchainObjects.swapchainImageFormat);
    vktools::PipelineInfo rasterizationPipelineInfo = vktools::createRasterizationPipeline(logicalDevice, rasterizationDescriptorSet, renderPass, vertexShader, fragmentShader);

    std::vector<VkFramebuffer> framebuffers = vktools::createSwapchainFramebuffers(logicalDevice, renderPass, swapchainObjects.swapchainExtent, swapchainImageViews);

    vertexShader.destroy(logicalDevice);
    fragmentShader.destroy(logicalDevice);

    vktools::SyncObjects syncObjects = vktools::createSyncObjects(logicalDevice);

    VkCommandPool commandPool = vktools::createCommandPool(physicalDevice, logicalDevice, surface);
    VkCommandBuffer commandBuffer = vktools::createCommandBuffer(logicalDevice, commandPool);

    rt::graphics::Models models{logicalDevice, physicalDevice, {"../models/stanford_bunny.obj", "../models/empty_cornell_box.obj", "../models/cornell_light.obj"}};
    rt::graphics::Blas box{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(1)};
    rt::graphics::Blas light{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(2)};
    rt::graphics::Blas bunny{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(0)};

    glm::mat4x4 baseTransform = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));

    std::vector<rt::graphics::Instance> instances{
            {box, 0, 0, baseTransform},
            {light, 1, 0, baseTransform},
            {bunny, 0, 0, baseTransform}
    };

    vktools::AccStructureInfo tlas = vktools::createTlas(logicalDevice, physicalDevice, commandPool, graphicsQueue, instances);


    // render
    bool firstFrame = true;
    while (!renderWindow.shouldClose()) {
        if (vkWaitForFences(logicalDevice, 1, &syncObjects.inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            throw std::runtime_error("Could not wait for fences");
        }

        if (vkResetFences(logicalDevice, 1, &syncObjects.inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Could not reset fences");
        }

        VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Could not begin command buffer");
        }

        transitionImage(
                commandBuffer,
                rtImageObjects.image,
                firstFrame ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                firstFrame ? static_cast<VkAccessFlagBits>(0) : VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                firstFrame ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
        );

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineInfo.pipeline);

        rtDescriptorSet.bind(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineInfo.pipelineLayout);

        VkDescriptorImageInfo descriptorImageInfo{.imageView = rtImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
        rtDescriptorSet.writeBinding(logicalDevice, 0, &descriptorImageInfo, nullptr, nullptr, nullptr);

        VkWriteDescriptorSetAccelerationStructureKHR descriptorAccStructure{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas.accelerationStructure
        };
        rtDescriptorSet.writeBinding(logicalDevice, 1, nullptr, nullptr, nullptr, &descriptorAccStructure);

        VkDescriptorBufferInfo verticesInfo{.buffer = models.getVerticesBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
        rtDescriptorSet.writeBinding(logicalDevice, 2, nullptr, &verticesInfo, nullptr, nullptr);

        VkDescriptorBufferInfo indicesInfo{.buffer = models.getIndicesBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
        rtDescriptorSet.writeBinding(logicalDevice, 3, nullptr, &indicesInfo, nullptr, nullptr);

        VkDescriptorBufferInfo objPropertiesInfo{.buffer = objectPropertiesBuffer.getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
        rtDescriptorSet.writeBinding(logicalDevice, 4, nullptr, &objPropertiesInfo, nullptr, nullptr);

        pushConstants.push(commandBuffer, rtPipelineInfo.pipelineLayout);
        pushConstants.getPushConstants().sampleBatch++;

        VkStridedDeviceAddressRegionKHR sbtRayGenRegion, sbtMissRegion, sbtHitRegion, sbtCallableRegion;
        VkDeviceAddress sbtStartAddress = getBufferDeviceAddress(logicalDevice, sbtBuffer.getHandle());

        sbtRayGenRegion.deviceAddress = sbtStartAddress;
        sbtRayGenRegion.stride = sbtSpacing.stride;
        sbtRayGenRegion.size = sbtSpacing.stride;

        sbtMissRegion = sbtRayGenRegion;
        sbtMissRegion.deviceAddress = sbtStartAddress + sbtSpacing.stride;
        sbtMissRegion.size = sbtSpacing.stride;

        sbtHitRegion = sbtRayGenRegion;
        sbtHitRegion.deviceAddress = sbtStartAddress + 2 * sbtSpacing.stride;
        sbtHitRegion.size = sbtSpacing.stride * (shaders.size() - 2);  // assuming shaders vector includes 2 non-rchit shaders

        sbtCallableRegion = sbtRayGenRegion;
        sbtCallableRegion.size = 0;

        auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
                vkGetDeviceProcAddr(logicalDevice, "vkCmdTraceRaysKHR"));

        if (!vkCmdTraceRaysKHR) {
            throw std::runtime_error("Failed to load vkCmdTraceRaysKHR");
        }

        vkCmdTraceRaysKHR(
                commandBuffer,
                &sbtRayGenRegion,
                &sbtMissRegion,
                &sbtHitRegion,
                &sbtCallableRegion,
                swapchainObjects.swapchainExtent.width,
                swapchainObjects.swapchainExtent.height,
                1
        );

        // everything below here is swapchain stuff

        // transition to the same and synchronize
        transitionImage(
                commandBuffer,
                rtImageObjects.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        );

        uint32_t imageIndex = -1;
        if (!renderWindow.isMinimized()) {
            VkResult result = vkAcquireNextImageKHR(logicalDevice, swapchainObjects.swapchain, UINT64_MAX, syncObjects.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("Swapchain is either out of date or suboptimal");
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to acquire swapchain image");
            }
        }

        if (!renderWindow.isMinimized()) {
            VkClearValue clearColor = {{0, 0, 0, 1}};

            VkRenderPassBeginInfo renderPassBeginInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = renderPass,
                .framebuffer = framebuffers[imageIndex],
                .renderArea = {
                        .offset = {0, 0},
                        .extent = swapchainObjects.swapchainExtent
                },
                .clearValueCount = 1,
                .pClearValues = &clearColor
            };

            vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkDescriptorImageInfo readImageInfo{.imageView = rtImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

            rasterizationDescriptorSet.bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterizationPipelineInfo.pipelineLayout);
            rasterizationDescriptorSet.writeBinding(logicalDevice, 0, &readImageInfo, nullptr, nullptr, nullptr);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterizationPipelineInfo.pipeline);

            VkViewport viewport{
                .x = 0,
                .y = 0,
                .width = static_cast<float>(swapchainObjects.swapchainExtent.width),
                .height = static_cast<float>(swapchainObjects.swapchainExtent.height),
                .minDepth = 0,
                .maxDepth = 1
            };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{
                .offset = {0, 0},
                .extent = swapchainObjects.swapchainExtent
            };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdDraw(commandBuffer, 6, 1, 0, 0);

            vkCmdEndRenderPass(commandBuffer);
        }

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Could not end command buffer");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {syncObjects.imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitInfo.waitSemaphoreCount = renderWindow.isMinimized() ? 0 : 1;
        submitInfo.pWaitSemaphores = renderWindow.isMinimized() ? VK_NULL_HANDLE : waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkSemaphore signalSemaphores[] = {syncObjects.renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = renderWindow.isMinimized() ? 0 : 1;
        submitInfo.pSignalSemaphores = renderWindow.isMinimized() ? VK_NULL_HANDLE : signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, syncObjects.inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Could not submit graphics queue");
        }

        // Present the swapchain image
        if (!renderWindow.isMinimized()) {
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchainObjects.swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(presentQueue, &presentInfo);
        }

        glfwPollEvents();
        firstFrame = false;
    }

    vkDeviceWaitIdle(logicalDevice);

    // clean up
    auto vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkDestroyAccelerationStructureKHR"));

    if (!vkDestroyAccelerationStructureKHR) {
        throw std::runtime_error("Destroy acceleration structure function cannot be found");
    }

    for (VkFramebuffer framebuffer : framebuffers) {
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }

    light.destroy(logicalDevice);
    box.destroy(logicalDevice);
    bunny.destroy(logicalDevice);
    tlas.buffer.destroy(logicalDevice);
    sbtBuffer.destroy(logicalDevice);
    objectPropertiesBuffer.destroy(logicalDevice);

    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
    vkDestroyAccelerationStructureKHR(logicalDevice, tlas.accelerationStructure, nullptr);
    rtDescriptorSet.destroy(logicalDevice);
    rasterizationDescriptorSet.destroy(logicalDevice);
    vkDestroySemaphore(logicalDevice, syncObjects.renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(logicalDevice, syncObjects.imageAvailableSemaphore, nullptr);
    vkDestroyFence(logicalDevice, syncObjects.inFlightFence, nullptr);
    models.destroy(logicalDevice);
    vkDestroyPipeline(logicalDevice, rtPipelineInfo.pipeline, nullptr);
    vkDestroyPipeline(logicalDevice, rasterizationPipelineInfo.pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, rtPipelineInfo.pipelineLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, rasterizationPipelineInfo.pipelineLayout, nullptr);
    vkDestroyImageView(logicalDevice, rtImageView, nullptr);
    vkDestroyImage(logicalDevice, rtImageObjects.image, nullptr);
    vkFreeMemory(logicalDevice, rtImageObjects.imageMemory, nullptr);

    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

    for (VkImageView imageView : swapchainImageViews) {
        vkDestroyImageView(logicalDevice, imageView, nullptr);
    }

    vkDestroySwapchainKHR(logicalDevice, swapchainObjects.swapchain, nullptr);

    vkDestroyDevice(logicalDevice, nullptr);

    if (debugMessenger.has_value()) {
        vktools::DestroyDebugUtilsMessengerEXT(instance, debugMessenger.value(), nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    renderWindow.destroy();
}


int main() {
    try {
        run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}