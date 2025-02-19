#include "Blas.h"

#include <stdexcept>

#include "../tools/vktools.h"

reina::graphics::Blas::Blas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue,
                            const reina::graphics::Models& models, const reina::graphics::ModelRange& modelRange) {

    uint32_t vertexCount = static_cast<uint32_t>(models.getVerticesBufferSize()) / 3;

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = {.deviceAddress = models.getVerticesBuffer().getDeviceAddress(logicalDevice)},
            .vertexStride = 4 * sizeof(float),
            .maxVertex = vertexCount - 1,
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = {.deviceAddress = models.getNonOffsetIndicesBuffer().getDeviceAddress(logicalDevice)}
    };

    VkAccelerationStructureGeometryKHR geometry{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {.triangles = triangles},
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
            .primitiveCount = modelRange.indexCount,
            .primitiveOffset = modelRange.indexOffset,
            .firstVertex = modelRange.firstVertex,
            .transformOffset = 0
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildSizesQueryBuildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
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

    blasBuffer = reina::core::Buffer{
            logicalDevice, physicalDevice, buildSizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = blasBuffer.value().getHandle();
    createInfo.size = buildSizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    auto vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkCreateAccelerationStructureKHR"));

    vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr, &blas);

    reina::core::Buffer scratchBufferObjects{
            logicalDevice, physicalDevice, buildSizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = blas,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = {.deviceAddress = scratchBufferObjects.getDeviceAddress(logicalDevice)}
    };

    // Build range
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &buildRangeInfo };

    // Submit the build command
    VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VkCommandBuffer cmdBuffer = vktools::createCommandBuffer(logicalDevice, cmdPool);
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

    scratchBufferObjects.destroy(logicalDevice);
}

VkAccelerationStructureKHR reina::graphics::Blas::getHandle() const {
    return blas;
}

const reina::core::Buffer &reina::graphics::Blas::getBuffer() const {
    return blasBuffer.value();
}

void reina::graphics::Blas::destroy(VkDevice logicalDevice) {
    auto vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkDestroyAccelerationStructureKHR"));

    if (!vkDestroyAccelerationStructureKHR) {
        throw std::runtime_error("Destroy acceleration structure function cannot be found");
    }

    vkDestroyAccelerationStructureKHR(logicalDevice, blas, nullptr);
    blasBuffer.value().destroy(logicalDevice);
}
