#include "gltfloader.h"
#include "fastgltf/tools.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <unordered_set>

#include <fastgltf/core.hpp>
#include <iostream>

fastgltf::Asset reina::scene::gltf::loadGltf(const std::string& filepath) {
    namespace fg = fastgltf;
    const std::filesystem::path path{filepath};

    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Failed to find glTF file: " + filepath);
    }

    // 1) Map the file into memory
    auto bufferOrErr = fg::MappedGltfFile::FromPath(path);
    if (!bufferOrErr) {
        throw std::runtime_error(
                "Failed to open glTF file: " + filepath + "\nError: " +
                std::string(fg::getErrorMessage(bufferOrErr.error()))
        );
    }

    // 2) Set up parser & options
    static constexpr auto supportedExts =
            fg::Extensions::KHR_mesh_quantization   |
            fg::Extensions::KHR_texture_transform  |
            fg::Extensions::KHR_materials_variants;

    constexpr auto loadOpts =
            fg::Options::DontRequireValidAssetMember |
            fg::Options::AllowDouble              |
            fg::Options::LoadExternalBuffers      |
            fg::Options::LoadExternalImages       |
            fg::Options::GenerateMeshIndices;

    fg::Parser parser(supportedExts);

    // 3) Auto-detect file type and parse
    auto fileType = fg::determineGltfFileType(bufferOrErr.get());
    auto assetOrErr =
            (fileType == fg::GltfType::GLB)
            ? parser.loadGltfBinary(bufferOrErr.get(), path.parent_path(), loadOpts)
            : parser.loadGltf     (bufferOrErr.get(), path.parent_path(), loadOpts);

    if (assetOrErr.error() != fg::Error::None) {
        throw std::runtime_error(
                "Failed to parse glTF: " +
                std::string(fg::getErrorMessage(assetOrErr.error()))
        );
    }

    // 4) Return the fully‐loaded Asset
    return std::move(assetOrErr.get());
}

bool reina::scene::gltf::loadMeshTBNs(fastgltf::Asset& asset, std::vector<MeshTBN>& outMeshes) {
    // — Gather meshes used by default scene
    std::unordered_set<size_t> used;
    size_t sceneIdx = asset.defaultScene.value_or(0);                  // defaultScene :contentReference[oaicite:5]{index=5}
    fastgltf::iterateSceneNodes(asset, sceneIdx, fastgltf::math::fmat4x4(1.0f),
                                [&](fastgltf::Node& node, const fastgltf::math::fmat4x4&) {
                                    if (node.meshIndex.has_value()) used.insert(*node.meshIndex);
                                });

    // — For each used mesh → each primitive
    for (auto mi : used) {
        const auto& mesh = asset.meshes[mi];
        for (const auto& prim : mesh.primitives) {
            MeshTBN m;
            // — POSITION
            if (auto a = prim.findAttribute("POSITION")) {
                const auto& acc = asset.accessors[a->accessorIndex];
                m.vertices.resize(acc.count);
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, acc,
                        [&](const fastgltf::math::fvec3& p, std::size_t i) {
                            m.vertices[i].position = p;
                        }
                );
            }
            // — NORMAL
            if (auto a = prim.findAttribute("NORMAL")) {
                const auto& acc = asset.accessors[a->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, acc,
                        [&](fastgltf::math::fvec3 n, size_t i) {
                            m.vertices[i].normal = n;
                        });
            }
            // — TANGENT: may be Vec4 (x,y,z + w sign) or fallback Vec3
            if (auto tanIt = prim.findAttribute("TANGENT"); tanIt != prim.attributes.end()) {
                const auto& acc = asset.accessors[tanIt->accessorIndex];

                // Vec4 case: apply w as bitangent sign
                if (acc.type == fastgltf::AccessorType::Vec4) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                            asset, acc,
                            [&](const fastgltf::math::fvec4& t, std::size_t i) {
                                // tangent.xyz
                                m.vertices[i].tangent  = { t.x(), t.y(), t.z() };
                                // bitangent = cross(normal, tangent) * w
                                m.vertices[i].bitangent =
                                        fastgltf::math::cross(m.vertices[i].normal,
                                                              m.vertices[i].tangent)
                                        * t.w();
                            }
                    );
                }
                    // Vec3 fallback: no handedness, assume w == +1
                else if (acc.type == fastgltf::AccessorType::Vec3) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                            asset, acc,
                            [&](const fastgltf::math::fvec3& t, std::size_t i) {
                                m.vertices[i].tangent   = t;
                                // bitangent = cross(normal, tangent)
                                m.vertices[i].bitangent =
                                        fastgltf::math::cross(m.vertices[i].normal,
                                                              m.vertices[i].tangent);
                            }
                    );
                }
                else {
                    // Unexpected accessor type—either skip or log a warning
                    std::cerr << "Warning: TANGENT accessor is neither Vec4 nor Vec3\n";
                }
            }
            // — TEXCOORD_0
            if (auto a = prim.findAttribute("TEXCOORD_0")) {
                const auto& acc = asset.accessors[a->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                        asset, acc,
                        [&](fastgltf::math::fvec2 uv, size_t i) {
                            m.vertices[i].uv = uv;
                        });
            }
            // — INDICES
            if (prim.indicesAccessor.has_value()) {
                const auto& acc = asset.accessors[*prim.indicesAccessor];
                m.indices.resize(acc.count);
                fastgltf::copyFromAccessor<uint32_t>(
                        asset, acc, m.indices.data());
            }

            outMeshes.emplace_back(std::move(m));
        }
    }

    return true;
}

reina::scene::ModelData reina::scene::gltf::MeshTBN::toModelData() {
    reina::scene::ModelData modelData;

    for (const VertexTBN& vertex : vertices) {
        modelData.vertices.push_back(vertex.position.x());
        modelData.vertices.push_back(vertex.position.y());
        modelData.vertices.push_back(vertex.position.z());
        modelData.vertices.push_back(1.0f);

        glm::mat3 tbn = glm::mat3(
                glm::vec3(vertex.tangent.x(), vertex.tangent.y(), vertex.tangent.z()),
                glm::vec3(vertex.bitangent.x(), vertex.bitangent.y(), vertex.bitangent.z()),
                glm::vec3(vertex.normal.x(), vertex.normal.y(), vertex.normal.z())
                );

        modelData.tbns.push_back(tbn);

        modelData.texCoords.push_back(vertex.uv.x());
        modelData.texCoords.push_back(vertex.uv.y());
    }

    for (uint32_t idx : indices) {
        modelData.indices.push_back(idx);
        modelData.tbnsIndices.push_back(idx);
        modelData.texIndices.push_back(idx);
    }

    return modelData;
}
