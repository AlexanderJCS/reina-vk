#ifndef REINA_VK_GLTFLOADER_H
#define REINA_VK_GLTFLOADER_H

#include "../Scene.h"

#include <string>
#include <fastgltf/core.hpp>
#include <unordered_map>

namespace reina::scene::gltf {
    struct VertexTBN {
        fastgltf::math::fvec3 position;
        fastgltf::math::fvec3 normal;
        fastgltf::math::fvec3 tangent;
        fastgltf::math::fvec3 bitangent;
        fastgltf::math::fvec2 uv;
    };

    struct MeshTBN {
        std::vector<VertexTBN> vertices;
        std::vector<uint32_t>  indices;

        reina::scene::ModelData toModelData();
    };

    fastgltf::Asset loadGltf(const std::string& filepath);
    void loadMeshTBNs(fastgltf::Asset& asset, std::vector<MeshTBN>& outMeshes);
    std::unordered_map<uint32_t, uint32_t> addMeshesToScene(reina::scene::Scene& scene, std::vector<reina::scene::gltf::MeshTBN> modelData);
    void addInstancesToScene(fastgltf::Asset& asset, reina::scene::Scene& scene, const std::unordered_map<uint32_t, uint32_t>& gltfIdToSceneId);
    reina::scene::Scene loadScene(const std::string& filepath);
}

#endif  // REINA_VK_GLTFLOADER_H
