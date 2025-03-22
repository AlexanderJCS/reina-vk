#include "Blas.h"

#include <stdexcept>
#include <iostream>

#include "../tools/vktools.h"
#include "../core/CmdBuffer.h"

reina::graphics::Blas::Blas(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue,
                            const reina::graphics::Models& models, const reina::graphics::ModelRange& modelRange, bool shouldCompact) {

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
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | (shouldCompact ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR : static_cast<VkBuildAccelerationStructureFlagsKHR>(0)),
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .geometryCount = 1,
            .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR buildSizes{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    vktools::loadVkFunc(logicalDevice, "vkGetAccelerationStructureBuildSizesKHR", vkGetAccelerationStructureBuildSizesKHR);

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
            .buffer = blasBuffer.getHandle(),
            .size = buildSizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    vktools::loadVkFunc(logicalDevice, "vkCreateAccelerationStructureKHR", vkCreateAccelerationStructureKHR);

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
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | (shouldCompact ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR : static_cast<VkBuildAccelerationStructureFlagsKHR>(0)),
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = blas,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = {.deviceAddress = scratchBuffer.getDeviceAddress(logicalDevice)}
    };

    // Build range
    const VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &buildRangeInfo };

    // Submit the build command
    reina::core::CmdBuffer cmdBuffer{logicalDevice, cmdPool, true};

    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    vktools::loadVkFunc(logicalDevice, "vkCmdBuildAccelerationStructuresKHR", vkCmdBuildAccelerationStructuresKHR);

    vkCmdBuildAccelerationStructuresKHR(cmdBuffer.getHandle(), 1, &buildInfo, rangeInfos);

    cmdBuffer.endWaitSubmit(logicalDevice, queue);
    cmdBuffer.destroy(logicalDevice);
    scratchBuffer.destroy(logicalDevice);

    if (shouldCompact) {
        VkDeviceSize compactSize = compact(logicalDevice, physicalDevice, cmdPool, queue);
        auto beforeSize = static_cast<float>(buildSizes.accelerationStructureSize);
        auto afterSize = static_cast<float>(compactSize);
        float percentDiff = (beforeSize - afterSize) / beforeSize * 100;
        std::cout << "BLAS Compaction: reduced size by " << percentDiff << "%\n\n";
    }
}

VkAccelerationStructureKHR reina::graphics::Blas::getHandle() const {
    return blas;
}

const reina::core::Buffer& reina::graphics::Blas::getBuffer() const {
    return blasBuffer;
}

void reina::graphics::Blas::destroy(VkDevice logicalDevice) {
    auto vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(logicalDevice, "vkDestroyAccelerationStructureKHR"));

    if (!vkDestroyAccelerationStructureKHR) {
        throw std::runtime_error("Destroy acceleration structure function cannot be found");
    }

    vkDestroyAccelerationStructureKHR(logicalDevice, blas, nullptr);
    blasBuffer.destroy(logicalDevice);
}

VkDeviceSize reina::graphics::Blas::compact(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue) {
    reina::core::CmdBuffer cmdBuffer{logicalDevice, cmdPool, true};
    VkCommandBuffer cmdBufferHandle = cmdBuffer.getHandle();

    // create query pool
    VkQueryPoolCreateInfo queryPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount = 1
    };
    VkQueryPool queryPool;
    vkCreateQueryPool(logicalDevice, &queryPoolCreateInfo, nullptr, &queryPool);

    vkCmdResetQueryPool(cmdBufferHandle, queryPool, 0, 1);

    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
    vktools::loadVkFunc(logicalDevice, "vkCmdWriteAccelerationStructuresPropertiesKHR", vkCmdWriteAccelerationStructuresPropertiesKHR);

    // write the compact buffer size
    vkCmdWriteAccelerationStructuresPropertiesKHR(
            cmdBufferHandle, 1, &blas,
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0
    );

    cmdBuffer.endWaitSubmit(logicalDevice, queue);

    VkDeviceSize compactSize;
    vkGetQueryPoolResults(logicalDevice, queryPool, 0, 1,
                          sizeof(VkDeviceSize), &compactSize, sizeof(VkDeviceSize),
                          VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);

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

    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    vktools::loadVkFunc(logicalDevice, "vkCreateAccelerationStructureKHR", vkCreateAccelerationStructureKHR);

    VkAccelerationStructureKHR compactBlas;
    vkCreateAccelerationStructureKHR(logicalDevice, &asCreateInfo, nullptr, &compactBlas);

    cmdBuffer.begin();

    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    vktools::loadVkFunc(logicalDevice, "vkCmdCopyAccelerationStructureKHR", vkCmdCopyAccelerationStructureKHR);

    VkCopyAccelerationStructureInfoKHR copyInfo{
        .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
        .src = blas,
        .dst = compactBlas,
        .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
    };
    vkCmdCopyAccelerationStructureKHR(cmdBufferHandle, &copyInfo);

    cmdBuffer.endWaitSubmit(logicalDevice, queue);
    cmdBuffer.destroy(logicalDevice);

    destroy(logicalDevice);
    vkDestroyQueryPool(logicalDevice, queryPool, nullptr);

    blas = compactBlas;
    blasBuffer = compactBlasBuffer;

    return compactSize;
}

reina::graphics::Blas::Blas() {

}
