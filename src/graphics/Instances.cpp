#include "Instances.h"

#include <unordered_map>

reina::graphics::Instances::Instances(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<Instance>& instances) : instances(instances) {
    computeSamplingDataEmissives();
    constructBuffers(logicalDevice, physicalDevice);
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
        allInstancesCDFs = {};
        emissiveInstancesData = {};
    }

    std::vector<int> emissiveInstanceIndices{};
    for (int i = 0; i < instances.size(); i++) {
        if (instances[i].isEmissive()) {
            emissiveInstanceIndices.push_back(i);
        }
    }

    std::unordered_map<size_t, size_t> duplicates = computeEmissiveDuplicates(emissiveInstanceIndices);

    allInstancesCDFs = std::vector<float>(0);
    emissiveInstancesData = std::vector<InstanceData>(emissiveInstanceIndices.size());

    for (int instanceIdxIdx = 0; instanceIdxIdx < emissiveInstanceIndices.size(); instanceIdxIdx++) {
        int instanceIdx = emissiveInstanceIndices[instanceIdxIdx];
        bool isDuplicate = duplicates.contains(instanceIdxIdx);

        const Instance& instance = instances[instanceIdx];
        InstanceData& instanceData = emissiveInstancesData[instanceIdxIdx];

        instanceData.transform = instance.getTransform();
        instanceData.materialOffset = instance.getMaterialOffset();
        instanceData.indexOffset = instance.getModelRange().indexOffset;

        if (isDuplicate) {
            const InstanceData& duplicateOf = emissiveInstancesData[duplicates.at(instanceIdx)];
            instanceData.cdfRangeStart = duplicateOf.cdfRangeStart;
            instanceData.cdfRangeEnd = duplicateOf.cdfRangeEnd;
            continue;
        }

        const std::vector<float>& cdf = instance.getCDF();

        instanceData.cdfRangeStart = allInstancesCDFs.size();
        instanceData.cdfRangeEnd = instanceData.cdfRangeStart + cdf.size() - 1;

        allInstancesCDFs.reserve(instance.getCDF().size());
        allInstancesCDFs.insert(allInstancesCDFs.end(), cdf.begin(), cdf.end());
    }
}

const std::vector<reina::graphics::Instance>& reina::graphics::Instances::getInstances() const {
    return instances;
}

void reina::graphics::Instances::constructBuffers(VkDevice logicalDevice, VkPhysicalDevice physicalDevice) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkMemoryAllocateFlags allocFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    allInstancesCDFsBuffer = reina::core::Buffer{
        logicalDevice, physicalDevice, allInstancesCDFs,
        usage, allocFlags, memFlags
    };

    emissiveInstancesDataBuffer = reina::core::Buffer{
        logicalDevice, physicalDevice, emissiveInstancesData,
        usage, allocFlags, memFlags
    };
}

void reina::graphics::Instances::destroy(VkDevice logicalDevice) {
    if (allInstancesCDFsBuffer.has_value()) {
        allInstancesCDFsBuffer->destroy(logicalDevice);
    } if (emissiveInstancesDataBuffer.has_value()) {
        emissiveInstancesDataBuffer->destroy(logicalDevice);
    }
}

const reina::core::Buffer &reina::graphics::Instances::getEmissiveInstancesDataBuffer() const {
    return emissiveInstancesDataBuffer.value();
}

const reina::core::Buffer &reina::graphics::Instances::getAllInstancesCDFsBuffer() const {
    return allInstancesCDFsBuffer.value();
}
