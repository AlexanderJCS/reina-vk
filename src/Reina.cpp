#include "Reina.h"

#include "graphics/ObjectProperties.h"
#include "graphics/Camera.h"
#include "tools/Clock.h"

#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>

#include <stb_image_write.h>


void save(VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool cmdPool, reina::graphics::Image& img, reina::core::Buffer& stagingBuffer, uint32_t width, uint32_t height) {
    reina::core::CmdBuffer saveCmdBuffer{logicalDevice, cmdPool, false};

    img.transition(saveCmdBuffer.getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    img.copyToBuffer(saveCmdBuffer.getHandle(), stagingBuffer.getHandle());
    saveCmdBuffer.endWaitSubmit(logicalDevice, graphicsQueue);

    std::vector<uint8_t> pixels = stagingBuffer.copyData<uint8_t>(logicalDevice);

    std::string filename = "../output.png";
    int success = stbi_write_png(
            filename.c_str(),
            static_cast<int>(width),
            static_cast<int>(height),
            4,
            pixels.data(),
            static_cast<int>(width) * 4
    );

    if (!success) {
        saveCmdBuffer.destroy(logicalDevice);
        throw std::runtime_error("Could not save PNG");
    }

    saveCmdBuffer.begin();
    img.transition(saveCmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    saveCmdBuffer.endWaitSubmit(logicalDevice, graphicsQueue);

    saveCmdBuffer.destroy(logicalDevice);
}


Reina::Reina() {
    // init
    renderWidth = 1080;
    renderHeight = 1350;
    const float aspectRatio = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);

    windowWidth = 800;
    windowHeight = static_cast<int>(static_cast<float>(windowWidth) / aspectRatio);

    renderWindow = reina::window::Window {windowWidth, windowHeight};

    instance = vktools::createInstance();
    debugMessenger = vktools::createDebugMessenger(instance);
    surface = vktools::createSurface(instance, renderWindow.getGlfwWindow());
    VkPhysicalDevice physicalDevice = vktools::pickPhysicalDevice(instance, surface);
    logicalDevice = vktools::createLogicalDevice(surface, physicalDevice);

    vktools::QueueFamilyIndices indices = vktools::findQueueFamilies(surface, physicalDevice);
    vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);

    swapchainObjects = vktools::createSwapchain(surface, physicalDevice, logicalDevice, renderWindow.getWidth(), renderWindow.getHeight());
    swapchainImageViews = vktools::createSwapchainImageViews(logicalDevice, swapchainObjects.swapchainImageFormat, swapchainObjects.swapchainImages);

    rtImage = reina::graphics::Image{
            logicalDevice, physicalDevice, renderWidth, renderHeight, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    rtDescriptorSet = reina::core::DescriptorSet{
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
    camera = reina::graphics::Camera{renderWindow, glm::radians(15.0f), aspectRatio, pos, glm::normalize(lookAt - pos)};
    pushConstants = reina::core::PushConstants{PushConstantsStruct{camera.getInverseView(), camera.getInverseProjection(), 0, 100, 10}, VK_SHADER_STAGE_RAYGEN_BIT_KHR};

    sbtSpacing = vktools::calculateSbtSpacing(physicalDevice);
    shaders = {
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/lambertian.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/metal.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/dielectric.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
    };

    rtPipelineInfo = vktools::createRtPipeline(logicalDevice, rtDescriptorSet, shaders, pushConstants);
    sbtBuffer = vktools::createSbt(logicalDevice, physicalDevice, rtPipelineInfo.pipeline, sbtSpacing, shaders.size());

    for (reina::graphics::Shader& shader : shaders) {
        shader.destroy(logicalDevice);
    }

    postprocessingOutputImage = reina::graphics::Image{
            logicalDevice, physicalDevice, renderWidth, renderHeight, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    fragmentImageSampler = vktools::createSampler(logicalDevice);

    postprocessingDescriptorSet = reina::core::DescriptorSet{
            logicalDevice,
            {
                    reina::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},  // input image
                    reina::core::Binding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}   // output image
            }
    };
    postprocessingShader = reina::graphics::Shader(logicalDevice, "../shaders/postprocessing.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    postprocessingPipeline = vktools::createComputePipeline(logicalDevice, postprocessingDescriptorSet, postprocessingShader);

    rasterizationDescriptorSet = reina::core::DescriptorSet{
            logicalDevice,
            {
                    reina::core::Binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
            }
    };

    reina::graphics::Shader vertexShader = reina::graphics::Shader(logicalDevice, "../shaders/display.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    reina::graphics::Shader fragmentShader = reina::graphics::Shader(logicalDevice, "../shaders/display.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    renderPass = vktools::createRenderPass(logicalDevice, swapchainObjects.swapchainImageFormat);
    rasterizationPipelineInfo = vktools::createRasterizationPipeline(logicalDevice, rasterizationDescriptorSet, renderPass, vertexShader, fragmentShader);

    framebuffers = vktools::createSwapchainFramebuffers(logicalDevice, renderPass, swapchainObjects.swapchainExtent, swapchainImageViews);

    vertexShader.destroy(logicalDevice);
    fragmentShader.destroy(logicalDevice);

    syncObjects = vktools::createSyncObjects(logicalDevice);

    commandPool = vktools::createCommandPool(physicalDevice, logicalDevice, surface);

    commandBuffer = reina::core::CmdBuffer{logicalDevice, commandPool, false, true};
    commandBuffer.endWaitSubmit(logicalDevice, graphicsQueue);  // since the command buffer automatically begins upon creation, and we don't want that in this specific case

    models = reina::graphics::Models{logicalDevice, physicalDevice, {"../models/ico_sphere_highres.obj", "../models/empty_cornell_box.obj", "../models/cornell_light.obj"}};
    box = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(1), true};
    light = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(2), true};
    subject = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(0), true};

    glm::mat4x4 baseTransform = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    glm::mat4x4 subjectTransform = glm::scale(glm::translate(baseTransform, glm::vec3(0.0f, 1.0f, 0)), glm::vec3(0.4f));

    instances = reina::graphics::Instances{
            logicalDevice, physicalDevice,
            {
                    {box, false, models.getModelRange(1), models.getObjData(1), 0, 0, baseTransform},
                    {light, true, models.getModelRange(2), models.getObjData(2), 1, 0, baseTransform},
                    {subject, false, models.getModelRange(0), models.getObjData(0), 2, 2, subjectTransform}
            },
    };

    tlas = vktools::createTlas(logicalDevice, physicalDevice, commandPool, graphicsQueue, instances.getInstances());

    std::vector<reina::graphics::ObjectProperties> objectProperties{
            {models.getModelRange(1).indexOffset, glm::vec3{0.9}, glm::vec4(0), models.getModelRange(1).normalsIndexOffset, 0.01, false, 0},
            {models.getModelRange(2).indexOffset, glm::vec3{0.9}, glm::vec4(3.5), models.getModelRange(2).normalsIndexOffset, 0, false, 0},
            {models.getModelRange(0).indexOffset, glm::vec3(1), glm::vec4(0), models.getModelRange(0).normalsIndexOffset, 1.4f, true, 0.7}
    };

    objectPropertiesBuffer = reina::core::Buffer{
            logicalDevice, physicalDevice, objectProperties,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            static_cast<VkMemoryAllocateFlagBits>(0),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
    };

    VkDeviceSize imageSize = renderWidth * renderHeight * 4;  // RGBA8

    stagingBuffer = reina::core::Buffer{
            logicalDevice, physicalDevice, imageSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            static_cast<VkMemoryAllocateFlags>(0),
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
}


void Reina::destroy() {
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

void Reina::renderLoop() {
    VkCommandBuffer cmdBufferHandle = commandBuffer.getHandle();

    rtDescriptorSet.writeBinding(logicalDevice, 0, rtImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);
    rtDescriptorSet.writeBinding(logicalDevice, 1, tlas);
    rtDescriptorSet.writeBinding(logicalDevice, 2, models.getVerticesBuffer());
    rtDescriptorSet.writeBinding(logicalDevice, 3, models.getOffsetIndicesBuffer());
    rtDescriptorSet.writeBinding(logicalDevice, 4, objectPropertiesBuffer);
    rtDescriptorSet.writeBinding(logicalDevice, 5, models.getNormalsBuffer());
    rtDescriptorSet.writeBinding(logicalDevice, 6, models.getOffsetNormalsIndicesBuffer());
    rtDescriptorSet.writeBinding(logicalDevice, 7, instances.getEmissiveMetadataBuffer());
    rtDescriptorSet.writeBinding(logicalDevice, 8, instances.getCdfTrianglesBuffer());
    rtDescriptorSet.writeBinding(logicalDevice, 9, instances.getCdfInstancesBuffer());

    postprocessingDescriptorSet.writeBinding(logicalDevice, 0, rtImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);
    postprocessingDescriptorSet.writeBinding(logicalDevice, 1, postprocessingOutputImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);

    rasterizationDescriptorSet.writeBinding(logicalDevice, 0, postprocessingOutputImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fragmentImageSampler);

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
//            std::cout << clock.summary() << "\n";
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
        if (samples == 1024) {
//        if (false) {
            save(logicalDevice, graphicsQueue, commandPool, postprocessingOutputImage, stagingBuffer, renderWidth, renderHeight);
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
}
