#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <iostream>
#include <vulkan/vulkan.h>

#include "vktools.h"
#include "consts.h"
#include "Window.h"
#include "DescriptorSet.h"
#include "PushConstants.h"

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

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    tinyobj::ObjReader reader;
    reader.ParseFromFile("../models/cornell_box.obj");

    if (!reader.Valid()) {
        throw std::runtime_error("Error reading OBJ:\n" + reader.Error());
    }

    std::vector<tinyobj::real_t> objVertices = reader.GetAttrib().GetVertices();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    if (shapes.size() != 1) {
        throw std::runtime_error("Several shapes to parse; need only one");
    }
    std::vector<uint32_t> objIndices(shapes[0].mesh.indices.size());
    for (const tinyobj::index_t& index : shapes[0].mesh.indices) {
        objIndices.push_back(index.vertex_index);
    }

    vktools::BufferObjects verticesBuffer = vktools::createBuffer(
            logicalDevice,
            physicalDevice,
            objVertices,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
    vktools::BufferObjects indicesBuffer = vktools::createBuffer(
            logicalDevice,
            physicalDevice,
            objIndices,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    DescriptorSet descriptorSet{
        logicalDevice,
            {
                Binding{
                    0,
                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR
                },
                Binding{
                    1,
                    VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                    1,
                    VK_SHADER_STAGE_RAYGEN_BIT_KHR
                },
                Binding{
                    2,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    1,
                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                },
                Binding{
                    3,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    1,
                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                }
        }
    };

    PushConstants pushConstants{PushConstantsStruct{0}, VK_SHADER_STAGE_RAYGEN_BIT_KHR};

    vktools::SbtSpacing sbtSpacing = vktools::calculateSbtSpacing(physicalDevice);
    std::vector<Shader> shaders = {
            Shader(logicalDevice, "../shaders/raytrace.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR),
            Shader(logicalDevice, "../shaders/raytrace.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR),
            Shader(logicalDevice, "../shaders/raytrace.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
    };

    vktools::PipelineInfo rtPipelineInfo = vktools::createRtPipeline(logicalDevice, descriptorSet, shaders, pushConstants);
    vktools::BufferObjects sbtInfo = vktools::createSbt(logicalDevice, physicalDevice, rtPipelineInfo.pipeline, sbtSpacing, 3);

    for (Shader& shader : shaders) {
        shader.destroy(logicalDevice);
    }

    vktools::SyncObjects syncObjects = vktools::createSyncObjects(logicalDevice);

    VkCommandPool commandPool = vktools::createCommandPool(physicalDevice, logicalDevice, surface);
    VkCommandBuffer commandBuffer = vktools::createCommandBuffer(logicalDevice, commandPool);

    vktools::AccStructureInfo blas = vktools::createBlas(
            logicalDevice, physicalDevice, commandPool, graphicsQueue,
            verticesBuffer.buffer, indicesBuffer.buffer, objVertices.size(),
            objIndices.size()
            );

    vktools::AccStructureInfo tlas = vktools::createTlas(
            logicalDevice, physicalDevice, commandPool, graphicsQueue,
            {blas.accelerationStructure}, sbtSpacing.stride
            );

    // render
    while (!renderWindow.shouldClose()) {
        if (vkWaitForFences(logicalDevice, 1, &syncObjects.inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            throw std::runtime_error("Could not wait for fences");
        }

        if (vkResetFences(logicalDevice, 1, &syncObjects.inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Could not reset fences");
        }


        VkCommandBufferBeginInfo beginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = 0,  // optional
                .pInheritanceInfo = nullptr  // optional
        };

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Could not begin command buffer");
        }

        transitionImage(
                commandBuffer,
                rtImageObjects.image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                static_cast<VkAccessFlagBits>(0), VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
        );

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineInfo.pipeline);

        descriptorSet.bind(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineInfo.pipelineLayout);

        VkDescriptorImageInfo descriptorImageInfo{.imageView = rtImageView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
        descriptorSet.writeBinding(logicalDevice, 0, &descriptorImageInfo, nullptr, nullptr, nullptr);

        VkWriteDescriptorSetAccelerationStructureKHR descriptorAccStructure{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas.accelerationStructure
        };
        descriptorSet.writeBinding(logicalDevice, 1, nullptr, nullptr, nullptr, &descriptorAccStructure);

        VkDescriptorBufferInfo verticesInfo{
            .buffer = verticesBuffer.buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };

        descriptorSet.writeBinding(logicalDevice, 2, nullptr, &verticesInfo, nullptr, nullptr);

        VkDescriptorBufferInfo indicesInfo{
                .buffer = indicesBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
        };

        descriptorSet.writeBinding(logicalDevice, 3, nullptr, &indicesInfo, nullptr, nullptr);

        pushConstants.push(commandBuffer, rtPipelineInfo.pipelineLayout);
        pushConstants.getPushConstants().sampleBatch++;

        VkStridedDeviceAddressRegionKHR sbtRayGenRegion, sbtMissRegion, sbtHitRegion, sbtCallableRegion;
        VkDeviceAddress sbtStartAddress = getBufferDeviceAddress(logicalDevice, sbtInfo.buffer);

        sbtRayGenRegion.deviceAddress = sbtStartAddress;
        sbtRayGenRegion.stride = sbtSpacing.stride;
        sbtRayGenRegion.size = sbtSpacing.stride;

        sbtMissRegion = sbtRayGenRegion;
        sbtMissRegion.deviceAddress = sbtStartAddress + sbtSpacing.stride;
        sbtMissRegion.size = sbtSpacing.stride;  // empty

        sbtHitRegion = sbtRayGenRegion;
        sbtHitRegion.deviceAddress = sbtStartAddress + 2 * sbtSpacing.stride;
        sbtHitRegion.size = sbtSpacing.stride * 1 /* todo: since there's only one hit shader */;

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
    }

    vkDeviceWaitIdle(logicalDevice);

    // clean up
    auto vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkDestroyAccelerationStructureKHR"));

    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
    vkDestroyAccelerationStructureKHR(logicalDevice, blas.accelerationStructure, nullptr);
    vkDestroyAccelerationStructureKHR(logicalDevice, tlas.accelerationStructure, nullptr);
    descriptorSet.destroy(logicalDevice);
    vkDestroySemaphore(logicalDevice, syncObjects.renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(logicalDevice, syncObjects.imageAvailableSemaphore, nullptr);
    vkDestroyFence(logicalDevice, syncObjects.inFlightFence, nullptr);
    vkDestroyBuffer(logicalDevice, verticesBuffer.buffer, nullptr);
    vkFreeMemory(logicalDevice, verticesBuffer.deviceMemory, nullptr);
    vkDestroyBuffer(logicalDevice, indicesBuffer.buffer, nullptr);
    vkFreeMemory(logicalDevice, indicesBuffer.deviceMemory, nullptr);
    vkDestroyBuffer(logicalDevice, blas.buffer.buffer, nullptr);
    vkFreeMemory(logicalDevice, blas.buffer.deviceMemory, nullptr);
    vkDestroyBuffer(logicalDevice, tlas.buffer.buffer, nullptr);
    vkFreeMemory(logicalDevice, tlas.buffer.deviceMemory, nullptr);
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