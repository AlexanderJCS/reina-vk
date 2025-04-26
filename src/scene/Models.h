#ifndef RAYGUN_VK_MODELS_H
#define RAYGUN_VK_MODELS_H

#include <vector>
#include <string>
#include <optional>
#include <glm/glm.hpp>

#include "../core/Buffer.h"

namespace reina::scene {
    struct ModelRange {
        uint32_t firstVertex;
        uint32_t firstNormal;
        uint32_t indexOffset;
        uint32_t tbnsIndexOffset;
        uint32_t texIndexOffset;
        uint32_t indexCount;
        uint32_t tbnsIndexCount;
        uint32_t texIndexCount;
    };

    struct ModelData {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        std::vector<glm::mat3> tbns;
        std::vector<uint32_t> tbnsIndices;
        std::vector<float> texCoords;
        std::vector<uint32_t> texIndices;
    };

    class Models {
    public:
        Models() = default;
        Models(const std::vector<std::string>& modelFilepaths);

        /**
         * @param filepath The filepath of the model OBJ
         * @return The model ID
         */
        uint32_t addModel(const std::string& filepath);

        /**
         * @param objData The model's OBJ data
         * @return The model ID
         */
        uint32_t addModel(const ModelData& objData);
        void buildBuffers(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue);

        [[nodiscard]] size_t getVerticesBufferSize() const;

        [[nodiscard]] const reina::core::Buffer& getVerticesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getOffsetIndicesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getNonOffsetIndicesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getTbnsBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getOffsetTbnsIndicesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getTexCoordsBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getOffsetTexIndicesBuffer() const;

        [[nodiscard]] const ModelData& getModelData(uint32_t index) const;
        [[nodiscard]] ModelRange getModelRange(uint32_t index) const;
        [[nodiscard]] size_t getNumModels() const;

        [[nodiscard]] bool areBuffersBuilt() const;

        void destroy(VkDevice logicalDevice);

    private:
        [[nodiscard]] static ModelData getObjData(const std::string& filepath);

        std::vector<ModelData> modelData;
        reina::core::Buffer verticesBuffer;
        reina::core::Buffer offsetIndicesBuffer;
        reina::core::Buffer nonOffsetIndicesBuffer;
        reina::core::Buffer tbnsBuffer;
        reina::core::Buffer offsetTbnsIndicesBuffer;
        reina::core::Buffer texCoordsBuffer;
        reina::core::Buffer offsetTexIndicesBuffer;

        size_t verticesBufferSize = 0;

        bool builtBuffers = false;

        std::vector<float> allVertices = std::vector<float>(0);
        std::vector<float> allTBNs = std::vector<float>(0);
        std::vector<float> allTexCoords = std::vector<float>(0);
        std::vector<uint32_t> allIndicesOffset = std::vector<uint32_t>(0);
        std::vector<uint32_t> allTexIndicesOffset = std::vector<uint32_t>(0);
        std::vector<uint32_t> allTBNsIndicesOffset = std::vector<uint32_t>(0);
        std::vector<uint32_t> allIndicesNonOffset = std::vector<uint32_t>(0);

        std::vector<ModelRange> modelRanges;
    };
}

#endif //RAYGUN_VK_MODELS_H
