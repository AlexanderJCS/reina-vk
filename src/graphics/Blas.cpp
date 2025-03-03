#include "Blas.h"

#include <stdexcept>
#include <iostream>

#include "../tools/vktools.h"

reina::graphics::Blas::Blas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue,
                            const reina::graphics::Models& models, const reina::graphics::ModelRange& modelRange, bool compact) {

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
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | (compact ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR : static_cast<VkBuildAccelerationStructureFlagsKHR>(0)),
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

    VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = blasBuffer.value().getHandle(),
            .size = buildSizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    auto vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkCreateAccelerationStructureKHR"));

    vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr, &blas);

    reina::core::Buffer scratchBuffer{
            logicalDevice, physicalDevice, buildSizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | (compact ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR : static_cast<VkBuildAccelerationStructureFlagsKHR>(0)),
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = blas,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = {.deviceAddress = scratchBuffer.getDeviceAddress(logicalDevice)}
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

    // Create a fence to synchronize
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;
    if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence for BLAS creation");
    }

    // Submit the command buffer
    VkSubmitInfo queueSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer
    };
    if (vkQueueSubmit(queue, 1, &queueSubmitInfo, fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer for BLAS creation");
    }

    if (vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for fence for BLAS creation");
    }

    // Clean up temporary resources
    vkDestroyFence(logicalDevice, fence, nullptr);
    vkFreeCommandBuffers(logicalDevice, cmdPool, 1, &cmdBuffer);

    scratchBuffer.destroy(logicalDevice);

    if (compact) {
        // todo: clean this code up
        if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence for compact BLAS creation");
        }

        VkCommandBuffer compactCmdBuffer = vktools::createCommandBuffer(logicalDevice, cmdPool);
        if (vkBeginCommandBuffer(compactCmdBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Could not begin command buffer for querying compact BLAS size");
        }

        // create query pool
        VkQueryPoolCreateInfo queryPoolCreateInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        queryPoolCreateInfo.queryCount = 1;
        queryPoolCreateInfo.queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
        VkQueryPool queryPool;
        vkCreateQueryPool(logicalDevice, &queryPoolCreateInfo, nullptr, &queryPool);

        vkCmdResetQueryPool(compactCmdBuffer, queryPool, 0, 1);

        auto vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
                vkGetDeviceProcAddr(logicalDevice, "vkCmdWriteAccelerationStructuresPropertiesKHR"));

        // write the compact buffer size
        vkCmdWriteAccelerationStructuresPropertiesKHR(
                compactCmdBuffer, 1, &blas,
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0
                );

        queueSubmitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &compactCmdBuffer
        };

        if (vkEndCommandBuffer(compactCmdBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer for querying compact BLAS size");
        }

        if (vkQueueSubmit(queue, 1, &queueSubmitInfo, fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit command buffer for querying compact BLAS size");
        }

        if (vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for fence for querying compact BLAS size");
        }
        vkDestroyFence(logicalDevice, fence, nullptr);

VkDeviceSize compactSize = 0;  // not initializing this causes undefined behavior
vkGetQueryPoolResults(logicalDevice, queryPool, 0, 1,
                      sizeof(VkDeviceSize), &compactSize, sizeof(VkDeviceSize),
                      VK_QUERY_RESULT_WAIT_BIT);

        // Create a compact version of the BLAS
        reina::core::Buffer compactBlasBuffer{
                logicalDevice, physicalDevice, compactSize,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };

        VkAccelerationStructureCreateInfoKHR asCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = compactBlasBuffer.getHandle(),
            .size = compactSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
        };

        VkAccelerationStructureKHR compactBlas;
        vkCreateAccelerationStructureKHR(logicalDevice, &asCreateInfo, nullptr, &compactBlas);

        // Copy the original BLAS into a compact version
        if (vkBeginCommandBuffer(compactCmdBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Could not begin command buffer for compact BLAS copying");
        }

        auto vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
                vkGetDeviceProcAddr(logicalDevice, "vkCmdCopyAccelerationStructureKHR"));

        VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
        copyInfo.src  = blas;
        copyInfo.dst  = compactBlas;
        copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
        vkCmdCopyAccelerationStructureKHR(compactCmdBuffer, &copyInfo);

        if (vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence for compact BLAS creation");
        }

        if (vkEndCommandBuffer(compactCmdBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer for compact BLAS copying");
        }

        if (vkQueueSubmit(queue, 1, &queueSubmitInfo, fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit command buffer for compact BLAS copying");
        }

        if (vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for fence for compact BLAS copying");
        }

        vkDestroyFence(logicalDevice, fence, nullptr);

        destroy(logicalDevice);
        vkDestroyQueryPool(logicalDevice, queryPool, nullptr);

        blas = compactBlas;
        blasBuffer = compactBlasBuffer;

        std::cout << "BLAS Compaction - reduced size by " << ((float) buildSizes.accelerationStructureSize - (float) compactSize) / (float) buildSizes.accelerationStructureSize * 100 << "%\n\n";
    }
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
