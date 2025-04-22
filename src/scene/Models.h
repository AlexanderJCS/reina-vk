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

    struct ObjData {
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
        Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::vector<std::string>& modelFilepaths);

        void addModel(const ObjData& objData);
        void createBuffers(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue);

        [[nodiscard]] size_t getVerticesBufferSize() const;

        [[nodiscard]] const reina::core::Buffer& getVerticesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getOffsetIndicesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getNonOffsetIndicesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getTbnsBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getOffsetTbnsIndicesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getTexCoordsBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getOffsetTexIndicesBuffer() const;

        [[nodiscard]] const ObjData& getObjData(int index) const;
        [[nodiscard]] ModelRange getModelRange(int index) const;
        [[nodiscard]] bool areBuffersCreated() const;

        void destroy(VkDevice logicalDevice);

    private:
        [[nodiscard]] static ObjData getObjData(const std::string& filepath);

        bool buffersCreated = false;

        std::vector<ObjData> modelObjData;
        reina::core::Buffer verticesBuffer;
        reina::core::Buffer offsetIndicesBuffer;
        reina::core::Buffer nonOffsetIndicesBuffer;
        reina::core::Buffer tbnsBuffer;
        reina::core::Buffer offsetTbnsIndicesBuffer;
        reina::core::Buffer texCoordsBuffer;
        reina::core::Buffer offsetTexIndicesBuffer;

        size_t verticesBufferSize = 0;
        size_t indicesBuffersSize = 0;
        size_t normalsBufferSize = 0;
        size_t normalsIndicesBufferSize = 0;
        size_t texCoordsBufferSize = 0;
        size_t texIndicesBufferSize = 0;

        std::vector<float> allVertices;
        std::vector<float> allTBNs;
        std::vector<float> allTexCoords;
        std::vector<uint32_t> allIndicesOffset;
        std::vector<uint32_t> allTexIndicesOffset;
        std::vector<uint32_t> allTBNsIndicesOffset;
        std::vector<uint32_t> allIndicesNonOffset;

        std::vector<ModelRange> modelRanges;
    };
}

#endif //RAYGUN_VK_MODELS_H
