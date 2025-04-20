#include "Models.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <stdexcept>
#include <cmath>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

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

reina::scene::Models::Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::vector<std::string>& modelFilepaths) {
    modelRanges = std::vector<ModelRange>(modelFilepaths.size());
    modelObjData = std::vector<ObjData>(modelFilepaths.size());

    size_t totalVertices = 0;
    size_t totalIndices = 0;
    size_t totalTBNs = 0;
    size_t totalTexCoords = 0;

    for (int i = 0; i < modelFilepaths.size(); i++) {
        modelObjData[i] = getObjData(modelFilepaths[i]);

        totalVertices += modelObjData[i].vertices.size();
        totalIndices += modelObjData[i].indices.size();
        totalTBNs += modelObjData[i].tbns.size();
        totalTexCoords += modelObjData[i].texCoords.size();
    }

    std::vector<float> allVertices(totalVertices);
    std::vector<float> allTBNs(totalTBNs * 9);
    std::vector<float> allTexCoords(totalTexCoords);
    std::vector<uint32_t> allIndicesOffset(totalIndices);
    std::vector<uint32_t> allTexIndicesOffset(totalIndices);
    std::vector<uint32_t> allTBNsIndicesOffset(totalIndices);
    std::vector<uint32_t> allIndicesNonOffset(totalIndices);

    size_t vertexOffset = 0;
    size_t tbnsOffset = 0;
    size_t texOffset = 0;
    size_t indexOffset = 0;
    size_t normalsIndexOffset = 0;
    size_t texIndexOffset = 0;

    // Copy the data to allVertices and allIndicesOffset
    for (int i = 0; i < modelObjData.size(); i++) {
        const ObjData& objectData = modelObjData[i];

        modelRanges[i] = ModelRange{
            .firstVertex = static_cast<uint32_t>(vertexOffset / 4),
            .firstNormal = static_cast<uint32_t>(tbnsOffset),
            .indexOffset = static_cast<uint32_t>(indexOffset * sizeof(uint32_t)),
            .tbnsIndexOffset = static_cast<uint32_t>(normalsIndexOffset * sizeof(uint32_t)),
            .texIndexOffset = objectData.texCoords.empty() ? static_cast<uint32_t>(-1) : static_cast<uint32_t>(texIndexOffset * sizeof(uint32_t)),
            .indexCount = static_cast<uint32_t>(objectData.indices.size() / 3),
            .tbnsIndexCount = static_cast<uint32_t>(objectData.indices.size() / 3),
            .texIndexCount = static_cast<uint32_t>(objectData.texIndices.size() / 3),
        };

        // Copy vertices
        std::copy(objectData.vertices.begin(), objectData.vertices.end(), allVertices.begin() + static_cast<long long>(vertexOffset));
//        std::copy(objectData.tbns.begin(), objectData.tbns.end(), allTBNs.begin() + static_cast<long long>(tbnsOffset));
        std::copy(objectData.texCoords.begin(), objectData.texCoords.end(), allTexCoords.begin() + static_cast<long long>(texOffset));

        for (size_t idx = 0; idx < objectData.tbns.size(); idx++) {
            const glm::mat3 &m = objectData.tbns[idx];

            // Compute the start of the i‑th matrix within allTBNs:
            size_t base = (tbnsOffset + idx) * 9;

            // Column‑major: for each column, then each row:
            for (int col = 0; col < 3; col++) {
                for (int row = 0; row < 3; row++) {
                    allTBNs[base + col * 3 + row] = m[col][row];
                }
            }
        }

        // Copy indices with proper offset
        for (uint32_t idx : objectData.indices) {
            allIndicesOffset[indexOffset] = idx + (vertexOffset / 4);
            allIndicesNonOffset[indexOffset++] = idx;
        }

        for (uint32_t idx : objectData.tbnsIndices) {
            allTBNsIndicesOffset[normalsIndexOffset++] = idx + tbnsOffset;
        }

        for (uint32_t idx : objectData.texIndices) {
            allTexIndicesOffset[texIndexOffset++] = idx == 0xFFFFFFFFu ? idx : idx + (texOffset / 2);
        }

        vertexOffset += objectData.vertices.size();
        tbnsOffset += objectData.tbns.size();
        texOffset += objectData.texCoords.size();
    }

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VkMemoryAllocateFlags allocFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    verticesBufferSize = allVertices.size();
    verticesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allVertices, usage, allocFlags};

    indicesBuffersSize = allIndicesOffset.size();
    offsetIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allIndicesOffset, usage, allocFlags};
    nonOffsetIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allIndicesNonOffset, usage, allocFlags};

    normalsBufferSize = allTBNs.size();
    tbnsBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTBNs, usage, allocFlags};

    normalsIndicesBufferSize = allTBNsIndicesOffset.size();
    offsetTbnsIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTBNsIndicesOffset, usage, allocFlags};

    texCoordsBufferSize = allTexCoords.size();
    texCoordsBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTexCoords.empty() ? std::vector<float>{0} : allTexCoords, usage, allocFlags};

    texIndicesBufferSize = allTexIndicesOffset.size();
    offsetTexIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTexIndicesOffset, usage, allocFlags};
}

reina::scene::ObjData reina::scene::Models::getObjData(const std::string& filepath) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace | aiProcess_FlipUVs);
    if (!scene || !scene->HasMeshes()) {
        throw std::runtime_error("Failed to load model with Assimp: " + std::string(importer.GetErrorString()));
    }

    const aiMesh* mesh = scene->mMeshes[0];

    std::vector<float> objVertices;
    std::vector<uint32_t> objIndices;
    std::vector<glm::mat3> objTbns;
    std::vector<uint32_t> objNormalsIndices;
    std::vector<float> objTexCoords;
    std::vector<uint32_t> objTexIndices;

    for (size_t i = 0; i < mesh->mNumVertices; i++) {
        const aiVector3D& v = mesh->mVertices[i];
        objVertices.push_back(v.x);
        objVertices.push_back(v.y);
        objVertices.push_back(v.z);
        objVertices.push_back(1.0f);

        if (mesh->HasNormals()) {
            const aiVector3D& n = mesh->mNormals[i];
            glm::vec3 normalVec(n.x, n.y, n.z);
            glm::vec3 tangentVec(0, 0, 0);
            glm::vec3 bitangentVec(0, 0, 0);

            if (mesh->HasTangentsAndBitangents()) {
                const aiVector3D t = mesh->mTangents[i];
                const aiVector3D b = mesh->mBitangents[i];
                tangentVec = glm::vec3(t.x, t.y, t.z);
                bitangentVec = glm::vec3(b.x, b.y, b.z);
            }

            objTbns.emplace_back(tangentVec, bitangentVec, normalVec);
        }

        if (mesh->HasTextureCoords(0)) {
            const aiVector3D& t = mesh->mTextureCoords[0][i];
            objTexCoords.push_back(t.x);
            objTexCoords.push_back(t.y);
        }
    }

    for (size_t i = 0; i < mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; j++) {
            uint32_t idx = face.mIndices[j];
            objIndices.push_back(idx);
            objNormalsIndices.push_back(idx);
            objTexIndices.push_back(idx);
        }
    }

    return {objVertices, objIndices, objTbns, objNormalsIndices, objTexCoords, objTexIndices};
}

size_t reina::scene::Models::getVerticesBufferSize() const {
    return verticesBufferSize;
}

size_t reina::scene::Models::getIndicesBuffersSize() const {
    return indicesBuffersSize;
}

const reina::core::Buffer& reina::scene::Models::getVerticesBuffer() const {
    return verticesBuffer;
}

const reina::core::Buffer& reina::scene::Models::getOffsetIndicesBuffer() const {
    return offsetIndicesBuffer;
}

const reina::core::Buffer& reina::scene::Models::getNonOffsetIndicesBuffer() const {
    return nonOffsetIndicesBuffer;
}

const reina::scene::ObjData& reina::scene::Models::getObjData(int index) const {
    if (index >= modelRanges.size() || index < 0) {
        throw std::runtime_error("Index " + std::to_string(index) + " out of range for models");
    }

    return modelObjData[index];
}


reina::scene::ModelRange reina::scene::Models::getModelRange(int index) const {
    if (index >= modelRanges.size() || index < 0) {
        throw std::runtime_error("Index " + std::to_string(index) + " out of range for models");
    }

    return modelRanges[index];
}

void reina::scene::Models::destroy(VkDevice logicalDevice) {
    verticesBuffer.destroy(logicalDevice);
    offsetIndicesBuffer.destroy(logicalDevice);
    nonOffsetIndicesBuffer.destroy(logicalDevice);
    offsetTbnsIndicesBuffer.destroy(logicalDevice);
    tbnsBuffer.destroy(logicalDevice);
    texCoordsBuffer.destroy(logicalDevice);
    offsetTexIndicesBuffer.destroy(logicalDevice);
}

size_t reina::scene::Models::getNormalsBufferSize() const {
    return normalsBufferSize;
}

size_t reina::scene::Models::getNormalsIndicesBufferSize() const {
    return normalsIndicesBufferSize;
}

const reina::core::Buffer& reina::scene::Models::getTbnsBuffer() const {
    return tbnsBuffer;
}

const reina::core::Buffer& reina::scene::Models::getOffsetTbnsIndicesBuffer() const {
    return offsetTbnsIndicesBuffer;
}

const reina::core::Buffer &reina::scene::Models::getOffsetTexIndicesBuffer() const {
    return offsetTexIndicesBuffer;
}

size_t reina::scene::Models::getTexIndicesBufferSize() const {
    return texIndicesBufferSize;
}

const reina::core::Buffer &reina::scene::Models::getTexCoordsBuffer() const {
    return texCoordsBuffer;
}

size_t reina::scene::Models::getTexCoordsBufferSize() const {
    return texCoordsBufferSize;
}
