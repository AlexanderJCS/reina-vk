#include "vktools.h"

#include <vector>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <set>
#include <algorithm>
#include <limits>
#include <fstream>

#include <GLFW/glfw3.h>

#include "consts.h"
#include "DescriptorSet.h"

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

VkResult vktools::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
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
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    throw std::runtime_error("VK_FORMAT_R8G8B8A8_SRGB is not available");
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

vktools::SyncObjects vktools::createSyncObjects(VkDevice logicalDevice) {
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    vktools::SyncObjects syncObjects{};

    if (
        vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &syncObjects.imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &syncObjects.renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &syncObjects.inFlightFence) != VK_SUCCESS
    ) {
        throw std::runtime_error("Failed to create sync objects");
    }

    return syncObjects;
}

vktools::SbtSpacing vktools::calculateSbtSpacing(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceProperties2 physicalDeviceProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtPipelineProperties
    };

    vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties);

    VkDeviceSize sbtHeaderSize = rtPipelineProperties.shaderGroupHandleSize;
    VkDeviceSize sbtBaseAlignment = rtPipelineProperties.shaderGroupBaseAlignment;
    VkDeviceSize sbtHandleAlignment = rtPipelineProperties.shaderGroupHandleAlignment;

    if (sbtBaseAlignment % sbtHandleAlignment != 0) {
        throw std::runtime_error("SBT base alignment is not a multiple of SBT handle alignment");
    }

    VkDeviceSize sbtStride = sbtBaseAlignment * ((sbtHeaderSize + sbtBaseAlignment - 1) / sbtBaseAlignment);

    if (sbtStride > rtPipelineProperties.maxShaderGroupStride) {
        throw std::runtime_error("Calculated SBT stride is greater than the max shader group stride");
    }

    return {sbtHeaderSize, sbtBaseAlignment, sbtHandleAlignment, sbtStride};
}

vktools::SbtInfo vktools::createSbt(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkPipeline rtPipeline, SbtSpacing sbtSpacing) {
    std::vector<uint8_t> cpuShaderHandleStorage(sbtSpacing.headerSize * 1 /* shader group size - change this when adding more shader groups */);

    auto vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR"));

    if (vkGetRayTracingShaderGroupHandlesKHR(logicalDevice, rtPipeline, 0, /* shader group count: */ 1, cpuShaderHandleStorage.size(), cpuShaderHandleStorage.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not get RT shader group handles");
    }

    auto sbtSize = static_cast<uint32_t>(sbtSpacing.stride * 1 /* shader group count */);
    VkBufferCreateInfo sbtBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sbtSize,
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE  // exclusive if using one queue family
    };

    VkBuffer sbtBuffer;
    if (vkCreateBuffer(logicalDevice, &sbtBufferCreateInfo, nullptr, &sbtBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader binding table buffer");
    }

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, sbtBuffer, &memoryRequirements);

    VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,  // needed for buffers with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT when VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress is enabled
            .deviceMask = 0
    };

    VkMemoryAllocateInfo memoryAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &memoryAllocateFlagsInfo,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = vktools::findMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VkDeviceMemory sbtBufferMemory;
    if (vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr, &sbtBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate SBT buffer memory");
    }

    vkBindBufferMemory(logicalDevice, sbtBuffer, sbtBufferMemory, 0);

    void* sbtMappedMemory;
    vkMapMemory(logicalDevice, sbtBufferMemory, 0, sbtSize, 0, &sbtMappedMemory);

    auto* sbtPtr = static_cast<uint8_t*>(sbtMappedMemory);
    for (uint32_t groupIdx = 0; groupIdx < 1 /* num shader groups */; groupIdx++) {
        memcpy(&sbtPtr[groupIdx * sbtSpacing.stride], &cpuShaderHandleStorage[groupIdx * sbtSpacing.headerSize], sbtSpacing.headerSize);
    }

    vkUnmapMemory(logicalDevice, sbtBufferMemory);

    return {sbtBuffer, sbtBufferMemory};
}

vktools::PipelineInfo vktools::createRtPipeline(VkDevice logicalDevice, const DescriptorSet& descriptorSet, const std::vector<Shader>& shaders) {
    VkPipelineShaderStageCreateInfo raygenStageCreateInfo = shaders[0].pipelineShaderStageCreateInfo();

    VkRayTracingShaderGroupCreateInfoKHR raygenGroup{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 0,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };

    // I have to store this in a variable first, then do .pSetLayouts = &thatVariable, instead of just
    //  &descriptorSet.getLayout(). I hate C++
    VkDescriptorSetLayout descriptorLayout = descriptorSet.getLayout();
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorLayout,
        .pushConstantRangeCount = 0,  // todo: add push constants
        .pPushConstantRanges = nullptr
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Cannot create pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR rtPipelineCreateInfo {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = 1, // edit as I add more shader stages
        .pStages = &raygenStageCreateInfo,
        .groupCount = 1,  // one shader group
        .pGroups = &raygenGroup,
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

VkImageView vktools::createRtImageView(VkDevice logicalDevice, VkImage rtImage) {
    VkImageViewCreateInfo imageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = rtImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,  // must match format of the image
        // swizzle identity by default
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkImageView imageView;
    if (vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RT image view");
    }

    return imageView;
}

vktools::ImageObjects vktools::createRtImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height) {
    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage image;
    if (vkCreateImage(logicalDevice, &imageCreateInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = vktools::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkDeviceMemory imageMemory;
    if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory)) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(logicalDevice, image, imageMemory, 0);

    return {image, imageMemory};
}


VkCommandBuffer vktools::createCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };


    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    return commandBuffer;
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
            .queueFamilyIndex = indices.graphicsFamily.value(),
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Query ray tracing pipeline features
    VkPhysicalDeviceBufferDeviceAddressFeatures addressFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &addressFeatures
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
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
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
        .pApplicationName = "Raygun",
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

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};  // in this scope to prevent being deleted early
    if (consts::ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(consts::VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = consts::VALIDATION_LAYERS.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    }

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance");
    }

    return instance;
}
