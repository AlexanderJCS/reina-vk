#ifndef RAYGUN_VK_MODELS_H
#define RAYGUN_VK_MODELS_H

#include <vector>
#include <string>
#include <optional>

#include "../core/Buffer.h"

namespace reina::graphics {
    struct ModelRange {
        uint32_t firstVertex;
        uint32_t firstNormal;
        uint32_t indexOffset;
        uint32_t normalsIndexOffset;
        uint32_t indexCount;
        uint32_t normalsIndexCount;
    };

    struct ObjData {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        std::vector<float> normals;
        std::vector<uint32_t> normalsIndices;
    };

    class Models {
    public:
        Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<std::string>& modelFilepaths);

        [[nodiscard]] size_t getVerticesBufferSize() const;
        [[nodiscard]] size_t getIndicesBuffersSize() const;

        [[nodiscard]] const reina::core::Buffer& getVerticesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getOffsetIndicesBuffer() const;
        [[nodiscard]] const reina::core::Buffer& getNonOffsetIndicesBuffer() const;

        [[nodiscard]] ModelRange getModelRange(int index) const;

        void destroy(VkDevice logicalDevice);

    private:
        [[nodiscard]] static ObjData getObjData(const std::string& filepath);

        std::optional<reina::core::Buffer> verticesBuffer;
        std::optional<reina::core::Buffer> offsetIndicesBuffer;
        std::optional<reina::core::Buffer> nonOffsetIndicesBuffer;
        std::optional<reina::core::Buffer> normalsBuffer;
        std::optional<reina::core::Buffer> offsetNormalsIndicesBuffer;

        size_t verticesBufferSize;
        size_t indicesBuffersSize;
        size_t normalsBufferSize;
        size_t offsetNormalsIndicesSize;

        std::vector<ModelRange> modelRanges;
    };
}

#endif //RAYGUN_VK_MODELS_H
