#ifndef REINA_VK_INSTANCES_H
#define REINA_VK_INSTANCES_H

#include "Instance.h"

#include "../core/Buffer.h"

#include <map>
#include <optional>
#include <glm/mat4x4.hpp>
#include <set>
#include <unordered_map>

namespace reina::graphics {
    struct alignas(16) InstanceData {
        // todo: put this in a polyglot file
        glm::mat4x4 transform;
        uint32_t materialOffset;
        uint32_t cdfRangeStart;
        uint32_t cdfRangeEnd;
        uint32_t indexOffset;
        float area;
        glm::vec3 padding;
    };

    class Instances {
    public:
        Instances() = default;
        explicit Instances(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<Instance>& instances);

        [[nodiscard]] const std::vector<Instance>& getInstances() const;

        [[nodiscard]] const reina::core::Buffer& getEmissiveMetadataBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getCdfTrianglesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getCdfInstancesBuffer() const;

        void destroy(VkDevice logicalDevice);

    private:
        /**
         * @return Pairs of duplicates in the instances vector. Larger index is always the key.
         */
        std::unordered_map<size_t, size_t> computeEmissiveDuplicates(const std::vector<int>& emissiveInstancesIndices);

        /**
         * Constructs the buffers given the initialized cdfTriangles and emissiveInstancesData member variables
         */
        void createBuffers(VkDevice logicalDevice, VkPhysicalDevice physicalDevice);

        /**
         * Does the following:
         * - Analyzes all instances and puts their CDFs in the cdfTriangles vector
         * - Populates emissiveInstancesData
         * - Removes any duplicate data and has the InstanceData.cdfRangeStart and InstanceData.cdfRangeEnd point to
         *    the correct place.
         */
        void computeSamplingDataEmissives();

        float instancesArea = 0;

        std::vector<Instance> instances;
        std::vector<float> cdfTriangles;
        std::vector<float> cdfInstances;
        std::vector<InstanceData> emissiveInstancesData;

        reina::core::Buffer emissiveMetadataBuffer;
        reina::core::Buffer cdfTrianglesBuffer;
        reina::core::Buffer cdfInstancesBuffer;
    };
}


#endif //REINA_VK_INSTANCES_H
