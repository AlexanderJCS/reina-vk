#ifndef REINA_VK_INSTANCES_H
#define REINA_VK_INSTANCES_H

#include "Instance.h"

#include <map>
#include <glm/mat4x4.hpp>
#include <set>
#include <unordered_map>

namespace reina::graphics {
    struct InstanceData {
        glm::mat4x4 transform;
        uint32_t materialOffset;
        uint32_t cdfRangeStart;
        uint32_t cdfRangeEnd;
        uint32_t indexOffset;
    };

    class Instances {
    public:
        explicit Instances(const std::vector<Instance>& instances);

        [[nodiscard]] const std::vector<Instance>& getInstances() const;

    private:
        /**
         * @return Pairs of duplicates in the instances vector. Larger index is always the key.
         */
        std::unordered_map<size_t, size_t> computeEmissiveDuplicates(const std::vector<int>& emissiveInstancesIndices);

        /**
         * Does the following:
         * - Analyzes all instances and puts their CDFs in the allInstancesCDFs vector
         * - Populates emissiveInstancesData
         * - Removes any duplicate data and has the InstanceData.cdfRangeStart and InstanceData.cdfRangeEnd point to
         *    the correct place.
         */
        void computeSamplingDataEmissives();

        std::vector<Instance> instances;
        std::vector<float> allInstancesCDFs;
        std::vector<InstanceData> emissiveInstancesData;
    };
}


#endif //REINA_VK_INSTANCES_H
