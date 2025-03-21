#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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
#include "tools/Clock.h"
#include "graphics/Camera.h"
#include "core/CmdBuffer.h"
#include "graphics/Image.h"
#include "graphics/Instances.h"


void run() {
    // init
    const uint32_t renderWidth = 1080;
    const uint32_t renderHeight = 1350;
    const float aspectRatio = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);

    const int windowWidth = 800;
    const int windowHeight = static_cast<int>(windowWidth / aspectRatio);

    reina::window::Window renderWindow{windowWidth, windowHeight};

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

    reina::graphics::Image rtImage{
        logicalDevice, physicalDevice, renderWidth, renderHeight, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    reina::core::DescriptorSet rtDescriptorSet{
        logicalDevice,
            {
                reina::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                reina::core::Binding{1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)},
                reina::core::Binding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)},
                reina::core::Binding{3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)},
                reina::core::Binding{4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                reina::core::Binding{5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                reina::core::Binding{6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                reina::core::Binding{7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)},
                reina::core::Binding{8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                reina::core::Binding{9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}
        }
    };

    glm::vec3 pos = glm::vec3(0, 1.3f, 8.4f);
    glm::vec3 lookAt = glm::vec3(0, 0.962f, 0);
    reina::graphics::Camera camera{renderWindow, glm::radians(15.0f), aspectRatio, pos, glm::normalize(lookAt - pos)};
    reina::core::PushConstants pushConstants{PushConstantsStruct{camera.getInverseView(), camera.getInverseProjection(), 0, 100, 10}, VK_SHADER_STAGE_RAYGEN_BIT_KHR};

    vktools::SbtSpacing sbtSpacing = vktools::calculateSbtSpacing(physicalDevice);
    std::vector<reina::graphics::Shader> shaders = {
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/lambertian.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/metal.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/dielectric.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
    };

    vktools::PipelineInfo rtPipelineInfo = vktools::createRtPipeline(logicalDevice, rtDescriptorSet, shaders, pushConstants);
    reina::core::Buffer sbtBuffer = vktools::createSbt(logicalDevice, physicalDevice, rtPipelineInfo.pipeline, sbtSpacing, shaders.size());

    for (reina::graphics::Shader& shader : shaders) {
        shader.destroy(logicalDevice);
    }

    reina::graphics::Image postprocessingOutputImage{
        logicalDevice, physicalDevice, renderWidth, renderHeight, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VkSampler fragmentImageSampler = vktools::createSampler(logicalDevice);

    reina::core::DescriptorSet postprocessingDescriptorSet{
        logicalDevice,
        {
            reina::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},  // input image
            reina::core::Binding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}   // output image
        }
    };
    reina::graphics::Shader postprocessingShader = reina::graphics::Shader(logicalDevice, "../shaders/postprocessing.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    vktools::PipelineInfo postprocessingPipeline = vktools::createComputePipeline(logicalDevice, postprocessingDescriptorSet, postprocessingShader);

    reina::core::DescriptorSet rasterizationDescriptorSet{
        logicalDevice,
        {
                reina::core::Binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
        }
    };

    reina::graphics::Shader vertexShader = reina::graphics::Shader(logicalDevice, "../shaders/display.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    reina::graphics::Shader fragmentShader = reina::graphics::Shader(logicalDevice, "../shaders/display.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkRenderPass renderPass = vktools::createRenderPass(logicalDevice, swapchainObjects.swapchainImageFormat);
    vktools::PipelineInfo rasterizationPipelineInfo = vktools::createRasterizationPipeline(logicalDevice, rasterizationDescriptorSet, renderPass, vertexShader, fragmentShader);

    std::vector<VkFramebuffer> framebuffers = vktools::createSwapchainFramebuffers(logicalDevice, renderPass, swapchainObjects.swapchainExtent, swapchainImageViews);

    vertexShader.destroy(logicalDevice);
    fragmentShader.destroy(logicalDevice);

    vktools::SyncObjects syncObjects = vktools::createSyncObjects(logicalDevice);

    VkCommandPool commandPool = vktools::createCommandPool(physicalDevice, logicalDevice, surface);

    reina::core::CmdBuffer commandBuffer{logicalDevice, commandPool, false, true};
    commandBuffer.endWaitSubmit(logicalDevice, graphicsQueue);  // since the command buffer automatically begins upon creation, and we don't want that in this specific case

    VkCommandBuffer cmdBufferHandle = commandBuffer.getHandle();

    reina::graphics::Models models{logicalDevice, physicalDevice, {"../models/ico_sphere_highres.obj", "../models/empty_cornell_box.obj", "../models/cornell_light.obj"}};
    reina::graphics::Blas box{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(1), true};
    reina::graphics::Blas light{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(2), true};
    reina::graphics::Blas subject{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(0), true};

    glm::mat4x4 baseTransform = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    glm::mat4x4 subjectTransform = glm::scale(glm::translate(baseTransform, glm::vec3(0.0f, 1.0f, 0)), glm::vec3(0.4f));

    reina::graphics::Instances instances{
        logicalDevice, physicalDevice,
        {
                {box, false, models.getModelRange(1), models.getObjData(1), 0, 0, baseTransform},
                {light, true, models.getModelRange(2), models.getObjData(2), 1, 0, baseTransform},
                {subject, false, models.getModelRange(0), models.getObjData(0), 2, 2, subjectTransform}
            },
    };

    vktools::AccStructureInfo tlas = vktools::createTlas(logicalDevice, physicalDevice, commandPool, graphicsQueue, instances.getInstances());

    std::vector<reina::graphics::ObjectProperties> objectProperties{
            {models.getModelRange(1).indexOffset, glm::vec3{0.9}, glm::vec4(0), models.getModelRange(1).normalsIndexOffset, 0.01, false, 0},
            {models.getModelRange(2).indexOffset, glm::vec3{0.9}, glm::vec4(3.5), models.getModelRange(2).normalsIndexOffset, 0, false, 0},
            {models.getModelRange(0).indexOffset, glm::vec3(1), glm::vec4(0), models.getModelRange(0).normalsIndexOffset, 1.4f, true, 0.7}
    };

    reina::core::Buffer objectPropertiesBuffer{
            logicalDevice, physicalDevice, objectProperties,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            static_cast<VkMemoryAllocateFlagBits>(0),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
    };

    VkDeviceSize imageSize = renderWidth * renderHeight * 4; // RGBA8

    reina::core::Buffer stagingBuffer{
        logicalDevice, physicalDevice, imageSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        static_cast<VkMemoryAllocateFlags>(0),
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    // render
    VkDescriptorImageInfo descriptorImageInfo{.imageView = rtImage.getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    rtDescriptorSet.writeBinding(logicalDevice, 0, &descriptorImageInfo, nullptr, nullptr, nullptr);

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccStructure{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas.accelerationStructure
    };
    rtDescriptorSet.writeBinding(logicalDevice, 1, nullptr, nullptr, nullptr, &descriptorAccStructure);

    VkDescriptorBufferInfo verticesInfo{.buffer = models.getVerticesBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    rtDescriptorSet.writeBinding(logicalDevice, 2, nullptr, &verticesInfo, nullptr, nullptr);

    VkDescriptorBufferInfo indicesInfo{.buffer = models.getOffsetIndicesBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    rtDescriptorSet.writeBinding(logicalDevice, 3, nullptr, &indicesInfo, nullptr, nullptr);

    VkDescriptorBufferInfo objPropertiesInfo{.buffer = objectPropertiesBuffer.getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    rtDescriptorSet.writeBinding(logicalDevice, 4, nullptr, &objPropertiesInfo, nullptr, nullptr);

    VkDescriptorBufferInfo normalsInfo{.buffer = models.getNormalsBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    rtDescriptorSet.writeBinding(logicalDevice, 5, nullptr, &normalsInfo, nullptr, nullptr);

    VkDescriptorBufferInfo normalsIndicesInfo{.buffer = models.getOffsetNormalsIndicesBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    rtDescriptorSet.writeBinding(logicalDevice, 6, nullptr, &normalsIndicesInfo, nullptr, nullptr);

    VkDescriptorBufferInfo emissiveMetadataBufferInfo{.buffer = instances.getEmissiveMetadataBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    rtDescriptorSet.writeBinding(logicalDevice, 7, nullptr, &emissiveMetadataBufferInfo, nullptr, nullptr);

    VkDescriptorBufferInfo cdfTriangleBufferInfo{.buffer = instances.getCdfTrianglesBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    rtDescriptorSet.writeBinding(logicalDevice, 8, nullptr, &cdfTriangleBufferInfo, nullptr, nullptr);

    VkDescriptorBufferInfo cdfInstancesBufferInfo{.buffer = instances.getCdfInstancesBuffer().getHandle(), .offset = 0, .range = VK_WHOLE_SIZE};
    rtDescriptorSet.writeBinding(logicalDevice, 9, nullptr, &cdfInstancesBufferInfo, nullptr, nullptr);

    VkDescriptorImageInfo rasterizationInputDescriptor{.sampler = fragmentImageSampler, .imageView = postprocessingOutputImage.getImageView(), .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    rasterizationDescriptorSet.writeBinding(logicalDevice, 0, &rasterizationInputDescriptor, nullptr, nullptr, nullptr);

    VkDescriptorImageInfo postprocessingInputDescriptor{.imageView = rtImage.getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    postprocessingDescriptorSet.writeBinding(logicalDevice, 0, &postprocessingInputDescriptor, nullptr, nullptr, nullptr);

    VkDescriptorImageInfo postprocessingOutputDescriptor{.imageView = postprocessingOutputImage.getImageView(), .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    postprocessingDescriptorSet.writeBinding(logicalDevice, 1, &postprocessingOutputDescriptor, nullptr, nullptr, nullptr);

    reina::tools::Clock clock;
    while (!renderWindow.shouldClose()) {
        // camera
        camera.processInput(renderWindow, clock.getTimeDelta());
        if (camera.hasChanged()) {
            camera.refresh();
            PushConstantsStruct& pushConstantsStruct = pushConstants.getPushConstants();
            pushConstantsStruct.invView = camera.getInverseView();
            pushConstantsStruct.invProjection = camera.getInverseProjection();
            pushConstantsStruct.sampleBatch = 0;  // reset the image
        }

        // clock
        bool firstFrame = clock.getFrameCount() == 0;

        if (!firstFrame) {
            std::cout << clock.summary() << "\n";
        }

        clock.markCategory("Ray Tracing");

        // render ray traced image
        commandBuffer.wait(logicalDevice);
        commandBuffer.begin();

        rtImage.transition(cmdBufferHandle, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

        vkCmdBindPipeline(cmdBufferHandle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineInfo.pipeline);
        rtDescriptorSet.bind(cmdBufferHandle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineInfo.pipelineLayout);

        pushConstants.push(cmdBufferHandle, rtPipelineInfo.pipelineLayout);
        pushConstants.getPushConstants().sampleBatch++;

        VkStridedDeviceAddressRegionKHR sbtRayGenRegion, sbtMissRegion, sbtHitRegion, sbtCallableRegion;
        VkDeviceAddress sbtStartAddress = sbtBuffer.getDeviceAddress(logicalDevice);

        sbtRayGenRegion.deviceAddress = sbtStartAddress;
        sbtRayGenRegion.stride = sbtSpacing.stride;
        sbtRayGenRegion.size = sbtSpacing.stride;

        sbtMissRegion = sbtRayGenRegion;
        sbtMissRegion.deviceAddress = sbtStartAddress + sbtSpacing.stride;
        sbtMissRegion.size = sbtSpacing.stride * 2;

        sbtHitRegion = sbtRayGenRegion;
        sbtHitRegion.deviceAddress = sbtStartAddress + 3 * sbtSpacing.stride;
        sbtHitRegion.size = sbtSpacing.stride * (shaders.size() - 3);  // assuming shaders vector includes 3 non-rchit shaders

        sbtCallableRegion = sbtRayGenRegion;
        sbtCallableRegion.size = 0;

        auto vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
                vkGetDeviceProcAddr(logicalDevice, "vkCmdTraceRaysKHR"));

        if (!vkCmdTraceRaysKHR) {
            throw std::runtime_error("Failed to load vkCmdTraceRaysKHR");
        }

        vkCmdTraceRaysKHR(
                cmdBufferHandle,
                &sbtRayGenRegion,
                &sbtMissRegion,
                &sbtHitRegion,
                &sbtCallableRegion,
                renderWidth,
                renderHeight,
                1
        );

        // everything below here is swapchain stuff
        clock.markCategory("Post Processing");

        // transition to the same and synchronize
        rtImage.transition(cmdBufferHandle, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        // apply postprocessing
        postprocessingOutputImage.transition(cmdBufferHandle, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        const int workgroupWidth = 32;
        const int workgroupHeight = 8;

        postprocessingDescriptorSet.bind(cmdBufferHandle, VK_PIPELINE_BIND_POINT_COMPUTE, postprocessingPipeline.pipelineLayout);
        vkCmdBindPipeline(cmdBufferHandle, VK_PIPELINE_BIND_POINT_COMPUTE, postprocessingPipeline.pipeline);
        vkCmdDispatch(
                cmdBufferHandle,
                (renderWidth + workgroupWidth - 1) / workgroupWidth,
                (renderHeight + workgroupHeight - 1) / workgroupHeight,
                1
                );

        // save
        clock.markCategory("Save");

        uint32_t samples = clock.getSampleCount();
//        if (samples == 64) {
        if (false) {
            postprocessingOutputImage.transition(cmdBufferHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            postprocessingOutputImage.copyToBuffer(cmdBufferHandle, stagingBuffer.getHandle());
            std::vector<uint8_t> pixels = stagingBuffer.copyData<uint8_t>(logicalDevice, imageSize);

            std::string filename = "../output.png";
            int success = stbi_write_png(
                    filename.c_str(),
                    static_cast<int>(renderWidth),
                    static_cast<int>(renderHeight),
                    4,
                    pixels.data(),
                    static_cast<int>(renderWidth) * 4
            );

            if (!success) {
                throw std::runtime_error("Could not save PNG");
            }

            postprocessingOutputImage.transition(cmdBufferHandle, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        }

        // render
        clock.markCategory("Display");

        postprocessingOutputImage.transition(cmdBufferHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        uint32_t imageIndex = -1;
        if (!renderWindow.isMinimized()) {
            VkResult result = vkAcquireNextImageKHR(logicalDevice, swapchainObjects.swapchain, UINT64_MAX, syncObjects.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                throw std::runtime_error("Swapchain is either out of date or suboptimal");
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to acquire swapchain image");
            }

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

            vkCmdBeginRenderPass(cmdBufferHandle, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            rasterizationDescriptorSet.bind(cmdBufferHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterizationPipelineInfo.pipelineLayout);

            vkCmdBindPipeline(cmdBufferHandle, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterizationPipelineInfo.pipeline);

            VkViewport viewport{
                .x = 0,
                .y = 0,
                .width = static_cast<float>(swapchainObjects.swapchainExtent.width),
                .height = static_cast<float>(swapchainObjects.swapchainExtent.height),
                .minDepth = 0,
                .maxDepth = 1
            };
            vkCmdSetViewport(cmdBufferHandle, 0, 1, &viewport);

            VkRect2D scissor{
                .offset = {0, 0},
                .extent = swapchainObjects.swapchainExtent
            };

            vkCmdSetScissor(cmdBufferHandle, 0, 1, &scissor);
            vkCmdDraw(cmdBufferHandle, 6, 1, 0, 0);
            vkCmdEndRenderPass(cmdBufferHandle);
        }

        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = static_cast<uint32_t>(renderWindow.isMinimized() ? 0 : 1),
            .pWaitSemaphores = renderWindow.isMinimized() ? VK_NULL_HANDLE : &syncObjects.imageAvailableSemaphore,
            .pWaitDstStageMask = waitStages,
            .signalSemaphoreCount = static_cast<uint32_t>(renderWindow.isMinimized() ? 0 : 1),
            .pSignalSemaphores = renderWindow.isMinimized() ? VK_NULL_HANDLE : &syncObjects.renderFinishedSemaphore
        };

        commandBuffer.endSubmit(logicalDevice, graphicsQueue, submitInfo);

        // Present the swapchain image
        if (!renderWindow.isMinimized()) {
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &syncObjects.renderFinishedSemaphore;

            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchainObjects.swapchain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(presentQueue, &presentInfo);
        }

        clock.markFrame();
        glfwPollEvents();
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
    subject.destroy(logicalDevice);
    tlas.buffer.destroy(logicalDevice);
    sbtBuffer.destroy(logicalDevice);
    objectPropertiesBuffer.destroy(logicalDevice);
    postprocessingOutputImage.destroy(logicalDevice);
    rtImage.destroy(logicalDevice);
    instances.destroy(logicalDevice);

    stagingBuffer.destroy(logicalDevice);

    vkDestroySampler(logicalDevice, fragmentImageSampler, nullptr);
    commandBuffer.destroy(logicalDevice);
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
    vkDestroyAccelerationStructureKHR(logicalDevice, tlas.accelerationStructure, nullptr);
    rtDescriptorSet.destroy(logicalDevice);
    postprocessingDescriptorSet.destroy(logicalDevice);
    postprocessingShader.destroy(logicalDevice);
    rasterizationDescriptorSet.destroy(logicalDevice);
    vkDestroySemaphore(logicalDevice, syncObjects.renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(logicalDevice, syncObjects.imageAvailableSemaphore, nullptr);
    models.destroy(logicalDevice);
    vkDestroyPipeline(logicalDevice, rtPipelineInfo.pipeline, nullptr);
    vkDestroyPipeline(logicalDevice, rasterizationPipelineInfo.pipeline, nullptr);
    vkDestroyPipeline(logicalDevice, postprocessingPipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, rtPipelineInfo.pipelineLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, rasterizationPipelineInfo.pipelineLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, postprocessingPipeline.pipelineLayout, nullptr);

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
