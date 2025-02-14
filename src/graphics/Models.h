#ifndef RAYGUN_VK_MODELS_H
#define RAYGUN_VK_MODELS_H

#include <vector>
#include <string>
#include <optional>

#include "../core/Buffer.h"

namespace rt::graphics {
    struct ModelRange {
        uint32_t firstVertex;
        uint32_t indexOffset;
        uint32_t indexCount;
    };

    struct ObjData {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
    };

    class Models {
    public:
        Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<std::string>& modelFilepaths);

        [[nodiscard]] size_t getVerticesBufferSize() const;
        [[nodiscard]] size_t getIndicesBufferSize() const;

        [[nodiscard]] const rt::core::Buffer& getVerticesBuffer() const;
        [[nodiscard]] const rt::core::Buffer& getIndicesBuffer() const;

        [[nodiscard]] ModelRange getModelRange(int index) const;

        void destroy(VkDevice logicalDevice);

    private:
        static ObjData getObjData(const std::string& filepath);

        std::optional<rt::core::Buffer> verticesBuffer;
        std::optional<rt::core::Buffer> indicesBuffer;

        size_t verticesBufferSize;
        size_t indicesBufferSize;

        std::vector<ModelRange> modelData;
    };
}

#endif //RAYGUN_VK_MODELS_H
