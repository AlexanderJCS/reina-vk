#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <vulkan/vulkan.h>

#include "vktools.h"
#include "consts.h"
#include "Window.h"
#include "DescriptorSet.h"

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
    window::Window renderWindow{800, 600};
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

    DescriptorSet descriptorSet{
        logicalDevice,
            {
                Binding{
                    0,
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                    VkDescriptorImageInfo{.imageView = rtImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL}
                }
        }
    };

    vktools::SbtSpacing sbtSpacing = vktools::calculateSbtSpacing(physicalDevice);
    std::vector<Shader> shaders = {
            Shader(logicalDevice, "../shaders/raygen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR)
    };

    vktools::PipelineInfo rtPipelineInfo = vktools::createRtPipeline(logicalDevice, descriptorSet, shaders);
    vktools::SbtInfo sbtInfo = vktools::createSbt(logicalDevice, physicalDevice, rtPipelineInfo.pipeline, sbtSpacing);

    for (Shader& shader : shaders) {
        shader.destroy(logicalDevice);
    }

    vktools::SyncObjects syncObjects = vktools::createSyncObjects(logicalDevice);

    // future: create render pass, frame buffer, and (maybe?) graphics pipeline for imgui stuff

    VkCommandPool commandPool = vktools::createCommandPool(physicalDevice, logicalDevice, surface);
    VkCommandBuffer commandBuffer = vktools::createCommandBuffer(logicalDevice, commandPool);

    // render
    while (!renderWindow.shouldClose()) {
        vkWaitForFences(logicalDevice, 1, &syncObjects.inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(logicalDevice, 1, &syncObjects.inFlightFence);


        VkCommandBufferBeginInfo beginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = 0,  // optional
                .pInheritanceInfo = nullptr  // optional
        };

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        transitionImage(
                commandBuffer,
                rtImageObjects.image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                static_cast<VkAccessFlagBits>(0), VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
        );

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineInfo.pipeline);

        descriptorSet.bind(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineInfo.pipelineLayout);
        descriptorSet.writeBindings(logicalDevice);

        // todo: push the push constants

        VkStridedDeviceAddressRegionKHR sbtRayGenRegion, sbtMissRegion, sbtHitRegion, sbtCallableRegion;
        VkDeviceAddress sbtStartAddress = getBufferDeviceAddress(logicalDevice, sbtInfo.buffer);

        sbtRayGenRegion.deviceAddress = sbtStartAddress;
        sbtRayGenRegion.stride = sbtSpacing.stride;
        sbtRayGenRegion.size = sbtSpacing.stride;

        sbtMissRegion = sbtRayGenRegion;
        sbtMissRegion.size = 0;  // empty

        sbtHitRegion = sbtRayGenRegion;
        sbtHitRegion.size = 0;

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

        uint32_t imageIndex = -1;
        if (!renderWindow.isMinimized()) {
            VkResult result = vkAcquireNextImageKHR(logicalDevice, swapchainObjects.swapchain, UINT64_MAX, syncObjects.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                // I don't feel like coding swapchain recreation
                throw std::runtime_error("Swapchain is either out of date or suboptimal");
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to acquire swapchain image");
            }
        }

        // Transition rayTracingImage to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        transitionImage(
                commandBuffer,
                rtImageObjects.image,
                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                VK_PIPELINE_STAGE_TRANSFER_BIT
        );

        // Transition swapchain image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        if (!renderWindow.isMinimized()) {

            transitionImage(
                    commandBuffer,
                    swapchainObjects.swapchainImages[imageIndex],
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    static_cast<VkAccessFlagBits>(0),
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
            );
        }

        // Define the blit region
        if (!renderWindow.isMinimized()) {
            VkImageBlit blitRegion{};
            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.mipLevel = 0;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.srcOffsets[0] = {0, 0, 0}; // Start at the top-left
            blitRegion.srcOffsets[1] = {static_cast<int32_t>(swapchainObjects.swapchainExtent.width), static_cast<int32_t>(swapchainObjects.swapchainExtent.height), 1}; // Match the source image dimensions

            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.mipLevel = 0;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstOffsets[0] = {0, 0, 0}; // Start at the top-left
            blitRegion.dstOffsets[1] = {static_cast<int32_t>(swapchainObjects.swapchainExtent.width), static_cast<int32_t>(swapchainObjects.swapchainExtent.height), 1}; // Match the destination image dimensions

            vkCmdBlitImage(
                    commandBuffer,
                    rtImageObjects.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    swapchainObjects.swapchainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blitRegion,
                    VK_FILTER_LINEAR // Use linear filtering for HDR downsampling
            );

            // Transition the destination image back to its original layout (e.g., for presentation)
            transitionImage(
                    commandBuffer,
                    swapchainObjects.swapchainImages[imageIndex],
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_TRANSFER_WRITE_BIT, static_cast<VkAccessFlagBits>(0),
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
            );
        }

        vkEndCommandBuffer(commandBuffer);

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

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, syncObjects.inFlightFence);

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
    }

    vkDeviceWaitIdle(logicalDevice);

    // clean up
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
    descriptorSet.destroy(logicalDevice);
    vkDestroySemaphore(logicalDevice, syncObjects.renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(logicalDevice, syncObjects.imageAvailableSemaphore, nullptr);
    vkDestroyFence(logicalDevice, syncObjects.inFlightFence, nullptr);
    vkDestroyBuffer(logicalDevice, sbtInfo.buffer, nullptr);
    vkFreeMemory(logicalDevice, sbtInfo.deviceMemory, nullptr);
    vkDestroyPipeline(logicalDevice, rtPipelineInfo.pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, rtPipelineInfo.pipelineLayout, nullptr);
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