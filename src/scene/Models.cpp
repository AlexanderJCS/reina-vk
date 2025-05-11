#include "Models.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <stdexcept>
#include <cmath>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>

reina::scene::Models::Models(const std::vector<std::string>& modelFilepaths) {
    // Copy the data to allVertices and allIndicesOffset
    for (const std::string& filepath : modelFilepaths) {
        addModel(filepath);
    }
}

uint32_t reina::scene::Models::addModel(const std::string& filepath) {
    return addModel(getObjData(filepath));
}

uint32_t reina::scene::Models::addModel(const reina::scene::ModelData& objData) {
    if (areBuffersBuilt()) {
        throw std::runtime_error("Could not add model; buffers are already built");
    }

    size_t vertexOffset = allVertices.size();
    size_t tbnsOffset = allTBNs.size() / 9;
    size_t texOffset = allTexCoords.size();
    size_t indexOffset = allIndicesOffset.size();
    size_t tbnsIndicesOffset = allTBNsIndicesOffset.size();
    size_t texIndexOffset = allTexIndicesOffset.size();

    allVertices.resize(allVertices.size() + objData.vertices.size());
    allTBNs.resize(allTBNs.size() + objData.tbns.size() * 9);
    allTexCoords.resize(allTexCoords.size() + objData.texCoords.size());
    allIndicesOffset.resize(allIndicesOffset.size() + objData.indices.size());
    allTexIndicesOffset.resize(allTexIndicesOffset.size() + objData.texIndices.size());
    allTBNsIndicesOffset.resize(allTBNsIndicesOffset.size() + objData.tbnsIndices.size());
    allIndicesNonOffset.resize(allIndicesNonOffset.size() + objData.indices.size());

    const ModelData& objectData = objData;

    modelRanges.push_back(ModelRange{
            .firstVertex = static_cast<uint32_t>(vertexOffset / 4),
            .firstNormal = static_cast<uint32_t>(tbnsOffset),
            .indexOffset = static_cast<uint32_t>(indexOffset),
            .tbnsIndexOffset = static_cast<uint32_t>(tbnsIndicesOffset),
            .texIndexOffset = objectData.texCoords.empty() ? static_cast<uint32_t>(-1) : static_cast<uint32_t>(texIndexOffset),
            .indexCount = static_cast<uint32_t>(objectData.indices.size() / 3),
            .tbnsIndexCount = static_cast<uint32_t>(objectData.indices.size() / 3),
            .texIndexCount = static_cast<uint32_t>(objectData.texIndices.size() / 3),
    });

    modelData.push_back(objData);

    // Copy vertices
    std::copy(objectData.vertices.begin(), objectData.vertices.end(), allVertices.begin() + static_cast<long long>(vertexOffset));
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
        allTBNsIndicesOffset[tbnsIndicesOffset++] = idx + tbnsOffset;
    }

    for (uint32_t idx : objectData.texIndices) {
        allTexIndicesOffset[texIndexOffset++] = idx == 0xFFFFFFFFu ? idx : idx + (texOffset / 2);
    }

    return static_cast<uint32_t>(modelRanges.size() - 1);
}

void reina::scene::Models::buildBuffers(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue) {
    if (areBuffersBuilt()) {
        throw std::runtime_error("Cannot call buildBuffers more than once");
    }

    builtBuffers = true;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VkMemoryAllocateFlags allocFlags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    verticesBufferSize = allVertices.size();
    verticesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allVertices, usage, allocFlags};
    offsetIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allIndicesOffset, usage, allocFlags};
    nonOffsetIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allIndicesNonOffset, usage, allocFlags};
    tbnsBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTBNs, usage, allocFlags};
    offsetTbnsIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTBNsIndicesOffset, usage, allocFlags};
    texCoordsBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTexCoords.empty() ? std::vector<float>{0} : allTexCoords, usage, allocFlags};
    offsetTexIndicesBuffer = reina::core::Buffer{logicalDevice, physicalDevice, cmdPool, queue, allTexIndicesOffset, usage, allocFlags};
}

reina::scene::ModelData reina::scene::Models::getObjData(const std::string& filepath) {
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

const reina::core::Buffer& reina::scene::Models::getVerticesBuffer() const {
    return verticesBuffer;
}

const reina::core::Buffer& reina::scene::Models::getOffsetIndicesBuffer() const {
    return offsetIndicesBuffer;
}

const reina::core::Buffer& reina::scene::Models::getNonOffsetIndicesBuffer() const {
    return nonOffsetIndicesBuffer;
}

const reina::scene::ModelData& reina::scene::Models::getModelData(uint32_t index) const {
    if (index >= modelRanges.size()) {
        throw std::runtime_error("Index " + std::to_string(index) + " out of range for models");
    }

    return modelData[index];
}


reina::scene::ModelRange reina::scene::Models::getModelRange(uint32_t index) const {
    if (index >= modelRanges.size()) {
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

const reina::core::Buffer& reina::scene::Models::getTbnsBuffer() const {
    return tbnsBuffer;
}

const reina::core::Buffer& reina::scene::Models::getOffsetTbnsIndicesBuffer() const {
    return offsetTbnsIndicesBuffer;
}

const reina::core::Buffer &reina::scene::Models::getOffsetTexIndicesBuffer() const {
    return offsetTexIndicesBuffer;
}

const reina::core::Buffer &reina::scene::Models::getTexCoordsBuffer() const {
    return texCoordsBuffer;
}

size_t reina::scene::Models::getNumModels() const {
    return modelRanges.size();
}

bool reina::scene::Models::areBuffersBuilt() const {
    return builtBuffers;
}
