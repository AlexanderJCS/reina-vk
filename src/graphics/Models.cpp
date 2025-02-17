#include "Models.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <stdexcept>
#include <cmath>

rt::graphics::Models::Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<std::string>& modelFilepaths) {
    modelRanges = std::vector<ModelRange>(modelFilepaths.size());
    std::vector<ObjData> allObjectsData(modelFilepaths.size());
    size_t totalVertices = 0;
    size_t totalIndices = 0;

    for (int i = 0; i < modelFilepaths.size(); i++) {
        allObjectsData[i] = getObjData(modelFilepaths[i]);

        totalVertices += allObjectsData[i].vertices.size();
        totalIndices += allObjectsData[i].indices.size();
    }

    std::vector<float> allVertices(totalVertices);
    std::vector<uint32_t> allIndicesOffset(totalIndices);
    std::vector<uint32_t> allIndicesNonOffset(totalIndices);

    size_t vertexOffset = 0;
    size_t indexOffset = 0;

    // Copy the data to allVertices and allIndicesOffset
    for (int i = 0; i < allObjectsData.size(); i++) {
        const ObjData& objectData = allObjectsData[i];

        modelRanges[i] = ModelRange{
                .firstVertex = static_cast<uint32_t>(vertexOffset / 4),
                .indexOffset = static_cast<uint32_t>(indexOffset * sizeof(uint32_t)),
                .indexCount  = static_cast<uint32_t>(objectData.indices.size() / 3)
        };

        // Copy vertices
        std::copy(objectData.vertices.begin(), objectData.vertices.end(), allVertices.begin() + vertexOffset);

        // Copy indices with proper offset
        for (uint32_t idx : objectData.indices) {
            allIndicesOffset[indexOffset] = idx + (vertexOffset / 4);
            allIndicesNonOffset[indexOffset++] = idx;
        }

        vertexOffset += objectData.vertices.size();
    }

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    verticesBufferSize = allVertices.size();
    verticesBuffer = rt::core::Buffer{
            logicalDevice,
            physicalDevice,
            allVertices,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    indicesBuffersSize = allIndicesOffset.size();
    offsetIndicesBuffer = rt::core::Buffer{
            logicalDevice,
            physicalDevice,
            allIndicesOffset,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    nonOffsetIndicesBuffer = rt::core::Buffer{
            logicalDevice,
            physicalDevice,
            allIndicesNonOffset,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
}

rt::graphics::ObjData rt::graphics::Models::getObjData(const std::string& filepath) {
    tinyobj::ObjReader reader;
    reader.ParseFromFile(filepath);

    if (!reader.Valid()) {
        throw std::runtime_error("Error reading OBJ:\n" + reader.Error());
    }

    std::vector<tinyobj::real_t> tinyobjVertices = reader.GetAttrib().GetVertices();
    // convert from 3 floats per vertex to 4 floats per vertex. std::round removes any potential floating-point inaccuracies
    std::vector<float> objVertices(static_cast<int>(std::round(static_cast<double>(tinyobjVertices.size()) * 4.0/3)));

    for (int vertex = 0; vertex < tinyobjVertices.size() / 3; vertex++) {
        objVertices[vertex * 4] = tinyobjVertices[vertex * 3];
        objVertices[vertex * 4 + 1] = tinyobjVertices[vertex * 3 + 1];
        objVertices[vertex * 4 + 2] = tinyobjVertices[vertex * 3 + 2];
        objVertices[vertex * 4 + 3] = 0;
    }

    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    if (shapes.size() != 1) {
        throw std::runtime_error("Several shapes to parse; need only one");
    }

    std::vector<uint32_t> objIndices(shapes[0].mesh.indices.size());
    for (int i = 0; i < shapes[0].mesh.indices.size(); i++) {
        objIndices[i] = shapes[0].mesh.indices[i].vertex_index;
    }

    return {objVertices, objIndices};
}

size_t rt::graphics::Models::getVerticesBufferSize() const {
    return verticesBufferSize;
}

size_t rt::graphics::Models::getIndicesBuffersSize() const {
    return indicesBuffersSize;
}

const rt::core::Buffer& rt::graphics::Models::getVerticesBuffer() const {
    return verticesBuffer.value();
}

const rt::core::Buffer& rt::graphics::Models::getOffsetIndicesBuffer() const {
    return offsetIndicesBuffer.value();
}

const rt::core::Buffer& rt::graphics::Models::getNonOffsetIndicesBuffer() const {
    return nonOffsetIndicesBuffer.value();
}

rt::graphics::ModelRange rt::graphics::Models::getModelRange(int index) const {
    // todo: do input validation
    return modelRanges[index];
}

void rt::graphics::Models::destroy(VkDevice logicalDevice) {
    if (verticesBuffer.has_value()) {
        verticesBuffer.value().destroy(logicalDevice);
    } if (offsetIndicesBuffer.has_value()) {
        offsetIndicesBuffer.value().destroy(logicalDevice);
    } if (nonOffsetIndicesBuffer.has_value()) {
        nonOffsetIndicesBuffer.value().destroy(logicalDevice);
    }
}
