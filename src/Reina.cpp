#include "Reina.h"

#include "graphics/Camera.h"
#include "tools/Clock.h"
#include "../polyglot/raytrace.h"

#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <toml.hpp>


void save(const std::string& filename, VkDevice logicalDevice, VkQueue graphicsQueue, VkCommandPool cmdPool, reina::graphics::Image& img, reina::core::Buffer& stagingBuffer, uint32_t width, uint32_t height) {
    reina::core::CmdBuffer saveCmdBuffer{logicalDevice, cmdPool, false};

    img.transition(saveCmdBuffer.getHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    img.copyToBuffer(saveCmdBuffer.getHandle(), stagingBuffer.getHandle());
    saveCmdBuffer.endWaitSubmit(logicalDevice, graphicsQueue);

    std::vector<uint8_t> pixels = stagingBuffer.copyData<uint8_t>(logicalDevice);

    int success = stbi_write_png(
            ("../" + filename).c_str(),
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
    auto config = toml::parse_file("../config/config.toml");

    // init
    renderWidth = 1080;  // todo: bug - when renderWidth < windowWidth, the image appears stretched
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

    commandPool = vktools::createCommandPool(physicalDevice, logicalDevice, surface);

    cmdBuffer = reina::core::CmdBuffer{logicalDevice, commandPool, false, true};
    cmdBuffer.endWaitSubmit(logicalDevice, graphicsQueue);  // since the command buffer automatically begins upon creation, and we don't want that in this specific case

    textures = std::vector<reina::graphics::Image>{
        reina::graphics::Image{logicalDevice, physicalDevice, commandPool, graphicsQueue, "../textures/qgdpH_2K_Albedo.jpg", true},
        reina::graphics::Image{logicalDevice, physicalDevice, commandPool, graphicsQueue, "../textures/qgdpH_2K_Normal.jpg", false},
        reina::graphics::Image{logicalDevice, physicalDevice, commandPool, graphicsQueue, "../textures/Hammered_Metal_Albedo.png", true},
        reina::graphics::Image{logicalDevice, physicalDevice, commandPool, graphicsQueue, "../textures/Hammered_Metal_normal.png", false},
        reina::graphics::Image{logicalDevice, physicalDevice, commandPool, graphicsQueue, "../textures/Soil_2K_Albedo.png", false},
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
                    reina::core::Binding{9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                    reina::core::Binding{10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                    reina::core::Binding{11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                    reina::core::Binding{12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
                    reina::core::Binding{13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(textures.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
            }
    };

    glm::vec3 pos = glm::vec3(0, 1.3f, 8.4f);
    glm::vec3 lookAt = glm::vec3(0, 0.962f, 0);
    camera = reina::graphics::Camera{renderWindow, glm::radians(15.0f), aspectRatio, pos, glm::normalize(lookAt - pos)};

    originalSamplesPerPixel = config.at_path("sampling.samples_per_pixel").value<uint32_t>().value();

    RtPushConsts defaultPushConstants = {
            .invView = camera.getInverseView(),
            .invProjection = camera.getInverseProjection(),
            .sampleBatch = 0,
            .totalEmissiveArea = 0,
            .focusDist = config.at_path("camera.dof.focus_dist").value<float>().value(),
            .defocusMultiplier = config.at_path("camera.dof.defocus_multiplier").value<float>().value() / 100,  // divide by 100 to provide human-scale values in config file
            .directClamp = config.at_path("sampling.direct_clamp").value<float>().value(),
            .indirectClamp = config.at_path("sampling.indirect_clamp").value<float>().value(),
            .samplesPerPixel = config.at_path("sampling.samples_per_pixel").value<uint32_t>().value(),
            .maxBounces = config.at_path("sampling.max_bounces").value<uint32_t>().value()
    };
    rtPushConsts = reina::core::PushConstants{defaultPushConstants, VK_SHADER_STAGE_RAYGEN_BIT_KHR};

    sbtSpacing = vktools::calculateSbtSpacing(physicalDevice);
    shaders = {
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace/raytrace.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace/raytrace.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace/shadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace/lambertian.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace/metal.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR),
            reina::graphics::Shader(logicalDevice, "../shaders/raytrace/dielectric.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
    };

    rtPipeline = vktools::createRtPipeline(logicalDevice, rtDescriptorSet, shaders, rtPushConsts);
    sbtBuffer = vktools::createSbt(logicalDevice, physicalDevice, rtPipeline.pipeline, sbtSpacing, shaders.size());

    for (reina::graphics::Shader& shader : shaders) {
        shader.destroy(logicalDevice);
    }

    tonemapOutputImage = reina::graphics::Image{
            logicalDevice, physicalDevice, renderWidth, renderHeight, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    fragmentImageSampler = vktools::createSampler(logicalDevice);

    tonemapPushConsts = reina::core::PushConstants{
        TonemappingPushConsts{config.at_path("postprocessing.tonemap.exposure").value<float>().value()},
        VK_SHADER_STAGE_COMPUTE_BIT
    };

    tonemapDescriptorSet = reina::core::DescriptorSet{
            logicalDevice,
            {
                    reina::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},  // input image
                    reina::core::Binding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}   // output image
            }
    };
    tonemapShader = reina::graphics::Shader(logicalDevice, "../shaders/postprocessing/tonemap/tonemapping.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    tonemapPipeline = vktools::createComputePipeline(logicalDevice, tonemapDescriptorSet, tonemapShader, tonemapPushConsts);

    rasterDescriptorSet = reina::core::DescriptorSet{
            logicalDevice,
            {
                    reina::core::Binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
            }
    };

    reina::graphics::Shader vertexShader = reina::graphics::Shader(logicalDevice, "../shaders/raster/display.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    reina::graphics::Shader fragmentShader = reina::graphics::Shader(logicalDevice, "../shaders/raster/display.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    renderPass = vktools::createRenderPass(logicalDevice, swapchainObjects.swapchainImageFormat);
    rasterPipeline = vktools::createRasterizationPipeline(logicalDevice, rasterDescriptorSet, renderPass, vertexShader, fragmentShader);

    framebuffers = vktools::createSwapchainFramebuffers(logicalDevice, renderPass, swapchainObjects.swapchainExtent, swapchainImageViews);

    vertexShader.destroy(logicalDevice);
    fragmentShader.destroy(logicalDevice);

    syncObjects = vktools::createSyncObjects(logicalDevice);

    models = reina::graphics::Models{logicalDevice, physicalDevice, {"../models/plant_leaves_1.obj", "../models/empty_cornell_box.obj", "../models/cornell_light.obj", "../models/plant_leaves_2.obj", "../models/plant_pot.obj", "../models/plant_soil.obj"}};
    box = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(1), true};
    light = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(2), true};
    leaves_1 = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(0), true};
    leaves_2 = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(3), true};
    pot = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(4), true};
    soil = reina::graphics::Blas{logicalDevice, physicalDevice, commandPool, graphicsQueue, models, models.getModelRange(5), true};

    glm::mat4x4 baseTransform = glm::translate(glm::mat4x4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
    glm::mat4x4 subjectTransform = glm::scale(glm::translate(baseTransform, glm::vec3(0.0f, 0.0f, 0)), glm::vec3(3.0f));

    std::vector<ObjectProperties> objectProperties{
            {models.getModelRange(1).indexOffset, glm::vec3{0.9}, glm::vec3(0), models.getModelRange(1).normalsIndexOffset, models.getModelRange(1).texIndexOffset, 0.01, false, 0, -1, -1, true},
            {models.getModelRange(2).indexOffset, glm::vec3{0.9}, glm::vec3(16), models.getModelRange(2).normalsIndexOffset, models.getModelRange(2).texIndexOffset, 0, false, 0, -1, -1, true},
            {models.getModelRange(0).indexOffset, glm::vec3(1), glm::vec3(0), models.getModelRange(0).normalsIndexOffset, models.getModelRange(0).texIndexOffset, 1.4f, true, 0.7, 0, 1, true},
            {models.getModelRange(3).indexOffset, glm::vec3(1), glm::vec3(0), models.getModelRange(3).normalsIndexOffset, models.getModelRange(3).texIndexOffset, 1.4f, true, 0.7, 0, 1, true},
            {models.getModelRange(4).indexOffset, glm::vec3(1), glm::vec3(0), models.getModelRange(4).normalsIndexOffset, models.getModelRange(4).texIndexOffset, 0.2f, false, 0, 2, 3, true},
            {models.getModelRange(5).indexOffset, glm::vec3(1), glm::vec3(0), models.getModelRange(5).normalsIndexOffset, models.getModelRange(5).texIndexOffset, 0.2f, false, 0, 4, -1, true},
    };

    instances = reina::graphics::Instances{
            logicalDevice, physicalDevice,
            {
                    {box, objectProperties[0].emission, models.getModelRange(1), models.getObjData(1), 0, 0, baseTransform},
                    {light, objectProperties[1].emission, models.getModelRange(2), models.getObjData(2), 1, 0, baseTransform},
                    {leaves_1, objectProperties[2].emission, models.getModelRange(0), models.getObjData(0), 2, 0, subjectTransform},
                    {leaves_2, objectProperties[3].emission, models.getModelRange(3), models.getObjData(3), 3, 0, subjectTransform},
                    {pot, objectProperties[4].emission, models.getModelRange(4), models.getObjData(4), 4, 0, subjectTransform},
                    {soil, objectProperties[5].emission, models.getModelRange(5), models.getObjData(5), 5, 0, subjectTransform}
            },
    };
    rtPushConsts.getPushConstants().totalEmissiveArea = instances.getEmissiveInstancesArea();

    tlas = vktools::createTlas(logicalDevice, physicalDevice, commandPool, graphicsQueue, instances.getInstances());

    objectPropertiesBuffer = reina::core::Buffer{
            logicalDevice, physicalDevice, objectProperties,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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

    pingImage = reina::graphics::Image{
            logicalDevice, physicalDevice, renderWidth, renderHeight, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    pongImage = reina::graphics::Image{
            logicalDevice, physicalDevice, renderWidth, renderHeight, VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    bloomPushConsts = reina::core::PushConstants{
        BloomPushConsts{
            config.at_path("postprocessing.bloom.radius").value<float>().value(),
            config.at_path("postprocessing.bloom.threshold").value<float>().value(),
            config.at_path("postprocessing.bloom.intensity").value<float>().value(),
        },
        VK_SHADER_STAGE_COMPUTE_BIT
    };

    blurXDescriptorSet = reina::core::DescriptorSet{
        logicalDevice, {
            reina::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
            reina::core::Binding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}
        }
    };

    blurXShader = reina::graphics::Shader(
            logicalDevice, "../shaders/postprocessing/bloom/blurX.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT
            );

    blurXPipeline = vktools::createComputePipeline(logicalDevice, blurXDescriptorSet, blurXShader, bloomPushConsts);

    blurYDescriptorSet = reina::core::DescriptorSet{
            logicalDevice, {
                    reina::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                    reina::core::Binding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}
            }
    };

    blurYShader = reina::graphics::Shader(
            logicalDevice, "../shaders/postprocessing/bloom/blurY.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT
    );

    blurYPipeline = vktools::createComputePipeline(logicalDevice, blurYDescriptorSet, blurYShader, bloomPushConsts);

    combineShader = reina::graphics::Shader(
            logicalDevice, "../shaders/postprocessing/bloom/combine.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT
            );

    combineDescriptorSet = reina::core::DescriptorSet{
        logicalDevice, {
                    reina::core::Binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                    reina::core::Binding{1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                    reina::core::Binding{2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
        }
    };

    combinePipeline = vktools::createComputePipeline(logicalDevice, combineDescriptorSet, combineShader, bloomPushConsts);

    saveManager = reina::tools::SaveManager{config};

    writeDescriptorSets();
}


void Reina::renderLoop() {
    VkCommandBuffer cmdBufferHandle = cmdBuffer.getHandle();

    reina::tools::Clock clock{rtPushConsts.getPushConstants().samplesPerPixel};
    while (!renderWindow.shouldClose()) {
        // camera
        camera.processInput(renderWindow, clock.getTimeDelta());
        if (camera.hasChanged()) {
            camera.refresh();
            RtPushConsts& pushConstantsStruct = rtPushConsts.getPushConstants();
            pushConstantsStruct.invView = camera.getInverseView();
            pushConstantsStruct.invProjection = camera.getInverseProjection();
            pushConstantsStruct.sampleBatch = 0;  // reset the image
        }

        if (camera.isAcceptingInput()) {
            rtPushConsts.getPushConstants().samplesPerPixel = 1;
        } else {
            rtPushConsts.getPushConstants().samplesPerPixel = originalSamplesPerPixel;
        }

        // clock
        bool firstFrame = clock.getFrameCount() == 0;

        if (!firstFrame) {
            std::cout << clock.summary() << "\n";
        }

        clock.markCategory("Ray Tracing");

        // render ray traced image
        cmdBuffer.wait(logicalDevice);
        cmdBuffer.begin();

        // todo: these transitions should be done when initialized
        for (reina::graphics::Image& texture : textures) {
            texture.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        }

        traceRays();

        clock.markCategory("Bloom");
        applyBloom();

        clock.markCategory("Tone Mapping");
        applyTonemapping();

        // save
        clock.markCategory("Save");

        uint32_t samples = clock.getSampleCount();

        reina::tools::SaveInfo saveInfo = saveManager.shouldSave(samples, clock.getAge());
        if (saveInfo.shouldSave) {
            save(saveInfo.filename, logicalDevice, graphicsQueue, commandPool, tonemapOutputImage, stagingBuffer, renderWidth, renderHeight);
        }

        // render
        clock.markCategory("Display");

        tonemapOutputImage.transition(cmdBufferHandle, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        uint32_t imageIndex = -1;
        if (!renderWindow.isMinimized()) {
            draw(imageIndex);
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

        cmdBuffer.endSubmit(logicalDevice, graphicsQueue, submitInfo);

        // Present the swapchain image
        if (!renderWindow.isMinimized()) {
            present(imageIndex);
        }

        clock.markFrame();
        glfwPollEvents();
    }

    vkDeviceWaitIdle(logicalDevice);
}

void Reina::writeDescriptorSets() {
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
    rtDescriptorSet.writeBinding(logicalDevice, 10, models.getTexCoordsBuffer());
    rtDescriptorSet.writeBinding(logicalDevice, 11, models.getOffsetTexIndicesBuffer());
    rtDescriptorSet.writeBinding(logicalDevice, 13, textures, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, fragmentImageSampler);

    blurXDescriptorSet.writeBinding(logicalDevice, 0, rtImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);
    blurXDescriptorSet.writeBinding(logicalDevice, 1, pingImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);

    blurYDescriptorSet.writeBinding(logicalDevice, 0, pingImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);
    blurYDescriptorSet.writeBinding(logicalDevice, 1, pongImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);

    combineDescriptorSet.writeBinding(logicalDevice, 0, rtImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);
    combineDescriptorSet.writeBinding(logicalDevice, 1, pongImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);
    combineDescriptorSet.writeBinding(logicalDevice, 2, pingImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);

    tonemapDescriptorSet.writeBinding(logicalDevice, 0, pingImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);
    tonemapDescriptorSet.writeBinding(logicalDevice, 1, tonemapOutputImage, VK_IMAGE_LAYOUT_GENERAL, VK_NULL_HANDLE);

    rasterDescriptorSet.writeBinding(logicalDevice, 0, tonemapOutputImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, fragmentImageSampler);
}

void Reina::traceRays() {
    rtImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

    vkCmdBindPipeline(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline.pipeline);
    rtDescriptorSet.bind(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline.pipelineLayout);

    rtPushConsts.push(cmdBuffer.getHandle(), rtPipeline.pipelineLayout);
    rtPushConsts.getPushConstants().sampleBatch++;

    // todo: there's no reason for the shader stage regions to be in the loop. this should be defined in createRtPipeline
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
            cmdBuffer.getHandle(),
            &sbtRayGenRegion,
            &sbtMissRegion,
            &sbtHitRegion,
            &sbtCallableRegion,
            renderWidth,
            renderHeight,
            1
    );
}

void Reina::applyBloom() {
    const int workgroupWidth = 32;
    const int workgroupHeight = 8;

    rtImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    pingImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    blurXDescriptorSet.bind(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, blurXPipeline.pipelineLayout);
    bloomPushConsts.push(cmdBuffer.getHandle(), blurXPipeline.pipelineLayout);

    vkCmdBindPipeline(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, blurXPipeline.pipeline);
    vkCmdDispatch(
            cmdBuffer.getHandle(),
            (renderWidth + workgroupWidth - 1) / workgroupWidth,
            (renderHeight + workgroupHeight - 1) / workgroupHeight,
            1
    );

    pingImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    pongImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    blurYDescriptorSet.bind(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, blurYPipeline.pipelineLayout);
    bloomPushConsts.push(cmdBuffer.getHandle(), blurYPipeline.pipelineLayout);

    vkCmdBindPipeline(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, blurYPipeline.pipeline);
    vkCmdDispatch(
            cmdBuffer.getHandle(),
            (renderWidth + workgroupWidth - 1) / workgroupWidth,
            (renderHeight + workgroupHeight - 1) / workgroupHeight,
            1
    );

    pongImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    pingImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    combineDescriptorSet.bind(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, combinePipeline.pipelineLayout);
    bloomPushConsts.push(cmdBuffer.getHandle(), combinePipeline.pipelineLayout);

    vkCmdBindPipeline(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, combinePipeline.pipeline);
    vkCmdDispatch(
            cmdBuffer.getHandle(),
            (renderWidth + workgroupWidth - 1) / workgroupWidth,
            (renderHeight + workgroupHeight - 1) / workgroupHeight,
            1
    );

    vkCmdPipelineBarrier(
            cmdBuffer.getHandle(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            0, nullptr
    );
}

void Reina::applyTonemapping() {
    const int workgroupWidth = 32;
    const int workgroupHeight = 8;

    pingImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    tonemapOutputImage.transition(cmdBuffer.getHandle(), VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    tonemapDescriptorSet.bind(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline.pipelineLayout);
    tonemapPushConsts.push(cmdBuffer.getHandle(), tonemapPipeline.pipelineLayout);

    vkCmdBindPipeline(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline.pipeline);
    vkCmdDispatch(
            cmdBuffer.getHandle(),
            (renderWidth + workgroupWidth - 1) / workgroupWidth,
            (renderHeight + workgroupHeight - 1) / workgroupHeight,
            1
    );
}

void Reina::draw(uint32_t& imageIndex) {
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

    vkCmdBeginRenderPass(cmdBuffer.getHandle(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    rasterDescriptorSet.bind(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, rasterPipeline.pipelineLayout);

    vkCmdBindPipeline(cmdBuffer.getHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, rasterPipeline.pipeline);

    VkViewport viewport{
            .x = 0,
            .y = 0,
            .width = static_cast<float>(swapchainObjects.swapchainExtent.width),
            .height = static_cast<float>(swapchainObjects.swapchainExtent.height),
            .minDepth = 0,
            .maxDepth = 1
    };
    vkCmdSetViewport(cmdBuffer.getHandle(), 0, 1, &viewport);

    VkRect2D scissor{
            .offset = {0, 0},
            .extent = swapchainObjects.swapchainExtent
    };

    vkCmdSetScissor(cmdBuffer.getHandle(), 0, 1, &scissor);
    vkCmdDraw(cmdBuffer.getHandle(), 6, 1, 0, 0);
    vkCmdEndRenderPass(cmdBuffer.getHandle());
}

void Reina::present(uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &syncObjects.renderFinishedSemaphore;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchainObjects.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);
}

Reina::~Reina() {
    auto vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkDestroyAccelerationStructureKHR"));

    for (VkFramebuffer framebuffer : framebuffers) {
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }

    for (reina::graphics::Image& texture : textures) {
        texture.destroy(logicalDevice);
    }

    pingImage.destroy(logicalDevice);
    blurXShader.destroy(logicalDevice);
    blurXDescriptorSet.destroy(logicalDevice);
    pongImage.destroy(logicalDevice);
    blurYShader.destroy(logicalDevice);
    blurYDescriptorSet.destroy(logicalDevice);
    combineShader.destroy(logicalDevice);
    combineDescriptorSet.destroy(logicalDevice);
    light.destroy(logicalDevice);
    box.destroy(logicalDevice);
    leaves_1.destroy(logicalDevice);
    leaves_2.destroy(logicalDevice);
    pot.destroy(logicalDevice);
    soil.destroy(logicalDevice);
    tlas.buffer.destroy(logicalDevice);
    sbtBuffer.destroy(logicalDevice);
    objectPropertiesBuffer.destroy(logicalDevice);
    tonemapOutputImage.destroy(logicalDevice);
    rtImage.destroy(logicalDevice);
    instances.destroy(logicalDevice);

    stagingBuffer.destroy(logicalDevice);

    vkDestroySampler(logicalDevice, fragmentImageSampler, nullptr);
    cmdBuffer.destroy(logicalDevice);
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
    vkDestroyAccelerationStructureKHR(logicalDevice, tlas.accelerationStructure, nullptr);
    rtDescriptorSet.destroy(logicalDevice);
    tonemapDescriptorSet.destroy(logicalDevice);
    tonemapShader.destroy(logicalDevice);
    rasterDescriptorSet.destroy(logicalDevice);
    vkDestroySemaphore(logicalDevice, syncObjects.renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(logicalDevice, syncObjects.imageAvailableSemaphore, nullptr);
    models.destroy(logicalDevice);
    vkDestroyPipeline(logicalDevice, rtPipeline.pipeline, nullptr);
    vkDestroyPipeline(logicalDevice, rasterPipeline.pipeline, nullptr);
    vkDestroyPipeline(logicalDevice, tonemapPipeline.pipeline, nullptr);
    vkDestroyPipeline(logicalDevice, blurXPipeline.pipeline, nullptr);
    vkDestroyPipeline(logicalDevice, blurYPipeline.pipeline, nullptr);
    vkDestroyPipeline(logicalDevice, combinePipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, rtPipeline.pipelineLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, rasterPipeline.pipelineLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, tonemapPipeline.pipelineLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, blurXPipeline.pipelineLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, blurYPipeline.pipelineLayout, nullptr);
    vkDestroyPipelineLayout(logicalDevice, combinePipeline.pipelineLayout, nullptr);

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
