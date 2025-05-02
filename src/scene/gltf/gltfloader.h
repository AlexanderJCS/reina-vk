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

    struct Primitive {
        std::vector<VertexTBN> vertices;
        std::vector<uint32_t> indices;
        int materialIdx;

        [[nodiscard]] reina::scene::ModelData toModelData() const;
    };

    fastgltf::Asset loadGltf(const std::string& filepath);

    std::unordered_map<uint32_t, std::vector<reina::scene::gltf::Primitive>> loadPrimitives(fastgltf::Asset& asset);

    std::unordered_map<uint32_t, std::vector<uint32_t>> addMeshesToScene(
            reina::scene::Scene& scene,
            const std::unordered_map<uint32_t,
            std::vector<reina::scene::gltf::Primitive>>& meshIdToPrimitive
            );

    void addInstancesToScene(
            fastgltf::Asset &asset, reina::scene::Scene& scene,
            const std::unordered_map<uint32_t, std::vector<uint32_t>>& gltfIdToSceneId,
            const std::unordered_map<uint32_t, std::vector<reina::scene::Material>>& gltfModelIdToMaterials
            );

    std::unordered_map<uint32_t, std::vector<reina::scene::Material>> materialsFromMeshTBNs(
            fastgltf::Asset& asset,
            const std::unordered_map<uint32_t, std::vector<reina::scene::gltf::Primitive>>& meshIdToPrimitives,
            std::unordered_map<uint32_t, uint32_t> gltfTexIdToSceneId
            );

    reina::scene::Scene loadScene(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool cmdPool, VkQueue queue, const std::string& filepath);
}

#endif  // REINA_VK_GLTFLOADER_H
