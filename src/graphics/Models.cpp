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

reina::graphics::Models::Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::vector<std::string>& modelFilepaths) {
    modelRanges = std::vector<ModelRange>(modelFilepaths.size());
    modelObjData = std::vector<ObjData>(modelFilepaths.size());

    size_t totalVertices = 0;
    size_t totalIndices = 0;
    size_t totalNormals = 0;
    size_t totalTexCoords = 0;

    for (int i = 0; i < modelFilepaths.size(); i++) {
        modelObjData[i] = getObjData(modelFilepaths[i]);

        totalVertices += modelObjData[i].vertices.size();
        totalIndices += modelObjData[i].indices.size();
        totalNormals += modelObjData[i].normals.size();
        totalTexCoords += modelObjData[i].texCoords.size();
    }

    std::vector<float> allVertices(totalVertices);
    std::vector<float> allNormals(totalNormals);
    std::vector<float> allTexCoords(totalTexCoords);
    std::vector<uint32_t> allIndicesOffset(totalIndices);
    std::vector<uint32_t> allTexIndicesOffset(totalIndices);
    std::vector<uint32_t> allNormalsIndicesOffset(totalIndices);
    std::vector<uint32_t> allIndicesNonOffset(totalIndices);

    size_t vertexOffset = 0;
    size_t normalsOffset = 0;
    size_t texOffset = 0;
    size_t indexOffset = 0;
    size_t normalsIndexOffset = 0;
    size_t texIndexOffset = 0;

    // Copy the data to allVertices and allIndicesOffset
    for (int i = 0; i < modelObjData.size(); i++) {
        const ObjData& objectData = modelObjData[i];

        modelRanges[i] = ModelRange{
            .firstVertex = static_cast<uint32_t>(vertexOffset / 4),
            .firstNormal = static_cast<uint32_t>(normalsOffset / 4),
            .indexOffset = static_cast<uint32_t>(indexOffset * sizeof(uint32_t)),
            .normalsIndexOffset = static_cast<uint32_t>(normalsIndexOffset * sizeof(uint32_t)),
            .texIndexOffset = objectData.texCoords.empty() ? static_cast<uint32_t>(-1) : static_cast<uint32_t>(texIndexOffset * sizeof(uint32_t)),
            .indexCount = static_cast<uint32_t>(objectData.indices.size() / 3),
            .normalsIndexCount = static_cast<uint32_t>(objectData.indices.size() / 3),
            .texIndexCount = static_cast<uint32_t>(objectData.texIndices.size() / 3),
        };

        // Copy vertices
        std::copy(objectData.vertices.begin(), objectData.vertices.end(), allVertices.begin() + static_cast<long long>(vertexOffset));
        std::copy(objectData.normals.begin(), objectData.normals.end(), allNormals.begin() + static_cast<long long>(normalsOffset));
        std::copy(objectData.texCoords.begin(), objectData.texCoords.end(), allTexCoords.begin() + static_cast<long long>(texOffset));

        // Copy indices with proper offset
        for (uint32_t idx : objectData.indices) {
            allIndicesOffset[indexOffset] = idx + (vertexOffset / 4);
            allIndicesNonOffset[indexOffset++] = idx;
        }

        for (uint32_t idx : objectData.normalsIndices) {
            allNormalsIndicesOffset[normalsIndexOffset++] = idx + (normalsOffset / 4);
        }

        for (uint32_t idx : objectData.texIndices) {
            allTexIndicesOffset[texIndexOffset++] = idx == 0xFFFFFFFFu ? idx : idx + (texOffset / 2);
        }

        vertexOffset += objectData.vertices.size();
        normalsOffset += objectData.normals.size();
        texOffset += objectData.texCoords.size();
    }

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VkMemoryAllocateFlags allocFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    verticesBufferSize = allVertices.size();
    verticesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allVertices, usage, allocFlags};

    indicesBuffersSize = allIndicesOffset.size();
    offsetIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allIndicesOffset, usage, allocFlags};
    nonOffsetIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allIndicesNonOffset, usage, allocFlags};

    normalsBufferSize = allNormals.size();
    normalsBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allNormals, usage, allocFlags};

    normalsIndicesBufferSize = allNormalsIndicesOffset.size();
    offsetNormalsIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allNormalsIndicesOffset, usage, allocFlags};

    texCoordsBufferSize = allTexCoords.size();
    texCoordsBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTexCoords.empty() ? std::vector<float>{0} : allTexCoords, usage, allocFlags};

    texIndicesBufferSize = allTexIndicesOffset.size();
    offsetTexIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTexIndicesOffset, usage, allocFlags};
}

reina::graphics::ObjData reina::graphics::Models::getObjData(const std::string& filepath) {
    tinyobj::ObjReader reader;
    reader.ParseFromFile(filepath);

    if (!reader.Valid()) {
        throw std::runtime_error("Error reading OBJ:\n" + reader.Error());
    }

    std::vector<float> objVertices = tinyobjToVec4(reader.GetAttrib().GetVertices());
    std::vector<float> objNormals = tinyobjToVec4(reader.GetAttrib().normals);
    std::vector<float> objTexCoords = reader.GetAttrib().texcoords;

    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    if (shapes.size() != 1) {
        throw std::runtime_error("Several shapes to parse; need only one");
    }

    std::vector<uint32_t> objIndices(shapes[0].mesh.indices.size());
    std::vector<uint32_t> objNormalsIndices(shapes[0].mesh.indices.size());
    std::vector<uint32_t> objTexIndices(shapes[0].mesh.indices.size());
    for (int i = 0; i < shapes[0].mesh.indices.size(); i++) {
        const tinyobj::index_t& index = shapes[0].mesh.indices[i];

        objIndices[i] = index.vertex_index;
        objNormalsIndices[i] = index.normal_index;
        objTexIndices[i] = index.texcoord_index;
    }

    return {objVertices, objIndices, objNormals, objNormalsIndices, objTexCoords, objTexIndices};
}

size_t reina::graphics::Models::getVerticesBufferSize() const {
    return verticesBufferSize;
}

size_t reina::graphics::Models::getIndicesBuffersSize() const {
    return indicesBuffersSize;
}

const reina::core::Buffer& reina::graphics::Models::getVerticesBuffer() const {
    return verticesBuffer;
}

const reina::core::Buffer& reina::graphics::Models::getOffsetIndicesBuffer() const {
    return offsetIndicesBuffer;
}

const reina::core::Buffer& reina::graphics::Models::getNonOffsetIndicesBuffer() const {
    return nonOffsetIndicesBuffer;
}

const reina::graphics::ObjData& reina::graphics::Models::getObjData(int index) const {
    if (index >= modelRanges.size() || index < 0) {
        throw std::runtime_error("Index " + std::to_string(index) + " out of range for models");
    }

    return modelObjData[index];
}


reina::graphics::ModelRange reina::graphics::Models::getModelRange(int index) const {
    if (index >= modelRanges.size() || index < 0) {
        throw std::runtime_error("Index " + std::to_string(index) + " out of range for models");
    }

    return modelRanges[index];
}

void reina::graphics::Models::destroy(VkDevice logicalDevice) {
    verticesBuffer.destroy(logicalDevice);
    offsetIndicesBuffer.destroy(logicalDevice);
    nonOffsetIndicesBuffer.destroy(logicalDevice);
    offsetNormalsIndicesBuffer.destroy(logicalDevice);
    normalsBuffer.destroy(logicalDevice);
    texCoordsBuffer.destroy(logicalDevice);
    offsetTexIndicesBuffer.destroy(logicalDevice);
}

size_t reina::graphics::Models::getNormalsBufferSize() const {
    return normalsBufferSize;
}

size_t reina::graphics::Models::getNormalsIndicesBufferSize() const {
    return normalsIndicesBufferSize;
}

const reina::core::Buffer& reina::graphics::Models::getNormalsBuffer() const {
    return normalsBuffer;
}

const reina::core::Buffer& reina::graphics::Models::getOffsetNormalsIndicesBuffer() const {
    return offsetNormalsIndicesBuffer;
}

const reina::core::Buffer &reina::graphics::Models::getOffsetTexIndicesBuffer() const {
    return offsetTexIndicesBuffer;
}

size_t reina::graphics::Models::getTexIndicesBufferSize() const {
    return texIndicesBufferSize;
}

const reina::core::Buffer &reina::graphics::Models::getTexCoordsBuffer() const {
    return texCoordsBuffer;
}

size_t reina::graphics::Models::getTexCoordsBufferSize() const {
    return texCoordsBufferSize;
}
