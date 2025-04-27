#ifndef REINA_VK_GLTFLOADER_H
#define REINA_VK_GLTFLOADER_H

#include "../Scene.h"

#include <string>
#include <fastgltf/core.hpp>

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
    bool loadMeshTBNs(fastgltf::Asset& asset, std::vector<MeshTBN>& outMeshes);
}

#endif  // REINA_VK_GLTFLOADER_H
