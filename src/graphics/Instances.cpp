#include "Instances.h"

#include <unordered_map>
#include <cstring>
#include <stdexcept>

reina::graphics::Instances::Instances(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<Instance>& instances) : instances(instances) {
    computeSamplingDataEmissives();
    createBuffers(logicalDevice, physicalDevice);
}

static bool compareFloatVectors(const std::vector<float>& vec1, const std::vector<float>& vec2) {
    const float tolerance = std::numeric_limits<float>::epsilon();

    if (vec1.size() != vec2.size()) {
        return false;
    }

    for (size_t i = 0; i < vec1.size(); ++i) {
        if (std::fabs(vec1[i] - vec2[i]) > tolerance) {
            return false;
        }
    }

    return true;
}

std::unordered_map<size_t, size_t> reina::graphics::Instances::computeEmissiveDuplicates(const std::vector<int>& emissiveInstancesIndices) {
    // todo: performance in this function may be optimized
    std::unordered_map<size_t, size_t> duplicateMapping;

    for (size_t idxI = 0; idxI < emissiveInstancesIndices.size(); idxI++) {
        int i = emissiveInstancesIndices[idxI];

        for (size_t idxJ = idxI + 1; idxJ < emissiveInstancesIndices.size(); idxJ++) {
            int j = emissiveInstancesIndices[idxJ];

            if (!compareFloatVectors(instances[i].getCDF(), instances[j].getCDF())) {
                continue;
            }

            // Only set the canonical index if not already assigned.
            if (duplicateMapping.find(j) == duplicateMapping.end()) {
                duplicateMapping[j] = i;
            }
        }
    }

    return duplicateMapping;
}

void reina::graphics::Instances::computeSamplingDataEmissives() {
    if (instances.empty()) {
        cdfTriangles = {};
        emissiveInstancesData = {};
    }

    std::vector<int> emissiveInstanceIndices{};
    for (int i = 0; i < instances.size(); i++) {
        if (instances[i].isEmissive()) {
            emissiveInstanceIndices.push_back(i);
        }
    }

    std::unordered_map<size_t, size_t> duplicates = computeEmissiveDuplicates(emissiveInstanceIndices);

    cdfTriangles = std::vector<float>(0);
    emissiveInstancesData = std::vector<InstanceData>(emissiveInstanceIndices.size());

    for (int instanceIdxIdx = 0; instanceIdxIdx < emissiveInstanceIndices.size(); instanceIdxIdx++) {
        int instanceIdx = emissiveInstanceIndices[instanceIdxIdx];
        bool isDuplicate = duplicates.contains(instanceIdxIdx);

        const Instance& instance = instances[instanceIdx];
        InstanceData& instanceData = emissiveInstancesData[instanceIdxIdx];

        instanceData.transform = instance.getTransform();
        instanceData.materialOffset = instance.getMaterialOffset();
        instanceData.indexOffset = instance.getModelRange().indexOffset;
        instanceData.emission = instance.getEmission();
        instanceData.weight = instance.getWeight();
        instanceData.area = instance.getArea();
        instanceData.cullBackface = instance.isCullBackface();

        if (isDuplicate) {
            const InstanceData& duplicateOf = emissiveInstancesData[duplicates.at(instanceIdx)];
            instanceData.cdfRangeStart = duplicateOf.cdfRangeStart;
            instanceData.cdfRangeEnd = duplicateOf.cdfRangeEnd;
            continue;
        }

        const std::vector<float>& cdf = instance.getCDF();

        instanceData.cdfRangeStart = cdfTriangles.size();
        instanceData.cdfRangeEnd = instanceData.cdfRangeStart + cdf.size() - 1;

        cdfTriangles.reserve(instance.getCDF().size());
        cdfTriangles.insert(cdfTriangles.end(), cdf.begin(), cdf.end());
    }

    cdfInstances = std::vector<float>(emissiveInstanceIndices.size());

    float cumulativeArea = 0;
    for (int i = 0; i < cdfInstances.size(); i++) {
        int instanceIdx = emissiveInstanceIndices[i];
        cumulativeArea += instances[instanceIdx].getWeight();
        cdfInstances[i] = cumulativeArea;
    }

    for (float& val : cdfInstances) {
        val /= cumulativeArea;
    }
    emissiveInstancesArea = cumulativeArea;
}

const std::vector<reina::graphics::Instance>& reina::graphics::Instances::getInstances() const {
    return instances;
}

void reina::graphics::Instances::createBuffers(VkDevice logicalDevice, VkPhysicalDevice physicalDevice) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkMemoryAllocateFlags allocFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (cdfTriangles.empty()) {
        throw std::runtime_error("Scene must have at least one emissive object");
    }

    cdfTrianglesBuffer = reina::core::Buffer{
            logicalDevice, physicalDevice, cdfTriangles,
            usage, allocFlags, memFlags
    };

    std::vector<uint8_t> packedBuffer;

    auto count = static_cast<uint32_t>(cdfInstances.size());
    auto* countPtr = reinterpret_cast<uint8_t*>(&count);
    packedBuffer.insert(packedBuffer.end(), countPtr, countPtr + sizeof(uint32_t));

    auto* areaPtr = reinterpret_cast<uint8_t*>(&emissiveInstancesArea);
    packedBuffer.insert(packedBuffer.end(), areaPtr, areaPtr + sizeof(float));

    auto* cdfDataPtr = reinterpret_cast<uint8_t*>(cdfInstances.data());
    packedBuffer.insert(packedBuffer.end(), cdfDataPtr, cdfDataPtr + cdfInstances.size() * sizeof(float));

    cdfInstancesBuffer = reina::core::Buffer{
        logicalDevice, physicalDevice, packedBuffer,
        usage, allocFlags, memFlags
    };

    emissiveMetadataBuffer = reina::core::Buffer{
        logicalDevice, physicalDevice, emissiveInstancesData,
        usage, allocFlags, memFlags
    };
}

void reina::graphics::Instances::destroy(VkDevice logicalDevice) {
    cdfTrianglesBuffer.destroy(logicalDevice);
    emissiveMetadataBuffer.destroy(logicalDevice);
    cdfInstancesBuffer.destroy(logicalDevice);
}

const reina::core::Buffer& reina::graphics::Instances::getEmissiveMetadataBuffer() const {
    return emissiveMetadataBuffer;
}

const reina::core::Buffer& reina::graphics::Instances::getCdfTrianglesBuffer() const {
    return cdfTrianglesBuffer;
}

const reina::core::Buffer& reina::graphics::Instances::getCdfInstancesBuffer() const {
    return cdfInstancesBuffer;
}

float reina::graphics::Instances::getEmissiveInstancesWeight() const {
    return emissiveInstancesArea;
}
