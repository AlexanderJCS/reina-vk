#include "vktools.h"

#include <vector>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <set>
#include <algorithm>
#include <limits>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "GLFW/glfw3.h"

#include "consts.h"
#include "../core/DescriptorSet.h"
#include "../graphics/Blas.h"
#include "../graphics/Instance.h"
#include "../core/CmdBuffer.h"

uint32_t vktools::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

bool vktools::QueueFamilyIndices::isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value();
}

bool vktools::hasValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto& layerName : consts::VALIDATION_LAYERS) {
        bool found = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // validation layer extension
    if (consts::ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vktools::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
) {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    }

    return VK_FALSE;
}

void vktools::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback
    };
}

VkResult vktools::createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

vktools::QueueFamilyIndices vktools::findQueueFamilies(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // graphics family
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // present support
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        // early exit
        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

vktools::SwapChainSupportDetails vktools::querySwapChainSupport(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

uint64_t vktools::getDeviceLocalMemory(VkPhysicalDevice device) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

    uint64_t deviceLocalMemorySize = 0;

    for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i) {
        // Check if the heap is device-local (VRAM)
        if (memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            deviceLocalMemorySize += memoryProperties.memoryHeaps[i].size;
        }
    }

    return deviceLocalMemorySize;
}

bool vktools::isDeviceSuitable(VkPhysicalDevice device) {
    // Check if all required extensions are supported
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensionsSet(consts::DEVICE_EXTENSIONS.begin(), consts::DEVICE_EXTENSIONS.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensionsSet.erase(extension.extensionName);
    }

    if (!requiredExtensionsSet.empty()) {
        return false;  // Missing required ray tracing extensions
    }

    // 2. Check Vulkan API version compatibility
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    if (deviceProperties.apiVersion < VK_API_VERSION_1_3) {
        return false;  // This application requires RT version 1.3 or later
    }

    // 3. Check for required ray tracing features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructureFeatures{};
    accelStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    rayTracingPipelineFeatures.pNext = &accelStructureFeatures;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &rayTracingPipelineFeatures;

    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);

    if (!rayTracingPipelineFeatures.rayTracingPipeline || !accelStructureFeatures.accelerationStructure) {
        return false;  // Device does not support ray tracing
    }

    // Additional checks like memory limits, queue families, etc., can go here

    return true;  // Device satisfies all requirements
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    throw std::runtime_error("VK_FORMAT_R8G8B8A8_UNORM is not available");
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        // since we want to render as fast as possible and don't really care about tearing, we can use VK_PRESENT_MODE_IMMEDIATE
        if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return availablePresentMode;
        }
    }

    // this is the only one guaranteed to be available, so use this as a fallback
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int windowWidth, int windowHeight) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = {
            static_cast<uint32_t>(windowWidth),
            static_cast<uint32_t>(windowHeight)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

vktools::PipelineInfo vktools::createComputePipeline(VkDevice logicalDevice, const ::reina::core::DescriptorSet& descriptorSet, const reina::graphics::Shader& shader) {
    VkDescriptorSetLayout descriptorLayout = descriptorSet.getLayout();
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = VK_NULL_HANDLE
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkComputePipelineCreateInfo pipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shader.pipelineShaderStageCreateInfo(),
        .layout = pipelineLayout
    };

    VkPipeline computePipeline;
    if (vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline");
    }

    return {computePipeline, pipelineLayout};
}

std::vector<VkFramebuffer> vktools::createSwapchainFramebuffers(VkDevice logicalDevice, VkRenderPass renderPass, VkExtent2D extent, const std::vector<VkImageView>& swapchainImageViews) {
    std::vector<VkFramebuffer> swapchainFramebuffers(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkFramebufferCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = &swapchainImageViews[i],
            .width = extent.width,
            .height = extent.height,
            .layers = 1
        };

        if (vkCreateFramebuffer(logicalDevice, &createInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    return swapchainFramebuffers;
}

vktools::PipelineInfo vktools::createRasterizationPipeline(VkDevice logicalDevice, const reina::core::DescriptorSet &descriptorSet, VkRenderPass renderPass, const reina::graphics::Shader &vertexShader, const reina::graphics::Shader &fragmentShader) {
    VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertexShader.pipelineShaderStageCreateInfo(),
            fragmentShader.pipelineShaderStageCreateInfo()
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkDescriptorSetLayout descriptorSetLayout = descriptorSet.getLayout();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline rasterizationPipeline;
    if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rasterizationPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    return {rasterizationPipeline, pipelineLayout};
}

VkRenderPass vktools::createRenderPass(VkDevice logicalDevice, VkFormat swapchainImageFormat) {
    VkAttachmentDescription colorAttachment{
        .format = swapchainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    VkRenderPass renderPass;
    if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    return renderPass;
}

vktools::AccStructureInfo vktools::createTlas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice,
                                              VkCommandPool cmdPool, VkQueue queue,
                                              const std::vector<reina::graphics::Instance>& instances) {

    std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
    for (const auto& instance : instances) {
        glm::mat4x4 tmp = glm::transpose(instance.getTransform());
        VkTransformMatrixKHR vkTransform;
        memcpy(&vkTransform, &tmp, sizeof(VkTransformMatrixKHR));

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = instance.getBlas().getHandle()
        };

        auto vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                vkGetDeviceProcAddr(logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR"));
        VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &addressInfo);

        VkAccelerationStructureInstanceKHR vkInstance{
                .transform = vkTransform,
                .instanceCustomIndex = instance.getObjectPropertiesID(),
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = instance.getMaterialOffset(),
                .flags = 0,
                .accelerationStructureReference = blasAddress
        };
        vkInstances.push_back(vkInstance);
    }

    // Create the instance buffer
    reina::core::Buffer instanceBuffer{
        logicalDevice, physicalDevice,
        vkInstances,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
    };

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE, // Contiguous array (not pointers)
            .data = {.deviceAddress = instanceBuffer.getDeviceAddress(logicalDevice)}
    };

    VkAccelerationStructureGeometryKHR geometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {.instances = instancesData},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    // Query build sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .geometryCount = 1,
            .pGeometries = &geometry
    };

    // Query build sizes
    VkAccelerationStructureBuildSizesInfoKHR buildSizes{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    auto vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkGetAccelerationStructureBuildSizesKHR"));
    auto instanceCount = static_cast<uint32_t>(vkInstances.size());
    vkGetAccelerationStructureBuildSizesKHR(
            logicalDevice,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &instanceCount,
            &buildSizes
    );

    // Create TLAS buffer
    reina::core::Buffer tlasBuffer{
            logicalDevice, physicalDevice,
            buildSizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    // Create acceleration structure
    VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = tlasBuffer.getHandle(),
            .size = buildSizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    auto vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkCreateAccelerationStructureKHR"));

    VkAccelerationStructureKHR tlas;
    vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr, &tlas);

    // Build TLAS
    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
            .primitiveCount = instanceCount,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;

    // Allocate scratch buffer (similar to BLAS)
    reina::core::Buffer scratchBuffer{
        logicalDevice, physicalDevice, buildSizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    buildInfo.dstAccelerationStructure = tlas;
    buildInfo.scratchData.deviceAddress = scratchBuffer.getDeviceAddress(logicalDevice);

    // Submit the build command
    VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    reina::core::CmdBuffer cmdBuffer{logicalDevice, cmdPool, true};

    auto vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkCmdBuildAccelerationStructuresKHR"));
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer.getHandle(), 1, &buildInfo, &pBuildRangeInfo);

    cmdBuffer.endWaitSubmit(logicalDevice, queue);

    cmdBuffer.destroy(logicalDevice);
    scratchBuffer.destroy(logicalDevice);
    instanceBuffer.destroy(logicalDevice);

    return {tlas, tlasBuffer};
}

vktools::SyncObjects vktools::createSyncObjects(VkDevice logicalDevice) {
    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vktools::SyncObjects syncObjects{};

    if (
        vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &syncObjects.imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &syncObjects.renderFinishedSemaphore) != VK_SUCCESS
    ) {
        throw std::runtime_error("Failed to create sync objects");
    }

    return syncObjects;
}

vktools::SbtSpacing vktools::calculateSbtSpacing(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceProperties2 physicalDeviceProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };

    vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties);

    VkDeviceSize sbtHeaderSize = rtProps.shaderGroupHandleSize;
    VkDeviceSize sbtBaseAlignment = rtProps.shaderGroupBaseAlignment;
    VkDeviceSize sbtHandleAlignment = rtProps.shaderGroupHandleAlignment;

    if (sbtBaseAlignment % sbtHandleAlignment != 0) {
        throw std::runtime_error("SBT base alignment is not a multiple of SBT cmdBuffer alignment");
    }

    VkDeviceSize sbtStride = (rtProps.shaderGroupHandleSize + rtProps.shaderGroupBaseAlignment - 1) & ~(rtProps.shaderGroupBaseAlignment - 1);

    // Validate
    if (sbtStride > rtProps.maxShaderGroupStride) {
        throw std::runtime_error("Stride exceeds device limit");
    }

    if (sbtStride % rtProps.shaderGroupBaseAlignment != 0) {
        throw std::runtime_error("SBT stride % shader group base alignment != 0");
    }

    return {sbtHeaderSize, sbtBaseAlignment, sbtHandleAlignment, sbtStride};
}

reina::core::Buffer vktools::createSbt(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkPipeline rtPipeline, SbtSpacing sbtSpacing, uint32_t shaderGroups) {
    std::vector<uint8_t> cpuShaderHandleStorage(sbtSpacing.headerSize * shaderGroups);

    auto vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR"));

    if (vkGetRayTracingShaderGroupHandlesKHR(logicalDevice, rtPipeline, 0, shaderGroups, cpuShaderHandleStorage.size(), cpuShaderHandleStorage.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not get RT shader group handles");
    }

    auto sbtSize = static_cast<VkDeviceSize>(sbtSpacing.stride * shaderGroups);

    reina::core::Buffer sbtBuffer{
            logicalDevice, physicalDevice, sbtSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    void* sbtMappedMemory;
    vkMapMemory(logicalDevice, sbtBuffer.getDeviceMemory(), 0, sbtSize, 0, &sbtMappedMemory);

    auto* sbtPtr = static_cast<uint8_t*>(sbtMappedMemory);
    for (uint32_t groupIdx = 0; groupIdx < shaderGroups; groupIdx++) {
        memcpy(&sbtPtr[groupIdx * sbtSpacing.stride], &cpuShaderHandleStorage[groupIdx * sbtSpacing.headerSize], sbtSpacing.headerSize);
    }

    vkUnmapMemory(logicalDevice, sbtBuffer.getDeviceMemory());

    return sbtBuffer;
}

vktools::PipelineInfo vktools::createRtPipeline(VkDevice logicalDevice, const reina::core::DescriptorSet& descriptorSet, const std::vector<reina::graphics::Shader>& shaders, const reina::core::PushConstants<RtPushConsts>& pushConstants) {
    if (shaders.size() < 2) {
        throw std::runtime_error("Must have minimally two shaders: raygen (index 0) and ray miss (index 1). Any following shaders are hit shaders");
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages(shaders.size());
    for (int i = 0; i < shaders.size(); i++) {
        stages[i] = shaders[i].pipelineShaderStageCreateInfo();
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups(shaders.size());
    groups[0] = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,  // index of ray gen
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    groups[1] = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1,  // index of ray miss
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
    };

    groups[2] = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 2,  // index of ray miss
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
    };

    for (int groupIdx = 3; groupIdx < shaders.size(); groupIdx++) {
        groups[groupIdx] = {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                .generalShader = VK_SHADER_UNUSED_KHR,
                .closestHitShader = static_cast<uint32_t>(groupIdx),
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR
        };
    }

    VkDescriptorSetLayout descriptorLayout = descriptorSet.getLayout();
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstants.getRange()
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Cannot create pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR rtPipelineCreateInfo {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = pipelineLayout
    };

    auto vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkCreateRayTracingPipelinesKHR"));

    if (!vkCreateRayTracingPipelinesKHR) {
        throw std::runtime_error("Failed to load vkCreateRayTracingPipelinesKHR");
    }

    VkPipeline rtPipeline;
    if (vkCreateRayTracingPipelinesKHR(logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rtPipelineCreateInfo, nullptr, &rtPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Cannot create RT compute pipeline");
    }

    return {rtPipeline, pipelineLayout};
}

VkSampler vktools::createSampler(VkDevice logicalDevice) {
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = 16.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkSampler sampler;
    if (vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }

    return sampler;
}

VkCommandPool vktools::createCommandPool(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface) {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(surface, physicalDevice);

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
    };

    VkCommandPool commandPool;
    if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    return commandPool;
}

std::vector<VkImageView> vktools::createSwapchainImageViews(VkDevice logicalDevice, VkFormat swapchainImageFormat, std::vector<VkImage> swapchainImages) {
    std::vector<VkImageView> swapchainImageViews;
    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapchainImageFormat,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
                },
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
        };


        if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views");
        }
    }

    return swapchainImageViews;
}

vktools::SwapchainObjects vktools::createSwapchain(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice logicalDevice, int windowWidth, int windowHeight) {
    vktools::SwapChainSupportDetails swapChainSupport = vktools::querySwapChainSupport(surface, physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, windowWidth, windowHeight);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // 0 is a special value that means no max image count
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    QueueFamilyIndices indices = findQueueFamilies(surface, physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = swapChainSupport.capabilities.currentTransform,  // this parameter might be interesting to google more about
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    vktools::SwapchainObjects swapchainObjects;
    if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapchainObjects.swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create the swap chain");
    }

    vkGetSwapchainImagesKHR(logicalDevice, swapchainObjects.swapchain, &imageCount, nullptr);
    swapchainObjects.swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice, swapchainObjects.swapchain, &imageCount, swapchainObjects.swapchainImages.data());

    swapchainObjects.swapchainImageFormat = surfaceFormat.format;
    swapchainObjects.swapchainExtent = extent;

    return swapchainObjects;
}

VkDevice vktools::createLogicalDevice(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice) {
    QueueFamilyIndices indices = vktools::findQueueFamilies(surface, physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Query ray tracing pipeline features
//    VkPhysicalDeviceRayTracingValidationFeaturesNV validationFeatures = {
//        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV
//    };

    VkPhysicalDeviceVulkan12Features vulkan12Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .runtimeDescriptorArray = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
//        .pNext = &validationFeatures
    };

    // Commented out since the VulkanPhysicalDeviceVulkan12Features covers this
//    VkPhysicalDeviceBufferDeviceAddressFeatures addressFeatures{
//        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
//        .pNext = &vulkan12Features
//    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &vulkan12Features
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &rtPipelineFeatures
    };

    VkPhysicalDeviceFeatures2 deviceFeatures2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &asFeatures
    };

    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

    // Enable required ray tracing features
    if (!rtPipelineFeatures.rayTracingPipeline) {
        throw std::runtime_error("Ray tracing pipeline feature is not supported by the physical device.");
    }

    if (!asFeatures.accelerationStructure) {
        throw std::runtime_error("Acceleration structure feature is not supported by the physical device.");
    }

//    if (!validationFeatures.rayTracingValidation) {
//        throw std::runtime_error("Ray tracing validation not supported");
//    }

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(consts::DEVICE_EXTENSIONS.size()),
        .ppEnabledExtensionNames = consts::DEVICE_EXTENSIONS.data(),
        .pEnabledFeatures = nullptr  // use the pNext thing instead
    };

    if (consts::ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(consts::VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = consts::VALIDATION_LAYERS.data();
    }

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create device");
    }

    return device;
}

VkPhysicalDevice vktools::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    VkPhysicalDevice highestScoreDevice = VK_NULL_HANDLE;
    uint32_t highestScore = 0;
    for (VkPhysicalDevice device : devices) {
        if (!isDeviceSuitable(device)) {
            continue;
        }

        // not the most efficient since I call vkGetPhysicalDeviceProperties here and also in isDeviceSuitable()
        //  but in the grand scheme of things it doesn't matter that much
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        // VRAM is an indicator of a GPU's strength
        uint32_t score = vktools::getDeviceLocalMemory(device);

        // favor discrete GPUs
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score *= 2;
        }

        if (score > highestScore) {
            highestScore = score;
            highestScoreDevice = device;
        }
    }

    if (deviceCount == 0 || highestScoreDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU");
    }

    return highestScoreDevice;
}


VkSurfaceKHR vktools::createSurface(VkInstance instance, GLFWwindow *window) {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    return surface;
}


void vktools::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

std::optional<VkDebugUtilsMessengerEXT> vktools::createDebugMessenger(VkInstance instance) {
    if (!consts::ENABLE_VALIDATION_LAYERS) {
        return std::nullopt;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    VkDebugUtilsMessengerEXT debugMessenger;
    if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to initialize debug messenger");
    }

    return debugMessenger;
}

VkInstance vktools::createInstance() {
    if (consts::ENABLE_VALIDATION_LAYERS && !hasValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested but not available");
    }

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Reina",
        .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 3, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> extensions = getRequiredExtensions();

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    // Structures to add into the pNext chain when validation layers are enabled.
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    VkValidationFeaturesEXT validationFeatures{};
    if (consts::ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(consts::VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = consts::VALIDATION_LAYERS.data();

        // Set up debug messenger create info.
        populateDebugMessengerCreateInfo(debugCreateInfo);

        // Set up validation features.
        std::array<VkValidationFeatureEnableEXT, 1> enabledFeatures = {
                VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
        };
        validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validationFeatures.enabledValidationFeatureCount = enabledFeatures.size();
        validationFeatures.pEnabledValidationFeatures = enabledFeatures.data();

        // Chain the structures.
        // Here we attach validationFeatures after the debug messenger.
        validationFeatures.pNext = debugCreateInfo.pNext;  // in case debugCreateInfo already chains something
        debugCreateInfo.pNext = &validationFeatures;
        // Now attach the debug messenger chain to the instance create info.
        createInfo.pNext = &debugCreateInfo;
    }

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance");
    }

    return instance;
}
