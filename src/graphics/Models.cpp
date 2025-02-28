#include "Models.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <stdexcept>
#include <cmath>

/**
 * Convert from 3 floats to vertex to 4 floats per vertex to more easily upload to the GPU.
 * @param tinyobjArr The array of tinyobj data
 * @return The array with 4 floats per vertex
 */
static std::vector<float> tinyobjToVec4(const std::vector<tinyobj::real_t>& tinyobjArr) {
    // convert from 3 floats per vertex to 4 floats per vertex. std::round removes any potential floating-point inaccuracies
    std::vector<float> output(static_cast<int>(std::round(static_cast<double>(tinyobjArr.size()) * 4.0/3)));

    for (int i = 0; i < tinyobjArr.size() / 3; i++) {
        output[i * 4] = tinyobjArr[i * 3];
        output[i * 4 + 1] = tinyobjArr[i * 3 + 1];
        output[i * 4 + 2] = tinyobjArr[i * 3 + 2];
        output[i * 4 + 3] = 0;
    }

    return output;
}

reina::graphics::Models::Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, const std::vector<std::string>& modelFilepaths) {
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
        std::copy(objectData.vertices.begin(), objectData.vertices.end(), allVertices.begin() + static_cast<long long>(vertexOffset));

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
    verticesBuffer = reina::core::Buffer{
            logicalDevice,
            physicalDevice,
            allVertices,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };

    indicesBuffersSize = allIndicesOffset.size();
    offsetIndicesBuffer = reina::core::Buffer{
            logicalDevice,
            physicalDevice,
            allIndicesOffset,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
    nonOffsetIndicesBuffer = reina::core::Buffer{
            logicalDevice,
            physicalDevice,
            allIndicesNonOffset,
            usage,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    };
}

reina::graphics::ObjData reina::graphics::Models::getObjData(const std::string& filepath) {
    tinyobj::ObjReader reader;
    reader.ParseFromFile(filepath);

    if (!reader.Valid()) {
        throw std::runtime_error("Error reading OBJ:\n" + reader.Error());
    }

    std::vector<float> objVertices = tinyobjToVec4(reader.GetAttrib().GetVertices());
    std::vector<float> objNormals = tinyobjToVec4(reader.GetAttrib().normals);

    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    if (shapes.size() != 1) {
        throw std::runtime_error("Several shapes to parse; need only one");
    }

    std::vector<uint32_t> objIndices(shapes[0].mesh.indices.size());
    std::vector<uint32_t> objNormalsIndices(shapes[0].mesh.indices.size());
    for (int i = 0; i < shapes[0].mesh.indices.size(); i++) {
        objIndices[i] = shapes[0].mesh.indices[i].vertex_index;
        objNormalsIndices[i] = shapes[0].mesh.indices[i].normal_index;
    }

    return {objVertices, objIndices, objNormals, objNormalsIndices};
}

size_t reina::graphics::Models::getVerticesBufferSize() const {
    return verticesBufferSize;
}

size_t reina::graphics::Models::getIndicesBuffersSize() const {
    return indicesBuffersSize;
}

const reina::core::Buffer& reina::graphics::Models::getVerticesBuffer() const {
    return verticesBuffer.value();
}

const reina::core::Buffer& reina::graphics::Models::getOffsetIndicesBuffer() const {
    return offsetIndicesBuffer.value();
}

const reina::core::Buffer& reina::graphics::Models::getNonOffsetIndicesBuffer() const {
    return nonOffsetIndicesBuffer.value();
}

reina::graphics::ModelRange reina::graphics::Models::getModelRange(int index) const {
    if (index >= modelRanges.size() || index < 0) {
        throw std::runtime_error("Index " + std::to_string(index) + " out of range for models");
    }

    return modelRanges[index];
}

void reina::graphics::Models::destroy(VkDevice logicalDevice) {
    if (verticesBuffer.has_value()) {
        verticesBuffer.value().destroy(logicalDevice);
    } if (offsetIndicesBuffer.has_value()) {
        offsetIndicesBuffer.value().destroy(logicalDevice);
    } if (nonOffsetIndicesBuffer.has_value()) {
        nonOffsetIndicesBuffer.value().destroy(logicalDevice);
    }
}
