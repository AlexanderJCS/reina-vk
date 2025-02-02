#ifndef RAYGUN_VK_VKTOOLS_H
#define RAYGUN_VK_VKTOOLS_H

#include <optional>
#include <utility>
#include <vector>
#include <string>
#include <memory>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "Shader.h"
#include "DescriptorSet.h"

namespace vktools {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        [[nodiscard]] bool isComplete() const;
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct SwapchainObjects {
        VkSwapchainKHR swapchain;
        std::vector<VkImage> swapchainImages;
        VkFormat swapchainImageFormat;
        VkExtent2D swapchainExtent;
    };

    struct ImageObjects {
        VkImage image;
        VkDeviceMemory imageMemory;
    };

    struct SbtSpacing {
        VkDeviceSize headerSize;
        VkDeviceSize baseAlignment;
        VkDeviceSize handleAlignment;
        VkDeviceSize stride;
    };

    struct PipelineInfo {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
    };

    struct BufferObjects {
        VkBuffer buffer;
        VkDeviceMemory deviceMemory;
    };

    struct SyncObjects {
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;
    };

    struct AccStructureInfo {
        VkAccelerationStructureKHR accelerationStructure;
        BufferObjects buffer;
    };

    VkDeviceAddress getBufferDeviceAddress(VkDevice logicalDevice, VkBuffer buffer);
    std::vector<char> readFile(const std::string& filename);
    bool hasValidationLayerSupport();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    QueueFamilyIndices findQueueFamilies(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice);
    SwapChainSupportDetails querySwapChainSupport(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
    );

    // todo: rename the create and destroy debug utils messenger EXT functions to something in-line with other code standards
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    uint64_t getDeviceLocalMemory(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);

    vktools::BufferObjects createBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkDeviceSize dataSize, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkMemoryPropertyFlags memFlags);

    template<typename T>
    vktools::BufferObjects createBuffer(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<T>& data, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkMemoryPropertyFlags memFlags) {
        BufferObjects bufferObjects = createBuffer(logicalDevice, physicalDevice, data.empty() ? 0 : sizeof(data[0]) * data.size(), usage, allocFlags, memFlags);

        void* bufferData;
        vkMapMemory(logicalDevice, bufferObjects.deviceMemory, 0, data.size(), 0, &bufferData);
        memcpy(bufferData, data.data(), data.size());
        vkUnmapMemory(logicalDevice, bufferObjects.deviceMemory);

        return bufferObjects;
    }

    vktools::AccStructureInfo createTlas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::vector<VkAccelerationStructureKHR>& blases, VkDeviceSize sbtStride);
    vktools::AccStructureInfo createBlas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, VkBuffer verticesBuffer, VkBuffer indicesBuffer, size_t verticesLen, size_t indicesLen);
    SyncObjects createSyncObjects(VkDevice logicalDevice);
    SbtSpacing calculateSbtSpacing(VkPhysicalDevice physicalDevice);
    BufferObjects createSbt(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkPipeline rtPipeline, SbtSpacing spacing);
    PipelineInfo createRtPipeline(VkDevice logicalDevice, const DescriptorSet& descriptorSet, const std::vector<Shader>& shaders);
    VkImageView createRtImageView(VkDevice logicalDevice, VkImage rtImage);
    ImageObjects createRtImage(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height);

    VkCommandBuffer createCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool);
    VkCommandPool createCommandPool(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSurfaceKHR surface);
    std::vector<VkImageView> createSwapchainImageViews(VkDevice logicalDevice, VkFormat swapchainImageFormat, std::vector<VkImage> swapchainImages);
    SwapchainObjects createSwapchain(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice logicalDevice, int windowWidth, int windowHeight);
    VkDevice createLogicalDevice(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice);
    VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window);
    std::optional<VkDebugUtilsMessengerEXT> createDebugMessenger(VkInstance instance);
    VkInstance createInstance();
}


#endif //RAYGUN_VK_VKTOOLS_H
