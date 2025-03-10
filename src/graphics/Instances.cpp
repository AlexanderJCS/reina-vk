#include "Instances.h"

#include <unordered_map>

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
    // all this code is crap. delete it and start again.
    //  P.S. I left almost no comments to help you understand this code :)
    if (instances.empty()) {
        allInstancesCDFs = {};
        emissiveInstancesData = {};
    }

    std::vector<int> emissiveInstancesIndices{};
    for (int i = 0; i < instances.size(); i++) {
        if (instances[i].isEmissive()) {
            emissiveInstancesIndices.push_back(i);
        }
    }

    std::unordered_map<size_t, size_t> duplicates = computeEmissiveDuplicates(emissiveInstancesIndices);

    allInstancesCDFs = std::vector<float>(0);
    emissiveInstancesData = std::vector<InstanceData>(emissiveInstancesIndices.size());

    for (int instanceIdxIdx = 0; instanceIdxIdx < emissiveInstancesIndices.size(); instanceIdxIdx++) {
        int instanceIdx = emissiveInstancesIndices[instanceIdxIdx];
        bool isDuplicate = duplicates.contains(instanceIdxIdx);

        const Instance& instance = instances[instanceIdx];
        InstanceData& instanceData = emissiveInstancesData[instanceIdxIdx];

        instanceData.transform = instance.getTransform();
        instanceData.materialOffset = instance.getMaterialOffset();

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

reina::graphics::Instances::Instances(const std::vector<Instance>& instances) : instances(instances) {
    computeSamplingDataEmissives();
}

const std::vector<reina::graphics::Instance>& reina::graphics::Instances::getInstances() const {
    return instances;
}
