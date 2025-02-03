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

VkDeviceAddress vktools::getBufferDeviceAddress(VkDevice logicalDevice, VkBuffer buffer) {
    VkBufferDeviceAddressInfo addressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };

    return vkGetBufferDeviceAddress(logicalDevice, &addressInfo);
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

vktools::BufferObjects vktools::createBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkDeviceSize dataSize,
                      VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkMemoryPropertyFlags memFlags) {
    VkBufferCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = dataSize,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer buffer;
    if (vkCreateBuffer(logicalDevice, &createInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = allocFlags
    };

    VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &allocFlagsInfo,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, memFlags)
    };

    VkDeviceMemory deviceMemory;
    if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &deviceMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(logicalDevice, buffer, deviceMemory, 0);

    return {buffer, deviceMemory};
}

vktools::AccStructureInfo vktools::createTlas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice,
                                              VkCommandPool cmdPool, VkQueue queue,
                                              const std::vector<VkAccelerationStructureKHR>& blases,
                                              VkDeviceSize sbtStride) {

    std::vector<VkAccelerationStructureInstanceKHR> instances;
    for (const auto& blas : blases) {
        VkTransformMatrixKHR identity{};
        identity.matrix[0][0] = 1;
        identity.matrix[1][1] = 1;
        identity.matrix[2][2] = 1;

        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = blas
        };

        auto vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                vkGetDeviceProcAddr(logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR"));
        VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &addressInfo);

        VkAccelerationStructureInstanceKHR instance{
                .transform = identity, // VkTransformMatrixKHR
                .instanceCustomIndex = 0,  // material id
                .mask = 0xFF,
                .instanceShaderBindingTableRecordOffset = static_cast<uint32_t>(sbtStride),
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
                .accelerationStructureReference = blasAddress
        };
        instances.push_back(instance);
    }

    // Create the instance buffer
    BufferObjects instanceBufferObjects = createBuffer(
            logicalDevice, physicalDevice,
            instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE, // Contiguous array (not pointers)
            .data = {.deviceAddress = getBufferDeviceAddress(logicalDevice, instanceBufferObjects.buffer)}
    };

    VkAccelerationStructureGeometryKHR geometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {.instances = instancesData},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR // Assume instances are opaque
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
    auto instanceCount = static_cast<uint32_t>(instances.size());
    vkGetAccelerationStructureBuildSizesKHR(
            logicalDevice,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &instanceCount,
            &buildSizes
    );

    // Create TLAS buffer
    BufferObjects tlasBufferObjects = createBuffer(
            logicalDevice, physicalDevice,
            buildSizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Create acceleration structure
    VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = tlasBufferObjects.buffer,
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
    BufferObjects scratchBufferObjects = createBuffer(
            logicalDevice, physicalDevice, buildSizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    buildInfo.dstAccelerationStructure = tlas;
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(logicalDevice, scratchBufferObjects.buffer);

    // Submit the build command
    VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VkCommandBuffer cmdBuffer = createCommandBuffer(logicalDevice, cmdPool);
    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Could not begin one-time command buffer for BLAS creation");
    }

    auto vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkCmdBuildAccelerationStructuresKHR"));
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &pBuildRangeInfo);

    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer for BLAS creation");
    }

    // Submit the command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    // Create a fence to synchronize
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence for BLAS creation");
    }

    // Submit the command buffer and wait for it to complete
    if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer for BLAS creation");
    }

    if (vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for fence for BLAS creation");
    }

    // Clean up temporary resources
    vkDestroyFence(logicalDevice, fence, nullptr);
    vkFreeCommandBuffers(logicalDevice, cmdPool, 1, &cmdBuffer);
    vkDestroyBuffer(logicalDevice, scratchBufferObjects.buffer, nullptr);
    vkFreeMemory(logicalDevice, scratchBufferObjects.deviceMemory, nullptr);
    vkDestroyBuffer(logicalDevice, instanceBufferObjects.buffer, nullptr);
    vkFreeMemory(logicalDevice, instanceBufferObjects.deviceMemory, nullptr);

    return {tlas, tlasBufferObjects};
}

vktools::AccStructureInfo vktools::createBlas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, VkBuffer verticesBuffer, VkBuffer indicesBuffer, size_t verticesLen, size_t indicesLen) {
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = getBufferDeviceAddress(logicalDevice, verticesBuffer)},
        .vertexStride = 3 * sizeof(float),  // I think this is correct
        .maxVertex = static_cast<uint32_t>(verticesLen) - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = getBufferDeviceAddress(logicalDevice, indicesBuffer)},
        // todo: add transform with .transformData
    };

    VkAccelerationStructureGeometryKHR geometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {.triangles = triangles},
            .flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR  // todo: do i need this flag?
    };

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
        .primitiveCount = static_cast<uint32_t>(indicesLen) / 3,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildSizesQueryBuildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR buildSizes{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    auto vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkGetAccelerationStructureBuildSizesKHR"));

    vkGetAccelerationStructureBuildSizesKHR(
            logicalDevice,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildSizesQueryBuildInfo,
            &buildRangeInfo.primitiveCount,
            &buildSizes
    );

    BufferObjects blasBufferObjects = createBuffer(
            logicalDevice, physicalDevice, buildSizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                 );

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = blasBufferObjects.buffer;
    createInfo.size = buildSizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    auto vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkCreateAccelerationStructureKHR"));

    VkAccelerationStructureKHR blas;
    vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr, &blas);

    BufferObjects scratchBufferObjects = createBuffer(
            logicalDevice, physicalDevice, buildSizes.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = blas,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = {.deviceAddress = getBufferDeviceAddress(logicalDevice, scratchBufferObjects.buffer)}
    };

    // Build range
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &buildRangeInfo };

    // Submit the build command
    VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VkCommandBuffer cmdBuffer = createCommandBuffer(logicalDevice, cmdPool);
    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Could not begin one-time command buffer for BLAS creation");
    }

    auto vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkCmdBuildAccelerationStructuresKHR"));
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, rangeInfos);

    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer for BLAS creation");
    }

    // Submit the command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    // Create a fence to synchronize
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence for BLAS creation");
    }

    // Submit the command buffer and wait for it to complete
    if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer for BLAS creation");
    }

    if (vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for fence for BLAS creation");
    }

    // Clean up temporary resources
    vkDestroyFence(logicalDevice, fence, nullptr);
    vkFreeCommandBuffers(logicalDevice, cmdPool, 1, &cmdBuffer);
    vkDestroyBuffer(logicalDevice, scratchBufferObjects.buffer, nullptr);
    vkFreeMemory(logicalDevice, scratchBufferObjects.deviceMemory, nullptr);

    // Return the BLAS buffer and memory
    return {blas, blasBufferObjects};
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
        throw std::runtime_error("SBT base alignment is not a multiple of SBT handle alignment");
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

vktools::BufferObjects vktools::createSbt(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkPipeline rtPipeline, SbtSpacing sbtSpacing, int shaderGroups) {
    std::vector<uint8_t> cpuShaderHandleStorage(sbtSpacing.headerSize * shaderGroups);

    auto vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR"));

    if (vkGetRayTracingShaderGroupHandlesKHR(logicalDevice, rtPipeline, 0, shaderGroups, cpuShaderHandleStorage.size(), cpuShaderHandleStorage.data()) != VK_SUCCESS) {
        throw std::runtime_error("Could not get RT shader group handles");
    }

    auto sbtSize = static_cast<uint32_t>(sbtSpacing.stride * shaderGroups);
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
            .memoryTypeIndex = vktools::findMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VkDeviceMemory sbtBufferMemory;
    if (vkAllocateMemory(logicalDevice, &memoryAllocateInfo, nullptr, &sbtBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate SBT buffer memory");
    }

    vkBindBufferMemory(logicalDevice, sbtBuffer, sbtBufferMemory, 0);

    void* sbtMappedMemory;
    vkMapMemory(logicalDevice, sbtBufferMemory, 0, sbtSize, 0, &sbtMappedMemory);

    auto* sbtPtr = static_cast<uint8_t*>(sbtMappedMemory);
    for (uint32_t groupIdx = 0; groupIdx < shaderGroups; groupIdx++) {
        memcpy(&sbtPtr[groupIdx * sbtSpacing.stride], &cpuShaderHandleStorage[groupIdx * sbtSpacing.headerSize], sbtSpacing.headerSize);
    }

    vkUnmapMemory(logicalDevice, sbtBufferMemory);

    return {sbtBuffer, sbtBufferMemory};
}

vktools::PipelineInfo vktools::createRtPipeline(VkDevice logicalDevice, const DescriptorSet& descriptorSet, const std::vector<Shader>& shaders) {
    if (shaders.size() != 3) {
        throw std::runtime_error("Must have 3 shaders in the order: raygen, miss, closest hit");
    }

    std::array<VkPipelineShaderStageCreateInfo, 3> stages{};
    for (int i = 0; i < shaders.size(); i++) {
        stages[i] = shaders[i].pipelineShaderStageCreateInfo("main");
    }

    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups{};
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
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
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
        .stageCount = stages.size(),
        .pStages = stages.data(),
        .groupCount = groups.size(),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 31,  // todo: query hardware limitations
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
