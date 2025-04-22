#include "Models.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <stdexcept>
#include <cmath>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

reina::scene::Models::Models(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::vector<std::string>& modelFilepaths) {
    for (const auto & modelFilepath : modelFilepaths) {
        ObjData objData = getObjData(modelFilepath);
        addModel(objData);
    }

    createBuffers(logicalDevice, physicalDevice, cmdPool, queue);
}

void reina::scene::Models::addModel(const reina::scene::ObjData& objData) {
    if (areBuffersCreated()) {
        throw std::runtime_error("Could not add model; buffers already created. Models must be added before buffers are created.");
    }

    modelObjData.push_back(objData);

    // The starting indices of the vertices/TBNs/indices for this model
    size_t vertexOffset = allVertices.size();
    size_t tbnsOffset = allTBNs.size();
    size_t texOffset = allTexCoords.size();
    size_t indexOffset = allIndicesOffset.size();
    size_t tbnsIndicesOffset = allTBNsIndicesOffset.size();
    size_t texIndexOffset = allTexIndicesOffset.size();

    allVertices.resize(allVertices.size() + objData.vertices.size());
    allTBNs.resize(allTBNs.size() + objData.tbns.size() * 9);
    allTexCoords.resize(allTexCoords.size() + objData.texCoords.size());
    allIndicesNonOffset.resize(allIndicesNonOffset.size() + objData.indices.size());
    allIndicesOffset.resize(allIndicesOffset.size() + objData.indices.size());
    allTBNsIndicesOffset.resize(allTBNsIndicesOffset.size() + objData.tbnsIndices.size());
    allTexIndicesOffset.resize(allTexIndicesOffset.size() + objData.texIndices.size());

    modelRanges.push_back(ModelRange{
        .firstVertex = static_cast<uint32_t>(vertexOffset / 4),
        .firstNormal = static_cast<uint32_t>(tbnsOffset),
        .indexOffset = static_cast<uint32_t>(indexOffset * sizeof(uint32_t)),
        .tbnsIndexOffset = static_cast<uint32_t>(tbnsIndicesOffset * sizeof(uint32_t)),
        .texIndexOffset = objData.texCoords.empty() ? static_cast<uint32_t>(-1) : static_cast<uint32_t>(texIndexOffset * sizeof(uint32_t)),
        .indexCount = static_cast<uint32_t>(objData.indices.size() / 3),
        .tbnsIndexCount = static_cast<uint32_t>(objData.indices.size() / 3),
        .texIndexCount = static_cast<uint32_t>(objData.texIndices.size() / 3),
    });

    // Copy vertices and tex coords
    std::copy(objData.vertices.begin(), objData.vertices.end(), allVertices.begin() + static_cast<long long>(vertexOffset));
    std::copy(objData.texCoords.begin(), objData.texCoords.end(), allTexCoords.begin() + static_cast<long long>(texOffset));

    // Copy TBN matrices
    for (size_t idx = 0; idx < objData.tbns.size(); idx++) {
        const glm::mat3& m = objData.tbns[idx];

        // Compute the start of the i‑th matrix within allTBNs:
        size_t base = tbnsOffset + idx * 9;

        // Column‑major: for each column, then each row:
        for (int col = 0; col < 3; col++) {
            for (int row = 0; row < 3; row++) {
                allTBNs[base + col * 3 + row] = m[col][row];
            }
        }
    }

    // Copy indices with proper offset
    for (uint32_t idx : objData.indices) {
        allIndicesOffset[indexOffset] = idx + (vertexOffset / 4);
        allIndicesNonOffset[indexOffset++] = idx;
    }

    for (uint32_t idx : objData.tbnsIndices) {
        allTBNsIndicesOffset[tbnsIndicesOffset++] = idx + tbnsOffset;
    }

    for (uint32_t idx : objData.texIndices) {
        allTexIndicesOffset[texIndexOffset++] = idx == 0xFFFFFFFFu ? idx : idx + (texOffset / 2);
    }
}

void reina::scene::Models::createBuffers(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue) {
    buffersCreated = true;

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

bool reina::scene::Models::areBuffersCreated() const {
    return buffersCreated;
}
